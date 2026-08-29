/**
  ******************************************************************************
  * @file      mqtt_log.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/8/5
  * @brief     MQTT登录日志模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/8/5   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */
 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "mqtt_log.h"
#include "comm_define.h"
#include "utils.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[Mqtt_log]"

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE

/* =============================== 文件内全局变量 ================================ */

// 记录暂存队列
static QueueHandle_t xQueue_mqtt_log = NULL;

// 当前文件头本地缓存
static USE_EXT_RAM_BSS mqtt_log_file_header_t Mqtt_Log_File_Header = {0};

// 记录操作互斥信号量
static SemaphoreHandle_t xMqttLogSemaphore = NULL;

// 记录临时存储区
static USE_EXT_RAM_BSS mqtt_conn_fail_log_t Mqtt_Log_Buffer[MQTT_LOG_PR0CESS_MAX_NUM] = {0};

// 记录临时存储序号
static uint16_t Mqtt_Log_Buffer_Index = 0;

/* ============================== 文件内引用外部接口 ============================ */

// 获取WIFI信号
extern int8_t iot_wifi_ap_rssi(void);

// 获取4G信号强度
extern int8_t iot_4g_rssi(void);

/* ================================ 模块函数定义 ================================ */

/* 简单封装：获取/释放互斥 */
static inline BaseType_t MqttLog_TakeMutex(TickType_t timeout_ms)
{
    if (!xMqttLogSemaphore) return pdFALSE;
    return xSemaphoreTake(xMqttLogSemaphore, pdMS_TO_TICKS(timeout_ms));
}
static inline void MqttLog_GiveMutex(void)
{
    if (xMqttLogSemaphore) xSemaphoreGive(xMqttLogSemaphore);
}

/**
 * @brief      【辅助函数】将记录块写入日志文件。
 * @details    封装了对 fs_record_write 的调用和错误处理，以减少主函数中的代码重复。
 *
 * @param[in]  path          文件路径。
 * @param[in]  records       指向待写入记录数组的指针。
 * @param[in]  start_offset  文件内记录区的起始物理地址。
 * @param[in]  record_index  写入的目标记录索引 (或 FILE_APPEND_MODE)。
 * @param[in]  num_to_write  本次要写入的记录数量。
 *
 * @return     int 0表示成功, -1表示失败。
 */
static int Mqtt_Log_Write(const char *path, void *records, uint32_t start_offset, uint16_t record_index, uint16_t num_to_write)
{
    if (num_to_write == 0) {
        return 0; // 没有记录要写入，直接返回成功。
    }
    
    // 调用文件系统写函数，并使用本模块正确的记录长度宏
    if (fs_record_write(path, records, start_offset, record_index, MQTT_LOG_LEN, num_to_write) != 0) {
        ESP_LOGE(TAG, "fs_record_write for mqtt log failed! index: %u, count: %u", record_index, num_to_write);
        return -1;
    }
    
    return 0;
}

/**
  * @brief      MQTT登录日志模块初始化
  * @note       建议在系统上电后、相关任务开始前执行。
  * @param[in]  void
  * @param[out] None
  * @return
  *  - 0: 成功
  *  - -1: 失败，通常是由于资源（信号量、队列）创建失败或文件系统错误。
  */
