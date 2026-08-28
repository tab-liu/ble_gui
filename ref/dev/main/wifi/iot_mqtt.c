#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "netdb.h"
#include "esp_log.h"

#include "iot_mqtt.h"
#include "cloud_diag.h"
#include "iot_wifi_init.h"
#include "password.h"
#include "modbus_slave.h"
#include "modbus_master.h"
#include "can_data.h"
#include "xmodem_client.h"
#include "crc.h"
#include "data_summary.h"
//#include "energy_data.h"
//#include "cc1312_ota.h"
#include "ll_param.h"
//#include "ParllelEMS.h"
//#include "EMSControlBox.h"
#include "cJSON.h"
#include "modbus_data.h"

//windy add debug
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "filesystem.h"
#include "iot_period_task.h"
#include "Cellular_4g_at.h"
#include "Cellular_4g_handle.h"

#include "server2internet.h"
#include "uart_device_process.h"
#include "iot_rsa.h"//A904503
#include "string.h"
#include "stdio.h"
#include "esp_http_client.h"
#include "esp_random.h"//Test
#include "iot_partition.h"
#include "mbedtls/md.h"
#include "mbedtls/platform_util.h"//Test
#include <mbedtls/sha256.h>
#include "do_di_ctrl.h"
#include "http_client.h"
#include "udt_transfer.h"
#include "can_protocol.h"
#include "dev_discovery.h"

#include "mesh_api.h"
#include "udt_port.h"
#include "DisasterWarn.h"
#include "can_pack.h"


#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 1024 //1024
#define download_buffer_len 2048
static char serial_data[33];//留出空字符的位置
char *download_buffer;
uint8_t serial_flag = 0;//请求新的证书序列号成功标志位

static QueueHandle_t xQueue_Mqtt_Active_Report = NULL;

#define MALLOC_CAP_SPIRA_SIZE      (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
// static uint16_t cycle_id = 0;// 周期性上报数据的完整数据包的Counter ID
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
} ;





typedef struct {
	char  dev_type[12];   		// ASCII 机型
	uint64_t dev_sn;      		// 设备唯一识别码
	uint8_t index;				// 索引
} inv_info_t;

typedef struct {
	uint8_t need_bind;//1-CAN设备变化，需要绑定；0-和服务器TCP绑定交互完成
	uint8_t dev_nums;
	inv_info_t invs_list[DEV_MAIN_NODE_MAX*INV_MAX_NUM];
    bool  tcp_bind_flag;
} bind_dev_list_t;

typedef struct {
    uint8_t *data;
    int data_len;
} Mqtt_Active_Report_Queue_Struct;


#define     TAG       								"[MQTTC]"

#define     IOT_PUBLISH_EVENT                       "\"ac_sw=%d,dc_sw=%d,pv_sw=%d,grid_sw=%d,soc=%d\""
#define 	ESP_TRANS_TIMEOUT						10000	// ESP传输层(TCP)收发超时时间定义
#define 	NO_STD_PROTOCOL_HEADER					4 		// 非标协议头
#define 	MQTT_PROTOCOL_HEADER_V12				10 		// 非标协议头
#define 	MQTT_TX_BUF_SIZE						2048
#define     PARTITION_SIZE                          4096




// uint8_t iot_get_cloud_dns(const char *url, char *host, uint16_t *port, uint8_t ipaddr[]);//static
// uint8_t iot_tcp_connect_host(const char ipaddr[4], uint16_t port, int *socket);//static
static uint32_t iot_sync_host_time(int sock, const char *iot_type, uint64_t iot_sn);
static uint64_t iot_login_password(uint32_t now_time, const char *iot_type, uint64_t iot_sn, uint64_t safetyCode);
static uint8_t iot_mqtt_login(login_info_t *login);

static void pack_data_head(uint8_t ver, uint8_t *head, uint8_t cause, uint16_t cycel, uint8_t total, uint8_t seq);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static int iot_mqtt_dev_event_ann(void);

 void iot_mqtt_announce(void);//(void * pvParameters); static
static uint32_t http_iot_update_cert(int sock_fd, const char *iot_type, login_info_t login_info) ;//A904503

static uint32_t http_get_cert_sn(login_info_t login_info);
int16_t json_get_down_cert_info(char *text);
static uint32_t http_iot_cert_exist(login_info_t login_info);

static uint32_t http_iot_download_cert(int sock_fd, const char *iot_type, login_info_t login_info) ;//A904503
static void iot_switch_netif(void);
static uint8_t iot_netif_is_connected(void);

EXT_RAM_BSS_ATTR static uint16_t old_dev_soc;
EXT_RAM_BSS_ATTR static uint8_t  login_step;//WIFI通道的MQTT登陆状态机
EXT_RAM_BSS_ATTR login_info_t login_info;//static
EXT_RAM_BSS_ATTR static uint8_t mqtt_tx_buf[MQTT_TX_BUF_SIZE];

#include "esp_transport.h"

EXT_RAM_BSS_ATTR static struct esp_mqtt_client_dump mqtt_client_dump;
//EXT_RAM_BSS_ATTR static  upload_data_t upload_data;
EXT_RAM_BSS_ATTR static bind_dev_list_t bind_dev_list;
static TimerHandle_t timeout_tmr;

static void mqtt_step_goto(uint8_t step, const char *reason)
{
    cloud_diag_mqtt_step_change(login_step, step, reason);
    login_step = step;
    reals.debug3 = login_step;
}

void iot_mqtt_cloud_diag_state(uint8_t *step, uint8_t *is_login, uint8_t *connect_fail)
{
    if (step) {
        *step = login_step;
    }
    if (is_login) {
        *is_login = login_info.mqtt_is_login;
    }
    if (connect_fail) {
        *connect_fail = login_info.mqtt_connect_fail;
    }
}

static bool iot_mqtt_start_should_skip(uint8_t mode)
{
    if (NETIF_TYPE_WIFI_STA != mode || mode != login_info.current_netif_id) {
        return false;
    }
    if (!wifi_info.flag.bit.sta_link_ap) {
        return false;
    }
    if (!login_info.mqtt_is_login || !login_info.mqtt_is_start || login_info.mqtt_client == NULL) {
        return false;
    }
    if (login_step != STEP_MQTT_MSG_HANDLE) {
        return false;
    }
    return true;
}


esp_mqtt_client_config_t mqtt_cfg;//windy change to global



int16_t TOP_MQTT_Send_Public_Data(char *send_topic, uint8_t *data_ptr, uint16_t data_len)
{
	int16_t ret=0;

//login_info.mqtt_client, login_info.pub_topic
	ret =esp_mqtt_client_publish(login_info.mqtt_client, send_topic, (const char *)data_ptr, data_len, 0, 0);

     ESP_LOGE(TAG, "*********MQTT data send done*********\n");
    //  ESP_LOG_BUFFER_HEX_LEVEL(TAG, data_ptr, data_len, ESP_LOG_WARN);
	ESP_LOGI(TAG,"RET_:%d ,send_topic:%s",ret,send_topic);
	return ret;	
}


static void timeout_timer_cb(TimerHandle_t xTimer)
{
    cloud_diag_mqtt_sub_timeout(login_step);
	login_info.mqtt_is_login = 0;
	mqtt_step_goto(STEP_MQTT_RESTART, "subscribe_ack_timeout_10s");
}

static void timeout_timer_start(void)
{
	/* MQTT订阅响应超时时间定义 */
	#define MQTT_SUB_RESP_TIMEOUT			10000

	if (timeout_tmr)
	{
		xTimerStart(timeout_tmr, 0);
		return;
	}

	/* 创建通信超时定时器 */
	timeout_tmr = xTimerCreate("mqtt sub timer", pdMS_TO_TICKS(MQTT_SUB_RESP_TIMEOUT), pdFALSE, NULL, timeout_timer_cb);
	if (timeout_tmr == NULL)
	{
		ESP_LOGE(TAG, "mqtt subscribe timeout timer create failed, Restarting");
		esp_restart();
		return;
	}
	xTimerStart(timeout_tmr, 0);
}

static void timeout_timer_stop(void)
{
	if (timeout_tmr)
		xTimerStop(timeout_tmr, 0);
}

 void log_error_if_nonzero(const char *message, int error_code) //static
 {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void Mqtt_Active_Report_Queue_Init(void);
/*------------------------------------------------------------------------
*@Function： iot_mqtt_client_task
WiFi网络连后MQTT登录逻辑

-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void iot_mqtt_client_task(void * pvParameters) 
{

    QueueHandle_t can_cmd_queue = (QueueHandle_t)pvParameters;

    if (login_info.mqtt_msg_queue == NULL) 
	{
        login_info.mqtt_msg_queue = xQueueCreate(10, sizeof(queue_msg_t));
        if (login_info.mqtt_msg_queue == NULL) 
		{
            ESP_LOGE(TAG, "mqtt recv message queue create failed");
            return ;
        }
    }

	/* 任务优先级设置的高于mqtt库内部的任务优先级(5) */
//    if (xTaskCreatePinnedToCore(iot_mqtt_announce, "mqtt-annce", 1024 * 5, NULL, 6, NULL, 1) != pdPASS) 
//	{
//        ESP_LOGE(TAG, "iot_mqtt_announce create failed");
//        return; /* 任务创建失败 */
//    }

	int16_t ret =0;
    login_step = STEP_MQTT_EXIT;
    reals.wifi_connect_ap = 0;
	login_info.sock = -1;
    login_info.mqtt_login_fail_cnt = 0;
    login_info.mqtt_client = NULL;
    int is_need_down_cert = 0;


	//windy add debug
//    wifi_connect_ap = 1;
//    login_step = STEP_MQTT_RESTART;
    /*上报队列初始化*/
    Mqtt_Active_Report_Queue_Init();

    vTaskDelay(pdMS_TO_TICKS(500));
    // use default netif:Eth
	login_info.current_netif_id =NETIF_TYPE_WIFI_STA;//只有WIFI，不切换
	
    set_mqtt_client_netif(login_info.current_netif_id);

	ESP_LOGI(TAG, "iot mqtt client task start");

    while(1) 
	{
		


        /**
         * @brief WiFi网络连后MQTT登录逻辑
         * 1、解析主机域名
         * 2、和主机建立tcp连接 (通过连接获取主机当前的时间)
         * 3、发送指令读取主机时间
         * 4、计算登录密码，发起登录
         * 5、处理MQTT交互信息
         */
        vTaskDelay(pdMS_TO_TICKS(20));
        
        //ESP_LOGI(TAG, "HAUNGJI SetData.dev_info_t.on_off.bit.wifi_enable:%d, reals.wifi_connect_ap:%d,reals.wifi_mesh_root:%d,login_step:%d", SetData.dev_info_t.on_off.bit.wifi_enable, reals.wifi_connect_ap,reals.wifi_mesh_root,login_step); 

         if ((1 != SetData.dev_info_t.on_off.bit.wifi_enable)
		 	||(0 == reals.wifi_connect_ap)
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
		 	||(0 == reals.wifi_mesh_root)
#endif
		 	)
		// 	||((GET_IP_4G_READY != reals.Cellular4G_step)&&(1 == SetData.dev_info_t.on_off.bit._4g_enable))
		// 	||((ETH_GET_IP_READY != reals.Ethernet_SPI_step)&&(1 == SetData.dev_info_t.on_off.bit.Eth_enable)) 
		// )
		 {
             login_step = STEP_MQTT_EXIT;
         }

//		ESP_LOGE(TAG, "windy in iot_mqtt_client_task!!!!!!!!!!");
			reals.debug3  =login_step;
        switch (login_step) 
		{
            /**
             * @brief 停止MQTT客服端
             */
            case STEP_MQTT_RESTART:
            {
                cloud_diag_mqtt_ctrl("RESTART_BEGIN", "state_machine", login_info.mqtt_is_login,
                                     login_step, login_info.mqtt_connect_fail);
                vTaskDelay(pdMS_TO_TICKS(100));
				memset(&mqtt_client_dump, 0x00, sizeof(struct esp_mqtt_client_dump));
                login_info.mqtt_is_login = 0;
                login_info.mqtt_is_start = 0;
                if (login_info.mqtt_client != NULL)
                {
                    esp_mqtt_client_stop(login_info.mqtt_client);
                    esp_mqtt_client_destroy(login_info.mqtt_client);
                    login_info.mqtt_client = NULL;
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
                ESP_LOGI(TAG, "mqtt client go to DNS");
				vTaskDelay(pdMS_TO_TICKS(30000));//减少因服务器重连失败带来的RAM压力
                mqtt_step_goto(STEP_HOST_DNS, "restart_destroy_done");
            }
            break;
#if HTTP_UTC_TIME_VERSION == 2
             case STEP_HOST_DNS:  /* 域名解析 */
                vTaskDelay(pdMS_TO_TICKS(20));
                if (mqtt_client_UTC_IP_GET(&login_info) == 1) {
                    if(login_info.now_time != 0){
#ifdef TCP_ENCRYPT_ENABLE
                        login_step = STEP_MQTT_CertCheck;
#else
                        login_step = STEP_MQTT_LOGIN;
#endif
                        ESP_LOGI(TAG, "http_utc_ip_get Success, now_time: %lld, mqtt client go to STEP_MQTT_LOGIN", login_info.now_time);
                    } else {
                        login_step = STEP_CREATE_TCP;
                        ESP_LOGI(TAG, "mqtt client go to TCP connect");
                    }
                } else {
                    vTaskDelay(pdMS_TO_TICKS(5000)); // 域名解析失败5s后在尝试
                    if (((0 == g_self_data.mod_reg11000_IOT_info.link.bit.wifi_sta)&&(1 == SetData.dev_info_t.on_off.bit.wifi_enable)) 
                        ||(1 != SetData.dev_info_t.on_off.bit.wifi_enable))
                    {
                        login_step = STEP_MQTT_EXIT;
                    }
                }
            break;
#else
            case STEP_HOST_DNS:  /* 域名解析 */
            {
                vTaskDelay(pdMS_TO_TICKS(20));
                // if (iot_get_cloud_dns(login_info.raw_url, login_info.host, &login_info.port, login_info.ipaddr) == 1) 
				// {
                //     login_step = STEP_CREATE_TCP; /* 下一步 */
				// 	ESP_LOGI(TAG, "mqtt client go to TCP connect");
                // }
                if (mqtt_client_UTC_IP_GET(&login_info) == 1) 
                {
				 	 if(login_info.now_time != 0)
                     {
#ifdef TCP_ENCRYPT_ENABLE
                        login_step = STEP_MQTT_CertCheck;
#else
                        login_step = STEP_MQTT_LOGIN;
#endif
                        ESP_LOGI(TAG, "http_utc_ip_get Success, mqtt client go to %d", login_step);
                    }
                    else {
                        login_step = STEP_CREATE_TCP;
                        ESP_LOGI(TAG, "mqtt client go to TCP connect");
                    }
                }
                else 
				{
                    vTaskDelay(pdMS_TO_TICKS(5000)); // 域名解析失败5s后在尝试
                    login_step = STEP_MQTT_CONNECT_FAIL;
                }
            }
            break;
#endif


            case STEP_CREATE_TCP: /* 建立tcp连接 */
            {
                vTaskDelay(pdMS_TO_TICKS(20));
                if (iot_tcp_connect_host(login_info.current_netif_id, (const char *)login_info.ipaddr, login_info.port, &login_info.sock) == 1) 
				{
                    login_step = STEP_GET_HOST_TIME; /* 下一步 */
					ESP_LOGI(TAG, "mqtt client go to get host time");
				
                } 
				else 
				{
					ESP_LOGW(TAG, "WW  STEP_CREATE_TCP else");
                    vTaskDelay(pdMS_TO_TICKS(5000));
					/* 网络在线回到上一步否则退出 */
                    (iot_netif_is_connected()) ? (login_step = STEP_HOST_DNS) : (login_step = STEP_MQTT_CONNECT_FAIL);
                }
            }
            break;

            case STEP_GET_HOST_TIME: /* 获取服务器时间(同步) */
            {
                vTaskDelay(pdMS_TO_TICKS(20));
                ESP_LOGI(TAG,"login_info sock:%d  iot_type:%s  iot_sn:%llu",login_info.sock, login_info.iot_type,login_info.iot_sn);
                ESP_LOGI(TAG,"dev_type:%s  dev_sn:%llu", login_info.dev_type,login_info.dev_sn);
                login_info.now_time = iot_sync_host_time(login_info.sock, login_info.iot_type, login_info.iot_sn);

#ifdef     TCP_ENCRYPT_ENABLE
                if(login_info.now_time <= 0)
                {
                    ESP_LOGI(TAG,"iot_tcp_connect_host:STEP_GET_HOST_TIME");//A904503
//                    login_step = STEP_CREATE_TCP; /* 重新创建tcp连接 */ 
                    vTaskDelay(pdMS_TO_TICKS(5000));
					/* 网络在线回到上一步否则退出 */
                    (iot_netif_is_connected()) ? (login_step = STEP_CREATE_TCP) : (login_step = STEP_MQTT_CONNECT_FAIL);	
                }								
                else
                {
                    ESP_LOGI(TAG,"iot_tcp_connect_host: STEP_GET_HOST_TIME success, now_time:= %lld, go to STEP_MQTT_CertCheck",login_info.now_time);

                    login_step = STEP_MQTT_CertCheck; /* 下一步 */ 
                }

#else
				 if(login_info.now_time <= 0)
                {
                    ESP_LOGI(TAG,"iot_tcp_connect_host:STEP_GET_HOST_TIME");//A904503
//                    login_step = STEP_CREATE_TCP; /* 重新创建tcp连接 */ 
                    vTaskDelay(pdMS_TO_TICKS(5000));
					/* 网络在线回到上一步否则退出 */
                    (iot_netif_is_connected()) ? (login_step = STEP_CREATE_TCP) : (login_step = STEP_MQTT_CONNECT_FAIL);	
                }								
                else
                {
                    ESP_LOGI(TAG,"iot_tcp_connect_host: STEP_GET_HOST_TIME success, now_time:= %lld, go to STEP_MQTT_LOGIN",login_info.now_time);
                    //断开tcp连接，启动mqtt登录连接
                    login_step = STEP_MQTT_LOGIN; /* 下一步 */ 
                }

#endif
            }
            break;

            /* 计算MQTT登录密码,并且登录 */
            case STEP_MQTT_LOGIN:
            {
                /* 按照登录流程计算两个密码,MQTT登录用户名与密码如下格式:
                * username: EBOX132564873121(SN)
                * passwrod: password,IOT@password2 */
                vTaskDelay(pdMS_TO_TICKS(20));
                //ESP_LOGI(TAG,"STEP_MQTT_LOGIN iot_now_time:%llu,sn:%llu safetyCode:%llu",login_info.now_time,login_info.iot_sn,login_info.safetyCode);
                if(reals.online_ACHUB_num == 0)
                {
                    login_info.password = iot_login_password((uint32_t)login_info.now_time, IOT_TYPE_IOT, login_info.iot_sn, login_info.safetyCode);
                }
                else if(reals.online_ACHUB_num == 1)
                {
                    login_info.password = iot_login_password((uint32_t)login_info.now_time, IOT_TYPE_HA1, login_info.iot_sn, login_info.safetyCode);
                }
                // if(reals.online_ACHUB_num == 0)
                // {
                //     login_info.password = iot_login_password((uint32_t)login_info.now_time, IOT_TYPE_BLE_DEFAULT, login_info.iot_sn, login_info.safetyCode);
                // }
                // else if(reals.online_ACHUB_num == 1)
                // {
                //     login_info.password = iot_login_password((uint32_t)login_info.now_time, IOT_TYPE_HA1, login_info.iot_sn, login_info.safetyCode);
                // }
                // login_info.password = iot_login_password((uint32_t)login_info.now_time, IOT_TYPE_BLE_DEFAULT, login_info.iot_sn, login_info.safetyCode); 
                login_info.password2 = iot_login_password((uint32_t)login_info.now_time, "IOT", login_info.iot_sn, login_info.safetyCode);
                
//                ESP_LOGI(TAG, "windy time now_time=%llu ",login_info.now_time);
//                ESP_LOGI(TAG, "windy time now_time,(u32)=%lu ",(uint32_t)login_info.now_time);
//                ESP_LOGI(TAG, "windy  password=%llu ",login_info.password);
                vTaskDelay(pdMS_TO_TICKS(20));

                if (iot_mqtt_login(&login_info) == 1) 
				{
                    login_info.mqtt_is_start = 1;
                    login_step = STEP_MQTT_MSG_HANDLE;
					ESP_LOGI(TAG, "mqtt client login successfully and go to message handle");
                } 
				else 
				{
                    ESP_LOGE(TAG,"iot_tcp_connect_host:STEP_MQTT_LOGIN failed");//A904503
                    if (iot_netif_is_connected()) {
                        mqtt_step_goto(STEP_MQTT_RESTART, "mqtt_login_fail");
                    } else {
                        mqtt_step_goto(STEP_MQTT_CONNECT_FAIL, "mqtt_login_fail_offline");
                    }
                }
            }
            break;

            /* MQTT数据处理 */
            case STEP_MQTT_MSG_HANDLE:
            {
                if (login_info.mqtt_client == NULL) 
				{
                    ESP_LOGE(TAG,"iot_tcp_connect_host:login_info.mqtt_client == NULL");//A904503				
                    break;
                }
//				ESP_LOGI(TAG,"iot_tcp_connect_host:STEP_MQTT_MSG_HANDLE");//A904503
				/* 设备产生了核心设置变化,IOT主动发布事件信息给到后台 */
                //iot_mqtt_dev_event_ann();
                iot_mqtt_msg_handle(login_info.mqtt_client, login_info.mqtt_msg_queue, login_info.pub_topic, can_cmd_queue);
            }
            break;

            case STEP_MQTT_CONNECT_FAIL:
            {
                cloud_diag_mqtt_ctrl("CONNECT_FAIL", "state_machine", login_info.mqtt_is_login,
                                     login_step, login_info.mqtt_connect_fail);
                login_info.mqtt_connect_fail++;
                //网卡不在线，直接切换，否则重试达到次数后再切换
                if (MQTT_RECONNECT_COUNT <= login_info.mqtt_connect_fail || 0 == iot_netif_is_connected())
                {
                    iot_switch_netif();
                    ESP_LOGI(TAG, "STEP_MQTT_CONNECT_FAIL switch to:%u, set_mqtt_client_netif:%s",
                                        login_step, mqtt_cfg.network.if_name->ifr_name);
                }
                vTaskDelay(pdMS_TO_TICKS(5000));
                mqtt_step_goto(STEP_MQTT_RESTART, "connect_fail_retry");
            }
            break;

            case STEP_MQTT_EXIT:
            {
                memset(&mqtt_client_dump, 0x00, sizeof(struct esp_mqtt_client_dump));
                login_info.mqtt_is_login = 0;
                login_info.mqtt_is_start = 0;
                if (login_info.mqtt_client != NULL) 
				{
                    esp_mqtt_client_stop(login_info.mqtt_client);
                    esp_mqtt_client_destroy(login_info.mqtt_client);
                    login_info.mqtt_client = NULL;
                    ESP_LOGW(TAG, "mqtt client stop");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
            }
            break;

#ifdef     TCP_ENCRYPT_ENABLE
            case STEP_MQTT_CertCheck:
            {
                if (http_iot_cert_exist(login_info))
                {
                    // 已存在，跳转到证书过期检查
                    ESP_LOGW(TAG,"0 cert_utc_sec:%ld", cert_utc_sec);//testwx
                    //每次与服务器比较证书时间是否过期时，都要先刷新，防止MQTT异常退出重连时还使用旧证书的时间
                    cert_utc_sec = calculate_certificate_utc_sec(iot_cert_ptr);
                    ESP_LOGW(TAG,"1 cert_utc_sec:%ld",cert_utc_sec);//testwx

                    if (is_need_down_cert || login_info.now_time > cert_utc_sec) 
                    {
                        //证书过期
                        ESP_LOGW(TAG,"iot_tcp_connect_host: STEP_GET_HOST_TIME success, now_time:= %lld, go to STEP_MQTT_CerExpiration",login_info.now_time);//A904503
                        login_step = STEP_MQTT_CerExpiration; /* 下一步 */
                    }
                    else
                    {
                        ESP_LOGI(TAG,"iot_tcp_connect_host: STEP_GET_HOST_TIME success, now_time:= %lld, go to STEP_MQTT_LOGIN",login_info.now_time);
                        //断开tcp连接，启动mqtt登录连接
                        login_step = STEP_MQTT_LOGIN; /* 下一步 */ 
                    }
                }
                else
                {
                    // 首次下载，使用http获取证书的序列号
                    ret = http_get_cert_sn(login_info);
                    if (ret == 1)
                    {
                        login_step = STEP_MQTT_CerDownload;
                        ESP_LOGW(TAG,"mqtt cert not exist, now_time = %lld, go to STEP_MQTT_CerDownload",login_info.now_time);//A904503
                    }
                }
            }
            break;

			case STEP_MQTT_CerExpiration:
				vTaskDelay(pdMS_TO_TICKS(20));
                ESP_LOGW(TAG,"0 cert_utc_sec:%ld",cert_utc_sec);//testwx
                //每次与服务器比较证书时间是否过期时，都要先刷新，防止MQTT异常退出重连时还使用旧证书的时间
                cert_utc_sec = calculate_certificate_utc_sec(iot_cert_ptr);
                ESP_LOGW(TAG,"1 cert_utc_sec:%ld",cert_utc_sec);//testwx
				if (is_need_down_cert || login_info.now_time > cert_utc_sec)
				{
					ESP_LOGW(TAG,"STEP_MQTT_CerExpiration,now_time=%lld",login_info.now_time);//A904503
					ret = http_iot_update_cert(login_info);//M

					if(ret > 0)
					{
						//下载最新的设备证书
						ESP_LOGI(TAG,"STEP_MQTT_CerExpiration success, go to STEP_MQTT_CerDownload");
						login_step = STEP_MQTT_CerDownload; /* 下一步 */ 
					}
					else
					{
						//尝试重新更新最新的设备证书						 
						ESP_LOGE(TAG,"STEP_MQTT_CerExpiration failed, try to get again"); 
						login_step = STEP_MQTT_CerExpiration; /* 重新发起HTTP请求，获取证书 */ 
					}
				}
				else 
				{
					ESP_LOGI(TAG,"STEP_MQTT_CerExpiration normal, go to STEP_MQTT_LOGIN");//A904503
					login_step = STEP_MQTT_LOGIN;
					vTaskDelay(pdMS_TO_TICKS(5000));
				}
				break;

			case STEP_MQTT_CerDownload:
				vTaskDelay(pdMS_TO_TICKS(20));
				if (serial_flag > 0)//testwx
				{
					ESP_LOGI(TAG,"STEP_MQTT_CerDownload,serial_data=%s",serial_data);//A904503 testwx
					ret = http_iot_download_cert(login_info.iot_type, login_info);//M

					if(ret == 2)//两份证书都下载完毕
					{
						if(load_all_cer())//重载证书
						{
							ESP_LOGI(TAG,"Certificate reload success, next step is STEP_MQTT_LOGIN");
							login_step = STEP_MQTT_LOGIN;

                            is_need_down_cert = 0;
						}
						else
						{
							ESP_LOGE(TAG,"Failed to reload Certificate, try again");
							login_step = STEP_CREATE_TCP; /* 重新创建tcp连接 */ 
							vTaskDelay(pdMS_TO_TICKS(5000));
						}
					}
					else
					{
						ESP_LOGE(TAG,"STEP_MQTT_CerDownload failed, lost some of them, try again");
						login_step = STEP_CREATE_TCP;
						vTaskDelay(pdMS_TO_TICKS(5000));
					}					 
				} 
				else 
				{
					ESP_LOGE(TAG,"STEP_MQTT_CerDownload failed, try again");//A904503
					login_step = STEP_CREATE_TCP; /* 重新创建tcp连接 */ 
					vTaskDelay(pdMS_TO_TICKS(5000));
				}
				break;
#else
#endif				

            default:  break;
        }
    }
}

void set_mqtt_client_netif(uint8_t netif_type)
{
    char *netif_key[2] = { NETIF_KEY_WIFI_STA,NETIF_KEY_WIFI_AP};//{NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP};
#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[netif_type]),
                                    login_info.netif_req.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[netif_type])),
                                login_info.netif_req.ifr_name);
