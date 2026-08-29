/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: gpio控制
Version: V1.00
Function List: none
***********************************************************/
#ifndef IOT_GPIO_H
#define IOT_GPIO_H

#include "ls_soc_gpio.h"
#include "platform.h"
#include <string.h>
#include "log.h"
#include "builtin_timer.h"

#define KEY_DETECT_TIME 20 /* 按键消抖检测xx ms */
#define KEY_HOLD_POLL_MS 100 /* 按住期间轮询间隔 ms */
#define KEY_PRESS_IGNORE_MS 20 /* 忽略误触 ms */
#define KEY_PRESS_TIER1_MS 3000 /* 3s：闪图标1，提示可松手执行关机 */
#define KEY_PRESS_TIER2_MS 6000 /* 6s：闪图标2，提示可松手进入配对 */
#define KEY_PRESS_TIER2_MAX_MS 16000 /* 配对窗口上限 ms */

#define LCD_BACKRIGHT_IO PB10
#define BLE_SLEEP_PIN PA00

// 充电检测引脚 充电中， 充电完成
#define CHARGING_DETECT_PIN_CHRG PA01
#define CHARGING_DETECT_PIN_STDBY PB09

void lcd_backlight_io_init(void);
void lcd_backlight_io_on(void);
void lcd_backlight_io_off(void);

void charging_detect_io_init(void);
bool is_charging(void);
bool is_charging_done(void);

extern void ble_sleep_input_init(void);
void key_detect_timer_init(void);

void deep_sleep_lp3();

#endif
