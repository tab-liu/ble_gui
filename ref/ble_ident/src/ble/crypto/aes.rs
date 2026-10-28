//! AES-CBC：握手 AES-128，业务包 AES-256。

use std::io;

use aes::cipher::{BlockDecryptMut, BlockEncryptMut, KeyIvInit};
use aes::{Aes128, Aes256};
use cbc::{Decryptor, Encryptor};
use cipher::block_padding::{NoPadding, ZeroPadding};

type Aes128Enc = Encryptor<Aes128>;
type Aes128Dec = Decryptor<Aes128>;
type Aes256Enc = Encryptor<Aes256>;
type Aes256Dec = Decryptor<Aes256>;

pub fn aes_cbc(encrypt: bool, key: &[u8], iv: &[u8], data: &[u8]) -> io::Result<Vec<u8>> {
    aes_cbc_with_padding(encrypt, key, iv, data, true)
}

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
            "AES-CBC data must be 16-byte aligned",
        ));
    }
    let iv_arr: [u8; 16] = iv
        .try_into()
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "AES IV must be 16 bytes"))?;

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
            } else if zero_pad_mode {
                let mut buf = data.to_vec();
                Ok(Aes128Dec::new(&key_arr.into(), &iv_arr.into())
                    .decrypt_padded_mut::<ZeroPadding>(&mut buf)
                    .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                    .to_vec())
            } else {
                let mut buf = data.to_vec();
                Ok(Aes128Dec::new(&key_arr.into(), &iv_arr.into())
                    .decrypt_padded_mut::<NoPadding>(&mut buf)
                    .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                    .to_vec())
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
            } else if zero_pad_mode {
                let mut buf = data.to_vec();
                Ok(Aes256Dec::new(&key_arr.into(), &iv_arr.into())
                    .decrypt_padded_mut::<ZeroPadding>(&mut buf)
                    .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                    .to_vec())
            } else {
                let mut buf = data.to_vec();
                Ok(Aes256Dec::new(&key_arr.into(), &iv_arr.into())
                    .decrypt_padded_mut::<NoPadding>(&mut buf)
                    .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?
                    .to_vec())
            }
        }
        _ => Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "AES key must be 16 or 32 bytes",
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
