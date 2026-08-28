#ifndef OTA_TYPE_H
#define OTA_TYPE_H

#include <stdio.h>
#include <string.h>
#include "esp_event.h"

#define INV_GROUP       1
#define PACK_GROUP      2
#define IOT_GROUP       3
#define LCD_GROUP       4

#define IMAGE_VAILD_TIMES       3

enum{
    READ_IMAGE_FROM_FLASH = 1,
    READ_IMAGE_FROM_FILE,
};

#if 0
enum{
    DEVICE_IOT = 0,
    DEVICE_ARM = 1,
    DEVICE_DSP = 2,
    DEVICE_BMS = 3,
    DEVICE_PACK_BA = 4,
    DEVICE_PACK_BCU = 5,
    DEVICE_PACK_BMU = 6,
    DEVICE_PACK_BMS = 7,
    DEVICE_PACK_M1 = 8,
    DEVICE_PACK_SAFTY = 9,
    DEVICE_PACK_HV = 10,
    DEVICE_HMI1 = 11,
    DEVICE_HMI2 = 12,
    DEVICE_RF = 13,
    DEVICE_DC_HUB = 14,
    DEVICE_AC_HUB = 15,
    MAX_DEVICE_NUN,
    DEVICE_FREE = 0xFF,
};//modbus beta 701
#endif
//enum 
//{
//    OTA_ERR_NORMAL = 0,
//    OTA_ERR_SIZE = 1,
//    OTA_ERR_FLASH_ERASE = 2,
//    OTA_ERR_FLASH_WRITE = 3,
//    OTA_ERR_FLASH_READ = 4,
//    OTA_ERR_FILE = 5,
//    OTA_ERR_CRC16 = 6,
//    OTA_ERR_CRC32 = 7,
//    OTA_ERR_SEQ = 8,
//    OTA_ERR_TIMEOUT = 9,
//    OTA_ERR_NO_RESP = 10,
//    OTA_ERR_MEMONY = 11,
//    OTA_ERR_ABORT = 12,
//    OTA_ERR_FILE_TYPE = 13,
//    OTA_ERR_CAN_BUSY = 14, /* can 总线繁忙 */
//    OTA_ERR_CAN_ERR = 15, /* can 总线发送失败 */
//    OTA_ERR_FILE_NULL = 16,
//};

#pragma pack (1)

typedef union {
    uint16_t all;
    struct {
        uint8_t target : 8; // 目标ID
        uint8_t group : 8; // 哪一组
    }bit;
}ota_target_t;

typedef struct {
    uint16_t start; //手机启动升级命令 0： 不升级，1：发起升级
    uint16_t type; // 本次升级文件类型
    uint32_t version; // 本次升级文件版本
    uint16_t kbytes; // 本次升级文件大小
    ota_target_t target; // 本次升级的目标,modbus beta 720
}ota_cmd_t;

typedef struct {
    union _bit1 {
        uint16_t all;
        struct {
            uint16_t where : 8; // 低8bit
            uint16_t doing  : 8; // 高8bit
        };
    } doing_where;

    union _bit2 {
        uint16_t all;
        struct {
            uint16_t level : 8; // 低8bit
            uint16_t type  : 8; // 高8bit
        };
    } type_level;

    union _bit3 {
        uint16_t all;
        struct {
            uint16_t errcode : 8; // 低8bit
            uint16_t pct  : 8; // 高8bit
        };
    } pct_errcode;
}ota_state_t2;

typedef struct {
    ota_target_t target;
    ota_state_t2 state[4]; // 
}ota_group_t2;

typedef struct {
    ota_cmd_t cmd; // 升级命令
    ota_group_t2 group; // 升级进度
}ota_data_t2;
extern EXT_RAM_BSS_ATTR ota_data_t2 ota_data;

typedef struct {
    uint16_t vaildTimes; // 文件有效剩余次数 最多3次
    uint32_t version; // 底层设备当前镜像版本号
    uint16_t cmdLen; // 指令长度,需要发送几个寄存器
    uint16_t slaveAddr; // modbus 从机地址
    uint16_t type; //modbus 文件类型
    uint16_t baud; //modbus 保留
    uint16_t mainDevId; //modbus 主设备号
    uint16_t subDevId; //modbus 子设备号
    uint16_t totalKbyte; //modbus 文件总的大小KB
    ota_target_t target;
    char recordId[32];
    uint16_t nc[10]; // 保留
    uint16_t crc16; // 当前结构体crc16
}image_info_t;
#pragma pack ()


#endif
