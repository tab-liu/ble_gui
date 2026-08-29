/**
  ******************************************************************************
  * @file      bms_can.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/1/6
  * @brief     48V BMS-CAN协议处理（移植钟工）
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/6   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include "driver/twai.h"
#include "bms_type.h"

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取bat_data结构体的指针
 * @return bat_data_t* 指向bat_data的指针
 */
bat_data_t* get_bat_data_ptr(void);

/**
 * @brief 根据索引获取CAN地址
 * @param[in] index 电池包索引
 * @return CAN地址，失败返回0
 */
int get_addr_trace(uint8_t index);

/**
 * @brief 获取指定索引电池包的类型字符串
 * @param[in] index 电池包索引
 * @return 类型字符串指针
 */
char* get_bat_type(uint8_t index);

/**
 * @brief 根据CAN地址和功能码获取/分配内部电池包索引
 * @param[in] addr CAN地址
 * @param[in] func 功能码
 * @return 索引，-1无效，0xFF主包帧
 */
int bms_addr_trace(uint8_t addr, uint8_t func);

/**
 * @brief 根据CAN地址查找电池包索引
 * @param[in] addr CAN地址
 * @return 索引，-1无效
 */
int bms_addr_to_index(uint8_t addr);

/**
 * @brief 根据版本号查找电池包索引
 * @param[in] version 版本号
 * @return 索引，-1无效
 */
int bms_version_to_index(uint32_t version);

/**
 * @brief 1秒周期的BMS CAN发送任务
 */
void bms_can_send_task_1s(void);

/**
 * @brief 处理BMS CAN帧
 * @param[in] can_frame 指向CAN帧结构体的指针
 */
void bms_can_handle(twai_message_t *can_frame);

/**
 * @brief 更新所有电池包数据到Modbus寄存器
 */
void update_bat_data_to_modbus(void);

/**
 * @brief 发送当前系统时间到BMS
 */
void bms_ann_now_time(void);

/**
 * @brief 根据固件大版本号获取电池包系列名称
 * @param[in] version_type 固件大版本号
 * @return 系列名称字符串
 */
const char* get_bms_series_name(uint16_t version_type);

/**
 * @brief 根据固件大版本号获取电池包系列类型索引
 * @param[in] version_type 固件大版本号
 * @return 类型索引
 */
uint8_t get_bms_series_type_index(uint16_t version_type);

/**
 * @brief 判断是否为alpha升级协议
 * @param[in] version_type 固件大版本号
 * @return true为alpha协议，false为beta协议
 */
bool is_bms_alpha_update_protocol(uint16_t version_type);

/**
 * @brief 更新电池维护状态
 */
void bms_alpha_aging_status_update(uint16_t data);

#ifdef __cplusplus
}
#endif

