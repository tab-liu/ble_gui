type KnownMap = Arc<Mutex<HashMap<String, PeripheralId>>>;

/// IOT 升级成功后 Windows 可能仍报已连接；到期则按离线处理，不自动重连。
type SharedExpectDrop = Arc<Mutex<Option<Instant>>>;

struct ActiveSession {
    notify_task: tokio::task::JoinHandle<()>,
    poll_task: tokio::task::JoinHandle<()>,
    write_task: tokio::task::JoinHandle<()>,
    peripheral: Peripheral,
    cmd_tx: tokio::sync::mpsc::UnboundedSender<SessionCommand>,
    in_flight_writes: Arc<Mutex<Vec<tokio::task::AbortHandle>>>,
    /// 必须持有，丢掉后系统可能把连接间隔改回省电档（尤其 Windows）。
    _throughput_hold: Option<super::conn_opt::ThroughputHold>,
}

fn abort_in_flight_writes(jobs: &Arc<Mutex<Vec<tokio::task::AbortHandle>>>) {
    if let Ok(mut g) = jobs.lock() {
        for h in g.drain(..) {
            h.abort();
        }
    }
}

fn spawn_tracked(
    jobs: &Arc<Mutex<Vec<tokio::task::AbortHandle>>>,
    fut: impl std::future::Future<Output = ()> + Send + 'static,
) {
    let handle = tokio::spawn(fut);
    if let Ok(mut g) = jobs.lock() {
        g.retain(|h| !h.is_finished());
        g.push(handle.abort_handle());
    }
}

fn finish_ota_session(
    policy: &SharedPollPolicy,
    ota: &SharedOtaLive,
    expect_drop: &SharedExpectDrop,
    ui: &super::UiRefreshSlot,
) {
    if let Ok(mut p) = policy.lock() {
        p.ota_busy = false;
    }
    notify_poll(policy);
    let iot_ok = ota
        .lock()
        .ok()
        .is_some_and(|g| g.phase == PHASE_SUCCESS && g.ble_only);
    if iot_ok {
        info!(
            target: "ble_gui::ota",
            "IOT 升级成功，等待设备重启后刷新连接状态",
        );
        if let Ok(mut slot) = expect_drop.lock() {
            *slot = Some(Instant::now() + Duration::from_secs(2));
        }
    }
    notify_ui_force(ui, true);
}

fn abort_session(active: &ActiveSession) {
    abort_in_flight_writes(&active.in_flight_writes);
    active.poll_task.abort();
    active.notify_task.abort();
    active.write_task.abort();
}

fn take_ota_disconnect_request(ota: &SharedOtaLive) -> bool {
    let Ok(mut g) = ota.lock() else {
        return false;
    };
    let requested = g.request_disconnect;
    g.request_disconnect = false;
    requested
}

fn on_peer_disconnect(
    poll_abort: &tokio::task::AbortHandle,
    write_abort: &tokio::task::AbortHandle,
    state: &SharedBleState,
    poll_policy: &SharedPollPolicy,
    modbus_live: &SharedModbusLive,
    ota: &crate::services::firmware::SharedOtaLive,
    ui: &super::UiRefreshSlot,
    event_tx: &std::sync::mpsc::Sender<()>,
    detail: &str,
) {
    poll_abort.abort();
    write_abort.abort();
    stop_polling(poll_policy);
    if let Ok(mut p) = poll_policy.lock() {
        p.ota_busy = false;
    }
    clear_live_on_disconnect(modbus_live);
    if let Ok(mut g) = ota.lock() {
        if g.running {
            g.running = false;
            g.phase = crate::services::firmware::PHASE_FAILED;
            g.result_text = "升级失败".into();
            g.fail_reason = "升级过程中蓝牙断开".into();
            g.stage_text = "升级失败".into();
            g.status_text = g.fail_reason.clone();
            g.freeze_elapsed();
        }
    }
    if let Ok(mut inner) = state.lock() {
        inner.phase = LinkPhase::Idle;
        inner.device_name.clear();
        inner.device_address.clear();
        inner.rssi = 0;
        inner.encryption_ready = false;
        inner.status_detail = detail.into();
    }
    notify_ui_force(ui, true);
    let _ = event_tx.send(());
}

