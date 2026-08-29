#ifndef IOT_MODBUS_MASTER_H
#define IOT_MODBUS_MASTER_H

#include "iot_modbus_slave.h"

uint8_t Modbus_Master(const uint8_t *cmdBuf, uint16_t cmdLen);
uint16_t Modbus_MasterReadCmd(uint16_t regAddress, uint8_t regNum, uint8_t *cmdbuf);
uint16_t Modbus_MasterWriteCmd(uint16_t regAddress,
                               uint8_t regNum,
                               uint16_t *data,
                               uint8_t *outbuf);
uint8_t Modbus_ErrorCode(void);

#endif
