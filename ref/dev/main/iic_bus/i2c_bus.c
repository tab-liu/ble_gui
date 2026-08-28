#include "i2c_bus.h"
#include "twi_master.h"
#include "freertos/semphr.h"
#include <string.h>
#include "esp_log.h"

#define TAG "[I2C_DRV]"

static SemaphoreHandle_t i2c_mutex;					/**< i2c bus shared resource */

//#ifndef  USE_GPIO_simulate_IIC//不是GPIO模拟I2C
#if 0
/**
 * @brief eeprom write register
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_eep_write(uint8_t dev_addr, uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len)
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, dev_addr | I2C_MASTER_WRITE, true);
	if(addr_width == 16) {
		i2c_master_write_byte(cmd, (uint8_t)(mem_addr >> 8), true);
	}
	i2c_master_write_byte(cmd, (uint8_t)(mem_addr & 0xff), true);
	i2c_master_write(cmd, buf, len, true);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_BUS_MASTER_NUM, cmd, I2C_BUS_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	xSemaphoreGive(i2c_mutex);

	// printf("write addr_width:%d, ret:%d(%s), len:%d, data_addr:%x\n", addr_width, ret, esp_err_to_name(ret), len, mem_addr);
	if(ret != ESP_OK) return -1;
	return 0;
}

/**
 * @brief eeprom read register
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_eep_read(uint8_t dev_addr,  uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len)
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, dev_addr | I2C_MASTER_WRITE, true);
	if(addr_width == 16) {
		i2c_master_write_byte(cmd, (uint8_t)(mem_addr >> 8), true);
	}
	i2c_master_write_byte(cmd, (uint8_t)(mem_addr & 0xff), true);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, dev_addr | I2C_MASTER_READ, true);
	i2c_master_read(cmd, buf, len, I2C_MASTER_LAST_NACK);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_BUS_MASTER_NUM, cmd, I2C_BUS_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	xSemaphoreGive(i2c_mutex);

	// printf("read addr_width:%d, ret:%d(%s), len:%d, data_addr:%x\n", addr_width, ret, esp_err_to_name(ret), len, mem_addr);
	if(ret != ESP_OK) return -1;
	return 0;
}

/**
 * @brief extend rtc write register
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_rtc_write(uint8_t addr, uint8_t reg, uint8_t data, uint16_t len)
{
	uint8_t write_buf[2] = {reg, data};
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
	esp_err_t ret = i2c_master_write_to_device(I2C_BUS_MASTER_NUM, addr, write_buf, sizeof(write_buf),  pdMS_TO_TICKS(I2C_BUS_MASTER_TIMEOUT_MS));
	xSemaphoreGive(i2c_mutex);
	if(ret != ESP_OK) return -1;
	return 0;
}

/**
 * @brief extend rtc read register
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_rtc_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
	esp_err_t ret = i2c_master_write_read_device(I2C_BUS_MASTER_NUM, addr, &reg, 1, data, len, pdMS_TO_TICKS(I2C_BUS_MASTER_TIMEOUT_MS));
	xSemaphoreGive(i2c_mutex);
	if(ret != ESP_OK) return -1;
	return 0;
}

/**
 * @brief i2c bus initialization
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_init(void)
{
	static uint8_t inited = 0;
	if(inited) return 0;

	/* setup i2c driver */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_BUS_MASTER_SDA_IO,
        .scl_io_num = I2C_BUS_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_BUS_MASTER_FREQ_HZ,
    };

	i2c_param_config(I2C_BUS_MASTER_NUM, &conf);
	esp_err_t ret = i2c_driver_install(I2C_BUS_MASTER_NUM, conf.mode, I2C_BUS_MASTER_RX_BUF_DISABLE, I2C_BUS_MASTER_TX_BUF_DISABLE, 0);
	if(ret != ESP_OK)
	{
		ESP_LOGE(TAG, "i2c_driver_install failed, err: %s", esp_err_to_name(ret));
		return -1;
	}

	i2c_mutex = xSemaphoreCreateMutex();	/**< create i2c bus shared resource mutex */
	assert(i2c_mutex != NULL);

	inited = 1;
	return 0;
}
#else//GPIO模拟I2C

//GPIO模拟I2C，封装为两种接口，RTC和EEPROM

/**
 * @brief i2c bus initialization
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_init(void)//GPIO模拟I2C
{
	static uint8_t inited = 0;
	if(inited) return 0;

	if (twi_master_init() == false) return -1;

	i2c_mutex = xSemaphoreCreateMutex();	/**< create i2c bus shared resource mutex */
	assert(i2c_mutex != NULL);

	inited = 1;
	return 0;
}

/**
 * @brief extend rtc write register
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_rtc_write(uint8_t addr, uint8_t reg, uint8_t data, uint16_t len)//GPIO模拟I2C
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
	uint8_t buff[] = {reg, data};
	bool ret = twi_master_transfer((addr<<1)|TWI_WRITE_BIT, buff, sizeof(buff), TWI_ISSUE_STOP);
	if (!ret) twi_master_init();
	xSemaphoreGive(i2c_mutex);

	if (ret == false) return -1;
	return 0;
}

/**
 * @brief extend rtc read register
 *
 * @return error code, 0 if successful, otherwise -1
 */
