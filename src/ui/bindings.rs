use slint::{Model, ModelRc, VecModel};
use std::cell::RefCell;
use std::rc::Rc;

use crate::services::ble::{BleScanEntry, BleSnapshot};
use crate::services::firmware::FirmwareSnapshot;
use crate::services::modbus::DashboardData;
use crate::state::AppContext;
use crate::ui::BleScanDevice;
use crate::ui::MainWindow;

const DEFAULT_RSSI_MIN: i32 = -70;

struct ScanListCache {
    rows: Rc<VecModel<BleScanDevice>>,
    addresses: Vec<String>,
}

struct BleUiCache {
    scan_generation: u64,
    filter_key: String,
}

thread_local! {
    static SCAN_LIST_CACHE: RefCell<Option<ScanListCache>> = RefCell::new(None);
    static BLE_UI_CACHE: RefCell<BleUiCache> = RefCell::new(BleUiCache {
        scan_generation: u64::MAX,
        filter_key: String::new(),
    });
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

    result
}

fn scan_empty_message(snap: &BleSnapshot, filtered_len: usize) -> String {
    if filtered_len > 0 {
        return String::new();
    }
    let raw = snap.scan_devices.len();
    if raw > 0 {
        return format!("已发现 {raw} 个蓝牙设备，均被当前过滤条件隐藏（请清空名称过滤或关闭信号强度过滤）");
    }
    if snap.scanning {
        "正在扫描蓝牙设备…".into()
    } else {
        "暂无蓝牙设备，请点击顶部「扫描设备」".into()
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
        filter_key != cache.filter_key || snap.scan_list_generation != cache.scan_generation
    })
}

fn mark_scan_list_refreshed(ui: &MainWindow, snap: &BleSnapshot) {
    BLE_UI_CACHE.with(|cell| {
        let mut cache = cell.borrow_mut();
        cache.scan_generation = snap.scan_list_generation;
        cache.filter_key = scan_filter_key(ui);
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

/// 尽量原地更新或末尾追加，避免整表替换导致已有行跳动。
fn sync_scan_devices(ui: &MainWindow, filtered: &[BleScanEntry]) {
    let devices = entries_to_devices(filtered);
    let addresses: Vec<String> = filtered.iter().map(|d| d.address.clone()).collect();

    SCAN_LIST_CACHE.with(|cell| {
        let mut cache = cell.borrow_mut();
        if let Some(cached) = cache.as_mut() {
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

            // 发现顺序追加：仅追加新行，已有行不动。
            if addresses.len() > cached.addresses.len()
                && addresses[..cached.addresses.len()] == cached.addresses[..]
            {
                for dev in devices.iter().skip(cached.addresses.len()) {
                    cached.rows.push(dev.clone());
                }
                cached.addresses = addresses;
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
    let connected = ctx.ble.is_connected();
    refresh_ble(ui, &ctx.ble.snapshot());
    if connected {
        ctx.modbus.on_connected();
    } else {
        ctx.modbus.on_disconnected();
    }
    refresh_modbus_dashboard(ui, &ctx.modbus);
    refresh_firmware(ui, &ctx.firmware.snapshot());
}

pub fn refresh_modbus_dashboard(ui: &MainWindow, modbus: &crate::services::modbus::ModbusService) {
    refresh_modbus_dashboard_from_live(ui, &modbus.shared_live());
}

pub fn refresh_modbus_dashboard_from_live(
    ui: &MainWindow,
    live: &crate::services::modbus::SharedModbusLive,
) {
    let (dash, busy, _) = live
        .lock()
        .map(|l| (l.dashboard.clone(), l.output_busy, l.modbus_online))
        .unwrap_or_default();
    refresh_dashboard(ui, &dash, busy);
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

pub fn refresh_dashboard(ui: &MainWindow, dash: &DashboardData, output_busy: bool) {
    ui.set_dashboard_soc(dash.soc);
    ui.set_dashboard_ac_output_w(dash.ac_output_w);
    ui.set_dashboard_dc_output_w(dash.dc_output_w);
    ui.set_dashboard_pv_input_w(dash.pv_input_w);
    ui.set_dashboard_ac_input_w(dash.ac_input_w);
    ui.set_dashboard_data_valid(dash.data_valid);
    ui.set_ac_output_on(dash.ac_output_on);
    ui.set_dc_output_on(dash.dc_output_on);
    ui.set_output_control_busy(output_busy);
}

pub fn refresh_firmware(ui: &MainWindow, snap: &FirmwareSnapshot) {
    ui.set_firmware_current_version(snap.current_version.clone().into());
    ui.set_firmware_status_text(snap.status_text.clone().into());
}

pub fn close_dialog(ui: &MainWindow) {
    ui.set_dialog_kind(crate::state::DIALOG_NONE);
    ui.set_dialog_name("".into());
}
