//! 安全码封包：X25519 ECIES。明文是版本、长度、十进制安全码、16 字节随机数。
//!
//! 长期公钥编进本模块。对应私钥只存在于 `decode` 程序，不从这里引用。

use aes_gcm::aead::{Aead, KeyInit};
use aes_gcm::{Aes256Gcm, Nonce};
use base64::{engine::general_purpose::STANDARD, Engine};
use hkdf::Hkdf;
use rand::rngs::OsRng;
use rand::RngCore;
use sha2::Sha256;
use x25519_dalek::{EphemeralSecret, PublicKey, StaticSecret};

/// 扫描程序使用的长期公钥。
pub const PUBLIC_KEY: [u8; 32] = hex32("88cf35c3c9e48286ca0e06d7904907ce1fb8ea9cc8cbedcd9343ba634f244426");

const HKDF_INFO: &[u8] = b"ble_ident-safe-v1";
const VERSION: u8 = 1;
const RANDOM_LEN: usize = 16;

/// 把安全码封成标准 Base64。同一个码每次结果不同。
pub fn seal(safe_code: &str) -> Result<String, String> {
    if safe_code.is_empty() || safe_code.len() > 20 || !safe_code.bytes().all(|b| b.is_ascii_digit()) {
        return Err("Safe code is not a decimal string".into());
    }

    let ephemeral = EphemeralSecret::random_from_rng(OsRng);
    let ephemeral_pub = PublicKey::from(&ephemeral);
    let recipient = PublicKey::from(PUBLIC_KEY);
    let shared = ephemeral.diffie_hellman(&recipient);
    let key = derive_key(shared.as_bytes());

    let mut nonce = [0u8; 12];
    OsRng.fill_bytes(&mut nonce);
    let mut random = [0u8; RANDOM_LEN];
    OsRng.fill_bytes(&mut random);

    let mut plain = Vec::with_capacity(2 + safe_code.len() + RANDOM_LEN);
    plain.push(VERSION);
    plain.push(safe_code.len() as u8);
    plain.extend_from_slice(safe_code.as_bytes());
    plain.extend_from_slice(&random);

    let cipher = Aes256Gcm::new_from_slice(&key).map_err(|e| e.to_string())?;
    let ct = cipher
        .encrypt(Nonce::from_slice(&nonce), plain.as_ref())
        .map_err(|_| "Encrypt failed".to_string())?;

    let mut blob = Vec::with_capacity(32 + nonce.len() + ct.len());
    blob.extend_from_slice(ephemeral_pub.as_bytes());
    blob.extend_from_slice(&nonce);
    blob.extend_from_slice(&ct);
    Ok(STANDARD.encode(blob))
}

/// 用调用方传入的私钥解开 Base64 密文，只返回安全码。
pub fn open(private_key: &[u8; 32], encoded: &str) -> Result<String, String> {
    let compact: String = encoded.chars().filter(|c| !c.is_whitespace()).collect();
    if compact.is_empty() {
        return Err("Empty ciphertext".into());
    }
    let blob = STANDARD
        .decode(compact)
        .map_err(|_| "Invalid ciphertext".to_string())?;
    if blob.len() < 32 + 12 + 16 {
        return Err("Invalid ciphertext".into());
    }
    let ephemeral_pub = PublicKey::from(
        <[u8; 32]>::try_from(&blob[..32]).map_err(|_| "Invalid ciphertext".to_string())?,
    );
    let nonce = &blob[32..44];
    let ct = &blob[44..];

    let secret = StaticSecret::from(*private_key);
    let shared = secret.diffie_hellman(&ephemeral_pub);
    let key = derive_key(shared.as_bytes());
    let cipher = Aes256Gcm::new_from_slice(&key).map_err(|e| e.to_string())?;
    let plain = cipher
        .decrypt(Nonce::from_slice(nonce), ct)
        .map_err(|_| "Invalid ciphertext".to_string())?;

    if plain.len() < 2 + RANDOM_LEN || plain[0] != VERSION {
        return Err("Invalid ciphertext".into());
    }
    let n = plain[1] as usize;
    if plain.len() != 2 + n + RANDOM_LEN || n == 0 || n > 20 {
        return Err("Invalid ciphertext".into());
    }
    let code = &plain[2..2 + n];
    if !code.iter().all(|b| b.is_ascii_digit()) {
        return Err("Invalid ciphertext".into());
    }
    std::str::from_utf8(code)
        .map(|s| s.to_string())
        .map_err(|_| "Invalid ciphertext".to_string())
}

fn derive_key(shared: &[u8; 32]) -> [u8; 32] {
    let hk = Hkdf::<Sha256>::new(None, shared);
    let mut key = [0u8; 32];
    hk.expand(HKDF_INFO, &mut key)
        .expect("HKDF expand length is valid");
    key
}

const fn hex32(s: &str) -> [u8; 32] {
    let b = s.as_bytes();
    let mut out = [0u8; 32];
    let mut i = 0;
    while i < 32 {
        out[i] = (hex(b[i * 2]) << 4) | hex(b[i * 2 + 1]);
        i += 1;
    }
    out
}

const fn hex(c: u8) -> u8 {
    match c {
        b'0'..=b'9' => c - b'0',
        b'a'..=b'f' => c - b'a' + 10,
        b'A'..=b'F' => c - b'A' + 10,
        _ => 0,
    }
}
