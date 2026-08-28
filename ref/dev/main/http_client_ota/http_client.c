#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_ota_ops.h>

#include <errno.h>
#include <sys/param.h>
//#include "comm_type.h"
#include "http_client.h"
#include "ota_type.h"
//#include "iot_flash.h"
#include "image_handle.h"
#include "iot_ota.h"
#include "json_cmd.h"
#include "comm_define.h"
//#include "system_data.h"
//#include "mqtt_handle.h"
//#include "iot_crc.h"
//#include "xmodem_server.h"
//#include "bms_ota.h"

#include "iot_mqtt.h"
#include "esp_app_format.h"
#include "filesystem.h"
//#include "cc1312_ota.h"
#include "xmodem_client.h"
#include "uart_device_process.h"
#include "iot_period_task.h"
#include "can_protocol.h"
#include "uart_ota.h"
#include "xmodem_transmitter.h"
#include "mesh_api.h"
#include "app_ble.h"
#include "DisasterWarn.h"
#include "cJSON.h"
#include "can_pack.h"
#include "dev_discovery.h"
#include "app_time.h"
#include "pack_type.h"
#include "webserver.h"
#include "utils.h"

//#include "mesh_lite_tcp.h"
// #include "wifi_mesh_ota.h"

#define TAG "[HTTPS]"

#define ESP_ERR_HTTPS_OTA_BASE            (0x9000)
#define ESP_ERR_HTTPS_OTA_IN_PROGRESS     (ESP_ERR_HTTPS_OTA_BASE + 1)  /* OTA operation in progress */

#define IMAGE_HEADER_SIZE       (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1)
#define DEFAULT_OTA_BUF_SIZE    IMAGE_HEADER_SIZE
#define DEFAULT_REQUEST_SIZE    (32 * 1024)

#define MAX_HTTP_RECV_BUFFER   2048
#define MAX_HTTP_OUTPUT_BUFFER 2048

#define HTTPS_OTA_HEAD  4

extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];

typedef enum {
    ESP_HTTPS_OTA_INIT,
    ESP_HTTPS_OTA_BEGIN,
    ESP_HTTPS_OTA_IN_PROGRESS,
    ESP_HTTPS_OTA_SUCCESS,
} https_ota_state;

typedef struct {
    const esp_http_client_config_t *http_config;
    bool partial_http_download; // 文件分片下载
    bool bulk_flash_erase;
    bool ota_esp_self;
    bool to_mesh;
    // bool iot_to_mesh;
    uint8_t type;
    char model[12];
    uint8_t dev_index;
    char dev_model_ascii[128];
    uint8_t dev_target_id;
    uint8_t dev_can_id;
    uint32_t max_http_request_size;
    uint32_t image_flash_offset;
    uint32_t image_flash_size;
    
    uint32_t image_flags_offset;
    uint32_t image_flags_size;

    uint8_t *ota_buffer;
} http_ota_config_t;

typedef struct {
    esp_ota_handle_t update_handle;
    esp_http_client_handle_t http_client;
    const esp_partition_t *update_partition;
    https_ota_state state;
    char *ota_upgrade_buf;
    uint32_t max_http_request_size;
    size_t ota_upgrade_buf_size;
    uint32_t binary_file_len; // 镜像接收长度
    uint32_t image_length; // 后台镜像总长度
    bool partial_http_download; // 文件分片下载
    bool bulk_flash_erase; // 大块flash擦除
    bool ota_esp_self;
    bool to_mesh;   // 发送到mesh标记
    // bool iot_to_mesh; // 发送到mesh标记且为IOT文件，文件需要写到文件系统
    uint32_t image_flash_offset;
    uint32_t image_flash_size;
    
    uint32_t image_head_offset;
    uint32_t image_head_size;
}http_ota_handle_t;

static image_info_t *pimage_head;

static bool is_update_check_triggered = false;
ota_cmd_list_t *new_firmware_list = NULL;
static bool need_query_recordId = false;
static char image_record_id[32] = {0};

bool http_ota_doing = false;
EXT_RAM_BSS_ATTR static ota_cmd_list_t *ota_cmd_list = NULL;
EXT_RAM_BSS_ATTR ota_data_t2 ota_data;
extern ota_mode_t ota_mode;//test httpOTA


#define RESPONSE_BUFFER_SIZE 256

#define HTTPS_GET_UTC_IP_TIMEOUT 5000 // 5s超时


#define HTTPS_GET_TIMEZONE_BIT  BIT0
#define HTTPS_GET_IP_BIT        BIT1
#define HTTPS_GET_TIME_DST_BIT      BIT2
#define HTTPS_GET_TIME_DST_STAMP_BIT    BIT3
#define HTTPS_GET_TIME_DST_ISO8601_BIT  BIT4

#if HTTP_UTC_TIME_VERSION==2
#define HTTP_UTC_TIME               "/api/midppkic/cert/app/v2/now/utc-time"
#else
#define HTTP_UTC_TIME               "/api/midppkic/cert/app/v1/now/utc-time"
#endif

EXT_RAM_BSS_ATTR char gw_server_ip[128] = {0}; 
EXT_RAM_BSS_ATTR int32_t timezone_offset = 0; 
EXT_RAM_BSS_ATTR time_t server_time_now = 0;
EXT_RAM_BSS_ATTR struct tm server_tm_time = {0};
EXT_RAM_BSS_ATTR int server_tz_hour=0;
EXT_RAM_BSS_ATTR int server_tz_min=0;

EXT_RAM_ATTR static char response_buffer[RESPONSE_BUFFER_SIZE];
static EventGroupHandle_t  http_event_handle = NULL;
EXT_RAM_BSS_ATTR static new_version_update_struct iot_version_update;
EXT_RAM_BSS_ATTR static new_version_update_struct inv_version_update;
EXT_RAM_BSS_ATTR static new_version_update_struct dchub_version_update;
EXT_RAM_BSS_ATTR static new_version_update_struct achub_version_update;
EXT_RAM_BSS_ATTR static new_version_update_struct pack_version_update[PACK_TYPE_MAX_NUM];
EXT_RAM_BSS_ATTR static new_version_update_struct d400s_version_update;

static void check_firmware_updates(void);
static bool check_firmware_version(uint32_t new_version, uint32_t old_version, bool force);
static int https_ota_get_record_id(char recordId[32], char *state, char *reason, uint8_t image_type, uint32_t new_ver, uint32_t old_ver);


static bool tm_time_to_timestamp(time_t *timestamp, struct tm *tm_time) 
{
    // 检查输入指针是否为空
    if (timestamp == NULL || tm_time == NULL) {
        ESP_LOGE(TAG, "Error: Input pointer is NULL.");
        return false;
    }

    // 检查 tm_time 的字段是否在合理范围内
    if (tm_time->tm_year < 70 || tm_time->tm_mon < 0 || tm_time->tm_mon > 11 ||
        tm_time->tm_mday < 1 || tm_time->tm_mday > 31 ||
        tm_time->tm_hour < 0 || tm_time->tm_hour > 23 ||
        tm_time->tm_min < 0 || tm_time->tm_min > 59 ||
        tm_time->tm_sec < 0 || tm_time->tm_sec > 59) {
        ESP_LOGE(TAG, "Error: tm_time contains invalid values.");
        return false;
    }

    // 转换为时间戳（假设本地时区与字符串中的时区一致）
    *timestamp = mktime(tm_time);

    // 检查 mktime 的返回值
    if (*timestamp == -1) {
        ESP_LOGE(TAG, "Error: Failed to convert to timestamp.");
        return false;
    }

    return true;
}

// static bool parse_iso8601_to_tm_timestamp(const char *datetime_str, struct tm *tm_timestamp) {
//     struct tm tm_time = {0};
//     int tz_hour = 0, tz_min = 0;
//     char sign = '+';

//     // 检查输入是否为空
//     if (datetime_str == NULL || tm_timestamp == NULL) {
//         ESP_LOGE(TAG, "Error: Input string or timestamp pointer is NULL.");
//         return false;
//     }

//     // 检查输入长度是否合理
//     if (strlen(datetime_str) < 25) { // ISO 8601 格式的最短长度
//         ESP_LOGE(TAG, "Error: Input string is too short.");
//         return false;
//     }

//     // 解析日期、时间和时区偏移部分
//     int parsed_items = sscanf(datetime_str, "%d-%d-%dT%d:%d:%d.%*d%c%d:%d",
//                                &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
//                                &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec,
//                                &sign, &tz_hour, &tz_min);

//     // 检查解析结果
//     if (parsed_items != 9) {
//         ESP_LOGE(TAG, "Error: Failed to parse datetime string. Ensure the format is correct.");
//         return false;
//     }

//     // 检查日期和时间的范围
//     if (tm_time.tm_year < 1900 || tm_time.tm_year > 9999) {
//         ESP_LOGE(TAG, "Error: Year out of range (1900-9999).");
//         return false;
//     }
//     if (tm_time.tm_mon < 1 || tm_time.tm_mon > 12) {
//         ESP_LOGE(TAG, "Error: Month out of range (1-12).");
//         return false;
//     }
//     if (tm_time.tm_mday < 1 || tm_time.tm_mday > 31) {
//         ESP_LOGE(TAG, "Error: Day out of range (1-31).");
//         return false;
//     }
//     if (tm_time.tm_hour < 0 || tm_time.tm_hour > 23) {
//         ESP_LOGE(TAG, "Error: Hour out of range (0-23).");
//         return false;
//     }
//     if (tm_time.tm_min < 0 || tm_time.tm_min > 59) {
//         ESP_LOGE(TAG, "Error: Minute out of range (0-59).");
//         return false;
//     }
//     if (tm_time.tm_sec < 0 || tm_time.tm_sec > 59) {
//         ESP_LOGE(TAG, "Error: Second out of range (0-59).");
//         return false;
//     }

//     // 调整年份和月份
//     tm_time.tm_year -= 1900; // tm_year 是从 1900 年开始计数
//     tm_time.tm_mon -= 1;     // tm_mon 是从 0 开始计数

//     // 将解析后的值复制到 tm_timestamp 指针所指向的内存中
//     *tm_timestamp = tm_time;

//     return true;
// }

static bool parse_iso8601_to_tm_timestamp(const char *datetime_str, struct tm *tm_timestamp,int *server_tz_hour,int *server_tz_min) {
    struct tm tm_time = {0};
    int tz_hour = 0, tz_min = 0;
    char sign = '+';

    // 检查输入是否为空
    if (datetime_str == NULL || tm_timestamp == NULL) {
        ESP_LOGE(TAG, "Error: Input string or timestamp pointer is NULL.");
        return false;
    }

    // 1. 先解析固定的日期和时间部分 (YYYY-MM-DDTHH:MM:SS)
    int parsed_items = sscanf(datetime_str, "%d-%d-%dT%d:%d:%d",
                               &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
                               &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

    if (parsed_items != 6) {
        ESP_LOGE(TAG, "Error: Failed to parse the base datetime part.");
        return false;
    }

    // 2. 定位到时间部分的末尾，准备解析时区
    const char *tz_part = datetime_str + 19; // 指向 "YYYY-MM-DDTHH:MM:SS" 之后的位置

    // 3. 跳过可选的毫秒部分
    if (*tz_part == '.') {
        tz_part++; // 跳过 '.'
        while (isdigit((unsigned char)*tz_part)) {
            tz_part++; // 跳过所有毫秒数字
        }
    }

    // 4. 解析时区部分
    if (*tz_part == 'Z' || *tz_part == 'z') {
        // UTC 时间
        tz_hour = 0;
        tz_min = 0;
    } else if (*tz_part == '+' || *tz_part == '-') {
        // 带偏移的时区
        sign = *tz_part;
        // 使用 sscanf 从 tz_part 的当前位置解析 HH:MM
        if (sscanf(tz_part + 1, "%d:%d", &tz_hour, &tz_min) != 2) {
            ESP_LOGE(TAG, "Error: Failed to parse timezone offset.");
            return false;
        }
    } else {
        ESP_LOGE(TAG, "Error: Invalid or missing timezone specifier.");
        return false;
    }

    // --- 后续的范围检查和赋值逻辑保持不变 ---

    // 检查日期和时间的范围
    if (tm_time.tm_year < 1900 || tm_time.tm_year > 9999) { /* ... */ return false; }
    if (tm_time.tm_mon < 1 || tm_time.tm_mon > 12) { /* ... */ return false; }
    if (tm_time.tm_mday < 1 || tm_time.tm_mday > 31) { /* ... */ return false; }
    if (tm_time.tm_hour < 0 || tm_time.tm_hour > 23) { /* ... */ return false; }
    if (tm_time.tm_min < 0 || tm_time.tm_min > 59) { /* ... */ return false; }
    if (tm_time.tm_sec < 0 || tm_time.tm_sec > 59) { /* ... */ return false; }

    // 更新时区
    if (sign == '-') {
        *server_tz_hour = -tz_hour;
    } else {
        *server_tz_hour = tz_hour;
    }
    *server_tz_min = tz_min; 

    // 调整 tm 结构体成员
    tm_time.tm_year -= 1900;
    tm_time.tm_mon -= 1;
    tm_time.tm_isdst = -1;

    // 复制结果
    *tm_timestamp = tm_time;

    return true;
}
static esp_err_t https_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            if(evt->header_key && strcmp(evt->header_key, "x-timezone-offset") == 0){
                ESP_LOGI(TAG, "timezone: %s ms", evt->header_value);
                char *endptr = NULL;
                long offset = strtol(evt->header_value, &endptr, 10);
                if (endptr && *endptr == '\0') {
                    timezone_offset = offset;
                    xEventGroupSetBits(http_event_handle, HTTPS_GET_TIMEZONE_BIT);
                } else {
                    ESP_LOGE(TAG, "Invalid timezone offset: %s", evt->header_value);
                }
            } else if(evt->header_key && strcmp(evt->header_key, "x-iot-server") == 0){
                 ESP_LOGI(TAG, "gw server ip: %s", evt->header_value);
                memset(gw_server_ip, 0, sizeof(gw_server_ip));
                strncpy(gw_server_ip, evt->header_value, sizeof(gw_server_ip)-1);
                xEventGroupSetBits(http_event_handle, HTTPS_GET_IP_BIT);
            } else if(evt->header_key && strcmp(evt->header_key, "x-dst") == 0){
                // ESP_LOGI(TAG, "time: %s", evt->header_value);
                char *endptr = NULL;
                time_t dst = strtol(evt->header_value, &endptr, 10);
                if (endptr && *endptr == '\0') {
                    server_time_now = dst;
                    ESP_LOGI(TAG, "get dst time success(timestamp) : %llu", server_time_now);
                    xEventGroupSetBits(http_event_handle, HTTPS_GET_TIME_DST_STAMP_BIT | HTTPS_GET_TIME_DST_BIT);
                } else if(parse_iso8601_to_tm_timestamp(evt->header_value, &server_tm_time,&server_tz_hour,&server_tz_min)) {
                    ESP_LOGI(TAG, "get dst time success(iso8601) : %s", evt->header_value);
                    xEventGroupSetBits(http_event_handle, HTTPS_GET_TIME_DST_ISO8601_BIT | HTTPS_GET_TIME_DST_BIT);
                } else {
                    ESP_LOGE(TAG, "Invalid time dst: %s", evt->header_value);
                }
            }
            break;
        case HTTP_EVENT_ON_DATA:
            memset(response_buffer, 0, RESPONSE_BUFFER_SIZE); // 清空缓冲区
            if (evt->data_len >= RESPONSE_BUFFER_SIZE) {
                
                ESP_LOGE(TAG, "Data length exceeds response buffer size");
                memcpy(response_buffer, evt->data, RESPONSE_BUFFER_SIZE-1);
                response_buffer[RESPONSE_BUFFER_SIZE - 1] = '\0'; // 确保字符串以空字符结尾
                // 处理截断后的数据
            } else {
                memcpy(response_buffer, evt->data, evt->data_len);
                response_buffer[evt->data_len] = '\0'; // Null-terminate the response
            }
            

            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            ESP_LOG_BUFFER_HEXDUMP(TAG, response_buffer, evt->data_len, ESP_LOG_INFO);


            break;
        case HTTP_EVENT_ON_FINISH:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            // ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;

        default:  
        // case HTTP_EVENT_REDIRECT:
            ESP_LOGW(TAG, "HTTP_EVENT_ON_REDIRECT");
            break;
    }
    return ESP_OK;
}

bool sys_is_updating(void) {
//    if (
//		xmd_server_is_doing() 
//		|| xmd_client_is_doing() 
//		||
//        bms_ota_is_doing() ||
//         http_ota_is_doing()
//        ) 
	if((esp_ota_is_doing())
//        ||(cc1312_ota_is_doing())
        ||(OTA_CH_UNKOWN != xmodem_client_is_doing())
        ||(http_ota_is_doing()))
    {
        //ESP_LOGE(TAG, "sys_is_updating");
        return true;
    }
#ifdef FUNC_ONECLICK_UPGRADE_EN
    if (can_ota_status[0].period_cmd_delay != 0) {
        if (!Can_OneClikeOta_Status_Get()) {
            return false;
        }
        return true;
    }
#endif
    return false;
}

// where = 1：IOT发送给设备，2：设备发送给子设备，3：服务器下载软件到IoT
// 同时http升级中，由此处代替app下发700~705的值 -> Inv_WR
void set_ota_pct_info(char* model, uint16_t addr, uint8_t type, uint8_t doing, 
                      uint8_t where, uint8_t pct,  uint8_t errcode) 
{    
    // extern ota_mode_t ota_mode;//test httpOTA
    char buffer[100] = {0};

    
    if(model != NULL)
    {
        ESP_LOGI(TAG,"set_ota_pct_info TYPE:%d model:%s",type,model);
        char dev_type[12]="SolarX 4K";
        if (type == DEVICE_IOT)
    	{  
            if (memcmp(model,dev_type, strlen(dev_type)) == 0) 
            {
                ota_mode = OTA_FULL_BROADCAST;//test httpOTA
                g_self_data.mod_reg00700_OTA.ota_cmd_group.dev_type = GROUP_CHARGE;  // IOT组
                g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_CHARGE;
                Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_CHARGE;  // IOT组
            }else 
            {
                ota_mode = OTA_FULL_BROADCAST;//test httpOTA
                g_self_data.mod_reg00700_OTA.ota_cmd_group.dev_type = GROUP_IOT;  // IOT组
                g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_IOT;
                Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_IOT;  // IOT组
            }
        }
        else if (type == DEVICE_DSP)
        {   
            if (memcmp(model,dev_type, strlen(dev_type)) == 0) 
            {
                ota_mode = OTA_FULL_BROADCAST;//test httpOTA
                g_self_data.mod_reg00700_OTA.ota_cmd_group.dev_type = GROUP_CHARGE;  // IOT组
                g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_CHARGE;
                Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_CHARGE;  // IOT组
            }else 
            {
                ota_mode = OTA_FULL_BROADCAST;//test httpOTA
                g_self_data.mod_reg00700_OTA.ota_cmd_group.dev_type = GROUP_INV;  // 逆变组 ota_cmd_group是720需要的值
                g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_INV;  // 逆变组 ota_cmd是700~705需要的值，在蓝牙升级中是由app下发，在http远程升级中由IOT自身写入
                Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_INV;  // 逆变组
            }
        }
        else if ((type == DEVICE_ARM) || (type == DEVICE_DC_HUB) || (type == DEVICE_AC_HUB)) //ARM,DSP
        {
            ota_mode = OTA_FULL_BROADCAST;//test httpOTA
            g_self_data.mod_reg00700_OTA.ota_cmd_group.dev_type = GROUP_INV;  // 逆变组 ota_cmd_group是720需要的值
            g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_INV;  // 逆变组 ota_cmd是700~705需要的值，在蓝牙升级中是由app下发，在http远程升级中由IOT自身写入
            Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_INV;  // 逆变组
        }
        else if ((type == DEVICE_BMS) || (type == DEVICE_PACK_BMS))// AP300 电池包无论内置外置升级类型都是3，远程升级时服务器下发的外置电池包类型为7，但主要靠料号区分
        {
            ota_mode = OTA_FULL_BROADCAST;//test httpOTA
            g_self_data.mod_reg00700_OTA.ota_cmd_group.dev_type = GROUP_PACK;  // 电池包组
            g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_PACK;  // 电池包组
            Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type = GROUP_PACK;  // 电池包组
        } 
    	else 
    	{
            ESP_LOGE(__func__, "Unkonw image type: %d", type);
            return ;
        }

        // pct_clear_time = 5000; // 如果进度5s未刷新则清除该段的数据


        g_self_data.mod_reg00700_OTA.ota_cmd_group.dev_id = addr; // 设备地址
        g_self_data.mod_reg00700_OTA.ota_group[0].isOta = doing; // 升级状态和升级阶段
        g_self_data.mod_reg00700_OTA.ota_group[0].where = where;
        g_self_data.mod_reg00700_OTA.ota_group[0].type  = type;
        g_self_data.mod_reg00700_OTA.ota_group[0].level = 1; // 传输深度 始终为1;
        g_self_data.mod_reg00700_OTA.ota_group[0].pct  = pct; // 传输进度和错误码
        g_self_data.mod_reg00700_OTA.ota_group[0].errCode = errcode; // 错误码
        if(errcode)
        { 
            for (uint8_t i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) 
            {
                can_ota_status[0].devStatus[i].errCode = errcode;
            }
        }

        g_self_data.mod_reg00700_OTA.ota_cmd.type = type; // 将升级类型给到700~705段(类似于app下发的指令)，便于统计进度函数can_ota_summary的使用

        memcpy(&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA,&g_self_data.mod_reg00700_OTA, sizeof(MOD_STRUCT_reg00700));

        // ESP_LOGW(TAG,"000 ---Inv[0].mod_reg00700_OTA");
		// esp_log_buffer_hex(TAG, &Inv[0].mod_reg00700_OTA, sizeof(MOD_STRUCT_reg00700));
    }
    ESP_LOGI(TAG,"set_ota_pct_info where:%d pct:%d",where,pct);
    image_info_t *image_info = get_image_info();
    if (strlen(image_info->recordId) ) {
        if (errcode) {
            if (where != 3) {
                snprintf(buffer, sizeof(buffer), "%d", errcode);
                ESP_LOGE(__func__, "HTTPS_OTA_FAILED: %d", errcode);
                https_ota_state_to_host(image_info->recordId, HTTPS_OTA_FAILED, buffer);
            }
        } else if (pct == 100) {
            if (where == 2) {   // 透传升级成功上报状态 (?)   AC380不用where判断
                https_ota_state_to_host(image_info->recordId, HTTPS_OTA_SUCCESS, ""); 
            }
        } else {
        }
    }
}

