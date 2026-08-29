/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: xiongf
Date: 2022/11/23
Description: ble鉴权、通信加密解密接口
Version: V1.00
Function List: none
***********************************************************/
#define LOG_TAG "IOT_AES"

#include <tinycrypt/ctr_mode.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/constants.h>

#include "iot_uart.h"
#include "iot_md5.h"
#include "iot_aes.h"
#include "ls_hal_crypt.h"
#include "tinycrypt/sha256.h"
#include "uECC.h"
#include "ls_hal_timer.h"

#define Log_Aes 01

uint8_t *OUTPUT = NULL;
uint32_t aes_key[4];
uint32_t client_aes_key[4];
uint32_t cbc_key_256[8];
uint32_t client_cbc_key_256[8];
uint32_t cbc_iv[4];
uint8_t IOT_swap_prv_key[32];
uint8_t IOT_swap_pub_key[64];
uint8_t OS_swap_pub_key[64];
uint8_t OS_swap_prv_key[32];

uint8_t IOT_verify_prv_key[] = {0x85, 0xC3, 0x2C, 0x6B, 0xCC, 0xC5, 0x06, 0xF5, 0xED, 0x08, 0x18,
                                0x26, 0xAA, 0x61, 0x21, 0x1F, 0x33, 0x9C, 0xF1, 0x19, 0xB5, 0x56,
                                0x7F, 0xF6, 0x4C, 0x7B, 0xF9, 0x2B, 0x19, 0x81, 0x9C, 0xF3};

// uint8_t OS_verify_prv_key[] = {0x91, 0x11, 0xb8, 0xbf, 0xa5, 0x13, 0x3c, 0x95, 0xb2, 0x3d, 0x5f,
//                                0x49, 0x3c, 0x87, 0xd7, 0xa3, 0x28, 0x29, 0x58, 0xb4, 0x44, 0x0e,
//                                0x76, 0xeb, 0x27, 0x50, 0x60, 0xb0, 0x7a, 0x83, 0x22, 0xaf};
uint8_t OS_verify_prv_key[] = {0x4F, 0x19, 0xA1, 0x6E, 0x3E, 0x87, 0xBD, 0xD9, 0xBD, 0x24, 0xD3,
                               0xE5, 0x49, 0x5B, 0x88, 0x04, 0x15, 0x11, 0x94, 0x3C, 0xBC, 0x8B,
                               0x96, 0x9A, 0xDE, 0x96, 0x41, 0xD0, 0xF5, 0x6A, 0xF3, 0x37};

uint8_t OS_verify_pub_key[] = {
    0xA7, 0x3A, 0xBF, 0x5D, 0x22, 0x32, 0xC8, 0xC1, 0xC7, 0x2E, 0x68, 0x30, 0x43, 0x43, 0xC2, 0x72,
    0x49, 0x5E, 0x3A, 0x8F, 0xD6, 0xF3, 0x0E, 0xA9, 0x6D, 0xE2, 0xF4, 0xB3, 0xCE, 0x60, 0xB2, 0x51,
    0xEE, 0x21, 0xAC, 0x66, 0x7C, 0xF8, 0xA7, 0x1E, 0x18, 0xB4, 0x6B, 0x66, 0x4E, 0xAE, 0xFF, 0xE3,
    0xC4, 0x89, 0xF2, 0x4F, 0x69, 0x5B, 0x64, 0x11, 0xDB, 0x7E, 0x22, 0xCC, 0xC8, 0x5A, 0x85, 0x94};

uint8_t IOT_verify_pub_key[] = {
    0x3F, 0x29, 0xE1, 0xB8, 0xB2, 0x9D, 0x84, 0x22, 0xBB, 0xB0, 0xE0, 0xF0, 0x90, 0x00, 0xCF, 0x2E,
    0xE2, 0x93, 0x1D, 0xA1, 0x3C, 0xDA, 0xC8, 0x12, 0x9F, 0x8C, 0x09, 0xDE, 0xAC, 0x07, 0xF5, 0x19,
    0xB5, 0xAC, 0x55, 0x09, 0xCA, 0x3F, 0x3F, 0x59, 0xB2, 0x34, 0xB7, 0x77, 0x7A, 0x23, 0x1A, 0xE5,
    0x95, 0xCA, 0x5A, 0xA1, 0x74, 0x5F, 0xD7, 0xA6, 0x2B, 0xC4, 0xD0, 0x20, 0x37, 0x11, 0x7D, 0xD1};

static uint8_t local_key[16];
extern bool Xmoderm_ota;

struct fw_digest cipherbuffer_sha256;

/*每16个数据倒置*/
void swap16_bytes(uint8_t *in_data, uint8_t *out_data) {
    for (uint8_t i = 0; i < 16; i++) {
        out_data[i] = in_data[15 - i];
    }
}

/*数据倒置*/
void Inver_Data(uint8_t *src, uint16_t len) {
    uint8_t temp[16];
    uint16_t count = len / 16;
    uint16_t offset = 0;
    //	LOG_I("len is %d!",len);
    for (uint16_t i = 0; i < count; i++) {
        swap16_bytes(src + offset, temp);
        memcpy(src + offset, temp, 16);
        offset += 16;
    }
}
void my_Inver_Data(uint8_t *src, uint16_t len) {
    uint8_t temp[64];
    memcpy(temp, src, len);
    //	LOG_I("len is %d!",len);
    for (uint16_t i = 0; i < len; i++) {
        src[i] = temp[len - i - 1];
    }
}

