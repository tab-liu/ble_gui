//! Modbus 查询页：寄存器地址解析与原始值格式化。

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

/// 将单个保持寄存器原始值格式化为展示字符串（后续可扩展单位/缩放）。
pub fn format_register_value(value: u16) -> String {
    value.to_string()
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
}
