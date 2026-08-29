
#ifndef IOT_MQTT_H
#define IOT_MQTT_H

#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "mqtt_client.h"
#include "freertos/timers.h"
#include "esp_transport.h"
#include "comm_define.h"
#include <net/if.h>


#define     PUBLISH_TOPIC                          	"PUB/%s/%013llu"// MQTT 发布主题
#define     SUBSCRIBE_TOPIC                        	"SUB/%s/%013llu"// MQTT 订阅主题
#define     ALL_PUBLIC_TOPIC                       	"PUBLIC/%s/POWER"

#define	CHANNEL_INTERNET_WIFI			1 //
#define	CHANNEL_INTERNET_SPI_ETH		2 //
#define	CHANNEL_INTERNET_4G_AT_MODULE	3 //

#pragma pack(1)

typedef struct {
    uint8_t *msg_ptr;
    uint16_t msg_len;
}queue_msg_t;


typedef union {
    uint8_t byte[4];
    uint32_t uword;
}host_time_t;

struct esp_mqtt_client_dump {
    esp_transport_list_handle_t transport_list;
    esp_transport_handle_t transport;
};

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
    uint8_t current_netif_id;
    uint8_t mqtt_connect_fail;
    bool is_need_down_cert;
}login_info_t;
extern    login_info_t login_info;//static

typedef struct {
    uint8_t  sreport_step;    /* 上报阶段：0-主机，1-电池包 */
    uint64_t send_ready_cnt;  /* 上报周期计数器 */
    uint16_t cycle;           /* 上报周期编号 */
    uint16_t sRunflag;        /* 上报运行标志 */
    uint16_t len;             /* 本次上报数据长度 */
    uint8_t  index;           /* 当前设备索引 */
} mqtt_report_ctx_t;

typedef struct {
    uint8_t  type;
    size_t   index;
    uint16_t send_seq;
    uint16_t md_protocol_ver;
    uint16_t total_period_count;
    uint16_t continue_addr;
    uint16_t remain_regnum;
} periodic_pub_ctx_t;

enum {
    PUB_PERIOD = 1,
    PUB_EVEVT = 2,
    PUB_PHONE = 3,
    PUB_Multi_DEV = 4,
} ;

enum {
    STEP_MQTT_RESTART = 0,
    STEP_HOST_DNS = 1,
    STEP_CREATE_TCP = 2,
    STEP_GET_HOST_TIME = 3,
    STEP_MQTT_LOGIN = 4,
    STEP_MQTT_MSG_HANDLE = 5,
    STEP_MQTT_EXIT = 6,
    STEP_MQTT_CertCheck = 7,
    STEP_MQTT_CerExpiration = 8,//A904503
    STEP_MQTT_CerDownload = 9,
    STEP_MQTT_CONNECT_FAIL = 10,    
    STEP_MQTT_WAITING = 11,    
} ;

enum {
    MQTT_EXTEND_CHL_BLE = 1,
    MQTT_EXTEND_CHL_WIFI = 2,
    MQTT_EXTEND_CHL_ETHERNET = 9,
    MQTT_EXTEND_CHL_PPP_4G = 40,
    MQTT_EXTEND_CHL_PPP_5G = 50,
};

typedef struct {
	char  dev_type[12];   		// ASCII 机型
	uint64_t dev_sn;      		// 设备唯一识别码
	uint8_t index;				// 索引
} inv_info_t;


extern esp_mqtt_client_config_t mqtt_cfg;

#pragma pack()

uint8_t iot_mqtt_login_step_get(void);

uint8_t iot_mqtt_get_connect(void);
const char * iot_mqtt_get_host(void);
uint8_t iot_mqtt_login_state(void);
void iot_mqtt_new_host(const char *new_host);
void iot_mqtt_new_dev(const char *dev_type, uint64_t dev_sn);
void iot_mqtt_new_iot(const char *iot_type, uint64_t iot_sn, uint64_t safety_code);
void iot_mqtt_delete(void);
void iot_mqtt_start(void);
void iot_mqtt_wait(uint32_t wait_time);

uint8_t iot_get_cloud_dns(const char *url, char *host, uint16_t *port, uint8_t ipaddr[]); //static
uint8_t mqtt_client_UTC_IP_GET(login_info_t *mqtt_info);
void iot_mqtt_new_host2(const char *new_host) ;
void iot_mqtt_msg_handle(esp_mqtt_client_handle_t client, QueueHandle_t msg_queue, const char *rsp_topic);
void set_mqtt_stop(void); 
int iot_mqtt_send(const uint8_t *data, int len);

void Modbus_Self_21000_Report_MQTT(void); 
uint16_t iot_mqtt_announce_collect(void);
uint16_t iot_mqtt_announce_single(void);
void iot_mqtt_actively_report_task (void);
int mqtt_task_init(void);
void iot_mqtt_State_Machine(void);
bool iot_net_status_check(void);
void set_mqtt_heartbeat(uint16_t heartbeat);
uint8_t iot_curr_netif_id_get(void);
bool iot_netif_is_connected(uint8_t current_netif_id);

uint32_t CreateEncryptPassword(char *name, uint64_t code, uint32_t nowTime);
void Mqtt_Active_Report_Queue_Push(uint8_t *pdata, uint16_t len, uint8_t scene, uint16_t cycle, uint8_t total, uint8_t seq);
void iot_mqtt_period_check_and_report(void);

#endif
