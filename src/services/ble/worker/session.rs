async fn connect_device(
    adapter: Adapter,
    peripheral: Peripheral,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    address_text: &str,
    modbus_live: &SharedModbusLive,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    poll_policy: &SharedPollPolicy,
    cancel_connect: &AtomicBool,
    ota_live: &SharedOtaLive,
    expect_drop: SharedExpectDrop,
) -> Result<ActiveSession, String> {
    if connect_cancelled(cancel_connect) {
        return Err("已取消连接".into());
    }
    set_phase(
        state,
        LinkPhase::Connecting,
        format!("正在连接 {address_text}……"),
    );
    notify_ui_force(ui_refresh, true);

    connect_gatt_with_retry(
        &peripheral,
        cancel_connect,
        state,
        ui_refresh,
        address_text,
    )
    .await?;

    // GATT 已连上后，后续任一步失败都必须主动 disconnect。
    // 否则链路占着：UI 像卡住、再连报 occupied，要等用户点取消才释放。
    let result = connect_after_gatt(
        adapter,
        peripheral.clone(),
        state,
        event_tx,
        ui_refresh,
        address_text,
        modbus_live,
        query_live,
        query_generation,
        poll_policy,
        cancel_connect,
        ota_live,
        expect_drop,
    )
    .await;
    if result.is_err() {
        disconnect_peripheral(&peripheral).await;
    }
    result
}

