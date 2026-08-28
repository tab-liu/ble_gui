//! Windows WinRT 设备名称解析（参考 C# BluetoothLEDevice.FromBluetoothAddressAsync）。

#[cfg(windows)]
pub async fn resolve_device_name_from_address(address: &str) -> Option<String> {
    use std::future::IntoFuture;

    use btleplug::api::BDAddr;
    use windows::Devices::Bluetooth::BluetoothLEDevice;

    let bd: BDAddr = address.parse().ok()?;
    let u64_addr: u64 = bd.into();
    let async_op = BluetoothLEDevice::FromBluetoothAddressAsync(u64_addr).ok()?;
    let device = async_op.into_future().await.ok()?;
    let name = device.Name().ok()?.to_string();
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
