/**
  ******************************************************************************
  * @file      aiot_can_log.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/7/30
  * @brief     CAN LOG触发存储机制
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/7/30  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stdbool.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "aiot_log.h"
#include "filesystem.h"

/* ================================ 头文件宏定义 ================================ */

/*CAN LOG触发存储机制使能*/
#define AIOT_CAN_LOG_SAVE_ENABLE

#if 0   // 引用其他文件时于该文件内实现，这里不定义

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE                ((uint16_t)-1)

/**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_BASE_PATH                    "/littlefs" 

#endif

/*文件二级目录*/
#define AIOT_CANLOG_FOLDER_LEVEL1   "L/"

/*文件系统内文件名称 */
#define AIOT_CANLOG_FILE_NAME       FS_BASE_PATH"/"AIOT_CANLOG_FOLDER_LEVEL1"iot01_c05"

/*文件最大长度*/
#define AIOT_CANLOG_FILE_MAX_LEN    0x80000 // 512K（视芯片能力而定）

/*单次存储最长持续时间*/
#define AIOT_CANLOG_SAVE_TIMEOUT    300000  // 5min

/*存储片最大长度*/
#define AIOT_CANLOG_BUF_MAX_LEN     4096    // 4K

/*存储片个数*/
#define AIOT_CANLOG_BUF_NUM         (AIOT_CANLOG_FILE_MAX_LEN / AIOT_CANLOG_BUF_MAX_LEN)

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

/*执行状态*/
enum {
    AIOT_CANLOG_IDLE = 0,
    AIOT_CANLOG_SAVEING,
};

// CAN日志存储控制命令 (对应 bit[1:0])
typedef enum {
    CAN_LOG_STORE_INVALID    = 0,  // 00: 无效
    CAN_LOG_STORE_START      = 1,  // 01: CAN log文件存储开始
    CAN_LOG_STORE_FORCE_END  = 2,  // 10: CAN log文件存储强制结束
    CAN_LOG_STORE_FORCE_DEL  = 3   // 11: CAN log文件存储强制删除
} can_log_store_ctrl_t;

// CAN数据收发方向 (对应 bit[3:2])
typedef enum {
    CAN_LOG_DIR_INVALID      = 0,  // 00: 无效
    CAN_LOG_DIR_TX_ONLY      = 1,  // 01: 仅CAN TX
    CAN_LOG_DIR_RX_ONLY      = 2,  // 10: 仅CAN RX
    CAN_LOG_DIR_TXRX_ALL     = 3   // 11: CAN TX + RX 全部
} can_log_dir_t;

// 日志记录状态 (对应 bit[15:14]，只读字段)
typedef enum {
    CAN_LOG_STATUS_NONE      = 0,  // 00: 文件不存在或已删除
    CAN_LOG_STATUS_RECORDING = 1,  // 01: 正在进行CAN LOG存储
    CAN_LOG_STATUS_FINISHED  = 2,  // 10: 本次存储完成
    CAN_LOG_STATUS_VALID     = 3   // 11: 文件有效（读取命令仅在此状态有效）
} can_log_record_status_t;

/* ===================== 32位寄存器联合体结构体 ===================== */
// 超级命令 - 本地CAN LOG存储高阶功能配置 (32bit 位域映射)
// 位域顺序：从最低位(bit0)到最高位(bit31)，匹配小端编译器位域分配规则
// 低16位保持原有功能定义，高16位为预留扩展位
typedef union {
    uint32_t reg_val;               // 整体32位寄存器值，用于总线读写

    struct {
        uint32_t store_ctrl     : 2;  // bit[1:0]   存储控制命令
        uint32_t can_direction  : 2;  // bit[3:2]   CAN收发方向
        uint32_t reserved_4_5   : 2;  // bit[5:4]   保留
        uint32_t reserved_6_7   : 2;  // bit[7:6]   保留
        uint32_t reserved_8_9   : 2;  // bit[9:8]   保留
        uint32_t reserved_10_11 : 2;  // bit[11:10] 保留
        uint32_t reserved_12_13 : 2;  // bit[13:12] 保留
        uint32_t record_status  : 2;  // bit[15:14] 记录状态（只读）
        uint32_t reserved_16_31 : 16; // bit[31:16] 高16位预留扩展
    } bit;
} super_cmd_can_log_cfg_t;

/**
 * @brief 数据分片信息结构体
 * 描述单块分片数据的长度、起始地址
 */
typedef struct {
    uint16_t slice_len;    ///< 片数据长度（u16，单位：字节）
    uint8_t  *slice_ptr;   ///< 片数据起始地址指针（u8*）
} data_slice_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief      CAN日志入队
  * @param[in]  const uint8_t *data  
                uint16_t len         
                log_type_t type      
  * @param[out] None
  * @return     uint16_t
  */
uint16_t AIoT_Can_Log_Push(const uint8_t *data, uint16_t len, log_type_t type);

/**
  * @brief      CAN LOG存储逻辑
  * @param[in]  uint32_t cmd_value  
  * @param[out] None
  * @return     uint32_t
  */
uint32_t AIoT_Can_Log_Process(uint32_t cmd_value);


#ifdef __cplusplus
}
#endif
