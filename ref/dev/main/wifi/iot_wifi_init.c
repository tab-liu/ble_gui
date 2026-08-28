#include "iot_wifi_init.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_smartconfig.h"
#include "freertos/event_groups.h"
#include "iot_mqtt.h"
#include "iot_period_task.h"
#include "pppos_client_main.h"
#include "ethernet_init.h"

#include "server2internet.h"
#include "mesh_api.h"
#include "udp_multicast.h"
#include "filesystem.h"
#include "utils.h"
#include "cloud_diag.h"

#define WIFI_STA_CONNECTED_AP      BIT0
#define WIFI_STA_DISCONNECT_AP     BIT1
#define WIFI_STA_CONNECT_FAILED    BIT2
#define WIFI_STA_SMARTCFG_DOING    BIT3
#define WIFI_STA_SMARTCFG_DONE     BIT4

#define WIFI_STA_SCAN_DONE         BIT5

#define WIFI_OPEN_AP               BIT6
#define WIFI_OPEN_STA              BIT7

#define WIFI_CLOSE_AP              BIT8
#define WIFI_CLOSE_STA             BIT9

#define WIFI_STA_NEW_AP            BIT10

#define WIFI_AP_NEW_AP             BIT11

#define WIFI_STA_MULT_ALL_FAILED   BIT12

#define EXAMPLE_ESP_MAXIMUM_RETRY   10

#define NATIVE_WIFI_LED				LED_WIFI

#define MAX_PASSWORD_ERR_CNT        3

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#define TAG "[WIFI]"

static int password_err_cnt;
static uint8_t s_retry_num = 0;
// • ESP_OK: succeed
// • ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
// • ESP_ERR_WIFI_NOT_STARTED: WiFi is not started by esp_wifi_start
// • ESP_ERR_WIFI_CONN: WiFi internal error, station or soft-AP control block wrong
// • ESP_ERR_WIFI_SSID: SSID of AP which station connects is invalid

//static EventGroupHandle_t  wifi_eventgroup = NULL;
EXT_RAM_BSS_ATTR  wifi_info_t wifi_info;//RAM中间变量集合static ,自己作为WIFI STA的参数,仅用于展示，不是最终的底层配置
EXT_RAM_BSS_ATTR  wifi_info_t _4g_info;//RAM中间变量集合static 
EXT_RAM_BSS_ATTR  wifi_info_t eth_info;//RAM中间变量集合static 

/*
listen_interval 推荐设置范围一般为 3~10。
(在 WIFI_PS_MIN_MODEM 或非省电模式下，listen_interval 设置不会影响唤醒频率，设备会按照默认频率（通常每个 Beacon 间隔）唤醒)
3~5：兼顾低功耗和网络实时性，适合大多数 IoT 场景。
6~10：更省电，但可能会延迟接收数据，适合对实时性要求不高的设备（如传感器、周期性上报设备）。
*/
static wifi_config_t wifi_config = {
    .sta = {
        .listen_interval = 3, // 只在 WIFI_PS_MAX_MODEM 下有效
    },
};//硬件底层配置直接结构体

EXT_RAM_BSS_ATTR static wifi_config_t wifi_config_ap;//硬件底层配置直接结构体

static SemaphoreHandle_t wifi_semaphore = NULL;

static const wifi_mmult_sta_base_t wifi_mmult_sta_addr[WIFI_STA_INDEX_MAX] = {
    {WIFI_STA_INDEX1,   &SetData.dev_info_t.wifi_sta_auth,   SetData.dev_info_t.wifi_sta_ssid,    SetData.dev_info_t.wifi_sta_password},
    {WIFI_STA_INDEX2,   &SetData.dev_info_t.wifi_sta2_auth,  SetData.dev_info_t.wifi_sta2_ssid,   SetData.dev_info_t.wifi_sta2_password},
    {WIFI_STA_INDEX3,   &SetData.dev_info_t.wifi_sta3_auth,  SetData.dev_info_t.wifi_sta3_ssid,   SetData.dev_info_t.wifi_sta3_password}
};

static uint8_t wifi_sta_num = 0;
static bool wifi_sta_stop_flag[WIFI_STA_INDEX_MAX] = {false};
static uint8_t wifi_mul_sta_en[WIFI_STA_INDEX_MAX] = {0};
static uint8_t wifi_mul_sta_static_ip_en[WIFI_STA_INDEX_MAX] = {0};
static bool wifi_sta_update_flag = false;

static uint16_t curr_ap_list_num = 0;
static wifi_ap_record_t *curr_ap_list = NULL;

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
static char g_dev_type[64] = {0};
static uint64_t g_dev_sn = 0;
static int g_wifi_connect_error_cnt = 0;    //wifi连接失败次数

