/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/10/10
Description: 蓝牙通道鉴权包、AES-ECB数据加密、AES-ECB数据解密、秘钥生成、ECDH密钥协商、ECDSA签名、ECDSA验签、AES-CBC数据加密、AES-CBC数据解密
Version: V1.00
Function List: none
***********************************************************/
#include "ble_crypt.h"
#include "esp_log.h"
#include "esp_random.h"
#include "utils.h"
#include "iot_rsa.h"
#include "iot_partition.h"
#include "comm_define.h"

static const char *TAG = "[iot_ble_encrypt]";

#define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3) \
    ((uint32_t)((uint32_t)(((uint32_t)Byte0) & 0x00FF) + ((uint32_t)(((uint32_t)Byte1) & 0x00FF) << 8) + ((uint32_t)(((uint32_t)Byte2) & 0x00FF) << 16) + ((uint32_t)(((uint32_t)Byte3) & 0x00FF) << 24)))

#define BREAK_UINT32(var, ByteNum) \
    (uint8_t)((uint32_t)(((var) >> ((ByteNum) * 8)) & 0x00FF))

///////////////////////////////ble server device //////////////////////////////////////////////

// 本地原始根秘钥
// 处理后的aes秘钥
const uint8_t partition_key[16] = {0x44, 0x96, 0xd4, 0x2c, 0xa1, 0xa0, 0x70, 0xc8, 0x31, 0xd8, 0xb1, 0xc0, 0x5f, 0x8a, 0x98, 0x44};
// uint8_t local_aes_key[16] = {0x45,0x9f, 0xc5, 0x35, 0x80, 0x89, 0x41, 0xf1, 0x70, 0x91, 0xe0, 0x99, 0x3e, 0xe3, 0xe9, 0x3d};
uint32_t random_num = 0;
USE_EXT_RAM_BSS uint8_t md5_authen_value[16] = {0}; // MD5值
USE_EXT_RAM_BSS uint8_t generate_key[16] = {0};     // 新key
// 通信AES秘钥
uint8_t *com_aes_key = NULL; // 协议文档约定双方公用根AES秘钥
// 文件AES秘钥
uint8_t *file_aes_key = NULL;

const uint8_t sign_private_key[] = {0x85, 0xC3, 0x2C, 0x6B, 0xCC, 0xC5, 0x06, 0xF5, 0xED, 0x08, 0x18, 0x26, 0xAA, 0x61, 0x21, 0x1F,
                                    0x33, 0x9C, 0xF1, 0x19, 0xB5, 0x56, 0x7F, 0xF6, 0x4C, 0x7B, 0xF9, 0x2B, 0x19, 0x81, 0x9C, 0xF3};

// uint8_t mobile_sign_public_key[] = {0xA7, 0x3A, 0xBF, 0x5D, 0x22, 0x32, 0xC8, 0xC1, 0xC7, 0x2E, 0x68, 0x30, 0x43, 0x43, 0xC2, 0x72,
//                                     0x49, 0x5E, 0x3A, 0x8F, 0xD6, 0xF3, 0x0E, 0xA9, 0x6D, 0xE2, 0xF4, 0xB3, 0xCE, 0x60, 0xB2, 0x51,
//                                     0xEE, 0x21, 0xAC, 0x66, 0x7C, 0xF8, 0xA7, 0x1E, 0x18, 0xB4, 0x6B, 0x66, 0x4E, 0xAE, 0xFF, 0xE3,
//                                     0xC4, 0x89, 0xF2, 0x4F, 0x69, 0x5B, 0x64, 0x11, 0xDB, 0x7E, 0x22, 0xCC, 0xC8, 0x5A, 0x85, 0x94};

// uint8_t mobile_sign_private_key[] = {0x4F, 0x19, 0xA1, 0x6E, 0x3E, 0x87, 0xBD, 0xD9, 0xBD, 0x24, 0xD3, 0xE5, 0x49, 0x5B, 0x88, 0x04,
//                                     0x15, 0x11, 0x94, 0x3C, 0xBC, 0x8B, 0x96, 0x9A, 0xDE, 0x96, 0x41, 0xD0, 0xF5, 0x6A, 0xF3, 0x37};

const uint8_t verify_public_key[] = {0x3F, 0x29, 0xE1, 0xB8, 0xB2, 0x9D, 0x84, 0x22, 0xBB, 0xB0, 0xE0, 0xF0, 0x90, 0x00, 0xCF, 0x2E,
                                     0xE2, 0x93, 0x1D, 0xA1, 0x3C, 0xDA, 0xC8, 0x12, 0x9F, 0x8C, 0x09, 0xDE, 0xAC, 0x07, 0xF5, 0x19,
                                     0xB5, 0xAC, 0x55, 0x09, 0xCA, 0x3F, 0x3F, 0x59, 0xB2, 0x34, 0xB7, 0x77, 0x7A, 0x23, 0x1A, 0xE5,
                                     0x95, 0xCA, 0x5A, 0xA1, 0x74, 0x5F, 0xD7, 0xA6, 0x2B, 0xC4, 0xD0, 0x20, 0x37, 0x11, 0x7D, 0xD1};

USE_EXT_RAM_BSS uint8_t ecc_key_agreement_data[128] = {0};

mbedtls_ecp_group *cli_grp = NULL;
mbedtls_mpi *cli_pri = NULL;
mbedtls_ctr_drbg_context *cli_ctr_drbg = NULL;

mbedtls_ecp_group *srv_grp = NULL;
mbedtls_mpi *srv_pri = NULL;
mbedtls_ctr_drbg_context *srv_ctr_drbg = NULL;

USE_EXT_RAM_BSS uint8_t aes_cbc_key[32] = {0}; // AES-CBC密钥

enum
{
    AUTHEN_PASS = 0x00, // 鉴权通过
    AUTHEN_FAIL = 0x01, // 鉴权失败
};

static void client_public_gen(uint8_t *public_key);
static void ecdh_key_agreement_data(uint8_t *public_key, uint8_t *out);
void iot_communciate_aes_cbc128_decrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv);
void iot_communciate_aes_cbc128_encrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv);

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    // mbedtls_printf("%s", info);
    // for (int i = 0; i < len; i++) {
    //     mbedtls_printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ",
    //                     buf[i], i == len - 1 ? "\n":"");
    // }
}
/* Mock the esp-random to return 0 for easier result checking */
// uint32_t esp_random(void)
// {
// //  if (random_mock == -1)
//   {
//     /* fall back to default "port/unix" rand (if other tests are launched with IP_NAPT ON) */
//     return rand();
//   }
// //  return random_mock;
// }

static int entropy_source(void *data, uint8_t *output, size_t len, size_t *olen)
{
    uint32_t seed;
    size_t offset = 0;
    size_t sum = len / 4;

    for (size_t i = 0; i < sum; i++)
    {
        seed = esp_random(); // 产生一个32位随机数
        memcpy(output + offset, &seed, 4);
        offset += 4;
    }
    if (len % 4 != 0)
    {
        seed = esp_random(); // 产生一个32位随机数
        memcpy(output + offset, &seed, 4);
        offset += len % 4;
    }
    *olen = offset;
    return 0;
}

// 计算MD5秘钥
static void md5_calculate_key(uint32_t value, uint8_t *out_md5)
{
    mbedtls_md5_context md5_ctx;
    uint8_t encrypt[4] = {0};
    // uint16_t sum = 0;
    encrypt[0] = (uint8_t)value;
    encrypt[1] = (uint8_t)(value >> 8);
    encrypt[2] = (uint8_t)(value >> 16);
    encrypt[3] = (uint8_t)(value >> 24);
    mbedtls_md5_init(&md5_ctx);
    //    mbedtls_md5_starts_ret(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);

    mbedtls_md5_update(&md5_ctx, encrypt, sizeof(encrypt));
    mbedtls_md5_finish(&md5_ctx, out_md5);
    mbedtls_md5_free(&md5_ctx);

    // printf("generate md5:\n");
    // for(int loop = 0; loop < 16; loop++)
    // {
    //     if(loop%16 == 0)
    //         printf("\n");
    //     printf("%02x ", out_md5[loop]);
    // }
    // sum = 6+out_md5[8]+out_md5[9]+out_md5[10]+out_md5[11];
    // printf("\nwould like respond:\n0x2a 0x2a 0x02 0x04 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", out_md5[8], out_md5[9],out_md5[10],out_md5[11],(uint8_t)(sum>>8), (uint8_t)sum);
    // printf("\n");
}

