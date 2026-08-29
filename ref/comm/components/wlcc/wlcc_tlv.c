/**
 * @brief TLV格式编码/解析实现
 * @author yuedan
 */
#include "wlcc_tlv.h"
#include "wlcc_common.h"
#ifdef ESP_LOG_MOCK
// In unit test environment, use mock esp_log
#include "esp_log_mock.h"
#else
// In normal build, use real esp_log
#include "esp_log.h"
#endif
#include <string.h>

#define TAG "[wlcc_tlv]"

/**
 * @brief 编码TLV数据块到缓冲区
 */
int wlcc_tlv_encode(const wlcc_tlv_block_t *blocks,
                    uint16_t block_count,
                    uint8_t *output,
                    uint16_t output_size)
{
    if (blocks == NULL || output == NULL || block_count == 0)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return IOT_ERR_INVALID_ARG;
    }

    if (block_count > TLV_MAX_BLOCK_COUNT)
    {
        ESP_LOGE(TAG, "Block count exceeds maximum(%d): %d", TLV_MAX_BLOCK_COUNT, block_count);
        return IOT_ERR_INVALID_SIZE;
    }

    uint16_t offset = 0;
    /**
 * 每个信息单元对象：地址(2Bytes) + 长度(2Bytes) + 内容
 */
    for (uint16_t i = 0; i < block_count; i++)
    {
        const wlcc_tlv_block_t *block = &blocks[i];

        if (block->addr < 1)
        {
            ESP_LOGE(TAG, "Invalid tlv block addr: %d (must be >= 1)", block->addr);
            return IOT_ERR_INVALID_ARG;
        }

        if (block->length > TLV_MAX_VALUE_LENGTH)
        {
            ESP_LOGE(TAG, "Invalid tlv block length: %d (must be 0-255)", block->length);
            return IOT_ERR_INVALID_SIZE;
        }

        uint16_t block_size = 4 + block->length; // 地址(2B) + 长度(2B) + 内容
        if (offset + block_size > output_size)
        {
            ESP_LOGE(TAG, "Output buffer overflow: need %d, have %d", offset + block_size, output_size);
            return IOT_ERR_INVALID_SIZE;
        }

        // 验证Value指针
        if (block->length > 0 && block->value == NULL)
        {
            ESP_LOGE(TAG, "Value pointer is NULL but length > 0");
            return IOT_ERR_INVALID_ARG;
        }

        // 编码Addr（2Bytes）
        output[offset++] = (uint8_t)(block->addr & 0xFF);
        output[offset++] = (uint8_t)(block->addr >> 8);
        
        // 编码Length（2Bytes）
        output[offset++] = (uint8_t)(block->length & 0xFF);
        output[offset++] = (uint8_t)(block->length >> 8);

        // 编码Value
        if (block->length > 0 && block->value != NULL)
        {
            memcpy(&output[offset], block->value, block->length);
            offset += block->length;
        }
    }

    return offset;
}

/**
 * @brief 解析TLV信息单元对象
 * @param input 输入缓冲区
 * @param input_len 输入缓冲区长度
 * @param blocks 输出TLV块数组（需要预先分配）
 * @param max_blocks 最大TLV块数量
 * @param parsed_blocks 实际解析的TLV块数量（输出参数）
 */
int wlcc_tlv_decode(const uint8_t *input,
                    uint16_t input_len,
                    wlcc_tlv_block_t *blocks,
                    uint16_t max_blocks,
                    uint16_t *parsed_blocks)
{
    if (input == NULL || blocks == NULL || parsed_blocks == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return IOT_ERR_INVALID_ARG;
    }

    if (max_blocks == 0 || max_blocks > TLV_MAX_BLOCK_COUNT)
    {
        ESP_LOGE(TAG, "Invalid max_blocks(must be 1~%d): %d", TLV_MAX_BLOCK_COUNT, max_blocks);
        return IOT_ERR_INVALID_ARG;
    }

    uint16_t offset = 0;
    uint16_t count = 0;

    // 循环解析TLV块
    while (offset < input_len && count < max_blocks)
    {
        // 地址和长度至少4字节
        if (offset + 4 > input_len)
        {
            ESP_LOGW(TAG, "Insufficient data for TLV header at offset %d", offset);
            break;
        }

        wlcc_tlv_block_t *block = &blocks[count];

        // 解析地址（2Bytes, 范围1~65535）
        block->addr = ((uint16_t)input[offset+1] << 8) | input[offset];
        offset += 2;
        if (block->addr < 1)
        {
            ESP_LOGE(TAG, "Invalid addr: %d at offset %d", block->addr, offset - 4);
            return IOT_ERR_INVALID_ARG;
        }

        // 解析长度（2Bytes）
        block->length = ((uint16_t)input[offset+1] << 8) | input[offset];
        offset += 2;
        if (block->length > TLV_MAX_VALUE_LENGTH)
        {
            ESP_LOGE(TAG, "Invalid length: %d at offset %d", block->length, offset - 2);
            return IOT_ERR_INVALID_SIZE;
        }

        // 检查是否有足够的字节读取Value
        if (offset + block->length > input_len)
        {
            ESP_LOGE(TAG, "Insufficient data for TLV value: need %d, have %d", 
                    block->length, input_len - offset);
            return IOT_ERR_INVALID_SIZE;
        }

        // 设置Value指针（指向输入缓冲区，不拷贝）
        if (block->length > 0)
        {
            block->value = &input[offset];
            offset += block->length;
        }
        else
        {
            block->value = NULL;
        }

        count++;
    }

    *parsed_blocks = count;
    return IOT_OK;
}

