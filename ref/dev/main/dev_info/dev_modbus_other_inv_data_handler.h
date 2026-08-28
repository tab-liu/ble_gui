#ifndef _DEV_MODBUS_OTHER_INV_DATA_HANDLER_H_
#define _DEV_MODBUS_OTHER_INV_DATA_HANDLER_H_

#include "modbus_protocol.h"

void dev_modbus_other_inv_clean(void);
void dev_modbus_other_inv_del(uint64_t dev_sn);
void dev_modbus_other_inv_update(uint16_t dev_type, uint64_t dev_sn, uint32_t online_time);
void dev_modbus_other_inv_offline_set(uint64_t dev_sn);

int dev_modbus_other_inv_slave_addr_get(uint16_t dev_type, uint64_t dev_sn);
md_read_t dev_modbus_other_inv_polling_read_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step);

const uint16_t* vLookupDataTab_from_other_inv( uint16_t dev_type, uint64_t dev_sn, uint16_t regAddr, uint16_t regNum, bool is_write);

int dev_modbus_other_inv_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len);

uint16_t dev_modbus_other_inv_soc_get(uint16_t dev_type, uint64_t dev_sn);
int16_t dev_modbus_other_inv_load_power_get(uint16_t dev_type, uint64_t dev_sn);

void dev_modbus_other_inv_data_summary(void);

#endif  /*end _DEV_MODBUS_OTHER_INV_DATA_HANDLER_H_*/

