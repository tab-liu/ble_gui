//! 整机（110～119）与 IoT 身份（11000 起 14 个寄存器）。

pub const REG_WHOLE_DEVICE: u16 = 110;
pub const REG_WHOLE_DEVICE_COUNT: u16 = 10;
pub const REG_IOT: u16 = 11000;
pub const REG_IOT_COUNT: u16 = 14;
pub const SLAVE_ID: u8 = 0;

const IOT_SN_OFF: usize = 6;
const IOT_SAFE_OFF: usize = 10;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct IotFields {
    pub iot_type: String,
    pub iot_sn: String,
    pub safe_code: String,
}

pub fn format_identity(values_whole: &[u16], values_iot: &[u16]) -> Result<(String, String, String), String> {
    if values_whole.len() < REG_WHOLE_DEVICE_COUNT as usize {
        return Err("Machine registers are short".into());
    }
    if values_iot.len() < REG_IOT_COUNT as usize {
        return Err("IoT registers are short".into());
    }
    let machine = join_type_sn(
        &ascii_low_byte_first(&values_whole[..6]),
        &format_u64(&values_whole[6..10]),
    );
    let iot = parse_iot(values_iot);
    let iot_text = join_type_sn(&iot.iot_type, &iot.iot_sn);
    Ok((machine, iot_text, iot.safe_code))
}

fn parse_iot(values: &[u16]) -> IotFields {
    IotFields {
        iot_type: ascii_low_byte_first(&values[..6]),
        iot_sn: format_u64(&values[IOT_SN_OFF..IOT_SN_OFF + 4]),
        safe_code: format_u64(&values[IOT_SAFE_OFF..IOT_SAFE_OFF + 4]),
    }
}

pub fn join_type_sn(ty: &str, sn: &str) -> String {
    match (ty.is_empty(), sn.is_empty()) {
        (true, true) => "—".into(),
        (false, true) => ty.to_string(),
        (true, false) => sn.to_string(),
        (false, false) => format!("{ty}{sn}"),
    }
}

fn format_u64(words: &[u16]) -> String {
    let n = words.iter().enumerate().fold(0u64, |acc, (i, &w)| {
        acc | (u64::from(w) << (16 * i))
    });
    if n == 0 {
        String::new()
    } else {
        n.to_string()
    }
}

fn ascii_low_byte_first(values: &[u16]) -> String {
    let mut out = String::new();
    for &reg in values {
        let lo = (reg & 0xFF) as u8;
        let hi = (reg >> 8) as u8;
        for byte in [lo, hi] {
            if byte != 0 && byte != 0xFF {
                out.push(byte as char);
            }
        }
    }
    out.trim().to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn joins_type_and_sn() {
        let mut whole = vec![0u16; 10];
        // "AB" in first register, low byte first.
        whole[0] = u16::from_le_bytes([b'A', b'B']);
        whole[6] = 9;
        let mut iot = vec![0u16; 14];
        iot[0] = u16::from_le_bytes([b'I', b'O']);
        iot[1] = u16::from_le_bytes([b'T', 0]);
        iot[6] = 42;
        iot[10] = 7;
        let (machine, iot_text, safe) = format_identity(&whole, &iot).unwrap();
        assert_eq!(machine, "AB9");
        assert_eq!(iot_text, "IOT42");
        assert_eq!(safe, "7");
    }
}
