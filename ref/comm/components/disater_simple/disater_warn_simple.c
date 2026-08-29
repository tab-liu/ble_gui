/**
  ******************************************************************************
  * @file      disater_warn_simple.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/06/25
  * @brief     灾害预警
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/14  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_bit_defs.h"
#include <string.h>
#include <stddef.h>
#include "esp_timer.h"
#include "esp_system.h"
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "cJSON.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "disater_warn_simple.h"
#include "utils.h"
#include "http_client.h"
#include "parameter.h"
#include "modbus_slave.h"
#include "modbus_define.h"
#include "comm_define.h"

#if CONFIG_DISASTER_WARN_CHANNEL == 1 

/* ================================ 文件内宏定义 ================================ */

#define TAG "[disater_warn_simple]"

/* =============================== 文件内全局变量 ================================ */

// 风暴预警SOC平衡阈值
const uint8_t backup_soc_limit = 5;        // 备电缓冲阈值
const uint8_t backup_soc_min = 0;          // 备电最低SOC阈值
const uint8_t retry_to_host_max_num = 5;   // 云端数据上报最大重试次数

// 最近一次云端收到的风暴预警数据（非实际生效变量）
static USE_EXT_RAM_BSS _SaveWebStormWatchApiData disater_warn_param = {0};

// 灾害预警接收备电状态检查定时器
static esp_timer_handle_t disater_check_timer = NULL;

/* 用于传递给定时器回调的待提交 reason 值（外部设置后由回调读取） */
static uint16_t pending_disaster_reason_all = 0;

/* ================================ 模块函数定义 ================================ */

/**
  * @brief      风暴预警参数解析
  * @param[in]  cJSON *root  
  * @param[out] None
  * @return     static int
  */
static int disater_warn_data_parse(cJSON *root)
{
	cJSON *var = NULL;
    memset(&disater_warn_param, 0, sizeof(_SaveWebStormWatchApiData));
    
    // recordId
	var = cJSON_GetObjectItem(root, "recordId");
	if ((var == NULL) || (var->type != cJSON_String))
	{
		ESP_LOGW(TAG, "field [recordId] error");
		goto __exit;
	}
	strncpy(disater_warn_param.msg_id, var->valuestring, sizeof(disater_warn_param.msg_id)-1);
    disater_warn_param.msg_id[sizeof(disater_warn_param.msg_id)-1] = '\0';

    // start
	var = cJSON_GetObjectItem(root, "start");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [start] error");
		goto __exit;
	}
	disater_warn_param.StartTime = (typeof(disater_warn_param.StartTime))var->valuedouble;
    
    // end
	var = cJSON_GetObjectItem(root, "end");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [end] error");
		goto __exit;
	}
	disater_warn_param.EndTime = (typeof(disater_warn_param.EndTime))var->valuedouble;
    
    // urgency
	var = cJSON_GetObjectItem(root, "urgency");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [urgency] error");
		goto __exit;
	}
	disater_warn_param.Urgency = (typeof(disater_warn_param.Urgency))var->valuedouble;

    // severity
	var = cJSON_GetObjectItem(root, "severity");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [severity] error");
		goto __exit;
	}
	disater_warn_param.Severity = (typeof(disater_warn_param.Severity))var->valuedouble;

    // certainty
	var = cJSON_GetObjectItem(root, "certainty");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [certainty] error");
		goto __exit;
	}
	disater_warn_param.Certainty = (typeof(disater_warn_param.Certainty))var->valuedouble;

    // event
	var = cJSON_GetObjectItem(root, "event");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [event] error");
		goto __exit;
	}
	disater_warn_param.Event = (typeof(disater_warn_param.Event))var->valuedouble;
    
    // soc
	var = cJSON_GetObjectItem(root, "soc");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [soc] error");
		goto __exit;
	}
	disater_warn_param.SoC = (typeof(disater_warn_param.SoC))var->valuedouble;
    
    // utc_offset_seconds
	var = cJSON_GetObjectItem(root, "utc_offset_seconds");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [utc_offset_seconds] error");
		goto __exit;
	}
	disater_warn_param.TimeZoneOffset = (typeof(disater_warn_param.TimeZoneOffset))var->valuedouble;

	return 0;

