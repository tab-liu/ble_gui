//! Tokio 运行时：在独立线程中驱动 BLE 异步任务。

use std::future::Future;
use std::thread::{self, JoinHandle};

pub struct TokioRuntime {
    pub handle: tokio::runtime::Handle,
    _thread: JoinHandle<()>,
}

impl TokioRuntime {
    pub fn new() -> Self {
        let runtime = tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .thread_name("ble-runtime")
            .build()
            .expect("create tokio runtime");

        let handle = runtime.handle().clone();
        let _thread = thread::spawn(move || {
            runtime.block_on(async {
                std::future::pending::<()>().await;
            });
        });

        Self { handle, _thread }
    }

    pub fn spawn<F>(&self, fut: F)
    where
        F: Future + Send + 'static,
        F::Output: Send,
    {
        self.handle.spawn(fut);
    }
}
