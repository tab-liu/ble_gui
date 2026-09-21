//! Modbus 传输原语：仪表板轮询、能力探测、保持寄存器读写。
//!
//! - 主页：SOC@102 + 功率@140～147 + 2011/2012 开关（标准读或 TLV）
//! - 写：FC06/FC10；[`write_holding_registers`] 支持单 bit 或
//!   [`super::modbus::RegisterFieldPatch`] 多位域 RMW
//! - 前台「该轮询什么」由 [`super::poll_executor`] 按策略调用本模块

use std::io;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use log::{debug, info, warn};

use crate::services::modbus::{ModbusReadMode, SharedModbusLive};

use super::modbus::{
    build_read_holding, build_write_multiple, build_write_single, iot_status_supports_tlv,
    is_fc10_write_ack, merge_control_states, parse_ascii_regs, parse_dashboard_soc_and_power,
    parse_device_info, parse_iot_identity, parse_iot_software_ver, parse_iot_type,
    parse_link_status_regs, parse_read_holding, parse_sta_ipv4_regs, parse_sta_rssi,
    parse_tlv_response_packet, parse_tlv_read_units, parse_whole_device, describe_tlv_units,
    format_regs_hex, tlv_register_values, TlReadSpec, TlvPacketCollector, DEFAULT_SLAVE_ID,
    IOT_CLOUD_DNS_REGISTER, IOT_CLOUD_DNS_REGISTER_COUNT, MODBUS_TIMEOUT_MS, REG_21000,
    REG_AC_OUTPUT, REG_DEVICE_INFO_COUNT, REG_DEVICE_INFO_START, REG_IOT_IDENTITY_COUNT,
    REG_IOT_INFO_COUNT, REG_IOT_INFO_START, REG_IOT_STATUS, REG_LINK_STATUS,
    REG_LINK_STATUS_BLOCK_COUNT, REG_POWER_COUNT, REG_POWER_START, REG_SOC, REG_SOC_COUNT,
    REG_STA_RSSI, REG_WHOLE_DEVICE_COUNT, REG_WHOLE_DEVICE_START, REG_WIFI_SSID_NOW,
    REG_WIFI_SSID_NOW_COUNT, REG_WIFI_STA_PASSWORD, REG_WIFI_STA_PASSWORD_COUNT,
};

/// TLV：收到 ACK / 任一数据分包后重置；无进展超过此时长则失败。
const MODBUS_TLV_IDLE_TIMEOUT_MS: u64 = 1000;
/// TLV：整次组合读硬上限（防断续永远凑不齐）。
const MODBUS_TLV_TOTAL_TIMEOUT_MS: u64 = 8000;

fn hex_preview(data: &[u8], max: usize) -> String {
    let take = data.len().min(max);
    let head = hex::encode(&data[..take]);
    if data.len() > max {
        format!("{head}…(+{}B)", data.len() - take)
    } else {
        head
    }
}

fn log_unrecognized_tlv_frame(frame: &[u8]) {
    if frame.len() < 3 {
        warn!(
            target: "ble_gui::poll",
            "TLV 收到无法识别的短帧 len={} hex={}",
            frame.len(),
            hex_preview(frame, 48),
        );
        return;
    }
    let detail = if frame[1] == 0x10 && frame.len() >= 4 {
        let start = u16::from_be_bytes([frame[2], frame[3]]);
        if start == REG_21000 {
            let func = if frame.len() >= 9 {
                Some(u16::from_be_bytes([frame[7], frame[8]]))
            } else {
                None
            };
            format!("FC10 start={start} func={func:?} byte_count={}", frame.get(6).copied().unwrap_or(0))
        } else {
            format!("FC10 start={start}")
        }
    } else {
        format!("func=0x{:02X}", frame[1])
    };
    warn!(
        target: "ble_gui::poll",
        "TLV 收到无法识别的 Modbus 帧 len={} {detail} hex={}",
        frame.len(),
        hex_preview(frame, 64),
    );
}
use super::protocol::ProtocolSession;

/// Modbus 请求串行锁（对齐 C# `_sendLock`）。
pub type ModbusGate = Arc<tokio::sync::Mutex<()>>;

/// Studio POST-KEX-PROBE：读 1～16，其中寄存器 3 bit3 表示是否支持 TLV。
const POST_KEX_PROBE_START: u16 = 1;
const POST_KEX_PROBE_COUNT: u16 = 16;

