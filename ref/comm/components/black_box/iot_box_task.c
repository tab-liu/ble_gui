/**
  ******************************************************************************
  * @file      iot_box_task.c
  * @version   1.0
  * @author    tcenter414
  * @date      2026/01/23
  * @brief     黑匣子
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/21  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */
  
/* ================================ 库文件引用 ================================ */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "iot_box_task.h"
#include "utils.h"
#include "comm_define.h"

#include "parameter.h"
#include "modbus_data.h"
#include "can_protocol.h"

/* ================================ 文件内宏定义 ================================ */

static const char *TAG = "[IOT_BOX_TASK]";

/* =============================== 文件内全局变量 ================================ */

#ifdef INV_LOG_DETAILED_INFO_RECORD

// log信息记录队列
static QueueHandle_t xQueue_inv_Detailed_Info_record = NULL; 

// 详细数据操作信号量
static SemaphoreHandle_t inv_detail_info_mutex = NULL;

// 黑匣子事件日志文件操作互斥信号量
static SemaphoreHandle_t xBoxFileProcessSemaphore = NULL;

// 逆变器详细数据记录
static USE_EXT_RAM_BSS Inv_Detailed_Info_Datas inv_Detail_Info_Data[(DEV_MAIN_NODE_MAX*INV_MAX_NUM)+1];

/* ================================ 模块函数定义 ================================ */

/**
 * @brief 创建iot_box_task任务及逆变器log信息队列
 *
 * 初始化逆变器详细信息记录队列，若创建失败则打印错误并中止程序。
 *
 * @param 无
 * @return 无
 */
void box_task_init(void)
{
    // log信息记录队列初始化    
    if (xQueue_inv_Detailed_Info_record == NULL) {
        xQueue_inv_Detailed_Info_record = xQueueCreate(16, sizeof(Log_Inv_Detailed_Info_Queue_Struct));
        if (xQueue_inv_Detailed_Info_record == NULL) {
            ESP_LOGE(TAG, "xQueue_inv_Detailed_Info_record queue create failed");
            abort();
        }
    }

    // 详细数据操作信号量初始化
    if (inv_detail_info_mutex == NULL) {
        inv_detail_info_mutex = xSemaphoreCreateMutex();
        if (inv_detail_info_mutex == NULL) {
            ESP_LOGE(TAG, "inv_detail_info_mutex create failed");
            abort();
        }
    }
    
    // 日志文件操作互斥信号量,防止多任务同时访问
    if (xBoxFileProcessSemaphore == NULL) { 
        xBoxFileProcessSemaphore = xSemaphoreCreateMutex();
        if (xBoxFileProcessSemaphore == NULL) {
            ESP_LOGE(TAG, "xBoxFileProcessSemaphore Create failed");
            abort();
        }
    }
}

/**
 * @brief    基于Modbus数据结构更新逆变器详细Log信息
 *
 * 从Modbus读取的数据结构体中提取逆变器和电池相关信息，更新到详细Log信息结构体中。
 *
 * @param[in]  node_id      设备序号
 * @param[in]  detailedData 逆变log信息结构体指针
 * @return     void
 */
