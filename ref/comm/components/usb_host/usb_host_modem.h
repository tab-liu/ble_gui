/**
  ******************************************************************************
  * @file      usb_host_modem.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/2
  * @brief     USB转4G控制模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/26  <td>1.0     <td>lixingyu   <td>Create the initial version
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

/* ======================== 本地模块文件引用（可选） ============================ */


/* ================================ 头文件宏定义 ================================ */


/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

/**
 * @brief Modem 步骤状态枚举
 *
 * 此枚举定义了Modem工作流程中的主要步骤/状态。
 */
enum {
    STEP_MODEM_IDLE = 0,        /*!< Modem空闲：无操作进行中 */
    STEP_MODEM_PPP_START,       /*!< Modem进入PPP数据模式 */
    STEP_MODEM_RUNNING,         /*!< Modem运行中：Modem处于正常工作状态 */
    STEP_MODEM_PPP_STOP,        /*!< Modem退出PPP数据模式 */
    STEP_MODEM_UNKNOWN          /*!< Modem未知响应：收到未知或异常响应 */
};

/**
 * @brief 4G模块信息结构体
 *
 * 用于存储4G模块的主要身份标识、网络参数、信号质量及状态信息。
 * 包括IMEI、IMSI、ICCID、IP地址、DNS、MAC、信号强度、误码率、网络连接状态和SIM卡状态等。
 */
typedef struct {
    uint8_t IMEI[20];        /*!< 模块IMEI号（国际移动设备识别码），每个设备唯一的标识 */
    uint8_t IMSI[20];        /*!< SIM卡IMSI号（国际移动用户识别码），每张SIM卡唯一的标识 */
    uint8_t ICCID[20];       /*!< SIM卡ICCID号（集成电路卡识别码），SIM卡的序列号 */
    uint8_t dns1[4];         /*!< 4G PPP连接分配的主DNS服务器IPv4地址（4字节） */
    uint8_t dns2[4];         /*!< 4G PPP连接分配的备用DNS服务器IPv4地址（4字节） */
    uint8_t ipv4[4];         /*!< 4G PPP连接分配的本地IPv4地址（4字节） */
    uint8_t gw[4];           /*!< 4G PPP连接分配的网关IPv4地址（4字节） */
    uint8_t mask[4];         /*!< 4G PPP连接分配的子网掩码IPv4地址（4字节） */
    uint8_t mac[6];          /*!< PPP接口使用的MAC地址（如适用，PPP模式下通常为0） */
    uint8_t rssi;            /*!< 信号强度（RSSI），2~31对应-109~-51dBm，99表示未知 */
    uint8_t ber;             /*!< 误码率（BER），单位百分比，99表示未知或不可检测 */
    bool net_status;         /*!< 4G网络连接状态，true表示已连接，false表示未连接 */
    bool sim_card_status;    /*!< SIM卡状态，true表示已插卡，false表示未检测到SIM卡 */
    int GPS_X;               /*!< GPS定位X坐标（如经度，具体含义根据实际应用定义） */
    int GPS_Y;               /*!< GPS定位Y坐标（如纬度，具体含义根据实际应用定义） */
    uint8_t gps_num;         /*!< Number of GPS satellites in view */
    uint8_t gl_num;          /*!< Number of GLONASS satellites in view */
    uint8_t gal_num;         /*!< Number of Galileo satellites in view */
    uint8_t bd_num;          /*!< Number of BeiDou satellites in view */
} usb_4g_info_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB 4G模块状态机
 * 
 * 根据当前Usb_4G_Config_Step状态，执行4G模块的初始化、待机等操作。
 */
void Usb_4G_Module_State_Machine(void);

/**
 * @brief 启用4G模块
 */
void iot_4G_start(void);

/**
 * @brief 停用4G模块
 */
void iot_4G_stop(void);

/**
 * @brief 获取4G网络连接状态
 *
 * @return true 表示已连接，false 表示未连接
 */
bool iot_4g_net_get_status(void);

/**
 * @brief 获取4G模块信息结构体指针
 *
 * @return 指向当前4G模块信息的usb_4g_info_t结构体常量指针
 */
const usb_4g_info_t * iot_4g_info_get(void);

/**
 * @brief 设置4G模块的APN参数
 *
 * 本函数用于设置4G模块的接入点名称（APN）。
 * - 如果当前4G模块处于空闲状态（STEP_MODEM_IDLE），则直接初始化APN；
 * - 如果不处于空闲状态，则根据USB设备连接状态，调用不同方式设置APN。
 *
 * @param new_apn 指向新APN字符串的指针
 */
void iot_4g_set_apn(const char *new_apn);

/**
 * @brief 获取4G模块初始化时间
 */
uint32_t iot_4g_init_time_get(void);

/**
 * @brief 获取4G信号强度
 */
int8_t iot_4g_rssi(void);

#ifdef __cplusplus
}
#endif
