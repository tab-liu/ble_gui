/**
  ******************************************************************************
  * @file      bms_can.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/1/6
  * @brief     48V BMS-CAN协议处理（移植钟工）
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/6   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */

#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "driver/twai.h"
#include "driver/gpio.h"

#include "esp_system.h"
#include "esp_log.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "utils.h"
#include "bms_ota.h"
#include "bms_can.h"
#include "can_protocol.h"
#include "can_transmit.h"
#include "can_type.h"
#include "http_client.h"
#include "modbus_data.h"
#include "comm_define.h"
#include "dev_access_log.h"
#include "parameter.h"
#include "aiot_log.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[ALPHA_CAN]"

// 电池包类型定义
#define BAT_TYPE_B300       0
#define BAT_TYPE_B300S      1
#define BAT_TYPE_B300K      2
#define BAT_TYPE_B230       3
#define BAT_TYPE_B210       4
#define BAT_TYPE_UNKNOWN    5
#define BAT_TYPE_MAX        6

// BOOT区 最多支持3种电池类型
#define BMS_BOOT_MAX_TYPES  3

// 版本信息区最大长度
#define MAX_MIN_VER_GROUP   10

#if 0   // 兼容CAN_BETA协议，暂不过滤
#define TWAI_FILTER_CONFIG_ACCEPT_TARGET() {.acceptance_code = CAN_BMS_ID, .acceptance_mask = CAN_ID_MASK, .single_filter = true}
#endif

/* ============================== 文件内本地结构体定义 ================================ */

#pragma pack(1)

// 电池包类型统计结构
typedef struct {
    char type_name[16];    // 类型名称
    uint8_t count;         // 电池包数量
    uint8_t version_count; // 版本数量
    uint32_t versions[8];  // 最多记录8个不同版本号
    uint8_t addr_list[8];  // 记录该类型所有电池包的地址，最多8个
} bat_type_stat_t;

// 电池包最小版本检查
typedef struct {
    uint16_t type;
    uint32_t version_prefix; // 软件料号不含后两位或后四位
    uint32_t min_version;    // 最小完整version
    uint8_t valid;
} min_version_t;

// 电池包地址分配
typedef struct {
    uint8_t addr_pool[MAX_BMS_NUM];
    uint8_t bat_num;
} addr_trace_t;

// BMS Boot版本信息结构体
typedef struct {
    char type[16];         // 设备类型 (如"B300", "B300S", "B300K")
    uint32_t version[6];   // 存储最多6个不同版本号
    uint8_t number;        // 记录版本个数
} bms_boot_version_t;

#pragma pack()

/* =============================== 文件内全局变量 ================================ */

// 电池包信息
static USE_EXT_RAM_BSS bat_data_t bat_data = {0};

// 电池包类型统计数组
static USE_EXT_RAM_BSS bat_type_stat_t bat_type_stats[BAT_TYPE_MAX] = {
     {"B300",  0, 0, {0}, {0}},
     {"B300S", 0, 0, {0}, {0}},
     {"B300K", 0, 0, {0}, {0}},
     {"B230",  0, 0, {0}, {0}},
     {"B210",  0, 0, {0}, {0}},
     {"Unknown",0, 0, {0}, {0}}
};

// 电池包地址分配
static USE_EXT_RAM_BSS addr_trace_t bat_addr_trace = {0};

// b300 b300s boot超时
static int b300_boot_timeout = 0;

// B230低电量需要停机避免亏电
static uint8_t bat_stop_cmd = 0; 

// 超时接收异常
static uint16_t recv_fail_count = 0;

//收到B230 boot CAN报文计数器
static uint8_t check_b230_count = 0;

// bms_boot数据缓存
static USE_EXT_RAM_BSS bms_boot_version_t bms_boot_versions[BMS_BOOT_MAX_TYPES] = {0};
static uint8_t bms_boot_type_count = 0;

// bms 电池维护状态
static aging_status_struct1 bms_aging_status = {0};

/* =============================== 变量及函数声明 ================================ */

static void bms_fault_update(void);
static void bms_timeout_check(uint16_t interval);

// B230部分特殊函数
static int check_b230_boot_ack(const twai_message_t *rx_frame);
static void bms_check_b230_boot_cmd(void);
static void update_b230_boot_data(void);
static void update_b230_data(void);
static void check_low_soc(uint32_t interval);

// 电池包系列识别函数
static void update_bms_type_from_version(uint8_t index, const char* new_type);
static bool is_b300_series(uint16_t version_type);
static bool is_b300s_series(uint16_t version_type);
static bool is_b300k_series(uint16_t version_type);
static bool is_b230_series(uint16_t version_type);
static bool is_b210_series(uint16_t version_type);
static bool is_power5_series(uint16_t version_type);

/* ================================ 模块函数定义 ================================ */

/**
 * @brief 处理B300启动超时计时逻辑
 *
 * @param[in] interval 本次递减的时间间隔（单位：与b300_boot_timeout一致）
 */
static void b300_boot_time_handle(int interval) {
    if(b300_boot_timeout > interval){
        b300_boot_timeout -= interval;
    } else {
        b300_boot_timeout = 0;
        bat_data.state.bit.is_b300_boot = 0;
    }
}

/**
 * @brief 1秒周期的BMS CAN发送任务
 *
 * 主要功能：
 * - 检查升级状态，超时处理
 * - OTA计时处理
 * - SOC低电量检测
 * - B300 BOOT超时状态更新
 * - B230接收超时处理
 * - 地址分配约束（预留）
 */
void bms_can_send_task_1s (void)
{	
#ifdef CONFIG_SYSTEM_SLEEP_CAN_SILENT_ENABLE
    /*休眠期间不检查*/
    if(1 == reals.IOT_Status_Flag.sBit.system_sleep_flag) return;
#endif

	// 系统升级过程中不更新
 	if(!sys_is_updating() 
        && (can_ota_status.ready_timeout == 0)) { 
        // BMS数据超时处理
        bms_timeout_check(1000);
    }
    // OTA相关CAN接收超时计时处理
    bms_ota_time_handle(1000);

    // 检查电池低SOC停机逻辑
    check_low_soc(1000);

    // B300 BOOT超时状态更新
	b300_boot_time_handle(1000);

    // B230超时判断
    if(!bms_ota_is_doing())
	{  
        if(recv_fail_count > 60){ // 超时没有接收到数据 
            check_b230_count = 1;
            recv_fail_count = 0;

            // B230 boot指令
	        // bms_check_b230_boot_cmd(); 
        }
    }
    
    // BMS地址分配约束（PCS->BMS）
//    bms_ann_addr_ctrl(); 
}

/*------------------------------------------------------------------------------
 Function: update_b230_data
 -----------------------------------------------------------------------------*/
/**
  * @brief      B230单包模拟主机数据
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void update_b230_data(void){
    bat_data.all_data.chg_max_volt = bat_data.bms_data[0].chg_max_volt;
    bat_data.all_data.chg_max_curr = bat_data.bms_data[0].chg_max_curr;
    bat_data.all_data.dsg_max_curr = bat_data.bms_data[0].dsg_max_curr;
    bat_data.all_data.chg_ctrl.all = bat_data.bms_data[0].chg_ctrl.all;
    bat_data.all_data.total_volt = bat_data.bms_data[0].total_voltage;
    bat_data.all_data.total_curr = bat_data.bms_data[0].total_current;
    bat_data.all_data.soc = bat_data.bms_data[0].soc;
    bat_data.all_data.soh = bat_data.bms_data[0].soh;
    bat_data.bms_data[0].sync_bit = 1;
    bat_data.all_data.sync_bit = bat_data.bms_data[0].sync_bit;
    bat_data.all_data.run_state = bat_data.bms_data[0].run_state;
    bat_data.all_data.charging_state = bat_data.bms_data[0].charging_state;
    if (bat_data.all_data.charging_state == 1) {// 充电
        bat_data.all_data.chg_full_time = bat_data.bms_data[0].chg_full_time;
    } else {
        bat_data.all_data.dsg_empty_time = bat_data.bms_data[0].dsg_empty_time;
    }
    if(bat_data.bms_data[0].protect1.all){
        bat_data.all_data.state.bit.stop_chg = 1;
        bat_data.all_data.state.bit.stop_dsg = 1;
    } else {
        bat_data.all_data.state.bit.stop_chg = 0;
        bat_data.all_data.state.bit.stop_dsg = 0;
    }
}

/**
 * @brief 更新基于VERSION来源的BMS型号
 *
 * 只有在type_source优先级不高于VERSION时才更新型号，
 * 持续接收VERSION帧则重置超时并检测型号变化（热插拔场景）。
 *
 * @param[in] index   BMS索引
 * @param[in] new_type 新的型号字符串
 */
static void update_bms_type_from_version(uint8_t index, const char* new_type) {
    if (index >= MAX_BMS_NUM || new_type == NULL) {
        return;
    }
    
    // 只有在优先级不高于VERSION时才更新type
    if (bat_data.bms_data[index].type_source < TYPE_SOURCE_VERSION) {
        strncpy(bat_data.bms_data[index].type, new_type, 12);
        bat_data.bms_data[index].type_source = TYPE_SOURCE_VERSION;
        bat_data.bms_data[index].type_source_timeout = BAT_TIMEOUT;
    } else if (bat_data.bms_data[index].type_source == TYPE_SOURCE_VERSION) {
        // 持续接收VERSION来源帧，重置超时并检查型号是否变化
        bat_data.bms_data[index].type_source_timeout = BAT_TIMEOUT;
        // 如果型号变化（热插拔场景），更新type
        if (strncmp(bat_data.bms_data[index].type, new_type, 12) != 0) {
            strncpy(bat_data.bms_data[index].type, new_type, 12);
        }
    }
}

/**
 * @brief 检查电池低SOC停机逻辑
 *
 * 若电池SOC低于设定值且光伏不在线，累计超时后触发停机命令。
 *
 * @param[in] interval 本次递增的时间间隔（单位ms）
 */
static void check_low_soc(uint32_t interval) {
    static uint32_t low_soc_timeout = 0;
    uint8_t is_pv_online = 0;//dcdc_mi_data->reg0000.state_a.bit.pv_online;
    uint16_t socl = 0;
    if(strcmp(bat_data.bms_data[0].type, "B230") == 0){
        socl = 0; // B230低电量停机SOC为0
    }

    // 电池包低电量停机避免过度亏电
    if((bat_data.all_data.soc <= socl) && (!is_pv_online)) { 
        // 1小时关机
        low_soc_timeout += interval;
        if(low_soc_timeout > LOW_SOC_TIMEOUT) { 
            bat_stop_cmd = 1;
        }
    } else {
        low_soc_timeout = 0;
        bat_stop_cmd = 0;
    }
}

// 获取电池包类型索引
 static uint8_t get_bat_type_index(const char* type_str) {
     if (strcmp(type_str, "B300") == 0) return BAT_TYPE_B300;
     if (strcmp(type_str, "B300S") == 0) return BAT_TYPE_B300S;
     if (strcmp(type_str, "B300K") == 0) return BAT_TYPE_B300K;
     if (strcmp(type_str, "B230") == 0) return BAT_TYPE_B230;
     if (strcmp(type_str, "B210") == 0 || strcmp(type_str, "B210P") == 0) return BAT_TYPE_B210;
     return BAT_TYPE_UNKNOWN;
 }

 // 重置电池包类型统计
 static void reset_bat_type_stats(void) {
     for (int i = 0; i < BAT_TYPE_MAX; i++) {
         bat_type_stats[i].count = 0;
         bat_type_stats[i].version_count = 0;
         memset(bat_type_stats[i].versions, 0, sizeof(bat_type_stats[i].versions));
         memset(bat_type_stats[i].addr_list, 0, sizeof(bat_type_stats[i].addr_list));
     }
 }

 // 添加电池包版本到类型统计
 static void add_bat_version_to_stats(uint8_t type_idx, uint32_t version, uint8_t addr) {
     // 添加地址
     uint8_t addr_added = 0;
     for (int i = 0; i < 8; i++) {
         if (bat_type_stats[type_idx].addr_list[i] == addr) {
             addr_added = 1;
             break;
         } else if (bat_type_stats[type_idx].addr_list[i] == 0) {
             bat_type_stats[type_idx].addr_list[i] = addr;
             bat_type_stats[type_idx].count++;
             addr_added = 1;
             break;
         }
     }
     
     // 添加版本号
     if (version == 0) return;

     uint8_t version_added = 0;
     for (int i = 0; i < bat_type_stats[type_idx].version_count; i++) {
         if (bat_type_stats[type_idx].versions[i] == version) {
             version_added = 1;
             break;
         }
     }
  
     if (!version_added && bat_type_stats[type_idx].version_count < 8) {
         bat_type_stats[type_idx].versions[bat_type_stats[type_idx].version_count++] = version;
     }
 }

/**
 * @brief 组合 BMS 数据并更新状态标志。
 * 
 * 该函数根据特定条件组合 BMS 数据并更新状态标志。
 * 它检查每个 BMS 的类型，并在混包 'mix' 变量中设置相应的位。
 * 它还检查是否有任何 BMS 正在忙碌或处于bootloader模式，并相应地更新 'bat_is_busy' 和 'bat_is_boot' 标志。
 * 如果存在 B300 系列和 B230/B210 系列的混合，或者连接了超过两个电池组，它会设置 'is_mix_err' 标志和 'bat_is_busy' 标志。
 * 它还检查是否有任何 BMS 处于bootloader模式达到一定次数，并相应地设置 'is_boot' 标志。
 * 最后，它更新整体电池状态的 'is_busy' 标志以及每个 BMS 的 'is_busy' 标志。
 */
