#define LOG_TAG "RTC_TIME"

#include "compile_flag.h"
#include "ls_hal_i2c.h"
#include "ls_soc_gpio.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "ls_hal_timer.h"
#include "iot_iic.h"
#include "iot_timer.h"
#include "iot_magnetic_screen.h"
#include "rtc_aip8563.h"

static iot_timer_id_t rtc_timer_check_id = 0xFF;

/* I2C address */
#define AIP8563_I2C_ADDR 0x51

/* 寄存器地址长度和寄存器大小 */
#define AIP8563_REG_ADDR_LEN 0x01
#define AIP8563_REG_LEN 0x01

/* 寄存器列表 */
#define AIP8563_REG_ST1 0x00 /* status */
#define AIP8563_REG_ST2 0x01
#define AIP8563_REG_SC 0x02 /* datetime */
#define AIP8563_REG_MN 0x03
#define AIP8563_REG_HR 0x04
#define AIP8563_REG_DM 0x05
#define AIP8563_REG_DW 0x06
#define AIP8563_REG_MO 0x07
#define AIP8563_REG_YR 0x08
#define AIP8563_REG_AMN 0x09 /* alarm */
#define AIP8563_REG_AHR 0x0A
#define AIP8563_REG_ADM 0x0B
#define AIP8563_REG_ADW 0x0C
#define AIP8563_REG_CLKO 0x0D /* clock out */
#define AIP8563_REG_TMRC 0x0E /* timer control */
#define AIP8563_REG_TMR 0x0F /* timer */

/* status register 1 */
#define AIP8563_SC_LV 0x80 /* low voltage */
#define AIP8563_MO_C 0x80 /* century */

static int c_polarity; /* 0: MO_C=1 means 19xx, otherwise MO_C=1 means 20xx */

#define bcd2bin(x) (((x) & 0x0f) + ((x) >> 4) * 10) /* bcd to hex */
#define bin2bcd(x) ((((x) / 10) << 4) + (x) % 10) /* hex_to_bcd */

// 写入一个字节
static int i2c_bus_rtc_write(uint8_t addr, uint8_t reg, uint8_t data) {
    uint8_t buffer[2] = {reg, data};
    return iot_i2c_write_blocking(addr, buffer, 2);
}

static int i2c_bus_rtc_read(uint8_t addr, uint8_t reg, uint8_t *data) {
    iot_i2c_write_blocking(addr, &reg, 1);
    iot_i2c_read_blocking(addr, data, 1);
    return 0;
}

/* pcf communication interface definition */
typedef struct pcf_i2c_client {
    int (*i2c_write)(uint8_t addr, uint8_t reg, uint8_t data);
    int (*i2c_read)(uint8_t addr, uint8_t reg, uint8_t *data);
    uint8_t dev_addr;
} pcf_i2c_client_t;

static pcf_i2c_client_t client = {
    .dev_addr = AIP8563_I2C_ADDR,
    .i2c_write = i2c_bus_rtc_write,
    .i2c_read = i2c_bus_rtc_read,
};

static const uint8_t rtc_days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int is_leap_year(uint32_t year) {
    /* can be divided by 4 but not by 100, or can be divided by 400 */
    return (!(year % 4) && (year % 100)) || !(year % 400);
}

/*
 * The number of days in the month.
 */
