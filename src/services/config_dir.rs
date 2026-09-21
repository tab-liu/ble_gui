//! 本机配置目录（各 store 共用，避免 APPDATA/HOME 拼装各写一份）。
//!
//! - Windows：`%APPDATA%\ble_gui\`
//! - 其它：`~/.config/ble_gui/`

use std::path::PathBuf;

pub fn config_dir() -> Option<PathBuf> {
    #[cfg(windows)]
    {
        std::env::var_os("APPDATA").map(|p| PathBuf::from(p).join("ble_gui"))
    }
    #[cfg(not(windows))]
    {
        std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".config").join("ble_gui"))
    }
}

pub fn config_file(name: &str) -> Option<PathBuf> {
    config_dir().map(|dir| dir.join(name))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn config_file_appends_name() {
        let Some(dir) = config_dir() else {
            return;
        };
        assert_eq!(config_file("modbus_query.toml").as_deref(), Some(dir.join("modbus_query.toml").as_path()));
    }
}
