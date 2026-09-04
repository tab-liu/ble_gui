//! BLE 异步 worker：扫描、连接、GATT 与协议通知处理。
//!
//! Windows 上 `connect()` 实为 Uncached GetGattServices；刚停扫描时常
//! `Unreachable`→`Not connected`。[`connect_gatt_with_retry`] 先等待再重试，
//! UI 侧不要在 Connect 之前单独发 StopScan。

use std::collections::{HashMap, HashSet};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::future::pending;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use btleplug::api::{
    BDAddr, Central, CentralEvent, CharPropFlags, Characteristic, Manager as _, Peripheral as _,
    ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral, PeripheralId};
use futures::StreamExt;
use log::{debug, info, warn};
use tokio::sync::mpsc;

use super::poll::{
    clear_live_on_disconnect, init_live_on_connect, write_control_register, write_holding_registers,
    ModbusGate,
};
use super::poll_executor::poll_foreground_once;
use super::poll_policy::{describe_poll_foreground, ensure_dashboard_poll_if_idle, notify_poll, PollForeground, SharedPollPolicy};
use super::protocol::{HandshakePhase, ProtocolSession};
use super::ota::run_ble_ota;
use super::state::{LinkPhase, ScanLinkHint, SharedBleState};
use super::target::{
    adv_link_hint_from_properties, is_target_manufacturer_data, is_target_properties, matches_ff00,
    AdvLinkHint,
};
use super::uuids::{notify_uuid, notify_uuid_ff03, write_uuid};
use crate::services::modbus::{SharedModbusLive, SharedQueryPollLive};
use crate::services::firmware::{OtaJob, SharedOtaLive, PHASE_SUCCESS};
use crate::services::ble::modbus::POLL_INTERVAL_MS;

pub enum BleCommand {
    StartScan,
    StopScan,
    Connect { address: String },
    Disconnect,
    WriteRegister { address: u16, value: u16 },
    WriteHolding {
        slave_id: u8,
        address: u16,
        values: Vec<u16>,
        bit: Option<u8>,
        field: Option<crate::services::ble::modbus::RegisterFieldPatch>,
    },
    StartOta { job: OtaJob },
}

enum SessionCommand {
    WriteRegister { address: u16, value: u16 },
    WriteHolding {
        slave_id: u8,
        address: u16,
        values: Vec<u16>,
        bit: Option<u8>,
        field: Option<crate::services::ble::modbus::RegisterFieldPatch>,
    },
    StartOta { job: OtaJob },
}

type KnownMap = Arc<Mutex<HashMap<String, PeripheralId>>>;

/// IOT 升级成功后 Windows 可能仍报已连接；到期则按离线处理，不自动重连。
type SharedExpectDrop = Arc<Mutex<Option<Instant>>>;

struct ActiveSession {
    notify_task: tokio::task::JoinHandle<()>,
    poll_task: tokio::task::JoinHandle<()>,
    write_task: tokio::task::JoinHandle<()>,
    peripheral: Peripheral,
    cmd_tx: tokio::sync::mpsc::UnboundedSender<SessionCommand>,
    /// 必须持有，丢掉后 Windows 可能把连接间隔改回省电档。
    _win_throughput: Option<super::win_conn::WinThroughputHold>,
}

fn abort_session(active: &ActiveSession) {
    active.poll_task.abort();
    active.notify_task.abort();
    active.write_task.abort();
}

fn on_peer_disconnect(
    poll_abort: &tokio::task::AbortHandle,
    write_abort: &tokio::task::AbortHandle,
    state: &SharedBleState,
    poll_policy: &SharedPollPolicy,
    modbus_live: &SharedModbusLive,
    ota: &crate::services::firmware::SharedOtaLive,
    ui: &super::UiRefreshSlot,
    event_tx: &std::sync::mpsc::Sender<()>,
    detail: &str,
) {
    poll_abort.abort();
    write_abort.abort();
    stop_polling(poll_policy);
    if let Ok(mut p) = poll_policy.lock() {
        p.ota_busy = false;
    }
    clear_live_on_disconnect(modbus_live);
    if let Ok(mut g) = ota.lock() {
        if g.running {
            g.running = false;
            g.phase = crate::services::firmware::PHASE_FAILED;
            g.result_text = "升级失败".into();
            g.fail_reason = "升级过程中蓝牙断开".into();
            g.stage_text = "升级失败".into();
            g.status_text = g.fail_reason.clone();
        }
    }
    if let Ok(mut inner) = state.lock() {
        inner.phase = LinkPhase::Idle;
        inner.device_name.clear();
        inner.device_address.clear();
        inner.rssi = 0;
        inner.encryption_ready = false;
        inner.status_detail = detail.into();
    }
    notify_ui_force(ui, true);
    let _ = event_tx.send(());
}

fn still_shows_connected(state: &SharedBleState) -> bool {
    matches!(
        state.lock().map(|s| s.phase.clone()).unwrap_or(LinkPhase::Idle),
        LinkPhase::GattReady | LinkPhase::Handshake | LinkPhase::Encrypted
    )
}

fn take_disconnect_detail(expect_drop: &SharedExpectDrop) -> &'static str {
    if expect_drop
        .lock()
        .ok()
        .and_then(|mut g| g.take())
        .is_some()
    {
        "设备已断开（升级后重启）"
    } else {
        "设备已断开"
    }
}

fn should_force_drop_after_ota(expect_drop: &SharedExpectDrop) -> bool {
    expect_drop
        .lock()
        .ok()
        .and_then(|g| g.as_ref().copied())
        .is_some_and(|t| Instant::now() >= t)
}

/// ATT 载荷上限（MTU 247 − 3）。OTA 用无应答写，避免每片都等 ATT 回包。
const GATT_ATT_PAYLOAD: usize = 244;

async fn write_ff02_air(
    peripheral: &Peripheral,
    write_char: &Characteristic,
    ota_fast: bool,
    can_without_response: bool,
    fallback_with_response: &AtomicBool,
    data: &[u8],
) {
    let write_type = if ota_fast
        && can_without_response
        && !fallback_with_response.load(Ordering::Relaxed)
    {
        WriteType::WithoutResponse
    } else {
        WriteType::WithResponse
    };
    if ota_fast {
        static LOGGED: AtomicBool = AtomicBool::new(false);
        if !LOGGED.swap(true, Ordering::Relaxed) {
            info!(
                target: "ble_gui::ota",
                "OTA GATT 写入: {} 模式, {}B 分 {} 片 (FF02 without_response={can_without_response})",
                if write_type == WriteType::WithoutResponse {
                    "无应答"
                } else {
                    "有应答"
                },
                data.len(),
                data.len().div_ceil(GATT_ATT_PAYLOAD),
            );
        }
    }
    if write_type == WriteType::WithoutResponse {
        write_ota_without_response(
            peripheral,
            write_char,
            fallback_with_response,
            data,
        )
        .await;
        return;
    }
    write_chunks_sequential(
        peripheral,
        write_char,
        write_type,
        fallback_with_response,
        data,
    )
    .await;
}

