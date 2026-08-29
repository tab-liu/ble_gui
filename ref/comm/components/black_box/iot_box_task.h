/**
  ******************************************************************************
  * @file      iot_box_task.h
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

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "filesystem.h"
#include "dev_data_record.h"

/* ================================ 头文件宏定义 ================================ */

// 黑匣子文件一级目录
#define INV_BOX_RECORD_FOLDER_LEVEL1    LOG_RECORD_FOLDER_LEVEL1

// 黑匣子log信息
#define INV_BOX_FILE_NAME			"box"

// 黑匣子文件路径（不含根目录）
#define INV_BOX_FILE_PATH_INV_INFO(buf ,node,protocol_code) \
 	sprintf(buf,"%s%s%02d_%02d", INV_BOX_RECORD_FOLDER_LEVEL1, INV_BOX_FILE_NAME, node,protocol_code)

// 黑匣子协议版本
#define INV_BOX_PROTOCOL_CODE   1

// 最大存储记录数
#define INV_DETAILED_INFO_MAX_COUNT 		 (50)	 // 建议为50-100

// 逆变故障区起始代码
#define INV_FAULTCODE_BASE  8001

// 逆变告警区起始代码
#define INV_ALARMCODE_BASE  8113

// 黑匣子日志单条长度
#define BOX_LOG_MAX_LEN	    208

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

// 设备索引序号结构体，包含并机序号等信息
typedef union {
    uint16_t all;
    struct {
        uint16_t inv_parallel_seq : 8;   // 逆变器并机序号
        uint16_t pack_parallel_seq : 8;  // 电池包并机序号
    } bit;
} device_index_seq;

// 逆变器详细信息结构体
typedef struct {
    uint32_t curTime;                  // 记录发生时间戳（地址0-1）
    uint16_t ver;                      // 协议版本号（地址2）
    uint16_t alarmCode;                // 告警码（地址3）
    uint16_t invWorkState;             // 逆变工作状态（地址4）
    uint16_t setCtrlWorkMode;          // 逆变工作模式（地址5）
    uint16_t gridFreq;                 // 电网频率（地址6）
    uint16_t grid1Voltage;             // 电网1电压 0.1V（地址7）
    uint16_t grid1Current;             // 电网1电流 0.1A（地址8）
    uint16_t grid2Voltage;             // 电网2电压 0.1V（地址9）
    uint16_t grid2Current;             // 电网2电流 0.1A（地址10）
    int16_t inv1Voltage;               // 逆变1电压 0.1V（地址11）
    int16_t inv1Current;               // 逆变1电流 0.1A（地址12）
    int16_t inv2Voltage;               // 逆变2电压 0.1V（地址13）
    int16_t inv2Current;               // 逆变2电流 0.1A（地址14）
    uint16_t acLoad1Voltage;           // AC负载1电压 0.1V（地址15）
    uint16_t acLoad1Current;           // AC负载1电流 0.1A（地址16）
    uint16_t acLoad2Voltage;           // AC负载2电压 0.1V（地址17）
    uint16_t acLoad2Current;           // AC负载2电流 0.1A（地址18）
    uint16_t pv1Voltage;               // PV1电压 0.1V（地址19）
    uint16_t pv1Current;               // PV1电流 0.1A（地址20）
    uint16_t pv2Voltage;               // PV2电压 0.1V（地址21）
    uint16_t pv2Current;               // PV2电流 0.1A（地址22）
    uint16_t ambientTemp;              // 环境温度 1℃（地址23）
    uint16_t invMaxTemp;               // 逆变器最高温度 1℃（地址24）
    uint16_t pvDcdcMaxTemp;            // 光伏DCDC优化器最高温度 1℃（地址25）
    device_index_seq device_seq;       // 设备索引序号（地址26）
    uint16_t packTotalVoltage;         // pack总电压 0.1V（地址27）
    uint16_t packTotalCurrent;         // pack总电流 0.1A（地址28）
    uint16_t soc;                      // pack soc（地址29）
    uint16_t soh;                      // pack soh（地址30）
    uint16_t packRunStatus;            // pack总运行状态（地址31）
    uint16_t cellMinVoltage;           // pack电芯最小电压 0.001V（地址32）
    uint16_t cellMaxVoltage;           // pack电芯最大电压 0.001V（地址33）
    uint16_t cellMinTemp;              // pack电芯最小温度 1℃（地址34）
    uint16_t cellMaxTemp;              // pack电芯最大温度 1℃（地址35）
    uint16_t packCycle;                // pack循环次数（地址36）
    uint16_t register_transmission_data[15]; // 自定义寄存器透传（地址37-51，30801~30815）
} Inv_Detailed_Info;


// 逆变器详细信息历史数据结构体（前后快照）
typedef union {
    uint8_t all[BOX_LOG_MAX_LEN];
    
    struct __attribute__((packed)) {
        Inv_Detailed_Info pre_DetailedInfo; // 前一次详细信息
        Inv_Detailed_Info cur_DetailedInfo; // 当前详细信息
    };
} Inv_Detailed_Info_Datas;

_Static_assert(sizeof(Inv_Detailed_Info_Datas) == BOX_LOG_MAX_LEN, "Inv_Detailed_Info_Datas size mismatch");

// 逆变器详细信息队列结构体
typedef struct {
    Inv_Detailed_Info_Datas *pdata; // 指向详细信息数据的指针
    uint8_t inv_dev_num;            // 逆变设备序号
} Log_Inv_Detailed_Info_Queue_Struct;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建iot_box_task任务及逆变器log信息队列
 *
 * 初始化逆变器详细信息记录队列，若创建失败则打印错误并中止程序。
 *
 * @param 无
 * @return 无
 */
void box_task_init(void);

/**
 * @brief    将故障或报警信息转换为报警码并记录逆变器log信息
 *
 * @param[in]  happenType      0：故障/报警消失  1：故障/报警发生
 * @param[in]  CodeIndex       故障/报警组索引（用于计算报警码偏移）
 * @param[in]  faultInformation 故障/报警信息（每一位代表一种故障/报警）
 * @param[in]  node_id         设备序号（1~3：单机序号 4：汇总序号）
 * @return     void
 */
void SaveErrorDetailInfo(uint8_t happenType, uint16_t CodeIndex, uint16_t faultInformation, uint8_t node_id);

/**
 * @brief    逆变器详细log信息记录写入到文件系统
 *
 * 从队列中取出逆变器详细信息，写入到对应的文件系统中（只记录汇总信息）。
 *
 * @return  uint8_t  0:成功  非0（bit0=1：写入单机文件失败 bit1=1：写入汇总文件失败）
 */
uint8_t Inv_Detailed_Info_Log_Pop(void);

/**
 * @brief    inv逆变详细Log信息实时刷新，刷新间隔：1s
 *
 * 每秒刷新所有逆变器（包括汇总信息）的详细Log信息。
 *
 * @param[in]  void  
 * @return     void 
 */
void Inv_Detailed_Info_update(void);

/**
 * @brief 清除已存储的逆变器详细log信息
 *
 * 删除所有逆变器详细信息日志文件，并重置相关索引和计数。
 *
 * @return 0: 清除成功
 */
int clear_DetailInfo_File(void);

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
int Box_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size);

/**
  * @brief      检查是否为黑匣子文件
  * @param[in]  const char *fname  
  * @param[out] None
  * @return     int
  */
int Is_Box_Log_File(const char *fname);

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
int Get_Box_Log_File_Id(char *out_buf, size_t buf_len);

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
int Is_Box_Log_Count_New(time_t since_timestamp);

#ifdef __cplusplus
}
#endif

