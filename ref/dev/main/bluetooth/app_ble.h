/**
  ******************************************************************************
  * @file      app_ble.h
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/1
  * @brief     蓝牙应用层头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/1   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#ifndef __APP_BLE_H__
#define __APP_BLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/queue.h"


typedef struct {
    uint16_t max_timeout;
    QueueHandle_t msg_queue;//解密后BLE rx 队列
}iot_ble_recv_struct;

typedef struct{
    uint8_t *data;
    uint16_t len;
    uint8_t status;
}ble_authenticate_buf_t;


// 属性表属性编号
typedef enum {
    IOT_BLE_SERVER = 0,
//FF01：ESP32 TX
	BLE_FF01_CHAR,
    BLE_FF01_CHAR_VAL,
    BLE_FF01_CHAR_DESC,
    BLE_FF01_CHAR_CFG,
//FF02：ESP32 RX
    BLE_FF02_CHAR,
    BLE_FF02_CHAR_VAL,
    BLE_FF02_CHAR_DESC,
//FF03：ESP32 TX,20240515新增主动上报BLE TX
	BLE_FF03_CHAR,
	BLE_FF03_CHAR_VAL,
	BLE_FF03_CHAR_DESC,
	BLE_FF03_CHAR_CFG,

    BLE_MAX_HANDLE,
}user_attr_table_index;


typedef struct {
    uint8_t *msg_ptr;
    uint16_t msg_len;
}msg_struct;

typedef struct {
	uint8_t *data;
	int data_len;
} ble_ff03_tx_struct;


enum{
	BLE_REFRESH_ENCRYPT = 0,
	BLE_ENCRYPT_PACK_RETRANSMITSSION,//1
	BLE_AUTHENTICATE_REQUEST,//发送 01包;2
	BLE_AUTHENTICATE_RESPOND,//3
	BLE_ECDH_REQUEST,//4
	BLE_ECDH_RESPOND,//5
	BLE_AES_CBC_ENCRYPT,//6
	BLE_ENCRYPT_COMPLATE,//7，加密握手阶段完成
};

 typedef union {
	 uint8_t all;
	 struct {
		 uint8_t is_enable : 1;
		 uint8_t init_done : 1; 	 // 蓝牙初始化完成
		 uint8_t is_adv_on : 2; 	 // 蓝牙广播状态
		 
		 uint8_t ble_connect : 1;	// 蓝牙连接状态 is_connected
		 uint8_t is_encrypted_fresh : 1;  //绑定配对状态
		 uint8_t is_encrypted : 1;	//数据加密状态
		 uint8_t authenticate_state : 1;  //鉴权状态
		 
 // 	 uint8_t is_retransmit : 1; //是否重传
	 }bit;
 }ble_status_t;
 

 /**
  * @brief 定义蓝牙数据信息结构
  * 
  */
 typedef struct {
	 ble_status_t  flag;
 
	 uint8_t authenticate_state;  // 
 
 } ble_encrypt_t;

 extern ble_encrypt_t ble_encrypt_info;


void ble_authenticate_task(void );
void ble_authenticate_task_step(void );
void ble_encript_disconnect_reset(void);
void ble_encript_connected(void);

void ble_encript_part(uint8_t notify);
void iot_ble_task(void *param);
void iot_ble_response(const uint8_t *rsp_data, uint16_t rsp_len,uint8_t TxChannel);
void iot_ble_new_name(const char *dev_type, uint64_t dev_sn);
const ble_encrypt_t *iot_get_ble_info(void);
void iot_ble_stop(void);
void iot_ble_start(void);
void ble_adv_status_check(void);

void iot_ble_rename(void);

 void ble_ff03_tx_queue_init(void);
 void ble_ff03_tx_queue_push(uint8_t *pdata, uint16_t len);
 uint8_t ble_ff03_report_task(void);
uint16_t Modbus_Bind_Ask_Ble(void);


#ifdef __cplusplus
}
#endif

#endif

