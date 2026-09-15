//! 握手完成后的 Modbus 业务包加解密（ECDH 共享密钥 + AES-256-CBC）。

use std::io;

use md5::{Digest, Md5};
use rand::RngCore;

use super::aes::{aes_cbc_raw, zero_pad};

/// 加密 Modbus RTU 明文帧为 BLE 空口业务包。
pub fn encrypt_business_packet(shared_key: &[u8; 32], plain: &[u8]) -> io::Result<Vec<u8>> {
    let padded = zero_pad(plain, 16);
    let mut random = [0u8; 4];
    rand::rngs::OsRng.fill_bytes(&mut random);
    let iv = md5_bytes(&random);
    // 已手写 zero_pad，用 NoPadding，避免再按 ZeroPadding 规则处理。
    let cipher = aes_cbc_raw(true, shared_key, &iv, &padded)?;

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
            format!(
                "加密业务包长度不足: air={}B（至少 22B）",
                packet.len()
            ),
        ));
    }
    let plain_length = ((packet[0] as usize) << 8) | packet[1] as usize;
    if plain_length == 0 || plain_length > 4096 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("加密业务包明文长度字段异常: plain_len={plain_length}"),
        ));
    }
    let random = &packet[2..6];
    let cipher = &packet[6..];
    let expect_cipher = plain_length.div_ceil(16) * 16;
    if cipher.len() != expect_cipher {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!(
                "密文长度与 plain_len 不匹配: plain_len={plain_length} expect_cipher={expect_cipher} got_cipher={}",
                cipher.len()
            ),
        ));
    }
    let iv = md5_bytes(random);
    // 必须用 raw：ZeroPadding 解密会剥尾 0x00，主页 TLV(~191B) CRC/寄存器常以 00 结尾会被误裁。
    let plain_blocks = aes_cbc_raw(false, shared_key, &iv, cipher)?;
    if plain_length > plain_blocks.len() {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!(
                "加密业务包明文长度超出解密结果: plain_len={plain_length} blocks={} air={}B",
                plain_blocks.len(),
                packet.len()
            ),
        ));
    }
    Ok(plain_blocks[..plain_length].to_vec())
}

/// 诊断用：对比 ZeroPadding 误裁与 NoPadding 正确长度（仅失败路径调用）。
pub fn diagnose_business_decrypt(shared_key: &[u8; 32], packet: &[u8]) -> String {
    if packet.len() < 6 {
        return format!("air太短={}B", packet.len());
    }
    let plain_length = ((packet[0] as usize) << 8) | packet[1] as usize;
    let random = &packet[2..6];
    let cipher = &packet[6..];
    let expect_cipher = plain_length.div_ceil(16) * 16;
    let iv = md5_bytes(random);
    let raw_len = match aes_cbc_raw(false, shared_key, &iv, cipher) {
        Ok(b) => format!("{}", b.len()),
        Err(e) => format!("err({e})"),
    };
    let zero_len = match super::aes::aes_cbc(false, shared_key, &iv, cipher) {
        Ok(b) => {
            let trimmed = b.len();
            let would_fail = plain_length > trimmed;
            format!("{trimmed}{}", if would_fail { "/会误判超长" } else { "" })
        }
        Err(e) => format!("err({e})"),
    };
    let tail = if cipher.len() >= 4 {
        format!(
            "{:02X} {:02X} {:02X} {:02X}",
            cipher[cipher.len() - 4],
            cipher[cipher.len() - 3],
            cipher[cipher.len() - 2],
            cipher[cipher.len() - 1]
        )
    } else {
        "-".into()
    };
    let hint = if plain_length == 191 && packet.len() == 198 {
        "疑似主页TLV整包应答(100~149+开关+链路+SSID)"
    } else if plain_length == 8 && packet.len() == 22 {
        "疑似FC10写应答"
    } else {
        "其它业务包"
    };
    format!(
        "plain_len={plain_length} air={} expect_cipher={expect_cipher} cipher={} raw_blocks={raw_len} zero_unpad={zero_len} random={:02X}{:02X}{:02X}{:02X} cipher_tail=[{tail}] hint={hint}",
        packet.len(),
        cipher.len(),
        random[0],
        random[1],
        random[2],
        random[3],
    )
}

fn md5_bytes(data: &[u8]) -> [u8; 16] {
    let digest = Md5::digest(data);
    let mut out = [0u8; 16];
    out.copy_from_slice(&digest);
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn decrypt_keeps_trailing_zeros_in_plaintext() {
        let key = [0x11u8; 32];
        // 191 字节且以 0x00 结尾：旧 ZeroPadding 路径会裁短并失败。
        let mut plain = vec![0xABu8; 191];
        plain[190] = 0x00;
        let air = encrypt_business_packet(&key, &plain).unwrap();
        assert_eq!(air.len(), 198);
        let out = decrypt_business_packet(&key, &air).unwrap();
        assert_eq!(out, plain);
    }
}
