#ifndef INV_TYPE_H
#define INV_TYPE_H

#include <stdint.h>
#include "can_type.h"

#define INV_CAN_ADDR        0x10
#define INV_MAX_NUM         1//windy 单逆变器系统内部的逆变器数量
#define INV_GROUP_MAX_NUM   INV_MAX_NUM	//一个逆变器对应的电池包群组的包数量

#define PLUG_MAX_NUM         NET_WIFI_S1_POINT//16//


#define INV_ANNOUNCE1        0x10//can beta old CAN ID funcode单帧
#define INV_ANNOUNCE2        0x11
#define INV_ANNOUNCE3        0x12
#define INV_ANNOUNCE4        0x13
#define INV_ANNOUNCE5        0x14
#define INV_ANNOUNCE6        0x15
#define INV_ANNOUNCE7        0x01//2023.5新增 CAN ID单帧
#define INV_ANNOUNCE8        0x02
#define INV_ANNOUNCE9        0x03
#define INV_ANNOUNCEA        0x09 //逆变基于故障变化触发式上报
#define INV_ANNOUNCE_0xFA        250//综合控制帧=0xFA

enum {
    INV_TYPE_ANNOUNCE_10H=0x10,
    INV_TYPE_BASE_11H = 0x11,
    INV_TYPE_LINE_12H = 0x12,
    INV_TYPE_ABOUT_13H = 0x13 ,
    INV_TYPE_PV_14H = 0x14 ,
    INV_TYPE_GRID_15H = 0x15 ,
    INV_TYPE_LOAD_16H = 0x16 ,
    INV_TYPE_DATA_17H = 0x17 ,
    INV_TYPE_METER_18H = 0x18 ,
    INV_TYPE_GEN_19H = 0x19,
    INV_TYPE_CONFIG00_1AH = 0x1A,//modbus 每台逆变备份独立-设置区 2000 - 2088
    INV_TYPE_CONFIG01_1BH = 0x1B,//每台逆变备份独立-高级设置区 2200 - 2217
    INV_TYPE_CONFIG02_1CH = 0x1C,//每台逆变备份独立-认证设置区 2300 - 2319
    INV_TYPE_CONFIG03_1DH = 0x1D,//每台逆变独立-认证设置区 2400 - 2449
    INV_TYPE_LOG_20H = 0x20,//每台逆变器历史记录区,3000~
    INV_TYPE_ALL_ENERGY_21H = 0x21,//每台逆变器累计年电量区,3500~
    INV_TYPE_CURR_ENERGY_22H = 0x22,//每台逆变器当年电量区,3600~
    INV_TYPE_WIFI_23H = 0x23,// 三方wifi 13000 -  
	INV_TYPE_WIFI_REPORT_24H = 0x24,
	INV_TYPE_WIFI_PARAM_25H = 0x25,
    INV_TYPE_CERT_27H = 0x27,//modbus 40000透传
    INV_TYPE_DCDC_48H = 0x48,//DCDC模块通用/CHARGER 1 信息区
    INV_TYPE_DCDC_49H = 0x49,//DCDC模块通用/CHARGER 1 设置区
};


#pragma pack(1)
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
        uint8_t ac_eco_on : 1;  // bit11 ac eco 开关
        uint8_t remote_off_on : 1;  // bit12 系统关机状态（IOT主动断开MQTT）
        uint8_t remote_sleep_on : 1;  // bit13 休眠状态（远程关机功能）
    }bit;
}device_ctrl;


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
}ctrl_event;




////////////////////////////////////////inv////////////////////////////////////////////////
////////////////////////////////////////inv////////////////////////////////////////////////
////////////////////////////////////////inv////////////////////////////////////////////////
typedef union {
    uint8_t status;
    struct {
        uint8_t have_grid       : 1; // 有电网
        uint8_t have_pv         : 1; // 有PV
        uint8_t have_dc_load    : 1; // 有DC负载
        uint8_t have_ac_load    : 1; // 有AC负载
    }bit;
}inv_status;

typedef union {
    uint8_t flag;
    struct {
        uint8_t bat_aging_en    : 1; // 电池老化使能
        uint8_t ats_fire_sig 	: 1; // ATS点火信号
    }bit;
}inv_flag2_t;

typedef  struct {
    uint8_t year;   // 年 - 2000偏移
    uint8_t month;  // 月
    uint8_t day;    // 日
    uint8_t hour;   // 时
    uint8_t minute; // 分
    uint8_t second; // 秒
}inv_date;



/**
 * @brief Type = 0x10
1				EP600
2				EP900
3				ES203
4				ES303
5				PR004
*/
typedef struct {
    inv_status  status_flags;   	// 逆变状态标记
	inv_flag2_t inv_flag2;			// 逆变标志2
	uint8_t 	stdby_pwr_coms;		// 待机功耗
    uint8_t     reserve[3];     	// 保留
    uint16_t    type;           	// 逆变器类型

    inv_date    date;           	// 逆变器日历，INV主动上报，实时数据，周期2s-陆文杰
    uint16_t    online;          	// 逆变并机数量,逆变通讯在线状态 按位表示

    uint16_t    alarm[4];       	// 告警信息

    uint16_t    fault[4];       	// 故障信息

    uint32_t    total_chg_power; 	// 逆变总的充电功率
    uint32_t    total_dsg_power; 	// 逆变总的放点功率

    uint16_t    fault5;       		// 告警信息5
    uint8_t     res1[6];      		// 保留

	/* 以下数据需要发送指令主动开启逆变器相关设置,逆变器才会主动上报 */
	uint16_t 	l1_chg_limit;		// 三相充电限制功率
	uint16_t 	l2_chg_limit;
	uint16_t	l3_chg_limit;
	uint16_t 	overall_status;		// 综合状态

	uint16_t 	l1_dsg_limit;		// 三相放电限制功率
	uint16_t 	l2_dsg_limit;
	uint16_t	l3_dsg_limit;
	uint16_t	res2;

	int16_t	l1_apparent_power;	// 三相视在功率 M139.3 uint16_t
	int16_t	l2_apparent_power;
	int16_t	l3_apparent_power;
	int16_t	res3;

    rw_cmd_struct   rw_cmd;
}inv_announce_struct;

/**
 * @brief Type = 0x11
 *
 */
