#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "json_cmd.h"
#include "esp_log.h"
#include "utils.h"

#include <stdio.h>

#define TAG "JSONs"

#define DEV_SN_CODE     "sn"
#define DEV_MODEL       "model"
#define OTA_FORCE       "force"
#define FILE_TYPE       "firmwareType"
#define FILE_SIZE       "fileSize"
#define FILE_VER        "version"
#define FILE_URL        "downloadUrl"
#define FILE_MD5        "fileMd5"
#define FILE_ENCRY      "encrypted"
#define FILE_RECORDID   "recordId"
#define FOURCE_UPDATE   "forcedUpdate"

ota_cmd_list_t * json_ota_cmd_parse(const char *message) {
    
/**
[{
        "sn": "EP6002326000001749",
        "model": "EP600",
        "force": false,   // 是否强制升级 
        "details": [
            {   
                "firmwareType": 1,  // 固件类型 0:物联网  1:ARM  2:DSP 3:BMS
                "fileSize": 101148,
                "version": "500435",
                "downloadUrl": "https://dev-oss.poweroak.ltd:18443/blu-smart-product/18a0257af6cb29b4b12b67cce1c.bin",
                "fileMd5": "253fb766eb4a1df4ca742cf277dc8654",
                "encrypted": false,
                "recordId":"18a180ef4faee2d856c48a4d2a0"   //  固件升级记录ID
            },
            {   
                "firmwareType": 2,
                "fileSize": 131072,
                "version": "500334",
                "downloadUrl": "https://dev-oss.poweroak.ltd:18443/blu-smart-product/189e24890d7b29b4b4cc61c79d0.bin",
                "fileMd5": "e0859ffd87fb5cf8e9809245f4ad565d",
                "encrypted": false,
                "recordId":"18a180ef4faee2d856c48a4d2a0"   //  固件升级记录ID
            }
        ]
    }]
 */

/*
[{
        "sn":"COMBOX2419000001756",
        "model":"COMBOX",
        "force":true,
        "details":[
            {
                "firmwareId":"18f57873105c1dbdb2d721176ea",
                "firmwareType":13,
                "fileSize":401428,
                "version":"907202",
                "downloadUrl":"https://dev-oss.poweroak.ltd:18443/blu-smart-product/18f5787316db29b4b56cbb4b144.bin",
                "fileMd5":"4c31577f6f7e8213bc4f9acc38f48436",
                "encrypted":false,
                "recordId":"18f7aeb04e4c1dbdb5a7c8af37b"
            }
        ]
    }] 
*/
    cJSON* cjson_object = cJSON_Parse(message);
    if(cjson_object == NULL) {
        ESP_LOGE(TAG, "json parse failed");
        return NULL;
    }

    ESP_LOGW(TAG,"%s \n", message);
    ota_cmd_list_t  *ota_cmd_head = NULL;
    ota_cmd_list_t  *ota_cmd = NULL;
    int dev_num = cJSON_GetArraySize(cjson_object);
    for (int i = 0; i < dev_num; i++) {
        cJSON* dev_item = cJSON_GetArrayItem(cjson_object, i);
        if (!dev_item) {
            continue;
        }

        cJSON*  cjson_device_sn     = cJSON_GetObjectItem(dev_item, DEV_SN_CODE); // dev model + sn
        cJSON*  cjson_device_model  = cJSON_GetObjectItem(dev_item, DEV_MODEL);   // 
        cJSON*  cjson_ota_force     = cJSON_GetObjectItem(dev_item, OTA_FORCE);
        
        ESP_LOGI(TAG, "ota cmd: device: %s, model: %s, focre: %d",  cjson_device_sn->valuestring, 
                                                                    cjson_device_model->valuestring,
                                                                    cjson_ota_force->valueint);
                                                                    
        cJSON*  cjson_details = cJSON_GetObjectItem(dev_item, "details");
        if (cjson_details) {
            int image_num = cJSON_GetArraySize(cjson_details);
            for (int j = 0; j < image_num; j++) {
                cJSON* array_item = cJSON_GetArrayItem(cjson_details, j);
                if (!array_item) {
                    continue;
                }
                
                cJSON* cjson_file_type = cJSON_GetObjectItem(array_item, FILE_TYPE);
                cJSON* cjson_file_size = cJSON_GetObjectItem(array_item, FILE_SIZE);
                cJSON* cjson_file_ver  = cJSON_GetObjectItem(array_item, FILE_VER);
                cJSON* cjson_file_url  = cJSON_GetObjectItem(array_item, FILE_URL);
                cJSON* cjson_file_md5  = cJSON_GetObjectItem(array_item, FILE_MD5);
                cJSON* cjson_recordId  = cJSON_GetObjectItem(array_item, FILE_RECORDID);
                
                ota_cmd_list_t  *curr_cmd = (ota_cmd_list_t *)iot_calloc(sizeof(ota_cmd_list_t));                                     
                if (!curr_cmd) {
                    continue;
                }         

                memset(curr_cmd, 0, sizeof(ota_cmd_list_t));
                curr_cmd->next = NULL;
                curr_cmd->object.force = cjson_ota_force->valueint;
                curr_cmd->object.type = cjson_file_type->valueint;
                curr_cmd->object.version = (uint32_t)strtol(cjson_file_ver->valuestring, NULL, 0);
                curr_cmd->object.total_bytes = cjson_file_size->valueint;
                ESP_LOGI(TAG, "file: type: %d, size: %d, version: %lu",  cjson_file_type->valueint, 
                                                                        cjson_file_size->valueint,
                                                                        (uint32_t)strtol(cjson_file_ver->valuestring, NULL, 0));  
                memcpy(curr_cmd->object.model, cjson_device_model->valuestring, strlen(cjson_device_model->valuestring) % 12); //  
                memcpy(curr_cmd->object.dev_model_ascii, cjson_device_sn->valuestring, strlen(cjson_device_sn->valuestring) % 32); //             
                memcpy(curr_cmd->object.md5_code,    cjson_file_md5->valuestring,  strlen(curr_cmd->object.md5_code) % 32); // md5
                memcpy(curr_cmd->object.recordId,    cjson_recordId->valuestring,  strlen(cjson_recordId->valuestring) % (32+1)); // recrdid
                memcpy(curr_cmd->object.url,         cjson_file_url->valuestring,  strlen(cjson_file_url->valuestring) % (256+1)); // url
                
                ESP_LOGI(TAG, "url: %s, md5: %s, recordId: %s", curr_cmd->object.url, 
                                                                curr_cmd->object.md5_code,
                                                                curr_cmd->object.recordId);                
                if (ota_cmd_head == NULL) {
                    ota_cmd_head = curr_cmd;
                    ota_cmd = curr_cmd;
                } else {
                    ota_cmd->next = curr_cmd;
                    ota_cmd = curr_cmd;
                }
            }
        }
    }
    cJSON_Delete(cjson_object);
    return ota_cmd_head;
}

