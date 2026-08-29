#ifndef _DEV_MODBUS_S1_DATA_HANDLER_H_
#define _DEV_MODBUS_S1_DATA_HANDLER_H_

#include "modbus_protocol.h"
#include "inv_define.h"

enum {
    PLUG_MODEL_S1 = 1, // bluetti S1
};

void dev_modbus_s1_clean(void);
void dev_modbus_s1_del(uint64_t dev_sn);
void dev_modbus_s1_update(uint64_t dev_sn, uint32_t online_time);

int dev_modbus_s1_slave_addr_get(uint16_t dev_type, uint64_t dev_sn);
md_read_t dev_modbus_s1_polling_read_rtn_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step);

uint16_t vLookupS1DataMaxLength(uint16_t regStartAddr);
const uint16_t* vLookupS1DataTab(uint16_t dev_type, uint64_t dev_sn, uint16_t iReadAddr, uint16_t iReadNum, bool is_write);
int dev_modbus_plug_data_get(uint16_t dev_type, uint64_t dev_sn, 
											uint8_t slaveAddr, uint8_t funcode, 
											uint16_t regAdderss, uint16_t gRegCnt, 
											uint8_t *response);

int dev_modbus_s1_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len);
int modbus_s1_report_handle(uint8_t *income, int len, uint64_t dev_sn);
int dev_modbus_plug_set_read_handle(uint16_t dev_type, uint64_t dev_sn, bool fast_flag);
int dev_modbus_plug_data_read_handle(uint16_t dev_type, uint64_t dev_sn);

void dev_modbus_s1_data_summary(void);
uint8_t dev_smart_plug_online(void);

int16_t plug_info_report_process(POINT_BIND_INFO *point_bind_info, bool is_all);
uint16_t plug_is_online(uint64_t dev_sn);
void plug_bind_process(const POINT_BIND_INFO_WR *point_bind_cmd);
int dev_modbus_plug_slave_addr_get(uint16_t dev_type, uint64_t dev_sn);
uint8_t plug_bind_check(uint64_t plug_sn);
int16_t plug_is_exist(uint64_t dev_sn);
uint8_t plug_info_status_check(void);

#endif  /*end _DEV_MODBUS_S1_DATA_HANDLER_H_*/

