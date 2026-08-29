#ifndef INV_TYPE_H
#define INV_TYPE_H

#include <stdint.h>
#include "comm_define.h"
#include "product_define.h"
#include "iot_define.h"
#include "pack_define.h"
#include "ota_define.h"
#include "modbus_protocol_alpha.h"
#include "can_product.h"
#include "modbus_data.h"

#define PV_MAX_NUM  4
#define MAX_YEAR_NUM    15

/* 设备主节点最大数量，每条主节点上都可以挂多个逆变子节点与PACK子节点 */
#define DEV_MAIN_NODE_MAX 	SYSTEM_PARALLELED_NUM // CAN总线逆变器储能系统数量

#define INV_MAX_NUM     1
#define IOT_MAX_NUM     1 // 一个逆变器系统固定只有一个IOT


#define INV_GROUP_MAX_NUM     INV_MAX_NUM

#define INV_IOT_CAN_ADDR    COMMON_INV_IOT_CAN_ADDR
#define INV_CAN_ADDR        COMMON_INV_ARM_CAN_ADDR
#define INV_CAN_DSP_ADDR    COMMON_INV_DSP_CAN_ADDR

#define AT1_CAN_ADDR        0xF0

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
#define INV_ANNOUNCE10       0x05
#define INV_ANNOUNCE11       0x06

#define INV_ANNOUNCE_0xFA        250//综合控制帧=0xFA

#define	MODBUS_VERSION_GAP 2000// <2000的为alpha,否则为beta


#define MD_SOFT_MAIN_CAP 6
#define MD_SOFT_BACK_CAP 6
#define MD_SOFT_MAX_CAP  (MD_SOFT_MAIN_CAP + MD_SOFT_BACK_CAP)

enum {
    IOT_TYPE_INFO_01H=0x01,//（IOT板信息）只读
    IOT_TYPE_SET_02H = 0x02,//（设置类变量）可读可写

};

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
        uint8_t ac_eco_on : 1;  // bit11 ac_eco开关
        uint8_t system_off : 1; // bit12 系统下电关机状态（须立即断开MQTT）
        uint8_t sleep_on : 1;   // bit13 系统休眠状态
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



typedef union {
    uint32_t all;
    struct {
		//1-enable;2-disable
        uint32_t wifi_enable : 2; // WiFi 开关;windy目前使用此bit，表示sta en,未使用 wifi_sta_enable;HMI使用，将错就错
        uint32_t wifi_sta_enable : 2; // wifi-STA开关
        uint32_t wifi_ap_enable : 2; // WiFi-AP开关
        uint32_t ble_enable : 2; // 蓝牙开关
        
        uint32_t ble_adv_enable : 2; // 蓝牙广播开关
        uint32_t ble_mash_enable : 2; // 蓝牙组网开关
        uint32_t _4g_enable : 2;//0：关闭，1：开启
        uint32_t gps_enable : 2;

        uint32_t Eth_enable : 2;//有线以太网使能
        uint32_t WIFI_mesh : 2;//wifi mesh使能
        uint32_t revd1 : 2;
		uint32_t new_dev_discovery : 2;//新设备发现模式（combox启用）
		
        uint32_t wifi_mesh_mode : 2; //wifi mesh mode
        uint32_t ble_client_enable : 2;  //webserver使能
        uint32_t mdns_en : 2;   //mDNS使能
        uint32_t webserver_en : 2;
		
    }bit;
}config_status;//cmd

typedef union {
    uint32_t all;
    struct {
        uint8_t wifi_sta : 1; // 1：联网,0：未联网
        uint8_t wifi_ap : 1;  // 1：连接,0：未连接
        uint8_t ble_connect : 1; // 1:连接,0：未连接
        uint8_t ble_mash : 1; // 1:连接,0：未连接 bit3:ble组网状态 0：断开；1：连接
        
        uint8_t _4g : 1; // 1:l连接,0：未连接
        uint8_t gps : 1; // 1:已定位,0：未定位
        uint8_t mqtt: 1; // 1:登录,0：未登录
        uint8_t wifi_mesh: 1; // 0: 断开； 1：连接
        
        uint8_t ble_mesh: 1; // 0: 断开； 1：连接
        uint8_t Eth_Stat: 1; //有线以太网 0: 断开； 1：连接
        uint8_t firmware_transfer : 1; //固件传输状态
        uint8_t revd: 5; //

        uint8_t revd2: 8; //
        uint8_t revd3: 8; //        
    }bit;
}link_status;//state,beta modbus 11018


typedef union {
	uint16_t all;
	struct {
		uint16_t Remote_set_enable      : 2;    // bit0/1:远程关机使能（用户是否需要该功能）
		uint16_t Remote_set_valid       : 2;    // bit2/3:底层是否支持远程关机功能（1：支持）
		uint16_t revd                   : 4;    // bit4~7：预留

		uint16_t Automatic_sleep_days   : 8;    // 高字节: 自动休眠天数设置(0-无效，1~7对应1~7天；FF永不休眠)
	};
} Remote_set_struct;

typedef union{
    uint16_t all;

    struct{
        uint8_t color_set:2;
        uint8_t revd:6;
        uint8_t brightness_set:8;
    }bit;
}led_set_struct;

typedef union {
    uint16_t all;
    struct {
        uint16_t lcd_data_adv_enable : 1;   // bit0:磁吸屏配套支持状态   
        uint16_t meter_shelly_enable : 1;   // bit1: 是否支持shelly电表
        uint16_t plug_s1_enable : 1;        // bit2:是否支持S1插座
        uint16_t modbus_tlv_enable : 1;     // bit3:是否支持ModbusTLV（功能码40004/40005）
    }bit;
} iot_factory_base_struct;

typedef struct {
	union {
		uint16_t all;

		struct {
            /*
            低字节：
            bit0~3: 
            0-无效（出厂模式）
            1-逆变器自行控制模式（预留）
            2-本地控制模式（比如EBOX）
            3-远程控制模式（云控制，不区分是否AI）
            4-APP控制、现场控制
            */
			uint8_t state : 4;
		};
	};
}ems_ctrl_struct;

typedef struct {
	union {
		uint16_t all;

		struct {
            // 低字节：COMBOX使用场景：0：无效；1：屋顶光伏；2：风机；FF：未设置场景
			uint8_t scene;
            
            // 高字节：bit0~3:项目种类：0：正常项目；1：PAYGO项目
            uint8_t type : 4;
		};
	};
}scene_struct;

typedef union {
    uint16_t all;
    struct {
        uint16_t support_ble_pwd    : 2; // bit1/0:密码区有效（支持密码功能）标志:0-无效；1-有效；2/3-预留（只读）
        uint16_t visitor_mode       : 2; // bit3/2:访客模式标志(手机APP处理逻辑，IOT存储标志):0-无效；1-禁用；2-使能（默认）.(可读可写)
        uint16_t ble_server_encrypt : 2; // bit5/4:ble_server模式加密模式:0-无效;1-非加密;2:加密;3-预留(可读可写)
    }bit;
} iot_mode_struct;


typedef union {
	uint16_t all;
	struct {
		uint8_t thunder_enable : 2;
		uint8_t thunder_state : 2;
		uint8_t thunder_cancel : 2;
        uint8_t thunder_work_mode_state : 2;
	};
} thunder_ctrl_t;

typedef union {
	uint16_t all;
	struct {
		uint16_t power_off_count : 4; // bit0~3 断电次数时间跨度0:invalid, 1:每日，2，每周，3，每月，4，每年，5-至今
        uint16_t save_money : 4; // bit4~7 省钱参数时间跨度
        uint16_t revd : 8; // bit8~15 预留
	};
} Time_Span_t;


typedef union {
    uint16_t all;
    struct {
        uint8_t app_temp_unit_set_flag : 1;  // APP温度单位设置标志: 0=未设置, 1=已设置
        uint8_t reserved : 7;                // 预留位
    };
}temp_unit_control_t;

typedef struct {
	union {
		uint16_t all;
		struct {
			uint16_t sta_passward_H32B_en : 8; // WIFI路由器密码高32字节有效标志,0xA5表示有效，否则无效
			uint16_t sta_authmode_open : 1;
            uint16_t sta_undhcp_enable : 1;
			uint16_t reserved : 6; // 预留
		};
	};
} sta_eanble_t;

/**
 * @brief Type = 0x00
 *
 */
typedef struct {
//    config_status cfg;     // IOT功能开启状态
    link_status   link;    // IOT功能连接状态
//    rw_cmd_struct rw_cmd;
}iot_base_struct;

typedef union {
    uint16_t all;

    struct {
        uint8_t ctrl_meter_enable;
        uint8_t ctrl_meter_select;
    };
} ctrl_meter_struct;


typedef union//2Byte
{
   // uint8_t all;
    uint16_t word;
    struct structIOT_ENABLE
    {
        uint8_t net_ch       :4;// 网络通道使能，0-无效；1-仅使能WIFI；2-仅使能有线以太网；3-仅使能4G模块；4-自动选择；5-禁止所有上网
        uint8_t revd2       :4;
        
        uint8_t revd4       :8;
    }bits;
}g_st_IOT_ENABLE;

typedef union {
	uint16_t all;
	struct {
//		"0-invalid;1-enable;2-disable;
		uint8_t trigger_regroup_IOT : 2;//bit1/0:触发IOT模块并机排序
		uint8_t trigger_regroup_INV : 2;//bit3/2:触发INV模块并机排序
		uint8_t trigger_regroup_PACK : 2;//bit5/4:触发PACK模块并机排序
		uint8_t trigger_BIND : 2;//bit7/6:设备绑定触发命令（生产、运维超级用户操作，条件：在单机条件下，将IOT硬件和逆变器硬件关联）

		uint8_t revd : 8;//bit15~6:预留
	}bit;
} Struct_IOT_Enable_mix1;

typedef union {
    uint16_t all;

    struct {
        int8_t hour;//低8bit：范围在-12~14（小时）
        int8_t min;//高8bit: 范围在0~59（分钟）
    };
} time_res_struct;

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

typedef struct {
    uint8_t DataCode;         // Byte0
    union {
        uint8_t byte1;
        struct {
            uint8_t BmsPowerOff    : 1; // Bit0
            uint8_t BmsSleep       : 1; // Bit1
            uint8_t BattDsgEnable  : 1; // Bit2
            uint8_t BattChgEnable  : 1; // Bit3
            uint8_t reserved4      : 1; // Bit4
            uint8_t reserved5      : 1; // Bit5
            uint8_t OtaReady       : 1; // Bit6
            uint8_t reserved7      : 1; // Bit7
        };
    };
    uint16_t reserved1;       // Byte2~3
    uint16_t BattVolt;        // Byte4~5，单位0.1V
    uint8_t FanSpeed1;        // Byte6，0~100
    uint8_t FanSpeed2;        // Byte7，0~100
} __attribute__((packed)) bms_data_frame_t;

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

    iot_date_t  date;           	// 逆变器日历，INV主动上报，实时数据，周期2s-陆文杰
    uint16_t    online;          	// 逆变并机数量,逆变通讯在线状态 按位表示

    uint16_t    alarm[4];       	// 告警信息

    uint16_t    fault[4];       	// 故障信息

    uint32_t    total_chg_power; 	// 逆变总的充电功率
    uint32_t    total_dsg_power; 	// 逆变总的放点功率

    uint16_t    fault5;       		// 告警信息5
    uint16_t    fault6;             // 告警信息6
    uint8_t     res1[4];      		// 保留

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

    bms_data_frame_t system_all_status; // 系统总帧

    uint16_t Pv_Chg_Power[3];   // PV允许充电功率
    
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
    uint32_t TotalEnergy_Car;   // 行车充电总能量
    uint32_t Sleep_endurance;   // 休眠续航时间
    uint16_t CarTotalPower; // 行车快充总功率
    uint32_t PackTotalChgEnergy;// INV计算PACK总充电能量
    uint32_t PackTotalDsgEnergy;// INV计算PACK总放电能量
    uint16_t INV_TEST;
    uint16_t Switch_memory_states;
    uint16_t fault6;
    uint16_t revd[20];
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
    can_soft_version  soft[6];  // 最多6个软件版本
    
	uint16_t P_INV_chg_limit_L1;    //1155
	uint16_t P_INV_chg_limit_L2;    //1156
	uint16_t P_INV_chg_limit_L3;    //1157
	uint16_t P_INV_disg_limit_L1;   //1158
	uint16_t P_INV_disg_limit_L2;   //1159
	uint16_t P_INV_disg_limit_L3;   //1160

    /*1161~1169*/
	uint16_t machine_l1_ac_input_rated_current;
    uint16_t machine_l2_ac_input_rated_current;
    uint16_t machine_l3_ac_input_rated_current;
    uint16_t machine_l1_ac_output_rated_current;
    uint16_t machine_l2_ac_output_rated_current;
    uint16_t machine_l3_ac_output_rated_current;
    uint16_t grid_l1_ac_output_rated_current;
    uint16_t grid_l2_ac_output_rated_current;
    uint16_t grid_l3_ac_output_rated_current;

    uint16_t Rated_Voltage;	//169
    uint16_t Rated_Frequency;	//170

    uint16_t grid_LN_phase_map_result;
    uint16_t grid_CT_phase_map_result;
    uint16_t inverter_LN_phase_map_result;
    uint16_t inverter_CT_phase_map_result;
    
    uint16_t revd[20];

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
    uint16_t grid_angle;
	uint16_t pf;
	uint16_t Grid1Papparent;
	uint16_t Grid1Preactive;
	uint16_t Grid2Papparent;
	uint16_t Grid2Preactive;
	uint16_t Grid3Papparent;
	uint16_t Grid3Preactive;
    uint32_t GridPower_U32[3];
    uint32_t GridPapparent_U32[3];
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

// typedef struct {
//     uint32_t total_dc_load_power; // w 总的DC负载功率
//     uint32_t total_dc_load_energy;  // 总的DC负载能量
//     uint16_t dc_05v_load_power; // w 5v负载功率
//     uint16_t dc_05v_load_current; // 0.1A 5v负载电流
//     uint16_t dc_12v_load_power; // w 12v负载功率
//     uint16_t dc_12v_load_current; // 0.1A 12v负载电流
//     uint16_t dc_24v_load_power; // w 24v负载功率
//     uint16_t dc_24v_load_current; // 0.1A 24v负载电流
//     uint32_t total_ac_load_power; // w 总的AC负载功率
//     uint32_t total_ac_load_energy;  // 总的AC负载能量
//     uint8_t ac_phase_number;    // ac输出并机相数
//     ac_load_struct ac_load[6]; // ac负载信息,根据并机相数
//     rw_cmd_struct   rw_cmd;
// }inv_load_struct;

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
    ac_load_struct ac_load[3]; // ac负载信息,根据并机相数
    uint8_t Sign_Valid;
    uint16_t DCLoadTotalVoltage;// DC负载总电压
    uint16_t DCLoadTotalCurrent;// DC负载总电流
	uint16_t pf;
	uint16_t ACLoad1Papparent;
	uint16_t ACLoad1Preactive;
	uint16_t ACLoad2Papparent;
	uint16_t ACLoad2Preactive;
	uint16_t ACLoad3Papparent;
	uint16_t ACLoad3Preactive;
    uint32_t ACLoadPower_U32[3];
    uint32_t ACLoadPapparent_U32[3];
    uint16_t revd[10];
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
}inv_detail_struct;

// typedef struct {
//     uint16_t freq;          // 逆变频率
//     uint32_t total_energy;  // 逆变的总能量
//     uint8_t  phase_number;  // 相位数量 最多3相
//     inv_detail_struct  inv_detail[6]; // 每相详细信息 最多3相
//     rw_cmd_struct   rw_cmd;
// }inv_data_struct;