#endif
    mqtt_cfg.network.if_name = &(login_info.netif_req);
    ESP_LOGE(TAG, "set_mqtt_client_netif:%s", mqtt_cfg.network.if_name->ifr_name);
}

static void iot_switch_netif(void)
{
    // if (1 == iot_netif_is_connected())
//    {
//        login_info.current_netif_id++;
//        if (2 < login_info.current_netif_id)
//        {
//            login_info.current_netif_id = 0;
//        }
//    }

//	login_info.current_netif_id =NETIF_TYPE_WIFI_AP; //NETIF_TYPE_WIFI_STA;//只有WIFI，不切换
//    set_mqtt_client_netif(login_info.current_netif_id);
    mqtt_step_goto(STEP_MQTT_RESTART, "switch_netif");
}

static uint8_t iot_netif_is_connected(void)
{
    if (NETIF_TYPE_ETH == login_info.current_netif_id)
    {
        return ((ETH_GET_IP_READY == reals.Ethernet_SPI_step)?1:0);
    }
    else if (NETIF_TYPE_WIFI_STA == login_info.current_netif_id)
    {
        return ((1 == reals.wifi_connect_ap)?1:0);
    }
    else if (NETIF_TYPE_PPP == login_info.current_netif_id)
    {
        return ((GET_IP_4G_READY == reals.Cellular4G_step)?1:0);
    }

    return 0;
}

uint8_t iot_mqtt_login_step_get(void)
{
    return login_step;//外部文件调用
}

#if  HTTP_UTC_TIME_VERSION == 2
/*------------------------------------------------------------------------------
 Function: iot_get_cloud_dns
 -----------------------------------------------------------------------------*/
/**
  * @brief      DNS解析，从URL中解析出主机名、端口和IP地址
  * @param[in]  const char *url - 输入URL，格式：host:port
  * @param[out] char *host - 输出主机名缓冲区
  * @param[out] uint16_t *port - 输出端口号
  * @param[out] uint8_t ipaddr[4] - 输出IPv4地址数组
  * @return     uint8_t (1-成功, 0-失败)
  */
 uint8_t iot_get_cloud_dns(const char *url, char *host, uint16_t *port, uint8_t ipaddr[])
{
#if 0
    ip_addr_t target_addr;// ip4_addr_t ip_addr_t
    struct in_addr ip_addr4;
    const struct addrinfo hints = {
        .ai_family = AF_INET,           /* 指定返回地址的协议簇，AF_INET(IPv4)、AF_INET6(IPv6)、AF_UNSPEC(IPv4 and IPv6)*/
        .ai_socktype = SOCK_STREAM,     /* 设定返回地址的socket类型，流式套接字 */
    };

    if (!strlen(url) || !port || !host || !ipaddr) {
        ESP_LOGE(TAG, "host url is null can not analyze");
        return 0;
    }

    sscanf(url, "%255[^:]:%hu", host, port);
    ESP_LOGI(TAG,"raw url: %s: %d", host, *port);

    struct addrinfo *result = NULL;
    int SetTimeZone = getaddrinfo(host, NULL, &hints, &result); /* 解析服务器地址 */
    if(SetTimeZone != 0) {
        ESP_LOGE(TAG, "dns analyze failed err=%d SetTimeZone=%p", SetTimeZone, result);
        iot_wifi_connect_status_check();
        if (result) freeaddrinfo(result); 
        return 0;
    }

    ip_addr4 = ((struct sockaddr_in *) (result->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &ip_addr4);
    freeaddrinfo(result); // 解析完成释放开辟的资源
	ESP_LOGI(TAG,"got host ipv4: %s port: %u", inet_ntoa(target_addr.u_addr.ip4) , *port);
    return 1;
    
#else

    struct addrinfo *result = NULL;
    struct in_addr ip_addr4;
    char url_copy[256] = {0};

    // 参数验证
    if (!url || !host || !port || !ipaddr) {
        ESP_LOGE(TAG, "Invalid parameters: url=%p, host=%p, port=%p, ipaddr=%p", 
                 url, host, port, ipaddr);
        return 0;
    }

    if (strlen(url) == 0 || strlen(url) >= sizeof(url_copy)) {
        ESP_LOGE(TAG, "Invalid URL length: %zu", strlen(url));
        return 0;
    }

    // 初始化输出参数
    *port = 0;
    memset(ipaddr, 0, 4);

    // 复制URL进行解析（避免修改原始字符串）
    strncpy(url_copy, url, sizeof(url_copy) - 1);
    url_copy[sizeof(url_copy) - 1] = '\0';

    // 解析URL格式：host:port
    char *colon_pos = strchr(url_copy, ':');
    if (colon_pos) {
        *colon_pos = '\0';  // 分割字符串
        strcpy(host, url_copy);
        
        // 解析端口号
        char *endptr;
        long port_val = strtol(colon_pos + 1, &endptr, 10);
        if (*endptr != '\0' || port_val <= 0 || port_val > 65535) {
            ESP_LOGE(TAG, "Invalid port number: %s", colon_pos + 1);
            return 0;
        }
        *port = (uint16_t)port_val;
    } else {
        // 没有端口号，只有主机名
        strcpy(host, url_copy);
        ESP_LOGW(TAG, "No port specified in URL, port remains: %d", *port);
    }

    ESP_LOGI(TAG, "Parsing URL: '%s' -> host: '%s', port: %u", url, host, *port);

    // 检查是否为IP地址格式
    struct in_addr addr_buf;
    if (inet_aton(host, &addr_buf) == 1) {
        // 输入已经是IP地址
        uint32_t ip = ntohl(addr_buf.s_addr);
        ipaddr[0] = (ip >> 24) & 0xFF;
        ipaddr[1] = (ip >> 16) & 0xFF;
        ipaddr[2] = (ip >> 8) & 0xFF;
        ipaddr[3] = ip & 0xFF;
        
        ESP_LOGI(TAG, "Input is already IP address: %d.%d.%d.%d:%u", 
                 ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3], *port);
        return 1;
    }

    // DNS解析
    const struct addrinfo hints = {
        .ai_family = AF_INET,       // 只支持IPv4
        .ai_socktype = SOCK_STREAM, // TCP socket
    };

    ESP_LOGI(TAG, "Starting DNS resolution for: %s", host);

    int dns_result = getaddrinfo(host, NULL, &hints, &result);
    if (dns_result != 0) {
        ESP_LOGE(TAG, "DNS resolution failed for '%s': error %d", 
                 host, dns_result);
        
        // 检查网络连接状态
        iot_wifi_connect_status_check();
        
        if (result) {
            freeaddrinfo(result);
        }
        return 0;
    }

    if (!result || !result->ai_addr) {
        ESP_LOGE(TAG, "DNS resolution returned null result");
        if (result) {
            freeaddrinfo(result);
        }
        return 0;
    }

    // 提取IPv4地址
    if (result->ai_family != AF_INET) {
        ESP_LOGE(TAG, "Unexpected address family: %d", result->ai_family);
        freeaddrinfo(result);
        return 0;
    }

    ip_addr4 = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;

    // 转换为数组格式
    uint32_t ip = ntohl(ip_addr4.s_addr);
    ipaddr[0] = (ip >> 24) & 0xFF;
    ipaddr[1] = (ip >> 16) & 0xFF;
    ipaddr[2] = (ip >> 8) & 0xFF;
    ipaddr[3] = ip & 0xFF;

    ESP_LOGI(TAG, "DNS resolution successful: %s -> %d.%d.%d.%d:%u", 
             host, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3], *port);

    // 释放资源
    freeaddrinfo(result);

    return 1;

#endif
}
#else
 uint8_t iot_get_cloud_dns(const char *url, char *host, uint16_t *port, uint8_t ipaddr[]) //static
{

    ip_addr_t target_addr;// ip4_addr_t ip_addr_t
    struct in_addr ip_addr4;
    const struct addrinfo hints = {
        .ai_family = AF_INET,           /* 指定返回地址的协议簇，AF_INET(IPv4)、AF_INET6(IPv6)、AF_UNSPEC(IPv4 and IPv6)*/
        .ai_socktype = SOCK_STREAM,     /* 设定返回地址的socket类型，流式套接字 */
    };

    if (!strlen(url) || !port || !host || !ipaddr) {
        ESP_LOGE(TAG, "host url is null can not analyze");
        return 0;
    }

//    sscanf(url, "%[^:] %*[:] %hd",  host, port);
    sscanf(url, "%255[^:]:%hu", host, port);
    ESP_LOGI(TAG,"raw url: %s, %u", host, *port);

    struct addrinfo *result = NULL;
    int res = getaddrinfo(host, NULL, &hints, &result); /* 解析服务器地址 */
    if(res != 0) {
        // freeaddrinfo(result); // 解析DNS失败不用释放内存
        ESP_LOGE(TAG, "dns analyze failed err=%d res=%p", res, result);
        return 0;
    }

    ip_addr4 = ((struct sockaddr_in *) (result->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &ip_addr4);
    //inet_addr_to_ip4addr(((&target_addr.addr)), &ip_addr4);
    freeaddrinfo(result); // 解析完成释放开辟的资源
	ESP_LOGI(TAG,"got host ipv4: %s port: %u", inet_ntoa(target_addr.u_addr.ip4) , *port);

    return 1;
}
#endif
/*
windy copy from A100
通用 TCP创建
int sock:sock 句柄，重要区分
const char *broker_uri：目标网址


rtn:
0-fail
1-ok
*/
static uint64_t app_mqtt_client_tcp_connect(uint8_t netif_id, int *sock, const char *broker_uri, uint16_t port) 
{   
    struct addrinfo *address_info;                                               
    struct addrinfo hints = {0};
    char raw_uri[128] = {0};
	
	//int sockfd=-1;//err
	//sock_fd= &sockfd;

    hints.ai_family = AF_UNSPEC;         /* 指定返回地址的协议簇，AF_INET(IPv4)、AF_INET6(IPv6)、AF_UNSPEC(IPv4 and IPv6) */
    hints.ai_socktype = SOCK_STREAM;     /* 设定返回地址的socket类型，流式套接字 tcp*/     
    if (broker_uri == NULL  || strlen(broker_uri) < 5)
    {
        ESP_LOGE(TAG,"broker_uri == NULL || model == NULL || sncode == 0");
        return 0;
    }

    ESP_LOGD(TAG,"Raw broker: '%s'", broker_uri);
    memcpy(raw_uri, broker_uri, strlen(broker_uri) % 128);

//    uint16_t port = 18760;
    const char *uri = strtok((char *)raw_uri, ":"); 
    const char *port_str = strtok(NULL, ":");
    if (port_str) {
        port = atoi(port_str);
    } else {
        ESP_LOGE(TAG,"broker uri port invaild (not value)");
    }
    ESP_LOGD(TAG,"broker: '%s', port: %d", uri, port);
        
    int res = 0;
    res = getaddrinfo(uri, NULL, &hints, &address_info); /* 解析服务器地址 */  
    if(res != 0 || address_info == NULL) {
        ESP_LOGE(TAG, "couldn't get hostname for `%s` "
                      "getaddrinfo() returns %d, addrinfo=%p", broker_uri, res, address_info);
        if (address_info) freeaddrinfo(address_info);              
        return 0;
    }

    struct in_addr addr4;
    if (address_info->ai_family == AF_INET) {
        addr4 = ((struct sockaddr_in *) (address_info->ai_addr))->sin_addr;
        ESP_LOGD(TAG, "broker IPv4 address: %s", inet_ntoa(addr4.s_addr));
    } else {
        ESP_LOGE(TAG, "Unsupported protocol family %d", address_info->ai_family);
        freeaddrinfo(address_info);
        return 0;
    }
    
//    /* socket fd 0、1、2分别表示标准输入、标准输出、标准错误,不能被关闭 */
//    if ((*sock != 0xff) && (*sock >= 3)) {
//        // 先调用 shutdown 再调用 close
//        // shutdown 通常用于流套接字（如 TCP 套接字），而不是数据报套接字（如 UDP 套接字）
//        if (shutdown(*sock, SHUT_RDWR) == -1) {
//            ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
//        }
//        //Operation not supported on socket 通常意味着你尝试对不支持 shutdown 操作的套接字调用了 shutdown 函数
//        if ( errno != 95 )
//        {
//            if (close(*sock) == -1) {
//                ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
//            }
//        }
//    }

    *sock = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);//int
    if( *sock < 0 )
    {
        ESP_LOGE(TAG, "socket create failed, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
        freeaddrinfo(address_info);
        return 0;
    }
    ESP_LOGI(TAG, "app_mqtt_client_tcp_connect : sockfd is %d", *sock);
    freeaddrinfo(address_info);

    struct timeval opt_on = {
        .tv_sec = 5, 
        .tv_usec = 0,
    };

    // use netif name bind to device
    struct ifreq ifr;
    char *netif_key[2] = { NETIF_KEY_WIFI_STA,NETIF_KEY_WIFI_AP};//{NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP};
#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[netif_id]), ifr.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[netif_id])), ifr.ifr_name);
#endif
    int ret = setsockopt(*sock, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(struct ifreq));
    if (ret < 0) {
        ESP_LOGE(TAG, "\"%s\" Unable to bind socket to specified interface: errno:%d, mean:%s", ifr.ifr_name, errno, strerror(errno));
    }

    if (setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &opt_on, sizeof(opt_on)) < 0) // 设置socket非阻塞
    { 
        ESP_LOGE(TAG, "socket setsockopt set failed");
        if (shutdown(*sock, SHUT_RDWR) == -1) {
            ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
        }
        if (close(*sock) == -1) {
            ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
            if ( errno == 9 )
            {
                /*Bad file number*/
                *sock = -1;
            }
        }
        else
        {
            *sock = -1;
        }

        return 0;
    } 

    ESP_LOGI(TAG, "connect '%s : %d' >> IPv4: %s", uri, port, inet_ntoa(addr4.s_addr));
    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = addr4.s_addr;
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(port);
    if (connect(*sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4)) < 0)//windy 后续的send需要此句柄login_info.sock
    {
        ESP_LOGE(TAG, "[sock=%d] Failed to connect", *sock);
		ESP_LOGE(TAG, "Socket connect failed %d: %s", errno, strerror(errno));
        if (shutdown(*sock, SHUT_RDWR) == -1) {
            ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
        }
        if (close(*sock) == -1) {
            ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
            if ( errno == 9 )
            {
                /*Bad file number*/
                *sock = -1;
            }
        }
        else
        {
            *sock = -1;
        }

        return 0;
    }
    
    return 1;
}


/*
return:
1-ok
*/
 uint8_t iot_tcp_connect_host(uint8_t netif_id, const char ipaddr[4], uint16_t port, int *sock_fd ) //static
{
#if 1
	return app_mqtt_client_tcp_connect(netif_id, sock_fd, SetData.dev_info_t.could_dns, port);//windy add debug
#else
	static int sock = 0xff;
    struct sockaddr_in server_host;
	int err=0xFF;
    ip4_addr_t ip_addr;//ip_addr_t  ip4_addr_t

    IP4_ADDR(&ip_addr, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]); // 服务器IPV4地址转换

	memset(&server_host, 0, sizeof(server_host));

    server_host.sin_family = AF_INET;
    server_host.sin_port = PP_HTONS(port);  // 服务器端口;htons(port);
    server_host.sin_addr.s_addr = PP_HTONL(htonl(ip_addr.addr));  //ip_addr.addr 服务器IP;addr4.s_addr;


	/* socket fd 0、1、2分别表示标准输入、标准输出、标准错误,不能被关闭 */
	if ((sock != 0xff) && (sock >= 3))
	{
		close(*sock_fd);
		shutdown(*sock_fd, SHUT_RDWR);
	}

    /* 创建socket 用于TCP连接 */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return 0;
    }

    struct timeval opt_on = {
        .tv_sec = 5,
        .tv_usec = 0,
    };

    /* 设置socket非阻塞接收超时时间5s */
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &opt_on, sizeof(opt_on)) < 0) {
		ESP_LOGE(TAG, "setsockopt: errno %d: %s", errno, strerror(errno));
        close(sock);
        shutdown(sock, SHUT_RDWR);
        return 0;
    }

     /* 建立TCP连接 */
//    if (connect(sock, (struct sockaddr *)&server_host, sizeof(server_host)) != 0) {
//		ESP_LOGE(TAG, "Socket connect failed %d: %s", errno, strerror(errno));
//        close(sock);
//        shutdown(sock, SHUT_RDWR);
//        return 0;
//    }

	err =connect(sock, (struct sockaddr *)&server_host, sizeof(server_host));
	ESP_LOGE(TAG, "[TCP connect err=%d] ", err);
	ESP_LOGE(TAG, "Socket connect failed %d: %s", errno, strerror(errno));

    if (err < 0)
    {
        ESP_LOGE(TAG, "[sock=%d] Failed to connect", sock);
        close(sock);
	    shutdown(sock, SHUT_RDWR);
		
        return 0;
    }	

    if (sock_fd) {
        *sock_fd = sock;
        ESP_LOGI(TAG, "tcp connected to %s port: %d", login_info.host, port);
        return 1;
    }
    close(sock);
    shutdown(sock,SHUT_RDWR);
    return 0;
#endif	
}

static uint32_t iot_sync_host_time(int sock_fd, const char *iot_type, uint64_t iot_sn) {

    if (sock_fd < 0) {
        return 0;
    }

    if ((iot_type == NULL) || (iot_sn == 0)) {
        close(sock_fd);
        shutdown(sock_fd,SHUT_RDWR);
        ESP_LOGE(TAG, "iot_sync_host_time NULL iot_type:%s,iot_sn:%llu", iot_type,iot_sn);
        return 0;
    }

    int rx_len = 0;
    uint8_t rx_buffer[128];

    char dev_str[128] = {0};
    snprintf(dev_str, sizeof(dev_str), "%s%llu", iot_type, iot_sn);

    int tx_len = 4 + strlen(dev_str);
    uint8_t *tx_buffer = heap_caps_malloc(tx_len, MALLOC_CAP_SPIRAM); // 分配发送缓存 4字节头 + 设备信息字符串(类型+SN字符串)
    if (tx_buffer == NULL) {
        close(sock_fd);
        shutdown(sock_fd,SHUT_RDWR);
        ESP_LOGE(TAG, "tcp get time cmd memony (%dbytes) malloc failed", tx_len);
        return 0;
    }

    tx_buffer[0] = 0x00;
    tx_buffer[1] = 0x01;
    tx_buffer[2] = (strlen(dev_str) >> 8);  // 负载长度高8bit
    tx_buffer[3] = (strlen(dev_str));       // 负载长度低8bit
    memcpy(tx_buffer + 4, dev_str, strlen(dev_str));

    /* tcp发送数据 */
    if (send(sock_fd, tx_buffer, tx_len, 0) < 0) {
        close(sock_fd);
        shutdown(sock_fd,SHUT_RDWR);
        free(tx_buffer);
        ESP_LOGE(TAG, "tcp Error occurred during sending: errno %d: %s", errno, strerror(errno));
        return 0;
    } else {
        free(tx_buffer);
    }

    /* 等待接收tcp数据 - 5s超时 */
    uint32_t time = 0;
    rx_len = recv(sock_fd, rx_buffer, sizeof(rx_buffer),  0);

    if (rx_len < 0) {
        ESP_LOGE(TAG, "tcp recv data err: errno %d: %s", errno, strerror(errno)); // Error occurred during receiving
    } else if (rx_len == 0) {
        ESP_LOGE(TAG, "tcp disconnected %d: %s", errno, strerror(errno));
    } else {

        /* 接收到主机数据 */
        host_time_t  nowtime;
        nowtime.uword = 0;
        if (rx_buffer[0] == 0x00 && rx_buffer[1] == 0x01) {
            int len = (((uint16_t)rx_buffer[2]<<8) | rx_buffer[3]);
            if (len == 4) {
                memcpy(nowtime.byte, rx_buffer + 4, len);
                time = htonl(nowtime.uword);
                ESP_LOGI(TAG, "host time: %lu", time);
            }
        }
    }
    close(sock_fd);
    shutdown(sock_fd,SHUT_RDWR);
    return time; /* 返回获取到的主机时间 */
}

static uint64_t iot_login_password(uint32_t now_time, const char *iot_type, uint64_t iot_sn, uint64_t safetyCode) {
    char iot_string[64] = {0};
    snprintf(iot_string, sizeof(iot_string), "%s%llu", iot_type, iot_sn);
    uint64_t pwd = CreateEncryptPassword(iot_string, safetyCode, now_time); /* 计算MQTT登录密码 */
    return pwd;
}

static uint8_t iot_mqtt_login(login_info_t *login) 
{

    if (login == NULL) 
	{
        return 0;
    }

#ifdef TCP_ENCRYPT_ENABLE	
	char host[150] = "mqtts://";
#else
	char host[150] = "mqtt://";
#endif	
    char username[64] = {0};
    char password[100] = {0};

    strcat(host, login->host);
    snprintf(username, sizeof(username), "%s%llu", login->iot_type, login->iot_sn);
	snprintf(password, sizeof(password), "%08llu,IOT@%08llu", login->password, login->password2);
    //snprintf(password, sizeof(password), "%08llu,HA1@%08llu", login->password, login->password2);

    mqtt_cfg.broker.address.uri = host;//mqtt_uri,                  /*!< MQTT host */
    mqtt_cfg.broker.address.port = login->port;
#if !MQTT_DEBUG
    mqtt_cfg.credentials.username = username;                /*!< MQTT username */        
    mqtt_cfg.credentials.authentication.password = password; /*!< MQTT password */
#endif
    mqtt_cfg.credentials.client_id = username;//clientId,               /*!< MQTT clientId */
    mqtt_cfg.network.disable_auto_reconnect = false;         /*!< MQTT auto reconnect  */
    mqtt_cfg.session.keepalive = 50;//60,                         /*!< MQTT keep alive */
#ifdef TCP_ENCRYPT_ENABLE	
    mqtt_cfg.credentials.authentication.certificate = (const char *)iot_cert_ptr;  //客户端证书
    mqtt_cfg.credentials.authentication.key = (const char *)private_key_ptr;  //客户端密钥
    // mqtt_cfg.broker.verification.certificate = (const char *)ca_cert_ptr,  //服务器证书(服务器部署后再启用) 
#endif		

	// /* 自搭建的EMQX MQTT服务器测试 */
	// mqtt_cfg.uri = NULL;
	// mqtt_cfg.port = 1883;
	// mqtt_cfg.host = "120.78.80.117";
	// ESP_LOGI(TAG, "host ip addr:  %s",  mqtt_cfg.host);

    ESP_LOGI(TAG, "********mqtt login info*********");
	if(mqtt_cfg.broker.address.uri) {
		ESP_LOGI(TAG, "hosturl:  %s",  mqtt_cfg.broker.address.uri);
	}
    ESP_LOGI(TAG, "hostport: %ld",  mqtt_cfg.broker.address.port);
    ESP_LOGI(TAG, "ClientId: %s",  mqtt_cfg.credentials.client_id);
    ESP_LOGI(TAG, "username: %s",  mqtt_cfg.credentials.username);
    ESP_LOGI(TAG, "password: %s",  mqtt_cfg.credentials.authentication.password);

    if (login->mqtt_client == NULL) {
        login->mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        if (login->mqtt_client != NULL) {
            /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
            ESP_ERROR_CHECK(esp_mqtt_client_register_event(login->mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, login));
            ESP_ERROR_CHECK(esp_mqtt_client_start(login->mqtt_client));
            ESP_LOGI(TAG, "init mqtt client and start");
            return 1;
        }
    } else {
        ESP_ERROR_CHECK(esp_mqtt_set_config(login->mqtt_client, &mqtt_cfg));
        ESP_ERROR_CHECK(esp_mqtt_client_start(login->mqtt_client));
		ESP_LOGI(TAG, "mqtt client start");
        return 1;
    }

    ESP_LOGI(TAG, "mqtt client start failed");
    return 0;
}

/*------------------------------------------------------------------------------
 Function: iot_bind_dev_send
 -----------------------------------------------------------------------------*/
