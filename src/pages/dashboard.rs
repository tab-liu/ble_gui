//! 主页：蓝牙扫描/连接 + Modbus 数据概览。

use slint::ComponentHandle;

use crate::app::refresh;
use crate::services::ble::{REG_AC_OUTPUT, REG_DC_OUTPUT};
use crate::state::{AppContext, PAGE_DASHBOARD, PAGE_FIRMWARE, PAGE_MODBUS};
use crate::ui::MainWindow;
use crate::ui::bindings::{refresh_ble, refresh_ble_scan_filter, refresh_modbus_dashboard};

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    let ui_weak = ui.as_weak();
    let ctx_nav = ctx.clone();
    ui.on_navigate(move |page| {
        let ui = ui_weak.unwrap();
        if (page == PAGE_MODBUS || page == PAGE_FIRMWARE) && !ctx_nav.ble.is_connected() {
            return;
        }
        ui.set_current_page(page);
    });

    let ui_weak = ui.as_weak();
    let ctx_ble = ctx.clone();
    ui.on_ble_action_clicked(move || {
        let ui = ui_weak.unwrap();
        if ctx_ble.ble.is_connected() {
            ctx_ble.ble.disconnect();
            if ui.get_current_page() == PAGE_MODBUS || ui.get_current_page() == PAGE_FIRMWARE {
                ui.set_current_page(PAGE_DASHBOARD);
            }
            refresh(&ui, &ctx_ble);
        } else if ctx_ble.ble.is_connecting() {
            // 连接过程中忽略顶部操作，避免重复发起扫描/连接。
        } else if ctx_ble.ble.is_scanning() {
            ctx_ble.ble.stop_scan();
            refresh_ble(&ui, &ctx_ble.ble.snapshot());
        } else {
            ctx_ble.ble.start_scan();
            ui.set_ble_scan_filter("".into());
            ui.set_ble_scan_rssi_filter_enabled(false);
            ui.set_selected_scan_address("".into());
            if ui.get_current_page() != PAGE_DASHBOARD {
                ui.set_current_page(PAGE_DASHBOARD);
            }
            refresh_ble(&ui, &ctx_ble.ble.snapshot());
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
        if ctx_connect.ble.is_connecting() || ctx_connect.ble.is_connected() {
            return;
        }
        let address = ui.get_selected_scan_address().to_string();
        if address.is_empty() {
            return;
        }
        ctx_connect.ble.connect(&address);
    });

    let ui_weak = ui.as_weak();
    let ctx_filter = ctx.clone();
    ui.on_ble_scan_filters_changed(move || {
        let ui = ui_weak.unwrap();
        refresh_ble_scan_filter(&ui, &ctx_filter.ble.snapshot());
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