static void bms_data_combine(void){
    static uint8_t boot_count = 0;
    bool bat_is_busy = false;
    bool bat_is_boot = false;
    uint8_t mix = 0;
    uint8_t i = 0;

#if 0   // 外部统计
    // 重置电池包类型统计
    reset_bat_type_stats();

    // 遍历所有 BMS 数据
    for(i = 0; i < MAX_BMS_NUM; i++){
        if(bat_data.bms_data[i].valid == 0){
            continue;
        }
        // 条件一：B300或B300S并机则繁忙 条件二：只要有一个电池包版本号为99则繁忙
        if(strcmp(bat_data.bms_data[i].type, "B300") == 0 || strcmp(bat_data.bms_data[i].type, "B300S") == 0) {
            mix |= 1<<0;
            if(bat_data.all_data.bat_num > 1){
                bat_is_busy = true;
            } else if(bat_data.bms_data[i].version[0]%100 == 99){
                bat_is_busy = true;
            }
        } else if(strcmp(bat_data.bms_data[i].type, "B300K") == 0) {
            mix |= 1<<0;
        } else if(strcmp(bat_data.bms_data[i].type, "B210") == 0 || strcmp(bat_data.bms_data[i].type, "B210P") == 0){
            mix |= 1<<1;
        } else if(strcmp(bat_data.bms_data[i].type, "B230") == 0){ // 只支持一个包
            mix |= 1<<1;
            if(bat_data.all_data.bat_num > 1){
                bat_is_busy = true;
                mix = 0x03; // B230多包不允许充放电
            }
        }
        // 检查BMS是否处于boot状态
        if((bat_data.bms_data[i].version[0]/100 != 0 && (bat_data.bms_data[i].version[0]%100 == 0 || bat_data.bms_data[i].version[0]%100 == 99)) ||
            (bat_data.bms_data[i].version[1]/100 != 0 && bat_data.bms_data[i].version[1]%100 == 0)){ // 有电池包处在Boot
            bat_is_boot = true; // 
        }

        // 更新电池包类型统计
        add_bat_version_to_stats(get_bat_type_index(bat_data.bms_data[i].type), bat_data.bms_data[i].version[0], get_addr_trace(i));    
        add_bat_version_to_stats(get_bat_type_index(bat_data.bms_data[i].type), bat_data.bms_data[i].version[1], get_addr_trace(i));    
    }
    
    // 0x01<<0 | 0x01<<1 B300系列和B230、B210并机
    if (mix == 0x03) { 
        bat_data.state.bit.is_mix_err = 1;
        bat_is_busy = true; // 混包后不能升级
    } else {
        bat_data.state.bit.is_mix_err = 0;
    }

    // Boot 超时状态判断
    if(bat_is_boot){
        boot_count++;
        if(boot_count >= 10){
            boot_count = 0;
            bat_data.state.bit.is_boot = 1;
        }
    } else {
        boot_count = 0;
        bat_data.state.bit.is_boot = 0;
    }

    if(bat_is_busy){
        bat_data.state.bit.is_busy = 1; // 汇总
        for (i = 0; i < MAX_BMS_NUM; i++){
            bat_data.bms_data[i].ota_state.bit.is_busy = 1; 
        }
    } else {
        bat_data.state.bit.is_busy = 0;
        for (i = 0; i < MAX_BMS_NUM; i++){
            bat_data.bms_data[i].ota_state.bit.is_busy = 0; 
        }
    }
#else
    bat_data.state.bit.is_busy = get_pack_ver_busy();
    bat_data.state.bit.is_mix_err = get_pack_ver_mix();
#endif

    // 特殊情况：B230单包没有主机帧
    if(strcmp(bat_data.bms_data[0].type, "B230") == 0 && bat_data.all_data.bat_num == 1 && bat_data.bms_data[0].valid == 1){ // B230目前仅支持1对1
        update_b230_data();
    }

    // 更新BMS故障与保护状态
    bms_fault_update();
}

/**
 * @brief BMS超时处理定时器
 *
 * 轮询所有电池包，处理超时、在线状态、型号来源降级等逻辑。
 * 定时器处理建议只做必要的状态维护，不做复杂业务。
 *
 * @param[in] interval 定时器周期（单位ms）
 */
static void bms_timeout_check(uint16_t interval) { // 定时器处理不要做太多事情
    uint8_t bat_online_num = 0;
    uint16_t bat_online_bit = 0;
    uint8_t i;
    for (i = 0; i < MAX_BMS_NUM; i++) {
        if (bat_data.bms_data[i].timeout > interval) {
            bat_data.bms_data[i].timeout -= interval;
            bat_online_num++;
            bat_online_bit |= (1 << i);
                    
            // 生成设备接入记录
            if((bat_data.bms_data[i].dev_access_state == 0) 
                && (bat_data.bms_data[i].type_source != TYPE_SOURCE_NONE)
                && (bat_data.bms_data[i].sn_code != 0)
                && (1 == reals.rtc_flag.sBit.Time_valid)){
                bat_data.bms_data[i].dev_access_state = 1;

#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE   
                // 更新记录参数
                dev_access_params_t record = {0};
                record.record_type = DEVICE_EVENT_REC_TYPE_DEV_FROM_CAN;
                record.parent_address = 0xFF; // 无效
                record.local_address = i + PACK_CAN_ADDR;
                record.operation_attribute = DEVICE_EVENT_OP_AUTO_ADD;
                record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                record.identifier_info.sn_info.dev_type = bat_data.bms_data[i].type;
                record.identifier_info.sn_info.dev_sn = &bat_data.bms_data[i].sn_code;

                // 生成记录到队列
                Dev_Access_Log_Generate(&record);
#endif             
            }
        } else {
            if ( bat_data.bms_data[i].valid )
            {
                // 生成设备接入记录
                if ( bat_data.bms_data[i].dev_access_state == 1 ) {
                    bat_data.bms_data[i].dev_access_state = 0;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                    // 更新记录参数
                    dev_access_params_t record = {0};
                    record.record_type = DEVICE_EVENT_REC_TYPE_DEV_FROM_CAN;
                    record.parent_address = 0xFF; // 无效
                    record.local_address = i + PACK_CAN_ADDR;
                    record.operation_attribute = DEVICE_EVENT_OP_TIMEOUT_OFFLINE;
                    record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                    record.identifier_info.sn_info.dev_type = bat_data.bms_data[i].type;
                    record.identifier_info.sn_info.dev_sn = &bat_data.bms_data[i].sn_code;
                    
                    // 生成记录到队列
                    Dev_Access_Log_Generate(&record);
#endif
                }
                
                bat_data.bms_data[i].timeout = 0;
                bat_data.bms_data[i].valid = 0;
                memset(&bat_data.bms_data[i], 0, sizeof(bms_data_t));
                bat_addr_trace.addr_pool[i] = 0;
            }

            continue;
        }
        
        // 检查型号来源超时 - 支持多来源优先级降级
        if (bat_data.bms_data[i].type_source_timeout > interval) {
            bat_data.bms_data[i].type_source_timeout -= interval;
        } else if (bat_data.bms_data[i].type_source_timeout > 0 || 
                   bat_data.bms_data[i].type_source != TYPE_SOURCE_NONE) {
            // 超时或需要清除
            bat_data.bms_data[i].type_source_timeout = 0;
            
            // 根据当前来源降级处理
            if (bat_data.bms_data[i].type_source == TYPE_SOURCE_0X56) {
                // 0x56超时，降级到VERSION或0x10（保留type，等待其他来源更新）
                bat_data.bms_data[i].type_source = TYPE_SOURCE_NONE;
                // ESP_LOGW(TAG, "BMS[%d] 0x56 frame timeout, type source cleared", i);
            } else if (bat_data.bms_data[i].type_source == TYPE_SOURCE_VERSION) {
                // VERSION超时，降级到0x10（保留type，等待其他来源更新）
                bat_data.bms_data[i].type_source = TYPE_SOURCE_NONE;
                // ESP_LOGW(TAG, "BMS[%d] VERSION frame timeout, type source cleared", i);
            } else if (bat_data.bms_data[i].type_source == TYPE_SOURCE_0X10) {
                // 0x10超时，清除所有type信息（可能电池包被更换）
                bat_data.bms_data[i].type_source = TYPE_SOURCE_NONE;
            }
        }
    }
   
    if (bat_online_num == 0) { // 电池汇总数据清除
        bat_data.all_data.valid = 0;
    } else {
        bat_data.all_data.valid = 1; // 电池包在线状态
    }
    
    bat_data.all_data.online_bit = bat_online_bit;
    bat_data.all_data.bat_num = bat_online_num; // 电池在线数量 
}

/**
 * 根据BMS型号数值获取型号字符串
 * 
 * Args:
 *     model_id: BMS型号数值
 * 
 * Returns:
 *     对应的型号字符串，如果数值无效则返回NULL
 */
static const char* get_bms_model_name_by_id(uint8_t model_id)
{
    switch(model_id) {
        case 0:  return "AC200M"; //AC200L 的设备型号发送的是AC200MAX-BMS  因旧B300上只响应这个型号的指令，AC200MAX型号缩写为AC200M
        case 1:  return "B230";
        case 2:  return "B300";
        case 3:  return "AC240";
        case 4:  return "B210";
        case 5:  return "AC60";
        case 6:  return "B80";
        case 7:  return "AC180";
        case 8:  return "PR009";
        case 9:  return "AC200L";
        case 10: return "B300K";
        case 11: return "AP300"; // AC380实际型号是AP300
        default: return NULL;
    }
}

//电池包类型
char* get_bat_type(uint8_t index){
    return bat_data.bms_data[index].type;
}

// index转换成can地址
int get_addr_trace(uint8_t index){
    if(index >= MAX_BMS_NUM){
        return 0;
    }
    return bat_addr_trace.addr_pool[index];
}

// bms从机地址转化成包号 返回-1表示无效地址，返回最大值不超过MAX_BMS_NUM，否则可能导致数组溢出
int bms_addr_to_index(uint8_t addr)
{
    for(uint8_t i = 0; i < MAX_BMS_NUM; i++){ 
        // ESP_LOGW(__func__, "addr_pool[%d]: %d, addr:%d", i, bat_addr_trace.addr_pool[i], addr);
        if(bat_addr_trace.addr_pool[i] == addr){
            return i;
        }
    }
    return -1;
}
// bms从机地址转化成包号 返回-1表示无效地址，返回最大值不超过MAX_BMS_NUM，否则可能导致数组溢出
int bms_version_to_index(uint32_t version)
{
	uint8_t index[MAX_BMS_NUM] = {0};
    for(uint8_t i = 0; i < MAX_BMS_NUM; i++){ 
        // ESP_LOGW(__func__, "addr_pool[%d]: %d, addr:%d", i, bat_addr_trace.addr_pool[i], addr);
        if((bat_data.bms_data[i].version[0]/100) == version){
			index[i] = 1;
			return i;
        }
		else
		{
			index[i] = 0;
		}
    }
    return -1;
}

/**
 * @brief 分配地址1到地址池[0]位置
 * 
 * @return int 分配的索引，-1表示失败
 */
static int allocate_addr_1(void)
{
    // 如果地址池[0]已被占用且不是地址1，需要重新分配
    if (bat_addr_trace.addr_pool[0] != 0 && bat_addr_trace.addr_pool[0] != 1) {
        uint8_t old_addr = bat_addr_trace.addr_pool[0];
        
        // 将原地址移到其他空位
        for (uint8_t i = 1; i < MAX_BMS_NUM; i++) {
            if (bat_addr_trace.addr_pool[i] == 0) {
                bat_addr_trace.addr_pool[i] = old_addr;
                break;
            }
        }
    }
    
    // 分配地址1到[0]位置
    bat_addr_trace.addr_pool[0] = 1;
    bat_data.bms_data[0].timeout = BAT_TIMEOUT;
    ESP_LOGW(__func__, "addr: 1, index: 0");
    return 0;
}

/**
 * @brief 分配新地址到地址池
 * 
 * @param addr 要分配的地址
 * @return int 分配的索引，-1表示地址池已满
 */
static int allocate_new_addr(uint8_t addr)
{
    for (uint8_t i = 0; i < MAX_BMS_NUM; i++) {
        if (bat_addr_trace.addr_pool[i] == 0) {
            bat_addr_trace.addr_pool[i] = addr;
            bat_data.bms_data[i].timeout = BAT_TIMEOUT;
            ESP_LOGW(__func__, "addr: %d, index: %d", addr, i);
            return i;
        }
    }
    
    ESP_LOGE(__func__, "Address pool full, cannot allocate addr: %d", addr);
    return -1;
}

/**
 * @brief 将CAN地址转换为内部索引，管理电池包地址池
 * 
 * @param addr CAN地址
 * @param func 功能码
 * @return int 返回索引：-1表示无效地址，0xFF表示主包帧忽略，其他为有效索引
 */
