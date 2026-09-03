//! BLE 下行/上行数据重组：加密业务包与明文 Modbus 帧。

use std::collections::VecDeque;
use std::io;

use crate::services::ble::crypto::decrypt_business_packet;
use crate::services::ble::modbus::plain_modbus_frame_length;

const XMODEM_ACK: u8 = 0x06;
const XMODEM_NAK: u8 = 0x15;
const XMODEM_CAN: u8 = 0x18;
const XMODEM_C: u8 = 0x43;

/// 累积 GATT 通知并解包为 Modbus RTU 明文响应。
pub struct ModbusRxAssembler {
    buffer: Vec<u8>,
    encrypted: bool,
    shared_key: Option<[u8; 32]>,
    responses: VecDeque<Vec<u8>>,
    /// OTA 期间允许裸 C/ACK；verbose 仅 Start 等待阶段打 hex。
    ota_passthrough: bool,
    ota_verbose: bool,
}

pub(crate) fn hex_preview(data: &[u8], max: usize) -> String {
    let n = data.len().min(max);
    let mut out = String::with_capacity(n * 3);
    for (i, b) in data.iter().take(n).enumerate() {
        if i > 0 {
            out.push(' ');
        }
        out.push_str(&format!("{b:02X}"));
    }
    if data.len() > max {
        out.push_str(&format!(" …(+{}B)", data.len() - max));
    }
    out
}

pub(crate) fn classify_ota_payload(plain: &[u8]) -> &'static str {
    if plain.is_empty() {
        return "empty";
    }
    if plain.len() <= 4 {
        return match plain[0] {
            XMODEM_C => "XMODEM-C",
            XMODEM_ACK => "XMODEM-ACK",
            XMODEM_NAK => "XMODEM-NAK",
            XMODEM_CAN => "XMODEM-CAN",
            0x04 => "XMODEM-EOT",
            _ => "short-other",
        };
    }
    if plain.len() == 8 && plain[1] == 0x10 && plain[2] == 0x02 && plain[3] == 0xBC {
        return "FC10-OTA-Start-ACK";
    }
    if plain.len() >= 6 && plain[1] == 0x10 && plain[2] == 0x02 && plain[3] == 0xBC {
        return "FC10-OTA-Start";
    }
    if plain.len() >= 2 && plain[1] == 0x10 {
        return "FC10-ACK";
    }
    if plain.len() >= 2 && plain[1] == 0x03 {
        return "FC03-read";
    }
    "other"
}

fn is_raw_ota_control(data: &[u8]) -> bool {
    !data.is_empty()
        && data.len() <= 4
        && matches!(data[0], XMODEM_C | XMODEM_ACK | XMODEM_NAK | XMODEM_CAN)
}

impl ModbusRxAssembler {
    pub fn new() -> Self {
        Self {
            buffer: Vec::new(),
            encrypted: false,
            shared_key: None,
            responses: VecDeque::new(),
            ota_passthrough: false,
            ota_verbose: false,
        }
    }

    pub fn set_encrypted(&mut self, shared_key: [u8; 32]) {
        self.encrypted = true;
        self.shared_key = Some(shared_key);
        self.buffer.clear();
    }

    pub fn set_ota_diag(&mut self, enabled: bool) {
        self.ota_passthrough = enabled;
        self.ota_verbose = enabled;
        if enabled {
            self.buffer.clear();
            self.responses.clear();
        }
    }

    pub fn set_ota_verbose(&mut self, enabled: bool) {
        self.ota_verbose = enabled;
    }

    pub fn pending_debug(&self) -> (usize, String) {
        (self.buffer.len(), hex_preview(&self.buffer, 32))
    }

    pub fn set_plaintext(&mut self) {
        self.encrypted = false;
        self.shared_key = None;
        self.buffer.clear();
        self.responses.clear();
    }

    pub fn push_notification(&mut self, data: &[u8]) -> io::Result<()> {
        if data.is_empty() {
            return Ok(());
        }
        // 对齐旧上位机：OTA 期间设备可能直接回裸 C/ACK/NAK/CAN（1～4 字节），
        // 不能按加密业务包头解析，否则 0x43 会被当成长度字段高字节。
        if self.ota_passthrough && is_raw_ota_control(data) {
            if self.ota_verbose {
                log::warn!(
                    target: "ble_gui::ota",
                    "RX-OTA-CONTROL[raw] {}B [{}] {}",
                    data.len(),
                    classify_ota_payload(data),
                    hex_preview(data, 16),
                );
            }
            self.enqueue(data.to_vec());
            return Ok(());
        }
        if self.encrypted {
            self.push_encrypted(data)?;
        } else {
            self.push_plain(data);
        }
        Ok(())
    }

