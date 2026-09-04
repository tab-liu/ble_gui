//! Modbus 传输原语：仪表板轮询、能力探测、保持寄存器读写。
//!
//! - 主页：100～149 实时数据 + 2011/2012 输出状态（标准读或 TLV）
//! - 写：FC06/FC10；[`write_holding_registers`] 支持单 bit 或
//!   [`super::modbus::RegisterFieldPatch`] 多位域 RMW
//! - 前台「该轮询什么」由 [`super::poll_executor`] 按策略调用本模块

use std::io;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use log::{debug, info, warn};

use crate::services::modbus::{DashboardData, ModbusReadMode, SharedModbusLive};

use super::modbus::{
    build_read_holding, build_write_multiple, build_write_single, iot_status_supports_tlv,
    is_fc10_write_ack, merge_control_states, parse_dashboard_registers, parse_device_info,
    parse_iot_software_ver, parse_iot_type, parse_read_holding, parse_tlv_response_packet,
    parse_tlv_read_units, describe_tlv_units, format_regs_hex, tlv_register_values, TlReadSpec,
    TlvPacketCollector, DEFAULT_SLAVE_ID, MODBUS_TIMEOUT_MS, REG_21000, REG_AC_OUTPUT,
    REG_DASHBOARD_COUNT, REG_DASHBOARD_START, REG_DEVICE_INFO_COUNT, REG_DEVICE_INFO_START,
    REG_IOT_INFO_COUNT, REG_IOT_INFO_START, REG_IOT_STATUS,
};

const MODBUS_TLV_TIMEOUT_MS: u64 = 8000;

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

/// 连接后读寄存器 3 bit3（仅执行一次），确定常规读或 TLV 批量读。
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

    let mode = match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_IOT_STATUS, 1),
        slave_id,
        1,
    )
    .await
    {
        Ok(regs) => {
            if iot_status_supports_tlv(regs[0]) {
                debug!(target: "ble_gui::poll", "设备支持 Modbus TLV 读（寄存器 3 bit3=1）");
                ModbusReadMode::Tlv
            } else {
                debug!(target: "ble_gui::poll", "设备不支持 Modbus TLV，使用常规读");
                ModbusReadMode::Standard
            }
        }
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            warn!(
                target: "ble_gui::poll",
                "探测 Modbus TLV 能力失败（寄存器 3）: {err}，本轮不读后续寄存器，下一轮再试",
            );
            return false;
        }
    };

    {
        let mut inner = live.lock().expect("modbus live lock");
        inner.read_mode = mode;
        inner.capabilities_probed = true;
    }
    true
}

/// 连接会话内读一次 1100～1130。失败不阻塞主页数据，下一轮再试。
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
                match modbus_read(
                    protocol,
                    write_tx,
                    build_read_holding(slave_id, REG_IOT_INFO_START, REG_IOT_INFO_COUNT),
                    slave_id,
                    REG_IOT_INFO_COUNT,
                )
                .await
                {
                    Ok(iot_regs) => {
                        info!(
                            target: "ble_gui::poll",
                            "11000～11015 原始 {} 个: {}",
                            iot_regs.len(),
                            format_regs_hex(&iot_regs),
                        );
                        if let Some(ver) = parse_iot_software_ver(&iot_regs) {
                            info!(
                                target: "ble_gui::poll",
                                "IOT software_ver(11014～11015)={ver} (u32 低字在前)",
                            );
                            info.merge_iot_version(ver);
                        }
                        if info.device_type.is_empty() {
                            info.device_type = parse_iot_type(&iot_regs);
                        }
                    }
                    Err(err) if err.kind() == io::ErrorKind::BrokenPipe => {}
                    Err(err) => {
                        warn!(target: "ble_gui::poll", "读寄存器 11000～11015 失败: {err}");
                    }
                }
                let summary = info.summary_text();
                info!(
                    target: "ble_gui::poll",
                    "设备信息 type={} sn={} versions={summary}",
                    info.device_type,
                    if info.sn.is_empty() { "—" } else { &info.sn },
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

    let deadline =
        tokio::time::Instant::now() + Duration::from_millis(MODBUS_TLV_TIMEOUT_MS);
    let mut collector = TlvPacketCollector::default();
    let mut got_ack = false;
    let mut rx_frames = 0u32;

    loop {
        tokio::time::sleep(Duration::from_millis(25)).await;
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
                continue;
            }
            log_unrecognized_tlv_frame(&resp);
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
        if tokio::time::Instant::now() >= deadline {
            warn!(
                target: "ble_gui::poll",
                "TLV 响应超时 ({}ms) ack={got_ack} frames={rx_frames} packets={}/{} assembled={}B",
                MODBUS_TLV_TIMEOUT_MS,
                collector.received_count(),
                collector.expected_total().unwrap_or(0),
                collector.assembled().len(),
            );
            return Err(io::Error::new(
                io::ErrorKind::TimedOut,
                "TLV 响应超时",
            ));
        }
    }
}

