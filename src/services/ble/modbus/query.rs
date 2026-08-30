//! Modbus 查询页：寄存器地址解析与按类型格式化。

/// 查询项数值类型（与 Slint `value-type` 字符串对应）。
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum QueryValueType {
    #[default]
    Integer,
    Float,
    String,
}

impl QueryValueType {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Integer => "integer",
            Self::Float => "float",
            Self::String => "string",
        }
    }
}

/// 解析用户输入的类型字符串；空或未知值 → 整数。
pub fn parse_value_type(text: &str) -> QueryValueType {
    match text.trim().to_lowercase().as_str() {
        "" | "integer" | "int" | "整数" | "整型" => QueryValueType::Integer,
        "float" | "f32" | "浮点" | "浮点数" => QueryValueType::Float,
        "string" | "str" | "字符串" | "文本" => QueryValueType::String,
        _ => QueryValueType::Integer,
    }
}

/// 添加表单 ComboBox 索引 → 类型（0=整数, 1=浮点, 2=字符串）。
pub fn value_type_from_index(index: i32) -> QueryValueType {
    match index {
        1 => QueryValueType::Float,
        2 => QueryValueType::String,
        _ => QueryValueType::Integer,
    }
}

/// 解析用户输入的寄存器地址。
///
/// - `40001`～`49999`：Modbus 4xxxx 保持寄存器记法 → 协议地址 = 值 − 40001
/// - `0`～`65535`：直接作为协议地址（如主页用的 100、2011）
pub fn parse_register_address(text: &str) -> Result<u16, String> {
    let s = text.trim();
    if s.is_empty() {
        return Err("寄存器地址为空".into());
    }
    let n: u32 = s
        .parse()
        .map_err(|_| format!("无法解析寄存器地址: {s}"))?;
    if (40001..=49999).contains(&n) {
        return Ok((n - 40001) as u16);
    }
    if n <= u16::MAX as u32 {
        return Ok(n as u16);
    }
    Err(format!("寄存器地址超出范围: {s}"))
}

/// 解析寄存器个数；空或无效 → `default`（至少 1，至多 125）。
pub fn parse_register_count(text: &str, default: u16) -> u16 {
    let s = text.trim();
    if s.is_empty() {
        return default.max(1);
    }
    s.parse::<u16>()
        .unwrap_or(default)
        .clamp(1, 125)
}

/// 解析显示倍数；空或无效 → 1（至少 1）。
pub fn parse_scale(text: &str) -> u32 {
    let s = text.trim();
    if s.is_empty() {
        return 1;
    }
    s.parse::<u32>().unwrap_or(1).max(1)
}

/// 按类型、长度与倍数格式化一次读保持寄存器的结果。
pub fn format_query_value(
    values: &[u16],
    value_type: QueryValueType,
    scale: u32,
) -> Result<String, String> {
    match value_type {
        QueryValueType::Integer => format_integer(values, scale),
        QueryValueType::Float => format_float(values),
        QueryValueType::String => format_string(values),
    }
}

/// 连续寄存器按「低字在前」拼成 u32/u64（对齐 ref/tool CombineUInt32 / SN 解析）。
fn combine_registers_u64(values: &[u16]) -> u64 {
    let mut out = 0u64;
    for (i, &word) in values.iter().enumerate().take(4) {
        out |= (word as u64) << (16 * i);
    }
    out
}

fn combine_u32(low: u16, high: u16) -> u32 {
    (low as u32) | ((high as u32) << 16)
}

fn format_scaled(v: f64) -> String {
    if v.fract().abs() < f64::EPSILON {
        format!("{:.0}", v)
    } else {
        let s = format!("{:.6}", v);
        s.trim_end_matches('0').trim_end_matches('.').to_string()
    }
}

fn format_integer(values: &[u16], scale: u32) -> Result<String, String> {
    if values.is_empty() {
        return Err("无寄存器数据".into());
    }
    let raw = combine_registers_u64(values);
    if scale <= 1 {
        Ok(raw.to_string())
    } else {
        Ok(format_scaled(raw as f64 / scale as f64))
    }
}

fn format_float(values: &[u16]) -> Result<String, String> {
    if values.len() < 2 {
        return Err("浮点类型至少需要 2 个寄存器".into());
    }
    let bits = combine_u32(values[0], values[1]);
    let f = f32::from_bits(bits);
    if !f.is_finite() {
        return Err("浮点值无效".into());
    }
    Ok(format_scaled(f as f64))
}