int bms_addr_trace(uint8_t addr, uint8_t func)
{
    // 检查是否为电池单包CAN ID
    bool is_battery_frame = ((func >= 0x01 && func <= 0x0D) || func == 0x3D);
    
    // 主包汇总帧处理 - 直接忽略
    if (addr == 1 && (func == 0x0E || func == 0x0F || func == 0x1F || func == 0x20 || func == 0x30)) {
        return 0xFF;
    }
    
    // 处理B300 boot状态
    if (func == 0x3D) {
        bat_data.state.bit.is_b300_boot = 1;
        b300_boot_timeout = 5000; // 5s超时
    }
    
    // 地址1的特殊处理：B300 boot状态下使用地址2
    uint8_t actual_addr = addr;
    if (addr == 1 && bat_data.state.bit.is_b300_boot == 1 && func != 0x3D) {
        actual_addr = 2;
    }
    
    // 查找已存在的地址
    for (uint8_t i = 0; i < MAX_BMS_NUM; i++) {
        if (bat_addr_trace.addr_pool[i] == actual_addr) {
            return i;
        }
    }
    
    // 地址1的特殊分配逻辑
    if (actual_addr == 1 && is_battery_frame) {
        return allocate_addr_1();
    }
    
    // 分配新地址
    if (is_battery_frame) {
        return allocate_new_addr(actual_addr);
    }
    
    return -1; // 无效地址
}



/**
 * @brief 更新BMS Boot版本记录
 * 
 * @param bms_type 设备类型
 * @param boot_ver 版本号
 */
static void update_bms_boot_version(const char *bms_type, uint32_t boot_ver) {
    int i, j;
    bool type_found = false;
    
    // 查找该类型是否已存在
    for (i = 0; i < bms_boot_type_count; i++) {
        if (strcmp(bms_boot_versions[i].type, bms_type) == 0) {
            type_found = true;
            
            // 查找该版本是否已存在
            bool version_found = false;
            for (j = 0; j < bms_boot_versions[i].number; j++) {
                if (bms_boot_versions[i].version[j] == boot_ver) {
                    version_found = true;
                    break;
                }
            }
            
            // 如果版本不存在且还有空间，则添加
            if (!version_found && bms_boot_versions[i].number < 6) {
                bms_boot_versions[i].version[bms_boot_versions[i].number] = boot_ver;
                bms_boot_versions[i].number++;
                ESP_LOGI(TAG, "Added new boot version %lu for type %s, total versions: %d", 
                        boot_ver, bms_type, bms_boot_versions[i].number);
            }
            
            break;
        }
    }
    
    // 如果类型不存在且还有空间，则添加新类型
    if (!type_found && bms_boot_type_count < BMS_BOOT_MAX_TYPES) {
        i = bms_boot_type_count;
        strncpy(bms_boot_versions[i].type, bms_type, sizeof(bms_boot_versions[i].type) - 1);
        bms_boot_versions[i].version[0] = boot_ver;
        bms_boot_versions[i].number = 1;
        bms_boot_type_count++;
        ESP_LOGI(TAG, "Added new BMS type %s with boot version %lu", 
                bms_type, boot_ver);
    }
}

/**
 * @brief 48V BMS-CAN帧解析处理
 *
 * 解析电池包的CAN总线帧，根据不同功能码(func)更新对应电池包的数据结构。
 * 支持SOC、电压电流、保护状态、型号、版本、同步状态等多种功能帧的解析。
 *
 * @param[in] can_frame 指向接收到的CAN帧结构体指针
 */
