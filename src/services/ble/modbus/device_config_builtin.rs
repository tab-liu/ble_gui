//! 设备配置「常用」页：固定寄存器/位域映射（对齐主页 dashboard 风格）。

use super::query::QueryValueType;

/// 控件类型（与 Slint BuiltinConfigItem.widget-kind 对齐）
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum BuiltinWidget {
    Switch = 0,
    Enum = 1,
    Number = 2,
}

/// 寄存器内多位域写入（RMW）：只改 `start_bit` 起 `width` 位，其它位保持不变。
#[derive(Debug, Clone, Copy)]
pub struct RegisterFieldPatch {
    pub start_bit: u8,
    pub width: u8,
    pub value: u16,
}

/// 单个常用设置的静态定义（schema 不落盘）。
#[derive(Debug, Clone, Copy)]
pub struct BuiltinSettingDef {
    pub id: &'static str,
    pub name: &'static str,
    pub register: u16,
    /// 若为 Some，表示该设置只占寄存器中的一位；写入需 RMW。
    pub bit: Option<u8>,
    /// 若为 Some，表示多位域写入；与 `bit` 互斥，写入需 RMW。
    pub field: Option<RegisterFieldPatch>,
    pub widget: BuiltinWidget,
    pub value_type: QueryValueType,
    pub register_count: u16,
    /// Enum 控件选项：(显示名, 写入值)
    pub enum_options: &'static [(&'static str, &'static str)],
}

pub const DEFAULT_SLAVE_ID: u8 = 0;

/// IoT 云服务器地址（128 字节字符串），寄存器 12067~12130。
pub const IOT_CLOUD_DNS_REGISTER: u16 = 12067;
pub const IOT_CLOUD_DNS_REGISTER_COUNT: u16 = 64;

pub const IOT_URL_DEV: &str = "dev-iot.poweroak.ltd:18760";
pub const IOT_URL_PROD: &str = "iot.bluettipower.com:18760";

pub const CLOUD_URL_OPTIONS: &[(&str, &str)] = &[
    ("研发环境", IOT_URL_DEV),
    ("生产环境", IOT_URL_PROD),
];

/// IoT 使能混合寄存器 1（`IOT_Enable_mix1` / 12170）。
///
/// 各位域 2bit：`0=无效, 1=enable, 2=disable`。选项对应功能×使能/禁用，写入时只 RMW 该位域。
pub const IOT_ENABLE_MIX1_REGISTER: u16 = 12170;

/// `trigger_BIND`：bit7/6，设备绑定（默认选项，写入 1=enable）。
pub const BIND_OPTION_DEVICE: &str = "bind";

pub const BIND_TRIGGER_OPTIONS: &[(&str, &str)] = &[
    ("设备绑定 · 使能", "bind:1"),
    ("设备绑定 · 禁用", "bind:2"),
    ("IOT模块并机排序 · 使能", "iot:1"),
    ("IOT模块并机排序 · 禁用", "iot:2"),
    ("INV模块并机排序 · 使能", "inv:1"),
    ("INV模块并机排序 · 禁用", "inv:2"),
    ("PACK模块并机排序 · 使能", "pack:1"),
    ("PACK模块并机排序 · 禁用", "pack:2"),
];

/// 并机设备类型前缀（`IOT_TYPE_HA1`）。
pub const PARALLEL_DEVICE_PREFIX: &str = "HA1";

/// 按所选功能与 1=enable / 2=disable 返回要 RMW 的位域。
pub fn bind_trigger_field(option: &str) -> Option<RegisterFieldPatch> {
    let (kind, value_text) = option.trim().split_once(':')?;
    let value: u16 = value_text.parse().ok()?;
    if value != 1 && value != 2 {
        return None;
    }
    let start_bit = match kind {
        "iot" => 0,
        "inv" => 2,
        "pack" => 4,
        "bind" => 6,
        _ => return None,
    };
    Some(RegisterFieldPatch {
        start_bit,
        width: 2,
        value,
    })
}

/// 仅「设备绑定」（bit7/6）要求单机；并机排序在 HA1 上可写。
pub fn bind_option_standalone_only(option: &str) -> bool {
    option
        .trim()
        .split_once(':')
        .map(|(kind, _)| kind == BIND_OPTION_DEVICE)
        .unwrap_or(false)
}

/// 常用设置映射表；有产品寄存器表后再往里加， schema 不落盘。
pub const BUILTIN_SETTINGS: &[BuiltinSettingDef] = &[
    BuiltinSettingDef {
        id: "cloud_url",
        name: "服务器地址",
        register: IOT_CLOUD_DNS_REGISTER,
        bit: None,
        field: None,
        widget: BuiltinWidget::Enum,
        value_type: QueryValueType::String,
        register_count: IOT_CLOUD_DNS_REGISTER_COUNT,
        enum_options: CLOUD_URL_OPTIONS,
    },
    BuiltinSettingDef {
        id: "device_bind",
        name: "绑定触发",
        register: IOT_ENABLE_MIX1_REGISTER,
        bit: None,
        field: None,
        widget: BuiltinWidget::Enum,
        value_type: QueryValueType::Integer,
        register_count: 1,
        enum_options: BIND_TRIGGER_OPTIONS,
    },
];

/// 并机（HA1 开头）设备不支持单机绑定触发。
pub fn is_parallel_ha1_device(device_name: &str) -> bool {
    device_name
        .trim()
        .to_ascii_uppercase()
        .starts_with(PARALLEL_DEVICE_PREFIX)
}

pub fn builtin_bind_supported(connected: bool, device_name: &str, option: &str) -> bool {
    connected && !(bind_option_standalone_only(option) && is_parallel_ha1_device(device_name))
}

pub fn enum_index_for_value(value: &str, options: &[(&str, &str)]) -> i32 {
    let needle = value.trim().trim_matches('\0');
    options
        .iter()
        .position(|(_, v)| *v == needle)
        .map(|i| i as i32)
        .unwrap_or(-1)
}
