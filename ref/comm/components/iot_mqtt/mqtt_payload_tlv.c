/**
  ******************************************************************************
  * @file      mqtt_payload_tlv.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/1/14
  * @brief     MQTT载荷中传输TLV报文格式
  *            参考文档https://192.168.102.10/svn/TecCenter/文件中心/技术资料/德兰明海总线协议/物联网载荷协议/IoT物联网载荷协议 - TLV.xlsx
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/14  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */
 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "esp_system.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "comm_define.h"
#include "mqtt_payload_tlv.h"
#include "iot_mqtt.h"
#include "sync_time.h"

#include "utils.h"
#include "crc.h"

#ifdef CONFIG_MQTT_PAYLOAD_TLV_ENABLE

/* ================================ 文件内宏定义 ================================ */

#define TAG "[MQTT_TLV]"

// 高低字节转换
#define U16_TO_BE(val)   (uint16_t)((((val) & 0xFF00) >> 8) | (((val) & 0x00FF) << 8))

/* =============================== 文件内全局变量 ================================ */

// 定义队列句柄
static QueueHandle_t mqtt_tlv_queue = NULL;

// 请求ID
static uint16_t time_check_request_id = 0;
static uint16_t device_cfg_get_id = 0;

/* ============================== 文件内引用外部接口 ============================= */

// 解析DST时间并直接更新到本地
extern bool iCloud_Dst_Time_Check_and_Update(const char *datetime_str, uint16_t str_len);

// 设置MQTT心跳
extern void set_mqtt_heartbeat(uint16_t heartbeat);

// 云边同步参数处理接口
extern const field_handler_t device_cfg_set_field_handler[];

/* ================================ 模块函数定义 ================================ */

// 主机字节序 -> 网络字节序
static inline uint16_t htons_host(uint16_t val) {
    return htons(val);
}

static inline uint32_t htonl_host(uint32_t val) {
    return htonl(val);
}

static inline int32_t htonl_int32_host(int32_t val) {
    return htonl((uint32_t)val);
}

// 手动实现 htoll
static inline uint64_t htoll_host(uint64_t val) {
    return ((uint64_t)htonl((uint32_t)(val >> 32)) << 32) |
           htonl((uint32_t)(val & 0xFFFFFFFF));
}

static inline int64_t htoll_int64_host(int64_t val) {
    return (int64_t)htoll_host((uint64_t)val);
}

/* =============================================================================== */

/**
  * @brief      MQTT TLV初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void mqtt_tlv_init(void)
{
    // 队列长度和每个元素大小
    const uint32_t queue_len = 16;
    mqtt_tlv_queue = xQueueCreate(queue_len, sizeof(mqtt_tlv_report_data_t));
    if (mqtt_tlv_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create mqtt_tlv_queue");
    } else {
        ESP_LOGI(TAG, "mqtt_tlv_queue created successfully");
    }
}

/**
 * @brief MQTT TLV载荷协议报文检查
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return 成功返回0, 否则返回-1
 */
static int mqtt_tlv_payload_check(uint8_t *data, int size)
{
    // 空指针和长度检查
    if (data == NULL || size <= 0) {
        ESP_LOGE(TAG, "[mqtt_tlv_payload_check] null or invalid size: %p, %d", data, size);
        return -1;
    }

    if (size <= sizeof(mqtt_tlv_payload_t) + 2) {
        ESP_LOGE(TAG, "[mqtt_tlv_payload_check] size too small: %d", size);
        return -1;
    }

    mqtt_tlv_payload_t *pkt = (mqtt_tlv_payload_t *)data;

    // 检查协议头部字段
    if ((pkt->frame_type != MQTT_TLV_MARK) 
        || ((pkt->protocol_ver != MQTT_TLV_TAG_FLAG) && (pkt->protocol_ver != MQTT_TLV_FIELD_FLAG))) {
        ESP_LOGE(TAG, "[mqtt_tlv_payload_check] header error, frame_type(0x%02X), protocol_ver(0x%02X)", pkt->frame_type, pkt->protocol_ver);
        return -1;
    }

    // 检查载荷长度
    uint16_t payload_len = U16_TO_BE(pkt->payload_len);
    if (payload_len + 2 != size) { // 4字节头部
        ESP_LOGE(TAG, "[mqtt_tlv_payload_check] payload_len error, payload_len(%d), size(%d)", payload_len, size);
        return -1;
    }

    // CRC校验
    uint16_t crc_old = (data[size-1] << 8) | data[size-2];
    uint16_t crc_now = crc16_modbus_calc(data, size-2);
    if (crc_old != crc_now) {
        ESP_LOGE(TAG, "[mqtt_tlv_payload_check] crc error, size(%d)", size);
        return -1;
    }

    return 0;
}

// 根据 tlv_type_t.encode_type 进行相应转换
static int serialize_value_by_type(const tlv_type_t *tlv_tag, 
                                   const void *value_data, 
                                   uint16_t value_len,
                                   uint8_t *out_buf, 
                                   uint16_t out_buf_size)
{
    if (!tlv_tag || !value_data || !out_buf || !out_buf_size) {
        return -1;
    }
    
    // 先根据 data_type 区分原始类型和变长类型
    if (tlv_tag->data_type == 1) {
        // ========== 变长类型 ==========
        switch (tlv_tag->encode_type) {
            case TLV_ENCODE_ASCII_STRING:
            case TLV_ENCODE_UTF8_STRING:
            case TLV_ENCODE_HEX_STRING:
                if (value_len > out_buf_size) return -2;
                memcpy(out_buf, value_data, value_len);
                return value_len;
                
            default:
                ESP_LOGE(TAG, "serialize_value_by_type: Unknown var encode type(%d)", 
                         tlv_tag->encode_type);
                return -3;
        }
    } else {
        // ========== 原始类型 ==========
        switch (tlv_tag->encode_type) {
            case 0x01:  // TLV_ENCODE_BOOL (data_type=0)
                if (value_len != 1 || out_buf_size < 1) return -2;
                out_buf[0] = *(bool *)value_data;
                return 1;
                
            case TLV_ENCODE_TINY:
                if (value_len != 1 || out_buf_size < 1) return -2;
                out_buf[0] = *(int8_t *)value_data;
                return 1;
                
            case TLV_ENCODE_UTINY:
                if (value_len != 1 || out_buf_size < 1) return -2;
                out_buf[0] = *(uint8_t *)value_data;
                return 1;
                
            case TLV_ENCODE_SHORT:
                if (value_len != 2 || out_buf_size < 2) return -2;
                *(int16_t *)out_buf = htons_host(*(int16_t *)value_data);
                return 2;
                
            case TLV_ENCODE_USHORT:
                if (value_len != 2 || out_buf_size < 2) return -2;
                *(uint16_t *)out_buf = htons_host(*(uint16_t *)value_data);
                return 2;
                
            case TLV_ENCODE_INT:
                if (value_len != 4 || out_buf_size < 4) return -2;
                *(int32_t *)out_buf = htonl_int32_host(*(int32_t *)value_data);
                return 4;
                
            case TLV_ENCODE_UINT:
                if (value_len != 4 || out_buf_size < 4) return -2;
                *(uint32_t *)out_buf = htonl_host(*(uint32_t *)value_data);
                return 4;
                
            case TLV_ENCODE_LONG:
                if (value_len != 8 || out_buf_size < 8) return -2;
                *(int64_t *)out_buf = htoll_int64_host(*(int64_t *)value_data);
                return 8;
                
            case TLV_ENCODE_ULONG:
                if (value_len != 8 || out_buf_size < 8) return -2;
                *(uint64_t *)out_buf = htoll_host(*(uint64_t *)value_data);
                return 8;
                
            case TLV_ENCODE_FLOAT:
                if (value_len != 4 || out_buf_size < 4) return -2;
                {
                    uint32_t tmp = htonl_host(*(uint32_t *)value_data);
                    float val;
                    memcpy(&val, &tmp, sizeof(val));
                    *(float *)out_buf = val;
                }
                return 4;
                
            case TLV_ENCODE_DOUBLE:
                if (value_len != 8 || out_buf_size < 8) return -2;
                {
                    uint64_t tmp = htoll_host(*(uint64_t *)value_data);
                    double val;
                    memcpy(&val, &tmp, sizeof(val));
                    *(double *)out_buf = val;
                }
                return 8;
                
            default:
                ESP_LOGE(TAG, "serialize_value_by_type: Unknown primitive encode type(%d)", 
                         tlv_tag->encode_type);
                return -3;
        }
    }
}