void bms_can_handle(twai_message_t *can_frame) 
{
    if (can_frame == NULL) return;
    
    can_id_t_alpha canId = {0};
    canId.id = can_frame->identifier;

    // 协议标志检查
    if (CAN_ID_ALPHA_FLAG != canId.bit.fix) {
        return;
    }

    // 检查和分配电池包地址
    uint8_t func = canId.bit.pn;
    int index = bms_addr_trace(canId.bit.sa, func); // 地址转换成索引
    if(index < 0){
        return; // 无效地址
    } else {
        reals.can_pack_rx_counter++;
    }

    // 检查B230 Boot状态
	if (can_frame && check_b230_count) {
		int ret = check_b230_boot_ack(can_frame);
		if (ret == 1) {
			check_b230_count++;
			if (check_b230_count == 2) update_b230_boot_data();
			check_b230_count = 0;
		} 
	}

#if CONFIG_LOG_UDP_REDIRECT == 2 
    supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
    supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
    if ((1 == resource.can) && (1 == direction.rx))
    {
        aiot_log_can_frame_t log_frame = {.id.value = 0, .data = {0}};
        log_frame.id.identifier = can_frame->identifier & 0x1FFFFFFF; // 仅保留29位ID
        log_frame.id.tx = 0; // 标记为接收帧
        memcpy(log_frame.data, can_frame->data, sizeof(can_frame->data));
        aiot_log_push((const uint8_t *)&log_frame, (uint16_t)sizeof(log_frame), LOG_TYPE_CAN_RX);
    }
#endif  

	ESP_LOGD (TAG, "Recv alpha frame [ID:0x%04lx, func:%d,data[0]: %d]", canId.id, func, can_frame->data[0]);
    switch (func)
    {
        case 0x01:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].devId = index;
            bat_data.bms_data[index].timeout = BAT_TIMEOUT;
            bat_data.bms_data[index].valid = 1;
            bat_data.bms_data[index].soc = can_frame->data[0]; 
            bat_data.bms_data[index].run_state = can_frame->data[1];
            bat_data.bms_data[index].protect1.all = can_frame->data[2] | (can_frame->data[3]<<8);
            bat_data.bms_data[index].charging_state = can_frame->data[4];
            bat_data.bms_data[index].avg_temp = can_frame->data[5];
            bat_data.bms_data[index].ver_u8[0] =  can_frame->data[6];
            bat_data.bms_data[index].ver_u8[1] = can_frame->data[7];

            ESP_LOGD(TAG, "cmd:0x01, devId: %d, timeout: %d, soc: %d, protect1: %d, protect2: %d, charging_state: %d, avg_temp: %d", 
                             bat_data.bms_data[index].devId, 
                             bat_data.bms_data[index].timeout, 
                             bat_data.bms_data[index].soc, 
                             bat_data.bms_data[index].protect1.all, 
                             bat_data.bms_data[index].protect2.all, 
                             bat_data.bms_data[index].charging_state, 
                             bat_data.bms_data[index].avg_temp);
        break;

        case 0x02:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].timeout = BAT_TIMEOUT;
            bat_data.bms_data[index].valid = 1;
            bat_data.bms_data[index].total_voltage = can_frame->data[0] | (can_frame->data[1] << 8);
            bat_data.bms_data[index].total_current = can_frame->data[2] | (can_frame->data[3] << 8);
            bat_data.bms_data[index].ver_u8[2] = can_frame->data[4];

            // 检查版本号数据是否完整
            if(bat_data.bms_data[index].ver_u8[1] && bat_data.bms_data[index].ver_u8[2]){
                uint32_t bms_ver_complete = bat_data.bms_data[index].ver_u8[0] + 
                                bat_data.bms_data[index].ver_u8[1] * 100 + 
                                bat_data.bms_data[index].ver_u8[2] * 10000; // 完整版本号 注：此处默认每一字节是两位数
                uint16_t bms_ver_type = bms_ver_complete / 100; // 大版本号

                // 符合短料号规则才使用这里，否则使用52帧
                if ( bms_ver_complete <= 999999 && bms_ver_complete >= 100001){
                    bat_data.bms_data[index].version[0] = bms_ver_complete;
                    bat_data.bms_data[index].is_ver_used_canid52 = false;
                    if(is_b300_series(bms_ver_type)) {
                        //伪造电池包SN码 B300无SN信息
                        bat_data.bms_data[index].sn_code = dev_factory.dev_sn;
                        bat_data.bms_data[index].mcu_type[0] = DEVICE_BMS;
                        update_bms_type_from_version(index, get_bms_series_name(bms_ver_type));
                        bat_data.bms_data[index].mcu_type[1] = 0;
                        bat_data.bms_data[index].version[1] = 0;
                    } else if(is_b300s_series(bms_ver_type)){
                        //伪造电池包SN码 B300S无SN信息
                        bat_data.bms_data[index].sn_code = dev_factory.dev_sn;
                        bat_data.bms_data[index].mcu_type[0] = DEVICE_BMS;
                        update_bms_type_from_version(index, get_bms_series_name(bms_ver_type));
                        bat_data.bms_data[index].mcu_type[1] = 0;
                        bat_data.bms_data[index].version[1] = 0;
                    }else if(is_b300k_series(bms_ver_type)){
                        /*识别到B300K电池包*/
                        bat_data.bms_data[index].mcu_type[0] = DEVICE_BMS;
                        update_bms_type_from_version(index, get_bms_series_name(bms_ver_type));
                        bat_data.bms_data[index].mcu_type[1] = 0;
                        bat_data.bms_data[index].version[1] = 0;
                    } else if(is_b230_series(bms_ver_type)){
                        //伪造电池包SN码 B230无SN信息
                        bat_data.bms_data[index].sn_code = dev_factory.dev_sn;
                        bat_data.bms_data[index].mcu_type[0] = DEVICE_PACK_BMS;
                        update_bms_type_from_version(index, get_bms_series_name(bms_ver_type));
                        bat_data.bms_data[index].mcu_type[1] = 0;
                        bat_data.bms_data[index].version[1] = 0;
                    } else if(is_b210_series(bms_ver_type)){
                        bat_data.bms_data[index].mcu_type[0] = DEVICE_PACK_BMS;
                        update_bms_type_from_version(index, get_bms_series_name(bms_ver_type));
                    } else if(is_power5_series(bms_ver_type)){
                        bat_data.bms_data[index].mcu_type[0] = DEVICE_BMS;
                        update_bms_type_from_version(index, get_bms_series_name(bms_ver_type));
                    }else{
                        // 新的电池包在上方添加(此处为默认)
                        bat_data.bms_data[index].mcu_type[0] = DEVICE_BMS;
                        update_bms_type_from_version(index, get_bms_series_name(bms_ver_type));
                    }
                }else {
                    // 版本号使用52帧
                    bat_data.bms_data[index].is_ver_used_canid52 = true;
                }

                uint8_t mcu_num = 0;
                if(bat_data.bms_data[index].version[0]) { // BMS版本号不为0
                    mcu_num++;
                }
                if(bat_data.bms_data[index].version[1]) { // M1版本号不为0
                    mcu_num++;
                }
                bat_data.bms_data[index].mcu_num = mcu_num;
            }
            
            ESP_LOGD(TAG, "bms_data[%d].sn_code is %llu",index, bat_data.bms_data[index].sn_code);
			ESP_LOGD(TAG, "bms_data[%d].version is %lu",index, bat_data.bms_data[index].version[0]);
            ESP_LOGD(TAG, "packet_total_voltage: %d, total_current: %d", 
                bat_data.bms_data[index].total_voltage, 
                bat_data.bms_data[index].total_current);
        break;

        case 0x03: // 每帧4节电芯
        case 0x04:
        case 0x05:
        case 0x06:
            if (index >= MAX_BMS_NUM) {
                break;
            }

            // 更新单节电压数据
            uint8_t j = (func - 0x03) * 4;
            for (uint8_t i = 0; i < 8; i += 2) {
                bat_data.bms_data[index].cell_volt[j++].bit.volt = 
                            can_frame->data[i] | (can_frame->data[i+1] << 8);

                // 更新电芯节数
                if ( bat_data.bms_data[index].total_cell_num < j
                    && bat_data.bms_data[index].cell_volt[j-1].bit.volt != 0 ) {
                    bat_data.bms_data[index].total_cell_num = j;
                }
            }
        break;

        case 0x09:
            if (index >= MAX_BMS_NUM) {
                break;
            }

            // SOH
            bat_data.bms_data[index].soh = (can_frame->data[0] > 100) ? 100 : can_frame->data[0]; 
            ESP_LOGD(TAG, "soh: %d", bat_data.bms_data[index].soh);
            
            // NTC温度
			for (int i = 0;  i < 4; ++i) {
				bat_data.bms_data[index].ntc_Group[i] = can_frame->data[i+1];
				ESP_LOGD(TAG, "ntc_Group[%d]: %d",i, bat_data.bms_data[index].ntc_Group[i]);
                // 更新NTC个数
                if ( bat_data.bms_data[index].total_ntc_num < (i + 1)
                    && bat_data.bms_data[index].ntc_Group[i] != 0 ) {
                    bat_data.bms_data[index].total_ntc_num = i + 1;
                }
			}

            // 电池电芯均衡标记
            uint16_t balance = can_frame->data[5] | (can_frame->data[6] << 8);
            for (uint8_t i = 0; i < 16; i++) { 
                bat_data.bms_data[index].cell_volt[i].bit.bal = (balance & 0x01) ? 1 : 0;
                balance >>= 1;
            }
        break;

        case 0x0A:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].curr_chg_energy = can_frame->data[0] | (can_frame->data[1] << 8); // 0.001Kwh
            bat_data.bms_data[index].PackInner = can_frame->data[2];    // mΩ
            bat_data.bms_data[index].curr_dsg_energy = can_frame->data[3] | (can_frame->data[4] << 8); // 0.001Kwh
            bat_data.bms_data[index].used_cycle = can_frame->data[6] | (can_frame->data[7] << 8); // 电池使用周期
            ESP_LOGD(TAG, "used_cycle: %d", bat_data.bms_data[index].used_cycle);
        break;

        case 0x0B:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].total_chg_energy =  (can_frame->data[0] << 0) | (can_frame->data[1] << 8) | 
                                                        (can_frame->data[2] << 16) | (can_frame->data[3] << 24); // 0.01Kwh
            bat_data.bms_data[index].total_dsg_energy =  (can_frame->data[4] << 0) | (can_frame->data[5] << 8) | 
                                                        (can_frame->data[6] << 16) | (can_frame->data[7] << 24); // 0.01Kwh
            ESP_LOGD(TAG, "total_chg_energy: %lu, total_dsg_energy: %lu", 
                            bat_data.bms_data[index].total_chg_energy, 
                            bat_data.bms_data[index].total_dsg_energy);                                            
        break;
        
        case 0x0C:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].pack_volt = can_frame->data[6] | (can_frame->data[7] << 8);
        break;
            
        case 0x0D:
        break;
    
        case 0x0E: // bms主机汇总帧/并包交互帧
            if (index == 0xFF) 
			{
				bat_data.all_data.chg_max_volt = can_frame->data[0] | (can_frame->data[1] << 8);
            	bat_data.all_data.chg_max_curr = can_frame->data[2] | (can_frame->data[3] << 8);
                bat_data.all_data.dsg_max_curr = can_frame->data[4] | (can_frame->data[5] << 8);
                bat_data.all_data.chg_ctrl.all = can_frame->data[6];  
                #if 0   // IOT自行计算(bms_timeout_check)
                bat_data.all_data.online_bit = can_frame->data[7];  
                #endif
                ESP_LOGD(TAG, "[all_data]chg_max_volt: %d, chg_max_curr:%d, dsg_max_curr:%d, chg_ctrl:%d, online_bit:%d", 
                                bat_data.all_data.chg_max_volt,
                                bat_data.all_data.chg_max_curr,
                                bat_data.all_data.dsg_max_curr,
                                bat_data.all_data.chg_ctrl.all,
                                bat_data.all_data.online_bit);				
            } 
			else if(index < MAX_BMS_NUM)
			{
                bat_data.bms_data[index].chg_max_volt = can_frame->data[0] | (can_frame->data[1] << 8);
                bat_data.bms_data[index].chg_max_curr = can_frame->data[2] | (can_frame->data[3] << 8);
                bat_data.bms_data[index].dsg_max_curr = can_frame->data[4] | (can_frame->data[5] << 8);
                bat_data.bms_data[index].chg_ctrl.all = can_frame->data[6];  
                bat_data.bms_data[index].online_bit = can_frame->data[7];  
				ESP_LOGD(TAG, "chg_max_volt: %d, chg_max_curr:%d, dsg_max_curr:%d, chg_ctrl:%d, online_bit:%d", 
												bat_data.bms_data[index].chg_max_volt,
												bat_data.bms_data[index].chg_max_curr,
												bat_data.bms_data[index].dsg_max_curr,
												bat_data.bms_data[index].chg_ctrl.all,
												bat_data.bms_data[index].online_bit);
			} else {
                //do nothing
            }

        break;

        case 0x0F: // bms主机汇总帧
            if (index != 0xFF) {
                break;
            }
            bat_data.all_data.total_volt = can_frame->data[0] | (can_frame->data[1] << 8); // 总电压
            bat_data.all_data.total_curr = can_frame->data[2] | (can_frame->data[3] << 8); // 总电流
            bat_data.all_data.soc = can_frame->data[4]; // SOC
            if(strcmp(bat_data.bms_data[0].type, "B300") == 0 || strcmp(bat_data.bms_data[0].type, "B300S") == 0
                || strcmp(bat_data.bms_data[0].type, "B500K") == 0){ // B300并容状态使用该帧
                uint16_t sync_bit = can_frame->data[5]&0x0F;
                uint16_t app_sync_bit = 0;
                for (uint8_t i = 0; i < 4; i++) { // B300地址1~4
                    int bat_index = bms_addr_to_index(i+1); // 地址转换成包号
                    if(bat_index >= 0){
                        if (sync_bit & (1<<i)) {
                            bat_data.bms_data[bat_index].sync = 1;
                            app_sync_bit |= 1 << bat_index;
                        } else {
                            bat_data.bms_data[bat_index].sync = 0;
                        }
                        ESP_LOGD(TAG, "bat[%d] sync %d", bat_index, bat_data.bms_data[bat_index].sync);
                    }
                }
                #if 0   // IOT自行计算(bms_timeout_check)
                bat_data.all_data.bat_num = can_frame->data[5]>>4;
                #endif
                bat_data.all_data.sync_bit = app_sync_bit;
                ESP_LOGD(TAG, "bat all sync: %d, to app bat all sync: %d", sync_bit, bat_data.all_data.sync_bit);
                bat_data.all_data.soh = 100; // B300系列此位置不提供SOH
            } else if(strcmp(bat_data.bms_data[0].type, "B210") == 0 || strcmp(bat_data.bms_data[0].type, "B210P") == 0){
                bat_data.all_data.soh = can_frame->data[5]; // 仅B210系列提供SOH
            } else {
                bat_data.all_data.soh = 100; // 其他电池包SOH默认100 B300系列此处为并包信息
            }
            
            bat_data.all_data.run_state = can_frame->data[6]; // 系统运行状态
            bat_data.all_data.charging_state = can_frame->data[7]; // 系统充放状态
            ESP_LOGD(TAG, "total_volt: %d, total_curr:%d, soc:%d, soh:%d, run_state:%d, charging_state:%d", 
                            bat_data.all_data.total_volt,  
                            bat_data.all_data.total_curr,
                            bat_data.all_data.soc,
                            bat_data.all_data.soh,
                            bat_data.all_data.run_state,
                            bat_data.all_data.charging_state);
                            
        break;

        case 0x10: // 个别机型不发送型号，依靠0x10帧判断型号 - 最低优先级
            if (index >= MAX_BMS_NUM) {
                break;
            }

            bat_data.bms_data[index].pack_type_num = can_frame->data[7];
            // 只有在没有更高优先级来源时才使用0x10帧（最低优先级）
            if (bat_data.bms_data[index].type_source == TYPE_SOURCE_NONE) {
                const char* model_name = get_bms_model_name_by_id(can_frame->data[7]);
                if (model_name != NULL) {
                    memset(bat_data.bms_data[index].type, 0, 12);
                    strncpy(bat_data.bms_data[index].type, model_name, 12);
                    bat_data.bms_data[index].type[11] = '\0';  // 确保字符串终止
                    bat_data.bms_data[index].type_source = TYPE_SOURCE_0X10;
                    bat_data.bms_data[index].type_source_timeout = BAT_TIMEOUT;
                    ESP_LOGI(TAG, "BMS[%d] model assigned by 0x10 frame: %s (id=%d, lowest priority)", 
                             index, bat_data.bms_data[index].type, can_frame->data[7]);
                }
            }
            // 如果已有0x10来源，持续接收到0x10帧时重置超时并检查型号是否变化
            else if (bat_data.bms_data[index].type_source == TYPE_SOURCE_0X10) {
                bat_data.bms_data[index].type_source_timeout = BAT_TIMEOUT;
                // 检查data[7]是否变化（热插拔场景），如果变化则更新型号
                const char* model_name = get_bms_model_name_by_id(can_frame->data[7]);
                if (model_name != NULL && strncmp(bat_data.bms_data[index].type, model_name, 12) != 0) {
                    memset(bat_data.bms_data[index].type, 0, 12);
                    strncpy(bat_data.bms_data[index].type, model_name, 12);
                    bat_data.bms_data[index].type[11] = '\0';  // 确保字符串终止
                    ESP_LOGW(TAG, "BMS[%d] type changed from 0x10: %s (id=%d)", 
                             index, bat_data.bms_data[index].type, can_frame->data[7]);
                }
            }

        break;

        case 0x1F:  // bms主机汇总帧
            if (index != 0xFF) {
                break;
            }
            if (bat_data.all_data.charging_state == 1) {// 充电
                bat_data.all_data.chg_full_time = can_frame->data[0] | (can_frame->data[1] << 8);
                ESP_LOGD(TAG, "chg_full_time: %d", bat_data.all_data.chg_full_time);
            } else {
                bat_data.all_data.dsg_empty_time = can_frame->data[0] | (can_frame->data[1] << 8);
                ESP_LOGD(TAG, "dsg_empty_time: %d", bat_data.all_data.dsg_empty_time);
            }
            #if 0 // 统一由IoT汇总(bms_fault_update)，不再调用电池包汇总
            bat_data.all_data.state.all = can_frame->data[2];
            #endif 
            if(strcmp(bat_data.bms_data[0].type, "B210") == 0 || strcmp(bat_data.bms_data[0].type, "B210P") == 0){
                uint16_t sync_bit = can_frame->data[6] | (can_frame->data[7] << 8); // B210系列并容状态使用该帧
                uint16_t app_sync_bit = 0;
                for (uint8_t i = 0; i < 8; i++) { 
                    int bat_index = bms_addr_to_index(i+1); // 地址转换成包号
                    if(bat_index >= 0){
                        if (sync_bit & (1<<i)) {
                            bat_data.bms_data[bat_index].sync = 1;
                            app_sync_bit |= 1 << bat_index;
                        } else {
                            bat_data.bms_data[bat_index].sync = 0;
                        }
                        ESP_LOGD(TAG, "bat[%d] sync %d", bat_index, bat_data.bms_data[bat_index].sync);
                    }
                }
                #if 0   // IOT自行计算(bms_timeout_check)
                bat_data.all_data.bat_num = (can_frame->data[7]&0xF0) >> 4;
                #endif
                bat_data.all_data.sync_bit = app_sync_bit;
                ESP_LOGD(TAG, "bat all sync: %d, to app bat all sync: %d", sync_bit, bat_data.all_data.sync_bit);
            }
        break;

        case 0x20: // bms主机汇总帧：只有B300S、B300K有(B300S改用0x0F，来源于Charge2)
            if (index != 0xFF) {
                break;
            }
            if(strcmp(bat_data.bms_data[0].type, "B300K") == 0){
                uint16_t sync_bit = can_frame->data[6] | (can_frame->data[7]&0x0F << 8); // // B300S、B300K并容状态使用该帧[7]高4位是并机数量
                uint16_t app_sync_bit = 0;
                for (uint8_t i = 0; i < 8; i++) { // B300S地址1~6、B300K地址2~7
                    int bat_index = bms_addr_to_index(i+1); // 地址转换成包号
                    if(bat_index >= 0){
                        if (sync_bit & (1<<i)) {
                            bat_data.bms_data[bat_index].sync = 1;
                            app_sync_bit |= 1 << bat_index;
                        } else {
                            bat_data.bms_data[bat_index].sync = 0;
                        }
                        ESP_LOGD(TAG, "bat[%d] sync %d", bat_index, bat_data.bms_data[bat_index].sync);
                    }
                }
                #if 0   // IOT自行计算(bms_timeout_check)
                bat_data.all_data.bat_num = (can_frame->data[7]&0xF0) >> 4;
                #endif
                bat_data.all_data.sync_bit = app_sync_bit;
                ESP_LOGD(TAG, "bat all sync: %d, to app bat all sync: %d", sync_bit, bat_data.all_data.sync_bit);
            }
        break;

        case 0x30:  // bms主机汇总帧
        break;

        case 0x32:  
        break;

        case 0x33:  
        break;

        case 0x34:  
        break;

        case 0x35:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].protect1.all = (can_frame->data[0] << 0) | (can_frame->data[1] << 8); // protect1 
            bat_data.bms_data[index].protect2.all = (can_frame->data[2] << 0) | (can_frame->data[3] << 8); // protect2
            ESP_LOGD(TAG,"0x35, protect1: 0x%x, protect2: 0x%x", 
                        bat_data.bms_data[index].protect1.all, bat_data.bms_data[index].protect2.all);
        break;

        case 0x3D: // B300\B300S进入boot后只发本帧，不发其他帧
            if(can_frame->data[1] && can_frame->data[2]){
                bat_data.bms_data[index].timeout = BAT_TIMEOUT;
                bat_data.bms_data[index].valid = 1;
                bat_data.bms_data[index].ver_u8[0] = can_frame->data[0];
                bat_data.bms_data[index].ver_u8[1] = can_frame->data[1];
                bat_data.bms_data[index].ver_u8[2] = can_frame->data[2]; 
                uint32_t boot_ver = bat_data.bms_data[index].ver_u8[0] + bat_data.bms_data[index].ver_u8[1] *100 + bat_data.bms_data[index].ver_u8[2]*10000;
                uint16_t bms_ver_type = boot_ver/100; // 大版本号
                bat_data.bms_data[index].state.bit.stop_chg = 1; // 进入boot后不允许充放
                bat_data.bms_data[index].state.bit.stop_dsg = 1;
                bat_data.bms_data[index].mcu_num = 1;
                bat_data.bms_data[index].mcu_type[0] = DEVICE_BMS;
                bat_data.bms_data[index].version[0] = (boot_ver / 100) * 100; // 例：102199-> 102100
                bat_data.bms_data[index].mcu_type[1] = 0; // 没有M1
                bat_data.bms_data[index].version[1] = 0;
                ESP_LOGW(__func__, "dev[%d] boot version %lu", index, bat_data.bms_data[index].version[0]);

                char *bms_type = NULL;
                if(is_b300_series(bms_ver_type)){ // B300
                    bat_data.bms_data[index].sn_code = dev_factory.dev_sn;
                    bms_type = get_bms_series_name(bms_ver_type);
                    update_bms_type_from_version(index, bms_type);
                } else if(is_b300s_series(bms_ver_type)){
                    bat_data.bms_data[index].sn_code = dev_factory.dev_sn;
                    bms_type = get_bms_series_name(bms_ver_type);
                    update_bms_type_from_version(index, bms_type);
                } else if(is_b300k_series(bms_ver_type)){
                    bms_type = get_bms_series_name(bms_ver_type);
                    update_bms_type_from_version(index, bms_type);
                }

                /*记录boot下的版本信息 */
                update_bms_boot_version(bms_type, boot_ver);
            }
        break;

        case 0x51:  
            if (index >= MAX_BMS_NUM) {
                break;
            }

            // 标定容量
            bat_data.bms_data[index].standard_capacity  = (can_frame->data[4] << 0) | (can_frame->data[5] << 8) |
                                                         (can_frame->data[6] << 16) | (can_frame->data[7] << 24);
        break;
        
        case 0x52:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            
            if ( bat_data.bms_data[index].is_ver_used_canid52 == true ) {
                bat_data.bms_data[index].mcu_type[0] = DEVICE_BMS; // BMS
                bat_data.bms_data[index].version[0]  = (can_frame->data[0] << 0) | (can_frame->data[1] << 8) |
                                                      (can_frame->data[2] << 16) | (can_frame->data[3] << 24);   
            }
            bat_data.bms_data[index].mcu_type[1] = DEVICE_PACK_M1; // M1
            bat_data.bms_data[index].version[1]  = (can_frame->data[4] << 0) | (can_frame->data[5] << 8) |
                                                  (can_frame->data[6] << 16) | (can_frame->data[7] << 24);
            ESP_LOGD(TAG, "index:%hhu,mcu1_type: %u, version:%lu, mcu2_type:%u, m1_version:%lu, mcu_num:%u", 
                            index,bat_data.bms_data[index].mcu_type[0],
                            bat_data.bms_data[index].version[0] ,
                            bat_data.bms_data[index].mcu_type[1],
                            bat_data.bms_data[index].version[1],
                            bat_data.bms_data[index].mcu_num);
        break;

        case 0x56: // bms type - 最高优先级
            if (index >= MAX_BMS_NUM) {
                break;
            }
            memcpy(bat_data.bms_data[index].type + 0, can_frame->data, 8);
            bat_data.bms_data[index].type_source = TYPE_SOURCE_0X56; // 设置最高优先级来源
            bat_data.bms_data[index].type_source_timeout = BAT_TIMEOUT; // 重置超时计数
            ESP_LOGD(TAG, "BMS[%d] type: %s (from 0x56, highest priority)", index, bat_data.bms_data[index].type);
        break;

        case 0x57: // bms type - 配合0x56使用
            if (index >= MAX_BMS_NUM) {
                break;
            }
            memcpy(bat_data.bms_data[index].type + 8, can_frame->data, 4);
            // 如果已有0x56来源，持续接收到0x57帧时也重置超时
            if (bat_data.bms_data[index].type_source == TYPE_SOURCE_0X56) {
                bat_data.bms_data[index].type_source_timeout = BAT_TIMEOUT;
            }
            ESP_LOGD(TAG, "BMS[%d] type: %s (0x57 continuation)", index, bat_data.bms_data[index].type);
        break;

        case 0x58: // bms sncode
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].sn_code = *(uint64_t *)can_frame->data;
            ESP_LOGD(TAG, "index:%hhu, sn_code: %llu",index, bat_data.bms_data[index].sn_code);
        break;
            
        case 0x60:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].max_cell_volt = (can_frame->data[0] << 0) | (can_frame->data[1] << 8); // mv
            bat_data.bms_data[index].min_cell_volt = (can_frame->data[2] << 0) | (can_frame->data[3] << 8); // mv
            bat_data.bms_data[index].max_cell_index = can_frame->data[4];
            bat_data.bms_data[index].min_cell_index = can_frame->data[5];
            ESP_LOGD(TAG, "max_cell_volt: %d, max_cell_index: %d", bat_data.bms_data[index].max_cell_volt, bat_data.bms_data[index].max_cell_index);
            ESP_LOGD(TAG, "min_cell_volt: %d, min_cell_index: %d", bat_data.bms_data[index].min_cell_volt, bat_data.bms_data[index].min_cell_index);
        break;

        case 0x61:
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].max_temp_value = (can_frame->data[0] << 0) | (can_frame->data[1] << 8);
            bat_data.bms_data[index].min_temp_value = (can_frame->data[2] << 0) | (can_frame->data[3] << 8);
            bat_data.bms_data[index].max_temp_index = can_frame->data[4];
            bat_data.bms_data[index].min_temp_index = can_frame->data[5];
            bat_data.bms_data[index].ntc_Group[4] = can_frame->data[6]; // NTC5
            bat_data.bms_data[index].ntc_Group[5] = can_frame->data[7]; // NTC6
            if ( bat_data.bms_data[index].ntc_Group[5] != 0 ) {
                bat_data.bms_data[index].total_ntc_num = 6;
            } else if ( bat_data.bms_data[index].ntc_Group[4] != 0 ) {
                bat_data.bms_data[index].total_ntc_num = 5;
            }
            ESP_LOGD(TAG, "max_cell_volt: %d, max_cell_index: %d", bat_data.bms_data[index].max_temp_value, bat_data.bms_data[index].max_temp_index);
            ESP_LOGD(TAG, "min_cell_volt: %d, min_cell_index: %d", bat_data.bms_data[index].min_temp_value, bat_data.bms_data[index].min_temp_index);
        break;
        
        case 0xFA: // BMS reboot Ack
            if (index >= MAX_BMS_NUM) {
                break;
            }
            bat_data.bms_data[index].timeout = BAT_TIMEOUT;
            if (can_frame->data[0] == 0xA0)
                bat_data.bms_data[index].reboot_rsp = 1;
        break;
        
        case 0xFF:
            if(can_frame->identifier == CAN_ID_INV_STATE){
                // 存在别的主机
            }
        break;

        default: break;
    }
}

