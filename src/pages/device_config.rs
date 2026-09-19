//! 设备配置页：固定「常用」+ 可编辑灵活分组。
//!
//! # 设计要点
//!
//! - **常用**（`builtin=true`）：schema 在 [`crate::services::ble::modbus::BUILTIN_SETTINGS`]，
//!   不可删改名。含服务器地址枚举，以及 12170 绑定触发（位域 RMW，默认设备绑定 enable）。
//!   广播名 HA1 开头时不可写「设备绑定」，并机排序仍可写。  
//!   常用页含 WiFi 配网：写 12001/12002/12018，轮询 11018 链路与 11108 当前 SSID。  
//! - **自定义分组**：整寄存器表单（文本/数值），TOML 持久化见
//!   [`crate::services::device_config_store`]。  
//! - **读写分离**：轮询只改「读回」字段，不覆盖「设置」输入框。  
//! - 常用项与 UI 共用同一 model，只 `set_row_data`，避免 ComboBox 被整表重建冲掉。  
//! - 与查询页共用 `QueryPollSnapshot`，目标为 [`QueryPollTarget::DeviceConfig`]。

use std::rc::Rc;
use std::time::{Duration, Instant};

use log::{debug, warn};
use slint::{ComponentHandle, Model, ModelRc, SharedString, VecModel};

use crate::services::ble::modbus::{
    bind_option_standalone_only, bind_trigger_field, builtin_bind_supported, disconnect_reason_text,
    encode_write_value, enum_index_for_value, integer_debug_hex, is_parallel_ha1_device,
    is_wifi_poll_index, parse_disconnect_reason, parse_link_status, parse_register_address,
    parse_sta_ipv4, parse_value_type, sta_enable_word, wifi_auth_for_password, BuiltinSettingDef,
    BuiltinWidget, QueryValueType, RegisterFieldPatch, BUILTIN_CONFIG_SLAVE_ID, BUILTIN_SETTINGS,
    REG_WIFI_ON_OFF, REG_WIFI_STA_AUTH, REG_WIFI_STA_ENABLE, REG_WIFI_STA_PASSWORD,
    REG_WIFI_STA_PASSWORD_COUNT, REG_WIFI_STA_SSID, REG_WIFI_STA_SSID_COUNT, WIFI_PASSWORD_MAX_BYTES,
    WIFI_POLL_DISCONNECT, WIFI_POLL_LINK, WIFI_POLL_SSID_NOW, WIFI_POLL_STA_IP, WIFI_SSID_MAX_BYTES,
};
use crate::services::device_config_store;
use crate::services::modbus::{QueryItemPollResult, QueryPollTarget};
use crate::services::poll_sync::sync_poll_policy;
use crate::services::{wifi_cred_store, wifi_scan};
use crate::state::{AppContext, PAGE_DEVICE_CONFIG};
use crate::ui::{
    BuiltinConfigItem, DeviceConfigGroup, DeviceConfigItem, MainWindow, ModbusQueryLayoutRow,
    WifiSavedNetwork, WifiScanAp,
};

const WIFI_CONNECT_TIMEOUT: Duration = Duration::from_secs(45);
const CLOUD_CONNECT_TIMEOUT: Duration = Duration::from_secs(60);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum WifiProvisionPhase {
    Idle,
    ConnectingWifi,
    ConnectingCloud,
    Success,
    Failed,
}

pub struct WifiProvisionUiState {
    pub saved: Rc<VecModel<WifiSavedNetwork>>,
    pub scan: Rc<VecModel<WifiScanAp>>,
    phase: WifiProvisionPhase,
    phase_since: Option<Instant>,
    pending_ssid: String,
    wifi_sta: bool,
    mqtt: bool,
    ssid_now: String,
    sta_ip: String,
    disconnect_reason: u16,
    hint: String,
}

impl WifiProvisionUiState {
    fn load() -> Self {
        let saved: Vec<WifiSavedNetwork> = wifi_cred_store::load()
            .into_iter()
            .map(|n| WifiSavedNetwork {
                ssid: n.ssid.into(),
                password: n.password.into(),
            })
            .collect();
        Self {
            saved: Rc::new(VecModel::from(saved)),
            scan: Rc::new(VecModel::from(Vec::<WifiScanAp>::new())),
            phase: WifiProvisionPhase::Idle,
            phase_since: None,
            pending_ssid: String::new(),
            wifi_sta: false,
            mqtt: false,
            ssid_now: String::new(),
            sta_ip: String::new(),
            disconnect_reason: 0,
            hint: String::new(),
        }
    }
}

pub struct DeviceConfigState {
    pub groups: Rc<VecModel<DeviceConfigGroup>>,
    pub builtin_items: Rc<VecModel<BuiltinConfigItem>>,
    pub tab_strip_width: f32,
    pub wifi: WifiProvisionUiState,
}

fn empty_read() -> (SharedString, SharedString, SharedString, i32) {
    ("—".into(), "—".into(), "".into(), 14)
}

fn result_hex_for(result: &str, value_type: &SharedString) -> SharedString {
    integer_debug_hex(result, parse_value_type(&value_type.to_string()), 1).into()
}

fn result_font_size(char_count: usize) -> i32 {
    match char_count {
        0..=6 => 16,
        7..=12 => 14,
        13..=20 => 12,
        _ => 11,
    }
}

fn display_for_config(result: &str, widget_kind: i32) -> SharedString {
    if widget_kind == 0 {
        match result.trim() {
            "1" | "true" => "开".into(),
            "0" | "false" => "关".into(),
            other if other.is_empty() || other == "—" => "—".into(),
            other => other.into(),
        }
    } else {
        result.into()
    }
}

fn display_for_builtin_result(result: &str, def: &BuiltinSettingDef) -> SharedString {
    display_for_config(result, def.widget as i32)
}

fn builtin_availability(
    def: &BuiltinSettingDef,
    connected: bool,
    device_name: &str,
    write_value: &str,
) -> (bool, SharedString) {
    if def.id == "device_bind" {
        let enabled = builtin_bind_supported(connected, device_name, write_value);
        let hint = if connected
            && bind_option_standalone_only(write_value)
            && is_parallel_ha1_device(device_name)
        {
            "并机(HA1)设备不支持设备绑定".into()
        } else {
            "".into()
        };
        (enabled, hint)
    } else {
        (connected, "".into())
    }
}

