#ifndef __LL_PARAM_DEF_H__
#define __LL_PARAM_DEF_H__
#include <stdint.h>
#include "inv_type.h"
#include "pack_type.h"
#include "DisasterWarn.h"

/**
 * @brief 逆变基本参数设置
 */
#pragma pack(push, 1)
typedef struct {
//    uint16_t time1;    							// 高8bit-年；低8bit-月；
//    uint16_t time2;    							// 高8bit-日；低8bit-时；
//    uint16_t time3;    							// 高8bit-分；低8bit-秒；
//    uint16_t res;
//    uint8_t work_mode;     						// 00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网
//    ctrl_event ctrl;       						// 一次性控制事件,
//    uint8_t ctrl_led;       					// 控制LED
//    uint8_t ctrl_meter;     					// 控制电表
//    uint8_t ctrl_pv;        					// 控制PV
//    uint8_t ctrl_inv;    						// 控制逆变
//    uint8_t ctrl_ac;  							// 控制ac
//    uint8_t ctrl_dc;  							// 控制dc
//    uint8_t ctrl_poweron;  						// 控制电源
//    uint8_t ctrl_dc_eco;       					// 控制dc eco模式0：关闭，1：开启
//    uint8_t ctrl_dc_eco_time;  					// 控制dc eco模式空闲时间
//    uint16_t eco_dc_power_value;    			// dc eco模式触发运行功率
//    uint8_t ctrl_ac_eco;       					// 控制ac eco模式 0：关闭，1：开启
//    uint8_t ctrl_ac_eco_time; 					// 控制ac eco模式空闲时间
//    uint16_t eco_ac_power_value; 				// ac eco模式触发运行功率
//    uint8_t ctrl_chg_mode; 						// 控制充电模式
//    uint8_t ctrl_super_power; 					// 控制超级输出模式
//    uint8_t ctrl_low_cap_pct;  					// 系统低电量百分比
//    uint8_t ctrl_high_cap_pct; 					// 系统满电量百分比
//    uint8_t ctrl_inv_mode; 						// 单火线，双火线，并机，并机双火线
//    uint8_t ctrl_dev_id; 						// 给逆变设置编号
//    uint8_t ctrl_all_energy_type; 				// 历史能量
//    uint8_t ctrl_now_energy_type; 				// 当前能量
//    uint8_t ctrl_log_page;   					// 控制历史记录页
//    uint8_t ctrl_time_area; 					// 控制时间分段管理
//    time_control ctrl_time[10];
//    uint8_t ctrl_PvType[6];
//    uint8_t ctrl_alarm_voice;
//    uint8_t nc[20];
	INV_SET00_STRUCT_COMMON_0x1A	
} inv_base_param_t;
#pragma pack(pop)

/**
 * @brief 逆变高级参数设置
 */
#pragma pack(push, 1)
typedef struct {
//    uint8_t password[8]; 						// 设置区密码
//    uint8_t ctrl_reset_factory; 				// 恢复出厂
//    uint8_t ctrl_grid;      					// 控制电网
//    uint8_t ctrl_feedback;  					// 控制馈电
//    uint16_t ctrl_output_inv_volt;  			// 输出电压控制
//    uint16_t ctrl_output_inv_freq;   			// 输出电压频率
//    uint16_t ctrl_chg_max_volt; 				// 最大充电电压
//    uint16_t ctrl_pv_max_curr; 					// 最大PV充电电流
//    uint16_t ctrl_grid_max_power;   			// 电网最大功率
//    uint16_t ctrl_grid_max_curr; 				// 电网最大电流
//    uint16_t ctrl_feedback_max_power;   		// 电网馈电最大功率
//    uint16_t ctrl_feedback_max_curr; 			// 电网馈电最大电流
//    uint16_t res0;   							// 保留
//    uint8_t  ctrl_user_area; 					// 用户地区设置
//    uint8_t  ctrl_pv_paralle[6]; 				// 设置PV并联开启和关闭
//    uint8_t  ctrl_grid_plus;    				// 电网增强模式
//    uint8_t  ctrl_save_power_state; 			// 保存AC,DC关机前状态
//    uint8_t  ctrl_meter_enable; 				// 电表开启
//    uint8_t  ctrl_meter_select; 				// 电表类型选择
//    uint8_t  ctrl_Inv_Multi_enable; 			// 逆变器并机模式
//    uint8_t  ctrl_Inv_addr_Set; 				// 设置逆变器地址
//	ct_test_t ct_test;
//	ctrl_mix_t ctrl_mix;
//	ctrl_mix_t ctrl_mix2;
//    uint8_t  res1[16];
	INV_SET01_STRUCT_COMMON_0x1B	
} inv_advance_param_t;
#pragma pack(pop)

