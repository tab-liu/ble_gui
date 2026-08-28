


#include "freertos/FreeRTOS.h"
#include "modbus_data.h"
#include "can_data.h"
#include "stddef.h"
#include "esp_log.h"
#include "modbus_master_data.h"
#include "modbus_slave_data.h"
#include "comm_define.h"
#include "can_protocol.h"



#define	TAG "MMD"

//static uint16_t FILL_VALUE = 0;


//const uint16_t* vLookupDataTab2(uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg2_position_t* reg_position)
const uint16_t* Master_vLookupDataTab2(uint8_t SlaveAddr, uint16_t iReadAddr, uint16_t iReadNum, bool is_write)
{
//	uint16_t start = 0;

//	reg2_position_t reg_position;
	ESP_LOGI(TAG,"Master_vLookupDataTab2");
	const uint16_t *reg_ptr = NULL;
	reg_ptr= vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddr,iReadAddr,	iReadNum,  is_write, NULL, NULL, NULL);

	return reg_ptr;
}

const uint16_t *vLookupDataTab(uint8_t SlaveAddr, uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t *reg_position) {
	reg2_position_t reg2_position = {
		.reg_addr = reg_position->reg_addr,
		.offset = reg_position->offset,
		.len = reg_position->len,
	};

    return vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddr, iReadAddr, iReadNum, is_write, NULL, NULL, &reg2_position);
}

