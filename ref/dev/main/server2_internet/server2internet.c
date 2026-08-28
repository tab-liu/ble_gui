/*


*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "esp_log.h"
//#include "sdkconfig.h"
#include "pppos_client_main.h"
//#include "custom_module.hpp"
//#include "Cellular_4g_at.h"
#include "iot_period_task.h"
#include "esp_err.h"
#include "server2internet.h"
#include "iot_period_task.h"
#include "filesystem.h"
#include "iot_partition.h"
#include "comm_define.h"
#include "modbus_slave.h"
#include "iot_mqtt.h"
#include "can_protocol.h"

#include "mesh_api.h"
#include "iot_wifi_init.h"


#define     TAG       								"[MQTTC2]"


enum {
    STEP_MQTT_RESTART = 0,
    STEP_HOST_DNS = 1,
    STEP_CREATE_TCP = 2,
//    STEP_GET_HOST_TIME = 3,
    STEP_MQTT_LOGIN = 4,
    STEP_MQTT_MSG_HANDLE = 5,
    STEP_MQTT_EXIT = 6,
    STEP_MQTT_CONNECT_FAIL = 7,
} ;


EXT_RAM_BSS_ATTR  uint8_t  login2_step;//WIFI通道的MQTT登陆状态机
esp_mqtt_client_config_t mqtt2_cfg;//windy change to global
EXT_RAM_BSS_ATTR  login_info_t login2_info;//static
EXT_RAM_BSS_ATTR static struct esp_mqtt_client_dump mqtt_client_dump2;
static TimerHandle_t timeout_tmr2;

static void iot_switch_netif(void);
static uint8_t iot_netif_is_connected(void);
static void iot_mqtt_send2(uint8_t *data, int len);

static void timeout_timer_cb2(TimerHandle_t xTimer)
{
	login2_info.mqtt_is_login = 0;
	login2_step = STEP_MQTT_RESTART;  // 重新开始连接
}

static void timeout_timer_start2(void)
{
	/* MQTT订阅响应超时时间定义 */
	#define MQTT_SUB_RESP_TIMEOUT			10000

	if (timeout_tmr2)
	{
		xTimerStart(timeout_tmr2, 0);
		return;
	}

	/* 创建通信超时定时器 */
	timeout_tmr2 = xTimerCreate("mqtt sub timer2", pdMS_TO_TICKS(MQTT_SUB_RESP_TIMEOUT), pdFALSE, NULL, timeout_timer_cb2);
	if (timeout_tmr2 == NULL)
	{
		ESP_LOGE(TAG, "mqtt subscribe timeout timer create failed, Restarting");
		esp_restart();
		return;
	}
	xTimerStart(timeout_tmr2, 0);
}

static void timeout_timer_stop2(void)
{
	if (timeout_tmr2)
		xTimerStop(timeout_tmr2, 0);
}


static void mqtt_event_handler2(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    login_info_t *login_info = (login_info_t *)handler_args;
    int msg_id = 0;
	char all_public_topic[100];

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "mqtt connected");
			memcpy(&mqtt_client_dump2, client, sizeof(struct esp_mqtt_client_dump));
