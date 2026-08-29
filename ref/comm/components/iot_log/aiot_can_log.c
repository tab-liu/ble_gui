/**
  ******************************************************************************
  * @file      aiot_can_log.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/7/30
  * @brief     CAN LOG触发存储机制
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/7/30  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  参考文档: https://192.168.102.10/svn/TecCenter/ComGroup/技术文件/通讯技术开发规范/IOT黑匣子方案/IOT CAN总线监听-文件远程运维方案20260729.docx
  */

/* ================================ 库文件引用 ================================ */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "aiot_can_log.h"
#include "comm_define.h"
#include "utils.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[AIoT_Can_Log]"

/* =============================== 文件内全局变量 ================================ */

/* 信号量（互斥） */
static SemaphoreHandle_t xAIoTCanLogMutex = NULL;

/*存储队列*/
static QueueHandle_t xAIoTCanLogQueue = NULL;

/*当前正在接收的存储片及其序号*/
static data_slice_t AIoT_Can_Log_Push_Slice = {0};
static uint16_t AIoT_Can_Log_Push_Slice_Index = 0;

/*当前正在存储的存储片序号*/
static uint16_t AIoT_Can_Log_Save_Slice_Index = 0;

/*超级命令备份*/
static super_cmd_can_log_cfg_t back_cmd_cfg = {0};

/* ================================ 模块函数定义 ================================ */

/**
 * @brief  CAN日志队列与互斥信号量初始化
 * @return true  初始化成功
 * @return false 初始化失败（通常为堆内存不足）
 * @note   请在任务调度启动后、使用CAN日志模块前调用，内置重复调用防重复创建
 */
static bool AIoT_CanLog_Init(void)
{
    /* 1. 创建互斥信号量 */
    if (xAIoTCanLogMutex == NULL)
    {
        xAIoTCanLogMutex = xSemaphoreCreateMutex();
        if (xAIoTCanLogMutex == NULL)
        {
            return false;
        }
    }

    /* 2. 创建存储队列 */
    if (xAIoTCanLogQueue == NULL)
    {
        xAIoTCanLogQueue = xQueueCreate(AIOT_CANLOG_BUF_NUM, sizeof(data_slice_t));
        if (xAIoTCanLogQueue == NULL)
        {
            /* 队列创建失败，回滚销毁已创建的互斥量，避免内存泄漏 */
            vSemaphoreDelete(xAIoTCanLogMutex);
            xAIoTCanLogMutex = NULL;
            return false;
        }
    }

    return true;
}

/**
 * @brief CAN日志队列与互斥信号量逆初始化（销毁资源）
 * @note  必须确保所有任务已停止使用队列和信号量后再调用，避免系统异常
 */
static void AIoT_CanLog_Deinit(void)
{
    /* 先销毁互斥信号量，停止数据流转 */
    if (xAIoTCanLogMutex != NULL)
    {
        vSemaphoreDelete(xAIoTCanLogMutex);
        xAIoTCanLogMutex = NULL;
    }

    /*再清空队列内数据*/
    data_slice_t recv_data;
    while ( xAIoTCanLogQueue && xQueueReceive(xAIoTCanLogQueue, &recv_data, 0) == pdPASS ) {
        iot_free(recv_data.slice_ptr);
    }

    /* 最后销毁数据队列 */
    if (xAIoTCanLogQueue != NULL)
    {
        vQueueDelete(xAIoTCanLogQueue);
        xAIoTCanLogQueue = NULL;
    }
}

/* 简单封装：获取/释放互斥 */
static inline BaseType_t AIoT_Can_Log_TakeMutex(TickType_t timeout_ms)
{
    if (!xAIoTCanLogMutex) return pdFALSE;
    return xSemaphoreTake(xAIoTCanLogMutex, pdMS_TO_TICKS(timeout_ms));
}
static inline void AIoT_Can_Log_GiveMutex(void)
{
    if (xAIoTCanLogMutex) xSemaphoreGive(xAIoTCanLogMutex);
}


/**
  * @brief      CAN日志入队
  * @param[in]  const uint8_t *data  
                uint16_t len         
                log_type_t type      
  * @param[out] None
  * @return     uint16_t
  */
