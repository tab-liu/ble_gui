#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
//#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#//include "esp_partition.h"
#include "errno.h"
#include "iot_ota.h"
#include "crc.h"
#include <arpa/inet.h>
#include "esp_app_format.h"

#define TAG "[OTA]"


#define SKIP_LEN    20

static esp_ota_handle_t update_handle = 0 ;
static const esp_partition_t *update_partition = NULL;
static bool first_block_data = false;
static int binary_file_length = 0;

// void ota_can_pct(CanOtaStruct *ota_status, uint8_t id, uint8_t type, uint8_t pct, uint8_t where, uint8_t errcode) {
//     ota_status->devStatus[ id ].type = type;
//     ota_status->devStatus[ id ].errCode = errcode;
//     ota_status->devStatus[ id ].where = where;
//     ota_status->devStatus[ id ].pct = pct;

//     if (id >= INV_CAN_ADDR && id <= INV_CAN_ADDR + INV_MAX_NUM) {
//         g_device_data.ota_cmd.group = 1;
//     } else if (id >= PACK_CAN_ADDR && id <= PACK_CAN_ADDR + PACK_MAX_NUM) {
//         g_device_data.ota_cmd.group = 2;
//     }
//     ota_group
// }

/* 1正确,0错误 */
uint8_t ota_file_check(FILE *pfile) {
    uint32_t file_size;
    uint32_t crc32A = ~0, crc32B = 0x1A;

    if (!pfile) return 0;

    fseek(pfile, 0, SEEK_END); // 指向文件末尾
    file_size = ftell(pfile);

    if (file_size == 0) return 0;

    for (uint16_t i = 1; i < file_size; ) { // 跳过0x1A 填充字符
        fseek(pfile, -i, SEEK_END);
        if (fread(&crc32B, 1, 1, pfile) == 1) {
            if (crc32B != 0x1A) {
                file_size -= i;
                break;
            }
            i++;
        }
    }
    if (file_size >= SKIP_LEN) {
        file_size -= SKIP_LEN;
    }

    file_size += 1;
    fseek(pfile, -SKIP_LEN, SEEK_CUR); // 在当前指针的位置前移20字节
    fread(&crc32B, 4, 1, pfile);       // 读取到文件内部的 CRC32

    // ESP_LOGI (TAG, "crc check size=%u", file_size);
    uint8_t *buf = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE (TAG, "memony malloc failed");
    }

    uint32_t offset = 0;
    uint32_t remaining = file_size - (file_size % 1024);
    while(remaining) {
        fseek(pfile, offset, SEEK_SET); // 设置到文件开始
        if (fread(buf, 4, 256, pfile) == 256) {
            crc32A = calcu_crc32(crc32A, buf, 1024);
            offset += 1024;
            remaining -= 1024;
        }
    }

    remaining = file_size % 1024;
    if(remaining) {
        if (fread(buf, 1, remaining, pfile) == remaining) {
            crc32A = calcu_crc32(crc32A, buf, remaining);
        }
    }
    free(buf);
    crc32A = ntohl(crc32A);
//    ESP_LOGI(TAG, "bin file (crc32B=0x%04x,crc32A=0x%04x)",crc32B, crc32A);
    if (crc32B != crc32A) {
        return 0;
    }

    return 1;
}
/*
IOT本身的 OTA erase；
*/
esp_err_t iot_ota_begin(void) {

    esp_err_t err = ESP_OK;
    update_partition = esp_ota_get_next_update_partition(NULL); // 获取空闲区域进行OTA
    assert(update_partition != NULL);
//    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
//                   update_partition->subtype, update_partition->address);

    // 开启OTA
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        esp_ota_abort(update_handle);
        // task_fatal_error();
        return err;
    }

    first_block_data = true;
    binary_file_length = 0;
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
    return err;
}


esp_err_t http_iot_ota_begin(esp_partition_t *http_update_partition, int size ,esp_ota_handle_t* handle)
{

    esp_err_t err = ESP_OK;
    //update_partition = esp_ota_get_next_update_partition(NULL); // 获取空闲区域进行OTA
    assert(http_update_partition != NULL);
//    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
//                   update_partition->subtype, update_partition->address);

    // 开启OTA
    err = esp_ota_begin(http_update_partition, size, handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        esp_ota_abort(*handle);
        // task_fatal_error();
        return err;
    }
    update_handle=*handle;
    update_partition=http_update_partition;
   // first_block_data = true;
    //binary_file_length = 0;
    ESP_LOGI(TAG, "esp_ota_begin succeeded handle:%lu update_handle:%lu size:%u",*handle,update_handle,size);
    return err;
}
/*
IOT本身的 OTA写入
*/
esp_err_t iot_ota_write(uint8_t *ota_write_data, uint32_t write_len) {

    esp_err_t err = ESP_OK;
    if (first_block_data == true) { // 输出第一包数据的信息,包含版本信息
        if (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) > write_len) {

            first_block_data = false;
            esp_app_desc_t new_app_info;
            esp_app_desc_t running_app_info;

            memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
            ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

            const esp_partition_t *running = esp_ota_get_running_partition();

            if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
            }
        }
    }
    ESP_LOGI(TAG,"iot_ota_write update_handle:%lu",update_handle);
    // 写OTA数据
    err = esp_ota_write(update_handle, (const void *)ota_write_data, write_len);
    if (err != ESP_OK) {
        esp_ota_abort(update_handle); // 终止OTA
        ESP_LOGE(TAG, "ota_write (%s)", esp_err_to_name(err));
        // task_fatal_error();
        return err;
    }

    binary_file_length += write_len;
    ESP_LOGD(TAG, "Written image length %d", binary_file_length);
    return err;
}


esp_err_t iot_ota_end(void) {

    esp_err_t err = ESP_OK;

    // 结束OTA
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        // task_fatal_error();
        ESP_LOGI(TAG,"iot_ota_end update_handle:%lu,err:%d",update_handle,err );
        return err;
    }

    // 设置boot启动
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        // task_fatal_error();
        return err;
    }
    return err;
}

esp_err_t iot_ota_abort(void) {
    esp_err_t err;
    err = esp_ota_abort(update_handle); // 终止OTA
    return err;
}

esp_err_t iot_ota_rollback(void) {
    esp_err_t ret = esp_ota_mark_app_invalid_rollback_and_reboot();
    if ( ESP_ERR_OTA_ROLLBACK_FAILED == ret) {
        ESP_LOGE(TAG, "The rollback is not possible due to flash does not have any apps");
    } else if (ESP_FAIL == ret) {
        ESP_LOGE(TAG, "The rollback is fail");
    }
    return ret;
}



