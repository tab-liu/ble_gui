/**
  ******************************************************************************
  * @file      modbus_tlv.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/4/15
  * @brief     Modbus TLV寄存器组合（借用21000段框架）
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/4/15  <td>1.0     <td>lixingyu   <td>Create the initial version
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

#include "utils.h"
#include "crc.h"

/* ================================ 头文件宏定义 ================================ */

// Modbus寄存器单次操作最大长度
#define MODBUS_REG_SINGLE_PROCESS_LEN_MAX  127

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

// 21000 协议版本：用于区分未来不同差异需求
enum {
    MODBUS_21000_FUNC_TLV_DATA = 40004,         // 40004-TLV寄存器组合 上报信息（信息单元对象包括TLV）；    
    MODBUS_21000_FUNC_TLV_READ_CMD = 40005,     // 40005-查询(读取)命令（信息单元对象包括TL）;
};

/**
 * 设备 X - 信息单元 y 的数据结构（按图示字段映射）
 *
 * 字节序/对齐：按需可加 __attribute__((packed))
 */
typedef struct {
    uint8_t  slave_addr;   /* 低字节: 从机地址 0..255 */
    uint8_t  reserved;     /* 高字节: 预留 */
    uint16_t reg_addr;     /* 地址 (T)，1..65535 (Modbus 寄存器地址) */
    uint16_t len;          /* 长度 (L)，单位：字节，范围 0..255（使用 uint16_t 保留空间） */
    uint8_t *value;        /* 内容 (V)，len 字节数据指针 */
} modbus_tlv_unit_t;

/**
 * 队列消息结构（用于队列内传递 TLV 消息）
 *
 * 语义：
 *  - data: 指向动态分配的缓冲区，长度为 len 字节；
 *  - len : 有效数据长度；
 *  - chl : 通道/来源标识（由业务定义，1 字节）；
 *
 * 所有权约定：
 *  - 入队时，调用者将 data 的所有权转移给队列（队列存储指针副本，不复制数据）；
 *  - 出队后，接收方负责释放 data（使用 project 中约定的 iot_free 或 free）。
 */
typedef struct {
    uint8_t  *data;
    uint16_t  len;
    uint8_t   chl;
} queue_msg_tlv_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 modbus TLV 接收队列
 * @param capacity 最大队列长度（0 使用默认16）
 * @return void
 */
void modbus_tlv_queue_init(size_t capacity);

/**
 * @brief 将一条 TLV 消息入队（队列保存 data 指针所有权）
 * @param pdata 消息数据指针（入队前由调用者提供）
 * @param len   数据长度（字节）
 * @param chl   通道标识
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数错误；ESP_ERR_NO_MEM 队列满或分配失败；ESP_ERR_INVALID_STATE 队列未初始化
 */
esp_err_t modbus_tlv_queue_push(const uint8_t *pdata, uint16_t len, uint8_t chl);

/**
 * @brief 处理 modbus TLV 接收队列中的消息（非阻塞，处理队列内所有待处理项）
 *
 * 行为与约定：
 *  - 从全局队列 xQueue_modbus_tlv_rx 非阻塞取出所有消息并处理；
 *  - 入队时队列取得 data 指针所有权，出队后此函数负责释放（使用 iot_free）；
 *  - 对未知或不合法消息会记录警告并释放资源；
 */
void modbus_tlv_process_task(void);

#ifdef __cplusplus
}
#endif
