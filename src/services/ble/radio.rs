//! 查询系统蓝牙开关。
//!
//! 蓝牙关闭时部分平台仍可能枚举到适配器，btleplug 的 `adapters()` 会成功，
//! 必须额外读 [`Central::adapter_state`]，否则界面会一直停在「扫描中 / 未连接」。
//! 平台差异由 btleplug 内部处理，本模块不再分 OS 分支。

use btleplug::api::{Central, CentralState};
use btleplug::platform::Adapter;
use log::info;

pub const RADIO_OFF_HINT: &str = "系统蓝牙已关闭，请打开后再点扫描";
pub const NO_ADAPTER_HINT: &str = "未找到蓝牙适配器，请打开系统蓝牙后再点扫描";

pub fn is_radio_unavailable_detail(detail: &str) -> bool {
    detail.contains("系统蓝牙")
        || detail.contains("未找到蓝牙适配器")
        || detail.contains("蓝牙不可用")
}

pub async fn radio_unavailable_reason(adapter: &Adapter) -> Option<String> {
    match adapter.adapter_state().await {
        Ok(CentralState::PoweredOn) => None,
        Ok(CentralState::PoweredOff) => {
            info!(target: "ble_gui::radio", "adapter_state=PoweredOff");
            Some(RADIO_OFF_HINT.into())
        }
        Ok(CentralState::Unknown) => {
            info!(target: "ble_gui::radio", "adapter_state=Unknown");
            None
        }
        Err(err) => {
            info!(target: "ble_gui::radio", "adapter_state error: {err}");
            None
        }
    }
}

pub async fn message_for_scan_failure(adapter: &Adapter, err: &impl std::fmt::Display) -> String {
    if let Some(reason) = radio_unavailable_reason(adapter).await {
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
        || lower.contains("powered off")
        || lower.contains("power off")
        || text.contains("0x800710DF")
        || text.contains("0x8007048F")
        || text.contains("0x800710df")
        || text.contains("0x8007048f")
}
