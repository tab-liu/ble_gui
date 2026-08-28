#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>
#include <string.h>

#include "can_data.h"
//#include "can_pack.h"

#include "uart_device_process.h"
#include "tou_relay_ctrl.h"

#define WRIET_DATA_START        0x60    // CAN 写入指令
#define WRITE_DATA              0x61
#define WIRTE_DATA_ACK          0x62


#define READ_DATA_CMD           0x63    // CAN 读取指令
#define READ_RESP_DATA_START    0x64
#define READ_RESP_DATA          0x65



//step:
#define	STEP_CAN_PARALLEL_INIT		0//默认
//#define	STEP_CAN_PARALLEL_TRIGER	1//	1-设备发现触发帧（STEP1）
//#define	STEP_CAN_PARALLEL_SEND_SN	2//	2-设备发现SN信息上报帧（STEP2）

#define	STEP_CAN_PARALLEL_FINISH	3//	3-设备发现完成帧（STEP3）

#define	STEP_CAN_GROUP_PARALLEL_TRIGER	5//	1-设备发现触发帧（STEP1）
#define	STEP_CAN_GROUP_PARALLEL_SEND_SN	6//	2-设备发现SN信息上报帧（STEP2）

#define	STEP_CAN_PARALLEL_AFTER		7//


#define	STEP_CAN_PARALLEL_DELAY1		30//step切换延迟，秒
#define	STEP_CAN_PARALLEL_DELAY2		10//

#define	STEP_CAN_SEND_PERIOD		3//300ms 主设备切换延迟
#define	STEP_CAN_PARALLEL_INTERVAL		100//10000ms 重排地址间隔


typedef enum
{
    COMPLETE = 0,
    WAITTING,
    RECEIVING,
}cmd_status;


typedef enum
{
    HIGHEST = 0,
    LEVEL0 = 0,
    LEVEL1,
    LEVEL2,
    LEVEL3,
    LEVEL4,
    LEVEL5,
    LEVEL6,
    LOWEST = LEVEL6,
    INVLVEL,
}cmd_priority;

//reals.can_poll[]数组序号
enum {
	INDEX_INV_TYPE_BASE_11H=0,
	INDEX_INV_TYPE_DATA_17H=1,
	INDEX_INV_TYPE_LINE_12H=2,
	INDEX_INV_TYPE_PV_14H=3,
	INDEX_INV_TYPE_GRID_15H=4,
	INDEX_INV_TYPE_LOAD_16H=5,
	INDEX_INV_TYPE_METER_18H=6,
	INDEX_INV_TYPE_GEN_19H=7,
	INDEX_INV_TYPE_CONFIG00_1AH=8,
	INDEX_INV_TYPE_CONFIG01_1BH=9,
	INDEX_INV_TYPE_CONFIG03_1DH=10,
	INDEX_INV_TYPE_WIFI_REPORT_24H=11,
	INDEX_INV_TYPE_WIFI_PARAM_25H=12,
	INDEX_INV_TYPE_CERT_27H=13,
	INDEX_PACK_TYPE_BASE_51H=14,
	INDEX_PACK_TYPE_EXTEND_52H=15,
	INDEX_PACK_TYPE_ABOUT_54H=16,
	INDEX_PACK_TYPE_CONFIG_55H=17,
	INDEX_PACK_TYPE_DEBUG1_56H=18,
	INDEX_PACK_TYPE_BMUSTATE_57H=19,
	INDEX_IOT_TYPE_INFO_01H=20,
	INDEX_IOT_TYPE_SET_02H=21,
	INDEX_INV_TYPE_ABOUT_13H=22,
	INDEX_MODULE_TYPE_AC_HUB_INFO_42H=23,
	INDEX_MODULE_TYPE_DC_HUB_INFO_41H=24,
    INDEX_MODULE_TYPE_AC_HUB_INFO_40H=25,
//	INDEX_INV_TYPE_ABOUT_13H=25,

};

//#define MAX_NUM_CAN_TYPE	(25)//30 2