// aes-ctr 解密 16字节数据，原地解密
void aes_ctr_decrypt(uint8_t *data, uint8_t *key, uint8_t *nonce) {
    uint8_t iv[16] = {0};
    struct tc_aes_key_sched_struct sched;
    tc_aes128_set_encrypt_key(&sched, key);

    iv[0] = nonce[0];
    iv[1] = nonce[1];

    tc_ctr_mode(data, 16, data, 16, iv, &sched);
}

static uint16_t Aes_Ecb_encry_core(uint8_t *data,
                                   uint8_t *MD5_DATA,
                                   uint16_t text_len,
                                   uint32_t *aes_key) {
    uint8_t count = 0;
    uint16_t data_len;
    uint32_t length;

    if (NULL == OUTPUT) {
        OUTPUT = malloc(1200 * sizeof(uint8_t));
    }

    if (text_len % 16 != 0) {
        data_len = (text_len / 16 + 1) * 16;
    } else {
        data_len = text_len;
    }
    count = data_len / 16; // 加密轮数
    uint16_t aes_offset = 0;
    uint8_t md5_data_temp[16];

    memcpy(md5_data_temp, MD5_DATA, sizeof(md5_data_temp)); // 传入第一次生成的MD5；
    for (uint8_t i = 0; i < 4; i++) {
        cbc_iv[i] = ((uint32_t)md5_data_temp[12 - 4 * i] << 24)
                    + ((uint32_t)md5_data_temp[13 - 4 * i] << 16)
                    + ((uint32_t)md5_data_temp[14 - 4 * i] << 8)
                    + md5_data_temp[15 - 4 * i]; // 打包MD5到初始化向量
    }

    // Inver_Data(data, data_len); // 倒置传入要加密的明文 // SDK v2.1后不需要
    HAL_LSCRYPT_Init();
    HAL_LSCRYPT_AES_Key_Config(aes_key, AES_KEY_128);
    for (uint8_t i = 0; i < count; i++) {
        length = 16;
        HAL_LSCRYPT_AES_CBC_Encrypt(
            cbc_iv, data + aes_offset, 16, OUTPUT + aes_offset, &length); // data数据开始加密
        // Inver_Data(OUTPUT + aes_offset, 16);  // SDK v2.1后不需要
        for (uint8_t j = 0; j < 4; j++) { // 重装向量
            cbc_iv[j] = ((uint32_t)OUTPUT[12 - 4 * j + aes_offset] << 24)
                        + ((uint32_t)OUTPUT[13 - 4 * j + aes_offset] << 16)
                        + ((uint32_t)OUTPUT[14 - 4 * j + aes_offset] << 8)
                        + OUTPUT[15 - 4 * j + aes_offset]; // 打包MD5到初始化向量
        }
        aes_offset += 16;
    }
    memcpy(data, OUTPUT, data_len);
    free(OUTPUT);
    OUTPUT = NULL;
    HAL_LSCRYPT_DeInit();
    return data_len;
}

/*AES_CBC加密_第一阶段*/
uint16_t Aes_Ecb_encry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len) {
    return Aes_Ecb_encry_core(data, MD5_DATA, text_len, aes_key);
}

/* client端 AES_CBC加密_第一阶段*/
uint16_t client_Aes_Ecb_encry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len) {
    return Aes_Ecb_encry_core(data, MD5_DATA, text_len, client_aes_key);
}

/* AES_CBC解密_第一阶段 // text_len为明文长度 */
static int8_t Aes_Ecb_decry_core(uint8_t *data,
                                 uint8_t *MD5_DATA,
                                 uint16_t text_len,
                                 uint32_t *aes_key) {
    uint16_t data_len;
    uint8_t count = 0;
    uint16_t aes_count = 0;
    uint32_t length;

    if (NULL == OUTPUT) {
        OUTPUT = malloc(1200 * sizeof(uint8_t));
    }

    if (text_len % 16 != 0) { // 取16倍数算加密轮数
        data_len = (text_len / 16 + 1) * 16;
    } else {
        data_len = text_len;
    }
    count = data_len / 16; // 加密轮数

    for (uint8_t i = 0; i < 4; i++) {
        cbc_iv[i] = ((uint32_t)MD5_DATA[12 - 4 * i] << 24) + ((uint32_t)MD5_DATA[13 - 4 * i] << 16)
                    + ((uint32_t)MD5_DATA[14 - 4 * i] << 8)
                    + MD5_DATA[15 - 4 * i]; // 打包MD5到初始化向量
    }

    // Inver_Data(data, data_len); // 倒置传入要解密的密文  // SDK v2.1后不需要
    HAL_LSCRYPT_Init();
    HAL_LSCRYPT_AES_Key_Config(aes_key, AES_KEY_128);
    for (uint8_t i = 0; i < count; i++) {
        length = 16;
        HAL_LSCRYPT_AES_CBC_Decrypt(
            cbc_iv, data + aes_count, 16, OUTPUT + aes_count, &length); // 从data第七个数据开始加密
        // Inver_Data(data + aes_count, 16);  // SDK v2.1后不需要
        for (uint8_t j = 0; j < 4; j++) { // 重装向量
            cbc_iv[j] = ((uint32_t)data[12 - 4 * j + aes_count] << 24)
                        + ((uint32_t)data[13 - 4 * j + aes_count] << 16)
                        + ((uint32_t)data[14 - 4 * j + aes_count] << 8)
                        + data[15 - 4 * j + aes_count]; // 打包MD5到初始化向量
        }
        aes_count += 16;
    }
    // Inver_Data(OUTPUT, data_len);  // SDK v2.1后不需要
    memcpy(data, OUTPUT, text_len);
    free(OUTPUT);
    OUTPUT = NULL;
    HAL_LSCRYPT_DeInit();
    return data_len;
}