typedef struct {
    uint8_t  inv_num;
    uint16_t inv_online;
    uint8_t  inv_power_rang; // 1: 2 : 3:    (小功率，中功率，大功率)
    uint16_t line_event;  // 00:能量线无效；01:能量线有效；PV能量线可接入逆变
    uint8_t  inv_work_state; // 逆变工作状态
    device_ctrl ctrl_status; //
    uint16_t alarm[4]; // 告警信息
    uint16_t fault[4]; // 故障信息
    uint32_t DCLoadAllTotalPower; //
    uint32_t ACLoadAllTotalPower; //
    uint32_t PVAllTotalPower; //
    uint32_t GridAllTotalPower; //
    uint32_t InvAllTotalPower; //
    uint32_t DCLoadTotalEnergy; //
    uint32_t ACLoadTotalEnergy; //
    uint32_t PvTotalChargingEnergy; //
    uint32_t GridTotalChargingEnergy;   //
    uint32_t FeedbackEnergy;    //
    uint16_t fault5;        // 故障5
    uint32_t PvToACLoadEnergy;
	uint8_t SelfConsumptionPercent;
	uint32_t PVToACloadPower;
    uint16_t grid_par_soc;		// 电网并机SOC
    uint32_t CarTotalEnergy;
    uint32_t SleepPredictTime;
    uint16_t CarTotalPower;
    uint32_t PackTotalChargEnergy;
    uint32_t PackTotalDisChargEnergy;
    uint16_t INV_TEST;
    uint16_t switch_memory_state;
    uint16_t FaultInformation6;
    rw_cmd_struct   rw_cmd;
}inv_base_struct;


/**
 * @brief Type = 0x12
 *
 */
typedef struct {
    uint16_t line_event;  // 00:能量线无效；01:能量线有效；PV能量线可接入逆变
    rw_cmd_struct   rw_cmd;
}inv_line_struct;

/**
 * @brief Type = 0x13
 *
 */
typedef struct {
    char  dev_type[12];   // ASCII 机型
    uint64_t dev_sn;      // 设备唯一识别码
    uint8_t voltage_lable; // 0-di低压；1-高压
    uint8_t time_area_num;   // 时间段管理数量
    uint8_t  software_total; // 系统中软件版本数量,相同的MCU只显示最低版本
    soft_version  soft[6];  // 最多6个软件版本
    uint16_t inv_chg_limit_L1;//逆变器L1交流充电限值
    uint16_t inv_chg_limit_L2;//逆变器L2交流充电限值
    uint16_t inv_chg_limit_L3;//逆变器L3交流充电限值 
    uint16_t inv_disg_limit_L1; //L1交流放电限值      
    uint16_t inv_disg_limit_L2; //L2交流放电限值   
    uint16_t inv_disg_limit_L3; //L3交流放电限值
    uint16_t ac_input_rated_current_L1;     //机器L1交流输入额定电流     
    uint16_t ac_input_rated_current_L2;     //机器L2交流输入额定电流    
    uint16_t ac_input_rated_current_L3;     //机器L3交流输入额定电流  
    uint16_t ac_output_rated_current_L1;     //机器L1交流输出额定电流     
    uint16_t ac_output_rated_current_L2;     //机器L2交流输出额定电流    
    uint16_t ac_output_rated_current_L3;     //机器L3交流输出额定电流   
    uint16_t grid_ac_output_rated_current_L1; //电网L1交流输入额定电流（机器+旁路带载）        
    uint16_t grid_ac_output_rated_current_L2; //电网L2交流输入额定电流（机器+旁路带载）    
    uint16_t grid_ac_output_rated_current_L3; //电网L3交流输入额定电流（机器+旁路带载）       
    uint16_t rated_Voltage;             //额定电压   
    uint16_t rated_Frequency;             //额定频率   
    rw_cmd_struct   rw_cmd;
}inv_about_struct;


/**
 * @brief Type = 0x14
 *
 */
typedef struct {
    uint8_t  status;        // 工作状态
    uint8_t  input_type;    // 接入类型 0--PV,1--CAR,2--Adpater,3--other
    uint16_t input_power;   // 输入功率 w
    uint16_t input_voltage; // 输入电压 0.1V
    uint16_t input_current; // 输入电流 0.1A
   // uint16_t revd[3];
}pv_detail_struct;

typedef union {
	uint8_t pv_total;

	struct {
		uint8_t dc_pv_numbers : 4;
		uint8_t ac_pv_numbers : 4;
	};
} pv_numbers_t;

typedef struct {
    uint32_t total_chg_power;   // w
    uint32_t total_chg_energy;  // 0.1kwh
    pv_numbers_t pv_number;   // pv接入数量
    pv_detail_struct  pv_detail[10];      // 假设最大10路pv
    rw_cmd_struct   rw_cmd;
}inv_pv_struct;

/**
 * @brief Type = 0x15
 *
 */
typedef struct {
    int16_t input_power;   // 输入功率 w //M139.3 uint16_t
    uint16_t input_voltage; // 输入电压 0.1V
    int16_t input_current; // 输入电流 0.1A //M139.3 uint16_t
}grid_detail_struct;

typedef struct {
    uint16_t freq;              // 电网频率 0.1Hz
    uint32_t total_chg_power;   // 电网总充电功率 1w
    uint32_t total_chg_energy;  // 0.1kwh 电网充电能量
    uint32_t total_fb_energy;   // 0.1kwh 电网馈电能量
    uint8_t  grid_phase_number; // grid相数
    grid_detail_struct  grid_detail[3];   // 假设最大10路grid
    uint8_t Sign_Valid;     // 
    int16_t grid_angle;    //电网角度
    rw_cmd_struct   rw_cmd;
}inv_grid_struct;

/**
 * @brief Type = 0x16
 *
 */
typedef struct {
    uint16_t load_power;    // 1W
    uint16_t load_voltage;  // 0.1V
    uint16_t load_current;  // 0.1A
}ac_load_struct;

typedef struct {
    uint32_t total_dc_load_power; // w 总的DC负载功率
    uint32_t total_dc_load_energy;  // 总的DC负载能量
    uint16_t dc_05v_load_power; // w 5v负载功率
    uint16_t dc_05v_load_current; // 0.1A 5v负载电流
    uint16_t dc_12v_load_power; // w 12v负载功率
    uint16_t dc_12v_load_current; // 0.1A 12v负载电流
    uint16_t dc_24v_load_power; // w 24v负载功率
    uint16_t dc_24v_load_current; // 0.1A 24v负载电流
    uint32_t total_ac_load_power; // w 总的AC负载功率
    uint32_t total_ac_load_energy;  // 总的AC负载能量
    uint8_t ac_phase_number;    // ac输出并机相数
    ac_load_struct ac_load[6]; // ac负载信息,根据并机相数
    rw_cmd_struct   rw_cmd;
}inv_load_struct;


