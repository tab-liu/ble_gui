#ifndef CAN_DATA_H
#define CAN_DATA_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "can_type.h"
#include "inv_type.h"
#include "pack_type.h"
#include "iot_type.h"
#include "parts_type.h"
//#include "can_protocol.h"
// #include "uart_device_process.h"
// #include "iot_period_task.h"
#include "comm_define.h"

#define SW_VERSION_COMBOX 		802001//
#define SW_VERSION_AT1 			801801//
#define SW_VERSION_EMS 			905266//
#define SW_VERSION_AC380 		802613//

#define IOT_TYPE_COMBOX			"COMBOX"//
#define INV_TYPE_A100S			"A100S"//

#define IOT_TYPE_AT1			"AT1"//
#define IOT_TYPE_EMS			"EBOX"//
#define IOT_TYPE_IOT			"IOT"//
#define IOT_TYPE_AC380			"AP300"//
#define IOT_TYPE_HA1			"HA1"//
#define IOT_TYPE_AP200			"AP200"//
#define IOT_TYPE_AP300			"AP300"//
#define IOT_TYPE_AP500			"AP500"//
#define IOT_TYPE_AP300V2			"AP300V2"//
#define IOT_TYPE_BLE_DEFAULT			"IOTB"//

#define HMI1_VERSION            606201
#define HMI2_VERSION            606301

#define IOT_VERSION_AP200       100600108  // 变更时间：2026.08.21
#define IOT_VERSION_AP300       802616     // 变更时间：2026.07.27
// #define IOT_VERSION_AP300V2     100820101  // 变更时间：2026.
// #define IOT_VERSION_AP500       100830101  // 变更时间：2026.

/*
COMBOX  (DT100)：8020**
AT1 (ATS柜)：8018**
*/
//#ifdef CONFIG_HARDWARE_DTU100
//#define IOT_TYPE_STR			IOT_TYPE_COMBOX//"IOT"//"WNRD01"//"DTU"
//#endif
//#ifdef CONFIG_HARDWARE_ATS
//#define IOT_TYPE_STR			IOT_TYPE_EMS//"IOT"//"WNRD01"//"DTU"
//#endif
#define IOT_TYPE_STR			IOT_TYPE_AC380//IOT_TYPE_EMS//

#define HW_VERSION				2
#define SUB_VERSION				7


#define FRAME_HEAD            	2
#define FRAME_DATA_BYTES      	6


#define DEVICE_SOFT_COUNT       20
#define DEVICE_OTA_PCT          0x78    //
#define DEVICE_IN_BOOT          0x79    //

#define INV_OFFLINE_TIME        	5000                            // 5s未收到节点数据,节点掉线
#define DC_HUB_OFFLINE_TIME        	10000                            // 10s未收到节点数据,节点掉线
#define D400S_OFFLINE_TIME        	10000                            // 10s未收到节点数据,节点掉线
#define AC_HUB_OFFLINE_TIME        	15000                            // 15s未收到节点数据,节点掉线，boot状态下是20s
#define ATS_OFFLINE_TIME        	10000                            // 10s未收到节点数据,节点掉线
#define PACK_OFFLINE_TIME        	20000                            // 20s未收到节点数据,节点掉线
#define IOT_OFFLINE_TIME        	12000                            // 12s未收到节点数据,节点掉线

#define HEART_BEAT_FRAME_PERIOD_MS		((IOT_OFFLINE_TIME-2000)>>1)//5s
#define	CAN_MASTER_CHANGE_DELAY		IOT_OFFLINE_TIME//2倍周期，(HEART_BEAT_FRAME_PERIOD_MS/100*2)//100//10s主设备切换延迟,1ms->100ms


/* 设备主节点最大数量，每条主节点上都可以挂多个逆变子节点与PACK子节点 */
#define DEV_MAIN_NODE_MAX 		3//windy CAN总线逆变器储能系统数量


#define DEV_INV_PACK_NUM        10//原电池方案使用寄存器数量
#define DEV_PACK_NUM_MAX 		50//当前电池包方案寄存器数量 0~49，当前定为最大10个电池包类型+最大40个电池包


/* 多路CAN ID */
#define LOCAL_CAN_ID			0
#define EXT_CAN0_ID				1
#define EXT_CAN1_ID				2

//#define STATE_COUNT
#define CAN_BLOCK_RX_TIME_OUT_FAST      	10//尾部，快速结束，10ms cycle,0.3s
#define CAN_BLOCK_RX_TIME_OUT_SLOW      	10//慢速超时结束，10ms cycle,1.5s


