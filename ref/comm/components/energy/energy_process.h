/**
  ******************************************************************************
  * @file      energy_process.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/14
  * @brief     能量本地存储模块
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
#include "comm_define.h"

/* ================================ 头文件宏定义 ================================ */

// 无特殊要求项目，按下方默认执行，否则应在"comm_define.h"内自行定义
#ifndef ENABLE_ENERGY_PROJECT_CUSTOM_SELF

// 能量项使能默认宏定义（可自行选择，至多不超过15项）
#define ENABLE_ENERGY_RAW_PV_TOTAL
#define ENABLE_ENERGY_RAW_GRID_INPUT
//#define ENABLE_ENERGY_RAW_GRID_OUTPUT
#define ENABLE_ENERGY_RAW_AC_LOAD
#define ENABLE_ENERGY_RAW_DC_LOAD
//#define ENABLE_ENERGY_RAW_AC_PV
//#define ENABLE_ENERGY_RAW_PV_TOTAL_AGAIN
#define ENABLE_ENERGY_RAW_BATTERY_CHARGE
#define ENABLE_ENERGY_RAW_BATTERY_DISCHARGE
//#define ENABLE_ENERGY_RAW_PV_TO_AC_LOAD
//#define ENABLE_ENERGY_RAW_CAR_CHARGE

#endif

// 能量项原始顺序宏定义（用于结构体成员顺序说明，便于转换和注释）
#define ENERGY_RAW_PV_TOTAL              1   // 总PV(直流+交流PV)
#define ENERGY_RAW_GRID_INPUT            2   // 电网买电(GridInput)
#define ENERGY_RAW_GRID_OUTPUT           3   // 电网卖电(GridOutput)
#define ENERGY_RAW_AC_LOAD               4   // AC负载
#define ENERGY_RAW_DC_LOAD               5   // DC负载
#define ENERGY_RAW_AC_PV                 6   // 交流PV
#define ENERGY_RAW_PV_TOTAL_AGAIN        7   // 总PV（重复项）
#define ENERGY_RAW_BATTERY_CHARGE        8   // 电池总充电
#define ENERGY_RAW_BATTERY_DISCHARGE     9   // 电池总放电
#define ENERGY_RAW_PV_TO_AC_LOAD         10  // PV到AC负载
#define ENERGY_RAW_CAR_CHARGE            11  // 车充

// 能量项最大支持总数
#define ENERGY_ITEM_MAX_NUM              15

// 能量节点每小时存储次数，默认为1
// 为支持智能预测等，当前使用2（即写入周期为30min）
// 如使用EEPROM存储，可使用12（即写入周期为5min）
#define ENERGY_HOUR_PERIOD_CNT           2

// 能量节点最大个数：当前存储过去30天，默认配置（ENERGY_FILE_HOUR_PERIOD_CNT为2）时需要80K
// 实际按照芯片能力实现，在不改变精度的情况下可选存储15天/7天
#define ENERGY_UNIT_MAX_NUM              (31*24*ENERGY_HOUR_PERIOD_CNT)

// 能量数据更新防抖上限（超过该值认为设备数变动，不存储本次变化）
#define ENERGY_DATA_FILTER_LIMIT_WH      150

/* ======================================================== */

// 能量文件节点记录单次最多处理条数
#define ENERGY_UNIT_PROCESS_MAX_NUM     5

// 能量文件头起始地址
#define ENERGY_FILE_HEADER_ADDR		    0

// 能量文件头数据长度
#define ENERGY_FILE_HEADER_LEN          512

// 能量文件节点记录起始地址
#define ENERGY_FILE_RECORD_UNIT_ADDR    ENERGY_FILE_HEADER_LEN

// 能量文件数据点长度
#define ENERGY_FILE_RECORD_UNIT_LEN     50

// 能量长期数据存储年限
#define ENERGY_LONG_TERM_SAVE_YEAR_MAX  15

// 能量长期数据结构体长度
#define ENERGY_LONG_TERM_DATA_MAX_LEN   128

#if 0   // 引用其他文件时于该文件内实现，这里不定义

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE                ((uint16_t)-1)

/**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_BASE_PATH                    "/littlefs" 

#endif

/* ======================================================== */

// 能量模块文件目录
#define ENERGY_FILE_FOLDER_LEVEL              "K/"

// 能量节点文件名称
#define ENERGY_FILE_UNIT_NAME                 "hourB"

