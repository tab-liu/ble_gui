//! 扫描本机附近 WiFi，供配网时选择 SSID。
//!
//! 通过 [`kawaiifi`] 统一调用各平台扫描能力，只保留 2.4GHz 热点。
//! macOS 扫前会请求定位权限（系统要求，否则 SSID 常为空）。
//! 请尽量在 UI 主线程先调用 [`prepare_platform_access`]，系统弹窗才稳定出现。

use kawaiifi::Band;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NearbyAp {
    pub ssid: String,
    pub signal: String,
    pub band: String,
}

/// 平台侧准备（macOS：在主线程弹出定位授权）。应在 UI 主线程调用。
pub fn prepare_platform_access() {
    #[cfg(target_os = "macos")]
    request_location_prompt_on_main_thread();
}

pub fn scan_nearby() -> Result<Vec<NearbyAp>, String> {
    #[cfg(target_os = "macos")]
    wait_for_location_authorization();

    let interface = kawaiifi::default_interface()
        .map_err(|e| format!("枚举无线网卡失败：{e}"))?
        .ok_or_else(|| "未找到无线网卡".to_string())?;

    let scan = interface
        .scan_blocking()
        .map_err(|e| format!("扫描失败：{e}"))?;

    let mut merged: Vec<NearbyAp> = Vec::new();
    let mut saw_hidden = false;
    for bss in scan.bss_list() {
        if bss.band() != Band::TwoPointFourGhz {
            continue;
        }
        let name = bss.ssid().unwrap_or("").trim().to_string();
        if name.is_empty() {
            saw_hidden = true;
            continue;
        }
        upsert_ap(&mut merged, name, signal_pct_from_dbm(bss.signal_dbm()));
    }

    if merged.is_empty() {
        if saw_hidden {
            #[cfg(target_os = "macos")]
            show_location_help_dialog();
            return Err(
                "未读到 WiFi 名称：请在定位授权弹窗中点「允许」，或到「系统设置 → 隐私与安全性 → 定位服务」中开启「BLE Modbus 工具」后再扫"
                    .into(),
            );
        }
        return Err("未扫描到 2.4GHz WiFi".into());
    }
    Ok(finalize_list(merged))
}

#[cfg(target_os = "macos")]
mod macos_location {
    use std::cell::RefCell;
    use std::time::Duration;

    use objc2::rc::Retained;
    use objc2_core_location::{CLAuthorizationStatus, CLLocationManager};
    use log::{info, warn};

    thread_local! {
        /// 主线程持有；过早 drop 会导致系统弹窗消失。
        static LOCATION_MANAGER: RefCell<Option<Retained<CLLocationManager>>> = const { RefCell::new(None) };
    }

    fn current_status() -> CLAuthorizationStatus {
        // 类方法可在任意线程查询，避免跨线程持有 CLLocationManager。
        #[allow(deprecated)]
        unsafe {
            CLLocationManager::authorizationStatus_class()
        }
    }

    fn bundle_debug() -> String {
        use objc2_foundation::NSBundle;
        let bundle = NSBundle::mainBundle();
        let id = bundle
            .bundleIdentifier()
            .map(|s| s.to_string())
            .unwrap_or_else(|| "(none)".into());
        let path = bundle.bundlePath().to_string();
        format!("id={id} path={path}")
    }

