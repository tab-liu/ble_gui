//! BLUETTI BLE 应用层协议：2A2A 鉴权 + 0086/0007 ECDH + AES-CBC 业务包。
//!
//! 逻辑对齐 ref/tool/BLUETTI_BLE_Bridge.cs 与 ref/dev/main/bluetooth/app_ble.c。

use std::io;

use aes::Aes128;
use cbc::{Decryptor, Encryptor};
use cipher::{block_padding::ZeroPadding, BlockDecryptMut, BlockEncryptMut, KeyIvInit};
use md5::{Digest, Md5};
use p256::ecdh::diffie_hellman;
use p256::ecdsa::{signature::hazmat::PrehashSigner, Signature, SigningKey, VerifyingKey};
use p256::elliptic_curve::sec1::ToEncodedPoint;
use p256::{EncodedPoint, PublicKey, SecretKey};
use rand::rngs::OsRng;
use sha2::{Digest as Sha2Digest, Sha256};

type Aes128CbcEnc = Encryptor<Aes128>;
type Aes128CbcDec = Decryptor<Aes128>;

/// 与 C# 常量一致。
const AES_KEY: [u8; 16] = hex_const("459FC535808941F17091E0993EE3E93D");
const PRIVATE_KEY_L1: [u8; 32] = hex_bytes32(
    "4F19A16E3E87BDD9BD24D3E5495B88041511943CBC8B969ADE9641D0F56AF337",
);
const PUBLIC_KEY_K2: [u8; 64] = hex_bytes64(
    "A73ABF5D2232C8C1C72E68304343C272495E3A8FD6F30EA96DE2F4B3CE60B251EE21AC667CF8A71E18B46B664EAEFFE3C489F24F695B6411DB7E22CCC85A8594",
);

const fn hex_const(s: &str) -> [u8; 16] {
    let bytes = s.as_bytes();
    let mut out = [0u8; 16];
    let mut i = 0;
    while i < 16 {
        let hi = hex_nibble(bytes[i * 2]);
        let lo = hex_nibble(bytes[i * 2 + 1]);
        out[i] = (hi << 4) | lo;
        i += 1;
    }
    out
}

const fn hex_bytes32(s: &str) -> [u8; 32] {
    let bytes = s.as_bytes();
    let mut out = [0u8; 32];
    let mut i = 0;
    while i < 32 {
        let hi = hex_nibble(bytes[i * 2]);
        let lo = hex_nibble(bytes[i * 2 + 1]);
        out[i] = (hi << 4) | lo;
        i += 1;
    }
    out
}

const fn hex_bytes64(s: &str) -> [u8; 64] {
    let bytes = s.as_bytes();
    let mut out = [0u8; 64];
    let mut i = 0;
    while i < 64 {
        let hi = hex_nibble(bytes[i * 2]);
        let lo = hex_nibble(bytes[i * 2 + 1]);
        out[i] = (hi << 4) | lo;
        i += 1;
    }
    out
}

