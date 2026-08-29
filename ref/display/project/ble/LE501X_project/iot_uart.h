/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_uart.h
Author: weiyt
Version: V1.0
Date: 2022.5.23
Description: none
*************************************************/
#ifndef IOT_UART_H
#define IOT_UART_H

#include "ls_hal_uart.h"
#include "platform.h"
#include "ls_soc_gpio.h"
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "builtin_timer.h"

#define RX_BUF_SIZE 1200
#define TX_BUF_SIZE 1200

enum { RX_CPMPLATE = 1, RX_DOING, RX_IDLE, TX_DOING, TX_IDLE };

enum {
    v_1022 = 1,
    v_2002,
};

typedef struct {
    // uart_rx
    uint8_t rx_state;
    uint16_t rx_size;
    uint16_t rx_offset;
    uint8_t rx_buf[RX_BUF_SIZE];

    // uart tx
    uint8_t tx_state;
    uint16_t tx_size;
    uint16_t tx_offset;
    uint8_t tx_buf[TX_BUF_SIZE];

} uart_date_t;

extern void iot_uart_init(void);
extern uint8_t *ServiceRecvBytes(uint16_t *msgLen);
extern uint8_t ServiceSendBytes(uint8_t *data, uint16_t len);
extern void iot_uart_deinit(void);
extern void Check_Com_Version(void);
extern uint8_t ServiceSendBytes_Uart_buf(uint8_t *data, uint16_t len);
void Get_ModbusVer(const uint8_t *cmd_buf);
void FillOld_IotVer(uint8_t *cmd_buf, uint16_t cmd_len);
void FillNew_IotVer(uint16_t Rec_add, uint16_t Num_add);
void OTA_start(void);
void Pass_Setting(void);
void Pass_Query(uint16_t Num_add, uint8_t slave_addr);
void Pass_Reseting(void);
void Set_Screen_pairing_mode(uint8_t mode);
void Screen_param_rw(void);
void change_to_modbus_data(uint8_t *buff_out, uint8_t *buff_in, uint16_t *out_len, uint16_t len);
void debug_test(uint8_t *data);
bool is_fcty_calibrated(void);
void uart_timer_cb(void *param);

#endif
