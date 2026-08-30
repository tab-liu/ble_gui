//! 设备配置页灵活分组持久化（TOML）。
//!
//! 路径：Unix `~/.config/ble_gui/device_config.toml`，
//! Windows `%APPDATA%\ble_gui\device_config.toml`。
//!
//! - **不写入**「常用」组内容（由代码 [`BUILTIN_SETTINGS`](crate::services::ble::modbus::BUILTIN_SETTINGS) 提供）  
//! - 只保存用户自定义分组的 schema 与 `active_group`  
//! - 加载时总是先插入 builtin「常用」，再追加文件中的自定义组

use std::fs;
use std::path::PathBuf;
use std::rc::Rc;

use log::info;
use serde::{Deserialize, Serialize};
use slint::{Model, ModelRc, SharedString, VecModel};

use crate::services::ble::modbus::BUILTIN_CONFIG_SLAVE_ID;
use crate::ui::{DeviceConfigGroup, DeviceConfigItem};

const CONFIG_VERSION: u32 = 1;

#[derive(Serialize, Deserialize)]
struct SavedConfigItem {
    name: String,
    register: String,
    value_type: String,
    register_count: i32,
    widget_kind: i32,
}

#[derive(Serialize, Deserialize)]
struct SavedGroup {
    title: String,
    slave_id: String,
    items: Vec<SavedConfigItem>,
}

#[derive(Serialize, Deserialize)]
struct SavedConfig {
    version: u32,
    active_group: i32,
    /// 仅自定义分组（不含「常用」）
    groups: Vec<SavedGroup>,
}

pub struct LoadedDeviceConfig {
    pub active_group: i32,
    pub groups: Rc<VecModel<DeviceConfigGroup>>,
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

fn empty_read() -> (SharedString, SharedString, i32) {
    ("—".into(), "—".into(), 14)
}

fn saved_item_to_config_item(item: SavedConfigItem) -> DeviceConfigItem {
    let (result, result_display, result_font_size) = empty_read();
    let widget_kind = item.widget_kind.clamp(0, 2);
    DeviceConfigItem {
        name: item.name.into(),
        register: item.register.into(),
        value_type: item.value_type.into(),
        register_count: item.register_count.max(1),
        widget_kind,
        result,
        result_display,
        result_font_size,
        write_value: if widget_kind == 0 {
            "0".into()
        } else {
            "".into()
        },
        status: "等待读取".into(),
        dirty: false,
    }
}

fn builtin_group() -> DeviceConfigGroup {
    DeviceConfigGroup {
        title: "常用".into(),
        slave_id: BUILTIN_CONFIG_SLAVE_ID.to_string().into(),
        builtin: true,
        items: ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())),
    }
}

fn saved_group_to_config_group(group: SavedGroup) -> DeviceConfigGroup {
    let items: Vec<DeviceConfigItem> = group
        .items
        .into_iter()
        .map(saved_item_to_config_item)
        .collect();
    DeviceConfigGroup {
        title: group.title.into(),
        slave_id: group.slave_id.into(),
        builtin: false,
        items: ModelRc::new(VecModel::from(items)),
    }
}

pub fn load() -> Option<LoadedDeviceConfig> {
    let path = config_path()?;
    let text = fs::read_to_string(&path).ok()?;
    let cfg: SavedConfig = toml::from_str(&text).ok()?;
    if cfg.version != CONFIG_VERSION {
        return None;
    }

    let mut groups = vec![builtin_group()];
    groups.extend(cfg.groups.into_iter().map(saved_group_to_config_group));

    let max_group = groups.len().saturating_sub(1) as i32;
    let active_group = cfg.active_group.clamp(0, max_group);
    info!(
        target: "ble_gui::config_store",
        "已加载设备配置: {} 个分组（含常用）, 激活={active_group}, 路径={}",
        groups.len(),
        path.display(),
    );
    Some(LoadedDeviceConfig {
        active_group,
        groups: Rc::new(VecModel::from(groups)),
    })
}

pub fn save(groups: &Rc<VecModel<DeviceConfigGroup>>, active_group: i32) -> std::io::Result<()> {
    let path = config_path().ok_or_else(|| {
        std::io::Error::new(std::io::ErrorKind::NotFound, "无法确定配置目录")
    })?;

    let mut saved_groups = Vec::new();
    for i in 0..groups.row_count() {
        let Some(group) = groups.row_data(i) else {
            continue;
        };
        if group.builtin {
            continue;
        }
        let items: Vec<SavedConfigItem> = (0..group.items.row_count())
            .filter_map(|j| group.items.row_data(j))
            .map(|item| SavedConfigItem {
                name: item.name.to_string(),
                register: item.register.to_string(),
                value_type: item.value_type.to_string(),
                register_count: item.register_count,
                widget_kind: item.widget_kind,
            })
            .collect();
        saved_groups.push(SavedGroup {
            title: group.title.to_string(),
            slave_id: group.slave_id.to_string(),
            items,
        });
    }

    // 仅常用组时也写入，以便记住 active_group=0
    let max_group = groups.row_count().saturating_sub(1) as i32;
    let cfg = SavedConfig {
        version: CONFIG_VERSION,
        active_group: active_group.clamp(0, max_group.max(0)),
        groups: saved_groups,
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
