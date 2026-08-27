//! 应用入口：初始化并运行 GUI。

mod app;
mod pages;
mod services;
mod state;
mod ui;

fn main() -> Result<(), slint::PlatformError> {
    app::run()
}
