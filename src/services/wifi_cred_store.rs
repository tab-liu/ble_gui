//! 本机已用过的 WiFi SSID/密码（TOML）。
//!
//! 路径：Unix `~/.config/ble_gui/wifi_networks.toml`，
//! Windows `%APPDATA%\ble_gui\wifi_networks.toml`。明文保存，仅供本机配网复用。

use std::fs;
use std::path::PathBuf;

use log::info;
use serde::{Deserialize, Serialize};

const CONFIG_VERSION: u32 = 1;
pub const MAX_NETWORKS: usize = 30;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WifiNetwork {
    pub ssid: String,
    pub password: String,
}

#[derive(Serialize, Deserialize)]
struct SavedWifi {
    version: u32,
    networks: Vec<WifiNetwork>,
}

fn config_path() -> Option<PathBuf> {
    #[cfg(windows)]
    {
        std::env::var_os("APPDATA").map(|p| {
            PathBuf::from(p)
                .join("ble_gui")
                .join("wifi_networks.toml")
        })
    }
    #[cfg(not(windows))]
    {
        std::env::var_os("HOME").map(|h| {
            PathBuf::from(h)
                .join(".config")
                .join("ble_gui")
                .join("wifi_networks.toml")
        })
    }
}

pub fn load() -> Vec<WifiNetwork> {
    let Some(path) = config_path() else {
        return Vec::new();
    };
    let Ok(text) = fs::read_to_string(&path) else {
        return Vec::new();
    };
    let Ok(cfg) = toml::from_str::<SavedWifi>(&text) else {
        return Vec::new();
    };
    if cfg.version != CONFIG_VERSION {
        return Vec::new();
    }
    info!(
        target: "ble_gui::wifi_store",
        "已加载 {} 条 WiFi 记录, 路径={}",
        cfg.networks.len(),
        path.display(),
    );
    cfg.networks
}

pub fn save(networks: &[WifiNetwork]) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let cfg = SavedWifi {
        version: CONFIG_VERSION,
        networks: networks.to_vec(),
    };
    let text = toml::to_string_pretty(&cfg).map_err(|e| {
        std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string())
    })?;
    fs::write(&path, text)
}
