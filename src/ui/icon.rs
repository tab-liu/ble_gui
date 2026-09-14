//! 窗口标题栏图标。
//!
//! 像素来自 `build.rs`：从 `app.ico` 解出 PNG，再写成 `OUT_DIR/window_icon.rgba`。
//! 这样不读独立 `.png`（本机透明加密会破坏签名），也让 Slint 的 `icon` 非空，
//! 避免 winit 用 `None` 把标题栏清成系统默认图。
//! Windows 上 HWND 就绪后再补一次 `WM_SETICON`，与任务栏同一份资源图标对齐。

use slint::{ComponentHandle, Image, Rgba8Pixel, SharedPixelBuffer};

use crate::ui::MainWindow;

const WINDOW_ICON_RGBA: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/window_icon.rgba"));

pub fn load_window_icon() -> Image {
    if WINDOW_ICON_RGBA.len() < 8 {
        log::warn!("窗口图标：rgba 数据过短");
        return Image::default();
    }
    let width = u32::from_le_bytes(WINDOW_ICON_RGBA[0..4].try_into().unwrap());
    let height = u32::from_le_bytes(WINDOW_ICON_RGBA[4..8].try_into().unwrap());
    let pixels = &WINDOW_ICON_RGBA[8..];
    let expected = (width as usize).saturating_mul(height as usize).saturating_mul(4);
    if pixels.len() != expected {
        log::warn!(
            "窗口图标：rgba 长度不符 expect={expected} got={}",
            pixels.len()
        );
        return Image::default();
    }
    Image::from_rgba8(SharedPixelBuffer::<Rgba8Pixel>::clone_from_slice(
        pixels, width, height,
    ))
}

/// 尝试把 exe 资源图标设到标题栏。`true` = 已处理完（成功或无需再试）。
pub fn apply_after_window_ready(ui: &MainWindow) -> bool {
    #[cfg(windows)]
    {
        return apply_win32_exe_icon(ui);
    }
    #[cfg(not(windows))]
    {
        let _ = ui;
        true
    }
}

#[cfg(windows)]
fn apply_win32_exe_icon(ui: &MainWindow) -> bool {
    use raw_window_handle::RawWindowHandle;
    use windows::Win32::Foundation::{HINSTANCE, HWND, LPARAM, WPARAM};
    use windows::Win32::System::LibraryLoader::GetModuleHandleW;
    use windows::Win32::UI::WindowsAndMessaging::{
        GetSystemMetrics, LoadImageW, SendMessageW, ICON_BIG, ICON_SMALL, IMAGE_ICON,
        LR_DEFAULTCOLOR, SM_CXICON, SM_CXSMICON, SM_CYICON, SM_CYSMICON, WM_SETICON,
    };
    use windows::core::PCWSTR;

    let slint_handle = ui.window().window_handle();
    let Ok(handle) = raw_window_handle::HasWindowHandle::window_handle(&slint_handle) else {
        return false;
    };
    let RawWindowHandle::Win32(win32) = handle.as_raw() else {
        return true;
    };
    let hwnd = HWND(win32.hwnd.get() as *mut core::ffi::c_void);
    // winres::set_icon 把 app.ico 编成资源 ID 1。
    let resource = PCWSTR(1usize as *const u16);

    unsafe {
        let Ok(module) = GetModuleHandleW(PCWSTR::null()) else {
            log::warn!("窗口图标：GetModuleHandleW 失败");
            return true;
        };
        let instance = HINSTANCE(module.0);
        let small_w = GetSystemMetrics(SM_CXSMICON);
        let small_h = GetSystemMetrics(SM_CYSMICON);
        let big_w = GetSystemMetrics(SM_CXICON);
        let big_h = GetSystemMetrics(SM_CYICON);

        match LoadImageW(
            Some(instance),
            resource,
            IMAGE_ICON,
            small_w,
            small_h,
            LR_DEFAULTCOLOR,
        ) {
            Ok(icon) => {
                SendMessageW(
                    hwnd,
                    WM_SETICON,
                    Some(WPARAM(ICON_SMALL as usize)),
                    Some(LPARAM(icon.0 as isize)),
                );
            }
            Err(err) => log::warn!("窗口图标：LoadImageW(SMALL) 失败: {err}"),
        }

        match LoadImageW(
            Some(instance),
            resource,
            IMAGE_ICON,
            big_w,
            big_h,
            LR_DEFAULTCOLOR,
        ) {
            Ok(icon) => {
                SendMessageW(
                    hwnd,
                    WM_SETICON,
                    Some(WPARAM(ICON_BIG as usize)),
                    Some(LPARAM(icon.0 as isize)),
                );
            }
            Err(err) => log::warn!("窗口图标：LoadImageW(BIG) 失败: {err}"),
        }
    }
    true
}
