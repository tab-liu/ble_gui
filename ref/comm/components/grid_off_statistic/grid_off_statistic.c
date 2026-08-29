/**
  ******************************************************************************
  * @file      grid_off_statistic.c
  * @version   1.0
  * @author    zdm
  * @date      2024
  * @brief     电网断电次数统计模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024       <td>1.0     <td>Generated  <td>
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "grid_off_statistic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

// 引用外部依赖项
#include "can_data.h"
#include "inv_define.h"
#include "can_protocol.h"
#include "parameter.h"
#include "http_client.h"
#include "filesystem.h"
#include "crc.h"
#include "utils.h"

static const char *TAG = "[grid_off_statistic]";

#ifdef CONFIG_GRID_OFF_COUNT_ENABLE

// 防抖配置常量
#define GRID_DEBOUNCE_TIME_MS    1000   // 防抖时间：1秒

// 周期上报配置常量
#define GRID_OFF_BATCH_REPORT_INTERVAL_MS   (60 * 60 * 1000)   // 批量上报检查间隔：60分钟，有事件发生立即上报
#define SYNC_DELAY_MS                       (60 * 1000)        // 首次同步延迟时间：1分钟
#define SYNC_PERIODIC_INTERVAL_MS           (60 * 60 * 1000)   // 周期同步间隔：1小时
#define GRID_OFF_DEBUG_PRINT_INTERVAL_MS    (60 * 1000)        // 每1分钟打印队列状态

// 模块内部静态变量
static uint8_t s_last_grid_state = 1;      // 上次电网状态，默认在线
USE_EXT_RAM_BSS static power_off_queue_t s_power_off_queue = {0}; // 断电事件队列
static uint32_t s_last_batch_report_time = 0;    // 上次批量上报的时间
static bool s_fast_report_flag = false;           // 是否需要立即上报
static bool s_first_sync_done = false;           // 是否已完成首次同步
static uint32_t s_last_periodic_sync_time = 0;   // 上次周期同步时间
static bool s_queue_needs_save = false;          // 队列是否需要保存到Flash

/* 信号量（互斥） */
static SemaphoreHandle_t xGridOffCountMutex = NULL;

// 内部静态函数声明
static uint8_t grid_state_debounce(uint8_t current_state);
static time_span_check_result_t Check_Time_Span_Change(uint64_t last_time, uint64_t current_time);
static bool execute_grid_off_sync(void);
static void check_and_execute_sync(uint32_t current_time_ms);
static void complete_incomplete_event(uint64_t current_time);
static int grid_off_queue_add_event(uint64_t start_time, uint64_t end_time);
static int grid_off_queue_update_event_end_time(int index, uint64_t end_time);
static int grid_off_queue_batch_report(void);
static int Grid_Off_Find_Incomplete_Event(void);
static int Grid_Off_Get_Unreported_Count(void);
static int Grid_Off_Save_Queue_To_Flash(void);
static int Grid_Off_Load_Queue_From_Flash(void);
static uint32_t Grid_Off_Calculate_Queue_Checksum(const power_off_queue_t *queue);
static bool Grid_Off_Verify_Queue_Integrity(const power_off_queue_t *queue);
static int Grid_Off_Write_Queue_File(const char *filename, const void *data, size_t size);
static int Grid_Off_Read_Queue_File(const char *filename, void *data, size_t size);
static void Grid_Off_Debug_Print_Queue(void);

/* 简单封装：获取/释放互斥 */
static inline BaseType_t xGridOffCountMutex_TakeMutex(TickType_t timeout_ms)
{
    if (!xGridOffCountMutex) return pdFALSE;
    return xSemaphoreTake(xGridOffCountMutex, pdMS_TO_TICKS(timeout_ms));
}
static inline void xGridOffCountMutex_GiveMutex(void)
{
    if (xGridOffCountMutex) xSemaphoreGive(xGridOffCountMutex);
}

/**
 * @brief 电网状态防抖处理函数
 * @param current_state 当前电网状态
 * @return 防抖后的稳定状态
 */
static uint8_t grid_state_debounce(uint8_t current_state)
{
    static uint32_t debounce_start_time = 0; // 防抖开始时间（0表示未激活）
    static uint8_t debounce_state = 0xFF;       // 防抖状态，默认在线
    static uint8_t last_stable_state = 0xFF;     // 上次稳定状态，默认在线
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // 状态变化时，重新开始防抖
    if (current_state != debounce_state) {
        debounce_start_time = current_time;
        debounce_state = current_state;
        ESP_LOGD(TAG, "Grid state change detected, starting debounce: %d", current_state);
        return last_stable_state; // 返回上次稳定状态
    }
    
    // 防抖时间到达，确认状态变化
    if (debounce_start_time > 0 && (current_time - debounce_start_time) >= GRID_DEBOUNCE_TIME_MS) {
        debounce_start_time = 0; // 重置防抖状态
        last_stable_state = debounce_state;
        // ESP_LOGI(TAG, "Grid state debounce completed: %d", debounce_state);
        return debounce_state;
    }
    
    // 防抖期间或状态未变化，返回上次稳定状态
    return last_stable_state;
}