/**
  * @brief      设备绑定
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_bind_dev_send(void)
{
    if ( bind_dev_list.tcp_bind_flag == true ) {
        return mqtt_bind_dev();
    } else {
        tcp_bind_dev();
    }
}

/*------------------------------------------------------------------------
*@Function： iot_bind_clean
清除bind_dev_list数据，用于MQTT重新登录时，重新上报绑定帧
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void iot_bind_clean(void)
{
	ESP_LOGI(TAG, "---------iot_bind_clean---------");
    memset(&bind_dev_list,0,sizeof(bind_dev_list));
	bind_dev_list.invs_list[0].index=0xff;
    bind_dev_list.invs_list[1].index=0xff;
    bind_dev_list.invs_list[2].index=0xff;
    printf("\n");
}

/*------------------------------------------------------------------------
*@Function： iot_bind_dump
仅打印，无功能
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
static void iot_bind_dump(void)
{
	ESP_LOGI(TAG, "---------device bind dump---------");
	bind_dev_list_t *bind = &bind_dev_list;
	for (int i = 0; i <bind->dev_nums; i++) 
	{
		ESP_LOGI(TAG, "bind type: %s, sn: %lld, index: %d",
			bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn, bind->invs_list[i].index);
	}
	printf("\n");
}

/*------------------------------------------------------------------------
*@Function： iot_bind_update
检测逆变器设备，加入绑定列表
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
static void iot_bind_update(char *type, uint64_t dev_sn, uint8_t idx)
{
	bind_dev_list_t *bind = &bind_dev_list;
    //ESP_LOGI(TAG,"bind->dev_nums:%d,type:%s,dev_sn:%llu",bind->dev_nums,type,dev_sn);
    if(bind->dev_nums)
    {
        for(int i = 0; i < bind->dev_nums; i++)
        {
            /* 索引不同继续轮询下一个绑定的设备 */
            if (bind->invs_list[i].index != idx)
            {
                continue;
            }

            /* 索引相同设备类型与sn一致则绑定关系不需要改变直接退出 
            strcmp，字符串比较，返回0表示相同

            */
            if ((strcmp(type, bind->invs_list[i].dev_type) == 0) && (dev_sn == bind->invs_list[i].dev_sn)) 
            {
                return;
            }

            /* 索引相同但设备类型或sn不一致则需要重新绑定 */
            ESP_LOGW(TAG, "binding device sn changed and rebind, old serial number: %s%lld, new serial number: %s%lld",
                bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn, type, dev_sn);
            strcpy(bind->invs_list[i].dev_type, type);
            bind->invs_list[i].dev_sn = dev_sn;
            bind->invs_list[i].index = idx;
            bind->need_bind = 1;
            iot_bind_dump();
            return;
        }
    }

	/* 将新的设备加入到绑定列表中 */
	strcpy(bind->invs_list[bind->dev_nums].dev_type, type);
	bind->invs_list[bind->dev_nums].dev_sn = dev_sn;
	bind->invs_list[bind->dev_nums].index = idx;
	bind->dev_nums++;
	bind->need_bind = 1;
	ESP_LOGW(TAG, "binding new device, serial number: %s%lld", type, dev_sn);
	iot_bind_dump();
}

static void iot_bind_check(void)
{
    bind_dev_list_t *bind = &bind_dev_list;
    static uint8_t bind_change_count=0;
	uint8_t index = 0;
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			if(Inv_can[node].inv_data[i].online)
			{
                index++;
                //ESP_LOGI(TAG,"iot_bind_check node:%d i:%d index:%d",node,i,index);
				inv_about_struct *inv_about = &Inv_can[node].inv_data[i].inv_about;

				/* 设备类型与SN有效检测是否需要绑定 */
				if ((inv_about->dev_sn != 0) && (inv_about->dev_sn != UINT64_MAX) && (strlen(inv_about->dev_type) != 0)) {
					iot_bind_update(inv_about->dev_type, inv_about->dev_sn, node);
				}
			}
		}
	}
    //实际在线设备数量和已记录的设备数量不一致则重新记录了
    //ESP_LOGI(TAG,"bind_change_count:%d",bind_change_count);
    if(index!=bind->dev_nums)
    {
        bind_change_count++;
        //判断间隔30s
        if(bind_change_count>30)
        {
            bind_change_count=0;
            ESP_LOGI(TAG,"need bind index:%d,bind->dev_nums:%d",index,bind->dev_nums);
            index=0;
            iot_bind_clean();
            for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
            {
                for(int i = 0; i < INV_MAX_NUM; i++)
                {
                    if(Inv_can[node].inv_data[i].online)
                    {
                        index++;
                        inv_about_struct *inv_about = &Inv_can[node].inv_data[i].inv_about;

                        /* 设备类型与SN有效检测是否需要绑定 */
                        if ((inv_about->dev_sn != 0) && (inv_about->dev_sn != UINT64_MAX) && (strlen(inv_about->dev_type) != 0)) {
                            iot_bind_update(inv_about->dev_type, inv_about->dev_sn, node);
                        }
                    }
                }
            }
        }
    }else{
        bind_change_count=0;
    }
}

      
/*------------------------------------------------------------------------
*@Function： tcp_bind_dev
发送TCP 报文：CAN设备绑定信息
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void tcp_bind_dev(void)
//static void tcp_bind_dev(void)
{
	int len;
	int total_len;
	int bind_len;
	bind_dev_list_t *bind = &bind_dev_list;							// 获取绑定信息

	//ESP_LOGE(TAG, "windy tcp_bind_dev	1 data");//windy debug
    if( login_step == STEP_MQTT_RESTART) return;

    if(reals.Addr_can_master==0) return ;

	if (bind->need_bind == 0) return;
		ESP_LOGE(TAG, "windy tcp_bind_dev	2 data");//windy debug
	if (bind->dev_nums == 0) return;
		ESP_LOGE(TAG, "windy tcp_bind_dev	3 data");//windy debug

    if(strcmp(login_info.iot_type, IOT_TYPE_IOT) == 0) //一代IOT不做绑定帧上报
    {
        bind->need_bind=0;
        ESP_LOGE(TAG, " tcp_bind_dev Not Bind For  iot_type:%s",login_info.iot_type);//windy debug
        return ;
    }

    if(bind->dev_nums>1)
    {
        if(reals.online_ACHUB_num!=1)//未接入ACHUB时，不可有多个设备绑定关系上报
        {
            ESP_LOGE(TAG, " tcp_bind_dev Not Bind For More dev_num:%d",bind->dev_nums);//windy debug
            return;
        }else 
        {
            if(strcmp(login_info.iot_type, IOT_TYPE_HA1) != 0) //只有HA1可以绑定多个AP300设备做上报
            {
                ESP_LOGE(TAG, " tcp_bind_dev Not Bind For err HA1 TYPE:%s",login_info.iot_type);//windy debug
                return ;
            }
        }
    }
		
	if (mqtt_client_dump.transport == NULL) return;


	//ESP_LOGE(TAG, "windy tcp_bind_dev	4 data");//windy debug

	/* 申请绑定缓存 */
//	char *buff = (char *)reals.TxBuff_TCP_Temp;//(char *)malloc(4096);
//	char *Rxbuff = (char *)reals.RxBuff_TCP_Temp;
	
	char *buff = (char *)heap_caps_malloc(sizeof(uint8_t) * 4096, MALLOC_CAP_SPIRAM);
	if (buff == NULL)
	{
		ESP_LOGE(TAG, "malloc failed for mqtt bind device");
		return;
	}
	char *p_data = &buff[NO_STD_PROTOCOL_HEADER];					// 指针跳过协议头
	p_data[0] = '\0';												// 清除缓存

	/* 按照协议建立绑定关系
	 * 参考文档：http://doc.poweroak.ltd/docs/bluetti-cloud-analysis/bluetti-cloud-analysis-1dqah2v384ps0 */
	sprintf(p_data, "%s&%llu@%d", bind->invs_list[0].dev_type, bind->invs_list[0].dev_sn, bind->invs_list[0].index);
    // sprintf(p_data, "%s&%llu", bind->invs_list[0].dev_type, bind->invs_list[0].dev_sn);
    if(bind->dev_nums>1)
    {
        for (int i = 1; i < bind->dev_nums; i++) 
        {
            sprintf(&p_data[strlen(p_data)], ",%s&%llu@%d", bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn, bind->invs_list[i].index);
            ESP_LOGI(TAG,"tcp_bind_dev i:%d dev_type:%s ,dev_sn:%llu,index:%d",i, bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn, bind->invs_list[i].index);
            // sprintf(&p_data[strlen(p_data)], ",%s&%llu", bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn);
        }
    }
	ESP_LOGW(TAG, "tcp device bind to server: %s ",p_data);
	//ESP_LOGE(TAG, "windy tcp_bind_dev	5 data");//windy debug

	/* 设置绑定协议头 */
	bind_len = strlen(p_data);
	buff[0] = 0x00;
	buff[1] = 0x02;
	buff[2] = bind_len >> 8;
	buff[3] = bind_len;

	/* TCP发送绑定数据到服务器 */
	total_len = bind_len + NO_STD_PROTOCOL_HEADER;
	

	while (total_len > 0)
	{
		ESP_LOGI(TAG, "tcp send bind data to server");
		len = esp_transport_write(mqtt_client_dump.transport, (char *)buff, total_len, ESP_TRANS_TIMEOUT);
		if (len < 0)
		{
			ESP_LOGE(TAG, "esp_transport_write failed, errno=%d, %s", errno, strerror(errno));
			goto __exit;
		}
		else if (len == 0)
		{
			ESP_LOGE(TAG, "esp_transport_write timeout, errno=%d, %s", errno, strerror(errno));
			goto __exit;
		}
		total_len -= len;
	}
	
	/* 获取绑定响应数据 */
	ESP_LOGI(TAG, "tcp read bind reply from server");
	len = esp_transport_read(mqtt_client_dump.transport, (char *)buff, 8, ESP_TRANS_TIMEOUT);
	if (len < 0)
	{
		// ESP_LOGE(TAG, "esp_transport_read error, errno=%d, %s", errno, strerror(errno));
		ESP_LOGE(TAG, "esp_transport_read error, errno=%d", len );
		goto __exit;
	}
	else if (len == 0)
	{
		ESP_LOGE(TAG, "esp_transport_read nothing or EOF");
		goto __exit;
	}



	/* 解析绑定响应数据 */
	if (buff[0] == 0x00 && buff[1] == 0x02)
	{
		len = ((int)buff[2] << 8) | buff[3];
		if (len == 4)
		{
			int result = *(int*)&buff[4];
			if (result == 0)
			{
				ESP_LOGW(TAG, "tcp bind device success");
				bind->need_bind = 0;								// 绑定成功清除绑定标志
                bind->tcp_bind_flag = true;
				free(buff);											// 释放缓存
				return;
			}
		}
	}
	ESP_LOGE(TAG, "tcp bind device failed, server respond data:");
	ESP_LOG_BUFFER_HEX(TAG, buff, 8);

__exit:
	free(buff);														// 释放缓存
    ESP_LOGW(TAG, "reals.online_ACHUB_num==%d",reals.online_ACHUB_num);
    // if(reals.online_ACHUB_num == 0)// 单机
    // {
    //    // bind->need_bind = 0;// testwx
    // }
    
}

/*------------------------------------------------------------------------
*@Function： mqtt_bind_dev
发送TCP 报文：CAN设备绑定信息
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void mqtt_bind_dev(void)
{
	int len;
	int total_len;
	int bind_len;
	bind_dev_list_t *bind = &bind_dev_list;							// 获取绑定信息

	//ESP_LOGE(TAG, "???dev_nums: %d\n", bind->dev_nums);
	//ESP_LOGE(TAG, "windy tcp_bind_dev	1 data");//windy debug
    ESP_LOGI(TAG,"mqtt_bind_dev login_step:%d dev_num:%d  iot_type:%s,online_ACHUB_num:%d",login_step,bind->dev_nums,login_info.iot_type,reals.online_ACHUB_num);
    if(reals.Addr_can_self > reals.Addr_can_Small)
    { 
        ESP_LOGW(TAG,"can_self:%d >can_small:%d not bind report",reals.Addr_can_self , reals.Addr_can_Small);
        return;
    }
    
    if(reals.Addr_can_master==0)
    {
        ESP_LOGW(TAG,"master:%d ERR no report",reals.Addr_can_master);
        return ;
    }
    if( login_step == STEP_MQTT_RESTART) 
    {
        ESP_LOGW(TAG,"login_step:%d ERR no report",login_step);
        return;
    }

    if(strcmp(login_info.iot_type, IOT_TYPE_IOT) == 0) //一代IOT不做绑定帧上报
    {
        bind->need_bind=0;
        ESP_LOGE(TAG, " mqtt_bind_dev Not Bind For  iot_type:%s",login_info.iot_type);//windy debug
        return ;
    }

	if (bind->need_bind == 0) return;
		  //ESP_LOGE(TAG, "windy mqtt_bind_dev	2 data");//windy debug
	if (bind->dev_nums == 0) return;
		//ESP_LOGE(TAG, "windy mqtt_bind_dev	3 data");//windy debug
	if(bind->dev_nums>1)
    {
        if(reals.online_ACHUB_num!=1)//未接入ACHUB时，不可有多个设备绑定关系上报
        {
            ESP_LOGE(TAG, " mqtt_bind_dev Not Bind For More dev_num:%d",bind->dev_nums);//windy debug
            return;
        }else 
        {
            if(strcmp(login_info.iot_type, IOT_TYPE_HA1) != 0) //只有HA1可以绑定多个AP300设备做上报
            {
                ESP_LOGE(TAG, " mqtt_bind_dev Not Bind For err iot_tpr:%s",login_info.iot_type);//windy debug
                return ;
            }
        }
    }
	if (mqtt_client_dump.transport == NULL) return;


	// ESP_LOGE(TAG, "windy tcp_bind_dev	4 data");//windy debug

	/* 申请绑定缓存 */
//	char *buff = (char *)reals.TxBuff_TCP_Temp;//(char *)malloc(4096);
//	char *Rxbuff = (char *)reals.RxBuff_TCP_Temp;
	
	char *buff = (char *)heap_caps_malloc(sizeof(uint8_t) * 4096, MALLOC_CAP_SPIRAM);
	if (buff == NULL)
	{
		ESP_LOGE(TAG, "malloc failed for mqtt bind device");
		return;
	}
    memset(buff, 0, 12);
	/* 设置绑定协议头 */
    buff[0] = 0x0A;
    buff[1] = 0xF8;
    buff[2] = 0x02;
    buff[MQTT_PROTOCOL_HEADER_V12] = 0x01;
    buff[MQTT_PROTOCOL_HEADER_V12+1] = 0x49;
	char *p_data = &buff[MQTT_PROTOCOL_HEADER_V12 + 2];					// 指针跳过协议头 + 01 + 49
	p_data[0] = '\0';												// 清除缓存

	/* 按照协议建立绑定关系
	 * 参考文档：http://doc.poweroak.ltd/docs/bluetti-cloud-analysis/bluetti-cloud-analysis-1dqah2v384ps0 */
	sprintf(p_data, "%s&%llu", bind->invs_list[0].dev_type, bind->invs_list[0].dev_sn);
    if(bind->dev_nums>1)
    {
        for (int i = 1; i < bind->dev_nums; i++) 
        {
            sprintf(&p_data[strlen(p_data)], ",%s&%llu", bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn);
        }
    }
	ESP_LOGW(TAG, "mqtt device bind to server: %s ",p_data);

	bind_len = strlen(p_data);
    uint16_t crc = CalcCrc16_modbus((uint8_t *)&buff[12], bind_len);
	// ESP_LOGW(TAG, "crc: %02x\n", crc);
    buff[3] = (uint8_t)(bind_len >> 8) ;
    buff[4] = (uint8_t)bind_len ;
    buff[bind_len+12] = (uint8_t)crc ;
    buff[bind_len+13] = (uint8_t)(crc >> 8);
	//ESP_LOGE(TAG, "windy tcp_bind_dev	5 data");//windy debug

	/* TCP发送绑定数据到服务器 */
	total_len = bind_len + MQTT_PROTOCOL_HEADER_V12 + 4; //4个字节表示 2字节的01 49 + 2字节的crc

    esp_err_t err = iot_mqtt_send((uint8_t *)buff, total_len);
    if (err == ESP_OK) {
            ESP_LOGI(TAG, "mqtt bind device success");
            bind->need_bind = 0;								// 绑定成功清除绑定标志
            free(buff);											// 释放缓存
            return;
    } else {
        ESP_LOGE(TAG, "mqtt send bind failed: %s", esp_err_to_name(err));
    }
	
	free(buff);														// 释放缓存
													// 释放缓存
}

#if 0
static bool tcp_send_iot_param(bool mqtt_conn)
{
	#define 	IOT_PARAM_FUNC_CODE		0x03						// 功能码
	#define		IOT_GEN_VER				2							// 第二代IOT
	#define 	GEN_JSON_STR			"{\"model\":\""IOT_TYPE_STR"\",\"generation\":%d,\"iotCurrVer\":\"%d\"}"

	char buff[100];
	static bool iot_param_flag = false;								// iot参数标志,当参数发送到服务器后该标志为真
	if (mqtt_client_dump.transport == NULL) goto __exit;

	if (mqtt_conn) {												// 每次mqtt重连都需要发送iot参数
		iot_param_flag = false;
	}

	/* iot参数已发送到服务器直接返回 */
	if (iot_param_flag) goto __exit;

	/* 组装非标数据(功能码0X03) */
	snprintf(&buff[NO_STD_PROTOCOL_HEADER], sizeof(buff)-NO_STD_PROTOCOL_HEADER, GEN_JSON_STR, IOT_GEN_VER, IOT_VERSION);
	int len = strlen(&buff[NO_STD_PROTOCOL_HEADER]);
	buff[0] = 0x00;
	buff[1] = IOT_PARAM_FUNC_CODE;
	buff[2] = len >> 8;
	buff[3] = len;
	len += NO_STD_PROTOCOL_HEADER;
	ESP_LOGW(TAG, "tcp send iot json parameters, len: %d, value: %02x %02x %02x %02x %s",
		len, buff[0], buff[1], buff[2], buff[3], &buff[NO_STD_PROTOCOL_HEADER]);

	/* 通过TCP发送iot参数到服务器 */
	len = esp_transport_write(mqtt_client_dump.transport, (char *)buff, len, ESP_TRANS_TIMEOUT);
	if (len < 0)
	{
		ESP_LOGE(TAG, "esp_transport_write iot parameters failed, errno=%d, %s", errno, strerror(errno));
		goto __exit;
	}
	else if (len == 0)
	{
		ESP_LOGE(TAG, "esp_transport_write iot parameters timeout, errno=%d, %s", errno, strerror(errno));
		goto __exit;
	}

	/* 通过TCP从服务器读取iot参数响应 */
	ESP_LOGI(TAG, "tcp read iot parameters reply from server");
	len = esp_transport_read(mqtt_client_dump.transport, (char *)buff, 8, ESP_TRANS_TIMEOUT);
	if (len < 0)
	{
		ESP_LOGE(TAG, "esp_transport_read iot parameters reply error, errno=%d, %s", errno, strerror(errno));
		goto __exit;
	}
	else if (len == 0)
	{
		ESP_LOGE(TAG, "esp_transport_read iot parameters reply nothing or EOF");
		goto __exit;
	}

	/* 解析IOT参数响应数据 */
	if (buff[0] == 0x00 && buff[1] == IOT_PARAM_FUNC_CODE)
	{
		len = ((int)buff[2] << 8) | buff[3];
		if (len == 4)
		{
			int result = *(int*)&buff[4];
			if (result == 0)
			{
				ESP_LOGW(TAG, "tcp send iot parameters success");
				iot_param_flag = true;								// 发送成功置位标志
				goto __exit;
			}
		}
	}

	ESP_LOGE(TAG, "tcp send iot parameters failed");
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, 8, ESP_LOG_ERROR);
__exit:
	return iot_param_flag;
}
#endif

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    login_info_t *login_info = (login_info_t *)handler_args;
    int msg_id = 0;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
        {
            uint8_t login_before = login_info->mqtt_is_login;
			ESP_LOGI(TAG, "mqtt connected");
			memcpy(&mqtt_client_dump, client, sizeof(struct esp_mqtt_client_dump));
			iot_bind_check();										// 检查系统绑定信息
			bind_dev_list.need_bind = 1;							// 每次登录都要绑定一次信息
            bind_dev_list.tcp_bind_flag = false;
			iot_bind_dev_send();									// 通过tcp方式绑定设备
    		//tcp_bind_dev();											// 通过tcp方式绑定设备
            //mqtt_bind_dev();
			// tcp_send_iot_param(true);								// 通过tcp方式发送iot参数

            if (NETIF_TYPE_ETH == login_info->current_netif_id)
            {
                reals.Ethernet_SPI_step = ETH_GET_IP_READY;
                ESP_LOGW(TAG, "mqtt disconnect, Ethernet SPI step set to %d", reals.Ethernet_SPI_step);
            }

            snprintf(login_info->sub_topic, sizeof(login_info->sub_topic), SUBSCRIBE_TOPIC, login_info->dev_type, login_info->dev_sn);
            snprintf(login_info->pub_topic, sizeof(login_info->pub_topic), PUBLISH_TOPIC,   login_info->dev_type, login_info->dev_sn);
			char all_public_topic[100];
			snprintf(all_public_topic, sizeof(all_public_topic), ALL_PUBLIC_TOPIC,  login_info->dev_type);

            msg_id = esp_mqtt_client_subscribe(client, all_public_topic, 0);		// 订阅公共主题
            ESP_LOGI(TAG, "subscribe public topic, msg_id=%d topic: %s", msg_id, all_public_topic);

            msg_id = esp_mqtt_client_subscribe(client, login_info->sub_topic, 0);	// 订阅接收服务器命令主题
            ESP_LOGI(TAG, "subscribe receive topic, msg_id=%d topic: %s", msg_id, login_info->sub_topic);
			ESP_LOGI(TAG, "data publish topic: %s, and waiting for MQTT_EVENT_SUBSCRIBED", login_info->pub_topic); // 数据发送主题
			timeout_timer_start();

            login_info->mqtt_connect_fail = 0;

            reals.need_new_version_flag.sBit.iot = 1;
            if(0 != reals.online_Inv_num) reals.need_new_version_flag.sBit.inv = 1;
            if(0 != reals.online_Pack_num) reals.need_new_version_flag.sBit.pack = 1;
            if(0 != reals.online_DCHUB_num) reals.need_new_version_flag.sBit.dchub= 1;
		    if(0 != reals.online_ACHUB_num) reals.need_new_version_flag.sBit.achub= 1;
			if(0 != reals.online_D400S_num) reals.need_new_version_flag.sBit.d400s= 1;
            cloud_diag_mqtt_event(MQTT_EVENT_CONNECTED, event, login_before, login_step);
            break;
        }

        case MQTT_EVENT_DISCONNECTED: // MQTT链接断开事件
        {
            uint8_t login_before = login_info->mqtt_is_login;
			memset(&mqtt_client_dump, 0x00, sizeof(struct esp_mqtt_client_dump));
            login_info->mqtt_is_login = 0;
            mqtt_step_goto(STEP_MQTT_CONNECT_FAIL, "mqtt_event_disconnected");
            reals.last_disconnect_cloud_time = reals.now;
            if (NETIF_TYPE_ETH == login_info->current_netif_id)
            {
                reals.Ethernet_SPI_step = ETH_LOST_IP;
                ESP_LOGW(TAG, "Ethernet SPI step link: %d", reals.Ethernet_SPI_step);
            }

            cloud_diag_mqtt_event(MQTT_EVENT_DISCONNECTED, event, login_before, login_step);
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            
            /* socket fd 0、1、2分别表示标准输入、标准输出、标准错误,不能被关闭 */
            if ((login_info->sock != 0xff) && (login_info->sock >= 3)) {
                // 先调用 shutdown 再调用 close
                if (shutdown(login_info->sock, SHUT_RDWR) == -1) {
                    ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", login_info->sock, errno, strerror(errno));
                }
                if (close(login_info->sock) == -1) {
                    ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", login_info->sock, errno, strerror(errno));
                    if ( errno == 9 )
                    {
                        /*Bad file number*/
                        login_info->sock = -1;
                    }
                }
                else
                {
                    login_info->sock = -1;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
            break;
        }

        case MQTT_EVENT_SUBSCRIBED: /* 订阅完成 */
        {
            uint8_t login_before = login_info->mqtt_is_login;
			timeout_timer_stop();
            login_info->mqtt_is_login = 1;
            reals.last_login_cloud_time = reals.now;
            g_self_data.mod_reg11000_IOT_info.link.bit.mqtt = 1;//更新11018-MQTT状态
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            cloud_diag_mqtt_event(MQTT_EVENT_SUBSCRIBED, event, login_before, login_step);
            // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);

            // 连接到MQTT后，触发一次检查是否有新版本可升级
            update_firmware_check();

            break;
        }

        case MQTT_EVENT_UNSUBSCRIBED: /* 取消订阅 */
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            // ESP_LOGI(TAG, "topic: %s recv message", event->topic);
            reals.net_frame_rx_counter++;
            if ( login_info->mqtt_msg_queue == NULL) {
                ESP_LOGE(TAG, "mqtt msg queue is null");
                break;
            }

            if (!strncmp(event->topic, ALL_PUBLIC_TOPIC, strlen(ALL_PUBLIC_TOPIC))) 
			{
                // 公共主题-数据接收

            } 
			else if (!strncmp(event->topic, login_info->sub_topic, strlen(login_info->sub_topic))) 
            {
                // 设备私有主题-数据接收

                queue_msg_t mqtt_msg;
                mqtt_msg.msg_len = event->data_len; //
                mqtt_msg.msg_ptr = (uint8_t *)heap_caps_malloc(event->data_len, MALLOC_CAP_SPIRAM);
                if (mqtt_msg.msg_ptr == NULL) {
                    ESP_LOGI(TAG, "mqtt recv msg malloc (%dbytes) failed", mqtt_msg.msg_len);
                    break;
                }

                memcpy(mqtt_msg.msg_ptr, event->data, event->data_len);

                ESP_LOGE(TAG,"!!!!!!!!!!!!!!!!!!!!MQTT_EVENT_DATA rsp_len:%d",mqtt_msg.msg_len);
                // ESP_LOG_BUFFER_HEX_LEVEL(TAG, mqtt_msg.msg_ptr, mqtt_msg.msg_len, ESP_LOG_WARN);
                if (xQueueSendToBack((QueueHandle_t)login_info->mqtt_msg_queue, &mqtt_msg, pdMS_TO_TICKS(100)) != pdPASS) 
				{
                    free(mqtt_msg.msg_ptr);
                    ESP_LOGE(TAG, "mqtt message handle queue full"); // MQTT发送过来的消息太多,就会处理不及时队列满
                }
            } 
			else 
			{
                // 未知主题
                ESP_LOGE(TAG, "mqtt unkonw topic");
            }
            break;

        case MQTT_EVENT_ERROR:
        {
            uint8_t login_before = login_info->mqtt_is_login;
            reals.last_disconnect_cloud_time = reals.now;
            cloud_diag_mqtt_event(MQTT_EVENT_ERROR, event, login_before, login_step);
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            // MQTT 错误类型
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
            {
                /* 登录名或密码错误 */
                if (event->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME || \
                    event->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED)
                {

                    // ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
                }
            }
            
            /* socket fd 0、1、2分别表示标准输入、标准输出、标准错误,不能被关闭 */
            if ((login_info->sock != 0xff) && (login_info->sock >= 3)) {
                // 先调用 shutdown 再调用 close
                if (shutdown(login_info->sock, SHUT_RDWR) == -1) {
                    ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", login_info->sock, errno, strerror(errno));
                }
                if (close(login_info->sock) == -1) {
                    ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", login_info->sock, errno, strerror(errno));
                    if ( errno == 9 )
                    {
                        /*Bad file number*/
                        login_info->sock = -1;
                    }
                }
                else
                {
                    login_info->sock = -1;
                }
            }

            login_info->mqtt_is_login = 0;
            mqtt_step_goto(STEP_MQTT_RESTART, "mqtt_event_error");
            break;
        }

        default: ESP_LOGI(TAG, "Other event id:%d", event->event_id); break;
    }
}


