//! 应用入口：初始化并运行 GUI。

mod app;
mod pages;
mod services;
mod state;
mod ui;

fn main() -> Result<(), slint::PlatformError> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("warn"))
    .format_timestamp_millis()
    .init();
    app::run()
}
