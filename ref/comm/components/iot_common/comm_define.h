/**
  ******************************************************************************
  * @file      comm_define.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/11
  * @brief     系统通用头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/11 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include "esp_event.h" //  需要
#include <time.h>
#include <sys/time.h>
#include "utils.h"
#include "sdkconfig.h"

/**********************************************************************************/

/*产品类型选择，仅支持一个*/
//#include "dev_define_PPS028.h"
#include "dev_define_PLP030.h"
//#include "dev_define_PPS034.h"
//#include "dev_define_TPR013.h"

/**********************************************************************************/

/*代码量产版本使能，屏蔽时为研发调试版本*/
//#define MASS_PRODUCTION_CONFIG_ENABLE

/**********************************************************************************/

/*黑匣子功能*/
#define INV_LOG_DETAILED_INFO_RECORD

/*uart使能*/
//#define CONFIG_UART_PORT_ENABLE

/*BLE clent 使能，开启BLE HOST主设备查询发现功能，Charger2/AC2AC需要*/
//#define BLE_CLIENT_ENABLE

/*wlcc使能*/
#define CONFIG_WLCC_ENABLE

/*数据通道使能*/
#define CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION

#ifdef CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION

/*数据通道增量读取使能*/
#define CONFIG_UDT_INCREMENTAL_READ_ENABLE

/*数据通道周期增量上报使能*/
#define CONFIG_UDT_PERIOD_INCREMENTAL_ANNOUNCE_ENABLE

/*数据通道断点续传使能*/
#define CONFIG_UDT_BREAKPOINT_RESUME_ENABLE

#endif

/*Wi-Fi AP 使能*/
#define CONFIG_WIFI_AP_MODE_ENABLE

/*MODBUS TCP使能*/
#define CONFIG_ENABLE_MD_TCP_SLAVE

/*WebServer使能*/
#define CONFIG_WEBSERVER_ENABLE

/*寄存器修改日志处理模块使能*/
#define CONFIG_REG_CHANGE_LOG_SAVE_ENABLE

/*设备接入日志处理模块使能*/
#define CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE

/*能量计算存储模块使能*/
#define CONFIG_ENERGY_FILE_PROCESS_ENABLE

/*蓝牙持续广播使能*/
#define CONFIG_BLE_ADV_ENABLE

/*蓝牙磁吸屏配套支持使能*/
#define CONFIG_BLE_LCD_LINK_ENABLE

#if defined(CONFIG_CAN_PORT_ENABLE)

/*CAN 主机主动轮询查询*/
#define CAN_MASTER_POLL_ENABLE

/*modbus读取触发CAN读取使能*/
#define MODBUS_READ_TRIGER_CAN_READ_ENABLE

/*can设置回读使能*/
#define CAN_CMD_SET_READ_BACK_ENABLE

/*IOT模块CAN被动升级使能*/
 #define IOT_CAN_RECEIVER_USE_ENABLE

/*CAN并机排地址策略*/
#define CAN_ADDRESS_PARALLEL_ENABLE

/*DCDC和INV的协同充电CAN报文交互，新增*/
// #define CAN_EMS_WITH_INV_ENABLE

/*CAN总线时间获取使能*/
#define CONFIG_RTC_TIME_VAILD_FROM_CAN

/*CAN总线时间检查逻辑使能*/
#define CONFIG_RTC_TIME_CHECK_SET_TO_CAN

/*读取ARM本地历史记录等功能使能*/
//#define MODBUS_GET_HISTORY_FROM_ARM_ENABLE

/*CAN 设备当日能量上报使能*/
//#define CONFIG_CAN_ENERGY_ANNOUNCE_ENABLE

/*电池包分类型版本信息统计使能*/
#define BATTERY_TYPE_VER_STAT_ENABLE

/*系统休眠CAN总线静默使能*/
#define CONFIG_SYSTEM_SLEEP_CAN_SILENT_ENABLE

#endif

/*加密证书保存到文件系统，否则保存到分区*/
#define ENCRYPT_CERT_USE_FILE_SYSTEM 1

/*文件系统目录使能(非必要不改动)*/
#define FILE_SYSTEM_DIRECTORY_ENABLE

/*本地存储历史故障记录使能*/
#define HISTORY_REPORT_ENABLE

/*HTTP升级使能*/
#define CONFIG_ENABLE_HTTP_OTA

/*HTTP我司服务器获取时间使能*/
#define TIME_GET_FROM_SERVER_BY_HTTP_ENABLE

/*云端周期获取时间通道选择
0：禁止，1：http，2：mqtt，3：ntp（暂未支持）*/
#define TIME_GET_FROM_SERVER_PERIOD_CHL  2

/*HTTP检查固件更新状态使能*/
#define HTTP_CHECK_FIRMWARE_UPDATE

/*HTTP获取省钱参数使能*/
#define HTTP_GET_MONEY_SAVE_DATE

/*原智能控制逻辑（19000段）*/
#define CONFIG_RELAY_CTRL_ENABLE

/*新智能TOU控制使能*/
#define CONFIG_SMART_TOU_CTRL_V1_ENABLE

// 宏定义：USE_PINNED_CORE=1 使用 xTaskCreatePinnedToCore，否则使用 xTaskCreate
#define USE_PINNED_CORE 0

/*三WIFI自切换逻辑使能*/
#define CONFIG_MORE_WIFI_STA_LINK_ENABLE

/*WIFI静态IP支持使能*/
#define CONFIG_WIFI_STA_UNDHCP_ENABLE

/*禁止馈电使能*/
#define CONFIG_GRID_FEEDBACK_DISABLE

/*UDP日志重定向(研发版本默认打开，生产版本默认关闭)
0：禁止，1：支持python脚本，2：支持DevAssist*/
#define CONFIG_LOG_UDP_REDIRECT  2

/*MQTT开放服务器使能*/
#define CONFIG_OPEN_MQTT_ENABLE

/*Modbus 3700段数据使能*/
#define CONFIG_MODBUS_REG_ADDR_3700_ENABLE

/*我司服务器IOT40000段调试数据上报*/
#define CONFIG_MQTT1_WLCC_REG_40000_IOT_DEBUG_ENABLE

/*IOT模组内部温度检测使能*/
#define CONFIG_IOT_INTERNAL_TEMPERATURE_GET_ENABLE

/*MODBUS_TLV寄存器组合协议使能*/
#define CONFIG_MODBUS_REG_TLV_ENABLE