/*AES_CBC解密_第一阶段*/
int8_t Aes_Ecb_decry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len) { // text_len为明文长度
    return Aes_Ecb_decry_core(data, MD5_DATA, text_len, aes_key);
}

/* client端 AES_CBC解密_第一阶段 , text_len为明文长度 */
int8_t client_Aes_Ecb_decry(uint8_t *data, uint8_t *MD5_DATA, uint16_t text_len) {
    return Aes_Ecb_decry_core(data, MD5_DATA, text_len, client_aes_key);
}

/*AES_CBC加密_第二阶段*/
// 传入的数据长度必须是16的整数倍，且不足要补0（data前四位装随机数）
uint16_t Aes_Cbc_encry_core(uint8_t *data, uint16_t text_len, uint32_t *cbc_key_256) {
    uint8_t count = 0;
    uint16_t data_len;
    uint32_t length;

    if (NULL == OUTPUT) {
        OUTPUT = malloc(1200 * sizeof(uint8_t));
    }

    if (text_len % 16 != 0) {
        data_len = (text_len / 16 + 1) * 16;
    } else {
        data_len = text_len;
    }
    count = data_len / 16; // 加密轮数
    uint16_t aes_offset = 0;
    uint8_t temp_rng_data[4];
    uint8_t md5_data[16];

    Pdc_Rng_data(temp_rng_data); // 生成4个随机数
    MD5_Encrpyt(temp_rng_data, md5_data, sizeof(temp_rng_data)); // MD5加密4个随机数
    for (uint8_t i = 0; i < 4; i++) {
        cbc_iv[i] = ((uint32_t)md5_data[12 - 4 * i] << 24) + ((uint32_t)md5_data[13 - 4 * i] << 16)
                    + ((uint32_t)md5_data[14 - 4 * i] << 8)
                    + md5_data[15 - 4 * i]; // 打包MD5到初始化向量
    }

    memcpy(data, temp_rng_data, sizeof(temp_rng_data)); // data前四位装随机数
    // Inver_Data(data + 4, data_len); // 倒置传入要加密的明文 // SDK v2.1后不需要
    HAL_LSCRYPT_Init();
    HAL_LSCRYPT_AES_Key_Config(cbc_key_256, AES_KEY_256);
    for (uint8_t i = 0; i < count; i++) {
        length = 16;
        HAL_LSCRYPT_AES_CBC_Encrypt(
            cbc_iv, data + 4 + aes_offset, 16, OUTPUT + aes_offset, &length); // data数据开始加密
        // Inver_Data(OUTPUT + aes_offset, 16); // SDK v2.1后不需要
        for (uint8_t j = 0; j < 4; j++) { // 重装向量
            cbc_iv[j] = ((uint32_t)OUTPUT[12 - 4 * j + aes_offset] << 24)
                        + ((uint32_t)OUTPUT[13 - 4 * j + aes_offset] << 16)
                        + ((uint32_t)OUTPUT[14 - 4 * j + aes_offset] << 8)
                        + OUTPUT[15 - 4 * j + aes_offset]; // 打包MD5到初始化向量
        }
        aes_offset += 16;
    }
    memcpy(data + 4, OUTPUT, data_len);
    free(OUTPUT);
    OUTPUT = NULL;
    HAL_LSCRYPT_DeInit();
    return data_len;
}

/*AES_CBC加密_第二阶段*/
// 传入的数据长度必须是16的整数倍，且不足要补0（data前四位装随机数）
uint16_t Aes_Cbc_encry(uint8_t *data, uint16_t text_len) {
    return Aes_Cbc_encry_core(data, text_len, cbc_key_256);
}

/* client 端AES_CBC加密_第二阶段*/
// 传入的数据长度必须是16的整数倍，且不足要补0（data前四位装随机数）
uint16_t client_Aes_Cbc_encry(uint8_t *data, uint16_t text_len) {
    return Aes_Cbc_encry_core(data, text_len, client_cbc_key_256);
}

/*AES_CBC解密_第二阶段*/
// text_len为明文长度（data前四位是随机数）
static int8_t Aes_Cbc_decry_core(uint8_t *data, uint16_t text_len, uint32_t *cbc_key_256) {
    uint16_t data_len;
    uint8_t count = 0;
    uint16_t aes_count = 0;
    uint8_t temp_rng_data[4];
    uint8_t md5_data[16];
    uint32_t length;

    if (NULL == OUTPUT) {
        OUTPUT = malloc(1200 * sizeof(uint8_t));
    }

    if (text_len % 16 != 0) { // 取16倍数算加密轮数
        data_len = (text_len / 16 + 1) * 16;
    } else {
        data_len = text_len;
    }
    count = data_len / 16; // 加密轮数

    memcpy(temp_rng_data, data, 4); // 取四个随机数
    MD5_Encrpyt(temp_rng_data, md5_data, sizeof(temp_rng_data)); // MD5加密4个随机数
    //	LOG_I("md5 is :");
    //		for(uint8_t i=0 ; i<16 ; i++)
    //		{
    //			printf("%x,",md5_data[i]);
    //		}
    for (uint8_t i = 0; i < 4; i++) {
        cbc_iv[i] = ((uint32_t)md5_data[12 - 4 * i] << 24) + ((uint32_t)md5_data[13 - 4 * i] << 16)
                    + ((uint32_t)md5_data[14 - 4 * i] << 8)
                    + md5_data[15 - 4 * i]; // 打包MD5到初始化向量
    }

    // Inver_Data(data + 4, data_len); // 倒置传入要解密的密文 // SDK v2.1后不需要
    HAL_LSCRYPT_Init();
    HAL_LSCRYPT_AES_Key_Config(cbc_key_256, AES_KEY_256);
    for (uint8_t i = 0; i < count; i++) {
        length = 16;
        HAL_LSCRYPT_AES_CBC_Decrypt(cbc_iv,
                                    data + 4 + aes_count,
                                    16,
                                    OUTPUT + aes_count,
                                    &length); // 从data第七个数据开始解密
        // Inver_Data(data + 4 + aes_count, 16); // SDK v2.1后不需要
        for (uint8_t j = 0; j < 4; j++) { // 重装向量
            cbc_iv[j] = ((uint32_t)data[16 - 4 * j + aes_count] << 24)
                        + ((uint32_t)data[17 - 4 * j + aes_count] << 16)
                        + ((uint32_t)data[18 - 4 * j + aes_count] << 8)
                        + data[19 - 4 * j + aes_count]; // 打包MD5到初始化向量
        }
        aes_count += 16;
    }
    // Inver_Data(OUTPUT, data_len); // SDK v2.1后不需要
    memcpy(data + 4, OUTPUT, text_len);
    free(OUTPUT);
    OUTPUT = NULL;
    HAL_LSCRYPT_DeInit();
    return data_len;
}

