/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName: 
Author: weiyt
Date: 2022/10/20
Description: 功能：解析x509 CA证书加载公钥，解析PKCS#8加载私钥，实现rsa的公钥加密、rsa的私钥解密、rsa的私钥签名、rsa的公钥验签
Version: V1.00
Function List: none
***********************************************************/  
#include "iot_rsa.h"

#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "time.h"

#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/asn1.h"
#include "mbedtls/oid.h"
#include "esp_random.h"
#include "iot_partition.h"
#include "utils.h"

#define DAY     86400
#define HOUR    3600
#define MINU    60

// #define MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED -0x006E

static const char *TAG = "[rsa]";

mbedtls_ctr_drbg_context *ctr_drbg_ptr = NULL;//用于指示随机函数上下文的指针
mbedtls_rsa_context *rsa_ptr = NULL; //保存私钥结构体
mbedtls_rsa_context *public_rsa_ptr = NULL; //保存公钥结构体
mbedtls_entropy_context *entropy_ptr = NULL;
uint32_t cert_utc_sec;

// char test_data[] = {0x93,0x86,0x01,0x54,0x64,0x4C,0x99,0xBE,0xEA,0xF6,0x8A,0x57,0x43,0xF3,0xB3,0xAB,0x4F,0xC6,0x9E,0x4F,0x20,0x97,0xD7,0x33,0xCE,0x3B,0x65,0x40,0xE2,0x06,0xAE,0xE5,0x1A,0xAF,0xE1,0xE4,0x15,0x9E,0x08,0xA3,0x96,0x26,0xFD,0xC4,0x92,0x4F,0x85,0x2F,0x70,0x5C,0xE5,0x62,0xE3,0x9F,0x4E,0x81,0x53,0x7B,0x90,0x8A,0x35,0xA7,0x4F,0x2B,0x63,0x82,0xBE,0x9B,0x67,0x29,0x2E,0x02,0xEF,0x51,0x03,0xFA,0x83,0x45,0x28,0x55,0x88,0xDF,0x86,0x31,0x13,0x3C,0x87,0xA0,0x0B,0x70,0xD6,0xC8,0xF8,0x64,0xB2,0x23,0x63,0x61,0x1F,0x14,0x2B,0xAB,0x2E,0x6A,0x2A,0xC1,0x90,0xFF,0xB1,0xE7,0xCC,0xB6,0x6C,0x93,0xC8,0xD3,0x95,0x5C,0xDE,0xB5,0xD7,0x16,0x2B,0x9B,0xE0,0x72,0x77,0x81,0x64,0xD7,0x1C,0xCD,0x3E,0x6E,0x8A,0x83,0x11,0x69,0xA7,0xA3,0x17,0x11,0x49,0x5E,0x35,0x8B,0x22,0xFC,0x97,0x60,0xBF,0x4C,0xED,0x64,0x25,0x54,0x84,0x81,0xA8,0xE3,0x14,0xFD,0x0F,0xF1,0x67,0x8D,0xF4,0xF1,0x5F,0xE4,0xA4,0xFB,0xB3,0xBD,0x9F,0x5B,0x94,0xE7,0x15,0xC4,0xCC,0xA9,0x13,0xBA,0xC3,0xEE,0xD7,0xC4,0x4E,0x77,0x9F,0xAF,0xC7,0x04,0x8C,0xC0,0xB3,0xEA,0x86,0x5D,0x06,0x0F,0xB6,0x97,0x0A,0x09,0xD3,0x0B,0xD8,0x98,0x05,0x73,0x3E,0xB3,0xEA,0xB2,0xD0,0xD5,0x72,0x63,0x5B,0x3F,0x6B,0xE4,0x22,0xAD,0x31,0x1A,0x5B,0xA2,0x4A,0x32,0xAB,0xA6,0xAE};
// char sign_data[] = {0x73,0x25,0x63,0xE5,0x28,0x2A,0xB5,0xF5,0xC2,0x01,0xF0,0xEE,0x75,0x22,0x95,0x6C,
//                     0xB3,0x3C,0xF6,0xEB,0xBC,0xBA,0x7A,0xCE,0x90,0x69,0x08,0xDA,0x00,0xD0,0x2F,0x72,
//                     0x27,0x3E,0x1E,0xEE,0xA1,0x13,0xE8,0x4C,0x7E,0x24,0x4E,0x20,0x24,0xDD,0x0D,0xA7,
//                     0x92,0xF0,0x1E,0xEC,0x28,0x89,0x55,0x50,0x4C,0x99,0xFA,0xC2,0xD1,0x4D,0x95,0xDC,
//                     0xA5,0x3D,0x09,0x1D,0x7C,0xB5,0x51,0x9D,0x30,0xAD,0x85,0xDC,0xE4,0xA6,0xAF,0xD4,
//                     0x01,0xAB,0x4F,0x92,0x3E,0xEF,0x31,0x27,0x25,0xFE,0x21,0x00,0x5C,0x73,0x4C,0x59,
//                     0xBB,0x7D,0x04,0x65,0x84,0x7C,0x11,0xB1,0x4E,0x11,0x2A,0x72,0x53,0x66,0x6B,0x74,
//                     0x4F,0x82,0x78,0x1D,0xFE,0x1D,0x42,0x06,0x0B,0xBD,0x50,0x02,0x43,0x18,0x62,0x04,
//                     0x66,0x65,0x1C,0xD5,0x54,0x1F,0xAF,0x6B,0x64,0xA5,0x24,0x82,0xB1,0x44,0x56,0x5A,
//                     0xA9,0x05,0x62,0xCF,0xDD,0x04,0x75,0x3F,0xC9,0xC2,0x59,0xF9,0xE9,0x05,0x9C,0x3C,
//                     0x91,0x6E,0x90,0x4E,0xD1,0x20,0xCF,0x9A,0xE6,0x05,0xEC,0xE7,0xD5,0x59,0x30,0x6C,
//                     0xFE,0x6D,0x58,0x58,0x3A,0x67,0x8B,0x40,0x55,0x66,0x30,0x64,0x38,0x85,0xF6,0xC5,
//                     0xEB,0x8D,0x50,0xA0,0x70,0xDD,0x2C,0x08,0x57,0x95,0xE5,0xD9,0xBA,0x03,0xB3,0xB4,
//                     0x4B,0x4E,0xD9,0xCB,0x27,0x53,0xC5,0x87,0x2C,0xC7,0xC8,0x12,0xFF,0x5E,0x00,0xA0,
//                     0x51,0x76,0xA5,0x1E,0x80,0xA8,0x85,0x97,0xE5,0x9B,0x1B};