/**
 * @brief 初始化断电统计模块
 */
void Grid_Off_Statistic_Init(void)
{
#if (GRID_OFF_STATISTIC_ENABLE == 0)
    return; // 断电统计功能未使能，直接返回
#endif

    s_last_grid_state = 0xFF; // 初始化电网状态为无效状态

    // 创建互斥信号量用于保护配置/运行态数据；
    if (xGridOffCountMutex == NULL) {
        xGridOffCountMutex = xSemaphoreCreateMutex();
        if (xGridOffCountMutex == NULL) {
            ESP_LOGE(TAG, "create mutex failed");
        }
    }    
    
    // 从文件加载断电事件队列
    if (Grid_Off_Load_Queue_From_Flash() != 0) {
        // 加载失败，初始化新队列
        ESP_LOGW(TAG, "Read power off queue from flash failed, initialized new power off queue");
        if (Grid_Off_Save_Queue_To_Flash() == 0) {
            ESP_LOGI(TAG, "Queue initialized flash successfully");
        } else {
            ESP_LOGE(TAG, "Failed to initialized");
        }
    }
}

/**
 * @brief 按自然时间周期检查时间跨度变化
 * @param last_time 上次时间戳
 * @param current_time 当前时间戳
 * @return 时间跨度检查结果
 */
static time_span_check_result_t Check_Time_Span_Change(uint64_t last_time, uint64_t current_time)
{
    time_span_check_result_t result = {false, false, false, false};
    
    // 如果时间戳相同，所有时间段都相同
    if (last_time == current_time) {
        result.same_year = true;
        result.same_month = true;
        result.same_week = true;
        result.same_day = true;
        return result;
    }
    
    // 转换为本地时间结构
    time_t t_last = (time_t)last_time;
    time_t t_current = (time_t)current_time;
    struct tm tm_last, tm_current;
    tm_last = *localtime(&t_last);
    tm_current = *localtime(&t_current);
    // 直接使用localtime的字段进行自然时间判断
    result.same_year = (tm_last.tm_year == tm_current.tm_year);
    result.same_month = (tm_last.tm_year == tm_current.tm_year && 
                        tm_last.tm_mon == tm_current.tm_mon);
    result.same_day = (tm_last.tm_year == tm_current.tm_year && 
                      tm_last.tm_mon == tm_current.tm_mon && 
                      tm_last.tm_mday == tm_current.tm_mday);
    
    // 自然周判断
    // 计算两个时间点距离各自所在周周一的秒数，如果相同则为同一周
    // 注意：tm_wday中0=周日，需要特殊处理
    int last_days_to_monday = (tm_last.tm_wday == 0) ? 6 : (tm_last.tm_wday - 1);
    int current_days_to_monday = (tm_current.tm_wday == 0) ? 6 : (tm_current.tm_wday - 1);
    // 计算各自所在周的周一日期（年-月-日）
    time_t last_monday = t_last - (last_days_to_monday * 86400);
    time_t current_monday = t_current - (current_days_to_monday * 86400);
    struct tm last_monday_tm, current_monday_tm;
    last_monday_tm = *localtime(&last_monday);
    current_monday_tm = *localtime(&current_monday);
    // 比较周一日期是否相同（年、月、日都相同）
    result.same_week = (last_monday_tm.tm_year == current_monday_tm.tm_year &&
                       last_monday_tm.tm_mon == current_monday_tm.tm_mon &&
                       last_monday_tm.tm_mday == current_monday_tm.tm_mday);
    
    return result;
}

// 断电事件上报检查函数已删除，改用队列批量上报机制

/**
 * @brief 统计电网断电次数主函数
 * @note 仅上电30s后开始运行，需要记录当日，当月，当周，当年和至今的断电次数，需要保存到flash中
 */
