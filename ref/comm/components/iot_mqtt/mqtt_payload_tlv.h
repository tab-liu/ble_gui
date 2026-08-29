/**
  ******************************************************************************
  * @file      mqtt_payload_tlv.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/1/14
  * @brief     MQTT载荷中传输TLV报文格式
  *            参考文档https://192.168.102.10/svn/TecCenter/文件中心/技术资料/德兰明海总线协议/物联网载荷协议/IoT物联网载荷协议 - TLV.xlsx
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/14  <td>1.0     <td>lixingyu   <td>Create the initial version
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

// MQTT载荷中传输TLV报文标志
#define MQTT_TLV_MARK           0xAA

// 表示扩展的V1.2版载荷协议，且为Tag编号模式的TLV协议数据
#define MQTT_TLV_TAG_FLAG       0xF8

// 表示扩展的V1.2版载荷协议，且为Field字段名模式的TLV协议数据
#define MQTT_TLV_FIELD_FLAG     0xF9

// http API接口ID
#define MQTT_TLV_HTTP_API1      2703286 // 数据上报
#define MQTT_TLV_HTTP_API2      2712978 // 校时
#define MQTT_TLV_HTTP_API3      2925133 // 服务器保存参数(Set)
#define MQTT_TLV_HTTP_API4      2925134 // 从服务器读取参数(Get)
#define MQTT_TLV_HTTP_API5      2944990 // 更新设备经纬度数据

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

// 定义主动上报数据结构体
typedef struct {
    uint8_t *data;      // 指向待上报的数据缓冲区
    uint16_t data_len;  // 数据长度
} mqtt_tlv_report_data_t;

// byte[2] 数据交互场景码（继承载荷协议的场景码定义），此处仅列举部分
enum {
    MQTT_FUNC_PERIOD = 1,           // 周期性上报设备数据，仅此场景的数据才支持解码中心服务的第三极应急备份消息队列
    MQTT_FUNC_EVEVT = 2,            // 关键指标数据上传下发或控制状态更新
    MQTT_FUNC_SET_AND_RTN = 3,      // 设备的设置指令下发、设置结果上报
    MQTT_FUNC_HTTP_REQUEST = 0x10,  // 模拟HTTP API接口请求上报数据（此场景下，API接口请求方式由byte[7]的bit7位、bit6位控制，报体的第一个TLV报文单元TLV0为API接口ID标识，TLV1开始才算是真正的数据。）
};

// 数据/业务操作类型（Bit 7~6）
typedef enum {
    MQTT_TLV_OP_NONE   = 0, // 00，未定义/无操作
    MQTT_TLV_OP_SET    = 1, // 01，Set：新增、更新数据
    MQTT_TLV_OP_GET    = 2, // 10，Get：查询数据
    MQTT_TLV_OP_DELETE = 3  // 11，Delete：删除数据
} MQTT_TLV_OP_TYPE;

// 数据/业务交互的响应类型（Bit 5~4）
typedef enum {
    MQTT_TLV_RESP_NONE    = 0, // 00，未定义/保留
    MQTT_TLV_RESP_VOID    = 1, // 01，不要求对端响应（void方法）
    MQTT_TLV_RESP_ASYNC   = 2, // 10，异步请求
    MQTT_TLV_RESP_SYNC    = 3  // 11，同步请求
} MQTT_TLV_RESP_TYPE;

// 数据压缩方式（Bit 3~2）
typedef enum {
    MQTT_TLV_COMPRESS_NONE    = 0, // 00，不启用数据压缩
    MQTT_TLV_COMPRESS_GZIP    = 1, // 01，使用GZIP压缩数据
    MQTT_TLV_COMPRESS_REVD2   = 2, // 10，暂时保留
    MQTT_TLV_COMPRESS_REVD3   = 3  // 11，暂时保留
} MQTT_TLV_COMPRESS;

// TLV的Tag编号的字节长度（Bit 1~0）
typedef enum {
    MQTT_TLV_TAG_NONE    = 0, // 00，未定义/保留
    MQTT_TLV_TAG_1BYTE   = 1, // 01，1个字节长（uint8）。Field字段名模式时固定为uint8
    MQTT_TLV_TAG_2BYTE   = 2, // 10，2个字节长（uint16）
    MQTT_TLV_TAG_4BYTE   = 3  // 11，4个字节长（uint32）
} MQTT_TLV_TAG_TYPE;

/*
byte[7]
数据/业务类型
*/
typedef union {
    uint8_t value;
    struct {
        uint8_t tag_type  : 2; // Bit 0~1，TLV的Tag编号的字节长度【根据2026-01-20评审会议的沟通，开发十四部维护全局不重复的Tag编号。但针对类似4G这类对流量有要求的项目，允许设置更短的Tag存储长度】
        uint8_t compress  : 2; // Bit 2~3，数据压缩方式（仅压缩报体[body]，即byte[11] ~ byte[n-2]，不可以压缩报头[header]和最后两字节的CRC校验值）
        uint8_t resp_type : 2; // Bit 4~5，数据/业务交互的响应类型
        uint8_t op_type   : 2; // Bit 6~7，数据/业务操作类型
    };
} mqtt_tlv_op_flag_t;

