#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ota_define.h"
#include "esp_log.h"
#include "parameter.h"
#include "image_back.h"
#include "image_handle.h"

#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "utils.h"

#define TAG "[SOFT_BACK]"

#ifdef CONFIG_MCU_AUTO_UPDATE_IN_BOOT

/* 信号量（互斥） */
static SemaphoreHandle_t xImageBackMutex = NULL;

/* 备份文件指针 */
static void *BackFile = NULL;

/* 简单封装：获取/释放互斥 */
static inline BaseType_t ImageBackTakeMutex(TickType_t timeout_ms)
{
    if (!xImageBackMutex) return pdFALSE;
    return xSemaphoreTake(xImageBackMutex, pdMS_TO_TICKS(timeout_ms));
}
static inline void ImageBackGiveMutex(void)
{
    if (xImageBackMutex) xSemaphoreGive(xImageBackMutex);
}

// 备份模块初始化
bool ImageBackInit(void)
{
    if (xImageBackMutex == NULL) {
        xImageBackMutex = xSemaphoreCreateMutex();
        if (xImageBackMutex == NULL) {
            ESP_LOGE(TAG, "xImageBackMutex: create mutex failed");
            return false;
        }
    }
    return true;
}

// 固件升级命令临时存储
static USE_EXT_RAM_BSS image_back_header_t image_back_header = {0};

static esp_err_t back_image_erase(uint32_t address, size_t len) {
    esp_err_t err = ESP_OK;

#if (IMAGE_BACK_CUSTOM_SAVE_CHL == 1)

    // Find the partition map in the partition table
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, FS_BACK_PARTITION_NAME);

    // 参数检查
    if ((address % FLASH_SEC_SIZE) != 0 || (len % FLASH_SEC_SIZE) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t erased = 0;
    while (erased < len) {
        err = esp_partition_erase_range(partition, address + erased, FLASH_SEC_SIZE);
        if (err != ESP_OK) {
            break;
        }
        erased += FLASH_SEC_SIZE;
        vTaskDelay(1); // 让出CPU，避免长时间阻塞
    }

#elif (IMAGE_BACK_CUSTOM_SAVE_CHL == 2)

    if ( !fs_file_exists(IMAGE_CUSTOM_BACK_FILE_NAME) ) {
        return ESP_OK;  // 文件不存在，无需擦除
    } else {
        if ( len == fs_file_custom_erase_at(BackFile, address, len) ){
            return ESP_OK;
        } else {
            return ESP_FAIL;
        }
    }

#else

    // 不支持
    return ESP_FAIL;

#endif

    return err;
}

static esp_err_t back_image_read(uint32_t address, uint8_t *data, size_t len) {
    esp_err_t err = ESP_OK;

#if (IMAGE_BACK_CUSTOM_SAVE_CHL == 1)

    // Find the partition map in the partition table
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, FS_BACK_PARTITION_NAME);

    // read the data, starting from the beginning of the partition
    err = esp_partition_read(partition, address, data, len);

#elif (IMAGE_BACK_CUSTOM_SAVE_CHL == 2)

    if ( len == fs_file_read_at(IMAGE_CUSTOM_BACK_FILE_NAME, address, data, len) ){
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }

#else

    // 不支持
    return ESP_FAIL;

#endif

    return err;
}

static esp_err_t back_image_write(uint32_t address, const uint8_t *data, size_t len) {
    esp_err_t err = ESP_OK;
    
#if (IMAGE_BACK_CUSTOM_SAVE_CHL == 1)

    // Find the partition map in the partition table
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, FS_BACK_PARTITION_NAME);

    // Write the data, starting from the beginning of the partition
    err = esp_partition_write(partition, address, data, len);

#elif (IMAGE_BACK_CUSTOM_SAVE_CHL == 2)
    
    if ( len == fs_file_custom_write_at(BackFile, address, data, len) ){
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }

#else

    // 不支持
    return ESP_FAIL;

#endif
    
    return err;
}