/**
 * @brief Type = 0x17
 *
 */
typedef struct {
    uint8_t work_status;    // 逆变工作状态
    int16_t power;         // 逆变功率 M139.3 uint16_
    uint16_t voltage;       // 逆变电压
    int16_t current;       // 逆变电流 M139.3 uint16_
    // 将这四个寄存器迁移至数组外，modbus对应数组预留的位置不够，且在380中暂不使用
    // uint8_t Sign_Valid;     //
    // uint8_t AmbientTemp;     //
    // uint8_t InvMaxTemp;     //
    // uint8_t PvDcdcMaxTemp;     //
}inv_detail_struct;

typedef struct {
    uint16_t freq;          // 逆变频率
    uint32_t total_energy;  // 逆变的总能量
    uint8_t  phase_number;  // 相位数量 最多3相
    inv_detail_struct  inv_detail[6]; // 每相详细信息 最多3相
    uint8_t Sign_Valid;     //
    uint8_t AmbientTemp;     //
    uint8_t InvMaxTemp;     //
    uint8_t PvDcdcMaxTemp;     //
    rw_cmd_struct   rw_cmd;
}inv_data_struct;


/**
 * @brief Type = 0x18
 *
 */
typedef struct {
    uint8_t data[50];
    rw_cmd_struct   rw_cmd;
}inv_meter_struct;


/**
 * @brief Type = 0x19
 *
 */
typedef struct {
    uint32_t total_energy;  // 发电机发电总能量 0.1Kwh
    uint16_t power;     // 发电机功率 1w
    uint16_t voltage;   // 发电机电压 0.1V
    uint16_t current;   // 发电机电流 0.1A
    rw_cmd_struct   rw_cmd;
}inv_gen_struct;


typedef struct {
    uint8_t minutes; // 分
    uint8_t hours;   // 时
    //uint8_t minutes; // 分 M108
}_section_time;

typedef struct {
    uint8_t lable; // 0：不启用, 1：充电时间区间，2：放电时间区间
	// _section_time end;   // 结束时间 (时:分)
    // _section_time start; // 开启时间 (时:分)
    _section_time start; // 开启时间 (时:分)//M128
    _section_time end;   // 结束时间 (时:分)
}time_control;

typedef enum{
    Self_HiConf_NULL = 1,                       //  无
    Self_HiConf_Time_Discharg,             //  定时充放电
    Self_HiConf_Use,                   //  自发自用
    Self_HiConf_Child_Lock,                //  童锁
    Self_HiConf_Weak_Grid_Adaptation,       //  弱电网自适应
    Self_HiConf_Super_on,                   //  大力士
    Self_HiConf_MAX,             //  自定义模式枚举最大值
}Self_HiConf_Type;  //自定义模式高字节枚举

//低字节：0无效；1：使能；2：不使能
typedef union {
    uint16_t all;
    struct {
        uint8_t en_app_selfset : 1;             // bit0 机器是否支持自定义功能模式
        uint8_t dis_app_selfset : 1;            // bit1 机器是否支持自定义功能模式
        uint8_t en_app_charge_set : 1;          // bit2 表示机器是否允许APP进行充电模式设置
        uint8_t dis_app_charge_set : 1;         // bit3 表示机器是否允许APP进行充电模式设置
        uint8_t en_child_lock : 1;              // bit4 表示机器是否允许APP进行充电模式设置
        uint8_t dis_child_lock : 1;             // bit5 表示机器是否允许APP进行充电模式设置
        uint8_t revd : 2;                       // bit6~bit7 预留

        uint8_t self_HiConf_Type;               // 高字节枚举值 设备端物理开关对应功能(见Self_HiConf_Type）；APP侧为灵动开关
    }bit;
}self_configuration1;

//低字节：0无效；1：使能；2：不使能
typedef struct {
    union{
        uint16_t all;

        struct {
            uint8_t remoteEn : 2 ;                  // bit0~1  远程关机使能 0-invalid;1-enable;2-disable;
            uint8_t remoteOffCtrlEn : 2 ;                  // bit2~3  底层是否支持远程关机功能 0-invalid;1-enable;2-disable;
            uint8_t res : 4;                        // bit4~7 预留

            uint8_t sleepTime ;             //  自动休眠天数设置 0-无效，1~7对应1~7天；FF永不休眠
        }bit;
    };
}remote_Set1;

//低字节：0无效；1：使能；2：不使能
typedef struct {
    union{
        uint16_t all;

        struct {
            uint8_t KeepEn : 2 ;                  // bit0~1  最低SOC保有量使能 0-invalid;1-enable;2-disable;
            uint8_t res : 6;                        // bit2~7 预留

            uint8_t socKeep ;             // 最低SOC保有量（1-100%）
        }bit;
    };
}rv_Soc_OwenerShip1;

//低字节：0无效；1：使能；2：不使能
typedef struct {
    union 
    {
        uint16_t all;

        struct {
            uint8_t level ;                  // bit0~1   低字节：0:无效，1：等级1；2：等级2
            uint8_t res ;             // 预留
         }bit;
    };  
}level_Switch1;

typedef union{
    uint16_t all;

    struct{
        uint8_t power_threshold;/*功率阈值 15~30W 机器默认18W*/
        uint8_t revd;
    }bit;
}remote_power_threshold;

typedef union{
    uint16_t all;

    struct{
        uint8_t num_set;//设置的数量（读写）
        uint8_t communication_num;/*当前正常通讯数量 （只读）*/
    }bit;
}number_show;

typedef union{
    uint16_t all;

    struct{
        uint8_t revd;
        uint8_t soc_ownership;/*最高SOC保有量（1-100%）通过PV,AC充电的最高SOC阈值，高于此SOC，PCS停止充电*/
    }bit;
}soc_max_ownership;

typedef union{
    uint16_t all;

    /*
    0-invalid;1-enable;2-disable;
    bit0/1：PV1(2060)是否开启高级模式
    bit2/3：PV2(2061)是否开启高级模式
    bit4/5: PV3
    bit6/7: PV4
    bit8/9: PV5
    bit10/11: PV6
    bit12~15:预留*/
    struct{
        uint16_t pv_1set:2;
        uint16_t pv_set2:2;
        uint16_t pv_set3:2;
        uint16_t pv_set4:2;
        uint16_t pv_set5:2;
        uint16_t pv_set6:2;
        uint16_t revd:4;

    }bit;
}pv_senior_setting;

