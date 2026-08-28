#ifndef __DEV_DATA_RECORD_H__
#define __DEV_DATA_RECORD_H__
//#include "can_data.h"
//#include "drv_gd25q128.h"
#include "inv_type.h"
#include "pack_type.h"
#include "littlefs.h"//

#include "esp_event.h"//
#include "comm_define.h"

#define SINGLE_TRANSMISSION_MAX_LEN			1024		// 单次传输最大长度

#define DATA_CLASS_FILE_DIR					0			// 文件/目录分类
#define DATA_CLASS_FILE_DATA				1 			// 文件内容分类

#define ROOT_DIR_FLAG						0 			// 根目录标志
#define FILE_DIR_LEN						10 			// 文件目录最大长度
#define RECORD_QUERY_CMD_LEN				15			// 历史记录指令长度

#define HISTORY_LOG_RECORD_MAX_NUM          100
#define HISTORY_PAGE_LOG_MAX_NUM            5
#define EVENT_LOG_RECORD_MAX_NUM            100

#define KWH_RECORD_TYPE						10//4											//A20240914
#define KWH_INIT_YEAR						24			//最多累计15年总电量,从2024年开始计算  //A20240914
#define MAX_KWH_STORE_YEAR					15
/* 子目录名判断 */
#define IS_RECORD_CMD_SUB_DIR(cmd)			((cmd>'1'&&cmd<'9') || \
											(cmd>'A'&&cmd<'Z')  || \
											(cmd>'a'&&cmd<'z'))


/*文件系统目录使能(非必要不改动)*/
#define FILE_SYSTEM_DIRECTORY_ENABLE

/**
 * @brief 历史记录指令枚举值定义
 */
typedef enum {
	RECORD_OP_ERROR,						// 操作错误
	RECORD_OP_QUERY_ROOT_DIR,				// 查询根目录
	RECORD_OP_QUERY_SUB_DIR,				// 查询子目录
	RECORD_OP_GET_ROOT_DATA,				// 获取根目录文件数据
	RECORD_OP_GET_SUB_DATA					// 获取子目录文件数据
} record_op_mode_t;

//modbus beta  3600: 1-直流PV；2-电网买电；3-电网卖电；4-AC负载；5-DC负载；6-交流PV
enum {
    PV_ENERGY_TYPE = 1,//dc+ac
    GRID_ENERGY_INPUT_TYPE = 2,
    GRID_ENERGY_OUTPUT_TYPE = 3,
    AC_LOAD_ENERGY_TYPE = 4,
    DC_LOAD_ENERGY_TYPE = 5,
    AC_PV_ENERGY_TYPE = 6,
    DC_PV_ENERGY_TYPE = 7,
    BAT_TOTAL_CHARGE_ENERGY_TYPE=8,
    BAT_TOTAL_DISCHARGE_ENERGY_TYPE=9,
    PV_TO_ACLOAD_ENERGY_TYPE=10,
	ENERGY_TYPE_MAX
};

/////////////////////

