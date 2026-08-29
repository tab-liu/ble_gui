#pragma once

#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_chip_info.h"
#include "spi_flash_mmap.h"
#endif

#include "comm_define.h"

#ifndef CONFIG_SPIFLASH_MOSI_GPIO_PIN
#define CONFIG_SPIFLASH_MOSI_GPIO_PIN   GPIO_NUM_15
#endif

#ifndef CONFIG_SPIFLASH_MISO_GPIO_PIN
#define CONFIG_SPIFLASH_MISO_GPIO_PIN   GPIO_NUM_7
#endif

#ifndef CONFIG_SPIFLASH_SCLK_GPIO_PIN
#define CONFIG_SPIFLASH_SCLK_GPIO_PIN   GPIO_NUM_6
#endif

#ifndef CONFIG_SPIFLASH_CS_GPIO_PIN
#define CONFIG_SPIFLASH_CS_GPIO_PIN     GPIO_NUM_16
#endif

// Defines for GPIO port and pin for ESP32 SPI port.
#define SPIFLASH_HOST 					SPI3_HOST//SPI3_HOST//HSPI_HOST
#define SPIFLASH_MOSI_GPIO_PIN 			CONFIG_SPIFLASH_MOSI_GPIO_PIN
#define SPIFLASH_MISO_GPIO_PIN 			CONFIG_SPIFLASH_MISO_GPIO_PIN
#define SPIFLASH_SCLK_GPIO_PIN 			CONFIG_SPIFLASH_SCLK_GPIO_PIN

// Defines for spiflash spi cs
#define SPIFLASH_CS 					CONFIG_SPIFLASH_CS_GPIO_PIN

// partition name of ext flash in partition table
#define FLASH_PARTITION_NAME    		"extflash"

#pragma pack(1)


#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif

esp_flash_t* flash_init_ext_flash(void);
const esp_partition_t* flash_add_partition(esp_flash_t* ext_flash);

#ifdef __cplusplus
}
#endif