/*MQTT载荷协议TLV扩展协议使能*/
#define CONFIG_MQTT_PAYLOAD_TLV_ENABLE

/*我司MQTT服务器通信加密使能（协议使能控制）*/
#define CONFIG_MQTT1_TCP_ENCRYPT_ENABLE

/*Devassist OTA升级触发使能*/
#define CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE

/*MODBUS_TCP通信加密使能（协议使能控制）*/
#define CONFIG_MODBUS_TCP_ENCRYPT_ENABLE

/*MQTT登录日志模块使能*/
#define CONFIG_MQTT_LOG_SAVE_ENABLE

/**********************************************************************************/

/*调试功能，出厂版本不开放*/
#ifndef MASS_PRODUCTION_CONFIG_ENABLE

/*INV写入设置后数据预判修改使能*/
//#define INV_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE

/*PACK写入设置后数据预判修改使能*/
//#define PACK_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE

/*配件写入设置后数据预判修改使能*/
//#define PARTS_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE

/*云端AC_EMS控制使能*/
//#define CONFIG_AC_EMS_CLOUD_ENABLE

/*电网断电次数检查使能*/
//#define CONFIG_GRID_OFF_COUNT_ENABLE

/*IOT低功耗模式使能*/
//#define CONFIG_IOT_AUTO_LIGHT_SLEEP_ENABLE

/*有线以太网连接使能*/
//#define CONFIG_USE_SPI_ETHERNET_ENABLE

/*文件系统内文件数据升级使能*/
// #define CONFIG_FILE_DATA_OTA_ENABLE

/*服从服务器云端连接允许使能*/
//#define CONFIG_MQTT_CONNECT_HTTP_TOKEN_ENABLE

/*外部RTC启用使能*/
//#define CONFIG_IIC_BUS_EXT_RTC_ENABLE

/*外部EEPROM启用使能*/
//#define CONFIG_IIC_BUS_EXT_EEPROM_ENABLE

/*外部SPI_FLASH启用使能（非模组）*/
//#define CONFIG_USER_SPI_EXT_FLASH_ENABLE

/*MQTT报文内部任务异步发送使能*/
//#define CONFIG_MQTT_CLIENT_SEND_BY_ENQUEUE

/*智能电表设备接入支持使能*/
//#define CONFIG_WLCC_METER_SUPPLY_ENABLE

/*离网并机负载功率调度使能*/
//#define CONFIG_OFF_GRID_LOAD_POWER_DISTRIBUTION

/*MQTT接收回调使能（含拼包逻辑）*/
//#define CONFIG_MQTT_RECV_CALLBACK_ENABLE

/*IOT云边参数同步使能（建议使能CONFIG_MQTT_RECV_CALLBACK_ENABLE）*/
//#define CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE

/*WLCC并机策略*/
//#define WLCC_ADDRESS_PARALLEL_ENABLE

/*USB接口HOST主机HID类功能启用使能（需先使能CONFIG_USB_HOST_ENABLE）*/
//#define CONFIG_USB_HOST_HID_ENABLE

/*4G模式下MQTT周期上报替换为ModbusTLV格式（待服务器适配）*/
//#define CONFIG_MQTT_4G_ANNOUNCE_MODBUS_TLV_ENABLE

#endif

/**********************************************************************************/

/*调试打印，出厂版本不开放*/
#ifndef MASS_PRODUCTION_CONFIG_ENABLE

/*LOG打印使能*/
#define CONFIG_ENABLE_LOG_DEBUG

/*Modbus协议交互调试打印*/
//#define MODBUS_DEBUG_LOG_ENABLE

/*BLE_ADV交互调试打印*/
//#define BLE_ADV_DEBUG_LOG_ENABLE

/*本地数据初始化调试打印*/
#define DATA_INIT_FROM_LITTLEFS_DEBUG_LOG

/*文件系统初始化调试打印*/
#define LITTLEFS_INIT_DEBUG_LOG

/*WIFI连接信息打印*/
#define WIFI_CONNECT_LINK_INFO_DEBUG_LOG

#endif

/********************************************************************/

/*modbus beta协议版本*/
#define MODBUS_VERSION          DEVICE_MODBUS_VERSION

/*iot协议版本*/
#define IOT_PROTOCOL_VERSION    DEVICE_IOT_PROTOCOL_VERSION

/*其他版本*/
#define HW_VERSION              2
#define SUB_VERSION             7

/*HTTP_UTC_TIME接口版本*/
#define HTTP_UTC_TIME_VERSION   2 

/********************************************************************/

/*公司服务器地址*/
#define DEV_ENV_HOST    "dev-iot.poweroak.ltd:18760"
#define TEST_ENV_HOST   "test-iot.poweroak.ltd:18760"
#define PRO_ENV_HOST    "iot.bluettipower.com:18760"

#define DEV_ENV_URL     "http://dev-gw.poweroak.ltd:18888"
#define TEST_ENV_URL    "http://test-gw.poweroak.ltd:18888"
#define PRO_ENV_URL     "https://gw.bluettipower.com"

#define MQTT_SERVER_ADDRESS_DEFAULT     "dev-iot.poweroak.ltd:18083"//MQTT公网服务器默认地址
#define MQTT_SERVER_PASSWD_DEFAULT      "admin123456"               //MQTT公网服务器登录默认密码

/*研发环境证书路径*/
#define DEV_CERT_HTTP_URL           "http://dev-gwpry.poweroak.ltd:18888/api/midppkic/pki/84670462/update/device"
#define DEV_CERT_HTTP_URL_FIRST     "http://dev-gwpry.poweroak.ltd:18888/api/midppkic/pki/84670462/apply/device"
#define DEV_CERT_HTTP_DOWN_URL      "http://dev-gwpry.poweroak.ltd:18888/api/midppkic/pki/84670462/download"

/*生产环境证书路径*/
#define PRO_CERT_HTTP_URL           "https://gw.bluettipower.com/api/midppkic/pki/84670462/update/device"
#define PRO_CERT_HTTP_URL_FIRST     "https://gw.bluettipower.com/api/midppkic/pki/84670462/apply/device"
#define PRO_CERT_HTTP_DOWN_URL      "https://gw.bluettipower.com/api/midppkic/pki/84670462/download"

/*测试mqtt主题*/
#define CONFIG_EXAMPLE_MQTT_TEST_TOPIC "/ci/esp-modem/pppos-client2"

