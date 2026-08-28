/**
  ******************************************************************************
  * @file      uart_ota.h
  * @version   1.0
  * @author    lixingyu
  * @date      2024/5/8
  * @brief     uart 升级
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/5/8   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#ifndef __UART_OTA_H__
#define __UART_OTA_H__
#include <stdint.h>
//#include "drv_mcu.h"
#include "xmodem_client.h"

/* RF modbus地址 */
#define UART_OTA_MD_ADDR				1

/* RF升级异常码定义 */
#define ERR_UPGRADE_NORMAL			0 		// 升级正常
#define ERR_FLASH_READ_FIALED		4 		// OTA数据读取失败
#define ERR_DEV_NO_RSP				10 		// LCD设备无响应
#define ERR_OUT_OF_MEMORY			11 		// 内存不足
#define ERR_DEV_ABORT				12 		// 设备终止传输
#define ERR_FILE_INVALID			13 		// 升级文件无效
#define RF_OTA_STA_IDLE			    0
#define RF_OTA_STA_DOING			1
#define RF_OTA_STA_CPLT			    2

extern ota_state_t uart_ota_state;

int uart_ota_start(ota_set_t *ota_set, char *bin_path, uint8_t *bin_data, uint8_t bin_flash_flag);
int uart_ota_is_doing(void);
int backup_is_doing(void);
void uart_ota_recv(const uint8_t *data, int len);
ota_state_t* uart_ota_state_get(void);
void uart_ota_state_clear(void);
uint8_t cc1312_NS_ota_state_init(void);
void cc1312_NS_ota_state_update(void);
uint8_t cc1312_NS_ota_num_get(void);

#endif
