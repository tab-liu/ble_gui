//! 设备配置页：固定「常用」+ 可编辑灵活分组。
//!
//! # 设计要点
//!
//! - **常用**（`builtin=true`）：schema 在 `device_config_builtin`（[`BUILTIN_SETTINGS`]），不可删改名；
//!   可含 bit 域，写入时 RMW。  
//! - **自定义分组**：整寄存器表单（开关/文本/数值），TOML 持久化见
//!   [`crate::services::device_config_store`]。  
//! - **读写分离**：轮询只改「读回」字段，不覆盖「设置」输入框。  
//! - 与查询页共用 `QueryPollSnapshot`，目标为 [`QueryPollTarget::DeviceConfig`]。

use std::rc::Rc;

use log::{debug, warn};
use slint::{ComponentHandle, Model, ModelRc, SharedString, VecModel};

use crate::services::ble::modbus::{
    bind_option_standalone_only, bind_trigger_field, builtin_bind_supported, encode_write_value,
    is_parallel_ha1_device, parse_register_address, parse_value_type, BuiltinSettingDef,
    BuiltinWidget, BUILTIN_CONFIG_SLAVE_ID, BUILTIN_SETTINGS, enum_index_for_value,
};
use crate::services::device_config_store;
use crate::services::modbus::QueryPollTarget;
use crate::services::poll_sync::sync_poll_policy;
use crate::state::{AppContext, PAGE_DEVICE_CONFIG};
use crate::ui::{BuiltinConfigItem, DeviceConfigGroup, DeviceConfigItem, MainWindow};

pub struct DeviceConfigState {
    pub groups: Rc<VecModel<DeviceConfigGroup>>,
    pub builtin_items: Rc<VecModel<BuiltinConfigItem>>,
}

fn empty_read() -> (SharedString, SharedString, i32) {
    ("—".into(), "—".into(), 14)
}

fn result_font_size(char_count: usize) -> i32 {
    match char_count {
        0..=6 => 16,
        7..=12 => 14,
        13..=20 => 12,
        _ => 11,
    }
}

fn display_for_config(result: &str, widget_kind: i32) -> SharedString {
    if widget_kind == 0 {
        match result.trim() {
            "1" | "true" => "开".into(),
            "0" | "false" => "关".into(),
            other if other.is_empty() || other == "—" => "—".into(),
            other => other.into(),
        }
    } else {
        result.into()
    }
}

fn display_for_builtin_result(result: &str, def: &BuiltinSettingDef) -> SharedString {
    display_for_config(result, def.widget as i32)
}

fn builtin_availability(
    def: &BuiltinSettingDef,
    connected: bool,
    device_name: &str,
    write_value: &str,
) -> (bool, SharedString) {
    if def.id == "device_bind" {
        let enabled = builtin_bind_supported(connected, device_name, write_value);
        let hint = if connected
            && bind_option_standalone_only(write_value)
            && is_parallel_ha1_device(device_name)
        {
            "并机(HA1)设备不支持设备绑定".into()
        } else {
            "".into()
        };
        (enabled, hint)
    } else {
        (connected, "".into())
    }
}

fn apply_builtin_availability_in_place(ctx: &AppContext) {
    let snap = ctx.ble.snapshot();
    let connected = ctx.ble.is_connected();
    let st = ctx.state.borrow();
    for i in 0..st.device_config.builtin_items.row_count() {
        let Some(def) = BUILTIN_SETTINGS.get(i) else {
            continue;
        };
        let Some(mut item) = st.device_config.builtin_items.row_data(i) else {
            continue;
        };
        let (write_enabled, disabled_hint) =
            builtin_availability(def, connected, &snap.device_name, item.write_value.as_str());
        if item.write_enabled != write_enabled || item.disabled_hint != disabled_hint {
            item.write_enabled = write_enabled;
            item.disabled_hint = disabled_hint;
            st.device_config.builtin_items.set_row_data(i, item);
        }
    }
}

fn builtin_item_from_def(def: &BuiltinSettingDef) -> BuiltinConfigItem {
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

fn refresh_builtin_enum_index(item: &mut BuiltinConfigItem, options: &[(&str, &str)]) {
    item.enum_index = enum_index_for_value(item.write_value.as_str(), options);
}

impl DeviceConfigState {
    pub fn new(
        groups: Rc<VecModel<DeviceConfigGroup>>,
        builtin_items: Rc<VecModel<BuiltinConfigItem>>,
    ) -> Self {
        Self {
            groups,
            builtin_items,
        }
    }

    /// 仅固定「常用」组；灵活分组由用户添加。
    pub fn sample_groups() -> Rc<VecModel<DeviceConfigGroup>> {
        Rc::new(VecModel::from(vec![DeviceConfigGroup {
            title: "常用".into(),
            slave_id: BUILTIN_CONFIG_SLAVE_ID.to_string().into(),
            builtin: true,
            items: ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())),
        }]))
    }

    pub fn sample_builtin() -> Rc<VecModel<BuiltinConfigItem>> {
        builtin_items_from_defs()
    }
}