//并机排序属性
typedef enum
{
	Can_Addr_Parallel_SnSame=0x1,	//CAN sn 重复
    Can_Addr_Parallel_SameAlarm=0x2,	//IOT并机重复报警
    Can_Addr_Parallel_SendBindChk=0x4,	//IOT发送一次0xAA查询指令
    Can_Addr_Parallel_SendBindErr=0x8,	//IOT绑定SN码和本机ARM绑定SN码不一致
}Can_Addr_Parallel_Attr;

typedef union
{
    uint32_t all;
    struct
    {
        uint8_t src : 8;       // 8bit 源地址
        uint8_t dst : 8;       // 8bit 目标地址
        uint8_t funcode  : 8;  // 8bit CAN帧功能
        uint8_t page : 2;      // 2bit 页编码(0-3) 每页255帧
        uint8_t priority : 3;  // 3bit
    }bit;
}IdStruct;


#pragma pack(1)
//typedef struct
//{
//    uint8_t  type;
//    uint16_t start;
//    uint16_t total;
//    union {
//        uint16_t crc16;
//        uint8_t  reserve[2];
//    };
//    uint8_t nc;
//}CmdStruct;

// typedef struct
// {
//     uint8_t  type;
//     uint16_t start;
//     uint16_t total;
//     uint16_t blockCrc16;
//     uint8_t  revd;
// }DataStruct_0x60;// 0x60


typedef struct
{
    uint8_t  type;
    uint8_t  seq;
    uint8_t  bytes[6];
}DataStruct;// 0x61 0x65


typedef struct
{
    uint8_t  type;
    uint16_t start;
    uint16_t total;
    uint16_t append;    // 附加信息,如果total = 0, 附加信息为错误码,反之,附加信息为crc16
}AckStruct;

typedef struct
{
    uint8_t  waitComplete;
    uint8_t  type;
    uint8_t  page;
	uint8_t	 dst;
    uint16_t start;
    uint16_t total;
    uint16_t errCode;
}HandleInfo;

typedef struct {
	uint8_t node_id;//not use,0
	IdStruct can_id;
	uint8_t data_type;
	uint32_t offset;
	uint32_t data_len;
	uint8_t *data;
} can_tx_rx_cmd_t;

typedef can_tx_rx_cmd_t can_recv_cmd_t;
typedef can_tx_rx_cmd_t can_send_cmd_t;

enum {
	CAN_RX_TYPE_HISTORIC_DATA = 0xFA,
};

#define CAN_RX_CMD_TYPE_INIT  	{	\
	CAN_RX_TYPE_HISTORIC_DATA,		\
}

#define CAN_RX_TEMP_SIZE		1100



