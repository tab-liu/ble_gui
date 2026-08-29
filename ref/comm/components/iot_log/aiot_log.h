#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "circular_queue.h"

#pragma pack(1)

/**
 * 协议分类, 对应于LOG TLV协议中的报文分类字段, 用于区分不同协议的日志数据
 */
typedef enum
{
    LOG_TYPE_BLE_TX = 101,
    LOG_TYPE_BLE_RX = 131,
    LOG_TYPE_BLE_MIX = 161,
    LOG_TYPE_MQTT1_TX = 102,
    LOG_TYPE_MQTT1_RX = 132,
    LOG_TYPE_MQTT1_MIX = 162,
    LOG_TYPE_MQTT2_TX = 103,
    LOG_TYPE_MQTT2_RX = 133,
    LOG_TYPE_MQTT2_MIX = 163,
    LOG_TYPE_CAN_TX = 104,
    LOG_TYPE_CAN_RX = 134,
    LOG_TYPE_CAN_MIX = 164,
    LOG_TYPE_MODBUS_TCP_TX = 105,
    LOG_TYPE_MODBUS_TCP_RX = 135,
    LOG_TYPE_MODBUS_TCP_MIX = 165,
    LOG_TYPE_LOG = 201,
    LOG_TYPE_OTHER = 200, // 混合类型，包含多个协议的日志
} log_type_t;

typedef union
{
    uint32_t value;
    struct {
        uint32_t identifier : 29;   // 实际的CAN ID
        uint32_t revd : 2;          // 保留
        uint32_t tx : 1;            // 是否为发送帧
    };
}can_id_frame_t;

typedef struct
{
    can_id_frame_t id;
    uint8_t data[8];
} aiot_log_can_frame_t;

typedef struct 
{
    uint8_t type;
    uint16_t len;
    uint32_t timestamp;
    uint8_t data[0]; // 可变长度数据
} log_frame_t;

typedef struct
{
    CircularQueue log_queue;
    log_type_t type;   // 报文类型分类
} aiot_log_t;

#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif

bool aiot_log_init(void);
uint16_t aiot_log_push(const uint8_t *data, uint16_t len, log_type_t type);

#ifdef __cplusplus
}
#endif
