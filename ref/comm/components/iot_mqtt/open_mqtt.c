/**
  ******************************************************************************
  * @file      open_mqtt.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/2/5
  * @brief     MQTT_OPEN模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/2/5   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "netdb.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "modbus_data.h"
#include "modbus_slave.h"
#include "modbus_master.h"
#include "modbus_define.h"
#include "can_data.h"
#include "can_protocol.h"

#include "open_mqtt.h"
#include "utils.h"
#include "comm_define.h"
#include "parameter.h"
#include "iot_mqtt.h"

/*------------------------------------------------------------------------------------------------------------------------*/

#define TAG   "[MQTT_OPEN]"

#ifdef CONFIG_OPEN_MQTT_ENABLE

static SemaphoreHandle_t open_mqtt_semaphore = NULL;
static uint8_t open_mqtt_step = 0;//WIFI通道的MQTT登陆状态机
static USE_EXT_RAM_BSS open_mqtt_config_t open_mqtt_config = {0};//static

static struct esp_mqtt_client_dump open_mqtt_client_dump = {0};
static TimerHandle_t open_mqtt_sub_timeout_tmr = NULL;
static USE_EXT_RAM_BSS esp_mqtt_client_config_t open_mqtt_cfg = {0};
static uint8_t open_mqtt_exit_flag = 0;//断开连接
static USE_EXT_RAM_BSS uint8_t open_mqtt_tx_buf[300] = {0};//周期发送缓存

static uint32_t Open_Mqtt_Machine_Pause_Time = 0;
static bool Open_Mqtt_Restart_Flag = false;

/*------------------------------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 Function: open_mqtt_timeout_timer_cb
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT订阅响应超时回调
  * @param[in]  TimerHandle_t xTimer  
  * @param[out] None
  * @return     static void
  */
static void open_mqtt_timeout_timer_cb(TimerHandle_t xTimer)
{
    memset(&open_mqtt_client_dump, 0x00, sizeof(struct esp_mqtt_client_dump));
    open_mqtt_config.mqtt_is_login = 0;
    if(open_mqtt_step != STEP_MQTT_WAITING) open_mqtt_step = STEP_MQTT_RESTART;  
    ESP_LOGE(TAG, "open_mqtt_timeout_timer_cb : mqtt sub failed!");
}

/*------------------------------------------------------------------------------
 Function: open_mqtt_timeout_timer_start
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT订阅响应超时
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void open_mqtt_timeout_timer_start(void)
{
	if (open_mqtt_sub_timeout_tmr)
	{
		xTimerStart(open_mqtt_sub_timeout_tmr, 0);
		return;
	}

	/* 创建通信超时定时器 */
	open_mqtt_sub_timeout_tmr = xTimerCreate("mqtt sub timer", pdMS_TO_TICKS(10000), pdFALSE, NULL, open_mqtt_timeout_timer_cb);
	if (open_mqtt_sub_timeout_tmr == NULL)
	{
		ESP_LOGE(TAG, "mqtt subscribe timeout timer create failed, Restarting");
		esp_restart();
		return;
	}
	xTimerStart(open_mqtt_sub_timeout_tmr, 0);
}

