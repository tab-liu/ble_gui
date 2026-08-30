//! UI → BLE worker：同步前台轮询策略。
//!
//! 在换页、换标签/分组、改查询列表后调用 [`sync_poll_policy`]，
//! 使 worker 只服务当前可见页（见 [`crate::services::ble::PollForeground`]）。

use slint::Model;

use crate::pages::modbus_query;
use crate::services::ble::modbus::{
    parse_register_address, parse_scale, parse_value_type, QueryValueType, BUILTIN_SETTINGS,
};
use crate::services::ble::{PollForeground, QueryPollItemSpec};
use crate::state::{
    AppContext, PAGE_DASHBOARD, PAGE_DEVICE_CONFIG, PAGE_FIRMWARE, PAGE_MODBUS, PAGE_SETTINGS,
};
use crate::ui::MainWindow;

/// 切换页面并同步至 Slint 与 worker 轮询策略。
pub fn set_app_page(ui: &MainWindow, ctx: &AppContext, page: i32) {
    ctx.ble.set_ui_page(page);
    ui.set_current_page(page);
    sync_poll_policy(ui, ctx);
    if page == PAGE_MODBUS {
        modbus_query::sync_active_query_items_to_ui(ui, ctx);
    }
}

/// 根据当前页面与激活列表，更新 worker 的 [`PollForeground`]。
pub fn sync_poll_policy(ui: &MainWindow, ctx: &AppContext) {
    let foreground = if !ctx.ble.is_connected() {
        PollForeground::None
    } else {
        match ctx.ble.ui_page() {
            PAGE_DASHBOARD => PollForeground::Dashboard,
            PAGE_MODBUS => build_modbus_query_foreground(ui, ctx),
            PAGE_DEVICE_CONFIG => build_device_config_foreground(ui, ctx),
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
            bit: None,
        });
    }

    PollForeground::ModbusQuery {
        tab_index,
        slave_id,
        items,
    }
}

fn build_device_config_foreground(ui: &MainWindow, ctx: &AppContext) -> PollForeground {
    let group_index = ui.get_active_config_group() as usize;
    let st = ctx.state.borrow();
    let Some(group) = st.device_config.groups.row_data(group_index) else {
        return PollForeground::None;
    };
    let slave_id = group
        .slave_id
        .to_string()
        .trim()
        .parse::<u8>()
        .unwrap_or(0);
    let builtin = group.builtin;

    if builtin {
        let items = BUILTIN_SETTINGS
            .iter()
            .enumerate()
            .map(|(i, def)| QueryPollItemSpec {
                item_index: i,
                register_text: def.register.to_string(),
                protocol_address: Some(def.register),
                register_count: 1,
                value_type: QueryValueType::Integer,
                scale: 1,
                bit: def.bit,
            })
            .collect();
        return PollForeground::DeviceConfig {
            group_index,
            builtin: true,
            slave_id,
            items,
        };
    }

    let items_model = group.items.clone();
    drop(st);

    let mut items = Vec::new();
    for i in 0..items_model.row_count() {
        let Some(row) = items_model.row_data(i) else {
            continue;
        };
        let register_text = row.register.to_string();
        items.push(QueryPollItemSpec {
            item_index: i,
            register_text: register_text.clone(),
            protocol_address: parse_register_address(&register_text).ok(),
            register_count: row.register_count.max(1) as u16,
            value_type: parse_value_type(&row.value_type.to_string()),
            scale: 1,
            bit: None,
        });
    }

    PollForeground::DeviceConfig {
        group_index,
        builtin: false,
        slave_id,
        items,
    }
}