/*网口名称定义*/
#define NETIF_KEY_ETH       "ETH_DEF"
#define NETIF_KEY_WIFI_STA  "WIFI_STA_DEF"
#define NETIF_KEY_WIFI_AP   "WIFI_AP_DEF"
#define NETIF_KEY_PPP       "PPP_DEF"

/********************************************************************/

// 本地存储配置参数区域的有效标志（研发环境）
#define DEV_SET_DATA_VALID_FLAG 0xAA02          

// 本地存储配置参数区域的有效标志
#define MASS_PRODUCTION_SET_DATA_VALID_FLAG 0xAA55

/********************************************************************/

#define MASS_PRODUCTION_ENV_HOST_URL        PRO_ENV_HOST    // 量产bluetti服务器地址
#define MASS_PRODUCTION_WIFI_STA_SSID       "BLUETTI"       // 默认wifi SSID
#define MASS_PRODUCTION_WIFI_STA_PASSWORD   "poweroak"      // 默认wifi 密码
#define MASS_PRODUCTION_WIFI_AP_PASSWORD    "poweroak"      // 默认wifi 密码

// 未标定使用的默认type
#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
#define MASS_PRODUCTION_DEFAULT_DEV_TYPE    DEVICE_TYPE_ASCII       
#else
#define MASS_PRODUCTION_DEFAULT_DEV_TYPE    "BLUETTI"
#endif

// 未标定使用的默认SN
#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
#define MASS_PRODUCTION_DEFAULT_DEV_SN      iot_default_sn_by_mac()
#else
#define MASS_PRODUCTION_DEFAULT_DEV_SN      123456          
#endif

// 未标定使用的默认type
#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
#define IOT_FACTORY_DEFAULT_DEV_TYPE        DEVICE_TYPE_ASCII               
#else
#define IOT_FACTORY_DEFAULT_DEV_TYPE        "IOT" 
#endif

// 未标定使用的默认SN
#define IOT_FACTORY_DEFAULT_DEV_SN          iot_default_sn_by_mac()         

// 未标定使用的默认安全码
#define IOT_FACTORY_DEFAULT_SAFE_CODE       0                               

// 蓝牙广播默认密钥
#define BLE_ADV_KEY_DEFAULT                 "BluettiBluetooth"

/*********************************************************************/

#ifdef MASS_PRODUCTION_CONFIG_ENABLE /*出厂*/

/*文件系统存储标志*/
#define EPROM_READY_FLAG            MASS_PRODUCTION_SET_DATA_VALID_FLAG

/*bluetti服务器地址*/
#define BLUETTI_DNS                 MASS_PRODUCTION_ENV_HOST_URL

/*默认wifi及密码*/
#define DEF_WIFI_STA1_SSID			MASS_PRODUCTION_WIFI_STA_SSID
#define DEF_WIFI_STA1_PASSWD	    MASS_PRODUCTION_WIFI_STA_PASSWORD
#define DEF_WIFI_STA2_SSID			MASS_PRODUCTION_WIFI_STA_SSID
#define DEF_WIFI_STA2_PASSWD	    MASS_PRODUCTION_WIFI_STA_PASSWORD
#define DEF_WIFI_STA3_SSID			MASS_PRODUCTION_WIFI_STA_SSID
#define DEF_WIFI_STA3_PASSWD	    MASS_PRODUCTION_WIFI_STA_PASSWORD
#define DEF_WIFI_AP_PASSWD	        MASS_PRODUCTION_WIFI_AP_PASSWORD

/*软件版本*/
#define IOT_VERSION                 DEVICE_IOT_SOFT_VERSION

/*证书路径*/
#define CERT_HTTP_URL               PRO_CERT_HTTP_URL
#define CERT_HTTP_URL_FIRST         PRO_CERT_HTTP_URL_FIRST
#define CERT_HTTP_DOWN_URL          PRO_CERT_HTTP_DOWN_URL

#else    /*研发调试*/

/*文件系统存储标志*/
#define EPROM_READY_FLAG            DEV_SET_DATA_VALID_FLAG

/*bluetti服务器地址*/
#define BLUETTI_DNS                 DEV_ENV_HOST

/*默认wifi及密码*/
#define DEF_WIFI_STA1_SSID			"POWEROAKHZ"
#define DEF_WIFI_STA1_PASSWD	    "88888888"
#define DEF_WIFI_STA2_SSID			"de-dev-2.4"
#define DEF_WIFI_STA2_PASSWD	    "88888888"
#define DEF_WIFI_STA3_SSID			MASS_PRODUCTION_WIFI_STA_SSID
#define DEF_WIFI_STA3_PASSWD	    MASS_PRODUCTION_WIFI_STA_PASSWORD
#define DEF_WIFI_AP_PASSWD	        MASS_PRODUCTION_WIFI_AP_PASSWORD

/*软件版本*/
#define IOT_VERSION                 DEVICE_IOT_SOFT_DEBUG_VERSION         

/*证书路径*/
#define CERT_HTTP_URL               DEV_CERT_HTTP_URL
#define CERT_HTTP_URL_FIRST         DEV_CERT_HTTP_URL_FIRST
#define CERT_HTTP_DOWN_URL          DEV_CERT_HTTP_DOWN_URL

/*LOG打印使能*/
#define CONFIG_ENABLE_LOG_DEBUG

#endif

/**********************************************************************************/

#define NET_SUB1G_MAX_POINT 0                   // 32//最大网络节点数量,Sub 1GHz网络,AC380无 sub 1ghz
#define NET_WIFI_INVBAT_POINT (uint16_t)100     // 普通储能逆变器数量,协议预留
#define NET_WIFI_INVBAT_POINT_MINI (uint16_t)10 // 普通储能逆变器数量,设备实际预留

#define NET_WIFI_S1_POINT 16                                                                     // S1点位
#define NET_WIFI_METER_POINT 3
#define NET_WIFI_MIX_POINT 20                                                                    // 除S1外其他 WIFI点位
#define NET_WIFI_MAX_POINT (NET_WIFI_INVBAT_POINT_MINI + NET_WIFI_S1_POINT + NET_WIFI_METER_POINT + NET_WIFI_MIX_POINT) // 最大网络节点数量,WIFI网络

#define BIND_ARRAY_MAX 5          // 绑定组最大数量
#define BIND_POINT_IN1ARRAY_MAX 5 // 绑定单组内最大节点数量

#define CAN_INV_MAX_POINT 10 // CAN INV并机最多数量