/// 同一 XMODEM 包的 244B 分片同时提交，让控制器在一个连接事件里发出多片。
/// 若系统只允许 1 个未完成写，则从未成功的那一片起改为串行，已发出的片不重发。
async fn write_ota_without_response(
    peripheral: &Peripheral,
    write_char: &Characteristic,
    fallback_with_response: &AtomicBool,
    data: &[u8],
) {
    let t0 = std::time::Instant::now();
    let chunks: Vec<Vec<u8>> = data
        .chunks(GATT_ATT_PAYLOAD)
        .map(|c| c.to_vec())
        .collect();
    if chunks.is_empty() {
        return;
    }
    let futs: Vec<_> = chunks
        .iter()
        .map(|c| peripheral.write(write_char, c, WriteType::WithoutResponse))
        .collect();
    let results = futures::future::join_all(futs).await;
    let first_fail = results.iter().position(|r| r.is_err());
    match first_fail {
        None => {
            static TIMED: AtomicBool = AtomicBool::new(false);
            if !TIMED.swap(true, Ordering::Relaxed) {
                info!(
                    target: "ble_gui::ota",
                    "OTA 无应答并行提交 {} 片 / {}B, {}ms",
                    chunks.len(),
                    data.len(),
                    t0.elapsed().as_millis(),
                );
            }
        }
        Some(0) => {
            warn!(
                target: "ble_gui::ota",
                "OTA 并行无应答写不被支持，改为串行: {}",
                results[0].as_ref().err().map(|e| e.to_string()).unwrap_or_default(),
            );
            write_chunks_sequential(
                peripheral,
                write_char,
                WriteType::WithoutResponse,
                fallback_with_response,
                data,
            )
            .await;
        }
        Some(i) => {
            warn!(
                target: "ble_gui::ota",
                "OTA 并行写在第 {}/{} 片失败，其余串行补发",
                i + 1,
                chunks.len(),
            );
            let rest: Vec<u8> = chunks[i..].iter().flatten().copied().collect();
            write_chunks_sequential(
                peripheral,
                write_char,
                WriteType::WithoutResponse,
                fallback_with_response,
                &rest,
            )
            .await;
        }
    }
}

async fn write_chunks_sequential(
    peripheral: &Peripheral,
    write_char: &Characteristic,
    mut write_type: WriteType,
    fallback_with_response: &AtomicBool,
    data: &[u8],
) {
    for chunk in data.chunks(GATT_ATT_PAYLOAD) {
        match peripheral.write(write_char, chunk, write_type).await {
            Ok(()) => {}
            Err(err) if write_type == WriteType::WithoutResponse => {
                warn!(
                    target: "ble_gui::worker",
                    "OTA 无应答写入失败，后续改回有应答（会明显变慢）: {err}",
                );
                fallback_with_response.store(true, Ordering::Relaxed);
                write_type = WriteType::WithResponse;
                if let Err(err) = peripheral.write(write_char, chunk, write_type).await {
                    warn!(target: "ble_gui::worker", "GATT 写入失败: {err}");
                    return;
                }
            }
            Err(err) => {
                warn!(target: "ble_gui::worker", "GATT 写入失败: {err}");
                return;
            }
        }
    }
}

fn stop_polling(poll_policy: &SharedPollPolicy) {
    let mut policy = poll_policy.lock().expect("poll policy lock");
    if policy.foreground != PollForeground::None {
        log::info!(
            target: "ble_gui::poll",
            "轮询策略: {} → 无（停止轮询）",
            describe_poll_foreground(&policy.foreground),
        );
        policy.foreground = PollForeground::None;
    }
}

fn set_phase(state: &SharedBleState, phase: LinkPhase, detail: impl Into<String>) {
    if let Ok(mut inner) = state.lock() {
        inner.phase = phase;
        inner.status_detail = detail.into();
    }
}

fn notify_ui_force(
    ui_refresh: &super::UiRefreshSlot,
    force: bool,
) {
    schedule_ui_refresh(ui_refresh, force);
}

/// 扫描列表 UI 刷新间隔；连接/阶段变化等仍可通过 force 立即刷新。
const UI_REFRESH_INTERVAL_MS: u64 = 1000;
/// 轮询 btleplug 已缓存外设的间隔（补充事件流未送达的广播）。
const SCAN_SYNC_INTERVAL_MS: u64 = 1000;
/// 连接前句柄失效时，定向找回目标设备的超时。
const REDISCOVER_TIMEOUT: Duration = Duration::from_secs(12);
/// Windows 停扫描后 radio 尚未释放时，立刻 GetGattServices 常返回 Unreachable（Not connected）。
const CONNECT_SETTLE_MS: u64 = 400;
const CONNECT_RETRY_ATTEMPTS: u32 = 4;
const CONNECT_RETRY_BASE_MS: u64 = 400;
const LINK_WATCH_MS: u64 = 500;

const MSG_DEVICE_NOT_NEARBY: &str =
    "附近未发现该设备（可能已关机、距离过远，或已被其它设备连接后停止广播）";
const MSG_DEVICE_OCCUPIED: &str =
    "设备仍在附近广播，但标记为已连接；请先在其它手机/电脑上断开后再试";

fn schedule_ui_refresh(ui_refresh: &super::UiRefreshSlot, force: bool) {
    use std::sync::atomic::{AtomicU64, Ordering};

    static LAST_MS: AtomicU64 = AtomicU64::new(0);
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as u64)
        .unwrap_or(0);
    let last = LAST_MS.load(Ordering::Relaxed);
    if !force && now.saturating_sub(last) < UI_REFRESH_INTERVAL_MS {
        return;
    }
    LAST_MS.store(now, Ordering::Relaxed);

    if let Ok(guard) = ui_refresh.lock() {
        if let Some(hook) = guard.as_ref() {
            hook();
        }
    }
}

fn format_address(addr: BDAddr) -> String {
    addr.to_string()
}

fn parse_address(text: &str) -> Option<BDAddr> {
    text.parse().ok()
}

fn to_scan_link_hint(hint: AdvLinkHint) -> ScanLinkHint {
    match hint {
        AdvLinkHint::Unknown => ScanLinkHint::Unknown,
        AdvLinkHint::Available => ScanLinkHint::Available,
        AdvLinkHint::Occupied => ScanLinkHint::Occupied,
    }
}

fn connect_cancelled(flag: &AtomicBool) -> bool {
    flag.load(Ordering::Acquire)
}

async fn sleep_unless_cancelled(flag: &AtomicBool, dur: Duration) -> Result<(), String> {
    let end = tokio::time::Instant::now() + dur;
    loop {
        if connect_cancelled(flag) {
            return Err("已取消连接".into());
        }
        let now = tokio::time::Instant::now();
        if now >= end {
            return Ok(());
        }
        let slice = (end - now).min(Duration::from_millis(50));
        tokio::time::sleep(slice).await;
    }
}

/// Windows 上 btleplug 的 connect() 实际是 Uncached GetGattServices；
/// 扫描刚停或首次 GATT 常返回 Unreachable → Not connected，第二次即可成功。
async fn connect_gatt_with_retry(
    peripheral: &Peripheral,
    cancel_connect: &AtomicBool,
    state: &SharedBleState,
    ui_refresh: &super::UiRefreshSlot,
    address_text: &str,
) -> Result<(), String> {
    sleep_unless_cancelled(cancel_connect, Duration::from_millis(CONNECT_SETTLE_MS)).await?;

    for attempt in 1..=CONNECT_RETRY_ATTEMPTS {
        if connect_cancelled(cancel_connect) {
            return Err("已取消连接".into());
        }
        if peripheral.is_connected().await.unwrap_or(false) {
            return Ok(());
        }
        match peripheral.connect().await {
            Ok(()) => {
                if attempt > 1 {
                    info!(
                        target: "ble_gui::services::ble",
                        "GATT connect succeeded on attempt {attempt}/{CONNECT_RETRY_ATTEMPTS} for {address_text}"
                    );
                }
                return Ok(());
            }
            Err(err) => {
                warn!(
                    target: "ble_gui::services::ble",
                    "GATT connect attempt {attempt}/{CONNECT_RETRY_ATTEMPTS} for {address_text} failed: {err}"
                );
                if attempt == CONNECT_RETRY_ATTEMPTS {
                    return Err(format!("BLE 连接失败：{err}"));
                }
                set_phase(
                    state,
                    LinkPhase::Connecting,
                    format!(
                        "连接未就绪，正在重试（{attempt}/{CONNECT_RETRY_ATTEMPTS}）……"
                    ),
                );
                notify_ui_force(ui_refresh, true);
                sleep_unless_cancelled(
                    cancel_connect,
                    Duration::from_millis(CONNECT_RETRY_BASE_MS * u64::from(attempt)),
                )
                .await?;
            }
        }
    }
    Err("BLE 连接失败：Not connected".into())
}

