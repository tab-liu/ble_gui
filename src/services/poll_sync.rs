//! UI 层 → BLE worker：同步前台轮询策略。

use slint::Model;

use crate::pages::modbus_query;
use crate::services::ble::modbus::{parse_register_address, parse_scale, parse_value_type};
use crate::services::ble::{PollForeground, QueryPollItemSpec};
use crate::state::{AppContext, PAGE_DASHBOARD, PAGE_FIRMWARE, PAGE_MODBUS, PAGE_SETTINGS};
use crate::ui::MainWindow;

/// 切换页面并同步 Slint 与轮询策略。
pub fn set_app_page(ui: &MainWindow, ctx: &AppContext, page: i32) {
    ctx.ble.set_ui_page(page);
    ui.set_current_page(page);
    sync_poll_policy(ui, ctx);
    if page == PAGE_MODBUS {
        modbus_query::sync_active_query_items_to_ui(ui, ctx);
    }
}

/// 根据当前页面、Modbus 标签与查询列表，更新 worker 轮询目标。
pub fn sync_poll_policy(ui: &MainWindow, ctx: &AppContext) {
    let foreground = if !ctx.ble.is_connected() {
        PollForeground::None
    } else {
        match ctx.ble.ui_page() {
            PAGE_DASHBOARD => PollForeground::Dashboard,
            PAGE_MODBUS => build_modbus_query_foreground(ui, ctx),
            PAGE_SETTINGS | PAGE_FIRMWARE => PollForeground::None,
            _ => PollForeground::None,
        }
    };
    ctx.ble.set_poll_foreground(foreground);
}

fn build_modbus_query_foreground(ui: &MainWindow, ctx: &AppContext) -> PollForeground {
    let tab_index = ui.get_active_modbus_tab() as usize;
    let st = ctx.state.borrow();
    let Some(tab) = st.modbus_query.tabs.row_data(tab_index) else {
        return PollForeground::None;
    };
    let slave_id = tab
        .slave_id
        .to_string()
        .trim()
        .parse::<u8>()
        .unwrap_or(0);
    let items_model = tab.items.clone();
    drop(st);

    let mut items = Vec::new();
    for i in 0..items_model.row_count() {
        let Some(row) = items_model.row_data(i) else {
            continue;
        };
        let register_text = row.register.to_string();
        let register_count = row.register_count.max(1) as u16;
        let scale = if row.scale > 0 {
            row.scale as u32
        } else {
            parse_scale("")
        };
        items.push(QueryPollItemSpec {
            item_index: i,
            register_text: register_text.clone(),
            protocol_address: parse_register_address(&register_text).ok(),
            register_count,
            value_type: parse_value_type(&row.value_type.to_string()),
            scale,
        });
    }

    PollForeground::ModbusQuery {
        tab_index,
        slave_id,
        items,
    }
}
