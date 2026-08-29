/**
  ******************************************************************************
  * @file      modbus_slave.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/24
  * @brief     服务器modbus交互
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/24  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */
#include "can_protocol.h"
#include "inv_define.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "modbus_slave.h"
#include "modbus_data.h"
#include "esp_log.h"
#include "modbus_protocol.h"
#include "comm_define.h"
#include <stdint.h>

#include "modbus_master.h"
#include "dev_modbus_manage.h"
#include "wlcc_process.h"

#define DEFAULT_ADDRESS     0x01

#define DALAY_MS_UART 100//避免连帧延时

static const char *TAG = "[MODBUS_SLAVE]";

static SemaphoreHandle_t xSemaphore = NULL; //申明互斥型信号量，在FreeRTOS中二值型信号量和互斥型信号量类型完全相同。

USE_EXT_RAM_BSS static uint8_t Modbus_To_Wireless_Write_buffer[280] = {0};

uint16_t Modbus_Error(uint8_t *response, uint8_t error)
{
    response[1] |= 0x80;
    response[2]  = error;
    uint16_t crc16 = ModbusCrc16(response,  3);
    response[3] = crc16;
    response[4] = crc16 >> 8;
    return 5;
}

uint16_t md_data_CallBack_run(uint8_t SlaveAddress, uint16_t startAddress,uint16_t readRegCnt) 
{
	md_priv_data_t priv_data = 
	{
//		.ota_type = BLE_OTA,					// ota通道类型
//		.ota_response = ble_ota_data_reponse,	// 传递给xmodem升级的响应函数
	};
	md_data_t *p_data = md_tbl_find(startAddress);
	if (p_data == NULL)
	{
		ESP_LOGD(TAG, "find register table failure, line: %d", __LINE__);
		return -1;
	}
	

	/* 表回调函数 */
	if (p_data->tbl.tbl_cb)//检查
	{
		tbl_cb_data_t cb_data = {
				.SlaveAddress =SlaveAddress,
				.reg_addr = startAddress,
				.reg_nums = readRegCnt,
				.is_write = true,
		};
		if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
		{
			ESP_LOGE(TAG, "md_data_CallBack_run :tbl_cb 1,startAddress=%u ",startAddress );
		
			return -1;
		}
	}	
	return 0;

}


#ifdef CONFIG_CAN_PORT_ENABLE

/*
modbus beta转can的 每一行寄存器的can字节数 CAN表没有的长度设为0

共计 Can beta四块 数据 
2000
2200
2300
2400
*/
static const uint8_t MOD_STRUCT_len_reg02000[] =
{
	0,//uint16_t SetInvSelect;//2000	2
	2,//uint16_t mon_year;//
	2,//uint16_t hour_day;
	2,//uint16_t sec_min;    
	2,//uint16_t  res;//2004 时区
	1,//uint16_t  work_mode;     // 2005  00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网
	2,//ctrl_event1  ctrl;       // 2006 一次性控制事件,
	1,//uint16_t  ctrl_led;       // 2007 控制LED //2
    1,//ctrl_meter_struct  ctrl_meter;     // 2008 控制电表 (未用) ;20240920 windy 除PLP024未启用
    1,//uint16_t  ctrl_pv;        // 2009 控制PV
    1,//uint16_t  ctrl_inv;     // 2010 控制逆变
    1,//uint16_t  ctrl_ac;  // 2011 控制ac
    1,//uint16_t  ctrl_dc;  // 2012 控制dc
    1,//uint16_t  ctrl_poweron;  // 2013 控制电源
    1,//uint16_t  ctrl_dc_eco;       // 2014 控制dc eco模式0：关闭，1：开启
    1,//uint16_t  ctrl_dc_eco_time;  // 2015 控制dc eco模式空闲时间
    2,//uint16_t eco_dc_power_value;    // 2016 dc eco模式触发运行功率
    1,//uint16_t  ctrl_ac_eco;       // 2017 控制ac eco模式 0：关闭，1：开启
    1,//uint16_t  ctrl_ac_eco_time; // 2018 控制ac eco模式空闲时间
    2,//uint16_t eco_ac_power_value; // 2019 ac eco模式触发运行功率
    1,//uint16_t  ctrl_chg_mode; // 2020 控制充电模式
    1,//uint16_t  ctrl_super_power; // 2021 控制超级输出模式
    1,//uint16_t  ctrl_low_cap_pct;  // 2022 系统低电量百分比
    1,//uint16_t  ctrl_high_cap_pct; // 2023 系统满电量百分比
    1,//uint16_t  ctrl_inv_mode; // 2024 单火线，双火线，并机，并机双火线
    1,//uint16_t  ctrl_dev_id; // 2025 给逆变设置编号
    1,// uint16_t  ctrl_all_energy_type; // 2026 历史能量
    1,// uint16_t  ctrl_now_energy_type; // 2027 当前能量
    1,// uint16_t  ctrl_log_page;   // 2028 控制历史记录页
    1,//uint16_t  ctrl_time_area; // 2029 控制时间分段管理
    1,//SetTimeCtrlLable1   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart1
    2,//SetTimeCtrlEnd1
    1,//SetTimeCtrlLable2   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart2
    2,//SetTimeCtrlEnd2
    1,//SetTimeCtrlLable3   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart3
    2,//SetTimeCtrlEnd3
    1,//SetTimeCtrlLable4   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart4
    2,//SetTimeCtrlEnd4
    1,//SetTimeCtrlLable5   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart5
    2,//SetTimeCtrlEnd5
    1,//SetTimeCtrlLable6   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart6
    2,//SetTimeCtrlEnd6
    1,//SetTimeCtrlLable7   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart7
    2,//SetTimeCtrlEnd7
    1,//SetTimeCtrlLable8   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart8
    2,//SetTimeCtrlEnd8
    1,//SetTimeCtrlLable9   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart9
    2,//SetTimeCtrlEnd9
    1,//SetTimeCtrlLable10   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart10
    2,//SetTimeCtrlEnd10
    1,//SetPv1Type  uint16_t ctrl_PvType[6];//2060~2065
    1,//SetPv2Type
    1,//SetPv3Type
    1,//SetPv4Type
    1,//SetPv5Type
    1,//SetPv6Type
    1,//uint16_t ctrl_alarm_voice;//2066
    1,//uint16_t ctrl_lcd_active_time;//2067
    0,//uint16_t P_inv_active_target_L1;//2068
    0,//uint16_t P_inv_active_target_L2;//2069
    0,//uint16_t P_inv_active_target_L3;//2070
    0,//uint16_t Rated_AC_Power;//2071
    2,//Custom_mode_set_struct Custom_mode_set;//2072
    2,//Remote_set_struct Remote_set;//2073
    2,//uint16_t Remote_set_soc_limit;//2074
    2,//uint16_t Rv_SOC_Ownership;//2075
    2,//uint16_t LevelSwitch 2076
    2,//uint16_t sleep_remain_time 2077
    2,//uint16_t LED_SET 2078
    2,//uint16_t Remote_set_power 2079
    2,//uint16_t Pack_nums_check 2080
    2,//uint16_t INV_Set_Show 2081
    2,//uint16_t DCDC_Set_Show 2082
    2,//uint16_t SOC_H; 2083
    2,//uint16_t PV_SET; 2084
    2,//uint16_t DC_output;//2085 DC输出
    2,//uint16_t Regulatory_set;//2086 合规设置
    2,//uint16_t Cycle_capacity;//2087 循环容量
    2,//uint16_t Cycle_max_capacity;//2088 最大循环容量
    2,//uint16_t Effective_time_ym;//2089 生效时间年月
    2,//uint16_t Effective_time_dh;//2090 生效时间日时
    2,//uint16_t Effective_time_ms;//2091 生效时间分秒
    2,//uint16_t ECO_status;//2092
    2,//uint16_t set_AC_branch; // 2093
    2,//uint16_t set_DC_branch; // 2094
    2,//uint16_t DC_SET;        // 2095
    2,//uint16_t AC_button_sign;// 2096
};//MOD_STRUCT_reg02000;

static const uint8_t MOD_STRUCT_len_reg02200[] =
{
    2,//uint8_t password[8]; //2200~2203 设置区密码
	2,//
	2,//
	2,// 2203
    0,// uint16_t revd1[2];  //2204~2205	1
	0,// 	1
    1,// uint16_t ctrl_reset_factory; //2206 恢复出厂
    1,// uint16_t ctrl_grid;      //2207 控制电网
    1,// uint16_t ctrl_feedback;  //2208 控制馈电
    2,// uint16_t ctrl_output_inv_volt;  //2209 输出电压控制
    2,// uint16_t ctrl_output_inv_freq;   //2210 输出电压频率
    2,// uint16_t ctrl_chg_max_volt; //2211 最大充电电压
    2,// uint16_t ctrl_pv_max_curr; //2212 最大PV充电电流
    2,// uint16_t ctrl_grid_max_power;   //2213 电网最大功率
    2,// uint16_t ctrl_grid_max_curr; //2214 电网最大电流
    2,// uint16_t ctrl_feedback_max_power;   //2215 电网馈电最大功率
    2,// uint16_t ctrl_feedback_max_curr; //2216 电网馈电最大电流
    2,// uint16_t off_grid_micro_rated_power; //2217 离网交流PV额定功率（该参数设置为系统实际接入的微逆的额定功率，使得逆变根据实际情况更准确的调节微逆功率）
    1,// uint16_t  ctrl_user_area; //2218 用户地区设置
    1,// uint16_t  ctrl_pv_paralle[6]; //2219~2224 设置PV并联开启和关闭
	1,//
	1,//
	1,//
	1,//
	1,// 2224
    1,// uint16_t  ctrl_grid_plus;    //2225 电网增强模式
    1,// uint16_t  ctrl_save_power_state; //2226 保存AC,DC关机前状态
    1,// uint16_t  ctrl_meter_enable; //2227 电表开启
    1,// uint16_t  ctrl_meter_select; //2228 电表类型选择
    1,// uint16_t  ctrl_Inv_Multi_enable; //2229 逆变器并机模式
    1,// uint16_t  ctrl_Inv_addr_Set; //2230 设置逆变器地址
    2,// ct_test_struct1 ct_test;    //2231
	2,// ctrl_mix_t ctrl_mix;  //2232 ctrl_mix_struct1
	0,// bat_maintain_struct1 bat_maintain;  //2233		2
	0,// uint16_t P_inv_active_target_L1; //2234		2
	0,// uint16_t P_inv_reactive_target_L1;//2235		2
	0,// uint16_t P_inv_active_target_L2; //2236		2
	0,// uint16_t P_inv_reactive_target_L2;//2237		2
	0,// uint16_t P_inv_active_target_L3; //2238		2
	0,// uint16_t P_inv_reactive_target_L3;//2239		2
	0,// uint16_t P_PV_Limit;//2240			2
	2,// ems_ctrl_struct1 ems_ctrl;//2241	2
	2,// ctrl_mix2_struct1 ctrl_mix2;//2242
    2,// uint16_t    chargingpile_type;  //2243 充电桩型号
    2,// ct_ratio_struct1  ct_ratio;           //2244 CT比例
    0,// ct_test_struct1   ac_ct_test;         //2245 交流耦合电表CT检测	2
    2,// GenSet_struct GenSet; //2246
    2,// uint16_t Gen_SOC_Start;//2247
    2,// uint16_t Gen_SOC_Stop;//2248
    2,// SetTimeCtrl_struct2 SetTimeCtrl[3];//2249~2257
	2,// 
	2,// 
	2,// 
	2,// 
	2,// 
	2,// 
	2,// 
	2,// 2257
    2,// Undervoltage_protection	uint16_t revd[8];//2258~2265 便携
	2,// Undervoltage_protection_time
	2,// Highvoltage_protection
	2,// Highvoltage_protection_time
	2,// Underfrequency_protection
	2,// Underfrequency_protection_time
	2,// Overvoltage_protection
	2,// Overvoltage_protection_time	2265
    0,// uint16_t CounterCurrentPower_Limit;//2266						2
    2,// ctrl_meter_struct  ctrl_meter;     // 2267 控制电表(同2008)	2
    0,// uint16_t Meter_Select;//2268 低字节：电表接入选择				2
	2,// uint16_t SetCtrlPv;//2269 设置PV开启关闭
	2,// uint16_t Phase_set;//2270 单相双火相设置
	2,// uint16_t DCHUB_set;//2271 DCHUB设置电压_AC380设置
    2,//uint16_t SetGridMaxCurrent_in;//2272 最大电网输入电流（每相）
    2,//uint16_t FUNC_set;//2273 开关设置
    2,//RvSettings_struct RvSettings;//2274 房车场景及电池设置
    2,//uint16_t AltDelays_ShutdownTime;//2275 Alt延迟关闭输出时间
    2,//RvOnOff_struct RvOnOff;//2276 房车开关杂项
    2,//uint32_t RvBatteryCapacity;//2277~2278  房车电池容量(0.1-2400.0Ah  单位0.1Ah，仅在电池类型5、6才显示)
    2,//
    2,//uint16_t BatterySettings 2279
    0,//uint32_t heat_pump_Enable;//2280~2281 热泵使能设置
    0,//
    2,//uint16_t heat_pump_auto_set_Active_Power;//2282 热泵自动模式的馈电功率阈值
    2,//uint16_t heat_pump_auto_set_minimum_Runtime;//2283 热泵自动模式的最小运行时间
    2,//uint16_t heat_pump_auto_set_Delay_Start_Time;//2284 热泵自动模式的启动延时
    0,//int32_t P_inv_active_target_L1; //2285~2286 L1相逆变器有功目标功率 (有符号)
    0,//
    0,//int32_t P_inv_active_target_L2; //2287~2288 L2相逆变器有功目标功率 (有符号)
    0,//
    0,//int32_t P_inv_active_target_L3; //2289~2290 L3相逆变器有功目标功率 (有符号)
    0,//
    0,//int32_t P_inv_reactive_target_L1;//2291~2292 L1相逆变器无功目标功率 (有符号)
    0,//
    0,//int32_t P_inv_reactive_target_L2;//2293~2294 L2相逆变器无功目标功率 (有符号)
    0,//
    0,//int32_t P_inv_reactive_target_L3;//2295~2296 L3相逆变器无功目标功率 (有符号)
    0,//
    0,//int32_t P_inv_apparent_target_L1;//2297~2298 L1相逆变器视在目标功率 (有符号)
    0,//
    0,//int32_t P_inv_apparent_target_L2;//2299~2300 L2相逆变器视在目标功率 (有符号)
    0,//
    0,//int32_t P_inv_apparent_target_L3;//2301~2302 L3相逆变器视在目标功率 (有符号)
    0,//
    2,//uint16_t SetFeedbackMode;// 2303 电网馈电模式配置（兼容户用储能+阳台光伏）
    2,//uint16_t PvMultiPeak;   // 2304 光伏多峰
    2,//uint16_t inv_meter_interaction; // 2305 逆变联动电表
    2,//AC_Input_Ctrl_struct AC_Input_Ctrl; // 2306 交流输入源设置
    2,//uint16_t grid_LN_phase_map_result;
    2,//uint16_t grid_CT_phase_map_result;
    2,//uint16_t inverter_LN_phase_map_result;
    2,//uint16_t inverter_CT_phase_map_result;
};//MOD_STRUCT_reg02200;