// static void dump_buf(char *info, uint8_t *buf, uint32_t len);
#endif
static void iot_wifi_get_cfg_ap(uint8_t *auth, char *ssid, char *pwd);
int wifi_ap_netif_config(void);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    ESP_LOGI(TAG,"wifi_info.flag.bit.sta_enable:%d event_base:%s",wifi_info.flag.bit.sta_enable,event_base);
	/* 如果wifi没有使能则直接退出 */
	if (!wifi_info.flag.bit.sta_enable && !wifi_info.flag.bit.ap_enable)
	{
		ESP_LOGW(TAG, "wifi event, wifi is closed and exit");
		return;
	}

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
	{
        // uint8_t vnd_ie[] = {0x01, 0x00, 0x00, 0xFF, 0x7F, 0xDD, 0x1E, 0x00, 0x1D};
        // esp_wifi_set_vendor_ie(true, WIFI_VND_IE_TYPE_BEACON,
        //             WIFI_VND_IE_ID_0, vnd_ie);
        // esp_wifi_set_vendor_ie_cb(wifi_vendor_process, "bluetti ctx");

        s_retry_num = 0;
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        /** AP网卡dhcps给子设备sta分配IP时, 会触发一次 WIFI_EVENT_STA_START 事件 */
        wifi_ap_record_t ap_info;
        if (ESP_OK == esp_wifi_sta_get_ap_info(&ap_info))
        {
            ESP_LOGW(TAG, "wifi station already connected");
            return;
        }

        if(true == is_root_node())
        {
            esp_wifi_connect();
        }
        else
        {
            // 必须要在wifi start和connect之间扫描wifi
            esp_wifi_scan_stop();
            esp_wifi_disconnect();
            // vTaskDelay(pdMS_TO_TICKS(3000));
            esp_wifi_scan_start(get_wifi_scan_config(), false);
        }
#else
        esp_wifi_connect();
#endif
        ESP_LOGI(TAG, "Start WiFi Station connect ssid: %s, password: %s", wifi_config.sta.ssid, wifi_config.sta.password);
    }
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) //自己作为STA的 断开连接
	{
        /* 仅多路STA模式启用时，才按 slot 使能判断；单STA时 sta1_en=0 表示用默认主配置，不能视为关闭 */
        if (wifi_sta_num > 0) {
            for ( uint8_t i = 0 ; i < WIFI_STA_INDEX_MAX ; i++ )
            {
                if (( wifi_info.sta_index == wifi_mmult_sta_addr[i].sta_index )
                    && (wifi_mul_sta_en[i] != 1)) {
                    reals.wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
                    password_err_cnt = 0;
                    s_retry_num = 0;
                    return;
                }
            }
        }

        if (s_retry_num < 10) 
		{
            ESP_LOGW(TAG, "retry %d to connect to the ap", s_retry_num);
            reals.wifi_eventgroup |= WIFI_STA_DISCONNECT_AP;
			s_retry_num++;

        } 
		else 
		{

            ESP_LOGW(TAG,"retry %d count and connect ap name: %s, passward: %s fail" , s_retry_num,
                            wifi_config.sta.ssid, wifi_config.sta.password);
            reals.wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
            
            s_retry_num = 0;
        }

		/* 统计密码错误次数 */
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        cloud_diag_wifi_sta_disconnected(disconnected->reason, disconnected->rssi, s_retry_num);
		if (disconnected->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||	// 路由器告知密码错误
			disconnected->reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {		// ESP32本身超时机制产生的错误
			password_err_cnt++;
		}
        reals.last_disconnect_router_time = reals.now;
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        g_wifi_connect_error_cnt++;
        if (disconnected->reason == WIFI_REASON_ASSOC_TOOMANY
         || disconnected->reason == WIFI_REASON_ASSOC_LEAVE)
        {
            g_wifi_connect_error_cnt = MAX_OVER_LIMIT_ERR_CNT;
            ESP_LOGI(TAG, "AP reach max error count");
        }
        ESP_LOGW(TAG, "sta disconnected, reason:%d", disconnected->reason);
        mesh_event_sta_disconnected(g_wifi_connect_error_cnt);
        if (false == is_root_node() && MAX_OVER_LIMIT_ERR_CNT <= g_wifi_connect_error_cnt)
        {
            return;
        }
#endif
	#if 0
        ESP_LOGE(TAG, "Disconnect reason : %d", disconnected->reason);

		/* Set code corresponding to the reason for disconnection */
        switch (disconnected->reason) {
        case WIFI_REASON_AUTH_EXPIRE: // 认证失效
			ESP_LOGE(TAG, "reason auth expire");
			break;
		case WIFI_REASON_ASSOC_LEAVE:
		    ESP_LOGE(TAG, "reason assoc leave");
			break;
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
			ESP_LOGE(TAG, "reason 4way handshake timeout");
			break;
        case WIFI_REASON_AUTH_FAIL:
			ESP_LOGE(TAG, "reason auth fail");
			break;
        case WIFI_REASON_ASSOC_EXPIRE:
			ESP_LOGE(TAG, "reason assoc expire");
			break;
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
			ESP_LOGE(TAG, "reason handshake timeout");
			break;
        case WIFI_REASON_MIC_FAILURE:
            ESP_LOGE(TAG, "reason STA Auth Error");
            break;
		case WIFI_REASON_BEACON_TIMEOUT:
			ESP_LOGE(TAG, "reason beacon timeout");
			break;
        case WIFI_REASON_NO_AP_FOUND:
            ESP_LOGE(TAG, "reason STA AP Not found");
            break;
		case WIFI_REASON_CONNECTION_FAIL:
			ESP_LOGE(TAG, "reason connection fail");
            break;
        default:
			ESP_LOGE(TAG, "reson else");
            /* If none of the expected reasons,
             * retry connecting to host SSID */
            //esp_wifi_connect();
			break;
        }
	#endif
    } 
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) //自己作为STA
	{
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_dns_info_t dns1_info;
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns1_info);
        esp_netif_dns_info_t dns2_info;
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns2_info);
        
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_info.flag.bit.sta_link_ap = 1;
        reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);
        reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_DISCONNECT_AP);
        ESP_LOGI(TAG, "gateway ip:" IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "netmask:" IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "DNS1:" IPSTR, IP2STR(&dns1_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "DNS2:" IPSTR, IP2STR(&dns2_info.ip.u_addr.ip4));

		password_err_cnt = 0;
        wifi_info.sta_ipv4[0] = esp_ip4_addr1(&event->ip_info.ip);//自己的 STA IP
        wifi_info.sta_ipv4[1] = esp_ip4_addr2(&event->ip_info.ip);
        wifi_info.sta_ipv4[2] = esp_ip4_addr3(&event->ip_info.ip);
        wifi_info.sta_ipv4[3] = esp_ip4_addr4(&event->ip_info.ip);

        wifi_info.sta_gw[0] = esp_ip4_addr1(&event->ip_info.gw);
        wifi_info.sta_gw[1] = esp_ip4_addr2(&event->ip_info.gw);
        wifi_info.sta_gw[2] = esp_ip4_addr3(&event->ip_info.gw);
        wifi_info.sta_gw[3] = esp_ip4_addr4(&event->ip_info.gw);

        wifi_info.sta_mask[0] = esp_ip4_addr1(&event->ip_info.netmask);
        wifi_info.sta_mask[1] = esp_ip4_addr2(&event->ip_info.netmask);
        wifi_info.sta_mask[2] = esp_ip4_addr3(&event->ip_info.netmask);
        wifi_info.sta_mask[3] = esp_ip4_addr4(&event->ip_info.netmask);

        wifi_info.sta_dns1[0] = esp_ip4_addr1(&dns1_info.ip.u_addr.ip4);
        wifi_info.sta_dns1[1] = esp_ip4_addr2(&dns1_info.ip.u_addr.ip4);
        wifi_info.sta_dns1[2] = esp_ip4_addr3(&dns1_info.ip.u_addr.ip4);
        wifi_info.sta_dns1[3] = esp_ip4_addr4(&dns1_info.ip.u_addr.ip4);

        wifi_info.sta_dns2[0] = esp_ip4_addr1(&dns2_info.ip.u_addr.ip4);
        wifi_info.sta_dns2[1] = esp_ip4_addr2(&dns2_info.ip.u_addr.ip4);
        wifi_info.sta_dns2[2] = esp_ip4_addr3(&dns2_info.ip.u_addr.ip4);
        wifi_info.sta_dns2[3] = esp_ip4_addr4(&dns2_info.ip.u_addr.ip4);

        s_retry_num = 0;
		
		if(1 == wifi_info.flag.bit.sta_enable)//windy change:STA模式使能情况下，才尝试连接AP
		{
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
            cloud_diag_wifi_got_ip(ip_str, (uint8_t)reals.wifi_ap_mode,
                                   (uint8_t)SetData.dev_info_t.on_off.bit.wifi_ap_enable);
//			xEventGroupSetBits(wifi_eventgroup, WIFI_STA_CONNECTED_AP); /* STA_CONNECTED */
            reals.wifi_eventgroup |= WIFI_STA_CONNECTED_AP;
			
		}
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        esp_wifi_scan_stop();
        mesh_event_sta_connected(event->ip_info);
        g_wifi_connect_error_cnt = 0;
#endif
        reals.wifi_connect_timestemp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        reals.last_login_router_time = reals.now;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        ESP_LOGI(TAG, "WiFi AP start");
        if(WIFI_AP_MODE_NORMAL == reals.wifi_ap_mode)
        {
            wifi_ap_netif_config();
            // esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey(NETIF_KEY_WIFI_AP);
            // esp_netif_dhcp_status_t state;
            // ESP_ERROR_CHECK(esp_netif_dhcps_get_status(ap_netif, &state));
            // if (state != ESP_NETIF_DHCP_STOPPED)
            // {
            //     ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
            // }
            // ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
        }
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        else if (WIFI_AP_MODE_MESH == reals.wifi_ap_mode)
        {
            mesh_event_ap_start();
        }
#endif
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        if (WIFI_AP_MODE_MESH == reals.wifi_ap_mode)
        {
            wifi_event_sta_scan_done_t* event = (wifi_event_sta_scan_done_t*) event_data;
            uint8_t auth = 0;
            uint8_t ssid[32] = {0};
            uint8_t bssid[6] = {0};
            uint8_t pwd[64] = {0};
            if ( 1 == wifi_scan_handler(event, &auth, ssid, bssid, pwd))
            {
                iot_wifi_new_ap(auth, (const char *)ssid, (const char *)pwd, (const char *)bssid);
            }
        }
        else
#endif
        {
            // 多WiFi自动切换的扫描处理
            ESP_LOGI(TAG, "WiFi Scan done");

            uint16_t ap_num = 0;
            esp_wifi_scan_get_ap_num(&ap_num);

            // 判断是否为空，避免重复申请
            if ( curr_ap_list == NULL ) {
                curr_ap_list_num = ap_num;
                // 扫描数量有效时申请缓存
                if(ap_num > 0) {
                    curr_ap_list = iot_calloc(sizeof(wifi_ap_record_t) * curr_ap_list_num);
                    esp_wifi_scan_get_ap_records(&curr_ap_list_num, curr_ap_list);
                }
                reals.wifi_eventgroup |= WIFI_STA_SCAN_DONE;
            } else {
                // curr_ap_list不为空，存在正在处理的扫描队列，不执行操作
            }
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP)
    {
        ESP_LOGI(TAG, "WiFi AP stop");
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        if (WIFI_AP_MODE_MESH == reals.wifi_ap_mode)
        {
            mesh_event_ap_stop();
        }
#endif
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "A sta disconnected:" MACSTR, MAC2STR(event->mac));
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
    {
        ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
        ESP_LOGI(TAG, "A sta connected:" IPSTR " mac:" MACSTR,
                    IP2STR(&event->ip), MAC2STR(event->mac));
#ifdef  MESH_SELF_TEST
        mesh_self_test_add_child_node(event->ip.addr);
#endif
    }
}

#if 0
int event_init_WIFI(esp_netif_t *sta)
{
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	if(0 == reals.event_wifi_register_flag)
	{
		reals.event_wifi_register_flag =1;
        
        sta = esp_netif_create_default_wifi_sta();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
		esp_event_handler_instance_t instance_any_id; // 标识已注册事件处理程序实例的上下文
		esp_event_handler_instance_t instance_got_ip; // WiFi联网后的IP地址
		ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
															ESP_EVENT_ANY_ID,
															&wifi_event_handler,
															NULL,
															&instance_any_id));
		ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
															IP_EVENT_STA_GOT_IP,
															&wifi_event_handler,
															NULL,
															&instance_got_ip)); 
	}

	return ESP_OK;
}