int json_ota_request(uint8_t image_num, char *type, uint64_t sn, new_image_req_t image_req[], uint8_t *output, int max_len) {
/*
* 
{
    "type": 2,
    "topic": "EP600.2326000001749",
    "data": {
        "sn": "EP6002326000001749",
        "model": "EP600",
        "firmwareVers": [
          {
            "firmwareId": 1,    // 固件类型 0:物联网  1:ARM  2:DSP 3:BMS
            "ver": 500415
          },
          {
            "firmwareId": 2,   // 固件类型 0:物联网  1:ARM  2:DSP 3:BMS
            "ver": 500313
          }
       ]
    }
}
* 
* */
    char str[128] = {0};
    snprintf(str, 128, "%.*s.%013llu", (int)strnlen(type, TYPE_SIZE), type, sn);
    cJSON* cjson_req = cJSON_CreateObject();
    cJSON_AddNumberToObject(cjson_req, "type", 2); 
    cJSON_AddStringToObject(cjson_req, "topic", str);

    cJSON* cjson_data_array = cJSON_CreateArray();
    for (uint8_t i = 0; i < image_num; i++) {
        cJSON* cjson_data = cJSON_CreateObject();
        char _str[128] = {0};
        snprintf(_str, 128, "%.*s%013llu", (int)strnlen(image_req[i].dev_type, TYPE_SIZE), image_req[i].dev_type, image_req[i].dev_sn); // vasprintf("%s%d", image_req.dev_type, image_req.dev_sn);
        cJSON_AddStringToObject(cjson_data, "sn", _str);
        cJSON_AddStringToObject(cjson_data, "model", image_req[i].dev_type);
        
        cJSON* cjson_arry = cJSON_CreateArray();
        for (uint8_t j = 0; j < image_req[i].ver_num; j++) {
            cJSON* cjson_version = cJSON_CreateObject();
            cJSON_AddNumberToObject(cjson_version, "firmwareId", image_req[i].ver_list[j].type);
            cJSON_AddNumberToObject(cjson_version, "ver", image_req[i].ver_list[j].version);
            cJSON_AddItemToArray(cjson_arry, cjson_version);
        }
        cJSON_AddItemToObject(cjson_data, "firmwareVers", cjson_arry);
        cJSON_AddItemToArray(cjson_data_array, cjson_data);
    }
    cJSON_AddItemToObject(cjson_req, "data", cjson_data_array);

    char *pdata = cJSON_PrintUnformatted(cjson_req);
    int len = strlen(pdata) % max_len;
    memcpy(output, pdata, len);
    ESP_LOGW(TAG, "%s, json len: %d", output, len);
    cJSON_free(pdata);
    cJSON_Delete(cjson_req);
    return len;
}

char * json_ota_response(const char recordId[32], const char *status, const char *reason) {
/*
{   "upgradeStatus": "upgrading" , //  download(下载完成)、upgrading(升级中) 、success(升级成功) 、failure(升级失败)
    "recordId": "18a180ef4faee2d856c48a4d2a0",  //  固件升级记录ID
    "remark":"" // 失败原因
}
* */
    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        cJSON_AddStringToObject(cjson_data, "upgradeStatus", status);
        cJSON_AddStringToObject(cjson_data, "recordId", recordId);
        cJSON_AddStringToObject(cjson_data, "remark", reason);
        char *pdata = cJSON_PrintUnformatted(cjson_data);
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
}

char *json_ota_http_request(char *type, uint64_t sn, ver_data_t ver_list[], uint8_t ver_num) 
{
/**
[{
    "model": "AC380",
    "sn": "AC3802236002361142",
    "firmwareVers": [{
        "firmwareId": 1,
        "ver": "100101"
    }, {
        "firmwareId": 2,
        "ver": "206801"
    }, {
        "firmwareId": 3,
        "ver": "107302"
    }]
}] */
    char str[256] = {0};
    snprintf(str, sizeof(str), "%.*s%013llu", (int)strnlen(type, TYPE_SIZE), type, sn);
    cJSON* cjson_req_array = cJSON_CreateArray();
    if (cjson_req_array == NULL) {
        return NULL; // Error handling
    }

    cJSON* cjson_req = cJSON_CreateObject();
    if (cjson_req == NULL) {
        cJSON_Delete(cjson_req_array);
        return NULL; // Error handling
    }
    cJSON_AddItemToArray(cjson_req_array, cjson_req);
    cJSON_AddStringToObject(cjson_req, "model", type);
    cJSON_AddStringToObject(cjson_req, "sn", str);

    cJSON* cjson_firmwareVers = cJSON_CreateArray();
    if (cjson_firmwareVers == NULL) {
        cJSON_Delete(cjson_req_array);
        return NULL; // Error handling
    }
    cJSON_AddItemToObject(cjson_req, "firmwareVers", cjson_firmwareVers);

    for (uint8_t j = 0; j < ver_num; j++) {
        cJSON* cjson_version = cJSON_CreateObject();
        if (cjson_version == NULL) {
            cJSON_Delete(cjson_req_array);
            return NULL; // Error handling
        }
        cJSON_AddNumberToObject(cjson_version, "firmwareId", ver_list[j].type);
        snprintf(str, sizeof(str), "%ld", ver_list[j].version);
        cJSON_AddStringToObject(cjson_version, "ver", str);
        cJSON_AddItemToArray(cjson_firmwareVers, cjson_version);
    }

    char *pdata = cJSON_PrintUnformatted(cjson_req_array);
    if (pdata == NULL) {
        cJSON_Delete(cjson_req_array);
        return NULL; // Error handling
    }

    int len = strlen(pdata);
    ESP_LOGW(TAG, "%s, json len: %d", pdata, len);
    cJSON_Delete(cjson_req_array); // 删除整个数组，包括其中的对象
    return pdata;
}

