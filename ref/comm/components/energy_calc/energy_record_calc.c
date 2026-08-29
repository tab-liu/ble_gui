/**
 ***************************************************************************************************
 * @file    energy_record_calc.C
 * @version 1.0
 * @author  HJ
 * @date    2026/6/14
 * @brief   energy_record_calc.c's source file
 * @par     History
 * <table>
 * <tr><th>Date         <th>Version     <th>Author  <th>Description
 * <tr><td>2023/4/1     <td>1.0         <td>MXQ     <td>Create the initial version
 * </table>
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 * **************************************************************************************************
*/
/* Private includes (#include "xx.h")--------------------------------------------------------------*/
#include "esp_log.h"
#include "dev_modbus_meter_data_handler.h"
#include "dev_modbus_other_inv_data_handler.h"
#include "disater_warn_simple.h"
#include "wlcc_protocol.h"
#include "dev_modbus_other_inv_data_handler.h"
#include "wlcc_interface.h"
#include "wlcc_process.h"
#include "general_conversions.h"
#include "energy_record_calc.h"s
#include "energy_process.h"
/* Private macros (#define)-----------------------------------------------*/
#define TAG "[energy_record_calc]"

#define POWER_FILTER_THRESHOLD 20

/**
 * @brief 单支路能量积分计算，沿用瓦秒累计溢出逻辑
 * @param cache 积分缓存结构体
 * @param index 支路编号
 * @param input_power 当前瞬时功率（可正负）
 * @param total_energy 外部累计能量指针
 * @return 该支路当前总100Wh计数快照
 */
uint32_t Energy_Integral_CalcChannel(Energy_Integral_CacheTypeDef *cache,
                                            uint8_t index,
                                            int32_t input_power,
                                            uint32_t *total_energy)
{
    uint32_t add_avl = 0;
    uint32_t abs_pow = (input_power >= 0) ? (uint32_t)input_power : (uint32_t)(-input_power);

    cache->power_Total_s[index] += abs_pow;
    if (cache->power_Total_s[index] >= WS_TO_100WH_UNIT)
    {
        cache->power_Total_s[index] -= WS_TO_100WH_UNIT;
        cache->power_Total[index] += 1U;
    }

    if (cache->power_delta_Total[index] < cache->power_Total[index])
    {
        add_avl = cache->power_Total[index] - cache->power_delta_Total[index];
        *total_energy += add_avl;
        cache->power_delta_Total[index] = cache->power_Total[index];
    }

    ESP_LOGI(TAG, "Energy index:%d, input_power:%ld, energy:%lu", index, input_power, *total_energy);
    return cache->power_delta_Total[index];
}


/**
 * @brief 功率分流纯逻辑计算，输入采集原始参数，填充统一结构体所有瞬时功率
 * @param input_data 硬件采集原始输入参数
 * @param power_info 输出：统一功率能量结构体，仅更新内部所有power字段
 */