// 能量项原始顺序宏定义对应字符名称（直接定义，不依赖其他宏）
#define ENERGY_RAW_PV_TOTAL_NAME              "pv"  // 总PV(直流+交流PV)
#define ENERGY_RAW_GRID_INPUT_NAME            "gr"  // 电网买电(GridInput)
#define ENERGY_RAW_GRID_OUTPUT_NAME           "fd"  // 电网卖电(GridOutput)
#define ENERGY_RAW_AC_LOAD_NAME               "ac"  // AC负载
#define ENERGY_RAW_DC_LOAD_NAME               "dc"  // DC负载
#define ENERGY_RAW_AC_PV_NAME                 "ap"  // 交流PV
#define ENERGY_RAW_PV_TOTAL_AGAIN_NAME        "pa"  // 总PV（重复项）
#define ENERGY_RAW_BATTERY_CHARGE_NAME        "bc"  // 电池总充电
#define ENERGY_RAW_BATTERY_DISCHARGE_NAME     "bd"  // 电池总放电
#define ENERGY_RAW_PV_TO_AC_LOAD_NAME         "pl"  // PV到AC负载
#define ENERGY_RAW_CAR_CHARGE_NAME            "cc"  // 车充

// 能量节点文件路径
#define ENERGY_FILE_PATH_IOT_MAX(buf)	\
	sprintf(buf, "%s/%s%s", FS_BASE_PATH, ENERGY_FILE_FOLDER_LEVEL, ENERGY_FILE_UNIT_NAME)

// 能量存储年文件路径
#define ENERGY_FILE_PATH_YEAR(buf,folder,node,sub_node)	\
	sprintf(buf, "%s/%s%s%d_%02d", FS_BASE_PATH, ENERGY_FILE_FOLDER_LEVEL, folder, node, sub_node)

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

/* 根据能量项宏开关生成枚举索引（按需注释对应 ENABLE_* 宏以屏蔽某项；索引会紧凑排列） */
typedef enum {
#ifdef ENABLE_ENERGY_RAW_PV_TOTAL
    ENERGY_IDX_PV_TOTAL = 0,           // 1. 总PV(直流+交流PV)
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_INPUT
    ENERGY_IDX_GRID_INPUT,             // 2. 电网买电(GridInput)
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_OUTPUT
    ENERGY_IDX_GRID_OUTPUT,            // 3. 电网卖电(GridOutput)
#endif

#ifdef ENABLE_ENERGY_RAW_AC_LOAD
    ENERGY_IDX_AC_LOAD,                // 4. AC负载
#endif

#ifdef ENABLE_ENERGY_RAW_DC_LOAD
    ENERGY_IDX_DC_LOAD,                // 5. DC负载
#endif

#ifdef ENABLE_ENERGY_RAW_AC_PV
    ENERGY_IDX_AC_PV,                  // 6. 交流PV
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TOTAL_AGAIN
    ENERGY_IDX_PV_TOTAL_AGAIN,         // 7. 总PV（重复项，按需求可合并）
#endif

#ifdef ENABLE_ENERGY_RAW_BATTERY_CHARGE
    ENERGY_IDX_BATTERY_CHARGE,         // 8. 电池总充电
#endif

#ifdef ENABLE_ENERGY_RAW_BATTERY_DISCHARGE
    ENERGY_IDX_BATTERY_DISCHARGE,      // 9. 电池总放电
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TO_AC_LOAD
    ENERGY_IDX_PV_TO_AC_LOAD,          // 10. PV到AC负载
#endif

#ifdef ENABLE_ENERGY_RAW_CAR_CHARGE
    ENERGY_IDX_CAR_CHARGE,             // 11. 车充
#endif

    ENERGY_ITEM_REAL_NUM               // 能量项当前使用总数
} energy_item_idx_e;

_Static_assert(ENERGY_ITEM_REAL_NUM <= ENERGY_ITEM_MAX_NUM, "ENERGY_ITEM_REAL_NUM exceeds ENERGY_ITEM_MAX_NUM");

/**
 * @brief 通用KWH能量统计结构体（uint32_t版）
 */
typedef struct {
    uint32_t energy_u32[ENERGY_ITEM_MAX_NUM]; // 能量数组，具体含义通过宏索引
} kwh_energy_u32_t;

/**
 * @brief 通用KWH能量统计结构体（uint16_t版）
 */
