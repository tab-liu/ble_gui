//! 握手完成后的 Modbus 业务包加解密（ECDH 共享密钥 + AES-256-CBC）。

use std::io;

use md5::{Digest, Md5};
use rand::RngCore;

use super::aes::{aes_cbc, zero_pad};

/// 加密 Modbus RTU 明文帧为 BLE 空口业务包。
pub fn encrypt_business_packet(shared_key: &[u8; 32], plain: &[u8]) -> io::Result<Vec<u8>> {
    let padded = zero_pad(plain, 16);
    let mut random = [0u8; 4];
    rand::rngs::OsRng.fill_bytes(&mut random);
    let iv = md5_bytes(&random);
    let cipher = aes_cbc(true, shared_key, &iv, &padded)?;

    let mut packet = Vec::with_capacity(6 + cipher.len());
    packet.push((plain.len() >> 8) as u8);
    packet.push(plain.len() as u8);
    packet.extend_from_slice(&random);
    packet.extend_from_slice(&cipher);
    Ok(packet)
}

/// 解密 BLE 业务包，返回 Modbus RTU 明文帧。
pub fn decrypt_business_packet(shared_key: &[u8; 32], packet: &[u8]) -> io::Result<Vec<u8>> {
    if packet.len() < 22 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "加密业务包长度不足",
        ));
    }
    let plain_length = ((packet[0] as usize) << 8) | packet[1] as usize;
    if plain_length == 0 || plain_length > 4096 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "加密业务包明文长度字段异常",
        ));
    }
    let random = &packet[2..6];
    let cipher = &packet[6..];
    let iv = md5_bytes(random);
    let plain_padded = aes_cbc(false, shared_key, &iv, cipher)?;
    if plain_length > plain_padded.len() {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "加密业务包明文长度超出解密结果",
        ));
    }
    Ok(plain_padded[..plain_length].to_vec())
}

fn md5_bytes(data: &[u8]) -> [u8; 16] {
    let digest = Md5::digest(data);
    let mut out = [0u8; 16];
    out.copy_from_slice(&digest);
    out
}
