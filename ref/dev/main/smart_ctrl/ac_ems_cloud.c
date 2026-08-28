/**
  ******************************************************************************
  * @file      ac_ems_cloud.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/25
  * @brief     云端AC_EMS控制模块
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

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "ac_ems_cloud.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[AC_EMS_CLOUD]"

/* =============================== 文件内全局变量 ================================ */

// 云端AC_EMS控制模块生效队列
static QueueHandle_t xQueue_ac_ems_cloud_ctrl = NULL;

// 云端AC_EMS控制模块配置信息（主配置结构体变量）
static USE_EXT_RAM_BSS ac_ems_cloud_ctrl_config_t AC_EMS_Cloud_Ctrl_Config = {0};

// 云端AC_EMS控制模块配置写入缓存
static USE_EXT_RAM_BSS ac_ems_cloud_ctrl_config_t AC_EMS_Cloud_Ctrl_Config_Wr = {0};

/* ================================ 模块函数定义 ================================ */

/**
 * @brief 调试用：手动设置AC_EMS_Cloud_Ctrl_Config中的几个TOU时间点
 * 
 * 该函数用于调试场景，直接在AC_EMS_Cloud_Ctrl_Config结构体中写入若干测试用的TOU配置点，
 * 便于开发过程中验证周期处理、触发判断等功能。
 */
static void Ac_Ems_Cloud_Module_DEBUG(void)
{
    // 第1个时间点：每天08:00:00，周一到周日，1月到12月，1号到31号
    tou_time_config_t *tou_cfg0 = &AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[0].time_cfg;
    memset(tou_cfg0, 0, sizeof(tou_time_config_t));
    tou_cfg0->bits.time_sec = 9 * 3600; // 09:00:00
    tou_cfg0->bits.month_sel = 0xFFF;   // 12个月
    tou_cfg0->bits.day_sel = 0x7FFFFFFF; // 31天
    tou_cfg0->bits.week_sel = 0x7F;     // 7天
    tou_cfg0->bits.reg_type = 1;
    tou_cfg0->bits.device_id = 1;
    // 假设有 channel_power 字段，初始化为测试值
    memset(AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[0].channel_power, 0x11, sizeof(AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[0].channel_power));

    // 第2个时间点：每天20:30:00，周一到周五，1月到12月，1号到31号
    tou_time_config_t *tou_cfg1 = &AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[1].time_cfg;
    memset(tou_cfg1, 0, sizeof(tou_time_config_t));
    tou_cfg1->bits.time_sec = 17 * 3600 + 30 * 60; // 17:30:00
    tou_cfg1->bits.month_sel = 0xFFF;   // 12个月
    tou_cfg1->bits.day_sel = 0x7FFFFFFF; // 31天
    tou_cfg1->bits.week_sel = 0x3E;     // 周一到周五(0b0111110)
    tou_cfg1->bits.reg_type = 2;
    tou_cfg1->bits.device_id = 2;
    memset(AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[1].channel_power, 0x22, sizeof(AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[1].channel_power));

    // 可选：写入文件
    char path[30] = {0};
    AC_EMS_CLOUD_FILE_PATH(path);
    fs_file_write_at(path, 0, (uint8_t*)&AC_EMS_Cloud_Ctrl_Config, sizeof(ac_ems_cloud_ctrl_config_t));
    ESP_LOGW(TAG, "AC_EMS Cloud Control Module Debug!");
}

/**
 * @brief 云端AC_EMS控制模块初始化
 * 
 * 1. 创建AC_EMS控制队列（用于任务间通信）。
 * 2. 加载或初始化AC_EMS控制配置信息（从文件读取，若无则初始化默认配置并写入文件）。
 * 
 * @return 0 初始化成功
 * @return -1 初始化失败
 */
