//! HTTP OTA 命令：与 MQTT `00 09` 相同的 JSON 与组帧、本机/设备同网判断。

use std::net::Ipv4Addr;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct LocalIpv4 {
    pub ip: Ipv4Addr,
    pub prefix: u8,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum HttpNetCheck {
    Ok {
        local_ip: Ipv4Addr,
        device_ip: Ipv4Addr,
    },
    WifiDown,
    NoDeviceIp,
    NotSameLan {
        device_ip: Ipv4Addr,
        local_ips: Vec<Ipv4Addr>,
    },
}

pub fn json_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if c.is_control() => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}

/// `sn` 与固件 `snprintf("%s%llu", type, sn)` 一致：机型与序列号直接拼接。
pub fn ota_sn_field(model: &str, sn: &str) -> String {
    format!("{}{}", model.trim(), sn.trim())
}

/// 组 MQTT/BLE HTTP OTA JSON。`model` / `sn` 为界面上的机型与序列号（未拼接）。
pub fn build_http_ota_cmd_json(
    model: &str,
    sn: &str,
    firmware_type: u8,
    file_size: u64,
    version: u32,
    download_url: &str,
    file_md5: &str,
    record_id: &str,
) -> String {
    let model_e = json_escape(model.trim());
    let sn_e = json_escape(&ota_sn_field(model, sn));
    let url_e = json_escape(download_url);
    let md5_e = json_escape(file_md5.trim());
    let rid_e = json_escape(record_id.trim());
    format!(
        concat!(
            "[{{\"sn\":\"{sn}\",\"model\":\"{model}\",\"force\":true,\"details\":[",
            "{{\"firmwareType\":{ty},\"fileSize\":{size},\"version\":\"{ver}\",",
            "\"downloadUrl\":\"{url}\",\"fileMd5\":\"{md5}\",\"encrypted\":false,",
            "\"recordId\":\"{rid}\"}}]}}]"
        ),
        sn = sn_e,
        model = model_e,
        ty = firmware_type,
        size = file_size,
        ver = version,
        url = url_e,
        md5 = md5_e,
        rid = rid_e,
    )
}

/// `00 09` + 大端 JSON 长度 + JSON 字节（不含 NUL）。
pub fn build_http_ota_frame(json: &str) -> Result<Vec<u8>, String> {
    let bytes = json.as_bytes();
    if bytes.is_empty() {
        return Err("OTA JSON 为空".into());
    }
    if bytes.len() > u16::MAX as usize {
        return Err("OTA JSON 过长".into());
    }
    let len = bytes.len() as u16;
    let mut out = Vec::with_capacity(4 + bytes.len());
    out.push(0x00);
    out.push(0x09);
    out.extend_from_slice(&len.to_be_bytes());
    out.extend_from_slice(bytes);
    Ok(out)
}

pub fn parse_ipv4(text: &str) -> Option<Ipv4Addr> {
    let trimmed = text.trim();
    if trimmed.is_empty() || trimmed == "0.0.0.0" || trimmed == "—" {
        return None;
    }
    trimmed.parse().ok()
}

pub fn ipv4_prefix_mask(prefix: u8) -> u32 {
    if prefix == 0 {
        0
    } else if prefix >= 32 {
        u32::MAX
    } else {
        !((1u32 << (32 - prefix)) - 1)
    }
}

pub fn same_subnet(a: Ipv4Addr, b: Ipv4Addr, prefix: u8) -> bool {
    let mask = ipv4_prefix_mask(prefix);
    (u32::from(a) & mask) == (u32::from(b) & mask)
}

pub fn pick_local_ip_for_device(device: Ipv4Addr, ifaces: &[LocalIpv4]) -> Option<Ipv4Addr> {
    ifaces
        .iter()
        .find(|iface| same_subnet(device, iface.ip, iface.prefix))
        .map(|iface| iface.ip)
}

pub fn check_http_lan(wifi_sta: bool, sta_ip: &str, ifaces: &[LocalIpv4]) -> HttpNetCheck {
    if !wifi_sta {
        return HttpNetCheck::WifiDown;
    }
    let Some(device_ip) = parse_ipv4(sta_ip) else {
        return HttpNetCheck::NoDeviceIp;
    };
    if let Some(local_ip) = pick_local_ip_for_device(device_ip, ifaces) {
        HttpNetCheck::Ok {
            local_ip,
            device_ip,
        }
    } else {
        HttpNetCheck::NotSameLan {
            device_ip,
            local_ips: ifaces.iter().map(|i| i.ip).collect(),
        }
    }
}