fn apply_builtin_availability_in_place(ctx: &AppContext) {
    let snap = ctx.ble.snapshot();
    let connected = ctx.ble.is_connected();
    let st = ctx.state.borrow();
    for i in 0..st.device_config.builtin_items.row_count() {
        let Some(def) = BUILTIN_SETTINGS.get(i) else {
            continue;
        };
        let Some(mut item) = st.device_config.builtin_items.row_data(i) else {
            continue;
        };
        let (write_enabled, disabled_hint) =
            builtin_availability(def, connected, &snap.device_name, item.write_value.as_str());
        if item.write_enabled != write_enabled || item.disabled_hint != disabled_hint {
            item.write_enabled = write_enabled;
            item.disabled_hint = disabled_hint;
            st.device_config.builtin_items.set_row_data(i, item);
        }
    }
}

fn builtin_item_from_def(def: &BuiltinSettingDef) -> BuiltinConfigItem {
    let default_write = def
        .enum_options
        .first()
        .map(|(_, v)| (*v).to_string())
        .unwrap_or_default();
    let enum_labels: Vec<SharedString> = def
        .enum_options
        .iter()
        .map(|(label, _)| (*label).into())
        .collect();
    let enum_values: Vec<SharedString> = def
        .enum_options
        .iter()
        .map(|(_, value)| (*value).into())
        .collect();
    let enum_index = enum_index_for_value(&default_write, def.enum_options);
    BuiltinConfigItem {
        id: def.id.into(),
        name: def.name.into(),
        register: def.register.to_string().into(),
        value_type: def.value_type.as_str().into(),
        register_count: def.register_count.max(1) as i32,
        widget_kind: def.widget as i32,
        enum_labels: ModelRc::new(VecModel::from(enum_labels)),
        enum_values: ModelRc::new(VecModel::from(enum_values)),
        enum_index,
        result_display: "—".into(),
        result_hex: "".into(),
        write_value: default_write.into(),
        status: "等待读取".into(),
        dirty: false,
        write_enabled: false,
        disabled_hint: "".into(),
    }
}

fn builtin_items_from_defs() -> Rc<VecModel<BuiltinConfigItem>> {
    let items: Vec<BuiltinConfigItem> = BUILTIN_SETTINGS
        .iter()
        .map(builtin_item_from_def)
        .collect();
    Rc::new(VecModel::from(items))
}

fn refresh_builtin_enum_index(item: &mut BuiltinConfigItem, options: &[(&str, &str)]) {
    item.enum_index = enum_index_for_value(item.write_value.as_str(), options);
}

impl DeviceConfigState {
    pub fn new(
        groups: Rc<VecModel<DeviceConfigGroup>>,
        builtin_items: Rc<VecModel<BuiltinConfigItem>>,
    ) -> Self {
        Self {
            groups,
            builtin_items,
            tab_strip_width: 804.0,
            wifi: WifiProvisionUiState::load(),
        }
    }

    /// 仅固定「常用」组；灵活分组由用户添加。
    pub fn sample_groups() -> Rc<VecModel<DeviceConfigGroup>> {
        Rc::new(VecModel::from(vec![DeviceConfigGroup {
            title: "常用".into(),
            slave_id: BUILTIN_CONFIG_SLAVE_ID.to_string().into(),
            builtin: true,
            items: ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())),
        }]))
    }

    pub fn sample_builtin() -> Rc<VecModel<BuiltinConfigItem>> {
        builtin_items_from_defs()
    }
}

fn is_builtin_group(ctx: &AppContext, group_index: usize) -> bool {
    ctx.state
        .borrow()
        .device_config
        .groups
        .row_data(group_index)
        .map(|g| g.builtin)
        .unwrap_or(false)
}

fn widget_kind_from_index(index: i32) -> i32 {
    match index {
        1 => 2,
        _ => 1,
    }
}

fn widget_kind_to_form_index(kind: i32) -> i32 {
    if kind == 2 {
        1
    } else {
        0
    }
}

fn value_type_from_index(index: i32) -> SharedString {
    match index {
        1 => "float".into(),
        2 => "string".into(),
        _ => "integer".into(),
    }
}

fn value_type_to_form_index(text: &str) -> i32 {
    match text {
        "float" => 1,
        "string" => 2,
        _ => 0,
    }
}

fn clear_config_form(ui: &MainWindow) {
    ui.set_show_add_config_form(false);
    ui.set_editing_config_index(-1);
    ui.set_config_form_name("".into());
    ui.set_config_form_register("".into());
    ui.set_config_form_widget_index(1);
    ui.set_config_form_value_type_index(0);
    ui.set_config_form_register_count("1".into());
}

fn tab_item_width(title: &str) -> f32 {
    (title.chars().count() as f32 * 7.5 + 24.0).max(72.0)
}

fn compute_group_tab_rows(
    groups: &Rc<VecModel<DeviceConfigGroup>>,
    width: f32,
) -> Vec<ModbusQueryLayoutRow> {
    let width = width.max(80.0);
    let mut rows: Vec<Vec<i32>> = Vec::new();
    let mut current: Vec<i32> = Vec::new();
    let mut used = 0.0;
    for i in 0..groups.row_count() {
        let title = groups
            .row_data(i)
            .map(|g| g.title.to_string())
            .unwrap_or_default();
        let w = tab_item_width(&title);
        let extra = if current.is_empty() { w } else { 6.0 + w };
        if !current.is_empty() && used + extra > width {
            rows.push(std::mem::take(&mut current));
            current.push(i as i32);
            used = w;
        } else {
            current.push(i as i32);
            used += extra;
        }
    }
    if !current.is_empty() {
        rows.push(current);
    }
    rows.into_iter()
        .map(|indices| ModbusQueryLayoutRow {
            indices: ModelRc::new(VecModel::from(indices)),
        })
        .collect()
}

fn sync_tab_strip_layout(ui: &MainWindow, ctx: &AppContext) {
    let st = ctx.state.borrow();
    let width = st.device_config.tab_strip_width;
    let rows = compute_group_tab_rows(&st.device_config.groups, width);
    drop(st);
    ui.set_config_tab_layout_rows(ModelRc::new(VecModel::from(rows)));
}

