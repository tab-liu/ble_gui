//! 蓝牙 UI 状态（主线程读取，worker 写入）。

use std::sync::{Arc, Mutex};

/// 扫描时从厂商广播推断的占用提示。
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ScanLinkHint {
    #[default]
    Unknown,
    Available,
    Occupied,
}

#[derive(Clone, Debug)]
pub struct BleScanEntry {
    pub name: String,
    pub address: String,
    /// 当前信号强度，随广播实时更新。
    pub rssi: i32,
    /// 广播中包含 FF00 服务，判定为目标 BLUETTI 设备。
    pub is_target: bool,
    pub link_hint: ScanLinkHint,
}

impl BleScanEntry {
    pub fn rssi_text(&self) -> String {
        format!("{} dBm", self.rssi)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LinkPhase {
    Idle,
    Scanning,
    Connecting,
    GattReady,
    Handshake,
    Encrypted,
}

#[derive(Clone, Debug)]
pub struct BleSnapshot {
    pub connected: bool,
    pub scanning: bool,
    pub connecting: bool,
    pub status_text: String,
    pub device_name: String,
    pub device_address: String,
    pub rssi_text: String,
    pub action_text: String,
    pub scan_devices: Vec<BleScanEntry>,
    pub scan_list_generation: u64,
    pub encryption_ready: bool,
}

pub struct BleInner {
    pub phase: LinkPhase,
    pub device_name: String,
    pub device_address: String,
    pub rssi: i32,
    pub scan_devices: Vec<BleScanEntry>,
    pub scan_list_generation: u64,
    pub status_detail: String,
    pub encryption_ready: bool,
}

impl Default for BleInner {
    fn default() -> Self {
        Self {
            phase: LinkPhase::Idle,
            device_name: String::new(),
            device_address: String::new(),
            rssi: 0,
            scan_devices: Vec::new(),
            scan_list_generation: 0,
            status_detail: String::new(),
            encryption_ready: false,
        }
    }
}

impl BleInner {
    pub fn upsert_advertisement(
        &mut self,
        name: &str,
        address: &str,
        rssi: i32,
        is_target: bool,
        link_hint: ScanLinkHint,
    ) {
        if !is_target {
            return;
        }
        let name = name.trim();
        if let Some(existing) = self
            .scan_devices
            .iter_mut()
            .find(|d| crate::services::ble_favorites::addresses_equal(&d.address, address))
        {
            let mut changed = existing.rssi != rssi || existing.link_hint != link_hint;
            existing.rssi = rssi;
            existing.link_hint = link_hint;
            if is_target {
                existing.is_target = true;
            }
            if !name.is_empty() && existing.name != name {
                existing.name = name.to_string();
                changed = true;
            }
            if changed {
                self.scan_list_generation += 1;
            }
        } else {
            let display_name = if !name.is_empty() {
                name.to_string()
            } else {
                placeholder_name(address)
            };
            self.scan_devices.push(BleScanEntry {
                name: display_name,
                address: address.to_string(),
                rssi,
                is_target,
                link_hint,
            });
            self.scan_list_generation += 1;
        }
    }

    pub fn snapshot(&self) -> BleSnapshot {
        let scanning = self.phase == LinkPhase::Scanning;
        let connecting = self.phase == LinkPhase::Connecting;
        let connected = matches!(
            self.phase,
            LinkPhase::GattReady | LinkPhase::Handshake | LinkPhase::Encrypted
        );

        let status_text = match self.phase {
            LinkPhase::Idle if connected => "已连接".into(),
            LinkPhase::Idle if !self.status_detail.is_empty() => self.status_detail.clone(),
            LinkPhase::Idle => "未连接".into(),
            LinkPhase::Scanning => "扫描中".into(),
            LinkPhase::Connecting => {
                if self.status_detail.is_empty() {
                    "连接中".into()
                } else {
                    self.status_detail.clone()
                }
            }
            LinkPhase::GattReady => "已连接".into(),
            LinkPhase::Handshake => "鉴权中".into(),
            LinkPhase::Encrypted => "已连接（加密）".into(),
        };

        BleSnapshot {
            connected,
            scanning,
            connecting,
            status_text,
            device_name: self.device_name.clone(),
            device_address: self.device_address.clone(),
            rssi_text: if connected {
                format_connected_rssi(self.rssi)
            } else {
                String::new()
            },
            action_text: if connected {
                "断开".into()
            } else if connecting {
                "取消".into()
            } else if scanning {
                "结束扫描".into()
            } else {
                "扫描设备".into()
            },
            scan_devices: self.scan_devices.clone(),
            scan_list_generation: self.scan_list_generation,
            encryption_ready: self.encryption_ready,
        }
    }
}

pub type SharedBleState = Arc<Mutex<BleInner>>;

fn placeholder_name(address: &str) -> String {
    format!("蓝牙设备 ({address})")
}

/// 连接后 GATT 往往不再更新广播 RSSI；0 / ≤-100 视为未知，避免画出假的 -100 dBm。
fn format_connected_rssi(rssi: i32) -> String {
    if rssi == 0 || rssi <= -100 {
        "—".into()
    } else {
        format!("{rssi} dBm")
    }
}

/// 尚未解析到广播名称时的占位显示名。
pub fn is_placeholder_scan_name(name: &str) -> bool {
    name.starts_with("蓝牙设备 (") && name.ends_with(')')
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn upsert_merges_same_mac_with_different_separators() {
        let mut inner = BleInner::default();
        inner.upsert_advertisement("A", "aa-bb-cc-dd-ee-ff", -60, true, ScanLinkHint::Available);
        inner.upsert_advertisement("B", "AA:BB:CC:DD:EE:FF", -55, true, ScanLinkHint::Available);
        assert_eq!(inner.scan_devices.len(), 1);
        assert_eq!(inner.scan_devices[0].name, "B");
        assert_eq!(inner.scan_devices[0].rssi, -55);
    }

    #[test]
    fn connected_rssi_hides_placeholder() {
        assert_eq!(format_connected_rssi(0), "—");
        assert_eq!(format_connected_rssi(-100), "—");
        assert_eq!(format_connected_rssi(-67), "-67 dBm");
    }
}
