/**
  ******************************************************************************
  * @file      can_product.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/2/24
  * @brief     CAN标定模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/2/24  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "product_define.h"

/* ================================ 头文件宏定义 ================================ */

#define CAN_FACTORY_CMD 0xFF

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

typedef union {
    uint16_t raw;
    struct {
        uint16_t bt      :1; /* bit0 */
        uint16_t wifi    :1; /* bit1 */
        uint16_t _4g     :1; /* bit2 (4G) */
        uint16_t paygo   :1; /* bit3 */
        uint16_t matter  :1; /* bit4 */
        uint16_t reserved:11;/* bit5..15 */
    } bits;
} product_support_t;

typedef struct __attribute__((packed)) {
    uint16_t protocol_id;               // addr 0   (uint16)
    product_support_t product_support;  // addr 2   (bitmap)
    char     model[12];                 // addr 4-15 ASCII12
    uint64_t sn;                        // addr 16-23 SN
} device_identity_t;                    // total 24 bytes (0..23)

typedef struct __attribute__((packed)) {
    uint32_t arm_version;        // addr 24
    uint32_t dsp_version;        // addr 28
    uint32_t iot_version;        // addr 32
    uint32_t bms_version;        // addr 36
    uint32_t hmi_version;        // addr 40
    uint8_t  reserved_44_59[16]; // addr 44-59 保留
} firmware_info_t;               // 36 bytes (24..59)

typedef struct __attribute__((packed)) {
    uint64_t security;           // addr 60 (示例)
    uint8_t  mac[6];             // addr 68 MAC[6]
    uint8_t  reserved_74_99[26]; // addr 74-99 保留
} iot_attrs_t;                  // 40 bytes (60..99)

/* PAYGO / 计费 信息（占位，按实际表格调整） */
typedef struct __attribute__((packed)) {
    uint32_t paygo_version;               // 100..103
    uint8_t  key[16];                     // 104..119
    uint64_t starting_code;               // 120..127
    uint16_t reset_count;                 // 128..129 (ResetCount 写)
    uint64_t active_code;                 // 130..137 (ActiveCode 写)
    uint16_t set_lock_information_flag;   // 138..139 (写1执行锁定)
    uint8_t  set_unlock_information_flag[16]; // 140..155 (写入完整Key执行解锁)
    uint16_t current_count;               // 156..157 (只读)
    uint16_t remaining_payg_time_day;     // 158..159 (天, 只读)
    uint16_t remain_payg_time_min;        // 160..161 (分, 只读)
    uint32_t paygo_id;                    // 162..165 (RW)
    uint8_t  reserved_166_299[34];       // 166..199 保留
} paygo_info_t;                           // 100 bytes (100..199)

/* 物联/4G 信息（示例） addr 200..259 */
typedef struct __attribute__((packed)) {
    char     imei[20];    // addr 200 IMEI ASCII20
    char     imsi[20];    // addr 220 IMSI ASCII20
    char     iccid[20];   // addr 240 ICCID ASCII20
    uint8_t  reserved_260_299[40]; // 260 - 299
} cellular_info_t;        // 100 bytes (200..299)

/* Matter 信息（示例） addr 300..363 */
typedef struct __attribute__((packed)) {
    char matter_qr[32];   // addr 300 MatterQrCode ASCII32
    char matter_moc[32];  // addr 332 MatterMoc ASCII32
    uint8_t reserved_364_399[36]; // 保留至 399
} matter_info_t;         // occupy up to 399

/* 自定义写入参数：400 - 599 */
typedef struct __attribute__((packed)) {
    uint64_t voltage_region;      // addr 400, 8 bytes, RW
                                  // 说明: 1-美规;2-日规;3-欧规;4-澳规;5-中国;6-非洲;7-德国;8-中国台湾;9-英规
    uint8_t  reserved_408_599[192];// addr 408-599 保留
} custom_params_t;        // total 200 bytes (400..599)

/* 预设地址（设备上电后或点击标定按钮会下发） addr 1000 */
typedef struct __attribute__((packed)) {
    uint16_t calibration_mode_ctrl; // addr 1000, 2 bytes, W 进入标定模式（设备上电后，主机会执行写1操作）
    uint8_t  reserved_1002_1199[198];// 保留至 1199
} preset_area_t;          // occupies 1000..1199

/* 标定后控制（标定完成后，上位机会下发） addr 1200 */
typedef struct __attribute__((packed)) {
    uint16_t reboot_cmd;          // addr 1200, 2 bytes, W 重启指令
    uint8_t  reserved_1202;       // 根据需要延展
} post_calibration_ctrl_t;

/* 顶层合并结构：把各块按地址顺序整合，便于一次性映射整个空间（示例） */
typedef struct __attribute__((packed)) {
    device_identity_t   id;            // 0 - 23
    firmware_info_t     fw;            // 24 - 59
    iot_attrs_t         iot;           // 60 - 99
    paygo_info_t        paygo;         // 100 - 199
    cellular_info_t     cell;          // 200 - 299
    matter_info_t       matter;        // 300 - 399
    custom_params_t     custom;        // 400 - 599
    uint8_t             reserved_600_999[400]; // 600 - 999 填充到 1000
    preset_area_t       preset;        // 1000 - 1199
    post_calibration_ctrl_t post_ctrl;  // 1200 - ...
    /* 继续根据表格扩展后续区域 */
    rw_cmd_struct   rw_cmd;
} device_registers_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief      CAN标定数据初始化
  * @param[in]  char *model        
                uint64_t sn        
                uint64_t safecode  
  * @param[out] None
  * @return     void
  */
void Can_Product_Data_Init(char *model, uint64_t sn, uint64_t safecode);

/**
  * @brief      CAN标定4G相关信息更新
  * @param[in]  char *imei   
                char *imsi   
                char *iccid  
  * @param[out] None
  * @return     void
  */
void Can_Product_4G_Data_Update(char *imei, char *imsi, char *iccid);

/**
  * @brief      CAN标定数据写入回调
  * @param[in]  uint16_t write_offset  
                uint16_t data_len      
  * @param[out] None
  * @return     void
  */
void Can_iot_callback_set_type_0xFF(uint16_t write_offset, uint16_t data_len);

#ifdef __cplusplus
}
#endif
