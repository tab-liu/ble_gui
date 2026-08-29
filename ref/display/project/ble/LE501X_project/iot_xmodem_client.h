/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_xmodem_client.h
Author: weiyt
Version: V1.0
Date: 2022.5.25
Description: none
*************************************************/
#ifndef IOT_XMODEM_CLIENT_H
#define IOT_XMODEM_CLIENT_H

#include <string.h>
#include <stdio.h>
#include "stdint.h"
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"

#define DEVICE_IOT 0
#define BIN_FILE_IOT_ADDRESS 0x98000

typedef struct {
    uint8_t used;
    uint8_t crc;
    uint8_t seq;
    uint16_t len;
    uint8_t *data;
} Buffer;

typedef struct {
    uint8_t port;
    uint8_t packSeq;
    uint8_t finished;
    uint32_t fileAddress;
    uint8_t retry;
    Buffer code[2];
} ClientStruct;

void XmodemClientInit(uint16_t target, uint8_t channel);
extern uint8_t XmodemClientTask(const uint8_t *income, uint16_t inLen, uint8_t channel);

#endif
