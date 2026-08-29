/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt\xiongf
Date: 2022/5/25
Description: OTA升级api、OTA验签
Version: V1.01
Function List: none
***********************************************************/

#define LOG_TAG "IOT_OTA"

/*说明：app的总空间从0x18035000-0x1807BFFF，共284KB，其中OTA1空间从0x18035000-0x180587FF，OTA2空间从0x18058800-0x1807BFFF
OTA成功后，重庆进入bootloader，先擦除OTA1空间，然后将固件从OTA2空间拷贝到OTA1空间，完成后，程序入口指向OTA1空间执行程序*/
#include "iot_ota.h"
#include "iot_bsp.h"
#include "iot_ble.h"
#include "tinycrypt/ecc_dsa.h"
#include "tinycrypt/sha256.h"
#include <string.h>
#include <stdio.h>
#include "ls_hal_timer.h"
#include "ls_soc_gpio.h"
#include "tinycrypt/sha256.h"
#include "uECC.h"
#include "ota_settings.h"

#define OTA_ADDR_START 0x18058800 /*OTA2起始地址*/
#define OTA_MAX_SIZE 145408 /*OTA2区总大小*/
#define CHIP_TYPE "LS5010_module" /*芯片类型*/
#define OTA_INFO_ADDR 0x00D0 /*地址偏移(要求地址偏移第一包内)*/

static uint8_t digest[32];
static uint8_t sign_key[64];
const uint8_t fotas_pub_key[64] = {
    0x96, 0xA1, 0x65, 0x9A, 0x9B, 0xD0, 0xC5, 0x9F, 0x0F, 0x19, 0x3F, 0xCC, 0x9A, 0x56, 0x73, 0xE6,
    0x9B, 0x29, 0x15, 0xFF, 0x56, 0xDA, 0x6D, 0x89, 0x19, 0xA3, 0x07, 0x43, 0xBF, 0xF3, 0xB7, 0x26,
    0xA4, 0xE3, 0x7E, 0x7A, 0x12, 0xA2, 0x00, 0x3F, 0x99, 0x64, 0xA4, 0xB4, 0xB1, 0xBF, 0x8F, 0xFC,
    0x92, 0xD2, 0x00, 0x19, 0x08, 0x4E, 0x7B, 0x68, 0xD2, 0x55, 0x47, 0x50, 0xE1, 0x77, 0x90, 0xDE};

typedef struct {
    char chip_type[20]; /*芯片类型*/
    uint32_t soft_version; /*软件版本*/
    char build_time[24]; /*编译时间*/
} soft_info_t;

/*在起始地址偏移0x2A0处添加自定义常量，方便识别自身的固件*/
const soft_info_t soft_ver __attribute__((at(OTA_INFO_ADDR)))
= {CHIP_TYPE, IOT_SYSTEM_VER, __DATE__ "-" __TIME__};

ota_info_t ota_info;
ota_cache_t ota_cache;
uint8_t OTA_SUCC = 0;

static void fotas_flash_cleanup(void);
void ota_write_unit(uint32_t addr, uint8_t *data, uint16_t len);

/*开始OTA*/
uint8_t iot_begin_ota(void) {
    LOG_I("Beginning OTA update");
    ota_info.current_addr = OTA_ADDR_START;
    ota_info.length = 0;
    ota_info.check_image_flag = false;
    ota_info.new_image.base = OTA_ADDR_START;
    ota_info.state = OTA_SUCCESS;
    ota_cache.offset = 0;
    ota_info.length = 0;
    ota_cache.res_len = 0;

    // 清除之前可能存在的任何OTA设置
    ota_settings_erase();

    // 擦除OTA目标区域
    LOG_I("Cleaning up flash for OTA");
    fotas_flash_cleanup();

    return OTA_SUCCESS;
}

static uint32_t get_ota_sign_key(void) {
    uint8_t End_data[256];
    uint8_t *ptr = NULL;
    uint32_t offset = 0;
    uint8_t cnt = 0;
    memset(End_data, 0, sizeof(End_data));
    while (offset < ota_info.length) {
        hal_flash_multi_io_read(
            OTA_ADDR_START + offset,
            End_data,
            FLASH_PAGE_SIZE); /*提取出备份区bin文件最后小于256字节的数据（从前往后）*/
        ptr = End_data; // 指向第一个分隔符；
        for (uint16_t i = 0; i < FLASH_PAGE_SIZE; i++) {
            if (*ptr == 0x1b) {
                cnt++;
                ptr++;
            } else {
                cnt = 0;
            }
            if (cnt == 32) {
                LOG_I("get sign_key success!");
                memcpy(sign_key, ptr, 64); // ptr已经指向签名值的首地址
                // for (uint16_t i = 0; i < 32; i++) {
                //     printf("");
                // }
                return offset + i - 31; // 固件去掉签名和1b后的实际长度
            }
        }
        offset += 256;
    }
    LOG_I("ota_info.length is %d, offset is %d ,get sign_key falied!", ota_info.length, offset);
    return 1;
}