static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    mbedtls_printf("%s", info);
    for (int i = 0; i < len; i++) {
        mbedtls_printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

static void dump_rsa_key(mbedtls_rsa_context *ctx)
{
    size_t olen;
    uint8_t buf[516];
    mbedtls_printf("\n  +++++++++++++++++ rsa keypair +++++++++++++++++\n\n");
    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(N) , 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("N: %s\n", buf); 

    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(E) , 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("E: %s\n", buf);

    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(D) , 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("D: %s\n", buf);

    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(P) , 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("P: %s\n", buf);

    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(Q) , 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("Q: %s\n", buf);

    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(DP), 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("DP: %s\n", buf);

    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(DQ), 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("DQ: %s\n", buf);

    mbedtls_mpi_write_string(&ctx->MBEDTLS_PRIVATE(QP), 16, (char*)buf, sizeof(buf), &olen);
    mbedtls_printf("QP: %s\n", buf);
    mbedtls_printf("\n  +++++++++++++++++ rsa keypair +++++++++++++++++\n\n");
}
static int entropy_source(void *data, uint8_t *output, size_t len, size_t *olen)
{
    uint32_t seed;
    size_t offset = 0;
    size_t sum = len/4;

    for(size_t i = 0; i < sum; i++)
    {
        seed = esp_random(); //产生一个32位随机数
        memcpy(output+offset, &seed, 4);
        offset+=4;
    }
    if(len%4 != 0)
    {
        seed = esp_random(); //产生一个32位随机数
        memcpy(output+offset, &seed, 4);
        offset+=len%4;
    }
    *olen = offset;
    return 0;
}


//rsa随机数种子
void priv_rsa_refresh_seed(void)
{
    int ret = 1;
    const char* pers = "rsa_decrypt";
    memset(ctr_drbg_ptr, 0, sizeof(mbedtls_ctr_drbg_context)+1);
    memset(entropy_ptr, 0, sizeof(mbedtls_entropy_context)+1);
    mbedtls_ctr_drbg_init(ctr_drbg_ptr);
    mbedtls_entropy_init(entropy_ptr);
    //添加熵源接口，设置熵源属性
    mbedtls_entropy_add_source(entropy_ptr, entropy_source, NULL, MBEDTLS_ENTROPY_MAX_GATHER, MBEDTLS_ENTROPY_SOURCE_STRONG);
    ret = mbedtls_ctr_drbg_seed(ctr_drbg_ptr, mbedtls_entropy_func,entropy_ptr, (const unsigned char*)pers, strlen(pers));
    if (ret != 0)
    {
        mbedtls_printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n",ret);
        //goto exit;
    }
}

//提取RSA私钥
void extract_rsa_private_key(uint8_t *private_key)
{
    entropy_ptr = (mbedtls_entropy_context*)iot_calloc(sizeof(mbedtls_entropy_context)+1);
    ctr_drbg_ptr = (mbedtls_ctr_drbg_context*)iot_calloc(sizeof(mbedtls_ctr_drbg_context)+1);
    // mbedtls_rsa_context rsa;//D
    mbedtls_pk_context ctx_pk_ptr;

     mbedtls_pk_init(&ctx_pk_ptr);
	 
//mbedtls_psa_random_state
//    if (0 != mbedtls_pk_parse_key(&ctx_pk_ptr, private_key, strlen((char*)private_key) + 1, NULL, 0))
//	if (0 != mbedtls_pk_parse_key(&ctx_pk_ptr, private_key, strlen((char*)private_key) + 1, NULL, 0,mbedtls_psa_get_random, MBEDTLS_PSA_RANDOM_STATE))
	if (0 != mbedtls_pk_parse_key(&ctx_pk_ptr, private_key, strlen((char*)private_key) + 1, NULL, 0,mbedtls_ctr_drbg_random, ctr_drbg_ptr))
    {
        mbedtls_printf("\n  . Can't import private key\n");
    }
    else
    {
        // mbedtls_printf("\n  . Import private key successfully\n");
    }
    rsa_ptr = (mbedtls_rsa_context*)iot_calloc(sizeof(mbedtls_rsa_context));
    memset(rsa_ptr, 0, sizeof(mbedtls_rsa_context));
    memcpy(rsa_ptr, ctx_pk_ptr.MBEDTLS_PRIVATE(pk_ctx), sizeof(mbedtls_rsa_context));
    mbedtls_rsa_set_padding(rsa_ptr, MBEDTLS_RSA_PKCS_V15, 0);
}
//提取RSA公钥,避免总是解析X509证书
void extract_rsa_public_key(uint8_t *cert)
{
    uint16_t ca_len = 0;
    mbedtls_rsa_context *my_rsa;
    mbedtls_x509_crt cacert;
    ca_len = strlen((char*)cert)+1;
    mbedtls_x509_crt_init( &cacert );//初始化证书结构体
    if (mbedtls_x509_crt_parse(&cacert, (const unsigned char *) cert, ca_len) != 0) //解析CA X509证书
    {
        mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse \n\n");
    }
    else
    {
        //mbedtls_printf( " success parse x509\n \n\n");
        //dump_buf("pbulic key", cacert.pk_raw.p, cacert.pk_raw.len);
    }
    my_rsa = mbedtls_pk_rsa(cacert.pk);
    if (my_rsa == NULL)
    {
        mbedtls_printf( "mbedtls_pk_rsa error\n");
        goto exit;
    }

    public_rsa_ptr = (mbedtls_rsa_context*)iot_calloc(sizeof(mbedtls_rsa_context));
    if(public_rsa_ptr == NULL)
    {
        mbedtls_printf( "rsa_public_key malloc faild\n");
            goto exit;
    }
    memset(public_rsa_ptr, 0, sizeof(mbedtls_rsa_context));
    //mbedtls_rsa_init(public_rsa_ptr, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
    mbedtls_rsa_init(public_rsa_ptr);

    // mbedtls_printf("1 public_rsa_ptr:\n");
    // dump_rsa_key(public_rsa_ptr);//testwx

    mbedtls_rsa_copy(public_rsa_ptr, my_rsa); //复制公钥信息

    // if(rsa_public_key.key == NULL)
    // {
    //     rsa_public_key.key = (uint8_t*)iot_calloc((cacert.pk_raw.len+1)*sizeof(uint8_t));
    //     if(rsa_public_key.key == NULL)
    //     {
    //         mbedtls_printf( "rsa_public_key malloc faild\n");
    //         goto exit;
    //     }
    // }
    // memset(rsa_public_key.key, 0, (cacert.pk_raw.len+1));
    // memcpy(rsa_public_key.key, cacert.pk_raw.p, cacert.pk_raw.len);
    // rsa_public_key.len = cacert.pk_raw.len;
    /*释放资源*/
exit:
    mbedtls_x509_crt_free(&cacert); 
}

//导入CA公钥，用公钥加密
int cer_ras_encrypt(const unsigned char* plaintext, int plaintext_len, unsigned char* ciphertext)
{
    int ret = 0;
    int16_t success = 1;
    unsigned char plaintext_buffer[plaintext_len];

    char *b64_out = NULL;
    int b64_len = 0;
    int16_t sig_len = public_rsa_ptr->MBEDTLS_PRIVATE(len);//公钥模数长度 等于加密后的密文长度
    uint8_t sig_buff[sig_len];

    memcpy(plaintext_buffer, plaintext, plaintext_len);//
//    ret = mbedtls_rsa_pkcs1_encrypt(rsa_ptr, mbedtls_ctr_drbg_random, ctr_drbg_ptr, MBEDTLS_RSA_PUBLIC, plaintext_len, plaintext, ciphertext);
    ret = mbedtls_rsa_pkcs1_encrypt(public_rsa_ptr, mbedtls_ctr_drbg_random, ctr_drbg_ptr, plaintext_len, plaintext_buffer, sig_buff);
    if (ret != 0)
    {
        mbedtls_printf(" failed\n  ! mbedtls_rsa_pkcs1_encrypt returned %d\n\n", ret);
        success = 0;
        goto exit;
    }

    /***** base64编码 *****/
    // b64_out = iot_calloc(sig_len*2);//必须要*2！！！  进行base64解码后的长度会比原来的长1/3
    // ret = mbedtls_base64_encode((unsigned char *)b64_out, sig_len*2,
    //                         (size_t *)&b64_len, (unsigned char *)sig_buff, (size_t)sig_len);//ciphertext
    // strncpy((char *)ciphertext, b64_out, sig_len*2);
    // /***** base64编码 *****/
    // ESP_LOGW(TAG,"base64编码 ciphertext:%s",ciphertext);


    /***** 十六进制编码 *****/    
    // bin_to_hex(sig_buff, (size_t)sig_len);
    // strncpy((char *)ciphertext, (bin_to_hex(sig_buff, (size_t)sig_len)), sig_len);
    bin_to_hex(sig_buff, (size_t)sig_len, ciphertext);
    // strncpy((char *)ciphertext, (bin_to_hex(sig_buff, (size_t)sig_len)), sig_len);
    /***** 十六进制编码 *****/
    ESP_LOGD(TAG,"十六进制编码 ciphertext:%s",ciphertext);

exit:
    return success;
}

//加载私钥，rsa解密
int priv_rsa_decrypt(unsigned char* ciphertext, unsigned char* plaintext, uint16_t *plaintext_len)
{
    int ret = 1;
    size_t i;

    ret = mbedtls_rsa_pkcs1_decrypt(rsa_ptr, mbedtls_ctr_drbg_random, ctr_drbg_ptr, &i, ciphertext, plaintext, 224);
    if (ret != 0)
    {
        mbedtls_printf(" failed\n  ! mbedtls_rsa_pkcs1_decrypt returned %0x\n\n", -ret);    
        *plaintext_len = 0;
        //goto exit;
    }
    else
    {
        *plaintext_len = i;
    }
    
    return ret;
}


//私钥RSA签名
int priv_rsa_sign(unsigned char* plaintext, unsigned int plaintext_len, unsigned char* sign_output)
{
    unsigned char hash[128];//D 32
    int ret = 1;
    int16_t success = 1;
    char *b64_out = NULL;
    int b64_len = 0;
    
    mbedtls_rsa_context *rsa=NULL;//M904503 
    rsa = (mbedtls_rsa_context*)iot_calloc(sizeof(mbedtls_rsa_context));
    if(rsa == NULL)
    {
        mbedtls_rsa_free(rsa);
        mbedtls_printf(" malloc fail\n\n");
        return 0;
    }
ESP_LOGI(TAG,"sizeof(mbedtls_rsa_context) =%d \n",sizeof(mbedtls_rsa_context));
    memset(rsa, 0, sizeof(mbedtls_rsa_context));
    memcpy(rsa, rsa_ptr, sizeof(mbedtls_rsa_context));
    int16_t sig_len = rsa->MBEDTLS_PRIVATE(len);
    ESP_LOGI(TAG,"sig_len =%d \nplaintext_len:%d",sig_len,plaintext_len);
	
   uint8_t sig_buff[sig_len];
	

    //对需要签名的数据进行 sha256 计算，生成消息摘要数据 A
    // ret = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), plaintext, plaintext_len, reals.hash);//D904503
    // ESP_LOGI(TAG,"\n  . RSA pkcs1 sign ...:ret=%d \n",ret);
    // ESP_LOGI(TAG,"windy priv_rsa_sign 1:%s",reals.hash);
    // ESP_LOGI(TAG,"reals.hash[0]=%d \n",reals.hash[0]);
    // ESP_LOGI(TAG,"reals.hash[1]=%d \n",reals.hash[1]);
    // ESP_LOGI(TAG,"reals.hash[2]=%d \n",reals.hash[2]);
	
    // ESP_LOGI(TAG,"windy priv_rsa_sign 2:%s",(unsigned char*)rsa);
    // ret = mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V15,
    //                           MBEDTLS_MD_NONE);
    // if (ret == 0) 
	// {
	// 	//签名
	// 	 ret = mbedtls_rsa_pkcs1_sign(rsa, NULL, NULL, MBEDTLS_MD_SHA256,
	// 								(unsigned int)32, (const unsigned char *)&reals.hash[0], (unsigned char *)reals.sig_buff ) ;//20
	// 	if( (ret) != 0 )//sign_output			 , MBEDTLS_RSA_PRIVATE				
	// 	{
	// 		ESP_LOGE(TAG," failed\n  ! mbedtls_rsa_pkcs1_sign returned -0x%0x \n\n", (unsigned int) -ret );
	// 		success = 0;
	// 		goto exit;
	// 	}

    // }
    ret = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), plaintext, plaintext_len, hash);//D904503
    ESP_LOGI(TAG,"\n  . RSA pkcs1 sign ...:ret=%d \n",ret);

    //签名
    if( ( ret = mbedtls_rsa_pkcs1_sign(rsa, mbedtls_ctr_drbg_random, ctr_drbg_ptr, MBEDTLS_MD_SHA256,
                                32, hash, sig_buff ) ) != 0 )//sign_output    32是对应的sha256的固定长度
    // if( ( ret = mbedtls_rsa_pkcs1_sign(rsa, mbedtls_ctr_drbg_random, ctr_drbg_ptr, MBEDTLS_MD_SHA256,
    //                             128, hash, sig_buff ) ) != 0 )//sign_output                             
    {
        ESP_LOGE(TAG," failed\n  ! mbedtls_rsa_pkcs1_sign returned -0x%0x \n\n", (unsigned int) -ret );
        success = 0;
        goto exit;
    }

    // /***** base64编码 *****/
    // b64_out = iot_calloc(sig_len*2);//必须要*2！！！
    // ret = mbedtls_base64_encode((unsigned char *)b64_out, sig_len*2,
    //                             (size_t *)&b64_len, (unsigned char *)sig_buff, (size_t)sig_len);//sign_output
    // strncpy((char *)sign_output, b64_out, sig_len*2);
    // /***** base64编码 *****/

    /***** 十六进制编码 *****/    
    // bin_to_hex(sig_buff, (size_t)sig_len);
    // strncpy((char *)ciphertext, (bin_to_hex(sig_buff, (size_t)sig_len)), sig_len);
    bin_to_hex(sig_buff, (size_t)sig_len, sign_output);
    // strncpy((char *)ciphertext, (bin_to_hex(sig_buff, (size_t)sig_len)), sig_len);
    /***** 十六进制编码 *****/
    ESP_LOGD(TAG,"十六进制编码 sign_output:%s",sign_output);