/*21000段最大设备数量*/
#define MODBUS_21000_DEVICE_MAX_CNT 100

/*绑定帧内单帧最大发送设备数量（当前不受限Buffer, 只受限最大存储数量）*/
#define BIND_READ_MAX_DEV_NUM MODBUS_21000_DEVICE_MAX_CNT

#define CAN_DATA_POLL_INDEX_MAXNUM 2

#define TIME_BASE_1S_CNT_10S 10 // 10s
#define TIME_BASE_1S_CNT_2S 2   //
#define TIME_BASE_1S_CNT_60S 60 // 60s

#define TIME_BASE_100MS_CNT_10S 100 // 10s

#define TEST_MODE_OLDING     // 0xAA:使能老化模式
#define TEST_MODE_NORMAL     // 0x55:进入正常模式
#define TEST_MODE_BOARD_TEST // 0x40:进入单板测试模式
#define TEST_MODE_REVD       // 0xA1:预留
#define TEST_MODE_RESET      // 0xA2:重启

/*
modbus slave address地址分配：
0~100：储能 INV + BAT
S1部分：101~116
除S1外其他 WIFI点位部分:117~117+19
*/

#define MODBUS_SLAVE_ADDR_WIFI_INVBAT_START (1) //

#define MODBUS_SLAVE_ADDR_WIFI_START (uint16_t)210 // Start

// S1部分：101~116
#define MODBUS_SLAVE_ADDR_WIFI_S1_START (MODBUS_SLAVE_ADDR_WIFI_START)                          // Start
#define MODBUS_SLAVE_ADDR_WIFI_S1_END (MODBUS_SLAVE_ADDR_WIFI_S1_START + NET_WIFI_S1_POINT - 1) //

// 除S1外其他 WIFI点位部分:117~117+19
#define MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START (MODBUS_SLAVE_ADDR_WIFI_S1_END + 1)                                                 // Start
#define MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_END (MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START + NET_WIFI_MAX_POINT - NET_WIFI_S1_POINT - 1) //

/**********************************************************************************/

/*公司产品条码的ASCII内容的代号-序号;配合绑定帧21000*/
/*便携*/
#define SN_TYPE_INV_AC200MAX         1
#define SN_TYPE_INV_AC200L           2  // AC200L/AC200MAXII共用编号2
#define SN_TYPE_INV_AC200MAXII       2
#define SN_TYPE_INV_AC300            3
#define SN_TYPE_INV_AC500            4
#define SN_TYPE_INV_AC70             5
#define SN_TYPE_INV_AP300            6
#define SN_TYPE_INV_PLP022           7
#define SN_TYPE_INV_ELITE200_V2      8
#define SN_TYPE_INV_RV5              9
#define SN_TYPE_INV_FP               10
#define SN_TYPE_INV_EL300            11
#define SN_TYPE_INV_EB3A             12
#define SN_TYPE_INV_AC60             13
#define SN_TYPE_INV_EB55             14
#define SN_TYPE_INV_EB70             15
#define SN_TYPE_INV_AC180T_SINGLE    16  // AC180T（单包）
#define SN_TYPE_INV_AC180T_DUAL      17  // AC180T（双包）
#define SN_TYPE_INV_EP500            18
#define SN_TYPE_INV_EP500PRO         19
#define SN_TYPE_INV_AC2A             20
#define SN_TYPE_INV_AC50B            21
#define SN_TYPE_INV_AC60P            22
#define SN_TYPE_INV_AC180            23
#define SN_TYPE_INV_AC200P           24
#define SN_TYPE_INV_AC240            25
#define SN_TYPE_INV_HANDSFREE1       26
#define SN_TYPE_INV_HANDSFREE2       27
#define SN_TYPE_INV_EL320            28
#define SN_TYPE_INV_EL400            29
#define SN_TYPE_INV_EL80V2           30
#define SN_TYPE_INV_EL100            31
#define SN_TYPE_INV_EL30V2           32
#define SN_TYPE_INV_AC200PL          33
#define SN_TYPE_INV_PR002            34
#define SN_TYPE_INV_AC45             35
#define SN_TYPE_INV_AC50P            36
#define SN_TYPE_INV_AC55             37
#define SN_TYPE_INV_AC70P            38
#define SN_TYPE_INV_AC240P           39
#define SN_TYPE_INV_AC180P           40
#define SN_TYPE_INV_AC2P             41
#define SN_TYPE_INV_PREMIUM_20C      42
#define SN_TYPE_INV_KW1000           43
#define SN_TYPE_INV_LFP700           44
#define SN_TYPE_INV_PR30V2           45
#define SN_TYPE_INV_PR100V2          46
#define SN_TYPE_INV_PR200V2          47
#define SN_TYPE_INV_AORA10           48
#define SN_TYPE_INV_AORA30P          49
#define SN_TYPE_INV_AORA30V2         50
#define SN_TYPE_INV_AORA_80          51
#define SN_TYPE_INV_AORA_100         52
#define SN_TYPE_INV_AORA100V2        53
#define SN_TYPE_INV_AORA200          54
#define SN_TYPE_INV_AORA320          55
#define SN_TYPE_INV_AP500            56    // 1-PLP-029-A
#define SN_TYPE_INV_POWER5           57    // 1-PLP-030-A
#define SN_TYPE_INV_HS5              58    // 1-RPS-016-A
#define SN_TYPE_INV_HS3              59    // 1-RPS-015-A
#define SN_TYPE_INV_HS2              60    // 1-RPS-014-A
#define SN_TYPE_INV_AORA400          61
#define SN_TYPE_INV_EL10             62
#define SN_TYPE_INV_EL100MINI        63    // PPS018
#define SN_TYPE_INV_EL30MINI         64    // PPS030
#define SN_TYPE_INV_EL200MINI        65    // PPS028
#define SN_TYPE_INV_AORA100MINI      66
#define SN_TYPE_INV_AORA30MINI       67
#define SN_TYPE_INV_AORA200MINI      68
#define SN_TYPE_INV_AP200            69
#define SN_TYPE_INV_AP300V2          70
#define SN_TYPE_INV_EL200V3          71
#define SN_TYPE_INV_EL250V3          72
#define SN_TYPE_INV_HYBRID3          73
#define SN_TYPE_INV_AORA200V3        74
#define SN_TYPE_INV_AORA250V3        75
#define SN_TYPE_INV_AORA300          76
#define SN_TYPE_INV_EL100V3          77
#define SN_TYPE_INV_AORA100V3        78
#define SN_TYPE_INV_PR150V3          79
#define SN_TYPE_INV_AORA150V3        80
#define SN_TYPE_INV_AORA200V2        81

