#pragma once

#include "comm_define.h"

#define TLV_MAX_BLOCK_COUNT 10  // 最大TLV块数量，TODO:暂定10个,后续可以调整
#define TLV_MAX_VALUE_LENGTH 255 // 最大Value长度

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TLV单个块结构
 */
typedef struct {
    uint16_t addr;        // 地址（Addr），范围1-65535
    uint16_t length;     // 长度（Length），范围0-255
    const uint8_t *value; // 内容（Value）指针，不拥有内存
} wlcc_tlv_block_t;

/**
 * @brief TLV编码结果
 */
typedef struct {
    uint8_t *data;       // 编码后的数据
    uint16_t length;     // 编码后的总长度
} wlcc_tlv_encoded_t;

/**
 * @brief TLV信息单元对象解析结果
 * 
 */
typedef struct
{
    wlcc_tlv_block_t *blocks;
    uint16_t block_count;
} wlcc_tlv_info_unit_t;

/**
 * @brief TLV解析结果
 */
typedef struct {
    wlcc_tlv_block_t *blocks;  // TLV块数组
    uint16_t block_count;       // TLV块数量
} wlcc_tlv_decoded_t;

// 注意：TLV数据使用大端序（Big-Endian），与Python程序MyWlccTool保持一致
// 编码时：将主机字节序转换为大端序
// 解码时：将大端序转换为主机字节序

/**
 * @brief 编码TLV数据块到output缓冲区
 * @param blocks TLV块数组
 * @param block_count TLV块数量
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 编码后的字节数，失败返回负数
 */
int wlcc_tlv_encode(const wlcc_tlv_block_t *blocks,
                    uint16_t block_count,
                    uint8_t *output,
                    uint16_t output_size);

/**
 * @brief 解析TLV信息单元对象
 * @return IOT_OK成功，IOT_FAIL失败
 */
int wlcc_tlv_decode(const uint8_t *input,
                    uint16_t input_len,
                    wlcc_tlv_block_t *blocks,
                    uint16_t max_blocks,
                    uint16_t *parsed_blocks);

/**
 * @brief 计算TLV编码后的总长度
 * @param blocks TLV块数组
 * @param block_count TLV数量
 * @return 编码后的总字节数
 */
uint16_t wlcc_tlv_calculate_length(const wlcc_tlv_block_t *blocks, uint16_t block_count);

/**
 * @brief 生成模拟TLV数据
 * @return IOT_OK成功，IOT_FAIL失败
 * 
 * 生成3个TLV块
 * - Addr=1, Length=4, Value={0x01, 0x02, 0x03, 0x04}
 * - Addr=2, Length=2, Value={0xAA, 0xBB}
 * - Addr=100, Length=8, Value={0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}
 */
int wlcc_tlv_generate_demo_data(wlcc_tlv_block_t *blocks,
                                uint16_t *block_count,
                                uint8_t *value_buf,
                                uint16_t value_buf_size);

/**
 * @brief 检查数据是否为TLV格式的21000帧
 * @param data 待检查的数据（Modbus帧数据，不包括长度字段和CRC）
 * @param data_len 数据长度
 * @return IOT_OK表示是TLV格式，IOT_FAIL表示不是TLV格式
 * 
 * 检查规则：
 * 1. 最小长度检查：至少18字节（协议版本2B + 帧序号2B + 设备数量2B + 设备SN8B + 设备类型2B + 后续数据长度2B）
 * 2. 协议版本检查：必须是40002(上报)或40003(查询)，大端序为0x9C42或0x9C43
 * 3. 协议版本低字节检查：第二个字节必须是0x42或0x43（标准Modbus功能码最大是0x17，不会是0x42或0x43）
 */
int wlcc_tlv_is_tlv_frame(const uint8_t *data, uint16_t data_len);

#ifdef __cplusplus
}
#endif

