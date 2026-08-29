#ifndef PARTS_TYPE_H
#define PARTS_TYPE_H

#include <stdint.h>
#include "can_type.h"
#include "iot_define.h"

#define DC_HUB_CAN_ADDR        0xe0 //dchub地址
#define AC_HUB_CAN_ADDR        0xef //achub地址
#define DC_HUB_MAX_NUM            1
#define AC_HUB_MAX_NUM            1
#define DC_HUB_GROUP_MAX_NUM   DC_HUB_MAX_NUM	//一个逆变器系统对应的dchub数量
#define AC_HUB_GROUP_MAX_NUM   AC_HUB_MAX_NUM	//

#define PANEL_ARM_CAN_ADDR      0xA1 //0xA1~0xA3
#define PANEL_MAX_NUM           1
#define PANEL_GROUP_MAX_NUM     PANEL_MAX_NUM
#define PANEL_SMART_DC_NUM      6
#define PANEL_SMART_AC_NUM      4

#define HMI_CAN_ADDR            0x02
#define HMI_MAX_NUM             1
#define HMI_GROUP_MAX_NUM       HMI_MAX_NUM

#define DCDC_ARM_CAN_ADDR        0x25
#define DCDC_IOT_CAN_ADDR        0x2F
#define DCDC_MAX_NUM     1

#define DCHUB_ANNOUNCE1        0x21
#define DCHUB_ANNOUNCE2        0x22
#define DCHUB_ANNOUNCE3        0x23
#define DCHUB_ANNOUNCE4        0x24
#define DCHUB_ANNOUNCE5        0x25
#define DCHUB_ANNOUNCE6        0x26
#define DCHUB_ANNOUNCE7        0x27
#define DCHUB_ANNOUNCE8        0x28
#define DCHUB_ANNOUNCE9        0x29

enum {
    MODULE_TYPE_DC_AC_HUB_SET_40H = 0x40,
    MODULE_TYPE_DC_HUB_INFO_41H = 0x41,
    MODULE_TYPE_AC_HUB_INFO_42H = 0x42,
    MODULE_TYPE_PANEL_SET_43H = 0x43,
    MODULE_TYPE_PANEL_DC_INFO_44H = 0x44,
    MODULE_TYPE_PANEL_AC_INFO_45H = 0x45,
    MODULE_TYPE_DCDC_INFO_48H = 0x48,
    MODULE_TYPE_DCDC_SET_49H = 0x49,
    MODULE_TYPE_PANEL_SET_CTRL_INFO_4AH = 0x4A,
    MODULE_TYPE_POWER_RELAY_SET1_4BH = 0x4B,
    MODULE_TYPE_POWER_RELAY_SET2_4CH = 0x4C,
    MODULE_TYPE_POWER_RELAY_SET3_4DH = 0x4D,
};


#pragma pack(1)

typedef union {
    uint16_t all;
    struct {
        uint8_t dc_switch           : 2; // dc开关
        uint8_t dc_saveswitch       : 2; // 记忆开关
        uint8_t dc_reset_factory    : 2; // 恢复出厂设置
        uint8_t revd                : 2; // 预留

        uint8_t dc_set_voltage;          // 电压设置值（bit0/1为关的时候可以设置电压值）
    }bit;
}dc_hub_set;

typedef union {
    uint16_t all;
    struct {
        uint8_t ac_switch           : 2; // ac开关
        uint8_t ac_saveswitch       : 2; // 记忆开关
        uint8_t ac_reset_factory    : 2; // 恢复出厂设置
        uint8_t revd1               : 2; // 预留

        uint8_t revd2;                   // 预留
    }bit;
}ac_hub_set;

/**
 * @brief Type = 0x40
 *
 */
typedef struct {
    dc_hub_set dc_hug_setting;
    ac_hub_set ac_hug_setting;
    rw_cmd_struct rw_cmd;
}dc_ac_hub_set;


/**
 * @brief Type = 0x41
 *
 */
typedef struct {
    char dc_hub_type[12];               // DC_HUB型号
    uint64_t dc_hub_sn;                 // DC_HUB序列号
    /* 以下数据由dchub主动上报 */
    uint16_t input_power;               // 输入功率 1w
    uint16_t input_voltage;             // 输入电压 0.1V
    /* END */
    uint16_t input_current;             // 输入电流 0.01A
    uint16_t output_power;              // 输出总功率 1w
    uint16_t output_voltage;            // 输出电压 0.1V
    uint16_t output_current;            // 输出电流 0.01A
    uint16_t cig1_output_power;         // 点烟器1输出功率 1w
    uint16_t cig1_output_voltage;       // 点烟器1输出电压 0.1V
    uint16_t cig1_output_current;       // 点烟器1输出电流 0.01A
    uint16_t cig2_output_power;         // 点烟器2输出功率 1w
    uint16_t cig2_output_voltage;       // 点烟器2输出电压 0.1V
    uint16_t cig2_output_current;       // 点烟器2输出电流 0.01A
    uint16_t USB_A_output_power;        // USB-A输出功率 1w
    uint16_t USB_A_output_voltage;      // USB-A输出电压 0.1V
    uint16_t USB_A_output_current;      // USB-A输出电流 0.01A
    uint16_t Type_C_output_power;       // Type-C输出功率 1w
    uint16_t Type_C_output_voltage;     // Type-C输出电压 0.1V
    uint16_t Type_C_output_current;     // Type-C输出电流 0.01A
    uint16_t Type_C2_output_power;      // Type-C2输出功率 1w
    uint16_t Type_C2_output_voltage;    // Type-C2输出电压 0.1V
    uint16_t Type_C2_output_current;    // Type-C2输出电流 0.01A
    uint16_t Anderson_output_power;     // 安德森输出功率 1w
    uint16_t Anderson_output_voltage;   // 安德森输出电压 0.1V
    uint16_t Anderson_output_current;   // 安德森输出电流 0.01A
    rw_cmd_struct rw_cmd;
}dc_hub_info_struct;

