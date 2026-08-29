#ifndef __GRID_OFF_STATISTIC_H__
#define __GRID_OFF_STATISTIC_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "parameter.h"

// 断电统计功能使能宏定义
#define GRID_OFF_STATISTIC_ENABLE         (1)                // 1-使能断电统计功能, 0-禁用断电统计功能
#define GRID_OFF_STATISTIC_DEBUG          (0)                // 1-使能断电统计调试功能, 0-禁用断电统计调试功能

// 文件存储相关宏定义
#define GRID_OFF_QUEUE_FLASH_FILENAME    "gridoff"           // 文件存储名称
#define GRID_OFF_QUEUE_MAX_CAPACITY       (100)              // 队列最大容量
#define GRID_OFF_BATCH_REPORT_MAX_COUNT   (100)              // 单次HTTP上报最大事件数量
#define GRID_OFF_QUEUE_MAGIC_NUMBER       (0x47524944)       // 'GRID' 魔数，用于数据校验

#pragma pack (1)

#if 0   // 外部定义，便于引用
// 电网断电次数按时间段统计结构体
typedef struct {
    uint32_t daily_count;       // 当日断电次数
    uint32_t weekly_count;      // 当周断电次数  
    uint32_t monthly_count;     // 当月断电次数
    uint32_t yearly_count;      // 当年断电次数
    uint32_t total_count;       // 至今总断电次数
    
    // 时间戳记录，用于判断是否需要清零统计
    uint64_t last_update_time;    // 上次更新的时间戳，使用uint64_t避免平台差异
} grid_off_count_statistics_t;

// 断电事件队列记录结构体（用于批量上报）
typedef struct {
    uint32_t start_time;        // 断电开始时间（使用32位时间戳，节省4字节）
    uint32_t end_time;          // 断电结束时间（使用32位时间戳，节省4字节）
    uint8_t need_report;        // 是否需要上报: 0=不需要上报, 1=需要上报
    uint8_t remark;             // 事件备注编号（预留）
} power_off_record_t;
#endif

// 时间跨度检查结果结构体
typedef struct {
    bool same_year;
    bool same_month;
    bool same_week;
    bool same_day;
} time_span_check_result_t;

// 断电事件队列管理结构体
typedef struct {
    uint32_t magic_number;                          // 魔数，用于数据校验
    uint8_t version;                                // 数据结构版本号
    uint8_t head_index;                             // 队列头索引
    uint8_t tail_index;                             // 队列尾索引
    uint8_t count;                                  // 当前记录数量
    power_off_record_t records[GRID_OFF_QUEUE_MAX_CAPACITY];  // 最多存储100条记录
    uint32_t checksum;                              // 数据校验和（移到最后，删除reserved字段）
} power_off_queue_t;

#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化断电统计模块
 */
void Grid_Off_Statistic_Init(void);

/**
 * @brief 统计电网断电次数主函数
 * @note 仅上电30s后开始运行，需要记录当日，当月，当周，当年和至今的断电次数，需要保存到flash中
 */
void Grid_Off_Event_Task(void);

/**
  * @brief      断电数据上报任务
  */
uint8_t Grid_Off_Report_Task(void);

/**
 * @brief 根据时间跨度设置获取对应时间段的断电次数
 * @return 断电次数
 */
uint32_t Get_Grid_Off_Count_By_Timespan(void);

#ifdef __cplusplus
}
#endif

#endif /* __GRID_OFF_STATISTIC_H__ */
