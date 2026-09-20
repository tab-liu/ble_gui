//! 页面回调接线：每个子模块对应一个侧栏页，在 [`wire_all`] 里统一注册。
//!
//! 约定：
//! - `wire(ui, ctx)`：绑定 `ui.on_*`，初始化该页属性
//! - 轮询结果合并函数（如 `apply_*_poll_results`）由 `app` 定时器调用
//! - 改列表结构后调用对应 `*_store::save` 与 `poll_sync::sync_poll_policy`

pub mod dashboard;
pub mod device_config;
pub mod external;
pub mod firmware_upgrade;
pub mod modbus_query;
pub mod wifi_provision;

use crate::services::poll_sync::sync_poll_policy;
use crate::state::{AppContext, PAGE_DEVICE_CONFIG, PAGE_MODBUS};
use crate::ui::{clipboard, MainWindow};

/// 切换页面并同步至 Slint 与 worker 轮询策略。
pub fn set_app_page(ui: &MainWindow, ctx: &AppContext, page: i32) {
    ctx.ble.set_ui_page(page);
    ui.set_current_page(page);
    sync_poll_policy(ui, ctx);
    if page == PAGE_MODBUS {
        modbus_query::sync_active_query_items_to_ui(ui, ctx);
        modbus_query::sync_layout_from_window(ui, ctx);
    }
    if page == PAGE_DEVICE_CONFIG {
        device_config::sync_layout_from_window(ui, ctx);
    }
}

/// 为所有页面注册 Slint 回调（启动时调用一次）。
pub fn wire_all(ui: &MainWindow, ctx: &AppContext) {
    ui.on_copy_text(|text| clipboard::copy_text(text.as_str()));
    dashboard::wire(ui, ctx);
    modbus_query::wire(ui, ctx);
    device_config::wire(ui, ctx);
    external::wire(ui, ctx);
    firmware_upgrade::wire(ui, ctx);
}
