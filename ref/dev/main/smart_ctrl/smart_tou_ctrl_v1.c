/**
  ******************************************************************************
  * @file      smart_tou_ctrl_v1.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/25
  * @brief     智能TOU控制模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/21  <td>1.0     <td>lixingyu   <td>Create the initial version
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
#include "can_protocol.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "smart_tou_ctrl_v1.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[SMART_TOU_CTRL]"

/* =============================== 文件内全局变量 ================================ */

// 智能TOU控制模块生效队列
static QueueHandle_t xQueue_smart_tou_ctrl = NULL;

/* 信号量（互斥） */
static SemaphoreHandle_t xSmartTouMutex = NULL;

// 智能TOU控制模块配置信息
static USE_EXT_RAM_BSS smart_tou_ctrl_config_t Smart_Tou_Ctrl_Config = {0};

// 智能TOU控制模块配置写入缓存
static USE_EXT_RAM_BSS smart_tou_ctrl_config_t Smart_Tou_Ctrl_Config_Wr = {0};

// 智能控制时间段控制汇总
static USE_EXT_RAM_BSS tou_time_quantum_t Time_Quantum_Config[SMART_TOU_QUANTUM_MAX_CNT] = {0};
    
// 智能TOU控制模块有效时间点数量
static uint8_t Smart_Tou_Ctrl_Time_Point_Valid_Cnt = 0;

// 智能TOU控制模块有效时间段数量
static uint8_t Smart_Tou_Ctrl_Time_Quantum_Valid_Cnt = 0; 

// 智能TOU控制模块配置信息更新标志
static uint8_t Smart_Tou_Ctrl_Config_update_Flag = 0;

// 智能TOU控制模块配置信息写入FLASH标志
static uint8_t Smart_Tou_Ctrl_Config_Write_Flag = 0;

// 系统运行状态数据，用于进行时间段持续状态检查
static USE_EXT_RAM_BSS smart_tou_ctrl_result_t System_Running_Data[SMART_TOU_UNIT_MAX_CNT] = {0};

// TOU控制上次执行时间节点
static uint32_t Smart_Tou_Ctrl_Ctrl_PreTick = 0;
    
/* 全局设备寄存器回调（默认为NULL）及检查间隔（ms）
   注意：该回调函数需外部按需求实现，逻辑参考device_reg_cb_t定义*/
static device_reg_cb_t g_device_reg_cb = NULL;
static uint32_t        g_device_reg_check_interval_ms = 0;

// 智能TOU控制模块时间段使能
static bool Smart_Tou_Ctrl_Time_Quantum_Enable = false; 

// 智能TOU控制支路状态
static smart_ctrl_status_struct Smart_Tou_Ctrl_Status = {0};

/* ================================ 模块函数定义 ================================ */

/* 简单封装：获取/释放互斥 */
static inline BaseType_t Smart_Tou_TakeMutex(TickType_t timeout_ms)
{
    if (!xSmartTouMutex) return pdFALSE;
    return xSemaphoreTake(xSmartTouMutex, pdMS_TO_TICKS(timeout_ms));
}
static inline void Smart_Tou_GiveMutex(void)
{
    if (xSmartTouMutex) xSemaphoreGive(xSmartTouMutex);
}

/**
 * @brief 注册/设置设备寄存器访问回调并指定检查时间间隔（毫秒）
 * @param cb 回调函数指针 (device_reg_cb_t)
 * @param check_interval_ms 检查时间间隔，单位毫秒；为0表示不检查
 */
void device_reg_cb_set(device_reg_cb_t cb, uint32_t check_interval_ms)
{
    g_device_reg_cb = cb;
    g_device_reg_check_interval_ms = check_interval_ms;
}

/**
 * @brief 调试用：手动设置Smart_Tou_Ctrl_Config中的几个TOU时间点
 * 
 * 该函数用于调试场景，直接在Smart_Tou_Ctrl_Config结构体中写入若干测试用的TOU配置点，
 * 便于开发过程中验证周期处理、触发判断等功能。
 */
static void Smart_Tou_Ctrl_Module_DEBUG(void)
{
    static const int hours[] = {0, 6, 8, 12, 16, 18, 23}; // 覆盖午夜、清晨、上午、下午、傍晚、深夜
    static const int mins[]  = {0, 30};                   // 整点、半点
    static const uint8_t week_mask[7] = {
        0x01, // 周日
        0x02, // 周一
        0x04, // 周二
        0x08, // 周三
        0x10, // 周四
        0x20, // 周五
        0x40  // 周六
    };

    int idx = 0;
    // 1. 每天的整点和半点（全周有效）
    for (int h = 0; h < sizeof(hours)/sizeof(hours[0]); ++h) {
        for (int m = 0; m < sizeof(mins)/sizeof(mins[0]); ++m) {
            if (idx >= SMART_TOU_UNIT_MAX_CNT) break;
            tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
            memset(tou_cfg, 0, sizeof(tou_time_config_t));
            tou_cfg->bits.time_sec = hours[h] * 3600 + mins[m] * 60;
            tou_cfg->bits.month_sel = 0xFFF;      // 12个月
            tou_cfg->bits.day_sel = 0x7FFFFFFF;   // 31天
            tou_cfg->bits.week_sel = 0x7F;        // 全周
            tou_cfg->bits.reg_type = 0;
            tou_cfg->bits.device_id = idx + 1;
            Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x100 + idx;
            Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x1000 + idx;
            idx++;
        }
    }

    // 2. 周一到周五的23:59:59（单点，week_sel=0x3E）
    if (idx < SMART_TOU_UNIT_MAX_CNT) {
        tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
        memset(tou_cfg, 0, sizeof(tou_time_config_t));
        tou_cfg->bits.time_sec = 23 * 3600 + 59 * 60 + 59;
        tou_cfg->bits.month_sel = 0xFFF;
        tou_cfg->bits.day_sel = 0x7FFFFFFF;
        tou_cfg->bits.week_sel = 0x3E; // 周一到周五
        tou_cfg->bits.reg_type = 1;
        tou_cfg->bits.device_id = idx + 1;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x2000;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x2000;
        idx++;
    }

    // 3. 每月单数天（1,3,5,...,31）的20:00（单点，day_sel=0x55555555，week_sel=0x7F）
    if (idx < SMART_TOU_UNIT_MAX_CNT) {
        tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
        memset(tou_cfg, 0, sizeof(tou_time_config_t));
        tou_cfg->bits.time_sec = 20 * 3600; // 20:00
        tou_cfg->bits.month_sel = 0xFFF;
        tou_cfg->bits.day_sel = 0x55555555; // 1,3,5,...,31号
        tou_cfg->bits.week_sel = 0x7F;      // 全周
        tou_cfg->bits.reg_type = 2;
        tou_cfg->bits.device_id = idx + 1;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x3000;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x3000;
        idx++;
    }

    // 4. 特殊边界：每月1号、31号的0:00和23:59:59（全周）
    if (idx + 4 <= SMART_TOU_UNIT_MAX_CNT) {
        // 1号 0:00
        tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
        memset(tou_cfg, 0, sizeof(tou_time_config_t));
        tou_cfg->bits.time_sec = 0;
        tou_cfg->bits.month_sel = 0xFFF;
        tou_cfg->bits.day_sel = 0x1; // 1号
        tou_cfg->bits.week_sel = 0x7F;
        tou_cfg->bits.reg_type = 3;
        tou_cfg->bits.device_id = idx + 1;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x4000;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x4000;
        idx++;

        // 1号 23:59:59
        tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
        memset(tou_cfg, 0, sizeof(tou_time_config_t));
        tou_cfg->bits.time_sec = 23 * 3600 + 59 * 60 + 59;
        tou_cfg->bits.month_sel = 0xFFF;
        tou_cfg->bits.day_sel = 0x1;
        tou_cfg->bits.week_sel = 0x7F;
        tou_cfg->bits.reg_type = 3;
        tou_cfg->bits.device_id = idx + 1;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x4001;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x4001;
        idx++;

        // 31号 0:00
        tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
        memset(tou_cfg, 0, sizeof(tou_time_config_t));
        tou_cfg->bits.time_sec = 0;
        tou_cfg->bits.month_sel = 0xFFF;
        tou_cfg->bits.day_sel = 0x40000000; // 31号
        tou_cfg->bits.week_sel = 0x7F;
        tou_cfg->bits.reg_type = 3;
        tou_cfg->bits.device_id = idx + 1;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x4002;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x4002;
        idx++;

        // 31号 23:59:59
        tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
        memset(tou_cfg, 0, sizeof(tou_time_config_t));
        tou_cfg->bits.time_sec = 23 * 3600 + 59 * 60 + 59;
        tou_cfg->bits.month_sel = 0xFFF;
        tou_cfg->bits.day_sel = 0x40000000;
        tou_cfg->bits.week_sel = 0x7F;
        tou_cfg->bits.reg_type = 3;
        tou_cfg->bits.device_id = idx + 1;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x4003;
        Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x4003;
        idx++;
    }

    // 5. 剩余点均匀分布在一周内（以小时为步进，按天循环，避免与前面重复）
    int day, hour, min;
    for (day = 0; idx < SMART_TOU_UNIT_MAX_CNT && day < 7; ++day) { // 0-周日，1-周一...
        for (hour = 1; idx < SMART_TOU_UNIT_MAX_CNT && hour < 8; ++hour) {
            for (min = 0; idx < SMART_TOU_UNIT_MAX_CNT && min < 60; min += 15) {
                // 跳过已覆盖的整点/半点
                if ((min == 0 || min == 30) && (hour == 0 || hour == 6 || hour == 8 || hour == 12 || hour == 16 || hour == 18 || hour == 23))
                    continue;
                tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[idx].time_cfg;
                memset(tou_cfg, 0, sizeof(tou_time_config_t));
                tou_cfg->bits.time_sec = hour * 3600 + min * 60;
                tou_cfg->bits.month_sel = 0xFFF;
                tou_cfg->bits.day_sel = 0x7FFFFFFF;
                tou_cfg->bits.week_sel = week_mask[day]; // 只在本周某一天
                tou_cfg->bits.reg_type = 0;
                tou_cfg->bits.device_id = idx + 1;
                Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_addr = 0x5000 + idx;
                Smart_Tou_Ctrl_Config.smart_tou_unit[idx].reg_cfg.reg_value = 0x5000 + idx;
                idx++;
            }
        }
    }

    // 可选：写入文件
    char path[30] = {0};
    SMART_TOU_FILE_PATH(path);
    fs_file_write_at(path, 0, (uint8_t*)&Smart_Tou_Ctrl_Config, sizeof(smart_tou_ctrl_config_t));
    ESP_LOGW(TAG, "Smart TOU Control Module Debug: 已批量生成%d个典型/边界/均匀分布时间点", idx);
}

/**
 * @brief 测试函数：预设若干时间段并触发重建（用于单元/集成测试）
 *
 * 说明：
 * - 在互斥保护下写入 Smart_Tou_Ctrl_Config 的若干 timepoint（类型为 QUANTUM），
 *   包含配对的 start/end、以及一个 start-only，用以验证配对/追加/触发逻辑。
 * - 调用后会设置 update/write 标志并执行 REBUILD，使全局 Time_Quantum_Config 生效。
 */
