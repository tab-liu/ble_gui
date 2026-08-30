pub mod dashboard;
pub mod device_config;
pub mod firmware_upgrade;
pub mod modbus_query;
pub mod settings;

use crate::state::AppContext;
use crate::ui::MainWindow;

pub fn wire_all(ui: &MainWindow, ctx: &AppContext) {
    dashboard::wire(ui, ctx);
    modbus_query::wire(ui, ctx);
    device_config::wire(ui, ctx);
    settings::wire(ui, ctx);
    firmware_upgrade::wire(ui, ctx);
}