static bool redirection_required(int status_code)
{
    switch (status_code) {
        case HttpStatus_MovedPermanently:
        case HttpStatus_Found:
        case HttpStatus_SeeOther:
        case HttpStatus_TemporaryRedirect:
        case HttpStatus_PermanentRedirect:
            return true;
        default:
            return false;
    }
    return false;
}

static bool process_again(int status_code)
{
    switch (status_code) {
        case HttpStatus_MovedPermanently:
        case HttpStatus_Found:
        case HttpStatus_SeeOther:
        case HttpStatus_TemporaryRedirect:
        case HttpStatus_PermanentRedirect:
        case HttpStatus_Unauthorized:
            return true;
        default:
            return false;
    }
    return false;
}

static esp_err_t _http_handle_response_code(esp_http_client_handle_t http_client, int status_code)
{
    esp_err_t err;
    if (redirection_required(status_code)) {
        err = esp_http_client_set_redirection(http_client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "URL redirection Failed");
            return err;
        }
    } else if (status_code == HttpStatus_Unauthorized) {
        esp_http_client_add_auth(http_client);
    } else if(status_code == HttpStatus_NotFound || status_code == HttpStatus_Forbidden) {
        ESP_LOGE(TAG, "File not found(%d)", status_code);
        return ESP_FAIL;
    } else if (status_code >= HttpStatus_BadRequest && status_code < HttpStatus_InternalError) {
        ESP_LOGE(TAG, "Client error (%d)", status_code);
        return ESP_FAIL;
    } else if (status_code >= HttpStatus_InternalError) {
        ESP_LOGE(TAG, "Server error (%d)", status_code);
        return ESP_FAIL;
    }

    char upgrade_data_buf[DEFAULT_OTA_BUF_SIZE];
    // process_again() returns true only in case of redirection.
    if (process_again(status_code)) {
        while (1) {
            /*
             *  In case of redirection, esp_http_client_read() is called
             *  to clear the response buffer of http_client.
             */
            int data_read = esp_http_client_read(http_client, upgrade_data_buf, DEFAULT_OTA_BUF_SIZE);
            if (data_read <= 0) {
                return ESP_OK;
            }
        }
    }
    return ESP_OK;
}

static int http_ota_flags_update(char recordId[32], uint8_t type, uint32_t version, uint8_t target, uint32_t kbytes, bool to_mesh) 
{
    if(to_mesh == true)
    {
        pimage_head->vaildTimes = 0; // 发送给mesh的固件不允许上电重传
    } 
    else
    {
        pimage_head->vaildTimes = IMAGE_VAILD_TIMES; //文件有效次数，次数为=0，不再发送给底层设备
    }
    
    pimage_head->totalKbyte = kbytes;    //modbus 文件总的大小KB
    pimage_head->version    = version;   // 当前文件版本号
    pimage_head->slaveAddr  = 0x01;      //modbus 从机地址
    pimage_head->type = type;            //modbus 文件类型
    pimage_head->baud = 0;               //modbus 保留
    if (pimage_head->type == DEVICE_DSP) 
    {
        pimage_head->mainDevId = 0;   // modbus 主设备号
        pimage_head->subDevId = 0;    // modbus 子设备号
        pimage_head->cmdLen = 0x02;   // modbus启动命令长度
    } 
//    else if ( pimage_head->type == DEVICE_PACK_BMS || pimage_head->type == DEVICE_PACK_M1) 
//    {
//        pimage_head->mainDevId = target; // 
//        pimage_head->subDevId = 0;   //modbus 子设备号  
//        pimage_head->cmdLen = 0x05; //modbus启动命令长度    
//    } 
    else if (pimage_head->type == DEVICE_RF) 
    {
        pimage_head->mainDevId = 0;   // modbus 主设备号
        pimage_head->subDevId = 0;    // modbus 子设备号  
        pimage_head->slaveAddr  = 0x00;      //modbus 从机地址
        pimage_head->cmdLen = 0x02; //modbus启动命令长度
    } 
    else if (pimage_head->type == DEVICE_IOT) 
    {
        pimage_head->mainDevId = 0;   // modbus 主设备号
        pimage_head->subDevId = 0;    // modbus 子设备号  
        pimage_head->slaveAddr  = 0x00;      //modbus 从机地址
        pimage_head->cmdLen = 0x06; //modbus启动命令长度
    } 
    else 
    {
        return -1; // 未知固件
    }
    
    ESP_LOGI(TAG, "image type:       %d", pimage_head->type);
    ESP_LOGI(TAG, "image version:    %lu", pimage_head->version);
    ESP_LOGI(TAG, "image mainDevId:  %d", pimage_head->mainDevId);
    ESP_LOGI(TAG, "image subDevId:   %d", pimage_head->subDevId);
    ESP_LOGI(TAG, "image totalKbyte: %d", pimage_head->totalKbyte);
    ESP_LOGI(TAG, "image remaining count: %d", pimage_head->vaildTimes);
    memcpy(pimage_head->recordId, recordId, 32);
    pimage_head->crc16 = xm_calcu_crc16((const uint8_t *)pimage_head, sizeof(image_info_t) - 2);
    if (iot_image_write(IMAGE_HEAD_AREA_ADDRESS, (uint8_t *)pimage_head, sizeof(image_info_t)) != ESP_OK) {
        ESP_LOGE(TAG, "image head info write falsh failed");
        return -1;
    } 
    return 0;
}

static int http_ota_have_new_image(ota_object_t *object) 
{

    ota_cmd_list_t  *list_head = ota_cmd_list; // 判断后台是否返回新的固件信息
    if (list_head == NULL) {
        return -1;
    }

    *object = list_head->object;
    ota_cmd_list = list_head->next;
    free(list_head);
    ESP_LOGI(TAG,"gXmodem_Status.firmware_resend_step:%d",gXmodem_Status.firmware_resend_step);
    if(gXmodem_Status.firmware_resend_step !=0)//还处于iot并机CAN升级中
    {
        return -1;
    }
    return 0;
}

esp_err_t _ota_write(http_ota_handle_t *https_ota_handle, const void *buffer, size_t buf_len)
{   
    esp_err_t err;
    if (buffer == NULL || https_ota_handle == NULL) {
        return ESP_FAIL;
    }
    if (https_ota_handle->ota_esp_self) {
        err = esp_ota_write(https_ota_handle->update_handle, buffer, buf_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
            return err;
        }
    } else {
        err = iot_image_write(https_ota_handle->image_flash_offset, buffer, buf_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error: iot_image_write failed! err=0x%x", err);
            return err;
        } else {
            https_ota_handle->image_flash_offset += buf_len;
        }
    }

    https_ota_handle->binary_file_len += buf_len;
    // ESP_LOGI(TAG, "Written image length %d", https_ota_handle->binary_file_len);
    return ESP_ERR_HTTPS_OTA_IN_PROGRESS;
}

static esp_err_t _http_connect(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_FAIL;
    int status_code, header_ret;
    do {
        char *post_data = NULL;
        /* Send POST request if body is set.
         * Note: Sending POST request is not supported if partial_http_download
         * is enabled
         */
          if (http_client != NULL) {
            ESP_LOGI(TAG, "client post_data");
        } else {
            ESP_LOGI(TAG, "No client post_data");
        }
        
        int post_len = esp_http_client_get_post_field(http_client, &post_data);
        err = esp_http_client_open(http_client, post_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            return err;
        }

        if (post_len) {
            int write_len = 0;
            while (post_len > 0) {
                write_len = esp_http_client_write(http_client, post_data, post_len);
                if (write_len < 0) {
                    ESP_LOGE(TAG, "Write failed");
                    return ESP_FAIL;
                }
                post_len -= write_len;
                post_data += write_len;
            }
        }
        
        header_ret = esp_http_client_fetch_headers(http_client);
        if (header_ret < 0) {
            return header_ret;
        }
        status_code = esp_http_client_get_status_code(http_client);
        err = _http_handle_response_code(http_client, status_code);
        if (err != ESP_OK) {
            return err;
        }
    } while (process_again(status_code));
    return err;
}

static void _http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}
// 未使用
#if 0 
static int esp_https_ota_get_image_len_read(http_ota_handle_t *https_ota_handle)
{
    http_ota_handle_t *handle = (http_ota_handle_t *)https_ota_handle;
    if (handle == NULL) {
        return -1;
    }

    if (handle->state < ESP_HTTPS_OTA_IN_PROGRESS) {
        return -1;
    }

    return handle->binary_file_len;
}

static int esp_https_ota_get_image_size(http_ota_handle_t *https_ota_handle)
{
    http_ota_handle_t *handle = (http_ota_handle_t *)https_ota_handle;
    if (handle == NULL) {
        return -1;
    }
    if (handle->state < ESP_HTTPS_OTA_BEGIN) {
        return -1;
    }
    return handle->image_length;
}

static esp_err_t esp_https_ota_get_img_desc(http_ota_handle_t *https_ota_handle, esp_app_desc_t *new_app_info)
{
    http_ota_handle_t *handle = (http_ota_handle_t *)https_ota_handle;
    if (handle == NULL || new_app_info == NULL)  {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (handle->state < ESP_HTTPS_OTA_BEGIN) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc: Invalid state");
        return ESP_FAIL;
    }

    if (read_header(handle) != ESP_OK) {
        return ESP_FAIL;
    }
    
    memcpy(new_app_info, &handle->ota_upgrade_buf[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
    return ESP_OK;
}
#endif
static esp_err_t read_header(http_ota_handle_t *handle)
{
    /*
     * `data_read_size` holds number of bytes needed to read complete header.
     * `bytes_read` holds number of bytes read.
     */
    int data_read_size = IMAGE_HEADER_SIZE;
    int data_read = 0, bytes_read = 0;
    /*
     * while loop is added to download complete image headers, even if the headers
     * are not sent in a single packet.
     */
    while (data_read_size > 0 && !esp_http_client_is_complete_data_received(handle->http_client)) {
        data_read = esp_http_client_read(handle->http_client,
                                          (handle->ota_upgrade_buf + bytes_read),
                                          data_read_size);
        /*
         * As esp_http_client_read doesn't return negative error code if select fails, we rely on
         * `errno` to check for underlying transport connectivity closure if any
         */
        if (errno == ENOTCONN || errno == ECONNRESET || errno == ECONNABORTED || data_read < 0) {
            ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
            break;
        }
        data_read_size -= data_read;
        bytes_read += data_read;
    }
    if (data_read_size > 0) {
        ESP_LOGE(TAG, "Complete headers were not received");
        return ESP_FAIL;
    }

    handle->binary_file_len = bytes_read;
    ESP_LOGI(TAG, "Complete headers length: %d", bytes_read);
    return ESP_OK;
}

static esp_err_t esp_ota_verify_chip_id(void *arg)
{
    esp_image_header_t *data = (esp_image_header_t*)(arg);
    if (data->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
        ESP_LOGE(TAG, "Mismatch chip id, expected %d, found %d", CONFIG_IDF_FIRMWARE_CHIP_ID, data->chip_id);
        return ESP_ERR_INVALID_VERSION;
    }
    return ESP_OK;
}

static esp_err_t https_ota_crc_verify(uint32_t address, uint32_t len) {

    // 找出文件末尾追加的CRC32
    uint32_t crc32A = 0;
    uint32_t endAddr = address + len - 1;
    uint32_t remaining = len;
    for (int i = 0; i < 2048; i++) {
        uint8_t data = 0; 
        if (iot_image_read(endAddr, &data, 1) == ESP_OK) {
            if (data != 0x1A) {   // 排除文件内部填充的0x1A
                endAddr -= JUMP_BYTES;  // 跳过20字节填充信息
                remaining -= JUMP_BYTES;
                iot_image_read(endAddr + 1, (uint8_t *)&crc32A, 4);
                break;
            }
            endAddr -= 1;
            remaining -= 1;
        } else {
            ESP_LOGI(TAG, "image interval crc32 read failed");
            break;
        }
    }

    uint8_t *buffer = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer != NULL) {
        memset(buffer, 0, 1024);
    }
    if (buffer == NULL) {
        ESP_LOGI(TAG, "file: %s, function: %s, %d, malloc(1024) falied", __FILE__, __func__, __LINE__);
        return ESP_ERR_NO_MEM;
    }

    uint32_t image_crc32 = 0xFFFFFFFF; // 设置CRC32初始值
    while (remaining) {
        int size = (remaining > 1024) ? 1024 : remaining;
        esp_err_t err = iot_image_read(address, buffer, size);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "iot read image data failed %d (address: %lu, size: %d)", err, address, size);
            break;
        }
        image_crc32 = calcu_crc32(image_crc32, buffer, size);
        remaining -= size;
        address += size;
    }
    free(buffer);

    image_crc32 = ntohl(image_crc32);
    if (crc32A != image_crc32) {
        ESP_LOGE(TAG, "image crc32 verify failed (file: 0x%04lX, new: 0x%04lX)", crc32A, image_crc32);
        return ESP_FAIL;
    }  
    ESP_LOGI(TAG, "image crc32 verify successfully: (file: 0x%04lx, new: 0x%04lx)", crc32A, image_crc32);
    return ESP_OK;
}

// bool https_ota_is_complete_data_received(esp_https_ota_handle_t https_ota_handle)
// {
//     bool ret = false;
//     esp_https_ota_t *handle = (esp_https_ota_t *)https_ota_handle;
//     if (handle->partial_http_download) {
//         ret = (handle->image_length == handle->binary_file_len);
//     } else {
//         ret = esp_http_client_is_complete_data_received(handle->http_client);
//     }
//     return ret;
// }
// static bool is_server_verification_enabled(https_ota_config_t *ota_config) {
//     // return  (ota_config->http_config->cert_pem
//     //         || ota_config->http_config->use_global_ca_store
//     //         || ota_config->http_config->crt_bundle_attach != NULL);
// }

static esp_err_t https_ota_start(http_ota_config_t *ota_config, http_ota_handle_t **handle)
{
    ESP_LOGE(TAG, "get into https_ota_start");
    esp_err_t err;

    http_ota_handle_t *https_ota_handle = heap_caps_malloc(sizeof(http_ota_handle_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (https_ota_handle != NULL) {
        memset(https_ota_handle, 0, sizeof(http_ota_handle_t));
    }
    if (!https_ota_handle) {
        *handle = 0;
        ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");
        return ESP_ERR_NO_MEM;
    }
    
//     if (!is_server_verification_enabled(ota_config)) {
// #if CONFIG_OTA_ALLOW_HTTP
//         ESP_LOGW(TAG, "Continuing with insecure option because CONFIG_OTA_ALLOW_HTTP is set.");
// #else
//         ESP_LOGE(TAG, "No option for server verification is enabled in esp_http_client config.");
//         *handle = NULL;
//         return ESP_ERR_INVALID_ARG;
// #endif
//     }

    https_ota_handle->max_http_request_size = (ota_config->max_http_request_size == 0) ? DEFAULT_REQUEST_SIZE : ota_config->max_http_request_size;; // 一次http请求的最大长度
    https_ota_handle->partial_http_download = ota_config->partial_http_download;
    https_ota_handle->ota_esp_self = ota_config->ota_esp_self;
    https_ota_handle->to_mesh  = ota_config->to_mesh;
    // https_ota_handle->iot_to_mesh = ota_config->iot_to_mesh;
    https_ota_handle->image_head_offset = ota_config->image_flags_offset;
    https_ota_handle->image_head_size = ota_config->image_flags_size;

    https_ota_handle->image_flash_offset = ota_config->image_flash_offset;
    https_ota_handle->image_flash_size = ota_config->image_flash_size;    

    /* Initiate HTTP Connection */
    https_ota_handle->http_client = esp_http_client_init(ota_config->http_config);
    if (https_ota_handle->http_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        err = ESP_FAIL;
        goto failure;
    }
    
    if (https_ota_handle->partial_http_download) {
        esp_http_client_set_method(https_ota_handle->http_client, HTTP_METHOD_HEAD);
        err = esp_http_client_perform(https_ota_handle->http_client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(https_ota_handle->http_client);
            if (status != HttpStatus_Ok) {
                ESP_LOGE(TAG, "Received incorrect http status %d", status);
                err = ESP_FAIL;
                goto http_cleanup;
            }
        } else {
            ESP_LOGE(TAG, "ESP HTTP client perform failed: %d", err);
            goto http_cleanup;
        }

        // 获取镜像的总长度
        https_ota_handle->image_length = esp_http_client_get_content_length(https_ota_handle->http_client);
        esp_http_client_close(https_ota_handle->http_client);

        if (https_ota_handle->image_length > https_ota_handle->max_http_request_size) { // 总长度超过接收最大长度，计算分片下载大小
            char *header_val = NULL;
            asprintf(&header_val, "bytes=0-%lu", https_ota_handle->max_http_request_size - 1);
            if (header_val == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for HTTP header");
                err = ESP_ERR_NO_MEM;
                goto http_cleanup;
            }
            esp_http_client_set_header(https_ota_handle->http_client, "Range", header_val); // 设置下载范围
            free(header_val);
        }
        esp_http_client_set_method(https_ota_handle->http_client, HTTP_METHOD_GET);
    }

    err = _http_connect(https_ota_handle->http_client); // 建立http链接
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to establish HTTP connection");
        goto http_cleanup;
    }

    if (!https_ota_handle->partial_http_download) {
        https_ota_handle->image_length = esp_http_client_get_content_length(https_ota_handle->http_client);
    }

    https_ota_handle->update_partition = NULL;
    ESP_LOGI(TAG, "Binary File size: %lu, Starting OTA...", https_ota_handle->image_length);
    if (https_ota_handle->ota_esp_self) { // 升级IOT本机
        https_ota_handle->update_partition = esp_ota_get_next_update_partition(NULL);
        if (https_ota_handle->update_partition == NULL) {
            ESP_LOGE(TAG, "Passive OTA partition not found");
            err = ESP_FAIL;
            goto http_cleanup;
        }
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
                        https_ota_handle->update_partition->subtype, 
                        https_ota_handle->update_partition->address);
    } 

    const int alloc_size = MAX(ota_config->http_config->buffer_size, DEFAULT_OTA_BUF_SIZE);
    https_ota_handle->ota_upgrade_buf = (char *)heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (https_ota_handle->ota_upgrade_buf != NULL) {
        memset( https_ota_handle->ota_upgrade_buf, 0, alloc_size);
    }
    if (!https_ota_handle->ota_upgrade_buf) {
        ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");
        err = ESP_ERR_NO_MEM;
        goto http_cleanup;
    }
    https_ota_handle->ota_upgrade_buf_size = alloc_size;
    https_ota_handle->bulk_flash_erase = ota_config->bulk_flash_erase;

    https_ota_handle->binary_file_len = 0;
    https_ota_handle->state = ESP_HTTPS_OTA_BEGIN;
    *handle = https_ota_handle;
    return ESP_OK;

http_cleanup:
    _http_cleanup(https_ota_handle->http_client);
failure:
    free(https_ota_handle);
    *handle = NULL;
    return err;
}

