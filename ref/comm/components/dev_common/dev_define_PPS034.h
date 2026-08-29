/**
  ******************************************************************************
  * @file      dev_define_PPS034.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/05/06
  * @brief     PPS034系统头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/11 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* =============================== 设备版本宏定义 ============================= */

/*modbus beta协议版本*/
#define DEVICE_MODBUS_VERSION          2021

/*iot协议版本*/
#define DEVICE_IOT_PROTOCOL_VERSION    7

/*iot研发固件版本*/
#define DEVICE_IOT_SOFT_DEBUG_VERSION  100680151

/*iot出厂固件版本*/
#define DEVICE_IOT_SOFT_VERSION        100680101

/* ================================ 设备功能宏定义 ============================ */

/*串口标定使能*/
#define CONFIG_UART0_SN_FACTORY_WRITE_ENABLE 

/*can使能*/
#define CONFIG_CAN_PORT_ENABLE

#ifdef CONFIG_CAN_PORT_ENABLE

/*can alpha协议的BAT使能*/
//#define BAT_CAN_PROTOCOL_ALPHA_ENABLE

/*can beta协议的BAT使能*/
#define BAT_CAN_PROTOCOL_BETA_ENABLE

/*EMS_CTRL(功能5)使能*/
#define CONFIG_IOT_CAN_EMS_CTRL_FUNC5

#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5
/*IOT端LED设置使能*/
#define CONFIG_IOT_LED_SET_ENABLE
#endif

#endif

/*内部电池包使能*/
#define INTERNAL_PACK_ENABLE

/*下级芯片处于boot时自动发起升级使能*/
#define CONFIG_MCU_AUTO_UPDATE_IN_BOOT
#define IMAGE_CUSTOM_BACK_TYPE1         1   // ARM

/*Modbus 21000段上报使能*/
#define CONFIG_MODBUS_21000_USE_ENABLE

/*智能插座设备接入支持使能*/
#define CONFIG_WLCC_PLUG_SUPPLY_ENABLE

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
#define CONFIG_RTC_TIME_SOURCE_CHANGE   2

/* =============================== 设备驱动宏定义 ============================= */

/*CAN通信波特率设置*/
#define CONFIG_CAN_BAUD_KBITS          250

/* =============================== 设备参数宏定义 ============================= */

/*系统并机数量*/
#define SYSTEM_PARALLELED_NUM          1

/*通用逆变器CAN_ID分配（存在IOT和ARM和DSP情况）*/
#define COMMON_INV_ARM_CAN_ADDR        0x10
#define COMMON_INV_DSP_CAN_ADDR        0x16
#define COMMON_INV_IOT_CAN_ADDR        0x1C

/*电池包信息（本机为不扩包便携储）*/
#define COMMON_PACK_BMS_CAN_ADDR       0x60
#define COMMON_PACK_INGROUP_MAX_NUM    1	    // 系统内单个堆（BA）内对应的电池包（BCU）的数量
#define COMMON_PACK_GROUP_MAX_NUM      1        // 系统内堆（BA）的数量
#define COMMON_PACK_MAX_NUM            (COMMON_PACK_GROUP_MAX_NUM * COMMON_PACK_INGROUP_MAX_NUM)

/* =========================================================================== */

