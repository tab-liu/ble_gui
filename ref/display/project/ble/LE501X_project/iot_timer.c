/**
 * @file iot_timer.c
 * @brief 基于BLE协议栈的定时器任务管理实现
 *
 * @details
 * 主要用于定时器的任务创建、删除、启动、停止等操作。
 * 避免了任务过多时直接使用BLE内置定时器对BLE协议栈大循环的影响。
 * 用该模块将协议栈任务与上层业务逻辑分离。
 * 同时避免使用全局flag变量来控制定时器的状态，以解耦上层任务之间的联系。
 * 参考FreeRTOS的API设计实现，这里只需要使用一个内置定时器。
 * 如需要移植，只改变底层使用的定时器即可。
 *
 * @author tab
 * @date 2025/04/15
 * @version V1.0.0
 * @copyright Copyright (C) PowerOak Tech. Co., Ltd.
 */

#include <string.h>
#include <stdio.h>
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "builtin_timer.h"
#include "iot_timer.h"

/** @brief 全局定时器队列实例 */
static IOT_TIMER_QUEUE_t iot_timer_queue = {0};

/** @brief 系统基础定时器实例 */
static struct builtin_timer *iot_timer_inst = NULL;

/**
 * @note 此函数由底层定时器周期性调用，不应由用户直接调用
 */
static void iot_timer_queue_process(void *param) {
    // 遍历所有任务
    for (uint8_t i = 0; i < MAX_TIMER_TASKS; i++) {
        // 跳过未激活状态的定时器
        if (iot_timer_queue.tasks[i].state != TIMER_STATE_ACTIVE) {
            continue;
        }
        // 更新剩余时间
        if (iot_timer_queue.tasks[i].remaining_ticks > 0) {
            iot_timer_queue.tasks[i].remaining_ticks--;
        } else {
            if (iot_timer_queue.tasks[i].repeat) { // 处理重复定时
                iot_timer_queue.tasks[i].remaining_ticks = iot_timer_queue.tasks[i].period_ticks;
            } else { // 单次定时
                iot_timer_queue.tasks[i].state = TIMER_STATE_COMPLETED; // 设置为完成状态
            }
            if (iot_timer_queue.tasks[i].callback) {
                iot_timer_queue.tasks[i].callback(iot_timer_queue.tasks[i].param);
            }
        }
    }
    // 重新启动基础定时器
    if (iot_timer_inst) {
        builtin_timer_start(iot_timer_inst, IOT_TIMER_TICK_MS, NULL);
    }
}

// 初始化定时器队列，创建和启动基础定时器
void iot_timer_queue_init(void) {
    // 创建一个基础定时器，周期为 `IOT_TIMER_TICK_MS` 毫秒
    iot_timer_inst = builtin_timer_create(iot_timer_queue_process);
    if (!iot_timer_inst) {
        LOG_E("Failed to create software timer instance");
        return;
    }
    LOG_I("Created software timer instance successfully");
    builtin_timer_start(iot_timer_inst, IOT_TIMER_TICK_MS, NULL);
}

void iot_timer_queue_start() {
    if (iot_timer_inst) {
        builtin_timer_start(iot_timer_inst, IOT_TIMER_TICK_MS, NULL); // 启动基础定时器
    }
}

void iot_timer_queue_stop() {
    if (iot_timer_inst) {
        builtin_timer_stop(iot_timer_inst); // 停止基础定时器
    }
}

// 添加定时任务 - 搜索队列中的空闲槽位
iot_timer_id_t iot_timer_add_task(int32_t period_ticks,
                                  bool repeat,
                                  void (*callback)(void *),
                                  void *param) {
    if (iot_timer_queue.task_count >= MAX_TIMER_TASKS) {
        return 0xFF; // 队列已满
    }

    for (uint8_t i = 0; i < MAX_TIMER_TASKS; i++) {
        if (iot_timer_queue.tasks[i].state != TIMER_STATE_INACTIVE) {
            continue;
        }
        // 找到一个空闲槽位，初始化任务
        iot_timer_queue.tasks[i].state = TIMER_STATE_IDLE;
        iot_timer_queue.tasks[i].period_ticks = period_ticks;
        iot_timer_queue.tasks[i].remaining_ticks = period_ticks;
        iot_timer_queue.tasks[i].repeat = repeat;
        iot_timer_queue.tasks[i].callback = callback;
        iot_timer_queue.tasks[i].param = param;

        iot_timer_queue.task_count++;
        return i; // 返回任务ID
    }
    return 0xFF; // 未找到空闲位置
}

// 删除指定ID的定时任务
bool iot_timer_remove_task(iot_timer_id_t task_id) {
    if (task_id >= MAX_TIMER_TASKS
        || iot_timer_queue.tasks[task_id].state == TIMER_STATE_INACTIVE) {
        return false;
    }

    iot_timer_queue.tasks[task_id].state = TIMER_STATE_INACTIVE; // 设置为未激活状态
    iot_timer_queue.tasks[task_id].callback = NULL; // 清空回调函数

    iot_timer_queue.task_count--;
    return true;
}

// 开始定时器任务
void iot_timer_start_task(iot_timer_id_t task_id) {
    if (task_id >= MAX_TIMER_TASKS) {
        return;
    }
    iot_timer_queue.tasks[task_id].state = TIMER_STATE_ACTIVE; // 设置为激活状态
    iot_timer_queue.tasks[task_id].remaining_ticks
        = iot_timer_queue.tasks[task_id].period_ticks; // 重置剩余时间
    return;
}

// 重启定时器任务
void iot_timer_restart_task(iot_timer_id_t task_id) {
    if (task_id >= MAX_TIMER_TASKS) {
        return;
    }
    iot_timer_queue.tasks[task_id].state = TIMER_STATE_ACTIVE; // 设置为激活状态
    iot_timer_queue.tasks[task_id].remaining_ticks
        = iot_timer_queue.tasks[task_id].period_ticks; // 重置剩余时间
    return;
}

// 停止定时器任务
void iot_timer_stop_task(iot_timer_id_t task_id) {
    if (task_id >= MAX_TIMER_TASKS) {
        return;
    }
    iot_timer_queue.tasks[task_id].state = TIMER_STATE_IDLE; // 设置为空闲状态
    return;
}

// 更改定时器任务参数
bool iot_timer_set_param(iot_timer_id_t task_id, void *new_param) {
    if (task_id >= MAX_TIMER_TASKS) {
        return false;
    }
    iot_timer_queue.tasks[task_id].param = new_param; // 更新参数
    return true;
}

// 获取定时器任务参数
void *iot_timer_get_param(iot_timer_id_t task_id) {
    if (task_id >= MAX_TIMER_TASKS) {
        return NULL;
    }
    return iot_timer_queue.tasks[task_id].param; // 返回参数
}

// 设置定时器任务周期
bool iot_timer_set_period(iot_timer_id_t task_id, int32_t new_period_ticks) {
    if (task_id >= MAX_TIMER_TASKS) {
        return false;
    }
    iot_timer_queue.tasks[task_id].period_ticks = new_period_ticks; // 更新周期
    iot_timer_queue.tasks[task_id].remaining_ticks = new_period_ticks; // 重置剩余时间
    return true;
}