// 刷新随机数MD5值
static void refresh_random_number_md5(void)
{
    mbedtls_md5_context md5_ctx;
    uint8_t encrypt[4] = {0};
    random_num = esp_random(); // 产生一个32位随机数

    encrypt[0] = (uint8_t)random_num;
    encrypt[1] = (uint8_t)(random_num >> 8);
    encrypt[2] = (uint8_t)(random_num >> 16);
    encrypt[3] = (uint8_t)(random_num >> 24);

    mbedtls_md5_init(&md5_ctx); // 初始化MD5
    //	  mbedtls_md5_starts_ret(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);
    mbedtls_md5_update(&md5_ctx, encrypt, sizeof(encrypt));
    mbedtls_md5_finish(&md5_ctx, md5_authen_value); // 生成MD5值
    mbedtls_md5_free(&md5_ctx);
}

// 刷新密钥协商数据
static void refresh_key_agreement(void)
{
    uint8_t pub[64];                                      // 存放公钥
    client_public_gen(pub);                               // 生成协商密钥
    ecdh_key_agreement_data(pub, ecc_key_agreement_data); // 组装密钥协商数据
    dump_buf("windy:ecc_key_agreement_data", ecc_key_agreement_data, sizeof(ecc_key_agreement_data));
}

// 刷新加密参数
void refresh_encrypt_param(void)
{
    refresh_random_number_md5();
    refresh_key_agreement();
    //    ESP_LOGI(TAG, "refresh encrypt param,random:0x%x", random_num);
    ESP_LOGI(TAG, "refresh encrypt param,random:%lu", random_num);
}

// 生成AES秘钥
static void generate_aes_key(void)
{
    // printf("AES key is:\n");
    for (uint8_t i = 0; i < 16; i++)
    {
        generate_key[i] = com_aes_key[i] ^ md5_authen_value[i]; // 异或运算
        // printf("0x%02x ", generate_key[i]);
    }
}

// 获取包类型的长度
static uint16_t get_pack_format_len(uint8_t type)
{
    uint16_t len = 0;
    switch (type)
    {
    case AUTHEN_REQUEST:
        len = 10;
        break;

    case AUTHEN_RESOPN:
        len = 10;
        break;

    case AUTHEN_RESLUT:
        len = 7;
        break;

    case ECDH_AGRESSMENT_REQUEST:
        len = 134;
        break;

    case ECDH_AGRESSMENT_RESPOND:
        len = 134;
        break;

    case ECDH_AGRESSMENT_RESULT:
        len = 7;
        break;

    default:
        break;
    }
    return len;
}

// 计算累加和
static uint16_t calcute_sum(uint8_t *data, uint16_t len)
{
    uint16_t sum = 0;
    uint8_t *ptr = data;
    for (uint16_t i = 0; i < len; i++)
    {
        sum += *(ptr + i);
    }
    return sum;
}

// 判断包格式
/*

retrun:
1-帧头格式正确，CRC正确；
0-错误
*/
static uint8_t check_pack_format(uint8_t *data, uint16_t len, uint8_t type)
{
    uint8_t ret = 0;
    uint16_t format_len = 0;
    uint16_t sum = 0;
    format_len = get_pack_format_len(type);
    if (format_len != len)
    {
        printf("invalid len\n");
        return ret;
    }
    if (data[0] != '*' || data[1] != '*' || data[2] != type)
    {
        printf("invalid preframe\n");
        return ret;
    }
    format_len = calcute_sum(data + 2, len - 4);
    sum = data[len - 2];
    sum = sum << 8;
    sum += data[len - 1];
    if (format_len != sum)
    {
        printf("invalid sum\n");
        return ret;
    }
    ret = 1;
    return ret;
}

/*打包鉴定包
组合发送包
out:*pack

*/
static uint8_t iot_ble_authen_pack(uint8_t type, uint8_t *data, uint8_t len, uint8_t *pack)
{
    uint16_t sum = 0;
    pack[0] = '*';
    pack[1] = '*';
    pack[2] = type;
    pack[3] = len;
    memcpy(pack + 4, data, len);
    sum = calcute_sum(&pack[2], len + 2);
    pack[len + 4] = (uint8_t)(sum >> 8);
    pack[len + 5] = (uint8_t)sum;
    return (len + 6);
}

/*鉴权请求包

TX  01
OUT:*data

*/
uint8_t iot_ble_authen_request(uint8_t *data)
{
    uint8_t len;
    // uint32_t ramdom;
    uint8_t encrypt[4] = {0};

    // ramdom = esp_random(); //产生一个32位随机数
    // printf("ramdom:0x%x\n", ramdom);
    encrypt[0] = (uint8_t)(random_num >> 24);
    encrypt[1] = (uint8_t)(random_num >> 16);
    encrypt[2] = (uint8_t)(random_num >> 8);
    encrypt[3] = (uint8_t)random_num;
    len = iot_ble_authen_pack(AUTHEN_REQUEST, encrypt, sizeof(encrypt), data);
    // md5_calculate_key(ramdom, md5_authen_value); //计算md5秘钥
    generate_aes_key(); // 生成key

    return len;
}

/*
鉴定结果回应
TX:2A 2A 03/06

组合发送包

*/
uint8_t iot_ble_authen_result(uint8_t res, uint8_t *data, uint8_t type)
{
    uint8_t value = res;
    return iot_ble_authen_pack(type, &value, 1, data);
}

/*对鉴定请求回应*/
uint8_t iot_ble_authen_respond(uint8_t *data, uint8_t len, uint8_t *reslt_resp, uint8_t *rlen)
{
    uint8_t res = 0;
    uint16_t sum1, sum2 = 0;
    uint32_t ramdom = 0;
    if (data[0] == '*' && data[1] == '*' && data[2] == AUTHEN_REQUEST)
    {
        sum1 = calcute_sum(&data[2], 6); // 计算crc
        sum2 = data[8];
        sum2 = sum2 << 8;
        sum2 += data[9];
        if (sum1 == sum2)
        {
            ramdom = (((uint32_t)data[4]) << 24) + (((uint32_t)data[5]) << 16) + (((uint32_t)data[6]) << 8) + data[7];
            md5_calculate_key(ramdom, md5_authen_value); // 计算md5
            res = 1;
        }
    }
    if (res != 0)
    {
        *rlen = iot_ble_authen_pack(AUTHEN_RESOPN, &md5_authen_value[8], 4, reslt_resp);
    }
    return res;
}

// 检查鉴定回应,并生成鉴定结果
/*
发送03包


IN:uint8_t *data
OUT:uint8_t *reslt_resp

return:
1-pass
0-fail
*/
uint8_t iot_ble_check_authen_respond(uint8_t *data, uint8_t len, uint8_t *reslt_resp, uint8_t *rlen)
{
    uint8_t res = 0;
    uint16_t sum1, sum2 = 0;
    if (data[0] == '*' && data[1] == '*' && data[2] == AUTHEN_RESOPN)
    {
        sum1 = calcute_sum(&data[2], 6); // 计算crc
        sum2 = ((uint16_t)data[8] << 8) + data[9];
        if (sum1 == sum2)
        {
            // 对比自己计算的md5
            if (md5_authen_value[8] == data[4] && md5_authen_value[9] == data[5] && md5_authen_value[10] == data[6] && md5_authen_value[11] == data[7])
            {
                res = 1; // 校验通过
            }
        }
    }
    if (res != 0)
    {
        *rlen = iot_ble_authen_result(AUTHEN_PASS, reslt_resp, AUTHEN_RESLUT);
    }
    else
    {
        *rlen = iot_ble_authen_result(AUTHEN_FAIL, reslt_resp, AUTHEN_RESLUT);
    }
    return res;
}

