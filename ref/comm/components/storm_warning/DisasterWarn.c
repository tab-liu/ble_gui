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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "DisasterWarn.h"
#include "can_data.h"
#include "can_type.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "http_client.h"
#include "comm_define.h"
#include "can_protocol.h"
#include "parameter.h"
#include "utils.h"

#define TAG "[DisasterWarn]"

#if CONFIG_DISASTER_WARN_CHANNEL == 2 

#define DISASTER_SAVE_PARAM_LABEL "disaster_s"
// static SemaphoreHandle_t disasterJudgeMutex = NULL; //申明互斥型信号量，在FreeRTOS中二值型信号量和互斥型信号量类型完全相同。
uint8_t power_on_flag = 1;
USE_EXT_RAM_BSS _StormWatchSetData StormWatchSetData = {0}; //实时状态，掉电丢失
USE_EXT_RAM_BSS static disaster_save_param_t originalDisasterSaveData = {0}; // 保存原始状态

void SetStormData(void* SetData1);
extern bool iot_net_status_check(void);

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
    DisasterSaveData.disaster_send_param.soc_start = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.soc; //g_device_data.pack_summary.pack_announce.soc;
    DisasterSaveData.disaster_send_param.send_flag.bit.end = 0;
    DisasterSaveData.disaster_send_param.total_chg_energy_begin = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.total_chg_energy; //g_device_data.pack_summary.pack_extend.total_chg_energy;
}

void send_end_param(void)
{
    DisasterSaveData.disaster_send_param.soc_end = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.soc; //g_device_data.pack_summary.pack_announce.soc;
    DisasterSaveData.disaster_send_param.send_flag.bit.start = 0;
    DisasterSaveData.disaster_send_param.total_chg_energy_end = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.total_chg_energy; //g_device_data.pack_summary.pack_extend.total_chg_energy;
}

int save_disaster_save_param(disaster_save_param_t disaster_save_param)
{
    // char path[35] = {0};
    // sprintf(path, "%s", DISASTER_SAVE_PARAM_PATH);
    // ESP_LOGI(TAG, "save_disaster_save_param path: %s", path);
    // if(write_file(path, (uint8_t *)&disaster_save_param, sizeof(disaster_save_param_t)) != 0)
    // {
    //     ESP_LOGE(TAG, "Error: Failed to save disaster_save_param");
    //     return false;
    // }
    reals.flasWrFlag.sBit.disaster_warn = 1;
    return 0;
}