//windy:基于Modbus beta寄存器创建结构体,包含INV和 pack汇总，不含单pack
typedef struct {
    MOD_STRUCT_reg00000 mod_reg00000; //  6~16, 其他INV内部的IOT
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
    MOD_STRUCT_reg02300 mod_reg02300_Inv_set02_struct; //2300~2400  电网认证区
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
    // MOD_STRUCT_reg06300 mod_reg06300_Pack_cell[18]; //6300~xxx  pack电芯信息
    MOD_STRUCT_reg07000 mod_reg07000_Pack_set; //7000~7004  pack设置区
    // MOD_STRUCT_reg07200 mod_reg07200_Pack_bmu[18]; //7200~xxx      BMU信息区
    MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040    IOT信息区（只读，面向客户）
    MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
    MOD_STRUCT_reg13000 mod_reg13000_3rd_WIFI; //第3方WiFi接入信息管理
    MOD_STRUCT_reg13500 mod_reg13500_mesh; //13500~13511   WIFI MESH区
    MOD_STRUCT_reg13600 mod_reg13600_open; //13600~13810 开放区
//    MOD_STRUCT_reg14000 mod_reg14000_HMI_info;////14000~14013    HMI信息区
    MOD_STRUCT_reg14500 mod_reg14500_SmartPlug_info;//14500~14525 智能插座只读区（周期上报）
    MOD_STRUCT_reg14700 mod_reg14700_SmartPlug_set;//14700~14731 智能插座设置区
    MOD_STRUCT_reg15500 mod_reg15500_D400s_info;//15500~15583 CHARGER 1 信息区
    MOD_STRUCT_reg15600 mod_reg15600_D400s_set;//15600~15612 CHARGER 1 设置区
    MOD_STRUCT_reg15700 mod_reg15700_Dc_Hub_info;//15700~15749 dchub信息区
    MOD_STRUCT_reg15750 mod_reg15750_Dc_Ac_Hub_set;//15750~15760 dc、achub设置区
    MOD_STRUCT_reg15800 mod_reg15800_Ac_Hub_info;//15800~15850 achub信息区
    MOD_STRUCT_reg19000 mod_reg19000_Relay_ctrl;//19000~19031 继电器智能控制区1
    MOD_STRUCT_reg19100 mod_reg19100_Relay_ctrl_2;//19100~19139 继电器智能控制区2
    MOD_STRUCT_reg19200 mod_reg19200_Backup_power;//19200~19231 预约备电区
    MOD_STRUCT_reg19300 mod_reg19300_Relay_ctrl_3;//19300~19331 继电器智能控制区3
	MOD_STRUCT_reg21000	mod_reg21000_bind;//绑定帧的读写不一样
	MOD_STRUCT_reg21000_WR	mod_reg21000_bind_WR;
	MOD_STRUCT_reg22000	mod_reg22000_net_server_2rd;
    MOD_STRUCT_reg29700 mod_reg29700_IOT_info;//29700~29720 IOT标定区（从模式写入）
    MOD_STRUCT_reg40000 mod_reg40000_transparent;//40000~4511 透传区域
}MOD_STRUCT_Inv;//AP300 modbus beta总变量结构体


//windy:基于Modbus beta寄存器创建结构体, 单独pack部分
typedef struct {
    // MOD_STRUCT_reg06000 mod_reg06000_Pack_sum; //6000~6030    pack主包汇总信息
    MOD_STRUCT_reg06100 mod_reg06100_Pack_each; //6100~6203  pack单包信息
    MOD_STRUCT_reg06300 mod_reg06300_Pack_cell[18]; //6300~xxx  pack电芯信息
    MOD_STRUCT_reg07000 mod_reg07000_Pack_set; //7000~7004  pack设置区
    MOD_STRUCT_reg07200 mod_reg07200_Pack_bmu[18]; //7200~xxx      BMU信息区
}MOD_STRUCT_Pack;//电池包 modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg00100 mod_reg00100_AppPage1; //100~168  APP首页数据（汇总）
}MOD_STRUCT_AcHub;//电池包 modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040    IOT信息区（只读，面向客户）
    MOD_STRUCT_reg15500 mod_reg15500_D400s_info;//15500~15583 CHARGER 1 信息区
    MOD_STRUCT_reg15600 mod_reg15600_D400s_set;//15600~15612 CHARGER 1 设置区
}MOD_STRUCT_D400S;//电池包 modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg01700 mod_reg01700_meter; //1700 1800  电表只读区
    MOD_STRUCT_reg01900 mod_reg01900_meter; //1900 电表设置区
} MOD_STRUCT_Meter;//电表 modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg14500 mod_reg14500_SmartPlug_info;//14500~14525 智能插座只读区（周期上报）
}MOD_STRUCT_PLUG;//S1 plug设备modbus beta总变量结构体

