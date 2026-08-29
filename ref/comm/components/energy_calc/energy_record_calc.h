/**
 ***************************************************************************************************
 * @file    energy_record_calc.C
 * @version 1.0
 * @author  HJ
 * @date    2026/6/14
 * @brief   energy_record_calc.c's header file
 * @par     History
 * <table>
 * <tr><th>Date         <th>Version     <th>Author  <th>Description
 * <tr><td>2023/4/1     <td>1.0         <td>MXQ     <td>Create the initial version
 * </table>
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 * **************************************************************************************************
*/
/* Private includes (#include "xx.h")--------------------------------------------------------------*/
#ifndef ENERGY_RECORD_CALC
#define ENERGY_RECORD_CALC
/* Includes (#include"xx.h")------------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
/* Exported macros (#define)------------------------------------------------------------------------*/
/* Exported types (typedef)-------------------------------------------------------------------------*/
/* Exported function prototype declarations (extern)------------------------------------------------*/
/* Exported variable declarations (extern)----------------------------------------------------------*/
/* Exported constant declarations (extern const)----------------------------------------------------*/


// 计量基准宏，保持不变
#define WS_TO_100WH_UNIT        (3600 * 100UL)
#define ENERGY_ITEM_MAX_NUM     16

/**
 * @brief 系统各类支路累计能量统计结构体
 */
typedef struct
{
    uint32_t pv_charge_energy;          // PV总充电能量
    uint32_t grid_charge_energy;         // 电网总充电能量
    uint32_t feed_back_energy;          // 馈电电网能量
    uint32_t ac_load_energy;            // 交流负载总消耗能量
    uint32_t pv_to_load_energy;         // PV直接供给负载能量
    uint32_t pv_to_grid_energy;         // PV馈入电网能量
    uint32_t pv_to_bat_energy;          // PV给电池充电能量
    uint32_t grid_to_load_energy;       // 电网供给负载能量
    uint32_t grid_to_bat_energy;        // 电网给电池充电能量
    uint32_t bat_to_grid_energy;        // 电池放电馈入电网能量
    uint32_t bat_to_load_energy;        // 电池放电供给负载能量
    uint32_t bat_charge_energy;         // 电池总充电累计能量
    uint32_t bat_discharge_energy;      // 电池总放电累计能量
} System_Energy_InfoTypeDef;

/**
 * @brief 能量计算采集输入结构体：所有硬件原始采集参数
 */
typedef struct
{
    // 逆变器三相原始功率
    int32_t grid_detail_power[3];
    int32_t inv_detail_power[3];
    int32_t pv_detail_power[10];
    int16_t ac_load_detail_power[3]; /*3相离网负载*/

    // 逆变器辅助参数
    int32_t input_ac_load_power;
    int32_t ac_load_power;  /*总离网负载*/
    uint16_t Apparent_load_power[3];/*视在离网负载，暂时无作用*/
    uint8_t inv_work_state;

    // 电池PACK参数
    uint32_t pack_total_voltage;
    uint32_t pack_total_current;
    uint8_t bat_chg_state;  // 1充电 2放电 其余静置

    // 电网电表
    uint8_t grid_meter_flag;
    int32_t grid_meter_power[3];
    int32_t grid_meter_total_power;

    // 智能插座
    uint8_t smart_plug_flag;
    int32_t smart_plug_power;

    uint8_t grid_on_flag;
} EnergyCalc_InputTypeDef;


/**
 * @brief 各支路瞬时功率结构体（仅实时功率，无能量）
 */
typedef struct
{
    // PV总支路
    uint32_t pv_total_power;

    // 电网总支路
    uint32_t grid_charge_power;

    // 总馈电网支路
    uint32_t feed_in_grid_power;

    // PV分流支路
    uint32_t pv_to_load_power;
    int32_t pv_to_grid_power;
    int32_t pv_to_bat_power;

    // 电网分流支路
    int32_t grid_to_load_power;
    int32_t grid_to_bat_power;

    // 电池分流支路
    int32_t bat_to_grid_power;
    int32_t bat_to_load_power;
    uint32_t bat_charge_power;
    uint32_t bat_discharge_power;

    // 整机汇总辅助功率
    int32_t grid_total_power;
    int32_t inv_total_power;
    uint32_t family_load_power;
} System_Energy_PowerTypeDef;

/**
 * @brief 各支路累计能量结构体（仅积分能量，无瞬时功率）
 */
typedef struct
{
    // PV总支路
    uint32_t pv_charge_energy;

    // 电网总支路
    uint32_t grid_charge_energy;

    // 总馈电网支路
    uint32_t feed_back_energy;

    // 总负载功率
    uint32_t total_load_energy;

    // PV分流支路
    uint32_t pv_to_load_energy;
    uint32_t pv_to_grid_energy;
    uint32_t pv_to_bat_energy;

    // 电网分流支路
    uint32_t grid_to_load_energy;
    uint32_t grid_to_bat_energy;

    // 电池分流支路
    uint32_t bat_to_grid_energy;
    uint32_t bat_to_load_energy;
    uint32_t bat_charge_energy;
    uint32_t bat_discharge_energy;
} System_Energy_EnergyTypeDef;

/**
 * @brief 能量积分中间缓存：替换原cur_energy_statistic内部static静态变量
 */
typedef struct
{
    uint32_t power_Total_s[ENERGY_ITEM_MAX_NUM];
    uint32_t power_Total[ENERGY_ITEM_MAX_NUM];
    uint32_t power_delta_Total[ENERGY_ITEM_MAX_NUM];
} Energy_Integral_CacheTypeDef;

uint32_t Energy_Integral_CalcChannel(Energy_Integral_CacheTypeDef *cache,
                                            uint8_t index,
                                            int32_t input_power,
                                            uint32_t *total_energy);

// //void Energy_Integral_CalcAllChannel(Energy_Integral_CacheTypeDef *cache,System_Energy_PowerTypeDef *power_info,
//                                            System_Energy_EnergyTypeDef *energy_info);

void Energy_CalcSplitPower(const EnergyCalc_InputTypeDef *input_data, System_Energy_PowerTypeDef *power_info);

#endif
/*End of file------------------------------------------------------------------------*/

