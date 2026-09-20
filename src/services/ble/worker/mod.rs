//! BLE 异步 worker：扫描、连接、GATT 与协议通知处理。
//!
//! 实现按职责拆在同模块的 `support` / `scan` / `session` / `connect`（`include!`，逻辑不变）。
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
use super::http_ota::run_http_ota;
use super::state::{LinkPhase, ScanLinkHint, SharedBleState};
use super::target::{
    adv_link_hint_from_properties, is_target_manufacturer_data, is_target_properties, matches_ff00,
    AdvLinkHint,
};
use super::uuids::{notify_uuid, notify_uuid_ff03, write_uuid};
use super::radio::{
    is_radio_unavailable_detail, message_for_scan_failure, radio_unavailable_reason, NO_ADAPTER_HINT,
};
use crate::services::modbus::{SharedModbusLive, SharedQueryPollLive};
use crate::services::firmware::{HttpOtaJob, OtaJob, SharedOtaLive, PHASE_SUCCESS};
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
    StartHttpOta { job: HttpOtaJob },
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
    StartHttpOta { job: HttpOtaJob },
}

include!("support.rs");
include!("scan.rs");
include!("session.rs");
include!("connect.rs");

async fn open_ble_adapter() -> Result<(Manager, Adapter), String> {
    let manager = Manager::new()
        .await
        .map_err(|err| format!("蓝牙初始化失败：{err}"))?;
    let adapters = manager
        .adapters()
        .await
        .map_err(|err| format!("枚举蓝牙适配器失败：{err}"))?;
    let adapter = adapters
        .into_iter()
        .next()
        .ok_or_else(|| NO_ADAPTER_HINT.to_string())?;
    Ok((manager, adapter))
}

async fn refresh_radio_status(
    adapter: &Adapter,
    state: &SharedBleState,
    ui_refresh: &super::UiRefreshSlot,
    scan_task: &mut Option<tokio::task::JoinHandle<()>>,
) {
    match radio_unavailable_reason(adapter).await {
        Some(reason) => {
            let (phase, detail) = {
                let inner = state.lock().expect("ble state lock");
                (inner.phase, inner.status_detail.clone())
            };
            if matches!(
                phase,
                LinkPhase::Connecting
                    | LinkPhase::GattReady
                    | LinkPhase::Handshake
                    | LinkPhase::Encrypted
            ) {
                return;
            }
            if phase == LinkPhase::Idle && detail == reason && scan_task.is_none() {
                return;
            }
            enter_idle(adapter, state, ui_refresh, scan_task, reason).await;
        }
        None => {
            let should_clear = {
                let inner = state.lock().expect("ble state lock");
                inner.phase == LinkPhase::Idle && is_radio_unavailable_detail(&inner.status_detail)
            };
            if should_clear {
                set_phase(state, LinkPhase::Idle, "蓝牙已打开，可以扫描");
                notify_ui_force(ui_refresh, true);
            }
        }
    }
}