int event_deinit_WIFI(esp_netif_t *sta)
{
	if(1 == reals.event_wifi_register_flag)
	{
		reals.event_wifi_register_flag =0;
		esp_event_handler_instance_t instance_any_id; // 标识已注册事件处理程序实例的上下文
		esp_event_handler_instance_t instance_got_ip; // WiFi联网后的IP地址
		esp_netif_destroy_default_wifi(sta);	
		ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,
															ESP_EVENT_ANY_ID,
															&instance_any_id));
		ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,
															IP_EVENT_STA_GOT_IP,
															&instance_got_ip)); 
	}

	return ESP_OK;
}
#endif

extern int event_SPI_Ethernet_Deinit(void);


/*------------------------------------------------------------------------------
 Function: wifi_scan_done_handler
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI SCAN解析函数
  * @param[in]  sta_scan_result_t *results[]  
  * @param[out] None
  * @return     static void
  */
static void wifi_scan_done_handler(sta_scan_result_t *results)
{
    // Initialize results with your SSID list
    for (int i = 0; i < WIFI_STA_INDEX_MAX; i++) {
        results[i].chl = i;
        results[i].rssi = -127; // Use minimum RSSI as default
        results[i].found = false;
    }

    // Match scanned APs with your table
    for (int i = 0; i < WIFI_STA_INDEX_MAX; i++) {
        for (int j = 0; j < curr_ap_list_num; j++) {
            if (strcmp(wifi_mmult_sta_addr[i].ssid, (const char *)curr_ap_list[j].ssid) == 0) {
                results[i].rssi = curr_ap_list[j].rssi;
                results[i].found = true;
                break;
            }
        }
    }

    // Sort: found first by RSSI descending, not found last
    for (int i = 0; i < WIFI_STA_INDEX_MAX - 1; i++) {
        for (int j = i + 1; j < WIFI_STA_INDEX_MAX; j++) {
            if ((!results[i].found && results[j].found) ||
                (results[i].found && results[j].found && results[i].rssi < results[j].rssi)) {
                sta_scan_result_t tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

#ifdef WIFI_CONNECT_LINK_INFO_DEBUG_LOG

    ESP_LOGI(TAG, "********************WIFI_AP_INFO*********************");
    // Print results
    for (int i = 0; i < WIFI_STA_INDEX_MAX; i++) {
        if (results[i].found) {
            ESP_LOGI(TAG, "SSID: %s, RSSI: %d, EN: %d, ERROR: %d", wifi_mmult_sta_addr[results[i].chl].ssid, results[i].rssi, 
                                                                wifi_mul_sta_en[results[i].chl], wifi_sta_stop_flag[results[i].chl]);
        } else {
            ESP_LOGW(TAG, "SSID: %s, Not found", wifi_mmult_sta_addr[results[i].chl].ssid);
        }
    }
    ESP_LOGI(TAG, "*****************************************************");

#endif    
}

/*------------------------------------------------------------------------------
| Function: wifi_scan_done_process
| -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI_SCAN处理函数
  * @param[in]  sta_scan_result_t *results[]  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t wifi_scan_done_process(sta_scan_result_t *results)
{
    uint8_t index = 0;
    for (index = 0; index < WIFI_STA_INDEX_MAX; index++)
    {
        if ((true == results[index].found)
            && (1 == wifi_mul_sta_en[results[index].chl])
            && (false == wifi_sta_stop_flag[results[index].chl]))
        {
            if (!wifi_info.flag.bit.sta_link_ap)
            {
                // 未连接时，优先连接能找到信号最好且密码未报错的WIFI
                ESP_LOGI(TAG, "Not connected. Connecting to STA index %d (RSSI: %d)",
                         wifi_mmult_sta_addr[results[index].chl].sta_index, results[index].rssi);
                iot_wifi_new_connect(wifi_mmult_sta_addr[results[index].chl].sta_index);
                return wifi_mmult_sta_addr[results[index].chl].sta_index;
            }
            else
            {
                // 已连接时，仅在成功扫描到信号强度高于当前设备至少10dbm且高于阈值的其他STA时更换连接
                if ((results[index].rssi > (iot_wifi_ap_rssi() + 10))
                    && (results[index].rssi > SetData.dev_info_t.wifi_sta_rssi_th)
                    && (wifi_info.sta_index != wifi_mmult_sta_addr[results[index].chl].sta_index))
                {
                    // 自动切换至信号更好的WIFI
                    ESP_LOGI(TAG, "Switching to stronger STA index %d (RSSI: %d), current RSSI: %d, threshold: %d",
                             wifi_mmult_sta_addr[results[index].chl].sta_index, results[index].rssi, iot_wifi_ap_rssi(), SetData.dev_info_t.wifi_sta_rssi_th);
                    iot_wifi_new_connect(wifi_mmult_sta_addr[results[index].chl].sta_index);
                    return wifi_mmult_sta_addr[results[index].chl].sta_index;
                }
            }
        }
    }

    return 0;
}

/*------------------------------------------------------------------------------
| Function: wifi_sta_dhcp_process
| -----------------------------------------------------------------------------*/
/**
  * @brief      STA静态IP相关逻辑
  * @param[in]  uint8_t sta_index  
  * @param[out] None
  * @return     static esp_err_t
  */
static esp_err_t wifi_sta_dhcp_process(uint8_t sta_index)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGE(TAG, "wifi_sta_dhcp_process : Failed to get netif handle for STA");
        return ESP_OK;
    }

    // 定义静态IP配置数组（需要根据实际项目的数据结构来设置）
    static const wifi_mmult_sta_static_ip_t wifi_mmult_sta_unDHCP[WIFI_STA_INDEX_MAX] = {
        {WIFI_STA_INDEX1, &SetData.dev_info_t.wifi_sta1_ip, &SetData.dev_info_t.wifi_sta1_mask, &SetData.dev_info_t.wifi_sta1_gw, &SetData.dev_info_t.wifi_sta1_dns1, &SetData.dev_info_t.wifi_sta1_dns2},
        {WIFI_STA_INDEX2, &SetData.dev_info_t.wifi_sta2_ip, &SetData.dev_info_t.wifi_sta2_mask, &SetData.dev_info_t.wifi_sta2_gw, &SetData.dev_info_t.wifi_sta2_dns1, &SetData.dev_info_t.wifi_sta2_dns2},
        {WIFI_STA_INDEX3, &SetData.dev_info_t.wifi_sta3_ip, &SetData.dev_info_t.wifi_sta3_mask, &SetData.dev_info_t.wifi_sta3_gw, &SetData.dev_info_t.wifi_sta3_dns1, &SetData.dev_info_t.wifi_sta3_dns2}
    };

    for ( uint8_t i = 0 ; i < WIFI_STA_INDEX_MAX ; i++ )
    {
        // 找到对应的存储STA
        if ( sta_index == wifi_mmult_sta_unDHCP[i].sta_index ) {
            // 1. Always stop DHCP before configuring IP
            esp_netif_dhcpc_stop(netif);
            
            // 2. Decide IP mode by config
            if (1 == wifi_mul_sta_static_ip_en[i]) {
                esp_netif_ip_info_t ip_info;
                ip_info.ip.addr      = *wifi_mmult_sta_unDHCP[i].sta_set_ip;
                ip_info.netmask.addr = *wifi_mmult_sta_unDHCP[i].sta_set_mask;
                ip_info.gw.addr      = *wifi_mmult_sta_unDHCP[i].sta_set_gw;

                ESP_LOGI(TAG, "Try set static IP: " IPSTR ", Mask: " IPSTR ", GW: " IPSTR,
                         IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask), IP2STR(&ip_info.gw));

                // 检查参数有效性
                if (ip_info.ip.addr == 0 || ip_info.netmask.addr == 0 || ip_info.gw.addr == 0) {
                    ESP_LOGE(TAG, "Invalid static IP config");
                    wifi_sta_stop_flag[i] = true;
                    return ESP_FAIL;
                }

                esp_err_t err = esp_netif_set_ip_info(netif, &ip_info);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_netif_set_ip_info failed: %d", err);
                    wifi_sta_stop_flag[i] = true;
                    return err;
                }
                
                // 设置DNS1
                esp_netif_dns_info_t dns_info;
                dns_info.ip.u_addr.ip4.addr = *wifi_mmult_sta_unDHCP[i].sta_set_dns1;
                dns_info.ip.type = ESP_IPADDR_TYPE_V4;
                err = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
                ESP_LOGI(TAG, "Local DNS1: " IPSTR ", set result: %d", IP2STR(&dns_info.ip.u_addr.ip4), err);

                // 设置DNS2
                dns_info.ip.u_addr.ip4.addr = *wifi_mmult_sta_unDHCP[i].sta_set_dns2;
                dns_info.ip.type = ESP_IPADDR_TYPE_V4;
                err = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
                ESP_LOGI(TAG, "Local DNS2: " IPSTR ", set result: %d", IP2STR(&dns_info.ip.u_addr.ip4), err);
            } else {
                // Use DHCP
                ESP_LOGI(TAG, "Enabling DHCP for STA index: %d", sta_index);
                esp_netif_dhcpc_start(netif);
            }

            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "wifi_sta_dhcp_process : STA index %d not found in configuration", sta_index);
    return ESP_FAIL;
}

