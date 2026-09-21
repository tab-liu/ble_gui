//! BLE 连接参数优化：尽量把间隔压到接近手机 App 的 ~15ms。
//!
//! btleplug 0.11 尚未暴露跨平台的「请求连接参数」API（0.12 也仅 Windows/Android）。
//! 因此这里只在 Windows 上走 WinRT；其它平台由系统协商，不做额外分支。
//!
//! Windows 上 `FromBluetoothAddressAsync` 会再拿一份 `BluetoothLEDevice`。
//! 必须 [`Close`](https://learn.microsoft.com/windows/uwp/devices-sensors/gatt-client)
//! 才能让系统把引用计数减掉，否则点断开后 ACL 仍可能挂到进程退出。

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
        request: BluetoothLEPreferredConnectionParametersRequest,
        device: BluetoothLEDevice,
    }

    impl Drop for ThroughputHold {
        fn drop(&mut self) {
            // 先放连接参数请求，再关这份额外的 LE 设备句柄。
            if let Err(err) = self.request.Close() {
                log::debug!(
                    target: "ble_gui::conn_opt",
                    "关闭连接参数请求失败: {err}",
                );
            }
            if let Err(err) = self.device.Close() {
                log::debug!(
                    target: "ble_gui::conn_opt",
                    "关闭吞吐优化用 BluetoothLEDevice 失败: {err}",
                );
            }
        }
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

        let params = match BluetoothLEPreferredConnectionParameters::ThroughputOptimized() {
            Ok(p) => p,
            Err(_) => {
                close_le_device(&device);
                return None;
            }
        };
        let request = match device.RequestPreferredConnectionParameters(&params) {
            Ok(req) => req,
            Err(err) => {
                warn!(
                    target: "ble_gui::conn_opt",
                    "Windows 不支持请求吞吐优先连接参数（常见于 Win10）: {err}",
                );
                close_le_device(&device);
                return None;
            }
        };
        let status = match request.Status() {
            Ok(s) => s,
            Err(_) => {
                close_hold_parts(&request, &device);
                return None;
            }
        };
        info!(
            target: "ble_gui::conn_opt",
            "Windows 吞吐优先连接参数 status={}",
            status_name(status),
        );
        log_connection_params(&device, "请求吞吐后");
        if status == BluetoothLEPreferredConnectionParametersRequestStatus::Success
            || status == BluetoothLEPreferredConnectionParametersRequestStatus::Unspecified
        {
            Some(ThroughputHold { request, device })
        } else {
            close_hold_parts(&request, &device);
            None
        }
    }

    fn close_le_device(device: &BluetoothLEDevice) {
        let _ = device.Close();
    }

    fn close_hold_parts(
        request: &BluetoothLEPreferredConnectionParametersRequest,
        device: &BluetoothLEDevice,
    ) {
        let _ = request.Close();
        let _ = device.Close();
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
