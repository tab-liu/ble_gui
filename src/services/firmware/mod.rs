//! 设备固件升级：本地选文件、自动识别头、MD5；传输由 BLE worker 走 OTA Start + XMODEM-1K。
//!
//! - `> 1MB`：视为 IOT 整包，不解析 POWEROAK 头，类型固定为 [`header`] 中的 IOT=0
//! - 否则先按 8 位头、再按 TI 16 位 Word 头识别 `POWEROAK`
//!
//! 进度：IOT 自升级只有蓝牙阶段（0–100）；子设备固件蓝牙占 0–50，IOT→CAN 占 50–100。
//! 当前阶段写在 `stage_text`。

use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use md5::{Digest, Md5};

pub mod header;

use header::{classify, part_number_wire, FirmwareInfo, FIRMWARE_MAX_BYTES};

pub const IOT_DEFAULT_OTA_VERSION: u32 = 100600199;
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
    /// IOT 自身升级只有蓝牙阶段，总进度不按 50/50 折算。
    pub ble_only: bool,
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
            ble_only: false,
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
    pub device_type: String,
    pub device_sn: String,
    pub status_text: String,
    pub file_name: String,
    pub file_size_text: String,
    pub md5: String,
    pub type_text: String,
    pub image_version: String,
    pub layout_text: String,
    pub parse_source: String,
    pub dev_model: String,
    pub ota_version_text: String,
    pub has_file: bool,
    pub phase: i32,
    pub progress: i32,
    pub stage_text: String,
    pub result_text: String,
    pub fail_reason: String,
    pub can_start: bool,
    pub can_stop: bool,
    pub part_mismatch: bool,
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
    device_type: String,
    device_sn: String,
    device_iot_version: Option<u32>,
    device_software: Vec<(u16, u32)>,
    selected: Option<SelectedFile>,
    parse_error: Option<String>,
    ota_version_text: String,
    /// 自动填入的料号可被后续设备信息覆盖；用户手改后不再覆盖。
    ota_version_auto: bool,
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
                device_type: "—".into(),
                device_sn: "—".into(),
                device_iot_version: None,
                device_software: Vec::new(),
                selected: None,
                parse_error: None,
                ota_version_text: String::new(),
                ota_version_auto: true,
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
        let version_ok = parse_ota_version_text(&inner.ota_version_text).is_ok();
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
        let ble_only = ota.as_ref().map(|g| g.ble_only).unwrap_or(false);
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

        let mismatch = inner.selected.as_ref().and_then(|sel| {
            part_mismatch_message(sel.info.type_code, sel.info.version, &inner.device_software)
        });
        let status_text = if let Some(err) = &inner.parse_error {
            err.clone()
        } else if running || ota_phase == PHASE_SUCCESS || ota_phase == PHASE_FAILED {
            ota.as_ref()
                .map(|g| g.status_text.clone())
                .unwrap_or_default()
        } else if let Some(msg) = &mismatch {
            msg.clone()
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
            device_type: inner.device_type.clone(),
            device_sn: inner.device_sn.clone(),
            status_text,
            file_name,
            file_size_text,
            md5,
            type_text,
            image_version,
            layout_text,
            parse_source,
            dev_model,
            ota_version_text: inner.ota_version_text.clone(),
            has_file: parse_ok,
            phase,
            progress: overall_progress(pc, device, ble_only),
            stage_text,
            result_text,
            fail_reason,
            can_start: parse_ok && version_ok && device_connected && !running,
            can_stop: running,
            part_mismatch: mismatch.is_some()
                && inner.parse_error.is_none()
                && !running
                && ota_phase != PHASE_SUCCESS
                && ota_phase != PHASE_FAILED,
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
        inner.ota_version_text.clear();
        inner.ota_version_auto = true;
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
            if parse_ota_version_text(&inner.ota_version_text).is_err() {
                inner.phase = PHASE_FAILED;
                inner.result_text = "升级失败".into();
                inner.fail_reason = "请填写料号版本号（IOT 整包读不到，需手填设备当前软件版本）".into();
                inner.status_text = inner.fail_reason.clone();
                return None;
            }
        }
        let job = {
            let inner = self.inner.borrow();
            let sel = inner.selected.as_ref()?;
            let version = parse_ota_version_text(&inner.ota_version_text).ok()?;
            OtaJob {
                firmware: sel.bytes.clone(),
                firmware_type: sel.info.type_code,
                version,
            }
        };
        if let Ok(mut g) = self.ota.lock() {
            *g = OtaLive {
                running: true,
                cancel: false,
                phase: PHASE_RUNNING,
                pc_percent: 0,
                device_percent: 0,
                ble_only: job.firmware_type == 0,
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

    pub fn set_ota_version_text(&self, text: String) {
        if self.is_running() {
            return;
        }
        let mut inner = self.inner.borrow_mut();
        inner.ota_version_auto = false;
        inner.ota_version_text = text;
    }

    /// 连接后 1100 / 11000 段读到的机型、SN、软件版本。
    /// IOT 文件在用户未手改料号时，用设备当前版本自动填 xx99。
    pub fn apply_device_info(
        &self,
        summary: String,
        iot_version: Option<u32>,
        device_type: String,
        device_sn: String,
        device_software: Vec<(u16, u32)>,
    ) {
        let running = self.is_running();
        let mut inner = self.inner.borrow_mut();
        inner.device_version = if summary.is_empty() {
            "—".into()
        } else {
            summary
        };
        inner.device_type = if device_type.is_empty() {
            "—".into()
        } else {
            device_type
        };
        inner.device_sn = if device_sn.is_empty() {
            "—".into()
        } else {
            device_sn
        };
        inner.device_iot_version = iot_version;
        inner.device_software = device_software;
        if running {
            return;
        }
        let iot_file = inner.selected.as_ref().is_some_and(|s| {
            s.info.layout == header::HeaderLayout::IotRaw || s.info.version == 0
        });
        if iot_file && inner.ota_version_auto {
            if let Some(v) = iot_version {
                inner.ota_version_text = part_number_wire(v).to_string();
            }
        }
    }

    fn load_path(&self, path: &Path) {
        self.reset_ota_outcome();
        match inspect_file(path) {
            Ok(selected) => {
                let mut inner = self.inner.borrow_mut();
                inner.ota_version_auto = true;
                inner.ota_version_text = auto_ota_version_text(&selected.info, inner.device_iot_version);
                inner.selected = Some(selected);
                inner.parse_error = None;
                inner.phase = PHASE_READY;
                inner.stage_text = "已验证，等待升级".into();
                inner.result_text = "待升级".into();
                inner.fail_reason.clear();
                inner.status_text.clear();
            }
            Err(err) => {
                let mut inner = self.inner.borrow_mut();
                inner.selected = None;
                inner.ota_version_text.clear();
                inner.ota_version_auto = true;
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

fn auto_ota_version_text(info: &FirmwareInfo, device_iot: Option<u32>) -> String {
    if info.version != 0 {
        part_number_wire(info.version).to_string()
    } else if let Some(v) = device_iot {
        part_number_wire(v).to_string()
    } else {
        IOT_DEFAULT_OTA_VERSION.to_string()
    }
}

/// ARM / DSP / BMS：设备上报了同类（含 BOOT 对 BOOT）版本时，按 version/100 核对产品线。
fn part_mismatch_message(
    file_type: u8,
    file_version: u32,
    device_software: &[(u16, u32)],
) -> Option<String> {
    if !matches!(file_type, 1 | 2 | 3) {
        return None;
    }
    let file_boot = header::software_is_boot(u16::from(file_type), file_version);
    let peers: Vec<u32> = device_software
        .iter()
        .filter(|(type_code, version)| {
            header::software_base_type(*type_code) == file_type
                && header::software_is_boot(*type_code, *version) == file_boot
        })
        .map(|(_, version)| *version)
        .collect();
    if peers.is_empty() {
        return None;
    }
    let file_family = file_version / 100;
    if peers.iter().any(|version| *version / 100 == file_family) {
        return None;
    }
    let name = header::format_software_name(u16::from(file_type), file_version);
    let device_list = peers
        .iter()
        .map(ToString::to_string)
        .collect::<Vec<_>>()
        .join("、");
    Some(format!(
        "料号不匹配：固件 {name} {file_version} 与设备当前 {device_list} 不是同一产品线，请确认后再升级"
    ))
}

fn parse_ota_version_text(text: &str) -> Result<u32, String> {
    let trimmed = text.trim().replace([' ', '_', ',', '-'], "");
    if trimmed.is_empty() {
        return Err("版本号为空".into());
    }
    let value = trimmed
        .parse::<u32>()
        .map_err(|_| "版本号须为数字，例如 100620109".to_string())?;
    if value == 0 {
        return Err("版本号不能为 0".into());
    }
    Ok(value)
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

/// IOT 自升级进度等于蓝牙进度；子设备则蓝牙与 CAN 各占一半。
fn overall_progress(pc_percent: i32, device_percent: i32, ble_only: bool) -> i32 {
    if ble_only {
        pc_percent.clamp(0, 100)
    } else {
        (pc_percent.clamp(0, 100) + device_percent.clamp(0, 100)) / 2
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_version_rejects_empty_and_zero() {
        assert!(parse_ota_version_text("").is_err());
        assert!(parse_ota_version_text("0").is_err());
        assert_eq!(parse_ota_version_text("100620109").unwrap(), 100620109);
        assert_eq!(parse_ota_version_text(" 100_620_109 ").unwrap(), 100620109);
        assert_eq!(overall_progress(40, 0, true), 40);
        assert_eq!(overall_progress(40, 0, false), 20);
        assert_eq!(overall_progress(100, 0, true), 100);
        assert_eq!(overall_progress(100, 100, false), 100);
    }

    #[test]
    fn auto_fill_uses_last_two_digits_99() {
        let mut info = FirmwareInfo {
            layout: header::HeaderLayout::Packed8,
            type_code: 1,
            version: 100650103,
            image_size: 1,
            crc32: 0,
            dev_model: String::new(),
            esp_version: String::new(),
            parse_source: String::new(),
        };
        assert_eq!(auto_ota_version_text(&info, None), "100650199");
        info.version = 0;
        info.layout = header::HeaderLayout::IotRaw;
        assert_eq!(auto_ota_version_text(&info, Some(100600108)), "100600199");
        assert_eq!(auto_ota_version_text(&info, None), IOT_DEFAULT_OTA_VERSION.to_string());
    }

    #[test]
    fn arm_dsp_bms_mismatch_only_when_device_has_same_kind() {
        assert!(part_mismatch_message(1, 100650103, &[]).is_none());
        assert!(part_mismatch_message(1, 100650103, &[(1, 100650108)]).is_none());
        let msg = part_mismatch_message(1, 100650103, &[(1, 200650108)]).unwrap();
        assert!(msg.contains("料号不匹配"));
        assert!(msg.contains("200650108"));
        // BOOT 只和 BOOT 比，设备只有 APP 时不提示
        assert!(part_mismatch_message(1, 100650100, &[(1, 100650103)]).is_none());
        let boot_msg = part_mismatch_message(1, 100650100, &[(1, 200650100)]).unwrap();
        assert!(boot_msg.contains("ARM-BOOT"));
        assert!(part_mismatch_message(3, 8026103, &[(3, 8026108)]).is_none());
        assert!(part_mismatch_message(3, 8026103, &[(3, 9026108)]).is_some());
        assert!(part_mismatch_message(0, 100600108, &[(0, 200600108)]).is_none());
    }
}