typedef union{
    uint16_t all;
    struct{
        uint16_t color1_set:2; /*1:暖色；2：冷色；(0无效）源于PPS019，20250226*/
        uint16_t revd:6; /*预留*/
        uint16_t light_level:8; /*灯光亮度 0: 无效 1-101：亮度 0% - 100% */
    }bit;
}led_color_setting;

typedef union {
    uint16_t all;
    struct { /* 8路开关， 0，3：无效。1：继电器闭合。2：继电器断开 */
        uint16_t sw1: 2;
        uint16_t sw2: 2;
        uint16_t sw3: 2;
        uint16_t sw4: 2;
        uint16_t sw5: 2;
        uint16_t sw6: 2;
        uint16_t sw7: 2;
        uint16_t sw8: 2;
    } bit;
} switch_setting_t;

// typedef union {
//     uint16_t all;
//     struct {
//         uint8_t child_lock : 2;            // bit0~1 童锁开关
//         uint16_t revd : 14;                 // bit2~bit15 预留
//     }bit;
// }switch_setting;

/**
 * @brief Type = 0x1A
 *
 */

#define	INV_SET00_STRUCT_COMMON_0x1A \
    uint16_t  time1;    /* 高8bit-年；低8bit-月； */\
    uint16_t  time2;    /* 高8bit-日；低8bit-时；*/\
    uint16_t  time3;    /* 高8bit-分；低8bit-秒；*/\
    uint16_t  res;\
    uint8_t  work_mode;     /* 00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网*/\
    ctrl_event  ctrl;       /* 一次性控制事件,// 2006 逆变设备清除事件*/\
    uint8_t  ctrl_led;       /* 控制LED*/\
    uint8_t  ctrl_meter;     /* 控制电表*/\
    uint8_t  ctrl_pv;        /* 控制PV*/\
    uint8_t  ctrl_inv;     /* 控制逆变*/\
    uint8_t  ctrl_ac;  /* 控制ac*/\
    uint8_t  ctrl_dc;  /* 控制dc*/\
    uint8_t  ctrl_poweron;  /* 控制电源*/\
    uint8_t  ctrl_dc_eco;       /* 控制dc eco模式0：关闭，1：开启*/\
    uint8_t  ctrl_dc_eco_time;  /* 控制dc eco模式空闲时间*/\
    uint16_t eco_dc_power_value;    /* dc eco模式触发运行功率*/\
    uint8_t  ctrl_ac_eco;       /* 控制ac eco模式 0：关闭，1：开启*/\
    uint8_t  ctrl_ac_eco_time; /* 控制ac eco模式空闲时间*/\
    uint16_t eco_ac_power_value; /* ac eco模式触发运行功率*/\
    uint8_t  ctrl_chg_mode; /* 控制充电模式*/\
    uint8_t  ctrl_super_power; /* 控制超级输出模式*/\
    uint8_t  ctrl_low_cap_pct;  /* 系统低电量百分比*/\
    uint8_t  ctrl_high_cap_pct; /* 系统满电量百分比*/\
    uint8_t  ctrl_inv_mode; /* 单火线，双火线，并机，并机双火线*/\
    uint8_t  ctrl_dev_id; /* 给逆变设置编号*/\
    uint8_t  ctrl_all_energy_type; /* 历史能量*/\
    uint8_t  ctrl_now_energy_type; /* 当前能量*/\
    uint8_t  ctrl_log_page;   /* 控制历史记录页*/\
    uint8_t  ctrl_time_area; /* 控制时间分段管理*/\
    time_control  ctrl_time[10];\
    uint8_t ctrl_PvType[6];\
    uint8_t ctrl_alarm_voice;\
    uint8_t setLcdActiveTime; /* 1：15s；2: 30s；3: 1分；4：5分；5：常亮 */\
    self_configuration1 self_config; /* 自定义模式 */\
    remote_Set1 remoteSet;/*远程设置 */  \
    uint16_t remoteSoc;/*远程设置相关SOC阈值 */ \
    rv_Soc_OwenerShip1 ownerShip;/*房车SOC保有量设置 */ \
    level_Switch1 LevelSwitch;/* 童锁开关的等级开关 */ \
    uint16_t sleepRemainTime;/* 休眠剩余时间 min*/\    
    uint16_t remote_set_power;/*2079 远程设置功率阈值*/ \
    number_show pack_set_show;/*2080 电池包数量*/\
    number_show inv_set_show;/*2081 INV数量*/\
    number_show dcdc_set_show;/*2082 DCDC数量*/\
    pv_senior_setting pv_senior_set;/*2084 pv高级设置*/\
    soc_max_ownership soc_max_ownership_set;/*2083 soc最高保有量设置*/\
    led_color_setting ledColorSet;/*2078 LED颜色设置*/\
    uint16_t dc_output;/*2085 DC输出*/\
    /*20251127 家储新能源法规JA12相关设置*/\
    uint16_t Regulatory_set;/*2086 合规设置*/\
    uint16_t Cycle_capacity;/*2087 循环容量 0.1Kwh*/\
    uint16_t Cycle_max_capacity;/*2088 最大循环容量 0.1Kwh*/\
    uint8_t Effective_time_mon;/*2089 生效时间*/\
    uint8_t Effective_time_year;/*2089 生效时间*/\
    uint8_t Effective_time_hour;/*2090 生效时间*/\
    uint8_t Effective_time_day;/*2090 生效时间*/\
    uint8_t Effective_time_sec;/*2091 生效时间*/\
    uint8_t Effective_time_min;/*2091 生效时间*/\
    uint16_t ECO_status;/*2092 ECO状态、是否支持JA12*/\
    uint16_t set_AC_branch;/*2093 控制AC支路*/\
    uint16_t set_DC_branch;/*2094 控制DC支路*/\
    uint8_t AC_button_sign; /*AC按钮标识，内部使用，不暴露到外部*/\
    uint8_t nc[12];

typedef struct {
	INV_SET00_STRUCT_COMMON_0x1A
    rw_cmd_struct   rw_cmd;
}inv_set00_struct;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint8_t ct_enable : 8;//低字节：CT测试使能；
			uint8_t ct_result : 8;//高字节：CT测试结果
		};
	};
} ct_test_t;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint16_t ct_grid : 8; //电网端口CT比例
			uint16_t ct_ac_pv : 8; //交流光伏CT比例
		};
	};
} ct_ratio_t;

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
		}bit;
	};
} ctrl_mix_t;

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
} ctrl_mix2_t;

