#ifndef __IOT_BOX_TASK_H__
#define __IOT_BOX_TASK_H__
#include <stdint.h>
#include "esp_event.h"//EXT_RAM_BSS_ATTR 需要
#include <stdint.h>
#include <string.h>

#include "comm_define.h"

#include "uart_device_process.h"
#include "can_protocol.h"
#include "dev_data_record.h"
//#include "dev_data_record.h"

//#include "app_uart.h"
//
//#include <time.h>
//#include <sys/time.h>

#ifdef INV_LOG_DETAILED_INFO_RECORD

#define INV_FAULTCODE_BASE      8001
#define INV_ALARMCODE_BASE      8113
#define DEV_MAIN_NODE_MAX 		3//windy CAN总线逆变器储能系统数量

#define INV_DETAILED_INFO_MAX_COUNT 		 (2000)	//最大历史记录数
#define INV_DETAILED_INFO_ONE_BYTE_COUNT 	 (sizeof(Inv_Detailed_Info_Datas)) //一条逆变log记录的存储字节长度，220B

enum{
	IOT_EVENT_POWERON = 15500,		//0	系统开机(MCU上电)
	IOT_EVENT_POWEROFF,	//1	系统关机(MCU下电)
	IOT_EVENT_CYCLE_RECORD ,		//2	定时记录
	reserved1,	//3	reserved1
	reserved2,	//4	reserved2
	reserved3,	//5	reserved3
	reserved4,	//6	reserved4
	reserved5,	//7	reserved5
	reserved6,	//8	reserved6
	reserved7,	//9	reserved7
	IOT_EVENT_WIFIAP_CONNECT,	//10	WIFI AP连接成功
	IOT_EVENT_ETH_CABLE_CONNECT ,		//11	有线以太网网线接入
	IOT_EVENT_BLUETTI_CLOUD_CONNECT,	//12	远程服务器连接成功
	IOT_EVENT_MESH_CONNECT ,		//13	WIFI MESH网络接入成功
	IOT_EVENT_4G_NET_CONNECT,	//14	4G网络连接成功
	GRID_ON,	//15	AC grid来电
	GRID_RELAY_ON,	//16	电网继电器闭合
	OLI_GENERATOR_RELAY_ON,	//17	油机继电器闭合
	AC_PV_RELAY_ON,	//18	AC PV微逆继电器闭合
	NUM_INVERTER_ADD,	//19	逆变器数量增加
	NUM_BAT_ADD,	//20	电池包数量增加
	NUM_MICRO_INVERTER_ADD,	//21	微型逆变器数量增加
	DC_PV_CONNECT,	//22	DC PV接入（Volt >xV）
/////////
	AC_Grid_Charge_Start = 17000,		//	AC 市电充电开始	
	PV_Charge_Start,	//17001	PV发电开始	
	AC_Backup_Load_Output_Start,	//17002	AC backup放电开始	
	AC_Grid_Feedback_Start,	//17003	AC 市电馈电开始	
	Battery_Charge_Start,	//17004	电池充电开始	
	Battery_Discharge_Start,	//17005	电池放电开始	
	Generator_Charge_Start,	//17006	油机发电开始	
	Alternator_Charge_Start,	//17007	行车发电机发电开始 
			
	//电流变大		
	AC_Grid_Power_change = 17020,	//17020	AC 市电电流变化	
	PV_Power_change,	//17021	PV电流变化	
	Battery_Power_chg_change,	//17022	电池充电电流变化	
	Battery_Power_disg_change,	//17023	电池放电电流变化	
	Generator_Power_change,	//17024	油机电流变化	
	AC_backup_Power_change,	//17025	AC backup电流变化	
	Alternator_Power_change,	//17026	行车发电机电流变化 

};

	


/* 定义历史数据存储路劲 */
//#define RECORD_ROOT_PATH				"/littlefs" 	// 文件系统根目录



//#define INV_BOX_FILE_NAME				"box"//INV 黑匣子log信息

// #define LOG_FILE_PATH_INV_INFO(buf ,node,protocol_code) 
//  	sprintf(buf,"%s%s%02d_%02d",LOG_RECORD_FOLDER_LEVEL1,INV_BOX_FILE_NAME,node,protocol_code)

#pragma pack(1)
typedef union {
	/*  0:汇总信息；
		0xFE:自身信息
		0xFF:无故障;
		1~N:具体的故障包序号
		L8bit:
		INV的并机序号；
		H8bit:
		PACK的并机序号； 
	*/
	uint16_t all;

	struct{
		uint8_t inv_parallel_seq;	//inv 的并机序号
		uint8_t pack_parallel_seq;	//pack 的并机序号
	}bit;

}device_index_seq;

