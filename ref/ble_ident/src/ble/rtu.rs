//! Modbus RTU：只保留读保持寄存器。

use std::io;

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

pub fn parse_read_holding(response: &[u8], slave_id: u8, count: u16) -> io::Result<Vec<u16>> {
    if response.len() < 5 {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "Modbus response is short"));
    }
    if response[0] != slave_id {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "Modbus slave mismatch"));
    }
    if response[1] & 0x80 != 0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("Modbus exception 0x{:02X}", response[2]),
        ));
    }
    if response[1] != 0x03 {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "Not a holding-register read"));
    }
    let byte_count = response[2] as usize;
    let expected_bytes = count as usize * 2;
    if byte_count != expected_bytes || response.len() < 3 + byte_count + 2 {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "Modbus length mismatch"));
    }
    if !verify_crc(response) {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "Modbus CRC failed"));
    }
    let mut regs = Vec::with_capacity(count as usize);
    for i in 0..count as usize {
        let hi = response[3 + i * 2] as u16;
        let lo = response[4 + i * 2] as u16;
        regs.push((hi << 8) | lo);
    }
    Ok(regs)
}

fn crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &byte in data {
        crc ^= u16::from(byte);
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

fn append_crc(mut frame: Vec<u8>) -> Vec<u8> {
    let c = crc16(&frame);
    frame.push(c as u8);
    frame.push((c >> 8) as u8);
    frame
}

fn verify_crc(frame: &[u8]) -> bool {
    if frame.len() < 3 {
        return false;
    }
    let body = &frame[..frame.len() - 2];
    let expected = crc16(body);
    let actual = u16::from(frame[frame.len() - 2]) | (u16::from(frame[frame.len() - 1]) << 8);
    expected == actual
}