/*------------------------------------------------------------------------------
 Function: open_mqtt_timeout_timer_stop
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT订阅响应超时停止
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void open_mqtt_timeout_timer_stop(void)
{
    /*MQTT订阅响应成功*/
	if (open_mqtt_sub_timeout_tmr)
		xTimerStop(open_mqtt_sub_timeout_tmr, 0);
}

 /*------------------------------------------------------------------------------
  Function: open_mqtt_task_init
  -----------------------------------------------------------------------------*/
 /**
   * @brief      MQTT初始化
   * @param[in]  void  
   * @param[out] None
   * @return     void
   */
 int open_mqtt_task_init(void)
 {
    if (open_mqtt_config.mqtt_msg_queue == NULL) {
        open_mqtt_config.mqtt_msg_queue = xQueueCreate(16, sizeof(queue_msg_t));
        if (open_mqtt_config.mqtt_msg_queue == NULL) {
            ESP_LOGE(TAG, "mqtt recv message queue create failed");
            return -1;
        }
    }

	open_mqtt_semaphore = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(open_mqtt_semaphore == NULL) {
        ESP_LOGE(TAG, "open_mqtt_semaphore  create failed");
		return -1;
	}
	xSemaphoreGive(open_mqtt_semaphore);						/**< the resource is available after creation */


	int16_t ret =0;
    open_mqtt_step = STEP_MQTT_WAITING;
	open_mqtt_config.sock = -1;
    open_mqtt_config.mqtt_client = NULL;
    login_info.current_netif_id = 0xFF;

    /*服务器地址配置*/
    open_mqtt_new_host(IotSetData.dev_info_t.Net_Server_address);

    /*服务器密码配置*/
    open_mqtt_new_password(IotSetData.dev_info_t.Net_Server_secret);

    /*MQTT订阅*/
    if ((0 != dev_factory.dev_sn) 
        && (MASS_PRODUCTION_DEFAULT_DEV_SN != dev_factory.dev_sn)) 
    {
        open_mqtt_new_dev(dev_factory.dev_type,  dev_factory.dev_sn);
    } else {
        /*未标定，默认用IOT的SN*/
        open_mqtt_new_dev(iot_factory.iot_type,  iot_factory.iot_sn);
    }   

    /*OPEN_MQTT开关检查*/
    if (1 == IotSetData.dev_info_t.open_mqtt_enable.enable) {    
        open_mqtt_start();
    } else {
        set_open_mqtt_stop();
    }
        
    return 0;
 }

/*------------------------------------------------------------------------------
 Function: open_mqtt_exit_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      退出MQTT
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void open_mqtt_exit_process(void)
{
     memset(&open_mqtt_client_dump, 0x00, sizeof(struct esp_mqtt_client_dump));
     open_mqtt_config.mqtt_is_login = 0;
     open_mqtt_config.mqtt_is_start = 0;
     
     /* socket fd 0、1、2分别表示标准输入、标准输出、标准错误,不能被关闭 */
     if ((open_mqtt_config.sock != 0xff) && (open_mqtt_config.sock >= 3)) {
         // 先调用 shutdown 再调用 close
         if (shutdown(open_mqtt_config.sock, SHUT_RDWR) == -1) {
             ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", open_mqtt_config.sock, errno, strerror(errno));
         }
         if (close(open_mqtt_config.sock) == -1) {
             ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", open_mqtt_config.sock, errno, strerror(errno));
             if ( errno == EBADF )
             {
                 /*Bad file number*/
                 open_mqtt_config.sock = -1;
             }
         }
         else
         {
             open_mqtt_config.sock = -1;
         }
     }
     
     if (open_mqtt_config.mqtt_client != NULL) 
     {
         esp_mqtt_client_stop(open_mqtt_config.mqtt_client);
         esp_mqtt_client_destroy(open_mqtt_config.mqtt_client);
         open_mqtt_config.mqtt_client = NULL;
         ESP_LOGW(TAG, "mqtt client stop");
     }
}


/*------------------------------------------------------------------------------
 Function: set_open_mqtt_client_netif
 -----------------------------------------------------------------------------*/
/**
  * @brief      切换网口使用
  * @param[in]  uint8_t netif_type  
  * @param[out] None
  * @return     static void
  */
void set_open_mqtt_client_netif(uint8_t netif_type)
{
    char *netif_key[NETIF_TYPE_MAX] = {NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP, NETIF_KEY_WIFI_AP};
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(netif_key[netif_type]);

#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(netif, open_mqtt_config.netif_req.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(netif), open_mqtt_config.netif_req.ifr_name);
#endif

    open_mqtt_cfg.network.if_name = &(open_mqtt_config.netif_req);
    open_mqtt_config.current_netif_id = netif_type;
    ESP_LOGI(TAG, "set_open_mqtt_client_netif:%s", open_mqtt_cfg.network.if_name->ifr_name);

    Open_Mqtt_Restart_Flag = true;
}

