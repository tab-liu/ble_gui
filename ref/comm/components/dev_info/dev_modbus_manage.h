#ifndef __DEV_MODBUS_MANAGE_H__
#define __DEV_MODBUS_MANAGE_H__

#include "modbus_data.h"

typedef struct {
	uint16_t dev_type;
	uint64_t dev_sn;
	uint16_t parent_type; 	// 设备父节点类型
	uint64_t parent_sn;   	// 设备父节点序列号
	uint8_t addr_in_parent; // 设备在父节点中的modbus地址
	uint8_t channel;  		// 设备在哪个通道上工作
	uint16_t state;			// 设备状态
}sMdAddrInfo_t;

void dev_modbus_addr_manage_init(void);


int dev_modbus_get_dev_type_from_addr(uint8_t addr);
uint16_t dev_modbus_get_dev_type_from_group(uint8_t addr);
sMdAddrInfo_t dev_modbus_get_dev_info_from_addr(uint8_t addr);
sMdAddrInfo_t dev_modbus_get_dev_info_from_type_sn(uint16_t dev_type, uint64_t dev_sn);
int dev_modbus_get_addr_from_type_sn(uint16_t dev_type, uint64_t dev_sn);

int dev_modbus_group_get(uint16_t dev_type);

int dev_modbus_specific_dev_add(uint8_t index, uint16_t dev_type, uint64_t dev_sn, uint8_t channel);
int dev_modbus_dev_add(uint16_t dev_type, uint64_t dev_sn, 
							uint16_t parent_type, uint64_t parent_sn, 
							uint8_t addr_in_parent, uint16_t state,
							uint8_t channel);
int dev_modbus_dev_del(uint16_t dev_type, uint64_t dev_sn);
int dev_modbus_dev_del_by_channel(uint8_t channel);



void device_data_summary(void);
void print_dev_info(void);



extern sMdAddrInfo_t gDevModbusAddrList[];
extern uint16_t gDevModbusGroupAddrList[];

#endif /*END __DEV_MODBUS_MANAGE_H__*/