pub async fn worker_main(
    mut cmd_rx: mpsc::UnboundedReceiver<BleCommand>,
    state: SharedBleState,
    event_tx: std::sync::mpsc::Sender<()>,
    ui_refresh: super::UiRefreshSlot,
    modbus_live: SharedModbusLive,
    query_live: SharedQueryPollLive,
    query_generation: Arc<AtomicU64>,
    poll_policy: SharedPollPolicy,
    cancel_connect: Arc<AtomicBool>,
    ota_live: SharedOtaLive,
) {
    let manager = match Manager::new().await {
        Ok(m) => m,
        Err(err) => {
            set_phase(&state, LinkPhase::Idle, format!("蓝牙初始化失败：{err}"));
            notify_ui_force(&ui_refresh, true);
            return;
        }
    };

    let adapters = match manager.adapters().await {
        Ok(a) => a,
        Err(err) => {
            set_phase(
                &state,
                LinkPhase::Idle,
                format!("枚举蓝牙适配器失败：{err}"),
            );
            notify_ui_force(&ui_refresh, true);
            return;
        }
    };

    let adapter = match adapters.into_iter().next() {
        Some(a) => a,
        None => {
            set_phase(&state, LinkPhase::Idle, "未找到蓝牙适配器");
            notify_ui_force(&ui_refresh, true);
            return;
        }
    };

    let mut scan_task: Option<tokio::task::JoinHandle<()>> = None;
    let mut session: Option<ActiveSession> = None;
    let known: KnownMap = Arc::new(Mutex::new(HashMap::new()));
    let expect_drop: SharedExpectDrop = Arc::new(Mutex::new(None));

    loop {
        if session
            .as_ref()
            .is_some_and(|s| s.notify_task.is_finished())
        {
            info!(target: "ble_gui::worker", "会话已结束，刷新连接状态");
            if let Some(active) = session.take() {
                abort_session(&active);
                let _ = active.peripheral.disconnect().await;
            }
            stop_polling(&poll_policy);
            clear_live_on_disconnect(&modbus_live);
            if still_shows_connected(&state) {
                set_phase(&state, LinkPhase::Idle, "设备已断开");
                notify_ui_force(&ui_refresh, true);
            }
            continue;
        }

        let maybe_cmd = tokio::select! {
            cmd = cmd_rx.recv() => cmd,
            _ = tokio::time::sleep(Duration::from_millis(250)), if session.is_some() => {
                continue;
            }
        };
        let Some(cmd) = maybe_cmd else {
            break;
        };

        match cmd {
            BleCommand::StartScan => {
                if state.lock().expect("ble state lock").phase == LinkPhase::Scanning
                    && scan_task.is_some()
                {
                    continue;
                }
                if let Some(active) = session.take() {
                    abort_session(&active);
                    let _ = active.peripheral.disconnect().await;
                    stop_polling(&poll_policy);
                    clear_live_on_disconnect(&modbus_live);
                }
                if let Some(task) = scan_task.take() {
                    task.abort();
                }
                let _ = adapter.stop_scan().await;

                {
                    let mut inner = state.lock().expect("ble state lock");
                    inner.phase = LinkPhase::Scanning;
                    inner.scan_devices.clear();
                    inner.scan_list_generation += 1;
                    inner.status_detail = "正在扫描附近蓝牙设备……".into();
                }
                known.lock().expect("known lock").clear();
                notify_ui_force(&ui_refresh, true);

                let adapter_clone = adapter.clone();
                let state_clone = state.clone();
                let event_tx_clone = event_tx.clone();
                let known_clone = known.clone();
                let ui_refresh_clone = ui_refresh.clone();
                scan_task = Some(tokio::spawn(async move {
                    run_scan_loop(
                        adapter_clone,
                        state_clone,
                        event_tx_clone,
                        ui_refresh_clone,
                        known_clone,
                    )
                    .await;
                }));
            }
            BleCommand::StopScan => {
                // 立刻切换状态，避免 UI 在 worker 清理完成前仍显示「扫描中」。
                set_phase(&state, LinkPhase::Idle, "正在停止扫描……");
                notify_ui_force(&ui_refresh, true);
                if let Some(task) = scan_task.take() {
                    task.abort();
                }
                let _ = adapter.stop_scan().await;
                let mut name_resolve_pending = HashSet::new();
                sync_all_peripherals(
                    &adapter,
                    &state,
                    &event_tx,
                    &ui_refresh,
                    &known,
                    &mut name_resolve_pending,
                    false,
                )
                .await;
                let count = state.lock().expect("ble state lock").scan_devices.len();
                set_phase(
                    &state,
                    LinkPhase::Idle,
                    format!("扫描已停止（共 {count} 个设备）"),
                );
                notify_ui_force(&ui_refresh, true);
            }
            BleCommand::Connect { address } => {
                if let Ok(mut g) = expect_drop.lock() {
                    *g = None;
                }
                cancel_connect.store(false, Ordering::Release);
                set_phase(
                    &state,
                    LinkPhase::Connecting,
                    format!("正在连接 {address}……"),
                );
                notify_ui_force(&ui_refresh, true);

                if let Some(active) = session.take() {
                    abort_session(&active);
                    let _ = active.peripheral.disconnect().await;
                    stop_polling(&poll_policy);
                    clear_live_on_disconnect(&modbus_live);
                }

                // 先在仍可能持有句柄时解析目标，避免「先停扫描再查找」把缓存清掉。
                let mut peripheral = find_peripheral(&adapter, &address, &known).await;

                if peripheral.is_none() {
                    if connect_cancelled(&cancel_connect) {
                        set_phase(&state, LinkPhase::Idle, "已取消连接");
                        notify_ui_force(&ui_refresh, true);
                        continue;
                    }
                    set_phase(&state, LinkPhase::Connecting, "正在查找设备……");
                    notify_ui_force(&ui_refresh, true);

                    if let Some(task) = scan_task.take() {
                        task.abort();
                    }
                    let _ = adapter.stop_scan().await;

                    match rediscover_target(
                        &adapter,
                        &address,
                        &known,
                        &state,
                        &ui_refresh,
                        &cancel_connect,
                    )
                    .await
                    {
                        Ok(p) => peripheral = Some(p),
                        Err(RediscoverError::Cancelled) => {
                            set_phase(&state, LinkPhase::Idle, "已取消连接");
                            notify_ui_force(&ui_refresh, true);
                            continue;
                        }
                        Err(RediscoverError::Occupied) => {
                            warn!(
                                target: "ble_gui::services::ble",
                                "Connect aborted for {address}: occupied"
                            );
                            set_phase(
                                &state,
                                LinkPhase::Idle,
                                format!("连接失败：{MSG_DEVICE_OCCUPIED}"),
                            );
                            notify_ui_force(&ui_refresh, true);
                            continue;
                        }
                        Err(RediscoverError::TimedOut) => {
                            warn!(
                                target: "ble_gui::services::ble",
                                "Connect aborted for {address}: rediscover timeout"
                            );
                            set_phase(
                                &state,
                                LinkPhase::Idle,
                                format!("连接失败：{MSG_DEVICE_NOT_NEARBY}"),
                            );
                            notify_ui_force(&ui_refresh, true);
                            continue;
                        }
                    }
                }

                if connect_cancelled(&cancel_connect) {
                    set_phase(&state, LinkPhase::Idle, "已取消连接");
                    notify_ui_force(&ui_refresh, true);
                    continue;
                }

                if let Some(task) = scan_task.take() {
                    task.abort();
                }
                let _ = adapter.stop_scan().await;

                let Some(peripheral) = peripheral else {
                    set_phase(
                        &state,
                        LinkPhase::Idle,
                        format!("连接失败：{MSG_DEVICE_NOT_NEARBY}"),
                    );
                    notify_ui_force(&ui_refresh, true);
                    continue;
                };

                if let Ok(Some(props)) = peripheral.properties().await {
                    if adv_link_hint_from_properties(&props) == AdvLinkHint::Occupied {
                        warn!(
                            target: "ble_gui::services::ble",
                            "Connect aborted for {address}: occupied (from cache)"
                        );
                        set_phase(
                            &state,
                            LinkPhase::Idle,
                            format!("连接失败：{MSG_DEVICE_OCCUPIED}"),
                        );
                        notify_ui_force(&ui_refresh, true);
                        continue;
                    }
                }

                match connect_device(
                    adapter.clone(),
                    peripheral,
                    &state,
                    &event_tx,
                    &ui_refresh,
                    &address,
                    &modbus_live,
                    &query_live,
                    &query_generation,
                    &poll_policy,
                    &cancel_connect,
                    &ota_live,
                    expect_drop.clone(),
                )
                .await
                {
                    Ok(active) => session = Some(active),
                    Err(err) => {
                        warn!(target: "ble_gui::services::ble", "Connect failed for {address}: {err}");
                        let detail = if err == "已取消连接" {
                            "已取消连接".into()
                        } else {
                            format!("连接失败：{err}")
                        };
                        set_phase(&state, LinkPhase::Idle, detail);
                        notify_ui_force(&ui_refresh, true);
                    }
                }
            }
            BleCommand::Disconnect => {
                if let Ok(mut g) = expect_drop.lock() {
                    *g = None;
                }
                if let Ok(mut g) = ota_live.lock() {
                    if g.running {
                        g.cancel = true;
                    }
                }
                if let Ok(mut p) = poll_policy.lock() {
                    p.ota_busy = false;
                }
                if let Some(active) = session.take() {
                    abort_session(&active);
                    let _ = active.peripheral.disconnect().await;
                }
                stop_polling(&poll_policy);
                clear_live_on_disconnect(&modbus_live);
                {
                    let mut inner = state.lock().expect("ble state lock");
                    inner.phase = LinkPhase::Idle;
                    inner.device_name.clear();
                    inner.device_address.clear();
                    inner.rssi = 0;
                    inner.encryption_ready = false;
                    inner.status_detail = "已断开".into();
                }
                notify_ui_force(&ui_refresh, true);
            }
            BleCommand::WriteRegister { address, value } => {
                if poll_policy.lock().map(|p| p.ota_busy).unwrap_or(false) {
                    continue;
                }
                if let Some(active) = &session {
                    let _ = active.cmd_tx.send(SessionCommand::WriteRegister { address, value });
                }
            }
            BleCommand::WriteHolding {
                slave_id,
                address,
                values,
                bit,
                field,
            } => {
                if poll_policy.lock().map(|p| p.ota_busy).unwrap_or(false) {
                    continue;
                }
                if let Some(active) = &session {
                    let _ = active.cmd_tx.send(SessionCommand::WriteHolding {
                        slave_id,
                        address,
                        values,
                        bit,
                        field,
                    });
                }
            }
            BleCommand::StartOta { job } => {
                if let Some(active) = &session {
                    let _ = active.cmd_tx.send(SessionCommand::StartOta { job });
                } else if let Ok(mut g) = ota_live.lock() {
                    g.running = false;
                    g.phase = crate::services::firmware::PHASE_FAILED;
                    g.result_text = "升级失败".into();
                    g.fail_reason = "未连接设备".into();
                    g.stage_text = "升级失败".into();
                }
            }
        }
    }
}