ota_cmd_list_t *json_parse_ota_rsp(const char *message){
    /*示例
    { "appId":null,
        "msgId":"17c504a22e5f79115029cdbe2b4",
        "msgCode":0,
        "message":"OK",
        "data":[
        {  
            "sn":"D100P2428000012239",
            "model":"D100P",
            "versions":[
            {
                "firmwareId":"191797e405300b8b13d6b5bd65e",
                "firmwareType":0,
                "fileSize":"1665168",
                "version":"802393",
                "downloadUrl":"https://dev-oss.poweroak.ltd:18443/blu-smart-product/191797e41904b43580b1d7cf9b9.bin",
                "fileMd5":"60c18fcbfeb2823c0111e70346916ebe",
                "encrypted":false,
                "recordId":null,
                "upgradeInstruction":"",
                "forcedUpdate":false
            },{
                "firmwareId":"1917294af5c2238c70b3b416cd5",
                "firmwareType":2,
                "fileSize":"114664",
                "version":"907979",
                "downloadUrl":"https://dev-oss.poweroak.ltd:18443/blu-smart-product/19172936ab84b43580b1d7cf830.bin",
                "fileMd5":"39862efa672cac9871d5186b70f6a67f",
                "encrypted":false,
                "recordId":null,
                "upgradeInstruction":"",
                "forcedUpdate":false
            }]
        }],
        "error":null,
        "code":0} 
    */
    cJSON* cjson_object = cJSON_Parse(message);
    if(cjson_object == NULL) {
        ESP_LOGE(TAG, "json parse failed");
        return NULL;
    }

    ESP_LOGW(TAG,"%s\n", message);
    ota_cmd_list_t  *ota_cmd_head = NULL;
    ota_cmd_list_t  *ota_cmd = NULL;
          
    cJSON* cjson_data = cJSON_GetObjectItem(cjson_object, "data");
    if(cjson_data == NULL) {
        ESP_LOGE(TAG, "json parse failed, without data");
        cJSON_Delete(cjson_object);
        return NULL;
    }
    cJSON* cjson_details = cJSON_GetArrayItem(cjson_data, 0);
    if (cjson_details) {
        cJSON* cjson_device_sn     = cJSON_GetObjectItem(cjson_details, DEV_SN_CODE); // dev model + sn
        cJSON* cjson_device_model  = cJSON_GetObjectItem(cjson_details, DEV_MODEL);   //
        if(cjson_device_sn == NULL || cjson_device_model == NULL) {
            ESP_LOGE(TAG, "json parse failed, without sn or model");
            cJSON_Delete(cjson_object);
            return NULL;
        }
        ESP_LOGI(TAG, "ota cmd: device: %s, model: %s",  cjson_device_sn->valuestring, 
                                                        cjson_device_model->valuestring);
        cJSON* cjson_versions = cJSON_GetObjectItem(cjson_details, "versions");
        if(cjson_versions){
            int image_num = cJSON_GetArraySize(cjson_versions);
            for (int j = 0; j < image_num; j++) {
                cJSON* array_item = cJSON_GetArrayItem(cjson_versions, j);
                if (!array_item) {
                    continue;
                }
                
                const cJSON* cjson_file_type = cJSON_GetObjectItem(array_item, FILE_TYPE);
                const cJSON* cjson_file_size = cJSON_GetObjectItem(array_item, FILE_SIZE);
                const cJSON* cjson_file_ver  = cJSON_GetObjectItem(array_item, FILE_VER);
                const cJSON* cjson_file_url  = cJSON_GetObjectItem(array_item, FILE_URL);
                const cJSON* cjson_file_md5  = cJSON_GetObjectItem(array_item, FILE_MD5);
                const cJSON* cjson_file_recordId  = cJSON_GetObjectItem(array_item, FILE_RECORDID);
                const cJSON* cjson_file_force  = cJSON_GetObjectItem(array_item, FOURCE_UPDATE);
                
                if(!cjson_file_type || !cjson_file_size || !cjson_file_ver || !cjson_file_url
                    || !cjson_file_md5 || !cjson_file_force) {
                    ESP_LOGW(TAG, "json parse failed, without expected item");
                    continue;
                }

                if( !cjson_file_size->valuestring || !cjson_file_ver->valuestring || !cjson_file_url->valuestring ) {
                    ESP_LOGW(TAG, "json parse failed, without expected item");
                    continue;
                }

                ota_cmd_list_t  *curr_cmd = (ota_cmd_list_t *)iot_calloc(sizeof(ota_cmd_list_t));                                     
                if (!curr_cmd) {
                    ESP_LOGE(TAG, "json parse failed, without memory");
                    continue;
                }         

                memset(curr_cmd, 0, sizeof(ota_cmd_list_t));
                curr_cmd->next = NULL;
                curr_cmd->object.type = cjson_file_type->valueint;
                curr_cmd->object.version = (uint32_t)strtol(cjson_file_ver->valuestring, NULL, 0);
                curr_cmd->object.total_bytes = (uint32_t)strtol(cjson_file_size->valuestring, NULL, 0);
                strncpy(curr_cmd->object.url, cjson_file_url->valuestring, 256);
                strncpy(curr_cmd->object.md5_code, cjson_file_md5->valuestring, 32);
                strncpy(curr_cmd->object.model, cjson_device_model->valuestring, 12);
                strncpy(curr_cmd->object.dev_model_ascii, cjson_device_sn->valuestring, 32);
                curr_cmd->object.force = cJSON_IsTrue(cjson_file_force)? 1 : 0;
                if(cjson_file_recordId && cJSON_IsString(cjson_file_recordId)) {
                    strncpy(curr_cmd->object.recordId, cjson_file_recordId->valuestring, 32);
                    // ESP_LOGI(TAG, "recordId: %s", cjson_file_recordId->valuestring);
                } else {
                    memset(curr_cmd->object.recordId, 0, 32);
                    ESP_LOGW(TAG, "recordId is null");
                }

                ESP_LOGI(TAG, "file: type: %d, size: %lu, version: %lu, url:%s",  cjson_file_type->valueint, 
                                        (uint32_t)strtol(cjson_file_size->valuestring, NULL, 0),
                                        (uint32_t)strtol(cjson_file_ver->valuestring, NULL, 0),
                                        cjson_file_url->valuestring);

                if (ota_cmd_head == NULL) {
                    ota_cmd_head = curr_cmd;
                    ota_cmd = curr_cmd;
                } else {
                    ota_cmd->next = curr_cmd;
                    ota_cmd = curr_cmd;
                }
            }
        }
    }
    cJSON_Delete(cjson_object);
    return ota_cmd_head;
}

