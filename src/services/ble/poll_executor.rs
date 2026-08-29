//! 按前台策略分发 Modbus 轮询。

use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

use log::{debug, info, warn};

use crate::services::modbus::{QueryItemPollResult, SharedModbusLive, SharedQueryPollLive};

use super::modbus::{build_read_holding, format_query_value};
use super::poll::{modbus_read, poll_dashboard, ModbusGate};
use super::poll_policy::{describe_poll_foreground, effective_foreground, ensure_dashboard_poll_if_idle, PollForeground, SharedPollPolicy};
use super::protocol::ProtocolSession;

/// 将单项轮询结果合并进共享快照；若该项相对上次有变化则返回 true。
fn publish_query_item(
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    tab_index: usize,
    result: &QueryItemPollResult,
) {
    let Ok(mut live) = query_live.lock() else {
        warn!(target: "ble_gui::query_ui", "query_live 写入失败: lock poisoned");
        return;
    };

    let changed = if live.tab_index != tab_index {
        live.tab_index = tab_index;
        live.items.clear();
        live.items.push(result.clone());
        true
    } else if let Some(existing) = live
        .items
        .iter_mut()
        .find(|i| i.item_index == result.item_index)
    {
        if existing == result {
            false
        } else {
            *existing = result.clone();
            true
        }
    } else {
        live.items.push(result.clone());
        true
    };

    if changed {
        debug!(
            target: "ble_gui::query_ui",
            "query_live 单项更新 标签={tab_index} [#{}] result={} status={}",
            result.item_index,
            result.result,
            result.status,
        );
        query_generation.fetch_add(1, Ordering::Release);
    }
}

/// 根据当前前台策略执行一轮轮询；`None` 时不发任何 Modbus 请求。
pub async fn poll_foreground(
    policy: &SharedPollPolicy,
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    modbus_live: &SharedModbusLive,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    gate: &ModbusGate,
) -> bool {
    if write_tx.is_closed() {
        return false;
    }
    ensure_dashboard_poll_if_idle(policy);
    let foreground = effective_foreground(policy);
    match &foreground {
        PollForeground::None => false,
        PollForeground::Dashboard => poll_dashboard(protocol, write_tx, modbus_live, gate).await,
        PollForeground::ModbusQuery {
            tab_index,
            slave_id,
            items,
        } => {
            info!(
                target: "ble_gui::poll",
                "开始 {}",
                describe_poll_foreground(&foreground),
            );
            poll_modbus_query(
                protocol,
                write_tx,
                query_live,
                query_generation,
                gate,
                *tab_index,
                *slave_id,
                items,
            )
            .await
        }
    }
}

/// 读当前标签页内每个查询项对应的一个保持寄存器。
pub async fn poll_modbus_query(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    gate: &ModbusGate,
    tab_index: usize,
    slave_id: u8,
    items: &[super::poll_policy::QueryPollItemSpec],
) -> bool {
    let _guard = gate.lock().await;
    let mut any_ok = false;

    if items.is_empty() {
        info!(
            target: "ble_gui::poll",
            "Modbus 查询轮询完成 标签={tab_index} 从站={slave_id}（无查询项）",
        );
        return false;
    }

    for item in items {
        let poll_result = match item.protocol_address {
            None => {
                info!(
                    target: "ble_gui::poll",
                    "查询 标签={tab_index} [#{}] 寄存器 {} → 地址无效",
                    item.item_index,
                    item.register_text,
                );
                QueryItemPollResult {
                    item_index: item.item_index,
                    status: "地址无效".into(),
                    result: item.register_text.clone(),
                    ok: false,
                }
            }
            Some(address) => {
                let count = item.register_count.max(1);
                match modbus_read(
                    protocol,
                    write_tx,
                    build_read_holding(slave_id, address, count),
                    slave_id,
                    count,
                )
                .await
                {
                    Ok(values) => {
                        match format_query_value(&values, item.value_type, item.scale) {
                            Ok(result) => {
                                any_ok = true;
                                info!(
                                    target: "ble_gui::poll",
                                    "查询 标签={tab_index} 从站={slave_id} [#{}] 寄存器 {} (协议 {address}×{count}) → {result}",
                                    item.item_index,
                                    item.register_text,
                                );
                                QueryItemPollResult {
                                    item_index: item.item_index,
                                    status: "正常".into(),
                                    result,
                                    ok: true,
                                }
                            }
                            Err(err) => {
                                warn!(
                                    target: "ble_gui::poll",
                                    "查询 标签={tab_index} [#{}] 寄存器 {} 解析失败: {err}",
                                    item.item_index,
                                    item.register_text,
                                );
                                QueryItemPollResult {
                                    item_index: item.item_index,
                                    status: "解析失败".into(),
                                    result: err,
                                    ok: false,
                                }
                            }
                        }
                    }
                    Err(err) if err.kind() == std::io::ErrorKind::BrokenPipe => {
                        return false;
                    }
                    Err(err) => {
                        warn!(
                            target: "ble_gui::poll",
                            "查询 标签={tab_index} [#{}] 寄存器 {} (协议 {address}×{count}) 失败: {err}",
                            item.item_index,
                            item.register_text,
                        );
                        QueryItemPollResult {
                            item_index: item.item_index,
                            status: "失败".into(),
                            result: err.to_string(),
                            ok: false,
                        }
                    }
                }
            }
        };
        if poll_result.ok {
            any_ok = true;
        }
        publish_query_item(query_live, query_generation, tab_index, &poll_result);
    }

    info!(
        target: "ble_gui::poll",
        "Modbus 查询轮询完成 标签={tab_index} 从站={slave_id} 共 {} 项",
        items.len(),
    );

    any_ok || !items.is_empty()
}

/// Modbus 链路就绪后按策略立即拉一次（替代固定 dashboard 首次轮询）。
pub async fn poll_foreground_once(
    policy: &SharedPollPolicy,
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    modbus_live: &SharedModbusLive,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    gate: &ModbusGate,
) {
    let ready = protocol.lock().expect("protocol lock").modbus_ready();
    if !ready {
        return;
    }
    ensure_dashboard_poll_if_idle(policy);
    poll_foreground(
        policy,
        protocol,
        write_tx,
        modbus_live,
        query_live,
        query_generation,
        gate,
    )
    .await;
}
