#ifndef __APP_TIME_H__
#define __APP_TIME_H__
#include "esp_err.h"


#define RTC_I2C_MASTER_WRITE_ADDR		0xa2			/*!< I2C master write register address */
#define RTC_I2C_MASTER_READ_ADDR		0xa3			/*!< I2C master read register address */

#define APP_NTP_SYNC_PERIOD				(3600000)		/*!< ntp synchronization period */

#define ONE_MINUTE_INTERVAL    (60000)    // 1min

#define FIVE_MINUTE_INTERVAL    (300000)    // 5min
#define ONE_HOUR_INTERVAL       (3600000)   // 1hour
#define H24_HOUR_INTERVAL       (86400000)   // 24hour,=24*3600*1000

esp_err_t app_time_task(void);
esp_err_t app_sntp_init(void);
int time_sync_local_to_rtc(void);
esp_err_t time_sync_rtc_to_local(void);//read
void time_debug(void);
void rtc_have_new_set(void);
void time_have_new_set(void);
void RTC_Valid_Check(uint8_t id_index);
esp_err_t time_sync_to_local(void);
void app_sntp_set_timezone(int8_t hour, int8_t min);

#endif