int i2c_bus_rtc_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)//GPIO模拟I2C
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);//一直阻塞，直到成功
	bool ret = twi_master_transfer((addr<<1)|TWI_WRITE_BIT, &reg, 1, TWI_DONT_ISSUE_STOP);
	if (ret == true) {
		ret = twi_master_transfer((addr<<1)|TWI_READ_BIT, data, 1, TWI_ISSUE_STOP);
	}
	if (!ret) twi_master_init();
	xSemaphoreGive(i2c_mutex);

	if (ret == false) return -1;
	return 0;
}

/**
 * @brief eeprom write register
 *
 基础EEPROM I2C写函数
 * @return error code, 
 0 -successful,
 otherwise -1
 */
int i2c_bus_eep_write_0(uint8_t dev_addr, uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len)
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
	uint32_t i = 0;
	uint8_t buff[len+sizeof(mem_addr)];
	uint32_t size = sizeof(buff) - 1;
	if (addr_width == 16)
	{
		buff[i++] = (uint8_t)(mem_addr >> 8);
		size = sizeof(buff);
	}
	buff[i++] = (uint8_t)(mem_addr & 0xff);
	memcpy(&buff[i], buf, len);
	bool ret = twi_master_transfer(dev_addr|TWI_WRITE_BIT, buff, size, TWI_ISSUE_STOP);
	if (!ret) twi_master_init();
	xSemaphoreGive(i2c_mutex);

	if (ret == false)
	{
		ESP_LOGE(TAG, "eep write failed, dev_addr:%02x, addr_width:%d, mem_addr:%d, date_len:%ld, errno:%08lX",
				 dev_addr, addr_width, mem_addr, len, twi_get_errno());
		return -1;
	}
	return 0;
}

/*******************************************************************************
*  函数名称:		  i2c_bus_eep_write
*  输入参数: 无		 
*  输出参数: 无		 
*  函数功能:	   写入EEPROM操作，增加异常重试1次逻辑
rtn:
0-ok
1-fail
Frequency/Period: ms

******************************************************************************/
int i2c_bus_eep_write(uint8_t dev_addr,  uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len)
{
	uint16_t	ret=0xFF;
	ret=i2c_bus_eep_write_0( dev_addr,   addr_width,  mem_addr,  buf,  len);
	if(0 != ret)
	{
		vTaskDelay(pdMS_TO_TICKS(1));
		ret=i2c_bus_eep_write_0( dev_addr,   addr_width,  mem_addr,  buf,	len);
		if(0 != ret)
		{
			return 1;//fail
		}
	}	
	return ret;
}

/**
 * @brief eeprom read register
 *
 基础EEPROM I2C读取函数
 * @return error code, 
 0 -successful,
 otherwise -1
 */
int i2c_bus_eep_read_0(uint8_t dev_addr,  uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len)
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
	uint32_t i = 0;
	uint8_t buff[sizeof(mem_addr)];
	uint32_t size = sizeof(buff) - 1;
	if(addr_width == 16)
	{
		buff[i++] = (uint8_t)(mem_addr >> 8);
		size = sizeof(buff);
	}
	buff[i++] = (uint8_t)(mem_addr & 0xff);
	bool ret = twi_master_transfer(dev_addr|TWI_WRITE_BIT, buff, size, TWI_DONT_ISSUE_STOP);
	if (ret == true) {
		ret = twi_master_transfer(dev_addr|TWI_READ_BIT, buf, len, TWI_ISSUE_STOP);
	}
	if (!ret) twi_master_init();
	xSemaphoreGive(i2c_mutex);

	if (ret == false)
	{
		ESP_LOGE(TAG, "eep read failed, dev_addr:%02x, addr_width:%d, mem_addr:%d, date_len:%ld, errno:%lu",
				 dev_addr, addr_width, mem_addr, len, twi_get_errno());
		return -1;
	}
	return 0;
}

/*******************************************************************************
*  函数名称:		  i2c_bus_eep_read
*  输入参数: 无		 
*  输出参数: 无		 
*  函数功能:	   读取EEPROM操作，增加异常重试1次逻辑
rtn:
0-ok
1-fail
Frequency/Period: ms

******************************************************************************/
int i2c_bus_eep_read(uint8_t dev_addr,  uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len)
{
	uint16_t	ret=0xFF;
	ret=i2c_bus_eep_read_0( dev_addr,   addr_width,  mem_addr,  buf,  len);
	if(0 != ret)
	{
		vTaskDelay(pdMS_TO_TICKS(1));
		ret=i2c_bus_eep_read_0( dev_addr,   addr_width,  mem_addr,  buf,	len);
		if(0 != ret)
		{
			return 1;//fail
		}
	}	
	return ret;
}

#endif
