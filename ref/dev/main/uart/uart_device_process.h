#ifndef __UART_DEVICE_PROCESS_H__
#define __UART_DEVICE_PROCESS_H__



#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

//#include "iot_period_task.h"
#include "comm_define.h"
#include "dev_data_record.h"

#include "modbus_data.h"
//#include "can_data.h"
#include "app_uart.h"
#include "iot_type.h"
#include "inv_type.h"
#include "parts_type.h"
#include "xmodem_client.h"
#include "comm_can_uart.h"
#include "comm_define.h"
#include "smart_tou_ctrl_v1.h"
#include "tou_relay_ctrl.h"

#define FAST_UART_TX_GAP		5//连续两次突发写的间隔 阈值，at 100ms
#define MAX_BMS_NUM		5//
#define MAX_CELL_NUM		16//

#define TIME_CTRL_NUM   10//6




//modbus寄存器地址map
//////Sub 1GHz;ATS+DTU
#define REG_ADDR_1GHz_READ1		(uint16_t)5//
#define REG_ADDR_1GHz_READ2		(uint16_t)5//
#define REG_ADDR_1GHz_READ3		(uint16_t)5//

#define REG_ADDR_1GHz_WR		(uint16_t)5//
#define REG_ADDR_1GHz_		(uint16_t)5//

#define CNT_REG_ADDR_1GHz_READ1		(uint16_t)5//
#define CNT_REG_ADDR_1GHz_READ2		(uint16_t)5//
#define CNT_REG_ADDR_1GHz_READ3		(uint16_t)5//


#define CNT_REG_ADDR_1GHz_WR		(uint16_t)5//
#define CNT_REG_ADDR_1GHz_		(uint16_t)5//

//////ATS-AT403
#define REG_ADDR_AT403_READ1		(uint16_t)5//
#define REG_ADDR_AT403_READ2		(uint16_t)5//
#define REG_ADDR_AT403_READ3		(uint16_t)5//
#define REG_ADDR_AT403_WR		(uint16_t)5//
#define REG_ADDR_AT403_		(uint16_t)5//

#define CNT_REG_ADDR_AT403_READ1		(uint16_t)5//
#define CNT_REG_ADDR_AT403_READ2		(uint16_t)5//
#define CNT_REG_ADDR_AT403_READ3		(uint16_t)5//


#define CNT_REG_ADDR_AT403_WR		(uint16_t)5//
#define CNT_REG_ADDR_AT403_		(uint16_t)5//

//////DTU-RS485
#define REG_ADDR_RS485_READ1		(uint16_t)5//
#define REG_ADDR_RS485_READ2		(uint16_t)5//
#define REG_ADDR_RS485_READ3		(uint16_t)5//
#define REG_ADDR_RS485_WR		(uint16_t)5//
#define REG_ADDR_RS485_		(uint16_t)5//

#define CNT_REG_ADDR_RS485_READ1		(uint16_t)5//
#define CNT_REG_ADDR_RS485_READ2		(uint16_t)5//
#define CNT_REG_ADDR_RS485_READ3		(uint16_t)5//

#define CNT_REG_ADDR_RS485_WR		(uint16_t)5//
#define CNT_REG_ADDR_RS485_		(uint16_t)5//


// //无线设备间协议版本类型
// #define	MESH_VERSION_TYPE_WIFI	    1//1-WIFI
// #define	MESH_VERSION_TYPE_SUB1GHZ	2//2-SUB1GHZ；


// //无线设备间协议报文类型
// #define	MESH_FRAME_TYPE_READ	1//1-读取；
// #define	MESH_FRAME_TYPE_WRITE	2//2-写入；
// #define	MESH_FRAME_TYPE_READ_RTN	3//3-读取-RTN；
// #define	MESH_FRAME_TYPE_WRITE_RTN	4//4-写入-RTN；
// #define	MESH_FRAME_TYPE_PERIOD	5//5-周期上报（无RTN）
// #define	MESH_FRAME_TYPE_HEART	6//6-周期上报（心跳）
// #define	MESH_FRAME_TYPE_XMODEM	7//7-上级发xmodem升级相关
// #define	MESH_FRAME_TYPE_XMODEM_RTN	8//8-下级响应xmodem升级相关




// //Sub 1GHz:无线设备间协议报文寄存器地址序号
// #define	MESH_FRAME_ADDR_HEAD		0//
// #define	MESH_FRAME_ADDR_VER			1//
// #define	MESH_FRAME_ADDR_MAC_SOURCE	2//MAC_源设备
// #define	MESH_FRAME_ADDR_MAC_TARGET	10//MAC_目标设备
// #define	MESH_FRAME_ADDR_TYPE		18//
// #define	MESH_FRAME_ADDR_MODBUS_HEAD	19//


// //WIFI UDP: 无线设备间协议报文寄存器地址序号
// #define	WIFI_UDP_FRAME_ADDR_HEAD		0//
// #define	WIFI_UDP_FRAME_ADDR_VER			1//
// #define	WIFI_UDP_FRAME_ADDR_SN_SOURCE	2//MAC_源设备
// #define	WIFI_UDP_FRAME_ADDR_TYPE_SOURCE	10//

// #define	WIFI_UDP_FRAME_ADDR_SN_TARGET	12//MAC_目标设备
// #define	WIFI_UDP_FRAME_ADDR_TYPE_TARGET	20//

// #define	WIFI_UDP_FRAME_ADDR_TYPE		22//
// #define	WIFI_UDP_FRAME_ADDR_MODBUS_LEN	23//
// #define	WIFI_UDP_FRAME_ADDR_MODBUS_HEAD	25//
#define	WIFI_UDP_FRAME_ADDR_MAC			25//
#define	WIFI_UDP_FRAME_ADDR_PRIORITY	31//源设备优先级
#define	WIFI_UDP_FRAME_ADDR_SEQ			33//并机序号



#define  NET_POINT_ONLINE	2//设备可靠在线,<30s
#define  NET_POINT_OFFLINE_HALF	1//设备短期通讯超时,>30s
#define  NET_POINT_OFFLINE	0//设备长期掉线,>5minute

/*
电表：
ADL400
AGF-AE-D/200:29.0101.0150-00  Acrel 美规单相双向电能表
ADL200N-CT
ADL400N-CT

*/

enum Meter_type
{
    invalid = 0,
	ADL400,
	AGF_AE_D_200,
	ADL200N,        //默认
	ADL400N_CT,
};

enum Meter_status
{
	enable = 1,
	disable,
};

#define  SinglePhaseMeterAddr          0x2100  //电表数据段
#define  SinglePhaseMeterRegCount      26      //数据段寄存器个数
#define  SinglePhaseMeterFreqAddr      0x2134  //电表频率寄存器
#define  SinglePhaseMeterFreqRegCount  2       

#define  ctrl_meter_default     (uint16_t)(ADL200N<<8)|enable

#define  GridMeterAddr               40000
#define  GridMeterRegCount           23



#define SET_REG_START_ADDRESS       40000
#define ADL200_FRONT_ADDRESS        0x2100
#define ADL200_BACK_ADDRESS        0x2134

#define GRID_ADDRESS     1  //固定电表从机地址

/////////////////////////////////////uart透传，windy add below
#define PV_MAX_NUM  4
#define MAX_YEAR_NUM    15




#pragma pack(1)

/*
上级(WIFI/BLE)和下级(UART)modbus透传转发结构体
*/

typedef union {
    uint8_t all;
    struct {
        uint8_t chg_over_t   : 1;  // bit0 充电过温保护
        uint8_t dchg_over_t  : 1;  // bit1 放电过温保护
        uint8_t chg_under_t  : 1;  // bit2 充电低温保护
        uint8_t dchg_under_t : 1;  // bit3 放电低温保护
        uint8_t tot_v_over   : 1;  // bit4 总压过压
        uint8_t tot_v_under  : 1;  // bit5 总电压欠压
        uint8_t cell_v_over  : 1;  // bit6 单节过压
        uint8_t cell_v_under : 1;  // bit7 单节欠压
    }bit;
}bms_protect1_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t ocp1       : 1;  // 过流保护1
        uint8_t ocp2       : 1;  // 过流保护2
        uint8_t ocp3       : 1;  // 过流保护3
        uint8_t ocp4       : 1;  // 过流保护4
        uint8_t prechg_fault : 1;  // 预充故障
        uint8_t scp        : 1;  // 短路保护
        uint8_t chg_mos    : 1;  // 充电MOS状态 开启：1 关闭：0
        uint8_t dchg_mos   : 1;  // 放电MOS状态 开启：1 关闭：0
    }bit;
}bms_protect2_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t cell_diff_volt : 1; //bit0 单体压差大
        uint8_t cell_diff_temp : 1; //bit1 单体温差大
        uint8_t cell_fault : 1; //bit2 单体故障
        uint8_t temp_fault : 1; //bit3 温度故障
        uint8_t shout_cut : 1; //bit4 系统短路
        uint8_t insulat_fault : 1;// bit5 结缘故障
        uint8_t relay_off_fault : 1;// bit6 继电器断开失败
        uint8_t relay_on_fault : 1;// bit7 继电器闭合失败
        uint8_t pre_dsg_fault : 1; //bit8 预充故障
        uint8_t power_off_fault : 1;//bit9 关机失败
        uint8_t fuse_fault : 1; //bit10 保险师熔断
        uint8_t relay_shout : 1; //bit11 继电器短路
        uint8_t hvil_fault : 1; //bit12 高压互锁异常
        uint8_t bmu_offline : 1; //bit13 bmu通讯错误
        uint8_t rsv : 1;//bit14 
        uint8_t balance_mos_fault : 1;// 均衡mos失效
    }bit;
}bms_error1_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t balance_fault : 1; //bit0 均衡故障
        uint8_t bmu_power : 1; //bit1 bmu供电异常
        uint8_t devId_err : 1; //bit2 设备id错误
        uint8_t arg_init_err : 1; //bit3 参数初始化错误
        uint8_t rsv1 : 1; //bit4  
        uint8_t rsv2 : 1; //bit5  
        uint8_t rsv3 : 1; //bit6  
        uint8_t rsv4 : 1; //bit7  
        uint8_t rsv5 : 1; //bit8 
        uint8_t rsv6 : 1; //bit9  
        uint8_t rsv7 : 1; //bit10 
        uint8_t rsv8 : 1; //bit11 
        uint8_t rsv9 : 1; //bit12 
        uint8_t rsv10 : 1; //bit13 
        uint8_t rsv11 : 1;//bit14 
        uint8_t rsv12 : 1;//  
    }bit;
}bms_error2_t;

typedef union {
    uint16_t all;
    // struct {
    //     uint8_t balance_fault : 1; //bit0 均衡故障
    //     uint8_t bmu_power : 1; //bit1 bmu供电异常
    //     uint8_t devId_err : 1; //bit2 设备id错误
    //     uint8_t arg_init_err : 1; //bit3 参数初始化错误
    //     uint8_t rsv1 : 1; //bit4  
    //     uint8_t rsv2 : 1; //bit5  
    //     uint8_t rsv3 : 1; //bit6  
    //     uint8_t rsv4 : 1; //bit7  
    //     uint8_t rsv5 : 1; //bit8 
    //     uint8_t rsv6 : 1; //bit9  
    //     uint8_t rsv7 : 1; //bit10 
    //     uint8_t rsv7 : 1; //bit11 
    //     uint8_t rsv8 : 1; //bit12 
    //     uint8_t rsv9 : 1; //bit13 
    //     uint8_t rsv10 : 1;//bit14 
    //     uint8_t rsv11 : 1;//  
    // }bit;
}bms_error3_t;
typedef union {
    uint16_t all;
    struct {
        uint8_t rsv1 : 4;
        uint8_t user_reset : 2;
        uint8_t rsv2 : 2;
        uint8_t rsv3 : 8;
    }bit;
}misc_enable_t;	
typedef struct {
	uint16_t default_reset; // 系统参数恢复出厂设置
	uint16_t auth_reset;	// 认证数据恢复到默认值
	uint16_t data_clear;	// 按位表示 bit0:all能量清除；bit1:DC负载能量清除；bit2:AC负载能量清除；bit3:PV能量清除；
							// bit4:GridChg能量清除；bit5:GridDsg能量清除；bit6:log历史清除；bit7:故障清除；bit8:重置充放时间段
	uint16_t history_energy_type;	// 历史能量选择
	uint16_t now_energy_type;		// 当前能量选择
	uint16_t log_page;				// 事件历史翻页
	misc_enable_t misc_enable; // 杂项使能
} once_set_t; 


typedef union {
    uint16_t all;
    struct {
        uint8_t is_host : 1; // bit0: 逆变主机
        uint8_t is_init : 1; // bit1: DSP参数需要初始化
        uint8_t read_fault: 1; // bit2
        uint8_t record_energy : 1; //bit3 记录发电量
        uint8_t dcdc_chg_normal : 1; //bit4 满足充电
        uint8_t dcdc_dsg_normal : 1; //bit5 满足放电
        uint8_t dcdc_fault_stop : 1; // bit6 故障停机
        uint8_t pv_online : 1; // bit7 当前PV在线
        uint8_t pack_swicth : 1; // bit8 pack开关状态
        uint8_t load_switch : 1; // bit9 负载开关状态
        uint8_t grid_switch : 1; // bit10 电网开光状态
        uint8_t dcdc1_inv_switch : 1; // bit11 dcdc1输出到微逆
        uint8_t dcdc2_inv_switch : 1; // bit12 dcdc2输出到微逆
        uint8_t dcdc_bat_dsg : 1; // bit13 dcdc2输出到微逆
        uint8_t dcdc_bat_chg : 1; // bit14 dcdc2输出到微逆
        uint8_t relay_change : 1; // bit15 0: 保持，1：改变
    }bit;
}state_a_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t stop : 1; // bit0: 停机
        uint8_t standby : 1; // bit1: 待机
        uint8_t alarm : 1; // bit2: 告警
        uint8_t fault : 1; // bit3: 故障
        uint8_t lower_freq : 1; // bit4: 降频
        uint8_t dcdc_chg_en : 1; // bit5
        uint8_t dcdc_dsg_en : 1; // bit6
        uint8_t rsv1 : 1; // bit7
        uint8_t off_grid : 1; // bit8 离网状态
        uint8_t chg_dsg_state : 1; // bit9 充放电状态
        uint8_t pre_chg_state : 1; // bit10 预充状态
        uint8_t const_chg_state : 1; // bit11 均状态 恒流充电
        uint8_t trickle_chg_state : 1; // bit12 涓流升级
        uint8_t full_state : 1; // bit13 充满
        uint8_t empty_state : 1; // bit14 放空
        uint8_t rsv2 : 1; // bit15 
    }bit;
}state_b_t;

typedef union {
    uint16_t all;
}state_c_t;

typedef union {
    uint16_t all;
}state_d_t;

typedef union {
    uint16_t all;
}state_e_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t bat_over_volt : 1; // bit0: 电池过压
        uint8_t bat_under_volt : 1; // bit1: 电池低压
        uint8_t bat_over_curr: 1; // bit2 电池过流
        uint8_t bat_over_curr_timeout : 1; //bit3 电池过流超时
        uint8_t bat_soft_boot_failed : 1; //bit4 电池软启动失败
        uint8_t pv1_over_volt : 1; // bit5 pv1过压
        uint8_t pv2_over_volt : 1; // bit6 pv2过压
        uint8_t rsv1 : 1; // bit7
        uint8_t bus_over_volt : 1; // bit8 总线过压
        uint8_t bus_under_volt : 1; // bit9 总线欠压
        uint8_t bus_over_curr : 1; // bit10 总线过流
        uint8_t bus_over_curr_timeout : 1; // bit11 总线过流超时
        uint8_t bus_soft_boot_failed : 1; // bit12 总线软启动失败
        uint8_t pv1_over_curr : 1; // bit13 pv1过流
        uint8_t pv2_over_curr : 1; // bit14 pv2过流
        uint8_t rsv3 : 1; // bit15
    }bit;
}alram_a_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t ac_over_volt : 1; // bit0: ac过压
        uint8_t ac_under_volt : 1; // bit1: ac欠压
        uint8_t ac_over_freq: 1; // bit2 ac过频
        uint8_t ac_under_freq : 1; //bit3 ac欠频
        uint8_t ac_inversion : 1; //bit4 ac反相
        uint8_t matrix_iso_error : 1; // bit5 绝缘阻抗ISO异常
        uint8_t rsv2 : 1; // bit6  
        uint8_t rsv3 : 1; // bit7
        uint8_t off_grid_volt : 1; // bit8 离网电压异常
        uint8_t off_grid_freq : 1; // bit9 离网频率异常
        uint8_t ac_over_load : 1; // bit10 交流过载
        uint8_t ac_over_load_timeout  : 1; // bit11 交流过载超时
        uint8_t ac_lock_phase_fault : 1; // bit12 交流锁相失败
        uint8_t ac_soft_boot_failt : 1; // bit13 软启动失败
        uint8_t rsv4 : 1; // bit14  
        uint8_t rsv5 : 1; // bit15
    }bit;
}alram_b_t;

typedef union {
    uint16_t all;
}alram_c_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t adc_zero_fault : 1; // bit0: 采样0点读取异常
        uint8_t abj_value_fault : 1; // bit1: 校准参数读取异常
        uint8_t version_fault : 1; // bit2 dsp软件版本读取异常
        uint8_t init_arg_fault : 1; //bit3 dsp初始化参数失败
        uint8_t arg_set_fault : 1; //bit4 参数设置错误
        uint8_t dev_id_out_scpoe : 1; // bit5 设备编号超范围
        uint8_t dev_id_multi : 1; // bit6 设备编号重复
        uint8_t rsv1 : 1; // bit7
        uint8_t uart_485_fault : 1; // bit8 485总线错误
        uint8_t can_bus_fault : 1; // bit9 can总线错误
        uint8_t rsv2 : 1; // bit10 总线过流
        uint8_t rsv3 : 1; // bit11 总线过流超时
        uint8_t rsv4 : 1; // bit12 总线软启动失败
        uint8_t rsv5 : 1; // bit13 pv1过流
        uint8_t rsv6 : 1; // bit14 pv2过流
        uint8_t rsv7 : 1; // bit15
    }bit;
}alram_d_t;

typedef union {
    uint16_t all;
}alram_e_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t stop_chg : 1; //bit0 0: 允许充电, 1: 禁止充电
        uint8_t stop_dsg : 1; //bit1 0: 允许放电, 1: 禁止放电
        uint8_t emergency_chg : 1; //bit2 0: 无紧急充电, 1: 紧急充电
        uint8_t chg_suspend: 1; //bit3 0: 无告警保护 1: 充电告警保护, 
        uint8_t dsg_suspend : 1; //bit4 0: 无告警保护 1: 放电告警保护, 
        uint8_t fault : 1; // bit5 0: 无故障保护 1: 故障保护, 
        uint8_t rsv : 2;// bit6 - bit7
        uint8_t chg_alarm : 1; //bit8 0: 无告警保护 1: 充电告警保护, 
        uint8_t dsg_alarm : 1; //bit9 0: 无告警保护 1: 放电告警保护, 
    }bit;
}bat_state_t;

typedef struct { // 功能码  0x03
    state_a_t state_a; // 0~4 状态量A-E，用于显示
    state_b_t state_b;
    state_c_t state_c;
    state_d_t state_d;
    state_e_t state_e;
    
    alram_a_t alarm_a; // 5~9 告警量A-E 用于系统故障
    alram_b_t alarm_b;
    alram_c_t alarm_c;
    alram_d_t alarm_d;
    alram_e_t alarm_e;
}dsp_reg0000_t;

