/**
 ***************************************************************************************************
 * @file    DisasterWarn.h
 * @version 1.0
 * @author  XXY
 * @date   
 * @brief   
 * @par     History
 * <table>
 * <tr><th>Date         <th>Version     <th>Author  <th>Description
 * <tr><td>2023/4/1     <td>1.0         <td>XXY     <td>Create the initial version
 * </table>
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 * **************************************************************************************************
*/
/* Private includes (#include "xx.h")--------------------------------------------------------------*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "DisasterWarn.h"
#include "can_data.h"
#include "can_type.h"
//#include "ParllelEMS.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
//#include "kvdb_eep.h"
#include "ll_param.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "http_client.h"
#include "utils.h"
#include "comm_define.h"
#include "iot_mqtt.h"
#include "can_protocol.h"
#include "iot_period_task.h"
#include "can_data.h"
// #include <uuid/uuid.h>
// #include <time.h>

// void generate_uuid(char *uuid_str, size_t size) {
//     srand((unsigned int)time(NULL));
//     // for (size_t i = 0; i < size - 1; ++i) {
//     //     uuid_str[i] = char[rand() % 16];
//     // }
//     // uuid_str[size - 1] = '\0'; // 确保字符串以空字符结尾
//     snprintf(uuid_str, size, "%08x", rand());
// }

#define TAG "[DisasterWarn]"
// static SemaphoreHandle_t disasterJudgeMutex = NULL; //申明互斥型信号量，在FreeRTOS中二值型信号量和互斥型信号量类型完全相同。
EXT_RAM_ATTR uint8_t power_on_flag = 0;
EXT_RAM_ATTR disaster_save_param_t DisasterSaveData = {0};
EXT_RAM_ATTR _StormWatchSetData StormWatchSetData = {0}; //实时状态，掉电丢失
// EXT_RAM_ATTR _StormWatchSetData StormWatchSetData = {0}; //实时状态，掉电丢失
EXT_RAM_ATTR static disaster_save_param_t originalDisasterSaveData = {0}; // 保存原始状态
EXT_RAM_ATTR uint8_t Disaster_Warm_data[255]={0};		
// extern uint8_t iot_mqtt_login_state(void);

static uint8_t s_retry_count = 0;
static uint8_t s_retry_begin_count = 0;
static bool s_reset_on = false;
static  storm_end_reason_t end_reason = { .all = 0 };
static SemaphoreHandle_t s_disaster_mutex = NULL;
static uint32_t s_last_total_chg_energy = 0; // 记录汇总充电能量的上一次采样值

static uint32_t storm_vaild_flag = 0;
static uint8_t get_thunder_info_flag = 0;
_SaveWebStormWatchApiData WebStormData_Save;
static char cur_msgid[100]={0};

void SetStormData(void* SetData1);
static bool is_device_supported(void);
static bool check_prerequisites(void);
static void handle_storm_watch_end(time_t current_time, uint16_t soc);
static void handle_storm_watch_active(time_t current_time, uint16_t soc);

/*------------------------------------------------------------------------
*@Function：Get_Power_flag*
-------------------------------------------------------------------------*/
/**
*@brief 上电标志
*@param[in]     None
*@param[out]    None
*@return        void
*/
void set_power_flag(void)
{
   power_on_flag = 1;
}
uint8_t Get_Power_flag(void)
{
   return power_on_flag;
}

void send_start_param(void)
{
    DisasterSaveData.disaster_send_param.uuid += 1; // 每发一次uuid+1处理
    DisasterSaveData.disaster_send_param.soc_start = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc;
    DisasterSaveData.disaster_send_param.send_flag.bit.end = 0;
    
    // 记录备电开始时的总能量汇总值作为期初
    DisasterSaveData.disaster_send_param.total_chg_energy_begin = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy;
    // 初始化增量统计的参考基准
    s_last_total_chg_energy = DisasterSaveData.disaster_send_param.total_chg_energy_begin;
    // 期末统计值初始等于期初值，后续通过增量累加
    DisasterSaveData.disaster_send_param.total_chg_energy_end = DisasterSaveData.disaster_send_param.total_chg_energy_begin;

    ESP_LOGI(TAG,"send_start_param soc  begin:%d  end:%d cur:%d",DisasterSaveData.disaster_send_param.soc_start,DisasterSaveData.disaster_send_param.soc_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc);
    ESP_LOGI(TAG,"send_start_param total_chg_energy  begin:%lu  end:%lu cur:%lu",DisasterSaveData.disaster_send_param.total_chg_energy_begin,DisasterSaveData.disaster_send_param.total_chg_energy_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy);
}

