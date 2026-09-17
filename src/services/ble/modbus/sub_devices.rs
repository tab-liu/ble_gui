//! 21000 段设备主动上报：组网节点 / 子设备列表（ver=1 / ver=5）。
//!
//! 设备用 FC10 写 21000，payload 为 `ver + bias + N × POINT_BIND_INFO`。
//! 对齐 `ref/comm` `Modbus_WriteMultiRegs_Report_Frame` / `POINT_BIND_INFO`。

use log::info;

use super::rtu::verify_crc;
use super::tlv::REG_21000;
use crate::services::modbus::{SharedModbusLive, SubDevice};

/// 已组网设备信息。
pub const MODBUS_21000_FUNC_NET_POINT: u16 = 1;
/// 含未绑定设备。
pub const MODBUS_21000_FUNC_ALL_POINT: u16 = 5;

const POINT_BIND_INFO_BYTES: usize = 16;

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SubDeviceReport {
    pub ver: u16,
    pub bias: u16,
    pub devices: Vec<SubDevice>,
}

/// 解析设备主动上报的 21000 绑定帧；TLV（40004）与写应答返回 None。
pub fn parse_reg21000_report_frame(frame: &[u8]) -> Option<SubDeviceReport> {
    if frame.len() < 13 || frame[1] != 0x10 {
        return None;
    }
    let start = u16::from_be_bytes([frame[2], frame[3]]);
    if start != REG_21000 {
        return None;
    }
    if !verify_crc(frame) {
        return None;
    }
    let func = u16::from_be_bytes([frame[7], frame[8]]);
    if func != MODBUS_21000_FUNC_NET_POINT && func != MODBUS_21000_FUNC_ALL_POINT {
        return None;
    }
    let byte_count = frame[6] as usize;
    if byte_count < 4 || frame.len() < 7 + byte_count + 2 {
        return None;
    }
    parse_reg21000_payload(func, &frame[7..7 + byte_count])
}

fn parse_reg21000_payload(ver: u16, data: &[u8]) -> Option<SubDeviceReport> {
    if data.len() < 4 {
        return None;
    }
    let bias = u16::from_be_bytes([data[2], data[3]]);
    let mut devices = Vec::new();
    let mut offset = 4;
    while offset + POINT_BIND_INFO_BYTES <= data.len() {
        if let Some(device) = parse_point_bind_info(&data[offset..offset + POINT_BIND_INFO_BYTES]) {
            devices.push(device);
        }
        offset += POINT_BIND_INFO_BYTES;
    }
    Some(SubDeviceReport {
        ver,
        bias,
        devices,
    })
}

fn parse_point_bind_info(data: &[u8]) -> Option<SubDevice> {
    if data.len() < POINT_BIND_INFO_BYTES {
        return None;
    }
    let info0 = u16::from_be_bytes([data[0], data[1]]);
    let info1 = u16::from_be_bytes([data[2], data[3]]);
    let state = u16::from_be_bytes([data[4], data[5]]);
    let sn_words = [
        u16::from_be_bytes([data[6], data[7]]),
        u16::from_be_bytes([data[8], data[9]]),
        u16::from_be_bytes([data[10], data[11]]),
        u16::from_be_bytes([data[12], data[13]]),
    ];
    let dev_type = u16::from_be_bytes([data[14], data[15]]);
    let sn = combine_u64(&sn_words);
    if info0 == 0 && info1 == 0 && state == 0 && sn == 0 && dev_type == 0 {
        return None;
    }
    let slave_addr = (info0 & 0xFF) as u8;
    Some(SubDevice {
        is_self: slave_addr == 0,
        slave_addr,
        group_addr: (info0 >> 8) as u8,
        master_addr: (info1 & 0xFF) as u8,
        group_same_type_addr: (info1 >> 8) as u8,
        state,
        sn,
        dev_type,
    })
}