#if 0

static const uint8_t MOD_STRUCT_len_reg02300[] =
{
    2,//uint16_t SetGridUV1Value;        // 电网欠压LEVEL1保护值
    2,//uint16_t SetGridUV1Time;         // 电网欠压LEVEL1触发时间
    2,//uint16_t SetGridUV2Value;        // 电网欠压LEVEL2保护值
    2,//uint16_t SetGridUV2Time;         // 电网欠压LEVEL2触发时间
    2,//uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
    2,//uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
    2,//uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
    2,//uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
    2,//uint16_t SetGridUF1Value;        // 电网欠频保护值
    2,//uint16_t SetGridUF1Time;         // 电网欠频保护时间
    2,//uint16_t SetGridOF1Value;        // 电网过频保护值
    2,//uint16_t SetGridOF1Time;         // 电网过频保护时间
    2,//uint16_t SetFreqSlope;           // 降频斜率
    2,//uint16_t SetPowerPct;            // 功率变化率
    2,//uint16_t SetGridVoltMinValue;    // 电网重连电压下限值
    2,//uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
    2,//uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
    2,//uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
    2,//uint16_t SetGridRetryTime;       // 电网异常重连时间
    2,//uint16_t SetGridAdvance;         // 高级功能使能
    2,//uint16_t SetGridUF2Value;         // 电网欠频保护值
    2,//uint16_t SetGridUF2Time;          // 电网欠频保护时间
    2,//uint16_t SetGridOF2Value;         // 电网过频保护值
    2,//uint16_t SetGridOF2Time;          // 电网过频保护时间
    2,//uint16_t SetGridAVGOVValue;       // 均值过压保护值
    2//uint16_t SetGridArea;             // 电网认证使用地区
};//inv_set02_struct;

#endif

static const uint8_t MOD_STRUCT_len_reg02400[] =
{
    2,//uint16_t SetGridEnable;          // 高级功能使能
    2,//uint16_t SetGridCountry;         // 电网认证使用地区
    2,//uint16_t SetGridUV1Value;        // 电网欠压LEVEL1保护值
    2,//uint16_t SetGridUV1Time;         // 电网欠压LEVEL1触发时间
    2,//uint16_t SetGridUV2Value;        // 电网欠压LEVEL2保护值
    2,//uint16_t SetGridUV2Time;         // 电网欠压LEVEL2触发时间
	2,//uint16_t pf;					 // 功率因数
	2,//uint16_t set_region;			 // 地区
	2,//uint16_t mode_enable;			 // 模式使能
	2,//uint16_t power_rate_limit;		 // 每分钟功率变化比率
    2,//uint16_t SetGridAVGOVValue;      // 电网均值过压
    2,//uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
    2,//uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
    2,//uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
    2,//uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
	2,//uint16_t power_reactive_to_active_ratio; // 无功占额定有功比例
	2,//uint16_t vw1;					 // Volt–watt响应模式的参数,Vw1<Vw2
	2,//uint16_t vw2;					 // Volt–watt响应模式的参数,Vw1<Vw2
    2,//uint16_t nc2[1];
    2,//uint16_t SetGridUF1Value;        // 电网欠频LEVEL1保护值
    2,//uint16_t SetGridUF1Time;         // 电网欠频LEVEL1保护时间
    2,//uint16_t SetGridUF2Value;        // 电网欠频LEVEL2保护值
    2,//uint16_t SetGridUF2Time;         // 电网欠频LEVEL2保护时间
	2,//uint16_t vv1;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	2,//uint16_t vv2;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	2,//uint16_t vv3;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	2,//uint16_t vv4;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
    2,//uint16_t SetGridOF1Value;        // 电网过频LEVEL1保护值
    2,//uint16_t SetGridOF1Time;         // 电网过频LEVEL1保护时间
    2,//uint16_t SetGridOF2Value;        // 电网过频LEVEL2保护值
    2,//uint16_t SetGridOF2Time;         // 电网过频LEVEL2保护时间
    2,//uint16_t nc4[4];
    2,//uint16_t SetGridVoltMinValue;    // 电网重连电压下限值
    2,//uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
    2,//uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
    2,//uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
    2,//uint16_t SetGridRetryTime;       // 电网异常重连时间
	2//uint16_t SetDefaultValue;        // 设置默认值
};//inv_set03_struct;


/*------------------------------------------------------------------------------
 Function: Get_Modbus2000_To_Can_Offset_Len
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取modbus协议2000段转Can后协议偏移及寄存器�-
                ��度
  * @param[in]  uint16_t iReadAddr         
                int remain_reg_cnt         
                can_data_label *can_label  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t Get_Modbus2000_To_Can_Offset_Len(uint16_t iReadAddr, int remain_reg_cnt, can_data_label *can_label)
{
    uint16_t iReadNum = 0;
    uint16_t templen = 0;
    uint8_t *temp_input = 0;
    uint8_t *temp_store = 0;

    /* 2000-2200 -> 0x1a */ //系统基本参数                     
    // 2005
    if(iReadAddr == 2005) 
    {       
        /*写入2005，工作模式*/
        if (( 1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.thunder_work_mode_state ) 
            && (4 != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.work_mode))
        {
            // 风暴预警期间不允许修改工作模式，不向下转发
            ESP_LOGE(TAG, "thunder_state is running, inv_mode set invaild!");
        }
        else
        {
            memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,work_mode), 
                (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,work_mode), 1);
            can_label->offset = offsetof(inv_set00_struct,work_mode);
            
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
            
            templen = 1;
            temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, work_mode);
            temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, work_mode);
            if(memcmp(temp_input, temp_store, templen) != 0)
            {
                memcpy(temp_store, temp_input, templen);
                reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
            }
        }
    }
    // 2006
    else if(iReadAddr == 2006) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl), 2);
        can_label->offset = offsetof(inv_set00_struct,ctrl);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2007
    else if(iReadAddr == 2007) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_led), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_led), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_led);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_led);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_led);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2008 
    else if(iReadAddr == 2008) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_meter), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_meter), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_meter);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_meter);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_meter);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2009 
    else if(iReadAddr == 2009) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_pv), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_pv), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_pv);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_pv);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_pv);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2010 
    else if(iReadAddr == 2010) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_inv), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_inv);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_inv);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_inv);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2011 
    else if(iReadAddr == 2011) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_ac), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_ac);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_ac);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_ac);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2012 
    else if(iReadAddr == 2012) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_dc), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_dc);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_dc);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_dc);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#ifndef CONFIG_IOT_CAN_EMS_CTRL_FUNC5    
    // 2013 
    else if(iReadAddr == 2013) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_poweron), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_poweron), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_poweron);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_poweron);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_poweron);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#endif    
    // 2014 
    else if(iReadAddr == 2014) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_dc_eco);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2015
    else if(iReadAddr == 2015) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco_time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco_time), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_dc_eco_time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2016
    else if(iReadAddr == 2016) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,eco_dc_power_value), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_dc_power_value), 2);
        can_label->offset = offsetof(inv_set00_struct,eco_dc_power_value);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, eco_dc_power_value);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, eco_dc_power_value);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2017 
    else if(iReadAddr == 2017) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_ac_eco);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2018 
    else if(iReadAddr == 2018) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco_time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco_time), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_ac_eco_time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2019
    else if(iReadAddr == 2019) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,eco_ac_power_value), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_ac_power_value), 2);
        can_label->offset = offsetof(inv_set00_struct,eco_ac_power_value);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, eco_ac_power_value);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, eco_ac_power_value);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2020 
    else if(iReadAddr == 2020) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_chg_mode), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_chg_mode), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_chg_mode);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_chg_mode);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_chg_mode);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2021 
    else if(iReadAddr == 2021) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_super_power), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_super_power), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_super_power);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_super_power);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_super_power);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2022 
    else if(iReadAddr == 2022) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_low_cap_pct), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_low_cap_pct), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_low_cap_pct);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_low_cap_pct);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_low_cap_pct);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2023 
    else if(iReadAddr == 2023) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_high_cap_pct), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_high_cap_pct), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_high_cap_pct);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_high_cap_pct);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_high_cap_pct);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2024 
    else if(iReadAddr == 2024) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_inv_mode), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv_mode), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_inv_mode);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_inv_mode);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_inv_mode);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2025 
    else if(iReadAddr == 2025) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_dev_id), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dev_id), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_dev_id);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_dev_id);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_dev_id);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#ifdef KWH_YEAR_REPORT_MODBUS_ENABLE
    // 2026 
    else if(iReadAddr == 2026) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_all_energy_type), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_all_energy_type), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_all_energy_type);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_all_energy_type);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_all_energy_type);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2027 
    else if(iReadAddr == 2027) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_now_energy_type), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_now_energy_type), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_now_energy_type);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_now_energy_type);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_now_energy_type);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#endif
