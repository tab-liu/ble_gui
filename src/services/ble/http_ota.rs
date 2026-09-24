//! HTTP OTA：本机提供固件，BLE 下发与 MQTT 相同的 `00 09` JSON，设备 HTTP GET。
//!
//! 下载进度看本机已发字节；刷写/CAN/成功读 Modbus 720（与 BLE 后半段相同）。

use std::net::Ipv4Addr;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use log::{info, warn};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::mpsc::UnboundedSender;

use crate::services::firmware::http_cmd::{
    build_http_ota_cmd_json, build_http_ota_frame, check_http_lan, format_not_same_lan, HttpNetCheck,
};
use crate::services::firmware::lan::list_local_ipv4;
use crate::services::firmware::{HttpOtaJob, SharedOtaLive};
use crate::services::modbus::SharedModbusLive;

use super::modbus::{
    build_read_holding, parse_link_status_regs, parse_sta_ipv4_regs, DEFAULT_SLAVE_ID,
    REG_LINK_STATUS, REG_LINK_STATUS_BLOCK_COUNT,
};
use super::ota::{
    cancelled, dist_started, fail, is_iot_self_upgrade, pick_distribution_slot, publish, send_air,
    succeed, wait_ota_distribution, DistOutcome, OTA_DIST_COUNT, OTA_DIST_REG,
};
use super::poll::{modbus_read, ModbusGate};
use super::protocol::ProtocolSession;

const GET_WAIT: Duration = Duration::from_secs(8);
const IOT_AFTER_DL: Duration = Duration::from_secs(20);
const PROGRESS_TICK: Duration = Duration::from_millis(200);

struct HttpServeState {
    firmware: Arc<[u8]>,
    path: String,
    bytes_sent: AtomicU64,
    got_get: AtomicBool,
    completed: AtomicBool,
    stop: AtomicBool,
}

impl HttpServeState {
    fn new(firmware: Arc<[u8]>, path: String) -> Arc<Self> {
        Arc::new(Self {
            firmware,
            path,
            bytes_sent: AtomicU64::new(0),
            got_get: AtomicBool::new(false),
            completed: AtomicBool::new(false),
            stop: AtomicBool::new(false),
        })
    }

    fn request_stop(&self) {
        self.stop.store(true, Ordering::Release);
    }

    fn stopped(&self) -> bool {
        self.stop.load(Ordering::Acquire)
    }
}

