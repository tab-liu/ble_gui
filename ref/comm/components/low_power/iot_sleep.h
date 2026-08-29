/**
  ******************************************************************************
  * @file      iot_sleep.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/7/3
  * @brief     低功耗休眠头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/7/3   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#ifndef IOT_SLEEP_H
#define IOT_SLEEP_H

#include "stdint.h"
#include <stdio.h>
#include "driver/gpio.h"

#include "comm_define.h"

/*------------------------------------------------------------------------------*/

#ifndef CONFIG_WAVE_DI_GPIO_PIN
#define CONFIG_WAVE_DI_GPIO_PIN        GPIO_NUM_38
#endif

#ifndef CONFIG_WAVE_DO_GPIO_PIN
#define CONFIG_WAVE_DO_GPIO_PIN        GPIO_NUM_8
#endif

/*DI唤醒*/
#define WAVE_DI_GPIO            CONFIG_WAVE_DI_GPIO_PIN

/*DO唤醒*/
#define WAVE_DO_GPIO            CONFIG_WAVE_DO_GPIO_PIN

/*GPIO通用初始化*/
#define CONFIGURE_GPIO(gpio_pin, gpio_mode) \
    do { \
        gpio_config_t io_conf = { \
            .pin_bit_mask = (1ULL << (gpio_pin)), \
            .mode = (gpio_mode), \
            .pull_up_en = GPIO_PULLUP_DISABLE, \
            .pull_down_en = GPIO_PULLDOWN_DISABLE, \
            .intr_type = GPIO_INTR_DISABLE \
        }; \
        gpio_config(&io_conf); \
    } while (0)

/*“先解除 hold，切换电平，再立即 hold”*/
#define GPIO_SET_LEVEL_HOLD(gpio_num, level) \
    do { \
        gpio_hold_dis(gpio_num); \
        gpio_set_level(gpio_num, level); \
        gpio_hold_en(gpio_num); \
    } while(0)

/*------------------------------------------------------------------------------*/

/*系统DO唤醒使能*/
#define SYSTEM_WAKEUP_BY_GPIO_ENABLE

/*DI唤醒IOT使能*/
#define IOT_WAKEUP_BY_GPIO_ENABLE

/*休眠模式时辅源GPIO控制使能（高电平有效）*/
//#define SLEEP_MODE_POWER_ENABLE

#ifdef CONFIG_IOT_AUTO_LIGHT_SLEEP_ENABLE

/*休眠期间禁用蓝牙协议栈*/
//#define SLEEP_NIMBLE_DEINIT_ENABLE

/*IOT非必要任务挂起使能*/
#define IOT_OTHER_TASK_SUSPEND_ENABLE

/*最大调制解调器省电模式使能（仅休眠期间，listen_interval配置见iot_wifi_init.c）*/
#define SLEEP_WIFI_MAX_MODEM_ENABLE

/*调试打印*/
//#define LIGHT_SLEEP_DEBUG_LOG

#endif

/*------------------------------------------------------------------------------*/

#define CONFIG_PM_MIN_CPU_FREQ_MHZ      40
#define CONFIG_PM_MAX_CPU_FREQ_MHZ      CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
#define CONFIG_DEFAULT_CPU_FREQ_MHZ     CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ

/*------------------------------------------------------------------------------*/

void auto_sleep_config(void);
void auto_sleep_deconfig(void);
uint8_t iot_sleep_status_get(void);

void example_sleep_task(void);
void example_gpio_output(uint8_t link);

void iot_sleep_config_init(void);
void system_do_wakeup_check(void);
void system_be_wakened_check(void);
void Sleep_Task_Create(void);

#endif
