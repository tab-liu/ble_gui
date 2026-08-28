//! 蓝牙连接服务：同步 UI 接口 + Tokio/btleplug 后台 worker。

mod protocol;
mod runtime;
mod state;
mod target;
mod uuids;
mod worker;
mod win_name;

use std::sync::{Arc, Mutex, mpsc};
use std::rc::Rc;

use tokio::sync::mpsc::UnboundedSender;

use runtime::TokioRuntime;
use state::{BleInner, LinkPhase, SharedBleState};
use worker::{BleCommand, worker_main};

pub use state::{BleScanEntry, BleSnapshot};

/// Worker 线程回调：通过 `slint::invoke_from_event_loop` 在主线程刷新 UI。
pub type UiRefreshHook = Arc<dyn Fn() + Send + Sync>;
pub(crate) type UiRefreshSlot = Arc<Mutex<Option<UiRefreshHook>>>;

struct BleServiceInner {
    state: SharedBleState,
    cmd_tx: UnboundedSender<BleCommand>,
    event_rx: Mutex<mpsc::Receiver<()>>,
    ui_refresh: UiRefreshSlot,
}

#[derive(Clone)]
pub struct BleService {
    inner: Rc<BleServiceInner>,
}

impl BleService {
    pub fn new() -> Self {
        let state: SharedBleState = Arc::new(Mutex::new(BleInner::default()));
        let (cmd_tx, cmd_rx) = tokio::sync::mpsc::unbounded_channel();
        let (event_tx, event_rx) = mpsc::channel();
        let ui_refresh: UiRefreshSlot = Arc::new(Mutex::new(None));

        let runtime = TokioRuntime::new();
        let worker_state = state.clone();
        let worker_ui_refresh = ui_refresh.clone();
        runtime.spawn(async move {
            worker_main(cmd_rx, worker_state, event_tx, worker_ui_refresh).await;
        });

        Self {
            inner: Rc::new(BleServiceInner {
                state,
                cmd_tx,
                event_rx: Mutex::new(event_rx),
                ui_refresh,
            }),
        }
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
        self.inner
            .cmd_tx
            .send(BleCommand::Connect {
                address: address.to_string(),
            })
            .is_ok()
    }

    pub fn disconnect(&self) {
        let _ = self.inner.cmd_tx.send(BleCommand::Disconnect);
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
