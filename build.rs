//! 编译期把 `ui/app.slint` 编进 crate（生成 `MainWindow` 等 Rust 绑定）。
//!
//! 修改任意 `ui/**/*.slint` 后需重新 `cargo build`；绑定类型在 [`crate::ui`]。
//! Windows 还会把 `assets/app.ico` 嵌进 exe，资源管理器 / 任务栏才能显示图标。

fn main() {
    println!("cargo:rerun-if-changed=assets/app.ico");
    println!("cargo:rerun-if-changed=assets/app-icon.png");
    slint_build::compile("ui/app.slint").unwrap();

    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        let mut res = winres::WindowsResource::new();
        res.set_icon("assets/app.ico");
        res.set("ProductName", "BLE Modbus 工具");
        res.set("FileDescription", "BLE Modbus 工具");
        res.set("OriginalFilename", "ble_gui.exe");
        res.compile().expect("embed Windows application icon");
    }
}