__exit:
	return -1;
}


//#define THUNDER_DBG

#ifdef THUNDER_DBG
const char *test_storm = "{\n\
    \"date\": \"2023-09-15 09:31\",\n\
    \"start\": 1775118367,\n\
    \"end\": 1775119367,\n\
    \"urgency\": 0,\n\
    \"severity\": 1,\n\
    \"certainty\": 2,\n\
    \"sender\": \"US Environmental Protection Agency\",\n\
    \"event\": 3,\n\
    \"headline\": \"Thunderstorm Alert for Natomas \",\n\
    \"recordId\": \"18a180ef4faee2d856c48a4d2a0\",\n\
    \"timezone\": \"Europe/Vilnius\",\n\
    \"utc_offset_seconds\": -18000,\n\
    \"soc\": 100\n\
}";
#endif

/*------------------------------------------------------------------------------
 Function: disater_warn_mode_handle
 -----------------------------------------------------------------------------*/
/**
  * @brief      风暴预警响应
  * @param[in]  uint8_t *buff  
                int len        
  * @param[out] None
  * @return     int
  */
int disater_warn_mode_handle(uint8_t *buff, int len)
{    
    if ( buff == NULL ) return 1;
	uint8_t *pdata = buff;
	int ret = 0;
    
	/* 非风暴模式指令直接退出 */
	if (!((pdata[0] == 0x00) && (pdata[1] == DISATER_MODE_FUNC_CODE))) return 1;

#ifdef THUNDER_DBG
	char test_buf[500];
	int test_len = strlen(test_storm);
	test_buf[0] = 0x00;
	test_buf[1] = 0x04;
	test_buf[2] = len >> 8;
	test_buf[3] = len;
	strcpy(&test_buf[4], test_storm);
	test_len += 4;
	buff = (uint8_t*)test_buf;
    pdata = buff;
#endif

	/* 解析风暴指令json数据 */
	cJSON *root = cJSON_Parse((const char*)&pdata[DISATER_PROTOCOL_HEADER]);
	if ((root == NULL) || (root->type != cJSON_Object))
	{
		ret = -1;
		ESP_LOGW(TAG, "cjson parse error");
		if (root == NULL) goto __exit;
	}

    /*获取响应record*/
    char msg_id[100];
    cJSON *recordId = cJSON_GetObjectItem(root, "recordId");
    if ((recordId == NULL) || (recordId->type != cJSON_String))
    {
        ret = -1;
        ESP_LOGW(TAG, "field [recordId] error");
        goto __exit;
    }
    strncpy(msg_id, recordId->valuestring, sizeof(msg_id)-1);
    msg_id[sizeof(msg_id)-1] = '\0';

    /*信息解析*/
	ESP_LOGW(TAG, "thunder message: %02x %02x %02x %02x\n%s\n", pdata[0], pdata[1], pdata[2], pdata[3], &pdata[4]);
	ret = disater_warn_data_parse(root);

	if (root != NULL)
	{
		cJSON_Delete(root);

		/* 收到新的风暴事件 */
		if (ret == 0) {

#if 0   // 可能会影响服务器识别
            bool result = https_disater_state_to_host(DisasterSaveData.disaster_send_param, ret, msg_id);
#endif
            /*如果存在正在生效的风暴预警*/
            if ( IotSetData.dev_info_t.thunder_ctrl.thunder_work_mode_state ) {
                // 先执行当前预警结束
                DisasterSaveData.disaster_send_param.dev_send_flag.bit.update = 1;
            } else {
                // 直接更新参数至本地
                disater_warn_param.uuid = DisasterSaveData.SaveWebStormWatchApiData.uuid;
                disater_warn_param.state.bits.first_flag = 1;
                DisasterSaveData.SaveWebStormWatchApiData = disater_warn_param;
                reals.flasWrFlag.sBit.disaster_warn = 1;
            }
		}
	}
    
__exit:
	return ret;
}

