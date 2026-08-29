/**
  ******************************************************************************
  * @file      http_client.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/24
  * @brief     http相关任务及接口
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/24  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* C标准库 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/param.h>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* ESP系统 */
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
 #include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "mbedtls/sha256.h"
#include "esp_netif.h"

/* 应用层 */
#include "http_client.h"
#include "ota_define.h"
#include "image_handle.h"
#include "image_back.h"
#include "iot_ota.h"
#include "iot_mqtt.h"
#include "comm_define.h"
#include "filesystem.h"
#include "uart_ota.h"
#include "cJSON.h"
#include "webserver.h"
#include "parameter.h"
#include "modbus_data.h"
#include "modbus_define.h"
#include "xmodem_client.h"
#include "sync_time.h"
#include "iot_mqtt.h"
#include "iot_define.h"
#include "file_ota.h"
#include "bms_can.h"
#include "bms_ota.h"
#include "can_protocol.h"
#include "sync_time.h"
#include "grid_off_statistic.h"

#define TAG "[HTTPS]"

// 时间间隔定义
#define UPDATE_CHECK_INTERVAL_MINUTES   60   // 省钱参数检查间隔单位：分钟
#define UPDATE_CHECK_INTERVAL_MS        (UPDATE_CHECK_INTERVAL_MINUTES * 60 * 1000)  // 转换为毫秒

#define ESP_ERR_HTTPS_OTA_BASE            (0x9000)
#define ESP_ERR_HTTPS_OTA_IN_PROGRESS     (ESP_ERR_HTTPS_OTA_BASE + 1)  /* OTA operation in progress */

#define IMAGE_HEADER_SIZE       (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1)
#define DEFAULT_OTA_BUF_SIZE    IMAGE_HEADER_SIZE
#define DEFAULT_REQUEST_SIZE    (4 * FLASH_SEC_SIZE) // 分片下载 buffer大小

#if 0
// OTA更新 - 平衡性能和稳定性
#define MAX_HTTP_OTA_BUFFER 4096   // 4KB

// 网络较差的环境
#define MAX_HTTP_OTA_BUFFER 2048   // 2KB，减少单次传输风险

// 网络良好，需要高效率
#define MAX_HTTP_OTA_BUFFER 8192   // 8KB

// 内存受限的设备
#define MAX_HTTP_OTA_BUFFER 1024   // 1KB
#endif

#define MAX_HTTP_OTA_BUFFER     (4096)  // 4096
#define HTTPS_OTA_TIMEOUT       (10000) // 10s超时

#define VER_LIST_MAX_NUM  12

#define HTTPS_REQUEST_IMAGE_INTERFACE   "/api/blusmartprod/device/firmware/v1/latest/firmwareVerList/batch?gwcredentials=osUZ8ygqt1s%2FawsURLTwupGKN%2FCH8sRRODw%2FlZLlrv49jdBtu7UuRqxUkYHH6jWswlLPybPJ2WShH%2Fr928K10amSgY0pWE2%2BeeijxYovV9DIcRgwZhBbSQ%3D%3D"
#define HTTPS_OTA_STATE_INTERFACE       "/api/blusmartprod/device/upgrade/record/v1/save"
#define HTTPS_SAVE_COST_INTERFACE       "/api/bluiotdata/dashboard/v1/getElectricCostSaveStatistics"
#define HTTPS_DISASTER_INTERFACE        "/api/bluiotdata/device/iotData/v1/reportDeviceBackupTime"
#define HTTPS_SAVE_MONEY_INTERFACE      "/api/bluiotdata/dashboard/v1/getEnergyStorageElectricCostSave"
#define HTTP_REQUEST_IMAGE_IF_TEST      "/api/blusmartprod/device/firmware/v1/latest/firmwareVerList/batch"
#define HTTPS_GRID_OFF_INTERFACE        "/api/bluiotdata/device/iotData/v1/reportDeviceBlackoutData" // 旧版本暂不使用
#define HTTPS_GRID_OFF_BATCH_INTERFACE  "/api/bluiotdata/device/iotData/v1/reportDeviceBlackoutDataBatch"
#define HTTPS_GET_GRID_OFF_COUNT_INTERFACE  "/api/bluiotdata/device/iotData/v1/blackout/countStats"

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
    bool ota_fs_file;
    bool to_mesh;
    uint8_t type;
    char model[12];
    uint8_t dev_index;
    uint32_t curr_version;
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

typedef struct
{
    uint16_t type;
    uint32_t version;
}soft_ver_struct;

typedef struct
{
    bool have_new_version_flag;
    uint8_t softver_num;
    soft_ver_struct ver_list[VER_LIST_MAX_NUM];
}new_version_update_struct;

static image_info_t *pimage_head;

static bool is_update_check_triggered = false;
static bool need_query_recordId = false;
USE_EXT_RAM_BSS static char image_record_id[32] = {0};

static bool http_ota_doing = false;
static ota_cmd_list_t *ota_cmd_list = NULL;
USE_EXT_RAM_BSS ota_data_t2 ota_data;

USE_EXT_RAM_BSS static new_version_update_struct iot_version_update = {0};
USE_EXT_RAM_BSS static new_version_update_struct inv_version_update = {0};
USE_EXT_RAM_BSS static new_version_update_struct dcdc_version_update = {0};
USE_EXT_RAM_BSS static new_version_update_struct dchub_version_update = {0};

#ifdef BATTERY_TYPE_VER_STAT_ENABLE 
USE_EXT_RAM_BSS static new_version_update_struct pack_version_update[MODBUS_PACK_TYPE_MAX];
#else
USE_EXT_RAM_BSS static new_version_update_struct pack_version_update[PACK_MAX_NUM];
#endif

#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
USE_EXT_RAM_BSS static new_version_update_struct devassist_version_update = {0};
#endif

struct ifreq g_netif_req;

USE_EXT_RAM_BSS ota_cmd_list_t *new_firmware_iot_list = NULL;
USE_EXT_RAM_BSS ota_cmd_list_t *new_firmware_inv_list = NULL;
USE_EXT_RAM_BSS ota_cmd_list_t *new_firmware_dcdc_list = NULL;
USE_EXT_RAM_BSS ota_cmd_list_t *new_firmware_dchub_list = NULL;
USE_EXT_RAM_BSS ota_cmd_list_t *new_firmware_pack_list[MODBUS_PACK_TYPE_MAX] = {NULL};
USE_EXT_RAM_BSS ota_cmd_list_t *new_firmware_devassist_list = NULL;

#define	 LEN_TEMP_BUF	100


int https_new_image_check(uint8_t target) ;
static void check_firmware_updates(void);
static bool check_firmware_version(uint32_t new_version, uint32_t old_version, bool force);
static int https_ota_get_record_id(char recordId[32], char *state, char *reason, uint8_t image_type, uint32_t new_ver, uint32_t old_ver);
static int update_money_save_data(void);

static QueueHandle_t s_ota_report_queue = NULL;

/**
 * @brief  OTA 上报模块初始化（创建队列 + 启动上报任务）
 * @note   必须在首次调用 Http_OTA_Report_to_Cloud 之前执行
 */
static void Http_OTA_Report_Module_Init(void)
{
    if (s_ota_report_queue == NULL) {
        // 队列深度可按业务调整，4 条足够覆盖失败+成功等常规场景
        s_ota_report_queue = xQueueCreate(4, sizeof(uint8_t));
    }
}

/**
 * @brief 向云端上报 HTTPS OTA 状态（仅在有网络且存在 recordId 时入队）
 *
 * @param where  上报来源标识（调用方业务逻辑决定，若 where==3 则不上报）
 * @param pct    升级完成百分比（0-100）
 * @param errcode 错误码，非 0 表示失败
 * @return 1: 已入队并清除 recordId；0: 未上报或无效
 */
static uint8_t Http_OTA_Report_Push(uint8_t where, uint8_t pct, uint8_t errcode)
{
    /* 无网络不上报 */
    if (!iot_net_status_check()) return 0;

    /* 云端下载中，无需上报 */
    if ( where == 3 ) return 0;

    uint8_t status = 0xFF;
    
    /* 优先处理错误上报 */
    if (errcode) {
        status = errcode;
        if ( s_ota_report_queue ) xQueueSend(s_ota_report_queue, &status, 0);
    } else if (pct == 100) {
        /* 成功上报（完成百分比为100） */
        status = 0;
        if ( s_ota_report_queue ) xQueueSend(s_ota_report_queue, &status, 0);
    }

    return 1;
}

/**
 * @brief OTA 状态上报独立任务
 *        从队列消费消息，执行实际的 HTTPS 上报请求
 */
static void Http_OTA_Report_Process(void)
{
    uint8_t errcode = 0xFF;

    // 非阻塞等待队列消息
    if (s_ota_report_queue && xQueueReceive(s_ota_report_queue, &errcode, 0) != pdTRUE) {
        return;
    }

    image_info_t *image_info = get_image_info();
    if (image_info == NULL) return;

    /* recordId 为空则无需上报 */
    if (strlen(image_info->recordId) == 0) return;

    if ( errcode ) {
        char err_buf[32] = {0};
        snprintf(err_buf, sizeof(err_buf), "%u", (unsigned)errcode);
        ESP_LOGE(__func__, "HTTPS_OTA_FAILED(%u) report for recordId: %s", (unsigned)errcode, image_info->recordId);
        https_ota_state_to_host(image_info->recordId, HTTPS_OTA_FAILED, err_buf);
        memset(image_info->recordId, 0, sizeof(image_info->recordId));
    } else {
        ESP_LOGI(__func__, "HTTPS_OTA_SUCCESS report for recordId: %s", image_info->recordId);
        https_ota_state_to_host(image_info->recordId, HTTPS_OTA_SUCCESS, "");
        memset(image_info->recordId, 0, sizeof(image_info->recordId));
    }
}

/*------------------------------------------------------------------------------
 Function: current_ota_info_update
 copy from PLP025
 -----------------------------------------------------------------------------*/
/**
  * @brief      升级文件信息记录更新
  * @param[in]  uint8_t isOta          
                uint8_t type           
                uint8_t errCode        
                uint32_t soft_version  
  * @param[out] None
  * @return     void
  */
void current_ota_info_update(uint8_t isOta, uint8_t type, uint8_t pct, uint8_t errCode, uint32_t soft_version)
{
    if ( errCode != 0 )
    {
        reals.current_ota_info.isOta = 3; // 0：未升级，1：升级中，  2：升级成功 3：升级失败
    }
    else
    {
        if ( pct == 100 )
        {
            reals.current_ota_info.isOta = 2;
        }
        else
        {
            reals.current_ota_info.isOta = isOta;
        }
    }
    
    reals.current_ota_info.errCode = errCode;
    reals.current_ota_info.type = type;
    if(soft_version != 0) reals.current_ota_info.soft_version = soft_version;
}

// where = 1：IOT发送给设备，2：设备发送给子设备
void set_ota_pct_info(char* model, uint16_t addr, uint8_t type, uint8_t doing, 
                      uint8_t where, uint8_t pct,  uint8_t errcode) 
{    
    uint8_t repeat_flag = 0;
    uint8_t level = 1; // 传输深度 始终为1;
    
    if(model != NULL) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_id = addr; // 设备地址
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd_group.dev_id = addr; // 设备地址
        top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_cmd_group.dev_id = addr; // 设备地址
    }

    if (( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta == doing )
        &&( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].where == where )
        &&( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].pct == pct )
        &&( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode == errcode )
       )
    {
        repeat_flag = 1;
    }

	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta = doing; // 升级状态和升级阶段
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].where = where;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].type	= type;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].level = level; // 传输深度
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].pct  = pct; // 传输进度和错误码
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode = errcode; // 错误码

    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_group[0].isOta = doing; // 升级状态和升级阶段
    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_group[0].where = where;
    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_group[0].type	= type;
    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_group[0].level = level; // 传输深度
    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_group[0].pct  = pct; // 传输进度和错误码
    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_group[0].errCode = errcode; // 错误码

    // 更新本地升级记录缓存
    current_ota_info_update(doing, type, pct, errcode, 0);

    // 检查，避免重复上报和清零上报
    if (where && (!repeat_flag)) {
#ifdef CONFIG_MODBUS_21000_USE_ENABLE
        if ( errcode 
            || (( 100 == pct ) && (where == level)))
        {
            /*升级结果上报*/
            if(1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.ble_connect) reals.modbus_self_report_ble = 3;
            if(1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.mqtt) reals.modbus_self_report_mqtt = 3;
        }
#endif        
        /*升级结果云端上报*/
        Http_OTA_Report_Push(where, pct, errcode);
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

    char *upgrade_data_buf = (char *)iot_calloc(DEFAULT_OTA_BUF_SIZE);
    if (upgrade_data_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for upgrade buffer");
        return ESP_ERR_NO_MEM;
    }
    // process_again() returns true only in case of redirection.
    if (process_again(status_code)) {
        while (1) {
            /*
             *  In case of redirection, esp_http_client_read() is called
             *  to clear the response buffer of http_client.
             */
            int data_read = esp_http_client_read(http_client, upgrade_data_buf, DEFAULT_OTA_BUF_SIZE);
            if (data_read <= 0) {
                free(upgrade_data_buf);
                return ESP_OK;
            }
        }
    }
    
    free(upgrade_data_buf);
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
    return 0;
}

static esp_err_t _ota_write(http_ota_handle_t *https_ota_handle, const void *buffer, size_t buf_len)
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
    if (client != NULL) {
        ESP_LOGI(TAG, "Cleaning up HTTP client resources");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
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

int set_http_client_netif(uint8_t netif_type)
{
    if (netif_type >= 3)
    {
        return -1;
    }
    char *netif_key[3] = {NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP};

#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[netif_type]),
                                    g_netif_req.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[netif_type])),
                                g_netif_req.ifr_name);