typedef struct
{
	//share
	uint16_t modbus_version; //识别到的modbus协议版本：1- alpha;2-beta

	MOD_STRUCT_reg00000 mod_reg00000; //  6~16,

	//Alpha modbus
	// md_alpha_reg0000_t alpha_inv_info_0000;
	// md_alpha_reg3000_t alpha_set_03000;

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
	// md_alpha_reg0000_t alpha_inv_info_0000;
	// md_alpha_reg3000_t alpha_set_03000;

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
//    MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区
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

//	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
//	MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
//	MOD_STRUCT_reg15500 mod_reg15500_DCDC_info;//16000~16040	DCDC模块通用 信息区
//	MOD_STRUCT_reg15600 mod_reg15600_DCDC_set;//15600~15640	DCDC模块通用 设置区
//    MOD_STRUCT_reg13500 mod_reg13500_mesh; //13500~13511   WIFI MESH区

//	MOD_STRUCT_reg21000	mod_reg21000_bind;//绑定帧的读写不一样
//	MOD_STRUCT_reg21000_WR	mod_reg21000_bind_WR;

//	MOD_STRUCT_reg22000 mod_reg22000_net_server_2rd;
//  MOD_STRUCT_reg29700 mod_reg29700_IOT_info;//29700~29720 IOT标定区（从模式写入）
} OTHER_INV_BETA_STRUCT;

typedef struct {
    OTHER_INV_DATA_STRUCT  bind_dev; // 需要绑定的设备，例如蓝牙绑定逆变器
	MOD_STRUCT_PLUG Plug[PLUG_MAX_NUM + 1];
	MOD_STRUCT_Meter Meter[NET_WIFI_METER_POINT + 1];
    OTHER_INV_BETA_STRUCT other_inv[NET_WIFI_INVBAT_POINT_MINI+1];
}STRUCT_OTHER_READ;
extern STRUCT_OTHER_READ g_other_rd;

typedef struct {
    OTHER_INV_DATA_STRUCT bind_dev;
	OTHER_INV_BETA_STRUCT other_inv;
    MOD_STRUCT_PLUG Plug;
//plug
}STRUCT_OTHER_WRITE;//
extern STRUCT_OTHER_WRITE g_other_wr;

extern MOD_STRUCT_Inv	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)+1];//[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)];//微逆设备modbus beta总变量结构体
extern MOD_STRUCT_Inv	Inv_WR;//modbus写缓存 [(INV_MAX_NUM*DEV_MAIN_NODE_MAX)]
extern MOD_STRUCT_Pack Inv_Pack[PACK_MAX_NUM*DEV_MAIN_NODE_MAX];
extern MOD_STRUCT_Pack Inv_Pack_Slave[DEV_PACK_NUM_MAX];
extern MOD_STRUCT_Inv Plug[PLUG_MAX_NUM+1];//
extern MOD_STRUCT_AcHub Inv_AcHub;//用于记录ACHUB需要单独记录的数据，当前使用：100段寄存器
extern MOD_STRUCT_D400S Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)+1];//用于记录D400S需要单独记录的数据，当前使用：11000段寄存器
extern MOD_STRUCT_Meter Meter[METER_MAX_NUM + 1];  // 电表 modbus beta总变量结构体
extern MOD_STRUCT_Meter Meter_wr;

extern uint8_t MOD_STRUCT_len_reg02000[];
extern uint8_t MOD_STRUCT_len_reg02200[];
extern uint8_t MOD_STRUCT_len_reg02300[];
extern uint8_t MOD_STRUCT_len_reg02400[];


typedef struct {
//	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
	IOT_INFO_STRUCT_COMMON_REG_11000_0x1
    rw_cmd_struct   rw_cmd;
}iot_can_node_struct_reg11000;
typedef struct {
//	MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
	IOT_SET_STRUCT_COMMON_REG_12000_0x2
    rw_cmd_struct   rw_cmd;
}iot_can_node_struct_reg12000;

typedef struct {
	IOT_SET_STRUCT_COMMON_REG_12000_0x2

} iot_can_node_struct_reg12000_mini;

/**
 * @brief type= 0xfa
 *
 */
typedef struct {
	IOT_SET_STRUCT_COMMON_REG_file
    rw_cmd_struct   rw_cmd;
}iot_file_can_masetr_struct;

/**
 * @brief type= 0xfb
 *
 */
typedef struct {
	IOT_SET_STRUCT_COMMON_REG_file
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


}iot_can_node_struct;//iot read

