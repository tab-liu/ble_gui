#include "wlcc_process.h"
#include "wlcc_interface.h"
#include "wlcc_protocol.h"
#include "wlcc_crypt.h"
#include "wlcc_common.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "modbus_define.h"
#include "modbus_protocol.h"


#define TAG "[MD_METER]"

void dev_modbus_meter_clean(void)
{
	memset(&g_other_rd.Meter, 0, sizeof(g_other_rd.Meter));
}

void dev_modbus_meter_del(uint64_t dev_sn)
{
	int i = 0;
	
	for (i = 0; i < NET_WIFI_METER_POINT; i++)
	{
		if(dev_sn == g_other_rd.Meter[i].meter_info.dev_sn){
			memset(&g_other_rd.Meter[i].meter_info.dev_type, 0, sizeof(MOD_STRUCT_reg01700));
			break;
		}
	}
}

void dev_modbus_meter_offline_set(uint64_t dev_sn)
{
	uint8_t i;

	for (i = 0; i < NET_WIFI_METER_POINT; i++)
	{
		if(dev_sn == g_other_rd.Meter[i].meter_info.dev_sn)
		{
			 g_other_rd.Meter[i].meter_info.status.online = 0;
			 return;
		}
	}
}


void dev_modbus_meter_update(uint64_t dev_sn, uint32_t online_time)
{
	int i = 0;
	int empty_index = NET_WIFI_METER_POINT;
	int offline_index = NET_WIFI_METER_POINT;
	uint32_t last_online_time;
	
	for (i = 0; i < NET_WIFI_METER_POINT; i++)
	{
		if(0 == g_other_rd.Meter[i].meter_info.dev_sn)
		{
			if(NET_WIFI_METER_POINT == empty_index){
				empty_index = i;
			}
		}

		if(0 == g_other_rd.Meter[i].meter_info.status.online)
		{
			if(NET_WIFI_METER_POINT == offline_index)
			{
				offline_index = i;
				last_online_time = g_other_rd.Meter[i].meter_info.last_online_time;
			}
			else
			{
				if(g_other_rd.Meter[i].meter_info.last_online_time < last_online_time)
				{
					offline_index = i;
					last_online_time = g_other_rd.Meter[i].meter_info.last_online_time;
				}
			}
		}
		
		if(dev_sn == g_other_rd.Meter[i].meter_info.dev_sn)
		{
			g_other_rd.Meter[i].meter_info.last_online_time = online_time;
			return;
		}
	}

	if(empty_index < NET_WIFI_METER_POINT)
	{
		g_other_rd.Meter[empty_index].meter_info.dev_sn = dev_sn;
		g_other_rd.Meter[empty_index].meter_info.last_online_time = online_time;
	}
	else if(offline_index < NET_WIFI_METER_POINT)
	{
		g_other_rd.Meter[offline_index].meter_info.dev_sn = dev_sn;
		g_other_rd.Meter[offline_index].meter_info.last_online_time = online_time;
	}
}


int dev_modbus_meter_slave_addr_get(uint16_t dev_type, uint64_t dev_sn)
{
	int i = 0;
	int modbus_slave_addr = IOT_FAIL;
	
	ESP_LOGI(TAG, "income SN:%llu", dev_sn);

	for (i = 0; i < NET_WIFI_METER_POINT; i++)
    {
        ESP_LOGI(TAG, "modbus[%d] SN:%llu", i, g_other_rd.Meter[i].meter_info.dev_sn);
		if((0 != dev_sn) && (dev_sn == g_other_rd.Meter[i].meter_info.dev_sn))
        {
			modbus_slave_addr = i;
			break;
		}
    }

	if(IOT_FAIL != modbus_slave_addr)
	{
		ESP_LOGI(TAG, "slave_addr_index:%d", modbus_slave_addr);
		modbus_slave_addr += 1;
	}
	
	ESP_LOGI(TAG, "modbus_slave_addr:%d", modbus_slave_addr);

	return modbus_slave_addr;
}

uint16_t vLookupMeterDataMaxLength(uint16_t regStartAddr)
{
	uint16_t regMaxLen = 0;

	if ((regStartAddr >= MOD_REG_START_ADDR_01700) && (regStartAddr <= (MOD_REG_START_ADDR_01700 + MOD_REG_LEN_01700))){
		regMaxLen = MOD_REG_START_ADDR_01700 + MOD_REG_LEN_01700 - regStartAddr;
	}
//	else if ((regStartAddr >= MOD_REG_START_ADDR_01900) && (regStartAddr <= (MOD_REG_START_ADDR_01900 + MOD_REG_LEN_01900))){
//		regMaxLen = MOD_REG_START_ADDR_01900 + MOD_REG_LEN_01900 - regStartAddr;
//	}

	return regMaxLen;
}