/*
 * 优化后的定时器回调和启动函数：
 * - 改为单次（one-shot）延时触发（原代码使用 periodic 但回调内立即停止并删除，使用 one-shot 更契合语义）；
 * - 增加对 esp_timer API 返回值检查，避免无效句柄或重复创建；
 * - 增加必要注释与最小化副作用（保持原变量名以兼容现有代码）。
 * - 从 arg 中读取外部传入的 reason.all（若 arg == NULL 则采用原先行为
 */
static void disater_check_callback(void* arg)
{
    uint16_t reason_all = 0;
    if (arg) {
        reason_all = *(uint16_t *)arg;
    }

    /* 更新 UUID 并标记需写回 */
    DisasterSaveData.SaveWebStormWatchApiData.uuid++;

    time_t now = time(NULL);

    /* 初始化备电数据：soc 和 能量（begin/end 同步为当前值） */
    DisasterSaveData.disaster_send_param.soc_start = top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.soc;
    DisasterSaveData.disaster_send_param.soc_end   = DisasterSaveData.disaster_send_param.soc_start;

#if PACK_MAX_NUM > 1
    DisasterSaveData.disaster_send_param.total_chg_energy_begin = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.total_chg_energy * 100;
#else
    DisasterSaveData.disaster_send_param.total_chg_energy_begin = top_modbus_rd.Pack[0].mod_reg06100_Pack_each.total_chg_energy;
#endif
    DisasterSaveData.disaster_send_param.total_chg_energy_end   = DisasterSaveData.disaster_send_param.total_chg_energy_begin;

    /* 时间与 uuid/msg_id */
    DisasterSaveData.disaster_send_param.BackupStartTime = now;
    DisasterSaveData.disaster_send_param.BackupEndTime   = now;
    DisasterSaveData.disaster_send_param.uuid = DisasterSaveData.SaveWebStormWatchApiData.uuid;
    memcpy(DisasterSaveData.disaster_send_param.msg_id,
           DisasterSaveData.SaveWebStormWatchApiData.msg_id,
           sizeof(DisasterSaveData.disaster_send_param.msg_id) <= sizeof(DisasterSaveData.SaveWebStormWatchApiData.msg_id) ?
           sizeof(DisasterSaveData.disaster_send_param.msg_id) : sizeof(DisasterSaveData.SaveWebStormWatchApiData.msg_id));

    /* reason 处理 */
    if (reason_all != 0) {
        DisasterSaveData.disaster_send_param.reason.all = reason_all;
    } else {
        DisasterSaveData.disaster_send_param.reason.all = 0;
        DisasterSaveData.disaster_send_param.reason.bit.discharge = 1;
    }

    /* 标记上报结束并请求写回 */
    DisasterSaveData.disaster_send_param.icloud_send_flag.bit.end = 1;

    /* 停止并删除定时器（容错检查返回值） */
    if (disater_check_timer) {
        esp_err_t err = esp_timer_stop(disater_check_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "esp_timer_stop failed: %s", esp_err_to_name(err));
        }
        err = esp_timer_delete(disater_check_timer);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_timer_delete failed: %s", esp_err_to_name(err));
        }
        disater_check_timer = NULL;
    }
    
    reals.flasWrFlag.sBit.disaster_warn = 1;
    ESP_LOGW(TAG, "disater_check_callback done, uuid=%u, reason_all=0x%04X",
             (unsigned)DisasterSaveData.disaster_send_param.uuid,
             (unsigned)DisasterSaveData.disaster_send_param.reason.all);
}


/**
 * @brief 创建并启动一次性备电检查定时器（延时触发）
 * @note 采用 esp_timer_start_once，避免回调内立即停止/删除与 periodic 冲突
 * @param reason_all 要写入 disaster_data.reason.all 的值（0 表示使用默认行为）
 */
