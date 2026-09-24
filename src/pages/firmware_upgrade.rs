//! 固件升级页：选文件、识别头；BLE XMODEM 或本机 HTTP + `00 09`。

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
    let ctx_force = ctx.clone();
    ui.on_firmware_force_toggled(move |on| {
        let ui = ui_weak.unwrap();
        ctx_force.firmware.set_force(on);
        refresh(&ui, &ctx_force);
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
    let ctx_http = ctx.clone();
    ui.on_firmware_start_http(move || {
        let ui = ui_weak.unwrap();
        let snap = ctx_http.ble.snapshot();
        let (wifi_sta, sta_ip, link_status_valid) = ctx_http
            .modbus
            .shared_live()
            .lock()
            .ok()
            .map(|l| (l.wifi_sta, l.sta_ip.clone(), l.link_status_valid))
            .unwrap_or((false, String::new(), false));
        if let Some(job) = ctx_http.firmware.begin_http_upgrade(
            snap.connected,
            snap.encryption_ready,
            wifi_sta,
            &sta_ip,
            link_status_valid,
        ) {
            ctx_http.ble.start_http_ota(job);
        }
        refresh(&ui, &ctx_http);
    });

    let ui_weak = ui.as_weak();
    let ctx_stop = ctx.clone();
    ui.on_firmware_stop(move || {
        let ui = ui_weak.unwrap();
        ctx_stop.firmware.request_stop();
        refresh(&ui, &ctx_stop);
    });
}
