#include "ota_type.h"
//#include "iot_flash.h"
#include "xmodem_client.h"
//#include "xmodem_server.h"
//#include "bms_ota.h"
//#include "iot_crc.h"
#include "esp_log.h"
//#include "system_data.h"
//#include "iot_flash.h"
#include "iot_ota.h"
#include "image_handle.h"
#include "filesystem.h"

#define TAG "IMAGE"

#define Swap32(A) ((((uint32_t)(A) & 0xff000000) >> 24UL) | \
				   (((uint32_t)(A) & 0x00ff0000) >>  8UL) | \
				   (((uint32_t)(A) & 0x0000ff00) <<  8UL) | \
				   (((uint32_t)(A) & 0x000000ff) << 24UL))

EXT_RAM_BSS_ATTR static image_info_t image_info = {0};

image_info_t *get_image_info(void) {
    return &image_info;
}

void set_image_invaild(void) {
    image_info.vaildTimes = 0;
    if (iot_image_erase(IMAGE_HEAD_AREA_ADDRESS, IMAGE_HEAD_AREA_MAX_LEN) != ESP_OK) {
        ESP_LOGE(TAG, "%s image head erase failed", __func__);
        return ;
    }

    image_info.crc16 = xm_calcu_crc16((const uint8_t *)&image_info, sizeof(image_info_t) - 2);
    if (iot_image_write(IMAGE_HEAD_AREA_ADDRESS, (uint8_t *)&image_info, sizeof(image_info_t)) != ESP_OK) {
        ESP_LOGE(TAG, "%s image head write failed", __func__);
        return ;
    }
}

bool check_image_vaild(void) {
    if (iot_image_read(IMAGE_HEAD_AREA_ADDRESS, (uint8_t *)&image_info, sizeof(image_info_t)) != ESP_OK) {
        image_info.vaildTimes = 0;
        ESP_LOGE(TAG, " %s read image head flash failed", __func__);
        return false;
    }

    uint16_t crc16 = xm_calcu_crc16((const uint8_t *)&image_info, sizeof(image_info_t) - 2);
    if (crc16 != image_info.crc16) {
        image_info.vaildTimes = 0;
        ESP_LOGE(TAG, "%s image head crc16 failed", __func__);
        return false;
    }
    
    if ((image_info.type != DEVICE_DSP) && \
        (image_info.type != DEVICE_BMS) && \
        (image_info.type != DEVICE_PACK_BMS) && \
        (image_info.type != DEVICE_PACK_M1)) {
        ESP_LOGE(TAG, "image type invaild: %d", image_info.type);
        return false;
    } 
    
    if (image_info.vaildTimes > 0) {
        image_info.vaildTimes--;
        if (iot_image_erase(IMAGE_HEAD_AREA_ADDRESS, IMAGE_HEAD_AREA_MAX_LEN) != ESP_OK) {
            ESP_LOGE(TAG, "%s image head erase failed", __func__);
            return false;
        }

        image_info.crc16 = xm_calcu_crc16((const uint8_t *)&image_info, sizeof(image_info_t) - 2);
        if (iot_image_write(IMAGE_HEAD_AREA_ADDRESS, (uint8_t *)&image_info, sizeof(image_info_t)) != ESP_OK) {
            ESP_LOGE(TAG, "image head write failed");
            return false;
        }
    } else {
        ESP_LOGW(TAG, "image invaild remaining times: %d", image_info.vaildTimes);
        return false;
    }
    
    ESP_LOGI(TAG, "image type: %d", image_info.type);
    ESP_LOGI(TAG, "image total bytes: %d", (image_info.totalKbyte * 1024));
    ESP_LOGI(TAG, "image vaild remaining times: %d", image_info.vaildTimes);
    memset(ota_data.group.state, 0, sizeof(ota_data.group.state));
    if (image_info.type == DEVICE_DSP) 
	{
//        xmd_server_start(DSP_XMD_CHANNEL);
    } 
	else 
	{
//        bms_ota_init(image_info.type, image_info.totalKbyte, image_info.mainDevId);
    } 
    return true;
}

bool image_flash_erase(uint32_t address, int len) {
    if (iot_image_erase(address, len) != ESP_OK) {
        return false;
    }
    return true;
}

bool image_flash_Write(uint32_t address, const uint8_t *pdata, int len) {
    if (image_info.type == DEVICE_IOT) {
        if (iot_ota_write(pdata, len) != ESP_OK) { /* IOT本身自己升级，不需使用 其他区域 */ 
            return false;
        }
    } else {
        if (iot_image_write(address, pdata, len) != ESP_OK) {
            return false;
        }
    }
    return true;
}

bool image_flash_check(uint32_t address, int len) {

    if (len < 1024) { 
        ESP_LOGE(TAG, "image size too short");
        return false;
    }
    
    if (image_info.type == 0) {
        // esp32 固件自带校验,这里不做校验
    } else { 
        // 找出文件末尾追加的CRC32
        uint32_t crc32A = 0;
        uint32_t endAddr = address + len - 1;
        uint32_t remaining = len;
        for (int i = 0; i < 2048; i++) {
            uint8_t data = 0; 
            if (iot_image_read(endAddr, &data, 1) == ESP_OK) {
                if (data != 0x1A) {   // 排除文件内部填充的0x1A
                    endAddr -= JUMP_BYTES;  // 跳过20字节填充信息
                    remaining -= JUMP_BYTES;
                    iot_image_read(endAddr + 1, (uint8_t *)&crc32A, 4);
                    break;
                }
                endAddr -= 1;
                remaining -= 1;
            } else {
                ESP_LOGI(TAG, "image interval crc32 read failed");
                break;
            }
        }

        uint8_t *buffer = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (buffer == NULL) {
            ESP_LOGI(TAG, "file: %s, function: %s, %d, malloc(1024) falied", __FILE__, __func__, __LINE__);
            return false;
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

        image_crc32 = Swap32(image_crc32);
        if (crc32A != image_crc32) {
            ESP_LOGE(TAG, "image crc32 verify failed (file: 0x%04lX, new: 0x%04lX)", crc32A, image_crc32);
            return false;
        }  
        ESP_LOGI(TAG, "image crc32 verify successfully: (file: 0x%04lx, new: 0x%04lx)", crc32A, image_crc32);

        // 保存image头信息
        image_info.vaildTimes = IMAGE_VAILD_TIMES;
        image_info.totalKbyte = len / 1024; // kb
        image_info.crc16 = xm_calcu_crc16((const uint8_t *)&image_info, sizeof(image_info_t) - 2);
        if (iot_image_write(IMAGE_HEAD_AREA_ADDRESS, (uint8_t *)&image_info, sizeof(image_info_t)) != ESP_OK) {
            ESP_LOGE(TAG, "image head info write falsh failed");
            return false;
        }
    }
    return true;
}

bool image_flash_abort(void) {
    if (image_info.type == 0) {
        if(iot_ota_abort() != ESP_OK) {
            return false;
        }
    } else {
        
    }
    return true;
}

bool image_trans_stop(void) {
    // 假设有文件系统，这里可以关闭文件之类
    if (image_info.type == 0) {
        if(iot_ota_end() != ESP_OK) {
            return false;
        }
    } else {
        // esp32 固件自带校验,这里不在做校验
    }
    return true;
}










