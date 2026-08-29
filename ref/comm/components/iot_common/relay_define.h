#pragma once

#include "stdint.h"

#define RELAY_CTRL_MAX_NUM          32                              // 支持智能控制的最大对象数量
#define RELAY_CTRL_REAL_NUM         (RELAY_INDEX_MAX - 1)           // 当前实际支持智能控制的对象个数
#define RELAY_TIME_CTRL_NODE_NUM    10                              // 定时控制的时间节点最大数量
#define RELAY_SETDATA_TYPE_LEN ((uint16_t)1024 * 2)

/*设置对应继电器位置状态*/
#define SET_POWER_RELAY_STATE(relay, index, state) \
    do { \
        relay &= ~(0x03 << (index * 2)); /* 清除对应位 */ \
        relay |= ((state & 0x03) << (index * 2)); /* 设置新值 */ \
    } while (0)

/*u64变量中DC、AC的开始序号*/
enum {
    RELAY_INDEX_DC_LOAD1 = 1,
    RELAY_INDEX_AC_LOAD1,
    RELAY_INDEX_DC_LOAD2,
    RELAY_INDEX_AC_LOAD2,
    RELAY_INDEX_DC_LOAD3,
    RELAY_INDEX_AC_LOAD3,
    RELAY_INDEX_MAX,
};

enum {
    POWERRELAY_SMART_SET01,
    POWERRELAY_SMART_SET02,
    POWERRELAY_SMART_SET03,
    POWERRELAY_SMART_SET04,
};

typedef union {
    /*
    功率继电器定时参数
    1-enable;0-disable
    */
    uint16_t all;
    struct {
        uint8_t week                    : 7;    //bit6~0:周六~周日(六(bit6)，五，四，三，二，一，日(bit0))的自定义模式
        uint8_t on_off                  : 4;    //bit10~7:动作属性，0-OFF;1-ON
        uint8_t index                   : 5;    //bit15~11:只读，功能对象枚举(设备不接受APP写来的,只可由设备自己来变化，APP依据对象将相应的时间点依次写入相应地址
    }bit;
}time_lable_set;

typedef union {//动作时间设置值
    uint16_t all;
    struct {
        uint8_t min;    //低字节：分钟（0~59）
        uint8_t hour;   //高字节：小时（0~23）
    }bit;
}time_active_set;

typedef struct {
    /*该结构体为一个时间点设置*/
    time_lable_set time_label;
    time_active_set time_active;
}relay_time_set;

typedef union {
    /*
    优先级的本质是基于SOC，基于不同的SOC断开继电器
    >100-invalid;0~100-有效,100表示SOC 100%,0xFF表示功能禁止
    */
    uint16_t all;
    struct {
        uint8_t CmdOffSoc                : 7;  //bit6~0:断开SOC;
        uint8_t CmdOffEnable             : 1;  //bit7:1:写指令有效，0无效
        uint8_t CmdOnSoc                 : 7;  //bit14~8:闭合SOC;
        uint8_t CmdOnEnable              : 1;  //bit15:1:写指令有效，0无效
    }bit;
}PowerRelay_Soc_Struct;

typedef union {
    // 0-invalid; 1-ON; 2-off
    uint64_t all;
    struct {
        uint8_t PowerRelay1           : 2; // bit0/1:(PowerRelay1)
        uint8_t PowerRelay2           : 2; // bit2/3:(PowerRelay2)
        uint8_t PowerRelay3           : 2; // bit4/5:(PowerRelay3)
        uint8_t PowerRelay4           : 2; // bit6/7:(PowerRelay4)
        uint8_t PowerRelay5           : 2; // bit8/9:(PowerRelay5)
        uint8_t PowerRelay6           : 2; // bit10/11:(PowerRelay6)
        uint8_t PowerRelay7           : 2; // bit12/13:(PowerRelay7)
        uint8_t PowerRelay8           : 2; // bit14/15:(PowerRelay8)
        uint8_t PowerRelay9           : 2; // bit16/17:(PowerRelay9)
        uint8_t PowerRelay10          : 2; // bit18/19:(PowerRelay10)
        uint8_t PowerRelay11          : 2; // bit20/21:(PowerRelay11)
        uint8_t PowerRelay12          : 2; // bit22/23:(PowerRelay12)
        uint8_t PowerRelay13          : 2; // bit24/25:(PowerRelay13)
        uint8_t PowerRelay14          : 2; // bit26/27:(PowerRelay14)
        uint8_t PowerRelay15          : 2; // bit28/29:(PowerRelay15)
        uint8_t PowerRelay16          : 2; // bit30/31:(PowerRelay16)
        uint8_t PowerRelay17          : 2; // bit32/33:(PowerRelay17)
        uint8_t PowerRelay18          : 2; // bit34/35:(PowerRelay18)
        uint8_t PowerRelay19          : 2; // bit36/37:(PowerRelay19)
        uint8_t PowerRelay20          : 2; // bit38/39:(PowerRelay20)
        uint8_t PowerRelay21          : 2; // bit40/41:(PowerRelay21)
        uint8_t PowerRelay22          : 2; // bit42/43:(PowerRelay22)
        uint8_t PowerRelay23          : 2; // bit44/45:(PowerRelay23)
        uint8_t PowerRelay24          : 2; // bit46/47:(PowerRelay24)
        uint8_t PowerRelay25          : 2; // bit48/49:(PowerRelay25)
        uint8_t PowerRelay26          : 2; // bit50/51:(PowerRelay26)
        uint8_t PowerRelay27          : 2; // bit52/53:(PowerRelay27)
        uint8_t PowerRelay28          : 2; // bit54/55:(PowerRelay28)
        uint8_t PowerRelay29          : 2; // bit56/57:(PowerRelay29)
        uint8_t PowerRelay30          : 2; // bit58/59:(PowerRelay30)
        uint8_t PowerRelay31          : 2; // bit60/61:(PowerRelay31)
        uint8_t PowerRelay32          : 2; // bit62/63:(PowerRelay32)
    } bit;
} PowerRelayOnOff_U64_Struct;