static esp_err_t https_ota_perform(http_ota_handle_t *https_ota_handle)
{
    http_ota_handle_t *handle = https_ota_handle;
    if (handle == NULL) {
        ESP_LOGE(TAG, "esp_https_ota_perform: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state < ESP_HTTPS_OTA_BEGIN) {
        ESP_LOGE(TAG, "esp_https_ota_perform: Invalid state");
        return ESP_FAIL;
    }

    esp_err_t err;
    int data_read;

    switch (handle->state) {
        case ESP_HTTPS_OTA_BEGIN:
            ESP_LOGI(TAG, "ESP_HTTPS_OTA_BEGIN");
            if (handle->ota_esp_self) {
                ESP_LOGI(TAG, "IOT image ready erase flash bulk_flash_erase:%d",handle->bulk_flash_erase);
                const int erase_size = handle->bulk_flash_erase ? OTA_SIZE_UNKNOWN : OTA_WITH_SEQUENTIAL_WRITES;
               // err = esp_ota_begin(handle->update_partition, erase_size, &handle->update_handle);
               err = http_iot_ota_begin(handle->update_partition, erase_size, &handle->update_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                    return err;
                }

                ESP_LOGI(TAG, "erase flash finish handle->update_handle: %lu", handle->update_handle); 
                handle->state = ESP_HTTPS_OTA_IN_PROGRESS;
                /* In case `esp_https_ota_read_img_desc` was invoked first,
                then the image data read there should be written to OTA partition
                */
                int binary_file_len = 0;
                if (handle->binary_file_len) {
                    /*
                    * Header length gets added to handle->binary_file_len in _ota_write
                    * Clear handle->binary_file_len to avoid additional 289 bytes in binary_file_len
                    */
                    binary_file_len = handle->binary_file_len;
                } else {
                    if (read_header(handle) != ESP_OK) {
                        return ESP_FAIL;
                    }
                    binary_file_len = IMAGE_HEADER_SIZE;
                }

                handle->binary_file_len = 0;
                // 校验镜像是否合法
                err = esp_ota_verify_chip_id(handle->ota_upgrade_buf);
                if (err != ESP_OK) {
                    return err;
                }

                //esp_log_buffer_hex(TAG, handle->ota_upgrade_buf, binary_file_len);
                return _ota_write(handle, (const void *)handle->ota_upgrade_buf, binary_file_len);
            } 
            else 
            {
                err = iot_image_erase(handle->image_head_offset, handle->image_head_size);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "image head erase failed");
                    return err;
                }  
                //ESP_LOGE(TAG, "image flash erase  addr: 0x%04lx, len: %ld",
                                //handle->image_flash_offset, handle->image_flash_size);
                err = iot_image_erase(handle->image_flash_offset, handle->image_flash_size);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "image flash erase failed addr: 0x%04lx, len: %ld",
                                    handle->image_flash_offset, handle->image_flash_size);
                    return err;
                }
                else
                {
                    //实验表明，需要分区块读取，否则会触发看门狗重启：Task watchdog got triggered
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    //ESP_LOGE(TAG, "image flash erase failed addr: 0x%04lx, len: %ld",
                        //handle->image_flash_offset + handle->image_flash_size,
                        //IMAGE_FLASH_AREA_MAX_LEN_EXTERN);
                    err = iot_image_erase(handle->image_flash_offset + handle->image_flash_size,
                                        IMAGE_FLASH_AREA_MAX_LEN_EXTERN);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "image flash erase failed addr: 0x%04lx, len: %ld",
                            handle->image_flash_offset + handle->image_flash_size,
                            IMAGE_FLASH_AREA_MAX_LEN_EXTERN);
                        return err;
                    }
                }

                handle->state = ESP_HTTPS_OTA_IN_PROGRESS;
                return ESP_ERR_HTTPS_OTA_IN_PROGRESS;
            }   
            break;
            
        case ESP_HTTPS_OTA_IN_PROGRESS: // 从http 服务器get指定长度的数据
            ESP_LOGI(TAG, "ESP_HTTPS_OTA_IN_PROGRESS:%d",handle->ota_upgrade_buf_size);
            data_read = esp_http_client_read(handle->http_client,
                                             handle->ota_upgrade_buf,
                                             handle->ota_upgrade_buf_size);
            if (data_read < 0) {
                ESP_LOGE(TAG, "Read error: %s (0x%X)", 
                    esp_err_to_name(data_read), data_read);
                // 获取更详细的HTTP状态
                int status = esp_http_client_get_status_code(handle->http_client);
                ESP_LOGI(TAG, "HTTP Status: %d", status);
            }                                             
            ESP_LOGI(TAG, "ESP_HTTPS_OTA_IN_PROGRES2:%d",data_read);
            if (data_read == 0) {
                /*
                 *  esp_http_client_is_complete_data_received is added to check whether
                 *  complete image is received.
                 */
                bool is_recv_complete = esp_http_client_is_complete_data_received(handle->http_client);
                /*
                 * As esp_http_client_read doesn't return negative error code if select fails, we rely on
                 * `errno` to check for underlying transport connectivity closure if any.
                 * Incase the complete data has not been received but the server has sent
                 * an ENOTCONN or ECONNRESET, failure is returned. We close with success
                 * if complete data has been received.
                 */
                if ((errno == ENOTCONN || errno == ECONNRESET || errno == ECONNABORTED) && !is_recv_complete) {
                    ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
                    return ESP_FAIL;
                } else if (!is_recv_complete) {
                    return ESP_ERR_HTTPS_OTA_IN_PROGRESS;
                } else {
                    ESP_LOGD(TAG, "Connection closed");
                }
            } else if (data_read > 0) {
                return _ota_write(handle, (const void *)handle->ota_upgrade_buf, data_read);
            } else {
                ESP_LOGE(TAG, "data read %d, errno %d", data_read, errno);
                return ESP_FAIL;
            }

            // 全部下载完成
            if (!handle->partial_http_download || 
                (handle->partial_http_download && handle->image_length == handle->binary_file_len)) {
                handle->state = ESP_HTTPS_OTA_SUCCESS;

                if (handle->ota_esp_self) { // 升级IOT本身不用补全
                    break;
                }
                ESP_LOGI(TAG,"binary_file_len:%lu",handle->binary_file_len);
                if(handle->binary_file_len % 1024){ // 补全1024
                    uint16_t padding_len = (1024 - handle->binary_file_len % 1024);//&&(padding_len<1024)
                    memset(handle->ota_upgrade_buf, 0x1A, padding_len);
                    ESP_LOGI(TAG, "Image Padding 0x1A of (%dbytes)", padding_len);
                    if (_ota_write(handle, (const void *)handle->ota_upgrade_buf, padding_len) != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
                    {
                        return ESP_FAIL;
                    }
                    handle->image_length += padding_len;
				}
            }
            break;

         default:
            ESP_LOGE(TAG, "Invalid ESP HTTPS OTA State");
            return ESP_FAIL;
            break;
    }


    if (handle->partial_http_download) { // 是否下载下一个分段片区
        if (handle->state == ESP_HTTPS_OTA_IN_PROGRESS && handle->image_length > handle->binary_file_len) {
            esp_http_client_close(handle->http_client);
            char *header_val = NULL;
            if ((handle->image_length - handle->binary_file_len) > handle->max_http_request_size) {
                asprintf(&header_val, "bytes=%ld-%ld", handle->binary_file_len, (handle->binary_file_len + handle->max_http_request_size - 1));
            } else {
                asprintf(&header_val, "bytes=%ld-", handle->binary_file_len);
            }
            if (header_val == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for HTTP header");
                return ESP_ERR_NO_MEM;
            }
            ESP_LOGI(TAG, "Range: %s", header_val);
            esp_http_client_set_header(handle->http_client, "Range", header_val);
            free(header_val);
            err = _http_connect(handle->http_client);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to establish HTTP connection");
                return ESP_FAIL;
            }
            ESP_LOGD(TAG, "Connection start");
            return ESP_ERR_HTTPS_OTA_IN_PROGRESS; // OTA操作进行中
        }
    }
    return ESP_OK;
}

static esp_err_t https_ota_finish(http_ota_handle_t *https_ota_handle)
{
    http_ota_handle_t *handle = (http_ota_handle_t *)https_ota_handle;
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state < ESP_HTTPS_OTA_BEGIN) {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    switch (handle->state) {

        case ESP_HTTPS_OTA_SUCCESS:
        case ESP_HTTPS_OTA_IN_PROGRESS:
            ESP_LOGI(TAG, "https_ota_finish");
            if (handle->ota_esp_self && reals.online_Iot_num <= 1) {
                // 结束OTA
                ESP_LOGI(TAG,"https_ota_finish2");
                err = esp_ota_end(handle->update_handle);  
                if (err != ESP_OK) {
                    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
                        ESP_LOGE(TAG, "Image validation failed, image is corrupted");
                    } else {
                        ESP_LOGE(TAG, "https_ota_finish failed (%s)!", esp_err_to_name(err));
                    }
                    // task_fatal_error();
                    return err;
                }
            } else {
            }
            /* falls through */
        case ESP_HTTPS_OTA_BEGIN:
            if (handle->ota_upgrade_buf) {
                free(handle->ota_upgrade_buf);
            }
            if (handle->http_client) {
                _http_cleanup(handle->http_client);
            }
            break;
        default:
            ESP_LOGE(TAG, "Invalid ESP HTTPS OTA State");
            break;
    }

    if ((err == ESP_OK) && (handle->state == ESP_HTTPS_OTA_SUCCESS)) {
        if (handle->ota_esp_self&& reals.online_Iot_num <= 1) {
            ESP_LOGI(TAG, "esp_ota_set_boot_partition");
            esp_err_t err = esp_ota_set_boot_partition(handle->update_partition);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
            }
        } else {
        }
    } else {
        err = ESP_FAIL;
    }
    free(handle);
    return err;
}

static esp_err_t https_ota_abort(http_ota_handle_t *https_ota_handle)
{
    http_ota_handle_t *handle = (http_ota_handle_t *)https_ota_handle;
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->state < ESP_HTTPS_OTA_BEGIN) {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    switch (handle->state) {
        case ESP_HTTPS_OTA_SUCCESS:
        case ESP_HTTPS_OTA_IN_PROGRESS:
            err = esp_ota_abort(handle->update_handle);
            /* falls through */
        case ESP_HTTPS_OTA_BEGIN:
            if (handle->ota_upgrade_buf) {
                free(handle->ota_upgrade_buf);
            }
            if (handle->http_client) {
                _http_cleanup(handle->http_client);
            }
            break;
        default:
            err = ESP_ERR_INVALID_STATE;
            ESP_LOGE(TAG, "Invalid ESP HTTPS OTA State");
            break;
    }
    free(handle);
    return err;
}


static esp_err_t https_ota_handle_func(ota_object_t *ota_object, http_ota_config_t *config, uint32_t* len)
{   
    ESP_LOGE(TAG, "get in https_ota_handle_func");
    esp_err_t err;
    if (!config || !ota_object) {
        ESP_LOGE(TAG, "esp_http_client config not found");
        return ESP_ERR_INVALID_ARG;
    }

    http_ota_handle_t *https_ota_handle = NULL;
    err = https_ota_start(config, &https_ota_handle);
    if (err != ESP_OK || https_ota_handle == NULL) {
       ESP_LOGE(TAG, "https_ota_start error");
        return ESP_FAIL;
    }

    // esp_app_desc_t app_desc;
    // esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    uint8_t pct = 0;
    while (1) {
        err = https_ota_perform(https_ota_handle);
        *len = https_ota_handle->binary_file_len;
        pct = (https_ota_handle->binary_file_len * 100) / https_ota_handle->image_length;
        ESP_LOGI(TAG, "Image read: %3lu, total: %3lu, pct: %hhu%% ,err:%u", 
                        https_ota_handle->binary_file_len, https_ota_handle->image_length, pct,err);
        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            set_ota_pct_info(config->model, config->dev_target_id, config->type, 1, 3, pct, OTA_ERR_NORMAL); // 升级进度填充到modbus中
            if (ota_object->method == OTA_METHOD_WEBSERVER)
            {
                web_server_upgrade_state_report(UPGRADE_STATE_DOING, pct);
            }
#ifdef FUNC_ONECLICK_UPGRADE_EN
            if (!config->ota_esp_self) {
                Can_OneClickOta_Keep();
            }
#endif
        } else { // 文件下载异常退出
#ifdef FUNC_ONECLICK_UPGRADE_EN
            if (!config->ota_esp_self) {
                Can_OneClickOta_End();
            }
#endif
            break;
        }
    }

    if (err != ESP_OK) { // 传输失败中断升级
        https_ota_abort(https_ota_handle);
        set_ota_pct_info(config->model, config->dev_target_id, config->type, 0, 3, pct, OTA_ERR_ABORT); // 升级进度填充到modbus中
        if (ota_object->method == OTA_METHOD_WEBSERVER)
        {
            web_server_upgrade_state_report(UPGRADE_STATE_FAIL, pct);
        }
        return err;
    }

    esp_err_t ota_finish_err = https_ota_finish(https_ota_handle); // 升级完成检查
    if (ota_finish_err != ESP_OK) {
        set_ota_pct_info(config->model, config->dev_target_id, config->type, 0, 3, pct, OTA_ERR_FILE); // 升级进度填充到modbus中
        if (ota_object->method == OTA_METHOD_WEBSERVER)
        {
            web_server_upgrade_state_report(UPGRADE_STATE_FAIL, pct);
        }
        return ota_finish_err;
    }
    return ESP_OK;
}

static bool check_version_and_model(ota_object_t ota_obj, uint32_t version, char* type, uint64_t sn, char* dev_model_ascii) 
{
    snprintf(dev_model_ascii, 127, "%s%llu", type, sn);

    if(0)// if(memcmp(type, INV_TYPE_A100S, strlen(type)) == 0)//广播升级不判断SN
    {
        if (memcmp(ota_obj.model, type, strlen(type)) != 0) 
        {
            ESP_LOGE(TAG, "0 Mismatch model %s != %s", type, ota_obj.model);
            return false;
        }
    }
    else
    {
        if (memcmp(ota_obj.dev_model_ascii, dev_model_ascii, strlen(dev_model_ascii)) != 0) 
        {
            ESP_LOGE(TAG, "1 Mismatch model %s != %s", dev_model_ascii, ota_obj.dev_model_ascii);
            return false;
        }
    }
    
    if (ota_obj.version / 100 != version / 100) 
    {
        ESP_LOGE(TAG, "2 Mismatch version:%lu != %lu", ota_obj.version, version);
        return false;
    }
    
    if (!ota_obj.force && ota_obj.version % 100 <= version % 100) 
    {
        ESP_LOGE(TAG, "3 ota_obj.force:%d, Mismatch version:%lu <= %lu",ota_obj.force, ota_obj.version, version);
        return false;
    }
    
    return true;
}

// void need_check_firmware_update(void)
// {
//     static int last_mday = -1;
//     time_t now = 0;
//     struct tm tm_time;

//     time(&now);//从系统获取时间
//     // 换算成日期
//     localtime_r(&now, &tm_time);

//     if (tm_time.tm_hour == 0 && tm_time.tm_min == 0 && tm_time.tm_sec < 10)
//     {
//         if (last_mday != tm_time.tm_mday)
//         {
//             last_mday = tm_time.tm_mday;
//             ESP_LOGI(TAG, "Trigger update_firmware_check at midnight: %d", last_mday);
//             update_firmware_check();
//         }
//     }
// }

/*------------------------------------------------------------------------------
 Function: get_inv_min_version
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取子设备最低版本号
  * @param[in]  uint8_t num  （0：DSP 1：RF ）
  * @param[out] None
  * @return     uint32_t
  */
static uint32_t get_inv_min_version(uint8_t num)
{
    uint8_t i = 0;
    uint32_t min_version = 0xFFFFFFFF;
    
//    for (i = 0; i < MAX_NET_POINT; i++)
//    {
//        if(NET_POINT_ONLINE == reals.net_point_base_Info[i].net_point_online)//
//        {
//            if((MicroInv[i+1].mod_reg01100_Inv_base.soft[num].version < min_version)
//                &&(MicroInv[i+1].mod_reg01100_Inv_base.soft[num].version != 0))
//            {
//                min_version = MicroInv[i+1].mod_reg01100_Inv_base.soft[num].version;
//            }
//        }
//    }

    if( num == 0 )
    {
//        for (i = 0; i < NET_SUB1G_MAX_POINT; i++)
//        {
//            if(NET_POINT_ONLINE == reals.net_point_base_Info[i].net_point_online)//
//            {
//                if((Inv[i+1].mod_reg01100_Inv_base.soft[0].version < min_version)
//                    &&(Inv[i+1].mod_reg01100_Inv_base.soft[0].version != 0))
//                {
//                    min_version = Inv[i+1].mod_reg01100_Inv_base.soft[0].version;
//                }
//            }
//        }
    }
    else if (num == 1)
    {
//        for (i = 0; i < NET_SUB1G_MAX_POINT; i++)
//        {
//            if(NET_POINT_ONLINE == reals.net_point_base_Info[i].net_point_online)//
//            {
//                if((Inv[i+1].mod_reg11000_IOT_info.software_ver < min_version)
//                    &&(Inv[i+1].mod_reg11000_IOT_info.software_ver != 0))
//                {
//                    min_version = Inv[i+1].mod_reg11000_IOT_info.software_ver;
//                }
//            }
//        }
    }


    if (min_version == 0xFFFFFFFF)
    {
        return 0;
    }

    return min_version;
}

