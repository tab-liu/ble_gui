#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "aiot_log.h"
#include "wlcc_process.h"
#include "wlcc_interface.h"
#include "udt_transfer.h"

#include "esp_log.h"

#define TAG "[AIoT_Log]"

static aiot_log_t aiot_log = {0};

/* 信号量（互斥） */
static SemaphoreHandle_t xAIoTLogMutex = NULL;

/* 简单封装：获取/释放互斥 */
static inline BaseType_t AIoT_Log_TakeMutex(TickType_t timeout_ms)
{
    if (!xAIoTLogMutex) return pdFALSE;
    return xSemaphoreTake(xAIoTLogMutex, pdMS_TO_TICKS(timeout_ms));
}
static inline void AIoT_Log_GiveMutex(void)
{
    if (xAIoTLogMutex) xSemaphoreGive(xAIoTLogMutex);
}

bool aiot_log_init(void)
{
    if (xAIoTLogMutex == NULL) {
        xAIoTLogMutex = xSemaphoreCreateMutex();
        if (xAIoTLogMutex == NULL) {
            ESP_LOGE(TAG, "aiot_log_init: create mutex failed");
        }
    }

    for (int i = 0; i < sizeof(reals.supper_control.cmd_list) / sizeof(reals.supper_control.cmd_list[0]); i++)
    {
        reals.supper_control.cmd_list[i].cmd = 10000 + i;
        reals.supper_control.cmd_list[i].value = 0;
    }

    aiot_log.type = 0;
    return CircularQueue_Init(&aiot_log.log_queue, CIRCULAR_QUEUE_MAX_SIZE);
}

// wlcc发送Log信息
static void aiot_log_send_by_wlcc(void)
{
    uint8_t *buffer = (uint8_t *)iot_malloc(CIRCULAR_QUEUE_MAX_SIZE * sizeof(uint8_t) + 100);
    if (!buffer) {
        printf("queue is full but failed to allocate buffer for flushing logs\n");
        return;
    }
    
    memset(buffer, 0, CIRCULAR_QUEUE_MAX_SIZE * sizeof(uint8_t) + 100);
    buffer[20] = aiot_log.type; // 在数据前面预留20字节作为协议头，实际数据从buffer[20]开始存储
    
    uint16_t buffer_len = CircularQueue_Pop(&aiot_log.log_queue, &buffer[23], CIRCULAR_QUEUE_MAX_SIZE);
    if ((buffer_len > 0) 
        && (IOT_OK == is_ready_wlcc_network()))
    {
        udt_tx_data_t tx_data;
        tx_data.funcode = UDT_FUNCODE_QUERY_CONTENT_RTN;    // 功能码
        tx_data.pkg_idx = 1;                                // 包索引
        tx_data.pkg_total = 1;                              // 总包数
        tx_data.start_pos = 0;                              // 数据偏移
        tx_data.data_size = buffer_len + 3;                 // 数据长度 + 3字节的头
        tx_data.data = &buffer[20];                         // 数据
        uint16_t udt_len = udt_header_data(buffer, &tx_data);
        wlcc_modbus_msg_send_file_context(buffer, udt_len, 0, 0);
    }
    
    iot_free(buffer);
    buffer = NULL;
}

// 该函数内禁用ESP_LOG，避免无限递归
uint16_t aiot_log_push(const uint8_t *data, uint16_t len, log_type_t type)
{
    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (AIoT_Log_TakeMutex(0) != pdTRUE) {
        return 0;   // 避免内部函数调用ESP_LOG导致无限递归
    }

    uint16_t ret = 0;
    if (aiot_log.type != 0 && aiot_log.type != type) {
        aiot_log.type = LOG_TYPE_OTHER; // 混合报文类型
    } else {
        aiot_log.type = type;
    }

    uint16_t log_data_len = sizeof(log_frame_t) + len;

    // TODO: 暂时不区分专属压缩协议
    if( log_data_len > CIRCULAR_QUEUE_MAX_SIZE ) {
        goto exit;  // 单条日志长度溢出，跳过处理
    }
    
    log_frame_t *log_data = (log_frame_t *)iot_calloc(log_data_len);
    if (!log_data) {
        printf("failed to allocate memory for log data\n");
        goto exit;
    }
    
    log_data->type = type;
    log_data->len = len;
    log_data->timestamp = time(NULL);
    memcpy(log_data->data, data, len);

    uint16_t remain_len = CircularQueue_Space(&aiot_log.log_queue);

    // 当队列剩余空间不足时，触发一次发送
    if (remain_len < 100 || remain_len < log_data_len)
    {
        // 当前默认仅支持wlcc发送
        aiot_log_send_by_wlcc();
    }

    ret = CircularQueue_Push(&aiot_log.log_queue, (uint8_t *)log_data, log_data_len);
    iot_free(log_data);
    log_data = NULL;

    exit:

    /* 释放互斥 */
    AIoT_Log_GiveMutex();

    return ret;
}

uint16_t aiot_log_pop(uint8_t *data, uint16_t len)
{
    return CircularQueue_Pop(&aiot_log.log_queue, data, len);
}
