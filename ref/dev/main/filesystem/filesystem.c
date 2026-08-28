#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include "esp_system.h"
#include "filesystem.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "vfs_fat_internal.h"
#include "diskio_impl.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "time.h"
#include "crc.h"
#include "diskio_wl.h"
#include "can_data.h"
#include "Cellular_4g_handle.h"
#include "server2internet.h"
#include "uart_device_process.h"
#include "modbus_data.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "iot_mqtt.h"

#include "can_data.h"
#include "iot_period_task.h"
#include "can_protocol.h"

#define TAG "[fileSystem]"




//#define PROMPT_STR  CONFIG_IDF_TARGET
#define CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH 1024

#define MOUNT_PATH                  root_path
#define HISTORY_PATH MOUNT_PATH     "/history.txt"

//const char *FileTypeString[] = {
// 	"iot", "invarm", "invdsp", "bms", "ba", "packbcu", "packbmu", "packbms", "packm1",
//    "packsafet", "packhv", "iotlcd", "iotlcd2", "RF", "dc_hub", "ac_hub", 
//};

/*
这里sizeof(FileTypeString)得到整个数组的字节数，而sizeof(FileTypeString[0])得到数组中单个元素的字节数。两者相除即得到数组中元素的数量

*/
//const uint8_t TypeCnt = sizeof(FileTypeString) / sizeof(FileTypeString[0]);
 uint8_t TypeCnt =20;// OTA 枚举序号 20， tbd

 factory_struct iot_factory;//iot本身
// inv_factory_struct inv_factory;//下级逆变器,not use


/* 从系统NVS区读取数据，NVS区存储的是key-value数据
 * lable 数据的key值
 * data 存储数据的value值
 * len 存储数据长度 */
esp_err_t iot_read_dev_info (const char *label, uint8_t *data, size_t *len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open nvs error[%s]", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_blob(nvs_handle, label, (char *)data, len);
    if (err != ESP_OK) {
       ESP_LOGE(TAG, "nvs read lable[%s] error[%s]!", label, esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
    return err;
}

/* 数据写入系统NVS区，NVS区存储的是key-value数据
 * lable 数据的key值
 * data 数据的value值
 * len 数据长度 */
esp_err_t iot_wtite_dev_info (const char *label, const uint8_t *data, size_t len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open nvs error[%s]", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, label, (const char *)data, len); /* 写入初始值 */
    ESP_LOGI(TAG, "nvs write %s %s", label, (err != ESP_OK) ? "Failed" : "Done");
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle); /* 提交 */
        ESP_LOGI(TAG, "nvs_commit %s %s",label, (err != ESP_OK) ? "Failed" : "Done");
    }

    nvs_close(nvs_handle);
    return err;
}

void initialize_nvs(void) 
{
    esp_err_t err = nvs_flash_init(); // NVS存储初始化
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void Read_Data_From_NVS(void) 
{
    size_t data_len;

    /*读取IOT工厂参数*/
    data_len = sizeof(iot_factory);
    if (iot_read_dev_info(IOT_FACTORY, (uint8_t *)&iot_factory, &data_len) != ESP_OK) 
    {
        ESP_LOGE(TAG, " IOT_FACTORY READ error");
        iot_wtite_dev_info(IOT_FACTORY, (const uint8_t *)&iot_factory, sizeof(iot_factory));
    }

    memset(iot_factory.iot_type, 0, sizeof(iot_factory.iot_type));
    memcpy(iot_factory.iot_type, IOT_TYPE_IOT, strlen(IOT_TYPE_IOT));

    // 烧录标定使用
    if(iot_factory.iot_sn == 0)
    {
        iot_factory.iot_sn =123456;
    }

    Inv_can_WR.factory.sn = iot_factory.iot_sn;
    Inv_can_WR.factory.safetCode = iot_factory.safe_code;
    memcpy(Inv_can_WR.factory.type, iot_factory.iot_type, sizeof(iot_factory.iot_type));

    ESP_LOGI(TAG, "iot type: %s", iot_factory.iot_type);
    ESP_LOGI(TAG, "Sn Code: %llu", iot_factory.iot_sn);
    ESP_LOGI(TAG, "safety Code: %llu", iot_factory.safe_code);

	ESP_LOGI(TAG, "HUANGJI EEE type:%s -%d -%d",iot_factory.iot_type,  (unsigned int)iot_factory.iot_sn, (unsigned int)iot_factory.safe_code);
	ESP_LOGI(TAG, "HUANGJI EEE type:%s -%d -%d",SetData.dev_info_t.Parallel_dev_type,  (unsigned int)SetData.dev_info_t.Parallel_dev_sn, (unsigned int)SetData.dev_info_t.Parallel_dev_safetycode);
    // ESP_LOGI(TAG, "inv type: %s", inv_factory.inv_type);
    // ESP_LOGI(TAG, "inv Sn Code: %llu", inv_factory.inv_sn);

	vTaskDelay(pdMS_TO_TICKS(50));

}







// 读取IOT出厂SN码
esp_err_t iot_read_factory(uint8_t *data, size_t len) {
    /* 查找分区表，type为0x40、SubType为0x00 */
	const esp_partition_t *custom_data = NULL; 
	custom_data = esp_partition_find_first(0x40, 0x00, "custom_data"); 
    esp_err_t err = esp_partition_read(custom_data, IOT_FACTORY_INFO_ADDRESS, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "iot_read_factory %s failed", esp_err_to_name(err));
    }
    return err;
}

