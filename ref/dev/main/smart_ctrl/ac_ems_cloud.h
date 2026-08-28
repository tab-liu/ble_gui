/**
  ******************************************************************************
  * @file      ac_ems_cloud.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/24
  * @brief     云端AC_EMS控制模块头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/21  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "smart_common.h"

/* ================================ 头文件宏定义 ================================ */

// AC_EMS通道最大数量
#define AC_EMS_CLOUD_CHANNEL_MAX            6

// AC_EMS控制时间点最大长度
#define AC_EMS_CLOUD_UNIT_MAX_LEN           (TIME_UNIT_MAX_LEN + (AC_EMS_CLOUD_CHANNEL_MAX * sizeof(int32_t)))

// AC_EMS控制时间点最大数量
#define AC_EMS_CLOUD_UNIT_MAX_CNT           20

// AC_EMS云端控制时间点单次执行最大处理数量
#define AC_EMS_CLOUD_UNIT_PROCESS_CNT       8

// AC_EMS云端控制文件路径
#define AC_EMS_CLOUD_FILE_PATH(buf)	\
	sprintf(buf, "%s/%s%s", FS_BASE_PATH, SMART_CTRL_FILE_FOLDER_LEVEL, "AC_EMS")

// AC_EMS云端控制寄存器段起始地址
#define AC_EMS_CLOUD_REG_START_ADDR         28000

// AC_EMS云端控制寄存器段最大长度
#define AC_EMS_CLOUD_REG_MAX_LEN            400

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

// 云端ACEMS控制配置节点结构体，包含时间配置和各通道功率配置
typedef union {
    uint8_t raw[AC_EMS_CLOUD_UNIT_MAX_LEN]; // 原始字节数据（长度由CLOUD_AC_EMS_UNIT_MAX_LEN定义，便于协议打包/解包）
    struct {
        tou_time_config_t time_cfg; // TOU时间配置（定时任务的时间与周期设置）
        int32_t channel_power[AC_EMS_CLOUD_CHANNEL_MAX]; // 各通道功率配置（单位：W，数组长度由CLOUD_AC_EMS_CHANNEL_MAX定义）
    };
} ac_ems_cloud_unit_config_t;

// 编译期断言，确保结构体大小与协议定义一致
_Static_assert(sizeof(ac_ems_cloud_unit_config_t) == AC_EMS_CLOUD_UNIT_MAX_LEN, "ac_ems_cloud_unit_config_t size mismatch");

// AC EMS Cloud TOU计划通道总数量结构体
typedef union {
    uint16_t value; // 原始16位数据
    struct {
        uint8_t ch_cnt   : 8; // bit0~7：相线+有无功控制通道总数量（0表示无效控制）
        uint8_t time_num : 8; // bit8~15：时间点总数（顺序排列，前N个）
    };
} ac_ems_cloud_tou_plan_ch_cnt_t;

// AC EMS Cloud TOU计划通道使能及差异类型结构体
typedef union {
    uint16_t value; // 原始16位数据
    struct {
        uint8_t  L1_active    : 1; // bit0: L1有功
        uint8_t  L1_reactive  : 1; // bit1: L1无功
        uint8_t  L2_active    : 1; // bit2: L2有功
        uint8_t  L2_reactive  : 1; // bit3: L2无功
        uint8_t  L3_active    : 1; // bit4: L3有功
        uint8_t  L3_reactive  : 1; // bit5: L3无功
        uint8_t  reserved     : 6; // bit6~11: 预留
        uint8_t  diff_type    : 4; // bit12~15: 下表定义差异类型（0-无效，1-当前内容，2/3-预留）
    };
} ac_ems_cloud_tou_plan_ch_en_t;

