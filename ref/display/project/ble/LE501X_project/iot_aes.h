/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_ble.h
Author: xiongf
Version: V1.0
Date: 2022.5.23
Description: none
*************************************************/

#ifndef AES_H
#define AES_H

#include <string.h>
#include <stdio.h>
#include "ls_ble.h"
#include "prf_diss.h"
#include "prf_fotas.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "builtin_timer.h"
#include "ls_hal_trng.h"
#include "platform.h"
#include "iot_ble.h"

enum {
    IDENT_PASS = 0,
    IDENT_FAIL = 1,
};

enum {
    iv_cbc_encry = 0,
    iv_cbc_decry = 1,
};

enum {
    identify_request = 0x01,
    identify_verity = 0x02,
    identify_rst = 0x03,
    ECDH_key_request = 0x04,
    ECDH_key_check = 0x05,
    ECDH_key_rsp = 0x06,
    IDENTIFY_OK = 0x07,
    // test_sign = 0x07,
};

void aes_ctr_decrypt(uint8_t *data, uint8_t *key, uint8_t *nonce);
bool check_sum(uint8_t *data, uint16_t len);
void identify_verify_pack(uint8_t *pack_data, uint8_t *md5);
extern uint8_t identify_rst_check(uint8_t *rst_data, uint8_t *local_MD5);
bool client_ECDH_key_req_check(uint8_t *req_data, uint8_t *local_MD5);
extern uint8_t ECDH_key_rst_check(uint8_t *rst_data, uint8_t *local_MD5);
extern uint8_t *identify_rst_pack(uint8_t *dst, uint8_t rst);
extern uint8_t *ECDH_key_rsp_pack(uint8_t *dst, uint8_t rst);
extern uint8_t *identify_request_pack(uint8_t *dst);
extern uint8_t *ECDH_key_request_pack(uint8_t *dst, uint16_t len);
extern uint16_t Crc_Sum(uint8_t *data, uint16_t len);
extern void Pdc_Aes_key_first(uint8_t *MD5_data);
extern void Pdc_Aes_key_second(uint8_t *MD5_data);
void client_Pdc_Aes_key_first(uint8_t *MD5_data);
void client_Pdc_Aes_key_second(uint8_t *MD5_data);
extern void MD5_Encrpyt(const uint8_t *src, uint8_t *dst, uint16_t len);
extern int8_t Aes_Ecb_decry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len);
extern uint16_t Aes_Ecb_encry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len);
extern uint16_t Aes_Cbc_encry(uint8_t *data, uint16_t text_len);
extern int8_t Aes_Cbc_decry(uint8_t *data, uint16_t text_len);
int8_t client_Aes_Ecb_decry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len);
uint16_t client_Aes_Ecb_encry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len);
uint16_t client_Aes_Cbc_encry(uint8_t *data, uint16_t text_len);
int8_t client_Aes_Cbc_decry(uint8_t *data, uint16_t text_len);
extern void Inver_Data(uint8_t *src, uint16_t len);
extern int Verify_test(uint8_t *text,
                       uint16_t text_len,
                       uint8_t *MD5_Data,
                       uint8_t *verify_pub_key,
                       uint8_t *sign_key);
extern int Sign_test(
    uint8_t *prv_key, uint8_t *MD5_Data, uint8_t *text, uint8_t text_len, uint8_t *sign_buf);
extern void OS_sign_test(uint8_t *sign_cache, uint8_t *rst, uint8_t *MD5);
bool client_OS_sign_test(uint8_t *sign_cache, uint8_t *rst, uint8_t *MD5);
extern bool OS_verify_test(uint8_t *rst, uint8_t *MD5);
extern void IOT_sign_test(uint8_t *sign_cache, uint8_t *rst, uint8_t *MD5);
extern int IOT_verify_test(uint8_t *rst, uint8_t *MD5);
extern void Pdc_Rng_data(uint8_t *dst);
extern void time_test(void);
extern void Ecc_fill_rng_function(void);
extern void Ecc_Make_key(uint8_t *pack_ecdh_sign_buf,
                         uint8_t *pack_buf,
                         uint8_t *MD5_all,
                         uint8_t *md5_sec);
extern void IOT_APP_TEST(void);
#endif
