#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define U64_MAX 0xFFFFFFFFFFFFFFFF
#define U32_MAX 0xFFFFFFFF
#define U16_MAX 0xFFFF

typedef union {
    uint64_t all;
    struct {
        uint64_t L1_Val_L8    : 8; // L1相值8位
        uint64_t L1_Val_M8 	: 8; // L1相值中8位
        uint64_t L1_Val_H4 	: 4; // L1相值高4位
        uint64_t L2_Val_L4    : 4; // L2相低4位
        uint64_t L2_Val_M8 	: 8; // L2相值中8位
        uint64_t L2_Val_H8 	: 8; // L2相值高8位
        uint64_t L3_Val_L8    : 8; // L1相值低8位
        uint64_t L3_Val_M8 	: 8; // L1相值中8位
        uint64_t L3_Val_H4 	: 4; // L1相值高4位
        uint64_t revd     : 4; //保留类型
    }bit;
}ThreePhaseValue_t;

int32_t float_to_int32(float f);
//大端模式获取8字节长度的数据
uint64_t  GetUin64FromPtrBig( const uint16_t* uBufPoint);
//小端模式获取8字节长度的数据
uint64_t  GetUin64FromPtrSmall( const uint16_t* uBufPoint);

int32_t decode_signed_explicit(uint32_t raw_value) ;

/**
 * @brief 将uint64_t SN码转换为十进制字符串
 * @param sn SN码值
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param zero_pad 是否用0填充到固定长度 (0表示不填充)
 * @return 成功返回指向buffer的指针，失败返回NULL
 */
char* sn_to_string_dec(uint64_t sn, char* buffer, size_t buffer_size, int zero_pad);

/**
 * @brief 合并两个字符串
 * @param str1 第一个字符串
 * @param str2 第二个字符串
 * @param sep 分隔符 (NULL表示不加)
 * @param out 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return 成功返回合并后的字符串指针，失败返回NULL
 */
char* str_join(const char* str1, const char* str2, const char* sep, char* out, size_t out_size);

int validate_version_n(uint32_t ver1, uint32_t ver2, int num);



int extract_three_phase_power_corrected(ThreePhaseValue_t* power_data, 
                                       int32_t *l1_val, 
                                       int32_t *l2_val, 
                                       int32_t *l3_val);

int32_t sign_extend_16to32_bit(uint32_t value) ;
#ifdef __cplusplus
}
#endif
