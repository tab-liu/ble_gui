//! 查询 Windows 系统蓝牙开关。
//!
//! 蓝牙关闭时 WinRT 仍可能枚举到适配器，btleplug 的 `adapters()` 会成功，
//! 必须额外读 Radio.State，否则界面会一直停在「扫描中 / 未连接」。

use log::info;

pub const RADIO_OFF_HINT: &str = "系统蓝牙已关闭，请打开后再点扫描";
pub const RADIO_DISABLED_HINT: &str = "系统蓝牙已禁用，请在 Windows 设置中启用蓝牙";
pub const NO_ADAPTER_HINT: &str = "未找到蓝牙适配器，请打开系统蓝牙后再点扫描";

pub fn is_radio_unavailable_detail(detail: &str) -> bool {
    detail.contains("系统蓝牙")
        || detail.contains("未找到蓝牙适配器")
        || detail.contains("蓝牙不可用")
}

#[cfg(windows)]
pub async fn radio_unavailable_reason() -> Option<String> {
    if let Some(reason) = radios_collection_reason().await {
        return Some(reason);
    }
    adapter_radio_reason().await
}

#[cfg(windows)]
async fn radios_collection_reason() -> Option<String> {
    use std::future::IntoFuture;

    use windows::Devices::Radios::{Radio, RadioKind, RadioState};

    let radios = Radio::GetRadiosAsync().ok()?.into_future().await.ok()?;
    let mut saw_bt = false;
    let mut any_on = false;
    let mut any_off = false;
    let mut any_disabled = false;

    for radio in radios {
        if radio.Kind().ok()? != RadioKind::Bluetooth {
            continue;
        }
        saw_bt = true;
        let state = radio.State().ok()?;
        info!(target: "ble_gui::win_radio", "Radio.GetRadiosAsync Bluetooth state={state:?}");
        if state == RadioState::On {
            any_on = true;
        } else if state == RadioState::Off {
            any_off = true;
        } else if state == RadioState::Disabled {
            any_disabled = true;
        }
    }

    if !saw_bt {
        return None;
    }
    if any_on {
        return None;
    }
    if any_disabled {
        return Some(RADIO_DISABLED_HINT.into());
    }
    if any_off {
        return Some(RADIO_OFF_HINT.into());
    }
    None
}

#[cfg(windows)]
async fn adapter_radio_reason() -> Option<String> {
    use std::future::IntoFuture;

    use windows::Devices::Bluetooth::BluetoothAdapter;
    use windows::Devices::Radios::RadioState;

    let adapter = match BluetoothAdapter::GetDefaultAsync() {
        Ok(op) => match op.into_future().await {
            Ok(adapter) => adapter,
            Err(_) => return Some(NO_ADAPTER_HINT.into()),
        },
        Err(_) => return Some(NO_ADAPTER_HINT.into()),
    };

    let radio = match adapter.GetRadioAsync() {
        Ok(op) => match op.into_future().await {
            Ok(radio) => radio,
            Err(_) => return None,
        },
        Err(_) => return None,
    };

    match radio.State() {
        Ok(state) => {
            info!(target: "ble_gui::win_radio", "BluetoothAdapter radio state={state:?}");
            if state == RadioState::On {
                None
            } else if state == RadioState::Off {
                Some(RADIO_OFF_HINT.into())
            } else if state == RadioState::Disabled {
                Some(RADIO_DISABLED_HINT.into())
            } else {
                None
            }
        }
        Err(_) => None,
    }
}

#[cfg(not(windows))]
pub async fn radio_unavailable_reason() -> Option<String> {
    None
}

pub async fn message_for_scan_failure(err: &impl std::fmt::Display) -> String {
    if let Some(reason) = radio_unavailable_reason().await {
        return reason;
    }
    let text = err.to_string();
    if looks_like_radio_off(&text) {
        return RADIO_OFF_HINT.to_string();
    }
    format!("启动扫描失败：{text}")
}

fn looks_like_radio_off(text: &str) -> bool {
    let lower = text.to_ascii_lowercase();
    lower.contains("radio")
        || lower.contains("turned off")
        || lower.contains("disabled")
        || lower.contains("not available")
        || lower.contains("not ready")
        || text.contains("0x800710DF")
        || text.contains("0x8007048F")
        || text.contains("0x800710df")
        || text.contains("0x8007048f")
}
