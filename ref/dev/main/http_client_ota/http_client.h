#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "DisasterWarn.h"

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
    HTTPS_CHECK_IOT_IMAGE = 1,
    HTTPS_CHECK_INV_IMAGE = 2,
    HTTPS_CHECK_PACK_IMAGE = 3,
    HTTPS_CHECK_ACHUB_IMAGE = 4,
    HTTPS_CHECK_DCHUB_IMAGE = 5,
    HTTPS_CHECK_D400S_IMAGE = 6,            
};

enum {
    MODBUS_TATOL_ADDR = 0,
    MODBUS_DCDC_ADDR_1 = 1,
    MODBUS_MI_ADDR_31 = 31,
    MODBUS_BAT_ADDR_91 = 91,
    MODBUS_PLUG_ADDR_211 = 211,
    MODBUS_MAX_ADDR = 255,
};

typedef struct
{
    uint16_t type;
    uint32_t version;
}soft_ver_struct;

#define HTTP_VER_NUM    6
typedef struct
{
    bool have_new_version_flag;
    uint8_t softver_num;
    soft_ver_struct ver_list[HTTP_VER_NUM];
}new_version_update_struct;

typedef union {
    uint16_t all;
    struct {
        uint16_t timeout         : 1; // current_time >= receive_end_time
        uint16_t app_cancel      : 1; // DisasterSaveData.AppCancelStormEvent.EventEndFlag == 1
        uint16_t soc_reach       : 1; // system_soc >= DisasterSaveData.SaveWebStormWatchApiData.SoC
        uint16_t disable         : 1; // StormWatchSetData.StormWatchEnable == 2
        uint16_t discharge         : 1;
        uint16_t new_event         : 1;
        uint16_t reserved        : 10;
    } bit;
} storm_end_reason_t;

typedef struct {
    disaster_send_param_t StormSendParam;
    int8_t send_type;
    storm_end_reason_t end_reason;
} disaster_post_args_t;

void create_https_task(void);
bool sys_is_updating(void);

void https_ota_state_to_host(char recordId[32], char *state, char *reason);
void https_ota_cmd_parse(const char *pdata, uint16_t len);
bool http_ota_is_doing(void);
void set_ota_pct_info(char* model, uint16_t addr, uint8_t type, uint8_t doing, uint8_t where, uint8_t pct,  uint8_t errcode);
#if 0
bool https_disater_state_to_host(disaster_send_param_t StormSendParam, int8_t send_type, char *id);
#else
bool https_disater_state_to_host(const disaster_post_args_t *args);
#endif
int http_get_utc_time_and_ip(char *server_ip, uint16_t max_len, uint64_t *time);
int https_new_image_check(uint8_t target);
void https_new_image_timeout_clean(void);
bool Get_http_new_version_flag(uint8_t dev_type, uint8_t index) ;
void http_dst_time_update(void);
uint8_t Get_Http_Version_Check(uint8_t dev_type,uint32_t version);

#define Get_ExtPack_Version(version)  Get_Http_Version_Check(HTTPS_CHECK_PACK_IMAGE,version)

void update_firmware_check(void);
int firmware_upgrade_start(char *model, char *version);
int http_update_money_save_data(void);

#ifdef __cplusplus
}
#endif