//			iot_bind_check();										// 检查系统绑定信息
//			bind_dev_list.need_bind = 1;							// 每次登录都要绑定一次信息
//			tcp_bind_dev();											// 通过tcp方式绑定设备
			// tcp_send_iot_param(true);								// 通过tcp方式发送iot参数

            snprintf(login_info->sub_topic, sizeof(login_info->sub_topic), SUBSCRIBE_TOPIC, login_info->dev_type, login_info->dev_sn);
            snprintf(login_info->pub_topic, sizeof(login_info->pub_topic), PUBLISH_TOPIC,   login_info->dev_type, login_info->dev_sn);
			snprintf(all_public_topic, sizeof(all_public_topic), ALL_PUBLIC_TOPIC,  login_info->dev_type);

            msg_id = esp_mqtt_client_subscribe(client, all_public_topic, 0);		// 订阅公共主题
            ESP_LOGI(TAG, "subscribe public topic, msg_id=%d topic: %s", msg_id, all_public_topic);

            msg_id = esp_mqtt_client_subscribe(client, login_info->sub_topic, 0);	// 订阅接收服务器命令主题
            ESP_LOGI(TAG, "subscribe receive topic, msg_id=%d topic: %s", msg_id, login_info->sub_topic);
			ESP_LOGI(TAG, "data publish topic: %s, and waiting for MQTT_EVENT_SUBSCRIBED", login_info->pub_topic); // 数据发送主题
			timeout_timer_start2();
            break;

        case MQTT_EVENT_DISCONNECTED: // MQTT链接断开事件
			memset(&mqtt_client_dump2, 0x00, sizeof(struct esp_mqtt_client_dump));
            login_info->mqtt_is_login = 0;
            login2_step = STEP_MQTT_CONNECT_FAIL;   // 重新开始连接
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            vTaskDelay(pdMS_TO_TICKS(2000));
            break;

        case MQTT_EVENT_SUBSCRIBED: /* 订阅完成 */
			timeout_timer_stop2();
            login_info->mqtt_is_login = 1;
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            break;

        case MQTT_EVENT_UNSUBSCRIBED: /* 取消订阅 */
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            // ESP_LOGI(TAG, "topic: %s recv message", event->topic);
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
            login_info->mqtt_is_login = 0;
            login2_step = STEP_MQTT_RESTART; // MQTT登录流程重新开始
            break;

        default: ESP_LOGI(TAG, "Other event id:%d", event->event_id); break;
    }
}

static uint8_t iot_mqtt_login2(login_info_t *login) 
{

    if (login == NULL) 
	{
        return 0;
    }

#ifdef     TCP2_ENCRYPT_ENABLE	
	char host[150] = "mqtts://";
#else
	char host[150] = "mqtt://";

#endif	
    char username[64] = {0};
    char password[100] = {0};

	

    strcat(host, login->host);//追加字符串尾部
    snprintf(username, sizeof(username), "%s%llu", login->iot_type, login->iot_sn);
	memcpy(&password, &SetData.dev_info_t.Net_Server_secret, sizeof(SetData.dev_info_t.Net_Server_secret));
#if 0
    esp_mqtt_client_config_t mqtt2_cfg = {
        .uri  = host,               /*!< MQTT host */
        .port = login->port,        /*!< MQTT port */
        .username = username,       /*!< MQTT username */
        .password = password,       /*!< MQTT password */
        .client_id = username,      /*!< MQTT clientId */
        .disable_auto_reconnect = false, /*!< MQTT auto reconnect  */
        .keepalive = 50,            /* 50s */
    };
#endif
//windy copy from A100
//    esp_mqtt_client_config_t 
        mqtt2_cfg.broker.address.uri = host;//mqtt_uri,                  /*!< MQTT host */
#ifdef     TCP2_ENCRYPT_ENABLE	
			mqtt2_cfg.broker.address.port = 18883;//第二个MQTTS的端口
#else
			mqtt2_cfg.broker.address.port = login->port;//18083 
		
#endif	

		
#if !MQTT_DEBUG
        mqtt2_cfg.credentials.username = username;                /*!< MQTT username */        
        mqtt2_cfg.credentials.authentication.password = password; /*!< MQTT password */
#endif
        mqtt2_cfg.credentials.client_id = username;//clientId,               /*!< MQTT clientId */
        mqtt2_cfg.network.disable_auto_reconnect = false;         /*!< MQTT auto reconnect  */
        mqtt2_cfg.session.keepalive = 50;//60,                         /*!< MQTT keep alive */
#ifdef     TCP2_ENCRYPT_ENABLE	
        mqtt2_cfg.credentials.authentication.certificate = (const char *)iot_cert_ptr_2;  //客户端证书
        mqtt2_cfg.credentials.authentication.key = (const char *)private_key_ptr_2;  //客户端密钥

#else
#endif	


	// /* 自搭建的EMQX MQTT服务器测试 */
	// mqtt2_cfg.uri = NULL;
	// mqtt2_cfg.port = 1883;
	// mqtt2_cfg.host = "120.78.80.117";
	// ESP_LOGI(TAG, "host ip addr:  %s",  mqtt2_cfg.host);

    //ESP_LOGI(TAG, "********mqtt login info*********");
	if(mqtt2_cfg.broker.address.uri) {
		ESP_LOGI(TAG, "hosturl:  %s",  mqtt2_cfg.broker.address.uri);
	}
    ESP_LOGI(TAG, "hostport: %ld",  mqtt2_cfg.broker.address.port);
    ESP_LOGI(TAG, "ClientId: %s",  mqtt2_cfg.credentials.client_id);
    ESP_LOGI(TAG, "username: %s",  mqtt2_cfg.credentials.username);
    ESP_LOGI(TAG, "password: %s",  mqtt2_cfg.credentials.authentication.password);

    if (login->mqtt_client == NULL) 
	{
        login->mqtt_client = esp_mqtt_client_init(&mqtt2_cfg);
        if (login->mqtt_client != NULL) 
		{
            /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
            ESP_ERROR_CHECK(esp_mqtt_client_register_event(login->mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler2, login));
            ESP_ERROR_CHECK(esp_mqtt_client_start(login->mqtt_client));
            ESP_LOGI(TAG, "init mqtt client and start");
            return 1;
        }
    } else {
        ESP_ERROR_CHECK(esp_mqtt_set_config(login->mqtt_client, &mqtt2_cfg));
        ESP_ERROR_CHECK(esp_mqtt_client_start(login->mqtt_client));
		ESP_LOGI(TAG, "mqtt client start");
        return 1;
    }

    ESP_LOGI(TAG, "mqtt client start failed");
    return 0;
}


