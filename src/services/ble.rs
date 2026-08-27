//! 蓝牙连接服务（当前为模拟实现，后续替换为真实 BLE 库）。

use std::cell::RefCell;
use std::rc::Rc;

#[derive(Clone, Debug)]
pub struct BleScanEntry {
    pub name: String,
    pub address: String,
    pub rssi: i32,
}

impl BleScanEntry {
    pub fn rssi_text(&self) -> String {
        format!("{} dBm", self.rssi)
    }
}

#[derive(Clone, Debug)]
pub struct BleSnapshot {
    pub connected: bool,
    pub scanning: bool,
    pub status_text: String,
    pub device_name: String,
    pub rssi_text: String,
    pub action_text: String,
    pub scan_devices: Vec<BleScanEntry>,
}

#[derive(Clone)]
pub struct BleService {
    inner: Rc<RefCell<BleInner>>,
}

struct BleInner {
    connected: bool,
    scanning: bool,
    device_name: String,
    device_address: String,
    rssi: i32,
    scan_devices: Vec<BleScanEntry>,
}

impl BleService {
    pub fn new() -> Self {
        Self {
            inner: Rc::new(RefCell::new(BleInner {
                connected: false,
                scanning: false,
                device_name: String::new(),
                device_address: String::new(),
                rssi: 0,
                scan_devices: Vec::new(),
            })),
        }
    }

    pub fn snapshot(&self) -> BleSnapshot {
        let inner = self.inner.borrow();
        BleSnapshot {
            connected: inner.connected,
            scanning: inner.scanning,
            status_text: if inner.connected {
                "已连接".into()
            } else if inner.scanning {
                "扫描中".into()
            } else {
                "未连接".into()
            },
            device_name: inner.device_name.clone(),
            rssi_text: if inner.connected {
                format!("{} dBm", inner.rssi)
            } else {
                String::new()
            },
            action_text: if inner.connected {
                "断开".into()
            } else {
                "扫描设备".into()
            },
            scan_devices: inner.scan_devices.clone(),
        }
    }

    pub fn start_scan(&self) {
        let mut inner = self.inner.borrow_mut();
        inner.scanning = true;
        inner.scan_devices = vec![
            scan_entry("Modbus-Gateway-A1", "AA:BB:CC:DD:EE:01", -58),
            scan_entry("Modbus-Gateway-B2", "AA:BB:CC:DD:EE:02", -62),
            scan_entry("Modbus-Gateway-C3", "AA:BB:CC:DD:EE:03", -65),
            scan_entry("Temp-Sensor-01", "AA:BB:CC:DD:EE:04", -68),
            scan_entry("Temp-Sensor-02", "AA:BB:CC:DD:EE:05", -70),
            scan_entry("BLE-Scale-Office", "AA:BB:CC:DD:EE:06", -72),
            scan_entry("BLE-Scale-Warehouse", "AA:BB:CC:DD:EE:07", -74),
            scan_entry("Modbus-Gateway-D4", "AA:BB:CC:DD:EE:08", -76),
            scan_entry("Modbus-Gateway-E5", "AA:BB:CC:DD:EE:09", -78),
            scan_entry("Unknown-Device", "AA:BB:CC:DD:EE:0A", -80),
        ];
        inner.scanning = false;
    }

    pub fn connect(&self, address: &str) -> bool {
        let device = self
            .inner
            .borrow()
            .scan_devices
            .iter()
            .find(|d| d.address == address)
            .cloned();
        if let Some(dev) = device {
            let mut inner = self.inner.borrow_mut();
            inner.connected = true;
            inner.device_name = dev.name;
            inner.device_address = dev.address;
            inner.rssi = dev.rssi;
            inner.scanning = false;
            true
        } else {
            false
        }
    }

    pub fn disconnect(&self) {
        let mut inner = self.inner.borrow_mut();
        inner.connected = false;
        inner.device_name.clear();
        inner.device_address.clear();
        inner.rssi = 0;
    }

    pub fn is_connected(&self) -> bool {
        self.inner.borrow().connected
    }
}

fn scan_entry(name: &str, address: &str, rssi: i32) -> BleScanEntry {
    BleScanEntry {
        name: name.into(),
        address: address.into(),
        rssi,
    }
}