pub async fn run_http_ota(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    gate: &ModbusGate,
    modbus_live: &SharedModbusLive,
    ota: &SharedOtaLive,
    ui: &super::UiRefreshSlot,
    job: HttpOtaJob,
) {
    let encrypted = protocol
        .lock()
        .expect("protocol lock")
        .is_encryption_ready();
    if !encrypted {
        fail(ota, ui, "加密通道尚未完成，不能启动升级");
        return;
    }
    if let Ok(mut g) = ota.lock() {
        g.ble_only = is_iot_self_upgrade(job.firmware_type);
    }

    let slave_id = modbus_live
        .lock()
        .map(|l| l.slave_id)
        .unwrap_or(DEFAULT_SLAVE_ID);
    let file_len = job.firmware.len();
    if file_len == 0 {
        fail(ota, ui, "固件文件为空");
        return;
    }

    let _guard = gate.lock().await;
    publish(ota, ui, "HTTP 升级：确认设备与电脑在同一网络", Some(0), Some(0));
    let (local_ip, device_ip) = match resolve_http_lan(
        protocol,
        write_tx,
        ota,
        ui,
        slave_id,
        job.local_ip,
        job.sta_ip,
    )
    .await
    {
        Some(pair) => pair,
        None => return,
    };

    let bind = (local_ip, 0);
    let listener = match TcpListener::bind(bind).await {
        Ok(l) => l,
        Err(err) => {
            fail(ota, ui, format!("无法在 {ip} 上启动 HTTP 服务：{err}", ip = local_ip));
            return;
        }
    };
    let port = match listener.local_addr() {
        Ok(addr) => addr.port(),
        Err(err) => {
            fail(ota, ui, format!("无法读取 HTTP 监听端口：{err}"));
            return;
        }
    };
    let token = format!("{:08x}.bin", rand::random::<u32>());
    let path = format!("/{token}");
    let url = format!("http://{}:{port}{path}", local_ip);
    let json = build_http_ota_cmd_json(
        &job.model,
        &job.sn,
        job.firmware_type,
        file_len as u64,
        job.version,
        &url,
        &job.md5,
        "local-http-ota",
        job.force,
    );
    let frame = match build_http_ota_frame(&json) {
        Ok(f) => f,
        Err(err) => {
            fail(ota, ui, err);
            return;
        }
    };

    info!(
        target: "ble_gui::ota",
        "HTTP OTA url={url} device={device_ip} type={} version={} size={} json={}B frame={}B",
        job.firmware_type,
        job.version,
        file_len,
        json.len(),
        frame.len(),
    );

    let state = HttpServeState::new(job.firmware.clone(), path);
    let server_state = state.clone();
    let server = tokio::spawn(async move {
        accept_loop(listener, server_state).await;
    });

    publish(ota, ui, "HTTP 升级：已启动本机服务，正在下发升级命令", Some(0), Some(0));
    if send_air(protocol, write_tx, &frame, "HTTP-00-09")
        .await
        .is_err()
    {
        state.request_stop();
        let _ = server.await;
        fail(ota, ui, "蓝牙发送 HTTP 升级命令失败");
        return;
    }

    publish(ota, ui, "等待设备下载固件", Some(0), Some(0));
    let wait_deadline = tokio::time::Instant::now() + GET_WAIT;
    let mut started = false;
    loop {
        if cancelled(ota) {
            state.request_stop();
            let _ = server.await;
            fail(ota, ui, "已停止");
            return;
        }
        let sent = state.bytes_sent.load(Ordering::Acquire) as usize;
        let pc = ((sent as u64 * 100) / file_len.max(1) as u64).min(100) as i32;
        if state.got_get.load(Ordering::Acquire) || state.completed.load(Ordering::Acquire) {
            started = true;
        }
        match modbus_read(
            protocol,
            write_tx,
            build_read_holding(slave_id, OTA_DIST_REG, OTA_DIST_COUNT),
            slave_id,
            OTA_DIST_COUNT,
        )
        .await
        {
            Ok(values) => {
                if dist_started(&values, job.firmware_type) {
                    started = true;
                }
                if let Some(found) = pick_distribution_slot(&values, job.firmware_type, -1) {
                    if found.error_code != 0 {
                        state.request_stop();
                        let _ = server.await;
                        fail(
                            ota,
                            ui,
                            format!("HTTP 升级失败 · 故障码 0x{:02X}", found.error_code),
                        );
                        return;
                    }
                }
            }
            Err(err) => {
                warn!(target: "ble_gui::ota", "HTTP OTA 读 720 失败（等待下载）：{err}");
            }
        }

        if started {
            publish(
                ota,
                ui,
                format!("HTTP 下载：{pc}%"),
                Some(pc),
                Some(0),
            );
        }

        if started && (state.completed.load(Ordering::Acquire) || sent >= file_len) {
            publish(ota, ui, "HTTP 下载完成", Some(100), Some(0));
            break;
        }
        if !started && tokio::time::Instant::now() >= wait_deadline {
            state.request_stop();
            let _ = server.await;
            fail(
                ota,
                ui,
                "设备未开始下载。当前固件可能还不支持蓝牙触发 HTTP 升级，请改用 BLE 升级。也请检查电脑防火墙是否拦截了入站连接。",
            );
            return;
        }
        tokio::time::sleep(PROGRESS_TICK).await;
    }

    if is_iot_self_upgrade(job.firmware_type) {
        finish_iot_after_download(
            protocol,
            write_tx,
            ota,
            ui,
            slave_id,
            job.firmware_type,
            &state,
            server,
        )
        .await;
        return;
    }

    publish(ota, ui, "设备内部传输：等待 IOT 分发进度", Some(100), Some(0));
    match wait_ota_distribution(protocol, write_tx, ota, ui, slave_id, job.firmware_type).await
    {
        DistOutcome::Success { stage: path } => {
            state.request_stop();
            let _ = server.await;
            succeed(ota, ui, format!("升级完成：HTTP 100% · {path} 100%"));
        }
        DistOutcome::Failed { reason } => {
            state.request_stop();
            let _ = server.await;
            fail(ota, ui, reason);
        }
        DistOutcome::Cancelled => {
            state.request_stop();
            let _ = server.await;
            fail(ota, ui, "已停止");
        }
    }
}

