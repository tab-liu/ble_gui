#ifndef CRC_H
#define CRC_H


#include <stdint.h>

#define reversal32( a,b,c,d )   ((uint32_t)(a<<24) | (b<<16) | (c<<8) | (d))

#define xm_calcu_crc16 			CalcCrc16_Xmodem//xmd_crc16_tlb
#define calcu_crc16 			CalcCrc16_modbus//modbus_crc16_tlb
#define crc16_modbus_calc 		CalcCrc16_modbus//modbus_crc16_tlb

uint16_t CalcCrc16_Xmodem(const uint8_t *pdata, uint16_t len);//xmd_crc16_tlb
uint16_t CalcCrc16_modbus(const uint8_t *pbBuf, uint16_t num); 
uint32_t calcu_crc32(uint32_t icrc, const uint8_t *data, int length);

#endif