static void Inv_Detailed_Info_Refresh(uint8_t node_id, Inv_Detailed_Info_Datas* detailedData)
{
    MOD_STRUCT_Inv *Inv_Modbus_Data = NULL;
    MOD_STRUCT_Pack *Pack_Modbus_Data = NULL;

    // 根据node_id选择对应的数据源
    if ((DEV_MAIN_NODE_MAX * INV_MAX_NUM) == node_id) {
#if (DEV_MAIN_NODE_MAX * INV_MAX_NUM > 1)        
        Inv_Modbus_Data = &top_modbus_rd.Inv[INV_MAX_NUM];
#else
        Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
#endif
        Pack_Modbus_Data = &top_modbus_rd.Pack[PACK_MAX_NUM];
    } else {
        Inv_Modbus_Data = &top_modbus_rd.Inv[node_id];
        Pack_Modbus_Data = &top_modbus_rd.Pack[node_id];
    }

    // 固定字段赋值
    detailedData->cur_DetailedInfo.ver = 1;
    detailedData->cur_DetailedInfo.alarmCode = 0;

    // 逆变器相关信息
    detailedData->cur_DetailedInfo.invWorkState    = Inv_Modbus_Data->mod_reg00100_AppPage1.inv_work_state;
    detailedData->cur_DetailedInfo.setCtrlWorkMode = Inv_Modbus_Data->mod_reg02000_Inv_base_set.work_mode;
    detailedData->cur_DetailedInfo.gridFreq        = Inv_Modbus_Data->mod_reg01300_Inv_grid.freq;
    detailedData->cur_DetailedInfo.grid1Voltage    = Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[0].input_voltage;
    detailedData->cur_DetailedInfo.grid1Current    = Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[0].input_current;
    detailedData->cur_DetailedInfo.grid2Voltage    = Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[1].input_voltage;
    detailedData->cur_DetailedInfo.grid2Current    = Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[1].input_current;

    detailedData->cur_DetailedInfo.inv1Voltage     = Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[0].voltage;
    detailedData->cur_DetailedInfo.inv1Current     = Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[0].current;
    detailedData->cur_DetailedInfo.inv2Voltage     = Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[1].voltage;
    detailedData->cur_DetailedInfo.inv2Current     = Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[1].current;

    detailedData->cur_DetailedInfo.acLoad1Voltage  = Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[0].load_voltage;
    detailedData->cur_DetailedInfo.acLoad1Current  = Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[0].load_current;
    detailedData->cur_DetailedInfo.acLoad2Voltage  = Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[1].load_voltage;
    detailedData->cur_DetailedInfo.acLoad2Current  = Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[1].load_current;

    detailedData->cur_DetailedInfo.pv1Voltage      = Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[0].input_voltage;
    detailedData->cur_DetailedInfo.pv1Current      = Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[0].input_current;
    detailedData->cur_DetailedInfo.pv2Voltage      = Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[1].input_voltage;
    detailedData->cur_DetailedInfo.pv2Current      = Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[1].input_current;

    detailedData->cur_DetailedInfo.ambientTemp     = Inv_Modbus_Data->mod_reg01500_Inv_inv.AmbientTemp;
    detailedData->cur_DetailedInfo.invMaxTemp      = Inv_Modbus_Data->mod_reg01500_Inv_inv.InvMaxTemp;
    detailedData->cur_DetailedInfo.pvDcdcMaxTemp   = Inv_Modbus_Data->mod_reg01500_Inv_inv.PvDcdcMaxTemp;

    // 电池相关信息
    detailedData->cur_DetailedInfo.packTotalVoltage = Pack_Modbus_Data->mod_reg06000_Pack_sum.total_voltage;
    detailedData->cur_DetailedInfo.packTotalCurrent = Pack_Modbus_Data->mod_reg06000_Pack_sum.total_current;
    detailedData->cur_DetailedInfo.soc              = Pack_Modbus_Data->mod_reg06000_Pack_sum.soc;
    detailedData->cur_DetailedInfo.soh              = Pack_Modbus_Data->mod_reg06000_Pack_sum.soh;
    detailedData->cur_DetailedInfo.packRunStatus    = Pack_Modbus_Data->mod_reg06000_Pack_sum.work_status;
    detailedData->cur_DetailedInfo.cellMinVoltage   = Pack_Modbus_Data->mod_reg06100_Pack_each.min_cell_voltage;
    detailedData->cur_DetailedInfo.cellMaxVoltage   = Pack_Modbus_Data->mod_reg06100_Pack_each.max_cell_voltage;
    detailedData->cur_DetailedInfo.cellMinTemp      = Pack_Modbus_Data->mod_reg06100_Pack_each.min_temp_value;
    detailedData->cur_DetailedInfo.cellMaxTemp      = Pack_Modbus_Data->mod_reg06100_Pack_each.max_temp_value;
    detailedData->cur_DetailedInfo.packCycle        = Pack_Modbus_Data->mod_reg06100_Pack_each.cycle_count;

    // 39000寄存器未赋值，相关代码已注释
}


/**
 * @brief    逆变器详细log信息记录存储到队列缓存
 *
 * 申请内存保存逆变器详细信息，并将其封装后发送到队列缓存。
 *
 * @param[in]  node_id 信息存储的文件序号
 * @param[in]  udata   数据指针（指向 Inv_Detailed_Info_Datas 结构体）
 * @return     uint8_t 0:成功 1:申请内存失败 2:发送队列失败
 */
