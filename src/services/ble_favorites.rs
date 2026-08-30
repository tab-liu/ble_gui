//! 蓝牙收藏设备持久化（TOML）。

use std::fs;
use std::path::PathBuf;

use log::info;
use serde::{Deserialize, Serialize};

const CONFIG_VERSION: u32 = 1;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FavoriteDevice {
    pub address: String,
    pub name: String,
}

#[derive(Serialize, Deserialize)]
struct SavedFavorites {
    version: u32,
    devices: Vec<FavoriteDevice>,
}

fn config_path() -> Option<PathBuf> {
    #[cfg(windows)]
    {
        std::env::var_os("APPDATA").map(|p| {
            PathBuf::from(p)
                .join("ble_gui")
                .join("favorites.toml")
        })
    }
    #[cfg(not(windows))]
    {
        std::env::var_os("HOME").map(|h| {
            PathBuf::from(h)
                .join(".config")
                .join("ble_gui")
                .join("favorites.toml")
        })
    }
}

pub fn normalize_address(addr: &str) -> String {
    addr.replace('-', ":").to_ascii_uppercase()
}

pub fn addresses_equal(a: &str, b: &str) -> bool {
    normalize_address(a) == normalize_address(b)
}

pub fn load() -> Vec<FavoriteDevice> {
    let Some(path) = config_path() else {
        return Vec::new();
    };
    let Ok(text) = fs::read_to_string(&path) else {
        return Vec::new();
    };
    let Ok(cfg) = toml::from_str::<SavedFavorites>(&text) else {
        return Vec::new();
    };
    if cfg.version != CONFIG_VERSION {
        return Vec::new();
    }
    info!(
        target: "ble_gui::favorites",
        "已加载 {} 个收藏设备, 路径={}",
        cfg.devices.len(),
        path.display(),
    );
    cfg.devices
}

pub fn save(devices: &[FavoriteDevice]) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let cfg = SavedFavorites {
        version: CONFIG_VERSION,
        devices: devices.to_vec(),
    };
    let text = toml::to_string_pretty(&cfg).map_err(|e| {
        std::io::Error::new(std::io::ErrorKind::InvalidData, e)
    })?;
    fs::write(path, text)
}

pub fn upsert(devices: &mut Vec<FavoriteDevice>, address: &str, name: &str) -> bool {
    let norm = normalize_address(address);
    if let Some(existing) = devices
        .iter_mut()
        .find(|d| normalize_address(&d.address) == norm)
    {
        if !name.trim().is_empty() && existing.name != name {
            existing.name = name.trim().to_string();
            return true;
        }
        return false;
    }
    devices.push(FavoriteDevice {
        address: address.to_string(),
        name: if name.trim().is_empty() {
            address.to_string()
        } else {
            name.trim().to_string()
        },
    });
    true
}

pub fn remove(devices: &mut Vec<FavoriteDevice>, address: &str) -> bool {
    let norm = normalize_address(address);
    let before = devices.len();
    devices.retain(|d| normalize_address(&d.address) != norm);
    devices.len() != before
}

pub fn contains(devices: &[FavoriteDevice], address: &str) -> bool {
    let norm = normalize_address(address);
    devices
        .iter()
        .any(|d| normalize_address(&d.address) == norm)
}
