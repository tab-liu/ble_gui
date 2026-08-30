//! # ble_gui — BLE Modbus 桌面工具
//!
//! 基于 **Rust + Slint + btleplug** 的跨平台 GUI：扫描/连接 BLUETTI 类设备，
//! 经加密 BLE 链路做 Modbus 轮询与写入，并提供主页仪表、查询列表、设备配置等页面。
//!
//! ## 生成文档（rustdoc）
//!
//! 本仓库是 **binary crate**（入口在 [`main`]），多数模块为 `mod` 私有。
//! 项目已在 `.cargo/config.toml` 默认开启 `--document-private-items`，直接：
//!
//! ```bash
//! cargo doc --open --no-deps
//! # 或
//! cargo docs
//! ```
//!
//! - 私有模块/函数会编进 HTML（无需再手写 `--document-private-items`）
//! - `--no-deps` / `cargo docs`：不生成依赖库文档，打开更快
//! - 源码里用 `//!`（模块/crate）与 `///`（条目）写 Markdown，会原样进文档页
//!
//! ## 架构一览
//!
//! ```text
//! ┌─────────────┐     callbacks      ┌──────────────────┐
//! │  Slint UI   │ ◄─────────────────► │ pages::*::wire   │
//! │  (ui/*.slint)│   property sync    │ AppContext       │
//! └──────┬──────┘                     └────────┬─────────┘
//!        │ 50ms timer / refresh_all            │
//!        ▼                                     ▼
//! ┌─────────────┐                     ┌──────────────────┐
//! │ ui::bindings│                     │ services::*      │
//! └─────────────┘                     │  ble / modbus /  │
//!                                     │  stores / theme  │
//!                                     └────────┬─────────┘
//!                                              │ cmd channel
//!                                              ▼
//!                                     ┌──────────────────┐
//!                                     │ ble::worker      │
//!                                     │ (Tokio + btleplug)│
//!                                     │ poll / crypto /  │
//!                                     │ Modbus RTU+TLV   │
//!                                     └──────────────────┘
//! ```
//!
//! ### 线程模型
//!
//! - **UI 线程**：Slint 事件循环；只在此线程碰 Slint 属性与 `Rc` 状态。
//! - **BLE worker**：独立 Tokio runtime；扫描、GATT、加密握手、轮询、写寄存器。
//! - 跨线程：`Arc<Mutex<_>>` 快照（`ModbusLive`、`QueryPollSnapshot`、`BleInner`）
//!   + `slint::invoke_from_event_loop` 刷新 UI。
//!
//! ### 前台轮询策略
//!
//! Worker **只轮询当前可见页**需要的数据（见 `services::ble::poll_policy`）：
//!
//! | 页面 | 轮询内容 |
//! |------|----------|
//! | 主页 | 寄存器 100～149 + AC/DC 2011/2012 |
//! | Modbus 查询 | 当前标签内查询项 |
//! | 设备配置 | 常用映射或当前自定义分组项 |
//! | 其它 | 停止 Modbus 轮询 |
//!
//! ### 配置落盘
//!
//! - Modbus 查询：`~/.config/ble_gui/modbus_query.toml`
//! - 设备配置：`~/.config/ble_gui/device_config.toml`
//! - 收藏设备 / 主题：同目录下其它文件
//!
//! Windows 使用 `%APPDATA%\ble_gui\`。
//!
//! ### 参考实现
//!
//! `ref/tool`（Device Studio / BLE Bridge）与 `ref/dev`（设备端 IOT）为协议与
//! 寄存器行为参照；业务以本仓库代码为准。
//!
//! ## 模块地图
//!
//! | 模块 | 职责 |
//! |------|------|
//! | [`app`] | 组装 UI、定时器、事件循环入口 |
//! | [`pages`] | 各页面回调接线（dashboard / modbus / device_config …） |
//! | [`services`] | BLE、Modbus 共享态、持久化、主题 |
//! | [`state`] | `AppContext`、页面 ID、对话框常量 |
//! | [`ui`] | Slint 生成类型 + 属性刷新辅助 |

mod app;
mod pages;
mod services;
mod state;
mod ui;

fn main() -> Result<(), slint::PlatformError> {
    env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("warn,ble_gui::poll=info,ble_gui::query_ui=info"),
    )
    .format_timestamp_millis()
    .init();
    app::run()
}