async fn resolve_http_lan(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    ota: &SharedOtaLive,
    ui: &super::UiRefreshSlot,
    slave_id: u8,
    cached_local: Ipv4Addr,
    cached_device: Ipv4Addr,
) -> Option<(Ipv4Addr, Ipv4Addr)> {
    let ifaces = list_local_ipv4();
    let mut wifi_sta = !cached_device.is_unspecified();
    let mut sta_ip = if cached_device.is_unspecified() {
        String::new()
    } else {
        cached_device.to_string()
    };
    match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_LINK_STATUS, REG_LINK_STATUS_BLOCK_COUNT),
        slave_id,
        REG_LINK_STATUS_BLOCK_COUNT,
    )
    .await
    {
        Ok(regs) => {
            if regs.len() >= 2 {
                let (sta, _) = parse_link_status_regs(&regs);
                wifi_sta = sta;
            }
            if regs.len() >= 4 {
                sta_ip = parse_sta_ipv4_regs(&regs[2..4]);
            }
        }
        Err(err) => {
            warn!(target: "ble_gui::ota", "HTTP OTA 读链路状态失败：{err}");
            if cached_local.is_unspecified() || cached_device.is_unspecified() {
                fail(
                    ota,
                    ui,
                    "无法读取设备网络状态，请先打开主页确认设备已连接 WiFi",
                );
                return None;
            }
        }
    }
    match check_http_lan(wifi_sta, &sta_ip, &ifaces) {
        HttpNetCheck::Ok {
            local_ip,
            device_ip,
        } => Some((local_ip, device_ip)),
        HttpNetCheck::WifiDown | HttpNetCheck::NoDeviceIp => {
            fail(
                ota,
                ui,
                "设备未连接 WiFi 或还没有 IP，请先配网并与电脑连到同一局域网",
            );
            None
        }
        HttpNetCheck::NotSameLan {
            device_ip,
            local_ips,
        } => {
            fail(ota, ui, format_not_same_lan(device_ip, &local_ips));
            None
        }
    }
}

async fn finish_iot_after_download(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    ota: &SharedOtaLive,
    ui: &super::UiRefreshSlot,
    slave_id: u8,
    firmware_type: u8,
    state: &Arc<HttpServeState>,
    server: tokio::task::JoinHandle<()>,
) {
    let deadline = tokio::time::Instant::now() + IOT_AFTER_DL;
    loop {
        if cancelled(ota) {
            state.request_stop();
            let _ = server.await;
            fail(ota, ui, "已停止");
            return;
        }
        match modbus_read(
            protocol,
            write_tx,
            build_read_holding(slave_id, OTA_DIST_REG, OTA_DIST_COUNT),
            slave_id,
            OTA_DIST_COUNT,
        )
        .await
        {
            Ok(values) => {
                if let Some(found) = pick_distribution_slot(&values, firmware_type, -1) {
                    if found.error_code != 0 {
                        state.request_stop();
                        let _ = server.await;
                        fail(
                            ota,
                            ui,
                            format!("HTTP 升级失败 · 故障码 0x{:02X}", found.error_code),
                        );
                        return;
                    }
                    if found.progress >= 100 {
                        state.request_stop();
                        let _ = server.await;
                        succeed(
                            ota,
                            ui,
                            "升级成功：HTTP 下载完成，设备即将重启".into(),
                        );
                        return;
                    }
                    publish(
                        ota,
                        ui,
                        format!("服务器 → IOT  {}%", found.progress),
                        Some(100),
                        Some(found.progress),
                    );
                }
            }
            Err(err) => {
                warn!(target: "ble_gui::ota", "IOT HTTP 升级后读 720 失败，视为设备重启：{err}");
                state.request_stop();
                let _ = server.await;
                succeed(
                    ota,
                    ui,
                    "升级成功：HTTP 下载完成，设备即将重启".into(),
                );
                return;
            }
        }
        if tokio::time::Instant::now() >= deadline {
            state.request_stop();
            let _ = server.await;
            succeed(
                ota,
                ui,
                "升级成功：HTTP 下载完成，设备即将重启".into(),
            );
            return;
        }
        tokio::time::sleep(Duration::from_secs(1)).await;
    }
}

