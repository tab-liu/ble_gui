#ifndef CAN_DATA_H
#define CAN_DATA_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "can_type.h"
#include "inv_define.h"
#include "pack_define.h"
#include "comm_define.h"
#include "parts_type.h"
#include "md2can.h"
#include "parameter.h"

//#define SW_VERSION_COMBOX 		802001//
//#define SW_VERSION_AT1 			801801//
//#define SW_VERSION_EMS 			905266//
//#define SW_VERSION_AC380 		802650//

#define IOT_TYPE_COMBOX			"COMBOX"//
#define INV_TYPE_A100S			"A100S"//

#define IOT_TYPE_AT1			"AT1"//
#define IOT_TYPE_EMS			"EBOX"//
#define IOT_TYPE_IOT			"IOT"//
#define IOT_TYPE_DSP			"SolarX 4K"//"D400S"//"AP300"//

#define IOT_TYPE_BLE_DEFAULT			"IOTB"//


//#define IOT_VERSION 			SW_VERSION_AC380//802001//905206
//#define IOT_VERSION 			905266//802001//905206
#define HMI1_VERSION            606201
#define HMI2_VERSION            606301

//#define HW_VERSION				2
//#define SUB_VERSION				7


#define FRAME_HEAD            	2
#define FRAME_DATA_BYTES      	6


#define DEVICE_SOFT_COUNT       20
#define DEVICE_OTA_PCT          0x78    //
#define DEVICE_IN_BOOT          0x79    //

#define INV_OFFLINE_TIME        	10000                            // 10s未收到节点数据,节点掉线
#define DC_HUB_OFFLINE_TIME        	10000                            // 10s未收到节点数据,节点掉线
#define AC_HUB_OFFLINE_TIME        	10000                            // 10s未收到节点数据,节点掉线
#define PACK_OFFLINE_TIME        	20000                            // 20s未收到节点数据,节点掉线
#define IOT_OFFLINE_TIME        	12000                            // 12s未收到节点数据,节点掉线
#define DCDC_OFFLINE_TIME        	10000                            // 10s未收到节点数据,节点掉线

#define HEART_BEAT_FRAME_PERIOD_MS		((IOT_OFFLINE_TIME-2000)>>1)//5s
#define	CAN_MASTER_CHANGE_DELAY		IOT_OFFLINE_TIME//2倍周期，(HEART_BEAT_FRAME_PERIOD_MS/100*2)//100//10s主设备切换延迟,1ms->100ms



/* 设备主节点最大数量，每条主节点上都可以挂多个逆变子节点与PACK子节点 */

/* 多路CAN ID */
#define LOCAL_CAN_ID			0
#define EXT_CAN0_ID				1
#define EXT_CAN1_ID				2

//#define STATE_COUNT
#define CAN_BLOCK_RX_TIME_OUT_FAST      	10//快速结束，10ms cycle
#define CAN_BLOCK_RX_TIME_OUT_SLOW      	500//慢速超时结束，500ms cycle
#define CAN_BLOCK_RX_TIME_OUT_READ_BACK     20//写入回读超时，20ms cycle

//can type UDT
#define UDT_QUERY_CMD			0xFA	// 查询历史数据指令,上级->IOT 写
#define UDT_RESP_CMD			0xFB	// 响应历史数据指令,IOT->上级 写

#define COMMON_TYPE_TRANSMIT_27H    0x27

typedef enum
{
    ERR_NONE = 0,
    ERR_TYPE,
    ERR_RANGE,
    ERR_LOSE,
    ERR_CRC,
    ERR_VALUE,
    ERR_TIMES,
    ERR_ADDR,
    ERR_MEMONY,
	//ERR_DEV_ABORT = 12,   //与 “lcd_ota.h” 定义冲突
    ERR_WAIT = 0xFF,
}ERR_CODE;

#pragma pack(1)

////////////////////////////////////////device////////////////////////////////////////////////
////////////////////////////////////////device////////////////////////////////////////////////
////////////////////////////////////////device////////////////////////////////////////////////
typedef struct {
    uint16_t len;
    uint8_t *pdata;
}dev_data;


typedef struct {
    uint16_t internal;  /* 间隔时间 */
    uint8_t  addr;      // 设备地址
    uint8_t  type;      // 数据类型
    uint32_t max_len;   // 数据最大长度

    // uint8_t *pdata;
    // rw_cmd_struct *cmd;
}data_abstract_struct;