#endif
    return 0;
}

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

    uint8_t *buffer = iot_calloc(1024);
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
    esp_err_t err;

    http_ota_handle_t *https_ota_handle = iot_calloc(sizeof(http_ota_handle_t));
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

    https_ota_handle->max_http_request_size = (ota_config->max_http_request_size == 0) ? DEFAULT_REQUEST_SIZE : ota_config->max_http_request_size; // 一次http请求的最大长度
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
            if (asprintf(&header_val, "bytes=0-%lu", https_ota_handle->max_http_request_size - 1) == -1) {
                ESP_LOGE(TAG, "Failed to allocate memory for HTTP header");
                err = ESP_ERR_NO_MEM;
                goto http_cleanup;
            }
            esp_http_client_set_header(https_ota_handle->http_client, "Range", header_val); // 设置下载范围
            free(header_val);
            header_val = NULL;
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
    ESP_LOGI(TAG, "HTTP OTA alloc_size: %d", alloc_size);
    https_ota_handle->ota_upgrade_buf = (char *)iot_calloc(alloc_size);
    if (https_ota_handle->ota_upgrade_buf != NULL) {
        // memset( https_ota_handle->ota_upgrade_buf, 0, alloc_size); // 不需要，calloc已初始化为0
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
                ESP_LOGI(TAG, "IOT image ready erase flash");
                const int erase_size = handle->bulk_flash_erase ? OTA_SIZE_UNKNOWN : OTA_WITH_SEQUENTIAL_WRITES;
                err = esp_ota_begin(handle->update_partition, erase_size, &handle->update_handle);
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
                
                err = iot_image_erase(handle->image_flash_offset, handle->image_flash_size);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "image flash erase failed addr: 0x%04lx, len: %ld",
                                    handle->image_flash_offset, handle->image_flash_size);
                    return err;
                } 
                handle->state = ESP_HTTPS_OTA_IN_PROGRESS;
                return ESP_ERR_HTTPS_OTA_IN_PROGRESS;
            }   
            break;
            
        case ESP_HTTPS_OTA_IN_PROGRESS: // 从http 服务器get指定长度的数据
            // ESP_LOGI(TAG, "ESP_HTTPS_OTA_IN_PROGRESS");
            data_read = esp_http_client_read(handle->http_client,
                                             handle->ota_upgrade_buf,
                                             handle->ota_upgrade_buf_size);
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
                    // ESP_LOGD(TAG, "Connection closed");
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
                
                // 错误补全：uint16_t padding_len = (1024 - handle->binary_file_len % 1024);
                uint16_t padding_len = (1024 - handle->binary_file_len % 1024) % 1024;
                if (padding_len) { // 补全1024
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
            if (handle->ota_esp_self) {
                // 结束OTA
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
                handle->ota_upgrade_buf = NULL;
            }
            if (handle->http_client) {
                _http_cleanup(handle->http_client);
                handle->http_client = NULL;
            }
            break;
        default:
            ESP_LOGE(TAG, "Invalid ESP HTTPS OTA State");
            break;
    }

    if ((err == ESP_OK) && (handle->state == ESP_HTTPS_OTA_SUCCESS)) {
        if (handle->ota_esp_self) {
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
                handle->ota_upgrade_buf = NULL;
            }
            if (handle->http_client) {
                _http_cleanup(handle->http_client);
                handle->http_client = NULL;
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
        ESP_LOGI(TAG, "Image download: %3lu, total: %3lu, pct: %hhu%%", 
                        https_ota_handle->binary_file_len, https_ota_handle->image_length, pct);
#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE
        reals.system_ota.ota_total_pct = System_ota_total_pct_update(pct, true, reals.system_ota.ota_mcu_curr_count, reals.system_ota.ota_mcu_total_count, config->ota_esp_self);
#endif        
        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            set_ota_pct_info(config->model, config->dev_target_id, config->type, 1, 3, pct, OTA_ERR_NORMAL); // 升级进度填充到modbus中
#ifdef CONFIG_WEBSERVER_ENABLE            
            if (ota_object->method == OTA_METHOD_WEBSERVER) {
                web_server_upgrade_state_report(UPGRADE_STATE_DOING, pct);
            }
#endif            
        } else { // 文件下载异常退出
            break;
        }
    }

    if (err != ESP_OK) { // 传输失败中断升级
        https_ota_abort(https_ota_handle);
        set_ota_pct_info(config->model, config->dev_target_id, config->type, 0, 3, pct, OTA_ERR_ABORT); // 升级进度填充到modbus中
#ifdef CONFIG_WEBSERVER_ENABLE        
        if (ota_object->method == OTA_METHOD_WEBSERVER) {
            web_server_upgrade_state_report(UPGRADE_STATE_FAIL, pct);
        }
#endif        
        return err;
    }

    esp_err_t ota_finish_err = https_ota_finish(https_ota_handle); // 升级完成检查
    if (ota_finish_err != ESP_OK) {
        set_ota_pct_info(config->model, config->dev_target_id, config->type, 0, 3, pct, OTA_ERR_FILE); // 升级进度填充到modbus中
#ifdef CONFIG_WEBSERVER_ENABLE        
        if (ota_object->method == OTA_METHOD_WEBSERVER) {
            web_server_upgrade_state_report(UPGRADE_STATE_FAIL, pct);
        }
#endif        
        return ota_finish_err;
    }
    return ESP_OK;
}

static bool check_version_and_model(ota_object_t ota_obj, uint32_t version, char* type, uint64_t sn, char* dev_model_ascii) 
{
    if (ota_obj.version / 100 != version / 100) 
    {
        return false;
    }
    
    if ((!ota_obj.force) && (ota_obj.version % 100 < version % 100)) 
    {
        return false;
    }
    
    return true;
}

static int https_ota_cmd_verify(ota_object_t ota_obj, http_ota_config_t *ota_config) 
{
    bool can_upgrade = false;
    // uint8_t i = 0;
    
    ota_config->dev_target_id = 0;
    ota_config->ota_esp_self = false;
    ota_config->ota_fs_file = false;
    ota_config->to_mesh = false;
    ota_config->type = ota_obj.type;
    memcpy(ota_config->model, ota_obj.model, strnlen(ota_obj.model, TYPE_SIZE));

    if (ota_obj.type >= TypeCnt) {
        ESP_LOGE(TAG, "https_ota_cmd_verify : file type unknown, file id = %d", ota_obj.type);
        return -1;
    } else {
        memcpy(pimage_head->recordId, ota_obj.recordId, 32);

        if (ota_obj.type == DEVICE_IOT) 
        {
            can_upgrade = check_version_and_model(ota_obj, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver, iot_factory.iot_type, iot_factory.iot_sn, NULL);
            if (can_upgrade) {
                ota_config->curr_version = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver;
                ota_config->ota_esp_self = true;
                ota_data.cmd.target.bit.group = GROUP_IOT;
            } else {
                /*非本机，检查CAN总线IOT设备，如charger2*/
                for(int i = 0; i < DCDC_MAX_NUM; i++)
                {
                    can_upgrade = check_version_and_model(ota_obj, top_modbus_rd.Dcdc[i].mod_reg11000_IOT_info.software_ver, 
                                                    top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, 
                                                    top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn, 
                                                    NULL);
                    
                    if(can_upgrade == true){
                        ota_config->curr_version = top_modbus_rd.Dcdc[i].mod_reg11000_IOT_info.software_ver;
                        ota_data.cmd.target.bit.group = GROUP_CHARGE;
                        ota_config->dev_can_id = DCDC_IOT_CAN_ADDR;
                        goto SUCCESS;
                    }
                } 
            }
        } 
        else if ((ota_obj.type == DEVICE_ARM))
        {
            /*INV*/
            for(int i = 0; i < top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.software_total; i++)
            {
                modbus_soft_version *target = NULL;
                if(i < MD_SOFT_MAIN_CAP) {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft[i];
                } else {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft_back[i-MD_SOFT_MAIN_CAP];
                }
                
                if ( target ) {
                    if (ota_obj.type == target->type)
                    {
                        can_upgrade = check_version_and_model(ota_obj, target->version, 
                                                                top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.InvType, 
                                                                top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.InvSN, 
                                                                NULL);
                        
                        if(can_upgrade == true){
                            ota_config->curr_version = target->version;
                            ota_data.cmd.target.bit.group = GROUP_INV;
                            ota_config->dev_can_id = INV_CAN_ADDR;
                            goto SUCCESS;
                        }
                    }
                }
            }
        }
        else if ((ota_obj.type == DEVICE_DSP))
        {
            /*INV*/
            for(int i = 0; i < top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.software_total; i++)
            {
                modbus_soft_version *target = NULL;
                if(i < MD_SOFT_MAIN_CAP) {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft[i];
                } else {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft_back[i-MD_SOFT_MAIN_CAP];
                }
                
                if ( target ) {
                    if (ota_obj.type == target->type)
                    {
                        can_upgrade = check_version_and_model(ota_obj, target->version, 
                                                                top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.InvType, 
                                                                top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.InvSN, 
                                                                NULL);
                        
                        if(can_upgrade == true){
                            ota_config->curr_version = target->version;
                            ota_data.cmd.target.bit.group = GROUP_INV;
                            ota_config->dev_can_id = INV_CAN_ADDR;
                            goto SUCCESS;
                        }
                    }
                }
            }
            
            /*DCDC*/
            for(int i = 0; i < DCDC_MAX_NUM; i++)
            {
                if ( ota_obj.type == top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareType )
                {
                    can_upgrade = check_version_and_model(ota_obj, top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareVersion, 
                                                    top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, 
                                                    top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn, 
                                                    NULL);
                    
                    if(can_upgrade == true){
                        ota_config->curr_version = top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareVersion;
                        ota_data.cmd.target.bit.group = GROUP_CHARGE;
                        ota_config->dev_can_id = DCDC_ARM_CAN_ADDR;
                        goto SUCCESS;
                    }
                }
            } 
        }
        else if ((ota_obj.type == DEVICE_PACK_M1))
        {
            /*INV*/
            for(int i = 0; i < top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.software_total; i++)
            {
                modbus_soft_version *target = NULL;
                if(i < MD_SOFT_MAIN_CAP) {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft[i];
                } else {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft_back[i-MD_SOFT_MAIN_CAP];
                }
                
                if ( target ) {
                    if (ota_obj.type == target->type)
                    {
                        can_upgrade = check_version_and_model(ota_obj, target->version, 
                                                                top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.InvType, 
                                                                top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.InvSN, 
                                                                NULL);
                        
                        if(can_upgrade == true){
                            ota_data.cmd.target.bit.group = GROUP_INV;
                            ota_config->dev_can_id = INV_CAN_ADDR;
                            goto SUCCESS;
                        }
                    }
                }
            }

            /*Pack*/
            for ( uint8_t index = 0 ; index < (PACK_MAX_NUM) ; index++ )
            {
                for(int i = 0; i < top_modbus_rd.Pack[index].mod_reg06100_Pack_each.software_total; i++)
                {
                    if (ota_obj.type == top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].type)
                    {
                        // 打印版本号
                        ESP_LOGI(TAG, "Pack[%d] version: %lu", index, top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].version);
                        can_upgrade = check_version_and_model(ota_obj, top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].version, 
                                                                top_modbus_rd.Pack[index].mod_reg06100_Pack_each.type_ascii, 
                                                                top_modbus_rd.Pack[index].mod_reg06100_Pack_each.sn_code, 
                                                                NULL);
                        
                        if(can_upgrade == true){
                            ota_config->curr_version = top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].version;
                            ota_data.cmd.target.bit.group = GROUP_PACK;
                            ota_config->dev_can_id = PACK_CAN_ADDR;
                            goto SUCCESS;
                        }
                    }
                }
            }
        }
        else if (( ota_obj.type == DEVICE_HMI1 ) || ( ota_obj.type == DEVICE_HMI2 ))
        {
#ifdef CONFIG_FILE_DATA_OTA_ENABLE         
            can_upgrade = true;
            ota_config->ota_fs_file = true;
            ota_data.cmd.target.bit.group = GROUP_IOT;
#endif            
        }
        else if ((ota_obj.type == DEVICE_DC_HUB))
        {
            /*DCHUB*/
            for(int i = 0; i < DC_HUB_MAX_NUM; i++)
            {
                can_upgrade = check_version_and_model(ota_obj, top_modbus_rd.DcHub[i].mod_reg01100_Inv_base.soft[0].version, 
                                                top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_type, 
                                                top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_sn, 
                                                NULL);
                
                if(can_upgrade == true){
                    ota_config->curr_version = top_modbus_rd.DcHub[i].mod_reg01100_Inv_base.soft[0].version;
                    ota_data.cmd.target.bit.group = GROUP_INV;
                    ota_config->dev_can_id = DC_HUB_CAN_ADDR;
                    goto SUCCESS;
                }
            } 
        }
        else if ((ota_obj.type == DEVICE_DC_DC))
        {
            /*DCDC*/
            for(int i = 0; i < DCDC_MAX_NUM; i++)
            {
                if ( ota_obj.type == top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareType )
                {
                    can_upgrade = check_version_and_model(ota_obj, top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareVersion, 
                                                    top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, 
                                                    top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn, 
                                                    NULL);
                    
                    if(can_upgrade == true){
                        ota_config->curr_version = top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareVersion;
                        ota_data.cmd.target.bit.group = GROUP_CHARGE;
                        ota_config->dev_can_id = DCDC_ARM_CAN_ADDR;
                        goto SUCCESS;
                    }
                }
            } 
        }
        else if ((ota_obj.type == DEVICE_BMS)||(ota_obj.type == DEVICE_PACK_BCU)
            ||(ota_obj.type == DEVICE_PACK_BMU)||(ota_obj.type == DEVICE_PACK_BMS)
            ||(ota_obj.type == DEVICE_PACK_SAFTY))
        {
            for ( uint8_t index = 0 ; index < (PACK_MAX_NUM) ; index++ )
            {
                for(int i = 0; i < top_modbus_rd.Pack[index].mod_reg06100_Pack_each.software_total; i++)
                {
                    if (ota_obj.type == top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].type)
                    {
                        // 打印版本号
                        ESP_LOGI(TAG, "Pack[%d] version: %lu", index, top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].version);
                        can_upgrade = check_version_and_model(ota_obj, top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].version, 
                                                                top_modbus_rd.Pack[index].mod_reg06100_Pack_each.type_ascii, 
                                                                top_modbus_rd.Pack[index].mod_reg06100_Pack_each.sn_code, 
                                                                NULL);
                        
                        if(can_upgrade == true){
                            ota_config->curr_version = top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].version;
                            ota_data.cmd.target.bit.group = GROUP_PACK;
                            ota_config->dev_can_id = PACK_CAN_ADDR;
                            goto SUCCESS;
                        }
                    }
                }
            }
        }

        else 
        {
            can_upgrade = false;
        }
        
        SUCCESS:
        
        if (can_upgrade) {  
            ota_config->dev_target_id = 0xFF;//ota_config->dev_can_id;
            
            ota_data.cmd.type = ota_obj.type;
            ota_data.cmd.version = ota_obj.version;
            ota_data.cmd.target.bit.target = ota_config->dev_target_id;
            ota_mode = OTA_FULL_BROADCAST;

            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type = ota_data.cmd.target.bit.group;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd_group.dev_type = ota_data.cmd.target.bit.group;
            top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA.ota_cmd.group.dev_type = ota_data.cmd.target.bit.group;
        } else {
            ESP_LOGE(TAG, "Mismatch type(%d), version(%lu)", ota_obj.type, ota_obj.version);
        }
    }
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

