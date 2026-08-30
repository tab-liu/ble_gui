//! BLE 下行/上行数据重组：加密业务包与明文 Modbus 帧。

use std::collections::VecDeque;
use std::io;

use crate::services::ble::crypto::decrypt_business_packet;
use crate::services::ble::modbus::plain_modbus_frame_length;

/// 累积 GATT 通知并解包为 Modbus RTU 明文响应。
pub struct ModbusRxAssembler {
    buffer: Vec<u8>,
    encrypted: bool,
    shared_key: Option<[u8; 32]>,
    responses: VecDeque<Vec<u8>>,
}

impl ModbusRxAssembler {
    pub fn new() -> Self {
        Self {
            buffer: Vec::new(),
            encrypted: false,
            shared_key: None,
            responses: VecDeque::new(),
        }
    }

    pub fn set_encrypted(&mut self, shared_key: [u8; 32]) {
        self.encrypted = true;
        self.shared_key = Some(shared_key);
        self.buffer.clear();
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
        self.buffer.extend_from_slice(data);
        loop {
            if self.buffer.len() < 6 {
                break;
            }
            let plain_length =
                ((self.buffer[0] as usize) << 8) | (self.buffer[1] as usize);
            if plain_length == 0 || plain_length > 4096 {
                self.buffer.clear();
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "加密业务包长度字段异常",
                ));
            }
            let encrypted_length = plain_length.div_ceil(16) * 16;
            let total_length = 6 + encrypted_length;
            if self.buffer.len() < total_length {
                break;
            }
            let raw: Vec<u8> = self.buffer.drain(..total_length).collect();
            match decrypt_business_packet(&key, &raw) {
                Ok(plain) => self.enqueue(plain),
                Err(err) => {
                    log::warn!(target: "ble_gui::transport", "业务包解密失败: {err}");
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
