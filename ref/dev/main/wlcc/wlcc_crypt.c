#include "wlcc_crypt.h"

#include "freertos/FreeRTOS.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"
#include "esp_random.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define TAG "[wlcc_crypt]"

#define BREAK_UINT32( var, ByteNum ) \
          (uint8_t)((uint32_t)(((var) >>((ByteNum) * 8)) & 0x00FF))

#define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3) \
          ((uint32_t)((uint32_t)(((uint32_t)Byte0) & 0x00FF) \
          + ((uint32_t)(((uint32_t)Byte1) & 0x00FF) << 8) \
          + ((uint32_t)(((uint32_t)Byte2) & 0x00FF) << 16) \
          + ((uint32_t)(((uint32_t)Byte3) & 0x00FF) << 24)))

static uint8_t aes_cbc_key[32] = {
    0xB7, 0x51, 0x93, 0x2E, 0x49, 0x87, 0x02, 0x43,
    0xFE, 0x25, 0x7A, 0xCB, 0xC1, 0xFA, 0x46, 0x05,
    0x05, 0x65, 0x43, 0x65, 0x29, 0x9F, 0xBA, 0x82,
    0x98, 0x71, 0x34, 0x80, 0x25, 0x35, 0x78, 0x7A};

static void md5_calculate_key(uint32_t value, uint8_t *out_md5);
static void aes_cbc_encrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *inv);
static void aes_cbc_decrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *inv);

//AES-CBC加密发送包
void wlcc_encrypt(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint32_t random = 0;
    uint8_t once_ramdom[4] = {0};
    uint8_t once_md5[16] = {0};
    uint16_t len = 0;
    //标记明文的数量
    out_Data[0] = (uint8_t)(in_data_len >> 8); //高8位在前
    out_Data[1] = (uint8_t)in_data_len; //低8位在后
    random = esp_random(); //产生一个32位随机数

    once_ramdom[0] = (uint8_t)random;
    once_ramdom[1] = (uint8_t)(random>>8);
    once_ramdom[2] = (uint8_t)(random>>16);
    once_ramdom[3] = (uint8_t)(random>>24);
    md5_calculate_key(random, once_md5); //计算MD5

    memcpy(out_Data + 2, once_ramdom, 4);
    // iot_communciate_aes_cbc_encrypt(in_data, in_data_len, out_Data+6, &len, once_md5);
    // dump_buf("mesh_aes_cbc_encrypt_sending_pack MD5:", once_md5, 16);
    aes_cbc_encrypt_data(in_data, in_data_len, out_Data+6, &len, once_md5);
    *out_data_len = len + 6; //密文数量+2个明文表示长度+4个随机数
}

void wlcc_decrypt(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len)
{
    uint32_t once_ramdom = 0;
    uint8_t once_md5[16] = {0};
    uint16_t len = 0;

    len = ((uint16_t)in_data[0]<<8) + in_data[1]; //获取明文长度
    if(len > (in_data_len - 6)) //检查明文长度标识和整体长度关系，非法则过滤处理
    {
        ESP_LOGE(TAG, "invalid aec-cbc len\n");
        return;
    }
    once_ramdom = BUILD_UINT32(in_data[2], in_data[3], in_data[4], in_data[5]);

    md5_calculate_key(once_ramdom, once_md5); //计算MD5
    // dump_buf("mesh_aes_cbc_decrypt_receive_pack MD5:", once_md5, 16);
    // iot_communciate_aes_cbc_decrypt(in_data+6, in_data_len-6, out_Data, out_data_len, once_md5); //AES-CBC解密
    aes_cbc_decrypt_data(in_data+6, in_data_len-6, out_Data, out_data_len, once_md5);
    *out_data_len = len;
}

//获取加密后包总长度
uint16_t wlcc_get_encrypt_len(uint16_t len)
{
    uint16_t count = 0;
    if(len > 0)
    {
        if(len % 16 != 0) //有余数
        {
            count = (len/16+1) * 16;
        }
        else
        {
            count = len;
        }
        count += 6;//2字节长度 + 4字节的初始向量(随机数)
    }
    return count;
}

/**
 * @brief 使用AES_CBC加密数据
 * @param[in] src 待加密数据
 * @param[in] slen 待加密数据长度
 * @param[out] dst 加密后的密文数据
 * @param[out] olen 加密后的密文数据长度
 * @param[in] inv aes-cbc加密使用的初始向量
 */
