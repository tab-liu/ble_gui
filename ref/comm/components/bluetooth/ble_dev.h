#pragma once
#include <stdint.h>
#include "drv_nimble.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_RX_MAX_SZ       1400            // APP发送的指令数据最大大小
#define BLE_EVT_ADV         NIMBLE_EVT_ADV  // ble广播事件
#define BLE_EVT_CONNECT     NIMBLE_EVT_CONNECT
#define BLE_EVT_DISCONNECT  NIMBLE_EVT_DISCONNECT
#define BLE_EVT_SUBSCRIBE   NIMBLE_EVT_SUBSCRIBE

/**
 * @brief 定义蓝牙数据结构
 *
 */
typedef struct {
    uint8_t *data;  // 数据指针
    int len;        // 数据长度
} ble_data_t;

typedef struct {
    uint8_t connected;
    uint8_t mac_addr[6];
} ble_sta_t;

/**
 * @brief 定义蓝牙事件回调函数
 * 
 */
typedef nimble_event_cb_t ble_event_cb_t;

int ble_dev_init(const char *name, ble_event_cb_t event_cb);
void ble_dev_deinit(void);
void ble_dev_reinit(const char *name, ble_event_cb_t event_cb);
int ble_disconnect(void);
int ble_dev_recv(ble_data_t *data, int timeout);
int ble_dev_send(ble_data_t *data);
int ble_dev_post(ble_data_t *data);
int ble_set_name(const char *name);
void ble_get_mac_addr(ble_sta_t *sta);
void ble_get_status(void);
void ble_debug(uint8_t *data, int len);
int ble_advertise_stop(void);
int ble_advertise_start(void);

#ifdef __cplusplus
}
#endif