bool UpdateDisasterSaveDataFromParam(void)
{
    // do nothing 如果需要对外modbus展示参数可在此添加
    return true;
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
    uint16_t SystemAverageSoC = 0;
    uint32_t ReceiveEndTime = DisasterSaveData.SaveWebStormWatchApiData.EndTime;
    static uint8_t cnt = 0; 
    static uint8_t cnt_begin = 0; 
    static bool fault = false;
    static uint8_t cnt2 = 0; 
    // int8_t backup_soc = 0;
    time_t CurSystemTime;
    time(&CurSystemTime);
    // struct tm *utc_time = gmtime(&CurSystemTime);
    
    if(!StormWatchSetData.StormWatchEnable)
    {
        return 0;
    }

    // 首次上电读取存储数据,power_on_flag为2
    if(power_on_flag == 1){
        // iot_read_flash(DISASTER_SAVE_PARAM_LABEL, (uint8_t *)&DisasterSaveData, sizeof(disaster_save_param_t));
        if (!UpdateDisasterSaveDataFromParam()) {
            return 0;
        }
        ReceiveEndTime = DisasterSaveData.SaveWebStormWatchApiData.EndTime;
        // 标记读取过数据
        power_on_flag = 2;
    }

    //风暴预警结束: 过时+ 用户取消 + SOC达到目标值
    SystemAverageSoC = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.soc;
    // ESP_LOGW(TAG,"CurSystemTime:%ld",CurSystemTime);

    if((CurSystemTime >= ReceiveEndTime) || (DisasterSaveData.AppCancelStormEvent.EventEndFlag == 1) || 
        SystemAverageSoC >= DisasterSaveData.SaveWebStormWatchApiData.SoC || StormWatchSetData.StormWatchEnable == 2 || fault)
    {
        fault = 0;
        if(StormWatchSetData.StormWatchState == 2){
            if((!DisasterSaveData.disaster_send_param.send_flag.bit.end) && iot_net_status_check() &&  cnt < 5){
                cnt ++;
                memcpy(&StormWatchSetData.StormSendParam, &DisasterSaveData.disaster_send_param, sizeof(disaster_send_param_t));
                // DisasterSaveData.disaster_send_param.BackupEndTime = CurSystemTime-DisasterSaveData.disaster_send_param.TimeZoneOffset;
                DisasterSaveData.disaster_send_param.send_flag.bit.end = https_disater_state_to_host(StormWatchSetData.StormSendParam,2);
                if(DisasterSaveData.disaster_send_param.send_flag.bit.end){
                    SetStormData(&DisasterSaveData);
                }
                ESP_LOGI(TAG,"post cnt :%d", cnt);
            }
        }
        // 两状态都退出为2
        else if(StormWatchSetData.StormWatchState == 1)//1->2
        {
            // ESP_LOGW(TAG,"AC on:%d,%d",g_device_data.inv_summary.inv_set00.ctrl_ac,StormWatchSetData.StormWatchState);
            StormWatchSetData.StormWatchState = 2;
            StormWatchSetData.StormWatchStateSingle = 2;   
            DisasterSaveData.disaster_send_param.BackupEndTime = CurSystemTime-DisasterSaveData.disaster_send_param.TimeZoneOffset;
            send_end_param();
            SetStormData(&DisasterSaveData);
            cnt = 0; 
            // http_post.bit.end= 0;         
        }

        if(DisasterSaveData.disaster_send_param.send_flag.bit.stop == 1 && SystemAverageSoC == DisasterSaveData.SaveWebStormWatchApiData.SoC){
            DisasterSaveData.disaster_send_param.send_flag.bit.stop = 2;
            SetStormData(&DisasterSaveData);
        }

        return 0;
    }
    //BMS SoC 值判断
    if(SystemAverageSoC < DisasterSaveData.SaveWebStormWatchApiData.SoC)
    {
        if(DisasterSaveData.disaster_send_param.send_flag.bit.stop == 2 && SystemAverageSoC > (DisasterSaveData.SaveWebStormWatchApiData.SoC - 20) && (DisasterSaveData.SaveWebStormWatchApiData.SoC>20)){
            ESP_LOGI(TAG,"soc:%d,stop:%d",SystemAverageSoC,DisasterSaveData.disaster_send_param.send_flag.bit.stop);
            return 0; //浮值20
        } 

        if(StormWatchSetData.StormWatchState == 1){
            if((!DisasterSaveData.disaster_send_param.send_flag.bit.start) && iot_net_status_check() && cnt_begin < 5){
                cnt_begin ++;
                DisasterSaveData.disaster_send_param.BackupStartTime = CurSystemTime - DisasterSaveData.disaster_send_param.TimeZoneOffset;
                memcpy(&StormWatchSetData.StormSendParam, &DisasterSaveData.disaster_send_param, sizeof(disaster_send_param_t));
                DisasterSaveData.disaster_send_param.send_flag.bit.start = https_disater_state_to_host(StormWatchSetData.StormSendParam,1);
                if(DisasterSaveData.disaster_send_param.send_flag.bit.start){
                    SetStormData(&DisasterSaveData);
                }
                ESP_LOGI(TAG,"post state cnt :%d", cnt_begin);
            }
        }
        // if((top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.ctrl_ac == 1) && ((top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.line_event.bit.PV_online == 1) || 
        //     ((top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.work_mode >= 2) && (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.work_mode <= 5)))) {
            
        // if((g_device_data.inv_summary.inv_set00.ctrl_ac == 1)) {
        // if((g_device_data.inv_summary.inv_set01.ctrl_grid == 1)) {
        StormWatchSetData.StormWatchState = 1;
        fault = 0;
        //开始充电:StormWatchStateSingle值为0、2为关状态；1、3为开状态
        if(StormWatchSetData.StormWatchStateSingle == 0 || StormWatchSetData.StormWatchStateSingle == 2){
            StormWatchSetData.StormWatchStateSingle = 1;
            DisasterSaveData.disaster_send_param.send_flag.bit.stop = 1;
            DisasterSaveData.disaster_send_param.BackupStartTime = CurSystemTime - DisasterSaveData.disaster_send_param.TimeZoneOffset;
            send_start_param();
            SetStormData(&DisasterSaveData);
            cnt_begin = 0;
            ESP_LOGI(TAG,"StormWatchDataDealSwitchOn");
        }
        // }
        // else {
        //     // ESP_LOGW(TAG,"AC off:%d",g_device_data.inv_summary.inv_set00.ctrl_ac);
        //     fault = 1;
        // }
    }
    return 1;
}

