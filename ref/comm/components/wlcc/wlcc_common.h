#ifndef WLCC_COMMON_H
#define WLCC_COMMON_H

#include "comm_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Definitions for error constants. */
#define IOT_OK          0       /*!< iot_err_t value indicating success (no error) */
#define IOT_FAIL        -1      /*!< Generic iot_err_t code indicating failure */
/* 系统级错误 */
#define IOT_ERR_NO_MEM                          0x101   /*!< Out of memory */
#define IOT_ERR_INVALID_ARG                     0x102   /*!< Invalid argument */
#define IOT_ERR_INVALID_STATE                   0x103   /*!< Invalid state */
#define IOT_ERR_INVALID_SIZE                    0x104   /*!< Invalid size */
#define IOT_ERR_NOT_FOUND                       0x105   /*!< Requested resource not found */
#define IOT_ERR_NOT_SUPPORTED                   0x106   /*!< Operation or feature not supported */
#define IOT_ERR_TIMEOUT                         0x107   /*!< Operation timed out */
#define IOT_ERR_INVALID_RIOTONSE                0x108   /*!< Received rIOTonse was invalid */
#define IOT_ERR_INVALID_CRC                     0x109   /*!< CRC or checksum was invalid */
#define IOT_ERR_INVALID_VERSION                 0x10A   /*!< Version was invalid */
#define IOT_ERR_INVALID_MAC                     0x10B   /*!< MAC address was invalid */
#define IOT_ERR_NOT_FINISHED                    0x10C   /*!< Operation has not fully completed */
#define IOT_ERR_NOT_ALLOWED                     0x10D   /*!< Operation is not allowed */

#define IOT_ERR_WLCC_BASE                       0x1000  /*!< Starting number of WLCC error codes */
/* 模块内部错误 */
#define IOT_ERR_WLCC_NOT_INIT                   (IOT_ERR_WLCC_BASE + 1)   /*!< WLCC driver was not installed by IOT_WLCC_init */
#define IOT_ERR_WLCC_NOT_STARTED                (IOT_ERR_WLCC_BASE + 2)   /*!< WLCC driver was not started by IOT_WLCC_start */
#define IOT_ERR_WLCC_NOT_STOPPED                (IOT_ERR_WLCC_BASE + 3)   /*!< WLCC driver was not stopped by IOT_WLCC_stop */
#define IOT_ERR_WLCC_IF                         (IOT_ERR_WLCC_BASE + 4)   /*!< WLCC interface error */
#define IOT_ERR_WLCC_IP                         (IOT_ERR_WLCC_BASE + 5)   /*!< ip address error */
#define IOT_ERR_WLCC_SOCKET_SETOPT_FAILED       (IOT_ERR_WLCC_BASE + 6)   /*!< failed to set/get socket option */
#define IOT_ERR_WLCC_GET_IF_IP                  (IOT_ERR_WLCC_BASE + 7)   /*!< WLCC internal control block of station or soft-AP error */
#define IOT_ERR_WLCC_FORMAT                     (IOT_ERR_WLCC_BASE + 8)   /*!< protocol frame format error */
#define IOT_ERR_WLCC_SEND                       (IOT_ERR_WLCC_BASE + 9)   /*!< sendto error */
#define IOT_ERR_WLCC_TARGET                     (IOT_ERR_WLCC_BASE + 10)  /*!< not own msg, ignor */

/* 组播和单播通信使用同一个 udp socket, 根据目的IP地址决定使用单播还是组播 */
#define USR_SINGLE_PORT         1

#define MULTICAST_IPV4_ADDR     "239.0.0.238"   // UDP组播地址
#define UDP_PORT                5000            // UDP组播端口
#define UDP_PORT_SINGLE         1234            // UDP单播端口
#define MULTICAST_TTL           1

/* 设备优先级定义<> */
#define DEV_PRIORITY_INVALID        0 //0-无效
#define DEV_PRIORITY_AT1            30300 //AT1(ATS):30300
#define DEV_PRIORITY_EMS            30200 //EMS:30200
#define DEV_PRIORITY_COMBOX         30100 //COMBOX:30100
#define DEV_PRIORITY_CMM_INV        30050 //普通储能逆变器，通用
#define DEV_PRIORITY_D100S          30000 //D100S:30000
#define DEV_PRIORITY_A100           20900 //A80/A100:20900
#define DEV_PRIORITY                65535 //超级强制主设备-65535（只允许特殊短暂发送）

#define DEV_PRIORITY_SELF           DEV_PRIORITY_CMM_INV

#define WLCC_FRAME_VERSION_WIFI         1
#define WLCC_FRAME_VERSION_SUB1GHZ      2

/**
 * @brief 无线设备间通讯协议帧定义
 */