/// 没有适配器时不要退出 worker，否则之后点「扫描」没有任何反应。
async fn wait_for_ble_adapter(
    cmd_rx: &mut mpsc::UnboundedReceiver<BleCommand>,
    state: &SharedBleState,
    ui_refresh: &super::UiRefreshSlot,
) -> Option<(Manager, Adapter)> {
    loop {
        match open_ble_adapter().await {
            Ok(pair) => return Some(pair),
            Err(msg) => {
                warn!(target: "ble_gui::worker", "{msg}");
                set_phase(state, LinkPhase::Idle, msg);
                notify_ui_force(ui_refresh, true);
            }
        }
        loop {
            match cmd_rx.recv().await {
                None => return None,
                Some(BleCommand::StartScan) => break,
                Some(_) => {
                    set_phase(
                        state,
                        LinkPhase::Idle,
                        NO_ADAPTER_HINT,
                    );
                    notify_ui_force(ui_refresh, true);
                }
            }
        }
    }
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
    let Some((_manager, adapter)) =
        wait_for_ble_adapter(&mut cmd_rx, &state, &ui_refresh).await
    else {
        return;
    };

    let mut scan_task: Option<tokio::task::JoinHandle<()>> = None;
    let mut session: Option<ActiveSession> = None;
    let known: KnownMap = Arc::new(Mutex::new(HashMap::new()));
    let expect_drop: SharedExpectDrop = Arc::new(Mutex::new(None));
    refresh_radio_status(&adapter, &state, &ui_refresh, &mut scan_task).await;

    loop {
        if session
            .as_ref()
            .is_some_and(|s| s.notify_task.is_finished())
        {
            info!(target: "ble_gui::worker", "会话已结束，刷新连接状态");
            if let Some(active) = session.take() {
                abort_session(&active);
                disconnect_peripheral(&active.peripheral).await;
            }
            stop_polling(&poll_policy);
            clear_live_on_disconnect(&modbus_live);
            if still_shows_connected(&state) {
                set_phase(&state, LinkPhase::Idle, "设备已断开");
                notify_ui_force(&ui_refresh, true);
            }
            continue;
        }

        let ota_wants_drop = take_ota_disconnect_request(&ota_live);
        let maybe_cmd = if ota_wants_drop && session.is_some() {
            info!(target: "ble_gui::worker", "OTA 停止后断开蓝牙，让设备恢复 Wi-Fi");
            Some(BleCommand::Disconnect)
        } else {
            tokio::select! {
                cmd = cmd_rx.recv() => cmd,
                _ = tokio::time::sleep(Duration::from_millis(250)), if session.is_some() => {
                    continue;
                }
                _ = tokio::time::sleep(Duration::from_millis(1000)), if session.is_none() => {
                    refresh_radio_status(&adapter, &state, &ui_refresh, &mut scan_task).await;
                    continue;
                }
            }
        };
        let Some(cmd) = maybe_cmd else {
            break;
        };

        match cmd {
            BleCommand::StartScan => {
                if let Some(reason) = radio_unavailable_reason(&adapter).await {
                    enter_idle(&adapter, &state, &ui_refresh, &mut scan_task, reason).await;
                    continue;
                }
                if state.lock().expect("ble state lock").phase == LinkPhase::Scanning
                    && scan_task.is_some()
                {
                    continue;
                }
                if let Some(active) = session.take() {
                    abort_session(&active);
                    stop_polling(&poll_policy);
                    clear_live_on_disconnect(&modbus_live);
                    disconnect_peripheral(&active.peripheral).await;
                }
                restart_scan(
                    &adapter,
                    &state,
                    &event_tx,
                    &ui_refresh,
                    &known,
                    &mut scan_task,
                    "正在扫描附近蓝牙设备……",
                )
                .await;
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

                let had_session = if let Some(active) = session.take() {
                    abort_session(&active);
                    stop_polling(&poll_policy);
                    clear_live_on_disconnect(&modbus_live);
                    disconnect_peripheral(&active.peripheral).await;
                    true
                } else {
                    false
                };
                if had_session {
                    if sleep_unless_cancelled(
                        &cancel_connect,
                        Duration::from_millis(CONNECT_SETTLE_MS),
                    )
                    .await
                    .is_err()
                    {
                        enter_idle(
                            &adapter,
                            &state,
                            &ui_refresh,
                            &mut scan_task,
                            "已取消连接",
                        )
                        .await;
                        continue;
                    }
                }

                let scan_was_running = scan_task.is_some();
                // 刚连过别的设备或扫描已停时，Windows 缓存的 Peripheral 往往 GATT 失败。
                // 只在「当前扫描中、且本会话还没连过」时用缓存句柄。
                let mut peripheral = if !had_session && scan_was_running {
                    find_peripheral(&adapter, &address, &known).await
                } else {
                    None
                };
                let require_fresh_adv = had_session || !scan_was_running;

                if peripheral.is_none() {
                    if connect_cancelled(&cancel_connect) {
                        enter_idle(
                            &adapter,
                            &state,
                            &ui_refresh,
                            &mut scan_task,
                            "已取消连接",
                        )
                        .await;
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
                        require_fresh_adv,
                    )
                    .await
                    {
                        Ok(p) => peripheral = Some(p),
                        Err(RediscoverError::Cancelled) => {
                            enter_idle(
                                &adapter,
                                &state,
                                &ui_refresh,
                                &mut scan_task,
                                "已取消连接",
                            )
                            .await;
                            continue;
                        }
                        Err(RediscoverError::Occupied) => {
                            warn!(
                                target: "ble_gui::services::ble",
                                "Connect aborted for {address}: occupied"
                            );
                            enter_idle(
                                &adapter,
                                &state,
                                &ui_refresh,
                                &mut scan_task,
                                format!("连接失败：{MSG_DEVICE_OCCUPIED}"),
                            )
                            .await;
                            continue;
                        }
                        Err(RediscoverError::TimedOut) => {
                            warn!(
                                target: "ble_gui::services::ble",
                                "Connect aborted for {address}: rediscover timeout"
                            );
                            enter_idle(
                                &adapter,
                                &state,
                                &ui_refresh,
                                &mut scan_task,
                                format!("连接失败：{MSG_DEVICE_NOT_NEARBY}"),
                            )
                            .await;
                            continue;
                        }
                    }
                }

                if connect_cancelled(&cancel_connect) {
                    enter_idle(
                        &adapter,
                        &state,
                        &ui_refresh,
                        &mut scan_task,
                        "已取消连接",
                    )
                    .await;
                    continue;
                }

                if let Some(task) = scan_task.take() {
                    task.abort();
                }
                let _ = adapter.stop_scan().await;

                let Some(peripheral) = peripheral else {
                    enter_idle(
                        &adapter,
                        &state,
                        &ui_refresh,
                        &mut scan_task,
                        format!("连接失败：{MSG_DEVICE_NOT_NEARBY}"),
                    )
                    .await;
                    continue;
                };

                if let Ok(Some(props)) = peripheral.properties().await {
                    if adv_link_hint_from_properties(&props) == AdvLinkHint::Occupied {
                        warn!(
                            target: "ble_gui::services::ble",
                            "Connect aborted for {address}: occupied (from cache)"
                        );
                        enter_idle(
                            &adapter,
                            &state,
                            &ui_refresh,
                            &mut scan_task,
                            format!("连接失败：{MSG_DEVICE_OCCUPIED}"),
                        )
                        .await;
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
                        enter_idle(
                            &adapter,
                            &state,
                            &ui_refresh,
                            &mut scan_task,
                            detail,
                        )
                        .await;
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
                let peripheral = session.take().map(|active| {
                    abort_session(&active);
                    active.peripheral
                });
                stop_polling(&poll_policy);
                clear_live_on_disconnect(&modbus_live);
                set_phase(&state, LinkPhase::Idle, "设备已断开");
                notify_ui_force(&ui_refresh, true);
                if let Some(peripheral) = peripheral.as_ref() {
                    disconnect_peripheral(peripheral).await;
                }
                enter_idle(
                    &adapter,
                    &state,
                    &ui_refresh,
                    &mut scan_task,
                    "设备已断开",
                )
                .await;
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
                    g.freeze_elapsed();
                }
            }
            BleCommand::StartHttpOta { job } => {
                if let Some(active) = &session {
                    let _ = active.cmd_tx.send(SessionCommand::StartHttpOta { job });
                } else if let Ok(mut g) = ota_live.lock() {
                    g.running = false;
                    g.phase = crate::services::firmware::PHASE_FAILED;
                    g.result_text = "升级失败".into();
                    g.fail_reason = "未连接设备".into();
                    g.stage_text = "升级失败".into();
                    g.freeze_elapsed();
                }
            }
        }
    }
}