async fn read_post_kex_probe(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    slave_id: u8,
) -> io::Result<Vec<u16>> {
    modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, POST_KEX_PROBE_START, POST_KEX_PROBE_COUNT),
        slave_id,
        POST_KEX_PROBE_COUNT,
    )
    .await
}

fn tlv_mode_from_iot_status(status_word: u16) -> ModbusReadMode {
    if iot_status_supports_tlv(status_word) {
        debug!(target: "ble_gui::poll", "设备支持 Modbus TLV 读（寄存器 3 bit3=1）");
        ModbusReadMode::Tlv
    } else {
        debug!(target: "ble_gui::poll", "设备不支持 Modbus TLV，使用常规读");
        ModbusReadMode::Standard
    }
}

/// 无有效探测回复：按不支持 TLV 定论，避免轮询卡在 POST-KEX 重试。
fn settle_probe_as_standard(live: &SharedModbusLive, reason: &str) {
    warn!(
        target: "ble_gui::poll",
        "探测 Modbus TLV 能力失败（寄存器 1～16 / 3）: {reason}；无有效回复，按不支持 TLV 处理，改用常规读",
    );
    let mut inner = live.lock().expect("modbus live lock");
    inner.read_mode = ModbusReadMode::Standard;
    inner.capabilities_probed = true;
}

/// 连接后读寄存器 1～16（仅一次），用寄存器 3 bit3 确定常规读或 TLV 批量读。
/// 超时或回复无效则视为不支持 TLV，继续后续常规轮询。
pub async fn probe_modbus_capabilities(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
) -> bool {
    {
        let inner = live.lock().expect("modbus live lock");
        if inner.capabilities_probed {
            return true;
        }
    }

    let slave_id = live.lock().expect("modbus live lock").slave_id;

    let regs = match read_post_kex_probe(protocol, write_tx, slave_id).await {
        Ok(regs) => regs,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            debug!(
                target: "ble_gui::poll",
                "POST-KEX 探测首包异常（{err}），立即重读 1～16",
            );
            match read_post_kex_probe(protocol, write_tx, slave_id).await {
                Ok(regs) => regs,
                Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
                Err(err) => {
                    settle_probe_as_standard(live, &err.to_string());
                    return true;
                }
            }
        }
    };

    apply_probe_regs(live, &regs);
    true
}

fn apply_probe_regs(live: &SharedModbusLive, regs: &[u16]) {
    let status_index = (REG_IOT_STATUS - POST_KEX_PROBE_START) as usize;
    let Some(&status_word) = regs.get(status_index) else {
        settle_probe_as_standard(
            live,
            &format!("回复过短 len={}，期望寄存器 3", regs.len()),
        );
        return;
    };

    let mut inner = live.lock().expect("modbus live lock");
    inner.read_mode = tlv_mode_from_iot_status(status_word);
    inner.capabilities_probed = true;
}