/*户用*/ 
#define SN_TYPE_EP600    	1000//
#define SN_TYPE_EP760    	1001//
#define SN_TYPE_EP800    	1002//
#define SN_TYPE_EP900    	1003//
#define SN_TYPE_EP2000   	1004//
#define SN_TYPE_EP13K    	1005//
#define SN_TYPE_EP6K     	1006//
#define SN_TYPE_EP18K     	1007//
#define SN_TYPE_EP5K     	1008//
#define SN_TYPE_EP5K5     	1009//
#define SN_TYPE_AP12K     	1010//
#define SN_TYPE_AC2AC       1050
#define SN_TYPE_NPP004      1051
#define SN_TYPE_NPP005      1052

/*微逆*/
#define SN_TYPE_A80    		2000//
#define SN_TYPE_D100S    	2001//
#define SN_TYPE_A100_Sub1G  2002//
#define SN_TYPE_A100_WIFI   2003//
#define SN_TYPE_D100P       2004//
#define SN_TYPE_Will        2005//

/*配件杂项*/
#define SN_TYPE_S1   		3000//
#define SN_TYPE_AT1   		3001//
#define SN_TYPE_COMBOX      3002//
#define SN_TYPE_PBOX   		3003//
#define SN_TYPE_EBOX   		3004//
#define SN_TYPE_HMI           3005  // 房车hmi/Epad
#define SN_TYPE_PANEL         3006  // Epanel
#define SN_TYPE_DCHUB         3007  // DCHUB
#define SN_TYPE_ACHUB         3008  // ACHUB
#define SN_TYPE_SOLARX4K      3009  // D400S
#define SN_TYPE_CHARGER1      3010  // CHARGER1
#define SN_TYPE_CHARGER2      3011  // CHARGER2
#define SN_TYPE_PACK_BOX      3012  // Edock
#define SN_TYPE_BLE_HMI       3013  // BLE_HMI(冰箱宝断码屏)
#define SN_TYPE_IOT_INSIDE    3014  // 逆变器AP300内部IOT
#define SN_TYPE_SHELLY_METER  3015  // shelly电表
#define SN_TYPE_METER         3018	// 自研电表


/*电池包 */
#define SN_TYPE_B500          4000  
#define SN_TYPE_B500H         4001  
#define SN_TYPE_IB500         4002  
#define SN_TYPE_B1210         4003  //2
#define SN_TYPE_B4810         4004  //3
#define SN_TYPE_B300          4005  //4
#define SN_TYPE_B300K         4006  //5
#define SN_TYPE_B300S         4007  //6
#define SN_TYPE_B1232         4008  //7
#define SN_TYPE_LEADACID      4009  //8
#define SN_TYPE_LFP	          4010  //9
#define SN_TYPE_B500A         4012  
#define SN_TYPE_BC200         4013  
#define SN_TYPE_B230          4014  
#define SN_TYPE_B210          4015  
#define SN_TYPE_B500K         4016  
#define SN_TYPE_B900          4017  
#define SN_TYPE_HB500         4018
#define SN_TYPE_BC260         4019
#define SN_TYPE_EK900         4020
#define SN_TYPE_IB800JP       4021
#define SN_TYPE_B500PRO       4022
#define SN_TYPE_B300PRO       4023
#define SN_TYPE_IB800         4024
#define SN_TYPE_HB500S        4025
#define SN_TYPE_BH500E        4026
#define SN_TYPE_B4805         4027

#define SN_TYPE_COMMON_DEVICE                30000  // 通用设备(不区分电池、逆变器等差异)
#define SN_TYPE_COMMON_BATTERY               30001  // 通用电池
#define SN_TYPE_COMMON_INVERTER              30002  // 通用逆变器
#define SN_TYPE_COMMON_DC_CHARGER            30003  // 通用DC充电设备
#define SN_TYPE_COMMON_DC_DISCHARGER         30004  // 通用DC放电设备
#define SN_TYPE_COMMON_DEVICE_CAN            30005  // 通用设备(CAN,不区分电池、逆变器等差异)
#define SN_TYPE_COMMON_BATTERY_CAN           30006  // 通用电池(CAN)
#define SN_TYPE_COMMON_INVERTER_CAN          30007  // 通用逆变器(CAN)
#define SN_TYPE_COMMON_DC_CHARGER_CAN        30008  // 通用DC充电设备(CAN)
#define SN_TYPE_COMMON_DC_DISCHARGER_CAN     30009  // 通用DC放电设备(CAN)
#define SN_TYPE_COMMON_DEVICE_WIRELESS       30010  // 通用设备(无线,不区分电池、逆变器等差异)
#define SN_TYPE_COMMON_BATTERY_WIRELESS      30011  // 通用电池(无线)
#define SN_TYPE_COMMON_INVERTER_WIRELESS     30012  // 通用逆变器(无线)
#define SN_TYPE_COMMON_DC_CHARGER_WIRELESS   30013  // 通用DC充电设备(无线)
#define SN_TYPE_COMMON_DC_DISCHARGER_WIRELESS 30014 // 通用DC放电设备(无线)

/**********************************************************************************/

