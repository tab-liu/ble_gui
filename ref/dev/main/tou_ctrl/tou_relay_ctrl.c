/**
  ******************************************************************************
  * @file      tou_relay_ctrl.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/5/29
  * @brief     TOU控制相关功能
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/5/29  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "tou_relay_ctrl.h"
#include <time.h>
#include "can_protocol.h"
#include "iot_period_task.h"

#define RELAY_TAG "[tou_relay_ctrl]"

/*TOU控制相关状态*/
Relay_ctrl_struct Relay_ctrl = {0};

/*TOU设置写入状态*/
BitData_RELAY_FILE_SET Relay_File_W_Flag = {0};

/*本地存储TOU控制相关*/
Relay_SetData_TypeDef RelaySetData = {0};

/*TOU运行所需输入参数*/
static uint8_t System_Time_Init_Flag = 0;
static uint8_t System_Time_Hour = 0;
static uint8_t System_Time_Min = 0;
static uint8_t System_Time_Wday = 0;
static uint8_t System_Soc = 0;

/*TOU设置指令暂存*/
static PowerRelayOnOff_U64_Struct PowerRelay_Set_By_Soc = {0};
static PowerRelayOnOff_U64_Struct PowerRelay_Set_By_Delay = {0};
static PowerRelayOnOff_U64_Struct PowerRelay_Set_By_Time = {0};
/*预约备电状态标志*/
static uint8_t Plan_Backup_Power_Active = 0;  // 预约备电激活标志：0-未激活，1-激活

/*TOU控制首次设置相关逻辑标志*/
static uint8_t System_Time_Ctrl_First_Set = 0;

/*TOU控制参数修改标志*/
static uint8_t Tou_Config_Set_Flag = 0;

/*TOU控制参数立即生效标志*/
static uint8_t Tou_Config_Set_Type_Info[RELAY_CTRL_MAX_NUM] = {0xFF};

/*获取对应继电器设置状态*/
#define GET_POWER_RELAY_STATE(relay, index) ((relay >> (index * 2)) & 0x03)

/*设置对应继电器位置状态*/
#define SET_POWER_RELAY_STATE(relay, index, state) \
    do { \
        relay &= ~(0x03 << (index * 2)); /* 清除对应位 */ \
        relay |= ((state & 0x03) << (index * 2)); /* 设置新值 */ \
    } while (0)

/*设置对应智能路控制是否有效状态*/
#define SET_BIT_BY_ENABLE_STATUS(u32_var, bit_pos, status) \
        do { \
            if ((status) != 0) { \
                (u32_var) |= (1U << (bit_pos)); \
            } else { \
                (u32_var) &= ~(1U << (bit_pos)); \
            } \
        } while(0)

/*指定通道的不同控制方式*/
#define TOU_TYPE_INDEX_SOC_ON      BIT0
#define TOU_TYPE_INDEX_SOC_OFF     BIT1

/*单寄存器写入参数*/
extern void Single_Reg_Set_iot_to_arm(uint16_t RegAddress, uint16_t value);
/*------------------------------------------------------------------------------
 Function: is_any_relay_on_u32
 -----------------------------------------------------------------------------*/
/**
  * @brief      判断继电器状态（0：全部关闭，1：有继电器-
                打开）
  * @param[in]  uint32_t relay_status  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t is_any_relay_on_u32(uint32_t relay_status) {
    for (int i = 0; i < 16; i++) { // 遍历16个继电器
        if (GET_POWER_RELAY_STATE(relay_status, i) == 1) { // 使用宏提取状态并判断是否为打开
            return 1; // 返回1，表示有继电器打开
        }
    }
    return 0; // 如果没有任何继电器打开，返回0
}

/*------------------------------------------------------------------------------
 Function: SyncPowerRelay_U16
 -----------------------------------------------------------------------------*/
/**
  * @brief      u16继电器新设置检查
  * @param[in]  uint16_t *current       
                uint16_t *new_settings  
                uint8_t chl_num         
  * @param[out] None
  * @return     static void
  */
static void SyncPowerRelay_U16(uint16_t *current, uint16_t *new_settings, uint8_t chl_num) {
    if (!current || !new_settings || (chl_num > 8)) {
        return; // 防止空指针
    }

    // 遍历每个继电器的状态
    for (int i = 0; i < chl_num; i++) { // 8 个继电器，每个占 2 位
        uint8_t new_state = GET_POWER_RELAY_STATE(*new_settings, i); // 获取新设置的状态
        if (new_state != 0) { // 忽略无效值（0）
            SET_POWER_RELAY_STATE(*current, i, new_state); // 更新当前继电器状态
        }
    }
}

/*------------------------------------------------------------------------------
 Function: SyncPowerRelay_U32
 -----------------------------------------------------------------------------*/
/**
  * @brief      u32继电器新设置检查
  * @param[in]  uint32_t *current       
                uint32_t *new_settings  
                uint8_t chl_num         
  * @param[out] None
  * @return     static void
  */
static void SyncPowerRelay_U32(uint32_t *current, uint32_t *new_settings, uint8_t chl_num) {
    if (!current || !new_settings || (chl_num > 16)) {
        return; // 防止空指针
    }

    // 遍历每个继电器的状态
    for (int i = 0; i < chl_num; i++) { // 16 个继电器，每个占 2 位
        uint8_t new_state = GET_POWER_RELAY_STATE(*new_settings, i); // 获取新设置的状态
        if (new_state != 0) { // 忽略无效值（0）
            SET_POWER_RELAY_STATE(*current, i, new_state); // 更新当前继电器状态
        }
    }
}

/*------------------------------------------------------------------------------
 Function: SyncPowerRelay_U64
 -----------------------------------------------------------------------------*/
/**
  * @brief      u64继电器新设置检查
  * @param[in]  uint64_t *current       
                uint64_t *new_settings  
                uint8_t chl_num         
  * @param[out] None
  * @return     static void
  */
static void SyncPowerRelay_U64(uint64_t *current, uint64_t *new_settings, uint8_t chl_num) {
    if (!current || !new_settings || (chl_num > 32)) {
        return; // 防止空指针或超出范围
    }

    // 遍历每个继电器的状态
    for (int i = 0; i < chl_num; i++) { // 32 个继电器，每个占 2 位
        uint8_t new_state = GET_POWER_RELAY_STATE(*new_settings, i); // 获取新设置的状态
        if (new_state != 0) { // 忽略无效值（0）
            SET_POWER_RELAY_STATE(*current, i, new_state); // 更新当前继电器状态
        }
    }
}

/*------------------------------------------------------------------------------
 Function: SyncPowerRelaySocArray
 -----------------------------------------------------------------------------*/
/**
  * @brief      同步继电器SOC控制
  * @param[in]  PowerRelay_Soc_Struct *target_array  
                PowerRelay_Soc_Struct *source_array  
                size_t array_size                    
  * @param[out] None
  * @return     void
  */
static void SyncPowerRelaySocArray(PowerRelay_Soc_Struct *target_array, PowerRelay_Soc_Struct *source_array, uint8_t array_size) {
    if (!target_array || !source_array || array_size == 0) {
        return; // 防止空指针或无效数组大小
    }

    // 遍历数组
    for (uint8_t i = 0; i < array_size; i++) {
        // 检查 source_array 的 CmdOffEnable，如果为 1，则将 CmdOffSoc 赋值给 target_array
        if (source_array[i].bit.CmdOffEnable == 1) {
            if(target_array[i].bit.CmdOffSoc != source_array[i].bit.CmdOffSoc) Tou_Config_Set_Type_Info[i] |= TOU_TYPE_INDEX_SOC_OFF;
            target_array[i].bit.CmdOffSoc = source_array[i].bit.CmdOffSoc;
        }

        // 检查 source_array 的 CmdOnEnable，如果为 1，则将 CmdOnSoc 赋值给 target_array
        if (source_array[i].bit.CmdOnEnable == 1) {
            if(target_array[i].bit.CmdOnSoc != source_array[i].bit.CmdOnSoc) Tou_Config_Set_Type_Info[i] |= TOU_TYPE_INDEX_SOC_ON;
            target_array[i].bit.CmdOnSoc = source_array[i].bit.CmdOnSoc;
        }
    }
}

