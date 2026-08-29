/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: flash驱动，flash写入和读取
Version: V1.00
Function List: none
***********************************************************/

#include "iot_flash.h"

/* flash once write max 256 byte */
uint8_t FlashWriteBuffer(uint32_t flashAddress, uint8_t *byteBuf, uint16_t len) {
    uint8_t status = 1;
    hal_flash_quad_page_program(flashAddress, byteBuf, len);
    return status;
}

uint8_t FlashReadBuffer(uint32_t flashAddress, uint8_t *byteBuf, uint16_t len) {
    uint8_t status = 1;
    hal_flash_quad_io_read(flashAddress, byteBuf, len);
    return status;
}

uint8_t FlashReadByte(uint32_t flashAddress) {
    uint8_t data;
    hal_flash_quad_io_read(flashAddress, &data, 1);
    return data;
}

uint32_t FlashReadWord(uint32_t flashAddress) {
    uint32_t data;
    uint8_t value[2];
    hal_flash_quad_io_read(flashAddress, value, 2);
    data = value[0];
    data |= (uint32_t)value[1] << 8;
    return data;
}

/*擦除*/
uint8_t FlashErase(uint32_t address, uint32_t size) {
    uint8_t status = 1;
    uint32_t offset = address;
    while (size) {
        hal_flash_sector_erase(offset);
        if (size > SPI_FLASH_SEC_SIZE) {
            size -= SPI_FLASH_SEC_SIZE;
            offset += SPI_FLASH_SEC_SIZE;
        } else {
            break;
        }
    }
    return status;
}
