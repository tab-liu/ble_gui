#ifndef PACK_TYPE_H
#define PACK_TYPE_H

#include <stdint.h>
#include "product_define.h"

#define PACK_CAN_ADDR           COMMON_PACK_BMS_CAN_ADDR
#define PACK_INGROUP_MAX_NUM    COMMON_PACK_INGROUP_MAX_NUM	//一个逆变器对应的电池包群组内的包数量
#define PACK_GROUP_NUM          COMMON_PACK_GROUP_MAX_NUM   //系统内电池包的群组数量
#define PACK_MAX_NUM            COMMON_PACK_MAX_NUM


#define PACK_ANNOUNCE1          0x9B
#define PACK_ANNOUNCE2          0x9C
#define PACK_ANNOUNCE3          0x9D
#define PACK_ANNOUNCE4          0x9E
#define PACK_ANNOUNCE5          0x9F
#define PACK_ANNOUNCE6          0xA0
#define PACK_ANNOUNCE7          0x65//2023.5新增 CAN ID单帧
#define PACK_ANNOUNCE8          0x66
#define PACK_ANNOUNCE9          0x67
#define PACK_ANNOUNCE6D         0x6D
#define PACK_ANNOUNCE6E         0x6E
#define PACK_ANNOUNCE6F         0x6F


enum {
    PACK_TYPE_ANNOUNCE_50H = 0x50,
    PACK_TYPE_BASE_51H = 0x51,
    PACK_TYPE_EXTEND_52H = 0x52,
    PACK_TYPE_NUKONW_53H = 0x53,
    PACK_TYPE_ABOUT_54H = 0x54,
    PACK_TYPE_CONFIG_55H = 0x55,
    PACK_TYPE_DEBUG1_56H = 0x56,
    PACK_TYPE_BMUSTATE_57H = 0x57,
    PACK_TYPE_BMU_DETAIL_INFO_58H = 0x58,
    PACK_TYPE_BMU_BASIC_INFO_59H = 0x59,
    PACK_TYPE_BA_ABOUT_INFO_5AH = 0x5A,
    PACK_TYPE_BMS_PAYGO_INFO_5BH = 0x5B,
};

typedef enum {
    PACK_WORK_INIT =0x0,  //初始化
    PACK_WORK_STANDYBY =0x11,  //待机       
    PACK_WORK_RUN =0x22,  //运行
    PACK_WORK_POWEROFF =0x33,  //关机                     
    PACK_WORK_SELF_TEST =0x44,  //自检
}pack_work_state;

#pragma pack(1)
typedef union {
    uint8_t all;
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
}status_byte1;

typedef union {
    uint8_t all;
    struct {
        uint8_t chg_alarm : 1; // 1：充电告警；0：消除
        uint8_t dsg_alarm : 1; // 1：放电告警；0：消除
    }bit;
}status_byte2;

typedef union {
    uint8_t all;
    struct {
        uint8_t status : 4;
        uint8_t progress : 4;
    };
}aging_status_t;

typedef struct {
    uint8_t func_code;         // Byte0  功能码标志（如0x80）
    uint8_t app_ver_low8;      // Byte1  APP版本低8位
    uint8_t app_ver_midlow8;   // Byte2  APP版本中低8位
    uint8_t app_ver_midhigh8;  // Byte3  APP版本中高8位
    uint8_t app_ver_high8;     // Byte4  APP版本高8位
    uint8_t boot_ver_low8;     // Byte5  Boot版本低8位
    uint8_t boot_ver_midlow8;  // Byte6  Boot版本中低8位
    uint8_t dual_boot_flag;    // Byte7  双Boot标志，1=双Boot，0=单Boot
} __attribute__((packed)) version_frame_t;

typedef struct {
    uint8_t func_code;    // Byte0 功能码（81）
    union {
        uint8_t value;
        struct {
            uint8_t key_on_flag   : 1; // Bit0 按键开机标志 1是 0否
            uint8_t limit_charge  : 1; // Bit1 限流充电 1限流 0无效
            uint8_t reserved2     : 1; // Bit2 预留
            uint8_t reserved3     : 1; // Bit3 预留
            uint8_t reserved4     : 1; // Bit4 预留
            uint8_t reserved5     : 1; // Bit5 预留
            uint8_t reserved6     : 1; // Bit6 预留
            uint8_t reserved7     : 1; // Bit7 预留
        };
    } flags;              // Byte1
    uint8_t reserved[6];  // Byte2~7 预留
} __attribute__((packed)) status_flag_frame_t;