typedef struct { // 功能码  0x03
    uint16_t pv_volt[PV_MAX_NUM]; //100-103 PV电压，0 - 5000 0.1V
    uint16_t pv_curr[PV_MAX_NUM]; //104-107 PV电压，0 - 500  0.AV

    uint16_t pv_total_power; //108 PV总功率 0 - 5000 1W
    uint16_t pv_power[PV_MAX_NUM]; //109-112 PV功率 0 - 5000 1W
    
    uint16_t grid_volt; //113 0-5000  0.1V
    uint16_t grid_curr; //114 0-500 0.1A
    uint16_t grid_freq; //1115 0-9000 0.01Hz
    uint16_t grid_p_power; //116 有功功率 1W
    uint16_t grid_q_power; //117 无功功率 1Var
    uint16_t grid_s_power; //118 视在功率 1VA
    uint16_t grid_pf_value; //119 电网功率因数 0.001
    uint16_t iInvVoltReserved; //data 120: 逆变电压预留：0.0~500.0V，*10
    uint16_t iInvCurrReserved; //data 121: 逆变电流预留：0.0~50.0A，*10
    uint16_t iBusVolt1;        //data 122: 母线电压1：0.0~500.0V，*10
    uint16_t iBusVolt2;        //data 123: 母线电压2：0.0~500.0V，*10
    uint16_t iVpkVolt1;        //data 124: Vpk电压1：0.0~500.0V，*10
    uint16_t iVpkVolt2;        //data 125: Vpk电压2：0.0~500.0V，*10
    uint16_t iVpkVolt3;        //data 126: Vpk电压3：0.0~500.0V，*10
    uint16_t iVpkVolt4;        //data 127: Vpk电压4：0.0~500.0V，*10
    uint16_t iITRCurrent1;     //data 128: ITR电流1：0.0~50.0A，*10
    uint16_t iITRCurrent2;     //data 129: ITR电流2：0.0~50.0A，*10
    uint16_t iITRCurrent3;     //data 130: ITR电流3：0.0~50.0A，*10
    uint16_t iITRCurrent4;     //data 131: ITR电流4：0.0~50.0A，*10
    uint16_t pv_ntc[PV_MAX_NUM]; //132-135 pv温度 1°
    uint16_t inv_ntc[2]; // 136-137 1°
    uint16_t mcu_version_h; // 138 33
    uint16_t mcu_version_l; // 139 13793
    uint16_t pv_max_num; // 140 
    uint16_t inv_work_state; // 141 0：停机；1：离网运行；2：电网带载；3：并网运行；4：并网充电；5：并网放电；
    uint16_t machine_type; //142 DSP设备类型,DCDC/微逆
    // uint16_t dcdc_work_state; //143 DCDC工作状态 0：PV-微逆，旁路 1：BAT-微逆，放电 2：PV-BAT，充电 3：STOP停机
}dsp_reg0100_t;

// 基础设置
typedef struct {  
    uint16_t set_inv_id; // 300 0-10
    uint16_t set_inv_mode; // 301  
    uint16_t set_boot_mode; // 302
    uint16_t rsv303;
    uint16_t rsv304;
    uint16_t rsv305;
    uint16_t rsv306;
    uint16_t rsv307;
    uint16_t rsv308;
    uint16_t off_grid_frequency_level; // 309
    uint16_t rsv310;
    uint16_t rsv311;
    uint16_t rsv312;
    uint16_t rsv313;
    uint16_t power_change_mode; // 314
    uint16_t off_grid_operation_mode; //315
    uint16_t off_grid_voltage_level; //316
    uint16_t system_poweroff; // 317 开机指令 逆变开启和关闭
    uint16_t shutdown_command; // 318关机指令
    uint16_t standby_command; // 319 待机指令
    uint16_t reset_command; // 320 复位指令
    uint16_t rsv321; // 321保留
    uint16_t aging_command; // 322 老化指令
    uint16_t bat_discharging; // 323 电池并联
    // uint16_t rsv[10];
    uint16_t save_data;
    uint16_t crc16;
}dsp_reg0300_t; // 基本设置项,不需要密码

// 高级设置
typedef struct {  
    uint16_t set_inv_max_power; // 400 0-5000 1W
    uint16_t set_pv_max_power; // 401 0-5000 1W
    uint16_t set_pv_start_volt; // 402 0-5000 0.1V PV启动电压
    uint16_t set_pv_stop_volt; // 403 0-5000 0.1V PV停止电压
    uint16_t set_pv_max_curr; // 404 0-5000 0.1A PV最大电流
    uint16_t set_ac_max_curr; // 405 0-500 0.1A 最大交流电流
    uint16_t set_grid_max_volt; // 406 0.1V 额定电网电压
    uint16_t set_grid_max_freq; // 407 0.01Hz  额定电网频率
    uint16_t set_rp_power; // 408 
    uint16_t set_rp_factor; // 409
    uint16_t set_feedback_power; // 410 馈电最大功率 1W
    uint16_t set_feedback_curr;  // 411 馈电最大电流 0.1A
    uint16_t set_over_freq_freq1; //412 -500 - 500 0.01Hz
    uint16_t set_over_freq_freq2; //413 -500 - 500 0.01Hz
    uint16_t set_over_freq_power1; //414 -500 - 500 pu
    uint16_t set_over_freq_power2; //415 -500 - 500 pu

    uint16_t set_volt_adj_volt[4]; //416-419 -500 - 500 0.01Hz
    uint16_t set_volt_adj_power[4]; //420-423 -500 - 500 0.01Hz

    uint16_t set_lvrt_rp_max_volt; // 424 最大无功电压 0.1V
    uint16_t set_lvrt_rp_max_power; // 425 最大无功功率
    uint16_t set_lvrt_rp_min_volt; // 426 最小无功电压
    uint16_t set_lvrt_rp_min_power; // 427 最小无功功率

    uint16_t set_islet_enable; // 428 孤岛使能 1：开启，0：禁止
    uint16_t set_lvrt_enable; // 429 lvrt使能  1：开启，0：禁止
    uint16_t set_over_freq_enable; // 430 过频降载使能  1：开启，0：禁止
    uint16_t set_gfci_enable; //431 GFCI使能
    uint16_t set_iso_enable; //432 ISO使能
    uint16_t set_rsv_enable; //433 预留使能
    uint16_t set_auth_ncb; // 434 认证国家机构
    uint16_t set_ap_ctrl; //435 有功控制模式
    uint16_t set_rp_ctrl; //436 无功控制模式
    uint16_t set_ap_value; //437 有功功率设置
    uint16_t set_power_slope; //438 功率变化率
    uint16_t set_grid_max_power; // 439 1w 电网最大功率
    uint16_t set_grid_max_curr; // 440 0.1A 电网最大电流
    uint16_t set_auth_enable; //441 认证开启/关闭，0：关闭，1开启
    // uint16_t rsv[10];
    uint16_t save_data;
    uint16_t crc16;
}dsp_reg0400_t; // 高级设置项需要密码

// 认证数据
typedef struct {  
    uint16_t grid_over_volt3; // 500 0-5000 0.1V  
    uint16_t grid_over_volt3_time; // 501 0-30000 10ms
    uint16_t grid_over_volt2; // 502 0-5000 0.1V  
    uint16_t grid_over_volt2_time; // 503 0-30000 10ms
    uint16_t grid_over_volt1; // 504 0-5000 0.1V
    uint16_t grid_over_volt1_time; // 505 0-30000 10ms
    uint16_t grid_under_volt3; // 506 0-5000 0.1V  
    uint16_t grid_under_volt3_time; // 506 0-30000 10ms
    uint16_t grid_under_volt2; // 508 0-5000 0.1V  
    uint16_t grid_under_volt2_time; // 508 0-30000 10ms
    uint16_t grid_under_volt1; // 510 0-5000 0.1V
    uint16_t grid_under_volt1_time; // 511 0-30000 10ms

    int16_t grid_over_freq3; //512 -550 - 550 0.01Hz
    int16_t grid_over_freq3_time; //513 10ms
    int16_t grid_over_freq2; //514 -550 - 550 0.01Hz
    int16_t grid_over_freq2_time; //515 10ms
    int16_t grid_over_freq1; //516 -550 - 550 0.01Hz
    int16_t grid_over_freq1_time; //517 10ms

    int16_t grid_under_freq3; //518 -550 - 550 0.01Hz
    int16_t grid_under_freq3_time; //519 10ms
    int16_t grid_under_freq2; //520 -550 - 550 0.01Hz
    int16_t grid_under_freq2_time; //521 10ms
    int16_t grid_under_freq1; //522 -550 - 550 0.01Hz
    int16_t grid_under_freq1_time; //523 10ms

    uint16_t ac_over_curr3; //524  0.1A
    uint16_t ac_over_curr3_time; //525 10ms
    uint16_t ac_over_curr2; //526  0.1A
    uint16_t ac_over_curr2_time; //527 10ms
    uint16_t ac_over_curr1; //528  0.1A
    uint16_t ac_over_curr1_time; //529 10ms    

    uint16_t max_ntc[4]; // 530-533

    uint16_t gfci_trigger_time; // 534 10ms
    uint16_t iso_trigger_time; // 535 10ms
    uint16_t pv_over_volt;// 536 0.1V
    uint16_t pv_over_curr; //537 0.1A
    uint16_t pv_over_volt_time; // 538 10ms
    uint16_t pv_over_curr_time; // 539 10ms

    uint16_t grid_retry_max_volt; // 540 0.1v 电网重连电压上限
    uint16_t grid_retry_min_volt; // 541 0.1V 电网重连电压下线
    int16_t grid_retry_max_freq; // 542 0.01Hz 电网重连频率上限
    int16_t grid_retry_min_freq; // 543 0.01Hz 电网重连频率下线

    uint16_t grid_retry_time; //544 1-30000 1s
    uint16_t grid_avg_volt ; // 545 0.1V
    uint16_t save_data; // 从DSP回读数据后被标记，1：表示需要存储，0：不需要存储
    uint16_t crc16;
}dsp_reg0500_t; // 高级设置项需要密码

typedef struct {  
    uint16_t set_pv_adj_volt1_value; // 700 pv电压1校准系数
    uint16_t set_pv_adj_volt2_value; // 701 pv电压2校准系数
    uint16_t set_pv_adj_volt3_value; // 702 pv电压3校准系数
    uint16_t set_pv_adj_volt4_value; // 703 pv电压4校准系数

    uint16_t set_pv_adj_curr1_value; // 704 pv电流1校准系数
    uint16_t set_pv_adj_curr2_value; // 705 pv电流2校准系数
    uint16_t set_pv_adj_curr3_value; // 706 pv电流3校准系数
    uint16_t set_pv_adj_curr4_value; // 707 pv电流4校准系数

    uint16_t set_grid_adj_volt_value; // 708 
    uint16_t set_grid_adj_curr_value; // 709 

    uint16_t set_vpk_adj_volt1_value; // 710 
    uint16_t set_vpk_adj_volt2_value; // 711 
    uint16_t set_vpk_adj_volt3_value; // 712 
    uint16_t set_vpk_adj_volt4_value; // 713 

    uint16_t set_itr_adj_curr1_value; // 714 
    uint16_t set_itr_adj_curr2_value; // 715 
    uint16_t set_itr_adj_curr3_value; // 716 
    uint16_t set_itr_adj_curr4_value; // 717 
    uint16_t save_data;
    uint16_t crc16;
}dsp_reg0700_t; // 高级设置项需要密码

typedef struct { // BMS数据透传给DSP
    uint16_t set_dc_mode; // 800 设置逆变工作模式0：PV-微逆，旁路 1：BAT-微逆，放电 2：PV-BAT，充电 3：STOP停机
    bat_state_t bat_state; // 801 电池状态
    uint16_t bat_soc; // 802 电池soc
    uint16_t bat_soh; // 803 电池soh
    uint16_t bat_max_chg_curr; // 804 电池最大充电电流 0.1A
    uint16_t bat_max_dsg_curr; // 805 电池最大放电电流 0.1A
    uint16_t bat_total_volt; // 806 电池总电压 0.1V
    uint16_t bat_total_curr; // 807 电池总电流 0.1A
    uint16_t dcdc_relay_cmd; // 808 bit0: 0：保持；1：动作
}dsp_reg0800_t;


typedef union {
    uint16_t all;
    struct {
        uint8_t pv_to_bat : 1; // bit0 pv to bat
        uint8_t grid_to_bat : 1; // bit1
        uint8_t bat_to_grid : 1; // bit2
        uint8_t ac_to_load : 1; // bit3
        uint8_t dc_to_load : 1; // bit4
        uint8_t bat_to_inv : 1; // bit5
        uint8_t inv_to_bat : 1; // bit6
        uint8_t grid_to_load : 1; // bit7
        uint8_t pv_icon : 1; // bit8
        uint8_t grid_icon : 1; // bit9
        uint8_t load_icon : 1; // bit10
        uint8_t pv_to_grid : 1; // bit11
        uint8_t pv_to_ac_load : 1; // bit12
        uint8_t bat_to_ac_load : 1; // bit13
        uint8_t rsv1 : 1; // bit14
        uint8_t rsv2 : 1; // bit15
    }bit;
}_line_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t bat_ov : 1; // bit0 电池过压
        uint8_t bat_uv : 1; // bit1 电池欠压
        uint8_t bat_oc : 1; // bit2 电池过流
        uint8_t bus_ov : 1; // bit3 母线过压
        uint8_t bus_uv : 1; // bit4 母线欠压
        uint8_t bus_oc : 1; // bit5 母线过流
        uint8_t relay_flt : 1; // bit6 继电器故障
        uint8_t bat_latch_flt : 1; // bit7 电池锁扣故障
        uint8_t dc_relay_opn : 1; // bit8 直流侧继电器开路
        uint8_t dc_relay_sht : 1; // bit9 直流侧继电器短路
        uint8_t grid_relay_opn : 1; // bit10 电网继电器开路
        uint8_t grid_relay_sht : 1; // bit11 电网继电器短路
        uint8_t load_relay_opn : 1; // bit12 负载继电器开路
        uint8_t load_relay_sht : 1; // bit13 负载继电器短路
        uint8_t sys_hi_temp : 1; // bit14 系统高温
        uint8_t sys_lo_temp : 1; // bit15 系统低温
    }bit;
}sys_fault1_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t grid_ov : 1; // bit0 交流过压
        uint8_t grid_uv : 1; // bit1 交流欠压
        uint8_t grid_of : 1; // bit2 交流过频
        uint8_t grid_uf : 1; // bit3 交流欠频
        uint8_t grid_phase_err : 1; // bit4 交流反相
        uint8_t rsv5 : 1; // bit5  
        uint8_t rsv6 : 1; // bit6  
        uint8_t rsv7 : 1; // bit7
        uint8_t off_grid_volt : 1; // bit8 离网电压异常
        uint8_t off_grid_freq : 1; // bit9 离网频率异常
        uint8_t grid_ol : 1; // bit10 交流过载
        uint8_t rsv11 : 1; // bit11 
        uint8_t ac_lock_fail : 1; // bit12 交流锁相失败
        uint8_t ac_soft_start_fail : 1; // bit13 交流软启动失败
        uint8_t rsv14 : 1; // bit14
        uint8_t rsv15 : 1; // bit15
    }bit;
}sys_fault2_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t pv1_ov : 1; // bit0 PV过压
        uint8_t pv2_ov : 1; // bit1 PV过压
        uint8_t pv3_ov : 1; // bit2 PV过压
        uint8_t pv4_ov : 1; // bit3 PV过压
        uint8_t pv1_oc : 1; // bit4 PV过流
        uint8_t pv2_oc : 1; // bit5 PV过流 
        uint8_t pv3_oc : 1; // bit6 PV过流 
        uint8_t pv4_oc : 1; // bit7 PV过流
        uint8_t pv1_ot : 1; // bit8 PV温度异常
        uint8_t pv2_ot : 1; // bit9 PV温度异常
        uint8_t pv3_ot : 1; // bit10 PV温度异常
        uint8_t pv4_ot : 1; // bit11 PV温度异常
        uint8_t rsv12 : 1; // bit12
        uint8_t rsv13 : 1; // bit13
        uint8_t rsv14 : 1; // bit14
        uint8_t rsv15 : 1; // bit15
    }bit;
}sys_fault3_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t pv1_ov : 1; // bit0 PV过压
        uint8_t pv2_ov : 1; // bit1 PV过压
        uint8_t pv3_ov : 1; // bit2 PV过压
        uint8_t pv4_ov : 1; // bit3 PV过压
        uint8_t pv1_oc : 1; // bit4 PV过流
        uint8_t pv2_oc : 1; // bit5 PV过流 
        uint8_t pv3_oc : 1; // bit6 PV过流 
        uint8_t pv4_oc : 1; // bit7 PV过流
        uint8_t pv1_ot : 1; // bit8 PV温度异常
        uint8_t pv2_ot : 1; // bit9 PV温度异常
        uint8_t pv3_ot : 1; // bit10 PV温度异常
        uint8_t pv4_ot : 1; // bit11 PV温度异常
        uint8_t rsv12 : 1; // bit12
        uint8_t rsv13 : 1; // bit13
        uint8_t rsv14 : 1; // bit14
        uint8_t rsv15 : 1; // bit15
    }bit;
}sys_fault4_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t rsv0 : 1; // bit0  
        uint8_t rsv1 : 1; // bit1 DCDC和DSP出现故障
        uint8_t rsv2 : 1; // bit2 微逆和DSP出现故障
        uint8_t dsp_comm_err : 1; // bit3 DCDC通讯故障
        uint8_t extflash_err : 1; // bit4 外部flash故障
        uint8_t bat_can_err : 1; // bit5 电池组通讯故障
        uint8_t inv_err : 1; // bit6 逆变组通讯故障
        uint8_t rtc_err : 1; // bit7 RTC错误
        uint8_t eeprom_err : 1; // bit8 eeprom错误
        uint8_t bms_err : 1; // bit9 电池组故障
        uint8_t rsv10 : 1; // bit10  
        uint8_t rsv11 : 1; // bit11   
        uint8_t rsv12 : 1; // bit12 
        uint8_t rsv13 : 1; // bit13
        uint8_t rsv14 : 1; // bit14
        uint8_t rsv15 : 1; // bit15
    }bit;
}sys_fault5_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t iso_error; // bit0 绝缘阻抗ISO异常
        uint8_t rsv1 : 1; // bit1
        uint8_t rsv2 : 1; // bit2
        uint8_t rsv3 : 1; // bit3
        uint8_t rsv4 : 1; // bit4
        uint8_t rsv5 : 1; // bit5
        uint8_t rsv6 : 1; // bit6
        uint8_t rsv7 : 1; // bit7
        uint8_t rsv8 : 1; // bit8
        uint8_t rsv9 : 1; // bit9
        uint8_t rsv10 : 1; // bit10
        uint8_t rsv11 : 1; // bit11
        uint8_t rsv12 : 1; // bit12
        uint8_t rsv13 : 1; // bit13
        uint8_t rsv14 : 1; // bit14
        uint8_t rsv15 : 1; // bit15
    }bit;
}sys_alarm1_t;


typedef union {
    uint16_t all;
    struct {
        uint16_t volt : 15;
        uint8_t bal : 1;
    }bit;
}cell_info;

typedef union {
    uint16_t all;
    struct {
        uint8_t enable_chg : 1; //bit0 0: 不能充，1:能充
        uint8_t enable_dsg : 1; //bit1 0: 不能放，1:能放
        uint8_t now_need_chg: 1;//bit2 0: 不需要，1:需要立即充电
        uint8_t limit_chg : 1; //bit3 0:无    1:-10℃不建议大载放电
        uint8_t low_energy: 1; //bit4 低能量
        uint8_t sync_doing: 1; //bit5 并机中
        uint8_t temp_alarm: 1; //bit6温度异常
        uint8_t rsv : 1; //bit7
    }bit;
}chg_enable_t;
typedef union {
    uint16_t all;
    struct {
        uint8_t stop_chg : 1; //bit0 1: 不能充，0:能充
        uint8_t stop_dsg : 1; //bit1 1: 不能放，0:能放
    }bit;
}to_state_t;


typedef struct {
    bool valid;
    uint16_t online;
    uint16_t sync;
    uint16_t timeout;
    uint16_t devId;
    char type[12];
    uint64_t sn_code;
    uint16_t mcu_num;
    uint16_t mcu1_type;
    uint16_t mcu2_type;
    uint32_t version;
    uint32_t m1_version;
    uint16_t packet_total_voltage; // 单包总电压 0.01
    uint16_t packet_total_current; // 单包总电流 0.1
    uint16_t soc; //
    uint16_t soh; //
    uint16_t charging_state;    // 0：空闲；1：充电；2：放电 
    uint32_t total_chg_energy;  // 0.001Kwh
    uint32_t total_dsg_energy;
    uint16_t min_cell_volt;
    uint16_t max_cell_volt;
    uint16_t min_cell_index;
    uint16_t max_cell_index;
    uint16_t avg_temp;
    uint16_t min_temp_value;
    uint16_t max_temp_value;
    uint16_t min_temp_index;
    uint16_t max_temp_index;
    uint16_t used_cycle;
    bms_protect1_t protect1;
    bms_protect2_t protect2;
    bms_error1_t fault1;
    bms_error2_t fault2;
    bms_error3_t fault3;
    uint16_t alarm;
    uint16_t total_cell_num;
    uint16_t total_ntc_num;
    uint16_t reboot_rsp;
    cell_info cell_volt[MAX_CELL_NUM];

    chg_enable_t chg_ctrl; 
    uint16_t online_bit; // 电池通讯在线
    uint16_t sync_bit; // 电池并容
    uint16_t bat_num; // 电池总数
    uint16_t dsg_empty_time; // 电池放空时间
    uint16_t chg_full_time; // 电池充满时间
    uint16_t chg_max_volt; //保留2位小数 200.00v
    uint16_t chg_max_curr; //保留1位小数 20.0A
    uint16_t dsg_max_curr; //保留1位小数 20.0A
    uint16_t total_volt; // 0.01V
    uint16_t total_curr; // 0.1A
    uint16_t run_state; // 11,22,33
    to_state_t state;
}bms_data_t;