/// 连接会话内读一次机型 / SN / IoT 身份 / 服务器地址 / WiFi 密码。
/// 失败不阻塞主页功率数据，下一轮再试。
pub async fn read_device_info_once(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
) {
    {
        let inner = live.lock().expect("modbus live lock");
        if inner.device_info_loaded {
            return;
        }
    }

    let slave_id = live.lock().expect("modbus live lock").slave_id;
    info!(
        target: "ble_gui::poll",
        "读取设备信息 1100～1130 slave_id={slave_id}",
    );

    match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_DEVICE_INFO_START, REG_DEVICE_INFO_COUNT),
        slave_id,
        REG_DEVICE_INFO_COUNT,
    )
    .await
    {
        Ok(regs) => match parse_device_info(&regs) {
            Ok(mut info) => {
                info!(
                    target: "ble_gui::poll",
                    "1100～1130 原始 {} 个: {}",
                    regs.len(),
                    format_regs_hex(&regs),
                );
                let iot_regs = match modbus_read(
                    protocol,
                    write_tx,
                    build_read_holding(slave_id, REG_IOT_INFO_START, REG_IOT_IDENTITY_COUNT),
                    slave_id,
                    REG_IOT_IDENTITY_COUNT,
                )
                .await
                {
                    Ok(regs) => Some(regs),
                    Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return,
                    Err(err) => {
                        warn!(
                            target: "ble_gui::poll",
                            "读寄存器 11000～11033 失败: {err}，回退 11000～11015",
                        );
                        match modbus_read(
                            protocol,
                            write_tx,
                            build_read_holding(slave_id, REG_IOT_INFO_START, REG_IOT_INFO_COUNT),
                            slave_id,
                            REG_IOT_INFO_COUNT,
                        )
                        .await
                        {
                            Ok(regs) => Some(regs),
                            Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return,
                            Err(err) => {
                                warn!(target: "ble_gui::poll", "读寄存器 11000～11015 失败: {err}");
                                None
                            }
                        }
                    }
                };

                let identity = if let Some(iot_regs) = iot_regs.as_ref() {
                    info!(
                        target: "ble_gui::poll",
                        "11000 段原始 {} 个: {}",
                        iot_regs.len(),
                        format_regs_hex(iot_regs),
                    );
                    if let Some(ver) = parse_iot_software_ver(iot_regs) {
                        info!(
                            target: "ble_gui::poll",
                            "IOT software_ver(11014～11015)={ver} (u32 低字在前)",
                        );
                        info.merge_iot_version(ver);
                    }
                    if info.device_type.is_empty() {
                        info.device_type = parse_iot_type(iot_regs);
                    }
                    parse_iot_identity(iot_regs)
                } else {
                    parse_iot_identity(&[])
                };

                match modbus_read(
                    protocol,
                    write_tx,
                    build_read_holding(slave_id, REG_WHOLE_DEVICE_START, REG_WHOLE_DEVICE_COUNT),
                    slave_id,
                    REG_WHOLE_DEVICE_COUNT,
                )
                .await
                {
                    Ok(whole) => {
                        let (ty, sn) = parse_whole_device(&whole);
                        if !ty.is_empty() {
                            info.device_type = ty;
                        }
                        if !sn.is_empty() {
                            info.sn = sn;
                        }
                    }
                    Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return,
                    Err(err) => {
                        warn!(target: "ble_gui::poll", "读寄存器 110～119 失败: {err}");
                    }
                }

                let cloud_url = match modbus_read(
                    protocol,
                    write_tx,
                    build_read_holding(slave_id, IOT_CLOUD_DNS_REGISTER, IOT_CLOUD_DNS_REGISTER_COUNT),
                    slave_id,
                    IOT_CLOUD_DNS_REGISTER_COUNT,
                )
                .await
                {
                    Ok(regs) => parse_ascii_regs(&regs),
                    Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return,
                    Err(err) => {
                        warn!(target: "ble_gui::poll", "读服务器地址 12067 失败: {err}");
                        String::new()
                    }
                };

                let wifi_password = match modbus_read(
                    protocol,
                    write_tx,
                    build_read_holding(slave_id, REG_WIFI_STA_PASSWORD, REG_WIFI_STA_PASSWORD_COUNT),
                    slave_id,
                    REG_WIFI_STA_PASSWORD_COUNT,
                )
                .await
                {
                    Ok(regs) => parse_ascii_regs(&regs),
                    Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return,
                    Err(err) => {
                        warn!(target: "ble_gui::poll", "读 WiFi 密码 12018 失败: {err}");
                        String::new()
                    }
                };

                let summary = info.summary_text();
                info!(
                    target: "ble_gui::poll",
                    "设备信息 type={} sn={} iot_sn={} safe={} cloud={} versions={summary}",
                    info.device_type,
                    if info.sn.is_empty() { "—" } else { &info.sn },
                    if identity.iot_sn.is_empty() { "—" } else { &identity.iot_sn },
                    if identity.safe_code.is_empty() { "—" } else { &identity.safe_code },
                    if cloud_url.is_empty() { "—" } else { &cloud_url },
                );
                if let Ok(mut inner) = live.lock() {
                    inner.device_software = info
                        .software
                        .iter()
                        .map(|s| (s.type_code, s.version))
                        .collect();
                    inner.iot_software_version = info.iot_version();
                    inner.device_type = info.device_type;
                    inner.device_sn = info.sn;
                    inner.device_versions_text = summary;
                    inner.iot_type = identity.iot_type;
                    inner.iot_sn = identity.iot_sn;
                    inner.safe_code = identity.safe_code;
                    inner.wifi_mac = identity.wifi_mac;
                    inner.ble_mac = identity.ble_mac;
                    inner.cloud_url = cloud_url;
                    inner.wifi_password = wifi_password;
                    inner.identity_loaded = true;
                    inner.device_info_loaded = true;
                }
            }
            Err(err) => {
                warn!(target: "ble_gui::poll", "解析 1100 段失败: {err}");
            }
        },
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => {}
        Err(err) => {
            warn!(target: "ble_gui::poll", "读寄存器 1100～1130 失败: {err}");
        }
    }
}