#ifdef MODBUS_GET_HISTORY_FROM_ARM_ENABLE
    // 2028 
    else if(iReadAddr == 2028) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_log_page), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_log_page), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_log_page);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_log_page);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_log_page);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
        
        /*插队读取3000段*/
        if(INV_TYPE_LOG_20H == reals.can_poll[INDEX_INV_TYPE_LOG_20H].can_type) {
            if((0 == reals.can_poll[INDEX_INV_TYPE_LOG_20H].modbus_Rd_cnt_100ms) 
                || (5 < reals.can_poll[INDEX_INV_TYPE_LOG_20H].modbus_Rd_cnt_100ms)){
                reals.can_poll[INDEX_INV_TYPE_LOG_20H].modbus_Rd_cnt_100ms = 5; 
            }
        } else {
            ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_LOG_20H].can_type);// 
        }   
    }
#endif
    // 2029 
    else if(iReadAddr == 2029) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_time_area), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time_area), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_time_area);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_time_area);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_time_area);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2030-2059    
    else if(iReadAddr <= 2059 && iReadAddr >= 2030) 
    {
        uint8_t temp_count = (iReadAddr - 2030);
        // memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_time), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time), sizeof(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_time));
        can_label->offset = ((temp_count * 2) - (temp_count/3)) + offsetof(inv_set00_struct,ctrl_time);
        
        iReadNum = ((2059 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2059 - iReadAddr + 1);

        for(int i = 0;i < 10;i++)// 2030~2059
        {
            memcpy((uint8_t *)&can_node_wr.Inv.inv_set00.ctrl_time[i].lable, (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_time[i].lable,sizeof(can_node_wr.Inv.inv_set00.ctrl_time[i].lable));
            memcpy((uint8_t *)&can_node_wr.Inv.inv_set00.ctrl_time[i].start, (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_time[i].start,sizeof(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_time[i].start));
            memcpy((uint8_t *)&can_node_wr.Inv.inv_set00.ctrl_time[i].end, (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_time[i].end,sizeof(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_time[i].end));
        }

        templen = sizeof(can_node_wr.Inv.inv_set00.ctrl_time);
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }   
    // 2060-2065
    else if(iReadAddr <= 2065 && iReadAddr >= 2060) //can_label->offset = ((iReadAddr - 2060) * 2) + offsetof(inv_set00_struct,ctrl_PvType);
    {
        templen = 1;

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
        
        switch(iReadAddr)
        {
            case 2060 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[0]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[0]), 1);
                can_label->offset = ((iReadAddr - 2060)) + offsetof(inv_set00_struct,ctrl_PvType[0]); 
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[0]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[0]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
                }
                break;
            case 2061 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[1]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[1]), 1);
                can_label->offset = ((iReadAddr - 2061)) + offsetof(inv_set00_struct,ctrl_PvType[1]);
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[1]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[1]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
                }
                break;
            case 2062 :
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[2]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[2]), 1);
                can_label->offset = ((iReadAddr - 2062)) + offsetof(inv_set00_struct,ctrl_PvType[2]);
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[2]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[2]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
                }
                break;
            case 2063 :
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[3]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[3]), 1);
                can_label->offset = ((iReadAddr - 2063)) + offsetof(inv_set00_struct,ctrl_PvType[3]);
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[3]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[3]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
                }
                break;
            case 2064 :
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[4]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[4]), 1);
                can_label->offset = ((iReadAddr - 2064)) + offsetof(inv_set00_struct,ctrl_PvType[4]);
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[4]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[4]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
                }
                break;
            case 2065 :
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[5]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[5]), 1);
                can_label->offset = ((iReadAddr - 2065)) + offsetof(inv_set00_struct,ctrl_PvType[5]);
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[5]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[5]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
                }
                break;
        }
    }
    // 2066 
    else if(iReadAddr == 2066) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ctrl_alarm_voice), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_alarm_voice), 1);
        can_label->offset = offsetof(inv_set00_struct,ctrl_alarm_voice);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ctrl_alarm_voice);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ctrl_alarm_voice);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2067
    else if(iReadAddr == 2067) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,setLcdActiveTime), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_lcd_active_time), 1);
        can_label->offset = offsetof(inv_set00_struct,setLcdActiveTime);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, setLcdActiveTime);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, setLcdActiveTime);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2068~2671 阳台光伏使用
    // 2072
    else if(iReadAddr == 2072) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Custom_mode_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Custom_mode_set), 2);
        can_label->offset = offsetof(inv_set00_struct,Custom_mode_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Custom_mode_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Custom_mode_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#ifndef CONFIG_IOT_CAN_EMS_CTRL_FUNC5    
    // 2073
    else if(iReadAddr == 2073) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Remote_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Remote_set), 2);
        can_label->offset = offsetof(inv_set00_struct,Remote_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Remote_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Remote_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2074
    else if(iReadAddr == 2074) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Remote_set_soc_limit), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Remote_set_soc_limit), 2);
        can_label->offset = offsetof(inv_set00_struct,Remote_set_soc_limit);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Remote_set_soc_limit);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Remote_set_soc_limit);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#endif    
    // 2075
    else if(iReadAddr == 2075) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Rv_SOC_Ownership), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Rv_SOC_Ownership), 2);
        can_label->offset = offsetof(inv_set00_struct,Rv_SOC_Ownership);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Rv_SOC_Ownership);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Rv_SOC_Ownership);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2076
    else if(iReadAddr == 2076) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,LevelSwitch), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,LevelSwitch), 2);
        can_label->offset = offsetof(inv_set00_struct,LevelSwitch);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, LevelSwitch);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, LevelSwitch);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2077
    else if(iReadAddr == 2077) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,sleep_remain_time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,sleep_remain_time), 2);
        can_label->offset = offsetof(inv_set00_struct,sleep_remain_time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, sleep_remain_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, sleep_remain_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#ifndef CONFIG_IOT_LED_SET_ENABLE    
    // 2078
    else if(iReadAddr == 2078) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,led_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,LED_SET), 2);
        can_label->offset = offsetof(inv_set00_struct,led_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, led_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, led_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
#endif    
    // 2079
    else if(iReadAddr == 2079) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,remote_set_power), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Remote_set_power), 2);
        can_label->offset = offsetof(inv_set00_struct,remote_set_power);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, remote_set_power);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, remote_set_power);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2080
    else if(iReadAddr == 2080) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,pack_set_show), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Pack_Set_Show), 2);
        can_label->offset = offsetof(inv_set00_struct,pack_set_show);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, pack_set_show);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, pack_set_show);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2081
    else if(iReadAddr == 2081) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,inv_set_show), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,INV_Set_Show), 2);
        can_label->offset = offsetof(inv_set00_struct,inv_set_show);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, inv_set_show);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, inv_set_show);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2082
    else if(iReadAddr == 2082) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,dcdc_set_show), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DCDC_Set_Show), 2);
        can_label->offset = offsetof(inv_set00_struct,dcdc_set_show);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, dcdc_set_show);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, dcdc_set_show);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2083
    else if(iReadAddr == 2083) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,soc_h), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,SOC_H), 2);
        can_label->offset = offsetof(inv_set00_struct,soc_h);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, soc_h);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, soc_h);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2084
    else if(iReadAddr == 2084) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,pv_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,PV_SET), 2);
        can_label->offset = offsetof(inv_set00_struct,pv_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, pv_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, pv_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2085
    else if(iReadAddr == 2085) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,dc_output), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DC_output), 2);
        can_label->offset = offsetof(inv_set00_struct,dc_output);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, dc_output);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, dc_output);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2086
    else if(iReadAddr == 2086) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Regulatory_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Regulatory_set), 2);
        can_label->offset = offsetof(inv_set00_struct,Regulatory_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Regulatory_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Regulatory_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2087
    else if(iReadAddr == 2087) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Cycle_capacity), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_capacity), 2);
        can_label->offset = offsetof(inv_set00_struct,Cycle_capacity);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Cycle_capacity);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Cycle_capacity);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2088
    else if(iReadAddr == 2088) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Cycle_max_capacity), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_max_capacity), 2);
        can_label->offset = offsetof(inv_set00_struct,Cycle_max_capacity);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Cycle_max_capacity);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Cycle_max_capacity);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2089
    else if(iReadAddr == 2089) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Effective_time_mon), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ym), 2);
        can_label->offset = offsetof(inv_set00_struct,Effective_time_mon);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Effective_time_mon);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Effective_time_mon);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2090
    else if(iReadAddr == 2090) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Effective_time_hour), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_dh), 2);
        can_label->offset = offsetof(inv_set00_struct,Effective_time_hour);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Effective_time_hour);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Effective_time_hour);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2091
    else if(iReadAddr == 2091) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,Effective_time_sec), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ms), 2);
        can_label->offset = offsetof(inv_set00_struct,Effective_time_sec);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, Effective_time_sec);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, Effective_time_sec);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2092
    else if(iReadAddr == 2092) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,ECO_status), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ECO_status), 2);
        can_label->offset = offsetof(inv_set00_struct,ECO_status);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, ECO_status);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, ECO_status);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2093
    else if(iReadAddr == 2093) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,set_AC_branch), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_AC_branch), 2);
        can_label->offset = offsetof(inv_set00_struct,set_AC_branch);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, set_AC_branch);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, set_AC_branch);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2094
    else if(iReadAddr == 2094) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,set_DC_branch), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_DC_branch), 2);
        can_label->offset = offsetof(inv_set00_struct,set_DC_branch);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, set_DC_branch);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, set_DC_branch);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }
    // 2095
    else if(iReadAddr == 2095) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct,DC_SET), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DC_SET), 2);
        can_label->offset = offsetof(inv_set00_struct,DC_SET);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set00 + offsetof(inv_set00_struct, DC_SET);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set00 + offsetof(inv_set00_struct, DC_SET);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2000 = 1;
        }
    }

    return iReadNum;
}