typedef struct {
    uint16_t freq;          // 逆变频率
    uint32_t total_energy;  // 逆变的总能量
    uint8_t  phase_number;  // 相位数量 最多3相
    inv_detail_struct  inv_detail[3]; // 每相详细信息 最多3相
    uint8_t Sign_Valid;     //
    uint8_t AmbientTemp;     //
    uint8_t InvMaxTemp;     //
    uint8_t PvDcdcMaxTemp;     //
    uint16_t Bms_current_in_VPP;
    uint16_t MeterSynchronization;
    uint16_t revd[10];
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
    uint8_t  ctrl_meter;     /* 控制电表,20240920 INV实际未使用*/\
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
    uint16_t Custom_mode_set;/* 自定义模式*/\
    Remote_set_struct Remote_set;/*远程设置*/\
    uint16_t Remote_set_soc_limit;/*远程设置相关SOC*/\
    uint16_t Rv_SOC_Ownership;/* 房车SOC保有量设置 */\
    uint16_t LevelSwitch;/* 童锁等级 */\
    uint16_t sleep_remain_time;   /* 休眠剩余时间 */\
    uint16_t remote_set_power;    /* 远程设置功率 */\
    uint16_t pack_set_show;       /* 电池包设置显示 */\
    uint16_t inv_set_show;        /* 逆变器设置显示 */\
    uint16_t dcdc_set_show;       /* DCDC设置显示 */\
    uint16_t pv_set;              /* PV高频设置 */\
    uint16_t soc_h;               /* SOC最高保有量设置 */\
    led_set_struct led_set;             /* LED颜色设置 */\
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
    uint16_t set_AC_branch; /* 2093 */\
    uint16_t set_DC_branch; /* 2094 */\
    uint16_t DC_SET;        /* 2095 */\
    uint16_t AC_button_sign;/* 2096 */\
    uint16_t revd[10];
//};

typedef struct {
//    uint16_t  time1;    // 高8bit-年；低8bit-月；
//    uint16_t  time2;    // 高8bit-日；低8bit-时；
//    uint16_t  time3;    // 高8bit-分；低8bit-秒；
//    uint16_t  res;
//    uint8_t  work_mode;     // 00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网
//    ctrl_event  ctrl;       // 一次性控制事件,// 2006 逆变设备清除事件
//    uint8_t  ctrl_led;       // 控制LED
//    uint8_t  ctrl_meter;     // 控制电表
//    uint8_t  ctrl_pv;        // 控制PV
//    uint8_t  ctrl_inv;     // 控制逆变
//    uint8_t  ctrl_ac;  // 控制ac
//    uint8_t  ctrl_dc;  // 控制dc
//    uint8_t  ctrl_poweron;  // 控制电源
//    uint8_t  ctrl_dc_eco;       // 控制dc eco模式0：关闭，1：开启
//    uint8_t  ctrl_dc_eco_time;  // 控制dc eco模式空闲时间
//    uint16_t eco_dc_power_value;    // dc eco模式触发运行功率
//    uint8_t  ctrl_ac_eco;       // 控制ac eco模式 0：关闭，1：开启
//    uint8_t  ctrl_ac_eco_time; // 控制ac eco模式空闲时间
//    uint16_t eco_ac_power_value; // ac eco模式触发运行功率
//    uint8_t  ctrl_chg_mode; // 控制充电模式
//    uint8_t  ctrl_super_power; // 控制超级输出模式
//    uint8_t  ctrl_low_cap_pct;  // 系统低电量百分比
//    uint8_t  ctrl_high_cap_pct; // 系统满电量百分比
//    uint8_t  ctrl_inv_mode; // 单火线，双火线，并机，并机双火线
//    uint8_t  ctrl_dev_id; // 给逆变设置编号
//    uint8_t  ctrl_all_energy_type; // 历史能量
//    uint8_t  ctrl_now_energy_type; // 当前能量
//    uint8_t  ctrl_log_page;   // 控制历史记录页
//    uint8_t  ctrl_time_area; // 控制时间分段管理
//    time_control  ctrl_time[10];
//    uint8_t ctrl_PvType[6];
//    uint8_t ctrl_alarm_voice;
//
//    uint8_t nc[20];
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
			uint8_t mix_l : 8;					// 杂项控制低8位
			uint8_t mix_h : 8;					// 杂项控制高8位
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

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t GenSet_enable : 2;
            uint8_t SetTimeCtrl1 : 2;
            uint8_t SetTimeCtrl2 : 2;
            uint8_t SetTimeCtrl3 : 2;

            uint8_t ctrl_mode : 2;
            uint8_t Unuse_time_ctrl : 2;
            uint8_t revd : 4;
        };
    };
}GenSet_struct;

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t Source_Mode;
            
            uint8_t Self_Check_Cmd : 2;
            uint8_t DO_Level_Set   : 2;
            uint8_t revd : 4;
        };
    };
}AC_Input_Ctrl_struct;

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
    GenSet_struct GenSet; /*2246*/\
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
    uint16_t SetGridMaxCurrent_in;/* 最大电网输入电流（每相） */\
    uint16_t RvSettings;/* 房车场景设置 */\
    uint16_t AltDelays_ShutdownTime;/* Alt延迟关闭输出时间 */\
    uint16_t RvOnOff;/* 房车开关杂项 */\
    uint32_t RvBatteryCapacity;/* 房车电池容量 */\
    uint16_t BatterySettings;/* 房车电池类型设置 */\
    uint16_t FUNC_set;\
    uint16_t ems_ctrl;\
    uint16_t revd2;\
    uint16_t SetCtrlMeter;\
    uint16_t SetFeedbackMode;\
    uint16_t AC_Gen_Type;\
    uint16_t heat_pump_auto_set_Active_Power;/* 2282 热泵自动模式的馈电功率阈值 */\
    uint16_t heat_pump_auto_set_minimum_Runtime;/* 2283 热泵自动模式的最小运行时间 */\
    uint16_t heat_pump_auto_set_Delay_Start_Time;/* 2284 热泵自动模式的启动延时 */\
	uint16_t PvMultiPeak;   /* 2304 光伏多峰 */\
	uint16_t inv_meter_interaction; /* 2305 逆变联动电表 */\
	AC_Input_Ctrl_struct AC_Input_Ctrl; /* 2306 交流输入源设置 */\
    uint16_t grid_LN_phase_map_result;\
    uint16_t grid_CT_phase_map_result;\
    uint16_t inverter_LN_phase_map_result;\
    uint16_t inverter_CT_phase_map_result;\
    uint32_t P_off_grid_load_power[3];\
    uint16_t revd[10];


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
    uint16_t  nc5[9];                /* 保留*/

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

typedef struct {
	INV_SET00_STRUCT_COMMON_0x1A

} inv_set00_0x1A_struct_mini;

typedef struct {
	INV_SET01_STRUCT_COMMON_0x1B

} inv_set01_0x1B_struct_mini;

typedef struct {
	INV_SET02_STRUCT_COMMON_0x1C//新产品不使用

} inv_set02_0x1C_struct_mini;
typedef struct {
	INV_SET03_STRUCT_COMMON_0x1D

} inv_set03_0x1D_struct_mini;	




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

//typedef struct {
//    uint8_t             devId;         // 当前数据属于哪个节点
//    uint8_t             online;//1-设备在线；0-超时清零
//    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
///*
//
//并机排序之后才set，排地址时候清零
//	*/    
//    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；
//
//	
//    inv_announce_struct inv_announce;  // 逆变主机的广播信息
//    inv_base_struct     inv_base;      // inv单包主要信息
//    inv_line_struct     inv_line;      // inv能量线
//    inv_about_struct    inv_about;     // inv软件版本信息
//    inv_pv_struct       inv_pv;        // inv光伏信息
//    inv_grid_struct     inv_grid;      // inv电网信息
//    inv_load_struct     inv_load;      // inv负载信息
//    inv_data_struct     inv_data;      // inv逆变数据
//    inv_meter_struct    inv_meter;     // inv电表数据
//    inv_gen_struct      inv_gen;       // inv发电机数据
//    inv_set00_struct    inv_set00;     // inv设置区00-基础设置区
//    inv_set01_struct    inv_set01;     // inv设置区01-高级设置区
//    inv_set02_struct    inv_set02;     // inv设置区02-认证设置区,新产品废除
//    inv_set03_struct    inv_set03;     // inv设置区03-认证设置区
//    inv_log_struct      inv_log;       // inv日志
//    inv_all_energy_struct inv_all_energy;	// inv 15年能量信息
//    inv_energy_struct   inv_energy;		// inv 当年能量信息
//    inv_wifi_struct     inv_wifi;		// inv第三方wifi
//	wifi_report_struct 	wifi_report;	// 外置wifi上报到服务器的数据(来自底层逆变设备)
//	wifi_param_struct	wifi_param;		// 外置wifi下发到逆变器的数据(来自外部wifi模块)
//    auth_struct    	 	auth_param; 	//认证参数
//}inv_node_struct;

//typedef struct {
//    inv_set00_struct    inv_set00;  // inv设置区00
//    inv_set01_struct    inv_set01;  // inv设置区01
//    inv_set02_struct    inv_set02;  // inv设置区02
//    inv_set03_struct    inv_set03;  // inv设置区03
//    inv_wifi_struct     inv_wifi;   // inv第三方wifi
//    auth_struct    	 	auth_param; //认证参数，modbus 40000透传
//} bk_inv_node_set;



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

/* 告警量A-E 注: bit成员实际无效，直接从ARM获取 */
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
        uint8_t voltage;
        uint8_t country;
    };
}Machine_standard_struct;

typedef struct {
    uint16_t revd0; // 0
    Machine_standard_struct Machine_standard;//1
    uint16_t revd1; // 2
    iot_factory_base_struct iot_reg00_status; // 3 
    ems_ctrl_struct ems_ctrl;//4
    scene_struct Scene_Flag;
    iot_mode_struct     support_mode;   // 6
	uint8_t app_password[6];    // 7~9
    uint16_t revd2[5]; // 10~14
    uint16_t modbus_ver_iot; // 15
	uint16_t modbus_ver;    // 16  modbus协议版本
    uint16_t revd3[5]; // 17~20
}MOD_STRUCT_reg00000;

typedef union {
    uint16_t all;
    struct {
        uint8_t status : 4;
        uint8_t progress : 4;
        uint8_t fault_state : 4;
        uint8_t rsv : 4;
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
        uint16_t revd : 12;                      // bit4~bit15 预留
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
    struct {
        uint8_t DC_HUB : 1;         // bit0 DC-HUB在线（只要有一个在线就置1）
        uint8_t AC_HUB : 1;         // bit1 AC-HUB在线
        uint8_t GridMeter : 1;      // bit2 电网电表在线状态
        uint8_t LoadMeter : 1;      // bit3 负载电表在线状态
        uint8_t ATS : 1;            // bit4 ATS在线状态
        uint8_t remote_switch : 1;  // bit5 是否支持远程关机功能
    }bit;
}Online_Component_struct;

typedef union {
    uint16_t all;
    struct {
        uint8_t new_discovery_mode : 1;         // bit0 新设备发现模式
        uint8_t update             : 1;         // bit1 升级状态
        uint8_t child_lock         : 1;         // bit2 童锁开关状态（2012添加）
        uint8_t remote_switch      : 1;         // bit3 远程开关状态（2012添加）
        uint8_t wifi_mesh_discover : 1;         // bit4 WIFI-MESH网络新设备发现模式（2012添加）
        uint8_t iot_fault_status   : 1;         // bit5 IOT告警状态
        uint8_t pack_fault_status  : 1;         // bit6 PACK系统故障状态
    }bit;
}IOT_state_struct;

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t map_set;        //低字节：RV100使用场景（0-无效/未定义；1-房车；2-离网小木屋；3-游艇；）
            uint8_t revd;
        };
    };
}SceneConfigFlag_Struct;

// BMS自检结构体
typedef struct {
    union{
        uint16_t all;

        struct{
            uint16_t battery_high_low_temp : 1;       // 电池高低温
            uint16_t battery_over_under_voltage : 1;  // 电池过欠压
            uint16_t battery_cell_consistency : 1;    // 电池电芯一致性
            uint16_t battery_internal_resistance : 1; // 电池内阻检测
            uint16_t battery_self_discharge : 1;      // 电池自耗电检测
            uint16_t battery_not_used_three_months : 1; // 电池超过三个月未使用
            uint16_t battery_over_discharge_protection : 1; // 电池过放保护
            uint16_t battery_over_charge_protection : 1; // 电池过充保护
            uint16_t battery_redundant_safety_protection : 1; // 电池冗余安全保护
            uint16_t current_sensor_detection : 1;    // 电流传感器检测
            uint16_t voltage_sensor_detection : 1;    // 电压传感器检测
            uint16_t current_limiting_module_detection : 1; // 限流模块检测
            uint16_t battery_temperature_sensor : 1;  // 电池温度传感器
            uint16_t onboard_temperature_sensor : 1;  // 板载温度传感器
            uint16_t heater_detection : 1;            // 加热器检测
            uint16_t reserved : 1;                    // 保留位
        };
    };
} BMS_SelfCheck_t;

// ARM控制板自检结构体
typedef struct {
    union{
        uint16_t all;

        struct{
            uint16_t communication_abnormal_detection : 1; // 通讯异常检测
            uint16_t temperature_sensor_detection : 1;     // 温度传感器检测
            uint16_t current_sensor_detection : 1;         // 电流传感器检测
            uint16_t voltage_sensor_detection : 1;         // 电压传感器检测
            uint16_t eeprom_fault_detection : 1;           // EEPROM故障检测
            uint16_t fan_detection : 1;                    // 风扇检测
            uint16_t relay_detection : 1;                  // 继电器检测
            uint16_t rtc_detection : 1;                    // RTC检测
            uint16_t pv1_input_detection : 1;              // PV1输入检测
            uint16_t pv2_input_detection : 1;              // PV2输入检测
            uint16_t no_communication_battery_detection : 1; // 无通信电池检测
            uint16_t reserved : 5;                         // 保留位
        };
    };
} ARM_ControlBoard_SelfCheck_t;

// IOT模块自检结构体
typedef struct {
    union{
        uint16_t all;

        struct{
            uint16_t panel_connection_detection : 1; // Panel连接检测
            uint16_t hmi_connection_detection : 1;   // HMI连接检测
            uint16_t cloud_connection_communication_detection : 1; // 云连接通讯检测
            uint16_t exflash_detection : 1;          // ExFlash检测
            uint16_t rtc_detection : 1;              // RTC检测
            uint16_t networking_connection_detection : 1; // 组网连接检测
            uint16_t bluetooth_status_detection : 1; // 蓝牙状态检测
            uint16_t wifi_status_detection : 1;      // WiFi状态检测
            uint16_t reserved : 8;                   // 保留位
        };
    };
} IOT_Module_SelfCheck_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t status : 4;
        uint8_t progress : 4;
        uint8_t fault_state : 4;
        uint8_t rsv : 4;
    };
}AC_Input_Status_t;

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
    device_ctrl ctrl_status; // 124 开关控制状态16bit
	uint16_t GridandMachineSOC; // 125 并网并机SOC
    uint16_t    alarm[4];       	// 126~129 告警信息
    uint16_t revd2[3]; // 130~132
    uint16_t    fault[6];       	// 133~138 故障信息
    uint16_t revd3; // 139
    
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
    Online_Component_struct Parts_online;//171 配件在线状态
    uint16_t revd4;//172
    uint16_t revd5;//173
    IOT_state_struct IOT_state;//174 IOT状态
    SceneConfigFlag_Struct SceneConfigFlag;//175 配置标志
    uint32_t TotalEnergy_Car;// 176~177
    uint32_t Sleep_endurance;   // 178~179 休眠续航时间
    uint32_t PackTotalChgEnergy;// 180~181
    uint16_t CarTotalPower; //182 行车快充总功率
    uint32_t ChargingPileTotalPower; //183~184 充电桩总功率
    IOT_Module_SelfCheck_t IOT_TEST;//185 自检
    ARM_ControlBoard_SelfCheck_t INV_TEST;//186 自检
    BMS_SelfCheck_t PACK_TEST;//187 自检
    uint16_t inv_flag;//188
    uint16_t PvTotalChargingEnergyToday;//189 今日PV发电量
    uint16_t PvTotalChargingEnergyMonth;//190 当月PV发电量
    uint16_t Switch_memory_states;//191 开关类记忆状态
    uint32_t PvToGridEnergy;    // 192~193 PV馈电到电网能量
    uint32_t PvToBatEnergy; // 194~195 PV供电到电池能量
    uint32_t GridToLoadEnergy;  // 196~197 电网到负载能量
    uint32_t GridToBatEnergy;  // 198~199 电网到电池能量
    uint32_t BatToGridEnergy;   // 200~201 电池馈电到电网能量
    uint32_t BatToLoadEnergy;   // 202~203 电池到负载能量
    uint32_t PvToGridPower;    // 204~205 PV馈电到电网功率
    uint32_t PvToBatPower; // 206~207 PV供电到电池功率
    uint32_t GridToLoadPower;  // 208~209 电网到负载功率
    uint32_t GridToBatPower;  // 210~211 电网到电池功率
    uint32_t BatToLoadPower;   // 212~213 电池到负载功率
    AC_Input_Status_t AC_Input_Status; // 214 交流输入自检状态
    uint16_t revd6[20];// 215~234
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
//typedef struct {
//    uint8_t level;  // ota 处于哪个阶段,0：IOT发送给设备，1：设备发送给子设备-低字节
//    uint8_t where;  // 文件传输位置
//    uint8_t type;   // 文件类型 最高位置1表示正在升级
//}ota_group_status_struct2;

typedef struct {
    ota_cmd_struct1      ota_cmd;        // 700~705 ota 控制，该结构数据由上层应用下发到IoT
    uint16_t ota_mcu_count; // 706 升级MCU个数
    uint16_t revd1[13]; // 707~719
    ota_group_t ota_cmd_group; // 720
    ota_group_status_struct1 ota_group[16];
}MOD_STRUCT_reg00700;

