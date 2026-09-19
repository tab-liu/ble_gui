//! 编译期把 `ui/app.slint` 编进 crate（生成 `MainWindow` 等 Rust 绑定）。
//!
//! 修改任意 `ui/**/*.slint` 后需重新 `cargo build`；绑定类型在 [`crate::ui`]。
//! - Windows：`winres` 把 `assets/app.ico` 嵌进 exe（资源管理器 / 任务栏）
//! - macOS：从 ico 生成 `assets/app.icns`，由 `scripts/macos-run.sh` 打进 `.app`（程序坞）
//! - 各平台：写出 `OUT_DIR/window_icon.rgba`，供运行时标题栏 / 任务栏窗口图标

use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=assets/app.ico");
    println!("cargo:rerun-if-changed=assets/macos_info.plist");
    slint_build::compile("ui/app.slint").unwrap();
    let png = write_window_icon_rgba();
    write_app_icns(&png);
    write_app_png(&png);

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

/// 返回 ico 内最大 PNG 帧，并写 `window_icon.rgba`。
fn write_window_icon_rgba() -> Vec<u8> {
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
    png
}

/// 生成 macOS 程序坞用的 `assets/app.icns`。
fn write_app_icns(png: &[u8]) {
    use icns::{IconFamily, IconType, Image as IcnsImage};

    let base = image::load_from_memory_with_format(png, image::ImageFormat::Png)
        .expect("decode png for icns")
        .into_rgba8();

    let mut family = IconFamily::new();
    // 常用尺寸；缺的会由系统缩放。
    for &(ty, size) in &[
        (IconType::RGBA32_16x16, 16u32),
        (IconType::RGBA32_32x32, 32),
        (IconType::RGBA32_64x64, 64),
        (IconType::RGBA32_128x128, 128),
        (IconType::RGBA32_256x256, 256),
        (IconType::RGBA32_512x512, 512),
    ] {
        let resized = image::imageops::resize(
            &base,
            size,
            size,
            image::imageops::FilterType::Lanczos3,
        );
        let icns_img = IcnsImage::from_data(
            icns::PixelFormat::RGBA,
            size,
            size,
            resized.into_raw(),
        )
        .expect("icns image from rgba");
        if let Err(err) = family.add_icon_with_type(&icns_img, ty) {
            eprintln!("cargo:warning=icns skip {size}x{size}: {err}");
        }
    }

    let out = PathBuf::from("assets/app.icns");
    let mut file = fs::File::create(&out).expect("create assets/app.icns");
    family.write(&mut file).expect("write assets/app.icns");
}

/// Linux 等可用的 PNG（窗口图标已走 rgba；此文件便于桌面入口 / 分享）。
fn write_app_png(png: &[u8]) {
    fs::write("assets/app_icon.png", png).expect("write assets/app_icon.png");
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
