//! 设备配置页的 WiFi 配网 UI：刷状态、写凭据、扫描 AP。
//!
//! 相位数据在 [`crate::state::wifi_provision`]；本模块只碰 Slint。

use std::time::Instant;

use log::warn;
use slint::{ComponentHandle, Model, ModelRc, VecModel};

use crate::services::ble::modbus::{
    encode_write_value, sta_enable_word, wifi_auth_for_password, QueryValueType,
    RegisterFieldPatch, BUILTIN_CONFIG_SLAVE_ID, REG_WIFI_ON_OFF, REG_WIFI_STA_AUTH,
    REG_WIFI_STA_ENABLE, REG_WIFI_STA_PASSWORD, REG_WIFI_STA_PASSWORD_COUNT, REG_WIFI_STA_SSID,
    REG_WIFI_STA_SSID_COUNT, WIFI_PASSWORD_MAX_BYTES, WIFI_SSID_MAX_BYTES,
};
use crate::services::modbus::QueryItemPollResult;
use crate::services::{wifi_cred_store, wifi_scan};
use crate::state::wifi_provision::WifiProvisionPhase;
use crate::state::{AppContext, PAGE_DEVICE_CONFIG};
use crate::ui::{MainWindow, WifiSavedNetwork, WifiScanAp};

pub fn tick_wifi_provision(ui: &MainWindow, ctx: &AppContext) {
    {
        let connected = ctx.ble.is_connected();
        ctx.state
            .borrow_mut()
            .device_config
            .wifi
            .advance(connected);
    }
    if ui.get_current_page() == PAGE_DEVICE_CONFIG {
        refresh_wifi_status(ui, ctx);
    }
}

pub fn apply_wifi_poll_item(ctx: &AppContext, r: &QueryItemPollResult) {
    ctx.state
        .borrow_mut()
        .device_config
        .wifi
        .apply_poll_item(r);
}

pub fn advance_wifi_phase(ctx: &AppContext) {
    let connected = ctx.ble.is_connected();
    ctx.state
        .borrow_mut()
        .device_config
        .wifi
        .advance(connected);
}

pub fn refresh_wifi_status(ui: &MainWindow, ctx: &AppContext) {
    let connected = ctx.ble.is_connected();
    let st = ctx.state.borrow();
    let wifi = &st.device_config.wifi;
    let phase = wifi.phase;
    let (wifi_ok, wifi_pending, cloud_ok, cloud_pending, wifi_label, cloud_label, hint) =
        if !connected {
            (
                false,
                false,
                false,
                false,
                "WiFi 未连接".into(),
                "服务器未连接".into(),
                String::new(),
            )
        } else {
            match phase {
                WifiProvisionPhase::Failed => {
                    let wifi_failed = !wifi.wifi_sta && !wifi.hint.is_empty();
                    let cloud_failed = wifi.wifi_sta && !wifi.mqtt;
                    (
                        wifi.wifi_sta,
                        false,
                        wifi.mqtt,
                        false,
                        if wifi_failed {
                            wifi.hint.clone().into()
                        } else if wifi.wifi_sta {
                            "WiFi 已连接".into()
                        } else {
                            "WiFi 未连接".into()
                        },
                        if wifi.mqtt {
                            "服务器已连接".into()
                        } else if cloud_failed {
                            if wifi.hint.is_empty() {
                                "云端连接超时".into()
                            } else {
                                wifi.hint.clone().into()
                            }
                        } else {
                            "服务器未连接".into()
                        },
                        String::new(),
                    )
                }
                WifiProvisionPhase::ConnectingWifi
                | WifiProvisionPhase::ConnectingCloud
                | WifiProvisionPhase::Idle
                | WifiProvisionPhase::Success => {
                    let wifi_ok = wifi.wifi_sta;
                    let cloud_ok = wifi.mqtt;
                    let wifi_pending =
                        matches!(phase, WifiProvisionPhase::ConnectingWifi) && !wifi_ok;
                    let cloud_pending = wifi_ok && !cloud_ok;
                    (
                        wifi_ok,
                        wifi_pending,
                        cloud_ok,
                        cloud_pending,
                        if wifi_pending {
                            "正在连接 WiFi…".into()
                        } else if wifi_ok {
                            "WiFi 已连接".into()
                        } else {
                            "WiFi 未连接".into()
                        },
                        if cloud_ok {
                            "服务器已连接".into()
                        } else if cloud_pending {
                            "正在连接云端…".into()
                        } else {
                            "服务器未连接".into()
                        },
                        String::new(),
                    )
                }
            }
        };
    let current_ssid = if connected && wifi.wifi_sta {
        let mut extra = String::new();
        if !wifi.ssid_now.is_empty() {
            extra.push_str(&wifi.ssid_now);
        }
        if !wifi.sta_ip.is_empty() {
            if !extra.is_empty() {
                extra.push_str("  ");
            }
            extra.push_str(&wifi.sta_ip);
        }
        extra
    } else {
        String::new()
    };
    drop(st);

    ui.set_wifi_ok(wifi_ok);
    ui.set_wifi_pending(wifi_pending);
    ui.set_wifi_cloud_ok(cloud_ok);
    ui.set_wifi_cloud_pending(cloud_pending);
    ui.set_wifi_status_label(wifi_label);
    ui.set_wifi_cloud_status_label(cloud_label);
    ui.set_wifi_current_ssid(current_ssid.into());
    ui.set_wifi_apply_busy(false);
    if ui.get_wifi_scan_busy() {
        return;
    }
    if !connected
        || matches!(
            phase,
            WifiProvisionPhase::ConnectingWifi
                | WifiProvisionPhase::ConnectingCloud
                | WifiProvisionPhase::Failed
        )
    {
        ui.set_wifi_hint(hint.into());
    }
}