/*
windy
因使能WIFI MESH 功能，要默认不关闭WIFI

*/
void Net_Exchange_WIFI(void ) 
{
	static uint8_t sWIFI_Enable_old=0;
    static esp_netif_t *sta = NULL;
	static uint8_t sonceflag=0;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	if(0 == sonceflag)
	{
		sonceflag = 1;
        //event_SPI_Ethernet_Deinit();//从ETH切换过来，需要先关闭ETH
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        if(WIFI_AP_MODE_MESH == reals.wifi_ap_mode)
        {
            if(false == is_root_node())
            {
                wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
                wifi_config.sta.channel = MESH_WIFI_STA_CHANNEL;
            }
            else
            {
                wifi_config.sta.scan_method = WIFI_FAST_SCAN;
            }
        }
#else
        wifi_config.sta.scan_method = WIFI_FAST_SCAN;
#endif
        wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL; // 通过信号强度对AP进行排序
        wifi_config.sta.threshold.rssi = -127; // AP信号强度小于-127dB不在连接

//			  event_deinit_WIFI(sta);
        
        if(0 == reals.event_wifi_register_flag)
        {
            reals.event_wifi_register_flag =1;
            
            sta = esp_netif_create_default_wifi_sta();
            if (WIFI_AP_MODE_CLOSE != reals.wifi_ap_mode)
            {
                esp_netif_create_default_wifi_ap();
            }
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            
            esp_event_handler_instance_t instance_any_id; // 标识已注册事件处理程序实例的上下文
            esp_event_handler_instance_t instance_got_ip; // WiFi联网后的IP地址
            esp_event_handler_instance_t instance_new_sta;
            ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                                ESP_EVENT_ANY_ID,
                                                                &wifi_event_handler,
                                                                NULL,
                                                                &instance_any_id));
            ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                                IP_EVENT_STA_GOT_IP,
                                                                &wifi_event_handler,
                                                                NULL,
                                                                &instance_got_ip));

            ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                                IP_EVENT_AP_STAIPASSIGNED,
                                                                &wifi_event_handler,
                                                                NULL,
                                                                &instance_new_sta));
        }
        
        if (WIFI_AP_MODE_CLOSE == reals.wifi_ap_mode)
        {
            ESP_LOGW(TAG, "set wifi mode sta");
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        }
        else
        {
            ESP_LOGW(TAG, "set wifi mode ap/sta");
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        }
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        if(WIFI_AP_MODE_MESH == reals.wifi_ap_mode)
        {
            init_mesh_ap_config(&wifi_config_ap.ap);
            // mesh模式下加密方式固定为WIFI_AUTH_WPA2_PSK
            // wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            /* 启动wifi SoftAp/station */
            set_ip_info_ap_netif();
            // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));

            init_mesh_scan_config();
            g_wifi_connect_error_cnt = 0;
        }
#endif
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM)); // Wifi节能模式
        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, wifi_info.sta_mac));
        ESP_LOGI(TAG, "wifi init station finished. and wifi no connect ap");
	}
//	if(sWIFI_Enable_old != SetData.dev_info_t.on_off.bit.wifi_enable)//模式变化时候 初始化或卸载驱动
//	{
//		sWIFI_Enable_old = SetData.dev_info_t.on_off.bit.wifi_enable;
//		
//		if(1 == SetData.dev_info_t.on_off.bit.wifi_enable)
//		
//		else if(2 == SetData.dev_info_t.on_off.bit.wifi_enable)
//		{
////			event_deinit_WIFI(sta);
//
//            if(1 == reals.event_wifi_register_flag)
//        	{
//        		reals.event_wifi_register_flag =0;
//        		esp_event_handler_instance_t instance_any_id; // 标识已注册事件处理程序实例的上下文
//        		esp_event_handler_instance_t instance_got_ip; // WiFi联网后的IP地址
//        		esp_wifi_stop();
//        		ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,
//        															ESP_EVENT_ANY_ID,
//        															&instance_any_id));
//        		ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,
//        															IP_EVENT_STA_GOT_IP,
//        															&instance_got_ip)); 
//                esp_netif_destroy_default_wifi(sta);	
//#ifdef BLUETTI_WLAN_MESH
//                esp_netif_destroy_default_wifi(g_mesh_net_info.ap_if);
//#endif
//                wifi_info.flag.bit.sta_link_ap = 0;
//        	}
//		}		
//	}


}

/*
1s cycle

*/
void Net_Exchange_configure(void ) 
{

	static uint8_t snet_ch_old = 0;	
	static uint16_t  Eth_link_Router_cnt=0;//通过有线以太网接入路由器连接成功计数器，用于超时自动切入WIFI的逻辑
	
	if((ETH_INIT == reals.Ethernet_SPI_step)
		&&(1 == SetData.dev_info_t.on_off.bit.Eth_enable))//从ETH自动切入WIFI的逻辑:ETH 上电10s后未插入网线，则自动切入WIFI；之后无法切回ETH
	{
		if(++Eth_link_Router_cnt >= 10)
		{
			Eth_link_Router_cnt = 0;
//            iot_wifi_eth_4g_switch_user(0); 
		}  
	} 
	else // 
	{
		Eth_link_Router_cnt =0;
	}
}

int wifi_ap_netif_config(void)
{
    // 通过Key获取AP网卡实例
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey(NETIF_KEY_WIFI_AP);
    if (NULL == ap_netif)
    {
        ESP_LOGE(TAG, "Get WiFi ap netif handle fail");
        return - 1;
    }

    esp_netif_ip_info_t info_t;
    memset(&info_t, 0, sizeof(esp_netif_ip_info_t));
    /* 设置IP信息前需关闭dhcps服务，否则会设置失败 */
    esp_netif_dhcp_status_t state;
    ESP_ERROR_CHECK(esp_netif_dhcps_get_status(ap_netif, &state));
    if (state != ESP_NETIF_DHCP_STOPPED)
    {
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    }

    //AP的IP等信息固定, 暂不支持修改
    info_t.ip.addr = esp_ip4addr_aton((const char *)"192.168.8.1");
    info_t.netmask.addr = esp_ip4addr_aton((const char *)"255.255.255.0");
    info_t.gw.addr = esp_ip4addr_aton((const char *)"192.168.8.1");
    esp_netif_set_ip_info(ap_netif, &info_t);

    // 设置DNS服务器为8.8.8.8
    esp_netif_dns_info_t dns_info;
    dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton((const char *)"8.8.8.8");
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info));

    uint8_t dhcps_dns_value = 0x02;   //dhcp server offer
    esp_err_t err = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                            &dhcps_dns_value, sizeof(dhcps_dns_value));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set DNS for DHCP server: %d", err);
        // return -1;
    }

    ESP_LOGI(TAG, "wifi ap set dns main to 8.8.8.8");

    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    return -2;
}

