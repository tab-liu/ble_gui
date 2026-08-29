//! BLE 加解密：握手 AES-128 + 业务 AES-256。

mod aes;
mod business;
pub mod keys;

pub use aes::{aes_cbc, trim_zero, zero_pad};
pub use business::{decrypt_business_packet, encrypt_business_packet};
pub use keys::{PRIVATE_KEY_L1, PUBLIC_KEY_K2, ROOT_AES_KEY};
