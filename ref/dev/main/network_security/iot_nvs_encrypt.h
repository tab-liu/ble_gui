
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
#include "iot_ble_encrypt.h"

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


//     uint8_t wifi_enable;        // wifi 开启关闭
//     uint8_t wifi_sta_enable;    // wifi sta 开启关闭
//     uint8_t wifi_ap_enable;     // wifi ap 开启关闭
//     uint8_t ble_enable;         // 蓝牙开启关闭
//     uint8_t ble_mash_enable;    // 蓝牙组网开启关闭
//     uint8_t _4g_enable;         // 4g 开启关闭
//     uint8_t gps_enable;         // gps 开启关闭

//     uint8_t wifi_sta_auth;      // wifi 作为sta时 需要连接的路由器加密方式
//     char wifi_sta_ssid[32];     // wifi 作为sta时 需要连接的路由器名称
//     char wifi_sta_password[32]; // wifi 作为sta时 需要连接的路由器密码
//     uint8_t wifi_ap_auth;       // wifi 作为apa时 广播时器加密方式
//     char wifi_ap_ssid[32];      // wifi 作为ap时 广播的名称
//     char wifi_ap_password[32];  // wifi 作为ap时 广播的密码
//     char could_dns[128];        // 服务器地址
//     char nc[20];                // 保留
//     uint16_t crc16;
// }dev_info_t;

extern void encrypt_nvs_test(void);
esp_err_t nvs_write_decrypt(const char *label, uint8_t *data, uint32_t len);
esp_err_t nvs_read_decrypt(const char *label, uint8_t *data, uint32_t len);
esp_err_t nvs_read_len(const char *label, uint16_t *len);
extern void copy_devinfo_Data(uint8_t *data, uint16_t len);
extern void copy_factory_data(uint8_t *data, uint16_t len);
esp_err_t save_cert_md5_to_nvs(const uint8_t *data, size_t len);
esp_err_t read_cert_md5_from_nvs(uint8_t *data, size_t len);

#endif