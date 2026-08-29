#ifndef __DRV_24CXX_H__
#define __DRV_24CXX_H__
#include <stdint.h>
#include "drv_24cxx_conf.h"


#ifndef EEPROM_A0_PIN_STATE
	#define EEPROM_A0_PIN_STATE 0	/**< Address pin in device address */
#endif

#ifndef EEPROM_A1_PIN_STATE
	#define EEPROM_A1_PIN_STATE 0	/**< Address pin in device address */
#endif

#ifndef EEPROM_A2_PIN_STATE
	#define EEPROM_A2_PIN_STATE 0	/**< Address pin in device address */
#endif

/* Address pin bit in device address */
#define _A0PIN_BIT 1
#define _A1PIN_BIT 2
#define _A2PIN_BIT 3

#ifdef EEPROM_24C01
#if ((EEPROM_A2_PIN_STATE < 0) || (EEPROM_A1_PIN_STATE < 0) || (EEPROM_A0_PIN_STATE < 0))
	#error "A0 or A1 or A2 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A2_PIN_STATE << _A2PIN_BIT) | (EEPROM_A1_PIN_STATE << _A1PIN_BIT) | (EEPROM_A0_PIN_STATE << _A0PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      8U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    8U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  16U						/**< Number of memory page */
	#define _MEM_STWC         10U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C02)
#if ((EEPROM_A2_PIN_STATE < 0) || (EEPROM_A1_PIN_STATE < 0) || (EEPROM_A0_PIN_STATE < 0))
	#error "A0 or A1 or A2 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A2_PIN_STATE << _A2PIN_BIT) | (EEPROM_A1_PIN_STATE << _A1PIN_BIT) | (EEPROM_A0_PIN_STATE << _A0PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      8U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    8U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  32U						/**< Number of memory page */
	#define _MEM_STWC         10U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C04)
#if ((EEPROM_A2_PIN_STATE < 0) || (EEPROM_A1_PIN_STATE < 0))
	#error "A1 or A2 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A2_PIN_STATE << _A2PIN_BIT) | (EEPROM_A1_PIN_STATE << _A1PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      8U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    16U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  32U						/**< Number of memory page */
	#define _MEM_STWC         10U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C08)
#if (EEPROM_A2_PIN_STATE < 0)
	#error "A2 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A2_PIN_STATE << _A2PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      8U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    16U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  64U						/**< Number of memory page */
	#define _MEM_STWC         10U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C16)
	#define _DEV_ADDRESS      0xA0 /**< Memory IC Address */
	#define _MEMADD_SIZE      8U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    16U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  128U						/**< Number of memory page */
	#define _MEM_STWC         10U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C32)
#if ((EEPROM_A2_PIN_STATE < 0) || (EEPROM_A1_PIN_STATE < 0) || (EEPROM_A0_PIN_STATE < 0))
	#error "A0 or A1 or A2 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A2_PIN_STATE << _A2PIN_BIT) | (EEPROM_A1_PIN_STATE << _A1PIN_BIT) | (EEPROM_A0_PIN_STATE << _A0PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      16U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    32U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  128U						/**< Number of memory page */
	#define _MEM_STWC         10U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C64)
#if ((EEPROM_A2_PIN_STATE < 0) || (EEPROM_A1_PIN_STATE < 0) || (EEPROM_A0_PIN_STATE < 0))
	#error "A0 or A1 or A2 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A2_PIN_STATE << _A2PIN_BIT) | (EEPROM_A1_PIN_STATE << _A1PIN_BIT) | (EEPROM_A0_PIN_STATE << _A0PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      16U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    32U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  256U						/**< Number of memory page */
	#define _MEM_STWC         10U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C128)
#if ((EEPROM_A1_PIN_STATE < 0) || (EEPROM_A0_PIN_STATE < 0))
	#error "A0 or A1 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A1_PIN_STATE << _A1PIN_BIT) | (EEPROM_A0_PIN_STATE << _A0PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      16U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    64U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  256U						/**< Number of memory page */
	#define _MEM_STWC         5U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C256)
#if ((EEPROM_A1_PIN_STATE < 0) || (EEPROM_A0_PIN_STATE < 0))
	#error "A0 or A1Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A1_PIN_STATE << _A1PIN_BIT) | (EEPROM_A0_PIN_STATE << _A0PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      16U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    64U						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  512U						/**< Number of memory page */
	#define _MEM_STWC         5U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C512)