/// BLUETTI ASCII 字符串：按寄存器逐个取 2 字节，寄存器内低字节在前（对齐 ref/tool
/// `DecodeAsciiRegistersLowByteFirst`；InvType 1101~1106 等同理，不能用整段 char[] 直译）。
fn format_string(values: &[u16]) -> Result<String, String> {
    if values.is_empty() {
        return Err("无寄存器数据".into());
    }
    let mut out = String::with_capacity(values.len() * 2);
    for &reg in values {
        let lo = (reg & 0xFF) as u8;
        let hi = (reg >> 8) as u8;
        for byte in [lo, hi] {
            if byte != 0 && byte != 0xFF {
                out.push(byte as char);
            }
        }
    }
    Ok(out.trim_matches(|c: char| c == '\0' || c.is_whitespace()).to_string())
}

/// 将字符串按 Modbus 寄存器规则编码（寄存器内低字节在前，与 [`format_string`] 互逆）。
pub fn encode_string_to_registers(text: &str, register_count: u16) -> Vec<u16> {
    let count = register_count.max(1) as usize;
    let mut bytes = vec![0u8; count * 2];
    for (i, &b) in text.as_bytes().iter().enumerate().take(bytes.len()) {
        bytes[i] = b;
    }
    bytes
        .chunks(2)
        .map(|pair| u16::from(pair[0]) | (u16::from(pair.get(1).copied().unwrap_or(0)) << 8))
        .collect()
}

/// 将写入文本编码为寄存器字（无倍数；整数按低字在前拆分）。
pub fn encode_write_value(
    text: &str,
    value_type: QueryValueType,
    register_count: u16,
) -> Result<Vec<u16>, String> {
    let count = register_count.max(1) as usize;
    match value_type {
        QueryValueType::String => Ok(encode_string_to_registers(text, register_count)),
        QueryValueType::Float => {
            let f: f32 = text
                .trim()
                .parse()
                .map_err(|_| format!("无法解析浮点: {text}"))?;
            let bits = f.to_bits();
            let low = (bits & 0xFFFF) as u16;
            let high = (bits >> 16) as u16;
            let mut regs = vec![low, high];
            regs.resize(count, 0);
            Ok(regs)
        }
        QueryValueType::Integer => {
            let s = text.trim();
            let on = matches!(s.to_lowercase().as_str(), "1" | "true" | "on" | "开");
            let off = matches!(s.to_lowercase().as_str(), "0" | "false" | "off" | "关");
            let raw: u64 = if on {
                1
            } else if off {
                0
            } else {
                s.parse()
                    .map_err(|_| format!("无法解析整数: {text}"))?
            };
            let mut regs = Vec::with_capacity(count);
            for i in 0..count {
                regs.push(((raw >> (16 * i)) & 0xFFFF) as u16);
            }
            Ok(regs)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn modbus_notation_40001_is_zero() {
        assert_eq!(parse_register_address("40001").unwrap(), 0);
    }

    #[test]
    fn protocol_address_100() {
        assert_eq!(parse_register_address("100").unwrap(), 100);
        assert_eq!(parse_register_address("40101").unwrap(), 100);
    }

    #[test]
    fn integer_scale_10() {
        assert_eq!(
            format_query_value(&[123], QueryValueType::Integer, 10).unwrap(),
            "12.3"
        );
    }

    #[test]
    fn integer_u32_two_regs() {
        assert_eq!(
            format_query_value(&[4, 1], QueryValueType::Integer, 1).unwrap(),
            "65540"
        );
    }

    #[test]
    fn integer_u64_four_regs() {
        // 对齐 ref/tool SN：1107~1110 低字在前拼 u64
        assert_eq!(
            format_query_value(&[1, 2, 3, 4], QueryValueType::Integer, 1).unwrap(),
            "1125912791875585"
        );
    }

    #[test]
    fn string_device_type_low_byte_first() {
        let regs = encode_string_to_registers("AP200", 3);
        assert_eq!(
            format_query_value(&regs, QueryValueType::String, 1).unwrap(),
            "AP200"
        );
    }

    #[test]
    fn string_encode_roundtrip() {
        let regs = encode_string_to_registers("AP200", 6);
        assert_eq!(
            format_query_value(&regs, QueryValueType::String, 1).unwrap(),
            "AP200"
        );
    }
}
