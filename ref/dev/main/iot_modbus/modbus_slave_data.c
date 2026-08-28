







////////////////////////////////////
////////////////////////////////////////uart透传，windy add
#include "freertos/FreeRTOS.h"
#include "modbus_data.h"
#include "can_data.h"
#include "stddef.h"
#include "esp_log.h"
#include "modbus_slave_data.h"
#include "comm_define.h"

#include "uart_device_process.h"

#define	TAG "[modbus_slave_data]"


static uint16_t FILL_VALUE = 0;

