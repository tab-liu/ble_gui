#include "ext_rtc.h"
#include "i2c_bus.h"

/**
 * @brief extend rtc initialization
 *
 * this function will set extend rtc i2c communication interface and install i2c driver.
 *
 * @return 0 if successful, otherwise -1
 */
int ext_rtc_init(void)
{
	/* set extended rtc communication interface */
	static pcf_i2c_client_t i2c_client = {
		.i2c_write = i2c_bus_rtc_write,
		.i2c_read = i2c_bus_rtc_read,
		.write_addr = RTC_I2C_MASTER_WRITE_ADDR >> 1,		/**< I2C drive requires a 7-bit address. */
		.read_addr = RTC_I2C_MASTER_READ_ADDR >> 1
	};
	pcf8563_set_port(&i2c_client);

	/* initialize i2c bus */
	assert(i2c_bus_init() == 0);
	return 0;
}

