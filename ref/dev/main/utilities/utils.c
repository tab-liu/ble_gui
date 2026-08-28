#include "utils.h"

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
    }
}