/**
 * @brief Type = 0x42
 *
 */
typedef struct {
    // uint16_t ac_hub_type[6];            // AC_HUB型号
    // uint16_t ac_hub_sn[4];              // AC_HUB序列号
    char ac_hub_type[12];            // AC_HUB型号
    uint64_t ac_hub_sn;              // AC_HUB序列号
    uint8_t SoftwareType;               // AC_HUB软件类型
    uint32_t SoftwareVersion;           // AC_HUB软件版本
    uint8_t revd[20];
    /* 以下数据由achub主动上报，Temperature暂时没有 */
    uint16_t InvVoltageL1;              // L1相逆变电压 0.01V
    uint16_t InvVoltageL2;              // L2相逆变电压 0.01V
    uint16_t InvVoltageL3;              // L3相逆变电压 0.01V
    uint16_t Volt_grid_L1;              // L1相电网电压 0.01V
    uint16_t Volt_grid_L2;              // L2相电网电压 0.01V
    uint16_t Volt_grid_L3;              // L3相电网电压 0.01V
    uint16_t Power_load_L1;             // L1相负载功率 1w
    uint16_t Power_load_L2;             // L2相负载功率 1w
    uint16_t Power_load_L3;             // L3相负载功率 1w
    uint16_t Power_grid_L1;             // L1相电网功率 1w
    uint16_t Power_grid_L2;             // L2相电网功率 1w
    uint16_t Power_grid_L3;             // L3相电网功率 1w
    uint16_t GridFrequency;             // 电网频率
    uint16_t Temperature;               // 温度
    uint16_t alarm_State;               // 告警状态
    rw_cmd_struct rw_cmd;
}ac_hub_info_struct;

typedef struct __attribute__((packed)) {
    // 周期报文1（DC向ARM汇报）
    uint16_t DCLoadTotalPower; /* DC总负载功率，单位：1 W（直接即瓦特） */
    uint16_t DCLoadTotalVolt;  /* DC总负载电压，单位：0.1 V（物理电压 = raw / 10.0 V） */
    uint16_t DCLoadTotalCurr;  /* DC总负载电流，单位：0.01 A（物理电流 = raw / 100.0 A） */
    uint16_t Cigar1Power;      /* 点烟器1输出功率，单位：1 W（直接即瓦特） */
    // 周期报文2（DC向ARM汇报）
    uint16_t Cigar1Volt;   /* 点烟器1输出电压，原始单位：0.1 V */
    uint16_t Cigar1Curr;   /* 点烟器1输出电流，原始单位：0.01 A */
    uint16_t Cigar2Power;  /* 点烟器2输出功率，原始单位：1 W */
    uint16_t Cigar2Volt;   /* 点烟器2输出电压，原始单位：0.1 V */
    // 周期报文3（DC上报ARM）
    uint16_t Cigar2Curr;   /* 点烟器2输出电流，原始单位：0.01 A */
    uint16_t USBAPower;    /* USBA 输出功率，原始单位：1 W */
    uint16_t USBAVolt;     /* USBA 输出电压，原始单位：0.1 V */
    uint16_t USBACurr;     /* USBA 输出电流，原始单位：0.01 A */
    // 周期报文4（DC上报ARM）
    uint16_t PD1Power; /* PD1 输出功率，单位：1 W */
    uint16_t PD1Volt;  /* PD1 输出电压，单位：0.1 V */
    uint16_t PD1Curr;  /* PD1 输出电流，单位：0.01 A */
    uint16_t PD2Power; /* PD2 输出功率，单位：1 W */
    // 周期报文5（DC上报ARM）
    uint16_t PD2Volt;  /* PD2 输出电压，原始单位：0.1 V */
    uint16_t PD2Curr;  /* PD2 输出电流，原始单位：0.01 A */
    uint16_t RVPower;  /* 安德森（RV）输出功率，原始单位：1 W */
    uint16_t RVVolt;   /* 安德森（RV）输出电压，原始单位：0.1 V */
    // 周期报文6（DC上报ARM）
    union {
        uint8_t status;
        struct {
            uint8_t DCShort     : 1; /* bit0  1 - DC 输出短路 */
            uint8_t DCOverVolt  : 1; /* bit1  1 - DC 输出过压 */
            uint8_t DCOverCurr  : 1; /* bit2  1 - DC 输出过流 */
            uint8_t DCOverTemp  : 1; /* bit3  1 - DC 输出过温 */
            uint8_t DCOutError  : 1; /* bit4  1 - DC 输出故障 */
            uint8_t DCOverLoad  : 1; /* bit5  1 - DC 输出过载 */
            uint8_t DCOutState  : 1; /* bit6  1 - DC 输出状态：1 有输出，0 无输出 */
            uint8_t reserved    : 1; /* bit7  保留 */
        } status_bits;
    };
    uint16_t PreCVolt;    /* 预充电压，原始 0-65535（建议物理单位: 0.1V，若不同请调整） */
    uint16_t BatteryVolt; /* 电池包输入电压，原始 0-65535（建议物理单位: 0.1V） */
    uint16_t RVCurr;      /* 安德森输出电流，原始单位 0.01A */
    uint8_t  ext8_1;      /* 额外 8 位保留/扩展字段（0-255） */
    // 周期报文7（DC上报ARM）
    uint64_t DCHubSN;     /* 设备SN */
    // 事件报文（DC上报ARM）
    union {
        uint8_t event;
        struct {
            uint8_t DCOnOffState : 1; /* bit0: DC 开关状态: 0=关闭, 1=开启 */
            uint8_t DC24VState   : 1; /* bit1: DC24V 状态: 0=12V, 1=24V */
            uint8_t UnnormalKey  : 1; /* bit2: 童锁使能时是否出现不被允许的按键操作: 0=否,1=是 */
            uint8_t reserved_bits: 5; /* bit3-7 保留 */
        } event_bits;
    };
    uint16_t reserved16_1; /* 预留 16 位字段 */
    uint16_t reserved16_2; /* 预留 16 位字段 */
    uint8_t  ext8_2;       /* 预留 8 位字段 */
    uint8_t  ext8_3;       /* 预留 8 位字段 */
    uint8_t  ext8_4;       /* 预留 8 位字段 */
    // 周期报文8（DC上报ARM）
    uint32_t SoftVersion;  /* 软件版本 */
    uint16_t reserved1;    /* 预留 */
    uint16_t reserved2;    /* 预留 */
}dc_hub_announce_struct;