void Grid_Off_Event_Task(void)
{
#if (GRID_OFF_STATISTIC_ENABLE == 0)
    return; // 断电统计功能未使能，直接返回
#endif

    static uint8_t init_flag = 0;
    static uint8_t incomplete_event_checked = 0; // 未完成事件检查标志
    
    // 系统时间无效或正在更新，不进行统计
    if (reals.rtc_flag.sBit.Time_valid == 0 || sys_is_updating() == 1) { 
        return;
    }

    // 逆变器离线，此时电网在线标记必为0,不应记录成断电事件
    if(can_node_rd.Inv[reals.Addr_can_self].online == 0) {
        init_flag = 0;
        incomplete_event_checked = 0; // 重置未完成事件检查标志
        return;
    }

    time_t now = time(NULL);

    // 逆变上报电网在线标志
    uint8_t raw_grid_state = (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.line_event.bit.Grid_online == 1) ? 1 : 0;

    // 使用防抖后的状态
    uint8_t current_grid_state = grid_state_debounce(raw_grid_state);
    grid_off_count_statistics_t *stats = &IotSetData.dev_info_t.grid_off_statistics;
    
    if (init_flag == 0) {
        s_last_grid_state = current_grid_state;
        init_flag = 1;
        
        // 初始化统计时间戳
        if (stats->last_update_time == 0) {
            stats->last_update_time = (uint64_t)now;
        }
        
        ESP_LOGI(TAG, "Grid tracking initialized, state: %d", current_grid_state);
        return;
    }
    
    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (xGridOffCountMutex_TakeMutex(10) != pdTRUE) {
        return;
    }
    
    // 检查是否有未完成的断电事件（仅在时间有效且第一次检查时执行）
    if (incomplete_event_checked == 0) {
        incomplete_event_checked = 1;
        
        // 根据当前电网状态决定如何处理
        if (current_grid_state == 1) {
            // 电网已恢复，完成未完成的事件
            complete_incomplete_event((uint64_t)now);
        }
    }
    
    // 检查时间边界重置统计
    if (stats->last_update_time > 0) {
        time_span_check_result_t time_check = Check_Time_Span_Change(stats->last_update_time, (uint64_t)now);
        
        if (!time_check.same_day) stats->daily_count = 0;
        if (!time_check.same_week) stats->weekly_count = 0;
        if (!time_check.same_month) stats->monthly_count = 0;
        if (!time_check.same_year) stats->yearly_count = 0;
        
        if (!time_check.same_day || !time_check.same_week || 
            !time_check.same_month || !time_check.same_year) {
            stats->last_update_time = (uint64_t)now;
            reals.SetDataWrFlag.sBit.grid_off_statistics = 1;
        }
    }
    
    // 检测电网状态变化
    if (s_last_grid_state == 1 && current_grid_state == 0) {
        // 电网断电 - 仅添加到队列，统计计数延迟到断电结束时
        int new_event_index = grid_off_queue_add_event((uint64_t)now, 0);
        if (new_event_index >= 0) {
            ESP_LOGI(TAG, "Grid off event started, added to queue at index %d", new_event_index);
        } else {
            ESP_LOGW(TAG, "Failed to add grid off event start to queue");
        }
        
        ESP_LOGW(TAG, "Grid off detected, waiting for recovery to update statistics");
    } 
    else if (s_last_grid_state == 0 && current_grid_state == 1) {
        // 电网恢复 - 完成未完成的断电事件
        complete_incomplete_event((uint64_t)now);
    }
        
    // 检查是否需要保存队列到文件
    if (s_queue_needs_save) {
        s_queue_needs_save = false;
        if (Grid_Off_Save_Queue_To_Flash() == 0) {
            // Grid_Off_Debug_Print_Queue(); // 调试打印
            // ESP_LOGI(TAG, "Queue saved to flash successfully");
        } else {
            ESP_LOGW(TAG, "Failed to save queue to flash");
        }
    }
    #if (GRID_OFF_STATISTIC_DEBUG == 1)
    static uint32_t s_last_debug_print_time = 0;     // 上次调试打印的时间
    uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if ((current_time_ms - s_last_debug_print_time) >= GRID_OFF_DEBUG_PRINT_INTERVAL_MS) {
        Grid_Off_Debug_Print_Queue();
        s_last_debug_print_time = current_time_ms;
    }
    #endif
    s_last_grid_state = current_grid_state;

    /* 释放互斥 */
    xGridOffCountMutex_GiveMutex();
}


/**
  * @brief      数据上报任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
uint8_t Grid_Off_Report_Task(void)
{
    uint8_t ret = 0;
    uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

#if 0 // 外部检查
    if (iot_mqtt_login_state() != 1) {
        ESP_LOGD(TAG, "Network disconnected, sync skipped");
        return 0;
    }
#endif

    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (xGridOffCountMutex_TakeMutex(10) != pdTRUE) {
        return 0;
    }

    // 从服务器同步断电统计数据并检查本地数据是否一致
    check_and_execute_sync(current_time_ms);
    
    // 周期性批量上报断电事件（兜底机制，主要上报在事件完成时立即触发）
    if (s_fast_report_flag || ((current_time_ms - s_last_batch_report_time) >= GRID_OFF_BATCH_REPORT_INTERVAL_MS)) {
        // 检查未上报的断电事件数量
        int unreported_count = Grid_Off_Get_Unreported_Count();
        if (unreported_count > 0) {
            ESP_LOGI(TAG, "Periodic batch report: %d unreported events", unreported_count);
            int reported_count = grid_off_queue_batch_report();
            if (reported_count > 0) {
                ESP_LOGI(TAG, "Periodic batch report successful: %d events reported", reported_count);
            } else {
                ESP_LOGW(TAG, "Periodic batch report failed or no events to report");
            }
        } else {
            ESP_LOGD(TAG, "No unreported events for periodic batch report");
        }
        
        // 更新上报时间
        s_fast_report_flag = false;
        s_last_batch_report_time = current_time_ms;
        ret = 1;
    }

    /* 释放互斥 */
    xGridOffCountMutex_GiveMutex();

    return ret;
}

