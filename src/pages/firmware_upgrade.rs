//! 固件升级页回调（检查更新等）；OTA 传输逻辑将落在 [`crate::services::firmware`] / BLE worker。

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