async fn run_scan_loop(
    adapter: Adapter,
    state: SharedBleState,
    event_tx: std::sync::mpsc::Sender<()>,
    ui_refresh: super::UiRefreshSlot,
    known: KnownMap,
) {
    // 必须先订阅事件流再 start_scan，否则广播事件会在无人接收时被丢弃。
    let mut events = match adapter.events().await {
        Ok(events) => events,
        Err(err) => {
            set_phase(
                &state,
                LinkPhase::Idle,
                format!("监听扫描事件失败：{err}"),
            );
            notify_ui_force(&ui_refresh, true);
            return;
        }
    };

    if adapter.start_scan(ScanFilter::default()).await.is_err() {
        warn!(target: "ble_gui::services::ble", "start_scan failed");
        set_phase(&state, LinkPhase::Idle, "启动扫描失败");
        notify_ui_force(&ui_refresh, true);
        return;
    }

    let mut name_resolve_pending: HashSet<String> = HashSet::new();

    // 立即同步一次 btleplug 已缓存的外设（重复点扫描能显示，往往是因为触发了这类同步）。
    sync_all_peripherals(
        &adapter,
        &state,
        &event_tx,
        &ui_refresh,
        &known,
        &mut name_resolve_pending,
        true,
    )
    .await;

    let mut poll = tokio::time::interval(Duration::from_millis(SCAN_SYNC_INTERVAL_MS));
    poll.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
    // 跳过 interval 首次立即触发的 tick，避免与上方 sync 重复。
    poll.tick().await;

    loop {
        if state.lock().expect("ble state lock").phase != LinkPhase::Scanning {
            break;
        }

        tokio::select! {
            _ = poll.tick() => {
                sync_all_peripherals(
                    &adapter,
                    &state,
                    &event_tx,
                    &ui_refresh,
                    &known,
                    &mut name_resolve_pending,
                    true,
                ).await;
            }
            maybe_event = events.next() => {
                match maybe_event {
                    None => break,
                    Some(event) => {
                        if state.lock().expect("ble state lock").phase != LinkPhase::Scanning {
                            break;
                        }
                        match event {
                            CentralEvent::DeviceDiscovered(id) | CentralEvent::DeviceUpdated(id) => {
                                if ingest_peripheral(
                                    &adapter,
                                    &id,
                                    &state,
                                    &event_tx,
                                    &ui_refresh,
                                    &known,
                                    &mut name_resolve_pending,
                                    false,
                                    true,
                                )
                                .await
                                {
                                    notify_scan_list_changed(&state, &ui_refresh);
                                }
                            }
                            CentralEvent::ServicesAdvertisement { id, services } => {
                                let force_target = services.iter().any(|uuid| matches_ff00(uuid));
                                if ingest_peripheral(
                                    &adapter,
                                    &id,
                                    &state,
                                    &event_tx,
                                    &ui_refresh,
                                    &known,
                                    &mut name_resolve_pending,
                                    force_target,
                                    true,
                                )
                                .await
                                {
                                    notify_scan_list_changed(&state, &ui_refresh);
                                }
                            }
                            CentralEvent::ManufacturerDataAdvertisement { id, manufacturer_data } => {
                                let force_target = manufacturer_data
                                    .values()
                                    .any(|data| is_target_manufacturer_data(data));
                                if force_target
                                    && ingest_peripheral(
                                        &adapter,
                                        &id,
                                        &state,
                                        &event_tx,
                                        &ui_refresh,
                                        &known,
                                        &mut name_resolve_pending,
                                        true,
                                        true,
                                    )
                                    .await
                                {
                                    notify_scan_list_changed(&state, &ui_refresh);
                                }
                            }
                            _ => {}
                        }
                    }
                }
            }
        }
    }

    let _ = adapter.stop_scan().await;
    if state.lock().expect("ble state lock").phase == LinkPhase::Scanning {
        set_phase(&state, LinkPhase::Idle, "扫描已结束");
        notify_ui_force(&ui_refresh, true);
    }
}