static int https_ota_cmd_verify(ota_object_t ota_obj, http_ota_config_t *ota_config) 
{
    bool can_upgrade = false;

    ota_config->dev_target_id = 0;
    ota_config->ota_esp_self = false;
    ota_config->to_mesh = false;
    ota_config->type = ota_obj.type;
    memcpy(ota_config->model, ota_obj.model, strlen(ota_obj.model));

    char *dev_model_ascii = heap_caps_malloc(128, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (dev_model_ascii != NULL) {
        memset(dev_model_ascii, 0, 128);
    }
    if (dev_model_ascii == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for dev_model_ascii : https_ota_cmd_verify");
        return -1;
    }

    memset(dev_model_ascii, 0, 128);
    memcpy(pimage_head->recordId, ota_obj.recordId, 32);
    ESP_LOGI(TAG,"ota_obj.version:%lu,ota_obj.type:%d",ota_obj.version,ota_obj.type);
    if (ota_obj.type == DEVICE_IOT) 
	{
        // can_upgrade = check_version_and_model(ota_obj, g_self_data.mod_reg11000_IOT_info.software_ver, g_self_data.mod_reg11000_IOT_info.iot_type, g_self_data.mod_reg11000_IOT_info.iot_sn, dev_model_ascii);
        if((ota_obj.version/100)==(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver/100))
        {
            // 要区分是单机还是并机 
            if(reals.online_ACHUB_num == 0)
            {
                // AP300单机远程升级时，服务器只下发AP300的型号及其SN，IOT侧需靠版本料号判断是否升级
                ESP_LOGW(TAG,"SetData.dev_info_t.INV_dev_type=%s,SetData.dev_info_t.INV_dev_sn=%llu",SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.INV_dev_sn);
                can_upgrade = check_version_and_model(ota_obj, g_self_data.mod_reg11000_IOT_info.software_ver, SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn, dev_model_ascii);
            }
            else
            {
                // 并机时，服务器只下发HA1的型号及其SN，IOT侧也需靠版本料号判断是否升级
                can_upgrade = check_version_and_model(ota_obj, g_self_data.mod_reg11000_IOT_info.software_ver, SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, dev_model_ascii);
            }
            reals.preSoftVersion =Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver;
            if (can_upgrade) {
                ota_config->ota_esp_self = true;
                ota_data.cmd.target.bit.group = GROUP_IOT;
                ota_config->dev_can_id = IOT_CAN_ADDR + ota_config->dev_target_id;
            }
        }else if(reals.online_D400S_num )
        {
            for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
            {
                if ((ota_obj.version/100)==(Inv_D400S[node_id].mod_reg11000_IOT_info.software_ver/100))
                {
                    uint64_t d400s_iotSn=Inv_D400S[node_id].mod_reg11000_IOT_info.iot_sn;//GetUin64FromPtrSmall(Inv_D400S[node_id].mod_reg11000_IOT_info.iot_sn);
                    ESP_LOGI(TAG,"https_ota_cmd_verify node_id:%d,d400s_sn:%llu type:%s",node_id,d400s_iotSn,Inv_can[node_id].d400s_data[0].d400s_common_info.d400s_type);
                    can_upgrade = check_version_and_model(ota_obj, Inv_D400S[node_id].mod_reg11000_IOT_info.software_ver, 
                                                             Inv_D400S[node_id].mod_reg11000_IOT_info.iot_type, d400s_iotSn, dev_model_ascii);
                    reals.preSoftVersion =Inv_D400S[node_id].mod_reg11000_IOT_info.software_ver;
                    break;
                }
            }
            if (can_upgrade) {
                ota_data.cmd.target.bit.group = GROUP_CHARGE;
                ota_config->dev_can_id = D400S_CAN_ADDR + ota_config->dev_target_id;
                memcpy(ota_config->model,ota_obj.model,12);
            }
        }


        /** AC380无线mesh升级S1智能插座 */
        else
        {
            //S1智能插座的版本料号8032+01(版本号)
            #define S1_SMART_PLUG_MODEL 803201

            memset(dev_model_ascii, 0, 128);
            if(reals.online_ACHUB_num == 0)
            {
                can_upgrade = check_version_and_model(ota_obj, S1_SMART_PLUG_MODEL,
                                                SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn, dev_model_ascii);
            }
            else
            {
                can_upgrade = check_version_and_model(ota_obj, S1_SMART_PLUG_MODEL,
                                                SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, dev_model_ascii);
            }

            ESP_LOGI(TAG,"s1 smart plug can_upgrade=%d",can_upgrade);

            gXmodem_server_Status.used_chl = OTA_CH_SELF_TO_WIFI_MESH;
            ota_data.cmd.target.bit.group = GROUP_IOT;
        }
  
        
    } 
//	else if (ota_obj.type == DEVICE_DSP) 
//	{
//	    //子设备广播
//        can_upgrade = check_version_and_model(ota_obj, get_inv_min_version(0), INV_TYPE_A100S, NULL, dev_model_ascii);
//        ota_data.cmd.target.bit.group = GROUP_INV;
//    } 
	else if ((ota_obj.type == DEVICE_ARM)||(ota_obj.type == DEVICE_DSP)||(ota_obj.type == DEVICE_DC_HUB))
	{
        for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
        {
            for(int i = 0; i < Inv_can[node_id].inv_data[0].inv_about.software_total; i++)
            {
                if (ota_obj.type == Inv_can[node_id].inv_data[0].inv_about.soft[i].type &&
                    ((ota_obj.version/100)==(Inv_can[node_id].inv_data[0].inv_about.soft[i].version/100)))
                {
                    if(ota_obj.type == DEVICE_DC_HUB)
                    {
                        can_upgrade = check_version_and_model(ota_obj, Inv_can[node_id].inv_data[0].inv_about.soft[i].version, Inv_can[node_id].inv_data[0].inv_about.dev_type, Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_sn, dev_model_ascii);
                    }else
                    {
                        can_upgrade = check_version_and_model(ota_obj, Inv_can[node_id].inv_data[0].inv_about.soft[i].version, Inv_can[node_id].inv_data[0].inv_about.dev_type, Inv_can[node_id].inv_data[0].inv_about.dev_sn, dev_model_ascii);
                    }
                    reals.preSoftVersion = Inv_can[node_id].inv_data[0].inv_about.soft[i].version;
                    ota_data.cmd.target.bit.group = GROUP_INV;
                    ESP_LOGI(TAG,"https_ota_cmd_verify INV--huangji---reals.preSoftVersion:%lu",reals.preSoftVersion);
                    goto _next;
                }
            }  
        }


        for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
        {
            ESP_LOGI(TAG,"DSP D400S VERSION:%lu,dcdc_SoftwareVersion:%lu",ota_obj.version,Inv_can[node_id].d400s_data[0].d400s_common_info.dcdc_SoftwareVersion);
            if((ota_obj.type == Inv_can[node_id].d400s_data[0].d400s_common_info.dcdc_SoftwareType)&&
            ((ota_obj.version/100)==(Inv_can[node_id].d400s_data[0].d400s_common_info.dcdc_SoftwareVersion/100)))
            {
                uint64_t d400sSn=GetUin64FromPtrSmall(Inv_can[node_id].d400s_data[0].d400s_common_info.d400s_sn);
                ESP_LOGI(TAG,"Inv_can[%d].d400s_data[0].d400s_common_info.d400s_type:%s",node_id,Inv_can[node_id].d400s_data[0].d400s_common_info.d400s_type);
                can_upgrade = check_version_and_model(ota_obj, Inv_can[node_id].d400s_data[0].d400s_common_info.dcdc_SoftwareVersion, Inv_can[node_id].d400s_data[0].d400s_common_info.d400s_type , d400sSn, dev_model_ascii);
                reals.preSoftVersion = Inv_can[node_id].d400s_data[0].d400s_common_info.dcdc_SoftwareVersion;
                ota_data.cmd.target.bit.group = GROUP_CHARGE;
                ESP_LOGI(TAG,"https_ota_cmd_verify DCDC--huangji---reals.preSoftVersion:%lu",reals.preSoftVersion);
                goto _next;
            }else{
                 reals.preSoftVersion=0;
            }
        } 
_next:
        if(can_upgrade == true){
            ota_config->dev_can_id = INV_CAN_ADDR + ota_config->dev_target_id;
            // g_device_data.ota_cmd.group |= 0x0100;
            // ota_config->dev_target_id = INV_CAN_ADDR;
            //   ESP_LOGE(TAG, "g_device_data.ota_cmd.group:%04x",g_device_data.ota_cmd.group);
        }
    }
    else if (ota_obj.type == DEVICE_AC_HUB)
	{
        for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)// 保留循环是为了并机后又变成单机，从机能从Inv_can[]的数组里取到正确的机型及其SN
        {
            can_upgrade = check_version_and_model(ota_obj, Inv_can_mix.ac_hub_data[0].ac_hub_info.SoftwareVersion, Inv_can[node_id].inv_data[0].inv_about.dev_type, Inv_can[node_id].inv_data[0].inv_about.dev_sn, dev_model_ascii);
            
            if(can_upgrade == true)
            {
                ota_data.cmd.target.bit.group = GROUP_INV;
                ota_config->dev_can_id = AC_HUB_CAN_ADDR;
                for ( uint8_t j = 0 ; j < 6 ; j++ )
                {
                    if (Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].type== DEVICE_AC_HUB)
                    {
                        reals.preSoftVersion = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version;
                        ESP_LOGI(TAG,"https_ota_cmd_verify CCC--huangji---reals.preSoftVersion:%lu",reals.preSoftVersion);
                        break;
                    }
                }
                break;
            }
        }
    }
      
	// else if ((ota_obj.type == DEVICE_BMS)||(ota_obj.type == DEVICE_PACK_BCU)||(ota_obj.type == DEVICE_PACK_BMU)||(ota_obj.type == DEVICE_PACK_BMS)||(ota_obj.type == DEVICE_PACK_SAFTY))
    else if (ota_obj.type == DEVICE_BMS)// 电池包无论内置外置升级类型都是3，远程升级时IOT侧无法区分，现逻辑为全广播、IOT不做区分直接下发，底层会靠料号区分
	{
        ESP_LOGW(TAG,"000---");
        for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
        {
            for(int j = 0; j < PACK_MAX_NUM; j++)
            {
                // if((Inv_can[node_id].pack_data[0].pack_announce.online & (1 << j)) == 0)
                // 电池包固件的透传依靠逆变，若要判断底层不在线应该把逆变的在线状态也考虑
                // 内置BMS升完后 准备升外置BMS时，会导致所有电池包重新上电，这个时候收到升级命令会误判认为没有电池包在线
                if(((Inv_can[node_id].pack_data[0].pack_announce.online & (1 << j)) == 0) && (reals.online_Inv_num == 0))
                {
                    ESP_LOGW(TAG,"111--- no pack online can_upgrade = false, break");
                    break;
                }

                for(int i = 0; i < Inv_can[node_id].pack_data[0].pack_about.software_total; i++)
                {
                    if (ota_obj.type == Inv_can[node_id].pack_data[0].pack_about.soft[i].type)// 外置电池包底层靠料号区分
                    {
                        if((ota_obj.version/100)==(Inv_can[node_id].pack_data[0].pack_about.soft[i].version/100))
                        {
                            // can_upgrade = check_version_and_model(ota_obj, Inv_can[node_id].pack_data[j].pack_about.soft[i].version, Inv_can[node_id].pack_data[j].pack_about.type_ascii, Inv_can[node_id].pack_data[j].pack_about.sn_code, dev_model_ascii);
                            ESP_LOGW(TAG,"222 Inv_can[%d].pack_data[%d].pack_about.soft[%d].version=%lu",node_id,j,i,Inv_can[node_id].pack_data[j].pack_about.soft[i].version);
                            // can_upgrade = check_version_and_model(ota_obj, Inv_can[node_id].pack_data[j].pack_about.soft[i].version, SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn, dev_model_ascii);
                            reals.preSoftVersion =  Inv_can[node_id].pack_data[0].pack_about.soft[i].version;
                            ESP_LOGI(TAG,"https_ota_cmd_verify DDD--huangji---reals.preSoftVersion:%lu",reals.preSoftVersion);
                            can_upgrade = true;// 只要检测到升级类型是3，且有电池包在线，就直接下发广播
                            break;
                        }
                    }
                }

                if(can_upgrade == true)
                {
                    ota_config->dev_target_id = j;
                    ESP_LOGW(TAG,"333--- can_upgrade = true, break");
                    break;
                }
                            
            }

            for(int i=0;i<DEFAULT_PACK_TYPE_NUM;i++)
            {
                ESP_LOGI(TAG,"6100 VERSION:%lu",Inv_Pack_Slave[i].mod_reg06100_Pack_each.soft[0].version);
                if ((Inv_Pack_Slave[i].mod_reg06100_Pack_each.soft[0].version/100)==(ota_obj.version/100))
                {
                        ESP_LOGI(TAG,"333 DEVICE_PACK version:%lu -%lu",Inv_Pack_Slave[i].mod_reg06100_Pack_each.soft[0].version,ota_obj.version);
                        reals.preSoftVersion =Inv_Pack_Slave[i].mod_reg06100_Pack_each.soft[0].version;
                        ESP_LOGI(TAG,"https_ota_cmd_verify EEE--huangji---reals.preSoftVersion:%lu",reals.preSoftVersion);
                        can_upgrade = true;// 只要检测到升级类型是3，且有电池包在线，就直接下发广播
                        break;
                }
            }
            
            if(can_upgrade == true)
            {
                ota_data.cmd.target.bit.group = GROUP_PACK;
                ota_config->dev_can_id = PACK_CAN_ADDR + ota_config->dev_target_id;// AC380是全广播升级，此处不用管给具体哪一个地址升级
                ESP_LOGW(TAG,"444--- can_upgrade = true, break");
                break;
            }
        }
    }
	else 
	{
        can_upgrade = false;
    }

    if (can_upgrade) 
	{  
        memcpy(ota_config->dev_model_ascii, dev_model_ascii, 128);
        ota_data.cmd.type = ota_obj.type;
        ota_data.cmd.version = ota_obj.version;
        ota_data.cmd.target.bit.target = 0xFF;
    }

    free(dev_model_ascii);
    return can_upgrade ? 0 : -1;
}
int find_first_nonzero_bit(uint16_t value) {
    for (int i = 0; i < 16; i++) {
        if (value & (1 << i)) {
            return i;
        }
    }
    return -1; // 如果没有非零位，返回 -1
}

/**
 * @brief 向版本列表中添加一个版本。
 *
 * 此函数向给定的版本列表中添加一个版本。如果版本列表已满，会记录一条错误消息并返回。如果版本号为0，函数将直接返回而不添加到列表中。
 *
 * @param ver_list 指向版本列表的指针。
 * @param ver_num 指向当前版本数量的指针。
 * @param type 版本的类型。
 * @param version 要添加的版本号。
 * @param max_num 列表中允许的最大版本数量。
 */
static void add_ver_list(ver_data_t *ver_list, uint8_t *ver_num, uint8_t type, uint32_t version, uint8_t max_num) {
    if (*ver_num >= max_num) {
        ESP_LOGE(TAG, "ver_list is full");
        return;
    }
    if(version == 0){
        return;
    }
    ESP_LOGI(TAG,"add ver type:%d ver:%lu",type,version);
    ver_list[*ver_num].type = type;
    ver_list[*ver_num].version = version;
    (*ver_num)++;
    return;
}

EXT_RAM_BSS_ATTR bool https_new_image_timeout_clean_flag = false; 

static void https_ota_task(void *pvParameters) 
{
    // 跳过证书验证，
    // 选中 menuconfig->ESP-TLS->Allow potentially insecure options
    // 选中 menuconfig->ESP-TLS->Skip server certificate verification by default (WARNING: ONLY FOR TESTING PURPOSE, READ HELP)
    
    ota_object_t ota_object;
    ota_object_t invalid_ota_object;
    http_ota_config_t ota_config = {
        // .iot_to_mesh = false,
        .ota_esp_self = false,
        .to_mesh = false,
        .bulk_flash_erase = true, // 按照块擦除
        .partial_http_download = false, // 文件分片下载
        .image_flash_offset = IMAGE_FLASH_AREA_ADDRESS,
        .image_flash_size   = IMAGE_FLASH_AREA_MAX_LEN,
        .image_flags_offset = IMAGE_HEAD_AREA_ADDRESS,
        .image_flags_size   = IMAGE_HEAD_AREA_MAX_LEN,
    };
    
    while (1) 
	{   
        vTaskDelay(pdMS_TO_TICKS(100)); 
        if (sys_is_updating()) 
        {
            /*检查后台是否给我们推送新的版本信息*/
            if (http_ota_have_new_image(&invalid_ota_object) >= 0)
            {
                /*升级过程中无效指令，不接受*/
                https_ota_state_to_host(invalid_ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "system is updating");
                ESP_LOGE(TAG, "http_ota : system is already updating!");
            }
            continue ;
        }   
        
        if (http_ota_have_new_image(&ota_object) < 0) 
		{
            // need_check_firmware_update();
            /*仅在连接服务器（即网络可用）时运行*/
            //ESP_LOGI(TAG,"OTA TASK MQTT:%d",g_self_data.mod_reg11000_IOT_info.link.bit.mqtt);
		    if ( g_self_data.mod_reg11000_IOT_info.link.bit.mqtt )
            {      
                //ESP_LOGI(TAG,"need_new_version_flag:0x%x",reals.need_new_version_flag.Byte2);
                /*升级延时结束*/
                if (!https_new_image_timeout_clean_flag )
                {
                    if (iot_mqtt_is_period_reporting()) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                    } else {
                    switch ( find_first_nonzero_bit(reals.need_new_version_flag.Byte2) )
                    {
                        case HTTPS_CHECK_IOT_IMAGE : 
                            if(0 == https_new_image_check(HTTPS_CHECK_IOT_IMAGE))reals.need_new_version_flag.sBit.iot = 0; 
                            vTaskDelay(pdMS_TO_TICKS(1000)); break;
                        case HTTPS_CHECK_INV_IMAGE : 
                            if(0 == https_new_image_check(HTTPS_CHECK_INV_IMAGE))reals.need_new_version_flag.sBit.inv = 0; 
                            vTaskDelay(pdMS_TO_TICKS(1000)); break;
                        case HTTPS_CHECK_PACK_IMAGE : 
                            if(0 == https_new_image_check(HTTPS_CHECK_PACK_IMAGE))reals.need_new_version_flag.sBit.pack = 0;
                            vTaskDelay(pdMS_TO_TICKS(1000)); break;
                        case HTTPS_CHECK_DCHUB_IMAGE : 
                            if(0 == https_new_image_check(HTTPS_CHECK_DCHUB_IMAGE))reals.need_new_version_flag.sBit.dchub = 0;
                            vTaskDelay(pdMS_TO_TICKS(1000)); break;
                        case HTTPS_CHECK_ACHUB_IMAGE : 
                            if(0 == https_new_image_check(HTTPS_CHECK_ACHUB_IMAGE))reals.need_new_version_flag.sBit.achub = 0;
                            vTaskDelay(pdMS_TO_TICKS(1000)); break;
                        case HTTPS_CHECK_D400S_IMAGE : 
                            if(0 == https_new_image_check(HTTPS_CHECK_D400S_IMAGE))reals.need_new_version_flag.sBit.d400s = 0;
                            vTaskDelay(pdMS_TO_TICKS(1000)); break;
                    
                        default: reals.need_new_version_flag.Byte2 = 0; break;
                    }
                    }
                }

                if(is_update_check_triggered == true) 
                {
                    ESP_LOGI(TAG, "checking new image...");
                    is_update_check_triggered = false;
                    // 检查升级列表
                    check_firmware_updates();

                    // 通知webserver前端显示.
                    web_server_version_report();
                }
		    }

            continue ; 
            //vTaskDelay(pdMS_TO_TICKS(1000));
            //continue ; // 检查后台是否给我们推送新的版本信息
        }

        if (need_query_recordId == true)
        {
            need_query_recordId = false;

            if (check_firmware_version(ota_object.version, g_self_data.mod_reg11000_IOT_info.software_ver, ota_object.force))
            {
                uint32_t old_version = g_self_data.mod_reg11000_IOT_info.software_ver;

                // 获取升级记录ID
                memset(image_record_id, 0, 32);
                if (https_ota_get_record_id(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ING, "", 
                                            ota_object.type, ota_object.version, old_version) == 0)
                {
                    if (strlen(image_record_id) == 0)
                    {
                        ESP_LOGW(TAG, "Lost record! Continue upgrade");
                    }
                    else
                    {
                        memcpy(ota_object.recordId, image_record_id, 32);
                        ota_object.method = OTA_METHOD_WEBSERVER;   // 升级触发来源为webserver
                    }
                }
            }
        }

        ESP_LOGI(TAG, "https ota have new image...");

        if (https_ota_cmd_verify(ota_object, &ota_config) < 0) 
		{
            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_IMAGE_ERROR, "invalid image");
            set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 3, 0, OTA_ERR_FILE); // 升级进度填充到modbus中
            if (ota_object.method == OTA_METHOD_WEBSERVER)
            {
                web_server_upgrade_state_report(UPGRADE_STATE_FAIL, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue ; // 新固件机型，SN，版本等检查
        }
        else
        {
#ifdef FUNC_ONECLICK_UPGRADE_EN
            if (!ota_config.ota_esp_self) {
                Can_OneClickOta_Start_Wifi(&can_ota_status[0]);
            }
#endif
            set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 1, 3, 0, OTA_ERR_NORMAL); // 升级进度填充到modbus中
            if (ota_object.method == OTA_METHOD_WEBSERVER)
            {
                web_server_upgrade_state_report(UPGRADE_STATE_DOING, 0);
            }
        }
        
        if(ota_config.to_mesh) 
        {
            ESP_LOGI(TAG, "firmware is to micro inverter");
            continue ; // 升级微逆先清空进度条，没有标记系统正在升级，交由微逆自己管理
        }
        esp_http_client_config_t http_config = { // 配置https登录服务器信息
            .method = HTTP_METHOD_GET, // get请求，从https服务器下载文件
            .url = ota_object.url,
            .keep_alive_enable = true,
            .buffer_size = 2048,
        };

        ota_config.http_config = &http_config;

        http_ota_doing = true;  // https ota启动
        https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ING, "");
        set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 1, 3, 0, OTA_ERR_NORMAL); // 升级进度填充到modbus中
        if (ota_object.method == OTA_METHOD_WEBSERVER)
        {
            web_server_upgrade_state_report(UPGRADE_STATE_DOING, 0);
        }
        // set_mqtt_stop();
        //vTaskDelay(pdMS_TO_TICKS(2000));
        //  set_mqtt_stop();
        uint32_t len = 0;
        esp_err_t err = https_ota_handle_func(&ota_object, &ota_config, &len);
        ESP_LOGI(TAG,"https_ota_task err:%d ",err);
        if ( err == ESP_OK ) 
		{
            vTaskDelay(pdMS_TO_TICKS(200));
            ESP_LOGI(TAG, "http ota task reals.online_Iot_num:%d,ota_esp_self:%d", reals.online_Iot_num,ota_config.ota_esp_self);
            //升级IoT，并且单机时，直接升级
            if (ota_config.ota_esp_self == true && reals.online_Iot_num <= 1) 
			{ 
                http_ota_doing = false;
                ota_config.ota_esp_self = false; // IOT-自己升级
                memcpy(pimage_head->recordId, ota_object.recordId, 32);
                https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_SUCCESS, "");
                set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 100, OTA_ERR_NORMAL); // 升级进度填充到modbus中
                if (ota_object.method == OTA_METHOD_WEBSERVER)
                {
                    web_server_upgrade_state_report(UPGRADE_STATE_SUCCESS, 100);
                }

                ESP_LOGI(TAG, "\r\n\r\n Delay 5s Prepare to restart system....");
                if(reals.online_ACHUB_num)
                {
                    reals.iot_ota_flag=1;//并机需要上报21000段升级结果后断开服务器连接
                }else{
                    reals.iot_ota_end_count=1;//单机延时断开服务器连接
                }              

                vTaskDelay(pdMS_TO_TICKS(5000));
