//! BLE 链路上的 Modbus RTU 辅助。

mod dashboard;
mod query;
mod rtu;

pub use dashboard::{
    merge_control_states, parse_dashboard_registers, DEFAULT_SLAVE_ID, MODBUS_TIMEOUT_MS,
    POLL_INTERVAL_MS, REG_AC_OUTPUT, REG_DASHBOARD_COUNT, REG_DASHBOARD_START, REG_DC_OUTPUT,
};
pub use query::{
    encode_string_to_registers, format_query_value, format_register_value, parse_register_address, parse_register_count,
    parse_scale, parse_value_type, value_type_from_index, QueryValueType,
};
pub use rtu::{
    append_crc, build_read_holding, build_write_single, parse_read_holding, plain_modbus_frame_length,
    verify_crc,
};
