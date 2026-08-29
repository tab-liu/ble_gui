//! 应用组装：创建 UI、注入各页面回调、运行事件循环。

use std::cell::Cell;
use std::rc::Rc;
use std::sync::Arc;
use std::time::Duration;

use slint::{ComponentHandle, SharedString, Timer, TimerMode};

use crate::pages;
use crate::pages::modbus_query;
use crate::services::ble::ensure_dashboard_poll_if_idle;
use crate::services::poll_sync::{set_app_page, sync_poll_policy};
use crate::state::{AppContext, DIALOG_NONE, PAGE_DASHBOARD};
use crate::ui::MainWindow;
use crate::ui::bindings::{self, refresh_all, refresh_ble, refresh_modbus_dashboard_from_live};

pub fn run() -> Result<(), slint::PlatformError> {
    let ui = MainWindow::new()?;
    let ctx = AppContext::new();

    let ui_weak = ui.as_weak();
    let ble_state = ctx.ble.shared_state();
    let modbus_live = ctx.modbus.shared_live();
    let poll_policy = ctx.ble.shared_poll_policy();
    ctx.ble.set_ui_refresh_hook(Arc::new(move || {
        let ui_weak = ui_weak.clone();
        let ble_state = ble_state.clone();
        let modbus_live = modbus_live.clone();
        let poll_policy = poll_policy.clone();
        let _ = slint::invoke_from_event_loop(move || {
            if let Some(ui) = ui_weak.upgrade() {
                let snap = ble_state.lock().expect("ble state lock").snapshot();
                refresh_ble(&ui, &snap);
                if snap.connected {
                    ensure_dashboard_poll_if_idle(&poll_policy);
                    if ui.get_current_page() == PAGE_DASHBOARD {
                        refresh_modbus_dashboard_from_live(&ui, &modbus_live);
                    }
                }
            }
        });
    }));

    set_app_page(&ui, &ctx, PAGE_DASHBOARD);
    ui.set_dialog_kind(DIALOG_NONE);
    ui.set_active_modbus_tab(0);
    ctx.bind_modbus_tabs(&ui);
    ctx.theme.apply(&ui);
    refresh_all(&ui, &ctx);

    pages::wire_all(&ui, &ctx);

    let ui_weak = ui.as_weak();
    let ctx_poll = ctx.clone();
    let was_connected = Rc::new(Cell::new(false));
    let query_gen_applied = Rc::new(Cell::new(0u64));
    let poll_timer = Timer::default();
    poll_timer.start(TimerMode::Repeated, Duration::from_millis(50), move || {
        let _ = ctx_poll.ble.drain_events();
        let connected = ctx_poll.ble.is_connected();
        if let Some(ui) = ui_weak.upgrade() {
            if connected {
                if !was_connected.get() {
                    sync_poll_policy(&ui, &ctx_poll);
                }
                was_connected.set(true);
                refresh_all(&ui, &ctx_poll);
            } else {
                if was_connected.get() {
                    sync_poll_policy(&ui, &ctx_poll);
                }
                was_connected.set(false);
            }

            let poll_gen = ctx_poll.modbus.query_poll_generation();
            if poll_gen != query_gen_applied.get() {
                modbus_query::apply_query_poll_results(&ui, &ctx_poll);
                query_gen_applied.set(poll_gen);
            }
        }
    });

    ui.run()
}

/// 供各页面使用的 UI 刷新辅助。
pub fn refresh(ui: &MainWindow, ctx: &AppContext) {
    refresh_all(ui, ctx);
}

pub fn open_dialog(ui: &MainWindow, kind: i32) {
    ui.set_dialog_kind(kind);
    ui.set_dialog_name(SharedString::default());
}

pub fn close_dialog(ui: &MainWindow) {
    bindings::close_dialog(ui);
}