/**
 * @brief 根据时间跨度设置获取对应时间段的断电次数
 * @return 断电次数
 */
uint32_t Get_Grid_Off_Count_By_Timespan(void)
{
#if (GRID_OFF_STATISTIC_ENABLE == 0)
    return 0; // 断电统计功能未使能，返回0
#endif

    grid_off_count_statistics_t *stats = &IotSetData.dev_info_t.grid_off_statistics;
    uint8_t timespan = IotSetData.dev_info_t.Time_Span.power_off_count;
    
    switch (timespan) {
        case 1: // 每日
            return stats->daily_count;
        case 2: // 每周
            return stats->weekly_count;
        case 3: // 每月
            return stats->monthly_count;
        case 4: // 每年
            return stats->yearly_count;
        case 5: // 至今
            return stats->total_count;
        default:
            return 0; // 无效设置
    }
}

// 断电事件HTTP上报和清理函数已删除，改用队列批量上报机制

/**
 * @brief 向断电事件队列添加事件
 * @param start_time 断电开始时间（不能为0）
 * @param end_time 断电结束时间（如果为0表示断电开始，事件未结束；如果>0必须大于start_time）
 * @return 队列中的索引(>=0)成功, -1-失败（参数无效或队列已满）
 */
static int grid_off_queue_add_event(uint64_t start_time, uint64_t end_time)
{
    // 参数有效性检查
    if (start_time < 1735660800) { // 2025-01-01 00:00:00
        ESP_LOGE(TAG, "Invalid start_time: 0");
        return -1;
    }
    
    // 如果有结束时间，检查时间戳的合理性
    if (end_time > 0 && end_time <= start_time) {
        ESP_LOGE(TAG, "Invalid time range: start_time=%llu, end_time=%llu", start_time, end_time);
        return -1;
    }
    
    if (s_power_off_queue.count >= GRID_OFF_QUEUE_MAX_CAPACITY) {
        ESP_LOGW(TAG, "Power off queue is full, removing oldest event");
        // 队列满时，移除最旧的事件
        s_power_off_queue.head_index = (s_power_off_queue.head_index + 1) % GRID_OFF_QUEUE_MAX_CAPACITY;
        s_power_off_queue.count--;
    }
    
    // 添加新事件到队列尾部
    int new_index = s_power_off_queue.tail_index;
    power_off_record_t *record = &s_power_off_queue.records[new_index];
    record->start_time = (uint32_t)start_time;
    record->end_time = (uint32_t)end_time;
    record->need_report = (end_time > 0) ? 1 : 0; // 完整事件需要上报，进行中的事件不需要上报
    record->remark = 0;   // 备注编号预留，暂时填0
    
    s_power_off_queue.tail_index = (s_power_off_queue.tail_index + 1) % GRID_OFF_QUEUE_MAX_CAPACITY;
    s_power_off_queue.count++;
    
    // 标记需要保存到文件
    s_queue_needs_save = true;
    
    if (end_time > 0) {
        ESP_LOGI(TAG, "Added complete power off event to queue: start=%lu, end=%lu, duration=%lu, count=%d, index=%d", 
                 record->start_time, record->end_time, record->end_time - record->start_time, s_power_off_queue.count, new_index);
    } else {
        ESP_LOGI(TAG, "Added ongoing power off event to queue: start=%lu, end=0, count=%d, index=%d", 
                 record->start_time, s_power_off_queue.count, new_index);
    }
    
    return new_index; // 返回队列中的索引
}

/**
 * @brief 检查队列中是否有未完成的断电事件
 * @return 未完成事件的队列索引，-1表示无未完成事件
 * @note 未完成事件只会在队列的最后位置（tail_index-1），无需遍历整个队列
 */