static void disater_backup_power_check(uint16_t reason_all)
{
    if (disater_check_timer != NULL) {
        ESP_LOGW(TAG, "disater_check_timer already exists");
        return;
    }

    /* 保存外部传入的 reason 到静态变量，回调通过 arg 读取 */
    pending_disaster_reason_all = reason_all;

    const esp_timer_create_args_t timer_args = {
        .callback = &disater_check_callback,
        .arg = &pending_disaster_reason_all,
        .name = "disater_check_timer"
    };

    esp_err_t err = esp_timer_create(&timer_args, &disater_check_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(err));
        disater_check_timer = NULL;
        return;
    }

    /* 启动一次性定时器：延时 6 秒（单位微秒） */
    const int64_t delay_us = 6LL * 1000 * 1000;
    err = esp_timer_start_once(disater_check_timer, delay_us);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_start_once failed: %s", esp_err_to_name(err));
        esp_timer_delete(disater_check_timer);
        disater_check_timer = NULL;
        return;
    }

    ESP_LOGW(TAG, "Disater_check_timer started (one-shot, 6s), reason_all=0x%08X", reason_all);
}

/**
 * @brief 外部取消未触发的 disater_check_timer（若已触发或回调在执行，按 esp_timer 返回处理）
 */
static void disater_check_timer_cancel(void)
{
    if (disater_check_timer == NULL) {
        ESP_LOGI(TAG, "disater_check_timer already NULL");
        return;
    }

    esp_err_t err;

    /* 尝试停止定时器（若为 one-shot 且尚未触发，可安全停止） */
    err = esp_timer_stop(disater_check_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "esp_timer_stop failed: %s", esp_err_to_name(err));
    }

    /* 删除定时器句柄 */
    err = esp_timer_delete(disater_check_timer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_timer_delete failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "disater_check_timer cancelled and deleted");
        disater_check_timer = NULL;
    }
}

/*------------------------------------------------------------------------------
 Function: disater_warn_state_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      风暴预警状态更新
  * @param[in]  thunder_ctrl_t *now_cfg  
  * @param[out] None
  * @return     static void
  */
