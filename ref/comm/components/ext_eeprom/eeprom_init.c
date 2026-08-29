#include <string.h>

#include "eeprom_init.h"
#include "freertos/semphr.h"

#include "comm_define.h"

#define TAG "[EXT_EEPROM]"

static SemaphoreHandle_t ext_eeprom_mutex;

static void eeprom_24cxx_delay(uint32_t ms)
{
	if(ms == 0) return;
	if(pdMS_TO_TICKS(ms) == 0) {
		vTaskDelay(1);
	}
	else {
		vTaskDelay(pdMS_TO_TICKS(ms));
	}
}

static void eeprom_24cxx_print(const char *const fmt, ...)
{
    char str[256];
    va_list args;

    memset((char *)str, 0, sizeof(char) * 256);
    va_start(args, fmt);
    vsnprintf((char *)str, 255, (char const *)fmt, args);
    va_end(args);

    printf("EEPROM print:%s\n", str);
}

static void eeprom_mutex_take(void)
{
	xSemaphoreTake(ext_eeprom_mutex, pdMS_TO_TICKS(100));
}

static void eeprom_mutex_release(void)
{
	xSemaphoreGive(ext_eeprom_mutex);
}

#if (EEPROM_WP_ENABLE > 0)
static void eeprom_24cxx_wp_enable(uint8_t en){}
#endif

int eeprom_init(void)
{
	ext_eeprom_mutex = xSemaphoreCreateMutex();
	assert(ext_eeprom_mutex != NULL);

    eeprom_24cxx_t eeprom_handle = {0};
	eeprom_handle.i2c_write = i2c_bus_eep_write;
	eeprom_handle.i2c_read = i2c_bus_eep_read;
	eeprom_handle.delay_ms = eeprom_24cxx_delay;
	eeprom_handle.print = eeprom_24cxx_print;
	eeprom_handle.mutex_take = eeprom_mutex_take;
	eeprom_handle.mutex_release = eeprom_mutex_release;
#if (EEPROM_WP_ENABLE > 0)
	eeprom_handle.wp_enable = eeprom_24cxx_wp_enable;
#endif

    // set eeprom interface
	eeprom_port_init(&eeprom_handle);	

	/* initialize i2c bus */
	assert(i2c_bus_init() == 0);
	return 0;
}


/*******************************************************************************
*  函数名称:		  
*  输入参数: 无		 
*  输出参数: 无		 
*  函数功能:	 使能EEPROM写操作
待填充硬件实际配置
Frequency/Period: ms

******************************************************************************/
static void Enable_Eeprom(void)//0-enable,允许写
{
//	EEPROM_WP_LOW;	
}

/*******************************************************************************
*  函数名称:		  
*  输入参数: 无		 
*  输出参数: 无		 
*  函数功能:	 禁止EEPROM写操作
待填充硬件实际配置
Frequency/Period: ms

******************************************************************************/
static void Disable_Eeprom(void)//1-disable ，禁止写
{
//	EEPROM_WP_HIGH;
}


/*******************************************************************************
*  函数名称:		  
*  输入参数: 无		 
*  输出参数: 无		 
*  函数功能:	   延时函数
待填充系统使用的延时函数：硬件延时或操作系统延时
注意操作系统初始化之前的延时仅能使用硬件延时

Frequency/Period: ms

******************************************************************************/
static void TimeDelayTop(uint32_t ms)
{
	vTaskDelay(pdMS_TO_TICKS(ms));
}

 /*******************************************************************************
 *	函数名称:		   
 *	输入参数: 无		  
 *	输出参数: 无		  
 *	函数功能:		看门狗喂狗函数
 Frequency/Period: ms
 
 ******************************************************************************/
 static void HARDDOG_Feed(void)
 {
    // 让出线程替代喂狗
	vTaskDelay(pdMS_TO_TICKS(1));
 }  

int eeprom_flag_read(uint16_t addr, uint16_t *flag)
{
    uint8_t buf[2] = {0xFF, 0xFF};
    int ret = eeprom_24cxx_read(addr, buf, 2);
    if (ret != 0) ret = eeprom_24cxx_read(addr, buf, 2);
    *flag = ((uint16_t)buf[1] << 8) | buf[0];
    return ret;
}

int eeprom_flag_write(uint16_t addr, uint16_t flag)
{
    Enable_Eeprom();
    uint8_t buf[2];
    buf[0] = flag & 0xFF;
    buf[1] = (flag >> 8) & 0xFF;
    int ret = eeprom_24cxx_write(addr, buf, 2);
	Disable_Eeprom();	
    return ret;
}

int eeprom_data_read(uint16_t base_addr, uint8_t *data, uint16_t len)
{
    uint16_t block_size = 16;
    uint16_t block_num = len / block_size;
    for (uint16_t ii = 0; ii < block_num; ii++) {
        int ret = eeprom_24cxx_read(base_addr + ii * block_size, data, block_size);
        if (ret != 0) {
            ret = eeprom_24cxx_read(base_addr + ii * block_size, data, block_size);
            if (ret != 0) return ret;
        }
        data += block_size;
        if ((ii % 64) == 0) HARDDOG_Feed();
    }
    HARDDOG_Feed();
    return 0;
}

int eeprom_data_write(uint16_t base_addr, uint8_t *data, uint16_t len)
{
    Enable_Eeprom();
    uint16_t block_size = 16;
    uint16_t block_num = len / block_size;
    for (uint16_t ii = 0; ii < block_num; ii++) {
        int ret = eeprom_24cxx_write(base_addr + ii * block_size, data, block_size);
        if (ret != 0) {
            ret = eeprom_24cxx_write(base_addr + ii * block_size, data, block_size);
            if (ret != 0) return ret;
        }
        data += block_size;
        if ((ii % 64) == 0) HARDDOG_Feed();
    }
    HARDDOG_Feed();
	Disable_Eeprom();	
    return 0;
}