fn persist_wifi_networks(ctx: &AppContext) {
    let saved = ctx.state.borrow().device_config.wifi.saved.clone();
    let networks: Vec<wifi_cred_store::WifiNetwork> = (0..saved.row_count())
        .filter_map(|i| saved.row_data(i))
        .map(|n| wifi_cred_store::WifiNetwork {
            ssid: n.ssid.to_string(),
            password: n.password.to_string(),
        })
        .collect();
    if let Err(e) = wifi_cred_store::save(&networks) {
        warn!(target: "ble_gui::wifi_store", "保存 WiFi 记录失败: {e}");
    }
}

fn upsert_saved_wifi(ctx: &AppContext, ssid: &str, password: &str) {
    let saved = ctx.state.borrow().device_config.wifi.saved.clone();
    for i in (0..saved.row_count()).rev() {
        if saved.row_data(i).is_some_and(|n| n.ssid.as_str() == ssid) {
            saved.remove(i);
        }
    }
    saved.insert(
        0,
        WifiSavedNetwork {
            ssid: ssid.into(),
            password: password.into(),
        },
    );
    while saved.row_count() > wifi_cred_store::MAX_NETWORKS {
        saved.remove(saved.row_count() - 1);
    }
    persist_wifi_networks(ctx);
}

fn apply_wifi_credentials(ui: &MainWindow, ctx: &AppContext) {
    if !ctx.ble.is_connected() {
        ui.set_wifi_hint("请先连接设备".into());
        return;
    }
    let ssid = ui.get_wifi_ssid().to_string().trim().to_string();
    let password = ui.get_wifi_password().to_string();
    if ssid.is_empty() {
        ui.set_wifi_hint("请填写 WiFi 名称".into());
        return;
    }
    if ssid.as_bytes().len() > WIFI_SSID_MAX_BYTES {
        ui.set_wifi_hint("WiFi 名称过长（最多 32 字节）".into());
        return;
    }
    if password.as_bytes().len() > WIFI_PASSWORD_MAX_BYTES {
        ui.set_wifi_hint("密码过长（最多 64 字节）".into());
        return;
    }
    if !password.is_empty() && password.len() < 8 {
        ui.set_wifi_hint("密码至少 8 位；开放网络请留空".into());
        return;
    }

    let auth = wifi_auth_for_password(&password);
    let ssid_regs = match encode_write_value(&ssid, QueryValueType::String, REG_WIFI_STA_SSID_COUNT)
    {
        Ok(v) => v,
        Err(err) => {
            ui.set_wifi_hint(err.into());
            return;
        }
    };
    let password_regs = match encode_write_value(
        &password,
        QueryValueType::String,
        REG_WIFI_STA_PASSWORD_COUNT,
    ) {
        Ok(v) => v,
        Err(err) => {
            ui.set_wifi_hint(err.into());
            return;
        }
    };

    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_AUTH,
        vec![auth],
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_PASSWORD,
        password_regs,
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_ENABLE,
        vec![sta_enable_word(&password)],
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_SSID,
        ssid_regs,
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_ON_OFF,
        vec![1],
        None,
        Some(RegisterFieldPatch {
            start_bit: 0,
            width: 2,
            value: 1,
        }),
    );

    upsert_saved_wifi(ctx, &ssid, &password);
    ctx.modbus.remember_wifi_credentials(&password);
    {
        let wifi = &mut ctx.state.borrow_mut().device_config.wifi;
        wifi.phase = WifiProvisionPhase::ConnectingWifi;
        wifi.phase_since = Some(Instant::now());
        wifi.pending_ssid = ssid;
        wifi.hint.clear();
    }
    refresh_wifi_status(ui, ctx);
}