void sha256_gen(uint32_t data_length) {
    // timers = 0;
    uint8_t cache[256];
    struct tc_sha256_state_struct s;
    tc_sha256_init(&s);
    uint32_t count = 0;
    count = data_length / 256;
    // LOG_I("length is %x", ota_info.length);
    // Basic_Timer_Cfg();
    for (uint32_t i = 0; i < count; i++) {
        hal_flash_multi_io_read(FLASH_PAGE_SIZE * i + OTA_ADDR_START, cache, FLASH_PAGE_SIZE);
        tc_sha256_update(&s, (const uint8_t *)cache, FLASH_PAGE_SIZE);
    }
    if (data_length % 256 != 0) { // 不足256字节算哈希；
        uint8_t END_data_len = data_length % 256;
        hal_flash_multi_io_read(data_length - END_data_len + OTA_ADDR_START, cache, END_data_len);
        tc_sha256_update(&s, (const uint8_t *)cache, END_data_len);
    }
    tc_sha256_final(digest, &s);
    // LOG_I("time is: %d", timers);
}

/*结束OTA*/
uint8_t iot_end_ota(void) {
    if (ota_cache.offset > 0 && (ota_info.length < OTA_MAX_SIZE)) { /*还有剩余数据*/
        // LOG_I("res len:%d",ota_cache.offset);
        ota_write_unit(ota_info.current_addr - FLASH_BASE_ADDR,
                       ota_cache.data,
                       ota_cache.offset); /*写剩余字节*/
        ota_info.length += ota_cache.offset;
        // LOG_I("all len:%d",ota_info.length);
    }

    if (!ota_info.state) { /*OTA的状态是否正常*/
        if (ota_info.new_image.base != get_app_image_base()) { /*检查基地址信息*/
            uint32_t length_addr = 0;
            length_addr = get_ota_sign_key(); // 获取签名值
            sha256_gen(length_addr); // 获取哈希值
            // 验签:
            if (!uECC_verify(fotas_pub_key, digest, sizeof(digest), sign_key, uECC_secp256r1())) {
                LOG_I("verify fail!");
                LOG_I("bin len:%d", ota_info.new_image.size);
                return OTA_FAIL;
            } else {
                OTA_SUCC = 1;
                LOG_I("verify success!");
                ota_info.new_image.size = ota_info.length;

                // 确保地址是4字节对齐的
                uint32_t src_addr = ota_info.new_image.base & ~0x3;
                uint32_t dst_addr = get_app_image_base() & ~0x3;

                // 使用SDK 2.1版本的OTA API设置固件信息
                struct fota_copy_info copy_info = {0};
                copy_info.fw_copy_src_addr = src_addr;
                copy_info.fw_copy_dst_addr = dst_addr;
                copy_info.fw_copy_size = ota_info.length;

                LOG_I("Setting OTA copy info:");
                LOG_I("src=0x%08x, dst=0x%08x, size=%d",
                      copy_info.fw_copy_src_addr,
                      copy_info.fw_copy_dst_addr,
                      copy_info.fw_copy_size);

                // 设置OTA相关信息
                ota_copy_info_set(&copy_info);
            }
        } else {
            // 需要删除OTA设置的情况
            LOG_I("Erasing OTA settings");
            ota_settings_erase_req_set();
        }
        // OTA完成，触发重启
        LOG_I("OTA completed successfully, resetting device");
        // platform_reset(RESET_OTA_SUCCEED);
        return OTA_SUCCESS;
    }
    return OTA_FAIL;
}

