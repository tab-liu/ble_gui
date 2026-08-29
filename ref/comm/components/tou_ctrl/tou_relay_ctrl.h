/**
  ******************************************************************************
  * @file      tou_relay_ctrl.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/6/28
  * @brief     TOU控制头文件，含主要结构体定义及对外接口-
               函数声明
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/6/28  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#ifndef TOU_RELAY_CTRL_H
#define TOU_RELAY_CTRL_H

#include <stdint.h>
#include <string.h>
#include "relay_define.h"

typedef union {
    uint16_t all;
    struct {
        uint8_t soc_ctrl_init       : 1;    //soc控制初始化
        uint8_t time_ctrl_init      : 1;    //时间控制初始化
        uint8_t delay_ctrl_init     : 1;    //延时配置初始化
        uint8_t plan_ctrl_init      : 1;    //预约备电初始化
        
        uint8_t soc_ctrl_target     : 1;    //SOC控制
        uint8_t time_ctrl_target    : 1;    //时间设置
        uint8_t delay_ctrl_target   : 1;    //延时设置
        uint8_t plan_ctrl_target    : 1;    //预约备电设置
        
        uint8_t revd5               : 8;
    }bit;
}Relay_ctrl_struct;

extern Relay_ctrl_struct Relay_ctrl;


typedef union
{
    uint16_t Byte2;   // 
    struct
    {
        uint16_t relay_data_soc_ctrl	    : 1;
        uint16_t relay_data_time_ctrl       : 1;
        uint16_t relay_data_delay_ctrl      : 1;
        uint16_t relay_data_plan_ctrl       : 1;  // 预约备电控制
        uint16_t reserved                   : 12; // 保留位
    } sBit;
} BitData_RELAY_FILE_SET;


typedef struct {
	struct {
		uint16_t mon:8;
		uint16_t year:8;			// 2000年为基准
	};

	struct {
		uint16_t hour:8;//0~23
		uint16_t day:8;
	};

	struct {
		uint16_t sec:8;
		uint16_t min:8;
	};
} relay_ctrl_rtc_time_t;


extern BitData_RELAY_FILE_SET Relay_File_W_Flag;    //TODO: 移到Reals全局变量中

#pragma pack()

/*对外接口：恢复出厂设置*/
void Relay_Ctrl_Factory_Parameter01_Update(void);
void Relay_Ctrl_Factory_Parameter02_Update(void);
void Relay_Ctrl_Factory_Parameter03_Update(void);
void Relay_Ctrl_Factory_Parameter04_Update(void);

/*对外接口：Modbus写入设置存储*/
void Relay_Ctrl_Setdata_Get_From_Modbus(uint32_t address_value, uint8_t type, uint16_t write_offset, uint16_t data_len);

/*对外接口：智能控制有效状态获取*/
uint32_t Relay_SocCtrl_Enable_Status_Get(void);
uint32_t Relay_DelayCtrl_Enable_Status_Get(void);
uint32_t Relay_TimeCtrl_Enable_Status_Get(void);

/*对外接口：重置上电初始化设置下发*/
void Relay_Poweron_Init_Flag_Set(uint8_t time_ctrl_flag);

/*对外接口：系统运行参数更新*/
void Relay_Ctrl_Parameter_update(uint8_t time_init_flag, uint8_t soc);

/*对外接口：系统主任务*/
void Relay_Ctrl_Main_Task(void);

/*对外接口：备电状态获取*/
uint8_t Get_Plan_Backup_Power_Status(void);

#endif