/**
 * @brief 计算TLV编码后的总长度
 */
uint16_t wlcc_tlv_calculate_length(const wlcc_tlv_block_t *blocks, uint16_t block_count)
{
    if (blocks == NULL || block_count == 0)
    {
        return 0;
    }

    uint16_t total_length = 0;

    for (uint16_t i = 0; i < block_count; i++)
    {
        // 地址(2B) + 长度(2B) + 内容
        total_length += 4 + blocks[i].length;
    }

    return total_length;
}

/**
 * @brief 生成模拟TLV数据
 * @param blocks 输出TLV块数组（需要预先分配，至少3个元素）
 * @param block_count 输出实际生成的TLV块数量
 * @param value_buf 输入值缓冲区（用于存储TLV块的值数据）
 * @param value_buf_size 输入值缓冲区大小
 */
int wlcc_tlv_generate_demo_data(wlcc_tlv_block_t *blocks,
                                uint16_t *block_count,
                                uint8_t *value_buf,
                                uint16_t value_buf_size)
{
    if (blocks == NULL || block_count == NULL || value_buf == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return IOT_ERR_INVALID_ARG;
    }

    if (value_buf_size < 20)
    {
        ESP_LOGE(TAG, "Value buffer too small: need at least 20 bytes");
        return IOT_ERR_INVALID_SIZE;
    }

    // 生成3个示例TLV块
    *block_count = 3;
    uint16_t value_offset = 0;

    // TLV块1: Addr=1, Length=4, Value={0x01, 0x02, 0x03, 0x04}
    blocks[0].addr = 1;
    blocks[0].length = 4;
    blocks[0].value = &value_buf[value_offset];
    value_buf[value_offset++] = 0x01;
    value_buf[value_offset++] = 0x02;
    value_buf[value_offset++] = 0x03;
    value_buf[value_offset++] = 0x04;

    // TLV块2: Addr=2, Length=2, Value={0xAA, 0xBB}
    blocks[1].addr = 2;
    blocks[1].length = 2;
    blocks[1].value = &value_buf[value_offset];
    value_buf[value_offset++] = 0xAA;
    value_buf[value_offset++] = 0xBB;

    // TLV块3: Addr=100, Length=8, Value={0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}
    blocks[2].addr = 100;
    blocks[2].length = 8;
    blocks[2].value = &value_buf[value_offset];
    value_buf[value_offset++] = 0x11;
    value_buf[value_offset++] = 0x22;
    value_buf[value_offset++] = 0x33;
    value_buf[value_offset++] = 0x44;
    value_buf[value_offset++] = 0x55;
    value_buf[value_offset++] = 0x66;
    value_buf[value_offset++] = 0x77;
    value_buf[value_offset++] = 0x88;

    ESP_LOGI(TAG, "Generated %d demo TLV blocks", *block_count);
    return IOT_OK;
}

/**
 * @brief 检查数据是否为TLV格式的21000帧
 * @param data 待检查的数据（Modbus帧数据，不包括长度字段和CRC）
 * @param data_len 数据长度
 * @return IOT_OK表示是TLV格式，IOT_FAIL表示不是TLV格式
 */
int wlcc_tlv_is_tlv_frame(const uint8_t *data, uint16_t data_len)
{
    const uint16_t TLV_MIN_FRAME_LENGTH = 8;
    
    if (data == NULL || data_len < TLV_MIN_FRAME_LENGTH){
        return IOT_FAIL;
    }

	// 先判断写，看后续读是否要判断
	if(0x10 != data[1]){
		return IOT_FAIL;
	}

	uint16_t regAdderss = data[2] << 8 | data[3];

	if(21000 != regAdderss){
		return IOT_FAIL;
	}
    
    // 检查协议版本（大端序）
    // 40002 = 0x9C42, 40003 = 0x9C43
    uint16_t protocol_ver = ((uint16_t)data[7] << 8) | data[8];
    
    // 检查协议版本是否为40002或40003
    if (protocol_ver != TLV_PROTOCOL_VERSION_REPORT && protocol_ver != TLV_PROTOCOL_VERSION_QUERY){
        return IOT_FAIL;
    }

	// TLV格式：协议版本(2B) + 帧序号(2B) + 设备数量(2B) + 设备SN(8B) + 设备类型(2B) + 后续数据长度(2B) = 18字节
        
    // 所有检查通过，确认为TLV格式
    return IOT_OK;
}