fn combine_u64(words: &[u16]) -> u64 {
    words.iter().enumerate().fold(0u64, |acc, (i, &w)| {
        acc | ((w as u64) << (16 * i))
    })
}

pub fn apply_sub_device_report(live: &SharedModbusLive, report: SubDeviceReport) {
    let Ok(mut inner) = live.lock() else {
        return;
    };
    let start = report.bias as usize;
    if start == 0 {
        inner.sub_devices = report.devices;
    } else {
        if inner.sub_devices.len() > start {
            inner.sub_devices.truncate(start);
        }
        inner.sub_devices.extend(report.devices);
    }
    inner.sub_devices_valid = true;
    info!(
        target: "ble_gui::poll",
        "21000 组网列表 ver={} bias={} 共 {} 台: {}",
        report.ver,
        report.bias,
        inner.sub_devices.len(),
        inner
            .sub_devices
            .iter()
            .map(SubDevice::summary)
            .collect::<Vec<_>>()
            .join(" | "),
    );
}

pub fn sn_type_name(dev_type: u16) -> String {
    if dev_type == 0 {
        return String::new();
    }
    if let Some(name) = sn_type_ascii(dev_type) {
        return name.to_string();
    }
    let kind = sn_type_kind(dev_type);
    if kind.is_empty() {
        format!("类型 {dev_type}")
    } else {
        format!("{kind} ({dev_type})")
    }
}

fn sn_type_kind(dev_type: u16) -> &'static str {
    match dev_type {
        1..=999 => "便携电源",
        1000..=1999 => "户储",
        2000..=2999 => "微逆",
        3000..=3999 => "配件",
        4000..=4999 => "电池包",
        30000..=30014 => "通用设备",
        _ => "",
    }
}

fn sn_type_ascii(dev_type: u16) -> Option<&'static str> {
    SN_TYPE_ASCII
        .iter()
        .find(|(id, _)| *id == dev_type)
        .map(|(_, name)| *name)
}

