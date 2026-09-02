//! 固件升级页：选文件、识别头、开始/停止（传输逻辑后续接入 BLE worker）。

use slint::ComponentHandle;

use crate::app::refresh;
use crate::state::AppContext;
use crate::ui::MainWindow;

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    let ui_weak = ui.as_weak();
    let ctx_pick = ctx.clone();
    ui.on_firmware_pick_file(move || {
        let ui = ui_weak.unwrap();
        ctx_pick.firmware.pick_file();
        refresh(&ui, &ctx_pick);
    });

    let ui_weak = ui.as_weak();
    let ctx_clear = ctx.clone();
    ui.on_firmware_clear_file(move || {
        let ui = ui_weak.unwrap();
        ctx_clear.firmware.clear_file();
        refresh(&ui, &ctx_clear);
    });

    let ui_weak = ui.as_weak();
    let ctx_start = ctx.clone();
    ui.on_firmware_start(move || {
        let ui = ui_weak.unwrap();
        let connected = ctx_start.ble.snapshot().connected;
        ctx_start.firmware.start_upgrade(connected);
        refresh(&ui, &ctx_start);
    });

    let ui_weak = ui.as_weak();
    let ctx_stop = ctx.clone();
    ui.on_firmware_stop(move || {
        let ui = ui_weak.unwrap();
        ctx_stop.firmware.stop_upgrade();
        refresh(&ui, &ctx_stop);
    });
}
