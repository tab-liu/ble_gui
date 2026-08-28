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
#include "dev_modbus_other_inv_data_handler.h"

#define TAG "[MD_OTHER_INV]"

void dev_modbus_other_inv_clean(void)
{
	memset(&g_other_rd.other_inv, 0, sizeof(g_other_rd.other_inv));
}

void dev_modbus_other_inv_del(uint64_t dev_sn)
{
	int i = 0;

	for (i = 0; i < NET_WIFI_INVBAT_POINT_MINI; i++)
	{
		if(dev_sn == g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN){
			memset(&g_other_rd.other_inv[i], 0, sizeof(OTHER_INV_BETA_STRUCT));
			break;
		}
	}
}

void dev_modbus_other_inv_offline_set(uint64_t dev_sn)
{

}


void dev_modbus_other_inv_update(uint16_t dev_type, uint64_t dev_sn, uint32_t online_time)
{
	int i = 0;
	int empty_index = NET_WIFI_INVBAT_POINT_MINI;
	int offline_index = NET_WIFI_INVBAT_POINT_MINI;
	int exist_index = NET_WIFI_INVBAT_POINT_MINI;
	uint32_t last_online_time = 0;
	uint32_t last_online_time_tmp;
	uint16_t dev_type_num;
	char pTypeAscii[TYPE_SIZE] = {0};

	if((0 == dev_type) || (0 == dev_sn)){
		ESP_LOGI(TAG, "dev Invalid type:%d, SN:%llu", dev_type, dev_sn);
		return;
	}

	for (i = 0; i < NET_WIFI_INVBAT_POINT_MINI; i++)
	{
		dev_type_num = SN_TYPE_ASCII_TO_NUM(g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvType);
		ESP_LOGI(TAG, "index[%d] dev_type_num:%d", i, dev_type_num);
		if((0 == dev_type_num) && (dev_sn == g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN))
		{
			if(NET_WIFI_INVBAT_POINT_MINI == exist_index){
				exist_index = i;
			}
		}
		else if(0 == g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN)
		{
			if(NET_WIFI_INVBAT_POINT_MINI == empty_index){
				empty_index = i;
			}
		}
		else if(dev_type_num)
		{
			last_online_time_tmp = wlcc_dev_online_time_get(dev_type_num, g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN);
			if(0 == wlcc_dev_online_check(dev_type_num, g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN))
			{
				if(NET_WIFI_INVBAT_POINT_MINI == offline_index)
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

		if((dev_type_num == dev_type)
			&& (dev_sn == g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN))
		{
			ESP_LOGI(TAG, "inv_dev type:%d, SN:%llu, is exist.", dev_type, dev_sn);
			return;
		}
	}

	ESP_LOGI(TAG, "exist_index:%d, empty_index:%d, offline_index:%d", exist_index, empty_index, offline_index);

	if(exist_index < NET_WIFI_INVBAT_POINT_MINI)
	{
		g_other_rd.other_inv[exist_index].mod_reg01100_Inv_base.InvSN = dev_sn;
		SN_TYPE_NUM_TO_ASCII(dev_type, pTypeAscii, TYPE_SIZE);
		strncpy(g_other_rd.other_inv[exist_index].mod_reg01100_Inv_base.InvType, pTypeAscii, TYPE_SIZE);
	}
	else if(empty_index < NET_WIFI_INVBAT_POINT_MINI)
	{
		g_other_rd.other_inv[empty_index].mod_reg01100_Inv_base.InvSN = dev_sn;
		SN_TYPE_NUM_TO_ASCII(dev_type, pTypeAscii, TYPE_SIZE);
		strncpy(g_other_rd.other_inv[empty_index].mod_reg01100_Inv_base.InvType, pTypeAscii, TYPE_SIZE);
	}
	else  if(offline_index < NET_WIFI_INVBAT_POINT_MINI)
	{
		g_other_rd.other_inv[offline_index].mod_reg01100_Inv_base.InvSN = dev_sn;
		SN_TYPE_NUM_TO_ASCII(dev_type, pTypeAscii, TYPE_SIZE);
		strncpy(g_other_rd.other_inv[offline_index].mod_reg01100_Inv_base.InvType, pTypeAscii, TYPE_SIZE);
	}else{
		ESP_LOGE(TAG, "dev type:%d, SN:%llu add fail!", dev_type, dev_sn);
	}
}


int dev_modbus_other_inv_slave_addr_get(uint16_t dev_type, uint64_t dev_sn)
{
	int i = 0;
	int modbus_slave_addr = IOT_FAIL;

	ESP_LOGI(TAG, "income SN:%llu", dev_sn);

	for (i = 0; i < NET_WIFI_INVBAT_POINT_MINI; i++)
    {
        ESP_LOGI(TAG, "modbus[%d] SN:%llu", i, g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN);
		if((0 != dev_sn) && (dev_sn == g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN))
        {
			modbus_slave_addr = i;
			break;
		}
    }

	ESP_LOGI(TAG, "modbus_slave_addr:%d", modbus_slave_addr);

	return modbus_slave_addr;
}

md_read_t dev_modbus_other_inv_polling_read_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step)
{
	md_read_t modbus_read_ret;
	int modbus_slave_addr = IOT_FAIL;

	modbus_read_ret.reg_num = 0;

	if(0 == step)
	{
		modbus_read_ret.reg_addr = MOD_REG_START_ADDR_00100;
		modbus_slave_addr = dev_modbus_other_inv_slave_addr_get(dev_type, dev_sn);
		if(IOT_FAIL == modbus_slave_addr){
			modbus_read_ret.reg_num = 0;
		}
		else
		{
			modbus_read_ret.slave_addr = modbus_slave_addr;
//			modbus_read_ret.reg_num = MOD_REG_LEN_00100;
			modbus_read_ret.reg_num = 75;
		}
	}else{
		modbus_read_ret.reg_num = 0;
	}

	return modbus_read_ret;
}

/**
*@brief
uint8_t SlaveAddr:modbus从机地址,用于区分不同设备的结构体变量，
*@param[iReadAddr]  寄存器起始地址
*@param[iReadNum]	 寄存器数量
*@param[is_write]	 1-WR;0-RD
*@param[* reg_position]    仅当WR有效，输出向下级传递的辅助信息
*@param[iReaddev] 不同设备信息
*@return
*/
const uint16_t* vLookupDataTab_from_other_inv( uint16_t dev_type, uint64_t dev_sn, uint16_t regAddr, uint16_t regNum, bool is_write)
{
	uint16_t start = 0;
	const uint16_t *reg_ptr = NULL;

	int slave_addr = dev_modbus_other_inv_slave_addr_get(dev_type, dev_sn);

	if((slave_addr < 0) || (slave_addr >= NET_WIFI_INVBAT_POINT_MINI)){
		return NULL;
	}

//		ESP_LOGW(TAG, "vLookupDataTab_from_other_inv:	SlaveAddr=%d,startAddress=%u,writeRegsCnt=%u ,is_write=%d ",SlaveAddr,iReadAddr,iReadNum,is_write);
	if ((regAddr >= MOD_REG_START_ADDR_00000) && ((regAddr + regNum) <= (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000)))
	{
		start = MOD_REG_START_ADDR_00000;	  /* 可读 */

		if(true == is_write){ //write
			reg_ptr = (const uint16_t*)&g_other_wr.other_inv.mod_reg00000;
		}
		else { //read
			reg_ptr = (const uint16_t*)&g_other_rd.other_inv[slave_addr].mod_reg00000;
		}
	}
	else if ((regAddr >= MOD_REG_START_ADDR_00100) && ((regAddr + regNum) <= (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100)))
	{
		start = MOD_REG_START_ADDR_00100;	  /* 可读 */

		if(true == is_write){//write
			reg_ptr = (const uint16_t*)&g_other_wr.other_inv.mod_reg00100_AppPage1;
		}
		else { //read
			ESP_LOGI(TAG,"get in mod_reg00100_AppPage1 read from other inv");
 			reg_ptr = (const uint16_t*)&g_other_rd.other_inv[slave_addr].mod_reg00100_AppPage1;
		}
	}
	else if ((regAddr >= MOD_REG_START_ADDR_01100) && ((regAddr + regNum) <= (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100)))
	{
		start = MOD_REG_START_ADDR_01100;	  /* 可读 */

		if(true == is_write){//write
			reg_ptr = (const uint16_t*)&g_other_wr.other_inv.mod_reg01100_Inv_base;
		}
		else { //read
			reg_ptr = (const uint16_t*)&g_other_rd.other_inv[slave_addr].mod_reg01100_Inv_base;
		}
	}
	else if ((regAddr >= MOD_REG_START_ADDR_01300) && ((regAddr + regNum) <= (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300)))
	{
		start = MOD_REG_START_ADDR_01300;	  /* 可读 */

		if(true == is_write){//write
			reg_ptr = (const uint16_t*)&g_other_wr.other_inv.mod_reg01300_Inv_grid;
		}
		else { //read
			reg_ptr = (const uint16_t*)&g_other_rd.other_inv[slave_addr].mod_reg01300_Inv_grid;
		}
	}
	else if ((regAddr >= MOD_REG_START_ADDR_01400) && ((regAddr + regNum) <= (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400)))
	{
		start = MOD_REG_START_ADDR_01400;	  /* 可读 */

		if(true == is_write){//write
			reg_ptr = (const uint16_t*)&g_other_wr.other_inv.mod_reg01400_Inv_load;
		}
		else { //read
			reg_ptr = (const uint16_t*)&g_other_rd.other_inv[slave_addr].mod_reg01400_Inv_load;
		}
	}
	else if ((regAddr >= MOD_REG_START_ADDR_02000) && ((regAddr + regNum) <= (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000)))
	{
		start = MOD_REG_START_ADDR_02000;	  /* 可读 */

		if(true == is_write){//write
			reg_ptr = (const uint16_t*)&g_other_wr.other_inv.mod_reg02000_Inv_base_set;
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg02000_Inv_base_set read from other inv");
			reg_ptr = (const uint16_t*)&g_other_rd.other_inv[slave_addr].mod_reg02000_Inv_base_set;
		}
	}
//	else if ((regAddr >= MOD_REG_START_ADDR_04800) && ((regAddr + regNum) <= (MOD_REG_START_ADDR_04800 + MOD_REG_LEN_04800)))
//	{
//		start = MOD_REG_START_ADDR_04800;	  /* 可读 */
//
//		if(true == is_write){//write
//			reg_ptr = (const uint16_t*)&g_other_wr.other_inv.mod_reg04800_Inv_ac;
//		}
//		else //read
//		{
//			ESP_LOGI(TAG,"get in mod_reg04800_Inv_ac read from other inv");
//			reg_ptr = (const uint16_t*)&g_other_rd.other_inv[slave_addr].mod_reg04800_Inv_ac;
//		}
//	}

	if (reg_ptr) {
		return (reg_ptr + (regAddr - start));
	}
	else{
		return NULL;
	}

}

int dev_modbus_other_inv_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len)
{
	uint8_t bytesCounter = 0;
	uint8_t i = 0;
    uint16_t *regPtr = NULL;

	if(0x03 == income[1])
	{
		regPtr = vLookupDataTab_from_other_inv(dev_type, dev_sn, regAdderss, gRegCnt, false);
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

uint16_t dev_modbus_other_inv_soc_get(uint16_t dev_type, uint64_t dev_sn)
{
	int slave_addr = dev_modbus_other_inv_slave_addr_get(dev_type, dev_sn);

	if((IOT_FAIL == slave_addr) || (slave_addr >= NET_WIFI_INVBAT_POINT_MINI)){
		return 0;
	}

	ESP_LOGI(TAG, "type[%d], sn[%llu] SOC=%d", dev_type, dev_sn, g_other_rd.other_inv[slave_addr].mod_reg00100_AppPage1.soc);

	return g_other_rd.other_inv[slave_addr].mod_reg00100_AppPage1.soc;
}

int16_t dev_modbus_other_inv_load_power_get(uint16_t dev_type, uint64_t dev_sn)
{
	int16_t load_power = 0;
	int slave_addr = dev_modbus_other_inv_slave_addr_get(dev_type, dev_sn);

	if((IOT_FAIL == slave_addr) || (slave_addr >= NET_WIFI_INVBAT_POINT_MINI)){
		return 0;
	}

	switch(dev_type)
	{
		case SN_TYPE_AC2AC:
			load_power = g_other_rd.other_inv[slave_addr].mod_reg01300_Inv_grid.grid_detail[0].input_power;
			ESP_LOGI(TAG, "type[%d], sn[%llu] load_power=%d", dev_type, dev_sn, load_power);
			break;
		default:
			break;
	}

	return load_power;
}



void dev_modbus_other_inv_data_summary(void)
{
	int i = 0;
	uint16_t dev_type;

	for (i = 0; i < NET_WIFI_INVBAT_POINT_MINI; i++)
	{
		if(g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN)
		{
			dev_type = SN_TYPE_ASCII_TO_NUM(g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvType);
			if(dev_type && wlcc_dev_online_check(dev_type, g_other_rd.other_inv[i].mod_reg01100_Inv_base.InvSN))
			{
				g_other_rd.other_inv[NET_WIFI_INVBAT_POINT_MINI].mod_reg00100_AppPage1.soc = g_other_rd.other_inv[i].mod_reg00100_AppPage1.soc;
				g_other_rd.other_inv[NET_WIFI_INVBAT_POINT_MINI].mod_reg00100_AppPage1.ACLoadTotalEnergy = g_other_rd.other_inv[i].mod_reg00100_AppPage1.ACLoadTotalEnergy;
			}
		}
	}
}

