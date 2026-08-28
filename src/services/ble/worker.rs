//! BLE 异步 worker：扫描、连接、GATT 与协议通知处理。

use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use btleplug::api::{
    BDAddr, Central, CentralEvent, Manager as _, Peripheral as _, PeripheralProperties,
    ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral, PeripheralId};
use futures::StreamExt;
use log::{debug, info, warn};
use tokio::sync::mpsc;
use uuid::Uuid;

use super::protocol::{HandshakePhase, ProtocolSession};
use super::state::{LinkPhase, SharedBleState};
use super::target::{is_target_manufacturer_data, is_target_properties, matches_ff00};
use super::uuids::{notify_uuid, notify_uuid_ff03, write_uuid};

pub enum BleCommand {
    StartScan,
    StopScan,
    Connect { address: String },
    Disconnect,
}

type KnownMap = Arc<Mutex<HashMap<String, PeripheralId>>>;

struct ActiveSession {
    notify_task: tokio::task::JoinHandle<()>,
    peripheral: Peripheral,
}

fn set_phase(state: &SharedBleState, phase: LinkPhase, detail: impl Into<String>) {
    if let Ok(mut inner) = state.lock() {
        inner.phase = phase;
        inner.status_detail = detail.into();
    }
}

fn notify_ui(
    ui_refresh: &super::UiRefreshSlot,
) {
    notify_ui_force(ui_refresh, false);
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

pub async fn worker_main(
    mut cmd_rx: mpsc::UnboundedReceiver<BleCommand>,
    state: SharedBleState,
    event_tx: std::sync::mpsc::Sender<()>,
    ui_refresh: super::UiRefreshSlot,
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
                    info!(target: "ble_gui::services::ble", "StartScan ignored: already scanning");
                    continue;
                }
                if let Some(active) = session.take() {
                    active.notify_task.abort();
                    let _ = active.peripheral.disconnect().await;
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
                    inner.status_detail = "正在扫描附近 BLE 设备……".into();
                }
                known.lock().expect("known lock").clear();
                info!(target: "ble_gui::services::ble", "StartScan: list cleared, spawning scan loop");
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
                info!(target: "ble_gui::services::ble", "StopScan requested");
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
                info!(target: "ble_gui::services::ble", "StopScan done: {count} devices in list");
                notify_ui_force(&ui_refresh, true);
            }
            BleCommand::Connect { address } => {
                if let Some(task) = scan_task.take() {
                    task.abort();
                }
                let _ = adapter.stop_scan().await;

                set_phase(
                    &state,
                    LinkPhase::Connecting,
                    format!("正在连接 {address}……"),
                );
                notify_ui_force(&ui_refresh, true);

                if let Some(active) = session.take() {
                    active.notify_task.abort();
                    let _ = active.peripheral.disconnect().await;
                }

                match connect_device(
                    &adapter,
                    &state,
                    &event_tx,
                    &ui_refresh,
                    &address,
                    &known,
                )
                .await {
                    Ok(active) => session = Some(active),
                    Err(err) => {
                        warn!(target: "ble_gui::services::ble", "Connect failed for {address}: {err}");
                        set_phase(&state, LinkPhase::Idle, format!("连接失败：{err}"));
                        notify_ui_force(&ui_refresh, true);
                    }
                }
            }
            BleCommand::Disconnect => {
                if let Some(active) = session.take() {
                    active.notify_task.abort();
                    let _ = active.peripheral.disconnect().await;
                }
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
    info!(target: "ble_gui::services::ble", "scan loop started");

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
                                let _ = ingest_peripheral(
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
                                .await;
                            }
                            CentralEvent::ServicesAdvertisement { id, services } => {
                                let force_target = services.iter().any(|uuid| matches_ff00(uuid));
                                let _ = ingest_peripheral(
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
                                .await;
                            }
                            CentralEvent::ManufacturerDataAdvertisement { id, manufacturer_data } => {
                                let force_target = manufacturer_data
                                    .values()
                                    .any(|data| is_target_manufacturer_data(data));
                                if force_target {
                                    let _ = ingest_peripheral(
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
                                    .await;
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
        debug!(target: "ble_gui::services::ble", "peripherals() failed");
        return;
    };
    let gen_before = state.lock().expect("ble state lock").scan_list_generation;
    for peripheral in peripherals {
        let _ = ingest_peripheral(
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
        .await;
    }
    let scanning = state.lock().expect("ble state lock").phase == LinkPhase::Scanning;
    if scanning {
        update_scan_status_detail(state);
        notify_ui(ui_refresh);
    } else if !require_scanning && gen_before != state.lock().expect("ble state lock").scan_list_generation {
        notify_ui_force(ui_refresh, true);
    }
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
        .filter(|d| !d.name.starts_with("BLUETTI ("))
        .count();
    inner.status_detail = format!("已发现 {total} 个 BLUETTI 设备，其中 {named} 个已解析名称");
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
        inner.upsert_advertisement(&name, &address, rssi as i32, is_target);
        inner.scan_list_generation > gen_before || inner.scan_devices.len() > before_len
    };

    if list_changed {
        debug!(
            target: "ble_gui::services::ble",
            "ingest + {address} name={name:?} rssi={rssi} target={is_target}"
        );
    }

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
    event_tx: std::sync::mpsc::Sender<()>,
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
            inner.upsert_advertisement(&name, &address, rssi as i32, is_target_properties(&props));
        }
        update_scan_status_detail(&state);
        notify_ui(&ui_refresh);
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
            inner.upsert_advertisement(&name, &address, rssi as i32, is_target);
        }
        update_scan_status_detail(&state);
        notify_ui(&ui_refresh);
        return;
    }
}