//                set_tcp_restart();
                set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 2, 100, OTA_ERR_NORMAL); // 这里没有第三段,模拟出第3段
                vTaskDelay(pdMS_TO_TICKS(2000)); 
                esp_restart(); 
                while(1);
            } 
			else 
			{
                uint32_t flash_addr = 0;
                if (ota_config.ota_esp_self == false)
                {
                    flash_addr = IMAGE_FLASH_AREA_ADDRESS;
                }
               
                ESP_LOGI(TAG, "flash addr: %lu, len: %lu", flash_addr, ota_object.total_bytes);
                if (https_ota_crc_verify(flash_addr, ota_object.total_bytes) != ESP_OK) {
                    set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 0, OTA_ERR_CRC32); // 升级进度填充到modbus中
                    https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "image crc32 verify failed");
                    http_ota_doing = false;
                    continue ;
                }
            }

            ota_data.cmd.kbytes = (ota_object.total_bytes + 1023) / 1024;
            ESP_LOGI(TAG," ota_data.cmd.kbytes:%u", ota_data.cmd.kbytes);
            
//            if (http_ota_flags_update(ota_object.recordId, ota_object.type, ota_object.version, ota_config.dev_target_id, ota_data.cmd.kbytes, ota_config.to_mesh) != 0) {
//                https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "write image state flags failed");
//                set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 0, OTA_ERR_FLASH_WRITE); // 升级进度填充到modbus中
//                http_ota_doing = false;
//                continue ;
//            }
            ESP_LOGI(TAG,"https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_OK, "");");
            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_OK, "");
            vTaskDelay(pdMS_TO_TICKS(200));
            
            switch (ota_object.type) // 本机升级
            {
                case DEVICE_ARM: 
	                if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.group & 0xFF), len) != 0) {
	                    ESP_LOGE(TAG, "OTA Init fail");
	                }	                
                    if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                    {
	                    ESP_LOGE(TAG, "OTA Begin fail");
	                }
	                break;
                case DEVICE_DSP: 
                //case PACK_SAFT:
	                if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.group & 0xFF), len) != 0) {
	                    ESP_LOGE(TAG, "OTA Init fail");
	                }
	                if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                    {
	                    ESP_LOGE(TAG, "OTA Begin fail");
	                    // xresp_value = XMODEM_CAN;
	                    // ota_status_err_set();
	                }
	                break;
                case DEVICE_DC_HUB:
                    if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.group & 0xFF), len) != 0) {
	                    ESP_LOGE(TAG, "OTA Init fail");
	                }
	                if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                    {
	                    ESP_LOGE(TAG, "OTA Begin fail");
	                }
	                break;
                case DEVICE_AC_HUB:
                    if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.group & 0xFF), len) != 0) {
	                    ESP_LOGE(TAG, "OTA Init fail");
	                }
	                if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                    {
	                    ESP_LOGE(TAG, "OTA Begin fail");
	                }
	                break;
                // case DEVICE_RF: 
                    
                //     ESP_LOGI(TAG, "Ready xmodem OTA"); 
//                    ota_config.ota_buffer = can_ota_buffer_addr_get();
//                
//                    if(iot_image_read(IMAGE_FLASH_AREA_ADDRESS, ota_config.ota_buffer, ota_data.cmd.kbytes * 1024) != ESP_OK)
//                    {
//                        set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 0, OTA_ERR_FLASH_READ); // 升级进度填充到modbus中
//                        ota_status_err_set();
//                        break;
//                    }
                    
//                	ota_set_t ota_set;
//        			ota_set.file_type = ota_data.cmd.type;
//        			ota_set.ver_low = ota_data.cmd.version & 0xffff;
//        			ota_set.ver_high = (uint32_t)ota_data.cmd.version >> 16;
//        			ota_set.file_size = ota_data.cmd.kbytes;
//        			ota_set.group.dev_type = (ota_data.cmd.target.bit.group) & 0xFF;
//
//                    if(ota_set.group.dev_type == GROUP_IOT)
//                    {
//                        ota_set.start_flag = 1; //升级边界路由
//                    }
//                    else if(ota_set.group.dev_type == GROUP_INV)
//                    {
//                        ota_set.start_flag = 3; //升级子设备
//                    }
//                    else
//                    {
//                        ota_set.start_flag = 3; //默认 
//                    }
//                    
//        			ota_set.group.dev_id = ota_data.cmd.target.bit.group & 0xFF;
//                    ESP_LOGW(TAG, "Group[%d : %d] type=%u, version=%lu, size=%u",
//        			ota_set.group.dev_type, ota_set.group.dev_id,
//
//        			ota_set.file_type,
//        			ota_object.version,
//        			ota_set.file_size);
//                    
//        			if (cc1312_ota_start(&ota_set, NULL,  NULL) != 0) {
//        				set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 0, OTA_ERR_SIZE); // 升级进度填充到modbus中
//        				ota_status_err_set();
//        			}
//                    else
//                    {
//                        https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_UPGRADING, "");
//                    }
//                    break;                 
                case DEVICE_BMS: 
	                if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.group & 0xFF), len) != 0) {
	                    ESP_LOGE(TAG, "OTA Init fail");
	                }
	                if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                    {
	                    ESP_LOGE(TAG, "OTA Begin fail");
	                }
	                break;
                case DEVICE_PACK_BMS:
                    if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.group & 0xFF), len) != 0) {
	                    ESP_LOGE(TAG, "OTA Init fail");
	                }
	                if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                    {
	                    ESP_LOGE(TAG, "OTA Begin fail");
	                }
	                break;
                case DEVICE_IOT:
                {
                    ESP_LOGI(TAG, "Ready IOT OTA"); 

                    //升级IoT，并且存在并机
                    if (ota_config.ota_esp_self == true)
                    {
                        if (reals.online_Iot_num > 1)
                        {
                            ESP_LOGI(TAG, "OTA ESP SELF");

                            if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes,
                                                    (ota_data.cmd.target.bit.group & 0xFF), len) != 0)
                            {
                                ESP_LOGE(TAG, "OTA Init fail");
                            }

                            if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                            {
                                ESP_LOGE(TAG, "OTA Begin fail");
                            }
                
                            //gXmodem_Status.firmware_resend_step=1;
                            ESP_LOGI(TAG, "start http_iot_can_ota");
                            break;
                        }
                    }
                    //D400S IOT升级
                    else if(ota_data.cmd.target.bit.group == GROUP_CHARGE)
                    {
                        ESP_LOGI(TAG,"d400s type:%u ,version:%lu",ota_data.cmd.type,ota_data.cmd.version);
                        if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.group & 0xFF), len) != 0) {
                            ESP_LOGE(TAG, "OTA Init fail");
                        }
                        if (can_ota_begin(CAN_BROADCAST_ADDRESS, ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                        {
                            ESP_LOGE(TAG, "OTA Begin fail");
                        }
                    }
                    //IOT 升级S1智能插座，使用wifi mesh
                    else
                    {
                        ota_set_t ota_set;
                        ota_set.start_flag = 1;
                        ota_set.file_type = ota_data.cmd.type;
                        ota_set.ver_low = ota_data.cmd.version & 0xffff;
                        ota_set.ver_high = (uint32_t)ota_data.cmd.version >> 16;
                        ota_set.file_size = ota_data.cmd.kbytes;
                        ota_set.group.dev_type = (ota_data.cmd.target.bit.group) & 0xFF;
                        ota_set.group.dev_id = ota_data.cmd.target.bit.group & 0xFF;

                        ESP_LOGW(TAG, "Group[%d : %d] type=%u, version=%lu, size=%u",
                        ota_set.group.dev_type, ota_set.group.dev_id,
                        ota_set.file_type,
                        ota_object.version,
                        ota_set.file_size);

                        if(uart_ota_start(&ota_set, NULL, NULL, 1) != 0) 
                        {
                            set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 0, OTA_ERR_SIZE); // 升级进度填充到modbus中
                            iot_mqtt_start(NETIF_TYPE_WIFI_STA);
                            ota_status_err_set();
                        }
                        else
                        {
                            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_UPGRADING, "");
                        }
                    }
                }
                    break;
                default: 
                    ESP_LOGE(TAG, "Ready xmodem ERROR : pimage_head->type(%d)",ota_object.type); 
                    https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "not supported image type"); 
                    set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 0, OTA_ERR_FILE); // 升级进度填充到modbus中
                    break;
            }
        }
        else 
        {
            set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 3, 0, OTA_ERR_FILE_NULL); // 升级进度填充到modbus中
            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "download failed");
            if (ota_object.method == OTA_METHOD_WEBSERVER)
            {
                web_server_upgrade_state_report(UPGRADE_STATE_FAIL, 0);
            }
        }
        http_ota_doing = false; // https ota结束
    }
    vTaskDelete(NULL);
}

void https_ota_state_to_host(char recordId[32], char *state, char *reason) {

    char *post_data = json_ota_response((const char *)recordId, state, reason);
    if (post_data)
    {
        esp_http_client_config_t config = {0};
        if(!strcmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST)){
            config.url = DEV_ENV_URL""HTTPS_INTERFACE;
        } else if(!strcmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST)){
            config.url = TEST_ENV_URL""HTTPS_INTERFACE;
        } else if(!strcmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)){
            config.url = PRO_ENV_URL""HTTPS_INTERFACE;
        } else{
            ESP_LOGE(TAG, "UNKNOWN HOST: %s",  SetData.dev_info_t.could_dns);
            return;
        }
        ESP_LOGI(TAG,"config.url:%s",config.url);
        config.method = HTTP_METHOD_POST;
        config.disable_auto_redirect = true;
        //config.event_handler = https_event_handle;
        esp_http_client_handle_t client = esp_http_client_init(&config);
       
        // POST
       // ESP_LOGI(TAG,"",client.);
        // esp_http_client_set_method(client, HTTP_METHOD_POST);
      //  esp_http_client_set_header(client, "Content-Type", "application/json");
        //esp_http_client_set_header(client, "Authorization", "Bearer YOUR_ACCESS_TOKEN");
                // POST
        if(set_http_header(client) < 0){
            esp_http_client_cleanup(client);
            json_buffer_clear(post_data);
            return;
        }

        esp_http_client_set_post_field(client, post_data, strlen(post_data));
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
        json_buffer_clear(post_data);
    }
}

bool http_ota_is_doing(void) {
    return http_ota_doing;
}

void https_ota_cmd_parse(const char *pdata, uint16_t len) 
{
    if(sys_is_updating())// 升级过程中不接受其他json指令
    { 
        return;
    }
    
    if (ota_cmd_list == NULL) // 解析后台发送过来的json升级信息
    { 
        ESP_LOGI(__func__, "https_ota_cmd_parse"); 
        ota_cmd_list = json_ota_cmd_parse(pdata);

#if 0//DEVICE_MODEL == DCDC
       if(ota_cmd_list && 
            (//strncmp(ota_cmd_list->object.dev_model_ascii, MI_A80_DEV_TYPE, strlen(MI_A80_DEV_TYPE)) == 0 || 
            //strncmp(ota_cmd_list->object.dev_model_ascii, MI_A100_DEV_TYPE, strlen(MI_A100_DEV_TYPE)) == 0 ||
            strncmp(ota_cmd_list->object.dev_model_ascii, MI_A100S_DEV_TYPE, strlen(MI_A100S_DEV_TYPE)) == 0 ||
            strncmp(ota_cmd_list->object.dev_model_ascii, DTU_COMBOX_DEV_TYPE, strlen(DTU_COMBOX_DEV_TYPE)) == 0))
       {
//            uint8_t *mqtt_ptr = heap_caps_malloc(len + 20, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//            if (mqtt_ptr == NULL) 
//            {
//                ESP_LOGE(__func__, "Failed to allocate memory for mqtt_ptr");
//                return;
//            }
//            memset(mqtt_ptr, 0, len + 20);
//            mqtt_ptr[0] = 0x00;
//            mqtt_ptr[1] = 0x09;
//            mqtt_ptr[2] = len >> 8;
//            mqtt_ptr[3] = len & 0xFF;
//            memcpy(mqtt_ptr + 4, pdata, len);
//            uint16_t mqtt_len = len + 4;
//            if(send_json_cmd_to_mesh(mqtt_ptr, mqtt_len, MODBUS_MI_ADDR_31))
//			  {
//                ESP_LOGI(__func__, "send json cmd to mesh success");
//            } // 收到升级指令同步发送一次指令到微逆
//            free(mqtt_ptr);
            ota_cmd_list_t *list_head = ota_cmd_list; // 判断后台是否返回新的固件信息
            ota_cmd_list = list_head->next; //从第二帧开始解析
            free(list_head);
       }
#endif
    } 
    else 
    {
        ESP_LOGW(__func__, "have image updating process, new json miss!!!");
    }
}

void create_https_task(void) {

    pimage_head = get_image_info();

    if (xTaskCreatePinnedToCore(https_ota_task, "http ota task",  1024*6, NULL, 3, NULL, 1) != pdPASS)
	{
        ESP_LOGE(TAG, "https_ota_task create failed"); 
		abort();
    }
	else
	{
		ESP_LOGI(TAG, "https_ota_task create Success");
	}	
}