/*------------------------------------------------------------------------
*@Function： pack_data_head
互联网载荷协议V1.2,帧头组合
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
static void pack_data_head(uint8_t ver, uint8_t *head, uint8_t cause, uint16_t cycel, uint8_t total, uint8_t seq) {
    head[0] = ver; // 0x01 modbus格式，0x02字符串格式
    head[1] = 0xF8;
    head[2] = cause;
    head[3] = cycel ;
    head[4] = (cycel >> 8) & 0xFF;
    head[5] = seq;
    head[6] = total;
    head[7] = g_self_data.mod_reg11000_IOT_info.sta_rssi;
    head[8] = 0;
    head[9] = 0;
}


int wifi_sta_config_flag;

void wifi_sta_config_check(uint8_t *md_msg)
{
	#define WIFI_STA_CFG_START_ADDR		12001		// wifi station配置开始地址
	#define WIFI_STA_CFG_END_ADDR		12033		// wifi station配置结束地址
	#define WIFI_STA64_CFG_END_ADDR		12049		// wifi station配置结束地址
	int code = md_msg[1];

	if (code == 0x06 || code == 0x10)
	{
		uint16_t reg_nums = 1;
		uint16_t start_addr = ((uint16_t)md_msg[2]<<8) | md_msg[3];

		if(code == 0x10) 
		{
			reg_nums = ((uint16_t)md_msg[4]<<8) | md_msg[5];
		}
		else if(code == 0x06) 
		{
			reg_nums = 1;
		}

		uint16_t end_addr = start_addr + reg_nums - 1;

		/* 检测是否是wifi station配置 */
		if (!((start_addr > WIFI_STA_CFG_END_ADDR) || (end_addr < WIFI_STA_CFG_START_ADDR))) {
			wifi_sta_config_flag = 1;
		}
	}
}


static int thunder_data_parse(cJSON *root,char *id)
{
	static _SaveWebStormWatchApiData thunder_param = {0};
	cJSON *var = NULL;

	var = cJSON_GetObjectItem(root, "start");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [start] error");
		goto __exit;
	}
	thunder_param.StartTime = (typeof(thunder_param.StartTime))var->valuedouble;

	var = cJSON_GetObjectItem(root, "end");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [end] error");
		goto __exit;
	}
	thunder_param.EndTime = (typeof(thunder_param.EndTime))var->valuedouble;

	var = cJSON_GetObjectItem(root, "urgency");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [urgency] error");
		goto __exit;
	}
	thunder_param.Urgency = (typeof(thunder_param.Urgency))var->valuedouble;

	var = cJSON_GetObjectItem(root, "severity");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [severity] error");
		goto __exit;
	}
	thunder_param.Severity = (typeof(thunder_param.Severity))var->valuedouble;

	var = cJSON_GetObjectItem(root, "certainty");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [certainty] error");
		goto __exit;
	}
	thunder_param.Certainty = (typeof(thunder_param.Certainty))var->valuedouble;

	var = cJSON_GetObjectItem(root, "event");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [event] error");
		goto __exit;
	}
	thunder_param.Event = (typeof(thunder_param.Event))var->valuedouble;

	var = cJSON_GetObjectItem(root, "soc");
	if ((var == NULL) || (var->type != cJSON_Number))
	{
		ESP_LOGW(TAG, "field [soc] error");
		goto __exit;
	}
	thunder_param.SoC = (typeof(thunder_param.SoC))var->valuedouble;

	ESP_LOGW(TAG, "set thunder param");
    //SetStormWebData(&thunder_param,TimeZoneOffset);		// 向EMS设置灾害告警参数
	//SetStormWebData(&thunder_param);		// 向EMS设置灾害告警参数
    SetStormWebData(&thunder_param,id);		// 向EMS设置灾害告警参数
	return 0;

__exit:
	return -1;
}

#ifdef THUNDER_DBG
const char *test_storm = "{\n\
    \"date\": \"2023-09-15 09:31\",\n\
    \"start\": 1606896000,\n\
    \"end\": 1606982400,\n\
    \"urgency\": 0,\n\
    \"severity\": 1,\n\
    \"certainty\": 2,\n\
    \"sender\": \"US Environmental Protection Agency\",\n\
    \"event\": 3,\n\
    \"headline\": \"Thunderstorm Alert for Natomas \",\n\
    \"recordId\": \"18a180ef4faee2d856c48a4d2a0\",\n\
    \"timezone\": \"Europe/Vilnius\",\n\
    \"utc_offset_seconds\": -18000,\n\
    \"soc\": 100\n\
}";
#endif

static int thunder_mode_handle(uint8_t *buff, int len)
{
	#define THUNDER_MODE_FUNC_CODE 	0x04

#ifdef THUNDER_DBG
	char test_buf[500];
	int test_len = strlen(test_storm);
	test_buf[0] = 0x00;
	test_buf[1] = 0x04;
	test_buf[2] = len >> 8;
	test_buf[3] = len;
	strcpy(&test_buf[4], test_storm);
	test_len += 4;
	buff = (uint8_t*)test_buf;
#endif

	uint8_t *pdata = buff;
	char msg_id[100] = {0};
	int ret = 0;

	if (pdata == NULL) {
		return -1;
	}

	/* 非风暴模式指令直接退出 */
	if (!((pdata[0] == 0x00) && (pdata[1] == THUNDER_MODE_FUNC_CODE))) return 1;

	/* 解析风暴指令json数据 */
	cJSON *root = cJSON_Parse((const char*)&pdata[NO_STD_PROTOCOL_HEADER]);
	if ((root == NULL) || (root->type != cJSON_Object))
	{
		ret = -1;
		ESP_LOGW(TAG, "cjson parse error");
		goto __exit;
	}

	ESP_LOGW(TAG, "thunder message: %02x %02x %02x %02x\n%s\n", pdata[0], pdata[1], pdata[2], pdata[3], &pdata[4]);
	cJSON *recordId = cJSON_GetObjectItem(root, "recordId");
	if ((recordId == NULL) || (recordId->type != cJSON_String) || (recordId->valuestring == NULL))
	{
		ret = -1;
		ESP_LOGW(TAG, "field [recordId] error");
		goto __exit;
	}

	strncpy(msg_id, recordId->valuestring, sizeof(msg_id) - 1);
	msg_id[sizeof(msg_id) - 1] = '\0';
	ret = thunder_data_parse(root,msg_id);

__exit:
	if (root != NULL)
	{
		// // StormWatchSetData.disaster_send_param.type = ret;
		// // disaster_send_param_t send_param;
        // // 初始化 disaster_send_param 变量
        // // memset(&send_param, 0, sizeof(disaster_send_param_t));

		// disaster_save_param_t *send_param = ll_iot_param_get("disaster_save_param");
		// if(ret == -1){
		// 	send_param->disaster_send_param.uuid++;
		// 	ESP_LOGW(TAG,"ret:%d",ret);
		// }
		//  ESP_LOGW(TAG,"StormSendParams.uuid:%d ret:%d",send_param->disaster_send_param.uuid,ret);

		// // UpdateDisasterSaveDataFromParam();
		// bool result = https_disater_state_to_host(send_param->disaster_send_param, ret, msg_id);
		// ret = !result;
		// // ret = !(https_disater_state_to_host(NULL,ret));

		
		cJSON_Delete(root);

		/* 响应风暴指令 */
		// char buffer[200];
		// root = cJSON_CreateObject();
		// snprintf(buffer, sizeof(buffer), "%s%llu", login_info.iot_type, login_info.iot_sn);
		// cJSON_AddStringToObject(root, "sn", buffer);
		// cJSON_AddStringToObject(root, "model", login_info.iot_type);
		// if (ret == 0) {
		// 	cJSON_AddStringToObject(root, "status", "success");
		// }
		// else {
		// 	cJSON_AddStringToObject(root, "status", "failure");
		// }
		// cJSON_AddStringToObject(root, "recordId", msg_id);
		// cJSON_AddStringToObject(root, "remark", "");

		// char *tmp = cJSON_Print(root);
		// assert(tmp != NULL);
		// char *reply = heap_caps_malloc(strlen(tmp)+10, MALLOC_CAP_SPIRAM);
		// assert(reply != NULL);
		// char *p_buffer = &reply[NO_STD_PROTOCOL_HEADER];
		// strcpy(p_buffer, tmp);
		// cJSON_Minify(p_buffer);
		// cJSON_free(tmp);

		// int length = strlen(p_buffer);
		// reply[0] = 0x00;
		// reply[1] = THUNDER_MODE_FUNC_CODE;
		// reply[2] = length >> 8;
		// reply[3] = length;
		// ESP_LOGW(TAG, "thunder cmd reply: %02x %02x %02x %02x\n%s\n", reply[0], reply[1], reply[2], reply[3], &reply[4]);
		// length = length + NO_STD_PROTOCOL_HEADER;
		// cJSON_Delete(root);

		// /* 通过TCP发送响应数据到服务器 */
		// // esp_transport_write(mqtt_client_dump.transport, (const char *)reply, length, ESP_TRANS_TIMEOUT);
		// free(reply);

		/* 发送完非标数据后服务器会响应,这里接收服务器的非标数据后丢弃,避免MQTT异常 */
		// esp_transport_read(mqtt_client_dump.transport, (char *)buffer, 8, ESP_TRANS_TIMEOUT);

		/* 每次收到新的风暴事件时清除取消状态,此时APP可设置取消该风暴事件 */
		if (ret == 0)
		{
			//g_device_data.iot_dev_node.iot_config.thunder_ctrl.thunder_cancel = 0;
			//g_device_data.bk_iot_dev_node.iot_config.thunder_ctrl.thunder_cancel = 0;
            g_self_data.mod_reg12000_IOT_set.thunder_ctrl.thunder_cancel=0;
	        Inv_WR.mod_reg12000_IOT_set.thunder_ctrl.thunder_cancel=0;
		}
	}

	return ret;
}

 void inv_node_data_process(uint8_t node)
{
    return;
    /* 每台逆变能量线 */
    inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
    if ((NULL!=  advance_param)
		&&(advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1))
    {
//		inv_node_struct *p_node = &g_device_data.inv_dev_node;
//        p_node->inv_base.line_event = SlaveEnergyTypeDetail[node].ALL;
//        p_node->inv_line.line_event = SlaveEnergyTypeDetail[node].ALL;
//		p_node->inv_base.GridAllTotalPower = UploadEMSData.SlaveEnergyData[node].GridAllTotalPower;
//		p_node->inv_base.PVAllTotalPower = UploadEMSData.SlaveEnergyData[node].PVAllTotalPower;
//		p_node->inv_base.ACLoadAllTotalPower = UploadEMSData.SlaveEnergyData[node].ACLoadAllTotalPower;
//
//		p_node->inv_load.ac_phase_number = UploadEMSData.SlaveEnergyData[node].ac_phase_number;
//		memcpy(p_node->inv_load.ac_load, UploadEMSData.SlaveEnergyData[node].ac_load, sizeof(UploadEMSData.SlaveEnergyData[node].ac_load));
//
//		p_node->inv_grid.grid_phase_number = UploadEMSData.SlaveEnergyData[node].grid_phase_number;
//		memcpy(p_node->inv_grid.grid_detail, UploadEMSData.SlaveEnergyData[node].grid_detail, sizeof(UploadEMSData.SlaveEnergyData[node].grid_detail));
//
//		p_node->inv_pv.pv_number = UploadEMSData.SlaveEnergyData[node].pv_number;
//		memcpy(p_node->inv_pv.pv_detail, UploadEMSData.SlaveEnergyData[node].pv_detail, sizeof(UploadEMSData.SlaveEnergyData[node].pv_detail));
    }
}

static void mqtt_param_handler(uint8_t addr, uint8_t funcode)
{
	if ((funcode != 0x06) && (funcode != 0x10)) 
		return;

	/* 触发类型的参数不保存 */
	bk_inv_node_set *p_inv_bk = &Inv_can_WR.bk_inv_dev_set;
	bk_pack_node_set *p_pack_bk = &Inv_can_WR.bk_pack_dev_set;
	inv_set00_struct bk_inv_base = p_inv_bk->inv_set00;
	inv_set01_struct bk_inv_advance = p_inv_bk->inv_set01;
	bk_inv_base.ctrl.all = 0;									// 控制事件参数不保存
	bk_inv_base.ctrl_log_page = 0;								// 日志翻页参数不保存
	bk_inv_base.ctrl_all_energy_type = 0;						// 历史能量参数不保存
	bk_inv_base.ctrl_now_energy_type = 0;						// 当前能量参数不保存
	bk_inv_advance.ct_test.all = 0;								// CT测试使能参数不保存
	bk_inv_advance.ctrl_mix.bit.reset_param = 0;					// 恢复默认参数设置不保存
	bk_inv_advance.ctrl_reset_factory = 0;						// 恢复出厂参数不保存
	if (bk_inv_advance.ctrl_mix.bit.multi_inv_off_grid_en == 0)		// 离网开关为无效时使用本地的开关状态填充
	{
		inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
		if(NULL!=  advance_param)
		{
			bk_inv_advance.ctrl_mix.bit.multi_inv_off_grid_en = advance_param->ctrl_mix.bit.multi_inv_off_grid_en;
			p_inv_bk->inv_set01.ctrl_mix.bit.multi_inv_off_grid_en = advance_param->ctrl_mix.bit.multi_inv_off_grid_en;
		}
	}

	/* 清除时间段时间,设置区清除后本地保存的参数也将被清除 */
	if (p_inv_bk->inv_set00.ctrl.bit.clear_chg_dsg_time_period)
	{
		bk_inv_base.ctrl_time_area = 0;
		memset(&bk_inv_base.ctrl_time, 0x00, sizeof(bk_inv_base.ctrl_time));
		p_inv_bk->inv_set00.ctrl_time_area = 0;
		memset(&p_inv_bk->inv_set00.ctrl_time, 0x00, sizeof(p_inv_bk->inv_set00.ctrl_time));
	}

	/* 虚拟主机参数设置 */
	if (addr == 0)
	{
		reals.flasWrFlag.sBit.set_data_inv =1;
		reals.flasWrFlag.sBit.set_data_pack =1;
		
	}
}




/**
 * 手机APP先使用MQTT V1.0载荷读取
 * IOT返回V1.2载荷协议,告知手机APP,IOT已经支持到V1.2版本
 * 下一次手机使用V1.2载荷协议开始交互
 *
*/
 void iot_mqtt_msg_handle(esp_mqtt_client_handle_t client, QueueHandle_t msg_queue, const char *rsp_topic, QueueHandle_t can_cmd_queue) 
{
    uint8_t rsp_buf[1024];
    uint8_t Frame_type= 0;
    uint16_t plen = 0;
    const uint8_t *pdata = NULL;

    if ((client == NULL) || (msg_queue == NULL)) { /* mqtt 消息队列不存在 */
        ESP_LOGE(TAG, "client or mqtt msg queue");
        return ; /* 退出 */
    }

    queue_msg_t income = {NULL, 0};
    if(xQueueReceive(msg_queue, &income, pdMS_TO_TICKS(400)) == pdTRUE )
	{
		if (income.msg_ptr == NULL) {
			return;
		}

		/* 风暴模式处理 */
	   	if (thunder_mode_handle(income.msg_ptr, income.msg_len) < 1)
		{
			free(income.msg_ptr);
			return;
		}

        if ((income.msg_ptr[0] == 0x01) && (income.msg_ptr[1] == 0x01)) 
		{
            pdata = (const uint8_t *)(income.msg_ptr + 1);
            plen = (income.msg_len - 1);
			Frame_type =1;
			
        } 
		else if ((income.msg_ptr[0] == 0x01) && income.msg_ptr[1] == 0xF8) //载荷协议版本
		{
            pdata = (const uint8_t *)(income.msg_ptr + 10);
            plen = (income.msg_len - 10);
			Frame_type =2;

            ESP_LOG_BUFFER_HEX_LEVEL(TAG, income.msg_ptr, income.msg_len, ESP_LOG_WARN);
        } 
		else if ((income.msg_ptr[0] == UDT_PROT_MARK) && (income.msg_ptr[1] == UDT_PROT_VER))//服务器通用数据通道协议
		{
			ESP_LOGW(TAG, "udt received server data and relay it to device, data length: %d, slave_addr: %d, funcode: %d",
					income.msg_len, income.msg_ptr[10], income.msg_ptr[11]);

#ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
            /*数据通道*/
			udt_process_handle(income.msg_ptr, income.msg_len, UDT_MODE_SVC);
#endif

// #ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
// 			    if(income.msg_ptr[10] == 0)//modbus slave :windy 暂定0=自身，其他-下级透传
// 			    {
// 					udt_receive(income.msg_ptr, income.msg_len);
// 				}
// 				else 
// 				{
// 					udt_relay_to_device(income.msg_ptr, income.msg_len);
// 				}

// #endif

			Frame_type =3;

		}		
        else if ((income.msg_len > 5) && (income.msg_ptr[0] == 0x00) && (income.msg_ptr[1] == 0x09)) 
		{  
            plen = (income.msg_ptr[2] << 8) | income.msg_ptr[3];
            ESP_LOGW (TAG, "https_ota_cmd_parse");
            https_ota_cmd_parse((char *)&income.msg_ptr[4], plen); // https 升级命令
            pdata = NULL; 
            plen = 0;
			Frame_type =4;
			
        }
		else 
        {
            pdata = NULL;
            plen = 0;
        }

		if(2 == Frame_type)//modbus
		{
			int rst = Modbus_Format_Check(pdata, plen);
			if (rst >= 0)//功能码
			{
				uint8_t md_addr = pdata[0];
				// can_cmd_queue_struct can_cmd = {NULL, 10, 0};
                can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};
				if ((rst == 0x06 || rst == 0x10) && can_cmd_queue) 
				{  // 当MODBUS为设置指令时,才需要开辟空间
					can_cmd.cmd = heap_caps_malloc(sizeof(can_data_label) * can_cmd.num, MALLOC_CAP_SPIRAM); /*  */
					if (can_cmd.cmd == NULL) {
						ESP_LOGE (TAG, "ble to can malloc failed");
					}
				}
		
		
				pack_data_head(0x01, rsp_buf, income.msg_ptr[2], 0, 0, 0);	// 封装mqtt数据1.2v协议头
				wifi_sta_config_check((uint8_t *)pdata);
		

				reg_position_list_t *position_list = NULL;

	            int rsp_len = Modbus_Slave(pdata, plen, &rsp_buf[10], can_cmd.cmd, &can_cmd.num, MD_CHL_WIFI_CLOUD, &position_list); // modbus协议解析
	            if (rsp_len > 0)
				{

					mqtt_param_handler(md_addr, pdata[1]);
                     ESP_LOGE(TAG, "topic:%s publish ", rsp_topic);
  					 ESP_LOG_BUFFER_HEX(TAG, &rsp_buf, rsp_len + 10);
	                if (esp_mqtt_client_publish(client, rsp_topic, (const char *)rsp_buf, (rsp_len + 10), 0, 0) == -1) {
	                    ESP_LOGE(TAG, "topic:%s publish failed", rsp_topic);
	                }

	                if (vXmodemCmdCheck(pdata[0], OTA_CH_CLOUD_TO_SELF)) {  /* WiFi通道xmodem命令检测 */
	                    ESP_LOGW(TAG, "wifi received xmodem start cmd");
	                }

	                if (can_cmd.cmd != NULL) 
					{
						can_cmd.md_addr = md_addr; //modbus指令转换为CAN指令的CAN节点ID
						if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(100)) != pdPASS) 
						{
							if (can_cmd.cmd) 
							{
								free(can_cmd.cmd); /* 释放CAN转换关系消息 */
							}
						}
						else//ok
						{
							reals.BLE_WIFI_to_Can_Cnt++;
                        	ESP_LOGE(TAG, "BLE_WIFI_to_Can_Cnt HUANGJI BBB:%d", reals.BLE_WIFI_to_Can_Cnt);

						}
	                }
	                else if (NULL != position_list) 
					{
//						sys_new_position_and_transmit(position_list);//copy from A80
	                }				
	            } 
				else 
				{
	                if (can_cmd.cmd) 
					{
	                    free(can_cmd.cmd); /* 释放CAN转换关系消息 */
	                }
	            }
                
                free(income.msg_ptr); /* 释放开辟出的缓存空间 */

            	return ;
			}
        }
    }

//__exit:

    rsp_buf[10] = Xmodem_Client_top(OTA_CH_CLOUD_TO_SELF, pdata, plen); /* xmodem运行 */
    if (rsp_buf[10]) {
        pack_data_head(0x01, rsp_buf, income.msg_ptr[2], 0, 0, 0);  // 封装响应消息自定义头
        if (esp_mqtt_client_publish(client, rsp_topic, (const char *)rsp_buf, 11, 0, 0) != -1) {
            ESP_LOGI(TAG, "publish resp:0x%x", rsp_buf[10]);
        }
    }

    if (income.msg_ptr) {
        free(income.msg_ptr); /* 释放开辟出的缓存空间 */
    }
}

uint8_t iot_mqtt_get_connect1(void) {
    return login_info.mqtt_is_login;
}

uint8_t iot_mqtt_get_connect2(void) {
    return login2_info.mqtt_is_login;
}

const char *iot_mqtt_get_host(void) {
    return login_info.raw_url;
}

const char *iot_mqtt_get_host2(void) {
    return login2_info.raw_url;
}

void iot_mqtt_new_host(const char *new_host) {
    cloud_diag_mqtt_ctrl("NEW_HOST", new_host, login_info.mqtt_is_login,
                         login_step, login_info.mqtt_connect_fail);
    memcpy(login_info.raw_url, new_host, sizeof(login_info.raw_url));
    mqtt_step_goto(STEP_MQTT_RESTART, "new_host");
}

void iot_mqtt_new_host2(const char *new_host) 
{
    memcpy(login2_info.raw_url, new_host, sizeof(login2_info.raw_url));
    login2_step = STEP_MQTT_RESTART;
}

///////////
void iot_mqtt_new_dev(const char *dev_type, uint64_t dev_sn) {
    memcpy(login_info.dev_type, dev_type, sizeof(login_info.dev_type));
    login_info.dev_sn = dev_sn;

    memcpy(login2_info.dev_type, dev_type, sizeof(login2_info.dev_type));
    login2_info.dev_sn = dev_sn;
    ESP_LOGI(TAG,"iot_mqtt_new_dev TYPE:%s ,SN:%llu",dev_type,dev_sn);
	
    login_step = STEP_MQTT_RESTART;
    login2_step = STEP_MQTT_RESTART;
}

void iot_mqtt_new_iot(const char *iot_type, uint64_t iot_sn, uint64_t safety_code) {
    memcpy(login_info.iot_type, iot_type, sizeof(login_info.iot_type));
    login_info.iot_sn     = iot_sn;
    login_info.safetyCode = safety_code;

    memcpy(login2_info.iot_type, iot_type, sizeof(login2_info.iot_type));
    login2_info.iot_sn     = iot_sn;
    ESP_LOGI(TAG,"iot_mqtt_new_iot TYPE:%s ,SN:%llu",iot_type,iot_sn);
//    login2_info.safetyCode = safety_code;


    login_step = STEP_MQTT_RESTART;
    login2_step = STEP_MQTT_RESTART;
}

void iot_mqtt_delete(uint8_t mode) 
{
    cloud_diag_mqtt_ctrl("DELETE", "iot_mqtt_delete", login_info.mqtt_is_login,
                         login_step, login_info.mqtt_connect_fail);
    if (NETIF_TYPE_WIFI_STA == mode && 1 == SetData.dev_info_t.on_off.bit.wifi_enable)
	{
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
		if(0 != strcmp(wifi_info.sta_ssid,MESH_AP_SSID))//different  memcmp SetData.dev_info_t.wifi_sta_ssid
		{
			reals.wifi_connect_ap = 0;
		}
#else
		reals.wifi_connect_ap = 0;
#endif
        ESP_LOGE(TAG, " wifi_connect_ap:%u", reals.wifi_connect_ap);
	}

	if(NETIF_TYPE_PPP == mode && 1 == SetData.dev_info_t.on_off.bit._4g_enable)
	{
		reals.Cellular4G_step = LOST_IP_4G_READY;
        ESP_LOGE(TAG, " Cellular4G_step:%u", reals.Cellular4G_step);
	}			

	if(NETIF_TYPE_ETH == mode && 1 == SetData.dev_info_t.on_off.bit.Eth_enable)
	{
		reals.Ethernet_SPI_step = ETH_LOST_IP;
        ESP_LOGE(TAG, " Ethernet_SPI_step:%u", reals.Ethernet_SPI_step);
	}

    // reals.wifi_connect_ap = 0;
    if (mode == login_info.current_netif_id)
    {
        mqtt_step_goto(STEP_MQTT_CONNECT_FAIL, "iot_mqtt_delete");
    }
	
}

void iot_mqtt_start(uint8_t mode) {
    cloud_diag_mqtt_ctrl("START", "iot_mqtt_start", login_info.mqtt_is_login,
                         login_step, login_info.mqtt_connect_fail);
    ESP_LOGI(TAG, "iot_mqtt_start:%u, current_netif_id:%u", mode, login_info.current_netif_id);

    if (iot_mqtt_start_should_skip(mode)) {
        ESP_LOGW(TAG, "skip mqtt restart: already logged in on netif %u, step:%u",
                 mode, login_step);
        cloud_diag_mqtt_ctrl("START_SKIP", "already_login", login_info.mqtt_is_login,
                             login_step, login_info.mqtt_connect_fail);
        return;
    }

	if (NETIF_TYPE_WIFI_STA == mode && 1 == SetData.dev_info_t.on_off.bit.wifi_enable)
	{
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
		if(0 != strcmp(wifi_info.sta_ssid,MESH_AP_SSID))//different  memcmp SetData.dev_info_t.wifi_sta_ssid
		{
			reals.wifi_connect_ap = 1;
		}
#else
		reals.wifi_connect_ap = 1;
#endif
        ESP_LOGE(TAG, " wifi_connect_ap:%u", reals.wifi_connect_ap);
	}

	if(NETIF_TYPE_PPP == mode && 1 == SetData.dev_info_t.on_off.bit._4g_enable)
	{
		reals.Cellular4G_step = GET_IP_4G_READY;
        ESP_LOGE(TAG, " Cellular4G_step:%u", reals.Cellular4G_step);
	}			

	if(NETIF_TYPE_ETH == mode && 1 == SetData.dev_info_t.on_off.bit.Eth_enable)
	{
		reals.Ethernet_SPI_step = ETH_GET_IP_READY;
        ESP_LOGE(TAG, " Ethernet_SPI_step:%u", reals.Ethernet_SPI_step);
	}		

    // 优先级更低的网卡上线，不切换MQTT联网方式
    // if(mode > login_info.current_netif_id)
    // {
    //     return;
    // }

    login_info.current_netif_id = mode;
    set_mqtt_client_netif(login_info.current_netif_id);
    mqtt_step_goto(STEP_MQTT_RESTART, "iot_mqtt_start");
}
 
 void set_mqtt_stop(void) 
{
//	 xSemaphoreTake(xMqttSemaphore,  portMAX_DELAY);
    if (0 == login_info.mqtt_is_start)
    {
        return;
    }
    esp_mqtt_client_stop(login_info.mqtt_client);
	 esp_err_t err = esp_mqtt_client_destroy(login_info.mqtt_client);
	 mqtt_client_dump.transport = NULL;
	 login_info.mqtt_client = NULL;
	 login_info.mqtt_is_login = 0;
	 login_info.mqtt_is_start = 0;
	 
//	 login_info->client_connected = 0;
//	 xSemaphoreGive(xMqttSemaphore);
	 
	 if (err == ESP_ERR_INVALID_ARG) {
		 ESP_LOGI(__func__, "mqtt client yet destroy");
	 } else if (err == ESP_OK){
		 ESP_LOGI(__func__, "mqtt client destroy done");
	 } else {
	 }
//	 return 1;
//	 ESP_LOGE(TAG,"mqtt stop");
	 // if (mqtt_task_handle != NULL) {
	 // 	vTaskSuspend(mqtt_task_handle); // 挂起MQTT任务
	 // }
	 login_step = STEP_MQTT_CONNECT_FAIL;	 // wifi不在线退出
 }

