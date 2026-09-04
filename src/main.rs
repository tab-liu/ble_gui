//! # ble_gui — BLE Modbus 桌面工具
//!
//! 基于 **Rust + Slint + btleplug** 的跨平台 GUI：扫描/连接 BLUETTI 类设备，
//! 经加密 BLE 链路做 Modbus 轮询与写入。页面包括主页仪表、Modbus 查询、
//! 设备配置、固件升级、设置。
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
//!                                     │  firmware /      │
//!                                     │  stores / theme  │
//!                                     └────────┬─────────┘
//!                                              │ cmd channel
//!                                              ▼
//!                                     ┌──────────────────┐
//!                                     │ ble::worker      │
//!                                     │ (Tokio + btleplug)│
//!                                     │ poll / crypto /  │
//!                                     │ OTA XMODEM /     │
//!                                     │ Modbus RTU+TLV   │
//!                                     └──────────────────┘
//! ```
//!
//! ### 线程模型
//!
//! - **UI 线程**：Slint 事件循环；只在此线程碰 Slint 属性与 `Rc` 状态。
//! - **BLE worker**：独立 Tokio runtime；扫描、GATT、加密握手、轮询、写寄存器、OTA。
//! - 跨线程：`Arc<Mutex<_>>` 快照（`ModbusLive`、`QueryPollSnapshot`、`BleInner`、OTA）
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
//! | 固件升级 / 设置 | 停止 Modbus 轮询 |
//!
//! 读回结果只更新界面，正常路径不再打「卡片更新」一类 info 日志。
//!
//! ### 设备配置「常用」
//!
//! Schema 在 [`services::ble::modbus::device_config_builtin`]，不落盘：
//!
//! - **服务器地址**（12067，128 字节）：研发 / 生产环境枚举
//! - **绑定触发**（12170 `IOT_Enable_mix1`）：按位域 RMW，`1=enable` / `2=disable`
//!   - bit7/6 设备绑定（默认；广播名 HA1 开头的并机设备不可写）
//!   - bit1/0 IOT 并机排序、bit3/2 INV、bit5/4 PACK（HA1 可写）
//!
//! 常用项与 UI 共用同一 `VecModel`，可用性就地更新，避免下拉框被整表重建冲掉。
//!
//! ### Windows 运行形态
//!
//! - 链接为 **GUI 子系统**：资源管理器双击不额外弹出控制台。
//! - 从已有终端启动（`cargo run`、cmd 里跑 exe）时 [`attach_parent_console`] 挂到父控制台，日志仍可见。
//! - 连接：停扫描后短暂等待，GATT `connect` 对 `Not connected`（WinRT Unreachable）自动重试。
//!
//! ### 配置落盘
//!
//! - Modbus 查询：`~/.config/ble_gui/modbus_query.toml`
//! - 设备配置：`~/.config/ble_gui/device_config.toml`
//! - 收藏设备（以 MAC 为键，广播名仅展示）/ 主题：同目录下其它文件
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
//! | [`pages`] | 各页面回调接线（dashboard / modbus / device_config / firmware …） |
//! | [`services`] | BLE、Modbus、固件 OTA、持久化、主题 |
//! | [`state`] | `AppContext`、页面 ID、对话框常量 |
//! | [`ui`] | Slint 生成类型 + 属性刷新辅助 |

// Windows 默认把 bin 编成控制台程序：从资源管理器启动会再开一个终端。
// GUI 子系统双击不再弹窗；若已在终端里启动（如 cargo run），再挂到父控制台输出日志。
#![windows_subsystem = "windows"]

mod app;
mod pages;
mod services;
mod state;
mod ui;

fn main() -> Result<(), slint::PlatformError> {
    #[cfg(windows)]
    attach_parent_console();

    env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("warn"),
    )
    .format_timestamp_millis()
    .init();
    app::run()
}

/// 若父进程已有控制台（`cargo run`、cmd 里启动），把 stdout/stderr 接到那里。
/// 从资源管理器双击时没有父控制台，AttachConsole 失败，也就不会多出一个黑窗口。
#[cfg(windows)]
fn attach_parent_console() {
    const ATTACH_PARENT_PROCESS: u32 = 0xFFFF_FFFF;
    const STD_OUTPUT_HANDLE: u32 = 0xFFFF_FFF5; // -11
    const STD_ERROR_HANDLE: u32 = 0xFFFF_FFF4; // -12
    const GENERIC_READ: u32 = 0x8000_0000;
    const GENERIC_WRITE: u32 = 0x4000_0000;
    const FILE_SHARE_READ: u32 = 1;
    const FILE_SHARE_WRITE: u32 = 2;
    const OPEN_EXISTING: u32 = 3;
    const FILE_ATTRIBUTE_NORMAL: u32 = 0x80;
    const INVALID_HANDLE: isize = -1;

    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn AttachConsole(dw_process_id: u32) -> i32;
        fn SetStdHandle(n_std_handle: u32, h_handle: isize) -> i32;
        fn CreateFileA(
            file_name: *const u8,
            desired_access: u32,
            share_mode: u32,
            security: isize,
            creation_disposition: u32,
            flags: u32,
            template: isize,
        ) -> isize;
    }

    unsafe {
        if AttachConsole(ATTACH_PARENT_PROCESS) == 0 {
            return;
        }
        let name = b"CONOUT$\0";
        let handle = CreateFileA(
            name.as_ptr(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0,
        );
        if handle == 0 || handle == INVALID_HANDLE {
            return;
        }
        let _ = SetStdHandle(STD_OUTPUT_HANDLE, handle);
        let _ = SetStdHandle(STD_ERROR_HANDLE, handle);
    }
}