int Ac_Ems_Cloud_Module_Init(void)
{
    char path[30] = {0};

    // 1. 创建AC_EMS控制模块队列（队列长度和元素大小可根据实际需求调整）
    if (xQueue_ac_ems_cloud_ctrl == NULL) {
        xQueue_ac_ems_cloud_ctrl = xQueueCreate(AC_EMS_CLOUD_UNIT_PROCESS_CNT, sizeof(ac_ems_cloud_ctrl_result_t *));
        if (xQueue_ac_ems_cloud_ctrl == NULL) {
            ESP_LOGE(TAG, "Failed to create AC_EMS cloud control queue!");
            return -1;
        }
    }

    // 2. 获取AC_EMS控制模块参数文件路径
    AC_EMS_CLOUD_FILE_PATH(path);

    // 3. 从文件读取配置信息
    int bytes_read = fs_file_read_at(path, 0, (uint8_t*)&AC_EMS_Cloud_Ctrl_Config, sizeof(ac_ems_cloud_ctrl_config_t));

    // 4. 检查读取是否成功，否则初始化默认配置并写入文件
    if (bytes_read != sizeof(ac_ems_cloud_ctrl_config_t)) 
    {
        ESP_LOGW(TAG, "AC_EMS config file read failed or invalid. Initializing default config.");

        // 初始化默认配置（全部清零）
        memset(&AC_EMS_Cloud_Ctrl_Config, 0, sizeof(AC_EMS_Cloud_Ctrl_Config));
        // 设置默认通道数量
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_cnt.ch_cnt = AC_EMS_CLOUD_CHANNEL_MAX;
        // 设置默认时间点数量
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_cnt.time_num = AC_EMS_CLOUD_UNIT_MAX_CNT;
        // 设置默认差异类型
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_en.diff_type = 1;

        // 默认使能所有通道
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_en.L1_active    = 1;
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_en.L1_reactive  = 1;
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_en.L2_active    = 1;
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_en.L2_reactive  = 1;
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_en.L3_active    = 1;
        AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_en.L3_reactive  = 1;
        
        // 写入默认配置到文件
        if (fs_file_write_at(path, 0, (uint8_t*)&AC_EMS_Cloud_Ctrl_Config, sizeof(ac_ems_cloud_ctrl_config_t)) == sizeof(ac_ems_cloud_ctrl_config_t)) {
            ESP_LOGI(TAG, "Default AC_EMS cloud config initialized and written to file.");
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write default AC_EMS cloud config to file!");
            return -1;
        }
    }

    // 本模块开发调试，出厂需屏蔽
    // Ac_Ems_Cloud_Module_DEBUG();

    ESP_LOGI(TAG, "AC_EMS Cloud Control Module initialized successfully.");
    return 0;
}

/**
 * @brief 获取云端AC_EMS控制模块配置信息的指针
 * 
 * 返回指向ac_ems_cloud_ctrl_config_t配置结构体的uint8_t指针，便于外部以字节流方式访问或传递配置数据。
 * 
 * - 当 is_write 为1时，返回写入缓存 AC_EMS_Cloud_Ctrl_Config_Wr 的指针（用于外部写入配置）。
 * - 当 is_write 为0时，返回当前生效配置 AC_EMS_Cloud_Ctrl_Config 的指针（用于外部读取配置）。
 * 
 * 建议在查表、配置同步等场景下调用本函数。
 * 
 * @param is_write  1-获取写入缓存指针，0-获取当前配置指针
 * @return uint8_t* 指向配置结构体的指针
 */
uint8_t* Ac_Ems_Cloud_Config_Get_Ptr(uint8_t is_write)
{
    if (is_write) {
        // 返回写入缓存指针，供外部写入配置
        return (uint8_t*)&AC_EMS_Cloud_Ctrl_Config_Wr;
    } else {
        // 返回当前生效配置指针，供外部读取配置
        return (uint8_t*)&AC_EMS_Cloud_Ctrl_Config;
    }
}

/**
 * @brief 将AC_EMS_Cloud_Ctrl_Config_Wr的数据（指定偏移和长度）写入AC_EMS_Cloud_Ctrl_Config，并同步更新到配置文件
 *  
 * @param offset   写入偏移（字节）
 * @param wr_len   写入长度（字节）
 * @return int     0-成功，-1-失败（如参数非法、写入越界、文件写入失败等）
 */
