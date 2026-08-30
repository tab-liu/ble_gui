//! 全局常量与共享应用上下文。

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use log::warn;
use slint::{ModelRc, VecModel};

use crate::pages::device_config::DeviceConfigState;
use crate::pages::modbus_query::ModbusQueryState;
use crate::services::ble::BleService;
use crate::services::ble_favorites::{self, FavoriteDevice};
use crate::services::device_config_store;
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
pub const PAGE_DEVICE_CONFIG: i32 = 4;

pub struct AppState {
    pub modbus_query: ModbusQueryState,
    pub device_config: DeviceConfigState,
}

#[derive(Clone)]
pub struct AppContext {
    pub state: Rc<RefCell<AppState>>,
    pub ble: BleService,
    pub modbus: ModbusService,
    pub firmware: FirmwareService,
    pub theme: ThemeService,
    /// 收藏设备（可跨线程读取，供 BLE UI 刷新钩子使用）。
    pub favorites: Arc<Mutex<Vec<FavoriteDevice>>>,
    /// 启动时恢复的 Modbus 标签页索引（来自持久化配置）。
    pub initial_modbus_tab: i32,
    /// 启动时恢复的设备配置分组索引。
    pub initial_device_config_group: i32,
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

        let (config_groups, config_builtin, initial_device_config_group) =
            match device_config_store::load() {
                Some(loaded) => (
                    loaded.groups,
                    DeviceConfigState::sample_builtin(),
                    loaded.active_group,
                ),
                None => (
                    DeviceConfigState::sample_groups(),
                    DeviceConfigState::sample_builtin(),
                    0,
                ),
            };

        Self {
            state: Rc::new(RefCell::new(AppState {
                modbus_query: ModbusQueryState::new(tabs),
                device_config: DeviceConfigState::new(config_groups, config_builtin),
            })),
            ble: BleService::new(modbus_live, query_live, query_generation),
            modbus,
            firmware: FirmwareService::new(),
            theme: ThemeService::new(),
            favorites: Arc::new(Mutex::new(ble_favorites::load())),
            initial_modbus_tab,
            initial_device_config_group,
        }
    }

    pub fn bind_modbus_tabs(&self, ui: &MainWindow) {
        let tabs = self.state.borrow().modbus_query.tabs.clone();
        ui.set_modbus_tabs(ModelRc::new(tabs));
    }

    pub fn favorites_snapshot(&self) -> Vec<FavoriteDevice> {
        self.favorites
            .lock()
            .map(|g| g.clone())
            .unwrap_or_default()
    }

    pub fn toggle_favorite(&self, address: &str, name: &str) -> bool {
        let Ok(mut favorites) = self.favorites.lock() else {
            return false;
        };
        let changed = if ble_favorites::contains(&favorites, address) {
            ble_favorites::remove(&mut favorites, address)
        } else {
            ble_favorites::upsert(&mut favorites, address, name)
        };
        if changed {
            if let Err(e) = ble_favorites::save(&favorites) {
                warn!(target: "ble_gui::favorites", "保存收藏失败: {e}");
            }
        }
        changed
    }

    pub fn favorite_name(&self, address: &str) -> Option<String> {
        self.favorites.lock().ok().and_then(|favorites| {
            favorites
                .iter()
                .find(|d| ble_favorites::addresses_equal(&d.address, address))
                .map(|d| d.name.clone())
        })
    }
}
