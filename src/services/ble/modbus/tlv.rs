//! Modbus TLV 组合读（21000 段，功能码 40004/40005），对齐 ref/comm `modbus_tlv`.

use std::collections::BTreeMap;
use std::io;

use super::rtu::{append_crc, verify_crc};

pub const REG_21000: u16 = 21000;
pub const REG_IOT_STATUS: u16 = 3;
pub const MODBUS_TLV_ENABLE_BIT: u16 = 1 << 3;

/// 汇总从机地址（TLV 请求/响应帧头）。
pub const MD_SUMMARY_ADDR: u8 = 0;

pub const MODBUS_21000_FUNC_TLV_DATA: u16 = 40004;
pub const MODBUS_21000_FUNC_TLV_READ_CMD: u16 = 40005;

// ── TLV 组合读协议上限（对齐 ref/comm `MODBUS_TLV_SINGLE_NUM_MAX` 量级）──

/// 单次 FC 0x10 写 21000 时，TL 列表最多包含的查询项数。
/// 一项对应 UI 中一条查询（如地址 100、长度 20 仍计 1 项）。
pub const MODBUS_TLV_MAX_ITEMS_PER_BATCH: usize = 40;

const TL_HEADER_LEN: usize = 6;

/// 单次 TLV 读条目（仅 TL，不含 V）。
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TlReadSpec {
    pub slave_addr: u8,
    pub reg_addr: u16,
    pub byte_len: u16,
}

impl TlReadSpec {
    pub fn from_register(slave_addr: u8, reg_addr: u16, register_count: u16) -> Self {
        Self {
            slave_addr,
            reg_addr,
            byte_len: register_count.max(1) * 2,
        }
    }

    pub fn register_count(&self) -> u16 {
        (self.byte_len / 2).max(1)
    }
}

/// 将 TL 列表按 [`MODBUS_TLV_MAX_ITEMS_PER_BATCH`] 分块。
pub fn chunk_tl_batches(specs: &[TlReadSpec]) -> Vec<&[TlReadSpec]> {
    specs.chunks(MODBUS_TLV_MAX_ITEMS_PER_BATCH).collect()
}

/// 查询项在 TLV 分批列表中的批次序号（0-based）。
pub fn tlv_item_batch_index(item_index: usize) -> usize {
    item_index / MODBUS_TLV_MAX_ITEMS_PER_BATCH
}

/// 某批次在查询项列表中的起始下标。
pub fn tlv_batch_start_index(batch_index: usize) -> usize {
    batch_index * MODBUS_TLV_MAX_ITEMS_PER_BATCH
}

/// 从 TLV 响应中取单项查询对应的寄存器值。
pub fn tlv_register_values(
    results: &[TlvReadResult],
    slave_addr: u8,
    reg_addr: u16,
) -> io::Result<Vec<u16>> {
    let unit = find_tlv_result(results, slave_addr, reg_addr).ok_or_else(|| {
        io::Error::new(
            io::ErrorKind::InvalidData,
            format!("TLV 响应缺少寄存器 {reg_addr}"),
        )
    })?;
    tlv_data_to_u16(&unit.data)
}

