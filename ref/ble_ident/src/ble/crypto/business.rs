//! 握手完成后的 Modbus 业务包（ECDH 共享密钥 + AES-256-CBC）。

use std::io;

use md5::{Digest, Md5};
use rand::RngCore;

use super::aes::{aes_cbc_raw, zero_pad};

pub fn encrypt_business_packet(shared_key: &[u8; 32], plain: &[u8]) -> io::Result<Vec<u8>> {
    let padded = zero_pad(plain, 16);
    let mut random = [0u8; 4];
    rand::rngs::OsRng.fill_bytes(&mut random);
    let iv = md5_bytes(&random);
    let cipher = aes_cbc_raw(true, shared_key, &iv, &padded)?;

    let mut packet = Vec::with_capacity(6 + cipher.len());
    packet.push((plain.len() >> 8) as u8);
    packet.push(plain.len() as u8);
    packet.extend_from_slice(&random);
    packet.extend_from_slice(&cipher);
    Ok(packet)
}

pub fn decrypt_business_packet(shared_key: &[u8; 32], packet: &[u8]) -> io::Result<Vec<u8>> {
    if packet.len() < 22 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Encrypted packet is too short",
        ));
    }
    let plain_length = ((packet[0] as usize) << 8) | packet[1] as usize;
    if plain_length == 0 || plain_length > 4096 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Encrypted packet length field is invalid",
        ));
    }
    let random = &packet[2..6];
    let cipher = &packet[6..];
    let expect_cipher = plain_length.div_ceil(16) * 16;
    if cipher.len() != expect_cipher {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Cipher length does not match plain_len",
        ));
    }
    let iv = md5_bytes(random);
    let plain_blocks = aes_cbc_raw(false, shared_key, &iv, cipher)?;
    if plain_length > plain_blocks.len() {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Plain length exceeds decrypted blocks",
        ));
    }
    Ok(plain_blocks[..plain_length].to_vec())
}

fn md5_bytes(data: &[u8]) -> [u8; 16] {
    let digest = Md5::digest(data);
    let mut out = [0u8; 16];
    out.copy_from_slice(&digest);
    out
}