int Ac_Ems_Cloud_Config_Commit(size_t offset, size_t wr_len)
{
    char path[30] = {0};

    // 参数检查：越界
    if (offset + wr_len > sizeof(ac_ems_cloud_ctrl_config_t)) {
        ESP_LOGE(TAG, "Write range out of AC_EMS_Cloud_Ctrl_Config struct bounds!");
        return -1;
    }

    // 拷贝数据
    memcpy((uint8_t*)&AC_EMS_Cloud_Ctrl_Config + offset, (uint8_t*)&AC_EMS_Cloud_Ctrl_Config_Wr + offset, wr_len);

    // 获取配置文件路径
    AC_EMS_CLOUD_FILE_PATH(path);

    // 写入文件
    if (fs_file_write_at(path, 0, (uint8_t*)&AC_EMS_Cloud_Ctrl_Config, sizeof(ac_ems_cloud_ctrl_config_t)) != sizeof(ac_ems_cloud_ctrl_config_t)) {
        ESP_LOGE(TAG, "Failed to write updated AC_EMS_Cloud_Ctrl_Config to file!");
        return -1;
    }

    ESP_LOGI(TAG, "AC_EMS_Cloud_Ctrl_Config updated and written to file. offset=%u, len=%u", (unsigned)offset, (unsigned)wr_len);
    return 0;
}

/**
 * @brief 首次调用时查找最近一次有效AC_EMS云端TOU触发并发送命令
 * 
 * 该函数用于在系统首次启动或AC_EMS云端模块首次运行时，回溯历史区间，查找所有TOU配置点中最近一次有效的触发时间点，
 * 并将该触发点对应的控制命令通过队列发送给后续处理任务，确保系统上电后不会遗漏历史有效事件。
 * 
 * 实现流程：
 * 1. 遍历所有TOU配置点，分别调用 Tou_Point_Last_Trigger 查找每个点在回溯区间内的最近一次有效触发时间。
 * 2. 记录所有点中最近的（最大且不大于当前时间）触发时间及其配置点索引。
 * 3. 若找到有效触发点，则构造对应的控制命令结构体，并通过队列发送给下游任务。
 * 4. 若内存分配或队列发送失败，及时释放资源并打印错误日志。
 * 
 * @param now_time 当前时间戳（秒，time_t）
 */
static void Ac_Ems_Cloud_First_Trigger_Handle(time_t now_time)
{
    int last_trigger_index = -1;
    time_t last_trigger = 0;

    // 遍历所有TOU配置点，寻找最近的一个触发点
    for (int i = 0; i < AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_cnt.time_num; ++i) {
        tou_time_config_t *tou_cfg = &AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[i].time_cfg;
        time_t find_result = Tou_Point_Last_Trigger(tou_cfg, now_time);
        if (find_result > last_trigger) {
            last_trigger = find_result;
            last_trigger_index = i;
        }
    }

    // 若找到有效触发点，则发送控制命令
    if (last_trigger_index >= 0) {
        tou_time_config_t *tou_cfg = &AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[last_trigger_index].time_cfg;
        ac_ems_cloud_ctrl_result_t *ctrl_result = (ac_ems_cloud_ctrl_result_t*)iot_calloc(sizeof(ac_ems_cloud_ctrl_result_t));
        if (ctrl_result) {
            ctrl_result->reg_type  = tou_cfg->bits.reg_type;
            ctrl_result->device_id = tou_cfg->bits.device_id;
            memcpy(ctrl_result->channel_power, AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[last_trigger_index].channel_power, sizeof(ctrl_result->channel_power));

            if (!xQueue_ac_ems_cloud_ctrl || xQueueSend(xQueue_ac_ems_cloud_ctrl, &ctrl_result, pdMS_TO_TICKS(100)) != pdPASS) {
                ESP_LOGE(TAG, "xQueueSend failed or queue not ready, free result. idx=%d", last_trigger_index);
                free(ctrl_result);
            }
        } else {
            ESP_LOGE(TAG, "iot_calloc failed for ac_ems_cloud_ctrl_result_t. idx=%d", last_trigger_index);
        }
    }
}

/**
 * @brief 云端AC_EMS控制模块周期处理函数
 * 
 * 该函数应被周期性调用（如定时器/任务中），用于检测AC_EMS配置点在(pre_time, now_time]区间内是否有触发，
 * 并将触发结果通过队列发送给后续处理任务。 
 * 该任务需在系统时间有效后执行，建议周期500ms~1s
 */