/// TLV 组合读：一次写 21000，经 FF03 收齐多包后解析。
pub(crate) async fn modbus_tlv_read(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    tl_items: &[TlReadSpec],
) -> io::Result<Vec<super::modbus::TlvReadResult>> {
    if tl_items.is_empty() {
        return Ok(Vec::new());
    }

    let request = super::modbus::build_tlv_read_request(tl_items);
    debug!(
        target: "ble_gui::poll",
        "TLV 读请求 {} 项 plain={} hex={}",
        tl_items.len(),
        request.len(),
        hex_preview(&request, 80),
    );
    for (idx, item) in tl_items.iter().enumerate() {
        debug!(
            target: "ble_gui::poll",
            "  TL[{idx}] slave={} reg={} bytes={}",
            item.slave_addr,
            item.reg_addr,
            item.byte_len,
        );
    }

    let ready = protocol.lock().expect("protocol lock").modbus_ready();
    if !ready {
        return Err(io::Error::new(
            io::ErrorKind::NotConnected,
            "Modbus 链路未就绪",
        ));
    }

    let air = {
        let mut session = protocol.lock().expect("protocol lock");
        session.clear_modbus_responses();
        session.wrap_modbus_request(&request)?
    };

    write_tx
        .send(air)
        .map_err(|_| io::Error::new(io::ErrorKind::BrokenPipe, "写通道已关闭"))?;

    let started = tokio::time::Instant::now();
    let total_deadline = started + Duration::from_millis(MODBUS_TLV_TOTAL_TIMEOUT_MS);
    let mut idle_deadline = started + Duration::from_millis(MODBUS_TLV_IDLE_TIMEOUT_MS);
    let mut collector = TlvPacketCollector::default();
    let mut got_ack = false;
    let mut rx_frames = 0u32;

    loop {
        tokio::time::sleep(Duration::from_millis(25)).await;
        let mut progressed = false;
        loop {
            let resp = protocol
                .lock()
                .expect("protocol lock")
                .pop_modbus_response();
            let Some(resp) = resp else {
                break;
            };
            rx_frames += 1;
            if is_fc10_write_ack(&resp) {
                got_ack = true;
                progressed = true;
                debug!(
                    target: "ble_gui::poll",
                    "TLV 收到 FC10 写应答 hex={}",
                    hex_preview(&resp, 16),
                );
                continue;
            }
            if let Some(packet) = parse_tlv_response_packet(&resp) {
                debug!(
                    target: "ble_gui::poll",
                    "TLV 数据包 {}/{} payload={}B frame={}B hex={}",
                    packet.curr_index,
                    packet.total_index,
                    packet.payload.len(),
                    resp.len(),
                    hex_preview(&resp, 64),
                );
                collector.insert(packet)?;
                progressed = true;
                continue;
            }
            log_unrecognized_tlv_frame(&resp);
        }
        if progressed {
            idle_deadline =
                tokio::time::Instant::now() + Duration::from_millis(MODBUS_TLV_IDLE_TIMEOUT_MS);
        }
        if collector.is_complete() {
            let assembled = collector.assembled();
            let units = parse_tlv_read_units(&assembled);
            debug!(
                target: "ble_gui::poll",
                "TLV 收包完成 ack={got_ack} frames={rx_frames} assembled={}B units={} [{}] hex={}",
                assembled.len(),
                units.len(),
                describe_tlv_units(&units),
                hex_preview(&assembled, 96),
            );
            if units.is_empty() && !assembled.is_empty() {
                warn!(
                    target: "ble_gui::poll",
                    "TLV assembled 有 {}B 数据但未解析出有效单元，请检查 TL 格式",
                    assembled.len(),
                );
            }
            return Ok(units);
        }
        let now = tokio::time::Instant::now();
        if now >= idle_deadline || now >= total_deadline {
            let (pending, pending_hex) = protocol
                .lock()
                .expect("protocol lock")
                .rx_pending_debug();
            let reason = if now >= total_deadline {
                "总时长"
            } else {
                "包间空闲"
            };
            warn!(
                target: "ble_gui::poll",
                "TLV 响应超时 ({reason} idle={}ms total={}ms elapsed={}ms) ack={got_ack} frames={rx_frames} packets={}/{} assembled={}B rx_pending={}B [{}]",
                MODBUS_TLV_IDLE_TIMEOUT_MS,
                MODBUS_TLV_TOTAL_TIMEOUT_MS,
                started.elapsed().as_millis(),
                collector.received_count(),
                collector.expected_total().unwrap_or(0),
                collector.assembled().len(),
                pending,
                pending_hex,
            );
            return Err(io::Error::new(
                io::ErrorKind::TimedOut,
                "TLV 响应超时",
            ));
        }
    }
}

