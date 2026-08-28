#include "wlcc_process.h"
#include "wlcc_interface.h"
#include "wlcc_protocol.h"
#include "wlcc_crypt.h"
#include "wlcc_common.h"


#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// #include "modbus_define.h"
#include "modbus_master_data.h"
#include "modbus_data.h"
#include "modbus_protocol.h"
#include "uart_device_process.h"
#include "comm_define.h"
#include "can_protocol.h"
#include "dev_discovery.h"

#define TAG "[MD_S1]"

void dev_modbus_s1_clean(void)
{
	memset(&g_other_rd.Plug, 0, sizeof(g_other_rd.Plug));
}

void dev_modbus_s1_del(uint64_t dev_sn)
{
	int i = 0;

	for (i = 0; i < PLUG_MAX_NUM; i++)
	{
		if(dev_sn == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN){
			memset(&g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Type, 0, sizeof(MOD_STRUCT_reg14500));
			break;
		}
	}
}

void dev_modbus_s1_offline_del(void)
{
	uint8_t i;

	for (i = 0; i < NET_WIFI_S1_POINT; i++)
	{
		if(0 != g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN)
		{
			 if(0 == wlcc_dev_online_check(SN_TYPE_S1, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN)){
				dev_modbus_s1_del(g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
			 }
		}
	}
}


void dev_modbus_s1_update(uint64_t dev_sn, uint32_t online_time)
{
	int i = 0;
	int empty_index = PLUG_MAX_NUM;
	int offline_index = PLUG_MAX_NUM;
	uint32_t last_online_time;
	uint32_t last_online_time_tmp;

	for (i = 0; i < PLUG_MAX_NUM; i++)
	{
		if(0 == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN)
		{
			if(PLUG_MAX_NUM == empty_index){
				empty_index = i;
			}
		}
		else
		{
			last_online_time_tmp = wlcc_dev_online_time_get(SN_TYPE_S1, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
			if(0 == wlcc_dev_online_check(SN_TYPE_S1, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN))
			{
				if(PLUG_MAX_NUM == offline_index)
				{
					offline_index = i;
					last_online_time = last_online_time_tmp;
				}
				else
				{
					if(last_online_time_tmp < last_online_time)
					{
						offline_index = i;
						last_online_time = last_online_time_tmp;
					}
				}
			}
		}

		if(dev_sn == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN){
			return;
		}
	}

	if(empty_index < PLUG_MAX_NUM){
		g_other_rd.Plug[empty_index].mod_reg14500_SmartPlug_info.SmartPlug_SN = dev_sn;
	}else  if(offline_index < PLUG_MAX_NUM){
		g_other_rd.Plug[offline_index].mod_reg14500_SmartPlug_info.SmartPlug_SN = dev_sn;
	}
}


int dev_modbus_s1_slave_addr_get(uint16_t dev_type, uint64_t dev_sn)
{
	int i = 0;
	int modbus_slave_addr = IOT_FAIL;

	ESP_LOGI(TAG, "income SN:%llu", dev_sn);

	for (i = 0; i < PLUG_MAX_NUM; i++)
    {
        ESP_LOGI(TAG, "modbus[%d] SN:%llu", i, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
		if((0 != dev_sn) && (dev_sn == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN))
        {
			modbus_slave_addr = i;
			break;
		}
    }

	ESP_LOGI(TAG, "modbus_slave_addr:%d", modbus_slave_addr);

	return modbus_slave_addr;
}

md_read_t dev_modbus_s1_polling_read_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step)
{
	md_read_t modbus_read_ret;
	int modbus_slave_addr = IOT_FAIL;

	modbus_read_ret.reg_num = 0;

	if(0 == step)
	{
		modbus_read_ret.reg_addr = MOD_REG_START_ADDR_14500;
		modbus_slave_addr = dev_modbus_s1_slave_addr_get(dev_type, dev_sn);
		if(IOT_FAIL == modbus_slave_addr){
			modbus_read_ret.reg_num = 0;
		}else{
			modbus_read_ret.reg_num = MOD_REG_LEN_14500;
		}
	}else{
		modbus_read_ret.reg_num = 0;
	}

	return modbus_read_ret;
}

const uint16_t* vLookupS1DataTab(uint16_t dev_type, uint64_t dev_sn, uint16_t iReadAddr, uint16_t iReadNum, bool is_write)
{
	uint16_t start = 0;
	uint16_t *reg_ptr = NULL;

	int slave_addr = dev_modbus_s1_slave_addr_get(dev_type, dev_sn);

	if(slave_addr < 0){
		return NULL;
	}

	if ((iReadAddr >= MOD_REG_START_ADDR_14500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500)))
	{
		start = MOD_REG_START_ADDR_14500;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&g_other_wr.Plug.mod_reg14500_SmartPlug_info;
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg14500_SmartPlug_info read");
			if(slave_addr < PLUG_MAX_NUM){
				reg_ptr = (const uint16_t*)&g_other_rd.Plug[slave_addr].mod_reg14500_SmartPlug_info;
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

int dev_modbus_s1_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len)
{
	uint8_t bytesCounter = 0;
	uint8_t i = 0;
    uint16_t *regPtr = NULL;

	if(0x03 == income[1])
	{
		regPtr = vLookupS1DataTab(0, dev_sn, regAdderss, gRegCnt, false);
	    if ((NULL != regPtr) && ((gRegCnt << 1) == income[2]))
	    {
	        ESP_LOGI(TAG, "[s1 modbus rsp] regAdderss2 ok");

	        bytesCounter = gRegCnt << 1;
	        for (i = 0; i < bytesCounter; i += 2){
	            regPtr[i / 2] = ((uint16_t)income[3 + i] << 8) | income[4 + i]; // H/L
	        }
	    }
	    else
	    {
	        ESP_LOGE(TAG, "[s1 modbus rsp] regAdderss2 or gRegCnt error");
	        return 1;
	    }
	}

	return 0;
}

void dev_modbus_s1_data_summary(void)
{
	static uint16_t time_100ms_cnt = 0;
	int i = 0;

	time_100ms_cnt++;

	memset(&g_other_rd.Plug[PLUG_MAX_NUM], 0, sizeof(MOD_STRUCT_PLUG));

	for (i = 0; i < PLUG_MAX_NUM; i++)
	{
		if(g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN)
		{
			if(wlcc_dev_online_check(SN_TYPE_S1, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN))
			{
				g_other_rd.Plug[PLUG_MAX_NUM].mod_reg14500_SmartPlug_info.SmartPlug_Power += g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power;

				if(0 == (time_100ms_cnt%30))
				{
					ESP_LOGI(TAG, "Voltage = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Voltage);
					ESP_LOGI(TAG, "Current = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Current);
					ESP_LOGI(TAG, "Freq = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Frequency);
					ESP_LOGI(TAG, "States = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_States.all);
					ESP_LOGI(TAG, "Power = %1f", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power/10.0);
				}
			}
		}
	}
}

