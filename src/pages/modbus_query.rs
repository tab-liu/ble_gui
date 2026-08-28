//! Modbus 查询页：标签与寄存器列表管理。

use std::rc::Rc;

use slint::language::{DragAction, DropEvent};
use slint::{ComponentHandle, DataTransfer, Model, ModelRc, SharedString, VecModel};

use crate::app::{close_dialog, open_dialog};
use crate::state::{AppContext, DIALOG_COPY_QUERY, DIALOG_NEW_TAB};
use crate::ui::{MainWindow, ModbusDndApi, ModbusQueryItem, ModbusQueryLayoutRow, ModbusTab};

const CARD_WIDTH: f32 = 180.0;
const CARD_HEIGHT: f32 = 110.0;
const CARD_SPACING: f32 = 12.0;

#[derive(Clone)]
struct QueryDragPayload {
    tab_index: usize,
    source_index: usize,
}

struct GridLayoutState {
    width: f32,
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

impl ModbusQueryState {
    pub fn new(tabs: Rc<VecModel<ModbusTab>>) -> Self {
        Self {
            tabs,
            pending_query_tab: -1,
            grid_layout: GridLayoutState { width: 720.0 },
            pending_copy_src_tab: -1,
            pending_copy_src_items: Vec::new(),
            copy_target_tabs: Vec::new(),
            selected_indices: Vec::new(),
        }
    }