static void Smart_Tou_Test_Preset_Quanta(void)
{
    /* 0/1：一对（08:00 -> 12:00），同一控制目标 */
    tou_time_config_t *t0 = &Smart_Tou_Ctrl_Config.smart_tou_unit[0].time_cfg;
    memset(t0, 0, sizeof(*t0));
    t0->bits.time_type = TOU_TIME_TYPE_QUANTUM;
    t0->bits.time_sec  = 8 * 3600; /* 08:00 */
    t0->bits.month_sel = 0xFFF;
    t0->bits.day_sel   = 0x7FFFFFFF;
    t0->bits.week_sel  = 0x7F;
    t0->bits.reg_type  = 0;
    t0->bits.device_id = 0;
    Smart_Tou_Ctrl_Config.smart_tou_unit[0].reg_cfg.reg_addr  = 7011;
    Smart_Tou_Ctrl_Config.smart_tou_unit[0].reg_cfg.reg_value = 65535;

    tou_time_config_t *t1 = &Smart_Tou_Ctrl_Config.smart_tou_unit[1].time_cfg;
    memset(t1, 0, sizeof(*t1));
    t1->bits.time_type = TOU_TIME_TYPE_QUANTUM;
    t1->bits.time_sec  = 12 * 3600; /* 12:00 */
    t1->bits.month_sel = 0xFFF;
    t1->bits.day_sel   = 0x7FFFFFFF;
    t1->bits.week_sel  = 0x7F;
    t1->bits.reg_type  = 0;         /* 与 t0 相同以便配对 */
    t1->bits.device_id = 0;
    Smart_Tou_Ctrl_Config.smart_tou_unit[1].reg_cfg.reg_addr  = 7011;
    Smart_Tou_Ctrl_Config.smart_tou_unit[1].reg_cfg.reg_value = 0;

    /* 2/3：另一对（18:00 -> 23:00），不同控制目标 */
    tou_time_config_t *t2 = &Smart_Tou_Ctrl_Config.smart_tou_unit[2].time_cfg;
    memset(t2, 0, sizeof(*t2));
    t2->bits.time_type = TOU_TIME_TYPE_QUANTUM;
    t2->bits.time_sec  = 18 * 3600; /* 18:00 */
    t2->bits.month_sel = 0xFFF;
    t2->bits.day_sel   = 0x7FFFFFFF;
    t2->bits.week_sel  = 0x7F;
    t2->bits.reg_type  = 0;
    t2->bits.device_id = 0;
    Smart_Tou_Ctrl_Config.smart_tou_unit[2].reg_cfg.reg_addr  = 7012;
    Smart_Tou_Ctrl_Config.smart_tou_unit[2].reg_cfg.reg_value = 65535;

    tou_time_config_t *t3 = &Smart_Tou_Ctrl_Config.smart_tou_unit[3].time_cfg;
    memset(t3, 0, sizeof(*t3));
    t3->bits.time_type = TOU_TIME_TYPE_QUANTUM;
    t3->bits.time_sec  = 23 * 3600; /* 23:00 */
    t3->bits.month_sel = 0xFFF;
    t3->bits.day_sel   = 0x7FFFFFFF;
    t3->bits.week_sel  = 0x7F;
    t3->bits.reg_type  = 0;
    t3->bits.device_id = 0;
    Smart_Tou_Ctrl_Config.smart_tou_unit[3].reg_cfg.reg_addr  = 7012;
    Smart_Tou_Ctrl_Config.smart_tou_unit[3].reg_cfg.reg_value = 0;
}

/**
 * @brief 比较函数：按 time_sec 从小到大排序（用于 qsort）
 *
 * @details
 * - 用于对 uint8_t 索引数组进行排序，索引指向全局 Smart_Tou_Ctrl_Config.smart_tou_unit[]。
 * - 读取每个索引对应的 tou_time_config_t.bits.time_sec 字段进行比较。
 * - 该比较函数遵循 qsort 约定：返回负值表示 a < b，0 表示相等，正值表示 a > b。
 *
 * @param a 指向数组中元素（uint8_t）的指针（实际为 const void*）
 * @param b 指向数组中元素（uint8_t）的指针（实际为 const void*）
 * @return int   返回比较结果：-1/0/1
 *
 * @note
 * - 调用方需保证索引值在 [0, SMART_TOU_UNIT_MAX_CNT) 范围内且对应的 Smart_Tou_Ctrl_Config 已初始化。
 * - 该函数不做越界检查以减少排序开销，若存在不可靠索引，应在调用 qsort 前过滤。
 */
static int compare_idx_by_time(const void *a, const void *b)
{
    uint8_t ia = *(const uint8_t *)a;
    uint8_t ib = *(const uint8_t *)b;

    uint32_t ta = Smart_Tou_Ctrl_Config.smart_tou_unit[ia].time_cfg.bits.time_sec;
    uint32_t tb = Smart_Tou_Ctrl_Config.smart_tou_unit[ib].time_cfg.bits.time_sec;

    if (ta < tb) return -1;
    if (ta > tb) return 1;
    return 0;
}

/**
 * @brief 判断两个配置单元是否兼容（用于时间段配对）
 *
 * @details
 * - 兼容定义为：reg_type、device_id 和 reg_addr 三者完全相同。
 * - 该比较用于配对起始点与结束点，确保同一控制对象在时间段内被成对控制。
 *
 * @param a 第一个配置单元索引（uint8_t，指向 Smart_Tou_Ctrl_Config.smart_tou_unit）
 * @param b 第二个配置单元索引（uint8_t）
 * @return bool 兼容返回 true，不兼容或任一索引越界返回 false
 *
 * @note
 * - 为性能考虑，函数对索引进行了越界检查并直接返回 false；调用方应尽量保证传入索引已在有效集合中。
 */
static inline bool tou_units_compatible(uint8_t a, uint8_t b)
{
    if (a >= SMART_TOU_UNIT_MAX_CNT || b >= SMART_TOU_UNIT_MAX_CNT) return false;

    return (Smart_Tou_Ctrl_Config.smart_tou_unit[a].time_cfg.bits.reg_type ==
            Smart_Tou_Ctrl_Config.smart_tou_unit[b].time_cfg.bits.reg_type)
        && (Smart_Tou_Ctrl_Config.smart_tou_unit[a].time_cfg.bits.device_id ==
            Smart_Tou_Ctrl_Config.smart_tou_unit[b].time_cfg.bits.device_id)
        && (Smart_Tou_Ctrl_Config.smart_tou_unit[a].reg_cfg.reg_addr ==
            Smart_Tou_Ctrl_Config.smart_tou_unit[b].reg_cfg.reg_addr);
}

/**
 * @brief 收集所有 time_type == TOU_TIME_TYPE_QUANTUM 且有效的时间点索引
 *
 * @details
 * - 遍历 Smart_Tou_Ctrl_Config.smart_tou_unit[0 .. time_num-1]，筛选出属于“时间段类型（QUANTUM）”且
 *   通过 tou_time_cfg_invalid 校验的时间点索引，并按发现顺序填充到 caller 提供的 idx_list。
 *
 * @param[out] idx_list  调用者分配的输出数组，大小至少为 SMART_TOU_UNIT_MAX_CNT
 * @return uint8_t       返回填充的索引数量（0 表示未找到）
 *
 * @note
 * - 函数内部使用 Smart_Tou_Ctrl_Config 全局数据，请在并发场景下由外部互斥保护（例如 Smart_Tou_TakeMutex）。
 * - 返回的索引未排序；如需按 time_sec 排序，请在调用方对 idx_list 使用 qsort(compare_idx_by_time)。
 */
static uint8_t collect_quantum_idx(uint8_t *idx_list)
{
    uint8_t idx_cnt = 0;

    for (int i = 0; i < Smart_Tou_Ctrl_Config.smart_tou_info.time_num && i < SMART_TOU_UNIT_MAX_CNT; ++i) {
        tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[i].time_cfg;
        if (!tou_cfg) continue;
        if (tou_cfg->bits.time_type != TOU_TIME_TYPE_QUANTUM) continue;
        if (tou_time_cfg_invalid(tou_cfg)) continue;
        idx_list[idx_cnt++] = (uint8_t)i;
    }

    return idx_cnt;
}

/**
 * @brief 构建已使用索引映射（用于区分新增点）
 *
 * 说明：
 * - 扫描给定的 time quantum 配置数组 cfg（长度 SMART_TOU_QUANTUM_MAX_CNT），
 *   将作为起点或终点被使用的 time point 索引在 used_map 中标记为 1。
 * - used_map 的下标为真实 time point 索引（0 .. SMART_TOU_UNIT_MAX_CNT-1）。
 *
 * 语义与约定：
 * - end_time_index == 0xFF 表示 start-only（无结束点），此时不对结束点做标记；
 * - 函数不会对 used_map 做越界检测，调用方须保证 used_map 已分配且长度至少为 SMART_TOU_UNIT_MAX_CNT；
 * - 调用方应在外部加锁以保证 cfg 与全局状态的一致性（若 cfg 指向全局 Time_Quantum_Config）。
 *
 * 复杂度：
 * - O(N)（N = SMART_TOU_QUANTUM_MAX_CNT）
 *
 * @param cfg       输入的 time quantum 数组（只读）
 * @param used_map  输出映射数组（调用者分配，函数会清零并设置被使用位）
 */
static void mark_used_timepoint_indices(const tou_time_quantum_t *cfg, uint8_t *used_map)
{
    /* 清空输出映射（确保幂等） */
    memset(used_map, 0, SMART_TOU_UNIT_MAX_CNT);

    /* 遍历所有 timequantum，标记其起点/终点索引为已使用 */
    for (uint8_t q = 0; q < SMART_TOU_QUANTUM_MAX_CNT; ++q) {
        if (cfg[q].status == TOU_PERIOD_STATUS_UNCONFIGURED) continue;

        uint8_t si = cfg[q].start_time_index;
        uint8_t ei = cfg[q].end_time_index;

        /* 标记起点（边界检查） */
        if (si < SMART_TOU_UNIT_MAX_CNT) {
            used_map[si] = 1;
        }

        /* 标记终点（跳过 start-only 的 0xFF） */
        if (ei != 0xFF && ei < SMART_TOU_UNIT_MAX_CNT) {
            used_map[ei] = 1;
        }
    }
}

/**
 * @brief 从已排序的 idx_list 中筛出“新增”索引（不在 used_map 中）
 *
 * @param idx_list   已排序的 time point 索引数组（按 time_sec，可含无效索引）
 * @param idx_cnt    idx_list 的长度
 * @param used_map   长度为 SMART_TOU_UNIT_MAX_CNT 的映射数组，非 0 表示该真实索引已被使用
 * @param out_list   输出缓冲（调用者分配），函数将新增的真实索引按遇到的顺序写入
 * @return uint8_t   写入 out_list 的新增索引数量（可能为 0）
 *
 * 说明：
 * - 仅当 used_map[id] == 0 且 id 在合法范围内时，才视为“新增”并写入 out_list；
 * - 函数不会对 out_list 大小或 used_map 指针做越界检查，调用方须保证缓冲足够大并正确初始化 used_map；
 * - 复杂度 O(idx_cnt)。
 */