/*------------------------------------------------------------------------------
 Function: http_period_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      HTTP周期更新
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void http_period_update(void)
{
    static uint32_t pre_dst_update_time = 0;
    static uint32_t pre_save_money_update_time = 0;
    static uint32_t pre_firmware_check_time = 0;
    static bool first_save_money = true;
    
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    /*仅在连接服务器（即网络可用）运行*/
    if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.mqtt ) 
    {
        /*云端校时*/
        if(((now_time - pre_dst_update_time) >= H24_HOUR_INTERVAL) && (!sys_is_updating()))
        {
#if TIME_GET_FROM_SERVER_PERIOD_CHL == 1       
            if ( 2 != IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update ) {
                http_dst_time_update();
            }
#endif            
            pre_dst_update_time = now_time;
        }
        else if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.MqttConnectMode != 3 )
        {
            // 仅在非4G网络情况下执行如下周期检查
#ifdef HTTP_GET_MONEY_SAVE_DATE        
            /*更新节省金额和温度单位数据*/
            if(((now_time - pre_save_money_update_time) >= ONE_HOUR_INTERVAL) || first_save_money)
            { 
                if(!reals.IOT_Status_Flag.sBit.system_sleep_flag) update_money_save_data();
                pre_save_money_update_time = now_time;
                first_save_money = false;
            }
#endif        
#ifdef HTTP_CHECK_FIRMWARE_UPDATE        
            /*更新固件版本信息*/
            else if((now_time - pre_firmware_check_time) >= H24_HOUR_INTERVAL)
            { 
                if(!reals.IOT_Status_Flag.sBit.system_sleep_flag) update_firmware_check();
                pre_firmware_check_time = now_time;
            }
#endif 
#ifdef CONFIG_GRID_OFF_COUNT_ENABLE 
            /*更新断电记录数据信息*/
            else if(Grid_Off_Report_Task())
            { 
                // 内部存在上报间隔处理，此处不做额外处理
            }
#endif
        } 
    }
    else
    {   
        /*未连接时暂停计时*/
        pre_dst_update_time = now_time;
        pre_firmware_check_time = now_time;
        pre_save_money_update_time = now_time;
        first_save_money = true;
    }
}


bool https_new_image_timeout_clean_flag = false; 

static USE_EXT_RAM_BSS ota_object_t ota_object = {0};

/*------------------------------------------------------------------------------
 Function: https_period_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      HTTP线程
  * @param[in]  void *pvParameters  
  * @param[out] None
  * @return     static void
  */
void https_period_task(void *pvParameters) 
{
    // 跳过证书验证，
    // 选中 menuconfig->ESP-TLS->Allow potentially insecure options
    // 选中 menuconfig->ESP-TLS->Skip server certificate verification by default (WARNING: ONLY FOR TESTING PURPOSE, READ HELP)
        
    http_ota_config_t ota_config = {
        .ota_esp_self = false,
        .ota_fs_file = false,
        .to_mesh = false,
        .bulk_flash_erase = false, // 按照块擦除 暂未使用
        .partial_http_download = false, // 文件分片下载
        //.max_http_request_size = DEFAULT_REQUEST_SIZE,
        .image_flash_offset = IMAGE_FLASH_AREA_ADDRESS,
        .image_flash_size   = IMAGE_FLASH_AREA_MAX_LEN,
        .image_flags_offset = IMAGE_HEAD_AREA_ADDRESS,
        .image_flags_size   = IMAGE_HEAD_AREA_MAX_LEN,
    };
        
    pimage_head = get_image_info();

    /*OTA 上报模块初始化*/
    Http_OTA_Report_Module_Init();
    
    while (1) 
	{   
        vTaskDelay(pdMS_TO_TICKS(100));

        /*OTA 状态上报（透传段）*/
        Http_OTA_Report_Process();
        
        /*检查是否正在升级*/
        if (sys_is_updating()) continue;
        
        /*检查后台是否给我们推送新的版本信息*/
        if (http_ota_have_new_image(&ota_object) < 0) 
		{
            /*HTTP周期数据更新*/
            http_period_update();
            
#ifdef HTTP_CHECK_FIRMWARE_UPDATE
            if(is_update_check_triggered == true) 
            {
                ESP_LOGI(TAG, "checking new image...");
                is_update_check_triggered = false;
                reals.need_new_version_flag.sBit.iot = 1;
                if(0 != reals.online_Inv_num) reals.need_new_version_flag.sBit.inv = 1;
                if(0 != reals.online_Pack_num) reals.need_new_version_flag.sBit.pack = 1;
                if(0 != reals.online_DCHUB_num) reals.need_new_version_flag.sBit.dchub = 1;
                if(0 != reals.online_Dcdc_num) reals.need_new_version_flag.sBit.dcdc = 1;
            }

            /*升级延时结束*/
            if ( !https_new_image_timeout_clean_flag )
            {            
                if (DEVASSIST_OTA_READY == reals.supper_control.ota_status) {
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE                    
                    /* 超级命令模式下仅检查网络，不对服务器连接状态强制要求*/
                    if (!iot_net_status_check()) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        continue;
                    }
#else
                    reals.supper_control.ota_status = DEVASSIST_OTA_FAIL;
                    continue;
#endif
                } else {
                    /*仅在连接服务器（即网络可用）时运行*/
                    if(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.mqtt != 1) { 
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        continue;
                    }
                    
                    /*4G模式下禁用新版本检查，节约流量*/
                    if ( 3 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.MqttConnectMode ) {
                        reals.need_new_version_flag.Byte2 = 0;
                        continue ;
                    }
                }
                
                // 检查列表，有新标志时延时500ms，确保数据从CAN向Modbus更新完成
                if( reals.need_new_version_flag.Byte2 ) vTaskDelay(pdMS_TO_TICKS(500)); 
                switch ( find_first_nonzero_bit(reals.need_new_version_flag.Byte2) )
                {
                    case HTTPS_CHECK_IOT_IMAGE : 
                        if(0 == https_new_image_check(HTTPS_CHECK_IOT_IMAGE))reals.need_new_version_flag.sBit.iot = 0; 
#ifdef CONFIG_WEBSERVER_ENABLE
                        // 通知webserver前端显示.
                        web_server_version_report();
#endif
                        vTaskDelay(pdMS_TO_TICKS(100)); break;
                    case HTTPS_CHECK_INV_IMAGE : 
                        if(0 == https_new_image_check(HTTPS_CHECK_INV_IMAGE))reals.need_new_version_flag.sBit.inv = 0; 
                        vTaskDelay(pdMS_TO_TICKS(100)); break;
                    case HTTPS_CHECK_PACK_IMAGE : 
                        if(0 == https_new_image_check(HTTPS_CHECK_PACK_IMAGE))reals.need_new_version_flag.sBit.pack = 0;
                        vTaskDelay(pdMS_TO_TICKS(100)); break;
                    case HTTPS_CHECK_DCDC_IMAGE : 
                        if(0 == https_new_image_check(HTTPS_CHECK_DCDC_IMAGE))reals.need_new_version_flag.sBit.dcdc = 0;
                        vTaskDelay(pdMS_TO_TICKS(100)); break;
                    case HTTPS_CHECK_DCHUB_IMAGE : 
                        if(0 == https_new_image_check(HTTPS_CHECK_DCHUB_IMAGE))reals.need_new_version_flag.sBit.dchub = 0;
                        vTaskDelay(pdMS_TO_TICKS(100)); break;
                    case HTTPS_CHECK_DEVASSIST : 
                        if(0 == https_new_image_check(HTTPS_CHECK_DEVASSIST))reals.need_new_version_flag.sBit.devassist = 0; 
                        vTaskDelay(pdMS_TO_TICKS(100)); break;
                    default: reals.need_new_version_flag.Byte2 = 0; break;
                }

#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
                // 版本检查完毕后再进行升级
                if ((DEVASSIST_OTA_READY == reals.supper_control.ota_status) 
                    && (0 == reals.need_new_version_flag.sBit.devassist))
                {
                    static uint8_t request_list_cnt = 0;
                    if ( new_firmware_devassist_list ) {
                        if(firmware_upgrade_start(NULL, NULL, OTA_METHOD_DEVASSIST) > 0)  {
                            // 升级请求处理完成
                            request_list_cnt = 0;
                            ESP_LOGI(TAG, "DEVASSIST_OTA : firmware_upgrade_start Success!");
                        } else {
                            // 升级请求处理失败
                            reals.supper_control.ota_status = DEVASSIST_OTA_FAIL;
                            ESP_LOGE(TAG, "DEVASSIST_OTA : firmware_upgrade_start Error!");
                        }
                    } else {
                        // 新版本获取异常，有限次重试
                        if ( request_list_cnt < 3 ) {
                            request_list_cnt++;
                            ESP_LOGE(TAG, "DEVASSIST_OTA : new_firmware_iot_list request retry(%d)", request_list_cnt);
                            reals.need_new_version_flag.sBit.devassist = 1; 
                        } else {
                            reals.supper_control.ota_status = DEVASSIST_OTA_FAIL;
                            ESP_LOGE(TAG, "DEVASSIST_OTA : new_firmware_iot_list request failed!");
                        }
                    }
                }
#endif
            }
#endif             
            continue ; 
        }

        // 特殊方式触发升级
        if (need_query_recordId == true)
        {
            need_query_recordId = false;
            if (DEVASSIST_OTA_READY == reals.supper_control.ota_status) {
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
                // 升级触发来源为devassist
                ota_object.method = OTA_METHOD_DEVASSIST;
#else
                ESP_LOGE(TAG, "DEVASSIST_OTA : Unsupport!");
                continue;
#endif
            } else {
#ifdef CONFIG_WEBSERVER_ENABLE                
                // 升级触发来源为webserver
                ota_object.method = OTA_METHOD_WEBSERVER;
#else
                ESP_LOGE(TAG, "WEBSERVER_OTA : Unsupport!");
                continue;
#endif
            }
        } else {
            // 云端正常下发升级指令
            if ( ota_object.force ) {
                // 升级触发来源为服务器强制升级
                ota_object.method = OTA_METHOD_CLOUD;
            } else {
                // 升级触发来源为APP正常触发
                ota_object.method = OTA_METHOD_APP;
            }
        }

        ESP_LOGI(TAG, "http_ota : have_new_image, source(%d)", ota_object.method);
        
        current_ota_info_update(1, ota_object.type, 0, 0, ota_object.version);
        memset(&ota_data, 0, sizeof(ota_data));

#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE    
        reals.system_ota.ota_mcu_curr_count++;
#endif        
        /*新固件机型，SN，版本等检查*/
        if (https_ota_cmd_verify(ota_object, &ota_config) < 0) 
		{
#ifdef CONFIG_WEBSERVER_ENABLE            
            if (ota_object.method == OTA_METHOD_WEBSERVER) {
                web_server_upgrade_state_report(UPGRADE_STATE_FAIL, 0);
            }
#endif

#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
            if (ota_object.method == OTA_METHOD_DEVASSIST) {
                reals.supper_control.ota_status = DEVASSIST_OTA_FAIL;
            }
#endif  
            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_IMAGE_ERROR, "invalid image");
            set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 3, 0, OTA_ERR_FILE); // 升级进度填充到modbus中
            vTaskDelay(pdMS_TO_TICKS(100));
            continue ;
        }
        else
        {
#ifdef CONFIG_WEBSERVER_ENABLE            
            if (ota_object.method == OTA_METHOD_WEBSERVER) {
                web_server_upgrade_state_report(UPGRADE_STATE_DOING, 0);
            }
#endif

#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
            if (ota_object.method == OTA_METHOD_DEVASSIST) {
                reals.supper_control.ota_status = DEVASSIST_OTA_DOING;
            }
#endif 
            set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 1, 3, 0, OTA_ERR_NORMAL); // 升级进度填充到modbus中
        }

        // 设备端主动发起的升级要单独获取记录ID
        if ( (ota_object.method == OTA_METHOD_DEVASSIST) 
            || (ota_object.method = OTA_METHOD_WEBSERVER) )        
        {
            // 获取升级记录ID
            memset(image_record_id, 0, 32);
            memset(ota_object.recordId, 0, 32);
            if (https_ota_get_record_id(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ING, "", 
                                        ota_object.type, ota_object.version, ota_config.curr_version) == 0)
            {
                if (strlen(image_record_id) == 0) {
                    // TODO：获取失败仍继续升级，不产生记录？
                    ESP_LOGW(TAG, "Lost record! Continue upgrade");
                } else {
                    memcpy(ota_object.recordId, image_record_id, 32);
                    memcpy(pimage_head->recordId, ota_object.recordId, 32);
                }
            } else {
                // TODO：获取失败仍继续升级，不产生记录？
                ESP_LOGW(TAG, "Get record Error! Continue upgrade");
            }
        }
            
        esp_http_client_config_t http_config = { // 配置https登录服务器信息
            .method = HTTP_METHOD_GET, // get请求，从https服务器下载文件
            .url = ota_object.url,
            .keep_alive_enable = true,
            .buffer_size = MAX_HTTP_OTA_BUFFER,
            .timeout_ms = HTTPS_OTA_TIMEOUT, // 设置超时时间
            .if_name = &g_netif_req,
        };

        ota_config.http_config = &http_config;
        http_ota_doing = true;  // https ota启动
        https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ING, "");
        set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 1, 3, 0, OTA_ERR_NORMAL); // 升级进度填充到modbus中
        ESP_LOGI(TAG, "http_ota_doing start");
        uint32_t len;
        esp_err_t err = https_ota_handle_func(&ota_object, &ota_config, &len);
        if ( err == ESP_OK ) 
		{
            vTaskDelay(pdMS_TO_TICKS(200));
            if (ota_config.ota_esp_self == true) 
			{ 
                http_ota_doing = false;
                ota_config.ota_esp_self = false; // IOT-自己升级
                memcpy(pimage_head->recordId, ota_object.recordId, 32);
                set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 3, 100, OTA_ERR_NORMAL); // 升级进度填充到modbus中
                vTaskDelay(pdMS_TO_TICKS(500));
                set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 1, 100, OTA_ERR_NORMAL); // 这里没有第三段,模拟出第3段
                https_ota_state_to_host(pimage_head->recordId, HTTPS_OTA_SUCCESS, "");
#ifdef CONFIG_WEBSERVER_ENABLE                
                if (ota_object.method == OTA_METHOD_WEBSERVER) {
                    web_server_upgrade_state_report(UPGRADE_STATE_SUCCESS, 100);
                }
#endif
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
                if (ota_object.method == OTA_METHOD_DEVASSIST) {
                    reals.supper_control.ota_status = DEVASSIST_OTA_SUCCESS;
                }
#endif 
                ESP_LOGI(TAG, "\r\n\r\n Delay 5s Prepare to restart system....");
                uint8_t reset_cnt = 4;
                while ( reset_cnt-- )
                {
                    reals.modbus_self_report_mqtt = 3;
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                set_mqtt_stop(); 
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart(); 
                while(1);
            } 
			else 
			{
	            ESP_LOGI(TAG, "flash addr: %lu, len: %lu", IMAGE_FLASH_AREA_ADDRESS, ota_object.total_bytes);
	            if (https_ota_crc_verify(IMAGE_FLASH_AREA_ADDRESS, ota_object.total_bytes) != ESP_OK) {
	                set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 3, 0, OTA_ERR_CRC32); // 升级进度填充到modbus中
	                https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "image crc32 verify failed");
	                http_ota_doing = false;
	                continue ;
	            } 
            }

            ota_data.cmd.kbytes = (ota_object.total_bytes + 1023) / 1024;
                       
            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_OK, "");
            vTaskDelay(pdMS_TO_TICKS(200));

            if ( true == ota_config.ota_fs_file )
            {
#ifdef CONFIG_FILE_DATA_OTA_ENABLE            
                if (Start_File_OTA_Task(ota_object.total_bytes) != ESP_OK) {
                    ESP_LOGE(TAG, "File OTA Init fail");
                    https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_FAILED, "File OTA Init fail");
                    break;
                }
                https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_UPGRADING, "");
