#pragma once

#include <stdio.h>
#include <string.h>
#include "esp_event.h"

#define INV_GROUP       1
#define PACK_GROUP      2
#define IOT_GROUP       3
#define LCD_GROUP       4

#define IMAGE_VAILD_TIMES       3

#ifndef FLASH_SEC_SIZE
#define FLASH_SEC_SIZE  ((uint32_t)4096)    /**< SPI Flash sector size */
#endif

/*app备份区flash空间大小与APP区相同*/
#define IMAGE_IOT_BACK_MAX_LEN          ((uint32_t)0x280000u) // 2560Kbyte

#define IOT_FACTORY_INFO_ADDRESS        ((uint32_t)0x0000u)
#define IOT_FACTORY_INFO_MAX_LEN        (FLASH_SEC_SIZE)  // 4Kbyte

#define IMAGE_HEAD_AREA_ADDRESS         (IOT_FACTORY_INFO_ADDRESS + IOT_FACTORY_INFO_MAX_LEN) // 4096
#define IMAGE_HEAD_AREA_MAX_LEN         (FLASH_SEC_SIZE)  // 4Kbyte

#define IMAGE_FLASH_AREA_ADDRESS         (IMAGE_HEAD_AREA_ADDRESS + IMAGE_HEAD_AREA_MAX_LEN) // 8192
#define IMAGE_FLASH_AREA_MAX_LEN         ((uint32_t)(IMAGE_IOT_BACK_MAX_LEN - IMAGE_FLASH_AREA_ADDRESS))

#define Swap32(A) ((((uint32_t)(A) & 0xff000000) >> 24UL) | \
				   (((uint32_t)(A) & 0x00ff0000) >>  8UL) | \
				   (((uint32_t)(A) & 0x0000ff00) <<  8UL) | \
				   (((uint32_t)(A) & 0x000000ff) << 24UL))

enum
{
    GROUP_INV = 1,
    GROUP_PACK = 2,
    GROUP_IOT = 3,
    GROUP_LCD = 4,
    GROUP_ATS = 5,
    GROUP_PANEL = 6,
    GROUP_CHARGE = 7,
    GROUP_MAX
};

enum{
    READ_IMAGE_FROM_FLASH = 1,
    READ_IMAGE_FROM_FILE,
};

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
    DEVICE_DC_DC = 16,
    DEVICE_AT1_ARM = 17,
    DEVICE_PANEL_ARM = 18,
    DEVICE_SYNLINK = 19,
    MAX_DEVICE_NUN,
    DEVICE_FREE = 0xFF,
};//modbus beta 701

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

#define JUMP_BYTES          20


extern const uint8_t TypeCnt;

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
//    ota_group_t2 group; // 升级进度
}ota_data_t2;
extern  ota_data_t2 ota_data;

/**
 * @brief ota组定义
 */
typedef union {
    uint16_t all;
    struct {
        uint8_t dev_id : 8;		// 设备id(索引),选择该组下哪个ID需要升级0~0xFF(从0开始，0表示#1；0xFF表示广播)
        uint8_t dev_type : 8;	// 设备类型，选择需要升级的组（1：INV-OTA；2：PACK-OTA；3: IOT-OTA; 4-LCD）
    };
} ota_group_t;

typedef enum
{
    OTA_NONE = 0,
    OTA_UNICAST,         // 1 单播升级
    OTA_SEMI_BROADCASST, // 2 半广播升级
    OTA_FULL_BROADCAST,  // 3 全广播升级
    OTA_UNKNOWN
} ota_mode_t;

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

#ifdef __cplusplus
extern "C" {
#endif

bool sys_is_updating(void);

uint8_t System_ota_total_pct_update(uint8_t curr_pct, bool is_download, uint8_t curr_count, uint8_t total_count, bool is_only_stage);

#ifdef __cplusplus
}
#endif