//extern factory_struct iot_factory;

#if 1
int16_t Server2_MQTT_Send_Public_Data(void)
{
	int16_t ret=0;
	uint8_t u8tempdata[30];
	static uint16_t sDelaycnt = 0;//
//    memcpy(u8tempdata, "windy hello !!?123", sizeof("windy hello !!?123"));
	
//    memcpy(u8tempdata, (uint8_t *)&iot_factory.iot_sn, sizeof(iot_factory.iot_sn));
	


//	if(1 == reals.st_FlagTime.bits.flagA_1s)
	{
//		reals.st_FlagTime.bits.flagA_1s =0;
		if(++sDelaycnt >= TIME_BASE_100MS_CNT_10S)//10s
		{
    		if((1 == login2_info.mqtt_is_login)
    			&&(STEP_MQTT_MSG_HANDLE == login2_step))
    		{
				memcpy(login2_info.pub_topic, CONFIG_EXAMPLE_MQTT_TEST_TOPIC2, sizeof(CONFIG_EXAMPLE_MQTT_TEST_TOPIC2));
				 sprintf((char *)u8tempdata,"SN:%s,%lld",iot_factory.iot_type,iot_factory.iot_sn);//sys_info
    		
    			ret =esp_mqtt_client_publish(login2_info.mqtt_client, login2_info.pub_topic, (const char *)u8tempdata,	sizeof(u8tempdata), 0, 0);
    		}
			sDelaycnt= 0;
		}

	}


//login_info.mqtt_client, login_info.pub_topic

	
	return ret;
}
#else

