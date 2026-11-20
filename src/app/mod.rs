//! 应用组装层：创建窗口、注入回调、接上 worker 事件、跑事件循环。
//!
//! # 启动顺序（[`run`]）
//!
//! 1. `MainWindow::new()` — 编译期由 `build.rs` 生成的 Slint 根组件  
//! 2. [`crate::state::AppContext::new`] — 加载 TOML、构造 BLE/Modbus 服务  
//! 3. 注册 BLE UI 刷新钩子（worker 事件 → 只刷新有变化的区域）  
//! 4. [`crate::pages::wire_all`] — 各页 `on_*` 回调  
//! 5. 1 秒时钟：固件用时、配网超时（这两处没有新事件也会变）  
//! 6. `ui.run()` — 阻塞直到窗口关闭  
//!
//! # 与页面层的边界
//!
//! 本模块不写具体业务；业务在 `pages::*` 与 `services::*`。
//! 这里只负责「把服务接到 UI 生命周期」。

use std::cell::{Cell, RefCell};
use std::rc::Rc;
use std::sync::Arc;
use std::time::Duration;

use slint::{ComponentHandle, SharedString, Timer, TimerMode};

use crate::pages::{self, device_config, modbus_query, set_app_page, wifi_provision};
use crate::services::ble::ensure_dashboard_poll_if_idle;
use crate::services::poll_sync::sync_poll_policy;
use crate::state::{AppContext, AppPage};
use crate::ui::MainWindow;
use crate::ui::bindings::{self, refresh_all, refresh_ble, refresh_external_devices_from_live, refresh_firmware_from_session, refresh_modbus_dashboard_from_live};

thread_local! {
    /// worker 钩子要 `Send`，而 [`AppContext`] 留在 UI 线程。事件回到事件循环后走这里。
    static WORKER_UI_WAKE: RefCell<Option<Rc<dyn Fn()>>> = RefCell::new(None);
}

/// 创建并运行主窗口（阻塞到退出）。
pub fn run() -> Result<(), slint::PlatformError> {
    let ui = MainWindow::new()?;
    ui.set_app_version(env!("CARGO_PKG_VERSION").into());
    ui.set_window_icon(crate::ui::icon::load_window_icon());
    let ctx = AppContext::new();

    let was_connected = Rc::new(Cell::new(false));
    let query_gen_applied = Rc::new(Cell::new(0u64));
    let wake_ui = ui.as_weak();
    let wake_ctx = ctx.clone();
    let wake_connected = was_connected.clone();
    let wake_gen = query_gen_applied.clone();
    let wake: Rc<dyn Fn()> = Rc::new(move || {
        if let Some(ui) = wake_ui.upgrade() {
            on_worker_event(&ui, &wake_ctx, &wake_connected, &wake_gen);
        }
    });
    WORKER_UI_WAKE.with(|slot| *slot.borrow_mut() = Some(wake));
    ctx.ble.set_ui_refresh_hook(Arc::new(|| {
        let _ = slint::invoke_from_event_loop(|| {
            let wake = WORKER_UI_WAKE.with(|slot| slot.borrow().clone());
            if let Some(wake) = wake {
                wake();
            }
        });
    }));

    set_app_page(&ui, &ctx, AppPage::Dashboard);
    ui.set_dialog_kind(crate::ui::DialogKind::None);
    ui.set_active_modbus_tab(ctx.initial_modbus_tab);
    ctx.bind_modbus_tabs(&ui);
    ctx.theme.apply(&ui);
    refresh_all(&ui, &ctx);

    pages::wire_all(&ui, &ctx);

    // 侧栏主题快捷切换（设置页已移除，主题只保留这一处入口）。
    let ui_weak_theme = ui.as_weak();
    let ctx_theme = ctx.clone();
    ui.on_toggle_theme_quick(move || {
        let ui = ui_weak_theme.unwrap();
        ctx_theme.theme.toggle_quick();
        ctx_theme.theme.apply(&ui);
    });

    // 启动即扫描，省掉主页再点一次「扫描设备」。
    ctx.ble.start_scan();

    let ui_clock = ui.as_weak();
    let ctx_clock = ctx.clone();
    let clock_timer = Timer::default();
    // 固件已用秒数、配网 45s/60s 超时没有对应的 worker 事件，按秒推进即可。
    clock_timer.start(TimerMode::Repeated, Duration::from_secs(1), move || {
        if let Some(ui) = ui_clock.upgrade() {
            if ctx_clock.firmware.is_running() {
                refresh_firmware_from_session(&ui, &ctx_clock);
            }
            if wifi_provision::wifi_phase_needs_clock(&ctx_clock) {
                wifi_provision::tick_wifi_provision(&ui, &ctx_clock);
            }
        }
    });

    let ui_for_icon = ui.as_weak();
    let icon_attempts = Rc::new(Cell::new(0u32));
    let icon_timer = Rc::new(Timer::default());
    let icon_timer_cb = icon_timer.clone();
    icon_timer.start(TimerMode::Repeated, Duration::from_millis(50), move || {
        let attempt = icon_attempts.get();
        if attempt >= 40 {
            log::warn!("窗口图标：多次重试仍未拿到窗口句柄");
            icon_timer_cb.stop();
            return;
        }
        icon_attempts.set(attempt + 1);
        if let Some(ui) = ui_for_icon.upgrade() {
            if crate::ui::icon::apply_after_window_ready(&ui) {
                icon_timer_cb.stop();
            }
        }
    });

    let result = ui.run();
    WORKER_UI_WAKE.with(|slot| *slot.borrow_mut() = None);
    drop((clock_timer, icon_timer));
    result
}

