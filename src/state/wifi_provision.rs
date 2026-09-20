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
    /// 空会话，不读本机配置目录。
    pub fn blank() -> Self {
        Self {
            saved: Rc::new(VecModel::from(Vec::<WifiSavedNetwork>::new())),
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

    pub fn load() -> Self {
        let saved: Vec<WifiSavedNetwork> = wifi_cred_store::load()
            .into_iter()
            .map(|n| WifiSavedNetwork {
                ssid: n.ssid.into(),
                password: n.password.into(),
            })
            .collect();
        let mut state = Self::blank();
        state.saved = Rc::new(VecModel::from(saved));
        state
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::services::modbus::QueryItemPollResult;

    fn poll(index: usize, result: &str) -> QueryItemPollResult {
        QueryItemPollResult {
            item_index: index,
            status: String::new(),
            result: result.into(),
            ok: true,
        }
    }

    fn expired(timeout: Duration) -> Instant {
        Instant::now()
            .checked_sub(timeout + Duration::from_secs(1))
            .expect("instant")
    }

    #[test]
    fn matching_ssid_advances_wifi_to_cloud() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingWifi;
        st.phase_since = Some(Instant::now());
        st.pending_ssid = " Home ".into();
        st.wifi_sta = true;
        st.ssid_now = "Home".into();
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::ConnectingCloud);
        assert!(st.hint.is_empty());
    }

    #[test]
    fn empty_pending_ssid_still_advances_when_sta_up() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingWifi;
        st.phase_since = Some(Instant::now());
        st.wifi_sta = true;
        st.ssid_now = "Other".into();
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::ConnectingCloud);
    }

    #[test]
    fn mismatched_ssid_does_not_advance() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingWifi;
        st.phase_since = Some(Instant::now());
        st.pending_ssid = "Home".into();
        st.wifi_sta = true;
        st.ssid_now = "Other".into();
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::ConnectingWifi);
    }

    #[test]
    fn wifi_timeout_fails_with_disconnect_hint() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingWifi;
        st.phase_since = Some(expired(WIFI_CONNECT_TIMEOUT));
        st.pending_ssid = "Home".into();
        st.disconnect_reason = 201;
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::Failed);
        assert_eq!(st.hint, "未找到该 WiFi");
    }

    #[test]
    fn wifi_timeout_without_reason_uses_generic_hint() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingWifi;
        st.phase_since = Some(expired(WIFI_CONNECT_TIMEOUT));
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::Failed);
        assert_eq!(st.hint, "WiFi 连接超时");
    }

    #[test]
    fn mqtt_ok_completes_cloud_phase() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingCloud;
        st.phase_since = Some(Instant::now());
        st.pending_ssid = "Home".into();
        st.wifi_sta = true;
        st.mqtt = true;
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::Success);
        assert!(st.pending_ssid.is_empty());
        assert!(st.phase_since.is_none());
    }

    #[test]
    fn cloud_phase_drops_back_when_wifi_lost() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingCloud;
        st.phase_since = Some(Instant::now());
        st.wifi_sta = false;
        st.mqtt = false;
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::ConnectingWifi);
        assert!(st.phase_since.is_some());
    }

    #[test]
    fn cloud_timeout_fails() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingCloud;
        st.phase_since = Some(expired(CLOUD_CONNECT_TIMEOUT));
        st.wifi_sta = true;
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::Failed);
        assert_eq!(st.hint, "云端连接超时");
    }

    #[test]
    fn failed_recovers_when_link_and_mqtt_are_up() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::Failed;
        st.hint = "WiFi 连接超时".into();
        st.wifi_sta = true;
        st.mqtt = true;
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::Success);
        assert!(st.hint.is_empty());
    }

    #[test]
    fn idle_marks_success_when_already_linked() {
        let mut st = WifiProvisionUiState::blank();
        st.wifi_sta = true;
        st.mqtt = true;
        st.advance(true);
        assert_eq!(st.phase, WifiProvisionPhase::Success);
    }

    #[test]
    fn disconnected_does_not_advance() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::ConnectingWifi;
        st.wifi_sta = true;
        st.ssid_now = "Home".into();
        st.pending_ssid = "Home".into();
        st.advance(false);
        assert_eq!(st.phase, WifiProvisionPhase::ConnectingWifi);
    }

    #[test]
    fn reset_on_disconnect_clears_session() {
        let mut st = WifiProvisionUiState::blank();
        st.phase = WifiProvisionPhase::Success;
        st.phase_since = Some(Instant::now());
        st.pending_ssid = "Home".into();
        st.wifi_sta = true;
        st.mqtt = true;
        st.ssid_now = "Home".into();
        st.sta_ip = "192.168.1.1".into();
        st.hint = "ok".into();
        st.reset_on_disconnect();
        assert_eq!(st.phase, WifiProvisionPhase::Idle);
        assert!(st.phase_since.is_none());
        assert!(st.pending_ssid.is_empty());
        assert!(!st.wifi_sta);
        assert!(!st.mqtt);
        assert!(st.ssid_now.is_empty());
        assert!(st.sta_ip.is_empty());
        assert!(st.hint.is_empty());
    }

    #[test]
    fn apply_poll_item_ignores_failed_read() {
        let mut st = WifiProvisionUiState::blank();
        st.apply_poll_item(&QueryItemPollResult {
            item_index: WIFI_POLL_LINK,
            status: String::new(),
            result: "65".into(),
            ok: false,
        });
        assert!(!st.wifi_sta);
        assert!(!st.mqtt);
    }

    #[test]
    fn apply_poll_item_parses_link_ssid_ip_and_reason() {
        let mut st = WifiProvisionUiState::blank();
        st.apply_poll_item(&poll(WIFI_POLL_LINK, "65"));
        st.apply_poll_item(&poll(WIFI_POLL_SSID_NOW, "  Home  "));
        st.apply_poll_item(&poll(WIFI_POLL_STA_IP, "16885952"));
        st.apply_poll_item(&poll(WIFI_POLL_DISCONNECT, "202"));
        assert!(st.wifi_sta);
        assert!(st.mqtt);
        assert_eq!(st.ssid_now, "Home");
        assert_eq!(st.sta_ip, "192.168.1.1");
        assert_eq!(st.disconnect_reason, 202);
    }

    #[test]
    fn ssid_match_ignores_padding_and_rejects_empty() {
        assert!(ssid_matches(" Home ", "Home"));
        assert!(!ssid_matches("", "Home"));
        assert!(!ssid_matches("Home", "Office"));
    }
}
