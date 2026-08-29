/**
  ******************************************************************************
  * @file      usb_host_modem.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/2
  * @brief     USB转4G控制模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/26  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */
 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "usb_host_common.h"
#include "usb_host_modem.h"
#include "utils.h"

// 下列文件为乐鑫官方库，移植本地做部分更改以匹配项目需求
#include "usbh_modem_board.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[USB_HOST_MODEM]"

// 自定义默认配置
#define MODEM_DEFAULT_CONFIG_SELF()           \
    {                                \
        .rx_buffer_size = 1024*2,   \
        .tx_buffer_size = 1024*2,   \
        .line_buffer_size = 1024*2,    \
        .event_task_priority = CONFIG_USBH_TASK_BASE_PRIORITY + 1,\
        .event_task_stack_size = 1024*5 \
    }

/* =============================== 文件内全局变量 ================================ */

// modem_board任务配置
static USE_EXT_RAM_BSS modem_config_t Modem_Task_Config = {0};

// 4G详细数据（对外）
static USE_EXT_RAM_BSS usb_4g_info_t _4g_info = {0};
    
// 4G功能开关
static bool IOT_4G_Switch = false;

// 4G模块任务状态机
static uint8_t Usb_4G_Config_Step = STEP_MODEM_IDLE;

// USB设备接入状态
static bool Usb_Dev_Conned_Status = false;

// SIM卡插入状态
static bool Sim_Card_Conned_Status = false;

// 4G网络连接状态
static bool Network_Conned_Status = false;

// 4G模块初始化时间
static uint32_t Usb_4G_Init_TickCount_MS = 0;

/* ================================ 模块函数定义 ================================ */

/**
 * @brief 4G模块事件回调处理函数
 * @param arg         用户自定义参数（一般不用）
 * @param event_base  事件基类型（如 MODEM_BOARD_EVENT）
 * @param event_id    事件ID（如 MODEM_EVENT_SIMCARD_CONN 等）
 * @param event_data  事件数据指针（视事件类型而定，常为NULL）
 */
static void on_modem_self_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == MODEM_BOARD_EVENT) {
        if (event_id == MODEM_EVENT_SIMCARD_DISCONN) {
            ESP_LOGW(TAG, "Modem Board Event: SIM Card disconnected");
            Sim_Card_Conned_Status = false;
        } else if (event_id == MODEM_EVENT_SIMCARD_CONN) {
            ESP_LOGW(TAG, "Modem Board Event: SIM Card Connected");
            Sim_Card_Conned_Status = true;
        } else if (event_id == MODEM_EVENT_DTE_DISCONN) {
            ESP_LOGW(TAG, "Modem Board Event: USB disconnected");
            Usb_Dev_Conned_Status = false;
            Sim_Card_Conned_Status = false;
            Network_Conned_Status = false;
        } else if (event_id == MODEM_EVENT_DTE_CONN) {
            ESP_LOGW(TAG, "Modem Board Event: USB connected");
            Usb_Dev_Conned_Status = true;
        } else if (event_id == MODEM_EVENT_DTE_RESTART) {
            ESP_LOGW(TAG, "Modem Board Event: Hardware restart");
        } else if (event_id == MODEM_EVENT_DTE_RESTART_DONE) {
            ESP_LOGW(TAG, "Modem Board Event: Hardware restart done");
        } else if (event_id == MODEM_EVENT_NET_CONN) {
            ESP_LOGW(TAG, "Modem Board Event: Network connected");
            Network_Conned_Status = true;
        } else if (event_id == MODEM_EVENT_NET_DISCONN) {
            ESP_LOGW(TAG, "Modem Board Event: Network disconnected");
            Network_Conned_Status = false;
        } else if (event_id == MODEM_EVENT_DEV_ID_GET) {
            ESP_LOGW(TAG, "Modem Board Event: Device ID obtained");
        } else {
            ESP_LOGE(TAG, "Modem Board Event: Unknown_event_id(%ld)", event_id);
        }
    }
}

