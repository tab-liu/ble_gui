/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_software_timer.h
Author: weiyt
Version: V1.0
Date: 2022.5.25
Description: none
*************************************************/
#ifndef IOT_SOFTWARE_TIMER_H
#define IOT_SOFTWARE_TIMER_H

#include <string.h>
#include <stdio.h>
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "builtin_timer.h"

typedef struct {
    uint8_t expired;
    uint32_t interval;
    uint32_t left;
} sIntervalTimer;

enum {
    SYSTEM_CONFIG_TIMER = 0,
    BLE_TIMEOUT_TIMER,
    XMODEM_TIMER,
    TIMER_ARRAY_SIZE,
};

extern sIntervalTimer gSoftTimer[TIMER_ARRAY_SIZE];

void InitTimer(void);
void catch_interval(int sig);
void TimerStart(uint8_t index, uint32_t interval, sIntervalTimer *itimer);
void TimerStop(uint8_t index, sIntervalTimer *itimer);
uint8_t TimerExpired(uint16_t index, sIntervalTimer *itimer);
uint8_t TimerStopped(uint16_t index, sIntervalTimer *itimer);
uint8_t TimerRunning(uint16_t index, sIntervalTimer *itimer);
extern void iot_software_timer_init(void);
extern void stop_iot_software_timer(void);

#endif