/**
 * @brief Type = 0x1B
 *
 */
typedef struct {
	union {
		uint8_t all;

		struct {
			uint8_t enable : 2;
			uint8_t reset : 2;
			uint8_t res1 : 2;
			uint8_t res2 : 2;
		};
	};
} bat_maintain_t;

/**
 * @brief Type = 0x1B
 *
 */

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
}GenSet;

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t minute;
            uint8_t hour;
        };
    };
}SetTimeCtrl;

typedef struct{
    
    uint16_t revd;
    SetTimeCtrl SetTimeCtrlStart;
    SetTimeCtrl SetTimeCtrlEnd;
    
}SetTimeCtrl2;


typedef union {
    uint16_t all;
    struct {
        uint8_t low_power_on : 2;         // bit0/1:低功耗使能
        uint8_t revd : 2;        // bit2/3:预留
        uint8_t revd1 : 4;              // bit4~bit7 预留
        uint8_t revd12 : 8;     // bit8~bit15 预留
    }bit;
}func_set_struct;

/**
 * @brief Type = 0x1B
 *
 */

#define	INV_SET01_STRUCT_COMMON_0x1B \
    uint8_t password[8]; /* 设置区密码*/\
    uint8_t ctrl_reset_factory; /* 恢复出厂 2206:0-无效;1-美规；2-日规；3-欧规；4-澳规；5-中国,6-非洲；7-德国;*/\
    uint8_t ctrl_grid;      /* 控制电网*/\
    uint8_t ctrl_feedback;  /* 控制馈电*/\
    uint16_t ctrl_output_inv_volt;  /* 输出电压控制*/\
    uint16_t ctrl_output_inv_freq;   /* 输出电压频率*/\
    uint16_t ctrl_chg_max_volt; /* 最大充电电压*/\
    uint16_t ctrl_pv_max_curr; /* 最大PV充电电流*/\
    uint16_t ctrl_grid_max_power;   /* 电网最大功率*/\
    uint16_t ctrl_grid_max_curr; /* 电网最大电流*/\
    uint16_t ctrl_feedback_max_power;   /* 电网馈电最大功率*/\
    uint16_t ctrl_feedback_max_curr; /* 电网馈电最大电流*/\
    uint16_t off_grid_micro_rated_power; /* 离网交流PV额定功率（该参数设置为系统实际接入的微逆的额定功率，使得逆变根据实际情况更准确的调节微逆功率）*/\
    uint8_t  ctrl_user_area; /* 用户地区设置*/\
    uint8_t  ctrl_pv_paralle[6]; /* 设置PV并联开启和关闭*/\
    uint8_t  ctrl_grid_plus;    /* 电网增强模式*/\
    uint8_t  ctrl_save_power_state; /* 保存AC,DC关机前状态*/\
    uint8_t  ctrl_meter_enable; /* 电表开启*/\
    uint8_t  ctrl_meter_select; /* 电表类型选择*/\
    uint8_t  ctrl_Inv_Multi_enable; /* 逆变器并机模式*/\
    uint8_t  ctrl_Inv_addr_Set; /* 设置逆变器地址*/\
    ct_test_t ct_test;\
	ctrl_mix_t ctrl_mix;/*2232  ctrl_mix1_t*/\
	ctrl_mix2_t ctrl_mix2;\
    uint16_t    ChargingPile_SET;  /*充电桩电压设置*/ /* 充电桩型号 chargingpile_type*/\
    ct_ratio_t  ct_ratio;           /* CT比例*/\
    /* ct_test_t   ac_ct_test;          交流耦合电表CT检测*/\
    GenSet GenSet; /*2246*/\
    uint16_t    Gen_SOC_Start;  /* 高于此SOC开始*/\
    uint16_t    Gen_SOC_Stop;  /* 低于此SOC停止*/\
    SetTimeCtrl2 SetTimeCtrl[3]; /* 时间段控制*/\
    uint16_t Undervoltage_protection; /* 欠压保护设置值*/\
    uint16_t Undervoltage_protection_time; /* 欠压保护设置时间*/\
    uint16_t Highvoltage_protection; /* 高压保护设置值*/\
    uint16_t Highvoltage_protection_time; /* 高压保护设置时间*/\
    uint16_t Underfrequency_protection; /* 欠频保护设置值*/\
    uint16_t Underfrequency_protection_time; /* 欠频保护设置时间*/\
    uint16_t Overvoltage_protection; /* 过频保护设置时间*/\
    uint16_t Overvoltage_protection_time; /* 过频保护设置时间*/\
    uint16_t setting_pv;/* 设置PV开启关闭 */\
    uint16_t Phase_set;/* 单相双火相设置 */\
    uint16_t DCHUB_set;/* DCHUB设置电压_AC380设置 */\
    uint16_t SetGridMaxCurrent_in;/* 最大电网输入电流 */\
    uint16_t HomeCarBat_Set;/* 房车场景及电池设置 */\
    uint16_t AltClose_Delay;/* Alt延迟关闭输出时间 */\
    uint16_t HomeCar_Switch;/* 房车开关杂项 */\
    uint32_t HomeCar_BatCapacity;/* 房车电池容量 */\
    uint16_t Battery_Type;/* 房车电池容量 */\
    func_set_struct Func_Set;/* 开关设置 */\
    uint16_t Ems_Mode;/* EMS运行模式 */\
    uint16_t Dial_Switch_Status;/* 拨码开关状态 */\
    /* @brief 预留区*/\
    uint8_t  res1[15];

