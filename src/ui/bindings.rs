//! 将服务层快照刷到 Slint 属性（BLE 列表、仪表板、固件文案等）。
//!
//! 尽量做增量更新（扫描列表缓存），避免每帧重建整个 model 造成闪烁。

use slint::{Model, ModelRc, VecModel};
use std::cell::RefCell;
use std::rc::Rc;

use crate::services::ble::{BleScanEntry, BleSnapshot, ScanLinkHint};
use crate::services::ble_favorites::{self, FavoriteDevice};
use crate::services::firmware::FirmwareSnapshot;
use crate::services::modbus::{DashboardData, ModbusReadMode};
use crate::services::poll_sync::sync_poll_policy;
use crate::state::{AppContext, PAGE_DASHBOARD, PAGE_DEVICE_CONFIG};
use crate::ui::{BleFavoriteDevice, BleScanDevice, MainWindow};

const DEFAULT_RSSI_MIN: i32 = -70;

struct ScanListCache {
    rows: Rc<VecModel<BleScanDevice>>,
    addresses: Vec<String>,
}

struct FavoriteListCache {
    rows: Rc<VecModel<BleFavoriteDevice>>,
    addresses: Vec<String>,
}

struct BleUiCache {
    scan_generation: u64,
    filter_key: String,
    favorites_fingerprint: String,
}

