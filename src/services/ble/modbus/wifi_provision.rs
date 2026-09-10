//! 设备 WiFi 配网寄存器与状态解析。
//!
//! 写入 `12001` 认证 / `12002` SSID / `12018` 密码；轮询 `11018` 链路与 `11108` 当前 SSID。

/// `wifi_sta_auth`（ESP `WIFI_AUTH_*`）。
pub const REG_WIFI_STA_AUTH: u16 = 12001;
/// `wifi_sta_ssid` 32 字节。
pub const REG_WIFI_STA_SSID: u16 = 12002;
pub const REG_WIFI_STA_SSID_COUNT: u16 = 16;
/// `wifi_sta_password` 64 字节。
pub const REG_WIFI_STA_PASSWORD: u16 = 12018;
pub const REG_WIFI_STA_PASSWORD_COUNT: u16 = 32;
/// `sta_enable`：低 8 位 `0xA5` 表示密码高 32 字节有效；bit8 开放式 WiFi。
pub const REG_WIFI_STA_ENABLE: u16 = 12050;
/// `on_off.wifi_enable`（2bit，1=enable）。
pub const REG_WIFI_ON_OFF: u16 = 12161;
/// `link_status` u32：bit0 `wifi_sta`，bit6 `mqtt`。
pub const REG_LINK_STATUS: u16 = 11018;
pub const REG_LINK_STATUS_COUNT: u16 = 2;
/// 当前已关联 SSID。
///
/// 头文件注释写 `11107~11122`，但 `period_report` 使 11045 之后整体后移 1 个寄存器：
/// `11107` 实际是 `wifi_mult_sta_flag`（`ext_wifi_enable_num=2` 时低字节 `0x02`，会显示成方块）。
pub const REG_WIFI_SSID_NOW: u16 = 11108;
pub const REG_WIFI_SSID_NOW_COUNT: u16 = 16;
/// `wifi_disconnect_reasion`。
pub const REG_WIFI_DISCONNECT: u16 = 11040;
/// `sta_ipv4` 4 字节。
pub const REG_STA_IPV4: u16 = 11020;
pub const REG_STA_IPV4_COUNT: u16 = 2;

pub const WIFI_AUTH_OPEN: u16 = 0;
pub const WIFI_AUTH_WPA_WPA2_PSK: u16 = 4;

/// 不与 [`super::BUILTIN_SETTINGS`] 下标冲突的配网轮询项。
pub const WIFI_POLL_LINK: usize = 1000;
pub const WIFI_POLL_SSID_NOW: usize = 1001;
pub const WIFI_POLL_DISCONNECT: usize = 1002;
pub const WIFI_POLL_STA_IP: usize = 1003;

pub const WIFI_SSID_MAX_BYTES: usize = 32;
pub const WIFI_PASSWORD_MAX_BYTES: usize = 64;

pub fn is_wifi_poll_index(item_index: usize) -> bool {
    matches!(
        item_index,
        WIFI_POLL_LINK | WIFI_POLL_SSID_NOW | WIFI_POLL_DISCONNECT | WIFI_POLL_STA_IP
    )
}

pub fn wifi_auth_for_password(password: &str) -> u16 {
    if password.is_empty() {
        WIFI_AUTH_OPEN
    } else {
        WIFI_AUTH_WPA_WPA2_PSK
    }
}

pub fn sta_enable_word(password: &str) -> u16 {
    let mut word = 0u16;
    if password.as_bytes().len() > 32 {
        word |= 0x00A5;
    }
    if password.is_empty() {
        word |= 1 << 8;
    }
    word
}

/// 从整数读回解析 `(wifi_sta, mqtt)`。
pub fn parse_link_status(result: &str) -> (bool, bool) {
    let raw: u64 = result.trim().parse().unwrap_or(0);
    ((raw & 1) != 0, (raw & (1 << 6)) != 0)
}

pub fn parse_disconnect_reason(result: &str) -> u16 {
    result.trim().parse().unwrap_or(0)
}

/// 从整数读回解析 STA IPv4（寄存器内低字节在前）。
pub fn parse_sta_ipv4(result: &str) -> String {
    let Ok(raw) = result.trim().parse::<u64>() else {
        return String::new();
    };
    if raw == 0 {
        return String::new();
    }
    format!(
        "{}.{}.{}.{}",
        raw & 0xFF,
        (raw >> 8) & 0xFF,
        (raw >> 16) & 0xFF,
        (raw >> 24) & 0xFF
    )
}

pub fn disconnect_reason_text(reason: u16) -> Option<&'static str> {
    Some(match reason {
        0 => return None,
        2 => "认证过期",
        3 => "路由器断开",
        8 => "接入点已满",
        15 | 204 => "握手超时（请检查密码）",
        201 => "未找到该 WiFi",
        202 => "认证失败（请检查密码）",
        203 => "关联失败",
        _ => return Some("WiFi 连接失败"),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn link_bits() {
        assert_eq!(parse_link_status("0"), (false, false));
        assert_eq!(parse_link_status("1"), (true, false));
        assert_eq!(parse_link_status("65"), (true, true));
        assert_eq!(parse_link_status("64"), (false, true));
    }

    #[test]
    fn open_wifi_sta_enable() {
        assert_eq!(sta_enable_word(""), 1 << 8);
        assert_eq!(sta_enable_word("short"), 0);
        let long = "a".repeat(33);
        assert_eq!(sta_enable_word(&long), 0xA5);
    }

    #[test]
    fn sta_ipv4_little_endian_regs() {
        // 192.168.1.10 → 寄存器 0xA8C0, 0x0A01 → 合成整数 167880896
        assert_eq!(parse_sta_ipv4("167880896"), "192.168.1.10");
        assert_eq!(parse_sta_ipv4("0"), "");
        assert_eq!(parse_sta_ipv4("失败"), "");
    }
}