fn is_builtin_group(ctx: &AppContext, group_index: usize) -> bool {
    ctx.state
        .borrow()
        .device_config
        .groups
        .row_data(group_index)
        .map(|g| g.builtin)
        .unwrap_or(false)
}

fn widget_kind_from_index(index: i32) -> i32 {
    match index {
        1 => 2,
        _ => 1,
    }
}

fn value_type_from_index(index: i32) -> SharedString {
    match index {
        1 => "float".into(),
        2 => "string".into(),
        _ => "integer".into(),
    }
}

fn items_vec(group: &DeviceConfigGroup) -> Vec<DeviceConfigItem> {
    (0..group.items.row_count())
        .filter_map(|i| group.items.row_data(i))
        .collect()
}

fn sync_active_to_ui(ui: &MainWindow, ctx: &AppContext) {
    let group_index = ui.get_active_config_group() as usize;
    let st = ctx.state.borrow();
    let Some(group) = st.device_config.groups.row_data(group_index) else {
        ui.set_active_config_items(ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())));
        ui.set_active_group_slave_id("0".into());
        return;
    };
    ui.set_active_group_slave_id(group.slave_id.clone());
    if group.builtin {
        drop(st);
        apply_builtin_availability_in_place(ctx);
        let model = ctx.state.borrow().device_config.builtin_items.clone();
        ui.set_builtin_config_items(ModelRc::new(model));
        ui.set_active_config_items(ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())));
    } else {
        let items = items_vec(&group);
        drop(st);
        ui.set_active_config_items(ModelRc::new(VecModel::from(items)));
    }
}

fn write_group_items(
    ctx: &AppContext,
    group_index: usize,
    slave_id: SharedString,
    title: SharedString,
    builtin: bool,
    items: Vec<DeviceConfigItem>,
) {
    let st = ctx.state.borrow();
    st.device_config.groups.set_row_data(
        group_index,
        DeviceConfigGroup {
            title,
            slave_id,
            builtin,
            items: ModelRc::new(VecModel::from(items)),
        },
    );
}

/// BLE 连接态或设备名变化时刷新常用项可用性（如 HA1 并机禁用绑定）。
/// 只就地改 `write_enabled`，不重建 model，避免 ComboBox 无法点选。
pub fn refresh_builtin_availability(_ui: &MainWindow, ctx: &AppContext) {
    apply_builtin_availability_in_place(ctx);
}

fn touch_poll_policy(ui: &MainWindow, ctx: &AppContext) {
    if ctx.ble.ui_page() == PAGE_DEVICE_CONFIG {
        sync_poll_policy(ui, ctx);
    }
}

fn persist_device_config(ctx: &AppContext, ui: &MainWindow) {
    let groups = ctx.state.borrow().device_config.groups.clone();
    let active = ui.get_active_config_group();
    if let Err(e) = device_config_store::save(&groups, active) {
        warn!(
            target: "ble_gui::config_store",
            "保存设备配置失败: {e}",
        );
    }
}