/*AES_CBC解密_第二阶段*/
// text_len为明文长度（data前四位是随机数）
int8_t Aes_Cbc_decry(uint8_t *data, uint16_t text_len) {
    return Aes_Cbc_decry_core(data, text_len, cbc_key_256);
}
/* client端 AES_CBC解密_第二阶段*/
// text_len为明文长度（data前四位是随机数）
int8_t client_Aes_Cbc_decry(uint8_t *data, uint16_t text_len) {
    return Aes_Cbc_decry_core(data, text_len, client_cbc_key_256);
}

/*第一阶段AES密钥生成器*/
static void gen_Aes_key_first(uint8_t *MD5_data, uint32_t *aes_key) {
    // clang-format off
    uint8_t mykey[] = {
        0x45, 0x8E, 0xE7, 0x06, 0xC4, 0xDC, 0x27, 0x86,
        0xF8, 0x08, 0x4A, 0x22, 0xF2, 0x3E, 0x07, 0xC2,
    };
    // clang-format on
    memset32(aes_key, 0, 4);
    uint8_t temp[16];
    uint8_t *p1 = local_key;
    uint8_t *p2 = MD5_data;
    uint8_t *p3 = temp;
    for (uint8_t i = 0; i < 16; i++) {
        local_key[i] = mykey[i] ^ (0x11 * i);
    }
    for (uint8_t i = 0; i < 16; i++) {
        *p3++ = (*p1++) ^ (*p2++);
    }
#if Log_Aes
    LOG_I("first_new_key is:");
    for (uint8_t i = 0; i < sizeof(temp); i++) {
        printf("%02x", temp[i]);
    }
    printf("\r\n");
#endif
    for (uint8_t i = 0; i < 4; i++) {
        aes_key[i] = ((uint32_t)temp[12 - 4 * i] << 24) + ((uint32_t)temp[13 - 4 * i] << 16)
                     + ((uint32_t)temp[14 - 4 * i] << 8) + temp[15 - 4 * i];
    }
}

/*第一阶段AES密钥生成*/
void Pdc_Aes_key_first(uint8_t *MD5_data) {
    gen_Aes_key_first(MD5_data, aes_key);
}

/* client端第一阶段AES密钥生成*/
void client_Pdc_Aes_key_first(uint8_t *MD5_data) {
    gen_Aes_key_first(MD5_data, client_aes_key);
}

/*第二阶段AES密钥生成器*/
static void gen_Pdc_Aes_key_second(uint8_t *aes_key_Data, uint32_t *cbc_key_256) {
    uint8_t temp[32];
    memset(temp, 0, 32);
    memcpy(temp, aes_key_Data, 32);
#if Log_Aes
    LOG_I("second_new_key is:");
    for (uint8_t i = 0; i < 32; i++) {
        printf("%02x", temp[i]);
    }
    printf("\r\n");
#endif
    for (uint8_t i = 0; i < 8; i++) {
        cbc_key_256[i] = ((uint32_t)temp[28 - 4 * i] << 24) + ((uint32_t)temp[29 - 4 * i] << 16)
                         + ((uint32_t)temp[30 - 4 * i] << 8) + temp[31 - 4 * i];
    }
}

/*第二阶段AES密钥生成*/
void Pdc_Aes_key_second(uint8_t *aes_key_Data) {
    gen_Pdc_Aes_key_second(aes_key_Data, cbc_key_256);
    // LOG_I("cbc_key_256 is:");
    // LOG_HEX(cbc_key_256, 32);
}

/* client端第二阶段AES密钥生成*/
void client_Pdc_Aes_key_second(uint8_t *aes_key_Data) {
    gen_Pdc_Aes_key_second(aes_key_Data, client_cbc_key_256);
    LOG_I("client_cbc_key_256 is:");
    LOG_HEX(client_cbc_key_256, 32);
}

extern uint32_t get_trng_value(void);
/*随机数生成*/
void Pdc_Rng_data(uint8_t *dst) {
    uint8_t RNG_data[4];
    uint32_t rander = get_trng_value();

    RNG_data[0] = rander >> 24;
    RNG_data[1] = (rander >> 16) & 0xff;
    RNG_data[2] = (rander >> 8) & 0xff;
    RNG_data[3] = rander & 0xff;

    memcpy(dst, RNG_data, 4);
}

/* 为uECC提供的随机数生成函数 */
int my_rng_function(uint8_t *dest, unsigned size) {
    int i = 0;
    for (i = 0; i < size; i++) {
        dest[i] = get_trng_value() & 0xff;
    }
    return 1; // 0表示失败，1表示成功
}