/*------------------------------------------------------------------------------
 Function: Get_Modbus2200_To_Can_Offset_Len
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取modbus协议2200段转Can后协议偏移及寄存器�-
                ��度
  * @param[in]  uint16_t iReadAddr         
                int remain_reg_cnt         
                can_data_label *can_label  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t Get_Modbus2200_To_Can_Offset_Len(uint16_t iReadAddr, int remain_reg_cnt, can_data_label *can_label)
{
    uint16_t iReadNum = 0;
    uint16_t templen = 0;
    uint8_t *temp_input = 0;
    uint8_t *temp_store = 0;

    /* 2200-2300 -> 0x1b */ //高级设置区
    // 2200-2203
    if(iReadAddr <= 2203 && iReadAddr >= 2200) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,password), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,password), 
            sizeof(top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.password));
        can_label->offset = ((iReadAddr - 2200) * 2) + offsetof(inv_set01_struct,password);
        
        iReadNum = ((2203 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2203 - iReadAddr + 1);

        templen = sizeof(can_node_wr.Inv.inv_set01.password);
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, password);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, password);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }   
    // 2206 
    else if(iReadAddr == 2206) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_reset_factory), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_reset_factory), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_reset_factory);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_reset_factory);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_reset_factory);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.reset = 1; //恢复出厂设置
        }
    }
    // 2207 
    else if(iReadAddr == 2207) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_grid), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_grid);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_grid);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_grid);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2208 
    else if(iReadAddr == 2208) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_feedback), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_feedback), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_feedback);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_feedback);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_feedback);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            //reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2209-2217    
    else if(iReadAddr <= 2217 && iReadAddr >= 2209) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_output_inv_volt), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_output_inv_volt), 
            (offsetof(MOD_STRUCT_reg02200,ctrl_user_area) - offsetof(MOD_STRUCT_reg02200,ctrl_output_inv_volt)));
        can_label->offset = ((iReadAddr - 2209) * 2) + offsetof(inv_set01_struct,ctrl_output_inv_volt);

        iReadNum = ((2217 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2217 - iReadAddr + 1);

        templen = (offsetof(inv_set01_struct,ctrl_user_area) - offsetof(inv_set01_struct, ctrl_output_inv_volt));
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_output_inv_volt);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_output_inv_volt);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2218 
    else if(iReadAddr == 2218) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_user_area), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_user_area), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_user_area);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_user_area);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_user_area);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2219-2224
    else if(iReadAddr <= 2224 && iReadAddr >= 2219) //can_label->offset = ((iReadAddr - 2219) * 2) + offsetof(inv_set01_struct,ctrl_pv_paralle);
    {
        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
        
        switch(iReadAddr)
        {
            case 2219 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[0]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[0]), 1);
                can_label->offset = ((iReadAddr - 2219)) + offsetof(inv_set01_struct,ctrl_pv_paralle[0]); 
                templen = 1;
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[0]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[0]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
                }
                break;
            case 2220 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[1]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[1]), 1);
                can_label->offset = ((iReadAddr - 2220)) + offsetof(inv_set01_struct,ctrl_pv_paralle[1]);
                templen = 1;
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[1]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[1]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
                }
                break;
            case 2221 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[2]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[2]), 1);
                can_label->offset = ((iReadAddr - 2221)) + offsetof(inv_set01_struct,ctrl_pv_paralle[2]);
                templen = 1;
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[2]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[2]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
                }
                break;
            case 2222 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[3]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[3]), 1);
                can_label->offset = ((iReadAddr - 2222)) + offsetof(inv_set01_struct,ctrl_pv_paralle[3]);
                templen = 1;
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[3]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[3]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
                }
                break;
            case 2223 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[4]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[4]), 1);
                can_label->offset = ((iReadAddr - 2223)) + offsetof(inv_set01_struct,ctrl_pv_paralle[4]);
                templen = 1;
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[4]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[4]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
                }
                break;
            case 2224 : 
                memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[5]), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[5]), 1);
                can_label->offset = ((iReadAddr - 2224)) + offsetof(inv_set01_struct,ctrl_pv_paralle[5]);
                templen = 1;
                temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[5]);
                temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[5]);
                if(memcmp(temp_input, temp_store, templen) != 0)
                {
                    memcpy(temp_store, temp_input, templen);
                    reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
                }
                break;
        }
    }
    // 2225 
    else if(iReadAddr == 2225) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_grid_plus), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid_plus), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_grid_plus);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_grid_plus);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_grid_plus);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2226 
    else if(iReadAddr == 2226) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_save_power_state), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_save_power_state), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_save_power_state);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_save_power_state);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_save_power_state);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2227 
    else if(iReadAddr == 2227) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_meter_enable), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_enable), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_meter_enable);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_enable);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_enable);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2228 
    else if(iReadAddr == 2228) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_meter_select), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_select), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_meter_select);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_select);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_select);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2229 
    else if(iReadAddr == 2229) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_Multi_enable), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_Multi_enable), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_Inv_Multi_enable);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_Multi_enable);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_Multi_enable);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2230 
    else if(iReadAddr == 2230) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_addr_Set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_Inv_addr_Set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 1;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_addr_Set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_addr_Set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2231
    else if(iReadAddr == 2231) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ct_test), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_test), 2);
        can_label->offset = offsetof(inv_set01_struct,ct_test);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ct_test);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ct_test);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2232
    else if(iReadAddr == 2232) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_mix), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_mix), 2);
        can_label->offset = offsetof(inv_set01_struct,ctrl_mix);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_mix);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_mix);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2241 
    else if(iReadAddr == 2241) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ems_ctrl), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ems_ctrl), 2);
        can_label->offset = offsetof(inv_set01_struct,ems_ctrl);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ems_ctrl);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ems_ctrl);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2242 
    else if(iReadAddr == 2242) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ctrl_mix2), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_mix2), 1);
        can_label->offset = offsetof(inv_set01_struct,ctrl_mix2);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ctrl_mix2);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ctrl_mix2);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2243-2244    
    else if(iReadAddr <= 2244 && iReadAddr >= 2243) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,ChargingPile_SET), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ChargingPile_SET), 
            (offsetof(MOD_STRUCT_reg02200,ac_ct_test) - offsetof(MOD_STRUCT_reg02200, ChargingPile_SET)));
        can_label->offset = ((iReadAddr - 2243) * 2) + offsetof(inv_set01_struct,ChargingPile_SET);
        
        iReadNum = ((2244 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2244 - iReadAddr + 1);

        templen = 4;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, ChargingPile_SET);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, ChargingPile_SET);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2245
#ifndef CONFIG_SMART_GENERATOR_ENABLE    
    // 2246
    else if(iReadAddr == 2246) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, GenSet), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200, GenSet), 2);
        can_label->offset = offsetof(inv_set01_struct, GenSet);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, GenSet);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, GenSet);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    else if(iReadAddr == 2247) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Gen_SOC_Start), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200, Gen_SOC_Start), 2);
        can_label->offset = offsetof(inv_set01_struct, Gen_SOC_Start);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Gen_SOC_Start);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Gen_SOC_Start);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    else if(iReadAddr == 2248) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Gen_SOC_Stop), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200, Gen_SOC_Stop), 2);
        can_label->offset = offsetof(inv_set01_struct, Gen_SOC_Stop);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Gen_SOC_Stop);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Gen_SOC_Stop);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2249-2251    
    else if(iReadAddr <= 2251 && iReadAddr >= 2249) 
    {
        iReadNum = ((2251 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2251 - iReadAddr + 1);
        templen = 4;
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[0]), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200, SetTimeCtrl[0]), 
            templen);
        can_label->offset = ((iReadAddr - 2249) * 2) + offsetof(inv_set01_struct, SetTimeCtrl[0]);
        
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[0]);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[0]);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2249-2251    
    else if(iReadAddr <= 2251 && iReadAddr >= 2249) 
    {
        iReadNum = ((2251 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2251 - iReadAddr + 1);
        templen = 4;
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[0]), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200, SetTimeCtrl[0]), 
            templen);
        can_label->offset = ((iReadAddr - 2249) * 2) + offsetof(inv_set01_struct, SetTimeCtrl[0]);
        
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[0]);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[0]);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2252-2254    
    else if(iReadAddr <= 2254 && iReadAddr >= 2252) 
    {
        iReadNum = ((2254 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2254 - iReadAddr + 1);
        templen = 4;
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[1]), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200, SetTimeCtrl[1]), 
            templen);
        can_label->offset = ((iReadAddr - 2252) * 2) + offsetof(inv_set01_struct, SetTimeCtrl[1]);
        
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[1]);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[1]);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2255-2257    
    else if(iReadAddr <= 2257 && iReadAddr >= 2255) 
    {
        iReadNum = ((2257 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2257 - iReadAddr + 1);
        templen = 4;
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[2]), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200, SetTimeCtrl[2]), 
            templen);
        can_label->offset = ((iReadAddr - 2255) * 2) + offsetof(inv_set01_struct, SetTimeCtrl[2]);
        
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[2]);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, SetTimeCtrl[2]);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
#endif    
    // 2258 
    else if(iReadAddr == 2258) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection), 2);
        can_label->offset = offsetof(inv_set01_struct,Undervoltage_protection);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2259
    else if(iReadAddr == 2259) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection_time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection_time), 2);
        can_label->offset = offsetof(inv_set01_struct,Undervoltage_protection_time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2260
    else if(iReadAddr == 2260) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection), 2);
        can_label->offset = offsetof(inv_set01_struct,Highvoltage_protection);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2261
    else if(iReadAddr == 2261) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection_time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection_time), 2);
        can_label->offset = offsetof(inv_set01_struct,Highvoltage_protection_time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2262
    else if(iReadAddr == 2262) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection), 2);
        can_label->offset = offsetof(inv_set01_struct,Underfrequency_protection);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2263
    else if(iReadAddr == 2263) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection_time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection_time), 2);
        can_label->offset = offsetof(inv_set01_struct,Underfrequency_protection_time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2264
    else if(iReadAddr == 2264) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection), 2);
        can_label->offset = offsetof(inv_set01_struct,Overvoltage_protection);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2265
    else if(iReadAddr == 2265) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection_time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection_time), 2);
        can_label->offset = offsetof(inv_set01_struct,Overvoltage_protection_time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection_time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection_time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2266
    // 2267
    else if(iReadAddr == 2267) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,SetCtrlMeter), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter), 2);
        can_label->offset = offsetof(inv_set01_struct,SetCtrlMeter);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetCtrlMeter);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, SetCtrlMeter);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2268
    // 2269
    else if(iReadAddr == 2269) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,setting_pv), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetCtrlPv), 2);
        can_label->offset = offsetof(inv_set01_struct,setting_pv);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, setting_pv);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, setting_pv);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2270
    else if(iReadAddr == 2270) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,Phase_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Phase_set), 2);
        can_label->offset = offsetof(inv_set01_struct,Phase_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, Phase_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, Phase_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2271
    else if(iReadAddr == 2271) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,DCHUB_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,DCHUB_set), 2);
        can_label->offset = offsetof(inv_set01_struct,DCHUB_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, DCHUB_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, DCHUB_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2272
    else if(iReadAddr == 2272) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,SetGridMaxCurrent_in), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetGridMaxCurrent_in), 2);
        can_label->offset = offsetof(inv_set01_struct,SetGridMaxCurrent_in);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetGridMaxCurrent_in);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, SetGridMaxCurrent_in);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2273
    else if(iReadAddr == 2273) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,FUNC_set), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,FUNC_set), 2);
        can_label->offset = offsetof(inv_set01_struct,FUNC_set);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, FUNC_set);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, FUNC_set);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2274
    else if(iReadAddr == 2274) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,RvSettings), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvSettings), 2);
        can_label->offset = offsetof(inv_set01_struct,RvSettings);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, RvSettings);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, RvSettings);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2275
    else if(iReadAddr == 2275) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,AltDelays_ShutdownTime), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,AltDelays_ShutdownTime), 2);
        can_label->offset = offsetof(inv_set01_struct,AltDelays_ShutdownTime);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, AltDelays_ShutdownTime);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, AltDelays_ShutdownTime);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2276
    else if(iReadAddr == 2276) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,RvOnOff), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvOnOff), 2);
        can_label->offset = offsetof(inv_set01_struct,RvOnOff);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, RvOnOff);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, RvOnOff);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2277-2278    
    else if(iReadAddr <= 2278 && iReadAddr >= 2277) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,RvBatteryCapacity), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvBatteryCapacity), 4);
        can_label->offset = ((iReadAddr - 2277) * 2) + offsetof(inv_set01_struct,RvBatteryCapacity);
        
        iReadNum = ((2278 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2278 - iReadAddr + 1);

        templen = 4;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, RvBatteryCapacity);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, RvBatteryCapacity);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2279
    else if(iReadAddr == 2279) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,BatterySettings), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,BatterySettings), 2);
        can_label->offset = offsetof(inv_set01_struct,BatterySettings);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, BatterySettings);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, BatterySettings);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2282
    else if(iReadAddr == 2282) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,heat_pump_auto_set_Active_Power), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,heat_pump_auto_set_Active_Power), 2);
        can_label->offset = offsetof(inv_set01_struct,heat_pump_auto_set_Active_Power);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, heat_pump_auto_set_Active_Power);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, heat_pump_auto_set_Active_Power);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2283
    else if(iReadAddr == 2283) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,heat_pump_auto_set_minimum_Runtime), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,heat_pump_auto_set_minimum_Runtime), 2);
        can_label->offset = offsetof(inv_set01_struct,heat_pump_auto_set_minimum_Runtime);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, heat_pump_auto_set_minimum_Runtime);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, heat_pump_auto_set_minimum_Runtime);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2284
    else if(iReadAddr == 2284) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,heat_pump_auto_set_Delay_Start_Time), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,heat_pump_auto_set_Delay_Start_Time), 2);
        can_label->offset = offsetof(inv_set01_struct,heat_pump_auto_set_Delay_Start_Time);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, heat_pump_auto_set_Delay_Start_Time);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, heat_pump_auto_set_Delay_Start_Time);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2303
    else if(iReadAddr == 2303) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,SetFeedbackMode), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetFeedbackMode), 2);
        can_label->offset = offsetof(inv_set01_struct,SetFeedbackMode);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, SetFeedbackMode);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, SetFeedbackMode);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2304
    else if(iReadAddr == 2304) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,PvMultiPeak), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,PvMultiPeak), 2);
        can_label->offset = offsetof(inv_set01_struct,PvMultiPeak);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, PvMultiPeak);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, PvMultiPeak);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2305
    else if(iReadAddr == 2305) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,inv_meter_interaction), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,inv_meter_interaction), 2);
        can_label->offset = offsetof(inv_set01_struct,inv_meter_interaction);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, inv_meter_interaction);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, inv_meter_interaction);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }
    // 2306
    else if(iReadAddr == 2306) 
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct,AC_Input_Ctrl), 
            (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,AC_Input_Ctrl), 2);
        can_label->offset = offsetof(inv_set01_struct,AC_Input_Ctrl);

        iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

        templen = 2;
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set01 + offsetof(inv_set01_struct, AC_Input_Ctrl);
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set01 + offsetof(inv_set01_struct, AC_Input_Ctrl);
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2200 = 1;
        }
    }

    return iReadNum;
}

