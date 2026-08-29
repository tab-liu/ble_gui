#include "ext_flash.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "[EXT_FLASH]";

static esp_partition_t* extflash_partition = NULL;

int ext_flash_init(void)
{
	static uint8_t inited = 0;
	if(inited) return 0;

    // Set up SPI bus and initialize the external SPI Flash chip
    esp_flash_t* flash = flash_init_ext_flash();
    if (flash == NULL) {
        return -1;
    }

    // Add the entire external flash chip as a partition
    extflash_partition = flash_add_partition(flash);
    if ( extflash_partition == NULL ) {
        return -1;
    }

	inited = 1;
	return 0;
}