/*
0x0CF501FE
给BMS更新系统时间
*/
 void bms_ann_now_time(void) 
{
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
    time_t now;
    struct tm tm;
    time(&now); // 当前系统时间
    localtime_r(&now, &tm); // 换算成日期
	canid.all = CAN_ID_NOW_TIME;
    can_ctrl.data[0] = (tm.tm_year - 100) % 100;
    can_ctrl.data[1] = tm.tm_mon + 1;
    can_ctrl.data[2] = tm.tm_mday;
    can_ctrl.data[3] = tm.tm_hour;
    can_ctrl.data[4] = tm.tm_min;
    can_ctrl.data[5] = tm.tm_sec;
    can_ctrl.data[6] = 0; // 保留
    can_ctrl.data[7] = 0; // 保留
	/* 发送单帧CAN数据 */
	CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
}

#if 0
/*
0x0CF501FD
PCS->BMS
*/
void Can_inv_send_state2(void) 
{
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
	
    uint16_t pv_input_power = top_modbus_rd.Dcdc[0].mod_reg15500_DCDC_info.DC1Power; 
	uint16_t dc_output_power = top_modbus_rd.Dcdc[0].mod_reg15500_DCDC_info.DC3Power; 
	uint8_t bat_chg_enable = top_modbus_rd.Dcdc[0].mod_reg15600_DCDC_set.DCDC_setings.DC_sw; 
	canid.all = CAN_ID_INV_PV;
    can_ctrl.data[0] = pv_input_power&0xFF;   // 充电功率L位
    can_ctrl.data[1] = pv_input_power>>8;     // 充电功率H位
//    can_ctrl.data[2] = dc_output_power&0xFF;  // 放电功率L位
//    can_ctrl.data[3] = dc_output_power>>8;    // 放电功率H位
    can_ctrl.data[2] = 0;  // 放电功率L位
    can_ctrl.data[3] = 0;    // 放电功率H位

    can_ctrl.data[4] = bat_chg_enable;         // 启动充电使能
    can_ctrl.data[5] = 0;               // 并机口使能
    can_ctrl.data[6] = 0;               // 预留
    can_ctrl.data[7] = 0;               // 预留

	ESP_LOGI(TAG,"bat_chg_power is %d,bat_chg_enable is %d ",pv_input_power,bat_chg_enable);
	/* 发送单帧CAN数据 */
	CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
}

// 补丁：PV在线突变为0时，连续5次才认为PV离线
static uint8_t check_pv_flag(uint8_t pv_flag) {
   static uint8_t count = 0;
   static uint8_t last_flag = 0;
   static uint8_t ret_flag = 0;
   if(!is_selfcheck_finish()){
       last_flag = 1;
       return 1; // 上电默认PV在线否则会导致断充
   }
   if(pv_flag != last_flag){
       count = 0;
       last_flag = pv_flag;
   } else {
       count++;
   }
   if(count >= 5){
       ret_flag = pv_flag;
   }
   return ret_flag;

}

/*
0x0CF501FF
PCS->BMS
*/
 void Can_inv_send_state(void) //bms_ann_inv_state
{
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
	uint8_t pv_flag = top_modbus_rd.Dcdc[reals.Addr_can_self].mod_reg15500_DCDC_info.Dev_info.PV_online;//PV在线	
	uint8_t pv_work_flag = top_modbus_rd.Dcdc[reals.Addr_can_self].mod_reg15500_DCDC_info.Dev_info.DCDC_work_state;//PV工作状态

	canid.all = CAN_ID_INV_STATE;
	
    can_ctrl.data[0] = 0; 			 // 电网在线状态: 0不在线，1在线
    can_ctrl.data[1] = pv_flag;		 // PV在线状态: 0不在线，1在线
    can_ctrl.data[2] = 0; 			// 负载DC在线 : 0不在线，1在线
    can_ctrl.data[3] = 0; 			// 负载AC在线 : 0不在线，1在线
    can_ctrl.data[4] = bat_stop_cmd; // APP关机指令 : 0不在线，1在线
    can_ctrl.data[5] = 8; 			// 逆变型号 DCDC 1~8
    can_ctrl.data[6] = 0; 			// 电网工作状态 : 0不在线，1在线
    can_ctrl.data[7] = pv_work_flag; // PV工作状态

    if(strcmp(bat_data.bms_data[0].type, "B230") == 0)
	{   // 避免B230关机需要额外处理
        if(bat_stop_cmd != 1)
		{  // B230不允许关机
           can_ctrl.data[1] = 1; // PV在线
           can_ctrl.data[3] = 1; // 负载AC在线
        }
    } 
    // else if(mic_inv_data.dc_data.reg0100.dc_total_power > 10){
    //     can_msg->data[3] = 1; // 负载AC在线
    // }

	/* 发送单帧CAN数据 */
	if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
}

/*0x0CF501FB 未使用*/
static void bms_ann_master_inv(void) {
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};	
	
	canid.all = CAN_ID_INV_MASTER;
    canid.bit.dst = DCDC_CAN_ADDR; // 该位用源地址
    can_ctrl.data[0] = 0; // 电网在线
    can_ctrl.data[1] = (g_self_data.mod_reg15500_DCDC_info.line_sate.DC1_EN == 0) ? 0 : 1; // PV在线
    can_ctrl.data[2] = (g_self_data.mod_reg15500_DCDC_info.line_sate.DC5_EN == 0) ? 0 : 1;; // DC负载在线
    can_ctrl.data[3] = 0; // AC负载在线
    can_ctrl.data[4] = 0;
    can_ctrl.data[5] = 0;
    can_ctrl.data[6] = 0;
    can_ctrl.data[7] = 0;
	/* 发送单帧CAN数据 */
	if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t))) //fail
	{
	//		return -1;
	}	
}

#endif

/*0x0CF501FC 未使用*/
static void bms_ann_addr_ctrl(void) {
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};	
	
	canid.all = CAN_ID_ADDR_CTRL;
    can_ctrl.data[0] = MAX_BMS_NUM; // 地址数量最大支持4个
    can_ctrl.data[1] = 0; // 地址起始0
    can_ctrl.data[2] = 1; // 地址结束1
    can_ctrl.data[3] = 0; // 本身不带电池包
    can_ctrl.data[4] = 0;
    can_ctrl.data[5] = 0;
    can_ctrl.data[6] = 0;
    can_ctrl.data[7] = 0;
	/* 发送单帧CAN数据 */
	if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t))) //fail
	{
	//		return -1;
	}	
}

/*0x0CF50137*/
static void bms_ann_aging_cmd(void) 
{
    if(strcmp(bat_data.bms_data[0].type, "B230") != 0){ // 仅B230需要老化指令开管
        return;
    }
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
	
	canid.all = CAN_ID_AGING_CMD;
	
    can_ctrl.data[0] = 0xAA;
    can_ctrl.data[1] = 0;
    can_ctrl.data[2] = 0;
    can_ctrl.data[3] = 0;
    can_ctrl.data[4] = 0;
    can_ctrl.data[5] = 0;
    can_ctrl.data[6] = 0;
    can_ctrl.data[7] = 0;
	/* 发送单帧CAN数据 */
	if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t))) //fail
	{
	//		return -1;
	}	
}

/**
 * @brief 获取所有电池包的总充电或放电能量
 *
 * @param[in] is_chg  true 获取总充电能量，false 获取总放电能量
 * @return 累加后的总能量值
 */
uint32_t bms_energy_data_get(bool is_chg)
{
    uint32_t energy_all = 0;
    if (is_chg) {
        for (uint8_t i = 0; i < bat_data.all_data.bat_num; i++) {
            if (bat_data.bms_data[i].valid && bat_data.bms_data[i].total_chg_energy != 0) {
                energy_all += bat_data.bms_data[i].total_chg_energy;
            }
        }
    } else {
        for (uint8_t i = 0; i < bat_data.all_data.bat_num; i++) {
            if (bat_data.bms_data[i].valid && bat_data.bms_data[i].total_dsg_energy != 0) {
                energy_all += bat_data.bms_data[i].total_dsg_energy;
            }
        }
    }
    return energy_all;
}

