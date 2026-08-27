//! Modbus 查询页：标签与寄存器列表管理。

use slint::{ComponentHandle, Model, ModelRc, SharedString, VecModel};

use crate::app::{close_dialog, open_dialog};
use crate::state::{AppContext, DIALOG_NEW_TAB};
use crate::ui::{MainWindow, ModbusQueryItem, ModbusTab};

pub struct ModbusQueryState {
    pub tabs: Rc<VecModel<ModbusTab>>,
    pub pending_query_tab: i32,
}

type Rc<T> = std::rc::Rc<T>;

impl ModbusQueryState {
    pub fn new(tabs: Rc<VecModel<ModbusTab>>) -> Self {
        Self {
            tabs,
            pending_query_tab: -1,
        }
    }

    pub fn default_tab(title: &str, with_sample: bool) -> ModbusTab {
        let items = if with_sample {
            vec![ModbusQueryItem {
                name: "室内温度".into(),
                slave_id: "1".into(),
                register: "40001".into(),
                status: "正常".into(),
                result: "24.6 °C".into(),
            }]
        } else {
            vec![]
        };
        ModbusTab {
            title: title.into(),
            items: ModelRc::new(VecModel::from(items)),
        }
    }
}

fn clear_query_form(ui: &MainWindow) {
    ui.set_show_add_query_form(false);
    ui.set_query_form_name(SharedString::default());
    ui.set_query_form_slave_id("1".into());
    ui.set_query_form_register(SharedString::default());
}