typedef union {
    uint16_t all;
}sys_alarm2_t;

typedef union {
    uint16_t all;
}sys_alarm3_t;

typedef union {
    uint16_t all;
}sys_alarm4_t;

typedef struct {
    _line_t energy_line;
    char type[12];
    uint32_t version;
    uint64_t sn_code;
    uint16_t inv_num;
    uint16_t mcu_num;
    uint16_t mcu_type;
    uint16_t time_ctrl_num;
    sys_alarm1_t alarm1;
    sys_alarm2_t alarm2;
    sys_alarm3_t alarm3;
    sys_alarm4_t alarm4;  
    uint16_t pv_max_ntc;
    uint16_t inv_max_ntc;
    sys_fault1_t fault1;
    sys_fault2_t fault2;
    sys_fault3_t fault3;
    sys_fault4_t fault4;
    sys_fault5_t fault5;
} dsp_private_t;


typedef struct {
    uint16_t area;
    uint16_t now[3];
} time_set_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t min : 8; // 分
        uint8_t hour : 8; // 时
    }bit;
} time_group;

typedef struct {
    uint16_t label; // 0x00:Disable；0x01:充电时间；0x02:放电时间；3：待机时间
    time_group start;
    time_group end;
} time_section_t;

typedef struct {
    uint16_t enable;
    uint16_t num;
    time_section_t section[TIME_CTRL_NUM];
} time_ctrl_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t Sun : 1; // bit0
        uint8_t Mon : 1;
        uint8_t Tues : 1;
        uint8_t Wed : 1;
        uint8_t Thurs : 1;
        uint8_t Fri : 1;
        uint8_t Sat : 1; // bit6
        uint8_t enable : 1; // 是否使能
        uint8_t repetition : 1;  // 是否重复
    }bit;
} week_ctrl_t;

typedef struct {
    time_ctrl_t  time_ctrl; // 时间段控制
    week_ctrl_t  week_ctrl; // 按周控制
    uint16_t chg_stop_soc; // 充电停止SOC
    uint16_t dsg_stop_soc; // 放点停止SOC
    uint16_t save_data;
    uint16_t crc16;
} ems_set_t;

typedef struct {
    char password[8]; // 高级设置区密码
    uint16_t inv_volt; // 逆变电压
    uint16_t inv_freq; // 逆变频率
    uint16_t save_data;
    uint16_t crc16;
} share_set_t;




typedef struct {
    uint16_t year;
    uint32_t energy;
}year_history;

typedef struct {
    uint16_t type;
    uint16_t head;
    uint32_t total_energy; // 总能量 0.1kwh
    year_history history[MAX_YEAR_NUM];
    uint16_t crc16;
}history_energy_t;

typedef struct {
    uint32_t pv_total_power; // 1W
    uint32_t grid_total_power; // 馈电 1W

    history_energy_t pv_history_energy; // PV历史能力信息
    history_energy_t grid_history_energy; // 馈电历史能量信息

    now_energy_t pv_now_energy; // 当前PV能量信息
    now_energy_t grid_now_energy; // 馈电电网能量信息
} dsp_eneryg_t;

typedef struct {
    bool valid;
    int timeout;
    uint16_t grid_num;
    now_energy_t     show_now; // 
    history_energy_t show_history; // 
    dsp_private_t   private;
    dsp_eneryg_t    energy;
    dsp_reg0000_t   reg0000;
    dsp_reg0100_t   reg0100;
    dsp_reg0300_t   reg0300;
    dsp_reg0400_t   reg0400;
    dsp_reg0500_t   reg0500;
    dsp_reg0700_t   reg0700;
    dsp_reg0800_t   reg0800;
}dcdc_mi_data_t;


typedef struct {
    dcdc_mi_data_t dc_data; // dcdc数据
    dcdc_mi_data_t mi_data; // 微逆数据
    dcdc_mi_data_t combin_data; // 微逆与DCDC合并数据
    dcdc_mi_data_t show_data; // 手机用于读取
    dcdc_mi_data_t ann_data; // MQTT连接后台后，用于上报的数据 
}inv_data_t;

typedef struct {
    uint16_t bat_index; // 用于兼容电池序号切包
    bms_data_t bms_data[MAX_BMS_NUM]; // Bat1-Batn
    bms_data_t all_data; // 电池总包
    bms_data_t show_data; // 手机用于读取
    bms_data_t ann_data; // MQTT连接后台后，用于上报的数据 
}bat_data_t;

typedef struct {
    uint16_t dsp_switch_state; // 开关组合设置   模拟地址0xFFFF-1
    dsp_reg0300_t   reg0300; // 微逆和DCDC 300
    dsp_reg0400_t   reg0400; // 微逆和DCDC 400
    dsp_reg0500_t   reg0500; // 微逆和DCDC 500
    dsp_reg0700_t   reg0700; // 微逆和DCDC 700
    time_set_t  time_set;  // 系统时间设置   模拟地址0xFFFF-2
    ems_set_t   ems_set;   // DCDC独有设置   模拟地址0xFFFF-3
    share_set_t share_set; // 共有设置  模拟地址0xFFFF-4
    once_set_t  once_set;  // 单次设置  模拟地址0xFFFF-5  
}sys_set_t;
//
//typedef struct {   
//    uint16_t     dsp_switch_state;
//    time_set_t   time_set; // 
//    ems_set_t    ems_set;  // DCDC独有设置
//    share_set_t  share_set; // 共有设置
//    once_set_t   once_set;  // 不需要保存的单次设置
//
//    iot_base_t   iot_base; // IOT基本信息  
//    iot_wifi_cfg_t wifi_set; // IOT配置信息  // 0xFF00
//    iot_wifi_cfg_t sys_wifi_set; // IOT配置信息  
//    mesh_set_t   mesh_set; // mesh设置  
//    mesh_set_t   sys_mesh_set; // mesh设置  
//    ble_cfg_t    ble_cfg; // 蓝牙配置信息 
//
//    iot_about_t  iot_about; // IOT出厂信息 
//    dev_about_t  dev_about; // 设备出厂信息 
//    iot_wifi_t   iot_wifi; // WiFi信息 
//
//}iot_data_t;


//typedef union {
//    uint16_t all;
//    struct {
//        uint16_t support_ble_pwd : 2; // 密码区有效
//        uint16_t visitor_mode : 2; // 访客模式标志
//
//        uint16_t revd1 : 4; //
//        uint16_t revd2 : 8; //        
//    }bit;
//} iot_mode_struct1;

typedef struct {
    uint8_t voltage_level;      // 低字节 0：低压，1：高压
    uint8_t area;               // 高字节 认证地区 0-无效:1-美规;2-日规;3-欧规;4-澳规;5-中国,6-非洲;7-德国:8-台湾
}match_stander_struct;

typedef union {
    uint16_t all;
    struct { 
        uint16_t support_lcd : 1;       // 是否支持磁吸屏, 0:不支持，1:支持
        uint16_t support_shelly : 1;    // 是否支持Shelly电表, APP使用此字段识别是否在首页显示元件列表, 0:不支持，1：支持
        uint16_t meter_s1_enable : 1;       // bit2:是否支持S1插座
        uint16_t modbus_tlv_enable : 1;     // bit3:是否支持ModbusTLV（功能码40004/40005）
        uint16_t revd : 12;             // 预留
    }bits;
} iot_state_t;

typedef struct {
    uint16_t revd0; // 0
    match_stander_struct match_stander;  // 1 AC380的机器标准，低字节 0：低压，1：高压；高字节 0-无效:1-美规;2-日规;3-欧规;4-澳规;5-中国,6-非洲;7-德国:8-台湾
    uint16_t revd1; // 2~5
    iot_state_t iot_state;
    uint16_t revd4[2];
    iot_mode_struct     support_mode;   // 6
	uint8_t app_password[6];    // 7~9
    uint16_t revd2[5]; // 10~14
    uint16_t modbus_ver_iot;    // 15  iot modbus协议版本
	uint16_t modbus_ver;    // 16  modbus协议版本
    uint16_t revd3[4]; // 17~20
}MOD_STRUCT_reg00000;

typedef union {
    uint16_t all;
    struct {
        uint8_t status : 4;
        uint8_t progress : 4;
        uint8_t fault_state : 4;
    };
}aging_status_struct1;
typedef union {
    uint16_t all;
    struct {
        uint8_t power_on : 1;   // bit0 电源开关
        uint8_t ac_on : 1;      // bit1 ac开关
        uint8_t dc_on : 1;      // bit2 dc开关
        uint8_t inv_on : 1;     // bit3 逆变开关
        uint8_t grid_on : 1;    // bit4 电网开关
        uint8_t pv_on : 1;      // bit5 pv开关
        uint8_t fb_on : 1;      // bit6 feedback 馈电
        uint8_t meter_on : 1;   // bit7 电表
        uint8_t led_on : 1;     // bit8 led开关
        uint8_t dc_eco_on : 1;  // bit9 eco开关
        uint8_t super_on : 1;   // bit10 大力士开关
        uint8_t ac_eco_on : 1;
    }bit;
}device_ctrl1;

typedef union {
    uint16_t all;
    struct {
        uint8_t dc_hub_online : 1;              // bit0 DC-HUB在线（只要有一个在线就置1）
        uint8_t ac_hub_set_status : 1;          // bit1 AC-HUB开关状态
        uint8_t grid_meter_online: 1;           // bit2 电网电表在线状态（2011协议添加）
        uint8_t load_meter_online : 1;          // bit3 负载电表在线状态(对应2268)（2011协议添加）
        uint8_t ats_online : 1;                 // bit4  ATS在线状态
        uint8_t remote_off_en : 1;          // bit5 是否支持远程关机功能
        uint16_t revd : 10;                      // bit6~bit15 预留
    }bit;
}Parts_online_struct;

typedef union {
    uint16_t all; // 整体访问
    struct {
        unsigned int PVToBattery        : 1; // 0 光伏给电池充电
        unsigned int LineGridToBattery  : 1; // 1 电网能量流入电池
        unsigned int LineBatteryToGrid  : 1; // 2 电池能量流入Grid
        unsigned int LineAcLoad         : 1; // 3 AC负载能量流
        unsigned int LineDcLoad         : 1; // 4 DC负载能量流
        unsigned int LineBatteryToInvert: 1; // 5 电池放电能量流
        unsigned int LineInvertToBattery: 1; // 6 电池充电能量流
        unsigned int GridToAcload       : 1; // 7 电网能量流入负载
        unsigned int PV_online          : 1; // 8 PV图标
        unsigned int Grid_online        : 1; // 9 电网图标
        unsigned int Load_online        : 1; // 10 负载图标
        unsigned int PVToGrid           : 1; // 11 光伏给电网供电
        unsigned int PVToAcload         : 1; // 12 光伏给AC负载供电
        unsigned int BatToAcload        : 1; // 13 电池给AC负载放电
        unsigned int GridPowerSign      : 1; // 14 Grid总功率正负
        unsigned int rsv                : 1; // 15 保留
    } bit;
} line_event_t;

typedef union {
    uint16_t all;

    struct{
         uint16_t sub1gHz_new_device_found : 1;                      // bit0 Sub1gHz新设备发现模式
         uint16_t ota_state : 1;                      // bit1:升级状态
         uint16_t child_lock_state : 1;                      // bit2:童锁开关状态（2012添加）
         uint16_t remote_off_state : 1;                      // bit3:休眠状态
         uint16_t wifi_mesh_new_device_found : 1;                      // bit4:WIFI-MESH网络新设备发现模式（2012添加）  
         uint16_t fault_alarm_state : 1;                      // bit5:IOT故障告警标志
         uint16_t revd : 10;                                  // bit6~15预留                                    
    }bit;

}Iot_State_struct;

typedef struct {
    uint16_t total_voltage; //  100  0.1V  总电压
    uint16_t total_current; //  101  0.1A  总电流
    uint16_t soc;            //  102  电池SOC
    uint16_t chg_status;     //  103 电池充放电状态
    uint16_t chg_full_time;  // 104 预计充满时间
    uint16_t dsg_empty_time; // 105 预计放空时间
	aging_status_struct1 aging_status;    // 106 维护状态
    
	uint16_t PackCnts	;//107 PACK并包数量
	uint16_t PackOnline	;//108 PACK并包在线状态
    
	uint16_t can_bus_fault;// 109	can总线故障
    char  dev_type[12];   //  110~115  ASCII 机型
    uint64_t dev_sn;      //  116~119  设备唯一识别SN码		
    uint16_t inv_num;    //  120  逆变数量
    uint16_t inv_online;  //  121  逆变在线地址
    uint16_t inv_power_rang; // 122 逆变功率类型[]1: 2 : 3:    (小功率，中功率，大功率)]
    line_event_t line_event;  // 123  能量线 [00:能量线无效；01:能量线有效；PV能量线可接入逆变]
    device_ctrl1 ctrl_status; // 124 开关控制状态16bit
	uint16_t GridandMachineSOC; // 125 并网并机SOC
    uint16_t    alarm[4];       	// 126~129 告警信息
    uint16_t revd2[3]; // 130~132
    uint16_t    fault[5];       	// 133~137 故障信息
    uint16_t revd3[2]; // 138~139
    
    uint32_t DCLoadAllTotalPower; //140~141
    uint32_t ACLoadAllTotalPower; //142~143
    uint32_t PVAllTotalPower; //144~145
    int32_t GridAllTotalPower; //146~147
    int32_t InvAllTotalPower; //148~149
    uint32_t DCLoadTotalEnergy; //150~151
    uint32_t ACLoadTotalEnergy; //152~153
    uint32_t PvTotalChargingEnergy; //154~155
    uint32_t GridTotalChargingEnergy;   //156~157
    uint32_t FeedbackEnergy;    //158~159
    uint16_t  ctrl_chg_mode;  // 160 控制充电模式
    uint16_t  inv_work_state; // 161 逆变工作状态
    
    uint32_t PvToACLoadEnergy;// 162~163
	uint16_t SelfConsumptionPercent;	//164

    uint32_t PVToACloadPower;   // 165~166
    uint32_t PackTotalDsgEnergy;// 167~168

    uint16_t Rated_Voltage;	//169
    uint16_t Rated_Frequency;	//170
    Parts_online_struct Parts_online;	//171 配件在线及开关状态
    uint16_t Fault;	//172 故障
    uint16_t Alarm;	//173 告警
    Iot_State_struct Iot_State;	//174 IOT状态
    uint16_t SceneConfigFlag;//175场景配置过的标志 低字节：IOT同步2274寄存器房车使用场景
    uint16_t TotalEnergy_Car_L;    //176 行车充电总能量低16bit
    uint16_t TotalEnergy_Car_H;    //177 行车充电总能量高16bit
    uint16_t SleepPredictTime_L;    //178 休眠预计续航时间低16bit 单位：min
    uint16_t SleepPredictTime_H;    //179 休眠预计续航时间高16bit
    uint16_t PackTotalChargeEnergyLo;   //180 Pack总充电能量低16bit 单位：0.1kwh
    uint16_t PackTotalChargeEnergyHi;   //181 Pack总充电能量高16bit 单位：0.1kwh
    uint16_t CarTotalPower;             //182 行车充电总功率 单位：1W
    uint16_t ChargingPileTotalPower_L;  //183 充电桩总功率_L
    uint16_t ChargingPileTotalPower_H;  //184 充电桩总功率_H
    uint16_t Iot_test;  //185 
    uint16_t Inv_test;  //186 
    uint16_t Pack_test;  //187 
    inv_status Inv_Flag;  //188
    uint8_t revd_188_L;
    uint16_t PvTotalChargingEnergyToday; // 189 PV当日发电量 0.1kwh
    uint16_t PvTotalChargingEnergyMonth; // 190 PV当月发电量 0.1kwh
    uint16_t SwitchMemoryState;          // 191 开关类记忆状态 bit0：AC bit1: DC bit2: DCHUB（来自0x11 switch_memory_state）
    uint16_t revd4[20];//192~211

}MOD_STRUCT_reg00100;

typedef struct {
    uint16_t begin;//0：不开启OTA只用于查看，1：需要开启单播OTA升级，2：需要开启半广播OTA升级，3：需要开启全广播OTA升级
    uint16_t type;
    uint32_t version;
    uint16_t size;
    ota_group_t group;
}ota_cmd_struct1;
typedef struct {
    uint8_t where;  // 低字节：1：IOT发送给设备；2：设备给子设备；3：服务器发送给IOT；
    uint8_t isOta;  // 0: 未进行OTA; 1: 正在升级-高字节
    uint8_t level;  // 文件传输深度
    uint8_t type;   // 文件类型
    uint8_t errCode;// 错误码
    uint8_t pct;    // 文件传输进度
}ota_group_status_struct1;
typedef struct {
    uint8_t level;  // ota 处于哪个阶段,0：IOT发送给设备，1：设备发送给子设备-低字节
    uint8_t where;  // 文件传输位置
    uint8_t type;   // 文件类型 最高位置1表示正在升级
}ota_group_status_struct2;

typedef struct {
//write area	
    ota_cmd_struct1      ota_cmd;        // 700~705 ota 控制，该结构数据由上层应用下发到IoT
    uint16_t revd1[14]; // 706~719
//read area    
    ota_group_t ota_cmd_group; // 720
    ota_group_status_struct1    ota_group[16];// 721~768 ota 状态 721~ 多余的预留出来
    // ota_group_status_struct1    ota_group[NET_SUB1G_MAX_POINT];  
    //ota_group_status_struct2 ota_group_status[16];
}MOD_STRUCT_reg00700;

typedef struct {
    uint16_t type; // 0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块11-内置HMI模块；12-内置HMI模块2；13--RF穿墙模块；14-DC-HUB；15-AC-HUB；16-DC-DC；
    uint32_t version; // mcu版本号
}soft_version1;

typedef struct {
    uint16_t InvID; // 1100 逆变并机后所分配的ID编号
	
    char  InvType[12];   // 1101~1106 ASCII 机型
    uint64_t InvSN;      // 1107~1110 设备唯一识别码
    uint16_t  inv_power_rang; // 1111 逆变功率类型 1: 2 : 3:    (小功率，中功率，大功率)
    uint16_t  software_total; // 1112 系统中软件版本数量,相同的MCU只显示最低版本
    soft_version1  soft[6];  // 1113~1130 最多6个软件版本
    uint16_t line_event;  // 1131  00:能量线无效；01:能量线有效；PV能量线可接入逆变
    device_ctrl1 ctrl_status_L; //1132	逆变设备开关设置低16bit	按位表示
    device_ctrl1 ctrl_status_H; //1133	逆变设备开关设置高16bit	按位表示
    uint16_t alarm[4]; // 1134~1137 告警信息
    uint16_t revd2[3]; // 1138~1140
    
    uint16_t fault[5]; // 1141~1145 故障信息    
    uint16_t revd3[2]; // 1146~1147
	uint16_t WorkTimeNumber; // 1148
	uint16_t DevVoltageLable;// 1149
	uint16_t InvCanBusError; // 1150
	uint16_t AmbientTemp;   // 1151
	uint16_t InvMaxTemp;    // 1152
	uint16_t PvDcdcMaxTemp; // 1153
	uint16_t INVFirmwareVersionDifferences; // 1154
    uint16_t InvChgLimitL1; // 1155 逆变器L1交流充电限值
    uint16_t InvChgLimitL2; // 1156 逆变器L2交流充电限值
    uint16_t InvChgLimitL3; // 1157 逆变器L13交流充电限值    
    uint16_t InvDisgLimitL1; // 1158 L1交流放电限值
    uint16_t InvDisgLimitL2; // 1159 L2交流放电限值
    uint16_t InvDisgLimitL3; // 1160 L3交流放电限值
    uint16_t machineL1AcInputRatedCurrent; // 1161 机器L1交流输入额定电流
    uint16_t machineL2AcInputRatedCurrent; // 1162 机器L2交流输入额定电流
    uint16_t machineL3AcInputRatedCurrent; // 1163 机器L3交流输入额定电流     
    uint16_t machineL1AcOutputRatedCurrent; // 1164 机器L1交流输出额定电流
    uint16_t machineL2AcOutputRatedCurrent; // 1165 机器L2交流输出额定电流
    uint16_t machineL3AcOutputRatedCurrent; // 1166 机器L3交流输出额定电流   
    uint16_t gridL1AcOutputRatedCurrent; // 1167 电网L1交流输入额定电流（机器+旁路带载）
    uint16_t gridL2AcOutputRatedCurrent; // 1168 电网L2交流输入额定电流（机器+旁路带载）
    uint16_t gridL3AcOutputRatedCurrent; // 1169 电网L3交流输入额定电流（机器+旁路带载）
    uint16_t revd[12];//1170~1181
    soft_version1 soft_b[6];//1182~1199
    //uint16_t revd4[10];//1155~1164
}MOD_STRUCT_reg01100;