/*计算校验和*/
uint16_t Crc_Sum(uint8_t *data, uint16_t len) {
    uint16_t sum = 0;
    uint8_t *ptr = data;
    for (uint16_t i = 0; i < len; i++) {
        sum += *(ptr + i);
    }
    return sum;
}

/** 检查校验和，最后两字节为sum值 */
bool check_sum(uint8_t *data, uint16_t len) {
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    sum1 = Crc_Sum(data, len - 2);
    sum2 = ((uint16_t)data[len - 2] << 8) + data[len - 1];
    return sum1 == sum2;
}

/*MD5加密*/
void MD5_Encrpyt(const uint8_t *src, uint8_t *dst, uint16_t len) {
    //	uint8_t temp[len];
    uint8_t *temp = NULL;
    temp = malloc(len * sizeof(uint8_t));
    if (temp == NULL) {
        LOG_E("malloc fail!");
        return;
    }
    memcpy(temp, src, len);
    unsigned char decrypt[16] = {0};
    MD5_CTX md5c;
    MD5Init(&md5c); //???
    MD5Update(&md5c, (unsigned char *)temp, len);
    MD5Final(&md5c, decrypt);
    memcpy(dst, (uint8_t *)decrypt, 16);
    free(temp);
}

/*鉴权请求数据包*/
uint8_t *identify_request_pack(uint8_t *dst) {
    uint8_t pack_data[10];
    pack_data[0] = '*';
    pack_data[1] = '*';
    pack_data[2] = identify_request;
    pack_data[3] = 0x04;
    Pdc_Rng_data(pack_data + 4);
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 6; i++)
        sum += pack_data[2 + i];
    pack_data[8] = sum >> 8;
    pack_data[9] = sum & 0xff;
    memcpy(dst, pack_data, 10);
    return dst;
}

/*鉴权请求回应数据包*/
void identify_verify_pack(uint8_t *pack_data, uint8_t *md5) {
    pack_data[0] = '*';
    pack_data[1] = '*';
    pack_data[2] = identify_verity;
    pack_data[3] = 0x04;
    memcpy(pack_data + 4, md5, 4); // 取出MD5数据 (9- 12位)
    uint16_t sum = Crc_Sum((uint8_t *)&pack_data[2], 6);
    pack_data[8] = sum >> 8;
    pack_data[9] = sum & 0xff;
    return;
}

/*鉴权结果数据包*/
uint8_t *identify_rst_pack(uint8_t *dst, uint8_t rst) {
    uint8_t pack_data[10];
    pack_data[0] = '*';
    pack_data[1] = '*';
    pack_data[2] = identify_rst;
    pack_data[3] = 0x01;
    pack_data[4] = rst;
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 3; i++)
        sum += pack_data[2 + i];
    pack_data[5] = sum >> 8;
    pack_data[6] = sum & 0xff;
    memcpy(dst, pack_data, 10);
    return dst;
}

/*检查鉴权回应*/
uint8_t identify_rst_check(uint8_t *rst_data, uint8_t *local_MD5) {
    uint8_t res = 0;
    if (rst_data[0] == '*' && rst_data[1] == '*' && rst_data[2] == identify_verity) {
        uint16_t sum1, sum2 = 0;
        sum1 = Crc_Sum(&rst_data[2], 6);
        sum2 = ((uint16_t)rst_data[8] << 8) + rst_data[9];
        if (sum1 == sum2) {
            if (local_MD5[0] == rst_data[4] && local_MD5[1] == rst_data[5]
                && local_MD5[2] == rst_data[6] && local_MD5[3] == rst_data[7]) {
                res = 1;
            }
        }
    }
    if (res != 1) {
        identify_rst_pack(rst_data, IDENT_FAIL);
        BleSendBytes(rst_data, 7);
    }
    return res;
}

/*ECDH密钥协商请求数据包*/
uint8_t *ECDH_key_request_pack(uint8_t *dst, uint16_t len) {
    uint8_t *pack_data;
    pack_data = malloc(len * sizeof(uint8_t));
    if (pack_data == NULL) {
        LOG_I("malloc fail!");
        return 0;
    }
    memcpy(pack_data, dst, len);
    pack_data[0] = '*';
    pack_data[1] = '*';
    pack_data[2] = ECDH_key_request;
    pack_data[3] = 0x80;
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 130; i++)
        sum += pack_data[2 + i];
    pack_data[len - 2] = sum >> 8;
    pack_data[len - 1] = sum & 0xff;
    memcpy(dst, pack_data, len);
    free(pack_data);
    return dst;
}

/*ECDH密钥协商结果回应数据包*/
uint8_t *ECDH_key_rsp_pack(uint8_t *dst, uint8_t rst) {
    uint8_t pack_data[10];
    pack_data[0] = '*';
    pack_data[1] = '*';
    pack_data[2] = ECDH_key_rsp;
    pack_data[3] = 0x01;
    pack_data[4] = rst;
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 3; i++)
        sum += pack_data[2 + i];
    pack_data[5] = sum >> 8;
    pack_data[6] = sum & 0xff;
    memcpy(dst, pack_data, 7);
    return dst;
}

