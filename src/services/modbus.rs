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

/// 21000 段上报的组网节点（本机或子设备）。
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SubDevice {
    pub is_self: bool,
    pub slave_addr: u8,
    pub group_addr: u8,
    pub master_addr: u8,
    pub group_same_type_addr: u8,
    pub state: u16,
    pub sn: u64,
    pub dev_type: u16,
}

impl SubDevice {
    pub fn online(&self) -> bool {
        self.state & 0x0001 != 0
    }

    pub fn update_need(&self) -> bool {
        self.state & 0x0002 != 0
    }

    pub fn alarm(&self) -> bool {
        self.state & 0x0004 != 0
    }

    pub fn protect(&self) -> bool {
        self.state & 0x0008 != 0
    }

    pub fn system_on(&self) -> bool {
        self.state & 0x0010 != 0
    }

    pub fn bat_alarm(&self) -> bool {
        self.state & 0x0020 != 0
    }

    pub fn sn_text(&self) -> String {
        if self.sn == 0 {
            String::new()
        } else {
            self.sn.to_string()
        }
    }

    pub fn status_text(&self) -> String {
        let mut parts = Vec::new();
        if self.online() {
            parts.push("在线");
        } else {
            parts.push("离线");
        }
        if self.system_on() {
            parts.push("开机");
        }
        if self.update_need() {
            parts.push("待升级");
        }
        if self.alarm() {
            parts.push("告警");
        }
        if self.protect() {
            parts.push("故障");
        }
        if self.bat_alarm() {
            parts.push("电池告警");
        }
        parts.join(" · ")
    }

    pub fn addr_text(&self) -> String {
        format!(
            "从机 {} · 群组 {} · 主机 {}",
            self.slave_addr, self.group_addr, self.master_addr
        )
    }

    pub fn summary(&self) -> String {
        let role = if self.is_self { "本机" } else { "配件" };
        let sn = self.sn_text();
        if sn.is_empty() {
            format!("{role} 从机{}", self.slave_addr)
        } else {
            format!("{role} {sn}")
        }
    }
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
    /// 连接后读一次的身份属性（不进功率轮询）。
    pub identity_loaded: bool,
    pub iot_type: String,
    pub iot_sn: String,
    pub safe_code: String,
    pub cloud_url: String,
    pub wifi_mac: String,
    pub ble_mac: String,
    pub wifi_password: String,
    /// 主页状态轮询：链路 / 当前 SSID / IP / RSSI。
    pub link_status_valid: bool,
    pub wifi_sta: bool,
    pub mqtt_ok: bool,
    pub ssid_now: String,
    pub sta_ip: String,
    pub sta_rssi: i16,
    /// 21000 段主动上报的组网设备（含本机）。
    pub sub_devices: Vec<SubDevice>,
    pub sub_devices_valid: bool,
}

impl ModbusLive {
    /// 清仪表、身份、链路。保留 `slave_id`（断开后从站号仍有意义）。
    pub fn clear_session(&mut self) {
        let slave_id = self.slave_id;
        *self = Self {
            slave_id,
            ..Self::default()
        };
    }
}

#[cfg(test)]
mod live_tests {
    use super::*;

    #[test]
    fn clear_session_keeps_slave_id() {
        let mut live = ModbusLive {
            slave_id: 7,
            modbus_online: true,
            device_sn: "x".into(),
            ..ModbusLive::default()
        };
        live.clear_session();
        assert_eq!(live.slave_id, 7);
        assert!(!live.modbus_online);
        assert!(live.device_sn.is_empty());
    }
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
            live.clear_session();
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

    /// 配网下发成功后立刻把密码写进主页快照，不必等下次重连。
    pub fn remember_wifi_credentials(&self, password: &str) {
        if let Ok(mut live) = self.inner.borrow().live.lock() {
            live.wifi_password = password.to_string();
        }
    }
}