#define WLCC_FRAME_ADDR_HEAD            0   /*!< 协议帧头 */
#define WLCC_FRAME_ADDR_VER             1   /*!< 协议版本 */
#define WLCC_FRAME_ADDR_MSG_ID          2   /*!< 消息ID */
#define WLCC_FRAME_ADDR_SN_SOURCE       4   /*!< 源设备SN */
#define WLCC_FRAME_ADDR_TYPE_SOURCE     12  /*!< 源设备型号 */
#define WLCC_FRAME_ADDR_SN_TARGET       14  /*!< 目标设备SN */
#define WLCC_FRAME_ADDR_TYPE_TARGET     22  /*!< 目标设备型号 */
#define WLCC_FRAME_ADDR_TYPE            24  /*!< 功能码 */
#define WLCC_FRAME_ADDR_TTL             25  /*!< 消息生存时间 */
#define WLCC_FRAME_ADDR_PRIORITY        27  /*!< 源设备优先级 */
#define WLCC_FRAME_ADDR_SEQ        		29  /*!< 并机序号 */
#define WLCC_FRAME_ADDR_IP_LEVEL        30  /*!< 路由层级数 */
#define WLCC_FRAME_ADDR_IP_BEGIN        31  /*!< 路由IP */

/* 设备发现 */
#define WLCC_FRAME_ADDR_IP              31  /*!< 源设备IP地址 */
#define WLCC_FRAME_ADDR_PORT            35  /*!< 源设备端口 */

/* 通用协议-WIFI */
#define WLCC_FRAME_ADDR_MODBUS_HEAD     25  /*!< Modbus报文起始 */

#define WLCC_FRAME_HEADER_COMMON        0   /*!< 默认的普通无线设备间报文 */
#define WLCC_FRAME_HEADER_TRIGER        1   /*!< 1-设备发现触发帧（组播,STEP1） */
#define WLCC_FRAME_HEADER_SEND_SN       2   /*!< 2-设备发现SN信息上报帧（组播,STEP2） */
#define WLCC_FRAME_HEADER_FINISH        3   /*!< 3-设备发现完成帧（组播,STEP3） */
#define WLCC_FRAME_HEADER_FINISH_AFTER  4   /*!< 设备发现完成后，进入定时周期上报模式，上报自身SN */
/* 功能码 */
#define WLCC_FRAME_TYPE_READ            1 /*!< 1-读取 */
#define WLCC_FRAME_TYPE_WRITE           2 /*!< 2-写入 */
#define WLCC_FRAME_TYPE_READ_RTN        3 /*!< 3-读取-RTN */
#define WLCC_FRAME_TYPE_WRITE_RTN       4 /*!< 4-写入-RTN */
#define WLCC_FRAME_TYPE_PERIOD          5 /*!< 5-周期上报（无RTN），固定使用0x10多字节写 */
#define WLCC_FRAME_TYPE_HEART           6 /*!< 6-周期上报（心跳） */

#define WLCC_FRAME_TYPE_FILE_CONTEXT    0x24 /*!< 数据通道透传协议: 0x24-设备（存储方）回复文件内容(功能码53) */

/* TLV格式协议版本定义 */
#define TLV_PROTOCOL_VERSION_REPORT     40002  /*!< TLV寄存器组合 上报信息（信息单元对象包括TLV） */
#define TLV_PROTOCOL_VERSION_QUERY      40003  /*!< 查询(读取)命令（信息单元对象包括TL） */
#define WLCC_21000_PROTO_VER_PARAM_SYNC 40004 /*!< 21000段协议版本为参数同步帧 */

#define WLCC_FRAME_TYPE_XMODEM          7 /*!< 7-上级发xmodem升级相关 */
#define WLCC_FRAME_TYPE_XMODEM_RTN      8 /*!< 8-下级响应xmodem升级相关 */

enum{
    WLCC_STATUS_OFFLINE,        /*< 设备长期掉线,>5minute */
    WLCC_STATUS_OFFLINE_HAL,    /*< 设备短期通讯超时, >30s */
    WLCC_STATUS_ONLINE          /*< 设备可靠在线,<30s */
};

enum {
    WLCC_EMS_MODE_INVALID = 0,  		/**< 无效 */
    WLCC_EMS_MODE_MASTER = 1,	/**< 并机模式主机 */
    WLCC_EMS_MODE_SLAVER = 2,	/**< 并机模式从机 */
};


/**
 * @brief Network interface types
 */
typedef enum
{
    NETIF_WIFI_STA = 0, /**< WiFi Station */
    NETIF_WIFI_AP,      /**< WiFi Access Point */
} netif_type_t;

typedef struct {
	uint16_t msg_id;
	uint16_t dev_type;  // 消息来源的设备类型
	uint64_t dev_sn;	// 消息来源的设备序列号
	uint16_t dev_priority;
	uint16_t port;
	uint32_t ip;
}wlcc_dev_info_t;


#ifdef __cplusplus
}
#endif

#endif