/**
* @brief :get_pack_type
解析 BLE 加密握手 RX包格式：开头，结尾
获取包的类型

data :输入报文
return:
帧序号类型

*/
uint8_t get_pack_type(uint8_t *data, uint8_t len)
{
    uint8_t type = 0xff;
    uint16_t sum1, sum2 = 0;
    // dump_buf("get_pack_type raw data:", data, len);
    if ((data[0] == '*') && (data[1] == '*')) // 加密报文帧头
    {
        if (len < 4)
        {
            ESP_LOGE(TAG, "get pack type data len error");
            return type;
        }

        sum1 = calcute_sum(&data[2], len - 4); // 计算crc
        sum2 = ((uint16_t)data[len - 2] << 8) + data[len - 1];
        // ESP_LOGE(TAG, "sum1:0x%x, sum2:0x%x", sum1, sum2);
        if (sum1 == sum2)
        {
            type = data[2];
        }
    }
    return type;
}

/*IOT数据加密，先base64编码，再aes加密*/
void iot_encrypt_data(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen)
{
    mbedtls_aes_context aes_ctx;
    size_t base64_enclen = 0;
    uint16_t count = 0;
    uint16_t aes_encrypt_len = 0;
    uint16_t ptr_len = 0;
    uint8_t *ptr = NULL;
    if (slen < 32) // base64编码的长度会变大
    {
        ptr_len = slen * 5;
    }
    else
    {
        ptr_len = (uint16_t)((float)slen * 1.4);
    }
    ptr = (uint8_t *)iot_calloc(ptr_len * sizeof(uint8_t));
    if (ptr == NULL)
    {
        printf("malloc faild");
        return;
    }

    memset(ptr, 0, ptr_len);

    mbedtls_aes_init(&aes_ctx);                          // 初始化指定的 AES 上下文
    mbedtls_aes_setkey_enc(&aes_ctx, generate_key, 128); // 设置AES加密秘钥

    mbedtls_base64_encode(ptr, ptr_len, &base64_enclen, scr, slen); // base64编码
    count = base64_enclen / 16;

    for (uint16_t i = 0; i < count + 1; i++)
    {
        // AES128加密，不足16字节用0填充
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, ptr + aes_encrypt_len, dst + aes_encrypt_len); // 每次可以加密16个字节数据
        aes_encrypt_len += 16;
    }
    if (ptr != NULL)
    {
        free(ptr);
    }
    *olen = aes_encrypt_len;

    mbedtls_aes_free(&aes_ctx); // 释放并清除指定的 AES 上下文
}

/*iot数据解密，先aes解密，再反base64编码*/
void iot_decrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t dlen, uint16_t *olen)
{
    mbedtls_aes_context aes_ctx;
    size_t base64_declen = 0, aes_decrypt_len = 0;
    uint16_t count = slen / 16;
    uint8_t *ptr = NULL;
    ptr = (uint8_t *)iot_calloc(slen * sizeof(uint8_t));
    if (ptr == NULL)
    {
        printf("malloc faild");
        return;
    }
    memset(ptr, 0, slen);

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, generate_key, 128); // 设置AES解密秘钥

    for (uint16_t i = 0; i < count; i++)
    {
        // 每一次解密16字节
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, src + aes_decrypt_len, ptr + aes_decrypt_len);
        aes_decrypt_len += 16;
    }

    mbedtls_base64_decode(dst, dlen, &base64_declen, ptr, strlen((char *)ptr));

    if (ptr != NULL)
    {
        free(ptr);
    }
    *olen = base64_declen;

    mbedtls_aes_free(&aes_ctx);
}

/*IOT数据AES加密*/
void iot_aes_encrypt_data(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *key)
{
    mbedtls_aes_context aes_ctx;
    uint16_t count = 0;
    uint8_t aes_in[16];
    uint16_t aes_encrypt_len = 0;
    memset(aes_in, 0, sizeof(aes_in));          // 清零
    mbedtls_aes_init(&aes_ctx);                 // 初始化指定的 AES 上下文
    mbedtls_aes_setkey_enc(&aes_ctx, key, 128); // 设置AES加密秘钥

    count = slen / 16;
    // 注意：每次一定要加密16字节，数组少于16字节会导致加密结果不正确！！！
    // 下面对不足16字节部分的处理
    if (slen < 16) // 不足16字节
    {
        memcpy(aes_in, scr, slen);
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, aes_in, dst); // 每次可以加密16个字节数据
        aes_encrypt_len += 16;
    }
    else // 超过16字节
    {
        for (uint16_t i = 0; i < count; i++)
        {
            // AES128加密，不足16字节用0填充
            mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, scr + aes_encrypt_len, dst + aes_encrypt_len); // 每次可以加密16个字节数据
            aes_encrypt_len += 16;
        }
        if (slen % 16 != 0) // 最后不足16字节部分
        {
            memcpy(aes_in, scr + aes_encrypt_len, slen % 16);
            mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, aes_in, dst + aes_encrypt_len);
            aes_encrypt_len += 16;
        }
    }
    *olen = aes_encrypt_len;
    mbedtls_aes_free(&aes_ctx); // 释放并清除指定的 AES 上下文
}

void iot_communciate_encrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen)
{
    iot_aes_encrypt_data(scr, slen, dst, olen, generate_key);
}

void iot_file_encrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen)
{
    iot_aes_encrypt_data(scr, slen, dst, olen, file_aes_key);
}

/*iot数据AES解密*/
void iot_aes_decrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *key)
{
    mbedtls_aes_context aes_ctx;
    size_t aes_decrypt_len = 0;
    uint16_t count = slen / 16;

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, key, 128); // 设置AES解密秘钥

    for (uint16_t i = 0; i < count; i++)
    {
        // 每一次解密16字节
        mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, src + aes_decrypt_len, dst + aes_decrypt_len);
        aes_decrypt_len += 16;
    }

    *olen = aes_decrypt_len;

    mbedtls_aes_free(&aes_ctx);
}

void iot_communciate_decrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen)
{
    iot_aes_decrypt_data(scr, slen, dst, olen, generate_key);
}

void iot_file_decrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen)
{
    iot_aes_decrypt_data(scr, slen, dst, olen, file_aes_key);
}

// AES加密发送包
void encrypt_sending_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint16_t len = 0;
    // 标记明文的数量
    out_Data[0] = (uint8_t)(in_data_len >> 8); // 高8位在前
    out_Data[1] = (uint8_t)in_data_len;        // 低8位在后
    // iot_communciate_encrypt(in_data, in_data_len, out_Data+2, &len);
    iot_communciate_aes_cbc128_encrypt(in_data, in_data_len, out_Data + 2, &len, md5_authen_value);
    *out_data_len = len + 2; // 密文数量+2个长度
}

// 获取加密后包总长度
uint16_t get_encrypt_sending_pack_len(uint16_t len)
{
    uint16_t count = 0;
    if (len > 0)
    {
        if (len % 16 != 0) // 有余数
        {
            count = (len / 16 + 1) * 16;
            count += 2; // 2个字节表示长度
        }
        else
        {
            count = len;
            count += 2; // 2个字节表示长度
        }
    }
    return count;
}

/**
* @brief :decrypt_receive_pack
AES解密接收包

in_data :输入报文
out_Data：输出报文

*/
void decrypt_receive_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint16_t len = 0;
    len = ((uint16_t)in_data[0] << 8) + in_data[1]; // 获取明文长度
    // ESP_LOGI(TAG, "plaintext,len:%d", len);
    if (len > in_data_len - 2) // 检查明文长度和总长度
    {
        ESP_LOGE(TAG, "aes decrypt invalid len");
    }
    // iot_communciate_decrypt(in_data+2, in_data_len-2, out_Data, out_data_len);
    iot_communciate_aes_cbc128_decrypt(in_data + 2, in_data_len - 2, out_Data, out_data_len, md5_authen_value);
    *out_data_len = len;
}

