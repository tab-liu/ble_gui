//! 枚举本机 IPv4 地址及前缀，供 HTTP OTA 判断是否与设备同网段。
//! 使用 `if-addrs`，Windows / macOS / Linux 统一实现。

use std::net::Ipv4Addr;

use super::http_cmd::LocalIpv4;

pub fn list_local_ipv4() -> Vec<LocalIpv4> {
    let Ok(ifaces) = if_addrs::get_if_addrs() else {
        log::warn!(target: "ble_gui::ota", "枚举本机网卡失败");
        return Vec::new();
    };

    let mut out = Vec::new();
    for iface in ifaces {
        if iface.is_loopback() || !iface.is_up() {
            continue;
        }
        let if_addrs::IfAddr::V4(v4) = iface.addr else {
            continue;
        };
        let ip = v4.ip;
        if ip.is_loopback() || ip.is_link_local() || ip.is_unspecified() {
            continue;
        }
        // Windows 上前缀列表偶尔对不上，netmask 会是 0.0.0.0，但 OnLinkPrefixLength 仍有效。
        // 掩码为 0 时若直接丢掉，后连上的 WiFi 地址一直进不了同网判断。
        let prefix = if v4.prefixlen > 0 {
            v4.prefixlen
        } else {
            prefix_len(v4.netmask)
        };
        if prefix == 0 {
            continue;
        }
        out.push(LocalIpv4 { ip, prefix });
    }
    out
}

fn prefix_len(mask: Ipv4Addr) -> u8 {
    mask.octets().iter().map(|b| b.count_ones() as u8).sum()
}
