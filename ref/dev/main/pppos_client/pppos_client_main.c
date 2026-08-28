/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "esp_modem_api.h"
#include "esp_log.h"
//#include "sdkconfig.h"
#include "pppos_client_main.h"
//#include "custom_module.hpp"
#include "Cellular_4g_at.h"
#include "iot_period_task.h"
#include "esp_err.h"
#include "iot_mqtt.h"
#include "iot_wifi_init.h"

#include "uart_device_process.h"
#include "server2internet.h"

extern  esp_err_t esp_modem_schedule_before(esp_modem_dce_t *dce_wrap);


#if defined(CONFIG_EXAMPLE_FLOW_CONTROL_NONE)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_NONE
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_SW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_SW
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_HW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW
#endif


static const char *TAG = "[pppos_example]";
//static EventGroupHandle_t event_group = NULL;
//static const int CONNECT_BIT = BIT0;
//static const int GOT_DATA_BIT = BIT2;
//static const int USB_DISCONNECTED_BIT = BIT3; // Used only with USB DTE but we define it unconditionally, to avoid too many #ifdefs in the code


#define CONNECT_BIT             BIT0
#define GOT_DATA_BIT            BIT2
#define USB_DISCONNECTED_BIT    BIT3

#ifdef CONFIG_EXAMPLE_MODEM_DEVICE_CUSTOM
esp_err_t esp_modem_get_time(esp_modem_dce_t *dce_wrap, char *p_time);
#endif
//
//
//
//
//
//static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
//{
//    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIu32, base, event_id);
//    esp_mqtt_event_handle_t event = event_data;
//    esp_mqtt_client_handle_t client = event->client;
//    int msg_id;
//    switch ((esp_mqtt_event_id_t)event_id) {
//    case MQTT_EVENT_CONNECTED:
//        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
//        msg_id = esp_mqtt_client_subscribe(client, CONFIG_EXAMPLE_MQTT_TEST_TOPIC, 0);
//        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
//        break;
//    case MQTT_EVENT_DISCONNECTED:
//        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
//        break;
//    case MQTT_EVENT_SUBSCRIBED:
//        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
//        msg_id = esp_mqtt_client_publish(client, CONFIG_EXAMPLE_MQTT_TEST_TOPIC, CONFIG_EXAMPLE_MQTT_TEST_DATA, 0, 0, 0);
//        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//        break;
//    case MQTT_EVENT_UNSUBSCRIBED:
//        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
//        break;
//    case MQTT_EVENT_PUBLISHED:
//        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
//        break;
//    case MQTT_EVENT_DATA:
//        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
//        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
//        printf("DATA=%.*s\r\n", event->data_len, event->data);
////        xEventGroupSetBits(event_group, GOT_DATA_BIT);
//        break;
//    case MQTT_EVENT_ERROR:
//        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
//        break;
//    default:
//        ESP_LOGI(TAG, "MQTT other event id: %d", event->event_id);
//        break;
//    }
//}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t **p_netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", *p_netif);
    }
}


static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) 
	{
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
		
        _4g_info.sta_ipv4[0] = esp_ip4_addr1(&event->ip_info.ip);
        _4g_info.sta_ipv4[1] = esp_ip4_addr2(&event->ip_info.ip);
        _4g_info.sta_ipv4[2] = esp_ip4_addr3(&event->ip_info.ip);
        _4g_info.sta_ipv4[3] = esp_ip4_addr4(&event->ip_info.ip);
		
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
//        xEventGroupSetBits(event_group, CONNECT_BIT);
		iot_mqtt_start(NETIF_TYPE_PPP);
		iot_mqtt2_start(NETIF_TYPE_PPP);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } 
	else if (event_id == IP_EVENT_PPP_LOST_IP) 
	{
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
		reals.Cellular4G_step=LOST_IP_4G_READY;
		iot_mqtt_delete(NETIF_TYPE_PPP);
		iot_mqtt2_delete(NETIF_TYPE_PPP);
		
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
		iot_mqtt_start(NETIF_TYPE_PPP);
		iot_mqtt2_start(NETIF_TYPE_PPP);
    }
}


