/**
  ******************************************************************************
  * @file      mini_tlv_struct.h
  * @version   1.0
  * @author
  * @date      2026/7/9
  * @brief     小TLV报文结构定义
  *            每个小TLV块由 Table(1Byte) + Length(2Byte) + Value 组成。
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/7/9   <td>1.0     <td>           <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================ 头文件宏定义 ================================ */

#pragma pack(1)

// 小TLV的Table编号
typedef enum {
    MINI_TLV_TABLE_SMALL_COUNT = 0x01, // 01，小TLV数量（Value为1字节计数值）
    MINI_TLV_TABLE_DEV_SN      = 0x02, // 02，设备SN
    MINI_TLV_TABLE_TIMESTAMP   = 0x03, // 03，时间戳（1970年秒）
    MINI_TLV_TABLE_MODBUS_BETA = 0xFA, // 250，modbus beta协议内容
} mini_tlv_table_e;

// 小TLV通用头（Table + Length）
typedef struct {
    uint8_t  table;     // 1字节：Table编号，见 mini_tlv_table_e
    uint16_t length;    // 2字节：Value长度
} mini_tlv_head_t;

/* ------------------------------ Table 01：小TLV数量 ------------------------------ */

// Table 01 的 Value（1字节计数值）
typedef struct {
    uint8_t count;      // 1字节：后续小TLV块的数量
} mini_tlv_small_count_t;

/* ------------------------------ Table 02：设备SN ------------------------------ */

// 厂商类型（设备SN首字节）
typedef enum {
    DEV_VENDOR_UNDEF   = 0,   // 未定义
    DEV_VENDOR_BLUETTI = 200, // BLUETTI（我司设备）
    DEV_VENDOR_THIRD   = 201, // 第三方通用（预留）
    DEV_VENDOR_MESH_ID = 255, // MESH ID（仅我司设备格式使用）
} dev_vendor_type_e;

// 我司设备SN（1+10字节）
// 厂商类型为 BLUETTI(200) / MESH ID(255) / 未定义(0) 时使用本结构
typedef struct {
    union {
        uint8_t raw[10];        // 10字节：整体SN
        struct {
            uint64_t sn_src;    // 8字节：SN_源设备
            uint16_t model;     // 2字节：压缩机型序号
        };
    } sn;
} dev_sn_bluetti_t;

// 第三方设备SN（1+1+32字节）
// 厂商类型为 第三方通用(201) 时使用本结构
typedef struct {
    uint8_t accessory_type;     // 1字节：配件系列/具体型号（各第三方厂商自定义枚举编号）
    char    sn_ascii[32];       // 32字节：第三方设备完整SN ASCII码
} dev_sn_thirdparty_t;

// Table 02 的 Value（设备SN，根据厂商类型选择对应分支）
// 注：union 取最大成员对齐，实际有效长度由 mini_tlv_head_t.length 决定
typedef struct {
    uint8_t vendor_type;        // 1字节：厂商类型，见 dev_vendor_type_e
    union {
        dev_sn_bluetti_t    bluetti;    // 我司设备：vendor_type = 200 / 255 / 0
        dev_sn_thirdparty_t thirdparty; // 第三方设备：vendor_type = 201
    } dev;
} mini_tlv_dev_sn_t;

/* ------------------------------ Table 03：时间戳 ------------------------------ */

// Table 03 的 Value（4字节）
typedef struct {
    uint32_t timestamp;  // 4字节：1970年起的秒数（大端字节序）
} mini_tlv_timestamp_t;

/* ------------------------------ Table 250：modbus beta ------------------------------ */

// Table 250 的 Value（变长，只允许一个TLV块）
typedef struct {
    uint16_t reg_addr;       // 2字节：寄存器地址（大端字节序）
    uint16_t reg_number;     // 2字节：寄存器数量 yy（大端字节序）
    uint16_t reg_value[];    // yy*2字节：寄存器值（变长，大端字节序）
} mini_tlv_modbus_beta_t;

#pragma pack()

/* ================================ C++兼容 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
