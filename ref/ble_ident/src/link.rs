//! 扫描、加密连接、读整机 / IoT / 安全码。

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use btleplug::api::{Central, Manager as _, Peripheral as _, ScanFilter, ValueNotification, WriteType};
use btleplug::platform::{Adapter, Manager, Peripheral};
use futures::{Stream, StreamExt};
use tokio::sync::mpsc::{UnboundedReceiver, UnboundedSender};

use crate::ble::{
    build_read_holding, is_target_properties, notify_uuid, notify_uuid_ff03, parse_read_holding,
    write_uuid, ProtocolSession,
};
use crate::identity::{
    format_identity, REG_IOT, REG_IOT_COUNT, REG_WHOLE_DEVICE, REG_WHOLE_DEVICE_COUNT, SLAVE_ID,
};
use crate::seal::seal;

const GATT_ATT_PAYLOAD: usize = 244;
const CONNECT_SETTLE: Duration = Duration::from_millis(400);
const CONNECT_ATTEMPTS: u32 = 4;
const READ_TIMEOUT: Duration = Duration::from_millis(1500);
const HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(12);

#[derive(Clone, Debug)]
pub struct FoundDevice {
    pub name: String,
    pub address: String,
    pub rssi: i32,
}

#[derive(Clone, Debug)]
pub enum LinkEvent {
    Status(String),
    Scanning(bool),
    Devices(Vec<FoundDevice>),
    Identity {
        machine: String,
        iot: String,
        safe_sealed: String,
    },
    Disconnected,
}

enum Cmd {
    StartScan,
    StopScan,
    Connect(String),
    Disconnect,
}

pub struct LinkHandle {
    cmd_tx: UnboundedSender<Cmd>,
}

pub fn spawn_link() -> (LinkHandle, Receiver<LinkEvent>) {
    let (cmd_tx, cmd_rx) = tokio::sync::mpsc::unbounded_channel();
    let (event_tx, event_rx) = mpsc::channel();
    std::thread::spawn(move || {
        let runtime = tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .build()
            .expect("tokio runtime");
        runtime.block_on(worker(cmd_rx, event_tx));
    });
    (LinkHandle { cmd_tx }, event_rx)
}

impl LinkHandle {
    pub fn start_scan(&self) {
        let _ = self.cmd_tx.send(Cmd::StartScan);
    }

    pub fn stop_scan(&self) {
        let _ = self.cmd_tx.send(Cmd::StopScan);
    }

    pub fn connect(&self, address: String) {
        let _ = self.cmd_tx.send(Cmd::Connect(address));
    }

    pub fn disconnect(&self) {
        let _ = self.cmd_tx.send(Cmd::Disconnect);
    }
}

struct LiveSession {
    cancel: Arc<AtomicBool>,
    task: tokio::task::JoinHandle<()>,
    peripheral: Peripheral,
}