    pub fn default_tab(title: &str, with_sample: bool) -> ModbusTab {
        let items = if with_sample {
            vec![enrich_query_item(ModbusQueryItem {
                name: "室内温度".into(),
                register: "40001".into(),
                status: "正常".into(),
                result: "24.6 °C".into(),
                result_display: SharedString::default(),
                result_font_size: 20,
            })]
        } else {
            vec![]
        };
        ModbusTab {
            title: title.into(),
            slave_id: "0".into(),
            items: ModelRc::new(VecModel::from(items)),
        }
    }
}

fn clear_query_form(ui: &MainWindow) {
    ui.set_show_add_query_form(false);
    ui.set_query_form_name(SharedString::default());
    ui.set_query_form_register(SharedString::default());
}

fn clear_selection(ctx: &AppContext, ui: &MainWindow) {
    ctx.state.borrow_mut().modbus_query.selected_indices.clear();
    sync_selection_ui(ctx, ui);
}

fn sync_selection_ui(ctx: &AppContext, ui: &MainWindow) {
    let st = ctx.state.borrow();
    let tab_index = ui.get_active_modbus_tab() as usize;
    let count = st
        .modbus_query
        .tabs
        .row_data(tab_index)
        .map(|tab| tab.items.row_count())
        .unwrap_or(0);
    let states: Vec<bool> = (0..count)
        .map(|i| st.modbus_query.selected_indices.contains(&i))
        .collect();
    let has_selected = !st.modbus_query.selected_indices.is_empty();
    drop(st);
    ui.set_query_selection_states(ModelRc::new(VecModel::from(states)));
    ui.set_has_selected_queries(has_selected);
}

fn adjust_selection_after_remove(ctx: &AppContext, ui: &MainWindow, removed_index: usize) {
    let mut st = ctx.state.borrow_mut();
    st.modbus_query.selected_indices = st
        .modbus_query
        .selected_indices
        .iter()
        .filter_map(|&idx| {
            if idx == removed_index {
                None
            } else if idx > removed_index {
                Some(idx - 1)
            } else {
                Some(idx)
            }
        })
        .collect();
    drop(st);
    sync_selection_ui(ctx, ui);
}

fn cancel_rename(ui: &MainWindow) {
    ui.set_renaming_tab_index(-1);
    ui.set_editing_tab_title(SharedString::default());
}

fn result_font_size(char_count: usize) -> i32 {
    match char_count {
        0..=6 => 20,
        7..=12 => 16,
        13..=20 => 13,
        _ => 11,
    }
}

fn enrich_query_item(mut item: ModbusQueryItem) -> ModbusQueryItem {
    let display = item.result.to_string();
    item.result_font_size = result_font_size(display.chars().count());
    item.result_display = display.into();
    item
}

fn enrich_query_items(items: Vec<ModbusQueryItem>) -> Vec<ModbusQueryItem> {
    items.into_iter().map(enrich_query_item).collect()
}

fn clone_query_item_for_copy(source: &ModbusQueryItem) -> ModbusQueryItem {
    enrich_query_item(ModbusQueryItem {
        name: source.name.clone(),
        register: source.register.clone(),
        status: "等待查询".into(),
        result: "（Modbus 轮询后将自动填充）".into(),
        result_display: SharedString::default(),
        result_font_size: 20,
    })
}

fn sync_active_tab_slave_id(ui: &MainWindow, ctx: &AppContext) {
    let st = ctx.state.borrow();
    let tab_index = ui.get_active_modbus_tab() as usize;
    if let Some(tab) = st.modbus_query.tabs.row_data(tab_index) {
        ui.set_active_tab_slave_id(tab.slave_id.clone());
    }
}

fn cards_per_row(width: f32) -> usize {
    let stride = CARD_WIDTH + CARD_SPACING;
    ((width + CARD_SPACING) / stride).floor().max(1.0) as usize
}

fn compute_layout_rows(count: usize, width: f32) -> Vec<ModbusQueryLayoutRow> {
    let cols = cards_per_row(width);
    (0..count)
        .collect::<Vec<_>>()
        .chunks(cols)
        .map(|chunk| ModbusQueryLayoutRow {
            indices: ModelRc::new(VecModel::from(
                chunk.iter().map(|&idx| idx as i32).collect::<Vec<_>>(),
            )),
        })
        .collect()
}

fn drop_index_from_position(x: f32, y: f32, width: f32, count: usize) -> usize {
    if count == 0 {
        return 0;
    }
    let cols = cards_per_row(width);
    let stride_x = CARD_WIDTH + CARD_SPACING;
    let stride_y = CARD_HEIGHT + CARD_SPACING;
    let row = (y / stride_y).floor().max(0.0) as usize;
    let col = (x / stride_x).floor().max(0.0) as usize;
    let cell_x = x - col as f32 * stride_x;
    let insert_after = cell_x > CARD_WIDTH / 2.0;
    let index = row * cols + col + if insert_after { 1 } else { 0 };
    index.min(count)
}

fn sync_query_layout(ui: &MainWindow, ctx: &AppContext) {
    let st = ctx.state.borrow();
    let tab_index = ui.get_active_modbus_tab() as usize;
    let count = st
        .modbus_query
        .tabs
        .row_data(tab_index)
        .map(|tab| tab.items.row_count())
        .unwrap_or(0);
    let width = st.modbus_query.grid_layout.width;
    let rows = compute_layout_rows(count, width);
    ui.set_modbus_query_layout_rows(ModelRc::new(VecModel::from(rows)));
}

fn update_tab_items(ctx: &AppContext, ui: &MainWindow, tab_index: usize, items: Vec<ModbusQueryItem>) {
    let items = enrich_query_items(items);
    let st = ctx.state.borrow();
    if tab_index >= st.modbus_query.tabs.row_count() {
        return;
    }
    let tab = st.modbus_query.tabs.row_data(tab_index).unwrap();
    st.modbus_query.tabs.set_row_data(
        tab_index,
        ModbusTab {
            title: tab.title,
            slave_id: tab.slave_id,
            items: ModelRc::new(VecModel::from(items)),
        },
    );
    sync_query_layout(ui, ctx);
    sync_selection_ui(ctx, ui);
}

fn reorder_modbus_query(
    ctx: &AppContext,
    ui: &MainWindow,
    tab_index: usize,
    from: usize,
    mut to: usize,
) {
    let st = ctx.state.borrow();
    if tab_index >= st.modbus_query.tabs.row_count() {
        return;
    }
    let tab = st.modbus_query.tabs.row_data(tab_index).unwrap();
    let items = tab.items.clone();
    let count = items.row_count();
    if from >= count || to > count {
        return;
    }
    if from == to || from + 1 == to {
        return;
    }

    let mut items_vec: Vec<ModbusQueryItem> = (0..count)
        .filter_map(|i| items.row_data(i))
        .collect();
    let item = items_vec.remove(from);
    if to > from {
        to -= 1;
    }
    to = to.min(items_vec.len());
    items_vec.insert(to, item);

    drop(st);
    update_tab_items(ctx, ui, tab_index, items_vec);
}

fn open_copy_dialog(ui: &MainWindow, ctx: &AppContext, src_tab: usize, src_items: &[usize]) {
    if src_items.is_empty() {
        return;
    }
    let mut st = ctx.state.borrow_mut();
    let count = st.modbus_query.tabs.row_count();
    let mut labels = Vec::new();
    let mut targets = Vec::new();
    for i in 0..count {
        if i == src_tab {
            continue;
        }
        let tab = st.modbus_query.tabs.row_data(i).unwrap();
        labels.push(SharedString::from(format!("{} · 从站 {}", tab.title, tab.slave_id)));
        targets.push(i);
    }
    if targets.is_empty() {
        return;
    }
    st.modbus_query.copy_target_tabs = targets;
    st.modbus_query.pending_copy_src_tab = src_tab as i32;
    st.modbus_query.pending_copy_src_items = src_items.to_vec();
    drop(st);
    ui.set_dialog_tab_options(ModelRc::new(VecModel::from(labels)));
    open_dialog(ui, DIALOG_COPY_QUERY);
}

fn copy_queries_to_tab(ctx: &AppContext, ui: &MainWindow, target_tab: usize) {
    let st = ctx.state.borrow();
    let src_tab = st.modbus_query.pending_copy_src_tab as usize;
    let src_items = st.modbus_query.pending_copy_src_items.clone();
    if src_tab >= st.modbus_query.tabs.row_count() || target_tab >= st.modbus_query.tabs.row_count() {
        return;
    }
    let source_tab = st.modbus_query.tabs.row_data(src_tab).unwrap();
    let source_items = source_tab.items.clone();
    let target = st.modbus_query.tabs.row_data(target_tab).unwrap();
    let target_items = target.items.clone();
    let mut items_vec: Vec<ModbusQueryItem> = (0..target_items.row_count())
        .filter_map(|i| target_items.row_data(i))
        .collect();
    for src_item in src_items {
        if src_item >= source_items.row_count() {
            continue;
        }
        let Some(source_item) = source_items.row_data(src_item) else {
            continue;
        };
        items_vec.push(clone_query_item_for_copy(&source_item));
    }
    let title = target.title;
    let slave_id = target.slave_id;
    drop(st);
    let st = ctx.state.borrow();
    st.modbus_query.tabs.set_row_data(
        target_tab,
        ModbusTab {
            title,
            slave_id,
            items: ModelRc::new(VecModel::from(items_vec)),
        },
    );
    drop(st);
    close_dialog(ui);
    clear_selection(ctx, ui);
    sync_query_layout(ui, ctx);
}

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    let dnd = ui.global::<ModbusDndApi>();