#else
                ESP_LOGE(TAG, "File OTA Unable.");
                https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_FAILED, "File OTA Unable");
                break;
#endif
            }
            else
            {
                switch (ota_object.type) // 本机升级
                {
                    case DEVICE_IOT: 
                    case DEVICE_ARM: 
                    case DEVICE_DSP: 
                    case DEVICE_DC_HUB: 
                    case DEVICE_DC_DC:     
                    case DEVICE_BMS: 
                    case DEVICE_PACK_M1:
#ifdef CONFIG_MCU_AUTO_UPDATE_IN_BOOT
                        uint8_t index = back_image_type_to_index(ota_data.cmd.type);
                        if ( index ) {
                            if (((ota_data.cmd.kbytes * 1024) <= IMAGE_CUSTOM_DATA_MAX) 
                                && ((ota_data.cmd.version / 100) % 100 != 0)) {
                                Start_Back_Image_Task(index, ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.target & 0xFF), len);
                            }
                        }
#endif
                        if (can_ota_data_init(ota_data.cmd.type, ota_data.cmd.version, ota_data.cmd.kbytes, (ota_data.cmd.target.bit.target & 0xFF), len) != 0) {
                            ESP_LOGE(TAG, "OTA Init fail");
                            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_FAILED, "OTA Init fail");
                            break;
                        }
                        https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_UPGRADING, "");
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE                        
                        if(!is_bms_alpha_update_protocol(ota_data.cmd.version/100)) // 第二代升级协议
#else
                        if (1)
#endif
                        {
                            if (can_ota_begin((ota_data.cmd.target.bit.target & 0xFF), ota_data.cmd.type, ota_data.cmd.version) != 0) //ota_config.dev_can_id 使用全广播地址
                            {
                                ESP_LOGE(TAG, "OTA Begin fail");
                                https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_FAILED, "no device upgrade!");
                                can_ota_status.ready_timeout = 30000;   // 30s
                            }
                            else
                            {
                                ESP_LOGI(TAG, "can_ota_begin success! ");                              
                            }
                        }
                        else // alpha升级协议
                        {                            
                            ESP_LOGI(TAG, "can alpha ota start! ");                        
                            if (0 != bms_ota_init(ota_data.cmd.type, ota_data.cmd.kbytes, 0, get_addr_trace(ota_data.cmd.target.bit.target), ota_data.cmd.version))   
                            {
                                ESP_LOGE(TAG, "OTA Begin fail");
                                https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_FAILED, "no device upgrade!");
                                can_ota_status.ready_timeout = 30000;   // 30s
                            }
                            else
                            {
                                ESP_LOGI(TAG, "bms_ota_init success! ");
                                IotSetData.dev_info_t.bms_ota_info.ota_state = OTA_STATE_START;
                                IotSetData.dev_info_t.bms_ota_info.ota_type = ota_data.cmd.type;
                                IotSetData.dev_info_t.bms_ota_info.version = ota_data.cmd.version;
                                IotSetData.dev_info_t.bms_ota_info.target_id = (ota_data.cmd.target.bit.target & 0xFF);
                                IotSetData.dev_info_t.bms_ota_info.file_size = ota_data.cmd.kbytes;
                                
                                reals.SetDataWrFlag.sBit.update_status = 1;
                            }
                        }

                        break;
    
                    default: 
                        ESP_LOGE(TAG, "Ready xmodem ERROR : pimage_head->type(%d)",ota_object.type); 
                        https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "not supported image type"); 
                        set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 3, 0, OTA_ERR_FILE); // 升级进度填充到modbus中
#ifdef CONFIG_WEBSERVER_ENABLE
                        if (ota_object.method == OTA_METHOD_WEBSERVER) {
                            web_server_upgrade_state_report(UPGRADE_STATE_FAIL, 0);
                        }
#endif   
                        break;
                }
            }
        }
        else 
        {
            set_ota_pct_info(ota_config.model, ota_config.dev_target_id, ota_config.type, 0, 3, 0, OTA_ERR_FILE_NULL); // 升级进度填充到modbus中
            https_ota_state_to_host(ota_object.recordId, HTTPS_OTA_DOWNLOAD_ERR, "download failed");
#ifdef CONFIG_WEBSERVER_ENABLE
            if (ota_object.method == OTA_METHOD_WEBSERVER) {
                web_server_upgrade_state_report(UPGRADE_STATE_FAIL, 0);
            }
#endif            
        }
        http_ota_doing = false; // https ota结束
    }
}

// 生成32位随机数字符串
static void generate_random_string(char *str, size_t len) 
{
    for (int i = 0; i < len; i++) 
    {
        str[i] = esp_random() % 26 + 'A';
    }
    str[len-1] = '\0'; // 添加字符串结尾的零
}

// SHA256加密函数
static void sha256_Encry(const char *input, char *output) 
{
    mbedtls_sha256_context sha256_ctx;
    unsigned char hash[32]; // SHA-256的输出是32字节
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0); // 初始化SHA256上下文
    mbedtls_sha256_update(&sha256_ctx, (const unsigned char *)input, strlen(input));
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    // 将32字节的hash转换为64字符的十六进制字符串

    snprintf(output, 70, "Sign %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
              hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
              hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15],
              hash[16], hash[17], hash[18], hash[19], hash[20], hash[21], hash[22], hash[23],
              hash[24], hash[25], hash[26], hash[27], hash[28], hash[29], hash[30], hash[31]);
    // sprintf(output,"%s",hash);
}

int set_http_header(esp_http_client_handle_t client){
    time_t localTime = time(NULL);    // 获取当前本地时间（秒级时间戳）
    uint64_t safety_code = iot_factory.safe_code;
    char localTime_str[32]={0};
    char Safe_Code_str[32]={0};
    char Sn_Str[32]={0};
    sprintf(localTime_str,"%llu", (uint64_t)localTime);
    sprintf(Safe_Code_str,"%llu", safety_code);
    snprintf(Sn_Str, sizeof(Sn_Str), "%.*s%013llu", (int)strnlen(iot_factory.iot_type, TYPE_SIZE), iot_factory.iot_type, iot_factory.iot_sn);
    
    //ETag是32位随机字符串，每次由客户端自动生成
    char ETag[33]={0};  
    generate_random_string(ETag,33);

    //Authorization，需要由请求头使用sha256加密生成
    char *Encry_Str = NULL;
    char *Safe_Code_Sin_str = NULL;

    Encry_Str = (char *)iot_calloc(256);
	if(Encry_Str == NULL){
        ESP_LOGE(TAG,"Encry_Str calloc faild");
        return -1;
    }
    Safe_Code_Sin_str = (char *)iot_calloc(256);
	if(Safe_Code_Sin_str == NULL){
        ESP_LOGE(TAG,"Safe_Code_Sin_str calloc faild");
        if(Encry_Str != NULL){
            free(Encry_Str);
        }
        return -1;
    }

    sprintf(Encry_Str,"Date=%s&ETag=%s&X-Att-DeviceId=%s&safetyCode=%s&x-app-key=%s&x-os=%s",
            localTime_str,ETag,Sn_Str,Safe_Code_str,"18C2BC21BE8F79115700FFBF91B","hdevice");

    sha256_Encry(Encry_Str,Safe_Code_Sin_str);
    // ESP_LOGI(TAG,"SHA256('%s') = %s\n", Encry_Str, Safe_Code_Sin_str);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Date", localTime_str);//Add wx
    esp_http_client_set_header(client, "ETag", ETag);//Add wx
    esp_http_client_set_header(client, "x-os", "hdevice");
    esp_http_client_set_header(client, "x-app-key", "18C2BC21BE8F79115700FFBF91B");
    esp_http_client_set_header(client, "X-Att-DeviceId", Sn_Str);//Add wx
    esp_http_client_set_header(client, "Authorization", Safe_Code_Sin_str);//Add wx
    
    if(Encry_Str != NULL){
        free(Encry_Str);
    }
    if(Safe_Code_Sin_str != NULL){
        free(Safe_Code_Sin_str);
    }
    return 0;
}

/*
send to http
*/
void https_ota_state_to_host(char recordId[32], char *state, char *reason) {

    char *post_data = json_ota_response((const char *)recordId, state, reason);
    if (post_data && strlen(recordId))
    {
        bool url_default = true;
        esp_http_client_config_t config = {0};
        
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
        if ( ota_object.method == OTA_METHOD_DEVASSIST )
        {
            if (0 != reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value
                    && 0 != reals.supper_control.cmd_list[SUPPER_CMD_SERVER_PORT].value)
            {
                char server_url[320] = {0};
                snprintf(server_url, sizeof(server_url), "http://%lu.%lu.%lu.%lu:%lu%s",
                    (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 24) & 0xFF,
                    (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 16) & 0xFF,
                    (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 8) & 0xFF,
                    reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value & 0xFF,
                    reals.supper_control.cmd_list[SUPPER_CMD_SERVER_PORT].value,
                    HTTPS_OTA_STATE_INTERFACE); // TODO: URL字段待定义
                config.url = server_url;
                url_default = false;
            }
        }
#endif        

        if ( url_default ) {
            if(!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)){
                config.url = DEV_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
            } else if(!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)){
                config.url = TEST_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
            } else if(!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)){
                config.url = PRO_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
            } else{
                ESP_LOGE(TAG, "UNKNOWN HOST: %s, use PRO_ENV_URL",  IotSetData.dev_info_t.could_dns);
                config.url = PRO_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
            }
        }
        
        config.method = HTTP_METHOD_POST;
        config.disable_auto_redirect = true;
        config.timeout_ms = 5000; // 设置超时时间
        config.if_name = &g_netif_req;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        // POST
        if(set_http_header(client) < 0){
            ESP_LOGE(TAG, "set_http_header failed");
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
        ESP_LOGW(TAG, "sys_is_updating, busy!!!");
        return;
    }
    
    if (ota_cmd_list == NULL) // 解析后台发送过来的json升级信息
    { 
        ESP_LOGI(TAG, "https_ota_cmd_parse"); 
        ota_cmd_list = json_ota_cmd_parse(pdata);
    } 
    else 
    {
        ESP_LOGW(TAG, "have image updating process, new json miss!!!");
    }
}

/**************************************************************************************************************************************************************************************/

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
    ver_list[*ver_num].type = type;
    ver_list[*ver_num].version = version;
    (*ver_num)++;
    return;
}

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
        ESP_LOGE(TAG, "Failed to fetch headers: 0x%llx", -content_length);
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
 * @brief 通过HTTPS GET请求获取新的图像信息。
 *
 * 此函数向指定的客户端发送HTTPS GET请求，以获取新的图像信息。
 * 函数接受客户端句柄、类型、序列号、版本列表和版本号作为参数。
 * 它返回一个指向ota_cmd_list_t结构的指针，其中包含HTTP OTA命令列表。
 *
 * @param client ESP HTTP客户端的句柄。
 * @param type 请求命令的类型。
 * @param sn 序列号。
 * @param ver_list 版本列表。
 * @param ver_num 版本数量。
 * @return 指向ota_cmd_list_t结构的指针，其中包含HTTP OTA命令列表。
 */
ota_cmd_list_t *https_get_new_image_info(esp_http_client_handle_t client, char *type, uint64_t sn, ver_data_t ver_list[], uint8_t ver_num) {
    ota_cmd_list_t *http_ota_cmd_list = NULL;
    char *request_cmd = NULL;
    char *rsp_data = NULL;

    if (client == NULL) {
        ESP_LOGE(TAG, "client is NULL");
        return NULL;
    }
    request_cmd = json_ota_http_request(type, sn, ver_list, ver_num);
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
    rsp_data = (char *)iot_calloc(4096);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }

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
 * @param ota_cmd_list OTA命令列表的头指针（不会被修改或释放）。
 * @param ver_list 提供的版本列表。
 * @param ver_num 版本列表中的版本数量。
 * @param version_data 输出的新版本信息结构体指针。
 * @return 如果找到新版本，返回0；否则返回-1。
 */
static int check_ver_list(const ota_cmd_list_t *ota_cmd_list, const ver_data_t ver_list[], uint8_t ver_num, new_version_update_struct *version_data) {
    int ret = -1;
    uint8_t j = 0;
    memset(version_data, 0, sizeof(new_version_update_struct));
    
    const ota_cmd_list_t *cur = ota_cmd_list;
    while (cur) {
        for (uint8_t i = 0; i < ver_num; i++) {
            if (cur->object.type == ver_list[i].type && 
                cur->object.version/100 == ver_list[i].version/100)
            {
                if (cur->object.version%100 > ver_list[i].version%100)
                {
                    ret = 0;
                    version_data->have_new_version_flag = true;
                }

                version_data->ver_list[j].type = cur->object.type;
                version_data->ver_list[j].version = cur->object.version;
                ESP_LOGI(TAG, "check_ver_list : have_new_version(type : %d)( %lu -> %lu )", cur->object.type, ver_list[i].version, cur->object.version);
                j++;
            }
        }
        cur = cur->next;
    }
    version_data->softver_num = j;
    return ret;
}

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
    static uint8_t error_cnt = 0;
    
    int ret = 0;
    ota_cmd_list_t *http_ota_cmd_list = NULL;
    esp_http_client_config_t config = {0};
    bool url_default = true;
    
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
    if ( target == HTTPS_CHECK_DEVASSIST )
    {
        if (0 != reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value
                && 0 != reals.supper_control.cmd_list[SUPPER_CMD_SERVER_PORT].value)
        {
            char server_url[320] = {0};
            snprintf(server_url, sizeof(server_url), "http://%lu.%lu.%lu.%lu:%lu%s",
                (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 24) & 0xFF,
                (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 16) & 0xFF,
                (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 8) & 0xFF,
                reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value & 0xFF,
                reals.supper_control.cmd_list[SUPPER_CMD_SERVER_PORT].value,
                HTTP_REQUEST_IMAGE_IF_TEST); // TODO: URL字段待定义
            config.url = server_url;
            url_default = false;
        }
    }