static int Grid_Off_Find_Incomplete_Event(void)
{
    // 打印s_power_off_queue.tail_index的值
    ESP_LOGI(TAG, "s_power_off_queue.tail_index: %d", s_power_off_queue.tail_index);
    if (s_power_off_queue.count == 0) {
        return -1; // 队列为空
    }
    
    // 计算最后添加的事件索引（tail_index - 1，考虑环形队列）
    int last_index = (s_power_off_queue.tail_index - 1 + GRID_OFF_QUEUE_MAX_CAPACITY) % GRID_OFF_QUEUE_MAX_CAPACITY;
    power_off_record_t *record = &s_power_off_queue.records[last_index];
    
    // 检查最后一个事件是否未完成：有开始时间，无结束时间，不需要上报
    if (record->start_time > 0 && record->end_time == 0 && record->need_report == 0) {
        ESP_LOGI(TAG, "Found incomplete event at queue index %d, start_time=%lu", last_index, record->start_time);
        return last_index;
    }
    
    return -1; // 最后一个事件已完成
}

/**
 * @brief 更新队列中指定事件的结束时间
 * @param index 事件在队列中的索引
 * @param end_time 断电结束时间
 * @return 0-成功, -1-失败
 */
static int grid_off_queue_update_event_end_time(int index, uint64_t end_time)
{
    if (index < 0 || index >= GRID_OFF_QUEUE_MAX_CAPACITY) {
        ESP_LOGE(TAG, "Invalid queue index: %d", index);
        return -1;
    }
    
    power_off_record_t *record = &s_power_off_queue.records[index];
    if (record->start_time == 0) {
        ESP_LOGW(TAG, "Queue index %d is empty, cannot update", index);
        return -1;
    }
    
    // 检查结束时间必须大于开始时间
    if (end_time <= record->start_time) {
        ESP_LOGE(TAG, "Invalid end time: start_time=%lu, end_time=%llu", record->start_time, end_time);
        return -1;
    }
    
    record->end_time = (uint32_t)end_time;
    record->need_report = 1; // 事件完成后需要上报

    ESP_LOGI(TAG, "Updated power off event end time: index=%d, start=%lu, end=%lu, duration=%lu", 
             index, record->start_time, record->end_time, record->end_time - record->start_time);
    
    return 0;
}

/**
 * @brief 批量上报断电事件队列中的未上报事件
 * @return 上报成功的事件数量
 */
static int grid_off_queue_batch_report(void)
{
    if (s_power_off_queue.count == 0) {
        return 0; // 队列为空
    }
    
    // 分配未上报事件数组，限制单次上报数量 弱网环境最好减少上报数量
    power_off_record_t *unreported_events = (power_off_record_t *)iot_calloc(GRID_OFF_BATCH_REPORT_MAX_COUNT * sizeof(power_off_record_t));
    if (unreported_events == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for unreported events");
        return 0;
    }
    
    int unreported_count = 0;
    
    for (int i = 0; i < s_power_off_queue.count && unreported_count < GRID_OFF_BATCH_REPORT_MAX_COUNT; i++) {
        int index = (s_power_off_queue.head_index + i) % GRID_OFF_QUEUE_MAX_CAPACITY;
        power_off_record_t *record = &s_power_off_queue.records[index];
        
        // 只上报需要上报的事件（已完成且标记为需要上报）
        if (record->need_report == 1 && record->end_time > 0) {
            unreported_events[unreported_count] = *record;
            unreported_count++;
        }
    }
    
    if (unreported_count == 0) {
        ESP_LOGI(TAG, "No unreported events in queue");
        free(unreported_events);
        return 0;
    }
    
    ESP_LOGI(TAG, "Batch reporting %d power off events", unreported_count);
    
    // 调用HTTP批量上报接口
    int result = http_batch_report_power_outages(
        unreported_events, 
        unreported_count
    );
    
    // 释放动态分配的内存
    free(unreported_events);
    
    if (result == 0) {
        // 上报成功，标记已上报的事件为不需要上报
        int marked_count = 0;
        for (int i = 0; i < s_power_off_queue.count && marked_count < unreported_count; i++) {
            int index = (s_power_off_queue.head_index + i) % GRID_OFF_QUEUE_MAX_CAPACITY;
            power_off_record_t *record = &s_power_off_queue.records[index];
            
            if (record->need_report == 1 && record->end_time > 0) {
                record->need_report = 0; // 已上报，不再需要上报
                marked_count++;
            }
        }
        
        // 标记需要保存到文件
        s_queue_needs_save = true;
        
        return unreported_count;
    } else {
        ESP_LOGW(TAG, "Batch report failed, error code: %d", result);
        return 0;
    }
}

/**
 * @brief 获取未上报的断电事件数量
 * @return 未上报事件数量
 */
static int Grid_Off_Get_Unreported_Count(void)
{
    int unreported_count = 0;
    
    for (int i = 0; i < s_power_off_queue.count; i++) {
        int index = (s_power_off_queue.head_index + i) % GRID_OFF_QUEUE_MAX_CAPACITY;
        power_off_record_t *record = &s_power_off_queue.records[index];
        
        // 只计算已完成且需要上报的事件
        if (record->need_report == 1 && record->end_time > 0) {
            unreported_count++;
        }
    }
    
    return unreported_count;
}

