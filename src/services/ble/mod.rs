//! 蓝牙连接服务：同步 UI 接口 + Tokio/btleplug 后台 worker。

mod crypto;
pub mod modbus;
mod poll;
mod poll_executor;
mod poll_policy;
mod protocol;
mod runtime;
mod state;
mod target;
mod transport;
mod uuids;
mod worker;
mod win_name;

use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex, mpsc};
use std::rc::Rc;

use tokio::sync::mpsc::UnboundedSender;

use crate::services::modbus::{SharedModbusLive, SharedQueryPollLive};

use runtime::TokioRuntime;
use state::{BleInner, LinkPhase, SharedBleState};
use worker::{BleCommand, worker_main};

pub use modbus::{REG_AC_OUTPUT, REG_DC_OUTPUT};
pub use poll_policy::{
    ensure_dashboard_poll_if_idle, PollForeground,
    QueryPollItemSpec,
};
pub use state::{BleScanEntry, BleSnapshot, ScanLinkHint};

/// Worker 线程回调：通过 `slint::invoke_from_event_loop` 在主线程刷新 UI。
pub type UiRefreshHook = Arc<dyn Fn() + Send + Sync>;
pub(crate) type UiRefreshSlot = Arc<Mutex<Option<UiRefreshHook>>>;

struct BleServiceInner {
    state: SharedBleState,
    cmd_tx: UnboundedSender<BleCommand>,
    event_rx: Mutex<mpsc::Receiver<()>>,
    ui_refresh: UiRefreshSlot,
    poll_policy: poll_policy::SharedPollPolicy,
    cancel_connect: Arc<AtomicBool>,
}

#[derive(Clone)]
pub struct BleService {
    inner: Rc<BleServiceInner>,
}

impl BleService {
    pub fn new(
        modbus_live: SharedModbusLive,
        query_live: SharedQueryPollLive,
        query_generation: Arc<AtomicU64>,
    ) -> Self {
        let state: SharedBleState = Arc::new(Mutex::new(BleInner::default()));
        let poll_policy = poll_policy::PollPolicy::new_shared();
        let (cmd_tx, cmd_rx) = tokio::sync::mpsc::unbounded_channel();
        let (event_tx, event_rx) = mpsc::channel();
        let ui_refresh: UiRefreshSlot = Arc::new(Mutex::new(None));
        let cancel_connect = Arc::new(AtomicBool::new(false));

        let runtime = TokioRuntime::new();
        let worker_state = state.clone();
        let worker_ui_refresh = ui_refresh.clone();
        let worker_live = modbus_live.clone();
        let worker_query = query_live.clone();
        let worker_query_gen = query_generation.clone();
        let worker_policy = poll_policy.clone();
        let worker_cancel = cancel_connect.clone();
        runtime.spawn(async move {
            worker_main(
                cmd_rx,
                worker_state,
                event_tx,
                worker_ui_refresh,
                worker_live,
                worker_query,
                worker_query_gen,
                worker_policy,
                worker_cancel,
            )
            .await;
        });

        Self {
            inner: Rc::new(BleServiceInner {
                state,
                cmd_tx,
                event_rx: Mutex::new(event_rx),
                ui_refresh,
                poll_policy,
                cancel_connect,
            }),
        }
    }

    /// 更新前台轮询目标（由 UI 在页面/标签切换时调用）。
    pub fn set_poll_foreground(&self, foreground: poll_policy::PollForeground) {
        let mut policy = self.inner.poll_policy.lock().expect("poll policy lock");
        if policy.foreground != foreground {
            log::info!(
                target: "ble_gui::poll",
                "轮询策略: {} → {}",
                poll_policy::describe_poll_foreground(&policy.foreground),
                poll_policy::describe_poll_foreground(&foreground),
            );
            policy.foreground = foreground;
        }
    }

    /// 记录 UI 当前页面（worker 连接就绪时用于默认轮询目标）。
    pub fn set_ui_page(&self, page: i32) {
        self.inner.poll_policy.lock().expect("poll policy lock").ui_page = page;
    }

    pub fn ui_page(&self) -> i32 {
        self.inner.poll_policy.lock().expect("poll policy lock").ui_page
    }

    pub fn shared_poll_policy(&self) -> poll_policy::SharedPollPolicy {
        self.inner.poll_policy.clone()
    }

    /// 注册 UI 刷新钩子（须在 `MainWindow` 创建后调用）。
    pub fn set_ui_refresh_hook(&self, hook: UiRefreshHook) {
        *self.inner.ui_refresh.lock().expect("ui refresh lock") = Some(hook);
    }

    pub fn snapshot(&self) -> BleSnapshot {
        self.inner
            .state
            .lock()
            .expect("ble state lock")
            .snapshot()
    }

    pub fn shared_state(&self) -> SharedBleState {
        self.inner.state.clone()
    }

    /// 拉取 worker 事件；若返回 true 表示 UI 应刷新。
    pub fn drain_events(&self) -> bool {
        let rx = self.inner.event_rx.lock().expect("event rx lock");
        let mut changed = false;
        while rx.try_recv().is_ok() {
            changed = true;
        }
        changed
    }

    pub fn start_scan(&self) {
        if self.is_scanning() {
            return;
        }
        let _ = self.inner.cmd_tx.send(BleCommand::StartScan);
    }

    pub fn stop_scan(&self) {
        let _ = self.inner.cmd_tx.send(BleCommand::StopScan);
    }

    pub fn connect(&self, address: &str) -> bool {
        self.inner.cancel_connect.store(false, Ordering::Release);
        self.inner
            .cmd_tx
            .send(BleCommand::Connect {
                address: address.to_string(),
            })
            .is_ok()
    }

    /// 取消进行中的连接/定向查找（不走命令队列，立即生效）。
    pub fn cancel_connect(&self) {
        self.inner.cancel_connect.store(true, Ordering::Release);
        if let Ok(mut inner) = self.inner.state.lock() {
            if inner.phase == LinkPhase::Connecting {
                inner.phase = LinkPhase::Idle;
                inner.status_detail = "已取消连接".into();
            }
        }
        if let Ok(guard) = self.inner.ui_refresh.lock() {
            if let Some(hook) = guard.as_ref() {
                hook();
            }
        }
    }

    pub fn disconnect(&self) {
        let _ = self.inner.cmd_tx.send(BleCommand::Disconnect);
    }

    pub fn write_register(&self, address: u16, value: u16) {
        let _ = self.inner.cmd_tx.send(BleCommand::WriteRegister { address, value });
    }

    pub fn is_connected(&self) -> bool {
        matches!(
            self.inner.state.lock().expect("ble state lock").phase,
            LinkPhase::GattReady | LinkPhase::Handshake | LinkPhase::Encrypted
        )
    }

    pub fn is_scanning(&self) -> bool {
        self.inner.state.lock().expect("ble state lock").phase == LinkPhase::Scanning
    }

    pub fn is_connecting(&self) -> bool {
        self.inner.state.lock().expect("ble state lock").phase == LinkPhase::Connecting
    }
}