/*------------------------------------------------------------------------------
 Function: SyncChgPlanEnable
 -----------------------------------------------------------------------------*/
/**
  * @brief      同步预约充电使能设置(每个状态占4位)
  * @param[in]  chg_plan_enable_t *current       
                chg_plan_enable_t *new_settings  
  * @param[out] None
  * @return     static void
  */
static void SyncChgPlanEnable(chg_plan_enable_t *current, chg_plan_enable_t *new_settings) {
    if (!current || !new_settings) {
        return; // 防止空指针
    }

    // 遍历4个状态，每个占4位
    for (int i = 0; i < 4; i++) {
        uint8_t new_state = (new_settings->all >> (i * 4)) & 0x0F; // 获取新设置的状态(4位)
        if (new_state != 0) { // 忽略无效值（0）
            // 清除对应的4位，然后设置新值
            current->all &= ~(0x0F << (i * 4)); // 清除对应的4位
            current->all |= ((new_state & 0x0F) << (i * 4)); // 设置新值
        }
    }
}


/*------------------------------------------------------------------------------
 Function: Long_Var_Update_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      长变量设置地址检查
  * @param[in]  uint32_t address_begin  
                uint32_t address_end    
                uint32_t var_start      
                uint32_t var_size       
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t Long_Var_Update_Check(uint32_t address_begin, uint32_t address_end, uint32_t var_start, uint32_t var_size) 
{
    if (var_size == 0) {
        return 0; // 无效输入，直接返回
    }

    uint32_t var_end = var_start + var_size;
    uint8_t update = 0;
    
    if((address_begin <= var_start) && (address_end >= var_end))
    {
        /*整段更新*/
        update = 1;
    }
    else if (((address_begin > var_start) && (address_begin < var_end)) ||
             ((address_end > var_start) && (address_end < var_end)))
    {
        /*部分更新*/
        update = 1;
    }

    return update;
}


/*------------------------------------------------------------------------------
 Function: Group_Update_settings
 -----------------------------------------------------------------------------*/
/**
  * @brief      群组设置同步
  * @param[in]  uint32_t address_begin     
                uint32_t address_end       
                uint32_t group_start       
                uint32_t group_size        
                uint8_t *current_settings  
                uint8_t *new_settings      
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t Group_Update_settings(uint32_t address_begin, uint32_t address_end, uint32_t group_start, uint32_t group_size,
                     uint8_t *current_settings, uint8_t *new_settings) 
{
    if (group_size == 0 || current_settings == NULL || new_settings == NULL) {
        return 0; // 无效输入，直接返回
    }

    uint32_t group_end = group_start + group_size;
    uint8_t update = 0;
    
    if((address_begin <= group_start) && (address_end >= group_end))
    {
        /*整段写入*/
        memcpy(current_settings, new_settings, group_size);
        update = 1;
    }
    else if (((address_begin > group_start) && (address_begin < group_end)) ||
             ((address_end > group_start) && (address_end < group_end)))
    {
        /*部分写入*/
        uint32_t offset_start = (address_begin > group_start) ? (address_begin - group_start) : 0;
        uint32_t offset_end = (address_end < group_end) ? (address_end - group_start) : group_size;
        
        if ((offset_start < group_size) &&
            (offset_end <= group_size) &&
            (offset_start < offset_end)) {
            memcpy(current_settings + offset_start,
                   new_settings + offset_start,
                   (offset_end - offset_start));
            update = 1;
        }
    }

    return update;
}


/*------------------------------------------------------------------------------
 Function: get_cached_uptime_minutes
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取系统运行至今的分钟数
  * @param[in]  None
  * @param[out] None
  * @return     static uint32_t
  */
static uint32_t get_cached_uptime_minutes(void) {
    static uint64_t last_uptime_us = 0;  // 上次获取的系统时间（微秒）
    static uint32_t last_uptime_minutes = 0; // 上次计算的分钟数

    // 获取当前系统时间
    uint64_t current_uptime_us = esp_timer_get_time();

    // 如果系统时间变化不足一分钟，直接返回缓存的分钟数
    if ((current_uptime_us - last_uptime_us) < 60000000) {
        return last_uptime_minutes;
    }

    // 更新缓存
    last_uptime_us = current_uptime_us;
    last_uptime_minutes = (uint32_t)(current_uptime_us / 60000000);

    return last_uptime_minutes;
}

/*------------------------------------------------------------------------------
 Function: convert_to_minutes
 -----------------------------------------------------------------------------*/
/**
  * @brief      uint16_t 时间值转换为总分钟数
  * @param[in]  uint16_t time  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t convert_to_minutes(uint16_t time) {
    uint8_t minutes = time & 0xFF;       // 低字节：分钟
    uint8_t hours = (time >> 8) & 0xFF; // 高字节：小时

    // 检查分钟和小时是否在有效范围内
    if (minutes > 59 || hours > 23) {
        ESP_LOGE(RELAY_TAG, "Invalid time value: hours=%d, minutes=%d", hours, minutes);
        return 0;
    }

    return hours * 60 + minutes; // 转换为总分钟数
}

/*------------------------------------------------------------------------------
 Function: convert_from_minutes
 -----------------------------------------------------------------------------*/
/**
  * @brief      总分钟数转换为uint16_t 时间值
  * @param[in]  uint16_t total_minutes  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t convert_from_minutes(uint16_t total_minutes) {
    // 检查总分钟数是否在一天范围内（0 ~ 1439 分钟）
    if (total_minutes > 1439) {
        ESP_LOGE(RELAY_TAG, "Invalid total minutes value: %d", total_minutes);
        return 0;
    }

    // 计算小时和分钟
    uint8_t hours = total_minutes / 60;       // 小时
    uint8_t minutes = total_minutes % 60;    // 分钟

    // 组合为 uint16_t（高字节：小时，低字节：分钟）
    return (hours << 8) | minutes;
}

/*------------------------------------------------------------------------------
 Function: get_minutes
 -----------------------------------------------------------------------------*/
