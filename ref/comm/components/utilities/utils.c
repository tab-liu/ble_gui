#include "utils.h"

#include "esp_mac.h"
#include "esp_heap_caps.h"

#include <stddef.h>
#include <string.h>

void *iot_malloc(size_t size)
{
#if CONFIG_SPI_RAM_ENABLE
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

void *iot_calloc(size_t size)
{
#if CONFIG_SPI_RAM_ENABLE
    // 使用 heap_caps_calloc 分配内存并自动初始化为零
    return heap_caps_calloc(size, sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    // 使用 calloc 分配内存并自动初始化为零
    return calloc(size, sizeof(uint8_t));
#endif
}

void iot_free(void *ptr)
{
    if (NULL != ptr)
    {
        free(ptr);
        ptr = NULL;
    }
}

uint64_t iot_default_sn_by_mac(void)
{
    uint8_t mac[6] = {0};
    esp_err_t ret = esp_efuse_mac_get_default(mac);

    if (ret != ESP_OK) {
        /* 读取失败，回退到固定默认值 */
        return MASS_PRODUCTION_FALLBACK_SN;
    }

    /* 将完整 6 字节 MAC 地址组合为 64 位 SN
     * 高 16 位为 0，低 48 位为 MAC 地址 */
    uint64_t sn = ((uint64_t)mac[0] << 40) |
                  ((uint64_t)mac[1] << 32) |
                  ((uint64_t)mac[2] << 24) |
                  ((uint64_t)mac[3] << 16) |
                  ((uint64_t)mac[4] <<  8) |
                  ((uint64_t)mac[5]);
    
    sn = sn % MASS_PRODUCTION_SN_MODULO;

    /* 取模后若为 0，回退到默认值 */
    if (sn == 0ULL) {
        return MASS_PRODUCTION_FALLBACK_SN;
    }

    return sn;
}

