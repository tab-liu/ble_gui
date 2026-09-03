//! Modbus 主页数据与查询轮询共享态。
//!
//! - [`ModbusLive`]：仪表板 SOC/功率、AC/DC 开关、读模式（标准/TLV）  
//! - [`QueryPollSnapshot`]：查询页 **与** 设备配置页共用的单项读回结果  
//!   （通过 [`QueryPollTarget`] 区分归属，避免串页）
//!
//! BLE worker 写入上述 `Arc<Mutex<_>>`；UI 定时器读 generation 后合并进 Slint。

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ModbusReadMode {
    #[default]
    Unknown,
    Standard,
    Tlv,
}

impl ModbusReadMode {
    pub fn label(self) -> &'static str {
        match self {
            Self::Unknown => "检测中",
            Self::Standard => "常规",
            Self::Tlv => "TLV",
        }
    }
}

#[derive(Clone, Debug, Default)]
pub struct DashboardData {
    pub soc: i32,
    pub ac_output_w: i32,
    pub dc_output_w: i32,
    pub pv_input_w: i32,
    pub ac_input_w: i32,
    pub data_valid: bool,
    pub ac_output_on: bool,
    pub dc_output_on: bool,
}

/// BLE worker 与 UI 共享的 Modbus 实时状态。
#[derive(Clone, Debug, Default)]
pub struct ModbusLive {
    pub dashboard: DashboardData,
    pub output_busy: bool,
    pub slave_id: u8,
    pub modbus_online: bool,
    pub read_mode: ModbusReadMode,
    pub capabilities_probed: bool,
    /// 连接后读一次 1100 段得到的版本摘要，周期轮询不再刷新。
    pub device_info_loaded: bool,
    pub device_type: String,
    pub device_sn: String,
    pub device_versions_text: String,
    pub iot_software_version: Option<u32>,
    /// 1100 段软件类型与版本（含 IOT 合并后的 11000 料号）。
    pub device_software: Vec<(u16, u32)>,
}

pub type SharedModbusLive = Arc<Mutex<ModbusLive>>;

/// Modbus 查询页 / 设备配置页轮询结果目标（共用一份 live，前台页互斥）。
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub enum QueryPollTarget {
    #[default]
    None,
    ModbusQuery {
        tab_index: usize,
    },
    DeviceConfig {
        group_index: usize,
        builtin: bool,
    },
}

/// Modbus 查询页单个卡片轮询结果（worker 写入，UI 合并到 Slint 模型）。
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct QueryItemPollResult {
    pub item_index: usize,
    pub status: String,
    pub result: String,
    pub ok: bool,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct QueryPollSnapshot {
    pub target: QueryPollTarget,
    pub items: Vec<QueryItemPollResult>,
}

pub type SharedQueryPollLive = Arc<Mutex<QueryPollSnapshot>>;

struct ModbusInner {
    live: SharedModbusLive,
    query_live: SharedQueryPollLive,
    /// worker 写入 query_live 后递增，UI 定时器据此触发合并。
    query_poll_generation: Arc<AtomicU64>,
    session_active: bool,
}

#[derive(Clone)]
pub struct ModbusService {
    inner: Rc<RefCell<ModbusInner>>,
}

impl ModbusService {
    pub fn new() -> Self {
        Self {
            inner: Rc::new(RefCell::new(ModbusInner {
                live: Arc::new(Mutex::new(ModbusLive::default())),
                query_live: Arc::new(Mutex::new(QueryPollSnapshot::default())),
                query_poll_generation: Arc::new(AtomicU64::new(0)),
                session_active: false,
            })),
        }
    }

    pub fn shared_live(&self) -> SharedModbusLive {
        self.inner.borrow().live.clone()
    }

    pub fn shared_query_live(&self) -> SharedQueryPollLive {
        self.inner.borrow().query_live.clone()
    }

    pub fn shared_query_poll_generation(&self) -> Arc<AtomicU64> {
        self.inner.borrow().query_poll_generation.clone()
    }

    pub fn query_poll_generation(&self) -> u64 {
        self.inner
            .borrow()
            .query_poll_generation
            .load(Ordering::Acquire)
    }

    pub fn on_connected(&self) {
        let mut inner = self.inner.borrow_mut();
        inner.session_active = true;
    }

    pub fn on_disconnected(&self) {
        let mut inner = self.inner.borrow_mut();
        inner.session_active = false;
        if let Ok(mut live) = inner.live.lock() {
            live.dashboard = DashboardData::default();
            live.output_busy = false;
            live.modbus_online = false;
            live.read_mode = ModbusReadMode::Unknown;
            live.capabilities_probed = false;
            live.device_info_loaded = false;
            live.device_type.clear();
            live.device_sn.clear();
            live.device_versions_text.clear();
            live.iot_software_version = None;
            live.device_software.clear();
        }
        if let Ok(mut query) = inner.query_live.lock() {
            *query = QueryPollSnapshot::default();
        }
        inner.query_poll_generation.store(0, Ordering::Release);
    }

    pub fn dashboard_data(&self) -> DashboardData {
        self.inner
            .borrow()
            .live
            .lock()
            .map(|l| l.dashboard.clone())
            .unwrap_or_default()
    }

    pub fn read_mode(&self) -> ModbusReadMode {
        self.inner
            .borrow()
            .live
            .lock()
            .map(|l| l.read_mode)
            .unwrap_or_default()
    }

    pub fn output_busy(&self) -> bool {
        self.inner
            .borrow()
            .live
            .lock()
            .map(|l| l.output_busy)
            .unwrap_or(false)
    }

    pub fn set_output_busy(&self, busy: bool) {
        if let Ok(mut live) = self.inner.borrow().live.lock() {
            live.output_busy = busy;
        }
    }
}
