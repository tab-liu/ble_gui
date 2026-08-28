/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "sdkconfig.h"
#include "iot_period_task.h"
#include "iot_mqtt.h"
#include "iot_wifi_init.h"
#include "uart_device_process.h"
#include "server2internet.h"



static const char *TAG = "[ETH_APP]";
//
///** Event handler for Ethernet events */
//static void eth_event_handler(void *arg, esp_event_base_t event_base,
//                              int32_t event_id, void *event_data)
//{
//    uint8_t mac_addr[6] = {0};
//    /* we can get the ethernet driver handle from event data */
//    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
//
//    switch (event_id) {
//    case ETHERNET_EVENT_CONNECTED://网线接入，PHY LED blink
//        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
//        ESP_LOGI(TAG, "Ethernet Link Up");
//        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
//                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
////		iot_wifi_eth_4g_switch_user(1);
//		reals.Ethernet_SPI_step = ETH_MAC_LINK_UP;
//		
//		g_self_data.mod_reg11000_IOT_info.link.bit.Eth_Stat = 1;
//        break;
//    case ETHERNET_EVENT_DISCONNECTED://网线断开
//        ESP_LOGI(TAG, "Ethernet Link Down");
//        
//        iot_wifi_eth_4g_switch_user(0);
//
//		reals.Ethernet_SPI_step = ETH_INIT;
//		g_self_data.mod_reg11000_IOT_info.link.bit.Eth_Stat = 0;
//		
//        break;
//    case ETHERNET_EVENT_START:
//        ESP_LOGI(TAG, "Ethernet Started");
//        break;
//    case ETHERNET_EVENT_STOP:
//        ESP_LOGI(TAG, "Ethernet Stopped");
//        break;
//    default:
//        break;
//    }
//}
//
///** Event handler for IP_EVENT_ETH_GOT_IP */
//static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
//                                 int32_t event_id, void *event_data)
//{
//    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
//    const esp_netif_ip_info_t *ip_info = &event->ip_info;
//
//
//
//	switch (event_id) 
//	{
//
//		case IP_EVENT_ETH_GOT_IP:
//	    ESP_LOGI(TAG, "Ethernet Got IP Address");
//	    ESP_LOGI(TAG, "~~~~~~~~~~~");
//	    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
//	    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
//	    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
//	    ESP_LOGI(TAG, "~~~~~~~~~~~");
//
//
//        eth_info.sta_ipv4[0] = esp_ip4_addr1(&ip_info->ip);
//        eth_info.sta_ipv4[1] = esp_ip4_addr2(&ip_info->ip);
//        eth_info.sta_ipv4[2] = esp_ip4_addr3(&ip_info->ip);
//        eth_info.sta_ipv4[3] = esp_ip4_addr4(&ip_info->ip);
//
//        eth_info.sta_gw[0] = esp_ip4_addr1(&ip_info->gw);
//        eth_info.sta_gw[1] = esp_ip4_addr2(&ip_info->gw);
//        eth_info.sta_gw[2] = esp_ip4_addr3(&ip_info->gw);
//        eth_info.sta_gw[3] = esp_ip4_addr4(&ip_info->gw);
//
//        eth_info.sta_mask[0] = esp_ip4_addr1(&ip_info->netmask);
//        eth_info.sta_mask[1] = esp_ip4_addr2(&ip_info->netmask);
//        eth_info.sta_mask[2] = esp_ip4_addr3(&ip_info->netmask);
//        eth_info.sta_mask[3] = esp_ip4_addr4(&ip_info->netmask);
//
//		
//		iot_mqtt_start(NETIF_TYPE_ETH);		
//		iot_mqtt2_start(NETIF_TYPE_ETH);
//		break;
//
//		case IP_EVENT_ETH_LOST_IP:
//		ESP_LOGI(TAG, "Ethernet Lost IP Address");
//		reals.Ethernet_SPI_step = ETH_LOST_IP;
//		iot_mqtt_delete(NETIF_TYPE_ETH);
//		iot_mqtt2_delete(NETIF_TYPE_ETH);
//		default:
//		break;
//	}
//
//
//
//	
//}
//								 
//
//void SPI_Ethernet_init(void)
//{
//    // Initialize Ethernet driver
//    uint8_t eth_port_cnt = 0;
//    esp_eth_handle_t *eth_handles;
//	ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));
//
////    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
////    ESP_ERROR_CHECK(esp_netif_init());
////    // Create default event loop that running in background
////    ESP_ERROR_CHECK(esp_event_loop_create_default());
//	if(0 == reals.event_eth_register_flag)
//	{
//		reals.event_eth_register_flag =1;
//        esp_eth_handle_t *eth_handles;
//	    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));
//    // Create instance(s) of esp-netif for Ethernet(s)
//	    if (eth_port_cnt == 1) 
//		{
//	        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
//	        // default esp-netif configuration parameters.
//	        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
//	        esp_netif_t *eth_netif = esp_netif_new(&cfg);
//	        // Attach Ethernet driver to TCP/IP stack
//	        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
//	    } 
//		else 
//		{
//	        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
//	        // esp-netif configuration parameters for each interface (name, priority, etc.).
//	        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
//	        esp_netif_config_t cfg_spi = {
//	            .base = &esp_netif_config,
//	            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
//	        };
//	        char if_key_str[10];
//	        char if_desc_str[10];
//	        char num_str[3];
//	        for (int i = 0; i < eth_port_cnt; i++) 
//			{
//	            itoa(i, num_str, 10);
//	            strcat(strcpy(if_key_str, "ETH_"), num_str);
//	            strcat(strcpy(if_desc_str, "eth"), num_str);
//	            esp_netif_config.if_key = if_key_str;
//	            esp_netif_config.if_desc = if_desc_str;
//	            esp_netif_config.route_prio -= i*5;
//	            esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);
//
//	            // Attach Ethernet driver to TCP/IP stack
//	            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
//	        }
//	    }
//
//		    // Register user defined event handers
//		    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
//		    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &got_ip_event_handler, NULL));
//
//		    // Start Ethernet driver state machine
//		    for (int i = 0; i < eth_port_cnt; i++) 
//			{
//		        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
//		    }		
//		}
//
//        // // Register user defined event handers
//        // ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
//        // ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &got_ip_event_handler, NULL));
//}
//
//int event_SPI_Ethernet_Deinit(void)
//
//{
//	if(1 == reals.event_eth_register_flag)
//	{
//		// reals.event_eth_register_flag =0;
//		// ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler));
//		// ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &got_ip_event_handler));
//		
//	}
//
//	
//	return ESP_OK;
//}
//
//
//void Net_Exchange_SPI_ETH(void ) 
//{
//	static uint8_t sEthernet_Enable_old=0;
//	
//
//	if(sEthernet_Enable_old != SetData.dev_info_t.on_off.bit.Eth_enable)//模式变化时候 初始化或卸载驱动
//	{
//		sEthernet_Enable_old =SetData.dev_info_t.on_off.bit.Eth_enable;
//		if(1 == SetData.dev_info_t.on_off.bit.Eth_enable)
//		{
//			SPI_Ethernet_init();
//            // 以太网禁用时实际未down，需要重新置位
//            if(ETH_GET_IP_READY == reals.Ethernet_SPI_step)
//            {
//                g_self_data.mod_reg11000_IOT_info.link.bit.Eth_Stat = 1;
//            }
//		}
//		else if(2 == SetData.dev_info_t.on_off.bit.Eth_enable)
//		{
//			event_SPI_Ethernet_Deinit();
//			g_self_data.mod_reg11000_IOT_info.link.bit.Eth_Stat = 0;
//		}		
//	}
//}			