typedef struct {
    uint8_t func_code; // data[0]

    union {
        uint8_t value;
        struct {
            uint8_t buzzer_enable    : 1; // bit0 蜂鸣器状态，1-enable，0-disable
            uint8_t sleep_status     : 1; // bit1 是否进入休眠，1-是，0-否
            uint8_t key_on_flag      : 1; // bit2 按键开机标志，1-按，0-非按
            uint8_t smart_charge     : 1; // bit3 智能充电模式，1-智能充电，0-其他
            uint8_t reserved         : 4; // bit4~7 预留
        };
    } status; // data[1]

    uint8_t sleep_soc; // data[2] 休眠SOC值

    union {
        uint8_t value;
        struct {
            uint8_t ac_switch      : 2; // bit0~1 AC物理开关 0-无效，1-物理按键开，2-物理按键关
            uint8_t reserved       : 6; // bit2~7 预留
        };
    } key_ac_status; // data[3]

    uint8_t reserved2[4]; // data[4]~data[7] 预留
} __attribute__((packed)) bms_status_resp_t;

/**
 * @brief 0x50
 *
 */
typedef struct {

    uint16_t total_voltage; // 0.1V  总电压
    uint16_t total_current; // 0.1A  总电流
    uint8_t soc;            // 1% 系统电流
    uint8_t avg_temp;       // 平均温度
    uint8_t work_status;    // 工作状态
    uint8_t chg_status;     // 充电状态

    uint16_t max_chg_voltage; // 最大充电电压
    uint16_t max_chg_current; // 为0时，不能充电
    uint16_t max_dsg_current; // 为0时，不能放电
    status_byte1 status1;     // 状态标记位1
    status_byte2 status2;     // 状态标记位2

    uint8_t soh;        // 0-100 1%
    uint16_t online;    // 最大并机16台，响应的位置1表示，在线 - 通讯并机，bit0~15依次表示不同bat状态;从 CAN ID 0x189dXXYY获取
    uint16_t cap_online; // 最大并机16台，响应的位置1表示，主mos输出打开 - 容量并机
    uint8_t pack_cnt;   // pack并机数量
    uint16_t soc2;       // 0-1000 0.1%,与soc是10倍关系

    uint16_t chg_full_time;  // 预计放空时间
    uint16_t dsg_empty_time; // 预计充满时间
    uint8_t is_high_volt;  // 所属电压平台
	uint16_t TotalCurrent_bias;
	uint8_t BcuBalanceSumPw;

    uint16_t cell_min_volt;
    uint16_t cell_max_volt;
    uint16_t BCU_State;
    uint16_t PACK_TEST;

	uint16_t protect_status1;
	uint16_t protect_status2;
	uint8_t res3[4];

	aging_status_t aging_status;
	uint16_t aging_current;
	uint8_t maintain_fault; // 维护故障
	uint8_t usb_power;      // USB功率
	uint8_t usb_current;    // USB电流
	uint8_t res4[2];

	uint8_t res5[8];

	uint8_t res6[8];

    iot_date_t pack_date; // 电池包日历，PACK主动上报，实时数据
    
    bms_status_resp_t pack_set_status; // 电池包设置状态
    version_frame_t pack_ver;
    status_flag_frame_t pack_flag;
    
    rw_cmd_struct   rw_cmd;
}pack_announce_struct;

typedef union {
    uint16_t all;
    struct {
        uint16_t cell_cnt : 8; // BMU中的电芯数量
        uint16_t temp_cnt : 8; // BMU中的温度数量
    }bit;
} bmu_count_t;

/**
 * @brief 0x51
 *
 */