#if 0
bool https_disater_state_to_host(disaster_send_param_t StormSendParam, int8_t send_type, char *id)
{
    if(id)
    {ESP_LOGI(TAG,"* id char:%s send_type:%d",id,send_type);}
    if((0==send_type)&&id){
        strncpy( SetData.dev_info_t.disaster_Warm_Msg_Id, id, sizeof( SetData.dev_info_t.disaster_Warm_Msg_Id));
        reals.SetDataWrFlag.sBit.SetDataUpdate_all=1;
    }

    esp_err_t err = -1;
    char Sn_Str[32] = {0};
    char send_dev_type[12];
    uint64_t send_dev_sn;
    char *post_data = NULL;
    ESP_LOGI(TAG,"msg_id:%s send_type:%d", SetData.dev_info_t.disaster_Warm_Msg_Id,send_type);
    // ESP_LOGW(TAG,"StormSendParam.uuid:%d",StormSendParam.uuid);
    if(reals.online_ACHUB_num==1)//接入achub，使用第二代通讯板SN
    {
        strncpy(send_dev_type,login_info.iot_type,sizeof(send_dev_type));
        send_dev_sn=login_info.iot_sn;
    }else //否则单机使用第一代设备SN
    {
        strncpy(send_dev_type,login_info.dev_type,sizeof(send_dev_type));
        send_dev_sn=login_info.dev_sn;
    }

    sprintf(Sn_Str, "%s%llu", send_dev_type, (unsigned long long)send_dev_sn); 
    if(send_type == 2){
        ESP_LOGW(TAG,"END");
        post_data = json_disaster_response((const char *)Sn_Str, StormSendParam.BackupStartTime, StormSendParam.BackupEndTime, (StormSendParam.total_chg_energy_end - StormSendParam.total_chg_energy_begin), (StormSendParam.soc_end - StormSendParam.soc_start), StormSendParam.uuid,send_dev_type, SetData.dev_info_t.disaster_Warm_Msg_Id);
    }
    // else if(send_type = 1){
    
    else if(send_type == 1){
        ESP_LOGW(TAG,"start");
        post_data = json_disaster_start((const char *)Sn_Str, StormSendParam.BackupStartTime, StormSendParam.uuid,send_dev_type,  SetData.dev_info_t.disaster_Warm_Msg_Id);
    }
    else {
        return false;
        //ESP_LOGW(TAG,"RECV");
        //post_data = json_disaster_rcv_response((const char *)Sn_Str, send_type, StormSendParam.uuid, (const char *) id,send_dev_type);
    }
    // else {
    //     return false;
    // }
    esp_http_client_config_t config = {0};
    if (!strcmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST) ) {
        config.url = DEV_ENV_URL""HTTPS_DISASTER;
    } else if (!strcmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST)) {
        config.url = TEST_ENV_URL""HTTPS_DISASTER;
    } else if (!strcmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
        config.url = PRO_ENV_URL""HTTPS_DISASTER;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST: %s", SetData.dev_info_t.could_dns);
        json_buffer_clear(post_data);
        return false;
    }


    // ESP_LOGW(TAG, "url: %s", config.url);
    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.timeout_ms = 5000;
    config.event_handler = https_event_handle;//https_event_handle; 

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        json_buffer_clear(post_data);
        return false;
    }
    //   err = esp_http_client_open(client, 0);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    //     // goto cleanup;
    // }
    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        esp_http_client_cleanup(client);
        json_buffer_clear(post_data);
        return false;
    }


    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    ESP_LOGI(TAG,"post_data:%s",post_data);
    err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        err = _http_handle_response_code(client, status_code);
        if (err == ESP_OK) {

            #if(0)
            char *rsp = NULL;
            rsp = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            memset(rsp, 0, 1024);
            int read_len = esp_http_client_read_response(client, rsp, 1024 - 1);
            // if (read_len < 0) {
            // ESP_LOGE(TAG, "Error reading data");
            ESP_LOGE(TAG, "Error reading data: %d", read_len);
            // return -1; // 返回错误码
       		 // }
            ESP_LOGE(TAG, "read_http_response len:%d", read_len);
            // json_parse_utc_ip(rsp, time);
            #endif
            char *rsp = (char *)heap_caps_malloc(256, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            // memset(rsp, 0, 1024);
            if (rsp == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
                return false;;
            }
            else{
                memset(rsp, 0, 256);
                memcpy(rsp, response_buffer, RESPONSE_BUFFER_SIZE-1);
                rsp[RESPONSE_BUFFER_SIZE-1] = '\0'; // Null-terminate the response
            }

            err = parse_json_response(rsp);
            free(rsp);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to parse JSON response: %d", err);
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    // if (client) {
        // esp_http_client_close(client);
        esp_http_client_cleanup(client);
    // }
    free(post_data);
    return (err == ESP_OK);
}
#else

bool https_disater_state_to_host(const disaster_post_args_t *args)
{
    char id[33] = {0};
    if(SetData.dev_info_t.disaster_Warm_Msg_Id[0] == '\0')  // 正确
    {
        ESP_LOGI(TAG,"* StormSendParam id NULL  char:%s send_type:%d msgid:%s",id,args->send_type,args->StormSendParam.msg_id);
    }
    else{
        memcpy(id, &SetData.dev_info_t.disaster_Warm_Msg_Id[0], 32);
        id[32] = '\0';
    }

    esp_err_t err = -1;
    char Sn_Str[32] = {0};
    char send_dev_type[13] = {0};
    uint64_t send_dev_sn;
    char *post_data = NULL;
    ESP_LOGI(TAG,"msg_id:%s send_type:%d,msg_id:%s", SetData.dev_info_t.disaster_Warm_Msg_Id,args->send_type,args->StormSendParam.msg_id );
    // ESP_LOGW(TAG,"StormSendParam.uuid:%d",StormSendParam.uuid);
    if(reals.online_ACHUB_num==1)//接入achub，使用第二代通讯板SN
    {
        memcpy(send_dev_type, login_info.iot_type, 12);
        send_dev_type[12] = '\0';
        send_dev_sn=login_info.iot_sn;
    }else //否则单机使用第一代设备SN
    {
        memcpy(send_dev_type, login_info.dev_type, 12);
        send_dev_type[12] = '\0';
        send_dev_sn=login_info.dev_sn;
    }
    ESP_LOGI(TAG,"send_start_param soc  begin:%d  end:%d cur:%d", args->StormSendParam.soc_start,args->StormSendParam.soc_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc);
    ESP_LOGI(TAG,"send_start_param total_chg_energy  begin:%lu  end:%lu cur:%lu",args->StormSendParam.total_chg_energy_begin,args->StormSendParam.total_chg_energy_end,Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy);
    sprintf(Sn_Str, "%s%llu", send_dev_type, (unsigned long long)send_dev_sn); // 第二代
    if (args->send_type == 2) {
        ESP_LOGI(TAG, "end:%d", args->end_reason.all);
        // 你可以在这里把 end_reason 也传到 json_disaster_response，如果需要
        post_data = json_disaster_response(
            (const char *)Sn_Str,
            args->StormSendParam.BackupStartTime,
            args->StormSendParam.BackupEndTime,
            (args->StormSendParam.total_chg_energy_end - args->StormSendParam.total_chg_energy_begin),
            (args->StormSendParam.soc_end - args->StormSendParam.soc_start),
            args->StormSendParam.uuid,
            send_dev_type,
            id,
            args->end_reason.all 
        );
    } else if (args->send_type == 1) {
        ESP_LOGI(TAG, "start");
        post_data = json_disaster_start(
            (const char *)Sn_Str,
            args->StormSendParam.BackupStartTime,
            args->StormSendParam.uuid,
            send_dev_type,
            args->StormSendParam.msg_id
        );
    } else {
        ESP_LOGI(TAG, "recv");
        return false;
    }
    esp_http_client_config_t config = {0};
    if (!strcmp(g_self_data.mod_reg12000_IOT_set.could_dns, DEV_ENV_HOST) ||
        !strcmp(g_self_data.mod_reg12000_IOT_set.could_dns, DEV_ENV_HOST_LAST)) {
        config.url = DEV_ENV_URL""HTTPS_DISASTER;
    } else if (!strcmp(g_self_data.mod_reg12000_IOT_set.could_dns, TEST_ENV_HOST)) {
        config.url = TEST_ENV_URL""HTTPS_DISASTER;
    } else if (!strcmp(g_self_data.mod_reg12000_IOT_set.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
        config.url = PRO_ENV_URL""HTTPS_DISASTER;
    } else {
        config.url = PRO_ENV_URL""HTTPS_DISASTER;
        ESP_LOGE(TAG, "UNKNOWN HOST: %s",  g_self_data.mod_reg12000_IOT_set.could_dns);
        json_buffer_clear(post_data);
        return false;
    }


    // ESP_LOGW(TAG, "url: %s", config.url);
    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.timeout_ms = 5000;
    config.event_handler = https_event_handle;//https_event_handle; 

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (!client) {
        ESP_LOGE(TAG, "client is NULL");
        goto cleanup;
    }

    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto cleanup;
    }


    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        err = _http_handle_response_code(client, status_code);
        if (err == ESP_OK) {

            #if(0)
            char *rsp = NULL;
            rsp = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            memset(rsp, 0, 1024);
            int read_len = esp_http_client_read_response(client, rsp, 1024 - 1);
            // if (read_len < 0) {
            // ESP_LOGE(TAG, "Error reading data");
            ESP_LOGE(TAG, "Error reading data: %d", read_len);
            // return -1; // 返回错误码
       		 // }
            ESP_LOGE(TAG, "read_http_response len:%d", read_len);
            // json_parse_utc_ip(rsp, time);
            #endif
            char *rsp = (char *)heap_caps_malloc(RESPONSE_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            // memset(rsp, 0, 1024);
            if (rsp == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
                return false;;
            }
            else{
                memset(rsp, 0, RESPONSE_BUFFER_SIZE);
                memcpy(rsp, response_buffer, RESPONSE_BUFFER_SIZE-1);
                rsp[RESPONSE_BUFFER_SIZE-1] = '\0'; // Null-terminate the response
            }

            err = parse_json_response(rsp);
            free(rsp);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to parse JSON response: %d", err);
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

cleanup:
    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (post_data) {
        free(post_data);
    }

    return (err == ESP_OK);
}

#endif

// 通用辅助函数：检查字符串是否为数字并比对范围
static bool is_valid_number(const char *str, int min, int max) {
    for (const char *p = str; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) {
            ESP_LOGE(TAG, "Invalid character in number: %c", *p);
            return false;
        }
    }
    int num = atoi(str);
    if (num < min || num > max) {
        ESP_LOGE(TAG, "Number %d is not in range %d-%d", num, min, max);
        return false;
    }
    return true;
}

inline static bool is_valid_port(const char *port_str) {
    return is_valid_number(port_str, 0, 65535);
}

inline static bool is_valid_ip_segment(const char *segment) {
    return is_valid_number(segment, 0, 255);
}

#if HTTP_UTC_TIME_VERSION == 2
/**
 * @brief 检查服务器返回的 IP 地址和端口号是否有效。
 *
 * 此函数验证提供的 IP 地址和端口号的有效性，格式为 "xx.xx.xx.xx:xxx"。
 * 它首先检查输入字符串是否为 NULL，然后分离 IP 地址和端口号。
 * 该函数验证端口号和 IP 地址的每个段，以确保它们在合理范围内。
 *
 * @param ip 格式为 "x.x.x.x:x" 的 IP 地址和端口号。
 * @return 如果 IP 地址和端口号有效，则返回 true，否则返回 false。
 */
static bool is_valid_ip(const char *ip) {
    if (ip == NULL) {
        return false;
    }

    char ip_copy[64];
    strncpy(ip_copy, ip, sizeof(ip_copy) - 1);
    ip_copy[sizeof(ip_copy) - 1] = '\0';

    // 检查是否有端口号
    char *colon_pos = strchr(ip_copy, ':');
    if (colon_pos != NULL) {
        *colon_pos = '\0'; // 将冒号替换为字符串结束符
        colon_pos++;
        if (!is_valid_port(colon_pos)) {
            return false;
        }
    } else {
        // ESP_LOGW(TAG, "No port number found");
        return false;
    }

    // 分割IP地址
    char *segment;
    int segment_count = 0;

    segment = strtok(ip_copy, ".");
    while (segment) {
        if (!is_valid_ip_segment(segment)) {
            return false;
        }
        segment = strtok(NULL, ".");
        segment_count++;
    }

    // 检查是否有3个点
    if (segment_count != 4) {
        ESP_LOGE(TAG, "IP does not contain 3 dots");
        return false;
    }

    return true;
}
#else
/**
 * @brief 检查服务器返回的 IP 地址和端口号是否有效。
 *
 * 此函数验证提供的 IP 地址和端口号的有效性，格式为 "xx.xx.xx.xx:xxx"。
 * 它首先检查输入字符串是否为 NULL，然后分离 IP 地址和端口号。
 * 该函数验证端口号和 IP 地址的每个段，以确保它们在合理范围内。
 *
 * @param ip 格式为 "x.x.x.x:x" 的 IP 地址和端口号。
 * @return 如果 IP 地址和端口号有效，则返回 true，否则返回 false。
 */
static bool is_valid_ip(const char *ip) {
    if (ip == NULL) {
        return false;
    }

    char ip_copy[64];
    strncpy(ip_copy, ip, sizeof(ip_copy) - 1);
    ip_copy[sizeof(ip_copy) - 1] = '\0';

    // 检查是否有端口号
    char *colon_pos = strchr(ip_copy, ':');
    if (colon_pos != NULL) {
        *colon_pos = '\0'; // 将冒号替换为字符串结束符
        colon_pos++;
        if (!is_valid_port(colon_pos)) {
            return false;
        }
    } else {
        // ESP_LOGW(TAG, "No port number found");
        return false;
    }

    // 分割IP地址
    char *segment;
    int segment_count = 0;

    segment = strtok(ip_copy, ".");
    while (segment) {
        if (!is_valid_ip_segment(segment)) {
            return false;
        }
        segment = strtok(NULL, ".");
        segment_count++;
    }

    // 检查是否有3个点
    if (segment_count != 4) {
        ESP_LOGE(TAG, "IP does not contain 3 dots");
        return false;
    }

    return true;
}
#endif

static bool is_valid_host(const char *host) {
    if (host == NULL) {
        return false;
    }

    char host_copy[128];
    strncpy(host_copy, host, sizeof(host_copy) - 1);
    host_copy[sizeof(host_copy) - 1] = '\0';

    // 检查是否有端口号
    char *colon_pos = strchr(host_copy, ':');
    if (colon_pos != NULL) {
        *colon_pos = '\0';
        colon_pos++;
        if (!is_valid_port(colon_pos)) {
            return false;
        }
        return true;
    } else {
        // 没有端口号
        return false;
    }
}

/*------------------------------------------------------------------------------
 Function: is_valid_server
 -----------------------------------------------------------------------------*/
/**
  * @brief      选择域名解析方式
  * @param[in]  const char *ip  
  * @param[out] None
  * @return     static bool
  */
static bool is_valid_server(const char *ip) {
#if HTTP_UTC_TIME_VERSION == 2
    return is_valid_host(ip);
#else
    return is_valid_ip(ip);
#endif
}


// static int json_parse_utc_ip(const char *msg, uint64_t *time) {
//     /*示例  
//         {
//         "appId": null,
//         "msgId": "17c504a22e5f79115029cdbe2b4",
//         "msgCode": 0,
//         "message": "OK",
//         "data": "1701455925000",
//         "error": null,
//         "code": 0
//         } */  
//     cJSON *cjson_object = cJSON_Parse(msg);
//     if (cjson_object == NULL) {
//         ESP_LOGE(TAG, "json parse failed");
//         return -1;
//     }
// //    ESP_LOGW(TAG,"%s\n", msg);
//     const cJSON *cjson_data = cJSON_GetObjectItem(cjson_object, "data");
//     if (cjson_data == NULL || !cJSON_IsString(cjson_data)) {
//         ESP_LOGE(TAG, "json parse failed, without data or data is not a string");
//         cJSON_Delete(cjson_object);
//         return -1;
//     }

//     char *endptr;
//     uint64_t value = strtoll(cjson_data->valuestring, &endptr, 10);
//     if (*endptr != '\0') {
//         ESP_LOGE(TAG, "strtol failed or non-numeric characters found");
//         cJSON_Delete(cjson_object);
//         return -1;
//     }

//     *time = (value / 1000); // ms to s
//     ESP_LOGI(TAG, "utc data: %llu", *time);
//     cJSON_Delete(cjson_object);
//     return 0;
// }

/**
 * @brief 从给定的客户端读取HTTP响应并将其存储在提供的缓冲区中。
 *
 * 此函数从HTTP客户端获取头信息并检查内容长度是否有效。
 * 如果内容长度小于0，则记录错误并返回-1。
 * 如果内容长度为0，表示没有Content-Length头或分块传输编码，
 * 函数将从客户端读取响应数据并将其存储在缓冲区中，直到响应结束。
 * 如果在读取数据时发生错误，则记录错误并返回-1。
 * 如果内容长度大于0，表示存在Content-Length头，
 * 函数将从客户端读取响应数据并将其存储在缓冲区中，直到响应结束或总读取长度与内容长度匹配。
 * 如果总读取长度小于内容长度，表示读取不完整，
 * 记录警告并返回-1。
 *
 * @param client HTTP客户端的句柄。
 * @param buffer 存储HTTP响应的缓冲区。
 * @param max_len 缓冲区的最大长度。
 * @return 成功时返回读取的总字节数，失败时返回-1。
 */

#if 0       //服务器返回的数据太长会覆盖掉 -zdm

static int read_http_response(esp_http_client_handle_t client, char *buffer, uint16_t max_len) {
    int64_t content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch headers: %lld", content_length);
        return -1; // 返回错误码
    }
    int read_len;
    int64_t total_read = 0;
    if (content_length == 0) {
        // 没有 Content-Length 头或者使用了分块传输编码
        while ((read_len = esp_http_client_read(client, buffer, max_len - 1)) > 0) {
            buffer[read_len] = 0; // 确保缓冲区以 null 结尾
            total_read += read_len;
        }
        if (read_len < 0) {
            ESP_LOGE(TAG, "Error reading data: %d", read_len);
            return -1; // 返回错误码
        }
    } else {
        // 有 Content-Length 头
        while (total_read < content_length && (read_len = esp_http_client_read(client, buffer, max_len - 1)) > 0) {
            buffer[read_len] = 0; // 确保缓冲区以 null 结尾
            total_read += read_len;
        }
        if (total_read < content_length) {
            ESP_LOGW(TAG, "Incomplete read: expected %lld, read %lld", content_length, total_read);
            return -1; // 返回错误码
        }
    }
    return (int)total_read; // 返回成功码
}

#else

static int read_http_response(esp_http_client_handle_t client, char *buffer, uint16_t max_len) {
    int64_t content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch headers: %lld", content_length);
        return -1; // 返回错误码
    }
    int read_len;
    int64_t total_read = 0;
    if (content_length == 0) {
        // 没有 Content-Length 头或者使用了分块传输编码
        read_len = esp_http_client_read_response(client, buffer, max_len - 1);
        if (read_len < 0) {
            ESP_LOGE(TAG, "Error reading data: %d", read_len);
            return -1; // 返回错误码
        }
        total_read = read_len;
    } else {
        // 有 Content-Length 头
        read_len = esp_http_client_read_response(client, buffer, max_len - 1);
        if (read_len < 0) {
            ESP_LOGE(TAG, "Error reading data: %d", read_len);
            return -1; // 返回错误码
        }
        total_read = read_len;
        if (total_read < content_length) {
            ESP_LOGW(TAG, "Incomplete read: expected %lld, read %lld", content_length, total_read);
            return -1; // 返回错误码
        }
    }
    buffer[total_read] = 0; // 确保缓冲区以 null 结尾
    return (int)total_read; // 返回成功码
}

#endif




/**
 * @brief 从指定服务器获取当前 UTC 时间和 IP 地址。
 *
 * 此函数发送一个 HTTP GET 请求到服务器，以检索当前的 UTC 时间和 IP 地址。
 * 服务器 URL 是根据 `iot_info` 结构中设置的 DNS 主机确定的。
 * 该函数处理不同的环境（开发、测试、生产）并设置适当的 URL。
 *
 * @param server_ip 指向存储服务器 IP 地址的缓冲区的指针。
 * @param max_len 服务器 IP 地址缓冲区的最大长度。参考值128
 * @param time 指向存储检索到的 UTC 时间的变量的指针。
 * @return int 成功时返回 0，失败时返回 -1。
 *
 *
 * 注意：该函数包含处理时区的注释代码，目前未使用。
 */
int http_get_utc_time_and_ip(char *server_ip, uint16_t max_len, uint64_t *time) {
    int ret = -1;
    char *rsp = NULL;
    if(server_ip == NULL || time == NULL){
        ESP_LOGE(TAG, "ip or time buff is NULL");
        return -1;
    }
    esp_http_client_config_t config = {0}; 

    // 根据环境选择URL
    if (!strcmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST)) {  // 开发环境
        config.url = DEV_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST)) { // 测试环境
        config.url = TEST_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) { // 生产环境
        config.url = PRO_ENV_URL""HTTP_UTC_TIME;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST, use default host");
        config.url = PRO_ENV_URL""HTTP_UTC_TIME; // 默认生产
    }
    ESP_LOGI(TAG,"http_get_utc_time_and_ip%s", config.url);
    config.method = HTTP_METHOD_GET;
    config.disable_auto_redirect = false;
    config.buffer_size = 1024;
    config.timeout_ms = HTTPS_GET_UTC_IP_TIMEOUT; // 设置超时时间
    config.event_handler = https_event_handle;
    //config.if_name = &g_netif_req; // 设置网口

    if(http_event_handle == NULL){
        http_event_handle = xEventGroupCreate();
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "client is NULL");
        goto cleanup;
    }

    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto cleanup;
    }

    #if 0
    //仅供调试使用，出厂代码禁止使用
    /*
    DE: 93.193.194.180
    US: 98.97.176.254
    HK: 119.229.140.106
    SG: 102.182.138.85
    */

	// if (uart_data_wr.mod_reg40000_transparent.param_control.bit.IP_test)
	{
        const char *vitual_ip1 = "93.193.194.180";
        const char *vitual_ip2 = "98.97.176.254";
        const char *vitual_ip3 = "119.229.140.106";
        const char *vitual_ip4 = "102.182.138.85";
        char *vitual_ip;
		// switch (uart_data_wr.mod_reg40000_DCDC_set.param_control.bit.IP_test)
        switch(1)
			{
			case 1:
				vitual_ip = (char *)vitual_ip1;
				break;
			case 2:
				vitual_ip = (char *)vitual_ip2;
				break;
			case 3:
				vitual_ip = (char *)vitual_ip3;
				break;
			case 4:
				vitual_ip = (char *)vitual_ip4;
				break;
			default:
				vitual_ip = (char *)vitual_ip3;		
				break;
			
			}
	    if(strlen(vitual_ip) > 0) {
	        ESP_LOGI(TAG,"set x-client-ip:%s", vitual_ip);
	        esp_http_client_set_header(client,"x-client-ip", vitual_ip);
	    }
	}
    #endif

	    #if 0
    //仅供调试使用，出厂代码禁止使用
    /*
    DE: 93.193.194.180
    US: 98.97.176.254
    HK: 119.229.140.106
    SG: 102.182.138.85

    澳大利亚:1.41.111.112
日本:1.66.105.204
美国:164.153.51.98(纽约IP，实行夏令时)
美国:64.38.192.100(亚利桑那IP，不实行夏令时)
德国:2.203.176.110
    */
	//const char *vitual_ip =  "14.21.30.26"; //HK
	//const char *vitual_ip = "119.229.140.106";
    //const char *vitual_ip = "93.193.194.180";
    //const char *vitual_ip = "98.97.176.254";
    const char *vitual_ip = "2.203.176.110";  
    //const char *vitual_ip = " 164.153.51.98";
    //const char *vitual_ip = " 13.76.45.134";
    //const char *vitual_ip = " 1.66.105.204";
    
    if(strlen(vitual_ip) > 0) {
        ESP_LOGI(TAG,"set x-client-ip:%s", vitual_ip);
        esp_http_client_set_header(client,"x-client-ip", vitual_ip);
    }

    #endif
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }
    rsp = (char *)iot_calloc(1024);
    memset(rsp, 0, 1024);
    read_http_response(client, rsp, 1024 - 1);
    json_parse_utc_ip(rsp, time); // 解析应答获取时间

    EventBits_t bit_mask = HTTPS_GET_TIMEZONE_BIT 
                            | HTTPS_GET_IP_BIT 
                            | HTTPS_GET_TIME_DST_BIT;

    // 等待所有事件标志位
    EventBits_t event = xEventGroupWaitBits(
        http_event_handle,
        bit_mask,
        pdTRUE,  // 清除标志位
        pdTRUE,  // 等待所有标志位
        pdMS_TO_TICKS(HTTPS_GET_UTC_IP_TIMEOUT) // 超时时间
    );

    #if 0
    // 获取时区，需要配合标定流程，暂不使用
    if(event & HTTPS_GET_TIMEZONE_BIT){
        if(check_timezone_flag == true){
            continue;
        }
        int hour = timezone_offset/(int32_t)3600000;
        int min = (timezone_offset/60000)%60;
        ESP_LOGI(TAG, "get timezone success UTC:%d:%d", hour, min);
        if((hour > 14) || (hour < -12) || (min > 59) || (min < 0)){
            continue;
        }
        if (((iot_info->time_set.area & 0xFF) != (int8_t)hour) || ((iot_info->time_set.area >> 8) & 0xFF) != (int8_t)min) {
            if(set_sntp_timezone(hour, min) != -1){
                iot_info->time_set.area = (hour & 0xFF) | ((min & 0xFF) << 8);
                iot_write_flash(RTC_AREA, (uint8_t *)&iot_info->time_set.area, 2);
            }
        }
        check_timezone_flag = true;
    }
    #endif

