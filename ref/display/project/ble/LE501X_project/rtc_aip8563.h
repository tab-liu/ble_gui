#ifndef __RTC_AIP8563_H__
#define __RTC_AIP8563_H__

/*
 * The meaning of MO_C bit varies by the chip type.
 * From AIP8563 datasheet: this bit is toggled when the years
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

#include <time.h>

int rtc_time_init(void);
void rtc_time_deinit(void);
int rtc_time_get_datetime(time_t *time);
int rtc_time_set_datetime(time_t time);
void rtc_time_check_start(void);
void rtc_time_check_stop(void);

#endif // __RTC_AIP8563_H__