async fn worker(mut cmd_rx: UnboundedReceiver<Cmd>, events: Sender<LinkEvent>) {
    let adapter = match open_adapter().await {
        Ok(adapter) => Some(adapter),
        Err(err) => {
            let _ = events.send(LinkEvent::Status(err));
            None
        }
    };
    let mut known: HashMap<String, Peripheral> = HashMap::new();
    let scanning = Arc::new(AtomicBool::new(false));
    let mut session: Option<LiveSession> = None;
    let mut tick = tokio::time::interval(Duration::from_millis(400));
    tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
    tick.tick().await;

    loop {
        tokio::select! {
            cmd = cmd_rx.recv() => {
                let Some(cmd) = cmd else { break };
                match cmd {
                    Cmd::StartScan => {
                        if session.is_some() {
                            continue;
                        }
                        let Some(adapter) = adapter.as_ref() else {
                            let _ = events.send(LinkEvent::Status("Bluetooth adapter not found".into()));
                            continue;
                        };
                        if adapter.start_scan(ScanFilter::default()).await.is_err() {
                            let _ = events.send(LinkEvent::Status("Scan failed".into()));
                            continue;
                        }
                        scanning.store(true, Ordering::Relaxed);
                        let _ = events.send(LinkEvent::Scanning(true));
                        let _ = events.send(LinkEvent::Status("Scanning".into()));
                        refresh(adapter, &mut known, &events).await;
                    }
                    Cmd::StopScan => {
                        if let Some(adapter) = adapter.as_ref() {
                            let _ = adapter.stop_scan().await;
                        }
                        scanning.store(false, Ordering::Relaxed);
                        let _ = events.send(LinkEvent::Scanning(false));
                        let _ = events.send(LinkEvent::Status("Scan stopped".into()));
                    }
                    Cmd::Connect(address) => {
                        if session.is_some() {
                            continue;
                        }
                        let Some(adapter) = adapter.clone() else {
                            let _ = events.send(LinkEvent::Status("Bluetooth adapter not found".into()));
                            continue;
                        };
                        let Some(peripheral) = known.get(&address).cloned() else {
                            let _ = events.send(LinkEvent::Status("Device not found".into()));
                            continue;
                        };
                        let cancel = Arc::new(AtomicBool::new(false));
                        let events_task = events.clone();
                        let scanning_task = scanning.clone();
                        let cancel_task = cancel.clone();
                        let peripheral_task = peripheral.clone();
                        let task = tokio::spawn(async move {
                            run_session(adapter, peripheral_task, cancel_task, scanning_task, events_task).await;
                        });
                        session = Some(LiveSession { cancel, task, peripheral });
                    }
                    Cmd::Disconnect => {
                        drop_session(&mut session, &events, true).await;
                    }
                }
            }
            _ = tick.tick() => {
                if session.as_ref().is_some_and(|live| live.task.is_finished()) {
                    session = None;
                }
                if scanning.load(Ordering::Relaxed) && session.is_none() {
                    if let Some(adapter) = adapter.as_ref() {
                        refresh(adapter, &mut known, &events).await;
                    }
                }
            }
        }
    }
}

async fn drop_session(session: &mut Option<LiveSession>, events: &Sender<LinkEvent>, tell_ui: bool) {
    if let Some(live) = session.take() {
        live.cancel.store(true, Ordering::Relaxed);
        let _ = live.peripheral.disconnect().await;
        live.task.abort();
        if tell_ui {
            let _ = events.send(LinkEvent::Status("Disconnected".into()));
            let _ = events.send(LinkEvent::Disconnected);
        }
    }
}

async fn open_adapter() -> Result<Adapter, String> {
    let manager = Manager::new().await.map_err(|e| format!("Bluetooth unavailable: {e}"))?;
    let adapters = manager.adapters().await.map_err(|e| format!("Bluetooth unavailable: {e}"))?;
    adapters.into_iter().next().ok_or_else(|| "Bluetooth adapter not found".into())
}

async fn refresh(adapter: &Adapter, known: &mut HashMap<String, Peripheral>, events: &Sender<LinkEvent>) {
    let Ok(list) = adapter.peripherals().await else {
        return;
    };
    let mut found = Vec::new();
    let mut next = HashMap::new();
    for peripheral in list {
        let Ok(Some(props)) = peripheral.properties().await else {
            continue;
        };
        let name = props.local_name.as_deref().unwrap_or("").trim().to_string();
        if !name.starts_with("AP200") || !is_target_properties(&props) {
            continue;
        }
        let address = props.address.to_string();
        let rssi = props.rssi.unwrap_or(-100) as i32;
        found.push(FoundDevice {
            name,
            address: address.clone(),
            rssi,
        });
        next.insert(address, peripheral);
    }
    found.sort_by(|a, b| b.rssi.cmp(&a.rssi));
    *known = next;
    let _ = events.send(LinkEvent::Devices(found));
}

async fn run_session(
    adapter: Adapter,
    peripheral: Peripheral,
    cancel: Arc<AtomicBool>,
    scanning: Arc<AtomicBool>,
    events: Sender<LinkEvent>,
) {
    let outcome = session_body(&adapter, &peripheral, &cancel, &scanning, &events).await;
    let _ = peripheral.disconnect().await;
    if cancel.load(Ordering::Relaxed) {
        return;
    }
    if let Err(err) = outcome {
        let _ = events.send(LinkEvent::Status(err));
    }
    let _ = events.send(LinkEvent::Disconnected);
}