const fn hex_nibble(b: u8) -> u8 {
    match b {
        b'0'..=b'9' => b - b'0',
        b'a'..=b'f' => b - b'a' + 10,
        b'A'..=b'F' => b - b'A' + 10,
        _ => 0,
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HandshakePhase {
    WaitingAuth,
    AuthDone,
    Encrypted,
    Plaintext,
}

pub struct ProtocolSession {
    pub phase: HandshakePhase,
    auth_started: bool,
    auth_succeeded: bool,
    key_exchange_done: bool,
    encryption_ready: bool,
    md5_hash: Option<[u8; 16]>,
    new_aes_key: Option<[u8; 16]>,
    iot_public_key: Option<[u8; 64]>,
    shared_key: Option<[u8; 32]>,
    ephemeral_secret: Option<SecretKey>,
    handshake_acc: Vec<u8>,
    handshake_prefix: Option<&'static str>,
}

impl ProtocolSession {
    pub fn new() -> Self {
        Self {
            phase: HandshakePhase::WaitingAuth,
            auth_started: false,
            auth_succeeded: false,
            key_exchange_done: false,
            encryption_ready: false,
            md5_hash: None,
            new_aes_key: None,
            iot_public_key: None,
            shared_key: None,
            ephemeral_secret: None,
            handshake_acc: Vec::new(),
            handshake_prefix: None,
        }
    }

    pub fn reset(&mut self) {
        *self = Self::new();
    }

    pub fn is_encryption_ready(&self) -> bool {
        self.encryption_ready
    }

    pub fn auth_started(&self) -> bool {
        self.auth_started
    }

    /// 处理 GATT 通知；返回需要写入 FF02 的响应包（若有）。
    pub fn on_notification(&mut self, data: &[u8]) -> io::Result<Option<Vec<u8>>> {
        if data.is_empty() {
            return Ok(None);
        }

        if self.handshake_prefix.is_some() {
            self.handshake_acc.extend_from_slice(data);
            let prefix = self.handshake_prefix.unwrap();
            if is_handshake_packet_complete(prefix, self.handshake_acc.len()) {
                let full = std::mem::take(&mut self.handshake_acc);
                self.handshake_prefix = None;
                return self.dispatch_handshake(prefix, &full);
            }
            return Ok(None);
        }

        let hex = hex::encode(data);
        if hex.starts_with("2a2a01") {
            return self.handle_auth_request(data);
        }
        if hex.starts_with("2a2a03") {
            self.handle_auth_result(data)?;
            return Ok(None);
        }
        if !self.key_exchange_done && hex.starts_with("0086") {
            if !is_handshake_packet_complete("0086", data.len()) {
                self.handshake_prefix = Some("0086");
                self.handshake_acc.extend_from_slice(data);
                return Ok(None);
            }
            return self.handle_key_exchange(data);
        }
        if !self.key_exchange_done && hex.starts_with("0007") {
            if !is_handshake_packet_complete("0007", data.len()) {
                self.handshake_prefix = Some("0007");
                self.handshake_acc.extend_from_slice(data);
                return Ok(None);
            }
            return self.handle_key_exchange_result(data);
        }

        // 业务包留待 Modbus 阶段处理。
        Ok(None)
    }

    fn dispatch_handshake(&mut self, prefix: &str, data: &[u8]) -> io::Result<Option<Vec<u8>>> {
        match prefix {
            "0086" => self.handle_key_exchange(data),
            "0007" => self.handle_key_exchange_result(data),
            _ => Ok(None),
        }
    }

    fn handle_auth_request(&mut self, data: &[u8]) -> io::Result<Option<Vec<u8>>> {
        self.auth_started = true;
        self.phase = HandshakePhase::WaitingAuth;
        if data.len() < 8 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "2A2A01 鉴权请求长度不足",
            ));
        }
        let mut random = [0u8; 4];
        random.copy_from_slice(&data[4..8]);
        random.reverse();

        let md5_hash = md5_bytes(&random);
        let mut new_aes_key = [0u8; 16];
        for i in 0..16 {
            new_aes_key[i] = md5_hash[i] ^ AES_KEY[i];
        }
        self.md5_hash = Some(md5_hash);
        self.new_aes_key = Some(new_aes_key);

        let mut frame = [0u8; 6];
        frame[0] = 0x02;
        frame[1] = 0x04;
        frame[2..6].copy_from_slice(&md5_hash[8..12]);
        let check = additive_checksum(&frame);

        let mut response = [0u8; 10];
        response[0] = 0x2A;
        response[1] = 0x2A;
        response[2..8].copy_from_slice(&frame);
        response[8] = (check >> 8) as u8;
        response[9] = check as u8;

        self.phase = HandshakePhase::WaitingAuth;
        Ok(Some(response.to_vec()))
    }

    fn handle_auth_result(&mut self, data: &[u8]) -> io::Result<()> {
        if data.len() < 5 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "2A2A03 鉴权结果长度不足",
            ));
        }
        self.auth_succeeded = data[4] == 0;
        if self.auth_succeeded {
            self.phase = HandshakePhase::AuthDone;
        } else {
            self.new_aes_key = None;
        }
        Ok(())
    }

    fn handle_key_exchange(&mut self, data: &[u8]) -> io::Result<Option<Vec<u8>>> {
        if !self.auth_succeeded {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "收到 0086 但 2A2A 鉴权尚未完成",
            ));
        }
        let md5_hash = self.md5_hash.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少 MD5 上下文")
        })?;
        let new_aes_key = self.new_aes_key.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少会话 AES 密钥")
        })?;

        let cipher = &data[2..];
        let decrypted = trim_zero(&aes_cbc(false, &new_aes_key, &md5_hash, cipher)?);
        if decrypted.len() < 134 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "0086 解密后长度不足",
            ));
        }
        let payload = &decrypted[4..decrypted.len() - 6];
        if payload.len() < 128 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "0086 有效载荷不足 128 字节",
            ));
        }
        let mut iot_public = [0u8; 64];
        iot_public.copy_from_slice(&payload[..64]);
        let signature = &payload[64..128];

        let mut sign_data = Vec::with_capacity(80);
        sign_data.extend_from_slice(&iot_public);
        sign_data.extend_from_slice(&md5_hash);
        if !verify_raw_signature(&PUBLIC_KEY_K2, signature, &sign_data) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "0086 IoT 公钥签名验证失败",
            ));
        }
        self.iot_public_key = Some(iot_public);
        self.phase = HandshakePhase::AuthDone;
        Ok(Some(self.build_key_exchange_response()?))
    }

    fn build_key_exchange_response(&mut self) -> io::Result<Vec<u8>> {
        let md5_hash = self.md5_hash.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少 MD5 上下文")
        })?;
        let new_aes_key = self.new_aes_key.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少会话 AES 密钥")
        })?;

        let secret = SecretKey::random(&mut OsRng);
        let public = secret.public_key();
        let encoded = public.to_encoded_point(false);
        let pub_bytes = encoded.as_bytes();
        let public_raw = &pub_bytes[1..65];

        self.ephemeral_secret = Some(secret);

        let mut sign_data = Vec::with_capacity(80);
        sign_data.extend_from_slice(public_raw);
        sign_data.extend_from_slice(&md5_hash);
        let signature = sign_raw(&PRIVATE_KEY_L1, &sign_data)?;

        let mut frame_data = Vec::with_capacity(2 + 64 + 64);
        frame_data.push(0x05);
        frame_data.push(0x80);
        frame_data.extend_from_slice(public_raw);
        frame_data.extend_from_slice(&signature);
        let check = additive_checksum(&frame_data);

        let mut response_data = Vec::with_capacity(frame_data.len() + 4);
        response_data.push(0x2A);
        response_data.push(0x2A);
        response_data.extend_from_slice(&frame_data);
        response_data.push((check >> 8) as u8);
        response_data.push(check as u8);

        let encrypted = aes_cbc(
            true,
            &new_aes_key,
            &md5_hash,
            &zero_pad(&response_data, 16),
        )?;

        let mut air = Vec::with_capacity(2 + encrypted.len());
        air.push(0x00);
        air.push(0x86);
        air.extend_from_slice(&encrypted);
        Ok(air)
    }

    fn handle_key_exchange_result(&mut self, data: &[u8]) -> io::Result<Option<Vec<u8>>> {
        let md5_hash = self.md5_hash.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少 MD5 上下文")
        })?;
        let new_aes_key = self.new_aes_key.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少会话 AES 密钥")
        })?;
        let ephemeral = self.ephemeral_secret.take().ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少 ECDH 临时私钥")
        })?;
        let iot_public_raw = self.iot_public_key.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "缺少 IoT 公钥")
        })?;

        let cipher = &data[2..];
        let plain = trim_zero(&aes_cbc(false, &new_aes_key, &md5_hash, cipher)?);
        if plain.len() < 5 || plain[4] != 0 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "0007 密钥协商结果失败",
            ));
        }

        let peer = public_key_from_raw64(&iot_public_raw)?;
        let shared = diffie_hellman(ephemeral.to_nonzero_scalar(), peer.as_affine());
        let shared_bytes = shared.raw_secret_bytes();
        let mut shared_key = [0u8; 32];
        let copy_len = shared_bytes.len().min(32);
        shared_key[32 - copy_len..].copy_from_slice(&shared_bytes[shared_bytes.len() - copy_len..]);

        self.shared_key = Some(shared_key);
        self.key_exchange_done = true;
        self.encryption_ready = true;
        self.phase = HandshakePhase::Encrypted;

        // 与 C# 一致：协商完成后发送 Modbus 探测包。
        let probe_plain = hex::decode("01030001001015C6").unwrap();
        Ok(Some(self.encrypt_business_packet(&probe_plain)?))
    }

    pub fn encrypt_business_packet(&self, plain: &[u8]) -> io::Result<Vec<u8>> {
        let shared_key = self.shared_key.ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "ECDH 共享密钥未建立")
        })?;
        let padded = zero_pad(plain, 16);
        let mut random = [0u8; 4];
        rand::RngCore::fill_bytes(&mut OsRng, &mut random);
        let iv = md5_bytes(&random);
        let cipher = aes_cbc(true, &shared_key, &iv, &padded)?;

        let mut packet = Vec::with_capacity(6 + cipher.len());
        packet.push((plain.len() >> 8) as u8);
        packet.push(plain.len() as u8);
        packet.extend_from_slice(&random);
        packet.extend_from_slice(&cipher);
        Ok(packet)
    }

    pub fn mark_plaintext_mode(&mut self) {
        self.phase = HandshakePhase::Plaintext;
        self.encryption_ready = false;
    }
}

