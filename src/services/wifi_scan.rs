//! 扫描本机附近 WiFi，供配网时选择 SSID。
//! Windows 优先走 `wlanapi` 主动扫描 BSS（`netsh` 缓存经常只剩当前已连接的那一个）。
//! 只保留 2.4GHz 热点。

use std::process::Command;
use std::time::Duration;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NearbyAp {
    pub ssid: String,
    pub signal: String,
    pub band: String,
}

pub fn scan_nearby() -> Result<Vec<NearbyAp>, String> {
    #[cfg(windows)]
    {
        scan_windows()
    }
    #[cfg(not(windows))]
    {
        Err("当前系统不支持扫描附近 WiFi，请手动填写名称".into())
    }
}

fn finalize_list(mut list: Vec<NearbyAp>) -> Vec<NearbyAp> {
    list.sort_by(|a, b| {
        signal_pct(&a.signal)
            .cmp(&signal_pct(&b.signal))
            .reverse()
            .then_with(|| a.ssid.cmp(&b.ssid))
    });
    list.truncate(40);
    list
}

#[cfg(windows)]
fn scan_windows() -> Result<Vec<NearbyAp>, String> {
    match scan_wlanapi() {
        Ok(list) if !list.is_empty() => Ok(finalize_list(list)),
        wlan_result => {
            match (wlan_result, scan_netsh()) {
                (_, Ok(list)) if !list.is_empty() => Ok(finalize_list(list)),
                (Ok(list), _) if !list.is_empty() => Ok(finalize_list(list)),
                (_, Ok(list)) => Ok(list),
                (Err(wlan_err), Err(netsh_err)) => Err(format!("{wlan_err}；{netsh_err}")),
                (Ok(_), Err(e)) => Err(e),
            }
        }
    }
}

#[cfg(windows)]
fn scan_netsh() -> Result<Vec<NearbyAp>, String> {
    use std::os::windows::process::CommandExt;

    const CREATE_NO_WINDOW: u32 = 0x0800_0000;
    let output = Command::new("netsh")
        .args(["wlan", "show", "networks", "mode=bssid"])
        .creation_flags(CREATE_NO_WINDOW)
        .output()
        .map_err(|e| format!("无法执行 netsh: {e}"))?;
    if !output.status.success() {
        let err = decode_bytes(&output.stderr);
        return Err(if err.trim().is_empty() {
            "扫描失败：请确认本机已打开 WLAN".into()
        } else {
            format!("扫描失败: {}", err.trim())
        });
    }
    Ok(parse_netsh_networks(&decode_bytes(&output.stdout)))
}

#[cfg(windows)]
fn scan_wlanapi() -> Result<Vec<NearbyAp>, String> {
    use windows::Win32::Foundation::HANDLE;
    use windows::Win32::NetworkManagement::WiFi::{
        dot11_BSS_type_any, wlan_interface_state_not_ready, WlanCloseHandle, WlanEnumInterfaces,
        WlanFreeMemory, WlanGetNetworkBssList, WlanOpenHandle, WlanScan, WLAN_BSS_LIST,
        WLAN_INTERFACE_INFO_LIST,
    };

    struct WlanClient(HANDLE);
    impl Drop for WlanClient {
        fn drop(&mut self) {
            unsafe {
                let _ = WlanCloseHandle(self.0, None);
            }
        }
    }
    struct WlanMem(*mut core::ffi::c_void);
    impl Drop for WlanMem {
        fn drop(&mut self) {
            if !self.0.is_null() {
                unsafe {
                    WlanFreeMemory(self.0);
                }
            }
        }
    }

    unsafe {
        let mut version = 0u32;
        let mut handle = HANDLE::default();
        let err = WlanOpenHandle(2, None, &mut version, &mut handle);
        if err != 0 {
            return Err(format!("打开 WLAN 失败 ({err})"));
        }
        let client = WlanClient(handle);

        let mut iface_list: *mut WLAN_INTERFACE_INFO_LIST = core::ptr::null_mut();
        let err = WlanEnumInterfaces(client.0, None, &mut iface_list);
        if err != 0 || iface_list.is_null() {
            return Err(format!("枚举无线网卡失败 ({err})"));
        }
        let _iface_mem = WlanMem(iface_list.cast());
        let count = (*iface_list).dwNumberOfItems as usize;
        if count == 0 {
            return Err("未找到无线网卡".into());
        }
        let ifaces = core::slice::from_raw_parts((*iface_list).InterfaceInfo.as_ptr(), count);

        let mut scanned = false;
        for iface in ifaces {
            if iface.isState == wlan_interface_state_not_ready {
                continue;
            }
            let _ = WlanScan(client.0, &iface.InterfaceGuid, None, None, None);
            scanned = true;
        }
        if !scanned {
            return Err("无线网卡未就绪".into());
        }

        // WlanScan 立即返回，等 BSS 列表刷新，否则往往只剩当前已关联的那一个。
        std::thread::sleep(Duration::from_millis(2500));

        let mut merged: Vec<NearbyAp> = Vec::new();
        for iface in ifaces {
            if iface.isState == wlan_interface_state_not_ready {
                continue;
            }
            let mut bss_list: *mut WLAN_BSS_LIST = core::ptr::null_mut();
            let err = WlanGetNetworkBssList(
                client.0,
                &iface.InterfaceGuid,
                None,
                dot11_BSS_type_any,
                false,
                None,
                &mut bss_list,
            );
            if err != 0 || bss_list.is_null() {
                continue;
            }
            let _bss_mem = WlanMem(bss_list.cast());
            let n = (*bss_list).dwNumberOfItems as usize;
            let entries = core::slice::from_raw_parts((*bss_list).wlanBssEntries.as_ptr(), n);
            for entry in entries {
                push_2g_bss(&mut merged, &entry.dot11Ssid, entry.uLinkQuality, entry.ulChCenterFrequency);
            }
        }
        if merged.is_empty() {
            return Err("未扫描到 2.4GHz WiFi".into());
        }
        Ok(merged)
    }
}

