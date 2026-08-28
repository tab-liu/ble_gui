#ifndef PARTS_TYPE_H
#define PARTS_TYPE_H

#include <stdint.h>
#include "can_type.h"

#define DC_HUB_CAN_ADDR        0xe0 //dchub地址
#define AC_HUB_CAN_ADDR        0xef //achub地址
#define ATS_CAN_ADDR           0xf0 //ATS地址
#define D400S_CAN_ADDR         0x25 //D400S地址
#define DC_HUB_MAX_NUM            1
#define AC_HUB_MAX_NUM            1
#define ATS_MAX_NUM               1
#define D400S_MAX_NUM             1
#define DC_HUB_GROUP_MAX_NUM   DC_HUB_MAX_NUM	//一个逆变器系统对应的dchub数量
#define AC_HUB_GROUP_MAX_NUM   AC_HUB_MAX_NUM	//
#define ATS_GROUP_MAX_NUM      ATS_MAX_NUM	    //
#define D400S_GROUP_MAX_NUM   D400S_MAX_NUM	//

#define AC_HUB_HIGHVOLT_VERSION      9087
#define AC_HUB_LOWVOLT_VERSION       9081


enum {
    MODULE_TYPE_DC_AC_HUB_SET_40H = 0x40,
    MODULE_TYPE_DC_HUB_INFO_41H = 0x41,
    MODULE_TYPE_AC_HUB_INFO_42H = 0x42,
    MODULE_TYPE_D400S_INFO_48H = 0x48,
    MODULE_TYPE_D400S_SET_49H = 0x49,
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
}ac_hub_alarmstate;

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
    uint16_t dc_hub_type[6];            // DC_HUB型号
    uint16_t dc_hub_sn[4];              // DC_HUB序列号
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
    char ac_hub_type[12];            // AC_HUB型号
    uint64_t ac_hub_sn;              // AC_HUB序列号
    uint8_t SoftwareType;               // AC_HUB软件类型
    uint32_t SoftwareVersion;           // AC_HUB软件版本
    uint64_t ACHUB_safe_code;           // AC_HUB安全码
    uint8_t revd[4];                   // 预留
    uint32_t GridEnergy;                //并机电网能量
    uint32_t AcLoadEnergy;              //并机ac负载能量
    // /* 以下数据由achub主动上报，Temperature暂时没有 */
    uint16_t InvVoltageL1;              // L1相逆变电压 0.01V
    uint16_t InvVoltageL2;              // L2相逆变电压 0.01V
    uint16_t InvVoltageL3;              // L3相逆变电压 0.01V
    uint16_t Volt_grid_L1;              // L1相电网电压 0.01V
    uint16_t Volt_grid_L2;              // L2相电网电压 0.01V
    uint16_t Volt_grid_L3;              // L3相电网电压 0.01V
    /* Power_load、alarm_State由achub主动上报 */
    uint16_t Power_load[3];             // L1~L3相负载功率 1w
    uint16_t Power_grid[3];             // L1~L3相电网功率 1w
    uint16_t GridFrequency;             // 电网频率
    uint16_t Temperature;               // 温度
    ac_hub_alarmstate alarm_State;               // 告警状态
    rw_cmd_struct rw_cmd;
}ac_hub_info_struct;

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
        uint16_t communication_mode:2;          //bit9~8：通讯模式（1：不恒压模式；2：恒压模式）
        uint16_t revd:6;            // bit15~8：预留
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