typedef struct {
    uint8_t             devId;         // 当前数据属于哪个节点
    uint8_t             online;//1-设备在线；0-超时清零
    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
/*

并机排序之后才set，排地址时候清零
	*/    
    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

    ac_hub_info_struct  ac_hub_info;    // AC_HUB信息
}ac_hub_can_node_struct;



typedef struct {
    uint8_t             devId;         // 当前数据属于哪个节点
    uint8_t             online;//1-设备在线；0-超时清零
    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
    
    uint8_t             dev_access_state;   // 设备接入标志，用于设备接入日志模块 0:无效/离线，1：接入
   
    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

#if 0   // 使用私有协议
    dc_hub_info_struct  dc_hub_info;    // DC_HUB信息
    dc_ac_hub_set       dc_ac_hub_setting;
#else
    dc_hub_announce_struct dc_hub_info;
#endif
}dc_hub_can_node_struct;

typedef struct {
    dc_ac_hub_set       dc_ac_hub_setting;
}dc_ac_hub_can_node_wr_struct;


typedef union {//8*2=16个DC开关
    /*0-invalid;1-enable;2-disable）;
    (为了安全，只具备强制断开功能，不能强制闭合)
    （1-命令闭合，命令、保护、SOC与逻辑才是执行结果；2-命令断开，或逻辑）*/
    uint32_t all;
    struct {
        uint8_t dc1           : 2; // bit0/1:(DC1)继电器开关
        uint8_t dc2           : 2; // bit2/3:(DC2)继电器开关
        uint8_t dc3           : 2; // bit4/5:(DC3)继电器开关
        uint8_t dc4           : 2; // bit6/7:(DC4)继电器开关
        uint8_t dc5           : 2; // bit8/9:(DC5)继电器开关
        uint8_t dc6           : 2; // bit10/11:(DC6)继电器开关
        uint8_t dc7           : 2; // bit12/13:(DC7)继电器开关
        uint8_t dc8           : 2; // bit14/15:(DC8)继电器开关
        uint8_t dc9           : 2; // bit16/17:(DC9)继电器开关
        uint8_t dc10          : 2; // bit18/19:(DC10)继电器开关
        uint8_t dc11          : 2; // bit20/21:(DC11)继电器开关
        uint8_t dc12          : 2; // bit22/23:(DC12)继电器开关
        uint8_t dc13          : 2; // bit24/25:(DC13)继电器开关
        uint8_t dc14          : 2; // bit26/27:(DC14)继电器开关
        uint8_t dc15          : 2; // bit28/29:(DC15)继电器开关
        uint8_t dc16          : 2; // bit30/31:(DC16)继电器开关
    }bit;
}dc_on_off_set;

