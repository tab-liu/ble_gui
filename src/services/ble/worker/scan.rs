fn spawn_scan_loop(
    adapter: Adapter,
    state: SharedBleState,
    event_tx: std::sync::mpsc::Sender<()>,
    ui_refresh: super::UiRefreshSlot,
    known: KnownMap,
) -> tokio::task::JoinHandle<()> {
    tokio::spawn(async move {
        run_scan_loop(adapter, state, event_tx, ui_refresh, known).await;
    })
}

/// 清空过期扫描缓存并重新开扫。换设备前必须这样，否则 Windows 会拿旧 GATT 句柄去连。
async fn restart_scan(
    adapter: &Adapter,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    known: &KnownMap,
    scan_task: &mut Option<tokio::task::JoinHandle<()>>,
    detail: impl Into<String>,
) {
    if let Some(task) = scan_task.take() {
        task.abort();
    }
    apply_scanning_state(state, known, detail);
    notify_ui_force(ui_refresh, true);
    if tokio::time::timeout(GATT_DISCONNECT_TIMEOUT, adapter.stop_scan())
        .await
        .is_err()
    {
        warn!(target: "ble_gui::worker", "停止扫描超时，继续重新扫描");
    }
    *scan_task = Some(spawn_scan_loop(
        adapter.clone(),
        state.clone(),
        event_tx.clone(),
        ui_refresh.clone(),
        known.clone(),
    ));
}

/// 停扫描并回到空闲。不断开后自动再扫，避免 Windows 上卡在「扫描中」且列表为空。
async fn enter_idle(
    adapter: &Adapter,
    state: &SharedBleState,
    ui_refresh: &super::UiRefreshSlot,
    scan_task: &mut Option<tokio::task::JoinHandle<()>>,
    detail: impl Into<String>,
) {
    if let Some(task) = scan_task.take() {
        task.abort();
    }
    set_phase(state, LinkPhase::Idle, detail);
    notify_ui_force(ui_refresh, true);
    if tokio::time::timeout(GATT_DISCONNECT_TIMEOUT, adapter.stop_scan())
        .await
        .is_err()
    {
        warn!(target: "ble_gui::worker", "停止扫描超时，已回到空闲");
    }
}
async fn run_scan_loop(
    adapter: Adapter,
    state: SharedBleState,
    event_tx: std::sync::mpsc::Sender<()>,
    ui_refresh: super::UiRefreshSlot,
    known: KnownMap,
) {
    // 必须先订阅事件流再 start_scan，否则广播事件会在无人接收时被丢弃。
    let mut events = match adapter.events().await {
        Ok(events) => events,
        Err(err) => {
            let msg = message_for_scan_failure(&adapter, &err).await;
            set_phase(&state, LinkPhase::Idle, msg);
            notify_ui_force(&ui_refresh, true);
            return;
        }
    };

    if let Err(err) = adapter.start_scan(ScanFilter::default()).await {
        warn!(target: "ble_gui::services::ble", "start_scan failed: {err}");
        set_phase(
            &state,
            LinkPhase::Idle,
            message_for_scan_failure(&adapter, &err).await,
        );
        notify_ui_force(&ui_refresh, true);
        return;
    }

    let mut name_resolve_pending: HashSet<String> = HashSet::new();

    // 立即同步一次 btleplug 已缓存的外设（重复点扫描能显示，往往是因为触发了这类同步）。
    sync_all_peripherals(
        &adapter,
        &state,
        &event_tx,
        &ui_refresh,
        &known,
        &mut name_resolve_pending,
        true,
    )
    .await;

    let mut poll = tokio::time::interval(Duration::from_millis(SCAN_SYNC_INTERVAL_MS));
    poll.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
    // 跳过 interval 首次立即触发的 tick，避免与上方 sync 重复。
    poll.tick().await;

    loop {
        if state.lock().expect("ble state lock").phase != LinkPhase::Scanning {
            break;
        }

        tokio::select! {
            _ = poll.tick() => {
                sync_all_peripherals(
                    &adapter,
                    &state,
                    &event_tx,
                    &ui_refresh,
                    &known,
                    &mut name_resolve_pending,
                    true,
                ).await;
            }
            maybe_event = events.next() => {
                match maybe_event {
                    None => break,
                    Some(event) => {
                        if state.lock().expect("ble state lock").phase != LinkPhase::Scanning {
                            break;
                        }
                        match event {
                            CentralEvent::DeviceDiscovered(id) | CentralEvent::DeviceUpdated(id) => {
                                if ingest_peripheral(
                                    &adapter,
                                    &id,
                                    &state,
                                    &event_tx,
                                    &ui_refresh,
                                    &known,
                                    &mut name_resolve_pending,
                                    false,
                                    true,
                                )
                                .await
                                {
                                    notify_scan_list_changed(&state, &ui_refresh);
                                }
                            }
                            CentralEvent::ServicesAdvertisement { id, services } => {
                                let force_target = services.iter().any(|uuid| matches_ff00(uuid));
                                if ingest_peripheral(
                                    &adapter,
                                    &id,
                                    &state,
                                    &event_tx,
                                    &ui_refresh,
                                    &known,
                                    &mut name_resolve_pending,
                                    force_target,
                                    true,
                                )
                                .await
                                {
                                    notify_scan_list_changed(&state, &ui_refresh);
                                }
                            }
                            CentralEvent::ManufacturerDataAdvertisement { id, manufacturer_data } => {
                                let force_target = manufacturer_data
                                    .values()
                                    .any(|data| is_target_manufacturer_data(data));
                                if force_target
                                    && ingest_peripheral(
                                        &adapter,
                                        &id,
                                        &state,
                                        &event_tx,
                                        &ui_refresh,
                                        &known,
                                        &mut name_resolve_pending,
                                        true,
                                        true,
                                    )
                                    .await
                                {
                                    notify_scan_list_changed(&state, &ui_refresh);
                                }
                            }
                            _ => {}
                        }
                    }
                }
            }
        }
    }

    let _ = adapter.stop_scan().await;
    if state.lock().expect("ble state lock").phase == LinkPhase::Scanning {
        set_phase(&state, LinkPhase::Idle, "扫描已结束");
        notify_ui_force(&ui_refresh, true);
    }
}

