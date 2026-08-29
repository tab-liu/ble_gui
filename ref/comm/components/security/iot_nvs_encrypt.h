
/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_nvs_encrypt.h
Author: weiyt
Version: V1.0
Date: 2022.10.10
Description: none
*************************************************/

#ifndef  IOT_NVS_ENCRYPT_H
#define IOT_NVS_ENCRYPT_H


#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>  
#include <string.h> 
#include <stdlib.h>  
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_wifi.h"

typedef struct
{
    uint8_t start_flag[2]; //开始标志
    uint32_t plaintext_len; //明文长度
    uint32_t ciphertext_len; //密文长度  
    uint8_t res[6]; //保留
}__attribute__((packed)) NvsStruct; //单字节对齐

typedef union
{
    uint8_t data[sizeof(NvsStruct)];
    NvsStruct    NvsDataStruct;
}NVS_DATA_UNION;

extern void encrypt_nvs_test(void);
esp_err_t nvs_write_decrypt(const char *label, uint8_t *data, uint32_t len);
esp_err_t nvs_read_decrypt(const char *label, uint8_t *data, uint32_t len);
esp_err_t nvs_read_len(const char *label, uint16_t *len);
extern void copy_devinfo_Data(uint8_t *data, uint16_t len);
extern void copy_factory_data(uint8_t *data, uint16_t len);
esp_err_t save_cert_md5_to_nvs(const uint8_t *data, size_t len);
esp_err_t read_cert_md5_from_nvs(uint8_t *data, size_t len);

#endif