typedef union {
    /*0-invalid;1-enable;2-disable）;
    (为了安全，只具备强制断开功能，不能强制闭合)
    （1-命令闭合，命令、保护、SOC与逻辑才是执行结果；2-命令断开，或逻辑）*/
    uint32_t all;
    struct {
        uint8_t ac1           : 2; // bit0/1:(AC1)继电器开关
        uint8_t ac2           : 2; // bit2/3:(AC2)继电器开关
        uint8_t ac3           : 2; // bit4/5:(AC3)继电器开关
        uint8_t ac4           : 2; // bit6/7:(AC4)继电器开关
        uint8_t ac5           : 2; // bit8/9:(AC5)继电器开关
        uint8_t ac6           : 2; // bit10/11:(AC6)继电器开关
        uint8_t ac7           : 2; // bit12/13:(AC7)继电器开关
        uint8_t ac8           : 2; // bit14/15:(AC8)继电器开关
        uint8_t ac9           : 2; // bit16/17:(AC9)继电器开关
        uint8_t ac10          : 2; // bit18/19:(AC10)继电器开关
        uint8_t ac11          : 2; // bit20/21:(AC11)继电器开关
        uint8_t ac12          : 2; // bit22/23:(AC12)继电器开关
        uint8_t ac13          : 2; // bit24/25:(AC13)继电器开关
        uint8_t ac14          : 2; // bit26/27:(AC14)继电器开关
        uint8_t ac15          : 2; // bit28/29:(AC15)继电器开关
        uint8_t ac16          : 2; // bit30/31:(AC16)继电器开关
    }bit;
}ac_on_off_set;


/**
 * @brief Type = 0x43
 *
 */
typedef struct {    
    ac_on_off_set AC_PowerRelay_force_set;  //AC开关设置
    dc_on_off_set DC_PowerRelay_force_set;  //DC开关设置

    /*保护设置暂未实现*/
    uint8_t revd[200];
    
    rw_cmd_struct rw_cmd;
}panel_set_struct;


typedef union {//当前开关数量
    uint16_t all;
    struct {
        uint8_t dc_num;    //低字节：DC开关数量
        uint8_t ac_num;    //高字节：AC开关数量
    }bit;
}dc_ac_num_struct;

typedef struct {
    uint16_t dc_voltage;    //DC输出电压(0.1V)
    uint16_t dc_current;    //DC输出电流(0.01A)
    uint16_t dc_power;      //DC输出功率(1W)
    uint32_t dc_energy;     //DC消耗有功电量(0.1kwh)
    uint16_t dc_alarm;      //DC告警

    uint16_t revd[2];
}dc_info_struct;

typedef struct {
    uint16_t acload_voltage;    //AC负载电压(0.1V)
    uint16_t acload_current;    //AC负载电流(0.01A)
    uint16_t acload_power;      //AC负载功率(1W)
    uint32_t acload_energy;     //AC消耗有功电量(0.1kwh)
    uint16_t acload_alarm;      //AC告警

    uint16_t revd[2];
}acload_info_struct;

/**
 * @brief Type = 0x44
 *
 */
typedef struct {
    char panel_type[12];            // panel型号
    uint64_t panel_sn;              // panel序列号
    uint32_t SoftwareVersion;       // panel软件版本
    dc_ac_num_struct dc_ac_num;     // 当前开关数量
    
    uint32_t DCLoadTotalPower;      // DC负载总功率(1W)
    uint32_t DCLoadTotalEnergy;     // DC负载总能量(0.1KWh)

    /*支持三种不同电压检测，tbd*/
    uint16_t V_dc1_RMS;             // 配电柜总接入直流1电压有效值，*10V,L1N
    uint16_t V_dc2_RMS;             // 配电柜总接入直流2电压有效值，*10V,L1N
    uint16_t V_dc3_RMS;             // 配电柜总接入直流3电压有效值，*10V,L1N

    dc_info_struct dc_info[6];     //DC详细信息[DC1~DC16]
    
    rw_cmd_struct rw_cmd;
}panel_dc_info_struct;

/**
 * @brief Type = 0x45
 *
 */
typedef struct {
    uint16_t ACfrequency;                       //交流频率，*100Hz
    uint16_t TEMP1;                             //腔体内温度,*10C
    uint16_t TEMP2;                             //腔体内温度,*10C
    uint16_t revd[10];
    
    uint32_t ACLoad1TotalPower;                 //AC负载总功率(1W)
    uint32_t ACLoadTotalEnergy;                 //AC负载总能量(0.1KWh)
    
    /*支持三种不同电压检测，tbd*/
    uint16_t V_ac1_RMS;             // 配电柜总接入交流1电压有效值，*10V,L1N
    uint16_t V_ac2_RMS;             // 配电柜总接入交流2电压有效值，*10V,L1N
    uint16_t V_ac3_RMS;             // 配电柜总接入交流3电压有效值，*10V,L1N

    acload_info_struct ac_load_info[4];     //AC详细信息[AC1~AC16]
    
    rw_cmd_struct rw_cmd;
}panel_ac_info_struct;