static void open_mqtt_log_error_if_nonzero(const char *message, int error_code) //static
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void open_mqtt_log_error_details(esp_mqtt_event_handle_t event)
{
    if (event == NULL || event->error_handle == NULL) {
        ESP_LOGE(TAG, "mqtt error_handle is NULL");
        return;
    }

    switch (event->error_handle->error_type)
    {
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            ESP_LOGE(TAG, "MQTT transport error");

            open_mqtt_log_error_if_nonzero("reported from esp-tls",
                                 event->error_handle->esp_tls_last_esp_err);
            open_mqtt_log_error_if_nonzero("reported from tls stack",
                                 event->error_handle->esp_tls_stack_err);
            open_mqtt_log_error_if_nonzero("captured as transport's socket errno",
                                 event->error_handle->esp_transport_sock_errno);
            break;

        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
            switch (event->error_handle->connect_return_code)
            {
                case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
                    ESP_LOGE(TAG, "MQTT connection refused: bad username");
                    break;

                case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
                    ESP_LOGE(TAG, "MQTT connection refused: not authorized / bad password");
                    break;

                case MQTT_CONNECTION_REFUSE_PROTOCOL:
                    ESP_LOGE(TAG, "MQTT connection refused: protocol error");
                    break;

                case MQTT_CONNECTION_REFUSE_ID_REJECTED:
                    ESP_LOGE(TAG, "MQTT connection refused: client id rejected");
                    break;

                case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
                    ESP_LOGE(TAG, "MQTT connection refused: server unavailable");
                    break;

                default:
                    ESP_LOGE(TAG, "MQTT connection refused: code=%d",
                             event->error_handle->connect_return_code);
                    break;
            }
            break;

        case MQTT_ERROR_TYPE_SUBSCRIBE_FAILED:
            ESP_LOGE(TAG, "MQTT subscribe failed");
            break;

        default:
            ESP_LOGE(TAG, "MQTT unknown error type=%d",
                     event->error_handle->error_type);
            break;
    }
    
    Open_Mqtt_Machine_Pause_Time = 60 * 1000;
}

