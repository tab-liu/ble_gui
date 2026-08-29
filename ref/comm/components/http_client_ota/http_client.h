#pragma once

#include "iot_define.h"
#include "comm_define.h"
#include "esp_http_client.h"
#include "json_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTPS_OTA_DOWNLOAD_ING      "downloading"
#define HTTPS_OTA_DOWNLOAD_ERR      "download failed"
#define HTTPS_OTA_DOWNLOAD_OK       "download success"
#define HTTPS_OTA_IMAGE_ERROR       "image error"
#define HTTPS_OTA_UPGRADING         "upgrading"
#define HTTPS_OTA_SUCCESS           "success"
#define HTTPS_OTA_FAILED            "failure"

#define MI_A80_DEV_TYPE             "A80"
#define MI_A100_DEV_TYPE            "A100"
#define DC_D100S_DEV_TYPE           "D100S"
#define MI_A100S_DEV_TYPE           "A100S"

#define DTU_COMBOX_DEV_TYPE         "COMBOX"

enum{
    HTTPS_CHECK_DEVASSIST = 0,
    HTTPS_CHECK_IOT_IMAGE = 1,
    HTTPS_CHECK_INV_IMAGE = 2,
    HTTPS_CHECK_PACK_IMAGE = 3,
    HTTPS_CHECK_DCDC_IMAGE = 4,
    HTTPS_CHECK_DCHUB_IMAGE = 5,
};


int set_http_client_netif(uint8_t netif_type);
void https_ota_state_to_host(char recordId[32], char *state, char *reason);
void https_ota_cmd_parse(const char *pdata, uint16_t len);
bool http_ota_is_doing(void);
void set_ota_pct_info(char* model, uint16_t addr, uint8_t type, uint8_t doing, uint8_t where, uint8_t pct,  uint8_t errcode);
bool Get_http_new_version_flag(uint8_t dev_type, uint8_t index) ;
int http_get_utc_time_and_ip(char *server_ip, uint16_t max_len, uint64_t *time);

#if CONFIG_DISASTER_WARN_CHANNEL == 1 
bool https_disater_state_to_host(disaster_send_param_t StormSendParam, int8_t send_type, char *id);
#elif CONFIG_DISASTER_WARN_CHANNEL == 2
bool https_disater_state_to_host(disaster_send_param_t StormSendParam, int8_t send_type);
#endif

int http_request_money_save(const uint64_t device_sn, const char *device_type, uint16_t flag, uint8_t *temperature_unit_out, uint32_t *saved_money_out);

int http_report_power_outage(uint32_t start_time, uint32_t end_time);
int http_batch_report_power_outages(const void *records, int record_count);
int http_get_grid_off_count(grid_off_count_statistics_t *stats_out);

void update_firmware_check(void);
int firmware_upgrade_start(char *model, char *version, uint8_t chl);
void https_new_image_timeout_clean(void);
void http_dst_time_update(void);
void http_period_update(void);
int set_http_header(esp_http_client_handle_t client);
connection_token_t *http_conn_token_info_get(void);
void https_period_task(void *pvParameters);

#ifdef __cplusplus
}
#endif



