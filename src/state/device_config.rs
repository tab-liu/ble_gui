//! 设备配置页 UI 状态（分组 VecModel + 常用项 + 配网会话）。

use std::rc::Rc;

use slint::{Model, ModelRc, SharedString, VecModel};

use crate::services::ble::modbus::{
    enum_index_for_value, BUILTIN_CONFIG_SLAVE_ID, BUILTIN_SETTINGS,
};
use crate::services::device_config_store::{ConfigGroupSchema, ConfigItemSchema};
use crate::ui::{BuiltinConfigItem, DeviceConfigGroup, DeviceConfigItem};

use super::wifi_provision::WifiProvisionUiState;

pub struct DeviceConfigState {
    pub groups: Rc<VecModel<DeviceConfigGroup>>,
    pub builtin_items: Rc<VecModel<BuiltinConfigItem>>,
    pub tab_strip_width: f32,
    pub wifi: WifiProvisionUiState,
}

fn empty_read() -> (SharedString, SharedString, SharedString, i32) {
    ("—".into(), "—".into(), "".into(), 14)
}

fn schema_item_to_config_item(item: ConfigItemSchema) -> DeviceConfigItem {
    let (result, result_display, result_hex, result_font_size) = empty_read();
    let widget_kind = item.widget_kind.clamp(0, 2);
    DeviceConfigItem {
        name: item.name.into(),
        register: item.register.into(),
        value_type: item.value_type.into(),
        register_count: item.register_count.max(1),
        widget_kind,
        result,
        result_display,
        result_hex,
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

fn schema_group_to_config_group(group: ConfigGroupSchema) -> DeviceConfigGroup {
    let items: Vec<DeviceConfigItem> = group
        .items
        .into_iter()
        .map(schema_item_to_config_item)
        .collect();
    DeviceConfigGroup {
        title: group.title.into(),
        slave_id: group.slave_id.into(),
        builtin: false,
        items: ModelRc::new(VecModel::from(items)),
    }
}

pub fn groups_from_schema(custom: Vec<ConfigGroupSchema>) -> Rc<VecModel<DeviceConfigGroup>> {
    let mut groups = vec![builtin_group()];
    groups.extend(custom.into_iter().map(schema_group_to_config_group));
    Rc::new(VecModel::from(groups))
}

pub fn custom_groups_to_schema(
    groups: &Rc<VecModel<DeviceConfigGroup>>,
) -> Vec<ConfigGroupSchema> {
    let mut out = Vec::new();
    for i in 0..groups.row_count() {
        let Some(group) = groups.row_data(i) else {
            continue;
        };
        if group.builtin {
            continue;
        }
        let items: Vec<ConfigItemSchema> = (0..group.items.row_count())
            .filter_map(|j| group.items.row_data(j))
            .map(|item| ConfigItemSchema {
                name: item.name.to_string(),
                register: item.register.to_string(),
                value_type: item.value_type.to_string(),
                register_count: item.register_count,
                widget_kind: item.widget_kind,
            })
            .collect();
        out.push(ConfigGroupSchema {
            title: group.title.to_string(),
            slave_id: group.slave_id.to_string(),
            items,
        });
    }
    out
}

fn builtin_item_from_def(
    def: &crate::services::ble::modbus::BuiltinSettingDef,
) -> BuiltinConfigItem {
    let default_write = def
        .enum_options
        .first()
        .map(|(_, v)| (*v).to_string())
        .unwrap_or_default();
    let enum_labels: Vec<SharedString> = def
        .enum_options
        .iter()
        .map(|(label, _)| (*label).into())
        .collect();
    let enum_values: Vec<SharedString> = def
        .enum_options
        .iter()
        .map(|(_, value)| (*value).into())
        .collect();
    let enum_index = enum_index_for_value(&default_write, def.enum_options);
    BuiltinConfigItem {
        id: def.id.into(),
        name: def.name.into(),
        register: def.register.to_string().into(),
        value_type: def.value_type.as_str().into(),
        register_count: def.register_count.max(1) as i32,
        widget_kind: def.widget as i32,
        enum_labels: ModelRc::new(VecModel::from(enum_labels)),
        enum_values: ModelRc::new(VecModel::from(enum_values)),
        enum_index,
        result_display: "—".into(),
        result_hex: "".into(),
        write_value: default_write.into(),
        status: "等待读取".into(),
        dirty: false,
        write_enabled: false,
        disabled_hint: "".into(),
    }
}

fn builtin_items_from_defs() -> Rc<VecModel<BuiltinConfigItem>> {
    let items: Vec<BuiltinConfigItem> = BUILTIN_SETTINGS
        .iter()
        .map(builtin_item_from_def)
        .collect();
    Rc::new(VecModel::from(items))
}

impl DeviceConfigState {
    pub fn new(
        groups: Rc<VecModel<DeviceConfigGroup>>,
        builtin_items: Rc<VecModel<BuiltinConfigItem>>,
    ) -> Self {
        Self {
            groups,
            builtin_items,
            tab_strip_width: 804.0,
            wifi: WifiProvisionUiState::load(),
        }
    }

    /// 仅固定「常用」组；灵活分组由用户添加。
    pub fn sample_groups() -> Rc<VecModel<DeviceConfigGroup>> {
        Rc::new(VecModel::from(vec![builtin_group()]))
    }

    pub fn sample_builtin() -> Rc<VecModel<BuiltinConfigItem>> {
        builtin_items_from_defs()
    }
}
