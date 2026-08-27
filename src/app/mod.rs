//! 应用组装：创建 UI、注入各页面回调、运行事件循环。

use slint::SharedString;

use slint::ComponentHandle;

use crate::pages;
use crate::state::{AppContext, DIALOG_NONE, PAGE_DASHBOARD};
use crate::ui::MainWindow;
use crate::ui::bindings::{self, refresh_all};

pub fn run() -> Result<(), slint::PlatformError> {
    let ui = MainWindow::new()?;
    let ctx = AppContext::new();

    ui.set_current_page(PAGE_DASHBOARD);
    ui.set_dialog_kind(DIALOG_NONE);
    ui.set_active_modbus_tab(0);
    ctx.bind_modbus_tabs(&ui);
    ctx.theme.apply(&ui);
    refresh_all(&ui, &ctx);

    pages::wire_all(&ui, &ctx);

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