/// 轮询适配器已发现的所有外设，不依赖事件是否送达。
async fn sync_all_peripherals(
    adapter: &Adapter,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    known: &KnownMap,
    name_resolve_pending: &mut HashSet<String>,
    require_scanning: bool,
) {
    let Ok(peripherals) = adapter.peripherals().await else {
        return;
    };
    let gen_before = state.lock().expect("ble state lock").scan_list_generation;
    let mut list_changed = false;
    for peripheral in peripherals {
        if ingest_peripheral(
            adapter,
            &peripheral.id(),
            state,
            event_tx,
            ui_refresh,
            known,
            name_resolve_pending,
            false,
            require_scanning,
        )
        .await
        {
            list_changed = true;
        }
    }
    if list_changed {
        update_scan_status_detail(state);
        notify_ui_force(ui_refresh, true);
    } else if !require_scanning
        && gen_before != state.lock().expect("ble state lock").scan_list_generation
    {
        notify_ui_force(ui_refresh, true);
    }
}

fn notify_scan_list_changed(state: &SharedBleState, ui_refresh: &super::UiRefreshSlot) {
    update_scan_status_detail(state);
    notify_ui_force(ui_refresh, true);
}

fn update_scan_status_detail(state: &SharedBleState) {
    let mut inner = state.lock().expect("ble state lock");
    if inner.phase != LinkPhase::Scanning {
        return;
    }
    let total = inner.scan_devices.len();
    let named = inner
        .scan_devices
        .iter()
        .filter(|d| !crate::services::ble::state::is_placeholder_scan_name(&d.name))
        .count();
    inner.status_detail = format!("已发现 {total} 个蓝牙设备，其中 {named} 个已解析名称");
}