/*------------------------------------------------------------------------
*@Function： iot_wifi_task
WIFI 连接状态机
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void iot_wifi_task(void * pvParameters) 
{

    static uint8_t sRun1timeflag=0;
	if(0 == sRun1timeflag)
	{
		sRun1timeflag =1;
		
		/* Init and register system/core components */
		ESP_ERROR_CHECK(esp_netif_init());
		ESP_ERROR_CHECK(esp_event_loop_create_default());
	}
	vTaskDelay(pdMS_TO_TICKS(3000));

    static uint32_t pre_time = 0;
    static uint32_t last_link_time = 0;
    static uint16_t weak_signal_cnt = 0;

    while (1) 
	{

        vTaskDelay(pdMS_TO_TICKS(500));

        uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /*弱信号检测逻辑*/
        if ((now_time - pre_time) >= 500) {
            pre_time = now_time;
            
            if( !wifi_info.flag.bit.sta_link_ap ) {
                // 未连接，不检查
                last_link_time = now_time;
                weak_signal_cnt = 0;
            } else {
                if ((now_time - last_link_time) > 1000) {
                    last_link_time = now_time;
                    // STA数量大于1时才执行弱网检测逻辑
                    if ( wifi_sta_num > 1 ) {
                        // 检查信号
                        if ( iot_wifi_ap_rssi() < SetData.dev_info_t.wifi_sta_rssi_th ) {
                            // 低于阈值，计数
                            weak_signal_cnt++;
                            if(weak_signal_cnt > SetData.dev_info_t.wifi_sta_switch_int) {
                                // 超过间隔后重新扫描
                                ESP_LOGI(TAG, "Weak signal(< %d dbm) count exceeded interval(> %d s). Triggering auto connect.", 
                                            SetData.dev_info_t.wifi_sta_rssi_th, SetData.dev_info_t.wifi_sta_switch_int);
                                weak_signal_cnt = 0;
                                iot_wifi_auto_connect();
                            }
                        } else {
                            // 信号波动，清空计数，避免频繁重新扫描
                            weak_signal_cnt = 0;
                        }
                    } else {
                        // STA可用数量不允许网络切换
                        weak_signal_cnt = 0;
                    }
                }
            }
        }

        Net_Exchange_WIFI();


#ifdef FORCE_BLE_WIFI_ON_ENABLE
	
	SetData.dev_info_t.on_off.bit.wifi_enable=1;
#else
//	if(((1 != SetData.dev_info_t.on_off.bit.wifi_enable)
//		||(1 != reals.Addr_can_master))//非主设备部开BLE/WIFI
//	&&(reals.wifi_eventgroup & WIFI_OPEN_STA))
//	{
//		continue;
//	}
	
#endif
	
		if(reals.wifi_eventgroup)//WIFI连接逻辑
		{
			if (reals.wifi_eventgroup & WIFI_STA_SCAN_DONE)	/* wifi 扫描网络 */
			{
			    uint8_t update_connect = 0;
                sta_scan_result_t results[WIFI_STA_INDEX_MAX] = {0};
                
                // 检查扫描结果
                if (( curr_ap_list != NULL ) && ( curr_ap_list_num > 0)) 
                {
                    // 解析扫描结果
                    wifi_scan_done_handler(results);

                    // 处理扫描结果
                    update_connect = wifi_scan_done_process(results);
                }

                // 未成功扫描连接
                if ( !update_connect )
                {
                    // 未连接时，默认连接已开启且未蓝牙报错的STA，避免卡死
                    if ( !wifi_info.flag.bit.sta_link_ap ) {
                        ESP_LOGW(TAG,"WIFI Scan error!");
                        if (( 1 == wifi_mul_sta_en[0] ) && ( false == wifi_sta_stop_flag[0] )) {                        
                            iot_wifi_new_connect(WIFI_STA_INDEX1);
                        } else if (( 1 == wifi_mul_sta_en[1] ) && ( false == wifi_sta_stop_flag[1] )) {                        
                            iot_wifi_new_connect(WIFI_STA_INDEX2);
                        } else if (( 1 == wifi_mul_sta_en[2] ) && ( false == wifi_sta_stop_flag[2] )) {                        
                            iot_wifi_new_connect(WIFI_STA_INDEX3);
                        } else {
                            ESP_LOGE(TAG,"All of the WIFI-STA is invalid!");
                            wifi_info.sta_index = WIFI_STA_INDEX_STOP;
                            reals.wifi_eventgroup |= WIFI_STA_MULT_ALL_FAILED;
                        }
                    }
                }
                            
                // 释放扫描空间
                curr_ap_list_num = 0;
                if ( curr_ap_list != NULL ) {
                    free(curr_ap_list);
                    curr_ap_list = NULL;
                }
                
				reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_SCAN_DONE);
			}
			else if (reals.wifi_eventgroup & WIFI_STA_CONNECTED_AP)	/* wifi 连接网络 */
			{
				ESP_LOGI(TAG,"wifi connect to the ap successfull");
                cloud_diag_wifi_task_event("STA_CONNECTED_AP", "trigger iot_mqtt_start",
                                           wifi_info.flag.bit.sta_link_ap, 0);
				password_err_cnt = 0; 
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
                g_wifi_connect_error_cnt = 0;
#endif
				vTaskDelay(pdMS_TO_TICKS(200));
				iot_mqtt_start(NETIF_TYPE_WIFI_STA);
				iot_mqtt2_start(NETIF_TYPE_WIFI_STA);
//                iot_udp_start(NETIF_TYPE_WIFI_STA);
				wifi_info.flag.bit.sta_link_ap = 1;
				reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECTED_AP);
                reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_DISCONNECT_AP);
                reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);
				
			}
			else if (reals.wifi_eventgroup & WIFI_STA_DISCONNECT_AP)  /* WiFi断开网络 */
			{
                uint8_t sta_link_before = wifi_info.flag.bit.sta_link_ap;
                uint8_t mqtt_deleted = 0;
                ESP_LOGW(TAG,"wifi disconnect ap");
				if (sta_link_before == 1)
				{
					cloud_diag_wifi_task_event("STA_DISCONNECT_AP", "delete mqtt before sta_link clear",
					                           sta_link_before, 1);
					iot_mqtt_delete(NETIF_TYPE_WIFI_STA);
					iot_mqtt2_delete(NETIF_TYPE_WIFI_STA);
					iot_udp_delete(NETIF_TYPE_WIFI_STA);
                    mqtt_deleted = 1;
				}
                else
                {
                    cloud_diag_wifi_task_event("STA_DISCONNECT_AP",
                                               "sta_link already 0, mqtt_delete skipped(bug_fixed)",
                                               sta_link_before, 0);
                }
				wifi_info.flag.bit.sta_link_ap = 0;
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
				// if(0 == strcmp(wifi_info.sta_ssid,MESH_AP_SSID))//same	memcmp 
                if (false == is_root_node())
				{
					vTaskDelay(pdMS_TO_TICKS(10000));//windy change from 1s to5s
				
					esp_wifi_connect(); // 重新联网
					reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_DISCONNECT_AP);
					
				}
				else//连接外部网络
#endif
				{
					vTaskDelay(pdMS_TO_TICKS(5000));//windy change from 1s to5s
					if(1 == SetData.dev_info_t.on_off.bit.wifi_enable)//enable
					{
					
						reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_DISCONNECT_AP);
						if (password_err_cnt <= MAX_PASSWORD_ERR_CNT) 
						{		// wifi密码错误达到一定上限时停止连接wifi
						
							esp_wifi_connect(); // 重新联网
						}
						else 
						{
							ESP_LOGW(TAG, "wifi stopped reconnecting due to password errors reaching the maximum limit");
						}						

					}
					else//禁止AP,不自动重连
					{

					}
                    (void)mqtt_deleted;
				}

			}
			else if (reals.wifi_eventgroup & WIFI_STA_CONNECT_FAILED) /* 连接WiFi热点失败 */
			{
                wifi_ap_record_t ap_info;
                if (ESP_OK == esp_wifi_sta_get_ap_info(&ap_info)) {
                    wifi_info.flag.bit.sta_link_ap = 1;
                    reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);
                    continue;
                }
                uint8_t sta_link_before = wifi_info.flag.bit.sta_link_ap;
                ESP_LOGW(TAG,"wifi connect failed");
                cloud_diag_wifi_task_event("STA_CONNECT_FAILED", "wifi connect failed",
                                           sta_link_before, 0);
				if (sta_link_before == 1)
				{
					iot_mqtt_delete(NETIF_TYPE_WIFI_STA);
					iot_mqtt2_delete(NETIF_TYPE_WIFI_STA);
					iot_udp_delete(NETIF_TYPE_WIFI_STA);
				}
				wifi_info.flag.bit.sta_link_ap = 0;

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
				if(0 == strcmp(wifi_info.sta_ssid,MESH_AP_SSID))//same	memcmp
				{
					vTaskDelay(pdMS_TO_TICKS(10000));//windy change from 1s to5s
					esp_wifi_connect(); // 重新联网
					reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);
				}
				else
#endif
				{
					vTaskDelay(pdMS_TO_TICKS(5000));//windy change from 1s to5s
					if(1 == SetData.dev_info_t.on_off.bit.wifi_enable)//enable
					{
						// wifi密码错误达到一定上限时
						if (password_err_cnt > MAX_PASSWORD_ERR_CNT) {         
							ESP_LOGW(TAG, "wifi stopped reconnecting due to password errors reaching the maximum limit");
							password_err_cnt = 0;
							for ( uint8_t i = 0 ; i < WIFI_STA_INDEX_MAX ; i++ )
							{
								// 停止连接该wifi，直到下次更新WIFI STA参数
								if(wifi_info.sta_index == wifi_mmult_sta_addr[i].sta_index) {
									wifi_sta_stop_flag[i] = true;
									break;
								}
							}
						}
						
						reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);
						
						// 重新扫描是否有可用WIFI
						iot_wifi_auto_connect();

					}
					else//禁止AP,不自动重连
					{

					}
				}

			}
			else if (reals.wifi_eventgroup & WIFI_CLOSE_STA)  /* 关闭WiFi STA模式 */
			{
                ESP_LOGW(TAG,"wifi close sta");
				password_err_cnt = 0;
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
                g_wifi_connect_error_cnt = 0;
#endif
				iot_mqtt_delete(NETIF_TYPE_WIFI_STA);			// 该处关闭MQTT后延时了1s,而在这1s内wifi可能重新连接上AP导致产生了WIFI_STA_CONNECTED_AP事件
				iot_mqtt2_delete(NETIF_TYPE_WIFI_STA);
                iot_udp_delete(NETIF_TYPE_WIFI_STA);
				vTaskDelay(pdMS_TO_TICKS(1000));
				ESP_ERROR_CHECK(esp_wifi_stop());
				wifi_info.flag.bit.sta_link_ap = 0;
				wifi_info.flag.bit.sta_enable = 0;
				/* 等待该事件后放弃本次事件,因为该事件可能会导致MQTT任务重连 */
	//			xEventGroupWaitBits(wifi_eventgroup, bit_mask, pdTRUE, pdFALSE, pdMS_TO_TICKS(100));//windy change:为了改为非阻塞
				ESP_LOGW(TAG,"wifi sta closed");
				reals.wifi_eventgroup &= ~((uint16_t)WIFI_CLOSE_STA);
				
			}
			else if (reals.wifi_eventgroup & WIFI_OPEN_STA) /* 开启WiFi STA模式 */
			{
                ESP_LOGW(TAG,"wifi open sta");
				password_err_cnt = 0;
				s_retry_num = 0;
				wifi_info.flag.bit.sta_enable = 1;
				wifi_info.flag.bit.sta_link_ap = 0;
				reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);

				esp_wifi_disconnect(); /* 断开WiFi连接 */
				esp_wifi_stop(); // 关闭WiFi

