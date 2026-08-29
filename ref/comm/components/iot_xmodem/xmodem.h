#ifndef xmodem_h
#define xmodem_h
// #pragma once

#include <inttypes.h>

#define XMODEM_BLOCK_SIZE 1024

enum XMODEM_CONTROL_CHARACTERS {
	SOH = 0x01,
	EOT = 0x04,
	ACK = 0x06,		/*'R' */
	NACK = 0x15,	/*'N' */
	ETB = 0x17,
	CAN = 0x18,
	C = 0x43,
	STX = 0x02
};

typedef struct {
  uint8_t preamble;
  uint8_t id;
  uint8_t id_complement;
  uint8_t data[XMODEM_BLOCK_SIZE];
  uint16_t crc;
} xmodem_packet_t;

//uint8_t xmodem_verify_packet(const xmodem_packet_t packet, uint8_t expected_packet_id);
uint8_t xmodem_calculate_crc(const uint8_t * data, const uint32_t size, uint16_t * result);

#endif
