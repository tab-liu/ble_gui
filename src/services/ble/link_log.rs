//! 一次连接的时间线：从点击连接到 TLV（或常规读）第一次查询结束。
//!
//! 目标 `ble_gui::link`。默认日志级别会放行这一路的 info，其余模块仍是 warn。
//! 查询确认之后不再输出，避免周期轮询把时间线冲掉。

use std::fs::OpenOptions;
use std::io::Write;
use std::sync::Mutex;
use std::time::Instant;

use log::info;

struct Trace {
    t0: Instant,
    open: bool,
}

static TRACE: Mutex<Option<Trace>> = Mutex::new(None);

pub(crate) fn link_begin(detail: &str) {
    if let Ok(mut guard) = TRACE.lock() {
        *guard = Some(Trace {
            t0: Instant::now(),
            open: true,
        });
    }
    emit(&format!("+0ms {detail}"));
}

pub(crate) fn link_mark(detail: &str) {
    let Some(ms) = elapsed_if_open() else {
        return;
    };
    emit(&format!("+{ms}ms {detail}"));
}

pub(crate) fn link_done(detail: &str) {
    let ms = elapsed_if_open();
    if let Ok(mut guard) = TRACE.lock() {
        if let Some(trace) = guard.as_mut() {
            trace.open = false;
        }
    }
    if let Some(ms) = ms {
        emit(&format!("+{ms}ms {detail}"));
    }
}

fn elapsed_if_open() -> Option<u128> {
    let guard = TRACE.lock().ok()?;
    let trace = guard.as_ref()?;
    trace.open.then(|| trace.t0.elapsed().as_millis())
}

fn emit(line: &str) {
    info!(target: "ble_gui::link", "{line}");
    if let Ok(mut file) = OpenOptions::new()
        .create(true)
        .append(true)
        .open("ble_connect.log")
    {
        let ts = chrono::Local::now().format("%Y-%m-%d %H:%M:%S%.3f");
        let _ = writeln!(file, "[{ts}] {line}");
    }
}
