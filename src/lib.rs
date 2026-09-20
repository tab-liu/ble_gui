//! ble_gui 库：页面接线、服务层与 BLE worker。
//!
//! 可执行入口在 `src/main.rs`（日志、父控制台、`app::run`）。
//! `cargo test --lib` 跑本库单元测试，不必启动 GUI 进程。

pub mod app;
pub mod pages;
pub mod services;
pub mod state;
pub mod ui;