static uint8_t collect_new_timepoint_indices(const uint8_t *idx_list, uint8_t idx_cnt, const uint8_t *used_map, uint8_t *out_list)
{
    uint8_t out = 0;
    for (uint8_t i = 0; i < idx_cnt; ++i) {
        uint8_t id = idx_list[i];
        if (id >= SMART_TOU_UNIT_MAX_CNT) continue;
        if (!used_map[id]) out_list[out++] = id;
    }
    return out;
}

/**
 * @brief 将单个运行时控制结果合并到 System_Running_Data 中
 *
 * 处理流程：
 * 1. 在 System_Running_Data 中查找与 tou_ctrl_result 除 reg_value 外完全一致的有效条目；
 *    - 若找到：更新该条目的 reg_value 并返回其索引。
 * 2. 若未找到匹配项：遍历寻找第一个无效槽（is_valid == false）；
 *    - 若找到：填充该槽的全部字段并将 is_valid 置为 true，返回其索引。
 * 3. 若无可用槽：返回 0xFF 表示失败（未插入）。
 *
 * @param tou_ctrl_result 指向待合并的控制结果（不可为 NULL）（reg_type/device_id/reg_addr/reg_value/is_valid）
 * @return 插入或更新的索引（0..SMART_TOU_UNIT_MAX_CNT-1），失败返回 0xFF
 */
static uint8_t Append_Smart_Tou_TimeCtrl_Status(smart_tou_ctrl_result_t *tou_ctrl_result)
{
    if (tou_ctrl_result == NULL) return 0xFF;

    /* 1) 查找已有匹配项（匹配 reg_type/device_id/reg_addr 且该项有效） */
    for (uint8_t i = 0; i < SMART_TOU_UNIT_MAX_CNT; ++i) {
        if (System_Running_Data[i].is_valid &&
            System_Running_Data[i].reg_type  == tou_ctrl_result->reg_type &&
            System_Running_Data[i].device_id == tou_ctrl_result->device_id &&
            System_Running_Data[i].reg_addr  == tou_ctrl_result->reg_addr) {
            /* 找到：仅更新值字段，保持其它字段不变 */
            System_Running_Data[i].reg_value = tou_ctrl_result->reg_value;
            Smart_Tou_Ctrl_Ctrl_PreTick = xTaskGetTickCount() * portTICK_PERIOD_MS;
            return i;
        }
    }

    /* 2) 未找到匹配项：寻找第一个空位插入（is_valid == false） */
    for (uint8_t i = 0; i < SMART_TOU_UNIT_MAX_CNT; ++i) {
        if (!System_Running_Data[i].is_valid) {
            System_Running_Data[i].reg_type  = tou_ctrl_result->reg_type;
            System_Running_Data[i].device_id = tou_ctrl_result->device_id;
            System_Running_Data[i].reg_addr  = tou_ctrl_result->reg_addr;
            System_Running_Data[i].reg_value = tou_ctrl_result->reg_value;
            System_Running_Data[i].is_valid  = true;
            Smart_Tou_Ctrl_Ctrl_PreTick = xTaskGetTickCount() * portTICK_PERIOD_MS;
            return i;
        }
    }

    /* 3) 无可用槽 */
    return 0xFF;
}

/**
 * @brief 周期性校验并收集运行时寄存器状态变化（保持控制检查）
 *
 * 行为说明（优化后）：
 * - 使用全局回调 g_device_reg_cb 读取设备寄存器值，读取间隔由 g_device_reg_check_interval_ms 控制（毫秒）。
 * - 使用时间差计算，兼容 tick 回绕：仅当自上次检查已超过间隔时才执行一次完整扫描。
 * - 对 System_Running_Data 中所有有效项（is_valid == true）逐项读取实际寄存器值：
 *   - 读取失败或读到的值与当前记录相同时跳过；
 *   - 若读取成功且值不同，会为差异结果动态分配 smart_tou_ctrl_result_t，拷贝当前记录，
 *     然后把指针入队到 xQueue_smart_tou_ctrl 供后续处理（若队列不可用或入队失败则释放内存并记录日志）。
 * - 内存分配失败或队列不可用不会中止整个扫描，仅记录错误并继续处理其他条目。
 *
 * 线程/实时注意：
 * - 函数在 FreeRTOS 环境下运行，使用 xQueueSend 带超时（pdMS_TO_TICKS(100)）。根据系统负载可调整超时。
 * - 回调实现必须是线程安全的且能在该上下文被调用（非阻塞为佳）。
 *
 * 参数：无（使用全局数据）
 * 返回：无
 */
static void Smart_Tou_Handle_Keeping_Control(void)
{
    if (g_device_reg_cb == NULL || g_device_reg_check_interval_ms == 0) {
        return;
    }

    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    /* 使用有符号差值以安全处理 wrap-around */
    int32_t elapsed = (int32_t)(now_ms - Smart_Tou_Ctrl_Ctrl_PreTick);
    if ((Smart_Tou_Ctrl_Ctrl_PreTick != 0) && (elapsed < (int32_t)g_device_reg_check_interval_ms)) {
        return;
    }
    Smart_Tou_Ctrl_Ctrl_PreTick = now_ms;

    if (xQueue_smart_tou_ctrl == NULL) {
        ESP_LOGW(TAG, "Smart_Tou: control queue not ready");
        return;
    }

    for (uint8_t i = 0; i < SMART_TOU_UNIT_MAX_CNT; ++i) {
        if (!System_Running_Data[i].is_valid) continue;

        uint16_t real_value = 0;
        /* 回调读取真实寄存器值；回调返回 false 表示读取失败 */
        if (!g_device_reg_cb(System_Running_Data[i].reg_type,
                             System_Running_Data[i].device_id,
                             System_Running_Data[i].reg_addr,
                             &real_value)) {
            continue;
        }

        /* 值未变则跳过 */
        if (real_value == System_Running_Data[i].reg_value) {
            continue;
        }

        /* 为变化结果分配并填充结构体 */
        smart_tou_ctrl_result_t *tou_ctrl_result = (smart_tou_ctrl_result_t *)iot_calloc(sizeof(*tou_ctrl_result));
        if (!tou_ctrl_result) {
            ESP_LOGE(TAG, "Smart_Tou_Handle_Keeping_Control: alloc fail idx=%u", (unsigned)i);
            continue; /* 不中断整体扫描 */
        }
        
        /* 拷贝原记录重新下发（保持原记录的 reg_value） */
        *tou_ctrl_result = System_Running_Data[i];

        /* 入队供后续处理，成功或失败都打印日志（纠错用） */
        if (xQueueSend(xQueue_smart_tou_ctrl, &tou_ctrl_result, pdMS_TO_TICKS(100)) != pdPASS) {
            ESP_LOGE(TAG, "Smart_Tou_Handle_Keeping_Control: enqueue FAIL reg=%u taget=%u now=%u",
                     (unsigned)System_Running_Data[i].reg_addr,
                     (unsigned)System_Running_Data[i].reg_value,
                     (unsigned)real_value);
            free(tou_ctrl_result);
            continue;
        } else {
            ESP_LOGW(TAG, "Smart_Tou_Handle_Keeping_Control: enqueue OK   reg=%u taget=%u now=%u",
                     (unsigned)System_Running_Data[i].reg_addr,
                     (unsigned)System_Running_Data[i].reg_value,
                     (unsigned)real_value);
        }
    }
}

/**
 * @brief 根据索引构造并入列一条 TOU 控制结果
 *
 * @param index 配置单元索引
 * @return int 0: 成功入列，-1: 失败（参数/分配/队列等）
 */