/*公司产品条码的ASCII内容*/
/*便携*/
#define SN_TYPE_INV_AC200MAX_ASCII       "AC200MAX"
#define SN_TYPE_INV_AC200L_ASCII         "AC200L"
#define SN_TYPE_INV_AC200MAXII_ASCII     "AC200MAXII"
#define SN_TYPE_INV_AC300_ASCII          "AC300"
#define SN_TYPE_INV_AC500_ASCII          "AC500"
#define SN_TYPE_INV_AC70_ASCII           "AC70"
#define SN_TYPE_INV_AP300_ASCII          "AP300"
#define SN_TYPE_INV_ELITE200_V2_ASCII    "Elite 200 V2"
#define SN_TYPE_INV_PR200V2_ASCII        "PR200V2"
#define SN_TYPE_INV_AORA200_ASCII        "AORA200"
#define SN_TYPE_INV_RV5_ASCII            "RV5"
#define SN_TYPE_FP_ASCII                 "FP"
#define SN_TYPE_INV_EB3A_ASCII           "EB3A"
#define SN_TYPE_INV_AC60_ASCII           "AC60"
#define SN_TYPE_INV_EB55_ASCII           "EB55"
#define SN_TYPE_INV_EB70_ASCII           "EB70"
#define SN_TYPE_INV_AC180T_SINGLE_ASCII  "AC180T-S"  // 单包
#define SN_TYPE_INV_AC180T_DUAL_ASCII    "AC180T-D"  // 双包
#define SN_TYPE_INV_EP500_ASCII          "EP500"
#define SN_TYPE_INV_EP500PRO_ASCII       "EP500Pro"
#define SN_TYPE_INV_AC2A_ASCII           "AC2A"
#define SN_TYPE_INV_AC50B_ASCII          "AC50B"
#define SN_TYPE_INV_AC60P_ASCII          "AC60P"
#define SN_TYPE_INV_AC180_ASCII          "AC180"
#define SN_TYPE_INV_AC200P_ASCII         "AC200P"
#define SN_TYPE_INV_AC240_ASCII          "AC240"
#define SN_TYPE_INV_HANDSFREE1_ASCII     "Handsfree1"
#define SN_TYPE_INV_HANDSFREE2_ASCII     "Handsfree2"
#define SN_TYPE_INV_EL300_ASCII          "EL300"
#define SN_TYPE_INV_AORA300_ASCII        "AORA300"
#define SN_TYPE_INV_EL300P_ASCII         "EL300P"
#define SN_TYPE_INV_EL320_ASCII          "EL320"
#define SN_TYPE_INV_AORA320_ASCII        "AORA320"
#define SN_TYPE_INV_EL400_ASCII          "EL400"
#define SN_TYPE_INV_AORA400_ASCII        "AORA400"
#define SN_TYPE_INV_EL80V2_ASCII         "EL80V2"
#define SN_TYPE_INV_EL100V2_ASCII        "EL100V2"
#define SN_TYPE_INV_PR100V2_ASCII        "PR100V2"
#define SN_TYPE_INV_AORA100V2_ASCII      "AORA100V2"
#define SN_TYPE_INV_EL30V2_ASCII         "EL30V2"
#define SN_TYPE_INV_PR30V2_ASCII         "PR30V2"
#define SN_TYPE_INV_AORA30V2_ASCII       "AORA30V2"
#define SN_TYPE_INV_AC200PL_ASCII        "AC200PL"
#define SN_TYPE_INV_PR002_ASCII          "PR002"
#define SN_TYPE_INV_AC45_ASCII           "AC45"
#define SN_TYPE_INV_AC50P_ASCII          "AC50P"
#define SN_TYPE_INV_AC55_ASCII           "AC55"
#define SN_TYPE_INV_AC70P_ASCII          "AC70P"
#define SN_TYPE_INV_AC240P_ASCII         "AC240P"
#define SN_TYPE_INV_AC180P_ASCII         "AC180P"
#define SN_TYPE_INV_AC2P_ASCII           "AC2P"
#define SN_TYPE_INV_PREMIUM_20C_ASCII    "Premium 20C"
#define SN_TYPE_INV_KW1000_ASCII         "KW1000"
#define SN_TYPE_INV_LFP700_ASCII         "LFP700"
#define SN_TYPE_INV_AORA10_ASCII         "AORA10"
#define SN_TYPE_INV_AORA30P_ASCII        "AORA30P"
#define SN_TYPE_INV_AORA_80_ASCII        "AORA 80"
#define SN_TYPE_INV_AORA_100_ASCII       "AORA 100"
#define SN_TYPE_INV_PLP022_ASCII         "PINa" 
#define SN_TYPE_INV_AP500_ASCII          "AP500"
#define SN_TYPE_INV_POWER5_ASCII         "PI5000EG"
#define SN_TYPE_INV_HS5_ASCII            "HS5"
#define SN_TYPE_INV_HS3_ASCII            "HS3"
#define SN_TYPE_INV_HS2_ASCII            "HS2"
#define SN_TYPE_INV_EL10_ASCII           "EL10"
#define SN_TYPE_INV_EL100MINI_ASCII      "EL100mini"
#define SN_TYPE_INV_EL30MINI_ASCII       "EL30mini"
#define SN_TYPE_INV_EL200MINI_ASCII      "EL200mini"
#define SN_TYPE_INV_AORA100MINI_ASCII    "AORA100mini"
#define SN_TYPE_INV_AORA30MINI_ASCII     "AORA30mini"
#define SN_TYPE_INV_AORA200MINI_ASCII    "AORA200mini"
#define SN_TYPE_INV_AP200_ASCII          "AP200"
#define SN_TYPE_INV_AP300V2_ASCII        "AP300V2"
#define SN_TYPE_INV_EL200V3_ASCII        "EL200V3"
#define SN_TYPE_INV_EL250V3_ASCII        "EL250V3"
#define SN_TYPE_INV_HYBRID3_ASCII        "PI3000HYB"
#define SN_TYPE_INV_AORA200V3_ASCII      "AORA200V3"
#define SN_TYPE_INV_AORA250V3_ASCII      "AORA250V3"
#define SN_TYPE_INV_EL100V3_ASCII        "EL100V3"
#define SN_TYPE_INV_PR150V3_ASCII        "PR150V3"
#define SN_TYPE_INV_AORA100V3_ASCII      "AORA100V3"
#define SN_TYPE_INV_AORA150V3_ASCII      "AORA150V3"
#define SN_TYPE_INV_AORA200V2_ASCII      "AORA200V2"

/* 户用 ASCII 定义 */
#define SN_TYPE_EP600_ASCII         "EP600"
#define SN_TYPE_EP760_ASCII         "EP760"
#define SN_TYPE_EP800_ASCII         "EP800"
#define SN_TYPE_EP900_ASCII         "EP900"
#define SN_TYPE_EP2000_ASCII        "EP2000"
#define SN_TYPE_EP13K_ASCII         "EP13K"
#define SN_TYPE_EP6K_ASCII          "EP6K"
#define SN_TYPE_EP18K_ASCII         "EP18K"
#define SN_TYPE_EP5K_ASCII          "EP5K"
#define SN_TYPE_EP5K5_ASCII         "EP5K5"
#define SN_TYPE_AP12K_ASCII         "AP12K"
#define SN_TYPE_NPP004_ASCII        "Balco500"
#define SN_TYPE_NPP005_ASCII        "Balco260"