/// 轮询适配器已发现的所有外设，不依赖事件是否送达。
async fn sync_all_peripherals(
    adapter: &Adapter,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    known: &KnownMap,
    name_resolve_pending: &mut HashSet<String>,
    require_scanning: bool,
) {
    let Ok(peripherals) = adapter.peripherals().await else {
        return;
    };
    let gen_before = state.lock().expect("ble state lock").scan_list_generation;
    let mut list_changed = false;
    for peripheral in peripherals {
        if ingest_peripheral(
            adapter,
            &peripheral.id(),
            state,
            event_tx,
            ui_refresh,
            known,
            name_resolve_pending,
            false,
            require_scanning,
        )
        .await
        {
            list_changed = true;
        }
    }
    if list_changed {
        update_scan_status_detail(state);
        notify_ui_force(ui_refresh, true);
    } else if !require_scanning
        && gen_before != state.lock().expect("ble state lock").scan_list_generation
    {
        notify_ui_force(ui_refresh, true);
    }
}

fn notify_scan_list_changed(state: &SharedBleState, ui_refresh: &super::UiRefreshSlot) {
    update_scan_status_detail(state);
    notify_ui_force(ui_refresh, true);
}

fn update_scan_status_detail(state: &SharedBleState) {
    let mut inner = state.lock().expect("ble state lock");
    if inner.phase != LinkPhase::Scanning {
        return;
    }
    let total = inner.scan_devices.len();
    let named = inner
        .scan_devices
        .iter()
        .filter(|d| !crate::services::ble::state::is_placeholder_scan_name(&d.name))
        .count();
    inner.status_detail = format!("已发现 {total} 个蓝牙设备，其中 {named} 个已解析名称");
}

async fn ingest_peripheral(
    adapter: &Adapter,
    id: &PeripheralId,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    known: &KnownMap,
    name_resolve_pending: &mut HashSet<String>,
    force_target: bool,
    require_scanning: bool,
) -> bool {
    let Ok(peripheral) = find_peripheral_by_id(adapter, id).await else {
        return false;
    };
    let Ok(Some(props)) = peripheral.properties().await else {
        return false;
    };

    let address = format_address(props.address);
    known
        .lock()
        .expect("known lock")
        .insert(address.clone(), id.clone());

    let is_target = force_target || is_target_properties(&props);
    let name = props
        .local_name
        .as_deref()
        .unwrap_or("")
        .trim()
        .to_string();
    let rssi = props.rssi.unwrap_or(-100);

    if !is_target {
        return false;
    }

    let list_changed = {
        let mut inner = state.lock().expect("ble state lock");
        if require_scanning && inner.phase != LinkPhase::Scanning {
            return false;
        }
        let before_len = inner.scan_devices.len();
        let gen_before = inner.scan_list_generation;
        let link_hint = to_scan_link_hint(adv_link_hint_from_properties(&props));
        inner.upsert_advertisement(&name, &address, rssi as i32, is_target, link_hint);
        inner.scan_list_generation > gen_before || inner.scan_devices.len() > before_len
    };

    if name.is_empty() && name_resolve_pending.insert(address.clone()) {
        let peripheral = peripheral.clone();
        let state = state.clone();
        let event_tx = event_tx.clone();
        let ui_refresh = ui_refresh.clone();
        tokio::spawn(async move {
            resolve_device_name(peripheral, address, state, event_tx, ui_refresh).await;
        });
    }

    list_changed
}

/// 轮询 peripheral.properties()，等待 Scan Response 中的 Complete Local Name 到达。
async fn resolve_device_name(
    peripheral: Peripheral,
    address: String,
    state: SharedBleState,
    _event_tx: std::sync::mpsc::Sender<()>,
    ui_refresh: super::UiRefreshSlot,
) {
    if let Some(name) = super::win_name::resolve_device_name_from_address(&address).await {
        let Ok(Some(props)) = peripheral.properties().await else {
            return;
        };
        let rssi = props.rssi.unwrap_or(-100);
        {
            let mut inner = state.lock().expect("ble state lock");
            if inner.phase != LinkPhase::Scanning {
                return;
            }
            let hint = to_scan_link_hint(adv_link_hint_from_properties(&props));
            inner.upsert_advertisement(
                &name,
                &address,
                rssi as i32,
                is_target_properties(&props),
                hint,
            );
        }
        update_scan_status_detail(&state);
        notify_ui_force(&ui_refresh, true);
        return;
    }

    for _ in 0..20 {
        tokio::time::sleep(Duration::from_millis(200)).await;

        let scanning = state
            .lock()
            .map(|inner| inner.phase == LinkPhase::Scanning)
            .unwrap_or(false);
        if !scanning {
            return;
        }

        let Ok(Some(props)) = peripheral.properties().await else {
            continue;
        };
        let name = props
            .local_name
            .as_deref()
            .unwrap_or("")
            .trim()
            .to_string();
        if name.is_empty() {
            continue;
        }

        let is_target = is_target_properties(&props);
        let rssi = props.rssi.unwrap_or(-100);
        {
            let mut inner = state.lock().expect("ble state lock");
            if inner.phase != LinkPhase::Scanning {
                return;
            }
            let hint = to_scan_link_hint(adv_link_hint_from_properties(&props));
            inner.upsert_advertisement(&name, &address, rssi as i32, is_target, hint);
        }
        update_scan_status_detail(&state);
        notify_ui_force(&ui_refresh, true);
        return;
    }
}