/// 将 worker 轮询结果合并到设备配置读回（不覆盖 write_value）。
pub fn apply_config_poll_results(ui: &MainWindow, ctx: &AppContext) {
    let snapshot = ctx
        .modbus
        .shared_query_live()
        .lock()
        .map(|s| s.clone())
        .unwrap_or_default();

    if snapshot.items.is_empty() {
        return;
    }

    let QueryPollTarget::DeviceConfig {
        group_index,
        builtin,
    } = snapshot.target
    else {
        return;
    };

    let active = ui.get_active_config_group() as usize;
    if group_index != active {
        debug!(
            target: "ble_gui::config_ui",
            "apply 跳过: 快照组 {group_index} != 当前组 {active}",
        );
        return;
    }

    if builtin {
        let st = ctx.state.borrow();
        for r in &snapshot.items {
            let Some(mut item) = st.device_config.builtin_items.row_data(r.item_index) else {
                continue;
            };
            let new_status: SharedString = r.status.clone().into();
            let new_display = BUILTIN_SETTINGS
                .get(r.item_index)
                .map(|def| display_for_builtin_result(&r.result, def))
                .unwrap_or_else(|| display_for_config(&r.result, item.widget_kind));
            if item.status != new_status || item.result_display != new_display {
                item.status = new_status;
                item.result_display = new_display;
                if !item.dirty {
                    if let Some(def) = BUILTIN_SETTINGS.get(r.item_index) {
                        if def.widget == BuiltinWidget::Enum
                            && def.id != "device_bind"
                            && def.field.is_none()
                            && !r.result.trim().is_empty()
                        {
                            let idx = enum_index_for_value(&r.result, def.enum_options);
                            if idx >= 0 {
                                item.write_value = r.result.clone().into();
                                item.enum_index = idx;
                            }
                        }
                    }
                }
                st.device_config
                    .builtin_items
                    .set_row_data(r.item_index, item);
            }
        }
        return;
    }

    let st = ctx.state.borrow();
    let Some(group) = st.device_config.groups.row_data(group_index) else {
        return;
    };
    let mut items = items_vec(&group);
    let title = group.title.clone();
    let slave_id = group.slave_id.clone();
    let group_builtin = group.builtin;
    drop(st);

    let mut changed = false;
    for r in &snapshot.items {
        let Some(item) = items.get_mut(r.item_index) else {
            continue;
        };
        let new_status: SharedString = r.status.clone().into();
        let new_result: SharedString = r.result.clone().into();
        if item.status != new_status || item.result != new_result {
            item.status = new_status;
            item.result = new_result.clone();
            let display = display_for_config(&new_result, item.widget_kind);
            item.result_display = display;
            item.result_font_size = result_font_size(new_result.chars().count());
            changed = true;
        }
    }

    if !changed {
        return;
    }

    write_group_items(ctx, group_index, slave_id, title, group_builtin, items);
    sync_active_to_ui(ui, ctx);
}