uint8_t iot_mqtt_login_state(void) 
{
    return login_info.mqtt_is_login; /* MQTT连接状态 */
}

void iot_mqtt_ChageFlagSet(uint8_t flag)
{
	if(reals.mqttChange_flag!=flag)
		reals.mqttChange_flag=flag;
}

uint8_t iot_mqtt_ChageFlagGet(void)
{
	return reals.mqttChange_flag;
}

void iot_mqtt_update(void)
{
	if ((1 == SetData.dev_info_t.on_off.bit.wifi_enable)
	&&(1 == reals.wifi_connect_ap)
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
	&&(1 == reals.wifi_mesh_root)
#endif
	)
    {
        if(iot_mqtt_ChageFlagGet())		 	
        {
            ESP_LOGI(TAG,"reals.mqttChange_flag:%d",iot_mqtt_ChageFlagGet());
            if(iot_mqtt_ChageFlagGet()==1)
            {
                iot_wifi_new_iot(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);	
                iot_wifi_new_dev(SetData.dev_info_t.INV_dev_type,  SetData.dev_info_t.INV_dev_sn);
                iot_bind_clean();
                iot_mqtt_ChageFlagSet(0);
            }else if(iot_mqtt_ChageFlagGet()==2)
            {
                iot_wifi_new_iot(SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, SetData.dev_info_t.Parallel_dev_safetycode);		
                iot_wifi_new_dev(SetData.dev_info_t.Parallel_dev_type,  SetData.dev_info_t.Parallel_dev_sn);
                iot_bind_clean();
                iot_mqtt_ChageFlagSet(0);
            }
        }

        if((strcmp(login_info.iot_type, IOT_TYPE_HA1) != 0)     //mqtt登录参数的iot类型不为HA1
        && (reals.Addr_can_master == 1)                 //设备为主机
        &&(SetData.dev_info_t.Parallel_dev_sn != 0)         //并机SN码不为空
        &&(strlen(SetData.dev_info_t.Parallel_dev_type)!=0) //achub设备类型不为空
        &&(SetData.dev_info_t.Parallel_dev_safetycode!=0)   //ACHUB安全码不为空
        &&(reals.online_ACHUB_num>0))       //存在achub设备接入
        {
            ESP_LOGI(TAG,"iot_mqtt_update ACHUB online, use parallel mode");
            iot_wifi_new_iot(SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, SetData.dev_info_t.Parallel_dev_safetycode);		
            iot_wifi_new_dev(SetData.dev_info_t.Parallel_dev_type,  SetData.dev_info_t.Parallel_dev_sn);
            iot_bind_clean();
        }

        if(reals.online_ACHUB_num==0)   //不存在achub接入
        {
            bool inv_id_valid = (reals.online_Inv_num > 0)
                            && (SetData.dev_info_t.INV_dev_sn != 0)
                            && (0 != memcmp(SetData.dev_info_t.INV_dev_type, reals.zero, sizeof(SetData.dev_info_t.INV_dev_type)));
            bool inv_id_changed = (0 != memcmp(login_info.dev_type, SetData.dev_info_t.INV_dev_type, sizeof(login_info.dev_type)))
                               || (login_info.dev_sn != SetData.dev_info_t.INV_dev_sn);

        	if((inv_id_valid && inv_id_changed)
			||((strcmp(login_info.iot_type, IOT_TYPE_IOT) != 0) && (reals.Addr_can_master == 1))) //变化
            {
                ESP_LOGI(TAG,"iot_type: %s Or dev_type:%s Err new wifi init ",login_info.iot_type,login_info.dev_type);
                iot_wifi_new_iot(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);	
                iot_wifi_new_dev(SetData.dev_info_t.INV_dev_type,  SetData.dev_info_t.INV_dev_sn);
                iot_bind_clean();
            }
        }


    }
}
/*
网络连接状态


return:

*/
void Net_Connect_status(void) 
{
    /*
    LED_Ethernet:
    DTU与BLUETTI cloud 通讯正常:常亮
    Internet disconnected：闪烁，200ms ON，200ms OFF
    Internet connected and Server disconnect：闪烁，600ms ON，600ms OFF
    local APP connected:闪烁，200ms ON，1800ms OFF
    */

    //local APP connected
    if((1 == SetData.dev_info_t.on_off.bit.ble_enable)&&(g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect == 1))
    {
        reals.LED_stat.bits.LED_Net = LED_STATE_BLINK_200MS_1800MS;
    }
	//DTU与BLUETTI cloud
//	else if((STEP_MQTT_RESTART != login_step)
//		&&(STEP_HOST_DNS != login_step)
//		&&(STEP_MQTT_EXIT != login_step))//net ok
    else if(STEP_MQTT_MSG_HANDLE == login_step)
	{
		reals.LED_stat.bits.LED_Net =LED_STATE_CONTINUE_ON;
	}
    //Internet connected and Server disconnect：
	else if(((1 == SetData.dev_info_t.on_off.bit.wifi_enable)&&(1 == iot_netif_is_connected()))
		||((1 == SetData.dev_info_t.on_off.bit.Eth_enable)&&(ETH_MAC_LINK_UP ==reals.Ethernet_SPI_step))
	)	
	{
		reals.LED_stat.bits.LED_Net = LED_STATE_BLINK_600MS;
	}		
	//Internet disconnected
	else
	{
		reals.LED_stat.bits.LED_Net = LED_STATE_BLINK_200MS;
	}


}

/**
 * @brief 当设备有以下特定事件发生时,IOT主动发布事件信息给到后台
 * - 1、AC输出变化
 * - 2、DC输出变化
 * - 3、PV充电变化
 * - 4、电网接入变化
 * - 5、SOC变化
 * 这些事件针对整个系统而不是针对哪一台设备
 *
 * @return 无
 */
static int iot_mqtt_dev_event_ann(void) {

    //EXT_RAM_BSS_ATTR static ac_hub_set old_achub_set;
    EXT_RAM_BSS_ATTR static device_ctrl old_dev_ctrl;
    uint8_t buf[250] = {0};
    int ret = 0;

    if (login_info.mqtt_is_login != 1) {
        old_dev_ctrl.all = 0xFFFF;
        //old_achub_set.all=0xFFFF;
        old_dev_soc = 0xFF;
        return ret; // MQTT登录断开不在进行事件推送
    }

	// uint16_t now_dev_soc = Inv_can[DEV_MAIN_NODE_MAX].pack_data[INV_MAX_NUM].pack_announce.soc;				// 当前系统soc
	// pack除升级信息靠逆变地址发出，其余信息按照can协议上的pack地址发
    ac_hub_set now_achub_set=Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting;
    //ESP_LOGI(TAG,"achubset:0x%x  inv:%u",now_achub_set.all,Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting.all);
    uint8_t now_dev_soc = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc;				// 当前系统soc
	device_ctrl now_dev_status = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ctrl_status;	// 当前系统开关状态
    now_dev_status.bit.ac_on|=now_achub_set.bit.ac_switch;
    /* 检测系统soc与开关状态是否发生变化 */
	if ((now_dev_soc != old_dev_soc) || (now_dev_status.all != old_dev_ctrl.all))
	{
        ESP_LOGE(TAG, "soc or ctel_status change");
        old_dev_soc = now_dev_soc;
        old_dev_ctrl.all = now_dev_status.all;
        //old_achub_set.all=now_achub_set.all;
       // old_dev_ctrl.bit.ac_on|=old_achub_set.bit.ac_switch; //ha1开关打开也被视为ac开关打开

		/* 上报给后台的数据需要使用MQTT负载协议Payload V1.2,(参考负载协议) */
		pack_data_head(0x01, buf, PUB_EVEVT, 0, 0, 0);
		buf[10] = MD_SUMMARY_ADDR;
		buf[11] = 0x49;

		/* 填充系统开关状态与soc */
        snprintf((char *)&buf[12], sizeof(buf)-12, IOT_PUBLISH_EVENT,
				 old_dev_ctrl.bit.ac_on,
				 old_dev_ctrl.bit.dc_on,
				 old_dev_ctrl.bit.pv_on,
				 old_dev_ctrl.bit.grid_on,
				 old_dev_soc);
        ESP_LOGI(TAG,"now_dev_status:0x%u  old_dev_ctrl:%u",now_dev_status.all,old_dev_ctrl.all);
		uint16_t data_len = 2 + strlen((const char*)&buf[12]) + 1;
		uint16_t crc16 = calcu_crc16(&buf[10], data_len);
		buf[10 + data_len + 0] = crc16 & 0xFF;
		buf[10 + data_len + 1] = (crc16>>8) & 0xFF;
//		if (esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)buf, 10+data_len+2, 0, 0) != -1) 
		if(TOP_MQTT_Send_Public_Data(login_info.pub_topic, buf, 10+data_len+2))
		{
            ret = 10+data_len+2;
			ESP_LOGI(TAG, "%s", &buf[12]);
		}
	}

    return ret;
}

/*------------------------------------------------------------------------
*@Function： is_device_online
是否有设备在线

-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
1-有
0-无
*/
static bool is_device_online(void)
{
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < INV_MAX_NUM; i++)
		{
			if(Inv_can[node].inv_data[i].online) 
			{
				return true;
			}
		}

		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			if(Inv_can[node].pack_data[0].pack_announce.online & (1 << i)) 
			{
				return true;
			}
		}
	}

	return false;
}

static void mqtt_inv_node_data_process(uint8_t node)
{
	return;
    /* 每台逆变能量线 */
    inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
    if ((NULL!=  advance_param)
		&&(advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1))
    {
//		inv_node_struct *p_node = &g_device_data.poll_inv_dev_node;
//        p_node->inv_base.line_event = SlaveEnergyTypeDetail[node].ALL;
//        p_node->inv_line.line_event = SlaveEnergyTypeDetail[node].ALL;
//		p_node->inv_base.GridAllTotalPower = UploadEMSData.SlaveEnergyData[node].GridAllTotalPower;
//		p_node->inv_base.PVAllTotalPower = UploadEMSData.SlaveEnergyData[node].PVAllTotalPower;
//		p_node->inv_base.ACLoadAllTotalPower = UploadEMSData.SlaveEnergyData[node].ACLoadAllTotalPower;
//
//		p_node->inv_load.ac_phase_number = UploadEMSData.SlaveEnergyData[node].ac_phase_number;
//		memcpy(p_node->inv_load.ac_load, UploadEMSData.SlaveEnergyData[node].ac_load, sizeof(UploadEMSData.SlaveEnergyData[node].ac_load));
//
//		p_node->inv_grid.grid_phase_number = UploadEMSData.SlaveEnergyData[node].grid_phase_number;
//		memcpy(p_node->inv_grid.grid_detail, UploadEMSData.SlaveEnergyData[node].grid_detail, sizeof(UploadEMSData.SlaveEnergyData[node].grid_detail));
//
//		p_node->inv_pv.pv_number = UploadEMSData.SlaveEnergyData[node].pv_number;
//		memcpy(p_node->inv_pv.pv_detail, UploadEMSData.SlaveEnergyData[node].pv_detail, sizeof(UploadEMSData.SlaveEnergyData[node].pv_detail));
    }
}
#define  MQTT_PUBLISH_INV_FRAME_MAX	15//
/*
windy change

*/
static void iot_publish_inv_data_step(uint8_t addr, uint16_t cycle, uint8_t i,uint8_t counter_total)
{
	uint16_t start_addr = 0;
	uint16_t reg_num = 0;

//	for (int i = 0; i < 4; i++)
	{
//		vTaskDelay(pdMS_TO_TICKS(5));
		if (login_info.mqtt_is_login == 0) return;

		switch (i)
		{		
			case 0: start_addr = MOD_REG_START_ADDR_00100; reg_num = MOD_REG_LEN_00100; break;
			case 1: start_addr = MOD_REG_START_ADDR_01100; reg_num = MOD_REG_LEN_01100; break;
			case 2: start_addr = MOD_REG_START_ADDR_01200; reg_num = MOD_REG_LEN_01200; break;
			case 3: start_addr = MOD_REG_START_ADDR_01300; reg_num = MOD_REG_LEN_01300; break;
			case 4: start_addr = MOD_REG_START_ADDR_01400; reg_num = MOD_REG_LEN_01400; break;
			case 5: start_addr = MOD_REG_START_ADDR_01500; reg_num = MOD_REG_LEN_01500; break;
			case 6: start_addr = MOD_REG_START_ADDR_01600; reg_num = MOD_REG_LEN_01600; break;
			case 7: start_addr = MOD_REG_START_ADDR_02000; reg_num = MOD_REG_LEN_02000; break;
			case 8: start_addr = MOD_REG_START_ADDR_02200; reg_num = MOD_REG_LEN_02200; break;
			case 9: start_addr = MOD_REG_START_ADDR_02300; reg_num = MOD_REG_LEN_02300; break;
			case 10: start_addr = MOD_REG_START_ADDR_02400; reg_num = MOD_REG_LEN_02400; break;
            case 11: start_addr = MOD_REG_START_ADDR_11000; reg_num = MOD_REG_LEN_11000; break;
            case 12: start_addr = MOD_REG_START_ADDR_12000; reg_num = MOD_REG_LEN_12000; break;
			default: return;
		}

        

        if(start_addr==MOD_REG_START_ADDR_00100)
        {
            if(0 == addr)//汇总
			{
                ESP_LOGI(TAG,"enter iot_publish_inv_data_step addr:%d,PvTotalChargingEnergy:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.PvTotalChargingEnergy);
            }else{
                ESP_LOGI(TAG,"enter iot_publish_inv_data_step addr:%d,PvTotalChargingEnergy:%lu",addr,Inv[(addr-1)].mod_reg00100_AppPage1.PvTotalChargingEnergy);
            }
        }
// ESP_LOGE(TAG,"iot_publish_inv_data_step addr=%d, cycle=%d, i+1=%d",addr,cycle,i+1);
		pack_data_head(0x01, mqtt_tx_buf, PUB_PERIOD, cycle, counter_total, i+1);
		int len = Modbus_MasterWriteRegs (addr, start_addr, reg_num, mqtt_tx_buf + 10);
		if ((len) 
//			&&((-1) != esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)mqtt_tx_buf, (10 + len), 0, 0))
			&&((-1) != TOP_MQTT_Send_Public_Data(login_info.pub_topic, mqtt_tx_buf, 10 + len))
		)			
		{
			 ESP_LOGI(TAG, "mqtt client publish inv successfully, sstart_addr:%u lave addr is %d \n", start_addr,addr);//ESP_LOGI
		} else {
			 ESP_LOGE(TAG, "Inv mqtt client publish inv failed,start_addr:%u  slave addr is %d\n", start_addr,addr);
		}
#if 1
        if(start_addr==MOD_REG_START_ADDR_00100)
        {
            if(0 == addr)//汇总
			{
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,.ctrl_status.all:0x%x",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.ctrl_status.all);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,DCLoadAllTotalPower:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.DCLoadAllTotalPower);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ACLoadAllTotalPower:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.ACLoadAllTotalPower);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,PVAllTotalPower:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.PVAllTotalPower);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,GridAllTotalPower:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.GridAllTotalPower);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,DCLoadTotalEnergy:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.DCLoadTotalEnergy);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ACLoadTotalEnergy:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.ACLoadTotalEnergy);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,PvTotalChargingEnergy:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.PvTotalChargingEnergy);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,GridTotalChargingEnergy:%lu",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.GridTotalChargingEnergy);
			}
			else//单INV
			{
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,.ctrl_status.all:0x%x",addr,Inv[(addr-1)].mod_reg00100_AppPage1.ctrl_status.all);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,DCLoadAllTotalPower:%lu",addr,Inv[(addr-1)].mod_reg00100_AppPage1.DCLoadAllTotalPower);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ACLoadAllTotalPower:%lu",addr,Inv[(addr-1)].mod_reg00100_AppPage1.ACLoadAllTotalPower);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,PVAllTotalPower:%lu",addr,Inv[(addr-1)].mod_reg00100_AppPage1.PVAllTotalPower);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,GridAllTotalPower:%lu",addr,Inv[(addr-1)].mod_reg00100_AppPage1.GridAllTotalPower);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,DCLoadTotalEnergy:%lu",addr,Inv[(addr-1)].mod_reg00100_AppPage1.DCLoadTotalEnergy);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ACLoadTotalEnergy:%lu",addr,Inv[(addr-1)].mod_reg00100_AppPage1.ACLoadTotalEnergy);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,PvTotalChargingEnergy:%lu",addr,Inv[addr-1].mod_reg00100_AppPage1.PvTotalChargingEnergy);
				ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,GridTotalChargingEnergy:%lu",addr,Inv[addr-1].mod_reg00100_AppPage1.GridTotalChargingEnergy);
			}	
             ESP_LOGI(TAG,"Inv_can[DEV_MAIN_NODE_MAX].inv_data[DEV_MAIN_NODE_MAX].inv_base.ctrl_status.all:0x%x",Inv_can[DEV_MAIN_NODE_MAX].inv_data[DEV_MAIN_NODE_MAX].inv_base.ctrl_status.all);
            //ESP_LOGI(TAG,"Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.PvTotalChargingEnergy:%lu",Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.PvTotalChargingEnergy);
        }
#endif
#if 0
        if(start_addr==MOD_REG_START_ADDR_02000)
        {
            if(0 == addr)//汇总
			{
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ctrl_ac:%u",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.ctrl_ac);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ctrl_dc:%u",addr,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.ctrl_dc);
			}
			else//单INV
			{
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ctrl_ac:%u",addr,Inv[(addr-1)].mod_reg02000_Inv_base_set.ctrl_ac);
                ESP_LOGI(TAG,"iot_publish_inv_data_step addr:%d,ctrl_dc:%u",addr,Inv[(addr-1)].mod_reg02000_Inv_base_set.ctrl_dc);
			}	
            ESP_LOGI(TAG,"Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.ctrl_ac:%u",Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set00.ctrl_ac);
            ESP_LOGI(TAG,"Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.ctrl_dc:%u",Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set00.ctrl_dc);
            ESP_LOGI(TAG,"Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_ac:%u",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_ac);
            ESP_LOGI(TAG,"Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_dc:%u",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_dc);
        }
#endif
        
	}
}



#define  MQTT_PUBLISH_PACK_FRAME_MAX	3
/*
windy change

*/
static void iot_publish_pack_data_step(uint8_t addr, uint16_t cycle, uint8_t i,uint8_t counter_total)
{
	uint16_t start_addr = 0;
	uint16_t reg_num = 0;

//	for (int i = 0; i < 4; i++)
	{
//		vTaskDelay(pdMS_TO_TICKS(5));
		if (login_info.mqtt_is_login == 0) return;
		switch (i)
		{
			case 0: start_addr = MOD_REG_START_ADDR_06000; reg_num = MOD_REG_LEN_06000; break;
			case 1: start_addr = MOD_REG_START_ADDR_06100; reg_num = MOD_REG_LEN_06100; break;
            case 2: start_addr = MOD_REG_START_ADDR_06300; reg_num = MOD_REG_LEN_06300; break;
			default: return;
		}

        ESP_LOGI(TAG,"iot_publish_pack_data_step addr:0x%x start_addr:0x%x counter_total:%d i:%d",addr,start_addr,counter_total,i);
		pack_data_head(0x01, mqtt_tx_buf, PUB_PERIOD, cycle, counter_total, i+1);//4 MQTT_PUBLISH_PACK_FRAME_MAX
		int len = Modbus_MasterWriteRegs (addr, start_addr, reg_num, mqtt_tx_buf + 10);
		if ((len) 
//			&&((-1) != esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)mqtt_tx_buf, (10 + len), 0, 0))
			&&((-1) != TOP_MQTT_Send_Public_Data(login_info.pub_topic, mqtt_tx_buf, 10 + len))
		) 
		{
			 ESP_LOGI(TAG, "mqtt client publish pack successfully");
		} 
		else 
		{
			 ESP_LOGE(TAG, "Pack mqtt client publish pack failed");
		}

	}
}

static void iot_publish_summary_pack_data(uint8_t addr, uint16_t cycle, uint8_t i,uint8_t counter_total)
{
	uint16_t start_addr = 0;
	uint16_t reg_num = 0;

    if (login_info.mqtt_is_login == 0) return;
    switch (i)
    {
        case 0: start_addr = MOD_REG_START_ADDR_06000; reg_num = MOD_REG_LEN_06000; break;
        default: return;
    }

    ESP_LOGI(TAG,"iot_publish_summary_pack_data addr:0x%x start_addr:0x%x counter_total:%d i:%d",addr,start_addr,counter_total,i);
    pack_data_head(0x01, mqtt_tx_buf, PUB_PERIOD, cycle, counter_total, i+1);//4 MQTT_PUBLISH_PACK_FRAME_MAX
    int len = Modbus_MasterWriteRegs (addr, start_addr, reg_num, mqtt_tx_buf + 10);
    if ((len) 
//			&&((-1) != esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)mqtt_tx_buf, (10 + len), 0, 0))
        &&((-1) != TOP_MQTT_Send_Public_Data(login_info.pub_topic, mqtt_tx_buf, 10 + len))
    ) 
    {
            ESP_LOGI(TAG, "mqtt client publish pack successfully");
    } 
    else 
    {
            ESP_LOGE(TAG, "Pack mqtt client publish pack failed");
    }

}

static void iot_publish_single_pack_data(uint8_t addr, uint16_t cycle, uint8_t i,uint8_t counter_total)
{
	uint16_t start_addr = 0;
	uint16_t reg_num = 0;

    if (login_info.mqtt_is_login == 0) return;
    switch (i)
    {
        case 0: start_addr = MOD_REG_START_ADDR_06100; reg_num = MOD_REG_LEN_06100; break;
        case 1: start_addr = MOD_REG_START_ADDR_06300; reg_num = MOD_REG_LEN_06300; break;
        default: return;
    }

    ESP_LOGI(TAG,"iot_publish_single_pack_data addr:0x%x start_addr:0x%x counter_total:%d i:%d",addr,start_addr,counter_total,i);
    pack_data_head(0x01, mqtt_tx_buf, PUB_PERIOD, cycle, counter_total, i+1);//4 MQTT_PUBLISH_PACK_FRAME_MAX
    int len = Modbus_MasterWriteRegs (addr, start_addr, reg_num, mqtt_tx_buf + 10);
    if ((len) 
//			&&((-1) != esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)mqtt_tx_buf, (10 + len), 0, 0))
        &&((-1) != TOP_MQTT_Send_Public_Data(login_info.pub_topic, mqtt_tx_buf, 10 + len))
    ) 
    {
            ESP_LOGI(TAG, "mqtt client publish pack successfully");
    } 
    else 
    {
            ESP_LOGE(TAG, "Pack mqtt client publish pack failed");
    }

}

/*
return:
0-一个大循环执行完毕；
1-一个大循环正在执行；
*/
static uint8_t iot_publish_Version(uint16_t *cycle_id)
{
	uint8_t rtn = 0xFF;

	uint8_t buf[100] = {0};
	pack_data_head(0x01, mqtt_tx_buf, PUB_PERIOD, *cycle_id, 1, 0); 									// 协议头数据
	int len = Modbus_MasterWriteRegs (MD_SUMMARY_ADDR, MOD_REG_START_ADDR_00000, MOD_REG_LEN_00000, mqtt_tx_buf + 10);
    ESP_LOGE(TAG,"iot_publish_Version summary cycle_id=%d",*cycle_id);
    if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据cycle_id的完整数据包的Counter ID

	if ((len) 
//			&&((-1) != esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)mqtt_tx_buf, (10 + len), 0, 0))
		&&((-1) != TOP_MQTT_Send_Public_Data(login_info.pub_topic, mqtt_tx_buf, 10 + len))
	) 
	{
		 ESP_LOGI(TAG, "mqtt client publish iot_publish_Version successfully");
	} 
	else 
	{
		 ESP_LOGE(TAG, "IOT mqtt client publish iot_publish_Version failed");
	}	
	rtn=0;
	return rtn;
}

/*
汇总：

return:
0-一个大循环执行完毕；
1-一个大循环正在执行；
*/
static uint8_t iot_publish_summary_A(uint16_t *cycle_id)//inv
{
	int inv_online = 0;
	int pack_online = 0;
	static uint16_t Cnt = 0;
	uint8_t z = 0;
	uint16_t STEP_Z_MAX = MQTT_PUBLISH_INV_FRAME_MAX;//最内for循环
	
	uint8_t rtn = 0xFF;
	z = Cnt%STEP_Z_MAX;
	
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < INV_MAX_NUM; i++)
		{
			if(Inv_can[node].inv_data[i].online)
			{
				inv_online = 1;
				break;
			}
		}
	}


	if (inv_online)
	{
        iot_publish_inv_data_step(MD_SUMMARY_ADDR, *cycle_id,z,STEP_Z_MAX);
		ESP_LOGI(TAG, "iot_publish_summary_A summary cycle_id:%u",*cycle_id);
        if(z == STEP_Z_MAX - 1)// 0到10 11段寄存器 都上报完一轮后，Counter ID加1
        {
            if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据的完整数据包的Counter ID
        }
	}

	if(++Cnt >= (STEP_Z_MAX))
	{
		Cnt=0;
		rtn=0;
	}
	else
	{
		rtn=1;
	}


	return rtn;

}