async fn session_body(
    adapter: &Adapter,
    peripheral: &Peripheral,
    cancel: &AtomicBool,
    scanning: &AtomicBool,
    events: &Sender<LinkEvent>,
) -> Result<(), String> {
    let _ = events.send(LinkEvent::Status("Connecting".into()));
    connect_with_retry(peripheral, cancel).await?;
    if cancelled(cancel) {
        return Ok(());
    }
    scanning.store(false, Ordering::Relaxed);
    let _ = adapter.stop_scan().await;
    let _ = events.send(LinkEvent::Scanning(false));

    let _ = events.send(LinkEvent::Status("Discovering services".into()));
    discover_services(peripheral, cancel).await?;
    if cancelled(cancel) {
        return Ok(());
    }

    let characteristics = peripheral.characteristics();
    let write_char = characteristics
        .iter()
        .find(|c| c.uuid == write_uuid())
        .cloned()
        .ok_or_else(|| "Write characteristic not found".to_string())?;
    let notify_ff01 = characteristics.iter().find(|c| c.uuid == notify_uuid()).cloned();
    let notify_ff03 = characteristics.iter().find(|c| c.uuid == notify_uuid_ff03()).cloned();
    let notify_char = notify_ff01
        .clone()
        .or(notify_ff03.clone())
        .ok_or_else(|| "Notify characteristic not found".to_string())?;
    peripheral
        .subscribe(&notify_char)
        .await
        .map_err(|e| format!("Subscribe failed: {e}"))?;
    if let Some(extra) = notify_ff03 {
        if notify_ff01.is_some() && extra.uuid != notify_char.uuid {
            peripheral
                .subscribe(&extra)
                .await
                .map_err(|e| format!("Subscribe failed: {e}"))?;
        }
    }

    let mut notifications = peripheral
        .notifications()
        .await
        .map_err(|e| format!("Notify stream failed: {e}"))?;

    let (write_tx, mut write_rx) = tokio::sync::mpsc::unbounded_channel::<Vec<u8>>();
    let writer = peripheral.clone();
    let write_char_task = write_char.clone();
    let write_task = tokio::spawn(async move {
        while let Some(data) = write_rx.recv().await {
            for chunk in data.chunks(GATT_ATT_PAYLOAD) {
                if writer
                    .write(&write_char_task, chunk, WriteType::WithResponse)
                    .await
                    .is_err()
                {
                    return;
                }
            }
        }
    });

    let protocol = Arc::new(Mutex::new(ProtocolSession::new()));
    let _ = events.send(LinkEvent::Status("Waiting for encryption".into()));
    let deadline = tokio::time::Instant::now() + HANDSHAKE_TIMEOUT;
    loop {
        if cancelled(cancel) {
            write_task.abort();
            return Ok(());
        }
        if protocol.lock().expect("protocol").is_encryption_ready() {
            break;
        }
        if tokio::time::Instant::now() >= deadline {
            write_task.abort();
            return Err("Encryption failed".into());
        }
        tokio::select! {
            note = notifications.next() => {
                let Some(note) = note else {
                    write_task.abort();
                    return Err("Device disconnected".into());
                };
                if let Err(err) = handle_notification(&protocol, &write_tx, &note.value) {
                    log::warn!(target: "ble_ident::link", "{err}");
                    if !protocol.lock().expect("protocol").is_encryption_ready() {
                        write_task.abort();
                        return Err("Encryption failed".into());
                    }
                }
            }
            _ = tokio::time::sleep(Duration::from_millis(40)) => {}
        }
    }

    let _ = events.send(LinkEvent::Status("Reading".into()));
    let whole = read_holding(
        &protocol,
        &write_tx,
        &mut notifications,
        cancel,
        REG_WHOLE_DEVICE,
        REG_WHOLE_DEVICE_COUNT,
    )
    .await?;
    let iot = read_holding(
        &protocol,
        &write_tx,
        &mut notifications,
        cancel,
        REG_IOT,
        REG_IOT_COUNT,
    )
    .await?;
    let (machine, iot_text, safe_code) = format_identity(&whole, &iot)?;
    let safe_sealed = if safe_code.is_empty() {
        "—".into()
    } else {
        seal(&safe_code)?
    };
    let _ = events.send(LinkEvent::Identity {
        machine,
        iot: iot_text,
        safe_sealed,
    });
    let _ = events.send(LinkEvent::Status("Done".into()));
    write_task.abort();
    Ok(())
}

