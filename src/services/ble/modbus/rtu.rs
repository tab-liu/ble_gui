//! Modbus RTU 帧：CRC16、读/写构建、响应解析。

use std::io;

/// Modbus RTU CRC16（多项式 0xA001，低字节在前）。
pub fn crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &byte in data {
        crc ^= byte as u16;
        for _ in 0..8 {
            if crc & 1 != 0 {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    crc
}

pub fn append_crc(mut frame: Vec<u8>) -> Vec<u8> {
    let c = crc16(&frame);
    frame.push(c as u8);
    frame.push((c >> 8) as u8);
    frame
}

pub fn verify_crc(frame: &[u8]) -> bool {
    if frame.len() < 3 {
        return false;
    }
    let body = &frame[..frame.len() - 2];
    let expected = crc16(body);
    let actual = frame[frame.len() - 2] as u16 | ((frame[frame.len() - 1] as u16) << 8);
    expected == actual
}

/// 读保持寄存器 FC 0x03。
pub fn build_read_holding(slave_id: u8, start: u16, count: u16) -> Vec<u8> {
    append_crc(vec![
        slave_id,
        0x03,
        (start >> 8) as u8,
        start as u8,
        (count >> 8) as u8,
        count as u8,
    ])
}

/// 写单个寄存器 FC 0x06。
pub fn build_write_single(slave_id: u8, address: u16, value: u16) -> Vec<u8> {
    append_crc(vec![
        slave_id,
        0x06,
        (address >> 8) as u8,
        address as u8,
        (value >> 8) as u8,
        value as u8,
    ])
}

/// 解析读保持寄存器响应，返回寄存器值数组。
pub fn parse_read_holding(response: &[u8], slave_id: u8, count: u16) -> io::Result<Vec<u16>> {
    if response.len() < 5 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Modbus 响应过短",
        ));
    }
    if response[0] != slave_id {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Modbus 从机地址不匹配",
        ));
    }
    if response[1] & 0x80 != 0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("Modbus 异常码 0x{:02X}", response[2]),
        ));
    }
    if response[1] != 0x03 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "非读保持寄存器响应",
        ));
    }
    let byte_count = response[2] as usize;
    let expected_bytes = count as usize * 2;
    if byte_count != expected_bytes || response.len() < 3 + byte_count + 2 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Modbus 响应字节数不匹配",
        ));
    }
    if !verify_crc(response) {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Modbus CRC 校验失败",
        ));
    }
    let mut regs = Vec::with_capacity(count as usize);
    for i in 0..count as usize {
        let hi = response[3 + i * 2] as u16;
        let lo = response[4 + i * 2] as u16;
        regs.push((hi << 8) | lo);
    }
    Ok(regs)
}

/// 明文模式下推断 Modbus RTU 帧完整长度；未知帧型返回 None 表示整包透传。
pub fn plain_modbus_frame_length(data: &[u8]) -> Option<usize> {
    if data.len() < 2 {
        return Some(0);
    }
    let function = data[1];
    if function & 0x80 != 0 {
        return Some(5);
    }
    match function {
        0x03 | 0x04 => {
            if data.len() < 3 {
                return Some(0);
            }
            Some(3 + data[2] as usize + 2)
        }
        0x06 => Some(8),
        0x10 => {
            if data.len() < 8 {
                return Some(0);
            }
            if data.len() >= 4 {
                let start = u16::from_be_bytes([data[2], data[3]]);
                if start == super::tlv::REG_21000 && data.len() >= 7 {
                    let byte_count = data[6] as usize;
                    return Some(7 + byte_count + 2);
                }
            }
            Some(8)
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn read_100_50_frame_len() {
        let req = build_read_holding(0, 100, 50);
        assert_eq!(req.len(), 8);
        assert_eq!(req[0], 0);
        assert_eq!(req[1], 0x03);
    }
}