pub fn format_not_same_lan(device_ip: Ipv4Addr, local_ips: &[Ipv4Addr]) -> String {
    let local = if local_ips.is_empty() {
        "无可用 IPv4".to_string()
    } else {
        local_ips
            .iter()
            .map(ToString::to_string)
            .collect::<Vec<_>>()
            .join("、")
    };
    format!(
        "电脑与设备不在同一网段。设备 IP：{device_ip}；本机：{local}。请把电脑和设备连到同一局域网后再试。"
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn frame_matches_mqtt_header() {
        let json = r#"[{"sn":"EP6001","model":"EP600","force":true}]"#;
        let frame = build_http_ota_frame(json).unwrap();
        assert_eq!(frame[0], 0x00);
        assert_eq!(frame[1], 0x09);
        let len = u16::from_be_bytes([frame[2], frame[3]]) as usize;
        assert_eq!(len, json.len());
        assert_eq!(&frame[4..], json.as_bytes());
    }

    #[test]
    fn json_has_string_version_and_concat_sn() {
        let json = build_http_ota_cmd_json(
            "EP600",
            "2326000001749",
            0,
            1870580,
            100600199,
            "http://192.168.1.10:54321/ab.bin",
            "aabbccddeeff00112233445566778899",
            "local-http-ota",
        );
        assert!(json.starts_with('['));
        assert!(json.contains("\"sn\":\"EP6002326000001749\""));
        assert!(json.contains("\"model\":\"EP600\""));
        assert!(json.contains("\"force\":true"));
        assert!(json.contains("\"firmwareType\":0"));
        assert!(json.contains("\"fileSize\":1870580"));
        assert!(json.contains("\"version\":\"100600199\""));
        assert!(json.contains("\"downloadUrl\":\"http://192.168.1.10:54321/ab.bin\""));
        assert!(!json.contains("\"version\":100600199"));
        let frame = build_http_ota_frame(&json).unwrap();
        let declared = u16::from_be_bytes([frame[2], frame[3]]) as usize;
        assert_eq!(declared, json.len());
    }

    #[test]
    fn subnet_uses_prefix_not_slash24() {
        let a = Ipv4Addr::new(192, 168, 1, 10);
        let b = Ipv4Addr::new(192, 168, 1, 50);
        assert!(same_subnet(a, b, 24));
        assert!(!same_subnet(a, Ipv4Addr::new(192, 168, 2, 1), 24));
        assert!(same_subnet(
            Ipv4Addr::new(10, 0, 0, 1),
            Ipv4Addr::new(10, 1, 2, 3),
            8
        ));
        assert!(same_subnet(a, a, 32));
        assert!(!same_subnet(a, b, 32));
        let ifaces = [
            LocalIpv4 {
                ip: Ipv4Addr::new(10, 0, 0, 2),
                prefix: 24,
            },
            LocalIpv4 {
                ip: Ipv4Addr::new(192, 168, 1, 10),
                prefix: 24,
            },
        ];
        assert_eq!(
            pick_local_ip_for_device(Ipv4Addr::new(192, 168, 1, 80), &ifaces),
            Some(Ipv4Addr::new(192, 168, 1, 10))
        );
        assert!(pick_local_ip_for_device(Ipv4Addr::new(172, 16, 0, 9), &ifaces).is_none());
    }

    #[test]
    fn lan_check_messages() {
        let ifaces = [LocalIpv4 {
            ip: Ipv4Addr::new(192, 168, 1, 10),
            prefix: 24,
        }];
        assert_eq!(
            check_http_lan(false, "192.168.1.50", &ifaces),
            HttpNetCheck::WifiDown
        );
        assert_eq!(check_http_lan(true, "", &ifaces), HttpNetCheck::NoDeviceIp);
        assert_eq!(
            check_http_lan(true, "192.168.1.50", &ifaces),
            HttpNetCheck::Ok {
                local_ip: Ipv4Addr::new(192, 168, 1, 10),
                device_ip: Ipv4Addr::new(192, 168, 1, 50),
            }
        );
        match check_http_lan(true, "10.0.0.8", &ifaces) {
            HttpNetCheck::NotSameLan {
                device_ip,
                local_ips,
            } => {
                assert_eq!(device_ip, Ipv4Addr::new(10, 0, 0, 8));
                assert_eq!(local_ips, vec![Ipv4Addr::new(192, 168, 1, 10)]);
            }
            other => panic!("{other:?}"),
        }
    }
}
