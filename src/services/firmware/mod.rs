//! 设备固件升级：本地选文件、自动识别头、MD5；传输由 BLE worker 走 OTA Start + XMODEM-1K。
//!
//! - `> 1MB`：视为 IOT 整包，不解析 POWEROAK 头，类型固定为 [`header`] 中的 IOT=0
//! - 否则先按 8 位头、再按 TI 16 位 Word 头识别 `POWEROAK`
//!
//! 总进度一条 0–100%：蓝牙 XMODEM 占 0–50，设备内部 CAN 占 50–100。
//! 当前阶段写在 `stage_text`。

use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use md5::{Digest, Md5};

pub mod header;

use header::{classify, FirmwareInfo, FIRMWARE_MAX_BYTES};

pub const PHASE_IDLE: i32 = 0;
pub const PHASE_READY: i32 = 1;
pub const PHASE_RUNNING: i32 = 2;
pub const PHASE_SUCCESS: i32 = 3;
pub const PHASE_FAILED: i32 = 4;

/// Worker 与 UI 共享的传输进度。
#[derive(Clone, Debug)]
pub struct OtaLive {
    pub running: bool,
    pub cancel: bool,
    pub phase: i32,
    pub pc_percent: i32,
    pub device_percent: i32,
    pub stage_text: String,
    pub result_text: String,
    pub fail_reason: String,
    pub status_text: String,
}

impl Default for OtaLive {
    fn default() -> Self {
        Self {
            running: false,
            cancel: false,
            phase: PHASE_IDLE,
            pc_percent: 0,
            device_percent: 0,
            stage_text: String::new(),
            result_text: String::new(),
            fail_reason: String::new(),
            status_text: String::new(),
        }
    }
}

pub type SharedOtaLive = Arc<Mutex<OtaLive>>;

pub fn new_shared_ota() -> SharedOtaLive {
    Arc::new(Mutex::new(OtaLive::default()))
}

#[derive(Clone)]
pub struct OtaJob {
    pub firmware: Arc<[u8]>,
    pub firmware_type: u8,
    pub version: u32,
}

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
    bytes: Arc<[u8]>,
}

struct FirmwareInner {
    device_version: String,
    selected: Option<SelectedFile>,
    parse_error: Option<String>,
    phase: i32,
    stage_text: String,
    result_text: String,
    fail_reason: String,
    status_text: String,
}

#[derive(Clone)]
pub struct FirmwareService {
    inner: Rc<RefCell<FirmwareInner>>,
    ota: SharedOtaLive,
}

impl FirmwareService {
    pub fn new(ota: SharedOtaLive) -> Self {
        Self {
            inner: Rc::new(RefCell::new(FirmwareInner {
                device_version: "—".into(),
                selected: None,
                parse_error: None,
                phase: PHASE_IDLE,
                stage_text: "等待选择固件".into(),
                result_text: "—".into(),
                fail_reason: String::new(),
                status_text: String::new(),
            })),
            ota,
        }
    }

    pub fn is_running(&self) -> bool {
        self.ota.lock().map(|g| g.running).unwrap_or(false)
    }

    fn reset_ota_outcome(&self) {
        if let Ok(mut g) = self.ota.lock() {
            if g.running {
                return;
            }
            *g = OtaLive::default();
        }
    }

    pub fn snapshot(&self, device_connected: bool) -> FirmwareSnapshot {
        let inner = self.inner.borrow();
        let ota = self.ota.lock().ok();
        let parse_ok = inner.selected.is_some();
        let running = ota.as_ref().is_some_and(|g| g.running);
        let ota_phase = ota.as_ref().map(|g| g.phase).unwrap_or(PHASE_IDLE);
        let phase = if running {
            PHASE_RUNNING
        } else if ota_phase == PHASE_SUCCESS || ota_phase == PHASE_FAILED {
            ota_phase
        } else {
            inner.phase
        };
        let pc = ota.as_ref().map(|g| g.pc_percent).unwrap_or(0);
        let device = ota.as_ref().map(|g| g.device_percent).unwrap_or(0);
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
        } else if running || ota_phase == PHASE_SUCCESS || ota_phase == PHASE_FAILED {
            ota.as_ref()
                .map(|g| g.status_text.clone())
                .unwrap_or_default()
        } else {
            inner.status_text.clone()
        };