/// 执行一次仪表板轮询（SOC@102、功率@140～147、开关 2011/2012）。
pub async fn poll_dashboard(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
    gate: &ModbusGate,
) -> bool {
    let _guard = gate.lock().await;
    let (slave_id, use_tlv) = {
        let inner = live.lock().expect("modbus live lock");
        (inner.slave_id, inner.read_mode == ModbusReadMode::Tlv)
    };

    if use_tlv {
        return poll_dashboard_tlv(protocol, write_tx, live, slave_id).await;
    }

    info!(
        target: "ble_gui::poll",
        "主页轮询 slave_id={slave_id} 寄存器 {REG_SOC}、{REG_POWER_START}～{}、2011～2012",
        REG_POWER_START + REG_POWER_COUNT - 1,
    );

    let soc_regs = match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_SOC, REG_SOC_COUNT),
        slave_id,
        REG_SOC_COUNT,
    )
    .await
    {
        Ok(r) => r,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            warn!(target: "ble_gui::poll", "读寄存器 {REG_SOC}(SOC) 失败: {err}");
            return false;
        }
    };
    let power_regs = match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_POWER_START, REG_POWER_COUNT),
        slave_id,
        REG_POWER_COUNT,
    )
    .await
    {
        Ok(r) => r,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            warn!(
                target: "ble_gui::poll",
                "读寄存器 {REG_POWER_START}～{} 失败: {err}",
                REG_POWER_START + REG_POWER_COUNT - 1,
            );
            return false;
        }
    };

    let soc = *soc_regs.first().unwrap_or(&0);
    if soc == 0 && power_regs.iter().all(|v| *v == 0) {
        warn!(
            target: "ble_gui::poll",
            "主页功率段全 0，请检查从机地址 slave_id={slave_id}"
        );
    }

    let mut dashboard = match parse_dashboard_soc_and_power(soc, &power_regs) {
        Some(d) => d,
        None => {
            warn!(target: "ble_gui::poll", "解析主页 SOC/功率数据失败");
            return false;
        }
    };

    match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_AC_OUTPUT, 2),
        slave_id,
        2,
    )
    .await
    {
        Ok(states) => {
            let ac_on = states.first().is_some_and(|v| *v != 0);
            let dc_on = states.get(1).is_some_and(|v| *v != 0);
            merge_control_states(&mut dashboard, ac_on, dc_on);
        }
        Err(err) => {
            warn!(target: "ble_gui::poll", "读寄存器 2011～2012 失败: {err}");
        }
    }

    poll_dashboard_link(
        protocol,
        write_tx,
        live,
        slave_id,
        false,
        None,
    )
    .await;

    {
        let mut inner = live.lock().expect("modbus live lock");
        inner.dashboard = dashboard.clone();
        inner.modbus_online = true;
    }
    info!(
        target: "ble_gui::poll",
        "主页轮询结果 soc={}% ac_out={}W dc_out={}W ac_on={} dc_on={}",
        dashboard.soc,
        dashboard.ac_output_w,
        dashboard.dc_output_w,
        dashboard.ac_output_on,
        dashboard.dc_output_on,
    );
    true
}