/* 微逆 ASCII 定义 */
#define SN_TYPE_A80_ASCII           "A80"
#define SN_TYPE_D100S_ASCII         "D100S"
#define SN_TYPE_A100_Sub1G_ASCII    "A100S"
#define SN_TYPE_A100_WIFI_ASCII     "A80P"
#define SN_TYPE_D100P_ASCII         "D100P"
#define SN_TYPE_Will_ASCII          "WT300"

/* 配件杂项 ASCII 定义 */
#define SN_TYPE_S1_ASCII            "S1"
#define SN_TYPE_AT1_ASCII           "AT1"
#define SN_TYPE_COMBOX_ASCII        "COMBOX"
#define SN_TYPE_PBOX_ASCII          "PBOX"
#define SN_TYPE_EBOX_ASCII          "EBOX"
#define SN_TYPE_HMI_ASCII           "Epad"
#define SN_TYPE_PANEL_ASCII         "Epanel"
#define SN_TYPE_DCHUB_ASCII         "HD1"
#define SN_TYPE_ACHUB_ASCII         "HA1"
#define SN_TYPE_SOLARX4K_ASCII      "SolarX 4K"
#define SN_TYPE_CHARGER1_ASCII      "CHARGER 1"
#define SN_TYPE_CHARGER2_ASCII      "CHARGER 2"
#define SN_TYPE_PACK_BOX_ASCII      "Edock"
#define SN_TYPE_BLE_HMI_ASCII       "Display 1"
#define SN_TYPE_IOT_INSIDE_ASCII    "AP300_IOT"
#define SN_TYPE_SHELLY_METER_ASCII  "SHELLY_METER"
#define SN_TYPE_METER_ASCII         "SMeter"

/* 电池包 ASCII 定义 */
#define SN_TYPE_B500_ASCII          "B500"
#define SN_TYPE_B500H_ASCII         "B500H"
#define SN_TYPE_IB500_ASCII         "IB500"
#define SN_TYPE_B1210_ASCII         "B1210"
#define SN_TYPE_B4810_ASCII         "B4810"
#define SN_TYPE_B300_ASCII          "B300"
#define SN_TYPE_B300K_ASCII         "B300K"
#define SN_TYPE_B300S_ASCII         "B300S"
#define SN_TYPE_B1232_ASCII         "B1232"
#define SN_TYPE_LEADACID_ASCII      "LeadAcid"
#define SN_TYPE_LFP_ASCII           "LFP"
#define SN_TYPE_B500A_ASCII         "B500A"
#define SN_TYPE_BC200_ASCII         "BC200"
#define SN_TYPE_B230_ASCII          "B230"
#define SN_TYPE_B210_ASCII          "B210"
#define SN_TYPE_B500K_ASCII         "B500K"
#define SN_TYPE_B900_ASCII          "B900"
#define SN_TYPE_HB500_ASCII         "HB500"
#define SN_TYPE_BC260_ASCII         "BC260"
#define SN_TYPE_EK900_ASCII         "EK900"
#define SN_TYPE_IB800JP_ASCII       "IB800JP"
#define SN_TYPE_B500PRO_ASCII       "B500Pro"
#define SN_TYPE_B300PRO_ASCII       "B300Pro"
#define SN_TYPE_IB800_ASCII         "IB800"
#define SN_TYPE_HB500S_ASCII        "HB500S"
#define SN_TYPE_BH500E_ASCII        "BH500E"
#define SN_TYPE_B4805_ASCII         "B4805"

/*通用设备伪装ASCII*/
#define SN_TYPE_COMMON_DEVICE_ASCII        "BLUETTI00"
#define SN_TYPE_COMMON_BATTERY_ASCII       "BLUETTI01"
#define SN_TYPE_COMMON_INVERTER_ASCII      "BLUETTI02"
#define SN_TYPE_COMMON_DC_CHARGER_ASCII    "BLUETTI03"
#define SN_TYPE_COMMON_DC_DISCHARGER_ASCII "BLUETTI04"
#define SN_TYPE_COMMON_DEVICE_CAN_ASCII    "BLUETTI05"
#define SN_TYPE_COMMON_BATTERY_CAN_ASCII   "BLUETTI06"
#define SN_TYPE_COMMON_INVERTER_CAN_ASCII  "BLUETTI07"
#define SN_TYPE_COMMON_DCCHG_CAN_ASCII     "BLUETTI08"
#define SN_TYPE_COMMON_DCDCHG_CAN_ASCII    "BLUETTI09"
#define SN_TYPE_COMMON_DEV_WL_ASCII        "BLUETTI10"
#define SN_TYPE_COMMON_BAT_WL_ASCII        "BLUETTI11"
#define SN_TYPE_COMMON_INV_WL_ASCII        "BLUETTI12"
#define SN_TYPE_COMMON_DCCHG_WL_ASCII      "BLUETTI13"
#define SN_TYPE_COMMON_DCDCHG_WL_ASCII     "BLUETTI14"

/**********************************************************************************/

#define TYPE_SIZE 12 // 类型号大小

#define DCDC_MAX_NUM    1
#define INV_MAX_NUM     1
#define IOT_MAX_NUM     1 // 一个逆变器系统固定只有一个IOT

/*******************************************************************************************************/

/*
DCDC设备的INV/BAT协作模式
can总线上设备情况*/
#define CUSTOM_MODE 1
#define WITHOUT_CAN 2      // 无通讯模式
#define CAN_WITHOUT_INV 3  // 不接INV场景，识别到BAT CAN
#define CAN_WITH_OLD_INV 4 // AC300/AC200MAX不改协议,DCDC适配补充
#define CAN_WITH_NEW_INV 5 // INV新增协议，优先DCDC PV充电

// CAN总线上设备类型
enum
{
    CAN_DEV_IOT = 1, //
    CAN_DEV_INV,     //
    CAN_DEV_PACK,    //
    CAN_DEV_ACHUB,  //
    CAN_DEV_DCHUB,  //
    CAN_DEV_AT1,     //
    CAN_DEV_EMS,     //
    CAN_DEV_DCDC,    //
    CAN_DEV_PAYGO,
    CAN_DEV_MIX,
};

