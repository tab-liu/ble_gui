/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_flash.h
Author: weiyt
Version: V1.0
Date: 2022.5.25
Description: none
*************************************************/

#ifndef IOT_FLASH_H
#define IOT_FLASH_H

#include "ls_hal_flash.h"
#include "le501x.h"
#include "ls_hal_cache.h"
#include "compile_flag.h"
#include "sdk_config.h"
#include "platform.h"
#include "reg_gpio.h"
#include "reg_syscfg.h"
#include "common.h"

#define SPI_FLASH_SEC_SIZE 4096 /*最小擦除大小位4K*/

uint8_t FlashErase(uint32_t address, uint32_t size);
uint8_t FlashWriteBuffer(uint32_t flashAddress, uint8_t *byteBuf, uint16_t len);
uint8_t FlashReadBuffer(uint32_t flashAddress, uint8_t *byteBuf, uint16_t len);
uint8_t FlashReadByte(uint32_t flashAddress);
uint16_t FlashReadHalfWord(uint32_t flashAddress);
uint32_t FlashReadWord(uint32_t flashAddress);

#endif