static void aes_cbc_encrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *inv)
{
    uint16_t count = 0;
    uint8_t iv_buf[16] = {0};
    uint8_t aes_in[16] = {0};
    uint16_t aes_encrypt_len = 0;
    mbedtls_aes_context aes_ctx;
    memcpy(iv_buf, inv, sizeof(iv_buf));
    mbedtls_aes_init( &aes_ctx );

    mbedtls_aes_setkey_enc( &aes_ctx, aes_cbc_key, 256); //设置AES-CBC加密密钥256

    if(slen < 16) //不足16字节
    {
        memcpy(aes_in, src, slen);
        mbedtls_aes_crypt_cbc( &aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, aes_in, dst);
        aes_encrypt_len += 16;
    }
    else //超过16字节
    {
        count = slen/16;
        for(uint16_t i = 0; i < count; i++)
        {
            //AES-CBC加密，每次加密16字节
            mbedtls_aes_crypt_cbc( &aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, src + aes_encrypt_len, dst + aes_encrypt_len);
            memcpy(iv_buf, dst+aes_encrypt_len, sizeof(iv_buf)); //本次目标密文作为下一次AES-CBC加密的初始IV向量
            aes_encrypt_len += 16;
        }

        if(slen % 16 != 0) //最后不足16字节部分
        {
            memcpy(aes_in, src + aes_encrypt_len, slen % 16); //后面补充0
            mbedtls_aes_crypt_cbc( &aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, aes_in, dst + aes_encrypt_len);
            aes_encrypt_len += 16;
        }
        else //如果刚好16字节整数倍，则需要再加一个包
        {
            // mbedtls_aes_crypt_cbc( &aes_ctx, MBEDTLS_AES_ENCRYPT, 16, iv_buf, aes_in, dst+aes_encrypt_len);
            // aes_encrypt_len += 16;
        }
    }
    *olen = aes_encrypt_len;
    mbedtls_aes_free(&aes_ctx); //释放并清除指定的 AES 上下文
}

/**
 * @brief 使用AES_CBC解密数据
 * @param[in] src 待解密数据
 * @param[in] slen 待解密数据长度
 * @param[out] dst 解密后的明文数据
 * @param[out] olen 解密后的明文数据长度
 * @param[in] inv aes-cbc解密使用的初始向量
 */
static void aes_cbc_decrypt_data(const uint8_t *src, uint16_t slen, uint8_t *dst, uint16_t *olen, uint8_t *inv)
{
    mbedtls_aes_context aes_ctx;
    uint8_t iv_buf[16] = {0};
    size_t aes_decrypt_len=0;
    uint16_t count = slen/16;

    memcpy(iv_buf, inv, sizeof(iv_buf)); //复制第一次初始化IV向量
    mbedtls_aes_init(&aes_ctx);

    mbedtls_aes_setkey_dec( &aes_ctx, aes_cbc_key, 256); //设置AES-CBC解密密钥256

    for(uint16_t i = 0; i< count; i++)
    {
        //每一次解密16字节
        mbedtls_aes_crypt_cbc( &aes_ctx, MBEDTLS_AES_DECRYPT, 16, iv_buf, src+aes_decrypt_len, dst+aes_decrypt_len);
        memcpy(iv_buf, src+aes_decrypt_len, sizeof(iv_buf)); //本次源密文作为下一次AES-CBC解密的初始IV向量
        aes_decrypt_len += 16;
    }
    *olen = aes_decrypt_len;
    mbedtls_aes_free(&aes_ctx); //释放并清除指定的 AES 上下文
}

//计算MD5秘钥
static void md5_calculate_key(uint32_t value, uint8_t *out_md5)
{
    mbedtls_md5_context md5_ctx;
    uint8_t encrypt[4] = {0};

    encrypt[0] = (uint8_t)value;
    encrypt[1] = (uint8_t)(value>>8);
    encrypt[2] = (uint8_t)(value>>16);
    encrypt[3] = (uint8_t)(value>>24);
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);

    mbedtls_md5_update(&md5_ctx, encrypt, sizeof(encrypt));
    mbedtls_md5_finish(&md5_ctx, out_md5);
    mbedtls_md5_free(&md5_ctx);
}