// 设备状态保存和重启检测功能已删除，改用队列机制统一管理

/**
 * @brief 计算断电事件队列的校验和
 * @param queue 队列指针
 * @return 校验和
 */
static uint32_t Grid_Off_Calculate_Queue_Checksum(const power_off_queue_t *queue)
{
    if (queue == NULL) {
        return 0;
    }
    
    // 计算除校验和字段外的所有数据的CRC32
    size_t data_size = sizeof(power_off_queue_t) - sizeof(uint32_t); // 减去checksum字段
    return calcu_crc32(0, (const uint8_t *)queue, data_size);
}

/**
 * @brief 验证断电事件队列的完整性
 * @param queue 队列指针
 * @return true-校验通过, false-校验失败
 */
static bool Grid_Off_Verify_Queue_Integrity(const power_off_queue_t *queue)
{
    if (queue == NULL) {
        return false;
    }
    
    // 检查魔数
    if (queue->magic_number != GRID_OFF_QUEUE_MAGIC_NUMBER) {
        ESP_LOGE(TAG, "Queue magic number mismatch: expected 0x%08lX, got 0x%08lX", 
                 (unsigned long)GRID_OFF_QUEUE_MAGIC_NUMBER, (unsigned long)queue->magic_number);
        return false;
    }
    
    // 检查版本号（当前版本为1）
    if (queue->version != 1) {
        ESP_LOGE(TAG, "Queue version mismatch: expected 1, got %u", (unsigned int)queue->version);
        return false;
    }
    
    // 检查队列参数有效性
    if (queue->count > GRID_OFF_QUEUE_MAX_CAPACITY || 
        queue->head_index >= GRID_OFF_QUEUE_MAX_CAPACITY ||
        queue->tail_index >= GRID_OFF_QUEUE_MAX_CAPACITY) {
        ESP_LOGE(TAG, "Queue parameters invalid: count=%d, head=%d, tail=%d", 
                 queue->count, queue->head_index, queue->tail_index);
        return false;
    }
    
    // 计算并验证校验和
    uint32_t calculated_checksum = Grid_Off_Calculate_Queue_Checksum(queue);
    if (calculated_checksum != queue->checksum) {
        ESP_LOGE(TAG, "Queue checksum mismatch: expected 0x%08lX, got 0x%08lX", 
                 (unsigned long)queue->checksum, (unsigned long)calculated_checksum);
        return false;
    }
    
    return true;
}

/**
 * @brief 保存断电事件队列到文件
 * @return 0-成功, -1-失败
 */
static int Grid_Off_Save_Queue_To_Flash(void)
{
    // 更新魔数和版本号
    s_power_off_queue.magic_number = GRID_OFF_QUEUE_MAGIC_NUMBER;
    s_power_off_queue.version = 1;
    
    // 计算校验和
    s_power_off_queue.checksum = Grid_Off_Calculate_Queue_Checksum(&s_power_off_queue);
    
    // 使用封装的文件写入函数
    if (Grid_Off_Write_Queue_File(GRID_OFF_QUEUE_FLASH_FILENAME, &s_power_off_queue, sizeof(power_off_queue_t)) == 0) {
        // ESP_LOGI(TAG, "Power off queue saved to file successfully, count=%d", s_power_off_queue.count);
        return 0;
    } else {
        ESP_LOGE(TAG, "Failed to save power off queue to file");
        return -1;
    }
}

/**
 * @brief 从文件加载断电事件队列
 * @return 0-成功, -1-失败
 */
static int Grid_Off_Load_Queue_From_Flash(void)
{
    power_off_queue_t temp_queue;
    
    // 使用封装的文件读取函数
    if (Grid_Off_Read_Queue_File(GRID_OFF_QUEUE_FLASH_FILENAME, &temp_queue, sizeof(power_off_queue_t)) != 0) {
        ESP_LOGE(TAG, "No power off queue file found, initializing new queue");
        
        // 初始化新的队列
        memset(&s_power_off_queue, 0, sizeof(power_off_queue_t));
        s_power_off_queue.magic_number = GRID_OFF_QUEUE_MAGIC_NUMBER;
        s_power_off_queue.version = 1;
        return -1;
    }
    
    // 验证数据完整性
    if (!Grid_Off_Verify_Queue_Integrity(&temp_queue)) {
        ESP_LOGE(TAG, "Power off queue integrity check failed, initializing new queue");
        
        // 初始化新的队列
        memset(&s_power_off_queue, 0, sizeof(power_off_queue_t));
        s_power_off_queue.magic_number = GRID_OFF_QUEUE_MAGIC_NUMBER;
        s_power_off_queue.version = 1;
        return -1;
    }
    
    // 数据校验通过，复制到工作队列
    memcpy(&s_power_off_queue, &temp_queue, sizeof(power_off_queue_t));
    
    ESP_LOGI(TAG, "Power off queue loaded from file successfully, count=%d, head=%d, tail=%d", 
             s_power_off_queue.count, s_power_off_queue.head_index, s_power_off_queue.tail_index);
    
    // 统计未上报的事件数量
    int unreported_count = Grid_Off_Get_Unreported_Count();
    if (unreported_count > 0) {
        ESP_LOGI(TAG, "Found %d unreported power off events in queue", unreported_count);
    }
    
    return 0;
}

