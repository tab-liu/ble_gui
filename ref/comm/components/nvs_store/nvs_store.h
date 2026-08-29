#ifndef __NVS_STORE_H__
#define __NVS_STORE_H__
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void nvs_store_init(void);
int nvs_store_write(const char *label, const uint8_t *data, size_t len);
int nvs_store_read(const char *label, uint8_t *data, size_t *len);
int nvs_store_erase(const char *label);

#ifdef __cplusplus
}
#endif

#endif

