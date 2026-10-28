//! BLUETTI 目标设备识别。本地名只用于显示。

use btleplug::api::PeripheralProperties;
use uuid::Uuid;

use super::uuids::service_uuid;

pub const COMPANY_ID_POWEROAK: u16 = 0x0F06;

const MFG_MAGIC_BLBLUETTF: &[u8] = b"BLBLUETTF";
const MFG_MAGIC_BLBLUETTI: &[u8] = b"BLBLUETTI";
const MFG_PREFIX_BASE: u16 = 0x02F0;
const MFG_PREFIX_CONNECTED_BIT: u16 = 0x8000;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum AdvLinkHint {
    #[default]
    Unknown,
    Available,
    Occupied,
}

pub fn matches_ff00(uuid: &Uuid) -> bool {
    uuid == &service_uuid() || (uuid.as_u128() & 0xFFFF) == 0xFF00
}

fn poweroak_prefix_hint(prefix: u16) -> AdvLinkHint {
    if prefix == MFG_PREFIX_BASE {
        return AdvLinkHint::Available;
    }
    if prefix == (MFG_PREFIX_BASE | MFG_PREFIX_CONNECTED_BIT) {
        return AdvLinkHint::Occupied;
    }
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

pub fn is_target_manufacturer_data(data: &[u8]) -> bool {
    if data.is_empty() {
        return false;
    }
    if data.windows(MFG_MAGIC_BLBLUETTF.len()).any(|w| w == MFG_MAGIC_BLBLUETTF) {
        return true;
    }
    if data.windows(MFG_MAGIC_BLBLUETTI.len()).any(|w| w == MFG_MAGIC_BLBLUETTI) {
        return true;
    }
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
    if data.len() >= 4 {
        let embedded = u16::from_le_bytes([data[0], data[1]]);
        if embedded == COMPANY_ID_POWEROAK {
            return poweroak_payload_hint(&data[2..]);
        }
    }
    if is_target_manufacturer_data(data) {
        return AdvLinkHint::Unknown;
    }
    AdvLinkHint::Unknown
}

pub fn is_target_properties(props: &PeripheralProperties) -> bool {
    if props.services.iter().any(matches_ff00) {
        return true;
    }
    props.manufacturer_data.iter().any(|(company, data)| {
        (*company == COMPANY_ID_POWEROAK && poweroak_payload_hint(data) != AdvLinkHint::Unknown)
            || is_target_manufacturer_data(data)
    })
}

#[allow(dead_code)]
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