/**
 * @brief 更新电池主包（Pack）汇总数据到Modbus寄存器
 *
 * 将 bat_data.all_data 的各项汇总信息同步到 Pack_Modbus_Data->mod_reg06000_Pack_sum，
 * 包括电池数量、在线状态、电压电流、SOC、SOH、温度、状态等。
 */
static void update_pack_summary_to_modbus_register(void)
{
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
    MOD_STRUCT_Pack *Pack_Modbus_Data = &top_modbus_rd.Pack[PACK_MAX_NUM];
    bms_data_t *Pack_Can_Data = &bat_data.all_data;

    /*Modbus 100*/
    Inv_Modbus_Data->mod_reg00100_AppPage1.total_voltage = Pack_Can_Data->total_volt / 10;  // 100
    Inv_Modbus_Data->mod_reg00100_AppPage1.total_current = Pack_Can_Data->total_current;  // 101
    Inv_Modbus_Data->mod_reg00100_AppPage1.soc = Pack_Can_Data->soc;  // 102
    Inv_Modbus_Data->mod_reg00100_AppPage1.chg_status = Pack_Can_Data->charging_state;  // 103
    // 如果正在充电则发chg_full_time，否则发dsg_empty_time
    // BMS确认：当前协议充放电时间单位是0.1h，转成Modbus协议需转成分钟
    if(Pack_Can_Data->charging_state == 0x01) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.chg_full_time = ((uint32_t)(Pack_Can_Data->chg_full_time * 6) > 0xFFFF) ? 0xFFFF : (Pack_Can_Data->chg_full_time * 6);  // 104
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.chg_full_time = ((uint32_t)(Pack_Can_Data->dsg_empty_time * 6) > 0xFFFF) ? 0xFFFF : (Pack_Can_Data->dsg_empty_time * 6);  // 104
    }
    Inv_Modbus_Data->mod_reg00100_AppPage1.dsg_empty_time = ((uint32_t)(Pack_Can_Data->dsg_empty_time * 6) > 0xFFFF) ? 0xFFFF : (Pack_Can_Data->dsg_empty_time * 6);  // 105
    Inv_Modbus_Data->mod_reg00100_AppPage1.aging_status.all = bms_aging_status.all;  // 106
    Inv_Modbus_Data->mod_reg00100_AppPage1.PackCnts = Pack_Can_Data->bat_num;  // 107
    Inv_Modbus_Data->mod_reg00100_AppPage1.PackOnline = Pack_Can_Data->online_bit;  // 108
    
    /*Modbus 6000*/
    Pack_Modbus_Data->mod_reg06000_Pack_sum.is_high_volt = 1;   // 6000 (1：低压；0：高压；)
    Pack_Modbus_Data->mod_reg06000_Pack_sum.pack_cnt = Pack_Can_Data->bat_num; // 6001
    Pack_Modbus_Data->mod_reg06000_Pack_sum.online = Pack_Can_Data->online_bit; // 6002
    Pack_Modbus_Data->mod_reg06000_Pack_sum.total_voltage = Pack_Can_Data->total_volt / 10; // 6003
    Pack_Modbus_Data->mod_reg06000_Pack_sum.total_current = Pack_Can_Data->total_curr; // 6004
    Pack_Modbus_Data->mod_reg06000_Pack_sum.soc = Pack_Can_Data->soc; // 6005

    if (Pack_Can_Data->soh != 100 && Pack_Can_Data->soh != 0) {
        Pack_Modbus_Data->mod_reg06000_Pack_sum.soh = Pack_Can_Data->soh; // 6006
    } else {
        /* 部分电池包不上报系统总SOH，计算所有电池包SOH的平均值 */
        uint16_t soh = 0;
        uint8_t soh_num = 0;
        for (uint8_t i = 0; i < Pack_Can_Data->bat_num; i++) {
            if (bat_data.bms_data[i].valid && bat_data.bms_data[i].soh != 0) {
                soh += bat_data.bms_data[i].soh;
                soh_num++;
            }
        }
        if (soh_num > 0) {
            Pack_Modbus_Data->mod_reg06000_Pack_sum.soh = soh / soh_num; // 6006
        } else {
            Pack_Modbus_Data->mod_reg06000_Pack_sum.soh = Pack_Can_Data->soh; // 6006
        }
    }

    Pack_Modbus_Data->mod_reg06000_Pack_sum.avg_temp = Pack_Can_Data->avg_temp; // 6007
    Pack_Modbus_Data->mod_reg06000_Pack_sum.work_status = Pack_Can_Data->run_state; // 6008
    Pack_Modbus_Data->mod_reg06000_Pack_sum.chg_status = Pack_Can_Data->charging_state; // 6009
    Pack_Modbus_Data->mod_reg06000_Pack_sum.max_chg_voltage = Pack_Can_Data->chg_max_volt / 10; // 6010
    Pack_Modbus_Data->mod_reg06000_Pack_sum.max_chg_current = Pack_Can_Data->chg_max_curr; // 6011
    Pack_Modbus_Data->mod_reg06000_Pack_sum.max_dsg_current = Pack_Can_Data->dsg_max_curr; // 6012
    Pack_Modbus_Data->mod_reg06000_Pack_sum.status1.all = Pack_Can_Data->chg_ctrl.all; // 6013
    Pack_Modbus_Data->mod_reg06000_Pack_sum.status2.all = Pack_Can_Data->state.all; // 6014
    Pack_Modbus_Data->mod_reg06000_Pack_sum.cap_online = Pack_Can_Data->sync_bit; // 6016
    Pack_Modbus_Data->mod_reg06000_Pack_sum.chg_full_time = Pack_Can_Data->chg_full_time; // 6017
    Pack_Modbus_Data->mod_reg06000_Pack_sum.dsg_empty_time = Pack_Can_Data->dsg_empty_time; // 6018

#if PACK_MAX_NUM == 1
	Pack_Modbus_Data->mod_reg06000_Pack_sum.total_chg_energy = bms_energy_data_get(true) / 10;  // 6019-6020
	Pack_Modbus_Data->mod_reg06000_Pack_sum.total_dsg_energy = bms_energy_data_get(false) / 10;  // 6021-6022
#else
    // 考虑到电池包变动，使用逆变器上报值
	Pack_Modbus_Data->mod_reg06000_Pack_sum.total_chg_energy = Inv_Modbus_Data->mod_reg00100_AppPage1.PackTotalChgEnergy;  // 6019-6020
	Pack_Modbus_Data->mod_reg06000_Pack_sum.total_dsg_energy = Inv_Modbus_Data->mod_reg00100_AppPage1.PackTotalDsgEnergy;  // 6021-6022    
#endif
    Pack_Modbus_Data->mod_reg06000_Pack_sum.all_pack_alarm1 = Pack_Can_Data->show_protect3.val; // 6029
    Pack_Modbus_Data->mod_reg06000_Pack_sum.all_pack_alarm2 = Pack_Can_Data->show_protect4.val; // 6030
    Pack_Modbus_Data->mod_reg06000_Pack_sum.PackProtect = ((uint32_t)Pack_Can_Data->show_protect2.all<<16) | (Pack_Can_Data->show_protect1.all);// 6036~6037
    Pack_Modbus_Data->mod_reg06000_Pack_sum.PackSysErr[0] = Pack_Can_Data->show_fault1.all;// 6038
    Pack_Modbus_Data->mod_reg06000_Pack_sum.PackSysErr[1] = Pack_Can_Data->show_fault2.all;// 6039
    Pack_Modbus_Data->mod_reg06000_Pack_sum.PackSysErr[2] = Pack_Can_Data->show_fault3.all;// 6040
    Pack_Modbus_Data->mod_reg06000_Pack_sum.PackAlarm = Pack_Can_Data->show_alarm1.all;// 6041

    /*6172 B300,B300S不支持并包升级*/
    if ( bat_data.state.bit.is_busy ) {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1); // 6172
    } else {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 1);
    }

    /* 6172 判断为混包时无法升级*/
    if ( bat_data.state.bit.is_mix_err ) {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 2);
    } else {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 2);
    }
}

/**
 * @brief 更新所有单个电池包数据到Modbus寄存器
 *
 * 遍历所有电池包，将每个包的详细数据同步到对应的Modbus寄存器区（如6100、6300等）。
 * 包括基本信息、状态、能量、保护、故障、温度、电芯电压、NTC等。
 */
static void update_single_pack_to_modbus_register(void)
{	
	for (int j = 0;  j < MAX_BMS_NUM; ++j)
	{
	    if(!bat_data.bms_data[j].valid) continue;
            
		bms_data_t *Pack_Can_Data = &bat_data.bms_data[j];
		MOD_STRUCT_Pack *Pack_Modbus_Data = &top_modbus_rd.Pack[j];
        
        /* modbus 6100 */
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_id = get_addr_trace(j);   // 6100
#ifdef INTERNAL_PACK_ENABLE 
        if ( j == 0 ) {
            memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each.type_ascii, (uint8_t *)&dev_factory.dev_type, sizeof(dev_factory.dev_type));// 6101~6106
            Pack_Modbus_Data->mod_reg06100_Pack_each.sn_code = dev_factory.dev_sn; //6107~6110
        } else {
            memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each.type_ascii, (uint8_t *)&Pack_Can_Data->type, sizeof(Pack_Can_Data->type));// 6101~6106
            Pack_Modbus_Data->mod_reg06100_Pack_each.sn_code = (Pack_Can_Data->sn_code > 0) ? Pack_Can_Data->sn_code : dev_factory.dev_sn; //6107~6110
        }
#else
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each.type_ascii, (uint8_t *)&Pack_Can_Data->type, sizeof(Pack_Can_Data->type));// 6101~6106
        Pack_Modbus_Data->mod_reg06100_Pack_each.sn_code = (Pack_Can_Data->sn_code > 0) ? Pack_Can_Data->sn_code : dev_factory.dev_sn; //6107~6110 (部分电池包不含SN，需要伪装)
