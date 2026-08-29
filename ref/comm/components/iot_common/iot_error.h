#pragma once

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码定义 */
#define IOT_OK          0       /*!< iot_err_t value indicating success (no error) */
#define IOT_FAIL        -1      /*!< Generic iot_err_t code indicating failure */

/* 系统级错误 */
#define IOT_ERR_NO_MEM                          0x101   /*!< Out of memory */
#define IOT_ERR_INVALID_ARG                     0x102   /*!< Invalid argument */
#define IOT_ERR_INVALID_STATE                   0x103   /*!< Invalid state */
#define IOT_ERR_INVALID_SIZE                    0x104   /*!< Invalid size */
#define IOT_ERR_NOT_FOUND                       0x105   /*!< Requested resource not found */
#define IOT_ERR_NOT_SUPPORTED                   0x106   /*!< Operation or feature not supported */
#define IOT_ERR_TIMEOUT                         0x107   /*!< Operation timed out */
#define IOT_ERR_INVALID_RIOTONSE                0x108   /*!< Received rIOTonse was invalid */
#define IOT_ERR_INVALID_CRC                     0x109   /*!< CRC or checksum was invalid */
#define IOT_ERR_INVALID_VERSION                 0x10A   /*!< Version was invalid */
#define IOT_ERR_INVALID_MAC                     0x10B   /*!< MAC address was invalid */
#define IOT_ERR_NOT_FINISHED                    0x10C   /*!< Operation has not fully completed */
#define IOT_ERR_NOT_ALLOWED                     0x10D   /*!< Operation is not allowed */

/* 各模块错误码 */
#define IOT_ERR_WIFI_BASE                       0x1000  /*!< Starting number of Wi-Fi error codes */
#define IOT_ERR_BLUETOOTH_BASE                  0x2000  /*!< Starting number of Buetooth error codes */

#ifdef __cplusplus
}
#endif

