use log::debug;
use slint::{Model, ModelRc, VecModel};
use std::cell::RefCell;
use std::rc::Rc;

use crate::services::ble::{BleScanEntry, BleSnapshot};
use crate::services::firmware::FirmwareSnapshot;
use crate::services::modbus::ModbusDashboard;
use crate::state::AppContext;
use crate::ui::BleScanDevice;
use crate::ui::MainWindow;

const DEFAULT_RSSI_MIN: i32 = -70;

struct ScanListCache {
    rows: Rc<VecModel<BleScanDevice>>,
    addresses: Vec<String>,
}

const SCAN_RSSI_REFRESH_MS: u64 = 1000;

struct BleUiCache {
    scan_generation: u64,
    filter_key: String,
    last_list_refresh_ms: u64,
}

thread_local! {
    static SCAN_LIST_CACHE: RefCell<Option<ScanListCache>> = RefCell::new(None);
    static BLE_UI_CACHE: RefCell<BleUiCache> = RefCell::new(BleUiCache {
        scan_generation: u64::MAX,
        filter_key: String::new(),
        last_list_refresh_ms: 0,
    });
}

fn now_ms() -> u64 {
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as u64)
        .unwrap_or(0)
}

fn parse_rssi_min(text: &str) -> i32 {
    text.trim()
        .trim_end_matches("dBm")
        .trim()
        .parse()
        .unwrap_or(DEFAULT_RSSI_MIN)
}

fn prepare_scan_devices(
    devices: &[BleScanEntry],
    name_filter: &str,
    rssi_filter_enabled: bool,
    rssi_min_text: &str,
) -> Vec<BleScanEntry> {
    let mut result: Vec<BleScanEntry> = devices
        .iter()
        .filter(|d| d.is_target)
        .cloned()
        .collect();

    let needle = name_filter.trim();
    if !needle.is_empty() {
        let needle = needle.to_lowercase();
        result.retain(|d| d.name.to_lowercase().contains(&needle));
    }

    if rssi_filter_enabled {
        let min_rssi = parse_rssi_min(rssi_min_text);
        result.retain(|d| d.rssi >= min_rssi);
    }

    result.sort_by(|a, b| {
        b.is_target
            .cmp(&a.is_target)
            .then_with(|| b.initial_rssi.cmp(&a.initial_rssi))
    });
    result
}

fn scan_empty_message(snap: &BleSnapshot, filtered_len: usize) -> String {
    if filtered_len > 0 {
        return String::new();
    }
    let raw = snap.scan_devices.len();
    if raw > 0 {
        return format!("已发现 {raw} 个 BLUETTI 设备，均被当前过滤条件隐藏（请清空名称过滤或关闭信号强度过滤）");
    }
    if snap.scanning {
        "正在扫描 BLUETTI 设备…".into()
    } else {
        "暂无 BLUETTI 设备，请点击顶部「扫描设备」".into()
    }
}

fn scan_filter_key(ui: &MainWindow) -> String {
    format!(
        "{}|{}|{}",
        ui.get_ble_scan_filter(),
        ui.get_ble_scan_rssi_filter_enabled(),
        ui.get_ble_scan_rssi_min(),
    )
}

fn should_refresh_scan_list(ui: &MainWindow, snap: &BleSnapshot) -> bool {
    let filter_key = scan_filter_key(ui);
    BLE_UI_CACHE.with(|cell| {
        let cache = cell.borrow();
        if filter_key != cache.filter_key {
            return true;
        }
        if snap.scanning {
            // 扫描中统一按 1s 周期刷新（新设备、RSSI 均在此周期内更新）。
            return now_ms().saturating_sub(cache.last_list_refresh_ms) >= SCAN_RSSI_REFRESH_MS;
        }
        snap.scan_list_generation != cache.scan_generation
    })
}

fn mark_scan_list_refreshed(ui: &MainWindow, snap: &BleSnapshot) {
    BLE_UI_CACHE.with(|cell| {
        let mut cache = cell.borrow_mut();
        cache.scan_generation = snap.scan_list_generation;
        cache.filter_key = scan_filter_key(ui);
        cache.last_list_refresh_ms = now_ms();
    });
}

fn refresh_ble_status(ui: &MainWindow, snap: &BleSnapshot) {
    ui.set_device_connected(snap.connected);
    ui.set_ble_status_text(snap.status_text.clone().into());
    ui.set_ble_device_name(snap.device_name.clone().into());
    ui.set_ble_rssi_text(snap.rssi_text.clone().into());
    ui.set_ble_action_text(snap.action_text.clone().into());
    ui.set_scanning(snap.scanning);
    ui.set_ble_connecting(snap.connecting);
}