typedef union {//不同AC继电器相间联动设置,默认不联动(0x00)
    uint8_t all;
    struct {
        uint8_t phase_index             : 2;  //bit1/0:相线序号(通道次索引)，0-无效,1-L1，2-L2，3-L3;
        uint8_t chl_index               : 5;  //bit6~2:通道主索引，0~31(最多一个panel支持32个3相继电器配置);
        uint8_t linkage_enable          : 1;  //bit:7:1-使能联动；0-禁止联动；
    }bit;
}PowerRelay_Phase_Set_Struct;

typedef union {
    /*
    AC功率继电器通道时间联动设置使能
    0-invalid;
    1-动作延时使能（基于当前状态的反向动作的延时设置）;
    2-顺序定时使能（多段时间使能），3-禁止和两类时间动作关联)
    */
    uint16_t all;
    struct {
        uint8_t ac1           : 2; // bit0/1:(AC1)继电器时间联动使能
        uint8_t ac2           : 2; // bit2/3:(AC2)继电器时间联动使能
        uint8_t ac3           : 2; // bit4/5:(AC3)继电器时间联动使能
        uint8_t ac4           : 2; // bit6/7:(AC4)继电器时间联动使能
        uint8_t ac5           : 2; // bit8/9:(AC5)继电器时间联动使能
        uint8_t ac6           : 2; // bit10/11:(AC6)继电器时间联动使能
        uint8_t ac7           : 2; // bit12/13:(AC7)继电器时间联动使能
        uint8_t ac8           : 2; // bit14/15:(AC8)继电器时间联动使能
    }bit;
}ACPowerRelay_Time_En_Struct;

typedef union {
    /*
    DC功率继电器通道时间联动设置使能
    0-invalid;
    1-动作延时使能（基于当前状态的反向动作的延时设置）;
    2-顺序定时使能（多段时间使能），3-禁止和两类时间动作关联)
    */
    uint16_t all;
    struct {
        uint8_t dc1           : 2; // bit0/1:(DC1)继电器时间联动使能
        uint8_t dc2           : 2; // bit2/3:(DC2)继电器时间联动使能
        uint8_t dc3           : 2; // bit4/5:(DC3)继电器时间联动使能
        uint8_t dc4           : 2; // bit6/7:(DC4)继电器时间联动使能
        uint8_t dc5           : 2; // bit8/9:(DC5)继电器时间联动使能
        uint8_t dc6           : 2; // bit10/11:(DC6)继电器时间联动使能
        uint8_t dc7           : 2; // bit12/13:(DC7)继电器时间联动使能
        uint8_t dc8           : 2; // bit14/15:(DC8)继电器时间联动使能
    }bit;
}DCPowerRelay_Time_En_Struct;

// typedef union {
//     /*
//     功率继电器负载基于SOC的控制
//     优先级的本质是基于SOC，基于不同的SOC控制继电器
//     >100-invalid;0~100-有效,100表示SOC 100%,0x7F表示功能禁止
//     */
//     uint8_t all;
//     struct {
//         uint8_t soc_ctrl                : 7;  //bit6~0:SOC限值;
//         uint8_t revd                    : 1;  //bit7:预留；
//     }bit;
// }PowerRelay_Soc_Struct;

// typedef union {//动作时间设置值
//     uint16_t all;
//     struct {
//         uint8_t min;    //低字节：分钟（0~59）
//         uint8_t hour;   //高字节：小时（0~23）
//     }bit;
// }time_active_set;

// typedef union {
//     /*
//     功率继电器定时参数
//     1-enable;0-disable
//     */
//     uint16_t all;
//     struct {
//         uint8_t week                    : 7;    //bit6~0:周六~周日(六(bit6)，五，四，三，二，一，日(bit0))的自定义模式
//         uint8_t on_off                  : 1;    //bit~7:动作属性，0-OFF;1-ON
//         uint8_t index                   : 3;    //bit10~8:(0-本段功能禁用；1-AC1继电器;2-AC2继电器;3-AC3继电器;4-AC4继电器;5-AC5继电器;6-AC6继电器;)
//         uint8_t rev                     : 5;    //bit15~11:预留
//     }bit;
// }time_lable_set;

// typedef struct {
//     /*该结构体为一个时间点设置*/
//     time_lable_set time_label;
//     time_active_set time_active;
// }panel_time_set;

typedef union {
    /*功率继电器客户自定义名称*/
    uint16_t all[10];
    struct {
        char name[20];
    }bit;
}PowerRelayName_Struct;

/**
 * @brief Type = 0x4A
 *
 */
// typedef struct {    
//     uint16_t revd[10];

