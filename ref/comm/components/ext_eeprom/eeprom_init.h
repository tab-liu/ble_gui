#pragma once

#include "drv_24cxx.h"
#include "i2c_bus.h"

#pragma pack(1)


#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif

int eeprom_init(void);
int eeprom_flag_read(uint16_t addr, uint16_t *flag);
int eeprom_flag_write(uint16_t addr, uint16_t flag);
int eeprom_data_read(uint16_t base_addr, uint8_t *data, uint16_t len);
int eeprom_data_write(uint16_t base_addr, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