/*
{
  "deviceSn": "123123",
  "recordId": "",
  "remark": "测试",
  "upgradeStatus": "download test",
  "upgradeVer": "888811",
  "firmwareType":0
  "oldVer": "888800",
  "deviceModel": "EP600",
  "connMode": 2 // 1:蓝牙 2:WIFI 3:RFID
}
* */
char * json_ota_response_first(const char recordId[32], const char *status, const char *reason,
                            uint64_t sn, char *type, uint8_t image_type, uint32_t new_ver, uint32_t old_ver)
{
    if(!recordId || !status || !reason || !type){
        return NULL;
    }

    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        char type_sn_str[64] = {0};
        char new_ver_str[16] = {0};
        char old_ver_str[16] = {0};
        snprintf(type_sn_str, 64, "%s%013llu", type, sn);
        snprintf(new_ver_str, 16, "%lu", new_ver);
        snprintf(old_ver_str, 16, "%lu", old_ver);

        cJSON_AddStringToObject(cjson_data, "deviceSn", type_sn_str);
        cJSON_AddStringToObject(cjson_data, "recordId", recordId);
        cJSON_AddStringToObject(cjson_data, "remark", reason);
        cJSON_AddStringToObject(cjson_data, "upgradeStatus", status);
        cJSON_AddStringToObject(cjson_data, "upgradeVer", new_ver_str);
        cJSON_AddNumberToObject(cjson_data, "firmwareType", image_type);
        cJSON_AddStringToObject(cjson_data, "oldVer", old_ver_str);
        cJSON_AddStringToObject(cjson_data, "deviceModel", type);
        cJSON_AddNumberToObject(cjson_data, "connMode", 2); // 1:蓝牙 2:WIFI 3:RFID
        
        char *pdata = cJSON_PrintUnformatted(cjson_data);
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
}

/*
json_parse_record_id函数需对以下两种格式兼容

服务器返回格式：
{"appId":null,"msgId":"17c504a22e5f79115029cdbe2b4","msgCode":0,"message":"OK","data":"19f17fc9e5f00b8b12416539dad","error":null,"code":0}

Devassist返回格式（2026.07.01 龚工已修正为服务器返回格式）
{"deviceSn":"EL200mini2605000001678","firmwareType":0,"recordId":"71b40a5bf841719617f723db","success":true,"upgradeStatus":"downloading","upgradeVer":"100310199"}
*/
int json_parse_record_id(char *indata, char *recordId)
{
    ESP_LOGW(TAG,"json_parse_record_id: indata:[%s]\n", indata);

    cJSON *json = cJSON_Parse(indata);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return -1;
    }

    bool recordId_OK = false;
    cJSON *recordId_item = NULL;
    
    // 获取 "recordId" 字段（服务器格式）
    if ( !recordId_OK ) {
        recordId_item = cJSON_GetObjectItem(json, "data");
        if (recordId_item && cJSON_IsString(recordId_item)) {
            recordId_OK = true;
        }
    }

#if 0    
    // 获取 "recordId" 字段（Devassist格式）
    if ( !recordId_OK ) {
        recordId_item = cJSON_GetObjectItem(json, "recordId");
        if (recordId_item && cJSON_IsString(recordId_item)) {
            recordId_OK = true;
        }
    }
#endif
    
    if ( !recordId_OK ) {
        // 获取失败
        ESP_LOGE(TAG, "Failed to get 'recordId' field or 'recordId' is not a string");
        cJSON_Delete(json);
        return -1;
    }

    // 复制 "recordId" 字段的值
    strncpy(recordId, recordId_item->valuestring, 32);
    recordId[32 - 1] = '\0'; // 确保字符串以 null 结尾

    // 打印解析到的 "recordId" 字段值
    ESP_LOGI(TAG, "Parsed recordId: %s", recordId);

    // 释放 JSON 对象
    cJSON_Delete(json);

    return 0;
}

void json_buffer_clear(void *pdata)
{
    cJSON_free(pdata);
}

int json_ota_pct(const char recordId[32], 
                 char * dev_model_ascii, uint8_t end, uint8_t image_type, uint8_t where, 
                 uint8_t pct, uint8_t errcode, uint8_t *output, int max_len) {
/**
{
    "type": 3,
    "data": {
        "sn"： "EP6002020xxxxxx",
        "firmwareType": 1,  // 固件类型 0:物联网  1:ARM  2:DSP 3:BMS
        "step": 1,          // 1,2,3 进度类型1：IOT获取文件，2：IOT给设备发送文件，3：设备给子设备发送文件
        "percent": 0,       // 0 - 100
        "errcode": 0,       // 0-255
        "recordId": "18a180ef4faee2d856c48a4d2a0"
    }
} */
    
    cJSON* cjson_pct  = cJSON_CreateObject();
    cJSON* cjson_data  = cJSON_CreateObject();

    cJSON_AddNumberToObject(cjson_pct, "type", 3); 
    cJSON_AddStringToObject(cjson_data, "sn", dev_model_ascii);
    cJSON_AddNumberToObject(cjson_data, "firmwareType", image_type);
    cJSON_AddNumberToObject(cjson_data, "step", where);
    cJSON_AddNumberToObject(cjson_data, "percent", pct);
    cJSON_AddNumberToObject(cjson_data, "errcode", errcode);
    cJSON_AddStringToObject(cjson_data, "recordId", recordId);
    cJSON_AddItemToObject(cjson_pct, "data", cjson_data);

    char *pdata = cJSON_PrintUnformatted(cjson_pct);
    int len = strlen(pdata) % max_len;
    memcpy(output, pdata, len);
    ESP_LOGW(TAG,"%s", output);
    cJSON_free(pdata);
    cJSON_Delete(cjson_pct);
    return len;
}


