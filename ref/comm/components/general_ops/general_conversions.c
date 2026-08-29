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
#include "general_conversions.h"


#define TAG "[general_conversions]"

/**/
int32_t float_to_int32(float f) {
    // 四舍五入到最近的整数（可选）
    return (int32_t)(f >= 0 ? f + 0.5f : f - 0.5f);
}

//大端模式获取8字节长度的数据
uint64_t  GetUin64FromPtrBig( const uint16_t* uBufPoint)
{
    return (uint64_t)((((uint64_t)uBufPoint[0])<<48)&0xffff000000000000)|( (((uint64_t)uBufPoint[1]<<32)&0xffff00000000))|((((uint64_t)uBufPoint[2]<<16)&0xffff0000))|(((uint64_t)uBufPoint[3]&0xffff));
} 

//小端模式获取8字节长度的数据
uint64_t  GetUin64FromPtrSmall( const uint16_t* uBufPoint)
{
    return (uint64_t)((((uint64_t)uBufPoint[3])<<48)&0xffff000000000000)|( (((uint64_t)uBufPoint[2]<<32)&0xffff00000000))|((((uint64_t)uBufPoint[1]<<16)&0xffff0000))|(((uint64_t)uBufPoint[0]&0xffff));
} 


int32_t decode_signed_explicit(uint32_t raw_value) 
{
    // 协议发送的是16位补码值
    uint16_t low16 = raw_value & 0xFFFF;
    
    // 判断是否为负数（16位符号位）
    if (low16 & 0x8000) {
        // 负数：低16位最高位为1
        // 先转换为int16_t（进行16位补码解释）
        // 再赋值给int32_t（自动符号扩展）
        return (int32_t)(int16_t)low16;
    } else {
        // 正数：直接转换
        return (int32_t)low16;
    }
}

/**
 * @brief 将uint64_t SN码转换为十进制字符串
 * @param sn SN码值
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param zero_pad 是否用0填充到固定长度 (0表示不填充)
 * @return 成功返回指向buffer的指针，失败返回NULL
 */
char* sn_to_string_dec(uint64_t sn, char* buffer, size_t buffer_size, int zero_pad) {
    if (!buffer || buffer_size == 0) {
        return NULL;
    }
    
    // 计算十进制字符串长度
    int needed_len = 1;
    uint64_t temp = sn;
    while (temp >= 10) {
        temp /= 10;
        needed_len++;
    }
    
    if (zero_pad > 0 && zero_pad > needed_len) {
        needed_len = zero_pad;
    }
    
    // 检查缓冲区是否足够
    if (needed_len + 1 > buffer_size) {  // +1 for null terminator
        return NULL;
    }
    
    // 填充前导零
    int pos = needed_len;
    buffer[pos] = '\0';
    
    do {
        buffer[--pos] = '0' + (sn % 10);
        sn /= 10;
    } while (pos > 0);
    
    // 如果还有位置，填充前导零
    while (pos > 0) {
        buffer[--pos] = '0';
    }
    
    return buffer;
}
/**
 * @brief 合并两个字符串
 * @param str1 第一个字符串
 * @param str2 第二个字符串
 * @param sep 分隔符 (NULL表示不加)
 * @param out 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return 成功返回合并后的字符串指针，失败返回NULL
 */
char* str_join(const char* str1, const char* str2, const char* sep, 
               char* out, size_t out_size) {
    if (!str1 || !str2 || !out || out_size == 0) {
        return NULL;
    }
    
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t sep_len = sep ? strlen(sep) : 0;
    
    if (len1 + sep_len + len2 + 1 > out_size) {
        return NULL;
    }
    
    char* ptr = out;
    
    // 复制第一个字符串
    memcpy(ptr, str1, len1);
    ptr += len1;
    
    // 复制分隔符
    if (sep_len > 0) {
        memcpy(ptr, sep, sep_len);
        ptr += sep_len;
    }
    
    // 复制第二个字符串
    memcpy(ptr, str2, len2);
    ptr += len2;
    
    *ptr = '\0';
    
    return out;
}
static int get_first_n_digits(uint32_t ver, int num) {
    if (num <= 0) return 0;
    if (ver < 0) ver = -ver;
    
    // 计算数字位数
    int temp = ver;
    int digit_count = 0;
    
    if (temp == 0) {
        digit_count = 1;
    } else {
        while (temp > 0) {
            temp /= 10;
            digit_count++;
        }
    }
     ESP_LOGI(TAG,"ver:%lu  digit_count:%d",ver,digit_count);
    // 如果位数小于等于n，直接返回
    if (digit_count <= num) {
        return ver;
    }
    
    // 计算除数
    int divisor = 1;
    for (int i = 0; i < digit_count - num; i++) {
        divisor *= 10;
    }
    ESP_LOGI(TAG,"ver:%lu / divisor:%d=%lu",ver,divisor,(ver / divisor));
    return ver / divisor;
}


