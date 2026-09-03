//! BLE OTA：OTA Start（寄存器 700）+ XMODEM-1K。
//!
//! - IOT（type=0）：只有蓝牙阶段。EOT ACK 后设备 `iot_ota_end()` 并重启，不再读 720 分发。
//! - 子设备：EOT 后再读 720～768，等 IOT 经 CAN 刷完。
//! - Start 后不等 0x10 应答，等 `'C'`(0x43)
//! - 数据块 1029 字节（STX + 序号 + 反码 + 1024 + CRC16-XMODEM）

use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use log::{info, warn};
use tokio::sync::mpsc::UnboundedSender;

use crate::services::firmware::{
    OtaJob, SharedOtaLive, PHASE_FAILED, PHASE_SUCCESS,
};

use super::modbus::{
    build_read_holding, build_write_multiple, DEFAULT_SLAVE_ID,
};
use super::poll::{modbus_read, ModbusGate};
use super::protocol::ProtocolSession;
use super::transport::{classify_ota_payload, hex_preview};
use crate::services::modbus::SharedModbusLive;

const XMODEM_BLOCK: usize = 1024;
const XMODEM_STX: u8 = 0x02;
const XMODEM_EOT: u8 = 0x04;
const XMODEM_ACK: u8 = 0x06;
const XMODEM_NAK: u8 = 0x15;
const XMODEM_CAN: u8 = 0x18;
const XMODEM_C: u8 = 0x43;
const XMODEM_PAD: u8 = 0x1A;

const OTA_START_REG: u16 = 0x02BC; // 700
const OTA_DIST_REG: u16 = 720;
const OTA_DIST_COUNT: u16 = 49;
const GATT_CHUNK: usize = 244;
const START_ATTEMPTS: u32 = 5;
const PACKET_ATTEMPTS: u32 = 5;
const CTRL_WAIT: Duration = Duration::from_secs(5);
const CTRL_WAIT_IOT_START: Duration = Duration::from_secs(15);
const DIST_POLL: Duration = Duration::from_secs(1);
const DIST_STALL: Duration = Duration::from_secs(90);

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum CtrlResult {
    Match,
    Nak,
    Can,
    Timeout,
}

/// 十进制版本末两位改成 99，与旧上位机一致。设备用 `version/100` 匹配机内软件。
pub fn ota_wire_version(version: u32) -> u32 {
    version / 100 * 100 + 99
}

/// OTA Start 寄存器 705：高字节 = GROUP_*（1 INV / 2 PACK / 3 IOT / 4 LCD / 7 CHARGE）。
pub fn ota_group_word(firmware_type: u8) -> u16 {
    match firmware_type {
        0 => 0x0300,      // DEVICE_IOT → GROUP_IOT
        3..=10 => 0x0200, // BMS / PACK* → GROUP_PACK
        11 | 12 => 0x0400, // HMI → GROUP_LCD
        16 => 0x0700,     // DEVICE_DC_DC → GROUP_CHARGE
        _ => 0x0100,      // ARM / DSP / HUB → GROUP_INV
    }
}

/// type=0 且 Group=IOT 时设备走 `iot_ota_end()` + `esp_restart()`，没有 CAN 分发。
fn is_iot_self_upgrade(firmware_type: u8) -> bool {
    firmware_type == 0
}

/// 设备经 BLE 回的 C/ACK/NAK/CAN 是 1 字节业务包，不能在 Modbus 载荷里扫 0x43。
pub(crate) fn match_ota_control(chunk: &[u8], expected: u8) -> Option<CtrlResult> {
    if chunk.is_empty() || chunk.len() > 4 {
        return None;
    }
    let b = chunk[0];
    if b == expected {
        return Some(CtrlResult::Match);
    }
    if (expected == XMODEM_ACK || expected == XMODEM_C) && b == XMODEM_NAK {
        return Some(CtrlResult::Nak);
    }
    if b == XMODEM_CAN {
        return Some(CtrlResult::Can);
    }
    None
}

pub fn crc16_xmodem(data: &[u8]) -> u16 {
    let mut crc: u16 = 0;
    for &byte in data {
        crc ^= (byte as u16) << 8;
        for _ in 0..8 {
            crc = if crc & 0x8000 != 0 {
                (crc << 1) ^ 0x1021
            } else {
                crc << 1
            };
        }
    }
    crc
}

