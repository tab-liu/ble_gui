/**
  ******************************************************************************
  * @file      rs485_transmit.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/3/26
  * @brief     RS485通信驱动
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/3/26  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"
#include <driver/gpio.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "comm_define.h"

/* ================================ 头文件宏定义 ================================ */

#ifndef CONFIG_RS485_TX_GPIO_PIN
#define CONFIG_RS485_TX_GPIO_PIN    GPIO_NUM_4
#endif

#ifndef CONFIG_RS485_RX_GPIO_PIN
#define CONFIG_RS485_RX_GPIO_PIN    GPIO_NUM_5
#endif

#ifndef CONFIG_RS485_DE_GPIO_PIN
#define CONFIG_RS485_DE_GPIO_PIN    GPIO_NUM_1
#endif

#ifndef CONFIG_RS485_BAUD
#define CONFIG_RS485_BAUD           115200
#endif

#define RS485_UART_NUM              UART_NUM_2
#define RS485_TXD                   CONFIG_RS485_TX_GPIO_PIN 
#define RS485_RXD                   CONFIG_RS485_RX_GPIO_PIN
#define RS485_RX_BUF_SIZE           1536
#define RS485_TX_BUF_SIZE           1536

/*
128: 硬件分配的 FIFO 存储空间
120: 驱动默认的"安全阈值" — 当 FIFO 收到 120 字节时，触发中断将数据拷贝到软件 RX buffer
*/
#define RS485_HW_FIFO_SIZE			120
#define RS485_RX_TIMEOUT	        10  // ms(默认容许报文分断间隔)

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)


typedef struct {
	uint8_t rxMessSplicFlag;
    uint8_t txSendingFlag;
	uint16_t rxBytesNum;//
	uint16_t TxBytesNum;
	uint8_t Rxbuffer[RS485_RX_BUF_SIZE];
	uint8_t Txbuffer[RS485_TX_BUF_SIZE];
}RS485_TRANSMIT_STRUCT;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

// RS485初始化
esp_err_t rs485_init(void);

// RS485非阻塞写入
esp_err_t rs485_send_nonblocking(const uint8_t *data, size_t len);

// RS485阻塞写入
esp_err_t rs485_send_blocking(const uint8_t *data, size_t len);

// rs485接收事件处理
void rs485_evt_porcess(void);

#ifdef __cplusplus
}
#endif

