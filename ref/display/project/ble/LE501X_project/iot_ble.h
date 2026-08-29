/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_ble.h
Author: weiyt
Version: V1.0
Date: 2022.5.23
Description: none
*************************************************/

#ifndef IOT_BLE_H
#define IOT_BLE_H

#include <string.h>
#include <stdio.h>
#include "ls_ble.h"
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "builtin_timer.h"
#include "ls_hal_trng.h"
#include "platform.h"

#define BLE_RX_SIZE 1200
#define BLE_TX_SIZE 1200

#define DISPLAY_TIMEOUT_15S (1)
#define DISPLAY_TIMEOUT_30S (2)
#define DISPLAY_TIMEOUT_60S (3)
#define DISPLAY_TIMEOUT_300S (4)
#define DISPLAY_TIMEOUT_INF (5)
#define DISPLAY_TIMEOUT_PRODUCTION (6) // 用于产测老化添加
#define DISPLAY_TIMEOUT_DFT DISPLAY_TIMEOUT_30S

typedef struct {
    // uart_rx
    uint8_t rx_state;
    uint16_t rx_size;
    uint16_t rx_offset;
    uint8_t rx_buf[BLE_RX_SIZE];

    // uart tx
    uint8_t tx_state;
    uint16_t tx_size;
    uint16_t tx_offset;
    uint8_t tx_buf[BLE_TX_SIZE];

    uint8_t self_rx_state;
    uint16_t self_rx_size;
    uint16_t self_rx_offset;
    uint8_t self_rx_buf[BLE_RX_SIZE];

    uint8_t self_tx_state;
    uint16_t self_tx_size;
    uint16_t self_tx_offset;
    uint8_t self_tx_buf[500];

} ble_data_t;

typedef struct identify_handr {
    uint8_t status;
    uint8_t md5_rst[16];
    uint8_t MD5_DATA[4]; /*MD5第9-12位*/
    bool rst_first;
    bool rst_second;
    bool Fct_test;
} identify_handr_t;

typedef struct ble_para {
    bool conn_prar_update_flag; /*更新连接标志*/
    bool adv_update_flag;
    uint8_t connect_id;
    uint8_t connect_status; /*连接状态*/
    uint8_t is_enable; /*使能开关*/
    uint8_t ble_mac[6]; /*自身mac地址*/
    // uint8_t remote_mac[6]; /*对方mac地址*/
    uint8_t device_name[32]; /*设备名称*/
    uint8_t tx_power; /*发射功率*/
    uint8_t adv_enable; /*广播开关*/
    uint8_t ota_enable;
    uint16_t conn_int; /*连接间隔*/
    uint16_t latency;
    uint16_t timeout;
    uint16_t max_wait_time;
    bool can_send_notify;
    bool self_can_send_notify;
    uint8_t current_notify;
    uint16_t mtu_size;
} ble_para_t;

typedef struct ble_save_para {
    uint16_t save_flag;
    uint8_t app_pwd[6];
    uint8_t iot_type[12];
    char ble_name[32];
    uint64_t iot_sn;
    uint64_t safe_code;
} ble_save_para_t;

typedef enum {
    PAIRING_MODE_INVALID = 0, // 无效
    PAIRING_MODE_SPECIFIC, // 指定配对
    PAIRING_MODE_AUTOMATIC, // 自动配对
} pairing_mode_t;

typedef struct magnetic_screen_para {
    uint16_t save_flag;
    uint16_t display_timeout; // 废弃，但保留字段，用于兼容旧版本
    pairing_mode_t pairing_mode;
    uint8_t temp_unit;
    char target_name[12];
    uint64_t target_sn;
    uint8_t target_key[16];
    uint8_t target_mac[6];
} magnetic_screen_para_t;

// clang-format off
enum {
    BLE_RX_CPMPLATE = 1,
    BLE_RX_IDLE,
    BLE_TX_CPMPLATE,
    BLE_TX_IDLE,
};
// clang-format on

// 加解密使用的key类型，鉴权阶段使用128位，数据传输阶段使用256位，这里表明是鉴权阶段还是数据传输阶段
typedef enum {
    CLIENT_IDENTITY_KEY,
    CLIENT_DATA_KEY,
} client_aes_key_t;

extern magnetic_screen_para_t magnetic_screen_para;

extern void make_ecc_key_startup_init(void);
bool is_client_encryption_required();
extern uint8_t AES_BleSendBytes(uint8_t *data, uint16_t len, bool type_aes);
extern void iot_ble_init(void);
extern uint8_t BleSendBytes(uint8_t *data, uint16_t len);
extern uint8_t *BleRecvBytes(uint16_t *msgLen);
extern void modify_scanRespon_name(char *name);
extern void adv_switch(uint8_t enable);
extern uint8_t self_BleSendBytes(uint8_t *data, uint16_t len);
extern uint8_t *self_BleRecvBytes(uint16_t *msgLen);
extern uint8_t save_to_tinyfs(void);
extern uint8_t update_to_tinyfs(void);
extern void save_magnetic_screen_para_to_tinyfs(void);
extern void clear_magnetic_screen_para(void);
extern void Set_Magnetic_Screen_Para_pairing_mode(pairing_mode_t pairing_mode);
extern void Set_Magnetic_Screen_Para_target(uint8_t *target);
extern uint8_t readPwd_from_tinyfs(void);
extern uint8_t savePwd_to_tinyfs(void);
extern bool get_ble_status(void);
extern void ble_adv_abort(void);
extern void disconnect_ble(void);
extern void init_bond(void);
extern int cmp_data_valid(uint16_t plaintxet_len, uint16_t rx_buf_size);
void ble_client_send_data(uint8_t *data, uint16_t len);
void ble_client_send_data_aes(uint8_t *data, uint16_t len, client_aes_key_t key_type);
void key_2_start_adv();
bool is_fast_match_mode(void);
bool is_ble_pairing_mode(void);
void set_fast_match_mode(bool enable);
void key_2_start_fast_match(void);
#endif
