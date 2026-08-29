/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_xmodem.h
Author: weiyt
Version: V1.0
Date: 2022.5.25
Description: none
*************************************************/
#ifndef IOT_XMODEM_H
#define IOT_XMODEM_H

#include <string.h>
#include <stdio.h>
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "stdint.h"
#include "iot_software_timer.h"

#define SOH 0x01
#define STX 0x02
#define ETX 0x03
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define CTRLZ 0x1A

#define IDLE_PROTOCOL 0
#define CLIENT_PROTOCOL 1
#define SERVER_PROTOCOL 2

#define IDLE 0
#define CLIENT 1
#define SERVER 2

#define XMODEM_CLIENT_TIMEOUT 3000
#define XMODEM_SERVER_TIMEOUT 8000

#define BLE_CHANNEL 2

enum {
    FILE_EMPTY = 0,
    FILE_RECEIVEING = 1,
    FILE_RECEIVED = 2,
    FILE_SENDING = 3,
    FILE_SENDED = 4,
    FIlE_TIMOUT = 5,
};

typedef struct {
    signed short type; // current downloading device
    unsigned short
        status[4]; // binary status, status[0]:IOT, status[1]:ARM, status[2]:DSP, status[3]:BMS
    unsigned short totalSeq; // fileSize / 1024
    unsigned short dummyAddress; // 0xAABB
} __attribute__((__packed__)) file_struct;

typedef struct {
    file_struct binFile;
    signed short precent;
} FileStruct;

extern uint8_t buf1[1029];
extern uint8_t buf2[1029];

unsigned short Xmodem_Crc16Cal(const void *buf, int length);
uint8_t CalcCodeCrc32(uint8_t clip, const uint8_t *code, uint16_t len);
extern FileStruct gXmodemFile;
extern uint8_t gXmodemType;

uint8_t EsareCode(uint16_t target);
uint8_t SaveCode(
    uint32_t fileAddress, uint16_t target, uint16_t seq, uint8_t *code, uint16_t codeLen);
int BinaryFileInfoWrite(uint16_t target, uint16_t seq, uint8_t state);
int BinaryFileInfoRead(void);
int BinaryFileSend(void);
// int BinaryFileLoader(void);
void SetProgressBar(int16_t pcs);

#endif