static int Smart_Tou_Enqueue_Result_ByIndex(int index)
{
    if (index < 0 || index >= SMART_TOU_UNIT_MAX_CNT) {
        ESP_LOGE(TAG, "Smart_Tou_Enqueue_Result_ByIndex: index out of range %d", index);
        return -1;
    }

    // 分配结果结构体，填充寄存器操作信息
    smart_tou_ctrl_result_t *tou_ctrl_result = (smart_tou_ctrl_result_t *)iot_calloc(sizeof(smart_tou_ctrl_result_t));
    if (!tou_ctrl_result) {
        ESP_LOGE(TAG, "Smart_Tou_Enqueue_Result_ByIndex: iot_calloc failed for idx=%d", index);
        return -1;
    }

    tou_ctrl_result->reg_type  = Smart_Tou_Ctrl_Config.smart_tou_unit[index].time_cfg.bits.reg_type;
    tou_ctrl_result->device_id = Smart_Tou_Ctrl_Config.smart_tou_unit[index].time_cfg.bits.device_id;
    tou_ctrl_result->reg_addr  = Smart_Tou_Ctrl_Config.smart_tou_unit[index].reg_cfg.reg_addr;
    tou_ctrl_result->reg_value = Smart_Tou_Ctrl_Config.smart_tou_unit[index].reg_cfg.reg_value;
    tou_ctrl_result->is_valid  = true;

    if (!Relay_Ctrl_Soc_Allows_Tou_Reg(tou_ctrl_result->reg_addr, tou_ctrl_result->reg_value)) {
        ESP_LOGW(TAG, "SOC gate blocked timed TOU idx=%d reg=%u val=%u soc=%u",
                 index,
                 (unsigned)tou_ctrl_result->reg_addr,
                 (unsigned)tou_ctrl_result->reg_value,
                 (unsigned)Relay_Ctrl_Get_System_Soc());
        free(tou_ctrl_result);
        return 1;
    }

    // 根据执行类型决定是否记录执行动作并维持
    if ( TOU_TIME_TYPE_POINT == Smart_Tou_Ctrl_Config.smart_tou_unit[index].time_cfg.bits.time_type ) {
        // Append_Smart_Tou_TimeCtrl_Status(tou_ctrl_result);
    } else if ( TOU_TIME_TYPE_QUANTUM == Smart_Tou_Ctrl_Config.smart_tou_unit[index].time_cfg.bits.time_type ) {
        Append_Smart_Tou_TimeCtrl_Status(tou_ctrl_result);
    }
    
    // 发送到TOU控制队列，若队列满则释放内存，防止内存泄漏
    if (!xQueue_smart_tou_ctrl || xQueueSend(xQueue_smart_tou_ctrl, &tou_ctrl_result, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Smart_Tou_Enqueue_Result_ByIndex: xQueueSend failed or queue not ready, idx=%d", index);
        free(tou_ctrl_result);
        return -1;
    }

    return 0;
}

/**
 * @brief 清除单个 Time_Quantum_Config 条目（若为 ACTIVE 则先触发结束点动作）
 *
 * 详细说明：
 * - 若条目为 ACTIVE：先分配结果结构体并填充 entry->ctrl 与 entry->ctrl.end_value，
 *   将指针入队给下游处理线程；若队列/分配失败则释放内存并记录错误；
 * - 最后将该条目整体 memset 为 0，等同于标记为 UNCONFIGURED。
 *
 * 注意：
 * - 期望调用者在外部保证并发安全（互斥已加），否则可能与正在运行的控制逻辑冲突。
 */
static void clear_time_quantum_entry(uint8_t index)
{
    /* 参数有效性检查：索引超出全局时间段表范围时直接返回 */
    if (index >= SMART_TOU_QUANTUM_MAX_CNT) return;

    tou_time_quantum_t *entry = &Time_Quantum_Config[index];

    /* 若处于激活态：发送结束点控制命令（使用 entry 中保存的 ctrl 信息） */
    if (entry->status == TOU_PERIOD_STATUS_ACTIVE) {
        smart_tou_ctrl_result_t *tou_ctrl_result = (smart_tou_ctrl_result_t *)iot_calloc(sizeof(smart_tou_ctrl_result_t));
        if (!tou_ctrl_result) {
            ESP_LOGE(TAG, "clear_time_quantum_entry: iot_calloc failed for quantum_id=%d", index);
        } else {
            /* 填充发送结构：使用 timequantum 中保存的控制目标与结束值（entry->ctrl / entry->ctrl.end_value） */
            tou_ctrl_result->reg_type  = entry->ctrl.reg_type;
            tou_ctrl_result->device_id = entry->ctrl.device_id;
            tou_ctrl_result->reg_addr  = entry->ctrl.reg_addr;
            tou_ctrl_result->reg_value = entry->ctrl.end_value;
            
            // 记录执行动作并维持
            Append_Smart_Tou_TimeCtrl_Status(tou_ctrl_result);
            
            // 发送到TOU控制队列，若队列满则释放内存，防止内存泄漏
            if (!xQueue_smart_tou_ctrl || xQueueSend(xQueue_smart_tou_ctrl, &tou_ctrl_result, pdMS_TO_TICKS(100)) != pdPASS) {
                ESP_LOGE(TAG, "clear_time_quantum_entry: xQueueSend failed or queue not ready, quantum_id=%d", index);
                free(tou_ctrl_result);
            }
        }
    }

    /* 最后将该时间段条目清零，标记为未配置/不可用 */
    memset(entry, 0, sizeof(*entry));
}

/**
 * @brief 清除全局 Time_Quantum_Config 中与指定 start/end 完全匹配的条目
 *
 * 详细说明：
 * - 遍历全局 Time_Quantum_Config 数组，若某项的 start_time_index 与 end_time_index
 *   同时等于给定值，则调用 clear_time_quantum_entry 删除该条目并记录日志。
 * - 该删除可确保在条目处于 ACTIVE 状态时，clear_time_quantum_entry 能触发对应的结束逻辑。
 *
 * 线程与调用约定：
 * - 本函数不做加锁，调用方需在外部保证并发安全（例如持有适当锁）。
 *
 * 参数：
 * - start_idx: 起始 time point 索引（uint8_t）
 * - end_idx:   结束 time point 索引（uint8_t），0xFF 亦按值匹配处理
 *
 * 返回：无
 */
static void clear_matching_time_quantum_entries(uint8_t start_idx, uint8_t end_idx)
{
    for (uint8_t gq = 0; gq < SMART_TOU_QUANTUM_MAX_CNT; ++gq) {
        if (Time_Quantum_Config[gq].start_time_index == start_idx &&
            Time_Quantum_Config[gq].end_time_index   == end_idx) {
            /*使用 clear_time_quantum_entry 保证 ACTIVE 时触发结束动作*/
            clear_time_quantum_entry(gq);
            ESP_LOGD(TAG, "detect_and_remove_deleted: cleared Time_Quantum_Config[%u] (start=%u end=%u)",
                     (unsigned)gq, (unsigned)start_idx, (unsigned)end_idx);
        }
    }
}

/**
 * @brief 校验时间段 old_quantum 的 ctrl 配置是否与其起止时间点配置严格一致
 *
 * 说明（针对当前实现）：
 * - 对参数做空指针与索引越界校验；要求 start_idx 和 end_idx 均小于 SMART_TOU_UNIT_MAX_CNT。
 * - 要求 old_quantum->status 已达到配置就绪阈值（>= TOU_PERIOD_STATUS_CONFIGURED_IDLE）。
 * - 校验规则（全部必须成立才返回 true）：
 *   1. reg_type 在 start 与 end 两端都等于 old_quantum->ctrl.reg_type；
 *   2. device_id 在 start 与 end 两端都等于 old_quantum->ctrl.device_id；
 *   3. reg_addr 在 start 与 end 两端都等于 old_quantum->ctrl.reg_addr；
 *   4. start_value 等于 start 端的 reg_value；
 *   5. end_value   等于 end   端的 reg_value；
 * - 注意：本函数按当前代码要求 end_idx 必须有效（不支持将 0xFF 当作 start-only）；调用方若使用 start-only 语义需先转换。
 *
 * @param old_quantum 待校验的时间段（包含 start_time_index、end_time_index、ctrl、status 等）
 * @return true  校验通过（所有关键字段同时匹配）
 *         false 校验失败或参数/索引/状态无效
 */
static bool Smart_Tou_Is_Ctrl_Valid(tou_time_quantum_t *old_quantum)
{
    /* 空指针检查：参数无效直接返回 false */
    if (old_quantum == NULL) return false;

    /* 提取区间起止索引；0xFF 表示仅有 start 点 */
    uint8_t start_idx = old_quantum->start_time_index;
    uint8_t end_idx   = old_quantum->end_time_index; /* 0xFF 表示 start-only */

    /* 边界保护：索引不得越界 */
    if (start_idx >= SMART_TOU_UNIT_MAX_CNT) return false;
    if (end_idx >= SMART_TOU_UNIT_MAX_CNT) return false;
    if (old_quantum->status < TOU_PERIOD_STATUS_CONFIGURED_IDLE) return false;

    /*检查当前本地日期是否满足 TOU 配置中的“日/周/月”选择*/
    if (!tou_day_is_valid(&Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg))  return false;
    if (!tou_day_is_valid(&Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].time_cfg))  return false;

    /* 以下按字段在 start 与 end 两端同时匹配来判断：
       - reg_type/reg_addr/device_id 要在两端都相等
       - start_value 必须等于 start 端的 reg_value
       - end_value 必须等于 end 端的 reg_value
       注：使用按位与 (&) 汇总布尔结果，最终要求所有条件为真 */
    bool is_type_present = ((old_quantum->ctrl.reg_type == Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg.bits.reg_type) && 
                            (old_quantum->ctrl.reg_type == Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].time_cfg.bits.reg_type)) ? true : false;

    bool is_dev_id_present = ((old_quantum->ctrl.device_id == Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg.bits.device_id) && 
                              (old_quantum->ctrl.device_id == Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].time_cfg.bits.device_id)) ? true : false;    

    bool is_addr_present = ((old_quantum->ctrl.reg_addr == Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].reg_cfg.reg_addr) && 
                            (old_quantum->ctrl.reg_addr == Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].reg_cfg.reg_addr)) ? true : false;    

    bool is_start_present = (old_quantum->ctrl.start_value == Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].reg_cfg.reg_value) ? true : false;   

    bool is_end_present = (old_quantum->ctrl.end_value == Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].reg_cfg.reg_value) ? true : false;   

    /* 全部字段均匹配则认为控制有效 */
    return (is_type_present & is_dev_id_present & is_addr_present & is_start_present & is_end_present);
}

/**
 * @brief 比较旧/新索引集合并处理被删除的 time point 引用
 *
 * 详细说明（逐步）：
 * 1) 当 current_idx_list 为空：直接清除全局所有 timequantum（使用 clear_time_quantum_entry，保证 ACTIVE 会触发结束逻辑）；
 * 2) 构建 present 映射用于快速存在性检测；
 * 3) 遍历 old_cfg 中每个已配置条目：
 *    - 若为 start-only：清除全局完全匹配的 start-only 条目并清除对应 time_cfg；
 *    - 若非 start-only：若任一端不存在，则清除全局完全匹配条目；若仅一端缺失，依据设计清除另一端的 time_cfg（并置写标志）。
 *
 * 关键点与假设：
 * - 假设每个 time point 只被一个 timequantum 使用，因此在仅一端缺失时可以直接清除配对点的 time_cfg；
 * - 使用 clear_time_quantum_entry 删除全局匹配项，以确保 ACTIVE 情况下能够正确触发结束动作；
 * - 调用方需在外部加锁以保证并发安全。
 *
 * @param old_cfg           备份的旧 Time_Quantum_Config（长度 SMART_TOU_QUANTUM_MAX_CNT）
 * @param current_idx_list  当前有效的 time point 索引数组（未排序也可）
 * @param current_cnt       当前索引数组长度
 */
static void detect_and_remove_deleted(const tou_time_quantum_t *old_cfg, const uint8_t *current_idx_list, uint8_t current_cnt)
{
    if (!old_cfg) return;

    /* 若当前索引集合为空，则清除所有旧 Time_Quantum_Config 条目并退出 */
    if (!current_idx_list || current_cnt == 0) {
        ESP_LOGI(TAG, "detect_and_remove_deleted: no current points, clear all Time_Quantum_Config");
        for (uint8_t q = 0; q < SMART_TOU_QUANTUM_MAX_CNT; ++q) {
            clear_time_quantum_entry(q);
        }
        return;
    }

    /* 构造当前存在映射：present_idx_map[idx] == 1 表示该 time point 仍存在 */
    uint8_t present_idx_map[SMART_TOU_UNIT_MAX_CNT] = {0};
    for (uint8_t i = 0; i < current_cnt; ++i) {
        uint8_t tp_idx = current_idx_list[i];
        if (tp_idx < SMART_TOU_UNIT_MAX_CNT) present_idx_map[tp_idx] = 1;
    }

    /* 遍历旧 timequantum，处理引用已删除点的情况 */
    for (uint8_t old_index = 0; old_index < SMART_TOU_QUANTUM_MAX_CNT; ++old_index) {
        const tou_time_quantum_t *old_quantum = &old_cfg[old_index];

        /* 跳过未配置条目 */
        if (old_quantum->status == TOU_PERIOD_STATUS_UNCONFIGURED) {
            continue;
        }

        uint8_t start_idx = old_quantum->start_time_index;
        uint8_t end_idx   = old_quantum->end_time_index; /* 0xFF 表示 start-only */

        /* 判断起/止点是否仍存在（对 start-only 仅关注 start，end直接视为被删除） */
        bool is_start_present = (start_idx < SMART_TOU_UNIT_MAX_CNT) && (present_idx_map[start_idx] != 0);
        bool is_end_present = false;
        if (old_quantum->status != TOU_PERIOD_STATUS_START_ONLY) {
            is_end_present = (end_idx < SMART_TOU_UNIT_MAX_CNT && end_idx != 0xFF) && (present_idx_map[end_idx] != 0);
        }

        /* 若两端仍存在则通常保持不变，但仍需校验 ctrl 有效性 */
        if (is_start_present && is_end_present) {
            /* 校验时间段 ctrl 是否仍然有效 */
            if (Smart_Tou_Is_Ctrl_Valid(old_quantum)) {
                continue;
            }
            
            /* ctrl 无效：记录详细信息并清理全局匹配条目 */
            clear_matching_time_quantum_entries(start_idx, end_idx);
            ESP_LOGI(TAG, "detect_and_remove_deleted: quantum [%u] ctrl invalid, clearing matched entries (start=%u end=%u)",
                     (unsigned)old_index, (unsigned)start_idx, (unsigned)end_idx);

            continue;
        }

        /* 从全局 Time_Quantum_Config 中删除与 old_quantum 完全匹配的条目*/
        clear_matching_time_quantum_entries(start_idx, end_idx);

        /* 两端均不存在：记录并继续 */
        if (!is_start_present && !is_end_present) {
            ESP_LOGI(TAG, "detect_and_remove_deleted: removed quantum[%u] both endpoints missing start=%u end=%u", (unsigned)old_index, (unsigned)start_idx, (unsigned)end_idx);
            continue;
        }

        /* 仅一端缺失：删除另一端对应的 time point 配置（在当前假设下该点只被此 quantum 使用） */
        if (!is_start_present && is_end_present) {
            /* start 被删除，清除 end 对应的 time_cfg */
            memset(&Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].time_cfg, 0, sizeof(tou_time_config_t));
            Smart_Tou_Ctrl_Config_Write_Flag |= 1;
            ESP_LOGW(TAG, "detect_and_remove_deleted: cleared paired time point config idx=%u (was end)", (unsigned)end_idx);
            continue;
        } else if (is_start_present && !is_end_present) {
            /* end 被删除，清除 start 对应的 time_cfg */
            memset(&Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg, 0, sizeof(tou_time_config_t));
            Smart_Tou_Ctrl_Config_Write_Flag |= 1;
            ESP_LOGW(TAG, "detect_and_remove_deleted: cleared paired time point config idx=%u (was start)", (unsigned)start_idx);
            continue;
        }
    }
}