/* 定义历史数据存储路劲 */
#define RECORD_ROOT_PATH				FLASH_BASE_PATH 	// 文件系统根目录
/* 定义IOT历史记录文件标识 */
#define IOT_RECORD_FILE_MARK			"iot"
/* 定义逆变历史记录文件标识 */
#define INV_RECORD_FILE_MARK			"inv"
/* 定义PACK历史记录文件标识 */
#define PACK_RECORD_FILE_MARK			"pack"
/*文件系统目录使能(非必要不改动)*/
#define FILE_SYSTEM_DIRECTORY_ENABLE
/* 定义IOT历史记录文件标识 */
#define EVENT_RECORD_FILE_MARK			"event"
#ifdef FILE_SYSTEM_DIRECTORY_ENABLE
#define RECORD_FILE_MARK                "R/info"
#define LOG_RECORD_FOLDER_LEVEL1		"L/"
#define KWH_RECORD_FOLDER_LEVEL1		"K/"
#define INV_LOG_RECORD_FOLDER_LEVEL1		"I/"
//#define KWH_RECORD_FILE_MARK_PV         "K/pv"
#define KWH_RECORD_FILE_MARK_GRID       "K/gr"
#define KWH_RECORD_FILE_MARK_FD         "K/fd"
#define KWH_RECORD_FILE_MARK_AC_LOAD         "K/ac"
#define KWH_RECORD_FILE_MARK_DC_LOAD         "K/dc"
#define KWH_RECORD_FILE_MARK_ACPV       "K/acpv"
#define KWH_RECORD_FILE_MARK_DCPV       "K/dcpv"
//#define KWH_RECORD_FILE_MARK_BATCH      "K/bch"
#define KWH_RECORD_FILE_MARK_BATDICH    "K/bat"//"K/bdh"
#define KWH_RECORD_FILE_MARK_PV_TO_LOAD     "K/pl"//pv to load
#define KWH_RECORD_FILE_MARK_HOUR       "K/hour"
//
#define PARAMETER_FOLDER_LEVEL1			"S/"
#define PARAMETER_FILE_MARK_IOT			"iot"//IOT本身参数存储
#define PARAMETER_FILE_MARK_INV			"inv"//INV下级参数存储
#define SN_FILE_MARK_INV			    "invsn"//INV下级参数存储
#define INV_BOX_FILE_NAME				"box"//INV 黑匣子log信息
#define PARAMETER_FILE_MARK_CA			"ca"//网络安全存储:CA证书
#define PARAMETER_FILE_MARK_CERT        "cert"//网络安全存储:IOT证书
#define PARAMETER_FILE_MARK_KEY         "key"//网络安全存储:IOT私钥
#define PARAMETER_FILE_MARK_RELAY   "touctrl"//TOU控制参数存储

#define DISASTER_SAVE_PARAM_FILE		"disaster_save_param"//风暴预警参数存储

#else//伪装，实际不使用目录
//#define RECORD_FILE_MARK                "Rinfo"
//#define LOG_RECORD_FOLDER_LEVEL1		"L"
//#define KWH_RECORD_FOLDER_LEVEL1		"K"
//#define KWH_RECORD_FILE_MARK_PV         "Kpv"
//#define KWH_RECORD_FILE_MARK_GRID       "Kgr"
//#define KWH_RECORD_FILE_MARK_FD         "Kfd"
//#define KWH_RECORD_FILE_MARK_DC         "Kdc"
//#define KWH_RECORD_FILE_MARK_AC         "Kac"
//#define KWH_RECORD_FILE_MARK_HOUR       "Khour"
#endif

#define KWH_FILE_TYPE_PV         		"dcpv"
#define KWH_FILE_TYPE_GRID       		"gr"
#define KWH_FILE_TYPE_FD         		"fd"
#define KWH_FILE_TYPE_DC         		"dc"
#define KWH_FILE_TYPE_AC         		"ac"
#define INV_FILE_TYPE_LOG        		"in"
#define IOT_FILE_TYPE_LOG        		"io"
#define EVENT_FILE_TYPE_LOG      		"ev"
#define IOT_FILE_TYPE_HOUR      		"ho"
/*
littlefs文件系统目录分配定义：
A:PV/load KWH 年记录;
A:hour KWH记录，一个月24*30 ，保存12个月，基于 年月文件名，一个月一个文件
C:tbd

*/
//#define NOW_PV_ENERGY_PATH            FLASH_BASE_PATH"/A/pv_energy.txt"
//#define NOW_GRID_ENERGY_PATH          FLASH_BASE_PATH"A/grid_energy.txt"
//#define NOW_HOUR_ENERGY_PATH          FLASH_BASE_PATH"A/hour_energy.txt"

#define PHASE_NUMBERS					3

/* 定义历史记录文件名及路劲
 * 数据最终的目录结构为:
 * 1->inv1_1、pack1_1、pack1_1、...
 * 2->inv2_1、pack2_1、pack2_2、...
 * 3->inv3_1、pack3_1、pack3_2、...
 * 其中invX_n代表逆变在X主节点下挂载的第n个子节点、packX_n代表pack在X主节点下挂载的第n个子节点 */
#define IOT_FILE_PATH(buf,node,sub_node)	\
	sprintf(buf, "%s%s%d_%02d", LOG_RECORD_FOLDER_LEVEL1, IOT_RECORD_FILE_MARK,node,sub_node)
	