async fn connect_device(
    adapter: Adapter,
    peripheral: Peripheral,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    address_text: &str,
    modbus_live: &SharedModbusLive,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    poll_policy: &SharedPollPolicy,
    cancel_connect: &AtomicBool,
    ota_live: &SharedOtaLive,
    expect_drop: SharedExpectDrop,
) -> Result<ActiveSession, String> {
    if connect_cancelled(cancel_connect) {
        return Err("已取消连接".into());
    }
    set_phase(
        state,
        LinkPhase::Connecting,
        format!("正在连接 {address_text}……"),
    );
    notify_ui_force(ui_refresh, true);

    connect_gatt_with_retry(
        &peripheral,
        cancel_connect,
        state,
        ui_refresh,
        address_text,
    )
    .await?;

    if connect_cancelled(cancel_connect) {
        let _ = peripheral.disconnect().await;
        return Err("已取消连接".into());
    }

    set_phase(state, LinkPhase::Connecting, "正在发现 GATT 服务……");
    notify_ui_force(ui_refresh, true);

    discover_services_with_retry(&peripheral, 10).await?;

    if connect_cancelled(cancel_connect) {
        let _ = peripheral.disconnect().await;
        return Err("已取消连接".into());
    }

    let characteristics = peripheral.characteristics();
    let write_id = write_uuid();
    let notify_id = notify_uuid();
    let notify_ff03 = notify_uuid_ff03();

    let write_char = characteristics
        .iter()
        .find(|c| c.uuid == write_id)
        .cloned()
        .ok_or_else(|| "未发现 FF02 写特征".to_string())?;

    let notify_char_ff01 = characteristics
        .iter()
        .find(|c| c.uuid == notify_id)
        .cloned();
    let notify_char_ff03 = characteristics
        .iter()
        .find(|c| c.uuid == notify_ff03)
        .cloned();

    let notify_char = notify_char_ff01
        .clone()
        .or_else(|| notify_char_ff03.clone())
        .ok_or_else(|| "未发现 FF01/FF03 通知特征".to_string())?;

    peripheral
        .subscribe(&notify_char)
        .await
        .map_err(|e| format!("订阅通知失败：{e}"))?;

    if let Some(ff03) = notify_char_ff03 {
        if notify_char_ff01.is_some() && ff03.uuid != notify_char.uuid {
            peripheral
                .subscribe(&ff03)
                .await
                .map_err(|e| format!("订阅 FF03 通知失败：{e}"))?;
            debug!(target: "ble_gui::worker", "已订阅 FF01 + FF03（TLV 异步数据走 FF03）");
        } else {
            debug!(target: "ble_gui::worker", "已订阅 FF03（无 FF01，TLV/通知均走 FF03）");
        }
    } else if notify_char_ff01.is_some() {
        info!(target: "ble_gui::worker", "已订阅 FF01（设备无 FF03 特征）");
    }

    let props = peripheral.properties().await.ok().flatten();
    let device_name = props
        .as_ref()
        .and_then(|p| p.local_name.clone())
        .filter(|n| !n.trim().is_empty())
        .unwrap_or_else(|| address_text.to_string());
    let device_address = props
        .as_ref()
        .map(|p| format_address(p.address))
        .unwrap_or_else(|| address_text.to_string());
    let device_rssi = props.as_ref().and_then(|p| p.rssi).unwrap_or(-100);

    {
        let mut inner = state.lock().expect("ble state lock");
        inner.phase = LinkPhase::GattReady;
        inner.device_name = device_name;
        inner.device_address = device_address;
        inner.rssi = device_rssi as i32;
        inner.status_detail = "GATT 已连接，等待设备鉴权……".into();
    }
    notify_ui_force(ui_refresh, true);

    init_live_on_connect(modbus_live);
    ensure_dashboard_poll_if_idle(poll_policy);

    let (write_tx, mut write_rx) = mpsc::unbounded_channel::<Vec<u8>>();
    let (session_cmd_tx, mut session_cmd_rx) = mpsc::unbounded_channel::<SessionCommand>();
    let protocol = Arc::new(Mutex::new(ProtocolSession::new()));
    let protocol_for_notify = protocol.clone();
    let protocol_for_write = protocol.clone();
    let state_for_notify = state.clone();
    let event_for_notify = event_tx.clone();
    let peripheral_for_write = peripheral.clone();
    let write_char_for_task = write_char.clone();
    let state_for_plain = state.clone();
    let event_for_plain = event_tx.clone();
    let ui_refresh_for_poll = ui_refresh.clone();
    let ui_refresh_for_notify = ui_refresh.clone();
    let modbus_live_poll = modbus_live.clone();
    let modbus_live_write = modbus_live.clone();
    let query_live_poll = query_live.clone();
    let query_live_write = query_live.clone();
    let query_gen_poll = query_generation.clone();
    let query_gen_write = query_generation.clone();
    let poll_policy_task = poll_policy.clone();
    let poll_policy_notify = poll_policy.clone();
    let write_tx_poll = write_tx.clone();
    let ota_live_session = ota_live.clone();
    let poll_policy_ota = poll_policy.clone();

    let modbus_gate: ModbusGate = Arc::new(tokio::sync::Mutex::new(()));
    let gate_for_poll = modbus_gate.clone();
    let gate_for_write = modbus_gate.clone();
    let event_for_poll = event_tx.clone();
    let protocol_for_poll_task = protocol.clone();
    let poll_task = tokio::spawn(async move {
        let wake = poll_policy_task
            .lock()
            .map(|p| p.wake.clone())
            .unwrap_or_else(|_| std::sync::Arc::new(tokio::sync::Notify::new()));
        loop {
            loop {
                let ready = protocol_for_poll_task
                    .lock()
                    .expect("protocol lock")
                    .modbus_ready();
                if ready {
                    break;
                }
                tokio::select! {
                    _ = tokio::time::sleep(Duration::from_millis(50)) => {}
                    _ = wake.notified() => {}
                }
            }

            let _ok = poll_foreground_once(
                &poll_policy_task,
                &protocol_for_poll_task,
                &write_tx_poll,
                &modbus_live_poll,
                &query_live_poll,
                &query_gen_poll,
                &gate_for_poll,
            )
            .await;
            notify_ui_force(&ui_refresh_for_poll, true);
            let _ = event_for_poll.send(());

            loop {
                let ota_busy = poll_policy_task
                    .lock()
                    .map(|p| p.ota_busy)
                    .unwrap_or(false);
                tokio::select! {
                    _ = tokio::time::sleep(Duration::from_millis(POLL_INTERVAL_MS)), if !ota_busy => {}
                    _ = wake.notified() => {}
                }
                let ready = protocol_for_poll_task
                    .lock()
                    .expect("protocol lock")
                    .modbus_ready();
                if !ready {
                    break;
                }
                if poll_policy_task
                    .lock()
                    .map(|p| p.ota_busy)
                    .unwrap_or(false)
                {
                    continue;
                }
                let _ok = poll_foreground_once(
                    &poll_policy_task,
                    &protocol_for_poll_task,
                    &write_tx_poll,
                    &modbus_live_poll,
                    &query_live_poll,
                    &query_gen_poll,
                    &gate_for_poll,
                )
                .await;
                notify_ui_force(&ui_refresh_for_poll, true);
                let _ = event_for_poll.send(());
            }
        }
    });

    let poll_abort = poll_task.abort_handle();
    let peripheral_for_session = peripheral.clone();
    let win_throughput = super::win_conn::request_throughput(address_text).await;
    let can_write_without_response = write_char
        .properties
        .contains(CharPropFlags::WRITE_WITHOUT_RESPONSE);
    info!(
        target: "ble_gui::worker",
        "FF02 写特征 properties={:?} without_response={can_write_without_response}",
        write_char.properties,
    );
    let ota_wr_fallback = Arc::new(AtomicBool::new(false));
    let poll_policy_write = poll_policy.clone();
    let write_task = tokio::spawn(async move {
        while let Some(data) = write_rx.recv().await {
            let ota_fast = poll_policy_write
                .lock()
                .map(|p| p.ota_busy)
                .unwrap_or(false);
            write_ff02_air(
                &peripheral_for_write,
                &write_char_for_task,
                ota_fast,
                can_write_without_response,
                &ota_wr_fallback,
                &data,
            )
            .await;
        }
    });
    let write_abort_notify = write_task.abort_handle();
    let ota_live_notify = ota_live.clone();
    let expect_drop_notify = expect_drop.clone();
    let peripheral_watch = peripheral.clone();
    let adapter_watch = adapter.clone();
    let our_id = peripheral.id();
    let notify_task = tokio::spawn(async move {
        let mut notifications = match peripheral.notifications().await {
            Ok(stream) => stream,
            Err(_) => {
                poll_abort.abort();
                write_abort_notify.abort();
                return;
            }
        };
        let mut adapter_events = adapter_watch.events().await.ok();
        let mut link_tick = tokio::time::interval(Duration::from_millis(LINK_WATCH_MS));
        link_tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);

        let plain_timer = tokio::time::sleep(Duration::from_secs(6));
        tokio::pin!(plain_timer);

        loop {
            tokio::select! {
                _ = link_tick.tick() => {
                    if !peripheral_watch.is_connected().await.unwrap_or(false)
                        || should_force_drop_after_ota(&expect_drop_notify)
                    {
                        info!(target: "ble_gui::worker", "链路轮询：设备已离线");
                        on_peer_disconnect(
                            &poll_abort,
                            &write_abort_notify,
                            &state_for_notify,
                            &poll_policy_notify,
                            &modbus_live_write,
                            &ota_live_notify,
                            &ui_refresh_for_notify,
                            &event_for_notify,
                            take_disconnect_detail(&expect_drop_notify),
                        );
                        break;
                    }
                }
                maybe_central = async {
                    match adapter_events.as_mut() {
                        Some(stream) => stream.next().await,
                        None => pending().await,
                    }
                } => {
                    if let Some(CentralEvent::DeviceDisconnected(id)) = maybe_central {
                        if id == our_id {
                            info!(target: "ble_gui::worker", "收到 DeviceDisconnected");
                            on_peer_disconnect(
                                &poll_abort,
                                &write_abort_notify,
                                &state_for_notify,
                                &poll_policy_notify,
                                &modbus_live_write,
                                &ota_live_notify,
                                &ui_refresh_for_notify,
                                &event_for_notify,
                                take_disconnect_detail(&expect_drop_notify),
                            );
                            break;
                        }
                    }
                }
                _ = &mut plain_timer => {
                    let session = protocol_for_notify.lock().expect("protocol lock");
                    if session.phase == HandshakePhase::WaitingAuth && !session.auth_started() {
                        drop(session);
                        let mut session = protocol_for_notify.lock().expect("protocol lock");
                        session.mark_plaintext_mode();
                        if let Ok(mut inner) = state_for_plain.lock() {
                            inner.phase = LinkPhase::GattReady;
                            inner.status_detail = "设备未触发加密握手，按明文 Modbus 模式".into();
                        }
                        let _ = event_for_plain.send(());
                        notify_poll(&poll_policy_notify);
                    }
                }
                maybe_cmd = session_cmd_rx.recv() => {
                    match maybe_cmd {
                        Some(SessionCommand::WriteRegister { address, value }) => {
                            if let Ok(mut live) = modbus_live_write.lock() {
                                live.output_busy = true;
                            }
                            notify_ui_force(&ui_refresh_for_notify, true);
                            let protocol = protocol_for_write.clone();
                            let write_tx = write_tx.clone();
                            let live = modbus_live_write.clone();
                            let ui = ui_refresh_for_notify.clone();
                            let event_tx = event_for_notify.clone();
                            let gate = gate_for_write.clone();
                            let protocol_poll = protocol.clone();
                            let write_tx_poll = write_tx.clone();
                            let live_poll = modbus_live_write.clone();
                            let query_poll = query_live_write.clone();
                            let query_gen = query_gen_write.clone();
                            let policy_poll = poll_policy_notify.clone();
                            let gate_poll = gate_for_write.clone();
                            tokio::spawn(async move {
                                let result = write_control_register(
                                    &protocol,
                                    &write_tx,
                                    &live,
                                    &gate,
                                    address,
                                    value,
                                )
                                .await;
                                if let Ok(mut live) = live.lock() {
                                    live.output_busy = false;
                                    if result.is_err() {
                                        warn!(
                                            target: "ble_gui::worker",
                                            "写寄存器 {address} 失败: {}",
                                            result.as_ref().err().unwrap_or(&String::new())
                                        );
                                    }
                                }
                                if result.is_ok() {
                                    poll_foreground_once(
                                        &policy_poll,
                                        &protocol_poll,
                                        &write_tx_poll,
                                        &live_poll,
                                        &query_poll,
                                        &query_gen,
                                        &gate_poll,
                                    )
                                    .await;
                                }
                                notify_ui_force(&ui, true);
                                let _ = event_tx.send(());
                            });
                        }
                        Some(SessionCommand::WriteHolding {
                            slave_id,
                            address,
                            values,
                            bit,
                            field,
                        }) => {
                            let protocol = protocol_for_write.clone();
                            let write_tx = write_tx.clone();
                            let ui = ui_refresh_for_notify.clone();
                            let event_tx = event_for_notify.clone();
                            let gate = gate_for_write.clone();
                            let protocol_poll = protocol.clone();
                            let write_tx_poll = write_tx.clone();
                            let live_poll = modbus_live_write.clone();
                            let query_poll = query_live_write.clone();
                            let query_gen = query_gen_write.clone();
                            let policy_poll = poll_policy_notify.clone();
                            let gate_poll = gate_for_write.clone();
                            tokio::spawn(async move {
                                let result = write_holding_registers(
                                    &protocol,
                                    &write_tx,
                                    &gate,
                                    slave_id,
                                    address,
                                    &values,
                                    bit,
                                    field,
                                )
                                .await;
                                if let Err(err) = &result {
                                    warn!(
                                        target: "ble_gui::worker",
                                        "写保持寄存器 {address} 失败: {err}",
                                    );
                                }
                                if result.is_ok() {
                                    poll_foreground_once(
                                        &policy_poll,
                                        &protocol_poll,
                                        &write_tx_poll,
                                        &live_poll,
                                        &query_poll,
                                        &query_gen,
                                        &gate_poll,
                                    )
                                    .await;
                                }
                                notify_ui_force(&ui, true);
                                let _ = event_tx.send(());
                            });
                        }
                        Some(SessionCommand::StartOta { job }) => {
                            if let Ok(mut p) = poll_policy_ota.lock() {
                                p.ota_busy = true;
                                p.foreground = PollForeground::None;
                            }
                            notify_poll(&poll_policy_ota);
                            let protocol = protocol_for_write.clone();
                            let write_tx = write_tx.clone();
                            let gate = gate_for_write.clone();
                            let live = modbus_live_write.clone();
                            let ota = ota_live_session.clone();
                            let ui = ui_refresh_for_notify.clone();
                            let policy = poll_policy_ota.clone();
                            let expect_drop = expect_drop_notify.clone();
                            tokio::spawn(async move {
                                run_ble_ota(
                                    &protocol, &write_tx, &gate, &live, &ota, &ui, job,
                                )
                                .await;
                                if let Ok(mut p) = policy.lock() {
                                    p.ota_busy = false;
                                }
                                notify_poll(&policy);
                                let iot_ok = ota.lock().ok().is_some_and(|g| {
                                    g.phase == PHASE_SUCCESS && g.ble_only
                                });
                                if iot_ok {
                                    info!(
                                        target: "ble_gui::ota",
                                        "IOT 升级成功，等待设备重启后刷新连接状态",
                                    );
                                    if let Ok(mut slot) = expect_drop.lock() {
                                        *slot = Some(Instant::now() + Duration::from_secs(2));
                                    }
                                }
                                notify_ui_force(&ui, true);
                            });
                        }
                        None => {
                            poll_abort.abort();
                            write_abort_notify.abort();
                            break;
                        }
                    }
                }
                maybe_notification = notifications.next() => {
                    match maybe_notification {
                        Some(notification) => {
                            let response = {
                                let mut session = protocol_for_notify.lock().expect("protocol lock");
                                let was_encryption_ready = session.is_encryption_ready();
                                let was_modbus_ready = session.modbus_ready();
                                let result = session.on_notification(&notification.value);
                                let encrypted = session.is_encryption_ready();
                                let became_encrypted = !was_encryption_ready && encrypted;
                                let phase = session.phase;
                                let modbus_ready = session.modbus_ready();
                                let became_modbus_ready = !was_modbus_ready && modbus_ready;
                                (
                                    result,
                                    became_encrypted,
                                    phase,
                                    became_modbus_ready,
                                )
                            };
                            match response.0 {
                                Ok(Some(bytes)) => {
                                    let _ = write_tx.send(bytes);
                                }
                                Ok(None) => {}
                                Err(err) => {
                                    warn!(target: "ble_gui::worker", "协议处理异常: {err}");
                                    if let Ok(mut inner) = state_for_notify.lock() {
                                        inner.status_detail = format!("协议处理异常：{err}");
                                    }
                                    let _ = event_for_notify.send(());
                                }
                            }
                            if response.1 {
                                if let Ok(mut inner) = state_for_notify.lock() {
                                    inner.phase = LinkPhase::Encrypted;
                                    inner.encryption_ready = true;
                                    inner.status_detail = "加密链路已完成".into();
                                }
                                let _ = event_for_notify.send(());
                                notify_poll(&poll_policy_notify);
                            } else if response.2 == HandshakePhase::AuthDone {
                                if let Ok(mut inner) = state_for_notify.lock() {
                                    inner.phase = LinkPhase::Handshake;
                                    inner.status_detail = "2A2A 鉴权成功，ECDH 握手中……".into();
                                }
                                let _ = event_for_notify.send(());
                            } else if response.3 {
                                let _ = event_for_notify.send(());
                                notify_poll(&poll_policy_notify);
                            }
                        }
                        None => {
                            info!(
                                target: "ble_gui::worker",
                                "GATT 通知流结束，判定设备已断开",
                            );
                            on_peer_disconnect(
                                &poll_abort,
                                &write_abort_notify,
                                &state_for_notify,
                                &poll_policy_notify,
                                &modbus_live_write,
                                &ota_live_notify,
                                &ui_refresh_for_notify,
                                &event_for_notify,
                                take_disconnect_detail(&expect_drop_notify),
                            );
                            break;
                        }
                    }
                }
            }
        }
    });

    Ok(ActiveSession {
        notify_task,
        poll_task,
        write_task,
        peripheral: peripheral_for_session,
        cmd_tx: session_cmd_tx,
        _win_throughput: win_throughput,
    })
}