/*------------------------------------------------------------------------------
 Function: validate_version_n
 -----------------------------------------------------------------------------*/
/**
  * @brief  通用版本号校验函数
*@param[in]uint32_t ver1, 版本号1
uint32_t ver2,版本号2 
int num比较数量
  * @param[out] 
  * @return    void 
  */
int validate_version_n(uint32_t ver1, uint32_t ver2, int num) 
{
    if(get_first_n_digits(ver1, num) == get_first_n_digits(ver2, num))
        return 1;
    else
        return 0;
}

/**
 * @brief 从inv_grid_power_t联合体中提取三相功率值（正确处理符号扩展）
 * @param val_data 输入的联合体数据
 * @param l1_val 输出L1相功率 (int32_t)
 * @param l2_val 输出L2相功率 (int32_t)
 * @param l3_val 输出L3相功率 (int32_t)
 * @return 保留类型 (0-3)
 */
int extract_three_phase_power_corrected(ThreePhaseValue_t* val_data, 
                                       int32_t *l1_val, 
                                       int32_t *l2_val, 
                                       int32_t *l3_val) {
    ESP_LOGI(TAG,"L1 h4:%lu,M8:%lu,L8:%lu",(uint32_t)val_data->bit.L1_Val_H4,(uint32_t)val_data->bit.L1_Val_M8,(uint32_t)val_data->bit.L1_Val_L8);
    ESP_LOGI(TAG,"l2 h4:%lu,M8:%lu,L8:%lu",(uint32_t)val_data->bit.L2_Val_H8,(uint32_t)val_data->bit.L2_Val_M8,(uint32_t)val_data->bit.L2_Val_L4);
    ESP_LOGI(TAG,"L3 h4:%lu,M8:%lu,L8:%lu",(uint32_t)val_data->bit.L3_Val_H4,(uint32_t)val_data->bit.L3_Val_M8,(uint32_t)val_data->bit.L3_Val_L8);
    ESP_LOGI(TAG,"revd:%lu",(uint32_t)val_data->bit.revd);
    // 提取L1相功率 (20位)
    uint32_t l1_raw = ((uint32_t)val_data->bit.L1_Val_H4 << 16) |
                      ((uint32_t)val_data->bit.L1_Val_M8 << 8) |
                      (uint32_t)val_data->bit.L1_Val_L8;
    
    // 提取L2相功率 (20位)
    uint32_t l2_raw = ((uint32_t)val_data->bit.L2_Val_H8 << 12) |
                      ((uint32_t)val_data->bit.L2_Val_M8 << 4) |
                      (uint32_t)val_data->bit.L2_Val_L4;
    
    // 提取L3相功率 (20位)
    uint32_t l3_raw = ((uint32_t)val_data->bit.L3_Val_H4 << 16) |
                      ((uint32_t)val_data->bit.L3_Val_M8 << 8) |
                      (uint32_t)val_data->bit.L3_Val_L8;

    // 符号扩展20位到32位
    // 检查第19位（0x80000 = 1<<19）是否为1，判断负数
    #define SIGN_20BIT_MASK 0x80000  // 20位有符号数的符号位掩码
    
    if (l1_raw & SIGN_20BIT_MASK) {
        *l1_val = (int32_t)(l1_raw | 0xFFF00000);  // 高位补1
    } else {
        *l1_val = (int32_t)l1_raw;  // 正数，高位已经是0
    }
    
    if (l2_raw & SIGN_20BIT_MASK) {
        *l2_val = (int32_t)(l2_raw | 0xFFF00000);  // 高位补1
    } else {
        *l2_val = (int32_t)l2_raw;
    }
    
    if (l3_raw & SIGN_20BIT_MASK) {
        *l3_val = (int32_t)(l3_raw | 0xFFF00000);  // 高位补1
    } else {
        *l3_val = (int32_t)l3_raw;
    }
    ESP_LOGI(TAG,"l1_raw:%lu l1_val:%ld,l2_raw:%lu l2_val:%ld,l3_raw:%lu l3_val:%ld",l1_raw,*l1_val,l2_raw,*l2_val,l3_raw,*l3_val);
    
    return val_data->bit.revd;
}

int32_t sign_extend_16to32_bit(uint32_t value) 
{
    uint16_t low16 = value & 0xFFFF;  // 取低16位
    if (low16 & 0x8000) {             // 检查第15位（符号位）
        // 负数：高16位补1
        return (int32_t)(low16 | 0xFFFF0000);
    } else {
        // 正数：高16位补0
        return (int32_t)low16;
    }
}
