async fn discover_services_with_retry(
    peripheral: &Peripheral,
    attempts: usize,
    cancel_connect: &AtomicBool,
) -> Result<(), String> {
    for attempt in 1..=attempts {
        if connect_cancelled(cancel_connect) {
            return Err("已取消连接".into());
        }
        match peripheral.discover_services().await {
            Ok(()) => {
                let has_target = peripheral.characteristics().iter().any(|c| {
                    c.uuid == write_uuid() || c.uuid == notify_uuid() || c.uuid == notify_uuid_ff03()
                });
                if has_target {
                    return Ok(());
                }
            }
            Err(err) => {
                let msg = err.to_string();
                // 对象已关闭 / 已中止：链路已废，再重试只会空转。
                let link_dead = msg.contains("0x80000013")
                    || msg.contains("已关闭")
                    || msg.contains("0x80004004")
                    || msg.contains("已中止");
                if link_dead && attempt >= 2 {
                    return Err(format!("GATT 服务发现失败：{err}"));
                }
                if attempt == attempts {
                    return Err(format!("GATT 服务发现失败：{err}"));
                }
            }
        }
        sleep_unless_cancelled(cancel_connect, Duration::from_millis(500)).await?;
    }
    Err("GATT 已连接但未发现 FF00 服务".to_string())
}

async fn find_peripheral_by_id(adapter: &Adapter, id: &PeripheralId) -> Result<Peripheral, ()> {
    adapter.peripheral(id).await.map_err(|_| ())
}

enum RediscoverError {
    Occupied,
    TimedOut,
    Cancelled,
}

fn address_matches(props_addr: BDAddr, target_text: &str, target_norm: &str) -> bool {
    let text = format_address(props_addr);
    text == target_text || normalize_address(&text) == target_norm
}

async fn note_peripheral_for_target(
    adapter: &Adapter,
    id: &PeripheralId,
    target_text: &str,
    target_norm: &str,
    known: &KnownMap,
) -> Result<Option<Peripheral>, RediscoverError> {
    let Ok(peripheral) = find_peripheral_by_id(adapter, id).await else {
        return Ok(None);
    };
    let Ok(Some(props)) = peripheral.properties().await else {
        return Ok(None);
    };
    if !address_matches(props.address, target_text, target_norm) {
        return Ok(None);
    }

    let address = format_address(props.address);
    known
        .lock()
        .expect("known lock")
        .insert(address, id.clone());

    if adv_link_hint_from_properties(&props) == AdvLinkHint::Occupied {
        return Err(RediscoverError::Occupied);
    }
    Ok(Some(peripheral))
}

