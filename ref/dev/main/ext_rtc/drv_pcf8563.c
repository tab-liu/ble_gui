#include "drv_pcf8563.h"
#include <limits.h>
#include "esp_log.h"
#include "iot_period_task.h"
#include <time.h>
#include <sys/time.h>
#include "app_time.h"


#define bcd2bin(x)	(((x) & 0x0f) + ((x) >> 4) * 10)	/* bcd to hex */
#define bin2bcd(x)	((((x) / 10) << 4) + (x) % 10)		/* hex_to_bcd */
#define TAG "[RTC-8563]"

/*
 * The meaning of MO_C bit varies by the chip type.
 * From PCF8563 datasheet: this bit is toggled when the years
 * register overflows from 99 to 00
 *   0 indicates the century is 20xx
 *   1 indicates the century is 19xx
 * From RTC8564 datasheet: this bit indicates change of
 * century. When the year digit data overflows from 99 to 00,
 * this bit is set. By presetting it to 0 while still in the
 * 20th century, it will be set in year 2000, ...
 * There seems no reliable way to know how the system use this
 * bit.  So let's do it heuristically, assuming we are live in
 * 1970...2069.
 */
static int c_polarity; /* 0: MO_C=1 means 19xx, otherwise MO_C=1 means 20xx */
static pcf_i2c_client_t *client;

static const uint8_t rtc_days_in_month[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int is_leap_year(uint32_t year)
{
	/* can be divided by 4 but not by 100, or can be divided by 400 */
	return (!(year % 4) && (year % 100)) || !(year % 400);
}

/*
 * The number of days in the month.
 */
static int rtc_month_days(uint32_t month, uint32_t year)
{
	return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

/*
 * Does the rtc_time represent a valid date/time?
 */
static int rtc_valid_tm(struct tm *tm)
{
	if (tm->tm_year < 70 ||
	    tm->tm_year > (2099 - 1900) || /* assume we are in 1970...2069 */
	    ((uint32_t)tm->tm_mon) >= 12 ||
	    tm->tm_mday < 1 ||
	    tm->tm_mday > rtc_month_days(tm->tm_mon, ((uint32_t)tm->tm_year + 1900)) ||
	    ((uint32_t)tm->tm_hour) >= 24 ||
	    ((uint32_t)tm->tm_min) >= 60 ||
	    ((uint32_t)tm->tm_sec) >= 60) {
		return -1;
	}
	return 0;
}
//
///**
// * @brief :时间读取
// 
// * @return 无
// */
// void rtc_time_read(void)
//{
//
//	/* 读RTC时间处理 */
//	uint32_t time = 0;
////	pcf8563_get_datetime(&time);
//	struct tm *tm = localtime((time_t *)&time);
//
//	/* RTC时间转换为自定义BETA格式时间 */
//	if (tm->tm_year < 100) 
//	{
//		reals.rtc_time.year = 0;
//	}
//	else 
//	{
//		reals.rtc_time.year = ((uint16_t)tm->tm_year + 1900 - 2000);
//	}
//	reals.rtc_time.mon = tm->tm_mon + 1;
//	reals.rtc_time.day = tm->tm_mday;
//	reals.rtc_time.hour = tm->tm_hour;
//	reals.rtc_time.min = tm->tm_min;
//	reals.rtc_time.sec = tm->tm_sec;
//
//	
//}
 /**
  * @brief :时间设置
  
  * @return 无
  */
 // void rtc_time_set(void)
 //{
 //  /* 自定义BETA格式时间转时间戳 */
 //  struct tm tm;
 //  tm.tm_year = reals.rtc_time.year + 2000 - 1900;
 //  if(reals.rtc_time.mon) 
 //  {
 // 	 tm.tm_mon = reals.rtc_time.mon - 1;
 //  }
 //  else 
 //  {
 // 	 tm.tm_mon = 0;
 //  }
 //  tm.tm_mday = reals.rtc_time.day;
 //  tm.tm_hour = reals.rtc_time.hour;
 //  tm.tm_min = reals.rtc_time.min;
 //  tm.tm_sec = reals.rtc_time.sec;
 //  uint32_t time = mktime(&tm);						 // 转换为时间戳
 //  pcf8563_set_datetime(time);						 // 更新RTC时间
 //}

 

 void system_time_init(void)
 {
	 /* when using extended rtc for the first time, it is necessary to first write a time,
	  * otherwise an incorrect time will be read. Here, a time is set when rtc reads an error or
	  * when the reading time is less than 2023 */
	 time_t rtc_time =0;
	//  if((pcf8563_get_datetime(&rtc_time)) < 0 || ((uint32_t)rtc_time < TIME_STAMP_2023))
	//  {
	// 	 if (pcf8563_set_datetime(TIME_STAMP_2023) != 0) {
	// 		 ESP_LOGE(TAG, "failed to set RTC time for the first time");
	// 	 }
	//  }
      //初始化时设置时区
     app_sntp_set_timezone(SetData.dev_info_t.SetTimeZone.hour, SetData.dev_info_t.SetTimeZone.min);
	 /* Synchronize RTC time to system time during system initialization */
	//  time_sync_rtc_to_local();
 
	 time_t now = time(NULL);
	 struct tm *tm = localtime((time_t *)&now);
	 if (tm->tm_year < 100)  // if the system time is less than 2000 years, set the system time to 2000 years
	 {

		 struct timeval tv_time = {
			 .tv_sec = TIME_STAMP_2020,
			 .tv_usec = 0
		 };
		 settimeofday(&tv_time, NULL);
	 }
 
	 /* print system current time */
	 now = time(NULL);
	 char buffer[80];
	 tm = localtime((time_t *)&now);
	 strftime(buffer, 80, "%Y-%m-%d %H:%M:%S %p %A",tm);
	 ESP_LOGI(TAG, "current formated system time:%s", buffer);
 }






















