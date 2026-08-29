#ifndef MODBUS_TCP_SLAVE_H
#define MODBUS_TCP_SLAVE_H

#include "modbus_tcp_common.h"
#include "tcp_server.h"
#include "tcps_server.h"
#include "modbus_data.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define MODBUS_TCP_AC_METER_SUPPORT 1


#define MODBUS_SLAVE_SUMMARY_ADDR 100

/* Modbus_Application_Protocol_V1_1b.pdf Chapter 4 Section 1 Page 5
 * TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes
 */
#define MODBUS_TCP_MAX_ADU_LENGTH 262

/* Modbus function codes */
#define MODBUS_FC_READ_COILS               0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS     0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS   0x03
#define MODBUS_FC_READ_INPUT_REGISTERS     0x04
#define MODBUS_FC_WRITE_SINGLE_COIL        0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER    0x06
#define MODBUS_FC_READ_EXCEPTION_STATUS    0x07
#define MODBUS_FC_WRITE_MULTIPLE_COILS     0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10
#define MODBUS_FC_REPORT_SLAVE_ID          0x11
#define MODBUS_FC_MASK_WRITE_REGISTER      0x16
#define MODBUS_FC_WRITE_AND_READ_REGISTERS 0x17

/* Modbus */
#define _MODBUS_TCP_DATA_OFFSET             6
#define _MODBUS_TCP_HEADER_LENGTH           7
#define _MODBUS_TCP_PRESET_REQ_LENGTH       12
#define _MODBUS_TCP_PRESET_RSP_LENGTH       8

#define _MODBUS_TCP_CHECKSUM_LENGTH         0

/* Protocol exceptions */
enum {
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 0x01,
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
    MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE,
    MODBUS_EXCEPTION_ACKNOWLEDGE,
    MODBUS_EXCEPTION_SLAVE_OR_SERVER_BUSY,
    MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE,
    MODBUS_EXCEPTION_MEMORY_PARITY,
    MODBUS_EXCEPTION_NOT_DEFINED,
    MODBUS_EXCEPTION_GATEWAY_PATH,
    MODBUS_EXCEPTION_GATEWAY_TARGET,
    MODBUS_EXCEPTION_MAX
};

/* 3 steps are used to parse the query */
typedef enum {
    _STEP_FUNCTION,
    _STEP_META,
    _STEP_DATA
} _step_t;

void modbus_tcp_start(uint8_t is_crypted, uint16_t port, uint8_t block);
void modbus_tcp_restart(uint8_t is_crypted, uint16_t port, uint8_t block);
bool modbus_tcp_is_running(void);
void modbus_tcp_task(void);
void modbus_tcp_stop(void);

#endif