typedef struct {
    uint16_t energy_u16[ENERGY_ITEM_MAX_NUM]; // 能量数组，具体含义通过宏索引
} kwh_energy_u16_t;

/**
 * @brief 节点充放电计划及相关信息结构体
 */
typedef struct {
    uint16_t price_Feedback;    // 馈电到电网电价（第三方数据，可选）
    uint16_t price_Chgin;       // 电网充电电价（第三方数据）
    uint16_t Pv_forecast;       // 光伏发电预测（第三方数据）
    int16_t  ac_charge_plan;    // 市电充放电计划（正值：充电，负值：馈电网，临时变量）
    uint8_t  soc;               // SOC日志（电池剩余电量百分比）
    uint8_t  reserved[11];      // 保留字段，便于扩展
} base_charge_plan_t;

/**
 * @brief 文件头协议版本枚举
 */
typedef enum {
    ENERGY_FILE_VER_V1 = 1,   // 协议版本1
    // 可根据实际需求继续扩展
} energy_file_ver_e;

/**
 * @brief 记录文件的头部信息结构体
 * @note 头部大小固定为 512 字节。
 *       使用联合体，方便以字节数组或结构化形式访问。
 */
typedef union {
    /**
     * @brief 以字节数组形式访问全部512字节数据。
     */
    uint8_t all[ENERGY_FILE_HEADER_LEN];

    /**
     * @brief 以结构化形式访问数据成员。
     * 内部结构体也需要 packed 属性以保证与字节数组的内存布局完全一致。
     */
    struct __attribute__((packed)) {
        uint8_t ver;                                // 文件头协议版本
        uint8_t unit_hour_period;                   // 数据单元周期（每小时记录次数）
        uint16_t unit_bytes;                        // 数据单元大小（字节）
        uint32_t first_utc_timestamp;               // 首次数据存储时间戳（UTC秒）
        uint32_t utc_timestamp;                     // 最新数据时间戳（UTC秒）
        uint32_t unit_max_cnt;                      // 数据单元最大个数
        uint32_t unit_real_cnt;                     // 数据单元实际个数
        uint32_t unit_bias;                         // 最新数据在文件中的偏移
        uint8_t item_total;                         // 支持能量标签总数
        uint8_t index[ENERGY_ITEM_MAX_NUM];         // 每一路能量的标签
        kwh_energy_u32_t delta_energy_all_100Wh;    // 总能量增量
        kwh_energy_u32_t delta_energy_year_100Wh;   // 年能量增量
        kwh_energy_u32_t delta_energy_month_100Wh;  // 月能量增量
        kwh_energy_u32_t delta_energy_day_100Wh;    // 日能量增量
        kwh_energy_u16_t delta_energy_unit_1Wh;     // 能量节点增量
        kwh_energy_u16_t delta_energy_calc_1Wh;     // 能量计算增量
        base_charge_plan_t base_charge_plan;  // 当前充放电计划及相关信息
    };
} energy_file_header_t;

_Static_assert(sizeof(energy_file_header_t) == ENERGY_FILE_HEADER_LEN, "energy_file_header_t size mismatch");

/**
 * @brief 能量文件记录单元结构体
 * @note 通过联合体实现字节数组与结构化成员的灵活访问，结构体需使用 packed 属性保证内存布局一致。
 */
typedef union {
    uint8_t all[ENERGY_FILE_RECORD_UNIT_LEN];  // 以字节数组形式访问全部数据

    struct __attribute__((packed)) {
        kwh_energy_u16_t delta_energy;        // 节点能量统计（uint16_t数组）
        base_charge_plan_t charge_plan;       // 节点充放电计划及相关信息
    };
} energy_file_record_unit_t;

_Static_assert(sizeof(energy_file_record_unit_t) == ENERGY_FILE_RECORD_UNIT_LEN, "energy_file_record_unit_t size mismatch");

/**
 * @brief      年度能量统计结构体
 * @details    用于存储某一年及其下属月份、天的能量统计数据，支持快速访问和校验。
 *             - 支持整体字节访问（all[128]），便于序列化/存储/传输。
 *             - 支持结构化访问，便于按年、月、日统计和查询。
 */