/**
 * @brief 逆变认证参数设置
 */
#pragma pack(push, 1)
typedef struct {
//    uint16_t SetGridEnable;          			// 高级功能使能
//    uint16_t SetGridCountry;         			// 电网认证使用地区
//    uint16_t SetGridUV1Value;        			// 电网欠压LEVEL1保护值
//    uint16_t SetGridUV1Time;         			// 电网欠压LEVEL1触发时间
//    uint16_t SetGridUV2Value;        			// 电网欠压LEVEL2保护值
//    uint16_t SetGridUV2Time;         			// 电网欠压LEVEL2触发时间
//    uint16_t nc1[4];
//    uint16_t SetGridAVGOVValue;      			// 电网均值过压
//    uint16_t SetGridOV1Value;        			// 电网高压LEVEL1保护值
//    uint16_t SetGridOV1Time;         			// 电网高压LEVEL1保护时间
//    uint16_t SetGridOV2Value;        			// 电网高压LEVEL2保护值
//    uint16_t SetGridOV2Time;         			// 电网高压LEVEL2保护时间
//    uint16_t nc2[4];
//    uint16_t SetGridUF1Value;        			// 电网欠频LEVEL1保护值
//    uint16_t SetGridUF1Time;         			// 电网欠频LEVEL1保护时间
//    uint16_t SetGridUF2Value;        			// 电网欠频LEVEL2保护值
//    uint16_t SetGridUF2Time;         			// 电网欠频LEVEL2保护时间
//    uint16_t nc3[4];
//    uint16_t SetGridOF1Value;        			// 电网过频LEVEL1保护值
//    uint16_t SetGridOF1Time;         			// 电网过频LEVEL1保护时间
//    uint16_t SetGridOF2Value;        			// 电网过频LEVEL2保护值
//    uint16_t SetGridOF2Time;         			// 电网过频LEVEL2保护时间
//    uint16_t nc4[4];
//    uint16_t SetGridVoltMinValue;    			// 电网重连电压下限值
//    uint16_t SetGridVoltMaxValue;    			// 电网重连电压上限值
//    uint16_t SetGridFreqMinValue;    			// 电网重连频率下限值
//    uint16_t SetGridFreqMaxValue;    			// 电网重连频率上限值
//    uint16_t SetGridRetryTime;       			// 电网异常重连时间
//	uint16_t SetDefaultVlue; 					// 设置默认值
//    uint16_t  nc5[9];                			// 保留
	INV_SET03_STRUCT_COMMON_0x1D    
} inv_auth_param_t;
#pragma pack(pop)

/**
 * @brief PACK参数设置
 */
#pragma pack(push, 1)
typedef struct {
//    uint8_t pack_heat_enable;					// pv接入时是否开启加热功能 0：关闭；1：开启
//    uint8_t ctr_heat_enable;					// 是否开启加热电池最佳温度功能0：关闭；1：开启 （开启此功能App需要提示可能会消耗电池电）
//    uint8_t unlock_failed_flags;				// 电池包因故障锁，重启不恢复，下发命令解锁电池包。普通用户应无此权限
	PACK_CONFIG_STRUCT_COMMON_0x55   
} pack_config_param_t;
#pragma pack(pop)

/**
 * @brief IOT参数设置
 */
#pragma pack(push, 1)
typedef struct {
    _SaveWebStormWatchApiData SaveWebStormWatchApiData;					
    uint8_t LastWorkMode;				
    _AppCancelStormEvent AppCancelStormEvent;
    disaster_send_param_t disaster_send_param;
    uint8_t nc[4];
} disaster_param_t;
#pragma pack(pop)

#endif

