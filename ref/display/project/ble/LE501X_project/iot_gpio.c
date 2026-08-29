/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: gpio
Version: V1.00
Function List: none
***********************************************************/

#include "iot_gpio.h"
#include "field_manipulate.h"
#include "sleep.h"
#include "iot_software_timer.h"
#include "iot_ble.h"
#include "iot_uart.h"
#include "iot_service.h"
#include "iot_timer.h"
#include "iot_iic.h"
#include "systick.h"
#include "iot_magnetic_screen.h"
#include "Lcd_Hw_cn91c4.h"

static iot_timer_id_t key_detect_timer_id = 0xFF;
static iot_timer_id_t key_hold_poll_timer_id = 0xFF;
static uint8_t key_pin_value = BLE_SLEEP_PIN;
uint8_t *key_pin = &key_pin_value;

static uint32_t key_pushed_systime = 0;
static uint32_t key_hold_ms = 0;
static bool key_is_pressed = false;
static key_press_hint_t key_current_hint = KEY_PRESS_HINT_NONE;

void lcd_backlight_io_init(void) {
    io_cfg_output(LCD_BACKRIGHT_IO); // PB10 config output
    io_pull_write(LCD_BACKRIGHT_IO, IO_PULL_DOWN); // PB10 pull none
    io_write_pin(LCD_BACKRIGHT_IO, 1); // PB10 write low level
}

void lcd_backlight_io_on(void) {
    io_write_pin(LCD_BACKRIGHT_IO, 1);
}

void lcd_backlight_io_off(void) {
    io_write_pin(LCD_BACKRIGHT_IO, 0);
}

void ble_sleep_input_init(void) {
    io_cfg_input(BLE_SLEEP_PIN);
    io_pull_write(BLE_SLEEP_PIN, IO_PULL_UP);
    io_exti_config(BLE_SLEEP_PIN, INT_EDGE_BOTH);
}

// 充电检测引脚初始化 充电中， 充电完成
void charging_detect_io_init(void) {
    io_cfg_input(CHARGING_DETECT_PIN_CHRG);
    io_pull_write(CHARGING_DETECT_PIN_CHRG, IO_PULL_UP);

    io_cfg_input(CHARGING_DETECT_PIN_STDBY);
    io_pull_write(CHARGING_DETECT_PIN_STDBY, IO_PULL_UP);
}

bool is_charging() {
    return io_read_pin(CHARGING_DETECT_PIN_CHRG) == 0;
}

bool is_charging_done() {
    return io_read_pin(CHARGING_DETECT_PIN_STDBY) == 0;
}

/* 按住期间轮询：只更新提示图标，并累计按住时间；不执行关机/配对 */
static void key_hold_poll_cb(void *param) {
    (void)param;
    if (!key_is_pressed || io_read_pin(BLE_SLEEP_PIN)) {
        return;
    }

    key_hold_ms += KEY_HOLD_POLL_MS;

    if (key_hold_ms >= KEY_PRESS_TIER2_MS) {
        if (key_current_hint != KEY_PRESS_HINT_PAIRING) {
            key_current_hint = KEY_PRESS_HINT_PAIRING;
            magnetic_screen_key_press_hint_show(KEY_PRESS_HINT_PAIRING);
        }
    } else if (key_hold_ms >= KEY_PRESS_TIER1_MS) {
        // 蓝牙匹配中不提示快配；已在快配中也不重复刷提示
        if (!is_ble_pairing_mode() && !is_fast_match_mode()
            && key_current_hint != KEY_PRESS_HINT_FAST_MATCH) {
            key_current_hint = KEY_PRESS_HINT_FAST_MATCH;
            magnetic_screen_key_press_hint_show(KEY_PRESS_HINT_FAST_MATCH);
        }
    }
}

static void key_hold_poll_start(void) {
    iot_timer_start_task(key_hold_poll_timer_id);
}

static void key_hold_poll_stop(void) {
    iot_timer_stop_task(key_hold_poll_timer_id);
}

static void key_on_press(void) {
    if (key_is_pressed) {
        return; // 已按下则不再重置计时，避免抖动把长按打成短按
    }
    LOG_I("vvvvvv (key pushed) vvvvvv");
    key_is_pressed = true;
    key_current_hint = KEY_PRESS_HINT_NONE;
    key_hold_ms = 0;
    key_pushed_systime = systick_get_value();
    key_hold_poll_start();
}

static uint32_t key_press_duration_ms(void) {
    uint32_t systick_ms = systick_get_value() - key_pushed_systime;
    return (systick_ms > key_hold_ms) ? systick_ms : key_hold_ms;
}