/**
 * @brief MQTT TLV协议的TAG字段组包
 * @param [in] field TLV的field字段数据
 * @param [in] field_len field字段数据长度
 * @param [in] value TLV的Value字段数据
 * @param [in] value_len Value字段数据长度
 * @param [out] outData 输出数据
 * @return 成功返回输出数据长度; 否则返回 < 0
 */
static int16_t mqtt_tlfv_frame_serialize(mqtt_tlfv_frame_t *frame, uint8_t *outData, uint16_t outDataSize)
{
    if (!frame || !outData || !outDataSize)
    {
        ESP_LOGE(TAG, "mqtt_tlfv_frame_serialize : parameter error");
        return -1;
    }

    if (outDataSize < frame->field_len + frame->value_len + 4)
    {
        ESP_LOGE(TAG, "mqtt_tlfv_frame_serialize : buffer len:%d < %d", outDataSize, frame->field_len + frame->value_len + 4);
        return -2;
    }

    // 单个TLV对象参数
    uint16_t len = 0;  // 数据区使用长度
    uint16_t tlv_len = 0;

    // T
    outData[len++] = frame->type.value;
    outData[len++] = frame->field_len;

    // L
    tlv_len = htons(frame->field_len + frame->value_len + 4);
    memcpy(&outData[len], &tlv_len, 2);
    len += 2;

    // F
    memcpy(&outData[len], frame->field, frame->field_len);
    len += frame->field_len;

    // V - 根据 encode_type 进行网络字节序转换
    int16_t result = serialize_value_by_type(&frame->type, 
                                              frame->value, 
                                              frame->value_len,
                                              &outData[len], 
                                              outDataSize - len);
    if (result < 0) {
        ESP_LOGE(TAG, "mqtt_tlfv_frame_serialize: serialize value failed");
        return result;
    }
    len += result;

    return len;
}

/* ====================================================================================================== */

/**
 * @brief 通用对象解析
 */
static bool field_common_handle(const char *field, const tlv_type_t *tlv_tag, const void *value_data, uint16_t value_len)
{
    /*调试功能，出厂版本不开放*/
#ifndef MASS_PRODUCTION_CONFIG_ENABLE
    if(field) ESP_LOGI(TAG, "field_common_handle : Field: %s", field);
    if ( 1 == tlv_tag->data_type )
    {
        // 变长类型
        switch ( tlv_tag->encode_type )
        {
            case TLV_ENCODE_ASCII_STRING :
                ESP_LOGI(TAG, "field_common_handle : ASCII(%.*s)", value_len, (char *)value_data);
                break;
            case TLV_ENCODE_UTF8_STRING :
                ESP_LOGI(TAG, "field_common_handle : UTF8(%.*s)", value_len, (char *)value_data);
                break;
            case TLV_ENCODE_HEX_STRING :
                ESP_LOGI(TAG, "field_common_handle : HEX");
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, (uint8_t *)value_data, value_len, ESP_LOG_INFO);
                break;
            default:
                ESP_LOGE(TAG, "field_common_handle : Unknown encode type(%d)", tlv_tag->encode_type);
                break;
        }
    }
    else
    {
        // 原始类型
        switch (tlv_tag->encode_type)
        {
            case TLV_ENCODE_BOOL:
                ESP_LOGI(TAG, "field_common_handle : BOOL(%d)",
                         *(bool *)value_data);
                break;
            case TLV_ENCODE_TINY:       // int8_t
                ESP_LOGI(TAG, "field_common_handle : TINY(%d)",
                         *(int8_t *)value_data);
                break;
            case TLV_ENCODE_UTINY:      // uint8_t
                ESP_LOGI(TAG, "field_common_handle : UTINY(%u)",
                         *(uint8_t *)value_data);
                break;
            case TLV_ENCODE_SHORT:      // int16_t
                ESP_LOGI(TAG, "field_common_handle : SHORT(%d)",
                         ntohs(*(int16_t *)value_data));
                break;
            case TLV_ENCODE_USHORT:     // uint16_t
                ESP_LOGI(TAG, "field_common_handle : USHORT(%u)",
                         ntohs(*(uint16_t *)value_data));
                break;
            case TLV_ENCODE_INT:        // int32_t
                ESP_LOGI(TAG, "field_common_handle : INT(%" PRId32 ")",
                         ntohl(*(int32_t *)value_data));
                break;
            case TLV_ENCODE_UINT:       // uint32_t
                ESP_LOGI(TAG, "field_common_handle : UINT(%" PRIu32 ")",
                         ntohl(*(uint32_t *)value_data));
                break;
            case TLV_ENCODE_LONG:       // int64_t
                uint64_t val_long;
                memcpy(&val_long, value_data, sizeof(val_long));
                // ESP-IDF 没有 ntohll，手动实现
                val_long = ((uint64_t)ntohl((uint32_t)(val_long & 0xFFFFFFFF)) << 32) |
                      ntohl((uint32_t)(val_long >> 32));
                ESP_LOGI(TAG, "field_common_handle : LONG(%" PRId64 ")", (int64_t)val_long);
                break;
            case TLV_ENCODE_ULONG:      // uint64_t
                uint64_t val_ulong;
                memcpy(&val_ulong, value_data, sizeof(val_ulong));
                // ESP-IDF 没有 ntohll，手动实现
                val_ulong = ((uint64_t)ntohl((uint32_t)(val_ulong & 0xFFFFFFFF)) << 32) |
                      ntohl((uint32_t)(val_ulong >> 32));
                ESP_LOGI(TAG, "field_common_handle : ULONG(%" PRIu64 ")", val_ulong);
                break;
            case TLV_ENCODE_FLOAT:
                uint32_t tmp_float;
                float val_float;
                memcpy(&tmp_float, value_data, sizeof(tmp_float));
                tmp_float = ntohl(tmp_float);
                memcpy(&val_float, &tmp_float, sizeof(val_float));
                ESP_LOGI(TAG, "field_common_handle : FLOAT(%f)", val_float);
                break;
            case TLV_ENCODE_DOUBLE:
                uint64_t tmp_double;
                double val_double;
                memcpy(&tmp_double, value_data, sizeof(tmp_double));
                tmp_double = ((uint64_t)ntohl((uint32_t)(tmp_double & 0xFFFFFFFF)) << 32) |
                      ntohl((uint32_t)(tmp_double >> 32));
                memcpy(&val_double, &tmp_double, sizeof(val_double));
                ESP_LOGI(TAG, "field_common_handle : DOUBLE(%lf)", val_double);
                break;
            default:
                ESP_LOGE(TAG, "field_common_handle : Unknown encode type(%d)",
                         tlv_tag->encode_type);
                break;
        }
    }
#endif

    return false;
}

/* ====================================================================================================== */

/**
 * @brief 解析Field对象“heartbeat”
 */
