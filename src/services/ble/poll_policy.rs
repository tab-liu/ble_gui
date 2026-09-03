//! 前台轮询策略：worker 只轮询当前可见页面所需的数据。
//!
//! [`PollForeground`] 由 UI 线程经 [`crate::services::poll_sync`] 写入
//! [`SharedPollPolicy`]；worker 在每轮轮询前读取 [`effective_foreground`]。
//!
//! 连接刚就绪且策略仍为 [`PollForeground::None`]、同时 UI 在主页时，
//! [`ensure_dashboard_poll_if_idle`] 会自动切到仪表板轮询（不依赖 UI 定时器）。

use std::sync::{Arc, Mutex};

use log::info;

use crate::services::ble::modbus::QueryValueType;

/// 与 `crate::state::PAGE_DASHBOARD` 一致，避免 ble 模块依赖 state。
pub const UI_PAGE_DASHBOARD: i32 = 0;

/// UI 告诉 worker「此刻该轮询什么」。
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum PollForeground {
    /// 非 Modbus 相关页（设置、固件等）或未连接。
    None,
    /// 主页仪表板（100～149、2011～2012）。
    Dashboard,
    /// Modbus 查询页：仅当前激活标签内的查询项。
    ModbusQuery {
        tab_index: usize,
        slave_id: u8,
        items: Vec<QueryPollItemSpec>,
    },
    /// 设备配置页：常用映射或当前灵活分组。
    DeviceConfig {
        group_index: usize,
        builtin: bool,
        slave_id: u8,
        items: Vec<QueryPollItemSpec>,
    },
}

/// 单个查询/配置项在轮询层的描述（与 Slint 模型解耦）。
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct QueryPollItemSpec {
    pub item_index: usize,
    pub register_text: String,
    /// `None` 表示寄存器字符串无法解析，轮询时直接标记失败。
    pub protocol_address: Option<u16>,
    pub register_count: u16,
    pub value_type: QueryValueType,
    pub scale: u32,
    /// 若为 Some，读回只取该 bit（设备配置常用项）。
    pub bit: Option<u8>,
}

#[derive(Clone, Debug)]
pub struct PollPolicy {
    pub foreground: PollForeground,
    /// UI 当前页面（worker 可读，用于连接后默认主页轮询）。
    pub ui_page: i32,
    /// OTA 传输中禁止普通 Modbus 轮询。
    pub ota_busy: bool,
}

impl Default for PollPolicy {
    fn default() -> Self {
        Self {
            foreground: PollForeground::None,
            ui_page: UI_PAGE_DASHBOARD,
            ota_busy: false,
        }
    }
}

pub type SharedPollPolicy = Arc<Mutex<PollPolicy>>;

impl PollPolicy {
    pub fn new_shared() -> SharedPollPolicy {
        Arc::new(Mutex::new(Self::default()))
    }
}

/// 连接就绪且策略仍为「无」时，若 UI 在主页则立即启用主页轮询（不依赖 UI 定时器）。
pub fn ensure_dashboard_poll_if_idle(policy: &SharedPollPolicy) {
    let mut p = policy.lock().expect("poll policy lock");
    if p.ota_busy {
        return;
    }
    if p.foreground != PollForeground::None {
        return;
    }
    if p.ui_page == UI_PAGE_DASHBOARD {
        info!(
            target: "ble_gui::poll",
            "连接就绪，UI 在主页，启用主页轮询",
        );
        p.foreground = PollForeground::Dashboard;
    }
}

/// 解析实际执行的轮询策略（`None` + 主页时回退为 Dashboard）。
pub fn effective_foreground(policy: &SharedPollPolicy) -> PollForeground {
    let p = policy.lock().expect("poll policy lock");
    if p.ota_busy {
        return PollForeground::None;
    }
    if p.foreground != PollForeground::None {
        return p.foreground.clone();
    }
    if p.ui_page == UI_PAGE_DASHBOARD {
        return PollForeground::Dashboard;
    }
    PollForeground::None
}

/// 日志用：描述当前前台轮询目标。
pub fn describe_poll_foreground(f: &PollForeground) -> String {
    match f {
        PollForeground::None => "无（停止轮询）".into(),
        PollForeground::Dashboard => "主页 · 100～149 + 2011～2012".into(),
        PollForeground::ModbusQuery {
            tab_index,
            slave_id,
            items,
        } => describe_items("Modbus 查询", *tab_index, *slave_id, items),
        PollForeground::DeviceConfig {
            group_index,
            builtin,
            slave_id,
            items,
        } => {
            let kind = if *builtin { "设备配置·常用" } else { "设备配置·分组" };
            describe_items(kind, *group_index, *slave_id, items)
        }
    }
}

fn describe_items(label: &str, index: usize, slave_id: u8, items: &[QueryPollItemSpec]) -> String {
    if items.is_empty() {
        return format!("{label} · #{index} · 从站 {slave_id} · （无项）");
    }
    let regs: Vec<String> = items
        .iter()
        .map(|i| {
            let len = if i.register_count > 1 {
                format!("×{}", i.register_count)
            } else {
                String::new()
            };
            let bit = i
                .bit
                .map(|b| format!(".b{b}"))
                .unwrap_or_default();
            if let Some(addr) = i.protocol_address {
                format!("{}→{addr}{bit}{len}", i.register_text)
            } else {
                format!("{}→?{bit}{len}", i.register_text)
            }
        })
        .collect();
    format!(
        "{label} · #{index} · 从站 {slave_id} · [{}]",
        regs.join(", ")
    )
}
