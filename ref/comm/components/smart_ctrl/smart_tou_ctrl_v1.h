/**
  ******************************************************************************
  * @file      smart_tou_ctrl_v1.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/24
  * @brief     智能TOU控制模块头文件
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

// 智能TOU控制时间点寄存器设置最大长度
#define SMART_TOU_CONFIG_MAX_LEN            4

// 智能TOU控制时间点最大长度
#define SMART_TOU_UNIT_MAX_LEN              (TIME_UNIT_MAX_LEN + SMART_TOU_CONFIG_MAX_LEN)

// 智能TOU控制时间点最大数量
#define SMART_TOU_UNIT_MAX_CNT              140

// 智能TOU控制时间段最大数量
#define SMART_TOU_QUANTUM_MAX_CNT           (SMART_TOU_UNIT_MAX_CNT / 2)

// 智能TOU控制时间点单次执行最大处理数量
#define SMART_TOU_UNIT_SINGLE_PROCESS_CNT   32

// 智能TOU控制文件路径
#define SMART_TOU_FILE_PATH(buf)	\
	sprintf(buf, "%s/%s%s", FS_BASE_PATH, SMART_CTRL_FILE_FOLDER_LEVEL, "SmartV1")

// 智能TOU控制寄存器段起始地址
#define SMART_TOU_REG_START_ADDR            26000

// 智能TOU控制寄存器段最大长度
#define SMART_TOU_REG_MAX_LEN               1000

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

// 严格按照位定义的TOU寄存器配置结构体
typedef union {
    uint8_t raw[SMART_TOU_CONFIG_MAX_LEN]; // 原始字节数据（长度由SMART_TOU_REG_MAX_LEN定义，便于协议打包/解包）
    struct {
        uint16_t reg_addr;  // 寄存器地址（16位，指定目标寄存器的地址）
        uint16_t reg_value; // 寄存器值（16位，写入或读取的寄存器数据）
    };
} tou_general_reg_config_t;

_Static_assert(sizeof(tou_general_reg_config_t) == SMART_TOU_CONFIG_MAX_LEN, "tou_general_reg_config_t size mismatch");

// 智能TOU控制配置节点结构体，包含时间配置和寄存器配置
typedef union {
    uint8_t raw[SMART_TOU_UNIT_MAX_LEN]; // 原始字节数据（长度由SMART_TOU_UNIT_MAX_LEN定义，便于协议打包/解包）
    struct {
        tou_time_config_t time_cfg; // TOU时间配置（定时任务的时间与周期设置）
        tou_general_reg_config_t  reg_cfg;  // TOU寄存器配置（目标寄存器地址和值）
    };
} smart_tou_unit_config_t;

_Static_assert(sizeof(smart_tou_unit_config_t) == SMART_TOU_UNIT_MAX_LEN, "smart_tou_unit_config_t size mismatch");

// TOU计划总配置结构体
typedef union {
    uint16_t value; // 原始16位数据，便于协议打包/解包
    struct {
        uint8_t diff_type : 4;   // bit0-3: TOU下表定义差异类型（0-无效，1-当前内容，2/3-预留）
        uint8_t reserved  : 4;   // bit4-7: 预留
        uint8_t time_num  : 8;   // bit8-15: 时间点总数（顺序排列，前N个）
    };
} smart_tou_plan_set_t;

// 智能TOU控制总配置结构体，包含TOU计划总配置和多个TOU单元配置
typedef union {
    uint16_t reg[SMART_TOU_REG_MAX_LEN]; // 原始寄存器数据（长度为1000，便于协议打包/解包）
    struct {
        smart_tou_plan_set_t smart_tou_info; // TOU计划总配置（包含差异类型和时间点总数等信息）
        smart_tou_unit_config_t smart_tou_unit[SMART_TOU_UNIT_MAX_CNT]; // TOU单元配置数组（每个单元包含具体的时间与寄存器设置）
    };
} smart_tou_ctrl_config_t;

_Static_assert(sizeof(smart_tou_ctrl_config_t) == (SMART_TOU_REG_MAX_LEN * sizeof(uint16_t)), "smart_tou_ctrl_config_t size mismatch");


/**
 * @brief 智能TOU控制结果结构体
 * 
 * 用于描述一次TOU控制操作的寄存器相关信息：
 * - reg_type  ：寄存器操作类型（如读/写等，具体含义由业务定义）
 * - device_id ：目标设备ID（用于区分不同设备）
 * - reg_addr  ：寄存器地址（16位，指定目标寄存器的地址）
 * - reg_value ：寄存器值（16位，写入或读取的寄存器数据） 
 * - is_valid  ：是否有效
 */
typedef struct {
    uint8_t  reg_type;   ///< 寄存器操作类型
    uint8_t  device_id;  ///< 目标设备ID
    uint16_t reg_addr;   ///< 寄存器地址（16位，指定目标寄存器的地址）
    uint16_t reg_value;  ///< 寄存器值（16位，写入或读取的寄存器数据）
    bool     is_valid;   ///< 该结果是否有效
} smart_tou_ctrl_result_t;