// 定义主表宏，每行一个成员
#define CAN_POLL_TABLE(ENTRY) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_BASE_11H,              4,  SUPER_FAST_POLL_INTV,  DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_DATA_17H,              0,  COMMON_POLL_INTV,      DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_LINE_12H,              0,  0,                     DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_ABOUT_13H,             3,  COMMON_POLL_INTV,      DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_PV_14H,                0,  COMMON_POLL_INTV,      DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_GRID_15H,              0,  COMMON_POLL_INTV,      DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_LOAD_16H,              0,  COMMON_POLL_INTV,      DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_METER_18H,             0,  0,                     DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_GEN_19H,               0,  0,                     DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_CONFIG00_1AH,          5,  COMMON_POLL_INTV,      DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_CONFIG01_1BH,          5,  COMMON_POLL_INTV,      DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_CONFIG03_1DH,          0,  SLOW_POLL_INTV,        DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_WIFI_REPORT_24H,       0,  0,                     DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_INV,  INV_TYPE_WIFI_PARAM_25H,        0,  0,                     DEV_MAIN_NODE_MAX, INV_MAX_NUM) \
    ENTRY(CAN_DEV_MIX,  COMMON_TYPE_TRANSMIT_27H,       0,  0,                     DEV_MAIN_NODE_MAX, (INV_MAX_NUM + PACK_MAX_NUM)) \
    ENTRY(CAN_DEV_PACK, PACK_TYPE_BASE_51H,             0,  FAST_POLL_INTV,        PACK_GROUP_NUM,    PACK_INGROUP_MAX_NUM) \
    ENTRY(CAN_DEV_PACK, PACK_TYPE_EXTEND_52H,           0,  SLOW_POLL_INTV,        PACK_GROUP_NUM,    PACK_INGROUP_MAX_NUM) \
    ENTRY(CAN_DEV_PACK, PACK_TYPE_ABOUT_54H,            0,  COMMON_POLL_INTV,      PACK_GROUP_NUM,    PACK_INGROUP_MAX_NUM) \
    ENTRY(CAN_DEV_PACK, PACK_TYPE_CONFIG_55H,           0,  COMMON_POLL_INTV,      PACK_GROUP_NUM,    PACK_INGROUP_MAX_NUM) \
    ENTRY(CAN_DEV_PACK, PACK_TYPE_DEBUG1_56H,           0,  SLOW_POLL_INTV,        PACK_GROUP_NUM,    PACK_INGROUP_MAX_NUM) \
    ENTRY(CAN_DEV_PAYGO,PACK_TYPE_BMS_PAYGO_INFO_5BH,   0,  COMMON_POLL_INTV,      PACK_GROUP_NUM,    PACK_INGROUP_MAX_NUM) \
    ENTRY(CAN_DEV_IOT,  IOT_TYPE_INFO_01H,              0,  SLOW_POLL_IOT,         DEV_MAIN_NODE_MAX, IOT_MAX_NUM) \
    ENTRY(CAN_DEV_IOT,  IOT_TYPE_SET_02H,               0,  SLOW_POLL_IOT,         DEV_MAIN_NODE_MAX, IOT_MAX_NUM) 

// 自动生成枚举
#define ENTRY_ENUM(dev, type, range, period, max_x, max_y) INDEX_##type,
typedef enum {
    CAN_POLL_TABLE(ENTRY_ENUM)
    MAX_NUM_CAN_TYPE
} CAN_POLL_INDEX_E;
#undef ENTRY_ENUM

#define STR_EQ_EXACT(a,b) (strncmp((a),(b), strlen(b)) == 0 && strlen(a) == strlen(b))

/* 返回计算后的 0..UINT16_MAX 值，作为外部返回值使用 */
#define COUNTER_DELTA_U16(curr, last) ({ \
    uint32_t _c = (uint32_t)(curr); \
    uint32_t _l = (uint32_t)(last); \
    uint32_t _d = _c - _l; /* 无符号回绕安全 */ \
    (uint16_t)((_d > UINT16_MAX) ? UINT16_MAX : _d); \
})

enum
{
    IOT = 0,
    INV_ARM = 1,
    INV_DSP = 2,
    BMS = 3,
    BA = 4,
    PACK_BCU = 5,
    PACK_BMU = 6,
    PACK_BMS = 7,
    PACK_M1 = 8,
    PACK_SAFT = 9,
    PACK_HV = 10,
    IOT_LCD = 11,
    IOT_LCD2 = 12,
    RF = 13,
    PLUG = 14, // windy add tbd, 需要WIFI MESH OTA的对象，S1
};

typedef enum
{
    CHANNEL_BLE = 0,
    CHANNEL_WIFI_STA,
    CHANNEL_WIFI_AP,
    CHANNEL_UART,
    CHANNEL_UNKOWN = 0xFF,
}channel_type;

typedef struct {
	uint8_t channel;
	uint16_t msg_id;
	uint16_t dev_type;  // 消息来源的设备类型
	uint64_t dev_sn;	// 消息来源的设备序列号
	uint16_t regAddr;   // 请求的Modbus地址
	uint16_t regNum;	// 请求的Modbus寄存器数量
    uint8_t funcode;    // Modbus功能码
	uint8_t slaveAddr;
	union 
	{
		//channel:MD_CHL_WIFI_CLOUD
		struct 
		{
			uint32_t ip;
			uint16_t port;// 
		};
        struct 
		{
			uint8_t protocol;
			uint8_t cause;
			uint16_t cycle;
		};
		//channel:其他通道有特殊值在下面再定对应的结构体
	};	
}modbus_addr_info_t;


// 顺序禁止更改
enum
{
    NETIF_TYPE_MIN = 0,
    NETIF_TYPE_ETH = NETIF_TYPE_MIN,
    NETIF_TYPE_WIFI_STA,
    NETIF_TYPE_PPP,
    NETIF_TYPE_WIFI_AP,
    NETIF_TYPE_MAX,
};

enum
{	
	OTA_STATE_IDEL =0,
	OTA_STATE_START = 1,
	OTA_STATE_COMPLETE = 2,
};

enum {
    METER_MODEL_MDNS = 1, // 通过mDNS扫描到的电表
    METER_MODEL_WLCC = 2, // 通过WLCC扫描到的电表
    PLUG_MODEL_WLCC = 3,  // 通过WLCC扫描到的插座
};

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define GET_CURRENT_TIME_MS_B() (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS)

#if USE_PINNED_CORE
#define CREATE_TASK(task_func, name, stack, param, prio, handle, core) \
    xTaskCreatePinnedToCore(task_func, name, stack, param, prio, handle, core)
#else
#define CREATE_TASK(task_func, name, stack, param, prio, handle, core) \
    xTaskCreate(task_func, name, stack, param, prio, handle)
#endif

