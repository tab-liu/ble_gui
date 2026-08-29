/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: 软件定时器初始化
Version: V1.00
Function List: none
***********************************************************/

#include "iot_software_timer.h"
#include "iot_uart.h"

static struct builtin_timer *iot_software_timer_inst = NULL;
sIntervalTimer gSoftTimer[TIMER_ARRAY_SIZE];
static uint32_t elapsed = 0;

static void iot_software_timer_callback(void *param);
static void create_iot_software_timer(void);
// static void stop_iot_software_timer(void);
static void set_iot_software_timer(uint16_t time);

void catch_interval(int sig) {
    elapsed += sig;
}

void InitTimer(void) {
    elapsed = 0;
}

void TimerUpdate(sIntervalTimer *itimer) {
    uint8_t i = 0;
    uint32_t delta = elapsed;

    if (!delta) {
        return;
    }

    elapsed = 0;
    for (i = 0; i < TIMER_ARRAY_SIZE; i++) {
        if (itimer[i].left >= delta)
            itimer[i].left -= delta;
        else
            itimer[i].left = 0;

        if (!itimer[i].left && itimer[i].interval) {
            itimer[i].expired = 1;
            itimer[i].left = itimer[i].interval;
        }
    }
}

void TimerStart(uint8_t index, uint32_t interval, sIntervalTimer *itimer) {
    if (index >= TIMER_ARRAY_SIZE) {
        return;
    }

    itimer[index].expired = 0;
    itimer[index].interval = interval;
    itimer[index].left = interval;
}

void TimerStop(uint8_t index, sIntervalTimer *itimer) {
    if (index >= TIMER_ARRAY_SIZE) {
        return;
    }

    itimer[index].interval = 0;
}

uint8_t TimerExpired(uint16_t index, sIntervalTimer *itimer) {
    TimerUpdate(itimer);

    if (index >= TIMER_ARRAY_SIZE) {
        return 0;
    }

    if (!itimer[index].expired) {
        return 0;
    }

    itimer[index].expired = 0;

    return 1;
}

uint8_t TimerStopped(uint16_t index, sIntervalTimer *itimer) {
    TimerUpdate(itimer);

    if (index >= TIMER_ARRAY_SIZE) {
        return 0;
    }

    if (!itimer[index].interval) {
        return 1;
    }

    return 0;
}

uint8_t TimerRunning(uint16_t index, sIntervalTimer *itimer) {
    TimerUpdate(itimer);

    if (index >= TIMER_ARRAY_SIZE) {
        return 0;
    }

    if (!itimer[index].interval && !itimer[index].expired) {
        return 0;
    }

    return 1;
}

extern uint8_t OTA_SUCC;
extern uint8_t FLAG_RECV;
uint16_t rst_timer = 0;
// 超时回调函数
void iot_software_timer_callback(void *param) {
    //    static uint8_t timer = 0;
    catch_interval(10); /*更新时间*/
    set_iot_software_timer(10);
    if (FLAG_RECV == 1) {
        FLAG_RECV = 0;
    } else {
        uart_timer_cb(NULL);
    }

    if (OTA_SUCC == 1) {
        if (++rst_timer >= 20) {
            OTA_SUCC = 0;
            rst_timer = 0;
            platform_reset(RESET_OTA_SUCCEED); /*重启*/
        }
    }
}

/*创建定时器*/
static void create_iot_software_timer(void) {
    iot_software_timer_inst = builtin_timer_create(iot_software_timer_callback); /*创建定时器*/
}

/*停止定时器*/
void stop_iot_software_timer(void) {
    builtin_timer_stop(iot_software_timer_inst); /*停止定时器*/
}

/*修改定时器的超时*/
static void set_iot_software_timer(uint16_t time) {
    builtin_timer_start(iot_software_timer_inst, time, NULL); /*开始定时器*/
}

void iot_software_timer_init(void) {
    // LOG_I("iot_software_timer_init");
    create_iot_software_timer();
    set_iot_software_timer(10); /*设定超时时间为10ms*/
}