static uint8_t Inv_Detailed_Log_Push(uint8_t node_id, void* udata)
{
    uint8_t rtn = 0;
    Log_Inv_Detailed_Info_Queue_Struct queue_msg = {NULL};

    queue_msg.pdata = (Inv_Detailed_Info_Datas *)iot_calloc(sizeof(Inv_Detailed_Info_Datas));
    if (!queue_msg.pdata)
    {
        ESP_LOGE(TAG, "Inv_Detailed_Log_Push malloc failed");
        rtn = 1;
    }
    else
    {
#if 0    
        ESP_LOGI(TAG, "Inv_Detailed_Log_Push inv_Detail_Info_Data:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, udata, sizeof(Inv_Detailed_Info_Datas), ESP_LOG_DEBUG);
#endif        
        memcpy(queue_msg.pdata, (Inv_Detailed_Info_Datas *)udata, sizeof(Inv_Detailed_Info_Datas));

        // 设备序号处理：（汇总）转为0
        if ((DEV_MAIN_NODE_MAX*INV_MAX_NUM) == node_id) {
            node_id = 0;
        } else {
            node_id = node_id + 1;
        }
        queue_msg.inv_dev_num = node_id;

        // 消息保存到队列
        if (xQueueSendToBack((QueueHandle_t)xQueue_inv_Detailed_Info_record, &queue_msg, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Inv_Detailed_Log_Push message push queue failed");
            free(queue_msg.pdata);
            queue_msg.pdata = NULL;
            rtn = 2;
        }
    }
    return rtn;
}

/**
 * @brief    基于触发故障/报警事件做log信息存储
 *
 * 根据故障或报警事件，更新逆变器详细信息结构体，并进行日志记录和队列推送。
 *
 * @param[in]  alarmCode  故障/报警码
 * @param[in]  happen     故障/报警状态（0：消失，1：发生）
 * @param[in]  node_id    设备序号
 * @param[in]  now_date   当前时间戳（秒级）
 * @return     void
 */
static void Inv_Info_Update_OnErr(uint16_t alarmCode, uint8_t happen, uint8_t node_id, uint64_t now_date)
{
    if (inv_detail_info_mutex) {
        if (xSemaphoreTake(inv_detail_info_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            ESP_LOGW(TAG, "Inv_Info_Update_OnErr: mutex timeout, skip update");
            return;
        }
    }

    time_t cur_time = now_date;
    time_t pre_time = inv_Detail_Info_Data[node_id].cur_DetailedInfo.curTime;
    if(( pre_time == 0 ) || ( pre_time == cur_time )) {
        pre_time = cur_time - 1;
    }

#if 0   // 避免多条记录同时产生导致前次信息迅速刷新失去意义，仅周期1s更新
    // 保存前一次详细信息
    memcpy(&inv_Detail_Info_Data[node_id].pre_DetailedInfo,
           &inv_Detail_Info_Data[node_id].cur_DetailedInfo,
           sizeof(Inv_Detailed_Info));
#endif

    // 更新时间戳
    inv_Detail_Info_Data[node_id].cur_DetailedInfo.curTime = cur_time;
    inv_Detail_Info_Data[node_id].pre_DetailedInfo.curTime = pre_time;
    
    // 更新设备序号 (0:汇总信息；0xFE:自身信息; 0xFF:无故障; 1~N:具体的故障包序号)
    if ( (DEV_MAIN_NODE_MAX*INV_MAX_NUM) == node_id ) {
        inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.inv_parallel_seq = 0;
        inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.pack_parallel_seq = 0;
    } else if ( reals.Addr_can_self == node_id ) {
        inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.inv_parallel_seq = 0xFE;
        inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.pack_parallel_seq = 0xFE;
    } else {
        inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.inv_parallel_seq = node_id;
        inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.pack_parallel_seq = node_id;
    }
    inv_Detail_Info_Data[node_id].pre_DetailedInfo.device_seq.all = inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.all;
    
    // 刷新详细信息
    Inv_Detailed_Info_Refresh(node_id, &inv_Detail_Info_Data[node_id]);

    // 更新当前报警码
    inv_Detail_Info_Data[node_id].cur_DetailedInfo.alarmCode = alarmCode;
    inv_Detail_Info_Data[node_id].pre_DetailedInfo.alarmCode = 0;

    // 如果是发生事件，设置最高位标志
    if (happen) {
        inv_Detail_Info_Data[node_id].cur_DetailedInfo.alarmCode |= 0x8000;
    }

    // 推送队列
    Inv_Detailed_Log_Push(node_id, &inv_Detail_Info_Data[node_id]);
    
#if 0   // 调试
        ESP_LOGI(TAG, "Inv_Info_Update_OnErr pre_DetailedInfo:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, &inv_Detail_Info_Data[node_id].pre_DetailedInfo, sizeof(inv_Detail_Info_Data[node_id].pre_DetailedInfo), ESP_LOG_DEBUG);
        ESP_LOGI(TAG, "Inv_Info_Update_OnErr cur_DetailedInfo:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, &inv_Detail_Info_Data[node_id].cur_DetailedInfo, sizeof(inv_Detail_Info_Data[node_id].cur_DetailedInfo), ESP_LOG_DEBUG);
#endif

    if (inv_detail_info_mutex) {
        xSemaphoreGive(inv_detail_info_mutex);
    }
}


/**
 * @brief    将故障或报警信息转换为报警码并记录逆变器log信息
 *
 * @param[in]  happenType      0：故障/报警消失  1：故障/报警发生
 * @param[in]  CodeIndex       故障/报警组索引（用于计算报警码偏移）
 * @param[in]  faultInformation 故障/报警信息（每一位代表一种故障/报警）
 * @param[in]  node_id         设备序号（1~3：单机序号 4：汇总序号）
 * @return     void
 */
void SaveErrorDetailInfo(uint8_t happenType, uint16_t CodeIndex, uint16_t faultInformation, uint8_t node_id)
{
    // 获取当前时间戳
    time_t now_date = time(NULL);
    uint16_t alarmCode = 0;

    // 遍历16位故障/报警信息，每一位代表一种故障/报警
    for (uint8_t i = 0; i < 16; i++)
    {
        if (faultInformation & (1 << i))
        {
            // 计算报警码
            alarmCode = i + CodeIndex;
            // 打印报警码及相关信息
            ESP_LOGI(TAG, "SaveErrorDetailInfo: alarmCode=%u, happenType=%u, node_id=%u, time=%llu, bit=%u",
                     alarmCode, happenType, node_id, (unsigned long long)now_date, i);
            // 记录报警信息
            Inv_Info_Update_OnErr(alarmCode, happenType, node_id, now_date);
        }
    }
}

/**
 * @brief    逆变器详细log信息记录写入到文件系统
 *
 * 从队列中取出逆变器详细信息，写入到对应的文件系统中（只记录汇总信息）。
 *
 * @return  uint8_t  0:成功  非0（bit0=1：写入单机文件失败 bit1=1：写入汇总文件失败）
 */
uint8_t Inv_Detailed_Info_Log_Pop(void)
{
    Log_Inv_Detailed_Info_Queue_Struct queue_msg = {NULL};
    char fname[30] = {0};
    uint32_t file_bias = 0;
    uint8_t rtn = 0;

    // 获取文件操作互斥锁，保护整个操作过程
    // 避免获取锁失败导致本次拉取到的记录丢失
    if (xBoxFileProcessSemaphore && xSemaphoreTake(xBoxFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Save_Task failed to take file process semaphore. Records will be discarded.");
        return rtn;
    }

    // 从队列取出一条记录
    if (xQueue_inv_Detailed_Info_record && xQueueReceive(xQueue_inv_Detailed_Info_record, &queue_msg, 0) == pdTRUE)
    {
        uint8_t index = queue_msg.inv_dev_num; // 单个逆变器详细信息

        // 只记录汇总信息（index==0），单机信息直接返回
        if (index != 0) {
            goto pop_exit;
        }

        // 汇总信息写入
        INV_BOX_FILE_PATH_INV_INFO(fname, 0, INV_BOX_PROTOCOL_CODE);
        file_bias = (uint16_t)IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0];

        if (0 == historic_data_write(fname, (uint8_t*)queue_msg.pdata, file_bias, sizeof(Inv_Detailed_Info_Datas)))
        {
            ESP_LOGI(TAG, "File written ok (%s), bias: %d", fname, IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0]);

            // 偏移地址
            IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0]++;
            if (IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0] >= INV_DETAILED_INFO_MAX_COUNT) {
                IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0] = 0;
                IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] = INV_DETAILED_INFO_MAX_COUNT;
            }

            // 存储条数
            if(IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] != INV_DETAILED_INFO_MAX_COUNT) {
                IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] = IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0];
            }

            reals.SetDataWrFlag.sBit.SetDataUpdate_invInfo_cnt = 1;
        }
        else
        {
            ESP_LOGE(TAG, "File written fail:Logger_Pop_Event (%s)", fname);
            rtn |= 0x2;
        }
        
