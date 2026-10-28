//! 扫描并连接加密设备，显示整机、IoT 和封好的安全码。

#![windows_subsystem = "windows"]

use std::rc::Rc;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use slint::{ModelRc, Timer, TimerMode, VecModel};

use ble_ident::{spawn_link, LinkEvent};

include!(concat!(env!("OUT_DIR"), "/ident_ui.rs"));

fn main() -> Result<(), slint::PlatformError> {
    #[cfg(windows)]
    attach_parent_console();

    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("warn")).init();

    let (link, events) = spawn_link();
    let link = Rc::new(link);
    let ui = IdentWindow::new()?;
    let events = Arc::new(Mutex::new(events));

    let ui_scan = ui.as_weak();
    let link_scan = link.clone();
    ui.on_scan(move || {
        let Some(ui) = ui_scan.upgrade() else { return };
        if ui.get_scanning() {
            link_scan.stop_scan();
        } else {
            link_scan.start_scan();
        }
    });

    let ui_connect = ui.as_weak();
    let link_connect = link.clone();
    ui.on_connect(move || {
        let Some(ui) = ui_connect.upgrade() else { return };
        if ui.get_connected() || ui.get_busy() {
            link_connect.disconnect();
            return;
        }
        let address = ui.get_selected().to_string();
        if address.is_empty() {
            return;
        }
        ui.set_busy(true);
        ui.set_machine("—".into());
        ui.set_iot("—".into());
        ui.set_safe_code("—".into());
        link_connect.connect(address);
    });

    let ui_copy = ui.as_weak();
    ui.on_copy(move || {
        let Some(ui) = ui_copy.upgrade() else { return };
        let text = ui.get_safe_code().to_string();
        if text.is_empty() || text == "—" {
            ui.set_status("Nothing to copy".into());
            return;
        }
        match arboard::Clipboard::new().and_then(|mut clipboard| clipboard.set_text(text)) {
            Ok(()) => ui.set_status("Copied".into()),
            Err(err) => {
                log::warn!("copy failed: {err}");
                ui.set_status("Copy failed".into());
            }
        }
    });

    let ui_timer = ui.as_weak();
    let events_timer = events.clone();
    let timer = Timer::default();
    timer.start(TimerMode::Repeated, Duration::from_millis(100), move || {
        let Some(ui) = ui_timer.upgrade() else { return };
        let Ok(rx) = events_timer.lock() else { return };
        while let Ok(event) = rx.try_recv() {
            apply_event(&ui, event);
        }
    });

    let _keep_link = link;
    ui.run()
}

fn signal_bars(rssi: i32) -> i32 {
    if rssi >= -55 {
        4
    } else if rssi >= -67 {
        3
    } else if rssi >= -80 {
        2
    } else {
        1
    }
}

fn apply_event(ui: &IdentWindow, event: LinkEvent) {
    match event {
        LinkEvent::Status(text) => ui.set_status(text.into()),
        LinkEvent::Scanning(on) => ui.set_scanning(on),
        LinkEvent::Devices(list) => {
            let selected = ui.get_selected().to_string();
            let still = list.iter().any(|d| d.address == selected);
            ui.set_device_count(list.len().to_string().into());
            let items: Vec<DeviceItem> = list
                .into_iter()
                .map(|d| {
                    let name = if d.name.is_empty() { d.address.clone() } else { d.name };
                    DeviceItem {
                        name: name.into(),
                        address: d.address.into(),
                        rssi: format!("{} dBm", d.rssi).into(),
                        bars: signal_bars(d.rssi),
                    }
                })
                .collect();
            ui.set_devices(ModelRc::new(VecModel::from(items)));
            if !still {
                ui.set_selected("".into());
            }
        }
        LinkEvent::Identity { machine, iot, safe_sealed } => {
            ui.set_machine(machine.into());
            ui.set_iot(iot.into());
            ui.set_safe_code(safe_sealed.into());
            ui.set_connected(true);
            ui.set_busy(false);
        }
        LinkEvent::Disconnected => {
            ui.set_connected(false);
            ui.set_busy(false);
        }
    }
}

#[cfg(windows)]
fn attach_parent_console() {
    const ATTACH_PARENT_PROCESS: u32 = 0xFFFF_FFFF;
    const STD_OUTPUT_HANDLE: u32 = 0xFFFF_FFF5;
    const STD_ERROR_HANDLE: u32 = 0xFFFF_FFF4;
    const GENERIC_READ: u32 = 0x8000_0000;
    const GENERIC_WRITE: u32 = 0x4000_0000;
    const FILE_SHARE_READ: u32 = 1;
    const FILE_SHARE_WRITE: u32 = 2;
    const OPEN_EXISTING: u32 = 3;
    const FILE_ATTRIBUTE_NORMAL: u32 = 0x80;
    const INVALID_HANDLE: isize = -1;

    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn AttachConsole(dw_process_id: u32) -> i32;
        fn SetStdHandle(n_std_handle: u32, h_handle: isize) -> i32;
        fn CreateFileA(
            file_name: *const u8,
            desired_access: u32,
            share_mode: u32,
            security: isize,
            creation_disposition: u32,
            flags: u32,
            template: isize,
        ) -> isize;
    }

    unsafe {
        if AttachConsole(ATTACH_PARENT_PROCESS) == 0 {
            return;
        }
        let name = b"CONOUT$\0";
        let handle = CreateFileA(
            name.as_ptr(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0,
        );
        if handle == 0 || handle == INVALID_HANDLE {
            return;
        }
        let _ = SetStdHandle(STD_OUTPUT_HANDLE, handle);
        let _ = SetStdHandle(STD_ERROR_HANDLE, handle);
    }
}