/**
 * @brief USB 4G模块初始化
 * 
 * 初始化4G USB Modem模块，配置缓冲区、事件回调、初始化标志等参数，并启动modem板卡。
 * 若未进入PPP，modem将进入命令模式；
 * 若不等待modem ready，则初始化后直接返回。
 */
static void Usb_4G_Module_Init(void)
{
    // 检查初始化状态
    if( 0 != Usb_4G_Init_TickCount_MS ) return;
    
#if 0   // 外部初始化
    /* Initialize default TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#endif

    /* Waiting for modem powerup */
    ESP_LOGI(TAG, "Usb_4G_Module_Init Start!");

    /* Initialize modem board. Dial-up internet */
    Modem_Task_Config = (modem_config_t)MODEM_DEFAULT_CONFIG_SELF();
    
    /* Modem init flag, used to control init process */
    // Modem_Task_Config.flags |= MODEM_FLAGS_INIT_NOT_FORCE_RESET;
    Modem_Task_Config.flags |= MODEM_FLAGS_INIT_NOT_ENTER_PPP;
    Modem_Task_Config.flags |= MODEM_FLAGS_INIT_NOT_BLOCK;
    
    Modem_Task_Config.handler = on_modem_self_event;
    if( ESP_OK == modem_board_init(&Modem_Task_Config) ) {
        Usb_4G_Init_TickCount_MS = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
}

/**
 * @brief 4G模块去初始化
 *
 * 此函数用于释放4G模块相关资源，关闭电源，并清空配置结构体。
 */
static void Usb_4G_Module_Deinit(void)
{
    ESP_LOGI(TAG, "Usb_4G_Module_Deinit : Deinitializing 4G module...");
    modem_board_deinit();
    memset(&_4g_info, 0, sizeof(_4g_info));
    memset(&Modem_Task_Config, 0, sizeof(Modem_Task_Config));   
    Usb_4G_Init_TickCount_MS = 0;
    ESP_LOGI(TAG, "Usb_4G_Module_Deinit : 4G module deinit complete.");
}

/**
 * @brief 更新4G模块信息数据
 *
 * 此函数从modem_info_t结构体获取最新的4G模块信息，并同步更新到全局的_4g_info结构体中。
 * 包括IMEI、IMSI、ICCID、MAC地址、DNS、IP、网关、掩码、信号强度、误码率、网络连接状态和SIM卡状态等。
 * 主要用于应用层获取4G模块的实时状态和参数。
 */
static void Usb_4G_Module_Data_Update(void)
{
	static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /*周期500ms执行*/
    if ((now_time - pre_time) < 500) return;
    pre_time = now_time;

    if ( Usb_Dev_Conned_Status ) {
        const modem_info_t *modem_info = modem_board_get_info();
        
        memcpy(_4g_info.IMEI, modem_info->IMEI, sizeof(_4g_info.IMEI));
        memcpy(_4g_info.mac, modem_info->ppp_mac, sizeof(_4g_info.mac));

        if ( Sim_Card_Conned_Status ) {
            memcpy(_4g_info.IMSI, modem_info->IMSI, sizeof(_4g_info.IMSI));
            memcpy(_4g_info.ICCID, modem_info->ICCID, sizeof(_4g_info.ICCID));
        } else {
            memset(_4g_info.IMSI, 0, sizeof(_4g_info.IMSI));
            memset(_4g_info.ICCID, 0, sizeof(_4g_info.ICCID));
        }

        if ( Network_Conned_Status ) {
            _4g_info.dns1[0] = esp_ip4_addr1(&modem_info->ppp_dns_ip_main.u_addr.ip4);
            _4g_info.dns1[1] = esp_ip4_addr2(&modem_info->ppp_dns_ip_main.u_addr.ip4);
            _4g_info.dns1[2] = esp_ip4_addr3(&modem_info->ppp_dns_ip_main.u_addr.ip4);
            _4g_info.dns1[3] = esp_ip4_addr4(&modem_info->ppp_dns_ip_main.u_addr.ip4);
            
            _4g_info.dns2[0] = esp_ip4_addr1(&modem_info->ppp_dns_ip_backup.u_addr.ip4);
            _4g_info.dns2[1] = esp_ip4_addr2(&modem_info->ppp_dns_ip_backup.u_addr.ip4);
            _4g_info.dns2[2] = esp_ip4_addr3(&modem_info->ppp_dns_ip_backup.u_addr.ip4);
            _4g_info.dns2[3] = esp_ip4_addr4(&modem_info->ppp_dns_ip_backup.u_addr.ip4);
            
            _4g_info.ipv4[0] = esp_ip4_addr1(&modem_info->ppp_ip_info.ip);
            _4g_info.ipv4[1] = esp_ip4_addr2(&modem_info->ppp_ip_info.ip);
            _4g_info.ipv4[2] = esp_ip4_addr3(&modem_info->ppp_ip_info.ip);
            _4g_info.ipv4[3] = esp_ip4_addr4(&modem_info->ppp_ip_info.ip);
            
            _4g_info.gw[0] = esp_ip4_addr1(&modem_info->ppp_ip_info.gw);
            _4g_info.gw[1] = esp_ip4_addr2(&modem_info->ppp_ip_info.gw);
            _4g_info.gw[2] = esp_ip4_addr3(&modem_info->ppp_ip_info.gw);
            _4g_info.gw[3] = esp_ip4_addr4(&modem_info->ppp_ip_info.gw);
            
            _4g_info.mask[0] = esp_ip4_addr1(&modem_info->ppp_ip_info.netmask);
            _4g_info.mask[1] = esp_ip4_addr2(&modem_info->ppp_ip_info.netmask);
            _4g_info.mask[2] = esp_ip4_addr3(&modem_info->ppp_ip_info.netmask);
            _4g_info.mask[3] = esp_ip4_addr4(&modem_info->ppp_ip_info.netmask);
            
            _4g_info.rssi = (uint8_t)modem_info->rssi;
            _4g_info.ber = (uint8_t)modem_info->ber;
        }else { 
            memset(_4g_info.dns1, 0, sizeof(_4g_info.dns1));
            memset(_4g_info.dns2, 0, sizeof(_4g_info.dns2));
            memset(_4g_info.ipv4, 0, sizeof(_4g_info.ipv4));
            memset(_4g_info.gw, 0, sizeof(_4g_info.gw));
            memset(_4g_info.mask, 0, sizeof(_4g_info.mask));
            _4g_info.rssi = 0;
            _4g_info.ber = 0;
        }
        
        _4g_info.net_status = Network_Conned_Status;
        _4g_info.sim_card_status = Sim_Card_Conned_Status;
        
        if ( modem_info->gps_info.fix ) {
            _4g_info.GPS_X = (int)(modem_info->gps_info.longitude * 1000000);
            _4g_info.GPS_Y = (int)(modem_info->gps_info.latitude  * 1000000);
        } else {
            _4g_info.GPS_X = 0;
            _4g_info.GPS_Y = 0;
        }

        _4g_info.gps_num = modem_info->gps_info.gps_num;
        _4g_info.gl_num = modem_info->gps_info.gl_num;
        _4g_info.gal_num = modem_info->gps_info.gal_num;
        _4g_info.bd_num = modem_info->gps_info.bd_num;
    } else {
        memset(&_4g_info, 0, sizeof(_4g_info));
    }
}

/**
 * @brief USB 4G模块状态机
 * 
 * 根据当前Usb_4G_Config_Step状态，执行4G模块的初始化、待机等操作。
 */
void Usb_4G_Module_State_Machine(void)
{
    static bool curr_status = false;

    // 启动4G模块初始化流程
    Usb_4G_Module_Init();

    // 检查初始化状态
    if( 0 == Usb_4G_Init_TickCount_MS ) return;

    // 4G功能开关检查
    if ( curr_status != IOT_4G_Switch ) {
        if ( IOT_4G_Switch ) {
            // 启用4G模块数据功能
            Usb_4G_Config_Step = STEP_MODEM_PPP_START;
        } else {
            // 停用4G模块数据功能
            Usb_4G_Config_Step = STEP_MODEM_PPP_STOP;
        }
        curr_status = IOT_4G_Switch;
    }

    // 状态机检测
    switch (Usb_4G_Config_Step)
    {
        // Modem空闲：无操作进行中
        case STEP_MODEM_IDLE:
            // 更新4G模块数据
            Usb_4G_Module_Data_Update();
            break;

        // 启用4G模块数据功能
        case STEP_MODEM_PPP_START:
            // 4G模块已连接时允许该操作
            if ( Usb_Dev_Conned_Status ) {
                modem_board_ppp_auto_connect(true);
                modem_board_ppp_start(0);
                Usb_4G_Config_Step = STEP_MODEM_RUNNING;
            }
            break;

        // Modem运行中：Modem处于正常工作状态    
        case STEP_MODEM_RUNNING:
            // 更新4G模块数据
            Usb_4G_Module_Data_Update();
            break;

        // 停用4G模块数据功能
        case STEP_MODEM_PPP_STOP:
            modem_board_ppp_auto_connect(false);
            modem_board_ppp_stop(0);
            Usb_4G_Config_Step = STEP_MODEM_IDLE;
            break;

        // Modem未知响应：收到未知或异常响应
        case STEP_MODEM_UNKNOWN:
            ESP_LOGE(TAG, "Usb_4G_Module_State_Machine: Unknown state, reset to IDLE.");
            Usb_4G_Config_Step = STEP_MODEM_IDLE;
            break;

        default:
            // 其他未定义状态
            ESP_LOGE(TAG, "Usb_4G_Module_State_Machine: Invalid state %d, reset to IDLE.", Usb_4G_Config_Step);
            Usb_4G_Config_Step = STEP_MODEM_IDLE;
            break;
    }
}

/**
 * @brief 启用4G模块
 */
void iot_4G_start(void)
{
    IOT_4G_Switch = true;
}

/**
 * @brief 停用4G模块
 */
void iot_4G_stop(void)
{
    IOT_4G_Switch = false;
}

/**
 * @brief 获取4G网络连接状态
 *
 * @return true 表示已连接，false 表示未连接
 */
bool iot_4g_net_get_status(void)
{
    return Network_Conned_Status;
}

/**
 * @brief 获取4G模块信息结构体指针
 *
 * @return 指向当前4G模块信息的usb_4g_info_t结构体常量指针
 */
const usb_4g_info_t * iot_4g_info_get(void)
{
    return &_4g_info;
}

/**
 * @brief 设置4G模块的APN参数
 *
 * 本函数用于设置4G模块的接入点名称（APN）。
 * - 如果当前4G模块处于空闲状态（STEP_MODEM_IDLE），则直接初始化APN；
 * - 如果不处于空闲状态，则根据USB设备连接状态，调用不同方式设置APN。
 *
 * @param new_apn 指向新APN字符串的指针
 */
void iot_4g_set_apn(const char *new_apn)
{
    if ( 0 == strlen(new_apn) ) {
        ESP_LOGW(TAG, "Modem APN initial invalid.");
        return;
    }
    
    if (STEP_MODEM_IDLE == Usb_4G_Config_Step) {
        ESP_LOGI(TAG, "Modem idle, initializing APN(%s).", new_apn);
        modem_board_apn_init(new_apn);
    } else {
        if (Usb_Dev_Conned_Status) {
            ESP_LOGI(TAG, "USB device connected, setting APN (%s).", new_apn);
            modem_board_set_apn(new_apn, true);
        } else {
            ESP_LOGI(TAG, "USB device not connected, setting APN (%s).", new_apn);
            modem_board_set_apn(new_apn, false);
        }
    }
}

/**
 * @brief 获取4G模块初始化时间
 */
uint32_t iot_4g_init_time_get(void)
{
    return Usb_4G_Init_TickCount_MS;
}

/**
 * @brief 获取4G信号强度
 */
int8_t iot_4g_rssi(void) {
    return ((_4g_info.rssi == 99) ? 0 : ((int8_t)-113 + (int8_t)_4g_info.rssi * 2));
}