pop_exit:

		if (queue_msg.pdata) 
		{
			free(queue_msg.pdata); 
			queue_msg.pdata = NULL; 
		}
    }

    // 释放文件操作互斥锁
    xSemaphoreGive(xBoxFileProcessSemaphore);

    return rtn;
}

/**
 * @brief 清除已存储的逆变器详细log信息
 *
 * 删除所有逆变器详细信息日志文件，并重置相关索引和计数。
 *
 * @return 0: 清除成功
 */
int clear_DetailInfo_File(void)
{
    char path[50] = {0};
    char fname[30] = {0};

    for (uint8_t i = 0; i <= DEV_MAIN_NODE_MAX; i++)
    {
        INV_BOX_FILE_PATH_INV_INFO(fname, i, INV_BOX_PROTOCOL_CODE);
        sprintf(path, "%s/%s", RECORD_ROOT_PATH, fname);
        ESP_LOGE(TAG, "clear_DetailInfo file name: %s", path);

        /* 只读格式打开文件，检查文件是否存在 */
        int fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            ESP_LOGE(TAG, "[detailed_file] open error, %s", path);
            continue;
        }

        close(fd);
        remove(path);
        IotSetData.dev_info_t.invDetailedInfo_AddrIndex[i] = 0;
        IotSetData.dev_info_t.invDetailedInfo_SaveCount[i] = 0;
        ESP_LOGI(TAG, "clear %s detailinfo ok", path);
    }
    return 0;
}