void send_end_param(void)
{
    DisasterSaveData.disaster_send_param.uuid += 1; // 每发一次uuid+1处理
    DisasterSaveData.disaster_send_param.soc_end = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc;
    DisasterSaveData.disaster_send_param.send_flag.bit.start = 0;
    
    // 确保期末不小于期初
    if(DisasterSaveData.disaster_send_param.total_chg_energy_end < DisasterSaveData.disaster_send_param.total_chg_energy_begin)
    {
        DisasterSaveData.disaster_send_param.total_chg_energy_end = DisasterSaveData.disaster_send_param.total_chg_energy_begin;
    }

    ESP_LOGI(TAG,"send_end_param soc  begin:%d  end:%d cur:%d",DisasterSaveData.disaster_send_param.soc_start,DisasterSaveData.disaster_send_param.soc_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc);
    ESP_LOGI(TAG,"send_end_param total_chg_energy  begin:%lu  end:%lu cur:%lu",DisasterSaveData.disaster_send_param.total_chg_energy_begin,DisasterSaveData.disaster_send_param.total_chg_energy_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy);
}

void data_kill(void)
{
    DisasterSaveData.disaster_send_param.soc_start = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc;
    DisasterSaveData.disaster_send_param.soc_end = DisasterSaveData.disaster_send_param.soc_start;
    DisasterSaveData.disaster_send_param.send_flag.status = 0;
    DisasterSaveData.disaster_send_param.total_chg_energy_begin = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy;
    DisasterSaveData.disaster_send_param.total_chg_energy_end = DisasterSaveData.disaster_send_param.total_chg_energy_begin;
    
    StormWatchSetData.picture.bit.recv_state = 0;
    StormWatchSetData.StormWatchState = STORM_STATE_INACTIVE;
    StormWatchSetData.StormWatchStateSingle = STORM_STATE_INACTIVE;
    DisasterSaveData.AppCancelStormEvent.EventEndFlag = 0;
}

bool UpdateDisasterSaveDataFromParam(void)
{
    StormWatchSetData.StormWatchEnable=SetData.dev_info_t.thunder_ctrl.thunder_enable;
    disaster_save_param_t *disaster_param = ll_iot_param_get("disaster_save_param");
    if (disaster_param != NULL) {
        memcpy(&DisasterSaveData, disaster_param, sizeof(disaster_save_param_t));
        return true;
    } else {
        ESP_LOGE(TAG, "Error: Failed to get disaster_save_param");
        return false;
    }
}

/*------------------------------------------------------------------------
*@Function：StormWatchEndProcess
-------------------------------------------------------------------------*/
/**
*@brief 风暴观察结束处理
*@param[in]     time_t current_time,  当前系统时间
                uint16_t system_soc, 系统平均SOC
                storm_end_reason_t end_reason 结束原因
*@param[out]    None
*@return        void
*/
void StormWatchEndProcess(time_t current_time, uint16_t system_soc, storm_end_reason_t end_reason)
{
    ESP_LOGI(TAG,"StormWatchSetData.StormWatchState:%d",StormWatchSetData.StormWatchState);
    if (StormWatchSetData.StormWatchState == STORM_STATE_ACTIVE || StormWatchSetData.StormWatchState == STORM_STATE_INACTIVE) {
        if (s_reset_on && DisasterSaveData.disaster_send_param.send_flag.bit.end == 1) {
            s_reset_on = false;
            ESP_LOGI(TAG, "System reset during storm watch, sending end data");
        } else {
            DisasterSaveData.disaster_send_param.BackupEndTime = current_time;
            send_end_param();
            SetStormData(&DisasterSaveData);
            ESP_LOGI(TAG, "Storm watch ended");
            s_retry_count = 0;
        }
        StormWatchSetData.StormWatchState = STORM_STATE_ENDED;
        StormWatchSetData.StormWatchStateSingle = STORM_STATE_ENDED;
    }
    if( StormWatchSetData.picture.bit.recv_state == 1){
        StormWatchSetData.StormWatchState = STORM_STATE_ENDED;
        StormWatchSetData.StormWatchStateSingle = STORM_STATE_ENDED;
    }

    
    // 尝试发送结束数据到主机
    ESP_LOGI(TAG,"END:%d,s_retry_count:%d",DisasterSaveData.disaster_send_param.send_flag.bit.end,s_retry_count);
    if (!DisasterSaveData.disaster_send_param.send_flag.bit.end && 
        reals.wifi_connect_ap && 
        s_retry_count < MAX_RETRY_COUNT) {
        s_retry_count++;
        
        memcpy(&StormWatchSetData.StormSendParam, 
               &DisasterSaveData.disaster_send_param, 
               sizeof(disaster_send_param_t));
        disaster_post_args_t post_args = {
            .StormSendParam = StormWatchSetData.StormSendParam,
            .send_type = 2,
            .end_reason = end_reason
        };
        DisasterSaveData.disaster_send_param.send_flag.bit.end = 
        https_disater_state_to_host(&post_args);
        DisasterSaveData.disaster_send_param.send_flag.bit.start = 0;
        
        if (DisasterSaveData.disaster_send_param.send_flag.bit.end) {
            SetStormData(&DisasterSaveData);
        }
        ESP_LOGI(TAG, "End post attempt: %d,end:%d", s_retry_count,DisasterSaveData.disaster_send_param.send_flag.bit.end);
    }
    
    // 处理停止标志：SOC达到目标后记为stop=2，供回差再入备电判断
    if (DisasterSaveData.disaster_send_param.send_flag.bit.stop == 1 && 
        system_soc >= DisasterSaveData.SaveWebStormWatchApiData.SoC) {
        DisasterSaveData.disaster_send_param.send_flag.bit.stop = 2;
        SetStormData(&DisasterSaveData);
    }
}

