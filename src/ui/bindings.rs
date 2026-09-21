//! 将服务层快照刷到 Slint 属性（BLE 列表、仪表板、固件文案等）。
//!
//! 尽量做增量更新（扫描列表缓存），避免每帧重建整个 model 造成闪烁。

use slint::{Model, ModelRc, VecModel};
use std::cell::RefCell;
use std::rc::Rc;
use std::sync::Mutex;

use log::warn;

use crate::services::ble::modbus::rssi_quality_text;
use crate::services::ble::modbus::sn_type_name;
use crate::services::ble::{BleScanEntry, BleSnapshot, ScanLinkHint};
use crate::services::ble_favorites::{self, FavoriteDevice};
use crate::services::firmware::FirmwareSnapshot;
use crate::services::modbus::{DashboardData, ModbusReadMode, SubDevice};
use crate::services::poll_sync::sync_poll_policy;
use crate::state::{AppContext, PAGE_DASHBOARD, PAGE_DEVICE_CONFIG, PAGE_EXTERNAL};
use crate::ui::{BleFavoriteDevice, BleScanDevice, MainWindow, SubDeviceInfo};

const DEFAULT_RSSI_MIN: i32 = -70;

struct ScanListCache {
    rows: Rc<VecModel<BleScanDevice>>,
    addresses: Vec<String>,
}

#[derive(Default)]
struct RssiFilterHold {
    /// `None` 表示过滤关闭或阈值刚改过，下一轮按硬阈值重新入列。
    min_rssi: Option<i32>,
    addresses: Vec<String>,
}

impl RssiFilterHold {
    fn reset(&mut self) {
        self.min_rssi = None;
        self.addresses.clear();
    }

    fn apply(&mut self, min_rssi: i32, devices: &mut Vec<BleScanEntry>) {
        if self.min_rssi != Some(min_rssi) {
            self.min_rssi = Some(min_rssi);
            self.addresses.clear();
        }
        apply_rssi_latch(devices, min_rssi, &self.addresses);
        self.addresses = devices.iter().map(|d| d.address.clone()).collect();
    }
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
    static SUB_DEVICE_FP: RefCell<String> = RefCell::new(String::new());
    static RSSI_FILTER_HOLD: RefCell<RssiFilterHold> = RefCell::new(RssiFilterHold {
        min_rssi: None,
        addresses: Vec::new(),
    });
}

fn parse_rssi_min(text: &str) -> i32 {
    text.trim()
        .trim_end_matches("dBm")
        .trim()
        .parse()
        .unwrap_or(DEFAULT_RSSI_MIN)
}

fn rssi_held(addresses: &[String], address: &str) -> bool {
    addresses
        .iter()
        .any(|held| ble_favorites::addresses_equal(held, address))
}

/// 达到阈值才入列；入列后不因 RSSI 变弱踢走。
/// 设备从扫描列表消失后 hold 会丢掉，弱信号再出现需重新达标。
fn apply_rssi_latch(devices: &mut Vec<BleScanEntry>, min_rssi: i32, previously_visible: &[String]) {
    devices.retain(|d| rssi_held(previously_visible, &d.address) || d.rssi >= min_rssi);
}

