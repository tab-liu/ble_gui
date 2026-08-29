/**
  ******************************************************************************
  * @file      open_mqtt.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/2/5
  * @brief     MQTT_OPEN模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/2/5   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */
  
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "mqtt_client.h"
#include <net/if.h>

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

typedef struct {
    char raw_url[128];//服务器网址+端口
    char password[64];

    int sock;
    char host[120];//从raw_url解析的服务器网址
    uint8_t ipaddr[4];//从raw_url解析的服务器IP char
    uint16_t port;//从raw_url解析的服务器端口

    char dev_type[12];
    uint64_t dev_sn;

    char sub_topic[128];//订阅接收,recv_topic
    char pub_topic[128];//数据发送主题,send_topic
    QueueHandle_t  mqtt_msg_queue;
    esp_mqtt_client_handle_t  mqtt_client;
    TimerHandle_t push_timer;
    uint8_t mqtt_is_login;
    uint8_t mqtt_is_start;
    
    struct ifreq netif_req;
    uint8_t current_netif_id;
    uint8_t mqtt_connect_fail;
}open_mqtt_config_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

int open_mqtt_task_init(void);
void set_open_mqtt_client_netif(uint8_t netif_type);
void open_mqtt_State_Machine(void);
void open_mqtt_start(void);
void set_open_mqtt_stop(void); 
void open_mqtt_new_dev(const char *dev_type, uint64_t dev_sn);
void open_mqtt_new_host(const char *new_host);
void open_mqtt_new_password(const char *new_password);
int open_mqtt_send(const uint8_t *data, int len);
uint16_t Open_Mqtt_Module_Announce(void);

#ifdef __cplusplus
}
#endif