// 生成通信AES秘钥
void generate_com_aes_key(const uint8_t *key_in)
{
    if (com_aes_key == NULL)
    {
        com_aes_key = (uint8_t *)iot_calloc(16 * sizeof(uint8_t));
        if (com_aes_key == NULL)
        {
            printf("malloc faild");
            return;
        }
    }

    for (uint8_t i = 0; i < 16; i++)
    {
        com_aes_key[i] = (i * 8 + 1) ^ key_in[i];
    }
    dump_buf("com_aes_key", com_aes_key, 16);
}

// 生成文件AES秘钥
void generate_file_aes_key(const uint8_t *key_in)
{
    uint8_t mac[16];
    esp_efuse_mac_get_default(mac);
    esp_efuse_mac_get_default(mac + 6);
    esp_efuse_mac_get_default(mac + 10);

    if (file_aes_key == NULL)
    {
        file_aes_key = (uint8_t *)iot_calloc(16 * sizeof(uint8_t));
        if (file_aes_key == NULL)
        {
            printf("malloc faild");
            return;
        }
    }
    for (uint8_t i = 0; i < 16; i++)
    {
        file_aes_key[i] = mac[i] ^ key_in[i];
    }
    dump_buf("file_aes_key", file_aes_key, 16);
}

/*iot数据AES-CBC加密*/
void iot_aes_cbc_encrypt_data(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *key, uint8_t *inv, uint8_t key_type)
{
    uint16_t count = 0;
    uint8_t iv_buf[16];
    uint8_t aes_in[16];
    uint16_t aes_encrypt_len = 0;
    mbedtls_aes_context aes_ctx;
    memcpy(iv_buf, inv, sizeof(iv_buf));
    memset(aes_in, 0, sizeof(aes_in)); // 清零
    mbedtls_aes_init(&aes_ctx);
    if (!key_type) // 密钥类型
    {
        mbedtls_aes_setkey_enc(&aes_ctx, key, 256); // 设置AES-CBC加密密钥256
    }
    else
    {
        mbedtls_aes_setkey_enc(&aes_ctx, key, 128); // 设置AES-CBC加密密钥128
    }
    if (slen < 16) // 不足16字节
    {
        memcpy(aes_in, scr, slen);
        mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, aes_in, dst);
        aes_encrypt_len += 16;
    }
    else // 超过16字节
    {
        count = slen / 16;
        for (uint16_t i = 0; i < count; i++)
        {
            // AES-CBC加密，每次加密16字节
            mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, scr + aes_encrypt_len, dst + aes_encrypt_len);
            memcpy(iv_buf, dst + aes_encrypt_len, sizeof(iv_buf)); // 本次目标密文作为下一次AES-CBC加密的初始IV向量
            aes_encrypt_len += 16;
        }

        if (slen % 16 != 0) // 最后不足16字节部分
        {
            memcpy(aes_in, scr + aes_encrypt_len, slen % 16); // 后面补充0
            mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, aes_in, dst + aes_encrypt_len);
            aes_encrypt_len += 16;
        }
        else // 如果刚好16字节整数倍，则需要再加一个包
        {
            // mbedtls_aes_crypt_cbc( &aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, aes_in, dst+aes_encrypt_len);
            // aes_encrypt_len += 16;
        }
    }
    *olen = aes_encrypt_len;
    mbedtls_aes_free(&aes_ctx); // 释放并清除指定的 AES 上下文
}

/*iot数据AES解密*/
void iot_aes_cbc_decrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *key, uint8_t *iv, uint8_t key_type)
{
    mbedtls_aes_context aes_ctx;
    uint8_t iv_buf[16];
    size_t aes_decrypt_len = 0;
    uint16_t count = slen / 16;

    memcpy(iv_buf, iv, sizeof(iv_buf)); // 复制第一次初始化IV向量
    mbedtls_aes_init(&aes_ctx);
    if (!key_type) // 256
    {
        // ESP_LOGI(TAG, "aes256");
        // ESP_LOGI(TAG, "slen:%d", slen);
        mbedtls_aes_setkey_dec(&aes_ctx, key, 256); // 设置AES-CBC解密密钥256
    }
    else
    {
        // ESP_LOGI(TAG, "aes128");
        mbedtls_aes_setkey_dec(&aes_ctx, key, 128); // 设置AES-CBC解密密钥128
    }

    for (uint16_t i = 0; i < count; i++)
    {
        // 每一次解密16字节
        mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, 16, iv_buf, src + aes_decrypt_len, dst + aes_decrypt_len);
        memcpy(iv_buf, src + aes_decrypt_len, sizeof(iv_buf)); // 本次源密文作为下一次AES-CBC解密的初始IV向量
        aes_decrypt_len += 16;
    }
    *olen = aes_decrypt_len;
    mbedtls_aes_free(&aes_ctx); // 释放并清除指定的 AES 上下文
}

// AES-CBC 256加密
void iot_communciate_aes_cbc_encrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv)
{
    iot_aes_cbc_encrypt_data(scr, slen, dst, olen, aes_cbc_key, iv, 0);
}

// AES-CBC 256解密
void iot_communciate_aes_cbc_decrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv)
{
    // ESP_LOGI(TAG, "iot aes256");
    iot_aes_cbc_decrypt_data(scr, slen, dst, olen, aes_cbc_key, iv, 0);
    //   dump_buf("windy:aes_cbc_key", aes_cbc_key, sizeof(aes_cbc_key));
}

// AES-CBC 128加密
void iot_communciate_aes_cbc128_encrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv)
{
    iot_aes_cbc_encrypt_data(scr, slen, dst, olen, generate_key, iv, 1);
}

// AES-CBC 128解密
void iot_communciate_aes_cbc128_decrypt(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv)
{
    iot_aes_cbc_decrypt_data(scr, slen, dst, olen, generate_key, iv, 1);
    //   dump_buf("windy:generate_key", generate_key, sizeof(generate_key));
}

// AES-CBC加密发送包
void aes_cbc_encrypt_sending_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint32_t random = 0;
    uint8_t once_ramdom[4];
    uint8_t once_md5[16];
    uint16_t len = 0;
    // 标记明文的数量
    out_Data[0] = (uint8_t)(in_data_len >> 8); // 高8位在前
    out_Data[1] = (uint8_t)in_data_len;        // 低8位在后
    random = esp_random();                     // 产生一个32位随机数
    // printf("send ramdom:0x%x\n", random);
    once_ramdom[0] = (uint8_t)random;
    once_ramdom[1] = (uint8_t)(random >> 8);
    once_ramdom[2] = (uint8_t)(random >> 16);
    once_ramdom[3] = (uint8_t)(random >> 24);
    md5_calculate_key(random, once_md5); // 计算MD5
    // dump_buf("send MD5", once_md5, sizeof(once_md5));
    memcpy(out_Data + 2, once_ramdom, 4);
    iot_communciate_aes_cbc_encrypt(in_data, in_data_len, out_Data + 6, &len, once_md5);
    *out_data_len = len + 6; // 密文数量+2个明文表示长度+4个随机数
}