static void open_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
//    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    open_mqtt_config_t *open_mqtt_config = (open_mqtt_config_t *)handler_args;
    int msg_id = 0;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "mqtt connected");
            open_mqtt_config->mqtt_connect_fail = 0;
			memcpy(&open_mqtt_client_dump, client, sizeof(struct esp_mqtt_client_dump));
           

            snprintf(open_mqtt_config->sub_topic, sizeof(open_mqtt_config->sub_topic), SUBSCRIBE_TOPIC, open_mqtt_config->dev_type, open_mqtt_config->dev_sn);
            snprintf(open_mqtt_config->pub_topic, sizeof(open_mqtt_config->pub_topic), PUBLISH_TOPIC,   open_mqtt_config->dev_type, open_mqtt_config->dev_sn);
			char all_public_topic[100];
			snprintf(all_public_topic, sizeof(all_public_topic), ALL_PUBLIC_TOPIC,  open_mqtt_config->dev_type);

            msg_id = esp_mqtt_client_subscribe(client, all_public_topic, 0);		// 订阅公共主题
            ESP_LOGI(TAG, "subscribe public topic, msg_id=%d topic: %s", msg_id, all_public_topic);

            msg_id = esp_mqtt_client_subscribe(client, open_mqtt_config->sub_topic, 0);	// 订阅接收服务器命令主题
            ESP_LOGI(TAG, "subscribe receive topic, msg_id=%d topic: %s", msg_id, open_mqtt_config->sub_topic);
			ESP_LOGI(TAG, "data publish topic: %s, and waiting for MQTT_EVENT_SUBSCRIBED", open_mqtt_config->pub_topic); // 数据发送主题
			open_mqtt_timeout_timer_start();
            
            break;

        case MQTT_EVENT_DISCONNECTED: // MQTT链接断开事件
            open_mqtt_config->mqtt_is_login = 0;
            ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
            if(open_mqtt_step != STEP_MQTT_WAITING) open_mqtt_step = STEP_MQTT_RESTART;  
            break;

        case MQTT_EVENT_SUBSCRIBED: /* 订阅完成 */
			open_mqtt_timeout_timer_stop();
            open_mqtt_config->mqtt_is_login = 1;
            ESP_LOGW(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED: /* 取消订阅 */
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "topic: %s recv message", event->topic);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, event->data, event->data_len, ESP_LOG_DEBUG);
            reals.net_frame_rx_counter++;
            
            if (!strncmp(event->topic, ALL_PUBLIC_TOPIC, strlen(ALL_PUBLIC_TOPIC))) {
                // 公共主题-数据接收
                ESP_LOGI(TAG, "PUBLIC_TOPIC(%s) : recv message(len = %u)", event->topic, event->data_len);
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, event->data, event->data_len, ESP_LOG_INFO);
            } else if (!strncmp(event->topic, open_mqtt_config->sub_topic, strlen(open_mqtt_config->sub_topic)))  {
                // 设备私有主题-数据接收
                if ( open_mqtt_config->mqtt_msg_queue == NULL) {
                    ESP_LOGE(TAG, "mqtt msg queue is null");
                    break;
                }
                queue_msg_t mqtt_msg;
                mqtt_msg.msg_len = event->data_len; //
                mqtt_msg.msg_ptr = (uint8_t *)iot_calloc(event->data_len);
                if (mqtt_msg.msg_ptr == NULL) {
                    ESP_LOGI(TAG, "mqtt recv msg malloc (%dbytes) failed", mqtt_msg.msg_len);
                    break;
                }

                memcpy(mqtt_msg.msg_ptr, event->data, event->data_len);
                if (xQueueSendToBack((QueueHandle_t)open_mqtt_config->mqtt_msg_queue, &mqtt_msg, pdMS_TO_TICKS(100)) != pdPASS) 
				{
                    free(mqtt_msg.msg_ptr);
                    ESP_LOGE(TAG, "mqtt message handle queue full"); // MQTT发送过来的消息太多,就会处理不及时队列满
                }
            } else {
                // 未知主题
                ESP_LOGE(TAG, "mqtt unkonw topic");
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            open_mqtt_config->mqtt_is_login = 0;
        
            // MQTT 错误类型
            open_mqtt_log_error_details(event);

            // MQTT登录流程重新开始
            if(open_mqtt_step != STEP_MQTT_WAITING) open_mqtt_step = STEP_MQTT_RESTART;  
            
            break;

        default: ESP_LOGI(TAG, "Other event id:%d", event->event_id); break;
    }
}

