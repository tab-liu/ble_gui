/**
  ******************************************************************************
  * @file      usb_host_common.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/31
  * @brief     USB主机通用配置
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/31 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"

/* ======================== 本地模块文件引用（可选） ============================ */


/* ================================ 头文件宏定义 ================================ */

// USB接口供电使能
#define CONFIG_USB_POWER_ENABLE

// USB接口供电使能（高电平使能）
#define USB_POWER_ON_GPIO        GPIO_NUM_2

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)


#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB模块电源复位
 */
void USB_POWER_RESET(void);

/**
 * @brief  初始化USB Host模块，启动USB主机任务并等待库安装完成
 *
 * @return ESP_OK    初始化成功
 * @return ESP_FAIL  初始化失败或USB主机库未安装
 */
esp_err_t usb_host_module_init(void);


#ifdef __cplusplus
}
#endif