static bool field_heartbeat_handle(const char *field, const tlv_type_t *tlv_tag, const void *value_data, uint16_t value_len)
{
    if (( 0 == tlv_tag->data_type ) 
        && ( TLV_LOGIC_EQ == tlv_tag->logic_type )
        && ( TLV_ENCODE_UINT == tlv_tag->encode_type ))
    {
        uint32_t heartbeat = ntohl(*(uint32_t *)value_data);
        ESP_LOGI(TAG, "field_heartbeat_handle : UINT(%" PRIu32 ")", heartbeat);
        if ( heartbeat <= 0xFFFF ) {
            set_mqtt_heartbeat((uint16_t)heartbeat);
            return true;
        }
    }

    return false;
}

// 通用设置表（无需响应）
static const field_handler_t void_set_field_handler[] = {
    {"heartbeat",   field_heartbeat_handle},
    {NULL,          NULL}  // 结束标记
};

/* ====================================================================================================== */

/**
 * @brief 解析Field对象“dst”
 */
static bool field_dst_handle(const char *field, const tlv_type_t *tlv_tag, const void *value_data, uint16_t value_len)
{
    if (( 1 == tlv_tag->data_type ) 
        && ( TLV_LOGIC_EQ == tlv_tag->logic_type )
        && ( TLV_ENCODE_ASCII_STRING == tlv_tag->encode_type ))
    {
        ESP_LOGI(TAG, "field_dst_handle : ASCII(%.*s)", value_len, (char *)value_data);
        return iCloud_Dst_Time_Check_and_Update((char *)value_data, value_len);
    }

    return false;
}

// 请求服务器域名及时间
static const field_handler_t server_and_dst_field_handler[] = {
    {"dst",         field_dst_handle},
    {"tzId",        field_common_handle},
    {"time",        field_common_handle},
    {"server",      field_common_handle},
    {"tzOffset",    field_common_handle},
    {NULL,          NULL}  // 结束标记
};

/* ====================================================================================================== */

// 请求解析表格
static const field_request_t field_request_table[] = {
    {&time_check_request_id,    server_and_dst_field_handler},
#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE    
    {&device_cfg_get_id,        device_cfg_set_field_handler},
#endif    
    {NULL,                      NULL}  // 结束标记
};

/* ====================================================================================================== */

/**
 * @brief MQTT TLV协议field查表处理
 *
 * @param data 数据指针
 * @param size 数据大小
 * @param field_handler_table 数据大小
 * @return 处理结果，0成功，-1失败
 */
static int mqtt_tlv_field_unit_process(uint8_t *data, int size, field_handler_t * field_handler_table)
{
    bool status = false;

    if ( NULL == data || !size || NULL == field_handler_table) {
        ESP_LOGE(TAG, "mqtt_tlv_field_unit_process: null input");
        return -1;
    }

    // TLV数据区起始指针
    uint8_t *tlv_data = data;
    uint16_t tlv_data_len = size;  // 数据区使用长度
    
    // 单个TLV对象参数
    tlv_type_t tlv_tag = {0};
    uint8_t field_len = 0;
    uint16_t value_len = 0;
    uint16_t tlv_len = 0;

    // 逐个解析TLFV块
    while ( tlv_data_len >= 2 ) {
        tlv_tag.value = tlv_data[0];
        field_len = tlv_data[1];
        tlv_len = ntohs(*((uint16_t *)&tlv_data[2]));
        value_len = tlv_len - field_len - 4;

        // 检查剩余数据长度
        if (tlv_data_len < tlv_len) {
            ESP_LOGE(TAG, "mqtt_tlv_field_unit_process: tlv_data_len(%d) error", tlv_data_len);
            break;
        }

        // 查找对应Field，执行对应回调
        bool founded = false;
        for (uint8_t i = 0; field_handler_table[i].key != NULL; i++) {
            if (strncmp((char *)&tlv_data[4], field_handler_table[i].key, field_len) == 0) {
                founded = true;
                if ( field_handler_table[i].handler ) {
                    status |= field_handler_table[i].handler(field_handler_table[i].key, &tlv_tag, &tlv_data[4 + field_len], value_len);
                }
                break;
            }
        }

        // 未知Field
        if(!founded) ESP_LOGE(TAG, "mqtt_tlv_field_unit_process : Unknown Field: %.*s", field_len, (char *)&tlv_data[4]);

        // 跳转到下一个TLV位置
        tlv_data = &tlv_data[tlv_len];
        tlv_data_len -= tlv_len;
    }

    return (status == true) ? 0 : -1;
}

/**
 * @brief MQTT TLV协议field接口SET处理函数
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return 处理结果，0成功，-1失败
 */
static int mqtt_tlv_field_set_handle(uint8_t *data, int size)
{
    int ret = -1;

    // 解析协议头
    mqtt_tlv_payload_t *pkt = (mqtt_tlv_payload_t *)data;

    // TLV数据区起始指针
    uint8_t *tlv_data = (uint8_t *)(data + sizeof(mqtt_tlv_payload_t));
    uint16_t tlv_data_len = (size - sizeof(mqtt_tlv_payload_t) - 2);  // 数据区使用长度
    
    // 单个TLV对象参数
    tlv_type_t tlv_tag = {0};
    uint8_t field_len = 0;
    uint16_t value_len = 0;
    uint16_t tlv_len = 0;

    // 根据数据/业务操作类型分发处理
    switch ( pkt->flag.op_type )
    {
        case MQTT_TLV_OP_SET :
            // 根据数据/业务交互的响应类型分发处理
            switch ( pkt->flag.resp_type )
            {
                case MQTT_TLV_RESP_VOID :
                    ret = mqtt_tlv_field_unit_process(tlv_data, tlv_data_len, void_set_field_handler);
                    break;
                    
                case MQTT_TLV_RESP_NONE  :   // 非法参数值
                case MQTT_TLV_RESP_ASYNC :   // 服务器设置设备后设备进行回复，暂未实现
                case MQTT_TLV_RESP_SYNC  :   // 不适用MQTT本身就是异步请求的场景，仅适用于点对点的TCP请求
                default:
                    ESP_LOGE(TAG, "mqtt_tlv_field_set_handle : Unsupported resp_type: %u", pkt->flag.resp_type);
                    break;
            }
            break;
            
        case MQTT_TLV_OP_NONE :     // 非法参数值
        case MQTT_TLV_OP_GET :      // 服务器请求获取设备数据，暂未实现
        case MQTT_TLV_OP_DELETE :   // 服务器请求删除设备数据，暂未实现
        default:
            ESP_LOGE(TAG, "mqtt_tlv_field_set_handle : Unsupported op_type: %u", pkt->flag.op_type);
            break;
    }

    return ret;
}

/**
 * @brief MQTT TLV协议field接口HTTP处理函数
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return 处理结果，0成功，-1失败
 */
