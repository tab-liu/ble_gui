
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
#include "comm_define.h"

#include <stdio.h>

#define TAG "[JSONs]"

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

static uint32_t json_cjson_to_u32(const cJSON *item)
{
    if (item == NULL) {
        return 0;
    }
    if (cJSON_IsNumber(item)) {
        return (uint32_t)item->valueint;
    }
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return (uint32_t)strtoul(item->valuestring, NULL, 0);
    }
    return 0;
}

static int json_cjson_to_int(const cJSON *item, int default_val)
{
    if (item == NULL) {
        return default_val;
    }
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? 1 : 0;
    }
    return default_val;
}

static bool json_cjson_string_ok(const cJSON *item)
{
    return cJSON_IsString(item) && item->valuestring != NULL;
}

static bool json_cjson_u32_ok(const cJSON *item)
{
    if (item == NULL) {
        return false;
    }
    if (cJSON_IsNumber(item)) {
        return true;
    }
    return json_cjson_string_ok(item);
}

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

        if (!json_cjson_string_ok(cjson_device_sn) || !json_cjson_string_ok(cjson_device_model)) {
            ESP_LOGE(TAG, "json parse failed, without sn or model");
            continue;
        }

        ESP_LOGI(TAG, "ota cmd: device: %s, model: %s, focre: %d",  cjson_device_sn->valuestring, 
                                                                    cjson_device_model->valuestring,
                                                                    json_cjson_to_int(cjson_ota_force, 0));
                                                                    
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

                if (cjson_file_type == NULL || !json_cjson_u32_ok(cjson_file_size)
                    || !json_cjson_string_ok(cjson_file_ver) || !json_cjson_string_ok(cjson_file_url)
                    || !json_cjson_string_ok(cjson_file_md5) || !json_cjson_string_ok(cjson_recordId)) {
                    ESP_LOGE(TAG, "json parse failed, without expected item");
                    continue;
                }
                
                ota_cmd_list_t *curr_cmd = (ota_cmd_list_t *)heap_caps_malloc(sizeof(ota_cmd_list_t), MALLOC_CAP_SPIRAM);
                if (!curr_cmd) {
                    continue;
                }

                memset(curr_cmd, 0, sizeof(ota_cmd_list_t));
                curr_cmd->next = NULL;
                curr_cmd->object.force = json_cjson_to_int(cjson_ota_force, 0);
                curr_cmd->object.type = json_cjson_to_int(cjson_file_type, 0);
                curr_cmd->object.version = json_cjson_to_u32(cjson_file_ver);
                curr_cmd->object.total_bytes = json_cjson_to_u32(cjson_file_size);
                ESP_LOGI(TAG, "file: type: %d, size: %lu, version: %lu",  curr_cmd->object.type, 
                                                                        curr_cmd->object.total_bytes,
                                                                        curr_cmd->object.version);  
                memcpy(curr_cmd->object.model, cjson_device_model->valuestring, strlen(cjson_device_model->valuestring) % 12); //  
                memcpy(curr_cmd->object.dev_model_ascii, cjson_device_sn->valuestring, strlen(cjson_device_sn->valuestring) % 32); //             
                memcpy(curr_cmd->object.md5_code,    cjson_file_md5->valuestring,  strlen(cjson_file_md5->valuestring) % 32); // md5
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
    snprintf(str, 128, "%s.%llu", type, sn);
    cJSON* cjson_req = cJSON_CreateObject();
    cJSON_AddNumberToObject(cjson_req, "type", 2); 
    cJSON_AddStringToObject(cjson_req, "topic", str);

    cJSON* cjson_data_array = cJSON_CreateArray();
    for (uint8_t i = 0; i < image_num; i++) {
        cJSON* cjson_data = cJSON_CreateObject();
        char _str[128] = {0};
        snprintf(_str, 128, "%s%llu", image_req[i].dev_type, image_req[i].dev_sn); // vasprintf("%s%d", image_req.dev_type, image_req.dev_sn);
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
    if (pdata == NULL) {
        cJSON_Delete(cjson_req);
        return 0;
    }
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
        if (pdata == NULL) {
            cJSON_Delete(cjson_data);
            return NULL;
        }
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
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
    if (pdata == NULL) {
        cJSON_Delete(cjson_pct);
        return 0;
    }
    int len = strlen(pdata) % max_len;
    memcpy(output, pdata, len);
    ESP_LOGW(TAG,"%s", output);
    cJSON_free(pdata);
    cJSON_Delete(cjson_pct);
    return len;
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

    // cJSON* cjson_message = cJSON_GetObjectItem(cjson_object, "message");
    // if (cJSON_IsString(cjson_message) && cjson_message->valuestring != NULL
    //     && strcmp(cjson_message->valuestring, "OK") != 0) {
    //     ESP_LOGE(TAG, "ota rsp message not OK: %s", cjson_message->valuestring);
    //     cJSON_Delete(cjson_object);
    //     return NULL;
    // }
          
    cJSON* cjson_data = cJSON_GetObjectItem(cjson_object, "data");
    if(cjson_data == NULL || !cJSON_IsArray(cjson_data)) {
        ESP_LOGE(TAG, "json parse failed, without data");
        cJSON_Delete(cjson_object);
        return NULL;
    }
    cJSON* cjson_details = cJSON_GetArrayItem(cjson_data, 0);
    if (cjson_details) {
        cJSON* cjson_device_sn     = cJSON_GetObjectItem(cjson_details, DEV_SN_CODE); // dev model + sn
        cJSON* cjson_device_model  = cJSON_GetObjectItem(cjson_details, DEV_MODEL);   //
        if(!json_cjson_string_ok(cjson_device_sn) || !json_cjson_string_ok(cjson_device_model)) {
            ESP_LOGE(TAG, "json parse failed, without sn or model");
            cJSON_Delete(cjson_object);
            return NULL;
        }
        ESP_LOGI(TAG, "ota cmd: device: %s, model: %s",  cjson_device_sn->valuestring, 
                                                        cjson_device_model->valuestring);
        cJSON* cjson_versions = cJSON_GetObjectItem(cjson_details, "versions");
        if(cjson_versions && cJSON_IsArray(cjson_versions)){
            int image_num = cJSON_GetArraySize(cjson_versions);
            for (int j = 0; j < image_num; j++) {
                cJSON* array_item = cJSON_GetArrayItem(cjson_versions, j);
                if (!array_item) {
                    continue;
                }
                
                const cJSON* cjson_file_type = cJSON_GetObjectItem(array_item, FILE_TYPE);
                const cJSON* cjson_file_size = cJSON_GetObjectItem(array_item, FILE_SIZE);
                const cJSON* cjson_file_ver  = cJSON_GetObjectItem(array_item, FILE_VER);
                if (cjson_file_type == NULL || !json_cjson_u32_ok(cjson_file_size)
                    || !json_cjson_u32_ok(cjson_file_ver)) {
                    ESP_LOGE(TAG, "json parse failed, without expected item");
                    continue;
                }

                ota_cmd_list_t  *curr_cmd = (ota_cmd_list_t *)heap_caps_malloc(sizeof(ota_cmd_list_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);                                     
                if (!curr_cmd) {
                    ESP_LOGE(TAG, "json parse failed, without memory");
                    continue;
                }         

                memset(curr_cmd, 0, sizeof(ota_cmd_list_t));
                curr_cmd->next = NULL;
                curr_cmd->object.force = 0;
                curr_cmd->object.type = json_cjson_to_int(cjson_file_type, 0);
                curr_cmd->object.version = json_cjson_to_u32(cjson_file_ver);
                curr_cmd->object.total_bytes = json_cjson_to_u32(cjson_file_size);
                ESP_LOGI(TAG, "file: type: %d, size: %lu, version: %lu",  curr_cmd->object.type, 
                                                                        curr_cmd->object.total_bytes,
                                                                        curr_cmd->object.version);         
               
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
    ESP_LOGI(TAG, "data: %llu", *time);
    cJSON_Delete(cjson_object);
    return 0;
}
char * json_disaster_response(const char *Sn_Str, uint32_t StartTime, uint32_t EndTime, uint32_t Energy, int8_t SOC, uint16_t uuid,char*type, const char *msg_id, uint16_t reason){
    if(msg_id==NULL)
    {
        ESP_LOGE(TAG,"msg_id NULL ERR");
        return NULL;
    }

    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        // cJSON_AddNumberToObject(cjson_data, "backupStartTime", StartTime);
        // cJSON_AddNumberToObject(cjson_data, "backupEndTime", EndTime);
        // cJSON_AddStringToObject(cjson_data, "model", type);
        // cJSON_AddStringToObject(cjson_data, "sn", Sn_Str);
        // cJSON_AddNumberToObject(cjson_data, "energy", Energy);
        // cJSON_AddNumberToObject(cjson_data, "soc", SOC);
        // cJSON_AddNumberToObject(cjson_data, "counter", uuid);
        // cJSON_AddStringToObject(cjson_data, "recordId", msg_id);

        if (!cJSON_AddNumberToObject(cjson_data, "backupStartTime", StartTime)) goto error;
        if (!cJSON_AddNumberToObject(cjson_data, "backupEndTime", EndTime)) goto error;
        if (!cJSON_AddStringToObject(cjson_data, "model", type)) goto error;
        if (!cJSON_AddStringToObject(cjson_data, "sn", Sn_Str)) goto error;
        if (!cJSON_AddNumberToObject(cjson_data, "energy", Energy)) goto error;
        if (!cJSON_AddNumberToObject(cjson_data, "soc", SOC)) goto error;
        if (!cJSON_AddNumberToObject(cjson_data, "counter", uuid)) goto error;
        if (!cJSON_AddStringToObject(cjson_data, "recordId", msg_id)) goto error;
        if (!cJSON_AddNumberToObject(cjson_data, "remark", reason)) goto error;
        char *pdata = cJSON_Print(cjson_data);
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    error:
    ESP_LOGE(TAG, "Error creating JSON response");
    cJSON_Delete(cjson_data);
    return NULL;
}

// char * json_disaster_rcv_response(const char *Sn_Str, int8_t type, uint16_t uuid, char *msg_id, ) {

//     cJSON* cjson_data = cJSON_CreateObject();
//     if(cjson_data){
//         cJSON_AddStringToObject(cjson_data, "sn", Sn_Str);
//         cJSON_AddStringToObject(cjson_data, "model", "EBOX");
//         if (type == 0) {
// 			cJSON_AddStringToObject(cjson_data, "status", "success");
// 		}
// 		else {
// 			cJSON_AddStringToObject(cjson_data, "status", "failure");
// 		}
//         cJSON_AddStringToObject(cjson_data, "recordId", msg_id);
//         cJSON_AddStringToObject(cjson_data, "remark", "");
//         cJSON_AddNumberToObject(cjson_data, "counter", uuid);
//         char *pdata = cJSON_Print(cjson_data);
//         ESP_LOGW(TAG, "%s", pdata);
//         cJSON_Delete(cjson_data);
//         return pdata;
//     }
//     return NULL;
// }

char * json_disaster_start(const char *Sn_Str, uint32_t StartTime,  uint16_t uuid,char* dev_type, const char *msg_id) {
    cJSON* cjson_data = cJSON_CreateObject();
    if(cjson_data){
        cJSON_AddNumberToObject(cjson_data, "backupStartTime", StartTime);
        // cJSON_AddNumberToObject(cjson_data, "backupEndTime", EndTime);
        cJSON_AddStringToObject(cjson_data, "model", dev_type);
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

char *json_ota_http_request(char *product_type,char *type, uint64_t sn, ver_data_t ver_list[], uint8_t ver_num) 
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
    //#define  TYPE_SIZE      12
    char str[128] = {0};
    snprintf(str, sizeof(str), "%.*s%llu", (int)strnlen(type, TYPE_SIZE), type, sn);
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
    cJSON_AddStringToObject(cjson_req, "model", product_type);
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
    ESP_LOGW(TAG, "%s", pdata);

    int len = strlen(pdata);
    ESP_LOGW(TAG, "%s, json len: %d", pdata, len);
    cJSON_Delete(cjson_req_array); // 删除整个数组，包括其中的对象
    return pdata;
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
        snprintf(type_sn_str, 64, "%s%" PRIu64, type, sn);
        snprintf(new_ver_str, 16, "%" PRIu32, new_ver);
        snprintf(old_ver_str, 16, "%" PRIu32, old_ver);

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
        if (pdata == NULL) {
            cJSON_Delete(cjson_data);
            return NULL;
        }
        ESP_LOGW(TAG, "%s", pdata);
        cJSON_Delete(cjson_data);
        return pdata;
    }
    return NULL;
}

int *json_parse_record_id(char *indata, char *recordId)
{
    ESP_LOGW(TAG,"json_parse_record_id: indata:[%s]\n", indata);

    cJSON *json = cJSON_Parse(indata);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return -1;
    }

    // 获取 "recordId" 字段
    cJSON *recordId_item = cJSON_GetObjectItem(json, "data");
    if (!json_cjson_string_ok(recordId_item)) {
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
    snprintf(device_sn_type, sizeof(device_sn_type), "%s%llu", device_type, device_sn);
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
            "savingsAmountTotal": "0"
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
        if (cJSON_IsString(savings_amount_json)) {
            uint64_t integer_part = 0U;
            uint32_t decimal_part = 0U;

            if (savings_amount_json->valuestring[0] == '-') {
                ESP_LOGE(TAG, "Negative value detected, set to 0: %s", savings_amount_json->valuestring);
            }else if (sscanf(savings_amount_json->valuestring, "%" SCNu64, &integer_part) == 1 ||
                sscanf(savings_amount_json->valuestring, "%" SCNu64 ".%" SCNu32, &integer_part, &decimal_part) >= 1) {
                // 解析成功，使用整数部分
            }else {
                ESP_LOGE(TAG, "Failed to parse savingsAmountTotal: %s", savings_amount_json->valuestring);
            }
            //*savings_amount_total = (uint64_t)strtoul(savings_amount_json->valuestring, NULL, 10);
            *savings_amount_total = integer_part;
            // ESP_LOGI(TAG, "Savings amount total: %lu", *savings_amount_total);
        }
        else
        {
            ESP_LOGI(TAG, "savingsAmountTotal type error: type:%d", (savings_amount_json->type & 0xFF));
        }
    }

    // 解析 userConfig.temperatureUnit
    cJSON *user_config = cJSON_GetObjectItem(data, "userConfig");
    if (cJSON_IsObject(user_config)) {
        cJSON *temperature_unit_json = cJSON_GetObjectItem(user_config, "temperatureUnit");
        if (cJSON_IsString(temperature_unit_json)) {
            // 转换温度单位字符串为数字: 0=invalid, 1=℃, 2=℉
            if (strcmp(temperature_unit_json->valuestring, "°C") == 0) {
                *temperature_unit = 1;
            } else if (strcmp(temperature_unit_json->valuestring, "°F") == 0) {
                *temperature_unit = 2;
            } else {
                *temperature_unit = 0; // invalid
            }
            // ESP_LOGI(TAG, "Temperature unit: %s (code: %d)", temperature_unit_json->valuestring, *temperature_unit);
        }
    }

    // 清理资源
    cJSON_Delete(root);
    
    return 0;
}
