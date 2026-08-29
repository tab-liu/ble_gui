
#ifndef MODBUS_DATA_H
#define MODBUS_DATA_H


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "crc.h"
#include "comm_define.h"

#pragma pack(1)

// static const uint16_t modbus_ver = MODBUS_VERSION;//2006;


#define LSB2MSB(src)    (((src) >> 8) | ((src) << 8))
#define GET_BIT_VALUE(value, mask, shift)   (((shift) >= 0) ? (((value) & (mask)) << (shift)) : (((value) & (mask)) >> -(shift)))
#define SET_BIT_VALUE(value, mask, shift)   (((shift) >= 0) ? (((value) << (shift)) & (mask)) : (((value) >> -(shift)) & (mask)))

/*Modbus读取触发CAN读取延时*/
#define MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS    5

/*
OTA通道汇总 OTA_CHANEL
*/
typedef enum {	
	MD_CHL_INVALID = 0,
	MD_CHL_BLE,// 
	MD_CHL_BLE_CLIENT,//
	MD_CHL_WIFI_CLOUD,// 
	MD_CHL_WIFI_MESH,//无线设备间 
	MD_CHL_UART_DOWN,// 串口
	MD_CHL_CAN,//can
	MD_CHL_SELF,//IOT
	MD_CHL_LCD,
	MD_CHL_WIFI_WLCC,
	MD_CHL_TCP,
	MD_CHL_CLOUD_SYNC,
    MD_CHL_MAX = 0xFF,// 
}channel_modbus;

typedef struct {
	channel_modbus chl;//转发目的通道
    uint8_t dev_index; // 被选中的设备地址,modbus slave address
	uint16_t reg_addr;//modbus beta,modbus WR reg 所在寄存器块的起始地址，用于区分结构体变量对象
	uint16_t offset;//写入reg相对寄存器块起始的相对偏移，指示具体写入寄存器地址起始位置，=寄存器个数*2，即字节偏移
	uint16_t len;//字节数量(一个寄存器为2)，寄存器数量*2
}reg_position_t;

typedef struct position_list {
//    uint8_t dev_index; // 被选中的设备地址,modbus slave address
    reg_position_t position;
    struct position_list *next;
}reg_position_list_t;


/*
上级(WIFI/BLE)和下级(UART)modbus透传转发结构体
*/
typedef struct {
    int unit; // -10
    uint16_t *read;//上级，读目标变量
    uint16_t *write;//上级，写目标变量
    uint16_t min;
    uint16_t max;
    reg_position_t position;//下级modbus的寄存器属性，目标变量在另外的函数指定
}reg_list_t;


typedef union {
    uint16_t all;
    struct {//1-active;0-inactive
        uint16_t point_online : 1; //      
        uint16_t update_need : 1; // 升级需求 
        uint16_t alarm : 1; // 告警发生 
        uint16_t protect : 1; // 保护/故障发生 

        uint16_t system_on : 1; // 设备总（电池对应为DC，逆变器对应AC）开关状态（0-关，1-开）
        uint16_t bat_alarm : 1; //  内置电池包故障告警状态（仅逆变器
        uint16_t bit6 : 1; //  
        uint16_t bit7 : 1; //  

        uint16_t bit8 : 1; //  
        uint16_t bit9 : 1; //  
        uint16_t bit10 : 1; //  
        uint16_t bit11 : 1; //  
        
        uint16_t bit12 : 1; //  
        uint16_t bit13 : 1; //  
        uint16_t bit14 : 1; //  
        uint16_t bit15 : 1; //       		
    }bit;
}POINT_STATE;



typedef struct {

    union 
    {
        uint16_t info[3];
        
        //01
        struct 
        {
            uint8_t slave_addr;//  
            uint8_t group_addr;//  群组序号（不同类型设备，子集汇总信息，从1开始；Modbus 从机地址）;0-表示未绑定
            uint8_t master_addr;// 
            uint8_t group_same_type_addr;// 同类型设备汇总信息的Modbus从机地址
            
            POINT_STATE state;//  
        };	

        //03
        struct 
        {
            uint8_t isOta;  // 0：未升级，1：升级中，  2：升级成功 3：升级失败
            uint8_t errCode;// 错误码  

            uint32_t soft_version;
        };	

    };	

    union 
    {
        uint16_t DevSn[5];
        struct 
        {
            uint64_t SN_64; 	
            uint16_t Dev_Type;
        };	
    };	
}POINT_BIND_INFO;