#if 0

/*------------------------------------------------------------------------------
 Function: Get_Modbus2300_To_Can_Offset_Len
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取modbus协议2300段转Can后协议偏移及寄存器�-
                ��度
  * @param[in]  uint16_t iReadAddr         
                int remain_reg_cnt         
                can_data_label *can_label  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t Get_Modbus2300_To_Can_Offset_Len(uint16_t iReadAddr, int remain_reg_cnt, can_data_label *can_label)
{
    uint16_t iReadNum = 0;
    uint16_t templen = 0;
    uint8_t *temp_input = 0;
    uint8_t *temp_store = 0;

    /* 2300-2400 -> 0x1c */ //电网认证区(保留)
    // 2300-2325
    if(iReadAddr >= 2300 && iReadAddr <= 2325)
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set02, (uint8_t *)&top_modbus_wr.Inv.mod_reg02300_Inv_set02_struct, sizeof(top_modbus_wr.Inv.mod_reg02300_Inv_set02_struct));
        can_label->offset = ((iReadAddr - 2300) * 2);
            
        iReadNum = ((2325 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2325 - iReadAddr + 1);
        
        templen = sizeof(top_modbus_wr.Inv.mod_reg02300_Inv_set02_struct);
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set02;
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set02;
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2300 = 1;
        }
    }

    return iReadNum;
}

#endif

/*------------------------------------------------------------------------------
 Function: Get_Modbus2400_To_Can_Offset_Len
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取modbus协议2400段转Can后协议偏移及寄存器�-
                ��度
  * @param[in]  uint16_t iReadAddr         
                int remain_reg_cnt         
                can_data_label *can_label  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t Get_Modbus2400_To_Can_Offset_Len(uint16_t iReadAddr, int remain_reg_cnt, can_data_label *can_label)
{
    uint16_t iReadNum = 0;
    uint16_t templen = 0;
    uint8_t *temp_input = 0;
    uint8_t *temp_store = 0;

    /* 2400-2500 -> 0x1d */ //电网认证区
    // 2400-2440
    if(iReadAddr >= 2400 && iReadAddr <= 2440)
    {
        memcpy((uint8_t *)&can_node_wr.Inv.inv_set03, (uint8_t *)&top_modbus_wr.Inv.mod_reg02400_Inv_certification, sizeof(top_modbus_wr.Inv.mod_reg02400_Inv_certification));
        can_label->offset = ((iReadAddr - 2400) * 2);
            
        iReadNum = ((2440 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2440 - iReadAddr + 1);
    
        templen = sizeof(top_modbus_wr.Inv.mod_reg02400_Inv_certification);
        temp_input = (uint8_t *)&can_node_wr.Inv.inv_set03;
        temp_store = (uint8_t *)&InvSetData.inv_can_info_t.inv_set03;
        if(memcmp(temp_input, temp_store, templen) != 0)
        {
            memcpy(temp_store, temp_input, templen);
            reals.INVFlasWrFlag.sBit.set_data_inv_2400 = 1;
        }
    }

    return iReadNum;
}

/*------------------------------------------------------------------------------
 Function: Modbus_To_CAN_Write_Info_After_Process
 -----------------------------------------------------------------------------*/
/**
  * @brief      执行接收后，将modbus INV全局变量赋值给CAN
                INV全局变量地址
  * @param[in]  uint16_t iReadAddr         
                uint16_t writeRegsCnt      
                can_data_label *can_label  
                uint16_t *cmd_num          
  * @param[out] None
  * @return     void
  */
void Modbus_To_CAN_Write_Info_After_Process(uint16_t iReadAddr, uint16_t writeRegsCnt, can_data_label *can_label, uint16_t *cmd_num)//app_param_handler
{
    uint16_t start = 0;
    uint16_t i = 0;
    uint16_t j = 0;

    int remain_cmd_num = *cmd_num;
    int remain_reg_cnt = writeRegsCnt;
    int send_cmd_num = 0;
    
    uint16_t iReadNum = 0;

	/* 将modbus表的值赋给CAN表 并返回CAN表的偏移量 并修正写入字节数*/
    while((remain_cmd_num > send_cmd_num) && (remain_reg_cnt > 0))
    {
        iReadNum = 0;
        can_label->type = 0;
        can_label->offset = 0;
        can_label->active_can_cmd_type = 0;
        can_label->len = 0;
       
        /*判断指令类型及长度*/
        if((iReadAddr >= MOD_REG_START_ADDR_02000) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000)))
        {
            start = MOD_REG_START_ADDR_02000;
            iReadNum = Get_Modbus2000_To_Can_Offset_Len(iReadAddr, remain_reg_cnt, can_label);
                
            can_label->type = INV_TYPE_CONFIG00_1AH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02000[%d]=%d",i,j,i,MOD_STRUCT_len_reg02000[i]);
                can_label->len += MOD_STRUCT_len_reg02000[i];            
            }
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_02200) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200)))
        {
            start = MOD_REG_START_ADDR_02200;
            iReadNum = Get_Modbus2200_To_Can_Offset_Len(iReadAddr, remain_reg_cnt, can_label);
            
            can_label->type = INV_TYPE_CONFIG01_1BH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02200[%d]=%d",i,j,i,MOD_STRUCT_len_reg02200[i]);
                can_label->len += MOD_STRUCT_len_reg02200[i];            
            }
        }
#if 0  // 已冲突       
        else if((iReadAddr >= MOD_REG_START_ADDR_02300) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_02300 + MOD_REG_LEN_02300)))
        {
            start = MOD_REG_START_ADDR_02300;
            iReadNum = Get_Modbus2300_To_Can_Offset_Len(iReadAddr, remain_reg_cnt, can_label);
            
            can_label->type = INV_TYPE_CONFIG02_1CH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02300[%d]=%d",i,j,i,MOD_STRUCT_len_reg02300[i]);
                can_label->len += MOD_STRUCT_len_reg02300[i];            
            }
        }
#endif
        else if((iReadAddr >= MOD_REG_START_ADDR_02400) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400)))
        {
            start = MOD_REG_START_ADDR_02400;
            iReadNum = Get_Modbus2400_To_Can_Offset_Len(iReadAddr, remain_reg_cnt, can_label);
            
            can_label->type = INV_TYPE_CONFIG03_1DH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02400[%d]=%d",i,j,i,MOD_STRUCT_len_reg02400[i]);
                can_label->len += MOD_STRUCT_len_reg02400[i];            
            }
        }
#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE
        else if((iReadAddr >= MOD_REG_START_ADDR_07000) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_07000 + MOD_REG_LEN_07000)))
        {
            can_label->active_can_cmd_type = 0; //tbd
            can_label->type = PACK_TYPE_CONFIG_55H;
            
            if ( iReadAddr == 7001 ) {
                iReadNum = 1;
                can_label->len = 1;    
                can_label->offset = offsetof(pack_config_struct, pack_heat_enable);
                can_node_wr.Pack.pack_config.pack_heat_enable = top_modbus_wr.Pack.mod_reg07000_Pack_set.SetPvHeat;
            } else if ( iReadAddr == 7002 ) {
                iReadNum = 1;
                can_label->len = 1;    
                can_label->offset = offsetof(pack_config_struct, ctr_heat_enable);
                can_node_wr.Pack.pack_config.ctr_heat_enable = top_modbus_wr.Pack.mod_reg07000_Pack_set.SetPackHeat;
            } else if ( iReadAddr == 7003 ) {
                iReadNum = 1;
                can_label->len = 1;    
                can_label->offset = offsetof(pack_config_struct, unlock_failed_flags);
                can_node_wr.Pack.pack_config.unlock_failed_flags = top_modbus_wr.Pack.mod_reg07000_Pack_set.SetPackUnlock;
            } else if ( iReadAddr == 7004 ) {
                iReadNum = 1;
                can_label->len = 1;    
                can_label->offset = offsetof(pack_config_struct, max_parallel_nums);
                can_node_wr.Pack.pack_config.max_parallel_nums = top_modbus_wr.Pack.mod_reg07000_Pack_set.PACK_Parallel_InOneGroup;
            } else if (( iReadAddr >= 7005 ) && ( iReadAddr <= 7012)) {
                start = 7005;
                iReadNum = ((7012 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (7012 - iReadAddr + 1);
                can_label->len = iReadNum * 2;    
                can_label->offset = offsetof(pack_config_struct, on_off_set) + (iReadAddr - start) * 2;
                memcpy((uint8_t *)&can_node_wr.Pack.pack_config + can_label->offset, 
                    (uint8_t *)&top_modbus_wr.Pack.mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000, on_off_set) + (iReadAddr - start) * 2,
                    can_label->len);
            }
        }
#endif
        else if((iReadAddr >= MOD_REG_START_ADDR_12000) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000)))
        {
            start = MOD_REG_START_ADDR_12000;
            iReadNum = remain_reg_cnt;
            
            can_label->type = IOT_TYPE_SET_02H;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;    
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            memcpy((uint8_t *)&can_node_wr.Iot.mod_reg12000_IOT_set + (iReadAddr -start)*2, (uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set + (iReadAddr -start)*2,can_label->len);
        }
#if 0        
        else if((iReadAddr >= MOD_REG_START_ADDR_15600) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600)))
        {
            start = MOD_REG_START_ADDR_15600;
            iReadNum = remain_reg_cnt;
            
            can_label->type = MODULE_TYPE_DCDC_SET_49H;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;    
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            memcpy((uint8_t *)&can_node_wr.Dcdc.dcdc_common_set + (iReadAddr -start)*2, (uint8_t *)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set + (iReadAddr -start)*2,can_label->len);
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_15750) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_15750 + MOD_REG_LEN_15750)))
        {
            start = MOD_REG_START_ADDR_15750;
            iReadNum = remain_reg_cnt;
            
            can_label->type = MODULE_TYPE_DC_AC_HUB_SET_40H;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;    
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            memcpy((uint8_t *)&can_node_wr.HUB.dc_ac_hub_setting + (iReadAddr -start)*2, (uint8_t *)&top_modbus_wr.DcHub.mod_reg15750_Dc_Ac_Hub_set + (iReadAddr -start)*2,can_label->len);
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_19000) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000)))
        {
            start = MOD_REG_START_ADDR_19000;
            iReadNum = remain_reg_cnt;
            
            can_label->type = MODULE_TYPE_POWER_RELAY_SET1_4BH;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;    
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            memcpy((uint8_t *)&Inv_can_mix_WR.PowerRelay_SmartCtrl_Set01 + (iReadAddr -start)*2, (uint8_t *)&top_modbus_wr.Inv.mod_reg19000_PowerRelay_Set01 + (iReadAddr -start)*2,can_label->len);
            Panel_Setting_Get_To_Setdata(PANEL_SET_FROM_MODBUS, POWERRELAY_SMART_SET01, ((iReadAddr - start)*2), (iReadNum*2));
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_19100) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_19100 + MOD_REG_LEN_19100)))
        {
            start = MOD_REG_START_ADDR_19100;
            iReadNum = remain_reg_cnt;
            
            #ifdef POWERRELAY_SMART_SET02_SYNCHRONOUS   //延时设置当前为一次性事件，暂不转发
            
            can_label->type = MODULE_TYPE_POWER_RELAY_SET2_4CH;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;    
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            memcpy((uint8_t *)&Inv_can_mix_WR.PowerRelay_SmartCtrl_Set02 + (iReadAddr -start)*2, (uint8_t *)&top_modbus_wr.Inv.mod_reg19100_PowerRelay_Set02 + (iReadAddr -start)*2,can_label->len);

            #endif
            
            Panel_Setting_Get_To_Setdata(PANEL_SET_FROM_MODBUS, POWERRELAY_SMART_SET02, ((iReadAddr - start)*2), (iReadNum*2));
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_19300) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_19300 + MOD_REG_LEN_19300)))
        {
            start = MOD_REG_START_ADDR_19300;
            iReadNum = remain_reg_cnt;
            
            can_label->type = MODULE_TYPE_POWER_RELAY_SET3_4DH;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;    
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            memcpy((uint8_t *)&Inv_can_mix_WR.PowerRelay_SmartCtrl_Set03 + (iReadAddr -start)*2, (uint8_t *)&top_modbus_wr.Inv.mod_reg19300_PowerRelay_Set03 + (iReadAddr -start)*2,can_label->len);
            Panel_Setting_Get_To_Setdata(PANEL_SET_FROM_MODBUS, POWERRELAY_SMART_SET03, ((iReadAddr - start)*2), (iReadNum*2));
        }
