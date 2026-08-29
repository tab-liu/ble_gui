/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_crc16.h
Author: weiyt
Version: V1.0
Date: 2021.11.1
Description: none
*************************************************/

#ifndef IOT_CRC16_H
#define IOT_CRC16_H

#include <string.h>
#include <stdio.h>
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"

unsigned short Modbus_Crc16Cal(const unsigned char *cmd, int len);
unsigned short Xmodem_Crc16Cal(const void *buf, int length);
// unsigned short Modbus_Crc16Cal(const void* ptr, int length);

#endif
