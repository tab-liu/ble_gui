//! 全局常量与共享应用上下文。

use std::cell::RefCell;
use std::rc::Rc;

use slint::{ModelRc, VecModel};

use crate::pages::modbus_query::ModbusQueryState;
use crate::services::ble::BleService;
use crate::services::firmware::FirmwareService;
use crate::services::modbus::ModbusService;
use crate::services::modbus_query_store;
use crate::services::theme::ThemeService;
use crate::ui::MainWindow;

pub mod dialog;

pub use dialog::{DIALOG_COPY_QUERY, DIALOG_NEW_TAB, DIALOG_NONE};

pub const PAGE_DASHBOARD: i32 = 0;
pub const PAGE_MODBUS: i32 = 1;
pub const PAGE_SETTINGS: i32 = 2;
pub const PAGE_FIRMWARE: i32 = 3;

pub struct AppState {
    pub modbus_query: ModbusQueryState,
}

#[derive(Clone)]
pub struct AppContext {
    pub state: Rc<RefCell<AppState>>,
    pub ble: BleService,
    pub modbus: ModbusService,
    pub firmware: FirmwareService,
    pub theme: ThemeService,
    /// 启动时恢复的 Modbus 标签页索引（来自持久化配置）。
    pub initial_modbus_tab: i32,
}

impl AppContext {
    pub fn new() -> Self {
        let modbus = ModbusService::new();
        let modbus_live = modbus.shared_live();
        let query_live = modbus.shared_query_live();
        let query_generation = modbus.shared_query_poll_generation();

        let (tabs, initial_modbus_tab) = match modbus_query_store::load() {
            Some(loaded) => (loaded.tabs, loaded.active_tab),
            None => (
                Rc::new(VecModel::from(vec![ModbusQueryState::default_tab(
                    "默认分组",
                    false,
                )])),
                0,
            ),
        };

        Self {
            state: Rc::new(RefCell::new(AppState {
                modbus_query: ModbusQueryState::new(tabs),
            })),
            ble: BleService::new(modbus_live, query_live, query_generation),
            modbus,
            firmware: FirmwareService::new(),
            theme: ThemeService::new(),
            initial_modbus_tab,
        }
    }

    pub fn bind_modbus_tabs(&self, ui: &MainWindow) {
        let tabs = self.state.borrow().modbus_query.tabs.clone();
        ui.set_modbus_tabs(ModelRc::new(tabs));
    }
}
