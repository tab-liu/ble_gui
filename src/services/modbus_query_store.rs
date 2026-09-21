//! Modbus 查询页标签与寄存器配置持久化（TOML）。
//!
//! 路径：Unix `~/.config/ble_gui/modbus_query.toml`，
//! Windows `%APPDATA%\ble_gui\modbus_query.toml`。
//!
//! 只存 schema（名称/寄存器/类型/长度/倍数）与激活标签；不存轮询读回值。
//! 本模块不依赖 Slint，页面层负责 schema ↔ UI 模型映射。

use std::fs;
use std::path::PathBuf;

use log::info;
use serde::{Deserialize, Serialize};

use super::config_dir;

const CONFIG_VERSION: u32 = 1;

/// 查询卡片的可持久化定义（不含读回值）。
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct QueryItemSchema {
    pub name: String,
    pub register: String,
    pub value_type: String,
    pub register_count: i32,
    pub scale: i32,
}

/// 一个查询标签。
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct QueryTabSchema {
    pub title: String,
    pub slave_id: String,
    pub items: Vec<QueryItemSchema>,
}

#[derive(Serialize, Deserialize)]
struct SavedConfig {
    version: u32,
    active_tab: i32,
    tabs: Vec<QueryTabSchema>,
}

pub struct LoadedModbusQuery {
    pub active_tab: i32,
    pub tabs: Vec<QueryTabSchema>,
}

fn config_path() -> Option<PathBuf> {
    config_dir::config_file("modbus_query.toml")
}

fn parse_config(text: &str) -> Option<LoadedModbusQuery> {
    let cfg: SavedConfig = toml::from_str(text).ok()?;
    if cfg.version != CONFIG_VERSION || cfg.tabs.is_empty() {
        return None;
    }
    let max_tab = cfg.tabs.len().saturating_sub(1) as i32;
    let active_tab = cfg.active_tab.clamp(0, max_tab);
    Some(LoadedModbusQuery {
        active_tab,
        tabs: cfg.tabs,
    })
}

fn format_config(tabs: &[QueryTabSchema], active_tab: i32) -> Option<String> {
    if tabs.is_empty() {
        return None;
    }
    let max_tab = tabs.len().saturating_sub(1) as i32;
    let cfg = SavedConfig {
        version: CONFIG_VERSION,
        active_tab: active_tab.clamp(0, max_tab),
        tabs: tabs.to_vec(),
    };
    toml::to_string_pretty(&cfg).ok()
}

pub fn load() -> Option<LoadedModbusQuery> {
    let path = config_path()?;
    let text = fs::read_to_string(&path).ok()?;
    let loaded = parse_config(&text)?;
    info!(
        target: "ble_gui::query_store",
        "已加载 Modbus 查询配置: {} 个标签, 激活标签={}, 路径={}",
        loaded.tabs.len(),
        loaded.active_tab,
        path.display(),
    );
    Some(loaded)
}

pub fn save(tabs: &[QueryTabSchema], active_tab: i32) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;

    let Some(text) = format_config(tabs, active_tab) else {
        return Ok(());
    };

    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(&path, text)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_tabs() -> Vec<QueryTabSchema> {
        vec![
            QueryTabSchema {
                title: "默认分组".into(),
                slave_id: "0".into(),
                items: vec![QueryItemSchema {
                    name: "SOC".into(),
                    register: "102".into(),
                    value_type: "integer".into(),
                    register_count: 1,
                    scale: 1,
                }],
            },
            QueryTabSchema {
                title: "第二组".into(),
                slave_id: "1".into(),
                items: vec![],
            },
        ]
    }

    #[test]
    fn roundtrip_keeps_schema_and_clamps_active_tab() {
        let tabs = sample_tabs();
        let text = format_config(&tabs, 99).expect("encode");
        let loaded = parse_config(&text).expect("decode");
        assert_eq!(loaded.active_tab, 1);
        assert_eq!(loaded.tabs, tabs);
        assert!(text.contains("SOC"));
        assert!(!text.contains("等待查询"));
    }

    #[test]
    fn empty_tabs_are_not_serialized() {
        assert!(format_config(&[], 0).is_none());
        assert!(parse_config("version = 1\nactive_tab = 0\ntabs = []\n").is_none());
    }

    #[test]
    fn wrong_version_is_rejected() {
        let text = r#"
version = 2
active_tab = 0
[[tabs]]
title = "x"
slave_id = "0"
items = []
"#;
        assert!(parse_config(text).is_none());
    }

    #[test]
    fn garbage_toml_is_rejected() {
        assert!(parse_config("not toml").is_none());
    }
}
