/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_partition.h
Author: weiyt
Version: V1.0
Date: 2022.10.10
Description: none
*************************************************/
#ifndef IOT_PARTITION_H
#define IOT_PARTITION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include <assert.h>
#include "esp_partition.h"
#include "esp_log.h"

#include "mbedtls/md.h"
#include "mbedtls/platform_util.h"

#include "esp_ota_ops.h"

#include "comm_define.h"

extern uint8_t *ca_cert_ptr;  //公钥证书指针
extern uint16_t ca_cert_ptr_len;
extern uint8_t *iot_cert_ptr; //设备证指针
extern uint16_t iot_cert_ptr_len;
extern uint8_t *private_key_ptr; //私钥指针
extern uint16_t private_key_ptr_len;

extern uint8_t *server_ca_cert_ptr;  //CA证书指针
extern uint16_t server_ca_cert_ptr_len;
extern uint8_t *server_cert_ptr; //设备证书指针
extern uint16_t server_cert_ptr_len;
extern uint8_t *server_key_ptr; //私钥指针
extern uint16_t server_key_ptr_len;

#ifdef     TCP2_ENCRYPT_ENABLE	
//第二个服务器的证书(对私服务器)
extern uint8_t *ca_cert_ptr_2;  //CA证书指针
extern uint8_t *iot_cert_ptr_2; //设备证书指针
extern uint8_t *private_key_ptr_2; //私钥指针
#else

#endif	
typedef struct
{
    uint8_t start_flag[2]; //开始标志
    uint32_t plaintext_len; //明文长度
    uint32_t ciphertext_len; //密文长度  
    uint8_t type; //类型
    uint8_t complate_flag; //类型
    uint8_t res[4]; //保留
}__attribute__((packed)) PartitionStruct; //单字节对齐

typedef struct partition_type
{
    uint8_t type;
    char name[20];
}partition_type_t;


typedef union  
{
    uint8_t data[sizeof(PartitionStruct)]; 
    PartitionStruct    PartitionDataStruct;
}PARTITION_DATA_UNION;


enum{
	CA_CERTIFICATE_AREA = 1,  //CA证书区
	IOT_CERTIFICATE_AREA,  //IOT设备证书区
    PRIVATE_AREA,  //私钥匙区
    UPDATE_AREA, //升级区(缓存设备证书、私钥)
#ifdef     TCP2_ENCRYPT_ENABLE	
    CA_CERTIFICATE_AREA_2,  //第二套CA证书区
	IOT_CERTIFICATE_AREA_2,  //IOT设备证书区
    PRIVATE_AREA_2,  //私钥匙区
    UPDATE_AREA_2, //升级区(缓存设备证书、私钥)
#endif	
    CUSTOM_DATA_AREA,
    MD_TCP_SERVER_CA_AREA,  //Modbus TCP服务器CA证书区
    MD_TCP_SERVER_CERT_AREA, //Modbus TCP服务器证书区
    MD_TCP_SERVER_PRIVATE_AREA, //Modbus TCP服务器私钥区
};

enum{
	RAW_INFO = 1,
	NORMAL_INFO,
};


void test_partition(void);

extern void partition_init(uint8_t area);
extern void partition_reinit(uint8_t area, uint32_t erase_size);
extern uint32_t partition_read_decrypt(uint8_t *data, uint32_t len, uint32_t offset, uint8_t area);
extern void partition_write_encrypt(uint8_t *data, uint32_t len, uint32_t offset, uint8_t area);
extern uint8_t load_all_cer(void);
uint8_t load_modbus_tcp_cert(void);
extern void iot_sha256_bin(const mbedtls_md_info_t *md_info, uint32_t len, unsigned char *output);
extern void other_device_sha256_bin(uint32_t address, const mbedtls_md_info_t *md_info, uint32_t len, unsigned char *output);
extern void save_cer_to_update_area(uint8_t *data, uint16_t len, uint16_t offset);
extern void write_cert_factory_complate(void);
extern void format_cert_factory_area(void);
extern void check_for_cert_update(void);
extern void read_cert(void);
void print_partition_info(uint8_t area, uint8_t info_type);

void test_bin(void);
void test_sha256(void);
void test_update(void);
uint32_t get_partition_plaintext_len(uint8_t area);//A
int16_t write_cert_to_file(uint8_t type, uint8_t *data, uint16_t len);
void write_cert(void);  //测试使用接口

#endif
