/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName: 
Author: weiyt
Date: 2022/10/26
Description: nvs接口加密和解密
Version: V1.00
Function List: none
***********************************************************/
#include "iot_nvs_encrypt.h"


#define TAG "encrypt_nvs"
#define NVS_FLAG '#'
#define CERT_INFO    "cert_info"



//16进制打印
static void  dump_partition(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

//ascii打印
static void  dump_partition_ascii(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%c%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

//分区信息体格式化
void nvs_data_init(NVS_DATA_UNION *data)
{
    data->NvsDataStruct.start_flag[0] = NVS_FLAG; //标志
    data->NvsDataStruct.start_flag[1] = NVS_FLAG;
    data->NvsDataStruct.ciphertext_len = 0; //密文长度
    data->NvsDataStruct.plaintext_len = 0;  //明文长度
}

//获取nvs明文长度
esp_err_t nvs_read_len(const char *label, uint16_t *len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = ESP_FAIL;
    NVS_DATA_UNION nvs_data_info;
    size_t get_len = 0;
    uint8_t *raw_buffer = NULL;
    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    } 

    err = nvs_get_blob(nvs_handle, label, NULL, &get_len); //获取nvs数据的长度
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs Error (%s) reading!", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "get_len:%d", get_len);
    if(get_len > 16)
    {
        raw_buffer = (uint8_t*)heap_caps_malloc((get_len+1)*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
         if(raw_buffer == NULL)
        {
            ESP_LOGE(TAG, "malloc fail!");
            return err;
        }
        memset(raw_buffer, 0, get_len+1);
        err = nvs_get_blob(nvs_handle, label, (char *)raw_buffer, &get_len); //读取NVS所有数据
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs Error (%s) reading!", esp_err_to_name(err));
        }
        memcpy(nvs_data_info.data, raw_buffer, sizeof(nvs_data_info.data));
        *len = nvs_data_info.NvsDataStruct.plaintext_len;
 //       ESP_LOGI(TAG, "get, plaintext len:%lu, ciphertext_len:%lu", nvs_data_info.NvsDataStruct.plaintext_len, nvs_data_info.NvsDataStruct.ciphertext_len);

        free(raw_buffer);
        if(nvs_data_info.NvsDataStruct.start_flag[0] != NVS_FLAG || nvs_data_info.NvsDataStruct.start_flag[1] != NVS_FLAG)
        {
            err = ESP_FAIL;
        }

    }
    nvs_close(nvs_handle);
    return err;
}

//读取分区明文数据数据
esp_err_t nvs_read_decrypt(const char *label, uint8_t *data, uint32_t len)
{
    uint8_t *raw_buffer = NULL;
    uint8_t *aes_buffer = NULL;
    uint16_t aes_len = 0;
    size_t get_len = 0;

    nvs_handle_t nvs_handle;
    esp_err_t err = ESP_FAIL;
    NVS_DATA_UNION nvs_data_info;
    memset(&nvs_data_info, 0, sizeof(nvs_data_info));
    if(len == 0)
    {
        return 0;
    }

    err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    } 

    err = nvs_get_blob(nvs_handle, label, NULL, &get_len); //获取nvs数据的长度
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs Error (%s) reading!", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    if(get_len == 0)
    {
        ESP_LOGE(TAG, ":%s no data", label);
    }
    else
    {
        raw_buffer = (uint8_t*)heap_caps_malloc((get_len+1)*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
        if(raw_buffer == NULL)
        {
            ESP_LOGE(TAG, "malloc fail!");
            nvs_close(nvs_handle);
            return err;
        }
        memset(raw_buffer, 0, get_len+1);
        err = nvs_get_blob(nvs_handle, label, (char *)raw_buffer, &get_len); //读取NVS所有数据
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs Error (%s) reading!", esp_err_to_name(err));
            free(raw_buffer);
            nvs_close(nvs_handle);
            return err;
        }

        memcpy(nvs_data_info.data, raw_buffer, sizeof(nvs_data_info.data));
    
 //       ESP_LOGI(TAG, "read_decrypt, plaintext len:%lu, ciphertext_len:%lu", nvs_data_info.NvsDataStruct.plaintext_len, nvs_data_info.NvsDataStruct.ciphertext_len);
        if (nvs_data_info.NvsDataStruct.ciphertext_len == 0 ||
            (size_t)nvs_data_info.NvsDataStruct.ciphertext_len + 16 > get_len) {
            ESP_LOGE(TAG, "invalid ciphertext_len:%lu get_len:%u",
                     (unsigned long)nvs_data_info.NvsDataStruct.ciphertext_len, (unsigned)get_len);
            free(raw_buffer);
            nvs_close(nvs_handle);
            return ESP_FAIL;
        }
        aes_buffer = (uint8_t*)heap_caps_malloc(nvs_data_info.NvsDataStruct.ciphertext_len*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
        if(aes_buffer == NULL)
        {
            ESP_LOGE(TAG, "malloc fail!");
            free(raw_buffer);
            nvs_close(nvs_handle);
            return err;
        }
        iot_file_decrypt(raw_buffer+16, nvs_data_info.NvsDataStruct.ciphertext_len, aes_buffer, &aes_len); //AES解密
        memcpy(data, aes_buffer, (len>nvs_data_info.NvsDataStruct.plaintext_len)?nvs_data_info.NvsDataStruct.plaintext_len:len);
        free(raw_buffer);
        free(aes_buffer);
    }
    nvs_close(nvs_handle);   
    return err;
}

//nvs加密写入
esp_err_t nvs_write_decrypt(const char *label, uint8_t *data, uint32_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = ESP_FAIL;
    uint8_t *aes_buffer = NULL;
    uint16_t aes_len = 0;
    NVS_DATA_UNION nvs_data_info;
    if(len > 0)
    {
        nvs_data_init(&nvs_data_info);
        err = nvs_open("nvs", NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
            return err;
        }    
        nvs_data_info.NvsDataStruct.plaintext_len = len; //明文长度
 //       ESP_LOGI(TAG, "plaintext_len:%lu", nvs_data_info.NvsDataStruct.plaintext_len);
        aes_len = (len >= 16)?((len%16 != 0)?(len-len%16+16):len):16;
        nvs_data_info.NvsDataStruct.ciphertext_len = aes_len; //密文长度
 //       ESP_LOGI(TAG, "ciphertext_len:%lu", nvs_data_info.NvsDataStruct.ciphertext_len);
        aes_buffer = (uint8_t*)heap_caps_malloc((aes_len+16)*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
        if(aes_buffer == NULL)
        {
            ESP_LOGE(TAG, "malloc fail!");
            return err;
        }
        iot_file_encrypt(data, len, aes_buffer+16, &aes_len); //AES加密
        memcpy(aes_buffer, nvs_data_info.data, sizeof(nvs_data_info.data));
        err = nvs_set_blob(nvs_handle, label, (const char *)aes_buffer, aes_len+16); /* 写入初始值 */
        ESP_LOGI(TAG, "nvs write %s %s", label, (err != ESP_OK) ? "Failed" : "Done");
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle); /* 提交 */
            ESP_LOGI(TAG, "nvs_commit %s %s",label, (err != ESP_OK) ? "Failed" : "Done");
        } 
        if(aes_buffer != NULL)
        {
            free(aes_buffer);
        }
        nvs_close(nvs_handle);   
    }
    return err;
}

esp_err_t save_cert_md5_to_nvs(const uint8_t *data, size_t len)
{
    return nvs_write_decrypt(CERT_INFO, data, len);
}

esp_err_t read_cert_md5_from_nvs(uint8_t *data, size_t len) 
{
    return nvs_read_decrypt(CERT_INFO, data, len);
}