uint16_t AIoT_Can_Log_Push(const uint8_t *data, uint16_t len, log_type_t type)
{
    uint16_t ret = 0;
    
    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (AIoT_Can_Log_TakeMutex(0) != pdTRUE) {
        return 0;
    }

    /*检查是否已达记录上限*/
    if ( AIoT_Can_Log_Push_Slice_Index >= AIOT_CANLOG_BUF_NUM ) {
        goto push_exit;
    }
    
    /* 当剩余空间不足时，触发一次入队 */
    uint16_t remain_len = AIOT_CANLOG_BUF_MAX_LEN - AIoT_Can_Log_Push_Slice.slice_len;
    uint16_t log_data_len = sizeof(log_frame_t) + len;
    if (AIoT_Can_Log_Push_Slice.slice_ptr && remain_len < log_data_len) {
        if (!xAIoTCanLogQueue || xQueueSend(xAIoTCanLogQueue, &AIoT_Can_Log_Push_Slice, 0) != pdPASS) {
            goto push_exit; // 入队失败，保留片数据，丢弃本次帧数据
        }

        AIoT_Can_Log_Push_Slice.slice_ptr = NULL;
        AIoT_Can_Log_Push_Slice.slice_len = 0;
        AIoT_Can_Log_Push_Slice_Index++;
    }

    /*申请片内存*/
    if ( !AIoT_Can_Log_Push_Slice.slice_ptr ) {
        AIoT_Can_Log_Push_Slice.slice_ptr = (uint8_t *)iot_calloc(AIOT_CANLOG_BUF_MAX_LEN);
        if ( !AIoT_Can_Log_Push_Slice.slice_ptr ) {
            ESP_LOGE(TAG, "Failed to allocate memory for AIoT_Can_Log_Push_Slice!");
            goto push_exit;
        }
        AIoT_Can_Log_Push_Slice.slice_len = 0;
    }

    /*更新记录数据*/
    log_frame_t *log_data = (log_frame_t *)(AIoT_Can_Log_Push_Slice.slice_ptr + AIoT_Can_Log_Push_Slice.slice_len);
    log_data->type = type;
    log_data->len = len;
    log_data->timestamp = time(NULL);
    memcpy(log_data->data, data, len);
    AIoT_Can_Log_Push_Slice.slice_len += log_data_len;
    ret = 1;

push_exit:

    /* 释放互斥 */
    AIoT_Can_Log_GiveMutex();

    return ret;
}

/**
  * @brief      CAN LOG存储逻辑
  * @param[in]  uint32_t cmd_value  
  * @param[out] None
  * @return     uint32_t
  */