typedef struct {
    d400s_charger_set_t charger_set;            // 设置 15600
    d400s_dc_volt_current_set dc_val_set[6];              //电流电压设置 15601~15612
    d400s_memory_set memory_val_set;                //15613 dc记忆开关模式设置
    d400s_charge_mode_2_set mode2_set;                //15614 dc充电模式设置1
    d400s_charge_mode_3_set mode3_set;                //15615 dc充电模式设置2
    uint16_t batteryCapacity_L;                 //15616 铅酸电池容量 0.1AH
    uint16_t batteryCapacity_H;                 //15617 铅酸电池容量 0.1AH
    uint16_t batteryType;                       //15618 电池类型
    int16_t dc_Power_Set[5];                    //15619~15623 流入DC为正，流出为负
    int16_t dc_Total_Power_Set;                //15624
    d400s_charge_mode_4_set mode4_set;           //15625
    rw_cmd_struct rw_cmd;
}d400s_hub_set;

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
            uint8_t revd : 1;    // Bit7:  预留
            uint8_t revd2 :8;
		} bit;						
	};
} d400s_work_ctrl;//0x08FAFFXX 逆变器CAN 单帧控制， data[8]
/**
 * @brief Type = 0x48 DCDC模块通用 信息区
 *
 */
typedef struct {
    char d400s_type[12];             // CHARGER 1 型号
    uint16_t d400s_sn[4];              // CHARGER 1 SN  低字节在前
    uint16_t battery_type;              //输入电池类型 1:12V铅酸电池 2:24V铅酸电池
    energy_line_t energy_line;                //能量流动条
    uint16_t fault_charger1;                 // dc1故障
    uint16_t fault_dcdc;                 // dcdc故障
    uint16_t proctect_dcdc;                 // dc保护
    uint16_t revd[7];                   //预留    
    d400s_work_ctrl ctrl_mode;                      //工作模式
    int32_t total_input_power;          // 所有DC通道总的进入功率  
    int32_t total_output_power;         // 所有DC通道总的输出功率 
    d400s_dc_info dc_info[6];           //dc 电压 、电流、功率     
    int32_t TotalInputEnergy;           //电量 0.1kwh
    d400s_dc_energy_info energy_info[6];   // 面向双向DC口定义，进出DCDC模块的能量信息
    uint16_t dcdc_SoftwareType;             //0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块 ；11-内置HMI模块；12-内置HMI模块2；13--RF穿墙模块；14-DC-HUB；15-AC-HUB；16-DC-DC；
    uint32_t dcdc_SoftwareVersion;          //软件版本号 例：1001.11； 填充值：100111 15582~15583
    rw_cmd_struct rw_cmd;
}d400s_common_info_struct;

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
    rw_cmd_struct rw_cmd;
}d400s_charger_set_struct;




typedef struct {
    uint8_t             devId;         // 当前数据属于哪个节点
    uint8_t             online;//1-设备在线；0-超时清零
    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
/*

并机排序之后才set，排地址时候清零
	*/    
    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

    dc_ac_hub_set       dc_ac_hub_setting;
    ac_hub_info_struct  ac_hub_info;    // AC_HUB信息
}ac_hub_can_node_struct;



typedef struct {
    uint8_t             devId;         // 当前数据属于哪个节点
    uint8_t             online;//1-设备在线；0-超时清零
    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
/*

并机排序之后才set，排地址时候清零
	*/    
    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

    dc_hub_info_struct  dc_hub_info;    // DC_HUB信息
}dc_hub_can_node_struct;



typedef struct {
    uint8_t             devId;         // 当前数据属于哪个节点
    uint8_t             online;//1-设备在线；0-超时清零
    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
/*

并机排序之后才set，排地址时候清零
	*/    
    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

}ATS_can_node_struct;

typedef struct {
    uint8_t             devId;         // 当前数据属于哪个节点
    uint8_t             online;//1-设备在线；0-超时清零
    uint16_t            alive_time;//CAN接收超时计数器，递减，0表示通讯识别
/*

并机排序之后才set，排地址时候清零
	*/    
    uint16_t            setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；

    iot_11000_can_struct iot_can_11000;  //  
    d400s_common_info_struct   d400s_common_info;    // d400s 通用信息区
    d400s_charger_set_struct   d400s_charger_set;    // d400s 通用CHARGER 1 设置区信息
}d400s_can_node_struct;


#pragma pack()
#endif
