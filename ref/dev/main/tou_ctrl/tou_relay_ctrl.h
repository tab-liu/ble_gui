#ifndef TOU_RELAY_CTRL_H
#define TOU_RELAY_CTRL_H

#include <stdint.h>
#include <string.h>

#define CONFIG_ENABLE_PLAN_BACKUP  0//预约备电功能使能


/*u64变量中DC、AC的开始序号 AP500暂定，协议需与APP协商，当前仅支持DC总开关和AC总开关*/
enum {
    RELAY_INDEX_AC1 = 1,
    RELAY_INDEX_AC2,
    RELAY_INDEX_AC3,
    RELAY_INDEX_AC4,
    RELAY_INDEX_MAX,
};

enum {
    POWERRELAY_SMART_SET01,
    POWERRELAY_SMART_SET02,
    POWERRELAY_SMART_SET03,
    POWERRELAY_SMART_SET04,
};

#define RELAY_CTRL_MAX_NUM          32                              // 支持智能控制的最大对象数量
#define RELAY_CTRL_REAL_NUM         (RELAY_INDEX_MAX - 1)           // 当前实际支持智能控制的对象个数
#define RELAY_TIME_CTRL_NODE_NUM    10                              // 定时控制的时间节点最大数量

#define	RELAY_SETDATA_TYPE_LEN	((uint16_t)2048)


#pragma pack(1)

typedef union {
    uint32_t all;
    struct {
        uint32_t relay1  : 1;
        uint32_t relay2  : 1;
        uint32_t relay3  : 1;
        uint32_t relay4  : 1;
        uint32_t relay5  : 1;
        uint32_t relay6  : 1;
        uint32_t relay7  : 1;
        uint32_t relay8  : 1;
        uint32_t relay9  : 1;
        uint32_t relay10 : 1;
        uint32_t relay11 : 1;
        uint32_t relay12 : 1;
        uint32_t relay13 : 1;
        uint32_t relay14 : 1;
        uint32_t relay15 : 1;
        uint32_t relay16 : 1;
        uint32_t relay17 : 1;
        uint32_t relay18 : 1;
        uint32_t relay19 : 1;
        uint32_t relay20 : 1;
        uint32_t relay21 : 1;
        uint32_t relay22 : 1;
        uint32_t relay23 : 1;
        uint32_t relay24 : 1;
        uint32_t relay25 : 1;
        uint32_t relay26 : 1;
        uint32_t relay27 : 1;
        uint32_t relay28 : 1;
        uint32_t relay29 : 1;
        uint32_t relay30 : 1;
        uint32_t relay31 : 1;
        uint32_t relay32 : 1;
    }bit;
}iot_smart_ctrl_struct;

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

typedef union {//动作时间设置值
    uint16_t all;
    struct {
        uint8_t min;    //低字节：分钟（0~59）
        uint8_t hour;   //高字节：小时（0~23）
    }bit;
}time_active_set;

typedef union {
    //0-invalid;1-ON;2-off
    uint16_t all;
    struct {
        uint8_t PowerRelay1           : 2; // bit0/1:(PowerRelay1)
        uint8_t PowerRelay2           : 2; // bit2/3:(PowerRelay2)
        uint8_t PowerRelay3           : 2; // bit4/5:(PowerRelay3)
        uint8_t PowerRelay4           : 2; // bit6/7:(PowerRelay4)
        uint8_t PowerRelay5           : 2; // bit8/9:(PowerRelay5)
        uint8_t PowerRelay6           : 2; // bit10/11:(PowerRelay6)
        uint8_t PowerRelay7           : 2; // bit12/13:(PowerRelay7)
        uint8_t PowerRelay8           : 2; // bit14/15:(PowerRelay8)
    }bit;
}PowerRelayOnOff_U16_Struct;