uint32_t AIoT_Can_Log_Process(uint32_t cmd_value)
{
    static uint32_t start_time = 0;
    super_cmd_can_log_cfg_t super_cmd_can_log_cfg = {.reg_val = cmd_value};
    
    switch ( super_cmd_can_log_cfg.bit.store_ctrl )
    {
        // CAN log文件存储开始
        case CAN_LOG_STORE_START :
            // 初始化
            if ( CAN_LOG_STATUS_RECORDING != back_cmd_cfg.bit.record_status ) {
                if ( AIoT_CanLog_Init() ) {
                    // 开始进行日志存储
                    super_cmd_can_log_cfg.bit.record_status = CAN_LOG_STATUS_RECORDING;
                    start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    AIoT_Can_Log_Save_Slice_Index = 0;
                    AIoT_Can_Log_Push_Slice_Index = 0;
                    iot_free(AIoT_Can_Log_Push_Slice.slice_ptr);
                    ESP_LOGW(TAG, "AIoT_Can_Log_Process : Start!");
                    
                    // 作为清理，尝试删除可能已创建的日志文件。
                    if (fs_file_exists(AIOT_CANLOG_FILE_NAME)) {
                        fs_file_delete(AIOT_CANLOG_FILE_NAME);
                    }
                } else {
                    // 初始化失败，强制结束
                    super_cmd_can_log_cfg.bit.store_ctrl = CAN_LOG_STORE_FORCE_END;
                    break;
                }
            }

            // 记录超时，立即存储等待结束
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_time) > AIOT_CANLOG_SAVE_TIMEOUT) {
                /* 尝试获取互斥，超时则跳过本轮处理 */
                if (AIoT_Can_Log_TakeMutex(500) != pdTRUE) {
                    break;
                }

                /*标记为已达上限，避免继续存储*/
                AIoT_Can_Log_Push_Slice_Index = AIOT_CANLOG_BUF_NUM;

                /*剩余内容入队*/
                if (AIoT_Can_Log_Push_Slice.slice_ptr && AIoT_Can_Log_Push_Slice.slice_len > 0) {
                    if (xAIoTCanLogQueue && xQueueSend(xAIoTCanLogQueue, &AIoT_Can_Log_Push_Slice, 0) == pdPASS) {
                        AIoT_Can_Log_Push_Slice.slice_ptr = NULL;
                    } else {
                        iot_free(AIoT_Can_Log_Push_Slice.slice_ptr);
                    }
                    AIoT_Can_Log_Push_Slice.slice_len = 0;
                }
                
                ESP_LOGW(TAG, "AIoT_Can_Log_Process : TIMEOUT!");
                
                /* 释放互斥 */
                AIoT_Can_Log_GiveMutex();
            }
            
            void *file = NULL;
            data_slice_t recv_data;
            while ( xAIoTCanLogQueue && xQueueReceive(xAIoTCanLogQueue, &recv_data, 0) == pdPASS ) {
                if ( !file ) {
                    // 检查文件是否存在，执行对应Open方式
                    if ( fs_file_exists(AIOT_CANLOG_FILE_NAME) ) {
                        file = fs_file_open(AIOT_CANLOG_FILE_NAME, "rb+");
                    } else {
                        file = fs_file_open(AIOT_CANLOG_FILE_NAME, "wb");
                    }
                    
                    // 打开文件失败，强制结束
                    if( file == NULL ) {
                        ESP_LOGE(TAG, "Failed to open %s!", AIOT_CANLOG_FILE_NAME);
                        goto receive_exit;
                    }
                }

                // 检查数据是否正常
                if ( recv_data.slice_ptr && recv_data.slice_len > 0 ) {
                    // 文件追加写入
                    if ( recv_data.slice_len == fs_file_custom_append(file, recv_data.slice_ptr, recv_data.slice_len) ){
                        // 写入成功
                        iot_free(recv_data.slice_ptr);
                        AIoT_Can_Log_Save_Slice_Index++;
                        if( AIoT_Can_Log_Save_Slice_Index >= AIOT_CANLOG_BUF_NUM ) break;
                    } else {
                        // 写入异常，强制结束
                        ESP_LOGE(TAG, "fs_file_custom_append error!");
                        goto receive_exit;
                    }
                } else {
                    // 接收数据无效，强制结束
                    ESP_LOGE(TAG, "recv_data is invalid!");
                    goto receive_exit;
                }

                continue;
                
receive_exit:
                // 强制结束
                if( file ) fs_file_close(file);
                super_cmd_can_log_cfg.bit.store_ctrl = CAN_LOG_STORE_FORCE_END;
                iot_free(recv_data.slice_ptr);
                break;
            }

            // 检查并关闭文件
            if( file ) fs_file_close(file);

            // 检查文件是否存储结束
            if (( AIoT_Can_Log_Save_Slice_Index >= AIOT_CANLOG_BUF_NUM ) 
                || ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_time) > AIOT_CANLOG_SAVE_TIMEOUT))
            {
                super_cmd_can_log_cfg.bit.record_status = CAN_LOG_STATUS_FINISHED;
                super_cmd_can_log_cfg.bit.store_ctrl = CAN_LOG_STORE_INVALID;
                ESP_LOGW(TAG, "AIoT_Can_Log_Process : End!");
            }
                
            break;

        // CAN log文件存储强制结束    
        case CAN_LOG_STORE_FORCE_END :
            // TODO：当前强制结束会丢失正在存储的片
            super_cmd_can_log_cfg.bit.record_status = CAN_LOG_STATUS_FINISHED;
            super_cmd_can_log_cfg.bit.store_ctrl = CAN_LOG_STORE_INVALID;
            ESP_LOGW(TAG, "AIoT_Can_Log_Process : FORCE_END!");
            break;

        // CAN log文件存储强制删除
        case CAN_LOG_STORE_FORCE_DEL :
            if ( CAN_LOG_STATUS_VALID == back_cmd_cfg.bit.record_status ) {
                if (fs_file_exists(AIOT_CANLOG_FILE_NAME)) {
                    fs_file_delete(AIOT_CANLOG_FILE_NAME);
                } 
                super_cmd_can_log_cfg.bit.record_status = CAN_LOG_STATUS_NONE;
                super_cmd_can_log_cfg.bit.store_ctrl = CAN_LOG_STORE_INVALID;
                ESP_LOGW(TAG, "AIoT_Can_Log_Process : DELETE!");
            } else {
                // 无效命令，还原备份命令
                return back_cmd_cfg.reg_val;
            }
            break;

        // CAN log 空命令
        case CAN_LOG_STORE_INVALID :         
            // 本次存储完成
            if ( CAN_LOG_STATUS_FINISHED == back_cmd_cfg.bit.record_status 
                || CAN_LOG_STATUS_RECORDING == back_cmd_cfg.bit.record_status ) {
                // 清空缓存
                start_time = 0;
                AIoT_Can_Log_Save_Slice_Index = 0;
                AIoT_Can_Log_Push_Slice_Index = 0;
                iot_free(AIoT_Can_Log_Push_Slice.slice_ptr);
                
                // 释放资源
                AIoT_CanLog_Deinit();
                ESP_LOGW(TAG, "AIoT_Can_Log_Process : FINISHED!");
            }
            
            if (fs_file_exists(AIOT_CANLOG_FILE_NAME)) {
                super_cmd_can_log_cfg.bit.record_status = CAN_LOG_STATUS_VALID;
            } else {
                super_cmd_can_log_cfg.bit.record_status = CAN_LOG_STATUS_NONE;
            }
            
            break;
            
        default:
            // 无效命令，还原备份命令
            return back_cmd_cfg.reg_val;
    }

    // 当前命令备份至本地
    back_cmd_cfg.reg_val = super_cmd_can_log_cfg.reg_val;
    
    return super_cmd_can_log_cfg.reg_val;
}
