#ifndef _DEV_MODBUS_S1_DATA_HANDLER_H_
#define _DEV_MODBUS_S1_DATA_HANDLER_H_

#include "modbus_protocol.h"

void dev_modbus_s1_clean(void);
void dev_modbus_s1_del(uint64_t dev_sn);
void dev_modbus_s1_update(uint64_t dev_sn, uint32_t online_time);
void dev_modbus_s1_offline_del(void);

int dev_modbus_s1_slave_addr_get(uint16_t dev_type, uint64_t dev_sn);
md_read_t dev_modbus_s1_polling_read_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step);

const uint16_t* vLookupS1DataTab(uint16_t dev_type, uint64_t dev_sn, uint16_t iReadAddr, uint16_t iReadNum, bool is_write);

int dev_modbus_s1_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len);
int modbus_s1_report_handle(uint8_t *income, int len, uint64_t dev_sn);

void dev_modbus_s1_data_summary(void);

#endif  /*end _DEV_MODBUS_S1_DATA_HANDLER_H_*/