int Mqtt_Log_Module_Init(void)
{
#if 0   // 引用其他文件时可选择于该文件内实现，这里不定义
    // 文件操作信号量
    if (xFileSemaphore == NULL) { 
        xFileSemaphore = xSemaphoreCreateMutex();
        if (xFileSemaphore == NULL) {
            ESP_LOGE(TAG, "xFileSemaphore Create failed");
            return -1;
        }
    }
#endif
    
    // 文件操作互斥信号量, 防止多任务同时访问文件
    if (xMqttLogSemaphore == NULL) { 
        xMqttLogSemaphore = xSemaphoreCreateMutex();
        if (xMqttLogSemaphore == NULL) {
            ESP_LOGE(TAG, "xMqttLogSemaphore Create failed");
            return -1;
        }
    }

    // 记录暂存队列
    if (xQueue_mqtt_log == NULL) { 
        // 假设队列项为指向记录的指针
        xQueue_mqtt_log = xQueueCreate(8, sizeof(mqtt_conn_fail_log_pointer_t));
        if (xQueue_mqtt_log == NULL) {
            ESP_LOGE(TAG, "xQueue_mqtt_log Create failed");
            return -1;
        }
    }

    // 尝试读取文件头到本地缓存
    int bytes_read = fs_file_read_at(MQTT_LOG_FILE_NAME, 0, (uint8_t*)&Mqtt_Log_File_Header, sizeof(mqtt_log_file_header_t));
    
    // 检查读取是否成功，以及文件头内容是否有效
    // (例如，max_records 应该等于预设的最大值)
    if (bytes_read != MQTT_LOG_FILE_HEADER_LEN 
        || Mqtt_Log_File_Header.record_ver != MQTT_LOG_FILE_VER
        || Mqtt_Log_File_Header.max_records != MQTT_LOG_MAX_NUM) 
    {
        ESP_LOGW(TAG, "Mqtt log header read failed or is invalid. Initializing a new one.");

        // 清理已创建的日志文件。
        if (fs_file_exists(MQTT_LOG_FILE_NAME)) {
            fs_file_delete(MQTT_LOG_FILE_NAME);
        }

        // 初始化一个新的文件头
        Mqtt_Log_File_Header.record_ver = MQTT_LOG_FILE_VER;
        Mqtt_Log_File_Header.max_records = MQTT_LOG_MAX_NUM;
        Mqtt_Log_File_Header.current_records = 0;
        Mqtt_Log_File_Header.write_index = 0;

        // 将新的文件头写入文件
        if (fs_record_write(MQTT_LOG_FILE_NAME, &Mqtt_Log_File_Header, 0, 0, MQTT_LOG_FILE_HEADER_LEN, 1) == 0) {
            ESP_LOGI(TAG, "New Mqtt access log header initialized and written successfully.");
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write new Mqtt access log header!");
            return -1; // 写入失败是致命错误，必须终止
        }
    }
    
    return 0;
}

/**
 * @brief      根据传入的参数生成一条MQTT事件记录。
 * @details    此函数负责动态分配内存、填充记录内容，并将其指针发送到处理队列。
 *             函数内部处理了线程安全和内存管理。
 *
 * @param[in]  params  一个指向 `mqtt_conn_fail_log_t` 结构体的常量指针，
 *                     该结构体封装了生成日志所需的所有参数。
 *
 * @return
 *         - 0: 成功，记录已成功创建并尝试发送到队列。
 *         - -1: 失败，可能由于参数错误、内存分配失败或队列发送失败。
 */