#endif        
        else if((iReadAddr >= MOD_REG_START_ADDR_30000) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_30000 + MOD_REG_LEN_30000)))
        {
            can_label->active_can_cmd_type = 0; //tbd
            can_label->type = PACK_TYPE_BMS_PAYGO_INFO_5BH;
            
            if (( iReadAddr >= 30001 ) && ( iReadAddr <= 30007)) {
                start = 30001;
                iReadNum = ((30007 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (30007 - iReadAddr + 1);
                can_label->len = iReadNum * 2;    
                can_label->offset = offsetof(bms_paygo_info_t, PAYG_ActCodeInput) + (iReadAddr - start) * 2;
                memcpy((uint8_t *)&can_node_wr.Pack.pack_paygo + can_label->offset, 
                    (uint8_t *)&top_modbus_wr.Pack.mod_reg30000_Pack_paygo + offsetof(MOD_STRUCT_reg30000, PAYG_ActCodeInput) + (iReadAddr - start) * 2,
                    can_label->len);
            } else if (( iReadAddr >= 30012 ) && ( iReadAddr <= 30017)) {
                start = 30012;
                iReadNum = ((30017 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (30017 - iReadAddr + 1);
                can_label->len = iReadNum * 2;    
                can_label->offset = offsetof(bms_paygo_info_t, PAYG_LastActTime) + (iReadAddr - start) * 2;
                memcpy((uint8_t *)&can_node_wr.Pack.pack_paygo + can_label->offset, 
                    (uint8_t *)&top_modbus_wr.Pack.mod_reg30000_Pack_paygo + offsetof(MOD_STRUCT_reg30000, PAYG_LastActTime) + (iReadAddr - start) * 2,
                    can_label->len);
            } else if (( iReadAddr >= 30022 ) && ( iReadAddr <= 30029)) {
                start = 30022;
                iReadNum = ((30029 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (30029 - iReadAddr + 1);
                can_label->len = iReadNum * 2;    
                can_label->offset = offsetof(bms_paygo_info_t, CurrentTime) + (iReadAddr - start) * 2;
                memcpy((uint8_t *)&can_node_wr.Pack.pack_paygo + can_label->offset, 
                    (uint8_t *)&top_modbus_wr.Pack.mod_reg30000_Pack_paygo + offsetof(MOD_STRUCT_reg30000, current_saved_timestamp) + (iReadAddr - start) * 2,
                    can_label->len);
            }
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_40000) && ((iReadAddr + remain_reg_cnt) <= (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000)))
        {
            start = MOD_REG_START_ADDR_40000;
            iReadNum = remain_reg_cnt;
            
            can_label->type = COMMON_TYPE_TRANSMIT_27H;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;    
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            memcpy((uint8_t *)&can_node_wr.Iot.common_transmission + (iReadAddr -start)*2, (uint8_t *)&top_modbus_wr.Inv.mod_reg40000_transparent + (iReadAddr -start)*2,can_label->len);
        }

        if ( can_label->len > 0)
        {
            ESP_LOGI(TAG, "modbus to can: type:0x%x, offset:%d, len:%d, iReadAddr:%d", can_label->type, can_label->offset, can_label->len, iReadAddr);
            // 将指针向后移动一个 can_data_label 大小
            if ( remain_cmd_num > send_cmd_num )
            {
                can_label = can_label + 1;
                send_cmd_num++;
            }
        }
        else
        {
            // CAN指令长度无效，继续访问
            can_label->type = 0;
            can_label->offset = 0;
            can_label->active_can_cmd_type = 0;
            if (!iReadNum)//无效原因为iReadNum，直接退出，访问地址有误
            {
                break;
            }
        }

        /*判断下次循环相关*/
        remain_reg_cnt -= iReadNum;
        iReadAddr += iReadNum;
    }

    *cmd_num = send_cmd_num;
}
#endif

/*------------------------------------------------------------------------------
 Function: Modbus_To_Wireless_Write_Info_After_Process
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus设置项无线转发
  * @param[in]  uint8_t *income              
                int len                      
  * @param[out] None
  * @return     void
  */
void Modbus_To_Wireless_Write_Info_After_Process(uint16_t regAddress, uint8_t reg_Num, uint8_t slave_address, int chl)
{
    // 仅限特定从机地址范围内的非21000格式的标准Modbus
    if((regAddress != MOD_REG_START_ADDR_21000) 
        && (slave_address >= DEV_MODBUS_ADDR_START) 
        && (slave_address <= DEV_MODBUS_ADDR_END)) 
    {
        modbus_addr_info_t src_addr = {
            .channel = chl,
            .slaveAddr = slave_address,
            .regAddr = regAddress,
            .regNum = reg_Num,
        };

		// 其他设备的地址
		sMdAddrInfo_t devInfo = dev_modbus_get_dev_info_from_addr(src_addr.slaveAddr);
    	uint8_t *modbus_buffer = Modbus_To_Wireless_Write_buffer;
        
		// 设备是某个设备的子设备处理，只需要处理单级即可,每个设备只对下一级负责
		uint8_t Send_SlaveAddress = 1;
		if(devInfo.parent_type && devInfo.parent_sn){
			Send_SlaveAddress = devInfo.addr_in_parent;
		}

		if(1 == Send_SlaveAddress) {
			switch(devInfo.dev_type) {
				case SN_TYPE_INV_EL300:
				case SN_TYPE_INV_AP300:
					Send_SlaveAddress = 0;
					break;
			}
		}
        
		// 修改modbus地址，重新组modbus帧
        uint16_t tx_len = Modbus_MasterWriteCmd_06H_10H(src_addr.regAddr, src_addr.regNum, true, modbus_buffer, Send_SlaveAddress, src_addr.channel);
            
		switch(devInfo.channel)
		{
			case MD_CHL_BLE_CLIENT:
#ifdef BLE_CLIENT_ENABLE                
				ble_c2s_send_to_queue(modbus_buffer, tx_len, src_addr);
                ESP_LOGI(TAG, "Modbus to BleClient(%d)(%llu) : %d bytes.", devInfo.dev_type, devInfo.dev_sn, tx_len);
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, modbus_buffer, tx_len, ESP_LOG_INFO);
#endif
				break;

			case MD_CHL_WIFI_WLCC:
#ifdef CONFIG_WLCC_ENABLE                
				wlcc_modbus_msg_send(modbus_buffer, tx_len, devInfo.dev_type, devInfo.dev_sn, src_addr);
                ESP_LOGI(TAG, "Modbus to Wlcc(%d)(%llu) : %d bytes.", devInfo.dev_type, devInfo.dev_sn, tx_len);
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, modbus_buffer, tx_len, ESP_LOG_INFO);
#endif
                break;

			default:
                ESP_LOGE(TAG, "Modbus_To_Wireless_Write_Info_After_Process : Invalid Dev Chl(%d)", devInfo.channel);
				break;
		}
	}
}

/*伪装寄存器长度*/
#define MAX_REGCNT_50   50
#define MAX_REGCNT_100  100
#define MAX_REGCNT_200  200
#define MAX_REGCNT_400  400
#define MAX_REGCNT_700  700
#define MAX_REGCNT_1000 1000

// 寄存器段配置结构体
typedef struct {
    uint16_t start_addr;      // 起始地址
    uint16_t real_len;        // 实际长度
    uint16_t max_len;         // 最大伪装长度
} modbus_reg_segment_t;

// 寄存器段配置表
static const modbus_reg_segment_t reg_segments[] = {
    {MOD_REG_START_ADDR_00100, MOD_REG_LEN_00100, MAX_REGCNT_200},
    {MOD_REG_START_ADDR_01100, MOD_REG_LEN_01100, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_01200, MOD_REG_LEN_01200, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_01300, MOD_REG_LEN_01300, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_01400, MOD_REG_LEN_01400, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_01500, MOD_REG_LEN_01500, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_01600, MOD_REG_LEN_01600, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_02000, MOD_REG_LEN_02000, MAX_REGCNT_200},
    {MOD_REG_START_ADDR_02200, MOD_REG_LEN_02200, MAX_REGCNT_200},
    {MOD_REG_START_ADDR_03700, MOD_REG_LEN_03700, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_06000, MOD_REG_LEN_06000, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_06100, MOD_REG_LEN_06100, MAX_REGCNT_200},
    {MOD_REG_START_ADDR_07000, MOD_REG_LEN_07000, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_11000, MOD_REG_LEN_11000, MAX_REGCNT_1000},
    {MOD_REG_START_ADDR_12000, MOD_REG_LEN_12000, MAX_REGCNT_1000},
    {MOD_REG_START_ADDR_13600, MOD_REG_LEN_13600, MAX_REGCNT_400},
    {MOD_REG_START_ADDR_14500, MOD_REG_LEN_14500, MAX_REGCNT_200},
    {MOD_REG_START_ADDR_14700, MOD_REG_LEN_14700, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_15500, MOD_REG_LEN_15500, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_15600, MOD_REG_LEN_15600, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_15700, MOD_REG_LEN_15700, MAX_REGCNT_50},
    {MOD_REG_START_ADDR_15750, MOD_REG_LEN_15750, MAX_REGCNT_50},
    {MOD_REG_START_ADDR_19000, MOD_REG_LEN_19000, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_19100, MOD_REG_LEN_19100, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_19200, MOD_REG_LEN_19200, MAX_REGCNT_100},
    {MOD_REG_START_ADDR_19300, MOD_REG_LEN_19300, MAX_REGCNT_700},
    {MOD_REG_START_ADDR_29800, MOD_REG_LEN_29800, MAX_REGCNT_100},
};

static const size_t reg_segments_len = sizeof(reg_segments) / sizeof(reg_segments[0]);
    
/*------------------------------------------------------------------------------
 Function: Get_Modbus_Read_Real_And_Mask_Length
 -----------------------------------------------------------------------------*/
/**
 * @brief      获取本次Modbus读取的实际长度和伪装长度
 * @param[in]  startAddress           起始地址
 * @param[in]  readRegCnt            请求读取的寄存器数量
 * @param[out] reg_real_num          实际可读取的寄存器数量
 * @param[out] feign_reg_remain_num  伪装的剩余寄存器数量
 * @return     void
 */
void Get_Modbus_Read_Real_And_Mask_Length(uint16_t startAddress, uint16_t readRegCnt, 
                                                  uint16_t *reg_real_num, uint16_t *feign_reg_remain_num)
{
    // 默认值：正常交互，未伪装
    *reg_real_num = readRegCnt;
    *feign_reg_remain_num = 0;
    
    // 查找匹配的寄存器段
    for (size_t i = 0; i < reg_segments_len; i++) {
        const modbus_reg_segment_t *segment = &reg_segments[i];
        
        // 检查是否在当前段的伪装范围内
        if ((startAddress >= segment->start_addr) && 
            ((startAddress + readRegCnt) > (segment->start_addr + segment->real_len)) &&
            ((startAddress + readRegCnt) <= (segment->start_addr + segment->max_len))) {
            
            // 起始位置有效（在实际数据范围内）
            if (startAddress < segment->start_addr + segment->real_len) {
                *reg_real_num = segment->start_addr + segment->real_len - startAddress;
                *feign_reg_remain_num = readRegCnt - *reg_real_num;
            } 
            // 起始位置无效（超出实际数据范围）
            else {
                *reg_real_num = 0;
                *feign_reg_remain_num = readRegCnt;
            }
            return; // 找到匹配段，直接返回
        }
    }
    
    // 如果没有找到匹配的段，保持默认值（正常交互）
}

/*------------------------------------------------------------------------
*@Function :Modbus_ReadRegs 


*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] chl：输入来源

*@return		 
0- fail
no 0: tx len
*/
static uint16_t Modbus_ReadRegs(const uint8_t *income, uint16_t inLen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, int chl)
{
    uint16_t j = 0;
	uint16_t *dst = NULL;
	reg_position_t reg_position;
	const uint16_t *p_tab = NULL;
    
    uint8_t SlaveAddress = income[0];
    
    uint16_t startAddress = (income[2] << 8) | income[3];
    uint16_t readRegCnt   = (income[4] << 8) | income[5];

    if (response == NULL) {
        return 0;
    }
    
    if (inLen != 8) {
        return Modbus_Error(response, BAD_COUNT);
    }
    
    response[j++] = income[0];
    response[j++] = 0x03;
    response[j++] = (readRegCnt << 1); // 读取的字节长度
    
    uint16_t reg_real_num = 0;//实际读取寄存器长度
    uint16_t feign_reg_remain_num = 0;//剩余需伪装读取寄存器长度

#if 1
    /*获取本次Modbus读取的实际长度和伪装长度*/
    Get_Modbus_Read_Real_And_Mask_Length(startAddress, readRegCnt, &reg_real_num, &feign_reg_remain_num);
#else
    /*不进行伪装*/
    reg_real_num = readRegCnt;
#endif
        
    /*根据传输通道选择访问区域*/
    if ( 0 != reg_real_num ) {
        p_tab = vLookupDataTab( SlaveAddress, startAddress, reg_real_num, false, &reg_position, chl);
        
        if ( p_tab ) {
            /* 该数据由ota数据表处理 */
            const md_priv_data_t priv_data = {0};
            md_data_t *p_data = md_tbl_find(startAddress);
            if (p_data == NULL) {
                ESP_LOGE(TAG, "find register table failure, startAddress=%u;readRegCnt= %u", startAddress, reg_real_num);
                return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
            }
            
            /* 表回调函数 */
            if (p_data->tbl.tbl_cb)//检查
            {
                tbl_cb_data_t cb_data = {
                    .SlaveAddress = SlaveAddress,
                    .reg_addr_offset =reg_position.offset,
                    .reg_addr = startAddress,
                    .reg_nums = reg_real_num,
                    .is_write = false,
                    .cb_chl = chl,
                };
                
                if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0) {
                    ESP_LOGE(TAG, "Modbus_ReadRegs: tbl_cb failed, SlaveAddress=%u, startAddress=%u, readRegCnt=%u", SlaveAddress, startAddress, reg_real_num);
                    return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
                }
            }  
        } 
    }

    /*填充数据*/
    if((NULL != p_tab) 
        || ((0 == reg_real_num) && (0 != feign_reg_remain_num)))
    {
        if (modbus_data_semaphore_Take())
	    {
            dst = (uint16_t *)&response[3];

            /*组帧*/
            for (uint16_t i = 0; i < reg_real_num; i++, j += 2)
            {
                /* table中的数据 */
                dst[i] = LSB2MSB(p_tab[i]);
            }  
            for (uint16_t i = 0; i < feign_reg_remain_num; i++, j += 2) {
                /* 伪装数据 */
                dst[reg_real_num + i] = 0;
            } 
            modbus_data_semaphore_Give();
	    }
        else
        {
            ESP_LOGE(TAG, "Modbus_ReadRegs: ACK_MASTER_WAIT!");
            return Modbus_Error(response, ACK_MASTER_WAIT);
        }
    }   
    else
    {
        ESP_LOGE(TAG, "Modbus_ReadRegs: UNKNOWN_REG_ADDRESS, SlaveAddress=%u, startAddress=%u, readRegCnt=%u", SlaveAddress, startAddress, reg_real_num);
        return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
    }   

	uint16_t crc16 = ModbusCrc16(response, j);
	response[j++] = crc16;
	response[j++] = crc16 >> 8;
    return j;
}