async fn accept_loop(listener: TcpListener, state: Arc<HttpServeState>) {
    loop {
        if state.stopped() {
            break;
        }
        let accept = tokio::time::timeout(Duration::from_millis(200), listener.accept());
        match accept.await {
            Ok(Ok((stream, peer))) => {
                if state.stopped() {
                    break;
                }
                let state = state.clone();
                tokio::spawn(async move {
                    handle_client(stream, peer.ip(), &state).await;
                });
            }
            Ok(Err(err)) => {
                warn!(target: "ble_gui::ota", "HTTP accept 失败：{err}");
                break;
            }
            Err(_) => {}
        }
    }
}

async fn handle_client(mut stream: TcpStream, peer: std::net::IpAddr, state: &HttpServeState) {
    let mut buf = vec![0u8; 4096];
    let mut got = 0usize;
    let header = loop {
        if state.stopped() {
            return;
        }
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buf[got..])).await {
            Ok(Ok(0)) => return,
            Ok(Ok(n)) => {
                got += n;
                if let Some(pos) = find_header_end(&buf[..got]) {
                    break buf[..pos].to_vec();
                }
                if got >= buf.len() {
                    warn!(target: "ble_gui::ota", "HTTP 请求头过长，来自 {peer}");
                    return;
                }
            }
            Ok(Err(err)) => {
                warn!(target: "ble_gui::ota", "HTTP 读请求失败：{err}");
                return;
            }
            Err(_) => return,
        }
    };
    let req = String::from_utf8_lossy(&header);
    let mut lines = req.split("\r\n");
    let Some(first) = lines.next() else {
        return;
    };
    let mut parts = first.split_whitespace();
    let method = parts.next().unwrap_or("");
    let req_path = parts.next().unwrap_or("");
    if req_path != state.path && req_path.trim_start_matches('/') != state.path.trim_start_matches('/')
    {
        let _ = stream
            .write_all(b"HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
            .await;
        return;
    }
    if method != "GET" && method != "HEAD" {
        let _ = stream
            .write_all(b"HTTP/1.0 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
            .await;
        return;
    }

    state.got_get.store(true, Ordering::Release);
    if let std::net::IpAddr::V4(v4) = peer {
        info!(target: "ble_gui::ota", "HTTP GET {} from {v4}", state.path);
    } else {
        info!(target: "ble_gui::ota", "HTTP GET {} from {peer}", state.path);
    }

    let body = state.firmware.clone();
    let headers = format!(
        "HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
        body.len()
    );
    if stream.write_all(headers.as_bytes()).await.is_err() {
        return;
    }
    if method == "HEAD" {
        return;
    }

    state.bytes_sent.store(0, Ordering::Release);
    const CHUNK: usize = 32 * 1024;
    let mut offset = 0usize;
    while offset < body.len() {
        if state.stopped() {
            return;
        }
        let end = (offset + CHUNK).min(body.len());
        if stream.write_all(&body[offset..end]).await.is_err() {
            return;
        }
        offset = end;
        state.bytes_sent.store(offset as u64, Ordering::Release);
    }
    let _ = stream.flush().await;
    state.completed.store(true, Ordering::Release);
}

fn find_header_end(buf: &[u8]) -> Option<usize> {
    buf.windows(4).position(|w| w == b"\r\n\r\n").map(|i| i + 4)
}
