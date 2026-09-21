//! 主题服务：模式切换、Palette 同步、配置持久化。

use std::cell::RefCell;
use std::fs;
use std::path::PathBuf;
use std::rc::Rc;

use slint::ComponentHandle;

use crate::services::config_dir;
use crate::ui::{MainWindow, Theme};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ThemeMode {
    System = 0,
    Light = 1,
    Dark = 2,
}

impl ThemeMode {
    pub fn from_i32(v: i32) -> Self {
        match v {
            1 => Self::Light,
            2 => Self::Dark,
            _ => Self::System,
        }
    }

    fn as_i32(self) -> i32 {
        self as i32
    }
}

#[derive(Clone)]
pub struct ThemeService {
    inner: Rc<RefCell<ThemeMode>>,
}

impl ThemeService {
    pub fn new() -> Self {
        let mode = load_mode().unwrap_or(ThemeMode::System);
        Self {
            inner: Rc::new(RefCell::new(mode)),
        }
    }

    pub fn mode(&self) -> ThemeMode {
        *self.inner.borrow()
    }

    pub fn set_mode(&self, mode: ThemeMode) {
        *self.inner.borrow_mut() = mode;
        if let Err(err) = save_mode(mode) {
            log::warn!(target: "ble_gui::theme", "保存主题失败: {err}");
        }
    }

    pub fn toggle_quick(&self) {
        let next = if self.effective_dark() {
            ThemeMode::Light
        } else {
            ThemeMode::Dark
        };
        self.set_mode(next);
    }

    pub fn apply(&self, ui: &MainWindow) {
        let mode = self.mode();
        let dark = self.effective_dark_for(mode);

        ui.global::<Theme>().set_mode(mode.as_i32());
        ui.global::<Theme>().set_dark(dark);
        ui.global::<Theme>().invoke_sync_palette();
    }

    fn effective_dark(&self) -> bool {
        self.effective_dark_for(self.mode())
    }

    fn effective_dark_for(&self, mode: ThemeMode) -> bool {
        match mode {
            ThemeMode::Light => false,
            ThemeMode::Dark => true,
            ThemeMode::System => detect_system_dark(),
        }
    }
}

fn detect_system_dark() -> bool {
    #[cfg(target_os = "macos")]
    {
        use std::process::Command;
        let output = Command::new("defaults")
            .args(["read", "-g", "AppleInterfaceStyle"])
            .output();
        if let Ok(out) = output {
            if out.status.success() {
                return String::from_utf8_lossy(&out.stdout).trim() == "Dark";
            }
        }
        false
    }
    #[cfg(windows)]
    {
        windows_system_dark().unwrap_or(false)
    }
    #[cfg(not(any(target_os = "macos", windows)))]
    {
        false
    }
}

#[cfg(windows)]
fn windows_system_dark() -> Option<bool> {
    use windows::Win32::System::Registry::{
        RegCloseKey, RegOpenKeyExW, RegQueryValueExW, HKEY_CURRENT_USER, KEY_READ, REG_VALUE_TYPE,
    };
    use windows::core::w;

    let mut key = Default::default();
    if unsafe {
        RegOpenKeyExW(
            HKEY_CURRENT_USER,
            w!("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
            None,
            KEY_READ,
            &mut key,
        )
    }
    .is_err()
    {
        return None;
    }
    let mut data: u32 = 1;
    let mut data_size = std::mem::size_of::<u32>() as u32;
    let mut ty = REG_VALUE_TYPE::default();
    let queried = unsafe {
        RegQueryValueExW(
            key,
            w!("AppsUseLightTheme"),
            None,
            Some(&mut ty),
            Some((&raw mut data).cast()),
            Some(&mut data_size),
        )
    };
    let _ = unsafe { RegCloseKey(key) };
    if queried.is_err() {
        return None;
    }
    Some(data == 0)
}

fn config_path() -> Option<PathBuf> {
    config_dir::config_file("theme")
}

fn load_mode() -> Option<ThemeMode> {
    let path = config_path()?;
    let text = fs::read_to_string(&path).ok()?;
    let mode = text.trim().parse::<i32>().ok().map(ThemeMode::from_i32)?;
    log::info!(
        target: "ble_gui::theme",
        "已加载主题 {:?}, 路径={}",
        mode,
        path.display(),
    );
    Some(mode)
}

fn save_mode(mode: ThemeMode) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, mode.as_i32().to_string())
}