static int mqtt_tlv_field_http_handle(uint8_t *data, int size)
{
    int ret = -1;

    // 解析协议头
    mqtt_tlv_payload_t *pkt = (mqtt_tlv_payload_t *)data;

    // TLV数据区起始指针
    uint8_t *tlv_data = (uint8_t *)(data + sizeof(mqtt_tlv_payload_t));
    uint16_t tlv_data_len = (size - sizeof(mqtt_tlv_payload_t) - 2);  // 数据区使用长度
    
    // 单个TLV对象参数
    tlv_type_t tlv_tag = {0};
    uint8_t field_len = 0;
    uint16_t value_len = 0;
    uint16_t tlv_len = 0;

    /* ========================================================= */

    // 检查@Code
    int code_data = -1;
    tlv_tag.value = tlv_data[0];
    field_len = tlv_data[1];
    tlv_len = ntohs(*((uint16_t *)&tlv_data[2]));
    value_len = tlv_len - field_len - 4;
    if (strncmp((char *)&tlv_data[4], "@code", field_len) == 0) {
        if (( 0 == tlv_tag.data_type ) 
            && ( TLV_ENCODE_INT == tlv_tag.encode_type ) 
            && ( TLV_LOGIC_EQ == tlv_tag.logic_type ))
        {
            code_data = ntohl(*((int *)&tlv_data[4 + field_len]));
            if ( 0 == code_data) {
                // @code=0时表示响应成功
            } else {
                // @code!=0时表示响应失败
                ESP_LOGE(TAG, "mqtt_tlv_field_http_handle : Code Error: %ld", code_data);
                tlv_data = &tlv_data[tlv_len];
                tlv_data_len -= tlv_len;
                if ( tlv_data_len  >= 2 ) {
                    tlv_tag.value = tlv_data[0];
                    field_len = tlv_data[1];
                    tlv_len = ntohs(*((uint16_t *)&tlv_data[2]));
                    value_len = tlv_len - field_len - 4;
                    if (strncmp((char *)&tlv_data[4], "@message", field_len) == 0) {
                        // HTTP接口响应错误提示消息
                        ESP_LOGE(TAG, "mqtt_tlv_field_http_handle : Message: %.*s", value_len, (char *)&tlv_data[4 + field_len]);
                    }
                }
                return -1;
            }
        } else {
            // 异常@code类型
            ESP_LOGE(TAG, "mqtt_tlv_field_http_handle : Invalid Type: %d", tlv_tag.value);
        }
    } else {
        // 非@code，违背协议标准
        ESP_LOGE(TAG, "mqtt_tlv_field_http_handle : Invalid Field: %.*s", field_len, (char *)&tlv_data[4]);
    }

    if ( 0 != code_data ) return -1;
    tlv_data = &tlv_data[tlv_len];
    tlv_data_len -= tlv_len;

    /* ========================================================= */

    // 根据数据/业务交互的响应类型分发处理
    switch ( pkt->flag.resp_type )
    {
        case MQTT_TLV_RESP_ASYNC :
            uint16_t request_id = pkt->request_id;
            ESP_LOGI(TAG, "mqtt_tlv_field_http_handle : request_id: %u", request_id);
            for (uint8_t i = 0; field_request_table[i].request_id != NULL; i++) {
                if (request_id == *field_request_table[i].request_id) {
                    ret = mqtt_tlv_field_unit_process(tlv_data, tlv_data_len, field_request_table[i].field_handler);
                    break;
                }
            }
            if ( 0 != ret ) ESP_LOGE(TAG, "mqtt_tlv_field_http_handle : MQTT_TLV_RESP_ASYNC ERROR");
            break;
            
        case MQTT_TLV_RESP_NONE :   // 非法参数值
        case MQTT_TLV_RESP_VOID :   // 本机只作为HTTP请求方，所接收必为HTTP响应
        case MQTT_TLV_RESP_SYNC :   // 不适用MQTT本身就是异步请求的场景，仅适用于点对点的TCP请求
        default:
            ESP_LOGE(TAG, "mqtt_tlv_field_http_handle : Unsupported resp_type: %u", pkt->flag.resp_type);
            break;
    }
    return ret;
}

/**
 * @brief MQTT TLV协议field接口处理函数
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return 处理结果，0成功，-1失败
 */
static int mqtt_tlv_field_protocol_handle(uint8_t *data, int size)
{
    int ret = -1;

    // 解析协议头
    mqtt_tlv_payload_t *pkt = (mqtt_tlv_payload_t *)data;

    // 根据协议功能分发处理
    switch (pkt->scene) {
        case MQTT_FUNC_PERIOD:
        case MQTT_FUNC_EVEVT:
            // TODO：处理逻辑待实现
            ESP_LOGE(TAG, "Unsupported scene: %u", pkt->scene);
            ret = 0;
            break;
            
        case MQTT_FUNC_SET_AND_RTN:
            /* 协议解析处理 */
            ret = mqtt_tlv_field_set_handle(data, size);
            break;
        
        case MQTT_FUNC_HTTP_REQUEST:
            /* 协议解析处理 */
            ret = mqtt_tlv_field_http_handle(data, size);
            break;   
        
        default:
            ESP_LOGE(TAG, "Unknown scene: %u", pkt->scene);
            return -1;
    }
    
    return ret;
}

/**
 * @brief MQTT TLV协议接口处理函数
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return 处理结果，0成功，-1失败
 */
int mqtt_tlv_protocol_handle(uint8_t *data, int size)
{
    int ret = -1;

    ESP_LOGD(TAG, "mqtt_tlv_protocol_handle (Len: %d)", size);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, size, ESP_LOG_DEBUG);    
    
    // 报文检查
    if (mqtt_tlv_payload_check(data, size) != 0) {
        ESP_LOGE(TAG, "[mqtt_tlv_protocol_handle] payload check failed");
        return -1;
    }

    // 解析协议头
    mqtt_tlv_payload_t *pkt = (mqtt_tlv_payload_t *)data;

    // 判断压缩类型
    if ( MQTT_TLV_COMPRESS_NONE != pkt->flag.compress ) {
        // TODO：已启用数据压缩，当前未实现
        ESP_LOGE(TAG, "Unknown compress: %u", pkt->flag.compress);
        return -1;
    }
    
    // 根据协议版本分发处理
    switch (pkt->protocol_ver) {
        case MQTT_TLV_TAG_FLAG:
            // TODO：处理逻辑待实现
            ESP_LOGE(TAG, "Unsupported protocol_ver: %u", pkt->protocol_ver);
            ret = 0;
            break;
            
        case MQTT_TLV_FIELD_FLAG:
            /* 协议解析处理 */
            ret = mqtt_tlv_field_protocol_handle(data, size);
            break;

        default:
            ESP_LOGE(TAG, "Unknown protocol_ver: %u", pkt->protocol_ver);
            return -1;
    }

    return ret;
}

/**
 * @brief      MQTT TLV协议数据主动上报服务器
 * @param[in]  void  
 * @param[out] None
 * @return     uint8_t  1成功，0失败
 */
uint8_t mqtt_tlv_protocol_report_to_cloud(void)
{
    uint8_t rtn = 0;
    mqtt_tlv_report_data_t report_data;
    if (mqtt_tlv_queue && (xQueueReceive(mqtt_tlv_queue, &report_data, 0) == pdTRUE)) // 非阻塞
    {
        ESP_LOGW(TAG, "mqtt_tlv_protocol_report_to_cloud (Len: %d)", report_data.data_len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, report_data.data, report_data.data_len, ESP_LOG_DEBUG);

        iot_mqtt_send(report_data.data, report_data.data_len);
        free(report_data.data);
        rtn = 1;
    }

    return rtn;
}

/**
 * @brief      组帧并请求服务器域名及时间
 * @param[in] latitude   纬度
 * @param[in] longitude  经度
 * @param[out] None
 * @return 0成功，-1失败
 */