/// 执行一次仪表板轮询（串行：先读 100～149，再读 2011～2012）。
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
        "主页轮询 slave_id={slave_id} 寄存器 100～149, 2011～2012",
    );

    let regs = match modbus_read(
        protocol,
        write_tx,
        build_read_holding(slave_id, REG_DASHBOARD_START, REG_DASHBOARD_COUNT),
        slave_id,
        REG_DASHBOARD_COUNT,
    )
    .await
    {
        Ok(r) => r,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            warn!(target: "ble_gui::poll", "读寄存器 100～149 失败: {err}");
            return false;
        }
    };

    if regs.iter().all(|v| *v == 0) {
        warn!(
            target: "ble_gui::poll",
            "100～149 段全部为 0，请检查从机地址 slave_id={slave_id}"
        );
    }

    let mut dashboard = match parse_dashboard_registers(&regs) {
        Some(d) => d,
        None => {
            warn!(target: "ble_gui::poll", "解析 100～149 数据失败");
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
        "主页 TLV 轮询 slave_id={slave_id} 寄存器 100～149, 2011～2012",
    );

    let tl_items = [
        TlReadSpec::from_register(slave_id, REG_DASHBOARD_START, REG_DASHBOARD_COUNT),
        TlReadSpec::from_register(slave_id, REG_AC_OUTPUT, 2),
    ];

    let results = match modbus_tlv_read(protocol, write_tx, &tl_items).await {
        Ok(r) => r,
        Err(err) if err.kind() == io::ErrorKind::BrokenPipe => return false,
        Err(err) => {
            warn!(target: "ble_gui::poll", "主页 TLV 读失败: {err}");
            return false;
        }
    };

    let dashboard = match tlv_register_values(&results, slave_id, REG_DASHBOARD_START) {
        Ok(regs) => {
            debug!(
                target: "ble_gui::poll",
                "TLV 100～149 原始 {} 个寄存器",
                regs.len(),
            );
            parse_dashboard_registers(&regs)
        }
        Err(err) => {
            warn!(
                target: "ble_gui::poll",
                "TLV 缺少 100～149 数据: {err}；已收到 [{}]",
                describe_tlv_units(&results),
            );
            None
        }
    };

    let mut dashboard = match dashboard {
        Some(d) => d,
        None => {
            warn!(
                target: "ble_gui::poll",
                "TLV 解析 100～149 数据失败（需要 ≥48 寄存器）；已收到 [{}]",
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
    inner.dashboard = DashboardData::default();
    inner.output_busy = false;
    inner.slave_id = DEFAULT_SLAVE_ID;
    inner.modbus_online = false;
    inner.read_mode = ModbusReadMode::Unknown;
    inner.capabilities_probed = false;
    inner.device_info_loaded = false;
    inner.device_type.clear();
    inner.device_sn.clear();
    inner.device_versions_text.clear();
    inner.iot_software_version = None;
    inner.device_software.clear();
}

pub fn clear_live_on_disconnect(live: &SharedModbusLive) {
    let mut inner = live.lock().expect("modbus live lock");
    inner.dashboard = DashboardData::default();
    inner.output_busy = false;
    inner.modbus_online = false;
    inner.read_mode = ModbusReadMode::Unknown;
    inner.capabilities_probed = false;
    inner.device_info_loaded = false;
    inner.device_type.clear();
    inner.device_sn.clear();
    inner.device_versions_text.clear();
    inner.iot_software_version = None;
    inner.device_software.clear();
}
