//! Slint 生成的 UI 类型与属性刷新。
//!
//! `slint::include_modules!()` 由 `build.rs` 编译 `ui/app.slint` 后注入
//! [`MainWindow`] 等类型。业务代码通过 `ui.set_*` / `ui.on_*` 与界面交互；
//! 批量刷新放在 [`bindings`]。

#![allow(unused_imports)]

slint::include_modules!();

pub mod bindings;