// 判断 index 是否有效（1~3 有效）
static inline int back_image_index_valid(uint8_t index) {
    return (index >= 1 && index <= IMAGE_CUSTOM_BACK_TYPE_MAX) ? 1 : 0;
}

// 返回 index 对应的 HEAD 地址（index: 1~3）
static inline uint32_t back_image_head_address(uint8_t index) {
    return (IMAGE_CUSTOM_HEAD_MAX + IMAGE_CUSTOM_DATA_MAX) * (index - 1);
}

// 返回 index 对应的 DATA 地址（index: 1~3）
static inline uint32_t back_image_data_address(uint8_t index) {
    return back_image_head_address(index) + IMAGE_CUSTOM_HEAD_MAX;
}

// 判断类型是否有效并返回对应index
uint8_t back_image_type_to_index(uint16_t type) {
    if(type == IMAGE_CUSTOM_BACK_TYPE1) return 1;
    if(type == IMAGE_CUSTOM_BACK_TYPE2) return 2;
    if(type == IMAGE_CUSTOM_BACK_TYPE3) return 3;
    return 0;
}

// 存储本次升级固件
static void Back_Image_Task(void *pvParameters)
{
    uint8_t index = back_image_type_to_index(image_back_header.file_type);
    if (!back_image_index_valid(index)) return;

    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (ImageBackTakeMutex(5000) != pdTRUE) return; 

#if (IMAGE_BACK_CUSTOM_SAVE_CHL == 2)
    // 检查文件是否存在，执行对应Open方式
    if ( fs_file_exists(IMAGE_CUSTOM_BACK_FILE_NAME) ) {
        BackFile = fs_file_open(IMAGE_CUSTOM_BACK_FILE_NAME, "rb+");
    } else {
        BackFile = fs_file_open(IMAGE_CUSTOM_BACK_FILE_NAME, "wb");
    }

    if( BackFile == NULL ) {
        ESP_LOGE(TAG, "Failed to open BackFile!");
        goto Back_Image_Task_Exit;
    }
#endif

    esp_err_t err = back_image_erase(back_image_head_address(index), IMAGE_CUSTOM_HEAD_MAX);
    if( err == ESP_OK ) {
        err = back_image_write(back_image_head_address(index), (const uint8_t *)&image_back_header, sizeof(image_back_header));
        if ( err != ESP_OK ) {
            ESP_LOGE(TAG, "Backup failed: write error at offset %lu", back_image_head_address(index));
            goto Back_Image_Task_Exit;
        }
    } else {
        ESP_LOGE(TAG, "Failed to erase image_back_header area!");
        goto Back_Image_Task_Exit;
    }

    if( err == ESP_OK ) {
        // 定义缓冲区大小为 Flash 页大小
        const uint32_t buffer_size = FLASH_SEC_SIZE;
        bool backup_success = true;
        
        // 使用动态内存避免栈溢出
        uint8_t *buf = (uint8_t *)iot_calloc(buffer_size);
        if (buf != NULL) {
            uint32_t total_bytes = image_back_header.filesize * 1024;
            uint32_t backup_area_size = (total_bytes + FLASH_SEC_SIZE - 1) / FLASH_SEC_SIZE * FLASH_SEC_SIZE;

            ESP_LOGW(TAG, "Firmware backup start!");        
            if (back_image_erase(back_image_data_address(index), backup_area_size) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to erase backup area!");
                backup_success = false;
            } else {
                uint32_t copied_bytes = 0;
                
                // 按 4096 字节的块大小循环复制固件
                while (copied_bytes < total_bytes)
                {
                    uint32_t chunk_size = buffer_size;
                    // 如果是最后一块，计算剩余的字节数
                    if (copied_bytes + chunk_size > total_bytes) {
                        chunk_size = total_bytes - copied_bytes;
                    }
                
                    if (iot_image_read((IMAGE_FLASH_AREA_ADDRESS + copied_bytes), buf, chunk_size) != ESP_OK) {
                        ESP_LOGE(TAG, "Backup failed: read error at offset %lu", copied_bytes);
                        backup_success = false;
                        break;
                    }
                    
                    if (back_image_write((back_image_data_address(index)  + copied_bytes), buf, chunk_size) != ESP_OK) {
                        ESP_LOGE(TAG, "Backup failed: write error at offset %lu", copied_bytes);
                        backup_success = false;
                        break;
                    }
                    
                    copied_bytes += chunk_size;
                    vTaskDelay(1); // 喂狗并让出CPU
                }
            }
          
            // 释放内存
            free(buf);
        
            if (backup_success) {
                ESP_LOGW(TAG, "Firmware backup success!");
            } else {
                ESP_LOGE(TAG, "Firmware backup failed!");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to allocate memory for backup buffer");
        }
    }

#if (IMAGE_BACK_CUSTOM_SAVE_CHL == 2)
    // close对应文件
    if( BackFile ) {
        if ( 0 == fs_file_close(BackFile) ) {
            BackFile = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to close BackFile!");
        }
    }
#endif

Back_Image_Task_Exit :

    /* 释放互斥 */
    ImageBackGiveMutex();

    vTaskDelete(NULL);   // 删除当前任务，释放其堆栈和TCB。
}

/**
 * @brief 创建并启动升级文件备份任务。
 *
 * @return esp_err_t 成功时返回 ESP_OK，失败时返回 ESP_FAIL。
 */
esp_err_t Start_Back_Image_Task(uint8_t index, uint8_t type, uint32_t version, uint16_t filesize, uint16_t dev_id, uint32_t len)
{
    // 检查输入参数的有效性
    if (!back_image_index_valid(index) 
        || ((filesize * 1024) > IMAGE_CUSTOM_DATA_MAX)) {
        ESP_LOGE(TAG, "Cannot start Back Image task.");
        return ESP_FAIL;
    }

    // 本地缓存固件基本信息
    memset(&image_back_header, 0, sizeof(image_back_header));
    image_back_header.can_id = dev_id;
    image_back_header.file_type = type;
    image_back_header.soft_version = version;
    image_back_header.filesize = filesize;
    image_back_header.data_len = len;

    // 使用 xTaskCreate 来创建任务
    // 在32位系统上，可以将一个 uint32_t 值直接强制转换为 void* 来传递，这是一种高效的技巧。
    BaseType_t result = CREATE_TASK(
        Back_Image_Task,                  // 任务函数指针
        "Back_Image_Task",                // 任务名 (用于调试)
        4*1024,                           // 任务堆栈大小 (字节)
        NULL,                             // 传递给任务的参数
        1,                                // 任务优先级 (低优先级)
        NULL,                             // 任务句柄 (如果不需要，则为NULL)
        1
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Back_Image_Task.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Back_Image_Task created successfully.");
    return ESP_OK;
}

/**
  * @brief      检查下级存储固件头
                (由于不同升级文件头大小不一致，多数为512b，部分ti芯片为1024b
                故IOT侧不做文件头大小及CRC32校验，由下级被升级方自行校验)
  * @param[in]  uint8_t soft_type     
                uint32_t softversion  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t CheckFirmware_VerifyFileHeader(ota_image_head_t *Header, uint8_t soft_type, uint32_t softversion)
{
    uint8_t Ret = 1;

    /** 检查固件类型 */
    Ret &= (Header->type == soft_type);

    /** 检查机型 */
    // Ret &= (strncmp((const char *)Header->dev_model, inv_factory.inv_type, strlen(inv_factory.inv_type)) == 0);

    /** 检查版本号 */
    Ret &= ((Header->version / 100) == (softversion / 100));

    /** 检查标识符 */
    Ret &= (strncmp((const char *)Header->identify, "POWEROAK", strlen("POWEROAK")) == 0);

    /** 判断固件大小是否大于头固件大小 */
    // Ret &= (Header->sizeOfBytes > 0x200);

    /** 判断固件大小是否超过了备份区大小 */
    Ret &= (Header->sizeOfBytes < IMAGE_CUSTOM_DATA_MAX);

    /** 判断固件大小是否超过了BOOT2区大小 */
    // Ret &= (Header->sizeOfBytes < (BOOT2_END_ADDR - BOOT2_START_ADDR) * ADDR_BYTE_ALIGN);

    /** CRC32代码是否完整 */
    // if (Ret) Ret &= CheckFirmware_CheckCodeCrc32(Header, HEADER_BYTES_SIZE, ADDR_BYTE_ALIGN);


    ESP_LOGW(TAG, "*********Firmware_VerifyFileHeader*************");
    char identify_str[9] = {0};
    memcpy(identify_str, Header->identify, 8);
    ESP_LOGI(TAG, "identify   : %s", identify_str);
    
    ESP_LOGI(TAG, "type       : %u / %u", Header->type, soft_type);
    
    char dev_model_str[13] = {0};
    memcpy(dev_model_str, Header->dev_model, 12);
    ESP_LOGI(TAG, "dev_model  : %s / %s", dev_model_str, dev_factory.dev_type);
    
    ESP_LOGI(TAG, "version    : %lu / %lu", Header->version, softversion);
    ESP_LOGI(TAG, "sizeOfBytes: %lu / %lu", Header->sizeOfBytes, IMAGE_CUSTOM_DATA_MAX);
    ESP_LOGI(TAG, "crc32      : 0x%04lx", Header->crc32);
    
    char time_str[21] = {0};
    memcpy(time_str, Header->time, 20);
    ESP_LOGI(TAG, "time       : %s", time_str);
    ESP_LOGW(TAG, "**********************************************");
    
    return Ret;
}

// 移动本次升级固件
static int back_image_data_move(uint8_t index, uint16_t filesize) {   
    if (!back_image_index_valid(index)) return -1;

    // 定义缓冲区大小为 Flash 页大小
    const uint32_t buffer_size = FLASH_SEC_SIZE;
    bool move_success = true;
    
    // 使用动态内存避免栈溢出
    uint8_t *buf = (uint8_t *)iot_calloc(buffer_size);
    if (buf != NULL) {
        uint32_t total_bytes = filesize * 1024;
        uint32_t backup_area_size = (total_bytes + FLASH_SEC_SIZE - 1) / FLASH_SEC_SIZE * FLASH_SEC_SIZE;
        
        if (iot_image_erase(IMAGE_FLASH_AREA_ADDRESS, backup_area_size) != ESP_OK) {
            move_success = false;
        } else {
            uint32_t copied_bytes = 0;
            
            // 按 4096 字节的块大小循环复制固件
            while (copied_bytes < total_bytes)
            {
                uint32_t chunk_size = buffer_size;
                // 如果是最后一块，计算剩余的字节数
                if (copied_bytes + chunk_size > total_bytes) {
                    chunk_size = total_bytes - copied_bytes;
                }
                if (back_image_read((back_image_data_address(index)  + copied_bytes), buf, chunk_size) != ESP_OK) {
                    move_success = false;
                    break;
                }
                
                if (iot_image_write((IMAGE_FLASH_AREA_ADDRESS + copied_bytes), buf, chunk_size) != ESP_OK) {
                    move_success = false;
                    break;
                }
                
                copied_bytes += chunk_size;
                vTaskDelay(1); // 喂狗并让出CPU
            }
        }
      
        // 释放内存
        free(buf);
    }

    return move_success ? 0 : -1;
}

// 计算CRC
static uint32_t back_image_data_crc(uint32_t len)
{
	if (len == 0) return 0;

    uint32_t address = IMAGE_FLASH_AREA_ADDRESS;
    uint32_t remaining = len;
    
    uint8_t *buffer = iot_calloc(1024);
    if (buffer == NULL) {
        ESP_LOGI(TAG, "file: %s, function: %s, %d, malloc(1024) falied", __FILE__, __func__, __LINE__);
        return 0;
    }

    uint32_t image_crc32 = 0xFFFFFFFF; // 设置CRC32初始值
    while (remaining) {
        int size = (remaining > 1024) ? 1024 : remaining;
        esp_err_t err = iot_image_read(address, buffer, size);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "iot read image data failed %d (address: %lu, size: %d)", err, address, size);
            break;
        }
        image_crc32 = calcu_crc32(image_crc32, buffer, size);
        remaining -= size;
        address += size;
    }
    free(buffer);

    return image_crc32;
}

