/**
  ******************************************************************************
  * @file      smart_common.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/24
  * @brief     智能控制模块通用头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/21  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "filesystem.h"
#include "utils.h"

/* ================================ 头文件宏定义 ================================ */

// 设置指定月份有效（month: 1~12）
#define TOU_MONTH_SET(month_sel, month)      ((month_sel) |= (1U << ((month) - 1)))
#define TOU_MONTH_IS_SET(month_sel, month)   (((month_sel) >> ((month) - 1)) & 0x1)
#define TOU_MONTH_CLR(month_sel, month)      ((month_sel) &= ~(1U << ((month) - 1)))

// 设置指定日期有效（day: 1~31）
#define TOU_DAY_SET(day_sel, day)            ((day_sel) |= (1UL << ((day) - 1)))
#define TOU_DAY_IS_SET(day_sel, day)         (((day_sel) >> ((day) - 1)) & 0x1)
#define TOU_DAY_CLR(day_sel, day)            ((day_sel) &= ~(1UL << ((day) - 1)))

// 设置指定周内天有效（week: 0~6，0-周日，1-周一，...，6-周六）
#define TOU_WEEK_SET(week_sel, week)         ((week_sel) |= (1U << (week)))
#define TOU_WEEK_IS_SET(week_sel, week)      (((week_sel) >> (week)) & 0x1)
#define TOU_WEEK_CLR(week_sel, week)         ((week_sel) &= ~(1U << (week)))

/* 检查 TOU 时间配置是否无效（month/day/week 任一为 0 或指针为空） */
#define TOU_TIME_CFG_INVALID(cfg) ( (cfg) == NULL || \
                                    (cfg)->bits.month_sel == 0 || \
                                    (cfg)->bits.day_sel   == 0 || \
                                    (cfg)->bits.week_sel  == 0 )

// 时间点时间最大长度
#define TIME_UNIT_MAX_LEN           10

#if 0   // 引用其他文件时于该文件内实现，这里不定义

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE                ((uint16_t)-1)

/**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_BASE_PATH                    "/littlefs" 

#endif

// 智能控制模块文件目录
#define SMART_CTRL_FILE_FOLDER_LEVEL    PARAMETER_FOLDER_LEVEL1

/* ============================== 头文件结构体定义 ================================ */


#pragma pack(1)

typedef enum {
    TOU_TIME_TYPE_POINT   = 0, // 标准时间点设置
    TOU_TIME_TYPE_QUANTUM = 1, // 标准时间段设置（当前用于工作模式）
    TOU_TIME_TYPE_RSVD2   = 2, // 预留
    TOU_TIME_TYPE_RSVD3   = 3  // 预留
} tou_time_type_t;

typedef enum {
    TOU_REG_TYPE_MODBUS_BETA   = 0, // modbus beta (默认)
    TOU_REG_TYPE_CUSTOM_EXT    = 1, // 自定义非标扩展（待定）
    TOU_REG_TYPE_RESERVED_2    = 2, // 预留
    TOU_REG_TYPE_RESERVED_3    = 3  // 预留
} tou_reg_type_t;

// 严格按照位定义的TOU时间配置结构体
typedef union {
    uint8_t raw[TIME_UNIT_MAX_LEN];    // 原始字节数据（10字节）
    struct {
        uint32_t time_sec  : 17; // bit0-16: 一天内的秒(小时、分钟、秒)
        uint8_t  week_sel  : 7;  // bit17-23: 周内天选择（bit17~23）
        uint32_t day_sel   : 31; // bit24-54: 月内各天选择（1~31）
        uint16_t month_sel : 12; // bit55-66: 年内各月选择（1~12）
        uint8_t  reg_type  : 2;  // bit67-68: 执行寄存器变量定义
        uint8_t  device_id : 8;  // bit69-76: 执行设备对象序号（modbus从机地址）
        uint8_t  time_type : 3;  // bit77-79: 时间类型
    } bits;
} tou_time_config_t;

_Static_assert(sizeof(tou_time_config_t) == TIME_UNIT_MAX_LEN, "tou_time_config_t size mismatch");

/**
 * @brief TOU时间基准结构体
 * 
 * 用于在不同场景下灵活存储TOU时间基准信息：
 * - t_base_arr：指向一组天基准（如每天00:00:00的time_t数组），用于批量时间点判断等场景。
 * - last_time ：区间起始时间（秒），可用于单点或区间判断。
 * - now_time  ：区间结束时间（秒），可用于单点或区间判断。
 * - days      ：存储天数信息，便于快速传递或统计区间天数。
 */