pub fn build_xmodem_1k_packet(firmware: &[u8], packet_index: usize) -> Vec<u8> {
    let seq = ((packet_index + 1) & 0xFF) as u8;
        let mut packet = vec![XMODEM_PAD; 3 + XMODEM_BLOCK + 2];
    packet[0] = XMODEM_STX;
    packet[1] = seq;
    packet[2] = 0xFF - seq;
    let offset = packet_index * XMODEM_BLOCK;
    if offset < firmware.len() {
        let copy = (firmware.len() - offset).min(XMODEM_BLOCK);
        packet[3..3 + copy].copy_from_slice(&firmware[offset..offset + copy]);
    }
    let crc = crc16_xmodem(&packet[3..3 + XMODEM_BLOCK]);
    packet[3 + XMODEM_BLOCK] = (crc >> 8) as u8;
    packet[3 + XMODEM_BLOCK + 1] = crc as u8;
    packet
}

pub fn build_ota_start_request(slave_id: u8, firmware_type: u8, version: u32, file_len: usize) -> Result<Vec<u8>, String> {
    if file_len == 0 {
        return Err("固件文件为空".into());
    }
    let packet_count = (file_len + 1023) / 1024;
    if packet_count == 0 || packet_count > 65535 {
        return Err("XMODEM 1K 包数量超出协议范围".into());
    }
    let ota_version = ota_wire_version(version);
    let group = ota_group_word(firmware_type);
    Ok(build_write_multiple(
        slave_id,
        OTA_START_REG,
        &[
            0x0001,
            firmware_type as u16,
            (ota_version & 0xFFFF) as u16,
            (ota_version >> 16) as u16,
            packet_count as u16,
            group,
        ],
    ))
}

fn cancelled(ota: &SharedOtaLive) -> bool {
    ota.lock().map(|g| g.cancel).unwrap_or(true)
}

fn publish(
    ota: &SharedOtaLive,
    ui: &super::UiRefreshSlot,
    stage: impl Into<String>,
    pc: Option<i32>,
    device: Option<i32>,
) {
    if let Ok(mut g) = ota.lock() {
        g.stage_text = stage.into();
        if let Some(v) = pc {
            g.pc_percent = v;
        }
        if let Some(v) = device {
            g.device_percent = v;
        }
    }
    if let Ok(hook) = ui.lock() {
        if let Some(cb) = hook.as_ref() {
            cb();
        }
    }
}

fn fail(ota: &SharedOtaLive, ui: &super::UiRefreshSlot, reason: impl Into<String>) {
    let reason = reason.into();
    if let Ok(mut g) = ota.lock() {
        g.running = false;
        g.phase = PHASE_FAILED;
        g.result_text = if g.cancel { "已停止".into() } else { "升级失败".into() };
        g.fail_reason = reason.clone();
        g.stage_text = if g.cancel { "已停止".into() } else { "升级失败".into() };
        g.status_text = reason;
    }
    if let Ok(hook) = ui.lock() {
        if let Some(cb) = hook.as_ref() {
            cb();
        }
    }
}

fn succeed(ota: &SharedOtaLive, ui: &super::UiRefreshSlot, stage: String) {
    if let Ok(mut g) = ota.lock() {
        g.running = false;
        g.phase = PHASE_SUCCESS;
        g.pc_percent = 100;
        g.device_percent = 100;
        g.result_text = "升级成功".into();
        g.fail_reason.clear();
        g.stage_text = stage;
        g.status_text.clear();
    }
    if let Ok(hook) = ui.lock() {
        if let Some(cb) = hook.as_ref() {
            cb();
        }
    }
}

struct OtaRxGuard {
    protocol: Arc<Mutex<ProtocolSession>>,
}

impl OtaRxGuard {
    fn enable(protocol: &Arc<Mutex<ProtocolSession>>) -> Self {
        if let Ok(mut session) = protocol.lock() {
            session.set_ota_rx_diag(true);
        }
        Self {
            protocol: Arc::clone(protocol),
        }
    }
}

impl Drop for OtaRxGuard {
    fn drop(&mut self) {
        if let Ok(mut session) = self.protocol.lock() {
            session.set_ota_rx_diag(false);
        }
    }
}

