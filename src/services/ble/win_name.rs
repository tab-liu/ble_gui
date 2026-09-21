//! Windows WinRT 设备名称解析（参考 C# BluetoothLEDevice.FromBluetoothAddressAsync）。
//!
//! `FromBluetoothAddressAsync` 必须在用完后 `Close()`。WinRT 的 Drop 只减 COM
//! 引用，不关 GATT 会话；不 Close 时扫描阶段留下的句柄会把 ACL 一直握到进程退出。

#[cfg(windows)]
pub async fn resolve_device_name_from_address(address: &str) -> Option<String> {
    use std::future::IntoFuture;

    use btleplug::api::BDAddr;
    use windows::Devices::Bluetooth::BluetoothLEDevice;

    let bd: BDAddr = address.parse().ok()?;
    let u64_addr: u64 = bd.into();
    let async_op = BluetoothLEDevice::FromBluetoothAddressAsync(u64_addr).ok()?;
    let device = async_op.into_future().await.ok()?;
    let name = device.Name().ok().map(|n| n.to_string());
    if let Err(err) = device.Close() {
        log::debug!(
            target: "ble_gui::win_name",
            "关闭名称解析用 BluetoothLEDevice 失败: {err}",
        );
    }
    let name = name?;
    let name = name.trim();
    if name.is_empty() {
        None
    } else {
        Some(name.to_string())
    }
}

#[cfg(not(windows))]
pub async fn resolve_device_name_from_address(_address: &str) -> Option<String> {
    None
}
