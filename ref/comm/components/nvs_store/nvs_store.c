#include "nvs_store.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>


#define TAG "[nvs_store]"

/**
 * @brief nvs存储初始化
 * 
 */
void nvs_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase()); // 初始化失败时擦除后重新初始化
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

/**
 * @brief 写入数据到nvs
 * - 该函数将使用默认的分区表"nvs"来写入数据
 * - 如果需要使用其它分区表，需要使用nvs_open_from_partition函数打开分区，并传入分区名称
 * 
 * @param label 数据标签
 * @param data 数据指针
 * @param len 数据长度
 * @return 成功返回0,失败返回-1
 */
int nvs_store_write(const char *label, const uint8_t *data, size_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "open nvs error: %s", esp_err_to_name(err));
        return -1;
    }

    err = nvs_set_blob(nvs_handle, label, data, len);
    ESP_LOGI(TAG, "nvs write %s %s", label, (err != ESP_OK) ? "failed" : "done");
    if (err != ESP_OK) goto __exit;

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_commit %s failed", label);
    }

__exit:
    nvs_close(nvs_handle);
    return (err == ESP_OK) ? (0) : (-1);
}

/**
 * @brief 从nvs中读取数据
 * 
 * @param label 数据标签
 * @param data 数据指针
 * @param len 数据长度
 * @return 成功返回0,失败返回-1 
 */
int nvs_store_read(const char *label, uint8_t *data, size_t *len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "open nvs error: %s", esp_err_to_name(err));
        return -1;
    }

    err = nvs_get_blob(nvs_handle, label, (char *)data, len);
    if (err != ESP_OK)
    {
       ESP_LOGE(TAG, "nvs read lable failure, lable: %s, error: %s, len:%d", label, esp_err_to_name(err), *len);
    }

    nvs_close(nvs_handle);
    return (err == ESP_OK) ? (0) : (-1);
}

/**
 * @brief 从nvs中删除数据
 * 
 * @param label 数据标签
 * @return 成功返回0,失败返回-1 
 */
int nvs_store_erase(const char *label)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    /* 未找到相应类型（已经被删除），返回成功 */
    nvs_iterator_t it = NULL;
    err = nvs_entry_find("nvs", "nvs", NVS_TYPE_BLOB, &it);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "not found blob type, the label[%s] may have been deleted", label);
        return 0;
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs error: %s", esp_err_to_name(err));
        return -1;
    }

    /* 查询lable是否存在 */
    int found = 0;
    do {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        err = nvs_entry_next(&it);
        if (strcmp(info.key, label) == 0)
        {
            found = 1;
            break;
        }
    } while (err == ESP_OK);

    /* 未找到该标签（已经被删除），返回成功 */
    if (found == 0)
    {
        ESP_LOGI(TAG, "not found label[%s], it may have been deleted", label);
        return 0;
    }

    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "open nvs error: %s", esp_err_to_name(err));
        return -1;
    }

    err = nvs_erase_key(nvs_handle, label);
    ESP_LOGI(TAG, "nvs erase %s %s", label, (err != ESP_OK) ? "failed" : "done");
    if (err != ESP_OK) goto __exit;

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "nvs_commit %s failed", label);
    }

__exit:
    nvs_close(nvs_handle);
    return (err == ESP_OK) ? (0) : (-1);
}