typedef struct {
    uint16_t  status;        // 工作状态
    uint16_t  input_type;    // 接入类型 0--PV,1--CAR,2--Adpater,3--other
    uint16_t input_power;   // 输入功率 w
    uint16_t input_voltage; // 输入电压 0.1V
    uint16_t input_current; // 输入电流 0.1A
    uint16_t revd[3];
}pv_detail_struct1;

typedef union {
	uint16_t pv_total;

	struct {
		uint8_t dc_pv_numbers : 4;
		uint8_t ac_pv_numbers : 4;
	};
} pv_numbers_struct1;

typedef struct {
    uint32_t total_chg_power;   //1200~1201 PV总充电功率 1w
    uint32_t total_chg_energy;  //1202~1203 PV总的充电能量 0.1kwh
    uint32_t PVToACloadPower;   //1204~1205 PV到AC负载功率
    uint32_t PvToACLoadEnergy;  //1206~1207 PV到AC负载能量
    uint16_t SelfConsumptionPercent;	//1208 自给率
    pv_numbers_struct1 pv_number;       //1209 pv接入数量
    pv_detail_struct1  pv_detail[10];   //1210~1289 假设最大10路pv
    uint16_t revd[10]; //1290~1299
}MOD_STRUCT_reg01200;

typedef struct {
    int16_t input_power;   // 输入功率 w //M139.3 uint16_t ；windy 有符号，电网到设备为负，设备到电网为正
    uint16_t input_voltage; // 输入电压 0.1V
    int16_t input_current; // 输入电流 0.1A //M139.3 uint16_t
    uint16_t revd[3];
}grid_detail_struct1;

typedef struct {
    uint16_t freq;              // 1300 电网频率 0.1Hz
    int32_t total_chg_power;   // 1301~1302 电网总充电功率 1w；；windy 有符号，电网到设备为负，设备到电网为正
    uint32_t total_chg_energy;  // 1303~1304 0.1kwh 电网充电能量
    uint32_t total_fb_energy;   // 1305~1306 0.1kwh 电网馈电能量
    uint16_t revd[5];           //1307~1311
    uint16_t  grid_phase_number;// 1312 grid相数
    grid_detail_struct1  grid_detail[3];   //1313~1330 假设最大10路grid
    uint16_t revd1[10]; //1331~1340
}MOD_STRUCT_reg01300;

typedef struct {
    uint16_t load_power;    // 1W
    uint16_t load_voltage;  // 0.1V
    uint16_t load_current;  // 0.1A
    uint16_t revd[3];
}ac_load_struct1;

typedef struct {
    uint32_t total_dc_load_power; //1400~1401  w 总的DC负载功率
    uint32_t total_dc_load_energy;  //1402~1403  总的DC负载能量
    uint16_t dc_05v_load_power; //1404  w 5v负载功率
    uint16_t dc_05v_load_current; //1405  0.1A 5v负载电流
    uint16_t dc_12v_load_power; //1406  w 12v负载功率
    uint16_t dc_12v_load_current; //1407  0.1A 12v负载电流
    uint16_t dc_24v_load_power; //1408  w 24v负载功率
    uint16_t dc_24v_load_current; //1409  0.1A 24v负载电流
    uint32_t dc_open_time;  //1410~1411
    uint16_t revd1[8];   //1412~1419
    uint32_t total_ac_load_power; //1420~1421 w 总的AC负载功率
    uint32_t total_ac_load_energy;  //1422~1423 总的AC负载能量
    uint32_t ac_open_time;  //1424~1425
    uint16_t revd2[3];   //1426~1428
    uint16_t ac_phase_number;    //1429 ac输出并机相数
    ac_load_struct1 ac_load[3]; //1430~1447  ac负载信息,根据并机相数 
    uint16_t dc1InputMinVoltage;    //1448 直流1输入最低电压
    uint16_t dc1InputMaxVoltage;    //1449 直流1输入最高电压
    uint16_t dc1InputMaxCurrent;    //1450 直流1输入最大电流
    uint16_t dc1InputMaxPower;      //1451 直流1输入最大功率
    uint16_t dc2InputMinVoltage;    //1452 直流2输入最低电压
    uint16_t dc2InputMaxVoltage;    //1453 直流2输入最高电压
    uint16_t dc2InputMaxCurrent;    //1454 直流2输入最大电流
    uint16_t dc2InputMaxPower;      //1455 直流2输入最大功率

    uint16_t revd3[14]; //1456~1469
}MOD_STRUCT_reg01400;

typedef struct {
    uint16_t work_status;    // 逆变工作状态
    uint16_t power;         // 逆变功率 M139.3 uint16_
    uint16_t voltage;       // 逆变电压
    uint16_t current;       // 逆变电流 M139.3 uint16_
    uint16_t revd[3];
}inv_detail_struct1;

typedef struct {
    uint16_t freq;          // 1500 逆变频率
    uint32_t total_energy;  // 1501~1502 逆变的总能量
    uint16_t revd[5];   //1503~1507
    uint16_t  phase_number;  // 1508 相位数量 最多3相
    inv_detail_struct1  inv_detail[6]; //1509~1529 每相详细信息 最多3相
    uint16_t revd1[13]; //1530~1542
}MOD_STRUCT_reg01500;

typedef struct {
    uint32_t total_energy;  //1600~1601 发电机发电总能量 0.1Kwh
    uint16_t power;     //1602 发电机功率 1w
    uint16_t voltage;   //1603 发电机电压 0.1V
    uint16_t current;   //1604 发电机电流 0.1A
    uint16_t revd[10];   //1605~1614
}MOD_STRUCT_reg01600;

enum {
    METER_STATUS_OFFLINE = 0,
    Meter_STATUS_ONLINE = 1
};

typedef struct {
    uint16_t time_status : 2;    // 1800 工作状态
    uint16_t online : 1;         // 
    uint16_t revd : 13;
}meter_status_t;

/**
 * @brief 电表只读区: 1700~1899
 */
typedef struct {
    uint8_t dev_type[12];               /**< 设备型号 */
    uint64_t dev_sn;                    /**< 设备SN */
    meter_status_t status;                    /**< 工作状态 */
    uint32_t sys_time;                  /**< 系统时间 */
    uint32_t voltageA;
    uint32_t voltageB;
    uint32_t voltageC;                     /**< 1714 C相电压 */
    uint32_t currentA;
    uint32_t currentB;
    uint32_t currentC;
    uint32_t active_powerA;
    uint32_t active_powerB;
    uint32_t active_powerC;
    uint32_t reactive_powerA;
    uint32_t reactive_powerB;
    uint32_t reactive_powerC;
    uint32_t apparent_powerA;
    uint32_t apparent_powerB;
    uint32_t apparent_powerC;
    uint32_t power_factorA;
    uint32_t power_factorB;
    uint32_t power_factorC;
    uint32_t average_voltage;
    uint32_t average_current;
    uint32_t unbalance_current;
    uint32_t total_current;
    uint32_t total_active_power;
    uint32_t total_reactive_power;
    uint32_t total_apparent_power;
    uint32_t total_power_factor;
    uint32_t frequency;
    uint32_t total_import_active_energy;
    uint32_t total_export_active_energy;
    uint32_t total_import_export_active_energy;
    uint32_t import_active_energyA;
    uint32_t export_active_energyA;
    uint32_t import_export_active_energyA;
    uint32_t import_active_energyB;
    uint32_t export_active_energyB;
    uint32_t import_export_active_energyB;
    uint32_t import_active_energyC;
    uint32_t export_active_energyC;
    uint32_t import_export_active_energyC;
    uint16_t revd[20];
    uint32_t last_online_time;                  /**< 最近一次上线时间 基于1970年初始日期的秒计数器 */
}MOD_STRUCT_reg01700;

/**
 * @brief 电表设置区: 1900
 */
typedef struct {
    uint16_t modbus_addr;           /**< modbus地址 */
    uint16_t baud_rate;             /**< 波特率 */
    uint16_t parity_stop_data;      /**< 奇偶校验位、停止位、数据位 */
    uint16_t dev_led;               /**< 设备指示灯 */
    uint16_t reset_energy;          /**< 清除电能量 */
    uint8_t dev_type;               /**< 设备型号 */
    uint8_t mfg_id;                 /**< 厂商类型 */
    uint8_t dev_id[32];             /**< 设备唯一识别码 */
    uint16_t func;                  /**< 功能位 */
}MOD_STRUCT_reg01900;

typedef struct {
    uint8_t minutes; // 分
    uint8_t hours;   // 时
}_section_time1;

typedef struct {
    /*0x00:Disable；0x01:充电时间；0x02:放电时间；3：待机时间
    bit3~0:正常天配置；
    bit7~4:特殊天（周内）配置；
    bit11~8:特殊天（季度内）天配置；
    bit15~12:预留
    */
    uint16_t lable; // 0x00:Disable；0x01:充电时间；0x02:放电时间；3：待机时间
    _section_time start; // 开启时间 (时:分)//M128
    _section_time end;   // 结束时间 (时:分)
}time_control1;

typedef union {
    uint16_t all;
    struct {
        uint8_t clear_all : 1;   // bit0 清除全部能量
        uint8_t clear_dc : 1;      // bit1 清除dc负载能量
        uint8_t clear_ac : 1;      // bit2 清除ac负载能量
        uint8_t clear_pv : 1;     // bit3 清除pv负载能量
        
        uint8_t clear_grid_chg : 1;    // bit4 清除电网充电能量
        uint8_t clear_grid_dsg : 1;     // bit5 清除电网放电能量
        uint8_t clear_log : 1;     // bit6 清除历史记录
        uint8_t clear_fault : 1;     // bit7 清除当前故障
        
		uint8_t clear_chg_dsg_time_period : 1;     // bit8 清除充放电时段
    }bit;
}ctrl_event1;

typedef union {
	uint16_t all;

	struct {
		uint8_t ctrl_meter_enable;
		uint8_t ctrl_meter_select;
	};
}ctrl_meter_struct;

typedef union 
{
    uint16_t all;
    struct {
        uint8_t adv_en : 4;             // bit0~3 server端广播数据使能
        uint8_t refresh_key : 2;        // bit4~5 刷新密钥 0:不刷新 1:刷新 2:恢复默认
        uint8_t comm_adv_en : 2;        // bit4~7 预留
        uint8_t lcd_adv_en : 2;         // bit8~9 LCD数据广播使能(record type为0x80/0x81)
        uint8_t md_adv_en : 2;          // bit10~11 广播modbus读写使能
        uint8_t revd : 4;               // bit12~15 保留
    };
} ble_protocol_en_t;

typedef union 
{
    uint16_t all;
    struct {
        uint8_t sta1_en : 2;                // bit0~1 sta1 使能
        uint8_t sta2_en : 2;                // bit2~3 sta2 使能
        uint8_t sta3_en : 2;                // bit4~5 sta3 使能
        uint8_t sta1_static_ip_en : 2;     // bit6~7 sta1 静态IP使能
        uint8_t sta2_static_ip_en : 2;     // bit8~9 sta2 静态IP使能
        uint8_t sta3_static_ip_en : 2;     // bit10~11 sta3 静态IP使能
        uint8_t revd: 4;
    };
} wifi_mul_sta_en_t;

typedef union
{
    uint16_t all;
    struct {
        uint8_t enable : 2;          // bit0~1 使能
        uint8_t crypt_en : 2;        // bit2~3 加密使能
        uint8_t ctrl_en : 2;         // bit4~5 控制使能
        uint8_t report_en : 2;       // bit6~7 上报使能
        uint8_t revd: 8;
    };
} open_mqtt_en_t;

typedef union
{
    uint16_t all;
    struct {
        uint16_t enable : 2;          // bit0~1 使能
        uint16_t crypt_en : 2;        // bit2~3 加密使能
        uint16_t revd: 12;
    };
}modbus_tcp_en_t;

//低字节：0无效；1：使能；2：不使能
typedef union {
    uint16_t all;
    struct {
        uint8_t en_app_selfset : 1;             // bit0 机器是否支持自定义功能模式
        uint8_t dis_app_selfset : 1;            // bit1 机器是否支持自定义功能模式
        uint8_t en_app_charge_set : 1;          // bit2 表示机器是否允许APP进行充电模式设置
        uint8_t dis_app_charge_set : 1;         // bit3 表示机器是否允许APP进行充电模式设置
        uint8_t en_child_lock : 1;              // bit4 童锁开关使能
        uint8_t dis_child_lock : 1;             // bit5 童锁开关使能禁止
        uint8_t revd : 2;                       // bit6~bit7 预留

        uint8_t self_HiConf_Type;               // 高字节枚举值 设备端物理开关对应功能(见Self_HiConf_Type）；APP侧为灵动开关
    }bit;
}self_configuration_uart;

//低字节：0无效；1：使能；2：不使能
typedef union {
    uint16_t all;

    struct {
        uint8_t remoteEn : 2 ;                  // bit0~1  远程关机使能 0-invalid;1-enable;2-disable;
        uint8_t remoteOffCtrlEn : 2 ;                  // bit2~3  底层是否支持远程关机功能 0-invalid;1-enable;2-disable;
        uint8_t res : 4;                        // bit4~7 预留

        uint8_t sleepTime ;             //  自动休眠天数设置 0-无效，1~7对应1~7天；FF永不休眠
    }bit;
}remote_Set;

//低字节：0无效；1：使能；2：不使能
typedef union {
    uint16_t all;

    struct {
        uint8_t KeepEn : 2 ;                  // bit0~1  最低SOC保有量使能 0-invalid;1-enable;2-disable;
        uint8_t res : 6;                        // bit2~7 预留

        uint8_t socKeep ;             // 最低SOC保有量（1-100%）
    }bit;
}rv_Soc_OwenerShip;

//低字节：0无效；1：使能；2：不使能
typedef union{
    uint16_t all;

    struct {
        uint8_t level ;                  // bit0~1   低字节：0:无效，1：等级1；2：等级2
        uint8_t res ;             // 预留
        }bit;
}level_Switch;





// typedef union {
//     uint16_t all;
//     struct {
//         uint8_t child_lock : 2;            // bit0~1 童锁开关
//         uint16_t revd : 14;                 // bit2~bit15 预留
//     }bit;
// }switch_setting_uart;

typedef struct {
	uint16_t SetInvSelect;//2000
	uint8_t mon;//
	uint8_t year;// 2001年为基准
	uint8_t hour;
	uint8_t day;//2002
	uint8_t sec;
	uint8_t min;//2003	

//    uint16_t  time1;    // 高8bit-年；低8bit-月；
//    uint16_t  time2;    // 高8bit-日；低8bit-时；
//    uint16_t  time3;    // 高8bit-分；低8bit-秒；
    
    time_res_struct  SetTimeZone;//2004 时区
    uint16_t  work_mode;     // 2005  00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网
    ctrl_event1  ctrl;       // 2006 一次性控制事件,
    uint16_t  ctrl_led;       // 2007 控制LED
    ctrl_meter_struct  ctrl_meter;     // 2008 控制电表 (未用)
    uint16_t  ctrl_pv;        // 2009 控制PV
    uint16_t  ctrl_inv;     // 2010 控制逆变
    uint16_t  ctrl_ac;  // 2011 控制ac
    uint16_t  ctrl_dc;  // 2012 控制dc
    uint16_t  ctrl_poweron;  // 2013 控制电源
    uint16_t  ctrl_dc_eco;       // 2014 控制dc eco模式0：关闭，1：开启
    uint16_t  ctrl_dc_eco_time;  // 2015 控制dc eco模式空闲时间
    uint16_t eco_dc_power_value;    // 2016 dc eco模式触发运行功率
    uint16_t  ctrl_ac_eco;       // 2017 控制ac eco模式 0：关闭，1：开启
    uint16_t  ctrl_ac_eco_time; // 2018 控制ac eco模式空闲时间
    uint16_t eco_ac_power_value; // 2019 ac eco模式触发运行功率
    uint16_t  ctrl_chg_mode; // 2020 控制充电模式
    uint16_t  ctrl_super_power; // 2021 控制超级输出模式
    uint16_t  ctrl_low_cap_pct;  // 2022 系统低电量百分比
    uint16_t  ctrl_high_cap_pct; // 2023 系统满电量百分比
    uint16_t  ctrl_inv_mode; // 2024 单火线，双火线，并机，并机双火线
    uint16_t  ctrl_dev_id; // 2025 给逆变设置编号
    uint16_t  ctrl_all_energy_type; // 2026 历史能量
    uint16_t  ctrl_now_energy_type; // 2027 当前能量
    uint16_t  ctrl_log_page;   // 2028 控制历史记录页
    uint16_t  ctrl_time_area; // 2029 控制时间分段管理
    time_control1  ctrl_time[10];//2030~2059
    uint16_t ctrl_PvType[6];//2060~2065
    uint16_t ctrl_alarm_voice;//2066
    uint16_t ctrl_lcd_active_time;//2067 LCD显示屏显示时间 1：15s；2: 30s；3: 1分；4：5分；5：常亮
    int16_t P_inv_active_target_L[3];//2068 2069 2070设备输出为正（馈电：0~32767）；设备输入为负（买电：-32768~0）,3相
	uint16_t microInv_rate;//2071 阳台光伏额定AC功率
    self_configuration_uart self_config;// 2072 自定义模式
    // switch_setting_uart on_off_set;// 2073 开关类设置
    remote_Set remoteSet;// 2073 远程设置
    uint16_t remoteSoc;//2074 远程设置相关SOC阈值
    rv_Soc_OwenerShip ownerShip;//2075 房车SOC保有量设置
    level_Switch LevelSwitch;//2076 童锁开关的等级开关
    uint16_t sleepRemainTime;//2077 休眠剩余时间
    led_color_setting ledColorSet;//2078 LED颜色设置
    uint16_t remote_set_power;//2079 远程设置功率阈值
    number_show pack_set_show;//2080 电池包数量
    number_show inv_set_show;//2081 INV数量
    number_show dcdc_set_show;//2082 DCDC数量
    soc_max_ownership soc_max_ownership_set;//2083 soc最高保有量设置
    pv_senior_setting pv_senior_set;     //2084 pv高级设置
    uint16_t DC_output;//2085 DC输出
    uint16_t Regulatory_set;//2086 合规设置
    uint16_t Cycle_capacity;//2087 循环容量
    uint16_t Cycle_max_capacity;//2088 最大循环容量
    uint16_t Effective_time_ym;//2089 生效时间年月
    uint16_t Effective_time_dh;//2090 生效时间日时
    uint16_t Effective_time_ms;//2091 生效时间分秒
    uint16_t ECO_status;//2092
    uint16_t set_AC_branch;//2093 控制AC支路
    uint16_t set_DC_branch;//2094 控制DC支路
    uint16_t revd[5];//2095~2099 
}MOD_STRUCT_reg02000;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint8_t ct_enable : 8;//低字节：CT测试使能；
			uint8_t ct_result : 8;//高字节：CT测试结果
		};
	};
} ct_test_struct1;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint16_t ct_grid : 8; //电网端口CT比例
			uint16_t ct_ac_pv : 8; //交流光伏CT比例
		};
	};
} ct_ratio_struct1;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint8_t ats_en : 2;					// ATS使能
			uint8_t multi_inv_off_grid_en : 2;	// 离网侧并机使能
			uint8_t reset_param : 2;			// 恢复默认参数
			uint8_t fuda_wifi_en : 2;			// 福达wifi使能
			uint8_t gen_en : 2;					// 发电机使能
			uint8_t micro_inv_en : 2;			// 微逆使能
			uint8_t power_op_en : 2;			// 功率优化器使能
			uint8_t on_grid_en : 2;				// 并网使能
		};
	};
} ctrl_mix_struct1;

typedef struct {
	union {
		uint16_t all;

		struct {
            uint8_t charging_pile_en:2;         //充电桩使能
            uint8_t low_soc_maintenance_en:2;         //SOC低电量维持使能
            uint8_t Hardware_limited_en:2;         //硬件限制输出使能位（逆变器的输出功率）    
            uint8_t Generator_inverter_combined_load_en:2;         //发电机+逆变联合带载使能（EP760）  

            uint8_t balanced_en:2;         //均衡模式使能(EP600)
            uint8_t Winter_mode_en:2;         //冬季模式使能（EP600\760):离网状态下SOC很低时设备停止放电
            uint8_t single_phase_maxcurrent_en:2;         //单相电网最大输入电流设置使能（EP800/900）当关闭时底层执行最大值48处理）
            uint8_t DRMS_en:2;         //DRMS使能(用户开启后，逆变器检测某DI口关机)    
		};
	};
} ctrl_mix2_struct1;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint8_t enable : 2;
			uint8_t reset : 2;
			uint8_t res1 : 2;
			uint8_t res2 : 2;
		};
	};
} bat_maintain_struct1;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint8_t state : 4;
		};
	};
}ems_ctrl_struct1;

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t GenSet_enable : 2;
            uint8_t SetTimeCtrl1 : 2;
            uint8_t SetTimeCtrl2 : 2;
            uint8_t SetTimeCtrl3 : 2;

            uint8_t revd;
        };
    };
}GenSet_struct;

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t minute;
            uint8_t hour;
        };
    };
}SetTimeCtrl_struct;

