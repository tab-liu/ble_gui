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

/// 加密 LCD 广播 prefix（ble_adv.h `MFG_HEADER_PREFIX_*`）
const MFG_PREFIX_DISCONNECTED: u16 = 0x02F0;
const MFG_PREFIX_CONNECTED: u16 = 0x82F0;

pub fn matches_ff00(uuid: &Uuid) -> bool {
    uuid == &service_uuid() || (uuid.as_u128() & 0xFFFF) == 0xFF00
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
        if company == COMPANY_ID_POWEROAK {
            let prefix = u16::from_le_bytes([data[2], data[3]]);
            if prefix == MFG_PREFIX_DISCONNECTED || prefix == MFG_PREFIX_CONNECTED {
                return true;
            }
        }
    }
    false
}

/// 根据广播字段判定是否 BLUETTI 目标设备（不用本地名过滤）。
pub fn is_target_properties(props: &PeripheralProperties) -> bool {
    if props.services.iter().any(matches_ff00) {
        return true;
    }
    props
        .manufacturer_data
        .values()
        .any(|data| is_target_manufacturer_data(data))
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
}
