//! Windows BLE 连接参数：尽量把间隔压到接近手机 App 的 15ms。
//!
//! `RequestPreferredConnectionParameters` 在 Windows 11 可用；Win10 会失败并忽略。
//! 返回值必须一直拿着，丢掉后系统可能把连接参数改回去。

#[cfg(windows)]
mod imp {
    use std::future::IntoFuture;

    use btleplug::api::BDAddr;
    use log::{info, warn};
    use windows::Devices::Bluetooth::{
        BluetoothLEDevice, BluetoothLEPreferredConnectionParameters,
        BluetoothLEPreferredConnectionParametersRequest,
        BluetoothLEPreferredConnectionParametersRequestStatus,
    };

    pub struct WinThroughputHold {
        _request: BluetoothLEPreferredConnectionParametersRequest,
    }

    pub async fn request_throughput(address: &str) -> Option<WinThroughputHold> {
        let bd: BDAddr = address.parse().ok()?;
        let u64_addr: u64 = bd.into();
        let device = BluetoothLEDevice::FromBluetoothAddressAsync(u64_addr)
            .ok()?
            .into_future()
            .await
            .ok()?;

        log_connection_params(&device, "请求吞吐前");

        let params = BluetoothLEPreferredConnectionParameters::ThroughputOptimized().ok()?;
        let request = match device.RequestPreferredConnectionParameters(&params) {
            Ok(req) => req,
            Err(err) => {
                warn!(
                    target: "ble_gui::ota",
                    "Windows 不支持请求吞吐优先连接参数（常见于 Win10）: {err}",
                );
                return None;
            }
        };
        let status = request.Status().ok()?;
        info!(
            target: "ble_gui::ota",
            "Windows 吞吐优先连接参数 status={}",
            status_name(status),
        );
        log_connection_params(&device, "请求吞吐后");
        if status == BluetoothLEPreferredConnectionParametersRequestStatus::Success
            || status == BluetoothLEPreferredConnectionParametersRequestStatus::Unspecified
        {
            Some(WinThroughputHold { _request: request })
        } else {
            None
        }
    }

    fn log_connection_params(device: &BluetoothLEDevice, when: &str) {
        match device.GetConnectionParameters() {
            Ok(p) => {
                let interval = p.ConnectionInterval().unwrap_or(0);
                let latency = p.ConnectionLatency().unwrap_or(0);
                let timeout = p.LinkTimeout().unwrap_or(0);
                info!(
                    target: "ble_gui::ota",
                    "BLE 连接参数[{when}]: interval={interval} ({:.1}ms) latency={latency} timeout={timeout}",
                    interval as f32 * 1.25,
                );
            }
            Err(err) => {
                info!(
                    target: "ble_gui::ota",
                    "无法读取 BLE 连接参数[{when}]: {err}",
                );
            }
        }
    }

    fn status_name(status: BluetoothLEPreferredConnectionParametersRequestStatus) -> &'static str {
        match status {
            BluetoothLEPreferredConnectionParametersRequestStatus::Success => "Success",
            BluetoothLEPreferredConnectionParametersRequestStatus::DeviceNotAvailable => {
                "DeviceNotAvailable"
            }
            BluetoothLEPreferredConnectionParametersRequestStatus::AccessDenied => "AccessDenied",
            BluetoothLEPreferredConnectionParametersRequestStatus::Unspecified => "Unspecified",
            _ => "Other",
        }
    }
}

#[cfg(windows)]
pub use imp::{request_throughput, WinThroughputHold};

#[cfg(not(windows))]
pub struct WinThroughputHold;

#[cfg(not(windows))]
pub async fn request_throughput(_address: &str) -> Option<WinThroughputHold> {
    None
}