// 检查是否存在预备升级固件并进行准备
int back_image_firmware_check_and_ready(uint8_t type, uint32_t version, CanOtaStruct *OtaStatus) 
{
    uint8_t index = back_image_type_to_index(type);
    if (!back_image_index_valid(index)) return -1;

    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (ImageBackTakeMutex(10) != pdTRUE) return -1; 

    bool update_cmd = false;

    /* 获取升级命令 */
    memset(&image_back_header, 0, sizeof(image_back_header));
    if(ESP_OK != back_image_read(back_image_head_address(index), (uint8_t *)&image_back_header, sizeof(image_back_header))) {
        ESP_LOGE(TAG, "read image back header failed at index=%u, addr=0x%08lx", index, back_image_head_address(index));
        goto check_end;
    };

    /*检查升级命令*/
    if ( (type != image_back_header.file_type) || ((version / 100) != (image_back_header.soft_version / 100)) ) {
        ESP_LOGE(TAG, "header mismatch: expected type=%u ver=0x%08x, header type=%u ver=0x%08x",
                 type, (unsigned)version,
                 image_back_header.file_type, (unsigned)image_back_header.soft_version);
        goto check_end;
    }
    
    /** 获取文件头 */
    ota_image_head_t Header = {0};
    if(ESP_OK != back_image_read(back_image_data_address(index), (uint8_t *)&Header, sizeof(ota_image_head_t))) {
        ESP_LOGE(TAG, "read ota image header failed at index=%u, addr=0x%08lx", index, back_image_data_address(index));
        goto check_end;
    };
    
    // 检查本地固件头
    if(!CheckFirmware_VerifyFileHeader(&Header, type, version)) {
        ESP_LOGE(TAG, "CheckFirmware_VerifyFileHeader failed for type=%u ver=0x%08x", type, (unsigned)version);
        goto check_end;
    }

    // 转移升级文件至升级区
    if ( 0 != back_image_data_move(index, image_back_header.filesize) ) {
        ESP_LOGE(TAG, "back_image_data_move failed for index=%u, filesize=%u", index, (unsigned)image_back_header.filesize);
        goto check_end;
    }

    // 检查待升级固件CRC
    uint32_t ota_data_crc = back_image_data_crc(image_back_header.data_len);
    if (!ota_data_crc) {
        ESP_LOGE(TAG, "ota_data_crc == 0, invalid crc");
        goto check_end;
    }

    memset(&OtaStatus->start, 0, sizeof(OtaStart));
    memset(&OtaStatus->end,	 0, sizeof(OtaEnd));
    OtaStatus->start.fileType = type;						 // 文件类型
    OtaStatus->start.fileVersion = version;					 /* 文件完全版本 */
    OtaStatus->start.fileSize = image_back_header.filesize;	 /* 获取文件大小,转换为Kbyte */
    OtaStatus->start.dev_id = image_back_header.can_id;
    OtaStatus->is_only_stage = true;    
    OtaStatus->end.fileCrc32 = ota_data_crc;
    ESP_LOGI(TAG, "ready ota data success(crc: 0x%08lx), ready for can-ota upgrade", OtaStatus->end.fileCrc32);
    update_cmd = true;
    
check_end:
    
    /* 释放互斥 */
    ImageBackGiveMutex();

    return update_cmd ? 0 : -1;
}

#endif
