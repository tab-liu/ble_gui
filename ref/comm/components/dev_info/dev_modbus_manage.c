#include <stdint.h>
#include <string.h>

#include <esp_log.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "modbus_define.h"
#include "modbus_master.h"
#include "modbus_ble_client.h"

#include "inv_define.h"
#include "can_protocol.h"
#include "can_data.h"
#include "utils.h"

#include "wlcc_process.h"
#include "dev_modbus_manage.h"

#include "dev_modbus_s1_data_handler.h"
#include "dev_modbus_meter_data_handler.h"
#include "dev_modbus_other_inv_data_handler.h"
#include "ble_client_central.h"


#define TAG "[MD_DEV]"


USE_EXT_RAM_BSS sMdAddrInfo_t gDevModbusAddrList[DEV_MODBUS_ADDR_NUM_MAX];
USE_EXT_RAM_BSS uint16_t gDevModbusGroupAddrList[DEV_MODBUS_GROUP_ADDR_NUM_MAX];

void dev_modbus_addr_manage_init(void)
{
	uint16_t self_type = 0;
	
	memset(&gDevModbusAddrList, 0, sizeof(gDevModbusAddrList));
	memset(&gDevModbusGroupAddrList, 0, sizeof(gDevModbusGroupAddrList));

	self_type = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
	dev_modbus_specific_dev_add(0, self_type, dev_factory.dev_sn, 0);
}

int dev_modbus_get_dev_type_from_addr(uint8_t addr)
{
	uint8_t i = 0;
	if((addr >= DEV_MODBUS_ADDR_START) || (addr <= DEV_MODBUS_ADDR_END)){
		return gDevModbusAddrList[addr].dev_type;
	}

	return 0;
}

uint16_t dev_modbus_get_dev_type_from_group(uint8_t addr)
{
	uint8_t i = 0;
	if((addr < DEV_MODBUS_GROUP_ADDR_START) || (addr > DEV_MODBUS_GROUP_ADDR_END)){
		return 0;
	}

	return gDevModbusGroupAddrList[addr - DEV_MODBUS_GROUP_ADDR_START]; // 组地址存的是设备型号
}

sMdAddrInfo_t dev_modbus_get_dev_info_from_addr(uint8_t addr)
{
	sMdAddrInfo_t devInfo;
	memset(&devInfo, 0, sizeof(sMdAddrInfo_t));
	
	if((addr < DEV_MODBUS_ADDR_START) || (addr > DEV_MODBUS_ADDR_END)){
		return devInfo;
	}

	return gDevModbusAddrList[addr - DEV_MODBUS_ADDR_START];
}

sMdAddrInfo_t dev_modbus_get_dev_info_from_type_sn(uint16_t dev_type, uint64_t dev_sn)
{
	uint8_t i = 0;
	sMdAddrInfo_t devInfo;
	memset(&devInfo, 0, sizeof(sMdAddrInfo_t));

	for(i=0; i<DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if((dev_type == gDevModbusAddrList[i].dev_type)
			&& (dev_sn == gDevModbusAddrList[i].dev_sn)){
			return gDevModbusAddrList[i];
		}
	}

	return devInfo;
}

int dev_modbus_get_addr_from_type_sn(uint16_t dev_type, uint64_t dev_sn)
{
	
	uint8_t i = 0;

	for(i=0; i<DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if((dev_type == gDevModbusAddrList[i].dev_type)
			&& (dev_sn == gDevModbusAddrList[i].dev_sn))
		{
			if(0 == i){
				return i;
			}
			return DEV_MODBUS_ADDR_START + i;
		}
	}

	return -1;
}

int dev_modbus_get_master_addr_from_type_sn(uint16_t dev_type, uint64_t dev_sn)
{
	
	uint8_t i = 0;

	for(i=0; i<DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if((dev_type == gDevModbusAddrList[i].dev_type)
			&& (dev_sn == gDevModbusAddrList[i].dev_sn)){
			return dev_modbus_get_addr_from_type_sn(gDevModbusAddrList[i].parent_type, gDevModbusAddrList[i].parent_sn);
		}
	}

	return -1;
}


int dev_modbus_group_get(uint16_t dev_type)
{
	uint8_t i = 0;

	for(i=0; i<DEV_MODBUS_GROUP_ADDR_NUM_MAX; i++)
	{
		if(dev_type == gDevModbusGroupAddrList[i]){
			return (DEV_MODBUS_GROUP_ADDR_START + i);
		}
	}

	return -1;
}

