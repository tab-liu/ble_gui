/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_ota.h
Author: weiyt
Version: V1.0
Date: 2022.5.25
Description: none
*************************************************/

#ifndef IOT_OTA_H
#define IOT_OTA_H

#include "common.h"
#include "platform.h"
#include "ls_dbg.h"
#include "ls_ble.h"
#include "reg_base_addr.h"
#include "ls_hal_flash.h"
#include "log.h"
#include "prf_fotas.h"
// #include "lssha.h"

typedef struct ota_info {
    uint8_t check_image_flag;
    uint8_t state;
    uint8_t new_bin_flag;
    uint32_t current_addr; /*当前偏移地址*/
    uint32_t length; /*长度*/
    struct fota_image_info new_image;
} ota_info_t;

typedef struct ota_cache {
    uint8_t data[257]; /*缓存数组*/
    uint8_t res_len; /*剩余长度*/
    uint16_t offset; /*偏移*/
} ota_cache_t;

enum {
    OTA_SUCCESS = 0, /*正常*/
    OTA_OVER_SIZE, /*大小超出*/
    OTA_INVALID_FIREWARE, /*非法固件*/
    OTA_FAIL,
};

extern uint8_t iot_begin_ota(void);
extern uint8_t iot_end_ota(void);
extern uint8_t iot_ota_write(uint8_t *data, uint16_t len);
extern uint8_t iot_ota_abort(void);
extern void sha256_gen(uint32_t data_length);

#endif
