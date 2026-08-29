/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_crc32.h
Author: weiyt
Version: V1.0
Date: 2021.11.1
Description: none
*************************************************/

#ifndef IOT_CRC32_H
#define IOT_CRC32_H

// #include "lscrc.h"

#include <string.h>
#include <stdio.h>
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"

extern uint32_t CRC_CalcCRC(uint32_t Data);
extern uint32_t CRC_GetCRC(void);
extern void CRC_ResetDR(void);

// extern uint32_t Esp32CRC_CalcCRC(uint32_t Data);
// extern uint32_t Esp32CRC_GetCRC(void);
// extern void Esp32CRC_ResetDR(void);

#endif