fn still_shows_connected(state: &SharedBleState) -> bool {
    matches!(
        state.lock().map(|s| s.phase.clone()).unwrap_or(LinkPhase::Idle),
        LinkPhase::GattReady | LinkPhase::Handshake | LinkPhase::Encrypted
    )
}

fn take_disconnect_detail(expect_drop: &SharedExpectDrop) -> &'static str {
    if expect_drop
        .lock()
        .ok()
        .and_then(|mut g| g.take())
        .is_some()
    {
        "设备已断开（升级后重启）"
    } else {
        "设备已断开"
    }
}

fn should_force_drop_after_ota(expect_drop: &SharedExpectDrop) -> bool {
    expect_drop
        .lock()
        .ok()
        .and_then(|g| g.as_ref().copied())
        .is_some_and(|t| Instant::now() >= t)
}

/// ATT 载荷上限（MTU 247 − 3）。OTA 用无应答写，避免每片都等 ATT 回包。
const GATT_ATT_PAYLOAD: usize = 244;

async fn write_ff02_air(
    peripheral: &Peripheral,
    write_char: &Characteristic,
    ota_fast: bool,
    can_without_response: bool,
    fallback_with_response: &AtomicBool,
    data: &[u8],
) {
    let write_type = if ota_fast
        && can_without_response
        && !fallback_with_response.load(Ordering::Relaxed)
    {
        WriteType::WithoutResponse
    } else {
        WriteType::WithResponse
    };
    if ota_fast {
        static LOGGED: AtomicBool = AtomicBool::new(false);
        if !LOGGED.swap(true, Ordering::Relaxed) {
            info!(
                target: "ble_gui::ota",
                "OTA GATT 写入: {} 模式, {}B 分 {} 片 (FF02 without_response={can_without_response})",
                if write_type == WriteType::WithoutResponse {
                    "无应答"
                } else {
                    "有应答"
                },
                data.len(),
                data.len().div_ceil(GATT_ATT_PAYLOAD),
            );
        }
    }
    if write_type == WriteType::WithoutResponse {
        write_ota_without_response(
            peripheral,
            write_char,
            fallback_with_response,
            data,
        )
        .await;
        return;
    }
    write_chunks_sequential(
        peripheral,
        write_char,
        write_type,
        fallback_with_response,
        data,
    )
    .await;
}

/// 同一 XMODEM 包的 244B 分片同时提交，让控制器在一个连接事件里发出多片。
/// 若系统只允许 1 个未完成写，则从未成功的那一片起改为串行，已发出的片不重发。
async fn write_ota_without_response(
    peripheral: &Peripheral,
    write_char: &Characteristic,
    fallback_with_response: &AtomicBool,
    data: &[u8],
) {
    let t0 = std::time::Instant::now();
    let chunks: Vec<Vec<u8>> = data
        .chunks(GATT_ATT_PAYLOAD)
        .map(|c| c.to_vec())
        .collect();
    if chunks.is_empty() {
        return;
    }
    let futs: Vec<_> = chunks
        .iter()
        .map(|c| peripheral.write(write_char, c, WriteType::WithoutResponse))
        .collect();
    let results = futures::future::join_all(futs).await;
    let first_fail = results.iter().position(|r| r.is_err());
    match first_fail {
        None => {
            static TIMED: AtomicBool = AtomicBool::new(false);
            if !TIMED.swap(true, Ordering::Relaxed) {
                info!(
                    target: "ble_gui::ota",
                    "OTA 无应答并行提交 {} 片 / {}B, {}ms",
                    chunks.len(),
                    data.len(),
                    t0.elapsed().as_millis(),
                );
            }
        }
        Some(0) => {
            warn!(
                target: "ble_gui::ota",
                "OTA 并行无应答写不被支持，改为串行: {}",
                results[0].as_ref().err().map(|e| e.to_string()).unwrap_or_default(),
            );
            write_chunks_sequential(
                peripheral,
                write_char,
                WriteType::WithoutResponse,
                fallback_with_response,
                data,
            )
            .await;
        }
        Some(i) => {
            warn!(
                target: "ble_gui::ota",
                "OTA 并行写在第 {}/{} 片失败，其余串行补发",
                i + 1,
                chunks.len(),
            );
            let rest: Vec<u8> = chunks[i..].iter().flatten().copied().collect();
            write_chunks_sequential(
                peripheral,
                write_char,
                WriteType::WithoutResponse,
                fallback_with_response,
                &rest,
            )
            .await;
        }
    }
}

