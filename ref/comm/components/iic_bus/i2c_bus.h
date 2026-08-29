#ifndef __I2C_BUS_H__
#define __I2C_BUS_H__
#include "driver/i2c.h"


#define  USE_GPIO_simulate_IIC//GPIO模拟I2C

/* I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_BUS_MASTER_NUM				0

#define I2C_BUS_MASTER_TIMEOUT_MS       1000
#define I2C_BUS_MASTER_SCL_IO           33      		/*!< GPIO number used for I2C master clock */
#define I2C_BUS_MASTER_SDA_IO           32      		/*!< GPIO number used for I2C master data  */
#define I2C_BUS_MASTER_FREQ_HZ          400000 			/*!< I2C master clock frequency */
#define I2C_BUS_MASTER_TX_BUF_DISABLE   0 				/*!< I2C master doesn't need buffer */
#define I2C_BUS_MASTER_RX_BUF_DISABLE   0 				/*!< I2C master doesn't need buffer */

int i2c_bus_init(void);
int i2c_bus_eep_write(uint8_t dev_addr, uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len);
int i2c_bus_eep_read(uint8_t dev_addr,  uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len);
int i2c_bus_rtc_write(uint8_t addr, uint8_t reg, uint8_t data, uint16_t len);
int i2c_bus_rtc_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);

#endif