typedef struct {
    uint8_t slave_addr;//  
    uint8_t group_addr;//  绑定群组序号（用不同的非零数字表示设备属于不同的群组；下级可只区分，而自己编号；0-表示解绑此SN）
    
    union 
    {
        uint16_t DevSn[5];
        struct 
        {
            uint64_t SN_64; 	
            uint16_t Dev_Type;
        };	
    };
}POINT_BIND_INFO_WR;


typedef struct
{
    uint16_t bind_num;
    POINT_BIND_INFO_WR *data;
} bind_msg_t;

#pragma pack()


#define FACTPRY_ADDR        0
#define	MD_VERSION_ADDR		0
#define	FIRST_PAGE_ADDR		100
#define	OTA_CMD_ADDR		700
#define	OTA_STATUS_ADDR		720
#define	INV_BASE_ADDR		1100
#define	INV_PV_ADDR			1200
#define	INV_GRID_ADDR		1300
#define	INV_LOAD_ADDR		1400
#define	INV_INV_ADDR		1500
#define	INV_GEN_ADDR		1600
#define	INV_METER_ADDR		1700
#define	INV_SET_ADDR		2000
#define	INV_APPEND_SET_ADDR	2200
#define	INV_ADV_SET_ADDR	2300
#define	INV_ADV1_SET_ADDR	2400
#define	INV_LOG_ADDR		3000
#define	INV_ALL_ENERGY_ADDR	3500
#define	INV_NOW_ENERGY_ADDR	3600

#define	PACK_BASE_ADDR		6000
#define	PACK_SINGLE_ADDR	6100
#define	PACK_DEBUG_ADDR		6300
#define	PACK_SET_ADDR		7000

#define	IOT_BASE_ADDR		11000
#define	IOT_STORM_ADDR		11500
#define	IOT_SET_ADDR		12000
#define	INV_WIFI_ADDR		13000
#define	HMI_ADDR			14000

#define	PILE_MEG_ADDR		15000//充电桩
#define	PILE_SET_ADDR		15100
#define	PILE_EMS_ADDR		15151

#define	IOT_DEV_BIND_ADDR		21000//20240312 windy :新增本“设备信息绑定帧” 
#define	IOT_2RD_SERVER_ADDR		22000//20240312 windy :公版网页服务器：配置区

#define	AUTH_PARAM_ADDR		40000

#define	MICRO_INV_START_ADDR		2500//微逆寄存器区域，开始
#define	MICRO_INV_END_ADDR			3000//微逆寄存器区域，结束，uart透传区别于CAN透传


/* 定义逆变与电池包对应的modbus地址 */
#define MD_SUMMARY_ADDR						0
#define MD_INV_ADDR_START					1
#define MD_INV_ADDR_END						20
#define MD_INV_ADDR_CNT						(MD_INV_ADDR_END-MD_INV_ADDR_START+1)
#define MD_PACK_SUM_ADDR_START				21
#define MD_PACK_SUM_ADDR_END				40
#define MD_PACK_SUM_ADDR_CNT				(MD_PACK_SUM_ADDR_END-MD_PACK_SUM_ADDR_START+1)
#define MD_PACK_ADDR_START					41
#define MD_PACK_ADDR_END					104
#define MD_PACK_ADDR_CNT				    (MD_PACK_ADDR_END-MD_PACK_ADDR_START+1)
#define MD_DCHUB_SUM_ADDR                   105
#define MD_DCHUB_ADDR_START                 106
#define MD_DCHUB_ADDR_END                   108
#define MD_DCHUB_ADDR_CNT                   (MD_DCHUB_ADDR_END-MD_DCHUB_ADDR_START+1)
#define MD_DCDC_SUM_ADDR                    109
#define MD_DCDC_ADDR_START                  110
#define MD_DCDC_ADDR_END                    112
#define MD_DCDC_ADDR_CNT                    (MD_DCDC_ADDR_END-MD_DCDC_ADDR_START+1)