/**
 * @brief 追加构建：对已排序的新增 idx_list 配对并写入 Time_Quantum_Config 空槽
 *
 * 详细注释（步骤）：
 * - 检查输入有效性，初始化 used_pos（按 idx_cnt 大小）；
 * - 对每个未被消费的位置 pi：
 *   1. 在后续位置查找第一个兼容的结束点 found_j；
 *   2. 从 next_slot 开始环形搜索全局第一个 UNCONFIGURED 槽位 slot；
 *   3. 如找到 slot，初始化并填充该 slot 的 start/end/ctrl/status；否则提前退出；
 *   4. 若配对成功，标记 used_pos[pi] 与 used_pos[found_j]；若未找到结束点，创建 start-only 并标记 used_pos[pi]。
 *
 * 性能注意：
 * - next_slot 避免每次都从 0 开始扫描空槽，提高插入效率；
 * - 若 SMART_TOU_QUANTUM_MAX_CNT 很大且频繁重建，可考虑维护空槽队列以进一步优化。
 *
 * 返回值：实际写入的 timequantum 数量。
 */
static uint8_t append_build_from_sorted_idx_list(const uint8_t *idx_list, uint8_t idx_cnt)
{
    if (!idx_list || idx_cnt == 0) return 0;

    uint8_t created = 0;
    uint8_t used_pos[idx_cnt];
    memset(used_pos, 0, idx_cnt);

    /* 插入时从 next_slot 开始查找空槽，避免每次都从 0 开始扫描 */
    uint8_t next_slot = 0;

    for (uint8_t pi = 0; pi < idx_cnt; ++pi) {
        if (used_pos[pi]) continue;

        uint8_t start_idx = idx_list[pi];
        if (start_idx >= SMART_TOU_UNIT_MAX_CNT) continue;

        /*检查当前本地日期是否满足 TOU 配置中的“日/周/月”选择*/
        if (!tou_day_is_valid(&Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg))  continue;

        /* 在后续项中寻找第一个兼容的结束点（优先最近的后续点） */
        int found_j = -1;
        for (uint8_t pj = pi + 1; pj < idx_cnt; ++pj) {
            if (used_pos[pj]) continue;
            uint8_t cand = idx_list[pj];
            if (cand >= SMART_TOU_UNIT_MAX_CNT) continue;
            if (!tou_units_compatible(start_idx, cand)) continue;
            if (tou_day_is_valid(&Smart_Tou_Ctrl_Config.smart_tou_unit[cand].time_cfg)) { found_j = pj; break; }
        }

        /* 在全局表中查找下一个未配置槽（环形搜索 starting at next_slot） */
        int slot = -1;
        for (uint8_t off = 0; off < SMART_TOU_QUANTUM_MAX_CNT; ++off) {
            uint8_t s = (next_slot + off) % SMART_TOU_QUANTUM_MAX_CNT;
            if (Time_Quantum_Config[s].status == TOU_PERIOD_STATUS_UNCONFIGURED) {
                slot = s;
                next_slot = (s + 1) % SMART_TOU_QUANTUM_MAX_CNT; /* 下一次从此后继续查找 */
                break;
            }
        }
        if (slot < 0) break; /* 无空位，提前退出 */

        /* 填充起点及控制目标（以起点的 reg_cfg 为基础） */
        memset(&Time_Quantum_Config[slot], 0, sizeof(Time_Quantum_Config[slot]));
        Time_Quantum_Config[slot].start_time_index = start_idx;
        Time_Quantum_Config[slot].ctrl.reg_type  = Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg.bits.reg_type;
        Time_Quantum_Config[slot].ctrl.device_id = Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg.bits.device_id;
        Time_Quantum_Config[slot].ctrl.reg_addr  = Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].reg_cfg.reg_addr;
        Time_Quantum_Config[slot].ctrl.start_value = Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].reg_cfg.reg_value;

        if (found_j >= 0) {
            /* 配对成功：填充结束点与结束值，标记为 CONFIGURED_IDLE */
            uint8_t end_idx = idx_list[found_j];
            Time_Quantum_Config[slot].end_time_index = end_idx;
            Time_Quantum_Config[slot].ctrl.end_value = Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].reg_cfg.reg_value;
            Time_Quantum_Config[slot].status = TOU_PERIOD_STATUS_CONFIGURED_IDLE;

            used_pos[pi] = 1;
            used_pos[found_j] = 1;
            ESP_LOGI(TAG, "Appended quantum[%d]: start=%u end=%u status=%d", (int)slot, (unsigned)start_idx, (unsigned)end_idx, Time_Quantum_Config[slot].status);
        } else {
            /* 未找到结束点：创建 start-only 条目 */
            Time_Quantum_Config[slot].end_time_index = 0xFF;
            Time_Quantum_Config[slot].status = TOU_PERIOD_STATUS_START_ONLY;
            Time_Quantum_Config[slot].ctrl.end_value = 0;

            used_pos[pi] = 1;
            ESP_LOGW(TAG, "Appended start-only quantum[%d]: start=%u status=%d", (int)slot, (unsigned)start_idx, Time_Quantum_Config[slot].status);
        }

        created++;
    }

    return created;
}

/**
 * @brief 重建或增量更新全局 Time_Quantum_Config（主控函数）
 *
 * 说明（关键步骤）：
 * - 收集并排序当前有效 timepoint 索引；
 * - REBUILD：清空全表并从 idx_list 重建（调用 append_build_from_sorted_idx_list）；
 * - UPDATE：
 *     1. 使用备份检测并移除对已删除点的引用（detect_and_remove_deleted）；
 *     2. 基于当前全局表生成 used_map（已被占用的真实索引）；
 *     3. 收集新增索引并追加构建（append_build_from_sorted_idx_list）。
 *
 * 返回：当前全局有效 timequantum 数量（status >= CONFIGURED_IDLE 计数）。
 */
static uint8_t Smart_Tou_RebuildOrUpdate_TimeQuantumConfig(timequantum_build_mode_t mode)
{
    uint8_t idx_list[SMART_TOU_UNIT_MAX_CNT];
    uint8_t idx_cnt = 0;

    ESP_LOGD(TAG, "Smart_Tou_RebuildOrUpdate_TimeQuantumConfig mode=%d", (int)mode);

    /* 收集当前有效的 quantum 点索引（未排序） */
    idx_cnt = collect_quantum_idx(idx_list);
    if (idx_cnt == 0) {
        ESP_LOGI(TAG, "Smart_Tou_RebuildOrUpdate_TimeQuantumConfig: no quantum time points found.");
        if (mode == TIMEQUANTUM_BUILD_REBUILD) {
            memset(Time_Quantum_Config, 0, sizeof(Time_Quantum_Config));
        } else {
            /* UPDATE 且无点：移除所有旧引用（传入 NULL 表示清空） */
            tou_time_quantum_t backup_dummy[SMART_TOU_QUANTUM_MAX_CNT];
            memcpy(backup_dummy, Time_Quantum_Config, sizeof(backup_dummy));
            detect_and_remove_deleted(backup_dummy, NULL, 0);
        }
        return 0;
    }

    /* 按时间排序（升序） */
    qsort(idx_list, idx_cnt, sizeof(idx_list[0]), compare_idx_by_time);

    if (mode == TIMEQUANTUM_BUILD_UPDATE) {
        /* UPDATE 模式：只追加新增索引，先构建旧配置使用映射并筛出新增索引 */
        tou_time_quantum_t old_cfg[SMART_TOU_QUANTUM_MAX_CNT];
        uint8_t used_map[SMART_TOU_UNIT_MAX_CNT];
        uint8_t new_list[SMART_TOU_UNIT_MAX_CNT];
        uint8_t new_cnt = 0;

        /* 备份当前全局表以便检测已被删除的引用（只在 UPDATE 模式进行） */
        memcpy(old_cfg, Time_Quantum_Config, sizeof(old_cfg));

        /* 删除旧配置中引用已删除的点（传入当前完整 idx_list） */
        detect_and_remove_deleted(old_cfg, idx_list, idx_cnt);

        /* 标记当前全局表中已被占用的真实索引（用于筛出新增点） */
        mark_used_timepoint_indices(Time_Quantum_Config, used_map);

        /* 从已排序的 idx_list 中收集不在 used_map 的“新增”索引 */
        new_cnt = collect_new_timepoint_indices(idx_list, idx_cnt, used_map, new_list);

        /* 仅对新增索引追加构建 */
        if (new_cnt > 0) {
            uint8_t appended = append_build_from_sorted_idx_list(new_list, new_cnt);
            ESP_LOGI(TAG, "Smart_Tou_RebuildOrUpdate_TimeQuantumConfig: appended %u new timequantums.", (unsigned)appended);
        } else {
            ESP_LOGD(TAG, "Smart_Tou_RebuildOrUpdate_TimeQuantumConfig: no new timepoints to append.");
        }
    } else {
        /* REBUILD：清空全局并从头构建所有时间段 */
        memset(Time_Quantum_Config, 0, sizeof(Time_Quantum_Config));
        uint8_t builded = append_build_from_sorted_idx_list(idx_list, idx_cnt);
        ESP_LOGI(TAG, "Smart_Tou_RebuildOrUpdate_TimeQuantumConfig: rebuilt %u new timequantums from %u indices.", (unsigned)builded, (unsigned)idx_cnt);
    }

    /* 统计当前有效的 timequantum 数量并返回 */
    uint8_t total = 0;
    for (uint8_t q = 0; q < SMART_TOU_QUANTUM_MAX_CNT; ++q) {
        if (Time_Quantum_Config[q].status >= TOU_PERIOD_STATUS_CONFIGURED_IDLE) total++;
    }

    ESP_LOGI(TAG, "Smart_Tou_RebuildOrUpdate_TimeQuantumConfig: total timequantums=%u", (unsigned)total);
    return total;
}

