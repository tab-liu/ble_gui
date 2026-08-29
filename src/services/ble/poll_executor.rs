//! 按前台策略分发 Modbus 轮询。

use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

use log::{debug, info, warn};

use crate::services::modbus::{ModbusReadMode, QueryItemPollResult, SharedModbusLive, SharedQueryPollLive};

use super::modbus::{
    build_read_holding, chunk_tl_batches, format_query_value, tlv_batch_start_index,
    tlv_item_batch_index, tlv_register_values, TlReadSpec,
};
use super::poll::{modbus_read, modbus_tlv_read, poll_dashboard, probe_modbus_capabilities, ModbusGate};
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
    if !modbus_live
        .lock()
        .map(|l| l.capabilities_probed)
        .unwrap_or(false)
    {
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
                modbus_live,
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
    modbus_live: &SharedModbusLive,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    gate: &ModbusGate,
    tab_index: usize,
    slave_id: u8,
    items: &[super::poll_policy::QueryPollItemSpec],
) -> bool {
    let _guard = gate.lock().await;
    let use_tlv = modbus_live
        .lock()
        .map(|l| l.read_mode == ModbusReadMode::Tlv)
        .unwrap_or(false);

    if items.is_empty() {
        info!(
            target: "ble_gui::poll",
            "Modbus 查询轮询完成 标签={tab_index} 从站={slave_id}（无查询项）",
        );
        return false;
    }

    if use_tlv {
        return poll_modbus_query_tlv(
            protocol,
            write_tx,
            query_live,
            query_generation,
            tab_index,
            slave_id,
            items,
        )
        .await;
    }

    let mut any_ok = false;

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

async fn poll_modbus_query_tlv(
    protocol: &Arc<Mutex<ProtocolSession>>,
    write_tx: &tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    tab_index: usize,
    slave_id: u8,
    items: &[super::poll_policy::QueryPollItemSpec],
) -> bool {
    let mut tl_items = Vec::new();
    let mut valid_items = Vec::new();

    for item in items {
        match item.protocol_address {
            None => {
                let poll_result = QueryItemPollResult {
                    item_index: item.item_index,
                    status: "地址无效".into(),
                    result: item.register_text.clone(),
                    ok: false,
                };
                publish_query_item(query_live, query_generation, tab_index, &poll_result);
            }
            Some(address) => {
                let count = item.register_count.max(1);
                tl_items.push(TlReadSpec::from_register(slave_id, address, count));
                valid_items.push((item, address, count));
            }
        }
    }

    if tl_items.is_empty() {
        return false;
    }

    let batches = chunk_tl_batches(&tl_items);
    info!(
        target: "ble_gui::poll",
        "开始 Modbus TLV 查询 标签={tab_index} 从站={slave_id} {} 项分 {} 批",
        tl_items.len(),
        batches.len(),
    );

    let mut batch_failed = vec![false; batches.len()];

    let mut all_results = Vec::new();
    for (batch_idx, batch) in batches.iter().enumerate() {
        info!(
            target: "ble_gui::poll",
            "Modbus TLV 查询 标签={tab_index} 批次 {}/{} ({} 项)",
            batch_idx + 1,
            batches.len(),
            batch.len(),
        );
        match modbus_tlv_read(protocol, write_tx, batch).await {
            Ok(mut chunk) => all_results.append(&mut chunk),
            Err(err) if err.kind() == std::io::ErrorKind::BrokenPipe => return false,
            Err(err) => {
                batch_failed[batch_idx] = true;
                let err_text = err.to_string();
                warn!(
                    target: "ble_gui::poll",
                    "Modbus TLV 查询 标签={tab_index} 批次 {} 失败: {err_text}",
                    batch_idx + 1,
                );
                let start = tlv_batch_start_index(batch_idx);
                let end = start + batch.len();
                for (item, _, _) in &valid_items[start..end] {
                    let poll_result = QueryItemPollResult {
                        item_index: item.item_index,
                        status: "失败".into(),
                        result: err_text.clone(),
                        ok: false,
                    };
                    publish_query_item(query_live, query_generation, tab_index, &poll_result);
                }
            }
        }
    }

    let mut any_ok = false;
    for (idx, (item, address, count)) in valid_items.iter().enumerate() {
        let batch_idx = tlv_item_batch_index(idx);
        if batch_failed.get(batch_idx).copied().unwrap_or(false) {
            continue;
        }
        let poll_result = match tlv_register_values(&all_results, slave_id, *address) {
            Ok(values) => match format_query_value(&values, item.value_type, item.scale) {
                Ok(result) => {
                    any_ok = true;
                    info!(
                        target: "ble_gui::poll",
                        "TLV 查询 标签={tab_index} 从站={slave_id} [#{}] 寄存器 {} (协议 {address}×{count}) → {result}",
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
                Err(err) => QueryItemPollResult {
                    item_index: item.item_index,
                    status: "解析失败".into(),
                    result: err,
                    ok: false,
                },
            },
            Err(err) => QueryItemPollResult {
                item_index: item.item_index,
                status: "失败".into(),
                result: err.to_string(),
                ok: false,
            },
        };
        publish_query_item(query_live, query_generation, tab_index, &poll_result);
    }

    info!(
        target: "ble_gui::poll",
        "Modbus TLV 查询完成 标签={tab_index} 从站={slave_id} 共 {} 项 ({} 批)",
        items.len(),
        batches.len(),
    );

    any_ok || !items.is_empty()
}

/// Modbus 链路就绪后：探测 TLV 能力（仅一次）并立即拉一次数据。
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
    if !probe_modbus_capabilities(protocol, write_tx, modbus_live).await {
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