fn is_handshake_packet_complete(prefix: &str, count: usize) -> bool {
    match prefix {
        "0086" => count >= 146 && (count - 2) % 16 == 0,
        "0007" => count >= 18 && (count - 2) % 16 == 0,
        _ => true,
    }
}

fn md5_bytes(data: &[u8]) -> [u8; 16] {
    let digest = Md5::digest(data);
    let mut out = [0u8; 16];
    out.copy_from_slice(&digest);
    out
}

fn additive_checksum(data: &[u8]) -> u16 {
    data.iter().map(|b| *b as u32).sum::<u32>() as u16
}

fn zero_pad(data: &[u8], block_size: usize) -> Vec<u8> {
    let padding = block_size - (data.len() % block_size);
    let mut out = Vec::with_capacity(data.len() + padding);
    out.extend_from_slice(data);
    out.resize(out.len() + padding, 0);
    out
}

fn trim_zero(data: &[u8]) -> Vec<u8> {
    let mut len = data.len();
    while len > 0 && data[len - 1] == 0 {
        len -= 1;
    }
    data[..len].to_vec()
}

fn aes_cbc(encrypt: bool, key: &[u8], iv: &[u8], data: &[u8]) -> io::Result<Vec<u8>> {
    if data.is_empty() || data.len() % 16 != 0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "AES-CBC 数据必须 16 字节对齐",
        ));
    }
    let key_arr: [u8; 16] = key.try_into().map_err(|_| {
        io::Error::new(io::ErrorKind::InvalidData, "AES 密钥长度必须为 16")
    })?;
    let iv_arr: [u8; 16] = iv.try_into().map_err(|_| {
        io::Error::new(io::ErrorKind::InvalidData, "AES IV 长度必须为 16")
    })?;

    if encrypt {
        let mut buf = data.to_vec();
        Aes128CbcEnc::new(&key_arr.into(), &iv_arr.into())
            .encrypt_padded_mut::<ZeroPadding>(&mut buf, data.len())
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?;
        Ok(buf)
    } else {
        let mut buf = data.to_vec();
        Ok(Aes128CbcDec::new(&key_arr.into(), &iv_arr.into())
            .decrypt_padded_mut::<ZeroPadding>(&mut buf)
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
            .to_vec())
    }
}