static uint8_t open_mqtt_login(open_mqtt_config_t *config) 
{
    if (config == NULL) return 0;


	char host[150] = "mqtt://";
    char username[64] = {0};
    char password[64] = {0};
    uint16_t keepalive_dafault = 0;
    
    if ( NETIF_TYPE_PPP == config->current_netif_id ) {
        keepalive_dafault = (IotSetData.dev_info_t.mqtt_heartbeat > 0) ? IotSetData.dev_info_t.mqtt_heartbeat : (60*5);   // 4G模式下默认为5分钟
    } else {
        keepalive_dafault = 50;     // 默认为50s
    }
    
    strcat(host, config->host);
    snprintf(username, sizeof(username), "%s%013llu", config->dev_type, config->dev_sn);
	snprintf(password, sizeof(password), "%s", config->password);
#if 0
    esp_mqtt_client_config_t open_mqtt_cfg = {
        .uri  = host,               /*!< MQTT host */
        .port = config->port,        /*!< MQTT port */
        .username = username,       /*!< MQTT username */
        .password = password,       /*!< MQTT password */
        .client_id = username,      /*!< MQTT clientId */
        .disable_auto_reconnect = false, /*!< MQTT auto reconnect  */
        .keepalive = 50,            /* 50s */
    };
#endif
    open_mqtt_cfg.broker.address.uri = host;                             /*!< MQTT host */
    open_mqtt_cfg.broker.address.port = config->port;
#if !MQTT_DEBUG
    open_mqtt_cfg.credentials.username = username;                       /*!< MQTT username */        
    open_mqtt_cfg.credentials.authentication.password = password;        /*!< MQTT password */
#endif
    open_mqtt_cfg.credentials.client_id = username;                      /*!< MQTT clientId */
    open_mqtt_cfg.network.disable_auto_reconnect = true;                 /*!< MQTT auto reconnect  */
    open_mqtt_cfg.network.reconnect_timeout_ms = 50;                     // 默认值为10000ms，关闭组件层自动重连后可以调小来优化错误断连后esp_mqtt_client_stop时长
    open_mqtt_cfg.network.timeout_ms = 10000;                            // 默认值为10000ms，网络操作超时 (连接/发送/接收)    
    open_mqtt_cfg.session.keepalive = keepalive_dafault;                 /*!< MQTT keep alive */	
#ifdef CONFIG_MQTT_CLIENT_SEND_BY_ENQUEUE
    /* 控制应用层允许积压的总量，实际消息数据会优先放到 PSRAM */
    open_mqtt_cfg.outbox.limit = 16 * 1024;
#endif

    ESP_LOGI(TAG, "********mqtt config info*********");
	if(open_mqtt_cfg.broker.address.uri) {
		ESP_LOGI(TAG, "hosturl:  %s",  open_mqtt_cfg.broker.address.uri);
	}
    ESP_LOGI(TAG, "hostport: %ld",  open_mqtt_cfg.broker.address.port);
    ESP_LOGI(TAG, "ClientId: %s",  open_mqtt_cfg.credentials.client_id);
    ESP_LOGI(TAG, "username: %s",  open_mqtt_cfg.credentials.username);
    ESP_LOGI(TAG, "password: %s",  open_mqtt_cfg.credentials.authentication.password);
    ESP_LOGI(TAG, "ifr_name: %s",   open_mqtt_cfg.network.if_name->ifr_name);
    ESP_LOGI(TAG, "keepalive: %d",  open_mqtt_cfg.session.keepalive);
    
    if (config->mqtt_client == NULL) {
        config->mqtt_client = esp_mqtt_client_init(&open_mqtt_cfg);
        if (config->mqtt_client != NULL) {
            /* The last argument may be used to pass data to the event handler, in this example open_mqtt_event_handler */
            ESP_ERROR_CHECK(esp_mqtt_client_register_event(config->mqtt_client, ESP_EVENT_ANY_ID, open_mqtt_event_handler, config));
            ESP_ERROR_CHECK(esp_mqtt_client_start(config->mqtt_client));
            ESP_LOGI(TAG, "init mqtt client and start");
            return 1;
        }
    } else {
        ESP_ERROR_CHECK(esp_mqtt_set_config(config->mqtt_client, &open_mqtt_cfg));
        ESP_ERROR_CHECK(esp_mqtt_client_start(config->mqtt_client));
		ESP_LOGI(TAG, "mqtt client start");
        return 1;
    }

    ESP_LOGI(TAG, "mqtt client start failed");
    return 0;
}