async fn connect_device(
    adapter: &Adapter,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    address_text: &str,
    known: &KnownMap,
) -> Result<ActiveSession, String> {
    let target_addr = parse_address(address_text).ok_or_else(|| "MAC 地址格式无效".to_string())?;

    let peripheral = find_peripheral(adapter, target_addr, known)
        .await
        .ok_or_else(|| "未找到目标设备，请重新扫描".to_string())?;

    peripheral
        .connect()
        .await
        .map_err(|e| format!("BLE 连接失败：{e}"))?;

    set_phase(state, LinkPhase::Connecting, "正在发现 GATT 服务……");
    notify_ui_force(ui_refresh, true);

    discover_services_with_retry(&peripheral, 10).await?;

    let characteristics = peripheral.characteristics();
    let write_id = write_uuid();
    let notify_id = notify_uuid();
    let notify_ff03 = notify_uuid_ff03();

    let write_char = characteristics
        .iter()
        .find(|c| c.uuid == write_id)
        .cloned()
        .ok_or_else(|| "未发现 FF02 写特征".to_string())?;

    let notify_char = characteristics
        .iter()
        .find(|c| c.uuid == notify_id)
        .cloned()
        .or_else(|| characteristics.iter().find(|c| c.uuid == notify_ff03).cloned())
        .ok_or_else(|| "未发现 FF01/FF03 通知特征".to_string())?;

    peripheral
        .subscribe(&notify_char)
        .await
        .map_err(|e| format!("订阅通知失败：{e}"))?;

    let props = peripheral.properties().await.ok().flatten();
    let device_name = props
        .as_ref()
        .and_then(|p| p.local_name.clone())
        .unwrap_or_else(|| "BLE Device".to_string());
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

    let (write_tx, mut write_rx) = mpsc::unbounded_channel::<Vec<u8>>();
    let protocol = Arc::new(Mutex::new(ProtocolSession::new()));
    let protocol_for_notify = protocol.clone();
    let state_for_notify = state.clone();
    let event_for_notify = event_tx.clone();
    let peripheral_for_write = peripheral.clone();
    let write_char_for_task = write_char.clone();
    let state_for_plain = state.clone();
    let event_for_plain = event_tx.clone();

    let peripheral_for_session = peripheral.clone();
    let notify_task = tokio::spawn(async move {
        let mut notifications = match peripheral.notifications().await {
            Ok(stream) => stream,
            Err(_) => return,
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
                    }
                }
                maybe_write = write_rx.recv() => {
                    match maybe_write {
                        Some(data) => {
                            let _ = peripheral_for_write
                                .write(&write_char_for_task, &data, WriteType::WithResponse)
                                .await;
                        }
                        None => break,
                    }
                }
                maybe_notification = notifications.next() => {
                    match maybe_notification {
                        Some(notification) => {
                            let response = {
                                let mut session = protocol_for_notify.lock().expect("protocol lock");
                                let result = session.on_notification(&notification.value);
                                let encrypted = session.is_encryption_ready();
                                let phase = session.phase;
                                (result, encrypted, phase)
                            };
                            match response.0 {
                                Ok(Some(bytes)) => {
                                    let _ = write_tx.send(bytes);
                                }
                                Ok(None) => {}
                                Err(err) => {
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
                            } else if response.2 == HandshakePhase::AuthDone {
                                if let Ok(mut inner) = state_for_notify.lock() {
                                    inner.phase = LinkPhase::Handshake;
                                    inner.status_detail = "2A2A 鉴权成功，ECDH 握手中……".into();
                                }
                                let _ = event_for_notify.send(());
                            }
                        }
                        None => break,
                    }
                }
            }
        }
    });

    Ok(ActiveSession {
        notify_task,
        peripheral: peripheral_for_session,
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

async fn find_peripheral(
    adapter: &Adapter,
    target: BDAddr,
    known: &KnownMap,
) -> Option<Peripheral> {
    let target_text = format_address(target);
    let target_norm = normalize_address(&target_text);
    let id = {
        let map = known.lock().expect("known lock");
        map.get(&target_text)
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
                if props.address == target
                    || normalize_address(&format_address(props.address)) == target_norm
                {
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