typedef struct {
    time_t *t_base_arr;  ///< 指向天基准time_t数组的指针
    time_t last_time;    ///< 区间起始时间
    time_t now_time;     ///< 区间结束时间
    int days;            ///< 区间天数
} tou_time_base_t;

/**
 * @brief 时间段执行状态
 *
 * 0: 未配置
 * 1: 仅配置了起始点
 * 2: 已配置，处于空闲（配置完成后未触发）
 * 3: 正在生效（处于时间段内）
 */
typedef enum {
    TOU_PERIOD_STATUS_UNCONFIGURED = 0, /* 未配置 */
    TOU_PERIOD_STATUS_START_ONLY   = 1, /* 仅配置了起始点 */
    TOU_PERIOD_STATUS_CONFIGURED_IDLE = 2, /* 已配置，处于空闲 */
    TOU_PERIOD_STATUS_ACTIVE       = 3, /* 正在生效（处于时间段内） */
} tou_period_status_t;

/**
 * @brief 时间段控制对象（寄存器操作/执行目标/执行动作）
 *
 * 说明：
 * - reg_type/device_id/reg_addr 标识目标寄存器/设备（与单个 time point 的配置对应）；
 * - start_value/end_value 分别为时间段开始/结束时应写入寄存器的值（或表示要执行的动作编码）。
 * - 使用此结构可以在构建 time quantum 时把「控制目标」与「起止动作值」一并保存在时间段对象中，
 *   便于在删除/清理/触发时直接访问，而无需再次查询原始 time point 配置。
 */
typedef struct {
    uint8_t  reg_type;      /* 执行寄存器变量定义（与 tou_cfg.bits.reg_type 对应） */
    uint8_t  device_id;     /* 目标设备 id（modbus 从机地址） */
    uint16_t reg_addr;      /* 寄存器地址 */
    uint16_t start_value;   /* 起始动作（起始时写入的值） */
    uint16_t end_value;     /* 结束动作（结束时写入的值） */
} tou_period_ctrl_obj_t;

/**
 * @brief 时间段结构体（包含起始/结束节点索引、执行状态与控制对象）
 *
 * 字段说明：
 * - start_time_index / end_time_index: 对应 Smart_Tou_Ctrl_Config.smart_tou_unit[] 的索引；
 * - ctrl: 时间段控制对象，包含目标寄存器/设备及起止动作值；
 * - status: 当前时间段执行状态（见 tou_period_status_t）。
 *
 * 语义与使用约定：
 * - 在构建时间段时，应以起始点的 reg_type/device_id/reg_addr 填充 ctrl 的目标信息；
 * - start_value/end_value 可分别从起始点/结束点的 reg_cfg.reg_value 获取并赋入，以确保
 *   在触发或清理时能直接执行对应动作而无需额外查表；
 * - status 字段用于指示时间段的生命周期（未配置/已配置空闲/正在生效/...）。
 */
typedef struct {
    uint8_t                 start_time_index; /* 起始时间点索引（0..SMART_TOU_UNIT_MAX_CNT-1） */
    uint8_t                 end_time_index;   /* 结束时间点索引，0xFF 表示无结束点（start-only） */
    tou_period_ctrl_obj_t   ctrl;             /* 时间段控制对象（目标寄存器、起止动作） */
    tou_period_status_t     status;           /* 当前执行状态 */
} tou_time_quantum_t;

typedef enum {
    TIMEQUANTUM_BUILD_REBUILD = 0, /* 重建全部时间段（清除旧配置） */
    TIMEQUANTUM_BUILD_UPDATE  = 1  /* 基于已有 Time_Quantum_Config 更新（保留未变更的 status） */
} timequantum_build_mode_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

#if 0   // 引用其他文件时可选择于该文件内实现，这里不定义

/**
 * @brief 从文件的指定偏移处读取指定长度的数据。
 *
 * @param path 文件路径。
 * @param offset 读取操作的起始偏移地址（字节）。
 * @param data 用于存储读取数据的缓冲区指针。
 * @param size 希望读取的数据长度（字节）。
 * @return int 成功时返回实际读取到的字节数。如果发生错误（如文件无法打开）则返回-1。
 *         如果读取的起始偏移超出了文件末尾，将返回0。
 */
int fs_file_read_at(const char *path, uint32_t offset, uint8_t *data, int size);