// 写入IOT出厂SN码
esp_err_t iot_write_factory(const uint8_t *data, size_t len) {

	const esp_partition_t *custom_data = NULL;
	custom_data = esp_partition_find_first(0x40, 00, "custom_data");  

    // 先擦除目标在写入
    esp_err_t err = esp_partition_erase_range(custom_data, IOT_FACTORY_INFO_ADDRESS, IOT_FACTORY_INFO_MAX_LEN);
	if (err == ESP_OK) {
        /* 查找分区表，type为0x40、SubType为0x00 */
	    custom_data = esp_partition_find_first(0x40, 0x00, "custom_data");
        err = esp_partition_write(custom_data, IOT_FACTORY_INFO_ADDRESS, data, len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "iot_write_factory %s failed", esp_err_to_name(err));
        }
	}
    return err;
}

esp_err_t iot_read_flash(const char *label, uint8_t *data, size_t len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size = 0;
    size_t read_size = 0;
    err = nvs_open("nvs",  NVS_READONLY, &nvs_handle); // 只读方式打开
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s, Error (%s) opening NVS handle!", label, esp_err_to_name(err));
        return err;
    } 
    err = nvs_get_blob(nvs_handle, label, NULL, &required_size); // 先获取实际存储长度
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s, nvs Error (%s) reading!", label, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    // 如果len长度超出了存储的长度，只读取存储的长度
    if(required_size == 0){
        ESP_LOGE(TAG, "%s, nvs Error (%s) reading!", label, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
    else if(required_size >= len) {
        read_size = len;
    } else {
        read_size = required_size;
        ESP_LOGW(TAG, "%s, flash_store_size=%d, buff size:%d, actual read_size=%d", label, required_size, len, read_size);
    }

    err = nvs_get_blob(nvs_handle, label, (char *)data, &read_size); 
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s, nvs Error (%s) reading!", label, esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
    return err;
}

esp_err_t iot_write_flash(const char *label, const uint8_t *data, size_t len) {
    
    esp_err_t err;
    nvs_handle_t nvs_handle;
    
    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle); // 读写方式打开
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    } 

    err = nvs_set_blob(nvs_handle, label, (const char *)data, len); /* 写入初始值 */
    ESP_LOGI(TAG, "nvs write %s %s", label, (err != ESP_OK) ? "Failed" : "Done");
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle); /* 提交 */
        ESP_LOGI(TAG, "nvs_commit %s %s",label, (err != ESP_OK) ? "Failed" : "Done");
    } 

    nvs_close(nvs_handle);  
    return err;
}

esp_err_t iot_wifi_ssid_write_flash(const char ssid[32]) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("wifi_config",  NVS_READWRITE, &nvs_handle); // 读写
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s, Error (%s) opening NVS handle!\n", "sta_ssid", esp_err_to_name(err));
        return err;
    } 

    err = nvs_set_blob(nvs_handle, "sta_ssid", (const char *)ssid, 32); /* 写入初始值 */
    ESP_LOGI(TAG, "nvs write %s %s", "sta_ssid", (err != ESP_OK) ? "Failed" : "Done");
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle); /* 提交 */
        ESP_LOGI(TAG, "nvs_commit %s %s", "sta_ssid", (err != ESP_OK) ? "Failed" : "Done");
    }  

    return err;
}


esp_err_t iot_wifi_password_write_flash(const char password[32]) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("wifi_config",  NVS_READWRITE, &nvs_handle); // 读写
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s, Error (%s) opening NVS handle!\n", "sta_pwd", esp_err_to_name(err));
        return err;
    } 

    err = nvs_set_blob(nvs_handle, "sta_pwd", (const char *)password, 32); /* 写入初始值 */
    ESP_LOGI(TAG, "nvs write %s %s", "sta_pwd", (err != ESP_OK) ? "Failed" : "Done");
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle); /* 提交 */
        ESP_LOGI(TAG, "nvs_commit %s %s", "sta_pwd", (err != ESP_OK) ? "Failed" : "Done");
    }  
    return err;
}