pub fn wire_wifi(ui: &MainWindow, ctx: &AppContext) {
    {
        let wifi = &ctx.state.borrow().device_config.wifi;
        ui.set_wifi_saved_networks(ModelRc::new(wifi.saved.clone()));
        ui.set_wifi_scan_aps(ModelRc::new(wifi.scan.clone()));
    }
    refresh_wifi_status(ui, ctx);

    let ui_weak = ui.as_weak();
    let ctx_wifi = ctx.clone();
    ui.on_wifi_apply(move || {
        let ui = ui_weak.unwrap();
        apply_wifi_credentials(&ui, &ctx_wifi);
    });

    let ui_weak = ui.as_weak();
    ui.on_wifi_scan(move || {
        let ui = ui_weak.unwrap();
        if ui.get_wifi_scan_busy() {
            return;
        }
        wifi_scan::prepare_platform_access();
        ui.set_wifi_picker_kind(1);
        ui.set_wifi_scan_busy(true);
        ui.set_wifi_scan_message("正在扫描附近 WiFi…".into());
        let ui_weak_done = ui.as_weak();
        std::thread::spawn(move || {
            let result = wifi_scan::scan_nearby();
            let _ = slint::invoke_from_event_loop(move || {
                let Some(ui) = ui_weak_done.upgrade() else {
                    return;
                };
                ui.set_wifi_scan_busy(false);
                match result {
                    Ok(aps) => {
                        let rows: Vec<WifiScanAp> = aps
                            .into_iter()
                            .map(|ap| WifiScanAp {
                                ssid: ap.ssid.into(),
                                signal: ap.signal.into(),
                                band: ap.band.into(),
                            })
                            .collect();
                        let empty = rows.is_empty();
                        let model = ui.get_wifi_scan_aps();
                        if let Some(vec) = model.as_any().downcast_ref::<VecModel<WifiScanAp>>() {
                            vec.set_vec(rows);
                        } else {
                            ui.set_wifi_scan_aps(ModelRc::new(VecModel::from(rows)));
                        }
                        ui.set_wifi_scan_message(if empty {
                            "未发现附近 2.4GHz WiFi，请确认本机 WLAN 已打开".into()
                        } else {
                            "".into()
                        });
                    }
                    Err(err) => {
                        ui.set_wifi_scan_message(err.into());
                    }
                }
            });
        });
    });

    let ui_weak = ui.as_weak();
    let ctx_pick_scan = ctx.clone();
    ui.on_wifi_select_scan_ap(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let Some(ap) = ui.get_wifi_scan_aps().row_data(idx) else {
            return;
        };
        let ssid = ap.ssid.clone();
        let saved = ctx_pick_scan.state.borrow().device_config.wifi.saved.clone();
        let saved_pwd = (0..saved.row_count())
            .filter_map(|i| saved.row_data(i))
            .find(|n| n.ssid == ssid)
            .map(|n| n.password);
        ui.set_wifi_ssid(ssid);
        if let Some(pwd) = saved_pwd {
            ui.set_wifi_password(pwd);
        }
        ui.set_wifi_picker_kind(0);
    });

    let ui_weak = ui.as_weak();
    let ctx_pick_saved = ctx.clone();
    ui.on_wifi_select_saved(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let Some(net) = ctx_pick_saved
            .state
            .borrow()
            .device_config
            .wifi
            .saved
            .row_data(idx)
        else {
            return;
        };
        ui.set_wifi_ssid(net.ssid);
        ui.set_wifi_password(net.password);
        ui.set_wifi_picker_kind(0);
    });

    let ctx_rm_wifi = ctx.clone();
    ui.on_wifi_remove_saved(move |index| {
        let idx = index as usize;
        let saved = ctx_rm_wifi.state.borrow().device_config.wifi.saved.clone();
        if idx < saved.row_count() {
            saved.remove(idx);
            persist_wifi_networks(&ctx_rm_wifi);
        }
    });
}