    pub(super) fn request_location_prompt_on_main_thread() {
        LOCATION_MANAGER.with(|cell| {
            let mut slot = cell.borrow_mut();
            if slot.is_none() {
                *slot = Some(unsafe { CLLocationManager::new() });
            }
            let manager = slot.as_ref().expect("manager");
            let status = unsafe { manager.authorizationStatus() };
            info!(
                target: "ble_gui::wifi_scan",
                "macOS 定位授权状态(主线程请求前)={} {}",
                status.0,
                bundle_debug()
            );
            if status == CLAuthorizationStatus::AuthorizedAlways
                || status == CLAuthorizationStatus::AuthorizedWhenInUse
            {
                return;
            }
            if status == CLAuthorizationStatus::Denied {
                warn!(
                    target: "ble_gui::wifi_scan",
                    "定位已被拒绝；请在系统设置中开启「BLE Modbus 工具」"
                );
                return;
            }
            unsafe { manager.requestWhenInUseAuthorization() };
            // 触发一次定位更新，有助于 TCC 把本 Bundle 登记进「定位服务」列表。
            unsafe { manager.startUpdatingLocation() };
            info!(
                target: "ble_gui::wifi_scan",
                "已调用 requestWhenInUseAuthorization + startUpdatingLocation"
            );
        });
    }

    pub(super) fn wait_for_location_authorization() {
        // 给用户足够时间点系统弹窗（最多约 30s）。
        for _ in 0..300 {
            let status = current_status();
            if status != CLAuthorizationStatus::NotDetermined {
                info!(
                    target: "ble_gui::wifi_scan",
                    "macOS 定位授权结果={}",
                    status.0
                );
                return;
            }
            std::thread::sleep(Duration::from_millis(100));
        }
        warn!(
            target: "ble_gui::wifi_scan",
            "等待定位授权超时；bundle={}",
            bundle_debug()
        );
    }

    pub(super) fn show_location_help_dialog() {
        let _ = rfd::MessageDialog::new()
            .set_level(rfd::MessageLevel::Warning)
            .set_title("需要定位权限")
            .set_description(
                "扫描附近 WiFi 名称需要 macOS 定位权限。\n\n\
请在系统弹窗中点「允许」，或到「系统设置 → 隐私与安全性 → 定位服务」中开启「BLE Modbus 工具」。\n\n\
必须使用 `cargo run` 启动（会自动签名），不要直接运行未签名的二进制。",
            )
            .set_buttons(rfd::MessageButtons::Ok)
            .show();
        let _ = std::process::Command::new("open")
            .arg("x-apple.systempreferences:com.apple.preference.security?Privacy_LocationServices")
            .spawn();
    }
}

#[cfg(target_os = "macos")]
use macos_location::{
    request_location_prompt_on_main_thread, show_location_help_dialog,
    wait_for_location_authorization,
};

fn finalize_list(mut list: Vec<NearbyAp>) -> Vec<NearbyAp> {
    list.sort_by(|a, b| {
        signal_pct(&a.signal)
            .cmp(&signal_pct(&b.signal))
            .reverse()
            .then_with(|| a.ssid.cmp(&b.ssid))
    });
    list.truncate(40);
    list
}

fn upsert_ap(out: &mut Vec<NearbyAp>, ssid: String, pct: u8) {
    let signal = format!("{pct}%");
    if let Some(existing) = out.iter_mut().find(|ap| ap.ssid == ssid) {
        if signal_pct(&signal) > signal_pct(&existing.signal) {
            existing.signal = signal;
        }
        return;
    }
    out.push(NearbyAp {
        ssid,
        signal,
        band: "2.4G".into(),
    });
}

fn signal_pct(text: &str) -> u8 {
    text.trim_end_matches('%')
        .split_whitespace()
        .next()
        .and_then(|s| s.parse().ok())
        .unwrap_or(0)
}

fn signal_pct_from_dbm(dbm: i32) -> u8 {
    ((2 * (i64::from(dbm) + 100)).clamp(0, 100)) as u8
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rssi_maps_into_percent() {
        assert_eq!(signal_pct_from_dbm(-100), 0);
        assert_eq!(signal_pct_from_dbm(-50), 100);
        assert_eq!(signal_pct_from_dbm(-75), 50);
    }

    #[test]
    fn upsert_keeps_stronger_signal() {
        let mut list = Vec::new();
        upsert_ap(&mut list, "Home".into(), 40);
        upsert_ap(&mut list, "Home".into(), 80);
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].signal, "80%");
    }
}