//				wifi_config_t wifi_config2 = {
//					.sta = {
//						.ssid = MESH_AP_SSID,
//						.password = MESH_AP_PASSWORD
//					},
//				};
//
//				ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config2));//wifi_config

				ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));//  自己作为STA的 底层配置
				ESP_LOGI(TAG,"WIFI_IF_STA  CCC: wifi_config.sta.ssid=%s;wifi_config.sta.password=%s",wifi_config.sta.ssid,wifi_config.sta.password);

				ESP_ERROR_CHECK(esp_wifi_start());

				reals.wifi_eventgroup &= ~((uint16_t)WIFI_OPEN_STA);
	
				ESP_LOGI(TAG,"wifi sta open");
			}
			else if (reals.wifi_eventgroup & WIFI_STA_NEW_AP) /* WiFi重新配网;自己作为STA，接入其他AP */
			{
                ESP_LOGW(TAG,"wifi sta new ap");
				password_err_cnt = 0;
				wifi_info.flag.bit.sta_enable = 1;
				wifi_info.flag.bit.sta_link_ap = 0;
				if (wifi_info.sta_index < WIFI_STA_INDEX1
				    || wifi_info.sta_index > WIFI_STA_INDEX3) {
					wifi_info.sta_index = WIFI_STA_INDEX1;
				}
				iot_mqtt_delete(NETIF_TYPE_WIFI_STA); /* 删除MQTT */
				iot_mqtt2_delete(NETIF_TYPE_WIFI_STA);
                iot_udp_delete(NETIF_TYPE_WIFI_STA);
				vTaskDelay(pdMS_TO_TICKS(1000));
				s_retry_num = 0;
				reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);
				esp_wifi_disconnect(); /* 断开WiFi连接 */
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
                g_wifi_connect_error_cnt = 0;
                // ESP_LOGI(TAG,"WIFI_STA_NEW_AP : wifi_config_ap.ap.ssid=%s, authmod:%d", 
                // wifi_config_ap.ap.ssid, wifi_config_ap.ap.authmode);
                // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
                // ESP_LOGI(TAG,"WIFI_STA_NEW_AP : wifi_config_ap.ap.ssid=%s",wifi_config_ap.ap.ssid);
                // vTaskDelay(pdMS_TO_TICKS(1000));
#endif
				ESP_LOGI(TAG,"WIFI_IF_STA  AAA: wifi_config.sta.ssid=%s;wifi_config.sta.password=%s",wifi_config.sta.ssid,wifi_config.sta.password);

                // STA静态IP相关逻辑
                esp_err_t err = wifi_sta_dhcp_process(wifi_info.sta_index);
                if (err == ESP_OK) {
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                    ESP_LOGI(TAG,"WIFI_IF_STA : wifi_config.sta.ssid=%s",wifi_config.sta.ssid);
                    esp_wifi_connect();  /* 开启WiFi连接 */
                } else {           
                    reals.wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
                    password_err_cnt = 0;
                    s_retry_num = 0;
                }

				reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_NEW_AP);
			}
			else if (reals.wifi_eventgroup & WIFI_OPEN_AP)
            {
                ESP_LOGW(TAG,"wifi open ap");
                wifi_info.flag.bit.ap_enable = 1;
                wifi_info.flag.bit.ap_is_link = 0;

                wifi_mode_t wifi_mode;
                esp_wifi_get_mode(&wifi_mode);
                if (WIFI_MODE_AP == wifi_mode || WIFI_MODE_APSTA == wifi_mode)
                {
                    if (wifi_info.flag.bit.sta_enable != 1)
                    {
                        esp_wifi_stop(); // 关闭WiFi
                    }

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
                    esp_wifi_scan_stop();
                    g_wifi_connect_error_cnt = 0;
#endif
                    // esp_wifi_scan_start(get_wifi_scan_config(), false);
                    esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap);//自己作为AP的底层配置
                    ESP_LOGI(TAG,"WIFI_IF_STA  BBB: wifi_config_ap.ap.ssid=%s;wifi_config.sta.password=%s",
                            wifi_config_ap.ap.ssid,wifi_config_ap.sta.password);

                    if (wifi_info.flag.bit.sta_enable != 1) //未开启Sta 则start wifi
                    {
                        ESP_ERROR_CHECK(esp_wifi_start());
                    }

                    // 修改AP模式下检测到Sta离线的超时时间为30s, ESP32默认为5min
                    esp_wifi_set_inactive_time(WIFI_IF_AP, 30);
                    esp_wifi_set_inactive_time(WIFI_IF_STA, 5);
                }

                reals.wifi_eventgroup &= ~((uint16_t)WIFI_OPEN_AP);
            }
            else if (reals.wifi_eventgroup & WIFI_AP_NEW_AP)
            {
                esp_wifi_deauth_sta(0); //踢掉所有STA
                wifi_ap_netif_config();
                reals.wifi_eventgroup &= ~((uint16_t)WIFI_AP_NEW_AP);
            }
            else if ( reals.wifi_eventgroup & WIFI_STA_MULT_ALL_FAILED )
            {
                if ( wifi_sta_update_flag )
                {
                    wifi_sta_update_flag = false;

                    // 无有效WIFI时重新更新扫描
                    if((wifi_info.sta_index == WIFI_STA_INDEX_STOP) 
                        && !wifi_info.flag.bit.sta_link_ap )
                    {
                        ESP_LOGW(TAG, "STA index is invalid! System update, reconnect...");
                        iot_wifi_auto_connect();
                    }
                        
                    reals.wifi_eventgroup &= ~((uint16_t)WIFI_STA_MULT_ALL_FAILED);
                }
            }
            else
			{
				/* 12000 WiFi 变更由 Modbus 回调 / iot_cfg_check 调 iot_wifi_reconnect_sta 处理 */
				if (wifi_sta_config_flag) {
					wifi_sta_config_flag = 0;
				}
			}
		}

    }
}

/*
给输入参数赋值
*/
static void iot_wifi_get_cfg_ap(uint8_t *auth, char *ssid, char *pwd) 
{
    *auth = wifi_config.sta.threshold.authmode;
    memcpy(ssid, wifi_config.sta.ssid,     sizeof(wifi_config.sta.ssid));
    memcpy(pwd,  wifi_config.sta.password, sizeof(wifi_config.sta.password));
}

/* 目标 STA 配置与当前 wifi_config 一致（逻辑与 iot_wifi_new_ap 写入规则相同） */
static bool iot_wifi_sta_target_same_as_config(uint8_t auth, const char *ssid, const char *pwd)
{
    uint8_t target_auth;
    char target_pwd[65] = {0};

    if (!strlen((const char *)pwd)) {
        target_auth = WIFI_AUTH_OPEN;
    } else {
        target_auth = auth;
        if (auth == WIFI_AUTH_OPEN) {
            target_pwd[0] = '\0';
        } else {
            strncpy(target_pwd, pwd, sizeof(target_pwd) - 1);
        }
    }

    if (strncmp(ssid, (const char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid)) != 0) {
        return false;
    }
    if (target_auth != wifi_config.sta.threshold.authmode) {
        return false;
    }
    if (strncmp(target_pwd, (const char *)wifi_config.sta.password, sizeof(wifi_config.sta.password)) != 0) {
        return false;
    }

    return true;
}

static void iot_wifi_new_ap_skip_same_linked(const char *ssid)
{
    ESP_LOGW(TAG, "skip WIFI_STA_NEW_AP: same AP already linked, ssid=%s", ssid);
    cloud_diag_wifi_task_event("NEW_AP_SKIP", ssid, 1, 0);
    /* 原 NEW_AP 会断网重连并再次 iot_mqtt_start；跳过断网后需补启 MQTT */
    if (!iot_mqtt_login_state()) {
        iot_mqtt_start(NETIF_TYPE_WIFI_STA);
        cloud_diag_wifi_task_event("NEW_AP_SKIP", "mqtt_start", 1, 0);
    }
}

