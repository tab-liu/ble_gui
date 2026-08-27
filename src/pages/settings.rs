//! 设置页：主题与其它应用偏好。

use slint::ComponentHandle;

use crate::services::theme::ThemeMode;
use crate::state::AppContext;
use crate::ui::MainWindow;

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    let ui_weak = ui.as_weak();
    let ctx_theme = ctx.clone();
    ui.on_set_theme_mode(move |mode| {
        let ui = ui_weak.unwrap();
        ctx_theme.theme.set_mode(ThemeMode::from_i32(mode));
        ctx_theme.theme.apply(&ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_quick = ctx.clone();
    ui.on_toggle_theme_quick(move || {
        let ui = ui_weak.unwrap();
        ctx_quick.theme.toggle_quick();
        ctx_quick.theme.apply(&ui);
    });
}