#endif  

    if ( url_default ) {
        if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)) {
            config.url = DEV_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
        } else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)) {
            config.url = TEST_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
        } else if (!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
            config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
        } else {
            ESP_LOGE(TAG, "UNKNOWN HOST, use PRO_ENV_URL");
            config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
        }
    }

    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 2048;
    config.timeout_ms = 5000; // 设置超时时间
    config.if_name = &g_netif_req; // 设置网口
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
            add_ver_list(ver_list, &ver_num, DEVICE_IOT, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver, VER_LIST_MAX_NUM);
            if(ver_num == 0) break;
            http_ota_cmd_list = https_get_new_image_info(client, dev_factory.dev_type, dev_factory.dev_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &iot_version_update) == 0){
                need_update_flag = true;

                // 释放旧的记录
                if (new_firmware_iot_list) {
                    ota_cmd_list_t  *current = new_firmware_iot_list;
                    while (current) {
                        ota_cmd_list_t *next = current->next;
                        free(current);
                        current = next;
                    }
                }

                // 新记录更新至Web，可升级IOT
                new_firmware_iot_list = http_ota_cmd_list;
                http_ota_cmd_list = NULL;
            }
            break;
        
        /*INV*/
        case HTTPS_CHECK_INV_IMAGE:
            for (uint8_t i = 0 ; i < top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.software_total ; i++ )
            {
                modbus_soft_version *target = NULL;
                if(i < MD_SOFT_MAIN_CAP) {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft[i];
                } else {
                    target = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base.soft_back[i-MD_SOFT_MAIN_CAP];
                }
                if(target) add_ver_list(ver_list, &ver_num, target->type, target->version, VER_LIST_MAX_NUM);
            }
            if(ver_num == 0) break;
            
            http_ota_cmd_list = https_get_new_image_info(client, dev_factory.dev_type, dev_factory.dev_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &inv_version_update) == 0){
                need_update_flag = true;
                
                // 释放旧的记录
                if (new_firmware_inv_list) {
                    ota_cmd_list_t  *current = new_firmware_inv_list;
                    while (current) {
                        ota_cmd_list_t *next = current->next;
                        free(current);
                        current = next;
                    }
                }

                // 新记录更新至Web，可升级INV
                new_firmware_inv_list = http_ota_cmd_list;
                http_ota_cmd_list = NULL;
            }
            break;
            
        /*PACK*/
        case HTTPS_CHECK_PACK_IMAGE:
#ifdef BATTERY_TYPE_VER_STAT_ENABLE 
            for(uint8_t i = 0; i < get_pack_type_count(); i++)
            {
                ver_num = 0;
                memset(ver_list, 0, sizeof(ver_list));
                if ( memcmp(top_modbus_rd.Type_Pack[i].mod_reg06100_Pack_each.type_ascii, SN_TYPE_COMMON_BATTERY_CAN_ASCII, TYPE_SIZE) == 0 ) continue;

                for (uint8_t j = 0 ; j < top_modbus_rd.Type_Pack[i].mod_reg06100_Pack_each.software_total ; j++ )
                {
                    add_ver_list(ver_list, &ver_num, 
                        top_modbus_rd.Type_Pack[i].mod_reg06100_Pack_each.soft[j].type, 
                        top_modbus_rd.Type_Pack[i].mod_reg06100_Pack_each.soft[j].version, 
                        VER_LIST_MAX_NUM);
                }
                if(ver_num == 0) continue;
                http_ota_cmd_list = https_get_new_image_info(client, 
                                            top_modbus_rd.Type_Pack[i].mod_reg06100_Pack_each.type_ascii, 
                                            top_modbus_rd.Type_Pack[i].mod_reg06100_Pack_each.sn_code, 
                                            ver_list, ver_num);
                
                if(http_ota_cmd_list == NULL){
                    ret = -1;
                } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &pack_version_update[i]) == 0){
                    need_update_flag = true;

                    // 释放旧的记录
                    if (new_firmware_pack_list[i]) {
                        ota_cmd_list_t  *current = new_firmware_pack_list[i];
                        while (current) {
                            ota_cmd_list_t *next = current->next;
                            free(current);
                            current = next;
                        }
                    }
                    
                    // 新记录更新至Web，可升级Pack
                    new_firmware_pack_list[i] = http_ota_cmd_list;
                    http_ota_cmd_list = NULL;
                }
            }
#else
            for(uint8_t i = 0; i < PACK_MAX_NUM; i++)
            {
                ver_num = 0;
                memset(ver_list, 0, sizeof(ver_list));
                for (uint8_t j = 0 ; j < top_modbus_rd.Pack[i].mod_reg06100_Pack_each.software_total ; j++ )
                {
                    add_ver_list(ver_list, &ver_num, top_modbus_rd.Pack[i].mod_reg06100_Pack_each.soft[j].type, top_modbus_rd.Pack[i].mod_reg06100_Pack_each.soft[j].version, VER_LIST_MAX_NUM);
                }
                if(ver_num == 0) continue;
                http_ota_cmd_list = https_get_new_image_info(client, top_modbus_rd.Pack[i].mod_reg06100_Pack_each.type_ascii, top_modbus_rd.Pack[i].mod_reg06100_Pack_each.sn_code, ver_list, ver_num);
                if(http_ota_cmd_list == NULL){
                    ret = -1;
                } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &pack_version_update[i]) == 0){
                    need_update_flag = true;

                    // 直接清除，暂不保留每个电池包的升级版本
                    ota_cmd_list_t  *current = http_ota_cmd_list;
                    while (current) {
                        ota_cmd_list_t *next = current->next;
                        free(current);
                        current = next;
                    }
                    http_ota_cmd_list = NULL;
                }
            }
#endif                
            break;

        /*DCDC*/
        case HTTPS_CHECK_DCDC_IMAGE:
#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE            
            if ( top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15500_DCDC_info.dcdc_SoftwareVersion ) {
                add_ver_list(ver_list, &ver_num, top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15500_DCDC_info.dcdc_SoftwareType, 
                    top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15500_DCDC_info.dcdc_SoftwareVersion, VER_LIST_MAX_NUM);
            } else {
                add_ver_list(ver_list, &ver_num, top_modbus_rd.Dcdc[0].mod_reg15500_DCDC_info.dcdc_SoftwareType, 
                    top_modbus_rd.Dcdc[0].mod_reg15500_DCDC_info.dcdc_SoftwareVersion, VER_LIST_MAX_NUM);
            }
            if ( top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg11000_IOT_info.software_ver ) {
                add_ver_list(ver_list, &ver_num, DEVICE_IOT, top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg11000_IOT_info.software_ver, VER_LIST_MAX_NUM);
            } else {
                add_ver_list(ver_list, &ver_num, DEVICE_IOT, top_modbus_rd.Dcdc[0].mod_reg11000_IOT_info.software_ver, VER_LIST_MAX_NUM);
            }
            
            if(ver_num == 0) break;
            
            http_ota_cmd_list = https_get_new_image_info(client, top_modbus_rd.Dcdc[0].mod_reg15500_DCDC_info.dev_type, 
                                                    top_modbus_rd.Dcdc[0].mod_reg15500_DCDC_info.dev_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &dcdc_version_update) == 0){
                need_update_flag = true;
                
                // 释放旧的记录
                if (new_firmware_dcdc_list) {
                    ota_cmd_list_t  *current = new_firmware_dcdc_list;
                    while (current) {
                        ota_cmd_list_t *next = current->next;
                        free(current);
                        current = next;
                    }
                }

                // 新记录更新至Web，可升级
                new_firmware_dcdc_list = http_ota_cmd_list;
                http_ota_cmd_list = NULL;
            }
#endif            
            break;
            
        /*DCHUB*/
        case HTTPS_CHECK_DCHUB_IMAGE:
#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE            
            if ( top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg01100_Inv_base.soft[0].version ) {
                add_ver_list(ver_list, &ver_num, DEVICE_DC_HUB, top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg01100_Inv_base.soft[0].version, VER_LIST_MAX_NUM);
            } else {
                add_ver_list(ver_list, &ver_num, DEVICE_DC_HUB, top_modbus_rd.DcHub[0].mod_reg01100_Inv_base.soft[0].version, VER_LIST_MAX_NUM);
            }
            
            if(ver_num == 0) break;
            
            http_ota_cmd_list = https_get_new_image_info(client, SN_TYPE_DCHUB_ASCII, top_modbus_rd.DcHub[0].mod_reg15700_Dc_Hub_info.dc_hub_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &dchub_version_update) == 0){
                need_update_flag = true;
                
                // 释放旧的记录
                if (new_firmware_dchub_list) {
                    ota_cmd_list_t  *current = new_firmware_dchub_list;
                    while (current) {
                        ota_cmd_list_t *next = current->next;
                        free(current);
                        current = next;
                    }
                }

                // 新记录更新至Web，可升级
                new_firmware_dchub_list = http_ota_cmd_list;
                http_ota_cmd_list = NULL;
            }
#endif            
            break;

        /*Devassist*/
        case HTTPS_CHECK_DEVASSIST:
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE            
            add_ver_list(ver_list, &ver_num, DEVICE_IOT, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver, VER_LIST_MAX_NUM);
            if(ver_num == 0) break;
            http_ota_cmd_list = https_get_new_image_info(client, dev_factory.dev_type, dev_factory.dev_sn, ver_list, ver_num);
            if(http_ota_cmd_list == NULL){
                ret = -1;
            } else if(check_ver_list(http_ota_cmd_list, ver_list, ver_num, &devassist_version_update) == 0){
                need_update_flag = true;

                // 释放旧的记录
                if (new_firmware_devassist_list) {
                    ota_cmd_list_t  *current = new_firmware_devassist_list;
                    while (current) {
                        ota_cmd_list_t *next = current->next;
                        free(current);
                        current = next;
                    }
                }

                // 新记录更新至外部
                new_firmware_devassist_list = http_ota_cmd_list;
                http_ota_cmd_list = NULL;
            }
            break;    
#endif            
        default:
            break;
    }
    esp_http_client_cleanup(client);
    if ( ret == -1 ) {
        error_cnt++;
        if ( error_cnt > 1 ) {
            error_cnt = 0;
            reals.need_new_version_flag.Byte2 &= ~(1 << target); 
        }
    } else {
        error_cnt = 0;
    }
    return ret;
}

/**
 * @brief 检查是否有新的固件版本。
 *
 * 此函数用于检查是否有新的固件版本可用。只要有一个新版本可用，函数就返回1。
 * 否则，返回0。
 */
int check_ver_info_update(uint8_t dev_type, uint8_t index, uint16_t curr_type, uint32_t curr_version) 
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
            version_data = &pack_version_update[index];
            break;

        case HTTPS_CHECK_DCDC_IMAGE :
            version_data = &dcdc_version_update;
            break;
        
        case HTTPS_CHECK_DCHUB_IMAGE :
            version_data = &dchub_version_update;
            break;
        
        default:
            return ret;
    }
        
    if ( !version_data->softver_num )
    {
        return ret;
    }
    
    for (uint8_t i = 0; i < version_data->softver_num; i++) 
    {
        if (version_data->ver_list[i].type == curr_type && 
            version_data->ver_list[i].version / 100 == curr_version / 100 &&
            version_data->ver_list[i].version % 100 > curr_version % 100) 
        {
            ret++;
        }
    }

    return ret;
}

/**
 * @brief 检查新固件标志。
 *
 * 此函数用于检查是否有新的固件版本可用。
 */
bool Get_http_new_version_flag(uint8_t dev_type, uint8_t index) 
{
    // int ret = 0;
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
            if (index < PACK_MAX_NUM) {
                version_data = &pack_version_update[index];
            } else {
                return 0;
            }
            break;
            
        case HTTPS_CHECK_DCDC_IMAGE :
            version_data = &dcdc_version_update;
            break;
        
        case HTTPS_CHECK_DCHUB_IMAGE :
            version_data = &dchub_version_update;
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

    bool new_ota_status = sys_is_updating();
    if (( new_ota_status == false ) && ( last_ota_status == true ))
    {
        /*升级成功，根据对应类型清除新固件信息*/
        if ( reals.current_ota_info.isOta == 2 )  
        {
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            switch ( reals.current_ota_info.type )
            {
                case DEVICE_IOT :
                    if ( reals.current_ota_info.soft_version/100 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver/100 ) {
                        /*本机*/
                        memset(&iot_version_update, 0, sizeof(new_version_update_struct));
                        reals.need_new_version_flag.sBit.iot = 1;
                    } else {
                        /*DCDC*/
                        memset(&dcdc_version_update, 0, sizeof(new_version_update_struct));
                        reals.need_new_version_flag.sBit.dcdc = 1;
                    } 
                    break;
                    
                case DEVICE_ARM :
                case DEVICE_PACK_M1 :
                    memset(&inv_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.inv = 1;
                    break;
                    
                case DEVICE_DSP :
                    memset(&inv_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.inv = 1;
                    memset(&dcdc_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.dcdc = 1;
                    break;
                    
                case DEVICE_DC_HUB :
                    memset(&dchub_version_update, 0, sizeof(new_version_update_struct));
                    reals.need_new_version_flag.sBit.dchub = 1;
                    break;
                
                case DEVICE_BMS :
                case DEVICE_PACK_BCU :
                case DEVICE_PACK_BMU :
                case DEVICE_PACK_BMS :
                case DEVICE_PACK_SAFTY :
                case DEVICE_SYNLINK :
                    memset(pack_version_update, 0, sizeof(pack_version_update));
                    reals.need_new_version_flag.sBit.pack = 1;
                    break;
                
                default:
                    break;
            }
            
            ESP_LOGW(TAG, "https_new_image_timeout_clean (type : %d)", reals.current_ota_info.type);
        }
    }

    last_ota_status = new_ota_status;
    
    if ( new_ota_status == true ) {
        /*升级时暂停查询*/
        https_new_image_timeout_clean_flag = true;
    } else {
        /*升级成功后延时30s再次查询新固件版本*/
        if ( pre_time != 0 ) {
            uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ( (now_time - pre_time) <= 30000 ) {
                https_new_image_timeout_clean_flag = true;
            } else {
                pre_time = 0;
            }
        } else {
            /*正常查询*/
            https_new_image_timeout_clean_flag = false;
        }
    }
}

#if 0
/** Webserver触发HTTP升级 */

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
    if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST))
    {
        config.url = DEV_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
    }
    else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST))
    {
        config.url = TEST_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
    }
    else if (!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL))
    {
        config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
    }
    else
    {
        ESP_LOGE(TAG, "UNKNOWN HOST, use PRO_ENV_URL");
        config.url = PRO_ENV_URL""HTTPS_REQUEST_IMAGE_INTERFACE;
    }

    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 2048;
    config.timeout_ms = 5000; // 设置超时时间
    config.if_name = &g_netif_req; // 设置网口

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "client is NULL");
        return NULL;
    }

    http_ota_cmd_list = https_get_new_image_info(client, req_info.dev_type, req_info.dev_sn, req_info.ver_list, req_info.ver_num);

    esp_http_client_cleanup(client);

    return http_ota_cmd_list;
}

#endif

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

