//! 把界面上的只读字段写入系统剪贴板。

use log::warn;

pub fn copy_text(text: &str) {
    let text = text.trim();
    if text.is_empty() || text == "—" || text == "（空）" || text == "未连接" {
        return;
    }
    match arboard::Clipboard::new() {
        Ok(mut cb) => {
            if let Err(err) = cb.set_text(text.to_string()) {
                warn!(target: "ble_gui::clipboard", "复制失败: {err}");
            }
        }
        Err(err) => warn!(target: "ble_gui::clipboard", "打开剪贴板失败: {err}"),
    }
}