typedef struct {
    uint16_t InvID; // 1100 逆变并机后所分配的ID编号
	
    char  InvType[12];   // 1101~1106 ASCII 机型
    uint64_t InvSN;      // 1107~1110 设备唯一识别码
    uint16_t  inv_power_rang; // 1111 逆变功率类型 1: 2 : 3:    (小功率，中功率，大功率)
    uint16_t  software_total; // 1112 系统中软件版本数量,相同的MCU只显示最低版本
    modbus_soft_version  soft[MD_SOFT_MAIN_CAP];  // 1113~1130 最多6个软件版本
    uint16_t line_event;  // 1131  00:能量线无效；01:能量线有效；PV能量线可接入逆变
    device_ctrl1 ctrl_status_L; //1132	逆变设备开关设置低16bit	按位表示
    device_ctrl1 ctrl_status_H; //1133	逆变设备开关设置高16bit	按位表示
    uint16_t alarm[4]; // 1134~1137 告警信息
    uint16_t revd2[3]; // 1138~1140
    
    uint16_t fault[6]; // 1141~1146 故障信息    
    uint16_t revd3; // 1147
	uint16_t WorkTimeNumber; // 1148
	uint16_t DevVoltageLable;// 1149
	uint16_t InvCanBusError; // 1150
	uint16_t revd5[3];   // 1151~1153
	uint16_t INVFirmwareVersionDifferences; // 1154
	uint16_t P_INV_chg_limit_L1;   // 1155
	uint16_t P_INV_chg_limit_L2;    // 1156
	uint16_t P_INV_chg_limit_L3; // 1157
	uint16_t P_INV_disg_limit_L1; // 1158
	uint16_t P_INV_disg_limit_L2; // 1159
	uint16_t P_INV_disg_limit_L3; // 1160
	
    /*1161~1169*/
	uint16_t machine_l1_ac_input_rated_current;
    uint16_t machine_l2_ac_input_rated_current;
    uint16_t machine_l3_ac_input_rated_current;
    uint16_t machine_l1_ac_output_rated_current;
    uint16_t machine_l2_ac_output_rated_current;
    uint16_t machine_l3_ac_output_rated_current;
    uint16_t grid_l1_ac_output_rated_current;
    uint16_t grid_l2_ac_output_rated_current;
    uint16_t grid_l3_ac_output_rated_current;
    
    /*1170~1173*/
    uint16_t grid_LN_phase_map_result;
    uint16_t grid_CT_phase_map_result;
    uint16_t inverter_LN_phase_map_result;
    uint16_t inverter_CT_phase_map_result;
    
    uint16_t revd4[8];//1174~1181
    
    modbus_soft_version  soft_back[MD_SOFT_BACK_CAP];  // 1182~1199 最多6个软件版本
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
    uint32_t DC_PV_Total_power; //1290~1291 DC_PV总功率
    uint16_t revd[8]; //1292~1299
}MOD_STRUCT_reg01200;

typedef struct {
    int16_t input_power;   // 有功功率
    uint16_t input_voltage; // 输入电压 0.1V
    int16_t input_current; // 输入电流 0.1A
    uint16_t input_papparent; // 视在功率
    uint16_t revd[2];
}grid_detail_struct1;

typedef struct {
    uint16_t freq;              // 1300 电网频率 0.1Hz
    int32_t total_chg_power;   // 1301~1302 电网总充电功率 1w；；windy 有符号，电网到设备为负，设备到电网为正
    uint32_t total_chg_energy;  // 1303~1304 0.1kwh 电网充电能量
    uint32_t total_fb_energy;   // 1305~1306 0.1kwh 电网馈电能量
    uint16_t revd[4];           //1307~1310
    uint16_t pf;//1311
    uint16_t  grid_phase_number;// 1312 grid相数
    grid_detail_struct1  grid_detail[3];   //1313~1330 
    uint32_t GridPower_U32[3];  // 1331~1336 电网有功功率
    uint32_t GridPapparent_U32[3];  // 1337~1342 电网视在功率
    uint16_t revd1[10]; //1343~1354
}MOD_STRUCT_reg01300;

typedef struct {
    uint16_t load_power;    // 1W
    uint16_t load_voltage;  // 0.1V
    uint16_t load_current;  // 0.1A
    uint16_t load_papparent;
    uint16_t load_preactive;
    uint16_t revd;
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
    uint16_t DCLoadTotalVoltage;//1412 DC负载总电压
    uint16_t DCLoadTotalCurrent;//1413 DC负载总电流
    uint16_t revd1[5];   //1414~1418
    uint16_t pf;//1419
    uint32_t total_ac_load_power; //1420~1421 w 总的AC负载功率
    uint32_t total_ac_load_energy;  //1422~1423 总的AC负载能量
    uint32_t ac_open_time;  //1424~1425
    uint16_t revd2[3];   //1426~1428
    uint16_t ac_phase_number;    //1429 ac输出并机相数
    ac_load_struct1 ac_load[3]; //1430~1447  ac负载信息,根据并机相数 
    uint16_t dc1_input_min_voltage;   //1448 直流1输入最低电压
    uint16_t dc1_input_max_voltage;   //1449 直流1输入最高电压
    uint16_t dc1_input_max_current;   //1450 直流1输入最大电流
    uint16_t dc1_input_max_power;     //1451 直流1输入最大功率
    uint16_t dc2_input_min_voltage;   //1452 直流2输入最低电压
    uint16_t dc2_input_max_voltage;   //1453 直流2输入最高电压
    uint16_t dc2_input_max_current;   //1454 直流2输入最大电流
    uint16_t dc2_input_max_power;     //1455 直流2输入最大功率
    uint32_t ACLoadPower_U32[3];  // 1456~1461 AC负载有功功率
    uint32_t ACLoadPapparent_U32[3];  // 1462~1467 AC负载视在功率
    uint16_t revd3[10]; //1468~1477
}MOD_STRUCT_reg01400;

typedef struct {
    uint16_t work_status;    // 逆变工作状态
    uint16_t power;         // 逆变功率 M139.3 uint16_
    uint16_t voltage;       // 逆变电压
    uint16_t current;       // 逆变电流 M139.3 uint16_
    uint16_t revd[3];
}inv_detail_struct1;

// typedef struct {
//     uint16_t freq;          // 1500 逆变频率
//     uint32_t total_energy;  // 1501~1502 逆变的总能量
//     uint16_t revd[5];   //1503~1507
//     uint16_t  phase_number;  // 1508 相位数量 最多3相
//     inv_detail_struct1  inv_detail[6]; //1509~1529 每相详细信息 最多3相
// }MOD_STRUCT_reg01500;

typedef struct {
    uint16_t freq;          // 1500 逆变频率
    uint32_t total_energy;  // 1501~1502 逆变的总能量
    uint16_t revd[5];   //1503~1507
    uint16_t  phase_number;  // 1508 相位数量 最多3相
    inv_detail_struct1  inv_detail[3]; //1509~1529 每相详细信息 最多3相
	uint16_t AmbientTemp;   // 1530
	uint16_t InvMaxTemp;    // 1531
	uint16_t PvDcdcMaxTemp; // 1532
	uint16_t Bms_current_in_VPP; // 1533 PCS计算的BMS充放电电流
	uint16_t MeterSynchronization; // 1534 设备电表间联动状态
    uint16_t revd1[10]; //1534~1543
}MOD_STRUCT_reg01500;

typedef struct {
    uint32_t total_energy;  //1600~1601 发电机发电总能量 0.1Kwh
    uint16_t power;     //1602 发电机功率 1w
    uint16_t voltage;   //1603 发电机电压 0.1V
    uint16_t current;   //1604 发电机电流 0.1A
    uint16_t revd[10];   //1605~1614
}MOD_STRUCT_reg01600;

typedef union {
	uint16_t all;
	struct {
		uint16_t rtc_set_from : 2;	// bit0/1： 设置RTC来源
		uint16_t online : 1;    	// bit2： 电表在线状态
		uint16_t revd : 13;			// bit3~15：预留
	};
} meter_status_u;

typedef struct {
    char dev_type[12];  //1700~1705 设备型号 
    uint64_t dev_sn;    //1706~1709 设备SN
    meter_status_u status;	//1710
    uint32_t sys_time;	//1711~1712
    uint32_t L1_V;		//1713~1714 A相电压
    uint32_t L2_V;		//1715~1716 B相电压
    uint32_t L3_V;		//1717~1718 C相电压
    uint32_t L1_C;		//1719~1720 A相电流
    uint32_t L2_C;		//1721~1722 B相电流
    uint32_t L3_C;		//1723~1724 C相电流
    uint32_t L1_Active_Power;	//1725~1726 A相有功功率
    uint32_t L2_Active_Power;	//1727~1728 B相有功功率
    uint32_t L3_Active_Power;	//1729~1730 C相有功功率
    uint32_t L1_Reactive_Power;	//1731~1732 A相无功功率
    uint32_t L2_Reactive_Power;	//1733~1734 B相无功功率
    uint32_t L3_Reactive_Power;	//1735~1736 C相无功功率
    uint32_t L1_Apparent_Power;	//1737~1738 A相视在功率
    uint32_t L2_Apparent_Power;	//1739~1740 B相视在功率
    uint32_t L3_Apparent_Power;	//1741~1742 C相视在功率
    uint32_t L1_Power_Factor;	//1743~1744 A相功率因数
    uint32_t L2_Power_Factor;	//1745~1746 B相功率因数
    uint32_t L3_Power_Factor;	//1747~1748 B相功率因数
    uint32_t Average_V_LN;		//1749~1750
    uint32_t Average_C_LN;		//1751~1752
    uint32_t Unbalance_C_LN;	//1753~1754
    uint32_t Total_C_LN;		//1755~1756
    uint32_t Total_Active_Power;//1757~1758
    uint32_t Total_Reactive_Power;//1759~1760
    uint32_t Total_Apparent_Power;//1761~1762
    uint32_t Total_Power_Factor;//1763~1764
    uint32_t Frequency;			//1765~1766
    uint32_t Total_Import_Active_Energy;			//1767~1768
    uint32_t Total_Export_Active_Energy;			//1769~1770
    uint32_t Total_Active_Energy;		//1771~1772 Import&Export
    uint32_t L1_Import_Active_Energy;	//1773~1774
    uint32_t L1_Export_Active_Energy;	//1775~1776
    uint32_t L1_Active_Energy;			//1777~1778 Import&Export
    uint32_t L2_Import_Active_Energy;	//1779~1780
    uint32_t L2_Export_Active_Energy;	//1781~1782
    uint32_t L2_Active_Energy;			//1783~1784 Import&Export
    uint32_t L3_Import_Active_Energy;	//1785~1786
    uint32_t L3_Export_Active_Energy;	//1787~1788
    uint32_t L3_Active_Energy;			//1789~1790 Import&Export
    uint16_t rvs1[20];
	uint32_t last_online_time;	// 1811~1812 电表最后在线时间
	uint32_t fault;     // 1813~1814 故障告警
	uint32_t Meter_type_support;    // 1815~1816 当前逆变器支持的电表型号枚举
	uint16_t rvs2[10];
}MOD_STRUCT_reg01700;

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
    _section_time1 start; // 开启时间 (时:分)//M128
    _section_time1 end;   // 结束时间 (时:分)
}time_control1;


typedef union {
	uint16_t all;
	struct {
		uint16_t Custom_mode_enable     : 2;    // bit0/1 表示机器是否支持自定义功能模式
		uint16_t chg_mode_set_by_app    : 2;    // bit2/3 表示机器是否允许APP进行充电模式设置       
		uint16_t Child_lock             : 2;    // bit5/4:童锁开关（APP设置后，设备屏幕不可设置）
		uint16_t revd                   : 2;    // bit7/6：预留

        uint16_t Smart_switch           : 8;    // 高字节: 设备端物理开关对应功能(见右表）；APP侧为灵动开关
	};
} Custom_mode_set_struct;

typedef struct{
    union{
        uint16_t all;
        struct{
            uint8_t Rv_SOC_Ownership_enable     : 2;   //bit0/1:最低SOC保有量使能
            uint8_t revd                        : 6;

            uint8_t Rv_SOC_Ownership_set;              //高字节：最低SOC保有量（1-100%）
        };
    };
}Rv_SOC_Ownership_struct;

typedef struct {
	uint16_t SetInvSelect;//2000
	uint8_t mon;//
	uint8_t year;// 2001年为基准
	uint8_t hour;
	uint8_t day;//2002
	uint8_t sec;
	uint8_t min;//2003	
    time_res_struct  SetTimeZone;//2004 时区
    uint16_t  work_mode;     // 2005  00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网
    ctrl_event  ctrl;       // 2006 一次性控制事件,
    uint16_t  ctrl_led;       // 2007 控制LED
    ctrl_meter_struct  ctrl_meter;     // 2008 控制电表 (未用)
    uint16_t  ctrl_pv;        // 2009 控制PV
    uint16_t  ctrl_inv;     // 2010 控制逆变
    uint16_t  ctrl_ac;  // 2011 控制ac
    uint16_t  ctrl_dc;  // 2012 控制dc
    uint16_t  ctrl_poweron;  // 2013 控制电源(房车复用：1：关机；2：休眠关、开机（2012添加）)
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
    uint16_t ctrl_PvType[6];//2060~2065（房车复用2061：0--PV,1--CAR,2--Adpater,3--other，4-行车快充（RV100新增））
    uint16_t ctrl_alarm_voice;//2066
    uint16_t ctrl_lcd_active_time;//2067 LCD显示屏显示时间 1：15s；2: 30s；3: 1分；4：5分；5：常亮
    uint16_t P_inv_active_target_L1;//2068
    uint16_t P_inv_active_target_L2;//2069
    uint16_t P_inv_active_target_L3;//2070
    uint16_t Rated_AC_Power;//2071
    Custom_mode_set_struct Custom_mode_set;//2072
    Remote_set_struct Remote_set;//2073
    uint16_t Remote_set_soc_limit;//2074
    Rv_SOC_Ownership_struct Rv_SOC_Ownership;//2075 房车SOC保有量设置
    uint16_t LevelSwitch;//2076 童锁等级
    uint16_t sleep_remain_time;//2077 休眠剩余时间
    led_set_struct LED_SET;      // 2078
    uint16_t Remote_set_power;   // 2079
    uint16_t Pack_Set_Show;      // 2080
    uint16_t INV_Set_Show;       // 2081
    uint16_t DCDC_Set_Show;      // 2082
    uint16_t SOC_H;              // 2083
    uint16_t PV_SET;             // 2084
    uint16_t DC_output;//2085 DC输出
    uint16_t Regulatory_set;//2086 合规设置
    uint16_t Cycle_capacity;//2087 循环容量
    uint16_t Cycle_max_capacity;//2088 最大循环容量
    uint16_t Effective_time_ym;//2089 生效时间年月
    uint16_t Effective_time_dh;//2090 生效时间日时
    uint16_t Effective_time_ms;//2091 生效时间分秒
    uint16_t ECO_status;//2092
    uint16_t set_AC_branch; // 2093
    uint16_t set_DC_branch; // 2094
    uint16_t DC_SET;        // 2095
    uint16_t AC_button_sign;// 2096
    uint16_t revd[10];//2097~2106
}MOD_STRUCT_reg02000;

// typedef struct {
//     uint16_t SetInvSelect;//2000
//     uint8_t mon;//
//     uint8_t year;// 2001年为基准
//     uint8_t hour;
//     uint8_t day;//2002
//     uint8_t sec;
//     uint8_t min;//2003  
//     time_res_struct  res;//2004 时区
//     uint16_t  work_mode;     // 2005  00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网
//     ctrl_event  ctrl;       // 2006 一次性控制事件,
//     uint16_t  ctrl_led;       // 2007 控制LED
//     ctrl_meter_struct  ctrl_meter;     // 2008 控制电表 (未用)
//     uint16_t  ctrl_pv;        // 2009 控制PV
//     uint16_t  ctrl_inv;     // 2010 控制逆变
//     uint16_t  ctrl_ac;  // 2011 控制ac
//     uint16_t  ctrl_dc;  // 2012 控制dc
//     uint16_t  ctrl_poweron;  // 2013 控制电源
//     uint16_t  ctrl_dc_eco;       // 2014 控制dc eco模式0：关闭，1：开启
//     uint16_t  ctrl_dc_eco_time;  // 2015 控制dc eco模式空闲时间
//     uint16_t eco_dc_power_value;    // 2016 dc eco模式触发运行功率
//     uint16_t  ctrl_ac_eco;       // 2017 控制ac eco模式 0：关闭，1：开启
//     uint16_t  ctrl_ac_eco_time; // 2018 控制ac eco模式空闲时间
//     uint16_t eco_ac_power_value; // 2019 ac eco模式触发运行功率
//     uint16_t  ctrl_chg_mode; // 2020 控制充电模式
//     uint16_t  ctrl_super_power; // 2021 控制超级输出模式
//     uint16_t  ctrl_low_cap_pct;  // 2022 系统低电量百分比
//     uint16_t  ctrl_high_cap_pct; // 2023 系统满电量百分比
//     uint16_t  ctrl_inv_mode; // 2024 单火线，双火线，并机，并机双火线
//     uint16_t  ctrl_dev_id; // 2025 给逆变设置编号
//     uint16_t  ctrl_all_energy_type; // 2026 历史能量
//     uint16_t  ctrl_now_energy_type; // 2027 当前能量
//     uint16_t  ctrl_log_page;   // 2028 控制历史记录页
//     uint16_t  ctrl_time_area; // 2029 控制时间分段管理
//     time_control1  ctrl_time[10];//2030~2059
//     uint16_t ctrl_PvType[6];//2060~2065
//     uint16_t ctrl_alarm_voice;//2066
//     uint16_t ctrl_lcd_active_time;//2067

//     uint16_t revd[17];//2068~2084
// }MOD_STRUCT_reg02000;

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
			uint8_t mix_l : 8;					// 杂项控制低8位
			uint8_t mix_h : 8;					// 杂项控制高8位
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

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t map_set;        //低字节：RV100使用场景（0-无效/未定义；1-房车；2-离网小木屋；3-游艇；）
            uint8_t revd;    
        };
    };
}RvSettings_struct;

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t smart_mode   : 2;   //bit0/1:智能模式
            uint8_t work_mode    : 2;   //bit2/3:工作开关
            uint8_t LN_mode      : 2;   //bit5/4:零地短接模式
            uint8_t voltage      : 2;   //bit7/6:电压平台选择

            uint8_t revd2;
        };
    };
}RvOnOff_struct;