/// TLV 响应中解析出的一个寄存器块。
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TlvReadResult {
    pub slave_addr: u8,
    pub reg_addr: u16,
    pub data: Vec<u8>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TlvResponsePacket {
    pub total_index: u8,
    pub curr_index: u8,
    pub payload: Vec<u8>,
}

/// 寄存器 3 bit3 = 1 表示设备支持 TLV 读。
pub fn iot_status_supports_tlv(status_word: u16) -> bool {
    status_word & MODBUS_TLV_ENABLE_BIT != 0
}

/// 构建 FC 0x10 写 21000 的 TLV 组合读请求（ver=40005 + TL 列表）。
pub fn build_tlv_read_request(tl_items: &[TlReadSpec]) -> Vec<u8> {
    let mut payload = Vec::with_capacity(4 + tl_items.len() * TL_HEADER_LEN);
    // 对齐 ref/comm `Modbus_WriteCmd_06H_10H_Build`：inbuf 为 LE u16 字节流（低字节在前）。
    payload.extend_from_slice(&MODBUS_21000_FUNC_TLV_READ_CMD.to_le_bytes());
    payload.push(0); // curr_idx（40005 段 layout，见 MOD_STRUCT_reg21000_WR）
    payload.push(0); // total
    for item in tl_items {
        payload.push(item.slave_addr);
        payload.push(0);
        payload.extend_from_slice(&item.reg_addr.to_le_bytes());
        payload.extend_from_slice(&item.byte_len.to_le_bytes());
    }
    let regs = le_bytes_to_modbus_regs(&payload);
    build_write_multiple(MD_SUMMARY_ADDR, REG_21000, &regs)
}

pub fn build_write_multiple(slave_id: u8, start: u16, values: &[u16]) -> Vec<u8> {
    let byte_count = values.len() * 2;
    let mut frame = vec![
        slave_id,
        0x10,
        (start >> 8) as u8,
        start as u8,
        (values.len() >> 8) as u8,
        values.len() as u8,
        byte_count as u8,
    ];
    for value in values {
        frame.push((value >> 8) as u8);
        frame.push(*value as u8);
    }
    append_crc(frame)
}

pub fn is_fc10_write_ack(frame: &[u8]) -> bool {
    frame.len() == 8 && frame[1] == 0x10 && verify_crc(frame)
}

pub fn parse_tlv_response_packet(frame: &[u8]) -> Option<TlvResponsePacket> {
    if frame.len() < 13 || frame[1] != 0x10 {
        return None;
    }
    let start = u16::from_be_bytes([frame[2], frame[3]]);
    if start != REG_21000 {
        return None;
    }
    let func = u16::from_be_bytes([frame[7], frame[8]]);
    if func != MODBUS_21000_FUNC_TLV_DATA {
        return None;
    }
    if !verify_crc(frame) {
        return None;
    }
    let byte_count = frame[6] as usize;
    if byte_count < 4 {
        return None;
    }
    let payload_len = byte_count - 4;
    if frame.len() < 11 + payload_len + 2 {
        return None;
    }
    // 设备在 offset 11 起对 TLV 数据区（含 TL 头与 V）逐 u16 做 U16_TO_BE，客户端按 Modbus 大端解析。
    let payload = frame[11..11 + payload_len].to_vec();
    Some(TlvResponsePacket {
        total_index: frame[9],
        curr_index: frame[10],
        payload,
    })
}

pub fn parse_tlv_read_units(data: &[u8]) -> Vec<TlvReadResult> {
    let mut out = Vec::new();
    let mut offset = 0;
    while offset + TL_HEADER_LEN <= data.len() {
        let slave_addr = data[offset];
        // 设备 memcpy TL 后对整段 payload 做 U16_TO_BE；reg_addr/len 在链路上为 Modbus 大端。
        let reg_addr = u16::from_be_bytes([data[offset + 2], data[offset + 3]]);
        let len = u16::from_be_bytes([data[offset + 4], data[offset + 5]]) as usize;
        offset += TL_HEADER_LEN;
        if len == 0 {
            out.push(TlvReadResult {
                slave_addr,
                reg_addr,
                data: Vec::new(),
            });
            continue;
        }
        if len % 2 != 0 || offset + len > data.len() {
            break;
        }
        out.push(TlvReadResult {
            slave_addr,
            reg_addr,
            data: data[offset..offset + len].to_vec(),
        });
        offset += len;
    }
    out
}

/// 简要描述 TLV 解析结果，供日志输出。
pub fn describe_tlv_units(units: &[TlvReadResult]) -> String {
    if units.is_empty() {
        return "（无单元）".into();
    }
    units
        .iter()
        .map(|u| format!("reg{}:{}B", u.reg_addr, u.data.len()))
        .collect::<Vec<_>>()
        .join(", ")
}

pub fn find_tlv_result<'a>(
    results: &'a [TlvReadResult],
    slave_addr: u8,
    reg_addr: u16,
) -> Option<&'a TlvReadResult> {
    results
        .iter()
        .find(|r| r.slave_addr == slave_addr && r.reg_addr == reg_addr)
}

