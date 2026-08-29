#pragma once

#include <stdint.h>
#include <stddef.h>
#include "sdkconfig.h"

#define MASS_PRODUCTION_FALLBACK_SN   123456ULL
#define MASS_PRODUCTION_SN_MODULO     10000000000000ULL  /* 10^13，确保结果不超过13位 */

/* 不同产品的差异化配置, 目前的配置根据芯片型号识别 */
#if CONFIG_IDF_FIRMWARE_CHIP_ID == 0x0005   //CHIP_ESP32C3
#define CONFIG_SPI_RAM_ENABLE             0
#define USE_EXT_RAM_BSS
#elif CONFIG_IDF_FIRMWARE_CHIP_ID == 0x0009    //CHIP_ESP32S3
// mesh使用SPI RAM开关
#define CONFIG_SPI_RAM_ENABLE             1
#define USE_EXT_RAM_BSS     EXT_RAM_BSS_ATTR
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 申请堆内存
 * @param[in] size: 申请的堆大小(bytes)
 * @return 申请成功返回堆地址, 若失败则返回NULL
 */
void *iot_malloc(size_t size);
void *iot_calloc(size_t size);

/**
 * @brief 释放堆内存
 * @param[in] ptr: 待释放的堆地址
 */
void iot_free(void *ptr);

/**
 * @brief 默认SN（借用MAC地址）
 */
uint64_t iot_default_sn_by_mac(void);

#ifdef __cplusplus
}
#endif