/*OTA数据写入*/
uint8_t iot_ota_write(uint8_t *data, uint16_t len) {
    /*OTA期间，多次调用此api可以让数据按顺序写入分区，地址自动累加*/
    if (ota_info.length == 0 && !ota_info.check_image_flag) { /*从第一包校验特定值*/
        /*检查固件的合法性*/
        // if (strncmp(CHIP_TYPE, (char *)data + OTA_INFO_ADDR, strlen(CHIP_TYPE)) != 0) {
        //     // LOG_I("invalid firmware");
        //     ota_info.state = OTA_INVALID_FIREWARE;
        //     return OTA_INVALID_FIREWARE;
        // } else {
        //     uint32_t version = 0;
        //     version |= ((uint32_t)(*(data + OTA_INFO_ADDR + 20)));
        //     version |= ((uint32_t)(*(data + OTA_INFO_ADDR + 21))) << 8;
        //     version |= ((uint32_t)(*(data + OTA_INFO_ADDR + 22))) << 16;
        //     version |= ((uint32_t)(*(data + OTA_INFO_ADDR + 23))) << 24;
        //     version -= version % 100; /*去掉小版本号*/
        //     if (version != IOT_SYSTEM_VER - IOT_SYSTEM_VER % 100) /*判断大版本是否一致*/
        //     {
        //         // LOG_I("invalid firmware");
        //         ota_info.state = OTA_INVALID_FIREWARE;
        //         return OTA_INVALID_FIREWARE;
        //     }
        // }
        ota_info.check_image_flag = true;
    }
    if (!ota_info.state && ota_info.length < OTA_MAX_SIZE) {
        if (len < 256) {
            if ((ota_cache.offset + len) > 256) {
                ota_cache.res_len = 256 - ota_cache.offset; /*计算剩余长度*/
                /*复制数据填充数组*/
                memcpy(ota_cache.data + ota_cache.offset, data, ota_cache.res_len);
                /*写256字节*/
                ota_write_unit(ota_info.current_addr - FLASH_BASE_ADDR, ota_cache.data, 256);
                ota_info.current_addr += 256; /*地址增加256*/
                ota_info.length += 256; /*长度增加*/
                /*复制剩余的数据到数组中*/
                memcpy(ota_cache.data, data + ota_cache.res_len, len - ota_cache.res_len);
                ota_cache.offset = len - ota_cache.res_len;
            } else { /*小于256字节部分*/
                memcpy(ota_cache.data + ota_cache.offset, data, len); /*复制不满256字节数据*/
                ota_cache.offset += len;
                // ota_info.length+=len;
            }
        } else {
            ota_write_unit(ota_info.current_addr - FLASH_BASE_ADDR, data, len);
            ota_info.current_addr += len; /*地址增加*/
            ota_info.length += len; /*长度增加*/
            // LOG_I("addr:%x, len:%d", ota_info.current_addr, ota_info.length);
        }
        return OTA_SUCCESS;
    } else {
        // LOG_I("OTA_OVER_SIZE");
        ota_info.state = OTA_OVER_SIZE;
        return OTA_OVER_SIZE;
    }
}

/*放弃ota升级*/
uint8_t iot_ota_abort(void) {
    /*放弃ota升级*/
    LOG_I("Aborting OTA update");
    ota_settings_erase(); // 擦除所有OTA设置 - SDK 2.1 API
    // platform_reset(RESET_OTA_FAILED); // 使用失败标志重启
    return 1;
}

/*全部擦除*/
static void fotas_flash_cleanup(void) {
    uint32_t size = OTA_MAX_SIZE; /*擦除大小*/
    uint32_t offset = OTA_ADDR_START - FLASH_BASE_ADDR; /*擦除起始地址*/
    while (size) {
        hal_flash_sector_erase(offset);
        if (size > FLASH_SECTOR_SIZE) {
            size -= FLASH_SECTOR_SIZE;
            offset += FLASH_SECTOR_SIZE;
        } else {
            break;
        }
    }
}

/*写入单元*/
void ota_write_unit(uint32_t addr, uint8_t *data, uint16_t len) {
    uint8_t i;
    uint16_t offset = 0;
    if (len >= 256) {
        for (i = 0; i < len / FLASH_PAGE_SIZE; i++) {
            /*按照一页256字节写*/
            hal_flash_quad_page_program(addr + offset, data + offset, FLASH_PAGE_SIZE);
            offset += FLASH_PAGE_SIZE;
        }
        /*判断是否有余数*/
        if (len % FLASH_PAGE_SIZE != 0) {
            hal_flash_quad_page_program(addr + offset, data + offset, len % FLASH_PAGE_SIZE);
        }
    } else {
        hal_flash_quad_page_program(addr, data, len);
    }
}