async fn poll_dashboard_tlv(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
    slave_id: u8,
) -> bool {
    debug!(
        target: "ble_gui::poll",
        "主页 TLV 轮询 slave_id={slave_id} SOC@{REG_SOC} 功率@{REG_POWER_START}×{REG_POWER_COUNT} 开关@2011",
    );

    let tl_items = [
        TlReadSpec::from_register(slave_id, REG_SOC, REG_SOC_COUNT),
        TlReadSpec::from_register(slave_id, REG_POWER_START, REG_POWER_COUNT),
        TlReadSpec::from_register(slave_id, REG_AC_OUTPUT, 2),
        TlReadSpec::from_register(slave_id, REG_LINK_STATUS, REG_LINK_STATUS_BLOCK_COUNT),
        TlReadSpec::from_register(slave_id, REG_WIFI_SSID_NOW, REG_WIFI_SSID_NOW_COUNT),
    ];

    let results = match modbus_tlv_read(protocol, write_tx, &tl_items).await {
        Ok(r) => r,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            warn!(target: "ble_gui::poll", "主页 TLV 读失败: {err}");
            return false;
        }
    };

    let soc = match tlv_register_values(&results, slave_id, REG_SOC) {
        Ok(regs) => *regs.first().unwrap_or(&0),
        Err(err) => {
            warn!(
                target: "ble_gui::poll",
                "TLV 缺少 SOC({REG_SOC}): {err}；已收到 [{}]",
                describe_tlv_units(&results),
            );
            return false;
        }
    };
    let power = match tlv_register_values(&results, slave_id, REG_POWER_START) {
        Ok(regs) => regs,
        Err(err) => {
            warn!(
                target: "ble_gui::poll",
                "TLV 缺少功率段({REG_POWER_START}): {err}；已收到 [{}]",
                describe_tlv_units(&results),
            );
            return false;
        }
    };

    let mut dashboard = match parse_dashboard_soc_and_power(soc, &power) {
        Some(d) => d,
        None => {
            warn!(
                target: "ble_gui::poll",
                "TLV 解析主页功率失败（需要 {REG_POWER_COUNT} 寄存器，实得 {}）；已收到 [{}]",
                power.len(),
                describe_tlv_units(&results),
            );
            return false;
        }
    };

    if let Ok(states) = tlv_register_values(&results, slave_id, REG_AC_OUTPUT) {
        let ac_on = states.first().is_some_and(|v| *v != 0);
        let dc_on = states.get(1).is_some_and(|v| *v != 0);
        merge_control_states(&mut dashboard, ac_on, dc_on);
    }

    poll_dashboard_link(
        protocol,
        write_tx,
        live,
        slave_id,
        true,
        Some(&results),
    )
    .await;

    {
        let mut inner = live.lock().expect("modbus live lock");
        inner.dashboard = dashboard.clone();
        inner.modbus_online = true;
    }
    debug!(
        target: "ble_gui::poll",
        "主页 TLV 轮询结果 soc={}% ac_out={}W dc_out={}W ac_on={} dc_on={}",
        dashboard.soc,
        dashboard.ac_output_w,
        dashboard.dc_output_w,
        dashboard.ac_output_on,
        dashboard.dc_output_on,
    );
    true
}

fn apply_dashboard_link(live: &SharedModbusLive, link_regs: &[u16], ssid_regs: &[u16]) {
    let (wifi_sta, mqtt_ok) = parse_link_status_regs(link_regs);
    let sta_ip = if link_regs.len() >= 4 {
        parse_sta_ipv4_regs(&link_regs[2..4])
    } else {
        String::new()
    };
    let sta_rssi = link_regs
        .get((REG_STA_RSSI - REG_LINK_STATUS) as usize)
        .copied()
        .map(parse_sta_rssi)
        .unwrap_or(0);
    let ssid_now = parse_ascii_regs(ssid_regs);
    if let Ok(mut inner) = live.lock() {
        inner.wifi_sta = wifi_sta;
        inner.mqtt_ok = mqtt_ok;
        inner.sta_ip = sta_ip;
        inner.sta_rssi = sta_rssi;
        inner.ssid_now = ssid_now;
        inner.link_status_valid = true;
    }
}

/// 主页状态：链路 / STA IP / RSSI / 当前 SSID。失败不影响功率卡片。
async fn poll_dashboard_link(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
    slave_id: u8,
    use_tlv: bool,
    tlv_results: Option<&[super::modbus::TlvReadResult]>,
) {
    if use_tlv {
        let Some(results) = tlv_results else {
            return;
        };
        let link_regs = tlv_register_values(results, slave_id, REG_LINK_STATUS).ok();
        let ssid_regs = tlv_register_values(results, slave_id, REG_WIFI_SSID_NOW).ok();
        match (link_regs, ssid_regs) {
            (Some(link), Some(ssid)) => apply_dashboard_link(live, &link, &ssid),
            (Some(link), None) => apply_dashboard_link(live, &link, &[]),
            _ => {}
        }
        return;
    }

    let link_regs = match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_LINK_STATUS, REG_LINK_STATUS_BLOCK_COUNT),
        slave_id,
        REG_LINK_STATUS_BLOCK_COUNT,
    )
    .await
    {
        Ok(r) => r,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return,
        Err(err) => {
            warn!(target: "ble_gui::poll", "读链路状态 11018～11026 失败: {err}");
            return;
        }
    };
    let ssid_regs = match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_WIFI_SSID_NOW, REG_WIFI_SSID_NOW_COUNT),
        slave_id,
        REG_WIFI_SSID_NOW_COUNT,
    )
    .await
    {
        Ok(r) => r,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return,
        Err(err) => {
            warn!(target: "ble_gui::poll", "读当前 SSID 11108 失败: {err}");
            Vec::new()
        }
    };
    apply_dashboard_link(live, &link_regs, &ssid_regs);
}