static void https_get_record_id(esp_http_client_handle_t client, char recordId[32], char *state,
                                    char *reason, uint8_t image_type, uint32_t new_ver, uint32_t old_ver)
{
    if (client == NULL) {
        ESP_LOGE(TAG, "client is NULL");
        return;
    }

    char *request_cmd = NULL;
    char *rsp_data = NULL;

    request_cmd = json_ota_response_first((const char *)recordId, state, reason, dev_factory.dev_sn, 
                                        dev_factory.dev_type, image_type, new_ver, old_ver);
    if (request_cmd == NULL) {
        ESP_LOGE(TAG, "Failed to create request command");
        goto cleanup;
    }

    ESP_LOGD(TAG,"request_cmd : %s", request_cmd);

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

    rsp_data = (char *)iot_calloc(2048);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }

    if (read_http_response(client, rsp_data, 2048) > 0) {
        ESP_LOGD(TAG,"rsp_data : %s", rsp_data);
        char record_id[32];
        if(json_parse_record_id(rsp_data, record_id) == 0) {
            memset(image_record_id, 0x00, sizeof(image_record_id));
            strncpy(image_record_id, record_id, sizeof(image_record_id));
        }
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
}

static int https_ota_get_record_id(char recordId[32], char *state, char *reason, uint8_t image_type, uint32_t new_ver, uint32_t old_ver)
{
    bool url_default = true;
    esp_http_client_config_t config = {0};
    
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
    if ( ota_object.method == OTA_METHOD_DEVASSIST )
    {
        if (0 != reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value
                && 0 != reals.supper_control.cmd_list[SUPPER_CMD_SERVER_PORT].value)
        {
            char server_url[320] = {0};
            snprintf(server_url, sizeof(server_url), "http://%lu.%lu.%lu.%lu:%lu%s",
                (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 24) & 0xFF,
                (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 16) & 0xFF,
                (reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value >> 8) & 0xFF,
                reals.supper_control.cmd_list[SUPPER_CMD_SERVER_IP].value & 0xFF,
                reals.supper_control.cmd_list[SUPPER_CMD_SERVER_PORT].value,
                HTTPS_OTA_STATE_INTERFACE); // TODO: URL字段待定义
            config.url = server_url;
            url_default = false;
        }
    }
#endif        

    if ( url_default ) {
        if(!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)){
            config.url = DEV_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
        } else if(!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)){
            config.url = TEST_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
        } else if(!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)){
            config.url = PRO_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
        } else{
            ESP_LOGE(TAG, "UNKNOWN HOST: %s, use PRO_ENV_URL",  IotSetData.dev_info_t.could_dns);
            config.url = PRO_ENV_URL""HTTPS_OTA_STATE_INTERFACE;
        }
    }

    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 2048;
    config.timeout_ms = 5000; // 设置超时时间
    config.if_name = &g_netif_req; // 设置网口

    // 至多重试两次
    uint8_t retry = 2;

    // 预留重试机制，降低丢失记录的概率
    while ( retry-- ) {
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "client is NULL, retry...");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        
        https_get_record_id(client, recordId, state, reason, image_type, new_ver, old_ver);
        
        esp_http_client_cleanup(client);
        if (strlen(image_record_id) > 0) {
            break;  // 已拿到 recordId
        } else {
            ESP_LOGW(TAG, "Get record id failed, retry...");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
    }

    return 0;
}

void update_firmware_check(void)
{
    is_update_check_triggered = true;
}

int firmware_upgrade_start(char *model, char *version, uint8_t chl)
{
#if 0   // 不做强制要求，提供则比对，不提供则忽略，后续升级过程中会再次校验
    if (!model || !version)
    {
        ESP_LOGE(TAG, "firmware upgrade start model or version error");
        return -1;
    }
#endif

    ota_cmd_list_t  *current = NULL;

    switch ( chl )
    {
        case OTA_METHOD_WEBSERVER :
            if (!new_firmware_iot_list) {
                ESP_LOGE(TAG, "new_firmware_iot_list : no firmware to upgrade");
                return -2;
            } else {
                current = new_firmware_iot_list;
            }
            break;
        case OTA_METHOD_DEVASSIST :
            if (!new_firmware_devassist_list) {
                ESP_LOGE(TAG, "new_firmware_devassist_list : no firmware to upgrade");
                return -2;
            } else {
                current = new_firmware_devassist_list;
            }
            break;
        default:
            ESP_LOGE(TAG, "Unknown Chl(%d)", chl);
            return -2;
    }

    ota_cmd_list_t  *target_head = NULL;
    ota_cmd_list_t  *target = NULL;

    while (current)
    {
        ESP_LOGW(TAG, "Find upgrade firmware, model:%s, version:%lu", current->object.model, current->object.version);

        // 型号检查（可选）
        if ( model ) {
            if (0 != strcmp(model, current->object.model)) {
                current = current->next;
                continue;
            }
        }

        // 版本检查（可选）
        if ( version ) {
            char firmware_ver[16] = {0};
            snprintf(firmware_ver, sizeof(firmware_ver), "%lu", current->object.version);
            if (0 != strcmp(version, firmware_ver)) {
                current = current->next;
                continue;
            }
        }        
        
        ota_cmd_list_t  *copy_cmd = (ota_cmd_list_t *)iot_calloc(sizeof(ota_cmd_list_t));                                     
        if (!copy_cmd) {
            ESP_LOGE(TAG, "firmware_upgrade_start : memory failed");
            current = current->next;
            continue;
        }  else {
            memcpy(&copy_cmd->object, &current->object, sizeof(ota_object_t));
        }

        // 拷贝升级版本链表
        if (target_head == NULL) {
            target_head = copy_cmd;
            target = copy_cmd;
        } else {
            target->next = copy_cmd;
            target = copy_cmd;
        }

        current = current->next;
    }

    if ( target_head ) {
        ota_cmd_list = target_head;
        need_query_recordId = true;
        return 1;
    }

    return 0;
}

/** Webserver触发HTTP升级 End */

/*******************************************************************************************************************************************************************************************/
// 将此文件代码添加到HTTP客户端的主文件中。
// 先通过HTTP GET获取服务器当前UTC时间和IP地址。IP地址在HTTP响应头字段，通过HTTP事件获取，UTC时间在HTTP响应Json报文获取。

#define HTTPS_GET_UTC_IP_TIMEOUT    5000 // 5s超时

#define HTTPS_GET_TIMEZONE_BIT          BIT0
#define HTTPS_GET_IP_BIT                BIT1
#define HTTPS_GET_TIME_DST_BIT          BIT2
#define HTTPS_GET_TIME_DST_STAMP_BIT    BIT3
#define HTTPS_GET_TIME_DST_ISO8601_BIT  BIT4
#define HTTPS_GET_TIME_CONN_TOKEN_BIT   BIT5

#if HTTP_UTC_TIME_VERSION == 2
#define HTTP_UTC_TIME               "/api/midppkic/cert/app/v2/now/utc-time"
#else
#define HTTP_UTC_TIME               "/api/midppkic/cert/app/v1/now/utc-time"
#endif

/***********************************************/

// 时区偏移量使用
//#define HTTPS_GET_TIMEZONE_OFFSET_ENABLE

// DST时区使用
#define HTTPS_GET_TIMEZONE_DST_ENABLE

/***********************************************/

USE_EXT_RAM_BSS char gw_server_ip[128] = {0}; 
int32_t timezone_offset = 0; 
time_t server_time_now = 0;
struct tm server_tm_time = {0};
time_res_struct CurrTimeZone = {0};
char *conn_token_json_string = NULL;
uint16_t conn_token_buf_len = 0;
USE_EXT_RAM_BSS connection_token_t conn_token_info = {0};

static EventGroupHandle_t  http_event_handle = NULL;

static void check_dst(time_t timestamp) {
    struct tm *tm_info = localtime(&timestamp);
    if (tm_info->tm_isdst > 0) {
        ESP_LOGW(TAG, "The specified time is in Daylight Saving Time (DST).");
    } else if (tm_info->tm_isdst == 0) {
        ESP_LOGI(TAG, "The specified time is not in Daylight Saving Time (DST).");
    } else {
        ESP_LOGI(TAG, "Unable to determine if the specified time is in Daylight Saving Time (tm_isdst = -1).");
    }
}

/**
 * @brief 将十六进制字符转换为其整数值。
 * @param c 十六进制字符 ('0'-'9', 'a'-'f', 'A'-'F').
 * @return 对应的整数值 (0-15)，如果字符无效则返回-1。
 */
static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief 将十六进制字符串解码为ASCII字符串。
 * @param hex_str 输入的十六进制字符串。
 * @param ascii_buf 用于存储解码后ASCII字符串的缓冲区。
 * @param buf_size 缓冲区的最大大小。
 * @return 成功时返回解码后的字符串长度，失败时返回-1。
 */
static int hex_str_to_ascii(const char *hex_str, char *ascii_buf, size_t buf_size) {
    if (hex_str == NULL || ascii_buf == NULL) return -1;

    size_t hex_len = strlen(hex_str);
    if (hex_len % 2 != 0) {
        ESP_LOGE(TAG, "Hex string has an odd length.");
        return -1; // 十六进制字符串长度必须是偶数
    }

    size_t ascii_len = hex_len / 2;
    if (ascii_len >= buf_size) {
        ESP_LOGE(TAG, "Output buffer is too small for decoded string.");
        return -1; // 输出缓冲区不足
    }

    for (size_t i = 0; i < ascii_len; i++) {
        int high = hex_char_to_int(hex_str[i * 2]);
        int low = hex_char_to_int(hex_str[i * 2 + 1]);

        if (high == -1 || low == -1) {
            ESP_LOGE(TAG, "Invalid character in hex string.");
            return -1; // 包含无效的十六进制字符
        }
        ascii_buf[i] = (char)((high << 4) | low);
    }
    ascii_buf[ascii_len] = '\0'; // 添加字符串结束符
    return ascii_len;
}

/**
 * @brief 管理 conn_token_json_string 的缓冲区，实现按需分配和增长。
 * @param required_size 新的 token 解码后所需的缓冲区大小。
 * @return 成功时返回 ESP_OK，内存分配失败时返回 ESP_ERR_NO_MEM。
 */
static esp_err_t manage_conn_token_buffer(size_t required_size)
{
    // 如果缓冲区已存在
    if (conn_token_json_string != NULL) {
        // 如果现有缓冲区大小不足
        if (conn_token_buf_len < required_size) {
            free(conn_token_json_string);
            conn_token_json_string = NULL;
            conn_token_buf_len = 0;
        } else {
            // 缓冲区大小足够，直接清零复用
            memset(conn_token_json_string, 0, conn_token_buf_len);
        }
    }
    
    // 如果缓冲区指针为 NULL (无论是首次分配还是因大小不足被释放)
    if (conn_token_json_string == NULL) {
        conn_token_json_string = iot_calloc(required_size);
        if (conn_token_json_string == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for conn_token_json_string");
            return ESP_ERR_NO_MEM;
        } else {
            conn_token_buf_len = required_size;
        }
    }
    return ESP_OK;
}

/*------------------------------------------------------------------------------
 Function: http_conn_token_info_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取conn_token信息
  * @param[in]  void  
  * @param[out] None
  * @return     connection_token_t *
  */
connection_token_t *http_conn_token_info_get(void)
{
    return &conn_token_info;
}

static esp_err_t https_event_handle(esp_http_client_event_t *evt){
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            char *endptr;
            if(evt->header_key && strcmp(evt->header_key, "x-dst") == 0){
                time_t dst = strtol(evt->header_value, &endptr, 10);
                if (*endptr == '\0') {
                    server_time_now = dst;
                    ESP_LOGI(TAG, "x-dst : stamp(%s)", evt->header_value);
                    xEventGroupSetBits(http_event_handle, HTTPS_GET_TIME_DST_STAMP_BIT | HTTPS_GET_TIME_DST_BIT);
                } else if(parse_iso8601_to_tm_timestamp(evt->header_value, &server_tm_time, &CurrTimeZone)) {
                    ESP_LOGI(TAG, "x-dst : ISO8601(%s)", evt->header_value);
                    xEventGroupSetBits(http_event_handle, HTTPS_GET_TIME_DST_ISO8601_BIT | HTTPS_GET_TIME_DST_BIT);
                } else {
                    ESP_LOGE(TAG, "x-dst : Invalid(%s)", evt->header_value);
                }
            } else if(evt->header_key && strcmp(evt->header_key, "x-timezone-offset") == 0){
                long offset = strtol(evt->header_value, &endptr, 10);
                if (*endptr == '\0') {
                    timezone_offset = offset;
                    ESP_LOGI(TAG, "x-timezone-offset : %s ms", evt->header_value);
                    xEventGroupSetBits(http_event_handle, HTTPS_GET_TIMEZONE_BIT);
                } else {
                    ESP_LOGE(TAG, "x-timezone-offset : Invalid(%s)", evt->header_value);
                }
            } else if(evt->header_key && strcmp(evt->header_key, "x-iot-server") == 0){
                ESP_LOGI(TAG, "x-iot-server : %s", evt->header_value);
                memset(gw_server_ip, 0, sizeof(gw_server_ip));
                strncpy(gw_server_ip, evt->header_value, sizeof(gw_server_ip)-1);
                xEventGroupSetBits(http_event_handle, HTTPS_GET_IP_BIT);
            } else if(evt->header_key && strcmp(evt->header_key, "x-timezone") == 0){
                ESP_LOGI(TAG, "x-timezone : %s", evt->header_value);
            } else if(evt->header_key && strcmp(evt->header_key, "x-conn-token") == 0){
                ESP_LOGI(TAG, "x-conn-token : %s", evt->header_value);
#ifdef CONFIG_MQTT_CONNECT_HTTP_TOKEN_ENABLE
                // 1. 管理缓冲区：按需分配或调整大小
                size_t required_size = strlen(evt->header_value) / 2 + 1;
                if (manage_conn_token_buffer(required_size) != ESP_OK) {
                    break; // 内存分配失败，直接退出
                }

                // 2.将HEX字符解码成ASCII字符
                if (hex_str_to_ascii(evt->header_value, conn_token_json_string, strlen(evt->header_value) / 2 + 1) > 0) {
                    ESP_LOGI(TAG, "Decoded conn-token: %s", conn_token_json_string);
                    xEventGroupSetBits(http_event_handle, HTTPS_GET_TIME_CONN_TOKEN_BIT);
                } else {
                    ESP_LOGE(TAG, "Failed to decode conn-token string.");
                }
#endif
            }
            break;
        case HTTP_EVENT_ON_DATA:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            // ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGW(TAG, "HTTP_EVENT_ON_REDIRECT");
            break;
    }
    return ESP_OK;
}

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
    if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)) {  // 开发环境
        config.url = DEV_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)) { // 测试环境
        config.url = TEST_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, PRO_ENV_HOST)) { // 生产环境
        config.url = PRO_ENV_URL""HTTP_UTC_TIME;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST, use default host");
        config.url = PRO_ENV_URL""HTTP_UTC_TIME; // 默认生产
    }
    ESP_LOGI(TAG,"config.url: %s", config.url);
    
    config.method = HTTP_METHOD_GET;
    config.disable_auto_redirect = false;
    config.buffer_size = 1024;
    config.timeout_ms = HTTPS_GET_UTC_IP_TIMEOUT; // 设置超时时间
    config.event_handler = https_event_handle;
    config.if_name = &g_netif_req; // 设置网口
    
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
    ● 澳大利亚：1.41.111.112
    ● 日 本：1.66.105.204
    ● 美 国：164.153.51.98（纽约IP，实行夏令时）
    ● 美 国：64.38.192.100（亚利桑那IP，不实行夏令时）
    ● 德 国：2.203.176.110
    */
    const char *vitual_ip = "2.203.176.110";
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

    // 解析应答获取时间
    if(ESP_OK != json_parse_utc_ip(rsp, time)) {
        ESP_LOGE(TAG, "Failed to json_parse_utc_ip: %s", rsp);
        goto cleanup;
    }

    EventBits_t bit_mask = HTTPS_GET_TIMEZONE_BIT 
                            | HTTPS_GET_IP_BIT 