typedef struct{
	uint64_t curTime;  //记录发生时间戳
	uint32_t ver;  //协议版本号
	uint16_t alarmCode; 	//告警码
	uint16_t invWorkState; 	//逆变工作状态 0：停机；1：离网运行；2：电网带载；3：并网运行；4：并网充电；5：并网放电；6：逆变故障；7：异常离网
	uint16_t setCtrlWorkMode; 	//逆变工作模式 00：默认（无工作模式）；01：高级(用户自定义模式)；02：经济模式；03：UPS在线；04：UPS后备；05：峰谷；06：离网；
	uint16_t gridFreq; 	//电网频率
	uint16_t grid1Voltage; 	//电网1电压 0.1V
	int16_t grid1Current; 	//电网1电流	0.1A
	uint16_t grid2Voltage; 	//电网2电压	0.1V
	int16_t grid2Current; 	//电网2电流 0.1A
	int16_t inv1Voltage; 	//逆变1电压 0.1V
	int16_t inv1Current; 	//逆变1电流	0.1A	
	int16_t inv2Voltage; 	//逆变2电压 0.1V
	int16_t inv2Current; 	//逆变2电流	0.1A	
	uint16_t acLoad1Voltage; 	//AC负载1电压 0.1V
	uint16_t acLoad1Current; 	//AC负载1电流 0.1A	
	uint16_t acLoad2Voltage; 	//AC负载2电压 0.1V
	uint16_t acLoad2Current; 	//AC负载2电流 0.1A
	uint16_t pv1Voltage; 	//PV1电压 0.1V
	uint16_t pv1Current; 	//PV1电流 0.1A	
	uint16_t pv2Voltage; 	//PV2电压 0.1V
	uint16_t pv2Current; 	//PV2电流 0.1A

	uint16_t ambientTemp; 	//环境温度 1℃
	uint16_t invMaxTemp; 	//逆变器最高温度 1℃
	uint16_t pvDcdcMaxTemp; 	//光伏DCDC优化器最高温度 1℃
	device_index_seq device_seq; //设备索引序号 

	uint16_t packTotalVoltage; 	//pack总电压 0.1V
	int16_t packTotalCurrent; 	//pack总电流 0.1A
	uint16_t soc ;	//pack soc
	uint16_t soh; //pack soh
	uint16_t packRunStatus;	//pack 总运行状态
	uint16_t cellMinVoltage; 	//pack 电芯最小电压 0.001V
	uint16_t cellMaxVoltage; 	//pack 电芯最大电压 0.001V
	uint16_t cellMinTemp; 	//pack 电芯最小温度 1℃
	uint16_t cellMaxTemp; 	//pack 电芯最大温度 1℃	
	uint16_t packCycle; 	//pack 循环次数	
	uint16_t register_transmission_data[15]; //自定义寄存器透传地址30801~30815
}Inv_Detailed_Info;

typedef struct 
{
	Inv_Detailed_Info pre_DetailedInfo;
	Inv_Detailed_Info cur_DetailedInfo;
}Inv_Detailed_Info_Datas;

typedef struct {
    Inv_Detailed_Info_Datas *pdata;
	uint8_t inv_dev_num;	//逆变设备序号
}Log_Inv_Detailed_Info_Queue_Struct;

#pragma pack()

extern EXT_RAM_BSS_ATTR Inv_Detailed_Info_Datas inv_Detail_Info_Data[DEV_MAIN_NODE_MAX+1];	

void Inv_Detailed_Info_update(void);
void Inv_Info_Update_OnErr(uint16_t alarmCode,uint8_t happen,uint8_t node_id,uint64_t now_date);


extern QueueHandle_t xQueue_inv_Detailed_Info_record; 
void SaveErrorDetailInfo(uint8_t HappenType,uint8_t ErrorCode,uint16_t FaultInformation, uint8_t node_id);
uint8_t Inv_Detailed_Info_Log_Pop(void);
uint8_t Inv_Detailed_Log_Push(uint8_t node_id,void* udata);
int clear_DetailInfo_File(void);
int GetInvDetailInfo(void* udata,uint8_t index,uint16_t file_pos);
void box_task_init(void);
void iot_box_task(void);
void SaveEventInfo(uint8_t happenType,uint16_t code,uint8_t node_id);

#else
#define Inv_Detailed_Info_update()
#define Inv_Info_Update_OnErr( alarmCode, happen,node_id,now_date)
#define SaveErrorDetailInfo(Happen,ErrorCode,FaultInformation, node_id)
#define Inv_Detailed_Info_Log_Pop()
#define Inv_Detailed_Log_Push(node ,data)
#define clear_DetailInfo_File()
#define GetInvDetailInfo(data,index,pos)
#define SaveEventInfo(happenType,code,node_id)
#define iot_box_task()

#endif

#define SaveInfoOnPowerOn(node_id) 				SaveEventInfo(0,IOT_EVENT_POWERON,node_id+1)
#define SaveInfoOnPowerOff(node_id) 			SaveEventInfo(0,IOT_EVENT_POWEROFF,node_id+1)
#define SaveInfoOnCycleRecord(node_id) 			SaveEventInfo(0,IOT_EVENT_CYCLE_RECORD,node_id+1)
#endif

