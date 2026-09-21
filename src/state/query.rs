//! 查询页 UI 状态（标签 VecModel + 选择/布局）。
//!
//! Schema 在 [`crate::services::modbus_query_store`]；本模块只做 schema ↔ Slint 映射。

use std::rc::Rc;

use slint::{Model, ModelRc, SharedString, VecModel};

use crate::services::modbus_query_store::{QueryItemSchema, QueryTabSchema};
use crate::ui::{ModbusQueryItem, ModbusTab};

pub struct GridLayoutState {
    pub(crate) width: f32,
    pub(crate) tab_strip_width: f32,
}

pub struct ModbusQueryState {
    pub tabs: Rc<VecModel<ModbusTab>>,
    pub pending_query_tab: i32,
    pub grid_layout: GridLayoutState,
    pub pending_copy_src_tab: i32,
    pub pending_copy_src_items: Vec<usize>,
    pub copy_target_tabs: Vec<usize>,
    pub selected_indices: Vec<usize>,
}

fn result_font_size(char_count: usize) -> i32 {
    match char_count {
        0..=6 => 20,
        7..=12 => 16,
        13..=20 => 13,
        _ => 11,
    }
}

fn schema_item_to_query_item(item: QueryItemSchema) -> ModbusQueryItem {
    let mut mapped = ModbusQueryItem {
        name: item.name.into(),
        register: item.register.into(),
        value_type: item.value_type.into(),
        register_count: item.register_count,
        scale: item.scale,
        status: SharedString::default(),
        result: SharedString::default(),
        result_display: SharedString::default(),
        result_hex: SharedString::default(),
        result_hex_below: false,
        result_font_size: 20,
    };
    reset_item_runtime(&mut mapped);
    mapped
}

pub(crate) fn reset_item_runtime(item: &mut ModbusQueryItem) {
    let result: SharedString = "（Modbus 轮询后将自动填充）".into();
    let display = result.to_string();
    item.status = "等待查询".into();
    item.result = result.clone();
    item.result_display = display.clone().into();
    item.result_hex = SharedString::default();
    item.result_hex_below = false;
    item.result_font_size = result_font_size(display.chars().count());
}

fn schema_tab_to_modbus_tab(tab: QueryTabSchema) -> ModbusTab {
    let items: Vec<ModbusQueryItem> = tab
        .items
        .into_iter()
        .map(schema_item_to_query_item)
        .collect();
    ModbusTab {
        title: tab.title.into(),
        slave_id: tab.slave_id.into(),
        items: ModelRc::new(VecModel::from(items)),
    }
}

pub fn tabs_from_schema(tabs: Vec<QueryTabSchema>) -> Rc<VecModel<ModbusTab>> {
    let mapped: Vec<ModbusTab> = tabs.into_iter().map(schema_tab_to_modbus_tab).collect();
    Rc::new(VecModel::from(mapped))
}

pub fn tabs_to_schema(tabs: &Rc<VecModel<ModbusTab>>) -> Vec<QueryTabSchema> {
    let mut out = Vec::new();
    for i in 0..tabs.row_count() {
        let Some(tab) = tabs.row_data(i) else {
            continue;
        };
        let items: Vec<QueryItemSchema> = (0..tab.items.row_count())
            .filter_map(|j| tab.items.row_data(j))
            .map(|item| QueryItemSchema {
                name: item.name.to_string(),
                register: item.register.to_string(),
                value_type: item.value_type.to_string(),
                register_count: item.register_count,
                scale: item.scale,
            })
            .collect();
        out.push(QueryTabSchema {
            title: tab.title.to_string(),
            slave_id: tab.slave_id.to_string(),
            items,
        });
    }
    out
}

impl ModbusQueryState {
    pub fn new(tabs: Rc<VecModel<ModbusTab>>) -> Self {
        Self {
            tabs,
            pending_query_tab: -1,
            grid_layout: GridLayoutState {
                // 与默认窗口 980 对齐，先按 4 列排；真正宽度由网格 init/changed 再校正。
                width: 788.0,
                tab_strip_width: 804.0,
            },
            pending_copy_src_tab: -1,
            pending_copy_src_items: Vec::new(),
            copy_target_tabs: Vec::new(),
            selected_indices: Vec::new(),
        }
    }

    pub fn empty_tab(title: &str) -> ModbusTab {
        ModbusTab {
            title: title.into(),
            slave_id: "0".into(),
            items: ModelRc::new(VecModel::from(Vec::<ModbusQueryItem>::new())),
        }
    }
}
