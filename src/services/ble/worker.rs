//! BLE 异步 worker：扫描、连接、GATT 与协议通知处理。

use std::collections::{HashMap, HashSet};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use btleplug::api::{
    BDAddr, Central, CentralEvent, Manager as _, Peripheral as _,
    ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral, PeripheralId};
use futures::StreamExt;
use log::{info, warn};
use tokio::sync::mpsc;

use super::poll::{
    clear_live_on_disconnect, init_live_on_connect, write_control_register, ModbusGate,
};
use super::poll_executor::{poll_foreground, poll_foreground_once};
use super::poll_policy::{describe_poll_foreground, ensure_dashboard_poll_if_idle, PollForeground, SharedPollPolicy};
use super::protocol::{HandshakePhase, ProtocolSession};
use super::state::{LinkPhase, ScanLinkHint, SharedBleState};
use super::target::{
    adv_link_hint_from_properties, is_target_manufacturer_data, is_target_properties, matches_ff00,
    AdvLinkHint,
};
use super::uuids::{notify_uuid, notify_uuid_ff03, write_uuid};
use crate::services::modbus::{SharedModbusLive, SharedQueryPollLive};
use crate::services::ble::modbus::POLL_INTERVAL_MS;

pub enum BleCommand {
    StartScan,
    StopScan,
    Connect { address: String },
    Disconnect,
    WriteRegister { address: u16, value: u16 },
}

enum SessionCommand {
    WriteRegister { address: u16, value: u16 },
}

type KnownMap = Arc<Mutex<HashMap<String, PeripheralId>>>;

struct ActiveSession {
    notify_task: tokio::task::JoinHandle<()>,
    poll_task: tokio::task::JoinHandle<()>,
    peripheral: Peripheral,
    cmd_tx: tokio::sync::mpsc::UnboundedSender<SessionCommand>,
}

fn abort_session(active: &ActiveSession) {
    active.poll_task.abort();
    active.notify_task.abort();
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

    while let Some(cmd) = cmd_rx.recv().await {
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
                if let Some(active) = &session {
                    let _ = active.cmd_tx.send(SessionCommand::WriteRegister { address, value });
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

    peripheral
        .connect()
        .await
        .map_err(|e| format!("BLE 连接失败：{e}"))?;

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
            info!(target: "ble_gui::worker", "已订阅 FF01 + FF03（TLV 异步数据走 FF03）");
        } else {
            info!(target: "ble_gui::worker", "已订阅 FF03（无 FF01，TLV/通知均走 FF03）");
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

    let modbus_gate: ModbusGate = Arc::new(tokio::sync::Mutex::new(()));
    let gate_for_poll = modbus_gate.clone();
    let gate_for_write = modbus_gate.clone();
    let event_for_poll = event_tx.clone();
    let protocol_for_poll_task = protocol.clone();
    let poll_task = tokio::spawn(async move {
        tokio::time::sleep(Duration::from_millis(900)).await;
        let mut poll_timer = tokio::time::interval(Duration::from_millis(POLL_INTERVAL_MS));
        poll_timer.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
        loop {
            poll_timer.tick().await;
            let ready = protocol_for_poll_task
                .lock()
                .expect("protocol lock")
                .modbus_ready();
            if !ready {
                continue;
            }
            let _ok = poll_foreground(
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
    });

    let poll_abort = poll_task.abort_handle();
    let peripheral_for_session = peripheral.clone();
    let notify_task = tokio::spawn(async move {
        let mut notifications = match peripheral.notifications().await {
            Ok(stream) => stream,
            Err(_) => {
                poll_abort.abort();
                return;
            }
        };

        let plain_timer = tokio::time::sleep(Duration::from_secs(6));
        tokio::pin!(plain_timer);

        loop {
            tokio::select! {
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
                        let p = protocol_for_notify.clone();
                        let w = write_tx.clone();
                        let l = modbus_live_write.clone();
                        let q = query_live_write.clone();
                        let poll_gen = query_gen_write.clone();
                        let pol = poll_policy_notify.clone();
                        let g = gate_for_write.clone();
                        let ui = ui_refresh_for_notify.clone();
                        let ev = event_for_notify.clone();
                        tokio::spawn(async move {
                            tokio::time::sleep(Duration::from_millis(300)).await;
                            poll_foreground_once(&pol, &p, &w, &l, &q, &poll_gen, &g).await;
                            notify_ui_force(&ui, true);
                            let _ = ev.send(());
                        });
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
                        None => {
                            poll_abort.abort();
                            break;
                        }
                    }
                }
                maybe_write = write_rx.recv() => {
                    match maybe_write {
                        Some(data) => {
                            match peripheral_for_write
                                .write(&write_char_for_task, &data, WriteType::WithResponse)
                                .await
                            {
                                Ok(()) => {}
                                Err(err) => {
                                    warn!(target: "ble_gui::worker", "GATT 写入失败: {err}");
                                }
                            }
                        }
                        None => {
                            poll_abort.abort();
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
                                // 仅首次加密就绪时拉一次仪表板（周期任务也会轮询）。
                                let p = protocol_for_notify.clone();
                                let w = write_tx.clone();
                                let l = modbus_live_write.clone();
                                let q = query_live_write.clone();
                                let poll_gen = query_gen_write.clone();
                                let pol = poll_policy_notify.clone();
                                let g = gate_for_write.clone();
                                let ui = ui_refresh_for_notify.clone();
                                let ev = event_for_notify.clone();
                                tokio::spawn(async move {
                                    tokio::time::sleep(Duration::from_millis(900)).await;
                                    poll_foreground_once(&pol, &p, &w, &l, &q, &poll_gen, &g).await;
                                    notify_ui_force(&ui, true);
                                    let _ = ev.send(());
                                });
                            } else if response.2 == HandshakePhase::AuthDone {
                                if let Ok(mut inner) = state_for_notify.lock() {
                                    inner.phase = LinkPhase::Handshake;
                                    inner.status_detail = "2A2A 鉴权成功，ECDH 握手中……".into();
                                }
                                let _ = event_for_notify.send(());
                            } else if response.3 {
                                let _ = event_for_notify.send(());
                            }
                        }
                        None => {
                            poll_abort.abort();
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
        peripheral: peripheral_for_session,
        cmd_tx: session_cmd_tx,
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
