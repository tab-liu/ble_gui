#ifndef __SERVER_2_INTERNET_H__
#define __SERVER_2_INTERNET_H__

#include <stdint.h>
#include "app_uart.h"
#include "iot_mqtt.h"
#include "mqtt_client.h"




extern EXT_RAM_BSS_ATTR  login_info_t login2_info;
extern EXT_RAM_BSS_ATTR uint8_t  login2_step;//WIFI通道的MQTT登陆状态机
extern esp_mqtt_client_config_t mqtt2_cfg;//windy change to global

int16_t Server2_MQTT_Send_Public_Data(void);
void server2_config_info_update(void); 
void iot_mqtt2_start(uint8_t mode);
void iot_mqtt2_delete(uint8_t mode);
void set_mqtt_client2_netif(uint8_t netif_type);
void set_mqtt2_stop();

#endif