#define	DEV_MODBUS_ADDR_START 			    113
#define	DEV_MODBUS_ADDR_END 			    200
#define	DEV_MODBUS_ADDR_NUM_MAX 		    (DEV_MODBUS_ADDR_END - DEV_MODBUS_ADDR_START + 1)

#define	DEV_MODBUS_GROUP_ADDR_START 	    201
#define	DEV_MODBUS_GROUP_ADDR_END 		    250
#define	DEV_MODBUS_GROUP_ADDR_NUM_MAX 	    (DEV_MODBUS_GROUP_ADDR_END - DEV_MODBUS_GROUP_ADDR_START + 1)

/* 读汇总数据 */
#define IS_READ_SUMMARY_DATA(addr) 			((addr) == MD_SUMMARY_ADDR)

/* 读逆变数据 */
#define IS_READ_INV_DATA(addr) 				(((addr) >= MD_INV_ADDR_START) && \
											 ((addr) <= MD_INV_ADDR_END))

/* 读电池汇总数据 */
#define IS_READ_PACK_SUM_DATA(addr) 		(((addr) >= MD_PACK_SUM_ADDR_START) && \
											 ((addr) <= MD_PACK_SUM_ADDR_END))

/* 读电池数据 */
#define IS_READ_PACK_DATA(addr) 			(((addr) >= MD_PACK_ADDR_START) && \
											 ((addr) <= MD_PACK_ADDR_END))

/* 读DcHub数据 */
#define IS_READ_DCHUB_DATA(addr) 			(((addr) >= MD_DCHUB_ADDR_START) && \
											 ((addr) <= MD_DCHUB_ADDR_END))

/* 读DCDC数据 */
#define IS_READ_DCDC_DATA(addr) 			(((addr) >= MD_DCDC_ADDR_START) && \
											 ((addr) <= MD_DCDC_ADDR_END))

#define INV_GRP_IDX_TO_ADDR(grp,idx)		(grp * INV_MAX_NUM + idx + MD_INV_ADDR_START)		// 逆变索引转地址
#define INV_ADDR_TO_IDX(addr)				(addr - MD_INV_ADDR_START)							// 逆变地址转索引

#define PACK_GROUP_SIZE						8 													// PACK分组大小
#define PACK_GRP_IDX_TO_ADDR(grp,idx)		(grp * PACK_GROUP_SIZE + idx + MD_PACK_ADDR_START)	// PACK分组与索引转地址
#define PACK_GRP_TO_IDX(addr)				(addr - MD_PACK_SUM_ADDR_START)					// PACK组转索引
#define PACK_ADDR_TO_GRP(addr)				((addr - MD_PACK_ADDR_START) / PACK_GROUP_SIZE)		// PACK地址转分组
#define PACK_ADDR_TO_IDX(addr)				((addr - MD_PACK_ADDR_START) % PACK_GROUP_SIZE)		// PACK地址转索引

#define IS_SUMMARY_ADDR(addr)				IS_READ_SUMMARY_DATA(addr)
#define IS_INV_ADDR(addr)					IS_READ_INV_DATA(addr)
#define IS_PACK_ADDR(addr)					IS_READ_PACK_DATA(addr)
#define IS_DCHUB_ADDR(addr)					IS_READ_DCHUB_DATA(addr)
#define IS_DCDC_ADDR(addr)					IS_READ_DCDC_DATA(addr)

#define LSB2MSB(src)    (((src) >> 8) | ((src) << 8))
#define GET_BIT_VALUE(value, mask, shift)   (((shift) >= 0) ? (((value) & (mask)) << (shift)) : (((value) & (mask)) >> -(shift)))
#define SET_BIT_VALUE(value, mask, shift)   (((shift) >= 0) ? (((value) << (shift)) & (mask)) : (((value) >> -(shift)) & (mask)))


#define ModbusCrc16 calcu_crc16



typedef struct {
    uint16_t reg_addr;//modbus beta,modbus WR reg 所在寄存器块的起始地址，用于区分结构体变量对象
    uint16_t can_type;//  can beta  type数据块区分用，
    uint16_t offset;//写入reg相对寄存器块起始的相对偏移，指示具体写入寄存器地址起始位置，=寄存器个数*2，即字节偏移
    uint16_t len;//字节数量(一个寄存器为2)，寄存器数量*2
}reg2_position_t;


