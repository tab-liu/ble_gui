
#ifndef IOT_MQTT_H
#define IOT_MQTT_H

#include <stdio.h>
#include <net/if.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "mqtt_client.h"
#include "freertos/timers.h"
#include "esp_transport.h"
#include "esp_http_client.h"


#define     PUBLISH_TOPIC                          	"PUB/%s/%llu"// MQTT 发布主题
#define     SUBSCRIBE_TOPIC                        	"SUB/%s/%llu"// MQTT 订阅主题
#define     ALL_PUBLIC_TOPIC                       	"PUBLIC/%s/POWER"
//#define 	PUBLIC_TOPIC            "PUBLIC/IOT/POWER"

#define     NETIF_KEY_ETH       "ETH_DEF"
#define     NETIF_KEY_WIFI_STA  "WIFI_STA_DEF"
#define     NETIF_KEY_WIFI_AP   "WIFI_AP_DEF"



#define     NETIF_KEY_PPP       "PPP_DEF"
#define     MQTT_RECONNECT_COUNT (6)

#define	CHANNEL_INTERNET_WIFI			1 //
#define	CHANNEL_INTERNET_SPI_ETH		2 //
#define	CHANNEL_INTERNET_4G_AT_MODULE	3 //




typedef struct {
    uint8_t *msg_ptr;
    uint16_t msg_len;
}queue_msg_t;





#if 0
typedef struct 
{
    uint32_t StartTime;
    uint32_t EndTime;
    _URGENCY Urgency;//紧急度
    _SEVERITY Severity;//严重度
    _CERTAINTY Certainty;//确信度
    uint16_t Event;//警报事件
    uint16_t SoC;//Severity对应目标电量
}_SaveWebStormWatchApiData;//API data
#endif

typedef union {
    uint8_t byte[4];
    uint32_t uword;
}host_time_t;

struct esp_mqtt_client_dump {
    esp_transport_list_handle_t transport_list;
    esp_transport_handle_t transport;
};

//typedef enum _netif_type
//{
//    NETIF_TYPE_MIN = 0,
//    NETIF_TYPE_ETH = NETIF_TYPE_MIN,
//    NETIF_TYPE_WIFI_STA,
//    NETIF_TYPE_PPP,
//    NETIF_TYPE_MAX = NETIF_TYPE_PPP
//}netif_type_e;
typedef enum _netif_type
{
	NETIF_TYPE_MIN = 0,
	NETIF_TYPE_WIFI_STA = NETIF_TYPE_MIN,
	NETIF_TYPE_WIFI_AP,	
	NETIF_TYPE_MAX,
	NETIF_TYPE_ETH,	//no
	NETIF_TYPE_PPP //no
	
}netif_type_e;

typedef struct {
    char raw_url[128];//服务器网址+端口

    int sock;
    char host[120];//从raw_url解析的服务器网址
    uint8_t ipaddr[4];//从raw_url解析的服务器IP char
    uint16_t port;//从raw_url解析的服务器端口

    char iot_type[12];
    uint64_t iot_sn;
    uint64_t safetyCode;

    char dev_type[12];
    uint64_t dev_sn;

    uint64_t now_time;
    uint64_t password;
	uint64_t password2;

    char sub_topic[128];//订阅接收,recv_topic
    char pub_topic[128];//数据发送主题,send_topic
    QueueHandle_t  mqtt_msg_queue;
    esp_mqtt_client_handle_t  mqtt_client;
    TimerHandle_t push_timer;
    uint8_t mqtt_is_login;
    uint8_t mqtt_is_start;

    struct ifreq netif_req;
    uint8_t mqtt_login_fail_cnt;
    uint8_t current_netif_id;
    uint8_t mqtt_connect_fail;
}login_info_t;
extern EXT_RAM_BSS_ATTR  login_info_t login_info;//static


extern esp_mqtt_client_config_t mqtt_cfg;
extern int wifi_sta_config_flag;

void iot_mqtt_client_task(void * pvParameters);
uint8_t iot_mqtt_login_step_get(void);

uint8_t iot_mqtt_get_connect(void);
const char * iot_mqtt_get_host(void);
uint8_t iot_mqtt_login_state(void);
void iot_mqtt_new_host(const char *new_host);
void iot_mqtt_new_dev(const char *dev_type, uint64_t dev_sn);
void iot_mqtt_new_iot(const char *iot_type, uint64_t iot_sn, uint64_t safety_code);
void iot_mqtt_delete(uint8_t mode);
void iot_mqtt2_delete(uint8_t mode); 
void iot_mqtt_start(uint8_t mode);
void tcp_bind_dev(void);
void mqtt_bind_dev(void);
uint8_t iot_get_cloud_dns(const char *url, char *host, uint16_t *port, uint8_t ipaddr[]); //static
uint8_t iot_tcp_connect_host(uint8_t netif_id, const char ipaddr[4], uint16_t port, int *sock_fd ); //static
void log_error_if_nonzero(const char *message, int error_code); //static
void iot_mqtt_new_host2(const char *new_host) ;
void iot_mqtt_msg_handle(esp_mqtt_client_handle_t client, QueueHandle_t msg_queue, const char *rsp_topic, QueueHandle_t can_cmd_queue);
uint8_t check_integrity(int16_t download_type, char *buffer, int buffer_len);
int16_t cjson_to_serial_info(char *text);
void inv_node_data_process(uint8_t node);
void set_mqtt_stop(void); 
void Net_Connect_status(void) ;
esp_err_t iot_mqtt_send(uint8_t *data, int len);
uint8_t Modbus_Self_21000_Report_MQTT(void) ;
void wifi_sta_config_check(uint8_t *md_msg);
void set_mqtt_client_netif(uint8_t netif_type);
 void iot_mqtt_actively_report_task (void);
bool iot_mqtt_is_period_reporting(void);
void iot_bind_clean(void);
int set_http_header(esp_http_client_handle_t client);
void iot_mqtt_ChageFlagSet(uint8_t flag);
uint8_t iot_mqtt_ChageFlagGet(void);
void iot_mqtt_update(void);
 // 先通过HTTP获取UTC时区和IP地址，如果失败则通过DNS解析获取IP地址
 uint8_t mqtt_client_UTC_IP_GET(login_info_t *mqtt_info);
 static void iot_last_host_set(const char *host, const uint16_t *port, const uint8_t ipaddr[]);
 static uint8_t iot_last_host_get(char *host, uint16_t *port, uint8_t ipaddr[]);

void Mqtt_Active_Report_Queue_Push(uint8_t *pdata, uint16_t len, uint8_t scene);
#endif
