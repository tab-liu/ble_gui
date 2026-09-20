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
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct ConfigItemSchema {
    pub name: String,
    pub register: String,
    pub value_type: String,
    pub register_count: i32,
    pub widget_kind: i32,
}

/// 一个自定义配置分组。
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
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

fn parse_config(text: &str) -> Option<LoadedDeviceConfig> {
    let cfg: SavedConfig = toml::from_str(text).ok()?;
    if cfg.version != CONFIG_VERSION {
        return None;
    }
    Some(LoadedDeviceConfig {
        active_group: cfg.active_group,
        groups: cfg.groups,
    })
}

fn format_config(
    custom_groups: &[ConfigGroupSchema],
    active_group: i32,
    group_count: usize,
) -> Result<String, toml::ser::Error> {
    let max_group = group_count.saturating_sub(1) as i32;
    let cfg = SavedConfig {
        version: CONFIG_VERSION,
        active_group: active_group.clamp(0, max_group.max(0)),
        groups: custom_groups.to_vec(),
    };
    toml::to_string_pretty(&cfg)
}

pub fn load() -> Option<LoadedDeviceConfig> {
    let path = config_path()?;
    let text = fs::read_to_string(&path).ok()?;
    let loaded = parse_config(&text)?;
    info!(
        target: "ble_gui::config_store",
        "已加载设备配置: {} 个自定义分组, 激活={}, 路径={}",
        loaded.groups.len(),
        loaded.active_group,
        path.display(),
    );
    Some(loaded)
}

pub fn save(
    custom_groups: &[ConfigGroupSchema],
    active_group: i32,
    group_count: usize,
) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;

    let text = format_config(custom_groups, active_group, group_count).map_err(|e| {
        std::io::Error::new(std::io::ErrorKind::InvalidData, e)
    })?;

    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(&path, text)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_group() -> ConfigGroupSchema {
        ConfigGroupSchema {
            title: "自定义".into(),
            slave_id: "0".into(),
            items: vec![ConfigItemSchema {
                name: "开关".into(),
                register: "2011".into(),
                value_type: "integer".into(),
                register_count: 1,
                widget_kind: 0,
            }],
        }
    }

    #[test]
    fn roundtrip_custom_groups_without_builtin() {
        let groups = vec![sample_group()];
        let text = format_config(&groups, 1, 2).expect("encode");
        let loaded = parse_config(&text).expect("decode");
        assert_eq!(loaded.active_group, 1);
        assert_eq!(loaded.groups, groups);
        assert!(!text.contains("常用"));
    }

    #[test]
    fn builtin_only_still_writes_active_group_zero() {
        let text = format_config(&[], 0, 1).expect("encode");
        let loaded = parse_config(&text).expect("decode");
        assert_eq!(loaded.active_group, 0);
        assert!(loaded.groups.is_empty());
    }

    #[test]
    fn active_group_clamps_to_group_count() {
        let text = format_config(&[sample_group()], 99, 2).expect("encode");
        let loaded = parse_config(&text).expect("decode");
        assert_eq!(loaded.active_group, 1);
    }

    #[test]
    fn wrong_version_is_rejected() {
        let text = r#"
version = 9
active_group = 0
groups = []
"#;
        assert!(parse_config(text).is_none());
    }

    #[test]
    fn garbage_toml_is_rejected() {
        assert!(parse_config("not toml").is_none());
    }
}
