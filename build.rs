//! 编译期把 `ui/app.slint` 编进 crate（生成 `MainWindow` 等 Rust 绑定）。
//!
//! 修改任意 `ui/**/*.slint` 后需重新 `cargo build`；绑定类型在 [`crate::ui`]。
//! Windows 还会把 `assets/app.ico` 嵌进 exe，资源管理器 / 任务栏才能显示图标。
//! 同时从 ICO 解出 RGBA 像素写到 `OUT_DIR`，供运行时标题栏图标使用
//! （避免直接 `include_bytes` 独立 `.png`，本机透明加密会破坏其签名）。

use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=assets/app.ico");
    println!("cargo:rerun-if-changed=assets/macos_info.plist");
    slint_build::compile("ui/app.slint").unwrap();
    write_window_icon_rgba();

    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        let mut res = winres::WindowsResource::new();
        res.set_icon("assets/app.ico");
        res.set("ProductName", "BLE Modbus 工具");
        res.set("FileDescription", "BLE Modbus 工具");
        res.set("OriginalFilename", "ble_gui.exe");
        res.compile().expect("embed Windows application icon");
    }

    // 把定位用途说明嵌进可执行文件，便于 cargo run 时也能弹出定位授权。
    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("macos") {
        let plist = PathBuf::from("assets/macos_info.plist")
            .canonicalize()
            .expect("assets/macos_info.plist");
        println!(
            "cargo:rustc-link-arg=-Wl,-sectcreate,__TEXT,__info_plist,{}",
            plist.display()
        );
    }
}

fn write_window_icon_rgba() {
    let ico = fs::read("assets/app.ico").expect("read assets/app.ico");
    let png = png_payload_from_ico(&ico).expect("app.ico 中没有 PNG 帧");
    let decoded = image::load_from_memory_with_format(&png, image::ImageFormat::Png)
        .expect("decode PNG inside app.ico")
        .into_rgba8();
    let w = decoded.width();
    let h = decoded.height();
    let mut out = Vec::with_capacity(8 + decoded.len());
    out.extend_from_slice(&w.to_le_bytes());
    out.extend_from_slice(&h.to_le_bytes());
    out.extend_from_slice(decoded.as_raw());

    let path = PathBuf::from(env::var("OUT_DIR").unwrap()).join("window_icon.rgba");
    fs::write(&path, out).expect("write window_icon.rgba");
}

fn png_payload_from_ico(ico: &[u8]) -> Option<Vec<u8>> {
    if ico.len() < 6 {
        return None;
    }
    let count = u16::from_le_bytes(ico[4..6].try_into().ok()?) as usize;
    let mut best: Option<Vec<u8>> = None;
    let mut best_area = 0u32;
    for i in 0..count {
        let e = 6 + i * 16;
        if e + 16 > ico.len() {
            break;
        }
        let width = if ico[e] == 0 { 256 } else { u32::from(ico[e]) };
        let height = if ico[e + 1] == 0 { 256 } else { u32::from(ico[e + 1]) };
        let nbytes = u32::from_le_bytes(ico[e + 8..e + 12].try_into().ok()?) as usize;
        let offset = u32::from_le_bytes(ico[e + 12..e + 16].try_into().ok()?) as usize;
        let end = offset.checked_add(nbytes)?;
        if end > ico.len() {
            continue;
        }
        let blob = &ico[offset..end];
        if !blob.starts_with(b"\x89PNG\r\n\x1a\n") {
            continue;
        }
        let area = width.saturating_mul(height);
        if area >= best_area {
            best_area = area;
            best = Some(blob.to_vec());
        }
    }
    best
}
