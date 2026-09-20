//! 设备配置页灵活分组持久化（TOML）。
//!
//! 路径：Unix `~/.config/ble_gui/device_config.toml`，
//! Windows `%APPDATA%\ble_gui\device_config.toml`。
//!
//! - **不写入**「常用」组内容（由代码 schema 提供）
//! - 只保存用户自定义分组的 schema 与 `active_group`
//! - 本模块不依赖 Slint；加载后由状态层插入 builtin「常用」

use std::fs;
use std::path::PathBuf;

use log::info;
use serde::{Deserialize, Serialize};

const CONFIG_VERSION: u32 = 1;

/// 自定义配置项的可持久化定义（不含读回值）。
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ConfigItemSchema {
    pub name: String,
    pub register: String,
    pub value_type: String,
    pub register_count: i32,
    pub widget_kind: i32,
}

/// 一个自定义配置分组。
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ConfigGroupSchema {
    pub title: String,
    pub slave_id: String,
    pub items: Vec<ConfigItemSchema>,
}

#[derive(Serialize, Deserialize)]
struct SavedConfig {
    version: u32,
    active_group: i32,
    /// 仅自定义分组（不含「常用」）
    groups: Vec<ConfigGroupSchema>,
}

pub struct LoadedDeviceConfig {
    pub active_group: i32,
    /// 仅自定义分组；调用方负责在前面插入「常用」。
    pub groups: Vec<ConfigGroupSchema>,
}

fn config_path() -> Option<PathBuf> {
    #[cfg(windows)]
    {
        std::env::var_os("APPDATA").map(|p| {
            PathBuf::from(p)
                .join("ble_gui")
                .join("device_config.toml")
        })
    }
    #[cfg(not(windows))]
    {
        std::env::var_os("HOME").map(|h| {
            PathBuf::from(h)
                .join(".config")
                .join("ble_gui")
                .join("device_config.toml")
        })
    }
}

pub fn load() -> Option<LoadedDeviceConfig> {
    let path = config_path()?;
    let text = fs::read_to_string(&path).ok()?;
    let cfg: SavedConfig = toml::from_str(&text).ok()?;
    if cfg.version != CONFIG_VERSION {
        return None;
    }

    info!(
        target: "ble_gui::config_store",
        "已加载设备配置: {} 个自定义分组, 激活={active_group}, 路径={}",
        cfg.groups.len(),
        path.display(),
        active_group = cfg.active_group,
    );
    Some(LoadedDeviceConfig {
        active_group: cfg.active_group,
        groups: cfg.groups,
    })
}

pub fn save(
    custom_groups: &[ConfigGroupSchema],
    active_group: i32,
    group_count: usize,
) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;

    // 仅常用组时也写入，以便记住 active_group=0
    let max_group = group_count.saturating_sub(1) as i32;
    let cfg = SavedConfig {
        version: CONFIG_VERSION,
        active_group: active_group.clamp(0, max_group.max(0)),
        groups: custom_groups.to_vec(),
    };

    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let text = toml::to_string_pretty(&cfg).map_err(|e| {
        std::io::Error::new(std::io::ErrorKind::InvalidData, e)
    })?;
    fs::write(&path, text)?;
    Ok(())
}
