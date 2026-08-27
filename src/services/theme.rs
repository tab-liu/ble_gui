//! 主题服务：模式切换、Palette 同步、配置持久化。

use std::cell::RefCell;
use std::fs;
use std::path::PathBuf;
use std::rc::Rc;

use slint::ComponentHandle;

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
        let _ = save_mode(mode);
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
    #[cfg(not(target_os = "macos"))]
    {
        false
    }
}

fn config_path() -> Option<PathBuf> {
    std::env::var_os("HOME").map(|home| {
        PathBuf::from(home)
            .join(".config")
            .join("ble_gui")
            .join("theme")
    })
}

fn load_mode() -> Option<ThemeMode> {
    let path = config_path()?;
    let text = fs::read_to_string(path).ok()?;
    text.trim().parse::<i32>().ok().map(ThemeMode::from_i32)
}

fn save_mode(mode: ThemeMode) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "no config path")
    })?;
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, mode.as_i32().to_string())
}
