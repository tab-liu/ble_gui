#ifndef __UTIL_SWAP_H__
#define __UTIL_SWAP_H__
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UTIL_SWAP_16(N)     ((((uint16_t)(N) & 0xff00) >> 8) | \
                            (((uint16_t)(N) & 0x00ff) << 8))

#define UTIL_SWAP_32(N)     ((((uint32_t)(N) & 0xff000000) >> 24) | \
                            (((uint32_t)(N) & 0x00ff0000) >> 8) | \
                            (((uint32_t)(N) & 0x0000ff00) << 8) | \
                            (((uint32_t)(N) & 0x000000ff) << 24))


uint32_t swap_htonl(uint32_t host);
uint32_t swap_ntohl(uint32_t net);
uint16_t swap_htons(uint16_t host);
uint16_t swap_ntohs(uint16_t net);
void swap_buffer(uint8_t *buf, uint32_t len);
int swap_is_cpu_big_endian(void);

#ifdef __cplusplus
}
#endif

#endif