typedef struct{
    
    uint16_t revd;
    SetTimeCtrl_struct SetTimeCtrlStart;
    SetTimeCtrl_struct SetTimeCtrlEnd;
    
}SetTimeCtrl_struct2;




typedef struct {
    uint8_t password[8]; //2200~2203 设置区密码
    uint16_t revd1[2];  //2204~2205
    uint16_t ctrl_reset_factory; //2206 恢复出厂
    uint16_t ctrl_grid;      //2207 控制电网
    uint16_t ctrl_feedback;  //2208 控制馈电
    uint16_t ctrl_output_inv_volt;  //2209 输出电压控制
    uint16_t ctrl_output_inv_freq;   //2210 输出电压频率
    uint16_t ctrl_chg_max_volt; //2211 最大充电电压
    uint16_t ctrl_pv_max_curr; //2212 最大PV充电电流
    uint16_t ctrl_grid_max_power;   //2213 电网最大功率
    uint16_t ctrl_grid_max_curr; //2214 电网最大电流
    uint16_t ctrl_feedback_max_power;   //2215 电网馈电最大功率
    uint16_t ctrl_feedback_max_curr; //2216 电网馈电最大电流
    uint16_t off_grid_micro_rated_power; //2217 离网交流PV额定功率（该参数设置为系统实际接入的微逆的额定功率，使得逆变根据实际情况更准确的调节微逆功率）
    uint16_t  ctrl_user_area; //2218 用户地区设置
    uint16_t  ctrl_pv_paralle[6]; //2219~2224 设置PV并联开启和关闭
    uint16_t  ctrl_grid_plus;    //2225 电网增强模式
    uint16_t  ctrl_save_power_state; //2226 保存AC,DC关机前状态
    uint16_t  ctrl_meter_enable; //2227 电表开启
    uint16_t  ctrl_meter_select; //2228 电表类型选择
    uint16_t  ctrl_Inv_Multi_enable; //2229 逆变器并机模式
    uint16_t  ctrl_Inv_addr_Set; //2230 设置逆变器地址
    ct_test_struct1 ct_test;    //2231
	ctrl_mix_t ctrl_mix;  //2232 ctrl_mix_struct1
	bat_maintain_struct1 bat_maintain;  //2233  
	uint16_t P_inv_active_target_L1; //2234
	uint16_t P_inv_reactive_target_L1;//2235
	uint16_t P_inv_active_target_L2; //2236
	uint16_t P_inv_reactive_target_L2;//2237
	uint16_t P_inv_active_target_L3; //2238
	uint16_t P_inv_reactive_target_L3;//2239
	uint16_t P_PV_Limit;//2240
	ems_ctrl_struct1 ems_ctrl;//2241
	ctrl_mix2_struct1 ctrl_mix2;//2242
    uint16_t    ChargingPile_SET;  //2243 充电桩电压设置 //充电桩型号 chargingpile_type
    ct_ratio_struct1  ct_ratio;           //2244 CT比例
    ct_test_struct1   ac_ct_test;         //2245 交流耦合电表CT检测
    GenSet_struct GenSet; //2246
    uint16_t Gen_SOC_Start;//2247
    uint16_t Gen_SOC_Stop;//2248
    SetTimeCtrl_struct2 SetTimeCtrl[3];//2249~2257
    uint16_t Undervoltage_protection; //2258 欠压保护设置值
    uint16_t Undervoltage_protection_time; //2259 欠压保护设置时间
    uint16_t Highvoltage_protection; //2260 高压保护设置值
    uint16_t Highvoltage_protection_time; //2261 高压保护设置时间
    uint16_t Underfrequency_protection; //2262 欠频保护设置值
    uint16_t Underfrequency_protection_time; //2263 欠频保护设置时间
    uint16_t Overvoltage_protection; //2264 过频保护设置值
    uint16_t Overvoltage_protection_time; //2265 过频保护设置时间
    // uint16_t revd[8];//2258~2265 便携
    uint16_t CounterCurrentPower_Limit;//2266
    ctrl_meter_struct  ctrl_meter;     // 2267 控制电表(同2008)
    uint16_t Meter_Select;//2268 低字节：电表接入选择
	uint16_t SetCtrlPv;//2269 设置PV开启关闭
    uint16_t Phase_set;//2270 单相双火相设置
    uint16_t DCHUB_set;//2271 DCHUB设置电压_AC380设置
    uint16_t SetGridMaxCurrent_in;//2272 最大电网输入电流
    func_set_struct Func_Set;// 2273 开关设置
    uint16_t RvSettings;// 2274 房车场景及电池设置
    uint16_t revd[46];//2275~2320
}MOD_STRUCT_reg02200;
//
//
//typedef struct {
//    /*2400~2409*/
//    uint16_t SetGridEnable;          // 高级功能使能
//    uint16_t SetGridCountry;         // 电网认证使用地区
//    uint16_t SetGridUV1Value;        // 电网欠压LEVEL1保护值
//    uint16_t SetGridUV1Time;         // 电网欠压LEVEL1触发时间
//    uint16_t SetGridUV2Value;        // 电网欠压LEVEL2保护值
//    uint16_t SetGridUV2Time;         // 电网欠压LEVEL2触发时间
//	uint16_t pf;					 // 功率因数
//	uint16_t set_region;			 // 地区
//	uint16_t mode_enable;			 // 模式使能
//	uint16_t power_rate_limit;		 // 每分钟功率变化比率
//	/*2410~2417*/
//    uint16_t SetGridAVGOVValue;      // 电网均值过压
//    uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
//    uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
//    uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
//    uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
//	uint16_t power_reactive_to_active_ratio; // 无功占额定有功比例
//	uint16_t vw1;					 // Volt–watt响应模式的参数,Vw1<Vw2
//	uint16_t vw2;					 // Volt–watt响应模式的参数,Vw1<Vw2
//	/*2418*/
//    uint16_t revd1[1];
//    /*2419~2430*/
//    uint16_t SetGridUF1Value;        // 电网欠频LEVEL1保护值
//    uint16_t SetGridUF1Time;         // 电网欠频LEVEL1保护时间
//    uint16_t SetGridUF2Value;        // 电网欠频LEVEL2保护值
//    uint16_t SetGridUF2Time;         // 电网欠频LEVEL2保护时间
//	uint16_t vv1;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
//	uint16_t vv2;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
//	uint16_t vv3;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
//	uint16_t vv4;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
//    uint16_t SetGridOF1Value;        // 电网过频LEVEL1保护值
//    uint16_t SetGridOF1Time;         // 电网过频LEVEL1保护时间
//    uint16_t SetGridOF2Value;        // 电网过频LEVEL2保护值
//    uint16_t SetGridOF2Time;         // 电网过频LEVEL2保护时间
//    /*2431~2434*/
//    uint16_t revd2[4];
//    /*2435~2440*/
//    uint16_t SetGridVoltMinValue;    // 电网重连电压下限值
//    uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
//    uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
//    uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
//    uint16_t SetGridRetryTime;       // 电网异常重连时间
//	uint16_t SetDefaultValue;        // 设置默认值
//    /// @brief 预留区 2441~2449
//    uint16_t  revd3[9];                // 保留
//}MOD_STRUCT_reg02400;

#define MOD_STRUCT_reg02300 inv_set02_struct// 

#define MOD_STRUCT_reg02400 inv_set03_struct// 

typedef struct {
	union {
		uint16_t all;

		struct {
            uint8_t set_islet_enable : 2; // 428 孤岛使能 1：开启，0：禁止
            uint8_t set_lvrt_enable : 2; // 429 lvrt使能  1：开启，0：禁止
            uint8_t set_over_freq_enable : 2; // 430 过频降载使能  1：开启，0：禁止
            uint8_t set_gfci_enable : 2; //431 GFCI使能
            uint8_t set_iso_enable : 2; //432 ISO使能
            uint8_t set_rsv_enable : 4; //433 预留使能
		};
	};
}dsp_switch_struct1;

typedef struct {
    /*2500~2504*/
    uint16_t set_boot_mode; // 302
    uint16_t set_ap_ctrl; //435 有功控制模式
    uint16_t set_rp_ctrl; //436 无功控制模式
    uint16_t set_ap_value; //437 有功功率设置
    uint16_t set_power_slope; //438 功率变化率
    /*2505~2509*/
    uint16_t set_inv_max_power; // 400 0-5000 1W
    uint16_t set_pv_max_power; // 401 0-5000 1W
    uint16_t set_pv_start_volt; // 402 0-5000 0.1V PV启动电压
    uint16_t set_pv_stop_volt; // 403 0-5000 0.1V PV停止电压
    uint16_t set_pv_max_curr; // 404 0-5000 0.1A PV最大电流
    /*2510~2515*/
    uint16_t set_rp_power; // 408 
    uint16_t set_rp_factor; // 409
//    uint16_t set_ac_max_curr; // 405 0-500 0.1A 最大交流电流
//    uint16_t set_grid_max_volt; // 406 0.1V 额定电网电压
//    uint16_t set_grid_max_freq; // 407 0.01Hz  额定电网频率
//    uint16_t set_feedback_power; // 410 馈电最大功率 1W
//    uint16_t set_feedback_curr;  // 411 馈电最大电流 0.1A
    uint16_t set_over_freq_freq1; //412 -500 - 500 0.01Hz
    uint16_t set_over_freq_freq2; //413 -500 - 500 0.01Hz
    uint16_t set_over_freq_power1; //414 -500 - 500 pu
    uint16_t set_over_freq_power2; //415 -500 - 500 pu
    /*2516~2519*/
    uint16_t set_volt_adj_volt[4]; //416-419 -500 - 500 0.01Hz
    /*2520~2523*/
    uint16_t set_volt_adj_power[4]; //420-423 -500 - 500 0.01Hz
    /*2524~2527*/
    uint16_t set_lvrt_rp_max_volt; // 424 最大无功电压 0.1V
    uint16_t set_lvrt_rp_max_power; // 425 最大无功功率
    uint16_t set_lvrt_rp_min_volt; // 426 最小无功电压
    uint16_t set_lvrt_rp_min_power; // 427 最小无功功率
    /*2528 DSP开关使能-按位*/
    dsp_switch_struct1 dsp_switch_state;
//    uint16_t set_islet_enable; // 428 孤岛使能 1：开启，0：禁止
//    uint16_t set_lvrt_enable; // 429 lvrt使能  1：开启，0：禁止
//    uint16_t set_over_freq_enable; // 430 过频降载使能  1：开启，0：禁止
//    uint16_t set_gfci_enable; //431 GFCI使能
//    uint16_t set_iso_enable; //432 ISO使能
//    uint16_t set_rsv_enable; //433 预留使能
//    uint16_t set_auth_ncb; // 434 认证国家机构
//    uint16_t set_ap_ctrl; //435 有功控制模式
//    uint16_t set_rp_ctrl; //436 无功控制模式
//    uint16_t set_ap_value; //437 有功功率设置
//    uint16_t set_power_slope; //438 功率变化率
//    uint16_t set_grid_max_power; // 439 1w 电网最大功率
//    uint16_t set_grid_max_curr; // 440 0.1A 电网最大电流
//    uint16_t set_auth_enable; //441 认证开启/关闭，0：关闭，1开启
    /*2529~2532 温度上限*/
    uint16_t max_ntc[4];//530~533
    /*2533~2538*/
    uint16_t gfci_trigger_time; // 534 10ms
    uint16_t iso_trigger_time; // 535 10ms
    uint16_t pv_over_volt;// 536 0.1V
    uint16_t pv_over_curr; //537 0.1A
    uint16_t pv_over_volt_time; // 538 10ms
    uint16_t pv_over_curr_time; // 539 10ms
    /*2539~2544*/
    uint16_t ac_over_curr3;//524
    uint16_t ac_over_curr3_time;//525
    uint16_t ac_over_curr2;//526
    uint16_t ac_over_curr2_time;//527
    uint16_t ac_over_curr1;//528
    uint16_t ac_over_curr1_time;//529
//    uint16_t grid_retry_max_volt; // 540 0.1v 电网重连电压上限
//    uint16_t grid_retry_min_volt; // 541 0.1V 电网重连电压下线
//    int16_t grid_retry_max_freq; // 542 0.01Hz 电网重连频率上限
//    int16_t grid_retry_min_freq; // 543 0.01Hz 电网重连频率下线	
}MOD_STRUCT_reg02500;

typedef union {
    uint16_t all;
    struct {
        uint8_t type;
        uint8_t happened;
    }bytes;
}fault_state1;

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    fault_state1 each;
    uint16_t fault_code;
}_log_struct1;

typedef struct {
    uint16_t total_page;        //3000 总的历史页
    uint16_t current_page_seq;  //3001 当前历史页编号
    _log_struct1 log_data[5];   //3002~3026
    uint16_t revd[10];//3027~3036
}MOD_STRUCT_reg03000;

typedef struct {
    uint16_t year;
    uint32_t energy;
}_year_energy1;

typedef struct {
    uint16_t energy_type;//3500
    uint32_t total_energy;//3501~3502
    _year_energy1 year_data[15];//3503~3547
    uint16_t revd[10];
}MOD_STRUCT_reg03500;

typedef struct {
    uint16_t energy_type;//3600
    uint16_t year;//3601
    uint32_t total_energy;//3602~3603
    uint32_t mouth_energy   [12]; //3604~3627 0.1Kwh
    uint16_t day_enengy[31]; //3628~3658 0.1Kwh
    uint16_t revd[10];
}MOD_STRUCT_reg03600;

typedef struct {
    /*4000~4009*/
    state_a_t state_a; // 0~4 状态量A-E，用于显示
    state_b_t state_b;
    state_c_t state_c;
    state_d_t state_d;
    state_e_t state_e;
    alram_a_t alarm_a; // 5~9 告警量A-E 用于系统故障
    alram_b_t alarm_b;
    alram_c_t alarm_c;
    alram_d_t alarm_d;
    alram_e_t alarm_e;
    /*4010~4022*/
    uint16_t pv_volt[PV_MAX_NUM]; //100-103 PV电压，0 - 5000 0.1V
    uint16_t pv_curr[PV_MAX_NUM]; //104-107 PV电压，0 - 500  0.AV
    uint16_t pv_total_power; //108 PV总功率 0 - 5000 1W
    uint16_t pv_power[PV_MAX_NUM]; //109-112 PV功率 0 - 5000 1W
    /*4023~4029*/
    uint16_t grid_volt; //113 0-5000  0.1V
    uint16_t grid_curr; //114 0-500 0.1A
    uint16_t grid_freq; //1115 0-9000 0.01Hz
    uint16_t grid_p_power; //116 有功功率 1W
    uint16_t grid_q_power; //117 无功功率 1Var
    uint16_t grid_s_power; //118 视在功率 1VA
    uint16_t grid_pf_value; //119 电网功率因数 0.001
    /*4030~4035*/
    uint16_t iBusVolt1;        //data 122: 母线电压1：0.0~500.0V，*10
    uint16_t iBusVolt2;        //data 123: 母线电压2：0.0~500.0V，*10
    uint16_t iVpkVolt1;        //data 124: Vpk电压1：0.0~500.0V，*10
    uint16_t iVpkVolt2;        //data 125: Vpk电压2：0.0~500.0V，*10
    uint16_t iVpkVolt3;        //data 126: Vpk电压3：0.0~500.0V，*10
    uint16_t iVpkVolt4;        //data 127: Vpk电压4：0.0~500.0V，*10
    /*4036~4039*/
    uint16_t iITRCurrent1;     //data 128: ITR电流1：0.0~50.0A，*10
    uint16_t iITRCurrent2;     //data 129: ITR电流2：0.0~50.0A，*10
    uint16_t iITRCurrent3;     //data 130: ITR电流3：0.0~50.0A，*10
    uint16_t iITRCurrent4;     //data 131: ITR电流4：0.0~50.0A，*10
    /*4040~4045*/
    uint16_t pv_ntc[PV_MAX_NUM]; //132-135 pv温度 1°
    uint16_t inv_ntc[2]; // 136-137 1°
    /*4046~4049*/
    uint16_t mcu_version_h; // 138 33
    uint16_t mcu_version_l; // 139 13793
    uint16_t inv_work_state; // 141 0：停机；1：离网运行；2：电网带载；3：并网运行；4：并网充电；5：并网放电；
    uint16_t machine_type; //142 DSP设备类型,DCDC/微逆
}MOD_STRUCT_reg04000;

typedef struct {
    /*4050*/
    uint16_t revd;
    /*4051~4057*/
    uint16_t set_inv_id; // 300 0-10
    uint16_t set_inv_mode; // 301  
    uint16_t set_boot_mode; // 302
    uint16_t off_grid_frequency_level; // 309
    uint16_t power_change_mode; // 314
    uint16_t off_grid_operation_mode; //315
    uint16_t off_grid_voltage_level; //316
    /*4058~4062*/
    uint16_t system_poweroff; // 317 开机指令 逆变开启和关闭
    uint16_t shutdown_command; // 318关机指令
    uint16_t standby_command; // 319 待机指令
    uint16_t reset_command; // 320 复位指令
    uint16_t aging_command; // 322 老化指令
    /*4063~4067*/
    uint16_t set_inv_max_power; // 400 0-5000 1W
    uint16_t set_pv_max_power; // 401 0-5000 1W
    uint16_t set_pv_start_volt; // 402 0-5000 0.1V PV启动电压
    uint16_t set_pv_stop_volt; // 403 0-5000 0.1V PV停止电压
    uint16_t set_pv_max_curr; // 404 0-5000 0.1A PV最大电流
    /*4068~4070*/
    uint16_t set_ac_max_curr; // 405 0-500 0.1A 最大交流电流
    uint16_t set_grid_max_volt; // 406 0.1V 额定电网电压
    uint16_t set_grid_max_freq; // 407 0.01Hz  额定电网频率
    /*4071~4072*/
    uint16_t set_rp_power; // 408 
    uint16_t set_rp_factor; // 409
    /*4073~4074*/
    uint16_t set_feedback_power; // 410 馈电最大功率 1W
    uint16_t set_feedback_curr;  // 411 馈电最大电流 0.1A
    /*4075~4078*/
    uint16_t set_over_freq_freq1; //412 -500 - 500 0.01Hz
    uint16_t set_over_freq_freq2; //413 -500 - 500 0.01Hz
    uint16_t set_over_freq_power1; //414 -500 - 500 pu
    uint16_t set_over_freq_power2; //415 -500 - 500 pu
    /*4079~4086*/
    uint16_t set_volt_adj_volt[4]; //416-419 -500 - 500 0.01Hz
    uint16_t set_volt_adj_power[4]; //420-423 -500 - 500 0.01Hz
    /*4087~4090*/
    uint16_t set_lvrt_rp_max_volt; // 424 最大无功电压 0.1V
    uint16_t set_lvrt_rp_max_power; // 425 最大无功功率
    uint16_t set_lvrt_rp_min_volt; // 426 最小无功电压
    uint16_t set_lvrt_rp_min_power; // 427 最小无功功率
    /*4091~4096*/
    uint16_t set_islet_enable; // 428 孤岛使能 1：开启，0：禁止
    uint16_t set_lvrt_enable; // 429 lvrt使能  1：开启，0：禁止
    uint16_t set_over_freq_enable; // 430 过频降载使能  1：开启，0：禁止
    uint16_t set_gfci_enable; //431 GFCI使能
    uint16_t set_iso_enable; //432 ISO使能
    uint16_t set_rsv_enable; //433 预留使能
    /*4097~4104*/
    uint16_t set_auth_ncb; // 434 认证国家机构
    uint16_t set_ap_ctrl; //435 有功控制模式
    uint16_t set_rp_ctrl; //436 无功控制模式
    uint16_t set_ap_value; //437 有功功率设置
    uint16_t set_power_slope; //438 功率变化率
    uint16_t set_grid_max_power; // 439 1w 电网最大功率
    uint16_t set_grid_max_curr; // 440 0.1A 电网最大电流
    uint16_t set_auth_enable; //441 认证开启/关闭，0：关闭，1开启
}MOD_STRUCT_reg04050;

