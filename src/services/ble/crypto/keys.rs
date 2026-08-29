//! BLUETTI BLE 协议密钥常量（对齐 ref/tool/BLUETTI_BLE_Bridge.cs）。

/// 根 AES 密钥，用于 2A2A 鉴权后会话密钥派生。
pub const ROOT_AES_KEY: [u8; 16] = hex16("459FC535808941F17091E0993EE3E93D");

/// 手机/App ECDSA 私钥（L1），用于 ECDH 回应签名。
pub const PRIVATE_KEY_L1: [u8; 32] = hex32(
    "4F19A16E3E87BDD9BD24D3E5495B88041511943CBC8B969ADE9641D0F56AF337",
);

/// IoT 侧验签公钥（K2）。
pub const PUBLIC_KEY_K2: [u8; 64] = hex64(
    "A73ABF5D2232C8C1C72E68304343C272495E3A8FD6F30EA96DE2F4B3CE60B251\
     EE21AC667CF8A71E18B46B664EAEFFE3C489F24F695B6411DB7E22CCC85A8594",
);

const fn hex16(s: &str) -> [u8; 16] {
    let b = s.as_bytes();
    let mut out = [0u8; 16];
    let mut i = 0;
    while i < 16 {
        out[i] = (hex(b[i * 2]) << 4) | hex(b[i * 2 + 1]);
        i += 1;
    }
    out
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

const fn hex64(s: &str) -> [u8; 64] {
    let b = s.as_bytes();
    let mut out = [0u8; 64];
    let mut i = 0;
    while i < 64 {
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
