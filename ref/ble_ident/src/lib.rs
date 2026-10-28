//! 扫描加密蓝牙设备，读出整机 / IoT / 安全码，并把安全码封成密文。
//!
//! 私钥不在本库里。`ident` 只调用 [`seal`]；`decode` 自带私钥再调用 [`open`]。

pub mod ble;
mod identity;
mod link;
mod seal;

pub use identity::{format_identity, IotFields};
pub use link::{spawn_link, FoundDevice, LinkEvent, LinkHandle};
pub use seal::{open, seal};