typedef struct {
    uint8_t is_auto;        /* true按时间周期自动读取; false触发读取 */
    uint8_t is_expire;      /* 时间已经到期 */
    uint16_t left_time;     /* 剩余时间 */
    uint8_t used_cnt;       /* 触发读取次数，已经使用次数 */
    uint8_t usable_cnt;     /* 一次触发，可读取总共可用次数 */
    const data_abstract_struct *abstract;
}can_cmd_array;

typedef struct {
    uint16_t begin;
    uint16_t type;
    uint32_t version;
    uint16_t size;
/*
	高8bit-选择需要升级的组（1：INV-OTA；2：PACK-OTA；3: IOT-OTA; 4-LCD）
	低8bit-选择该组下哪个ID需要升级0~0xFF(从0开始，0表示#1；0xFF表示广播)
	*/	
    uint16_t group;
}ota_cmd_struct;

typedef struct {
    uint8_t level;  // ota 处于哪个阶段,0：IOT发送给设备，1：设备发送给子设备-低字节
    uint8_t isOta;  // 0: 未进行OTA; 1: 正在升级-高字节
    uint8_t where;  // 文件传输位置
    uint8_t type;   // 文件类型 最高位置1表示正在升级
    uint8_t errCode;// 错误码
    uint8_t pct;    // 文件传输进度
}ota_group_status;

typedef struct {

    int16_t *pgValue;   // modbus 一个寄存器
    uint32_t mask;      //
    int8_t shift;       // 整数左移，负数右移
    uint8_t is_lsb;        // 是否交换字节序

    can_data_label can_label;
}reg_array;

typedef struct
{
    uint8_t  type;
    uint16_t start;
    uint16_t total;
    union {
        uint16_t crc16;
        uint8_t  reserve[2];
    };
    uint8_t nc;
}CmdStruct;

/**
 * @brief Type = 0x27
 */
typedef struct {
    uint16_t  param[512];
    rw_cmd_struct rw_cmd;
}common_transmit_struct;

typedef struct {

//	iot_can_node_struct_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
	iot_can_node_struct_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
    device_registers_t factory; // iot标定信息,can type=0xFF

    iot_file_can_masetr_struct  iot_can_master_file; //  数据通道
    iot_file_can_slave_struct   iot_can_slave_file;  //  数据通道
	common_transmit_struct common_transmission;
}can_node_struct_Iot_write;//iot read


typedef struct {
    dcdc_common_set_struct dcdc_common_set;    // d400s 通用CHARGER 1 设置区信息    
}can_node_struct_DCDC_write;//iot write

typedef union
{
    uint8_t all[8];
    struct
    {
        uint8_t index;
        uint8_t revd;
        uint8_t energy[6];
    };
}Can_Energy_Struct;


typedef struct {
    uint8_t              devId;
    uint8_t              online;
    uint16_t             alive_time;
    
    uint8_t             dev_access_state;   // 设备接入标志，用于设备接入日志模块 0:无效/离线，1：接入
    
	uint16_t			setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；
	
    dcdc_common_info_struct dcdc_common_info;    // d400s 通用信息区
    dcdc_common_set_struct dcdc_common_set;    // d400s 通用CHARGER 1 设置区信息
	iot_can_node_struct_reg11000 iot_info; //11000~11040	IOT信息区（只读，面向客户）
}can_node_struct_DCDC;//iot read


typedef struct {
    uint8_t              devId;
    uint8_t              online;
    uint16_t             alive_time;
    
    uint8_t             dev_access_state;   // 设备接入标志，用于设备接入日志模块 0:无效/离线，1：接入

    /*并机排序之后才set，排地址时候清零	*/    
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

    inv_set00_struct    inv_set00;  // inv设置区00
    inv_set01_struct    inv_set01;  // inv设置区01
    inv_set02_struct    inv_set02;  // inv设置区02
    inv_set03_struct    inv_set03;  // inv设置区03
    inv_log_struct      inv_log;       // inv日志
    inv_all_energy_struct inv_all_energy;	// inv 15年能量信息
    inv_energy_struct   inv_energy;		// inv 当年能量信息
    Can_Energy_Struct   inv_today_energy[40];
	common_transmit_struct common_transmission;
}can_node_struct_Inv;//