/// 按当前窗口宽度重算分组标签换行（刚进页时 `changed width` 不会触发）。
pub fn sync_layout_from_window(ui: &MainWindow, ctx: &AppContext) {
    const TAB_WINDOW_CHROME: f32 = 176.0;
    let window = ui.window();
    let scale = window.scale_factor().max(0.01);
    let win_w = window.size().width as f32 / scale;
    if win_w >= 72.0 + TAB_WINDOW_CHROME {
        ctx.state.borrow_mut().device_config.tab_strip_width =
            (win_w - TAB_WINDOW_CHROME).max(80.0);
    }
    sync_tab_strip_layout(ui, ctx);
}

fn items_vec(group: &DeviceConfigGroup) -> Vec<DeviceConfigItem> {
    (0..group.items.row_count())
        .filter_map(|i| group.items.row_data(i))
        .collect()
}

fn sync_active_to_ui(ui: &MainWindow, ctx: &AppContext) {
    let group_index = ui.get_active_config_group() as usize;
    let st = ctx.state.borrow();
    let Some(group) = st.device_config.groups.row_data(group_index) else {
        ui.set_active_config_items(ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())));
        ui.set_active_group_slave_id("0".into());
        return;
    };
    ui.set_active_group_slave_id(group.slave_id.clone());
    if group.builtin {
        drop(st);
        apply_builtin_availability_in_place(ctx);
        let model = ctx.state.borrow().device_config.builtin_items.clone();
        ui.set_builtin_config_items(ModelRc::new(model));
        ui.set_active_config_items(ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())));
    } else {
        let items = items_vec(&group);
        drop(st);
        ui.set_active_config_items(ModelRc::new(VecModel::from(items)));
    }
}

fn write_group_items(
    ctx: &AppContext,
    group_index: usize,
    slave_id: SharedString,
    title: SharedString,
    builtin: bool,
    items: Vec<DeviceConfigItem>,
) {
    let st = ctx.state.borrow();
    st.device_config.groups.set_row_data(
        group_index,
        DeviceConfigGroup {
            title,
            slave_id,
            builtin,
            items: ModelRc::new(VecModel::from(items)),
        },
    );
}

/// BLE 连接态或设备名变化时刷新常用项可用性（如 HA1 并机禁用绑定）。
/// 只就地改 `write_enabled`，不重建 model，避免 ComboBox 无法点选。
pub fn refresh_builtin_availability(ui: &MainWindow, ctx: &AppContext) {
    apply_builtin_availability_in_place(ctx);
    if !ctx.ble.is_connected() {
        let wifi = &mut ctx.state.borrow_mut().device_config.wifi;
        wifi.phase = WifiProvisionPhase::Idle;
        wifi.phase_since = None;
        wifi.pending_ssid.clear();
        wifi.wifi_sta = false;
        wifi.mqtt = false;
        wifi.ssid_now.clear();
        wifi.sta_ip.clear();
        wifi.hint.clear();
    }
    refresh_wifi_status(ui, ctx);
}

/// 配网超时在后台计时，不占用页面；文案只在配置页刷新。
pub fn tick_wifi_provision(ui: &MainWindow, ctx: &AppContext) {
    advance_wifi_phase(ctx);
    if ui.get_current_page() == PAGE_DEVICE_CONFIG {
        refresh_wifi_status(ui, ctx);
    }
}

fn apply_wifi_poll_item(ctx: &AppContext, r: &QueryItemPollResult) {
    let wifi = &mut ctx.state.borrow_mut().device_config.wifi;
    if !r.ok {
        return;
    }
    match r.item_index {
        WIFI_POLL_LINK => {
            let (sta, mqtt) = parse_link_status(&r.result);
            wifi.wifi_sta = sta;
            wifi.mqtt = mqtt;
        }
        WIFI_POLL_SSID_NOW => {
            wifi.ssid_now = r.result.trim().to_string();
        }
        WIFI_POLL_STA_IP => {
            wifi.sta_ip = parse_sta_ipv4(&r.result);
        }
        WIFI_POLL_DISCONNECT => {
            wifi.disconnect_reason = parse_disconnect_reason(&r.result);
        }
        _ => {}
    }
}

fn ssid_matches(expected: &str, actual: &str) -> bool {
    let expected = expected.trim();
    !expected.is_empty() && expected == actual.trim()
}

fn advance_wifi_phase(ctx: &AppContext) {
    if !ctx.ble.is_connected() {
        return;
    }
    let wifi = &mut ctx.state.borrow_mut().device_config.wifi;
    let elapsed = wifi.phase_since.map(|t| t.elapsed());
    match wifi.phase {
        WifiProvisionPhase::ConnectingWifi => {
            if wifi.wifi_sta && ssid_matches(&wifi.pending_ssid, &wifi.ssid_now) {
                wifi.phase = WifiProvisionPhase::ConnectingCloud;
                wifi.phase_since = Some(Instant::now());
                wifi.hint.clear();
            } else if wifi.wifi_sta && wifi.pending_ssid.is_empty() {
                wifi.phase = WifiProvisionPhase::ConnectingCloud;
                wifi.phase_since = Some(Instant::now());
            } else if elapsed.is_some_and(|d| d >= WIFI_CONNECT_TIMEOUT) {
                wifi.phase = WifiProvisionPhase::Failed;
                wifi.hint = disconnect_reason_text(wifi.disconnect_reason)
                    .unwrap_or("WiFi 连接超时")
                    .to_string();
            }
        }
        WifiProvisionPhase::ConnectingCloud => {
            if wifi.mqtt {
                wifi.phase = WifiProvisionPhase::Success;
                wifi.phase_since = None;
                wifi.pending_ssid.clear();
                wifi.hint.clear();
            } else if !wifi.wifi_sta {
                wifi.phase = WifiProvisionPhase::ConnectingWifi;
                wifi.phase_since = Some(Instant::now());
            } else if elapsed.is_some_and(|d| d >= CLOUD_CONNECT_TIMEOUT) {
                wifi.phase = WifiProvisionPhase::Failed;
                wifi.hint = "云端连接超时".into();
            }
        }
        WifiProvisionPhase::Failed => {
            if wifi.wifi_sta && wifi.mqtt {
                wifi.phase = WifiProvisionPhase::Success;
                wifi.hint.clear();
            }
        }
        WifiProvisionPhase::Idle | WifiProvisionPhase::Success => {
            if wifi.wifi_sta && wifi.mqtt {
                wifi.phase = WifiProvisionPhase::Success;
            }
        }
    }
}

