#ifndef _DEV_MODBUS_METER_DATA_HANDLER_H_
#define _DEV_MODBUS_METER_DATA_HANDLER_H_

#include "modbus_protocol.h"

void dev_modbus_meter_clean(void);
void dev_modbus_meter_del(uint64_t dev_sn);
void dev_modbus_meter_update(uint64_t dev_sn, uint32_t online_time);
void dev_modbus_meter_offline_set(uint64_t dev_sn);

int dev_modbus_meter_slave_addr_get(uint16_t dev_type, uint64_t dev_sn);
uint16_t vLookupMeterDataMaxLength(uint16_t regStartAddr);
const uint16_t* vLookupMeterDataTab(uint16_t dev_type, uint64_t dev_sn, uint16_t iReadAddr, uint16_t iReadNum, bool is_write);
int dev_modbus_meter_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len);

md_read_t dev_modbus_meter_polling_read_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step);

void dev_modbus_meter_data_summary(void);


#endif  /*end _DEV_MODBUS_METER_DATA_HANDLER_H_*/