typedef struct{
    union{
        uint16_t all;

        struct{
            uint8_t revd;
            uint8_t Voltage_set;    //高字节：电压设置值（bit0/1为关的时候可以设置电压值） bit15~8：1：12V; 2: 24V
        };
    };
}DCHUB_set_struct;

typedef struct {
    uint8_t password[8]; //2200~2203 设置区密码
    uint16_t revd1[2];  //2204~2205
    uint16_t ctrl_reset_factory; //2206 恢复出厂
    uint16_t ctrl_grid;      //2207 控制电网
    uint16_t ctrl_feedback;  //2208 控制馈电
    uint16_t ctrl_output_inv_volt;  //2209 输出电压控制
    uint16_t ctrl_output_inv_freq;   //2210 输出电压频率
    uint16_t ctrl_chg_max_volt; //2211 最大充电电压(房车复用12.0-13.5V    24.0V-27.0V  单位0.1V)
    uint16_t ctrl_pv_max_curr; //2212 最大PV充电电流(房车复用（10A-50A  单位1A）)
    uint16_t ctrl_grid_max_power;   //2213 电网最大功率
    uint16_t ctrl_grid_max_curr; //2214 电网最大电流(房车复用（AC/Grid最大输入电流，1~50A）)
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
	uint16_t P_inv_active_target_L1_U16; //2234
	uint16_t P_inv_reactive_target_L1_U16;//2235
	uint16_t P_inv_active_target_L2_U16; //2236
	uint16_t P_inv_reactive_target_L2_U16;//2237
	uint16_t P_inv_active_target_L3_U16; //2238
	uint16_t P_inv_reactive_target_L3_U16;//2239
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
    DCHUB_set_struct DCHUB_set;//2271 DCHUB设置电压_AC380设置(房车复用)
    uint16_t SetGridMaxCurrent_in;//2272 最大电网输入电流（每相）
    uint16_t FUNC_set;//2273 开关设置
    RvSettings_struct RvSettings;//2274 房车场景设置
    uint16_t AltDelays_ShutdownTime;//2275 Alt延迟关闭输出时间
    RvOnOff_struct RvOnOff;//2276 房车开关杂项
    uint32_t RvBatteryCapacity;//2277~2278  房车电池容量(0.1-2400.0Ah  单位0.1Ah，仅在电池类型5、6才显示)
    uint16_t BatterySettings;//2279 房车电池类型设置
    uint32_t heat_pump_Enable;//2280~2281 热泵使能设置
    uint16_t heat_pump_auto_set_Active_Power;//2282 热泵自动模式的馈电功率阈值
    uint16_t heat_pump_auto_set_minimum_Runtime;//2283 热泵自动模式的最小运行时间
    uint16_t heat_pump_auto_set_Delay_Start_Time;//2284 热泵自动模式的启动延时
	int32_t P_inv_active_target_L1; //2285~2286 L1相逆变器有功目标功率 (有符号)
	int32_t P_inv_active_target_L2; //2287~2288 L2相逆变器有功目标功率 (有符号)
	int32_t P_inv_active_target_L3; //2289~2290 L3相逆变器有功目标功率 (有符号)
	int32_t P_inv_reactive_target_L1;//2291~2292 L1相逆变器无功目标功率 (有符号)
	int32_t P_inv_reactive_target_L2;//2293~2294 L2相逆变器无功目标功率 (有符号)
	int32_t P_inv_reactive_target_L3;//2295~2296 L3相逆变器无功目标功率 (有符号)
	int32_t P_inv_apparent_target_L1;//2297~2298 L1相逆变器视在目标功率 (有符号)
	int32_t P_inv_apparent_target_L2;//2299~2300 L2相逆变器视在目标功率 (有符号)
	int32_t P_inv_apparent_target_L3;//2301~2302 L3相逆变器视在目标功率 (有符号)
	uint16_t SetFeedbackMode;// 2303 电网馈电模式配置（兼容户用储能+阳台光伏）
	uint16_t PvMultiPeak;   // 2304 光伏多峰
	uint16_t inv_meter_interaction; // 2305 逆变联动电表
	AC_Input_Ctrl_struct AC_Input_Ctrl; // 2306 交流输入源设置 
	uint16_t grid_LN_phase_swap;    // 2307 修正grid口功率相线口接线序号
	uint16_t grid_CT_phase_swap;    // 2308 修正grid口CT接线方向和相线
	uint16_t inverter_LN_phase_swap;    // 2309 修正inverter口功率相线口接线序号
	uint16_t inverter_CT_phase_swap;    // 2310 修正inverter口CT接线方向和相线
    uint32_t P_off_grid_load_power[3];  // 2311~2316 离网口负载目标功率
    uint16_t revd[10];// 2317~2326
}MOD_STRUCT_reg02200;




typedef struct {
    uint8_t password[8]; //2200~2203 设置区密码
    uint16_t revd1[2];  //2204~2205
    uint16_t ctrl_reset_factory; //2206 恢复出厂
#if 0//    
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
    uint16_t    ChargingPile_SET;  //2243 充电桩型号
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
    remote_set_struct remote_set;// 2273 远程设置
    uint16_t revd[8];//2274~2281

#endif
}MOD_STRUCT_reg02200_MINI;//仅仅为借用恢复默认变量


typedef struct {
    /*2400~2409*/
    uint16_t SetGridEnable;          // 高级功能使能
    uint16_t SetGridCountry;         // 电网认证使用地区
    uint16_t SetGridUV1Value;        // 电网欠压LEVEL1保护值
    uint16_t SetGridUV1Time;         // 电网欠压LEVEL1触发时间
    uint16_t SetGridUV2Value;        // 电网欠压LEVEL2保护值
    uint16_t SetGridUV2Time;         // 电网欠压LEVEL2触发时间
	uint16_t pf;					 // 功率因数
	uint16_t set_region;			 // 地区
	uint16_t mode_enable;			 // 模式使能
	uint16_t power_rate_limit;		 // 每分钟功率变化比率
	/*2410~2417*/
    uint16_t SetGridAVGOVValue;      // 电网均值过压
    uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
    uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
    uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
    uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
	uint16_t power_reactive_to_active_ratio; // 无功占额定有功比例
	uint16_t vw1;					 // Volt–watt响应模式的参数,Vw1<Vw2
	uint16_t vw2;					 // Volt–watt响应模式的参数,Vw1<Vw2
	/*2418*/
    uint16_t revd1[1];
    /*2419~2430*/
    uint16_t SetGridUF1Value;        // 电网欠频LEVEL1保护值
    uint16_t SetGridUF1Time;         // 电网欠频LEVEL1保护时间
    uint16_t SetGridUF2Value;        // 电网欠频LEVEL2保护值
    uint16_t SetGridUF2Time;         // 电网欠频LEVEL2保护时间
	uint16_t vv1;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	uint16_t vv2;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	uint16_t vv3;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	uint16_t vv4;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
    uint16_t SetGridOF1Value;        // 电网过频LEVEL1保护值
    uint16_t SetGridOF1Time;         // 电网过频LEVEL1保护时间
    uint16_t SetGridOF2Value;        // 电网过频LEVEL2保护值
    uint16_t SetGridOF2Time;         // 电网过频LEVEL2保护时间
    /*2431~2434*/
    uint16_t revd2[4];
    /*2435~2440*/
    uint16_t SetGridVoltMinValue;    // 电网重连电压下限值
    uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
    uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
    uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
    uint16_t SetGridRetryTime;       // 电网异常重连时间
	uint16_t SetDefaultValue;        // 设置默认值
    /*2441~2444*/
    uint16_t revd3[4];
    /*2445~2451*/
    uint16_t setMaxVA;  // 最大连续视在功率输出能力
    uint16_t setMaxVar; // 最大连续无功功率接收能力（正方向）
    uint16_t setMaxVarNeg;  // 最大连续无功功率接收能力（负方向）
    uint16_t setMaxW;   // 最大连续有功输出功率能力
    uint16_t setMaxChargeRateW; // 最大充电功率
    uint16_t setMaxDischargeRateW;  // 最大放电功率
    uint16_t opModControl;
    uint16_t revd_end[10];                // 保留
}MOD_STRUCT_reg02400;

typedef  inv_set02_struct MOD_STRUCT_reg02300;// 


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
}MOD_STRUCT_reg03000;

typedef struct {
    uint16_t year;
    uint32_t energy;
}_year_energy1;

typedef struct {
    uint16_t energy_type;//3500
    uint32_t total_energy;//3501~3502
    _year_energy1 year_data[15];//3503~3547
}MOD_STRUCT_reg03500;

typedef struct {
    uint16_t energy_type;//3600
    uint16_t year;//3601
    uint32_t total_energy;//3602~3603
    uint32_t mouth_energy   [12]; //3604~3627 0.1Kwh
    uint16_t day_enengy[31]; //3628~3658 0.1Kwh
}MOD_STRUCT_reg03600;


typedef union {
    uint16_t all;
    struct {
        uint16_t day_energy : 15;    // 能量的当日变化量(23:59:59清零)
        uint16_t valid      : 1;     // 能量有效标志
    }bit;
}day_energy_struct;

typedef struct {
    day_energy_struct PackTotalDsgEnergy;       // 3700 Pack总放电能量的当日变化量
    day_energy_struct PvTotalChargingEnergy;    // 3701 PV总的充电能量的当日变化量
    day_energy_struct PVToACloadEnergy;         // 3702 PV到AC负载能量的当日变化量
    day_energy_struct GridTotalChargingEnergy;  // 3703 电网充电能量的当日变化量
    day_energy_struct GridFeedbackEnergy;       // 3704 馈电电网能量的当日变化量
    day_energy_struct DCLoadTotalEnergy;        // 3705 DC负载总能量的当日变化量
    day_energy_struct ACLoadTotalEnergy;        // 3706 AC负载总能量的当日变化量
    day_energy_struct InvTotalEnergy;           // 3707 逆变总能量的当日变化量
    day_energy_struct GenTotalEnergy;           // 3708 发电机总能量的当日变化量
    day_energy_struct TotalEnergy_Car;          // 3709 行车充电总能量的当日变化量
    day_energy_struct PackTotalChargeEnergy;    // 3710 Pack总充电能量的当日变化量
    
    uint16_t  revd[10];                         // 保留
}MOD_STRUCT_reg03700;

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


//typedef union {
//    uint16_t all;
//    struct {
//        uint8_t chg_enable : 1;     // 1：允许充电；0：禁止
//        uint8_t dsg_enable : 1;     // 1：允许放电；0：禁止
//        uint8_t chg_now : 1;        // 1：紧急补电
//        uint8_t chg_protect : 1;    // 1：充电进入保护
//        uint8_t dgs_protect : 1;    // 1：放电进入保护
//        uint8_t fault : 1;          // 1：系统故障
//		uint8_t battery_heat : 1;   // 1：电池加热
//		uint8_t battery_balance : 1;// 1：电池均衡
//    }bit;
//}status1_byte1;
//
//typedef union {
//    uint16_t all;
//    struct {
//        uint8_t chg_alarm : 1; // 1：充电告警；0：消除
//        uint8_t dsg_alarm : 1; // 1：放电告警；0：消除
//    }bit;
//}status1_byte2;
//
//
//typedef struct {
//    uint16_t is_high_volt;  //6000 所属电压平台
//    uint16_t pack_cnt;   //6001 pack并机数量
//    uint16_t online;    //6002 最大并机16台，响应的位置1表示，在线 - 通讯并机，bit0~15依次表示不同bat状态
//    uint16_t total_voltage; //6003 0.1V  总电压
//    uint16_t total_current; //6004 0.1A  总电流
//    uint16_t soc;            //6005 1% 系统电流
//    uint16_t soh;        //6006 0-100 1%
//    uint16_t avg_temp;       //6007 平均温度
//    uint16_t work_status;    //6008 工作状态
//    uint16_t chg_status;     //6009 充电状态
//    uint16_t max_chg_voltage; //6010 最大充电电压
//    uint16_t max_chg_current; //6011 为0时，不能充电
//    uint16_t max_dsg_current; //6012 为0时，不能放电
//    status1_byte1 status1;     //6013 状态标记位1
//    status1_byte2 status2;     //6014 状态标记位2
//    uint16_t revd1;//6015 
//    uint16_t cap_online; //6016 最大并机16台，响应的位置1表示，主mos输出打开 - 容量并机
//    uint16_t chg_full_time;  //6017 预计放空时间
//    uint16_t dsg_empty_time; //6018 预计充满时间
//    uint32_t total_chg_energy;//6019~6020
//    uint32_t total_dsg_energy;//6021~6022
//	uint16_t TotalCurrent_bias;//6023
//	uint16_t revd3;//6024
//    uint16_t all_pack_control;//6025
//    uint16_t buckle_status;//6026
//    uint16_t merge_status;//6027
//    uint16_t heat_status;//6028
//    uint16_t all_pack_alarm1;//6029
//    uint16_t all_pack_alarm2;//6030
//    uint16_t BCU_State;//6031
//    uint16_t revd4;//6032
//}MOD_STRUCT_reg06000;

//
//typedef struct {
//    /*6100~6110*/
//    uint16_t pack_id;//6100
//    char    type_ascii[12];//6101~6106
//    uint64_t sn_code;//6107~6110
//    /*6111~6125*/
//    uint16_t total_voltage; // 0.1V  单pack电压
//    uint16_t total_current; // 0.1A  单pack电流
//    uint16_t  soc;           // 单pack soc
//    uint16_t  soh;           // 单pack soh
//    uint16_t  avg_temp;      // 平均温度
//    uint16_t min_cell_voltage; // 最小电芯电压
//    uint16_t max_cell_voltage; // 最大电芯电压
//    uint16_t  min_cell_index;    // 最小电芯位置
//    uint16_t  max_cell_index;    // 最大电芯位置
//    uint16_t  min_temp_value;    // 最小温度值
//    uint16_t  max_temp_value;    // 最大温度值
//    uint16_t  min_temp_index;    // 最小温度位置
//    uint16_t  max_temp_index;    // 最大温度位置
//    uint16_t  work_status;       // 工作状态
//    uint16_t  chg_status;        // 充电状态
//    /*6126*/
//    uint16_t revd1[1];
//    /*6127*/
//    uint16_t pack_online;
//    /*6128~6130*/
//    uint16_t relay;              // 继电器状态：主继电器Bit0-Bit1、预充继电器Bit2-Bit3、加热继电器Bit4-Bit5.
//    uint16_t pack_cap_online;     // pack 容量并机状态
//    uint16_t pack_canbus_error;
//    /*6131~6143*/
//    uint32_t capacity; // 0.001Ah 电池包容量
//    uint32_t total_chg_energy; // 1Wh 电池累计充电能量
//    uint32_t total_dsg_energy; // 1Wh 电池累计放电能量
//    uint32_t once_chg_energy; // 1Wh 电池当前充电能量
//    uint32_t once_dsg_energy; // 1Wh 电池当前放电能量
//    uint16_t sir_value;     // 0.1kR 绝缘包内阻
//    uint16_t inner_value; // 1mR 电池包内阻
//    uint16_t cycle_count; // 循环次数
//    /*6144~6149*/
//    uint32_t protect;         // pack 保护
//    uint16_t SysErr[3];           // pack 故障
//    uint16_t alarm1;           // pack 告警
//    /*6150~6153*/
//    uint16_t chg_full_time;     // 预计充满时间
//    uint16_t dsg_empty_time;    // 预计放空时间
//    uint16_t pack_total_cell;   // 电芯总数
//    uint16_t  pack_total_ntc;    // NTC总数
//    /*6154~6156*/
//    uint16_t PackBMUCnt;
//    uint16_t BMUFaultOrder;
//    uint16_t revd2;
//    /*6157~6166*/
//    uint16_t pack_type;
//    uint16_t pack_outsum_voltage;
//    uint16_t pack_control;
//	uint16_t allow_max_chg_current;		// 允许最大充电电流
//	uint16_t allow_max_dsg_current;		// 允许最大放电电流
//	uint16_t pack_mos_status;
//    uint16_t pack_single_status;
//    uint32_t alarm2;
//    uint16_t pack_event;
//    /*6167*/
//    uint16_t revd3[1];
//    /*6168~6171*/
//    uint16_t iaging;
//    uint16_t revd4[3];
//    /*6172~6173*/
//    uint16_t pack_ver_diff;
//    uint16_t software_total; // 软件版本总数量（相同版本只显示最低版本号）
//    /*6174~6203*/
//    modbus_soft_version soft[10]; // 10组软件版本信息
//}MOD_STRUCT_reg06100;



// AC380假设 N == 16(最大)
//typedef struct {
//    uint16_t PackCellCnt;                      //6300          Pack中总的电芯数量
//    uint16_t PackNTCCnt;                        //6301          Pack中总的NTC数控
//    uint16_t CellVoltage[16];                       //6302~6317     电芯电压1 ~ 电芯电压16
//    uint16_t NTCGroup[4];                          //6318~6335     NTC1-NTC2 ~ NTC15-NTC16
//}MOD_STRUCT_reg06300;


/**
 * @brief 0x55
 *
 */
