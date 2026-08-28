#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void wlcc_encrypt(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len);
void wlcc_decrypt(const uint8_t *in_data, uint16_t in_data_len, uint8_t *out_Data, uint16_t *out_data_len);
uint16_t wlcc_get_encrypt_len(uint16_t len);

#ifdef __cplusplus
}
#endif