typedef struct {
    uint16_t total_voltage; // 0.1V  单pack电压
    uint16_t total_current; // 0.1A  单pack电流
    uint8_t  soc;           // 单pack soc
    uint8_t  soh;           // 单pack soh
    uint8_t  avg_temp;      // 平均温度
    uint16_t min_cell_voltage; // 最小电芯电压
    uint16_t max_cell_voltage; // 最大电芯电压
    uint8_t  min_cell_index;    // 最小电芯位置
    uint8_t  max_cell_index;    // 最大电芯位置
    uint8_t  min_temp_value;    // 最小温度值
    uint8_t  max_temp_value;    // 最大温度值
    uint8_t  min_temp_index;    // 最小温度位置
    uint8_t  max_temp_index;    // 最大温度位置
    uint8_t  work_status;       // 工作状态
    uint8_t  chg_status;        // 充电状态
    uint8_t protect[4];         // pack 保护
    uint8_t fault[6];           // pack 故障
    uint8_t alarm[2];           // pack 告警
    uint8_t relay;              // 继电器状态：主继电器Bit0-Bit1、预充继电器Bit2-Bit3、加热继电器Bit4-Bit5.
    uint8_t  pack_cap_online;     // pack 容量并机状态
    uint16_t pack_total_cell;   // 电芯总数
    uint8_t  pack_total_ntc;    // NTC总数
    uint16_t chg_full_time;     // 预计充满时间
    uint16_t dsg_empty_time;    // 预计放空时间
	uint16_t allow_max_chg_voltage;		// 允许最大充电电压
	uint16_t allow_max_chg_current;		// 允许最大充电电流
	uint16_t allow_max_dsg_current;		// 允许最大放电电流
	uint16_t protect_status;			// 保护状态
	uint32_t vbus;						// 外部端口电压
	uint16_t ibus;						// 端口电流
	uint16_t stop_reason;				// 关机原因
	uint8_t min_cell_bcu_idx;
	uint8_t max_cell_bcu_idx;
	uint8_t min_temp_bcu_idx;
	uint8_t max_temp_bcu_idx;
	uint8_t bmu_nums;
	uint16_t DCDC_project;
    uint16_t DCDC_fault;
    uint8_t revd[4];
    uint16_t BMUOnline;
    uint16_t BMUFaultOnline;
    uint16_t protect_status2;			// 保护状态
    uint8_t usb_power;   // USB功率
    uint8_t usb_voltage; // USB电压
    uint8_t usb_current; // USB电流
    uint8_t usb_fault;   // USB故障

    uint16_t revd1[15];
    
    rw_cmd_struct   rw_cmd;
}pack_base_struct;


/**
 * @brief 0x52
 *
 */
typedef struct {
    uint32_t capacity; // 0.001Ah 电池包容量
    uint32_t total_chg_energy; // 1Wh 电池累计充电能量
    uint32_t total_dsg_energy; // 1Wh 电池累计放电能量
    uint32_t once_chg_energy; // 1Wh 电池当前充电能量
    uint32_t once_dsg_energy; // 1Wh 电池当前放电能量
    uint16_t sir_value;     // 0.1kR 绝缘包内阻
    uint16_t inner_value; // 1mR 电池包内阻
    uint16_t cycle_count; // 循环次数
    uint32_t MaxPackHistoryChgEnergy; // 单次最大历史充电容量
    uint32_t MaxPackHistoryDsgEnergy; // 单次最大历史放电容量
    uint16_t revd[10];
    rw_cmd_struct   rw_cmd;
}pack_extend_struct;



/**
 * @brief 0x54
 *
 */
typedef struct {
    char    type_ascii[12];
    uint64_t sn_code;
    uint8_t software_total; // 软件版本总数量（相同版本只显示最低版本号）
    can_soft_version soft[10]; // 20组软件版本信息
    rw_cmd_struct   rw_cmd;
}pack_about_struct;

typedef union {
    uint16_t all;
    struct {
        uint16_t revd1 : 2;
        uint16_t dc_on : 2;
        uint16_t revd2 : 12;
    }bit;
}bms_switch_t;

/**
 * @brief 0x55
 *
 */
#define	PACK_CONFIG_STRUCT_COMMON_0x55 \
    uint8_t pack_heat_enable;   /* pv接入时是否开启加热功能 0：关闭；1：开启 */\
    uint8_t ctr_heat_enable;    /* 是否开启加热电池最佳温度功能0：关闭；1：开启 （开启此功能App需要提示可能会消耗电池电）*/\
    uint8_t unlock_failed_flags;/* 电池包因故障锁，重启不恢复，下发命令解锁电池包。普通用户应无此权限*/\
    uint8_t max_parallel_nums;/* 每套电池包的最大支持数量*/\
    uint16_t on_off_set;/*7005 开关设置（掉电不保存设置项）*/\
    uint32_t ReservationTime;/*7006~7007 预约加热时间*/\
    uint16_t NormalTempSocMinLimit;/*7008 常温SOC放电下限*/\
    uint16_t LowTempSocMinLimit;/*7009 常温SOC放电下限*/\
    uint16_t BMS_Protol_Type;/*7010 BMS对外协议类型和接口配置*/\
    uint16_t BatteryChargingPower;/* 7011 电池充电功率设置*/\
    uint16_t BatteryDischargingPower;/* 7012 电池放电功率设置*/\
    uint16_t revd[10];

