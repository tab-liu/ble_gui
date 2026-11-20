//! 配置文件里的整数和界面枚举之间的转换。
//!
//! 页面、阶段、寄存器类型直接用 Slint 生成的枚举。这里只认旧文件里的控件编号。

use crate::ui::ConfigWidget;

pub fn config_widget_from_stored(kind: i32) -> ConfigWidget {
    match kind {
        0 => ConfigWidget::Switch,
        2 => ConfigWidget::Number,
        _ => ConfigWidget::Text,
    }
}

pub fn config_widget_to_stored(kind: ConfigWidget) -> i32 {
    match kind {
        ConfigWidget::Switch => 0,
        ConfigWidget::Text => 1,
        ConfigWidget::Number => 2,
    }
}

pub fn config_widget_from_form(index: i32) -> ConfigWidget {
    match index {
        1 => ConfigWidget::Number,
        _ => ConfigWidget::Text,
    }
}

pub fn config_widget_to_form(kind: ConfigWidget) -> i32 {
    match kind {
        ConfigWidget::Number => 1,
        _ => 0,
    }
}