/**
 * @brief 写入队列数据到文件
 * @param filename 文件名
 * @param data 数据指针
 * @param size 数据大小
 * @return 0-成功, -1-失败
 */
static int Grid_Off_Write_Queue_File(const char *filename, const void *data, size_t size)
{
    if (filename == NULL || data == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid parameters for file write");
        return -1;
    }
    
    // 构建文件路径
    char path[30] = {0};
    sprintf(path, "%s/%s%s", PARAMETER_ROOT_PATH, PARAMETER_FOLDER_LEVEL1, filename);

    // 打开文件进行写入
    return fs_file_write(path, data, 0, size);
}

/**
 * @brief 从文件读取队列数据
 * @param filename 文件名
 * @param data 数据缓冲区指针
 * @param size 期望读取的数据大小
 * @return 0-成功, -1-失败
 */
static int Grid_Off_Read_Queue_File(const char *filename, void *data, size_t size)
{
    if (filename == NULL || data == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid parameters for file read");
        return -1;
    }
    
    // 构建文件路径
    char path[30] = {0};
    sprintf(path, "%s/%s%s", PARAMETER_ROOT_PATH, PARAMETER_FOLDER_LEVEL1, filename);
    
    // 打开文件读取
    return fs_file_read(path, data, size);
}

/**
 * @brief 调试函数：打印文件中存储的断电事件队列
 * @note 此函数用于调试目的，打印队列的详细信息和所有事件记录
 */
static void Grid_Off_Debug_Print_Queue(void)
{
    ESP_LOGI(TAG, "=== Debug: Grid Off Queue Status ===");
    
    // 打印当前内存中的队列状态
    ESP_LOGI(TAG, "Memory Queue Status:");
    ESP_LOGI(TAG, "  Magic Number: 0x%08lX", (unsigned long)s_power_off_queue.magic_number);
    ESP_LOGI(TAG, "  Version: %u", s_power_off_queue.version);
    ESP_LOGI(TAG, "  Count: %d", s_power_off_queue.count);
    ESP_LOGI(TAG, "  Head Index: %d", s_power_off_queue.head_index);
    ESP_LOGI(TAG, "  Tail Index: %d", s_power_off_queue.tail_index);
    ESP_LOGI(TAG, "  Checksum: 0x%08lX", (unsigned long)s_power_off_queue.checksum);
    
    // 打印内存中的事件记录
    if (s_power_off_queue.count > 0) {
        ESP_LOGI(TAG, "Memory Queue Events:");
        for (int i = 0; i < s_power_off_queue.count; i++) {
            int index = (s_power_off_queue.head_index + i) % GRID_OFF_QUEUE_MAX_CAPACITY;
            power_off_record_t *record = &s_power_off_queue.records[index];
            
            // 转换时间戳为可读格式
            time_t start_time = (time_t)record->start_time;
            time_t end_time = (time_t)record->end_time;
            struct tm start_tm = *localtime(&start_time);
            struct tm end_tm = *localtime(&end_time);
            
            char start_str[32] = {0};
            char end_str[32] = {0};
            strftime(start_str, sizeof(start_str), "%Y-%m-%d %H:%M:%S", &start_tm);
            if (record->end_time > 0) {
                strftime(end_str, sizeof(end_str), "%Y-%m-%d %H:%M:%S", &end_tm);
            } else {
                strcpy(end_str, "In Progress");
            }
            
            uint32_t duration = (record->end_time > record->start_time) ? 
                               (record->end_time - record->start_time) : 0;
            
            ESP_LOGI(TAG, "  Event[%d] Index[%d]: Start=%s, End=%s, Duration=%lus, NeedReport=%d, Remark=%u",
                     i, index, start_str, end_str, duration, record->need_report, record->remark);
        }
    } else {
        ESP_LOGI(TAG, "Memory Queue is empty");
    }
    
    // 统计信息
    int unreported_count = Grid_Off_Get_Unreported_Count();
    int incomplete_index = Grid_Off_Find_Incomplete_Event();
    
    ESP_LOGI(TAG, "Queue Statistics:");
    ESP_LOGI(TAG, "  Unreported Events: %d", unreported_count);
    ESP_LOGI(TAG, "  Incomplete Event Index: %d", incomplete_index);
    ESP_LOGI(TAG, "  Queue Needs Save: %s", s_queue_needs_save ? "Yes" : "No");
    
    ESP_LOGI(TAG, "=== End Debug Print ===");
}