#if ((EEPROM_A1_PIN_STATE < 0) || (EEPROM_A0_PIN_STATE < 0))
	#error "A0 or A1Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A1_PIN_STATE << _A1PIN_BIT) | (EEPROM_A0_PIN_STATE << _A0PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      16U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    128UL						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  512UL						/**< Number of memory page */
	#define _MEM_STWC         5U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#elif defined(EEPROM_24C1024)
#if ((EEPROM_A2_PIN_STATE < 0) || (EEPROM_A1_PIN_STATE < 0))
	#error "A1 or A2 Pin state in not correct"
#endif
	#define _DEV_ADDRESS      (0xA0 | (EEPROM_A2_PIN_STATE << _A2PIN_BIT) | (EEPROM_A1_PIN_STATE << _A1PIN_BIT)) /**< Memory IC Address */
	#define _MEMADD_SIZE      16U						/**< Memory address size */
	#define _MEM_PAGE_SIZE    256UL						/**< Size of memory page */
	#define _MEM_NUM_OF_PAGE  512UL						/**< Number of memory page */
	#define _MEM_STWC         5U						/**< Self Timed Write Cycle */
	#define _MEM_MAX_SIZE     ((_MEM_PAGE_SIZE) * (_MEM_NUM_OF_PAGE)) /**< Maximum memory size */
#else
	#error [MEM ERROR01]Memory is not selected Or not supported.
#endif

#define _P0_SHIFT_VAL_MEMADD_SIZE_8BIT   7    /**< Value for memory address shift */
#define _P0_BIT_SEL_MEMADD_SIZE_8BIT     0x0E /**< Value for check P0 addressing bit in 8bit memory address mode */
#define _P0_SHIFT_VAL_MEMADD_SIZE_16BIT  15   /**< Value for memory address shift */
#define _P0_BIT_SEL_MEMADD_SIZE_16BIT    0x02 /**< Value for check P0 addressing bit in 16bit memory address mode */

/* Calculating new address for 8-bit memory addressing */
#define _CALC_DEVADD_8BIT(dev_addr, mem_addr) \
	((dev_addr) | (uint8_t)(((mem_addr) >> _P0_SHIFT_VAL_MEMADD_SIZE_8BIT) & _P0_BIT_SEL_MEMADD_SIZE_8BIT))

/* Calculating new address for 16-bit memory addressing */
#define _CALC_DEVADD_16BIT(dev_addr, mem_addr) \
	((dev_addr) | (uint8_t)(((mem_addr) >> _P0_SHIFT_VAL_MEMADD_SIZE_16BIT) & _P0_BIT_SEL_MEMADD_SIZE_16BIT))

/**
 * @brief eeprom_24cxx handle structure definition
 */
typedef struct eeprom_24cxx
{
	/* point to an iic_read function address
	 * for some chips, the dev_addr also includes a page address */
    int (*i2c_read)(uint8_t dev_addr, uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len);

	/* point to an iic_write function address
	 * for some chips, the dev_addr also includes a page address */
    int (*i2c_write)(uint8_t dev_addr, uint8_t addr_width, uint16_t mem_addr, uint8_t *buf, uint32_t len);

    void (*delay_ms)(uint32_t ms);				/**< point to a delay_ms function address */
    void (*print)(const char *const fmt, ...);	/**< point to a debug_print function address */

#if (EEPROM_WP_ENABLE > 0)
	void (*wp_enable)(uint8_t en);				/**< point to a write protection function address */
#endif

	void (*mutex_take)(void);
	void (*mutex_release)(void);

	uint8_t inited;								/**< inited flag ；1-已初始化，0-未初始化*/
} eeprom_24cxx_t;

/**
 * @brief     eeprom port init
 * @param[in] *handle points to a eeprom_24cxx_t struct
 * @return    none
 * @note      none
 */
void eeprom_port_init(eeprom_24cxx_t *handle);

/**
 * @brief     eeprom write
 * @param[in] address is the register address
 * @param[in] buf points to a data buffer
 * @param[in] len is the buffer length
 * @return    status code
 *            - 0 success
 *            - -1 write data failed
 * @note      none
 */
int eeprom_24cxx_write(uint32_t mem_addr, uint8_t *buf, uint32_t len);

/**
 * @brief      eeprom read
 * @param[in]  address is the register address
 * @param[out] buf points to a data buffer
 * @param[in]  len is the buffer length
 * @return     status code
 *             - 0 success
 *             - -1 read data failed
 * @note       none
 */
int eeprom_24cxx_read(uint32_t mem_addr, uint8_t *buf, uint32_t len);

#endif /* __EEPROM_24CXX_H__ */