typedef struct {
	iot_can_node_struct_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
    iot_file_can_masetr_struct  iot_can_masetr_file;  //  数据通道
    iot_file_can_slave_struct   iot_can_slave_file;  //  数据通道
}iot_can_node_struct_write;//iot write


//windy:基于can beta寄存器创建结构体
typedef struct {
	/* 保存每条主节点上挂载的逆变数据与PACK数据
	一个DEV节点可能包含多个逆变器或电池包

[x][end]表示汇总
	*/
	inv_node_struct 	inv_data[INV_MAX_NUM+1];//加一的部分是原来summary的部分;windy end+1存储汇总
	pack_node_struct	pack_data[PACK_MAX_NUM+1];
	iot_can_node_struct iot_data[IOT_MAX_NUM];//[IOT_MAX_NUM+1];//windy add,IOT only 1
    dc_hub_can_node_struct dc_hub_data[DC_HUB_MAX_NUM+1];
    d400s_can_node_struct d400s_data[D400S_MAX_NUM+1];
//    ac_hub_can_node_struct ac_hub_data[AC_HUB_MAX_NUM+1];//achub数据全部放在汇总处:DEV_MAIN_NODE_MAX,1

}MOD_STRUCT_Inv_READ;//can设备 INV+BAT beta总变量结构体

typedef struct {
	bk_inv_node_set 	bk_inv_dev_set; // app 通过modbus设置的数据先保存在这里，然后再转换成CAN命令
	bk_pack_node_set	bk_pack_dev_set; // app 通过modbus设置的数据先保存在这里，然后再转换成CAN命令
	iot_can_node_struct_write mod_IOT_set;//iot set

    iot_factory_struct  factory; // iot标定信息,can type=0xFF

}MOD_STRUCT_Inv_WRITE;//can beta写 总变量结构体

typedef struct {
    dc_ac_hub_set dc_ac_hub_setting;
    ac_hub_can_node_struct ac_hub_data[AC_HUB_MAX_NUM]; //achub数据全部放在汇总处:AC_HUB_MAX_NUM,1
    ATS_can_node_struct ATS_data[ATS_MAX_NUM];          //ATS数据全部放在汇总处:ATS_MAX_NUM,1
//AT1 ATS
//EMS
}MOD_STRUCT_Inv_MIX_READ;//can设备 杂项 结构体




typedef struct {
    dc_ac_hub_set dc_ac_hub_setting;
    d400s_hub_set d400s_hub_sets;//d400s设置
}MOD_STRUCT_Inv_MIX_WRITE;


#define CAN_DATA_PACK_NUM   24


typedef struct {
    char type_ascii[12]; // pack类型
    uint32_t version[10];
	uint8_t count_version;//一个类型号下面版本号数量
	uint8_t count_online;//一个类型号下面实际在线的电池包数量 6154
} Data;

typedef struct {
	char type_ascii[12]; 						// pack类型
	uint64_t BMU_SN[CAN_DATA_PACK_NUM];						// BMU SN码
	uint32_t BMU_Detail[CAN_DATA_PACK_NUM];					// BMU 故障和保护等信息
	uint16_t Single_Cell_and_Temp_Count[CAN_DATA_PACK_NUM];	// 单个BMU中电芯数量及NTC数量
	uint8_t BMU_Type[CAN_DATA_PACK_NUM];						// BMU类型 等同于pack类型
	uint32_t BMU_Version[CAN_DATA_PACK_NUM];					// 版本号
	uint8_t count_online;						// 一个类型号下面实际在线的电池包数量 6154
} Data_Pack;//某一个类型电池包7200段需要的信息

typedef struct {
    uint32_t version_ARM;
	uint32_t version_DSP;
	uint32_t version_BMS;
	uint32_t version_DCHub;
} Data_INV;

typedef struct {
    uint8_t type_ascii; // 软件类型	0x01:ARM 0x02:DSP 0x03:BMS
    uint32_t version[3];
	uint8_t count_version;//一个类型号下面版本号数量
} Data_INV2;