// AES-CBC解密接收包
/*
*in_data
*out_Data


*/
void aes_cbc_decrypt_receive_pack(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint32_t once_ramdom;
    uint8_t once_md5[16];
    uint16_t len = 0;
    len = ((uint16_t)in_data[0] << 8) + in_data[1]; // 获取明文长度
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, in_data, in_data_len, ESP_LOG_DEBUG);
    
    if (len > (in_data_len - 6)) {
        // 检查明文长度标识和整体长度关系，非法则过滤处理
        ESP_LOGE(TAG, "invalid aec-cbc len ( in_data_len : %d, out_data_len : %d)", in_data_len, len);
        return;
    } else if (0 != ((in_data_len - 6) % 16)) {
        // AES加密后数据(数量为16字节整数倍)
        ESP_LOGE(TAG, "invalid aec-cbc len ( in_data_len : %d)", in_data_len);
        return;
    }
    
    once_ramdom = BUILD_UINT32(in_data[2], in_data[3], in_data[4], in_data[5]);
    // printf("get ramdom:0x%x\n", once_ramdom);
    md5_calculate_key(once_ramdom, once_md5); // 计算MD5
    // dump_buf("receive MD5", once_md5, sizeof(once_md5));

    iot_communciate_aes_cbc_decrypt(in_data + 6, in_data_len - 6, out_Data, out_data_len, once_md5); // AES-CBC解密
    *out_data_len = len;
}

// 生成ECDH协商公密钥
static void client_public_gen(uint8_t *public_key)
{
    int ret = 0;
    size_t olen;
    unsigned char buf[65];
    mbedtls_entropy_context entropy;
    mbedtls_ecp_point pub;
    char pers[] = "simple_ecdh";

    if (cli_grp != NULL)
    {
        mbedtls_ecp_group_free(cli_grp);
        free(cli_grp);
        cli_grp = NULL;
    }

    if (cli_pri != NULL)
    {
        mbedtls_mpi_free(cli_pri);
        free(cli_pri);
        cli_pri = NULL;
    }

    if (cli_ctr_drbg != NULL)
    {
        mbedtls_ctr_drbg_free(cli_ctr_drbg);
        free(cli_ctr_drbg);
        cli_ctr_drbg = NULL;
    }

    cli_grp = (mbedtls_ecp_group *)iot_calloc(sizeof(mbedtls_ecp_group));
    if (cli_grp == NULL)
    {
        printf("cli_grp malloc fail!");
        goto cleanup;
    }

    cli_pri = (mbedtls_mpi *)iot_calloc(sizeof(mbedtls_mpi));
    if (cli_pri == NULL)
    {
        printf("cli_pri malloc fail!");
        goto cleanup;
    }

    cli_ctr_drbg = (mbedtls_ctr_drbg_context *)iot_calloc(sizeof(mbedtls_ctr_drbg_context));
    if (cli_ctr_drbg == NULL)
    {
        printf("cli_ctr_drbg malloc fail!");
        goto cleanup;
    }
    // mbedtls_platform_set_printf(printf);

    mbedtls_mpi_init(cli_pri);       // 初始化大数结构体
    mbedtls_ecp_group_init(cli_grp); // 初始化椭圆曲线群结构体
    mbedtls_ecp_point_init(&pub);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(cli_ctr_drbg);

    mbedtls_entropy_add_source(&entropy, entropy_source, NULL,
                               MBEDTLS_ENTROPY_MAX_GATHER, MBEDTLS_ENTROPY_SOURCE_STRONG);
    mbedtls_ctr_drbg_seed(cli_ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const uint8_t *)pers, strlen(pers));
    // mbedtls_printf("\n  . setup rng ... ok\n");

    ret = mbedtls_ecp_group_load(cli_grp, MBEDTLS_ECP_DP_SECP256R1); // 加载椭圆曲线SECP256R1
    if (ret != 0)
    {
        mbedtls_printf("ecp_group_load fail\n");
        goto cleanup;
    }
    // mbedtls_printf("\n  . select ecp group SECP256R1 ... ok\n");

    ret = mbedtls_ecdh_gen_public(cli_grp, cli_pri, &pub, mbedtls_ctr_drbg_random, cli_ctr_drbg); // 客户端生成公开参数
    if (ret != 0)
    {
        mbedtls_printf("ecdh_gen_public fail\n");
        goto cleanup;
    }
    // assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(cli_grp, &pub,
                                   MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, sizeof(buf));
    dump_buf("  1. ecdh client generate public parameter:", buf, olen);
    memcpy(public_key, buf + 1, 64); // 复制公钥，第一个字节是压缩标志，需要剔除。

cleanup:
    mbedtls_entropy_free(&entropy);
    mbedtls_ecp_point_free(&pub);
}

// 根据对方公钥计算ECDH共享密钥
void client_calculate_secret(uint8_t *public_key)
{
    //    uint8_t out_public[65];
    int ret = 0;
    unsigned char buf[65];
    // char secret_buf[33];
    mbedtls_mpi secret;
    mbedtls_ecp_point pub;
    mbedtls_ecdh_context srv;
    //    memset(out_public, 0, sizeof(out_public));
    // memset(secret_buf, 0 ,sizeof(secret_buf));
    mbedtls_ecdh_init(&srv);
    mbedtls_ecp_point_init(&pub);
    mbedtls_mpi_init(&secret);

    dump_buf("windy:public_key", public_key, 80);

    buf[0] = 0x04;
    memcpy(buf + 1, public_key, 64);
    mbedtls_ecp_point_read_binary(cli_grp, &pub, buf, sizeof(buf)); // 加载公钥

    ret = mbedtls_ecdh_compute_shared(cli_grp, &secret, &pub, cli_pri, mbedtls_ctr_drbg_random, cli_ctr_drbg); // 客户端生成会话秘钥
    if (ret != 0)
    {
        printf("ecdh_compute_shared fail\n");
        goto cleanup;
    }

    // printf("secret,len:%d", mbedtls_mpi_size(&secret));
    mbedtls_mpi_write_binary(&secret, aes_cbc_key, mbedtls_mpi_size(&secret)); // windy 输出aes_cbc_key
    // dump_buf("client generate secret:", aes_cbc_key, mbedtls_mpi_size(&secret));

cleanup:

    mbedtls_ecp_group_free(cli_grp);
    mbedtls_ctr_drbg_free(cli_ctr_drbg);
    mbedtls_mpi_free(cli_pri);
    mbedtls_mpi_free(&secret);
    mbedtls_ecp_point_free(&pub);

    // 释放内存
    if (cli_grp != NULL)
    {
        free(cli_grp);
        cli_grp = NULL;
    }

    if (cli_pri != NULL)
    {
        free(cli_pri);
        cli_pri = NULL;
    }

    if (cli_ctr_drbg != NULL)
    {
        free(cli_ctr_drbg);
        cli_ctr_drbg = NULL;
    }
}

// 生成密钥
void server_gen_public_key(uint8_t *public_key)
{
    int ret = 0;
    size_t olen;
    unsigned char buf[65];
    mbedtls_entropy_context entropy;
    mbedtls_ecp_point pub;
    char pers[] = "simple_ecdh";

    if (srv_grp != NULL)
    {
        mbedtls_ecp_group_free(srv_grp);
        free(srv_grp);
        srv_grp = NULL;

        printf("do srv_grp  free AA !!\n");
    }

    if (srv_pri != NULL)
    {
        mbedtls_mpi_free(srv_pri);
        free(srv_pri);
        srv_pri = NULL;
    }

    if (srv_ctr_drbg != NULL)
    {
        mbedtls_ctr_drbg_free(srv_ctr_drbg);
        free(srv_ctr_drbg);
        srv_ctr_drbg = NULL;
    }

    // mbedtls_platform_set_printf(printf);
    if (srv_grp == NULL)
    {
        srv_grp = (mbedtls_ecp_group *)iot_calloc(sizeof(mbedtls_ecp_group));
        if (srv_grp == NULL)
        {
            printf("malloc fail!");
            goto cleanup;
        }
    }

    if (srv_pri == NULL)
    {
        srv_pri = (mbedtls_mpi *)iot_calloc(sizeof(mbedtls_mpi));
        if (srv_pri == NULL)
        {
            printf("malloc fail!");
            goto cleanup;
        }
    }

    if (srv_ctr_drbg == NULL)
    {
        srv_ctr_drbg = (mbedtls_ctr_drbg_context *)iot_calloc(sizeof(mbedtls_ctr_drbg_context));
        if (srv_ctr_drbg == NULL)
        {
            printf("malloc fail!");
            goto cleanup;
        }
    }

    mbedtls_mpi_init(srv_pri);
    mbedtls_ecp_group_init(srv_grp); // 初始化椭圆曲线群结构体
    mbedtls_ecp_point_init(&pub);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(srv_ctr_drbg);

    mbedtls_entropy_add_source(&entropy, entropy_source, NULL,
                               MBEDTLS_ENTROPY_MAX_GATHER, MBEDTLS_ENTROPY_SOURCE_STRONG);
    mbedtls_ctr_drbg_seed(srv_ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const uint8_t *)pers, strlen(pers));
    mbedtls_printf("\n  . setup rng ... ok\n");

    ret = mbedtls_ecp_group_load(srv_grp, MBEDTLS_ECP_DP_SECP256R1); // 加载椭圆曲线SECP256R1
    if (ret != 0)
    {
        mbedtls_printf("ecp_group_load fail\n");
        //        printf("22 ecp_group_load fail\n");
        goto cleanup;
    }
    //    mbedtls_printf("\n  . select ecp group SECP256R1 ... ok\n");

    ret = mbedtls_ecdh_gen_public(srv_grp, srv_pri, &pub, mbedtls_ctr_drbg_random, srv_ctr_drbg); // 服务端生成公开参数
    if (ret != 0)
    {
        mbedtls_printf("ecdh_gen_public fail\n");
        //        printf("22 ecdh_gen_public fail\n");
        goto cleanup;
    }
    // assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(srv_grp, &pub,
                                   MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, sizeof(buf));
    dump_buf("  2. ecdh server generate public parameter:", buf, olen);
    memcpy(public_key, buf + 1, 64); // 复制公钥，第一个字节是压缩标志，需要剔除。