static void disater_warn_state_update(thunder_ctrl_t *now_cfg)
{  
#ifdef THUNDER_DATA_UPDATE_DEBUG
    _SaveWebStormWatchApiData test_Data = {
        .StartTime = 1745742121,//2025-04-27 16:22:01
        .EndTime = 1748334121,//2025-05-27 16:22:01
     };
    _SaveWebStormWatchApiData *thunder_data = &test_Data;
#else
    _SaveWebStormWatchApiData *thunder_data = &DisasterSaveData.SaveWebStormWatchApiData;
#endif

    uint16_t system_soc = top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.soc;

    /*时间无效时暂不执行*/
    if( !reals.rtc_flag.sBit.Time_valid ) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.all = now_cfg->all;
        return;
    }

    /*检查风暴预警功能是否开启*/
    if ( now_cfg->thunder_enable == 1 )
    {
        /*判断时间设置是否有效*/
        if (( thunder_data->StartTime != 0 ) && ( thunder_data->EndTime != 0 ))
        {
            // UTC时间戳
            time_t timestamp = time(NULL);

            if ( now_cfg->thunder_cancel == 1 )
            {
                /* 取消当前风暴事件，清空预警信息 */
                now_cfg->thunder_cancel = 0;
                now_cfg->thunder_work_mode_state = 0;
                ESP_LOGW(TAG, "[disater_warn_state_update]  thunder_cancel, start(%ld), end(%ld), now(%lld)", thunder_data->StartTime, thunder_data->EndTime, timestamp);
                thunder_data->StartTime = 0;
                thunder_data->EndTime = 0;
                thunder_data->reason.bit.app_cancel = 1;
                reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
                reals.flasWrFlag.sBit.disaster_warn = 1;
            }
            else if (( thunder_data->StartTime <= timestamp ) && ( thunder_data->EndTime > timestamp ))
            {
                /*处于风暴预警时段*/
                if ( now_cfg->thunder_work_mode_state != 1 )
                {
                    uint16_t soc_threshold = (thunder_data->SoC > (backup_soc_limit + backup_soc_min)) ? (thunder_data->SoC - backup_soc_limit) : backup_soc_min;
                
                    // 收到风暴预警进入时段后第一次执行
                    if ( thunder_data->state.bits.first_flag )
                    {
                        // SOC低于目标值，下发备电模式指令
                        if ( system_soc < thunder_data->SoC )
                        {
                            now_cfg->thunder_work_mode_state = 1;
                            thunder_data->state.bits.first_flag = 0;
                            ESP_LOGW(TAG, "[disater_warn_state_update]  first_flag : thunder_state(%d), start(%ld), now(%lld), soc(%d)", now_cfg->thunder_work_mode_state, thunder_data->StartTime, timestamp, system_soc);
                            reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
                            reals.flasWrFlag.sBit.disaster_warn = 1;
                        }
                        else
                        {
                            /*上报结束，表示收到预警（SOC达到目标system_soc）*/
                            disater_backup_power_check(0x04);
                            thunder_data->state.bits.first_flag = 0;
                            reals.flasWrFlag.sBit.disaster_warn = 1;
                        }
                    }
                    // 预警期间电量低于阈值，重新启动备电
                    else if ( system_soc <= soc_threshold )
                    {
                        now_cfg->thunder_work_mode_state = 1;
                        ESP_LOGW(TAG, "[disater_warn_state_update]  soc_threshold : thunder_state(%d), start(%ld), now(%lld), soc(%d)", now_cfg->thunder_work_mode_state, thunder_data->StartTime, timestamp, system_soc);
                        reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
                    }
                }
                else
                {
                    // 预警期间备电电量满足期望值，停止备电
                    if (system_soc >= thunder_data->SoC)
                    {
                        now_cfg->thunder_work_mode_state = 0;
                        thunder_data->reason.bit.soc_reach = 1;
                        ESP_LOGW(TAG, "[disater_warn_state_update]  Soc OK : thunder_state(%d), end(%ld), now(%lld), soc(%d)", now_cfg->thunder_work_mode_state, thunder_data->EndTime, timestamp, system_soc);
                        reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
                    }
                }
            }
            else if (( thunder_data->StartTime < timestamp ) && ( thunder_data->EndTime <= timestamp ))
            {
                /*风暴预警时段结束, 清空预警信息*/
                if ( now_cfg->thunder_work_mode_state == 1 )
                {
                    now_cfg->thunder_work_mode_state = 0;
                    thunder_data->reason.bit.timeout = 1;
                    ESP_LOGW(TAG, "[disater_warn_state_update]  Time end : thunder_state(%d), start(%ld), end(%ld), now(%lld)", now_cfg->thunder_work_mode_state, thunder_data->StartTime, thunder_data->EndTime, timestamp);
                    thunder_data->StartTime = 0;
                    thunder_data->EndTime = 0;
                    reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
                    reals.flasWrFlag.sBit.disaster_warn = 1;
                }
            }
            else
            {
                /*风暴预警时段未生效*/
                if ( now_cfg->thunder_work_mode_state != 0 ) {
                    now_cfg->thunder_work_mode_state = 0;   // 正常不触发
                    ESP_LOGE(TAG, "[disater_warn_state_update]  Time error, start(%ld), end(%ld), now(%lld)", thunder_data->StartTime, thunder_data->EndTime, timestamp);
                    reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
                }
            }
        }
        else
        {
            /*无风暴预警有效信息*/
            if(now_cfg->thunder_work_mode_state != 0) {
                now_cfg->thunder_work_mode_state = 0;   // 正常不触发
                ESP_LOGE(TAG, "[disater_warn_state_update]  Info invalid");
                reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
            }
        }
    }
    else
    {
        /*风暴预警未使能*/
        if(now_cfg->thunder_work_mode_state != 0) {
            now_cfg->thunder_work_mode_state = 0;
            thunder_data->reason.bit.disable = 1;
            ESP_LOGE(TAG, "[disater_warn_state_update]  Mode disable");
            reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
        }
    }

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.all = now_cfg->all;
}