//     PowerRelay_Phase_Set_Struct PowerRelay_Phase_Set_AC[6];     //不同AC继电器相间联动设置
//     ACPowerRelay_Time_En_Struct ACPowerRelay_Time_En;           //AC功率继电器通道时间联动设置使能
//     DCPowerRelay_Time_En_Struct DCPowerRelay_Time_En;           //DC功率继电器通道时间联动设置使能
//     PowerRelay_Soc_Struct AC_PowerRelayOffSoc[6];               //AC功率继电器负载断开SOC
//     PowerRelay_Soc_Struct AC_PowerRelayOnSoc[6];                //AC功率继电器负载闭合SOC
//     PowerRelay_Soc_Struct DC_PowerRelayOffSoc[6];               //DC功率继电器负载断开SOC
//     PowerRelay_Soc_Struct DC_PowerRelayOnSoc[6];                //DC功率继电器负载闭合SOC
//     time_active_set PowerRelayDelay_Set_AC[6];                  //AC功率继电器L1动作延时设置目标时间(倒计时)
//     time_active_set PowerRelayDelay_Set_DC[6];                  //DC功率继电器L1动作延时设置目标时间(倒计时)

//     /*共计2*6*10=120个点位时间设置，一个继电器最多支持10个时间点设置*/
//     panel_time_set PowerRelayTime_Set_AC[6][10];                //AC功率继电器时间段
//     panel_time_set PowerRelayTime_Set_DC[6][10];                //DC功率继电器时间段

//     PowerRelayName_Struct AC_PowerRelayName[6];                 //AC功率继电器客户自定义名称
//     PowerRelayName_Struct DC_PowerRelayName[6];                 //DC功率继电器客户自定义名称

//     rw_cmd_struct rw_cmd;
// }panel_set_ctrl_iot_struct;

// typedef struct {
//     uint8_t             devId;         // 当前数据属于哪个节点
//     uint8_t             online;//1-设备在线；0-超时清零
//     uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
// /*

// 并机排序之后才set，排地址时候清零
// 	*/    
//     uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

//     panel_set_struct        panel_setting;
//     panel_dc_info_struct    panel_dc_info;
//     panel_ac_info_struct    panel_ac_info;
//     panel_set_ctrl_iot_struct panel_set_ctrl_iot;
//     iot_11000_can_struct mod_reg11000_IOT_info;  // 
// }panel_can_node_struct;


// typedef union {
//     uint16_t all;
//     struct {
//         uint8_t soc_ctrl_init       : 1;    //soc控制初始化
//         uint8_t time_ctrl_init      : 1;    //时间控制初始化
//         uint8_t delay_ctrl_init     : 1;    //延时配置初始化
//         uint8_t revd2               : 1;
        
//         uint8_t delay_ctrl_set      : 1;    //新延时设置
//         uint8_t soc_ctrl_ctrl       : 1;    //新soc设置
//         uint8_t time_ctrl_set       : 1;    //新时间设置
//         uint8_t revd5               : 1;

//         uint8_t time_ctrl_target    : 1;    //时间设置
//         uint8_t force_ctrl_target   : 1;    //强制设置
//         uint8_t delay_ctrl_target   : 1;    //延时设置
//         uint8_t revd4               : 1;
//     }bit;
// }panel_ctrl_status_struct;

typedef struct {
    uint8_t status;                 // 延时设置状态 0：无效，1：正在计时，2：计时结束，等待下发设置
    uint64_t timecnt_1s;            // 延时时间计算，最大24小时（86400s）
    uint8_t set;                    // 设置，0：无效，1：打开，2：关闭
}relay_node_delay_struct;

typedef union
{
    uint16_t all;

    struct{
        uint16_t line_car_flow_to_dc1:1;       // bit0: 车流向CHARGER 1(DC1),仅原CHARGER 1使用 0-无；1-进入DCDC模块；2-从DCDC模块输出
        uint16_t line_dc1_flow_to_dc3:1;        // bit1: CHARGER 1流向储能产品(DC3),仅原CHARGER 1使用 0-无；1-进入DCDC模块；2-从DCDC模块输出
        uint16_t line_dc1:2;        // bit2/3:DC1
        uint16_t line_dc2:2;        // bit4/5:DC2
        uint16_t line_dc3:2;        // bit6/7:DC3
        uint16_t line_dc4:2;        // bit8/9:DC4
        uint16_t line_dc5:2;        // bit10/11:DC5
        uint16_t line_dc6:2;        // bit12/13:DC6
        uint16_t revd:2;        // bit14~15:预留
    } bit;
}energy_line_t;

typedef union
{
    uint16_t all;

    struct{
        uint16_t dc_out_onoff:2;            // bit1/0: DC输出开关 1：ON; 2：OFF;
        uint16_t silent_mode:2;            //  bit3/2: 静音模式 1：ON; 2：OFF;
        uint16_t restore_factory:2;            // bit5/4: 恢复出厂设置：1-enable;2-disable;
        uint16_t adaptive_mode:2;            // bit7/6: 自适应模式（2009协议更新），1-disable;2-enable;
        uint16_t dc1_onoff:2;          //bit9~8：DC1开关
        uint16_t dc2_onoff:2;          //bit11~10：DC2开关
        uint16_t dc4_onoff:2;          //bit13~12：DC4开关
        uint16_t dc5_onoff:2;          //bit15~14：DC5开关
    } bit;
}d400s_charger_set_t;

