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

const CONFIG_VERSION: u32 = 1;

/// 查询卡片的可持久化定义（不含读回值）。
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct QueryItemSchema {
    pub name: String,
    pub register: String,
    pub value_type: String,
    pub register_count: i32,
    pub scale: i32,
}

/// 一个查询标签。
#[derive(Clone, Debug, Serialize, Deserialize)]
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
    #[cfg(windows)]
    {
        std::env::var_os("APPDATA").map(|p| {
            PathBuf::from(p)
                .join("ble_gui")
                .join("modbus_query.toml")
        })
    }
    #[cfg(not(windows))]
    {
        std::env::var_os("HOME").map(|h| {
            PathBuf::from(h)
                .join(".config")
                .join("ble_gui")
                .join("modbus_query.toml")
        })
    }
}

pub fn load() -> Option<LoadedModbusQuery> {
    let path = config_path()?;
    let text = fs::read_to_string(&path).ok()?;
    let cfg: SavedConfig = toml::from_str(&text).ok()?;
    if cfg.version != CONFIG_VERSION || cfg.tabs.is_empty() {
        return None;
    }
    let max_tab = cfg.tabs.len().saturating_sub(1) as i32;
    let active_tab = cfg.active_tab.clamp(0, max_tab);
    info!(
        target: "ble_gui::query_store",
        "已加载 Modbus 查询配置: {} 个标签, 激活标签={active_tab}, 路径={}",
        cfg.tabs.len(),
        path.display(),
    );
    Some(LoadedModbusQuery {
        active_tab,
        tabs: cfg.tabs,
    })
}

pub fn save(tabs: &[QueryTabSchema], active_tab: i32) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;

    if tabs.is_empty() {
        return Ok(());
    }

    let max_tab = tabs.len().saturating_sub(1) as i32;
    let cfg = SavedConfig {
        version: CONFIG_VERSION,
        active_tab: active_tab.clamp(0, max_tab),
        tabs: tabs.to_vec(),
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
