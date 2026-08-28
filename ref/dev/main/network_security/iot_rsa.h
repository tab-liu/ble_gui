/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_rsa.h
Author: weiyt
Version: V1.0
Date: 2022.10.20
Description: none
*************************************************/

#ifndef IOT_RSA_H
#define IOT_RSA_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "mbedtls/rsa.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"
#include "mbedtls/pk.h"
#include "certs.h"//"mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/psa_util.h"
typedef struct key_type
{
    uint8_t *key;
    uint16_t len;
}key_type_t;


typedef struct {
    volatile uint16_t year;    // 2019+
    volatile uint8_t month;    // 0-11
    volatile uint8_t day;      // 1-31
    volatile uint8_t seconds;  // 0-59
    volatile uint8_t minutes;  // 0-59
    volatile uint8_t hour;     // 0-23
    volatile uint8_t weekday;  // 0 means sunday
} my_utc_time_t;

extern uint32_t cert_utc_sec;

extern int cer_ras_encrypt(const unsigned char* plaintext, int plaintext_len, unsigned char* ciphertext);
extern int priv_rsa_decrypt(unsigned char* ciphertext, unsigned char* plaintext, uint16_t *plaintext_len);
extern int priv_rsa_sign(unsigned char* plaintext, unsigned int plaintext_len, unsigned char* sign_output);
extern int ca_rsa_verify(unsigned char* plaintext, unsigned int plaintext_len, unsigned char* sign_output);
extern void rsa_key_init(void);
int ca_rsa_verify_ota_bin(uint32_t bin_len, unsigned char* sign_output);
int ca_rsa_verify_other_device_bin(uint32_t address, uint32_t bin_len, unsigned char* sign_output);
extern void priv_rsa_refresh_seed(void);
extern uint32_t get_certificate_utc_sec(void);
extern uint16_t cer_ras_get_serial_number(char *SerialNumber);
uint16_t cer_ras_get_common_name(char *common_name, uint16_t cn_size);
extern void convert_mac_string(const char *input, char *output);
extern void byte_to_hex(unsigned char byte, char *hex);
extern void bin_to_hex(const unsigned char *bin_data, size_t bin_len, unsigned char *out_data);
extern uint32_t calculate_certificate_utc_sec(uint8_t *cert);

#endif