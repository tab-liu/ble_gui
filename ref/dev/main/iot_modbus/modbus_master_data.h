
#ifndef MODBUS_MASTER_DATA_H
#define MODBUS_MASTER_DATA_H


#include "modbus_data.h"

extern  uint16_t FRAME_BIND_INFO_REG_COUNT ;//const
extern  uint16_t NET_SERVER_2RD_REG_COUNT	 ;//const


const reg_array* Master_vLookupDataTab(uint16_t iReadAddr, uint16_t iReadNum, bool is_write);
const uint16_t* Master_vLookupDataTab2(uint8_t addr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write);
const uint16_t *vLookupDataTab(uint8_t SlaveAddr, uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t *reg_position);

#endif