int dev_modbus_group_add(uint16_t dev_type)
{
	uint8_t i = 0;
	uint8_t empty_index = DEV_MODBUS_GROUP_ADDR_NUM_MAX;

	for(i=0; i<DEV_MODBUS_GROUP_ADDR_NUM_MAX; i++)
	{
		if(0 == gDevModbusGroupAddrList[i])
		{
			if(DEV_MODBUS_GROUP_ADDR_NUM_MAX == empty_index){
				empty_index = i;
			}
		}
		
		if(dev_type == gDevModbusGroupAddrList[i]){
			return 0;
		}
	}

	if(empty_index < DEV_MODBUS_GROUP_ADDR_NUM_MAX){
		gDevModbusGroupAddrList[empty_index] = dev_type;
	}

	return 0;
}

int dev_modbus_group_del(uint16_t dev_type)
{
	uint8_t i = 0;

	for(i=0; i<DEV_MODBUS_GROUP_ADDR_NUM_MAX; i++)
	{
		if(dev_type == gDevModbusGroupAddrList[i]){
			gDevModbusGroupAddrList[i] = 0;
		}
	}

	return 0;
}

int dev_modbus_dev_update(uint16_t dev_type, uint64_t dev_sn, 
									uint16_t parent_type, uint64_t parent_sn, 
									uint8_t addr_in_parent, uint16_t state,
									uint8_t channel)
{
	if((0 == dev_type) || (0 == dev_sn)){
		return -1;
	}
	
	for(int i=0; i<DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if((dev_type == gDevModbusAddrList[i].dev_type)
			&& (dev_sn == gDevModbusAddrList[i].dev_sn))
		{
			if((parent_type != gDevModbusAddrList[i].parent_type)
				|| (parent_sn != gDevModbusAddrList[i].parent_sn)
				|| (state != gDevModbusAddrList[i].state)){
				reals.net_point_Comein = 1;
			}
			gDevModbusAddrList[i].parent_type = parent_type;
			gDevModbusAddrList[i].parent_sn = parent_sn;
			gDevModbusAddrList[i].addr_in_parent = addr_in_parent;
			gDevModbusAddrList[i].state = state;
			gDevModbusAddrList[i].channel = channel;
			return 0;
		}
	}

	return 0;
}

/*------------------------------------------------------------------------
*@Function :dev_modbus_specific_dev_add
*@brief  
*@param[in]     dev_type:设备型号
				dev_sn:设备序列号
				channel：设备通信通道
*@param[out]    None
*@return         
*/
int dev_modbus_specific_dev_add(uint8_t index, uint16_t dev_type, uint64_t dev_sn, uint8_t channel)
{
	ESP_LOGI(TAG, "start addr type[%d] sn[%"PRIu64"]", dev_type, dev_sn);
	if((0 == dev_type) || (0 == dev_sn)){
		ESP_LOGE(TAG, "type or sn is 0");
		return -1;
	}

	if(index >= DEV_MODBUS_ADDR_NUM_MAX){
		ESP_LOGE(TAG, "index err!");
		return -1;
	}

	gDevModbusAddrList[index].dev_type = dev_type;
	gDevModbusAddrList[index].dev_sn = dev_sn;
	gDevModbusAddrList[index].parent_type = 0;
	gDevModbusAddrList[index].parent_sn = 0;
	gDevModbusAddrList[index].addr_in_parent = 0;
	gDevModbusAddrList[index].channel = channel;

	ESP_LOGI(TAG, "[%d]add ok", index);

	dev_modbus_group_add(dev_type);

	return 0;
}


/*------------------------------------------------------------------------
*@Function :dev_modbus_dev_add
*@brief  
*@param[in]     dev_type:设备型号
				dev_sn:设备序列号
				parent_type：设备父节点型号，0表示自己是根节点
				parent_sn：设备父节点序列号
				addr_in_parent：设备在父节点中的modbus地址
				channel：设备通信通道
*@param[out]    None
*@return         
*/
int dev_modbus_dev_add(uint16_t dev_type, uint64_t dev_sn, 
							uint16_t parent_type, uint64_t parent_sn, 
							uint8_t addr_in_parent, uint16_t state,
							uint8_t channel)
{
	uint8_t i = 0;
	uint8_t empty_index = DEV_MODBUS_ADDR_NUM_MAX;

	ESP_LOGI(TAG, "start addr type[%d] sn[%"PRIu64"]", dev_type, dev_sn);
	if((0 == dev_type) || (0 == dev_sn)){
		ESP_LOGE(TAG, "type or sn is 0");
		return -1;
	}

	// 0是本机，1是绑定设备	
	for(i=2; i<DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if((0 == gDevModbusAddrList[i].dev_type)
			&& (0 == gDevModbusAddrList[i].dev_sn))
		{
			if(DEV_MODBUS_ADDR_NUM_MAX == empty_index){
				empty_index = i;
			}
		}
		
		if((dev_type == gDevModbusAddrList[i].dev_type)
			&& (dev_sn == gDevModbusAddrList[i].dev_sn))
		{
			gDevModbusAddrList[i].parent_type = parent_type;
			gDevModbusAddrList[i].parent_sn = parent_sn;
			gDevModbusAddrList[i].addr_in_parent = addr_in_parent;
			gDevModbusAddrList[i].state = state;
			gDevModbusAddrList[i].channel = channel;
			ESP_LOGI(TAG, "is exist");
			return 0;
		}
	}

	if(empty_index < DEV_MODBUS_ADDR_NUM_MAX)
	{
		gDevModbusAddrList[empty_index].dev_type = dev_type;
		gDevModbusAddrList[empty_index].dev_sn = dev_sn;
		gDevModbusAddrList[empty_index].parent_type = parent_type;
		gDevModbusAddrList[empty_index].parent_sn = parent_sn;
		gDevModbusAddrList[empty_index].addr_in_parent = addr_in_parent;
		gDevModbusAddrList[empty_index].state = state;
		gDevModbusAddrList[empty_index].channel = channel;

		ESP_LOGI(TAG, "[%d]add ok", empty_index);
		dev_modbus_group_add(dev_type);
	}
    
	return 0;
}