/**
 * @brief 刷新指定逆变器的详细信息（保存前一次快照并更新当前信息）
 *
 * @param[in] inv_index 逆变器索引
 */
static void Inv_Detailed_Info_update_by_index(uint8_t inv_index)
{
    if ( (DEV_MAIN_NODE_MAX*INV_MAX_NUM) != inv_index ) {
        // 检查逆变器在线状态和设备序列号
        if (!can_node_rd.Inv[inv_index].online || !can_node_rd.Inv[inv_index].inv_about.dev_sn) {
            return;
        }
    }

    // 保存前一次的详细信息
    memcpy(&inv_Detail_Info_Data[inv_index].pre_DetailedInfo,
           &inv_Detail_Info_Data[inv_index].cur_DetailedInfo,
           sizeof(Inv_Detailed_Info));

    // 更新当前时间
    if (1 == reals.rtc_flag.sBit.Time_valid) {
       time_t current_timestamp = time(NULL);
       inv_Detail_Info_Data[inv_index].cur_DetailedInfo.curTime = current_timestamp;
    }
    
    // 更新设备序号 (0:汇总信息；0xFE:自身信息; 0xFF:无故障; 1~N:具体的故障包序号)
    if ( (DEV_MAIN_NODE_MAX*INV_MAX_NUM) == inv_index ) {
        inv_Detail_Info_Data[inv_index].cur_DetailedInfo.device_seq.bit.inv_parallel_seq = 0;
        inv_Detail_Info_Data[inv_index].cur_DetailedInfo.device_seq.bit.pack_parallel_seq = 0;
    } else if ( reals.Addr_can_self == inv_index ) {
        inv_Detail_Info_Data[inv_index].cur_DetailedInfo.device_seq.bit.inv_parallel_seq = 0xFE;
        inv_Detail_Info_Data[inv_index].cur_DetailedInfo.device_seq.bit.pack_parallel_seq = 0xFE;
    } else {
        inv_Detail_Info_Data[inv_index].cur_DetailedInfo.device_seq.bit.inv_parallel_seq = inv_index;
        inv_Detail_Info_Data[inv_index].cur_DetailedInfo.device_seq.bit.pack_parallel_seq = inv_index;
    }

    // 刷新详细信息数据
    Inv_Detailed_Info_Refresh(inv_index, &inv_Detail_Info_Data[inv_index]);
}

/**
 * @brief    inv逆变详细Log信息实时刷新，刷新间隔：1s
 *
 * 每秒刷新所有逆变器（包括汇总信息）的详细Log信息。加锁保护，避免多线程冲突。
 *
 * @return void
 */
void Inv_Detailed_Info_update(void)
{
    static uint32_t pre_time = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 检查是否到达更新间隔（1000ms）
    if ((current_time - pre_time) >= 1000) {
        pre_time = current_time;

        // 互斥保护，避免多线程冲突
        if (inv_detail_info_mutex) {
            // 不阻塞，获取不到直接返回
            if (xSemaphoreTake(inv_detail_info_mutex, 0) != pdTRUE) {
                return;
            }
        }

        // 刷新汇总信息
        Inv_Detailed_Info_update_by_index(DEV_MAIN_NODE_MAX * INV_MAX_NUM);

        // 刷新所有单机逆变器信息（如果有多个）
        if (1 < (DEV_MAIN_NODE_MAX * INV_MAX_NUM)) {
            for (uint8_t addr = 0; addr < (DEV_MAIN_NODE_MAX * INV_MAX_NUM); addr++) {
                Inv_Detailed_Info_update_by_index(addr);
            }
        }

        if (inv_detail_info_mutex) {
            xSemaphoreGive(inv_detail_info_mutex);
        }
    }
}