//can type UDT
#define UDT_QUERY_CMD			0xFA	// 查询历史数据指令,上级->IOT 写
#define UDT_RESP_CMD			0xFB	// 响应历史数据指令,IOT->上级 写


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
    uint8_t  type;					//modbus转can的指令数据类型
    uint16_t offset;				//数据偏移 是相对某一块CAN数据的偏移(eg:0x1a)
    uint16_t len;    				//数据长度

	/* 发送该指令后需要激活的指令类型
	 * 例如要读取逆变的历史日志，先设置历史日志页指令（type=INV_TYPE_CONFIG00，offset=offsetof(inv_set00_struct, ctrl_log_page)，len=sizeof(ctrl_log_page)）
	 * 然后激活历史日志上传指令（active_can_cmd_type=0x20,因为历史日志是触发读取）*/
    uint16_t active_can_cmd_type;
}can_data_label;

typedef struct {

    int16_t *pgValue;   // modbus 一个寄存器
    uint32_t mask;      //
    int8_t shift;       // 整数左移，负数右移
    uint8_t is_lsb;        // 是否交换字节序

    can_data_label can_label;
}reg_array;


typedef struct {
    can_data_label *cmd;
    uint16_t num; 			// 需要转换成多少条CAN命令
	uint8_t md_addr;		// modbus地址
} can_cmd_queue_struct;


//typedef struct {
//    uint32_t protocol_ver; // 协议版本号
//    uint32_t hardware_ver; // 硬件版本号
//    uint32_t software_ver; // 软件版本号
//    char     iot_type[12];
//    uint64_t iot_sn;        // sn码
//    uint64_t safe_code;     // 安全码
//    uint8_t cert_md5[48]; //三份证书的MD5值 
//
//	/* 系统架构发生变化后,使用0号主节点上的0号逆变设备的设备类型与SN码 */
//    char     dev_type[12];  // 下游设备SN码
//    uint64_t dev_sn;        // 下游设备SN码
//
//    uint8_t wifi_sta_auth;      // wifi 作为sta时 需要连接的路由器加密方式
//    char wifi_sta_ssid[32];     // wifi 作为sta时 需要连接的路由器名称
//    char wifi_sta_password[64]; // wifi 作为sta时 需要连接的路由器密码
//    sta_eanble_t sta_enable;       // bit0~7:密码高32字节有效标志(0xA5有效); bit8:开放式WiFi支持(1-支持)
//    char reserved[32];     // 预留
//    char could_dns[128];        // 服务器地址
//	config_status on_off;//cmd,12161
//	thunder_ctrl_t thunder_ctrl;
//	
//    char nc[20];                // 保留
//    uint16_t crc16;
//}dev_info_t;
//extern EXT_RAM_BSS_ATTR dev_info_t  SetData.dev_info_t;//windy AT1/DTU改存到EEPROM


typedef struct {
	/* 只用于APP切换不同的逆变与电池包 */
    uint16_t inv_id;     		// inv的ID号选择-通过ID来切换包
    uint16_t pack_id;    		// pack的ID号选择-通过ID来切换包

	/* 这两个ID只用于mqtt上传数据到服务器时，标识上传数据所属的逆变ID与电池包ID */
    uint16_t poll_inv_id;     	// 内部轮询 inv的ID号选择-通过ID来切换包
    uint16_t poll_pack_id;    	// 内部轮询 pack的ID号选择-通过ID来切换包

	uint16_t inv_ver_diff;		// 逆变固件版本差异标志
	uint16_t pack_ver_diff;		// PACK固件版本差异标志

    ota_cmd_struct      ota_cmd;        // ota 控制，该结构数据由上层应用下发到IoT
    ota_group_status    ota_group[16];  // ota 状态

	/* 用于ble上传数据到APP时，缓存待上传的设备数据 */
    inv_node_struct     inv_dev_node;   // 逆变单节点数据 - APP读取的逆变数据是这个地方给出
    pack_node_struct    pack_dev_node;  // pack单节点数据 - APP读取的pack数据是这个地方给出

	/* 用于mqtt客户端上传数据到服务器时，缓存待上传的设备数据 */
    inv_node_struct     poll_inv_dev_node;   // 逆变单节点数据 - 服务器读取的逆变数据是这个地方给出
    pack_node_struct    poll_pack_dev_node;  // pack单节点数据 - 服务器读取的pack数据是这个地方给出

    inv_node_struct     mcu_inv_dev_node;   // 逆变单节点数据 - 上报MCU的逆变数据是这个地方给出
    pack_node_struct    mcu_pack_dev_node;  // pack单节点数据 - 上报MCU的pack数据是这个地方给出

    iot_node_struct     iot_dev_node;   // iot节点数据 - APP读取的iot数据是这个地方给出 （唯一IOT节点）
	hmi_node_struct_t 	hmi_node;		// HMI节点数据
	bk_iot_node_set     bk_iot_dev_node;//iot_dev_node的 modbus slave 写变量
//    bk_inv_node_set     bk_inv_dev_set; // app 通过modbus设置的数据先保存在这里，然后再转换成CAN命令
//    bk_pack_node_set    bk_pack_dev_set; // app 通过modbus设置的数据先保存在这里，然后再转换成CAN命令

	/* APP切换节点ID，根据节点ID来切换不同主节点上的逆变与PACK数据
	 * 对于modbus来说，modbus的从机地址对应不同的节点ID
	 * modbus的0号地址代表广播地址，1号开始代表从机地址，节点ID等于modbus从机地址减1 */
	uint8_t 			app_node_id;//BLE read ,md_addr - 1

	/* 保存每条主节点上挂载的逆变数据与PACK数据
	一个DEV节点可能包含多个逆变器或电池包*/
//	inv_node_struct		inv_data[DEV_MAIN_NODE_MAX][INV_MAX_NUM];
//	pack_node_struct	pack_data[DEV_MAIN_NODE_MAX][PACK_MAX_NUM];
//	inv_node_struct		inv_summary;
//	pack_node_struct	pack_summary;
//	pack_node_struct	pack_grp_summary[DEV_MAIN_NODE_MAX];

//    charging_pile_struct charging_pile_node;//充电桩数据




}device_data_struct;