/**
 * @brief 处理时间段（time quantum）控制：触发起止动作并维护状态机
 *
 * 说明（基于当前实现）：
 * - 周期调用；计算当天本地 00:00:00 基点 t_base 与当前日内秒 time_sec；
 * - 在日切（t_base > pre_time）或配置更新（Smart_Tou_Ctrl_Config_update_Flag）时：
 *     - 若日切：先对前一日仍处于 ACTIVE 的条目触发其 end 操作，再完全重建 Time_Quantum_Config；
 *     - 若配置更新：执行增量更新 Time_Quantum_Config；
 * - 遍历 Time_Quantum_Config，有效条目按状态处理：
 *     - CONFIGURED_IDLE：当 time_sec 处于 [start_sec, end_sec) 时触发 start 并置为 ACTIVE；
 *     - ACTIVE：仅当该区间存在结束点（tq->end_time_index != 0xFF）且当前时间不在活动区间 [start_sec, end_sec) 时，
 *               触发 end 操作并将状态置为 TOU_PERIOD_STATUS_CONFIGURED_IDLE。
 *     - START_ONLY：当 time_sec >= start_sec 时直接移除该条目（一次性触发）；
 * - start/end 判定使用 ">="，以容错错过精确秒的情况；对无效区间（start_sec >= end_sec）跳过处理（start-only 除外）。
 */
static void Smart_Tou_Handle_TimeQuantum_Control(time_t now_time)
{
    static time_t pre_time = 0;
    bool day_first_running_flag = false;

    /* 计算今天本地 00:00:00 的 time_t 与当前日内秒数 */
    struct tm tm_now;
    localtime_r(&now_time, &tm_now);
    tm_now.tm_hour = tm_now.tm_min = tm_now.tm_sec = 0;
    time_t t_base = mktime(&tm_now); /* 本地00:00:00 */
    uint32_t time_sec = (uint32_t)(now_time - t_base);

    /* 检测日切（只在本函数内维护上一次的 t_base） */
    if (t_base > pre_time) {
        day_first_running_flag = true;
        pre_time = t_base;
    }

    /* 日切或配置更新时重建/更新 timequantum 表 */
    if (day_first_running_flag || Smart_Tou_Ctrl_Config_update_Flag) {
        ESP_LOGI(TAG, "Rebuild/Update triggered: day_first=%d config_update=%d", (int)day_first_running_flag, (int)Smart_Tou_Ctrl_Config_update_Flag);
        if (day_first_running_flag) {
            /* 若前一天有尚未结束的 ACTIVE 段，先触发其结束动作（防止丢失） */
            for (uint8_t q = 0; q < SMART_TOU_QUANTUM_MAX_CNT; ++q) {
                if (Time_Quantum_Config[q].status == TOU_PERIOD_STATUS_ACTIVE) {
                    uint8_t start_idx = Time_Quantum_Config[q].end_time_index;
                    uint8_t end_idx = Time_Quantum_Config[q].end_time_index;

                    /*判断是否为全天时间段*/
                    if ((start_idx < SMART_TOU_UNIT_MAX_CNT) && (end_idx < SMART_TOU_UNIT_MAX_CNT)) {
                        if (( 1 >= Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg.bits.time_sec ) 
                            && ( 86400 == Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].time_cfg.bits.time_sec ))
                        {
                            /*检查当前本地日期是否满足 TOU 配置中的“日/周/月”选择*/
                            if (tou_day_is_valid(&Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg) 
                                && tou_day_is_valid(&Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].time_cfg)) {
                                /*判定为全天时间段，且次日同样生效，本次不执行退出，等待直接在下个时间段重新生效*/
                                continue;
                            }
                        }
                    }

                    if (end_idx < SMART_TOU_UNIT_MAX_CNT) {
                        /* 将结束点入队，后续任务执行实际寄存器写入 */
                        Smart_Tou_Enqueue_Result_ByIndex((int)end_idx);
                    }
                }
            }
            /* 完全重建：清空并从头构建（保留设计上 append 的实现） */
            Smart_Tou_RebuildOrUpdate_TimeQuantumConfig(TIMEQUANTUM_BUILD_REBUILD);
        } else {
            /* 增量更新：仅追加新增点并移除被删除引用 */
            Smart_Tou_RebuildOrUpdate_TimeQuantumConfig(TIMEQUANTUM_BUILD_UPDATE);
        }

        /* 重置标志 */
        day_first_running_flag = false;
    }

    /* 遍历全局 timequantum 表，按状态触发 start/end（并统计有效条目数） */
    uint8_t valid_cnt = 0;
    for (uint8_t q = 0; q < SMART_TOU_QUANTUM_MAX_CNT; ++q) {
        tou_time_quantum_t *tq = &Time_Quantum_Config[q];

        /* 仅统计并处理已配置的条目（包含 CONFIGURED_IDLE/ACTIVE/START_ONLY 等） */
        if (tq->status == TOU_PERIOD_STATUS_UNCONFIGURED) continue;
        valid_cnt++;

        uint8_t start_idx = tq->start_time_index;
        uint8_t end_idx = tq->end_time_index;

        /* 获取起/止点的 time_sec（Smart_Tou_Ctrl_Config.smart_tou_unit[].time_cfg.bits.time_sec 为 17bit，读取时确保掩码） */
        const uint32_t DAYSEC_MASK = 0x1FFFFU; /* 17 bits */
        uint32_t start_sec = (start_idx < SMART_TOU_UNIT_MAX_CNT) ? (Smart_Tou_Ctrl_Config.smart_tou_unit[start_idx].time_cfg.bits.time_sec & DAYSEC_MASK) : UINT32_MAX;
        uint32_t end_sec   = (end_idx < SMART_TOU_UNIT_MAX_CNT && end_idx != 0xFF) ? (Smart_Tou_Ctrl_Config.smart_tou_unit[end_idx].time_cfg.bits.time_sec & DAYSEC_MASK) : UINT32_MAX;

        /* 若结束时间早于等于起始时间（无效区间），跳过处理（start-only 除外） */
        if (start_sec >= end_sec && tq->status != TOU_PERIOD_STATUS_START_ONLY) {
            ESP_LOGW(TAG, "Quantum[%u] invalid interval start=%u end=%u, skip", (unsigned)q, (unsigned)start_sec, (unsigned)end_sec);
            continue;
        }

        switch (tq->status) {
            case TOU_PERIOD_STATUS_CONFIGURED_IDLE:
                /* 到达或越过起点且尚未到结束点时触发 start -> 进入 ACTIVE */
                if (time_sec >= start_sec && time_sec < end_sec) {
                    if (Smart_Tou_Enqueue_Result_ByIndex((int)start_idx) == 0) {
                        tq->status = TOU_PERIOD_STATUS_ACTIVE;           /* 防止重复触发 start */
                        ESP_LOGI(TAG, "TimeQuantum[%u] start (idx=%u) triggered at %u (start_sec=%u end_sec=%u)", (unsigned)q, (unsigned)start_idx, (unsigned)time_sec, (unsigned)start_sec, (unsigned)end_sec);
                    }
                }
                break;

            case TOU_PERIOD_STATUS_ACTIVE:
                /* 仅在存在结束点时判断；增加 start_sec 检查以处理起始点被修改的情况 */
                if (tq->end_time_index != 0xFF && (time_sec < start_sec || time_sec >= end_sec)) {
                    Smart_Tou_Enqueue_Result_ByIndex((int)end_idx); /* enqueue end op */
                    tq->status = TOU_PERIOD_STATUS_CONFIGURED_IDLE; /* 可在次日再次触发 */
                    ESP_LOGI(TAG, "TimeQuantum[%u] end (idx=%u) triggered at %u (start_sec=%u end_sec=%u)",
                             (unsigned)q, (unsigned)end_idx, (unsigned)time_sec, (unsigned)start_sec, (unsigned)end_sec);
                }
                break;

            case TOU_PERIOD_STATUS_START_ONLY:
                /* start-only：移除该条目（防止重复） */
                if (time_sec >= start_sec) {
                    /* 直接清零条目（不触发 end） */
                    memset(tq, 0, sizeof(*tq));
                    ESP_LOGW(TAG, "TimeQuantum[%u] start-only (idx=%u) triggered and removed at %u (start_sec=%u)", (unsigned)q, (unsigned)start_idx, (unsigned)time_sec, (unsigned)start_sec);
                }
                break;

            default:
                ESP_LOGE(TAG, "Quantum[%u] unknown status=%d", (unsigned)q, (int)tq->status);
                break;
        }
    }

    // 更新有效时间段数量
    Smart_Tou_Ctrl_Time_Quantum_Valid_Cnt = valid_cnt;
}

/**
 * @brief 智能TOU控制模块初始化
 * 
 * 1. 创建TOU控制队列（用于任务间通信）。
 * 2. 加载或初始化TOU控制配置信息（从文件读取，若无则初始化默认配置并写入文件）。
 * 
 * @return 0 初始化成功
 * @return -1 初始化失败
 */
int Smart_Tou_Ctrl_Module_Init(void)
{
    char path[30] = {0};

    // 1. 创建智能TOU控制模块队列（队列长度和元素大小可根据实际需求调整）
    if (xQueue_smart_tou_ctrl == NULL) {
        xQueue_smart_tou_ctrl = xQueueCreate(SMART_TOU_UNIT_SINGLE_PROCESS_CNT, sizeof(smart_tou_ctrl_result_t *));
        if (xQueue_smart_tou_ctrl == NULL) {
            ESP_LOGE(TAG, "Failed to create smart TOU control queue!");
            return -1;
        }
    }

    // 2. 创建互斥信号量用于保护配置/运行态数据；
    if (xSmartTouMutex == NULL) {
        xSmartTouMutex = xSemaphoreCreateMutex();
        if (xSmartTouMutex == NULL) {
            ESP_LOGE(TAG, "Smart_Tou_Semaphores_Init: create mutex failed");
        }
    }
    
    // 3. 获取智能TOU控制模块参数文件路径
    SMART_TOU_FILE_PATH(path);

    // 4. 从文件读取配置信息
    int bytes_read = fs_file_read_at(path, 0, (uint8_t*)&Smart_Tou_Ctrl_Config, sizeof(smart_tou_ctrl_config_t));
    
    // 5. 检查读取是否成功，否则初始化默认配置并写入文件
    if (bytes_read != sizeof(smart_tou_ctrl_config_t)) 
    {
        ESP_LOGW(TAG, "Config file read failed or invalid. Initializing default config.");

        // 初始化默认配置
        memset(&Smart_Tou_Ctrl_Config, 0, sizeof(Smart_Tou_Ctrl_Config));
        Smart_Tou_Ctrl_Config.smart_tou_info.diff_type = 1; // 默认类型：当前内容
        Smart_Tou_Ctrl_Config.smart_tou_info.time_num = SMART_TOU_UNIT_MAX_CNT; // 默认时间点数量

        // 写入默认配置到文件
        if (fs_file_write_at(path, 0, (uint8_t*)&Smart_Tou_Ctrl_Config, sizeof(smart_tou_ctrl_config_t)) == sizeof(smart_tou_ctrl_config_t)) {
            ESP_LOGI(TAG, "Default TOU config initialized and written to file.");
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write default TOU config to file!");
            return -1;
        }
    }

    // 本模块开发调试，出厂需屏蔽
#if 0    
    // Smart_Tou_Ctrl_Module_DEBUG();
    Smart_Tou_Test_Preset_Quanta();
#endif

    ESP_LOGI(TAG, "Smart TOU Control Module initialized successfully.");
    return 0;
}