exit:
    // mbedtls_printf("mbedtls_rsa_free:\n");
    // mbedtls_rsa_free(rsa);//D
    if(rsa != NULL)
    {
        mbedtls_printf("free ras\n");
        free(rsa);
    }   
    return success;
}



//公钥RSA验签
int ca_rsa_verify(unsigned char* plaintext, unsigned int plaintext_len, unsigned char* sign_output)
{
    int ret = 0;
    unsigned char hash[32];
    
    mbedtls_md(mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ), plaintext, plaintext_len, hash);
    printf( "\n  . RSA pkcs1 verify..." );
    
//    if( ( ret = mbedtls_rsa_pkcs1_verify( &rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC,
//                                  MBEDTLS_MD_SHA256, 20, hash, sign_output ) ) != 0 )
    if( ( ret = mbedtls_rsa_pkcs1_verify( rsa_ptr, MBEDTLS_MD_SHA256, 20, hash, sign_output ) ) != 0 )	
    {
        mbedtls_printf( " failed\n  ! mbedtls_rsa_pkcs1_verify returned -0x%0x\n\n", (unsigned int) -ret );
        goto exit;
    }

    mbedtls_printf( "\n  . OK (the signature is valid)\n\n" );

exit:  
   // mbedtls_rsa_free(&rsa);
    return 0;
}