fn expected_name(expected: u8) -> &'static str {
    match expected {
        XMODEM_C => "C",
        XMODEM_ACK => "ACK",
        XMODEM_NAK => "NAK",
        XMODEM_CAN => "CAN",
        _ => "CTRL",
    }
}

async fn send_air(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    ota: &SharedOtaLive,
    plain: &[u8],
    stage: &str,
) -> Result<(), String> {
    if cancelled(ota) {
        return Err("用户终止".into());
    }
    let air = {
        let session = protocol.lock().expect("protocol lock");
        session
            .wrap_modbus_request(plain)
            .map_err(|e| e.to_string())?
    };
    let chunks = air.len().div_ceil(GATT_CHUNK);
    let verbose = stage.starts_with("START") || stage == "EOT";
    if verbose {
        warn!(
            target: "ble_gui::ota",
            "TX-OTA[{stage}] plain={}B [{}] {} | air={}B chunks={chunks}",
            plain.len(),
            classify_ota_payload(plain),
            hex_preview(plain, 48),
            air.len(),
        );
    }
    write_tx
        .send(air)
        .map_err(|_| "写通道已关闭".to_string())?;
    Ok(())
}

struct WaitStats {
    chunks: u32,
    bytes: usize,
    saw_fc10_start_ack: bool,
    saw_c: bool,
    last_class: &'static str,
}

async fn wait_control(
    protocol: &Arc<Mutex<ProtocolSession>>,
    ota: &SharedOtaLive,
    expected: u8,
    timeout: Duration,
) -> (CtrlResult, WaitStats) {
    let deadline = tokio::time::Instant::now() + timeout;
    let mut stats = WaitStats {
        chunks: 0,
        bytes: 0,
        saw_fc10_start_ack: false,
        saw_c: false,
        last_class: "none",
    };
    loop {
        if cancelled(ota) {
            return (CtrlResult::Timeout, stats);
        }
        loop {
            let chunk = protocol
                .lock()
                .expect("protocol lock")
                .pop_modbus_response();
            let Some(chunk) = chunk else {
                break;
            };
            stats.chunks += 1;
            stats.bytes += chunk.len();
            let class = classify_ota_payload(&chunk);
            stats.last_class = class;
            if class == "FC10-OTA-Start-ACK" {
                stats.saw_fc10_start_ack = true;
            }
            if class == "XMODEM-C" {
                stats.saw_c = true;
            }
            if expected == XMODEM_C || class != "XMODEM-ACK" {
                warn!(
                    target: "ble_gui::ota",
                    "wait {} 扫到 {}B [{}] {}",
                    expected_name(expected),
                    chunk.len(),
                    class,
                    hex_preview(&chunk, 48),
                );
            }
            if let Some(result) = match_ota_control(&chunk, expected) {
                return (result, stats);
            }
        }
        if tokio::time::Instant::now() >= deadline {
            let (pending, preview) = protocol
                .lock()
                .expect("protocol lock")
                .rx_pending_debug();
            warn!(
                target: "ble_gui::ota",
                "wait {} 超时：已解码 {} 包 / {}B last={} fc10_ack={} saw_C={} leftover={}B {}",
                expected_name(expected),
                stats.chunks,
                stats.bytes,
                stats.last_class,
                stats.saw_fc10_start_ack,
                stats.saw_c,
                pending,
                preview,
            );
            return (CtrlResult::Timeout, stats);
        }
        tokio::time::sleep(Duration::from_millis(1)).await;
    }
}

fn dist_path_text(depth: i32) -> &'static str {
    match depth {
        1 => "IOT → 设备",
        2 => "设备 → 子设备",
        3 => "服务器 → IOT",
        _ => "IOT 分发",
    }
}

struct DistProgress {
    progress: i32,
    error_code: i32,
    depth: i32,
    slot: i32,
}

