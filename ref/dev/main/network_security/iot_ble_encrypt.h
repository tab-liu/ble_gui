/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_ble_encry.h
Author: weiyt
Version: V1.0
Date: 2022.10.10
Description: none
*************************************************/
#ifndef IOT_BLE_ENCRYPT_H
#define IOT_BLE_ENCRYPT_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"

#include "esp_mac.h"

enum {
    AUTHEN_REQUEST = 0x01, //鉴权请求
    AUTHEN_RESOPN = 0x02,  //鉴权回应
    AUTHEN_RESLUT = 0x03,  //鉴权结果
    ECDH_AGRESSMENT_REQUEST = 0x04, //密钥协商请求
    ECDH_AGRESSMENT_RESPOND = 0x05, //密钥协商回应
    ECDH_AGRESSMENT_RESULT = 0x06  //密钥协商结果

};

extern uint8_t iot_ble_authen_request(uint8_t *data);
extern uint8_t iot_ble_check_authen_respond(uint8_t *data, uint8_t len, uint8_t *reslt_resp, uint8_t *rlen);
extern uint8_t iot_ble_authen_respond(uint8_t *data, uint8_t len, uint8_t *reslt_resp, uint8_t *rlen);
extern void iot_encrypt_data(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen);
extern void iot_decrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t dlen, uint16_t *olen);
//extern void iot_aes_encrypt_data(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen);
//extern void iot_aes_decrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t *olen);
extern void encrypt_sending_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len);
extern void decrypt_receive_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len);
extern uint16_t get_encrypt_sending_pack_len(uint16_t len);
extern void aes_key_init(void);
void md5_calculate_key_test(uint32_t value);

extern void iot_communciate_encrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen);
extern void iot_communciate_decrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen);
extern void iot_file_encrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen);
extern void iot_file_decrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen);
extern void aes_cbc_encrypt_sending_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len);
extern void aes_cbc_decrypt_receive_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len);
extern uint16_t iot_ble_authen_ecdh_key_agreement_request(uint8_t *data);
extern uint8_t iot_ble_authen_ecdh_key_agreement_respond_check(uint8_t *data, uint8_t len, uint8_t *reslt_resp, uint8_t *rlen);
extern uint16_t mobile_ble_authen_ecdh_key_agreement_respond(uint8_t *data);
extern void refresh_encrypt_param(void);
extern uint8_t get_pack_type(uint8_t *data, uint8_t len);
void encrypt_data_init(void);
void zzapp_main (void);


#endif