/*------------------------------------------------------------------------------
 Function: disater_warn_dev_send_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      向下级发送备电命令
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t disater_warn_dev_send_task(void)
{
    static uint8_t running_flag = 0;
    uint8_t update_flag = 0;
    
    // UTC时间戳
    time_t timestamp = time(NULL);
    
    if ( DisasterSaveData.disaster_send_param.dev_send_flag.bit.start )
    {
        /*发送备电命令*/
        DisasterSaveData.disaster_send_param.old_work_mode = top_modbus_rd.Inv[INV_MAX_NUM].mod_reg02000_Inv_base_set.work_mode;
        Single_Reg_Set_to_IoT(MD_SUMMARY_ADDR, 2005, 4); 

        /*预备执行下一步*/
        DisasterSaveData.disaster_send_param.dev_send_flag.bit.start = 0;
        DisasterSaveData.disaster_send_param.dev_send_flag.bit.running = 1;

        ESP_LOGW(TAG, "Disaster Cmd start: old_work_mode=%d", DisasterSaveData.disaster_send_param.old_work_mode);
        update_flag |= 1;
    }
    else if ( DisasterSaveData.disaster_send_param.dev_send_flag.bit.end )
    {
        /*发送工作模式恢复命令*/
        Single_Reg_Set_to_IoT(MD_SUMMARY_ADDR, 2005, DisasterSaveData.disaster_send_param.old_work_mode); 

        /*停止执行下一步*/
        DisasterSaveData.disaster_send_param.dev_send_flag.bit.running = 0;
        DisasterSaveData.disaster_send_param.dev_send_flag.bit.end = 0;

        ESP_LOGW(TAG, "Disaster Cmd end!");
        update_flag |= 1;
    }
    else if ( DisasterSaveData.disaster_send_param.dev_send_flag.bit.running )
    {
        if ( IotSetData.dev_info_t.thunder_ctrl.thunder_work_mode_state )
        {
            if ( running_flag == 0 )
            {
                // 设备预警期间重新上电，重新设置下级ARM
                ESP_LOGW(TAG, "Disaster Cmd restart!");
                Single_Reg_Set_to_IoT(MD_SUMMARY_ADDR, 2005, 4); 
            }
            else if( DisasterSaveData.disaster_send_param.dev_send_flag.bit.update ) 
            {
                // 满足新的风暴预警时段，云端更新结束及开始记录，设备侧不变
                if (( disater_warn_param.StartTime <= timestamp ) && ( disater_warn_param.EndTime > timestamp ))
                {
                    if ( IotSetData.dev_info_t.thunder_ctrl.thunder_state )
                    {
                        // 发送云端结束信息，预备更新新预警记录
                        DisasterSaveData.disaster_send_param.icloud_send_flag.bit.update = 1;
                        update_flag |= 1;
                    }
                    else
                    {                        
                        // 直接更新参数至本地
                        disater_warn_param.uuid = DisasterSaveData.SaveWebStormWatchApiData.uuid;
                        disater_warn_param.state.bits.first_flag = 1;
                        DisasterSaveData.SaveWebStormWatchApiData = disater_warn_param;
                        reals.flasWrFlag.sBit.disaster_warn = 1;
                        DisasterSaveData.disaster_send_param.dev_send_flag.bit.update = 0;
                        update_flag |= 1;

                        ESP_LOGW(TAG, "Disaster Update!");
                    }
                }
            }
        }
        else
        {
            /*保护措施，避免卡在备电模式，理论上不执行*/
            ESP_LOGE(TAG, "Disaster running: thunder_state=0, exit backup mode");
            DisasterSaveData.disaster_send_param.dev_send_flag.bit.running = 0;
            DisasterSaveData.disaster_send_param.icloud_send_flag.bit.running = 0;
            update_flag |= 1;
        }
    }
    /*本地有新的风暴预警待生效*/
    else if ( DisasterSaveData.disaster_send_param.dev_send_flag.bit.update )
    {
        // 直接更新参数至本地
        disater_warn_param.uuid = DisasterSaveData.SaveWebStormWatchApiData.uuid;
        disater_warn_param.state.bits.first_flag = 1;
        DisasterSaveData.SaveWebStormWatchApiData = disater_warn_param;
        reals.flasWrFlag.sBit.disaster_warn = 1;
        DisasterSaveData.disaster_send_param.dev_send_flag.bit.update = 0;
        update_flag |= 1;

        ESP_LOGW(TAG, "Disaster Update!");
    }
    
    running_flag = DisasterSaveData.disaster_send_param.dev_send_flag.bit.running;

    return update_flag;
}