typedef struct {
//    uint8_t pack_heat_enable;   // pv接入时是否开启加热功能 0：关闭；1：开启
//    uint8_t ctr_heat_enable;    // 是否开启加热电池最佳温度功能0：关闭；1：开启 （开启此功能App需要提示可能会消耗电池电）
//    uint8_t unlock_failed_flags;// 电池包因故障锁，重启不恢复，下发命令解锁电池包。普通用户应无此权限
//    uint8_t max_parallel_nums;// 每套电池包的最大支持数量
	PACK_CONFIG_STRUCT_COMMON_0x55
    rw_cmd_struct   rw_cmd;
}pack_config_struct;

typedef struct {
	PACK_CONFIG_STRUCT_COMMON_0x55

} pack_config_0x55_struct_mini;	

/**
 * @brief 0x56
 *
 */
typedef union {
    uint16_t value;
    struct {
        uint16_t volt : 14;
        uint8_t flag : 2;
    };
}cell_struct;

typedef union {
    struct {
        int16_t ntc1 : 8;
        int16_t ntc2 : 8;
    };
}ntc_struct;

typedef struct {
	uint16_t cells;
	uint16_t ntc_cnt;
    uint16_t payload[384];
    rw_cmd_struct   rw_cmd;
}pack_debug1_struct;

/**
 * @brief 0x5B
 *
 */
typedef struct __attribute__((packed)) {
    uint32_t PAYG_ActCodeInput;       /* 激活码输入（9位数字），例如 123456789 */
    uint16_t PAYG_AppClearData;       /* 清除数据：0=无，1=全部清空，2=只清除电池累计电量(预留) */
    uint16_t PAYG_ActCodeCount;       /* 激活码生成序列号（示例：001,002） */
    uint16_t PAYG_ActCodeRemind;      /* 激活码输入提示信息，取值 1~10（参见定义） */
    uint16_t PAYG_ActState;           /* 激活状态：0=过期,1=激活,2=永久激活,3=重卡电池类激活 等 */
    uint16_t PAYG_ActDaysRemain;      /* 剩余天数（整数天） */
    uint32_t PAYG_LastActTime;        /* 最近一次输入激活码记录的 RTC 时间（Unix 时间戳） */
    uint32_t PAYG_LastActCode;        /* 最近一次输入的激活码（若以数值保存） */
    uint16_t PAYG_ActDaysRemainHours; /* 剩余小时（不含整天） */
    uint16_t PAYG_ActDaysRemainMin;   /* 剩余分钟（不含整小时） */
    uint32_t CurrentTime;             /* 当前保存的时间戳（Unix） */
    uint16_t PavGoActCodeType;        /* 激活码输入类型：0=无,1=手动,2=服务器下发 等 */
    uint16_t Lock_Remaining_Hours;    /* 锁定剩余小时 */
    uint16_t Lock_Remaining_Min;      /* 锁定剩余分钟 */
    uint16_t Err_Cnt;                 /* 剩余错误次数/错误计数 */
    uint32_t PayGo_ID;                /* 用于显示设备的 paygo id */
    uint16_t revd[10];
    rw_cmd_struct   rw_cmd;
} bms_paygo_info_t;


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
    uint16_t BcuBalanceSumPw;//6033 电池均衡功率
    uint16_t FireNum;//6034 打火次数
    uint16_t PackCycle;//6035 PACK循环次数
    uint32_t PackProtect;//6036~6037 Pack保护
    uint16_t PackSysErr[3];//6038~6040 故障---BMS关闭mos
    uint16_t PackAlarm;//6041 告警
    uint16_t HeartCur;// 6042 加热电流（服务器记录）
    uint16_t NumOfBatAllowedToDis; // 6043 接收BMS发送的“允许放电的电池包数量”
    uint32_t CombinedCapacity;  // 6044~6045 并包总容量
    uint16_t revd[10];//6046~6055
}MOD_STRUCT_reg06000;

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
    uint32_t protect;         // pack 保护              //6144~6145
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
    uint16_t alarm2;
    uint16_t alarm3;
	
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
    modbus_soft_version soft[10]; // 10组软件版本信息
    uint16_t revd[10];
}MOD_STRUCT_reg06100;