/// worker 通知到达 UI 线程后：只刷新这次事件涉及的区域。
fn on_worker_event(
    ui: &MainWindow,
    ctx: &AppContext,
    was_connected: &Cell<bool>,
    query_gen_applied: &Cell<u64>,
) {
    let _ = ctx.ble.drain_events();
    let connected = ctx.ble.is_connected();
    let edge = connected != was_connected.get();
    if edge {
        if connected {
            ctx.modbus.on_connected();
        } else {
            ctx.modbus.on_disconnected();
        }
        sync_poll_policy(ui, ctx);
        was_connected.set(connected);
    }

    let read_mode = if connected {
        Some(ctx.modbus.read_mode())
    } else {
        None
    };
    refresh_ble(ui, &ctx.favorites, &ctx.ble.snapshot(), read_mode);

    if connected {
        ensure_dashboard_poll_if_idle(&ctx.ble.shared_poll_policy());
        match ui.get_current_page() {
            AppPage::Dashboard => {
                refresh_modbus_dashboard_from_live(ui, &ctx.modbus.shared_live());
            }
            AppPage::External => {
                refresh_external_devices_from_live(ui, &ctx.modbus.shared_live());
            }
            AppPage::DeviceConfig => device_config::refresh_builtin_availability(ui, ctx),
            _ => {}
        }
        apply_poll_results_if_new(ui, ctx, query_gen_applied);
    } else if edge {
        refresh_modbus_dashboard_from_live(ui, &ctx.modbus.shared_live());
        refresh_external_devices_from_live(ui, &ctx.modbus.shared_live());
        device_config::refresh_builtin_availability(ui, ctx);
        apply_poll_results_if_new(ui, ctx, query_gen_applied);
    }

    if edge || ctx.firmware.is_running() || ui.get_current_page() == AppPage::Firmware {
        refresh_firmware_from_session(ui, ctx);
    }
}

fn apply_poll_results_if_new(ui: &MainWindow, ctx: &AppContext, query_gen_applied: &Cell<u64>) {
    let poll_gen = ctx.modbus.query_poll_generation();
    if poll_gen != query_gen_applied.get() {
        modbus_query::apply_query_poll_results(ui, ctx);
        device_config::apply_config_poll_results(ui, ctx);
        query_gen_applied.set(poll_gen);
    }
}

/// 供各页面使用的 UI 刷新辅助。
pub fn refresh(ui: &MainWindow, ctx: &AppContext) {
    refresh_all(ui, ctx);
}

/// 打开通用对话框（种类为 [`crate::ui::DialogKind`]）。
pub fn open_dialog(ui: &MainWindow, kind: crate::ui::DialogKind) {
    ui.set_dialog_kind(kind);
    ui.set_dialog_name(SharedString::default());
}

/// 关闭对话框。
pub fn close_dialog(ui: &MainWindow) {
    bindings::close_dialog(ui);
}