char *json_get_money_save_cmd(const uint64_t device_sn, const char *device_type, uint16_t flag)
{
    /*
    {
        "flag": "2", // flag:0:当日, 1:当月, 2:当年, 5:近6年; 
        "remoteFlag": false, // 远程管理标识
        "deviceSn": "EBOX2414000009896" // 设备序列号
        "startDate": "2025-01-01", // 查询⽇期（yyyy-MM-dd）
        "installerFlag": false // 安装商标识
    }
    */

    cJSON *cjson_cmd = cJSON_CreateObject();

    // 添加设备SN
    char device_sn_type[64];
    snprintf(device_sn_type, sizeof(device_sn_type), "%s%013llu", device_type, device_sn);
    cJSON_AddStringToObject(cjson_cmd, "deviceSn", device_sn_type);

    // 添加日期
    char start_date[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(start_date, sizeof(start_date), "%Y-%m-%d", tm_info);
    cJSON_AddStringToObject(cjson_cmd, "startDate", start_date);

    // 添加标志和默认值
    char flag_str[8];
    snprintf(flag_str, sizeof(flag_str), "%d", flag);
    cJSON_AddStringToObject(cjson_cmd, "flag", flag_str);
    cJSON_AddBoolToObject(cjson_cmd, "remoteFlag", false);
    cJSON_AddBoolToObject(cjson_cmd, "installerFlag", false);

    // 生成JSON字符串
    char *pdata = cJSON_PrintUnformatted(cjson_cmd);
    if (pdata == NULL) {
        cJSON_Delete(cjson_cmd);
        return NULL; // Error handling
    }

    int len = strlen(pdata);
    ESP_LOGW(TAG, "%s, json len: %d", pdata, len);
    cJSON_Delete(cjson_cmd); // 删除整个数组，包括其中的对象
    return pdata;
}

int json_get_money_save_rsp(const char *message, uint64_t *savings_amount_total, uint8_t *temperature_unit) {
/*
{
    "appId": null,
    "msgId": "17c504a22e5f79115029cdbe2b4",
    "msgCode": 0,
    "message": "OK",
    "data": {
        "costSaveStatistics": {
            "savingsAmountTotal": "0.0"
        },
        "userConfig": {
            "temperatureUnit": "°F"
        }
    },
    "error": null,
    "code": 0
}
*/
    if (savings_amount_total == NULL || temperature_unit == NULL) {
        ESP_LOGE(TAG, "output parameters are NULL");
        return -1;
    }

    // 初始化输出参数
    *savings_amount_total = 0;
    *temperature_unit = 0; // 0 = invalid

    // 解析JSON字符串
    cJSON *root = cJSON_Parse(message);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return -1;
    }
    ESP_LOGW(TAG,"%s\n", message);

    // 获取data字段
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "data field is missing or not an object");
        cJSON_Delete(root);
        return -1;
    }

    // 解析 costSaveStatistics.savingsAmountTotal
    cJSON *cost_save_stats = cJSON_GetObjectItem(data, "costSaveStatistics");
    if (cJSON_IsObject(cost_save_stats)) {
        cJSON *savings_amount_json = cJSON_GetObjectItem(cost_save_stats, "savingsAmountTotal");
        if (cJSON_IsNumber(savings_amount_json)) {
            double val = savings_amount_json->valuedouble;
            *savings_amount_total = (val < 0) ? 0 : (uint64_t)val;
            // ESP_LOGI(TAG, "Savings amount total: %lu", *savings_amount_total);
        } else if (cJSON_IsString(savings_amount_json)) {
            uint64_t integer_part = 0U;
            uint32_t decimal_part = 0U;
            if (savings_amount_json->valuestring[0] == '-') {
                ESP_LOGE(TAG, "Negative value detected, set to 0: %s", savings_amount_json->valuestring);
            } else if (sscanf(savings_amount_json->valuestring, "%llu.%lu", &integer_part, &decimal_part) >= 1 ||
                       sscanf(savings_amount_json->valuestring, "%llu", &integer_part) == 1) {
                *savings_amount_total = integer_part;
                // ESP_LOGI(TAG, "Savings amount total: %lu", *savings_amount_total);
            } else {
                ESP_LOGE(TAG, "Failed to parse savingsAmountTotal: %s", savings_amount_json->valuestring);
            }
        } else {
            ESP_LOGE(TAG, "Failed to parse savingsAmountTotal");
        }
    }

    // 解析 userConfig.temperatureUnit
    cJSON *user_config = cJSON_GetObjectItem(data, "userConfig");
    if (cJSON_IsObject(user_config)) {
        cJSON *temperature_unit_json = cJSON_GetObjectItem(user_config, "temperatureUnit");
        if (cJSON_IsString(temperature_unit_json)) {
            // 转换温度单位字符串为数字: 0=invalid, 1=℃, 2=℉
            if ((strcmp(temperature_unit_json->valuestring, "°C") == 0) 
                || (strcmp(temperature_unit_json->valuestring, "℃") == 0)) {
                *temperature_unit = TEMP_UNIT_CELSIUS;
            } else if ((strcmp(temperature_unit_json->valuestring, "°F") == 0) 
                        || (strcmp(temperature_unit_json->valuestring, "℉") == 0)) {
                *temperature_unit = TEMP_UNIT_FAHRENHEIT;
            } else {
                *temperature_unit = TEMP_UNIT_INVALID; // invalid
            }
            // ESP_LOGI(TAG, "Temperature unit: %s (code: %d)", temperature_unit_json->valuestring, *temperature_unit);
        }
    }

    // 清理资源
    cJSON_Delete(root);
    
    return 0;
}


int json_parse_utc_ip(const char *msg, uint64_t *time) {
    /*示例  
        {
        "appId": null,
        "msgId": "17c504a22e5f79115029cdbe2b4",
        "msgCode": 0,
        "message": "OK",
        "data": "1701455925000",
        "error": null,
        "code": 0
        } */  
    if(msg == NULL) {
        ESP_LOGE(TAG, "json parse failed, msg is NULL");
        return -1;
    }
    cJSON *cjson_object = cJSON_Parse(msg);
    if (cjson_object == NULL) {
        ESP_LOGE(TAG, "json parse failed");
        return -1;
    }
   ESP_LOGW(TAG,"%s\n", msg);
    const cJSON *cjson_data = cJSON_GetObjectItem(cjson_object, "data");
    if (cjson_data == NULL || !cJSON_IsString(cjson_data)) {
        ESP_LOGE(TAG, "json parse failed, without data or data is not a string");
        cJSON_Delete(cjson_object);
        return -1;
    }

    char *endptr;
    uint64_t value = strtoll(cjson_data->valuestring, &endptr, 10);
    if (*endptr != '\0') {
        ESP_LOGE(TAG, "strtol failed or non-numeric characters found");
        cJSON_Delete(cjson_object);
        return -1;
    }

    *time = (value / 1000); // ms to s
    ESP_LOGI(TAG, "utc data: %llu", *time);
    cJSON_Delete(cjson_object);
    return 0;
}

#if CONFIG_DISASTER_WARN_CHANNEL == 1 

 char * json_disaster_rcv_response(const char *Model, const char *Sn_Str, int8_t type, uint16_t uuid, char *msg_id) {

     cJSON* cjson_data = cJSON_CreateObject();
     if(cjson_data){
         cJSON_AddStringToObject(cjson_data, "sn", Sn_Str);
         cJSON_AddStringToObject(cjson_data, "model", Model);
         if (type == 0) {
 			cJSON_AddStringToObject(cjson_data, "status", "success");
 		}
 		else {
 			cJSON_AddStringToObject(cjson_data, "status", "failure");
 		}
         cJSON_AddStringToObject(cjson_data, "recordId", msg_id);
         cJSON_AddStringToObject(cjson_data, "remark", "");
         cJSON_AddNumberToObject(cjson_data, "counter", uuid);
         char *pdata = cJSON_Print(cjson_data);
         ESP_LOGW(TAG, "%s", pdata);
         cJSON_Delete(cjson_data);
         return pdata;
     }
     return NULL;
 }