typedef struct {
    inv_node_struct     inv_data[DEV_MAIN_NODE_MAX][INV_MAX_NUM];
    pack_node_struct    pack_data[DEV_MAIN_NODE_MAX][PACK_MAX_NUM];
	inv_node_struct		inv_summary;
	pack_node_struct	pack_summary;
	pack_node_struct	pack_grp_summary[DEV_MAIN_NODE_MAX];
} upload_data_t;


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

#pragma pack()

typedef struct {
	uint8_t node_id;					//指向某个主节点

	inv_node_struct *inv_info;			//指向主节点上的单个逆变器
	inv_node_struct (*invs_info)[];		//指向主节点上的一组逆变器

    dc_hub_can_node_struct *dchub_info;			//指向主节点上的单个dchub
	dc_hub_can_node_struct (*dchubs_info)[];		//指向主节点上的一组dchub

	// ac_hub_can_node_struct *achub_info;			//指向主节点上的单个achub ac380目前固定只有一个achub
	ac_hub_can_node_struct (*achubs_info)[];		//指向主节点上的一组achub

	pack_node_struct *pack_info;		//指向主节点上的单个电池包
	pack_node_struct (*packs_info)[];	//指向主节点上的一组电池包

	iot_node_struct *iot_info;			//指向IoT节点 一个逆变器系统固定只有一个IOT
	//iot_node_struct (*iots_info)[];		//指向IoT节点   增加其他设备的IOT节点信息

    d400s_can_node_struct *d400s_info;		//指向主节点上的单个d400s
	d400s_can_node_struct (*d400s_s_info)[];	//指向主节点上的一组d400s

	
} node_info_t;

extern device_data_struct  g_device_data;
extern const uint8_t inv_cmd_count ;
extern const uint8_t pack_cmd_count;
extern const uint8_t iot_cmd_count ;


extern can_cmd_array   g_cmd_array_inv[];
extern can_cmd_array   g_cmd_array_pack[];
extern can_cmd_array   g_cmd_array_iot[];

void CanDataInit(void);
void CanNodeOfflineCheck(uint16_t interval);
void CanNodeOfflineFree(void);

void CanCmdPackReset(void);
void CanCmdInvReset(void);
void CanCmdResetTime(uint8_t cmd_type);
void CanCmdActivte(uint8_t cmd_type);
void CanCmdCountDown(uint16_t interval);

uint8_t CanVerifyCmd(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, CmdStruct *p_cmd, dev_data *payload);
uint8_t CanVerifyData(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, uint8_t type, uint8_t seq, uint8_t len, const uint8_t *payload);
// uint8_t CanVerifySet(uint8_t devId, uint8_t type);


void debug_detail_value(node_info_t *node, uint8_t devid, uint8_t type);
void CAN_To_Modbus_Read_Info_Process(uint8_t type);
void CAN_Dev_Ctrl_SetData_Check(uint8_t type);
int8_t Can_ID_Check(uint32_t CanId, uint8_t *level1_addr, uint8_t *level2_addr);
int8_t Can_ID_Check_Beta_Multi_Read(uint32_t CanId);
int8_t Can_ID_Check_Beta_Multi_Write(uint32_t CanId);
int8_t Can_ID_Check_Beta_Multi_OTA(uint32_t CanId);

	//将ACHUB的报警状态汇总到故障表中
void CAN_SetAlarmStateFromAcHub(uint8_t num,uint8_t unumpack);

#endif
