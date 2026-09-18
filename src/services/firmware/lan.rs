//! 枚举本机 IPv4 地址及前缀，供 HTTP OTA 判断是否与设备同网段。

use super::http_cmd::LocalIpv4;

#[cfg(windows)]
pub fn list_local_ipv4() -> Vec<LocalIpv4> {
    windows_list_local_ipv4()
}

#[cfg(not(windows))]
pub fn list_local_ipv4() -> Vec<LocalIpv4> {
    Vec::new()
}

#[cfg(windows)]
fn windows_list_local_ipv4() -> Vec<LocalIpv4> {
    use std::net::Ipv4Addr;

    use windows::Win32::NetworkManagement::IpHelper::{
        GetAdaptersAddresses, GAA_FLAG_SKIP_ANYCAST, GAA_FLAG_SKIP_DNS_SERVER,
        GAA_FLAG_SKIP_MULTICAST, IF_TYPE_SOFTWARE_LOOPBACK, IP_ADAPTER_ADDRESSES_LH,
    };
    use windows::Win32::NetworkManagement::Ndis::IfOperStatusUp;
    use windows::Win32::Networking::WinSock::{AF_INET, SOCKADDR_IN};

    const ERROR_BUFFER_OVERFLOW: u32 = 111;
    const WORKING_BUFFER_SIZE: u32 = 16 * 1024;

unsafe {
    let mut size = WORKING_BUFFER_SIZE;
    let mut buf = vec![0u8; size as usize];
    let flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    let mut ret = GetAdaptersAddresses(
        AF_INET.0 as u32,
        flags,
        None,
        Some(buf.as_mut_ptr().cast::<IP_ADAPTER_ADDRESSES_LH>()),
        &mut size,
    );
    if ret == ERROR_BUFFER_OVERFLOW {
        buf.resize(size as usize, 0);
        ret = GetAdaptersAddresses(
            AF_INET.0 as u32,
            flags,
            None,
            Some(buf.as_mut_ptr().cast::<IP_ADAPTER_ADDRESSES_LH>()),
            &mut size,
        );
    }
    if ret != 0 {
        log::warn!(target: "ble_gui::ota", "GetAdaptersAddresses 失败：{ret}");
        return Vec::new();
    }

    let mut out = Vec::new();
    let mut adapter = buf.as_ptr().cast::<IP_ADAPTER_ADDRESSES_LH>();
    while !adapter.is_null() {
        let item = &*adapter;
        if item.IfType != IF_TYPE_SOFTWARE_LOOPBACK && item.OperStatus == IfOperStatusUp {
            let mut unicast = item.FirstUnicastAddress;
            while !unicast.is_null() {
                let ua = &*unicast;
                let sa = ua.Address.lpSockaddr;
                if !sa.is_null() && (*sa).sa_family == AF_INET {
                    let sin = sa.cast::<SOCKADDR_IN>();
                    let s_addr = (*sin).sin_addr.S_un.S_addr;
                    let ip = Ipv4Addr::from(s_addr.to_ne_bytes());
                    let prefix = ua.OnLinkPrefixLength;
                    if !ip.is_loopback() && !ip.is_link_local() && !ip.is_unspecified() && prefix > 0
                    {
                        out.push(LocalIpv4 { ip, prefix });
                    }
                }
                unicast = ua.Next;
            }
        }
        adapter = item.Next;
    }
    out
}
}