void set_work_mode(uint8_t work_mode_tag) 
{
    // MOD_STRUCT_reg02000 *base_param = NULL;// TODO ll_inv_param_get("inv_base_param");
    // base_param->work_mode = work_mode_tag;
    int ret = 0; //ll_inv_param_set("inv_base_param", base_param, offsetof(inv_set00_struct, rw_cmd), "strom_param");
    //先不修改工作模式，通过心跳帧下发
    // top_modbus_wr.Inv.mod_reg02000_Inv_base_set.work_mode = work_mode_tag; //g_device_data.bk_inv_dev_set.inv_set00.work_mode = work_mode_tag;
    if(ret != 0) {
        return;
    }
    can_data_label *work_flag = iot_calloc(sizeof(can_data_label));
    if (work_flag != NULL) {
        work_flag->type = 0x1A;
        work_flag->offset = 8;
        work_flag->len = 1;
        work_flag->active_can_cmd_type = 0xFFFF;
        // CanCmdGenerate(0, work_flag, 1); // TODO是否需要下发修改工作模式，还是仅通过心跳帧下发
        free(work_flag);
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

void StormWatchDataDealSwitchOff(void)
{
    if(StormWatchSetData.StormWatchStateSingle == 1){
        StormWatchSetData.StormWatchStateSingle = 3;
        if( power_on_flag == 2) {
            power_on_flag = 0;
            return;
        }
        // disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
        // disaster_param->LastWorkMode = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.work_mode;
        DisasterSaveData.disaster_send_param.old_work_mode = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.work_mode;
        int ret = save_disaster_save_param(DisasterSaveData); //ll_iot_param_set("disaster_save_param", disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
        if (ret != 0) {
            ESP_LOGE(TAG, "Error: ll_iot_param_set failed with code %d", ret);
            return;
        }
        set_work_mode(4);
        ESP_LOGI(TAG,"StormWatchDataDealSwitchOff");
        return;
    }  
    // 防止进入页面改写状态逻辑
    else if(StormWatchSetData.StormWatchStateSingle == 3){
       
        if((top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.work_mode != 4) || (top_modbus_wr.Inv.mod_reg02000_Inv_base_set.work_mode != 4)){
        
            set_work_mode(4);
            // ESP_LOGI(TAG,"StormWatchDataDealSwitchOff1");
            return;
        }
    }
    else if (StormWatchSetData.StormWatchStateSingle == 2){
        const disaster_save_param_t *disaster_param_get = &DisasterSaveData;//ll_iot_param_get("disaster_save_param");
        if (disaster_param_get == NULL) {
            ESP_LOGI(TAG, "Error: ll_inv_param_get returned NULL");
            return;
        }
        uint8_t work_mode = disaster_param_get->disaster_send_param.old_work_mode;
        set_work_mode(work_mode);
        StormWatchSetData.StormWatchStateSingle = 0;
        ESP_LOGI(TAG,"StormWatchDataDealSwitchOff2");
    }
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
void SetStormWatchEnableRegisterData(uint8_t IotSetData)
{
    if(IotSetData > 2)//M138 1 风暴预警去使能bit0~1:2
    {
        ESP_LOGE(TAG, "Error:SetStormWatchEnableRegisterData Over Limit=%d",IotSetData);
    }
    else
    {
        StormWatchSetData.StormWatchEnable = IotSetData;
        // DisasterJudge();
    }
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
void SetAppCancelEventData(uint8_t IotSetData)
{
    // 保存 DisasterSaveData 的原始状态
    originalDisasterSaveData = DisasterSaveData;

    // 更新 DisasterSaveData
    DisasterSaveData.AppCancelStormEvent.EventEndFlag = IotSetData;
    // DisasterJudge();
    // 设置参数
    // disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
    int ret = save_disaster_save_param(DisasterSaveData); //ll_iot_param_set("disaster_save_param", disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
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
    // ESP_LOGI(TAG,"GetStormWatchData, enable: %d, state: %d, single: %d, cancel: %d", StormWatchSetData.StormWatchEnable, StormWatchSetData.StormWatchState, StormWatchSetData.StormWatchStateSingle, StormWatchSetData.StormWatchCancel);
    return StormWatchSetData.StormWatchState; // 1:风暴预警开始 2:风暴预警结束;GetStormWatchDataState() == ENABLE_2
}


/*------------------------------------------------------------------------
*@Function：获取风暴预警的目标SOC 
-------------------------------------------------------------------------*/
uint16_t GetStormWatchTargetSoc(void)
{
    return DisasterSaveData.SaveWebStormWatchApiData.SoC;
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
// extern void disaster_param_dump(void);
void DisasterJudge(void)
{

    // if (!((strstr(g_device_data.inv_summary.inv_about.dev_type, "EP800") || 
    //     strstr(g_device_data.inv_summary.inv_about.dev_type, "EP900")|| 
    //     strstr(g_device_data.inv_summary.inv_about.dev_type, "EP13K"))
    //     ))    return;
    // if (disasterJudgeMutex == NULL) { 
    //     disasterJudgeMutex = xSemaphoreCreateMutex();
    //     if (disasterJudgeMutex == NULL) {
    //         ESP_LOGE(TAG, "Disaster Judge Semaphore Create failed");
    //         return;
    //     }
    // }

    // /* 获取信号量 */
    // if (xSemaphoreTake( disasterJudgeMutex, pdMS_TO_TICKS(300) ) != pdPASS) {
    //     return;
    // }


    StormWatchDataDeal();
    // 检查离网并机使能开关是否关闭
    // if (g_device_data.inv_summary.inv_set01.ctrl_mix.multi_inv_off_grid_en != 1) {
        StormWatchDataDealSwitchOff(); // 离网并机使能开关关掉下的判断
    // }
    // xSemaphoreGive(disasterJudgeMutex);  /* 释放信号量 */
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
void SetStormWebData(void* IotSetData, int32_t Offset, char *id)
{

    _SaveWebStormWatchApiData *SaveWebStormData0 = (_SaveWebStormWatchApiData *)IotSetData;
    if(SaveWebStormData0->StartTime)
    {
        UpdateDisasterSaveDataFromParam();
        // 新的预警前发上一次结束
        if(StormWatchSetData.StormWatchState == 1){
            time_t CurSystemTime_end;
            time(&CurSystemTime_end);

            DisasterSaveData.disaster_send_param.BackupEndTime = CurSystemTime_end - Offset;

            DisasterSaveData.disaster_send_param.soc_end = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.soc;
            // DisasterSaveData.disaster_send_param.send_flag.bit.start = 0;
            DisasterSaveData.disaster_send_param.total_chg_energy_end = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.total_chg_energy;
            https_disater_state_to_host(DisasterSaveData.disaster_send_param,2);
            // StormWatchSetData.StormWatchStateSingle = 0;
            DisasterSaveData.disaster_send_param.BackupStartTime = CurSystemTime_end - Offset;
        }
        DisasterSaveData.disaster_send_param.uuid += 1; 
        DisasterSaveData.disaster_send_param.TimeZoneOffset = Offset;
       
        originalDisasterSaveData = DisasterSaveData;
        
        memcpy(&DisasterSaveData.SaveWebStormWatchApiData, SaveWebStormData0, sizeof(_SaveWebStormWatchApiData));
        if(id != NULL){
            memcpy(&DisasterSaveData.disaster_send_param.msg_id, id, sizeof(DisasterSaveData.disaster_send_param.msg_id));
        }
        
        DisasterSaveData.AppCancelStormEvent.EventEndFlag = 0;
        // disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
        int ret = save_disaster_save_param(DisasterSaveData); //ll_iot_param_set("disaster_save_param", disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
        if (ret != 0) {
            // 恢复 DisasterSaveData 的原始状态
            DisasterSaveData = originalDisasterSaveData;
            ESP_LOGE(TAG, "Error: ll_iot_param_webdata_set failed with code, use original webdata %d", ret);
            return;
        }
        // StormWatchSetData.StormWatchState = 0;
        power_on_flag = 0;
        DisasterSaveData.disaster_send_param.send_flag.bit.start = 0;
        // DisasterJudge();
        ESP_LOGI(TAG,"SetStormWebData");
    }
    else
    {
        ESP_LOGE(TAG, "Error:SetStormWebData.Event=%d",SaveWebStormData0->Event);
    }
}

void SetStormData(void* SetData1)
{
    _SaveWebStormWatchApiData *SaveWebStormData1 = (_SaveWebStormWatchApiData *)SetData1;
    if(SaveWebStormData1->StartTime)
    {

        originalDisasterSaveData = DisasterSaveData;
        memcpy(&DisasterSaveData.SaveWebStormWatchApiData, SaveWebStormData1, sizeof(_SaveWebStormWatchApiData));
        // DisasterSaveData.AppCancelStormEvent.EventEndFlag = 0;
        // disaster_save_param_t *disaster_param = (disaster_save_param_t *)&DisasterSaveData;
        int ret = save_disaster_save_param(DisasterSaveData); //ll_iot_param_set("disaster_save_param", disaster_param, offsetof(disaster_save_param_t, nc), "strom_param");
        if (ret != 0) {
            // 恢复 DisasterSaveData 的原始状态
            DisasterSaveData = originalDisasterSaveData;
            ESP_LOGE(TAG, "Error: ll_iot_param_webdata_set failed with code, use original webdata %d", ret);
            return;
        }
        else{
            ESP_LOGI(TAG,"SetStormData");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error:SetStormWebData.Event=%d",SaveWebStormData1->Event);
    }
}

#endif