int Mqtt_Log_Generate(const mqtt_conn_fail_log_t *params)
{
    static uint32_t curr_log_id = 0;
    
    // 1. 参数有效性检查
    if (params == NULL) {
        ESP_LOGE(TAG, "Mqtt_Log_Generate received null params.");
        return -1;
    }

    int ret = 0;
    mqtt_conn_fail_log_t *new_record = NULL;

    // 2. 动态分配内存来存储一条新记录
    new_record = (mqtt_conn_fail_log_t *)iot_calloc(sizeof(mqtt_conn_fail_log_t));
    if (new_record == NULL) {
        ESP_LOGE(TAG, "mqtt_conn_fail_log_t data calloc failed.");
        ret = -1;
        goto exit_point;
    }
    
    // 3. 填充记录内容
    memcpy(new_record, params, sizeof(mqtt_conn_fail_log_t));
    new_record->log_id = (++curr_log_id);
    new_record->fail_timestamp = time(NULL);
    if ( NETIF_TYPE_WIFI_STA == new_record->network_type ) {
        new_record->rssi = iot_wifi_ap_rssi();
    } else if ( NETIF_TYPE_PPP == new_record->network_type ) {
        new_record->rssi = iot_4g_rssi();
    }
    
    // 打印生成的完整日志记录信息
    ESP_LOGD(TAG, "Mqtt log generated (ID: %lu, Time: %llu, Uptime: %u sec, NetType: %u, RSSI: %d dBm, Reason: %u, ErrNo: %ld, RetCode: %u, UseMQTTS: %u, Password: %llu, RetryDelay: %u ms)",
             new_record->log_id,
             new_record->fail_timestamp,
             new_record->uptime_seconds,
             new_record->network_type,
             new_record->rssi,
             new_record->reason,
             new_record->sys_errno,
             new_record->connect_return_code,
             new_record->use_mqtts,
             new_record->password,
             new_record->retry_delay_ms);
    
    // 4. 创建指针包装结构体
    mqtt_conn_fail_log_pointer_t record_ptr_wrapper;
    record_ptr_wrapper.p_record = new_record;
    
    // 5. 将包装结构体（包含指针）发送到队列
    if (xQueue_mqtt_log && xQueueSend(xQueue_mqtt_log, &record_ptr_wrapper, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Mqtt_Log_Generate send to queue failed.");
        // 发送失败，必须释放刚刚分配的内存以防泄漏
        free(new_record);
        new_record = NULL; // 防止悬空指针
        ret = -1;
    } 

exit_point:

    return ret;
}

/**
 * @brief      从队列中批量消费日志，并以循环方式写入文件。
 * @details    此函数是设备接入日志模块的“消费者”核心。它被设计为一个周期性执行的任务。
 *             其主要工作流程如下：
 *             1. **批量拉取**: 非阻塞地从 `xQueue_mqtt_log` 队列中拉取最多
 *                `MQTT_LOG_PR0CESS_MAX_NUM` 条记录，暂存到本地数组中。
 *                这样做是为了将多次零散的文件写入合并为一次或两次批量写入，以提升I/O性能。
 *             2. **内存管理**: 将队列中指针指向的数据复制到本地后，立即释放生产者分配的内存，
 *                防止内存泄漏。
 *             3. **分块计算**: 根据文件当前状态（是否已满），计算出本次批量写入需要被拆分的
 *                两个部分（`cnt1` 和 `cnt2`）。`cnt1` 是填满当前空间的部分，`cnt2` 是
 *                回环覆盖到文件开头的部分。
 *             4. **循环写入**: 调用辅助函数 `Mqtt_Log_Write`，最多执行两次写入操作，
 *                以完成 `cnt1` 和 `cnt2` 数据的持久化。
 *             5. **文件头更新**: 根据 `cnt1` 和 `cnt2` 的计算结果，精确地更新文件头信息
 *                （`current_records` 和 `write_index`），以保证状态的正确性。
 *             6. **状态持久化**: 将更新后的文件头写回磁盘。
 */
void Mqtt_Log_Save_Task(void)
{
    // 等待任务延时间隔
	static uint32_t pre_time = 0;

    // 获取文件操作互斥锁，保护整个操作过程
    // 避免获取锁失败导致本次拉取到的记录丢失
    if (MqttLog_TakeMutex(100) != pdTRUE) {
        ESP_LOGD(TAG, "Save_Task failed to take file process semaphore. Records will be discarded.");
        return;
    }

    // =================================================================================
    // 阶段 1: 从队列中批量拉取数据
    // =================================================================================
    // 循环拉取，直到缓冲区满或队列为空。
    while ( Mqtt_Log_Buffer_Index < MQTT_LOG_PR0CESS_MAX_NUM )
    {
        mqtt_conn_fail_log_pointer_t received_ptr_wrapper = { .p_record = NULL };
        
        // 以非阻塞方式（超时时间为0）尝试从队列接收一条记录。
        if (xQueue_mqtt_log && xQueueReceive(xQueue_mqtt_log, &received_ptr_wrapper, 0) == pdPASS) {
            // 成功接收到记录指针。
            if ( received_ptr_wrapper.p_record != NULL ) {
                // 将指针指向的动态内存中的数据，复制到本地栈上的缓冲区中。
                memcpy(&Mqtt_Log_Buffer[Mqtt_Log_Buffer_Index], received_ptr_wrapper.p_record, sizeof(mqtt_conn_fail_log_t));
                // 关键：立即释放生产者任务分配的内存，防止内存泄漏。
                free(received_ptr_wrapper.p_record);    
                Mqtt_Log_Buffer_Index++;
            }
        } else {
            // 队列已空，没有更多可获取的新记录，退出拉取循环。
            break;
        }
    }

    // =================================================================================
    // 阶段 2: 将缓冲区中的数据持久化到文件
    // =================================================================================
    // 仅当目前拉取到了至少一条记录且距离上次写入超过指定时间，或当前缓冲区已满，才执行文件操作。
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (( Mqtt_Log_Buffer_Index >= MQTT_LOG_PR0CESS_MAX_NUM ) 
        || ((Mqtt_Log_Buffer_Index > 0) && ((0 == pre_time) || (now_time - pre_time > MQTT_LOG_SAVE_TIMEOUT))))
    {
        // 检查文件是否已满，分为“首次填充”和“循环覆盖”两种主要情况。
        if ( Mqtt_Log_File_Header.current_records < Mqtt_Log_File_Header.max_records ) {
            // --- 情况 A: 文件尚未写满 (首次填充阶段) ---
            uint16_t space_left = Mqtt_Log_File_Header.max_records - Mqtt_Log_File_Header.current_records;
            uint16_t cnt1 = (Mqtt_Log_Buffer_Index > space_left) ? space_left : Mqtt_Log_Buffer_Index;
            uint16_t cnt2 = Mqtt_Log_Buffer_Index - cnt1;
            
            // 第1部分追加写入到文件末尾
            if (Mqtt_Log_Write(MQTT_LOG_FILE_NAME, &Mqtt_Log_Buffer[0], MQTT_LOG_START_ADDR, FILE_APPEND_MODE, cnt1) != 0) goto save_exit;
            
            // 第2部分（如果本次写入后，文件将会被填满并溢出时存在）从文件记录区的开头开始覆盖写入
            if (Mqtt_Log_Write(MQTT_LOG_FILE_NAME, &Mqtt_Log_Buffer[cnt1], MQTT_LOG_START_ADDR, 0, cnt2) != 0) goto save_exit;
            
            // 更新文件头
            if (cnt2 > 0) {
                // 当前记录数达到最大值，写入点索引更新为本次溢出覆盖的记录数。
                Mqtt_Log_File_Header.current_records = Mqtt_Log_File_Header.max_records;
                Mqtt_Log_File_Header.write_index = cnt2;
            } else {
                // 增加当前记录数，写入点索引在填充阶段无意义，保持为0。
                Mqtt_Log_File_Header.current_records += cnt1;
                Mqtt_Log_File_Header.write_index = 0;
            }
        } else {
            // --- 情况 B: 文件已满 (循环覆盖阶段) ---
            uint16_t space_to_end = Mqtt_Log_File_Header.max_records - Mqtt_Log_File_Header.write_index;
            uint16_t cnt1 = (Mqtt_Log_Buffer_Index > space_to_end) ? space_to_end : Mqtt_Log_Buffer_Index;
            uint16_t cnt2 = Mqtt_Log_Buffer_Index - cnt1;

            // 第1部分从当前索引开始覆盖写入
            if (Mqtt_Log_Write(MQTT_LOG_FILE_NAME, &Mqtt_Log_Buffer[0], MQTT_LOG_START_ADDR, Mqtt_Log_File_Header.write_index, cnt1) != 0) goto save_exit;
            
            // 第2部分（如果本次写入会跨越末尾时存在）从文件记录区的开头继续覆盖写入
            if (Mqtt_Log_Write(MQTT_LOG_FILE_NAME, &Mqtt_Log_Buffer[cnt1], MQTT_LOG_START_ADDR, 0, cnt2) != 0) goto save_exit;

            // 更新文件头
            if (cnt2 > 0) {
                // 本次写入跨越末尾，写入点索引更新为第二部分写入的记录数。
                Mqtt_Log_File_Header.write_index = cnt2;
            } else {
                // 本次写入未跨越末尾，写入点索引向后移动。如果恰好到达末尾，则归零
                Mqtt_Log_File_Header.write_index += cnt1;
                if (Mqtt_Log_File_Header.write_index == Mqtt_Log_File_Header.max_records) {
                    Mqtt_Log_File_Header.write_index = 0;
                }
            }
        }

        // =================================================================================
        // 阶段 3: 将更新后的文件头写回文件
        // =================================================================================
        // 无论走了哪个逻辑分支，最后都需要将内存中更新过的文件头持久化。
        if (fs_record_write(MQTT_LOG_FILE_NAME, &Mqtt_Log_File_Header, 0, 0, MQTT_LOG_FILE_HEADER_LEN, 1) != 0) {
            ESP_LOGE(TAG, "Mqtt log File Header update failed!");
            // 注意：即使文件头更新失败，记录本身已写入，这里只记录错误日志而不影响下次运行。
        } else {
            ESP_LOGI(TAG, "Mqtt log updated. Records added: %u, Current total: %u, Next index: %u", 
                     Mqtt_Log_Buffer_Index, Mqtt_Log_File_Header.current_records, Mqtt_Log_File_Header.write_index);
        }

        // 清空缓存
        pre_time = now_time;
        memset(Mqtt_Log_Buffer, 0, sizeof(Mqtt_Log_Buffer));
        Mqtt_Log_Buffer_Index = 0;
    }
    
 save_exit:

    // 释放文件操作互斥锁
    MqttLog_GiveMutex();
}

/**
 * @brief      【辅助函数】计算晚于指定时间戳的新MQTT连接日志数量 (二分查找优化版)。
 * @details    此函数使用二分查找算法来快速定位第一个新记录的位置，
 *             从而避免在大量记录中进行耗时的线性扫描。
 *             它同样包含快速路径优化，用于处理全量同步的场景。
 *
 * @param[in]  since_timestamp  一个UNIX时间戳。
 *
 * @return     int
 *             - >=0 : 新记录数量
 *             - -1  : 参数错误或其它不可恢复错误
 */
static int Mqtt_Log_Count_New_Records(time_t since_timestamp)
{
    // 1. 基本参数和路径检查
    if (Mqtt_Log_File_Header.max_records == 0 || Mqtt_Log_File_Header.current_records == 0) {
        return 0; // 模块未初始化或文件为空，没有新记录
    }

    // 2. 快速路径检查：处理全量同步的情况
    if (since_timestamp == 0) {
        ESP_LOGD(TAG, "since_timestamp is 0, counting all Mqtt access records.");
        return Mqtt_Log_File_Header.current_records;
    }

    // 3. 快速路径检查：与最旧和最新的记录比较
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (Mqtt_Log_File_Header.current_records < Mqtt_Log_File_Header.max_records) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = Mqtt_Log_File_Header.write_index;
    }

    mqtt_conn_fail_log_t temp_record;
    uint32_t record_offset = MQTT_LOG_START_ADDR + oldest_record_p_idx * MQTT_LOG_LEN;
    if (fs_file_read_at(MQTT_LOG_FILE_NAME, record_offset, (uint8_t*)&temp_record, MQTT_LOG_LEN) == MQTT_LOG_LEN) {
        // 如果 since_timestamp 早于文件中最旧的记录，则所有记录都是新的
        if (since_timestamp < temp_record.fail_timestamp) {
            ESP_LOGD(TAG, "since_timestamp is older than the oldest record, counting all records.");
            return Mqtt_Log_File_Header.current_records;
        }
    } else {
        ESP_LOGE(TAG, "Failed to read oldest record at index %u for pre-check.", oldest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // 在逻辑上，最新的记录是第 (current_records - 1) 条。
    uint16_t newest_record_l_idx = Mqtt_Log_File_Header.current_records - 1;
    // 其物理索引 = (最旧记录的物理索引 + 最新记录的逻辑索引) % 数组总容量
    uint16_t newest_record_p_idx = (oldest_record_p_idx + newest_record_l_idx) % Mqtt_Log_File_Header.max_records;
    
    record_offset = MQTT_LOG_START_ADDR + newest_record_p_idx * MQTT_LOG_LEN;
    if (fs_file_read_at(MQTT_LOG_FILE_NAME, record_offset, (uint8_t*)&temp_record, MQTT_LOG_LEN) == MQTT_LOG_LEN) {
        // 如果 since_timestamp 晚于文件中最新的记录，则所有记录都是旧的
        if (since_timestamp >= temp_record.fail_timestamp) {
            ESP_LOGD(TAG, "since_timestamp is newer than or equal to the newest record, no new records.");
            return 0;
        }
    } else {
        ESP_LOGE(TAG, "Failed to read newest record at index %u for pre-check.", newest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // --- 4. 二分查找：定位第一个新记录 ---
    // 在逻辑索引 [0, current_records - 1] 上进行二分查找。
    int low = 0, high = Mqtt_Log_File_Header.current_records - 1;
    int first_new_l_idx = -1; // 第一个新记录的逻辑索引

    while (low <= high) {
        int mid_l_idx = low + (high - low) / 2; // 中间点的逻辑索引

        // 将逻辑索引转换为物理文件索引
        // 当前物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
        uint16_t mid_p_idx = (oldest_record_p_idx + mid_l_idx) % Mqtt_Log_File_Header.max_records;
        
        record_offset = MQTT_LOG_START_ADDR + mid_p_idx * MQTT_LOG_LEN;
        if (fs_file_read_at(MQTT_LOG_FILE_NAME, record_offset, (uint8_t*)&temp_record, MQTT_LOG_LEN) != MQTT_LOG_LEN) {
            ESP_LOGE(TAG, "Binary search failed to read record at physical index %u", mid_p_idx);
            return 0; // 查找失败
        }

        if (temp_record.fail_timestamp > since_timestamp) {
            // 这是一个新记录。
            // 找到了一个潜在的边界，记录它，并尝试在更早的部分（左半区）寻找更早的新记录。
            first_new_l_idx = mid_l_idx;
            high = mid_l_idx - 1;
        } else {
            // 这是一个旧记录。
            // 在更晚的部分（右半区）寻找新记录。
            low = mid_l_idx + 1;
        }
    }

    // 5. 计算并返回结果
    if (first_new_l_idx != -1) {
        // 新记录的数量 = 总记录数 - 第一个新记录的逻辑索引
        return Mqtt_Log_File_Header.current_records - first_new_l_idx;
    }

    return 0; // 没有找到任何新记录
}

/*------------------------------------------------------------------------------
 Function: Mqtt_Log_Read_Incremental
 -----------------------------------------------------------------------------
*/
/**
 * @brief      读取增量MQTT连接日志到动态分配的缓冲区。
 * @details    此函数是实现日志增量同步的核心接口。它会：
 *             1. 调用辅助函数 `Mqtt_Log_Count_New_Records` 计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *             5. 注意，返回的缓冲区大小取决于MQTT_LOG_MAX_NUM，
 *                512条时可达32k，自行评估使用。
 *
 * @param[in]  since_timestamp  一个UNIX时间戳，用于界定"新"记录。
 * @param[out] out_buffer       一个指向 `uint8_t*` 的指针。如果函数成功，
 *                              它将被设置为新分配的缓冲区的地址。
 * @param[out] out_size         一个指向 `uint32_t` 的指针。如果函数成功，
 *                              它将被设置为缓冲区的总大小（字节）。
 *
 * @return     int
 *             - 0: 成功 (即使没有新记录也返回成功)。
 *             - -1: 失败（如内存分配失败、文件读取失败等）。
 *
 * @note       **重要**: 调用者在处理完 `out_buffer` 中的数据后，
 *             **必须**负责调用 `free(out_buffer)` 来释放内存。
 */
int Mqtt_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size)
{
    // 1. 参数校验
    if (out_buffer == NULL || out_size == NULL) {
        return -1;
    }
    
    // 获取文件操作互斥锁，保护整个读操作过程
    if (MqttLog_TakeMutex(100) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take file process semaphore for reading.");
        return -1;
    }

    int ret = 0;
    uint8_t *buffer = NULL;
    
    *out_buffer = NULL;
    *out_size = 0;

    // 2. 调用辅助函数，获取新记录的数量
    uint16_t new_records_count = Mqtt_Log_Count_New_Records(since_timestamp);

    // 如果没有新记录，则无需分配内存和读取，直接返回成功
    if (new_records_count == 0) {
        ESP_LOGW(TAG, "No new Mqtt access records found since timestamp %lld.", (long long)since_timestamp);
        goto exit_point; 
    }

    ESP_LOGI(TAG, "Found %u new Mqtt access records since timestamp %lld. Preparing to read.", new_records_count, (long long)since_timestamp);

    // 3. 计算总大小并分配内存
    uint32_t total_size = MQTT_LOG_FILE_HEADER_LEN + new_records_count * MQTT_LOG_LEN;
    buffer = (uint8_t *)iot_calloc(total_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes for incremental log.", total_size);
        ret = -1;
        goto exit_point;
    }

    // 4. 读取文件头到缓冲区的起始位置
    if (fs_file_read_at(MQTT_LOG_FILE_NAME, 0, buffer, MQTT_LOG_FILE_HEADER_LEN) != MQTT_LOG_FILE_HEADER_LEN) {
        ESP_LOGE(TAG, "Failed to read file header for incremental sync.");
        ret = -1;
        goto exit_point;
    }

    // 5. 读取所有新记录到缓冲区
    //    首先，找到文件中第一个新记录的物理位置
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (Mqtt_Log_File_Header.current_records < Mqtt_Log_File_Header.max_records) {
        oldest_record_p_idx = 0;
    } else {
        oldest_record_p_idx = Mqtt_Log_File_Header.write_index;
    }
    
    // 第一个新记录的逻辑索引 = 总记录数 - 新记录数
    uint16_t first_new_l_idx = Mqtt_Log_File_Header.current_records - new_records_count;
    // 第一个新记录的物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
    uint16_t first_new_p_idx = (oldest_record_p_idx + first_new_l_idx) % Mqtt_Log_File_Header.max_records;

    // 分一或两块，将所有新记录顺序读入缓冲区
    uint8_t *record_write_ptr = buffer + MQTT_LOG_FILE_HEADER_LEN;
    uint16_t space_to_end = Mqtt_Log_File_Header.max_records - first_new_p_idx;

    if (new_records_count <= space_to_end) {
        // 情况A: 所有新记录都在一个连续的块中
        uint32_t read_offset = MQTT_LOG_START_ADDR + first_new_p_idx * MQTT_LOG_LEN;
        if (fs_file_read_at(MQTT_LOG_FILE_NAME, read_offset, record_write_ptr, new_records_count * MQTT_LOG_LEN) != new_records_count * MQTT_LOG_LEN) {
            ESP_LOGE(TAG, "Failed to read continuous block of new records.");
            ret = -1;
            goto exit_point;
        }
    } else {
        // 情况B: 新记录跨越了物理文件的末尾，需要分两次读取
        uint16_t cnt1 = space_to_end;
        uint16_t cnt2 = new_records_count - cnt1;

        // 读取第一部分 (从 first_new_p_idx 到文件末尾)
        uint32_t read_offset1 = MQTT_LOG_START_ADDR + first_new_p_idx * MQTT_LOG_LEN;
        if (fs_file_read_at(MQTT_LOG_FILE_NAME, read_offset1, record_write_ptr, cnt1 * MQTT_LOG_LEN) != cnt1 * MQTT_LOG_LEN) {
            ESP_LOGE(TAG, "Failed to read first part of wrapped records.");
            ret = -1;
            goto exit_point;
        }

        // 读取第二部分 (从文件开头到剩余记录结束)
        uint32_t read_offset2 = MQTT_LOG_START_ADDR; // 从记录区开头读
        if (fs_file_read_at(MQTT_LOG_FILE_NAME, read_offset2, record_write_ptr + cnt1 * MQTT_LOG_LEN, cnt2 * MQTT_LOG_LEN) != cnt2 * MQTT_LOG_LEN) {
            ESP_LOGE(TAG, "Failed to read second part of wrapped records.");
            ret = -1;
            goto exit_point;
        }
    }

    // 6. 成功，返回缓冲区地址和大小
    *out_buffer = buffer;
    *out_size = total_size;

exit_point:
    // 统一的出口：处理资源释放
    if (ret != 0 && buffer != NULL) {
        // 如果函数执行失败，且内存已分配，则释放内存
        free(buffer);
        *out_buffer = NULL; // 确保外部不会使用悬空指针
        *out_size = 0;
    }

    // 释放文件操作互斥锁
    MqttLog_GiveMutex();
    
    return ret;
}

/*------------------------------------------------------------------------------
 Function: Is_Mqtt_Log_File
 -----------------------------------------------------------------------------
*/
/**
 * @brief      判断传入的文件标识符是否为当前的MQTT连接日志文件。
 * @details    此函数用于验证外部请求的文件是否为本模块管理的日志文件。
 *             它会根据传入的参数构建一个完整的路径，并与模块内部生成的
 *             标准路径进行比较。
 *
 * @param[in]  fname  一个指向数据通道文件（自定义格式）参数的指针。
 *                    - fname[0]: 文件夹层级。
 *                    - &fname[1]: 文件名字符串。
 *
 * @return     int
 *             - 0: 是日志文件。
 *             - -1: 不是日志文件或参数错误。
 */
int Is_Mqtt_Log_File(const char *fname)
{
    // 1. 参数校验
    if (fname == NULL) {
        return -1;
    }

    // 2. 根据传入的参数构建请求的文件路径
    //    fname[0] 是目录, &fname[1] 是文件名
    char request_path[40] = {0};
    char filedir = fname[0];
    const char* filename = &fname[1]; // 使用 const char* 更安全

    if ( (MQTT_LOG_FOLDER_LEVEL1)[0] != filedir ) {
        // 目录不匹配，不是目标文件
        return -1;
    }
    
    // 使用 snprintf 安全地构建路径，防止溢出
    snprintf(request_path, sizeof(request_path), "%s/%s%s", FS_BASE_PATH, MQTT_LOG_FOLDER_LEVEL1, filename);

    // 3. 使用与日志模块内部完全相同的宏来生成标准文件名
    char local_log_path[40] = {0};
    snprintf(local_log_path, sizeof(local_log_path), "%s", MQTT_LOG_FILE_NAME);

    // 4. 比较两个路径字符串
    if (strcmp(request_path, local_log_path) == 0) {
        // 两个字符串完全相同，确认是目标日志文件
        return 0;
    }

    // 字符串不匹配，不是目标文件
    return -1;
}

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符（MQTT_LOG_FOLDER_LEVEL1）
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Mqtt_Log_File_Id(char *out_buf, size_t buf_len)
{
    if (out_buf == NULL || buf_len < 2) {
        return -1;
    }

    char local_log_path[30] = {0};
    snprintf(local_log_path, sizeof(local_log_path), "%s", MQTT_LOG_FILE_NAME); // 生成完整路径

    const char *folder_char = (MQTT_LOG_FOLDER_LEVEL1 && MQTT_LOG_FOLDER_LEVEL1[0]) ?
                                 &MQTT_LOG_FOLDER_LEVEL1[0] : NULL;
    if (folder_char == NULL) {
        return -1;
    }

    // 提取文件名部分（最后一个 '/' 之后）
    const char *p = strrchr(local_log_path, '/');
    const char *filename = p ? p + 1 : local_log_path;
    size_t fnlen = strlen(filename);

    if (fnlen + 1 > buf_len) { // 1 字节 folder + fnlen
        return -1;
    }

    out_buf[0] = folder_char[0];
    memcpy(out_buf + 1, filename, fnlen); // 不包含终止符

    return 0;
}

/**
 * @brief 统计晚于指定时间戳的日志新记录数（含信号量保护）。
 *
 * @param[in] since_timestamp  自 Unix 纪元起的时间戳；传 0 表示统计全部记录。
 *
 * @return int
 *         - >=0 : 新记录数量
 *         - -1  : 参数错误或其它不可恢复错误
 *         - -2  : 获取文件处理信号量失败（记录将被丢弃）
 */
int Is_Mqtt_Log_Count_New(time_t since_timestamp)
{
    uint16_t new_count = 0;

    /* 获取信号量保护 */
    if (MqttLog_TakeMutex(100) != pdTRUE) {
        ESP_LOGE(TAG, "failed to take file process semaphore. Records will be discarded.");
        return -2;
    }

    /* 统计晚于 since_timestamp 的新记录数量（内部处理文件读错时返回0） */
    new_count = Mqtt_Log_Count_New_Records(since_timestamp);

    /* 释放信号量 */
    MqttLog_GiveMutex();

    return (int)new_count;
}

#endif