async fn ingest_peripheral(
    adapter: &Adapter,
    id: &PeripheralId,
    state: &SharedBleState,
    event_tx: &std::sync::mpsc::Sender<()>,
    ui_refresh: &super::UiRefreshSlot,
    known: &KnownMap,
    name_resolve_pending: &mut HashSet<String>,
    force_target: bool,
    require_scanning: bool,
) -> bool {
    let Ok(peripheral) = find_peripheral_by_id(adapter, id).await else {
        return false;
    };
    let Ok(Some(props)) = peripheral.properties().await else {
        return false;
    };

    let address = format_address(props.address);
    known
        .lock()
        .expect("known lock")
        .insert(address.clone(), id.clone());

    let is_target = force_target || is_target_properties(&props);
    let name = props
        .local_name
        .as_deref()
        .unwrap_or("")
        .trim()
        .to_string();
    let rssi = props.rssi.unwrap_or(-100);

    if !is_target {
        return false;
    }

    let list_changed = {
        let mut inner = state.lock().expect("ble state lock");
        if require_scanning && inner.phase != LinkPhase::Scanning {
            return false;
        }
        let before_len = inner.scan_devices.len();
        let gen_before = inner.scan_list_generation;
        let link_hint = to_scan_link_hint(adv_link_hint_from_properties(&props));
        inner.upsert_advertisement(&name, &address, rssi as i32, is_target, link_hint);
        inner.scan_list_generation > gen_before || inner.scan_devices.len() > before_len
    };

    if name.is_empty() && name_resolve_pending.insert(address.clone()) {
        let peripheral = peripheral.clone();
        let state = state.clone();
        let event_tx = event_tx.clone();
        let ui_refresh = ui_refresh.clone();
        tokio::spawn(async move {
            resolve_device_name(peripheral, address, state, event_tx, ui_refresh).await;
        });
    }

    list_changed
}

/// 轮询 peripheral.properties()，等待 Scan Response 中的 Complete Local Name 到达。
async fn resolve_device_name(
    peripheral: Peripheral,
    address: String,
    state: SharedBleState,
    _event_tx: std::sync::mpsc::Sender<()>,
    ui_refresh: super::UiRefreshSlot,
) {
    if let Some(name) = super::win_name::resolve_device_name_from_address(&address).await {
        let Ok(Some(props)) = peripheral.properties().await else {
            return;
        };
        let rssi = props.rssi.unwrap_or(-100);
        {
            let mut inner = state.lock().expect("ble state lock");
            if inner.phase != LinkPhase::Scanning {
                return;
            }
            let hint = to_scan_link_hint(adv_link_hint_from_properties(&props));
            inner.upsert_advertisement(
                &name,
                &address,
                rssi as i32,
                is_target_properties(&props),
                hint,
            );
        }
        update_scan_status_detail(&state);
        notify_ui_force(&ui_refresh, true);
        return;
    }

    for _ in 0..20 {
        tokio::time::sleep(Duration::from_millis(200)).await;

        let scanning = state
            .lock()
            .map(|inner| inner.phase == LinkPhase::Scanning)
            .unwrap_or(false);
        if !scanning {
            return;
        }

        let Ok(Some(props)) = peripheral.properties().await else {
            continue;
        };
        let name = props
            .local_name
            .as_deref()
            .unwrap_or("")
            .trim()
            .to_string();
        if name.is_empty() {
            continue;
        }

        let is_target = is_target_properties(&props);
        let rssi = props.rssi.unwrap_or(-100);
        {
            let mut inner = state.lock().expect("ble state lock");
            if inner.phase != LinkPhase::Scanning {
                return;
            }
            let hint = to_scan_link_hint(adv_link_hint_from_properties(&props));
            inner.upsert_advertisement(&name, &address, rssi as i32, is_target, hint);
        }
        update_scan_status_detail(&state);
        notify_ui_force(&ui_refresh, true);
        return;
    }
}
