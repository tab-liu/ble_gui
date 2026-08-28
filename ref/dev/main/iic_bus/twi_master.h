/**
 * Copyright (c) 2009 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef TWI_MASTER_H
#define TWI_MASTER_H

/*lint ++flb "Enter library region" */

#include <stdbool.h>
#include <stdint.h>
#include "sys/unistd.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @file
* @brief Software controlled TWI Master driver.
*
*
* @defgroup lib_driver_twi_master Software controlled TWI Master driver
* @{
* @ingroup nrf_twi
* @brief Software controlled TWI Master driver (deprecated).
*
* @warning This module is deprecated.
*
* Supported features:
* - Repeated start
* - No multi-master
* - Only 7-bit addressing
* - Supports clock stretching (with optional SMBus style slave timeout)
* - Tries to handle slaves stuck in the middle of transfer
*/

#define TWI_READ_BIT 				(0x01)        	//!< If this bit is set in the address field, transfer direction is from slave to master.
#define TWI_WRITE_BIT 				(0x00)			//!< If this bit is set in the address field, transfer direction is from master to slave.

#define TWI_ISSUE_STOP 				((bool)true)  	//!< Parameter for @ref twi_master_transfer
#define TWI_DONT_ISSUE_STOP 		((bool)false) 	//!< Parameter for @ref twi_master_transfer

//ATS config
#define TWI_MASTER_SCL           	18 				//!< GPIO number used for I2C master clock
#define TWI_MASTER_SDA           	17 				//!< GPIO number used for I2C master data

/* These macros are needed to see if the slave is stuck and we as master send dummy clock cycles to end its wait */
/*lint -e717 -save "Suppress do {} while (0) for these macros" */
/*lint ++flb "Enter library region" */
#define TWI_SCL_HIGH()   			do { gpio_set_level(TWI_MASTER_SCL, 1); } while (0)		/*!< Pulls SCL line high */
#define TWI_SCL_LOW()    			do { gpio_set_level(TWI_MASTER_SCL, 0); } while (0)		/*!< Pulls SCL line low  */
#define TWI_SDA_HIGH()   			do { gpio_set_level(TWI_MASTER_SDA, 1); } while (0)		/*!< Pulls SDA line high */
#define TWI_SDA_LOW()    			do { gpio_set_level(TWI_MASTER_SDA, 0); } while (0)		/*!< Pulls SDA line low  */
#define TWI_SDA_INPUT()  			do { gpio_set_direction(TWI_MASTER_SDA, GPIO_MODE_INPUT_OUTPUT_OD); } while (0)	/*!< Configures SDA pin as input  */
#define TWI_SDA_OUTPUT() 			do { gpio_set_direction(TWI_MASTER_SDA, GPIO_MODE_OUTPUT_OD); } while (0)		/*!< Configures SDA pin as output */
#define TWI_SCL_OUTPUT() 			do { gpio_set_direction(TWI_MASTER_SCL, GPIO_MODE_INPUT_OUTPUT_OD); } while (0)	/*!< Configures SCL pin as output */
/*lint -restore */

#define TWI_SDA_READ() 				(gpio_get_level(TWI_MASTER_SDA))			/*!< Reads current state of SDA */
#define TWI_SDA_OUT_READ() 			(gpio_get_level(TWI_MASTER_SDA))			/*!< Reads current state of SDA */
#define TWI_SCL_READ() 				(gpio_get_level(TWI_MASTER_SCL))			/*!< Reads current state of SCL */
#define TWI_SCL_OUT_READ() 			(gpio_get_level(TWI_MASTER_SCL))			/*!< Reads current state of SCL */

#define TWI_DELAY() 				usleep(4) /*!< Time to wait when pin states are changed. For fast-mode the delay can be zero and for standard-mode 4 us delay is sufficient. */

#define TWI_ERR_STATCOND			(1 << 0)
#define TWI_ERR_ADDR_SCL			(1 << 1)
#define TWI_ERR_ADDR_ACK			(1 << 2)
#define TWI_ERR_DATA_SCL			(1 << 3)
#define TWI_ERR_DATA_ACK			(1 << 4)
#define TWI_ERR_DATA_IN_SCL			(1 << 5)
#define TWI_ERR_DATA_IN_ACK			(1 << 6)
#define TWI_ERR_STOPCOND			(1 << 7)
#define TWI_ERR_ADDR_NACK			(1 << 8)
#define TWI_ERR_DATA_NACK			(1 << 9)

/**
 * @brief Function for initializing TWI bus IO pins and checks if the bus is operational.
 *
 * Both pins are configured as Standard-0, No-drive-1 (open drain).
 *
 * @return
 * @retval true TWI bus is clear for transfers.
 * @retval false TWI bus is stuck.
 */
bool twi_master_init(void);

/**
 * @brief Function for transferring data over TWI bus.
 *
 * If TWI master detects even one NACK from the slave or timeout occurs, STOP condition is issued
 * and the function returns false.
 * Bit 0 (@ref TWI_READ_BIT) in the address parameter controls transfer direction;
 * - If 1, master reads data_length number of bytes from the slave
 * - If 0, master writes data_length number of bytes to the slave.
 *
 * @note Make sure at least data_length number of bytes is allocated in data if TWI_READ_BIT is set.
 * @note @ref TWI_ISSUE_STOP
 *
 * @param address Data transfer direction (LSB) / Slave address (7 MSBs).
 * @param data Pointer to data.
 * @param length Number of bytes to transfer.
 * @param issue_stop_condition If @ref TWI_ISSUE_STOP, STOP condition is issued before exiting function. If @ref TWI_DONT_ISSUE_STOP, STOP condition is not issued before exiting function. If transfer failed for any reason, STOP condition will be issued in any case.
 * @return
 * @retval true Data transfer succeeded without errors.
 * @retval false Data transfer failed.
 */
bool twi_master_transfer(uint8_t address, uint8_t * data, uint32_t length, bool issue_stop_condition);
uint32_t twi_get_errno(void);

/**
 *@}
 **/

/*lint --flb "Leave library region" */

#ifdef __cplusplus
}
#endif

#endif //TWI_MASTER_H