/*------------------------------------------------------------------------
*@Function：StormWatchDataDeal*
-------------------------------------------------------------------------*/
/**
*@brief 处理活跃的风暴观察状态
*@param[in]     time_t current_time, 
                uint16_t system_soc
*@param[out]    None
*@return        void
*/
static void handle_storm_watch_active(time_t current_time, uint16_t system_soc)
{
	/* 已因SOC达标退出过备电(stop==2)：仅当SOC回落到目标-5%及以下才再次进入备电；
	 * 首次进入不受该回差限制 */
	uint16_t target_soc = DisasterSaveData.SaveWebStormWatchApiData.SoC;
	uint16_t soc_threshold = (target_soc > 5) ? (uint16_t)(target_soc - 5) : 0;
	if (DisasterSaveData.disaster_send_param.send_flag.bit.stop == 2 &&
		system_soc > soc_threshold) {
		ESP_LOGI(TAG, "SOC hysteresis hold: soc:%u thr:%u target:%u stop:%d",
				system_soc, soc_threshold, target_soc,
				DisasterSaveData.disaster_send_param.send_flag.bit.stop);
		return;
	}
    
    StormWatchSetData.picture.bit.recv_state = 1;
    uint8_t charge_state = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.chg_status;
    ESP_LOGI(TAG,"charge_state:%d",charge_state);
    ESP_LOGI(TAG,"handle_storm_watch_active soc  begin:%d  end:%d cur:%d",DisasterSaveData.disaster_send_param.soc_start,DisasterSaveData.disaster_send_param.soc_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc);
    ESP_LOGI(TAG,"handle_storm_watch_active total_chg_energy  begin:%lu  end:%lu cur:%lu",DisasterSaveData.disaster_send_param.total_chg_energy_begin,DisasterSaveData.disaster_send_param.total_chg_energy_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy);
    // uint8_t charge_state = g_device_data.inv_summary.inv_set01.ctrl_grid_max_curr;
    // charge_state = CHARGE_STATE_ACTIVE; // 测试代码，正式版本删除
    if (charge_state == CHARGE_STATE_ACTIVE) {
        StormWatchSetData.StormWatchState = STORM_STATE_ACTIVE;
        end_reason.bit.discharge = 0;
        if (StormWatchSetData.StormWatchStateSingle == STORM_STATE_INACTIVE || 
            StormWatchSetData.StormWatchStateSingle == STORM_STATE_ENDED) {
            StormWatchSetData.StormWatchStateSingle = STORM_STATE_ACTIVE;
            DisasterSaveData.disaster_send_param.send_flag.bit.stop = 1;
            
            if (s_reset_on && DisasterSaveData.disaster_send_param.send_flag.bit.start == 1) {
                s_reset_on = false;
                ESP_LOGI(TAG, "System reset during storm watch, resuming without sending start data");
                // 重启后同步汇总基准，防止重启瞬间因基准为0导致巨大虚假增量
                s_last_total_chg_energy = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy;
            } else {
                DisasterSaveData.disaster_send_param.BackupStartTime = current_time;
                send_start_param();
                SetStormData(&DisasterSaveData);
                s_retry_begin_count = 0;
                ESP_LOGI(TAG, "Storm watch started");
            }
        } else {
            // 正常备电活跃期间，通过增量统计来计算累计充电能量，规避并机跳变
            uint32_t current_total = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy;
            
            if (current_total > s_last_total_chg_energy) {
                uint32_t diff = current_total - s_last_total_chg_energy;
                // 过滤机制：单位为Wh。正常10kW充电1秒增量约为2.78Wh。
                // 设定阈值为100Wh (0.1kWh)，若1秒内增量超过此值，判定为并机状态改变（新设备加入）。
                // 100Wh 是单机满功率充电增量的 30 多倍，足以排除干扰并精准捕获并机跳变。
                if (diff < 100) { 
                    DisasterSaveData.disaster_send_param.total_chg_energy_end += diff;
                } else {
                    ESP_LOGW(TAG, "Parallel jump detected (Positive): %lu -> %lu, diff: %lu Wh, ignore", s_last_total_chg_energy, current_total, diff);
                }
            } else if (current_total < s_last_total_chg_energy) {
                // 汇总值变小：并机变单机（设备退出），不累加负值，仅更新参考基准
                ESP_LOGW(TAG, "Parallel status change detected (Negative jump): %lu -> %lu, ignore jump", s_last_total_chg_energy, current_total);
            }
            
            // 更新每一秒的参考基准
            s_last_total_chg_energy = current_total;

            // 智能节流保存：只有当数据有变化，且距离上次保存约 10 秒时才写入 Flash
            static uint32_t last_save_energy = 0;
            static uint32_t save_tick = 0;
            if (DisasterSaveData.disaster_send_param.total_chg_energy_end != last_save_energy) {
                if (++save_tick >= 10) { 
                    save_tick = 0;
                    last_save_energy = DisasterSaveData.disaster_send_param.total_chg_energy_end;
                    SetStormData(&DisasterSaveData);
                }
            }
        }
        
        // 尝试发送开始数据到主机
        if (!DisasterSaveData.disaster_send_param.send_flag.bit.start && 
            reals.wifi_connect_ap && 
            s_retry_begin_count < MAX_RETRY_COUNT) {
            s_retry_begin_count++;
            
            memcpy(&StormWatchSetData.StormSendParam, 
                   &DisasterSaveData.disaster_send_param, 
                   sizeof(disaster_send_param_t));
            disaster_post_args_t post_args = {
                .StormSendParam = StormWatchSetData.StormSendParam,
                .send_type = 1,
                .end_reason = {0}
            };
            DisasterSaveData.disaster_send_param.send_flag.bit.start = 
            https_disater_state_to_host(&post_args);

            DisasterSaveData.disaster_send_param.send_flag.bit.stop = 1;
            
            if (DisasterSaveData.disaster_send_param.send_flag.bit.start) {
                s_retry_begin_count = 0;
                SetStormData(&DisasterSaveData);
            }
            
            ESP_LOGI(TAG, "Start post attempt: %d", s_retry_begin_count);
        }
    } else {
        end_reason.all = 0;
        end_reason.bit.discharge = 1;
        StormWatchEndProcess(current_time, system_soc, end_reason);
    }
}
/*------------------------------------------------------------------------
*@Function：StormWatchDataDeal*
-------------------------------------------------------------------------*/
/**
*@brief 风暴预警数据处理
*@param[in]     None
*@param[out]    None
*@return        void
*/
uint16_t StormWatchDataDeal(void)
{
    time_t current_time;
    time(&current_time);
    
    uint32_t receive_end_time = DisasterSaveData.SaveWebStormWatchApiData.EndTime;
    uint16_t system_soc = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc;
    // uint16_t system_soc = g_device_data.inv_summary.inv_set01.ctrl_grid_max_power;
    ESP_LOGI(TAG,"current_time:%llu , receive_end_time:%lu",current_time , receive_end_time);
    ESP_LOGI(TAG,"system_soc:%u , soc:%u",system_soc ,DisasterSaveData.SaveWebStormWatchApiData.SoC);
    // 首次上电读取存储数据
    if (power_on_flag == 1) {
        if (!UpdateDisasterSaveDataFromParam()) {
            return 0;
        }
        receive_end_time = DisasterSaveData.SaveWebStormWatchApiData.EndTime;
        power_on_flag = 2;
        s_reset_on = true;
    }
    if(end_reason.bit.new_event) return 0;
    end_reason.all = 0;
    // 检查风暴是否应该结束
    if (current_time >= receive_end_time)
        end_reason.bit.timeout = 1;
    if (DisasterSaveData.AppCancelStormEvent.EventEndFlag == 1)
        end_reason.bit.app_cancel = 1;
    if (system_soc >= DisasterSaveData.SaveWebStormWatchApiData.SoC)
        end_reason.bit.soc_reach = 1;
    if (StormWatchSetData.StormWatchEnable == 2)
        end_reason.bit.disable = 1;
        
    ESP_LOGI(TAG,"StartTime:%lu ,Urgency:%d,Severity:%d",DisasterSaveData.SaveWebStormWatchApiData.StartTime,DisasterSaveData.SaveWebStormWatchApiData.Urgency,DisasterSaveData.SaveWebStormWatchApiData.Severity);
    ESP_LOGI(TAG,"Certainty:%d ,Event:%u,SoC:%u",DisasterSaveData.SaveWebStormWatchApiData. Certainty,DisasterSaveData.SaveWebStormWatchApiData.Event,DisasterSaveData.SaveWebStormWatchApiData.SoC);
    ESP_LOGI(TAG,"DisasterSaveData.AppCancelStormEvent.EventEndFlag:%d,DisasterSaveData.SaveWebStormWatchApiData.SoC:%u",DisasterSaveData.AppCancelStormEvent.EventEndFlag,DisasterSaveData.SaveWebStormWatchApiData.SoC);
    ESP_LOGI(TAG,"DisasterSaveData.disaster_send_param.send_flag.bit.stop:%d",DisasterSaveData.disaster_send_param.send_flag.bit.stop);        
    ESP_LOGI(TAG,"StormWatchState:%d",StormWatchSetData.StormWatchState);    
    ESP_LOGI(TAG, "Storm end reason: 0x%04X (timeout:%d, app_cancel:%d, soc_reach:%d, disable:%d, charge:%d,picture:%d,state:%d)",
    end_reason.all,
    end_reason.bit.timeout,
    end_reason.bit.app_cancel,
    end_reason.bit.soc_reach,
    end_reason.bit.disable,
    end_reason.bit.discharge,
    StormWatchSetData.picture.bit.recv_state,StormWatchSetData.StormWatchStateSingle);
    
   if (end_reason.all) {
		/* SOC到目标/100%也要退出备电并恢复进入前模式（不再因仅soc_reach而保持mode4） */
        StormWatchSetData.picture.bit.recv_state = 0;
        
        StormWatchEndProcess(current_time, system_soc, end_reason);

    } else {
        // 风暴观察活跃状态处理
        handle_storm_watch_active(current_time, system_soc);
    }
    
    return 1;
}