/* 客户端处理ECDH密钥请求并发送响应 */
bool client_ECDH_key_req_check(uint8_t *req_data, uint8_t *local_MD5) {
    uint8_t sign_data[128];
    memcpy(sign_data, req_data + 4, 128); // 获取IOT签名内容

    // 验证数据包头和消息类型
    if (req_data[0] == '*' && req_data[1] == '*' && req_data[2] == ECDH_key_request) {
        // 验证校验和
        uint16_t sum1, sum2 = 0;
        sum1 = Crc_Sum(&req_data[2], 130);
        sum2 = ((uint16_t)req_data[132] << 8) + req_data[133];

        if (sum1 == sum2) {
            if (OS_verify_test(sign_data, local_MD5)) {
                return true;
            }
        } else {
            LOG_I("sum is ERROR!");
        }
    }
    return false;
}

/*ECDH鉴权检查*/
uint8_t ECDH_key_rst_check(uint8_t *rst_data, uint8_t *local_MD5) {
    uint8_t sign_data[128];
    memcpy(sign_data, rst_data + 4, 128); // 获取OS签名内容
    uint8_t res = 0;
    if (rst_data[0] == '*' && rst_data[1] == '*' && rst_data[2] == ECDH_key_check) {
        uint16_t sum1, sum2 = 0;
        sum1 = Crc_Sum(&rst_data[2], 130);
        sum2 = ((uint16_t)rst_data[132] << 8) + rst_data[133];
        if (sum1 == sum2) {
            if (IOT_verify_test(sign_data, local_MD5)) {
                res = 1;
            }
        } else {
            LOG_I("sum is ERROR!");
        }
    }
    if (res != 1) {
        uint8_t fail_rst[7];
        ECDH_key_rsp_pack(fail_rst, IDENT_FAIL);
        AES_BleSendBytes(fail_rst, 7, false);
    }
    return res;
}

/*定时器测试*/
// TIM_HandleTypeDef TimHandle;
// uint32_t timers=0;
// void Basic_Timer_Cfg(void)
//{
//     io_cfg_output(PA00);
//     io_write_pin(PA00,0);

//    TimHandle.Instance           = LSGPTIMA;
//    TimHandle.Init.Prescaler     = SDK_HCLK_MHZ-1;
//    TimHandle.Init.Period        = 1000-1;//周期为1ms
//    TimHandle.Init.ClockDivision = 0;
//    TimHandle.Init.CounterMode   = TIM_COUNTERMODE_UP;
//		HAL_TIM_Init(&TimHandle);
//		HAL_TIM_Base_Start_IT(&TimHandle);
//}
// void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
//{
//    if(htim->Instance == LSGPTIMA)
//    {
//        timers++;
//    }
//}
// void time_test(void)
//{
//	Basic_Timer_Cfg();
//	while(1)
//	{
//		if(timers%1000 == 0)
//		{
//			LOG_I("OK");
//		}
//	}
//}

/*ECC验签*/
int Verify_test(uint8_t *text,
                uint16_t text_len,
                uint8_t *MD5_Data,
                uint8_t *verify_pub_key,
                uint8_t *sign_key) {
    uint8_t all_data[80];
    memcpy(all_data, text, 64);
    memcpy(all_data + 64, MD5_Data, 16);
    struct tc_sha256_state_struct L;
    tc_sha256_init(&L); // sha256计算哈希值
    tc_sha256_update(&L, (const uint8_t *)all_data, 80);
    // tc_sha256_update(&L,(const uint8_t*)MD5_Data,16);
    // tc_sha256_update(&s,(const uint8_t*)OS_pub_key,sizeof(OS_pub_key));
    tc_sha256_final(cipherbuffer_sha256.data, &L);

    if (!uECC_verify(verify_pub_key,
                     cipherbuffer_sha256.data,
                     sizeof(cipherbuffer_sha256.data),
                     sign_key,
                     uECC_secp256r1())) // 验签；
    {
        return 0;
    } else {
        return 1;
    }
}

/*ECC签名*/
int Sign_test(
    uint8_t *prv_key, uint8_t *MD5_Data, uint8_t *text, uint8_t text_len, uint8_t *sign_buf) {
    uint8_t all_data[80];
    memcpy(all_data, text, 64);
    memcpy(all_data + 64, MD5_Data, 16);
    struct tc_sha256_state_struct S;
    tc_sha256_init(&S); // sha256计算哈希值
    tc_sha256_update(&S, (const uint8_t *)all_data, 80);
    // tc_sha256_update(&S, (const uint8_t *)MD5_Data, 16);

    // tc_sha256_update(&s, (const uint8_t *)OS_pub_key, sizeof(OS_pub_key));
    tc_sha256_final(cipherbuffer_sha256.data, &S);

    if (!uECC_sign(prv_key,
                   cipherbuffer_sha256.data,
                   sizeof(cipherbuffer_sha256.data),
                   sign_buf,
                   uECC_secp256r1())) // 签名；
    {
        // LOG_I("sign is fail!");
        return 0;
    } else {
        // LOG_I("sign is success!");
        return 1;
    }
}

void OS_sign_test(uint8_t *sign_cache, uint8_t *rst, uint8_t *MD5) // 手机签名
{
    for (uint8_t i = 0; i < 32; i++) {
        OS_verify_prv_key[i] = OS_verify_prv_key[i] ^ (0x11 + i);
    }

    if (!Sign_test(OS_verify_prv_key, MD5, OS_swap_pub_key, sizeof(OS_swap_pub_key), sign_cache)) {
        printf("OS_sign_generate fail!");
    } else {
        memcpy(rst, OS_swap_pub_key, 64);
        memcpy(rst + 64, sign_cache, 64);
        printf("OS_sign_generate success!\r\n");
        // printf("send_rst is:\r\n");
        // for (uint8_t i = 0; i < 128; i++) {
        //     printf("%x,", rst[i]);
        // }
        // printf("sum is %x,", Crc_Sum(rst, 128) + 0x85);
    }
}

