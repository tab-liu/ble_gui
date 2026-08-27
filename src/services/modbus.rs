//! Modbus 数据服务（当前为模拟实现）。

use crate::ui::DashboardMetric;

#[derive(Clone, Debug, Default)]
pub struct ModbusDashboard {
    pub metrics: Vec<DashboardMetric>,
    pub summary: Vec<String>,
}

#[derive(Clone)]
pub struct ModbusService;

impl ModbusService {
    pub fn new() -> Self {
        Self
    }

    pub fn dashboard_snapshot(&self) -> ModbusDashboard {
        ModbusDashboard {
            metrics: vec![
                metric("室内温度", "24.6", "°C", "寄存器 40001", green()),
                metric("运行状态", "正常", "", "状态字 00001", green()),
                metric("输出电压", "220", "V", "寄存器 40003", blue()),
                metric("负载电流", "3.2", "A", "寄存器 40004", blue()),
            ],
            summary: vec![
                "Modbus 通信: 正常".into(),
                "最近轮询: 刚刚".into(),
                "设备固件: v2.1.0".into(),
                "已配置查询项: 可在 Modbus 页管理".into(),
            ],
        }
    }

    pub fn disconnected_dashboard(&self) -> ModbusDashboard {
        ModbusDashboard::default()
    }
}

fn metric(label: &str, value: &str, unit: &str, hint: &str, accent: slint::Color) -> DashboardMetric {
    DashboardMetric {
        label: label.into(),
        value: value.into(),
        unit: unit.into(),
        hint: hint.into(),
        accent_color: accent,
    }
}

fn green() -> slint::Color {
    slint::Color::from_rgb_u8(126, 200, 164)
}

fn blue() -> slint::Color {
    slint::Color::from_rgb_u8(74, 124, 255)
}