int mqtt_tlv_request_server_and_dst(int latitude, int longitude)
{
    // 分配缓冲区
    uint8_t *buff = (uint8_t *)iot_calloc(128);
    if (buff == NULL) {
        ESP_LOGE(TAG, "mqtt_tlv_request_server_and_dst malloc failed!");
        return -1;
    }

    // TLV数据区起始指针
    uint8_t *tlv_data = (uint8_t *)(buff + sizeof(mqtt_tlv_payload_t));
    uint16_t tlv_data_len = 0;  // 数据区使用长度

    // 单个TLV对象参数
    tlv_type_t tlv_tag = {0};
    uint8_t field_len = 0;
    uint16_t value_len = 0;
    uint16_t tlv_len = 0;

    /* ========================================================= */

    // 组帧接口ID
    tlv_tag.data_type = 0;
    tlv_tag.logic_type = TLV_LOGIC_EQ;
    tlv_tag.encode_type = TLV_ENCODE_UINT;
    field_len = strlen("@id");
    value_len = 4;

    // T
    tlv_data[tlv_data_len++] = tlv_tag.value;
    tlv_data[tlv_data_len++] = field_len;

    // L
    tlv_len = htons(field_len + value_len + 4);
    memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
    tlv_data_len += 2;

    // F
    memcpy(&tlv_data[tlv_data_len], "@id", field_len);
    tlv_data_len += field_len;

    // V
    uint32_t id = htonl(MQTT_TLV_HTTP_API2);
    memcpy(&tlv_data[tlv_data_len], &id, value_len);
    tlv_data_len += value_len;
    
    /* ========================================================= */

    // 组帧lat
    tlv_tag.data_type = 0;
    tlv_tag.logic_type = TLV_LOGIC_EQ;
    tlv_tag.encode_type = TLV_ENCODE_INT;
    field_len = strlen("lat");
    value_len = 4;

    // T
    tlv_data[tlv_data_len++] = tlv_tag.value;
    tlv_data[tlv_data_len++] = field_len;

    // L
    tlv_len = htons(field_len + value_len + 4);
    memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
    tlv_data_len += 2;

    // F
    memcpy(&tlv_data[tlv_data_len], "lat", field_len);
    tlv_data_len += field_len;

    // V
    int lat = htonl(latitude);
    memcpy(&tlv_data[tlv_data_len], &lat, value_len);
    tlv_data_len += value_len;
    
    /* ========================================================= */

    // 组帧lng
    tlv_tag.data_type = 0;
    tlv_tag.logic_type = TLV_LOGIC_EQ;
    tlv_tag.encode_type = TLV_ENCODE_INT;
    field_len = strlen("lng");
    value_len = 4;

    // T
    tlv_data[tlv_data_len++] = tlv_tag.value;
    tlv_data[tlv_data_len++] = field_len;

    // L
    tlv_len = htons(field_len + value_len + 4);
    memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
    tlv_data_len += 2;

    // F
    memcpy(&tlv_data[tlv_data_len], "lng", field_len);
    tlv_data_len += field_len;

    // V
    int lng = htonl(longitude);
    memcpy(&tlv_data[tlv_data_len], &lng, value_len);
    tlv_data_len += value_len;
    
    /* ========================================================= */

    // 计算总帧长
    uint16_t total_len = sizeof(mqtt_tlv_payload_t) + tlv_data_len;

    // 组装MQTT TLV协议帧头
    mqtt_tlv_payload_t *payload = (mqtt_tlv_payload_t *)buff;
    payload->frame_type = MQTT_TLV_MARK;
    payload->protocol_ver = MQTT_TLV_FIELD_FLAG;
    payload->scene = MQTT_FUNC_HTTP_REQUEST;
    payload->payload_len = U16_TO_BE(total_len);
    payload->flag.op_type = MQTT_TLV_OP_GET; 
    payload->flag.resp_type = MQTT_TLV_RESP_ASYNC;
    payload->flag.compress = MQTT_TLV_COMPRESS_NONE;
    payload->flag.tag_type = MQTT_TLV_TAG_1BYTE;
    payload->request_id = esp_random();
    payload->slaveaddress = 0;

    // CRC16（小端）
    uint16_t crc = crc16_modbus_calc((uint8_t *)buff, total_len);
    buff[total_len++] = crc & 0xFF;
    buff[total_len++] = (crc >> 8) & 0xFF;

    // 打印合成的TLV数据
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, total_len, ESP_LOG_INFO);

    // 发送
    mqtt_tlv_report_data_t report_data = {
        .data = buff,
        .data_len = total_len
    };
    if (mqtt_tlv_queue && xQueueSendToBack(mqtt_tlv_queue, &report_data, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "mqtt_tlv_request_server_and_dst: queue send failed");
        iot_free(buff);
        return -1;
    }

    // 记录请求ID
    time_check_request_id = payload->request_id;
    
    return 0;
}

/**
 * @brief 组帧并上报4G物联网卡信息（IMEI、IMSI、ICCID）
 * @param[in] imei              IMEI字符串
 * @param[in] imsi              IMSI字符串
 * @param[in] iccid             ICCID字符串
 * @return 0成功，-1失败
 */