typedef struct {
    /*4105~4110*/
    uint16_t grid_over_volt3; // 500 0-5000 0.1V  
    uint16_t grid_over_volt3_time; // 501 0-30000 10ms
    uint16_t grid_over_volt2; // 502 0-5000 0.1V  
    uint16_t grid_over_volt2_time; // 503 0-30000 10ms
    uint16_t grid_over_volt1; // 504 0-5000 0.1V
    uint16_t grid_over_volt1_time; // 505 0-30000 10ms
    /*4111~4116*/
    uint16_t grid_under_volt3; // 506 0-5000 0.1V  
    uint16_t grid_under_volt3_time; // 506 0-30000 10ms
    uint16_t grid_under_volt2; // 508 0-5000 0.1V  
    uint16_t grid_under_volt2_time; // 508 0-30000 10ms
    uint16_t grid_under_volt1; // 510 0-5000 0.1V
    uint16_t grid_under_volt1_time; // 511 0-30000 10ms
    /*4117~4122*/
    int16_t grid_over_freq3; //512 -550 - 550 0.01Hz
    int16_t grid_over_freq3_time; //513 10ms
    int16_t grid_over_freq2; //514 -550 - 550 0.01Hz
    int16_t grid_over_freq2_time; //515 10ms
    int16_t grid_over_freq1; //516 -550 - 550 0.01Hz
    int16_t grid_over_freq1_time; //517 10ms
    /*4123~4128*/
    int16_t grid_under_freq3; //518 -550 - 550 0.01Hz
    int16_t grid_under_freq3_time; //519 10ms
    int16_t grid_under_freq2; //520 -550 - 550 0.01Hz
    int16_t grid_under_freq2_time; //521 10ms
    int16_t grid_under_freq1; //522 -550 - 550 0.01Hz
    int16_t grid_under_freq1_time; //523 10ms
    /*4129~4134*/
    uint16_t ac_over_curr3; //524  0.1A
    uint16_t ac_over_curr3_time; //525 10ms
    uint16_t ac_over_curr2; //526  0.1A
    uint16_t ac_over_curr2_time; //527 10ms
    uint16_t ac_over_curr1; //528  0.1A
    uint16_t ac_over_curr1_time; //529 10ms    
    /*4135~4138*/
    uint16_t max_ntc[4]; // 530-533
    /*4139~4144*/
    uint16_t gfci_trigger_time; // 534 10ms
    uint16_t iso_trigger_time; // 535 10ms
    uint16_t pv_over_volt;// 536 0.1V
    uint16_t pv_over_curr; //537 0.1A
    uint16_t pv_over_volt_time; // 538 10ms
    uint16_t pv_over_curr_time; // 539 10ms
    /*4145~4150*/
    uint16_t grid_retry_max_volt; // 540 0.1v 电网重连电压上限
    uint16_t grid_retry_min_volt; // 541 0.1V 电网重连电压下线
    int16_t grid_retry_max_freq; // 542 0.01Hz 电网重连频率上限
    int16_t grid_retry_min_freq; // 543 0.01Hz 电网重连频率下线
    uint16_t grid_retry_time; //544 1-30000 1s
    uint16_t grid_avg_volt ; // 545 0.1V
    /*4151~4158*/
    uint16_t set_pv_adj_volt1_value; // 700 pv电压1校准系数
    uint16_t set_pv_adj_volt2_value; // 701 pv电压2校准系数
    uint16_t set_pv_adj_volt3_value; // 702 pv电压3校准系数
    uint16_t set_pv_adj_volt4_value; // 703 pv电压4校准系数
    uint16_t set_pv_adj_curr1_value; // 704 pv电流1校准系数
    uint16_t set_pv_adj_curr2_value; // 705 pv电流2校准系数
    uint16_t set_pv_adj_curr3_value; // 706 pv电流3校准系数
    uint16_t set_pv_adj_curr4_value; // 707 pv电流4校准系数
    /*4159~4160*/
    uint16_t set_grid_adj_volt_value; // 708 
    uint16_t set_grid_adj_curr_value; // 709 
    /*4161~4164*/
    uint16_t set_vpk_adj_volt1_value; // 710 
    uint16_t set_vpk_adj_volt2_value; // 711 
    uint16_t set_vpk_adj_volt3_value; // 712 
    uint16_t set_vpk_adj_volt4_value; // 713 
    /*4165~4168*/
    uint16_t set_itr_adj_curr1_value; // 714 
    uint16_t set_itr_adj_curr2_value; // 715 
    uint16_t set_itr_adj_curr3_value; // 716 
    uint16_t set_itr_adj_curr4_value; // 717 
    /*4169*/
    uint16_t set_dc_mode; // 800 设置逆变工作模式0：PV-微逆，旁路 1：BAT-微逆，放电 2：PV-BAT，充电 3：STOP停机
    /*4170*/
    bat_state_t bat_state; // 801 电池状态
    /*4171~4176*/
    uint16_t bat_soc; // 802 电池soc
    uint16_t bat_soh; // 803 电池soh
    uint16_t bat_max_chg_curr; // 804 电池最大充电电流 0.1A
    uint16_t bat_max_dsg_curr; // 805 电池最大放电电流 0.1A
    uint16_t bat_total_volt; // 806 电池总电压 0.1V
    uint16_t bat_total_curr; // 807 电池总电流 0.1A
    /*4177*/
    uint16_t dcdc_relay_cmd; // 808 bit0: 0：保持；1：动作
}MOD_STRUCT_reg04105;


typedef union {
    uint16_t all;
    struct {
        uint8_t chg_enable : 1;     // 1：允许充电；0：禁止
        uint8_t dsg_enable : 1;     // 1：允许放电；0：禁止
        uint8_t chg_now : 1;        // 1：紧急补电
        uint8_t chg_protect : 1;    // 1：充电进入保护
        uint8_t dgs_protect : 1;    // 1：放电进入保护
        uint8_t fault : 1;          // 1：系统故障
		uint8_t battery_heat : 1;   // 1：电池加热
		uint8_t battery_balance : 1;// 1：电池均衡
    }bit;
}status1_byte1;

typedef union {
    uint16_t all;
    struct {
        uint8_t chg_alarm : 1; // 1：充电告警；0：消除
        uint8_t dsg_alarm : 1; // 1：放电告警；0：消除
    }bit;
}status1_byte2;

typedef struct {
    uint16_t is_high_volt;  //6000 所属电压平台
    uint16_t pack_cnt;   //6001 pack并机数量
    uint16_t online;    //6002 最大并机16台，响应的位置1表示，在线 - 通讯并机，bit0~15依次表示不同bat状态
    uint16_t total_voltage; //6003 0.1V  总电压
    uint16_t total_current; //6004 0.1A  总电流
    uint16_t soc;            //6005 1% 系统电流
    uint16_t soh;        //6006 0-100 1%
    uint16_t avg_temp;       //6007 平均温度
    uint16_t work_status;    //6008 工作状态
    uint16_t chg_status;     //6009 充电状态
    uint16_t max_chg_voltage; //6010 最大充电电压
    uint16_t max_chg_current; //6011 为0时，不能充电
    uint16_t max_dsg_current; //6012 为0时，不能放电
    status1_byte1 status1;     //6013 状态标记位1
    status1_byte2 status2;     //6014 状态标记位2
    uint16_t revd1[1];//6015 
    uint16_t cap_online; //6016 最大并机16台，响应的位置1表示，主mos输出打开 - 容量并机
    uint16_t chg_full_time;  //6017 预计放空时间
    uint16_t dsg_empty_time; //6018 预计充满时间
    uint32_t total_chg_energy;//6019~6020
    uint32_t total_dsg_energy;//6021~6022
	uint16_t TotalCurrent_bias;//6023
	uint16_t revd2;//6024
    uint16_t all_pack_control;//6025
    uint16_t buckle_status;//6026
    uint16_t merge_status;//6027
    uint16_t heat_status;//6028
    uint16_t all_pack_alarm1;//6029
    uint16_t all_pack_alarm2;//6030
    uint16_t BCU_State;//6031
    uint16_t BCU_error_revd;//6032
    uint16_t revd[10];
}MOD_STRUCT_reg06000;
/*
电池保护状态1：
bit0	总压过压保护
bit1	保留
bit2	电池簇充电过流保护1
bit3	电池簇充电过流保护2
bit4	电池簇充电过流保护3
bit5	充电过温保护
bit6	充电低温保护
bit7	单节过压保护
bit8	电池高电荷保护

电池保护状态2：
bit0	总压欠压保护
bit1	保留
bit2	电池簇放电过流保护1
bit3	电池簇放电过流保护2
bit4	电池簇放电过流保护3
bit5	放电过温保护
bit6	放电低温保护
bit7	单节欠压保护*/
typedef union {
    uint32_t all;
    struct {
        uint8_t total_overvoltage : 1; // bit0 总压过压保护
        uint8_t res1_1 : 1; // bit1 保留
        uint8_t charge_overcurrent1 : 1; // bit2 电池簇充电过流保护1
        uint8_t charge_overcurrent2 : 1; // bit3 电池簇充电过流保护2
        uint8_t charge_overcurrent3 : 1; // bit4 电池簇充电过流保护3
        uint8_t charge_overtemp : 1; // bit5 充电过温保护
        uint8_t charge_lowtemp : 1; // bit6 充电低温保护
        uint8_t single_overvoltage : 1; // bit7 单节过压保护 

        uint8_t highcharge : 1; // bit0 电池高电荷保护
        uint8_t res1_2 : 7; // bit1~7 保留

        uint8_t total_undervoltage : 1; // bit0 总压欠压保护
        uint8_t res2_1 : 1; // bit1 保留
        uint8_t discharge_overcurrent1 : 1; // bit2 电池簇放电过流保护1
        uint8_t discharge_overcurrent2 : 1; // bit3 电池簇放电过流保护2
        uint8_t discharge_overcurrent3 : 1; // bit4 电池簇放电过流保护3
        uint8_t discharge_overtemp : 1; // bit5 放电过温保护
        uint8_t discharge_lowtemp : 1; // bit6 放电低温保护
        uint8_t single_undervoltage : 1; // bit7 单节欠压保护 

        uint8_t res2_2 : 8; // bit0~7 保留
    }bit;
} single_pack_protect;

typedef struct {
    /*6100~6110*/
    uint16_t pack_id;//6100
    char    type_ascii[12];//6101~6106
    uint64_t sn_code;//6107~6110
    /*6111~6125*/
    uint16_t total_voltage; // 0.1V  单pack电压 6111
    uint16_t total_current; // 0.1A  单pack电流 6112
    uint16_t  soc;           // 单pack soc       6113
    uint16_t  soh;           // 单pack soh       6114
    uint16_t  avg_temp;      // 平均温度         6115
    uint16_t min_cell_voltage; // 最小电芯电压   6116
    uint16_t max_cell_voltage; // 最大电芯电压       6117
    uint16_t  min_cell_index;    // 最小电芯位置     6118
    uint16_t  max_cell_index;    // 最大电芯位置     6119
    uint16_t  min_temp_value;    // 最小温度值       6120
    uint16_t  max_temp_value;    // 最大温度值      6121
    uint16_t  min_temp_index;    // 最小温度位置    6122
    uint16_t  max_temp_index;    // 最大温度位置    6123
    uint16_t  work_status;       // 工作状态        6124
    uint16_t  chg_status;        // 充电状态        6125
    /*6126*/
    uint16_t revd1[1];
    /*6127*/
    uint16_t pack_online;
    /*6128~6130*/
    uint16_t relay;              // 继电器状态：主继电器Bit0-Bit1、预充继电器Bit2-Bit3、加热继电器Bit4-Bit5.    6128
    uint16_t pack_cap_online;     // pack 容量并机状态                                                        6129
    uint16_t pack_canbus_error;                                                                            // 6130
    /*6131~6143*/
    uint32_t capacity; // 0.001Ah 电池包容量            //6131~6132
    uint32_t total_chg_energy; // 1Wh 电池累计充电能量  //6133~6134
    uint32_t total_dsg_energy; // 1Wh 电池累计放电能量  //6135~6136
    uint32_t once_chg_energy; // 1Wh 电池当前充电能量   //6137~6138
    uint32_t once_dsg_energy; // 1Wh 电池当前放电能量   //6139~6140
    uint16_t sir_value;     // 0.1kR 绝缘包内阻         //6141
    uint16_t inner_value; // 1mR 电池包内阻             //6142
    uint16_t cycle_count; // 循环次数                   //6143
    /*6144~6149*/
    single_pack_protect protect;         // pack 保护              //6144~6145
    uint16_t SysErr[3];           // pack 故障          //6146~6148
    uint16_t alarm1;           // pack 告警             //6149
    /*6150~6153*/
    uint16_t chg_full_time;     // 预计充满时间         //6150
    uint16_t dsg_empty_time;    // 预计放空时间         //6151
    uint16_t pack_total_cell;   // 电芯总数             //6152
    uint16_t  pack_total_ntc;    // NTC总数              //6153  //uint8_t
    /*6154~6156*/
    uint16_t PackBMUCnt;        // Pack中BMU数量，AC380中用作某一类型的电池包的数量    //6154
    uint16_t BMUFaultOrder;     // BMU故障序号  //6155
    uint16_t revd2;
    /*6157~6166*/
    uint16_t pack_type;
    uint16_t pack_outsum_voltage;
    uint16_t pack_control;
	uint16_t allow_max_chg_current;		// 允许最大充电电流
	uint16_t allow_max_dsg_current;		// 允许最大放电电流
	uint16_t pack_mos_status;
    uint16_t pack_single_status;
    uint32_t alarm2;
    uint16_t pack_event;
    /*6167*/
    uint16_t revd3[1];
    /*6168~6171*/
    uint16_t iaging;
    uint16_t revd4[3];
    /*6172~6173*/
    uint16_t pack_ver_diff;
    uint16_t software_total; // 软件版本总数量（相同版本只显示最低版本号）
    /*6174~6203*/
    soft_version1 soft[10]; // 10组软件版本信息
    uint16_t revd[10];
}MOD_STRUCT_reg06100;


// AC380假设 N == 16(最大)
typedef struct {
    uint16_t PackCellCnt;                      //6300          Pack中总的电芯数量
    uint16_t PackNTCCnt;                        //6301          Pack中总的NTC数控
    uint16_t CellVoltage[16];                       //6302~6317     电芯电压1 ~ 电芯电压16
    uint16_t NTCGroup[4];                          //6318~6335     NTC1-NTC2 ~ NTC15-NTC16
}MOD_STRUCT_reg06300;

// #define MOD_STRUCT_reg07000 pack_config_struct//pack config set

typedef struct {
    uint16_t SetPackID;   //7000 用于选择Pack
    uint16_t pack_heat_enable;  //7001  设置Pv加热选项开关
    uint16_t ctr_heat_enable;    //7002 电池高电量电网自动加热
    uint16_t unlock_failed_flags;//7003 设置PACK因故障锁死后解锁
    uint16_t max_parallel_nums;//7004 每套电池包的最大支持数量
}MOD_STRUCT_reg07000;


// AC380假设 N == 18(最大)
// typedef struct {
//     uint64_t BMU_SN[18];                         //7200~7271     7200 ~ 7203+(N-1)*4  BMU SN码
//     uint32_t BMU_Detail[18];                     //7272~7307     7204+(N-1)*4+(N-1)*2 ~ 7205+(N-1)*4+(N-1)*2  BMU 故障和保护等信息
//     uint16_t Single_Cell_and_Temp_Count[18];     //7308~7335     7206+(N-1)*4+(N-1)*2+(N-1)*2 ~ 7207+(N-1)*4+(N-1)*2+(N-1)*2 单个BMU中电芯数量及NTC数量
//     uint8_t BMU_Type[18];                        //7336~7344     7208+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2 ~ 7209+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2  BMU类型 //uint16_t
//     uint32_t BMU_Version[18];                    //7345~7380     7210+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2+(N-1)*2 ~ 7211+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2+(N-1)*2  BMU版本号
// }MOD_STRUCT_reg07200;

typedef union {
    uint16_t all;
    struct {
        uint8_t BMU_Type1 : 8;	
        uint8_t BMU_Type2 : 8;	
    }bit;
}BMU_Type_t;



// AC380假设 N == 18(最大)
typedef struct {
    uint64_t BMU_SN;                         //7200~7271     7200 ~ 7203+(N-1)*4  BMU SN码
    uint32_t BMU_Detail;                     //7272~7307     7204+(N-1)*4+(N-1)*2 ~ 7205+(N-1)*4+(N-1)*2  BMU 故障和保护等信息
    uint16_t Single_Cell_and_Temp_Count;     //7308~7335     7206+(N-1)*4+(N-1)*2+(N-1)*2 ~ 7207+(N-1)*4+(N-1)*2+(N-1)*2 单个BMU中电芯数量及NTC数量
    BMU_Type_t BMU_Type;                      //7336~7344     7208+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2 ~ 7209+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2  BMU类型 //uint16_t
    uint32_t BMU_Version;                    //7345~7380     7210+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2+(N-1)*2 ~ 7211+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2+(N-1)*2  BMU版本号
}MOD_STRUCT_reg07200;



#define MOD_STRUCT_reg13000 inv_wifi_struct// inv第三方wifi

typedef struct {
    uint8_t mesh_id[6];// 13500~13502
    uint16_t revd1[2];      //13503~13504 预留
    uint16_t ready_mesh_set;//13505
    uint16_t sys_sequence;//13506
    uint16_t dev_type;//13507
    uint16_t cmd;//13508
    uint8_t wifi_station_bssid[6];//13509~13511
}MOD_STRUCT_reg13500;

typedef struct {
    ble_protocol_en_t ble_protocol;// 13600
    int16_t blec_rssi_threshold; // 13601 自己作为BLE client查询设备有效 RSSI信号强度切换阈值
    uint16_t blec_switch_interval; // 13602 BLE Client 切换周期间隔（RSSI <X）
    uint8_t bles_adv_key[16];         // 13603~13610 ble server 广播数据加密秘钥, 对外只读, 设置蓝牙密码时更新
    uint16_t wifi_sta_rssi_threshold; // 13611 WiFi STA RSSI阈值
    uint16_t wifi_sta_switch_interval; // 13612 WiFi STA 切换周期间隔（RSSI <X）
    wifi_mul_sta_en_t wifi_mul_sta_en; // 13613 WiFi多连接使能
    uint32_t wifi_sta1_ip;   // 13614~13615 WiFi STA1静态IP地址
    uint32_t wifi_sta1_mask; // 13616~13617 WiFi STA静态子网掩码
    uint32_t wifi_sta1_gw; // 13618~13619 WiFi STA静态网关地址
    uint32_t wifi_sta1_dns1; // 13620~13621 WiFi STA静态DNS2地址
    uint32_t wifi_sta1_dns2; // 13622~13623 WiFi STA静态DNS2地址

    uint16_t wifi_sta2_auth;    // 13624 WiFi STA2认证方式
    char wifi_sta2_ssid[32];    // 13625~13640 WiFi STA2 SSID
    char wifi_sta2_password[64];    // 13641~13672 WiFi STA2密码
    uint32_t wifi_sta2_ip;  // 13673~13674 WiFi STA2静态IP地址
    uint32_t wifi_sta2_mask; // 13675~13676 WiFi STA2静态子网掩码
    uint32_t wifi_sta2_gw; // 13677~13678 WiFi STA2静态网关地址
    uint32_t wifi_sta2_dns1; // 13679~13680 WiFi STA2静态DNS1地址
    uint32_t wifi_sta2_dns2; // 13681~13682 WiFi STA2静态DNS2地址

    uint16_t wifi_sta3_auth;    // 13683 WiFi STA3认证方式
    char wifi_sta3_ssid[32];    // 13684~13699 WiFi STA3 SSID
    char wifi_sta3_password[64];    // 13700~13731 WiFi STA3密码
    uint32_t wifi_sta3_ip; // 13732~13733 WiFi STA3静态IP地址
    uint32_t wifi_sta3_mask; // 13734~13735 WiFi STA3静态子网掩码
    uint32_t wifi_sta3_gw; // 13736~13737 WiFi STA3静态网关地址
    uint32_t wifi_sta3_dns1; // 13738~13739 WiFi STA3静态DNS1地址
    uint32_t wifi_sta3_dns2; // 13740~13741 WiFi STA3静态DNS2地址

    open_mqtt_en_t open_mqtt_enable; // 13742 MQTT上报使能
    uint16_t open_mqtt_report_cycle; // 13743 MQTT上报周期 上报使能打开后有效

    modbus_tcp_en_t modbus_tcp_enable; // 13744 Modbus TCP使能
    uint16_t modbus_tcp_port; // 13745 Modbus TCP端口号
}MOD_STRUCT_reg13600;

typedef struct {
    /*11000~11015*/
    char     hmi_type[12];//11000~11005
    uint64_t hmi_sn;   //11006~11009 sn码
    uint32_t mcu_ver; // 11010~11011软件版本号
    uint32_t flash_ver; // 11012~11013软件版本号
}MOD_STRUCT_reg14000;