/**
  * @brief    计算自 since_timestamp 之后的新事件记录数
  * @param[in] since_timestamp  时间戳基准（0 表示统计全部）
  * @return   新记录数（uint16_t）
  *
  * 说明：
  * - 逻辑索引按从0到Event_SaveCount-1排列（0为最早的逻辑记录，当文件未环满时物理索引==逻辑索引）；
  * - 若读取关键位置（最旧/最新）失败，函数返回0表示无法确定或无新记录；
  */
static uint16_t Box_Log_Count_New_Records(time_t since_timestamp)
{
    // 1. 基本参数和路径检查
    char path[30] = {0};
    sprintf(path,"%s/%s%s%02d_%02d", FS_BASE_PATH, INV_BOX_RECORD_FOLDER_LEVEL1, INV_BOX_FILE_NAME, 0, INV_BOX_PROTOCOL_CODE);

    if (IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] == 0) {
        return 0; // 模块未初始化或文件为空，没有新记录
    }

    // 2. 快速路径检查：处理全量同步的情况
    if (since_timestamp == 0) {
        ESP_LOGI(TAG, "since_timestamp is 0, counting all records.");
        return IotSetData.dev_info_t.invDetailedInfo_SaveCount[0];
    }

    // 3. 快速路径检查：与最旧和最新的记录比较
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] < INV_DETAILED_INFO_MAX_COUNT) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0];
    }

    Inv_Detailed_Info_Datas temp_record;
    uint32_t record_offset = oldest_record_p_idx * BOX_LOG_MAX_LEN;
    if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, BOX_LOG_MAX_LEN) == BOX_LOG_MAX_LEN) {
        // 如果 since_timestamp 早于文件中最旧的记录，则所有记录都是新的
        if (since_timestamp < temp_record.cur_DetailedInfo.curTime) {
            ESP_LOGI(TAG, "since_timestamp is older than the oldest record, counting all records.");
            return IotSetData.dev_info_t.invDetailedInfo_SaveCount[0];
        }
    } else {
        ESP_LOGE(TAG, "Failed to read oldest record at index %u for pre-check.", oldest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // 在逻辑上，最新的记录是第 (current_records - 1) 条。
    uint16_t newest_record_l_idx = IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] - 1;
    // 其物理索引 = (最旧记录的物理索引 + 最新记录的逻辑索引) % 数组总容量
    uint16_t newest_record_p_idx = (oldest_record_p_idx + newest_record_l_idx) % INV_DETAILED_INFO_MAX_COUNT;
    
    record_offset = newest_record_p_idx * BOX_LOG_MAX_LEN;
    if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, BOX_LOG_MAX_LEN) == BOX_LOG_MAX_LEN) {
        // 如果 since_timestamp 晚于文件中最新的记录，则所有记录都是旧的
        if (since_timestamp >= temp_record.cur_DetailedInfo.curTime) {
            ESP_LOGW(TAG, "since_timestamp is newer than or equal to the newest record, no new records.");
            return 0;
        }
    } else {
        ESP_LOGE(TAG, "Failed to read newest record at index %u for pre-check.", newest_record_p_idx);
        return 0; // 关键记录读取失败，无法继续
    }

    // --- 4. 二分查找：定位第一个新记录 ---
    // 在逻辑索引 [0, current_records - 1] 上进行二分查找。
    int low = 0, high = IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] - 1;
    int first_new_l_idx = -1; // 第一个新记录的逻辑索引

    while (low <= high) {
        int mid_l_idx = low + (high - low) / 2; // 中间点的逻辑索引

        // 将逻辑索引转换为物理文件索引
        // 当前物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
        uint16_t mid_p_idx = (oldest_record_p_idx + mid_l_idx) % INV_DETAILED_INFO_MAX_COUNT;
        
        record_offset = mid_p_idx * BOX_LOG_MAX_LEN;
        if (fs_file_read_at(path, record_offset, (uint8_t*)&temp_record, BOX_LOG_MAX_LEN) != BOX_LOG_MAX_LEN) {
            ESP_LOGE(TAG, "Binary search failed to read record at physical index %u", mid_p_idx);
            return 0; // 查找失败
        }

        if (temp_record.cur_DetailedInfo.curTime > since_timestamp) {
            // 这是一个新记录。
            // 找到了一个潜在的边界，记录它，并尝试在更早的部分（左半区）寻找更早的新记录。
            first_new_l_idx = mid_l_idx;
            high = mid_l_idx - 1;
        } else {
            // 这是一个旧记录。
            // 在更晚的部分（右半区）寻找新记录。
            low = mid_l_idx + 1;
        }
    }

    // 5. 计算并返回结果
    if (first_new_l_idx != -1) {
        // 新记录的数量 = 总记录数 - 第一个新记录的逻辑索引
        return IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] - first_new_l_idx;
    }

    return 0; // 没有找到任何新记录
}

