#ifndef IOT_WIFI_INIT_H
#define IOT_WIFI_INIT_H

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_wifi.h"
#include "mesh_api.h"

#define WIFI_MODE_STA   1
#define WIFI_MODE_AP    2
#define WIFI_MODE_ALL   3

#define WIFI_AP_PWD_LEN_MIN 8 // AP密码最小长度
#define WIFI_STA_INDEX_MAX 3 // 最大支持3个WIFI STA

#pragma pack(1)

typedef union {
    uint16_t all;
    struct {
        uint8_t enable : 1; // wifi 开启
        uint8_t sta_enable : 2; // 0：关闭，1：打开，2：故障;state, not cmd;STA模式使能情况下，才尝试连接AP
        uint8_t ap_enable  : 2; // 0：关闭，1：打开，2：故障

        uint8_t sta_link_ap : 1; // 0：断开；1：连接
        uint8_t ap_is_link  : 1; // 0：断开；1：被连接

        uint8_t mqtt_login : 1; // 0：未登录，1：登录
        uint8_t smart_cfg  : 1; // 0：关闭，1：开启
    }bit;
}wifi_flags_t;


typedef struct {
    wifi_flags_t  flag;
    uint8_t sta_ipv4[4];
    uint8_t sta_gw[4];
    uint8_t sta_mask[4];
    uint8_t sta_dns1[4];
    uint8_t sta_dns2[4];
    uint8_t sta_mac[6];
    uint8_t fault_code;
    uint8_t wait_smart_cfg;
    uint8_t sta_index;
    uint8_t sta_auth;
    char sta_ssid[32];//64
    char sta_pwd[64];
}wifi_info_t;

typedef struct {
    uint8_t sta_index;
    uint16_t *sta_auth;
    char *ssid;
    char *password;
}wifi_mmult_sta_base_t;

typedef struct {
    uint8_t sta_index;
    uint32_t *sta_set_ip;
    uint32_t *sta_set_mask;
    uint32_t *sta_set_gw;
    uint32_t *sta_set_dns1;
    uint32_t *sta_set_dns2;
}wifi_mmult_sta_static_ip_t;

typedef struct {
    uint8_t chl;
    int rssi;
    bool found;
} sta_scan_result_t;

#pragma pack()

enum {
    WIFI_STA_INDEX_UNKNOWN = 0,
    WIFI_STA_INDEX1 = 1,
    WIFI_STA_INDEX2 = 2,
    WIFI_STA_INDEX3 = 3,
    WIFI_STA_INDEX_STOP,
};

extern wifi_info_t wifi_info;//RAM中间变量集合static 
extern wifi_info_t _4g_info;//RAM中间变量集合static 
extern wifi_info_t eth_info;//RAM中间变量集合static 


void iot_wifi_task(void * pvParameters);

const wifi_info_t * iot_get_wifi_info(void);
const wifi_info_t * iot_get_4g_info(void);
const wifi_info_t * iot_get_eth_info(void);

void iot_wifi_reset(void);
void iot_wifi_close(uint8_t mode);
void iot_wifi_open(uint8_t mode);

void iot_wifi_open_smartcfg(void);
void iot_wifi_close_smartcfg(void);

void iot_wifi_new_host(const char *host);
void iot_wifi_new_iot(const char *iot_type, uint64_t iot_sn, uint64_t safety_code);
void iot_wifi_new_dev(const char *dev_type, uint64_t dev_sn);
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
void iot_wifi_new_ap(uint8_t auth, const char *ssid, const char *pwd, const char *bssid);
void iot_wifi_reconnect_sta(uint8_t auth, const char *ssid, const char *pwd, const char *bssid);
#else
void iot_wifi_new_ap(uint8_t auth, const char *ssid, const char *pwd);
void iot_wifi_reconnect_sta(uint8_t auth, const char *ssid, const char *pwd);
#endif
void iot_wifi_ap_config(uint8_t auth, const char *ssid, const char *pwd);
int8_t iot_wifi_ap_rssi(void);
void iot_mqtt_state(void);
const char * iot_wifi_get_host(void);
void iot_wifi_connect_status_check(void);
void iot_wifi_new_connect(uint8_t STA_index);
void iot_wifi_auto_connect(void);
void iot_wifi_base_cfg_update(void);
uint8_t iot_wifi_sta_index_get(void);
uint8_t iot_wifi_sta_link_status_get(void);
void wifi_sta_update_flag_set(void);
#endif
