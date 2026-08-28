#include "freertos/FreeRTOS.h"
#include "modbus_data.h"
#include "can_data.h"
#include "stddef.h"
#include "esp_log.h"
//#include "ParllelEMS.h"
#include "iot_mqtt.h"

#define	TAG "SCREEN_DATA"
//EXT_RAM_BSS_ATTR _SaveWebStormWatchApiData SaveWebStormWatchApiData = {0};


static uint16_t FILL_VALUE = 0;