char * json_disaster_start(const char *Model, const char *Sn_Str, uint32_t StartTime,  uint16_t uuid, const char *msg_id) {
    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        cJSON_AddNumberToObject(cjson_data, "backupStartTime", StartTime);
        // cJSON_AddNumberToObject(cjson_data, "backupEndTime", EndTime);
        cJSON_AddStringToObject(cjson_data, "model", Model);
        cJSON_AddStringToObject(cjson_data, "sn", Sn_Str);
        cJSON_AddNumberToObject(cjson_data, "counter", uuid);
        cJSON_AddStringToObject(cjson_data, "recordId", msg_id);
        char *pdata = cJSON_Print(cjson_data);
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
}

char * json_disaster_response(const char *Model, const char *Sn_Str, uint32_t StartTime, uint32_t EndTime, uint32_t Energy, int8_t SOC, uint16_t uuid, const char *msg_id, uint16_t reason){

    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        cJSON_AddNumberToObject(cjson_data, "backupStartTime", StartTime);
        cJSON_AddNumberToObject(cjson_data, "backupEndTime", EndTime);
        cJSON_AddStringToObject(cjson_data, "model", Model);
        cJSON_AddStringToObject(cjson_data, "sn", Sn_Str);
        cJSON_AddNumberToObject(cjson_data, "energy", Energy);
        cJSON_AddNumberToObject(cjson_data, "soc", SOC);
        cJSON_AddNumberToObject(cjson_data, "counter", uuid);
        cJSON_AddStringToObject(cjson_data, "recordId", msg_id);
        cJSON_AddNumberToObject(cjson_data, "remark", reason);
        char *pdata = cJSON_Print(cjson_data);
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
}

esp_err_t parse_json_response(const char *response) {
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON *message = cJSON_GetObjectItem(json, "message");
    if (cJSON_IsString(message) && (message->valuestring != NULL)) {
        if (strcmp(message->valuestring, "OK") == 0) {
            ESP_LOGI(TAG, "Message is OK");
            cJSON_Delete(json);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Message is not OK: %s", message->valuestring);
        }
    } else {
        ESP_LOGE(TAG, "Message field is missing or not a string");
    }

    cJSON_Delete(json);
    return ESP_FAIL;
}

#elif CONFIG_DISASTER_WARN_CHANNEL == 2

char * json_disaster_response(const char *Sn_Str, uint32_t StartTime, uint32_t EndTime, uint32_t Energy, int8_t SOC, uint16_t uuid, const char *msg_id, char *dev_type){

    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        cJSON_AddNumberToObject(cjson_data, "backupStartTime", StartTime);
        cJSON_AddNumberToObject(cjson_data, "backupEndTime", EndTime);
        cJSON_AddStringToObject(cjson_data, "model", dev_type);
        cJSON_AddStringToObject(cjson_data, "sn", Sn_Str);
        cJSON_AddNumberToObject(cjson_data, "energy", Energy);
        cJSON_AddNumberToObject(cjson_data, "soc", SOC);
        cJSON_AddNumberToObject(cjson_data, "counter", uuid);
        cJSON_AddStringToObject(cjson_data, "recordId", msg_id);
        char *pdata = cJSON_PrintUnformatted(cjson_data);
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
}

/* 
{
    "backupStartTime": 1749523889,    // 备份开始时间（Unix时间戳）
    "model": "EP600",                 // 设备型号
    "sn": "EP6002326000001749",       // 设备序列号
    "counter": 12345,                 // 计数器值（uuid）
    "recordId": "18a180ef4faee2d856c48a4d2a0"  // 记录ID
} */
char * json_disaster_start(const char *Sn_Str, uint32_t StartTime,  uint16_t uuid, const char *msg_id, char *dev_type) {
    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        cJSON_AddNumberToObject(cjson_data, "backupStartTime", StartTime);
        // cJSON_AddNumberToObject(cjson_data, "backupEndTime", EndTime);
        cJSON_AddStringToObject(cjson_data, "model", dev_type);
        cJSON_AddStringToObject(cjson_data, "sn", Sn_Str);
        cJSON_AddNumberToObject(cjson_data, "counter", uuid);
        cJSON_AddStringToObject(cjson_data, "recordId", msg_id);
        char *pdata = cJSON_PrintUnformatted(cjson_data);
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
}

esp_err_t json_is_rsp_message_ok(const char *response) {
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON *message = cJSON_GetObjectItem(json, "message");
    if (cJSON_IsString(message) && (message->valuestring != NULL)) {
        if (strcmp(message->valuestring, "OK") == 0) {
            ESP_LOGI(TAG, "Message is OK");
            cJSON_Delete(json);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Message is not OK: %s", message->valuestring);
        }
    } else {
        ESP_LOGE(TAG, "Message field is missing or not a string");
    }

    cJSON_Delete(json);
    return ESP_FAIL;
}

#endif

/*
 * 雷暴预警JSON格式示例:
{
    "date": "2025-06-10 10:51:29",
    "start": 1749523889,
    "end": 1749527889,
    "urgency": 0,
    "severity": 1,
    "certainty": 2,
    "sender": "US Environmental Protection Agency",
    "event": 3,
    "headline": "Thunderstorm Alert for Natomas",
    "recordId": "18a180ef4faee2d856c48a4d2a0",
    "timezone": "Europe/Vilnius",
    "utc_offset_seconds": -18000,
    "soc": 100
}
*/
int parse_thunderstorm_alert(const char* json_str, _SaveWebStormWatchApiData *out_data) {
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return -1;
    }

    if (out_data == NULL) {
        ESP_LOGE(TAG, "Invalid input parameters");
        cJSON_Delete(root);
        return -1;
    }

    cJSON *start = cJSON_GetObjectItem(root, "start");
    cJSON *end = cJSON_GetObjectItem(root, "end");
    cJSON *urgency = cJSON_GetObjectItem(root, "urgency");
    cJSON *severity = cJSON_GetObjectItem(root, "severity");
    cJSON *certainty = cJSON_GetObjectItem(root, "certainty");
    cJSON *event = cJSON_GetObjectItem(root, "event");
    cJSON *soc = cJSON_GetObjectItem(root, "soc");
    cJSON *utc_offset_seconds = cJSON_GetObjectItem(root, "utc_offset_seconds");
    cJSON *recordId = cJSON_GetObjectItem(root, "recordId");

    if (cJSON_IsNumber(start)) out_data->StartTime = start->valueint;
    if (cJSON_IsNumber(end)) out_data->EndTime = end->valueint;
    if (cJSON_IsNumber(urgency)) out_data->Urgency = urgency->valueint;
    if (cJSON_IsNumber(severity)) out_data->Severity = severity->valueint;
    if (cJSON_IsNumber(certainty)) out_data->Certainty = certainty->valueint;
    if (cJSON_IsNumber(event)) out_data->Event = event->valueint;
    if (cJSON_IsNumber(soc)) out_data->SoC = soc->valueint;
    if (cJSON_IsNumber(utc_offset_seconds)) out_data->TimeZoneOffset = utc_offset_seconds->valueint;
    if (cJSON_IsString(recordId)) strncpy(out_data->msg_id, recordId->valuestring, 32);
    cJSON_Delete(root);
    return 0;
}