#[cfg(windows)]
fn push_2g_bss(out: &mut Vec<NearbyAp>, ssid: &windows::Win32::NetworkManagement::WiFi::DOT11_SSID, quality: u32, freq_khz: u32) {
    if !(2_400_000..=2_500_000).contains(&freq_khz) {
        return;
    }
    let name = ssid_from_dot11(ssid);
    if name.is_empty() {
        return;
    }
    let pct = quality.min(100) as u8;
    let signal = format!("{pct}%");
    if let Some(existing) = out.iter_mut().find(|ap| ap.ssid == name) {
        if signal_pct(&signal) > signal_pct(&existing.signal) {
            existing.signal = signal;
        }
        return;
    }
    out.push(NearbyAp {
        ssid: name,
        signal,
        band: "2.4G".into(),
    });
}

#[cfg(windows)]
fn ssid_from_dot11(ssid: &windows::Win32::NetworkManagement::WiFi::DOT11_SSID) -> String {
    let len = (ssid.uSSIDLength as usize).min(ssid.ucSSID.len());
    let bytes = &ssid.ucSSID[..len];
    if bytes.is_empty() {
        return String::new();
    }
    if let Ok(s) = std::str::from_utf8(bytes) {
        return s.trim().to_string();
    }
    decode_bytes(bytes).trim().to_string()
}

fn decode_bytes(bytes: &[u8]) -> String {
    if let Ok(s) = std::str::from_utf8(bytes) {
        return s.to_string();
    }
    let (cow, _, _) = encoding_rs::GB18030.decode(bytes);
    cow.into_owned()
}

fn signal_pct(text: &str) -> u8 {
    text.trim_end_matches('%')
        .split_whitespace()
        .next()
        .and_then(|s| s.parse().ok())
        .unwrap_or(0)
}

#[derive(Default)]
struct Draft {
    ssid: String,
    signal_2g: Option<u8>,
    signal_any: Option<u8>,
    has_2g: bool,
    has_5g: bool,
    pending_signal: Option<u8>,
}

impl Draft {
    fn take_signal(&mut self) -> Option<u8> {
        self.pending_signal.take()
    }
}

fn parse_netsh_networks(text: &str) -> Vec<NearbyAp> {
    let mut out: Vec<NearbyAp> = Vec::new();
    let mut draft = Draft::default();

    fn flush(out: &mut Vec<NearbyAp>, draft: &mut Draft) {
        let name = draft.ssid.trim().to_string();
        let keep = !name.is_empty() && (draft.has_2g || (!draft.has_2g && !draft.has_5g));
        if keep {
            let pct = draft.signal_2g.or(draft.signal_any);
            let signal = pct.map(|s| format!("{s}%")).unwrap_or_default();
            let band = if draft.has_2g {
                "2.4G".to_string()
            } else {
                String::new()
            };
            if let Some(existing) = out.iter_mut().find(|ap| ap.ssid == name) {
                if signal_pct(&signal) > signal_pct(&existing.signal) {
                    existing.signal = signal;
                }
                if existing.band.is_empty() {
                    existing.band = band;
                }
            } else {
                out.push(NearbyAp {
                    ssid: name,
                    signal,
                    band,
                });
            }
        }
        *draft = Draft::default();
    }

    for raw in text.lines() {
        let line = raw.trim();
        if let Some(name) = ssid_line_value(line) {
            flush(&mut out, &mut draft);
            draft.ssid = name;
            continue;
        }
        if let Some(pct) = signal_line_value(line) {
            draft.pending_signal = Some(pct);
            draft.signal_any = Some(draft.signal_any.map_or(pct, |old| old.max(pct)));
            if draft.has_2g {
                draft.signal_2g = Some(draft.signal_2g.map_or(pct, |old| old.max(pct)));
            }
            continue;
        }
        if let Some(is_2g) = band_or_channel_is_2g(line) {
            let pct = draft.take_signal().or(draft.signal_any);
            if is_2g {
                draft.has_2g = true;
                if let Some(p) = pct {
                    draft.signal_2g = Some(draft.signal_2g.map_or(p, |old| old.max(p)));
                }
            } else {
                draft.has_5g = true;
            }
        }
    }
    flush(&mut out, &mut draft);
    out
}