#ifdef CONFIG_MQTT_CONNECT_HTTP_TOKEN_ENABLE
                            | HTTPS_GET_TIME_CONN_TOKEN_BIT 
#endif  
                            | HTTPS_GET_TIME_DST_BIT;

    // 等待所有事件标志位
    EventBits_t event = xEventGroupWaitBits(
        http_event_handle,
        bit_mask,
        pdTRUE,  // 清除标志位
        pdTRUE,  // 等待所有标志位
        pdMS_TO_TICKS(HTTPS_GET_UTC_IP_TIMEOUT) // 超时时间
    );

#ifdef TIME_GET_FROM_SERVER_BY_HTTP_ENABLE
    if ( 2 != IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update ) {
        // 获取时区
        if(event & HTTPS_GET_TIMEZONE_BIT) {
#ifdef HTTPS_GET_TIMEZONE_OFFSET_ENABLE        
            int hour = timezone_offset/(int32_t)3600000;
            int min = (timezone_offset/60000)%60;
            if(!((hour > 14) || (hour < -12) || (min > 59) || (min < 0))){
    
                time_res_struct SetTimeZone = {
                    .hour = (hour & 0xFF),
                    .min = (min & 0xFF),
                };
                    
                ESP_LOGI(TAG, "get timezone success UTC: [%02d:%02d]", SetTimeZone.hour, SetTimeZone.min);
                
                //判断是否为无效写入
                if(IotSetData.dev_info_t.SetTimeZone.all != SetTimeZone.all)
                {
                    app_sntp_set_timezone(SetTimeZone.hour, SetTimeZone.min);
                    IotSetData.dev_info_t.SetTimeZone.all = SetTimeZone.all;
                    reals.SetDataWrFlag.sBit.SetTimeZone = 1;
                }
            }
#endif        
        }
    
        // 获取本地时间
        if(event & HTTPS_GET_TIME_DST_BIT) {
            // 获取本地时间(timestamp)
            if(event & HTTPS_GET_TIME_DST_STAMP_BIT) {
                ESP_LOGI(TAG, "get dst time success(timestamp) : %llu", server_time_now);
                struct timeval tv_time = {
                    .tv_sec = server_time_now,
                    .tv_usec = 0
                };
                settimeofday(&tv_time, NULL);//设置时间到系统时间
                reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;
            }
            // 获取本地时间(ISO8601)
            else if(event & HTTPS_GET_TIME_DST_ISO8601_BIT) {
#ifdef HTTPS_GET_TIMEZONE_DST_ENABLE          
                // 获取当前时区
                ESP_LOGI(TAG, "get DST timezone success UTC: [%02d:%02d]", CurrTimeZone.hour, CurrTimeZone.min);
    
                //判断是否为无效写入
                if(IotSetData.dev_info_t.SetTimeZone.all != CurrTimeZone.all)
                {
                    app_sntp_set_timezone(CurrTimeZone.hour, CurrTimeZone.min);
                    IotSetData.dev_info_t.SetTimeZone.all = CurrTimeZone.all;
                    reals.SetDataWrFlag.sBit.SetTimeZone = 1;
                }
#endif
                if ( tm_time_to_timestamp(&server_time_now, &server_tm_time) )
                {
                    ESP_LOGI(TAG, "get dst time success(ISO8601) : %llu", server_time_now);
                    struct timeval tv_time = {
                        .tv_sec = server_time_now,
                        .tv_usec = 0
                    };
                    settimeofday(&tv_time, NULL);//设置时间到系统时间
                    reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;
                }
            }
        }    
    }
#endif

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
    
#ifdef CONFIG_MQTT_CONNECT_HTTP_TOKEN_ENABLE
    // 获取conn-token信息
    if(event & HTTPS_GET_TIME_CONN_TOKEN_BIT) {
        if (json_get_conn_token_info(conn_token_json_string, &conn_token_info) != 0) {
            ESP_LOGE(TAG, "Failed to get connection token info from JSON.");
        } 

    #if 0
        //仅供调试使用，出厂代码禁止使用
        conn_token_info.state = 1;
        conn_token_info.waitSec = 0;
    #endif
    }
#endif

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
    if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)) {  // 开发环境
        config.url = DEV_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)) { // 测试环境
        config.url = TEST_ENV_URL""HTTP_UTC_TIME;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, PRO_ENV_HOST)) { // 生产环境
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
    config.if_name = &g_netif_req; // 设置网口
    
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
    ● 澳大利亚：1.41.111.112
    ● 日 本：1.66.105.204
    ● 美 国：164.153.51.98（纽约IP，实行夏令时）
    ● 美 国：64.38.192.100（亚利桑那IP，不实行夏令时）
    ● 德 国：2.203.176.110
    */
    const char *vitual_ip = "2.203.176.110";
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
                            | HTTPS_GET_TIME_DST_BIT;

    // 等待所有事件标志位
    EventBits_t event = xEventGroupWaitBits(
        http_event_handle,
        bit_mask,
        pdTRUE,  // 清除标志位
        pdTRUE,  // 等待所有标志位
        pdMS_TO_TICKS(HTTPS_GET_UTC_IP_TIMEOUT) // 超时时间
    );

    // 获取时区
    if(event & HTTPS_GET_TIMEZONE_BIT) {
#ifdef HTTPS_GET_TIMEZONE_OFFSET_ENABLE           
        int hour = timezone_offset/(int32_t)3600000;
        int min = (timezone_offset/60000)%60;
        if(!((hour > 14) || (hour < -12) || (min > 59) || (min < 0))){

            time_res_struct SetTimeZone = {
                .hour = (hour & 0xFF),
                .min = (min & 0xFF),
            };
                
            ESP_LOGI(TAG, "get timezone success UTC: [%02d:%02d]", SetTimeZone.hour, SetTimeZone.min);
            
            //判断是否为无效写入
            if(IotSetData.dev_info_t.SetTimeZone.all != SetTimeZone.all)
            {
                app_sntp_set_timezone(SetTimeZone.hour, SetTimeZone.min);
                IotSetData.dev_info_t.SetTimeZone.all = SetTimeZone.all;
                reals.SetDataWrFlag.sBit.SetTimeZone = 1;
            }
        }
#endif        
    }

    // 获取本地时间
    if(event & HTTPS_GET_TIME_DST_BIT) {
        // 获取本地时间(timestamp)
        if(event & HTTPS_GET_TIME_DST_STAMP_BIT) {
            ESP_LOGI(TAG, "get dst time success(timestamp) : %llu", server_time_now);
            struct timeval tv_time = {
                .tv_sec = server_time_now,
                .tv_usec = 0
            };
            settimeofday(&tv_time, NULL);//设置时间到系统时间
            reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;
        }
        // 获取本地时间(ISO8601)
        else if(event & HTTPS_GET_TIME_DST_ISO8601_BIT) {
#ifdef HTTPS_GET_TIMEZONE_DST_ENABLE          
            // 获取当前时区
            ESP_LOGI(TAG, "get DST timezone success UTC: [%02d:%02d]", CurrTimeZone.hour, CurrTimeZone.min);

            //判断是否为无效写入
            if(IotSetData.dev_info_t.SetTimeZone.all != CurrTimeZone.all)
            {
                app_sntp_set_timezone(CurrTimeZone.hour, CurrTimeZone.min);
                IotSetData.dev_info_t.SetTimeZone.all = CurrTimeZone.all;
                reals.SetDataWrFlag.sBit.SetTimeZone = 1;
            }
#endif
            if ( tm_time_to_timestamp(&server_time_now, &server_tm_time) )
            {
                ESP_LOGI(TAG, "get dst time success(ISO8601) : %llu", server_time_now);
                struct timeval tv_time = {
                    .tv_sec = server_time_now,
                    .tv_usec = 0
                };
                settimeofday(&tv_time, NULL);//设置时间到系统时间
                reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;
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
 * @brief 请求并获取节省金钱的数据
 * 
 * 输入参数：flag:0:当日, 1:当月, 2:当年, 5:近6年; 
 * @return uint64_t 节省的金额,失败返回0
 */
int http_request_money_save(const uint64_t device_sn, const char *device_type, uint16_t flag, uint8_t *temperature_unit_out, uint32_t *saved_money_out) {
    #define RX_BUFFER_SIZE 3072 // 3k

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
    if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)) {
        config.url = DEV_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)) {
        config.url = TEST_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
        config.url = PRO_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST, use PRO_ENV_URL");
        config.url = PRO_ENV_URL""HTTPS_SAVE_MONEY_INTERFACE;
    }

    // 3. 设置HTTP客户端配置
    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 1024;
    config.buffer_size = 1024;
    config.timeout_ms = 5000; // 增加超时时间到5秒
    // config.skip_cert_common_name_check = true;
    config.if_name = &g_netif_req; // 设置网口
    
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
    rsp_data = (char *)iot_calloc(RX_BUFFER_SIZE);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }

    int read_len = read_http_response(client, rsp_data, RX_BUFFER_SIZE);
    if (read_len > 0) {
        ESP_LOGD(TAG, "Money save response: len %d, %s", read_len, rsp_data);
        
        // 9. 记录请求状态
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGD(TAG, "HTTP POST Status = %d, content_length = %lld",
                 status_code,
                 esp_http_client_get_content_length(client));

        if(status_code == HttpStatus_Ok) {
            if(json_get_money_save_rsp(rsp_data, &saved_money, &temperature_unit) == 0) {
                ESP_LOGI(TAG, "Successfully got money save data: %llu, temperature_unit: %d", saved_money, temperature_unit);
                *temperature_unit_out = temperature_unit;
                if (saved_money > UINT32_MAX) {
                    *saved_money_out = 0xFFFFFFFF;
                } else {
                    *saved_money_out = saved_money;
                }
                ret = 0;
            } else {
                ESP_LOGE(TAG,"%s", rsp_data);
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


#if CONFIG_DISASTER_WARN_CHANNEL == 1 

bool https_disater_state_to_host(disaster_send_param_t StormSendParam, int8_t send_type, char *id){
    if (id == NULL) {
        return false;
    }
    esp_err_t err = -1;
    char Sn_Str[32] = {0};
    sprintf(Sn_Str,"%s%lld", dev_factory.dev_type, dev_factory.dev_sn);
    char *post_data = NULL;
    char *rsp_data = NULL;
    
    if(send_type == 2){
        post_data = json_disaster_response((const char *)dev_factory.dev_type, (const char *)Sn_Str, 
                                            StormSendParam.BackupStartTime, StormSendParam.BackupEndTime, 
                                            (abs(StormSendParam.total_chg_energy_end - StormSendParam.total_chg_energy_begin)), 
                                            (StormSendParam.soc_end- StormSendParam.soc_start), StormSendParam.uuid, 
                                            (const char *) id, StormSendParam.reason.all);
        if (post_data) ESP_LOGW(TAG, "HTTPS_DISASTER END : %s", post_data);
    }    
    else if(send_type == 1){
        post_data = json_disaster_start((const char *)dev_factory.dev_type, (const char *)Sn_Str, 
                                        StormSendParam.BackupStartTime, StormSendParam.uuid, (const char *) id);
        if (post_data) ESP_LOGW(TAG, "HTTPS_DISASTER START : %s", post_data);
    }
    else if((send_type == 0) || (send_type == -1)){
        post_data = json_disaster_rcv_response((const char *)dev_factory.dev_type, (const char *)Sn_Str, 
                                                send_type, DisasterSaveData.SaveWebStormWatchApiData.uuid, (const char *) id);
        if (post_data) ESP_LOGW(TAG, "HTTPS_DISASTER RECV : %s", post_data);
    }
    else {
        return false;
    }

    if (!post_data) return false;
        
    esp_http_client_config_t config = {0};
    if(!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)){
        config.url = DEV_ENV_URL""HTTPS_DISASTER_INTERFACE;
    } else if(!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)){
        config.url = TEST_ENV_URL""HTTPS_DISASTER_INTERFACE;
    } else if(!strcmp(IotSetData.dev_info_t.could_dns, PRO_ENV_HOST)){
        config.url = PRO_ENV_URL""HTTPS_DISASTER_INTERFACE;
    } else{
        ESP_LOGE(TAG, "UNKNOWN HOST, use default host");
        config.url = PRO_ENV_URL""HTTPS_DISASTER_INTERFACE; // 默认生产
    }
    
    config.method = HTTP_METHOD_POST;
    config.disable_auto_redirect = false;
    config.buffer_size_tx = 512;
    config.buffer_size = 512;
    config.timeout_ms = 5000; // 设置超时时间
    config.if_name = &g_netif_req; // 设置网口
    
    // 创建HTTP客户端
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        goto cleanup;
    }
    
    // 设置通用请求头
    if(set_http_header(client) != 0) {
        ESP_LOGE(TAG, "Failed to set HTTP headers");
        goto cleanup;
    }

    // 发送请求
    esp_err_t status = esp_http_client_open(client, strlen(post_data));
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(status));
        goto cleanup;
    }

    int write_len = esp_http_client_write(client, post_data, strlen(post_data));
    if (write_len < 0) {
        ESP_LOGE(TAG, "Failed to write HTTP connection");
        goto cleanup;
    }

    // 接收响应
    rsp_data = (char *)iot_calloc(512);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }

    int read_len = read_http_response(client, rsp_data, 512);
    
    if (read_len > 0) {        
        // 记录请求状态
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGD(TAG, "HTTP POST Status = %d, content_length = %lld",
                 status_code,
                 esp_http_client_get_content_length(client));

        if(status_code == HttpStatus_Ok) {
            err = parse_json_response(rsp_data);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to parse JSON response: %d", err);
            }
        } else {
            ESP_LOGE(TAG, "HTTP POST request failed with status code %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Failed to read HTTP response");
    }  
    
cleanup:

    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (post_data) {
        json_buffer_clear(post_data);
    }
    if (rsp_data) {
        free(rsp_data);
    }

    return (err == ESP_OK);
}

#elif CONFIG_DISASTER_WARN_CHANNEL == 2

