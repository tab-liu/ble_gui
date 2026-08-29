#pragma once

#include "ota_define.h"
#include "filesystem.h"
#include "comm_define.h"
#include "iot_ota.h"

/*默认为“单独备份分区”，可在独立配置文件中单独定义*/
#ifndef IMAGE_BACK_CUSTOM_SAVE_CHL
/*备份固件存储方式（0：禁用，1：单独备份分区，2：文件系统(耗时大约是单独分区的100倍)）*/
#define IMAGE_BACK_CUSTOM_SAVE_CHL  1
#endif

#if (IMAGE_BACK_CUSTOM_SAVE_CHL == 1)

/**< 分区表中备份数据分区名称 */
#define FS_BACK_PARTITION_NAME   "custom_data"

/*备份区块最大数量*/
#define IMAGE_CUSTOM_BACK_TYPE_MAX  3

/*备份区总大小*/
#define IMAGE_CUSTOM_MAX_LEN    ((uint32_t)0xD7000) // 860Kbyte

/*备份区信息头大小*/
#define IMAGE_CUSTOM_HEAD_MAX   (FLASH_SEC_SIZE)  // 4Kbyte

/*备份区单个区块的最大容量*/
#define IMAGE_CUSTOM_DATA_MAX   ((uint32_t)0x46000) // 280Kbyte

#elif (IMAGE_BACK_CUSTOM_SAVE_CHL == 2)

/*备份文件二级目录*/
#define IMAGE_CUSTOM_FOLDER_LEVEL1 "C/"

/*文件系统内备份文件名称 */
#define IMAGE_CUSTOM_BACK_FILE_NAME   FS_BASE_PATH"/"IMAGE_CUSTOM_FOLDER_LEVEL1"custom"

/*备份区块最大数量*/
#define IMAGE_CUSTOM_BACK_TYPE_MAX  2

/*备份区总大小*/
#define IMAGE_CUSTOM_MAX_LEN    ((uint32_t)0x82000) // 520Kbyte

/*备份区信息头大小*/
#define IMAGE_CUSTOM_HEAD_MAX   (FLASH_SEC_SIZE)  // 4Kbyte

/*备份区单个区块的最大容量*/
#define IMAGE_CUSTOM_DATA_MAX   ((uint32_t)0x40000) // 256Kbyte

#endif

/*默认无有效类型，需要"comm_define.h"里主动定义*/
#ifndef IMAGE_CUSTOM_BACK_TYPE1
#define IMAGE_CUSTOM_BACK_TYPE1 DEVICE_FREE
#endif

#ifndef IMAGE_CUSTOM_BACK_TYPE2
#define IMAGE_CUSTOM_BACK_TYPE2 DEVICE_FREE
#endif

#ifndef IMAGE_CUSTOM_BACK_TYPE3
#define IMAGE_CUSTOM_BACK_TYPE3 DEVICE_FREE
#endif

#define IMAGE_CUSTOM_BACK1_HEAD_ADDRESS ((IMAGE_CUSTOM_HEAD_MAX + IMAGE_CUSTOM_DATA_MAX) * 0)
#define IMAGE_CUSTOM_BACK1_HEAD_MAX_LEN IMAGE_CUSTOM_HEAD_MAX
#define IMAGE_CUSTOM_BACK1_DATA_ADDRESS (IMAGE_CUSTOM_BACK1_HEAD_ADDRESS + IMAGE_CUSTOM_BACK1_HEAD_MAX_LEN)
#define IMAGE_CUSTOM_BACK1_DATA_MAX_LEN IMAGE_CUSTOM_DATA_MAX

#define IMAGE_CUSTOM_BACK2_HEAD_ADDRESS ((IMAGE_CUSTOM_HEAD_MAX + IMAGE_CUSTOM_DATA_MAX) * 1)
#define IMAGE_CUSTOM_BACK2_HEAD_MAX_LEN IMAGE_CUSTOM_HEAD_MAX
#define IMAGE_CUSTOM_BACK2_DATA_ADDRESS (IMAGE_CUSTOM_BACK2_HEAD_ADDRESS + IMAGE_CUSTOM_BACK2_HEAD_MAX_LEN)
#define IMAGE_CUSTOM_BACK2_DATA_MAX_LEN IMAGE_CUSTOM_DATA_MAX

#define IMAGE_CUSTOM_BACK3_HEAD_ADDRESS ((IMAGE_CUSTOM_HEAD_MAX + IMAGE_CUSTOM_DATA_MAX) * 2)
#define IMAGE_CUSTOM_BACK3_HEAD_MAX_LEN IMAGE_CUSTOM_HEAD_MAX
#define IMAGE_CUSTOM_BACK3_DATA_ADDRESS (IMAGE_CUSTOM_BACK3_HEAD_ADDRESS + IMAGE_CUSTOM_BACK3_HEAD_MAX_LEN)
#define IMAGE_CUSTOM_BACK3_DATA_MAX_LEN IMAGE_CUSTOM_DATA_MAX

_Static_assert(((IMAGE_CUSTOM_HEAD_MAX + IMAGE_CUSTOM_DATA_MAX) * IMAGE_CUSTOM_BACK_TYPE_MAX) <= IMAGE_CUSTOM_MAX_LEN, "IMAGE_CUSTOM_MAX_LEN too small");

#pragma pack (1)

typedef union {
    uint8_t all[128];
    struct {
        uint8_t can_id;
        uint8_t file_type;
        uint32_t soft_version;
        uint16_t filesize;
        uint32_t data_len;
    };
}image_back_header_t;

_Static_assert(sizeof(image_back_header_t) == 128, "image_back_header_t invalid");

typedef struct
{
    uint8_t  identify[8];      // 识别符
    uint8_t  type;             // 文件类型
    uint8_t  dev_model[12];    // 支持的机器型号
    uint32_t version;          // 镜像版本
    uint32_t sizeOfBytes;      // 镜像大小
    uint32_t crc32;            // 镜像CRC32
    uint8_t  time[20];         // 镜像修改日期
} ota_image_head_t;

#pragma pack ()

#ifdef __cplusplus
extern "C" {
#endif

// 备份模块初始化
bool ImageBackInit(void);

// 判断类型是否有效并返回对应index
uint8_t back_image_type_to_index(uint16_t type);

/**
 * @brief 创建并启动升级文件备份任务。
 */
esp_err_t Start_Back_Image_Task(uint8_t index, uint8_t type, uint32_t version, uint16_t filesize, uint16_t dev_id, uint32_t len);


// 检查是否存在预备升级固件并进行准备
int back_image_firmware_check_and_ready(uint8_t type, uint32_t version, CanOtaStruct *OtaStatus);

#ifdef __cplusplus
}
#endif