async fn write_chunks_sequential(
    peripheral: &Peripheral,
    write_char: &Characteristic,
    mut write_type: WriteType,
    fallback_with_response: &AtomicBool,
    data: &[u8],
) {
    for chunk in data.chunks(GATT_ATT_PAYLOAD) {
        match peripheral.write(write_char, chunk, write_type).await {
            Ok(()) => {}
            Err(err) if write_type == WriteType::WithoutResponse => {
                warn!(
                    target: "ble_gui::worker",
                    "OTA 无应答写入失败，后续改回有应答（会明显变慢）: {err}",
                );
                fallback_with_response.store(true, Ordering::Relaxed);
                write_type = WriteType::WithResponse;
                if let Err(err) = peripheral.write(write_char, chunk, write_type).await {
                    warn!(target: "ble_gui::worker", "GATT 写入失败: {err}");
                    return;
                }
            }
            Err(err) => {
                warn!(target: "ble_gui::worker", "GATT 写入失败: {err}");
                return;
            }
        }
    }
}

fn stop_polling(poll_policy: &SharedPollPolicy) {
    let mut policy = poll_policy.lock().expect("poll policy lock");
    if policy.foreground != PollForeground::None {
        log::info!(
            target: "ble_gui::poll",
            "轮询策略: {} → 无（停止轮询）",
            describe_poll_foreground(&policy.foreground),
        );
        policy.foreground = PollForeground::None;
    }
}

fn set_phase(state: &SharedBleState, phase: LinkPhase, detail: impl Into<String>) {
    if let Ok(mut inner) = state.lock() {
        inner.phase = phase;
        inner.status_detail = detail.into();
    }
}

fn notify_ui_force(
    ui_refresh: &super::UiRefreshSlot,
    force: bool,
) {
    schedule_ui_refresh(ui_refresh, force);
}

async fn disconnect_peripheral(peripheral: &Peripheral) {
    match tokio::time::timeout(GATT_DISCONNECT_TIMEOUT, peripheral.disconnect()).await {
        Ok(Ok(())) => {}
        Ok(Err(err)) => {
            warn!(target: "ble_gui::worker", "GATT 断开失败: {err}");
        }
        Err(_) => {
            warn!(
                target: "ble_gui::worker",
                "GATT 断开超时（{}s），继续清理会话",
                GATT_DISCONNECT_TIMEOUT.as_secs()
            );
        }
    }
}

fn apply_scanning_state(
    state: &SharedBleState,
    known: &KnownMap,
    detail: impl Into<String>,
) {
    {
        let mut inner = state.lock().expect("ble state lock");
        inner.phase = LinkPhase::Scanning;
        inner.device_name.clear();
        inner.device_address.clear();
        inner.rssi = 0;
        inner.encryption_ready = false;
        inner.scan_devices.clear();
        inner.scan_list_generation += 1;
        inner.status_detail = detail.into();
    }
    known.lock().expect("known lock").clear();
}

/// 扫描列表 UI 刷新间隔；连接/阶段变化等仍可通过 force 立即刷新。
const UI_REFRESH_INTERVAL_MS: u64 = 1000;
/// 轮询 btleplug 已缓存外设的间隔（补充事件流未送达的广播）。
const SCAN_SYNC_INTERVAL_MS: u64 = 1000;
/// 连接前句柄失效时，定向找回目标设备的超时。
const REDISCOVER_TIMEOUT: Duration = Duration::from_secs(12);
/// Windows 停扫描后 radio 尚未释放时，立刻 GetGattServices 常返回 Unreachable（Not connected）。
const CONNECT_SETTLE_MS: u64 = 400;
const CONNECT_RETRY_ATTEMPTS: u32 = 4;
const CONNECT_RETRY_BASE_MS: u64 = 400;
const LINK_WATCH_MS: u64 = 500;
/// Windows GATT 断开可能挂起；超时后继续清理，避免「断开」按钮无响应。
const GATT_DISCONNECT_TIMEOUT: Duration = Duration::from_secs(2);