        let stage_text = if running || ota_phase == PHASE_SUCCESS || ota_phase == PHASE_FAILED {
            ota.as_ref()
                .map(|g| g.stage_text.clone())
                .unwrap_or_else(|| inner.stage_text.clone())
        } else {
            inner.stage_text.clone()
        };

        let (result_text, fail_reason) = if ota_phase == PHASE_SUCCESS || ota_phase == PHASE_FAILED {
            (
                ota.as_ref()
                    .map(|g| g.result_text.clone())
                    .unwrap_or_else(|| inner.result_text.clone()),
                ota.as_ref()
                    .map(|g| g.fail_reason.clone())
                    .unwrap_or_else(|| inner.fail_reason.clone()),
            )
        } else {
            (inner.result_text.clone(), inner.fail_reason.clone())
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
            phase,
            progress: overall_progress(pc, device),
            stage_text,
            result_text,
            fail_reason,
            can_start: parse_ok && device_connected && !running,
            can_stop: running,
        }
    }

    pub fn pick_file(&self) -> bool {
        if self.is_running() {
            return false;
        }
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
        if self.is_running() {
            return;
        }
        self.reset_ota_outcome();
        let mut inner = self.inner.borrow_mut();
        inner.selected = None;
        inner.parse_error = None;
        inner.phase = PHASE_IDLE;
        inner.stage_text = "等待选择固件".into();
        inner.result_text = "—".into();
        inner.fail_reason.clear();
        inner.status_text.clear();
    }

    /// 校验后进入传输；返回任务给 BLE worker。失败时已写入界面原因。
    pub fn begin_upgrade(&self, device_connected: bool, encryption_ready: bool) -> Option<OtaJob> {
        if self.is_running() {
            return None;
        }
        {
            let mut inner = self.inner.borrow_mut();
            if inner.selected.is_none() {
                inner.phase = PHASE_FAILED;
                inner.result_text = "升级失败".into();
                inner.fail_reason = "未选择有效固件".into();
                inner.status_text = inner.fail_reason.clone();
                return None;
            }
            if !device_connected {
                inner.phase = PHASE_FAILED;
                inner.result_text = "升级失败".into();
                inner.fail_reason = "未连接设备，无法开始升级".into();
                inner.status_text = inner.fail_reason.clone();
                return None;
            }
            if !encryption_ready {
                inner.phase = PHASE_FAILED;
                inner.result_text = "升级失败".into();
                inner.fail_reason = "加密通道尚未完成，不能启动升级".into();
                inner.status_text = inner.fail_reason.clone();
                return None;
            }
        }
        let job = {
            let inner = self.inner.borrow();
            let sel = inner.selected.as_ref()?;
            OtaJob {
                firmware: sel.bytes.clone(),
                firmware_type: sel.info.type_code,
                version: sel.info.version,
            }
        };
        if let Ok(mut g) = self.ota.lock() {
            *g = OtaLive {
                running: true,
                cancel: false,
                phase: PHASE_RUNNING,
                pc_percent: 0,
                device_percent: 0,
                stage_text: "准备升级".into(),
                result_text: String::new(),
                fail_reason: String::new(),
                status_text: String::new(),
            };
        }
        Some(job)
    }

    pub fn request_stop(&self) {
        if let Ok(mut g) = self.ota.lock() {
            if !g.running {
                return;
            }
            g.cancel = true;
            g.stage_text = "正在停止…".into();
        }
    }

    fn load_path(&self, path: &Path) {
        self.reset_ota_outcome();
        match inspect_file(path) {
            Ok(selected) => {
                let mut inner = self.inner.borrow_mut();
                let status = format!("已识别 {}", selected.info.type_name());
                inner.selected = Some(selected);
                inner.parse_error = None;
                inner.phase = PHASE_READY;
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
        bytes: Arc::from(bytes.into_boxed_slice()),
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
