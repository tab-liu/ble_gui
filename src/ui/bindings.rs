use slint::{ModelRc, VecModel};

use crate::services::ble::{BleScanEntry, BleSnapshot};
use crate::services::firmware::FirmwareSnapshot;
use crate::services::modbus::ModbusDashboard;
use crate::state::AppContext;
use crate::ui::MainWindow;

const DEFAULT_RSSI_MIN: i32 = -70;

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
    let mut result: Vec<BleScanEntry> = devices.to_vec();

    let needle = name_filter.trim();
    if !needle.is_empty() {
        let needle = needle.to_lowercase();
        result.retain(|d| d.name.to_lowercase().contains(&needle));
    }

    if rssi_filter_enabled {
        let min_rssi = parse_rssi_min(rssi_min_text);
        result.retain(|d| d.rssi >= min_rssi);
    }

    result.sort_by(|a, b| b.rssi.cmp(&a.rssi));
    result
}

fn scan_devices_model(devices: &[BleScanEntry]) -> ModelRc<crate::ui::BleScanDevice> {
    ModelRc::new(VecModel::from(
        devices
            .iter()
            .map(|d| crate::ui::BleScanDevice {
                name: d.name.clone().into(),
                address: d.address.clone().into(),
                rssi: d.rssi_text().into(),
            })
            .collect::<Vec<_>>(),
    ))
}

fn refresh_ble_scan_list(ui: &MainWindow, snap: &BleSnapshot) {
    ui.set_scan_device_total(snap.scan_devices.len() as i32);
    let filtered = prepare_scan_devices(
        &snap.scan_devices,
        ui.get_ble_scan_filter().as_str(),
        ui.get_ble_scan_rssi_filter_enabled(),
        ui.get_ble_scan_rssi_min().as_str(),
    );
    ui.set_scan_devices(scan_devices_model(&filtered));
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
    ui.set_device_connected(snap.connected);
    ui.set_ble_status_text(snap.status_text.clone().into());
    ui.set_ble_device_name(snap.device_name.clone().into());
    ui.set_ble_rssi_text(snap.rssi_text.clone().into());
    ui.set_ble_action_text(snap.action_text.clone().into());
    ui.set_scanning(snap.scanning);
    refresh_ble_scan_list(ui, snap);
}

pub fn refresh_ble_scan_filter(ui: &MainWindow, snap: &BleSnapshot) {
    refresh_ble_scan_list(ui, snap);
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