/**
 * @brief      读取增量日志数据到动态分配的缓冲区。
 * @details    此函数是实现日志增量同步的核心接口。它会：
 *             1. 调用辅助函数  计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
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
int Box_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size)
{
    // 1. 参数校验
    if (out_buffer == NULL || out_size == NULL) {
        return -1;
    }
    
    // 获取文件操作互斥锁，保护整个读操作过程
    if (xBoxFileProcessSemaphore && xSemaphoreTake(xBoxFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to take file process semaphore for reading.");
        return -1;
    }

    int ret = 0;
    uint8_t *buffer = NULL;
    
    *out_buffer = NULL;
    *out_size = 0;

    // 2. 调用辅助函数，获取新记录的数量
    uint16_t new_records_count = Box_Log_Count_New_Records(since_timestamp);

    // 如果没有新记录，则无需分配内存和读取，直接返回成功
    if (new_records_count == 0) {
        ESP_LOGW(TAG, "No new records found since timestamp %lld.", since_timestamp);
        goto exit_point; 
    }

    ESP_LOGI(TAG, "Found %u new records since timestamp %lld. Preparing to read.", new_records_count, since_timestamp);

    // 3. 计算总大小并分配内存
    uint32_t total_size = new_records_count * BOX_LOG_MAX_LEN;
    buffer = (uint8_t *)iot_calloc(total_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes for incremental log.", total_size);
        ret = -1;
        goto exit_point;
    }

    // 4. 读取所有新记录到缓冲区
    char path[30] = {0};
    sprintf(path,"%s/%s%s%02d_%02d", FS_BASE_PATH, INV_BOX_RECORD_FOLDER_LEVEL1, INV_BOX_FILE_NAME, 0, INV_BOX_PROTOCOL_CODE);
    //    首先，找到文件中第一个新记录的物理位置
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] < INV_DETAILED_INFO_MAX_COUNT) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0];
    }
    // 第一个新记录的逻辑索引 = 总记录数 - 新记录数
    uint16_t first_new_l_idx = IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] - new_records_count;
    // 第一个新记录的物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
    uint16_t first_new_p_idx = (oldest_record_p_idx + first_new_l_idx) % INV_DETAILED_INFO_MAX_COUNT;

    // 分一或两块，将所有新记录顺序读入缓冲区
    uint8_t *record_write_ptr = buffer;
    uint16_t space_to_end = INV_DETAILED_INFO_MAX_COUNT - first_new_p_idx;

    if (new_records_count <= space_to_end) {
        // 情况A: 所有新记录都在一个连续的块中
        uint32_t read_offset = first_new_p_idx * BOX_LOG_MAX_LEN;
        if (fs_file_read_at(path, read_offset, record_write_ptr, new_records_count * BOX_LOG_MAX_LEN) != new_records_count * BOX_LOG_MAX_LEN) {
            ESP_LOGE(TAG, "Failed to read continuous block of new records.");
            ret = -1;
            goto exit_point;
        }
    } else {
        // 情况B: 新记录跨越了物理文件的末尾，需要分两次读取
        uint16_t cnt1 = space_to_end;
        uint16_t cnt2 = new_records_count - cnt1;

        // 读取第一部分 (从 first_new_p_idx 到文件末尾)
        uint32_t read_offset1 = first_new_p_idx * BOX_LOG_MAX_LEN;
        if (fs_file_read_at(path, read_offset1, record_write_ptr, cnt1 * BOX_LOG_MAX_LEN) != cnt1 * BOX_LOG_MAX_LEN) {
            ESP_LOGE(TAG, "Failed to read first part of wrapped records.");
            ret = -1;
            goto exit_point;
        }

        // 读取第二部分 (从文件开头到剩余记录结束)
        uint32_t read_offset2 = 0; // 从记录区开头读
        if (fs_file_read_at(path, read_offset2, record_write_ptr + cnt1 * BOX_LOG_MAX_LEN, cnt2 * BOX_LOG_MAX_LEN) != cnt2 * BOX_LOG_MAX_LEN) {
            ESP_LOGE(TAG, "Failed to read second part of wrapped records.");
            ret = -1;
            goto exit_point;
        }
    }

    // 6. 成功，返回缓冲区地址和大小
    *out_buffer = buffer;
    *out_size = total_size;

exit_point:
    // 统一的出口：处理资源释放
    if (ret != 0 && buffer != NULL) {
        // 如果函数执行失败，且内存已分配，则释放内存
        free(buffer);
        *out_buffer = NULL; // 确保外部不会使用悬空指针
        *out_size = 0;
    }

    // 释放文件操作互斥锁
    xSemaphoreGive(xBoxFileProcessSemaphore);
    
    return ret;
}

/*------------------------------------------------------------------------------
 Function: Is_Box_Log_File
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查是否为黑匣子文件
  * @param[in]  const char *fname  
  * @param[out] None
  * @return     int
  */
