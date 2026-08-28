//! BLUETTI BLE GATT UUID（与 ref/tool/BLUETTI_BLE_Bridge.cs 一致）。

use uuid::Uuid;

pub fn service_uuid() -> Uuid {
    Uuid::parse_str("0000ff00-0000-1000-8000-00805f9b34fb").expect("service uuid")
}

pub fn write_uuid() -> Uuid {
    Uuid::parse_str("0000ff02-0000-1000-8000-00805f9b34fb").expect("write uuid")
}

pub fn notify_uuid() -> Uuid {
    Uuid::parse_str("0000ff01-0000-1000-8000-00805f9b34fb").expect("notify uuid")
}

pub fn notify_uuid_ff03() -> Uuid {
    Uuid::parse_str("0000ff03-0000-1000-8000-00805f9b34fb").expect("notify ff03 uuid")
}
