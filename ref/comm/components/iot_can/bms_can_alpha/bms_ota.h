/**
  ******************************************************************************
  * @file      bms_ota.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/1/8
  * @brief     48V BMS-CAN协议升级（移植钟工）
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/8   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include "driver/twai.h"

/* ================================ 头文件宏定义 ================================ */

#define BMS_OTA_READY   ((uint32_t)0x0CF50038)
#define BMS_OTA_START   ((uint32_t)0x0CF50039)
#define BMS_OTA_DATA    ((uint32_t)0x0CF5003B)
#define BMS_OTA_END     ((uint32_t)0x0CF5003E)

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 判断当前是否正在进行BMS OTA升级
 * @return true 正在升级，false 未升级
 */
bool bms_ota_is_doing(void);

/**
 * @brief OTA升级过程定时处理
 * @param[in] interval 定时器周期（单位ms）
 */
void bms_ota_time_handle(int interval);

/**
 * @brief 初始化BMS OTA升级流程
 * @param[in] type        升级类型
 * @param[in] totalKbyte  升级总大小（单位KB）
 * @param[in] index       APP传入的电池包索引（从0开始）
 * @param[in] addr        实际电池包CAN地址
 * @param[in] version     升级目标版本号
 * @return int 初始化结果，0成功，其他为错误码
 */
int bms_ota_init(uint8_t type, uint32_t totalKbyte, uint8_t index, uint8_t addr, uint32_t version);

/**
 * @brief 处理BMS OTA相关CAN帧
 * @param[in] can_frame 接收到的CAN帧指针
 */
void bms_ota_handle(const twai_message_t *can_frame);

#ifdef __cplusplus
}
#endif