//typedef struct {
//    uint16_t SetPackID;   //7000 用于选择Pack
//    uint16_t pack_heat_enable;  //7001  设置Pv加热选项开关
//    uint16_t ctr_heat_enable;    //7002 电池高电量电网自动加热
//    uint16_t unlock_failed_flags;//7003 设置PACK因故障锁死后解锁
//    uint16_t max_parallel_nums;//7004 每套电池包的最大支持数量
//
//}MOD_STRUCT_reg07000;
//typedef union {
//    uint16_t all;
//    struct {
//        uint8_t BMU_Type1 : 8;	
//        uint8_t BMU_Type2 : 8;	
//    }bit;
//}BMU_Type_t;


//// AC380假设 N == 18(最大)
//typedef struct {
//    uint64_t BMU_SN;                         //7200~7271     7200 ~ 7203+(N-1)*4  BMU SN码
//    uint32_t BMU_Detail;                     //7272~7307     7204+(N-1)*4+(N-1)*2 ~ 7205+(N-1)*4+(N-1)*2  BMU 故障和保护等信息
//    uint16_t Single_Cell_and_Temp_Count;     //7308~7335     7206+(N-1)*4+(N-1)*2+(N-1)*2 ~ 7207+(N-1)*4+(N-1)*2+(N-1)*2 单个BMU中电芯数量及NTC数量
//    BMU_Type_t BMU_Type;                      //7336~7344     7208+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2 ~ 7209+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2  BMU类型 //uint16_t
//    uint32_t BMU_Version;                    //7345~7380     7210+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2+(N-1)*2 ~ 7211+(N-1)*4+(N-1)*2+(N-1)*2+(N-1)*2+(N-1)*2  BMU版本号
//}MOD_STRUCT_reg07200;

typedef struct {
	union {
		uint16_t all;

		struct {
		uint8_t CHARGER_direction : 2;//bit0: 车流向CHARGER 1(DC1)  bit1: CHARGER 1流向储能产品(DC3)
        uint8_t DC1_EN : 2;//0-无；1-进入DCDC模块；2-从DCDC模块输出
        uint8_t DC2_EN : 2;
        uint8_t DC3_EN : 2;
        uint8_t DC4_EN : 2;
        uint8_t DC5_EN : 2;
        uint8_t DC6_EN : 2;
        uint8_t revd : 2;   // 预留
		};
	};
}line_sate_struct;//DCDC模块通用 信息区

typedef struct{
	union
	{
		uint16_t all;

		struct{
			uint8_t DCDC_work_state : 1;
			uint8_t DCDC_module_fault : 1;
			uint8_t PV_online : 1;
			uint8_t work_model : 4;
			uint8_t res : 1;
       		uint8_t res1 : 8;
			
			};
	};
}dev_info_struct;

typedef struct {
	union {
		uint16_t all;

		struct {
		uint8_t DC_sw : 2;//bit0: ON  bit1: OFF
        uint8_t On_Sound_En : 2;//0-无；1-进入DCDC模块；2-从DCDC模块输出
        uint8_t FctRst_en : 2;
        uint8_t Self_Adaption : 2;
		uint8_t PV_model : 2;   // 
        uint8_t revd : 6;   // 预留
		};
	};
}DCDC_set_struct;//DCDC模块通用

typedef union {
    uint16_t all;
    struct {
        uint8_t spi_flash_fault : 1; // SPI flash故障
        uint8_t eeprom_fault : 1; // EEPROM故障
        uint8_t sub_1ghz_fault : 1;  // Sub 1GHz(CC1312)通信超时
        uint8_t _4g_fault : 1; // 4G模块通信超时
        uint8_t meter_fault : 1; // meter(BL6552)通信超时
        uint8_t arm_fault : 1; // ARM(IOT板内)通信超时
        uint8_t rtc_fault : 1; // RTC(IOT板内)通信故障
        
        uint16_t revd : 9; //
    }bit;
}iot_fault_struct;

typedef union {
    uint16_t all;
    struct {
        uint8_t ext_wifi_enable_num; // 低字节：当前设备支持的额外WIFI设置数 （0：无额外新增，1~N：额外支持N个）
        uint8_t curr_sta_index;      // 高字节：当前连接WIFI序号（0：无效，1：STA1，2：STA2，3：STA3）
    }bit;
}iot_wifi_mult_sta_flag_struct;

/**
 * @brief Type = 0x1 ,iot
 *
 windy 20240814:168 len
 */

#define	IOT_INFO_STRUCT_COMMON_REG_11000_0x1 \
	    char 	iot_type[12];/*11000~11005*/\
		uint64_t iot_sn;   /*11006~11009 sn码*/\
		uint64_t safe_code; /*11010~11013 安全码*/\
		uint32_t software_ver; /* 软件版本号*/\
		uint16_t revd1[2];\
		link_status   link;    /* IOT功能连接状态*/\
		uint8_t sta_ipv4[4];\
		uint8_t sta_gw[4];\
		uint8_t sta_mask[4];\
		uint16_t sta_rssi; \
		uint8_t sta_mac[6];\
		uint8_t ble_mac[6];\
		uint16_t mobile_rssi; \
        uint8_t MqttConnectMode;\
        uint8_t revd2;\
        iot_fault_struct iot_fault;\
        uint16_t revd3[3];\
		uint16_t can_bus_error; /* 11039 IOT CAN总线错误*/\
		uint16_t wifi_disconnect_reasion; /* 11040 WIFI断开原因*/\
		uint8_t IP_4G[4]; /* 11041~11042*/\
		uint8_t IP_ETH[4]; /* 11043~11044*/\
		uint32_t time_lastreport;/*最后联网（设备与服务器通讯）时间 2	基于1970年初始日期的秒计数器； R  11045~11046*/\
		uint8_t signal_4G;/*信号强度	1	L8bit:信号强度（0-31或者99）	 11047*/\
		uint8_t SIM_ready;/*  */\
		uint32_t GPS_X;/*uint16_t	11048~11049*/\
		uint32_t GPS_Y;/*uint16_t	11050~11051*/\
		uint8_t IMEI[20];/*IMEI号码(移动设备识别号码) 10	15~17位数字,ASCII	R*/\
		uint8_t IMSI[20];/*IMSI号码(移动用户识别号码) 10	15位数字,ASCII	R*/\
		uint8_t ICCID[20];/*ICCID号码(集成电路/SIM卡识别号码)	10	20个字符组成（19位数字+最后1位英文字母）,ASCII */\
		uint16_t Bind_SN;/*绑定群组代号*/\
        uint16_t WIFI_Mesh_ID[3];/*11083~11085*/\
        uint8_t eth_mac[6];\
        uint16_t revd4[11];\
        uint16_t Power_Off_Nums;\
        uint32_t Save_Money_Nums;\
        iot_smart_ctrl_struct iot_smart_control;\
        uint16_t DI_State;\
        iot_wifi_mult_sta_flag_struct wifi_mult_sta_flag;\
        char wifi_station_ssid_now[32];\
        uint8_t sta_dns1[4];\
        uint8_t sta_dns2[4];\
        char Connected_BLE_Server_SN[32];\
        uint16_t revd6[10];

#define	DCDC_INFO_STRUCT_COMMON_REG_15500 \
    uint16_t revd[5];\
    line_sate_struct  line_sate;\
    uint16_t DC1_fault;/*DC1故障*/\
    uint16_t DC2_fault;/*DC2故障*/\
    uint16_t DC3_alarm;/*DC3故障*/\
    uint16_t DC4_fault;/*DC4故障*/\
    uint16_t DC5_fault;/*DC5故障*/\
    uint16_t DC6_fault;/*DC6故障*/\
    int32_t TotalInputPower;/*输入功率*/\
    int32_t TotalOutputPower;/*输出功率*/\
    int16_t DC1Voltage;/*DC1电压 0.1V*/\
    int32_t DC1Current;/*DC1电流 0.01A*/\
    int16_t DC1Power;/*DC1功率 1W*/\
    int16_t DC2Voltage;/*DC2电压 0.1V*/\
    int32_t DC2Current;/*DC2电流 0.01A*/\
    int16_t DC2Power;/*DC2功率 1W*/\
    int16_t DC3Voltage;/*DC3电压 0.1V*/\
    int32_t DC3Current;/*DC3电流 0.01A*/\
    int16_t DC3Power;/*DC3功率 1W*/\
    int16_t DC4Voltage;/*DC4电压 0.1V*/\
    int32_t DC4Current;/*DC4电流 0.01A*/\
    int16_t DC4Power;/*DC4功率 1W*/\
    int16_t DC5Voltage;/*DC5电压 0.1V*/\
    int32_t DC5Current;/*DC5电流 0.01A*/\
    int16_t DC5Power;\
    int16_t DC6Voltage;/*DC6电压 0.1V*/\
    int32_t DC6Current;/*DC6电流 0.01A*/\
    int16_t DC6Power;/*DC6功率 1W*/\
    int32_t TotalInputEnergy;/*DCDC输入总电量 0.1KWh*/\
    int32_t DC1EnergyInput;/*DC1输入电量 0.1KWh*/\
    int32_t DC1EnergyOutput;/*DC1输出电量 0.1KWh*/\
    int32_t DC2EnergyInput;/*DC2输入电量 0.*/\
    int32_t DC2EnergyOutput;/*DC2输出电量 0.1KWh*/\
    int32_t DC3EnergyInput;/*DC3输入电量 0.1KWh*/\
    int32_t DC3EnergyOutput;/*DC3输出电量 0.1KWh*/\
    int32_t DC4EnergyInput; /*DC4输入总电量 0.1KWh*/\
    int32_t DC4EnergyOutput;/*DC4输出总电量 0.1KWh*/\
    int32_t DC5EnergyInput;/*DC5输入总电量 0.1KWh*/\
    int32_t DC5EnergyOutput;/*DC5输出总电量 0.1KWh*/\
    int32_t DC6EnergyInput; /*DC6输入总电量 0.1KWh*/\
    int32_t DC6EnergyOutput;/*DC6输出总电量 0.1KWh*/

#define	DCDC_SET_STRUCT_COMMON_REG_15600 \
    DCDC_set_struct DCDC_setings;  /*DC1电流设置 0.01A*/\
    uint16_t DC1_Current_set;/*DC1电流设置 0.01A*/\
    uint16_t DC1_Voltage_set;/*DC1电压设置 0.1V*/\
    uint16_t DC2_Current_set;/*DC2电流设置 0.01A*/\
    uint16_t DC2_Voltage_set;/*DC2电压设置 0.1V*/\
    uint16_t DC3_Current_set;/*DC3电流设置 0.01A*/\
    uint16_t DC3_Voltage_set;/*DC3电压设置 0.1V*/\
    uint16_t DC4_Current_set;/*DC4电流设置 0.01A*/\
    uint16_t DC4_Voltage_set;/*DC4电压设置 0.1V*/\
    uint16_t DC5_Current_set;/*DC5电流设置 0.01A*/\
    uint16_t DC5_Voltage_set;/*DC5电压设置 0.1V*/\
    uint16_t DC6_Current_set;/*DC6电流设置 0.01A*/\
    uint16_t DC6_Voltage_set;/*DC6电压设置 0.1V*/


typedef union {
    uint32_t value;
    struct {
        uint32_t gps_num     : 8;  // bit0~7   GPS 卫星数量
        uint32_t glonass_num : 8;  // bit8~15  GLONASS 卫星数量
        uint32_t galileo_num : 8;  // bit16~23 Galileo 卫星数量
        uint32_t beidou_num  : 8;  // bit24~31 BeiDou 卫星数量
    };
} SatellitesNum_t;

typedef struct {
    union {
        struct {
            /*11000~11015*/
            char     iot_type[12];//11000~11005
            uint64_t iot_sn;   //11006~11009 sn码
            uint64_t safe_code; //11010~11013 安全码
            uint32_t software_ver; // 软件版本号
            /*11016~11017*/
            uint32_t software_ver2;
            /*11018~11019*/
            //config_status1 cfg;     // IOT功能开启状态
            link_status   link;    // IOT功能连接状态
            /*11020~11029*/
            uint8_t sta_ipv4[4];
            uint8_t sta_gw[4];
            uint8_t sta_mask[4];
            uint16_t sta_rssi; //
            uint8_t sta_mac[6];
            /*11030~11033*/
            uint8_t ble_mac[6];
            uint16_t mobile_rssi; //
            /*11034~11038*/
            uint8_t MqttConnectMode;//0：无效 1：以太网 2：WIFI 3：4G
            uint8_t revd2;
            iot_fault_struct iot_fault;//iot主要告警
            uint16_t revd3[3];
            /*11039~11044*/
            uint16_t can_bus_error; // 11039 IOT CAN总线错误
            uint16_t wifi_disconnect_reasion; // 11040 WIFI断开原因
            uint8_t IP_4G[4]; // 11041
            uint8_t IP_ETH[4]; // 11043
            /*11045~11047*/
            uint32_t time_lastreport;//最后联网（设备与服务器通讯）时间 2   基于1970年初始日期的秒计数器； R
            uint8_t signal_4G;//信号强度    1   L8bit:信号强度（0-31或者99）    
            uint8_t SIM_ready;//bit0：SIM卡是否在位; bit7~1:预留    R 
            /*11048~11051*/
            int GPS_X;//
            int GPS_Y;//
            /*11052~11081*/
            uint8_t IMEI[20];//IMEI号码(移动设备识别号码) 10  15~17位数字,ASCII    R
            uint8_t IMSI[20];//IMSI号码(移动用户识别号码) 10  15位数字,ASCII   R
            uint8_t ICCID[20];//ICCID号码(集成电路/SIM卡识别号码)    10  20个字符组成（19位数字+最后1位英文字母）,ASCII 
            /*11082*/
            uint16_t Bind_SN;//绑定群组代号  
            /*11083~11085*/
            uint16_t WIFI_Mesh_ID[3];//6字节，用于区分不同MESH网络；(顶层网络名称，当前网络实际值)
            /*11086~11088*/
            uint8_t eth_mac[6];
            /*11089~11090*/
            SatellitesNum_t SatellitesNum;  // 定位服务卫星数量
            /*11091~11099*/
            uint16_t revd4[9];
            /*11100*/
            uint16_t Power_Off_Nums;//断电次数
            /*11101~11102*/
            uint32_t Save_Money_Nums;//省钱参数
            /*11103~11104*/
            iot_smart_ctrl_struct iot_smart_control;//TOU控制有效状态
            /*11105*/
            uint16_t DI_State;
            /*11106*/
            iot_wifi_mult_sta_flag_struct wifi_mult_sta_flag;//多WIFI标志
            /*11107~11122*/
            char wifi_station_ssid_now[32];     //当前连接的wifi ssid
            /*11123~11126*/
            uint8_t sta_dns1[4];
            uint8_t sta_dns2[4];
            /*11127~11142*/
            char Connected_BLE_Server_SN[32];   // 作为BLE Client,蓝牙配对已接入的Server广播SN
            /*11143*/
            uint16_t Device_Compression_Type;   // 设备压缩型号
            uint16_t revd_end[10];
        };
    };
}MOD_STRUCT_reg11000;

typedef struct {
	union {
		uint16_t all[100];//预留协议向上兼容

		struct {
		DCDC_set_struct DCDC_setings;// 
        uint16_t DC1_Voltage_set;//DC1电压设置 0.1V
		uint16_t DC1_Current_set;//DC1电流设置 0.1A
	    uint16_t DC2_Voltage_set;//DC2电压设置 0.1V
	    uint16_t DC2_Current_set;//DC2电流设置 0.1A
	    uint16_t DC3_Voltage_set;//DC3电压设置 0.1V
	    uint16_t DC3_Current_set;//DC3电流设置 0.1A
	    uint16_t DC4_Voltage_set;//DC4电压设置 0.1V
	    uint16_t DC4_Current_set;//DC4电流设置 0.1A
	    uint16_t DC5_Voltage_set;//DC5电压设置 0.1V
	    uint16_t DC5_Current_set;//DC5电流设置 0.1A
	    uint16_t DC6_Voltage_set;//DC6电压设置 0.1V
	    uint16_t DC6_Current_set;//DC6电流设置 0.1A
		};
	};
}CAN_STRUCT_reg15600;//DCDC模块通用/CHARGER 1 设置区


#define	IOT_SET_STRUCT_COMMON_REG_file \
	uint8_t file_data_flag;			 /*文件数据标识*/\
	uint8_t protocol_ver;			 /*载荷协议版本*/\
	uint8_t state_cod;			 	 /*场景代码*/\
	uint16_t RequestId;			 	 /*RequestId*/\
	uint16_t data_num;			 	 /*第几包数据*/\
	uint16_t data_total;			 /*数据包总数*/\
	uint8_t rsvd;			 		 /*预留*/\
	uint8_t slave_addr;			 	 /*从机地址*/\
	uint8_t func_code;			 	 /*功能码*/\
	uint32_t data_start_pos;		 /*文件数据开始位*/\
	uint32_t available_data;		 /*本次指令有效数据字节数*/\
	uint8_t *data_temp;		 		 /*有效数据*/\
	uint16_t crc;			 		 /*CRC*/


typedef union {
    uint16_t all;
    struct {
        uint16_t mqtt1_enable           : 2;  // bit1/0: MQTT1服务器主动登录上报使能
        uint16_t mqtt1_encrypt_enable   : 2;  // bit3/2: MQTT1加密使能
        uint16_t bluetti_time_update    : 2;  // bit5/4: 同步时间使能（iot不主动通过BLUETTI服务器网络获取时间）
    };
} Protocol_north_ctrl_t;

