//! 全局常量与共享应用上下文。

use std::cell::RefCell;
use std::rc::Rc;

use slint::{ModelRc, VecModel};

use crate::pages::modbus_query::ModbusQueryState;
use crate::services::ble::BleService;
use crate::services::firmware::FirmwareService;
use crate::services::modbus::ModbusService;
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
}

impl AppContext {
    pub fn new() -> Self {
        Self {
            state: Rc::new(RefCell::new(AppState {
                modbus_query: ModbusQueryState::new(Rc::new(VecModel::from(vec![
                    ModbusQueryState::default_tab("默认分组", true),
                ]))),
            })),
            ble: BleService::new(),
            modbus: ModbusService::new(),
            firmware: FirmwareService::new(),
            theme: ThemeService::new(),
        }
    }

    pub fn bind_modbus_tabs(&self, ui: &MainWindow) {
        let tabs = self.state.borrow().modbus_query.tabs.clone();
        ui.set_modbus_tabs(ModelRc::new(tabs));
    }
}