pub fn tlv_data_to_u16(values: &[u8]) -> io::Result<Vec<u16>> {
    if values.len() % 2 != 0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "TLV 数据长度不是偶数",
        ));
    }
    Ok(values
        .chunks(2)
        .map(|c| u16::from_be_bytes([c[0], c[1]]))
        .collect())
}

#[derive(Default)]
pub struct TlvPacketCollector {
    total: Option<u8>,
    packets: BTreeMap<u8, Vec<u8>>,
}

impl TlvPacketCollector {
    pub fn received_count(&self) -> usize {
        self.packets.len()
    }

    pub fn expected_total(&self) -> Option<u8> {
        self.total
    }

    pub fn insert(&mut self, packet: TlvResponsePacket) -> io::Result<()> {
        if packet.total_index == 0 || packet.curr_index == 0 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "TLV 响应包序号无效",
            ));
        }
        if self.total.is_none() {
            self.total = Some(packet.total_index);
        } else if self.total != Some(packet.total_index) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "TLV 响应总包数不一致",
            ));
        }
        self.packets.insert(packet.curr_index, packet.payload);
        Ok(())
    }

    pub fn is_complete(&self) -> bool {
        match self.total {
            Some(total) if total > 0 => self.packets.len() == total as usize,
            _ => false,
        }
    }

    pub fn assembled(&self) -> Vec<u8> {
        let Some(total) = self.total else {
            return Vec::new();
        };
        let mut out = Vec::new();
        for index in 1..=total {
            if let Some(chunk) = self.packets.get(&index) {
                out.extend_from_slice(chunk);
            }
        }
        out
    }
}