fn ssid_line_value(line: &str) -> Option<String> {
    let lower = line.to_ascii_lowercase();
    if lower.starts_with("bssid") {
        return None;
    }
    if !lower.starts_with("ssid") {
        return None;
    }
    let rest = line.get(4..)?;
    let value = rest
        .split_once(':')
        .or_else(|| rest.split_once('：'))
        .map(|(_, v)| v)?;
    let name = value.trim().to_string();
    if name.is_empty() {
        return None;
    }
    Some(name)
}

fn signal_line_value(line: &str) -> Option<u8> {
    let lower = line.to_ascii_lowercase();
    let is_signal = lower.starts_with("signal") || line.contains("信号");
    if !is_signal {
        return None;
    }
    let value = line
        .split_once(':')
        .or_else(|| line.split_once('：'))
        .map(|(_, v)| v)?;
    let digits: String = value.chars().filter(|c| c.is_ascii_digit()).collect();
    digits.parse().ok()
}

/// `Some(true)`=2.4G，`Some(false)`=5G/6G，`None`=不是频段行。
fn band_or_channel_is_2g(line: &str) -> Option<bool> {
    let lower = line.to_ascii_lowercase();
    if lower.starts_with("band") || line.contains("频段") || line.contains("频带") {
        let value = line
            .split_once(':')
            .or_else(|| line.split_once('：'))
            .map(|(_, v)| v.to_ascii_lowercase())?;
        if value.contains("2.4") || value.contains("2,4") {
            return Some(true);
        }
        if value.contains("5.") || value.contains("5g") || value.contains("6.") || value.contains("6g")
        {
            return Some(false);
        }
        return None;
    }
    let is_channel = (lower.starts_with("channel") && !lower.contains("bssid"))
        || line.contains("信道")
        || line.contains("频道");
    if !is_channel {
        return None;
    }
    let value = line
        .split_once(':')
        .or_else(|| line.split_once('：'))
        .map(|(_, v)| v)?;
    let digits: String = value.chars().filter(|c| c.is_ascii_digit()).collect();
    let ch: u32 = digits.parse().ok()?;
    if (1..=14).contains(&ch) {
        Some(true)
    } else if ch > 0 {
        Some(false)
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_two_24g_networks() {
        let sample = r#"
SSID 1 : HomeNet
    Network type            : Infrastructure
    BSSID 1                 : aa:bb:cc:dd:ee:ff
         Signal             : 80%
         Channel            : 6
SSID 2 : Cafe
    BSSID 1                 : 11:22:33:44:55:66
         Signal             : 40%
         Channel            : 1
"#;
        let list = parse_netsh_networks(sample);
        assert_eq!(
            list,
            vec![
                NearbyAp {
                    ssid: "HomeNet".into(),
                    signal: "80%".into(),
                    band: "2.4G".into(),
                },
                NearbyAp {
                    ssid: "Cafe".into(),
                    signal: "40%".into(),
                    band: "2.4G".into(),
                },
            ]
        );
    }

    #[test]
    fn drops_5g_only_keeps_24g_of_dual_band() {
        let sample = r#"
SSID 1 : Dual
    BSSID 1                 : aa:bb:cc:dd:ee:01
         Signal             : 90%
         Channel            : 36
    BSSID 2                 : aa:bb:cc:dd:ee:02
         Signal             : 55%
         Channel            : 11
SSID 2 : FiveOnly
    BSSID 1                 : 11:22:33:44:55:66
         Signal             : 70%
         Channel            : 149
"#;
        let list = parse_netsh_networks(sample);
        assert_eq!(
            list,
            vec![NearbyAp {
                ssid: "Dual".into(),
                signal: "55%".into(),
                band: "2.4G".into(),
            }]
        );
    }

    #[test]
    fn chinese_labels_and_fullwidth_colon() {
        let sample = "SSID 1 ：办公室\n     信号             ：72%\n     信道             ：6\nSSID 2 ：客厅\n     信号             ：30%\n     信道             ：11\n";
        let list = parse_netsh_networks(sample);
        assert_eq!(list.len(), 2);
        assert_eq!(list[0].ssid, "办公室");
        assert_eq!(list[0].signal, "72%");
        assert_eq!(list[1].ssid, "客厅");
    }

    #[test]
    fn bssid_line_is_not_ssid() {
        let sample = r#"
SSID 1 : RealNet
BSSID 1                 : aa:bb:cc:dd:ee:ff
         Signal             : 80%
         Channel            : 6
"#;
        let list = parse_netsh_networks(sample);
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].ssid, "RealNet");
    }
}
