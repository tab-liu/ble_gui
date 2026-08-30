//! BLE 链路上的 Modbus RTU 辅助。

mod dashboard;
mod device_config_builtin;
mod query;
mod rtu;
mod tlv;

pub use device_config_builtin::{
    BuiltinWidget, BUILTIN_SETTINGS, DEFAULT_SLAVE_ID as BUILTIN_CONFIG_SLAVE_ID,
};

pub use dashboard::{
    merge_control_states, parse_dashboard_registers, DEFAULT_SLAVE_ID, MODBUS_TIMEOUT_MS,
    POLL_INTERVAL_MS, REG_AC_OUTPUT, REG_DASHBOARD_COUNT, REG_DASHBOARD_START, REG_DC_OUTPUT,
};
pub use query::{
    encode_write_value, format_query_value, parse_register_address, parse_register_count,
    parse_scale, parse_value_type, value_type_from_index, QueryValueType,
};
pub use rtu::{
    build_read_holding, build_write_single, parse_read_holding, plain_modbus_frame_length,
};
pub use tlv::{
    build_tlv_read_request, build_write_multiple, chunk_tl_batches,
    iot_status_supports_tlv, is_fc10_write_ack, parse_tlv_read_units, parse_tlv_response_packet,
    describe_tlv_units, tlv_register_values, tlv_batch_start_index, tlv_item_batch_index,
    TlReadSpec, TlvPacketCollector, TlvReadResult, REG_IOT_STATUS, REG_21000,
};