/// 外部设备页：写 21000=1，等待设备上报组网配件列表。
pub async fn poll_external_devices(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
    gate: &ModbusGate,
) -> bool {
    let _guard = gate.lock().await;
    let request = build_write_single(0, REG_21000, 1);
    match modbus_transaction(protocol, write_tx, request).await {
        Ok(_) => info!(target: "ble_gui::poll", "已请求 21000 组网列表"),
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            warn!(target: "ble_gui::poll", "请求 21000 组网列表失败: {err}");
            return false;
        }
    }

    let deadline = tokio::time::Instant::now() + Duration::from_millis(1000);
    loop {
        tokio::time::sleep(Duration::from_millis(25)).await;
        loop {
            let more = protocol
                .lock()
                .expect("protocol lock")
                .pop_modbus_response()
                .is_some();
            if !more {
                break;
            }
        }
        if tokio::time::Instant::now() >= deadline {
            break;
        }
    }

    live.lock()
        .map(|inner| inner.sub_devices_valid)
        .unwrap_or(false)
}

/// 写保持寄存器：单字用 FC06，多字用 FC10；`bit` 为 Some 时先读后改写该位；`field` 为 Some 时先读后改多位域。
pub async fn write_holding_registers(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    gate: &ModbusGate,
    slave_id: u8,
    address: u16,
    values: &[u16],
    bit: Option<u8>,
    field: Option<crate::services::ble::modbus::RegisterFieldPatch>,
) -> Result<(), String> {
    if values.is_empty() && bit.is_none() && field.is_none() {
        return Err("写入值为空".into());
    }
    let _guard = gate.lock().await;

    let to_write = if let Some(field) = field {
        let current = modbus_read(
            protocol,
            write_tx,
            build_read_holding(slave_id, address, 1),
            slave_id,
            1,
        )
        .await
        .map_err(|e| e.to_string())?;
        let mut word = current.first().copied().unwrap_or(0);
        let field_mask = ((1u16 << field.width) - 1) << field.start_bit;
        let val = values
            .first()
            .copied()
            .unwrap_or(field.value)
            & ((1u16 << field.width) - 1);
        word = (word & !field_mask) | (val << field.start_bit);
        vec![word]
    } else if let Some(bit_idx) = bit {
        let current = modbus_read(
            protocol,
            write_tx,
            build_read_holding(slave_id, address, 1),
            slave_id,
            1,
        )
        .await
        .map_err(|e| e.to_string())?;
        let mut word = current.first().copied().unwrap_or(0);
        let on = values.first().copied().unwrap_or(0) != 0;
        if on {
            word |= 1u16 << bit_idx;
        } else {
            word &= !(1u16 << bit_idx);
        }
        vec![word]
    } else {
        values.to_vec()
    };

    let request = if to_write.len() == 1 {
        build_write_single(slave_id, address, to_write[0])
    } else {
        build_write_multiple(slave_id, address, &to_write)
    };

    let response = modbus_transaction(protocol, write_tx, request)
        .await
        .map_err(|e| e.to_string())?;

    let ok = if to_write.len() == 1 {
        response.len() >= 8 && response[1] == 0x06
    } else {
        is_fc10_write_ack(&response)
    };
    if !ok {
        return Err(format!("写寄存器 {address} 响应异常"));
    }
    Ok(())
}

/// 写单个控制寄存器（2011/2012），成功后回读确认。
pub async fn write_control_register(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
    gate: &ModbusGate,
    address: u16,
    value: u16,
) -> Result<(), String> {
    let _guard = gate.lock().await;
    let slave_id = live.lock().expect("modbus live lock").slave_id;
    let request = build_write_single(slave_id, address, value);
    let response = modbus_transaction(protocol, write_tx, request)
        .await
        .map_err(|e| e.to_string())?;
    if response.len() < 8 || response[1] != 0x06 {
        return Err(format!("写寄存器 {address} 响应异常"));
    }

    tokio::time::sleep(Duration::from_millis(500)).await;

    match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_AC_OUTPUT, 2),
        slave_id,
        2,
    )
    .await
    {
        Ok(states) => {
            let ac_on = states.first().is_some_and(|v| *v != 0);
            let dc_on = states.get(1).is_some_and(|v| *v != 0);
            if let Ok(mut inner) = live.lock() {
                inner.dashboard.ac_output_on = ac_on;
                inner.dashboard.dc_output_on = dc_on;
                inner.dashboard.data_valid = true;
            }
            let expected = value != 0;
            let actual = if address == REG_AC_OUTPUT { ac_on } else { dc_on };
            if actual != expected {
                return Err(format!("写寄存器 {address} 后回读不一致"));
            }
        }
        Err(err) => {
            warn!(target: "ble_gui::poll", "写后回读 2011～2012 失败: {err}");
            if let Ok(mut inner) = live.lock() {
                if address == REG_AC_OUTPUT {
                    inner.dashboard.ac_output_on = value != 0;
                } else {
                    inner.dashboard.dc_output_on = value != 0;
                }
            }
        }
    }

    Ok(())
}