#define INV_FILE_PATH(buf,node,sub_node)	\
	sprintf(buf, "%s%s%d_%02d", LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK,node,sub_node)

#define PACK_FILE_PATH(buf,node,sub_node)	\
	sprintf(buf, "%s%s%d_%02d", LOG_RECORD_FOLDER_LEVEL1, PACK_RECORD_FILE_MARK,node,sub_node)

#define INV_KWH_PATH(buf,folder,node,sub_node)	\
	sprintf(buf, "%s%d_%02d", folder, node, sub_node)

#define EVENT_HISTORY_PATH(buf)	\
	sprintf(buf, "%s%s", LOG_RECORD_FOLDER_LEVEL1, EVENT_RECORD_FILE_MARK)

#define SAVE_FILE_PATH(buf,name,node,sub_node)	\
	sprintf(buf, "%s%s%d_%d", LOG_RECORD_FOLDER_LEVEL1,name,node, sub_node)

#if 1 //M20240918 0
#define COMMON_FILE_PATH_YYAll(buf,folder, node, sub_node)	\
		sprintf(buf, "%s%d_%02d", folder,node,sub_node)

#define IOT_FILE_PATH_HOURAll(buf,folder, node, sub_node)	\
		sprintf(buf, "%s%d_%02d", folder,node,sub_node)
#endif

/*文件系统全路径，参数，IOT
output:buf
input:node,sub_node
SetData
*/
#define PARAMETER_FILE_PATH_IOT(buf,node,sub_node)	\
	sprintf(buf, "%s%s%d_%d", PARAMETER_FOLDER_LEVEL1, PARAMETER_FILE_MARK_IOT, node, sub_node)

/**
 *  IoT加密相关
 * CA: CA证书, 0: MQTT CA证书; 1: MQTT2 CA证书; 2:Modbus TCP Slave CA证书
 * IOT: IoT证书, 0: MQTT 客户端证书; 1: MQTT2 客户端证书; 2:Modbus TCP Slave 服务端证书
 * KEY: IoT私钥, 0: MQTT 客户端私钥; 1: MQTT2 客户端私钥; 2:Modbus TCP Slave 服务端私钥
*/
#define PARAMETER_FILE_PATH_CA_CERT(buf,node,sub_node)	\
    sprintf(buf, "%s%s%d_%d", PARAMETER_FOLDER_LEVEL1, PARAMETER_FILE_MARK_CA, node, sub_node)
#define PARAMETER_FILE_PATH_IOT_CERT(buf,node,sub_node)	\
    sprintf(buf, "%s%s%d_%d", PARAMETER_FOLDER_LEVEL1, PARAMETER_FILE_MARK_CERT, node, sub_node)
#define PARAMETER_FILE_PATH_IOT_KEY(buf,node,sub_node)	\
    sprintf(buf, "%s%s%d_%d", PARAMETER_FOLDER_LEVEL1, PARAMETER_FILE_MARK_KEY, node, sub_node)

//
/*文件系统全路径，参数，下级INV
output:buf
input:node,sub_node
SetData2

*/
#define PARAMETER_FILE_PATH_INV(buf,node,sub_node)	\
		sprintf(buf, "%s%s%d_%d", PARAMETER_FOLDER_LEVEL1, PARAMETER_FILE_MARK_INV, node, sub_node)


#define PARAMETER_FILE_PATH_RELAY(buf,node,sub_node)	\
	sprintf(buf, "%s%s%d_%02d", PARAMETER_FOLDER_LEVEL1, PARAMETER_FILE_MARK_RELAY, node, sub_node)
/*文件系统全路径，历史记录，IOT
output:buf
input:node,代号种类，使用2位数字表示
input:protocol_code:事件记录协议代号
” 协议代号”:
1-模块更换记录
2-寄存器修改记录
3-能量记录
4-IOT事件记录
5-INV事件记录
6-PACK事件记录

SetData
范例：
iot01_c23

*/
#define LOG_FILE_PATH_IOT(buf,node,protocol_code)	\
	sprintf(buf, "%s%s%02d_c%02d", LOG_RECORD_FOLDER_LEVEL1, PARAMETER_FILE_MARK_IOT, node, protocol_code)