const wifi_info_t * iot_get_wifi_info(void) {
    return (const wifi_info_t *)&wifi_info;
}

const wifi_info_t * iot_get_4g_info(void) {
    return (const wifi_info_t *)&_4g_info;
}

const wifi_info_t * iot_get_eth_info(void) {
    return (const wifi_info_t *)&eth_info;
}

/* 关闭WiFi(STA AP) */
void iot_wifi_close(uint8_t mode) {
//    if (wifi_eventgroup == NULL) {
//        return ;
//    }
	ESP_LOGE(TAG, "wifi close!!!!!!!!!!");
    iot_wifi_close_smartcfg();

    switch (mode)
    {
        case WIFI_MODE_STA:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_STA);
            reals.wifi_eventgroup |= WIFI_CLOSE_STA;
			
            break;

        case WIFI_MODE_AP:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_AP);
            reals.wifi_eventgroup |= WIFI_CLOSE_AP;
			
            break;

        case WIFI_MODE_ALL:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_STA);
            reals.wifi_eventgroup |= WIFI_CLOSE_STA;
			
            vTaskDelay(pdMS_TO_TICKS(50));
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_AP);
            reals.wifi_eventgroup |= WIFI_CLOSE_AP;
			
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* 开启WiFi(STA AP) */
void iot_wifi_open(uint8_t mode) 
{
//    if (wifi_eventgroup == NULL) {
//        return ;
//    }

    switch (mode)
    {
        case WIFI_MODE_STA:
            reals.wifi_eventgroup |= WIFI_OPEN_STA;
			
            break;
        case WIFI_MODE_AP:
            reals.wifi_eventgroup |= WIFI_OPEN_AP;
			
            break;
        case WIFI_MODE_ALL:
            reals.wifi_eventgroup |= WIFI_OPEN_AP|WIFI_OPEN_STA;
			
            break;		
    }
}

void iot_wifi_reset(void) {
    iot_wifi_close(WIFI_MODE_ALL);
    vTaskDelay(pdMS_TO_TICKS(50));
    iot_wifi_open(WIFI_MODE_STA);
    vTaskDelay(pdMS_TO_TICKS(50));
}

const char * iot_wifi_get_host(void) {
    return iot_mqtt_get_host();
}

void iot_wifi_new_host(const char *host) {
    iot_mqtt_new_host(host);
    vTaskDelay(pdMS_TO_TICKS(50));
}

/**
 * @brief 配置WiFi-AP信息
 * @details 暂不支持配置IP网关等信息
 */
void iot_wifi_ap_config(uint8_t auth, const char *ssid, const char *pwd)
{
    if (!strlen((const char *)ssid))
    {
        return ;
    }

    ESP_LOGW(TAG, "iot wifi ap config, enable:%d ssid:%s, pwd:%s, auth:%d",
                    wifi_info.flag.bit.ap_enable, ssid, pwd, auth);

    memcpy(wifi_config_ap.ap.ssid, ssid, 32);
    wifi_config_ap.ap.ssid_len = strlen(ssid);
    wifi_config_ap.ap.ssid_hidden = 0;
    wifi_config_ap.ap.channel = 0;
    wifi_config_ap.ap.max_connection = 4;
    wifi_config_ap.ap.pmf_cfg.required = true;

    // AP密码必须超过8个字符，否则会设置AP失败
    if (!strlen((const char *)pwd) || strlen((const char *)pwd) < WIFI_AP_PWD_LEN_MIN)
    {
        wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN; // 密码认证方式
        memset(wifi_config_ap.ap.password, '\0', 64);
    }
    else
    {
        wifi_config_ap.ap.authmode = auth;
        memcpy(wifi_config_ap.ap.password, pwd, 64); // 密码认证方式
    }

    //使能AP但未打开，则重新打开
    if (1 == SetData.dev_info_t.on_off.bit.wifi_ap_enable
        && 1 !=wifi_info.flag.bit.ap_enable)
    {
        iot_wifi_open(WIFI_MODE_AP);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    if (wifi_info.flag.bit.ap_enable == 1) 
    {
        reals.wifi_eventgroup |= WIFI_AP_NEW_AP;
    }
}
void iot_wifi_ap_config_get(uint8_t *auth, const char *ssid, const char *pwd)
{
    char wifi_ap_ssid[32] = {0};
    if (SetData.dev_info_t.Parallel_dev_sn)
    {
        snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid), "%s%llu", SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);
    }
    else
    {
        snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);
    }
    memset(SetData.dev_info_t.wifi_ap_ssid, 0x00, sizeof(SetData.dev_info_t.wifi_ap_ssid));
    memcpy(SetData.dev_info_t.wifi_ap_ssid, wifi_ap_ssid, sizeof(wifi_ap_ssid));

    //注意: AP密码必须超过8个字符
    char wifi_ap_password[64] = {0};
    uint8_t md5_value[16] = {0};
    snprintf(wifi_ap_password, sizeof(wifi_ap_password), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);
    // 32位小写MD5
    calculate_md5((uint8_t *)wifi_ap_password, strlen(wifi_ap_password), md5_value);
    snprintf(wifi_ap_password, sizeof(wifi_ap_password), "%02x%02x%02x%02x%02x%02x%02x%02x",
        md5_value[0], md5_value[2], md5_value[8], md5_value[3], md5_value[4], md5_value[6], md5_value[12], md5_value[10]);
    memset(SetData.dev_info_t.wifi_ap_password, 0x00, sizeof(SetData.dev_info_t.wifi_ap_password));
    memcpy(SetData.dev_info_t.wifi_ap_password, wifi_ap_password, sizeof(wifi_ap_password));
}

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
static void iot_wifi_apply_sta_config(uint8_t auth, const char *ssid, const char *pwd,
                                      const char *bssid, uint8_t force_reconnect)
{
    if (!strlen((const char *)ssid)) {
        return;
    }

    if (!force_reconnect
        && wifi_info.flag.bit.sta_link_ap
        && iot_wifi_sta_target_same_as_config(auth, ssid, pwd)) {
        iot_wifi_new_ap_skip_same_linked(ssid);
        return;
    }

    password_err_cnt = 0;
    memset(wifi_sta_stop_flag, 0, sizeof(wifi_sta_stop_flag));
    wifi_info.sta_index = WIFI_STA_INDEX1;

    memcpy(wifi_config.sta.ssid, ssid, 32);
    if (!strlen((const char *)pwd)) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        memset(wifi_config.sta.password, '\0', 64);
    } else {
        if (auth == WIFI_AUTH_OPEN) {
            auth = WIFI_AUTH_WPA2_PSK;
        }
        wifi_config.sta.threshold.authmode = auth;
        memcpy(wifi_config.sta.password, pwd, 64);
    }

    if (NULL != bssid) {
        memcpy(wifi_config.sta.bssid, bssid, 6);
    } else {
        memset(wifi_config.sta.bssid, 0, 6);
    }

    if (force_reconnect) {
        ESP_LOGW(TAG, "force reconnect to AP, ssid: %s, bssid: " MACSTR ", auth: %d",
                 wifi_config.sta.ssid, MAC2STR(wifi_config.sta.bssid),
                 wifi_config.sta.threshold.authmode);
    } else {
        ESP_LOGW(TAG, "connect to a new AP, ssid: %s, bssid: " MACSTR ", pwd: %s, auth mode: %d",
                 wifi_config.sta.ssid, MAC2STR(wifi_config.sta.bssid),
                 wifi_config.sta.password, wifi_config.sta.threshold.authmode);
    }
    iot_wifi_get_cfg_ap(&wifi_info.sta_auth, wifi_info.sta_ssid, wifi_info.sta_pwd);
    if (wifi_info.flag.bit.sta_enable == 1) {
        reals.wifi_eventgroup |= WIFI_STA_NEW_AP;
    } else {
        reals.wifi_eventgroup |= WIFI_OPEN_STA;
    }
}

