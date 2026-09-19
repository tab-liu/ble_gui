//! BLE 连接参数优化：尽量把间隔压到接近手机 App 的 ~15ms。
//!
//! btleplug 0.11 尚未暴露跨平台的「请求连接参数」API（0.12 也仅 Windows/Android）。
//! 因此这里只在 Windows 上走 WinRT；其它平台由系统协商，不做额外分支。

#[cfg(windows)]
mod windows_imp {
    use std::future::IntoFuture;

    use btleplug::api::BDAddr;
    use log::{info, warn};
    use windows::Devices::Bluetooth::{
        BluetoothLEDevice, BluetoothLEPreferredConnectionParameters,
        BluetoothLEPreferredConnectionParametersRequest,
        BluetoothLEPreferredConnectionParametersRequestStatus,
    };

    pub struct ThroughputHold {
        _request: BluetoothLEPreferredConnectionParametersRequest,
    }

    pub async fn request_throughput(address: &str) -> Option<ThroughputHold> {
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
                    target: "ble_gui::conn_opt",
                    "Windows 不支持请求吞吐优先连接参数（常见于 Win10）: {err}",
                );
                return None;
            }
        };
        let status = request.Status().ok()?;
        info!(
            target: "ble_gui::conn_opt",
            "Windows 吞吐优先连接参数 status={}",
            status_name(status),
        );
        log_connection_params(&device, "请求吞吐后");
        if status == BluetoothLEPreferredConnectionParametersRequestStatus::Success
            || status == BluetoothLEPreferredConnectionParametersRequestStatus::Unspecified
        {
            Some(ThroughputHold { _request: request })
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
                    target: "ble_gui::conn_opt",
                    "BLE 连接参数[{when}]: interval={interval} ({:.1}ms) latency={latency} timeout={timeout}",
                    interval as f32 * 1.25,
                );
            }
            Err(err) => {
                info!(
                    target: "ble_gui::conn_opt",
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
pub use windows_imp::{request_throughput, ThroughputHold};

#[cfg(not(windows))]
pub struct ThroughputHold;

#[cfg(not(windows))]
pub async fn request_throughput(_address: &str) -> Option<ThroughputHold> {
    None
}