// #define LOG_FILE_PATH_INV(buf,node,protocol_code)	
// 	sprintf(buf, "%s%s%02d_c%02d", LOG_RECORD_FOLDER_LEVEL1, PARAMETER_FILE_MARK_INV, node, protocol_code)
#define LOG_FILE_PATH_INV(buf)	\
	sprintf(buf, "%s%s1_1", LOG_RECORD_FOLDER_LEVEL1, PARAMETER_FILE_MARK_INV)
/*windy add
目录组合字符串
*/
#define LOG_FILE_PATH_INV_INFO(buf ,node,protocol_code) \
 	sprintf(buf,"%s%s%02d_%02d",LOG_RECORD_FOLDER_LEVEL1,INV_BOX_FILE_NAME,node,protocol_code)

#pragma pack(1)//A20240923

/*
#pragma pack(push) 和#pragma pack(pop) 以及#pragma pack()解释：
https://www.cnblogs.com/yangguang-it/p/7392726.html

*/
typedef struct {
    uint16_t power;         // 逆变功率
    uint16_t voltage;       // 逆变电压
    uint16_t current;       // 逆变电流
}inv_detail_struct_t;   

typedef struct {
    uint16_t input_power;   // 输入功率 w
    uint16_t input_voltage; // 输入电压 0.1V
    uint16_t input_current; // 输入电流 0.1A
}pv_detail_struct_t;

typedef struct {
    uint16_t load_power;    // 1W
    uint16_t load_voltage;  // 0.1V
    uint16_t load_current;  // 0.1A
}inv_ac_load_struct_t;

typedef struct {
    uint16_t input_power;   // 输入功率 w
    uint16_t input_voltage; // 输入电压 0.1V
    uint16_t input_current; // 输入电流 0.1A
}grid_detail_struct_t;


/**
 * @brief 逆变历史数据结构定义
 * 定义单个逆变器历史数据结构
 */
typedef struct {
	uint16_t node_id;
	uint16_t inv_id;
	uint16_t gird_freq;
	grid_detail_struct_t  grid_detail[PHASE_NUMBERS];
	inv_ac_load_struct_t inv_ac_load[PHASE_NUMBERS];
	inv_detail_struct_t inv_detail[PHASE_NUMBERS];
	pv_detail_struct_t pv_detail[PHASE_NUMBERS];
} inv_data_record_t;

/**
 * @brief PACK历史数据结构定义
 * 定义单个电池包历史数据结构
 */
typedef struct {
	uint16_t node_id;
	uint16_t pack_id;
	uint16_t voltage;
	int16_t current;
	uint16_t soc;
	uint16_t soh;
	int16_t avg_temp;
	uint16_t min_cell_voltage;
	uint16_t max_cell_voltage;
	int16_t min_cell_temp;
	int16_t max_cell_temp;
	uint16_t min_cell_temp_id;
	uint16_t max_cell_temp_id;
	uint16_t total_cell_cnt;
	uint16_t total_ntc_cnt;
	uint16_t pack_cycle;
} pack_data_record_t;

/**
 * @brief 历史记录指令定义
 * 
 * 定义历史记录指令结构
 */
typedef struct {
	record_op_mode_t op_mode;				// 操作模式
	int offset;								// 数据偏移(指示历史记录数据在文件中的位置)
	uint8_t continuous;						// 连续模式支持
	char dir_name;							// 目录名
	char file_name[FILE_DIR_LEN-1];			// 操作的文件名
} historic_record_cmd_t;

/**
 * @brief 历史记录数据定义
 * 
 * 定义历史记录数据结构
 */
typedef struct {
	uint16_t class;							// 数据分类
	uint32_t total;							// 数据总数
	uint16_t len;							// 当前数据长度
	uint16_t offset;						// 当前数据偏移(指示本次数据在总长度中的位置)
	uint8_t *data;							// 当前数据
} historic_record_data_t;

/**
 * @brief 文件或目录链表结构定义
所有指针类型，其sizeof(x)指针长度为4字节 
 */
typedef struct name_list{
	char *name;					// 文件或目录名
	uint16_t filename_value;//文件名转换为hex数字，用于查找顺序上报
	struct name_list *list;		// 指向下一个链表
} name_list_t;

typedef struct name_list2{
	char name[10];					// 文件或目录名
	uint16_t filename_value;//文件名转换为hex数字，用于查找顺序上报
} name_list_t2;