typedef struct {
    uint8_t              devId;
    uint8_t              online;
    uint16_t             alive_time;//在线计数器
    
    uint8_t             dev_access_state;   // 设备接入标志，用于设备接入日志模块 0:无效/离线，1：接入

	pack_announce_struct pack_announce;  // pack主机的广播信息
	pack_base_struct	 pack_base; 	 // pack单包主要信息,type=0x51
	pack_extend_struct	 pack_extend;	 // pack 扩展数据，type=0x52
	pack_about_struct	 pack_about;	 // 关于pack
	pack_config_struct	 pack_config;	 // pack 参数配置区
	pack_debug1_struct	 pack_debug1;	 // pack 调试数据,type= 0x56
	bms_paygo_info_t     pack_paygo;     // pack paygo数据
	common_transmit_struct common_transmission;
}can_node_struct_Pack;//

typedef struct {
    can_node_struct_DCDC Dcdc[(DCDC_MAX_NUM)+1];// .Dcdc;end元素用于汇总modbus slave0，需要IOT自身额外拟合
    can_node_struct_Inv Inv[(INV_MAX_NUM)+1];// .Inv
    can_node_struct_Iot Iot[IOT_MAX_NUM];//
    dc_hub_can_node_struct DCHUB[(DC_HUB_MAX_NUM)+1];//top_modbus_rd.DcHUB
    can_node_struct_Pack Pack[PACK_MAX_NUM+1];//INV_MAX_NUM 分3段：前：INV内部集成的PACK；中：独立PACK；尾部：汇总
//plug
}STRUCT_CAN_READ;// 


typedef struct {
    can_node_struct_Iot_write Iot;//

    can_node_struct_DCDC_write Dcdc;//top_modbus_rd.Dcdc;end元素用于汇总modbus slave0，需要IOT自身额外拟合
    can_node_struct_Inv Inv;//top_modbus_rd.Inv
    dc_ac_hub_can_node_wr_struct HUB;//top_modbus_rd.DcHUB
    can_node_struct_Pack Pack;//top_modbus_rd.Pack
//plug
}STRUCT_CAN_WRITE;// 

typedef struct {
	uint8_t node_id;					//指向某个主节点

	can_node_struct_Inv (*invs_info)[];		//指向主节点上的一组逆变器
	dc_hub_can_node_struct (*dchubs_info)[];		//指向主节点上的一组dchub
	ac_hub_can_node_struct (*achubs_info)[];		//指向主节点上的一组achub
	can_node_struct_Pack (*packs_info)[];	//指向主节点上的一组电池包
	can_node_struct_Iot (*iots_info)[];		//指向IoT节点
	can_node_struct_DCDC (*dcdc_s_info)[]; 	//指向主节点上的一组逆变器
} node_info_t;//借用此次指针指向 全局变量结构体，增强可移植性
#pragma pack()



// void CanDataInit(void);
void CanNodeOfflineCheck(uint16_t interval);
void CanNodeOfflineFree(void);

void CanCmdPackReset(void);
void CanCmdInvReset(void);
void CanCmdResetTime(uint8_t cmd_type);
//void CanCmdActivte(uint8_t cmd_type);
//void CanCmdCountDown(uint16_t interval);

uint8_t CanVerifyCmd(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, CmdStruct *p_cmd, dev_data *payload);
uint8_t CanVerifyData(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, uint8_t type, uint8_t seq, uint8_t len, const uint8_t *payload);
// uint8_t CanVerifySet(uint8_t devId, uint8_t type);

void debug_detail_value(node_info_t *node, uint8_t devid, uint8_t type);
void CAN_To_Modbus_Read_Info_Process_Beta(uint8_t type, uint8_t devId);
// int8_t Can_ID_Check(uint32_t CanId, uint8_t *level1_addr, uint8_t *level2_addr);
int8_t Can_ID_Check_Beta_Multi_Read(uint32_t CanId);
int8_t Can_ID_Check_Beta_Multi_Write(uint32_t CanId);
int8_t Can_ID_Check_Beta_Multi_OTA(uint32_t CanId);
uint8_t esp_canbus_myself_address(void);
//void set_canbus_dcdc_work(uint8_t work_mode);
//uint8_t get_canbus_dcdc_work(void);
void sys_new_can_data_resend(can_cmd_queue_struct *can_cmd, uint8_t modbus_slave_address) ;
int8_t Can_ID_Analyze(uint32_t CanId, uint8_t *level1_addr, uint8_t *level2_addr);
void Sum_CAN_To_Modbus_Read_Info_Process(void);
int8_t Can_ID_Non_native_IOT_Check(uint8_t addr_source);
uint8_t Modbus_Addr_To_Can_Id(uint8_t md_addr);

#endif