int mqtt_tlv_report_4g_info(const char *imei, const char *imsi, const char *iccid)
{
    // 计算数据长度
    size_t imei_len = 0;
    size_t imsi_len = 0;
    size_t iccid_len = 0;
    if(imei) imei_len = strlen(imei);
    if(imsi) imsi_len = strlen(imsi);
    if(iccid) iccid_len = strlen(iccid);

    // 最大长度检查（假设每个字段最大20字节）
    if (imei_len > 20 || imsi_len > 20 || iccid_len > 20) {
        ESP_LOGE(TAG, "mqtt_tlv_report_4g_info: input string too long");
        return -1;
    }

    // 分配缓冲区
    uint8_t *buff = (uint8_t *)iot_calloc(256);
    if (buff == NULL) {
        ESP_LOGE(TAG, "mqtt_tlv_report_4g_info malloc failed!");
        return -1;
    }

    // TLV数据区起始指针
    uint8_t *tlv_data = (uint8_t *)(buff + sizeof(mqtt_tlv_payload_t));
    uint16_t tlv_data_len = 0;  // 数据区使用长度

    // 单个TLV对象参数
    tlv_type_t tlv_tag = {0};
    uint8_t field_len = 0;
    uint16_t value_len = 0;
    uint16_t tlv_len = 0;

    /* ========================================================= */

    // 组帧接口ID
    tlv_tag.data_type = 0;
    tlv_tag.logic_type = TLV_LOGIC_EQ;
    tlv_tag.encode_type = TLV_ENCODE_UINT;
    field_len = strlen("@id");
    value_len = 4;

    // T
    tlv_data[tlv_data_len++] = tlv_tag.value;
    tlv_data[tlv_data_len++] = field_len;

    // L
    tlv_len = htons(field_len + value_len + 4);
    memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
    tlv_data_len += 2;

    // F
    memcpy(&tlv_data[tlv_data_len], "@id", field_len);
    tlv_data_len += field_len;

    // V
    uint32_t id = htonl(MQTT_TLV_HTTP_API1);
    memcpy(&tlv_data[tlv_data_len], &id, value_len);
    tlv_data_len += value_len;
    
    /* ========================================================= */
    
    // 组帧IMEI
    if ( imei_len > 0 ) {
        tlv_tag.data_type = 1;
        tlv_tag.logic_type = TLV_LOGIC_EQ;
        tlv_tag.encode_type = TLV_ENCODE_ASCII_STRING;
        field_len = strlen("imei");
        value_len = 20;
        
        // T
        tlv_data[tlv_data_len++] = tlv_tag.value;
        tlv_data[tlv_data_len++] = field_len;
        
        // L
        tlv_len = htons(field_len + value_len + 4);
        memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
        tlv_data_len += 2;
        
        // F
        memcpy(&tlv_data[tlv_data_len], "imei", field_len);
        tlv_data_len += field_len;
        
        // V
        memcpy(&tlv_data[tlv_data_len], imei, imei_len);
        tlv_data_len += value_len;
    }

    /* ========================================================= */

    // 组帧IMSI
    if ( imsi_len > 0 ) {
        tlv_tag.data_type = 1;
        tlv_tag.logic_type = TLV_LOGIC_EQ;
        tlv_tag.encode_type = TLV_ENCODE_ASCII_STRING;
        field_len = strlen("imsi");
        value_len = 20;
        
        // T
        tlv_data[tlv_data_len++] = tlv_tag.value;
        tlv_data[tlv_data_len++] = field_len;
        
        // L
        tlv_len = htons(field_len + value_len + 4);
        memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
        tlv_data_len += 2;
        
        // F
        memcpy(&tlv_data[tlv_data_len], "imsi", field_len);
        tlv_data_len += field_len;
        
        // V
        memcpy(&tlv_data[tlv_data_len], imsi, imsi_len);
        tlv_data_len += value_len;
    }

    /* ========================================================= */

    // 组帧ICCID
    if ( iccid_len > 0 ) {
        tlv_tag.data_type = 1;
        tlv_tag.logic_type = TLV_LOGIC_EQ;
        tlv_tag.encode_type = TLV_ENCODE_ASCII_STRING;
        field_len = strlen("iccid");
        value_len = 20;
        
        // T
        tlv_data[tlv_data_len++] = tlv_tag.value;
        tlv_data[tlv_data_len++] = field_len;
        
        // L
        tlv_len = htons(field_len + value_len + 4);
        memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
        tlv_data_len += 2;
        
        // F
        memcpy(&tlv_data[tlv_data_len], "iccid", field_len);
        tlv_data_len += field_len;
        
        // V
        memcpy(&tlv_data[tlv_data_len], iccid, iccid_len);
        tlv_data_len += value_len;
    }
    
    /* ========================================================= */

    // 计算总帧长
    uint16_t total_len = sizeof(mqtt_tlv_payload_t) + tlv_data_len;

    // 组装MQTT TLV协议帧头
    mqtt_tlv_payload_t *payload = (mqtt_tlv_payload_t *)buff;
    payload->frame_type = MQTT_TLV_MARK;
    payload->protocol_ver = MQTT_TLV_FIELD_FLAG;
    payload->scene = MQTT_FUNC_HTTP_REQUEST;
    payload->payload_len = U16_TO_BE(total_len);
    payload->flag.op_type = MQTT_TLV_OP_SET; 
    payload->flag.resp_type = MQTT_TLV_RESP_VOID;
    payload->flag.compress = MQTT_TLV_COMPRESS_NONE;
    payload->flag.tag_type = MQTT_TLV_TAG_1BYTE;
    payload->request_id = 0;
    payload->slaveaddress = 0;

    // CRC16（小端）
    uint16_t crc = crc16_modbus_calc((uint8_t *)buff, total_len);
    buff[total_len++] = crc & 0xFF;
    buff[total_len++] = (crc >> 8) & 0xFF;

    // 打印合成的TLV数据
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, total_len, ESP_LOG_INFO);

    // 发送
    mqtt_tlv_report_data_t report_data = {
        .data = buff,
        .data_len = total_len
    };
    if (mqtt_tlv_queue && xQueueSendToBack(mqtt_tlv_queue, &report_data, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "mqtt_tlv_report_4g_info: queue send failed");
        iot_free(buff);
        return -1;
    }
    
    return 0;
}


/**
 * @brief 组帧并上报设备经纬度数据
 * @param[in] latitude          纬度
 * @param[in] longitude         经度
 * @param[in] signalStrength    信号强度
 * @param[in] connectState4G    4G连接状态
 * @return 0成功，-1失败
 */
int mqtt_tlv_report_location_info(int latitude, int longitude, uint8_t signalStrength, bool connectState4G)
{
    // 分配缓冲区
    uint8_t *buff = (uint8_t *)iot_calloc(256);
    if (buff == NULL) {
        ESP_LOGE(TAG, "mqtt_tlv_report_location_info malloc failed!");
        return -1;
    }

    // TLV数据区起始指针
    uint8_t *tlv_data = (uint8_t *)(buff + sizeof(mqtt_tlv_payload_t));
    uint16_t tlv_data_len = 0;  // 数据区使用长度

    // 单个TLV对象参数
    tlv_type_t tlv_tag = {0};
    uint8_t field_len = 0;
    uint16_t value_len = 0;
    uint16_t tlv_len = 0;

    /* ========================================================= */

    // 组帧接口ID
    tlv_tag.data_type = 0;
    tlv_tag.logic_type = TLV_LOGIC_EQ;
    tlv_tag.encode_type = TLV_ENCODE_UINT;
    field_len = strlen("@id");
    value_len = 4;

    // T
    tlv_data[tlv_data_len++] = tlv_tag.value;
    tlv_data[tlv_data_len++] = field_len;

    // L
    tlv_len = htons(field_len + value_len + 4);
    memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
    tlv_data_len += 2;

    // F
    memcpy(&tlv_data[tlv_data_len], "@id", field_len);
    tlv_data_len += field_len;

    // V
    uint32_t id = htonl(MQTT_TLV_HTTP_API5);
    memcpy(&tlv_data[tlv_data_len], &id, value_len);
    tlv_data_len += value_len;
  
    /* ========================================================= */

    // 组帧latitude
    if ( latitude != 0 ) {
        tlv_tag.data_type = 0;
        tlv_tag.logic_type = TLV_LOGIC_EQ;
        tlv_tag.encode_type = TLV_ENCODE_INT;
        field_len = strlen("latitude");
        value_len = 4;
        
        // T
        tlv_data[tlv_data_len++] = tlv_tag.value;
        tlv_data[tlv_data_len++] = field_len;
        
        // L
        tlv_len = htons(field_len + value_len + 4);
        memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
        tlv_data_len += 2;
        
        // F
        memcpy(&tlv_data[tlv_data_len], "latitude", field_len);
        tlv_data_len += field_len;
        
        // V
        int lat = htonl(latitude);
        memcpy(&tlv_data[tlv_data_len], &lat, value_len);
        tlv_data_len += value_len;
    }
    
    /* ========================================================= */

    // 组帧longitude
    if ( longitude != 0 ) {
        tlv_tag.data_type = 0;
        tlv_tag.logic_type = TLV_LOGIC_EQ;
        tlv_tag.encode_type = TLV_ENCODE_INT;
        field_len = strlen("longitude");
        value_len = 4;
        
        // T
        tlv_data[tlv_data_len++] = tlv_tag.value;
        tlv_data[tlv_data_len++] = field_len;
        
        // L
        tlv_len = htons(field_len + value_len + 4);
        memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
        tlv_data_len += 2;
        
        // F
        memcpy(&tlv_data[tlv_data_len], "longitude", field_len);
        tlv_data_len += field_len;
        
        // V
        int lng = htonl(longitude);
        memcpy(&tlv_data[tlv_data_len], &lng, value_len);
        tlv_data_len += value_len;
    }
    
    /* ========================================================= */

    // 组帧SignalStrength
    if ( signalStrength != 0 ) {
        tlv_tag.data_type = 0;
        tlv_tag.logic_type = TLV_LOGIC_EQ;
        tlv_tag.encode_type = TLV_ENCODE_UTINY;
        field_len = strlen("signalStrength");
        value_len = 1;
        
        // T
        tlv_data[tlv_data_len++] = tlv_tag.value;
        tlv_data[tlv_data_len++] = field_len;
        
        // L
        tlv_len = htons(field_len + value_len + 4);
        memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
        tlv_data_len += 2;
        
        // F
        memcpy(&tlv_data[tlv_data_len], "signalStrength", field_len);
        tlv_data_len += field_len;
        
        // V
        tlv_data[tlv_data_len] = signalStrength;
        tlv_data_len += value_len;
    }

    /* ========================================================= */

    // 组帧connectState4G
    tlv_tag.data_type = 0;
    tlv_tag.logic_type = TLV_LOGIC_EQ;
    tlv_tag.encode_type = TLV_ENCODE_BOOL;
    field_len = strlen("connectState4G");
    value_len = 1;
    
    // T
    tlv_data[tlv_data_len++] = tlv_tag.value;
    tlv_data[tlv_data_len++] = field_len;
    
    // L
    tlv_len = htons(field_len + value_len + 4);
    memcpy(&tlv_data[tlv_data_len], &tlv_len, 2);
    tlv_data_len += 2;
    
    // F
    memcpy(&tlv_data[tlv_data_len], "connectState4G", field_len);
    tlv_data_len += field_len;
    
    // V
    tlv_data[tlv_data_len] = connectState4G;
    tlv_data_len += value_len;
        
    /* ========================================================= */

    // 计算总帧长
    uint16_t total_len = sizeof(mqtt_tlv_payload_t) + tlv_data_len;

    // 组装MQTT TLV协议帧头
    mqtt_tlv_payload_t *payload = (mqtt_tlv_payload_t *)buff;
    payload->frame_type = MQTT_TLV_MARK;
    payload->protocol_ver = MQTT_TLV_FIELD_FLAG;
    payload->scene = MQTT_FUNC_HTTP_REQUEST;
    payload->payload_len = U16_TO_BE(total_len);
    payload->flag.op_type = MQTT_TLV_OP_SET; 
    payload->flag.resp_type = MQTT_TLV_RESP_VOID;
    payload->flag.compress = MQTT_TLV_COMPRESS_NONE;
    payload->flag.tag_type = MQTT_TLV_TAG_1BYTE;
    payload->request_id = 0;
    payload->slaveaddress = 0;

    // CRC16（小端）
    uint16_t crc = crc16_modbus_calc((uint8_t *)buff, total_len);
    buff[total_len++] = crc & 0xFF;
    buff[total_len++] = (crc >> 8) & 0xFF;

    // 打印合成的TLV数据
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, total_len, ESP_LOG_INFO);

    // 发送
    mqtt_tlv_report_data_t report_data = {
        .data = buff,
        .data_len = total_len
    };
    if (mqtt_tlv_queue && xQueueSendToBack(mqtt_tlv_queue, &report_data, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "mqtt_tlv_report_4g_info: queue send failed");
        iot_free(buff);
        return -1;
    }
    
    return 0;
}

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE    

