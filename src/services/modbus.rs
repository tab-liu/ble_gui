//! Modbus 主页数据（寄存器 100～149 实时数据、2011/2012 输出控制）。
//! 实时值由 BLE worker 写入 `SharedModbusLive`，UI 从本服务读取。

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

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
}

pub type SharedModbusLive = Arc<Mutex<ModbusLive>>;

struct ModbusInner {
    live: SharedModbusLive,
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
                session_active: false,
            })),
        }
    }

    pub fn shared_live(&self) -> SharedModbusLive {
        self.inner.borrow().live.clone()
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
        }
    }

    pub fn dashboard_data(&self) -> DashboardData {
        self.inner
            .borrow()
            .live
            .lock()
            .map(|l| l.dashboard.clone())
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
