#ifndef CAN_PACK_H
#define CAN_PACK_H


#include <stdint.h>
#include <string.h>

#include "can_data.h"
#include "can_control.h"
#include "comm_define.h"
#include "pack_type.h"

#include "uart_device_process.h"

//extern EXT_RAM_BSS_ATTR MOD_STRUCT_Pack Inv_Pack[PACK_MAX_NUM*DEV_MAIN_NODE_MAX];

typedef enum{
	PACK_TYPE_NULL=0,
	PACK_TYPE_B700,
	PACK_TYPE_B300K,
	PACK_TYPE_B300S,
	PACK_TYPE_B300,
	PACK_TYPE_B500K,
	PACK_TYPE_NEW,	//新电池包类型
}Pack_Type;

typedef enum{
	SLAVE_B300K_INDEX=0,
	SLAVE_B300S_INDEX,
	SLAVE_B300_INDEX,
	SLAVE_B500K_INDEX,
	SLAVE_NEW_TYPE_INDEX,
	SLAVE_MAX_INDEX=20,
} Slave_Pack_Index;

typedef enum{
	SLAVE_COMMON_MASK=0x1,
	SLAVE_B300S_MASK=0x2,
	SLAVE_B300_MASK=0x4,
	
	SLAVE_PACK_MAX_COUNT=3,
} Slave_Pack_Mask;

#define  PACK_DEVICE_BMS  3

#define PACK_SAVE_INFO_ADDR  0						//电池包汇总信息存储起始地址
#define SLAVE_PACK_MAX_NUM  PACK_SALVE_MAX_POINT			//电池包可记录最大数量
#define SLAVE_PACK_EACHTYPE_COUNT  PACK_SALVE_MAX_POINT			//每个类型电池包最大数量
#define SLAVE_MIN_VERSION_COUNT  10	//可存取最小版本号数量

#define SLAVE_PACK_READ_ADDR  (PACK_SAVE_INFO_ADDR+30)		//电池包信息读取从机地址 从30开始，实际存储地址从PACK_SAVE_INFO_ADDR（0）开始
#define SLAVE_SINGLE_PACK_ADDR		(PACK_SAVE_INFO_ADDR+21)			//单电池包存储起始地址
#define SLAVE_SINGLE_PACK_READ_ADDR  (SLAVE_SINGLE_PACK_ADDR+30)		//电池包信息读取从机地址 从50开始，实际存储地址从SLAVE_SINGLE_PACK_ADDR（21）开始


#define SLAVE_B300K_GROUP_ADDR 		(PACK_SAVE_INFO_ADDR+SLAVE_B300K_INDEX)				//B300K 组地址 从SLAVE_PACK_ADDR开始
#define SLAVE_B300S_GROUP_ADDR		(PACK_SAVE_INFO_ADDR+SLAVE_B300S_INDEX)		//B300S 组地址 从SLAVE_PACK_ADDR+1开始
#define SLAVE_B300_GROUP_ADDR		(PACK_SAVE_INFO_ADDR+SLAVE_B300_INDEX)		//B300 组地址 从SLAVE_PACK_ADDR+2开始
#define SLAVE_B500K_GROUP_ADDR 		(PACK_SAVE_INFO_ADDR+SLAVE_B500K_INDEX)				//B500K 组地址 从SLAVE_PACK_ADDR+3开始




#define	MAX_DATA_TYPE		4

#define	MAX_PACK_TYPE_NUM		DEFAULT_PACK_TYPE_NUM									// 最大电池包类型数
#define	MAX_DATA_SLAVE		DEV_MAIN_NODE_MAX * (PACK_MAX_NUM-1)// 最大从包数
#define VERSION_SIZE 		6 									// 包括小数点

#define NEW_PACK_TYEP_NUM	6

#pragma pack(1)

typedef struct{
	uint8_t index;	//新增电池包设备在线序号
	char type[TYPE_SIZE];	//电池包产品型号
}NewPack_Type_Info;

typedef struct{
	uint8_t new_pack_num;//当前新电池包设备在线数量
	NewPack_Type_Info typeinfo[NEW_PACK_TYEP_NUM]; //当前会新增的电池包类型数量在那时默认为6个
}NewPack_Type_Ctrls;


typedef union 
{
	uint16_t all;

	struct {
		 uint8_t packSlaveAddr ;	 	//电池包从机地址
		 uint8_t uGroupIndex ; 	 		// 群组序号
	 }Info;
} slaveInfo;


typedef struct {
	char type_ascii[TYPE_SIZE]; 						// pack类型
	uint64_t packSn;							// 单电池包 SN码
	uint32_t packDetail;						// BMU pack 故障和保护等信息
	uint32_t packSoftVersion[PACK_SOFT_NUM];					// 版本号
	uint16_t single_Cell_and_Temp_Count;		// 单个BMU中电芯数量及NTC数量
	uint16_t packType;							// pack类型
	slaveInfo packSlaveInfo; 						//电池包从机地址
	uint8_t groupAddr; 						//电池包组地址
	uint8_t soc;							//单电池包soc
	uint8_t chg_status;							//充电状态
	uint8_t node_idx;						// CAN节点索引 invCan[node_idx]
	uint8_t pack_idx;						// pack槽位 pack_data[pack_idx]
	pack_debug1_struct   packDebug1;    // pack 调试数据,type= 0x56
} Pack_Info;//某一个类型电池包段信息

typedef struct {
    char type_ascii[TYPE_SIZE]; // pack类型
    uint32_t minVersion[SLAVE_MIN_VERSION_COUNT];//最小版本号
	uint8_t versionCount;//一个类型号下面版本号数量
	uint8_t onlineCount;//一个类型号下面实际在线的电池包数量 6154
    uint8_t minGroupAddr; //最小组地址
	Pack_Info minPack_Info;//最小版本电池包信息
	Pack_Info pack_infos[SLAVE_PACK_MAX_NUM];//电池包[0~9]
} Pack_InfosCtrl;


#pragma pack()


void pack_semaphore_init(void);
void pack_infos_init(void);
void pack_bmsinfos_summary(MOD_STRUCT_Inv_READ *invCan);
void pack_infos_summary(MOD_STRUCT_Inv_READ *invCan);
void pack_Extinfos_summary(MOD_STRUCT_Inv_READ *invCan);
void pack_group_summary(Pack_InfosCtrl*uCtrl, MOD_STRUCT_Inv_READ *invCan);


#if 1//def CAN_PACK_SUM_ENABLE
void judge_pack_min_version(void);
void single_pack_information(void);//单包信息7200 
void Pack_6300_offset(uint8_t *str, uint8_t num, uint8_t seq);
void Pack_7200_offset(uint8_t *str, uint8_t num, uint8_t seq);
int findFirstMatchVersion(const uint32_t strings[], size_t count, const uint32_t target);


#endif 

#endif
