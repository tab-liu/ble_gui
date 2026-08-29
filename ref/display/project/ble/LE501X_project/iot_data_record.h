/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_data_record.h
Author: weiyt
Version: V1.0
Date: 2021.11.1
Description: none
*************************************************/

#ifndef IOT_DATA_RECORD_H
#define IOT_DATA_RECORD_H

#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "iot_modbus_slave.h"

#define IOT_FACTORY_INFO_ADDRESS 0x0000
#define IOT_FACTORY_INFO_MAX_LEN 4096 // 4k

#define POWER_FACTORY_INFO_ADDRESS 0x1000
#define POWER_FACTORY_INFO_MAX_LEN 4096

typedef struct {
    uint64_t DeviceSn;
    uint8_t DeviceType[DEVICE_TYPE_SRTING_LENGTH]; //
    uint16_t flags;
} __attribute__((packed)) DeviceFactory;

typedef struct {
    uint64_t IotSn;
    uint64_t SafetyCode; //
    uint8_t IotType[DEVICE_TYPE_SRTING_LENGTH];
    uint16_t flags;
} __attribute__((packed)) IotFactory;

typedef struct {
    DeviceFactory device;
    IotFactory iot;
} __attribute__((packed)) SystemFactoryInfo;

extern SystemFactoryInfo gFactoryInfo;

int WritePowerDeviceInfo(void);
int ReadPowerDeviceInfo(void);

int WriteIotFactoryInfo(void);
int ReadIotFactoryInfo(void);

int IotFactoryConfigCheck(void);
int PowerDeviceConfigCheck(void);

extern void LsbtoMsb(uint8_t *dst, uint8_t *src, uint16_t len);
uint64_t short2long(uint16_t data[4]);
void long2short(uint16_t data[4], uint64_t src);

void ResLong2short(uint16_t data[4], uint64_t src);
uint64_t ResShort2long(uint16_t data[4]);

int Iot_Factroy_Info_test(uint8_t num);
extern uint64_t get_IotSn(void);
int WriteIotFactoryInfo_to_nvs(void);

extern uint16_t ntohs(uint16_t src);

#endif