/// 将 LE 字节流转为 Modbus 写寄存器值（对齐 `Modbus_WriteCmd_06H_10H_Build` 的 inbuf）。
fn le_bytes_to_modbus_regs(bytes: &[u8]) -> Vec<u16> {
    let mut padded = bytes.to_vec();
    if padded.len() % 2 != 0 {
        padded.push(0);
    }
    padded
        .chunks(2)
        .map(|pair| u16::from_le_bytes([pair[0], pair[1]]))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tlv_enable_bit_on_reg3() {
        assert!(!iot_status_supports_tlv(0));
        assert!(iot_status_supports_tlv(1 << 3));
        assert!(iot_status_supports_tlv(0x0008));
    }

    #[test]
    fn build_read_request_contains_ver_40005() {
        let frame = build_tlv_read_request(&[TlReadSpec::from_register(1, 100, 2)]);
        assert_eq!(frame[1], 0x10);
        assert_eq!(u16::from_be_bytes([frame[2], frame[3]]), REG_21000);
        assert_eq!(
            u16::from_be_bytes([frame[7], frame[8]]),
            MODBUS_21000_FUNC_TLV_READ_CMD
        );
    }

    #[test]
    fn build_read_request_wire_order_matches_modbus_write_cmd() {
        // 主页 TLV：reg100×50 + reg2011×2；reg2011(0x07DB) 高字节非零，可暴露 BE/LE 组包差异。
        let frame = build_tlv_read_request(&[
            TlReadSpec::from_register(0, 100, 50),
            TlReadSpec::from_register(0, 2011, 2),
        ]);
        assert!(
            super::super::rtu::verify_crc(&frame),
            "FC10 帧 CRC 无效",
        );
        // 旧实现（BE 字节对）在 reg2011 处发出 db 07，设备会解析成 0xDB07。
        assert_eq!(
            &frame[19..21],
            &[0x07, 0xdb],
            "reg2011 在 Modbus 数据区须为 07 db（LE u16 0x07DB 的 wire 序）",
        );
        assert_ne!(
            &frame[19..21],
            &[0xdb, 0x07],
            "不应再出现旧版错误的 db 07 序",
        );
    }

    #[test]
    fn chunk_tl_batches_respects_max_items_per_batch() {
        let total = MODBUS_TLV_MAX_ITEMS_PER_BATCH * 2;
        let specs: Vec<_> = (0..total)
            .map(|i| TlReadSpec::from_register(1, 1000 + i as u16, 20))
            .collect();
        let batches = chunk_tl_batches(&specs);
        assert_eq!(batches.len(), 2);
        assert_eq!(batches[0].len(), MODBUS_TLV_MAX_ITEMS_PER_BATCH);
        assert_eq!(batches[1].len(), MODBUS_TLV_MAX_ITEMS_PER_BATCH);
        assert_eq!(batches[0][0].register_count(), 20);
        assert_eq!(
            tlv_batch_start_index(1),
            MODBUS_TLV_MAX_ITEMS_PER_BATCH
        );
        assert_eq!(tlv_item_batch_index(MODBUS_TLV_MAX_ITEMS_PER_BATCH - 1), 0);
        assert_eq!(tlv_item_batch_index(MODBUS_TLV_MAX_ITEMS_PER_BATCH), 1);
    }

    #[test]
    fn parse_tlv_units_from_device_wire_sample() {
        // 实机空数据响应：设备 U16_TO_BE 后 reg100=00 64、len=0
        let payload = hex::decode("000000640000000007db0000").unwrap();
        let units = parse_tlv_read_units(&payload);
        assert_eq!(units.len(), 2);
        assert_eq!(units[0].reg_addr, 100);
        assert!(units[0].data.is_empty());
        assert_eq!(units[1].reg_addr, 2011);
        assert!(units[1].data.is_empty());
    }

    #[test]
    fn parse_tlv_response_frame_from_device_log() {
        let payload = hex::decode("000000640000000007db0000").unwrap();
        let frame = super::super::rtu::append_crc({
            let mut f = hex::decode("001052080008109c440101").unwrap();
            f.extend_from_slice(&payload);
            f
        });
        let packet = parse_tlv_response_packet(&frame).expect("TLV 帧");
        assert_eq!(packet.total_index, 1);
        assert_eq!(packet.curr_index, 1);
        assert_eq!(packet.payload, payload);
        let units = parse_tlv_read_units(&packet.payload);
        assert_eq!(units[0].reg_addr, 100);
        assert_eq!(units[1].reg_addr, 2011);
    }

    #[test]
    fn parse_tlv_response_with_reg100_data_from_device_log() {
        // 实机结构：TL1(reg100,100B) + TL2(reg2011,4B)；V 区与日志前缀一致
        let mut payload = hex::decode(concat!(
            "000000640064",
            "14cc0000005d0000531c531c0000000300030000",
            "5041303300300000000000e6cde5d8024b000000010004000104",
            "080002000000000000000000000000000000000000000000000000",
            "000000000000000000000000000000000000000000000000000000",
            "000007db0004",
            "00000000",
        ))
        .unwrap();
        assert_eq!(payload.len(), 116, "合成 payload 须为 116B");
        let units = parse_tlv_read_units(&payload);
        assert_eq!(units.len(), 2);
        assert_eq!(units[0].reg_addr, 100);
        assert_eq!(units[0].data.len(), 100);
        assert_eq!(units[1].reg_addr, 2011);
        assert_eq!(units[1].data.len(), 4);
        assert_eq!(tlv_data_to_u16(&units[0].data).unwrap().len(), 50);
    }

    #[test]
    fn parse_tlv_units_skips_zero_len_and_continues() {
        let payload = hex::decode("000000640000000007db000400640100").unwrap();
        let units = parse_tlv_read_units(&payload);
        assert_eq!(units.len(), 2);
        assert_eq!(units[0].reg_addr, 100);
        assert!(units[0].data.is_empty());
        assert_eq!(units[1].reg_addr, 2011);
        assert_eq!(units[1].data.len(), 4);
        assert_eq!(tlv_data_to_u16(&units[1].data).unwrap(), vec![100, 256]);
    }

    #[test]
    fn parse_tlv_units_roundtrip() {
        let payload = hex::decode("00000064000400640100").unwrap();
        let units = parse_tlv_read_units(&payload);
        assert_eq!(units.len(), 1);
        assert_eq!(units[0].reg_addr, 100);
        assert_eq!(tlv_data_to_u16(&units[0].data).unwrap(), vec![100, 256]);
    }
}