esp_err_t iot_wifi_para_read(char ssid[32], char password[32])
{
    esp_err_t err = ESP_OK;
    return err;
}

esp_err_t iot_wifi_para_write(const char ssid[32], const char password[32])
{
    esp_err_t err;
    err = iot_wifi_password_write_flash(password);
    if (err != ESP_OK) {

    }
    err = iot_wifi_ssid_write_flash(ssid);
    if (err != ESP_OK) {

    }
    return err;
}

#if 1
esp_err_t iot_image_erase(uint32_t address, size_t len)
{
    // Find the next OTA update partition
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);

    // Erase entire partition
    return esp_partition_erase_range(partition, address, len);
}

/*
读取备份区flash
*/
esp_err_t iot_image_read(uint32_t address, uint8_t *data, size_t len) {
    esp_err_t err;
    // Find the next OTA update partition
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    // ESP_LOGI(TAG, "iot_image_read, partition.addr:0x%08lx address:0x%08lx, len:%d", partition->address, address, len);
    // read the data, starting from the beginning of the partition
    err = esp_partition_read(partition, address, data, len);
    // dump_buf("iot_image_read data:", data, len);
    return err;
}

/*
非IOT本身的 OTA写入；借用IOT的APPx非运行分区
*/
esp_err_t iot_image_write(uint32_t address, const uint8_t *data, size_t len) {
    esp_err_t err;
    // Find the next OTA update partition
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);    

    // ESP_LOGI(TAG, "iot_image_write, partition.addr:0x%08lx address:0x%08lx, len:%d", partition->address, address, len);
    // dump_buf("iot_image_write data:", data, len);
    // Write the data, starting from the beginning of the partition
    err = esp_partition_write(partition, address, data, len);
    return err;
}

#else
esp_err_t iot_image_erase(uint32_t address, size_t len) {

    // Find the partition map in the partition table
    const esp_partition_t *partition = esp_partition_find_first(0x3A, 0x00, "custom_data");

    // Erase entire partition
    return esp_partition_erase_range(partition, address, len);
}

esp_err_t iot_image_read(uint32_t address, uint8_t *data, size_t len) {
    esp_err_t err;
    // Find the partition map in the partition table
    const esp_partition_t *partition = esp_partition_find_first(0x3A, 0x00, "custom_data");

    // read the data, starting from the beginning of the partition
    err = esp_partition_read(partition, address, data, len);
    return err;
}

esp_err_t iot_image_write(uint32_t address, const uint8_t *data, size_t len) {
    esp_err_t err;
    // Find the partition map in the partition table
    const esp_partition_t *partition = esp_partition_find_first(0x3A, 0x00, "custom_data");

    // Write the data, starting from the beginning of the partition
    err = esp_partition_write(partition, address, data, len);
    return err;
}
#endif

/*
读取当前运行APP区flash
*/
esp_err_t iot_image_read_From_Appx_backup_Part(uint32_t address, uint8_t *data, size_t len) 
{
    esp_err_t err;
#if 0    
    // Find the partition map in the partition table
//    const esp_partition_t *partition = esp_partition_find_first(0x3A, 0x00, "custom_data");
	// 获取当前运行的分区
	const esp_partition_t *partition = esp_ota_get_running_partition();
	if (partition == NULL) {
		ESP_LOGE(TAG, "iot_image_read_From_Running_Part ：Could not find running partition");
		return -1;
	}

#else
    // Find the next OTA update partition
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
#endif
    // read the data, starting from the beginning of the partition
    err = esp_partition_read(partition, address, data, len);
    return err;
}

/*
//大端模式获取2字节长度的数据
uint16_t  GetWordFromPtrBig( const uint8_t* uBufPoint)
{
    return (((uint16_t)uBufPoint[0])<<8)|((uint16_t) uBufPoint[1]);
}   

//大端模式获取4字节长度的数据
uint32_t  GetDWordFromPtrBig( const uint8_t* uBufPoint)
{
    return (((uint32_t)uBufPoint[0])<<24)|((uint32_t) (uBufPoint[1]<<16)|(uint32_t)(uBufPoint[2]<<8)|(uint32_t)uBufPoint[3]);
}   

//小端模式获取2字节长度的数据
uint16_t  GetWordFromPtrSmall( const uint8_t* uBufPoint)
{
    return (((uint16_t)uBufPoint[1])<<8)|((uint16_t) uBufPoint[0]);
}   

//小端模式获取4字节长度的数据
uint32_t  GetDWordFromPtrSmall( const uint8_t* uBufPoint)
{
    return (((uint32_t)uBufPoint[3])<<24)|((uint32_t) (uBufPoint[2]<<16)|(uint32_t)(uBufPoint[1]<<8)|(uint32_t)uBufPoint[0]);
}  
*/
 