typedef struct {
//    uint8_t password[8]; // 设置区密码
//    uint8_t ctrl_reset_factory; // 恢复出厂 2206:0-无效;1-美规；2-日规；3-欧规；4-澳规；5-中国,6-非洲；7-德国;
//    uint8_t ctrl_grid;      // 控制电网
//    uint8_t ctrl_feedback;  // 控制馈电
//    uint16_t ctrl_output_inv_volt;  // 输出电压控制
//    uint16_t ctrl_output_inv_freq;   // 输出电压频率
//    uint16_t ctrl_chg_max_volt; // 最大充电电压
//    uint16_t ctrl_pv_max_curr; // 最大PV充电电流
//    uint16_t ctrl_grid_max_power;   // 电网最大功率
//    uint16_t ctrl_grid_max_curr; // 电网最大电流
//    uint16_t ctrl_feedback_max_power;   // 电网馈电最大功率
//    uint16_t ctrl_feedback_max_curr; // 电网馈电最大电流
//    uint16_t off_grid_micro_rated_power; // 离网交流PV额定功率（该参数设置为系统实际接入的微逆的额定功率，使得逆变根据实际情况更准确的调节微逆功率）
//    uint8_t  ctrl_user_area; // 用户地区设置
//    uint8_t  ctrl_pv_paralle[6]; // 设置PV并联开启和关闭
//    uint8_t  ctrl_grid_plus;    // 电网增强模式
//    uint8_t  ctrl_save_power_state; // 保存AC,DC关机前状态
//    uint8_t  ctrl_meter_enable; // 电表开启
//    uint8_t  ctrl_meter_select; // 电表类型选择
//    uint8_t  ctrl_Inv_Multi_enable; // 逆变器并机模式
//    uint8_t  ctrl_Inv_addr_Set; // 设置逆变器地址
//    ct_test_t ct_test;
//	ctrl_mix_t ctrl_mix;//2232  ctrl_mix1_t
//	ctrl_mix2_t ctrl_mix2;
//    uint16_t    chargingpile_type;  // 充电桩型号
//    ct_ratio_t  ct_ratio;           // CT比例
//    // ct_test_t   ac_ct_test;         // 交流耦合电表CT检测
//    GenSet GenSet; //2246
//    uint16_t    Gen_SOC_Start;  // 高于此SOC开始
//    uint16_t    Gen_SOC_Stop;  // 低于此SOC停止
//    SetTimeCtrl2 SetTimeCtrl[3]; // 时间段控制
//    /// @brief 预留区
//    uint8_t  res1[16];
	INV_SET01_STRUCT_COMMON_0x1B

    rw_cmd_struct   rw_cmd;
}inv_set01_struct;

/**
 * @brief Type = 0x1C
 *
 */
#define	INV_SET02_STRUCT_COMMON_0x1C \
    uint16_t SetGridUV1Value;        /* 电网欠压LEVEL1保护值*/\
    uint16_t SetGridUV1Time;         /* 电网欠压LEVEL1触发时间*/\
    uint16_t SetGridUV2Value;        /* 电网欠压LEVEL2保护值*/\
    uint16_t SetGridUV2Time;         /* 电网欠压LEVEL2触发时间*/\
    uint16_t SetGridOV1Value;        /* 电网高压LEVEL1保护值*/\
    uint16_t SetGridOV1Time;         /* 电网高压LEVEL1保护时间*/\
    uint16_t SetGridOV2Value;        /* 电网高压LEVEL2保护值*/\
    uint16_t SetGridOV2Time;         /* 电网高压LEVEL2保护时间*/\
    uint16_t SetGridUF1Value;        /* 电网欠频保护值*/\
    uint16_t SetGridUF1Time;         /* 电网欠频保护时间*/\
    uint16_t SetGridOF1Value;        /* 电网过频保护值*/\
    uint16_t SetGridOF1Time;         /* 电网过频保护时间*/\
    uint16_t SetFreqSlope;           /* 降频斜率*/\
    uint16_t SetPowerPct;            /* 功率变化率*/\
    uint16_t SetGridVoltMinValue;    /* 电网重连电压下限值*/\
    uint16_t SetGridVoltMaxValue;    /* 电网重连电压上限值*/\
    uint16_t SetGridFreqMinValue;    /* 电网重连频率下限值*/\
    uint16_t SetGridFreqMaxValue;    /* 电网重连频率上限值*/\
    uint16_t SetGridRetryTime;       /* 电网异常重连时间*/\
    uint16_t SetGridAdvance;         /* 高级功能使能*/\
    uint16_t SetGridUF2Value;         /* 电网欠频保护值*/\
    uint16_t SetGridUF2Time;          /* 电网欠频保护时间*/\
    uint16_t SetGridOF2Value;         /* 电网过频保护值*/\
    uint16_t SetGridOF2Time;          /* 电网过频保护时间*/\
    uint16_t SetGridAVGOVValue;       /* 均值过压保护值*/\
    uint16_t SetGridArea;             /* 电网认证使用地区*/\
    /* @brief 预留区*/\
    uint8_t  res1[20];                /* 保留*/



typedef struct {
//    uint16_t SetGridUV1Value;        // 电网欠压LEVEL1保护值
//    uint16_t SetGridUV1Time;         // 电网欠压LEVEL1触发时间
//    uint16_t SetGridUV2Value;        // 电网欠压LEVEL2保护值
//    uint16_t SetGridUV2Time;         // 电网欠压LEVEL2触发时间
//    uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
//    uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
//    uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
//    uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
//    uint16_t SetGridUF1Value;        // 电网欠频保护值
//    uint16_t SetGridUF1Time;         // 电网欠频保护时间
//    uint16_t SetGridOF1Value;        // 电网过频保护值
//    uint16_t SetGridOF1Time;         // 电网过频保护时间
//    uint16_t SetFreqSlope;           // 降频斜率
//    uint16_t SetPowerPct;            // 功率变化率
//    uint16_t SetGridVoltMinValue;    // 电网重连电压下限值
//    uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
//    uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
//    uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
//    uint16_t SetGridRetryTime;       // 电网异常重连时间
//    uint16_t SetGridAdvance;         // 高级功能使能
//    uint16_t SetGridUF2Value;         // 电网欠频保护值
//    uint16_t SetGridUF2Time;          // 电网欠频保护时间
//    uint16_t SetGridOF2Value;         // 电网过频保护值
//    uint16_t SetGridOF2Time;          // 电网过频保护时间
//    uint16_t SetGridAVGOVValue;       // 均值过压保护值
//    uint16_t SetGridArea;             // 电网认证使用地区
//    /// @brief 预留区
//    uint8_t  res1[20];                // 保留
	INV_SET02_STRUCT_COMMON_0x1C    
    rw_cmd_struct   rw_cmd;
}inv_set02_struct;

/**
 * @brief Type = 0x1D
 *
 */