    pub fn pop_response(&mut self) -> Option<Vec<u8>> {
        self.responses.pop_front()
    }

    /// 发起新 Modbus 事务前清空已解码响应（保留重组缓冲区）。
    pub fn clear_pending_responses(&mut self) {
        self.responses.clear();
    }

    fn enqueue(&mut self, plain: Vec<u8>) {
        if !plain.is_empty() {
            self.responses.push_back(plain);
        }
    }

    fn push_encrypted(&mut self, data: &[u8]) -> io::Result<()> {
        let key = self.shared_key.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少 ECDH 共享密钥")
        })?;
        if self.ota_verbose && data.len() <= 48 {
            log::warn!(
                target: "ble_gui::ota",
                "GATT-ENC-CHUNK {}B pending={}B {}",
                data.len(),
                self.buffer.len(),
                hex_preview(data, 24),
            );
        }
        self.buffer.extend_from_slice(data);
        loop {
            if self.buffer.len() < 6 {
                if self.ota_verbose && !self.buffer.is_empty() {
                    log::warn!(
                        target: "ble_gui::ota",
                        "RX-OTA 重组未满包头 pending={}B {}",
                        self.buffer.len(),
                        hex_preview(&self.buffer, 16),
                    );
                }
                break;
            }
            let plain_length =
                ((self.buffer[0] as usize) << 8) | (self.buffer[1] as usize);
            if plain_length == 0 || plain_length > 4096 {
                log::warn!(
                    target: "ble_gui::ota",
                    "RX-OTA 长度字段异常 plain_len={} pending={}B {}",
                    plain_length,
                    self.buffer.len(),
                    hex_preview(&self.buffer, 48),
                );
                self.buffer.clear();
                if self.ota_passthrough {
                    return Ok(());
                }
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "加密业务包长度字段异常",
                ));
            }
            let encrypted_length = plain_length.div_ceil(16) * 16;
            let total_length = 6 + encrypted_length;
            if self.buffer.len() < total_length {
                if self.ota_verbose {
                    log::warn!(
                        target: "ble_gui::ota",
                        "RX-OTA 重组未满业务包 need={} have={} plain_len={}",
                        total_length,
                        self.buffer.len(),
                        plain_length,
                    );
                }
                break;
            }
            let raw: Vec<u8> = self.buffer.drain(..total_length).collect();
            match decrypt_business_packet(&key, &raw) {
                Ok(plain) => {
                    if self.ota_verbose {
                        log::warn!(
                            target: "ble_gui::ota",
                            "RX-OTA-PLAIN {}B [{}] {}",
                            plain.len(),
                            classify_ota_payload(&plain),
                            hex_preview(&plain, 48),
                        );
                    }
                    self.enqueue(plain);
                }
                Err(err) => {
                    log::warn!(
                        target: "ble_gui::ota",
                        "RX-OTA 业务包解密失败: {err}; air {}B {}",
                        raw.len(),
                        hex_preview(&raw, 24),
                    );
                }
            }
        }
        Ok(())
    }

    fn push_plain(&mut self, data: &[u8]) {
        self.buffer.extend_from_slice(data);
        loop {
            match plain_modbus_frame_length(&self.buffer) {
                Some(0) => break,
                Some(expected) if self.buffer.len() >= expected => {
                    let raw: Vec<u8> = self.buffer.drain(..expected).collect();
                    self.enqueue(raw);
                }
                Some(_) => break,
                None => {
                    let raw = std::mem::take(&mut self.buffer);
                    self.enqueue(raw);
                    break;
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn raw_c_is_not_parsed_as_length_field() {
        let mut rx = ModbusRxAssembler::new();
        rx.set_encrypted([0u8; 32]);
        rx.set_ota_diag(true);
        rx.push_notification(&[0x43]).unwrap();
        assert_eq!(rx.pop_response().as_deref(), Some(&[0x43][..]));
        let (pending, _) = rx.pending_debug();
        assert_eq!(pending, 0);
    }
}
