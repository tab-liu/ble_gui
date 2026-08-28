#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"

#include "esp_littlefs.h"

#include "littlefs.h"
//#include "iot_crc.h"

//#include "system_data.h"

#define TAG "littlefs"

SemaphoreHandle_t xFileSemaphore = NULL; 


void mount_littlefs(void) 
{
    xFileSemaphore = xSemaphoreCreateMutex();
    if (xFileSemaphore == NULL) {
        return ;
    }

    ESP_LOGI(TAG, "Initializing LittelFS");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = FLASH_BASE_PATH,
        .partition_label = FLASH_PARTITION_NAME,//"disk",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}


/**
 * @brief 获取文件大小
 * @param path 文件路径
 * @return 读取成功返回文件大小，失败返回-1
 */
int get_file_size(const char *path)
{
    int file_size = -1;

    if (NULL == path)
    {
        return -1;
    }

    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

    FILE *file = fopen(path, "rb"); 
    if (file)
    {
        // 移动文件指针到文件末尾，通过ftell获取文件大小
        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        fclose(file);
    }

    xSemaphoreGive(xFileSemaphore); 
    return file_size;
}

// 注：当数据结构体大小变化时，仍能读取数据，新增部分数据为0，需要额外逻辑赋初始值
int read_file(const char *path, uint8_t *data, int len)
{
    int err = -1;
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));
    FILE *file = fopen(path, "rb"); 
    if (file) {
        fseek(file, 0, SEEK_END);
        int file_size = ftell(file); // 先获取文件大小
        fseek(file, 0, SEEK_SET);
        int read_size = (file_size < len) ? file_size : len; // 实际读取文件大小
        if (fread(data, 1, read_size, file) == read_size) {
            err = 0;
        }
        fclose(file);
    }
    xSemaphoreGive(xFileSemaphore); 
    return err;
}
   
int write_file(const char *path, uint8_t *data, int len)  //
{   
    int err = -1;
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));
    FILE *pfile = fopen(path, "wb"); // 打开/创建一个的文件,文件存在内容清空
    if (pfile) {
        if (fwrite(data, len, 1, pfile) == 1) {
            err = 0;
        } 
        fclose(pfile);
    }
    xSemaphoreGive(xFileSemaphore); 
    return err;
}

void file_list(const char *path) {
    DIR *dir;
    struct dirent *ptr;
    char base[255];

    if ((dir = opendir(path)) == NULL) {
        ESP_LOGE(TAG, "Open dir %s error...", path);
        return ;
    }
    
    ESP_LOGI(TAG,"open dir %s", path);
    while ((ptr = readdir(dir)) != NULL) {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0) {   ///current dir OR parrent dir
            continue;
        } else if(ptr->d_type == DT_DIR) {     ///dir
            memset(base,'\0',sizeof(base));
            strcpy(base, path);
            strcat(base,"/");
            strcat(base, ptr->d_name);
            file_list(base);
        } else {
            ESP_LOGI(TAG,"%s/%s", path, ptr->d_name);
        }
    }
    closedir(dir);
    return ;
}
