/**
 * @file task_time_stats.h
 * @brief 任务时间统计工具
 * @version 1.0
 * @date 2025-07-29
 * 
 * @description
 * 本文件提供了一套完整的任务执行时间统计功能，可以方便地集成到其他项目中。
 * 支持统计任务的运行次数、总时间、平均时间、最大时间、最小时间等信息。
 * 
 * @features
 * - 支持编译时开关控制，避免影响发布版本性能
 * - 自动计算平均、最大、最小执行时间
 * - 支持定期打印统计信息
 * - 使用宏定义，使用简单，对代码侵入性小
 * - 基于ESP-IDF框架，可移植到其他ESP32项目，其他平台只需要修改定时器接口即可
 * 
 * @usage
 * 1. 在需要统计的函数开头使用 TASK_TIME_STATS_DECLARE(task_name)
 * 2. 在需要开始计时的地方使用 TASK_TIME_START(task_name)
 * 3. 在需要结束计时的地方使用 TASK_TIME_END(task_name)
 * 4. 可选：使用 TASK_TIME_PRINT(task_name, interval) 定期打印统计信息
 * 
 * @example
 * ```c
 * #include "task_time_stats.h"
 * 
 * void my_task_function(void) {
 *     TASK_TIME_STATS_DECLARE(my_task);  // 声明统计变量
 *     while(1)
 *     {
 *         TASK_TIME_START(my_task);          // 开始计时
 *     
 *          // 你的任务代码
 *          do_some_work();
 *     
 *          TASK_TIME_END(my_task);            // 结束计时
 *     
 *          TASK_TIME_PRINT(my_task, 100);     // 每100次调用打印一次统计信息
 *     }
 * }
 * ```
 * 输出示例：
 * [TASK_TIME] my_task - Runs: 100, Avg: 100 us, Max: 100 us, Min: 100 us, Last: 100 us
 * 
 * @note
 * - 通过修改 CONFIG_TASK_TIME_STATS_ENABLE 宏来控制功能开关
 * - 统计数据使用静态变量存储，每个任务名对应一组独立的统计数据
 * - 时间精度为微秒(us)
 * - 依赖ESP-IDF的esp_timer组件
 */

#ifndef __TASK_TIME_STATS_H__
#define __TASK_TIME_STATS_H__

#include <stdint.h>
#include <string.h>
#include "utils.h"

// ================================================================================================
// 平台适配接口 - 用户可根据需要修改
// ================================================================================================

/**
 * @brief 时间获取函数接口
 * @note 用户可以根据不同平台修改此宏定义
 *       默认使用ESP32平台的esp_timer_get_time()
 *       返回值单位：微秒(us)
 */
#ifndef TASK_TIME_GET_TIME_US
    // 默认使用ESP32平台
    #include "esp_timer.h"
    #define TASK_TIME_GET_TIME_US() esp_timer_get_time()
#endif

/**
 * @brief 日志打印函数接口
 * @note 用户可以根据不同平台修改此宏定义
 *       默认使用ESP32平台的ESP_LOGW
 */
#ifndef TASK_TIME_LOG
    // 默认使用ESP32平台
    #include "esp_log.h"
    #define TASK_TIME_LOG(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ================================================================================================
// 配置宏定义
// ================================================================================================

/**
 * @brief 任务时间统计功能使能宏定义
 * @note 0: 禁用任务时间统计 
         1: 启用任务时间统计
         2: 启用任务时间统计打印
 *       在发布版本中建议设置为0以避免性能影响
 */
#define CONFIG_TASK_TIME_STATS_ENABLE 1

/**
 * @brief 统计信息打印标签
 */
#define TASK_TIME_STATS_TAG "[TASK_TIME]"

/**
 * @brief 通用清除超时
 */
#define TASK_COMMON_CLEAN_TIMEOUT_MS    (2 * 60 * 1000)

// ================================================================================================
// 数据结构定义
// ================================================================================================

/**
 * @brief 任务时间统计数据结构
 */