static void pack_data_head2(uint8_t ver, uint8_t *head, uint8_t cause, uint16_t cycel, uint8_t total, uint8_t seq) {
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

int16_t Server2_MQTT_Send_Public_Data(void)
{
	int16_t ret=0;
	static uint32_t sDelaycnt = 0;//
	uint32_t ReprotCycle = 0;//	
    uint8_t tx_buff[1024];
        
    if((1 == SetData.dev_info_t.on_off.bit._4g_enable)&&(ETH_GET_IP_READY ==reals.Cellular4G_step))//限制流量
    {
        ReprotCycle = SetData.dev_info_t.data_report_cycle;
    }
    else
    {
        ReprotCycle = 20;//20s
    }
    
    if( (0 == login2_info.mqtt_is_login) // wifi未连接或mqtt未登录
        ||((GET_IP_4G_READY != reals.Cellular4G_step)&&(1 == SetData.dev_info_t.on_off.bit._4g_enable))
        ||((0 == reals.wifi_connect_ap)&&(1 == SetData.dev_info_t.on_off.bit.wifi_enable))
        ||((ETH_GET_IP_READY != reals.Ethernet_SPI_step)&&(1 == SetData.dev_info_t.on_off.bit.Eth_enable))  
    )  
    {
        return 0; 
    }
    
    uint16_t start_addr;
    uint16_t reg_num;
    static uint16_t send_cnt = 0;
    static uint16_t send_ready_cnt = 0;
    static uint16_t cycle = 1;

    /*未连接直接返回*/
    if (login2_info.mqtt_is_login == 0) return 0;

    /*上报一次当前数据*/
    if (++send_ready_cnt < ReprotCycle) return 0;
    
    switch (send_cnt)
    {
        case 0: start_addr = MOD_REG_START_ADDR_00000; reg_num = 21; break;
        case 1: start_addr = MOD_REG_START_ADDR_00100; reg_num = 67; break;
        case 2: start_addr = MOD_REG_START_ADDR_01100; reg_num = MOD_REG_LEN_01100; break;
        case 3: start_addr = MOD_REG_START_ADDR_01200; reg_num = 16; break;
        case 4: start_addr = MOD_REG_START_ADDR_01300; reg_num = 17; break;
        case 5: start_addr = MOD_REG_START_ADDR_01400; reg_num = 34; break;
        case 6: start_addr = MOD_REG_START_ADDR_01500; reg_num = 14; break;
        case 7: start_addr = MOD_REG_START_ADDR_02000; reg_num = 68; break;
        case 8: start_addr = MOD_REG_START_ADDR_02200; reg_num = MOD_REG_LEN_02200; break;
        case 9: start_addr = MOD_REG_START_ADDR_06000; reg_num = MOD_REG_LEN_06000; break;
        case 10: start_addr = MOD_REG_START_ADDR_06100; reg_num = MOD_REG_LEN_06100; break;
        case 11: start_addr = MOD_REG_START_ADDR_07000; reg_num = MOD_REG_LEN_07000; break;
        case 12: start_addr = MOD_REG_START_ADDR_11000; reg_num = MOD_REG_LEN_11000; break;
        case 13: start_addr = MOD_REG_START_ADDR_12000; reg_num = MOD_REG_LEN_12000; break;
        default: 
            send_cnt = 0; 
            send_ready_cnt = 0;
            if (cycle == 65535) cycle = 0;
            cycle++; 
            return 0;
    }
    send_cnt++;
    
    pack_data_head2(0x01, tx_buff, 1, cycle, 14, send_cnt); // 10字节 载荷V1.2
    uint16_t len = Modbus_MasterWriteCmd_06H_10H(start_addr, reg_num, false, tx_buff + 10, 1);
    

    if (len > 0) 
    {
        ret =esp_mqtt_client_publish(login2_info.mqtt_client, CONFIG_EXAMPLE_MQTT_TEST_TOPIC2, (const char *)tx_buff, (len + 10), 0, 0);
    }

	return ret;
}

#endif
void server2_config_info_update(void) 
{


//	同步 modbus读和写变量，有差异，则更新
//	if(0 != strncmp(g_device_data.iot_dev_node.iot_config.net_server_2rd.Net_Server_address, g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_address, sizeof(g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_address)))//不相等
//	{
//		memcpy(g_device_data.iot_dev_node.iot_config.net_server_2rd.Net_Server_address, g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_address, sizeof(g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_address));

		iot_mqtt_new_host2((const char *)Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address);
		login2_info.mqtt_is_login = 0;
		login2_info.mqtt_is_start = 0;

//	}
	
//	if(0 != strncmp( g_device_data.iot_dev_node.iot_config.net_server_2rd.Net_Server_secret, g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_secret, sizeof(g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_secret)))//不相等
//	{
//		memcpy(g_device_data.iot_dev_node.iot_config.net_server_2rd.Net_Server_secret, g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_secret, sizeof(g_device_data.bk_iot_dev_node.iot_config.net_server_2rd.Net_Server_secret));

		login2_step = STEP_MQTT_RESTART;
		login2_info.mqtt_is_login = 0;
		login2_info.mqtt_is_start = 0;

//	}
	g_self_data.mod_reg22000_net_server_2rd.iot_enable.word = Inv_WR.mod_reg22000_net_server_2rd.iot_enable.word;


}


static void iot_mqtt_msg_handle2(esp_mqtt_client_handle_t client, QueueHandle_t msg_queue, const char *rsp_topic, QueueHandle_t can_cmd_queue) 
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
        pdata = (const uint8_t *)(income.msg_ptr);
        plen = income.msg_len;

		int rst = Modbus_Format_Check(pdata, plen);
		if (rst >= 0)//功能码
		{
			uint8_t md_addr = pdata[0];
			can_cmd_queue_struct can_cmd = {NULL, 10, 0};
			if ((rst == 0x06 || rst == 0x10) && can_cmd_queue) 
			{  // 当MODBUS为设置指令时,才需要开辟空间
				can_cmd.cmd = heap_caps_malloc(sizeof(can_data_label) * can_cmd.num, MALLOC_CAP_SPIRAM); /*  */
				if (can_cmd.cmd == NULL) {
					ESP_LOGE (TAG, "ble to can malloc failed");
				}
			}

			wifi_sta_config_check((uint8_t *)pdata);
	
			reg_position_list_t *position_list = NULL;

            int rsp_len = Modbus_Slave(pdata, plen, rsp_buf, can_cmd.cmd, &can_cmd.num, MD_CHL_WIFI_CLOUD, &position_list); // modbus协议解析
            if (rsp_len > 0)
			{
                if (esp_mqtt_client_publish(client, rsp_topic, (const char *)rsp_buf, rsp_len, 0, 0) == -1) {
                    ESP_LOGE(TAG, "topic:%s publish failed", rsp_topic);
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
                }
                else if (NULL != position_list) 
				{
//					sys_new_position_and_transmit(position_list);//透传转发给下级uart
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

    if (income.msg_ptr) {
        free(income.msg_ptr); /* 释放开辟出的缓存空间 */
    }
}


/*------------------------------------------------------------------------
*@Function： iot_mqtt_client_task
WiFi网络连后MQTT登录逻辑
第二服务器，对公服务器

-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void iot_mqtt_client_task2(void * pvParameters) 
{

    QueueHandle_t can_cmd_queue = (QueueHandle_t)pvParameters;
/*
	周未来内网MQTT服务器：192.168.1.9 
	dev-iot.poweroak.ltd:18083
	管理：18083 
	默认：1883
	用户： admin
	密码： admin123456


	*/



    if (login2_info.mqtt_msg_queue == NULL) 
	{
        login2_info.mqtt_msg_queue = xQueueCreate(10, sizeof(queue_msg_t));
        if (login2_info.mqtt_msg_queue == NULL) 
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

    login2_step = STEP_MQTT_EXIT;
    reals.wifi_connect_ap = 0;
	login2_info.sock = -1;
    login2_info.mqtt_client = NULL;


	//windy add debug
//    wifi_connect_ap = 1;
//    login2_step = STEP_MQTT_RESTART;
	login2_info.current_netif_id =NETIF_TYPE_WIFI_STA;//只有WIFI，不切换

    set_mqtt_client2_netif(login2_info.current_netif_id);

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
        vTaskDelay(pdMS_TO_TICKS(100));


		
        // if (((0 == reals.wifi_connect_ap)&&(1 == SetData.dev_info_t.on_off.bit.wifi_enable)) 
		// 	||((GET_IP_4G_READY != reals.Cellular4G_step)&&(1 == SetData.dev_info_t.on_off.bit._4g_enable))
		// 	||((ETH_GET_IP_READY != reals.Ethernet_SPI_step)&&(1 == SetData.dev_info_t.on_off.bit.Eth_enable)) 

		// )
		// {
        //     login2_step = STEP_MQTT_EXIT;
        // }
//		iot_mqtt_announce();
//		ESP_LOGE(TAG, "windy in iot_mqtt_client_task!!!!!!!!!!");
			reals.debug4  =login2_step;

        switch (login2_step) 
		{
            /**
             * @brief 停止MQTT客服端
             */
            case STEP_MQTT_RESTART:
                vTaskDelay(pdMS_TO_TICKS(100));
				memset(&mqtt_client_dump2, 0x00, sizeof(struct esp_mqtt_client_dump));
                login2_info.mqtt_is_login = 0;
                login2_info.mqtt_is_start = 0;
                if (login2_info.mqtt_client != NULL)
                {
                    esp_mqtt_client_stop(login2_info.mqtt_client);
                    esp_mqtt_client_destroy(login2_info.mqtt_client);
                    login2_info.mqtt_client = NULL;
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
                ESP_LOGI(TAG, "mqtt client go to DNS");
                login2_step = STEP_HOST_DNS;
            break;

            case STEP_HOST_DNS:  /* 域名解析 */
                vTaskDelay(pdMS_TO_TICKS(20));
				ESP_LOGI(TAG, "STEP_HOST_DNS login2_info.raw_url=%s ",login2_info.raw_url);
				ESP_LOGI(TAG, "STEP_HOST_DNS login2_info.host=%s ",login2_info.host);
				ESP_LOGI(TAG, "STEP_HOST_DNS login2_info.port=%d ",login2_info.port);
				ESP_LOGI(TAG, "STEP_HOST_DNS login2_info.ipaddr=%s ",login2_info.ipaddr);
				ESP_LOGI(TAG, "STEP_HOST_DNS login2_info.mqtt_connect_fail=%d ",login2_info.mqtt_connect_fail);

			
                if (iot_get_cloud_dns(login2_info.raw_url, login2_info.host, &login2_info.port, login2_info.ipaddr) == 1) 
				{
                    login2_step = STEP_CREATE_TCP; /* 下一步 */
					ESP_LOGI(TAG, "mqtt client go to TCP connect");
                } 
				else 
				{
                    vTaskDelay(pdMS_TO_TICKS(5000)); // 域名解析失败5s后在尝试
                    login2_step = STEP_MQTT_CONNECT_FAIL;
					// if (((0 == reals.wifi_connect_ap)&&(1 == SetData.dev_info_t.on_off.bit.wifi_enable)) 
					// 	||((GET_IP_4G_READY != reals.Cellular4G_step)&&(1 == SetData.dev_info_t.on_off.bit._4g_enable))
					// 	||((ETH_GET_IP_READY != reals.Ethernet_SPI_step)&&(1 == SetData.dev_info_t.on_off.bit.Eth_enable)) 

					// )
					// {
					// 	login2_step = STEP_MQTT_EXIT;	// wifi不在线退出
					// }

                }
            break;

            case STEP_CREATE_TCP: /* 建立tcp连接 */
                vTaskDelay(pdMS_TO_TICKS(20));
                if (iot_tcp_connect_host(login2_info.current_netif_id, (const char *)login2_info.ipaddr, login2_info.port, &login2_info.sock) == 1) 
				{
                    login2_step = STEP_MQTT_LOGIN; /* 下一步 */
					ESP_LOGI(TAG, "mqtt client go to login");
				
                } 
				else 
				{
					ESP_LOGW(TAG, "WW  STEP_CREATE_TCP else");
					
                    vTaskDelay(pdMS_TO_TICKS(5000));
					/* wifi在线回到上一步否则退出 */
					// if(1 == SetData.dev_info_t.on_off.bit.wifi_enable)
					{
						(iot_netif_is_connected()) ? (login2_step = STEP_HOST_DNS) : (login2_step = STEP_MQTT_CONNECT_FAIL);
					}
                }
            break;

            

            /* 计算MQTT登录密码,并且登录 */
            case STEP_MQTT_LOGIN:
				/* 按照登录流程计算两个密码,MQTT登录用户名与密码如下格式:
				 * username: EBOX132564873121(SN)
				 * passwrod: password,IOT@password2 */

                vTaskDelay(pdMS_TO_TICKS(20));
                if (iot_mqtt_login2(&login2_info) == 1) 
				{
                    login2_info.mqtt_is_start = 1;
                    login2_step = STEP_MQTT_MSG_HANDLE;
					ESP_LOGI(TAG, "mqtt client login successfully and go to message handle");
                } 
				else 
				{
					ESP_LOGE(TAG, "mqtt client login failed");
                    vTaskDelay(pdMS_TO_TICKS(5000));
					/* wifi在线重新进行mqtt登录否则退出 */
					// if(1 == SetData.dev_info_t.on_off.bit.wifi_enable)
					{
						(iot_netif_is_connected()) ? (login2_step = STEP_MQTT_RESTART) : (login2_step = STEP_MQTT_CONNECT_FAIL);
					}						
                }
            break;

            /* MQTT数据处理 */
            case STEP_MQTT_MSG_HANDLE:
                if (login2_info.mqtt_client == NULL) 
				{
                    break;
                }

				/* 设备产生了核心设置变化,IOT主动发布事件信息给到后台 */
//                iot_mqtt_dev_event_ann();
                iot_mqtt_msg_handle2(login2_info.mqtt_client, login2_info.mqtt_msg_queue, login2_info.pub_topic, can_cmd_queue);
            break;

            case STEP_MQTT_CONNECT_FAIL:
            {
                login2_info.mqtt_connect_fail++;
                //网卡不在线，直接切换，否则重试达到次数后再切换
                if (MQTT_RECONNECT_COUNT <= login2_info.mqtt_connect_fail || 0 == iot_netif_is_connected())
                {
                    iot_switch_netif();
                    ESP_LOGI(TAG, "STEP_MQTT_CONNECT_FAIL switch to:%u, set_mqtt_client2_netif:%s",
                                        login2_step, mqtt2_cfg.network.if_name->ifr_name);
                }
                vTaskDelay(pdMS_TO_TICKS(5000));
                login2_step = STEP_MQTT_RESTART;
            }
            break;

            case STEP_MQTT_EXIT:
                memset(&mqtt_client_dump2, 0x00, sizeof(struct esp_mqtt_client_dump));
                login2_info.mqtt_is_login = 0;
                login2_info.mqtt_is_start = 0;
                if (login2_info.mqtt_client != NULL) 
				{
                    esp_mqtt_client_stop(login2_info.mqtt_client);
                    esp_mqtt_client_destroy(login2_info.mqtt_client);
                    login2_info.mqtt_client = NULL;
                    ESP_LOGW(TAG, "mqtt client stop");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
            break;

            default:  break;
        }
    }
}

void iot_mqtt2_delete(uint8_t mode) 
{
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
        ESP_LOGE(TAG, " wifi_connect_ap2:%u", reals.wifi_connect_ap);
	}

	if(NETIF_TYPE_PPP == mode && 1 == SetData.dev_info_t.on_off.bit._4g_enable)
	{
		reals.Cellular4G_step = LOST_IP_4G_READY;
        ESP_LOGE(TAG, " Cellular4G_step2:%u", reals.Cellular4G_step);
	}			

	if(NETIF_TYPE_ETH == mode && 1 == SetData.dev_info_t.on_off.bit.Eth_enable)
	{
		reals.Ethernet_SPI_step = ETH_LOST_IP;
        ESP_LOGE(TAG, " Ethernet_SPI_step2:%u", reals.Ethernet_SPI_step);
	}

    // reals.wifi_connect_ap = 0;
    if (mode == login2_info.current_netif_id)
    {
        login2_step = STEP_MQTT_CONNECT_FAIL;
    }
}

void iot_mqtt2_start(uint8_t mode) {
    ESP_LOGI(TAG, " iot_mqtt_start2:%u, current_netif_id:%u", mode, login2_info.current_netif_id);

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
        ESP_LOGI(TAG, " wifi_connect_ap2:%u", reals.wifi_connect_ap);
	}

	if(NETIF_TYPE_PPP == mode && 1 == SetData.dev_info_t.on_off.bit._4g_enable)
	{
		reals.Cellular4G_step = GET_IP_4G_READY;
        ESP_LOGI(TAG, " Cellular4G_step2:%u", reals.Cellular4G_step);
	}			

	if(NETIF_TYPE_ETH == mode && 1 == SetData.dev_info_t.on_off.bit.Eth_enable)
	{
		reals.Ethernet_SPI_step = ETH_GET_IP_READY;
        ESP_LOGI(TAG, " Ethernet_SPI_step2:%u", reals.Ethernet_SPI_step);
	}		

    // 优先级更低的网卡上线，不切换MQTT联网方式
    if(mode > login2_info.current_netif_id)
    {
        return;
    }

    login2_info.current_netif_id = mode;
    set_mqtt_client2_netif(login2_info.current_netif_id);
    /* MQTT登录处理任务 */
    login2_step = STEP_MQTT_RESTART;
}

 void set_mqtt2_stop(void) 
{
//	 xSemaphoreTake(xMqttSemaphore,  portMAX_DELAY);
    if (0 == login2_info.mqtt_is_start)
    {
        return;
    }
    esp_mqtt_client_stop(login2_info.mqtt_client);
	 esp_err_t err = esp_mqtt_client_destroy(login2_info.mqtt_client);
	 mqtt_client_dump2.transport = NULL;
	 login2_info.mqtt_client = NULL;
	 login2_info.mqtt_is_login = 0;
	 login2_info.mqtt_is_start = 0;
	 
//	 login_info->client_connected = 0;
//	 xSemaphoreGive(xMqttSemaphore);
	 
	 if (err == ESP_ERR_INVALID_ARG) {
		 ESP_LOGI(__func__, "mqtt client2 yet destroy");
	 } else if (err == ESP_OK){
		 ESP_LOGI(__func__, "mqtt client2 destroy done");
	 } else {
	 }
//	 return 1;
//	 ESP_LOGE(TAG,"mqtt stop");
	 // if (mqtt_task_handle != NULL) {
	 // 	vTaskSuspend(mqtt_task_handle); // 挂起MQTT任务
	 // }
	 login2_step = STEP_MQTT_CONNECT_FAIL;	 // wifi不在线退出
 }

void set_mqtt_client2_netif(uint8_t netif_type)
{
    char *netif_key[2] = { NETIF_KEY_WIFI_STA,NETIF_KEY_WIFI_AP};//{NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP};
#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[netif_type]),
                                    login2_info.netif_req.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[netif_type])),
                                login2_info.netif_req.ifr_name);