extern EXT_RAM_BSS_ATTR name_list_t2 *fsys_file_list;


typedef struct {
    uint16_t type; // 能量类型
    uint16_t year; // 哪一年
    uint32_t total_energy; // 当年总能量
    uint32_t mouth_energy[12]; // 每个月的总能量
    uint16_t day_enengy[31]; // 当月每天的总能量
    
    uint16_t crc16;
}now_energy_t;

typedef struct {
//	uint32_t pv_energy; // 当前PV能量信息
	uint32_t grid_chg_energy; // 
    uint32_t grid_fd_energy; //
	//A20240929
	uint32_t ac_load_energy;
	uint32_t dc_load_energy;
	uint32_t ac_pv_energy;
	uint32_t dc_pv_energy;
//	uint32_t bat_charge_energy;
	uint32_t bat_discharge_energy;
	uint32_t pvto_acload_energy;
}baseKWH_energy_t_u32;//KWH变量基础结构体



typedef struct {
	uint32_t DCLoad_Total;//	DC负载总能量 ,1w*s;	0.1kwh
	uint32_t ACLoad_Total;//	AC负载总能量 
	uint32_t Pv_dc;//	PV总的充电能量  PvChgin_Total
	uint32_t Pv_ac;//	PV总的充电能量  
	uint32_t GridChgin_Total;// 电网充电能量 
	uint32_t GridFeedback_Total;//	馈电到电网能量  
	uint32_t PVToload_Total;//	PV到AC负载能量 		 
	uint32_t PackDsg_Total;//	Pack总放电能量 	
	uint32_t revd[10];//

}baseKWH_energy_t_u32_2;//KWH变量基础结构体



typedef struct {
	
	uint16_t DCLoad_Total;//	DC负载总能量 ,1w*s;	0.1kwh
	uint16_t ACLoad_Total;//	AC负载总能量 	
	uint32_t Pv_dc;//	PV总的充电能量  PvChgin_Total
	uint32_t Pv_ac;//	PV总的充电能量  
	uint16_t GridChgin_Total;// 电网充电能量 
	uint16_t GridFeedback_Total;//	馈电到电网能量  
	uint16_t PVToload_Total;//	PV到AC负载能量 		 
	uint16_t PackDsg_Total;//	Pack总放电能量 	
	uint16_t revd[10];//
	
}baseKWH_energy_t_u16;//KWH变量基础结构体


typedef struct {
    uint8_t su8tm_year;
    uint8_t su8tm_mon;
    uint8_t su8tm_mday;
    uint8_t su8tm_hour;

}energy_time_t;//能量信息存储时间戳



typedef struct {
    union{
        uint8_t all[128];

        struct{
            uint16_t type; // 能量类型
            uint16_t year; // 哪一年

            uint32_t total_energy; // 当年总能量
            uint32_t mouth_energy[12]; // 每个月的总能量
            uint16_t day_enengy[31]; // 当月每天的总能量

            uint16_t crc16;
        };
    };
}year_energy_t;

typedef struct {
	//[0]汇总电量 [1]单机电量 
    baseKWH_energy_t_u32_2 KWH_all[2];//长期永久累计
    baseKWH_energy_t_u32_2 DeltaKWH_Year[2];//新增加年的累计能量；在年切换清零，在下电瞬间更新记录
    baseKWH_energy_t_u32_2 DeltaKWH_Month[2];//      
    baseKWH_energy_t_u16 DeltaKWH_Day[2];//
    baseKWH_energy_t_u16 DeltaKWH_Hour[2];// 实时增加的数据

    energy_time_t Energy_time_message;//上述电量信息存储时间戳
    
    uint8_t kwh_years_num;

}POINT_KWH_INFO_Store;

/**
 * @brief 文件或目录查询结构结构定义
 */
typedef struct {
//	name_list_t *dir_list;		// 目录链表指针
//	uint32_t dir_nums;			// 目录数

	name_list_t *file_list;		// 文件链表指针
	uint32_t file_nums;			// 文件数
} historic_file_query_t;