void Energy_CalcSplitPower(const EnergyCalc_InputTypeDef *input_data, System_Energy_PowerTypeDef *power_info)
{
    uint32_t pv_total_power = 0;
    int32_t grid_total_power = 0;
    int32_t inv_total_power = 0;
    uint32_t family_load_power = 0;
    uint32_t feed_in_grid_power = 0;
    uint32_t bat_charge_power = 0;
    uint32_t bat_discharge_power = 0;

    uint32_t pv_to_load_power = 0;
    int32_t pv_to_grid_power = 0;
    int32_t pv_to_bat_power = 0;
    int32_t grid_to_load_power = 0;
    int32_t grid_to_bat_power = 0;
    int32_t bat_to_load_power = 0;
    int32_t bat_to_grid_power = 0;

    // 1、汇总PV、电网、逆变三相总功率
    for (int i = 0; i < 10; i++)
    {
        pv_total_power += input_data->pv_detail_power[i];
    }
    grid_total_power = input_data->grid_detail_power[0] + input_data->grid_detail_power[1] + input_data->grid_detail_power[2];
    inv_total_power = input_data->inv_detail_power[0] + input_data->inv_detail_power[1] + input_data->inv_detail_power[2];

    // 2、计算电池实时功率
    uint32_t cur_pack_power = ((uint32_t)input_data->pack_total_voltage * (uint32_t)input_data->pack_total_current) / 1000;

    // 3、计算家庭总负载功率（区分电表/智能插座场景）
    if (input_data->grid_meter_flag)/*电表在线*/
    {
        int32_t grid_meter_total_power = input_data->grid_meter_total_power;
        if (grid_meter_total_power < 0)/*电网电表输入功率  实际为正，内部转换为负值*/
        {
            if ((inv_total_power + input_data->input_ac_load_power) > 0)/*设备有输出功率 ,设备和电网共同供负载*/
            {
                family_load_power = input_data->input_ac_load_power + inv_total_power + abs(grid_meter_total_power);
            }
            else/*设备为负，电网电表视为负载供电，电表功率已包含该逆变功率*/
            {
                if (abs(grid_meter_total_power) > abs(inv_total_power + input_data->input_ac_load_power))
                    family_load_power = abs(grid_meter_total_power) - abs(inv_total_power + input_data->input_ac_load_power);
                else
                    family_load_power = abs(input_data->ac_load_power);
            }
        }
        else/*设备馈电电网 或0馈电*/
        {
            if (((inv_total_power + input_data->input_ac_load_power) > POWER_FILTER_THRESHOLD) && ((inv_total_power + input_data->input_ac_load_power) > grid_meter_total_power))
                family_load_power = abs(inv_total_power + input_data->input_ac_load_power - grid_meter_total_power);
            else/*逆变功率为负 负载功率为0*/
                family_load_power = abs(input_data->ac_load_power);

            feed_in_grid_power = grid_meter_total_power;
        }
    }
    else if (input_data->smart_plug_flag)/*有智能插座&&电网在线*/
    {
        family_load_power = abs(input_data->ac_load_power) + input_data->smart_plug_power;/*本地负载功率+智能插座功率*/
    }
    else
    {
        family_load_power = abs(input_data->ac_load_power);
    }

    // 4、电池充电分支 bat_chg_state == 1
    if (input_data->bat_chg_state == 1)
    {
        bat_to_load_power = 0;//电池充电，无电池带载功率
        bat_to_grid_power = 0;//电池充电，无电池馈电网功率
        bat_charge_power = cur_pack_power;

        if (pv_total_power >= bat_charge_power)
        {
            //grid_charge_power=0;/*光伏满足全部电池充电功率*/
            pv_to_bat_power = bat_charge_power;
            grid_to_bat_power = 0;
            uint32_t cut_power = pv_total_power - bat_charge_power;
            //ESP_LOGI(TAG,"pv_total_power(%ld)-bat_charge_power(%ld)= CUT_POWER:%lu",pv_total_power,bat_charge_power,cut_power);

            if (input_data->grid_meter_flag)/*存在电表*/
            {
                if (family_load_power)//存在家庭负载功率，光伏余量供电负载
                {
                    if (cut_power >= family_load_power)
                    {
                        pv_to_load_power = family_load_power;//pv余量多于家庭负载功率，pv供电负载功率即为家庭负载功率
                        grid_to_load_power = 0;//无电网带载
                        uint16_t val = cut_power - family_load_power;
                        //ESP_LOGI(TAG,"11 val:%u",val);
                        if (feed_in_grid_power > 0)
                        {
                            if (val < feed_in_grid_power)
                            {
                                pv_to_grid_power = val;//光伏馈电网功率
                            }
                            else
                            {
                                pv_to_grid_power = feed_in_grid_power;
                            }
                        }
                    }
                    else
                    {
                        pv_to_load_power = cut_power;//pv余量少于家庭负载功率，pv带载功率为pv余量功率
                        pv_to_grid_power = 0;//光伏功率全部输出完
                        if (input_data->grid_meter_total_power < 0)
                        {
                            grid_to_load_power = family_load_power - cut_power;
                        }

                    }
                }
                else
                {
                    pv_to_load_power = 0;
                    grid_to_load_power = 0;
                    if (feed_in_grid_power > 0)/*存在馈电市电工况*/
                    {
                        if (cut_power > feed_in_grid_power)
                        {
                            pv_to_grid_power = feed_in_grid_power;//光伏馈电网功率
                        }
                        else
                        {
                            pv_to_grid_power = cut_power;
                        }
                    }
                }
            }
            else/*电表不在线*/
            {
                if (family_load_power > 0)/*存在家庭负载功率*/
                {
                    if (cut_power > family_load_power)
                    {
                        pv_to_load_power = family_load_power;/*光伏余量满足家庭负载，光伏带载功率取全部负载功率*/
                        pv_to_grid_power = cut_power - family_load_power;/*光伏去掉充电、带载功率后，余量为光伏馈电功率*/
                        grid_to_load_power = 0;
                    }
                    else
                    {
                        pv_to_load_power = cut_power;/*光伏带载部分功率为光伏全部余量*/
                        pv_to_grid_power = 0;
                        grid_to_load_power = family_load_power - cut_power;/*剩余负载由电网带载*/
                    }
                }
                else/*不存在负载*/
                {
                    pv_to_load_power = 0;/*光伏不带载*/
                    pv_to_grid_power = cut_power;/*光伏余量馈市电电网*/
                    grid_to_load_power = 0;/*电网不带载*/
                }
            }
        }
        else/*光伏功率不满足全部充电功率*/
        {
            pv_to_bat_power = pv_total_power;//光伏充电功率
            pv_to_load_power = 0;//光伏带载功率为0
            pv_to_grid_power = 0;
            uint32_t cut_val = bat_charge_power - pv_total_power;/*去掉光伏充电部分后的余量*/
            ESP_LOGI(TAG,"bat_charge_power(%ld)-pv_total_power(%ld)=cut_val(%lu)",bat_charge_power,pv_total_power,cut_val);
            //grid_to_bat_power=bat_charge_power-pv_total_power;//电网充电功率 ,当前不考虑AC耦合功率

            if (input_data->grid_meter_flag)
            {
                int32_t cur_grid_charge_val = (-input_data->grid_meter_total_power);
                //ESP_LOGI(TAG,"cur_grid_charge_val:%ld",cur_grid_charge_val);
                if (cur_grid_charge_val > 0)/*存在市电电网功率输入*/
                {
                    if (cur_grid_charge_val >= cut_val)
                    {
                        grid_to_bat_power = cut_val;/*市电输入功率完全满足剩余充电功率*/
                    }
                    else
                    {
                        grid_to_bat_power = cur_grid_charge_val;
                        pv_to_bat_power += (cut_val - cur_grid_charge_val);/*判断存在部分ac耦合功率参与设备充电，该部分功率算作光伏功率*/
                    }
                }
                else
                {
                    pv_to_bat_power += cut_val;/*判断存在部分ac耦合功率参与设备充电，该部分功率算作光伏功率*/
                }
                if (family_load_power > 0)//
                    grid_to_load_power = family_load_power;
                else
                    grid_to_load_power = 0;

            }
            else/*电表不在线*/
            {
                grid_to_bat_power = cut_val;/*剩余充电功率由电网充电*/
                if (family_load_power)
                {
                    int32_t val = (-grid_total_power);
                    ESP_LOGI(TAG,"333 val:%ld",val);
                    if (val > grid_to_bat_power)
                        grid_to_load_power = val - grid_to_bat_power;/*设备电网端口（非市电）输入功率多于电网充电功率，取两者相减余量*/
                    else
                        grid_to_load_power = 0;/*设备电网端口充电功率均用于电池充电*/
                }
                else
                {
                    grid_to_load_power = 0;
                }
            }
        }
    }
    // 5、电池放电分支 bat_chg_state == 2
    else if (input_data->bat_chg_state == 2)
    {
        pv_to_bat_power = 0;  //光伏充电功率为0
        grid_to_bat_power = 0;    //电网充电功率为0
        bat_discharge_power = cur_pack_power;
        if (input_data->grid_meter_flag)/*电表在线*/
        {
            if (family_load_power > 0)/*存在家庭负载*/
            {
                if (bat_discharge_power > family_load_power)/*电池放电功率多于家庭负载功率*/
                {
                    bat_to_load_power = family_load_power;/*电池带载功率取总家庭负载*/
                    uint16_t val = bat_discharge_power - family_load_power;/*电池放电余量*/
                    ESP_LOGI(TAG,"222 VAL:%u",val);
                    pv_to_load_power = 0;/*pv功率判断未使用于家庭负载*/
                    grid_to_load_power = 0;/*电网功率判断未使用于负载*/
                    if (feed_in_grid_power > 0)/*存在电网馈电*/
                    {
                        if (val > feed_in_grid_power)/*电池放电余量大于馈电功率*/
                        {
                            bat_to_grid_power = feed_in_grid_power;/*电池馈电功率取总的电网馈电功率*/
                            pv_to_grid_power = 0;
                        }
                        else
                        {
                            bat_to_grid_power = val;/*电池馈电网功率取电池带载余量*/
                            if (pv_total_power > 0)
                            {
                                uint32_t gap = feed_in_grid_power - val;
                                if (pv_total_power > gap)
                                    pv_to_grid_power = gap;
                                else
                                    pv_to_grid_power = pv_total_power;
                            }
                        }
                    }
                }
                else/*电池放电功率不能完全满足家庭负载*/
                {
                    bat_to_load_power = bat_discharge_power;/*电池带载功率为当前电池全部放电功率*/
                    bat_to_grid_power = 0;/*无电池馈电网功率*/
                    uint32_t cut_val = family_load_power - bat_discharge_power;/*去掉电池带载部分功率取剩余负载余量*/
                    ESP_LOGI(TAG,"family_load_power(%lu)-bat_discharge_power(%lu)=cut_val(%lu)",family_load_power,bat_discharge_power,cut_val);
                    if (pv_total_power > 0)/*存在光伏功率*/
                    {
                        if (pv_total_power > cut_val)/*光伏功率多于剩余家庭负载功率*/
                        {
                            pv_to_load_power = cut_val;/*光伏功率多于剩余负载功率，光伏带载功率仅取负载余量*/
                            grid_to_load_power = 0;
                            if (feed_in_grid_power > 0)
                            {
                                uint32_t pv_cut_val = pv_total_power - pv_to_load_power;/*减去光伏带载功率，获取剩余光伏功率*/
                                ESP_LOGI(TAG,"111 pv_cut_val:%lu",pv_cut_val);
                                if (pv_cut_val >= feed_in_grid_power)
                                {
                                    pv_to_grid_power = feed_in_grid_power;/*余量多于或等于馈电网功率，则光伏馈电网功率取总馈电网功率*/
                                }
                                else
                                {
                                    pv_to_grid_power = pv_cut_val;/*余量小于馈电网功率，光伏馈电网功率取余量*/
                                }

                            }
                        }
                        else/*光伏功率不能满足全部剩余家庭负载功率*/
                        {
                            pv_to_load_power = pv_total_power;
                            pv_to_grid_power = 0;/*光伏馈电功率为0*/
                            if (input_data->grid_meter_total_power < 0)
                            {
                                int32_t cur_meter_power = -(input_data->grid_meter_total_power);
                                //ESP_LOGI(TAG,"cur_meter_power:%ld",cur_meter_power);
                                uint32_t load_gap = cut_val - pv_total_power;
                                if (cur_meter_power >= load_gap)
                                {
                                    /*电网实际输入功率大于或等于负载余量（减去pv和bat的放电部分），电网带载功率仅取余量部分*/
                                    grid_to_load_power = load_gap;
                                }
                                else
                                {
                                    /*电网输入功率小于负载余量，判断可能存在其他逆变器放电，仅取电表检测的电网输入功率*/
                                    grid_to_load_power = cur_meter_power;
                                }
                            }

                        }
                    }
                    else
                    {
                        pv_to_load_power = 0;
                        pv_to_grid_power = 0;
                        if (abs(grid_total_power) > cut_val)
                        {
                            grid_to_load_power = cut_val;
                        }
                        else
                        {
                            grid_to_load_power = grid_total_power;
                        }
                    }
                }

            }
            else
            {
                pv_to_load_power = 0;
                grid_to_load_power = 0;
                bat_to_load_power = 0;
                if (feed_in_grid_power > 0)
                {
                    if (pv_total_power > 0)
                    {
                        if (pv_total_power > feed_in_grid_power)
                        {
                            pv_to_grid_power = feed_in_grid_power;
                            bat_to_grid_power = 0;
                        }
                        else
                        {
                            pv_to_grid_power = pv_total_power;
                            bat_to_grid_power = feed_in_grid_power - pv_total_power;
                        }
                    }
                    else
                    {
                        pv_to_grid_power = 0;
                        bat_to_grid_power = feed_in_grid_power;
                    }
                }
                else
                {
                    if (pv_total_power > 0)
                    {
                        pv_to_load_power = pv_total_power;
                    }
                    bat_to_load_power = bat_discharge_power;

                }
            }
        }
        else/*电表未在线*/
        {
            if (family_load_power > 0)/*存在离网本地负载*/
            {
                if (bat_discharge_power > family_load_power)
                {
                    /*电池放电功率多于负载功率*/
                    bat_to_load_power = family_load_power;
                    bat_to_grid_power = bat_discharge_power - family_load_power;
                    grid_to_load_power = 0;
                    pv_to_load_power = 0;
                    if(pv_total_power>0)
                        pv_to_grid_power=pv_total_power;
                    else
                        pv_to_grid_power=0;

                }
                else
                {
                    /*电池放电功率不能完全满足负载功率*/
                    bat_to_load_power = bat_discharge_power;/*电池功率全部带载*/
                    bat_to_grid_power = 0;
                    uint32_t cut_val = family_load_power - bat_discharge_power;/*去除电池带载功率部分后，剩余功率由其他能源提供*/
                    if (pv_total_power > 0)
                    {
                        if (pv_total_power > cut_val)/*光伏总功率多于剩余带载功率*/
                        {
                            pv_to_load_power = cut_val;
                            pv_to_grid_power = 0;
                            grid_to_load_power = 0;
                        }
                        else
                        {
                            pv_to_load_power = pv_total_power;
                            pv_to_grid_power = 0;
                            grid_to_load_power = cut_val - pv_to_load_power;
                        }
                    }
                }
            }
            else/*不存在负载（无电表时为离网本地负载）*/
            {
                bat_to_load_power = 0;
                bat_to_grid_power = bat_discharge_power;
                pv_to_load_power = 0;
                grid_to_load_power = 0;
                if (pv_total_power)
                    pv_to_grid_power = pv_total_power;/*光伏所有功率全部馈电网*/
                else
                    pv_to_grid_power = 0;
            }

        }
    }
    // 6、电池静置 非充非放
    else
    {
        /*非充非放阶段，电池无放电，光伏、电网不充电 */
        bat_to_grid_power = 0;
        bat_to_load_power = 0;
        pv_to_bat_power = 0;
        grid_to_bat_power = 0;
        if (input_data->grid_meter_flag)/*电表在线*/
        {
            if (family_load_power > 0)/*存在家庭负载功率*/
            {
                if (pv_total_power > 0)/*存在光伏功率*/
                {
                    if (pv_total_power >= family_load_power)
                    {
                        pv_to_load_power = family_load_power;/*光伏带载功率满足所有的家庭负载功率需求*/
                        uint32_t cut_val = pv_total_power - family_load_power;
                        ESP_LOGI(TAG,"pv_total_power(%ld)-family_load_power(%lu)=vut_val(%lu)",pv_total_power,family_load_power,cut_val);
                        grid_to_load_power = 0;/*市电电网不带载*/
                        if (feed_in_grid_power > 0)
                        {
                            if (cut_val > feed_in_grid_power)
                                pv_to_grid_power = feed_in_grid_power;
                            else
                                pv_to_grid_power = cut_val;
                        }
                    }
                    else
                    {
                        pv_to_load_power = pv_total_power;/*光伏带载功率不能满足总家庭负载功率，光伏带载功率取总光伏功率*/
                        grid_to_load_power = family_load_power - pv_total_power;/*剩余家庭负载由市电电网带载*/
                    }
                }
                else
                {
                    pv_to_grid_power = 0;/*无光伏馈电市电*/
                    if (input_data->grid_meter_total_power < 0)/*存在市电电网输入*/
                    {
                        grid_to_load_power = (-input_data->grid_meter_total_power);/*所有市电输入功率均为市电电网带载功率*/
                    }
                    else
                    {
                        grid_to_load_power = 0;/*无市电电网功率输入，推测家庭有其他逆变发电，市电电网带载功率为0*/
                    }
                }
            }
            else
            {
                pv_to_load_power = 0;
                grid_to_load_power = 0;
                if ((pv_total_power > 0) && (feed_in_grid_power > 0))
                {
                    if (pv_total_power > feed_in_grid_power)
                        pv_to_grid_power = feed_in_grid_power;
                    else
                        pv_to_grid_power = pv_total_power;
                }
            }
        }
        else
        {
            if (family_load_power)
            {
                if (pv_total_power > 0)
                {
                    if (pv_total_power > family_load_power)
                    {
                        pv_to_load_power = family_load_power;/*光伏带载全部负载*/
                        pv_to_grid_power = pv_total_power - pv_to_load_power;/*剩余光伏余量馈电网*/
                        grid_to_load_power = 0;/*电网不带载*/
                    }
                    else
                    {
                        pv_to_load_power = pv_total_power;/*全部光伏功率均带负载*/
                        pv_to_grid_power = 0;
                        grid_to_load_power = family_load_power - pv_to_load_power;/*电网带载剩余负载*/
                    }
                }
                else
                {
                    pv_to_load_power = 0;
                    pv_to_grid_power = 0;
                    grid_to_load_power = family_load_power;/*电网带载全部负载*/
                }
            }
            else
            {
                grid_to_load_power = 0;
                pv_to_load_power = 0;
                if (pv_total_power > 0)
                    pv_to_grid_power = pv_total_power;/*光伏全部功率均馈电网*/
                else
                    pv_to_grid_power = 0;
            }
        }
    }

    // 汇总电网总输入功率
    uint32_t grid_charge_power = grid_to_load_power + grid_to_bat_power;
    feed_in_grid_power = bat_to_grid_power + pv_to_grid_power;
    ESP_LOGI(TAG,"pv_to_load_power:%lu,pv_to_grid_power:%lu,pv_to_bat_power:%lu,grid_to_load_power:%lu,grid_to_bat_power:%lu,bat_to_load_power:%lu,bat_to_grid_power:%lu,grid_charge_power:%lu,bat_discharge_power:%lu,bat_charge_power:%lu,feed_in_grid_power:%lu",
        pv_to_load_power,pv_to_grid_power,pv_to_bat_power,grid_to_load_power,grid_to_bat_power,
        bat_to_load_power,bat_to_grid_power,grid_charge_power,bat_discharge_power,bat_charge_power,feed_in_grid_power);

    // 全部功率输出赋值到统一结构体
    power_info->pv_total_power = pv_total_power;
    power_info->grid_total_power = grid_total_power;
    power_info->inv_total_power = inv_total_power;
    power_info->family_load_power = family_load_power;
    power_info->feed_in_grid_power = feed_in_grid_power;
    power_info->bat_charge_power = bat_charge_power;
    power_info->bat_discharge_power = bat_discharge_power;
    power_info->grid_charge_power = grid_charge_power;

    power_info->pv_to_load_power = pv_to_load_power;
    power_info->pv_to_grid_power = pv_to_grid_power;
    power_info->pv_to_bat_power = pv_to_bat_power;
    power_info->grid_to_load_power = grid_to_load_power;
    power_info->grid_to_bat_power = grid_to_bat_power;
    power_info->bat_to_load_power = bat_to_load_power;
    power_info->bat_to_grid_power = bat_to_grid_power;
}