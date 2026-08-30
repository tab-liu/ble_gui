//! 设备固件升级服务。
//!
//! 当前为占位实现（检查更新仅更新文案）。后续 BLE OTA 应对齐 `ref/tool`
//!（固件头解析 + Modbus OTA Start + XMODEM-1K），类型枚举以 `ref/dev` 为准
//!（`DEVICE_IOT = 0` 等，勿使用 Studio 显示表里错误的「IOT=4」）。

use std::cell::RefCell;
use std::rc::Rc;

#[derive(Clone, Debug)]
pub struct FirmwareSnapshot {
    pub current_version: String,
    pub status_text: String,
}

#[derive(Clone)]
pub struct FirmwareService {
    inner: Rc<RefCell<FirmwareInner>>,
}

struct FirmwareInner {
    current_version: String,
    last_status: String,
}

impl FirmwareService {
    pub fn new() -> Self {
        Self {
            inner: Rc::new(RefCell::new(FirmwareInner {
                current_version: "v2.1.0".into(),
                last_status: "等待检查更新".into(),
            })),
        }
    }

    pub fn snapshot(&self) -> FirmwareSnapshot {
        let inner = self.inner.borrow();
        FirmwareSnapshot {
            current_version: inner.current_version.clone(),
            status_text: inner.last_status.clone(),
        }
    }

    pub fn check_update(&self) {
        self.inner.borrow_mut().last_status = "已是最新固件（模拟）".into();
    }
}