//获取证书过期utc时间戳
uint32_t calculate_certificate_utc_sec(uint8_t *cert)
{
    struct tm tm_time;
    uint32_t utc_sec = 0;
    uint16_t ca_len = 0;
    mbedtls_rsa_context *my_rsa;
    mbedtls_x509_crt cacert;
    ca_len = strlen((char*)cert)+1;
    mbedtls_x509_crt_init( &cacert );//初始化证书结构体
    if (mbedtls_x509_crt_parse(&cacert, (const unsigned char *) cert, ca_len) != 0) //解析CA X509证书
    {
        mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse \n\n");
        goto exit;
    }
    else
    {
        //mbedtls_printf( " success parse x509\n \n\n");
        //dump_buf("pbulic key", cacert.pk_raw.p, cacert.pk_raw.len);
    }
    // mbedtls_printf( "end time:year:%d,month:%d,day:%d\n,hour:%d,min:%d,sec:%d\n", cacert.valid_to.year, cacert.valid_to.mon, cacert.valid_to.day, cacert.valid_to.hour, cacert.valid_to.min, cacert.valid_to.sec);
    memset(&tm_time, 0, sizeof(struct tm));
    tm_time.tm_year = cacert.valid_to.year-1900;
    tm_time.tm_mon = cacert.valid_to.mon-1;
    tm_time.tm_mday = cacert.valid_to.day;
    tm_time.tm_hour = cacert.valid_to.hour;
    tm_time.tm_min = cacert.valid_to.min;
    tm_time.tm_sec = cacert.valid_to.sec;
    tm_time.tm_isdst = -1; //夏令时
    utc_sec = mktime(&tm_time); //转换为utc时间
    // mbedtls_printf( "\nutc_sec:%lu\n", utc_sec);

    exit:
    mbedtls_x509_crt_free(&cacert);
    return utc_sec;
}