/**
 * @brief 设备寄存器访问回调函数类型
 *
 * @param proto_ver 协议版本号（u8）
 * @param dev_addr  设备地址（u8）
 * @param reg_addr  寄存器地址（u16）
 * @param reg_value 指向寄存器值的指针（u16*）
 *                  - 读取场景：函数应把读取到的值写入 *reg_value
 * @return true  成功（读到/写入正确）
 *         false 失败（地址/权限/通信错误等）
 *
 * 说明：
 * - 回调实现应保证对 reg_value 指向内存的合法访问（非 NULL）。
 * - 建议调用方与实现方约定 proto_ver 的语义（例如字段长度/字节序）。
 */
typedef bool (*device_reg_cb_t)(uint8_t proto_ver, uint8_t dev_addr, uint16_t reg_addr, uint16_t *reg_value);

// 智能控制支路生效标志
typedef union {
    uint32_t all;
    struct {
        uint32_t DC_Total  : 1;   // DC 2012
        uint32_t AC_Total  : 1;   // AC 2011
    }bit;
}smart_ctrl_status_struct;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册/设置设备寄存器访问回调并指定检查时间间隔（毫秒）
 * @param cb 回调函数指针 (device_reg_cb_t)
 * @param check_interval_ms 检查时间间隔，单位毫秒；为0表示不检查
 */
void device_reg_cb_set(device_reg_cb_t cb, uint32_t check_interval_ms);

/**
 * @brief 智能TOU控制模块初始化
 * 
 * 1. 创建TOU控制队列（用于任务间通信）。
 * 2. 加载或初始化TOU控制配置信息（从文件读取，若无则初始化默认配置并写入文件）。
 * 
 * @return 0 初始化成功
 * @return -1 初始化失败
 */
int Smart_Tou_Ctrl_Module_Init(void);

/**
 * @brief 获取智能TOU控制模块配置信息的指针
 * 
 * 返回指向TOU配置结构体的uint8_t指针，便于外部以字节流方式访问或传递配置数据。
 * 
 * - 当 is_write 为1时，返回写入缓存 Ctrl_Config_Wr 的指针（用于外部写入配置）。
 * - 当 is_write 为0时，返回当前生效配置 Smart_Tou_Ctrl_Config 的指针（用于外部读取配置）。
 * 
 * 建议在查表、配置同步等场景下调用本函数。
 * 
 * @param is_write  1-获取写入缓存指针，0-获取当前配置指针
 * @return uint8_t* 指向配置结构体的指针
 */
uint8_t* Smart_Tou_Ctrl_Config_Get_Ptr(uint8_t is_write);

/**
 * @brief 将Ctrl_Config_Wr的数据（指定偏移和长度）写入Smart_Tou_Ctrl_Config，并同步更新到配置文件
 *  
 * @param offset   写入偏移（字节）
 * @param wr_len   写入长度（字节）
 * @return int     0-成功，-1-失败（如参数非法、写入越界、文件写入失败等）
 */
int Smart_Tou_Ctrl_Config_Commit(size_t offset, size_t wr_len);

/**
 * @brief 智能TOU控制模块周期处理函数
 * 
 * 该函数应被周期性调用（如定时器/任务中），用于检测TOU配置点在(pre_time, now_time]区间内是否有触发，
 * 并将触发结果通过队列发送给后续处理任务。 
 * 该任务需在系统时间有效后执行，建议周期500ms~1s
 */
void Smart_Tou_Ctrl_Module_Process(void);

/**
 * @brief 获取智能TOU控制模块的下一条控制命令
 * 
 * 本函数用于从TOU控制队列（xQueue_smart_tou_ctrl）中获取一条待处理的控制命令（如寄存器操作等）。
 * 通常由下游任务/线程调用，实现TOU触发后的具体控制动作。
 * 具体动作由调用任务自行决定，建议周期100ms~200ms。
 *
 * - 若成功获取到命令，则将内容拷贝到用户提供的结构体 tou_ctrl_result，并释放内部分配的内存。
 * - 若队列为空、接收失败或参数无效，则返回0。
 * 
 * @param tou_ctrl_result [out] 指向结果结构体的指针，函数成功时填充该结构体内容
 * @return int  1-获取到命令，0-无命令或参数无效
 */
int Smart_Tou_Ctrl_Command_Get(smart_tou_ctrl_result_t *tou_ctrl_result);

/**
 * @brief 获取智能TOU控制模块有效节点数量
 * @return 有效节点数量
 */
uint8_t get_smart_tou_ctrl_unit_valid_cnt(void);

/**
 * @brief 恢复 Smart TOU 配置为出厂默认并写入文件
 * @return 0 成功，-1 失败
 *
 * 说明：
 * - 不假定全局变量 Smart_Tou_Ctrl_Config 是否已初始化，函数在写入成功后会更新该全局变量。
 * - 调用方需传入正确的文件路径（平台文件系统路径）。
 */
int Smart_Tou_Restore_Factory_Defaults(void);

/**
 * @brief 设置智能TOU控制模块时间段使能
 * @return 无
 */
void Smart_Tou_Ctrl_Time_Quantum_Enable_Set(bool flag);

/**
 * @brief 获取当前智能TOU控制支路状态指针
 *
 * 返回内部全局状态变量 Smart_Tou_Ctrl_Status 的地址，供外部查询当前支路状态。
 * 使用 const 限制调用方直接修改内部状态。
 *
 * @return const smart_ctrl_status_struct* 当前智能TOU控制支路状态指针
 */
const smart_ctrl_status_struct *Smart_Tou_Ctrl_Status_Get(void);

#ifdef __cplusplus
}
#endif