// client端签名，复用IOT server端在开机时生成的公私钥对
bool client_OS_sign_test(uint8_t *sign_cache, uint8_t *rst, uint8_t *MD5) {
    // for (uint8_t i = 0; i < 32; i++) {
    //     OS_verify_prv_key[i] = OS_verify_prv_key[i] ^ (0x11 + i);
    // }

    if (!Sign_test(
            OS_verify_prv_key, MD5, IOT_swap_pub_key, sizeof(IOT_swap_pub_key), sign_cache)) {
        printf("OS_sign_generate fail!");
        return false;
    } else {
        memcpy(rst, IOT_swap_pub_key, 64);
        memcpy(rst + 64, sign_cache, 64);
        // printf("OS_sign_generate success!\r\n");
        // printf("send_rst is:\r\n");
        // for (uint8_t i = 0; i < 128; i++) {
        //     printf("%x,", rst[i]);
        // }
        // printf("sum is %x,", Crc_Sum(rst, 128) + 0x85);
    }
    return true;
}

bool OS_verify_test(uint8_t *rst, uint8_t *MD5) { // 手机验签
    uint8_t result_y[32];
    uint8_t rst_plaintext[64];
    memcpy(rst_plaintext, rst, 64);
#if Log_Aes
    printf("APP swap pub key is:\r\n");
    for (uint8_t i = 0; i < sizeof(rst_plaintext); i++) {
        printf("%02x,", rst_plaintext[i]);
    }
    printf("\r\n");
#endif

    if (!Verify_test(rst_plaintext, sizeof(rst_plaintext), MD5, OS_verify_pub_key, rst + 64)) {
        LOG_I("OS_verify_test fail!");
        return false;
    } else {
        LOG_I("OS_verify_test success!\r\n");
        // return true;
    }

    my_Inver_Data(IOT_swap_prv_key, sizeof(IOT_swap_prv_key));
    my_Inver_Data(rst_plaintext, sizeof(rst_plaintext));

    // 硬件加速API
    my_ecc_calc_start(
        IOT_swap_prv_key, rst_plaintext + 32, rst_plaintext, cipherbuffer_sha256.data, result_y);

    my_Inver_Data(cipherbuffer_sha256.data, sizeof(cipherbuffer_sha256.data));
    client_Pdc_Aes_key_second(cipherbuffer_sha256.data);
    return true;
}

void IOT_sign_test(uint8_t *sign_cache, uint8_t *rst, uint8_t *MD5) // IOT签名
{
    // for (uint8_t i = 0; i < 32; i++) {
    //     IOT_verify_prv_key[i] = IOT_verify_prv_key[i] ^ (0x11 + i);
    // }

    if (!Sign_test(IOT_verify_prv_key,
                   MD5,
                   IOT_swap_pub_key,
                   sizeof(IOT_swap_pub_key),
                   sign_cache)) { // iot签名；
        LOG_I("IOT_sign_generate fail!");
    } else {
        // printf("IOT_sign_generate success!\r\n");
        memcpy(rst, IOT_swap_pub_key, 64);
        memcpy(rst + 64, sign_cache, 64);
#if Log_Aes
        printf("Iot Swap Pub key data is:\r\n");
        for (uint8_t i = 0; i < sizeof(IOT_swap_pub_key); i++) {
            printf("%02x", IOT_swap_pub_key[i]);
        }
        printf("\r\n");

        printf("Iot Swap Prv key data is:\r\n");
        for (uint8_t i = 0; i < sizeof(IOT_swap_prv_key); i++) {
            printf("%02x", IOT_swap_prv_key[i]);
        }
        printf("\r\n");
        // printf("sign data is:\r\n");
        // for (uint8_t i = 0; i < 64; i++) {
        //     printf("%02x", sign_cache[i]);
        // }
        // printf("\r\n");
#endif
    }

    // OS_verify_test(rst, MD5);
}

int IOT_verify_test(uint8_t *rst, uint8_t *MD5) // IOT验签
{
    uint8_t result_y[32];
    uint8_t rst_plaintext[64];
    memcpy(rst_plaintext, rst, 64); // OS的公钥内容（签名内容）

    printf("APP Swap Pub key data is:\r\n");
    for (uint8_t i = 0; i < sizeof(rst_plaintext); i++) {
        printf("%02x", rst_plaintext[i]);
    }
    printf("\r\n");
    // for (uint8_t i = 0; i < 64; i++) {
    //     OS_verify_pub_key[i] = OS_verify_pub_key[i] ^ (0x11 + i);
    // }
    if (!Verify_test(
            rst_plaintext, sizeof(rst_plaintext), MD5, IOT_verify_pub_key, rst + 64)) // iot验签；
    {
        LOG_I("IOT_verify_test fail!");
        return 0;
    } else {
        // LOG_I("IOT_verify_test success!");
    }

    my_Inver_Data(IOT_swap_prv_key, sizeof(IOT_swap_prv_key));
    my_Inver_Data(rst_plaintext, sizeof(rst_plaintext));
    // 硬件加速API
    my_ecc_calc_start(
        IOT_swap_prv_key, rst_plaintext + 32, rst_plaintext, cipherbuffer_sha256.data, result_y);
    my_Inver_Data(cipherbuffer_sha256.data, sizeof(cipherbuffer_sha256.data));
    Pdc_Aes_key_second(cipherbuffer_sha256.data);
    return 1;
}

/* 填充随机数生成发生器 */
void Ecc_fill_rng_function(void) {
    uECC_set_rng(my_rng_function);
}

