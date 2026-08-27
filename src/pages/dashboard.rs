//! 主页：蓝牙扫描/连接 + Modbus 数据概览。

use slint::ComponentHandle;

use crate::app::refresh;
use crate::state::{AppContext, PAGE_DASHBOARD, PAGE_FIRMWARE, PAGE_MODBUS};
use crate::ui::MainWindow;
use crate::ui::bindings::refresh_ble;
use crate::ui::bindings::refresh_ble_scan_filter;

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
        } else {
            ctx_ble.ble.start_scan();
        }
        refresh_ble(&ui, &ctx_ble.ble.snapshot());
        refresh(&ui, &ctx_ble);
    });

    let ui_weak = ui.as_weak();
    let ctx_connect = ctx.clone();
    ui.on_connect_device(move |address| {
        let ui = ui_weak.unwrap();
        if ctx_connect.ble.connect(&address.to_string()) {
            refresh(&ui, &ctx_connect);
        }
    });

    let ui_weak = ui.as_weak();
    let ctx_filter = ctx.clone();
    ui.on_ble_scan_filters_changed(move || {
        let ui = ui_weak.unwrap();
        refresh_ble_scan_filter(&ui, &ctx_filter.ble.snapshot());
    });
}