/**
 * income:接收到的MODBUS指令,
 * inLen:MODBUS指令长度
 * response:MODBUS响应数据
 * cmd_label: MODBUS转换成CAN命令关系表,
 * cmd_num：命令数量
 *
 * return ：MODBUS响应的数据长度
*/
/*------------------------------------------------------------------------
*@Function :Modbus_WriteSingleReg 


*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

*@return		 
0- fail
no 0: tx len
*/
static uint16_t Modbus_WriteSingleReg(const uint8_t *income, uint16_t inLen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl, reg_position_list_t **position_list)
{
    uint16_t j = 0;
	reg_position_t reg_position;
	uint8_t SlaveAddress = 0;
	uint16_t *p_tab = NULL;

    if (position_list) {
        *position_list = NULL;
    }

	SlaveAddress = income[0];
	
    uint16_t startAddress = income[2]<<8 | income[3]; // 开始地址
    uint16_t writeRegData = income[4]<<8 | income[5]; // 写入的数据,数据已经交换lsb
	uint16_t writeRegsCnt=0;

    if(NULL != response)
    {
		response[j++] = income[0];
		response[j++] = 0x06;
	}


    if (inLen != 8) {
        return Modbus_Error(response, BAD_COUNT);
    }

	writeRegsCnt=1;
	p_tab = vLookupDataTab( SlaveAddress, startAddress, 1, true, &reg_position, chl);					// 查询table2中的数据
		
	if(p_tab)// 
	{
		*p_tab = writeRegData;
#ifdef CONFIG_UART_PORT_ENABLE
		if (position_list) //没有串口不需要
		{
			*position_list = NULL;
		}
	
		if (position_list) 
		{
			reg_position_list_t *new_position = (reg_position_list_t *)iot_calloc(sizeof(reg_position_list_t));
			if (new_position != NULL) 
			{
				new_position->next = NULL;
				new_position->position.dev_index = SlaveAddress;
				new_position->position.reg_addr = reg_position.reg_addr;
				new_position->position.offset = reg_position.offset;
				new_position->position.len	= reg_position.len;
	
				(*position_list) = new_position;
				ESP_LOGI(TAG, "dev_index: %02x, reg_num: %02x, offset: %02x, bytes: %02x", 
									(*position_list)->position.dev_index, (*position_list)->position.reg_addr,
									(*position_list)->position.offset, (*position_list)->position.len);
			}
		}			
#endif
	}
	else
	{
#ifdef CONFIG_UART_PORT_ENABLE
        Wifi_ble_to_arm_uart_transmit(income, inLen, response, chl);
#endif
        ESP_LOGE(TAG, "modbus to CAN single convert failed, addr: %u", startAddress);    
        return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
	}
    
#ifdef CONFIG_CAN_PORT_ENABLE
    if((NULL != cmd_num)&&(NULL != cmd_label)) {
		*cmd_num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
    	Modbus_To_CAN_Write_Info_After_Process(startAddress, writeRegsCnt, cmd_label, cmd_num);//testwx
	}   
#endif

    /*无线透传设置*/
    Modbus_To_Wireless_Write_Info_After_Process(startAddress, writeRegsCnt, SlaveAddress, chl);

	/* 该数据由ota数据表处理 */
	md_priv_data_t priv_data = {0};
	md_data_t *p_data = md_tbl_find(startAddress);
	if (p_data == NULL) {
		return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
	}

    /* 表回调函数 */
	if (p_data->tbl.tbl_cb)//检查
	{
		tbl_cb_data_t cb_data = {
			.SlaveAddress = SlaveAddress,
			.reg_addr_offset =reg_position.offset,
			.reg_addr = startAddress,
			.reg_nums = writeRegsCnt,
			.is_write = true,
			.cb_chl = chl,
		};

        if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
        {
            return 0;
        }
	}		
    if(NULL != response)
    {
		response[j++] = income[2];
		response[j++] = income[3];
		response[j++] = income[4];
		response[j++] = income[5];
		response[j++] = income[6];
		response[j++] = income[7];
	}

    return j;
}