async fn connect_after_gatt(
    adapter: Adapter,
    peripheral: Peripheral,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    address_text: &str,
    modbus_live: &SharedModbusLive,
    query_live: &SharedQueryPollLive,
    query_generation: &Arc<AtomicU64>,
    poll_policy: &SharedPollPolicy,
    cancel_connect: &AtomicBool,
    ota_live: &SharedOtaLive,
    expect_drop: SharedExpectDrop,
) -> Result<ActiveSession, String> {
    if connect_cancelled(cancel_connect) {
        return Err("已取消连接".into());
    }

    set_phase(state, LinkPhase::Connecting, "正在发现 GATT 服务……");
    notify_ui_force(ui_refresh, true);

    discover_services_with_retry(&peripheral, 10, cancel_connect).await?;

    if connect_cancelled(cancel_connect) {
        return Err("已取消连接".into());
    }

    let characteristics = peripheral.characteristics();
    let write_id = write_uuid();
    let notify_id = notify_uuid();
    let notify_ff03 = notify_uuid_ff03();

    let write_char = characteristics
        .iter()
        .find(|c| c.uuid == write_id)
        .cloned()
        .ok_or_else(|| "未发现 FF02 写特征".to_string())?;

    let notify_char_ff01 = characteristics
        .iter()
        .find(|c| c.uuid == notify_id)
        .cloned();
    let notify_char_ff03 = characteristics
        .iter()
        .find(|c| c.uuid == notify_ff03)
        .cloned();

    let notify_char = notify_char_ff01
        .clone()
        .or_else(|| notify_char_ff03.clone())
        .ok_or_else(|| "未发现 FF01/FF03 通知特征".to_string())?;

    peripheral
        .subscribe(&notify_char)
        .await
        .map_err(|e| format!("订阅通知失败：{e}"))?;

    if let Some(ff03) = notify_char_ff03 {
        if notify_char_ff01.is_some() && ff03.uuid != notify_char.uuid {
            peripheral
                .subscribe(&ff03)
                .await
                .map_err(|e| format!("订阅 FF03 通知失败：{e}"))?;
            debug!(target: "ble_gui::worker", "已订阅 FF01 + FF03（TLV 异步数据走 FF03）");
        } else {
            debug!(target: "ble_gui::worker", "已订阅 FF03（无 FF01，TLV/通知均走 FF03）");
        }
    } else if notify_char_ff01.is_some() {
        info!(target: "ble_gui::worker", "已订阅 FF01（设备无 FF03 特征）");
    }

    let props = peripheral.properties().await.ok().flatten();
    let device_name = props
        .as_ref()
        .and_then(|p| p.local_name.clone())
        .filter(|n| !n.trim().is_empty())
        .unwrap_or_else(|| address_text.to_string());
    let device_address = props
        .as_ref()
        .map(|p| format_address(p.address))
        .unwrap_or_else(|| address_text.to_string());
    let device_rssi = props.as_ref().and_then(|p| p.rssi).unwrap_or(-100);

    {
        let mut inner = state.lock().expect("ble state lock");
        inner.phase = LinkPhase::GattReady;
        inner.device_name = device_name;
        inner.device_address = device_address;
        inner.rssi = device_rssi as i32;
        inner.status_detail = "GATT 已连接，等待设备鉴权……".into();
    }
    notify_ui_force(ui_refresh, true);

    init_live_on_connect(modbus_live);
    ensure_dashboard_poll_if_idle(poll_policy);

    let (write_tx, mut write_rx) = mpsc::unbounded_channel::<Vec<u8>>();
    let (session_cmd_tx, mut session_cmd_rx) = mpsc::unbounded_channel::<SessionCommand>();
    let protocol = Arc::new(Mutex::new({
        let mut session = ProtocolSession::new();
        session.set_modbus_live(modbus_live.clone());
        session
    }));
    let protocol_for_notify = protocol.clone();
    let protocol_for_write = protocol.clone();
    let state_for_notify = state.clone();
    let event_for_notify = event_tx.clone();
    let peripheral_for_write = peripheral.clone();
    let write_char_for_task = write_char.clone();
    let state_for_plain = state.clone();
    let event_for_plain = event_tx.clone();
    let ui_refresh_for_poll = ui_refresh.clone();
    let ui_refresh_for_notify = ui_refresh.clone();
    let modbus_live_poll = modbus_live.clone();
    let modbus_live_write = modbus_live.clone();
    let query_live_poll = query_live.clone();
    let query_live_write = query_live.clone();
    let query_gen_poll = query_generation.clone();
    let query_gen_write = query_generation.clone();
    let poll_policy_task = poll_policy.clone();
    let poll_policy_notify = poll_policy.clone();
    let write_tx_poll = write_tx.clone();
    let ota_live_session = ota_live.clone();
    let poll_policy_ota = poll_policy.clone();

    let modbus_gate: ModbusGate = Arc::new(tokio::sync::Mutex::new(()));
    let gate_for_poll = modbus_gate.clone();
    let gate_for_write = modbus_gate.clone();
    let event_for_poll = event_tx.clone();
    let protocol_for_poll_task = protocol.clone();
    let poll_task = tokio::spawn(async move {
        let wake = poll_policy_task
            .lock()
            .map(|p| p.wake.clone())
            .unwrap_or_else(|_| std::sync::Arc::new(tokio::sync::Notify::new()));
        loop {
            loop {
                let ready = protocol_for_poll_task
                    .lock()
                    .expect("protocol lock")
                    .modbus_ready();
                if ready {
                    break;
                }
                tokio::select! {
                    _ = tokio::time::sleep(Duration::from_millis(50)) => {}
                    _ = wake.notified() => {}
                }
            }

            let _ok = poll_foreground_once(
                &poll_policy_task,
                &protocol_for_poll_task,
                &write_tx_poll,
                &modbus_live_poll,
                &query_live_poll,
                &query_gen_poll,
                &gate_for_poll,
            )
            .await;
            notify_ui_force(&ui_refresh_for_poll, true);
            let _ = event_for_poll.send(());

            loop {
                let ota_busy = poll_policy_task
                    .lock()
                    .map(|p| p.ota_busy)
                    .unwrap_or(false);
                let probed = modbus_live_poll
                    .lock()
                    .map(|l| l.capabilities_probed)
                    .unwrap_or(false);
                // 探测尚未成功时不要空等 2s，立刻再发 POST-KEX。
                if probed || ota_busy {
                    tokio::select! {
                        _ = tokio::time::sleep(Duration::from_millis(POLL_INTERVAL_MS)), if !ota_busy => {}
                        _ = wake.notified() => {}
                    }
                }
                let ready = protocol_for_poll_task
                    .lock()
                    .expect("protocol lock")
                    .modbus_ready();
                if !ready {
                    break;
                }
                if poll_policy_task
                    .lock()
                    .map(|p| p.ota_busy)
                    .unwrap_or(false)
                {
                    continue;
                }
                let _ok = poll_foreground_once(
                    &poll_policy_task,
                    &protocol_for_poll_task,
                    &write_tx_poll,
                    &modbus_live_poll,
                    &query_live_poll,
                    &query_gen_poll,
                    &gate_for_poll,
                )
                .await;
                notify_ui_force(&ui_refresh_for_poll, true);
                let _ = event_for_poll.send(());
            }
        }
    });

    let poll_abort = poll_task.abort_handle();
    let peripheral_for_session = peripheral.clone();
    let throughput_hold = super::conn_opt::request_throughput(address_text).await;
    let can_write_without_response = write_char
        .properties
        .contains(CharPropFlags::WRITE_WITHOUT_RESPONSE);
    info!(
        target: "ble_gui::worker",
        "FF02 写特征 properties={:?} without_response={can_write_without_response}",
        write_char.properties,
    );
    let ota_wr_fallback = Arc::new(AtomicBool::new(false));
    let poll_policy_write = poll_policy.clone();
    let write_task = tokio::spawn(async move {
        while let Some(data) = write_rx.recv().await {
            let ota_fast = poll_policy_write
                .lock()
                .map(|p| p.ota_busy)
                .unwrap_or(false);
            write_ff02_air(
                &peripheral_for_write,
                &write_char_for_task,
                ota_fast,
                can_write_without_response,
                &ota_wr_fallback,
                &data,
            )
            .await;
        }
    });
    let write_abort_notify = write_task.abort_handle();
    let in_flight_writes: Arc<Mutex<Vec<tokio::task::AbortHandle>>> =
        Arc::new(Mutex::new(Vec::new()));
    let in_flight_for_notify = in_flight_writes.clone();
    let ota_live_notify = ota_live.clone();
    let expect_drop_notify = expect_drop.clone();
    let peripheral_watch = peripheral.clone();
    let adapter_watch = adapter.clone();
    let our_id = peripheral.id();
    let notify_task = tokio::spawn(async move {
        let mut notifications = match peripheral.notifications().await {
            Ok(stream) => stream,
            Err(_) => {
                poll_abort.abort();
                write_abort_notify.abort();
                return;
            }
        };
        let mut adapter_events = adapter_watch.events().await.ok();
        let mut link_tick = tokio::time::interval(Duration::from_millis(LINK_WATCH_MS));
        link_tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);

        let plain_timer = tokio::time::sleep(Duration::from_secs(6));
        tokio::pin!(plain_timer);

        loop {
            tokio::select! {
                _ = link_tick.tick() => {
                    if !peripheral_watch.is_connected().await.unwrap_or(false)
                        || should_force_drop_after_ota(&expect_drop_notify)
                    {
                        info!(target: "ble_gui::worker", "链路轮询：设备已离线");
                        on_peer_disconnect(
                            &poll_abort,
                            &write_abort_notify,
                            &state_for_notify,
                            &poll_policy_notify,
                            &modbus_live_write,
                            &ota_live_notify,
                            &ui_refresh_for_notify,
                            &event_for_notify,
                            take_disconnect_detail(&expect_drop_notify),
                        );
                        break;
                    }
                }
                maybe_central = async {
                    match adapter_events.as_mut() {
                        Some(stream) => stream.next().await,
                        None => pending().await,
                    }
                } => {
                    if let Some(CentralEvent::DeviceDisconnected(id)) = maybe_central {
                        if id == our_id {
                            info!(target: "ble_gui::worker", "收到 DeviceDisconnected");
                            on_peer_disconnect(
                                &poll_abort,
                                &write_abort_notify,
                                &state_for_notify,
                                &poll_policy_notify,
                                &modbus_live_write,
                                &ota_live_notify,
                                &ui_refresh_for_notify,
                                &event_for_notify,
                                take_disconnect_detail(&expect_drop_notify),
                            );
                            break;
                        }
                    }
                }
                _ = &mut plain_timer => {
                    let session = protocol_for_notify.lock().expect("protocol lock");
                    if session.phase == HandshakePhase::WaitingAuth && !session.auth_started() {
                        drop(session);
                        let mut session = protocol_for_notify.lock().expect("protocol lock");
                        session.mark_plaintext_mode();
                        if let Ok(mut inner) = state_for_plain.lock() {
                            inner.phase = LinkPhase::GattReady;
                            inner.status_detail = "设备未触发加密握手，按明文 Modbus 模式".into();
                        }
                        let _ = event_for_plain.send(());
                        notify_poll(&poll_policy_notify);
                    }
                }
                maybe_cmd = session_cmd_rx.recv() => {
                    match maybe_cmd {
                        Some(SessionCommand::WriteRegister { address, value }) => {
                            if let Ok(mut live) = modbus_live_write.lock() {
                                live.output_busy = true;
                            }
                            notify_ui_force(&ui_refresh_for_notify, true);
                            let protocol = protocol_for_write.clone();
                            let write_tx = write_tx.clone();
                            let live = modbus_live_write.clone();
                            let ui = ui_refresh_for_notify.clone();
                            let event_tx = event_for_notify.clone();
                            let gate = gate_for_write.clone();
                            let protocol_poll = protocol.clone();
                            let write_tx_poll = write_tx.clone();
                            let live_poll = modbus_live_write.clone();
                            let query_poll = query_live_write.clone();
                            let query_gen = query_gen_write.clone();
                            let policy_poll = poll_policy_notify.clone();
                            let gate_poll = gate_for_write.clone();
                            spawn_tracked(&in_flight_for_notify, async move {
                                let result = write_control_register(
                                    &protocol,
                                    &write_tx,
                                    &live,
                                    &gate,
                                    address,
                                    value,
                                )
                                .await;
                                if let Ok(mut live) = live.lock() {
                                    live.output_busy = false;
                                    if result.is_err() {
                                        warn!(
                                            target: "ble_gui::worker",
                                            "写寄存器 {address} 失败: {}",
                                            result.as_ref().err().unwrap_or(&String::new())
                                        );
                                    }
                                }
                                if result.is_ok() {
                                    poll_foreground_once(
                                        &policy_poll,
                                        &protocol_poll,
                                        &write_tx_poll,
                                        &live_poll,
                                        &query_poll,
                                        &query_gen,
                                        &gate_poll,
                                    )
                                    .await;
                                }
                                notify_ui_force(&ui, true);
                                let _ = event_tx.send(());
                            });
                        }
                        Some(SessionCommand::WriteHolding {
                            slave_id,
                            address,
                            values,
                            bit,
                            field,
                        }) => {
                            let protocol = protocol_for_write.clone();
                            let write_tx = write_tx.clone();
                            let ui = ui_refresh_for_notify.clone();
                            let event_tx = event_for_notify.clone();
                            let gate = gate_for_write.clone();
                            let protocol_poll = protocol.clone();
                            let write_tx_poll = write_tx.clone();
                            let live_poll = modbus_live_write.clone();
                            let query_poll = query_live_write.clone();
                            let query_gen = query_gen_write.clone();
                            let policy_poll = poll_policy_notify.clone();
                            let gate_poll = gate_for_write.clone();
                            spawn_tracked(&in_flight_for_notify, async move {
                                let result = write_holding_registers(
                                    &protocol,
                                    &write_tx,
                                    &gate,
                                    slave_id,
                                    address,
                                    &values,
                                    bit,
                                    field,
                                )
                                .await;
                                if let Err(err) = &result {
                                    warn!(
                                        target: "ble_gui::worker",
                                        "写保持寄存器 {address} 失败: {err}",
                                    );
                                }
                                if result.is_ok() {
                                    poll_foreground_once(
                                        &policy_poll,
                                        &protocol_poll,
                                        &write_tx_poll,
                                        &live_poll,
                                        &query_poll,
                                        &query_gen,
                                        &gate_poll,
                                    )
                                    .await;
                                }
                                notify_ui_force(&ui, true);
                                let _ = event_tx.send(());
                            });
                        }
                        Some(SessionCommand::StartOta { job }) => {
                            if let Ok(mut p) = poll_policy_ota.lock() {
                                p.ota_busy = true;
                                p.foreground = PollForeground::None;
                            }
                            notify_poll(&poll_policy_ota);
                            let protocol = protocol_for_write.clone();
                            let write_tx = write_tx.clone();
                            let gate = gate_for_write.clone();
                            let live = modbus_live_write.clone();
                            let ota = ota_live_session.clone();
                            let ui = ui_refresh_for_notify.clone();
                            let policy = poll_policy_ota.clone();
                            let expect_drop = expect_drop_notify.clone();
                            spawn_tracked(&in_flight_for_notify, async move {
                                run_ble_ota(
                                    &protocol, &write_tx, &gate, &live, &ota, &ui, job,
                                )
                                .await;
                                finish_ota_session(&policy, &ota, &expect_drop, &ui);
                            });
                        }
                        Some(SessionCommand::StartHttpOta { job }) => {
                            if let Ok(mut p) = poll_policy_ota.lock() {
                                p.ota_busy = true;
                                p.foreground = PollForeground::None;
                            }
                            notify_poll(&poll_policy_ota);
                            let protocol = protocol_for_write.clone();
                            let write_tx = write_tx.clone();
                            let gate = gate_for_write.clone();
                            let live = modbus_live_write.clone();
                            let ota = ota_live_session.clone();
                            let ui = ui_refresh_for_notify.clone();
                            let policy = poll_policy_ota.clone();
                            let expect_drop = expect_drop_notify.clone();
                            spawn_tracked(&in_flight_for_notify, async move {
                                run_http_ota(
                                    &protocol, &write_tx, &gate, &live, &ota, &ui, job,
                                )
                                .await;
                                finish_ota_session(&policy, &ota, &expect_drop, &ui);
                            });
                        }
                        None => {
                            poll_abort.abort();
                            write_abort_notify.abort();
                            break;
                        }
                    }
                }
                maybe_notification = notifications.next() => {
                    match maybe_notification {
                        Some(notification) => {
                            let response = {
                                let mut session = protocol_for_notify.lock().expect("protocol lock");
                                let was_encryption_ready = session.is_encryption_ready();
                                let was_modbus_ready = session.modbus_ready();
                                let result = session.on_notification(&notification.value);
                                let encrypted = session.is_encryption_ready();
                                let became_encrypted = !was_encryption_ready && encrypted;
                                let phase = session.phase;
                                let modbus_ready = session.modbus_ready();
                                let became_modbus_ready = !was_modbus_ready && modbus_ready;
                                (
                                    result,
                                    became_encrypted,
                                    phase,
                                    became_modbus_ready,
                                )
                            };
                            match response.0 {
                                Ok(Some(bytes)) => {
                                    let _ = write_tx.send(bytes);
                                }
                                Ok(None) => {}
                                Err(err) => {
                                    warn!(target: "ble_gui::worker", "协议处理异常: {err}");
                                    if let Ok(mut inner) = state_for_notify.lock() {
                                        inner.status_detail = format!("协议处理异常：{err}");
                                    }
                                    let _ = event_for_notify.send(());
                                }
                            }
                            if response.1 {
                                if let Ok(mut inner) = state_for_notify.lock() {
                                    inner.phase = LinkPhase::Encrypted;
                                    inner.encryption_ready = true;
                                    inner.status_detail = "加密链路已完成".into();
                                }
                                let _ = event_for_notify.send(());
                                notify_poll(&poll_policy_notify);
                            } else if response.2 == HandshakePhase::AuthDone {
                                if let Ok(mut inner) = state_for_notify.lock() {
                                    inner.phase = LinkPhase::Handshake;
                                    inner.status_detail = "2A2A 鉴权成功，ECDH 握手中……".into();
                                }
                                let _ = event_for_notify.send(());
                            } else if response.3 {
                                let _ = event_for_notify.send(());
                                notify_poll(&poll_policy_notify);
                            }
                        }
                        None => {
                            info!(
                                target: "ble_gui::worker",
                                "GATT 通知流结束，判定设备已断开",
                            );
                            on_peer_disconnect(
                                &poll_abort,
                                &write_abort_notify,
                                &state_for_notify,
                                &poll_policy_notify,
                                &modbus_live_write,
                                &ota_live_notify,
                                &ui_refresh_for_notify,
                                &event_for_notify,
                                take_disconnect_detail(&expect_drop_notify),
                            );
                            break;
                        }
                    }
                }
            }
        }
    });

    Ok(ActiveSession {
        notify_task,
        poll_task,
        write_task,
        peripheral: peripheral_for_session,
        cmd_tx: session_cmd_tx,
        in_flight_writes,
        _throughput_hold: throughput_hold,
    })
}
