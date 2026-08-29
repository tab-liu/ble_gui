/**
  ******************************************************************************
  * @file      udt_incremental.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/2/27
  * @brief     数据通道增量读取逻辑模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/2/27  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */


#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "filesystem.h"
#include "dev_data_record.h"

/* ================================ 头文件宏定义 ================================ */

// 增量读取记录最大存储文件个数
#define UDT_READ_RECORD_FILE_NUM_MAX    50

// 单条增量读取记录最大长度
#define UDT_READ_RECORD_MAX_LEN         20

#if 0   // 引用其他文件时于该文件内实现，这里不定义

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE                ((uint16_t)-1)

/**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_BASE_PATH                    "/littlefs" 

// 历史记录存储目录
#define PARAMETER_FOLDER_LEVEL1         "S/"

#endif

// 增量读取记录文件目录
#define UDT_READ_RECORD_FILE_FOLDER_LEVEL    PARAMETER_FOLDER_LEVEL1
#define UDT_READ_RECORD_FILE_PATH_IOT(buf)	\
	sprintf(buf, "%s/%s%s", FS_BASE_PATH, UDT_READ_RECORD_FILE_FOLDER_LEVEL, "udtrf")

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

typedef union {
    uint8_t all[UDT_READ_RECORD_MAX_LEN];
    struct __attribute__((packed)) {
        uint8_t  fname[10];        /**< 文件名，10字节 */
        uint32_t last_time;        /**< 最后一次读取的时间戳（使用 uint32_t 确保大小） */
        uint16_t last_send_cnt;    /**< 最后一次发送到服务器的包数 */
        uint16_t last_resp_cnt;    /**< 最后一次服务器响应的包数 */
        uint8_t  reserved[2];      /**< 保留，补齐到20字节 */
    };
} udt_file_record_t;
_Static_assert(sizeof(udt_file_record_t) == UDT_READ_RECORD_MAX_LEN, "udt_file_record_t size mismatch");

// 固定长度版本（兼容原来定长10字节 name）
typedef struct __attribute__((packed)) {
    uint8_t  tag;             // TLV 类型（例如 NEW_LOG）
    uint8_t  value_len;       // new_log_value 长度（字节）
    uint16_t new_log_value;   // 新日志值（小端）
    uint8_t  file_index;      // 文件索引或标识
    uint8_t  file_name_len;   // 文件名实际长度（<= FILE_NAME_MAX）
    uint8_t  file_name[10]; // 不足时可以 '\0' 填充
} TLV_NewLog_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* 静态/运行时初始化 udt_file_record_t 的辅助函数 */
udt_file_record_t udt_record_init_from(const char *fname, time_t last_time, uint16_t last_send_cnt, uint16_t last_resp_cnt);

/**
  * @brief      UDT 增量读取模块初始化
  * @note       建议在系统上电后、相关任务启动前调用。本函数会：
  *                - 初始化本地增量记录数组为0；
  *                - 创建用于保护增量记录/文件操作的互斥信号量（若尚未创建）；
  *                - 从文件读取增量记录到内存，若读取长度不符则写入默认（全0）记录文件。
  *                - 在访问文件时会获取互斥锁（超时 1000 ms）。
  * @param[in]  void
  * @param[out] None
  * @return
  *  -  0 : 成功
  *  - -1 : 失败，可能原因包括信号量创建失败、互斥获取超时或文件读写错误
  */
int Udt_Incremental_Module_Init(void);

/**
  * @brief    在 udt_incremental_record 数组中查找与 fname 相同的记录并返回序号
  * @param[in] fname  长度为10的文件名（不保证以'\0'终止）
  * @return   >=0: 找到的索引； -1: 未找到或发生错误（包括互斥获取失败）
  */
int Udt_Incremental_Find_Index(const uint8_t fname[10]);

/**
  * @brief    根据索引返回指向 udt_file_record_t 的指针
  * @note     返回的指针指向静态数组，调用者在并发访问时应自行保证同步（或通过模块接口保护）。
  * @param[in] index 要查找的索引（0 ~ UDT_READ_RECORD_FILE_NUM_MAX-1）
  * @return   指向对应记录的指针；索引越界或互斥获取失败时返回 NULL
  */
udt_file_record_t* Udt_Incremental_Get_Record_By_Index(int index);

/**
  * @brief    使用输入记录更新 udt_incremental_record 数组
  * @note     行为：
  *             - 若数组中存在 fname 相同的记录，则覆盖该记录；
  *             - 否则查找 fname 为空（全0）的槽位写入；
  *             - 若无空位，则覆盖 last_time 最小的记录。
  *           本函数对数组访问做互斥保护。
  * @param[in] new_rec 指向要写入的记录（不得为 NULL）
  * @return   >=0: 更新/插入的索引； -1: 参数错误或互斥获取失败
  */
int Udt_Incremental_Update_Record(const udt_file_record_t *new_rec);

#ifdef __cplusplus
}
#endif