#ifdef TIME_GET_FROM_SERVER_BY_HTTP_ENABLE

    // 获取时区 取消从该响应头获取时区
    // if((event & HTTPS_GET_TIMEZONE_BIT)&&!(event & HTTPS_GET_TIME_DST_ISO8601_BIT)) {
    //     int hour = timezone_offset/(int32_t)3600000;
    //     int min = (timezone_offset/60000)%60;
    //     if(!((hour > 14) || (hour < -12) || (min > 59) || (min < 0))){

    //         time_res_struct res = {
    //             .hour = (hour & 0xFF),
    //             .min = (min & 0xFF),
    //         };
                
    //         ESP_LOGI(TAG, "get timezone success UTC: [%02d:%02d]", res.hour, res.min);
            
    //         //判断是否为无效写入
    //         if(SetData.dev_info_t.SetTimeZone.all != res.all)
    //         {
    //             app_sntp_set_timezone(res.hour, res.min);
    //             SetData.dev_info_t.SetTimeZone.all = res.all;
    //             reals.SetDataWrFlag.sBit.res_2005 = 1;
    //             Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.SetTimeZone.all =res.all;
    //         }
    //     }
    // }
    
    // 获取本地时间
    if(event & HTTPS_GET_TIME_DST_BIT) {
        // 获取本地时间(timestamp)
        if(event & HTTPS_GET_TIME_DST_STAMP_BIT) {
            ESP_LOGI(TAG, "get dst time success : %llu", server_time_now);
            struct timeval tv_time = {
                .tv_sec = server_time_now,
                .tv_usec = 0
            };
            settimeofday(&tv_time, NULL);//设置时间到系统时间
            reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;
        }
        // 获取本地时间(ISO8601)
        else if(event & HTTPS_GET_TIME_DST_ISO8601_BIT) {
            int hour = server_tz_hour;
            int min = server_tz_min;
            if(!((hour > 14) || (hour < -12) || (min > 59) || (min < 0))){

                time_res_struct res = {
                    .hour = (hour & 0xFF),
                    .min = (min & 0xFF),
                };
                    
                ESP_LOGI(TAG, "get bttp ISO8601 timezone success UTC: [%02d:%02d]", res.hour, res.min);
                
                //判断是否为无效写入
                if(SetData.dev_info_t.SetTimeZone.all != res.all)
                {
                    app_sntp_set_timezone(res.hour, res.min);
                    SetData.dev_info_t.SetTimeZone.all = res.all;
                    reals.SetDataWrFlag.sBit.SetTimeZone = 1;
                    Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.SetTimeZone.all =res.all;
                    ESP_LOGI(TAG,"get bttp ISO8601 UTC:%u",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.SetTimeZone.all);
                }
            }   
            else
            {
                ESP_LOGE(TAG, "get timezone error : %ld", timezone_offset);
            }

            if ( tm_time_to_timestamp(&server_time_now, &server_tm_time) )
            {
                ESP_LOGI(TAG, "get dst time success : %llu", server_time_now);
                struct timeval tv_time = {
                    .tv_sec = server_time_now,
                    .tv_usec = 0
                };
                settimeofday(&tv_time, NULL);//设置时间到系统时间
                reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;
            }
        }
		
    }
#endif

    // // 获取IP
    // if(event & HTTPS_GET_IP_BIT) {
    //     if(is_valid_ip(gw_server_ip) == false) {
    //         ESP_LOGW(TAG, "invalid gw ip, use DNS ip");
    //     } else {
    //         strncpy(server_ip, gw_server_ip, max_len);
    //         ESP_LOGI(TAG, "get gw ip success: %s", server_ip);
    //         ret = 0;
    //     }
    // }

        // 获取IP
    if(event & HTTPS_GET_IP_BIT) {
        if(is_valid_server(gw_server_ip) == false) {
            ESP_LOGW(TAG, "invalid server: %s, use DNS ip", gw_server_ip);
        } else {
            strncpy(server_ip, gw_server_ip, max_len);
            ESP_LOGI(TAG, "get server success: %s", server_ip);
            
            // 时间也需有效
            if(*time != 0) ret = 0;
        }
    }

cleanup:
    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (rsp) {
        free(rsp);
    }

    return ret;
}

void http_dst_time_update(void) 
{
    char *rsp = NULL;
    esp_http_client_config_t config = {0}; 

    // 根据环境选择URL
    if (!strcmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST)) {  // 开发环境
        config.url = DEV_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST)) { // 测试环境
        config.url = TEST_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) { // 生产环境
        config.url = PRO_ENV_URL""HTTP_UTC_TIME;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST, use default host");
        config.url = PRO_ENV_URL""HTTP_UTC_TIME; // 默认生产
    }

    config.method = HTTP_METHOD_GET;
    config.disable_auto_redirect = false;
    config.buffer_size = 1024;
    config.timeout_ms = HTTPS_GET_UTC_IP_TIMEOUT; // 设置超时时间
    config.event_handler = https_event_handle;
    //config.if_name = &g_netif_req; // 设置网口

    if(http_event_handle == NULL){
        http_event_handle = xEventGroupCreate();
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "client is NULL");
        goto http_dst_time_update_cleanup;
    }

    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto http_dst_time_update_cleanup;
    }

    #if 0
    //仅供调试使用，出厂代码禁止使用
    /*
    DE: 93.193.194.180
    US: 98.97.176.254
    HK: 119.229.140.106
    SG: 102.182.138.85
    */
    const char *vitual_ip = "93.193.194.180";
    if(strlen(vitual_ip) > 0) {
        ESP_LOGI(TAG,"set x-client-ip:%s", vitual_ip);
        esp_http_client_set_header(client,"x-client-ip", vitual_ip);
    }
    #endif
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto http_dst_time_update_cleanup;
    }

    rsp = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    memset(rsp, 0, 1024);
    read_http_response(client, rsp, 1024 - 1);

    EventBits_t bit_mask = HTTPS_GET_TIMEZONE_BIT 
                            | HTTPS_GET_IP_BIT 
                            | HTTPS_GET_TIME_DST_BIT;
    
    // 等待所有事件标志位
    EventBits_t event = xEventGroupWaitBits(
        http_event_handle,
        bit_mask,
        pdTRUE,  // 清除标志位
        pdTRUE,  // 等待所有标志位
        pdMS_TO_TICKS(HTTPS_GET_UTC_IP_TIMEOUT) // 超时时间
    );
    ESP_LOGI(TAG,"http_dst_time_update event:0X%lu",event);

    // 获取时区 取消从该响应头获取时区
    // if((event & HTTPS_GET_TIMEZONE_BIT)&&!(event & HTTPS_GET_TIME_DST_ISO8601_BIT)) {
    //     int hour = timezone_offset/(int32_t)3600000;
    //     int min = (timezone_offset/60000)%60;
    //     if(!((hour > 14) || (hour < -12) || (min > 59) || (min < 0))){

    //         time_res_struct res = {
    //             .hour = (hour & 0xFF),
    //             .min = (min & 0xFF),
    //         };
                
    //         ESP_LOGI(TAG, "get timezone success UTC: [%02d:%02d]", res.hour, res.min);
            
    //         //判断是否为无效写入
    //         if(SetData.dev_info_t.SetTimeZone.all != res.all)
    //         {
    //             app_sntp_set_timezone(res.hour, res.min);
    //             SetData.dev_info_t.SetTimeZone.all = res.all;
    //             reals.SetDataWrFlag.sBit.SetTimeZone = 1;
    //             Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.SetTimeZone.all =res.all;    
    //         }
    //     }   
    //     else
    //     {
    //         ESP_LOGE(TAG, "get timezone error : %ld", timezone_offset);
    //     }
    // }
    
    // 获取本地时间
    if(event & HTTPS_GET_TIME_DST_BIT) {
        // 获取本地时间(timestamp)
        if(event & HTTPS_GET_TIME_DST_STAMP_BIT) {
            ESP_LOGI(TAG, "get dst time success : %llu", server_time_now);
            struct timeval tv_time = {
                .tv_sec = server_time_now,
                .tv_usec = 0
            };
            settimeofday(&tv_time, NULL);//设置时间到系统时间
            reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;

	        struct tm tm_now;
            struct tm tm_now1;
            memcpy(&tm_now, localtime(&server_time_now), sizeof(struct tm));

            ESP_LOGI(TAG, "get http ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now.tm_mon+1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
            memcpy(&tm_now1, gmtime(&server_time_now), sizeof(struct tm));
            ESP_LOGI(TAG, "get http NowdateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now1.tm_mon+1,tm_now1.tm_mday,tm_now1.tm_hour,tm_now1.tm_min,tm_now1.tm_sec);
        }
        // 获取本地时间(ISO8601)
        else if(event & HTTPS_GET_TIME_DST_ISO8601_BIT) {
            int hour = server_tz_hour;
            int min = server_tz_min;
            if(!((hour > 14) || (hour < -12) || (min > 59) || (min < 0))){

                time_res_struct res = {
                    .hour = (hour & 0xFF),
                    .min = (min & 0xFF),
                };
                    
                ESP_LOGI(TAG, "get bttp ISO8601 timezone success UTC: [%02d:%02d]", res.hour, res.min);
                
                //判断是否为无效写入
                if(SetData.dev_info_t.SetTimeZone.all != res.all)
                {
                    app_sntp_set_timezone(res.hour, res.min);
                    SetData.dev_info_t.SetTimeZone.all = res.all;
                    reals.SetDataWrFlag.sBit.SetTimeZone = 1;
                    Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.SetTimeZone.all =res.all;
                }
            }   
            else
            {
                ESP_LOGE(TAG, "get timezone error : %ld", timezone_offset);
            }



            if ( tm_time_to_timestamp(&server_time_now, &server_tm_time) )
            {
                ESP_LOGI(TAG, "get dst time success : %llu", server_time_now);
                struct timeval tv_time = {
                    .tv_sec = server_time_now,
                    .tv_usec = 0
                };
                settimeofday(&tv_time, NULL);//设置时间到系统时间
                reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;

                struct tm tm_now;
                struct tm tm_now1;
                memcpy(&tm_now, localtime(&server_time_now), sizeof(struct tm));

                ESP_LOGI(TAG, "get http ISO8601 ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now.tm_mon+1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
                memcpy(&tm_now1, gmtime(&server_time_now), sizeof(struct tm));
                ESP_LOGI(TAG, "get http ISO8601 NowdateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now1.tm_mon+1,tm_now1.tm_mday,tm_now1.tm_hour,tm_now1.tm_min,tm_now1.tm_sec);
            }
        }
        else
        {
            ESP_LOGE(TAG, "get dst time error : %llu", server_time_now);
        }
    }
    
http_dst_time_update_cleanup:

    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (rsp) {
        free(rsp);
    }

    return;
}

/**
 * @brief 通过HTTPS GET请求获取新的图像信息。
 *
 * 此函数向指定的客户端发送HTTPS GET请求，以获取新的图像信息。
 * 函数接受客户端句柄、类型、序列号、版本列表和版本号作为参数。
 * 它返回一个指向ota_cmd_list_t结构的指针，其中包含HTTP OTA命令列表。
 *
 * @param client ESP HTTP客户端的句柄。
 * @param product_type 产品类型。
 * @param type 请求命令的类型。
 * @param sn 序列号。
 * @param ver_list 版本列表。
 * @param ver_num 版本数量。
 * @return 指向ota_cmd_list_t结构的指针，其中包含HTTP OTA命令列表。
 */
ota_cmd_list_t *https_get_new_image_info(esp_http_client_handle_t client, char *product_type,char * type ,uint64_t sn, ver_data_t ver_list[], uint8_t ver_num) {
    ota_cmd_list_t *http_ota_cmd_list = NULL;
    char *request_cmd = NULL;
    char *rsp_data = NULL;
    ESP_LOGI(TAG,"https_get_new_image_info product_type:%s,TYPE:%s,sn:%llu",product_type,type,sn);
    for(int i=0;i<ver_num;i++)
    {
        ESP_LOGI(TAG,"i:%d,type:%d ,version:%lu",i,ver_list[i].type,ver_list[i].version);
    }
    if (client == NULL) {
        ESP_LOGE(TAG, "client is NULL");
        return NULL;
    }
    request_cmd = json_ota_http_request(product_type,type, sn, ver_list, ver_num);
    if (request_cmd == NULL) {
        ESP_LOGE(TAG, "Failed to create request command");
        goto cleanup;
    }
    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto cleanup;
    }
    esp_err_t err = esp_http_client_open(client, strlen(request_cmd));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }
    if (esp_http_client_write(client, request_cmd, strlen(request_cmd)) < 0) {
        ESP_LOGE(TAG, "Failed to write HTTP connection");
        goto cleanup;
    }
    rsp_data = (char *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }
    memset(rsp_data, 0, 4096);
    if (read_http_response(client, rsp_data, 4096) > 0) {
        http_ota_cmd_list = json_parse_ota_rsp(rsp_data);
    } else {
        ESP_LOGE(TAG, "Failed to read HTTP response");
    }
    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));

cleanup:
    if (client) {
        esp_http_client_close(client);
    }
    if (rsp_data) {
        free(rsp_data);
    }
    if (request_cmd) {
        json_buffer_clear(request_cmd);
    }

    return http_ota_cmd_list;
}

/**
 * @brief 检查是否有新的固件版本。
 *
 * 此函数用于检查是否有新的固件版本可用。只要有一个新版本可用，函数就返回0。
 * 它遍历OTA命令列表，并与提供的版本列表进行比较。
 * 如果找到新版本（版本号的料号相同，最后两位数更高），则返回0。
 * 否则，返回-1。
 *
 * @param ota_cmd_list OTA命令列表的头指针。
 * @param ver_list 提供的版本列表。
 * @param ver_num 版本列表中的版本数量。
 * @return 如果找到新版本，返回0；否则返回-1。
 */
static int check_ver_list(ota_cmd_list_t *ota_cmd_list, ver_data_t ver_list[], uint8_t ver_num, new_version_update_struct *version_data) {
    int ret = -1;
    uint8_t j = 0;
    memset(version_data, 0, sizeof(new_version_update_struct));
    
    while (ota_cmd_list) {
        for (uint8_t i = 0; i < ver_num; i++) {
            if (ota_cmd_list->object.type == ver_list[i].type && 
                ota_cmd_list->object.version/100 == ver_list[i].version/100)
            {
                if ( ota_cmd_list->object.version%100 > ver_list[i].version%100 )
                {
                    ret = 0;
                    version_data->have_new_version_flag = true;
                }

                version_data->ver_list[j].type = ota_cmd_list->object.type;
                version_data->ver_list[j].version = ota_cmd_list->object.version;
                ESP_LOGI(TAG, "check_ver_list : have_new_version(type : %d)( %lu -> %lu )", ota_cmd_list->object.type, ver_list[i].version, ota_cmd_list->object.version);
                j++;
            }
        }
        ota_cmd_list_t *list_head = ota_cmd_list;
        ota_cmd_list = list_head->next;
        free(list_head);
    }
    version_data->softver_num = j;
    return ret;
}


#define HTTPS_REQUEST_IMAGE "/api/blusmartprod/device/firmware/v1/latest/firmwareVerList/batch?gwcredentials=osUZ8ygqt1s%2FawsURLTwupGKN%2FCH8sRRODw%2FlZLlrv49jdBtu7UuRqxUkYHH6jWswlLPybPJ2WShH%2Fr928K10amSgY0pWE2%2BeeijxYovV9DIcRgwZhBbSQ%3D%3D"

/**
 * @brief 获取最新固件版本。
 *
 * 此函数用于获取最新固件版本。
 * 应在以下情况下调用此函数：
 * 1. 系统启动(联网)时检查一次
 * 2. 周期24h检查一次
 * 3. OTA升级完成后检查一次
 * 4. 有新的设备接入检查一次
 * 5. 下级设备软件版本数量变动时检查一次
 * @param target 检查的目标。
 * @return 如果所有检查都成功，返回0，否则返回-1。
 */
int https_new_image_check(uint8_t target) 
{
    #define VER_LIST_MAX_NUM  6
    
    static uint8_t error_cnt = 0;
    int ret = 0;
    ota_cmd_list_t *http_ota_cmd_list = NULL;
    
    esp_http_client_config_t config = {0};
    if (!strcmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST)) {
        config.url = DEV_ENV_URL""HTTPS_REQUEST_IMAGE;
    } else if (!strcmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST)) {
        config.url = TEST_ENV_URL""HTTPS_REQUEST_IMAGE;
    } else if (!strcmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
        config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST, use default host");
        config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE;
    }
    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 2048;
    config.timeout_ms = 2000; // 设置超时时间
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "client is NULL");
        reals.need_new_version_flag.Byte2 = 0; 
        return -1;
    }

    uint8_t ver_num = 0;
    ver_data_t ver_list[VER_LIST_MAX_NUM] = {0};
    bool need_update_flag = false; 

    switch (target) {

        /*IOT*/
        case HTTPS_CHECK_IOT_IMAGE:
            add_ver_list(ver_list, &ver_num, DEVICE_IOT, Inv[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.software_ver, VER_LIST_MAX_NUM);
            if(ver_num == 0) break;
#if 0
            http_ota_cmd_list = https_get_new_image_info(client, g_self_data.mod_reg11000_IOT_info.iot_type, g_self_data.mod_reg11000_IOT_info.iot_sn, ver_list, ver_num);
#else
            http_ota_cmd_list = https_get_new_image_info(client, SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn, ver_list, ver_num);
#endif
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &iot_version_update) == 0){
                need_update_flag = true;
            }
            break;
        
        /*INV、DSP、BMS*/
        case HTTPS_CHECK_INV_IMAGE:
            for (uint8_t i = 0 ; i < Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.software_total ; i++ )
            {
                if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type<=DEVICE_BMS&&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type>=DEVICE_ARM)
                    add_ver_list(ver_list, &ver_num, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].version, VER_LIST_MAX_NUM);
            }
            if(ver_num == 0) break;
            http_ota_cmd_list = https_get_new_image_info(client,SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &inv_version_update) == 0){
                need_update_flag = true;
            }

            break;

        /*PACK*/
        case HTTPS_CHECK_PACK_IMAGE:
            for(uint8_t i = 0; i < SLAVE_MAX_INDEX; i++)
            {
                ver_num = 0;
                memset(ver_list, 0, sizeof(ver_list));
                ESP_LOGI(TAG,"check pack:%d version total:%d",i,Inv_Pack_Slave[i].mod_reg06100_Pack_each.software_total);
                for (uint8_t j = 0 ; j < Inv_Pack_Slave[i].mod_reg06100_Pack_each.software_total ; j++ )
                {
                    add_ver_list(ver_list, &ver_num, Inv_Pack_Slave[i].mod_reg06100_Pack_each.soft[j].type, Inv_Pack_Slave[i].mod_reg06100_Pack_each.soft[j].version, VER_LIST_MAX_NUM);
                }
                if(ver_num == 0) continue;
                http_ota_cmd_list = https_get_new_image_info(client,Inv_Pack_Slave[i].mod_reg06100_Pack_each.type_ascii, Inv_Pack_Slave[i].mod_reg06100_Pack_each.type_ascii, Inv_Pack_Slave[i].mod_reg06100_Pack_each.sn_code, ver_list, ver_num);
                if(http_ota_cmd_list == NULL){
                    ret = -1;
                } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &pack_version_update[i]) == 0){
                    need_update_flag = true;
                }
            }
                
            break;
        case HTTPS_CHECK_ACHUB_IMAGE:
            for (uint8_t i = 0 ; i < Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.software_total ; i++ )
            {
                if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type==DEVICE_AC_HUB)
                    add_ver_list(ver_list, &ver_num, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].version, VER_LIST_MAX_NUM);
            }
            if(ver_num == 0) break;
            http_ota_cmd_list = https_get_new_image_info(client,SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &achub_version_update) == 0){
                need_update_flag = true;
            }

            break;
        case HTTPS_CHECK_DCHUB_IMAGE:
            for (uint8_t i = 0 ; i < Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.software_total ; i++ )
            {
                if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type==DEVICE_DC_HUB)
                    add_ver_list(ver_list, &ver_num, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].version, VER_LIST_MAX_NUM);
            }
            if(ver_num == 0) break;
            uint64_t dcSn=GetUin64FromPtrSmall(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15700_Dc_Hub_info.dc_hub_sn);
            http_ota_cmd_list = https_get_new_image_info(client,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15700_Dc_Hub_info.dc_hub_type, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15700_Dc_Hub_info.dc_hub_type, dcSn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &dchub_version_update) == 0){
                need_update_flag = true;
            }

            break;

        /*D400S*/
        case HTTPS_CHECK_D400S_IMAGE:
            add_ver_list(ver_list, &ver_num, DEVICE_DC_DC, Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.software_ver, VER_LIST_MAX_NUM);
            // for (uint8_t i = 0 ; i < Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.software_ver.software_total ; i++ )
            // {
            //     add_ver_list(ver_list, &ver_num, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type, Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].version, VER_LIST_MAX_NUM);
            // }
            if(ver_num == 0) break;
            http_ota_cmd_list = https_get_new_image_info(client,Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_type, Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_type, Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &d400s_version_update) == 0){
                need_update_flag = true;
            }

            break;

        
        default:break;
    }
    esp_http_client_cleanup(client);
    if ( ret == -1 ) {
        error_cnt++;
        if ( error_cnt >= 2 ) {
            error_cnt = 0;
            reals.need_new_version_flag.Byte2 &= ~(1 << target); 
        }
    } else {
        error_cnt = 0;
    }
    return ret;
}
/**
 * @brief 根据版本号判断是否需要升级。
 *
 * @param input  uint8_t dev_type,uint64_t version
 * @return uint8_t，如果需要更新 返回1，否则返回0。
 */
