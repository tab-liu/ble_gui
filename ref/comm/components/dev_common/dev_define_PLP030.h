/**
  ******************************************************************************
  * @file      dev_define_PLP030.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/11
  * @brief     PLP030系统头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/11 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

#include "sdkconfig.h"

/* =============================== 设备版本宏定义 ============================= */

/*modbus beta协议版本*/
#define DEVICE_MODBUS_VERSION          2022

/*iot协议版本*/
#define DEVICE_IOT_PROTOCOL_VERSION    7

#ifdef CONFIG_SPIRAM_MODE_QUAD

/*iot研发固件版本*/
#define DEVICE_IOT_SOFT_DEBUG_VERSION  100650154

/*iot出厂固件版本*/
#define DEVICE_IOT_SOFT_VERSION        100650103

#else

/*iot研发固件版本*/
#define DEVICE_IOT_SOFT_DEBUG_VERSION  100450155

/*iot出厂固件版本*/
#define DEVICE_IOT_SOFT_VERSION        100450104

#endif

/*设备类型*/
#define DEVICE_TYPE_ASCII              "PI5000EG"

/* ================================ 设备功能宏定义 ============================ */

/*串口标定使能*/
//#define CONFIG_UART0_SN_FACTORY_WRITE_ENABLE 

/*CAN标定使能*/
#define CONFIG_CAN_FACTORY_WRITE_ENABLE

/*can使能*/
#define CONFIG_CAN_PORT_ENABLE

#ifdef CONFIG_CAN_PORT_ENABLE

/*can alpha协议的BAT使能*/
#define BAT_CAN_PROTOCOL_ALPHA_ENABLE

/*can beta协议的BAT使能*/
//#define BAT_CAN_PROTOCOL_BETA_ENABLE

/*一键升级总线静默超时使能*/
//#define CONFIG_CAN_OTA_READY_TIMEOUT

#ifdef CONFIG_CAN_OTA_READY_TIMEOUT

/*系统升级总进度控制使能*/
#define CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE

#endif

#endif

/*内部电池包使能*/
#define INTERNAL_PACK_ENABLE

/*DCHUB设备CAN接入支持使能*/
#define CONFIG_CAN_DCHUB_SUPPLY_ENABLE

/*DCDC设备CAN接入支持使能*/
#define CONFIG_CAN_DCDC_SUPPLY_ENABLE

/*USB使能*/
#define CONFIG_USB_PORT_ENABLE 

#ifdef CONFIG_USB_PORT_ENABLE

/*USB接口HOST主机HID类功能启用使能*/
#define CONFIG_USB_HOST_HID_ENABLE

/*USB_4G功能启用使能*/
#define CONFIG_USB_4G_MODULE_ENABLE

#endif

/*二代混合IOT标定使能*/
#define CONFIG_IOT_2ND_GENERATION_MIX_ENABLE

/*Modbus 21000段上报使能*/
#define CONFIG_MODBUS_21000_USE_ENABLE

/*paygo功能支持*/
//#define CONFIG_FUNC_PAYGO_ENABLE

/*RS485通信接口使能*/
//#define CONFIG_RS485_PORT_ENABLE

/*智能发电机控制使能*/
#define CONFIG_SMART_GENERATOR_ENABLE

/*MQTT CONNECT(连接认证)扩展 : IOT物联网的联网方式*/
#define CONFIG_MQTT_CONNECT_EXTEND_CHL

/*
风暴预警备电使能
0：无效
1：便携
2：家储
*/
#define CONFIG_DISASTER_WARN_CHANNEL    1

/*
时间来源配置宏定义
0：本地RTC
1：CAN(INV)
2：CAN(BMS_BELTA)
3：CAN(BMS_ALPHA)
*/
#define CONFIG_RTC_TIME_SOURCE_CHANGE   1

/* =============================== 设备驱动宏定义 ============================= */

/*CAN通信波特率设置*/
#define CONFIG_CAN_BAUD_KBITS          100

/*RS485通信波特率*/
#define CONFIG_RS485_BAUD              115200

/* =============================== 设备参数宏定义 ============================= */

/*系统并机数量*/
#define SYSTEM_PARALLELED_NUM          1

/*通用逆变器CAN_ID分配（存在IOT和ARM和DSP和TI137情况）*/
#define COMMON_INV_ARM_CAN_ADDR        0x10
#define COMMON_INV_DSP_CAN_ADDR        0x15
#define COMMON_INV_TI137_CAN_ADDR      0x1A
#define COMMON_INV_IOT_CAN_ADDR        0x1F

/*电池包信息（本机为扩包便携储）*/
#define COMMON_PACK_BMS_CAN_ADDR       0x60
#define COMMON_PACK_INGROUP_MAX_NUM    8	    // 系统内单个堆（BA）内对应的电池包（BCU）的数量
#define COMMON_PACK_GROUP_MAX_NUM      1        // 系统内堆（BA）的数量
#define COMMON_PACK_MAX_NUM            (COMMON_PACK_GROUP_MAX_NUM * COMMON_PACK_INGROUP_MAX_NUM)

/* =========================================================================== */