void Ac_Ems_Cloud_Module_Process(void)
{
    static time_t pre_time = 0;           // 上次处理的时间戳
    time_t now_time = time(NULL);         // 当前时间戳

    // 首次调用或系统时间异常（如回拨）
    if (pre_time == 0 || now_time <= pre_time) {
        // 检查是否为首次调用
        if ( pre_time == 0 ) {
            // 系统上电后对过去任务的追溯逻辑尚不清晰，暂不实现
            // Ac_Ems_Cloud_First_Trigger_Handle(now_time);
        }

        // 直接更新时间并退出
        pre_time = now_time;
        return;
    }

    // 生成区间内所有天的本地00:00:00数组，便于批量TOU点判断
    tou_time_base_t base = tou_time_get_base_arr(pre_time, now_time);

    // 只有分配成功才继续处理
    if (!base.t_base_arr) {
        ESP_LOGE(TAG, "Failed to allocate t_base_arr or invalid time range. pre_time=%lld, now_time=%lld", pre_time, now_time);
    } else {
        // 遍历所有AC_EMS配置点，判断是否有触发
        for (int i = 0; i < AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_tou_plan_ch_cnt.time_num; ++i) {
            tou_time_config_t *tou_cfg = &AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[i].time_cfg;

            // 判断该TOU点在区间内是否触发
            if (tou_time_is_trigger(tou_cfg, &base)) {
                // 分配结果结构体，填充功率控制信息
                ac_ems_cloud_ctrl_result_t *ctrl_result = (ac_ems_cloud_ctrl_result_t*)iot_calloc(sizeof(ac_ems_cloud_ctrl_result_t));
                if (ctrl_result) {
                    // 填充操作类型、设备ID、各通道功率
                    ctrl_result->reg_type  = tou_cfg->bits.reg_type;
                    ctrl_result->device_id = tou_cfg->bits.device_id;
                    memcpy(ctrl_result->channel_power, AC_EMS_Cloud_Ctrl_Config.ac_ems_cloud_unit[i].channel_power, sizeof(ctrl_result->channel_power));

                    // 发送到AC_EMS控制队列，若队列满则释放内存，防止内存泄漏
                    if (!xQueue_ac_ems_cloud_ctrl || xQueueSend(xQueue_ac_ems_cloud_ctrl, &ctrl_result, pdMS_TO_TICKS(100)) != pdPASS) {
                        ESP_LOGE(TAG, "xQueueSend failed or queue not ready, free result. idx=%d", i);
                        free(ctrl_result);
                    }
                } else {
                    ESP_LOGE(TAG, "iot_calloc failed for ac_ems_cloud_ctrl_result_t. idx=%d", i);
                }
            }
        }

        // 释放天基准数组内存，防止内存泄漏
        free(base.t_base_arr);
    }

    // 更新时间戳，确保下次只处理新时间段
    pre_time = now_time;
}

/**
 * @brief 获取云端AC_EMS控制模块的下一条控制命令
 * 
 * 本函数用于从AC_EMS控制队列（xQueue_ac_ems_cloud_ctrl）中获取一条待处理的控制命令（如功率控制等）。
 * 通常由下游任务/线程调用，实现TOU触发后的具体控制动作。
 * 具体动作由调用任务自行决定，建议周期100ms~200ms。
 * 
 * - 若成功获取到命令，则将内容拷贝到用户提供的结构体 ctrl_result，并释放内部分配的内存。
 * - 若队列为空、接收失败或参数无效，则返回0。
 * 
 * @param ctrl_result [out] 指向结果结构体的指针，函数成功时填充该结构体内容
 * @return int  1-获取到命令，0-无命令或参数无效
 */
int Ac_Ems_Cloud_Command_Get(ac_ems_cloud_ctrl_result_t *ctrl_result)
{
    // 参数有效性检查
    if (!ctrl_result) {
        ESP_LOGE(TAG, "Ac_Ems_Cloud_Command_Get: ctrl_result is NULL");
        return 0;
    }

    // 检查队列是否已初始化
    if (!xQueue_ac_ems_cloud_ctrl) {
        ESP_LOGE(TAG, "Ac_Ems_Cloud_Command_Get: xQueue_ac_ems_cloud_ctrl not initialized");
        return 0;
    }

    // 从队列中接收一条命令（非阻塞，立即返回）
    ac_ems_cloud_ctrl_result_t *result_ptr = NULL;
    if (xQueueReceive(xQueue_ac_ems_cloud_ctrl, &result_ptr, 0) == pdPASS && result_ptr != NULL) {
        // 拷贝内容到用户结构体
        memcpy(ctrl_result, result_ptr, sizeof(ac_ems_cloud_ctrl_result_t));
        // 释放队列中分配的内存
        free(result_ptr);
        return 1;
    }

    // 队列为空或接收失败
    return 0;
}