/**
 * @brief 完成未完成的断电事件
 * @param current_time 当前时间戳
 */
static void complete_incomplete_event(uint64_t current_time)
{
    int incomplete_index = Grid_Off_Find_Incomplete_Event();
    if (incomplete_index >= 0) {
        power_off_record_t *record = &s_power_off_queue.records[incomplete_index];
        ESP_LOGI(TAG, "Found incomplete event at index %d, start_time=%lu", 
                 incomplete_index, record->start_time);
        
        if (grid_off_queue_update_event_end_time(incomplete_index, current_time) == 0) {
            ESP_LOGI(TAG, "Event completed, duration: %lu seconds", 
                     record->end_time - record->start_time);
            
            // 断电事件完成时增加统计计数 注：产品经理决策，断电事件结束时断电次数才加一，与服务器保持一致。——2025年11月10日
            grid_off_count_statistics_t *stats = &IotSetData.dev_info_t.grid_off_statistics;
            stats->daily_count++;
            stats->weekly_count++;
            stats->monthly_count++;
            stats->yearly_count++;
            stats->total_count++;
            
            reals.SetDataWrFlag.sBit.grid_off_statistics = 1;
            
            ESP_LOGW(TAG, "Grid off event completed - Daily:%lu, Weekly:%lu, Monthly:%lu, Yearly:%lu, Total:%lu",
                     stats->daily_count, stats->weekly_count, stats->monthly_count, 
                     stats->yearly_count, stats->total_count);
            
            // 断电事件完成后立即上报
            s_fast_report_flag = true;

            // 存储标志
            s_queue_needs_save = true;
        } else {
            ESP_LOGW(TAG, "Failed to update event end time");
        }
    } else {
        ESP_LOGD(TAG, "No incomplete events found");
    }
}

/**
 * @brief 执行断电统计数据同步
 * @return true 同步成功，false 同步失败或网络未连接
 */
static bool execute_grid_off_sync(void)
{
    grid_off_count_statistics_t server_stats = {0};
    if (http_get_grid_off_count(&server_stats) == 0) {
        // 检查本地和服务器数据是否一致
        grid_off_count_statistics_t *local_stats = &IotSetData.dev_info_t.grid_off_statistics;
        
        if (local_stats->daily_count != server_stats.daily_count ||
            local_stats->weekly_count != server_stats.weekly_count ||
            local_stats->monthly_count != server_stats.monthly_count ||
            local_stats->yearly_count != server_stats.yearly_count ||
            local_stats->total_count != server_stats.total_count) {
            
            // 数据不一致，需要同步
            memcpy(local_stats, &server_stats, sizeof(grid_off_count_statistics_t));
            reals.SetDataWrFlag.sBit.grid_off_statistics = 1; // 保存到flash
            ESP_LOGI(TAG, "Grid off statistics synced: daily=%lu, weekly=%lu, monthly=%lu, yearly=%lu, total=%lu",
                     server_stats.daily_count, server_stats.weekly_count, server_stats.monthly_count,
                     server_stats.yearly_count, server_stats.total_count);
        } else {
            // 数据一致，无需同步
            ESP_LOGD(TAG, "Grid off statistics already in sync, no update needed");
        }
        return true;
    } else {
        ESP_LOGW(TAG, "Grid off statistics sync failed");
        return false;
    }
}

/**
 * @brief 检查并执行断电统计数据同步
 * @param current_time_ms 当前时间（毫秒）
 * @note 包含首次同步和周期同步的检查逻辑
 */
static void check_and_execute_sync(uint32_t current_time_ms)
{
    if (s_first_sync_done == false && Grid_Off_Get_Unreported_Count() == 0) {
        s_first_sync_done = true; // 标记首次同步完成
        if (execute_grid_off_sync()) {
            s_last_periodic_sync_time = current_time_ms; // 记录周期同步起始时间
            ESP_LOGI(TAG, "First sync completed, switched to periodic sync mode");
        } else {
            ESP_LOGW(TAG, "First sync failed, will retry on next batch report");
        }
    }
    
    // 检查周期同步（仅在首次同步完成后）
    if (s_first_sync_done && (current_time_ms - s_last_periodic_sync_time) >= SYNC_PERIODIC_INTERVAL_MS) {
        execute_grid_off_sync();
        s_last_periodic_sync_time = current_time_ms; // 更新周期同步时间
    }
}

#endif