typedef union {
    uint16_t all;
    struct {
        uint16_t revd                   : 2;
    };
} Protocol_south_ctrl_t;

/**
 * @brief Type = 0x1 ,iot
 *
 20240814 windy 1999 len
 */

#define	IOT_SET_STRUCT_COMMON_REG_12000_0x2 \
	uint16_t factory;			 /*12000 恢复出厂设置*/\
	uint16_t wifi_sta_auth; 	 /*12001 wifi sta 加密认证方式*/\
	char wifi_sta_ssid[32]; 	/*12002~12017 WiFi sta ssid*/\
	char wifi_sta_password[64]; /*12018~12049 wifi sta 密码*/\
	sta_eanble_t sta_enable;	/*12050 WIFI路由器密码高32字节有效标志,0xA5表示有效，否则无效*/\
	uint16_t revd1[16]; 	 /*12051~12066 预留*/\
	char could_dns[128];		/*12067~12130 服务器地址*/\
	char mobile_apn[60];		/*12131~12160 apn*/\
	config_status on_off;/*12161~12162*/\
	thunder_ctrl_t thunder_ctrl;/*12163*/\
	uint32_t period_report;/*12164~12165 数据上报服务器周期（休眠周期，4G模块省流量目的）	2	秒数	R/W*/\
	Struct_IOT_Enable_mix1 IOT_Enable_mix1;\
	uint16_t IOT_Enable_mix2;\
	uint16_t Protocol_3r_Enable_mix1;\
	uint16_t Protocol_3r_Enable_mix2;\
    uint8_t ETH_netmask[4];/*12174~12175    子网掩码*/\
    uint8_t ETH_gw[4];/*12176~12177    网关*/\
    uint8_t ETH_DNS1[4];/*12178~12179    以太网DNS1*/\
    uint8_t ETH_DNS2[4];/*12180~12181    以太网DNS2*/\
    uint8_t Internal_Server_IP[4];/*12182~12183    内部服务器IP*/\
    uint16_t Internal_Server_Port;/*12184    内部服务器端口*/\
    char Set_BLE_Server_SN[20];/*12185~12194 客户端配对指定的SN 作为BLE Client,蓝牙配对Server广播SN设置*/\
    char Connected_BLE_Client_SN[20];/*12195~12204 服务端配对已接入的客户端 作为BLE Server,蓝牙配对已接入的BLE Client SN（作为Server，通过BLE modbus协议被动修改写入，蓝牙断开清零，用于辅助表征被谁接入）*/\
    lcd_mode_struct LCD_Mode;/*12205 断码屏设置*/\
    time_span_struct Time_Span;/*12206 时间跨度*/\
    uint8_t Wired_Ethernet_IP[4];/* 12207~12208 手动设置IP*/\
    uint16_t revd2[742];/*12209~12950*/\
    uint16_t wifi_ap_auth;/*12951*/\
    char wifi_AP_ssid[32];/*12952~12967*/\
    char wifi_AP_password[64];/*12968~12999*/


typedef struct {
    union {
        uint16_t all[1000];
        struct {
            uint16_t factory;            //12000 恢复出厂设置
            uint16_t wifi_sta_auth;      //12001 wifi sta 加密认证方式
            char wifi_sta_ssid[32];     //12002~12017 WiFi sta ssid
            char wifi_sta_password[64]; //12018~12049 wifi sta 密码
            sta_eanble_t sta_enable;    //12050 WIFI路由器密码高32字节有效标志,0xA5表示有效，否则无效
            uint16_t revd1[16];      //12051~12066 预留
            char could_dns[128];        //12067~12130 服务器地址
            char mobile_apn[60];        //12131~12160 apn
            config_status on_off;//12161~12162
            thunder_ctrl_t thunder_ctrl;//12163
            uint32_t period_report;//12164~12165 数据上报服务器周期（休眠周期，4G模块省流量目的）    2   秒数  R/W
            uint64_t Network_name_BR_set;/*12166~12169 下级网络名称（SUB1GHZ）*/
			Struct_IOT_Enable_mix1 IOT_Enable_mix1;//12170
            uint16_t IOT_Enable_mix2;/*12171*/
            Protocol_north_ctrl_t Protocol_3r_Enable_mix1;/*12172*/
            Protocol_south_ctrl_t Protocol_3r_Enable_mix2;/*12173*/
            uint8_t ETH_netmask[4];/*12174~12175    子网掩码*/
            uint8_t ETH_gw[4];/*12176~12177    网关*/
            uint8_t ETH_DNS1[4];/*12178~12179    以太网DNS1*/
            uint8_t ETH_DNS2[4];/*12180~12181    以太网DNS2*/
            uint8_t Internal_Server_IP[4];/*12182~12183    内部服务器IP*/
            uint16_t Internal_Server_Port;/*12184    内部服务器端口*/
            uint16_t revd[20];  // 12185~12204
            lcd_mode_struct LCD_Mode;//12205 断码屏设置
            time_span_struct Time_Span;//12206 时间跨度
            uint8_t Wired_Ethernet_IP[4];// 12207~12208 手动设置IP
            uint16_t revd_end[742];//12209~12950
            uint16_t wifi_ap_auth;//12951
            char wifi_AP_ssid[32];//12952~12967
            char wifi_AP_password[64];//12968~12999
        };
    };
}MOD_STRUCT_reg12000;

_Static_assert(sizeof(MOD_STRUCT_reg12000) == 2000, "MOD_STRUCT_reg12000 size mismatch");

typedef struct {
    ble_protocol_en_t ble_protocol;     // 13600
    int16_t blec_rssi_threshold;        // 13601 自己作为BLE client查询设备有效 RSSI信号强度切换阈值
    uint16_t blec_switch_interval;      // 13602 BLE Client 切换周期间隔（RSSI <X）
    uint8_t bles_adv_key[16];           // 13603~13610 ble server 广播数据加密秘钥, 对外只读
    int16_t wifi_sta_rssi_threshold;    // 13611 WiFi STA RSSI阈值
    uint16_t wifi_sta_switch_interval;  // 13612 WiFi STA 切换周期间隔（RSSI <X）
    wifi_mul_sta_en_t wifi_mul_sta_en;  // 13613 WiFi多连接使能
    uint32_t wifi_sta1_ip;              // 13614~13615 WiFi STA1静态IP地址
    uint32_t wifi_sta1_mask;            // 13616~13617 WiFi STA静态子网掩码
    uint32_t wifi_sta1_gw;              // 13618~13619 WiFi STA静态网关地址
    uint32_t wifi_sta1_dns1;            // 13620~13621 WiFi STA静态DNS2地址
    uint32_t wifi_sta1_dns2;            // 13622~13623 WiFi STA静态DNS2地址

    uint16_t wifi_sta2_auth;            // 13624 WiFi STA2认证方式
    char wifi_sta2_ssid[32];            // 13625~13640 WiFi STA2 SSID
    char wifi_sta2_password[64];        // 13641~13672 WiFi STA2密码
    uint32_t wifi_sta2_ip;              // 13673~13674 WiFi STA2静态IP地址
    uint32_t wifi_sta2_mask;            // 13675~13676 WiFi STA2静态子网掩码
    uint32_t wifi_sta2_gw;              // 13677~13678 WiFi STA2静态网关地址
    uint32_t wifi_sta2_dns1;            // 13679~13680 WiFi STA2静态DNS1地址
    uint32_t wifi_sta2_dns2;            // 13681~13682 WiFi STA2静态DNS2地址

    uint16_t wifi_sta3_auth;            // 13683 WiFi STA3认证方式
    char wifi_sta3_ssid[32];            // 13684~13699 WiFi STA3 SSID
    char wifi_sta3_password[64];        // 13700~13731 WiFi STA3密码
    uint32_t wifi_sta3_ip;              // 13732~13733 WiFi STA3静态IP地址
    uint32_t wifi_sta3_mask;            // 13734~13735 WiFi STA3静态子网掩码
    uint32_t wifi_sta3_gw;              // 13736~13737 WiFi STA3静态网关地址
    uint32_t wifi_sta3_dns1;            // 13738~13739 WiFi STA3静态DNS1地址
    uint32_t wifi_sta3_dns2;            // 13740~13741 WiFi STA3静态DNS2地址

    open_mqtt_en_t open_mqtt_enable;    // 13742 MQTT上报使能
    uint16_t open_mqtt_report_cycle;    // 13743 MQTT上报周期 上报使能打开后有效

    modbus_tcp_en_t modbus_tcp_enable;  // 13744 Modbus TCP使能
    uint16_t modbus_tcp_port;           // 13745 Modbus TCP端口号

    uint16_t ETH_config;                // 13746 有线以太网接口的配置
    uint32_t IP_ETH_Set3;               // 13747~13748 有线以太网接口配置的本地IP配置
    uint32_t Mask_ETH_Set3;             // 13749~13750 有线以太网接口配置的本地网络掩码
    uint32_t Gateway_ETH_Set3;          // 13751~13752 有线以太网接口配置的本地网关
    uint32_t DNS1_ETH_Set3;             // 13753~13754 有线以太网接口配置的本地DNS1
    uint32_t DNS2_ETH_Set3;             // 13755~13756 有线以太网接口配置的本地DNS2

    uint32_t IP_WIFI_AP_Set;            // 13757~13758 自身WIFI AP的本地IP配置(默认192.168.8.1,暂定不可修改)
    uint32_t Mask_WIFI_AP_Set;          // 13759~13760 自身WIFI AP的本地网络掩码(默认225.255.255.0(即24位掩码)，暂不可修改)
    uint32_t Gateway_WIFI_AP_Set;       // 13761~13762 自身WIFI AP的本地网关(默认192.168.8.1,暂定不可修改)

    uint16_t Internal_Server_Port[10];  // 13763~13772 内部服务器端口

    uint16_t IOT_power_aux_enable;      // 13773 IOT辅助功能使能
    uint16_t CAN_protocol;              // 13774 HMI CAN接口协议选择
    uint16_t RS485_protocol;            // 13775 HMI RS485接口协议选择
    
    uint8_t pcs_name_set[32];            // 13776~13791 绑定的pcs名称
    uint8_t pcs_name_connected[32];      // 13792~13807 已连接的pcs名称
    
    uint16_t wlcc_hearbeat_interval;    // 13808 设备间通讯协议心跳帧间隔 bit0~9:设备间通讯协议心跳帧间隔，单位100毫秒(1代表100ms)
    uint16_t AC_EMS_Coordination_Mode;  // 13809 本地设备间AC EMS调度协作模式
    uint16_t Gen_Time_Status1;          // 13810 IOT智能油机定时状态1
    uint16_t Gen_Time_Status2;          // 13811 IOT智能油机定时状态2
    uint16_t revd_end[10];
}MOD_STRUCT_reg13600;


typedef  inv_wifi_struct MOD_STRUCT_reg13000;// inv第三方wifi

typedef struct {
    uint8_t mesh_id[6];// 13500~13502
    uint16_t revd1[2];      //13503~13504 预留
    uint16_t ready_mesh_set;//13505
    uint16_t sys_sequence;//13506
    uint16_t dev_type;//13507
    uint16_t cmd;//13508
    uint8_t wifi_station_bssid[6];//13509~13511
    // add by mesh
    uint16_t node_type;   //13512   [0:普通节点]; 1:根节点    
}MOD_STRUCT_reg13500;

typedef struct {
    /*11000~11015*/
    char     hmi_type[12];//11000~11005
    uint64_t hmi_sn;   //11006~11009 sn码
    uint32_t mcu_ver; // 11010~11011软件版本号
    uint32_t flash_ver; // 11012~11013软件版本号
}MOD_STRUCT_reg14000;

typedef union {
    int16_t all;

    struct {
        int8_t ntc_temp : 8;
        uint8_t revd : 8;
    };
} plug_ntc_temp_t;

typedef struct {
    char     SmartPlug_Type[12];//14500~14505
    uint64_t SmartPlug_SN;   //14506~14509 sn码
    uint32_t SmartPlug_Software_Version; //14510~14511 软件版本号
    uint16_t SmartPlug_Nums;//14512 智能插座接入数量
    int32_t SmartPlug_Power;//14513~14514 AC功率,X10w
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
    uint16_t SmartPlug_States;//14518 状态 
    uint16_t FaultInformation;//14519 故障/保护
    uint16_t WarnInformation;//14520 告警(不影响正常工作)
    uint32_t Day_Consumption_Electricity;//14521~14522 今日使用电量
    uint32_t Day_Use_Time;//14523~14524 今日使用时长
    uint16_t Delay_Set_Start;//14525 动作延时设置时间
	uint32_t Day_feedin_kwh;//	14526 14527今日产生电量 
	uint16_t PF;//14528	功率因数
    plug_ntc_temp_t ntc_temperature;    //NTC温度传感器检测的温度, 单位℃
    uint16_t revd_end[10];
}MOD_STRUCT_reg14500;

/*设置只执行透传，本地不具体分析*/
typedef struct {
    uint16_t Protection_En;//14700 保护使能
    // uint16_t Set_Enable[3];//14701~14703 设置使能
    plug_setting1_t setting1;
	plug_setting2_t setting2;
	plug_setting3_t setting3;
    uint16_t Time_Set_Enable[2];//14704~14705 定时时间设置使能
    uint16_t Overload_protection_power_set;//14706 过载保护功率设置值
    uint16_t Underload_protection_power_set;//14707 欠功率保护功率设置值
    uint16_t Indicator_light_regulator;//14708 插座指示灯亮度调节
    uint32_t Time_Set;//14709~14710 定时时间设置
    uint16_t Delay_Set;//14711 动作延时设置目标时间(倒计时)
    SmartPlug_Time_Set Time_Set_Detail[10];//14712~14731
    soc_cfg_t soc_cfg;                 // 14732 soc限制
    uint16_t revd_end[10];
}MOD_STRUCT_reg14700;


typedef struct {
    char dc_hub_type[12];               // DC_HUB型号           15700~15705
    uint64_t dc_hub_sn;                 // DC_HUB序列号         15706~15709
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
    uint16_t revd1[48];                 // 15752~15799
}MOD_STRUCT_reg15750;


typedef struct {
    char ac_hub_type[12];               // AC_HUB型号           15800~15805
    uint64_t ac_hub_sn;                 // AC_HUB序列号         15806~15809
    uint8_t SoftwareType;               // AC_HUB软件类型       15810
    uint32_t SoftwareVersion;           // AC_HUB软件版本       15811~15812
    uint64_t ACHUB_safe_code;           // AC_HUB安全码         15813~15816
    uint8_t revd[12];                   // 预留                 15817~15822
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
    uint16_t alarm_State;               // 告警状态             15837
    uint16_t revd1[26];                 // 15838~15850
}MOD_STRUCT_reg15800;

typedef struct {
    uint16_t ver;// 21000 协议版本
    union 
    {
        uint16_t all[999];

        // ver : 01/03
        struct 
        {
            uint16_t bias;// 21001 设备数量偏移;第一帧为0
            POINT_BIND_INFO self_bind_info;
            POINT_BIND_INFO point_bind_info[MODBUS_21000_DEVICE_MAX_CNT];
        };	

        // ver : 40004
        struct 
        {
            uint8_t curr_idx;  // 当前包序号
            uint8_t total;     // 包总数  
            uint16_t reg_data[128];
        };	
    };	    
}MOD_STRUCT_reg21000;

_Static_assert(sizeof(MOD_STRUCT_reg21000) == (1000 * sizeof(uint16_t)), "MOD_STRUCT_reg21000 size mismatch");

typedef struct {
    uint16_t ver;// 21000 协议版本
    union 
    {
        uint16_t all[999];

        // ver : 30001
        struct 
        {
            uint16_t bias;// 21001 设备数量偏移;第一帧为0
            POINT_BIND_INFO_WR point_bind_info[30];//21002,单贞最大32个节点 
        };	

        // ver : 40004/40005
        struct 
        {
            uint8_t curr_idx;  // 当前包序号
            uint8_t total;     // 包总数  
            uint16_t reg_data[128];
        };	
    };
}MOD_STRUCT_reg21000_WR;

