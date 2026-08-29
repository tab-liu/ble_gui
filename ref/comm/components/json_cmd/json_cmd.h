#ifndef JSON_CMD_H
#define JSON_CMD_H

#include "stdint.h"
#include "parameter.h"
#include "comm_define.h"

#define    MAX_VER_NUM 2

typedef enum {
    TEMP_UNIT_INVALID = 0,
    TEMP_UNIT_CELSIUS = 1,
    TEMP_UNIT_FAHRENHEIT = 2
} temperature_unit_e;

enum {
    OTA_METHOD_UNKNOWN,
    OTA_METHOD_APP,
    OTA_METHOD_CLOUD,
    OTA_METHOD_WEBSERVER,
    OTA_METHOD_DEVASSIST
};

#pragma pack(1)

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

/**
 * @brief TCP连接令牌结构体，用于解析从服务器获取的连接策略。
 * @note  该结构体定义与服务器下发的 x-conn-token 解码后的JSON对象对应。
 */
typedef struct {
    /**
     * @brief TCP服务器可接受客户端连接的状态。
     * @details
     *   - 0: 服务器现在可接收客户端的连接，客户端可立即连接到服务器。
     *   - 1: 服务器拒绝客户端连接，此时值需要根据 `waitSec` 值做出下一步逻辑处理。
     */
    uint8_t state;

    /**
     * @brief 客户端等待下一次连接的秒数。
     * @details
     *   - -1: 一直永远不要发起服务器的TCP连接 (客户端系统下次上电重启时，可以重新向服务器获取最新的等待秒数)。
     *   - 0:  由客户端自主决定等待的秒数，如固定30秒或60秒内随机。
     *   - >0: 以服务器告知的等待时长为准。
     */
    int waitSec;

    // 后续可扩展其它的TCP连接属性
    // ...

} connection_token_t;

#pragma pack()

void json_buffer_clear(void *pdata);
char *json_ota_http_request(char *type, uint64_t sn, ver_data_t ver_list[], uint8_t ver_num);
int json_ota_request(uint8_t image_num, char *type, uint64_t sn, new_image_req_t image_req[], uint8_t *output, int max_len);
char *json_ota_http_request(char *type, uint64_t sn, ver_data_t ver_list[], uint8_t ver_num);
char * json_ota_response(const char recordId[32], const char *status, const char *reason);
int json_ota_pct(const char recordId[32], 
                 char * dev_model_ascii, uint8_t end, uint8_t image_type, uint8_t where, 
                 uint8_t pct, uint8_t errcode, uint8_t *output, int max_len);
ota_cmd_list_t *json_ota_cmd_parse(const char *message);
ota_cmd_list_t *json_parse_ota_rsp(const char *message);
char * json_ota_response_first(const char recordId[32], const char *status, const char *reason,
                            uint64_t sn, char *type, uint8_t image_type, uint32_t new_ver, uint32_t old_ver);
int json_parse_record_id(char *indata, char *recordId);
char *json_get_money_save_cmd(const uint64_t device_sn, const char *device_type, uint16_t flag);
int json_get_money_save_rsp(const char *message, uint64_t *savings_amount_total, uint8_t *temperature_unit);
int json_parse_utc_ip(const char *msg, uint64_t *time);

#if CONFIG_DISASTER_WARN_CHANNEL == 1 
char * json_disaster_rcv_response(const char *Model, const char *Sn_Str, int8_t type, uint16_t uuid, char *msg_id);
char * json_disaster_start(const char *Model, const char *Sn_Str, uint32_t StartTime,  uint16_t uuid, const char *msg_id);
char * json_disaster_response(const char *Model, const char *Sn_Str, uint32_t StartTime, uint32_t EndTime, uint32_t Energy, int8_t SOC, uint16_t uuid, const char *msg_id, uint16_t reason);
esp_err_t parse_json_response(const char *response);
#elif CONFIG_DISASTER_WARN_CHANNEL == 2
char * json_disaster_response(const char *Sn_Str, uint32_t StartTime, uint32_t EndTime, uint32_t Energy, int8_t SOC, uint16_t uuid, const char *msg_id, char *dev_type);
char * json_disaster_start(const char *Sn_Str, uint32_t StartTime,  uint16_t uuid, const char *msg_id, char *dev_type);
esp_err_t json_is_rsp_message_ok(const char *response);
#endif

int parse_thunderstorm_alert(const char* json_str, _SaveWebStormWatchApiData *out_data);
int json_get_conn_token_info(const char *message, connection_token_t *conn_token_info);

char *json_power_outage_report(uint32_t start_time, uint32_t end_time, 
                               const char *device_model, uint64_t device_sn, 
                               const char *remark);
char *json_batch_power_outage_report(const char *device_model, uint64_t device_sn,
                                     const power_off_record_t *records, int record_count);
int json_parse_batch_power_outage_response(const char *response);
int json_parse_grid_off_stats_response(const char *response, grid_off_count_statistics_t *stats_out);

#endif