#endif        
        Pack_Modbus_Data->mod_reg06100_Pack_each.total_voltage = Pack_Can_Data->total_voltage/10;// 6111 
        Pack_Modbus_Data->mod_reg06100_Pack_each.total_current = Pack_Can_Data->total_current;// 6112
        Pack_Modbus_Data->mod_reg06100_Pack_each.soc = Pack_Can_Data->soc;// 6113
        Pack_Modbus_Data->mod_reg06100_Pack_each.soh = Pack_Can_Data->soh;// 6114
        Pack_Modbus_Data->mod_reg06100_Pack_each.avg_temp = Pack_Can_Data->avg_temp;// 6115
        Pack_Modbus_Data->mod_reg06100_Pack_each.min_cell_voltage = Pack_Can_Data->min_cell_volt;// 6116
        Pack_Modbus_Data->mod_reg06100_Pack_each.max_cell_voltage = Pack_Can_Data->max_cell_volt;// 6117
        Pack_Modbus_Data->mod_reg06100_Pack_each.min_cell_index = Pack_Can_Data->min_cell_index;// 6118
        Pack_Modbus_Data->mod_reg06100_Pack_each.max_cell_index = Pack_Can_Data->max_cell_index;// 6119
        Pack_Modbus_Data->mod_reg06100_Pack_each.min_temp_value = Pack_Can_Data->min_temp_value;// 6120
        Pack_Modbus_Data->mod_reg06100_Pack_each.max_temp_value = Pack_Can_Data->max_temp_value;// 6121
        Pack_Modbus_Data->mod_reg06100_Pack_each.min_temp_index = Pack_Can_Data->min_temp_index;// 6122
        Pack_Modbus_Data->mod_reg06100_Pack_each.max_temp_index = Pack_Can_Data->max_temp_index;// 6123
        Pack_Modbus_Data->mod_reg06100_Pack_each.work_status = Pack_Can_Data->run_state;// 6124
        Pack_Modbus_Data->mod_reg06100_Pack_each.chg_status = Pack_Can_Data->charging_state;// 6125
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_online = Pack_Can_Data->valid;// 6127 备注：单包用valid，汇总用online_bit
        // Pack_Modbus_Data->mod_reg06100_Pack_each.relay = Pack_Can_Data->relay;// 6128
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_cap_online =  Pack_Can_Data->sync;// 6129 备注：单包用sync，汇总用sync_bit
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_canbus_error = !Pack_Can_Data->valid; // 6130
        Pack_Modbus_Data->mod_reg06100_Pack_each.capacity = Pack_Can_Data->standard_capacity; // 6131~6132
        Pack_Modbus_Data->mod_reg06100_Pack_each.total_chg_energy =  Pack_Can_Data->total_chg_energy*10;// 6133~6134
        Pack_Modbus_Data->mod_reg06100_Pack_each.total_dsg_energy =  Pack_Can_Data->total_dsg_energy*10;// 6135~6136
        Pack_Modbus_Data->mod_reg06100_Pack_each.once_chg_energy =  Pack_Can_Data->curr_chg_energy;// 6137~6138
        Pack_Modbus_Data->mod_reg06100_Pack_each.once_dsg_energy =  Pack_Can_Data->curr_dsg_energy;// 6139~6140
        // Pack_Modbus_Data->mod_reg06100_Pack_each.sir_value = 0;// 6141
        Pack_Modbus_Data->mod_reg06100_Pack_each.inner_value = Pack_Can_Data->PackInner;// 6142
        Pack_Modbus_Data->mod_reg06100_Pack_each.cycle_count = Pack_Can_Data->used_cycle;// 6143
        Pack_Modbus_Data->mod_reg06100_Pack_each.protect = ((uint32_t)Pack_Can_Data->show_protect2.all<<16) | (Pack_Can_Data->show_protect1.all);// 6144~6145
        Pack_Modbus_Data->mod_reg06100_Pack_each.SysErr[0] = Pack_Can_Data->show_fault1.all;// 6146
        Pack_Modbus_Data->mod_reg06100_Pack_each.SysErr[1] = Pack_Can_Data->show_fault2.all;// 6147
        Pack_Modbus_Data->mod_reg06100_Pack_each.SysErr[2] = Pack_Can_Data->show_fault3.all;// 6148
        Pack_Modbus_Data->mod_reg06100_Pack_each.alarm1 = Pack_Can_Data->show_alarm1.all;// 6149
        Pack_Modbus_Data->mod_reg06100_Pack_each.chg_full_time = Pack_Can_Data->chg_full_time;// 6150
        Pack_Modbus_Data->mod_reg06100_Pack_each.dsg_empty_time = Pack_Can_Data->dsg_empty_time;// 6151
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_total_cell = Pack_Can_Data->total_cell_num;// 6152
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_total_ntc = Pack_Can_Data->total_ntc_num;// 6153
        // 6154~6156
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_type = Pack_Can_Data->pack_type_num;// 6157
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_outsum_voltage = Pack_Can_Data->pack_volt;// 6158
        // 6159
        Pack_Modbus_Data->mod_reg06100_Pack_each.allow_max_chg_current = Pack_Can_Data->chg_max_curr;// 6160
        Pack_Modbus_Data->mod_reg06100_Pack_each.allow_max_dsg_current = Pack_Can_Data->dsg_max_curr;// 6161
        Pack_Modbus_Data->mod_reg06100_Pack_each.alarm2 = Pack_Can_Data->show_protect3.val;// 6164
        Pack_Modbus_Data->mod_reg06100_Pack_each.alarm3 = Pack_Can_Data->show_protect4.val;// 6165

        if ( bat_data.state.bit.is_busy ) {
            Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1); // 6172
        } else {
            Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 1);
        }
        if ( bat_data.state.bit.is_mix_err ) {
            Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 2);
        } else {
            Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 2);
        }
        
        uint8_t ver_num = 0;
        for(uint8_t i = 0; i < 2; i++) {
            // 6174~6203
            if(Pack_Can_Data->version[i] != 0 && Pack_Can_Data->mcu_type[i] != 0) {
                Pack_Modbus_Data->mod_reg06100_Pack_each.soft[ver_num].type= Pack_Can_Data->mcu_type[i];
                Pack_Modbus_Data->mod_reg06100_Pack_each.soft[ver_num].version = Pack_Can_Data->version[i];
                ver_num++;
            }
        }
        Pack_Modbus_Data->mod_reg06100_Pack_each.software_total = ver_num;  // 6173

		/* modbus 6300 */
        Pack_Modbus_Data->mod_reg06300_Pack_cell.PackCellCnt = Pack_Can_Data->total_cell_num;// 6300
		Pack_Modbus_Data->mod_reg06300_Pack_cell.PackNTCCnt = Pack_Can_Data->total_ntc_num;// 6301
        for (int j = 0; j < Pack_Modbus_Data->mod_reg06300_Pack_cell.PackCellCnt; j++) {
            Pack_Modbus_Data->mod_reg06300_Pack_cell.payload[j*2] = Pack_Can_Data->cell_volt[j].all&0xFF;
            Pack_Modbus_Data->mod_reg06300_Pack_cell.payload[j*2+1] = Pack_Can_Data->cell_volt[j].all>>8;
            ESP_LOGD(__func__,"mod_reg06300_Pack_cell.payload[%d] is %d ,payload[%d] is %d",j*2,Pack_Modbus_Data->mod_reg06300_Pack_cell.payload[j*2],j*2+1,
				Pack_Modbus_Data->mod_reg06300_Pack_cell.payload[j*2+1]);
        }
		uint8_t tempdata = Pack_Modbus_Data->mod_reg06300_Pack_cell.PackCellCnt*2;
        for (int j = 0; j < Pack_Modbus_Data->mod_reg06300_Pack_cell.PackNTCCnt; j++) {
            Pack_Modbus_Data->mod_reg06300_Pack_cell.payload[tempdata+j] = Pack_Can_Data->ntc_Group[j]&0xFF;
            ESP_LOGD(__func__,"mod_reg06300_Pack_cell.payload[%d] is %d ,ntc_Group[%d] is %d",tempdata+j,Pack_Modbus_Data->mod_reg06300_Pack_cell.payload[tempdata+j],
            j,Pack_Can_Data->ntc_Group[j]);
        }
	}
}

/**
 * @brief 统计所有电池包各类型/前缀的最小版本号，并写入汇总Modbus寄存器
 *
 * 优化说明：
 * - 只统计有效的type和version
 * - 支持新旧料号前缀区分
 * - 结果最多记录10组，超出不再统计
 * - 写入主包寄存器的soft数组及software_total
 */
static void update_pack_min_ver(void)
{
    MOD_STRUCT_Pack *Pack_Modbus_Data = &top_modbus_rd.Pack[PACK_MAX_NUM];
    min_version_t min_versions[MAX_MIN_VER_GROUP] = {0};

    /*Modbus 6100*/
    for(int8_t i = bat_data.all_data.bat_num - 1; i >= 0; i--){ // 从后往前遍历，找到第一个在线的包，优先使用从包，作为OTA展示
        if(bat_data.bms_data[i].valid == 1){
            memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each.type_ascii, (uint8_t *)&bat_data.bms_data[i].type, 12);// 6101~6106
            Pack_Modbus_Data->mod_reg06100_Pack_each.sn_code = bat_data.bms_data[i].sn_code;
            break;
        }
    }

    // 统计所有电池包的最小版本
    for (uint8_t i = 0; i < bat_data.all_data.bat_num; i++) {
        bms_data_t *Pack_Can_Data = &bat_data.bms_data[i];
        for (uint8_t n = 0; n < 2; n++) {
            uint16_t type = Pack_Can_Data->mcu_type[n];
            uint32_t version = Pack_Can_Data->version[n];
            if (type == 0 || version == 0) continue;

            uint32_t version_prefix = (version > 1000000) ? (version / 10000) : (version / 100);

            // 查找是否已记录
            int found = 0;
            for (int j = 0; j < MAX_MIN_VER_GROUP; j++) {
                if (min_versions[j].valid &&
                    min_versions[j].type == type &&
                    min_versions[j].version_prefix == version_prefix) {
                    if (version < min_versions[j].min_version) {
                        min_versions[j].min_version = version;
                    }
                    found = 1;
                    break;
                }
            }

            // 新记录
            if (!found) {
                for (int j = 0; j < MAX_MIN_VER_GROUP; j++) {
                    if (!min_versions[j].valid) {
                        min_versions[j].type = type;
                        min_versions[j].version_prefix = version_prefix;
                        min_versions[j].min_version = version;
                        min_versions[j].valid = 1;
                        break;
                    }
                }
            }
        }
    }

    // 清空主包寄存器
    for (int i = 0; i < MAX_MIN_VER_GROUP; i++) {
        Pack_Modbus_Data->mod_reg06100_Pack_each.soft[i].type = 0;
        Pack_Modbus_Data->mod_reg06100_Pack_each.soft[i].version = 0;
    }

    // 写入结果
    int out_cnt = 0;
    for (int i = 0; i < MAX_MIN_VER_GROUP; i++) {
        if (min_versions[i].valid) {
            Pack_Modbus_Data->mod_reg06100_Pack_each.soft[out_cnt].type = min_versions[i].type;
            Pack_Modbus_Data->mod_reg06100_Pack_each.soft[out_cnt].version = min_versions[i].min_version;
            out_cnt++;
            if (out_cnt >= MAX_MIN_VER_GROUP) break;
        }
    }
    Pack_Modbus_Data->mod_reg06100_Pack_each.software_total = out_cnt;

    /*6172 B300,B300S不支持并包升级*/
    if ( bat_data.state.bit.is_busy ) {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1); // 6172
    } else {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 1);
    }

    /* 6172 判断为混包时无法升级*/
    if ( bat_data.state.bit.is_mix_err ) {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 2);
    } else {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 2);
    }
}

/*------------------------------------------------------------------------------
 Function: update_bat_data_to_modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      更新电池包数据
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void update_bat_data_to_modbus(void)
{
    /*组合 BMS 数据并更新状态标志*/
    bms_data_combine();

    /*更新所有单个电池包数据到Modbus寄存器*/
    update_single_pack_to_modbus_register();

#ifdef INTERNAL_PACK_ENABLE
    /*存在主包，汇总段使用主包数据*/
    memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06100_Pack_each, (uint8_t *)&top_modbus_rd.Pack[0].mod_reg06100_Pack_each, sizeof(MOD_STRUCT_reg06100));
    memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06300_Pack_cell, (uint8_t *)&top_modbus_rd.Pack[0].mod_reg06300_Pack_cell, sizeof(MOD_STRUCT_reg06300));
#endif
    
    /* 更新电池主包（Pack）汇总数据到Modbus寄存器 */
    update_pack_summary_to_modbus_register();
}

/**
 * @brief 发送B230电池包BOOT指令CAN帧
 *
 * 构造BOOT命令ID和数据，发送单帧CAN数据，无需返回结果。
 */
static void bms_check_b230_boot_cmd(void) {
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
	
	canid.all = CAN_ID_B230_BOOT;
	
	can_ctrl.data[0]  =0;
	can_ctrl.data[1]  =0;
	can_ctrl.data[2]  =0;
	can_ctrl.data[3]  =0;

	can_ctrl.data[4]  =0;
	can_ctrl.data[5]  =0;
	can_ctrl.data[6]  =0;
	can_ctrl.data[7]  =0;

	/* 发送单帧CAN数据 */
	CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));

    return; // 返回结果
}

// 超时CAN总线没有数据认为b230在boot
static void update_b230_boot_data(void){
    static uint32_t bms_ver = 0;
    if (bms_ver/100 != 101100 && bms_ver/100 != 101800){
        // if(iot_read_flash(B230_VERSION, (uint8_t *)&bms_ver, 4) != ESP_OK  || bms_ver/100 != 1011){
        //     bms_ver = 101800;
        // }
    }
    ESP_LOGI(__func__, "b230 is running boot");
    bat_data.bms_data[0].sn_code = dev_factory.dev_sn;
    strncpy(bat_data.bms_data[0].type, "B230", 12);
    bat_data.bms_data[0].mcu_num = 1;
    bat_data.bms_data[0].mcu_type[0] = DEVICE_BMS ;//DEVICE_PACK_BMS;
    bat_data.bms_data[0].version[0] = bms_ver/100*100; // 最后两位为0
    bat_data.bms_data[0].mcu_type[1] = 0;
    bat_data.bms_data[0].version[1] = 0;
    bat_data.bms_data[0].valid = 1;
    bat_data.bms_data[0].timeout = BAT_TIMEOUT;
    bat_data.bms_data[0].soc = 0;
    bat_addr_trace.addr_pool[0] = 1; // 赋予地址
}

/*
判断接收到B230 boot CAN报文
1-收到
*/
static int check_b230_boot_ack(const twai_message_t *rx_frame){
    can_id_t_alpha canId;
    int result = -1; // 默认返回值
    canId.id = rx_frame->identifier;
    if (CAN_ID_ALPHA_FLAG == canId.bit.fix) 
	{ // 检查固定位
        if(canId.id == CAN_ID_B230_BOOT)
		{
            result = 1; // 认为b230已经进入boot
        } 
		else if((canId.id >= CAN_ID_ADDR_CTRL && canId.id <= CAN_ID_INV_STATE) || canId.id == CAN_ID_AGING_CMD)
		{
            result = 0; // 忽略自己发的帧
        }
    } 
	else 
	{
        result = 0; // 噪音忽略
    }
    return result;
}

/**
 * @brief 更新BMS故障与保护状态
 *
 * 1. 汇总所有电池包的保护、故障、停充/停放电状态；
 * 2. 检查短路和预充故障，若任一电池包出现则全局标记；
 * 3. 支持多包场景下的充放电联动控制。
 */