const uint16_t* vLookupMeterDataTab(uint16_t dev_type, uint64_t dev_sn, uint16_t iReadAddr, uint16_t iReadNum, bool is_write)
{
	uint16_t start = 0;
	uint16_t *reg_ptr = NULL;

	int slave_addr = dev_modbus_meter_slave_addr_get(dev_type, dev_sn);

	if(slave_addr < 0){
		return NULL;
	}

	if ((iReadAddr >= MOD_REG_START_ADDR_01700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01700 + MOD_REG_LEN_01700)))
	{
		start = MOD_REG_START_ADDR_01700;	  /* 可读 */

		if (true == is_write) //write
		{
//			reg_ptr = (const uint16_t*)&g_other_wr.Plug.mod_reg14500_SmartPlug_info;
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg14500_SmartPlug_info read");
			if(slave_addr < NET_WIFI_METER_POINT){
				reg_ptr = (const uint16_t*)&g_other_rd.Meter[slave_addr].meter_info;
			}
		}
	}

	if (reg_ptr) {
		return (reg_ptr + (iReadAddr - start));
	}
	else{
		return NULL;
	}
}


int dev_modbus_meter_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len)
{
	uint8_t bytesCounter = 0;
	uint8_t i = 0;
    uint16_t *regPtr = NULL;

	if(0x03 == income[1])
	{
		regPtr = vLookupMeterDataTab(dev_type, dev_sn, regAdderss, gRegCnt, false);
	    if ((NULL != regPtr) && ((gRegCnt << 1) == income[2]))
	    {
	        ESP_LOGI(TAG, "[modbus other inv rsp] regAdderss2 ok");

	        bytesCounter = gRegCnt << 1;
	        for (i = 0; i < bytesCounter; i += 2){
	            regPtr[i / 2] = ((uint16_t)income[3 + i] << 8) | income[4 + i]; // H/L
	        }
	    }
	    else
	    {
	        ESP_LOGE(TAG, "[modbus other inv rsp] regAdderss2 or gRegCnt error");
	        return 1;
	    }
	}

	return 0;
}


md_read_t dev_modbus_meter_polling_read_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step)
{
	md_read_t modbus_read_ret;
	int modbus_slave_addr = IOT_FAIL;

	modbus_read_ret.reg_num = 0;

	if(0 == step)
	{
		modbus_read_ret.reg_addr = MOD_REG_START_ADDR_01700;
		modbus_slave_addr = dev_modbus_meter_slave_addr_get(dev_type, dev_sn);
		if(IOT_FAIL == modbus_slave_addr){
			modbus_read_ret.reg_num = 0;
		}
		else
		{
			modbus_read_ret.slave_addr = modbus_slave_addr;
			modbus_read_ret.reg_num = 91;
		}
	}else{
		modbus_read_ret.reg_num = 0;
	}
	
	return modbus_read_ret;
}


void dev_modbus_meter_data_summary(void)
{
	int i = 0;

	memset(&g_other_rd.Meter[NET_WIFI_METER_POINT], 0, sizeof(MOD_STRUCT_Meter));
	
	for (i = 0; i < NET_WIFI_METER_POINT; i++)
	{
		if(g_other_rd.Meter[i].meter_info.dev_sn)
		{
			if(wlcc_dev_online_check(SN_TYPE_METER, g_other_rd.Meter[i].meter_info.dev_sn))
			{
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L1_Active_Power += g_other_rd.Meter[i].meter_info.L1_Active_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L2_Active_Power += g_other_rd.Meter[i].meter_info.L2_Active_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L2_Active_Power += g_other_rd.Meter[i].meter_info.L3_Active_Power;

				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L1_Reactive_Power += g_other_rd.Meter[i].meter_info.L1_Reactive_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L2_Reactive_Power += g_other_rd.Meter[i].meter_info.L2_Reactive_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L3_Reactive_Power += g_other_rd.Meter[i].meter_info.L3_Reactive_Power;

				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L1_Apparent_Power += g_other_rd.Meter[i].meter_info.L1_Apparent_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L2_Apparent_Power += g_other_rd.Meter[i].meter_info.L2_Apparent_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L3_Apparent_Power += g_other_rd.Meter[i].meter_info.L3_Apparent_Power;

				// 可能还需要做处理
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L1_Power_Factor = g_other_rd.Meter[i].meter_info.L1_Power_Factor;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L2_Power_Factor = g_other_rd.Meter[i].meter_info.L2_Power_Factor;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.L3_Power_Factor = g_other_rd.Meter[i].meter_info.L3_Power_Factor;

				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.Total_Active_Power = g_other_rd.Meter[i].meter_info.Total_Active_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.Total_Reactive_Power = g_other_rd.Meter[i].meter_info.Total_Reactive_Power;
				g_other_rd.Meter[NET_WIFI_METER_POINT].meter_info.Total_Apparent_Power = g_other_rd.Meter[i].meter_info.Total_Apparent_Power;
			}		
		}
	}
}