// TLV数据类型具体定义
typedef union {
    uint8_t value;
    struct {
        uint8_t encode_type  : 4; // bit0~3，数据编码类型
        uint8_t logic_type   : 3; // bit4~6，数据逻辑类型
        uint8_t data_type    : 1; // bit7，数据类型（0=原始类型，1=变长类型）
    };
} tlv_type_t;

// 原始类型（data_type = 0）时，encode_type的枚举
typedef enum {
    TLV_ENCODE_BOOL      = 0x01, // 0001 布尔型(bool)
    TLV_ENCODE_TINY      = 0x02, // 0010 有符号字节(tiny)
    TLV_ENCODE_UTINY     = 0x03, // 0011 无符号字节(utiny)
    TLV_ENCODE_SHORT     = 0x04, // 0100 有符号短整型(short)
    TLV_ENCODE_USHORT    = 0x05, // 0101 无符号短整型(ushort)
    TLV_ENCODE_INT       = 0x06, // 0110 有符号整型(int)
    TLV_ENCODE_UINT      = 0x07, // 0111 无符号整型(uint)
    TLV_ENCODE_LONG      = 0x08, // 1000 有符号长整型(long)
    TLV_ENCODE_ULONG     = 0x09, // 1001 无符号长整型(ulong)
    TLV_ENCODE_FLOAT     = 0x0A, // 1010 单精度浮点(float)
    TLV_ENCODE_DOUBLE    = 0x0B  // 1011 双精度浮点(double)
} TLV_EncodeType_Fixed_E;

// 变长类型（data_type = 1）时，encode_type的枚举
typedef enum {
    TLV_ENCODE_ASCII_STRING = 0x01, // 0001 ASCII字符串
    TLV_ENCODE_UTF8_STRING  = 0x02, // 0010 UTF8-mb4字符串
    TLV_ENCODE_HEX_STRING   = 0x0F  // 1111 Hex字符串
} TLV_EncodeType_Var_E;

// logic_type的枚举
typedef enum {
    TLV_LOGIC_EQ   = 0x0, // 000 等于 (eq)         =
    TLV_LOGIC_NE   = 0x1, // 001 不等于 (ne)      !=
    TLV_LOGIC_LT   = 0x2, // 010 小于 (lt)        <
    TLV_LOGIC_LTE  = 0x3, // 011 小于等于 (lte)   <=
    TLV_LOGIC_GT   = 0x4, // 100 大于 (gt)        >
    TLV_LOGIC_GTE  = 0x5, // 101 大于等于 (gte)   >=
    TLV_LOGIC_NULL = 0x6, // 110 是null           is null
    TLV_LOGIC_NNULL= 0x7  // 111 不是null         not null
} TLV_LogicType_E;

// MQTT TLV载荷协议报文结构体（大端字节序）
typedef struct {
    uint8_t  frame_type;      // byte[0]：0xAA，MQTT载荷TLV报文标识
    uint8_t  protocol_ver;    // byte[1]：协议版本
    uint8_t  scene;           // byte[2]：数据交互场景码
    uint16_t payload_len;     // byte[3]~[4]：载荷数据长度（不含CRC），高字节在前
    uint16_t revd;            // byte[5]~[6]：保留(Reserve)，置0x00
    mqtt_tlv_op_flag_t flag;  // byte[7]：操作类型+响应类型+保留位
    uint16_t request_id;      // byte[8]~[9]：请求标识（同步/异步请求时有效）
    uint8_t  slaveaddress;    // byte[10]：地址域
} mqtt_tlv_payload_t;

