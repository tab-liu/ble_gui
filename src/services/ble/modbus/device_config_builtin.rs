//! 设备配置「常用」页：固定寄存器/位域映射（对齐主页 dashboard 风格）。

/// 控件类型（与 Slint BuiltinConfigItem.widget-kind 对齐）
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum BuiltinWidget {
    Switch = 0,
    Enum = 1,
    Number = 2,
}

/// 单个常用设置的静态定义（schema 不落盘）。
#[derive(Debug, Clone, Copy)]
pub struct BuiltinSettingDef {
    pub id: &'static str,
    pub name: &'static str,
    pub register: u16,
    /// 若为 Some，表示该设置只占寄存器中的一位；写入需 RMW。
    pub bit: Option<u8>,
    pub widget: BuiltinWidget,
}

pub const DEFAULT_SLAVE_ID: u8 = 0;

/// 常用设置映射表；有产品寄存器表后再往里加，schema 不落盘。
pub const BUILTIN_SETTINGS: &[BuiltinSettingDef] = &[];