cleanup:
    mbedtls_entropy_free(&entropy);
    mbedtls_ecp_point_free(&pub);
}

// 计算共享密钥
void server_calculate_secret(uint8_t *public_key, uint8_t *share_key)
{
    //    uint8_t out_public[65];
    int ret = 0;
    unsigned char buf[65];
    // char secret_buf[33];
    mbedtls_mpi secret;
    mbedtls_ecp_point pub;
    mbedtls_ecdh_context srv;
    mbedtls_ecdh_init(&srv);

    mbedtls_ecp_point_init(&pub);
    mbedtls_mpi_init(&secret);

    buf[0] = 0x04;
    memcpy(buf + 1, public_key, 64);

    printf("srv_grp =%ld, len=%d,\n", (uint32_t)((uint8_t *)srv_grp), sizeof(*srv_grp));
    if (NULL == srv_grp)
    {
        printf("srv_grp  NULL !!\n");
    }

    dump_buf("(*srv_grp) dd", (uint8_t *)srv_grp, sizeof(*srv_grp));

    mbedtls_ecp_point_read_binary(srv_grp, &pub, buf, sizeof(buf)); // 加载公钥

    ret = mbedtls_ecdh_compute_shared(srv_grp, &secret, &pub, srv_pri, mbedtls_ctr_drbg_random, srv_ctr_drbg); // 客户端生成会话秘钥
    if (ret != 0)
    {
        printf("ecdh_compute_shared fail\n");
        goto cleanup;
    }
    // printf("server secret,len:%d\n", mbedtls_mpi_size(&secret));
    mbedtls_mpi_write_binary(&secret, share_key + (32 - mbedtls_mpi_size(&secret)), mbedtls_mpi_size(&secret));
    // dump_buf("server generate secret:", share_key, mbedtls_mpi_size(&secret));

cleanup:
    mbedtls_ecp_group_free(srv_grp);
    mbedtls_mpi_free(srv_pri);
    mbedtls_ctr_drbg_free(srv_ctr_drbg);
    mbedtls_mpi_free(&secret);
    mbedtls_ecp_point_free(&pub);

    if (srv_grp != NULL)
    {
        free(srv_grp);
        srv_grp = NULL;
        printf("do srv_grp  free BB !!\n");
    }

    if (srv_pri != NULL)
    {
        free(srv_pri);
        srv_pri = NULL;
    }

    if (srv_ctr_drbg != NULL)
    {
        free(srv_ctr_drbg);
        srv_ctr_drbg = NULL;
    }
}

// 计算sha256
void calculate_sha256(uint8_t *data, uint16_t len, uint8_t *hash)
{
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);                                                  // 初始化MD结构体
    mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), data, len, hash); // 使用MD接口计算消息熵要
    mbedtls_md_free(&md_ctx);
}

// 用自己公钥对数据做ECDSA验签
/*
对输入的加密报文解密：
*data:公钥A2
*sign_data:ECDSA签名2

return:
1-ok
0-fail

*/
int ecdsa_verify(uint8_t *data, uint16_t len, uint8_t *sign_data, uint8_t *public_key)
{
    uint8_t ret = 0;
    uint8_t pub[65]; // 65个字节公钥，第一个字节0x04表示公钥未压缩
    uint8_t hash[32];
    mbedtls_mpi r, s;
    mbedtls_ecdsa_context ctx;

    mbedtls_mpi_init(&r); // 初始化mpi结构体
    mbedtls_mpi_init(&s);
    mbedtls_ecdsa_init(&ctx); // 初始化ECDSA结构体

    mbedtls_ecp_group_load(&ctx.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    pub[0] = 0x04; // 未压缩标志
    memcpy(pub + 1, public_key, 64);
    dump_buf("ecdh public_key:", public_key, 64);
    mbedtls_ecp_point_read_binary(&ctx.MBEDTLS_PRIVATE(grp), &ctx.MBEDTLS_PRIVATE(Q), pub, 65); // 加载公钥

    mbedtls_mpi_read_binary(&r, sign_data, 32); // 加载签名
    mbedtls_mpi_read_binary(&s, sign_data + 32, 32);

    calculate_sha256(data, len, hash); // 计算哈希值sha256
    // dump_buf("verify hash:", hash, 32);
    // ECDSA验签接口
    if (!mbedtls_ecdsa_verify(&ctx.MBEDTLS_PRIVATE(grp), hash, 32, &ctx.MBEDTLS_PRIVATE(Q), &r, &s))
    {
        // mbedtls_printf("ecdsa verify signature ... ok\n\n");
        ret = 1;
    }
    else
    {
        mbedtls_printf("ecdsa verify signature ... fail\n\n");
    }

    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecdsa_free(&ctx);
    return ret;
}

// 用自己的私钥对数据做ECDSA签名
void ecdsa_sign(uint8_t *data, uint16_t len, uint8_t *private_key, uint8_t *sign_out)
{
    int ret = 0;
    uint8_t hash[32];
    char pers[] = "Poweroak";
    size_t rlen, slen;
    mbedtls_mpi r, s;
    mbedtls_ecdsa_context ctx;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_mpi_init(&r); // 初始化mpi结构体
    mbedtls_mpi_init(&s);
    mbedtls_ecdsa_init(&ctx);         // 初始化ECDSA结构体
    mbedtls_entropy_init(&entropy);   // 初始化熵结构体
    mbedtls_ctr_drbg_init(&ctr_drbg); // 初始化随机数结构体
                                      // 添加熵源接口，设置熵源属性
    mbedtls_entropy_add_source(&entropy, entropy_source, NULL,
                               MBEDTLS_ENTROPY_MAX_GATHER, MBEDTLS_ENTROPY_SOURCE_STRONG);
    // 根据个性化字符串更新种子
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const uint8_t *)pers, strlen(pers));
    if (ret != 0)
    {
        printf("drbg_seedfail\n");
        goto cleanup;
    }
    // assert_exit(ret == 0, ret);
    // mbedtls_printf("\n  . setup rng ... ok\n\n");
    mbedtls_ecp_group_load(&ctx.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1); // 椭圆曲线算法为SECP256R1
    mbedtls_mpi_read_binary(&ctx.MBEDTLS_PRIVATE(d), private_key, 32);           // 加载私钥
    // dlen = mbedtls_mpi_size(&ctx.d);
    // printf("dlen:%d\n", dlen);
    calculate_sha256(data, len, hash);
    // dump_buf("hash:", hash, 32);
    ret = mbedtls_ecdsa_sign(&ctx.MBEDTLS_PRIVATE(grp), &r, &s, &ctx.MBEDTLS_PRIVATE(d), hash, sizeof(hash), mbedtls_ctr_drbg_random, &ctr_drbg);
    rlen = mbedtls_mpi_size(&r);
    slen = mbedtls_mpi_size(&s);
    // printf("rlen:%d, slen:%d\n", rlen, slen);
    mbedtls_mpi_write_binary(&r, sign_out, rlen);
    mbedtls_mpi_write_binary(&s, sign_out + rlen, slen);
    // dump_buf("  3. ecdsa generate signature:", sign_out, rlen + slen);

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecdsa_free(&ctx);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void pack_sign(uint8_t *data, uint16_t len, uint8_t *sign_out)
{
    ecdsa_sign(data, len, sign_private_key, sign_out);
}

