/**
  ******************************************************************************
  * @file      usbh_cdc_user_common.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/28
  * @brief     USB用户自定义参数
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/28 <td>1.0     <td>lixingyu   <td>Create the initial version
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

/* ======================== 本地模块文件引用（可选） ============================ */


/* ================================ 头文件宏定义 ================================ */


/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

typedef struct {
    uint16_t vid;           // 厂商ID
    uint16_t pid;           // 产品ID
    uint8_t  modem_port;    // Modem通道口编号
    uint8_t  second_at_port;// SecondAT通道口编号
    bool     allow_connect; // 允许连接标志
} usbh_cdc_user_info_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  根据VID和PID查找usbh_cdc_user_list中的设备信息
 *
 * @param  vid       设备的厂商ID
 * @param  pid       设备的产品ID
 * @param  out_info  输出参数，指向找到的usbh_cdc_user_info_t结构体指针，未找到时为NULL
 * @return ESP_OK    查找到对应设备信息
 * @return ESP_FAIL  未查找到对应设备信息
 */
esp_err_t find_usbh_cdc_user_info(uint16_t vid, uint16_t pid, const usbh_cdc_user_info_t **out_info);

#ifdef __cplusplus
}
#endif