void set_work_mode(uint8_t work_mode_tag) 
{
    inv_base_param_t *base_param = ll_inv_param_get("inv_base_param");
    base_param->work_mode = work_mode_tag;
    ESP_LOGI(TAG,"set_work_mode:%d",work_mode_tag);
    //SetData_Can.dev_info_t2.inv_set00.work_mode=work_mode_tag;
    //reals.flasWrFlag.sBit.set_data_inv = 1;
    int ret = ll_inv_param_set("inv_base_param", base_param, offsetof(inv_set00_struct, rw_cmd), "strom_param");
    //int ret=Inv_Set00_Write();
    if(ret != 0) {
        ESP_LOGW(TAG,"Inv_Set00_Write FAIL:%d",ret);
        return;
     }
    Inv_can_WR.bk_inv_dev_set.inv_set00.work_mode = work_mode_tag;
    can_data_label *work_flag = iot_malloc(sizeof(can_data_label));
    if (work_flag != NULL) {
        work_flag->type = 0x1A;
        work_flag->offset = 8;
        work_flag->len = 1;
        work_flag->active_can_cmd_type = 0xFFFF;
        CanCmdGenerate(0, work_flag, 1);
        free(work_flag);
    }
}

/*------------------------------------------------------------------------
*@StormWatchDataDealSwitchOff*
-------------------------------------------------------------------------*/
/**
*@brief 风暴预警数据处理变更工作状态,单机
*@param[in]     None
*@param[out]    None
*@return        void
*/

