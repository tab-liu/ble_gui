//! 主页仪表板寄存器映射（对齐 ref/tool：SOC@102，功率@140～147，开关@2011/2012）。

use crate::services::modbus::DashboardData;

pub const REG_SOC: u16 = 102;
pub const REG_SOC_COUNT: u16 = 1;

/// DC/AC 输出、PV 输入、AC 输入：各 2 个寄存器（低字在前）。
pub const REG_POWER_START: u16 = 140;
pub const REG_POWER_COUNT: u16 = 8;

pub const REG_AC_OUTPUT: u16 = 2011;
pub const REG_DC_OUTPUT: u16 = 2012;

pub const DEFAULT_SLAVE_ID: u8 = 0;
pub const POLL_INTERVAL_MS: u64 = 2000;
pub const MODBUS_TIMEOUT_MS: u64 = 1800;

/// SOC（102）+ 功率段 140～147（8 个寄存器）。
pub fn parse_dashboard_soc_and_power(soc: u16, power: &[u16]) -> Option<DashboardData> {
    if power.len() < 8 {
        return None;
    }
    Some(DashboardData {
        soc: (soc as i32).clamp(0, 100),
        dc_output_w: combine_u32(power[0], power[1]) as i32,
        ac_output_w: combine_u32(power[2], power[3]) as i32,
        pv_input_w: combine_u32(power[4], power[5]) as i32,
        ac_input_w: combine_i32(power[6], power[7]),
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
        assert_eq!(combine_i32(2089, 0), 2089);
        assert_eq!(combine_u32(0x1234, 0x5678), 0x5678_1234);
    }

    #[test]
    fn parse_from_soc_and_power_block() {
        let power = [10u16, 0, 20, 0, 30, 0, 2089, 0];
        let dash = parse_dashboard_soc_and_power(91, &power).unwrap();
        assert_eq!(dash.soc, 91);
        assert_eq!(dash.dc_output_w, 10);
        assert_eq!(dash.ac_output_w, 20);
        assert_eq!(dash.pv_input_w, 30);
        assert_eq!(dash.ac_input_w, 2089);
    }
}