#define	INV_SET03_STRUCT_COMMON_0x1D \
    uint16_t SetGridEnable;          /* 高级功能使能*/\
    uint16_t SetGridCountry;         /* 电网认证使用地区*/\
    uint16_t SetGridUV1Value;        /* 电网欠压LEVEL1保护值*/\
    uint16_t SetGridUV1Time;         /* 电网欠压LEVEL1触发时间*/\
    uint16_t SetGridUV2Value;        /* 电网欠压LEVEL2保护值*/\
    uint16_t SetGridUV2Time;         /* 电网欠压LEVEL2触发时间*/\
	uint16_t pf;					 /* 功率因数*/\
	uint16_t set_region;			 /* 地区*/\
	uint16_t mode_enable;			 /* 模式使能*/\
	uint16_t power_rate_limit;		 /* 每分钟功率变化比率*/\
    uint16_t SetGridAVGOVValue;      /* 电网均值过压*/\
    uint16_t SetGridOV1Value;        /* 电网高压LEVEL1保护值*/\
    uint16_t SetGridOV1Time;         /* 电网高压LEVEL1保护时间*/\
    uint16_t SetGridOV2Value;        /* 电网高压LEVEL2保护值*/\
    uint16_t SetGridOV2Time;         /* 电网高压LEVEL2保护时间*/\
	uint16_t power_reactive_to_active_ratio; /* 无功占额定有功比例*/\
	uint16_t vw1;					 /* Volt–watt响应模式的参数,Vw1<Vw2*/\
	uint16_t vw2;					 /* Volt–watt响应模式的参数,Vw1<Vw2*/\
    uint16_t nc2[1];\
    uint16_t SetGridUF1Value;        /* 电网欠频LEVEL1保护值*/\
    uint16_t SetGridUF1Time;         /* 电网欠频LEVEL1保护时间*/\
    uint16_t SetGridUF2Value;        /* 电网欠频LEVEL2保护值*/\
    uint16_t SetGridUF2Time;         /* 电网欠频LEVEL2保护时间*/\
	uint16_t vv1;					 /* V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4*/\
	uint16_t vv2;					 /* V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4*/\
	uint16_t vv3;					 /* V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4*/\
	uint16_t vv4;					 /* V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4*/\
    uint16_t SetGridOF1Value;        /* 电网过频LEVEL1保护值*/\
    uint16_t SetGridOF1Time;         /* 电网过频LEVEL1保护时间*/\
    uint16_t SetGridOF2Value;        /* 电网过频LEVEL2保护值*/\
    uint16_t SetGridOF2Time;         /* 电网过频LEVEL2保护时间*/\
    uint16_t nc4[4];\
    uint16_t SetGridVoltMinValue;    /* 电网重连电压下限值*/\
    uint16_t SetGridVoltMaxValue;    /* 电网重连电压上限值*/\
    uint16_t SetGridFreqMinValue;    /* 电网重连频率下限值*/\
    uint16_t SetGridFreqMaxValue;    /* 电网重连频率上限值*/\
    uint16_t SetGridRetryTime;       /* 电网异常重连时间*/\
	uint16_t SetDefaultValue;        /* 设置默认值*/\
    /* @brief 预留区*/\
    uint16_t  nc5[30];                /* 保留*/

typedef struct {
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
//    uint16_t SetGridAVGOVValue;      // 电网均值过压
//    uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
//    uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
//    uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
//    uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
//	uint16_t power_reactive_to_active_ratio; // 无功占额定有功比例
//	uint16_t vw1;					 // Volt–watt响应模式的参数,Vw1<Vw2
//	uint16_t vw2;					 // Volt–watt响应模式的参数,Vw1<Vw2
//    uint16_t nc2[1];
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
//    uint16_t nc4[4];
//    uint16_t SetGridVoltMinValue;    // 电网重连电压下限值
//    uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
//    uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
//    uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
//    uint16_t SetGridRetryTime;       // 电网异常重连时间
//	uint16_t SetDefaultValue;        // 设置默认值
//    /// @brief 预留区
//    uint16_t  nc5[9];                // 保留
	INV_SET03_STRUCT_COMMON_0x1D

    rw_cmd_struct   rw_cmd;
}inv_set03_struct;


/**
 * @brief Type = 0x20
 *
 */
typedef union {
    uint16_t all;
    struct {
        uint8_t type;
        uint8_t happened;
    }bytes;
}fault_state;

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    fault_state each;
    //uint16_t each;
    uint16_t fault_code;
}_log_struct;

typedef struct {
    uint16_t total_page;        // 总的历史页
    uint16_t current_page_seq;  // 当前历史页编号
    _log_struct log_data[5];
    rw_cmd_struct   rw_cmd;
}inv_log_struct;

/**
 * @brief Type = 0x21
 *
 */
typedef struct {
    uint16_t year;
    uint32_t energy;
}_year_energy;

typedef struct {
    uint8_t energy_type;
    uint32_t total_energy;
    _year_energy year_data[15];
    rw_cmd_struct   rw_cmd;
}inv_all_energy_struct;

/**
 * @brief Type = 0x22
 *
 */
typedef struct {
    uint8_t energy_type;
    uint16_t year;
    uint32_t total_energy;
    uint32_t month_data[12]; // 0.1Kwh
    uint16_t day_data[31]; // 0.1Kwh
    rw_cmd_struct   rw_cmd;
}inv_energy_struct;

/**
 * @brief Type = 0x23
 *
 */
typedef struct {
    uint8_t  res[8];
    uint16_t manufacturer;
    uint8_t  ssid[32];
    uint8_t  bssid[6]; // mac地址
    uint8_t  password[64];
    uint8_t  res1[4];
    uint8_t  sncode[20];
    uint32_t version;
    uint16_t serviceFlags;
    uint16_t networkType;
    uint16_t networkState;
    uint16_t wifiChannel;
    uint16_t wifiRSSI;
    uint16_t authmode;
    uint8_t  ipv4[4];
    uint8_t  mask[4];
    uint8_t  gateway[4];
    uint8_t  dns0[4];
    uint8_t  dns1[4];
    uint8_t  dns2[4];
    rw_cmd_struct   rw_cmd;
}inv_wifi_struct;

/**
 * @brief Type = 0x24
 *
 */
typedef struct {
	uint8_t hw_version[4];
	int16_t active_power;
	int16_t reactive_power;
   	uint16_t freq;
    uint16_t operationalModeState;
    uint16_t connection_status;
    uint16_t alarmStatus;
    uint16_t inverterStatus;
    uint16_t localControlModeStatus;
    uint16_t storageModeStatus;
    rw_cmd_struct rw_cmd;
}wifi_report_struct; 	// 外置wifi上报的数据结构

/**
 * @brief Type = 0x25
 *
 */
