/**
 * @file iot_timer.h
 * @details 提供软件定时器功能，支持基于滴答和毫秒的定时任务创建、删除和管理。
 *          通过单一硬件定时器实现多任务定时功能，降低对系统资源的占用。
 * @author tab
 * @version V1.0.0
 * @date 2025/04/15
 * @copyright Copyright (C) PowerOak Tech. Co., Ltd.
 */

#ifndef __IOT_TIMER_H__
#define __IOT_TIMER_H__

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t iot_timer_id_t;

/**
 * @brief 最大定时器任务数量
 * @details 可以根据需要调整定时器任务的数量，增加此值将消耗更多RAM
 */
#define MAX_TIMER_TASKS 20

/**
 * @brief 定时器的基本滴答时间，单位为毫秒
 * @details 系统定时器的最小计时单位，所有定时任务的分辨率基于此值，可以根据实际需求调整更大间隔
 */
#define IOT_TIMER_TICK_MS 10

/**
 * @brief 将毫秒转换为滴答数，使用向上取整
 * @param ms 毫秒数
 * @return 对应的滴答数
 */
#define MS_TO_TICKS(ms) ((ms + IOT_TIMER_TICK_MS - 1) / IOT_TIMER_TICK_MS)

/**
 * @brief 将滴答数转换为毫秒
 * @param ticks 滴答数
 * @return 对应的毫秒数
 */
#define TICKS_TO_MS(ticks) (ticks * IOT_TIMER_TICK_MS)

/**
 * @brief 定时器状态枚举
 */
typedef enum {
    TIMER_STATE_INACTIVE = 0, ///< 未激活(未创建或已删除)
    TIMER_STATE_IDLE, ///< 已创建但未运行(已暂停)
    TIMER_STATE_ACTIVE, ///< 正在运行
    TIMER_STATE_COMPLETED ///< 一次性定时器已完成(等待重启)
} IOT_TIMER_STATE_t;

/**
 * @brief 定时器任务结构体
 * @details 定义单个定时任务的属性和状态
 */
typedef struct {
    int32_t period_ticks; ///< 定时周期(滴答数)
    int32_t remaining_ticks; ///< 剩余时间(滴答数)
    void (*callback)(void *); ///< 定时器回调函数，定时到期时调用
    void *param; ///< 回调函数参数，传递给回调函数
    IOT_TIMER_STATE_t state; ///< 定时器状态
    bool repeat; ///< 是否重复执行，true表示周期性，false表示一次性
} IOT_TIMER_TASK_t;

/**
 * @brief 定时器队列结构体
 * @details 管理所有定时任务的容器
 */
typedef struct {
    IOT_TIMER_TASK_t tasks[MAX_TIMER_TASKS]; ///< 定时任务数组
    uint8_t task_count; ///< 当前活动的定时任务数量
} IOT_TIMER_QUEUE_t;

/**
 * @brief 初始化定时器队列
 * @details 创建底层硬件定时器并初始化定时器队列，必须在使用其他定时器函数前调用
 * @note 此函数使用IOT_TIMER_TICK_MS作为基本滴答周期
 */
void iot_timer_queue_init(void);

/**
 * @brief 添加基于滴答的定时任务
 * @param period_ticks 定时周期(滴答数)
 * @param repeat 是否重复执行，true表示周期性，false表示一次性
 * @param callback 回调函数，定时到期时调用
 * @param param 传递给回调函数的参数
 * @return 任务ID(0~MAX_TIMER_TASKS-1)，失败返回0xFF
 */
iot_timer_id_t iot_timer_add_task(int32_t period_ticks,
                                  bool repeat,
                                  void (*callback)(void *),
                                  void *param);

/**
 * @brief 删除定时任务
 * @param task_id 任务ID，由iot_timer_add_task返回
 * @return 成功返回true，失败返回false
 */
bool iot_timer_remove_task(iot_timer_id_t task_id);

/**
 * @brief 添加基于滴答的定时任务（与 @see iot_timer_add_task 相同）
 * @details 为了API语义清晰而提供的别名
 */
#define iot_timer_add_task_ticks iot_timer_add_task

/**
 * @brief 添加基于毫秒的定时任务
 * @param period_ms 定时周期(毫秒)
 * @param repeat 是否重复执行，true表示周期性，false表示一次性
 * @param callback 回调函数，定时到期时调用
 * @param param 传递给回调函数的参数
 * @return 任务ID(0~MAX_TIMER_TASKS-1)，失败返回0xFF
 * @note 内部会将毫秒转换为滴答数，可能存在舍入误差
 */
#define iot_timer_add_task_ms(period_ms, repeat, callback, param) \
    iot_timer_add_task(MS_TO_TICKS(period_ms), repeat, callback, param)

// 开始定时器任务
void iot_timer_start_task(iot_timer_id_t task_id);

// 重启定时器任务
void iot_timer_restart_task(iot_timer_id_t task_id);

// 停止定时器任务
void iot_timer_stop_task(iot_timer_id_t task_id);

// 更改定时器任务参数
bool iot_timer_set_param(iot_timer_id_t task_id, void *new_param);

// 获取定时器任务参数
void *iot_timer_get_param(iot_timer_id_t task_id);

// 更改定时器时间间隔
bool iot_timer_set_period(iot_timer_id_t task_id, int32_t new_period_ticks);

// 重新开始底层定时器
void iot_timer_queue_start();

// 停止底层定时器，方便进入低功耗模式LP0
void iot_timer_queue_stop();

#endif // __IOT_TIMER_H__