uint32_t get_certificate_utc_sec(void)
{
    return cert_utc_sec;
}

//公钥RSA验签bin文件
int ca_rsa_verify_ota_bin(uint32_t bin_len, unsigned char* sign_output)
{
    int ret = 0;
    unsigned char hash[32];
    
    iot_sha256_bin(mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ), bin_len, hash); //计算sha256
    dump_buf("hash", hash, sizeof(hash));
    printf( "\n  . RSA pkcs1 verify bin..." );
    
    //rsa验签
//    if( ( ret = mbedtls_rsa_pkcs1_verify( rsa_ptr, NULL, NULL, MBEDTLS_RSA_PUBLIC,
//                                  MBEDTLS_MD_SHA256, 20, hash, sign_output ) ) != 0 )
	if( ( ret = mbedtls_rsa_pkcs1_verify( rsa_ptr,MBEDTLS_MD_SHA256, 20, hash, sign_output ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_rsa_pkcs1_verify returned -0x%0x\n\n", (unsigned int) -ret );
        goto exit;
    }

    mbedtls_printf( "\n  . OK (the signature is valid)\n\n" );

exit:  
    //mbedtls_rsa_free(&rsa);
    return ret;
}

//公钥RSA验签bin文件
int ca_rsa_verify_other_device_bin(uint32_t address, uint32_t bin_len, unsigned char* sign_output)
{
    int ret = 0;
    unsigned char hash[32];
    
    other_device_sha256_bin(address, mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 ), bin_len, hash);
    dump_buf("hash", hash, sizeof(hash));
    printf( "\n  . RSA pkcs1 verify bin..." );
    
    //rsa验签
//    if( ( ret = mbedtls_rsa_pkcs1_verify( rsa_ptr, NULL, NULL, MBEDTLS_RSA_PUBLIC,
//                                  MBEDTLS_MD_SHA256, 20, hash, sign_output ) ) != 0 )
	if( ( ret = mbedtls_rsa_pkcs1_verify( rsa_ptr, MBEDTLS_MD_SHA256, 20, hash, sign_output ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_rsa_pkcs1_verify returned -0x%0x\n\n", (unsigned int) -ret );
        goto exit;
    }

    mbedtls_printf( "\n  . OK (the signature is valid)\n\n" );

exit:  
    //mbedtls_rsa_free(&rsa);
    return ret;
}

//RSA公钥和私钥初始化
void rsa_key_init(void)
{
    if(private_key_ptr != NULL) {
        extract_rsa_private_key(private_key_ptr);
        priv_rsa_refresh_seed();
    }

    //extract_rsa_public_key(ca_cert_ptr); //解析x509证书，获取RSA公钥 //A904503
    extract_rsa_public_key(iot_cert_ptr); //解析x509证书，获取RSA公钥 //testwx 设备证书

    if(iot_cert_ptr != NULL) {
        cert_utc_sec = calculate_certificate_utc_sec(iot_cert_ptr);
    }
}

//将PEM格式的证书解码成可读取的十六进制数————序列号解析
uint16_t parse_certificate_pem_to_hex(uint8_t *cert, char *SerialNumber)
{
    uint16_t ca_len = strlen((char*)cert)+1;
    char serial_number[32]; // 存储整个证书十六进制数的数组 20
    char serial_number_buffer[32]; // 存储整个证书十六进制数的数组 20
    size_t serial_number_len = sizeof(serial_number);
    mbedtls_x509_crt cacert;

    mbedtls_x509_crt_init( &cacert );//初始化证书结构体
    if (mbedtls_x509_crt_parse(&cacert, (const unsigned char *) cert, ca_len) != 0) //解析CA X509证书
    {
        ESP_LOGE(TAG," failed\n  !  mbedtls_x509_crt_parse \n\n");
        mbedtls_x509_crt_free(&cacert);
    }

    mbedtls_x509_serial_gets(serial_number_buffer, serial_number_len, &cacert.serial);
    
    //对解析出的序列号格式进行处理 去掉冒号、去掉首位0
    convert_mac_string(serial_number_buffer, serial_number);
    sprintf(SerialNumber,"%s",serial_number);

    return serial_number_len;
}

int get_certificate_common_name(uint8_t *cert, char *common_name, uint16_t cn_size)
{
    int ret;
    mbedtls_x509_crt cacert;
    mbedtls_x509_name *name;
    const char *cn_oid = MBEDTLS_OID_AT_CN; // Common Name OID
    
    mbedtls_x509_crt_init(&cacert);
    
    uint16_t ca_len = strlen((char*)cert) + 1;
    ret = mbedtls_x509_crt_parse(&cacert, (const unsigned char *)cert, ca_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse certificate: -0x%04x\n", -ret);
        mbedtls_x509_crt_free(&cacert);
        return 0;
    }
    
    // 遍历subject字段查找Common Name
    for (name = &cacert.subject; name != NULL; name = name->next) {
        // 检查OID是否匹配Common Name
        if (name->oid.len == 3 && 
            memcmp(name->oid.p, cn_oid, 3) == 0) {
            // 确保缓冲区足够大
            if (name->val.len >= cn_size) {
                mbedtls_x509_crt_free(&cacert);
                ESP_LOGE(TAG, "get certificate common name buffer too small");
                return 0; // 缓冲区太小
            }
            
            // 复制Common Name值
            memcpy(common_name, name->val.p, name->val.len);
            common_name[name->val.len] = '\0';
            ret = name->val.len;
            mbedtls_x509_crt_free(&cacert);
            return ret; // 成功
        }
    }
    
    ESP_LOGE(TAG, "not found certificate common name ");
    mbedtls_x509_crt_free(&cacert);
    return 0; // 未找到Common Name
}

uint16_t cer_ras_get_serial_number(char *SerialNumber)
{
    size_t serial_number_len = 0;
    serial_number_len = parse_certificate_pem_to_hex(iot_cert_ptr,SerialNumber);

    return serial_number_len;
} 

uint16_t cer_ras_get_common_name(char *common_name, uint16_t cn_size)
{
    size_t serial_number_len = 0;
    serial_number_len = get_certificate_common_name(iot_cert_ptr, common_name, cn_size);

    return serial_number_len;
}

//对解析出的序列号格式进行处理
void convert_mac_string(const char *input, char *output)
{
    int i = 0, j = 0;
    char current_hex[3]; // 用于存储每两个字符的临时数组
  
    // 遍历输入字符串
    while (i < strlen(input))
    {
        // 跳过冒号
        if (input[i] == ':')
        {
            i++;
            continue;
        }
          
        // 获取当前的两个十六进制字符  
        current_hex[0] = input[i++];
        current_hex[1] = input[i++];
        current_hex[2] = '\0';
          
        // 转换为大写并检查是否有前导零
        for (int k = 0; k < 2; k++)
        {
            if (current_hex[k] >= '0' && current_hex[k] <= '9')
            {
                if (k == 0 && current_hex[k] == '0' && current_hex[k + 1] >= '0' && current_hex[k + 1] <= '9')
                {
                    // 忽略首位零
                    if(i == 2)
                    {
                        continue;
                    }
                }
            }
            output[j++] = current_hex[k];
        }
    }

    // 添加字符串结束符
    output[j] = '\0';
}

// 将一个字节转换为两个16进制字符  
void byte_to_hex(unsigned char byte, char *hex)
{  
    sprintf(hex, "%02X", byte);  
}

// 将二进制数据转换为16进制编码的字符串  
void bin_to_hex(const unsigned char *bin_data, size_t bin_len, unsigned char *out_data)
{  
    char *hex_str = (char *)iot_calloc((bin_len * 2) + 1); // 每个字节需要两个字符，加上一个字符串结束符  

    for (size_t i = 0; i < bin_len; ++i)
    {  
        byte_to_hex(bin_data[i], hex_str + (i * 2));  
    }  

    hex_str[bin_len * 2] = '\0'; // 添加字符串结束符  
    memcpy(out_data, hex_str, (bin_len * 2) + 1);
    free(hex_str); // 释放内存 
}

