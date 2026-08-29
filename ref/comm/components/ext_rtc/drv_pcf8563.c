#include "drv_pcf8563.h"
#include <limits.h>
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

// #include "uart_device_process.h"
#include "sync_time.h"
#include "filesystem.h"
#include "comm_define.h"
#include "parameter.h"


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

/*
 * set rtc communication interface
 */
int pcf8563_set_port(pcf_i2c_client_t *i2c_client)
{
	if(!i2c_client) return -1;
	client = i2c_client;
	return 0;
}

/**
 * @brief get rtc time
 * when using rtc for the first time, it is necessary to first write a time,
 * otherwise an incorrect time may be read
 *
 * @param time time to get
output: 基于1970-1-1, 00:00:00以来经过的秒数
无时区偏移
 * @return 0 if success, otherwise -1
 */
int pcf8563_get_datetime(time_t *time)//time_t uint32_t
{
//	rtc_log("windy in  pcf8563_get_datetime 1\n");

	if(!client) return -1;

	uint8_t buf[9];
	for(int i = 0; i < sizeof(buf); i++)
	{
		if(client->i2c_read(client->read_addr, (PCF8563_REG_ST1+i), &buf[i], 1) < 0) {
			rtc_log("rtc read failed, addr=%02x\n", (PCF8563_REG_SC+i));
			return -1;
		}
	}

	if (buf[PCF8563_REG_SC] & PCF8563_SC_LV) {
		rtc_log("low voltage detected, date/time is not reliable.\n");
		return -1;
	}

#ifdef RTC_DBG
	rtc_log("raw registers data is st1=%02x, st2=%02x, sec=%02x, min=%02x, hr=%02x, "
			"mday=%02x, wday=%02x, mon=%02x, year=%02x\n",
			buf[0], buf[1], buf[2], buf[3], buf[4],
			buf[5], buf[6], buf[7], buf[8]);
#endif

	struct tm tm = {0};
	tm.tm_sec = bcd2bin(buf[PCF8563_REG_SC] & 0x7F);
	tm.tm_min = bcd2bin(buf[PCF8563_REG_MN] & 0x7F);
	tm.tm_hour = bcd2bin(buf[PCF8563_REG_HR] & 0x3F); /* rtc hr 0-23 */
	tm.tm_mday = bcd2bin(buf[PCF8563_REG_DM] & 0x3F);
	tm.tm_wday = buf[PCF8563_REG_DW] & 0x07;
	tm.tm_mon = bcd2bin(buf[PCF8563_REG_MO] & 0x1F) - 1; /*, rtc mn 1-12 */
	tm.tm_year = bcd2bin(buf[PCF8563_REG_YR]);
//	if (tm.tm_year < 70) {
//		tm.tm_year += 100;     /* assume we are in 1970...2069 */
//	}

	/* detect the polarity heuristically. see note above. */
//	c_polarity = (buf[PCF8563_REG_MO] & PCF8563_MO_C) ? (tm.tm_year >= 100) : (tm.tm_year < 100);
    c_polarity = (buf[PCF8563_REG_MO] & PCF8563_MO_C) ? 1 : 0;

    if(c_polarity == 0)
    {
        tm.tm_year += 100;  //年份：20xx
    }
//	tm.tm_year += 1900;

#ifdef RTC_DBG
	rtc_log("secs=%d, mins=%d, hours=%d, "
			"mday=%d, mon=%d, year=%d, wday=%d\n",
			tm.tm_sec, tm.tm_min, tm.tm_hour,
			tm.tm_mday, tm.tm_mon, tm.tm_year, tm.tm_wday);
#endif

	/* the clock can give out invalid datetime, but we cannot return
	 * -EINVAL otherwise hwclock will refuse to set the time on bootup.
	 */
	if (rtc_valid_tm(&tm) < 0)
	{
		rtc_log("retrieved date/time is not valid.\n");
		return -1;
	}
//	rtc_log("windy in  pcf8563_get_datetime 5\n");

	*time = mktime(&tm);
	return 0;
}

/**
 * @brief set rtc time
 * when using rtc for the first time, it is necessary to first write a time,
 * otherwise an incorrect time may be read
 *
 * @param time time to set
input: 基于1970-1-1, 00:00:00以来经过的秒数
 
无时区偏移 
 * @return 0 if success, otherwise -1
 */
int pcf8563_set_datetime(uint32_t time)
{
//return 0;//windy debug
	uint8_t buf[9];
	struct tm tm;
	reals.now =(time_t)time;

//	localtime_r((time_t *)&time, &tm);
	localtime_r(&reals.now, &tm);

#ifdef RTC_DBG
	rtc_log("secs=%d, mins=%d, hours=%d, "
			"mday=%d, mon=%d, year=%d, wday=%d\n",
			tm.tm_sec, tm.tm_min, tm.tm_hour,
			tm.tm_mday, tm.tm_mon, tm.tm_year, tm.tm_wday);
#endif

	if (rtc_valid_tm(&tm) < 0)
	{
		rtc_log("transfed date/time is not valid.\n");
		return -1;
	}

	/* hours, minutes and seconds */
	buf[PCF8563_REG_SC] = bin2bcd(tm.tm_sec);
	buf[PCF8563_REG_MN] = bin2bcd(tm.tm_min);
	buf[PCF8563_REG_HR] = bin2bcd(tm.tm_hour);

	/* days, 1 - 31 */
	buf[PCF8563_REG_DM] = bin2bcd(tm.tm_mday);

	/* month, 1 - 12 */
	buf[PCF8563_REG_MO] = bin2bcd(tm.tm_mon + 1);

	/* year, 00 - 99 */
	buf[PCF8563_REG_YR] = bin2bcd(tm.tm_year % 100);
//	buf[PCF8563_REG_YR] = bin2bcd((tm.tm_year -1900) % 100);

    if ((tm.tm_year) < 100)
    {
        buf[PCF8563_REG_MO] |= PCF8563_MO_C;    //年份：19xx
    }

	/* century */
//	if (c_polarity ? (tm.tm_year >= 100) : (tm.tm_year < 100)) {
//		buf[PCF8563_REG_MO] |= PCF8563_MO_C;
//	}

	/* weekdays, 0 - 6 */
	buf[PCF8563_REG_DW] = tm.tm_wday & 0x07;

	/* write register's data */
	for(int i = PCF8563_REG_SC; i < sizeof(buf); i++)
	{
		if(client->i2c_write(client->write_addr, i, buf[i], 1) < 0)
		{
			rtc_log("rtc write failed, addr=%02x, data=%02x\n", i, buf[i]);
			return -1;
		}
	}

	return 0;
}





