async fn read_holding(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    notifications: &mut (impl Stream<Item = ValueNotification> + Unpin),
    cancel: &AtomicBool,
    start: u16,
    count: u16,
) -> Result<Vec<u16>, String> {
    let mut last_err = "Read timed out".to_string();
    for _ in 0..2 {
        if cancelled(cancel) {
            return Err("Disconnected".into());
        }
        match read_holding_once(protocol, write_tx, notifications, cancel, start, count).await {
            Ok(values) => return Ok(values),
            Err(err) => last_err = err,
        }
    }
    Err(last_err)
}

async fn read_holding_once(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    notifications: &mut (impl Stream<Item = ValueNotification> + Unpin),
    cancel: &AtomicBool,
    start: u16,
    count: u16,
) -> Result<Vec<u16>, String> {
    let request = build_read_holding(SLAVE_ID, start, count);
    {
        let mut session = protocol.lock().expect("protocol");
        session.clear_modbus_responses();
        let air = session.wrap_modbus_request(&request).map_err(|e| e.to_string())?;
        write_tx.send(air).map_err(|_| "Write channel closed".to_string())?;
    }
    let deadline = tokio::time::Instant::now() + READ_TIMEOUT;
    loop {
        if cancelled(cancel) {
            return Err("Disconnected".into());
        }
        if let Some(frame) = protocol.lock().expect("protocol").pop_modbus_response() {
            if frame.len() >= 2 && frame[0] == SLAVE_ID && (frame[1] == 0x03 || frame[1] & 0x80 != 0) {
                return parse_read_holding(&frame, SLAVE_ID, count).map_err(|e| e.to_string());
            }
        }
        if tokio::time::Instant::now() >= deadline {
            return Err("Read timed out".into());
        }
        tokio::select! {
            note = notifications.next() => {
                let Some(note) = note else {
                    return Err("Device disconnected".into());
                };
                handle_notification(protocol, write_tx, &note.value)?;
            }
            _ = tokio::time::sleep(Duration::from_millis(25)) => {}
        }
    }
}

fn handle_notification(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    data: &[u8],
) -> Result<(), String> {
    let reply = protocol
        .lock()
        .expect("protocol")
        .on_notification(data)
        .map_err(|e| e.to_string())?;
    if let Some(bytes) = reply {
        write_tx.send(bytes).map_err(|_| "Write channel closed".to_string())?;
    }
    Ok(())
}

fn cancelled(cancel: &AtomicBool) -> bool {
    cancel.load(Ordering::Relaxed)
}

async fn connect_with_retry(peripheral: &Peripheral, cancel: &AtomicBool) -> Result<(), String> {
    tokio::time::sleep(CONNECT_SETTLE).await;
    for attempt in 1..=CONNECT_ATTEMPTS {
        if cancelled(cancel) {
            return Ok(());
        }
        if peripheral.is_connected().await.unwrap_or(false) {
            return Ok(());
        }
        match peripheral.connect().await {
            Ok(()) => return Ok(()),
            Err(err) => {
                log::warn!(target: "ble_ident::link", "connect attempt {attempt}: {err}");
                if attempt == CONNECT_ATTEMPTS {
                    return Err("Connect failed".into());
                }
                tokio::time::sleep(Duration::from_millis(400 * u64::from(attempt))).await;
            }
        }
    }
    Err("Connect failed".into())
}

async fn discover_services(peripheral: &Peripheral, cancel: &AtomicBool) -> Result<(), String> {
    for attempt in 1..=10 {
        if cancelled(cancel) {
            return Ok(());
        }
        match peripheral.discover_services().await {
            Ok(()) => {
                let ready = peripheral.characteristics().iter().any(|c| {
                    c.uuid == write_uuid() || c.uuid == notify_uuid() || c.uuid == notify_uuid_ff03()
                });
                if ready {
                    return Ok(());
                }
            }
            Err(err) => {
                log::warn!(target: "ble_ident::link", "discover attempt {attempt}: {err}");
                if attempt == 10 {
                    return Err("Service discovery failed".into());
                }
            }
        }
        tokio::time::sleep(Duration::from_millis(500)).await;
    }
    Err("Service discovery failed".into())
}
