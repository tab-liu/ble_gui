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
const IOT_VER_OFF: usize = 14;

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
}
