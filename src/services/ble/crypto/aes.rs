//! AES-CBC 加解密：握手阶段 AES-128，业务阶段 AES-256。

use std::io;

use aes::cipher::{BlockDecryptMut, BlockEncryptMut, KeyIvInit};
use aes::{Aes128, Aes256};
use cipher::block_padding::{NoPadding, ZeroPadding};
use cbc::{Decryptor, Encryptor};

type Aes128Enc = Encryptor<Aes128>;
type Aes128Dec = Decryptor<Aes128>;
type Aes256Enc = Encryptor<Aes256>;
type Aes256Dec = Decryptor<Aes256>;

/// AES-CBC 加解密；密钥长度 16 → AES-128，32 → AES-256。
/// 解密使用 ZeroPadding（会剥掉末尾 0x00），仅适合握手等已约定可 trim 的场景。
pub fn aes_cbc(encrypt: bool, key: &[u8], iv: &[u8], data: &[u8]) -> io::Result<Vec<u8>> {
    aes_cbc_with_padding(encrypt, key, iv, data, true)
}

/// 业务包专用：加解密均按整块处理，不剥尾零。
/// 明文真实长度由包头 `plain_len` 决定；Modbus 帧尾字节常为 0x00，ZeroPadding 会误裁。
pub fn aes_cbc_raw(encrypt: bool, key: &[u8], iv: &[u8], data: &[u8]) -> io::Result<Vec<u8>> {
    aes_cbc_with_padding(encrypt, key, iv, data, false)
}

fn aes_cbc_with_padding(
    encrypt: bool,
    key: &[u8],
    iv: &[u8],
    data: &[u8],
    zero_pad_mode: bool,
) -> io::Result<Vec<u8>> {
    if data.is_empty() || data.len() % 16 != 0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "AES-CBC 数据必须 16 字节对齐",
        ));
    }
    let iv_arr: [u8; 16] = iv.try_into().map_err(|_| {
        io::Error::new(io::ErrorKind::InvalidData, "AES IV 长度必须为 16")
    })?;

    match key.len() {
        16 => {
            let key_arr: [u8; 16] = key.try_into().unwrap();
            if encrypt {
                let mut buf = data.to_vec();
                if zero_pad_mode {
                    Aes128Enc::new(&key_arr.into(), &iv_arr.into())
                        .encrypt_padded_mut::<ZeroPadding>(&mut buf, data.len())
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?;
                } else {
                    Aes128Enc::new(&key_arr.into(), &iv_arr.into())
                        .encrypt_padded_mut::<NoPadding>(&mut buf, data.len())
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?;
                }
                Ok(buf)
            } else {
                let mut buf = data.to_vec();
                if zero_pad_mode {
                    Ok(Aes128Dec::new(&key_arr.into(), &iv_arr.into())
                        .decrypt_padded_mut::<ZeroPadding>(&mut buf)
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                        .to_vec())
                } else {
                    Ok(Aes128Dec::new(&key_arr.into(), &iv_arr.into())
                        .decrypt_padded_mut::<NoPadding>(&mut buf)
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                        .to_vec())
                }
            }
        }
        32 => {
            let key_arr: [u8; 32] = key.try_into().unwrap();
            if encrypt {
                let mut buf = data.to_vec();
                if zero_pad_mode {
                    Aes256Enc::new(&key_arr.into(), &iv_arr.into())
                        .encrypt_padded_mut::<ZeroPadding>(&mut buf, data.len())
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?;
                } else {
                    Aes256Enc::new(&key_arr.into(), &iv_arr.into())
                        .encrypt_padded_mut::<NoPadding>(&mut buf, data.len())
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?;
                }
                Ok(buf)
            } else {
                let mut buf = data.to_vec();
                if zero_pad_mode {
                    Ok(Aes256Dec::new(&key_arr.into(), &iv_arr.into())
                        .decrypt_padded_mut::<ZeroPadding>(&mut buf)
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                        .to_vec())
                } else {
                    Ok(Aes256Dec::new(&key_arr.into(), &iv_arr.into())
                        .decrypt_padded_mut::<NoPadding>(&mut buf)
                        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                        .to_vec())
                }
            }
        }
        _ => Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "AES 密钥长度必须为 16 或 32",
        )),
    }
}

pub fn zero_pad(data: &[u8], block_size: usize) -> Vec<u8> {
    let padding = block_size - (data.len() % block_size);
    let mut out = Vec::with_capacity(data.len() + padding);
    out.extend_from_slice(data);
    out.resize(out.len() + padding, 0);
    out
}

pub fn trim_zero(data: &[u8]) -> Vec<u8> {
    let mut len = data.len();
    while len > 0 && data[len - 1] == 0 {
        len -= 1;
    }
    data[..len].to_vec()
}
