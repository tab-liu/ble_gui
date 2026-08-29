#ifndef __DEV_DATA_RECORD_H__
#define __DEV_DATA_RECORD_H__

#include "inv_define.h"
#include "pack_define.h"
#include "parameter.h"

#include "esp_event.h"

#define SINGLE_TRANSMISSION_MAX_LEN			1024		// 单次传输最大长度

#define DATA_CLASS_FILE_DIR					0			// 文件/目录分类
#define DATA_CLASS_FILE_DATA				1 			// 文件内容分类

#define ROOT_DIR_FLAG						0 			// 根目录标志
#define FILE_DIR_LEN						10 			// 文件目录最大长度
#define RECORD_QUERY_CMD_LEN				15			// 历史记录指令长度

#define HISTORY_LOG_RECORD_MAX_NUM          100
#define HISTORY_PAGE_LOG_MAX_NUM            5
#define EVENT_LOG_RECORD_MAX_NUM            200

/* 子目录名判断 */
#define IS_RECORD_CMD_SUB_DIR(cmd)			((cmd>'1'&&cmd<'9') || \
											(cmd>'A'&&cmd<'Z')  || \
											(cmd>'a'&&cmd<'z'))

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


/////////////////////

/* 定义历史数据存储路劲 */
#define RECORD_ROOT_PATH			FS_BASE_PATH 	// 文件系统根目录

/* 定义IOT历史记录文件标识 */
#define IOT_RECORD_FILE_MARK		"iot"

/* 定义逆变历史记录文件标识 */
#define INV_RECORD_FILE_MARK		"inv"

/* 定义PACK历史记录文件标识 */
#define PACK_RECORD_FILE_MARK		"pack"

#ifdef FILE_SYSTEM_DIRECTORY_ENABLE

#define RECORD_FILE_MARK                "R/info"
#define LOG_RECORD_FOLDER_LEVEL1		"L/"

#else//伪装，实际不使用目录

#define RECORD_FILE_MARK                "Rinfo"
#define LOG_RECORD_FOLDER_LEVEL1		"L"

#endif

/*
littlefs文件系统目录分配定义：
A:PV/load KWH 年记录;
A:hour KWH记录，一个月24*30 ，保存12个月，基于 年月文件名，一个月一个文件
C:tbd

*/

#define PHASE_NUMBERS               3

/* 定义历史记录文件名及路劲
 * 数据最终的目录结构为:
 * 1->inv1_1、pack1_1、pack1_1、...
 * 2->inv2_1、pack2_1、pack2_2、...
 * 3->inv3_1、pack3_1、pack3_2、...
 * 其中invX_n代表逆变在X主节点下挂载的第n个子节点、packX_n代表pack在X主节点下挂载的第n个子节点 */
#define IOT_FILE_PATH(buf)	\
	sprintf(buf, "%s%s", LOG_RECORD_FOLDER_LEVEL1, IOT_RECORD_FILE_MARK)
	
#define INV_FILE_PATH(buf,node,sub_node)	\
	sprintf(buf, "%s%s%d_%02d", LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, node, sub_node)

#define PACK_FILE_PATH(buf,node,sub_node)	\
	sprintf(buf, "%s%s%d_%02d", LOG_RECORD_FOLDER_LEVEL1, PACK_RECORD_FILE_MARK, node, sub_node)


/*文件系统全路径，历史记录，IOT
output:buf
input:node,代号种类，使用2位数字表示
input:protocol_code:事件记录协议代号
" 协议代号":
1-模块更换记录
2-寄存器修改记录
3-能量记录
4-IOT事件记录
5-INV事件记录
6-PACK事件记录

IotSetData
范例：
iot01_c23

*/
#define LOG_FILE_PATH_IOT(buf,node,protocol_code)	\
	sprintf(buf, "%s%s%02d_c%02d", LOG_RECORD_FOLDER_LEVEL1, IOT_RECORD_FILE_MARK, node, protocol_code)


