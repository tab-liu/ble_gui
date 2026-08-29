#ifndef __DRV_24CXX_CONF_H_
#define __DRV_24CXX_CONF_H_

#include "comm_define.h"

/** 
 * Select EEPROM type. The available types are:
 *		EEPROM_24C01 
 *		EEPROM_24C02 
 *		EEPROM_24C04 
 *		EEPROM_24C08 
 *		EEPROM_24C16 
 *		EEPROM_24C32 
 *		EEPROM_24C64 
 *		EEPROM_24C128 
 *		EEPROM_24C256 
 *		EEPROM_24C512 
 *		EEPROM_24C1024
 */
 
/**
 * 选择 EEPROM 类型（根据 CONFIG_EEPROM_SIZE 数值，单位为 kbit）
 * 支持：1,2,4,8,16,32,64,128,256,512,1024
 * 未定义或不支持的值将默认使用 EEPROM_24C1024
 */
#ifdef CONFIG_EEPROM_SIZE
    
    #if CONFIG_EEPROM_SIZE == 1
        #define EEPROM_24C01
    #elif CONFIG_EEPROM_SIZE == 2
        #define EEPROM_24C02
    #elif CONFIG_EEPROM_SIZE == 4
        #define EEPROM_24C04
    #elif CONFIG_EEPROM_SIZE == 8
        #define EEPROM_24C08
    #elif CONFIG_EEPROM_SIZE == 16
        #define EEPROM_24C16
    #elif CONFIG_EEPROM_SIZE == 32
        #define EEPROM_24C32
    #elif CONFIG_EEPROM_SIZE == 64
        #define EEPROM_24C64
    #elif CONFIG_EEPROM_SIZE == 128
        #define EEPROM_24C128
    #elif CONFIG_EEPROM_SIZE == 256
        #define EEPROM_24C256
    #elif CONFIG_EEPROM_SIZE == 512
        #define EEPROM_24C512
    #elif CONFIG_EEPROM_SIZE == 1024
        #define EEPROM_24C1024
    #else
        #warning "Unsupported CONFIG_EEPROM_SIZE, default to EEPROM_24C1024"
        #define EEPROM_24C1024
    #endif
    
#else
    #define EEPROM_24C1024   /**< 默认 eeprom 类型 */
#endif

/** 
 * Set device address state:
 *		-1 - not use 
 *		0  - address state 0
 *		1  - address state 1
 */
#define EEPROM_A0_PIN_STATE 	(-1)	/**< eeprom address A0 state */
#define EEPROM_A1_PIN_STATE 	(0)		/**< eeprom address A1 state */
#define EEPROM_A2_PIN_STATE 	(1)		/**< eeprom address A2 state */

/** 
 * Set device write protection:
 *		0  - write protection disable
 *		1  - write protection enable
 */
#define EEPROM_WP_ENABLE		(0)		/**< eeprom enable write protection */


#endif /* __EEPROM_24CXX_CONF_H_ */


