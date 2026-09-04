//! 主页：蓝牙扫描/连接 + Modbus 数据概览。

use slint::ComponentHandle;

use crate::app::refresh;
use crate::services::ble::{REG_AC_OUTPUT, REG_DC_OUTPUT};
use crate::services::ble_favorites;
use crate::services::poll_sync::{set_app_page, sync_poll_policy};
use crate::state::{AppContext, PAGE_DASHBOARD};
use crate::ui::MainWindow;
use crate::ui::bindings::{refresh_ble, refresh_ble_scan_filter, refresh_modbus_dashboard};

fn start_connect(ui: &MainWindow, ctx: &AppContext, address: &str) {
    if address.is_empty() || ctx.ble.is_connecting() || ctx.ble.is_connected() {
        return;
    }
    ui.set_selected_scan_address(address.into());
    // Connect 命令会在 worker 内停止扫描。不要先发 StopScan，
    // 否则 Windows 上刚停扫描立刻做 GATT 会 Unreachable（Not connected）。
    ctx.ble.connect(address);
    refresh_ble(ui, &ctx.favorites, &ctx.ble.snapshot(), None);
}

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    let ui_weak = ui.as_weak();
    let ctx_nav = ctx.clone();
    ui.on_navigate(move |page| {
        let ui = ui_weak.unwrap();
        set_app_page(&ui, &ctx_nav, page);
    });

    let ui_weak = ui.as_weak();
    let ctx_ble = ctx.clone();
    ui.on_ble_action_clicked(move || {
        let ui = ui_weak.unwrap();
        if ctx_ble.ble.is_connected() {
            ctx_ble.ble.disconnect();
            sync_poll_policy(&ui, &ctx_ble);
            refresh(&ui, &ctx_ble);
        } else if ctx_ble.ble.is_connecting() {
            ctx_ble.ble.cancel_connect();
            refresh_ble(&ui, &ctx_ble.favorites, &ctx_ble.ble.snapshot(), None);
        } else if ctx_ble.ble.is_scanning() {
            ctx_ble.ble.stop_scan();
            refresh_ble(&ui, &ctx_ble.favorites, &ctx_ble.ble.snapshot(), None);
        } else {
            ctx_ble.ble.start_scan();
            ui.set_selected_scan_address("".into());
            if ctx_ble.ble.ui_page() != PAGE_DASHBOARD {
                set_app_page(&ui, &ctx_ble, PAGE_DASHBOARD);
            }
            refresh_ble(&ui, &ctx_ble.favorites, &ctx_ble.ble.snapshot(), None);
        }
    });

    let ui_weak = ui.as_weak();
    let ctx_select = ctx.clone();
    ui.on_select_scan_device(move |address| {
        let ui = ui_weak.unwrap();
        if ctx_select.ble.is_connecting() || ctx_select.ble.is_connected() {
            return;
        }
        ui.set_selected_scan_address(address);
    });

    let ui_weak = ui.as_weak();
    let ctx_connect = ctx.clone();
    ui.on_connect_selected_clicked(move || {
        let ui = ui_weak.unwrap();
        let address = ui.get_selected_scan_address().to_string();
        start_connect(&ui, &ctx_connect, &address);
    });

    let ui_weak = ui.as_weak();
    let ctx_toggle = ctx.clone();
    ui.on_toggle_favorite_clicked(move |address| {
        let ui = ui_weak.unwrap();
        if ctx_toggle.ble.is_connecting() {
            return;
        }
        let address = address.to_string();
        let snap = ctx_toggle.ble.snapshot();
        let name = snap
            .scan_devices
            .iter()
            .find(|d| ble_favorites::addresses_equal(&d.address, &address))
            .map(|d| d.name.clone())
            .or_else(|| ctx_toggle.favorite_name(&address))
            .unwrap_or_else(|| address.clone());
        ctx_toggle.toggle_favorite(&address, &name);
        // 收藏后从右侧移到左侧（或相反），强制刷新列表缓存。
        refresh_ble_scan_filter(
            &ui,
            &ctx_toggle.favorites,
            &ctx_toggle.ble.snapshot(),
            None,
        );
    });

    let ui_weak = ui.as_weak();
    let ctx_filter = ctx.clone();
    ui.on_ble_scan_filters_changed(move || {
        let ui = ui_weak.unwrap();
        refresh_ble_scan_filter(
            &ui,
            &ctx_filter.favorites,
            &ctx_filter.ble.snapshot(),
            if ctx_filter.ble.is_connected() {
                Some(ctx_filter.modbus.read_mode())
            } else {
                None
            },
        );
    });

    let ui_weak = ui.as_weak();
    let ctx_ac = ctx.clone();
    ui.on_ac_output_toggle(move || {
        let ui = ui_weak.unwrap();
        if !ctx_ac.ble.is_connected() {
            return;
        }
        let data = ctx_ac.modbus.dashboard_data();
        if !data.data_valid || ctx_ac.modbus.output_busy() {
            return;
        }
        let on = !data.ac_output_on;
        ctx_ac.modbus.set_output_busy(true);
        refresh_modbus_dashboard(&ui, &ctx_ac.modbus);
        ctx_ac.ble.write_register(REG_AC_OUTPUT, if on { 1 } else { 0 });
    });

    let ui_weak = ui.as_weak();
    let ctx_dc = ctx.clone();
    ui.on_dc_output_toggle(move || {
        let ui = ui_weak.unwrap();
        if !ctx_dc.ble.is_connected() {
            return;
        }
        let data = ctx_dc.modbus.dashboard_data();
        if !data.data_valid || ctx_dc.modbus.output_busy() {
            return;
        }
        let on = !data.dc_output_on;
        ctx_dc.modbus.set_output_busy(true);
        refresh_modbus_dashboard(&ui, &ctx_dc.modbus);
        ctx_dc.ble.write_register(REG_DC_OUTPUT, if on { 1 } else { 0 });
    });
}