typedef struct {
    uint32_t total_runs;           /**< 总运行次数 */
    uint32_t total_time_us;        /**< 总运行时间(微秒) */
    uint32_t start_time_us;        /**< 开始运行时间(微秒) */
    uint32_t max_time_us;          /**< 最大运行时间(微秒) */
    uint32_t min_time_us;          /**< 最小运行时间(微秒) */
    uint32_t last_time_us;         /**< 上次运行时间(微秒) */
    uint32_t avg_time_us;          /**< 平均运行时间(微秒) */
    uint32_t print_counter;        /**< 打印计数器 */
} task_time_stats_t;

// ================================================================================================
// 功能宏定义
// ================================================================================================

#if CONFIG_TASK_TIME_STATS_ENABLE

/**
 * @brief 声明并初始化任务时间统计变量
 * @param task_name 任务名称，用作变量名后缀
 * @note 必须在函数开头调用，为指定任务创建独立的统计变量
 */
#define TASK_TIME_STATS_DECLARE(task_name) \
    static USE_EXT_RAM_BSS task_time_stats_t task_stats_##task_name; \
    static USE_EXT_RAM_BSS int task_stats_##task_name##_init = 0; \
    if (task_stats_##task_name##_init == 0) { \
        memset(&task_stats_##task_name, 0, sizeof(task_time_stats_t)); \
        task_stats_##task_name.min_time_us = 0xFFFFFFFF; \
        task_stats_##task_name##_init = 1; \
    }

/**
 * @brief 开始任务时间统计
 * @param task_name 任务名称，必须与TASK_TIME_STATS_DECLARE中的名称一致
 * @note 记录当前时间戳作为计时起点
 */
#define TASK_TIME_START(task_name) \
    int64_t start_time_##task_name = TASK_TIME_GET_TIME_US()

/**
 * @brief 结束任务时间统计并更新统计数据
 * @param task_name 任务名称，必须与TASK_TIME_START中的名称一致
 * @note 计算执行时间并更新所有统计数据
 */
#define TASK_TIME_END(task_name) do { \
    uint32_t elapsed_us = (uint32_t)(TASK_TIME_GET_TIME_US() - start_time_##task_name); \
    if(task_stats_##task_name.start_time_us == 0) task_stats_##task_name.start_time_us = start_time_##task_name; \
    task_stats_##task_name.total_runs++; \
    task_stats_##task_name.total_time_us += elapsed_us; \
    task_stats_##task_name.last_time_us = elapsed_us; \
    if (elapsed_us > task_stats_##task_name.max_time_us) { \
        task_stats_##task_name.max_time_us = elapsed_us; \
    } \
    if (elapsed_us < task_stats_##task_name.min_time_us) { \
        task_stats_##task_name.min_time_us = elapsed_us; \
    } \
    task_stats_##task_name.avg_time_us = task_stats_##task_name.total_time_us / task_stats_##task_name.total_runs; \
} while(0)

/**
 * @brief 重置任务统计数据
 * @param task_name 任务名称
 * @note 清零所有统计数据，重新开始统计
 */
#define TASK_TIME_RESET_STATS(task_name) do { \
    memset(&task_stats_##task_name, 0, sizeof(task_time_stats_t)); \
    task_stats_##task_name.min_time_us = 0xFFFFFFFF; \
} while(0)

/**
 * @brief 定时重置任务统计数据
 * @param task_name 任务名称
 * @note 达到指定时间后清零所有统计数据，重新开始统计
 */
#define TASK_TIME_TIMEOUT_CLEAN(task_name, interval_ms) do { \
    uint32_t timeout_us = (interval_ms > 0) ? (uint32_t)(interval_ms * 1000) : 0xFFFFFFFF; \
    if (timeout_us <= (TASK_TIME_GET_TIME_US() - task_stats_##task_name.start_time_us)) { \
        TASK_TIME_RESET_STATS(task_name); \
    } \
} while(0)

/**
 * @brief 获取指定任务最大运行时长
 * @param task_name 任务名称
 * @param field     需要赋值的变量
 */
#define TASK_TIME_RUNS_MAX_TO_FIELD(task_name, field) do { \
    (field) = task_stats_##task_name.max_time_us; \
} while(0)

#else

// ================================================================================================
// 禁用版本 - 所有宏都定义为空操作
// ================================================================================================

#define TASK_TIME_STATS_DECLARE(task_name) do { } while(0)
#define TASK_TIME_START(task_name) do { } while(0)
#define TASK_TIME_END(task_name) do { } while(0)
#define TASK_TIME_PRINT(task_name, print_interval) do { } while(0)
#define TASK_TIME_RESET_STATS(task_name) do { } while(0)
#define TASK_TIME_TIMEOUT_CLEAN(task_name, interval_ms) do { } while(0)
#define TASK_TIME_RUNS_MAX_TO_FIELD(task_name, field) do { } while(0)

#endif
 

#if CONFIG_TASK_TIME_STATS_ENABLE >= 2

/**
 * @brief 定期打印任务时间统计信息
 * @param task_name 任务名称
 * @param print_interval 打印间隔，每执行多少次打印一次统计信息
 * @note 输出格式: "[TASK_TIME] task_name - Runs: X, Avg: X us, Max: X us, Min: X us, Last: X us"
 */
#define TASK_TIME_PRINT(task_name, print_interval) do { \
    task_stats_##task_name.print_counter++; \
    if (task_stats_##task_name.print_counter >= (print_interval)) { \
        TASK_TIME_LOG(TASK_TIME_STATS_TAG, "%s - RunsCound: %lu, RunsTime: %lu us, Avg: %lu us, Max: %lu us, Min: %lu us, Last: %lu us", \
                      #task_name, \
                      task_stats_##task_name.total_runs, \
                      task_stats_##task_name.total_time_us, \
                      task_stats_##task_name.avg_time_us, \
                      task_stats_##task_name.max_time_us, \
                      task_stats_##task_name.min_time_us, \
                      task_stats_##task_name.last_time_us); \
        task_stats_##task_name.print_counter = 0; \
    } \
} while(0)

#else

// ================================================================================================
// 禁用版本 - 所有宏都定义为空操作
// ================================================================================================

#define TASK_TIME_PRINT(task_name, print_interval) do { } while(0)

#endif

// ================================================================================================
// 使用示例和说明
// ================================================================================================

/*
使用示例1 - 基本用法:

void example_task(void) {
    TASK_TIME_STATS_DECLARE(example);  // 声明统计变量
    
    TASK_TIME_START(example);          // 开始计时
    
    // 执行任务代码
    vTaskDelay(pdMS_TO_TICKS(10));
    
    TASK_TIME_END(example);            // 结束计时
    
    TASK_TIME_PRINT(example, 50);      // 每50次执行打印一次统计
}

使用示例2 - 多个统计点:

void complex_task(void) {
    TASK_TIME_STATS_DECLARE(phase1);
    TASK_TIME_STATS_DECLARE(phase2);
    
    // 统计第一阶段
    TASK_TIME_START(phase1);
    do_phase1_work();
    TASK_TIME_END(phase1);
    
    // 统计第二阶段
    TASK_TIME_START(phase2);
    do_phase2_work();
    TASK_TIME_END(phase2);
    
    TASK_TIME_PRINT(phase1, 100);
    TASK_TIME_PRINT(phase2, 100);
}

使用示例3 - 获取统计数据:

void check_performance(void) {
    TASK_TIME_STATS_DECLARE(my_task);
    
    // ... 执行任务和统计 ...
    
    // 获取统计数据进行自定义处理
    task_time_stats_t *stats = TASK_TIME_GET_STATS(my_task);
    if (stats && stats->avg_time_us > 1000) {
        ESP_LOGW("PERF", "Task %s is running slow: %lu us", "my_task", stats->avg_time_us);
    }
}

移植说明:
1. 确保目标项目包含ESP-IDF框架
2. 包含本头文件: #include "task_time_stats.h"
3. 根据需要修改 CONFIG_TASK_TIME_STATS_ENABLE 宏
4. 在需要统计的函数中使用相应的宏
5. 编译时确保链接了esp_timer组件

性能影响:
- 启用时：每次统计增加约2-5微秒开销
- 禁用时：零开销，宏展开为空操作
- 内存占用：每个统计任务约28字节静态内存
*/

#ifdef __cplusplus
}
#endif

#endif // __TASK_TIME_STATS_H__ 