/*------------------------------------------------------------------------------
 Function: open_mqtt_msg_handle
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT接收数据处理
  * @param[in]  esp_mqtt_client_handle_t client  
                QueueHandle_t msg_queue          
                const char *rsp_topic            
  * @param[out] None
  * @return     void i
  */
 void open_mqtt_msg_handle(esp_mqtt_client_handle_t client, QueueHandle_t msg_queue, const char *rsp_topic ) 
{
    uint8_t Frame_type = 0;
    uint16_t plen = 0;
    const uint8_t *pdata = NULL;

    if ((client == NULL) || (msg_queue == NULL)) { /* mqtt 消息队列不存在 */
        ESP_LOGE(TAG, "client or mqtt msg queue");
        return ; /* 退出 */
    }

    queue_msg_t income = {NULL, 0};
    if(xQueueReceive(msg_queue, &income, 0) == pdTRUE )
	{                
        int rst = Modbus_Format_Check(income.msg_ptr, income.msg_len);
        if (rst < 0) {
            if (income.msg_ptr) free(income.msg_ptr);
            return;
        }

        uint8_t md_addr = income.msg_ptr[0];
        can_cmd_queue_struct can_cmd = { NULL, MODBUS_TO_CAN_MAX_NUM, 0 };
#ifdef CONFIG_CAN_PORT_ENABLE                
        if ((rst == 0x06 || rst == 0x10) && can_cmd_queue) {
            can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
            if (!can_cmd.cmd) {
                ESP_LOGE(TAG, "mqtt to can malloc failed");
                if (income.msg_ptr) free(income.msg_ptr);
                return;
            }
        }
#endif

        reg_position_list_t *position_list = NULL;
        uint8_t rsp_buf[270] = {0};
        int rsp_len = 0;    
    
        rsp_len = Modbus_Slave(income.msg_ptr, income.msg_len, rsp_buf, can_cmd.cmd, &can_cmd.num, MD_CHL_WIFI_CLOUD, &position_list);
        
        if (rsp_len > 0) {
            open_mqtt_send(rsp_buf, rsp_len);
    
            /* modbus指令转换为can指令发送到队列 */
            if (can_cmd.cmd) 
            {
#ifdef CONFIG_CAN_PORT_ENABLE
                sys_new_can_data_resend(&can_cmd, md_addr);
#else
                free(can_cmd.cmd);
#endif
            }

            if (NULL != position_list) {
#ifdef CONFIG_UART_PORT_ENABLE
                sys_new_position_and_transmit(position_list);
#else
                delete_set_position(position_list);  
#endif
            }
        } else {
            if (can_cmd.cmd) {
                free(can_cmd.cmd);
            }
            if (NULL != position_list) {
                delete_set_position(position_list);  
            }
        }
        
        if (income.msg_ptr) {
            free(income.msg_ptr);
        }
    }
}

 
 /*------------------------------------------------------------------------------
  Function: open_mqtt_cloud_dns
  -----------------------------------------------------------------------------*/
 /**
   * @brief      DNS解析，从URL中解析出主机名、端口和IP地址
   * @param[in]  const char *url - 输入URL，格式：host:port
   * @param[out] char *host - 输出主机名缓冲区
   * @param[out] uint16_t *port - 输出端口号
   * @param[out] uint8_t ipaddr[4] - 输出IPv4地址数组
   * @return     uint8_t (1-成功, 0-失败)
   */
 static uint8_t open_mqtt_cloud_dns(const char *url, char *host, uint16_t *port, uint8_t ipaddr[])
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
 
 /*------------------------------------------------------------------------------
  Function: open_mqtt_State_Machine
  -----------------------------------------------------------------------------*/
 /**
   * @brief      MQTT连接状态机
   * @param[in]  void  
   * @param[out] None
   * @return     static
   */
 void open_mqtt_State_Machine(void)
 {
     static bool wait_state = false;
     static uint32_t pre_wait_time = 0;
     static uint32_t wait_time = 0;
     
     /**
      * @brief MQTT登录逻辑
      * 1、解析主机域名
      * 2、发起登录
      * 3、处理MQTT交互信息
      */
     if (!iot_net_status_check()
         ||(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.ctrl_status.bit.system_off == 1)
         ||(open_mqtt_exit_flag)
         ||(open_mqtt_config.current_netif_id >= NETIF_TYPE_MAX)
         )
     {            
         if ( open_mqtt_config.mqtt_client ) {
             open_mqtt_step = STEP_MQTT_EXIT;
         } else {
             open_mqtt_step = STEP_MQTT_WAITING;
         }
     }
     else
     {
         /*等待状态结束，重新发起连接*/
         if ( open_mqtt_step == STEP_MQTT_WAITING || Open_Mqtt_Restart_Flag) {
             open_mqtt_step = STEP_MQTT_RESTART;
             
             // 状态机记录清空
             Open_Mqtt_Restart_Flag = false;
             Open_Mqtt_Machine_Pause_Time = 0;
             wait_state = false;
             pre_wait_time = 0;
             wait_time = 0;
         }
         
         /*判断是否有外部延时请求*/
         if ( Open_Mqtt_Machine_Pause_Time ) {
             wait_state = true;
             pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
             wait_time = Open_Mqtt_Machine_Pause_Time;
             Open_Mqtt_Machine_Pause_Time = 0;
         }
 
         /*延时等待执行下一步*/
         if ( wait_state == true ) {
             uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
             if ( (now_time - pre_wait_time) <= wait_time ) {
                 return;
             }
             
             wait_state = false;
             pre_wait_time = 0;
             wait_time = 0;
         }
     }
 
     switch (open_mqtt_step) 
     {
         case STEP_MQTT_WAITING:
             // 系统状态不满足云端任务启动条件，暂不执行
         break;
         
         /**
          * @brief 停止MQTT客服端
          */
         case STEP_MQTT_RESTART:
             open_mqtt_exit_process();
             open_mqtt_step = STEP_HOST_DNS;
         break;
 
         case STEP_HOST_DNS:  /* 域名解析 */
             ESP_LOGI(TAG, "mqtt client go to DNS");
             if (open_mqtt_cloud_dns(open_mqtt_config.raw_url, open_mqtt_config.host, &open_mqtt_config.port, open_mqtt_config.ipaddr)) {
                 open_mqtt_step = STEP_MQTT_LOGIN; /* 下一步 */
             } else {
                 // 域名解析失败10s后再尝试
                 wait_state = true;
                 wait_time = 10000;
                 pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
 
                 /* 网络在线重新进行mqtt登录否则退出 */
                 iot_netif_is_connected(open_mqtt_config.current_netif_id) ? (open_mqtt_step = STEP_HOST_DNS) : (open_mqtt_step = STEP_MQTT_CONNECT_FAIL);
             }
         break;
 
         /* 登录 */
         case STEP_MQTT_LOGIN:
             if (open_mqtt_login(&open_mqtt_config) == 1) 
             {
                 open_mqtt_config.mqtt_is_start = 1;
                 open_mqtt_step = STEP_MQTT_MSG_HANDLE;
                 ESP_LOGI(TAG, "mqtt client login successfully and go to message handle");
             } 
             else 
             {
                 ESP_LOGE(TAG,"STEP_MQTT_LOGIN failed");
                 
                 /* 网络在线重新进行mqtt登录否则退出 */
                 iot_netif_is_connected(open_mqtt_config.current_netif_id) ? (open_mqtt_step = STEP_MQTT_RESTART) : (open_mqtt_step = STEP_MQTT_CONNECT_FAIL);
 
                 // 失败10s后再尝试
                 wait_state = true;
                 wait_time = 10000;
                 pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
             }
         break;
 
         /* MQTT数据处理 */
         case STEP_MQTT_MSG_HANDLE:
             if (open_mqtt_config.mqtt_client == NULL) {
                 ESP_LOGE(TAG,"iot_tcp_connect_host:open_mqtt_config.mqtt_client == NULL");            
                 break;
             }
             open_mqtt_msg_handle(open_mqtt_config.mqtt_client, open_mqtt_config.mqtt_msg_queue, open_mqtt_config.pub_topic);
         break;
 
         case STEP_MQTT_CONNECT_FAIL:
             
             /*MQTT连接失败，预备重新连接*/
             open_mqtt_step = STEP_MQTT_RESTART;
         
         break;
         
         case STEP_MQTT_EXIT:
             
             /*已提前执行退出*/
             if(open_mqtt_exit_flag) break;
             
             /*MQTT断开*/
             open_mqtt_exit_process();
             open_mqtt_exit_flag = 1;
             
         break;
 
         default:  break;
     }
 }