typedef struct{
    char d400s_type[12];            // CHARGER 1 型号 15500~15505
    uint16_t d400s_sn[4];              // CHARGER 1 SN  低字节在前 15506~15509
    uint16_t dc1_input_voltage;         //dc1 输入电压 15510
    uint16_t dc3_output_voltage;         //dc3 输出电压 15511
    uint16_t dc3_output_current;         //dc3 输出电流 15512
    uint16_t dc3_input_voltage;         //dc3 输入电压 15513      
    energy_line_t energy_line;           //能量流动条   15514
    uint16_t battery_type;              //输入电池类型 1:12V铅酸电池 2:24V铅酸电池 15515
    uint16_t fault_charger1;                 // charger故障      15516
    uint16_t fault_dcdc;                 // dcdc故障      15517
    uint16_t proctect_dcdc;                 // dcdc保护      15518
    uint16_t revd[7];                 // 预留         15519~15525 
    d400s_work_ctrl ctrl_mode;                      //工作模式15526 
    int32_t total_input_power;          // 所有DC通道总的进入功率 15527~15528 
    int32_t total_output_power;         // 所有DC通道总的输出功率 15529~15530
    d400s_dc_info dc_info[6];           //dc 电压 、电流、功率     15531~15554
    int32_t TotalInputEnergy;           //电量 0.1kwh 15555~15556
    d400s_dc_energy_info energy_info[6];   // 面向双向DC口定义，进出DCDC模块的能量信息 15557~15580
    uint16_t dcdc_SoftwareType;             //0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块 ；11-内置HMI模块；12-内置HMI模块2；13--RF穿墙模块；14-DC-HUB；15-AC-HUB；16-DC-DC；
    uint32_t dcdc_SoftwareVersion;          //软件版本号 例：1001.11； 填充值：100111 15582~15583
    uint16_t revd2[10];                       //预留 15584~15593

}MOD_STRUCT_reg15500;

typedef struct{
    d400s_charger_set_t charger_set;            // 设置 15600
    d400s_dc_volt_current_set dc_val_set[6];              //电流电压设置 15601~15612
    d400s_memory_set memory_val_set;                //15613 dc记忆开关模式设置
    d400s_charge_mode_2_set mode2_set;                //15614 dc充电模式设置1
    d400s_charge_mode_3_set mode3_set;                //15615 dc充电模式设置1
    uint16_t batteryCapacity_L;                 //15616 铅酸电池容量 0.1AH
    uint16_t batteryCapacity_H;                 //15617 铅酸电池容量 0.1AH
    uint16_t battery_Type;                       //15618 电池类型
    int16_t dc_Power_Set[5];                    //15619~15623 流入DC为正，流出为负
    int16_t dc_Total_Power_Set;                //15624
    d400s_charge_mode_4_set mode4_set;           //15625
    uint16_t revd[40];                           //15626~15665

}MOD_STRUCT_reg15600;

typedef union 
{
    /* data */
    uint16_t all;
    struct
    {
        uint16_t matter_sta : 1; //bit0:Matter 0：断开； 1：连接
        uint16_t wifi_mesh_sta : 1; //bit1:WiFi-mesh 0：断开；1：连接
        uint16_t wifi_sta : 1; //bit2:WIFI-sta 0：断开；1：连接
        uint16_t ble_sta : 1; //bit3:ble状态 0：断开；1：连接
        uint16_t mqtt_sta : 1; //bit4:MQTT登录状态 0：断开；1：连接
        uint16_t onoff_sta : 1; //bit5:插座的状态 0：关闭；1：打开
        uint16_t res : 8; //6~13:预留
        uint16_t ac_phase_en : 2;   //14~15:交流相位 0：无效；1/2/3:相位L1/L2/L3
    };
} smart_plug_state_t;

typedef struct {
    char     SmartPlug_Type[12];//14500~14505
    uint64_t SmartPlug_SN;   //14506~14509 sn码
    uint32_t SmartPlug_Software_Version; //14510~14511 软件版本号
    uint16_t SmartPlug_Nums;//14512 智能插座接入数量
    uint32_t SmartPlug_Power;//14513~14514 AC功率,X10w
    uint16_t SmartPlug_Voltage;//14515 AC电压,X100
    uint16_t SmartPlug_Current;//14516 AC电流,X100
    uint16_t SmartPlug_Frequency;//14517 AC频率,X100
    /*bit0:Matter 0：断开； 1：连接
    bit1:WiFi-mesh 0：断开；1：连接
    bit2:WIFI-sta 0：断开；1：连接
    bit3:ble状态 0：断开；1：连接
    bit4:MQTT登录状态 0：断开；1：连接
    bit5:插座的状态 0：关闭；1：打开
    6~15:预留*/
    smart_plug_state_t SmartPlug_States;//14518 状态 
    uint16_t FaultInformation;//14519 故障/保护
    uint16_t WarnInformation;//14520 告警(不影响正常工作)
    uint32_t Day_Consumption_Electricity;//14521~14522 今日使用电量
    uint32_t Day_Use_Time;//14523~14524 今日使用时长
    uint16_t Delay_Set_Start;//14525 动作延时设置时间
	uint32_t Day_feedin_kwh;//	14526 14527今日产生电量 
	uint16_t PF;//14528	功率因数
    
}MOD_STRUCT_reg14500;


typedef struct {
    uint16_t Week_Set;//时间设置标签
    uint16_t Set_Action_Time;//动作时间设置值
}SmartPlug_Time_Set;


/*设置只执行透传，本地不具体分析*/
typedef struct {
    uint16_t Protection_En;//14700 保护使能
    uint16_t Set_Enable[3];//14701~14703 设置使能
    uint16_t Time_Set_Enable[2];//14704~14705 定时时间设置使能
    uint16_t Overload_protection_power_set;//14706 过载保护功率设置值
    uint16_t Underload_protection_power_set;//14707 欠功率保护功率设置值
    uint16_t Indicator_light_regulator;//14708 插座指示灯亮度调节
    uint32_t Time_Set;//14709~14710 定时时间设置
    uint16_t Delay_Set;//14711 动作延时设置目标时间(倒计时)
    SmartPlug_Time_Set Time_Set_Detail[10];//14712~14731
}MOD_STRUCT_reg14700;


typedef struct {
    uint16_t dc_hub_type[6];            // DC_HUB型号           15700~15705
    uint16_t dc_hub_sn[4];              // DC_HUB序列号         15706~15709
    uint16_t input_power;               // 输入功率 1w          15710
    uint16_t input_voltage;             // 输入电压 0.1V        15711
    uint16_t input_current;             // 输入电流 0.01A       15712
    uint16_t output_power;              // 输出总功率 1w        15713
    uint16_t output_voltage;            // 输出电压 0.1V        15714
    uint16_t output_current;            // 输出电流 0.01A       15715
    uint16_t cig1_output_power;         // 点烟器1输出功率 1w       15716
    uint16_t cig1_output_voltage;       // 点烟器1输出电压 0.1V     15717
    uint16_t cig1_output_current;       // 点烟器1输出电流 0.01A    15718
    uint16_t cig2_output_power;         // 点烟器2输出功率 1w       15719
    uint16_t cig2_output_voltage;       // 点烟器2输出电压 0.1V     15720
    uint16_t cig2_output_current;       // 点烟器2输出电流 0.01A    15721
    uint16_t USB_A_output_power;        // USB-A输出功率 1w         15722
    uint16_t USB_A_output_voltage;      // USB-A输出电压 0.1V       15723
    uint16_t USB_A_output_current;      // USB-A输出电流 0.01A      15724
    uint16_t Type_C_output_power;       // Type-C输出功率 1w        15725
    uint16_t Type_C_output_voltage;     // Type-C输出电压 0.1V      15726
    uint16_t Type_C_output_current;     // Type-C输出电流 0.01A     15727
    uint16_t Type_C2_output_power;      // Type-C2输出功率 1w       15728
    uint16_t Type_C2_output_voltage;    // Type-C2输出电压 0.1V     15729
    uint16_t Type_C2_output_current;    // Type-C2输出电流 0.01A    15730
    uint16_t Anderson_output_power;     // 安德森输出功率 1w        15731
    uint16_t Anderson_output_voltage;   // 安德森输出电压 0.1V      15732
    uint16_t Anderson_output_current;   // 安德森输出电流 0.01A     15733
    uint16_t revd1[16];                 // 15734~15749 51
}MOD_STRUCT_reg15700;


typedef union {
    uint16_t all;
    struct {
        uint8_t dc_switch           : 2; // dc开关
        uint8_t dc_saveswitch       : 2; // 记忆开关
        uint8_t dc_reset_factory    : 2; // 恢复出厂设置
        uint8_t revd                : 2; // 预留

        uint8_t dc_set_voltage;          // 电压设置值（bit0/1为关的时候可以设置电压值）
    }bit;
}dc_hub_set_struct;

typedef union {
    uint16_t all;
    struct {
        uint8_t ac_switch           : 2; // ac开关
        uint8_t ac_saveswitch       : 2; // 记忆开关
        uint8_t ac_reset_factory    : 2; // 恢复出厂设置
        uint8_t revd1               : 2; // 预留

        uint8_t revd2;                   // 预留
    }bit;
}ac_hub_set_struct;

typedef struct {
    dc_hub_set_struct dc_hug_setting;   // 15750
    ac_hub_set_struct ac_hug_setting;   // 15751
    uint16_t revd1[10];                 // 15751~15760
}MOD_STRUCT_reg15750;

typedef union{
    uint16_t all;
    struct{
        uint8_t GridRlyFault               :1;  //bit 0 电网继电器故障
        uint8_t LoadRlyFault               :1;  //bit 1 负载继电器故障
        uint8_t GridOverVolt               :1;  //bit 2 电网过压           
        uint8_t GridUnderVolt              :1;  //bit 3 电网欠压          
        uint8_t GridOverFreq               :1;  //bit 4 电网过频
		uint8_t GridUnderFreq              :1;  //bit 5 电网欠频
		uint8_t GridOscillation            :1;  //bit 6 电网振荡
        uint8_t GridPhaseLoss              :1;  //bit 7 电网缺相
        uint8_t OverLoadFault              :1;  //bit 8 负载过载故障
        uint8_t LoadShortFault             :1;  //bit 9 负载短路
        uint8_t InvOverTemp                :1;  //bit 10 PCB过温
        uint8_t FittingWorkError           :1;  //bit 11 配件工作异常，即HUB和AC380电压等级不适配
        uint8_t InvWorkSta                 :1;  //bit 12 0--停止，1--离网运行
        uint8_t GridWorkSta                :1;  //bit 13 0--停止，1--电网充电/AT1接入并离网
        uint8_t res                        :2;
    }bit;
}ac_hub_alarmstate1;
typedef struct {
    char ac_hub_type[12];               // AC_HUB型号           15800~15805
    uint64_t ac_hub_sn;                 // AC_HUB序列号         15806~15809
    uint8_t SoftwareType;               // AC_HUB软件类型       15810
    uint32_t SoftwareVersion;           // AC_HUB软件版本       15811~15812
    uint64_t ACHUB_safe_code;           // AC_HUB安全码         15813~15816
    uint8_t revd[4];                   // 预留                 15817~15818
    uint32_t GridEnergy;                //并机电网能量          15819~15820
    uint32_t AcLoadEnergy;              //并机ac负载能量        15821~15822
    uint16_t InvVoltageL1;              // L1相逆变电压 0.01V   15823
    uint16_t InvVoltageL2;              // L2相逆变电压 0.01V   15824
    uint16_t InvVoltageL3;              // L3相逆变电压 0.01V   15825
    uint16_t Volt_grid_L1;              // L1相电网电压 0.01V   15826
    uint16_t Volt_grid_L2;              // L2相电网电压 0.01V   15827
    uint16_t Volt_grid_L3;              // L3相电网电压 0.01V   15828
    uint16_t Power_load[3];             // L1相负载功率 1w      15829~15831
    // uint16_t Power_load_L2;             // L2相负载功率 1w
    // uint16_t Power_load_L3;             // L3相负载功率 1w
    uint16_t Power_grid[3];             // L1相电网功率 1w      15832~15834
    // uint16_t Power_grid_L2;             // L2相电网功率 1w
    // uint16_t Power_grid_L3;             // L3相电网功率 1w
    uint16_t GridFrequency;             // 电网频率             15835
    uint16_t Temperature;               // 温度                 15836
    ac_hub_alarmstate1 alarm_State;               // 告警状态             15837
    uint16_t revd1[26];                 // 15838~15850
}MOD_STRUCT_reg15800;


typedef union {
    uint16_t all;
    struct {
        uint16_t point_online : 1; //1-active;0-inactive 
        uint16_t update_need : 1; // 升级需求 
        uint16_t alarm : 1; // 告警发生 
        uint16_t protect : 1; // 保护/故障发生 

        uint16_t bit4 : 1; //  
        uint16_t bit5 : 1; //  
        uint16_t bit6 : 1; //  
        uint16_t bit7 : 1; //  

        uint16_t bit8 : 1; //  
        uint16_t bit9 : 1; //  
        uint16_t bit10 : 1; //  
        uint16_t bit11 : 1; //  
        
        uint16_t bit12 : 1; //  
        uint16_t bit13 : 1; //  
        uint16_t bit14 : 1; //  
        uint16_t bit15 : 1; //       		
    }bit;
}POINT_STATE;


typedef struct {

    union 
    {
        uint16_t info[3];
        
        //01
        struct 
        {
            uint8_t slave_addr;//  
            uint8_t group_addr;//  群组序号（不同类型设备，子集汇总信息，从1开始；Modbus 从机地址）;0-表示未绑定
            uint8_t revd;// 
            uint8_t group_same_type_addr;// 同类型设备汇总信息的Modbus从机地址
            
            POINT_STATE state;//  
        };	

        //03
        struct 
        {
            uint8_t isOta;  // 0：未升级，1：升级中，  2：升级成功 3：升级失败
            uint8_t errCode;// 错误码  

            uint32_t soft_version;
        };	

    };	

    union 
    {
        uint16_t DevSn[5];
        struct 
        {
            uint64_t SN_64; 	
            uint16_t Dev_Type;
        };	
    };	
}POINT_BIND_INFO;

typedef struct {
    uint8_t slave_addr;//  
    uint8_t group_addr;//  绑定群组序号（用不同的非零数字表示设备属于不同的群组；下级可只区分，而自己编号；0-表示解绑此SN）
    
    union 
    {
        uint16_t DevSn[5];
        struct 
        {
            uint64_t SN_64; 	
            uint16_t Dev_Type;
        };	
    };
}POINT_BIND_INFO_WR;

typedef union
{
    uint16_t all;
    struct
    {
        uint16_t bind:1;
        uint16_t resv:15;
    } bits;
} meter_scan_state_t;

typedef struct
{
    uint8_t dev_type;
    uint8_t mfg_id;
    uint8_t dev_id[32];
    uint32_t ip;
    uint16_t port;
    meter_scan_state_t state;
    uint16_t revd[5];
} meter_scan_result_t;

typedef union {
    uint16_t all;
    struct
    {
        uint16_t bind:1;
        uint16_t resv:15;
    } bits;
} meter_bind_set_t;

typedef struct
{
    uint8_t dev_type;
    uint8_t mfg_id;
    uint8_t dev_id[32];
    uint32_t ip;
    uint16_t port;
    uint16_t func;
    meter_bind_set_t set;
    uint16_t revd[4];
} meter_bind_info_t;

typedef struct {
    uint16_t err_code;
    uint8_t dev_type;
    uint8_t mfg_id;
    uint8_t dev_id[32];
    uint32_t ip;
    uint16_t port;
    uint16_t revd[5];
} meter_bind_result_t;

typedef union {
    uint16_t all;
    struct
    {
        uint16_t is_connected:1;
        uint16_t resv:15;
    } bits;
} meter_state_t;

typedef struct {
    uint8_t dev_type;
    uint8_t mfg_id;
    uint8_t dev_id[32];
    uint32_t ip;
    uint16_t port;
    uint8_t slave_addr;
    uint8_t resv1;
    meter_state_t state;
    uint16_t revd2[3];
} meter_dev_info_t;

typedef struct {
    uint16_t ver;// 21000 蓝牙版本：1；uart版本：2；
    uint16_t bias;// 21001 设备数量偏移;第一帧为0
    //uint32_t point_online;// 21002
    union 
    {
        struct
        {
            POINT_BIND_INFO self_bind_info;
            POINT_BIND_INFO point_bind_info[POINT_SLAVE_MAX_NUM];//21002,单贞最大32个节点 
        };
        meter_scan_result_t meter_scan_result[METER_SCAN_MAX_NUM];      // 协议版本为6
        meter_bind_result_t meter_bind_result[METER_MAX_NUM];           // 协议版本为7
        meter_dev_info_t meter_dev_state[METER_MAX_NUM];                // 协议版本为8
    };
}MOD_STRUCT_reg21000;

typedef struct {
    uint16_t ver;// 21000 蓝牙版本：1；uart版本：2；
    uint16_t bias;// 21001 设备数量偏移;第一帧为0
    union {
        POINT_BIND_INFO_WR point_bind_info[POINT_SLAVE_MAX_NUM];//21002,单贞最大32个节点
        meter_bind_info_t meter_bind_info[METER_MAX_NUM];
    };
}MOD_STRUCT_reg21000_WR;

typedef struct {
	uint16_t Net_Public_Protocol_version;	//22000 服务器交互协议版本 
	uint16_t revd[3];		//4个预留
	g_st_IOT_ENABLE iot_enable;

	
	char Net_Server_address[128];	//22005 服务器网址 
	char Net_Server_secret[64];	//22069 登录服务器密码	

}MOD_STRUCT_reg22000;


typedef struct {
    uint16_t into_factory; // 29700 1-生产标定，自己做从机
    // 设备型号
    char dev_type[12];  // 29701~29706
    // IOT型号
    char iot_type[12];  // 29707~29712
    uint16_t iot_sn[4]; // 29713~29716
    uint16_t iot_safetyCode[4]; // 29717~29720
    uint16_t revd1;
    uint32_t revd2; 
    uint32_t revd3;
    uint32_t revd4;
    uint32_t revd5;
    uint32_t revd6;
}MOD_STRUCT_reg29700;

typedef struct {
    uint16_t revd1;//30900
    uint16_t test_work_mode; //30901 测试模式切换 0x55:进入正常模式 0xA0:进入单板测试模式
    uint16_t test_net_mode; //30902 网络模式切换 0：WIFI模式（默认） 1：以太网模式 2：4g模式
    char test_ip[16];//30903~30910 当前模式ip地址
    uint16_t test_reset;// 30911 恢复出厂设置标志 0：无 1：已完成，待断电
    uint32_t revd6;
}MOD_STRUCT_reg30900;

#if 1//def MASS_PRODUCTION_CONFIG_ENABLE
typedef struct {
    uint16_t  param_index;
	
    uint16_t  param[511];

}MOD_STRUCT_reg40000;
#else
typedef union{
	uint16_t all;
	struct{
		uint8_t can_para_control: 2;
		uint8_t lcd_red: 2;
		uint8_t lcd_yellow: 2;
		uint8_t lcd_green: 2;
		uint8_t power_from_bat: 2;
		uint8_t IP_test: 4;// 1- 2- 3- 4-
	}bit;
}param_control_struct;

typedef struct {
    param_control_struct  param_control;

	char dc_dev_type[12];   // DCDC设备型号
    uint64_t dc_dev_sn;     // DCDC设备SN
    uint16_t CANID;
	uint16_t IOT_fault; // IOT产生故障
	
//    uint16_t  param[511];

}MOD_STRUCT_reg40000;
#endif

