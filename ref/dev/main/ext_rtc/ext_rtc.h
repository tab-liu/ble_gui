#ifndef __EXT_RTC_INIT_H__
#define __EXT_RTC_INIT_H__
#include "drv_pcf8563.h"


#define RTC_I2C_MASTER_WRITE_ADDR		0xa2			/*!< I2C master write register address */
#define RTC_I2C_MASTER_READ_ADDR		0xa3			/*!< I2C master read register address */


int ext_rtc_init(void);

#endif
