//! 设备固件升级页。

use slint::ComponentHandle;

use crate::app::refresh;
use crate::state::AppContext;
use crate::ui::MainWindow;

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    let ui_weak = ui.as_weak();
    let ctx_fw = ctx.clone();
    ui.on_firmware_check_update(move || {
        let ui = ui_weak.unwrap();
        ctx_fw.firmware.check_update();
        refresh(&ui, &ctx_fw);
    });
}
