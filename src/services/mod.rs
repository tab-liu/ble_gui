//! 服务层：与 UI 解耦的业务能力。
//!
//! | 子模块 | 说明 |
//! |--------|------|
//! | [`ble`] | 蓝牙扫描/连接/加密/Modbus 轮询 worker |
//! | [`modbus`] | UI 可读的仪表板与查询轮询共享快照 |
//! | [`poll_sync`] | 根据当前页面把轮询策略推给 worker |
//! | [`modbus_query_store`] | Modbus 查询页 TOML 持久化 |
//! | [`device_config_store`] | 设备配置自定义分组 TOML 持久化 |
//! | [`ble_favorites`] | 收藏设备列表 |
//! | [`theme`] | 明暗主题 |
//! | [`firmware`] | 固件升级：选文件、自动识别头、MD5（传输后续接入） |

pub mod ble;
pub mod ble_favorites;
pub mod device_config_store;
pub mod firmware;
pub mod modbus;
pub mod modbus_query_store;
pub mod poll_sync;
pub mod theme;
