//! Modbus 查询页标签与寄存器配置持久化（TOML）。
//!
//! 路径：Unix `~/.config/ble_gui/modbus_query.toml`，
//! Windows `%APPDATA%\ble_gui\modbus_query.toml`。
//!
//! 只存 schema（名称/寄存器/类型/长度/倍数）与激活标签；不存轮询读回值。

use std::fs;
use std::path::PathBuf;
use std::rc::Rc;

use log::info;
use serde::{Deserialize, Serialize};
use slint::{Model, ModelRc, SharedString, VecModel};

use crate::ui::{ModbusQueryItem, ModbusTab};

const CONFIG_VERSION: u32 = 1;

#[derive(Serialize, Deserialize)]
struct SavedQueryItem {
    name: String,
    register: String,
    value_type: String,
    register_count: i32,
    scale: i32,
}

#[derive(Serialize, Deserialize)]
struct SavedTab {
    title: String,
    slave_id: String,
    items: Vec<SavedQueryItem>,
}

#[derive(Serialize, Deserialize)]
struct SavedConfig {
    version: u32,
    active_tab: i32,
    tabs: Vec<SavedTab>,
}

pub struct LoadedModbusQuery {
    pub active_tab: i32,
    pub tabs: Rc<VecModel<ModbusTab>>,
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

fn result_font_size(char_count: usize) -> i32 {
    match char_count {
        0..=6 => 20,
        7..=12 => 16,
        13..=20 => 13,
        _ => 11,
    }
}

fn saved_item_to_query_item(item: SavedQueryItem) -> ModbusQueryItem {
    let result: SharedString = "（Modbus 轮询后将自动填充）".into();
    let display = result.to_string();
    ModbusQueryItem {
        name: item.name.into(),
        register: item.register.into(),
        value_type: item.value_type.into(),
        register_count: item.register_count,
        scale: item.scale,
        status: "等待查询".into(),
        result: result.clone(),
        result_display: display.clone().into(),
        result_font_size: result_font_size(display.chars().count()),
    }
}

fn saved_tab_to_modbus_tab(tab: SavedTab) -> ModbusTab {
    let items: Vec<ModbusQueryItem> = tab
        .items
        .into_iter()
        .map(saved_item_to_query_item)
        .collect();
    ModbusTab {
        title: tab.title.into(),
        slave_id: tab.slave_id.into(),
        items: ModelRc::new(VecModel::from(items)),
    }
}

pub fn load() -> Option<LoadedModbusQuery> {
    let path = config_path()?;
    let text = fs::read_to_string(&path).ok()?;
    let cfg: SavedConfig = toml::from_str(&text).ok()?;
    if cfg.version != CONFIG_VERSION || cfg.tabs.is_empty() {
        return None;
    }
    let tabs: Vec<ModbusTab> = cfg.tabs.into_iter().map(saved_tab_to_modbus_tab).collect();
    let max_tab = tabs.len().saturating_sub(1) as i32;
    let active_tab = cfg.active_tab.clamp(0, max_tab);
    info!(
        target: "ble_gui::query_store",
        "已加载 Modbus 查询配置: {} 个标签, 激活标签={active_tab}, 路径={}",
        tabs.len(),
        path.display(),
    );
    Some(LoadedModbusQuery {
        active_tab,
        tabs: Rc::new(VecModel::from(tabs)),
    })
}

pub fn save(tabs: &Rc<VecModel<ModbusTab>>, active_tab: i32) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;

    let mut saved_tabs = Vec::new();
    for i in 0..tabs.row_count() {
        let Some(tab) = tabs.row_data(i) else {
            continue;
        };
        let items: Vec<SavedQueryItem> = (0..tab.items.row_count())
            .filter_map(|j| tab.items.row_data(j))
            .map(|item| SavedQueryItem {
                name: item.name.to_string(),
                register: item.register.to_string(),
                value_type: item.value_type.to_string(),
                register_count: item.register_count,
                scale: item.scale,
            })
            .collect();
        saved_tabs.push(SavedTab {
            title: tab.title.to_string(),
            slave_id: tab.slave_id.to_string(),
            items,
        });
    }

    if saved_tabs.is_empty() {
        return Ok(());
    }

    let max_tab = saved_tabs.len().saturating_sub(1) as i32;
    let cfg = SavedConfig {
        version: CONFIG_VERSION,
        active_tab: active_tab.clamp(0, max_tab),
        tabs: saved_tabs,
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
