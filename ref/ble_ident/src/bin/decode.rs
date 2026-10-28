//! 粘贴安全码密文，用私钥解开并只显示安全码。

#![windows_subsystem = "windows"]

/// 安全码封包的长期私钥。扫描程序里没有这段数据。
const PRIVATE_KEY: [u8; 32] =
    hex32("485c4aa7fc0cea4d8ae3acdc8611c43077d83ce044c5e42462d9ee3d29ccc650");

include!(concat!(env!("OUT_DIR"), "/decode_ui.rs"));

fn main() -> Result<(), slint::PlatformError> {
    let ui = DecodeWindow::new()?;
    let ui_handle = ui.as_weak();
    ui.on_decrypt(move || {
        let Some(ui) = ui_handle.upgrade() else {
            return;
        };
        let cipher = ui.get_cipher().to_string();
        if cipher.trim().is_empty() {
            ui.set_plain("—".into());
            ui.set_status("请先粘贴密文".into());
            return;
        }
        match ble_ident::open(&PRIVATE_KEY, &cipher) {
            Ok(code) => {
                ui.set_plain(code.into());
                ui.set_status("已解密".into());
            }
            Err(_) => {
                ui.set_plain("—".into());
                ui.set_status("密文无效".into());
            }
        }
    });
    ui.run()
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

#[cfg(test)]
mod tests {
    use super::PRIVATE_KEY;

    #[test]
    fn sealed_code_roundtrips_and_changes() {
        let first = ble_ident::seal("12345678901234567890").unwrap();
        let second = ble_ident::seal("12345678901234567890").unwrap();
        assert_ne!(first, second);
        assert_eq!(
            ble_ident::open(&PRIVATE_KEY, &first).unwrap(),
            "12345678901234567890"
        );
        assert!(ble_ident::open(&PRIVATE_KEY, "not-base64").is_err());
    }
}
