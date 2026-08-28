/*
 * File : drv_nimble.h
 * 该文件是智能插座软件工程的一部分
 * 该文件使用nimble主机初始化蓝牙设备，同时创建一个服务与两个特征
 * 一个特征用于接收用户数据，另一个特征用于发送本地数据到用户
 * 需要在idf.py menuconfig中作如下配置：
 * Maximum number of concurrent connections最大并发连接数设置为1
 * Enable BLE Central role中心设备角色关闭
 * Enable BLE Observer role蓝牙观察者角色关闭
 * Preferred MTU size in octets蓝牙预置MTU设置为247
 * Maximum number of GATT client procedures最大GATT客户端程序数量设置为1
 * 蓝牙调试完成后将nimble的日志等级设置为告警级别：NimBLE Host log verbosity (Warning logs)
 * 
 * Change Logs:
 * Date         Author          Notes
 * 2024-03-04   heyinping       初始版本
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 蓝牙默认名称 */
#define BLE_DEFAULT_NAME    "IOT123456"

#define NIMBLE_EVT_ADV      1   // 广播事件




/**
 * @brief 定义蓝牙数据信息结构
 * 
 */
typedef struct {
    uint8_t conn_sta;       // 蓝牙连接状态
    uint16_t mtu_size;      // 蓝牙MTU大小
    uint16_t rx_timeout;    // 蓝牙接收超时时间（当central设备数据分包发送时，包之间的超时时间）
    const char *dev_name;   // 蓝牙设备名称


} ble_info_t;

typedef union {
    uint8_t all;

    struct {
        uint8_t connectEvent : 1;//蓝牙已连接事件
        uint8_t res : 7;//保留
    }bit;
}ble_event_t;

typedef struct{
    ble_event_t ble_event;
} ble_EventInfo_t;

typedef void (*app_rx_callback_t)(uint8_t *data, uint8_t len);
typedef void (*nimble_event_cb_t)(uint8_t evt);

int drv_ble_init(const char *name, app_rx_callback_t cb, nimble_event_cb_t event_cb);
int drv_ble_deinit(void);
int drv_ble_disconnect(void);
int drv_ble_chr1_send(uint8_t *data, int size);
int drv_ble_chr3_send(uint8_t *data, int size);
int drv_ble_set_dev_name(const char *name);
int drv_ble_advertise_stop(void);
int drv_ble_advertise_start(void);
int drv_ble_advertise_pause(void);
int drv_ble_advertise_restart(void);
bool drv_ble_advertise_status(void);
ble_info_t* drv_ble_info_get(void);
uint8_t drv_ble_get_ConnectEvent(void);
void drv_ble_get_mac_addr(uint8_t mac[6]);

void Print_debug1(void) ;
int drv_ble_advertise_stop2(void);
char* drv_ble_name_get(void);

uint8_t ble_adv_mfg_data_default(uint8_t *mfg_data);
int16_t set_ble_adv_data(uint8_t *mfg_data, uint8_t mfg_data_len);

#ifdef __cplusplus
}
#endif
