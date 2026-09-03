//! 设备固件升级：本地选文件、自动识别头、MD5，传输（OTA Start + XMODEM）后续接入。
//!
//! - `> 1MB`：视为 IOT 整包，不解析 POWEROAK 头，类型固定为 [`header`] 中的 IOT=0
//! - 否则先按 8 位头、再按 TI 16 位 Word 头识别 `POWEROAK`
//!
//! 总进度一条 0–100%：蓝牙 XMODEM 占 0–50，设备内部 CAN 占 50–100。
//! 当前阶段写在 `stage_text`，不再拆两条百分比。

use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;

use md5::{Digest, Md5};

pub mod header;

use header::{classify, FirmwareInfo, FIRMWARE_MAX_BYTES};

pub const PHASE_IDLE: i32 = 0;
pub const PHASE_READY: i32 = 1;
pub const PHASE_RUNNING: i32 = 2;
#[allow(dead_code)]
pub const PHASE_SUCCESS: i32 = 3;
pub const PHASE_FAILED: i32 = 4;

#[derive(Clone, Debug)]
pub struct FirmwareSnapshot {
    pub device_version: String,
    pub status_text: String,
    pub file_name: String,
    pub file_size_text: String,
    pub md5: String,
    pub type_text: String,
    pub image_version: String,
    pub layout_text: String,
    pub parse_source: String,
    pub dev_model: String,
    pub has_file: bool,
    pub phase: i32,
    pub progress: i32,
    pub stage_text: String,
    pub result_text: String,
    pub fail_reason: String,
    pub can_start: bool,
    pub can_stop: bool,
}

struct SelectedFile {
    #[allow(dead_code)]
    path: PathBuf,
    name: String,
    size: u64,
    md5: String,
    info: FirmwareInfo,
}

struct FirmwareInner {
    device_version: String,
    selected: Option<SelectedFile>,
    parse_error: Option<String>,
    phase: i32,
    pc_percent: i32,
    device_percent: i32,
    stage_text: String,
    result_text: String,
    fail_reason: String,
    status_text: String,
}

#[derive(Clone)]
pub struct FirmwareService {
    inner: Rc<RefCell<FirmwareInner>>,
}

impl FirmwareService {
    pub fn new() -> Self {
        Self {
            inner: Rc::new(RefCell::new(FirmwareInner {
                device_version: "—".into(),
                selected: None,
                parse_error: None,
                phase: PHASE_IDLE,
                pc_percent: 0,
                device_percent: 0,
                stage_text: "等待选择固件".into(),
                result_text: "—".into(),
                fail_reason: String::new(),
                status_text: String::new(),
            })),
        }
    }

    pub fn snapshot(&self, device_connected: bool) -> FirmwareSnapshot {
        let inner = self.inner.borrow();
        let parse_ok = inner.selected.is_some();
        let running = inner.phase == PHASE_RUNNING;
        let (file_name, file_size_text, md5, type_text, image_version, layout_text, parse_source, dev_model) =
            if let Some(sel) = &inner.selected {
                (
                    sel.name.clone(),
                    format_size(sel.size),
                    sel.md5.clone(),
                    format!("{}  ·  {}", sel.info.type_name(), sel.info.type_code),
                    sel.info.version_text(),
                    sel.info.layout.label().to_string(),
                    if sel.info.parse_source.is_empty() {
                        "—".into()
                    } else {
                        sel.info.parse_source.clone()
                    },
                    if sel.info.dev_model.is_empty() {
                        "—".into()
                    } else {
                        sel.info.dev_model.clone()
                    },
                )
            } else {
                (
                    "未选择".into(),
                    "—".into(),
                    "—".into(),
                    "—".into(),
                    "—".into(),
                    "—".into(),
                    "—".into(),
                    "—".into(),
                )
            };

        let status_text = if let Some(err) = &inner.parse_error {
            err.clone()
        } else {
            inner.status_text.clone()
        };

        FirmwareSnapshot {
            device_version: inner.device_version.clone(),
            status_text,
            file_name,
            file_size_text,
            md5,
            type_text,
            image_version,
            layout_text,
            parse_source,
            dev_model,
            has_file: parse_ok,
            phase: inner.phase,
            progress: overall_progress(inner.pc_percent, inner.device_percent),
            stage_text: inner.stage_text.clone(),
            result_text: inner.result_text.clone(),
            fail_reason: inner.fail_reason.clone(),
            can_start: parse_ok && device_connected && !running,
            can_stop: running,
        }
    }