typedef union {
    uint16_t all;
    struct {
        uint16_t dc6_output_set : 2;	// bit1/0: DC6输出开关 1：ON; 2：OFF;
        uint16_t dc1_memory_set : 2;	//bit3/2：DC1记忆开关1
        uint16_t dc2_memory_set : 2;	//bit5/4：DC2记忆开关1
        uint16_t dc3_memory_set : 2;	//bit7/6：DC3记忆开关1
        uint16_t dc4_memory_set : 2;	//bit9/8：DC4记忆开关1
        uint16_t dc5_memory_set : 2;	//bit11/10：DC5记忆开关1
        uint16_t dc6_memory_set : 2;    //bit13/12：DC6记忆开关1
        uint16_t revd:2; 	            //bit15/14：预留                                
    }bit;
}d400s_memory_set;

typedef union {
    uint16_t all;
    struct {
        uint16_t dc1_charge_mode : 4;	// bit3/0: 1-电池充电模式；2-PV充电模式；3-铅酸电池充电
        uint16_t dc2_charge_mode : 4;	//bit7/4：1-电池充电模式；2-PV充电模式；3-铅酸电池充电
        uint16_t dc3_charge_mode : 4;	//bit11/8：1-电池充电模式；2-PV充电模式；3-铅酸电池充电
        uint16_t dc4_charge_mode : 4;	//bit15/12：1-电池充电模式；2-PV充电模式；3-铅酸电池充电                     
    }bit;
}d400s_charge_mode_2_set;

typedef union {
    uint16_t all;
    struct {
        uint16_t dc5_charge_mode : 4;	// bit3/0: 1-电池充电模式；2-PV充电模式；3-铅酸电池充电
        uint16_t dc6_charge_mode : 4;	//bit7/4：1-电池充电模式；2-PV充电模式；3-铅酸电池充电
        uint16_t revd:2; 	            //bit15/8：预留                                 
    }bit;
}d400s_charge_mode_3_set;

typedef union {     //
    uint16_t all;
    struct {
        uint16_t generator_Detection : 2;	//bit1/0: 发电机检测  0-invalid;1-enable;2-disable;
        uint16_t generator_Type : 2;	    //bit3/2：发电机类型（1智能，2普通）
        uint16_t charging_Priority:2; 	    //bit5/4：充电优先：1-驻车电瓶优先，2-储能设备优先 
        uint16_t rechargeMode:2;            //bit7/6 补电模式
        uint16_t pulse_Freq:2;              //bit9/8  脉冲频率
        uint16_t mode_Onoff:2;              //bit11/10 模式开关
        uint16_t maintenance_Mode:2;        //bit13/12 保养模式  1-保养模式-涓流；2-保养模式-脉冲
        uint16_t long_short_line:2;         //bit15/14 1-小电流（对应长线）；2-大电流（对应短线）；默认小电流                           
    }bit;
}d400s_charge_mode_4_set;

//面向双向DC口定义，进入DCDC模块为正，从DCDC模块输出为负
typedef struct {        
    int16_t dc_voltage;     //0.1V
    int32_t dc_current;     //0.1A
    int16_t dc_power;       //1W
}d400s_dc_info;

//面向双向DC口定义，进入DCDC模块为正，从DCDC模块输出为负
typedef struct {        
    uint32_t dc_energy_input;     //0.1kwh
    uint32_t dc_energy_output;     //0.1kwh
}d400s_dc_energy_info;


typedef struct {        
    uint16_t dc_voltage_set;              // DC输出电压设置值 单位0.1V
    uint16_t dc_current_set;                //dc输出电流限制设置 单位0.1A
}d400s_dc_volt_current_set;

typedef union {
	uint16_t all;

	struct {
		struct {
            uint8_t work_on : 1;   //Bit0:  工作状态 0:停机 1:工作
            uint8_t fault_exist : 1;      // Bit1:  故障 0:有故障 1:无故障
            uint8_t pv_online : 1;      // Bit2:  PV在线 0:不在线 1:在线
            uint8_t work_mode : 4;     /* Bit3~6: 1-自定义模式：客户APP设置，参数掉电不丢失，Vset,Iset,on off;
                                        2-无效，自适应-未知
                                        3-自适应-主动模式：无逆变器模式
                                        4-自适应-补充模式：有旧型号逆变器模式(AC200MAX,AC200L,AC300,AC500)
                                        5-自适应-受控最优模式：有新型号逆变器模式(匹配D400S的新增INV协议)，D400S优先放电*/
            uint8_t pv_enable : 1;    // Bit7:  光伏使能 0:不使能 1:使能
            uint8_t revd :8;
		} bit;						
	};
} d400s_work_ctrl;//0x08FAFFXX 逆变器CAN 单帧控制， data[8]

/**
 * @brief Type = 0x48 DCDC模块通用 信息区
 *
 */