typedef struct {
	uint16_t Net_Public_Protocol_version;	//22000 服务器交互协议版本 
	uint16_t revd[4];		//4个预留
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

#define SOFTVER_29800_NUM_MAX   17

typedef struct {
    uint32_t SW_Code_Ver;   // 公司标准软件代号
    uint32_t Ctrl_Code;     // 内部管控版本号
}softver_29800_struct;

typedef struct {
    uint16_t softver_num;       // 29800 软件编号总数量
    softver_29800_struct softver[SOFTVER_29800_NUM_MAX];
}MOD_STRUCT_reg29800;

typedef struct {
    uint16_t revd1;//30900
    uint16_t test_work_mode; //30901 测试模式切换 0x55:进入正常模式 0xA0:进入单板测试模式 0xB0:4G测试模式
    union 
    { 
        uint16_t revd[98];
        
        // common iot
        struct 
        { 
#if CONFIG_LOG_UDP_REDIRECT == 1	
            uint16_t test_net_mode; //30902 网络模式切换 0：WIFI模式（默认） 1：以太网模式 2：4g模式
            char test_ip[16];//30903~30910 当前模式ip地址
            uint16_t test_reset;// 30911 恢复出厂设置标志 0：无 1：已完成，待断电
            uint16_t revd2[2];
            uint32_t log_mark; // 30914~30915
            uint16_t log_mode; // 30916 0:原始串口, 1:udp
            uint16_t log_port; // 30917 日志发送端口
            char log_dst_ip[16]; // 30918~30927 日志发送目标ip地址
#elif CONFIG_LOG_UDP_REDIRECT == 2
            uint16_t super_cmd;     //30902 超级命令
            uint32_t cmd_value;     // 30903~30904 超级命令参数值
            uint16_t server_url[16];   //30905~30920 服务器ip地址
            uint16_t server_port;     //30921 当前模式端口号
            uint16_t test_reset;// 30922 恢复出厂设置标志 0：无 1：已完成，待断电
            uint32_t revd2[2];
#endif
        };

        // 4g test
        struct 
        { 
            char IMEI[20];        /* 30902~30911 !< 模块IMEI号（国际移动设备识别码），每个设备唯一的标识 */
            char IMSI[20];        /* 30912~30921 !< SIM卡IMSI号（国际移动用户识别码），每张SIM卡唯一的标识 */
            char ICCID[20];       /* 30922~30931 !< SIM卡ICCID号（集成电路卡识别码），SIM卡的序列号 */
            char ip[20];          /* 30932~30941 !< 4G PPP连接分配的本地IPv4地址（4字节） */
            int16_t rssi;           /* 30942 !< 信号强度（RSSI），-109~-51dBm，0表示未知 */
            uint16_t net_status;         /* 30943 !< 4G网络连接状态，true表示已连接，false表示未连接 */
            uint16_t sim_card_status;    /* 30944 !< SIM卡状态，true表示已插卡，false表示未检测到SIM卡 */
            uint32_t GPS_X;          /* 30945~30946 !< GPS定位X坐标（如经度，具体含义根据实际应用定义） */
            uint32_t GPS_Y;          /* 30947~30948 !< GPS定位Y坐标（如纬度，具体含义根据实际应用定义） */
            uint16_t gps_num;         /* 30949 !< Number of GPS satellites in view */
            uint16_t gl_num;          /* 30950 !< Number of GLONASS satellites in view */
            uint16_t gal_num;         /* 30951 !< Number of Galileo satellites in view */
            uint16_t bd_num;          /* 30952 !< Number of BeiDou satellites in view */
            uint16_t fix;             /* 30953 !< Fix status (1 = valid, 0 = invalid) */
        };
    };
}MOD_STRUCT_reg30900;

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

    /************************ 设备其他数据 ************************/
    int16_t  iot_internal_temp;              // 模组内部温度
    uint16_t iot_reset_reason;               // 模块复位原因
} MOD_STRUCT_reg40000_IotDebugStatus_t;

typedef struct {
    uint16_t  param_index;

    union 
    { 
        uint16_t  param[511];
        MOD_STRUCT_reg40000_IotDebugStatus_t IoT_Debug_Data;
    };
}MOD_STRUCT_reg40000;


typedef struct {
    MOD_STRUCT_reg00000 mod_reg00000; //  6~16,

    MOD_STRUCT_reg00100 mod_reg00100_AppPage1; //100~168  APP首页数据（汇总）
    MOD_STRUCT_reg00700 mod_reg00700_OTA; //700~768  OTA设置区
    MOD_STRUCT_reg01100 mod_reg01100_Inv_base; //1100~1154  每台逆变器基本信息
//    MOD_STRUCT_reg01200 mod_reg01200_Inv_pv; //1200~1289  每台逆变器PV信息
//    MOD_STRUCT_reg01300 mod_reg01300_Inv_grid; //1300~1330 每台逆变器电网信息 
//    MOD_STRUCT_reg01400 mod_reg01400_Inv_load; //1400~1447  每台逆变器负载信息
//    MOD_STRUCT_reg01500 mod_reg01500_Inv_inv; //1500~1529  每台逆变器逆变信息
//    MOD_STRUCT_reg01600 mod_reg01600_Inv_generator; //1600~1613 每台逆变器发电机信息
    MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067  基础设置项
    MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区
//    MOD_STRUCT_reg02300 mod_reg02300_Inv_set02_struct; //7000~7004  pack设置区
//    
//    MOD_STRUCT_reg02400 mod_reg02400_Inv_certification; //2400~2449  认证设置项
//    MOD_STRUCT_reg02500 mod_reg02500_Inv_advance_set2; //2500~2544  每台逆变高级设置区2（面向微逆）
//    MOD_STRUCT_reg03000 mod_reg03000_Inv_history; //3000~3026  每台逆变器历史记录区
//    MOD_STRUCT_reg03500 mod_reg03500_Inv_yearX_statistic; //3500~3547    每台逆变器累计年电量区
//    MOD_STRUCT_reg03600 mod_reg03600_Inv_year1_statistic; //3600~3658  每台逆变器当年电量区
//    MOD_STRUCT_reg04000 mod_reg04000_Dsp_data;//4000~4049 dsp数据区
//    MOD_STRUCT_reg04050 mod_reg04050_Dsp_set1;//4050~4104 dsp设置区1
//    MOD_STRUCT_reg04105 mod_reg04105_Dsp_set2;//4105~4177 dsp设置区2
//    MOD_STRUCT_reg06000 mod_reg06000_Pack_sum; //6000~6031    pack主包汇总信息
//    MOD_STRUCT_reg06100 mod_reg06100_Pack_each; //6100~6203  pack单包信息
//    MOD_STRUCT_reg07000 mod_reg07000_Pack_set; //7000~7004  pack设置区

	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
	MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
//	MOD_STRUCT_reg15500 mod_reg15500_DCDC_info;//16000~16040	DCDC模块通用 信息区
//	MOD_STRUCT_reg15600 mod_reg15600_DCDC_set;//15600~15640	DCDC模块通用 设置区
    MOD_STRUCT_reg13500 mod_reg13500_mesh; //13500~13511   WIFI MESH区
    
	MOD_STRUCT_reg21000	mod_reg21000_data;//绑定帧的读写不一样
	MOD_STRUCT_reg21000_WR	mod_reg21000_bind_WR;
	
//	MOD_STRUCT_reg22000 mod_reg22000_net_server_2rd;
//  MOD_STRUCT_reg29700 mod_reg29700_IOT_info;//29700~29720 IOT标定区（从模式写入）
    MOD_STRUCT_reg29800 mod_reg29800_softver_info;//软件版本汇总上报（29801~29900）
	MOD_STRUCT_reg30900 mod_reg30900_Test;
	MOD_STRUCT_reg40000 mod_reg40000_Param;
}SELF_DATA_STRUCT;

//extern   SELF_DATA_STRUCT  top_modbus_rd.Inv[reals.Addr_can_self];
//extern   SELF_DATA_STRUCT  top_modbus_wr.Inv;

////////////////////////////////////////ESP IOT////////////////////////////////////////////////

typedef union//2Byte
{
   // uint8_t all;
    uint16_t word;
    struct structBIND_DEV_STATE
    {
        uint8_t need_update          :1; //  
        uint8_t happen_alarm         :1; //  
        uint8_t happen_protect       :1;// 
        uint8_t revd1       		 :1;// 

        uint8_t revd2       :4;// 
        
        uint8_t revd4       :8;// 
    }bits;
}g_st_BIND_DEV_STATE;


typedef struct {
	uint8_t modbus_slave_address;
	uint8_t hreserved;
	g_st_BIND_DEV_STATE  dev_state;
	char SN[10];	
} Dev_bind_info_struct_t;




typedef struct {
	uint16_t version;//蓝牙/MQTT=2；
	uint16_t bias_dev_addr;//设备数量偏移;第一帧为0，第二帧为前面所有帧已传递节点数量的和，最大节点数量仅支持255个
	uint16_t onlineL16;//低16个节点在线状态，0~15bit分别顺序表示各个节点的在线状态，1-在线，0-不在线
	uint16_t onlineH16;//高16个节点在线状态
	
	Dev_bind_info_struct_t	dev_bind_info[32];//tbd
} Frame_bind_info_struct_t;


//网络通道使能，0-无效；1-仅使能WIFI；2-仅使能有线以太网；3-仅使能4G模块；4-自动选择；5-禁止所有上网
#define	NET_CH_INVALIDE		0//
#define	NET_CH_WIFI			1//
#define	NET_CH_SPI_ETH		2//
#define	NET_CH_4G			3//
#define	NET_CH_AUTO			4//
#define	NET_CH_FORBID_ALL	5//



/**
 * @brief Type = 0x01
 *
 */
typedef struct {
    uint8_t wifi_sta_auth;      // wifi sta 加密认证方式
    char wifi_sta_ssid[32];     // WiFi sta ssid
    char wifi_sta_password[64]; // wifi sta 密码
    sta_eanble_t sta_enable;    // WIFI路由器密码高32字节有效标志,0xA5表示有效，否则无效
    char reserved[32];      // 预留
    char could_dns[128];        // 服务器地址
    char mobile_apn[60];        // apn
    uint8_t factory;            // 恢复出厂设置
	config_status on_off;
	thunder_ctrl_t thunder_ctrl;
//	Net_Server_2RD_struct_t net_server_2rd;//22000，读+写
	
//    rw_cmd_struct rw_cmd;
}iot_config_struct;

/**
 * @brief Type = 0x02
 *
 */
typedef struct {
    char     dev_type[12]; // 下游设备SN码
    uint64_t dev_sn;       // 下游设备SN码

    char     iot_type[12];
    uint64_t iot_sn;   // sn码
    uint64_t safe_code; // 安全码

    uint8_t  status;
    uint32_t protocol_ver; // 协议版本号
    uint32_t hardware_ver; // 硬件版本号
    uint32_t software_ver; // 软件版本号
//    rw_cmd_struct rw_cmd;
}iot_about_struct;

/**
 * @brief Type = 0x03
 *
 */
typedef struct {
    uint8_t sta_fault;
    uint8_t ap_fault;
    uint8_t sta_ipv4[4];
    uint8_t sta_gw[4];
    uint8_t sta_mask[4];
    uint8_t sta_mac[6];
     int8_t sta_rssi; //
    uint8_t sta_fault_code;
//    rw_cmd_struct rw_cmd;
}iot_wifi_struct;



///**
// * @brief type= 0x1
// *
// */
//typedef struct {
////	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
//	IOT_INFO_STRUCT_COMMON_REG_11000_0x1
//
//	rw_cmd_struct   rw_cmd;
//}iot_11000_can_struct;
//
///**
// * @brief type= 0x2
// *
// */
//typedef struct {
////	MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
//	IOT_SET_STRUCT_COMMON_REG_12000_0x2
//    rw_cmd_struct   rw_cmd;
//}iot_12000_can_struct;

/**
 * @brief type= 0x48
 *
 */
typedef struct {
//	DCDC_INFO_STRUCT_COMMON_REG_15500
    char dc_dev_type[12];   // DCDC设备型号
    uint64_t dc_dev_sn;     // DCDC设备SN
    uint16_t dc_input_voltage;  // DCDC输入电压 0.1V
    uint16_t dc_output_voltage;  // DCDC输出电压 0.1V
    uint16_t dc_output_current;  // DCDC输出电流 0.1A
    uint16_t dc_output_power;    // DCDC输出功率 1W
    line_sate_struct  line_sate; //能量流动条
    uint16_t bat_type;          // 电池类型  
    uint16_t DC1_fault;//DC1故障
    uint16_t DC2_fault;//DC2故障
    uint16_t DC3_alarm;//DC3故障
    uint16_t DC4_fault;//DC4故障
    uint16_t DC5_fault;//DC5故障
    uint16_t DC6_fault;//DC6故障
    uint16_t res[4];          // 保留
    dev_info_struct Dev_info;
    int32_t TotalInputPower;//输入功率
    int32_t TotalOutputPower;//输出功率
    int16_t DC1Voltage;//DC1电压 0.1V
    int32_t DC1Current;//DC1电流 0.01A
    int16_t DC1Power;//DC1功率 1W
    int16_t DC2Voltage;//DC2电压 0.1V
    int32_t DC2Current;//DC2电流 0.01A
    int16_t DC2Power;//DC2功率 1W
    int16_t DC3Voltage;//DC3电压 0.1V
    int32_t DC3Current;//DC3电流 0.01A
    int16_t DC3Power;//DC3功率 1W
    int16_t DC4Voltage;//DC4电压 0.1V
    int32_t DC4Current;//DC4电流 0.01A
    int16_t DC4Power;//DC4功率 1W
    int16_t DC5Voltage;//DC5电压 0.1V
    int32_t DC5Current;//DC5电流 0.01A
    int16_t DC5Power;
    int16_t DC6Voltage;//DC6电压 0.1V
    int32_t DC6Current;//DC6电流 0.01A
    int16_t DC6Power;//DC6功率 1W
    int32_t TotalInputEnergy;//DCDC输入总电量 0.1KWh
    int32_t DC1EnergyInput;//DC1输入电量 0.1KWh
    int32_t DC1EnergyOutput;//DC1输出电量 0.1KWh
    int32_t DC2EnergyInput;//DC2输入电量 0.
    int32_t DC2EnergyOutput;//DC2输出电量 0.1KWh
    int32_t DC3EnergyInput;//DC3输入电量 0.1KWh
    int32_t DC3EnergyOutput;//DC3输出电量 0.1KWh
    int32_t DC4EnergyInput; //DC4输入总电量 0.1KWh
    int32_t DC4EnergyOutput;//DC4输出总电量 0.1KWh
    int32_t DC5EnergyInput;//DC5输入总电量 0.1KWh
    int32_t DC5EnergyOutput;//DC5输出总电量 0.1KWh
    int32_t DC6EnergyInput; //DC6输入总电量 0.1KWh
    int32_t DC6EnergyOutput;//DC6输出总电量 0.1KWh
    uint16_t dev_type;
	uint32_t version;

	rw_cmd_struct   rw_cmd;
}iot_dcdc_15500_can_struct;

/**
 * @brief type= 0x49
 *
 */
typedef struct {
//	DCDC_SET_STRUCT_COMMON_REG_15600
	DCDC_set_struct DCDC_setings;// 15600
   uint16_t DC1_Voltage_set;// 15601 DC1电压设置 0.1V
   uint16_t DC1_Current_set;//15602 DC1电流设置 0.1A
   uint16_t DC2_Voltage_set;//15603 DC2电压设置 0.1V
   uint16_t DC2_Current_set;//15604 DC2电流设置 0.1A
   uint16_t DC3_Voltage_set;//15605 DC3电压设置 0.1V
   uint16_t DC3_Current_set;//15606 DC3电流设置 0.1A
   uint16_t DC4_Voltage_set;//15607 DC4电压设置 0.1V
   uint16_t DC4_Current_set;//15608 DC4电流设置 0.1A
   uint16_t DC5_Voltage_set;//15609 DC5电压设置 0.1V
   uint16_t DC5_Current_set;//15610 DC5电流设置 0.1A
   uint16_t DC6_Voltage_set;//15611 DC6电压设置 0.1V
   uint16_t DC6_Current_set;//15612 DC6电流设置 0.1A

    rw_cmd_struct   rw_cmd;
}iot_dcdc_15600_can_struct;
typedef struct {
//	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
	IOT_INFO_STRUCT_COMMON_REG_11000_0x1
    rw_cmd_struct   rw_cmd;
}iot_can_node_struct_reg11000;	
typedef struct {
	// MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
	IOT_SET_STRUCT_COMMON_REG_12000_0x2	
    rw_cmd_struct   rw_cmd;
}iot_can_node_struct_reg12000;	

typedef struct {
//	
	DCDC_INFO_STRUCT_COMMON_REG_15500
    rw_cmd_struct   rw_cmd;
}iot_can_node_struct_reg15500;	
typedef struct {
//	
	DCDC_SET_STRUCT_COMMON_REG_15600	
    rw_cmd_struct   rw_cmd;
}iot_can_node_struct_reg15600;	

typedef struct {
	IOT_SET_STRUCT_COMMON_REG_12000_0x2

} iot_can_node_struct_reg12000_mini;	


/*

DCDC 模块 windy 20241122 add
关联
MOD_STRUCT_reg15500
MOD_STRUCT_reg16000
MOD_STRUCT_reg15600

*/
typedef struct {
	DCDC_INFO_STRUCT_COMMON_REG_15500	
    rw_cmd_struct   rw_cmd;
}can_node_struct_DCDC_INFO1;	
typedef struct {
	IOT_SET_STRUCT_COMMON_REG_12000_0x2	
    rw_cmd_struct   rw_cmd;
}can_node_struct_DCDC_INFO2;	
typedef struct {
	DCDC_SET_STRUCT_COMMON_REG_15600	
    rw_cmd_struct   rw_cmd;
}can_node_struct_DCDC_SET;	

/**
 * @brief type= 0xfa
 *
 */
typedef struct {
    uint8_t file[2048];
	// IOT_SET_STRUCT_COMMON_REG_file
    rw_cmd_struct   rw_cmd;
}iot_file_can_masetr_struct;

/**
 * @brief type= 0xfb
 *
 */
typedef struct {
    uint8_t file[2048];
	// IOT_SET_STRUCT_COMMON_REG_file // 结构体长度不定长，复制时会导致溢出
    rw_cmd_struct   rw_cmd;
}iot_file_can_slave_struct;