/**
  * @brief      根据小时和分钟获取时间标签
  * @param[in]  uint8_t hour  
                uint8_t min   
  * @param[out] None
  * @return     int
  */
  int get_minutes(uint8_t hour, uint8_t min) {
    return hour * 60 + min;
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_By_Soc
 -----------------------------------------------------------------------------*/
/**
  * @brief      单个继电器基于SOC的控制逻辑
                0：无效，1：开启，2：关闭
  * @param[in]  uint8_t soc        
                uint8_t soc_on     
                uint8_t soc_off    
                uint8_t *soc_ctrl  
  * @param[out] None
  * @return     static void
  */
static void Relay_Ctrl_By_Soc(uint8_t old_soc, uint8_t soc, uint8_t soc_on, uint8_t soc_off, PowerRelayOnOff_U64_Struct *soc_ctrl, uint8_t index)
{
    /*存在有效设置*/
    if((soc_on <= 100) || (soc_off <= 100))
    {
        // ESP_LOGI(RELAY_TAG, "Relay_Ctrl_By_Soc: soc=%d, soc_on=%d, soc_off=%d, index=%d, soc old=%d", soc, soc_on, soc_off, index, old_soc);
        if ((soc > soc_on) 
            && (soc_on <= 100)
            && ((old_soc <= soc_on) || (Tou_Config_Set_Type_Info[index] & TOU_TYPE_INDEX_SOC_ON))
            )
        {
            // ESP_LOGI(RELAY_TAG, "SET_POWER_RELAY_STATE 1"); 
            /*SOC增加至闭合阈值以上，闭合*/
            SET_POWER_RELAY_STATE(soc_ctrl->all, index, 1);
        }
        else if ((soc < soc_off) 
            && (soc_off <= 100)
            && ((old_soc >= soc_off) || (Tou_Config_Set_Type_Info[index] & TOU_TYPE_INDEX_SOC_OFF))
            )
        {
            /*SOC降低至断开阈值以下，断开*/
            SET_POWER_RELAY_STATE(soc_ctrl->all, index, 2);
        }
        else
        {
            /*SOC控制无动作*/
        }
    }
    Tou_Config_Set_Type_Info[index] &= ~((uint8_t)TOU_TYPE_INDEX_SOC_ON);
    Tou_Config_Set_Type_Info[index] &= ~((uint8_t)TOU_TYPE_INDEX_SOC_OFF);
}

uint8_t Relay_Ctrl_Get_System_Soc(void)
{
    return System_Soc;
}

/**
 * @brief 端口 SOC 阈值是否允许执行定时任务（19000 CmdOnSoc）
 * @note 阈值 0~100 有效；>100(含0x7F) 表示未设置阈值，不限制定时任务
 */
uint8_t Relay_Ctrl_Soc_Allows_Timed_Task(uint8_t relay_index)
{
    uint8_t threshold;

    if (relay_index >= RELAY_CTRL_REAL_NUM) {
        return 1;
    }
    if (0 == Relay_ctrl.bit.soc_ctrl_init) {
        return 1;
    }

    threshold = RelaySetData.Relay_info_t.PowerRelay_SmartSet01.GeneralPowerRelaySoc[relay_index].bit.CmdOnSoc;
    if (threshold > 100) {
        return 1;
    }

    return (System_Soc > threshold) ? 1 : 0;
}

/**
 * @brief 按目标寄存器判断 SOC 是否允许执行 Smart TOU 定时任务
 */
uint8_t Relay_Ctrl_Soc_Allows_Tou_Reg(uint16_t reg_addr, uint16_t reg_value)
{
    uint8_t i;

    switch (reg_addr) {
    case 2011:
        for (i = 0; i < RELAY_CTRL_REAL_NUM; i++) {
            if (!Relay_Ctrl_Soc_Allows_Timed_Task(i)) {
                return 0;
            }
        }
        return 1;
    case 2093:
        for (i = 0; i < RELAY_CTRL_REAL_NUM; i++) {
            if ((reg_value >> (i * 2)) & 0x03) {
                if (!Relay_Ctrl_Soc_Allows_Timed_Task(i)) {
                    return 0;
                }
            }
        }
        return 1;
    default:
        return 1;
    }
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Process_Soc_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      Relay基于soc的控制任务
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Relay_Ctrl_Process_Soc_Task(void)
{
    PowerRelayOnOff_U64_Struct soc_ctrl_set = {0};
    MOD_STRUCT_reg19000 *Panel_Set = &RelaySetData.Relay_info_t.PowerRelay_SmartSet01;
    static uint8_t old_soc = 0;
    uint8_t soc = System_Soc;   // 当前系统soc
    uint8_t ret = 0;

    /*SOC控制参数未初始化*/
    if ( 0 == Relay_ctrl.bit.soc_ctrl_init ) {
        ESP_LOGE(RELAY_TAG, "SOC control parameters not initialized.");
        return;
    }

    /*当前仅在SOC变动时执行，避免单点频繁设置*/
    if (( old_soc != soc ) || Tou_Config_Set_Flag)
    {
        /*检查soc设置*/
        for ( uint8_t i = 0 ; i < RELAY_CTRL_REAL_NUM ; i++ )
        {
            //>100-invalid;0~100-有效,100表示SOC 100%,0x7F表示功能禁止
            PowerRelay_Soc_Struct PowerRelay_Soc = Panel_Set->GeneralPowerRelaySoc[i];

            /*仅在设置生效时传入正确SOC配置*/
            uint8_t CmdOnSoc = PowerRelay_Soc.bit.CmdOnSoc;
            uint8_t CmdOffSoc = PowerRelay_Soc.bit.CmdOffSoc;
            
            /*根据SOC设置执行对应逻辑*/
            Relay_Ctrl_By_Soc(old_soc, soc, CmdOnSoc, CmdOffSoc, &soc_ctrl_set, i);
        }

        /*更新历史SOC值*/
        old_soc = soc;

        /*更新到全局变量*/
        if (PowerRelay_Set_By_Soc.all != soc_ctrl_set.all) {
            PowerRelay_Set_By_Soc.all = soc_ctrl_set.all;
            ret = 1;
            ESP_LOGI(RELAY_TAG, "Updating SOC control settings.");
        }
    }
    /*存在新的SOC设置*/
    if (ret == 1) Relay_ctrl.bit.soc_ctrl_target = 1;

}

/*------------------------------------------------------------------------------
 Function: find_latest_past_time_index
 -----------------------------------------------------------------------------*/
/**
  * @brief      先查找今天已过去的点，没有则依次查找昨、前天……直到找到为止。
  * @param[in]  relay_time_set *time_points  
                int num                      
                uint8_t cur_hour             
                uint8_t cur_min              
                uint8_t today(0=周日, 1=周一, ..., 6=周六)   
                uint8_t target_index
  * @param[out] None
  * @return     int
  */
  static int find_latest_past_time_index(relay_time_set *time_points, int num, uint8_t cur_hour, uint8_t cur_min, uint8_t today, uint8_t target_index) {
    int cur_minutes = get_minutes(cur_hour, cur_min);

    // 1. 先查找今天已过去的时间点
    int latest_index = -1;
    int latest_minutes = -1;
    for (int i = 0; i < num; i++) {
        if (time_points[i].time_label.bit.index == target_index &&
            (time_points[i].time_label.bit.week & (1 << today))) {
            int point_minutes = get_minutes(time_points[i].time_active.bit.hour, time_points[i].time_active.bit.min);
            if (point_minutes <= cur_minutes && point_minutes > latest_minutes) {
                latest_minutes = point_minutes;
                latest_index = i;
            }
        }
    }
    if (latest_index != -1) {
        return latest_index;
    }

#if 0   // HJ: 郭经理确认，只支持本周查询，不跨周(本周指代周一到周日)
    // 2. 如果今天没有，依次往前推一天，找最近的过去时间点
    for (int d = 1; d < 7; d++) {
        uint8_t day = (today + 7 - d) % 7; // 向前推d天
        int max_minutes = -1;
        int max_index = -1;
        for (int i = 0; i < num; i++) {
            if (time_points[i].time_label.bit.index == target_index &&
                (time_points[i].time_label.bit.week & (1 << day))) {
                int point_minutes = get_minutes(time_points[i].time_active.bit.hour, time_points[i].time_active.bit.min);
                if (point_minutes > max_minutes) {
                    max_minutes = point_minutes;
                    max_index = i;
                }
            }
        }
        if (max_index != -1) {
            return max_index;
        }
    }
#else
    // 2. 如果今天没有，往前查找本周的前几天（到周一为止）
    // 计算能往前查找的天数：
    // - 周日(0): 可往前查6天到周一(1)
    // - 周一(1): 不能往前查
    // - 周二(2): 可往前查1天到周一(1)  
    // - ...
    // - 周六(6): 可往前查5天到周一(1)
    int max_days_back;
    if (today == 0) {        // 周日
        max_days_back = 6;   // 可查到周一
    } else {                 // 周一到周六
        max_days_back = today - 1;  // 查到周一为止
    }

    for (int d = 1; d <= max_days_back; d++) {
        uint8_t day;
        if (today == 0) {
            // 今天是周日，往前数：周六(6)→周五(5)→...→周一(1)
            day = 7 - d;
        } else {
            // 今天是周一到周六，往前数
            day = today - d;
        }
        
        int max_minutes = -1;
        int max_index = -1;
        for (int i = 0; i < num; i++) {
            if (time_points[i].time_label.bit.index == target_index &&
                (time_points[i].time_label.bit.week & (1 << day))) {
                int point_minutes = get_minutes(time_points[i].time_active.bit.hour, time_points[i].time_active.bit.min);
                if (point_minutes > max_minutes) {
                    max_minutes = point_minutes;
                    max_index = i;
                }
            }
        }
        if (max_index != -1) {
            return max_index;
        }
    }
#endif

    // 3. 如果一周都没有，返回-1
    return -1;
}

/*------------------------------------------------------------------------------
 Function: Relay_Poweron_Init_By_TimeLable
 -----------------------------------------------------------------------------*/
/**
  * @brief      上电初始化配置，根据最近时间点
  * @param[in]  PowerRelayOnOff_U64_Struct *ctrl_time_set  
                MOD_STRUCT_reg19300 *Panel_Set             
                relay_ctrl_rtc_time_t time                 
                uint8_t time_wday                          
  * @param[out] None
  * @return     static void
  */
static void Relay_Poweron_Init_By_TimeLable(PowerRelayOnOff_U64_Struct *ctrl_time_set, MOD_STRUCT_reg19300 *Panel_Set, relay_ctrl_rtc_time_t time, uint8_t time_wday)
{
    uint8_t hour = time.hour;
    uint8_t min = time.min;
    uint8_t wday = time_wday;//0~6:周日~周六

    /*检查时间设置*/
    for ( uint8_t i = 0 ; i < RELAY_CTRL_REAL_NUM; i++ )
    {
        /*0-invalid; 1-顺序定时使能（多段时间使能），2-禁止和两类时间动作关联)*/
        if ( 1 == GET_POWER_RELAY_STATE(Panel_Set->GeneralPowerRelay_Set_Time_En.all, i))
        {  
            uint8_t label_index = (i * RELAY_TIME_CTRL_NODE_NUM);

            /*查找过去最近的时间点*/
            int idx = find_latest_past_time_index(&Panel_Set->PowerRelayTime_Set_General[label_index], RELAY_TIME_CTRL_NODE_NUM, 
                                                    hour, min, wday, (i+1));

            if (idx >= 0) {
                ESP_LOGW(RELAY_TAG, "Relay_Poweron_Init_By_TimeLable : relay%d(week: %d, hour: %d, min: %d)", i+1,
                          Panel_Set->PowerRelayTime_Set_General[label_index + idx].time_label.bit.week, 
                          Panel_Set->PowerRelayTime_Set_General[label_index + idx].time_active.bit.hour, 
                          Panel_Set->PowerRelayTime_Set_General[label_index + idx].time_active.bit.min);
                
                /*执行对应开关操作*/
                if ( 1 == Panel_Set->PowerRelayTime_Set_General[label_index + idx].time_label.bit.on_off ) {
                    SET_POWER_RELAY_STATE(ctrl_time_set->all, i, 1);
                    ESP_LOGI(RELAY_TAG, "Turning on relay index %d", i+1);
                } else if ( 0 == Panel_Set->PowerRelayTime_Set_General[label_index + idx].time_label.bit.on_off ) {
#if 0   // HJ: 郭经理确认，上电查询最近时间点时只执行开操作，不执行关操作                    
                    SET_POWER_RELAY_STATE(ctrl_time_set->all, i, 2);
                    ESP_LOGI(RELAY_TAG, "Turning off relay index %d", i+1);
#endif
                } else {
                    /*无效操作*/
                }
            } else {
                ESP_LOGE(RELAY_TAG, "Relay_Poweron_Init_By_TimeLable : relay%d found fail.", i+1);
            }
        }
    }   
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_By_TimeLable
 -----------------------------------------------------------------------------*/
/**
  * @brief      继电器时间控制
  * @param[in]  panel_set_struct *ctrl_time_set  
                MOD_STRUCT_reg19300 *Panel_Set   
  * @param[out] None
  * @return     static void
  */
static void Relay_Ctrl_By_TimeLable(PowerRelayOnOff_U64_Struct *ctrl_time_set, MOD_STRUCT_reg19300 *Panel_Set, relay_ctrl_rtc_time_t time, uint8_t time_wday)
{
    uint8_t hour = time.hour;
    uint8_t min = time.min;
    uint8_t wday = time_wday;//0~6:周日~周六

    /*检查时间设置*/
    for ( uint8_t i = 0 ; i < RELAY_CTRL_REAL_NUM; i++ )
    {
        /*0-invalid; 1-顺序定时使能（多段时间使能），2-禁止和两类时间动作关联)*/
        if ( 1 == GET_POWER_RELAY_STATE(Panel_Set->GeneralPowerRelay_Set_Time_En.all, i))
        {  
            for ( uint8_t j = 0 ; j < RELAY_TIME_CTRL_NODE_NUM ; j++ )
            {
                uint8_t label_index = (i * RELAY_TIME_CTRL_NODE_NUM) + j;
                /*bit6~0:周六~周日(六(bit6)，五，四，三，二，一，日(bit0))的自定义模式*/
                if ( 1 == ((Panel_Set->PowerRelayTime_Set_General[label_index].time_label.bit.week >> wday) & 1) )
                {
                    /*确认时间是否吻合*/
                    if ((hour == Panel_Set->PowerRelayTime_Set_General[label_index].time_active.bit.hour)
                        &&(min == Panel_Set->PowerRelayTime_Set_General[label_index].time_active.bit.min))
                    {
                        ESP_LOGI(RELAY_TAG, "Time match found for relay index %d", i+1);
                        
                        /*执行对应开关操作*/
                        if ( 1 == Panel_Set->PowerRelayTime_Set_General[label_index].time_label.bit.on_off ) {
                            SET_POWER_RELAY_STATE(ctrl_time_set->all, i, 1);
                            ESP_LOGI(RELAY_TAG, "Turning on relay index %d", i+1);
                        } else if ( 0 == Panel_Set->PowerRelayTime_Set_General[label_index].time_label.bit.on_off ) {
                            SET_POWER_RELAY_STATE(ctrl_time_set->all, i, 2);
                            ESP_LOGI(RELAY_TAG, "Turning off relay index %d", i+1);
                        } else {
                            /*无效操作*/
                        }
                    }
                }
            }
        }
    }   
}


/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Process_Time_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      Relay基于时间标签的控制任务
  * @param[in]  void 
  * @param[out] None
  * @return     void
  */
static void Relay_Ctrl_Process_Time_Task(void)
{
    PowerRelayOnOff_U64_Struct Set_Time_ctrl = {0};
    MOD_STRUCT_reg19300 *Panel_Set = &RelaySetData.Relay_info_t.PowerRelay_SmartSet03;
    static uint8_t old_time_min = 0;
    uint8_t ret = 0;
    
    relay_ctrl_rtc_time_t time = {
        .hour = System_Time_Hour,
        .min = System_Time_Min,
    };
    uint8_t time_wday = System_Time_Wday;//0~6:周日~周六

    /*时间控制参数未初始化*/
    if ( 0 == Relay_ctrl.bit.time_ctrl_init ) {
        ESP_LOGE(RELAY_TAG, "Time control parameters not initialized.");
        return;
    }

    /*时间未初始化*/
    if ( 0 == System_Time_Init_Flag ) {
        return;
    }

    /*当前仅在分钟变动时执行，避免单个时间点频繁设置*/
    if ((old_time_min != time.min)
        || (System_Time_Ctrl_First_Set == 0)
        || Tou_Config_Set_Flag )
    {
        old_time_min = time.min;
        if (System_Time_Ctrl_First_Set == 0) {
            /*继电器时间控制(上电首次)*/
            Relay_Poweron_Init_By_TimeLable(&Set_Time_ctrl, Panel_Set, time, time_wday);
            System_Time_Ctrl_First_Set = 1;
        } else {
            /*继电器时间控制*/
            Relay_Ctrl_By_TimeLable(&Set_Time_ctrl, Panel_Set, time, time_wday);
        }
        /*更新到全局变量*/
        if (PowerRelay_Set_By_Time.all != Set_Time_ctrl.all) {
            PowerRelay_Set_By_Time.all = Set_Time_ctrl.all;
            ret = 1;
            ESP_LOGI(RELAY_TAG, "Updating time control settings.");
        }
    }
    /*存在新的时间设置*/
    if(ret == 1) Relay_ctrl.bit.time_ctrl_target = 1;
}


/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_By_Delay
 -----------------------------------------------------------------------------*/
/**
  * @brief      继电器延时控制
  * @param[in]  panel_set_struct *ctrl_time_set  
                MOD_STRUCT_reg19100 *Panel_Set   
  * @param[out] None
  * @return     static void
  */
static void Relay_Ctrl_By_Delay(PowerRelayOnOff_U64_Struct *ctrl_delay_set, MOD_STRUCT_reg19100 *Panel_Set, uint16_t delay_scnt_min)
{    
    /*检查延时设置*/
    for ( uint8_t i = 0 ; i < RELAY_CTRL_REAL_NUM; i++ )
    {
        /*0-invalid; 1-延时使能，2-禁止和两类时间动作关联)*/
        if ( 1 == GET_POWER_RELAY_STATE(Panel_Set->GeneralPowerRelay_Delay_Time_En.all, i))
        {  
            /*计算剩余时间*/
            uint16_t remaining_time_min = convert_to_minutes(Panel_Set->PowerRelayDelay_Set_General[i].all);

            if ( remaining_time_min > 0 )
            {
                remaining_time_min = (remaining_time_min > delay_scnt_min) ? (remaining_time_min - delay_scnt_min) : 0;
                Panel_Set->PowerRelayDelay_Set_General[i].all = convert_from_minutes(remaining_time_min);

                /*触发延时操作*/
                if ( remaining_time_min == 0 )
                {
                    ESP_LOGI(RELAY_TAG, "Delay ctrl found for relay index %d", i+1);

                    /*执行对应开关操作*/
                    if ( 1 == GET_POWER_RELAY_STATE(Panel_Set->GeneralPowerRelayDelay_Action.all, i))
                    {
                        SET_POWER_RELAY_STATE(ctrl_delay_set->all, i, 1);
                        ESP_LOGI(RELAY_TAG, "Turning on relay index %d", i+1);
                    }
                    else if ( 2 == GET_POWER_RELAY_STATE(Panel_Set->GeneralPowerRelayDelay_Action.all, i))
                    {
                        SET_POWER_RELAY_STATE(ctrl_delay_set->all, i, 2);
                        ESP_LOGI(RELAY_TAG, "Turning off relay index %d", i+1);
                    }
                    else
                    {
                        ESP_LOGE(RELAY_TAG, "Invalid ctrl relay index %d", i+1);
                    }

                    /*延时设置为一次性设置，执行完清除*/
                    SET_POWER_RELAY_STATE(Panel_Set->GeneralPowerRelayDelay_Action.all, i, 0);
                    Relay_File_W_Flag.sBit.relay_data_delay_ctrl = 1;
                }
            }
        }
    }   
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Delay_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      panel基于延时时间的控制任务(1s)
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void Relay_Ctrl_Delay_Task(void)
{
    PowerRelayOnOff_U64_Struct Set_Delay_ctrl = {0};
    MOD_STRUCT_reg19100 *Panel_Set = &RelaySetData.Relay_info_t.PowerRelay_SmartSet02;
    static uint32_t pre_time_min = 0;
    uint8_t ret = 0;

    /*延时控制参数未初始化*/
    if ( 0 == Relay_ctrl.bit.delay_ctrl_init ) {
        ESP_LOGE(RELAY_TAG, "Delay control parameters not initialized.");
        return;
    }

    /*计算时间差（基于系统运行时间）*/
    uint32_t now_time_min = get_cached_uptime_minutes();
    uint16_t delay_scnt_min = ((pre_time_min != 0)&&(now_time_min >= pre_time_min)) ? (now_time_min - pre_time_min) : 0;
    pre_time_min = now_time_min;

    /*当前仅在时间差变动时执行，避免无效轮询*/
    if ( delay_scnt_min > 0 )
    {
        /*继电器延时控制*/
        Relay_Ctrl_By_Delay(&Set_Delay_ctrl, Panel_Set, delay_scnt_min);
    }
        
    /*更新到全局变量*/
    if (PowerRelay_Set_By_Delay.all != Set_Delay_ctrl.all) {
        PowerRelay_Set_By_Delay.all = Set_Delay_ctrl.all;
        ret = 1;
        ESP_LOGI(RELAY_TAG, "Updating delay control settings.");
    }
    
    /*存在新的延时设置*/
    if(ret == 1) Relay_ctrl.bit.delay_ctrl_target = 1;
}


/*------------------------------------------------------------------------------
 Function: get_current_timestamp
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取当前UTC时间戳
  * @param[in]  None
  * @param[out] None
  * @return     static uint32_t
  */
static uint32_t get_current_timestamp(void) {
    time_t local_time = time(NULL);  // 获取本地时间戳
    
    return (uint32_t)local_time;
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_By_Plan
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查预约备电状态
  * @param[in]  MOD_STRUCT_reg19200 *Panel_Set   
  * @param[out] None
  * @return     static void
  */
static void Relay_Ctrl_Check_Plan_Status(MOD_STRUCT_reg19200 *Panel_Set) {    
    #if CONFIG_ENABLE_PLAN_BACKUP
    // 静态变量声明
    static uint8_t last_backup_active = 0xFF; // 初始值设为无效值，确保第一次能检测到变化
    static uint16_t saved_work_mode = 0; // 保存进入备电模式前的工作模式
    
    uint8_t backup_active = 0;
    uint8_t backup_status_changed = 0;
    
    // 检查预约备电总使能
    if (Panel_Set->enable != 1) {  // 未使能时直接返回
        ESP_LOGD(RELAY_TAG, "Plan backup power not enabled, resetting status");
        
        // 恢复静态变量的默认值
        last_backup_active = 0xFF;  // 重置为初始值
        saved_work_mode = 0;        // 重置保存的工作模式
        
        // 更新预约备电激活标志为未激活
        Plan_Backup_Power_Active = 0;
        return;
    }
    
    // 使能状态下，检查时间段设置
    uint32_t current_time = get_current_timestamp();
    
    // 遍历8个时间段设置
    for (uint8_t i = 0; i < 8; i++) {
        time_stamp_set_t *time_slot = &Panel_Set->timestamp1[i];
        
        // 检查时间段是否有效配置
        if (time_slot->start_time != 0 && time_slot->end_time != 0 && 
            time_slot->start_time < time_slot->end_time) {
            
            // 检查当前时间是否在该时间段内
            if (current_time >= time_slot->start_time && current_time <= time_slot->end_time) {
                // 获取对应时间段的充放电状态 (每个status占用4位)
                uint8_t status = 0;
                if (i < 4) {
                    // 时间段0~3使用Chg_Plan_Time_En1
                    status = (Panel_Set->Chg_Plan_Time_En1.all >> (i * 4)) & 0x0F;
                } else {
                    // 时间段4~7使用Chg_Plan_Time_En2
                    status = (Panel_Set->Chg_Plan_Time_En2.all >> ((i - 4) * 4)) & 0x0F;
                }
                
                // 仅处理充电状态，放电暂不处理
                if (status == 1) {  // 1-充电
                    backup_active = 1;
                    ESP_LOGD(RELAY_TAG, "Plan backup power active for time slot %d", i+1);
                    break;  // 找到活动时间段后退出循环
                }
            }
        }
    }
    
    // 检查备电状态是否发生变化
    if (last_backup_active != backup_active) {
        backup_status_changed = 1;
        ESP_LOGI(RELAY_TAG, "Plan backup power status changed: %d -> %d", last_backup_active, backup_active);
        last_backup_active = backup_active;
    }
    
    // 更新预约备电激活标志
    Plan_Backup_Power_Active = backup_active;
    
    // 当备电状态发生变化时，下发标准UPS模式
    if (backup_status_changed) {
        if (backup_active == 1) {
            // 进入备电模式前，保存当前工作模式
            saved_work_mode = Get_Current_Work_Mode();
            ESP_LOGI(RELAY_TAG, "Entering backup power mode, saving current work_mode: %d", saved_work_mode);
            
            // 下发标准UPS模式
            ESP_LOGI(RELAY_TAG, "Setting standard UPS mode");
            Single_Reg_Set_iot_to_arm(2005, 4); // 4: 标准UPS模式
        } else {
            // 退出备电模式，智能恢复工作模式
            uint16_t current_work_mode = Get_Current_Work_Mode();
            ESP_LOGI(RELAY_TAG, "Exiting backup power mode, current work_mode: %d, saved work_mode: %d", 
                     current_work_mode, saved_work_mode);
            
            // 如果当前模式是标准UPS模式(4)，且有保存的模式，则恢复到进入备电前的模式
            if (current_work_mode == 4 && saved_work_mode != 0 && saved_work_mode != 4) {
                ESP_LOGI(RELAY_TAG, "Restoring work mode from %d to %d", current_work_mode, saved_work_mode);
                Single_Reg_Set_iot_to_arm(2005, saved_work_mode);
            } else {
                // ESP_LOGI(RELAY_TAG, "No need to restore work mode (current: %d, saved: %d)", 
                //          current_work_mode, saved_work_mode);
            }
        }
    }
    #endif
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Process_Plan_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      预约备电状态检查任务
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
#if CONFIG_ENABLE_PLAN_BACKUP
static void Relay_Ctrl_Process_Plan_Task(void)
{
    MOD_STRUCT_reg19200 *Panel_Set = &RelaySetData.Relay_info_t.Backup_power_set;
    
    /*预约备电控制参数未初始化*/
    if (0 == Relay_ctrl.bit.plan_ctrl_init) {
        ESP_LOGE(RELAY_TAG, "Plan control parameters not initialized.");
        return;
    }

    /*时间未初始化*/
    if (0 == System_Time_Init_Flag) {
        return;
    }

    /*检查预约备电状态*/
    Relay_Ctrl_Check_Plan_Status(Panel_Set);
}
#endif

/*------------------------------------------------------------------------------
 Function: Get_Plan_Backup_Power_Status
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取预约备电激活状态
  * @param[in]  None
  * @param[out] None
  * @return     uint8_t - 0:未激活, 1:激活
  */
uint8_t Get_Plan_Backup_Power_Status(void)
{
    return Plan_Backup_Power_Active;
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      继电器设置下发
  * @param[in]  PowerRelayOnOff_U64_Struct *New_Relay_On_Off_Set  
  * @param[out] None
  * @return     static void
  */
static void Relay_Ctrl_Task(PowerRelayOnOff_U64_Struct *New_Relay_On_Off_Set)
{
    /*检查设置来源是否无效*/
    if(New_Relay_On_Off_Set == NULL)
    {
        return;
    }

    for ( uint8_t i = 0 ; i < RELAY_CTRL_REAL_NUM; i++ )
    {
        uint16_t PowerRelay_Set = GET_POWER_RELAY_STATE(New_Relay_On_Off_Set->all, i);

        /*判断是否存在最新设置*/
        if ( 0 != PowerRelay_Set && 3 != PowerRelay_Set)
        {
            /*执行对应有效设置*/
            uint8_t index = i + 1;
            ESP_LOGI(RELAY_TAG, "New relay setting detected for relay %d, set %d", index, PowerRelay_Set);
            uint16_t set_value = (PowerRelay_Set << (i * 2));
            switch ( index )
            {
                case RELAY_INDEX_AC1:
                case RELAY_INDEX_AC2:
                case RELAY_INDEX_AC3:
                case RELAY_INDEX_AC4:
                    if(set_value != 0xFF) {
                        CanSetTarget(0, 0, INV_TYPE_CONFIG00_1AH, offsetof(inv_set00_struct, set_AC_branch), (uint8_t*)&set_value, 2, 0xFF); // 广播
                    }
                break;

                default:
                    ESP_LOGE(RELAY_TAG, "Relay %d invalid!", index);
                    break;
            }
        }
    }
    
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Factory_Parameter01_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      出厂参数更新
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Relay_Ctrl_Factory_Parameter01_Update(void)
{
    /*继电器控制区1*/
    MOD_STRUCT_reg19000 *Panel_Set = &RelaySetData.Relay_info_t.PowerRelay_SmartSet01;
    /*写缓冲也必须清，否则后续单路写19000时 Sync 会把脏 Enable/阈值写回生效区*/
    MOD_STRUCT_reg19000 *Wr_Set = &Inv_WR.mod_reg19000_Relay_ctrl;

    /*默认SOC控制功能禁止*/
    for ( uint8_t i = 0 ; i < RELAY_CTRL_REAL_NUM ; i++ ) {
        Panel_Set->GeneralPowerRelaySoc[i].bit.CmdOffSoc = 0x7F;
        Panel_Set->GeneralPowerRelaySoc[i].bit.CmdOnSoc = 0x7F;
        Panel_Set->GeneralPowerRelaySoc[i].bit.CmdOffEnable = 0;
        Panel_Set->GeneralPowerRelaySoc[i].bit.CmdOnEnable = 0;

        Wr_Set->GeneralPowerRelaySoc[i].bit.CmdOffSoc = 0x7F;
        Wr_Set->GeneralPowerRelaySoc[i].bit.CmdOnSoc = 0x7F;
        Wr_Set->GeneralPowerRelaySoc[i].bit.CmdOffEnable = 0;
        Wr_Set->GeneralPowerRelaySoc[i].bit.CmdOnEnable = 0;
    }
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Factory_Parameter02_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      出厂参数更新
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Relay_Ctrl_Factory_Parameter02_Update(void)
{
    /*继电器控制区2*/
    MOD_STRUCT_reg19100 *Panel_Set = &RelaySetData.Relay_info_t.PowerRelay_SmartSet02;

    /*默认延时功能使能*/
    for ( uint8_t i = 0 ; i < RELAY_CTRL_REAL_NUM ; i++ ) {
        SET_POWER_RELAY_STATE(Panel_Set->GeneralPowerRelay_Delay_Time_En.all, i, 1);
    }
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Factory_Parameter03_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      出厂参数更新（避免APP设置覆盖）
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Relay_Ctrl_Factory_Parameter03_Update(void)
{
    uint16_t time_label_index = 0;

    /*继电器控制区3*/
    MOD_STRUCT_reg19300 *Panel_Set = &RelaySetData.Relay_info_t.PowerRelay_SmartSet03;
    /*固定可用时间点数量，不可更改*/
    Panel_Set->PowerRelay_Time_Set_Cnt = (RELAY_CTRL_REAL_NUM * RELAY_TIME_CTRL_NODE_NUM);

    /*固定时间点标签，不可更改*/
    for ( uint8_t i = 0 ; i < RELAY_TIME_CTRL_NODE_NUM ; i++ ) {
        Panel_Set->PowerRelayTime_Set_General[time_label_index++].time_label.bit.index = RELAY_INDEX_AC1;
    }
    
    /*固定时间点标签，不可更改*/
    for ( uint8_t i = 0 ; i < RELAY_TIME_CTRL_NODE_NUM ; i++ ) {
        Panel_Set->PowerRelayTime_Set_General[time_label_index++].time_label.bit.index = RELAY_INDEX_AC2;
    }

    /*固定时间点标签，不可更改*/
    for ( uint8_t i = 0 ; i < RELAY_TIME_CTRL_NODE_NUM ; i++ ) {
        Panel_Set->PowerRelayTime_Set_General[time_label_index++].time_label.bit.index = RELAY_INDEX_AC3;
    }

    /*固定时间点标签，不可更改*/
    for ( uint8_t i = 0 ; i < RELAY_TIME_CTRL_NODE_NUM ; i++ ) {
        Panel_Set->PowerRelayTime_Set_General[time_label_index++].time_label.bit.index = RELAY_INDEX_AC4;
    }
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Factory_Parameter04_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      出厂参数更新 - 预约备电区
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */

void Relay_Ctrl_Factory_Parameter04_Update(void)
{
    #if CONFIG_ENABLE_PLAN_BACKUP
    /*预约备电区*/
    MOD_STRUCT_reg19200 *Panel_Set = &RelaySetData.Relay_info_t.Backup_power_set;

    /*默认预约备电功能关闭*/
    Panel_Set->enable = 1;  // 改为1默认开启

    /*默认所有时间段状态为无效*/
    Panel_Set->Chg_Plan_Time_En1.all = 0;
    Panel_Set->Chg_Plan_Time_En2.all = 0;

    /*清空所有时间戳设置*/
    for (uint8_t i = 0; i < 8; i++) {
        Panel_Set->timestamp1[i].start_time = 0;
        Panel_Set->timestamp1[i].end_time = 0;
    }
    
    ESP_LOGI(RELAY_TAG, "Plan control factory parameters initialized.");
    #endif
}



/*------------------------------------------------------------------------------
 Function: RelaySetData_Get_From_Modbus_19000
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus19000段设置同步
  * @param[in]  uint16_t write_offset  
                uint16_t data_len      
  * @param[out] None
  * @return     static void
  */
static void RelaySetData_Get_From_Modbus_19000(MOD_STRUCT_reg19000 *new_settings, uint16_t write_offset, uint16_t data_len)
{
    uint8_t update = 0;
	uint32_t address_begin = (uint32_t)new_settings + write_offset;
	uint32_t address_end = (uint32_t)new_settings + write_offset + data_len;
	MOD_STRUCT_reg19000 *current_settings = &RelaySetData.Relay_info_t.PowerRelay_SmartSet01;

    /*地址或长度有误*/
    if ( address_begin >= address_end ) return;

    /*19000~*/
    if(Long_Var_Update_Check(address_begin, address_end, 
                              (uint32_t)&new_settings->GeneralPowerRelaySoc, 
                              sizeof(new_settings->GeneralPowerRelaySoc)))
    {
        SyncPowerRelaySocArray(current_settings->GeneralPowerRelaySoc,
                               new_settings->GeneralPowerRelaySoc,
                               RELAY_CTRL_REAL_NUM);
        update |= 1;
    }

    if(update == 1) Relay_File_W_Flag.sBit.relay_data_soc_ctrl = 1;
}

/*------------------------------------------------------------------------------
 Function: RelaySetData_Get_From_Modbus_19100
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus19100段设置同步
  * @param[in]  uint16_t write_offset  
                uint16_t data_len      
  * @param[out] None
  * @return     static void
  */
static void RelaySetData_Get_From_Modbus_19100(MOD_STRUCT_reg19100 *new_settings, uint16_t write_offset, uint16_t data_len)
{
    uint8_t update = 0;
	uint32_t address_begin = (uint32_t)new_settings + write_offset;
	uint32_t address_end = (uint32_t)new_settings + write_offset + data_len;
	MOD_STRUCT_reg19100 *current_settings = &RelaySetData.Relay_info_t.PowerRelay_SmartSet02;

    /*地址或长度有误*/
    if ( address_begin >= address_end ) return;

    /*19100~19103*/
    if(Long_Var_Update_Check(address_begin, address_end, 
                              (uint32_t)&new_settings->GeneralPowerRelay_Delay_Time_En, 
                              sizeof(new_settings->GeneralPowerRelay_Delay_Time_En)))
    {
        SyncPowerRelay_U64(&current_settings->GeneralPowerRelay_Delay_Time_En.all, &new_settings->GeneralPowerRelay_Delay_Time_En.all, RELAY_CTRL_REAL_NUM);
        update |= 1;
    }
        
    /*19104~19107*/
    if(Long_Var_Update_Check(address_begin, address_end, 
                              (uint32_t)&new_settings->GeneralPowerRelayDelay_Action, 
                              sizeof(new_settings->GeneralPowerRelayDelay_Action)))
    {
        SyncPowerRelay_U64(&current_settings->GeneralPowerRelayDelay_Action.all, &new_settings->GeneralPowerRelayDelay_Action.all, RELAY_CTRL_REAL_NUM);
        update |= 1;
    }

    /*19108~19113*/
    update |= Group_Update_settings(address_begin, address_end, 
                                    (uint32_t)&new_settings->PowerRelayDelay_Set_General, 
                                    sizeof(new_settings->PowerRelayDelay_Set_General),
                                    (uint8_t *)&current_settings->PowerRelayDelay_Set_General,
                                    (uint8_t *)&new_settings->PowerRelayDelay_Set_General);

    /*19114~19117*/
    update |= Group_Update_settings(address_begin, address_end, 
                                    (uint32_t)&new_settings->PowerRelayDelay_Set_General, 
                                    sizeof(new_settings->PowerRelayDelay_Set_General),
                                    (uint8_t *)&current_settings->PowerRelayDelay_Set_General,
                                    (uint8_t *)&new_settings->PowerRelayDelay_Set_General);

    if(update == 1) Relay_File_W_Flag.sBit.relay_data_delay_ctrl = 1;
}

/*------------------------------------------------------------------------------
 Function: RelaySetData_Get_From_Modbus_19300
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus19300段设置同步
  * @param[in]  uint16_t write_offset  
                uint16_t data_len      
  * @param[out] None
  * @return     static void
  */
static void RelaySetData_Get_From_Modbus_19300(MOD_STRUCT_reg19300 *new_settings, uint16_t write_offset, uint16_t data_len)
{
    uint8_t update = 0;
	uint32_t address_begin = (uint32_t)new_settings + write_offset;
	uint32_t address_end = (uint32_t)new_settings + write_offset + data_len;
	MOD_STRUCT_reg19300 *current_settings = &RelaySetData.Relay_info_t.PowerRelay_SmartSet03;

    /*地址或长度有误*/
    if ( address_begin >= address_end ) return;

    /*19300~19303*/
    if(Long_Var_Update_Check(address_begin, address_end, 
                              (uint32_t)&new_settings->GeneralPowerRelay_Set_Time_En, 
                              sizeof(new_settings->GeneralPowerRelay_Set_Time_En)))
    {
        SyncPowerRelay_U64(&current_settings->GeneralPowerRelay_Set_Time_En.all, &new_settings->GeneralPowerRelay_Set_Time_En.all, RELAY_CTRL_REAL_NUM);
        update |= 1;
    }

    /*19305~*/
    update |= Group_Update_settings(address_begin, address_end, 
                                    (uint32_t)&new_settings->PowerRelayTime_Set_General, 
                                    sizeof(new_settings->PowerRelayTime_Set_General),
                                    (uint8_t *)&current_settings->PowerRelayTime_Set_General,
                                    (uint8_t *)&new_settings->PowerRelayTime_Set_General);

    if(update == 1) Relay_File_W_Flag.sBit.relay_data_time_ctrl = 1;
}

/*------------------------------------------------------------------------------
 Function: RelaySetData_Get_From_Modbus_19200
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus19200段设置同步
  * @param[in]  MOD_STRUCT_reg19200 *new_settings
                uint16_t write_offset  
                uint16_t data_len      
  * @param[out] None
  * @return     static void
  */
#if CONFIG_ENABLE_PLAN_BACKUP
static void RelaySetData_Get_From_Modbus_19200(MOD_STRUCT_reg19200 *new_settings, uint16_t write_offset, uint16_t data_len)
{
    uint8_t update = 0;
    uint32_t address_begin = (uint32_t)new_settings + write_offset;
    uint32_t address_end = (uint32_t)new_settings + write_offset + data_len;
    MOD_STRUCT_reg19200 *current_settings = &RelaySetData.Relay_info_t.Backup_power_set;

    /*地址或长度有误*/
    if (address_begin >= address_end) return;

    /*19200 - enable字段*/
    if(Long_Var_Update_Check(address_begin, address_end, 
                              (uint32_t)&new_settings->enable, 
                              sizeof(new_settings->enable)))
    {
        if (new_settings->enable == 1 || new_settings->enable == 2) {
            current_settings->enable = new_settings->enable;
            update |= 1;
            ESP_LOGI(RELAY_TAG, "Plan control enable updated: %d", new_settings->enable);
        }
    }

    /*Chg_Plan_Time_En1字段*/
    if(Long_Var_Update_Check(address_begin, address_end, 
                              (uint32_t)&new_settings->Chg_Plan_Time_En1, 
                              sizeof(new_settings->Chg_Plan_Time_En1)))
    {
        // 位成员不为0则赋值，为0则保留原值 - 直接调用现有函数
        SyncPowerRelay_U16(&current_settings->Chg_Plan_Time_En1.all, &new_settings->Chg_Plan_Time_En1.all, 8);
        update |= 1;
        ESP_LOGI(RELAY_TAG, "Plan control time enable updated: 0x%04X", new_settings->Chg_Plan_Time_En1.all);
    }

    /*19201-19217 - timestamp1数组*/
    update |= Group_Update_settings(address_begin, address_end, 
                                    (uint32_t)&new_settings->timestamp1, 
                                    sizeof(new_settings->timestamp1),
                                    (uint8_t *)&current_settings->timestamp1,
                                    (uint8_t *)&new_settings->timestamp1);

    if(update == 1) {
        Relay_File_W_Flag.sBit.relay_data_plan_ctrl = 1;
        ESP_LOGI(RELAY_TAG, "Plan control settings updated from Modbus.");
    }
}
#endif

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Setdata_Get_From_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus设置同步
  * @param[in]  uint8_t type           
                uint16_t write_offset  
                uint16_t data_len      
  * @param[out] None
  * @return     void
  */
void Relay_Ctrl_Setdata_Get_From_Modbus(uint32_t address_value, uint8_t type, uint16_t write_offset, uint16_t data_len)
{
    switch ( type )
    {
        case POWERRELAY_SMART_SET01 :
            RelaySetData_Get_From_Modbus_19000((MOD_STRUCT_reg19000 *)address_value, write_offset, data_len);
            break;
        case POWERRELAY_SMART_SET02 :
            RelaySetData_Get_From_Modbus_19100((MOD_STRUCT_reg19100 *)address_value, write_offset, data_len);
            break;
        case POWERRELAY_SMART_SET03 :
            RelaySetData_Get_From_Modbus_19300((MOD_STRUCT_reg19300 *)address_value, write_offset, data_len);
            break;
#if CONFIG_ENABLE_PLAN_BACKUP
        case POWERRELAY_SMART_SET04 :
            RelaySetData_Get_From_Modbus_19200((MOD_STRUCT_reg19200 *)address_value, write_offset, data_len);
            break;
#endif
        default:
            break;
    }

    // 更新参数修改标志
    Tou_Config_Set_Flag = 1;

    return;
}

/*------------------------------------------------------------------------------
 Function: Relay_Poweron_Init_Flag_Set
 -----------------------------------------------------------------------------*/
/**
* @brief      上电初始化配置标志设置
* @param[in]  uint8_t time_ctrl_flag  
* @param[out] None
* @return     void
*/
void Relay_Poweron_Init_Flag_Set(uint8_t time_ctrl_flag) {
    System_Time_Ctrl_First_Set = time_ctrl_flag;
}

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Parameter_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      Panel工作参数更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
void Relay_Ctrl_Parameter_update(uint8_t time_init_flag, uint8_t soc)
{
    /*时间初始化标志*/
    System_Time_Init_Flag = time_init_flag;

    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    /*小时*/
    System_Time_Hour = tm_info->tm_hour;

    /*分钟*/
    System_Time_Min = tm_info->tm_min;

    /*星期*/
    System_Time_Wday = tm_info->tm_wday;//0~6:周日~周六

    /*电量*/
    System_Soc = soc;   // 当前系统soc
}

#define PANEL_CTRL_TASK_PERIOD  1000   //PANEL控制任务周期

/*------------------------------------------------------------------------------
 Function: Relay_Ctrl_Main_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      继电器控制任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Relay_Ctrl_Main_Task(void)
{
	static uint32_t pre_time = 0;
    
    PowerRelayOnOff_U64_Struct *New_Relay_On_Off_Set = NULL;

    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if((now_time - pre_time) >= PANEL_CTRL_TASK_PERIOD)
	{
	    pre_time = now_time;

        // 更新开始标志
        uint8_t set_start_flag = Tou_Config_Set_Flag;

        /*基于soc的控制任务*/
        Relay_Ctrl_Process_Soc_Task();
#if 0
        /*基于延时时间的控制任务*/
        Relay_Ctrl_Delay_Task();

        /*panel基于时间标签的控制任务*/
        Relay_Ctrl_Process_Time_Task();
#endif
        
        /*基于预约备电的控制任务*/
#if CONFIG_ENABLE_PLAN_BACKUP
        Relay_Ctrl_Process_Plan_Task();
#endif

        // 更新参数修改标志
        if(set_start_flag == Tou_Config_Set_Flag) Tou_Config_Set_Flag = 0;
    }

    /*更新设置来源 - 按优先级处理*/
    if ( 1 == Relay_ctrl.bit.delay_ctrl_target )
    {
        ESP_LOGI(RELAY_TAG, "Processing delay control target.");
        New_Relay_On_Off_Set = &PowerRelay_Set_By_Delay;
        Relay_ctrl.bit.delay_ctrl_target = 0;
    }
    else if ( 1 == Relay_ctrl.bit.time_ctrl_target )
    {
        ESP_LOGI(RELAY_TAG, "Processing time control target.");
        New_Relay_On_Off_Set = &PowerRelay_Set_By_Time;
        Relay_ctrl.bit.time_ctrl_target = 0;
    }
    else if ( 1 == Relay_ctrl.bit.soc_ctrl_target )
    {
        ESP_LOGI(RELAY_TAG, "Processing soc control target.");
        New_Relay_On_Off_Set = &PowerRelay_Set_By_Soc;
        Relay_ctrl.bit.soc_ctrl_target = 0;
    }

    /*继电器控制逻辑*/
    Relay_Ctrl_Task(New_Relay_On_Off_Set);
    
    /*清除本次操作记录*/
    if(New_Relay_On_Off_Set != NULL) memset(New_Relay_On_Off_Set, 0, sizeof(PowerRelayOnOff_U64_Struct));
}


