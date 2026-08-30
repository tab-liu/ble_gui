//! 编译期把 `ui/app.slint` 编进 crate（生成 `MainWindow` 等 Rust 绑定）。
//!
//! 修改任意 `ui/**/*.slint` 后需重新 `cargo build`；绑定类型在 [`crate::ui`]。

fn main() {
    slint_build::compile("ui/app.slint").unwrap();
}