/*
对输入的加密报文解密：
*IN:data:公钥A2
*IN:sign_data:ECDSA签名2
return:
1-ok
0-fail
*/
uint8_t pack_verify(uint8_t *data, uint16_t len, uint8_t *sign_data)
{
    return ecdsa_verify(data, len, sign_data, verify_public_key);
}

// 组装密钥协商数据
static void ecdh_key_agreement_data(uint8_t *public_key, uint8_t *out)
{
    uint8_t sign_data[64];
    memcpy(out, public_key, 64);
    memcpy(out + 64, md5_authen_value, 16); // 第一次MD5的数据
    // dump_buf("out sign_data:", out, 80);
    pack_sign(out, 80, sign_data); // ECDSA签名
    memcpy(out + 64, sign_data, sizeof(sign_data));
    // dump_buf("iot sign_data:", sign_data, sizeof(sign_data));
}

uint16_t iot_ble_authen_ecdh_key_agreement_request(uint8_t *data)
{
    uint16_t len = 0;
    //uint8_t pack_data[128];
    //uint8_t pub[64];
    //client_public_gen(pub); //生成协商密钥
    //dump_buf("IOT ECDH PUB:", pub, sizeof(pub));
    //ecdh_key_agreement_data(pub, pack_data);
    len = iot_ble_authen_pack(ECDH_AGRESSMENT_REQUEST, ecc_key_agreement_data, sizeof(ecc_key_agreement_data), data); //打包
    return len;
}

// IOT对密钥协商请求的回应处理
uint8_t iot_ble_authen_ecdh_key_agreement_respond_check(uint8_t *data, uint8_t len, uint8_t *reslt_resp, uint8_t *rlen)
{
    uint8_t ret = 0;
    uint8_t sign_data[64];
    uint8_t verify_data[80];
    // dump_buf("respond_check", data, len);
    if (!check_pack_format(data, len, ECDH_AGRESSMENT_RESPOND)) // 格式检查
    {
        *rlen = iot_ble_authen_result(AUTHEN_FAIL, reslt_resp, ECDH_AGRESSMENT_RESULT);
        return ret;
    }

    memcpy(sign_data, data + 68, sizeof(sign_data)); // 复制签名值
    dump_buf("iot get sign_data:", sign_data, sizeof(sign_data));
    memcpy(verify_data, data + 4, 64);              // 复制公钥部分
    memcpy(verify_data + 64, md5_authen_value, 16); // 复制第一次MD5的计算结果

    if (pack_verify(verify_data, sizeof(verify_data), sign_data)) // 验签
    {
        client_calculate_secret(verify_data); // 计算ECDH共享密钥
        // dump_buf("iot_share_key:\n", aes_cbc_key, 32);
        ESP_LOGI(TAG, "ecdsa verify ok");
        *rlen = iot_ble_authen_result(AUTHEN_PASS, reslt_resp, ECDH_AGRESSMENT_RESULT);
        ret = 1;
        return ret;
    }

    *rlen = iot_ble_authen_result(AUTHEN_FAIL, reslt_resp, ECDH_AGRESSMENT_RESULT);
    return ret;
}

// AES秘钥初始化
void aes_key_init(void)
{
    generate_com_aes_key(partition_key);
    generate_file_aes_key(partition_key);
}

void encrypt_data_init(void)
{
    /*BLE 初始化AES秘钥*/
    aes_key_init();

#ifdef CONFIG_MQTT1_TCP_ENCRYPT_ENABLE    
    /*加载证书和私钥*/
    if(load_all_cer()) {
        ESP_LOGI(TAG, "load cert success!");
    } else {
        ESP_LOGE(TAG, "load cert fail!");

        // 伪装旧证书，用作首次证书下载时的加密动作
        cert_default_init();
    }

    /*RSA公钥和私钥初始化*/
    rsa_key_init();
#endif    
}

///////////////////////////////ble client center 20240103 add//////////////////////////////////////////////

/// @brief 本地基础私钥，交换数据时，需要用于对目标数据的签名
const uint8_t privateKey[32] = {0x4F, 0x19, 0xA1, 0x6E, 0x3E, 0x87, 0xBD, 0xD9, 0xBD, 0x24, 0xD3, 0xE5, 0x49, 0x5B, 0x88, 0x04, 0x15, 0x11, 0x94, 0x3C, 0xBC, 0x8B, 0x96, 0x9A, 0xDE, 0x96, 0x41, 0xD0, 0xF5, 0x6A, 0xF3, 0x37};

/// @brief 本地基础公钥，交换数据时，需要用于对目标数据的验签
const uint8_t publicKey[64] = {0xA7, 0x3A, 0xBF, 0x5D, 0x22, 0x32, 0xC8, 0xC1, 0xC7, 0x2E, 0x68, 0x30, 0x43, 0x43, 0xC2, 0x72, 0x49, 0x5E, 0x3A, 0x8F, 0xD6, 0xF3, 0x0E, 0xA9, 0x6D, 0xE2, 0xF4, 0xB3, 0xCE, 0x60, 0xB2, 0x51,
                               0xEE, 0x21, 0xAC, 0x66, 0x7C, 0xF8, 0xA7, 0x1E, 0x18, 0xB4, 0x6B, 0x66, 0x4E, 0xAE, 0xFF, 0xE3, 0xC4, 0x89, 0xF2, 0x4F, 0x69, 0x5B, 0x64, 0x11, 0xDB, 0x7E, 0x22, 0xCC, 0xC8, 0x5A, 0x85, 0x94};
/// @brief 每次AES参与的IV
uint8_t md5_authen_value_client[16]; // md5 iv

uint8_t SendPublicKey[64];

///// @brief 交换拿到的公钥
// uint8_t publicKeyA1[64];

/// @brief 共享Key
uint8_t shartKey_client[32];

/*
ble client侧，解密加密正常modbus报文
*/
// AES-CBC 256加密
void iot_communciate_aes_cbc_encrypt_run_in_client(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv)
{
    iot_aes_cbc_encrypt_data(scr, slen, dst, olen, shartKey_client, iv, 0); // 0,0-256;1-128
}

// AES-CBC 256解密
void iot_communciate_aes_cbc_decrypt_run_in_client(const uint8_t *scr, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *iv)
{
    // ESP_LOGI(TAG, "iot aes256");
    iot_aes_cbc_decrypt_data(scr, slen, dst, olen, shartKey_client, iv, 0);
    //   dump_buf("windy:aes_cbc_key", aes_cbc_key, sizeof(aes_cbc_key));
}

