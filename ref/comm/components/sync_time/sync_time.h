#pragma once

#include <time.h>
#include <sys/time.h>
#include "esp_err.h"
#include "inv_define.h"

#define RTC_I2C_MASTER_WRITE_ADDR   0xa2 /*!< I2C master write register address */
#define RTC_I2C_MASTER_READ_ADDR    0xa3  /*!< I2C master read register address */

#define APP_NTP_SYNC_PERIOD         (3600000) /*!< ntp synchronization period */

#define ONE_MINUTE_INTERVAL     (60000)    // 1min
#define FIVE_MINUTE_INTERVAL    (300000)    // 5min
#define TEN_MINUTE_INTERVAL     (600000)    // 10min
#define ONE_HOUR_INTERVAL       (3600000)   // 1hour
#define H4_HOUR_INTERVAL       (14400000)   //4hour
#define H24_HOUR_INTERVAL       (86400000)   // 24hour,=24*3600*1000

esp_err_t app_time_task(void);
esp_err_t app_sntp_init(void);
void app_sntp_set_timezone(int8_t hour, int8_t min);
int time_sync_local_to_arm(void);
esp_err_t time_sync_to_local(void);
void time_debug(void);
void system_time_init(void);
esp_err_t time_sync_system_to_local(uint8_t set_flag);
int time_sync_save_to_system(void);
void time_new_set_check(void);
void RTC_Valid_Check(uint8_t id_index);
bool parse_iso8601_to_tm_timestamp(const char *datetime_str, struct tm *tm_timestamp, time_res_struct *CurrTimeZone);
bool tm_time_to_timestamp(time_t *timestamp, struct tm *tm_time);
bool iCloud_Dst_Time_Check_and_Update(const char *datetime_str, uint16_t str_len);
time_t get_local_midnight(void);