async fn discover_services_with_retry(peripheral: &Peripheral, attempts: usize) -> Result<(), String> {
    for attempt in 1..=attempts {
        match peripheral.discover_services().await {
            Ok(()) => {
                let has_target = peripheral.characteristics().iter().any(|c| {
                    c.uuid == write_uuid() || c.uuid == notify_uuid() || c.uuid == notify_uuid_ff03()
                });
                if has_target {
                    return Ok(());
                }
            }
            Err(err) if attempt == attempts => {
                return Err(format!("GATT 服务发现失败：{err}"));
            }
            Err(_) => {}
        }
        tokio::time::sleep(Duration::from_millis(1000)).await;
    }
    Err("GATT 已连接但未发现 FF00 服务".to_string())
}

async fn find_peripheral_by_id(adapter: &Adapter, id: &PeripheralId) -> Result<Peripheral, ()> {
    adapter.peripheral(id).await.map_err(|_| ())
}

enum RediscoverError {
    Occupied,
    TimedOut,
    Cancelled,
}

fn address_matches(props_addr: BDAddr, target_text: &str, target_norm: &str) -> bool {
    let text = format_address(props_addr);
    text == target_text || normalize_address(&text) == target_norm
}

async fn note_peripheral_for_target(
    adapter: &Adapter,
    id: &PeripheralId,
    target_text: &str,
    target_norm: &str,
    known: &KnownMap,
) -> Result<Option<Peripheral>, RediscoverError> {
    let Ok(peripheral) = find_peripheral_by_id(adapter, id).await else {
        return Ok(None);
    };
    let Ok(Some(props)) = peripheral.properties().await else {
        return Ok(None);
    };
    if !address_matches(props.address, target_text, target_norm) {
        return Ok(None);
    }

    let address = format_address(props.address);
    known
        .lock()
        .expect("known lock")
        .insert(address, id.clone());

    if adv_link_hint_from_properties(&props) == AdvLinkHint::Occupied {
        return Err(RediscoverError::Occupied);
    }
    Ok(Some(peripheral))
}

