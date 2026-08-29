#ifndef __IOT_ADC_BATTERY_H__
#define __IOT_ADC_BATTERY_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ls_soc_gpio.h"
#include "platform.h"

void adc_battery_check_init(void);
void adc_battery_check_deinit();
void adc_battery_check_start();
void adc_battery_check_stop();

#endif // __IOT_ADC_BATTERY_H__