/// 句柄失效时短时扫描，仅找回指定地址对应的外设。
/// `require_fresh_adv`：不采用 adapter 里过期的 Peripheral，等新广播（换设备时必须）。
async fn rediscover_target(
    adapter: &Adapter,
    address_text: &str,
    known: &KnownMap,
    state: &SharedBleState,
    ui_refresh: &super::UiRefreshSlot,
    cancel_connect: &AtomicBool,
    require_fresh_adv: bool,
) -> Result<Peripheral, RediscoverError> {
    let target_norm = normalize_address(address_text);
    let deadline = tokio::time::Instant::now() + REDISCOVER_TIMEOUT;

    let mut events = adapter
        .events()
        .await
        .map_err(|_| RediscoverError::TimedOut)?;
    if adapter.start_scan(ScanFilter::default()).await.is_err() {
        return Err(RediscoverError::TimedOut);
    }

    if !require_fresh_adv {
        if let Ok(peripherals) = adapter.peripherals().await {
            for peripheral in peripherals {
                if connect_cancelled(cancel_connect) {
                    let _ = adapter.stop_scan().await;
                    return Err(RediscoverError::Cancelled);
                }
                let id = peripheral.id();
                match note_peripheral_for_target(adapter, &id, address_text, &target_norm, known)
                    .await
                {
                    Ok(Some(p)) => {
                        let _ = adapter.stop_scan().await;
                        return Ok(p);
                    }
                    Err(RediscoverError::Occupied) => {
                        let _ = adapter.stop_scan().await;
                        return Err(RediscoverError::Occupied);
                    }
                    Ok(None) | Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled) => {}
                }
            }
        }
    }

    let mut poll = tokio::time::interval(Duration::from_millis(SCAN_SYNC_INTERVAL_MS));
    poll.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
    poll.tick().await;

    let result = loop {
        if connect_cancelled(cancel_connect)
            || state.lock().expect("ble state lock").phase != LinkPhase::Connecting
        {
            break Err(RediscoverError::Cancelled);
        }
        if tokio::time::Instant::now() >= deadline {
            break Err(RediscoverError::TimedOut);
        }

        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        tokio::select! {
            _ = poll.tick() => {
                if require_fresh_adv {
                    continue;
                }
                if connect_cancelled(cancel_connect) {
                    break Err(RediscoverError::Cancelled);
                }
                if let Some(p) = find_peripheral(adapter, address_text, known).await {
                    break Ok(p);
                }
                let mut occupied = false;
                let mut found = None;
                if let Ok(peripherals) = adapter.peripherals().await {
                    for peripheral in peripherals {
                        let id = peripheral.id();
                        match note_peripheral_for_target(
                            adapter,
                            &id,
                            address_text,
                            &target_norm,
                            known,
                        )
                        .await
                        {
                            Ok(Some(p)) => {
                                found = Some(p);
                                break;
                            }
                            Err(RediscoverError::Occupied) => {
                                occupied = true;
                                break;
                            }
                            Ok(None) | Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled) => {}
                        }
                    }
                }
                if occupied {
                    break Err(RediscoverError::Occupied);
                }
                if let Some(p) = found {
                    break Ok(p);
                }
            }
            maybe_event = events.next() => {
                match maybe_event {
                    None => break Err(RediscoverError::TimedOut),
                    Some(CentralEvent::DeviceDiscovered(id) | CentralEvent::DeviceUpdated(id))
                    | Some(CentralEvent::ServicesAdvertisement { id, .. })
                    | Some(CentralEvent::ManufacturerDataAdvertisement { id, .. }) => {
                        match note_peripheral_for_target(
                            adapter,
                            &id,
                            address_text,
                            &target_norm,
                            known,
                        )
                        .await
                        {
                            Ok(Some(p)) => break Ok(p),
                            Err(RediscoverError::Occupied) => {
                                break Err(RediscoverError::Occupied);
                            }
                            Ok(None) | Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled) => {}
                        }
                    }
                    Some(_) => {}
                }
            }
            _ = tokio::time::sleep(remaining) => {
                break Err(RediscoverError::TimedOut);
            }
        }
    };

    let _ = adapter.stop_scan().await;
    if matches!(result, Err(RediscoverError::TimedOut) | Err(RediscoverError::Cancelled)) {
        notify_ui_force(ui_refresh, true);
    }
    result
}

async fn find_peripheral(
    adapter: &Adapter,
    address_text: &str,
    known: &KnownMap,
) -> Option<Peripheral> {
    let target_norm = normalize_address(address_text);
    let parsed = parse_address(address_text);
    let id = {
        let map = known.lock().expect("known lock");
        map.get(address_text)
            .cloned()
            .or_else(|| {
                map.iter()
                    .find(|(k, _)| normalize_address(k) == target_norm)
                    .map(|(_, v)| v.clone())
            })
    };
    if let Some(id) = id {
        if let Ok(peripheral) = find_peripheral_by_id(adapter, &id).await {
            return Some(peripheral);
        }
    }

    if let Ok(peripherals) = adapter.peripherals().await {
        for peripheral in peripherals {
            if let Ok(Some(props)) = peripheral.properties().await {
                let text = format_address(props.address);
                let matched = text == address_text
                    || normalize_address(&text) == target_norm
                    || parsed.is_some_and(|t| props.address == t);
                if matched {
                    known
                        .lock()
                        .expect("known lock")
                        .insert(text, peripheral.id());
                    return Some(peripheral);
                }
            }
        }
    }
    None
}

fn normalize_address(addr: &str) -> String {
    addr.replace('-', ":").to_ascii_uppercase()
}