thread_local! {
    static SCAN_LIST_CACHE: RefCell<Option<ScanListCache>> = RefCell::new(None);
    static FAVORITE_LIST_CACHE: RefCell<Option<FavoriteListCache>> = RefCell::new(None);
    static BLE_UI_CACHE: RefCell<BleUiCache> = RefCell::new(BleUiCache {
        scan_generation: u64::MAX,
        filter_key: String::new(),
        favorites_fingerprint: String::new(),
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
    favorites: &[FavoriteDevice],
) -> Vec<BleScanEntry> {
    let mut result: Vec<BleScanEntry> = devices
        .iter()
        .filter(|d| d.is_target)
        .filter(|d| d.link_hint != ScanLinkHint::Occupied)
        .filter(|d| !ble_favorites::contains(favorites, &d.address))
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
    let raw = snap
        .scan_devices
        .iter()
        .filter(|d| d.is_target && d.link_hint != ScanLinkHint::Occupied)
        .count();
    if raw > 0 {
        return "附近设备均已在左侧收藏，或被当前过滤条件隐藏".into();
    }
    if snap.scanning {
        "正在扫描蓝牙设备…".into()
    } else {
        "暂无扫描结果，可点顶部「扫描设备」，或从左侧收藏 ↗ 定向连接".into()
    }
}

fn favorites_fingerprint(favorites: &[FavoriteDevice], snap: &BleSnapshot) -> String {
    let mut parts: Vec<String> = favorites
        .iter()
        .map(|f| format!("{}:{}", f.address, f.name))
        .collect();
    parts.sort();
    let seen: Vec<String> = snap
        .scan_devices
        .iter()
        .filter(|d| d.is_target)
        .map(|d| format!("{}:{}:{:?}", d.address, d.rssi, d.link_hint))
        .collect();
    format!("{}|{}|{}", parts.join(";"), snap.scanning, seen.join(";"))
}

fn favorite_seen(snap: &BleSnapshot, address: &str) -> bool {
    snap.scan_devices.iter().any(|d| {
        d.is_target && ble_favorites::addresses_equal(&d.address, address)
    })
}

fn favorite_row_unchanged(existing: &BleFavoriteDevice, row: &BleFavoriteDevice) -> bool {
    existing.name == row.name
        && existing.address == row.address
        && existing.rssi == row.rssi
        && existing.seen == row.seen
        && existing.occupied == row.occupied
        && existing.selectable == row.selectable
}

fn favorite_addresses_match(a: &[String], b: &[FavoriteDevice]) -> bool {
    a.len() == b.len()
        && a.iter()
            .zip(b.iter())
            .all(|(left, right)| ble_favorites::addresses_equal(left, &right.address))
}

fn build_favorite_rows(favorites: &[FavoriteDevice], snap: &BleSnapshot) -> Vec<BleFavoriteDevice> {
    favorites
        .iter()
        .map(|fav| {
            let seen_entry = snap.scan_devices.iter().find(|d| {
                d.is_target && ble_favorites::addresses_equal(&d.address, &fav.address)
            });
            let (seen, occupied, rssi) = match seen_entry {
                Some(d) => (
                    true,
                    d.link_hint == ScanLinkHint::Occupied,
                    d.rssi_text(),
                ),
                None => (false, false, String::new()),
            };
            let name = if !fav.name.is_empty() {
                fav.name.clone()
            } else if let Some(d) = seen_entry {
                d.name.clone()
            } else {
                fav.address.clone()
            };
            BleFavoriteDevice {
                name: name.into(),
                address: fav.address.clone().into(),
                rssi: rssi.into(),
                seen,
                occupied,
                selectable: seen && !occupied,
            }
        })
        .collect()
}

fn sync_favorite_devices(ui: &MainWindow, favorites: &[FavoriteDevice], snap: &BleSnapshot) {
    let rows_data = build_favorite_rows(favorites, snap);
    let addresses: Vec<String> = favorites.iter().map(|f| f.address.clone()).collect();

    FAVORITE_LIST_CACHE.with(|cell| {
        let mut cache = cell.borrow_mut();
        if let Some(cached) = cache.as_mut() {
            if favorite_addresses_match(&cached.addresses, favorites) {
                for (i, row) in rows_data.iter().enumerate() {
                    let unchanged = cached.rows.row_data(i).is_some_and(|existing| {
                        favorite_row_unchanged(&existing, row)
                    });
                    if !unchanged {
                        cached.rows.set_row_data(i, row.clone());
                    }
                }
                return;
            }
        }

        let rows = Rc::new(VecModel::from(rows_data));
        ui.set_favorite_devices(ModelRc::new(rows.clone()));
        *cache = Some(FavoriteListCache { rows, addresses });
    });
}

fn scan_filter_key(ui: &MainWindow) -> String {
    format!(
        "{}|{}|{}",
        ui.get_ble_scan_filter(),
        ui.get_ble_scan_rssi_filter_enabled(),
        ui.get_ble_scan_rssi_min(),
    )
}

fn should_refresh_scan_list(ui: &MainWindow, snap: &BleSnapshot, favorites_fp: &str) -> bool {
    let filter_key = scan_filter_key(ui);
    BLE_UI_CACHE.with(|cell| {
        let cache = cell.borrow();
        filter_key != cache.filter_key
            || snap.scan_list_generation != cache.scan_generation
            || favorites_fp != cache.favorites_fingerprint
    })
}

fn mark_scan_list_refreshed(ui: &MainWindow, snap: &BleSnapshot, favorites_fp: String) {
    BLE_UI_CACHE.with(|cell| {
        let mut cache = cell.borrow_mut();
        cache.scan_generation = snap.scan_list_generation;
        cache.filter_key = scan_filter_key(ui);
        cache.favorites_fingerprint = favorites_fp;
    });
}

fn refresh_ble_status(ui: &MainWindow, snap: &BleSnapshot, read_mode: Option<ModbusReadMode>) {
    ui.set_device_connected(snap.connected);
    let status_text = if snap.connected {
        match read_mode {
            Some(ModbusReadMode::Standard) | None => snap.status_text.clone(),
            Some(mode) => format!("{} · Modbus {}", snap.status_text, mode.label()),
        }
    } else {
        snap.status_text.clone()
    };
    ui.set_ble_status_text(status_text.into());
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

fn refresh_ble_scan_list(ui: &MainWindow, snap: &BleSnapshot, favorites: &[FavoriteDevice]) {
    let filtered = prepare_scan_devices(
        &snap.scan_devices,
        ui.get_ble_scan_filter().as_str(),
        ui.get_ble_scan_rssi_filter_enabled(),
        ui.get_ble_scan_rssi_min().as_str(),
        favorites,
    );

    ui.set_scan_device_total(filtered.len() as i32);
    sync_scan_devices(ui, &filtered);
    sync_favorite_devices(ui, favorites, snap);
    ui.set_scan_empty_message(scan_empty_message(snap, filtered.len()).into());

    let selected = ui.get_selected_scan_address().to_string();
    let in_scan = filtered
        .iter()
        .any(|d| ble_favorites::addresses_equal(&d.address, &selected));
    let in_fav_seen = favorite_seen(snap, &selected);
    if !selected.is_empty() && !in_scan && !in_fav_seen {
        ui.set_selected_scan_address("".into());
    }

    mark_scan_list_refreshed(ui, snap, favorites_fingerprint(favorites, snap));
}

pub fn refresh_all(ui: &MainWindow, ctx: &AppContext) {
    let connected = ctx.ble.is_connected();
    if connected {
        sync_poll_policy(ui, ctx);
    }
    let page = ctx.ble.ui_page();
    let read_mode = if connected {
        Some(ctx.modbus.read_mode())
    } else {
        None
    };
    refresh_ble(ui, &ctx.favorites_snapshot(), &ctx.ble.snapshot(), read_mode);
    if connected {
        ctx.modbus.on_connected();
    } else {
        ctx.modbus.on_disconnected();
    }
    if connected && page == PAGE_DASHBOARD {
        refresh_modbus_dashboard(ui, &ctx.modbus);
    }
    if page == PAGE_DEVICE_CONFIG {
        crate::pages::device_config::refresh_builtin_availability(ui, ctx);
    }
    refresh_firmware(ui, &ctx.firmware.snapshot(connected));
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

pub fn refresh_ble(
    ui: &MainWindow,
    favorites: &[FavoriteDevice],
    snap: &BleSnapshot,
    read_mode: Option<ModbusReadMode>,
) {
    let fav_fp = favorites_fingerprint(favorites, snap);
    if should_refresh_scan_list(ui, snap, &fav_fp) {
        refresh_ble_scan_list(ui, snap, favorites);
    } else {
        sync_favorite_devices(ui, favorites, snap);
    }
    refresh_ble_status(ui, snap, read_mode);
}

pub fn refresh_ble_scan_filter(
    ui: &MainWindow,
    favorites: &[FavoriteDevice],
    snap: &BleSnapshot,
    read_mode: Option<ModbusReadMode>,
) {
    refresh_ble_scan_list(ui, snap, favorites);
    refresh_ble_status(ui, snap, read_mode);
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
    ui.set_firmware_current_version(snap.device_version.clone().into());
    ui.set_firmware_status_text(snap.status_text.clone().into());
    ui.set_firmware_file_name(snap.file_name.clone().into());
    ui.set_firmware_file_size(snap.file_size_text.clone().into());
    ui.set_firmware_md5(snap.md5.clone().into());
    ui.set_firmware_type_text(snap.type_text.clone().into());
    ui.set_firmware_image_version(snap.image_version.clone().into());
    ui.set_firmware_layout_text(snap.layout_text.clone().into());
    ui.set_firmware_parse_source(snap.parse_source.clone().into());
    ui.set_firmware_dev_model(snap.dev_model.clone().into());
    ui.set_firmware_has_file(snap.has_file);
    ui.set_firmware_phase(snap.phase);
    ui.set_firmware_progress(snap.progress);
    ui.set_firmware_pc_percent(snap.pc_percent);
    ui.set_firmware_device_percent(snap.device_percent);
    ui.set_firmware_stage_text(snap.stage_text.clone().into());
    ui.set_firmware_result_text(snap.result_text.clone().into());
    ui.set_firmware_fail_reason(snap.fail_reason.clone().into());
    ui.set_firmware_can_start(snap.can_start);
    ui.set_firmware_can_stop(snap.can_stop);
}

pub fn close_dialog(ui: &MainWindow) {
    ui.set_dialog_kind(crate::state::DIALOG_NONE);
    ui.set_dialog_name("".into());
}