/**
 * @brief 获取智能TOU控制模块配置信息的指针
 * 
 * 返回指向TOU配置结构体的uint8_t指针，便于外部以字节流方式访问或传递配置数据。
 * 
 * - 当 is_write 为1时，返回写入缓存 Ctrl_Config_Wr 的指针（用于外部写入配置）。
 * - 当 is_write 为0时，返回当前生效配置 Smart_Tou_Ctrl_Config 的指针（用于外部读取配置）。
 * 
 * 建议在查表、配置同步等场景下调用本函数。
 * 
 * @param is_write  1-获取写入缓存指针，0-获取当前配置指针
 * @return uint8_t* 指向配置结构体的指针
 */
uint8_t* Smart_Tou_Ctrl_Config_Get_Ptr(uint8_t is_write)
{
    if (is_write) {
        // 返回写入缓存指针，供外部写入配置
        return (uint8_t*)&Smart_Tou_Ctrl_Config_Wr;
    } else {
        // 返回当前生效配置指针，供外部读取配置
        return (uint8_t*)&Smart_Tou_Ctrl_Config;
    }
}

/**
 * @brief 将当前 Smart_Tou_Ctrl_Config 写入配置文件
 * @return 0 成功，-1 失败
 */
static int Smart_Tou_Write_Config_To_File(void)
{
    char path[64] = {0};
    SMART_TOU_FILE_PATH(path);

    if (fs_file_write_at(path, 0, (uint8_t *)&Smart_Tou_Ctrl_Config, sizeof(smart_tou_ctrl_config_t))
        != (int)sizeof(smart_tou_ctrl_config_t)) {
        ESP_LOGE(TAG, "Smart_Tou_Write_Config_To_File: write failed");
        return -1;
    }

    ESP_LOGI(TAG, "Smart_Tou_Write_Config_To_File: config written to %s", path);
    return 0;
}

/**
 * @brief 将Ctrl_Config_Wr的数据（指定偏移和长度）写入Smart_Tou_Ctrl_Config，并预备同步更新到配置文件
 *  
 * @param offset   写入偏移（字节）
 * @param wr_len   写入长度（字节）
 * @return int     0-成功，-1-失败（如参数非法、写入越界、文件写入失败等）
 */
int Smart_Tou_Ctrl_Config_Commit(size_t offset, size_t wr_len)
{
    char path[30] = {0};

    // 参数检查：越界
    if (offset + wr_len > sizeof(smart_tou_ctrl_config_t)) {
        ESP_LOGE(TAG, "Write range out of config struct bounds!");
        return -1;
    }

    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (Smart_Tou_TakeMutex(500) != pdTRUE) {
        ESP_LOGW(TAG, "Smart_Tou_Ctrl_Config_Commit: mutex busy, skip");
        return -1;
    }

    // 拷贝数据
    memcpy((uint8_t*)&Smart_Tou_Ctrl_Config + offset, (uint8_t*)&Smart_Tou_Ctrl_Config_Wr + offset, wr_len);

    // 维持只读区内容
    Smart_Tou_Ctrl_Config.smart_tou_info.value = 0;
    Smart_Tou_Ctrl_Config.smart_tou_info.diff_type = 1; // 默认类型：当前内容
    Smart_Tou_Ctrl_Config.smart_tou_info.time_num = SMART_TOU_UNIT_MAX_CNT; // 默认时间点数量

    // 设置文件更新标志
    Smart_Tou_Ctrl_Config_update_Flag = 1;

    /* 释放互斥 */
    Smart_Tou_GiveMutex();

    // 将当前 Smart_Tou_Ctrl_Config 写入配置文件
    Smart_Tou_Write_Config_To_File();

    ESP_LOGI(TAG, "Smart_Tou_Ctrl_Config updated and written to file. offset=%u, len=%u", (unsigned)offset, (unsigned)wr_len);
    return 0;
}

/**
 * @brief 首次调用时查找最近一次有效TOU触发并发送命令
 * 
 * 该函数用于在系统首次启动或TOU模块首次运行时，回溯历史区间，查找所有TOU配置点中最近一次有效的触发时间点，
 * 并将该触发点对应的控制命令通过队列发送给后续处理任务，确保系统上电后不会遗漏历史有效事件。
 * 
 * 实现流程：
 * 1. 遍历所有TOU配置点，分别调用 Tou_Point_Last_Trigger 查找每个点在回溯区间内的最近一次有效触发时间。
 * 2. 记录所有点中最近的（最大且不大于当前时间）触发时间及其配置点索引。
 * 3. 若找到有效触发点，则构造对应的控制命令结构体，并通过队列发送给下游任务。
 * 4. 若内存分配或队列发送失败，及时释放资源并打印错误日志。
 * 
 * @param now_time 当前时间戳（秒，time_t）
 */
static void Smart_Tou_First_Trigger_Handle(time_t now_time)
{
    int last_trigger_index = -1;
    time_t last_trigger = 0;

    // 遍历所有TOU配置点，寻找最近的一个触发点
    for (int i = 0; i < Smart_Tou_Ctrl_Config.smart_tou_info.time_num; ++i) {
        tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[i].time_cfg;
        time_t find_result = Tou_Point_Last_Trigger(tou_cfg, now_time);
        if (find_result > last_trigger) {
            last_trigger = find_result;
            last_trigger_index = i;
        }
    }

    // 若找到有效触发点，则发送控制命令
    if (last_trigger_index >= 0) {
        // TOU 控制结果入列
        Smart_Tou_Enqueue_Result_ByIndex(last_trigger_index);
    }
}

/**
 * @brief  处理时间点触发的 TOU 控制逻辑
 *
 * 将 Smart_Tou_Ctrl_Module_Process 中遍历/触发/队列发送等时间点处理逻辑集中到此函数。
 *
 * @param[in] now_time 当前时间戳
 */
static void Smart_Tou_Handle_TimePoint_Control(time_t now_time)
{
    static time_t pre_time = 0;
    uint8_t Valid_Unit_Cnt = 0;

    // 首次调用或系统时间异常（如回拨）
    if (pre_time == 0 || now_time <= pre_time) {
        // 检查是否为首次调用
        if ( pre_time == 0 ) {
            // 系统上电后对过去任务的追溯逻辑尚不清晰，暂不实现
            // Smart_Tou_First_Trigger_Handle(now_time);
        }

        // 直接更新时间并退出
        pre_time = now_time;
        return;
    }

    // 生成区间内所有天的本地00:00:00数组，便于批量TOU点判断
    tou_time_base_t base = tou_time_get_base_arr(pre_time, now_time);

    // 只有分配成功才继续处理
    if (!base.t_base_arr) {
        ESP_LOGE(TAG, "Failed to allocate t_base_arr or invalid time range. pre_time=%lld, now_time=%lld", pre_time, now_time);
    } else {
        // 遍历所有TOU配置点，判断是否有触发
        for (int i = 0; i < Smart_Tou_Ctrl_Config.smart_tou_info.time_num; ++i) {
            tou_time_config_t *tou_cfg = &Smart_Tou_Ctrl_Config.smart_tou_unit[i].time_cfg;

            // 检查是否为有效时间节点
            if ((TOU_TIME_TYPE_POINT != tou_cfg->bits.time_type) || tou_time_cfg_invalid(tou_cfg)) {
                continue;
            } else {
                Valid_Unit_Cnt++;
            }
            
            // 判断该TOU点在区间内是否触发
            if (tou_time_is_trigger(tou_cfg, &base)) {
                // TOU 控制结果入列
                Smart_Tou_Enqueue_Result_ByIndex(i);
            }
        }

        // 释放天基准数组内存，防止内存泄漏
        free(base.t_base_arr);
    }

    // 更新时间戳，确保下次只处理新时间段
    pre_time = now_time;

    // 更新有效时间点数量
    Smart_Tou_Ctrl_Time_Point_Valid_Cnt = Valid_Unit_Cnt;
}

/**
 * @brief 检查并更新 Smart TOU 控制状态
 * 
 * 在首次调用或配置发生更新时遍历所有 TOU 时间点，判断是否有触发的寄存器地址，
 * 并设置对应的状态位（例如 AC_Total / DC_Total）。
 *
 * 行为与并发约定：
 *   仅在首次运行或 Smart_Tou_Ctrl_Config_update_Flag 为 true 时重算，避免频繁无谓写回；
 *   仅在计算结果与全局 Smart_Tou_Ctrl_Status.all 不同时写回该全局变量；
 *   如果 Smart_Tou_Ctrl_Status 或 Smart_Tou_Ctrl_Config_update_Flag 在多任务并发访问，调用方应在外部加锁（互斥或原子操作）以保证线程安全。
 * 
 * @note 该函数不阻塞、无参数、无返回值；建议将其置于定时或配置变更处理路径中调用。
*/
static void Smart_Tou_Ctrl_Status_Check(void)
{
    /* 首次运行或者配置发生变更时才重新计算状态，避免频繁无谓写操作 */
    static bool first_flag = true;
    if (!first_flag && !Smart_Tou_Ctrl_Config_update_Flag) {
        return;
    }

    /* 本次计算的临时状态，先清零 */
    smart_ctrl_status_struct status = { .all = 0 };

    /* 防护：若没有配置点则直接写回并清标志 */
    if (Smart_Tou_Ctrl_Config.smart_tou_info.time_num == 0) {
        Smart_Tou_Ctrl_Status.all = status.all;
        first_flag = false;
        return;
    }

    /* 遍历所有 TOU 配置点，只有有效时间节点才参与判断 */
    for (int i = 0; i < (int)Smart_Tou_Ctrl_Config.smart_tou_info.time_num; ++i) {
        /* 取引用方便阅读并减少重复访问 */
        const smart_tou_unit_config_t *unit = &Smart_Tou_Ctrl_Config.smart_tou_unit[i];

        /* 无效或未配置的时间节点跳过 */
        if (tou_time_cfg_invalid(&unit->time_cfg)) {
            continue;
        }
        ESP_LOGI(TAG, "Smart_Tou_Ctrl_Status_Check: unit->reg_cfg.reg_addr=%d", unit->reg_cfg.reg_addr);
        /* 根据配置的寄存器地址设置对应的状态位 */
        switch (unit->reg_cfg.reg_addr) {
            case 2012:
                status.bit.DC_Total = 1;
                break;
            case 2011:
                status.bit.AC_Total = 1;
                break;
            case 2093: // AC支路开关设置
                if (unit->reg_cfg.reg_value & 3) {
                    status.bit.AC_Branch_1 = 1;
                }
                if ((unit->reg_cfg.reg_value >> 2) & 3) {
                    status.bit.AC_Branch_2 = 1;
                }
                if ((unit->reg_cfg.reg_value >> 4) & 3) {
                    status.bit.AC_Branch_3 = 1;
                }
                if ((unit->reg_cfg.reg_value >> 6) & 3) {
                    status.bit.AC_Branch_4 = 1;
                }
                break;
            default:
                break;
        }
    }

    /* 仅在实际变化时写回全局状态，减少对共享变量的写操作 */
    if (Smart_Tou_Ctrl_Status.all != status.all) {
        Smart_Tou_Ctrl_Status.all = status.all;
    }

    /* 清理首次标识 */
    first_flag = false;
}