#endif
    mqtt2_cfg.network.if_name = &(login2_info.netif_req);
    ESP_LOGE(TAG, "set_mqtt_client2_netif:%s", mqtt2_cfg.network.if_name->ifr_name);
}

static void iot_switch_netif(void)
{
    // if (1 == iot_netif_is_connected())
//    {
//        login2_info.current_netif_id++;
//        if (2 < login2_info.current_netif_id)
//        {
//            login2_info.current_netif_id = 0;
//        }
//    }
	login2_info.current_netif_id =NETIF_TYPE_WIFI_STA;//只有WIFI，不切换

//    set_mqtt_client2_netif(login2_info.current_netif_id);
    login2_step = STEP_MQTT_RESTART;
    ESP_LOGE(TAG, "login2_info.current_netif_id:%d", login2_info.current_netif_id);
	
}

static uint8_t iot_netif_is_connected(void)
{
    if (NETIF_TYPE_ETH == login2_info.current_netif_id)
    {
        return ((ETH_GET_IP_READY == reals.Ethernet_SPI_step)?1:0);
    }
    else if (NETIF_TYPE_WIFI_STA == login2_info.current_netif_id)
    {
        return ((1 == reals.wifi_connect_ap)?1:0);
    }
    else if (NETIF_TYPE_PPP == login2_info.current_netif_id)
    {
        return ((GET_IP_4G_READY == reals.Cellular4G_step)?1:0);
    }

    return 0;
}

static void iot_mqtt_send2(uint8_t *data, int len)
{
    ESP_LOGI(TAG, " 22222 iot mqtt login:%u topic:[%s] send:[%s]", login2_info.mqtt_is_login, login2_info.pub_topic, data);
    //(iot_netif_is_connected() == 0) || 
	if ((login2_info.mqtt_is_login == 0)) 
	{
		return;	// wifi未连接或mqtt未登录
	}
    ESP_LOGI(TAG, " 2222 iot mqtt topic:[%s] send:[%s]", login2_info.pub_topic, data);
	esp_mqtt_client_publish(login2_info.mqtt_client, login2_info.pub_topic, (const char *)data, len, 0, 0);
}