void StormWatchDataDealSwitchOff(void)
{
    /*当第一个图标显示的时候发送工作模式4，工作模式已经是4不会再次发送，其中ll_iot_param_set如果写入的和EEPROM一样将不会再次写入*/
    if(StormWatchSetData.picture.bit.recv_state == 1){
        if(  Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode == 4 || Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode == 0) {
            // power_on_flag = 0;
            return;
        }
        disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
        disaster_param->LastWorkMode =  Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode;
        DisasterSaveData.work_flag.bit.set_workmode_flag = 1;

        int ret = ll_iot_param_set("disaster_save_param", disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
        if (ret != 0) {
            ESP_LOGE(TAG, "Error: ll_iot_param_set failed with code %d", ret);
            return;
        }
        set_work_mode(4);
        ESP_LOGI(TAG,"set work mode 4");
    }  
    // 防止进入页面改写状态逻辑,新方案决定允许用户手动切换工作模式
    // else if(StormWatchSetData.StormWatchStateSingle == 3){
       
    //     if((g_device_data.inv_summary.inv_set00.work_mode != 4) || (g_device_data.bk_inv_dev_set.inv_set00.work_mode != 4)){
        
    //         set_work_mode(4);
    //         ESP_LOGI(TAG,"StormWatchDataDealSwitchOff1");
    //         return;
    //     }
    // }
    else if (StormWatchSetData.StormWatchStateSingle == STORM_STATE_ENDED && DisasterSaveData.work_flag.bit.set_workmode_flag == 1) {
        const disaster_save_param_t *disaster_param_get = ll_iot_param_get("disaster_save_param");
        if (disaster_param_get == NULL) {
            ESP_LOGI(TAG, "Error: ll_inv_param_get returned NULL");
            return;
        }
        uint8_t work_mode = disaster_param_get->LastWorkMode;

        DisasterSaveData.work_flag.bit.set_workmode_flag = 0;

        disaster_save_param_t disaster_param;
        memcpy(&disaster_param, disaster_param_get, sizeof(disaster_save_param_t));
        disaster_param.work_flag.bit.set_workmode_flag = DisasterSaveData.work_flag.bit.set_workmode_flag;

        int ret = ll_iot_param_set("disaster_save_param", &disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
        if (ret != 0) {
            DisasterSaveData.work_flag.bit.set_workmode_flag = 1; // 恢复标志位
            ESP_LOGE(TAG, "Error: ll_iot_param_set failed with code %d", ret);
            return;
        }

        set_work_mode(work_mode);
        //StormWatchSetData.StormWatchStateSingle = STORM_STATE_INACTIVE;
        ESP_LOGI(TAG,"StormWatchDataDealSwitchOff2");
    }
}

/*------------------------------------------------------------------------
*@Function：StormWatchDataDeal*
-------------------------------------------------------------------------*/
/**
*@brief 风暴预警数据处理变更工作状态,单机
*@param[in]     None
*@param[out]    None
*@return        void
*/

// void StormWatchDataDealSwitchOff(void)
// {
//     ESP_LOGI(TAG,"StormWatchSetData.StormWatchStateSingle:%d,StormWatchState:%d",StormWatchSetData.StormWatchStateSingle,StormWatchSetData.StormWatchState );
//     if(StormWatchSetData.StormWatchStateSingle == 1){
//         StormWatchSetData.StormWatchStateSingle = 3;
//         if( power_on_flag == 2) {
//             power_on_flag = 0;
//             return;
//         }
//         disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
//         disaster_param->LastWorkMode = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode;
//         ESP_LOGI(TAG," disaster_param->LastWorkMode:%d", disaster_param->LastWorkMode);
//         int ret = ll_iot_param_set("disaster_save_param", disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
//         if (ret != 0) {
//             ESP_LOGE(TAG, "Error: ll_iot_param_set failed with code %d", ret);
//             return;
//         }
//         set_work_mode(4);
//         ESP_LOGE(TAG,"StormWatchDataDealSwitchOff");
//         return;
//     }  
//     // 防止进入页面改写状态逻辑
//     else if(StormWatchSetData.StormWatchStateSingle == 3){
       
//        ESP_LOGI(TAG,"SETWORK_%d ,%d",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode,Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.work_mode);
//         if((Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode != 4) || (Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.work_mode != 4)){
        
//             set_work_mode(4);
//             ESP_LOGE(TAG,"StormWatchDataDealSwitchOff1");
//             return;
//         }
//     }
//     else if (StormWatchSetData.StormWatchStateSingle == 2){
//         const disaster_save_param_t *disaster_param_get = ll_iot_param_get("disaster_save_param");
//         if (disaster_param_get == NULL) {
//             ESP_LOGE(TAG, "Error: ll_inv_param_get returned NULL");
//             return;
//         }
//         uint8_t work_mode = disaster_param_get->LastWorkMode;
//         set_work_mode(work_mode);
//         StormWatchSetData.StormWatchStateSingle = 0;
//         ESP_LOGE(TAG,"StormWatchDataDealSwitchOff2");
//     }
// }

/*------------------------------------------------------------------------
*@Function：GetStormData*
-------------------------------------------------------------------------*/
/**
*@brief 获取风暴预警使能寄存器数据
*@param[in]     None
*@param[out]    None
*@return        void
*/
uint8_t GetStormWatchEnableRegisterData(void)
{
    return StormWatchSetData.StormWatchEnable;
}

/*------------------------------------------------------------------------
*@Function：SetStormData*
-------------------------------------------------------------------------*/
/**
*@brief 设置风暴预警使能寄存器数据
*@param[in]     None
*@param[out]    None
*@return        void
*/
void SetStormWatchEnableRegisterData(uint8_t enableData)
{
    if(enableData > 2)//M138 1 风暴预警去使能bit0~1:2
    {
        ESP_LOGE(TAG, "Error:SetStormWatchEnableRegisterData Over Limit=%d",enableData);
    }
    else
    {
        ESP_LOGI(TAG,"enableData:%d",enableData);
        StormWatchSetData.StormWatchEnable = enableData;
        SetData.dev_info_t.thunder_ctrl.thunder_enable=StormWatchSetData.StormWatchEnable;
        //if((DisasterSaveData.AppCancelStormEvent.EventEndFlag==1)==1&&(enableData==1))
            //DisasterSaveData.AppCancelStormEvent.EventEndFlag=0;
        // DisasterJudge();
    }
}

/*------------------------------------------------------------------------
*@Function：GetAppCancelEventEndFlag*
-------------------------------------------------------------------------*/
/**
*@brief 获取APP主动取消的备电事件数据
*@param[in]     None
*@param[out]    None
*@return        void
*/
uint8_t  GetAppCancelEventEndFlag(void)
{
   return DisasterSaveData.AppCancelStormEvent.EventEndFlag ;
}
/*------------------------------------------------------------------------
*@Function：SetAppCancelEventData*
-------------------------------------------------------------------------*/
/**
*@brief 设置APP主动取消的备电事件数据
*@param[in]     None
*@param[out]    None
*@return        void
*/
void SetAppCancelEventData(uint8_t SetData)
{

    // 保存 DisasterSaveData 的原始状态
    originalDisasterSaveData = DisasterSaveData;

    // 更新 DisasterSaveData
    DisasterSaveData.AppCancelStormEvent.EventEndFlag = SetData;
    // DisasterJudge();
    // 设置参数
    disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;

    int ret = ll_iot_param_set("disaster_save_param", disaster_param, sizeof(disaster_param_t), "strom_param");
    if (ret != 0) {
        // 恢复 DisasterSaveData 的原始状态
        DisasterSaveData = originalDisasterSaveData;
        ESP_LOGE(TAG, "Error: ll_iot_param_app_cancel_set failed with code %d", ret);
        return;
    }
}
/*------------------------------------------------------------------------
*@Function：GetStormWatchDataState*
-------------------------------------------------------------------------*/
/**
*@brief 获取风暴预警事件状态
*@param[in]     None
*@param[out]    None
*@return        void
*/
uint8_t GetStormWatchDataState(void)
{
    return StormWatchSetData.StormWatchState; // 1:风暴预警开始 2:风暴预警结束;GetStormWatchDataState() == ENABLE_2
}

/*------------------------------------------------------------------------
*@Function：GetStormPicture1State*
-------------------------------------------------------------------------*/
/**
*@brief 获取风暴预警事件状态
*@param[in]     None
*@param[out]    None
*@return        void
*/
uint8_t GetStormPicture1State(void)
{
    return StormWatchSetData.picture.bit.recv_state; // 1:显示已切换为备电模式 2:切换为上一工作模式 0:无显示
}


/*------------------------------------------------------------------------
*@Function：获取风暴预警的目标SOC 
-------------------------------------------------------------------------*/
uint16_t GetStormWatchTargetSoc(void)
{
    return DisasterSaveData.SaveWebStormWatchApiData.SoC;
}

/*------------------------------------------------------------------------
*@Function：StormWebDataCheck
-------------------------------------------------------------------------*/
/**
*@brief 风暴预警Web数据检测，如果已收到数据，且已更新同步电池包、时间等信息，则做主动上报处理
*@param[in]     None
*@param[out]    None
*@return        void
*/
void StormWebDataCheck(void)
{

    if(get_thunder_info_flag&&storm_vaild_flag)
    {
        if(WebStormData_Save.StartTime)
        {
            UpdateDisasterSaveDataFromParam();
            // 新的预警前发上一次结束
            time_t CurSystemTime_end;
            time(&CurSystemTime_end);
            uint16_t system_soc = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc;
            // storm_end_reason_t end_reason = { .all = 0 };
            end_reason.bit.new_event = 1;
            
            // 如果上一次没有发结束，新的预警前发上一次结束
            if(DisasterSaveData.disaster_send_param.send_flag.bit.start == 1 && DisasterSaveData.disaster_send_param.send_flag.bit.end == 0){
                StormWatchEndProcess(CurSystemTime_end, system_soc, end_reason);
                ESP_LOGI(TAG,"SetStormWebData send last end");
            }
    
            // 防止第一次结束发送的开始时间是上次的
            DisasterSaveData.disaster_send_param.BackupStartTime = CurSystemTime_end;
            data_kill();
    
            originalDisasterSaveData = DisasterSaveData;
            memcpy(&DisasterSaveData.SaveWebStormWatchApiData, &WebStormData_Save, sizeof(_SaveWebStormWatchApiData));
            end_reason.bit.new_event = 0;

            memcpy(DisasterSaveData.disaster_send_param.msg_id, cur_msgid, sizeof(DisasterSaveData.disaster_send_param.msg_id));
            // strncpy(SetData.dev_info_t.disaster_Warm_Msg_Id, cur_msgid, sizeof( SetData.dev_info_t.disaster_Warm_Msg_Id));
            reals.SetDataWrFlag.sBit.SetDataUpdate_all=1;

            
            // DisasterSaveData.AppCancelStormEvent.EventEndFlag = 0;
            // DisasterSaveData.disaster_send_param.send_flag.status = 0; // 新事件开始清空所有标志
            disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
            
            int ret = ll_iot_param_set("disaster_save_param", disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
            if (ret != 0) {
                // 恢复 DisasterSaveData 的原始状态
                DisasterSaveData = originalDisasterSaveData;
                ESP_LOGE(TAG, "Error: ll_iot_param_webdata_set failed with code, use original webdata %d", ret);
                return;
            }
           // StormWatchSetData.picture.bit.recv_state = 0;
            power_on_flag = 0;
            get_thunder_info_flag = 0;
            ESP_LOGI(TAG,"SetStormWebData");
        }
    }
}

/*------------------------------------------------------------------------
*@Function：SetStormWebData*
-------------------------------------------------------------------------*/
/**
*@brief 设置风暴预警Web数据
*@param[in]     None
*@param[out]    None
*@return        void
*/
void SetStormWebData(void* stromSetData, char *id)
{
    if (stromSetData == NULL) {
        ESP_LOGE(TAG, "SetStormWebData: SetData is NULL");
        return;
    }
    _SaveWebStormWatchApiData *SaveWebStormData0 = (_SaveWebStormWatchApiData *)stromSetData;
    if(SaveWebStormData0->StartTime)
    {
        get_thunder_info_flag = 1;
        memcpy(&WebStormData_Save, SaveWebStormData0, sizeof(_SaveWebStormWatchApiData));
        if (id != NULL)
        {
            memcpy(cur_msgid, id, sizeof(cur_msgid));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error:SetStormWebData.Event=%d",SaveWebStormData0->Event);
    }
}

void SetStormData(void* SetData1)
{
    if(SetData1==NULL)
    {
        ESP_LOGI(TAG,"SetData1 is NULL Err");
        return;
    }

    _SaveWebStormWatchApiData *SaveWebStormData1 = (_SaveWebStormWatchApiData *)SetData1;
    if(SaveWebStormData1->StartTime)
    {
        originalDisasterSaveData = DisasterSaveData;
        memcpy(&DisasterSaveData.SaveWebStormWatchApiData, SaveWebStormData1, sizeof(_SaveWebStormWatchApiData));
        // DisasterSaveData.AppCancelStormEvent.EventEndFlag = 0;
        disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
        int ret = ll_iot_param_set("disaster_save_param", disaster_param, sizeof(disaster_param_t), "strom_param");
        if (ret != 0) {
            // 恢复 DisasterSaveData 的原始状态
            DisasterSaveData = originalDisasterSaveData;
            ESP_LOGE(TAG, "Error: ll_iot_param_webdata_set failed with code, use original webdata %d", ret);
            return;
        }
        else{
            ESP_LOGI(TAG,"SetStormData");
        }
        // StormWatchSetData.StormWatchStateSingle = 0;
        // power_on_flag = 0;
        // DisasterJudge();
    }
    else
    {
        ESP_LOGE(TAG, "Error:SetStormWebData.Event=%d",SaveWebStormData1->Event);
    }
}

/*------------------------------------------------------------------------
*@Function：DisasterVaildCheck
-------------------------------------------------------------------------*/
/**
*@brief 风暴预警有效性
*@param[in]     None
*@param[out]    None
*@return        void
*/
uint8_t DisasterVaildCheck(void)
{
    if ((0 == reals.rtc_flag.sBit.RTC_valid_from_NTP)
        &&(0 == reals.rtc_flag.sBit.RTC_valid_from_APP)
        &&(0 == reals.rtc_flag.sBit.RTC_valid_from_SERVER)
        &&(0 == reals.rtc_flag.sBit.RTC_valid_from_CAN))
    {
        return 0;
    }

    if (!reals.online_Pack_num)
    {
        ESP_LOGI(TAG,"Pack Num Null:%d",reals.online_Pack_num);
        return 0;
    }

    if ((reals.MCUPoweronCnt < 30))//5s
	{
        ESP_LOGI(TAG,"reals.MCUPoweronCnt:%llu",reals.MCUPoweronCnt);
		return 0;
	}

    if (reals.Addr_can_master != 1)
    {
        return 0;
    }

    return 1;
}

// static TimerHandle_t storm_watch_timer;
/*------------------------------------------------------------------------
*@Function：DisasterJudge*
-------------------------------------------------------------------------*/
/**
*@brief 单机、并机模式判断
*@param[in]     None
*@param[out]    None
*@return        void
*/
void DisasterJudge(void)
{
    storm_vaild_flag = DisasterVaildCheck();
    if (reals.ota_happen || !storm_vaild_flag)
    {
        return;
    }

    if (!StormWatchSetData.StormWatchEnable) {
        return;
    }

    StormWebDataCheck();
    StormWatchDataDeal();
    // 检查离网并机使能开关是否关闭
    //if (disaster_Device_info.inv_summary.inv_set01.ctrl_mix.multi_inv_off_grid_en != 1) {
    StormWatchDataDealSwitchOff(); // 离网并机使能开关关掉下的判断
    //}
    // xSemaphoreGive(disasterJudgeMutex);  /* 释放信号量 */
}


// void StormWatchTimerCallback(TimerHandle_t xTimer)
// {
//     DisasterJudge();
// }

// void InitStormWatchTimer(void)
// {
//     storm_watch_timer = xTimerCreate("StormWatchTimer", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, StormWatchTimerCallback);
//     if (storm_watch_timer == NULL) {
//         ESP_LOGE(TAG, "Failed to create timer");
//     } else {
//         if (xTimerStart(storm_watch_timer, 0) != pdPASS) {
//             ESP_LOGE(TAG, "Failed to start timer");
//         }
//     }
// }

