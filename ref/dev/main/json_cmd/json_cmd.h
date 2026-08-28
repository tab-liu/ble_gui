#ifndef JSON_CMD_H
#define JSON_CMD_H

#define    MAX_VER_NUM 2

enum {
    OTA_METHOD_APP,
    OTA_METHOD_CLOUD,
    OTA_METHOD_WEBSERVER
};

typedef struct {
    uint8_t type;
    uint32_t version;
}ver_data_t;

typedef struct {
    char dev_type[12];
    uint64_t dev_sn;
    uint8_t ver_num;
    ver_data_t ver_list[MAX_VER_NUM];
}new_image_req_t;

typedef struct {
    bool encrypted;
    uint8_t force;
    uint8_t type;
    uint32_t total_bytes;
    uint32_t version;
    char model[12];
    char dev_model_ascii[32]; // type 12 + id 64bit number;
    char md5_code[32];
    char recordId[32];
    char url[256];
    uint8_t method;
}ota_object_t;

typedef struct cmd_list{
    struct cmd_list *next;
    ota_object_t object;
}ota_cmd_list_t;

void json_buffer_clear(void *pdata);
int json_ota_request(uint8_t image_num, char *type, uint64_t sn, new_image_req_t image_req[], uint8_t *output, int max_len);
char * json_ota_response(const char recordId[32], const char *status, const char *reason);
int json_ota_pct(const char recordId[32], 
                 char * dev_model_ascii, uint8_t end, uint8_t image_type, uint8_t where, 
                 uint8_t pct, uint8_t errcode, uint8_t *output, int max_len);
ota_cmd_list_t *json_ota_cmd_parse(const char *message);
ota_cmd_list_t *json_parse_ota_rsp(const char *message);
char * json_ota_response_first(const char recordId[32], const char *status, const char *reason,
                            uint64_t sn, char *type, uint8_t image_type, uint32_t new_ver, uint32_t old_ver);
int *json_parse_record_id(char *indata, char *recordId);
int json_parse_utc_ip(const char *msg, uint64_t *time);
// char * json_disaster_response(const char *Sn_Str, uint32_t StartTime, uint32_t EndTime, uint32_t Energy, int8_t SOC, const char *uuid);
esp_err_t parse_json_response(const char *response);
// char * json_disaster_rcv_response(const char *Sn_Str, int8_t type, uint16_t uuid, char *id);
char * json_disaster_start(const char *Sn_Str, uint32_t StartTime, uint16_t uuid,char* dev_type, const char *msg_id);
char * json_disaster_response(const char *Sn_Str, uint32_t StartTime, uint32_t EndTime, uint32_t Energy, int8_t SOC, uint16_t uuid,char*type, const char *msg_id, uint16_t reason);
char * json_ota_http_request(char *product_type,char *type, uint64_t sn, ver_data_t ver_list[], uint8_t ver_num);

char *json_get_money_save_cmd(const uint64_t device_sn, const char *device_type, uint16_t flag);
int json_get_money_save_rsp(const char *message, uint64_t *savings_amount_total, uint8_t *temperature_unit);

#endif