void Ecc_Make_key(uint8_t *pack_ecdh_sign_buf,
                  uint8_t *pack_buf,
                  uint8_t *MD5_all,
                  uint8_t *md5_sec) {
    if (!uECC_make_key(IOT_swap_pub_key, IOT_swap_prv_key, uECC_secp256r1())) {
        LOG_I("uECC_make_key failed!");
    } else {
        // LOG_I("uECC_make_key succeeded!");
    }

    identify_request_pack(pack_buf); // 随机数,MD5
    memcpy(md5_sec, &pack_buf[4], 4); // 随机数
    uint8_t temp[4];
    for (uint8_t i = 0; i < 4; i++) {
        temp[i] = md5_sec[3 - i]; // 倒序传入
    }
    uint8_t MD5_all_temp[16];
    MD5_Encrpyt(temp, MD5_all_temp, 4); /*生成本地MD5*/
#if Log_Aes
    printf("Request 01 MD5 data is:\r\n");
    for (uint8_t i = 0; i < sizeof(MD5_all_temp); i++) {
        printf("%02x,", MD5_all_temp[i]);
    }
    printf("\r\n");
#endif
    memcpy(MD5_all, MD5_all_temp, 16);
    memcpy(md5_sec, &MD5_all_temp[8], 4); /*取本地MD5第9-12位*/

    uint8_t sign_buf[64] = {0}; // 装签名值
    IOT_sign_test(sign_buf, pack_ecdh_sign_buf + 4, MD5_all_temp); // 前面空4个字节打包格式
    ECDH_key_request_pack(pack_ecdh_sign_buf, 134);
    Xmoderm_ota = false;
}

static const char *test_str_pub
    = "29bc6d0d5caca7dfc13f66cc7859233dcd8cb29fb8944b8dfd8a50da6082e105410941985428afcf5e55757960ef"
      "ef4f7ceb357c3d29545082d8d15e2164d31a";

static const char *test_str_pri
    = "309e0d4c71b10a10491b0ce5c5b4619e299d70209ebefa6c0acecaae64417583";

static void trans_hex(const char *src, uint8_t *output, uint16_t len) {
    char *ptr = (char *)src;
    for (uint8_t i = 0; i < len; i += 2) {
        // printf("%02x,", (ptr[i] - 0x30) < 0x0a ? (ptr[i] - 0x30) : (ptr[i] - 87));
        output[i / 2] = (((ptr[i] - 0x30) < 0x0a ? (ptr[i] - 0x30) : (ptr[i] - 87)) << 4)
                        + ((ptr[i + 1] - 0x30) < 0x0a ? (ptr[i + 1] - 0x30) : (ptr[i + 1] - 87));
    }
}

// static uint8_t IOT_TEST_PRV_KEY[]
//     = {0x2f, 0xad, 0xfe, 0x4d, 0x4c, 0x0d, 0x98, 0x43, 0xff, 0xe1, 0xe5,
//        0xe8, 0xea, 0x26, 0xd1, 0xe0, 0xed, 0x1b, 0x2f, 0x15, 0xdc, 0xbf,
//        0x18, 0x92, 0xbf, 0x64, 0xb1, 0xc6, 0xa4, 0xf2, 0xd5, 0x78};
// static uint8_t APP_TEST_PUB_KEY[]
//     = {0x5e, 0x14, 0x8a, 0x84, 0xa2, 0x16, 0x33, 0x17, 0xbd, 0x2c, 0xe8, 0xca, 0xe3,
//        0xc1, 0x70, 0xce, 0x13, 0xd3, 0x7d, 0x80, 0x33, 0x0c, 0xc3, 0xd4, 0x46, 0x8e,
//        0xcc, 0xbf, 0x1f, 0xaa, 0x91, 0x67, 0x90, 0x50, 0x5e, 0xb3, 0x13, 0xa4, 0xba,
//        0x53, 0x0e, 0x41, 0x30, 0xd4, 0xb0, 0x39, 0x84, 0xe0, 0xa9, 0xc2, 0xe5, 0x02,
//        0xac, 0x01, 0x5c, 0x97, 0x34, 0x86, 0x8b, 0xb6, 0xf1, 0xbc, 0xf5, 0x4b};
void IOT_APP_TEST(void) {
    uint8_t APP_TEST_PUB_KEY[64] = {0};
    uint8_t IOT_TEST_PRV_KEY[32] = {0};
    trans_hex(test_str_pub, APP_TEST_PUB_KEY, strlen(test_str_pub));
    trans_hex(test_str_pri, IOT_TEST_PRV_KEY, strlen(test_str_pri));
    // printf("trans result is:\r\n");
    // for (uint8_t i = 0; i < sizeof(test_hex); i++) {
    //     printf("%02x,", test_hex[i]);
    // }
    // uint8_t result_y[32] = {0};
    uint8_t result_x[32] = {0};
    // my_Inver_Data(IOT_TEST_PRV_KEY, sizeof(IOT_TEST_PRV_KEY));
    // my_Inver_Data(APP_TEST_PUB_KEY, sizeof(APP_TEST_PUB_KEY));

    // 软件加速API
    uECC_shared_secret(APP_TEST_PUB_KEY, IOT_TEST_PRV_KEY, result_x, uECC_secp256r1());

    // 硬件加速API
    // my_ecc_calc_start(
    //     IOT_TEST_PRV_KEY,
    //     APP_TEST_PUB_KEY + 32,
    //     APP_TEST_PUB_KEY,
    //     result_x,
    //     result_y
    // );

    // my_Inver_Data(result_x, sizeof(result_x));

    printf("\r\nIOT_APP test result is:\r\n");
    for (uint8_t i = 0; i < sizeof(result_x); i++) {
        printf("%02x", result_x[i]);
    }
    printf("\r\n");
}