fn public_key_from_raw64(raw: &[u8; 64]) -> io::Result<PublicKey> {
    let mut encoded = [0u8; 65];
    encoded[0] = 0x04;
    encoded[1..].copy_from_slice(raw);
    EncodedPoint::from_bytes(encoded)
        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))
        .and_then(|point| {
            PublicKey::from_sec1_bytes(point.as_bytes())
                .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))
        })
}

fn sign_raw(private_key: &[u8; 32], data: &[u8]) -> io::Result<[u8; 64]> {
    let signing_key = SigningKey::from_bytes(private_key.into()).map_err(|e| {
        io::Error::new(io::ErrorKind::InvalidData, e.to_string())
    })?;
    let hash = Sha256::digest(data);
    let signature: Signature = signing_key.sign_prehash(&hash).map_err(|e| {
        io::Error::new(io::ErrorKind::InvalidData, e.to_string())
    })?;
    Ok(signature.to_bytes().into())
}

fn verify_raw_signature(public_key_raw: &[u8; 64], signature: &[u8], data: &[u8]) -> bool {
    use p256::ecdsa::signature::hazmat::PrehashVerifier;

    if signature.len() != 64 {
        return false;
    }
    let Ok(public) = public_key_from_raw64(public_key_raw) else {
        return false;
    };
    let verifying_key = VerifyingKey::from(&public);
    let Ok(sig) = Signature::from_bytes(signature.into()) else {
        return false;
    };
    let hash = Sha256::digest(data);
    verifying_key.verify_prehash(&hash, &sig).is_ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn auth_request_roundtrip_structure() {
        let mut session = ProtocolSession::new();
        let req = hex::decode("2A2A010001020304").unwrap();
        let resp = session.handle_auth_request(&req).unwrap().unwrap();
        assert_eq!(resp.len(), 10);
        assert_eq!(resp[0], 0x2A);
        assert_eq!(resp[1], 0x2A);
    }
}
