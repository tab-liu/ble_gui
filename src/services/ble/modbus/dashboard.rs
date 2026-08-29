//! 主页仪表板寄存器映射（对齐 ref/tool 100～149、2011～2012）。

use crate::services::modbus::DashboardData;

pub const REG_DASHBOARD_START: u16 = 100;
pub const REG_DASHBOARD_COUNT: u16 = 50;
pub const REG_AC_OUTPUT: u16 = 2011;
pub const REG_DC_OUTPUT: u16 = 2012;

pub const DEFAULT_SLAVE_ID: u8 = 0;
pub const POLL_INTERVAL_MS: u64 = 2000;
pub const MODBUS_TIMEOUT_MS: u64 = 1800;

/// 从 100～149 段 50 个寄存器解析仪表板数据。
pub fn parse_dashboard_registers(regs: &[u16]) -> Option<DashboardData> {
    if regs.len() < 48 {
        return None;
    }
    Some(DashboardData {
        soc: (regs[2] as i32).clamp(0, 100),
        dc_output_w: combine_u32(regs[40], regs[41]) as i32,
        ac_output_w: combine_u32(regs[42], regs[43]) as i32,
        pv_input_w: combine_u32(regs[44], regs[45]) as i32,
        ac_input_w: combine_i32(regs[46], regs[47]),
        data_valid: true,
        ac_output_on: false,
        dc_output_on: false,
    })
}

pub fn merge_control_states(data: &mut DashboardData, ac_on: bool, dc_on: bool) {
    data.ac_output_on = ac_on;
    data.dc_output_on = dc_on;
}

/// 两个连续 u16 拼成 u32：Modbus 低地址寄存器为低 16 位（对齐 C# CombineUInt32）。
fn combine_u32(low: u16, high: u16) -> u32 {
    (low as u32) | ((high as u32) << 16)
}

fn combine_i32(low: u16, high: u16) -> i32 {
    combine_u32(low, high) as i32
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn combine_u32_low_word_first() {
        // REG146=2089, REG147=0 → 2089W（与 C# CombineInt32 一致）
        assert_eq!(combine_i32(2089, 0), 2089);
        assert_eq!(combine_u32(0x1234, 0x5678), 0x5678_1234);
    }

    #[test]
    fn parse_soc_from_single_register() {
        let mut regs = [0u16; 50];
        regs[2] = 91;
        regs[46] = 2089;
        let dash = parse_dashboard_registers(&regs).unwrap();
        assert_eq!(dash.soc, 91);
        assert_eq!(dash.ac_input_w, 2089);
    }
}