void iot_wifi_new_ap(uint8_t auth, const char *ssid, const char *pwd, const char *bssid)
{
    iot_wifi_apply_sta_config(auth, ssid, pwd, bssid, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void iot_wifi_reconnect_sta(uint8_t auth, const char *ssid, const char *pwd, const char *bssid)
{
    iot_wifi_apply_sta_config(auth, ssid, pwd, bssid, 1);
}
#else
static void iot_wifi_apply_sta_config(uint8_t auth, const char *ssid, const char *pwd, uint8_t force_reconnect)
{
    if (!strlen((const char *)ssid)) {
        return;
    }

    if (!force_reconnect
        && wifi_info.flag.bit.sta_link_ap
        && iot_wifi_sta_target_same_as_config(auth, ssid, pwd)) {
        iot_wifi_new_ap_skip_same_linked(ssid);
        return;
    }

    password_err_cnt = 0;
    memset(wifi_sta_stop_flag, 0, sizeof(wifi_sta_stop_flag));
    wifi_info.sta_index = WIFI_STA_INDEX1;

    memcpy(wifi_config.sta.ssid, ssid, 32);
    if (!strlen((const char *)pwd)) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        memset(wifi_config.sta.password, '\0', 64);
    } else {
        if (auth == WIFI_AUTH_OPEN) {
            auth = WIFI_AUTH_WPA2_PSK;
        }
        wifi_config.sta.threshold.authmode = auth;
        memcpy(wifi_config.sta.password, pwd, 64);
    }

    if (force_reconnect) {
        ESP_LOGW(TAG, "force reconnect to AP, ssid: %s, auth: %d",
                 wifi_config.sta.ssid, wifi_config.sta.threshold.authmode);
    } else {
        ESP_LOGW(TAG, "connect to a new AP, ssid: %s, pwd: %s, auth mode: %d",
                 wifi_config.sta.ssid, wifi_config.sta.password,
                 wifi_config.sta.threshold.authmode);
    }
    iot_wifi_get_cfg_ap(&wifi_info.sta_auth, wifi_info.sta_ssid, wifi_info.sta_pwd);
    if (wifi_info.flag.bit.sta_enable == 1) {
        reals.wifi_eventgroup |= WIFI_STA_NEW_AP;
    } else {
        reals.wifi_eventgroup |= WIFI_OPEN_STA;
    }
}

void iot_wifi_new_ap(uint8_t auth, const char *ssid, const char *pwd)
{
    iot_wifi_apply_sta_config(auth, ssid, pwd, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void iot_wifi_reconnect_sta(uint8_t auth, const char *ssid, const char *pwd)
{
    iot_wifi_apply_sta_config(auth, ssid, pwd, 1);
}
#endif
void iot_wifi_new_dev(const char *dev_type, uint64_t dev_sn) 
{
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
    memcpy(g_dev_type, dev_type, sizeof(g_dev_type));
    g_dev_sn = dev_sn;
    is_root_node();
#endif
    iot_mqtt_new_dev(dev_type, dev_sn);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void iot_wifi_new_iot(const char *iot_type, uint64_t iot_sn, uint64_t safety_code) {
    iot_mqtt_new_iot(iot_type, iot_sn, safety_code);
    vTaskDelay(pdMS_TO_TICKS(50));
}

int8_t iot_wifi_ap_rssi(void) {
    if (wifi_info.flag.bit.sta_link_ap == 1) {
        wifi_ap_record_t ap_info;
        esp_wifi_sta_get_ap_info(&ap_info);
        return ap_info.rssi;
    }
    return 0;
}

void iot_wifi_open_smartcfg(void) {
//    if ((wifi_info.flag.bit.sta_enable == 1) && wifi_eventgroup) 
	if(wifi_info.flag.bit.sta_enable == 1)
	{ /* 开启smartlink */
//        xEventGroupSetBits(wifi_eventgroup, WIFI_STA_SMARTCFG_DOING);
		reals.wifi_eventgroup |= WIFI_STA_SMARTCFG_DOING;
		
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

void iot_wifi_close_smartcfg(void) {
//    if (wifi_eventgroup) 
	{ /* 关闭smartlink */
//        xEventGroupSetBits(wifi_eventgroup, WIFI_STA_SMARTCFG_DONE);
		reals.wifi_eventgroup |= WIFI_STA_SMARTCFG_DONE;
		
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

void iot_mqtt_state(void) {
    wifi_info.flag.bit.mqtt_login = iot_mqtt_login_state();
}

// static void dump_buf(char *info, uint8_t *buf, uint32_t len)
// {
//     printf("[MESH] %s", info);
//     for (int i = 0; i < len; i++) {
//         printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
//                         buf[i], i == len - 1 ? "\n":"");
//     }
// }

/*------------------------------------------------------------------------------
 Function: iot_wifi_connect_status_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI连接异常但未触发事件情况下重新连接WIFI(本函数仅在连接异常下触发)
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_wifi_connect_status_check(void) {
    wifi_ap_record_t ap_info;
    static uint8_t cnt = 0;
    if ((wifi_info.flag.bit.sta_link_ap)
        &&(esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK))  
    {
        if ( ++cnt >= 3 )
        {
            cnt = 0;
            ESP_LOGE(TAG, "WiFi not connected (unknown error), attempting to reconnect...");
            // 重连当前WIFI
            iot_wifi_new_connect(wifi_info.sta_index);
        }
    }
    else
    {
        cnt = 0;//未连接，不在本函数内处理逻辑
    }
}

/*------------------------------------------------------------------------------
| Function: iot_wifi_new_connect
| -----------------------------------------------------------------------------*/
/**
  * @brief      发起新的STA连接
  * @param[in]  uint8_t STA_index  
  * @param[out] None
  * @return     void
  */
void iot_wifi_new_connect(uint8_t STA_index)
{
    if((!reals.event_wifi_register_flag) || (1 != wifi_info.flag.bit.sta_enable)) {
        ESP_LOGE(TAG, "WIFI State Error : event_wifi_register_flag(%d), sta_enable(%d)", 
                 reals.event_wifi_register_flag, wifi_info.flag.bit.sta_enable);
        return;
    }
    
    bool found = false;
    for ( uint8_t i = 0 ; i < WIFI_STA_INDEX_MAX ; i++ )
    {
        // 找到对应的存储STA
        if ( STA_index == wifi_mmult_sta_addr[i].sta_index ) {
            // 连接指定WIFI
            found = true;
            wifi_info.sta_index = STA_index;
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
            iot_wifi_new_ap(*wifi_mmult_sta_addr[i].sta_auth, 
                            wifi_mmult_sta_addr[i].ssid, 
                            wifi_mmult_sta_addr[i].password,
                            NULL);
#else
            iot_wifi_new_ap(*wifi_mmult_sta_addr[i].sta_auth, 
                            wifi_mmult_sta_addr[i].ssid, 
                            wifi_mmult_sta_addr[i].password);
#endif
            
            ESP_LOGW(TAG, "STA(%d) updated and enabled, connecting to STA(%d)", STA_index, wifi_info.sta_index);
            break;
        }
    }
    if (!found) {
        ESP_LOGE(TAG, "STA config for index %d not found, cannot connect.", STA_index);
    }
}

/*------------------------------------------------------------------------------
| Function: iot_wifi_sta_index_get
| -----------------------------------------------------------------------------*/
/**
  * @brief      获取当前连接的STA标签
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_wifi_sta_index_get(void)
{
    return wifi_info.sta_index; 
}

/*------------------------------------------------------------------------------
| Function: iot_wifi_sta_link_status_get
| -----------------------------------------------------------------------------*/
/**
  * @brief      获取当前STA的链接状态
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_wifi_sta_link_status_get(void)
{
    return wifi_info.flag.bit.sta_link_ap;
}

/*------------------------------------------------------------------------------
| Function: iot_wifi_auto_connect
| -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI自动连接
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_wifi_auto_connect(void)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };
    esp_wifi_scan_start(&scan_config, false); // non-blocking
    ESP_LOGI(TAG, "WiFi scan started...");
}

/*------------------------------------------------------------------------------
| Function: iot_wifi_base_cfg_update
| -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI基本设置更新
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_wifi_base_cfg_update(void)
{
    uint8_t sta_cnt = 0;

    // 更新当前WIFI STA使能数量
    if (1 == SetData.dev_info_t.wifi_mul_sta_en.sta1_en) sta_cnt++;
    if (1 == SetData.dev_info_t.wifi_mul_sta_en.sta2_en) sta_cnt++;
    if (1 == SetData.dev_info_t.wifi_mul_sta_en.sta3_en) sta_cnt++;
    wifi_sta_num = sta_cnt;

    // WIFI STA使能标志
    wifi_mul_sta_en[0] = SetData.dev_info_t.wifi_mul_sta_en.sta1_en;
    wifi_mul_sta_en[1] = SetData.dev_info_t.wifi_mul_sta_en.sta2_en;
    wifi_mul_sta_en[2] = SetData.dev_info_t.wifi_mul_sta_en.sta3_en;

    // WIFI STA静态IP使能标志
    wifi_mul_sta_static_ip_en[0] = SetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en;
    wifi_mul_sta_static_ip_en[1] = SetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en;
    wifi_mul_sta_static_ip_en[2] = SetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en;
    
    // 清除WIFI错误标志
    memset(wifi_sta_stop_flag, 0, sizeof(wifi_sta_stop_flag));
}

/*------------------------------------------------------------------------------
| Function: wifi_sta_update_flag_set
| -----------------------------------------------------------------------------*/
/**
  * @brief      设置WIFI STA更新标志
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void wifi_sta_update_flag_set(void)
{
    wifi_sta_update_flag = true;
}