/**
 * @brief 解析连接令牌(Connection Token)的JSON字符串。
 * @param message 包含JSON数据的字符串，例如：'{"state":0, "waitSec":30}'。
 * @param conn_token_info 指向 connection_token_t 结构体的指针，用于存储解析结果。
 * @return 成功时返回0，失败时返回-1。
 */
int json_get_conn_token_info(const char *message, connection_token_t *conn_token_info) {
    // 1. 检查输入参数是否有效
    if (message == NULL || conn_token_info == NULL) {
        ESP_LOGE(TAG, "Error: Input message or conn_token_info pointer is NULL.");
        return -1;
    }

    // 2. 初始化输出结构体
    memset(conn_token_info, 0, sizeof(connection_token_t));

    // 3. 解析JSON字符串
    cJSON *root = cJSON_Parse(message);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Failed to parse JSON at: %s", error_ptr);
        } else {
            ESP_LOGE(TAG, "Failed to parse JSON, unknown error.");
        }
        return -1;
    }

    int ret = -1; // 默认返回失败

    // 4. 获取 "state" 字段
    cJSON *state_item = cJSON_GetObjectItem(root, "state");
    if (!cJSON_IsNumber(state_item)) {
        ESP_LOGE(TAG, "JSON field 'state' is missing or not a number.");
        goto cleanup; // 跳转到清理步骤
    }

    // 5. 获取 "waitSec" 字段
    cJSON *wait_sec_item = cJSON_GetObjectItem(root, "waitSec");
    if (!cJSON_IsNumber(wait_sec_item)) {
        ESP_LOGE(TAG, "JSON field 'waitSec' is missing or not a number.");
        goto cleanup; // 跳转到清理步骤
    }

    // 6. 填充结构体
    conn_token_info->state = (uint8_t)state_item->valueint;
    conn_token_info->waitSec = (int32_t)wait_sec_item->valueint;

    ESP_LOGI(TAG, "Parsed Connection Token -> state: %u, waitSec: %d", 
             conn_token_info->state, conn_token_info->waitSec);

    ret = 0; // 所有字段解析成功，设置返回值为成功

cleanup:
    // 7. 释放cJSON对象占用的内存
    cJSON_Delete(root);
    return ret;
}

#ifdef CONFIG_GRID_OFF_COUNT_ENABLE 

/**
 * @brief 生成断电记录上报JSON指令
 * @param start_time 断电开始时间（Unix时间戳）
 * @param end_time 断电结束时间（Unix时间戳）
 * @param device_model 设备型号
 * @param device_sn 设备序列号
 * @param remark 备注信息
 * @return 返回JSON字符串指针，需要调用cJSON_free释放内存，失败返回NULL
 */
char *json_power_outage_report(uint32_t start_time, uint32_t end_time, 
                               const char *device_model, uint64_t device_sn, 
                               const char *remark) {
    if (!device_model || !remark) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return NULL;
    }

    cJSON *cjson_data = cJSON_CreateObject();
    if (!cjson_data) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return NULL;
    }

    // 生成设备SN字符串 (型号+序列号)
    char device_sn_str[64] = {0};
    snprintf(device_sn_str, sizeof(device_sn_str), "%s%llu", device_model, device_sn);

    // 添加JSON字段
    cJSON_AddNumberToObject(cjson_data, "startTime", start_time);
    cJSON_AddNumberToObject(cjson_data, "endTime", end_time);
    cJSON_AddStringToObject(cjson_data, "model", device_model);
    cJSON_AddStringToObject(cjson_data, "sn", device_sn_str);
    cJSON_AddStringToObject(cjson_data, "remark", remark);

    // 生成不带格式的JSON字符串
    char *pdata = cJSON_PrintUnformatted(cjson_data);
    if (!pdata) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(cjson_data);
        return NULL;
    }

    int len = strlen(pdata);
    ESP_LOGW(TAG, "Power outage report JSON: %s, len: %d", pdata, len);
    
    cJSON_Delete(cjson_data);
    return pdata;
}

/**
 * @brief 生成批量断电事件上报的JSON请求数据
 * @param device_model 设备型号
 * @param device_sn 设备序列号
 * @param records 断电事件记录数组
 * @param record_count 记录数量
 * @return JSON字符串指针，需要调用json_buffer_clear释放
 *
 * 生成的数据格式示例:
 * {
 *   "model": "FP",
 *   "recordList": [
 *     {
 *       "endTime": 1761546710,
 *       "remark": "1",
 *       "startTime": 1761546610
 *     },
 *     {
 *       "endTime": 1761546730,
 *       "remark": "3",
 *       "startTime": 1761546630
 *     },
 *     {
 *       "endTime": 1761546710,
 *       "remark": "2",
 *       "startTime": 1761546610
 *     }
 *   ],
 *   "sn": "FP2534000032400"
 * }
 */
char *json_batch_power_outage_report(const char *device_model, uint64_t device_sn,
                                     const power_off_record_t *records, int record_count)
{
    if (!device_model || !records || record_count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for batch power outage report");
        return NULL;
    }
    
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return NULL;
    }
    
    // 添加设备型号
    cJSON_AddStringToObject(json, "model", device_model);
    
    // 添加设备序列号
    char sn_str[64] = {0};
    snprintf(sn_str, sizeof(sn_str), "%s%llu", device_model, device_sn);
    cJSON_AddStringToObject(json, "sn", sn_str);
    
    // 创建记录列表
    cJSON *record_list = cJSON_CreateArray();
    if (record_list == NULL) {
        ESP_LOGE(TAG, "Failed to create record list JSON");
        cJSON_Delete(json);
        return NULL;
    }
    
    // 添加每条断电记录
    for (int i = 0; i < record_count; i++) {
        const power_off_record_t *record = &records[i];
        
        // 跳过无效记录
        if (record->start_time == 0 || record->end_time == 0 || 
            record->end_time <= record->start_time) {
            ESP_LOGW(TAG, "Skipping invalid record %d: start=%lu, end=%lu", 
                     i, record->start_time, record->end_time);
            continue;
        }
        
        cJSON *record_obj = cJSON_CreateObject();
        if (record_obj == NULL) {
            ESP_LOGE(TAG, "Failed to create record object %d", i);
            continue;
        }
        
        // 添加开始时间
        cJSON_AddNumberToObject(record_obj, "startTime", record->start_time);
        
        // 添加结束时间
        cJSON_AddNumberToObject(record_obj, "endTime", record->end_time);
        
        // 添加备注（使用remark字段）
        char remark_str[16] = {0};
        snprintf(remark_str, sizeof(remark_str), "%u", record->remark);
        cJSON_AddStringToObject(record_obj, "remark", remark_str);
        
        // 将记录添加到数组
        cJSON_AddItemToArray(record_list, record_obj);
    }
    
    // 将记录列表添加到主JSON对象
    cJSON_AddItemToObject(json, "recordList", record_list);
    
    // 生成JSON字符串
    char *json_string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON string");
        return NULL;
    }
    
    int len = strlen(json_string);
    ESP_LOGW(TAG, "Batch power outage report JSON: %s, len: %d", json_string, len);
    
    return json_string;
}