uint8_t Get_Http_Version_Check(uint8_t dev_type,uint32_t version)
{
    int ret = 0;
    new_version_update_struct *version_data = NULL;
    switch ( dev_type )
    {
        case HTTPS_CHECK_PACK_IMAGE :
            for(int i=0;i<PACK_TYPE_MAX_NUM;i++)
            {
                for(int j=0;j<pack_version_update[i].softver_num;j++)
                {
                   // ESP_LOGI(TAG,"pack_version_update[%d].ver_list[%d].version:%lu,version:%lu",i,j,pack_version_update[i].ver_list[j].version,version);
                    if(pack_version_update[i].ver_list[j].version/100==version/100)
                    {
                        if(pack_version_update[i].ver_list[j].version>version)
                        {
                            ret=1;
                            goto _end;
                        }
                    }
                }
                
            }
            break;
        default:
            break;;
    }
_end:
    return ret;
}

/**
 * @brief 检查新固件标志。
 *
 * 此函数用于检查是否有新的固件版本可用。
 */
bool Get_http_new_version_flag(uint8_t dev_type, uint8_t index) 
{
    int ret = 0;
    new_version_update_struct *version_data = NULL;
    switch ( dev_type )
    {
        case HTTPS_CHECK_IOT_IMAGE :
            version_data = &iot_version_update;
            break;
        
        case HTTPS_CHECK_INV_IMAGE :
            version_data = &inv_version_update;
            break;
        
        case HTTPS_CHECK_PACK_IMAGE :
            if (( index >= 0 ) && ( index < PACK_TYPE_MAX_NUM )) {
                version_data = &pack_version_update[index];
            } else {
                return 0;
            }
            break;
        case HTTPS_CHECK_DCHUB_IMAGE :
            version_data = &dchub_version_update;
            break;
        case HTTPS_CHECK_ACHUB_IMAGE :
            version_data = &achub_version_update;
            break;
        case HTTPS_CHECK_D400S_IMAGE :
            version_data = &d400s_version_update;
            break;
        
        
        default:
            return 0;
    }

    return version_data->have_new_version_flag;
}

void https_new_image_timeout_clean(void)
{
    static bool last_ota_status = false;
    static uint32_t pre_time = 0;

    bool new_ota_status = reals.ota_happen;//sys_is_updating();
    if (( new_ota_status == false ) && ( last_ota_status == true ))
    {
        ESP_LOGI(TAG,"https_new_image_timeout_clean iSOta:%d type:%d",can_ota_status[0].ota_summary.isOta,can_ota_status[0].ota_summary.type);
        /*升级成功，根据对应类型清除新固件信息*/
        if ( can_ota_status[0].ota_summary.isOta == 0 )  
        {
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            switch ( can_ota_status[0].ota_summary.type )
            {
                case DEVICE_IOT :
                    memset(&iot_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.iot = 1;
                    break;
                    
                case DEVICE_ARM :
                case DEVICE_DSP :
                case DEVICE_BMS :
                    memset(&inv_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.inv = 1;
                    break;
                case DEVICE_DC_HUB :
                    memset(&dchub_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.dchub = 1;
                    break;
                case DEVICE_AC_HUB :
                    memset(&achub_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.achub = 1;
                    break;            
                

                case DEVICE_PACK_BCU :
                case DEVICE_PACK_BMU :
                case DEVICE_PACK_BMS :
                case DEVICE_PACK_SAFTY :
                    memset(pack_version_update, 0, sizeof(pack_version_update));
                    reals.need_new_version_flag.sBit.pack = 1;
                    break;
                case DEVICE_DC_DC :
                    memset(&d400s_version_update, 0, sizeof(&d400s_version_update));
                    reals.need_new_version_flag.sBit.d400s = 1;
                    break;
                
                default:
                    break;
            }
            
            ESP_LOGW(TAG, "https_new_image_timeout_clean (type : %d)", can_ota_status[0].ota_summary.type);
        }
    }

    last_ota_status = new_ota_status;
    
    if ( new_ota_status == true )
    {
        /*升级时暂停查询*/
        https_new_image_timeout_clean_flag = true;
    }
    else
    {
        /*升级成功后延时20s再次查询新固件版本*/
        if ( pre_time != 0 )
        {
            uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ( (now_time - pre_time) <= 30000 )
            {
                https_new_image_timeout_clean_flag = true;
            }
            else
            {
                pre_time = 0;
            }
        }
        else
        {
            /*正常查询*/
            https_new_image_timeout_clean_flag = false;
        }
    }
}

static void https_get_record_id(esp_http_client_handle_t client, char recordId[32], char *state,
                                    char *reason, uint8_t image_type, uint32_t new_ver, uint32_t old_ver)
{
    if (client == NULL) {
        ESP_LOGE(TAG, "client is NULL");
        return;
    }

    char *request_cmd = NULL;
    char *rsp_data = NULL;

    uint64_t dev_sn = 0;
    char dev_type[13] = {0};
    if(reals.online_ACHUB_num == 0)
    {
        // AP300单机远程升级时，服务器只下发AP300的型号及其SN，IOT侧需靠版本料号判断是否升级
        dev_sn = SetData.dev_info_t.INV_dev_sn;
        memcpy(dev_type, SetData.dev_info_t.INV_dev_type, 12);
        dev_type[12] = '\0';
    }
    else
    {
        // 并机时，服务器只下发HA1的型号及其SN，IOT侧也需靠版本料号判断是否升级
        dev_sn = SetData.dev_info_t.Parallel_dev_sn;
        memcpy(dev_type, SetData.dev_info_t.Parallel_dev_type, 12);
        dev_type[12] = '\0';
    }

    request_cmd = json_ota_response_first((const char *)recordId, state, reason, dev_sn, 
                                        dev_type, image_type, new_ver, old_ver);
    if (request_cmd == NULL) {
        ESP_LOGE(TAG, "Failed to create request command");
        goto cleanup;
    }

    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto cleanup;
    }

    esp_err_t err = esp_http_client_open(client, strlen(request_cmd));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }

    if (esp_http_client_write(client, request_cmd, strlen(request_cmd)) < 0) {
        ESP_LOGE(TAG, "Failed to write HTTP connection");
        goto cleanup;
    }

    rsp_data = (char *)heap_caps_malloc(2048, MALLOC_CAP_SPIRAM);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }

    if (read_http_response(client, rsp_data, 2048) > 0) {
        char record_id[32];
        if(json_parse_record_id(rsp_data, record_id) == 0) {
            memset(image_record_id, 0x00, sizeof(image_record_id));
            strncpy(image_record_id, record_id, sizeof(image_record_id) - 1);
            image_record_id[sizeof(image_record_id) - 1] = '\0';
        }
    } else {
        ESP_LOGE(TAG, "Failed to read HTTP response");
    }
    ESP_LOGI(TAG, "https_get_record_id HTTP POST Status = %d, content_length = %lld",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));

cleanup:
    if (client) {
        esp_http_client_close(client);
    }
    if (rsp_data) {
        free(rsp_data);
    }
    if (request_cmd) {
        json_buffer_clear(request_cmd);
    }
}

/* 不同设备的固件需要传入不同的设备型号 ，同一设备型号可以同时传入多个不同固件版本到req_info.verlist中，同时填入数量req_info.ver_num 
    eg: RV100+SN+Ver_list, B210+SN+Ver_list*/
static ota_cmd_list_t *https_new_image_check_by_req(new_image_req_t req_info)
{
    if(req_info.ver_num == 0)
    {
        ESP_LOGW(TAG, "no version to check");
        return NULL;
    }

    int ret = 0;
    ota_cmd_list_t *http_ota_cmd_list = NULL;

    esp_http_client_config_t config = {0};
    if (!strcmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST))
    {
        config.url = DEV_ENV_URL""HTTPS_REQUEST_IMAGE;
    }
    else if (!strcmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST))
    {
        config.url = TEST_ENV_URL""HTTPS_REQUEST_IMAGE;
    }
    else if (!strcmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL))
    {
        config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE;
    }
    else
    {
        ESP_LOGE(TAG, "UNKNOWN HOST, use PRO_ENV_URL");
        config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE;
    }

    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 2048;
    config.timeout_ms = 8000; // 设置超时时间

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "client is NULL");
        return NULL;
    }

    http_ota_cmd_list = https_get_new_image_info(client, req_info.dev_type, req_info.dev_type, req_info.dev_sn, req_info.ver_list, req_info.ver_num);

    esp_http_client_cleanup(client);

    return http_ota_cmd_list;
}

static void check_firmware_updates(void)
{
    new_image_req_t *req_info = NULL;
    ota_cmd_list_t *firmware_info = NULL;

    // 分配请求信息结构体内存
    req_info = (new_image_req_t *)heap_caps_malloc(sizeof(new_image_req_t), MALLOC_CAP_SPIRAM);
    if (req_info == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for req_info");
        return;
    }

    /* 检查IOT的固件版本信息 */
    memset(req_info, 0, sizeof(new_image_req_t));

    // req_info->dev_sn = g_self_data.mod_reg11000_IOT_info.iot_sn;
    // strncpy(req_info->dev_type, g_self_data.mod_reg11000_IOT_info.iot_type, sizeof(req_info->dev_type));
#if 1
    // AP300单机远程升级时，服务器只下发AP300的型号及其SN，IOT侧需靠版本料号判断是否升级
    req_info->dev_sn = SetData.dev_info_t.INV_dev_sn;
    strncpy(req_info->dev_type, SetData.dev_info_t.INV_dev_type, sizeof(req_info->dev_type));
#else
    if(reals.online_ACHUB_num == 0)
    {
        // AP300单机远程升级时，服务器只下发AP300的型号及其SN，IOT侧需靠版本料号判断是否升级
        req_info->dev_sn = SetData.dev_info_t.INV_dev_sn;
        strncpy(req_info->dev_type, SetData.dev_info_t.INV_dev_type, sizeof(req_info->dev_type));
    }
    else
    {
        // 并机时，服务器只下发HA1的型号及其SN，IOT侧也需靠版本料号判断是否升级
        req_info->dev_sn = SetData.dev_info_t.Parallel_dev_sn;
        strncpy(req_info->dev_type, SetData.dev_info_t.Parallel_dev_type, sizeof(req_info->dev_type));
    }
#endif
    req_info->ver_num = 1;
    req_info->ver_list[0].type = DEVICE_IOT;
    req_info->ver_list[0].version = g_self_data.mod_reg11000_IOT_info.software_ver;
    firmware_info = https_new_image_check_by_req(*req_info);

    // 释放请求信息结构体内存
    free(req_info);

    if (firmware_info)
    {
        if (new_firmware_list)
        {
            //释放掉旧的信息
            ota_cmd_list_t  *current = new_firmware_list;
            while (current)
            {
                ota_cmd_list_t *next = current->next;
                free(current);
                current = next;
            }
        }

        new_firmware_list = firmware_info;

        /* TODO: 临时调试打印日志 */
        ota_cmd_list_t  *current = new_firmware_list;
        while (current)
        {
            ota_cmd_list_t *next = current->next;
            ESP_LOGI(TAG, "Have a new version, model:%s, dev_model:%s, version:%lu, url:%s", current->object.model,
                    current->object.dev_model_ascii, current->object.version, current->object.url);
            current = next;
        }

    }
}

static bool check_firmware_version(uint32_t new_version, uint32_t old_version, bool force)
{
    // 检查版本格式(新料号格式大于1000000，旧料号格式小于1000000)
    bool is_new_format = (new_version >= 1000000 && old_version >= 1000000);
    
    // 根据料号格式确定比较方法
    bool same_type;
    bool need_upgrade;
    
    if (is_new_format) {
        // 新料号格式比较 (前四位相同表示同一产品)
        same_type = (new_version / 10000 == old_version / 10000);
        need_upgrade = (new_version % 10000 > old_version % 10000);
    } else {
        // 旧料号格式比较 (前两位相同表示同一产品)
        same_type = (new_version / 100 == old_version / 100);
        need_upgrade = (new_version % 100 > old_version % 100);
    }
    
    // 判断是否需要升级：产品型号相同且版本需要升级(或强制升级)
    return (same_type && (force || need_upgrade));
}

static int https_ota_get_record_id(char recordId[32], char *state, char *reason, uint8_t image_type, uint32_t new_ver, uint32_t old_ver)
{
    int ret = 0;

    esp_http_client_config_t config = {0};
    if (!strcmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST))
    {
        config.url = DEV_ENV_URL""HTTPS_OTA_STATE_IF;
    }
    else if (!strcmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST))
    {
        config.url = TEST_ENV_URL""HTTPS_OTA_STATE_IF;
    }
    else if (!strcmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL))
    {
        config.url = PRO_ENV_URL""HTTPS_OTA_STATE_IF;
    }
    else
    {
        ESP_LOGE(TAG, "unknown host: %s, use product env",  SetData.dev_info_t.could_dns);
        config.url = PRO_ENV_URL""HTTPS_OTA_STATE_IF;
    }

    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 2048;
    config.timeout_ms = 2000; // 设置超时时间

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "client is NULL");
        return -1;
    }

    https_get_record_id(client, recordId, state, reason, image_type, new_ver, old_ver);

    esp_http_client_cleanup(client);

    return 0;
}

void update_firmware_check(void)
{
    is_update_check_triggered = true;
}

int firmware_upgrade_start(char *model, char *version)
{
    if (!model || !version)
    {
        ESP_LOGE(TAG, "firmware upgrade start model or version error");
        return -1;
    }

    if (!new_firmware_list)
    {
        ESP_LOGE(TAG, "no firmware to upgrade");
        return -2;
    }

    //释放掉旧的信息
    ota_cmd_list_t  *current = new_firmware_list;
    while (current)
    {
        ota_cmd_list_t *next = current->next;
        ESP_LOGW(TAG, "Find upgrade firmware, model:%s, version:%lu", current->object.model, current->object.version);
        char firmware_ver[16] = {0};
        snprintf(firmware_ver, sizeof(firmware_ver), "%lu", current->object.version);
        if (0 == strcmp(model, current->object.model) && 0 == strcmp(version, firmware_ver))
        {
            ota_cmd_list = current;
            need_query_recordId = true;

            ESP_LOGW(TAG, "Start upgrade firmware, model:%s, version:%s", model, version);
            return 1;
        }
        current = next;
    }

    return 0;
}

/**
 * @brief 请求并获取节省金钱的数据
 * 
 * 输入参数：flag:0:当日, 1:当月, 2:当年, 5:近6年; 
 * @return uint64_t 节省的金额,失败返回0
 */
static int http_request_money_save(const uint64_t device_sn, const char *device_type, uint16_t flag,
                                        uint8_t *temperature_unit_out, uint32_t *saved_money_out)
{
    esp_http_client_handle_t client = NULL;
    char *json_cmd = NULL;
    char *rsp_data = NULL;
    uint8_t temperature_unit = 0;
    uint64_t saved_money = 0;
    int ret = -1;
    // 1. 检查输入参数
    if (device_type == NULL || temperature_unit_out == NULL || saved_money_out == NULL) {
        ESP_LOGE(TAG, "Invalid input parameters");
        goto cleanup;
    }

    // 2. 构建请求URL
    esp_http_client_config_t config = {0};
    if(!strncmp(SetData.dev_info_t.could_dns, DEV_ENV_HOST, 128)){
        config.url = DEV_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    } else if (!strncmp(SetData.dev_info_t.could_dns, TEST_ENV_HOST, 128)) {
        config.url = TEST_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    } else if (!strncmp(SetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL, 128)) {
        config.url = PRO_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST, use default host");
        config.url = PRO_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    }

    // 3. 设置HTTP客户端配置
    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 1024;
    config.timeout_ms = 5000; // 增加超时时间到5秒
    // config.skip_cert_common_name_check = true;

    // 4. 创建HTTP客户端
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        goto cleanup;
    }
    
    // 5. 设置通用请求头
    if(set_http_header(client) != 0) {
        ESP_LOGE(TAG, "Failed to set HTTP headers");
        goto cleanup;
    }

    // 6. 设置请求体
    json_cmd = json_get_money_save_cmd(device_sn, device_type, flag);
    if (json_cmd == NULL) {
        ESP_LOGE(TAG, "Failed to get money save cmd");
        goto cleanup;
    }

    // 7. 发送请求
    esp_err_t err = esp_http_client_open(client, strlen(json_cmd));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }

    int write_len = esp_http_client_write(client, json_cmd, strlen(json_cmd));
    if (write_len < 0) {
        ESP_LOGE(TAG, "Failed to write HTTP connection");
        goto cleanup;
    }

    // 8. 接收响应
    rsp_data = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }

    int read_len = read_http_response(client, rsp_data, 1024);
    if (read_len > 0) {
        ESP_LOGI(TAG, "Money save response: len %d, %s", read_len, rsp_data);
        
        // 9. 记录请求状态
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %llu, rsp_data:%s",
                 status_code, esp_http_client_get_content_length(client), rsp_data);

        if(status_code == HttpStatus_Ok) {
            if(json_get_money_save_rsp(rsp_data, &saved_money, &temperature_unit) == 0) {
                ESP_LOGI(TAG, "Successfully got money, URL:%s save data: %llu, temperature_unit: %d", config.url, saved_money, temperature_unit);
                *temperature_unit_out = temperature_unit;
                if (saved_money > UINT32_MAX) {
                    *saved_money_out = 0xFFFFFFFF;
                } else {
                    *saved_money_out = saved_money;
                }
                ret = 0;
            } else {
                ESP_LOGE(TAG, "Failed to get money save data");
            }
        } else {
            ESP_LOGW(TAG, "HTTP request failed with status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Failed to read HTTP response");
    }

cleanup:
    if (json_cmd) {
        free(json_cmd);
    }
    if (rsp_data) {
        free(rsp_data);
    }
    if (client) {
        esp_http_client_cleanup(client);
    }
    
    return ret;
}

/**
 * @brief 更新节省金额和温度单位数据
 * @param void
 * @return int 0-成功更新, -1-HTTP请求失败
 */
int http_update_money_save_data(void)
{    
    // 请求服务器获取最新的节省金额和温度单位
    uint8_t temperature_unit = 0;
    uint32_t saved_money = 0;
    uint8_t save_money_flag = 0;

    switch (SetData.dev_info_t.Time_Span.save_money) {
        case 1:
            save_money_flag = 0; // 当日
            break;
        case 3:
            save_money_flag = 1; // 当月
            break;
        case 4:
            save_money_flag = 2; // 当年
            break;
        case 5:
            save_money_flag = 5; // 近6年
            break;
        default:
            save_money_flag = 2; // 默认当年
            break;
    }

    if(reals.online_ACHUB_num > 0)
    {
        if (http_request_money_save(SetData.dev_info_t.Parallel_dev_sn, SetData.dev_info_t.Parallel_dev_type,
                                save_money_flag,
                                &temperature_unit, &saved_money) != 0) {
            ESP_LOGE(TAG, "ACHUB HTTP request for money save data failed");
            return -1; // HTTP请求失败
        }
    }
    else
    {
        if (http_request_money_save(SetData.dev_info_t.INV_dev_sn, SetData.dev_info_t.INV_dev_type,
                                save_money_flag,
                                &temperature_unit, &saved_money) != 0) {
            ESP_LOGE(TAG, "HTTP request for money save data failed");
            return -1; // HTTP请求失败
        }
    }

    // 仅当APP没设置过才通过HTTP同步温度单位
    if (SetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag == 0)
    {
        //本地温度单位与服务器温度单位不同时
        if ((SetData.dev_info_t.LCD_Mode.temperature_unit != temperature_unit) && (temperature_unit != 0))
        {
            g_self_data.mod_reg12000_IOT_set.LCD_Mode.temperature_unit = temperature_unit;
            SetData.dev_info_t.LCD_Mode.temperature_unit = temperature_unit;
            reals.SetDataWrFlag.sBit.mode_set = 1;
            ESP_LOGI(TAG, "Update temperature unit to %d from server", temperature_unit);
        }
    }

    // 更新节省金额（仅当数值发生变化时）
    if (SetData.dev_info_t.saveMoneyNums != saved_money) {
        g_self_data.mod_reg11000_IOT_info.saveMoneyNums = saved_money;
        SetData.dev_info_t.saveMoneyNums = saved_money;
        reals.SetDataWrFlag.sBit.saved_money = 1;
        ESP_LOGI(TAG, "Update saved_money to %lu from server", saved_money);
    }

    return 0; // 成功更新
}