/*------------------------------------------------------------------------------
 Function: disater_warn_icloud_send_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      向云端发送风暴预警数据
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t disater_warn_icloud_send_task(void)
{
    static uint8_t running_flag = 0;
    static uint8_t retry_num = 0;
    uint8_t update_flag = 0;
    bool result = false;

    /*无网情况下不执行*/
    if(!top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.mqtt) return update_flag;
    
    if ( DisasterSaveData.disaster_send_param.icloud_send_flag.bit.start )
    {                
        /*备电开始数据上报*/
		result = https_disater_state_to_host(DisasterSaveData.disaster_send_param, 1, DisasterSaveData.disaster_send_param.msg_id);

        if ( result 
|| ( ++retry_num >= retry_to_host_max_num ))
        {
            DisasterSaveData.disaster_send_param.icloud_send_flag.bit.start = 0;
            update_flag |= 1;
        }
    }
    else if ( DisasterSaveData.disaster_send_param.icloud_send_flag.bit.end )
    {        
        /*备电结束数据上报*/
		result = https_disater_state_to_host(DisasterSaveData.disaster_send_param, 2, DisasterSaveData.disaster_send_param.msg_id);
        
        if ( result 
|| ( ++retry_num >= retry_to_host_max_num ))
        {
            DisasterSaveData.disaster_send_param.icloud_send_flag.bit.running = 0;
            DisasterSaveData.disaster_send_param.icloud_send_flag.bit.end = 0;
            update_flag |= 1;
        }
    }
    else if ( DisasterSaveData.disaster_send_param.icloud_send_flag.bit.running )
    {
#if 0    
        /*上电后重新进行备电数据上报*/
        if ( running_flag == 0 )
        {
            result = https_disater_state_to_host(DisasterSaveData.disaster_send_param, 1, DisasterSaveData.disaster_send_param.msg_id);
        }
#endif        
    }

    running_flag = DisasterSaveData.disaster_send_param.icloud_send_flag.bit.running;

    // 上报完成，超时清零
    if(update_flag) retry_num = 0;
    
    return update_flag;
}

  /*------------------------------------------------------------------------------
   Function: disater_warn_process_task
   -----------------------------------------------------------------------------*/
  /**
    * @brief      风暴预警备电任务
    * @param[in]  void  
    * @param[out] None
    * @return     void
    */
