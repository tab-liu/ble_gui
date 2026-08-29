/**
  ******************************************************************************
  * @file      usb_host_hid.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/31
  * @brief     USB主机HID类设备处理
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

/* ======================== 本地模块文件引用（可选） ============================ */


/* ================================ 头文件宏定义 ================================ */

/*HID键盘使能*/
#define CONFIG_USB_HID_KEYBOARD_ENABLE

/*HID鼠标使能*/
#define CONFIG_USB_HID_MOUSE_ENABLE

/*HID通用设备使能*/
#define CONFIG_USB_HID_GENERAL_ENABLE

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 HID Host 驱动
 */
void hid_host_init(void);

/**
 * @brief 清除 HID Host 驱动
 */
void hid_host_deinit(void);

/**
 * @brief 从 key_input_queue 获取一条输入行，拷贝到目标缓冲区
 *
 * @param[out] dst      目标存储地址
 * @param[in]  dst_size 目标存储区最大长度
 * @param[out] out_len  实际拷贝的长度
 * @param[in]  timeout_ticks 等待超时时间（单位：tick）
 * @return true 获取成功，false 队列无数据或出错
 */
bool hid_receive_key_input_from_queue(char *dst, size_t dst_size, size_t *out_len, TickType_t timeout_ticks);

/**
 * @brief 获取当前键盘输入缓存及其长度
 *
 * 功能说明：
 * 1. 返回当前键盘输入缓存的指针和有效长度，便于外部模块直接读取当前输入内容。
 * 2. 仅返回指针和长度，不做任何修改操作。
 *
 * @param[out] buf  返回输入缓存的指针地址（const char **，指向只读字符串）
 * @param[out] len  返回输入缓存的有效长度（size_t *）
 */
void hid_receive_key_input_real(const char **buf, size_t *len);

#ifdef __cplusplus
}
#endif