typedef struct {
	uint16_t const_pf_mode_en;
	uint16_t const_pf;
	uint16_t const_pf_excitation;
	uint16_t vref;
	uint16_t auto_vref_adj_en;
	uint16_t vref_adj_time_const;
	uint16_t volt_reactive_power_curve_v1;
	uint16_t volt_reactive_power_curve_v2;
	uint16_t volt_reactive_power_curve_v3;
	uint16_t volt_reactive_power_curve_v4;
	uint16_t volt_reactive_power_curve_q1;
	uint16_t volt_reactive_power_curve_q2;
	uint16_t volt_reactive_power_curve_q3;
	uint16_t volt_reactive_power_curve_q4;
	uint16_t volt_reactive_power_curve_time;
	uint16_t active_reactive_power_curve_p1;
	uint16_t active_reactive_power_curve_p2;
	uint16_t active_reactive_power_curve_p3;
	uint16_t active_reactive_power_curve_p1n;
	uint16_t active_reactive_power_curve_p2n;
	uint16_t active_reactive_power_curve_p3n;
	uint16_t active_reactive_power_curve_q1;
	uint16_t active_reactive_power_curve_q2;
	uint16_t active_reactive_power_curve_q3;
	uint16_t active_reactive_power_curve_q1n;
	uint16_t active_reactive_power_curve_q2n;
	uint16_t active_reactive_power_curve_q3n;
	uint16_t const_reactive_power;
	uint16_t volt_active_power_curve_v1;
	uint16_t volt_active_power_curve_v2;
	uint16_t volt_active_power_curve_p1;
	uint16_t volt_active_power_curve_p2;
	uint16_t volt_active_power_curve_time;
	uint16_t volt_trip_ov1;
	uint16_t volt_trip_ov2;
	uint16_t volt_trip_ov1_t;
	uint16_t volt_trip_ov2_t;
	uint16_t volt_trip_uv1;
	uint16_t volt_trip_uv2;
	uint16_t volt_trip_uv1_t;
	uint16_t volt_trip_uv2_t;
	uint16_t freq_trip_of1;
	uint16_t freq_trip_of2;
	uint32_t freq_trip_of1_t;
	uint32_t freq_trip_of2_t;
	uint16_t freq_trip_uf1;
	uint16_t freq_trip_uf2;
	uint32_t freq_trip_uf1_t;
	uint32_t freq_trip_uf2_t;
	uint16_t over_freq_droop_dbof;
	uint16_t under_freq_droop_dbuf;
	uint16_t over_freq_droop_kof;
	uint16_t under_freq_droop_kuf;
	uint16_t open_loop_response_time;
	uint16_t permit_service;
	uint16_t enter_service_volt_high;
	uint16_t enter_service_volt_low;
	uint16_t enter_service_freq_high;
	uint16_t enter_service_freq_low;
	uint16_t enter_service_delay;
	uint16_t enter_service_randomized_delay;
	uint16_t enter_service_ramp_rate;
	uint16_t maximun_active_power;
    rw_cmd_struct rw_cmd;
}wifi_param_struct; 	// 外置wifi下发的数据结构

typedef struct{
    union{
        uint64_t inv_transer_data; 

        struct{
            uint8_t dataindex;
            uint8_t buf1_L; 
            uint8_t buf1_H; 
            uint8_t buf2_L; 
            uint8_t buf2_H;
            uint8_t buf3_L; 
            uint8_t buf3_H;
            uint8_t revd;
        }transer_struct;
    };
}inv_transer_struct;

typedef struct{
    inv_transer_struct inv_transer_buf[5];
}Inv_Transer_Datas;
/**
 * @brief Type = 0x27
 *
 */
typedef struct {
    uint16_t  param[512];
    rw_cmd_struct rw_cmd;
}auth_struct;

/**
 * @brief 参数类型定义
 setdata_valid的 bit偏移
 */
typedef enum {
	CAN_INV_SETDATA_set00=0,//inv_set00
	CAN_INV_SETDATA_set01=1,//inv_set01
	CAN_INV_SETDATA_set02=2,//inv_set02,新产品废除
	CAN_INV_SETDATA_set03=3,//inv_set03
	CAN_INV_SETDATA_auth_param=4//auth_param

} setdata_valid_type_t;

typedef struct {
    uint8_t             devId;         // 当前数据属于哪个节点
    uint8_t             online;//1-设备在线；0-超时清零
    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
/*

并机排序之后才set，排地址时候清零
	*/    
    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

	
    inv_announce_struct inv_announce;  // 逆变主机的广播信息
    inv_base_struct     inv_base;      // inv单包主要信息
    inv_line_struct     inv_line;      // inv能量线
    inv_about_struct    inv_about;     // inv软件版本信息
    inv_pv_struct       inv_pv;        // inv光伏信息
    inv_grid_struct     inv_grid;      // inv电网信息
    inv_load_struct     inv_load;      // inv负载信息
    inv_data_struct     inv_data;      // inv逆变数据
    inv_meter_struct    inv_meter;     // inv电表数据
    inv_gen_struct      inv_gen;       // inv发电机数据
    inv_set00_struct    inv_set00;     // inv设置区00-基础设置区
    inv_set01_struct    inv_set01;     // inv设置区01-高级设置区
    inv_set02_struct    inv_set02;     // inv设置区02-认证设置区,新产品废除
    inv_set03_struct    inv_set03;     // inv设置区03-认证设置区
    inv_log_struct      inv_log;       // inv日志
    inv_all_energy_struct inv_all_energy;	// inv 15年能量信息
    inv_energy_struct   inv_energy;		// inv 当年能量信息
    inv_wifi_struct     inv_wifi;		// inv第三方wifi
	wifi_report_struct 	wifi_report;	// 外置wifi上报到服务器的数据(来自底层逆变设备)
	wifi_param_struct	wifi_param;		// 外置wifi下发到逆变器的数据(来自外部wifi模块)
    auth_struct    	 	auth_param; 	//认证参数
    Inv_Transer_Datas   inv_detailedinfo;  //inv用于触发故障上报时的详细信息
}inv_node_struct;

typedef struct {
    inv_set00_struct    inv_set00;  // inv设置区00
    inv_set01_struct    inv_set01;  // inv设置区01
    inv_set02_struct    inv_set02;  // inv设置区02
    inv_set03_struct    inv_set03;  // inv设置区03
    inv_wifi_struct     inv_wifi;   // inv第三方wifi
    auth_struct    	 	auth_param; //认证参数，modbus 40000透传
} bk_inv_node_set;




#pragma pack()
#endif