/////////////////////////////////he solution below

/* 配置无效、启用、关闭定义 */
#define CFG_INVALID						0
#define CFG_ENABLE						1
#define CFG_DISABLE						2

/* modbus读写属性定义 */
#define TBL_READ_ONLY					false
#define TBL_READ_WRITE					true

/* 蓝牙配置数据表定义 */
#define	BLE_SET_TBL_START				0
#define	BLE_SET_TBL_END					20
#define	BLE_SET_TBL_NUMS				(BLE_SET_TBL_END - BLE_SET_TBL_START + 1)
#define	BLE_SET_TBL_SIZE				(BLE_SET_TBL_NUMS * sizeof(short))

/* modbus ota升级数据表结构 */
#pragma pack(push, 2)
/**
 * @brief 蓝牙标志
 * 
 */
typedef union {
    uint16_t all;

    struct {
        uint16_t pwd_valid : 2;     // 密码是否有效
        uint16_t guest_falg : 2;    // 访客标志
    };
} ble_flag_t;

/**
 * @brief 蓝牙配置参数结构定义
 * 
 */
typedef struct {
    ble_flag_t ble_flag;            // 蓝牙标志
    uint8_t ble_pwd[6];             // 蓝牙密码
} ble_cfg_t;

/**
 * @brief 蓝牙设置
 * 
 */
typedef struct {
	uint16_t reserved1[6];
	ble_cfg_t ble_cfg;
	uint16_t reserved2[6];
	uint16_t modbus_ver;
	uint16_t reserved3[4];
} ble_set_t;
#pragma pack()

/* 静态检测（编译阶段）结构体是否与期望的大小相等 */
static_assert(sizeof(ble_set_t) == BLE_SET_TBL_SIZE, "sizeof(ble_set_t) detection error");

/* ota升级数据表定义 */
#define	OTA_DATA_TBL_START				700
#define	OTA_DATA_TBL_END				705
#define	OTA_DATA_TBL_NUMS				(OTA_DATA_TBL_END - OTA_DATA_TBL_START + 1)
#define	OTA_DATA_TBL_SIZE				(OTA_DATA_TBL_NUMS * sizeof(short))

/* modbus ota升级数据表结构 */
#pragma pack(push, 2)
typedef struct {
	uint16_t start;
	uint16_t file_type;
	uint32_t file_ver;
	uint16_t file_size;
	uint16_t group;
} ota_data_t;
#pragma pack()

/* 静态检测（编译阶段）结构体是否与期望的大小相等 */
static_assert(sizeof(ota_data_t) == OTA_DATA_TBL_SIZE, "sizeof(ota_data_t) detection error");

/* 设备只读数据表定义 */
#define	DEV_RO_DATA_TBL_START			11000
#define	DEV_RO_DATA_TBL_END				11040
#define	DEV_RO_DATA_TBL_NUMS			(DEV_RO_DATA_TBL_END - DEV_RO_DATA_TBL_START + 1)
#define	DEV_RO_DATA_TBL_SIZE			(DEV_RO_DATA_TBL_NUMS * sizeof(short))

/* modbus设备只读数据表结构 */
#pragma pack(push, 2)
typedef struct {
	uint8_t dev_type[12];
	uint64_t dev_sn;
	uint64_t safe_code;
	uint32_t soft_ver;
	uint16_t reserved[2];
	uint32_t conenct_info;
	uint32_t wifi_sta_ip;
	uint32_t wifi_sta_gateway;
	uint32_t wifi_sta_mask;
	uint16_t wifi_rssi;
	uint8_t wifi_mac[6];
	uint8_t ble_mac[6];
	uint16_t mobile_rssi;
	uint16_t log_dir;
	uint32_t lat_value;
	uint32_t log_value;
	uint16_t can_error;
	uint16_t wifi_disconnect_reason;
} dev_ro_data_t;
#pragma pack()

/* 静态检测（编译阶段）结构体是否与期望的大小相等 */
static_assert(sizeof(dev_ro_data_t) == DEV_RO_DATA_TBL_SIZE, "sizeof(dev_ro_data_t) detection error");