const SN_TYPE_ASCII: &[(u16, &str)] = &[
    (1, "AC200MAX"),
    (2, "AC200L"),
    (3, "AC300"),
    (4, "AC500"),
    (5, "AC70"),
    (6, "AP300"),
    (7, "PINa"),
    (8, "Elite 200 V2"),
    (9, "RV5"),
    (10, "FP"),
    (11, "EL300"),
    (12, "EB3A"),
    (13, "AC60"),
    (14, "EB55"),
    (15, "EB70"),
    (16, "AC180T-S"),
    (17, "AC180T-D"),
    (18, "EP500"),
    (19, "EP500Pro"),
    (20, "AC2A"),
    (21, "AC50B"),
    (22, "AC60P"),
    (23, "AC180"),
    (24, "AC200P"),
    (25, "AC240"),
    (26, "Handsfree1"),
    (27, "Handsfree2"),
    (28, "EL320"),
    (29, "EL400"),
    (30, "EL80V2"),
    (31, "EL100"),
    (32, "EL30V2"),
    (33, "AC200PL"),
    (34, "PR002"),
    (35, "AC45"),
    (36, "AC50P"),
    (37, "AC55"),
    (38, "AC70P"),
    (39, "AC240P"),
    (40, "AC180P"),
    (41, "AC2P"),
    (42, "Premium 20C"),
    (43, "KW1000"),
    (44, "LFP700"),
    (45, "PR30V2"),
    (46, "PR100V2"),
    (47, "PR200V2"),
    (48, "AORA10"),
    (49, "AORA30P"),
    (50, "AORA30V2"),
    (51, "AORA 80"),
    (52, "AORA 100"),
    (53, "AORA100V2"),
    (54, "AORA200"),
    (55, "AORA320"),
    (56, "AP500"),
    (57, "PI5000EG"),
    (58, "HS5"),
    (59, "HS3"),
    (60, "HS2"),
    (61, "AORA400"),
    (62, "EL10"),
    (63, "EL100mini"),
    (64, "EL30mini"),
    (65, "EL200mini"),
    (66, "AORA100mini"),
    (67, "AORA30mini"),
    (68, "AORA200mini"),
    (69, "AP200"),
    (70, "AP300V2"),
    (71, "EL200V3"),
    (72, "EL250V3"),
    (73, "PI3000HYB"),
    (74, "AORA200V3"),
    (75, "AORA250V3"),
    (76, "AORA300"),
    (77, "EL100V3"),
    (78, "AORA100V3"),
    (79, "PR150V3"),
    (80, "AORA150V3"),
    (81, "AORA200V2"),
    (1000, "EP600"),
    (1001, "EP760"),
    (1002, "EP800"),
    (1003, "EP900"),
    (1004, "EP2000"),
    (1005, "EP13K"),
    (1006, "EP6K"),
    (1007, "EP18K"),
    (1008, "EP5K"),
    (1009, "EP5K5"),
    (1010, "AP12K"),
    (1051, "Balco500"),
    (1052, "Balco260"),
    (2000, "A80"),
    (2001, "D100S"),
    (2002, "A100S"),
    (2003, "A80P"),
    (2004, "D100P"),
    (2005, "WT300"),
    (3000, "S1"),
    (3001, "AT1"),
    (3002, "COMBOX"),
    (3003, "PBOX"),
    (3004, "EBOX"),
    (3005, "Epad"),
    (3006, "Epanel"),
    (3007, "HD1"),
    (3008, "HA1"),
    (3009, "SolarX 4K"),
    (3010, "CHARGER 1"),
    (3011, "CHARGER 2"),
    (3012, "Edock"),
    (3013, "Display 1"),
    (3014, "AP300_IOT"),
    (3015, "SHELLY_METER"),
    (3018, "SMeter"),
    (4000, "B500"),
    (4001, "B500H"),
    (4002, "IB500"),
    (4003, "B1210"),
    (4004, "B4810"),
    (4005, "B300"),
    (4006, "B300K"),
    (4007, "B300S"),
    (4008, "B1232"),
    (4009, "LeadAcid"),
    (4010, "LFP"),
    (4012, "B500A"),
    (4013, "BC200"),
    (4014, "B230"),
    (4015, "B210"),
    (4016, "B500K"),
    (4017, "B900"),
    (4018, "HB500"),
    (4019, "BC260"),
    (4020, "EK900"),
    (4021, "IB800JP"),
    (4022, "B500Pro"),
    (4023, "B300Pro"),
    (4024, "IB800"),
    (4025, "HB500S"),
    (4026, "BH500E"),
    (4027, "B4805"),
];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_device_log_fc10_ver1() {
        let frame = hex::decode(
            "00105208000a1400010000000000000001e85893d30264000000003c85",
        )
        .unwrap();
        let report = parse_reg21000_report_frame(&frame).expect("21000 绑定帧");
        assert_eq!(report.ver, 1);
        assert_eq!(report.bias, 0);
        assert_eq!(report.devices.len(), 1);
        let dev = &report.devices[0];
        assert!(dev.is_self);
        assert_eq!(dev.slave_addr, 0);
        assert_eq!(dev.group_addr, 0);
        assert_eq!(dev.state, 1);
        assert!(dev.online());
        assert_eq!(dev.sn, 0x0264_93D3_E858);
        assert_eq!(dev.dev_type, 0);
    }

    #[test]
    fn skips_tlv_and_short_ack() {
        let tlv = hex::decode("001052080002049c4400000000").unwrap();
        assert!(parse_reg21000_report_frame(&tlv).is_none());
        let ack = hex::decode("00105208000a3c85").unwrap();
        assert!(parse_reg21000_report_frame(&ack).is_none());
    }

    #[test]
    fn battery_type_name() {
        assert_eq!(sn_type_name(4005), "B300");
        assert_eq!(sn_type_name(3007), "HD1");
        assert_eq!(sn_type_name(0), "");
    }
}