/*
return:
0-一个大循环执行完毕；
1-一个大循环正在执行；
*/
static uint8_t iot_publish_summary_B(uint16_t *cycle_id)//pack
{
	int pack_online = 0;
	static uint16_t Cnt = 0;
	uint8_t z = 0;
	uint16_t STEP_Z_MAX = MQTT_PUBLISH_PACK_FRAME_MAX-2;
	
	uint8_t rtn = 0xFF;
	z = Cnt%STEP_Z_MAX;//上传0段数据（6000段）
	
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			if(Inv_can[node].pack_data[0].pack_announce.online & (1 << i))
			{
				pack_online = 1;
				break;
			}
		}
	}

	if (pack_online)
	{
        // cycle = cycle_count;
		iot_publish_summary_pack_data(MD_SUMMARY_ADDR, *cycle_id,z,STEP_Z_MAX);
		ESP_LOGI(TAG, "iot_publish_summary_B summary cycle_id:%u",*cycle_id);
        if(z == STEP_Z_MAX - 1)// 0段寄存器 上报完一轮后，Counter ID加1
        {
            if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据的完整数据包的Counter ID
        }      
	}

	if(++Cnt >= (STEP_Z_MAX))
	{
		Cnt=0;
		rtn=0;
	}
	else
	{
		rtn=1;
	}
	return rtn;
	
}

/*
外置电池包同类型数据上报
*/
static uint8_t iot_publish_ext_pack_group_step(uint16_t *cycle_id)
{
	static uint16_t Cnt = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t z = 0;
    uint8_t send_flag=0;

	uint16_t STEP_X_MAX=	DEFAULT_PACK_TYPE_NUM;//最外for循环,针对3种电池包类型消息上报
	uint16_t STEP_Y_MAX=	1;//同类型电池包只有一种
	uint16_t STEP_Z_MAX=	MQTT_PUBLISH_PACK_FRAME_MAX-2;//最内for循环 只上传一段寄存器消息
	uint8_t rtn = 0xFF;

	do{
	
		//每次循环仅执行一次
		x = (Cnt/(STEP_Y_MAX*STEP_Z_MAX))%STEP_X_MAX;
		y = (Cnt/STEP_Y_MAX)%STEP_Y_MAX;
		z = Cnt%STEP_Z_MAX;//上传第1段数据（6100段）
		if(Inv_Pack_Slave[x].mod_reg06100_Pack_each.sn_code)	
		{
#define PACK_GRP_ADDR_START 	MD_INV_ADDR_START
			iot_publish_single_pack_data(SLAVE_PACK_READ_ADDR+x, *cycle_id, z,STEP_Z_MAX);
			ESP_LOGI(TAG, "mqtt publish group pack summary data, group: %d, y = %d,z=%d,cycle_id:%u", PACK_GRP_ADDR_START+x,y,z,*cycle_id);
            if(z == STEP_Z_MAX - 1)// 汇总数据上报 都上报完一轮后，Counter ID加1
            {
                if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据的完整数据包的Counter ID
            }
            send_flag=1;
		}

		if(++Cnt >= (STEP_X_MAX*STEP_Y_MAX*STEP_Z_MAX))
		{
			Cnt=0;
			rtn=0;
			return rtn;
			
		}
		else
		{
			rtn=1;
		}
	
	}
	while(!send_flag);//预判下次不在线，要直接下一步

	
	return rtn;

}
/*
内置电池包d单包数据上报
*/
static uint8_t iot_publish_bms_pack_single_step(uint16_t *cycle_id)
{
	static uint16_t Cnt = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t z = 0;
    uint8_t send_flag=0;

	uint16_t STEP_X_MAX=	DEV_MAIN_NODE_MAX;//最外for循环,
	uint16_t STEP_Y_MAX=	1;//每个逆变器储能系统只有1个内置电池包
	uint16_t STEP_Z_MAX=	MQTT_PUBLISH_PACK_FRAME_MAX-1;//最内for循环  上传2段数据
	uint8_t rtn = 0xFF;

	do{
	
		//每次循环仅执行一次
		x = (Cnt/(STEP_Y_MAX*STEP_Z_MAX))%STEP_X_MAX;
		y = (Cnt/STEP_Z_MAX)%STEP_Y_MAX;
		z = Cnt%STEP_Z_MAX;//上传1、2段数据（6100段、6300段）
		/* 轮询所有PACK节点数据并上报到服务器,每次上传一台PACK设备数据 */
			
		/* 轮询所有逆变节点数据并上报到服务器,每次上传一台逆变设备数据 */
		if (Inv_can[x].pack_data[0].online) 		// 主PACK节点离线不上报数据
		{
			iot_publish_single_pack_data(MD_INV_ADDR_START+x, *cycle_id, z,STEP_Z_MAX);
			ESP_LOGI(TAG, "iot_publish_bms_pack_single_step, group: %d, y = %d,z=%d single cycle_id:%u", PACK_GRP_ADDR_START+x,y,z,*cycle_id);
            if(z == STEP_Z_MAX - 1)// 0到3 4段寄存器 都上报完一轮后，Counter ID加1
            {
                if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据的完整数据包的Counter ID
            }
            send_flag=1;
		}

		if(++Cnt >= (STEP_X_MAX*STEP_Y_MAX*STEP_Z_MAX))
		{
			Cnt=0;
			rtn=0;
			return rtn;
		}
		else
		{
			rtn=1;
		}
	
	}
	while(!send_flag);

	
	return rtn;

}

/*
外置电池包单包信息上报
*/
static uint8_t iot_publish_ext_pack_single_step(uint16_t *cycle_id)
{

    if(!reals.packPoinitNum)//无外置电池包时，跳过该段
        return 0;

    static uint16_t Cnt = 0;
    uint8_t i=0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t z = 0;
    uint8_t send_flag=0;
    uint8_t rtn = 0xFF;

	uint16_t STEP_X_MAX=	reals.packPoinitNum;//最外for循环,外置电池包节点数量
	uint16_t STEP_Y_MAX=	1 ;//只有一个电池包节点
	uint16_t STEP_Z_MAX=	MQTT_PUBLISH_PACK_FRAME_MAX-1;//最内for循环
	
    do
    {
        //每次循环仅执行一次
        x = (Cnt/(STEP_Y_MAX*STEP_Z_MAX))%STEP_X_MAX;
        y = (Cnt/STEP_Z_MAX)%STEP_Y_MAX;
        z= Cnt%STEP_Z_MAX;//上传2段寄存器数据（6100段、6300段）

        //if(	Inv_Pack_Slave[SLAVE_SINGLE_PACK_ADDR+x].mod_reg06100_Pack_each.sn_code)
        if(reals.packPointInfos[x].pointState.state.point_online)
        {
            iot_publish_single_pack_data(SLAVE_SINGLE_PACK_READ_ADDR+x, *cycle_id, z,STEP_Z_MAX);		// 推送该PACK设备数据
            ESP_LOGI(TAG, "iot_publish_ext_pack_single_step, addr: %d, z: %d ,single cycle_id:%u",
                    SLAVE_SINGLE_PACK_READ_ADDR+x, z,*cycle_id);
            send_flag=1;
        }
        if(z == STEP_Z_MAX - 1)// 1到2段寄存器 都上报完一轮后，Counter ID加1，对于从包地址，不做6300、7200汇总的处理
        {
            if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据的完整数据包的Counter ID
        }


        if(++Cnt >= (STEP_X_MAX*STEP_Y_MAX*STEP_Z_MAX))
        {
            Cnt=0;
            rtn=0;
            return rtn;
            
        }
        else
        {
            rtn=1;
        }

    } while (!send_flag);

	return rtn;


}

/*
三维循环改单次执行
return:
0-一个大循环执行完毕； 
1-一个大循环正在执行；
*/
static uint8_t iot_publish_pack_each_step(uint16_t *cycle_id)
{
	static uint16_t Cnt = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t z = 0;

	uint16_t STEP_X_MAX=	DEV_MAIN_NODE_MAX;//最外for循环,
	uint16_t STEP_Y_MAX=	PACK_MAX_NUM ;//
	uint16_t STEP_Z_MAX=	MQTT_PUBLISH_PACK_FRAME_MAX;//最内for循环
	uint8_t rtn = 0xFF;

do{

	//每次循环仅执行一次
	x = (Cnt/(STEP_Y_MAX*STEP_Z_MAX))%STEP_X_MAX;
	y = (Cnt/STEP_Z_MAX)%STEP_Y_MAX;
	z = Cnt%STEP_Z_MAX;
	/* 轮询所有PACK节点数据并上报到服务器,每次上传一台PACK设备数据 */

	
	if (Inv_can[x].pack_data[0].pack_announce.online & (1 << y))	// 检查当前设备是否在线
	{
		iot_publish_pack_data_step(PACK_GRP_IDX_TO_ADDR(x, y), *cycle_id, z,STEP_Z_MAX);		// 推送该PACK设备数据
		ESP_LOGI(TAG, "mqtt publish pack data, main_node: %d, sub_node: %02x, md_addr: %d",
				 x, y, PACK_GRP_IDX_TO_ADDR(x, y));
        if(z == MQTT_PUBLISH_PACK_FRAME_MAX - 3)// 0到1 2段寄存器 都上报完一轮后，Counter ID加1，对于从包地址，不做6300、7200汇总的处理
        {
            if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据的完整数据包的Counter ID
        }
	}

	if(++Cnt >= (STEP_X_MAX*STEP_Y_MAX*STEP_Z_MAX))
	{
		Cnt=0;
		rtn=0;
		return rtn;
		
	}
	else
	{
		rtn=1;
	}

	//每次循环仅执行一次
	x = (Cnt/(STEP_Y_MAX*STEP_Z_MAX))%STEP_X_MAX;
	y = (Cnt/STEP_Z_MAX)%STEP_Y_MAX;
	z = Cnt%STEP_Z_MAX;

}
while(0 == (Inv_can[x].pack_data[0].pack_announce.online & (1 << y)));//预判下次不在线，要直接下一步



	return rtn;


}






/*
三维循环改单次执行
return:
0-一个大循环执行完毕；
1-一个大循环正在执行；
*/
// static uint8_t iot_publish_inv_each_step(uint16_t cycle)
static uint8_t iot_publish_inv_single_step(uint16_t* cycle_id)
{
	static uint16_t Cnt = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t z = 0;
    uint8_t send_flag=0;

	uint16_t STEP_X_MAX=	DEV_MAIN_NODE_MAX;//最外for循环,
	uint16_t STEP_Y_MAX=	INV_MAX_NUM ;//
	uint16_t STEP_Z_MAX=	MQTT_PUBLISH_INV_FRAME_MAX;//最内for循环
	uint8_t rtn = 0xFF;

	do{
	
		//每次循环仅执行一次
		x = (Cnt/(STEP_Y_MAX*STEP_Z_MAX))%STEP_X_MAX;
		y = (Cnt/STEP_Z_MAX)%STEP_Y_MAX;
		z = Cnt%STEP_Z_MAX;
		/* 轮询所有PACK节点数据并上报到服务器,每次上传一台PACK设备数据 */
	
		
		/* 轮询所有逆变节点数据并上报到服务器,每次上传一台逆变设备数据 */
		if (Inv_can[x].inv_data[y].online)								// 检查当前设备是否在线
		{
			//mqtt_inv_node_data_process(x);
			iot_publish_inv_data_step(INV_GRP_IDX_TO_ADDR(x, y), *cycle_id,z,STEP_Z_MAX);		// 推送该INV设备数据
			ESP_LOGI(TAG, "mqtt publish inv data, main_node: %d, sub_node: %02x, md_addr: %d ,z:%d,single cycle_id:%u",
					 x, y, INV_GRP_IDX_TO_ADDR(x, y),z,*cycle_id);
            if(z == STEP_Z_MAX - 1)// 0到10 11段寄存器 都上报完一轮后，Counter ID加1
            {
                if (++(*cycle_id) >= 0xffff) *cycle_id = 1;//周期性上报数据的完整数据包的Counter ID
            }
            send_flag=1;
		}

	
		if(++Cnt >= (STEP_X_MAX*STEP_Y_MAX*STEP_Z_MAX))
		{
			Cnt=0;
			rtn=0;
			return rtn;
			
		}
		else
		{
			rtn=1;
		}
	}
	while(!send_flag);//预判下次不在线，要直接下一步
	
	return rtn;



}

#define  MQTT_REPORT_STEP_BIG	6//
#define  MQTT_COLLECT_REPORT_STEP_BIG	3//
#define  MQTT_SINGLE_REPORT_STEP_BIG	2//
#define  MQTT_REPORT_CYCLE	5//
#define  MQTT_REPORT_IDLE_COOLDOWN_MS  3000

static uint8_t s_mqtt_collect_active = 0;
static uint8_t s_mqtt_single_active = 0;
static uint32_t s_mqtt_report_idle_since = 0;

static void iot_mqtt_period_report_sync(void)
{
    static uint8_t prev_active = 0;
    uint8_t active = s_mqtt_collect_active || s_mqtt_single_active;

    if (prev_active && !active) {
        s_mqtt_report_idle_since = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    prev_active = active;
}

static void iot_mqtt_collect_active_update(uint8_t runflag)
{
    s_mqtt_collect_active = (runflag == 1) ? 1 : 0;
    iot_mqtt_period_report_sync();
}

static void iot_mqtt_single_active_update(uint8_t runflag)
{
    s_mqtt_single_active = (runflag == 1) ? 1 : 0;
    iot_mqtt_period_report_sync();
}

bool iot_mqtt_is_period_reporting(void)
{
    if (s_mqtt_collect_active || s_mqtt_single_active) {
        return true;
    }
    if (s_mqtt_report_idle_since != 0) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if ((now - s_mqtt_report_idle_since) < MQTT_REPORT_IDLE_COOLDOWN_MS) {
            return true;
        }
        s_mqtt_report_idle_since = 0;
    }
    return false;
}

/*------------------------------------------------------------------------
*@Function： iot_mqtt_announce_collect
发送MQTT，周期上报汇总设备
100ms cycle
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
uint8_t iot_mqtt_announce_collect(void)
{
	uint32_t ReprotCycle = 0;//
    static uint8_t sRunflag = 0xff;
    static uint8_t sreport_step = 0;
    static uint32_t pre_time = 0;
	uint8_t rtn = 0xFF;
    static uint16_t cycle=0;


	//ESP_LOGI(TAG,"iot_mqtt_announce_collect sRunflag:%d",sRunflag);
	if((1 == SetData.dev_info_t.on_off.bit._4g_enable)&&(ETH_GET_IP_READY ==reals.Cellular4G_step))//限制流量
	{
		ReprotCycle = SetData.dev_info_t.data_report_cycle;
	}
	else
	{
		ReprotCycle = 60;//60s
	}

		
	if(sRunflag!=1)
	{
        uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
		if(((now_time - pre_time) >= (ReprotCycle*1000))||(sRunflag==0xff))//60*1000ms,帧群之间的间隔时间
		{
			pre_time = now_time;
			sRunflag = 1;
		}	
        //ESP_LOGI(TAG,"iot_mqtt_announce_collect now_time:%lu,pre_time:%lu",now_time,pre_time);
		iot_mqtt_collect_active_update(sRunflag);
		return sRunflag;
	}

	
    if ((1 != SetData.dev_info_t.on_off.bit.wifi_enable)
        ||(0 == reals.wifi_connect_ap)
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        ||(0 == reals.wifi_mesh_root)
#endif
        ||(0 == login_info.mqtt_is_login)// wifi未连接或mqtt未登录
        ||(bind_dev_list.need_bind) 
        ) 	    	
    {
        //ESP_LOGI(TAG,"iot_mqtt_announce_collect wifi_enable:%d,wifi_connect_ap:%d,wifi_mesh_root:%d,mqtt_is_login:%d,need_bind:%d",SetData.dev_info_t.on_off.bit.wifi_enable,reals.wifi_connect_ap,reals.wifi_mesh_root,login_info.mqtt_is_login,bind_dev_list.need_bind);
        iot_mqtt_collect_active_update(sRunflag);
        return 0; 
    }
	
    switch(sreport_step)
    {
        case 0:
            rtn =iot_publish_Version(&cycle);/* 锁定绑定设备上传的数据,避免上传数据过程中设备发生变化从而导致上传的数据非绑定的设备数据 */
        break;     
        case 1:
            rtn =iot_publish_summary_A(&cycle);// 逆变汇总 00
        break;    
        case 2:
            rtn =iot_publish_summary_B(&cycle);// pack汇总 00
        break;   
        default :
            sreport_step = 0;
            sRunflag = 0;
        iot_mqtt_collect_active_update(sRunflag);
        return sRunflag; 
    }
    ESP_LOGI(TAG,"iot_mqtt_announce_collect sreport_step:%d",sreport_step);
    if(0 == rtn)
    {
        if(++sreport_step > MQTT_COLLECT_REPORT_STEP_BIG)
        {
            sreport_step=0;
            sRunflag =0;	
        }
    }
    
    iot_mqtt_collect_active_update(sRunflag);
    return sRunflag;
}

/*------------------------------------------------------------------------
*@Function： iot_mqtt_announce_collect
发送MQTT，周期上报汇总设备
100ms cycle
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
uint8_t iot_mqtt_announce_single(void)
{
	uint16_t ReprotCycle = 0;//

    static uint8_t sRunflag = 0xff;//10s
    static uint8_t sreport_step = 0;
    static uint32_t pre_time = 0;
	uint8_t rtn = 0xFF;
    static uint16_t cycle=0;

    //ESP_LOGI(TAG,"iot_mqtt_announce_single sRunflag:%d",sRunflag);
	ReprotCycle = 120;//120s
		
	if(sRunflag!=1)
	{
        uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
		if(((now_time - pre_time) >= (ReprotCycle*1000))||((sRunflag==0xff)))//120*1000ms,帧群之间的间隔时间
		{
			pre_time = now_time;
			sRunflag = 1;
		}	
		iot_mqtt_single_active_update(sRunflag);
		return sRunflag;
	}

	
    if ((1 != SetData.dev_info_t.on_off.bit.wifi_enable)
        ||(0 == reals.wifi_connect_ap)
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        ||(0 == reals.wifi_mesh_root)
#endif
        ||(0 == login_info.mqtt_is_login)// wifi未连接或mqtt未登录
        ||(bind_dev_list.need_bind) 
        ) 	    	
    {
        ESP_LOGI(TAG,"iot_mqtt_announce_single wifi_enable:%d,wifi_connect_ap:%d,mqtt_is_login:%d,need_bind:%d",SetData.dev_info_t.on_off.bit.wifi_enable,reals.wifi_connect_ap,login_info.mqtt_is_login,bind_dev_list.need_bind);
        iot_mqtt_single_active_update(sRunflag);
        return 0;
    }
	
    switch(sreport_step)
    {
        case 0:
            rtn =iot_publish_inv_single_step(&cycle);// 单台逆变 01~03
        break;     
        case 1:
            rtn =iot_publish_bms_pack_single_step(&cycle);// 内置电池包单包数据上报	1~3
        break;    
        case 2:
            rtn =iot_publish_ext_pack_single_step(&cycle);// 单台外置pack 40~69
        break;   
        default :
            sreport_step = 0;
            sRunflag =0;	
        iot_mqtt_single_active_update(sRunflag);
        return sRunflag;
    }
    ESP_LOGI(TAG,"iot_mqtt_announce_single sreport_step:%d",sreport_step);
    if(0 == rtn)
    {
        if(++sreport_step > MQTT_SINGLE_REPORT_STEP_BIG)
        {
            sreport_step=0;
            sRunflag =0;	
        }
    }
    
    iot_mqtt_single_active_update(sRunflag);
    return sRunflag;
}

#if 0
/*------------------------------------------------------------------------
*@Function： iot_mqtt_announce
发送MQTT，周期上报各个设备信息，子设备和汇总设备
从独立线程改为 1s周期，合并，精简线程RAM
1s cycle
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
 void iot_mqtt_announce(void)//(void * pvParameters) static
{
	uint32_t ReprotCycle = 0;//

    // static uint16_t cycle = 0;
    static uint16_t sRunflag = 0;//10s

    static uint8_t sreport_step = 0;
    static uint8_t sreport_period = 0;
	uint8_t rtn = 0xFF;


	
	if((1 == SetData.dev_info_t.on_off.bit._4g_enable)&&(ETH_GET_IP_READY ==reals.Cellular4G_step))//限制流量
	{
		ReprotCycle = SetData.dev_info_t.data_report_cycle;
	}
	else
	{
		ReprotCycle = 20;//20s
	}

		
	if(0 == sRunflag)
	{
		if(++sreport_period >= (ReprotCycle*10))//200*100ms=20s,帧群之间的间隔时间
		{
			sreport_period=0;
			sRunflag =1;

		}	
//		ESP_LOGI(TAG, "windy in iot_mqtt_announce:	AAAAAAAAAAA ,sreport_period=%d,now_time=%ld,cyclee=%ld",sreport_period,now_time,cycle);
	
		return;
	}

	
        //ESP_LOGE(TAG, "windy iot_mqtt_announce: wifi_connect_ap: %0d, mqtt_is_login: %d",reals.wifi_connect_ap,login_info.mqtt_is_login);	 
         if ((1 != SetData.dev_info_t.on_off.bit.wifi_enable)
		 	||(0 == reals.wifi_connect_ap)
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
		 	||(0 == reals.wifi_mesh_root)
#endif
		 	||(0 == login_info.mqtt_is_login) // wifi未连接或mqtt未登录
		 	) 	    	
    	{
    		//ESP_LOGE(TAG, "windy iot_mqtt_announce: wifi_connect_ap: %0d, mqtt_is_login: %d ,return=%d",reals.wifi_connect_ap,login_info.mqtt_is_login,is_device_online()); 
    		return; 
    	}

        // bind_dev_list.dev_nums = reals.online_Inv_num;//testwx mqtt
		// ESP_LOGE(TAG, "!!!dev_nums: %d\n", bind_dev_list.dev_nums);
		iot_bind_check();
		//tcp_bind_dev(); 														// 通过tcp方式绑定设备(帧头-4)
		mqtt_bind_dev(); 													   // 通过mqtt方式绑定设备(帧头-10)

		if(bind_dev_list.need_bind)
		{
			ESP_LOGE(TAG, "tcp bind is not complete!\n");
//			vTaskDelay(pdMS_TO_TICKS(2000));
			return; 														// 绑定未完成不主动上报数据
		}
	
	
    
		if(0 == sreport_step)
		{
			/* 锁定绑定设备上传的数据,避免上传数据过程中设备发生变化从而导致上传的数据非绑定的设备数据 */
            rtn =iot_publish_Version();
		}
		// else if((1 == sreport_step) && (1 == reals.online_ACHUB_num))//achub在线时认为是并机，走二代IOT，00表示汇总；achub不在线时认为是单机，走一代IOT，不发00汇总
        else if(1 == sreport_step)
		{
            rtn =iot_publish_summary_A();// 逆变汇总 00
	
		}
		// else if((2 == sreport_step) && (1 == reals.online_ACHUB_num))
        else if(2 == sreport_step)
		{
            rtn =iot_publish_summary_B();// pack汇总 00
	
		}	
		else if(3 == sreport_step)
		{
            rtn =iot_publish_inv_each_step();// 单台逆变 01~03
		}
		// else if((4 == sreport_step) && (1 == reals.online_ACHUB_num))
        else if(4 == sreport_step)
		{
            rtn =iot_publish_bms_pack_single_step();// 内置电池包单包数据上报	1~3						// 推送电池汇总数据
		}	
		else if(5 == sreport_step)
		{
            rtn =iot_publish_ext_pack_group_step();// 外置电池包同类型数据上报 30~39
		}
        else if(6 == sreport_step)
		{
            rtn =iot_publish_ext_pack_single_step();// 单台外置pack 40~69
		}

		if(0 == rtn)
		{
			if(++sreport_step > MQTT_REPORT_STEP_BIG)
			{
				sreport_step=0;
				sRunflag =0;	
			}
		}


}
#endif
static int Mqtt_Active_Report_Queue_Report(void);
/*------------------------------------------------------------------------------
 Function: iot_mqtt_actively_report_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      mqtt上报任务
  * @param[in]  void  
  * @param[out] None
  * @return     void i
  */
 void iot_mqtt_actively_report_task (void)
 {

    static uint8_t mqtt_send = 0;	
    static uint32_t pre_time = 0;
    static uint8_t ts_u16Cnts=0xff;
     //单次循环，只向服务器发送1个函数内容
    if(iot_mqtt_dev_event_ann())
    {}
    else if(Modbus_Self_21000_Report_MQTT())//mqtt绑定帧主动上报
    {
//				ESP_LOGI(TAG, "Modbus_Self_Report_MQTT	in ");
    }
    else if(udt_mqtt_Report_to_Cloud())
    {
//				ESP_LOGI(TAG, "udt_mqtt_Report_to_Cloud  in ");
    }//http_client数据通道响应
        /*设备特殊主动上报*/
    else if (Mqtt_Active_Report_Queue_Report() > 0) {
    }
    else
    {	
        if(ts_u16Cnts != reals.Timer_1sCount)							  
		{
			ts_u16Cnts = reals.Timer_1sCount;
            iot_mqtt_update();
            if(reals.Addr_can_master==1)
            {
                iot_bind_check();
            }			 

            if ((login_info.mqtt_is_login)&&(bind_dev_list.need_bind))
            {
                mqtt_bind_dev(); 
            }
		}
        else if (mqtt_send == 0)
        {
            if(iot_mqtt_announce_collect()){}
            else
            {
            /*空闲周期上报*/
            //iot_mqtt_announce();
            uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if((now_time - pre_time) >= (500))        // 500ms
            {
                pre_time = now_time;
                iot_mqtt_announce_single();       // mqtt单设备周期上报
            }
            }

            mqtt_send = 1;
        }
        else if (mqtt_send == 1)
        {
        
#ifdef  TCP_SERVER2_ENABLE	
//			ESP_LOGI(TAG, "Server2_MQTT_Send_Public_Data	in ");
            /*调试用服务器，不使能*/
            Server2_MQTT_Send_Public_Data();
#endif	
            mqtt_send = 0;

        }
    }
 }

// 生成32位随机数字符串
void generate_random_string(char *str, size_t len) 
{
    for (int i = 0; i < len; i++) 
    {
        str[i] = esp_random() % 26 + 'A';
    }
    str[len-1] = '\0'; // 添加字符串结尾的零
}

// SHA256加密函数
void sha256_Encry(const char *input, char *output) 
{
    mbedtls_sha256_context sha256_ctx;
    unsigned char hash[32]; // SHA-256的输出是32字节
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0); // 初始化SHA256上下文
    mbedtls_sha256_update(&sha256_ctx, (const unsigned char *)input, strlen(input));
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    // 将32字节的hash转换为64字符的十六进制字符串
    // snprintf(output, 65, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    //           hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
    //           hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15],
    //           hash[16], hash[17], hash[18], hash[19], hash[20], hash[21], hash[22], hash[23],
    //           hash[24], hash[25], hash[26], hash[27], hash[28], hash[29], hash[30], hash[31]);
    snprintf(output, 70, "Sign %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
              hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
              hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15],
              hash[16], hash[17], hash[18], hash[19], hash[20], hash[21], hash[22], hash[23],
              hash[24], hash[25], hash[26], hash[27], hash[28], hash[29], hash[30], hash[31]);
    // sprintf(output,"%s",hash);
}