    let ui_weak = ui.as_weak();
    dnd.on_make_data(move |source_index| {
        let ui = ui_weak.unwrap();
        let mut transfer = DataTransfer::default();
        transfer.set_user_data(Rc::new(QueryDragPayload {
            tab_index: ui.get_active_modbus_tab() as usize,
            source_index: source_index as usize,
        }));
        transfer
    });

    dnd.on_can_drop(|event: DropEvent| -> DragAction {
        if event
            .data
            .user_data()
            .and_then(|rc| rc.downcast::<QueryDragPayload>().ok())
            .is_some()
        {
            return DragAction::Move;
        }
        DragAction::None
    });

    let ui_weak = ui.as_weak();
    let ctx_reorder = ctx.clone();
    dnd.on_dropped(move |event: DropEvent, hover_x, hover_y, grid_width| {
        let ui = ui_weak.unwrap();
        let Some(payload) = event
            .data
            .user_data()
            .and_then(|rc| rc.downcast::<QueryDragPayload>().ok())
        else {
            return;
        };
        let st = ctx_reorder.state.borrow();
        let count = st
            .modbus_query
            .tabs
            .row_data(payload.tab_index)
            .map(|tab| tab.items.row_count())
            .unwrap_or(0);
        drop(st);

        let target_index = drop_index_from_position(hover_x, hover_y, grid_width, count);
        reorder_modbus_query(
            &ctx_reorder,
            &ui,
            payload.tab_index,
            payload.source_index,
            target_index,
        );
    });

    let ui_weak = ui.as_weak();
    let ctx_layout = ctx.clone();
    ui.on_modbus_query_layout_width_changed(move |width| {
        let ui = ui_weak.unwrap();
        ctx_layout.state.borrow_mut().modbus_query.grid_layout.width = width;
        sync_query_layout(&ui, &ctx_layout);
    });