static int rtc_month_days(uint32_t month, uint32_t year) {
    return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

/*
 * Does the rtc_time represent a valid date/time?
 */
static int rtc_valid_tm(struct tm *tm) {
    if (tm->tm_year < 70 || tm->tm_year > (2069 - 1900) || /* assume we are in 1970...2069 */
        ((uint32_t)tm->tm_mon) >= 12 || tm->tm_mday < 1
        || tm->tm_mday > rtc_month_days(tm->tm_mon, ((uint32_t)tm->tm_year + 1900))
        || ((uint32_t)tm->tm_hour) >= 24 || ((uint32_t)tm->tm_min) >= 60
        || ((uint32_t)tm->tm_sec) >= 60) {
        return -1;
    }
    return 0;
}

// 关闭RTC时钟输出，clk波形关闭
static int rtc_off_clk_output(void) {
    uint8_t value = 0;
    if (client.i2c_read(client.dev_addr, AIP8563_REG_CLKO, &value) < 0) {
        LOG_E("rtc read failed, addr=%02x", AIP8563_REG_CLKO);
        return -1;
    }

    // LOG_I("Read AIP8563_REG_CLKO, addr=%02x value=0x%x", AIP8563_REG_CLKO, value);
    if (value & 0x80) { // clock out 关闭设置为高阻抗
        value = 0;
        if (client.i2c_write(client.dev_addr, AIP8563_REG_CLKO, value) < 0) {
            LOG_E("rtc read failed, addr=%02x", AIP8563_REG_CLKO);
            return -1;
        }
        // LOG_I("Set AIP8563_REG_CLKO, addr=%02x value=0x%x", AIP8563_REG_CLKO, value);
    }
    return 0;
}

// 关闭RTC中断、闹钟功能
static int rtc_int_disable(void) {
    uint8_t value = 0;
    if (client.i2c_read(client.dev_addr, AIP8563_REG_ST2, &value) < 0) {
        LOG_E("rtc read failed, addr=%02x", AIP8563_REG_ST2);
        return -1;
    }

    // LOG_I("Read AIP8563_REG_ST2, addr=%02x value=0x%x", AIP8563_REG_ST2, value);
    if (value & 0x03) {
        value = 0; // 设置AIE和TIE为0，关闭定时中断和报警中断
        if (client.i2c_write(client.dev_addr, AIP8563_REG_ST2, value) < 0) {
            LOG_E("rtc read failed, addr=%02x", AIP8563_REG_ST2);
            return -1;
        }
        // LOG_I("Set AIP8563_REG_ST2, addr=%02x value=0x%x", AIP8563_REG_ST2, value);
    }
    return 0;
}

/**
 * @brief get rtc time
 * when using rtc for the first time, it is necessary to first write a time,
 * otherwise an incorrect time may be read
 *
 * @param time time to get
 *
 * @return 0 if success, otherwise -1
 */
int rtc_time_get_datetime(time_t *time) {
    uint8_t buf[9] = {0};
    // 检查低电压标志 - 对于没有备用电池的设计，这是正常现象
    if (buf[AIP8563_REG_ST1] & AIP8563_SC_LV) {
        LOG_W("Low voltage flag detected (expected without backup battery)");
        return -1;
    }

    for (int i = 0; i < sizeof(buf); i++) {
        if (client.i2c_read(client.dev_addr, (AIP8563_REG_ST1 + i), &buf[i]) < 0) {
            LOG_E("rtc read failed, addr=%02x", (AIP8563_REG_SC + i));
            return -1;
        }
    }

    // // 启用原始寄存器数据调试输出
    // LOG_I("raw registers data is st1=%02x, st2=%02x, sec=%02x, min=%02x, hr=%02x, "
    //       "mday=%02x, wday=%02x, mon=%02x, year=%02x",
    //       buf[0],
    //       buf[1],
    //       buf[2],
    //       buf[3],
    //       buf[4],
    //       buf[5],
    //       buf[6],
    //       buf[7],
    //       buf[8]);

    struct tm tm = {0};

    tm.tm_sec = bcd2bin(buf[AIP8563_REG_SC] & 0x7F);
    tm.tm_min = bcd2bin(buf[AIP8563_REG_MN] & 0x7F);
    tm.tm_hour = bcd2bin(buf[AIP8563_REG_HR] & 0x3F);
    tm.tm_mday = bcd2bin(buf[AIP8563_REG_DM] & 0x3F);
    tm.tm_wday = buf[AIP8563_REG_DW] & 0x07;
    tm.tm_mon = bcd2bin(buf[AIP8563_REG_MO] & 0x1F) - 1; /* rtc mn 1-12 */
    tm.tm_year = bcd2bin(buf[AIP8563_REG_YR]);
    if (tm.tm_year < 70) {
        tm.tm_year += 100; /* assume we are in 1970...2069 */
    }

    /* detect the polarity heuristically. see note above. */
    c_polarity = (buf[AIP8563_REG_MO] & AIP8563_MO_C) ? (tm.tm_year >= 100) : (tm.tm_year < 100);

    // LOG_D("parsed time - secs=%d, mins=%d, hours=%d, "
    //       "mday=%d, mon=%d, year=%d, wday=%d",
    //       tm.tm_sec,
    //       tm.tm_min,
    //       tm.tm_hour,
    //       tm.tm_mday,
    //       tm.tm_mon + 1,
    //       tm.tm_year + 1900,
    //       tm.tm_wday);

    /* the clock can give out invalid datetime, but we cannot return
     * -EINVAL otherwise hwclock will refuse to set the time on bootup.
     */
    if (rtc_valid_tm(&tm) < 0) {
        LOG_E("retrieved date/time is not valid.");
        return -1;
    }

    *time = mktime(&tm);
    // LOG_I("RTC time read successfully: %lu", *time);
    return 0;
}

/**
 * @brief set rtc time
 * when using rtc for the first time, it is necessary to first write a time,
 * otherwise an incorrect time may be read
 *
 * @param time time to set
 *
 * @return 0 if success, otherwise -1
 */
int rtc_time_set_datetime(time_t time) {
    uint8_t buf[9] = {0};

    struct tm tm;
    localtime_r(&time, &tm);

    // LOG_I("--> set: secs=%d, mins=%d, hours=%d, "
    //       "mday=%d, mon=%d, year=%d, wday=%d",
    //       tm.tm_sec,
    //       tm.tm_min,
    //       tm.tm_hour,
    //       tm.tm_mday,
    //       tm.tm_mon + 1,
    //       tm.tm_year + 1900,
    //       tm.tm_wday);

    if (rtc_valid_tm(&tm) < 0) {
        LOG_E("transfed date/time is not valid.");
        return -1;
    }

    /* hours, minutes and seconds */
    buf[AIP8563_REG_SC] = bin2bcd(tm.tm_sec); // 60
    buf[AIP8563_REG_MN] = bin2bcd(tm.tm_min); //
    buf[AIP8563_REG_HR] = bin2bcd(tm.tm_hour);

    /* days, 1 - 31 */
    buf[AIP8563_REG_DM] = bin2bcd(tm.tm_mday);

    /* month, 1 - 12 */
    buf[AIP8563_REG_MO] = bin2bcd(tm.tm_mon + 1);

    /* year, 00 - 99 */
    buf[AIP8563_REG_YR] = bin2bcd(tm.tm_year % 100);

    /* century */
    if (c_polarity ? (tm.tm_year >= 100) : (tm.tm_year < 100)) {
        buf[AIP8563_REG_MO] |= AIP8563_MO_C;
    }

    /* weekdays, 0 - 6 */
    buf[AIP8563_REG_DW] = tm.tm_wday & 0x07;

    /* write register's data */
    for (int i = AIP8563_REG_SC; i < sizeof(buf); i++) {
        if (client.i2c_write(client.dev_addr, i, buf[i]) < 0) {
            LOG_E("rtc write failed, addr=%02x, data=%02x", i, buf[i]);
            return -1;
        }
    }
    return 0;
}

static void rtc_timer_check_cb(void *arg) {
    // LOG_I("rtc_timer_check_cb");
    time_t current_time;
    if (rtc_time_get_datetime(&current_time) < 0) {
        LOG_W("RTC time invalid, setting default time");
        if (rtc_time_set_datetime(1704067200) < 0) {
            LOG_E("Failed to set default RTC time");
        } else {
            // LOG_I("Default RTC time set successfully");
        }
    }
    magnetic_screen_update_time(current_time);
}

int rtc_time_init(void) {
    // LOG_I("rtc_time_init");
    int state = 0;

    if (rtc_off_clk_output() < 0) {
        LOG_E("rtc_off_clk_output failed");
        state = -1;
    }

    if (rtc_int_disable() < 0) {
        LOG_E("rtc_int_disable failed");
        state = -1;
    }

    // 检查RTC是否需要初始化时间
    time_t current_time;
    if (rtc_time_get_datetime(&current_time) < 0) {
        LOG_W("RTC time invalid, setting default time");

        // 设置一个默认时间：2024年1月1日 00:00:00
        if (rtc_time_set_datetime(1704067200) < 0) {
            LOG_E("Failed to set default RTC time");
            state = -1;
        } else {
            // LOG_I("Default RTC time set successfully");
        }
    } else {
        // LOG_I("RTC time is valid, current timestamp: %lu", current_time);
    }

    rtc_timer_check_id = iot_timer_add_task_ms(1000, true, rtc_timer_check_cb, NULL);
    return state;
}

void rtc_time_deinit() {
    rtc_time_check_stop();
    iot_timer_remove_task(rtc_timer_check_id);
}

void rtc_time_check_start() {
    rtc_timer_check_cb(NULL);
    iot_timer_start_task(rtc_timer_check_id);
}

void rtc_time_check_stop() {
    iot_timer_stop_task(rtc_timer_check_id);
}