fn refresh_wifi_status(ui: &MainWindow, ctx: &AppContext) {
    let connected = ctx.ble.is_connected();
    let st = ctx.state.borrow();
    let wifi = &st.device_config.wifi;
    let phase = wifi.phase;
    let (wifi_ok, wifi_pending, cloud_ok, cloud_pending, wifi_label, cloud_label, hint) =
        if !connected {
            (
                false,
                false,
                false,
                false,
                "WiFi 未连接".into(),
                "服务器未连接".into(),
                String::new(),
            )
        } else {
            match phase {
                WifiProvisionPhase::Failed => {
                    let wifi_failed = !wifi.wifi_sta && !wifi.hint.is_empty();
                    let cloud_failed = wifi.wifi_sta && !wifi.mqtt;
                    (
                        wifi.wifi_sta,
                        false,
                        wifi.mqtt,
                        false,
                        if wifi_failed {
                            wifi.hint.clone().into()
                        } else if wifi.wifi_sta {
                            "WiFi 已连接".into()
                        } else {
                            "WiFi 未连接".into()
                        },
                        if wifi.mqtt {
                            "服务器已连接".into()
                        } else if cloud_failed {
                            if wifi.hint.is_empty() {
                                "云端连接超时".into()
                            } else {
                                wifi.hint.clone().into()
                            }
                        } else {
                            "服务器未连接".into()
                        },
                        String::new(),
                    )
                }
                WifiProvisionPhase::ConnectingWifi
                | WifiProvisionPhase::ConnectingCloud
                | WifiProvisionPhase::Idle
                | WifiProvisionPhase::Success => {
                    let wifi_ok = wifi.wifi_sta;
                    let cloud_ok = wifi.mqtt;
                    let wifi_pending = matches!(phase, WifiProvisionPhase::ConnectingWifi) && !wifi_ok;
                    let cloud_pending = wifi_ok && !cloud_ok;
                    (
                        wifi_ok,
                        wifi_pending,
                        cloud_ok,
                        cloud_pending,
                        if wifi_pending {
                            "正在连接 WiFi…".into()
                        } else if wifi_ok {
                            "WiFi 已连接".into()
                        } else {
                            "WiFi 未连接".into()
                        },
                        if cloud_ok {
                            "服务器已连接".into()
                        } else if cloud_pending {
                            "正在连接云端…".into()
                        } else {
                            "服务器未连接".into()
                        },
                        String::new(),
                    )
                }
            }
        };
    let current_ssid = if connected && wifi.wifi_sta {
        let mut extra = String::new();
        if !wifi.ssid_now.is_empty() {
            extra.push_str(&wifi.ssid_now);
        }
        if !wifi.sta_ip.is_empty() {
            if !extra.is_empty() {
                extra.push_str("  ");
            }
            extra.push_str(&wifi.sta_ip);
        }
        extra
    } else {
        String::new()
    };
    drop(st);

    ui.set_wifi_ok(wifi_ok);
    ui.set_wifi_pending(wifi_pending);
    ui.set_wifi_cloud_ok(cloud_ok);
    ui.set_wifi_cloud_pending(cloud_pending);
    ui.set_wifi_status_label(wifi_label);
    ui.set_wifi_cloud_status_label(cloud_label);
    ui.set_wifi_current_ssid(current_ssid.into());
    ui.set_wifi_apply_busy(false);
    if ui.get_wifi_scan_busy() {
        return;
    }
    if !connected
        || matches!(
            phase,
            WifiProvisionPhase::ConnectingWifi
                | WifiProvisionPhase::ConnectingCloud
                | WifiProvisionPhase::Failed
        )
    {
        ui.set_wifi_hint(hint.into());
    }
}

fn persist_wifi_networks(ctx: &AppContext) {
    let saved = ctx.state.borrow().device_config.wifi.saved.clone();
    let networks: Vec<wifi_cred_store::WifiNetwork> = (0..saved.row_count())
        .filter_map(|i| saved.row_data(i))
        .map(|n| wifi_cred_store::WifiNetwork {
            ssid: n.ssid.to_string(),
            password: n.password.to_string(),
        })
        .collect();
    if let Err(e) = wifi_cred_store::save(&networks) {
        warn!(target: "ble_gui::wifi_store", "保存 WiFi 记录失败: {e}");
    }
}

fn upsert_saved_wifi(ctx: &AppContext, ssid: &str, password: &str) {
    let saved = ctx.state.borrow().device_config.wifi.saved.clone();
    for i in (0..saved.row_count()).rev() {
        if saved
            .row_data(i)
            .is_some_and(|n| n.ssid.as_str() == ssid)
        {
            saved.remove(i);
        }
    }
    saved.insert(
        0,
        WifiSavedNetwork {
            ssid: ssid.into(),
            password: password.into(),
        },
    );
    while saved.row_count() > wifi_cred_store::MAX_NETWORKS {
        saved.remove(saved.row_count() - 1);
    }
    persist_wifi_networks(ctx);
}