// 设备调试状态结构体（按功能模块分组，便于协议打包/解析），借用reg40000段，40000=300索引，表示IOT 北向诊断心跳变量
typedef struct {
    /************************ 基础通信状态 ************************/
    int8_t  wifi_signal_strength;          // WIFI信号强度（UINT8）
    int8_t  ble_server_signal_strength;          //自己作为BLE client去访问BLE server从设备的信号强度
    
    uint32_t device_rtc_time;               // 设备RTC时间（UINT32秒计数器）
    uint32_t device_heartbeat_counter;      // 设备心跳计数器（UINT32秒计数器）
    uint32_t last_login_cloud_time;         // 最近一次登录云服务器时间（UINT32）
    uint32_t last_disconnect_cloud_time;    // 最近一次断开云服务器时间（UINT32）
    
    /************************ 设备在线数量 ************************/
    uint16_t  iot_online_num;                // IOT在线数量（UINT16）
    uint16_t  inverter_online_num;           // 逆变器在线数量（UINT16）
    uint16_t  pack_online_num;               // PACK在线数量（UINT16）
    uint16_t  wifi_local_device_num;         // WIFI局域网内我方设备节点数量（UINT16）
    
    /************************ CAN总线统计 ************************/
    uint16_t  can_inv_rx_counter;            // CAN总线INV设备报文RX计数器（UINT16）
    uint16_t  can_pack_rx_counter;           // CAN总线PACK设备报文RX计数器（UINT16）
    uint16_t  can_iot_rx_counter;            // CAN总线IOT设备报文RX计数器（UINT16）
    uint16_t  can_x_rx_counter;              // CAN总线X设备报文RX计数器（UINT16）
    
    /************************ 内存与网络统计 ************************/
    uint16_t iot_remaining_ram_in_kb;        // IOT剩余内置RAM（KB，UINT16）单位1KB，最大显示65535
    uint16_t iot_remaining_ram_frag_pct;     // IOT内置RAM碎片率（UINT16）
    uint16_t iot_remaining_ram_ext_10kb;     // IOT剩余外置RAM（KB，UINT16）单位10KB，最大显示65535
    uint16_t  net_frame_tx_counter;          // 网络报文帧TX计数器（MQTT/HTTP等，UINT16）
    uint16_t  net_frame_rx_counter;          // 网络报文帧RX计数器（MQTT/HTTP等，UINT16）
    uint16_t  ble_frame_tx_counter;          // BLE报文帧TX计数器（UINT16）
    uint16_t  ble_frame_rx_counter;          // BLE报文帧RX计数器（UINT16）
    
    /************************ 设备连接时间戳 ************************/
    uint32_t iot_build_time;                // IOT固件编译时间（UINT32）
    uint32_t last_login_router_time;        // 最后成功登录路由器时间（UINT32）
    uint32_t last_disconnect_router_time;   // 最后断开路由器时间（UINT32）
    uint32_t last_ble_client_connect_time;  // 最后成功连接BLE Client(手机)时间（UINT32）
    uint32_t last_ble_client_disconn_time;  // 最后断开BLE Client(手机)时间（UINT32）
    uint32_t last_ble_server_connect_time;  // 最后成功连接BLE Server(储能设备)时间（UINT32）
    uint32_t last_ble_server_disconn_time;  // 最后断开BLE Server(储能设备)时间（UINT32）
    
    /************************ RTC时间同步 ************************/
    uint32_t last_rtc_sync_ble;             // 最后一次RTC同步时间（来自BLE，UINT32）
    uint32_t last_rtc_sync_cloud;           // 最后一次RTC同步时间（来自CLOUD，UINT32）
    uint32_t last_rtc_sync_local;           // 最后一次RTC同步时间（来自LOCAL，UINT32）

    /************************ 线程最长运行时间（单位：us）************************/
    uint32_t thread1_max_runtime_us;   // 线程1最长运行时间（us）
    uint32_t thread2_max_runtime_us;   // 线程2最长运行时间（us）
    uint32_t thread3_max_runtime_us;   // 线程3最长运行时间（us）
    uint32_t thread4_max_runtime_us;   // 线程4最长运行时间（us）
    uint32_t thread5_max_runtime_us;   // 线程5最长运行时间（us）
    uint32_t thread6_max_runtime_us;   // 线程6最长运行时间（us）
} MOD_STRUCT_reg40000_IotDebugStatus_t;

typedef struct {
    uint16_t  param_index;

    union 
    { 
        uint16_t  param[511];
        MOD_STRUCT_reg40000_IotDebugStatus_t IoT_Debug_Data;
    };
}MOD_STRUCT_reg40000_wlcc;

typedef struct {
    MOD_STRUCT_reg40000_wlcc mod_reg40000_IOT_Debug;
}MOD_STRUCT_IOTSELF_wlcc;// modbus beta总变量结构体,read 

extern MOD_STRUCT_IOTSELF_wlcc  g_self_data_wlcc;
typedef struct {
//windy add
	MOD_STRUCT_reg00000 mod_reg00000; //  6~16,
    MOD_STRUCT_reg00700 mod_reg00700_OTA; //700~768  OTA设置区
	
    MOD_STRUCT_reg01100 mod_reg01100_Inv_base; //1100~1154  每台逆变器基本信息; 借用slave 0报 IOT所有 SWver
    MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067  基础设置项
    MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区

    MOD_STRUCT_reg03000 mod_reg03000_Inv_history; //3000~3026  每台逆变器历史记录区
    MOD_STRUCT_reg03500 mod_reg03500_Inv_yearX_statistic; //3500~3547    每台逆变器累计年电量区
    MOD_STRUCT_reg03600 mod_reg03600_Inv_year1_statistic; //3600~3658  每台逆变器当年电量区
	
	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
	MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区

    MOD_STRUCT_reg13500 mod_reg13500_mesh; //13500~13511   WIFI MESH区
    MOD_STRUCT_reg13600 mod_reg13600_open; //13600~13681 开放平台设置区

    MOD_STRUCT_reg15500 mod_reg15500_D400s_info;//15500~15583 CHARGER 1 信息区
    MOD_STRUCT_reg15600 mod_reg15600_D400s_set;//15600~15612 CHARGER 1 设置区
    
	MOD_STRUCT_reg21000	mod_reg21000_bind;//绑定帧的读写不一样
	MOD_STRUCT_reg21000_WR	mod_reg21000_bind_WR;
	
	MOD_STRUCT_reg22000 mod_reg22000_net_server_2rd;
    
    MOD_STRUCT_reg30900 mod_reg30900_test;//生产测试
}SELF_DATA_STRUCT;
extern SELF_DATA_STRUCT  g_self_data;
//extern EXT_RAM_BSS_ATTR SELF_DATA_STRUCT  g_self_data_WR;


//windy:基于Modbus beta寄存器创建结构体
typedef struct {
//    MOD_STRUCT_reg00000 mod_reg00000; //  6~16,
    MOD_STRUCT_reg00100 mod_reg00100_AppPage1; //100~168  APP首页数据（汇总）
    MOD_STRUCT_reg00700 mod_reg00700_OTA; //700~768  OTA设置区
    MOD_STRUCT_reg01100 mod_reg01100_Inv_base; //1100~1154  每台逆变器基本信息
    MOD_STRUCT_reg01200 mod_reg01200_Inv_pv; //1200~1289  每台逆变器PV信息
    MOD_STRUCT_reg01300 mod_reg01300_Inv_grid; //1300~1330 每台逆变器电网信息 
    MOD_STRUCT_reg01400 mod_reg01400_Inv_load; //1400~1447  每台逆变器负载信息
    MOD_STRUCT_reg01500 mod_reg01500_Inv_inv; //1500~1529  每台逆变器逆变信息
    MOD_STRUCT_reg01600 mod_reg01600_Inv_generator; //1600~1613 每台逆变器发电机信息
    MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067  基础设置项
    MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区
    MOD_STRUCT_reg02300 mod_reg02300_Inv_set02_struct; //7000~7004  pack设置区
    MOD_STRUCT_reg02400 mod_reg02400_Inv_certification; //2400~2449  认证设置项
    MOD_STRUCT_reg02500 mod_reg02500_Inv_advance_set2; //2500~2544  每台逆变高级设置区2（面向微逆）
    MOD_STRUCT_reg03000 mod_reg03000_Inv_history; //3000~3026  每台逆变器历史记录区
    MOD_STRUCT_reg03500 mod_reg03500_Inv_yearX_statistic; //3500~3547    每台逆变器累计年电量区
    MOD_STRUCT_reg03600 mod_reg03600_Inv_year1_statistic; //3600~3658  每台逆变器当年电量区
    MOD_STRUCT_reg04000 mod_reg04000_Dsp_data;//4000~4049 dsp数据区
    MOD_STRUCT_reg04050 mod_reg04050_Dsp_set1;//4050~4104 dsp设置区1
    MOD_STRUCT_reg04105 mod_reg04105_Dsp_set2;//4105~4177 dsp设置区2
    MOD_STRUCT_reg06000 mod_reg06000_Pack_sum; //6000~6030    pack主包汇总信息
    MOD_STRUCT_reg06100 mod_reg06100_Pack_each; //6100~6203  pack单包信息
    MOD_STRUCT_reg07000 mod_reg07000_Pack_set; //7000~7004  pack设置区	
    MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040    IOT信息区（只读，面向客户）
//    MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
    MOD_STRUCT_reg13000 mod_reg13000_3rd_WIFI; //第3方WiFi接入信息管理
//    MOD_STRUCT_reg13500 mod_reg13500_mesh; //13500~13511   WIFI MESH区
//    MOD_STRUCT_reg14000 mod_reg14000_HMI_info;////14000~14013    HMI信息区    
    MOD_STRUCT_reg14500 mod_reg14500_SmartPlug_info;//14500~14525 智能插座只读区（周期上报）
    MOD_STRUCT_reg14700 mod_reg14700_SmartPlug_set;//14700~14731 智能插座设置区
//	MOD_STRUCT_reg21000	mod_reg21000_bind;//tbd not use,move
//	MOD_STRUCT_reg22000	mod_reg22000_net_server_2rd;
    MOD_STRUCT_reg29700 mod_reg29700_IOT_info;//29700~29720 IOT标定区（从模式写入）
//    MOD_STRUCT_reg00000 mod_reg00000; //  
 //     
}MOD_STRUCT_MicroInv;//微逆设备modbus beta总变量结构体

//#define MAX_NUM_MICRO_INV	(NET_SUB1G_MAX_POINT + NET_WIFI_MAX_POINT + 1)

//extern EXT_RAM_BSS_ATTR MOD_STRUCT_MicroInv	MicroInv[MAX_NUM_MICRO_INV];//微逆设备modbus beta总变量结构体
//extern EXT_RAM_BSS_ATTR MOD_STRUCT_MicroInv	MicroInv_WR;//modbus写缓存 [MAX_NUM_MICRO_INV]

#pragma pack()


#define	MOD_REG_START_ADDR_00000		(uint16_t)0//
#define	MOD_REG_LEN_00000				((uint16_t)sizeof(MOD_STRUCT_reg00000)>>1)//
#define	MOD_REG_START_ADDR_00100		(uint16_t)100//
#define	MOD_REG_LEN_00100				((uint16_t)sizeof(MOD_STRUCT_reg00100)>>1)//
#define	MOD_REG_START_ADDR_00700		(uint16_t)700//
#define	MOD_REG_LEN_00700				((uint16_t)sizeof(MOD_STRUCT_reg00700)>>1)//
#define	MOD_REG_START_ADDR_01100		(uint16_t)1100//
#define	MOD_REG_LEN_01100				((uint16_t)sizeof(MOD_STRUCT_reg01100)>>1)//
#define	MOD_REG_START_ADDR_01200		(uint16_t)1200//
#define	MOD_REG_LEN_01200				((uint16_t)sizeof(MOD_STRUCT_reg01200)>>1)//
#define	MOD_REG_START_ADDR_01300		(uint16_t)1300//
#define	MOD_REG_LEN_01300				((uint16_t)sizeof(MOD_STRUCT_reg01300)>>1)//
#define	MOD_REG_START_ADDR_01400		(uint16_t)1400//
#define	MOD_REG_LEN_01400				((uint16_t)sizeof(MOD_STRUCT_reg01400)>>1)//
#define	MOD_REG_START_ADDR_01500		(uint16_t)1500//
#define	MOD_REG_LEN_01500				((uint16_t)sizeof(MOD_STRUCT_reg01500)>>1)//
#define	MOD_REG_START_ADDR_01600		(uint16_t)1600//
#define	MOD_REG_LEN_01600				((uint16_t)sizeof(MOD_STRUCT_reg01600)>>1)//
#define	MOD_REG_START_ADDR_01700		(uint16_t)1700//
#define	MOD_REG_LEN_01700				((uint16_t)sizeof(MOD_STRUCT_reg01700)>>1)//
#define	MOD_REG_START_ADDR_01900		(uint16_t)1900//
#define	MOD_REG_LEN_01900				((uint16_t)sizeof(MOD_STRUCT_reg01900)>>1)//
#define	MOD_REG_START_ADDR_02000		(uint16_t)2000//
#define	MOD_REG_LEN_02000				((uint16_t)sizeof(MOD_STRUCT_reg02000)>>1)//
#define	MOD_REG_START_ADDR_02200		(uint16_t)2200//
#define	MOD_REG_LEN_02200				((uint16_t)sizeof(MOD_STRUCT_reg02200)>>1)//
#define	MOD_REG_START_ADDR_02300		(uint16_t)2300//
#define	MOD_REG_LEN_02300				((uint16_t)sizeof(MOD_STRUCT_reg02300)>>1)//
#define	MOD_REG_START_ADDR_02400		(uint16_t)2400//
#define	MOD_REG_LEN_02400				((uint16_t)sizeof(MOD_STRUCT_reg02400)>>1)//
#define	MOD_REG_START_ADDR_02500		(uint16_t)2500//
#define	MOD_REG_LEN_02500				((uint16_t)sizeof(MOD_STRUCT_reg02500)>>1)//
#define	MOD_REG_START_ADDR_03000		(uint16_t)3000//
#define	MOD_REG_LEN_03000				((uint16_t)sizeof(MOD_STRUCT_reg03000)>>1)//
#define	MOD_REG_START_ADDR_03500		(uint16_t)3500//
#define	MOD_REG_LEN_03500				((uint16_t)sizeof(MOD_STRUCT_reg03500)>>1)//
#define	MOD_REG_START_ADDR_03600		(uint16_t)3600//
#define	MOD_REG_LEN_03600				((uint16_t)sizeof(MOD_STRUCT_reg03600)>>1)//
#define	MOD_REG_START_ADDR_04000		(uint16_t)4000//
#define	MOD_REG_LEN_04000				((uint16_t)sizeof(MOD_STRUCT_reg04000)>>1)//
#define	MOD_REG_START_ADDR_04050		(uint16_t)4050//
#define	MOD_REG_LEN_04050				((uint16_t)sizeof(MOD_STRUCT_reg04050)>>1)//
#define	MOD_REG_START_ADDR_04105		(uint16_t)4105//
#define	MOD_REG_LEN_04105				((uint16_t)sizeof(MOD_STRUCT_reg04105)>>1)//
#define	MOD_REG_START_ADDR_06000		(uint16_t)6000//
#define	MOD_REG_LEN_06000				((uint16_t)sizeof(MOD_STRUCT_reg06000)>>1)//
#define	MOD_REG_START_ADDR_06100		(uint16_t)6100//
#define	MOD_REG_LEN_06100				((uint16_t)sizeof(MOD_STRUCT_reg06100)>>1)//
#define	MOD_REG_START_ADDR_06300		(uint16_t)6300//
#define	MOD_REG_LEN_06300				((uint16_t)sizeof(MOD_STRUCT_reg06300)>>1)//
#define	MOD_REG_START_ADDR_07000		(uint16_t)7000//
#define	MOD_REG_LEN_07000				((uint16_t)sizeof(MOD_STRUCT_reg07000)>>1)//
#define	MOD_REG_START_ADDR_07200		(uint16_t)7200//
#define	MOD_REG_LEN_07200				((uint16_t)sizeof(MOD_STRUCT_reg07200)>>1)//
#define	MOD_REG_START_ADDR_11000		(uint16_t)11000//
#define	MOD_REG_LEN_11000				((uint16_t)sizeof(MOD_STRUCT_reg11000)>>1)//
#define	MOD_REG_START_ADDR_12000		(uint16_t)12000//
#define	MOD_REG_LEN_12000				((uint16_t)sizeof(MOD_STRUCT_reg12000)>>1)//
#define	MOD_REG_START_ADDR_13000		(uint16_t)13000//
#define	MOD_REG_LEN_13000				((uint16_t)sizeof(MOD_STRUCT_reg13000)>>1)//
#define	MOD_REG_START_ADDR_13500		(uint16_t)13500//
#define	MOD_REG_LEN_13500				((uint16_t)sizeof(MOD_STRUCT_reg13500)>>1)//	
#define	MOD_REG_START_ADDR_13600		(uint16_t)13600//
#define	MOD_REG_LEN_13600				((uint16_t)sizeof(MOD_STRUCT_reg13600)>>1)//	
#define	MOD_REG_START_ADDR_14000		(uint16_t)14000//
#define	MOD_REG_LEN_14000				(sizeof(MOD_STRUCT_reg14000)>>1)//	
#define	MOD_REG_START_ADDR_14500		(uint16_t)14500//
#define	MOD_REG_LEN_14500				((uint16_t)sizeof(MOD_STRUCT_reg14500)>>1)//
#define	MOD_REG_START_ADDR_14700		(uint16_t)14700//
#define	MOD_REG_LEN_14700				((uint16_t)sizeof(MOD_STRUCT_reg14700)>>1)//
#define	MOD_REG_START_ADDR_15500		(uint16_t)15500//
#define	MOD_REG_LEN_15500				((uint16_t)sizeof(MOD_STRUCT_reg15500)>>1)//
#define	MOD_REG_START_ADDR_15600		(uint16_t)15600//
#define	MOD_REG_LEN_15600				((uint16_t)sizeof(MOD_STRUCT_reg15600)>>1)//
#define	MOD_REG_START_ADDR_15700		(uint16_t)15700//
#define	MOD_REG_LEN_15700				((uint16_t)sizeof(MOD_STRUCT_reg15700)>>1)//
#define	MOD_REG_START_ADDR_15750		(uint16_t)15750//
#define	MOD_REG_LEN_15750				((uint16_t)sizeof(MOD_STRUCT_reg15750)>>1)//
#define	MOD_REG_START_ADDR_15800		(uint16_t)15800//
#define	MOD_REG_LEN_15800				((uint16_t)sizeof(MOD_STRUCT_reg15800)>>1)//
#define	MOD_REG_START_ADDR_19000		(uint16_t)19000//
#define	MOD_REG_LEN_19000				((uint16_t)sizeof(MOD_STRUCT_reg19000)>>1)//
#define	MOD_REG_START_ADDR_26000		(uint16_t)SMART_TOU_REG_START_ADDR//
#define	MOD_REG_LEN_26000				((uint16_t)SMART_TOU_REG_MAX_LEN)//
#define	MOD_REG_START_ADDR_21000		(uint16_t)21000//
#define	MOD_REG_LEN_21000				((uint16_t)sizeof(MOD_STRUCT_reg21000)>>1)//	
#define	MOD_REG_START_ADDR_22000		(uint16_t)22000//
#define	MOD_REG_LEN_22000				((uint16_t)sizeof(MOD_STRUCT_reg22000)>>1)//	
#define	MOD_REG_START_ADDR_29700		(uint16_t)29700//
#define	MOD_REG_LEN_29700				((uint16_t)sizeof(MOD_STRUCT_reg29700)>>1)//	

#define	MOD_REG_START_ADDR_40000		(uint16_t)40000//
#define	MOD_REG_LEN_40000				((uint16_t)sizeof(MOD_STRUCT_reg40000)>>1)//	

extern QueueHandle_t position_queue; // MODBUS转到DTU UART的消息队列

uint64_t convertToUint64(uint16_t *i) ;
void Uart_Tx_To_AT403(void);
uint16_t Mesh_Modbus_MasterTxCmd(uint16_t regAddress, uint8_t regNum, uint8_t *outbuf, uint8_t slave_address,uint8_t frametype, uint16_t *input_regdata);
uint8_t Modbus_MasterRespones_RS485(UART_STRUCT *struct_uart, uint8_t *cmdBuf, uint16_t cmdLen);
uint8_t Modbus_SlaveRespones_RS485(UART_STRUCT *struct_uart, uint8_t *cmdBuf, uint16_t cmdLen);
uint8_t Modbus_MasterRespones_Sub1GHz(UART_STRUCT *struct_uart, uint8_t *income, uint16_t cmdLen);
uint8_t Modbus_MasterRespones_AT403(UART_STRUCT *struct_uart, uint8_t *cmdBuf, uint16_t cmdLen);
int8_t set_position_task(void); 
void sys_new_position_and_transmit(reg_position_list_t *position_list); 
int UART_Tx_Cmd_Sub1GHz(UART_STRUCT *struct_uart);
void Uart_Tx_To_RS485_ext_meter(void);
void Uart_Tx_To_Sub1GHz_Period_module(void);
void Uart_Tx_To_Sub1GHz_DRMOs(void);
void Uart_Tx_To_Sub1GHz_RTC(void);
void Uart_Tx_To_Sub1GHz_NET_ID(void);
uint16_t Modbus_MasterReadCmd_03H(uint16_t regAddress, uint8_t regNum, uint8_t *outbuf, uint8_t slave_address);
// uint16_t Modbus_MasterWriteCmd_06H_10H(uint16_t regAddress, uint8_t regNum, const uint16_t *data, uint8_t *outbuf, uint8_t slave_address);
uint16_t Modbus_MasterWriteCmd_06H_10H(uint16_t regAddress, uint8_t regNum, bool is_write, uint8_t *outbuf, uint8_t slave_address);

void serial_recv_callback(UART_STRUCT *uart_struct);
void fcty_cal_app(void);
#endif