/**
 * @brief      组装设备参数配置更新请求并推送至队列
 * @param[in]  meshID     空间ID（优先使用），为NULL时使用sn
 * @param[in]  sn         设备序列号，meshID无效时使用
 * @param[in]  object     参数对象数组
 * @param[in]  object_cnt 参数对象个数
 * @param[out] buff       接收TLV协议帧的缓冲区
 * @param[in]  buff_size  缓冲区大小
 * @return     0成功，-1参数错误或queue发送失败，-2 HTTP_ID序列化失败，-3 meshId/sn错误，-4-i 第i个参数序列化失败
 * @note       使用此函数前需调用 mqtt_tlv_init() 初始化队列
 */
int mqtt_tlv_request_device_config_set(uint8_t *meshID, char *sn, mqtt_tlfv_config_field_t *object, uint16_t object_cnt, uint8_t *buff, uint16_t buff_size)
{
    if (!object || !buff || !object_cnt || !buff_size) {
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_set : parameter error");
        return -1;
    }

    // TLV数据区起始指针
    uint8_t *tlv_data = (uint8_t *)(buff + sizeof(mqtt_tlv_payload_t));
    uint16_t tlv_data_len = 0;  // 数据区使用长度

    // 单个TLV对象参数
    mqtt_tlfv_frame_t tlfv_frame = {0};
    int16_t tlfv_len = 0;
    uint32_t api_id = MQTT_TLV_HTTP_API3;

    /* ========================================================= */

    // 组帧接口ID
    tlfv_frame.type.data_type = 0;
    tlfv_frame.type.logic_type = TLV_LOGIC_EQ;
    tlfv_frame.type.encode_type = TLV_ENCODE_UINT;
    tlfv_frame.field = (uint8_t *)"@id";
    tlfv_frame.field_len = strlen("@id");
    tlfv_frame.value = (uint8_t *)&api_id;
    tlfv_frame.value_len = sizeof(uint32_t);
    tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
    if (tlfv_len < 0) {
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_set : http_id error:%d", tlfv_len);
        return -2;
    }
    
    tlv_data_len += tlfv_len;

    /* ========================================================= */
    
    tlfv_frame.type.data_type = 1;
    tlfv_frame.type.logic_type = TLV_LOGIC_EQ;

    /* meshId有效，优先上报空间参数 */
    if (meshID != NULL && memcmp(meshID, "\0\0\0\0\0\0", 6) != 0) {
        tlfv_frame.type.encode_type = TLV_ENCODE_HEX_STRING;
        tlfv_frame.field = (uint8_t *)"meshId";
        tlfv_frame.field_len = strlen("meshId");
        tlfv_frame.value = (uint8_t *)meshID;
        tlfv_frame.value_len = 6;
        tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
        if (tlfv_len < 0) {
            ESP_LOGE(TAG, "mqtt_tlv_request_device_config_set : meshId error:%d", tlfv_len);
            return -3;
        }
        
        tlv_data_len += tlfv_len;
    } else if(sn && strlen(sn) != 0) {
        /* meshId无效，上报本机参数 */
        tlfv_frame.type.encode_type = TLV_ENCODE_ASCII_STRING;
        tlfv_frame.field = (uint8_t *)"sn";
        tlfv_frame.field_len = strlen("sn");
        tlfv_frame.value = (uint8_t *)sn;
        tlfv_frame.value_len = strlen(sn);
        tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
        if (tlfv_len < 0) {
            ESP_LOGE(TAG, "mqtt_tlv_request_device_config_set : sn error:%d", tlfv_len);
            return -3;
        }
        
        tlv_data_len += tlfv_len;
    } else {
        /*meshId和sn均无效，参数错误*/
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_set : meshID and sn all invalid!");
        return -3;
    }

    /* ========================================================= */

    tlfv_frame.type.data_type = 1;
    tlfv_frame.type.logic_type = TLV_LOGIC_EQ;
    tlfv_frame.type.encode_type = TLV_ENCODE_HEX_STRING;

    /*组帧：实际参数*/
    for (int i = 0; i < object_cnt; i++) {
        if (object[i].data_len == 0 || !object[i].data_field || !object[i].data_addr) {
            continue;
        }
        
        tlfv_frame.field = (uint8_t *)object[i].data_field;
        tlfv_frame.field_len = strlen(object[i].data_field);
        tlfv_frame.value = (uint8_t *)object[i].data_addr;
        tlfv_frame.value_len = object[i].data_len;
        tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
        if (tlfv_len < 0) {
            ESP_LOGE(TAG, "MQTT tlv device config %s error:%d", object[i].data_field, tlfv_len);
            return -(4 + i);
        }

        tlv_data_len += tlfv_len;
    }

    /* ========================================================= */

    // 计算总帧长
    uint16_t total_len = sizeof(mqtt_tlv_payload_t) + tlv_data_len;

    // 组装MQTT TLV协议帧头
    mqtt_tlv_payload_t *payload = (mqtt_tlv_payload_t *)buff;
    payload->frame_type = MQTT_TLV_MARK;
    payload->protocol_ver = MQTT_TLV_FIELD_FLAG;
    payload->scene = MQTT_FUNC_HTTP_REQUEST;
    payload->payload_len = U16_TO_BE(total_len);
    payload->flag.op_type = MQTT_TLV_OP_SET;
    payload->flag.resp_type = MQTT_TLV_RESP_VOID;
    payload->flag.compress = MQTT_TLV_COMPRESS_NONE;
    payload->flag.tag_type = MQTT_TLV_TAG_1BYTE;
    payload->request_id = 0;
    payload->slaveaddress = 0;

    // CRC16（小端）
    uint16_t crc = crc16_modbus_calc((uint8_t *)buff, total_len);
    buff[total_len++] = crc & 0xFF;
    buff[total_len++] = (crc >> 8) & 0xFF;

    // 打印合成的TLV数据
    ESP_LOGD(TAG, "MQTT tlv device config set:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, total_len, ESP_LOG_DEBUG);

    // 发送
    mqtt_tlv_report_data_t report_data = {
        .data = buff,
        .data_len = total_len
    };
    if (mqtt_tlv_queue && xQueueSendToBack(mqtt_tlv_queue, &report_data, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_set: queue send failed");
        return -1;
    }

    return 0;
}

/**
 * @brief      组装设备参数配置获取请求并推送至队列
 * @param[in]  meshID     空间ID（优先使用），为NULL时使用sn
 * @param[in]  sn         设备序列号，meshID无效时使用
 * @param[in]  object     指定参数对象数组（可选）
 * @param[in]  object_cnt 参数对象个数（可选）
 * @param[out] buff       接收TLV协议帧的缓冲区
 * @param[in]  buff_size  缓冲区大小
 * @return     0成功，-1参数错误或queue发送失败，-2 HTTP_ID序列化失败，-3 meshId/sn错误，-4-i 第i个参数序列化失败
 * @note       使用此函数前需调用 mqtt_tlv_init() 初始化队列
 */
int mqtt_tlv_request_device_config_get(uint8_t *meshID, char *sn, mqtt_tlfv_config_field_t *object, uint16_t object_cnt, uint8_t *buff, uint16_t buff_size)
{
    if (!buff || !buff_size) {
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_get : parameter error");
        return -1;
    }

    // TLV数据区起始指针
    uint8_t *tlv_data = (uint8_t *)(buff + sizeof(mqtt_tlv_payload_t));
    uint16_t tlv_data_len = 0;  // 数据区使用长度

    // 单个TLV对象参数
    mqtt_tlfv_frame_t tlfv_frame = {0};
    int16_t tlfv_len = 0;
    uint32_t api_id = MQTT_TLV_HTTP_API4;

    /* ========================================================= */

    // 组帧接口ID
    tlfv_frame.type.data_type = 0;
    tlfv_frame.type.logic_type = TLV_LOGIC_EQ;
    tlfv_frame.type.encode_type = TLV_ENCODE_UINT;
    tlfv_frame.field = (uint8_t *)"@id";
    tlfv_frame.field_len = strlen("@id");
    tlfv_frame.value = (uint8_t *)&api_id;
    tlfv_frame.value_len = sizeof(uint32_t);
    tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
    if (tlfv_len < 0) {
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_get : http_id error:%d", tlfv_len);
        return -2;
    }
    
    tlv_data_len += tlfv_len;

    /* ========================================================= */
    
    tlfv_frame.type.data_type = 1;
    tlfv_frame.type.logic_type = TLV_LOGIC_EQ;

    /* meshId有效，优先上报空间参数 */
    if (meshID != NULL && memcmp(meshID, "\0\0\0\0\0\0", 6) != 0) {
        tlfv_frame.type.encode_type = TLV_ENCODE_HEX_STRING;
        tlfv_frame.field = (uint8_t *)"meshId";
        tlfv_frame.field_len = strlen("meshId");
        tlfv_frame.value = (uint8_t *)meshID;
        tlfv_frame.value_len = 6;
        tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
        if (tlfv_len < 0) {
            ESP_LOGE(TAG, "mqtt_tlv_request_device_config_get : meshId error:%d", tlfv_len);
            return -3;
        }
        
        tlv_data_len += tlfv_len;
    } else if(sn && strlen(sn) != 0) {
        /* meshId无效，上报本机参数 */
        tlfv_frame.type.encode_type = TLV_ENCODE_ASCII_STRING;
        tlfv_frame.field = (uint8_t *)"sn";
        tlfv_frame.field_len = strlen("sn");
        tlfv_frame.value = (uint8_t *)sn;
        tlfv_frame.value_len = strlen(sn);
        tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
        if (tlfv_len < 0) {
            ESP_LOGE(TAG, "mqtt_tlv_request_device_config_get : sn error:%d", tlfv_len);
            return -3;
        }
        
        tlv_data_len += tlfv_len;
    } else {
        /*meshId和sn均无效，参数错误*/
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_get : meshID and sn all invalid!");
        return -3;
    }

    /* ========================================================= */

    tlfv_frame.type.data_type = 1;
    tlfv_frame.type.logic_type = TLV_LOGIC_EQ;
    tlfv_frame.type.encode_type = TLV_ENCODE_ASCII_STRING;

    /*组帧：请求参数（可选）*/
    if ( object && object_cnt )
    {
        for (int i = 0; i < object_cnt; i++) {
            if (!object[i].data_field) {
                continue;
            }
            
            tlfv_frame.field = (uint8_t *)"groupName";
            tlfv_frame.field_len = strlen("groupName");
            tlfv_frame.value = (uint8_t *)object[i].data_field;
            tlfv_frame.value_len = strlen(object[i].data_field);
            tlfv_len = mqtt_tlfv_frame_serialize(&tlfv_frame, &tlv_data[tlv_data_len], buff_size - sizeof(mqtt_tlv_payload_t) - tlv_data_len);
            if (tlfv_len < 0) {
                ESP_LOGE(TAG, "MQTT tlv device config %s error:%d", object[i].data_field, tlfv_len);
                return -(4 + i);
            }
        
            tlv_data_len += tlfv_len;
        }
    }

    /* ========================================================= */

    // 计算总帧长
    uint16_t total_len = sizeof(mqtt_tlv_payload_t) + tlv_data_len;

    // 组装MQTT TLV协议帧头
    mqtt_tlv_payload_t *payload = (mqtt_tlv_payload_t *)buff;
    payload->frame_type = MQTT_TLV_MARK;
    payload->protocol_ver = MQTT_TLV_FIELD_FLAG;
    payload->scene = MQTT_FUNC_HTTP_REQUEST;
    payload->payload_len = U16_TO_BE(total_len);
    payload->flag.op_type = MQTT_TLV_OP_GET;
    payload->flag.resp_type = MQTT_TLV_RESP_ASYNC;
    payload->flag.compress = MQTT_TLV_COMPRESS_NONE;
    payload->flag.tag_type = MQTT_TLV_TAG_1BYTE;
    payload->request_id = esp_random();
    payload->slaveaddress = 0;

    // CRC16（小端）
    uint16_t crc = crc16_modbus_calc((uint8_t *)buff, total_len);
    buff[total_len++] = crc & 0xFF;
    buff[total_len++] = (crc >> 8) & 0xFF;

    // 打印合成的TLV数据
    ESP_LOGD(TAG, "MQTT tlv device config get:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, total_len, ESP_LOG_DEBUG);

    // 发送
    mqtt_tlv_report_data_t report_data = {
        .data = buff,
        .data_len = total_len
    };
    if (mqtt_tlv_queue && xQueueSendToBack(mqtt_tlv_queue, &report_data, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "mqtt_tlv_request_device_config_get: queue send failed");
        return -1;
    }

    // 记录请求ID
    device_cfg_get_id = payload->request_id;

    return 0;
}

#endif

#endif
