#ifndef __IOT_4G_HANDLE_H__
#define __IOT_4G_HANDLE_H__

#include <stdint.h>


#define AT_SEND_BUF_SIZE        1200                // AT发送缓存大小
#define AT_RECV_BUF_SIZE        1200                // AT接收缓存大小
#define AT_WAIT_TIME            500                 // 等待应答超时时间
#define AT_RETRY_TIMES          3                   // AT指令超时重发次数

extern uint8_t Cellular4G_mqtt_step;
enum 
{
    RESET_4G_MODULE = 0,
    INIT_4G_MODULE,
    GET_IP_4G,
    RESOLVE_DNS_4G,
    TCP_CONNECT_4G,
    TCP_SYNC_4G,
    MQTT_START_4G,
    MQTT_RECV_4G,//已完成 订阅主题流程
    MQTT_STBY_4G,
};


void create_iot_4g_task(void);
void set_mqtt_new_host(const char could_dns[128]);
void set_mqtt_new_iot(const char new_type[12], uint64_t new_sn, uint64_t safetycode);
void set_mqtt_new_device(const char new_type[12], uint64_t new_sn);
void stop_4g_task(void);
void start_4g_task(void);
void Cellular_init(void);
void iot_4g_task(void *pvParameters);
void mqtt_client_4G_Module_task(void * pvParameters);
void Cellular_4G_State_Print(void); 


#endif