fn parse_switch_on(text: &str) -> bool {
    matches!(
        text.trim().to_lowercase().as_str(),
        "1" | "true" | "on" | "开"
    )
}

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    ui.set_config_groups(ModelRc::new(ctx.state.borrow().device_config.groups.clone()));
    ui.set_active_config_group(ctx.initial_device_config_group);
    sync_active_to_ui(ui, ctx);
    ui.set_show_add_config_form(false);
    ui.set_config_form_widget_index(1);
    ui.set_config_form_value_type_index(0);
    ui.set_config_form_register_count("1".into());
    ui.set_renaming_config_group_index(-1);

    let ui_weak = ui.as_weak();
    let ctx_add_group = ctx.clone();
    ui.on_add_device_config_group_request(move || {
        let ui = ui_weak.unwrap();
        let st = ctx_add_group.state.borrow();
        let custom_n = (0..st.device_config.groups.row_count())
            .filter_map(|i| st.device_config.groups.row_data(i))
            .filter(|g| !g.builtin)
            .count()
            + 1;
        st.device_config.groups.push(DeviceConfigGroup {
            title: format!("分组 {custom_n}").into(),
            slave_id: "0".into(),
            builtin: false,
            items: ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())),
        });
        let idx = st.device_config.groups.row_count() as i32 - 1;
        drop(st);
        ui.set_active_config_group(idx);
        ui.set_show_add_config_form(false);
        sync_active_to_ui(&ui, &ctx_add_group);
        touch_poll_policy(&ui, &ctx_add_group);
        persist_device_config(&ctx_add_group, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_rm = ctx.clone();
    ui.on_remove_device_config_group(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        if is_builtin_group(&ctx_rm, idx) {
            return;
        }
        let st = ctx_rm.state.borrow();
        if idx >= st.device_config.groups.row_count() {
            return;
        }
        // 至少保留「常用」
        if st.device_config.groups.row_count() <= 1 {
            return;
        }
        st.device_config.groups.remove(idx);
        let max = st.device_config.groups.row_count().saturating_sub(1);
        drop(st);
        let new_active = (ui.get_active_config_group() as usize).min(max);
        ui.set_active_config_group(new_active as i32);
        ui.set_renaming_config_group_index(-1);
        ui.set_show_add_config_form(false);
        sync_active_to_ui(&ui, &ctx_rm);
        touch_poll_policy(&ui, &ctx_rm);
        persist_device_config(&ctx_rm, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_sw = ctx.clone();
    ui.on_switch_device_config_group(move |index| {
        let ui = ui_weak.unwrap();
        ui.set_active_config_group(index);
        ui.set_renaming_config_group_index(-1);
        ui.set_show_add_config_form(false);
        sync_active_to_ui(&ui, &ctx_sw);
        touch_poll_policy(&ui, &ctx_sw);
        persist_device_config(&ctx_sw, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_start_rename = ctx.clone();
    ui.on_start_rename_device_config_group(move |index| {
        if is_builtin_group(&ctx_start_rename, index as usize) {
            return;
        }
        let ui = ui_weak.unwrap();
        let title = ctx_start_rename
            .state
            .borrow()
            .device_config
            .groups
            .row_data(index as usize)
            .map(|g| g.title)
            .unwrap_or_default();
        ui.set_editing_group_title(title);
        ui.set_renaming_config_group_index(index);
    });

    let ui_weak = ui.as_weak();
    let ctx_rename = ctx.clone();
    ui.on_commit_rename_device_config_group(move || {
        let ui = ui_weak.unwrap();
        let idx = ui.get_renaming_config_group_index();
        if idx < 0 || is_builtin_group(&ctx_rename, idx as usize) {
            ui.set_renaming_config_group_index(-1);
            return;
        }
        let title = ui.get_editing_group_title().to_string();
        let st = ctx_rename.state.borrow();
        if let Some(mut g) = st.device_config.groups.row_data(idx as usize) {
            if !title.trim().is_empty() {
                g.title = title.trim().into();
                st.device_config.groups.set_row_data(idx as usize, g);
            }
        }
        drop(st);
        ui.set_renaming_config_group_index(-1);
        persist_device_config(&ctx_rename, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_slave = ctx.clone();
    ui.on_commit_device_config_group_slave_id(move || {
        let ui = ui_weak.unwrap();
        let idx = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_slave, idx) {
            return;
        }
        let slave = ui.get_active_group_slave_id();
        let st = ctx_slave.state.borrow();
        if let Some(mut g) = st.device_config.groups.row_data(idx) {
            g.slave_id = slave;
            st.device_config.groups.set_row_data(idx, g);
        }
        drop(st);
        touch_poll_policy(&ui, &ctx_slave);
        persist_device_config(&ctx_slave, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_show_add = ctx.clone();
    ui.on_show_add_device_config_panel(move || {
        let ui = ui_weak.unwrap();
        if is_builtin_group(&ctx_show_add, ui.get_active_config_group() as usize) {
            return;
        }
        ui.set_show_add_config_form(true);
        ui.set_config_form_name("".into());
        ui.set_config_form_register("".into());
        ui.set_config_form_register_count("1".into());
    });

    let ui_weak = ui.as_weak();
    ui.on_cancel_add_device_config(move || {
        ui_weak.unwrap().set_show_add_config_form(false);
    });

    let ui_weak = ui.as_weak();
    let ctx_add = ctx.clone();
    ui.on_confirm_add_device_config(move || {
        let ui = ui_weak.unwrap();
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_add, group_index) {
            return;
        }
        let name = ui.get_config_form_name().to_string();
        let register = ui.get_config_form_register().to_string();
        if name.trim().is_empty() || register.trim().is_empty() {
            return;
        }
        let count = ui
            .get_config_form_register_count()
            .to_string()
            .trim()
            .parse::<i32>()
            .unwrap_or(1)
            .max(1);
        let (result, result_display, result_font_size) = empty_read();
        let item = DeviceConfigItem {
            name: name.trim().into(),
            register: register.trim().into(),
            value_type: value_type_from_index(ui.get_config_form_value_type_index()),
            register_count: count,
            widget_kind: widget_kind_from_index(ui.get_config_form_widget_index()),
            result,
            result_display,
            result_font_size,
            write_value: "".into(),
            status: "等待读取".into(),
            dirty: false,
        };

        let st = ctx_add.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        items.push(item);
        write_group_items(&ctx_add, group_index, slave_id, title, builtin, items);
        ui.set_show_add_config_form(false);
        sync_active_to_ui(&ui, &ctx_add);
        touch_poll_policy(&ui, &ctx_add);
        persist_device_config(&ctx_add, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_edit = ctx.clone();
    ui.on_device_config_value_edited(move |index, value| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_edit, group_index) {
            return;
        }
        let st = ctx_edit.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        let Some(item) = items.get_mut(idx) else {
            return;
        };
        item.write_value = value;
        item.dirty = true;
        item.status = "未写入".into();
        write_group_items(&ctx_edit, group_index, slave_id, title, builtin, items);
        sync_active_to_ui(&ui, &ctx_edit);
    });

    let ui_weak = ui.as_weak();
    let ctx_write = ctx.clone();
    ui.on_write_device_config_item(move |index| {
        let ui = ui_weak.unwrap();
        if !ctx_write.ble.is_connected() {
            return;
        }
        let idx = index as usize;
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_write, group_index) {
            return;
        }
        let st = ctx_write.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id_text = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        let Some(item) = items.get_mut(idx) else {
            return;
        };

        let slave_id = slave_id_text
            .to_string()
            .trim()
            .parse::<u8>()
            .unwrap_or(0);
        let address = match parse_register_address(&item.register.to_string()) {
            Ok(a) => a,
            Err(err) => {
                item.status = err.into();
                write_group_items(&ctx_write, group_index, slave_id_text, title, builtin, items);
                sync_active_to_ui(&ui, &ctx_write);
                return;
            }
        };
        let value_type = parse_value_type(&item.value_type.to_string());
        let values = match encode_write_value(
            &item.write_value.to_string(),
            value_type,
            item.register_count.max(1) as u16,
        ) {
            Ok(v) => v,
            Err(err) => {
                item.status = err.into();
                write_group_items(&ctx_write, group_index, slave_id_text, title, builtin, items);
                sync_active_to_ui(&ui, &ctx_write);
                return;
            }
        };

        item.dirty = false;
        item.status = "写入中…".into();
        write_group_items(
            &ctx_write,
            group_index,
            slave_id_text,
            title,
            builtin,
            items,
        );
        sync_active_to_ui(&ui, &ctx_write);
        ctx_write
            .ble
            .write_holding(slave_id, address, values, None, None);
    });

    let ui_weak = ui.as_weak();
    let ctx_rm_item = ctx.clone();
    ui.on_remove_device_config_item(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_rm_item, group_index) {
            return;
        }
        let st = ctx_rm_item.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        if idx < items.len() {
            items.remove(idx);
            write_group_items(&ctx_rm_item, group_index, slave_id, title, builtin, items);
            sync_active_to_ui(&ui, &ctx_rm_item);
            touch_poll_policy(&ui, &ctx_rm_item);
            persist_device_config(&ctx_rm_item, &ui);
        }
    });

    let ctx_builtin_edit = ctx.clone();
    ui.on_builtin_config_value_edited(move |index, value| {
        let idx = index as usize;
        let st = ctx_builtin_edit.state.borrow();
        let Some(mut item) = st.device_config.builtin_items.row_data(idx) else {
            return;
        };
        item.write_value = value;
        item.dirty = true;
        item.status = "未写入".into();
        if let Some(def) = BUILTIN_SETTINGS.get(idx) {
            refresh_builtin_enum_index(&mut item, def.enum_options);
        }
        st.device_config.builtin_items.set_row_data(idx, item);
        drop(st);
        apply_builtin_availability_in_place(&ctx_builtin_edit);
    });

    let ctx_builtin_write = ctx.clone();
    ui.on_write_builtin_config_item(move |index| {
        if !ctx_builtin_write.ble.is_connected() {
            return;
        }
        let idx = index as usize;
        let Some(def) = BUILTIN_SETTINGS.get(idx) else {
            return;
        };
        let st = ctx_builtin_write.state.borrow();
        let Some(mut item) = st.device_config.builtin_items.row_data(idx) else {
            return;
        };
        let write_text = item.write_value.to_string();
        if def.id == "device_bind" {
            let snap = ctx_builtin_write.ble.snapshot();
            if bind_option_standalone_only(&write_text)
                && is_parallel_ha1_device(&snap.device_name)
            {
                item.status = "并机(HA1)设备不支持设备绑定".into();
                st.device_config.builtin_items.set_row_data(idx, item);
                return;
            }
            let Some(field) = bind_trigger_field(&write_text) else {
                item.status = "未知绑定选项".into();
                st.device_config.builtin_items.set_row_data(idx, item);
                return;
            };
            item.dirty = false;
            item.status = "写入中…".into();
            st.device_config.builtin_items.set_row_data(idx, item);
            drop(st);
            ctx_builtin_write.ble.write_holding(
                BUILTIN_CONFIG_SLAVE_ID,
                def.register,
                vec![field.value],
                None,
                Some(field),
            );
            return;
        }
        let values = match def.widget {
            BuiltinWidget::Switch => {
                vec![if parse_switch_on(&write_text) { 1 } else { 0 }]
            }
            BuiltinWidget::Enum | BuiltinWidget::Number => {
                match encode_write_value(&write_text, def.value_type, def.register_count) {
                    Ok(v) => v,
                    Err(err) => {
                        item.status = err.into();
                        st.device_config.builtin_items.set_row_data(idx, item);
                        return;
                    }
                }
            }
        };
        item.dirty = false;
        item.status = "写入中…".into();
        st.device_config.builtin_items.set_row_data(idx, item);
        drop(st);

        ctx_builtin_write.ble.write_holding(
            BUILTIN_CONFIG_SLAVE_ID,
            def.register,
            values,
            def.bit,
            def.field,
        );
    });
}