static void bms_fault_update(void) {
    uint16_t temp_protect1 = 0;
    uint16_t temp_protect2 = 0;
    uint16_t temp_protect3 = 0;
    uint16_t temp_protect4 = 0;
    uint16_t temp_fault1 = 0;
    to_state_t temp_to_state[MAX_BMS_NUM] = {0};
    to_state_t temp_combin_state = {0};
    temp_combin_state.bit.stop_chg = 1;
    temp_combin_state.bit.stop_dsg = 1;

    // 添加短路和预充故障检测变量
    bool any_short_circuit = false;
    bool any_precharge_fault = false;

    for (uint8_t i = 0; i < MAX_BMS_NUM; i++) {
        if (bat_data.bms_data[i].valid == 1) {

            // 检测短路和预充故障
            if (bat_data.bms_data[i].protect1.bit.short_current) {
                any_short_circuit = true;
            }
            
            if (bat_data.bms_data[i].protect1.bit.pre_chg_error) {
                any_precharge_fault = true;
            }

            // 充电
            bat_data.bms_data[i].show_protect1.bit.cell_ov  = bat_data.bms_data[i].protect1.bit.cell_v_over;  // 单电池过压
            bat_data.bms_data[i].show_protect1.bit.total_ov = bat_data.bms_data[i].protect1.bit.tot_v_over;   // 总压过压
            bat_data.bms_data[i].show_protect1.bit.chg_over_temp  = bat_data.bms_data[i].protect1.bit.chg_over_temp;  // 充电过温
            bat_data.bms_data[i].show_protect1.bit.chg_under_temp = bat_data.bms_data[i].protect1.bit.chg_under_temp; // 充电低温
            bat_data.bms_data[i].show_protect1.bit.chg_over_curr1 = bat_data.bms_data[i].protect2.bit.chg_over_curr1; // 充电过流1
            bat_data.bms_data[i].show_protect1.bit.chg_over_curr2 = bat_data.bms_data[i].protect2.bit.chg_over_curr2; // 充电过流2
            bat_data.bms_data[i].show_protect1.bit.chg_over_curr3 = bat_data.bms_data[i].protect2.bit.chg_over_curr3; // 充电过流3
            
            // 放电
            bat_data.bms_data[i].show_protect2.bit.cell_uv  = bat_data.bms_data[i].protect1.bit.cell_v_under;     // 单电池低压
            bat_data.bms_data[i].show_protect2.bit.total_uv = bat_data.bms_data[i].protect1.bit.tot_v_under;      // 总压低压
            bat_data.bms_data[i].show_protect2.bit.dsg_over_temp = bat_data.bms_data[i].protect1.bit.dsg_over_temp;   // 放电过温
            bat_data.bms_data[i].show_protect2.bit.dsg_under_temp = bat_data.bms_data[i].protect1.bit.dsg_under_temp; // 放电低温
            bat_data.bms_data[i].show_protect2.bit.dsg_over_curr1 = bat_data.bms_data[i].protect2.bit.dsg_over_curr1; // 放电过流1
            bat_data.bms_data[i].show_protect2.bit.dsg_over_curr2 = bat_data.bms_data[i].protect2.bit.dsg_over_curr2; // 放电过流2
            bat_data.bms_data[i].show_protect2.bit.dsg_over_curr3 = bat_data.bms_data[i].protect2.bit.dsg_over_curr3; // 放电过流3
            
            // 短路
            bat_data.bms_data[i].show_fault1.bit.cluster_short = bat_data.bms_data[i].protect1.bit.short_current; // 短路
            bat_data.bms_data[i].show_fault1.bit.precharge_fault1 = bat_data.bms_data[i].protect1.bit.pre_chg_error; // 预充故障

            // 电池保护状态（表7
            bat_data.bms_data[i].show_protect3.bit.vol_total_high = bat_data.bms_data[i].protect1.bit.tot_v_over;   // 总压过高
            bat_data.bms_data[i].show_protect3.bit.vol_total_low = bat_data.bms_data[i].protect1.bit.tot_v_under;   // 总压过低
            bat_data.bms_data[i].show_protect3.bit.vol_cell_high = bat_data.bms_data[i].protect1.bit.cell_v_over;   // 单体过压
            bat_data.bms_data[i].show_protect3.bit.vol_cell_low = bat_data.bms_data[i].protect1.bit.cell_v_under;   // 单体欠压
            bat_data.bms_data[i].show_protect3.bit.temp_charge_high = bat_data.bms_data[i].protect1.bit.chg_over_temp;   // 充电过温
            bat_data.bms_data[i].show_protect3.bit.temp_charge_low = bat_data.bms_data[i].protect1.bit.chg_under_temp;   // 充电欠温
            bat_data.bms_data[i].show_protect3.bit.temp_discharge_high = bat_data.bms_data[i].protect1.bit.dsg_over_temp;   // 放电过温
            bat_data.bms_data[i].show_protect3.bit.temp_discharge_low = bat_data.bms_data[i].protect1.bit.dsg_under_temp;   // 放电欠温
            bat_data.bms_data[i].show_protect3.bit.curr_charge_over = bat_data.bms_data[i].protect2.bit.chg_over_curr1 
                                                                        | bat_data.bms_data[i].protect2.bit.chg_over_curr2 
                                                                        | bat_data.bms_data[i].protect2.bit.chg_over_curr3 
                                                                        | bat_data.bms_data[i].protect2.bit.chg_over_curr4;   // 充电过流
            bat_data.bms_data[i].show_protect3.bit.curr_discharge_over = bat_data.bms_data[i].protect2.bit.dsg_over_curr1 
                                                                            | bat_data.bms_data[i].protect2.bit.dsg_over_curr2 
                                                                            | bat_data.bms_data[i].protect2.bit.dsg_over_curr3 
                                                                            | bat_data.bms_data[i].protect2.bit.dsg_over_curr4;   // 放电过流
            bat_data.bms_data[i].show_protect3.bit.precharge_fail = bat_data.bms_data[i].protect1.bit.pre_chg_error;   // 预充故障
            bat_data.bms_data[i].show_protect3.bit.short_circuit = bat_data.bms_data[i].protect1.bit.short_current;   // 短路保护
            bat_data.bms_data[i].show_protect3.bit.curr_protect1 = bat_data.bms_data[i].protect1.bit.curr_over_level1;   // 过流保护1
            bat_data.bms_data[i].show_protect3.bit.curr_protect2 = bat_data.bms_data[i].protect1.bit.curr_over_level2;   // 过流保护2
            bat_data.bms_data[i].show_protect4.bit.curr_protect3 = bat_data.bms_data[i].protect1.bit.curr_over_level3;   // 过流保护3
            bat_data.bms_data[i].show_protect4.bit.curr_protect4 = bat_data.bms_data[i].protect1.bit.curr_over_level4;   // 过流保护4
            
            // 汇总
            temp_protect1 |= bat_data.bms_data[i].show_protect1.all;
            temp_protect2 |= bat_data.bms_data[i].show_protect2.all;
            temp_protect3 |= bat_data.bms_data[i].show_protect3.val;
            temp_protect4 |= bat_data.bms_data[i].show_protect4.val;
            temp_fault1 |= bat_data.bms_data[i].show_fault1.all;
            temp_to_state[i].bit.stop_chg = bat_data.bms_data[i].protect1.bit.cell_v_over    |
                                            bat_data.bms_data[i].protect1.bit.tot_v_over     |
                                            bat_data.bms_data[i].protect1.bit.chg_over_temp  |
                                            bat_data.bms_data[i].protect1.bit.chg_under_temp |
                                            bat_data.bms_data[i].protect2.bit.chg_over_curr1 |
                                            bat_data.bms_data[i].protect2.bit.chg_over_curr2 |
                                            bat_data.bms_data[i].protect2.bit.chg_over_curr3 |
                                            bat_data.bms_data[i].protect1.bit.short_current  |
                                            bat_data.bms_data[i].protect1.bit.pre_chg_error;
            temp_to_state[i].bit.stop_dsg = bat_data.bms_data[i].protect1.bit.cell_v_under   |
                                            bat_data.bms_data[i].protect1.bit.tot_v_under    |  
                                            bat_data.bms_data[i].protect1.bit.dsg_over_temp  |  
                                            bat_data.bms_data[i].protect1.bit.dsg_under_temp |
                                            bat_data.bms_data[i].protect2.bit.dsg_over_curr1 |
                                            bat_data.bms_data[i].protect2.bit.dsg_over_curr2 |
                                            bat_data.bms_data[i].protect2.bit.dsg_over_curr3;
            temp_combin_state.bit.stop_chg &= temp_to_state[i].bit.stop_chg;  // 多包时，只有所有包都停止充电才停止充电
            temp_combin_state.bit.stop_dsg &= temp_to_state[i].bit.stop_dsg;  // 多包时，只有所有包都停止放电才停止放电
        }
    }

    // 为系统更新短路和预充故障状态
#if 0    
    // 短路故障处理：任一电池包短路，所有电池包都标记短路
    reals.bat_short_current = any_short_circuit ? 1 : 0;
    
    // 预充故障处理：任一电池包预充故障，所有电池包都标记预充故障
    reals.bat_pre_chg_error = any_precharge_fault ? 1 : 0;   
#endif

    bat_data.all_data.show_protect1.all = temp_protect1;
    bat_data.all_data.show_protect2.all = temp_protect2;
    bat_data.all_data.show_protect3.val = temp_protect3;
    bat_data.all_data.show_protect4.val = temp_protect4;
    bat_data.all_data.show_fault1.all   = temp_fault1;
    bat_data.all_data.state.all         = temp_combin_state.all;
}

/**
 * @brief 判断BMS锁定错误（同步数量与在线数量不一致）
 *
 * @return 1 错误（同步数量与在线数量不一致），0 正常
 */
static uint8_t is_bms_lck_error(void){
    uint8_t sync_count = 0;
    for(uint8_t i = 0; i < 16; i++){
        if(bat_data.all_data.sync_bit & (1<<i)){
            sync_count++;
        }
    }
    if(bat_data.all_data.bat_num != sync_count){
        return 1;
    } else {
        return 0;
    }
}

/*------------------------------------------------------------------------------
 Function: is_b300_series
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断是否为B300系列电池包
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     bool true: 是B300系列, false: 不是B300系列
 */
static bool is_b300_series(uint16_t version_type)
{
    return (version_type == 1014 || version_type == 1024 || 
            version_type == 1021 || version_type == 1047);
}

/*------------------------------------------------------------------------------
 Function: is_b300s_series
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断是否为B300S系列电池包
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     bool true: 是B300S系列, false: 不是B300S系列
 */
static bool is_b300s_series(uint16_t version_type)
{
    return (version_type == 1022);
}

/*------------------------------------------------------------------------------
 Function: is_b300k_series
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断是否为B300K系列电池包
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     bool true: 是B300K系列, false: 不是B300K系列
 */
static bool is_b300k_series(uint16_t version_type)
{
    return (version_type == 1067);
}

/*------------------------------------------------------------------------------
 Function: is_b230_series
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断是否为B230系列电池包
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     bool true: 是B230系列, false: 不是B230系列
 */
static bool is_b230_series(uint16_t version_type)
{
    return (version_type == 1018 || version_type == 1011);
}

/*------------------------------------------------------------------------------
 Function: is_b210_series
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断是否为B210系列电池包
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     bool true: 是B210系列, false: 不是B210系列
 */
static bool is_b210_series(uint16_t version_type)
{
    return (version_type == 1036 || version_type == 1064);
}

/*------------------------------------------------------------------------------
 Function: is_power5_series
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断是否为POWER5内置电池包
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     bool true: 是POWER5, false: 不是POWER5
 */
static bool is_power5_series(uint16_t version_type)
{
    return (version_type == 1097);
}

/*------------------------------------------------------------------------------
 Function: get_bms_series_name
 -----------------------------------------------------------------------------*/
/**
 * @brief      根据固件版本号获取电池包系列名称
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     const char* 电池包系列名称字符串
 */
const char* get_bms_series_name(uint16_t version_type)
{
    if (is_b300_series(version_type)) {
        return SN_TYPE_B300_ASCII;
    } else if (is_b300s_series(version_type)) {
        return SN_TYPE_B300S_ASCII;
    } else if (is_b300k_series(version_type)) {
        return SN_TYPE_B300K_ASCII;
    } else if (is_b230_series(version_type)) {
        return SN_TYPE_B230_ASCII;
    } else if (is_b210_series(version_type)) {
        return SN_TYPE_B210_ASCII;     
    } else if (is_power5_series(version_type)) {
        return SN_TYPE_INV_POWER5_ASCII;  
    } else {
        return NULL;
    }
}

/*------------------------------------------------------------------------------
 Function: get_bms_series_type_index
 -----------------------------------------------------------------------------*/
/**
 * @brief      根据固件版本号获取电池包系列类型索引
 * @param[in]  version_type 固件版本号的大版本部分 (version/100)
 * @param[out] None
 * @return     uint8_t 电池包系列类型索引 (对应bat_type_stat_t数组索引)
 */
uint8_t get_bms_series_type_index(uint16_t version_type)
{
    if (is_b300_series(version_type)) {
        return BAT_TYPE_B300;
    } else if (is_b300s_series(version_type)) {
        return BAT_TYPE_B300S;
    } else if (is_b300k_series(version_type)) {
        return BAT_TYPE_B300K;
    } else if (is_b230_series(version_type)) {
        return BAT_TYPE_B230;
    } else if (is_b210_series(version_type)) {
        return BAT_TYPE_B210;        
    } else {
        return BAT_TYPE_UNKNOWN;
    }
}

/*------------------------------------------------------------------------------
 Function: is_bms_alpha_update_protocol
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断升级协议是否为alpha版本
 *
 * @param[in]  version_type  固件版本号的大版本部分 (version/100)
 * @return     bool  true表示为alpha协议，false表示为beta协议
 */
bool is_bms_alpha_update_protocol(uint16_t version_type) // 备注：待完善一代升级协议型号
{
    return (is_b300_series(version_type) || is_b300s_series(version_type) || is_b230_series(version_type));
}

/**
 * @brief 获取bat_data结构体的指针
 * @return bat_data_t* 指向bat_data的指针
 */
bat_data_t* get_bat_data_ptr(void)
{
    return &bat_data;
}

/**
  * @brief      更新电池维护状态
  * @param[in]  uint16_t data  
  * @param[out] None
  * @return     void
  */
void bms_alpha_aging_status_update(uint16_t data)
{
    bms_aging_status.all = data;
}