void open_mqtt_start(void) 
{
    /* MQTT登录处理任务 */
    open_mqtt_exit_flag = 0;
    Open_Mqtt_Restart_Flag = true;
}
 
void set_open_mqtt_stop(void) 
{
    /*MQTT断开*/
    open_mqtt_exit_process();
	open_mqtt_exit_flag = 1;
}

void open_mqtt_new_dev(const char *dev_type, uint64_t dev_sn) {
    memcpy(open_mqtt_config.dev_type, dev_type, sizeof(open_mqtt_config.dev_type));
    open_mqtt_config.dev_sn = dev_sn;
    Open_Mqtt_Restart_Flag = true;
}

void open_mqtt_new_host(const char *new_host) {
    memcpy(open_mqtt_config.raw_url, new_host, sizeof(open_mqtt_config.raw_url));
    Open_Mqtt_Restart_Flag = true;
}

void open_mqtt_new_password(const char *new_password) {
    memcpy(open_mqtt_config.password, new_password, sizeof(open_mqtt_config.password));
    Open_Mqtt_Restart_Flag = true;
}

int open_mqtt_send(const uint8_t *data, int len)
{
	if ((!iot_net_status_check()) || (open_mqtt_config.mqtt_is_login == 0)) 
	{
	    ESP_LOGE(TAG,"net error or mqtt error");
		return -1;	// wifi未连接或mqtt未登录
	}
    
    if (!open_mqtt_semaphore || xSemaphoreTake(open_mqtt_semaphore, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG,"open_mqtt_semaphore error");
        return -1; 
    }

#ifdef CONFIG_MQTT_CLIENT_SEND_BY_ENQUEUE
    int ret = esp_mqtt_client_enqueue(open_mqtt_config.mqtt_client, open_mqtt_config.pub_topic, (const char *)data, len, 0, 0, true);
    if (ret == -2) {
        ESP_LOGE(TAG, "mqtt outbox full, topic:%s", open_mqtt_config.pub_topic);
    } else if (ret < 0) {
        ESP_LOGE(TAG, "topic:%s enqueue failed", open_mqtt_config.pub_topic);
    } else {
        reals.net_frame_tx_counter++;
    }
#else
    int ret = esp_mqtt_client_publish(open_mqtt_config.mqtt_client, open_mqtt_config.pub_topic, (const char *)data, len, 0, 0);
    if ( ret == -1 ) {
        ESP_LOGE(TAG, "topic:%s publish failed", open_mqtt_config.pub_topic);
    } else {
        reals.net_frame_tx_counter++;
    }
#endif
    
    xSemaphoreGive(open_mqtt_semaphore);  /*释放信号量*/
	return ret;
}