typedef union {
    uint32_t all;
    struct {
        uint8_t Power_Relay1           : 2; // bit0/1:(Power_Relay1)继电器开关
        uint8_t Power_Relay2           : 2; // bit2/3:(Power_Relay2)继电器开关
        uint8_t Power_Relay3           : 2; // bit4/5:(Power_Relay3)继电器开关
        uint8_t Power_Relay4           : 2; // bit6/7:(Power_Relay4)继电器开关
        uint8_t Power_Relay5           : 2; // bit8/9:(Power_Relay5)继电器开关
        uint8_t Power_Relay6           : 2; // bit10/11:(Power_Relay6)继电器开关
        uint8_t Power_Relay7           : 2; // bit12/13:(Power_Relay7)继电器开关
        uint8_t Power_Relay8           : 2; // bit14/15:(Power_Relay8)继电器开关
        uint8_t Power_Relay9           : 2; // bit16/17:(Power_Relay9)继电器开关
        uint8_t Power_Relay10          : 2; // bit18/19:(Power_Relay10)继电器开关
        uint8_t Power_Relay11          : 2; // bit20/21:(Power_Relay11)继电器开关
        uint8_t Power_Relay12          : 2; // bit22/23:(Power_Relay12)继电器开关
        uint8_t Power_Relay13          : 2; // bit24/25:(Power_Relay13)继电器开关
        uint8_t Power_Relay14          : 2; // bit26/27:(Power_Relay14)继电器开关
        uint8_t Power_Relay15          : 2; // bit28/29:(Power_Relay15)继电器开关
        uint8_t Power_Relay16          : 2; // bit30/31:(Power_Relay16)继电器开关
    }bit;
}PowerRelayOnOff_U32_Struct;

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

typedef struct {
    /*该结构体为一个时间点设置*/
    time_lable_set time_label;
    time_active_set time_active;
}relay_time_set;

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

typedef union {
    uint16_t all;
    struct {
        uint16_t status1 : 4; // 0-无效 1-充 2-放 3-关闭
        uint16_t status2 : 4; // 0-无效 1-充 2-放 3-关闭
        uint16_t status3 : 4; // 0-无效 1-充 2-放 3-关闭
        uint16_t status4 : 4; // 0-无效 1-充 2-放 3-关闭

    }bit;
} chg_plan_enable_t;

typedef struct {
    uint32_t start_time; // 开始时间戳
    uint32_t end_time;   // 结束时间戳
}time_stamp_set_t;

/*预约备电区*/
typedef struct {
    /*19200*/
    uint16_t enable; // bit0-1: 0-无效 1-使能 2-关闭
    chg_plan_enable_t Chg_Plan_Time_En1; // 预约充电使能1 (1-使能预约功能 2-关闭) 对应预约时间间隔timestamp1 0~3
    chg_plan_enable_t Chg_Plan_Time_En2; // 预约充电使能2 (1-使能预约功能 2-关闭) 对应预约时间间隔timestamp2 4~7
    /*19201-19217*/
    time_stamp_set_t timestamp1[8]; // 预约时间间隔1 (起始) 0~7
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
	} ArrayData;			/*设置数据结构*/

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
            uint16_t Set2[100]; // 预留
            MOD_STRUCT_reg19100 PowerRelay_SmartSet02;  //继电器智能控制区2
        };  

        union 
        {
            uint16_t Set3[100]; // 预留
            MOD_STRUCT_reg19200 Backup_power_set;  //预约备电区 19200-19219
        }; 

        union 
        {
            uint16_t Set4[700];
            MOD_STRUCT_reg19300 PowerRelay_SmartSet03;  //继电器智能控制区3
        };         
        
		uint8_t u8temp; //  
	}Relay_info_t;	            //设置数据结构
}Relay_SetData_TypeDef;	 

extern Relay_SetData_TypeDef RelaySetData;

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


extern BitData_RELAY_FILE_SET Relay_File_W_Flag;
extern Relay_ctrl_struct Relay_ctrl;

#pragma pack()

void Relay_Ctrl_Factory_Parameter01_Update(void);
void Relay_Ctrl_Factory_Parameter02_Update(void);
void Relay_Ctrl_Factory_Parameter03_Update(void);
void Relay_Ctrl_Factory_Parameter04_Update(void);

void Relay_Ctrl_Setdata_Get_From_Modbus(uint32_t address_value, uint8_t type, uint16_t write_offset, uint16_t data_len);

void Relay_Ctrl_Parameter_update(uint8_t time_init_flag, uint8_t soc);
void Relay_Ctrl_Main_Task(void);

uint8_t Relay_Ctrl_Get_System_Soc(void);
uint8_t Relay_Ctrl_Soc_Allows_Timed_Task(uint8_t relay_index);
uint8_t Relay_Ctrl_Soc_Allows_Tou_Reg(uint16_t reg_addr, uint16_t reg_value);

uint8_t Get_Plan_Backup_Power_Status(void);

#endif
