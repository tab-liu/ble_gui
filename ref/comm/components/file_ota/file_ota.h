/**
  ******************************************************************************
  * @file      file_ota.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/11
  * @brief     文件资源升级模块
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

/* ======================== 本地模块文件引用（可选） ============================ */

#include "filesystem.h"

/* ================================ 头文件宏定义 ================================ */

// Xmodem 协议的负载大小（单位：字节）
#define XMODEM_PAYLOAD_SIZE 1024

// 文件头的固定长度为 20 字节（不含路径）
#define FILE_HEADER_FIXED_LEN 20

// 文件路径的最大长度为 255 字节
#define FILE_NAME_MAX_LEN 255

#if 0   // 其他文件内已定义

// 备份区起始地址
#define IMAGE_FLASH_AREA_ADDRESS 8192

// Flash操作块的长度（单位：字节）
#define FLASH_SEC_SIZE  4096

// 挂载点路径，文件系统将挂载到此路径下
#define FS_BASE_PATH    "/littlefs" 

#endif

// 文件资源数据目录
#define FILE_DATA_FOLDER_LEVEL1     "F/"

// 文件资源数据版本号路径
#define FILE_DATA_MARK_VERSION_PATH \
    FS_BASE_PATH"/"FILE_DATA_FOLDER_LEVEL1"ver.bin"

// 假设文件内版本号字符串最大长度为20
#define ver_str_len  20

/* ============================== 头文件结构体定义 ================================ */


#pragma pack(1)

/**
 * @brief OTA升级过程中的状态码定义
 */
typedef enum
{
    OTA_STATUS_OK = 0,                // 正常，无错误
    OTA_STATUS_SIZE_ERROR = 1,        // 文件或数据大小错误
    OTA_STATUS_FLASH_ERASE_ERROR = 2, // Flash擦除失败
    OTA_STATUS_FLASH_WRITE_ERROR = 3, // Flash写入失败
    OTA_STATUS_FLASH_READ_ERROR = 4,  // Flash读取失败
    OTA_STATUS_FILE_ERROR = 5,        // 文件操作错误
    OTA_STATUS_CRC16_ERROR = 6,       // CRC16校验失败
    OTA_STATUS_CRC32_ERROR = 7,       // CRC32校验失败
    OTA_STATUS_SEQ_ERROR = 8,         // 升级包序号错误
    OTA_STATUS_TIMEOUT = 9,           // 超时错误
    OTA_STATUS_NO_RESPONSE = 10,      // 无响应错误
    OTA_STATUS_MEMORY_ERROR = 11,     // 内存分配失败
    OTA_STATUS_ABORT = 12,            // 升级过程被中止
    OTA_STATUS_FILE_TYPE_ERROR = 13,  // 文件类型错误
    OTA_STATUS_CAN_BUSY = 14,         // CAN总线繁忙
    OTA_STATUS_CAN_SEND_ERROR = 15,   // CAN总线发送失败
    OTA_STATUS_FILE_NULL = 16,        // 文件为空或未找到
} OTAStatusCode;

/**
 * @brief 文件头解析信息结构体
 * 
 * 用于在内存中存储从文件传输协议头中解析出的所有关键信息。
 * 它不仅包含了协议头部的固定字段，还包含了在解析过程中生成的运行时数据（如文件名缓冲区）。
 * 这个结构体本身是一个方便的数据容器，而不是对传输流的直接内存映射。
 * 文件编号代表每个OTA文件的序号，依次递增；路径长度只包含路径信息的长度，文件长度只包含文件数据的长度；
 * CRC32只校验文件数据内容。起始信息的所有格式均为低位在前，高位在后
 */
typedef struct
{
    /* --- 从协议头直接解析的字段 --- */

    uint16_t file_id;           /**< 文件编号 (2字节)，从协议中读取。 */
    uint32_t crc32;             /**< 文件数据的CRC32校验和 (4字节)，从协议中读取。 */
    uint16_t path_length;       /**< 原始路径/文件名的实际长度 (2字节)，从协议中读取。 */
    uint32_t file_length;       /**< 文件数据内容的实际长度 (4字节)，从协议中读取。 */
    uint32_t modification_time; /**< 文件的修改时间 (通常为Unix时间戳, 4字节)，从协议中读取。 */
    uint32_t extra_info;        /**< 附加信息 (4字节)，从协议中读取，具体含义由应用定义。 */

    /* --- 解析过程中生成的运行时字段 --- */

    /**
     * @brief 用于存储从协议中解析出的原始文件名的缓冲区。
     *        其内容根据解析出的 path_length 填充。
     */
    char     original_name[270];

    /**
     * @brief 用于存储生成的、将在本地文件系统上使用的备份文件名。
     *        例如，将 "data.txt" 转换为 "data_bp.bin"。
     */
    char     backup_name[270];

} FilePacketInfo_t;


#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建并启动文件OTA处理任务。
 *
 * @param ota_data_len 要处理的OTA数据的总长度（字节）。
 * @return esp_err_t 成功时返回 ESP_OK，失败时返回 ESP_FAIL。
 */
esp_err_t Start_File_OTA_Task(uint32_t ota_data_len);

/**
 * @brief 获取当前文件升级状态标志
 * @return bool 当前升级状态（true: 正在升级，false: 未升级）
 */
bool Get_File_OTA_Status(void);

/**
 * @brief 获取当前文件升级进度
 * @return uint8_t 当前升级进度（0~100）
 */
uint8_t Get_File_OTA_Pct(void);

/**
 * @brief 获取当前文件升级故障码
 * @return uint8_t 当前升级故障码
 */
uint8_t Get_File_OTA_Errcode(void);

/**
 * @brief 获取当前文件数据版本号
 * @return uint32_t 当前文件数据版本号
 */
uint32_t Get_File_Data_Version(void);

#ifdef __cplusplus
}
#endif