// #define LOG_FILE_PATH_INV(buf,node,protocol_code)	
// 	sprintf(buf, "%s%s%02d_c%02d", LOG_RECORD_FOLDER_LEVEL1, PARAMETER_FILE_MARK_INV, node, protocol_code)
#define LOG_FILE_PATH_INV(buf)	\
	sprintf(buf, "%s%s1_1", LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK)
/*windy add
目录组合字符串

*/
#define COMMON_FILE_PATH_YY(buf,folder,year)	\
		sprintf(buf, "%s%02u", folder, year)

#define COMMON_FILE_PATH_YYMM(buf,folder,year,month)	\
	sprintf(buf, "%s%02u%02u", folder, year, month)
#define COMMON_FILE_PATH_YYMMDD(buf,folder,year,month,day)	\
		sprintf(buf, "%s%02u%02u%02u", folder, year, month,day)

#define INV_KWH_PATH(buf,folder,node,sub_node)	\
	sprintf(buf, "%s%d_%02d", folder, node, sub_node)

//#pragma pack(push, 2)
#pragma pack(2)

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
#pragma pack()

#pragma pack(1)

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


/**
 * @brief 文件或目录查询结构结构定义
 */
typedef struct {
//	name_list_t *dir_list;		// 目录链表指针
//	uint32_t dir_nums;			// 目录数

	name_list_t *file_list;		// 文件链表指针
	uint32_t file_nums;			// 文件数
} historic_file_query_t;

typedef struct {
    LOG_FAULT_STRUCT *pdata;
	uint8_t INV_NUM;
}LOG_FAULT_STRUCT_queue_struct;


#define	IOT_EVENT_FUNC_04_TYPE_LEN	((uint16_t)20)// 

//事件记录协议版本
#define EVENT_MODE_PROTOCOL       1
#define EVENT_REG_PROTOCOL        2
#define EVENT_ENERGY_PROTOCOL     3
#define EVENT_IOT_PROTOCOL        4
#define EVENT_INV_PROTOCOL        5
#define EVENT_PACK_PROTOCOL       6

//windy 新IOT类统一告警码表格

#define	CODE_ALARM_ON	15000	//系统开机(MCU上电)
#define	CODE_ALARM_OFF	15001	//系统关机(MCU下电)
#define	CODE_ALARM_PERIOD_RECORD	15002	//定时记录
#define	CODE_ALARM_SPI_FLASH_ERR	15003	//SPI flash故障
#define	CODE_ALARM_EEPROM_ERR	15004	//EEPROM故障
#define	CODE_ALARM_SUB1GHZ_ERR	15005	//Sub 1GHz(CC1312)通信超时
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

typedef struct {
    EventHistoryData_Struct *pdata;
}LOG_EventHistoryData_Struct;


#pragma pack()


extern QueueHandle_t xQueue_Log_record;
extern QueueHandle_t xQueue_iot_record_event;


int historic_data_get_size(char *fname, uint32_t file_pos, uint32_t *size);
int historic_data_read(char *fname, void *record, uint32_t *len, uint32_t file_pos);
int historic_data_bias_read(char *fname, uint8_t *record, uint32_t *len, uint32_t file_pos);

int historic_data_write(char *fname, void *record, uint32_t file_bias, uint32_t len);

int filename_list_get(char *fname);
void Filename_Compare_Sequence( void);
int file_list_clear(char *target_file_type);
uint32_t historic_record_serialize_query( uint8_t *buf);
int GetHistoryLogFromLittlefs(uint16_t history_page, uint8_t slaveaddress);
uint8_t Logger_Pop(void);
void history_queue_init(void);
int clear_log_file(void);
uint8_t Event_Pop(void);
void historic_dir_check(char *fname);

void dev_log_check(void);
void event_check(void);


int IoT_Event_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size);
int Is_Event_Log_File(const char *fname);
int Get_IoT_Event_Log_File_Id(char *out_buf, size_t buf_len);
int Is_IoT_Event_Log_Count_New(time_t since_timestamp);

int Inv_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size);
int Is_Inv_Log_File(const char *fname);
int Get_Inv_Log_File_Id(char *out_buf, size_t buf_len);
int Is_Inv_Log_Count_New(time_t since_timestamp);

#endif
