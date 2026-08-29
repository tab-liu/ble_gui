//! Modbus 轮询：读 100～149 实时数据、2011～2012 输出状态。

use std::io;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use log::warn;

use crate::services::modbus::{DashboardData, SharedModbusLive};

use super::modbus::{
    build_read_holding, build_write_single, merge_control_states, parse_dashboard_registers,
    parse_read_holding, DEFAULT_SLAVE_ID, MODBUS_TIMEOUT_MS, REG_AC_OUTPUT,
    REG_DASHBOARD_COUNT, REG_DASHBOARD_START,
};
use super::protocol::ProtocolSession;

/// Modbus 请求串行锁（对齐 C# `_sendLock`）。
pub type ModbusGate = Arc<tokio::sync::Mutex<()>>;

/// 执行一次仪表板轮询（串行：先读 100～149，再读 2011～2012）。
pub async fn poll_dashboard(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
    gate: &ModbusGate,
) -> bool {
    let _guard = gate.lock().await;
    let slave_id = live.lock().expect("modbus live lock").slave_id;

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
        inner.dashboard = dashboard;
        inner.modbus_online = true;
    }
    true
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

async fn modbus_read(
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
}

pub fn clear_live_on_disconnect(live: &SharedModbusLive) {
    let mut inner = live.lock().expect("modbus live lock");
    inner.dashboard = DashboardData::default();
    inner.output_busy = false;
    inner.modbus_online = false;
}

/// 加密/明文 Modbus 就绪后立即拉一次数据。
pub async fn poll_dashboard_once(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    live: &SharedModbusLive,
    gate: &ModbusGate,
) {
    let ready = protocol.lock().expect("protocol lock").modbus_ready();
    if !ready {
        return;
    }
    poll_dashboard(protocol, write_tx, live, gate).await;
}
