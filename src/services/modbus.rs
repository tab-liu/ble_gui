//! Modbus 主页数据（寄存器 100～149 实时数据、2011/2012 输出控制）。
//! 实时值由 BLE worker 写入 `SharedModbusLive`，UI 从本服务读取。

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
}

pub type SharedModbusLive = Arc<Mutex<ModbusLive>>;

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
    pub tab_index: usize,
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

    pub fn mark_query_poll_updated(&self) {
        self.inner
            .borrow()
            .query_poll_generation
            .fetch_add(1, Ordering::Release);
    }

    pub fn query_snapshot(&self) -> QueryPollSnapshot {
        self.inner
            .borrow()
            .query_live
            .lock()
            .map(|s| s.clone())
            .unwrap_or_default()
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

    pub fn apply_control_state(&self, ac_on: bool, dc_on: bool) {
        if let Ok(mut live) = self.inner.borrow().live.lock() {
            live.dashboard.ac_output_on = ac_on;
            live.dashboard.dc_output_on = dc_on;
        }
    }
}