    pub fn pick_file(&self) -> bool {
        let picked = rfd::FileDialog::new()
            .add_filter("固件", &["bin", "img", "fw"])
            .add_filter("所有文件", &["*"])
            .set_title("选择升级固件")
            .pick_file();
        let Some(path) = picked else {
            return false;
        };
        self.load_path(&path);
        true
    }

    pub fn clear_file(&self) {
        let mut inner = self.inner.borrow_mut();
        if inner.phase == PHASE_RUNNING {
            return;
        }
        inner.selected = None;
        inner.parse_error = None;
        inner.phase = PHASE_IDLE;
        inner.pc_percent = 0;
        inner.device_percent = 0;
        inner.stage_text = "等待选择固件".into();
        inner.result_text = "—".into();
        inner.fail_reason.clear();
        inner.status_text.clear();
    }

    pub fn start_upgrade(&self, device_connected: bool) {
        let mut inner = self.inner.borrow_mut();
        if inner.phase == PHASE_RUNNING {
            return;
        }
        if inner.selected.is_none() {
            inner.phase = PHASE_FAILED;
            inner.result_text = "升级失败".into();
            inner.fail_reason = "未选择有效固件".into();
            inner.status_text = inner.fail_reason.clone();
            return;
        }
        if !device_connected {
            inner.phase = PHASE_FAILED;
            inner.result_text = "升级失败".into();
            inner.fail_reason = "未连接设备，无法开始升级".into();
            inner.status_text = inner.fail_reason.clone();
            return;
        }
        inner.phase = PHASE_READY;
        inner.pc_percent = 0;
        inner.device_percent = 0;
        inner.result_text = "待升级".into();
        inner.fail_reason.clear();
        inner.stage_text = "等待开始".into();
        inner.status_text.clear();
    }

    pub fn stop_upgrade(&self) {
        let mut inner = self.inner.borrow_mut();
        if inner.phase != PHASE_RUNNING {
            return;
        }
        inner.phase = PHASE_FAILED;
        inner.result_text = "已停止".into();
        inner.fail_reason = "用户终止".into();
        inner.stage_text = "已停止".into();
        inner.status_text = "升级已停止".into();
    }

    fn load_path(&self, path: &Path) {
        match inspect_file(path) {
            Ok(selected) => {
                let mut inner = self.inner.borrow_mut();
                let status = format!("已识别 {}", selected.info.type_name());
                inner.selected = Some(selected);
                inner.parse_error = None;
                inner.phase = PHASE_READY;
                inner.pc_percent = 0;
                inner.device_percent = 0;
                inner.stage_text = "已验证，等待升级".into();
                inner.result_text = "待升级".into();
                inner.fail_reason.clear();
                inner.status_text = status;
            }
            Err(err) => {
                let mut inner = self.inner.borrow_mut();
                inner.selected = None;
                inner.parse_error = Some(err.clone());
                inner.phase = PHASE_FAILED;
                inner.pc_percent = 0;
                inner.device_percent = 0;
                inner.stage_text = "识别失败".into();
                inner.result_text = "无法使用".into();
                inner.fail_reason = err.clone();
                inner.status_text = err;
            }
        }
    }
}

fn inspect_file(path: &Path) -> Result<SelectedFile, String> {
    let meta = std::fs::metadata(path).map_err(|e| format!("无法读取文件：{e}"))?;
    let size = meta.len();
    if size > FIRMWARE_MAX_BYTES {
        return Err(format!(
            "文件过大（{}），上限 {} MB",
            format_size(size),
            FIRMWARE_MAX_BYTES / (1024 * 1024)
        ));
    }
    let bytes = std::fs::read(path).map_err(|e| format!("无法读取文件：{e}"))?;
    let info = classify(size, &bytes)?;
    let md5 = format!("{:x}", Md5::digest(&bytes));
    let name = path
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_else(|| path.display().to_string());
    Ok(SelectedFile {
        path: path.to_path_buf(),
        name,
        size,
        md5,
        info,
    })
}

fn format_size(bytes: u64) -> String {
    const KB: u64 = 1024;
    const MB: u64 = 1024 * 1024;
    if bytes >= MB {
        format!("{:.2} MB  （{bytes} 字节）", bytes as f64 / MB as f64)
    } else if bytes >= KB {
        format!("{:.1} KB  （{bytes} 字节）", bytes as f64 / KB as f64)
    } else {
        format!("{bytes} 字节")
    }
}

/// 蓝牙 XMODEM 与设备 CAN 各占一半。
fn overall_progress(pc_percent: i32, device_percent: i32) -> i32 {
    (pc_percent.clamp(0, 100) + device_percent.clamp(0, 100)) / 2
}
