/**
  ******************************************************************************
  * @file      cloud_sync.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/7/6
  * @brief     云边参数同步
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/7/6   <td>1.0     <td>lixingyu   <td>Create the initial version
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
#include "utils.h"

/* ================================ 头文件宏定义 ================================ */

/*参数主从对象地址使能，映射Modbus从机地址，逆变器本身默认不存储虚拟主机参数区，即无此定义*/
//#define CONFIG_CLOUD_SYNC_SLAVE_ADDR_ENABLE

// 参数属性文件版本标志
#define MODBUS_PARAM_DESC_FILE_VER_V1   0xAA55

/*各具备主机能力的逆变器都开辟800个字节掉电不丢失区域预留，用于预留100个参数单元*/
#define MODBUS_PARAM_DESC_OBJECT_MAX    100

// 参数属性文件头数据长度
#define MODBUS_PARAM_DESC_HEADER_LEN    32

/* ======================================================== */

#if 0   // 引用其他文件时于该文件内实现，这里不定义

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE                ((uint16_t)-1)

/**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_BASE_PATH                    "/littlefs" 

#endif

// 参数对象属性文件头起始地址
#define CLOUD_SYNC_FILE_HEADER_ADDR		    0

// 参数对象属性文件数据起始地址
#define CLOUD_SYNC_FILE_DATA_ADDR           MODBUS_PARAM_DESC_HEADER_LEN

// 参数对象属性文件目录
#define CLOUD_SYNC_FILE_FOLDER_LEVEL        "S/"

// 参数对象属性文件名称
#define CLOUD_SYNC_FILE_NAME                "sync"

// 参数对象属性文件路径
#define CLOUD_SYNC_FILE_PATH_IOT_MAX(buf)	\
	sprintf(buf, "%s/%s%s", FS_BASE_PATH, CLOUD_SYNC_FILE_FOLDER_LEVEL, CLOUD_SYNC_FILE_NAME)

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

/**
 * @brief 记录文件的头部信息结构体
 */
typedef union {
    /**
     * @brief 以字节数组形式访问全部32字节数据。
     */
    uint8_t all[MODBUS_PARAM_DESC_HEADER_LEN];
    struct __attribute__((packed)) {
        uint16_t ver;                               // 文件头协议版本
        uint16_t unit_bytes;                        // 数据单元大小（字节）
        uint32_t unit_max_cnt;                      // 数据单元最大个数
        uint32_t unit_real_cnt;                     // 数据单元实际个数
        uint32_t mesh_utc_timestamp;                // 空间最近一次上传时间戳（UTC秒）
        uint32_t sn_utc_timestamp;                  // 本机最近一次上传时间戳（UTC秒）
    };
} modbus_param_desc_header_t;

_Static_assert(sizeof(modbus_param_desc_header_t) == MODBUS_PARAM_DESC_HEADER_LEN, "modbus_param_desc_header_t size mismatch");

/**
 * @brief 参数对象属性
 */
typedef struct {
#ifdef CONFIG_CLOUD_SYNC_SLAVE_ADDR_ENABLE
    uint8_t  slave_addr;   /* 参数主从对象地址：映射 Modbus 从机地址 */
#endif
    uint16_t start_addr;   /* 参数起始地址 */
    union {
        uint16_t all;
        struct {
            uint16_t length_value : 12; /* bit11~0: 长度值，max=4096 */
            uint16_t reserved     : 2;  /* bit13~12: 预留，填0 */
            uint16_t protocol_ver : 1;  /* bit14: 起始地址协议版本，0=modbus beta */
            uint16_t length_type  : 1;  /* bit15: 长度类型，0=字节数量，1=bit数量 */
        } bits;
    } len;                    /* 参数连续长度字节数 */
    uint32_t timestamp;       /* 参数更新时间戳 */
} modbus_param_desc_t;

/**
 * @brief 参数对象额外属性（非存储）
 */
typedef struct {
    bool is_need_update;        /* 是否需要升级标志 */
} cloud_sync_ext_param_desc_t;

/**
 * @brief 参数表格属性
 */
typedef struct {
    char  data_field[32];       /* Field字段，每个Field代表一个分组 */
    uint8_t slaveaddr;          /* 参数设备从机地址，0xFF表示需根据SN锁定 */
    uint16_t start_reg_addr;    /* 参数起始寄存器地址 */
    uint16_t data_reg_cnt;      /* 参数寄存器个数 */
    bool is_user_defined;       /* 自定义生成标识，适用多电表等场景，需自定义组帧函数*/
} modbus_beta_field_object_t;

// 云同步更新队列项
typedef struct {
    uint8_t  slave_addr;    /* Modbus 从机地址 */
    uint16_t reg_addr;      /* 寄存器起始地址 */
    uint16_t reg_cnt;       /* 寄存器个数 */
} cloud_sync_queue_item_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 云边参数同步模块初始化（建议在寄存器初始化完成后执行）
 * @return true 成功；false 创建失败
 */
bool CloudSyncInit(void);

/**
 * @brief 向云同步更新队列压入一个参数更新请求（非阻塞）
 * @param slave_addr  Modbus 从机地址
 * @param reg_addr    寄存器起始地址
 * @param reg_cnt     寄存器个数
 * @return true 压入成功；false 队列满或队列未初始化
 */
bool cloud_sync_queue_push(uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_cnt);

/**
  * @brief      本地参数更新记录存储
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void cloud_sync_record_process(void);

/**
 * @brief 请求云端最新参数更新推送命令
 */
void cloud_sync_revd_param_cmd(bool cmd);

/**
 * @brief 基于时间戳对比的参数更新推送命令
 */
void cloud_sync_push_param_cmd(bool cmd);

#ifdef __cplusplus
}
#endif