// AC EMS Cloud TOU控制总配置结构体，包含TOU计划通道数量、通道使能及多个TOU单元配置
typedef union {
    uint16_t reg[AC_EMS_CLOUD_REG_MAX_LEN]; // 原始寄存器数据（长度为1000，便于协议打包/解包）
    struct {
        ac_ems_cloud_tou_plan_ch_cnt_t ac_ems_cloud_tou_plan_ch_cnt; // AC EMS Cloud TOU计划通道总数量配置（包含通道数量和时间点总数）
        ac_ems_cloud_tou_plan_ch_en_t  ac_ems_cloud_tou_plan_ch_en;  // AC EMS Cloud TOU计划通道使能及差异类型配置
        ac_ems_cloud_unit_config_t ac_ems_cloud_unit[AC_EMS_CLOUD_UNIT_MAX_CNT]; // TOU单元配置数组（每个单元包含具体的时间与通道功率设置）
    };
} ac_ems_cloud_ctrl_config_t;

_Static_assert(sizeof(ac_ems_cloud_ctrl_config_t) == (AC_EMS_CLOUD_REG_MAX_LEN * sizeof(uint16_t)), "ac_ems_cloud_ctrl_config_t size mismatch");

/**
 * @brief 云端AC_EMS控制结果结构体
 * 
 * 用于描述一次AC_EMS控制操作的相关信息：
 * - reg_type      ：操作类型（如读/写等，具体含义由业务定义）
 * - device_id     ：目标设备ID（用于区分不同设备）
 * - channel_power ：各通道功率配置（单位：W，数组长度由AC_EMS_CLOUD_CHANNEL_MAX定义）
 */
typedef struct {
    uint8_t  reg_type;   ///< 操作类型
    uint8_t  device_id;  ///< 目标设备ID
    int32_t  channel_power[AC_EMS_CLOUD_CHANNEL_MAX]; ///< 各通道功率配置（单位：W）
} ac_ems_cloud_ctrl_result_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 云端AC_EMS控制模块初始化
 * 
 * 1. 创建云端AC_EMS控制队列（用于任务间通信）。
 * 2. 加载或初始化云端AC_EMS控制配置信息（从文件读取，若无则初始化默认配置并写入文件）。
 * 
 * @return 0 初始化成功
 * @return -1 初始化失败
 */
int Ac_Ems_Cloud_Module_Init(void);

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
uint8_t* Ac_Ems_Cloud_Config_Get_Ptr(uint8_t is_write);

/**
 * @brief 将AC_EMS_Cloud_Ctrl_Config_Wr的数据（指定偏移和长度）写入AC_EMS_Cloud_Ctrl_Config，并同步更新到配置文件
 *  
 * @param offset   写入偏移（字节）
 * @param wr_len   写入长度（字节）
 * @return int     0-成功，-1-失败（如参数非法、写入越界、文件写入失败等）
 */
int Ac_Ems_Cloud_Config_Commit(size_t offset, size_t wr_len);

/**
 * @brief 云端AC_EMS控制模块周期处理函数
 * 
 * 该函数应被周期性调用（如定时器/任务中），用于检测TOU配置点在(pre_time, now_time]区间内是否有触发，
 * 并将触发结果通过队列发送给后续处理任务。 
 * 该任务需在系统时间有效后执行，建议周期500ms~1s
 */
void Ac_Ems_Cloud_Module_Process(void);

/**
 * @brief 获取云端AC_EMS控制模块的下一条控制命令
 * 
 * 本函数用于从AC_EMS控制队列中获取一条待处理的控制命令（如寄存器操作等）。
 * 通常由下游任务/线程调用，实现TOU触发后的具体控制动作。 
 * 具体动作由调用任务自行决定，建议周期100ms~200ms。
 * 
 * - 若成功获取到命令，则将内容拷贝到用户提供的结构体 ctrl_result，并释放内部分配的内存。
 * - 若队列为空、接收失败或参数无效，则返回0。
 * 
 * @param ctrl_result [out] 指向结果结构体的指针，函数成功时填充该结构体内容
 * @return int  1-获取到命令，0-无命令或参数无效
 */
int Ac_Ems_Cloud_Command_Get(ac_ems_cloud_ctrl_result_t *ctrl_result);

#ifdef __cplusplus
}
#endif