/**
 * @brief 解析批量断电事件上报的响应数据
 * @param response 服务器响应的JSON字符串
 * @return 0-成功(data=true), -1-失败或data=false
 *
 * 响应JSON格式示例:
 * {
 *   "appId": null,
 *   "msgId": "17c504a22e5f79115029cdbe2b4",
 *   "msgCode": 0,
 *   "message": "OK",
 *   "data": true,
 *   "error": null,
 *   "code": 0
 * }
 */
int json_parse_batch_power_outage_response(const char *response)
{
    if (!response) {
        ESP_LOGE(TAG, "Response is NULL");
        return -1;
    }
    
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return -1;
    }
    
    int ret = -1;
    
    // 检查msgCode字段
    cJSON *msg_code = cJSON_GetObjectItem(json, "msgCode");
    if (cJSON_IsNumber(msg_code)) {
        int code = (int)cJSON_GetNumberValue(msg_code);
        if (code != 0) {
            ESP_LOGE(TAG, "Server returned error msgCode: %d", code);
            goto cleanup;
        }
    }
    
    // 检查code字段
    cJSON *code = cJSON_GetObjectItem(json, "code");
    if (cJSON_IsNumber(code)) {
        int code_value = (int)cJSON_GetNumberValue(code);
        if (code_value != 0) {
            ESP_LOGE(TAG, "Server returned error code: %d", code_value);
            goto cleanup;
        }
    }
    
    // 检查data字段
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (cJSON_IsBool(data)) {
        if (cJSON_IsTrue(data)) {
            ESP_LOGI(TAG, "Batch power outage report successful");
            ret = 0;
        } else {
            ESP_LOGE(TAG, "Server returned data=false");
        }
    } else {
        ESP_LOGE(TAG, "Invalid or missing data field in response");
    }
    
    // 记录message信息（如果有）
    cJSON *message = cJSON_GetObjectItem(json, "message");
    if (cJSON_IsString(message)) {
        ESP_LOGI(TAG, "Server message: %s", cJSON_GetStringValue(message));
    }
    
cleanup:
    cJSON_Delete(json);
    return ret;
}


/**
 * @brief 解析断电统计数据的响应
 * @param response 服务器响应的JSON字符串
 * @param stats_out 输出的统计数据结构
 * @return 0-成功, -1-失败
 */
int json_parse_grid_off_stats_response(const char *response, grid_off_count_statistics_t *stats_out)
{
    if (!response || !stats_out) {
        ESP_LOGE(TAG, "Invalid parameters for blackout stats response parsing");
        return -1;
    }
    
    // 初始化输出结构
    memset(stats_out, 0, sizeof(grid_off_count_statistics_t));
    
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse blackout stats JSON response");
        return -1;
    }
    
    int ret = -1;
    
    // 检查msgCode字段
    cJSON *msg_code = cJSON_GetObjectItem(json, "msgCode");
    if (cJSON_IsNumber(msg_code)) {
        int code = (int)cJSON_GetNumberValue(msg_code);
        if (code != 0) {
            ESP_LOGE(TAG, "Server returned error msgCode: %d", code);
            goto cleanup;
        }
    }
    
    // 检查code字段
    cJSON *code = cJSON_GetObjectItem(json, "code");
    if (cJSON_IsNumber(code)) {
        int code_value = (int)cJSON_GetNumberValue(code);
        if (code_value != 0) {
            ESP_LOGE(TAG, "Server returned error code: %d", code_value);
            goto cleanup;
        }
    }
    
    // 解析data字段
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (cJSON_IsObject(data)) {
        // 解析各个统计字段
        cJSON *day = cJSON_GetObjectItem(data, "day");
        if (cJSON_IsNumber(day)) {
            stats_out->daily_count = (uint32_t)cJSON_GetNumberValue(day);
        }
        
        cJSON *month = cJSON_GetObjectItem(data, "month");
        if (cJSON_IsNumber(month)) {
            stats_out->monthly_count = (uint32_t)cJSON_GetNumberValue(month);
        }

        cJSON *week = cJSON_GetObjectItem(data, "week");
        if (cJSON_IsNumber(week)) {
            stats_out->weekly_count = (uint32_t)cJSON_GetNumberValue(week);
        }
        
        cJSON *total = cJSON_GetObjectItem(data, "total");
        if (cJSON_IsNumber(total)) {
            stats_out->total_count = (uint32_t)cJSON_GetNumberValue(total);
        }
        
        cJSON *timestamp = cJSON_GetObjectItem(data, "timestamp");
        if (cJSON_IsString(timestamp)) {
            const char *ts_str = cJSON_GetStringValue(timestamp);
            stats_out->last_update_time = (uint64_t)strtoull(ts_str, NULL, 10);
        }
        
        cJSON *year = cJSON_GetObjectItem(data, "year");
        if (cJSON_IsNumber(year)) {
            stats_out->yearly_count = (uint32_t)cJSON_GetNumberValue(year);
        }
        
        // ESP_LOGI(TAG, "Blackout stats parsed: daily=%lu, weekly=%lu, monthly=%lu, yearly=%lu, total=%lu, timestamp=%llu",
        //          stats_out->daily_count, stats_out->weekly_count, stats_out->monthly_count, stats_out->yearly_count, 
        //          stats_out->total_count, stats_out->last_update_time);
        ret = 0;
    } else {
        ESP_LOGE(TAG, "Invalid or missing data field in blackout stats response");
    }
    
    // 记录message信息（如果有）
    cJSON *message = cJSON_GetObjectItem(json, "message");
    if (cJSON_IsString(message)) {
        // ESP_LOGI(TAG, "Server message: %s", cJSON_GetStringValue(message));
    }
    
cleanup:
    cJSON_Delete(json);
    return ret;
}

#endif