// AES-CBC加密发送包
void aes_cbc_encrypt_sending_pack_run_in_client(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint32_t random = 0;
    uint8_t once_ramdom[4];
    uint8_t once_md5[16];
    uint16_t len = 0;
    // 标记明文的数量
    out_Data[0] = (uint8_t)(in_data_len >> 8); // 高8位在前
    out_Data[1] = (uint8_t)in_data_len;        // 低8位在后
    random = esp_random();                     // 产生一个32位随机数
    // printf("send ramdom:0x%x\n", random);
    once_ramdom[0] = (uint8_t)random;
    once_ramdom[1] = (uint8_t)(random >> 8);
    once_ramdom[2] = (uint8_t)(random >> 16);
    once_ramdom[3] = (uint8_t)(random >> 24);
    md5_calculate_key(random, once_md5); // 计算MD5
    // dump_buf("send MD5", once_md5, sizeof(once_md5));
    memcpy(out_Data + 2, once_ramdom, 4);
    iot_communciate_aes_cbc_encrypt_run_in_client(in_data, in_data_len, out_Data + 6, &len, once_md5);
    *out_data_len = len + 6; // 密文数量+2个明文表示长度+4个随机数
}

// AES-CBC解密接收包
/*
*in_data
*out_Data
*/
void aes_cbc_decrypt_receive_pack_run_in_client(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint32_t once_ramdom;
    uint8_t once_md5[16];
    uint16_t len = 0;
    len = ((uint16_t)in_data[0] << 8) + in_data[1]; // 获取明文长度
    // printf("calculate receive len:%d\n", len);
    if (len > (in_data_len - 6)) // 检查明文长度标识和整体长度关系，非法则过滤处理
    {
        ESP_LOGE(TAG, "invalid aec-cbc len\n");
        return;
    }
    once_ramdom = BUILD_UINT32(in_data[2], in_data[3], in_data[4], in_data[5]);
    // printf("get ramdom:0x%x\n", once_ramdom);
    md5_calculate_key(once_ramdom, once_md5); // 计算MD5
    // dump_buf("receive MD5", once_md5, sizeof(once_md5));

    iot_communciate_aes_cbc_decrypt_run_in_client(in_data + 6, in_data_len - 6, out_Data, out_data_len, once_md5); // AES-CBC解密
    *out_data_len = len;
}

/*
buf2: input  原始数据
resend:output解密数据
*/
esp_err_t ble_center_encrypt_process_data(const uint8_t *buf2, size_t len, uint8_t *resend)
{
    if (len != 10 || buf2[0] != '*' || buf2[1] != '*')
    {
        ESP_LOGE(TAG, "Invalid input data");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t subarray[4] = {buf2[7], buf2[6], buf2[5], buf2[4]};
    resend[0] = '*';
    resend[1] = '*';
    resend[2] = 2;
    resend[3] = 4;

    // 初始化MD5上下文
    mbedtls_md5_context ctx;

    // 开始MD5计算
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    // 更新MD5上下文
    mbedtls_md5_update(&ctx, subarray, 4);
    mbedtls_md5_finish(&ctx, md5_authen_value_client);
    mbedtls_md5_free(&ctx);
    memcpy(resend + 4, md5_authen_value_client + 8, 4);

    uint16_t sum = 0;
    for (uint8_t i = 2; i < 8; i++)
    {
        sum = (sum + resend[i]) % 0xffff;
    }
    resend[8] = sum >> 8;   // 存储高字节
    resend[9] = sum & 0xff; // 存储低字节
    return ESP_OK;
}

esp_err_t ble_center_encrypt_process_data4(const uint8_t *buf2, size_t len, uint8_t *resend, uint8_t *publicKey_get)
{

    if (len < 10 || buf2[0] != 0 || buf2[1] != 0x86)
    {
        ESP_LOGI(TAG, "Invalid input data");
        return ESP_ERR_INVALID_ARG;
    }
    // 假设buf2从某个位置开始存储的是已加密的payload，需要解密部分的长度为plen
    size_t plen = len - 2; // 替换PAYLOAD_START_INDEX为实际的偏移量
    uint8_t *ciphertext = buf2 + 2;
    uint8_t plaintext[plen];

    uint16_t outlen = 0;
    uint8_t key[16] = {0};
    // 异或运算得第一次密钥
    for (int i = 0; i < 16; i++)
    {
        key[i] = md5_authen_value_client[i] ^ com_aes_key[i];
    }
    iot_aes_cbc_decrypt_data(ciphertext, plen, plaintext, &outlen, key, md5_authen_value_client, 1);

    // 将plaintext数组内容转换为十六进制字符串用于打印
    // printData(plaintext,plen," 0x04 Des data");

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, plaintext, outlen, ESP_LOG_WARN);

    if (outlen < 134 || plaintext[0] != '*' || plaintext[1] != '*')
    {
        ESP_LOGE(TAG, "Invalid input data ,  0x04 ECDH Error:len:%d", outlen);
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(publicKey_get, plaintext + 4, 64);

    // printData(publicKeyA1,64,"Get IOT PublicKey");
    // 内容
    uint8_t content[80] = {0};
    memcpy(content, plaintext + 4, 64);
    memcpy(content + 64, md5_authen_value_client, 16);

    // 签名
    uint8_t signature[64] = {0};
    // 内容
    memcpy(signature, plaintext + 68, 64);

    if (ecdsa_verify(content, sizeof(content), signature, publicKey) != 1)
    {
        ESP_LOGI(TAG, "Data signature fial--------------------------------");
        return ESP_FAIL;
    }

    // 发送公钥组装回复数据
    // uint8_t sendPublicKey[64];426571213

    // printData(SendPublicKey,64,"server_gen_public_key PublicKey");
    memcpy(content, SendPublicKey, 64);
    memcpy(content + 64, md5_authen_value_client, 16);

    uint8_t sign_data[64];
    // 签名
    ecdsa_sign(content, sizeof(content), privateKey, sign_data);

    uint8_t sendData[134] = {0};
    sendData[0] = '*';
    sendData[1] = '*';
    sendData[2] = 5;
    sendData[3] = 0x80;
    memcpy(sendData + 4, SendPublicKey, sizeof(SendPublicKey));
    memcpy(sendData + 68, sign_data, sizeof(sign_data));

    // 计算sum
    uint16_t sum = calcute_sum(sendData + 2, sizeof(sendData) - 4);

    // sendData[132] = (uint8_t)(sum>>8);
    // sendData[133] = (uint8_t)sum;

    sendData[132] = sum >> 8;
    sendData[133] = sum & 0xff;

    // 加密
    iot_aes_cbc_encrypt_data(sendData, 134, resend + 2, &outlen, key, md5_authen_value_client, 1);

    // ESP_LOGI(TAG, "iot_aes_cbc_encrypt_data  deat  len:%d",outlen);

    resend[0] = 0x00;
    resend[1] = 0x86;

    return ESP_OK;
}

esp_err_t ble_center_encrypt_process_data6(const uint8_t *buf2, size_t len, uint8_t *resend, uint8_t *publicKey_get)
{
    // 假设buf2从某个位置开始存储的是已加密的payload，需要解密部分的长度为plen
    size_t plen = len - 2; // 替换PAYLOAD_START_INDEX为实际的偏移量
    uint8_t *ciphertext = buf2 + 2;
    uint8_t plaintext[plen];

    uint16_t outlen = 0;
    uint8_t key[16] = {0};
    // 异或运算得第一次密钥
    for (int i = 0; i < 16; i++)
    {
        key[i] = md5_authen_value_client[i] ^ com_aes_key[i];
    }
    iot_aes_cbc_decrypt_data(ciphertext, plen, plaintext, &outlen, key, md5_authen_value_client, 1);

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, plaintext, outlen, ESP_LOG_WARN);

    if (plaintext[0] != '*' || plaintext[1] != '*' || plaintext[4] != 0)
    {
        ESP_LOGI(TAG, "Invalid input data");
        return ESP_ERR_INVALID_ARG;
    }

    // 计算本次会话的共享加密key
    memset(shartKey_client, 0, sizeof(shartKey_client));
    server_calculate_secret(publicKey_get, shartKey_client);

    resend[0] = 0;
    resend[1] = 1;

    return ESP_OK;
}

void RandomSendPKey(void)
{
    server_gen_public_key(SendPublicKey);
}