fn cancel_rename(ui: &MainWindow) {
    ui.set_renaming_tab_index(-1);
    ui.set_editing_tab_title(SharedString::default());
}

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    ui.on_add_modbus_tab_request({
        let ui_weak = ui.as_weak();
        move || {
            open_dialog(&ui_weak.unwrap(), DIALOG_NEW_TAB);
        }
    });

    let ui_weak = ui.as_weak();
    let ctx_rm = ctx.clone();
    ui.on_remove_modbus_tab(move |index| {
        let ui = ui_weak.unwrap();
        cancel_rename(&ui);
        clear_query_form(&ui);
        let st = ctx_rm.state.borrow_mut();
        if st.modbus_query.tabs.row_count() <= 1 {
            return;
        }
        let idx = index as usize;
        if idx < st.modbus_query.tabs.row_count() {
            st.modbus_query.tabs.remove(idx);
        }
        let active = ui.get_active_modbus_tab() as usize;
        let new_active = if active >= st.modbus_query.tabs.row_count() {
            st.modbus_query.tabs.row_count().saturating_sub(1)
        } else {
            active
        };
        ui.set_active_modbus_tab(new_active as i32);
    });

    let ui_weak = ui.as_weak();
    ui.on_switch_modbus_tab(move |index| {
        let ui = ui_weak.unwrap();
        cancel_rename(&ui);
        clear_query_form(&ui);
        ui.set_active_modbus_tab(index);
    });

    let ui_weak = ui.as_weak();
    let ctx_rename = ctx.clone();
    ui.on_start_rename_modbus_tab(move |index| {
        let ui = ui_weak.unwrap();
        clear_query_form(&ui);
        let st = ctx_rename.state.borrow();
        if let Some(tab) = st.modbus_query.tabs.row_data(index as usize) {
            ui.set_editing_tab_title(tab.title);
            ui.set_renaming_tab_index(index);
            ui.set_active_modbus_tab(index);
        }
    });

    let ui_weak = ui.as_weak();
    let ctx_commit = ctx.clone();
    ui.on_commit_rename_modbus_tab(move || {
        let ui = ui_weak.unwrap();
        let name = ui.get_editing_tab_title().trim().to_string();
        if name.is_empty() {
            cancel_rename(&ui);
            return;
        }
        let idx = ui.get_renaming_tab_index() as usize;
        cancel_rename(&ui);
        let st = ctx_commit.state.borrow();
        if idx >= st.modbus_query.tabs.row_count() {
            return;
        }
        let tab = st.modbus_query.tabs.row_data(idx).unwrap();
        let items = tab.items.clone();
        drop(st);
        ctx_commit.state.borrow().modbus_query.tabs.set_row_data(
            idx,
            ModbusTab {
                title: name.into(),
                items,
            },
        );
    });

    let ui_weak = ui.as_weak();
    ui.on_cancel_rename_modbus_tab(move || {
        cancel_rename(&ui_weak.unwrap());
    });

    let ui_weak = ui.as_weak();
    let ctx_panel = ctx.clone();
    ui.on_show_add_query_panel(move |tab_index| {
        let ui = ui_weak.unwrap();
        cancel_rename(&ui);
        ctx_panel.state.borrow_mut().modbus_query.pending_query_tab = tab_index;
        ui.set_query_form_name(SharedString::default());
        ui.set_query_form_slave_id("1".into());
        ui.set_query_form_register(SharedString::default());
        ui.set_show_add_query_form(true);
    });

    let ui_weak = ui.as_weak();
    ui.on_cancel_add_query(move || {
        clear_query_form(&ui_weak.unwrap());
    });

    let ui_weak = ui.as_weak();
    let ctx_add = ctx.clone();
    ui.on_confirm_add_query(move || {
        let ui = ui_weak.unwrap();
        let name = ui.get_query_form_name().trim().to_string();
        let slave_id = ui.get_query_form_slave_id().trim().to_string();
        let register = ui.get_query_form_register().trim().to_string();
        if name.is_empty() || slave_id.is_empty() || register.is_empty() {
            return;
        }
        let idx = ctx_add.state.borrow().modbus_query.pending_query_tab as usize;
        clear_query_form(&ui);
        let st = ctx_add.state.borrow();
        if idx >= st.modbus_query.tabs.row_count() {
            return;
        }
        let tab = st.modbus_query.tabs.row_data(idx).unwrap();
        let items = tab.items.clone();
        let mut items_vec: Vec<ModbusQueryItem> = (0..items.row_count())
            .filter_map(|i| items.row_data(i))
            .collect();
        items_vec.push(ModbusQueryItem {
            name: name.into(),
            slave_id: slave_id.into(),
            register: register.into(),
            status: "等待查询".into(),
            result: "（Modbus 轮询后将自动填充）".into(),
        });
        st.modbus_query.tabs.set_row_data(
            idx,
            ModbusTab {
                title: tab.title,
                items: ModelRc::new(VecModel::from(items_vec)),
            },
        );
    });

    let ctx_rm_query = ctx.clone();
    ui.on_remove_modbus_query(move |tab_index, item_index| {
        let st = ctx_rm_query.state.borrow();
        let t_idx = tab_index as usize;
        let i_idx = item_index as usize;
        if t_idx >= st.modbus_query.tabs.row_count() {
            return;
        }
        let tab = st.modbus_query.tabs.row_data(t_idx).unwrap();
        let items = tab.items.clone();
        if i_idx >= items.row_count() {
            return;
        }
        let mut items_vec: Vec<ModbusQueryItem> = (0..items.row_count())
            .filter_map(|i| items.row_data(i))
            .collect();
        items_vec.remove(i_idx);
        st.modbus_query.tabs.set_row_data(
            t_idx,
            ModbusTab {
                title: tab.title,
                items: ModelRc::new(VecModel::from(items_vec)),
            },
        );
    });

    let ui_weak = ui.as_weak();
    let ctx_confirm = ctx.clone();
    ui.on_dialog_confirm(move || {
        let ui = ui_weak.unwrap();
        if ui.get_dialog_kind() != DIALOG_NEW_TAB {
            return;
        }
        let name = ui.get_dialog_name().trim().to_string();
        if name.is_empty() {
            return;
        }
        close_dialog(&ui);
        let st = ctx_confirm.state.borrow();
        st.modbus_query.tabs.push(ModbusQueryState::default_tab(
            &name,
            false,
        ));
        ui.set_active_modbus_tab(st.modbus_query.tabs.row_count() as i32 - 1);
    });

    let ui_weak = ui.as_weak();
    ui.on_dialog_cancel(move || {
        close_dialog(&ui_weak.unwrap());
    });
}