extern MOD_STRUCT_Inv_READ		Inv_can[DEV_MAIN_NODE_MAX+1];//can设备 INV+BAT beta总变量结构体 [end]表示汇总
extern MOD_STRUCT_Inv_WRITE	Inv_can_WR;//can写缓存

extern MOD_STRUCT_Inv_MIX_READ	    Inv_can_mix;//一个大系统中 仅一个模块，杂项混合
extern MOD_STRUCT_Inv_MIX_WRITE	Inv_can_mix_WR;//一个大系统中 仅一个模块，杂项混合



#pragma pack()

//20240715 windy tbd:待增加 IOT CAN节点收发

//typedef struct {
//	/*
//[x][end]表示汇总
//	*/
//	inv_node_struct 	iot_data;//加一的部分是原来summary的部分;windy end+1存储汇总
//
//}MOD_STRUCT_Iot_READ;//can设备 INV+BAT beta总变量结构体



/*
windy 20240613
用前面结构体替换后面结构体
Inv_can[].inv_data[]
Inv_can[].pack_data[]
Inv_can_WR.bk_inv_dev_set
Inv_can_WR.bk_pack_dev_set


g_device_data.inv_data[DEV_MAIN_NODE_MAX][INV_MAX_NUM];
g_device_data.pack_data[DEV_MAIN_NODE_MAX][PACK_MAX_NUM];
g_device_data.bk_inv_dev_set; // app 通过modbus设置的数据先保存在这里，然后再转换成CAN命令
g_device_data.bk_pack_dev_set; // app 通过modbus设置的数据先保存在这里，然后再转换成CAN命令

*/

void iot_Inv_data_init(void);
void iot_can_data_init(void);
//void iot_can_task(void * pvParameters);

void iot_to_can_cmd_task(void);//(void * pvParameters);
void iot_can_low_level_task(void * pvParameters);
void iot_can_high_level_task(void * pvParameters);

uint8_t CanGetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, uint16_t data_len, uint8_t dst_addr);
uint8_t CanSetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, const uint8_t *pdata, uint16_t data_len, uint8_t dst_addr);
void iot_can_cmd_check(void);
uint8_t CanAckData(uint8_t node_id, uint32_t id, const uint8_t *raw, uint8_t len);
uint8_t CanAckStart(uint8_t node_id, uint32_t id, uint16_t type, uint16_t start, uint16_t len, uint16_t crc16);
const uint16_t* vLookupDataTab_Can(uint8_t master_source,uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, can_data_label *can_label, uint16_t *can_cmd_num ,reg2_position_t* reg_position);
void Device_Can_Address_Parallel_Step(void);
void Iot_Self_Data_To_Can_data(void);
void Can_Beta_Tx_Block_Timeout_Check(void);

#ifdef FUNC_DEV_TRIGGER_BIND
void Can_binding_triger_chk(void);
#else
#define Can_binding_triger_chk()
#endif
uint8_t Can_Tx_single_binding_triger(void); //100ms cycle
uint8_t Can_Tx_single_binding_Check(void); //100ms cycle
uint8_t Can_Tx_single_binding_ack(uint8_t* sn_bind); //100ms cycle


void judge_inv_min_version(void);//扫描所有online==1的node，按料号(ver/100)合并，BMS置尾
void can_data_poll_index_set(int8_t index);


void CAN_IOT_Parameter_WR(void);//1s cycle
void  CAN_DEV_SN_Get(uint8_t devId);
void  CAN_DEV_INV_ACHUB_SN_Get_to_Save(void);

uint8_t Device_Can_Address_Parallel_SetTriger(uint16_t time) ;
void Device_Can_Address_Parallel_Triger(void); //100ms cycle
void CanCmdGenerate(uint8_t md_addr, can_data_label *cmd_label, uint16_t num);
void CAN_Dev_PowerOff_Check(void);
void can_data_poll_mask_set(uint32_t uMask,uint8_t flag);
void RTC_Update_iot_to_can(void);
#endif