/**
 * @brief RTC时间结构体定义
 */
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
} rtc_time_t;
typedef struct {
	union {
		uint16_t all;

		struct {
			uint16_t FaultSeq : 8;//低字节：故障/告警编号
			uint16_t FaultState : 8;//高字节：故障发生（1）或消失（0）
		};
	};
} LogFaultSeq_STRUCT;

typedef struct
{
    rtc_time_t LogTime;//故障发生时间
    LogFaultSeq_STRUCT LogFaultSeq;//故障/告警编号
    uint16_t LogFaultCode;//故障/告警代码
}LOG_FAULT_STRUCT;//modbus 3000历史记录

typedef struct {
    LOG_FAULT_STRUCT *pdata;
	uint8_t INV_NUM;
}LOG_FAULT_STRUCT_queue_struct;


#define	HISTORY2_CHAR_TYPE_LEN	((uint16_t)20)// 

//事件记录协议版本
#define EVENT_MODE_PROTOCOL       1
#define EVENT_REG_PROTOCOL        2
#define EVENT_ENERGY_PROTOCOL     3
#define EVENT_IOT_PROTOCOL        4
#define EVENT_INV_PROTOCOL        5
#define EVENT_PACK_PROTOCOL       6

//windy 新IOT类统一告警码表格

//事件记录告警码
#define	CODE_ALARM_ON	15000	//系统开机(MCU上电)
#define	CODE_ALARM_OFF	15001	//系统关机(MCU下电)
#define	CODE_ALARM_PERIOD_RECORD	15002	//定时记录
#define	CODE_ALARM_SPI_FLASH_ERR	15003	//SPI flash故障
#define	CODE_ALARM_EEPROM_ERR	15004	//EEPROM故障
#define	CODE_ALARM_SUB1GHZ_ERR	15005	//Sub 1GHz(CC1312)通信超时
#define	CODE_ALARM_4G_ERR	15006	//4G模块通信超时
#define	CODE_ALARM_4GSIM_ERR	15007	//4G SIM卡未插
#define	CODE_ALARM_USB_PORTECT	15008	//USB输出接口供电保护
#define	CODE_ALARM_IOT_IN_ARM_COMM_ERR	15009	//ARM(IOT板内)通信超时
#define	CODE_ALARM_IOT_RTC_ERR	15010	//RTC(IOT板内)通信故障
#define	CODE_ALARM_BL6552_1_ERR	15011	//meter(BL6552)1通信超时
#define	CODE_ALARM_BL6552_2_ERR	15012	//meter(BL6552)2通信超时
#define	CODE_ALARM_BL6552_3_ERR	15013	//meter(BL6552)3通信超时
#define	CODE_ALARM_BL6552_4_ERR	15014	//meter(BL6552)4通信超时
#define	CODE_ALARM_BL6552_5_ERR	15015	//meter(BL6552)5通信超时

//事件记录协议版本
#define EVENT_MODE_PROTOCOL      	1
#define EVENT_REG_PROTOCOL        	2
#define EVENT_ENERGY_PROTOCOL     	3
#define EVENT_IOT_PROTOCOL        	4
#define EVENT_INV_PROTOCOL        	5
#define EVENT_PACK_PROTOCOL       	6


#define INV_FAULTCODE_BASE      0x8001
#define INV_ALARMCODE_BASE      0x8113
//windy新增历史记录
//typedef union tagHistory2Data//union内部 struct等效,  
//{
//	struct
//	{ 
//		uint8_t	CfgCharData[HISTORY2_CHAR_TYPE_LEN];		// 
//	}History2ArrayData;			/*设置数据结构*/
//
//
//
//
//	struct sHistory2NameData// 
//	{
//		uint32_t time_s;//
//		uint8_t ver_protocol;// 
//		uint16_t FaultCode;// 
//		uint16_t WarnCode;// 
//		uint8_t Num_IOT;//Num_IOT IOT数量			1
//		uint8_t Num_INV;//Num_INV 逆变器数量 		1
//		uint8_t Num_PACK;//Num_PACK	PACK数量			1
//		uint8_t Num_up_net;//Num_up_net	WIFI局域网我司设备节点数量			1
//		uint8_t Num_sub1g_net;//Num_sub1g_net	Sub1G网络我司设备节点数量 		1
//		uint16_t time_up_connect;//time_up_connect 北向(WIFI)联网持续时间	分钟		2
//		uint16_t time_sub1g_connect;//time_sub1g_connect	Sub1G联网持续时间 分钟		2
//
//	}History2_info_t;//NameData;	//设置数据结构
//}History2Data_TypeDef;	//设置数据联合 