int set_http_header(esp_http_client_handle_t client)
{
    time_t localTime = time(NULL);    // 获取当前本地时间（秒级时间戳）
    uint64_t safety_code =iot_factory.safe_code;;// g_dev_info.safe_code;
    char localTime_str[32]={0};
    char Safe_Code_str[32]={0};
    char Sn_Str[32]={0};
    sprintf(localTime_str,"%llu", (uint64_t)localTime);
    sprintf(Safe_Code_str,"%llu", safety_code);
    sprintf(Sn_Str,"%s%lld", iot_factory.iot_type, iot_factory.iot_sn); // 第二代
//    sprintf(Sn_Str,"%s%d", inv_factory.inv_type, inv_factory.inv_sn); // 第一代

   ESP_LOGI(TAG,"localTime:%llu", (uint64_t)localTime);
   ESP_LOGI(TAG,"localTime_str:%s", localTime_str);
   ESP_LOGI(TAG,"sn_str:%s", Sn_Str);    

    //ETag是32位随机字符串，每次由客户端自动生成
    char ETag[33]={0};  
    generate_random_string(ETag,33);
   ESP_LOGI(TAG,"Etag:%s",ETag);
    //Authorization，需要由请求头使用sha256加密生成
    char *Encry_Str = NULL;
    char *Safe_Code_Sin_str = NULL;
//    Encry_Str = (char *)heap_caps_malloc(256, MALLOC_CAP_SPIRA_SIZE | MALLOC_CAP_8BIT);
    Encry_Str = (char *)heap_caps_malloc(256, MALLOC_CAP_SPIRA_SIZE);
    if (Encry_Str != NULL) {
        memset(Encry_Str, 0, 256);
    }
    // Encry_Str = (char *)calloc(256, 1);
	if(Encry_Str == NULL){
       ESP_LOGE(TAG,"Encry_Str calloc faild");
        return -1;
    }
//    Safe_Code_Sin_str = (char *)heap_caps_malloc(256, MALLOC_CAP_SPIRA_SIZE | MALLOC_CAP_8BIT);
    // Safe_Code_Sin_str = (char *)calloc(256, 1);
    Safe_Code_Sin_str = (char *)heap_caps_malloc(256, MALLOC_CAP_SPIRA_SIZE);
    if (Safe_Code_Sin_str != NULL) {
        memset(Safe_Code_Sin_str, 0, 256);
    }

	if(Safe_Code_Sin_str == NULL){
       ESP_LOGE(TAG,"Safe_Code_Sin_str calloc faild");
        if(Encry_Str != NULL){
            free(Encry_Str);
        }
        return -1;
    }

    sprintf(Encry_Str,"Date=%s&ETag=%s&X-Att-DeviceId=%s&safetyCode=%s&x-app-key=%s&x-os=%s",
            localTime_str,ETag,Sn_Str,Safe_Code_str,"18C2BC21BE8F79115700FFBF91B","hdevice");

    sha256_Encry(Encry_Str,Safe_Code_Sin_str);
   ESP_LOGI(TAG,"SHA256('%s') = %s\n", Encry_Str, Safe_Code_Sin_str);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Date", localTime_str);//Add wx
    esp_http_client_set_header(client, "ETag", ETag);//Add wx
    esp_http_client_set_header(client, "x-os", "hdevice");
    esp_http_client_set_header(client, "x-app-key", "18C2BC21BE8F79115700FFBF91B");
    esp_http_client_set_header(client, "X-Att-DeviceId", Sn_Str);//Add wx
    esp_http_client_set_header(client, "Authorization", Safe_Code_Sin_str);//Add wx
    
    if(Encry_Str != NULL){
        free(Encry_Str);
    }
    if(Safe_Code_Sin_str != NULL){
        free(Safe_Code_Sin_str);
    }
    return 0;
}

//请求证书更新获取新序列号
static uint32_t http_iot_update_cert(int sock_fd, const char *iot_type, login_info_t login_info) 
{
    if(sock_fd<0)
    {
        ESP_LOGW(TAG,"http_iot_update_cert sock_fd err");
    }

    if(iot_type == NULL)
    {
        ESP_LOGW(TAG,"http_iot_update_cert iot_type err");
    }

    if (login_info.iot_sn == 0) {
        ESP_LOGW(TAG,"http_iot_update_cert iot_sn:%lld",login_info.iot_sn);
        return 0;
    }

    int16_t ret = 0;
    int16_t len=0;
    int16_t success = 0;
    uint64_t SN = login_info.iot_sn;
    uint64_t Safe_Code = login_info.safetyCode;
    char *post_data=NULL;

    /***** 密文提取 *****/
    char sys_info_all[33]={0};// 64 32
    sprintf(sys_info_all,"IOT%llu,%llu",SN,Safe_Code);//sys_info
    sys_info_all[32] = '\0';//防止越界

    int16_t sys_info_len = strlen(sys_info_all);//sys_info
    ESP_LOGI(TAG,"sys_info_all:%s,sys_info_len=%d",sys_info_all,sys_info_len);//sys_info

    //使用设备的X.509公钥证书对设备身份信息deviceIdentity 进行加密，得到加密后的设备身份信息ciphertext
    unsigned char plaintext[33]= {0};// A103 = {0} 117 200 32
    // unsigned char ciphertext[471]= {0};  // A103 = {0} 128 256->16_hex 改用堆
	unsigned char *ciphertext = NULL;
//    ciphertext = (unsigned char*)calloc(471, sizeof(char));
    ciphertext = (unsigned char*)heap_caps_malloc((uint16_t)471*sizeof(char), MALLOC_CAP_SPIRAM);
	
    if(ciphertext == NULL)
    {
        mbedtls_printf( "ciphertext calloc faild\n");
        return 0;
    }

    memcpy(plaintext, sys_info_all, sys_info_len);//M msg //sys_info
    plaintext[sys_info_len] = '\0';//防止越界  +1
    if((sys_info_len > 117) || (sys_info_len < 0))
    {
        perror("msg length must be less than 116 bytes!\n");
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }
        return 0;
    }

    ret = cer_ras_encrypt(plaintext,sys_info_len,ciphertext);//M904503 sys_info

    memset(plaintext,0,sizeof(plaintext));//testwx 对原明文区域初始化重新赋初值
    memcpy(plaintext, sys_info_all, sys_info_len);//M msg //sys_info

    if(ret == 0)
    {
        ESP_LOGW(TAG,"priv_rsa_sign failed,ret=%d",ret);
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }

        return 0;
    }
    ESP_LOGI(TAG,"ciphertext:%s",ciphertext);
    /***** 密文提取 *****/


    /***** 数字签名提取 *****/
    //使用设备的私钥对设备身份信息deviceIdentity 进行数字签名，得到设备身份信息的签名指纹signature
    // static char signature[471] = {0};//256 350 235 小于330~350会溢出报错重启 235->16_hex 改用堆
	unsigned char *signature = NULL;
//    signature = (unsigned char*)calloc(471, sizeof(char));
    signature = (unsigned char*)heap_caps_malloc((uint16_t)471*sizeof(char), MALLOC_CAP_SPIRAM);
    if(signature == NULL)
    {
        ESP_LOGE(TAG,"signature calloc faild\n");
        goto exit;
    }

    ret = priv_rsa_sign(plaintext, sys_info_len, signature);
    if(ret == 0)
    {
        ESP_LOGE(TAG,"priv_rsa_sign failed,ret=%d",ret);
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }
        if(signature != NULL)
        {
            free(signature);
        }

        return 0;
    }
    ESP_LOGI(TAG,"signature:%s",signature);
    /***** 数字签名提取 *****/


    /***** 序列号提取 *****/
    //已过期的X509数字证书的序列号
    char x509CertSn[33];//32
    len = cer_ras_get_serial_number(x509CertSn);
    
    if(!len)
    {
        ESP_LOGE(TAG,"cer_ras_get_serial_number failed,len=%d",len);
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }
        if(signature != NULL)
        {
            free(signature);
        }

        return 0;
    }
    else
    {
        x509CertSn[len]='\0';//防止越界
        len++;
        char x509CertSn_str[33]={0};//32 100
        sprintf(x509CertSn_str,"%s",x509CertSn);
        x509CertSn_str[32] = '\0';//防止越界
        ESP_LOGI(TAG,"cer_ras_get_serial_number:len=%d,x509CertSn_str:%s",len,x509CertSn_str);
        len = (len > 0) ? len:15; //9
    }
    /***** 序列号提取 *****/
    

    /***** POST Request body data *****/
//    post_data = (char*)calloc(1024, sizeof(char));//512 2048
    post_data = (char*)heap_caps_malloc((uint16_t)1024*sizeof(char), MALLOC_CAP_SPIRAM);
    
    if(post_data == NULL)
    {
        ESP_LOGE(TAG,"post_data calloc faild\n");
        goto exit;
    }

    if((ciphertext != NULL)&&(signature != NULL))
    {
        ret = sprintf(post_data,"{\"x509CertSn\":\"%s\",\"ciphertext\":\"%s\",\"signature\":\"%s\"}", x509CertSn,ciphertext,signature);//035cf17fc6802000 035CF17FC6802000
        free(ciphertext);
        free(signature);
        ciphertext = NULL;
        signature = NULL;
    }
    else
    {
        ESP_LOGE(TAG,"ciphertext or signature calloc faild\n");
        goto exit;
    }
    
    len = ret;
    ESP_LOGI(TAG,"post_data_update:%s, len:%d",post_data, len);
    if(len < 0)
    {
        ESP_LOGW(TAG,"Error len=%d",len);
    }
    /***** POST Request body data *****/

    int content_length = 0;
    //本地时间转UTC+0时间
    time_t localTime = time(NULL);    // 获取当前本地时间（秒级时间戳）
    // time_t local_utc0Time = localTimeToUTC0(localTime);  // 转换为UTC+0时间

    //http请求包
    static const char *REQUEST = CERT_HTTP_URL;

    // 初始化HTTP客户端
    esp_http_client_config_t config = {
        .url = REQUEST,
        .method = HTTP_METHOD_POST,
    };
    // 创建HTTP客户端实例
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG,"Failed to initialize HTTP client\n");
        goto exit;
    }
    
    char localTime_str[32]={0};
    char Safe_Code_str[32]={0};
    char Sn_Str[32]={0};

    sprintf(localTime_str,"%llu",(uint64_t)localTime);//localTime    local_utc0Time
    sprintf(Safe_Code_str,"%llu",Safe_Code);
    sprintf(Sn_Str,"IOT%llu",SN);
    
    ESP_LOGI(TAG,"localTime:%llu\n",(uint64_t)localTime);//local_utc0Time
    ESP_LOGI(TAG,"localTime_str:%s\n",localTime_str);
    ESP_LOGI(TAG,"sn_str:%s\n",Sn_Str);

    //ETag是32位随机字符串，每次由客户端自动生成
    char ETag[33]={0};  
    generate_random_string(ETag,33);//生成32位
    ESP_LOGI(TAG,"Etag:%s\n",ETag);
    // sprintf(ETag,"%s","PLIBDCMCSTRAVBEASDBPAIOOCNGTMLZK");//test
   
    //Authorization，需要由请求头使用sha256加密生成
    char Encry_Str[256]={0};
    char Safe_Code_Sin_str[256]={0};
    sprintf(Encry_Str,"Date=%s&ETag=%s&X-Att-DeviceId=%s&safetyCode=%s&x-app-key=%s&x-os=%s",
            localTime_str,ETag,Sn_Str,Safe_Code_str,"18C2BC21BE8F79115700FFBF91B","hdevice");

    sha256_Encry(Encry_Str,Safe_Code_Sin_str);
    ESP_LOGI(TAG,"SHA256('%s') = %s\n", Encry_Str, Safe_Code_Sin_str);

    esp_http_client_set_url(client, REQUEST);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Date", localTime_str);//Add 
    esp_http_client_set_header(client, "ETag", ETag);
    esp_http_client_set_header(client, "x-os", "hdevice");
    esp_http_client_set_header(client, "x-app-key", "18C2BC21BE8F79115700FFBF91B");
    esp_http_client_set_header(client, "X-Att-DeviceId", Sn_Str);
    esp_http_client_set_header(client, "Authorization", Safe_Code_Sin_str);
    esp_http_client_set_authtype(client,HTTP_AUTH_TYPE_NONE);

    ESP_LOGI(TAG,"http_iot_update_cert:len:%d",len);
    esp_err_t err = esp_http_client_open(client, len);//M strlen(post_data)
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG,"1_post_data_update:%s, len:%d",post_data, len);
        int wlen = esp_http_client_write(client, (const char *)post_data, len);//M strlen(post_data)
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write failed");
        }
        content_length = esp_http_client_fetch_headers(client);
        if(content_length < 0) 
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        }
        else 
        {
            char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request MAX_HTTP_OUTPUT_BUFFER
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
//                ESP_LOGI(TAG, "HTTP POST Status = %ld, content_length = %ld,data_read=%lld",
//                        esp_http_client_get_status_code(client),
//                        esp_http_client_get_content_length(client),
//                        data_read);
                ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
                success = cjson_to_serial_info(output_buffer);
                ESP_LOGI(TAG,"cjson_to_serial_info:success=%d",success);
          
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }

    if(client != NULL)
    {
        esp_http_client_cleanup(client); 
    }
exit:   
    if(ciphertext != NULL)
    {
        free(ciphertext);
    }
    if(signature != NULL)
    {
        free(signature);
    }
    if(post_data != NULL)
    {
        free(post_data);
    }

    return success;
}

//获取证书序列号
static uint32_t http_get_cert_sn(login_info_t login_info)
{
    if ((login_info.iot_sn == 0)) {
        ESP_LOGW(TAG,",iot_type:%s,iot_sn:%lld", login_info.iot_type, login_info.iot_sn);
        return 0;
    }

    int16_t ret = 0;
    int16_t len=0;
    int16_t success = 0;
    uint64_t SN = login_info.iot_sn;
    uint64_t Safe_Code = login_info.safetyCode;

    /***** 密文提取 *****/
    char sys_info_all[33]={0};// 64 32
    sprintf(sys_info_all,"IOT%llu,%llu",SN,Safe_Code);//sys_info
    sys_info_all[32] = '\0';//防止越界

    int16_t sys_info_len = strlen(sys_info_all);//sys_info
    ESP_LOGI(TAG,"sys_info_all:%s,sys_info_len=%d",sys_info_all,sys_info_len);//sys_info

    int content_length = 0;
    //本地时间转UTC+0时间
    time_t localTime = time(NULL);    // 获取当前本地时间（秒级时间戳）
    // time_t local_utc0Time = localTimeToUTC0(localTime);  // 转换为UTC+0时间

    //http请求包
    static const char *REQUEST = CERT_HTTP_URL_FIRST;

    // 初始化HTTP客户端
    esp_http_client_config_t config = {
        .url = REQUEST,
        .method = HTTP_METHOD_POST,
    };
    // 创建HTTP客户端实例
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG,"Failed to initialize HTTP client\n");
        goto exit;
    }
    
    char localTime_str[32]={0};
    char Safe_Code_str[32]={0};
    char Sn_Str[32]={0};

    sprintf(localTime_str,"%llu",(uint64_t)localTime);//localTime    local_utc0Time
    sprintf(Safe_Code_str,"%llu",Safe_Code);
    sprintf(Sn_Str,"IOT%llu",SN);
    
    ESP_LOGI(TAG,"localTime:%llu\n",(uint64_t)localTime);//local_utc0Time
    ESP_LOGI(TAG,"localTime_str:%s\n",localTime_str);
    ESP_LOGI(TAG,"sn_str:%s\n",Sn_Str);

    //ETag是32位随机字符串，每次由客户端自动生成
    char ETag[33]={0};  
    generate_random_string(ETag,33);//生成32位
    ESP_LOGI(TAG,"Etag:%s\n",ETag);
    // sprintf(ETag,"%s","PLIBDCMCSTRAVBEASDBPAIOOCNGTMLZK");//test
   
    //Authorization，需要由请求头使用sha256加密生成
    char Encry_Str[256]={0};
    char Safe_Code_Sin_str[256]={0};
    sprintf(Encry_Str,"Date=%s&ETag=%s&X-Att-DeviceId=%s&safetyCode=%s&x-app-key=%s&x-os=%s",
            localTime_str,ETag,Sn_Str,Safe_Code_str,"18C2BC21BE8F79115700FFBF91B","hdevice");

    sha256_Encry(Encry_Str,Safe_Code_Sin_str);
    ESP_LOGI(TAG,"SHA256('%s') = %s\n", Encry_Str, Safe_Code_Sin_str);

    esp_http_client_set_url(client, REQUEST);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Date", localTime_str);//Add 
    esp_http_client_set_header(client, "ETag", ETag);
    esp_http_client_set_header(client, "x-os", "hdevice");
    esp_http_client_set_header(client, "x-app-key", "18C2BC21BE8F79115700FFBF91B");
    esp_http_client_set_header(client, "X-Att-DeviceId", Sn_Str);
    esp_http_client_set_header(client, "Authorization", Safe_Code_Sin_str);
    esp_http_client_set_authtype(client,HTTP_AUTH_TYPE_NONE);

    ESP_LOGI(TAG,"http_iot_update_cert:len:%d", len);
    esp_err_t err = esp_http_client_open(client, len);//M strlen(post_data)
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        // ESP_LOGI(TAG,"1_post_data_update:%s, len:%d", post_data, len);
        int wlen = esp_http_client_write(client, NULL, 0);//M strlen(post_data)
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write failed");
        }
        content_length = esp_http_client_fetch_headers(client);
        if(content_length < 0) 
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        }
        else 
        {
            char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request MAX_HTTP_OUTPUT_BUFFER
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
            //    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d,data_read=%d",
            //            esp_http_client_get_status_code(client),
            //            esp_http_client_get_content_length(client),
            //            data_read);
                ESP_LOGI(TAG, "http get cert sn:%s", output_buffer);
                ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
                success = json_get_down_cert_info(output_buffer);
                ESP_LOGI(TAG,"json_get_down_cert_info:success=%d",success);
          
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }

    if(client != NULL)
    {
        esp_http_client_cleanup(client); 
    }
exit:

    return success;
}

/**
 * @brief 检测当前IoT的证书是否存在
 * @param login_info MQTTS登录信息
 * @return 0:不存在(需要下载)，1:存在
 */
static uint32_t http_iot_cert_exist(login_info_t login_info)
{
    if (login_info.iot_sn == 0) {
        ESP_LOGW(TAG,"iot_type:%s,iot_sn:%lld", login_info.iot_type, login_info.iot_sn);
        return 0;
    }

    if (NULL == iot_cert_ptr || 0 == iot_cert_ptr_len || NULL == private_key_ptr || 0 == private_key_ptr_len)
    {
        ESP_LOGW(TAG, "mqtts cert is null, need to download");
        return 0;
    }

    int16_t ret = 0;
    int16_t len=0;
    int16_t success = 0;
    uint64_t SN = login_info.iot_sn;
    uint64_t Safe_Code = login_info.safetyCode;

    /***** 密文提取 *****/
    char sys_info_all[33] = {0};// 64 32
    sprintf(sys_info_all,"IOT%llu", SN);//sys_info
    sys_info_all[32] = '\0';//防止越界

    int16_t sys_info_len = strlen(sys_info_all);//sys_info
    ESP_LOGI(TAG,"sys_info_all:%s, sys_info_len=%d", sys_info_all, sys_info_len);//sys_info

    /* 提取证书CommonName, 与当前MQTT登录信息对比，不一致则需要下载 */
    char common_name[33] = {0};
    len = cer_ras_get_common_name(common_name, sizeof(common_name));
    if(!len)
    {
        ESP_LOGE(TAG,"cer_ras_get_common_name failed,len=%d",len);

        return 0;
    }
    else
    {
        common_name[len]='\0';//防止越界
        len++;
        char common_name_str[33]={0};//32 100
        sprintf(common_name_str,"%s",common_name);
        common_name_str[32] = '\0';//防止越界
        ESP_LOGI(TAG,"common_name len=%d, strlen(common_name_str):%d, common_name_str:%s", len, strlen(common_name_str), common_name_str);
        len = (len > 0) ? len:15;

        /* 检测当前证书是否为当前MQTTS登录所使用的IOT */
        if (sys_info_len == strlen(common_name_str) && 0 == memcmp(sys_info_all, common_name, sys_info_len))
        {
            success = 1;
        }
    }

    return success;
}

//json数据解析函数
int16_t cjson_to_serial_info(char *text)
{
    int16_t success=0;
    //截取有效json
    char *index=strchr(text,'{');
    strcpy(text,index);
    ESP_LOGI(TAG,"text=%s",text);
    cJSON *root = cJSON_Parse(text);

    uint32_t serial=0;
    char serial_number[33];//32 testwx 空字符在33位
    if(root != NULL)
    {
        char *sys_info = cJSON_Print(root);  
        ESP_LOGW(TAG,"sys_info:%s",sys_info);

        int msgCode = cJSON_GetObjectItem(root, "msgCode")->valueint;
        cJSON* string = cJSON_GetObjectItemCaseSensitive(root, "data");//cJSON_GetObjectItem          
        if(NULL == string)
        {
            ESP_LOGE(TAG, "json not get data");
            goto exit;
        }

        const char *str_value = cJSON_GetStringValue(string); // 获取字符串值
        if(NULL == str_value)
        {
            ESP_LOGE(TAG, "json not get data value");
            goto exit;
        }

        size_t str_len = strnlen(str_value, sizeof(serial_number) - 1); // 使用strnlen确保不会读取超过serial_number大小 

        if (str_len < sizeof(serial_number)) 
        {  
            // 安全地复制字符串到serial_number数组  
            strncpy(serial_number, str_value, sizeof(serial_number) - 1); // 减去1是为了给字符串末尾的空字符'\0'留出空间  
            serial_number[str_len] = '\0'; // 确保字符串以空字符结尾  
            ESP_LOGI(TAG,"Serial number: %s\n", serial_number);  
        } 
        else 
        {  
            // 字符串太长，无法放入serial_number数组  
            printf("Error: Serial number is too long to fit in the array.\n");  
        }  
        ESP_LOGI(TAG,"msgCode:%d,data:%s",msgCode,serial_number);// testwx

        if(msgCode == 0)
        {
            memcpy(serial_data,serial_number,sizeof(serial_number));
            serial_flag = 1;
            success = 1;
        }
        else
        {
            serial_flag = 0;
        }
    }
exit:
    // ESP_LOGI(TAG,"serial_number:%s,serial_data:%s",serial_number,serial_data);
    if (NULL != root)
    {
        cJSON_Delete(root);
        root = NULL;
    }
    return success;
}

//json数据解析函数
int16_t json_get_down_cert_info(char *text)
{
    int16_t success=0;

    //截取有效json
    char *index=strchr(text,'{');
    if (NULL == index)
    {
        ESP_LOGI(TAG, " json_get_down_cert_info text error:%s", text);
        return 0;
    }

    strcpy(text,index);
    ESP_LOGI(TAG,"text=%s",text);
    cJSON *root = cJSON_Parse(text);

    uint32_t serial=0;
    char serial_number[33];//32 testwx 空字符在33位
    if(root != NULL)
    {
        char *sys_info = cJSON_Print(root);  
        ESP_LOGW(TAG,"sys_info:%s",sys_info);

        int msgCode = cJSON_GetObjectItem(root, "msgCode")->valueint;
        cJSON* string = cJSON_GetObjectItemCaseSensitive(root, "data");//cJSON_GetObjectItem          
        if(NULL == string)
        {
            ESP_LOGE(TAG, "json not get data");
            goto exit;
        }

        const char *str_value = cJSON_GetStringValue(string); // 获取字符串值
        if(NULL == str_value)
        {
            ESP_LOGE(TAG, "json not get data value");
            goto exit;
        }

        size_t str_len = strnlen(str_value, sizeof(serial_number) - 1); // 使用strnlen确保不会读取超过serial_number大小 

        if (str_len < sizeof(serial_number)) 
        {  
            // 安全地复制字符串到serial_number数组  
            strncpy(serial_number, str_value, sizeof(serial_number) - 1); // 减去1是为了给字符串末尾的空字符'\0'留出空间  
            serial_number[str_len] = '\0'; // 确保字符串以空字符结尾  
            ESP_LOGI(TAG,"Serial number: %s\n", serial_number);  
        } 
        else 
        {  
            // 字符串太长，无法放入serial_number数组  
            printf("Error: Serial number is too long to fit in the array.\n");  
        }

        if(msgCode == 0)
        {
            memcpy(serial_data, serial_number,sizeof(serial_number));
            serial_flag = 1;
            success = 1;
        }
        else
        {
            serial_flag = 0;
        }
    }
exit:
    // ESP_LOGI(TAG,"serial_number:%s,serial_data:%s",serial_number,serial_data);
    if (NULL != root)
    {
        cJSON_Delete(root);
        root = NULL;
    }
    return success;
}