/**
 * @brief 将数据写入文件的指定偏移处。
 *
 * 如果文件不存在，将会被创建。如果文件存在，则从指定偏移处开始覆盖写入，
 * 文件其他部分的数据保持不变。
 *
 * @param path 文件路径。
 * @param offset 写入操作的起始偏移地址（字节）。
 * @param data 指向要写入数据的缓冲区的指针。
 * @param size 希望写入的数据长度（字节）。
 * @return int 成功时返回实际写入的字节数。如果发生错误则返回-1。
 */
int fs_file_write_at(const char *path, uint32_t offset, const uint8_t *data, int size);

/**
 * @brief 分配并清零一段内存
 *
 * @details 这是一个内存分配的封装函数。
 *          通过预编译宏，它可以选择使用 ESP-IDF 特定的 `heap_caps_calloc`
 *          从 PSRAM (SPIRAM) 中分配内存，或者回退到标准的 `calloc` 函数。
 *          无论使用哪种方式，分配的内存都会被自动初始化为零。
 *
 * @param size 要分配的内存大小（以字节为单位）。
 *
 * @return void* 成功时返回指向已分配并清零的内存块的指针。
 *             如果内存分配失败，则返回 NULL。
 */
void *iot_calloc(size_t size);

#endif

/**
 * @brief 查找单个TOU配置点在指定区间内最近一次有效触发时间
 * 
 * @param tou_cfg    TOU时间配置指针
 * @param end        当前时间（秒，time_t）
 * @return time_t    最近一次有效触发时间（若无则返回0）
 *
 * 说明：
 * - 回溯区间为 [end - CONFIG_TOU_CHECK_BACK_DAYS * 86400, end]，即向前查找指定天数。
 * - 只要找到第一个满足月、日、周条件的有效触发点即返回，减少无效计算。
 * - 若配置无效或区间内无有效触发点，返回0。
 */
time_t Tou_Point_Last_Trigger(const tou_time_config_t *tou_cfg, time_t end);

/**
 * @brief 计算(last_time, now_time]区间内每一天的本地00:00:00的time_t基准数组
 * 
 * 该函数用于批量TOU时间点判断等场景，返回区间内所有日历天的本地零点time_t数组。
 * 例如：last_time=2025-11-25 23:00:00，now_time=2025-11-27 01:00:00，
 * 则返回2025-11-25、2025-11-26、2025-11-27三天的本地00:00:00的time_t。
 * 
 * @param last_time 上一次检查的时间戳（秒，time_t，UTC或系统本地时间）
 * @param now_time  当前时间戳（秒，time_t，UTC或系统本地时间）
 * @return tou_time_base_t 结构体
 *         - t_base_arr: 指向分配的time_t数组（需调用者释放）
 *         - days: 数组元素个数（天数，含首尾）
 *         若无有效天数，t_base_arr为NULL，days为0
 */
tou_time_base_t tou_time_get_base_arr(time_t last_time, time_t now_time);

/**
 * @brief 判断tou_time_base_t区间内是否存在TOU配置的有效触发点（高效且时区安全）
 * 
 * 本函数遍历tou_time_base_t中的每一天的本地time_sec时刻，避免时区误差和效率低下。
 * 适用于任意跨天、跨月、跨年、跨周等场景，且不会遗漏边界触发点。
 * 
 * @param tou_cfg   TOU时间配置结构体指针
 * @param base      tou_time_base_t结构体，包含区间内所有天的本地00:00:00 time_t数组及天数
 * @return int      1-达到触发条件，0-未达到
 */
int tou_time_is_trigger(const tou_time_config_t *tou_cfg, const tou_time_base_t *base);

/**
 * @brief 检查 TOU 时间配置是否无效
 *
 * 如果 cfg 为 NULL，或 month_sel/day_sel/week_sel 任一为 0，则认为配置无效。
 *
 * @param[in] cfg 指向 tou_time_config_t 的指针
 * @return int 1: 无效，0: 有效
 */
int tou_time_cfg_invalid(const tou_time_config_t *cfg);

/**
 * @brief 检查当前本地日期是否满足 TOU 配置中的“日/周/月”选择
 *
 * 本函数基于本地时间判断当天是否为配置中指定的有效日子。
 * - 使用本地 time() + localtime_r()，避免时区/夏令时偏差；
 * - 先校验配置有效性，再按月、日、周逐项验证；
 * - 仅判断日期匹配，不涉及具体时间点（time_sec）或区间比较。
 *
 * @param tou_cfg   指向 TOU 时间配置结构体的指针（不得为 NULL）
 * @return int      返回 1 表示“今天”满足配置；返回 0 表示不满足或参数/配置无效
 */
int tou_day_is_valid(const tou_time_config_t *tou_cfg);

#ifdef __cplusplus
}
#endif
