//! BLE 链路上的 Modbus RTU 辅助（帧构建、查询格式化、TLV、仪表解析）。
//!
//! | 子模块 | 内容 |
//! |--------|------|
//! | [`rtu`] | CRC、读/写保持寄存器帧、响应解析 |
//! | [`query`] | 地址记法、整数/浮点/字符串格式化与写入编码 |
//! | [`tlv`] | 组合读（写 21000）批量 TL |
//! | [`sub_devices`] | 21000 段组网配件列表 |
//! | [`dashboard`] | 主页 SOC@102 / 功率@140～147 / 2011～2012 |
//! | [`device_info`] | 1100～1130 机型 / SN / 软件版本 |
//! | [`device_config_builtin`] | 设备配置「常用」静态寄存器表 |
//! | [`wifi_provision`] | WiFi 配网寄存器与链路状态 |

mod dashboard;
mod device_config_builtin;
mod device_info;
mod query;
mod rtu;
mod sub_devices;
mod tlv;
mod wifi_provision;

pub use device_config_builtin::{
    bind_option_standalone_only, bind_trigger_field, builtin_bind_supported, enum_index_for_value,
    is_parallel_ha1_device, BuiltinSettingDef, BuiltinWidget, RegisterFieldPatch, BUILTIN_SETTINGS,
    DEFAULT_SLAVE_ID as BUILTIN_CONFIG_SLAVE_ID, IOT_CLOUD_DNS_REGISTER,
    IOT_CLOUD_DNS_REGISTER_COUNT,
};

pub use dashboard::{
    merge_control_states, parse_dashboard_soc_and_power, DEFAULT_SLAVE_ID, MODBUS_TIMEOUT_MS,
    POLL_INTERVAL_MS, REG_AC_OUTPUT, REG_DC_OUTPUT, REG_POWER_COUNT, REG_POWER_START, REG_SOC,
    REG_SOC_COUNT,
};
pub use device_info::{
    format_regs_hex, parse_ascii_regs, parse_device_info, parse_iot_identity, parse_iot_software_ver,
    parse_iot_type, parse_whole_device, REG_DEVICE_INFO_COUNT, REG_DEVICE_INFO_START,
    REG_IOT_IDENTITY_COUNT, REG_IOT_INFO_COUNT, REG_IOT_INFO_START, REG_WHOLE_DEVICE_COUNT,
    REG_WHOLE_DEVICE_START,
};
pub use query::{
    encode_write_value, format_query_value, integer_debug_hex, parse_register_address,
    parse_register_count, parse_scale, parse_value_type, value_type_from_index, QueryValueType,
};
pub use wifi_provision::{
    disconnect_reason_text, is_wifi_poll_index, parse_disconnect_reason, parse_link_status,
    parse_link_status_regs, parse_sta_ipv4, parse_sta_ipv4_regs, parse_sta_rssi, rssi_quality_text,
    sta_enable_word, wifi_auth_for_password, REG_LINK_STATUS, REG_LINK_STATUS_BLOCK_COUNT,
    REG_LINK_STATUS_COUNT, REG_STA_IPV4, REG_STA_IPV4_COUNT, REG_STA_RSSI, REG_WIFI_DISCONNECT,
    REG_WIFI_ON_OFF, REG_WIFI_SSID_NOW, REG_WIFI_SSID_NOW_COUNT, REG_WIFI_STA_AUTH,
    REG_WIFI_STA_ENABLE, REG_WIFI_STA_PASSWORD, REG_WIFI_STA_PASSWORD_COUNT, REG_WIFI_STA_SSID,
    REG_WIFI_STA_SSID_COUNT, WIFI_PASSWORD_MAX_BYTES, WIFI_POLL_DISCONNECT, WIFI_POLL_LINK,
    WIFI_POLL_SSID_NOW, WIFI_POLL_STA_IP, WIFI_SSID_MAX_BYTES,
};
pub use rtu::{
    build_read_holding, build_write_single, parse_read_holding, plain_modbus_frame_length,
};
pub use sub_devices::{apply_sub_device_report, parse_reg21000_report_frame, sn_type_name};
pub use tlv::{
    build_tlv_read_request, build_write_multiple, chunk_tl_batches,
    iot_status_supports_tlv, is_fc10_write_ack, parse_tlv_read_units, parse_tlv_response_packet,
    describe_tlv_units, tlv_register_values, tlv_batch_start_index, tlv_item_batch_index,
    TlReadSpec, TlvPacketCollector, TlvReadResult, REG_IOT_STATUS, REG_21000,
};
