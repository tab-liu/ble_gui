//! 蓝牙收藏设备持久化（TOML）。
//!
//! 身份以 **MAC 地址** 为准（连接必须用地址）。广播名会随单机/并机切换变化，
//! 因此名称只作展示：扫到同一地址时用当前广播名覆盖并写回文件。

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

pub(crate) fn is_usable_advertised_name(name: &str) -> bool {
    let name = name.trim();
    !name.is_empty() && !(name.starts_with("蓝牙设备 (") && name.ends_with(')'))
}

/// 用扫描到的当前广播名更新收藏展示名。返回是否有写入文件的必要变更。
pub fn apply_advertised_names<'a, I>(devices: &mut Vec<FavoriteDevice>, advertised: I) -> bool
where
    I: IntoIterator<Item = (&'a str, &'a str)>,
{
    let mut changed = false;
    for (address, name) in advertised {
        if !is_usable_advertised_name(name) {
            continue;
        }
        let name = name.trim();
        let Some(existing) = devices
            .iter_mut()
            .find(|d| addresses_equal(&d.address, address))
        else {
            continue;
        };
        if existing.name != name {
            info!(
                target: "ble_gui::favorites",
                "收藏设备改名 {} → {} ({})",
                existing.name,
                name,
                existing.address,
            );
            existing.name = name.to_string();
            changed = true;
        }
    }
    changed
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn identity_is_mac_name_follows_advertisement() {
        let mut devices = vec![FavoriteDevice {
            address: "AA:BB:CC:DD:EE:FF".into(),
            name: "AP300-StandAlone".into(),
        }];
        assert!(!apply_advertised_names(
            &mut devices,
            [("AA-BB-CC-DD-EE-FF", "蓝牙设备 (AA:BB:CC:DD:EE:FF)")],
        ));
        assert_eq!(devices[0].name, "AP300-StandAlone");
        assert!(apply_advertised_names(
            &mut devices,
            [("aa:bb:cc:dd:ee:ff", "AP300-Parallel")],
        ));
        assert_eq!(devices[0].name, "AP300-Parallel");
        assert!(contains(&devices, "AA-BB-CC-DD-EE-FF"));
    }
}
