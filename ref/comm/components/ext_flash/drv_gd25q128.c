#include "drv_gd25q128.h"
#include "esp_log.h"

static const char *TAG = "[GD_DRV]";

esp_flash_t* flash_init_ext_flash(void)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = SPIFLASH_MOSI_GPIO_PIN,
        .miso_io_num = SPIFLASH_MISO_GPIO_PIN,
        .sclk_io_num = SPIFLASH_SCLK_GPIO_PIN,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
    };

    const esp_flash_spi_device_config_t device_config = {
        .host_id = SPIFLASH_HOST,
        .cs_id = 0,
        .cs_io_num = SPIFLASH_CS,
        .io_mode = SPI_FLASH_DIO,
        .freq_mhz = ESP_FLASH_5MHZ,//ESP_FLASH_40MHZ,        
    };

	esp_err_t ret;
    
    // Initialize the SPI bus
    if((ret = spi_bus_initialize(device_config.host_id, &bus_config, SPI_DMA_CH_AUTO)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize spi bus, error: %s!", esp_err_to_name(ret));
        return NULL;
    }

    // Add device to the SPI bus
    esp_flash_t* ext_flash;
    if((ret = spi_bus_add_flash_device(&ext_flash, &device_config)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add flash device to spi bus, error: %s!", esp_err_to_name(ret));
        return NULL;
    }

    // Probe the Flash chip and initialize it
    ret = esp_flash_init(ext_flash);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize external Flash: %s (0x%x)", esp_err_to_name(ret), ret);
        return NULL;
    }

    // Print out the ID and size
    uint32_t id;
    ret = esp_flash_read_id(ext_flash, &id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read external Flash ID: %s (0x%x)", esp_err_to_name(ret), ret);
        return NULL;
    }
    ESP_LOGI(TAG, "Initialized external flash, size=%lu KB, ID=0x%lx", ext_flash->size / 1024, id);

    return ext_flash;
}

const esp_partition_t* flash_add_partition(esp_flash_t* ext_flash)
{
    ESP_LOGI(TAG, "Adding external Flash as a partition, label=\"%s\", size=%lu KB", FLASH_PARTITION_NAME, ext_flash->size / 1024);
    const esp_partition_t* spiffs_partition;
    esp_err_t err = esp_partition_register_external(ext_flash, 0, ext_flash->size, FLASH_PARTITION_NAME, ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, &spiffs_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register flash to partition: %s (0x%x)", esp_err_to_name(err), err);
        return NULL;
    }
    return spiffs_partition;
}