typedef struct {
    char dev_type[12];             // CHARGER 1 型号
    uint64_t dev_sn;              // CHARGER 1 SN  低字节在前
    uint16_t battery_type;              //输入电池类型 1:12V铅酸电池 2:24V铅酸电池
    energy_line_t energy_line;                //能量流动条
    uint16_t fault_dc1;    // DC1故障（原CHARGER 1故障）
    uint16_t fault_dcdc;    // dcdc故障
    uint16_t proctect_dcdc; // dc保护
    uint16_t fault_dc2; // DC2故障
    uint16_t fault_dc3; // DC3故障
    uint16_t fault_dc4; // DC4故障
    uint16_t fault_dc5; // DC5故障
    uint16_t fault_dc6; // DC6故障
    uint16_t fault_charger2; // CHAEGE2汇总故障
    uint16_t alarm_dcdc; // DCDC告警
    d400s_work_ctrl ctrl_mode;                      //工作模式
    int32_t total_input_power;          // 所有DC通道总的进入功率  
    int32_t total_output_power;         // 所有DC通道总的输出功率 
    d400s_dc_info dc_info[6];           //dc 电压 、电流、功率     
    int32_t TotalInputEnergy;           //电量 0.1kwh
    d400s_dc_energy_info energy_info[6];   // 面向双向DC口定义，进出DCDC模块的能量信息
    uint16_t dcdc_SoftwareType;             //0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块 ；11-内置HMI模块；12-内置HMI模块2；13--RF穿墙模块；14-DC-HUB；15-AC-HUB；16-DC-DC；
    uint32_t dcdc_SoftwareVersion;          //软件版本号 例：1001.11； 填充值：100111 15582~15583
    uint16_t revd2[10];
    rw_cmd_struct rw_cmd;
}dcdc_common_info_struct;

/**
 * @brief Type = 0x49 DCDC模块通用/CHARGER 1 设置区
 *
 */
typedef struct {
    d400s_charger_set_t charger_set;            // 设置
    d400s_dc_volt_current_set dc_val_set[6];              //电流电压设置
    d400s_memory_set memory_val_set;                //15613 dc记忆开关模式设置
    d400s_charge_mode_2_set mode2_set;                //15614 dc充电模式设置1
    d400s_charge_mode_3_set mode3_set;                //15615 dc充电模式设置2
    uint16_t batteryCapacity_L;                 //15616 铅酸电池容量 0.1AH
    uint16_t batteryCapacity_H;                 //15617 铅酸电池容量 0.1AH
    uint16_t battery_Type;                     //15618 电池类型
    int16_t dc_Power_Set[5];                    //15619~15623 流入DC为正，流出为负
    int16_t dc_Total_Power_Set;                //15624
    d400s_charge_mode_4_set mode4_set;           //15625
    uint16_t mode5_set;                          //15626
    uint16_t revd[10];
    rw_cmd_struct rw_cmd;
}dcdc_common_set_struct;

typedef struct{
    char dev_type[12];            // CHARGER 1 型号 15500~15505
    uint64_t dev_sn;              // CHARGER 1 SN  低字节在前 15506~15509
    uint16_t dc1_input_voltage;         //dc1 输入电压 15510 
    uint16_t dc3_output_voltage;         //dc3 输出电压 15511 
    uint16_t dc3_output_current;         //dc3 输出电流 15512
    uint16_t dc3_output_power;           //dc3 输出功率 15513    
    energy_line_t energy_line;           //能量流动条   15514
    uint16_t battery_type;              //输入电池类型 1:12V铅酸电池 2:24V铅酸电池 15515
    uint16_t fault_dc1;                 // charger故障      15516
    uint16_t fault_dcdc;                 // dcdc故障      15517
    uint16_t proctect_dcdc;                 // dcdc保护      15518
    uint16_t fault_dc2;                 // DC2故障      15519
    uint16_t fault_dc3;                 // DC3故障      15520
    uint16_t fault_dc4;                 // DC4故障      15521
    uint16_t fault_dc5;                 // DC5故障      15522
    uint16_t fault_dc6;                 // DC6故障      15523
    uint16_t fault_charge2;                 // CHAEGE2汇总故障      15524
    uint16_t alarm_dcdc;                 // DCDC告警      15525
    d400s_work_ctrl ctrl_mode;                      //工作模式15526 
    int32_t total_input_power;          // 所有DC通道总的进入功率 15527~15528 
    int32_t total_output_power;         // 所有DC通道总的输出功率 15529~15530
    d400s_dc_info dc_info[6];           //dc 电压 、电流、功率     15531~15554
    int32_t TotalInputEnergy;           //电量 0.1kwh 15555~15556
    d400s_dc_energy_info energy_info[6];   // 面向双向DC口定义，进出DCDC模块的能量信息 15557~15580
    uint16_t dcdc_SoftwareType;             //0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块 ；11-内置HMI模块；12-内置HMI模块2；13--RF穿墙模块；14-DC-HUB；15-AC-HUB；16-DC-DC；
    uint32_t dcdc_SoftwareVersion;          //软件版本号 例：1001.11； 填充值：100111 15582~15583
    uint16_t PACK_SOC;                  // PACK总SOC 15584
    uint16_t PackVoltValue;             // 汽车电瓶电压平台 15585
    uint16_t revd2[14];                 //预留 15586~15599
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
    uint16_t mode5_set;                          //15626
    uint16_t revd[73];                           //15627~15699
}MOD_STRUCT_reg15600;

#pragma pack()
#endif