const MSG_DEVICE_NOT_NEARBY: &str =
    "附近未发现该设备（可能已关机、距离过远，或已被其它设备连接后停止广播）";
const MSG_DEVICE_OCCUPIED: &str =
    "设备仍在附近广播，但标记为已连接；请先在其它手机/电脑上断开后再试";

fn schedule_ui_refresh(ui_refresh: &super::UiRefreshSlot, force: bool) {
    use std::sync::atomic::{AtomicU64, Ordering};

    static LAST_MS: AtomicU64 = AtomicU64::new(0);
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as u64)
        .unwrap_or(0);
    let last = LAST_MS.load(Ordering::Relaxed);
    if !force && now.saturating_sub(last) < UI_REFRESH_INTERVAL_MS {
        return;
    }
    LAST_MS.store(now, Ordering::Relaxed);

    if let Ok(guard) = ui_refresh.lock() {
        if let Some(hook) = guard.as_ref() {
            hook();
        }
    }
}

fn format_address(addr: BDAddr) -> String {
    addr.to_string()
}

fn parse_address(text: &str) -> Option<BDAddr> {
    text.parse().ok()
}

fn to_scan_link_hint(hint: AdvLinkHint) -> ScanLinkHint {
    match hint {
        AdvLinkHint::Unknown => ScanLinkHint::Unknown,
        AdvLinkHint::Available => ScanLinkHint::Available,
        AdvLinkHint::Occupied => ScanLinkHint::Occupied,
    }
}

fn connect_cancelled(flag: &AtomicBool) -> bool {
    flag.load(Ordering::Acquire)
}

async fn sleep_unless_cancelled(flag: &AtomicBool, dur: Duration) -> Result<(), String> {
    let end = tokio::time::Instant::now() + dur;
    loop {
        if connect_cancelled(flag) {
            return Err("已取消连接".into());
        }
        let now = tokio::time::Instant::now();
        if now >= end {
            return Ok(());
        }
        let slice = (end - now).min(Duration::from_millis(50));
        tokio::time::sleep(slice).await;
    }
}

/// Windows 上 btleplug 的 connect() 实际是 Uncached GetGattServices；
/// 扫描刚停或首次 GATT 常返回 Unreachable → Not connected，第二次即可成功。
async fn connect_gatt_with_retry(
    peripheral: &Peripheral,
    cancel_connect: &AtomicBool,
    state: &SharedBleState,
    ui_refresh: &super::UiRefreshSlot,
    address_text: &str,
) -> Result<(), String> {
    sleep_unless_cancelled(cancel_connect, Duration::from_millis(CONNECT_SETTLE_MS)).await?;

    for attempt in 1..=CONNECT_RETRY_ATTEMPTS {
        if connect_cancelled(cancel_connect) {
            return Err("已取消连接".into());
        }
        if peripheral.is_connected().await.unwrap_or(false) {
            return Ok(());
        }
        match peripheral.connect().await {
            Ok(()) => {
                if attempt > 1 {
                    info!(
                        target: "ble_gui::services::ble",
                        "GATT connect succeeded on attempt {attempt}/{CONNECT_RETRY_ATTEMPTS} for {address_text}"
                    );
                }
                return Ok(());
            }
            Err(err) => {
                warn!(
                    target: "ble_gui::services::ble",
                    "GATT connect attempt {attempt}/{CONNECT_RETRY_ATTEMPTS} for {address_text} failed: {err}"
                );
                if attempt == CONNECT_RETRY_ATTEMPTS {
                    return Err(format!("BLE 连接失败：{err}"));
                }
                set_phase(
                    state,
                    LinkPhase::Connecting,
                    format!(
                        "连接未就绪，正在重试（{attempt}/{CONNECT_RETRY_ATTEMPTS}）……"
                    ),
                );
                notify_ui_force(ui_refresh, true);
                sleep_unless_cancelled(
                    cancel_connect,
                    Duration::from_millis(CONNECT_RETRY_BASE_MS * u64::from(attempt)),
                )
                .await?;
            }
        }
    }
    Err("BLE 连接失败：Not connected".into())
}
