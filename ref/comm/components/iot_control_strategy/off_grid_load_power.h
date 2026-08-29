/**
  ******************************************************************************
  * @file      off_grid_load_power.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/7/2
  * @brief     离网负载功率再分配
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/7/2   <td>1.0     <td>lixingyu   <td>Create the initial version
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

/* ================================ 头文件宏定义 ================================ */

/*离网负载功率调度模块测试使能*/
//#define OFF_GRID_LOAD_POWER_TEST

/*离网负载功率调度设备最大数量*/
#define OFF_GRID_LOAD_DEV_MAX    10

/* 单设备期望输出离网负载功率上限(W) */
#define OFF_GRID_DEV_MAX_POWER  1000

/* 单设备期望输出离网负载功率下限(W) */
#define OFF_GRID_DEV_MIN_POWER  0

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

/* 设备离网负载功率对象 */
typedef struct {
    uint16_t dev_type;              /* 设备类型序号 */
    uint64_t dev_sn;                /* 设备SN（序列号）*/
    uint8_t  dev_soc;               /* 设备SOC（电量百分比 0-100）*/
    int16_t  req_off_grid_power;    /* 设备当前所需离网负载功率（W），有符号 */
    int16_t  exp_off_grid_power;    /* 期望输出离网负载功率（W），有符号 */
    bool     sched_allowed;         /* 是否允许调度（true:允许参与离网负载功率调度）*/
} off_grid_load_dev_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 离网负载功率模块初始化：创建互斥信号量
 * @return true 成功；false 创建失败
 */
bool off_grid_load_power_init(void);

/**
 * @brief 记录/更新一条离网负载设备信息（按 dev_sn 匹配：存在则更新，不存在则写入空槽）
 * @note  更新已存在条目时，exp_off_grid_power（期望输出离网负载功率）由调度算法维护，
 *        不被本函数覆盖，保留原值；其余字段按 dev 更新。新增条目时按 dev 整体写入。
 * @param dev 设备信息指针（dev_sn 不能为 0）
 * @return true 成功；false 参数非法/取锁失败/表满
 */
bool off_grid_load_dev_upsert(const off_grid_load_dev_t *dev);

/**
 * @brief 从设备表删除指定设备（按 dev_sn 匹配）
 * @param sn 设备SN
 * @return true 找到并删除；false 未找到/取锁失败
 */
bool off_grid_load_dev_delete(uint64_t sn);

/**
 * @brief 离网负载功率主调度
 * @note  优先级：Σexp == sum_req（总量守恒） > soc 比例分配；[MIN,MAX] 为硬边界。
 *        步骤：
 *        1) 有效设备 = sched_allowed && dev_sn != 0；
 *           sum_req = Σ req_off_grid_power，sum_soc = Σ dev_soc；
 *        2) 按 soc 比例初分配 exp[i] = sum_req*dev_soc[i]/sum_soc
 *           （sum_soc==0 退化为均分），并先钳位到 [MIN,MAX]；
 *        3) 计算 deficit = sum_req - Σexp（钳位/取整导致偏差）；
 *           deficit>0：把缺口分给仍有上限余量(exp<MAX)的设备；
 *           deficit<0：把盈余从仍有下限余裕(exp>MIN)的设备收回；
 *           循环直至 deficit==0 或无可用余量（此时总需求超出[ΣMIN,ΣMAX]容量，
 *           属不可守恒边界情形，无法继续）。
 *        非参与调度设备 exp_off_grid_power 清 0。
 * @return 参与调度的有效设备数（0 表示无有效设备或取锁失败）
 */
uint8_t off_grid_load_power_schedule(void);

/**
 * @brief 获取指定SN设备的期望输出离网负载功率
 * @param sn 设备SN
 * @param exp_out 找到时写入 exp_off_grid_power
 * @return true 找到；false 未找到/参数非法/取锁失败
 */
bool off_grid_load_dev_get_exp(uint64_t sn, int16_t *exp_out);

/* ============================== 调试自测（可选） =============================== */

#ifdef OFF_GRID_LOAD_POWER_TEST
/**
 * @brief 模块非阻塞自测（仅在定义 OFF_GRID_LOAD_POWER_TEST 时编译）
 * @return 失败用例数（0=全通过；0xFF=初始化失败）
 */
uint8_t off_grid_load_power_self_test(void);
#endif

#ifdef __cplusplus
}
#endif

