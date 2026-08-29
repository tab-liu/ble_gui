#ifndef __DRV_PCF8563_H__
#define __DRV_PCF8563_H__
#include <stdint.h>
#include <time.h>
#include "stdio.h"

#define DRV_PCF8563_VERSION 	"1.0.0"

//#define RTC_DBG
#define rtc_log(fmt, ...)		printf("%s: %s %d: ", __FILE__, __func__, __LINE__); \
								printf(fmt, ##__VA_ARGS__);

#define PCF8563_REG_ST1			0x00 /* status */
#define PCF8563_REG_ST2			0x01

#define PCF8563_REG_SC			0x02 /* datetime */
#define PCF8563_REG_MN			0x03
#define PCF8563_REG_HR			0x04
#define PCF8563_REG_DM			0x05
#define PCF8563_REG_DW			0x06
#define PCF8563_REG_MO			0x07
#define PCF8563_REG_YR			0x08

#define PCF8563_REG_AMN			0x09 /* alarm */
#define PCF8563_REG_AHR			0x0A
#define PCF8563_REG_ADM			0x0B
#define PCF8563_REG_ADW			0x0C

#define PCF8563_REG_CLKO		0x0D /* clock out */
#define PCF8563_REG_TMRC		0x0E /* timer control */
#define PCF8563_REG_TMR			0x0F /* timer */

#define PCF8563_SC_LV			0x80 /* low voltage */
#define PCF8563_MO_C			0x80 /* century */

#define TIME_STAMP_2023 1672531200	//2023.01.01 00:00:00
#define TIME_STAMP_2020 1577836800  //2020.01.01 00:00:00


/* pcf communication interface definition */
typedef struct pcf_i2c_client{
	int (*i2c_write)(uint8_t addr, uint8_t reg, uint8_t data, uint16_t len);
	int (*i2c_read)(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
	uint8_t write_addr;
	uint8_t read_addr;
}pcf_i2c_client_t;

int pcf8563_set_port(pcf_i2c_client_t *i2c_client);
int pcf8563_get_datetime(time_t *time);
int pcf8563_set_datetime(uint32_t time);

//void rtc_time_set(void);
//void rtc_time_read(void);

#endif