fn apply_wifi_credentials(ui: &MainWindow, ctx: &AppContext) {
    if !ctx.ble.is_connected() {
        ui.set_wifi_hint("请先连接设备".into());
        return;
    }
    let ssid = ui.get_wifi_ssid().to_string().trim().to_string();
    let password = ui.get_wifi_password().to_string();
    if ssid.is_empty() {
        ui.set_wifi_hint("请填写 WiFi 名称".into());
        return;
    }
    if ssid.as_bytes().len() > WIFI_SSID_MAX_BYTES {
        ui.set_wifi_hint("WiFi 名称过长（最多 32 字节）".into());
        return;
    }
    if password.as_bytes().len() > WIFI_PASSWORD_MAX_BYTES {
        ui.set_wifi_hint("密码过长（最多 64 字节）".into());
        return;
    }
    if !password.is_empty() && password.len() < 8 {
        ui.set_wifi_hint("密码至少 8 位；开放网络请留空".into());
        return;
    }

    let auth = wifi_auth_for_password(&password);
    let ssid_regs = match encode_write_value(
        &ssid,
        QueryValueType::String,
        REG_WIFI_STA_SSID_COUNT,
    ) {
        Ok(v) => v,
        Err(err) => {
            ui.set_wifi_hint(err.into());
            return;
        }
    };
    let password_regs = match encode_write_value(
        &password,
        QueryValueType::String,
        REG_WIFI_STA_PASSWORD_COUNT,
    ) {
        Ok(v) => v,
        Err(err) => {
            ui.set_wifi_hint(err.into());
            return;
        }
    };

    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_AUTH,
        vec![auth],
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_PASSWORD,
        password_regs,
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_ENABLE,
        vec![sta_enable_word(&password)],
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_STA_SSID,
        ssid_regs,
        None,
        None,
    );
    ctx.ble.write_holding(
        BUILTIN_CONFIG_SLAVE_ID,
        REG_WIFI_ON_OFF,
        vec![1],
        None,
        Some(RegisterFieldPatch {
            start_bit: 0,
            width: 2,
            value: 1,
        }),
    );

    upsert_saved_wifi(ctx, &ssid, &password);
    ctx.modbus.remember_wifi_credentials(&password);
    {
        let wifi = &mut ctx.state.borrow_mut().device_config.wifi;
        wifi.phase = WifiProvisionPhase::ConnectingWifi;
        wifi.phase_since = Some(Instant::now());
        wifi.pending_ssid = ssid;
        wifi.hint.clear();
    }
    refresh_wifi_status(ui, ctx);
}

fn touch_poll_policy(ui: &MainWindow, ctx: &AppContext) {
    if ctx.ble.ui_page() == PAGE_DEVICE_CONFIG {
        sync_poll_policy(ui, ctx);
    }
}

fn persist_device_config(ctx: &AppContext, ui: &MainWindow) {
    let groups = ctx.state.borrow().device_config.groups.clone();
    let active = ui.get_active_config_group();
    if let Err(e) = device_config_store::save(&groups, active) {
        warn!(
            target: "ble_gui::config_store",
            "保存设备配置失败: {e}",
        );
    }
}

/// 将 worker 轮询结果合并到设备配置读回（不覆盖 write_value）。
pub fn apply_config_poll_results(ui: &MainWindow, ctx: &AppContext) {
    let snapshot = ctx
        .modbus
        .shared_query_live()
        .lock()
        .map(|s| s.clone())
        .unwrap_or_default();

    if snapshot.items.is_empty() {
        return;
    }

    let QueryPollTarget::DeviceConfig {
        group_index,
        builtin,
    } = snapshot.target
    else {
        return;
    };

    let active = ui.get_active_config_group() as usize;
    if group_index != active {
        debug!(
            target: "ble_gui::config_ui",
            "apply 跳过: 快照组 {group_index} != 当前组 {active}",
        );
        return;
    }

    if builtin {
        for r in &snapshot.items {
            if is_wifi_poll_index(r.item_index) {
                apply_wifi_poll_item(ctx, r);
                continue;
            }
            let st = ctx.state.borrow();
            let Some(mut item) = st.device_config.builtin_items.row_data(r.item_index) else {
                continue;
            };
            let new_status: SharedString = r.status.clone().into();
            let new_display = BUILTIN_SETTINGS
                .get(r.item_index)
                .map(|def| display_for_builtin_result(&r.result, def))
                .unwrap_or_else(|| display_for_config(&r.result, item.widget_kind));
            let new_hex = BUILTIN_SETTINGS
                .get(r.item_index)
                .map(|def| integer_debug_hex(&r.result, def.value_type, 1).into())
                .unwrap_or_else(|| result_hex_for(&r.result, &item.value_type));
            if item.status != new_status
                || item.result_display != new_display
                || item.result_hex != new_hex
            {
                item.status = new_status;
                item.result_display = new_display;
                item.result_hex = new_hex;
                if !item.dirty {
                    if let Some(def) = BUILTIN_SETTINGS.get(r.item_index) {
                        if def.widget == BuiltinWidget::Enum
                            && def.id != "device_bind"
                            && def.field.is_none()
                            && !r.result.trim().is_empty()
                        {
                            let idx = enum_index_for_value(&r.result, def.enum_options);
                            if idx >= 0 {
                                item.write_value = r.result.clone().into();
                                item.enum_index = idx;
                            }
                        }
                    }
                }
                st.device_config
                    .builtin_items
                    .set_row_data(r.item_index, item);
            }
        }
        advance_wifi_phase(ctx);
        refresh_wifi_status(ui, ctx);
        return;
    }

    let st = ctx.state.borrow();
    let Some(group) = st.device_config.groups.row_data(group_index) else {
        return;
    };
    let mut items = items_vec(&group);
    let title = group.title.clone();
    let slave_id = group.slave_id.clone();
    let group_builtin = group.builtin;
    drop(st);

    let mut changed = false;
    for r in &snapshot.items {
        let Some(item) = items.get_mut(r.item_index) else {
            continue;
        };
        let new_status: SharedString = r.status.clone().into();
        let new_result: SharedString = r.result.clone().into();
        let new_hex = result_hex_for(&new_result, &item.value_type);
        if item.status != new_status || item.result != new_result || item.result_hex != new_hex {
            item.status = new_status;
            item.result = new_result.clone();
            item.result_display = display_for_config(&new_result, item.widget_kind);
            item.result_hex = new_hex;
            item.result_font_size = result_font_size(new_result.chars().count());
            changed = true;
        }
    }

    if !changed {
        return;
    }

    write_group_items(ctx, group_index, slave_id, title, group_builtin, items);
    sync_active_to_ui(ui, ctx);
}

fn parse_switch_on(text: &str) -> bool {
    matches!(
        text.trim().to_lowercase().as_str(),
        "1" | "true" | "on" | "开"
    )
}

