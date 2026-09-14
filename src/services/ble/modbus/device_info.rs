//! 寄存器 1100～1130：机型、SN、软件版本列表（对齐 `ref/tool` 1100 段）。
//!
//! 每条软件版本占 3 个寄存器：`type` + `version` u32（低字在前，两个寄存器）。
//! IOT 自身料号在 11000 段的 `software_ver`（同样两个寄存器），1100 的 type=0 槽经常是 0。

use crate::services::firmware::header::format_software_name;

/// 1100 起始，共 31 个寄存器覆盖到 1130（6 条软件版本）。
pub const REG_DEVICE_INFO_START: u16 = 1100;
pub const REG_DEVICE_INFO_COUNT: u16 = 31;

/// 11000～11015：IOT 类型 / SN / software_ver（11014～11015）。
pub const REG_IOT_INFO_START: u16 = 11000;
pub const REG_IOT_INFO_COUNT: u16 = 16;
/// 11000～11033：安全码、STA/BLE MAC（连接后读一次，不进周期轮询）。
pub const REG_IOT_IDENTITY_COUNT: u16 = 34;
/// 整机类型 + SN（110～115 ASCII，116～119 uint64）。
pub const REG_WHOLE_DEVICE_START: u16 = 110;
pub const REG_WHOLE_DEVICE_COUNT: u16 = 10;
const IOT_VER_OFF: usize = 14;
const IOT_SN_OFF: usize = 6;
const IOT_SAFE_OFF: usize = 10;
const IOT_STA_MAC_OFF: usize = 27;
const IOT_BLE_MAC_OFF: usize = 30;