typedef struct {
    union {
        uint8_t all[ENERGY_LONG_TERM_DATA_MAX_LEN];   /**< 原始字节数组视图，用于文件读写、序列化等场景 */

        struct {
            uint16_t type;              /**< 能量类型标识（如PV/GRID等，具体类型由业务定义） */
            uint16_t year;              /**< 统计年份（如2025） */
            uint32_t total_energy;      /**< 当年总能量（单位Wh或kWh，具体由业务定义） */
            uint32_t mouth_energy[12];  /**< 每个月的总能量（下标0~11分别对应1~12月） */
            uint16_t day_enengy[31];    /**< 当月每天的总能量（下标0~30分别对应1~31日，未用天可为0） */
            uint16_t crc16;             /**< CRC16校验值，用于数据完整性校验 */
        };
    };
} energy_file_year_record_t;

_Static_assert(sizeof(energy_file_year_record_t) == ENERGY_LONG_TERM_DATA_MAX_LEN, "energy_file_year_record_t size mismatch");

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief      能量存储模块初始化
  * @note       建议在系统上电后初始化期间执行。
  * @param[in]  void  
  * @param[out] None
  * @return
  *  - 0: 成功，所有资源已成功创建并尝试读取文件头。
  *  - -1: 失败，可能由于参数错误或内存分配失败。
  */
int Energy_Module_Init(void);

/*------------------------------------------------------------------------------
 Function: Energy_File_Unit_Data_Read_Incremental
 -----------------------------------------------------------------------------*/
/**
 * @brief      读取增量能量数据到动态分配的缓冲区（数据通道接口）
 * @details    此函数是实现能量数据增量同步的核心接口。它会：
 *             1. 调用辅助函数 `Energy_File_Unit_Data_Count_New_Records` 计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *             5. 注意，返回的缓冲区大小取决于ENERGY_UNIT_MAX_NUM，
 *                1000条时可达几十K，自行评估使用
 *
 * @param[in]  since_timestamp  一个UNIX时间戳，用于界定“新”记录。
 * @param[out] out_buffer       一个指向 `uint8_t*` 的指针。如果函数成功，
 *                              它将被设置为新分配的缓冲区的地址。
 * @param[out] out_size         一个指向 `uint32_t` 的指针。如果函数成功，
 *                              它将被设置为缓冲区的总大小（字节）。
 *
 * @return     int
 *             - 0: 成功 (即使没有新记录也返回成功)。
 *             - -1: 失败（如内存分配失败、文件读取失败等）。
 *
 * @note       **重要**: 调用者在处理完 `out_buffer` 中的数据后，
 *             **必须**负责调用 `free(out_buffer)` 来释放内存。
 */
int Energy_File_Unit_Data_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size);

/*------------------------------------------------------------------------------
 Function: Is_Energy_File_Unit_Data_File
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断传入的文件标识符是否为当前的能量数据文件。（数据通道接口）
 * @details    此函数用于验证外部请求的文件是否为本模块管理的能量数据文件。
 *             它会根据传入的参数构建一个完整的路径，并与模块内部生成的
 *             标准路径进行比较。
 *
 * @param[in]  fname  一个指向数据通道文件（自定义格式）参数的指针。
 *                    - fname[0]: 文件夹层级。
 *                    - &fname[1]: 文件名字符串。
 *
 * @return     int
 *             - 0: 是能量数据文件。
 *             - -1: 不是能量数据文件或参数错误。
 */
int Is_Energy_File_Unit_Data_File(const char *fname);

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符（ENERGY_FILE_FOLDER_LEVEL[0]）
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Energy_File_Unit_Data_File_Id(char *out_buf, size_t buf_len);

/**
 * @brief 统计晚于指定时间戳的日志新记录数（含信号量保护）。
 *
 * @param[in] since_timestamp  自 Unix 纪元起的时间戳；传 0 表示统计全部记录。
 *
 * @return int
 *         - >=0 : 新记录数量
 *         - -1  : 参数错误或其它不可恢复错误
 *         - -2  : 获取文件处理信号量失败（记录将被丢弃）
 */
int Is_Energy_File_Unit_Data_Count_New(time_t since_timestamp);

/*------------------------------------------------------------------------------
 Function: Energy_File_Data_Process
 -----------------------------------------------------------------------------*/
