#ifndef IMAGE_HANDLE_H
#define IMAGE_HANDLE_H

#include "ota_type.h"

#define JUMP_BYTES              20

image_info_t *get_image_info(void);
void set_image_invaild(void);
bool check_image_vaild(void);

bool image_flash_check(uint32_t address, int len);
bool image_flash_erase(uint32_t address, int len);
bool image_flash_Write(uint32_t address, const uint8_t *pdata, int len);
bool image_flash_abort(void);
bool image_trans_stop(void);
#endif