bool https_disater_state_to_host(disaster_send_param_t StormSendParam, int8_t send_type) {
    esp_err_t err = ESP_FAIL;
    char Sn_Str[32] = {0};
    sprintf(Sn_Str, "%s%013llu", iot_factory.iot_type, iot_factory.iot_sn);
    char *post_data = NULL;
    uint8_t *rsp_data = NULL;

    switch (send_type) {
        case 2:
            ESP_LOGI(TAG, "END");
            post_data = json_disaster_response(Sn_Str, StormSendParam.BackupStartTime, StormSendParam.BackupEndTime, 
                                               StormSendParam.total_chg_energy_end - StormSendParam.total_chg_energy_begin, 
                                               StormSendParam.soc_end - StormSendParam.soc_start, 
                                               StormSendParam.uuid, StormSendParam.msg_id, iot_factory.iot_type);
            break;
        case 1:
            ESP_LOGI(TAG, "START");
            post_data = json_disaster_start(Sn_Str, StormSendParam.BackupStartTime, StormSendParam.uuid, StormSendParam.msg_id, iot_factory.iot_type);
            break;
        default:
            ESP_LOGI(TAG, "RECV");
            return false;
    }

    if (!post_data) {
        ESP_LOGE(TAG, "Failed to create JSON post data");
        return false;
    }

    esp_http_client_config_t config = {
        .method = HTTP_METHOD_POST,
        .disable_auto_redirect = false,
        .timeout_ms = 5000,
        .if_name = &g_netif_req,
    };

    if (!strcmp(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.could_dns, DEV_ENV_HOST)) {
        config.url = DEV_ENV_URL""HTTPS_DISASTER_INTERFACE;
    } else if (!strcmp(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.could_dns, TEST_ENV_HOST)) {
        config.url = TEST_ENV_URL""HTTPS_DISASTER_INTERFACE;
    } else if (!strcmp(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.could_dns, PRO_ENV_HOST)) {
        config.url = PRO_ENV_URL""HTTPS_DISASTER_INTERFACE;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST: %s, using default host", top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.could_dns);
        config.url = PRO_ENV_URL""HTTPS_DISASTER_INTERFACE;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(post_data);
        return false;
    }

    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        esp_http_client_cleanup(client);
        free(post_data);
        return false;
    }

    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }

    if (esp_http_client_write(client, post_data, strlen(post_data)) < 0) {
        ESP_LOGE(TAG, "Failed to write HTTP connection");
        goto cleanup;
    }

    rsp_data = (uint8_t *)iot_calloc(256);
    if (!rsp_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response data");
        goto cleanup;
    }

    if (read_http_response(client, (char *)rsp_data, 256) > 0) {
        err = json_is_rsp_message_ok((char *)rsp_data);
    } else {
        ESP_LOGE(TAG, "Failed to read HTTP response");
    }

    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));

cleanup:
    if (rsp_data) {
        free(rsp_data);
    }
    if (post_data) {
        free(post_data);
    }
    esp_http_client_cleanup(client);
    return (err == ESP_OK);
}

#endif

/**
 * @brief 更新节省金额和温度单位数据
 * @param void
 * @return int 0-成功更新, -1-HTTP请求失败
 */
static int update_money_save_data(void)
{
    // 请求服务器获取最新的节省金额和温度单位
    uint8_t temperature_unit = 0;
    uint32_t saved_money = 0;
    uint8_t save_money_flag = 0;

    switch (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Time_Span.money_save) {
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

    if (http_request_money_save(dev_factory.dev_sn, dev_factory.dev_type, 
                               save_money_flag, 
                               &temperature_unit, &saved_money) != 0) {
        return -1; // HTTP请求失败
    }

    // 更新温度单位（仅当数值发生变化时）
    if ((IotSetData.dev_info_t.lcd_mode.temperature_unit != temperature_unit) && (temperature_unit != 0)) { 
        // 检查APP设置标志位
        if (IotSetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag == 1) {
            ESP_LOGW(TAG, "APP has set temperature unit(%d), skip HTTP sync", IotSetData.dev_info_t.lcd_mode.temperature_unit);
        } else {
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.LCD_Mode.temperature_unit = temperature_unit;
            IotSetData.dev_info_t.lcd_mode.temperature_unit = temperature_unit;
            reals.SetDataWrFlag.sBit.lcd_mode = 1;
        }
    }
    
    // 更新节省金额（仅当数值发生变化时）
    if (IotSetData.dev_info_t.saved_money != saved_money) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.Save_Money_Nums = saved_money;
        IotSetData.dev_info_t.saved_money = saved_money;
        reals.SetDataWrFlag.sBit.saved_money = 1;
    }

    return 0; // 成功更新
}

#ifdef CONFIG_GRID_OFF_COUNT_ENABLE 

/**
 * @brief 通过HTTP POST上报断电记录到服务器
 * @param start_time 断电开始时间（Unix时间戳）
 * @param end_time 断电结束时间（Unix时间戳）
 * @return 成功返回0，失败返回-1
 */
int http_report_power_outage(uint32_t start_time, uint32_t end_time) {
    char *post_data = NULL;
    char *rsp_data = NULL;
    esp_http_client_handle_t client = NULL;
    int ret = -1;

    // 1. 生成JSON数据（使用空字符串作为备注）
    post_data = json_power_outage_report(start_time, end_time, 
                                        iot_factory.iot_type, 
                                        iot_factory.iot_sn, 
                                        "");
    if (!post_data) {
        ESP_LOGE(TAG, "Failed to create JSON data");
        return -1;
    }
    
    // 2. 配置HTTP客户端
    esp_http_client_config_t config = {
        .method = HTTP_METHOD_POST,
        .disable_auto_redirect = true,
        .timeout_ms = 10000,
        .if_name = &g_netif_req, // 设置网口
    };
    
    // 3. 根据环境设置URL
    if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)) {
        config.url = DEV_ENV_URL""HTTPS_GRID_OFF_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)) {
        config.url = TEST_ENV_URL""HTTPS_GRID_OFF_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
        config.url = PRO_ENV_URL""HTTPS_GRID_OFF_INTERFACE;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST: %s, use PRO_ENV_URL", IotSetData.dev_info_t.could_dns);
        config.url = PRO_ENV_URL""HTTPS_GRID_OFF_INTERFACE;
    }
    
    // 4. 初始化HTTP客户端
    client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    
    // 5. 设置通用HTTP头部
    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto cleanup;
    }
    
    // 6. 设置POST数据
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    // 7. 发送HTTP请求
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    // 8. 检查响应状态码
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    
    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status_code, content_length);
    
    if (status_code == 200) {
        ret = 0; // 成功
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
    }
    
cleanup:
    if (rsp_data) {
        free(rsp_data);
    }
    if (client) {
        esp_http_client_cleanup(client);
    }
    if (post_data) {
        json_buffer_clear(post_data);
    }
    
    return ret;
}

/**
 * @brief 批量上报断电事件到服务器
 * @param records 断电事件记录数组
 * @param record_count 记录数量
 * @return 0-成功, -1-失败
 * @note 设备型号和序列号从全局变量iot_factory中获取
 */
int http_batch_report_power_outages(const void *records, int record_count)
{
    if (!records || record_count <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for batch power outage report");
        return -1;
    }
    
    const power_off_record_t *power_records = (const power_off_record_t *)records;
    char *post_data = NULL;
    char *rsp_data = NULL;
    esp_http_client_handle_t client = NULL;
    int ret = -1;
    
    const char *model = iot_factory.iot_type;
    uint64_t device_sn = iot_factory.iot_sn;
    
    post_data = json_batch_power_outage_report(model, device_sn, power_records, record_count);
    if (!post_data) {
        ESP_LOGE(TAG, "Failed to create batch JSON data");
        return -1;
    }
    
    esp_http_client_config_t config = {
        .method = HTTP_METHOD_POST,
        .disable_auto_redirect = true,
        .timeout_ms = 5000, // 增加太长容易导致线程阻塞复位
        .buffer_size = 1024,
        .buffer_size_tx = 2048, // 增加发送缓冲区
        .if_name = &g_netif_req, // 设置网口
    };
    
    if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)) {
        config.url = DEV_ENV_URL""HTTPS_GRID_OFF_BATCH_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)) {
        config.url = TEST_ENV_URL""HTTPS_GRID_OFF_BATCH_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
        config.url = PRO_ENV_URL""HTTPS_GRID_OFF_BATCH_INTERFACE;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST: %s, use PRO_ENV_URL", IotSetData.dev_info_t.could_dns);
        config.url = PRO_ENV_URL""HTTPS_GRID_OFF_BATCH_INTERFACE;
    }
    
    client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    
    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto cleanup;
    }
    
    esp_err_t err;
    err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    int write_len = esp_http_client_write(client, post_data, strlen(post_data));
    if (write_len < 0) {
        ESP_LOGE(TAG, "Failed to write HTTP data: %d", write_len);
        goto cleanup;
    }
    ESP_LOGI(TAG, "Written %d bytes of POST data", write_len);
    
    int64_t content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch HTTP headers: %lld", content_length);
        goto cleanup;
    }
    
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld", status_code, content_length);
    
    if (status_code == 200) {
        rsp_data = (char *)iot_calloc(2048);
        if (!rsp_data) {
            ESP_LOGE(TAG, "Failed to allocate memory for response data");
            goto cleanup;
        }
        
        int read_len = esp_http_client_read(client, rsp_data, 2047);
        if (read_len > 0) {
            rsp_data[read_len] = '\0'; // 确保字符串结束
            ESP_LOGI(TAG, "Read %d bytes of response data", read_len);
            ESP_LOGD(TAG, "Response data: %s", rsp_data);
            
            int parse_result = json_parse_batch_power_outage_response(rsp_data);
            if (parse_result == 0) {
                ESP_LOGI(TAG, "Batch power outage report successful: %d records", record_count);
                ret = 0; // 成功
            } else {
                ESP_LOGE(TAG, "Server rejected batch power outage report");
            }
        } else {
            ESP_LOGE(TAG, "Failed to read HTTP response: %d", read_len);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
    }
    
    esp_http_client_close(client);
    
cleanup:
    if (rsp_data) {
        free(rsp_data);
    }
    if (client) {
        esp_http_client_cleanup(client);
    }
    if (post_data) {
        json_buffer_clear(post_data);
    }
    
    return ret;
}

#define GRID_OFF_COUNT_RX_BUFFER_SIZE 1024
#define GRID_OFF_COUNT_URL_BUFFER_SIZE 512

/**
 * @brief 获取断电统计次数数据
 * @param stats_out 输出的统计数据结构指针
 * @return 0-成功, -1-失败
 * @note 设备型号和序列号从全局变量iot_factory中获取
 */
int http_get_grid_off_count(grid_off_count_statistics_t *stats_out)
{    
    esp_http_client_handle_t client = NULL;
    char *rsp_data = NULL;
    char *full_url = NULL;
    int ret = -1;
    
    /* 检查输入参数 */
    if (stats_out == NULL) {
        ESP_LOGE(TAG, "Invalid input parameter: stats_out is NULL");
        return -1;
    }
    
    /* 获取设备信息 */
    const char *model = iot_factory.iot_type;
    uint64_t device_sn = iot_factory.iot_sn;
    
    /* 分配URL缓冲区 */
    full_url = (char *)iot_calloc(GRID_OFF_COUNT_URL_BUFFER_SIZE);
    if (full_url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL buffer");
        return -1;
    }
    
    /* 构建完整的GET请求URL */
    const char *base_url = NULL;
    if (!strcmp(IotSetData.dev_info_t.could_dns, DEV_ENV_HOST)) {
        base_url = DEV_ENV_URL""HTTPS_GET_GRID_OFF_COUNT_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, TEST_ENV_HOST)) {
        base_url = TEST_ENV_URL""HTTPS_GET_GRID_OFF_COUNT_INTERFACE;
    } else if (!strcmp(IotSetData.dev_info_t.could_dns, MASS_PRODUCTION_ENV_HOST_URL)) {
        base_url = PRO_ENV_URL""HTTPS_GET_GRID_OFF_COUNT_INTERFACE;
    } else {
        ESP_LOGE(TAG, "UNKNOWN HOST: %s, use PRO_ENV_URL", IotSetData.dev_info_t.could_dns);
        base_url = PRO_ENV_URL""HTTPS_GET_GRID_OFF_COUNT_INTERFACE;
    }
    
    /* 拼接URL参数 */
    snprintf(full_url, GRID_OFF_COUNT_URL_BUFFER_SIZE, "%s?deviceSn=%s%llu", 
             base_url, model, device_sn);
    
    // ESP_LOGI(TAG, "Request URL: %s", full_url);
    
    /* 配置HTTP客户端 */
    esp_http_client_config_t config = {
        .url = full_url,
        .method = HTTP_METHOD_GET,
        .disable_auto_redirect = true,
        .timeout_ms = 5000,
        .buffer_size = 1024,
        .buffer_size_tx = 0,
        .if_name = &g_netif_req, // 设置网口
    };
    
    /* 初始化HTTP客户端 */
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    
    /* 设置HTTP头部 */
    if (set_http_header(client) < 0) {
        ESP_LOGE(TAG, "Failed to set HTTP header");
        goto cleanup;
    }

    /* 执行分步骤HTTP GET请求 */
    esp_err_t err;
    
    /* 打开HTTP连接 */
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    /* 写入请求数据（GET请求无需写入数据，但调用此函数发送请求头） */
    int write_len = esp_http_client_write(client, NULL, 0);
    if (write_len < 0) {
        ESP_LOGE(TAG, "Failed to write HTTP request: %d", write_len);
        goto cleanup;
    }
    // ESP_LOGI(TAG, "HTTP GET request sent successfully");
    
    /* 获取响应头 */
    int64_t content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch HTTP headers: %lld", content_length);
        goto cleanup;
    }
    
    /* 检查响应状态码 */
    int status_code = esp_http_client_get_status_code(client);
    // ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld", status_code, content_length);
    
    if (status_code == 200) {
        /* 读取响应数据 */
        rsp_data = (char *)iot_calloc(GRID_OFF_COUNT_RX_BUFFER_SIZE);
        if (!rsp_data) {
            ESP_LOGE(TAG, "Failed to allocate memory for response data");
            goto cleanup;
        }
        
        int read_len = esp_http_client_read(client, rsp_data, GRID_OFF_COUNT_RX_BUFFER_SIZE - 1);
        if (read_len > 0) {
            rsp_data[read_len] = '\0';   /* 确保字符串结束 */
            // ESP_LOGI(TAG, "Read %d bytes of response data", read_len);
            ESP_LOGI(TAG, "Blackout stats response: %s", rsp_data);
            
            /* 解析响应数据 */
            int parse_result = json_parse_grid_off_stats_response(rsp_data, stats_out);
            if (parse_result == 0) {
                ESP_LOGI(TAG, "Successfully got blackout statistics");
                ret = 0;   /* 成功 */
            } else {
                ESP_LOGE(TAG, "Failed to parse blackout stats response");
            }
        } else {
            ESP_LOGE(TAG, "Failed to read HTTP response: %d", read_len);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
    }
    
    /* 关闭HTTP连接 */
    esp_http_client_close(client);
    
cleanup:
    if (rsp_data) {
        free(rsp_data);
    }
    if (client) {
        esp_http_client_cleanup(client);
    }
    if (full_url) {
        free(full_url);
    }
    
    return ret;
}

#endif