pub(crate) async fn modbus_read(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    request: Vec<u8>,
    slave_id: u8,
    count: u16,
) -> io::Result<Vec<u16>> {
    let response = modbus_transaction(protocol, write_tx, request).await?;
    parse_read_holding(&response, slave_id, count)
}

async fn modbus_transaction(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    request: Vec<u8>,
) -> io::Result<Vec<u8>> {
    let ready = {
        let session = protocol.lock().expect("protocol lock");
        session.modbus_ready()
    };
    if !ready {
        return Err(io::Error::new(
            io::ErrorKind::NotConnected,
            "Modbus 链路未就绪",
        ));
    }

    let air = {
        let mut session = protocol.lock().expect("protocol lock");
        session.clear_modbus_responses();
        session.wrap_modbus_request(&request)?
    };

    write_tx
        .send(air)
        .map_err(|_| io::Error::new(io::ErrorKind::BrokenPipe, "写通道已关闭"))?;

    let deadline = tokio::time::Instant::now() + Duration::from_millis(MODBUS_TIMEOUT_MS);
    loop {
        tokio::time::sleep(Duration::from_millis(25)).await;
        if let Some(resp) = protocol
            .lock()
            .expect("protocol lock")
            .pop_modbus_response()
        {
            return Ok(resp);
        }
        if tokio::time::Instant::now() >= deadline {
            warn!(
                target: "ble_gui::poll",
                "Modbus 响应超时 ({}ms)",
                MODBUS_TIMEOUT_MS,
            );
            return Err(io::Error::new(
                io::ErrorKind::TimedOut,
                "Modbus 响应超时",
            ));
        }
    }
}

/// 连接建立后初始化 live 状态。
pub fn init_live_on_connect(live: &SharedModbusLive) {
    let mut inner = live.lock().expect("modbus live lock");
    inner.clear_session();
    inner.slave_id = DEFAULT_SLAVE_ID;
}

pub fn clear_live_on_disconnect(live: &SharedModbusLive) {
    let mut inner = live.lock().expect("modbus live lock");
    inner.clear_session();
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::services::modbus::ModbusLive;

    #[test]
    fn timeout_settles_probe_as_standard_not_retry_forever() {
        let live: SharedModbusLive = Arc::new(Mutex::new(ModbusLive::default()));
        settle_probe_as_standard(&live, "Modbus 响应超时");
        let inner = live.lock().expect("modbus live lock");
        assert!(inner.capabilities_probed);
        assert_eq!(inner.read_mode, ModbusReadMode::Standard);
    }

    #[test]
    fn short_reply_settles_probe_as_standard() {
        let live: SharedModbusLive = Arc::new(Mutex::new(ModbusLive::default()));
        apply_probe_regs(&live, &[0, 0]);
        let inner = live.lock().expect("modbus live lock");
        assert!(inner.capabilities_probed);
        assert_eq!(inner.read_mode, ModbusReadMode::Standard);
    }

    #[test]
    fn iot_status_bit3_selects_tlv_or_standard() {
        assert_eq!(tlv_mode_from_iot_status(0), ModbusReadMode::Standard);
        assert_eq!(tlv_mode_from_iot_status(0b0111), ModbusReadMode::Standard);
        assert_eq!(tlv_mode_from_iot_status(1 << 3), ModbusReadMode::Tlv);
        assert_eq!(tlv_mode_from_iot_status(0xFFFF), ModbusReadMode::Tlv);

        let live: SharedModbusLive = Arc::new(Mutex::new(ModbusLive::default()));
        let mut regs = vec![0u16; 16];
        regs[(REG_IOT_STATUS - POST_KEX_PROBE_START) as usize] = 1 << 3;
        apply_probe_regs(&live, &regs);
        let inner = live.lock().expect("modbus live lock");
        assert!(inner.capabilities_probed);
        assert_eq!(inner.read_mode, ModbusReadMode::Tlv);
    }
}