const TYPE_OFF: usize = 1101 - 1100;
const TYPE_REGS: usize = 6;
const SN_OFF: usize = 1107 - 1100;
const SOFT_COUNT_OFF: usize = 1112 - 1100;
const SOFT_FIRST_OFF: usize = 1113 - 1100;
const SOFT_STRIDE: usize = 3;
const SOFT_MAX: usize = 6;

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct DeviceSoftware {
    pub type_code: u16,
    pub version: u32,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct DeviceInfo {
    pub device_type: String,
    pub sn: String,
    pub software: Vec<DeviceSoftware>,
}

impl DeviceInfo {
    pub fn summary_text(&self) -> String {
        if self.software.is_empty() {
            return "—".into();
        }
        self.software
            .iter()
            .map(|s| {
                format!(
                    "{} {}",
                    format_software_name(s.type_code, s.version),
                    s.version
                )
            })
            .collect::<Vec<_>>()
            .join("  |  ")
    }

    pub fn iot_version(&self) -> Option<u32> {
        self.software
            .iter()
            .find(|s| s.type_code == 0)
            .map(|s| s.version)
    }

    pub fn merge_iot_version(&mut self, version: u32) {
        if version == 0 {
            return;
        }
        if let Some(existing) = self.software.iter_mut().find(|s| s.type_code == 0) {
            if existing.version == 0 {
                existing.version = version;
            }
            return;
        }
        self.software.insert(
            0,
            DeviceSoftware {
                type_code: 0,
                version,
            },
        );
    }
}

/// 低字在前拼 u32，与仪表板 / C# `CombineUInt32` 一致。
fn combine_u32(low: u16, high: u16) -> u32 {
    u32::from(low) | (u32::from(high) << 16)
}

pub fn format_regs_hex(values: &[u16]) -> String {
    values
        .iter()
        .map(|v| format!("{v:04X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

pub fn parse_device_info(values: &[u16]) -> Result<DeviceInfo, String> {
    if values.len() < REG_DEVICE_INFO_COUNT as usize {
        return Err(format!(
            "1100 段寄存器不足（{} / {}）",
            values.len(),
            REG_DEVICE_INFO_COUNT
        ));
    }

    let device_type = ascii_low_byte_first(&values[TYPE_OFF..TYPE_OFF + TYPE_REGS]);
    let sn = combine_u64(&values[SN_OFF..SN_OFF + 4]);
    let sn = if sn == 0 {
        String::new()
    } else {
        sn.to_string()
    };

    let reported = (values[SOFT_COUNT_OFF] as usize).min(SOFT_MAX);
    // software_total 未填时仍扫 6 槽，避免 IOT 槽为 0 时整表被跳过。
    let n = reported.max(SOFT_MAX);
    let mut software = Vec::new();
    for i in 0..n {
        let off = SOFT_FIRST_OFF + i * SOFT_STRIDE;
        if off + 2 >= values.len() {
            break;
        }
        let type_code = values[off];
        let version = combine_u32(values[off + 1], values[off + 2]);
        if version == 0 {
            continue;
        }
        software.push(DeviceSoftware { type_code, version });
    }

    Ok(DeviceInfo {
        device_type,
        sn,
        software,
    })
}

/// 11000 段：`software_ver` 在 11014～11015，两个寄存器低字在前。
pub fn parse_iot_software_ver(values: &[u16]) -> Option<u32> {
    if values.len() < REG_IOT_INFO_COUNT as usize {
        return None;
    }
    let version = combine_u32(values[IOT_VER_OFF], values[IOT_VER_OFF + 1]);
    (version != 0).then_some(version)
}

pub fn parse_iot_type(values: &[u16]) -> String {
    if values.len() < 6 {
        return String::new();
    }
    ascii_low_byte_first(&values[..6])
}

/// 110～119：整机 ASCII 类型 + uint64 SN（低字在前）。
pub fn parse_whole_device(values: &[u16]) -> (String, String) {
    if values.len() < REG_WHOLE_DEVICE_COUNT as usize {
        return (String::new(), String::new());
    }
    (
        ascii_low_byte_first(&values[..6]),
        format_u64_decimal(&values[6..10]),
    )
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct IotIdentity {
    pub iot_type: String,
    pub iot_sn: String,
    pub safe_code: String,
    pub wifi_mac: String,
    pub ble_mac: String,
}

/// 从 11000 段解析 IoT 类型 / SN / 安全码 / MAC。不足 34 个寄存器时 MAC 可为空。
pub fn parse_iot_identity(values: &[u16]) -> IotIdentity {
    IotIdentity {
        iot_type: parse_iot_type(values),
        iot_sn: values
            .get(IOT_SN_OFF..IOT_SN_OFF + 4)
            .map(format_u64_decimal)
            .unwrap_or_default(),
        safe_code: values
            .get(IOT_SAFE_OFF..IOT_SAFE_OFF + 4)
            .map(format_u64_decimal)
            .unwrap_or_default(),
        wifi_mac: values
            .get(IOT_STA_MAC_OFF..IOT_STA_MAC_OFF + 3)
            .map(parse_mac6)
            .unwrap_or_default(),
        ble_mac: values
            .get(IOT_BLE_MAC_OFF..IOT_BLE_MAC_OFF + 3)
            .map(parse_mac6)
            .unwrap_or_default(),
    }
}

/// 保持寄存器 ASCII：寄存器内低字节在前。
pub fn parse_ascii_regs(values: &[u16]) -> String {
    ascii_low_byte_first(values)
}

/// 6 字节 MAC：每寄存器低字节在前，与 `sta_ipv4` 打包方式一致。
pub fn parse_mac6(values: &[u16]) -> String {
    if values.len() < 3 {
        return String::new();
    }
    let mut bytes = [0u8; 6];
    for i in 0..3 {
        bytes[i * 2] = (values[i] & 0xFF) as u8;
        bytes[i * 2 + 1] = (values[i] >> 8) as u8;
    }
    if bytes.iter().all(|&b| b == 0) {
        return String::new();
    }
    format!(
        "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]
    )
}

fn format_u64_decimal(words: &[u16]) -> String {
    let n = combine_u64(words);
    if n == 0 {
        String::new()
    } else {
        n.to_string()
    }
}

fn ascii_low_byte_first(values: &[u16]) -> String {
    let mut out = String::new();
    for &reg in values {
        let lo = (reg & 0xFF) as u8;
        let hi = (reg >> 8) as u8;
        for byte in [lo, hi] {
            if byte != 0 && byte != 0xFF {
                out.push(byte as char);
            }
        }
    }
    out.trim().to_string()
}

fn combine_u64(words: &[u16]) -> u64 {
    words.iter().enumerate().fold(0u64, |acc, (i, &w)| {
        acc | ((w as u64) << (16 * i))
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn version_is_two_registers_low_word_first() {
        let version = 100600108u32;
        let low = (version & 0xFFFF) as u16;
        let high = (version >> 16) as u16;
        assert_eq!(combine_u32(low, high), version);
        let mut values = vec![0u16; 31];
        values[SOFT_COUNT_OFF] = 1;
        values[SOFT_FIRST_OFF] = 1; // ARM
        values[SOFT_FIRST_OFF + 1] = low;
        values[SOFT_FIRST_OFF + 2] = high;
        let info = parse_device_info(&values).unwrap();
        assert_eq!(info.software[0].version, version);
        assert_eq!(info.summary_text(), "ARM 100600108");
    }

    #[test]
    fn boot_version_shows_boot_suffix() {
        let mut info = DeviceInfo::default();
        info.software.push(DeviceSoftware {
            type_code: 1,
            version: 100650100,
        });
        info.software.push(DeviceSoftware {
            type_code: 2,
            version: 802616,
        });
        assert_eq!(info.summary_text(), "ARM-BOOT 100650100  |  DSP 802616");
    }

    #[test]
    fn skips_zero_version_slots() {
        let values = vec![0u16; 31];
        let info = parse_device_info(&values).unwrap();
        assert!(info.software.is_empty());
        assert_eq!(info.iot_version(), None);
    }

    #[test]
    fn iot_software_ver_at_11014() {
        let mut values = vec![0u16; 16];
        values[14] = (100600108u32 & 0xFFFF) as u16;
        values[15] = (100600108u32 >> 16) as u16;
        assert_eq!(parse_iot_software_ver(&values), Some(100600108));
    }

    #[test]
    fn merge_fills_missing_iot_slot() {
        let mut info = DeviceInfo::default();
        info.merge_iot_version(100600108);
        assert_eq!(info.iot_version(), Some(100600108));
        assert!(info.summary_text().contains("IOT 100600108"));
    }

    #[test]
    fn mac_low_byte_first() {
        assert_eq!(
            parse_mac6(&[0xBBAA, 0xDDCC, 0xFFEE]),
            "AA:BB:CC:DD:EE:FF"
        );
        assert_eq!(parse_mac6(&[0, 0, 0]), "");
    }

    #[test]
    fn whole_device_and_iot_identity() {
        let mut whole = vec![0u16; 10];
        whole[0] = u16::from(b'H') | (u16::from(b'A') << 8);
        whole[1] = u16::from(b'1');
        whole[6] = 0x0042;
        let (ty, sn) = parse_whole_device(&whole);
        assert_eq!(ty, "HA1");
        assert_eq!(sn, "66");

        let mut iot = vec![0u16; 34];
        iot[0] = u16::from(b'I') | (u16::from(b'O') << 8);
        iot[6] = 7;
        iot[10] = 9;
        iot[27] = 0xBBAA;
        iot[28] = 0xDDCC;
        iot[29] = 0xFFEE;
        iot[30] = 0x2211;
        iot[31] = 0x4433;
        iot[32] = 0x6655;
        let id = parse_iot_identity(&iot);
        assert_eq!(id.iot_type, "IO");
        assert_eq!(id.iot_sn, "7");
        assert_eq!(id.safe_code, "9");
        assert_eq!(id.wifi_mac, "AA:BB:CC:DD:EE:FF");
        assert_eq!(id.ble_mac, "11:22:33:44:55:66");
    }
}