fn pick_distribution_slot(values: &[u16], firmware_type: u8, locked_slot: i32) -> Option<DistProgress> {
    if values.len() < OTA_DIST_COUNT as usize {
        return None;
    }
    let mut selected = locked_slot;
    if !(0..16).contains(&selected) {
        let mut best = -1i32;
        let mut best_score = -1i32;
        for index in 0..16 {
            let base = 1 + index * 3;
            if base + 2 >= values.len() {
                break;
            }
            let state = values[base];
            let file = values[base + 1];
            let pct = values[base + 2];
            let file_type = ((file >> 8) & 0xFF) as u8;
            if file_type != firmware_type {
                continue;
            }
            let active = (state >> 8) & 0xFF;
            let state_depth = state & 0xFF;
            let file_depth = file & 0xFF;
            let progress = ((pct >> 8) & 0xFF) as i32;
            let err = (pct & 0xFF) as i32;
            let depth = if state_depth != 0 { state_depth } else { file_depth };
            let mut score = progress;
            if active == 1 {
                score += 1000;
            }
            if depth == 1 || depth == 2 {
                score += 200;
            }
            if err != 0 {
                score += 100;
            }
            if score > best_score {
                best_score = score;
                best = index as i32;
            }
        }
        selected = best;
    }
    if !(0..16).contains(&selected) {
        return None;
    }
    let offset = 1 + selected as usize * 3;
    let state = values[offset];
    let file = values[offset + 1];
    let pct = values[offset + 2];
    let selected_type = ((file >> 8) & 0xFF) as u8;
    if selected_type != firmware_type && locked_slot < 0 {
        return None;
    }
    let state_depth = (state & 0xFF) as i32;
    let file_depth = (file & 0xFF) as i32;
    Some(DistProgress {
        progress: ((pct >> 8) & 0xFF).min(100) as i32,
        error_code: (pct & 0xFF) as i32,
        depth: if state_depth != 0 { state_depth } else { file_depth },
        slot: selected,
    })
}