// 指定field的处理函数表
typedef struct {
    const char *key;
    bool (*handler)(const char *field, const tlv_type_t *tlv_tag, const void *value_data, uint16_t value_len);  // 自定义处理函数指针
} field_handler_t;

// 查询请求时缓存的请求ID和解析表格
typedef struct {
    uint16_t *request_id;
    field_handler_t *field_handler;
} field_request_t;

// MQTT TLFV 协议报文结构体
typedef struct {
    tlv_type_t type;
    uint8_t *field;
    uint8_t field_len;
    uint8_t *value;
    uint16_t value_len;
} mqtt_tlfv_frame_t;

// MQTT TLFV 参数更新结构体
typedef struct {
    char  *data_field;       /* Field字段，每个Field代表一个分组 */
    uint8_t *data_addr;      /* 参数地址指针 */
    uint16_t data_len;       /* 参数长度 */
} mqtt_tlfv_config_field_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief      MQTT TLV初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void mqtt_tlv_init(void);

/**
 * @brief MQTT TLV协议接口处理函数
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return 处理结果，0成功，-1失败
 */
int mqtt_tlv_protocol_handle(uint8_t *data, int size);

/**
 * @brief      MQTT TLV协议数据主动上报服务器
 * @param[in]  void  
 * @param[out] None
 * @return     uint8_t  1成功，0失败
 */
uint8_t mqtt_tlv_protocol_report_to_cloud(void);

/**
 * @brief      组帧并请求服务器域名及时间
 * @param[in] latitude   纬度
 * @param[in] longitude  经度
 * @param[out] None
 * @return 0成功，-1失败
 */
int mqtt_tlv_request_server_and_dst(int latitude, int longitude);

/**
 * @brief 组帧并上报4G物联网卡信息（IMEI、IMSI、ICCID）
 * @param[in] imei              IMEI字符串
 * @param[in] imsi              IMSI字符串
 * @param[in] iccid             ICCID字符串
 * @return 0成功，-1失败
 */
int mqtt_tlv_report_4g_info(const char *imei, const char *imsi, const char *iccid);

/**
 * @brief 组帧并上报设备经纬度数据
 * @param[in] latitude          纬度
 * @param[in] longitude         经度
 * @param[in] signalStrength    信号强度
 * @param[in] connectState4G    4G连接状态
 * @return 0成功，-1失败
 */
int mqtt_tlv_report_location_info(int latitude, int longitude, uint8_t signalStrength, bool connectState4G);

/**
 * @brief      组装设备参数配置更新请求并推送至队列
 * @param[in]  meshID     空间ID（优先使用），为NULL时使用sn
 * @param[in]  sn         设备序列号，meshID无效时使用
 * @param[in]  object     参数对象数组
 * @param[in]  object_cnt 参数对象个数
 * @param[out] buff       接收TLV协议帧的缓冲区
 * @param[in]  buff_size  缓冲区大小
 * @return     0成功，-1参数错误或queue发送失败，-2 HTTP_ID序列化失败，-3 meshId/sn错误，-4-i 第i个参数序列化失败
 * @note       使用此函数前需调用 mqtt_tlv_init() 初始化队列
 */
int mqtt_tlv_request_device_config_set(uint8_t *meshID, char *sn, mqtt_tlfv_config_field_t *object, 
                                         uint16_t object_cnt, uint8_t *buff, uint16_t buff_size);

/**
* @brief      组装设备参数配置获取请求并推送至队列
* @param[in]  meshID     空间ID（优先使用），为NULL时使用sn
* @param[in]  sn         设备序列号，meshID无效时使用
* @param[in]  object     指定参数对象数组（可选）
* @param[in]  object_cnt 参数对象个数（可选）
* @param[out] buff       接收TLV协议帧的缓冲区
* @param[in]  buff_size  缓冲区大小
* @return     0成功，-1参数错误或queue发送失败，-2 HTTP_ID序列化失败，-3 meshId/sn错误，-4-i 第i个参数序列化失败
* @note       使用此函数前需调用 mqtt_tlv_init() 初始化队列
*/
int mqtt_tlv_request_device_config_get(uint8_t *meshID, char *sn, mqtt_tlfv_config_field_t *object, 
                                        uint16_t object_cnt, uint8_t *buff, uint16_t buff_size);

#ifdef __cplusplus
}
#endif