typedef union {
    uint16_t all;
    struct {
        // 0-无效 1-充 2-放 3-关闭
        uint16_t status1 : 2;
        uint16_t status2 : 2;
        uint16_t status3 : 2;
        uint16_t status4 : 2;
        uint16_t status5 : 2;
        uint16_t status6 : 2;
        uint16_t status7 : 2;
        uint16_t status8 : 2;
    }bit;
} chg_plan_enable_t;

typedef struct {
    uint32_t start_time; // 开始时间戳
    uint32_t end_time;   // 结束时间戳
}time_stamp_set_t;

/*继电器智能控制区1*/
typedef struct {
    /*19000-19031*/
    PowerRelay_Soc_Struct GeneralPowerRelaySoc[RELAY_CTRL_MAX_NUM];     // 继电器负载SOC
}MOD_STRUCT_reg19000;

/*继电器智能控制区2*/
typedef struct {

    /*19100-19103*/
    PowerRelayOnOff_U64_Struct GeneralPowerRelay_Delay_Time_En;         //继电器通道时间联动设置使能（延时）

    /*19104-19107*/
    PowerRelayOnOff_U64_Struct GeneralPowerRelayDelay_Action;           //延时动作属性

    /*19108-19139*/
    time_active_set PowerRelayDelay_Set_General[RELAY_CTRL_MAX_NUM];                    //动作延时设置目标时间(倒计时)

}MOD_STRUCT_reg19100;

/*预约备电区*/
typedef struct {
    /*19200*/
    uint16_t enable; // bit0-1: 0-无效 1-使能 2-关闭
    chg_plan_enable_t Chg_Plan_Time_En1; // 预约充电使能1 (1-使能预约功能 2-关闭)

    /*19201-19217*/
    time_stamp_set_t timestamp1[8]; // 预约时间间隔1 (起始)
}MOD_STRUCT_reg19200;

/*继电器智能控制区3*/
typedef struct {

    /*19300-19303*/
    PowerRelayOnOff_U64_Struct GeneralPowerRelay_Set_Time_En;               //继电器通道时间联动设置使能（定时）

    /*19304*/
    uint16_t PowerRelay_Time_Set_Cnt;                                       //时间点总数

    /*19305~*/
    relay_time_set PowerRelayTime_Set_General[(RELAY_CTRL_REAL_NUM*RELAY_TIME_CTRL_NODE_NUM)];                         // 继电器时间段

}MOD_STRUCT_reg19300;

typedef union tagSetRelayData
{
	struct
	{ 
		uint8_t	CfgCharData[RELAY_SETDATA_TYPE_LEN];		
	}ArrayData;			/*设置数据结构*/

	struct sNameRelayData// 
	{
		uint16_t valid_Relay;//参数有效标志
        uint16_t revd;
        
        union 
        {
            uint16_t Set1[100];
            MOD_STRUCT_reg19000 PowerRelay_SmartSet01;  //继电器智能控制区1
        };  

        union 
        {
            uint16_t Set2[200];
            MOD_STRUCT_reg19100 PowerRelay_SmartSet02;  //继电器智能控制区2
        };  

        union 
        {
            uint16_t Set3[20];
            MOD_STRUCT_reg19200 Backup_power_set;  //预约备电区
        }; 
        
        union 
        {
            uint16_t Set4[700];
            MOD_STRUCT_reg19300 PowerRelay_SmartSet03;  //继电器智能控制区3
        };         
        
		uint8_t u8temp; //  
	}Relay_info_t;	            //设置数据结构
}Relay_SetData_TypeDef;	 