/**
 * @brief      能量数据文件主处理任务（批量写入+补零+时间对齐）
 * @details    该函数负责将本周期生成的能量记录批量写入本地文件，并自动处理补零、长期数据保存及时间戳对齐。
 *             主要流程如下：
 *             1. 批量生成节点单位数据，填充本地缓冲区（Energy_Unit_Data）。
 *             2. 若有新记录，分块写入能量数据文件（循环队列方式），并清空本周期增量。
 *             3. 检查是否需要补零（空记录），如有则批量补零并打印日志。
 *             4. 持久化长期能量统计数据（如年度、月度等）。
 *             5. 按小时节点对齐更新时间戳（utc_timestamp/first_utc_timestamp），保证记录时间与存储节点一致。
 *             6. 持久化文件头到磁盘，确保状态同步。
 *             7. 全过程受互斥锁保护，保证线程安全。
 *
 * @note       本函数适用于循环队列结构的能量数据文件，支持高效批量写入和自动覆盖。
 *             注意：本函数必须运行在系统时间已获取的情况下
 *             涉及Flash操作，建议运行在低速任务，周期1~5s即可
 */
void Energy_File_Data_Process(void);

/*------------------------------------------------------------------------------
 Function: Energy_File_Raw_Data_Update
 -----------------------------------------------------------------------------*/
/**
 * @brief      能量原始数据及充电计划批量更新接口
 * @details    依次调用能量累计数据更新和充电计划更新函数，
 *             用于一次性同步能量统计和充电计划到文件头缓存。
 *             （建议调用周期1s，不涉及Flash操作，可放置于快速线程）
 *             - 能量数据和充电计划均可独立为空，若为空则对应部分不更新。
 *             - 返回值为各子操作的结果，若有任一失败则整体返回失败。
 *
 * @param[in]  energy      指向能量数据结构体（kwh_energy_u32_t），可为NULL
 * @param[in]  to_1wh      能量单位换算比例（如1表示直接累加，10表示每单位乘10）
 * @param[in]  charge_plan 指向充电计划结构体（base_charge_plan_t），可为NULL
 *
 * @return     int
 *             - 0: 成功，所有数据已正确更新
 *             - -1: 参数错误或任一子操作失败
 */
int Energy_File_Raw_Data_Update(kwh_energy_u32_t *energy, uint16_t to_1wh, base_charge_plan_t *charge_plan);

/**
 * @brief 获取指定索引的日能量（单位：Wh）
 *
 * @param index 索引号（由调用方保证有效范围）
 * @return uint32_t 返回日能量值，单位为瓦时(Wh)
 */
uint32_t Energy_Day_Data_Wh_Get(uint8_t index);

/**
 * @brief 获取指定索引的月能量（单位：100Wh）
 *
 * @param index 索引号（由调用方保证有效范围）
 * @return uint32_t 返回月能量值，单位为瓦时(Wh)
 */
uint32_t Energy_Month_Data_100Wh_Get(uint8_t index);

/**
 * @brief 获取指定索引的总能量（单位：100Wh）
 *
 * @param index 索引号（由调用方保证有效范围）
 * @return uint32_t 返回总能量值，单位为100瓦时(Wh)
 */
uint32_t Energy_All_Data_100Wh_Get(uint8_t index); 

/**
  * @brief      立即保存文件头，可用于下电前存储临时数据                
  */
int Energy_File_Header_Save(void); 

/**
 * @brief 恢复能量文件到出厂默认并初始化文件头并写入存储
 *
 * 该函数执行以下步骤：
 *  - 尝试获取文件操作互斥锁（xEnergyFileProcessSemaphore），超时则返回错误。
 *  - 删除能量文件所在目录（递归删除）。
 *  - 清零并初始化 Energy_File_Header（版本、单元周期、单元字节数、最大单元数等）。
 *  - 初始化能量索引映射（Energy_Index_Map_Init）。
 *  - 将新的文件头写入到能量文件起始位置（fs_file_write_at）。
 *  - 释放互斥锁。
 *
 * @note
 *  - 函数在写入失败时会记录日志但仍会释放互斥；调用者可根据返回值决定后续动作。
 *  - 该函数会修改全局 Energy_File_Header，并依赖全局信号量 xEnergyFileProcessSemaphore。
 *  - 删除目录与写文件为可能耗时操作，互斥等待超时时间为 100 ms（可按需调整）。
 *
 * @return int
 *      0   成功（已完成默认恢复并写入文件头）
 *     -1   失败（未能获取互斥或其他致命错误）
 */
int Energy_File_Restore_Factory_Defaults(void);

/**
 * @brief 清除指定索引的能量数据并更新能量文件头
 * @param index 能量项索引
 * @return 0 成功；-1 失败
 */
int Energy_File_Clean_Specific_Data_By_Index(uint8_t index);

#ifdef __cplusplus
}
#endif