    sync_query_layout(ui, ctx);
    sync_active_tab_slave_id(ui, ctx);
    sync_selection_ui(ctx, ui);

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
        clear_selection(&ctx_rm, &ui);
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
        drop(st);
        ui.set_active_modbus_tab(new_active as i32);
        sync_query_layout(&ui, &ctx_rm);
        sync_active_tab_slave_id(&ui, &ctx_rm);
    });

    let ui_weak = ui.as_weak();
    let ctx_switch = ctx.clone();
    ui.on_switch_modbus_tab(move |index| {
        let ui = ui_weak.unwrap();
        cancel_rename(&ui);
        clear_query_form(&ui);
        clear_selection(&ctx_switch, &ui);
        ui.set_active_modbus_tab(index);
        sync_query_layout(&ui, &ctx_switch);
        sync_active_tab_slave_id(&ui, &ctx_switch);
    });

    let ui_weak = ui.as_weak();
    let ctx_rename = ctx.clone();
    ui.on_start_rename_modbus_tab(move |index| {
        let ui = ui_weak.unwrap();
        clear_query_form(&ui);
        clear_selection(&ctx_rename, &ui);
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
        let slave_id = tab.slave_id;
        drop(st);
        ctx_commit.state.borrow().modbus_query.tabs.set_row_data(
            idx,
            ModbusTab {
                title: name.into(),
                slave_id,
                items,
            },
        );
    });

    let ui_weak = ui.as_weak();
    ui.on_cancel_rename_modbus_tab(move || {
        cancel_rename(&ui_weak.unwrap());
    });

    let ui_weak = ui.as_weak();
    let ctx_slave = ctx.clone();
    ui.on_commit_tab_slave_id(move || {
        let ui = ui_weak.unwrap();
        let slave_id = ui.get_active_tab_slave_id().trim().to_string();
        if slave_id.is_empty() {
            sync_active_tab_slave_id(&ui, &ctx_slave);
            return;
        }
        let idx = ui.get_active_modbus_tab() as usize;
        let st = ctx_slave.state.borrow();
        if idx >= st.modbus_query.tabs.row_count() {
            return;
        }
        let tab = st.modbus_query.tabs.row_data(idx).unwrap();
        let items = tab.items.clone();
        let title = tab.title;
        drop(st);
        ctx_slave.state.borrow().modbus_query.tabs.set_row_data(
            idx,
            ModbusTab {
                title,
                slave_id: slave_id.into(),
                items,
            },
        );
    });

    let ui_weak = ui.as_weak();
    let ctx_panel = ctx.clone();
    ui.on_show_add_query_panel(move |tab_index| {
        let ui = ui_weak.unwrap();
        cancel_rename(&ui);
        clear_selection(&ctx_panel, &ui);
        ctx_panel.state.borrow_mut().modbus_query.pending_query_tab = tab_index;
        ui.set_query_form_name(SharedString::default());
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
        let register = ui.get_query_form_register().trim().to_string();
        if name.is_empty() || register.is_empty() {
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
        items_vec.push(enrich_query_item(ModbusQueryItem {
            name: name.into(),
            register: register.into(),
            status: "等待查询".into(),
            result: "（Modbus 轮询后将自动填充）".into(),
            result_display: SharedString::default(),
            result_font_size: 20,
        }));
        drop(st);
        update_tab_items(&ctx_add, &ui, idx, items_vec);
    });

    let ui_weak = ui.as_weak();
    let ctx_rm_query = ctx.clone();
    ui.on_remove_modbus_query(move |tab_index, item_index| {
        let ui = ui_weak.unwrap();
        let t_idx = tab_index as usize;
        let i_idx = item_index as usize;
        let st = ctx_rm_query.state.borrow();
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
        drop(st);
        adjust_selection_after_remove(&ctx_rm_query, &ui, i_idx);
        update_tab_items(&ctx_rm_query, &ui, t_idx, items_vec);
    });

    let ui_weak = ui.as_weak();
    let ctx_sel = ctx.clone();
    ui.on_select_modbus_query(move |index, ctrl| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let mut st = ctx_sel.state.borrow_mut();
        let selected = &mut st.modbus_query.selected_indices;
        if ctrl {
            if let Some(pos) = selected.iter().position(|&i| i == idx) {
                selected.remove(pos);
            } else {
                selected.push(idx);
            }
        } else if selected.len() == 1 && selected[0] == idx {
            selected.clear();
        } else {
            *selected = vec![idx];
        }
        drop(st);
        sync_selection_ui(&ctx_sel, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_copy_sel = ctx.clone();
    ui.on_copy_selected_modbus_query(move || {
        let ui = ui_weak.unwrap();
        let selected = ctx_copy_sel.state.borrow().modbus_query.selected_indices.clone();
        if selected.is_empty() {
            return;
        }
        let src_tab = ui.get_active_modbus_tab() as usize;
        open_copy_dialog(&ui, &ctx_copy_sel, src_tab, &selected);
    });

    let ctx_pick = ctx.clone();
    let ui_weak = ui.as_weak();
    ui.on_dialog_pick_copy_tab(move |list_index| {
        let ui = ui_weak.unwrap();
        let st = ctx_pick.state.borrow();
        let target = st
            .modbus_query
            .copy_target_tabs
            .get(list_index as usize)
            .copied();
        drop(st);
        if let Some(target_tab) = target {
            copy_queries_to_tab(&ctx_pick, &ui, target_tab);
        }
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
        sync_query_layout(&ui, &ctx_confirm);
        sync_active_tab_slave_id(&ui, &ctx_confirm);
    });

    let ui_weak = ui.as_weak();
    ui.on_dialog_cancel(move || {
        close_dialog(&ui_weak.unwrap());
    });
}