fn entries_to_devices(entries: &[BleScanEntry]) -> Vec<BleScanDevice> {
    entries
        .iter()
        .map(|d| BleScanDevice {
            name: d.name.clone().into(),
            address: d.address.clone().into(),
            rssi: d.rssi_text().into(),
        })
        .collect()
}

/// 尽量原地更新 VecModel，避免扫描期间整表替换导致列表行无法点击。
fn sync_scan_devices(ui: &MainWindow, filtered: &[BleScanEntry]) {
    let devices = entries_to_devices(filtered);
    let addresses: Vec<String> = filtered.iter().map(|d| d.address.clone()).collect();

    SCAN_LIST_CACHE.with(|cell| {
        let mut cache = cell.borrow_mut();
        if let Some(cached) = cache.as_ref() {
            if cached.addresses == addresses {
                for (i, dev) in devices.iter().enumerate() {
                    let unchanged = cached.rows.row_data(i).is_some_and(|row| {
                        row.name == dev.name && row.address == dev.address && row.rssi == dev.rssi
                    });
                    if !unchanged {
                        cached.rows.set_row_data(i, dev.clone());
                    }
                }
                return;
            }
        }

        let rows = Rc::new(VecModel::from(devices));
        ui.set_scan_devices(ModelRc::new(rows.clone()));
        *cache = Some(ScanListCache { rows, addresses });
    });
}

fn refresh_ble_scan_list(ui: &MainWindow, snap: &BleSnapshot) {
    let filtered = prepare_scan_devices(
        &snap.scan_devices,
        ui.get_ble_scan_filter().as_str(),
        ui.get_ble_scan_rssi_filter_enabled(),
        ui.get_ble_scan_rssi_min().as_str(),
    );

    debug!(
        target: "ble_gui::ui",
        "refresh scan list: phase={:?} scanning={} raw={} filtered={}",
        snap.link_phase,
        snap.scanning,
        snap.scan_devices.len(),
        filtered.len(),
    );

    ui.set_scan_device_total(filtered.len() as i32);
    sync_scan_devices(ui, &filtered);
    ui.set_scan_empty_message(scan_empty_message(snap, filtered.len()).into());

    let selected = ui.get_selected_scan_address().to_string();
    if !selected.is_empty() && !filtered.iter().any(|d| d.address == selected) {
        ui.set_selected_scan_address("".into());
    }

    mark_scan_list_refreshed(ui, snap);
}

/// 将各服务状态同步到 Slint UI。
pub fn refresh_all(ui: &MainWindow, ctx: &AppContext) {
    refresh_ble(ui, &ctx.ble.snapshot());
    if ctx.ble.is_connected() {
        refresh_dashboard(ui, &ctx.modbus.dashboard_snapshot());
    } else {
        refresh_dashboard(ui, &ctx.modbus.disconnected_dashboard());
    }
    refresh_firmware(ui, &ctx.firmware.snapshot());
}

pub fn refresh_ble(ui: &MainWindow, snap: &BleSnapshot) {
    if should_refresh_scan_list(ui, snap) {
        refresh_ble_scan_list(ui, snap);
    }
    refresh_ble_status(ui, snap);
}

pub fn refresh_ble_scan_filter(ui: &MainWindow, snap: &BleSnapshot) {
    refresh_ble_scan_list(ui, snap);
    refresh_ble_status(ui, snap);
}

pub fn refresh_dashboard(ui: &MainWindow, dash: &ModbusDashboard) {
    ui.set_metrics(ModelRc::new(VecModel::from(dash.metrics.clone())));
    ui.set_dashboard_summary(ModelRc::new(VecModel::from(
        dash.summary
            .iter()
            .map(|s| slint::SharedString::from(s.as_str()))
            .collect::<Vec<_>>(),
    )));
}

pub fn refresh_firmware(ui: &MainWindow, snap: &FirmwareSnapshot) {
    ui.set_firmware_current_version(snap.current_version.clone().into());
    ui.set_firmware_status_text(snap.status_text.clone().into());
}

pub fn close_dialog(ui: &MainWindow) {
    ui.set_dialog_kind(crate::state::DIALOG_NONE);
    ui.set_dialog_name("".into());
}