// AC380假设 N == 16(最大)
typedef struct {
    uint16_t PackCellCnt;                      //6300          Pack中总的电芯数量
    uint16_t PackNTCCnt;                        //6301          Pack中总的NTC数控
    uint8_t payload[DEBUG_REG_NUM];//cell_ntc_struct PackCellNTC[16];
}MOD_STRUCT_reg06300;

typedef struct {
    uint16_t SetPackID;   //7000 用于选择Pack
    uint16_t SetPvHeat;   //7001
    uint16_t SetPackHeat; //7002
    uint16_t SetPackUnlock;//7003
    uint16_t PACK_Parallel_InOneGroup;//7004
    uint16_t on_off_set;//7005 开关设置（掉电不保存设置项）
    uint32_t ReservationTime;//7006~7007 预约加热时间
    uint16_t NormalTempSocMinLimit;//7008 常温SOC放电下限
    uint16_t LowTempSocMinLimit;//7009 常温SOC放电下限
    uint16_t BMS_Protol_Type;//7010 BMS对外协议类型和接口配置
    uint16_t BatteryChargingPower;// 7011 电池充电功率设置
    uint16_t BatteryDischargingPower;// 7012 电池放电功率设置
    uint16_t revd[10];
}MOD_STRUCT_reg07000;

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

typedef struct {
    /* 30000 */ uint16_t revd1;
    /* 30001 */ uint32_t PAYG_ActCodeInput;        // 激活码输入 (u32, 占 30001-30002)
    /* 30003 */ uint16_t PAYG_AppClearData;        // 清除数据 (u16)
    /* 30004 */ uint16_t PAYG_ActCodeCount;        // 激活码序列号 (u16)
    /* 30005 */ uint16_t PAYG_ActCodeRemind;       // 激活码提示 (u16)
    /* 30006 */ uint16_t PAYG_ActState;            // 激活状态 (u16)
    /* 30007 */ uint16_t PAYG_ActDaysRemain;       // 剩余天数（天） (u16)
    /* 30008 */ uint16_t PAYG_SOH;                 // SOH (%) (u16)
    /* 30009 */ uint16_t PAYG_SOC;                 // SOC (%) (u16)
    /* 30010 */ uint32_t PAYG_SumPowerOutput;      // 电池累计输出电量 (u32, 占 30010-30011)
    /* 30012 */ uint32_t PAYG_LastActTime;         // 最近一次激活时间 (u32, 起始表项为30012)
    /* 30014 */ uint32_t PAYG_LastActCode;         // 最近一次激活码 (u32, 起始表项为30014)
    /* 30016 */ uint16_t PAYG_ActDaysRemainHours;  // 剩余小时 (u16)
    /* 30017 */ uint16_t PAYG_ActDaysRemainMin;    // 剩余分钟 (u16)
    /* 30018 */ int32_t  GPS_longitude;            // 经度 (s32, 占 30018-30019)
    /* 30020 */ int32_t  GPS_latitude;             // 纬度 (s32, 占 30020-30021)
    /* 30022 */ uint32_t current_saved_timestamp;  // 当前保存的时间戳 (u32, 起始表项为30022)
    /* 30024 */ uint16_t ACT_CODE_TYPE;            // 激活码输入类型 (u16)
    /* 30025 */ uint16_t Lock_Remaining_Hours;     // 锁定剩余小时 (u16)
    /* 30026 */ uint16_t Lock_Remaining_Min;       // 锁定剩余分钟 (u16)
    /* 30027 */ uint16_t Err_Cnt;                  // 错误计数 (u16)
    /* 30028 */ uint32_t PayGo_ID;                 // PayGo ID (u32, 起始表项为30028)

    /* 30030-30047 reserved area */
    uint16_t revd2[18];

    /* 30048 */ uint16_t DEVICE_LOCK;              // 设备锁定状态 (u16)
    /* 30049 */ uint16_t SPECIAL_FORCE_START;      // 服务器下发设置 (u16)
    /* 30050 */ uint16_t REPORT_DATA_NUMBER;       // 上报数据次数 (u16)
    /* 30051 */ uint16_t WAKEUP_4G_TIME;           // 唤醒4G时间间隔 (min) (u16)
    /* 30052 */ uint16_t WORKING_TIME;             // 4G模块持续工作时间 (min) (u16)
    
    /* 30053-30099 reserved area */
    uint16_t revd3[47];
} MOD_STRUCT_reg30000;

#pragma pack()

#endif