typedef struct {

	union{ 
		uint8_t	CfgCharData[HISTORY2_CHAR_TYPE_LEN];		// 

	    struct{
    		uint32_t time_s;//
    		uint8_t ver_protocol;// 
    		
            union {
                uint16_t Fault;
            
                struct {
                    uint16_t FaultCode : 15;
                    uint16_t FaultOn : 1;//1-happen;0-clear
                };
            };
     
            union {
                uint16_t Warn;
            
                struct {
                    uint16_t WarnCode : 15;
                    uint16_t WarnOn : 1;//1-happen;0-clear
                };
            };

    		uint8_t Num_IOT;//Num_IOT IOT数量			1
    		uint8_t Num_INV;//Num_INV 逆变器数量 		1
    		uint8_t Num_PACK;//Num_PACK	PACK数量			1
    		uint8_t Num_udp_net;//Num_up_net	WIFI局域网我司设备节点数量			1
    		uint8_t Num_sub1g_net;//Num_sub1g_net	Sub1G网络我司设备节点数量 		1
    		uint16_t time_wifi_connect;//time_up_connect 北向(WIFI)联网持续时间	分钟		2
    		uint16_t time_sub1g_connect;//time_sub1g_connect	Sub1G联网持续时间 分钟		2

	    };
    };
}EventHistoryData_Struct;	//本地事件记录

typedef struct {
    EventHistoryData_Struct *pdata;
}LOG_EventHistoryData_Struct;


#pragma pack()


extern QueueHandle_t xQueue_Log_record;
extern QueueHandle_t xQueue_iot_historydata_record;





int inv_data_record(int main_node, int sub_node, inv_node_struct *inv_node);
int pack_data_record(int main_node, int sub_node, pack_node_struct *pack_node);
int historic_data_get_size(char *fname, uint32_t file_pos, uint32_t *size);
int historic_data_read(char *fname, void *record, uint32_t *len, uint32_t file_pos);
int historic_data_bias_read(char *fname, void *record, uint32_t *len, uint32_t file_pos);

int historic_data_write(char *fname, void *record, uint32_t file_bias, uint32_t len);
int SetData_file_data_write(char *fname, void *record, uint32_t file_bias, uint32_t len);

int filename_list_get(char *fname);
void Filename_Compare_Sequence( void);
int GetInitDataFromLittlefs(void);
//int GetYearXDataFromExtFlash(uint16_t energy_type);
int GetYearXIotDataFromExtFlash(uint16_t energy_type);

int file_list_clear(char *target_file_type);
uint32_t historic_record_serialize_query( uint8_t *buf);
void check_with_conditional_resize(const char* filename);
void Flash_Update_Scan(void);
void Ext_Flash_Store(void);
int GetYearXDataFromLittlefs(uint16_t energy_type);
int GetHistoryLogFromLittlefs(uint16_t history_page);
int GetHistoryLogFromLittlefsReversal(uint16_t history_page);
uint8_t Inv_Fault_Log_Pop(void);// 

uint8_t Logger_Pop_Event(void);// 
void History_Cnt_Read_Flag_Update(void );
void inv_fault_check(void);
void inv_achub_fault_check(void);
void inv_alarm_check(void);
void inv_achub_alarm_check(void);
void Logger_event_check(void);

int clear_kwh_file(void);


uint8_t* debug_print_data_from_storage( uint32_t offset);
void Copy_Data_From_Set_To_Modbus(void);
void SaveOneEvent(uint16_t FaultCode,uint16_t FaultOn,uint16_t WarnCode,uint16_t WarnOn);
int clear_log_file(void);
int SaveFileileWrite(char* fname,uint8_t* buf ,uint16_t index ,uint32_t file_pos,uint16_t writelen);
int FileReadForName(char* fname,void *buf,uint16_t index,uint32_t file_pos,uint16_t read_size);
void historic_dir_check(char *fname);
int get_relay_parameter_from_flash(void);
void Default_tou_ctrl_data_init(void);

#endif
