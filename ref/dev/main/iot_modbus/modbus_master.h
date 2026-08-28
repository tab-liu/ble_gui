#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "modbus_master_data.h"



uint16_t Modbus_MasterWriteRegs (uint8_t addr, uint16_t start, uint16_t reg_num, uint8_t *out_buf);

uint16_t Modbus_WriteCmd_06H_10H_Build(uint8_t slave_address, uint16_t regAddress, uint8_t regNum, uint8_t *inbuf, uint8_t *outbuf);

#endif