/* 设备读写数据表定义 */
#define	DEV_RW_DATA_TBL_START			12000
#define	DEV_RW_DATA_TBL_END				12163
#define	DEV_RW_DATA_TBL_NUMS			(DEV_RW_DATA_TBL_END - DEV_RW_DATA_TBL_START + 1)
#define	DEV_RW_DATA_TBL_SIZE			(DEV_RW_DATA_TBL_NUMS * sizeof(short))

/* modbus设备读写数据表结构 */
#pragma pack(push, 2)
typedef struct {
	uint16_t factory;
	uint16_t wifi_sta_auth;
	uint8_t wifi_sta_ssid[32];
	uint8_t wifi_sta_pwd[64];
	uint16_t wifi_expand_pwd;
	uint16_t reserved[16];
	uint8_t cloud_addr[128];
	uint8_t mobile_apn[60];
	uint32_t dev_enable;
	uint16_t disaster_warn_set;
} dev_rw_data_t;
#pragma pack()

/* 静态检测（编译阶段）结构体是否与期望的大小相等 */
static_assert(sizeof(dev_rw_data_t) == DEV_RW_DATA_TBL_SIZE, "sizeof(dev_rw_data_t) detection error");

/* mesh数据表定义 */
#define	MESH_DATA_TBL_START				13500
#define	MESH_DATA_TBL_END				13511
#define	MESH_DATA_TBL_NUMS				(MESH_DATA_TBL_END - MESH_DATA_TBL_START + 1)
#define	MESH_DATA_TBL_SIZE				(MESH_DATA_TBL_NUMS * sizeof(short))

/* modbus mesh数据表结构 */
#pragma pack(push, 2)
typedef struct {
	uint16_t mesh_id1;
	uint16_t mesh_id2;
	uint16_t mesh_id3;
	uint16_t reserved[2];
	uint16_t mesh_set;
	uint16_t sys_seq;
	uint16_t dev_type;
	uint16_t command;
	uint8_t wifi_sta_bssid[6];
} mesh_data_t;
#pragma pack()

/* 静态检测（编译阶段）结构体是否与期望的大小相等 */
static_assert(sizeof(mesh_data_t) == MESH_DATA_TBL_SIZE, "sizeof(mesh_data_t) detection error");

/* 插座只读数据表定义 */
#define	PLUG_RO_DATA_TBL_START			14500
#define	PLUG_RO_DATA_TBL_END			14522
#define	PLUG_RO_DATA_TBL_NUMS			(PLUG_RO_DATA_TBL_END - PLUG_RO_DATA_TBL_START + 1)
#define	PLUG_RO_DATA_TBL_SIZE			(PLUG_RO_DATA_TBL_NUMS * sizeof(short))

/* modbus插座只读数据表结构 */
#pragma pack(push, 2)
typedef struct {
	uint8_t dev_type[12];
	uint64_t dev_sn;
	uint32_t soft_ver;
	uint16_t power;
	uint16_t voltage;
	uint16_t current;
	uint16_t freq;
	uint16_t dev_status;
	uint16_t fault;
	uint16_t warn;
	uint32_t energy_day;
	uint32_t work_time_day;
} plug_ro_data_t;
#pragma pack()

/* 静态检测（编译阶段）结构体是否与期望的大小相等 */
static_assert(sizeof(plug_ro_data_t) == PLUG_RO_DATA_TBL_SIZE, "sizeof(plug_ro_data_t) detection error");

/* 插座读写数据表定义 */
#define	PLUG_RW_DATA_TBL_START			14700
#define	PLUG_RW_DATA_TBL_END			14731
#define	PLUG_RW_DATA_TBL_NUMS			(PLUG_RW_DATA_TBL_END - PLUG_RW_DATA_TBL_START + 1)
#define	PLUG_RW_DATA_TBL_SIZE			(PLUG_RW_DATA_TBL_NUMS * sizeof(short))

/* modbus插座读写数据表结构 */
#pragma pack(push, 2)
typedef union {
	uint16_t all;

	struct {
		uint16_t mode : 2;
		uint16_t onoff : 2;
		uint16_t over_load_en : 2;
		uint16_t under_load_en : 2;
		uint16_t delay_en : 2;
	};
} plug_setting1_t;