typedef struct {
    uint8_t              devId;
    uint8_t              online;
    uint16_t             alive_time;

	/*
	
	并机排序之后才set，排地址时候清零
		*/	  
	uint16_t			setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

	iot_can_node_struct_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
	iot_can_node_struct_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
    device_registers_t factory; // iot标定信息,can type=0xFF
}can_node_struct_Iot;//iot read



/**
 * @brief Type = 0x04
 *
 */
typedef struct {
    uint8_t fault;
    uint8_t mac[6];
    uint8_t ver; // 0：ble4.0,1：ble4.2,2：ble5.0
    char name[29]; // 蓝牙名
//    rw_cmd_struct rw_cmd;
}iot_ble_struct;

/**
 * @brief Type = 0x05
 *
 */
typedef struct {
    uint8_t fault; // 4g状态
     int8_t rssi;   // 基站信号强度
    uint8_t type[20]; // 移动基站网络类型
    uint16_t mcc; // 移动设备所在国家代码
    uint8_t mnc;    // 移动设备网络代码
    uint32_t station_code;  // 基站编号
    uint8_t sim_iccid[20];  //
    char network_operator[40]; // 运营商
//    rw_cmd_struct rw_cmd;
}iot_4g_struct;

/**
 * @brief Type = 0x06
 *
 */
typedef struct {
    uint8_t status; // 状态
    uint8_t longitude_direct; // 经度纬度
    uint32_t lagitue_value; //纬度
    uint32_t longitude_value; // 经度
//    rw_cmd_struct rw_cmd;
}iot_local_struct;

typedef struct {
	char hmi_model[12];
	uint64_t hmi_sn;
	uint32_t mcu_ver;
	uint32_t flash_ver;
} hmi_node_struct_t;

typedef struct {
    uint16_t  pile_type[20];   // 充电桩型号
    uint16_t  sncode[20];      // 充电桩SN
}charging_pile_about_struct;

typedef struct {
    uint16_t input_voltage;  //输入交流电压
    int16_t  input_current;  //输入交流电流
    int16_t  input_power;   // 输入交流有功功率 w
}charging_pile_detail_struct;

typedef struct {
    #if(0)
    uint16_t input_L1_voltage;   // 输入L1交流电压
    uint16_t input_L2_voltage;   // 输入L1交流电压
    uint16_t input_L3_voltage;   // 输入L1交流电压
    int16_t  input_L1_current;   // 输入L1交流电压
    int16_t  input_L2_current;   // 输入L1交流电压
    int16_t  input_L3_current;   // 输入L1交流电压
    int16_t  input_L1_power;   // 输入功率 w
    int16_t  input_L2_power;   // 输入功率 w
    int16_t  input_L3_power;   // 输入功率 w
    uint8_t  status;        // 工作状态
    uint8_t  input_type;    // 接入类型 0--PV,1--CAR,2--Adpater,3--other
    uint16_t input_power;   // 输入功率 w
    uint16_t input_voltage; // 输入电压 0.1V
    uint16_t input_current; // 输入电流 0.1A
    #endif
    charging_pile_detail_struct charging_pile_L1;  //LIN
    charging_pile_detail_struct charging_pile_L2;  //L2N
    charging_pile_detail_struct charging_pile_L3;  //L3N
    charging_pile_detail_struct input_L1_sum;   //总
    int16_t  input_L1_sum_reactive_power;   //总无功功率实际值
    int16_t  input_L1_sum_active_power;    //总视在功率实际值
    uint16_t input_PF;          //交流PF
    uint16_t input_f;           //交流频率
    int16_t  T_gun_head;        //枪头温度
    int16_t  T_box;             //枪体内部温度
    uint16_t pail_fault;        //充电桩故障码代号
    uint16_t gun_state;         //枪工作状态代号
    uint16_t pail_state;        //状态
    uint16_t work_time_L;       //已充电时长_L（本次）
    uint16_t work_time_H; 
    int16_t  Charge_Energy;     //已充电电量
    uint16_t TimeCar_Begin_L;   //汽车充电开始时间
    uint16_t TimeCar_Begin_H; 
    uint16_t TimeCar_End_L;     //汽车充电结束时间
    uint16_t TimeCar_End_H; 
    uint16_t CP_Volt;           //交流枪头CP信号
    uint16_t CC_PWM_duty;       //交流枪头CC信号占空比
}charging_pile_meg_struct;

typedef struct {
	union {
		uint16_t all;
		struct {
			uint8_t charge_en : 2;		//充电开关			
			uint8_t discharge_en : 2;	//放点开关
			uint8_t supply_phases : 4;	//供电相数数量设置	
        };	
	};
} pile_set_t;

typedef struct {
	union {
		uint16_t all;
		struct {
			uint8_t charge_start_en : 2;	//充电时间设置使能				
			uint8_t charge_end_en : 2;	    //结束充电时间设置使能
			uint8_t pile_get_pack_en : 2;	//允许充电桩从储能电池取电；	
            uint8_t pile_gridout_en : 2;	//允许充电桩在电网断电情况下给车桩充电			
			uint8_t gun_en : 2;	            //使能插枪自动充电功能
            uint8_t pile_en : 2;	        //使能插枪自动充电功能
		};
	};
} pile_enable_t;

typedef struct {
    uint16_t PF_set;                        //PF设置值
    pile_set_t  pile_set;                   //设置
    int16_t Pactive_set;                    //有功功率设置目标
} charging_pile_set_struct;

typedef struct {
    uint16_t SetTimeStartCarCharge_L;      //汽车充电开始设置时间	
    uint16_t SetTimeStartCarCharge_H;
    uint16_t SetTimeEndCarCharge_L;        //汽车充电结束设置时间
    uint16_t SetTimeEndCarCharge_H;
    pile_enable_t pile_enable;             //使能设置
    uint16_t SOC_Low_level_ChargeCar;      //汽车充电从储能电池取电的储能最小限值
} charging_pile_ems_struct;

typedef struct {
    charging_pile_about_struct   charging_pile;
    charging_pile_meg_struct     charging_pile_meg;  //充电桩信息区
    charging_pile_set_struct     charging_pile_set;  //充电桩设置区    
    charging_pile_ems_struct     charging_pile_ems;  //充电桩配合EMS
} charging_pile_struct;

///**
// * @brief Type = 0x1B
// *
// */
//typedef struct {
//	union {
//		uint8_t all;
//
//		struct {
//			uint8_t enable : 2;
//			uint8_t reset : 2;
//			uint8_t res1 : 2;
//			uint8_t res2 : 2;
//		};
//	};
//} bat_maintain_t;

typedef struct {
//    uint8_t new_cfg;
    iot_config_struct   iot_config; // iot配置信息备份区
	uint8_t 			app_password[6];
	bat_maintain_t 		bat_maintain;
    iot_mode_struct     support_mode; //访客模式及蓝牙密码备份
} bk_iot_node_set;
//
//typedef struct {
//
//    int16_t *pgValue;   // modbus 一个寄存器
//    uint32_t mask;      //
//    int8_t shift;       // 整数左移，负数右移
//    bool is_lsb;        // 是否交换字节序
//
////    can_data_label can_label;
//}reg_array;

//
//typedef struct {
//	union {
////		uint16_t all[1000];//预留协议向上兼容
//
//		struct {
//		uint8_t revd[100];// 
//
//		};
//	};
//}MOD_STRUCT_reg15500;//CHARGER 1 信息区
//
//typedef struct {
//	union {
////		uint16_t all[1000];//预留协议向上兼容
//
//		struct {
//		uint8_t revd[100];// 
//
//		};
//	};
//}MOD_STRUCT_reg16000;//DCDC模块通用 信息区
//
//typedef struct {
//	union {
////		uint16_t all[1000];//预留协议向上兼容
//
//		struct {
//		uint8_t revd[100];// 
//
//		};
//	};
//}MOD_STRUCT_reg15600;//DCDC模块通用/CHARGER 1 设置区


//////////////////////////////////////////
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
        uint8_t ac_relay_opn : 1; // bit12 交流继电器开路
        uint8_t ac_relay_sht : 1; // bit13 交流继电器短路
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
        uint8_t aux_pwr_flt  : 1; // bit5 辅助电源故障
        uint8_t rsv6 : 1; // bit6 保留
        uint8_t rsv7 : 1;  // bit7 保留
        uint8_t off_grid_volt : 1; // bit8 离网电压异常
        uint8_t off_grid_freq : 1; // bit9 离网频率异常
        uint8_t grid_ol : 1; // bit10 交流过载
        uint8_t grid_ol_time_out : 1; // bit11 交流过载超时
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
        uint8_t pv1_snsr_flt  : 1; // bit12 电流1传感器异常
        uint8_t pv2_snsr_flt : 1; // bit13 电流2传感器异常
        uint8_t rsv14 : 1; // bit14
        uint8_t rsv15 : 1; // bit15
    }bit;
}sys_fault3_t;

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
        uint8_t par_can_flt  : 1; // bit10 并机CAN通讯故障
        uint8_t int_spi_flt  : 1; // bit11 内部SPI通讯故障
        uint8_t freq_sync_flt  : 1; // bit12 工频同步故障
        uint8_t car_sync_flt  : 1; // bit13 载波同步故障
        uint8_t mod_fan_flt  : 1; // bit14 模块风扇故障
        uint8_t rsv7 : 1; // bit15
    }bit;
}sys_fault4_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t dc_smp_err : 1; // bit0 dcout采样异常
        uint8_t rsv1 : 1; // bit1 DCDC和DSP出现故障
        uint8_t rsv2 : 1; // bit2 微逆和DSP出现故障
        uint8_t dsp_comm_err : 1; // bit3 DCDC通讯故障
        uint8_t extflash_err : 1; // bit4 外部flash故障
        uint8_t bat_can_err : 1; // bit5 电池组通讯故障
        uint8_t inv_err : 1; // bit6 逆变组通讯故障
        uint8_t rtc_err : 1; // bit7 RTC错误
        uint8_t eeprom_err : 1; // bit8 eeprom错误
        uint8_t bms_err : 1; // bit9 电池组故障
        uint8_t dc1_ov : 1; // bit10 dcout1过压
        uint8_t dc2_ov : 1; // bit11 dcout2过压
        uint8_t dc1_uv : 1; // bit12 dcout1欠压
        uint8_t dc2_uv : 1; // bit13 dcout2欠压
        uint8_t dc1_oc : 1; // bit14 dcout1过流
        uint8_t dc2_oc : 1; // bit15 dcout2过流
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
}sys_alarm2_t;

typedef union {
    uint16_t all;
}sys_alarm3_t;

typedef union {
    uint16_t all;
}sys_alarm4_t;

typedef struct {
    uint16_t grid_num;
    uint16_t inv_num;
    uint16_t mcu_num;
    uint16_t mcu_type;
    uint16_t time_ctrl_num;
}dsp_constant_t;

typedef struct {
    uint16_t pv_max_ntc;
    uint16_t inv_max_ntc; 
    _line_t energy_line;
    sys_alarm1_t alarm1;
    sys_alarm2_t alarm2;
    sys_alarm3_t alarm3;
    sys_alarm4_t alarm4;  
    sys_fault1_t fault1;
    sys_fault2_t fault2;
    sys_fault3_t fault3;
    sys_fault4_t fault4;
    sys_fault5_t fault5;
    uint16_t dsp_mode_enable;
} dsp_private_t;


// typedef struct {
//     uint16_t type; // 能量类型
//     uint16_t year; // 哪一年
//     uint32_t total_energy; // 总能量
//     uint32_t mouth_energy[12];  // 0-11每个月的总能量
//     uint16_t day_enengy[31];    // 0-30 每天的总能量
//     uint16_t crc16;
// }now_energy_t;

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



//typedef struct {
//    bool valid;
//    int timeout;
//    now_energy_t     show_now; // 
//    history_energy_t show_history; // 
//    dsp_constant_t  constant;
//    dsp_private_t   private; // mesh私有协议
//    dsp_eneryg_t    energy;
//    dsp_reg0000_t   reg0000;
//    dsp_reg0100_t   reg0100;
//    dsp_reg0300_t   reg0300;
//    dsp_reg0400_t   reg0400;
//    dsp_reg0500_t   reg0500;
//    dsp_reg0700_t   reg0700;
//    dsp_reg0800_t   reg0800;
//}dcdc_mi_data_t;

////////////////////////////////////////bind inv begin////////////////////////////////////////////////
typedef struct 
{
	//share 
	uint16_t modbus_version; //识别到的modbus协议版本：1- alpha;2-beta

	MOD_STRUCT_reg00000 mod_reg00000; //  6~16,

	//Alpha modbus
	md_alpha_reg0000_t alpha_inv_info_0000;
	md_alpha_reg3000_t alpha_set_03000;

	//Beta modbus
    MOD_STRUCT_reg00100 mod_reg00100_AppPage1; //100~168  APP首页数据（汇总）
//    MOD_STRUCT_reg00700 mod_reg00700_OTA; //700~768  OTA设置区
    MOD_STRUCT_reg01100 mod_reg01100_Inv_base; //1100~1154  每台逆变器基本信息
    MOD_STRUCT_reg01200 mod_reg01200_Inv_pv; //1200~1289  每台逆变器PV信息
    MOD_STRUCT_reg01300 mod_reg01300_Inv_grid; //1300~1330 每台逆变器电网信息 
    MOD_STRUCT_reg01400 mod_reg01400_Inv_load; //1400~1447  每台逆变器负载信息
//    MOD_STRUCT_reg01500 mod_reg01500_Inv_inv; //1500~1529  每台逆变器逆变信息
//    MOD_STRUCT_reg01600 mod_reg01600_Inv_generator; //1600~1613 每台逆变器发电机信息
    MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067  基础设置项
    MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区
}OTHER_INV_DATA_STRUCT;

////////////////////////////////////////other dev begin////////////////////////////////////////////////
typedef struct 
{
	//share 
	uint16_t modbus_version; //识别到的modbus协议版本：1- alpha;2-beta

	MOD_STRUCT_reg00000 mod_reg00000; //  6~16,

	//Alpha modbus
	md_alpha_reg0000_t alpha_inv_info_0000;
	md_alpha_reg3000_t alpha_set_03000;

	//Beta modbus
    MOD_STRUCT_reg00100 mod_reg00100_AppPage1; //100~168  APP首页数据（汇总）
//    MOD_STRUCT_reg00700 mod_reg00700_OTA; //700~768  OTA设置区
    MOD_STRUCT_reg01100 mod_reg01100_Inv_base; //1100~1154  每台逆变器基本信息
    MOD_STRUCT_reg01200 mod_reg01200_Inv_pv; //1200~1289  每台逆变器PV信息
    MOD_STRUCT_reg01300 mod_reg01300_Inv_grid; //1300~1330 每台逆变器电网信息 
    MOD_STRUCT_reg01400 mod_reg01400_Inv_load; //1400~1447  每台逆变器负载信息
    MOD_STRUCT_reg01500 mod_reg01500_Inv_inv; //1500~1529  每台逆变器逆变信息
//    MOD_STRUCT_reg01600 mod_reg01600_Inv_generator; //1600~1613 每台逆变器发电机信息
    MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067  基础设置项
    MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区
//    MOD_STRUCT_reg02300 mod_reg02300_Inv_set02_struct; //7000~7004  pack设置区
//    
//    MOD_STRUCT_reg02400 mod_reg02400_Inv_certification; //2400~2449  认证设置项
//    MOD_STRUCT_reg02500 mod_reg02500_Inv_advance_set2; //2500~2544  每台逆变高级设置区2（面向微逆）
//    MOD_STRUCT_reg03000 mod_reg03000_Inv_history; //3000~3026  每台逆变器历史记录区
//    MOD_STRUCT_reg03500 mod_reg03500_Inv_yearX_statistic; //3500~3547    每台逆变器累计年电量区
//    MOD_STRUCT_reg03600 mod_reg03600_Inv_year1_statistic; //3600~3658  每台逆变器当年电量区
//    MOD_STRUCT_reg04000 mod_reg04000_Dsp_data;//4000~4049 dsp数据区
//    MOD_STRUCT_reg04050 mod_reg04050_Dsp_set1;//4050~4104 dsp设置区1
//    MOD_STRUCT_reg04105 mod_reg04105_Dsp_set2;//4105~4177 dsp设置区2
    MOD_STRUCT_reg06000 mod_reg06000_Pack_sum; //6000~6031    pack主包汇总信息
//    MOD_STRUCT_reg06100 mod_reg06100_Pack_each; //6100~6203  pack单包信息
//    MOD_STRUCT_reg07000 mod_reg07000_Pack_set; //7000~7004  pack设置区

	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
//	MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
//	MOD_STRUCT_reg15500 mod_reg15500_DCDC_info;//16000~16040	DCDC模块通用 信息区
//	MOD_STRUCT_reg15600 mod_reg15600_DCDC_set;//15600~15640	DCDC模块通用 设置区
//    MOD_STRUCT_reg13500 mod_reg13500_mesh; //13500~13511   WIFI MESH区
    
//	MOD_STRUCT_reg21000	mod_reg21000_data;//绑定帧的读写不一样
//	MOD_STRUCT_reg21000_WR	mod_reg21000_bind_WR;
	
//	MOD_STRUCT_reg22000 mod_reg22000_net_server_2rd;
//  MOD_STRUCT_reg29700 mod_reg29700_IOT_info;//29700~29720 IOT标定区（从模式写入）
}OTHER_INV_BETA_STRUCT;


////////////////////////////////////////other dev end////////////////////////////////////////////////

#pragma pack()
#endif