pub async fn run_ble_ota(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &UnboundedSender<Vec<u8>>,
    gate: &ModbusGate,
    modbus_live: &SharedModbusLive,
    ota: &SharedOtaLive,
    ui: &super::UiRefreshSlot,
    job: OtaJob,
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
    let packet_total = (file_len + 1023) / 1024;
    let start_req = match build_ota_start_request(slave_id, job.firmware_type, job.version, file_len)
    {
        Ok(r) => r,
        Err(err) => {
            fail(ota, ui, err);
            return;
        }
    };

    info!(
        target: "ble_gui::ota",
        "开始升级 type={} version={} size={} packets={} iot_self={}",
        job.firmware_type,
        job.version,
        file_len,
        packet_total,
        is_iot_self_upgrade(job.firmware_type),
    );

    let wire_version = ota_wire_version(job.version);
    let packet_count = (file_len + 1023) / 1024;
    let group = ota_group_word(job.firmware_type);
    warn!(
        target: "ble_gui::ota",
        "OTA Start 参数 slave={} type={} version={} wire_ver={} size={} packets={} group=0x{group:04X}",
        slave_id,
        job.firmware_type,
        job.version,
        wire_version,
        file_len,
        packet_count,
    );
    warn!(
        target: "ble_gui::ota",
        "OTA Start 明文 {}B {}",
        start_req.len(),
        hex_preview(&start_req, 64),
    );
    if job.firmware_type == 0 && job.version / 100 == 0 {
        warn!(
            target: "ble_gui::ota",
            "IOT 下发 version/100=0，设备 vXmodemCmdCheck 会因版本前缀不匹配直接 return 0，不会发 C",
        );
    }

    let _rx_guard = OtaRxGuard::enable(protocol);
    let _guard = gate.lock().await;
    {
        let mut session = protocol.lock().expect("protocol lock");
        session.clear_modbus_responses();
    }

    publish(
        ota,
        ui,
        "蓝牙传输：发送 OTA Start，等待设备就绪",
        Some(0),
        Some(0),
    );

    let mut got_c = false;
    for attempt in 1..=START_ATTEMPTS {
        if cancelled(ota) {
            fail(ota, ui, "用户终止");
            return;
        }
        {
            let mut session = protocol.lock().expect("protocol lock");
            session.clear_modbus_responses();
        }
        let wait = if job.firmware_type == 0 {
            CTRL_WAIT_IOT_START
        } else {
            CTRL_WAIT
        };
        warn!(
            target: "ble_gui::ota",
            "OTA Start 第 {attempt}/{START_ATTEMPTS} 次发送，等待 C(0x43) 最长 {}s",
            wait.as_secs(),
        );
        if send_air(protocol, write_tx, ota, &start_req, &format!("START-{attempt}")).await.is_err() {
            warn!(target: "ble_gui::ota", "OTA Start 第 {attempt} 次空口发送失败");
            continue;
        }
        let (result, stats) = wait_control(protocol, ota, XMODEM_C, wait).await;
        match result {
            CtrlResult::Match => {
                warn!(
                    target: "ble_gui::ota",
                    "OTA Start 成功：收到 C(0x43)，fc10_ack={}",
                    stats.saw_fc10_start_ack,
                );
                got_c = true;
                break;
            }
            CtrlResult::Nak => {
                warn!(target: "ble_gui::ota", "OTA Start 收到 NAK，重发 ({attempt}/{START_ATTEMPTS})");
            }
            CtrlResult::Can => {
                fail(ota, ui, "设备取消升级（CAN）");
                return;
            }
            CtrlResult::Timeout => {
                let hint = if stats.saw_fc10_start_ack {
                    "已收到 700 的 FC10 ACK，但设备未回 C：vXmodemCmdCheck 可能因 Group/类型/版本前缀不匹配直接 return 0"
                } else if stats.chunks == 0 {
                    "等待期间没有任何已解码业务包：要么设备没回，要么加密重组把包丢掉了"
                } else {
                    "有解密数据但其中没有 C(0x43)"
                };
                warn!(
                    target: "ble_gui::ota",
                    "OTA Start 等待 C 超时 ({attempt}/{START_ATTEMPTS})：{hint}",
                );
            }
        }
    }
    if !got_c {
        fail(
            ota,
            ui,
            if cancelled(ota) {
                "用户终止".to_string()
            } else {
                "OTA Start 失败：未收到设备就绪（C）".to_string()
            },
        );
        return;
    }

    if let Ok(mut session) = protocol.lock() {
        session.set_ota_rx_verbose(false);
    }

    let xfer_start = Instant::now();
    let mut last_ui = Instant::now() - Duration::from_secs(1);
    for packet_index in 0..packet_total {
        if cancelled(ota) {
            fail(ota, ui, "用户终止");
            return;
        }
        let pc = ((packet_index as i64) * 100 / packet_total.max(1) as i64) as i32;
        let done_kb = packet_index + 1;
        let total_kb = packet_total.max(1);
        let stage = format!("蓝牙传输  {done_kb} / {total_kb} KB");
        if packet_index == 0
            || packet_index + 1 == packet_total
            || last_ui.elapsed() >= Duration::from_millis(200)
        {
            publish(ota, ui, stage, Some(pc), Some(0));
            last_ui = Instant::now();
        } else if let Ok(mut g) = ota.lock() {
            g.stage_text = stage;
            g.pc_percent = pc;
            g.device_percent = 0;
        }
        let packet = build_xmodem_1k_packet(&job.firmware, packet_index);
        let mut acked = false;
        for _attempt in 1..=PACKET_ATTEMPTS {
            if cancelled(ota) {
                fail(ota, ui, "用户终止");
                return;
            }
            {
                let mut session = protocol.lock().expect("protocol lock");
                session.clear_modbus_responses();
            }
            if send_air(
                protocol,
                write_tx,
                ota,
                &packet,
                &format!("XMODEM-{}", packet_index + 1),
            )
            .await
            .is_err()
            {
                continue;
            }
            let (result, _) = wait_control(protocol, ota, XMODEM_ACK, CTRL_WAIT).await;
            match result {
                CtrlResult::Match => {
                    acked = true;
                    break;
                }
                CtrlResult::Nak => {
                    warn!(
                        target: "ble_gui::ota",
                        "XMODEM 包 {} NAK，重发",
                        packet_index + 1
                    );
                }
                CtrlResult::Can => {
                    fail(ota, ui, "设备取消升级（CAN）");
                    return;
                }
                CtrlResult::Timeout => {}
            }
        }
        if !acked {
            fail(
                ota,
                ui,
                format!("XMODEM 数据包 {} 未收到 ACK", packet_index + 1),
            );
            return;
        }
        let pc = ((packet_index as i64 + 1) * 100 / packet_total.max(1) as i64) as i32;
        if let Ok(mut g) = ota.lock() {
            g.pc_percent = pc;
        }
        let done_packets = packet_index + 1;
        if done_packets == 1 || done_packets % 32 == 0 || done_packets == packet_total {
            let secs = xfer_start.elapsed().as_secs_f32().max(0.001);
            let kb = (done_packets * XMODEM_BLOCK) as f32 / 1024.0;
            info!(
                target: "ble_gui::ota",
                "XMODEM {done_packets}/{packet_total} 已传 {kb:.0}KB / {secs:.1}s ({:.1} KB/s)",
                kb / secs,
            );
        }
    }

    publish(ota, ui, "蓝牙传输：发送结束符", Some(100), Some(0));
    let mut eot_ok = false;
    for _attempt in 1..=PACKET_ATTEMPTS {
        if cancelled(ota) {
            fail(ota, ui, "用户终止");
            return;
        }
        {
            let mut session = protocol.lock().expect("protocol lock");
            session.clear_modbus_responses();
        }
        if send_air(protocol, write_tx, ota, &[XMODEM_EOT], "EOT")
            .await
            .is_err()
        {
            continue;
        }
        let (result, _) = wait_control(protocol, ota, XMODEM_ACK, CTRL_WAIT).await;
        match result {
            CtrlResult::Match => {
                eot_ok = true;
                break;
            }
            CtrlResult::Can => {
                fail(ota, ui, "设备取消升级（CAN）");
                return;
            }
            _ => {}
        }
    }
    if !eot_ok {
        fail(ota, ui, "蓝牙传输结束符未收到 ACK");
        return;
    }

    drop(_guard);

    if is_iot_self_upgrade(job.firmware_type) {
        succeed(
            ota,
            ui,
            "升级成功：蓝牙传输完成，设备即将重启".to_string(),
        );
        info!(target: "ble_gui::ota", "IOT 自升级完成，等待设备重启断链");
        return;
    }

    publish(
        ota,
        ui,
        "设备内部传输：等待 IOT 分发进度",
        Some(100),
        Some(0),
    );

    let mut locked_slot = -1i32;
    let mut last_progress = -1i32;
    let mut last_depth = -1i32;
    let mut last_slot = -1i32;
    let mut ever_seen = false;
    let mut stall_deadline = tokio::time::Instant::now() + DIST_STALL;

    loop {
        if cancelled(ota) {
            fail(ota, ui, "用户终止");
            return;
        }
        if tokio::time::Instant::now() >= stall_deadline {
            fail(
                ota,
                ui,
                if ever_seen {
                    "设备内部传输进度长时间无变化".to_string()
                } else {
                    "未找到当前固件的分发进度".to_string()
                },
            );
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
                if let Some(found) = pick_distribution_slot(&values, job.firmware_type, locked_slot)
                {
                    ever_seen = true;
                    locked_slot = found.slot;
                    if found.progress != last_progress
                        || found.depth != last_depth
                        || found.slot != last_slot
                    {
                        stall_deadline = tokio::time::Instant::now() + DIST_STALL;
                        last_progress = found.progress;
                        last_depth = found.depth;
                        last_slot = found.slot;
                    }
                    let path = dist_path_text(found.depth);
                    if found.error_code != 0 {
                        fail(
                            ota,
                            ui,
                            format!(
                                "{path} 失败 · 故障码 0x{:02X}",
                                found.error_code
                            ),
                        );
                        return;
                    }
                    publish(
                        ota,
                        ui,
                        format!("{path}  {}%", found.progress),
                        Some(100),
                        Some(found.progress),
                    );
                    if found.progress >= 100 {
                        succeed(ota, ui, format!("升级完成：蓝牙 100% · {path} 100%"));
                        info!(target: "ble_gui::ota", "升级成功 type={}", job.firmware_type);
                        return;
                    }
                } else {
                    publish(
                        ota,
                        ui,
                        format!(
                            "设备内部传输：等待 FileType={} 的分发任务",
                            job.firmware_type
                        ),
                        Some(100),
                        Some(0),
                    );
                }
            }
            Err(err) => {
                warn!(target: "ble_gui::ota", "读分发进度失败，将重试: {err}");
                publish(
                    ota,
                    ui,
                    format!("读取分发进度暂时失败，继续等待：{err}"),
                    None,
                    None,
                );
            }
        }

        tokio::time::sleep(DIST_POLL).await;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crc16_xmodem_123456789() {
        assert_eq!(crc16_xmodem(b"123456789"), 0x31C3);
    }

    #[test]
    fn ota_version_replaces_last_two_digits() {
        assert_eq!(ota_wire_version(0), 99);
        assert_eq!(ota_wire_version(100620109), 100620199);
    }

    #[test]
    fn ota_start_frame_matches_studio_layout() {
        let frame = build_ota_start_request(0, 2, 100620109, 102428).unwrap();
        assert_eq!(frame[0], 0x00);
        assert_eq!(frame[1], 0x10);
        assert_eq!(&frame[2..6], &[0x02, 0xBC, 0x00, 0x06]);
        assert_eq!(frame[6], 0x0C);
        assert_eq!(&frame[7..9], &[0x00, 0x01]);
        assert_eq!(&frame[9..11], &[0x00, 0x02]);
        let ver = ota_wire_version(100620109);
        assert_eq!(frame[11], ((ver >> 8) & 0xFF) as u8);
        assert_eq!(frame[12], (ver & 0xFF) as u8);
        assert_eq!(frame[13], ((ver >> 24) & 0xFF) as u8);
        assert_eq!(frame[14], ((ver >> 16) & 0xFF) as u8);
        let packets = ((102428 + 1023) / 1024) as u16;
        assert_eq!(u16::from_be_bytes([frame[15], frame[16]]), packets);
        assert_eq!(&frame[17..19], &[0x01, 0x00]);
        assert_eq!(frame.len(), 21);
    }

    #[test]
    fn iot_start_uses_group_iot() {
        let frame = build_ota_start_request(0, 0, 100620109, 1_870_580).unwrap();
        assert_eq!(&frame[9..11], &[0x00, 0x00]);
        assert_eq!(&frame[17..19], &[0x03, 0x00]);
        assert_eq!(ota_group_word(0), 0x0300);
        assert_eq!(ota_group_word(2), 0x0100);
    }

    #[test]
    fn control_match_ignores_c_inside_modbus() {
        let leaked = [
            0x00, 0x10, 0x52, 0x08, 0x00, 0x1A, 0x34, 0x00, 0x03, 0x5F, 0xE3, 0x43, 0x71,
        ];
        assert_eq!(match_ota_control(&leaked, XMODEM_C), None);
        assert_eq!(
            match_ota_control(&[XMODEM_C], XMODEM_C),
            Some(CtrlResult::Match)
        );
        assert_eq!(
            match_ota_control(&[XMODEM_NAK], XMODEM_C),
            Some(CtrlResult::Nak)
        );
    }

    #[test]
    fn xmodem_packet_pads_and_sequences() {
        let data = vec![0xAAu8; 10];
        let pkt = build_xmodem_1k_packet(&data, 0);
        assert_eq!(pkt.len(), 1029);
        assert_eq!(pkt[0], 0x02);
        assert_eq!(pkt[1], 1);
        assert_eq!(pkt[2], 0xFE);
        assert_eq!(&pkt[3..13], &data[..]);
        assert_eq!(pkt[13], 0x1A);
        let crc = crc16_xmodem(&pkt[3..1027]);
        assert_eq!(pkt[1027], (crc >> 8) as u8);
        assert_eq!(pkt[1028], crc as u8);
    }

    #[test]
    fn distribution_picks_matching_type() {
        let mut values = vec![0u16; 49];
        values[1] = 0x0101;
        values[2] = 0x0201;
        values[3] = 0x2A00;
        let found = pick_distribution_slot(&values, 2, -1).unwrap();
        assert_eq!(found.slot, 0);
        assert_eq!(found.progress, 42);
        assert_eq!(found.depth, 1);
        assert_eq!(found.error_code, 0);
    }

    #[test]
    fn iot_self_upgrade_has_no_can_stage() {
        assert!(is_iot_self_upgrade(0));
        assert!(!is_iot_self_upgrade(1));
        assert!(!is_iot_self_upgrade(2));
        assert!(!is_iot_self_upgrade(11));
    }
}
