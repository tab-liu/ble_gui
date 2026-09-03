//! 固件升级页：选文件、识别头，开始后由 BLE worker 做 XMODEM 与分发进度。

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
    let ctx_ver = ctx.clone();
    ui.on_firmware_ota_version_edited(move |text| {
        let ui = ui_weak.unwrap();
        ctx_ver.firmware.set_ota_version_text(text.to_string());
        refresh(&ui, &ctx_ver);
    });

    let ui_weak = ui.as_weak();
    let ctx_start = ctx.clone();
    ui.on_firmware_start(move || {
        let ui = ui_weak.unwrap();
        let snap = ctx_start.ble.snapshot();
        if let Some(job) = ctx_start
            .firmware
            .begin_upgrade(snap.connected, snap.encryption_ready)
        {
            ctx_start.ble.start_ota(job);
        }
        refresh(&ui, &ctx_start);
    });

    let ui_weak = ui.as_weak();
    let ctx_stop = ctx.clone();
    ui.on_firmware_stop(move || {
        let ui = ui_weak.unwrap();
        ctx_stop.firmware.request_stop();
        refresh(&ui, &ctx_stop);
    });
}