fn prepare_scan_devices(
    devices: &[BleScanEntry],
    name_filter: &str,
    rssi_filter_enabled: bool,
    rssi_min_text: &str,
    favorites: &[FavoriteDevice],
    rssi_hold: &mut RssiFilterHold,
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
        rssi_hold.apply(parse_rssi_min(rssi_min_text), &mut result);
    } else {
        rssi_hold.reset();
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
            let live_name = seen_entry
                .map(|d| d.name.as_str())
                .filter(|n| ble_favorites::is_usable_advertised_name(n));
            let name = if let Some(n) = live_name {
                n.to_string()
            } else if !fav.name.is_empty() {
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
    let filtered = RSSI_FILTER_HOLD.with(|cell| {
        let mut hold = cell.borrow_mut();
        prepare_scan_devices(
            &snap.scan_devices,
            ui.get_ble_scan_filter().as_str(),
            ui.get_ble_scan_rssi_filter_enabled(),
            ui.get_ble_scan_rssi_min().as_str(),
            favorites,
            &mut hold,
        )
    });

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
    refresh_ble(ui, &ctx.favorites, &ctx.ble.snapshot(), read_mode);
    if connected {
        ctx.modbus.on_connected();
    } else {
        ctx.modbus.on_disconnected();
    }
    if page == PAGE_DASHBOARD {
        refresh_modbus_dashboard(ui, &ctx.modbus);
    }
    if page == PAGE_EXTERNAL {
        refresh_external_devices(ui, &ctx.modbus);
    }
    if page == PAGE_DEVICE_CONFIG {
        crate::pages::device_config::refresh_builtin_availability(ui, ctx);
    }
    let (versions, iot_ver, dev_type, sn, software) = if connected {
        ctx.modbus
            .shared_live()
            .lock()
            .map(|l| {
                (
                    l.device_versions_text.clone(),
                    l.iot_software_version,
                    l.device_type.clone(),
                    l.device_sn.clone(),
                    l.device_software.clone(),
                )
            })
            .unwrap_or_default()
    } else {
        (
            String::new(),
            None,
            String::new(),
            String::new(),
            Vec::new(),
        )
    };
    ctx.firmware.sync_connection(connected);
    ctx.firmware
        .apply_device_info(versions, iot_ver, dev_type, sn, software);
    refresh_firmware(ui, &ctx.firmware.snapshot(connected));
}

pub fn refresh_modbus_dashboard(ui: &MainWindow, modbus: &crate::services::modbus::ModbusService) {
    refresh_modbus_dashboard_from_live(ui, &modbus.shared_live());
}

pub fn refresh_modbus_dashboard_from_live(
    ui: &MainWindow,
    live: &crate::services::modbus::SharedModbusLive,
) {
    let snap = live.lock().ok();
    let (dash, busy) = snap
        .as_ref()
        .map(|l| (l.dashboard.clone(), l.output_busy))
        .unwrap_or_default();
    refresh_dashboard(ui, &dash, busy);

    let empty = || "—".to_string();
    let or_dash = |s: &str| {
        if s.trim().is_empty() {
            empty()
        } else {
            s.to_string()
        }
    };

    let (
        identity_ready,
        device_type,
        device_sn,
        iot_type,
        iot_sn,
        safe_code,
        cloud_url,
        wifi_mac,
        ble_mac,
        wifi_ok,
        cloud_ok,
        ssid_now,
        wifi_password,
        rssi_text,
        sta_ip,
    ) = snap
        .as_ref()
        .map(|l| {
            let rssi_text = if l.wifi_sta && l.sta_rssi != 0 {
                format!("{} dBm（{}）", l.sta_rssi, rssi_quality_text(l.sta_rssi))
            } else {
                empty()
            };
            (
                l.identity_loaded || l.device_info_loaded,
                or_dash(&l.device_type),
                or_dash(&l.device_sn),
                or_dash(&l.iot_type),
                or_dash(&l.iot_sn),
                or_dash(&l.safe_code),
                or_dash(&l.cloud_url),
                or_dash(&l.wifi_mac),
                or_dash(&l.ble_mac),
                l.wifi_sta,
                l.mqtt_ok,
                or_dash(&l.ssid_now),
                l.wifi_password.clone(),
                rssi_text,
                or_dash(&l.sta_ip),
            )
        })
        .unwrap_or_else(|| {
            (
                false,
                empty(),
                empty(),
                empty(),
                empty(),
                empty(),
                empty(),
                empty(),
                empty(),
                false,
                false,
                empty(),
                String::new(),
                empty(),
                empty(),
            )
        });

    ui.set_dashboard_identity_ready(identity_ready);
    ui.set_dashboard_device_type(device_type.into());
    ui.set_dashboard_device_sn(device_sn.into());
    ui.set_dashboard_iot_type(iot_type.into());
    ui.set_dashboard_iot_sn(iot_sn.into());
    ui.set_dashboard_safe_code(safe_code.into());
    ui.set_dashboard_cloud_url(cloud_url.into());
    ui.set_dashboard_wifi_mac(wifi_mac.into());
    ui.set_dashboard_ble_mac(ble_mac.into());
    ui.set_dashboard_wifi_ok(wifi_ok);
    ui.set_dashboard_cloud_ok(cloud_ok);
    ui.set_dashboard_wifi_status_label(if wifi_ok { "WiFi 已连接" } else { "WiFi 未连接" }.into());
    ui.set_dashboard_cloud_status_label(if cloud_ok { "云端已登录" } else { "云端未登录" }.into());
    ui.set_dashboard_ssid_now(ssid_now.into());
    ui.set_dashboard_wifi_password(wifi_password.into());
    ui.set_dashboard_wifi_rssi_text(rssi_text.into());
    ui.set_dashboard_sta_ip(sta_ip.into());
}

pub fn refresh_external_devices(ui: &MainWindow, modbus: &crate::services::modbus::ModbusService) {
    refresh_external_devices_from_live(ui, &modbus.shared_live());
}

pub fn refresh_external_devices_from_live(
    ui: &MainWindow,
    live: &crate::services::modbus::SharedModbusLive,
) {
    let snap = live.lock().ok();
    let (devices, valid) = snap
        .as_ref()
        .map(|l| (l.sub_devices.clone(), l.sub_devices_valid))
        .unwrap_or_default();
    ui.set_external_devices_valid(valid);
    let fp = devices
        .iter()
        .map(|d| format!("{}:{}:{}:{}", d.sn, d.state, d.dev_type, d.slave_addr))
        .collect::<Vec<_>>()
        .join("|");
    let changed = SUB_DEVICE_FP.with(|cached| {
        if *cached.borrow() == fp {
            false
        } else {
            *cached.borrow_mut() = fp;
            true
        }
    });
    if changed {
        ui.set_external_devices(ModelRc::new(VecModel::from(
            devices.iter().map(sub_device_to_ui).collect::<Vec<_>>(),
        )));
    }
}

fn sub_device_to_ui(dev: &SubDevice) -> SubDeviceInfo {
    let type_name = sn_type_name(dev.dev_type);
    SubDeviceInfo {
        role: if dev.is_self { "本机".into() } else { "配件".into() },
        type_name: if type_name.is_empty() {
            "—".into()
        } else {
            type_name.into()
        },
        sn: {
            let sn = dev.sn_text();
            if sn.is_empty() {
                "—".into()
            } else {
                sn.into()
            }
        },
        addr_text: dev.addr_text().into(),
        status_text: dev.status_text().into(),
        online: dev.online(),
        alarm: dev.alarm() || dev.protect() || dev.bat_alarm(),
    }
}

fn synced_favorite_snapshot(
    store: &Mutex<Vec<FavoriteDevice>>,
    snap: &BleSnapshot,
) -> Vec<FavoriteDevice> {
    let Ok(mut favorites) = store.lock() else {
        return Vec::new();
    };
    let changed = ble_favorites::apply_advertised_names(
        &mut favorites,
        snap.scan_devices
            .iter()
            .filter(|d| d.is_target)
            .map(|d| (d.address.as_str(), d.name.as_str())),
    );
    if changed {
        if let Err(e) = ble_favorites::save(&favorites) {
            warn!(target: "ble_gui::favorites", "保存收藏名称失败: {e}");
        }
    }
    favorites.clone()
}

pub fn refresh_ble(
    ui: &MainWindow,
    favorites: &Mutex<Vec<FavoriteDevice>>,
    snap: &BleSnapshot,
    read_mode: Option<ModbusReadMode>,
) {
    let favorites = synced_favorite_snapshot(favorites, snap);
    let fav_fp = favorites_fingerprint(&favorites, snap);
    if should_refresh_scan_list(ui, snap, &fav_fp) {
        refresh_ble_scan_list(ui, snap, &favorites);
    } else {
        sync_favorite_devices(ui, &favorites, snap);
    }
    refresh_ble_status(ui, snap, read_mode);
}

pub fn refresh_ble_scan_filter(
    ui: &MainWindow,
    favorites: &Mutex<Vec<FavoriteDevice>>,
    snap: &BleSnapshot,
    read_mode: Option<ModbusReadMode>,
) {
    let favorites = synced_favorite_snapshot(favorites, snap);
    refresh_ble_scan_list(ui, snap, &favorites);
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
    ui.set_firmware_device_type(snap.device_type.clone().into());
    ui.set_firmware_device_sn(snap.device_sn.clone().into());
    ui.set_firmware_status_text(snap.status_text.clone().into());
    ui.set_firmware_file_name(snap.file_name.clone().into());
    ui.set_firmware_file_size(snap.file_size_text.clone().into());
    ui.set_firmware_md5(snap.md5.clone().into());
    ui.set_firmware_type_text(snap.type_text.clone().into());
    ui.set_firmware_image_version(snap.image_version.clone().into());
    if ui.get_firmware_ota_version().as_str() != snap.ota_version_text {
        ui.set_firmware_ota_version(snap.ota_version_text.clone().into());
    }
    ui.set_firmware_layout_text(snap.layout_text.clone().into());
    ui.set_firmware_parse_source(snap.parse_source.clone().into());
    ui.set_firmware_dev_model(snap.dev_model.clone().into());
    ui.set_firmware_has_file(snap.has_file);
    ui.set_firmware_part_mismatch(snap.part_mismatch);
    ui.set_firmware_phase(snap.phase);
    ui.set_firmware_progress(snap.progress);
    ui.set_firmware_stage_text(snap.stage_text.clone().into());
    ui.set_firmware_elapsed_text(snap.elapsed_text.clone().into());
    ui.set_firmware_result_text(snap.result_text.clone().into());
    ui.set_firmware_fail_reason(snap.fail_reason.clone().into());
    ui.set_firmware_can_start(snap.can_start);
    ui.set_firmware_can_stop(snap.can_stop);
}

pub fn close_dialog(ui: &MainWindow) {
    ui.set_dialog_kind(crate::state::DIALOG_NONE);
    ui.set_dialog_name("".into());
}

#[cfg(test)]
mod tests {
    use super::*;

    fn entry(address: &str, rssi: i32) -> BleScanEntry {
        BleScanEntry {
            name: "dev".into(),
            address: address.into(),
            rssi,
            is_target: true,
            link_hint: ScanLinkHint::Unknown,
        }
    }

    fn addrs(devices: &[BleScanEntry]) -> Vec<&str> {
        devices.iter().map(|d| d.address.as_str()).collect()
    }

    #[test]
    fn new_device_must_meet_min_rssi() {
        let mut devices = vec![entry("AA:BB", -71), entry("CC:DD", -70)];
        apply_rssi_latch(&mut devices, -70, &[]);
        assert_eq!(addrs(&devices), vec!["CC:DD"]);
    }

    #[test]
    fn held_device_stays_even_when_rssi_plunges() {
        let hold = vec!["AA:BB".into()];
        let mut devices = vec![entry("AA:BB", -95)];
        apply_rssi_latch(&mut devices, -70, &hold);
        assert_eq!(addrs(&devices), vec!["AA:BB"]);
    }

    #[test]
    fn hold_state_adds_but_does_not_drop() {
        let mut hold = RssiFilterHold::default();
        let favorites: [FavoriteDevice; 0] = [];

        let shown = prepare_scan_devices(
            &[entry("AA:BB", -70)],
            "",
            true,
            "-70",
            &favorites,
            &mut hold,
        );
        assert_eq!(addrs(&shown), vec!["AA:BB"]);

        let shown = prepare_scan_devices(
            &[entry("AA:BB", -90), entry("CC:DD", -80)],
            "",
            true,
            "-70",
            &favorites,
            &mut hold,
        );
        assert_eq!(addrs(&shown), vec!["AA:BB"]);
    }

    #[test]
    fn changing_threshold_resets_hold() {
        let mut hold = RssiFilterHold::default();
        let favorites: [FavoriteDevice; 0] = [];
        let _ = prepare_scan_devices(
            &[entry("AA:BB", -70)],
            "",
            true,
            "-70",
            &favorites,
            &mut hold,
        );
        let shown = prepare_scan_devices(
            &[entry("AA:BB", -68)],
            "",
            true,
            "-60",
            &favorites,
            &mut hold,
        );
        assert!(shown.is_empty());
    }

    #[test]
    fn occupied_and_favorites_never_enter_hold() {
        let mut hold = RssiFilterHold::default();
        let mut occupied = entry("AA:BB", -40);
        occupied.link_hint = ScanLinkHint::Occupied;
        let favorites = [FavoriteDevice {
            address: "CC:DD".into(),
            name: "fav".into(),
        }];
        let shown = prepare_scan_devices(
            &[occupied, entry("CC:DD", -40), entry("EE:FF", -40)],
            "",
            true,
            "-70",
            &favorites,
            &mut hold,
        );
        assert_eq!(addrs(&shown), vec!["EE:FF"]);
        assert_eq!(hold.addresses, vec!["EE:FF".to_string()]);
    }

    #[test]
    fn leaving_scan_clears_hold_so_weak_return_stays_hidden() {
        let mut hold = RssiFilterHold::default();
        let favorites: [FavoriteDevice; 0] = [];
        let _ = prepare_scan_devices(&[entry("AA:BB", -70)], "", true, "-70", &favorites, &mut hold);
        let _ = prepare_scan_devices(&[], "", true, "-70", &favorites, &mut hold);
        let shown = prepare_scan_devices(&[entry("AA:BB", -90)], "", true, "-70", &favorites, &mut hold);
        assert!(shown.is_empty());
    }

    #[test]
    fn disabling_filter_resets_hold() {
        let mut hold = RssiFilterHold::default();
        let favorites: [FavoriteDevice; 0] = [];
        let _ = prepare_scan_devices(&[entry("AA:BB", -70)], "", true, "-70", &favorites, &mut hold);
        let _ = prepare_scan_devices(&[entry("AA:BB", -90)], "", false, "-70", &favorites, &mut hold);
        let shown = prepare_scan_devices(&[entry("AA:BB", -90)], "", true, "-70", &favorites, &mut hold);
        assert!(shown.is_empty());
    }

    #[test]
    fn parse_rssi_min_accepts_dbm_suffix() {
        assert_eq!(parse_rssi_min(" -70 dBm "), -70);
        assert_eq!(parse_rssi_min("abc"), DEFAULT_RSSI_MIN);
    }
}
