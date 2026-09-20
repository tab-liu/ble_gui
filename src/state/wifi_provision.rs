//! WiFi 配网会话状态（与配置页 UI 解耦的数据与相位推进）。

use std::rc::Rc;
use std::time::{Duration, Instant};

use slint::VecModel;

use crate::services::ble::modbus::{
    disconnect_reason_text, parse_disconnect_reason, parse_link_status, parse_sta_ipv4,
    WIFI_POLL_DISCONNECT, WIFI_POLL_LINK, WIFI_POLL_SSID_NOW, WIFI_POLL_STA_IP,
};
use crate::services::modbus::QueryItemPollResult;
use crate::services::wifi_cred_store;
use crate::ui::{WifiSavedNetwork, WifiScanAp};

pub const WIFI_CONNECT_TIMEOUT: Duration = Duration::from_secs(45);
pub const CLOUD_CONNECT_TIMEOUT: Duration = Duration::from_secs(60);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WifiProvisionPhase {
    Idle,
    ConnectingWifi,
    ConnectingCloud,
    Success,
    Failed,
}

pub struct WifiProvisionUiState {
    pub saved: Rc<VecModel<WifiSavedNetwork>>,
    pub scan: Rc<VecModel<WifiScanAp>>,
    pub phase: WifiProvisionPhase,
    pub phase_since: Option<Instant>,
    pub pending_ssid: String,
    pub wifi_sta: bool,
    pub mqtt: bool,
    pub ssid_now: String,
    pub sta_ip: String,
    pub disconnect_reason: u16,
    pub hint: String,
}

impl WifiProvisionUiState {
    pub fn load() -> Self {
        let saved: Vec<WifiSavedNetwork> = wifi_cred_store::load()
            .into_iter()
            .map(|n| WifiSavedNetwork {
                ssid: n.ssid.into(),
                password: n.password.into(),
            })
            .collect();
        Self {
            saved: Rc::new(VecModel::from(saved)),
            scan: Rc::new(VecModel::from(Vec::<WifiScanAp>::new())),
            phase: WifiProvisionPhase::Idle,
            phase_since: None,
            pending_ssid: String::new(),
            wifi_sta: false,
            mqtt: false,
            ssid_now: String::new(),
            sta_ip: String::new(),
            disconnect_reason: 0,
            hint: String::new(),
        }
    }

    pub fn reset_on_disconnect(&mut self) {
        self.phase = WifiProvisionPhase::Idle;
        self.phase_since = None;
        self.pending_ssid.clear();
        self.wifi_sta = false;
        self.mqtt = false;
        self.ssid_now.clear();
        self.sta_ip.clear();
        self.hint.clear();
    }

    pub fn apply_poll_item(&mut self, r: &QueryItemPollResult) {
        if !r.ok {
            return;
        }
        match r.item_index {
            WIFI_POLL_LINK => {
                let (sta, mqtt) = parse_link_status(&r.result);
                self.wifi_sta = sta;
                self.mqtt = mqtt;
            }
            WIFI_POLL_SSID_NOW => {
                self.ssid_now = r.result.trim().to_string();
            }
            WIFI_POLL_STA_IP => {
                self.sta_ip = parse_sta_ipv4(&r.result);
            }
            WIFI_POLL_DISCONNECT => {
                self.disconnect_reason = parse_disconnect_reason(&r.result);
            }
            _ => {}
        }
    }

    pub fn advance(&mut self, connected: bool) {
        if !connected {
            return;
        }
        let elapsed = self.phase_since.map(|t| t.elapsed());
        match self.phase {
            WifiProvisionPhase::ConnectingWifi => {
                if self.wifi_sta && ssid_matches(&self.pending_ssid, &self.ssid_now) {
                    self.phase = WifiProvisionPhase::ConnectingCloud;
                    self.phase_since = Some(Instant::now());
                    self.hint.clear();
                } else if self.wifi_sta && self.pending_ssid.is_empty() {
                    self.phase = WifiProvisionPhase::ConnectingCloud;
                    self.phase_since = Some(Instant::now());
                } else if elapsed.is_some_and(|d| d >= WIFI_CONNECT_TIMEOUT) {
                    self.phase = WifiProvisionPhase::Failed;
                    self.hint = disconnect_reason_text(self.disconnect_reason)
                        .unwrap_or("WiFi 连接超时")
                        .to_string();
                }
            }
            WifiProvisionPhase::ConnectingCloud => {
                if self.mqtt {
                    self.phase = WifiProvisionPhase::Success;
                    self.phase_since = None;
                    self.pending_ssid.clear();
                    self.hint.clear();
                } else if !self.wifi_sta {
                    self.phase = WifiProvisionPhase::ConnectingWifi;
                    self.phase_since = Some(Instant::now());
                } else if elapsed.is_some_and(|d| d >= CLOUD_CONNECT_TIMEOUT) {
                    self.phase = WifiProvisionPhase::Failed;
                    self.hint = "云端连接超时".into();
                }
            }
            WifiProvisionPhase::Failed => {
                if self.wifi_sta && self.mqtt {
                    self.phase = WifiProvisionPhase::Success;
                    self.hint.clear();
                }
            }
            WifiProvisionPhase::Idle | WifiProvisionPhase::Success => {
                if self.wifi_sta && self.mqtt {
                    self.phase = WifiProvisionPhase::Success;
                }
            }
        }
    }
}

fn ssid_matches(expected: &str, actual: &str) -> bool {
    let expected = expected.trim();
    !expected.is_empty() && expected == actual.trim()
}