/**
 * income:接收到的MODBUS指令,
 * inLen:MODBUS指令长度
 * response:MODBUS响应数据
 * cmd_label: MODBUS转换成CAN命令关系表,
 * cmd_num：命令数量
 *
 * return ：MODBUS响应的数据长度
*/
/*------------------------------------------------------------------------
*@Function :Modbus_WriteMultiRegs 


*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

*@return		 
0- fail
no 0: tx len
*/
static uint16_t Modbus_WriteMultiRegs(const uint8_t *income, uint16_t inLen, uint8_t *response,can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl, reg_position_list_t **position_list) 
{
    uint16_t i = 0;
    uint16_t j = 0;
	uint16_t *src =NULL;
	reg_position_t reg_position;
	uint16_t *p_tab = NULL;
	
    uint16_t writeRegData;
    uint8_t SlaveAddress = 0;

    if (position_list) {
        *position_list = NULL;
    }

	SlaveAddress = income[0];

    uint16_t startAddress  = income[2]<<8 | income[3]; // 写入寄存器地址
    uint16_t writeRegsCnt  = income[4]<<8 | income[5]; // 写入寄存器数量

    // response[j++] = DEFAULT_ADDRESS;
    if(NULL != response)
    {
		response[j++] = income[0];
		response[j++] = 0x10;
	}

    if (inLen < 9 || (inLen - 9) != (writeRegsCnt*2)) 
	{
        return Modbus_Error(response, BAD_COUNT);
    }


	p_tab = vLookupDataTab( SlaveAddress, startAddress, writeRegsCnt, true,&reg_position, chl);					// 查询table2中的数据
	src = (uint16_t *)(income + 7);

	if(p_tab)//微逆，uart
	{
		for ( i = 0; i < writeRegsCnt; i++) 
		{
			writeRegData = LSB2MSB(src[i]);
			*(p_tab+i) = writeRegData;
		}	
#ifdef CONFIG_UART_PORT_ENABLE
		if (position_list) //没有串口不需要
		{
			*position_list = NULL;
		}
	
		if (position_list) 
		{
			reg_position_list_t *new_position = (reg_position_list_t *)iot_calloc(sizeof(reg_position_list_t));
			if (new_position != NULL) 
			{
				new_position->next = NULL;
				new_position->position.dev_index = SlaveAddress;
				new_position->position.reg_addr = reg_position.reg_addr;
				new_position->position.offset = reg_position.offset;
				new_position->position.len	= reg_position.len;
	
				(*position_list) = new_position;
				ESP_LOGI(TAG, "dev_index: %02x, reg_num: %02x, offset: %02x, bytes: %02x", 
									(*position_list)->position.dev_index, (*position_list)->position.reg_addr,
									(*position_list)->position.offset, (*position_list)->position.len);
			}
		}		
#endif		
	}
	else
	{
#ifdef CONFIG_UART_PORT_ENABLE
        return Wifi_ble_to_arm_uart_transmit(income, inLen, response, chl);
#endif
		ESP_LOGE(TAG, "modbus to CAN multi convert failed, addr: %u", startAddress); 	
		return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
	}
    
#ifdef CONFIG_CAN_PORT_ENABLE
	if((NULL != cmd_num)&&(NULL != cmd_label)) {
		*cmd_num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
    	Modbus_To_CAN_Write_Info_After_Process(startAddress, writeRegsCnt, cmd_label, cmd_num);//testwx
	}
#endif

    /*无线透传设置*/
    Modbus_To_Wireless_Write_Info_After_Process(startAddress, writeRegsCnt, SlaveAddress, chl);

	/* 该数据由ota数据表处理 */
	md_priv_data_t priv_data = {0};
	md_data_t *p_data = md_tbl_find(startAddress);
	if (p_data == NULL) {
		return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
	}

    /* 表回调函数 */
	if (p_data->tbl.tbl_cb)//检查
	{
		tbl_cb_data_t cb_data = {
				.SlaveAddress = SlaveAddress,
				.reg_addr_offset =reg_position.offset,
				.reg_addr = startAddress,
				.reg_nums = writeRegsCnt,
				.is_write = true,
				.cb_chl = chl,
		};

        if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
        {
            return 0;
        }
	}		
	//ESP_LOGE(TAG, "windy writeX :tbl_cb 2,startAddress=%u ",startAddress );

    if(NULL != response)
    {
		response[j++] = income[2];
		response[j++] = income[3];
		response[j++] = income[4];
		response[j++] = income[5];
		uint16_t crc16 = ModbusCrc16(response, j);
		response[j++] = crc16;
		response[j++] = crc16 >> 8;
	}

    return j;
}

/*------------------------------------------------------------------------
*@Function :Modbus_Slave 
作为modbus从机的报文解析

上级BLE/wifi等到下级uart/can的转发原则：
uart 只写转发，
can:写和部分读历史记录转发



*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

*cmd_num、**position_list用 NULL来区分切换，二者一个NULL，另一个非NULL(需要输出的对象)


*@return         
0- fail
no 0: tx len
*/
int Modbus_Slave(const uint8_t *income, uint16_t inlen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl, reg_position_list_t **position_list) 
{

    if (xSemaphore == NULL) { // MODBUS互斥锁,防止多任务同时访问
        xSemaphore = xSemaphoreCreateMutex();
        if (xSemaphore == NULL) {
            ESP_LOGE(TAG, "Modbus slave Semaphore Create failed");
            return 0;
        }
    }

    /* 获取信号量 */
    if (xSemaphoreTake( xSemaphore, pdMS_TO_TICKS(300) ) != pdPASS) 
	{
        return 0;
    }

    int len = 0;
    switch (income[1])
    {
        case 0x03: len = Modbus_ReadRegs(income, inlen, response, cmd_label, cmd_num,chl);  *cmd_num = 0;     break;
        case 0x06: len = Modbus_WriteSingleReg(income, inlen, response, cmd_label, cmd_num,chl, position_list); break;
        case 0x10: len = Modbus_WriteMultiRegs(income, inlen, response, cmd_label, cmd_num,chl, position_list); break;

        default:
	    if(NULL != response)
	    {
			response[0] = income[0];
			response[1] = income[1];
			len = Modbus_Error(response, FCN_NOT_SUPPORTED);
		}  
		else
		{
			len = 0;
		}
            break;
    }

    xSemaphoreGive(xSemaphore);  /* 释放信号量 */
    return len; // modbus response data length
}

/*------------------------------------------------------------------------
*@Function :Modbus_WriteMultiRegs_report 

主动上报绑定帧
借用modbus master send 0x10 报文格式
framecnt:设备数量偏移;
第一帧为0，第二帧为前面所有帧已传递节点数量的和，最大节点数量仅支持255个
writeRegsCnt:带查设备节点数量的寄存器数量

*@return		  
0- fail
no 0: tx len
*/
uint16_t Modbus_WriteMultiRegs_Report_Frame(uint8_t *response , uint16_t writeRegsCnt, uint16_t ver) 
{
    uint16_t i = 0;
    uint16_t j = 0;
    uint16_t *p_tab2 = NULL;
    uint16_t crc16_temp =0;

    uint8_t SlaveAddress = 0;
    uint16_t startAddress = MOD_REG_START_ADDR_21000; // 写入寄存器地址
    uint16_t writeBytesCnt = (writeRegsCnt + 2) * 2; // 写入字节数量

    // 越界时字节数无效，约定上位机不解析
    if(writeBytesCnt > 0xFF) writeBytesCnt = 0;

    response[j++] = SlaveAddress;
    response[j++] = 0x10;
    response[j++] = startAddress >> 8;
    response[j++] = startAddress & 0xFF;
    response[j++] = (writeRegsCnt + 2) >> 8;
    response[j++] = (writeRegsCnt + 2) & 0xFF;
    response[j++] = writeBytesCnt & 0xFF;

    response[j++] = (LSB2MSB(ver)) & 0xFF;
    response[j++] = (LSB2MSB(ver) >> 8) & 0xFF;

    response[j++] = (LSB2MSB(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.bias)) & 0xFF;
    response[j++] = (LSB2MSB(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.bias) >> 8) & 0xFF;

    if ( ver == 1 || ver == 5) {
        if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.bias == 0 ) {
            p_tab2 =(uint16_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.self_bind_info;
        } else {
            p_tab2 =(uint16_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.point_bind_info[top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.bias - 1];
        }
    } else {
        p_tab2 =(uint16_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.point_bind_info[top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.bias];
    }
    
    if(p_tab2) {
        for ( i = 0; i < writeRegsCnt; i++) {
            response[j++] = (LSB2MSB(p_tab2[i]))&0xFF;
            response[j++] = (LSB2MSB(p_tab2[i])>>8)&0xFF; 
        }	 
    }	 
    
    crc16_temp = ModbusCrc16(response, j);
    response[j++] = crc16_temp;
    response[j++] = crc16_temp >> 8;

    return j;
}

void Single_Reg_Set_to_IoT(uint8_t SlaveAddress, uint16_t RegAddress, uint16_t value){
	reg_position_t reg_position;
	uint16_t *p_tab = NULL;
    uint16_t startAddress = RegAddress; // 开始地址
    uint16_t writeRegData = value;
	uint8_t writeRegsCnt=1;

	p_tab = vLookupDataTab(SlaveAddress, startAddress, writeRegsCnt, true, &reg_position, MD_CHL_SELF);					// 查询table2中的数据
		
	if(p_tab) {
		ESP_LOGI(TAG, "Single_Reg_Set_to_IoT: RegAddress(%d), writeRegData(%d)", startAddress, writeRegData);
		*p_tab = writeRegData;
	} else {
#ifdef CONFIG_UART_PORT_ENABLE
        return Wifi_ble_to_arm_uart_transmit(income, inLen, response, chl);
#else
        return;
#endif
	}
    
#ifdef CONFIG_CAN_PORT_ENABLE
    can_cmd_queue_struct can_cmd = {NULL, 0, 0};
	can_cmd.num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
    can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
    if (!can_cmd.cmd) {
        ESP_LOGE (TAG, "Single_Reg_Set_to_IoT malloc failed");
        return ;
    }
	Modbus_To_CAN_Write_Info_After_Process(startAddress, writeRegsCnt, can_cmd.cmd, &can_cmd.num);
    sys_new_can_data_resend(&can_cmd, SlaveAddress);     
#endif

    /*无线透传设置*/
    Modbus_To_Wireless_Write_Info_After_Process(startAddress, writeRegsCnt, SlaveAddress, MD_CHL_SELF);

	/* 该数据由ota数据表处理 */
	md_priv_data_t priv_data = {0};
	md_data_t *p_data = md_tbl_find(startAddress);
	if (p_data == NULL) {
		// ESP_LOGE(TAG, "find register table failure, line: %d", __LINE__);
		return;
	}

	/* 表回调函数 */
	if (p_data->tbl.tbl_cb)//检查
	{
		tbl_cb_data_t cb_data = {
			.SlaveAddress = SlaveAddress,
			.reg_addr_offset =reg_position.offset,
			.reg_addr = reg_position.reg_addr,
			.reg_nums = writeRegsCnt,
			.is_write = true,
			.cb_chl = MD_CHL_SELF,
		};

        if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
        {
            return;
        }
	}		

    return ;
}

/**
 * 修正并健壮化单寄存器读取（从 IoT 表）
 */
bool Single_Reg_Get_From_IoT(uint8_t SlaveAddress, uint16_t RegAddress, uint16_t *value)
{
    if (value == NULL) return false;

    reg_position_t reg_position;
    uint16_t startAddress = RegAddress;
    uint16_t *p_tab = vLookupDataTab(SlaveAddress, startAddress, 1, false, &reg_position, MD_CHL_SELF); /* 查询 table */

    if (p_tab == NULL) {
        ESP_LOGW(TAG, "Single_Reg_Get_From_IoT: vLookupDataTab fail addr=%u", (unsigned)startAddress);
        return false;
    }

    /* 该数据由表回调处理时，先查找表项并调用回调以保证状态同步 */
    md_priv_data_t priv_data = {0};
    md_data_t *p_data = md_tbl_find(startAddress);
    if (p_data == NULL) {
        ESP_LOGE(TAG, "Single_Reg_Get_From_IoT: md_tbl_find fail addr=%u", (unsigned)startAddress);
        return false;
    }

    if (p_data->tbl.tbl_cb) {
        tbl_cb_data_t cb_data = {0};
        cb_data.SlaveAddress     = SlaveAddress;
        cb_data.reg_addr         = startAddress;
        cb_data.reg_addr_offset  = 0;
        cb_data.reg_nums         = 1;
        cb_data.is_write         = false;
        cb_data.cb_chl           = MD_CHL_SELF;

        if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0) {
            ESP_LOGE(TAG, "Single_Reg_Get_From_IoT: tbl_cb failed addr=%u", (unsigned)startAddress);
            return false;
        }
    }

    /* 读取并返回值 */
    *value = *p_tab;
    return true;
}