/**
 * @brief 智能TOU控制模块周期处理函数
 *
 * 该函数应被周期性调用（如定时器/任务中），用于检测TOU配置点在(pre_time, now_time]区间内是否有触发，
 * 并将触发结果通过队列发送给后续处理任务。
 * 该任务需在系统时间有效后执行，建议周期500ms~1s
 */
void Smart_Tou_Ctrl_Module_Process(void)
{
    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (Smart_Tou_TakeMutex(100) != pdTRUE) {
        ESP_LOGW(TAG, "Smart_Tou_Ctrl_Module_Process: mutex busy, skip");
        return;
    }

    time_t now_time = time(NULL);

    /* 时间点控制 */
    Smart_Tou_Handle_TimePoint_Control(now_time);

    /*判断是否需要支持时间段控制*/
    if ( true == Smart_Tou_Ctrl_Time_Quantum_Enable ) {
        /* 时间段控制 */
        Smart_Tou_Handle_TimeQuantum_Control(now_time);
        
        /* 保持控制检查 */
        Smart_Tou_Handle_Keeping_Control();
    }

    /*检查并更新 Smart TOU 控制状态*/
    Smart_Tou_Ctrl_Status_Check();

    // 清空文件更新标志
    Smart_Tou_Ctrl_Config_update_Flag = 0;
    
    /* 若有写文件标志，执行写入操作（成功则清除标志） */
    if (Smart_Tou_Ctrl_Config_Write_Flag) {
        if (Smart_Tou_Write_Config_To_File() == 0) {
            Smart_Tou_Ctrl_Config_Write_Flag = 0;
        }
    }

    /* 释放互斥 */
    Smart_Tou_GiveMutex();
}

/**
 * @brief 获取智能TOU控制模块的下一条控制命令
 * 
 * 本函数用于从TOU控制队列（xQueue_smart_tou_ctrl）中获取一条待处理的控制命令（如寄存器操作等）。
 * 通常由下游任务/线程调用，实现TOU触发后的具体控制动作。 
 * 具体动作由调用任务自行决定，建议周期100ms~200ms。
 * 
 * - 若成功获取到命令，则将内容拷贝到用户提供的结构体 tou_ctrl_result，并释放内部分配的内存。
 * - 若队列为空、接收失败或参数无效，则返回0。
 * 
 * @param tou_ctrl_result [out] 指向结果结构体的指针，函数成功时填充该结构体内容
 * @return int  1-获取到命令，0-无命令或参数无效
 */
int Smart_Tou_Ctrl_Command_Get(smart_tou_ctrl_result_t *tou_ctrl_result)
{
    // 参数有效性检查
    if (!tou_ctrl_result) {
        ESP_LOGE(TAG, "Smart_Tou_Ctrl_Command_Get: tou_ctrl_result is NULL");
        return 0;
    }

    // 检查队列是否已初始化
    if (!xQueue_smart_tou_ctrl) {
        ESP_LOGE(TAG, "Smart_Tou_Ctrl_Command_Get: xQueue_smart_tou_ctrl not initialized");
        return 0;
    }

    // 从队列中接收一条命令（非阻塞，立即返回）
    smart_tou_ctrl_result_t *result_ptr = NULL;
    if (xQueueReceive(xQueue_smart_tou_ctrl, &result_ptr, 0) == pdPASS && result_ptr != NULL) {
        // 拷贝内容到用户结构体
        memcpy(tou_ctrl_result, result_ptr, sizeof(smart_tou_ctrl_result_t));
        // 释放队列中分配的内存
        free(result_ptr);
        return 1;
    }

    // 队列为空或接收失败
    return 0;
}

/**
 * @brief 获取智能TOU控制模块有效节点数量
 * @return 有效节点数量
 */
uint8_t get_smart_tou_ctrl_unit_valid_cnt(void)
{
    return (Smart_Tou_Ctrl_Time_Point_Valid_Cnt + Smart_Tou_Ctrl_Time_Quantum_Valid_Cnt);
}

/**
 * @brief 恢复 Smart TOU 配置为出厂默认并写入文件（直接修改全局 Smart_Tou_Ctrl_Config，节省栈/堆）
 * @return 0 成功，-1 失败
 *
 * 说明：
 * - 直接在全局 Smart_Tou_Ctrl_Config 上置零并设置默认项，避免申请临时结构体占用内存。
 * - 在修改/写文件期间持有互斥，确保线程安全；无论成功或失败都会释放互斥。
 */
int Smart_Tou_Restore_Factory_Defaults(void)
{
    char path[30] = {0};
    SMART_TOU_FILE_PATH(path);

    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (Smart_Tou_TakeMutex(500) != pdTRUE) {
        ESP_LOGW(TAG, "Smart_Tou_Restore_Factory_Defaults: mutex busy, skip");
        return -1;
    }

    /* 清零并设置默认值（直接写入全局配置） */
    memset(&Smart_Tou_Ctrl_Config, 0, sizeof(Smart_Tou_Ctrl_Config));
    Smart_Tou_Ctrl_Config.smart_tou_info.diff_type = 1;
    Smart_Tou_Ctrl_Config.smart_tou_info.time_num  = SMART_TOU_UNIT_MAX_CNT;

    /* 如需对各单元初始化可在此循环中设置（若结构较大，按需设置具体字段以避免重复写入） */
    // for (int i = 0; i < SMART_TOU_UNIT_MAX_CNT; ++i) {
    //     Smart_Tou_Ctrl_Config.units[i].xxx = ...;
    // }

    /* 写入文件 */
    ssize_t written = fs_file_write_at(path, 0, (uint8_t *)&Smart_Tou_Ctrl_Config, sizeof(Smart_Tou_Ctrl_Config));
    if (written != sizeof(Smart_Tou_Ctrl_Config)) {
        ESP_LOGE(TAG, "Failed to write default TOU config to file (%d/%zu)", (int)written, sizeof(Smart_Tou_Ctrl_Config));
        Smart_Tou_GiveMutex();
        return -1;
    }

    /* 设置文件更新标志 */
    Smart_Tou_Ctrl_Config_update_Flag = 1;
    ESP_LOGI(TAG, "Smart TOU config restored to factory defaults and written to %s", path);

    Smart_Tou_GiveMutex();
    return 0;
}

/**
 * @brief 设置智能TOU控制模块时间段使能
 * @return 无
 */
void Smart_Tou_Ctrl_Time_Quantum_Enable_Set(bool flag) {
    Smart_Tou_Ctrl_Time_Quantum_Enable = flag;
}

/**
 * @brief 获取当前智能TOU控制支路状态指针
 *
 * 返回内部全局状态变量 Smart_Tou_Ctrl_Status 的地址，供外部查询当前支路状态。
 * 使用 const 限制调用方直接修改内部状态。
 *
 * @return const smart_ctrl_status_struct* 当前智能TOU控制支路状态指针
 */
const smart_ctrl_status_struct *Smart_Tou_Ctrl_Status_Get(void)
{
    return &Smart_Tou_Ctrl_Status;
}

/*------------------------------------------------------------------------------
 Function: Smart_Tou_Ctrl_Data_Update
 -----------------------------------------------------------------------------*/
/**
* @brief      IF-THEN智能控制数据更新
* @param[in]  void  
* @param[out] None
* @return     static void
*/
void Smart_Tou_Ctrl_Data_Update(void)
{
    // 获取智能TOU控制模块的下一条控制命令
    smart_tou_ctrl_result_t tou_ctrl_result = {0};
    if(Smart_Tou_Ctrl_Command_Get(&tou_ctrl_result)) {
        ESP_LOGI(TAG, "TOU命令获取成功，reg_type=%d, device_id=%d, reg_addr=%d, reg_value=%d",
                tou_ctrl_result.reg_type,
                tou_ctrl_result.device_id,
                tou_ctrl_result.reg_addr,
                tou_ctrl_result.reg_value);
        
        // 0-modbus beta
        if (TOU_REG_TYPE_MODBUS_BETA != tou_ctrl_result.reg_type) {
            return;
        }
		// addr: 2011：AC总开关 | 2012：DC总开关 | 2093：AC支路开关 | 2094：DC支路开关
		switch (tou_ctrl_result.reg_addr) {
		case 2011:
			CanSetTarget(0, 0, INV_TYPE_CONFIG00_1AH, offsetof(inv_set00_struct, ctrl_ac), (uint8_t*)&tou_ctrl_result.reg_value, 1, 0xFF); // 广播
			break;
		case 2012:
			CanSetTarget(0, 0, INV_TYPE_CONFIG00_1AH, offsetof(inv_set00_struct, ctrl_dc), (uint8_t*)&tou_ctrl_result.reg_value, 1, 0xFF); // 广播
			break;
		case 2093:
			CanSetTarget(0, 0, INV_TYPE_CONFIG00_1AH, offsetof(inv_set00_struct, set_AC_branch), (uint8_t*)&tou_ctrl_result.reg_value, 2, 0xFF); // 广播
			break;
		case 2094:
			CanSetTarget(0, 0, INV_TYPE_CONFIG00_1AH, offsetof(inv_set00_struct, set_DC_branch), (uint8_t*)&tou_ctrl_result.reg_value, 2, 0xFF); // 广播
			break;
		default:
			return;
		}
    #if 0   // 调试
        uint16_t debug_old_data = 0;
        // 声明并填充参数结构体
        modbus_change_params_t log_params = {
            .slave_address       = tou_ctrl_result.device_id,
            .start_address       = tou_ctrl_result.reg_addr,
            .reg_count           = 1,
            .old_data            = (uint16_t *)&debug_old_data,
            .new_data            = (uint16_t *)&tou_ctrl_result.reg_value,
            .protocol_version    = PROTOCOL_MODBUS_BETA,
            .modification_source = DATA_SOURCE_IOT_INTERNAL,
        };
        
        // 3. 调用新函数，只传递一个参数
        if (Reg_Change_Log_Generate_By_Modbus(&log_params) != 0) {
            // 处理错误
        }
    #endif
    };
}