int event_init_PPP(void)
{
	if(0 == reals.event_ppp_register_flag)
	{
		reals.event_ppp_register_flag =1;
	    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
	    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
		
	}
	return ESP_OK;
}

int event_deinit_PPP(void)
{
	if(1 == reals.event_ppp_register_flag)
	{
		reals.event_ppp_register_flag =0;
		ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event));
		ESP_ERROR_CHECK(esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed));
		
	}

	
	return ESP_OK;
}




void Net_Exchange_Cellular4G(void ) 
{
	static uint8_t sCellular4G_Enable_old=0;
	static uint8_t sLostCnt=0;//4G模块网络异常超时计数器
	
	static esp_modem_dce_t *dce = NULL;
	static esp_netif_t *esp_netif = NULL;
	uint8_t tempdata[30];
	
	int rssi, ber;
	esp_err_t err;

	memset(tempdata, 0, sizeof(tempdata));//clear

	/* Configure the DTE */
	esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
	/* setup UART specific configuration based on kconfig options */
	dte_config.uart_config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
	dte_config.uart_config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
//	  dte_config.uart_config.rts_io_num = CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
//	  dte_config.uart_config.cts_io_num = CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;
	dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;//EXAMPLE_FLOW_CONTROL;
	dte_config.uart_config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
	dte_config.uart_config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
	dte_config.uart_config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;

	dte_config.uart_config.baud_rate = UART2_BAUD,			 /* 波特率 19200 */
	dte_config.uart_config.data_bits = UART_DATA_8_BITS;	/*8个数据位*/
	dte_config.uart_config.parity = UART_PARITY_DISABLE;	/*没有奇偶校验*/
	dte_config.uart_config.stop_bits = UART_STOP_BITS_1;	/*1个停止位*/
	dte_config.uart_config.source_clk = UART_SCLK_APB;	/*串口时钟源*/ 
	dte_config.uart_config.port_num = UART2;
	dte_config.task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
	
	dte_config.task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
	dte_config.dte_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE / 2;

	/* Configure the PPP netif */
	esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
//			memcpy(dce_config.apn, CONFIG_EXAMPLE_MODEM_PPP_APN, sizeof(CONFIG_EXAMPLE_MODEM_PPP_APN));
	esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

	/* Init and register system/core components */
//	ESP_ERROR_CHECK(esp_netif_init());
//	ESP_ERROR_CHECK(esp_event_loop_create_default());

	if((reals.MCUPoweronCnt >= 100)
		&&(NULL != dce)//已初始化（基于使能条件），才判断
		&&(0 == g_self_data.mod_reg11000_IOT_info.link.bit.mqtt)//bluetti服务器连接失败
		&&(1 == SetData.dev_info_t.on_off.bit._4g_enable)		
	)//
	{
		if(++sLostCnt >= 60)
		{
			sLostCnt=0;
			reals.MCUPoweronCnt =100;
			g_self_data.mod_reg11000_IOT_info.link.bit._4g = 0;
			err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);//退出 PPP，进入AT 模式，rx收到“NO CARRIER”
			if (err != ESP_OK) 
			{
			
				ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
				return;
			}
			else//ok
			{
				reals.MCUPoweronCnt =0;
				sCellular4G_Enable_old =0;//重新开始
				ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) OK");
			}
		}

	
	}


	if(sCellular4G_Enable_old != SetData.dev_info_t.on_off.bit._4g_enable)//模式变化时候 初始化或卸载驱动
	{
		if(1 == SetData.dev_info_t.on_off.bit._4g_enable)
		{
//			event_init_PPP();
			if(0 == reals.event_ppp_register_flag)
			{
				reals.event_ppp_register_flag =1;
				ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
				ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
				esp_netif = esp_netif_new(&netif_ppp_config);
//				if(NULL == esp_netif)
//				{
//					ESP_LOGE(TAG, "esp_netif_new  fail");
//					return;
//				}
				assert(esp_netif);
			
			//	  event_group = xEventGroupCreate();
			
	
	
				ESP_LOGI(TAG, "Initializing esp_modem for a generic module...");
				dce = esp_modem_new(&dte_config, &dce_config, esp_netif);
//				if(NULL == dce)
//				{
//					ESP_LOGE(TAG, "esp_modem_new  fail");
//					return;
//				}
				assert(dce);
			}

		//待测试 ESP 重启，但是 4G模块 已进入PPP模式，但没退出的情况
			if(reals.MCUPoweronCnt <= 20)//上电前20s不操作
			{
				return;
			}
		
		//	  xEventGroupClearBits(event_group, CONNECT_BIT | GOT_DATA_BIT | USB_DISCONNECTED_BIT);
		
			/* Run the modem demo app */
		//#if CONFIG_EXAMPLE_NEED_SIM_PIN == 1
		//	  // check if PIN needed
		//	  bool pin_ok = false;
		//	  if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok == false) {
		//		  if (esp_modem_set_pin(dce, CONFIG_EXAMPLE_SIM_PIN) == ESP_OK) {
		//			  vTaskDelay(pdMS_TO_TICKS(1000));
		//		  } else {
		//			  abort();
		//		  }
		//	  }
		//#endif
		
		//	   err = esp_modem_get_signal_quality(dce, &rssi, &ber);
		//	  if (err != ESP_OK) {
		//		  ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
		//		  return;
		//	  }
		//	  ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
		
			 err = esp_modem_read_pin(dce,(bool *)tempdata);
			if (err != ESP_OK) 
			{
				ESP_LOGE(TAG, "esp_modem_read_pin:failed with %d %s", err, esp_err_to_name(err));
				g_self_data.mod_reg11000_IOT_info.SIM_ready =0;
				
				reals.Cellular4G_AT_fail_cnt++;
				return;
			}
			else//ok
			{
				if(0 == tempdata[0])//false
				{
					g_self_data.mod_reg11000_IOT_info.SIM_ready =0;
				}
				else//true
				{
					g_self_data.mod_reg11000_IOT_info.SIM_ready =1;
				}
				
				reals.Cellular4G_AT_fail_cnt=0;
			}

//            if (reals.Cellular4G_AT_fail_cnt > 5)//超过5次无回复
//            {
//                g_self_data.mod_reg11000_IOT_info.iot_fault.bit._4g_fault = 1;
//            }
//            else
//            {
//                g_self_data.mod_reg11000_IOT_info.iot_fault.bit._4g_fault = 0;
//            }

			
			 err = esp_modem_schedule_before(dce);
			if (err != ESP_OK) 
			{
				ESP_LOGE(TAG, "esp_modem_schedule_before:failed with %d %s", err, esp_err_to_name(err));
				reals.Cellular4G_AT_fail_cnt++;
				
				return;
			}
//20240530
			 err = esp_modem_get_imei(dce,(char *)tempdata);
			if (err != ESP_OK) 
			{
				ESP_LOGE(TAG, "esp_modem_get_imei:failed with %d %s", err, esp_err_to_name(err));
				reals.Cellular4G_AT_fail_cnt++;
				return;
			}
			else
			{
				memcpy(g_self_data.mod_reg11000_IOT_info.IMEI, tempdata, sizeof(tempdata));
				ESP_LOGI(TAG, "IMEI =%s", g_self_data.mod_reg11000_IOT_info.IMEI);
				
			}
			
			 err = esp_modem_get_imsi(dce,(char *)tempdata);
			if (err != ESP_OK) 
			{
				ESP_LOGE(TAG, "esp_modem_get_imsi:failed with %d %s", err, esp_err_to_name(err));
				reals.Cellular4G_AT_fail_cnt++;
				return;
			}
			else
			{
				memcpy(g_self_data.mod_reg11000_IOT_info.IMSI, tempdata, sizeof(tempdata));
				ESP_LOGI(TAG, "IMSI =%s", g_self_data.mod_reg11000_IOT_info.IMSI);
				
			}			

#define	ICCID_NL668_AT	"+CCID: "//
#define	ICCID_EC25_AT	"+QCCID: "//

			 err = esp_modem_get_iccid(dce,(char *)tempdata);
			if (err != ESP_OK) 
			{
				ESP_LOGE(TAG, "esp_modem_get_iccid:failed with %d %s", err, esp_err_to_name(err));
				reals.Cellular4G_AT_fail_cnt++;
				return;
			}
			else
			{
				
//				char *p;	
//				char *token = strtok_r((char *)tempdata, " ", &p);		
//				strncpy((char *)g_self_data.mod_reg11000_IOT_info.ICCID, token,20);

				char* target_pos = strstr((char *)tempdata, ICCID_NL668_AT);//strchr "+CCID: "
				uint8_t target_len = strlen(ICCID_NL668_AT);//"+CCID: "
				uint8_t remaining_len = strlen(target_pos + target_len); // 计算剩余长度
				if(remaining_len >= 20)
				{
					remaining_len =20;

				}
				strncpy((char *)g_self_data.mod_reg11000_IOT_info.ICCID, (char *)(target_pos + target_len),remaining_len);

				ESP_LOGI(TAG, "ICCID =%s", g_self_data.mod_reg11000_IOT_info.ICCID);
				
			}

			
			ESP_LOGI(TAG, "after :esp_modem_schedule_before " );
	
		
//#ifdef CONFIG_EXAMPLE_MODEM_DEVICE_CUSTOM
//			{
//				char time[64];
//				err = esp_modem_get_time(dce, time);
//				if (err != ESP_OK) {
//					ESP_LOGE(TAG, "esp_modem_get_time failed with %d %s", err, esp_err_to_name(err));
//					return;
//				}
//				ESP_LOGI(TAG, "esp_modem_get_time: %s", time);
//			}
//#endif
//		
//#if CONFIG_EXAMPLE_SEND_MSG//发送短信
//			if (esp_modem_sms_txt_mode(dce, true) != ESP_OK || esp_modem_sms_character_set(dce) != ESP_OK) {
//				ESP_LOGE(TAG, "Setting text mode or GSM character set failed");
//				return;
//			}
//		
//			err = esp_modem_send_sms(dce, CONFIG_EXAMPLE_SEND_MSG_PEER_PHONE_NUMBER, "Text message from esp-modem");
//			if (err != ESP_OK) {
//				ESP_LOGE(TAG, "esp_modem_send_sms() failed with %d", err);
//				return;
//			}
//#endif






			err = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);//进入 PPP的data 模式
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", err);
				return;
			}
			reals.Cellular4G_step = G4_MODULE_GET_IN_DTATA_MODE;
			/* Wait for IP address */
			ESP_LOGI(TAG, "Waiting for IP address");
//			xEventGroupWaitBits(event_group, CONNECT_BIT | USB_DISCONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);



		sCellular4G_Enable_old = SetData.dev_info_t.on_off.bit._4g_enable;//在正确执行后再赋值

		g_self_data.mod_reg11000_IOT_info.link.bit._4g = 1;

		}
		else if(2 == SetData.dev_info_t.on_off.bit._4g_enable)
		{
//			event_deinit_PPP();
			if(1 == reals.event_ppp_register_flag)
			{
				reals.event_ppp_register_flag =0;
				ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event));
				ESP_ERROR_CHECK(esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed));

				// UART DTE clean-up
				esp_modem_destroy(dce);
				esp_netif_destroy(esp_netif);
				
			}	
			if((G4_MODULE_GET_IN_CMD_MODE == reals.Cellular4G_step)
				&&(NULL !=dce)
				&&(NULL !=esp_netif))
			{
				err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
				if (err != ESP_OK) 
				{
					ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
					return;
				}
				else
				{
				}
			
			}
				sCellular4G_Enable_old = SetData.dev_info_t.on_off.bit._4g_enable;//在正确执行后再赋值
				
				g_self_data.mod_reg11000_IOT_info.link.bit._4g = 0;
		}
	}

}