//下载新证书函数
static uint32_t http_iot_download_cert(int sock_fd, const char *iot_type, login_info_t login_info) 
{
    if(sock_fd<0)
    {
        ESP_LOGW(TAG,"http_iot_download_cert sock_fd err"); 
        return 0;
    }
    if(iot_type == NULL) 
    {
        ESP_LOGW(TAG,"http_iot_download_cert iot_type null");
        return 0;
    }
    if (login_info.iot_sn == 0) {
        ESP_LOGW(TAG,"http_iot_download_cert iot_sn:%lld",login_info.iot_sn);
        return 0;
    }

    int16_t ret = 0;
    int16_t len=0;
    int16_t success = 0;
    uint64_t SN = login_info.iot_sn;
    uint64_t Safe_Code = login_info.safetyCode;

    int16_t download_file_type=0;//0-PrivateKey,1-Certificate
    char holder[33];
    // char password[128];
    char type[32];
    int content_length = 0;
    // unsigned char password[471]= {0};  // 16_hex
    unsigned char *password = NULL;
//    password = (unsigned char*)calloc(471, sizeof(char));//512 2048
    password = (unsigned char*)heap_caps_malloc((uint16_t)471*sizeof(char), MALLOC_CAP_SPIRAM);
    
    if(password == NULL)
    {
        ESP_LOGE(TAG,"password calloc faild\n");
        goto exit;
    }    

    //使用SHA256加密算法对设备的SecurityCode进行加密后的结果
    unsigned char plaintext[33];// 117 
    char sys_info[33];// 128
    int16_t sys_info_len = sprintf(sys_info,"%llu",Safe_Code);
    memcpy(plaintext, sys_info, sys_info_len);//M msg
    plaintext[sys_info_len] = '\0';//防止越界
    sys_info_len = sys_info_len+1;
    ESP_LOGI(TAG,"sys_info:%s,sys_info_len:%d",sys_info,sys_info_len);
    if((sys_info_len > 117) || (sys_info_len < 0))
    {
        perror("msg length must be less than 116 bytes!\n");
        goto exit;
    }
    cer_ras_encrypt(plaintext,sys_info_len,(unsigned char*)password);//M904503 sys_info 将安全码用公钥加密
    ESP_LOGI(TAG,"password:%s",password);

    //http请求包
    static const char *REQUEST = CERT_HTTP_DOWN_URL;

    // 初始化HTTP客户端
    esp_http_client_config_t config = {
        .url = REQUEST,//"/path/to/your/resource"
        .method = HTTP_METHOD_POST,
    };
    // 创建HTTP客户端实例
    esp_http_client_handle_t client = esp_http_client_init(&config);

download_again:
    sprintf(holder,"%s",serial_data);
    if(download_file_type == 0)
    {
        sprintf(type,"PrivateKey");
    }
    else
    {
        sprintf(type,"Certificate");
    }

    //POST Request
    char *post_data=NULL;
//    post_data = (char*)calloc(1024, sizeof(char));
    post_data = (char*)heap_caps_malloc((uint16_t)1024*sizeof(char), MALLOC_CAP_SPIRAM);
	
	if(post_data == NULL)
    {
        ESP_LOGE(TAG,"post_data calloc faild\n");
        goto exit;
    }
    // ret = sprintf(post_data,"{\"holder\":\"%.32s\",\"password\":\"%.128s\",\"type\":\"%.128s\"}", holder,password,type);
    ret = sprintf(post_data,"{\"holder\":\"%s\",\"password\":\"%s\",\"type\":\"%s\"}", holder,password,type);
    

    len = ret;//M strlen(post_data)
    ESP_LOGI(TAG,"post_data_download:%s, len:%d",post_data, len);

    if(len < 0)
    {
        ESP_LOGW(TAG,"Error len=%d",len);
    }

    /***** POST Headers data *****/
    //本地时间转UTC+0时间
    time_t localTime = time(NULL);    // 获取当前本地时间（秒级时间戳）
    
    char localTime_str[32]={0};
    // localTime = 1701694088452;//Test 1053756488

    char Safe_Code_str[32]={0};
    char Sn_Str[32]={0};
    // localTime = 1701694088452;//Test 1053756488
    sprintf(localTime_str,"%llu",(uint64_t)localTime);//localTime    local_utc0Time
    sprintf(Safe_Code_str,"%llu",Safe_Code);
    sprintf(Sn_Str,"IOT%llu",SN);
    
    ESP_LOGI(TAG,"localTime:%lu\n",(uint32_t)localTime);//local_utc0Time
    ESP_LOGI(TAG,"localTime_str:%s\n",localTime_str);
    ESP_LOGI(TAG,"sn_str:%s\n",Sn_Str);    

    //ETag是32位随机字符串，每次由客户端自动生成
    char ETag[33]={0};  
    generate_random_string(ETag,33);
    ESP_LOGI(TAG,"Etag:%s\n",ETag);
    sprintf(ETag,"%s","PLIBDCMCSTRAVBEASDBPAIOOCNGTMLZK");//test

    //Authorization，需要由请求头使用sha256加密生成
    // char Encry_Str[256]={0};
    // char Safe_Code_Sin_str[256]={0};
    char *Encry_Str = NULL;
    char *Safe_Code_Sin_str = NULL;
//    Encry_Str = (char*)calloc(256, sizeof(char));
    Encry_Str = (char*)heap_caps_malloc((uint16_t)256*sizeof(char), MALLOC_CAP_SPIRAM);
	
	if(Encry_Str == NULL)
    {
        ESP_LOGE(TAG,"Encry_Str calloc faild\n");
        goto exit;
    }
//    Safe_Code_Sin_str = (char*)calloc(256, sizeof(char));
    Safe_Code_Sin_str = (char*)heap_caps_malloc((uint16_t)256*sizeof(char), MALLOC_CAP_SPIRAM);
	
	if(Safe_Code_Sin_str == NULL)
    {
        ESP_LOGE(TAG,"Safe_Code_Sin_str calloc faild\n");
        goto exit;
    }

    sprintf(Encry_Str,"Date=%s&ETag=%s&X-Att-DeviceId=%s&safetyCode=%s&x-app-key=%s&x-os=%s",
            localTime_str,ETag,Sn_Str,Safe_Code_str,"18C2BC21BE8F79115700FFBF91B","hdevice");

    sha256_Encry(Encry_Str,Safe_Code_Sin_str);
    ESP_LOGI(TAG,"SHA256('%s') = %s\n", Encry_Str, Safe_Code_Sin_str);

    esp_http_client_set_url(client, REQUEST);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Date", localTime_str);//Add wx
    esp_http_client_set_header(client, "ETag", ETag);//Add wx
    esp_http_client_set_header(client, "x-os", "hdevice");
    esp_http_client_set_header(client, "x-app-key", "18C2BC21BE8F79115700FFBF91B");
    esp_http_client_set_header(client, "X-Att-DeviceId", Sn_Str);//Add wx
    esp_http_client_set_header(client, "Authorization", Safe_Code_Sin_str);//Add wx
    esp_http_client_set_authtype(client,HTTP_AUTH_TYPE_NONE);
    /***** POST Headers data *****/

    if(Encry_Str != NULL)
    {
        free(Encry_Str);
    }
	if(Safe_Code_Sin_str != NULL)
    {
        free(Safe_Code_Sin_str);
    }

    ESP_LOGI(TAG,"http_iot_download_cert:len:%d",len);
    esp_err_t err = esp_http_client_open(client, len);//M strlen(post_data)
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        int wlen = esp_http_client_write(client, (const char *)post_data, len);//M strlen(post_data)
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write failed");
        }
        content_length = esp_http_client_fetch_headers(client);
        if(content_length < 0) 
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } 
        else 
        {
//            download_buffer = (char*)calloc(download_buffer_len,sizeof(char));
			download_buffer = (char*)heap_caps_malloc((uint16_t)download_buffer_len*sizeof(char), MALLOC_CAP_SPIRAM);
			
            if(download_buffer == NULL)
            {
                ESP_LOGE(TAG, "download_buffer calloc faild\n");
                goto exit;
            }
            // char *read_buffer = (char*)calloc(download_buffer_len,sizeof(char));
            // if(read_buffer == NULL)
            // {
            //     ESP_LOGE(TAG, "read_buffer calloc faild\n");
            //     goto exit;
            // }
            memset(download_buffer, 0x00, download_buffer_len);
            int data_read = esp_http_client_read_response(client, download_buffer, download_buffer_len);
            if (data_read >= 0) 
            {
//                ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d, data_read=%d",
//                esp_http_client_get_status_code(client),
//                esp_http_client_get_content_length(client),
//                data_read);

                ESP_LOGE(TAG, "download file type:%d, buffer data:%s", download_file_type, download_buffer);
                //验证收到的证书是否为完整的PEM格式的函数
//                ESP_LOGI(TAG,"download_buffer:%s,download_buffer_len:%d,len:%d",
//                    download_buffer,download_buffer_len,esp_http_client_get_content_length(client));
                success = check_integrity(download_file_type,download_buffer,(esp_http_client_get_content_length(client) - 1));//-1有个空字符
                //要将公钥、私钥证书全部校验完后再存入
                //先写入私钥，再写入公钥
#if ENCRYPT_CERT_USE_FILE_SYSTEM
                if(success == 1)
                {
                    write_cert_to_file(PRIVATE_AREA, (uint8_t*)download_buffer, strlen(download_buffer));
                }
                else if(success == 2)
                {
                    write_cert_to_file(IOT_CERTIFICATE_AREA, (uint8_t*)download_buffer, strlen(download_buffer));
                }
#else
                partition_reinit(UPDATE_AREA, PARTITION_SIZE);
                if(success == 1)
                {
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, UPDATE_AREA);//先存入缓存区
                    /*此处最好再有一个检验证书的步骤 暂时从缓存区读*/
                    memset(download_buffer, 0, strlen(download_buffer));

                    uint32_t len = get_partition_plaintext_len(UPDATE_AREA); //获取长度
                    // partition_read_decrypt((uint8_t*)read_buffer, len, 0, UPDATE_AREA);
                    // partition_write_encrypt((uint8_t*)read_buffer, strlen(read_buffer), 0, PRIVATE_AREA);//覆盖原私钥区
                    partition_read_decrypt((uint8_t*)download_buffer, len, 0, UPDATE_AREA);
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, PRIVATE_AREA);//覆盖原私钥区
                    // memset(download_buffer, 0, strlen(download_buffer));
                    // free(read_buffer);
                    // read_cert();
                    // ESP_LOGI(TAG,"PrivateKey download done");
                }
                else if(success == 2)
                {                    
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, UPDATE_AREA);//先存入缓存区
                    /*此处最好再有一个检验证书的步骤*/
                    memset(download_buffer, 0, strlen(download_buffer));

                    uint32_t len = get_partition_plaintext_len(UPDATE_AREA); //获取长度
                    // partition_read_decrypt((uint8_t*)read_buffer, len, 0, UPDATE_AREA);
                    // partition_write_encrypt((uint8_t*)read_buffer, strlen(read_buffer), 0, IOT_CERTIFICATE_AREA);//覆盖原公钥区
                    partition_read_decrypt((uint8_t*)download_buffer, len, 0, UPDATE_AREA);
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, IOT_CERTIFICATE_AREA);//覆盖原公钥区
                    // memset(download_buffer, 0, strlen(download_buffer));
                    // free(read_buffer);
                    // read_cert();
                    // ESP_LOGI(TAG,"Certificate download done");
                }                
#endif
                ESP_LOGI(TAG,"cjson_to_serial_info:success=%d",success);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
        if(success == 1)
        {
            ESP_LOGI(TAG,"PrivateKey done, continue to download Certificate");
            download_file_type = 1;//继续下载
            memset(download_buffer,0,download_buffer_len);
            goto download_again;
        }
        else if(success == 2)
        {
            ESP_LOGI(TAG,"Certificate done, all files done");
			if(download_buffer != NULL)
			{
				free(download_buffer);
			}            
        }
        else
        {
            ESP_LOGE(TAG,"File download failed");
            if(download_buffer != NULL)
			{
				free(download_buffer);
			}
        }
    }    
    esp_http_client_cleanup(client);  
    if(post_data != NULL)
    {
        free(post_data);
    }
exit:
    return success;
}

//验证收到的证书是否为完整的PEM格式的函数 PKCS#8 标准定义
uint8_t check_integrity(int16_t download_type, char *buffer, int buffer_len)
{
    uint8_t success = 0;
    if(download_type == 0)
    {
        //-----BEGIN PRIVATE KEY-----比较前27个开头格式的字符和末尾25个字符(不包含回车和空字符) 移动数组指针去比较末尾字符
        if((!memcmp("-----BEGIN PRIVATE KEY-----", buffer, 27)) && (!strncmp("-----END PRIVATE KEY-----", buffer + buffer_len - 25, 25)))
        {
            ESP_LOGI(TAG,"PRIVATE KEY check finished");
            success = 1;
        }
        else
        {
            ESP_LOGE(TAG,"PRIVATE KEY check wrong");
            success = 0;
        }
    }
    else
    {
        //-----BEGIN CERTIFICATE-----
        if((!memcmp("-----BEGIN CERTIFICATE-----", buffer, 27)) && (!strncmp("-----END CERTIFICATE-----", buffer + buffer_len - 25, 25)))
        {
            ESP_LOGI(TAG,"CERTIFICATE check finished");
            success = 2;
        }
        else
        {
            ESP_LOGE(TAG,"CERTIFICATE check wrong");
            success = 0;
        }
    }
    return success;
}
esp_err_t iot_mqtt_send(uint8_t *data, int len)
{
    esp_err_t ret=ESP_FAIL;
	if ((reals.wifi_connect_ap == 0) || (login_info.mqtt_is_login == 0)) 
	{
		return ret;	// wifi未连接或mqtt未登录
	}
	ret=esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)data, len, 0, 0);
    if ( ret == -1 ) {
        ESP_LOGE(TAG, "topic:%s publish failed", login_info.pub_topic);
    } else {
        reals.net_frame_tx_counter++;
    }
    return ret;
}

 /*------------------------------------------------------------------------------
  Function: Mqtt_Active_Report_Queue_Init
-----------------------------------------------------------------------------*/
 /**
* @brief      队列初始化
* @param[in]  void  
* @param[out] None
* @return     void
*/
static void Mqtt_Active_Report_Queue_Init(void)
{
    /*队列发送*/
    if (xQueue_Mqtt_Active_Report == NULL)
    {
        xQueue_Mqtt_Active_Report = xQueueCreate(16, sizeof(Mqtt_Active_Report_Queue_Struct));
        if (xQueue_Mqtt_Active_Report == NULL)
        {
            ESP_LOGE(TAG, "Mqtt_Active_Report_Queue_Init create failed");
        }
    }
}

/*------------------------------------------------------------------------------
   Function: Mqtt_Active_Report_Queue_Push
-----------------------------------------------------------------------------*/
/**
* @brief      紧急发送存储到队列缓存
* @param[in]  void  
* @param[out] None
* @return     void
*/
void Mqtt_Active_Report_Queue_Push(uint8_t *pdata, uint16_t len, uint8_t scene)
{
    Mqtt_Active_Report_Queue_Struct queue_msg;

    if (xQueue_Mqtt_Active_Report == NULL) {
        ESP_LOGE(TAG, "xQueue_Mqtt_Active_Report is NULL!");
        return;
    }

    queue_msg.data = (uint8_t *)iot_calloc(len + 10);

    if (!queue_msg.data) 
    {
        ESP_LOGE(TAG, "Mqtt_Active_Report_Queue_Push calloc failed");
    } 
    else 
    {
        pack_data_head(0x01, queue_msg.data, scene, 0, 0, 0);   // 封装mqtt数据1.2v协议头
        memcpy(&queue_msg.data[10], pdata, len);
        queue_msg.data_len = len + 10;

        /*消息保存到队列*/
        if (!xQueue_Mqtt_Active_Report || xQueueSendToBack(xQueue_Mqtt_Active_Report, &queue_msg, 0) != pdPASS) 
        {
            free(queue_msg.data);
        }
    }
}

/*------------------------------------------------------------------------------
  Function: Mqtt_Active_Report_Queue_Report
-----------------------------------------------------------------------------*/
/**
* @brief      主动上报任务
* @param[in]  void  
* @param[out] None
* @return     void b
*/
static int Mqtt_Active_Report_Queue_Report(void)
{
    int ret = -1;
    Mqtt_Active_Report_Queue_Struct queue_msg;

    if(0 != login_info.mqtt_is_login)
    {
        if (xQueue_Mqtt_Active_Report && xQueueReceive(xQueue_Mqtt_Active_Report, &queue_msg,  0) == pdTRUE)//非阻塞
        {
            if (queue_msg.data) 
            {
                ret = iot_mqtt_send(queue_msg.data, queue_msg.data_len);  
                free(queue_msg.data);
            }
        }
    }

    return ret;
}

 /*------------------------------------------------------------------------
 *@Function :Modbus_Self_21000_Report_MQTT 
 
 主动上报绑定帧
 
主动上报和基于RX查询的TX的 mqtt通道，二者不同

 */
uint8_t Modbus_Self_21000_Report_MQTT(void) 
{
	uint16_t rsp_len = 0;
	uint8_t pointcnt = 0;	
	uint8_t point_tx_nums = 0;	
	uint16_t ver = 0;
	uint8_t point_bias = 0;	
    uint8_t *tx_buff = NULL;
    uint8_t ret = 0;
    uint8_t sendcnt = 0;
    uint8_t is_ota_reply=0;
   // ESP_LOGI(TAG,"Modbus_Self_21000_Report_MQTT %d  %d  %d  %d",login_info.mqtt_is_login,reals.modbus_self_report_ble,reals.bind_state_ask,g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect);
    if(0 == login_info.mqtt_is_login) return 0;	//mqtt未登录

	if(((0 == reals.modbus_self_report_ble)||(0 == g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect)) //蓝牙未连接或未在发送绑定帧
        &&(0 == reals.bind_state_ask)) ver = reals.modbus_self_report_mqtt;         
    
    if ( ver == 0 ) return 0;
    if(!Modbus_21000_semaphore_Take()) return 0;
    
    switch ( ver )
    {    
        case 1 :
            /*绑定帧响应*/
            pointcnt = Modbus_21000_1_Net_Point_Frame();
            ESP_LOGW(TAG, "Modbus_Self_21000_Report_MQTT(01)  pointcnt:%d", pointcnt);
            break;
            
        case 3 :

            /*升级状态查询响应*/
            pointcnt=Modbus_21000_3_Net_Point_Frame();
            is_ota_reply=1;
            ESP_LOGW(TAG, "Modbus_Self_21000_Report_MQTT(03)  pointcnt:%d", pointcnt);
            break;

        default:
            
            break;
    }

	do
	{
//	    ESP_LOGW(TAG, "Modbus_Self_21000_Report_MQTT  pointcnt:%d", pointcnt);
        ESP_LOGW(TAG, "Modbus_Self_21000_Report_MQTT  pointcnt:%d sendcnt:%d", pointcnt,sendcnt);

	    if(pointcnt >= BIND_READ_MAX_DEV_NUM)
	    {
		    pointcnt -= BIND_READ_MAX_DEV_NUM;
		    point_tx_nums = BIND_READ_MAX_DEV_NUM;
	    }
	    else if(pointcnt > 0)
	    {
		    point_tx_nums = pointcnt;	
		    pointcnt = 0;					   
	    }		
        else
        {
            break;
        }
        
        if(tx_buff == NULL) {
            tx_buff = heap_caps_malloc(300 * sizeof(char), MALLOC_CAP_SPIRAM);
            if (tx_buff == NULL) {
                ESP_LOGE(TAG, "malloc fail");
                goto end;
            }
        } else {
            memset(tx_buff, 0, 300);
        }
        
		g_self_data.mod_reg21000_bind.bias = sendcnt;
		point_bias += point_tx_nums;
        pack_data_head(0x01, tx_buff, PUB_Multi_DEV, 0, 0, 0);   // 封装mqtt数据1.2v协议头

        uint8_t reg_cnt = 0;

        reg_cnt = point_tx_nums * (sizeof(POINT_BIND_INFO) / 2);

		rsp_len = Modbus_WriteMultiRegs_Report_Frame(&tx_buff[10], reg_cnt, ver);
        
        ESP_LOGW(TAG, "Modbus_Self_21000_Report_MQTT, Ver:%u, reg_addr:%d, data_len:%d, data:", ver, 21000, rsp_len+10);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, tx_buff, rsp_len+10, ESP_LOG_WARN);

        iot_mqtt_send(tx_buff, rsp_len+10); 
        ret |= 1;
        
        if ( pointcnt > 0 ) vTaskDelay(pdMS_TO_TICKS(50));//为减少发送报文两帧粘连，需要强制延时隔断

        sendcnt+=point_tx_nums;
        Mqtt_Active_Report_Queue_Push(tx_buff, rsp_len, PUB_Multi_DEV);
	}
	while (pointcnt);	
    
    if (tx_buff != NULL) free(tx_buff);
    if(is_ota_reply&&ret&&(reals.iot_ota_flag==1))//(gXmodem_Status.firmware_resend_step ==2))
    {
        ESP_LOGI(TAG,"mqtt reals.iot_ota_flag:%d",reals.iot_ota_flag);
        reals.iot_ota_flag=3;
    }
    end:
    
    if(ver == reals.modbus_self_report_mqtt) reals.modbus_self_report_mqtt = 0;	
    Modbus_21000_semaphore_Give();

    return ret;
}

#if HTTP_UTC_TIME_VERSION == 2
 uint8_t mqtt_client_UTC_IP_GET(login_info_t *mqtt_info) 
 {
     char url[128] = {0};
     uint64_t time = 0;
     uint8_t ret = 0;
     
     mqtt_info->now_time = 0;

     // 先通过HTTP获取UTC时区和IP地址
     if(http_get_utc_time_and_ip(url, 128, &time) == 0){
         mqtt_info->now_time = time;
         
#if HTTP_UTC_TIME_VERSION == 2
         /*V2接口：返回连接域名*/
         ret = iot_get_cloud_dns(url, mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
#else
         /*V1接口，返回IP地址*/
         int n1 = sscanf(url, "%120[^:] %*[:]%hu",  mqtt_info->host, &mqtt_info->port); // 将输入的HOST地址解析出地址和端口
         int n2 = sscanf(mqtt_info->host, "%hhu.%hhu.%hhu.%hhu", &mqtt_info->ipaddr[0], &mqtt_info->ipaddr[1], \
                                                 &mqtt_info->ipaddr[2], &mqtt_info->ipaddr[3]); // 将输入的HOST地址解析出地址和端口
         ret = (n1 == 2 && n2 == 4) ? 1 : 0;
#endif

         /*缓存上次连接接口*/
         if (ret) iot_last_host_set(mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
     }

     if (!ret) {
         /*获取本地存储的HOST*/
         ret = iot_last_host_get(mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
            
         // 如果失败则通过DNS解析获取IP地址
         if(!ret) return iot_get_cloud_dns(mqtt_info->raw_url, mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
     }
     
     return ret;
 }
 #else
 // 先通过HTTP获取UTC时区和IP地址，如果失败则通过DNS解析获取IP地址
 uint8_t mqtt_client_UTC_IP_GET(login_info_t *mqtt_info) 
 {
     char host[128] = {0};
     uint64_t time = 0;
     mqtt_info->now_time = 0;
    
     if(http_get_utc_time_and_ip(host, 128, &time) == 0){
         // 将char host填充到数组host_ipv4中
         mqtt_info->now_time = time;
         sscanf(host, "%120[^:] %*[:]%hu",  mqtt_info->host, &mqtt_info->port); // 将输入的HOST地址解析出地址和端口

         sscanf(mqtt_info->host, "%hhu.%hhu.%hhu.%hhu", &mqtt_info->ipaddr[0], &mqtt_info->ipaddr[1], \
                                                 &mqtt_info->ipaddr[2], &mqtt_info->ipaddr[3]); // 将输入的HOST地址解析出地址和端口

		 return 1;
     }
 
    return iot_get_cloud_dns(mqtt_info->raw_url, mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
 }
 #endif


//  /*上次连接的缓存数据*/
// static uint8_t http_get_host_valid = 0;
// static char http_get_old_host[120] = {0};
// static uint16_t http_get_old_port = 0;
// static uint8_t http_get_old_ipaddr[4] = {0};

/*------------------------------------------------------------------------------
 Function: iot_last_host_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      上次连接域名获取（返回上次缓存的host、port、ipaddr）
  * @param[out] char *host        输出：上次连接的host
  * @param[out] uint16_t *port    输出：上次连接的端口
  * @param[out] uint8_t ipaddr[]  输出：上次连接的IP地址（长度4）
  * @return     static uint8_t           1-有有效缓存，0-无缓存
  */
static uint8_t iot_last_host_get(char *host, uint16_t *port, uint8_t ipaddr[])
{
    // extern uint8_t http_get_host_valid;
    // extern char http_get_old_host[120];
    // extern uint16_t http_get_old_port;
    // extern uint8_t http_get_old_ipaddr[4];

    // 检查参数有效性
    if (!host || !port || !ipaddr) {
        ESP_LOGE(TAG, "iot_last_host_get: invalid output pointer");
        return 0;
    }

    // 检查缓存是否有效
    if (SetData.dev_info_t.http_get_host_valid &&
        SetData.dev_info_t.http_get_old_host[0] != '\0' &&
        SetData.dev_info_t.http_get_old_port != 0 &&
        (SetData.dev_info_t.http_get_old_ipaddr[0] | SetData.dev_info_t.http_get_old_ipaddr[1] | SetData.dev_info_t.http_get_old_ipaddr[2] | SetData.dev_info_t.http_get_old_ipaddr[3]) != 0) {
        strncpy(host, SetData.dev_info_t.http_get_old_host, 119);
        host[119] = '\0';
        *port = SetData.dev_info_t.http_get_old_port;
        memcpy(ipaddr, SetData.dev_info_t.http_get_old_ipaddr, 4);
        ESP_LOGI(TAG, "iot_last_host_get: use cached host=%s, port=%u, ip=%d.%d.%d.%d",
                 host, *port, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
        return 1;
    }

    ESP_LOGW(TAG, "iot_last_host_get: no valid cache");
    return 0;
}

 /*------------------------------------------------------------------------------
  Function: iot_last_host_set
  -----------------------------------------------------------------------------*/
 /**
  * @brief      保存本次连接的host、port、ipaddr到缓存
  * @param[in]  const char *host
  * @param[in]  uint16_t port
  * @param[in]  const uint8_t ipaddr[4]
  * @return     static void
  */
static void iot_last_host_set(const char *host, const uint16_t *port, const uint8_t ipaddr[])
{
    // extern uint8_t http_get_host_valid;
    // extern char http_get_old_host[120];
    // extern uint16_t http_get_old_port;
    // extern uint8_t http_get_old_ipaddr[4];

    // SetData.dev_info_t.http_get_host_valid
    // SetData.dev_info_t.http_get_old_host
    // SetData.dev_info_t.http_get_old_port
    // SetData.dev_info_t.http_get_old_ipaddr
          
        ESP_LOGI(TAG,"SAME cur vail:%d, host:%s,port:%u ,ip=%d.%d.%d.%d",SetData.dev_info_t.http_get_host_valid,SetData.dev_info_t.http_get_old_host,SetData.dev_info_t.http_get_old_port,SetData.dev_info_t.http_get_old_ipaddr[0], SetData.dev_info_t.http_get_old_ipaddr[1], SetData.dev_info_t.http_get_old_ipaddr[2], SetData.dev_info_t.http_get_old_ipaddr[3]);
    if (host && strlen(host) > 0 && port && *port != 0 && ipaddr) {
        if(0==strncmp(SetData.dev_info_t.http_get_old_host,host,119)
        &&(SetData.dev_info_t.http_get_old_port==*port)
        &&(0==memcmp(SetData.dev_info_t.http_get_old_ipaddr, ipaddr,sizeof(SetData.dev_info_t.http_get_old_ipaddr)))
        )
        {
            ESP_LOGI(TAG,"SAME new host:%s,port:%u ,ip=%d.%d.%d.%d",host,*port,ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
            SetData.dev_info_t.http_get_host_valid = 1;
            goto __end;
        }  
        strncpy(SetData.dev_info_t.http_get_old_host, host, 119);
        SetData.dev_info_t.http_get_old_host[119] = '\0';
        SetData.dev_info_t.http_get_old_port = *port;
        memcpy(SetData.dev_info_t.http_get_old_ipaddr, ipaddr, 4);
        SetData.dev_info_t.http_get_host_valid = 1;
        reals.SetDataWrFlag.sBit.Set_Host_Ip=1;
        ESP_LOGI(TAG, "iot_last_host_set: cache updated host=%s, port=%u, ip=%d.%d.%d.%d", host, *port, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
    } else {
        SetData.dev_info_t.http_get_host_valid = 0;
        ESP_LOGW(TAG, "iot_last_host_set: invalid input, cache cleared");
    }
__end:
    return;
}