/// 句柄失效时短时扫描，仅找回指定地址对应的外设。
async fn rediscover_target(
    adapter: &Adapter,
    address_text: &str,
    known: &KnownMap,
    state: &SharedBleState,
    ui_refresh: &super::UiRefreshSlot,
    cancel_connect: &AtomicBool,
) -> Result<Peripheral, RediscoverError> {
    let target_norm = normalize_address(address_text);
    let deadline = tokio::time::Instant::now() + REDISCOVER_TIMEOUT;

    let mut events = adapter
        .events()
        .await
        .map_err(|_| RediscoverError::TimedOut)?;
    if adapter.start_scan(ScanFilter::default()).await.is_err() {
        return Err(RediscoverError::TimedOut);
    }

    if let Ok(peripherals) = adapter.peripherals().await {
        for peripheral in peripherals {
            if connect_cancelled(cancel_connect) {
                let _ = adapter.stop_scan().await;
                return Err(RediscoverError::Cancelled);
            }
            let id = peripheral.id();
            match note_peripheral_for_target(adapter, &id, address_text, &target_norm, known).await
            {
                Ok(Some(p)) => {
                    let _ = adapter.stop_scan().await;
                    return Ok(p);
                }
                Err(RediscoverError::Occupied) => {
                    let _ = adapter.stop_scan().await;
                    return Err(RediscoverError::Occupied);
                }
                Ok(None) | Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled) => {}
            }
        }
    }

    let mut poll = tokio::time::interval(Duration::from_millis(SCAN_SYNC_INTERVAL_MS));
    poll.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
    poll.tick().await;

    let result = loop {
        if connect_cancelled(cancel_connect)
            || state.lock().expect("ble state lock").phase != LinkPhase::Connecting
        {
            break Err(RediscoverError::Cancelled);
        }
        if tokio::time::Instant::now() >= deadline {
            break Err(RediscoverError::TimedOut);
        }

        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        tokio::select! {
            _ = poll.tick() => {
                if connect_cancelled(cancel_connect) {
                    break Err(RediscoverError::Cancelled);
                }
                if let Some(p) = find_peripheral(adapter, address_text, known).await {
                    break Ok(p);
                }
                let mut occupied = false;
                let mut found = None;
                if let Ok(peripherals) = adapter.peripherals().await {
                    for peripheral in peripherals {
                        let id = peripheral.id();
                        match note_peripheral_for_target(
                            adapter,
                            &id,
                            address_text,
                            &target_norm,
                            known,
                        )
                        .await
                        {
                            Ok(Some(p)) => {
                                found = Some(p);
                                break;
                            }
                            Err(RediscoverError::Occupied) => {
                                occupied = true;
                                break;
                            }
                            Ok(None) | Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled) => {}
                        }
                    }
                }
                if occupied {
                    break Err(RediscoverError::Occupied);
                }
                if let Some(p) = found {
                    break Ok(p);
                }
            }
            maybe_event = events.next() => {
                match maybe_event {
                    None => break Err(RediscoverError::TimedOut),
                    Some(CentralEvent::DeviceDiscovered(id) | CentralEvent::DeviceUpdated(id))
                    | Some(CentralEvent::ServicesAdvertisement { id, .. })
                    | Some(CentralEvent::ManufacturerDataAdvertisement { id, .. }) => {
                        match note_peripheral_for_target(
                            adapter,
                            &id,
                            address_text,
                            &target_norm,
                            known,
                        )
                        .await
                        {
                            Ok(Some(p)) => break Ok(p),
                            Err(RediscoverError::Occupied) => {
                                break Err(RediscoverError::Occupied);
                            }
                            Ok(None) | Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled) => {}
                        }
                    }
                    Some(_) => {}
                }
            }
            _ = tokio::time::sleep(remaining) => {
                break Err(RediscoverError::TimedOut);
            }
        }
    };

    let _ = adapter.stop_scan().await;
    if matches!(result, Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled)) {
        notify_ui_force(ui_refresh, true);
    }
    result
}

async fn find_peripheral(
    adapter: &Adapter,
    address_text: &str,
    known: &KnownMap,
) -> Option<Peripheral> {
    let target_norm = normalize_address(address_text);
    let parsed = parse_address(address_text);
    let id = {
        let map = known.lock().expect("known lock");
        map.get(address_text)
            .cloned()
            .or_else(|| {
                map.iter()
                    .find(|(k, _)| normalize_address(k) == target_norm)
                    .map(|(_, v)| v.clone())
            })
    };
    if let Some(id) = id {
        if let Ok(peripheral) = find_peripheral_by_id(adapter, &id).await {
            return Some(peripheral);
        }
    }

    if let Ok(peripherals) = adapter.peripherals().await {
        for peripheral in peripherals {
            if let Ok(Some(props)) = peripheral.properties().await {
                let text = format_address(props.address);
                let matched = text == address_text
                    || normalize_address(&text) == target_norm
                    || parsed.is_some_and(|t| props.address == t);
                if matched {
                    known
                        .lock()
                        .expect("known lock")
                        .insert(text, peripheral.id());
                    return Some(peripheral);
                }
            }
        }
    }
    None
}

fn normalize_address(addr: &str) -> String {
    addr.replace('-', ":").to_ascii_uppercase()
}