void disater_warn_process_task(void)
{
    static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if((now_time - pre_time) >= 1000)
    {
        pre_time = now_time;

        thunder_ctrl_t old_cfg = IotSetData.dev_info_t.thunder_ctrl;
        thunder_ctrl_t *now_cfg = &IotSetData.dev_info_t.thunder_ctrl;
        uint8_t update_flag = 0;

        /*时间无效时暂不执行*/
        if( !reals.rtc_flag.sBit.Time_valid ) return;

        /*风暴预警状态更新*/
        disater_warn_state_update(now_cfg);

        if ( old_cfg.thunder_work_mode_state != now_cfg->thunder_work_mode_state )
        {
            if ( now_cfg->thunder_work_mode_state == 1 ) {
                /*进入风暴预警模式*/
                DisasterSaveData.disaster_send_param.dev_send_flag.bit.start = 1;
                DisasterSaveData.SaveWebStormWatchApiData.reason.all = 0;
                /*超时检查备电状态，未实际备电上报结束，表示收到预警（电池放电或待机）*/
                disater_backup_power_check(0x10);
                update_flag |= 1;
            } else {
                /*退出风暴预警模式*/
                DisasterSaveData.disaster_send_param.dev_send_flag.bit.end = 1;
                DisasterSaveData.disaster_send_param.reason.all = DisasterSaveData.SaveWebStormWatchApiData.reason.all;
                update_flag |= 1;
            }  
        }

        if ( DisasterSaveData.disaster_send_param.dev_send_flag.status != 0 )
        {
            /*向下级发送备电命令*/
            update_flag |= disater_warn_dev_send_task();
        }

        /*实际备电状态更新*/
        if ( DisasterSaveData.disaster_send_param.icloud_send_flag.bit.update ) {
            now_cfg->thunder_state = 0;
            DisasterSaveData.disaster_send_param.reason.bit.new_event = 1;
            DisasterSaveData.disaster_send_param.icloud_send_flag.bit.update = 0;
            update_flag |= 1;
        } else {
            now_cfg->thunder_state = (1 == now_cfg->thunder_work_mode_state) & (1 == top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.chg_status);
        }
        
        if ( old_cfg.thunder_state != now_cfg->thunder_state )
        {
            if ( now_cfg->thunder_state == 1 ) {
                /*停止备电检查*/
                disater_check_timer_cancel();
                
                /*更新UUID*/
                DisasterSaveData.SaveWebStormWatchApiData.uuid++;
                reals.flasWrFlag.sBit.disaster_warn = 1;
            
                /*初始化相关备电数据*/
                DisasterSaveData.disaster_send_param.soc_start = top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.soc;
                DisasterSaveData.disaster_send_param.soc_end = top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.soc;
                DisasterSaveData.disaster_send_param.BackupStartTime = time(NULL);
                DisasterSaveData.disaster_send_param.BackupEndTime = time(NULL);
#if PACK_MAX_NUM > 1               
                DisasterSaveData.disaster_send_param.total_chg_energy_begin = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.total_chg_energy * 100;    // 6019~6020
#else
                DisasterSaveData.disaster_send_param.total_chg_energy_begin = top_modbus_rd.Pack[0].mod_reg06100_Pack_each.total_chg_energy;    // 6133~6134
#endif
                DisasterSaveData.disaster_send_param.total_chg_energy_end = DisasterSaveData.disaster_send_param.total_chg_energy_begin;

                DisasterSaveData.disaster_send_param.uuid = DisasterSaveData.SaveWebStormWatchApiData.uuid;
                memcpy(DisasterSaveData.disaster_send_param.msg_id, DisasterSaveData.SaveWebStormWatchApiData.msg_id, sizeof(DisasterSaveData.SaveWebStormWatchApiData.msg_id));
                DisasterSaveData.disaster_send_param.reason.all = 0;
                
                /*上报备电开始数据*/
                DisasterSaveData.disaster_send_param.icloud_send_flag.status = 0;
                DisasterSaveData.disaster_send_param.icloud_send_flag.bit.start = 1;
                DisasterSaveData.disaster_send_param.icloud_send_flag.bit.running = 1;
                update_flag |= 1;
            } else {
                /*检查结束原因*/
                if (( now_cfg->thunder_work_mode_state ) && ( top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.chg_status != 1 )) {
                    DisasterSaveData.disaster_send_param.reason.bit.discharge = 1;
                }
                
                /*配置相关备电数据*/
                DisasterSaveData.disaster_send_param.soc_end = top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.soc;
                DisasterSaveData.disaster_send_param.BackupEndTime = time(NULL);
#if PACK_MAX_NUM > 1                
                DisasterSaveData.disaster_send_param.total_chg_energy_end = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.total_chg_energy * 100;    // 6019~6020
#else                
                DisasterSaveData.disaster_send_param.total_chg_energy_end = top_modbus_rd.Pack[0].mod_reg06100_Pack_each.total_chg_energy;    // 6133~6134
#endif                
                /*上报备电结束数据*/
                DisasterSaveData.disaster_send_param.icloud_send_flag.bit.end = 1;
                update_flag |= 1;
            }  
            old_cfg.thunder_state = now_cfg->thunder_state;
            reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
        }
        
        if ( DisasterSaveData.disaster_send_param.icloud_send_flag.status != 0 )
        {
            /*向云端发送风暴预警数据*/
            update_flag |= disater_warn_icloud_send_task();
        }

        if(update_flag) reals.flasWrFlag.sBit.disaster_warn = 1;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.all = IotSetData.dev_info_t.thunder_ctrl.all;
        top_modbus_rd.Inv[INV_MAX_NUM].mod_reg12000_IOT_set.thunder_ctrl.all = IotSetData.dev_info_t.thunder_ctrl.all;
    }
}

#endif