int Is_Box_Log_File(const char *fname)
{
    // 1. 参数校验
    if (fname == NULL) {
        return -1;
    }

    // 2. 根据传入的参数构建请求的文件路径
    //    fname[0] 是目录, &fname[1] 是文件名
    char request_path[40] = {0};
    char filedir = fname[0];
    char* filename = &fname[1];

    if ( (LOG_RECORD_FOLDER_LEVEL1)[0] != filedir ) {
        // 目录不匹配，不是目标文件
        // ESP_LOGE(TAG, "dir is error ：%c.", filedir);
        return -1;
    }
    
    // 使用 snprintf 安全地构建路径，防止溢出
    snprintf(request_path, sizeof(request_path), "%s/%s%s", FS_BASE_PATH, LOG_RECORD_FOLDER_LEVEL1, filename);

    // 3. 使用与日志模块内部完全相同的宏来生成标准文件名
    char local_log_path[40] = {0};
    sprintf(local_log_path,"%s/%s%s%02d_%02d", FS_BASE_PATH, INV_BOX_RECORD_FOLDER_LEVEL1, INV_BOX_FILE_NAME, 0, INV_BOX_PROTOCOL_CODE);

    // 4. 比较两个路径字符串
    if (strcmp(request_path, local_log_path) == 0) {
        // 两个字符串完全相同，确认是目标日志文件
        return 0;
    }

    // 字符串不匹配，不是目标文件
    // ESP_LOGE(TAG, "request_path is error ：%s.", request_path);
    return -1;
}

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Box_Log_File_Id(char *out_buf, size_t buf_len)
{
    if (out_buf == NULL || buf_len < 2) {
        return -1;
    }

    char local_log_path[30] = {0};
    sprintf(local_log_path,"%s/%s%s%02d_%02d", FS_BASE_PATH, INV_BOX_RECORD_FOLDER_LEVEL1, INV_BOX_FILE_NAME, 0, INV_BOX_PROTOCOL_CODE);

    const char *folder_char = (INV_BOX_RECORD_FOLDER_LEVEL1 && INV_BOX_RECORD_FOLDER_LEVEL1[0]) ?
                                &INV_BOX_RECORD_FOLDER_LEVEL1[0] : NULL;
    if (folder_char == NULL) {
        return -1;
    }

    // 提取文件名部分（最后一个 '/' 之后）
    const char *p = strrchr(local_log_path, '/');
    const char *filename = p ? p + 1 : local_log_path;
    size_t fnlen = strlen(filename);

    if (fnlen + 1 > buf_len) { // 1 字节 folder + fnlen
        return -1;
    }

    out_buf[0] = folder_char[0];
    memcpy(out_buf + 1, filename, fnlen); // 不包含终止符

    return 0;
}

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
int Is_Box_Log_Count_New(time_t since_timestamp)
{
    uint16_t new_count = 0;

    /* 获取信号量保护 */
    if (xBoxFileProcessSemaphore && xSemaphoreTake(xBoxFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "failed to take file process semaphore. Records will be discarded.");
        return -2;
    }

    /* 统计晚于 since_timestamp 的新记录数量（内部处理文件读错时返回0） */
    new_count = Box_Log_Count_New_Records(since_timestamp);

    /* 释放信号量 */
    if (xBoxFileProcessSemaphore) {
        xSemaphoreGive(xBoxFileProcessSemaphore);
    }

    return (int)new_count;
}

#endif