static void key_on_release(void) {
    if (!key_is_pressed) {
        return;
    }
    key_is_pressed = false;
    key_hold_poll_stop();

    uint32_t press_duration = key_press_duration_ms();
    LOG_I("^^^^^^ (key release, press duration: %dms, hold_ms: %d) ^^^^^^",
          press_duration,
          key_hold_ms);

    if (press_duration < KEY_PRESS_IGNORE_MS) {
        magnetic_screen_key_press_hint_clear();
        return;
    }
    if (press_duration < KEY_PRESS_TIER1_MS) {
        magnetic_screen_key_press_hint_clear();
        deep_sleep_lp3();
    } else if (press_duration < KEY_PRESS_TIER2_MS) {
        // 蓝牙匹配中不允许切入快配
        if (is_ble_pairing_mode()) {
            magnetic_screen_key_press_hint_clear();
            return;
        }
        if (is_fcty_calibrated()) {
            magnetic_screen_key_press_hint_dismiss(); // 沿用长按闪烁
            key_2_start_fast_match();
        } else {
            magnetic_screen_key_press_hint_clear();
            deep_sleep_lp3();
        }
    } else if (press_duration < KEY_PRESS_TIER2_MAX_MS) {
        magnetic_screen_key_press_hint_clear();
        if (is_fcty_calibrated()) {
            key_2_start_adv(); // 进入蓝牙匹配，内部会退出快配
            make_ecc_key_startup_init();
        } else {
            deep_sleep_lp3();
        }
    } else {
        magnetic_screen_key_press_hint_clear();
    }
}

/* 按键消抖定时器回调函数 */
static void key_detect_timer_cb(void *param) {
    (void)param;
    if (io_read_pin(BLE_SLEEP_PIN)) { // 1: 抬起
        key_on_release();
    } else { // 0: 按下
        key_on_press();
    }
}

/* 按键定时器初始化 */
void key_detect_timer_init(void) {
    key_pin = &key_pin_value;
    key_detect_timer_id = iot_timer_add_task_ms(KEY_DETECT_TIME, false, key_detect_timer_cb, NULL);
    key_hold_poll_timer_id
        = iot_timer_add_task_ms(KEY_HOLD_POLL_MS, true, key_hold_poll_cb, NULL);
}

/*GPIO中断回调函数*/
void io_exti_callback(uint8_t pin, exti_edge_t edge) {
    (void)edge;
    switch (pin) {
    case BLE_SLEEP_PIN:
        *key_pin = pin;
        iot_timer_set_param(key_detect_timer_id, key_pin);
        iot_timer_start_task(key_detect_timer_id);
        break;
    default:
        break;
    }
}

// 进入LP3前，把所有输出的IO改为输入并设置为对应的上下拉，以防止漏电
static void process_gpio_before_sleep(void) {
    io_cfg_input(LCD_BACKRIGHT_IO); // PB10 config output
    io_pull_write(LCD_BACKRIGHT_IO, IO_PULL_DOWN); // PB10 pull none
}

/*进入低功耗前处理(关闭软件定时器、反初始化外设等)*/
static void deal_with_before_sleep(void) {
    stop_iot_software_timer();
    iot_uart_deinit();
    iot_i2c_deinit();
    stop_iot_service_timer();
}

/*唤醒引脚配置*/
static void exitpa00_iowkup_init(void) {
    io_cfg_input(BLE_SLEEP_PIN);
    io_pull_write(BLE_SLEEP_PIN, IO_PULL_UP);
    // 注意这里设置为上升沿唤醒，避免按键时间过长系统启动后，检测到上升沿处理按键
    io_exti_config(BLE_SLEEP_PIN, INT_EDGE_RISING);
}

/*配置参数,外部中断下降沿唤醒,进入LP3睡眠模式*/
static void ls_sleep_enter_lp3(void) {
    struct deep_sleep_wakeup wakeup;
    memset(&wakeup, 0, sizeof(wakeup));
    wakeup.pa00 = 1;
    wakeup.pa00_rising_edge = 1; // 1表示上升沿唤醒，0表示下降沿唤醒
    enter_deep_sleep_mode_lvl2_lvl3(&wakeup); // 调用唤醒函数
}

/* 关机延时定时器回调函数 */
static void shutdown_delay_timer_cb(void *param) {
    LOG_I("shutdown delay timer expired, entering deep sleep now");
    DELAY_US(10 * 1000);
    deal_with_before_sleep();
    exitpa00_iowkup_init();
    ls_sleep_enter_lp3();
}

void deep_sleep_lp3() {
    LOG_I("deep sleep lp3 - setting up 100ms delay timer");
    process_gpio_before_sleep(); // 关背光
    magnetic_screen_set_sleep_mode(); // 清空显示
    CDD_Lcd_Hw_SetEN(0); // 关闭LCD使能
    // 创建500ms延时定时器，到期后真正进入关机，保证关机前动作完成
    iot_timer_id_t shutdown_timer_id
        = iot_timer_add_task_ms(500, false, shutdown_delay_timer_cb, NULL);
    iot_timer_start_task(shutdown_timer_id);
}