pub fn wire(ui: &MainWindow, ctx: &AppContext) {
    ui.set_config_groups(ModelRc::new(ctx.state.borrow().device_config.groups.clone()));
    ui.set_active_config_group(ctx.initial_device_config_group);
    sync_active_to_ui(ui, ctx);
    ui.set_show_add_config_form(false);
    ui.set_config_form_widget_index(1);
    ui.set_config_form_value_type_index(0);
    ui.set_config_form_register_count("1".into());
    ui.set_renaming_config_group_index(-1);
    ui.set_editing_config_index(-1);
    {
        let wifi = &ctx.state.borrow().device_config.wifi;
        ui.set_wifi_saved_networks(ModelRc::new(wifi.saved.clone()));
        ui.set_wifi_scan_aps(ModelRc::new(wifi.scan.clone()));
    }
    refresh_wifi_status(ui, ctx);
    sync_layout_from_window(ui, ctx);

    let ui_weak = ui.as_weak();
    let ctx_tabs = ctx.clone();
    ui.on_config_tab_strip_width_changed(move |width| {
        if width < 80.0 {
            return;
        }
        let ui = ui_weak.unwrap();
        ctx_tabs.state.borrow_mut().device_config.tab_strip_width = width;
        sync_tab_strip_layout(&ui, &ctx_tabs);
    });

    let ui_weak = ui.as_weak();
    let ctx_add_group = ctx.clone();
    ui.on_add_device_config_group_request(move || {
        let ui = ui_weak.unwrap();
        let st = ctx_add_group.state.borrow();
        let custom_n = (0..st.device_config.groups.row_count())
            .filter_map(|i| st.device_config.groups.row_data(i))
            .filter(|g| !g.builtin)
            .count()
            + 1;
        st.device_config.groups.push(DeviceConfigGroup {
            title: format!("分组 {custom_n}").into(),
            slave_id: "0".into(),
            builtin: false,
            items: ModelRc::new(VecModel::from(Vec::<DeviceConfigItem>::new())),
        });
        let idx = st.device_config.groups.row_count() as i32 - 1;
        drop(st);
        ui.set_active_config_group(idx);
        clear_config_form(&ui);
        sync_active_to_ui(&ui, &ctx_add_group);
        sync_tab_strip_layout(&ui, &ctx_add_group);
        touch_poll_policy(&ui, &ctx_add_group);
        persist_device_config(&ctx_add_group, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_rm = ctx.clone();
    ui.on_remove_device_config_group(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        if is_builtin_group(&ctx_rm, idx) {
            return;
        }
        let st = ctx_rm.state.borrow();
        if idx >= st.device_config.groups.row_count() {
            return;
        }
        // 至少保留「常用」
        if st.device_config.groups.row_count() <= 1 {
            return;
        }
        st.device_config.groups.remove(idx);
        let max = st.device_config.groups.row_count().saturating_sub(1);
        drop(st);
        let new_active = (ui.get_active_config_group() as usize).min(max);
        ui.set_active_config_group(new_active as i32);
        ui.set_renaming_config_group_index(-1);
        clear_config_form(&ui);
        sync_active_to_ui(&ui, &ctx_rm);
        sync_tab_strip_layout(&ui, &ctx_rm);
        touch_poll_policy(&ui, &ctx_rm);
        persist_device_config(&ctx_rm, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_sw = ctx.clone();
    ui.on_switch_device_config_group(move |index| {
        let ui = ui_weak.unwrap();
        ui.set_active_config_group(index);
        ui.set_renaming_config_group_index(-1);
        clear_config_form(&ui);
        sync_active_to_ui(&ui, &ctx_sw);
        touch_poll_policy(&ui, &ctx_sw);
        persist_device_config(&ctx_sw, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_start_rename = ctx.clone();
    ui.on_start_rename_device_config_group(move |index| {
        if is_builtin_group(&ctx_start_rename, index as usize) {
            return;
        }
        let ui = ui_weak.unwrap();
        let title = ctx_start_rename
            .state
            .borrow()
            .device_config
            .groups
            .row_data(index as usize)
            .map(|g| g.title)
            .unwrap_or_default();
        ui.set_editing_group_title(title);
        ui.set_renaming_config_group_index(index);
    });

    let ui_weak = ui.as_weak();
    let ctx_rename = ctx.clone();
    ui.on_commit_rename_device_config_group(move || {
        let ui = ui_weak.unwrap();
        let idx = ui.get_renaming_config_group_index();
        if idx < 0 || is_builtin_group(&ctx_rename, idx as usize) {
            ui.set_renaming_config_group_index(-1);
            return;
        }
        let title = ui.get_editing_group_title().to_string();
        let st = ctx_rename.state.borrow();
        if let Some(mut g) = st.device_config.groups.row_data(idx as usize) {
            if !title.trim().is_empty() {
                g.title = title.trim().into();
                st.device_config.groups.set_row_data(idx as usize, g);
            }
        }
        drop(st);
        ui.set_renaming_config_group_index(-1);
        sync_tab_strip_layout(&ui, &ctx_rename);
        persist_device_config(&ctx_rename, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_slave = ctx.clone();
    ui.on_commit_device_config_group_slave_id(move || {
        let ui = ui_weak.unwrap();
        let idx = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_slave, idx) {
            return;
        }
        let slave = ui.get_active_group_slave_id();
        let st = ctx_slave.state.borrow();
        if let Some(mut g) = st.device_config.groups.row_data(idx) {
            g.slave_id = slave;
            st.device_config.groups.set_row_data(idx, g);
        }
        drop(st);
        touch_poll_policy(&ui, &ctx_slave);
        persist_device_config(&ctx_slave, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_show_add = ctx.clone();
    ui.on_show_add_device_config_panel(move || {
        let ui = ui_weak.unwrap();
        if is_builtin_group(&ctx_show_add, ui.get_active_config_group() as usize) {
            return;
        }
        ui.set_editing_config_index(-1);
        ui.set_show_add_config_form(true);
        ui.set_config_form_name("".into());
        ui.set_config_form_register("".into());
        ui.set_config_form_widget_index(1);
        ui.set_config_form_value_type_index(0);
        ui.set_config_form_register_count("1".into());
    });

    let ui_weak = ui.as_weak();
    ui.on_cancel_add_device_config(move || {
        clear_config_form(&ui_weak.unwrap());
    });

    let ui_weak = ui.as_weak();
    let ctx_add = ctx.clone();
    ui.on_confirm_add_device_config(move || {
        let ui = ui_weak.unwrap();
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_add, group_index) {
            return;
        }
        let name = ui.get_config_form_name().to_string();
        let register = ui.get_config_form_register().to_string();
        if name.trim().is_empty() || register.trim().is_empty() {
            return;
        }
        let count = ui
            .get_config_form_register_count()
            .to_string()
            .trim()
            .parse::<i32>()
            .unwrap_or(1)
            .max(1);
        let (result, result_display, result_hex, result_font_size) = empty_read();
        let item = DeviceConfigItem {
            name: name.trim().into(),
            register: register.trim().into(),
            value_type: value_type_from_index(ui.get_config_form_value_type_index()),
            register_count: count,
            widget_kind: widget_kind_from_index(ui.get_config_form_widget_index()),
            result,
            result_display,
            result_hex,
            result_font_size,
            write_value: "".into(),
            status: "等待读取".into(),
            dirty: false,
        };

        let st = ctx_add.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        let edit_idx = ui.get_editing_config_index();
        if edit_idx >= 0 {
            let i = edit_idx as usize;
            if i >= items.len() {
                return;
            }
            let register_changed = items[i].register != item.register
                || items[i].value_type != item.value_type
                || items[i].register_count != item.register_count;
            items[i].name = item.name;
            items[i].register = item.register;
            items[i].value_type = item.value_type;
            items[i].register_count = item.register_count;
            items[i].widget_kind = item.widget_kind;
            if register_changed {
                items[i].result = item.result;
                items[i].result_display = item.result_display;
                items[i].result_hex = item.result_hex;
                items[i].result_font_size = item.result_font_size;
                items[i].status = item.status;
            }
        } else {
            items.push(item);
        }
        write_group_items(&ctx_add, group_index, slave_id, title, builtin, items);
        clear_config_form(&ui);
        sync_active_to_ui(&ui, &ctx_add);
        touch_poll_policy(&ui, &ctx_add);
        persist_device_config(&ctx_add, &ui);
    });

    let ui_weak = ui.as_weak();
    let ctx_edit_item = ctx.clone();
    ui.on_edit_device_config_item(move |item_index| {
        let ui = ui_weak.unwrap();
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_edit_item, group_index) {
            return;
        }
        let st = ctx_edit_item.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let Some(item) = group.items.row_data(item_index as usize) else {
            return;
        };
        drop(st);
        ui.set_editing_config_index(item_index);
        ui.set_config_form_name(item.name);
        ui.set_config_form_register(item.register);
        ui.set_config_form_widget_index(widget_kind_to_form_index(item.widget_kind));
        ui.set_config_form_value_type_index(value_type_to_form_index(item.value_type.as_str()));
        ui.set_config_form_register_count(item.register_count.to_string().into());
        ui.set_show_add_config_form(true);
    });

    let ui_weak = ui.as_weak();
    let ctx_edit = ctx.clone();
    ui.on_device_config_value_edited(move |index, value| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_edit, group_index) {
            return;
        }
        let st = ctx_edit.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        let Some(item) = items.get_mut(idx) else {
            return;
        };
        item.write_value = value;
        item.dirty = true;
        item.status = "未写入".into();
        write_group_items(&ctx_edit, group_index, slave_id, title, builtin, items);
        sync_active_to_ui(&ui, &ctx_edit);
    });

    let ui_weak = ui.as_weak();
    let ctx_write = ctx.clone();
    ui.on_write_device_config_item(move |index| {
        let ui = ui_weak.unwrap();
        if !ctx_write.ble.is_connected() {
            return;
        }
        let idx = index as usize;
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_write, group_index) {
            return;
        }
        let st = ctx_write.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id_text = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        let Some(item) = items.get_mut(idx) else {
            return;
        };

        let slave_id = slave_id_text
            .to_string()
            .trim()
            .parse::<u8>()
            .unwrap_or(0);
        let address = match parse_register_address(&item.register.to_string()) {
            Ok(a) => a,
            Err(err) => {
                item.status = err.into();
                write_group_items(&ctx_write, group_index, slave_id_text, title, builtin, items);
                sync_active_to_ui(&ui, &ctx_write);
                return;
            }
        };
        let value_type = parse_value_type(&item.value_type.to_string());
        let values = match encode_write_value(
            &item.write_value.to_string(),
            value_type,
            item.register_count.max(1) as u16,
        ) {
            Ok(v) => v,
            Err(err) => {
                item.status = err.into();
                write_group_items(&ctx_write, group_index, slave_id_text, title, builtin, items);
                sync_active_to_ui(&ui, &ctx_write);
                return;
            }
        };

        item.dirty = false;
        item.status = "写入中…".into();
        write_group_items(
            &ctx_write,
            group_index,
            slave_id_text,
            title,
            builtin,
            items,
        );
        sync_active_to_ui(&ui, &ctx_write);
        ctx_write
            .ble
            .write_holding(slave_id, address, values, None, None);
    });

    let ui_weak = ui.as_weak();
    let ctx_rm_item = ctx.clone();
    ui.on_remove_device_config_item(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let group_index = ui.get_active_config_group() as usize;
        if is_builtin_group(&ctx_rm_item, group_index) {
            return;
        }
        let st = ctx_rm_item.state.borrow();
        let Some(group) = st.device_config.groups.row_data(group_index) else {
            return;
        };
        let mut items = items_vec(&group);
        let title = group.title.clone();
        let slave_id = group.slave_id.clone();
        let builtin = group.builtin;
        drop(st);
        if idx < items.len() {
            if ui.get_editing_config_index() == index {
                clear_config_form(&ui);
            }
            items.remove(idx);
            write_group_items(&ctx_rm_item, group_index, slave_id, title, builtin, items);
            sync_active_to_ui(&ui, &ctx_rm_item);
            touch_poll_policy(&ui, &ctx_rm_item);
            persist_device_config(&ctx_rm_item, &ui);
        }
    });

    let ctx_builtin_edit = ctx.clone();
    ui.on_builtin_config_value_edited(move |index, value| {
        let idx = index as usize;
        let st = ctx_builtin_edit.state.borrow();
        let Some(mut item) = st.device_config.builtin_items.row_data(idx) else {
            return;
        };
        item.write_value = value;
        item.dirty = true;
        item.status = "未写入".into();
        if let Some(def) = BUILTIN_SETTINGS.get(idx) {
            refresh_builtin_enum_index(&mut item, def.enum_options);
        }
        st.device_config.builtin_items.set_row_data(idx, item);
        drop(st);
        apply_builtin_availability_in_place(&ctx_builtin_edit);
    });

    let ctx_builtin_write = ctx.clone();
    ui.on_write_builtin_config_item(move |index| {
        if !ctx_builtin_write.ble.is_connected() {
            return;
        }
        let idx = index as usize;
        let Some(def) = BUILTIN_SETTINGS.get(idx) else {
            return;
        };
        let st = ctx_builtin_write.state.borrow();
        let Some(mut item) = st.device_config.builtin_items.row_data(idx) else {
            return;
        };
        let write_text = item.write_value.to_string();
        if def.id == "device_bind" {
            let snap = ctx_builtin_write.ble.snapshot();
            if bind_option_standalone_only(&write_text)
                && is_parallel_ha1_device(&snap.device_name)
            {
                item.status = "并机(HA1)设备不支持设备绑定".into();
                st.device_config.builtin_items.set_row_data(idx, item);
                return;
            }
            let Some(field) = bind_trigger_field(&write_text) else {
                item.status = "未知绑定选项".into();
                st.device_config.builtin_items.set_row_data(idx, item);
                return;
            };
            item.dirty = false;
            item.status = "写入中…".into();
            st.device_config.builtin_items.set_row_data(idx, item);
            drop(st);
            ctx_builtin_write.ble.write_holding(
                BUILTIN_CONFIG_SLAVE_ID,
                def.register,
                vec![field.value],
                None,
                Some(field),
            );
            return;
        }
        let values = match def.widget {
            BuiltinWidget::Switch => {
                vec![if parse_switch_on(&write_text) { 1 } else { 0 }]
            }
            BuiltinWidget::Enum | BuiltinWidget::Number => {
                match encode_write_value(&write_text, def.value_type, def.register_count) {
                    Ok(v) => v,
                    Err(err) => {
                        item.status = err.into();
                        st.device_config.builtin_items.set_row_data(idx, item);
                        return;
                    }
                }
            }
        };
        item.dirty = false;
        item.status = "写入中…".into();
        st.device_config.builtin_items.set_row_data(idx, item);
        drop(st);

        ctx_builtin_write.ble.write_holding(
            BUILTIN_CONFIG_SLAVE_ID,
            def.register,
            values,
            def.bit,
            def.field,
        );
    });

    let ui_weak = ui.as_weak();
    let ctx_wifi = ctx.clone();
    ui.on_wifi_apply(move || {
        let ui = ui_weak.unwrap();
        apply_wifi_credentials(&ui, &ctx_wifi);
    });

    let ui_weak = ui.as_weak();
    ui.on_wifi_scan(move || {
        let ui = ui_weak.unwrap();
        if ui.get_wifi_scan_busy() {
            return;
        }
        // macOS 定位弹窗必须在主线程请求，且进程需经 codesign 绑定 Info.plist。
        wifi_scan::prepare_platform_access();
        ui.set_wifi_picker_kind(1);
        ui.set_wifi_scan_busy(true);
        ui.set_wifi_scan_message("正在扫描附近 WiFi…".into());
        let ui_weak_done = ui.as_weak();
        std::thread::spawn(move || {
            let result = wifi_scan::scan_nearby();
            let _ = slint::invoke_from_event_loop(move || {
                let Some(ui) = ui_weak_done.upgrade() else {
                    return;
                };
                ui.set_wifi_scan_busy(false);
                match result {
                    Ok(aps) => {
                        let rows: Vec<WifiScanAp> = aps
                            .into_iter()
                            .map(|ap| WifiScanAp {
                                ssid: ap.ssid.into(),
                                signal: ap.signal.into(),
                                band: ap.band.into(),
                            })
                            .collect();
                        let empty = rows.is_empty();
                        let model = ui.get_wifi_scan_aps();
                        if let Some(vec) = model.as_any().downcast_ref::<VecModel<WifiScanAp>>() {
                            vec.set_vec(rows);
                        } else {
                            ui.set_wifi_scan_aps(ModelRc::new(VecModel::from(rows)));
                        }
                        ui.set_wifi_scan_message(
                            if empty {
                                "未发现附近 2.4GHz WiFi，请确认本机 WLAN 已打开".into()
                            } else {
                                "".into()
                            },
                        );
                    }
                    Err(err) => {
                        ui.set_wifi_scan_message(err.into());
                    }
                }
            });
        });
    });

    let ui_weak = ui.as_weak();
    let ctx_pick_scan = ctx.clone();
    ui.on_wifi_select_scan_ap(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let Some(ap) = ui.get_wifi_scan_aps().row_data(idx) else {
            return;
        };
        let ssid = ap.ssid.clone();
        let saved = ctx_pick_scan.state.borrow().device_config.wifi.saved.clone();
        let saved_pwd = (0..saved.row_count())
            .filter_map(|i| saved.row_data(i))
            .find(|n| n.ssid == ssid)
            .map(|n| n.password);
        ui.set_wifi_ssid(ssid);
        if let Some(pwd) = saved_pwd {
            ui.set_wifi_password(pwd);
        }
        ui.set_wifi_picker_kind(0);
    });

    let ui_weak = ui.as_weak();
    let ctx_pick_saved = ctx.clone();
    ui.on_wifi_select_saved(move |index| {
        let ui = ui_weak.unwrap();
        let idx = index as usize;
        let Some(net) = ctx_pick_saved
            .state
            .borrow()
            .device_config
            .wifi
            .saved
            .row_data(idx)
        else {
            return;
        };
        ui.set_wifi_ssid(net.ssid);
        ui.set_wifi_password(net.password);
        ui.set_wifi_picker_kind(0);
    });

    let ctx_rm_wifi = ctx.clone();
    ui.on_wifi_remove_saved(move |index| {
        let idx = index as usize;
        let saved = ctx_rm_wifi.state.borrow().device_config.wifi.saved.clone();
        if idx < saved.row_count() {
            saved.remove(idx);
            persist_wifi_networks(&ctx_rm_wifi);
        }
    });
}