int dev_modbus_dev_del(uint16_t dev_type, uint64_t dev_sn)
{
	uint8_t i = 0;
	uint8_t dev_num = 0;

	// 0是本机
	for(i = 1; i < DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if(dev_type == gDevModbusAddrList[i].dev_type)
		{
			dev_num++;
			if(dev_sn == gDevModbusAddrList[i].dev_sn)
			{
				gDevModbusAddrList[i].dev_type = 0;
				gDevModbusAddrList[i].dev_sn = 0;
				gDevModbusAddrList[i].parent_type = 0;
				gDevModbusAddrList[i].parent_sn = 0;
				gDevModbusAddrList[i].addr_in_parent = 0;
				gDevModbusAddrList[i].channel = 0;
				if(dev_num){
					dev_num--;
				}
			}
		}
	}

	if(0 == dev_num){
		dev_modbus_group_del(dev_type);
	}

	return 0;
}

int dev_modbus_dev_del_by_channel(uint8_t channel)
{
	uint8_t i = 0;

	// 0是本机
	for(i = 1; i < DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if(channel == gDevModbusAddrList[i].channel)
		{
			gDevModbusAddrList[i].dev_type = 0;
			gDevModbusAddrList[i].dev_sn = 0;
			gDevModbusAddrList[i].parent_type = 0;
			gDevModbusAddrList[i].parent_sn = 0;
			gDevModbusAddrList[i].addr_in_parent = 0;
			gDevModbusAddrList[i].channel = 0;
		}
	}

    // 执行对应清除逻辑
    switch ( channel )
    {
        case MD_CHL_WIFI_WLCC :
            dev_modbus_s1_clean();
            break;
        
        default:
            break;
    }
    
	return 0;
}

void device_data_summary(void)
{
//	ble_client_data_summary();
	dev_modbus_s1_data_summary();
	dev_modbus_meter_data_summary();
}

void print_dev_info(void)
{
	int i = 0;
	int group;
	uint8_t master;

	ESP_LOGI(TAG, "\n------------------------------------------------print_dev_info--------------------------------------------------");
	ESP_LOGI(TAG, "| %-4s | %5s | %6s | %16s | %3s | %1s | %5s | %16s | %8s |",
             "addr", "group", "type", "sn", "chl", "M","ftype", "fsn", "sub_addr");
	for(i=0; i<DEV_MODBUS_ADDR_NUM_MAX; i++)
	{
		if(gDevModbusAddrList[i].dev_type && gDevModbusAddrList[i].dev_sn)
		{
			group = dev_modbus_group_get(gDevModbusAddrList[i].dev_type);
			master = wlcc_dev_master_type(gDevModbusAddrList[i].dev_type, gDevModbusAddrList[i].dev_sn);
			ESP_LOGI(TAG, "| %4" PRIu16  " | %5" PRIu16 " | %6" PRIu16 
						 " | %16" PRIu64 " | %3" PRIu16 " | %1" PRIu8
						 " | %5" PRIu16  " | %16" PRIu64 " | %8" PRIu16 " |", 
							i, group, gDevModbusAddrList[i].dev_type,
							gDevModbusAddrList[i].dev_sn, gDevModbusAddrList[i].channel, master,
							gDevModbusAddrList[i].parent_type, gDevModbusAddrList[i].parent_sn, gDevModbusAddrList[i].addr_in_parent);

		}
	}
}




