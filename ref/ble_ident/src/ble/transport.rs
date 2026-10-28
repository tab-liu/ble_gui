//! 加密业务包重组。明文 Modbus 不在这里处理。

use std::collections::VecDeque;
use std::io;

use super::crypto::decrypt_business_packet;

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

    pub fn push_notification(&mut self, data: &[u8]) -> io::Result<()> {
        if data.is_empty() || !self.encrypted {
            return Ok(());
        }
        let key = self.shared_key.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "Missing shared key")
        })?;
        self.buffer.extend_from_slice(data);
        loop {
            if self.buffer.len() < 6 {
                break;
            }
            let plain_length = ((self.buffer[0] as usize) << 8) | (self.buffer[1] as usize);
            if plain_length == 0 || plain_length > 4096 {
                self.buffer.clear();
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "Encrypted packet length field is invalid",
                ));
            }
            let total_length = 6 + plain_length.div_ceil(16) * 16;
            if self.buffer.len() < total_length {
                break;
            }
            let raw: Vec<u8> = self.buffer.drain(..total_length).collect();
            match decrypt_business_packet(&key, &raw) {
                Ok(plain) => {
                    if !plain.is_empty() {
                        self.responses.push_back(plain);
                    }
                }
                Err(err) => {
                    log::warn!(target: "ble_ident::transport", "decrypt failed: {err}");
                }
            }
        }
        Ok(())
    }

    pub fn pop_response(&mut self) -> Option<Vec<u8>> {
        self.responses.pop_front()
    }

    pub fn clear_pending_responses(&mut self) {
        self.responses.clear();
    }
}
