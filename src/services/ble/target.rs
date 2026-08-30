//! BLUETTI 目标设备识别（仅依据 ref/dev 广播格式，不维护名称前缀表）。
//!
//! 识别来源：
//! - 广播 Service UUID `0xFF00`
//! - Manufacturer Data magic `BLBLUETTF` / `BLBLUETTI`（默认识别包）
//! - Manufacturer Data Company ID `0x0F06` + prefix `0x02F0` / `0x82F0`（加密 LCD 广播）
//!
//! Scan Response 中的本地名只用于**显示**，不参与是否为目标设备的判定。

use btleplug::api::PeripheralProperties;
use uuid::Uuid;

use super::uuids::service_uuid;

/// SIG Company ID：SHENZHEN POWEROAK NEWENER CO., LTD
pub const COMPANY_ID_POWEROAK: u16 = 0x0F06;

/// 默认识别广播 Manufacturer Data（drv_nimble.c `MFG_DATA`，无 Company ID 前缀）
const MFG_MAGIC_BLBLUETTF: &[u8] = b"BLBLUETTF";

/// 扩展广播 Manufacturer Data（`BLE_CONFIG_EXTENDED_ADV`）
const MFG_MAGIC_BLBLUETTI: &[u8] = b"BLBLUETTI";

/// 加密 LCD 广播 prefix 基值（ble_adv.h）；bit15=`0x8000` 表示已连接。
const MFG_PREFIX_BASE: u16 = 0x02F0;
const MFG_PREFIX_CONNECTED_BIT: u16 = 0x8000;
const MFG_PREFIX_DISCONNECTED: u16 = MFG_PREFIX_BASE;
const MFG_PREFIX_CONNECTED: u16 = MFG_PREFIX_BASE | MFG_PREFIX_CONNECTED_BIT;

/// 从广播推断的对端连接占用提示（非通用 BLE Flags，而是 BLUETTI 厂商字段）。
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum AdvLinkHint {
    #[default]
    Unknown,
    /// 广播声明未连接（可尝试连接）。
    Available,
    /// 广播声明已连接（可能被其它主机占用）。
    Occupied,
}

pub fn matches_ff00(uuid: &Uuid) -> bool {
    uuid == &service_uuid() || (uuid.as_u128() & 0xFFFF) == 0xFF00
}

fn poweroak_prefix_hint(prefix: u16) -> AdvLinkHint {
    if prefix == MFG_PREFIX_CONNECTED {
        return AdvLinkHint::Occupied;
    }
    if prefix == MFG_PREFIX_DISCONNECTED {
        return AdvLinkHint::Available;
    }
    // 兼容同基值上其它标志位组合。
    if (prefix & !MFG_PREFIX_CONNECTED_BIT) == MFG_PREFIX_BASE {
        return if prefix & MFG_PREFIX_CONNECTED_BIT != 0 {
            AdvLinkHint::Occupied
        } else {
            AdvLinkHint::Available
        };
    }
    AdvLinkHint::Unknown
}

fn poweroak_payload_hint(payload: &[u8]) -> AdvLinkHint {
    if payload.len() < 2 {
        return AdvLinkHint::Unknown;
    }
    poweroak_prefix_hint(u16::from_le_bytes([payload[0], payload[1]]))
}

/// 厂商数据段是否匹配 BLUETTI 广播格式。
pub fn is_target_manufacturer_data(data: &[u8]) -> bool {
    if data.is_empty() {
        return false;
    }
    if data
        .windows(MFG_MAGIC_BLBLUETTF.len())
        .any(|w| w == MFG_MAGIC_BLBLUETTF)
    {
        return true;
    }
    if data
        .windows(MFG_MAGIC_BLBLUETTI.len())
        .any(|w| w == MFG_MAGIC_BLBLUETTI)
    {
        return true;
    }
    // PowerOak 加密广播：Company ID + prefix（ble_adv_container.header）
    if data.len() >= 4 {
        let company = u16::from_le_bytes([data[0], data[1]]);
        if company == COMPANY_ID_POWEROAK
            && poweroak_prefix_hint(u16::from_le_bytes([data[2], data[3]])) != AdvLinkHint::Unknown
        {
            return true;
        }
    }
    false
}

fn manufacturer_entry_hint(company: u16, data: &[u8]) -> AdvLinkHint {
    if company == COMPANY_ID_POWEROAK {
        let hint = poweroak_payload_hint(data);
        if hint != AdvLinkHint::Unknown {
            return hint;
        }
    }
    // 部分平台把 Company ID 留在 value 前两字节。
    if data.len() >= 4 {
        let embedded = u16::from_le_bytes([data[0], data[1]]);
        if embedded == COMPANY_ID_POWEROAK {
            return poweroak_payload_hint(&data[2..]);
        }
    }
    if is_target_manufacturer_data(data) {
        // 默认识别包无连接 bit，只能判断是目标设备。
        return AdvLinkHint::Unknown;
    }
    AdvLinkHint::Unknown
}

/// 从广播属性读取连接占用提示（若有 PowerOak LCD 加密前缀）。
pub fn adv_link_hint_from_properties(props: &PeripheralProperties) -> AdvLinkHint {
    let mut best = AdvLinkHint::Unknown;
    for (company, data) in &props.manufacturer_data {
        match manufacturer_entry_hint(*company, data) {
            AdvLinkHint::Occupied => return AdvLinkHint::Occupied,
            AdvLinkHint::Available => best = AdvLinkHint::Available,
            AdvLinkHint::Unknown => {}
        }
    }
    best
}

/// 根据广播字段判定是否 BLUETTI 目标设备（不用本地名过滤）。
pub fn is_target_properties(props: &PeripheralProperties) -> bool {
    if props.services.iter().any(matches_ff00) {
        return true;
    }
    props.manufacturer_data.iter().any(|(company, data)| {
        *company == COMPANY_ID_POWEROAK && poweroak_payload_hint(data) != AdvLinkHint::Unknown
            || is_target_manufacturer_data(data)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn magic_default_adv() {
        assert!(is_target_manufacturer_data(b"BLBLUETTF"));
        assert!(is_target_manufacturer_data(b"BLBLUETTI"));
        assert!(!is_target_manufacturer_data(b"EDIFIER"));
    }

    #[test]
    fn magic_poweroak_encrypted() {
        let mut data = vec![0x06, 0x0F, 0xF0, 0x02, 0x80];
        assert!(is_target_manufacturer_data(&data));
        data[2] = 0xF0;
        data[3] = 0x82;
        assert!(is_target_manufacturer_data(&data));
        assert!(!is_target_manufacturer_data(&[0x06, 0x0F, 0x00, 0x00]));
    }

    #[test]
    fn poweroak_prefix_connected_bit() {
        assert_eq!(poweroak_prefix_hint(MFG_PREFIX_DISCONNECTED), AdvLinkHint::Available);
        assert_eq!(poweroak_prefix_hint(MFG_PREFIX_CONNECTED), AdvLinkHint::Occupied);
        assert_eq!(poweroak_prefix_hint(0x1234), AdvLinkHint::Unknown);
    }
}