/**
 * @brief MQTT周期性上报设备信息
 *
 * 功能说明：
 * 1. 判断MQTT连接和上电时间，未满足条件不上报。
 * 2. 500ms为周期线程，控制上报频率。
 *
 * @return uint16_t 是否正在上报（1正在上报，0未上报）
 */
uint16_t Open_Mqtt_Module_Announce(void)
{
    static uint8_t sreport_step = 0;      // 上报阶段：0主机，1电池包
    static uint64_t send_ready_cnt = 0;   // 上报周期计数器
    static uint16_t sRunflag = 0;         // 上报运行标志
    uint16_t len = 0;                     // 本次上报数据长度

	/*以下状态下暂停上报*/
    if ((open_mqtt_config.mqtt_is_login == 0)                       // 云端未连接
        || (1 != IotSetData.dev_info_t.open_mqtt_enable.report_en)  // 上报未使能
        || (sys_is_updating())                                      // 升级中
        || (reals.MCUPoweronCnt < 30)) return 0;

    // 1s周期线程控制
    static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if ((now_time - pre_time) >= 1000) {
        pre_time = now_time;
    } else {
        return 0;
    }

    // 设置周期
    uint64_t ReportCycle = (IotSetData.dev_info_t.open_mqtt_report_cycle > 0) 
                                ? IotSetData.dev_info_t.open_mqtt_report_cycle : 60;
        
    // 周期未到则不上报
    if (!sRunflag) {
        if (++send_ready_cnt < ReportCycle) return 0;
        send_ready_cnt = 0;
        sRunflag = 1;     
    }

    switch ( sreport_step )
    {
        case 0 :
            len = Modbus_MasterWriteCmd_06H_10H(MOD_REG_START_ADDR_11000, 100, false, open_mqtt_tx_buf, 0, MD_CHL_WIFI_CLOUD);
            sreport_step++;
            break;    
        
        default:
            
            // 所有阶段完成，重置状态
            sreport_step = 0;
            sRunflag = 0;
            return sRunflag;
    }
    
    if ( len > 0 ) {
        open_mqtt_send(open_mqtt_tx_buf, len);
    }
    
    return sRunflag;    
}

#endif