typedef union {
	uint16_t all;

	struct {
		uint16_t general_recover : 2;
		uint16_t advance_recover : 2;
		uint16_t stat_clear : 2;
	};
} plug_setting2_t;

typedef union {
	uint16_t all;

	struct {
		uint16_t timing_set1 : 2;
		uint16_t timing_set2 : 2;
		uint16_t timing_set3 : 2;
		uint16_t timing_set4 : 2;
		uint16_t timing_set5 : 2;
		uint16_t timing_set6 : 2;
		uint16_t timing_set7 : 2;
		uint16_t timing_set8 : 2;
	};
} plug_timing1_t;

typedef union {
	uint16_t all;

	struct {
		uint16_t timing_set9 : 2;
		uint16_t timing_set10 : 2;
	};
} plug_timing2_t;

typedef union {
	uint16_t all;

	struct {
        #define AC_PHASE_LINE_INVALID   0
        #define AC_PHASE_LINE_L1        1
        #define AC_PHASE_LINE_L2        2
        #define AC_PHASE_LINE_L3        3
        #define AC_PHASE_LINE_AUTO      4
		uint16_t ac_phase_line_en : 3;
	};
} plug_setting3_t;

typedef struct {
    uint16_t Week_Set;//时间设置标签
    uint16_t Set_Action_Time;//动作时间设置值
}SmartPlug_Time_Set;

typedef union {
    uint16_t all;

    struct {
        uint16_t limit : 8;
        uint16_t revd : 8;
    };
} soc_cfg_t;

typedef union {
	uint16_t all;

	struct {
		uint16_t min : 8;
		uint16_t hour : 8;
	};
} plug_delay_t;

typedef struct {
	union {
		uint16_t all;

		struct {
			uint16_t week : 7;
			uint16_t onoff : 2;
		};
	}  week_set;
	
	union {
		uint16_t all;

		struct {
			uint16_t min : 8;
			uint16_t hour: 8;
		};
	} time;
} timing_args_t;

typedef struct {
	uint16_t protect_en;
	plug_setting1_t setting1;
	plug_setting2_t setting2;
	uint16_t setting3;
	plug_timing1_t timing_set1;
	plug_timing2_t timing_set2;
	uint16_t over_load_limit;
	uint16_t under_load_limit;
	uint16_t led_brightness;
	uint32_t timing_time;
	plug_delay_t work_delay;
	timing_args_t timing_args[10];
} plug_rw_data_t;

/* 静态检测（编译阶段）结构体是否与期望的大小相等 */
static_assert(sizeof(plug_rw_data_t) == PLUG_RW_DATA_TBL_SIZE, "sizeof(plug_rw_data_t) detection error");

#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif

const uint16_t* vLookupDataTab(uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t* reg_position, channel_modbus chl);
const uint16_t* vLookupDataTab_from_other_dev( uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t* reg_position, channel_modbus chl);
const uint16_t* vLookupDataTab_from_other_dev_alpha( uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t* reg_position, channel_modbus chl);

void copy_data_from_set_to_modbus(void);

void Modbus_21000_semaphore_init(void);
uint8_t Modbus_21000_semaphore_Take(void);
void Modbus_21000_semaphore_Give(void);
uint8_t Modbus_21000_1_Net_Point_Frame(void);
uint8_t Modbus_21000_3_Net_Point_Frame(void);
uint8_t Modbus_21000_5_Net_Point_Frame(void);

uint16_t SN_TYPE_ASCII_TO_NUM(const char *type);
bool SN_TYPE_NUM_TO_ASCII(uint16_t type_num, char* out_buffer, size_t buffer_size); 
void Dev_All_Point_State_Check(void);
bool get_pack_ver_busy(void);
bool get_pack_ver_mix(void);
uint8_t get_pack_type_count(void);
void delete_set_position(reg_position_list_t *position_list);

int16_t Modbus_21000_bind_msg_create(void);
int16_t Modbus_21000_bind_msg_push(uint8_t bind_num, const POINT_BIND_INFO_WR *bind_info);
int16_t Modbus_21000_bind_msg_process(void);

#ifdef __cplusplus
}
#endif

#endif
