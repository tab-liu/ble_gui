#include "iot_wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_smartconfig.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "iot_mqtt.h"
#include "filesystem.h"
#include "parameter.h"
#include "task_debug.h"
#include "wlcc_common.h"
#include "wlcc_process.h"
#include "iot_sleep.h"
#include "comm_define.h"
#include "esp_wifi_types.h"
#include "log_redirect.h"
#include "utils.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define TAG "[WIFI]"

/*------------------------------------------------------------------------------*/

#define WIFI_STA_CONNECTED_AP      BIT0
#define WIFI_STA_DISCONNECT_AP     BIT1
#define WIFI_STA_CONNECT_FAILED    BIT2
#define WIFI_STA_CONNECT_TIMEOUT   BIT3
#define WIFI_STA_PERIOD_SCAN       BIT4
#define WIFI_STA_SCAN_DONE         BIT5
#define WIFI_OPEN_AP               BIT6
#define WIFI_OPEN_STA              BIT7
#define WIFI_CLOSE_AP              BIT8
#define WIFI_CLOSE_STA             BIT9
#define WIFI_STA_NEW_AP            BIT10
#define WIFI_AP_NEW_AP             BIT11
#define WIFI_STA_MULT_ALL_FAILED   BIT12

#define MAX_RETRY	            8
#define MAX_PASSWORD_ERR_CNT	4
#define MAX_AUTH_EXPIRE_RETRY   3

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

/*------------------------------------------------------------------------------*/

/*
listen_interval 推荐设置范围一般为 3~10。
(在 WIFI_PS_MIN_MODEM 或非省电模式下，listen_interval 设置不会影响唤醒频率，设备会按照默认频率（通常每个 Beacon 间隔）唤醒)
3~5：兼顾低功耗和网络实时性，适合大多数 IoT 场景。
6~10：更省电，但可能会延迟接收数据，适合对实时性要求不高的设备（如传感器、周期性上报设备）。
*/
USE_EXT_RAM_BSS static wifi_config_t wifi_config = {
    .sta = {
        .listen_interval = 5, // 只在 WIFI_PS_MAX_MODEM 下有效
    },
};//硬件底层配置直接结构体

static int password_err_cnt = 0;
static uint8_t s_retry_num = 0;
USE_EXT_RAM_BSS wifi_info_t wifi_info = {0};//RAM中间变量集合static 

USE_EXT_RAM_BSS static wifi_config_t wifi_config_ap = {0};//硬件底层配置直接结构体
static uint8_t event_wifi_register_flag = 0;//1-已注册；0-未注册，互斥用
static uint16_t wifi_eventgroup = 0;//将阻塞改为非阻塞		

static SemaphoreHandle_t wifi_semaphore = NULL;

static const wifi_mmult_sta_base_t wifi_mmult_sta_addr[WIFI_STA_INDEX_MAX] = {
    {WIFI_STA_INDEX1,   &IotSetData.dev_info_t.wifi_sta_auth,   IotSetData.dev_info_t.wifi_sta_ssid,    IotSetData.dev_info_t.wifi_sta_password},
    {WIFI_STA_INDEX2,   &IotSetData.dev_info_t.wifi_sta2_auth,  IotSetData.dev_info_t.wifi_sta2_ssid,   IotSetData.dev_info_t.wifi_sta2_password},
    {WIFI_STA_INDEX3,   &IotSetData.dev_info_t.wifi_sta3_auth,  IotSetData.dev_info_t.wifi_sta3_ssid,   IotSetData.dev_info_t.wifi_sta3_password}
};

static const wifi_mmult_sta_static_ip_t wifi_mmult_sta_unDHCP[WIFI_STA_INDEX_MAX] = {
    {WIFI_STA_INDEX1,   &IotSetData.dev_info_t.wifi_sta1_ip,    &IotSetData.dev_info_t.wifi_sta1_mask,  &IotSetData.dev_info_t.wifi_sta1_gw,    &IotSetData.dev_info_t.wifi_sta1_dns1,  &IotSetData.dev_info_t.wifi_sta1_dns2},
    {WIFI_STA_INDEX2,   &IotSetData.dev_info_t.wifi_sta2_ip,    &IotSetData.dev_info_t.wifi_sta2_mask,  &IotSetData.dev_info_t.wifi_sta2_gw,    &IotSetData.dev_info_t.wifi_sta2_dns1,  &IotSetData.dev_info_t.wifi_sta2_dns2},
    {WIFI_STA_INDEX3,   &IotSetData.dev_info_t.wifi_sta3_ip,    &IotSetData.dev_info_t.wifi_sta3_mask,  &IotSetData.dev_info_t.wifi_sta3_gw,    &IotSetData.dev_info_t.wifi_sta3_dns1,  &IotSetData.dev_info_t.wifi_sta3_dns2}
};

static uint8_t wifi_sta_num = 0;
USE_EXT_RAM_BSS static bool wifi_sta_stop_flag[WIFI_STA_INDEX_MAX] = {false};
USE_EXT_RAM_BSS static uint8_t wifi_mul_sta_en[WIFI_STA_INDEX_MAX] = {0};
USE_EXT_RAM_BSS static uint8_t wifi_mul_sta_static_ip_en[WIFI_STA_INDEX_MAX] = {0};
static bool wifi_sta_update_flag = false;
static bool wifi_sta_internet_error_flag = false;
static uint8_t wifi_sta_reconnect_num = 0;

static uint16_t curr_ap_list_num = 0;
static wifi_ap_record_t *curr_ap_list = NULL;
static esp_timer_handle_t wifi_connect_wdt = NULL;

/*------------------------------------------------------------------------------*/

static void iot_wifi_get_cfg_ap(uint8_t *auth, char *ssid, char *pwd);
void wifi_ap_netif_config(void);

/*------------------------------------------------------------------------------*/

static void wifi_connect_wdt_cb(void* arg)
{
    ESP_LOGE(TAG, "wifi_connect_wdt_cb : wifi connect timeout!");
    
    // 超时视为连接失败，通知上层并尝试断开以清理状态
    wifi_eventgroup |= WIFI_STA_CONNECT_TIMEOUT;
}

static void ensure_wifi_wdt(void)
{
    if (wifi_connect_wdt == NULL) {
        const esp_timer_create_args_t args = {
            .callback = &wifi_connect_wdt_cb,
            .name = "wifi_connect_wdt"
        };
        esp_timer_create(&args, &wifi_connect_wdt);
    }
}

/* 封装 esp_wifi_connect 并打印结果 */
static esp_err_t iot_wifi_connect_safe(void)
{
    static uint8_t err_cnt = 0;

    if((!event_wifi_register_flag) || (1 != wifi_info.flag.bit.sta_enable)) {
        ESP_LOGE(TAG, "WIFI State Error : event_wifi_register_flag(%d), sta_enable(%d)", 
                 event_wifi_register_flag, wifi_info.flag.bit.sta_enable);
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_OK) {
        err_cnt = 0;
        // 在某些手机热点/随身WiFi 情况下，AP 可能对探测/关联不响应，导致既不触发成功也不触发明确失败（表现为“返回 OK 但无事件”），这时驱动可能处于等待/重试状态
        // 启动 20s 超时（按需调整毫秒）
        if(wifi_connect_wdt) esp_timer_start_once(wifi_connect_wdt, 20 * 1000 * 1000);
    } else {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
        err_cnt++;
        if (err_cnt <= 10) {
            wifi_eventgroup |= WIFI_STA_DISCONNECT_AP;
        } else {
            wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
            err_cnt = 0;
        }
    }
    return err;
}

/* 插入改进后的原因打印和字符串化函数 */
static const char* wifi_reason_to_str(int reason)
{
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:                return "AUTH_EXPIRE";
        case WIFI_REASON_ASSOC_LEAVE:                return "ASSOC_LEAVE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:    return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_AUTH_FAIL:                  return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_EXPIRE:               return "ASSOC_EXPIRE";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:          return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_MIC_FAILURE:                return "MIC_FAILURE";
        case WIFI_REASON_BEACON_TIMEOUT:             return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND:                return "NO_AP_FOUND";
        case WIFI_REASON_CONNECTION_FAIL:            return "CONNECTION_FAIL";
        case WIFI_REASON_ASSOC_FAIL:                 return "ASSOC_FAIL";
        /* 若需更多 reason，可在确认存在后追加对应 case */
        default: {
            static char unknown_reason[32];
            snprintf(unknown_reason, sizeof(unknown_reason), "UNKNOWN_REASON_%d", reason);
            return unknown_reason;
        }
    }
}

static const char* wifi_err_type_str(wifi_err_type_t t)
{
    switch (t) {
        case WIFI_ERR_PASSWORD: return "PASSWORD/4WAY/HANDSHAKE";
        case WIFI_ERR_AUTH:     return "AUTH_FAIL/EXPIRE";
        case WIFI_ERR_NO_AP:    return "NO_AP_FOUND/AP_TSF_RESET";
        case WIFI_ERR_BEACON_LOSS:return "BEACON_TIMEOUT/LOSS";
        case WIFI_ERR_ASSOC_FAIL:return "ASSOC_FAIL/EXPIRE/LEAVE";
        case WIFI_ERR_CHANNEL:  return "CHANNEL/INTERFERENCE";
        case WIFI_ERR_CAPACITY: return "AP_CAPACITY_LIMIT";
        default:                return "OTHER";
    }
}

static const char *wifi_auth_mode_to_str(wifi_auth_mode_t authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2_WPA3_PSK";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI_PSK";
    case WIFI_AUTH_OWE:             return "OWE";
    default:                        return "UNKNOWN";
    }
}

/* 非阻塞 TCP 连接，timeout_ms 毫秒 */
static int tcp_connect_timeout(const char *ip, uint16_t port, int timeout_ms)
{
    int sock = -1;
    int flags = 0, ret;
    struct sockaddr_in addr;
    fd_set wset;
    struct timeval tv;
    int so_error = 0;
    socklen_t len = sizeof(so_error);

    if (!ip) return -1;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGW(TAG, "socket create failed: %d", errno);
        return -1;
    }

    /* 设置非阻塞 */
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_aton(ip, &addr.sin_addr) == 0) {
        ESP_LOGW(TAG, "invalid ip string: %s", ip);
        close(sock);
        return -1;
    }

    ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == 0) {
        /* 立即连接成功 */
        fcntl(sock, F_SETFL, flags); // restore
        close(sock);
        return 0;
    } else if (ret < 0 && errno != EINPROGRESS) {
        ESP_LOGW(TAG, "connect() failed immediate: errno=%d", errno);
        close(sock);
        return -1;
    }

    /* 等待连接完成或超时 */
    FD_ZERO(&wset);
    FD_SET(sock, &wset);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(sock + 1, NULL, &wset, NULL, &tv);
    if (ret > 0 && FD_ISSET(sock, &wset)) {
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0) {
            if (so_error == 0) {
                fcntl(sock, F_SETFL, flags);
                close(sock);
                return 0;
            } else {
                ESP_LOGW(TAG, "connect failed after select: so_error=%d", so_error);
            }
        } else {
            ESP_LOGW(TAG, "getsockopt failed: errno=%d", errno);
        }
    } else if (ret == 0) {
        ESP_LOGW(TAG, "connect timeout");
    } else {
        ESP_LOGW(TAG, "select error: %d", errno);
    }

    close(sock);
    return -1;
}


/* 主检测函数：timeout_ms 为每步超时（ms） */
static internet_status_t wifi_check_internet(int timeout_ms)
{
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif) {
        ESP_LOGW(TAG, "no sta netif handle");
        return INTERNET_NO_IP;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
        ESP_LOGW(TAG, "no IP assigned");
        return INTERNET_NO_IP;
    }

    /* 1) DNS 解析测试（非阻塞/快速判定） */
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int rc;
    bool dns_ok = false;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; /* 希望解析出可连的地址 */

    rc = getaddrinfo("www.baidu.com", "80", &hints, &res);
    if (rc == 0 && res) {
        dns_ok = true;
        freeaddrinfo(res);
    } else {
        ESP_LOGW(TAG, "DNS resolve failed: %d", rc);
    }

    /* 2) 直连用于判路由的测试 IP */
    const char *test_ip = "8.8.8.8";

    if (tcp_connect_timeout(test_ip, 53, timeout_ms) == 0) {
        if (!dns_ok) {
            ESP_LOGW(TAG, "Internet reachable but DNS failed");
            return INTERNET_DNS_FAIL;
        }
        ESP_LOGI(TAG, "Internet reachable");
        return INTERNET_OK;
    } else {
        ESP_LOGW(TAG, "no route to internet (direct connect to %s:53 failed)", test_ip);
        return dns_ok ? INTERNET_NO_ROUTE : INTERNET_DNS_FAIL;
    }
}

/**
 * @brief 处理 STA 断开事件的集中逻辑（从“记录并更精细地判断断连原因”开始）
 *
 * 说明：
 * - 将底层断连 reason 映射到更高层的错误分类（wifi_err_type_t），便于统一策略处理；
 * - 对密码/认证类错误做专门限次重试/停机处理（使用 password_err_cnt 与 MAX_PASSWORD_ERR_CNT）；
 * - 对一般性/瞬时性错误采用重试计数 s_retry_num，与 MAX_RETRY 配合，超过阈值后判定连接失败；
 * - 日志统一使用 wifi_reason_to_str() 与 wifi_err_type_str() 提供可读性强的输出；
 * - 函数副作用：修改 wifi_eventgroup、password_err_cnt、s_retry_num 等。
 *
 * @param event_data 指向 wifi_event_sta_disconnected_t 的指针（来自事件系统）
 */
static void iot_wifi_handle_sta_disconnected(void *event_data)
{
    if (!event_data) {
        ESP_LOGW(TAG, "iot_wifi_handle_sta_disconnected: null event_data");
        return;
    }

    wifi_event_sta_disconnected_t *disconnected = (wifi_event_sta_disconnected_t *)event_data;
    int reason = disconnected->reason;
    static uint8_t auth_expire_retry = 0;

    /* 高层分类：便于日志与策略判断 */
    wifi_err_type_t last_wifi_err = WIFI_ERR_OTHER;

    switch (reason) {
        /* 密码 / 加密 / 四次握手 / MIC 失败 -> 高概率密码/加密相关 */
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            last_wifi_err = WIFI_ERR_PASSWORD;
            break;

        /* 认证失败（可能密码错或加密类型不匹配） */
        case WIFI_REASON_AUTH_FAIL:
            last_wifi_err = WIFI_ERR_AUTH;
            break;

        case WIFI_REASON_AUTH_EXPIRE:
            /* 把 AUTH_EXPIRE 当作偶发性/短暂问题，先限次重试，不立即归为认证错误或计入 password_err_cnt */
            if (auth_expire_retry < MAX_AUTH_EXPIRE_RETRY) {
                auth_expire_retry++;
                ESP_LOGW(TAG, "AUTH_EXPIRE transient (%d/%d), retrying without counting as password/auth error",
                         auth_expire_retry, MAX_AUTH_EXPIRE_RETRY);
                last_wifi_err = WIFI_ERR_OTHER;
            } else {
                auth_expire_retry = 0;
                last_wifi_err = WIFI_ERR_AUTH;
            }
            break;

        /* 找不到 AP 或 AP 信息异常（SSID 不在范围 / AP TSF 重置） */
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_AP_TSF_RESET:
            last_wifi_err = WIFI_ERR_NO_AP;
            break;

        /* Beacon 超时 -> 信号弱或 AP 切换（短期不稳定） */
        case WIFI_REASON_BEACON_TIMEOUT:
            last_wifi_err = WIFI_ERR_BEACON_LOSS;
            break;

        /* 关联失败/超时/主动离开 -> 关联层问题 */
        case WIFI_REASON_ASSOC_FAIL:
        case WIFI_REASON_ASSOC_EXPIRE:
        case WIFI_REASON_ASSOC_LEAVE:
            last_wifi_err = WIFI_ERR_ASSOC_FAIL;
            break;

        /* 连接/握手类的通用失败 */
        case WIFI_REASON_CONNECTION_FAIL:
            last_wifi_err = WIFI_ERR_OTHER;
            break;

        default:
            last_wifi_err = WIFI_ERR_OTHER;
            break;
    }

    /* 记录分类与原因，便于诊断 */
    ESP_LOGI(TAG, "STA disconnected: reason=%d (%s), classified=%s",
             reason, wifi_reason_to_str(reason), wifi_err_type_str(last_wifi_err));

    /* 密码/认证相关：优先按密码错误计数处理，避免无休止重试 */
    if (last_wifi_err == WIFI_ERR_PASSWORD || last_wifi_err == WIFI_ERR_AUTH) {
        password_err_cnt++;
        ESP_LOGW(TAG, "auth/password error detected (%s). password_err_cnt(%d/%d)",
                 wifi_reason_to_str(reason), password_err_cnt, MAX_PASSWORD_ERR_CNT);

        /* 达到密码错误上限 -> 标记连接失败并重置重试计数 */
        if (password_err_cnt >= MAX_PASSWORD_ERR_CNT) {
            ESP_LOGE(TAG, "password error count >= %d, mark as connect failed", MAX_PASSWORD_ERR_CNT);
            wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
            s_retry_num = 0;
            /* 不立即清 password_err_cnt，让上层或用户介入后显式清零或重置 */
        } else {
            /* 密码错误但未达到阈值 -> 发起重新连接（由事件循环处理） */
            wifi_eventgroup |= WIFI_STA_DISCONNECT_AP;
        }
        return;
    }

    /* 非密码/认证类错误：按一般重试逻辑：短暂错误先重试，重试耗尽则判定连接失败 */
    if (s_retry_num < MAX_RETRY) {
        s_retry_num++;
        ESP_LOGW(TAG, "transient disconnect (%s). will retry %d/%d", wifi_reason_to_str(reason), s_retry_num, MAX_RETRY);
        wifi_eventgroup |= WIFI_STA_DISCONNECT_AP;
    } else {
        ESP_LOGE(TAG, "retries exhausted (%d). mark connect failed for SSID:%s (%s)",
                 s_retry_num, wifi_config.sta.ssid, wifi_reason_to_str(reason));
        wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
        s_retry_num = 0;
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	/* 如果wifi没有使能则直接退出 */
	if (!wifi_info.flag.bit.sta_enable && !wifi_info.flag.bit.ap_enable) {
		//ESP_LOGW(TAG, "wifi event, wifi is closed and exit");
		return;
	}
        
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
	{
        s_retry_num = 0;
        iot_wifi_connect_safe();
        ESP_LOGI(TAG, "Start WiFi Station connect ssid: %s, password: %s", wifi_config.sta.ssid, wifi_config.sta.password);
    } 
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
	{
        for ( uint8_t i = 0 ; i < WIFI_STA_INDEX_MAX ; i++ )
        {
            // 找到对应的存储STA
            if (( wifi_info.sta_index == wifi_mmult_sta_addr[i].sta_index ) 
                && (wifi_mul_sta_en[i] != 1)){
                // 该路WIFI已关闭
                wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
                password_err_cnt = 0;
                s_retry_num = 0;
                return;
            }
        }
        
        /* 记录并更精细地判断断连原因（委托到独立函数处理） */
        iot_wifi_handle_sta_disconnected(event_data);
        reals.last_disconnect_router_time = reals.now;
    } 
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
	{
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey(NETIF_KEY_WIFI_STA);
        esp_netif_dns_info_t dns1_info;
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns1_info);
        esp_netif_dns_info_t dns2_info;
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns2_info);
        
#ifdef WIFI_CONNECT_LINK_INFO_DEBUG_LOG
        wifi_ap_record_t ap_info = {0};
        esp_err_t ap_ret = esp_wifi_sta_get_ap_info(&ap_info);

        ESP_LOGI(TAG, "**********WIFI_CONNECT_INFO************");
        ESP_LOGI(TAG, "Got IP   : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask  : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway  : " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "DNS1     : " IPSTR, IP2STR(&dns1_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "DNS2     : " IPSTR, IP2STR(&dns2_info.ip.u_addr.ip4));
        if (ap_ret == ESP_OK) {
            ESP_LOGI(TAG, "SSID     : %s", (char *)ap_info.ssid);
            ESP_LOGI(TAG, "BSSID    : %02X:%02X:%02X:%02X:%02X:%02X",
                     ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2],
                     ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);
            ESP_LOGI(TAG, "RSSI     : %d", ap_info.rssi);
            ESP_LOGI(TAG, "AUTH     : %s(%d)",
                     wifi_auth_mode_to_str(ap_info.authmode), ap_info.authmode);
        } else {
            ESP_LOGW(TAG, "Get AP info failed: %s", esp_err_to_name(ap_ret));
        }
        ESP_LOGI(TAG, "***************************************");
#endif

        s_retry_num = 0;
		password_err_cnt = 0;
        
        wifi_info.sta_ipv4[0] = esp_ip4_addr1(&event->ip_info.ip);
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

        wifi_eventgroup |= WIFI_STA_CONNECTED_AP;
        
        reals.wifi_connect_timestemp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        reals.last_login_router_time = reals.now;

        log_switch_mode(LOG_MODE_UDP);  // 连接成功后切换日志模式
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        ESP_LOGI(TAG, "WiFi AP start");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP)
    {
        ESP_LOGI(TAG, "WiFi AP stop");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_sta_list_t *wifi_sta_list = iot_calloc(sizeof(wifi_sta_list_t));
        if ( NULL != wifi_sta_list ) {
            esp_wifi_ap_get_sta_list(wifi_sta_list);
            if(0 == wifi_sta_list->num) wifi_info.flag.bit.ap_is_link = 0;
            free(wifi_sta_list);
        }
        
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
    {
        wifi_info.flag.bit.ap_is_link = 1;
        ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, IP=" IPSTR, MAC2STR(event->mac), IP2STR(&event->ip));
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
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
            wifi_eventgroup |= WIFI_STA_SCAN_DONE;
        } else {
            // curr_ap_list不为空，存在正在处理的扫描队列，不执行操作
        }
    }
}

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
 Function: wifi_scan_done_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI_SCAN处理函数
  * @param[in]  sta_scan_result_t *results[]  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t wifi_scan_done_process(sta_scan_result_t *results)
{
    uint8_t index = 0;
    
    /* 遍历扫描结果（数组索引对应通道/候选） */
    for (index = 0; index < WIFI_STA_INDEX_MAX; index++)
    {
        /* 仅处理存在的 AP 且该通道对应的 STA 被配置为启用且未被手动停止 */
        if ((true == results[index].found)
            && (1 == wifi_mul_sta_en[results[index].chl])
            && (false == wifi_sta_stop_flag[results[index].chl]))
        {
            /* 若当前设备未连接任意 STA，则优先连接扫描到的信号且被允许的第一个候选（以遍历顺序为准） */
            if (!wifi_info.flag.bit.sta_link_ap)
            {
                // 未连接时，优先连接能找到信号最好且密码未报错的WIFI
                ESP_LOGI(TAG, "Not connected. Connecting to STA index %d (RSSI: %d)",
                         wifi_mmult_sta_addr[results[index].chl].sta_index, results[index].rssi);
                iot_wifi_new_connect(wifi_mmult_sta_addr[results[index].chl].sta_index);

                /* 清除因网络不可达而触发的标志，避免连续误判 */
                wifi_sta_internet_error_flag = false;
                wifi_sta_reconnect_num = 0;
                
                return wifi_mmult_sta_addr[results[index].chl].sta_index;
            }
            else
            {
                /*
                 * 已连接时的切换判定：
                 * - 候选信号要比当前信号强至少 10 dBm，或当前存在 internet 故障标记；
                 * - 候选信号必须高于用户设定的阈值；
                 * - 候选不能是当前已连接的 STA 本身。
                 * 满足以上条件则触发切换。
                 */
                if (((results[index].rssi > (iot_wifi_ap_rssi() + 10)) 
                        || wifi_sta_internet_error_flag )
                    && (results[index].rssi > IotSetData.dev_info_t.wifi_sta_rssi_th)
                    && (wifi_info.sta_index != wifi_mmult_sta_addr[results[index].chl].sta_index))
                {
                    // 自动切换至信号更好的WIFI
                    ESP_LOGI(TAG, "Switching to stronger STA index %d (RSSI: %d), current RSSI: %d, threshold: %d",
                             wifi_mmult_sta_addr[results[index].chl].sta_index, results[index].rssi, iot_wifi_ap_rssi(), IotSetData.dev_info_t.wifi_sta_rssi_th);
                    iot_wifi_new_connect(wifi_mmult_sta_addr[results[index].chl].sta_index);
                    
                    /* 清除因网络不可达而触发的标志，避免连续误判 */
                    wifi_sta_internet_error_flag = false;
                    wifi_sta_reconnect_num = 0;
                    
                    return wifi_mmult_sta_addr[results[index].chl].sta_index;
                }
            }
        }
    }

    // 当前存在 internet 故障且未搜索到任何其他可用WIFI（最多重试5次）
    if ( wifi_info.flag.bit.sta_link_ap 
        && wifi_sta_internet_error_flag 
        && ( ++wifi_sta_reconnect_num < 5 )) {
        // 重连当前WIFI
        ESP_LOGW(TAG, "Only Found Current STA index %d, reconnect...", wifi_info.sta_index);
        iot_wifi_new_connect(wifi_info.sta_index);
        
        /* 清除因网络不可达而触发的标志，避免连续误判 */
        wifi_sta_internet_error_flag = false;

        return wifi_info.sta_index;
    }
    
    /* 未触发任何连接或切换时返回 0 表示无操作 */
    return 0;
}


/*------------------------------------------------------------------------------
 Function: wifi_sta_dhcp_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      STA静态IP相关逻辑
  * @param[in]  uint8_t sta_index  
  * @param[out] None
  * @return     static esp_err_t
  */
static esp_err_t wifi_sta_dhcp_process(uint8_t sta_index)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(NETIF_KEY_WIFI_STA);
    if (netif == NULL) {
        ESP_LOGE(TAG, "wifi_sta_dhcp_process : Failed to get netif handle for STA");
        return ESP_OK;
    }

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

/*------------------------------------------------------------------------------
 Function: Net_Exchange_WIFI
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI驱动配置
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Net_Exchange_WIFI(void)
{
    static uint8_t wifi_Enable_old = 0;
    static uint8_t sSTA_Enable_old = 0;
    static uint8_t sAP_Enable_old = 0;
    
    static esp_netif_t *sta = NULL;
    static esp_netif_t *ap = NULL;
    static esp_event_handler_instance_t instance_any_id;
    static esp_event_handler_instance_t instance_got_ip_sta, instance_ap_staipassigned;
    static bool wifi_inited = false;
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    uint8_t wifi_enable = (IotSetData.dev_info_t.on_off.bit.wifi_enable == 1) ? 1 : 0;
    uint8_t sta_enable = (IotSetData.dev_info_t.on_off.bit.wifi_sta_enable == 1) ? 1 : 0;
#ifdef CONFIG_WIFI_AP_MODE_ENABLE 
    uint8_t ap_enable  = (IotSetData.dev_info_t.on_off.bit.wifi_ap_enable == 1) ? 1 : 0;
#else
    uint8_t ap_enable  = sAP_Enable_old;
#endif
    if (!wifi_semaphore || xSemaphoreTake(wifi_semaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG,"Net_Exchange_WIFI : wifi_semaphore error");
        return; 
    }

    // esp_wifi_init 只能在整个生命周期内调用一次，重复调用会报错
    if ( !wifi_inited ) {
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    
        // STA
        if (!sta) sta = esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip_sta));

        // AP
        if (!ap) ap = esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_event_handler, NULL, &instance_ap_staipassigned));
        
        wifi_inited = true;
    }

    // 状态相同时不执行
    if ((sSTA_Enable_old == sta_enable)
        &&(sAP_Enable_old == ap_enable)
        &&(wifi_Enable_old == wifi_enable)) goto exit;
    
    // 判断WiFi模式
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (!wifi_enable) mode = WIFI_MODE_NULL;
    else if (sta_enable && ap_enable) mode = WIFI_MODE_APSTA;
    else if (sta_enable) mode = WIFI_MODE_STA;
    else if (ap_enable) mode = WIFI_MODE_AP;
    else mode = WIFI_MODE_NULL;

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

    if (mode != WIFI_MODE_NULL) {
        if ( 0 == event_wifi_register_flag ) {
            ESP_ERROR_CHECK(esp_wifi_start());
        }
        event_wifi_register_flag = 1;
        ESP_LOGW(TAG, "WiFi Mode Change: %s (%d)", WIFI_MODE_DESC(mode), mode);
    } else {
        esp_wifi_stop();
        event_wifi_register_flag = 0;
        ESP_LOGW(TAG, "WiFi STA and AP Close!");
    }

    sSTA_Enable_old = sta_enable;
    sAP_Enable_old = ap_enable;
    wifi_Enable_old = wifi_enable;
    
exit:

    /*释放信号量*/
    xSemaphoreGive(wifi_semaphore);  
}

void wifi_ap_netif_config(void)
{
    if (!wifi_semaphore || xSemaphoreTake(wifi_semaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG,"wifi_ap_netif_config : wifi_semaphore error");
        return;
    }

    // 通过Key获取AP网卡实例
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey(NETIF_KEY_WIFI_AP);
    if (NULL == ap_netif) {
        ESP_LOGE(TAG, "Get WiFi ap netif handle fail");
        goto exit;
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

exit:
    
    /*释放信号量*/
    xSemaphoreGive(wifi_semaphore);      
}
/*------------------------------------------------------------------------------
 Function: wifi_task_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI任务初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void wifi_task_init(void)
{
    /*初始化网络状态变量*/
	memset(&wifi_info, 0, sizeof(wifi_info_t));

#if 0	// 改到外部执行
    /* Init and register system/core components */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#endif

	wifi_semaphore = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(wifi_semaphore == NULL) {
        ESP_LOGE(TAG, "wifi_semaphore  create failed");
		return;
	}
	xSemaphoreGive(wifi_semaphore);						/**< the resource is available after creation */

    /*WiFi AP配置*/
    iot_wifi_ap_config(IotSetData.dev_info_t.wifi_ap_auth, IotSetData.dev_info_t.wifi_AP_ssid, IotSetData.dev_info_t.wifi_AP_password);

    /*WIFI连接超时检查*/
    ensure_wifi_wdt();

    /*WIFI开关检查*/
    if (1 == IotSetData.dev_info_t.on_off.bit.wifi_enable) { 	
        /*WIFI STA开关检查*/
        if (1 == IotSetData.dev_info_t.on_off.bit.wifi_sta_enable) {    
            iot_wifi_open(WIFI_Set_Mode_STA);
        }
        
        /*WIFI AP开关检查*/
        if (1 == IotSetData.dev_info_t.on_off.bit.wifi_ap_enable) {
            iot_wifi_open(WIFI_Set_Mode_AP);
        }
    }    
}

/**
 * @brief 检测当前连接信号是否持续偏弱并在阈值触发时执行自动切换
 * @param now_time 当前时间，单位：毫秒（例如系统 tick 转换）
 * @details
 * - 未连接时重置计时与计数；
 * - 每秒检测一次 RSSI，仅在 wifi_sta_num > 1 时允许触发切换；
 * - 当连续弱信号次数 > IotSetData.dev_info_t.wifi_sta_switch_int 时调用 iot_wifi_auto_connect()；
 * - 信号恢复时清零计数以避免抖动触发。
 */
static void iot_wifi_check_weak_signal(uint32_t now_time)
{
    static uint32_t last_link_time = 0;
    static uint16_t weak_signal_cnt = 0;

    /* 未连接则重置计时器 */
    if (!wifi_info.flag.bit.sta_link_ap) {
        last_link_time = now_time;
        weak_signal_cnt = 0;
        return;
    }

    /* 每秒检查一次 */
    if ((now_time - last_link_time) <= 1000) {
        return;
    }
    last_link_time = now_time;

    /* 多 STA 环境下才允许切换 */
    if (wifi_sta_num <= 1) {
        weak_signal_cnt = 0;
        return;
    }

    int rssi = iot_wifi_ap_rssi();
    int th = IotSetData.dev_info_t.wifi_sta_rssi_th;
    if (rssi < th) {
        weak_signal_cnt++;
        if (weak_signal_cnt > IotSetData.dev_info_t.wifi_sta_switch_int) {
            ESP_LOGI(TAG, "Weak signal(< %d dBm) count exceeded interval(> %d s). Triggering auto connect.",
                     th, IotSetData.dev_info_t.wifi_sta_switch_int);
            weak_signal_cnt = 0;
            iot_wifi_auto_connect();
        }
    } else {
        /* 信号恢复则清零计数，避免抖动触发 */
        weak_signal_cnt = 0;
    }
}

/**
 * @brief 周期性检测当前 STA 是否能访问互联网并在连续失败达到阈值时触发自动切换
 *
 * 逻辑要点：
 * - 在未连接或已完成 MQTT 登录时重置计时与失败计数，并清除 internet error 标志；
 * - 首次延迟 90s 后开始检测；若 wifi_sta_internet_error_flag 为 true 则跳过检测（上层可用于抑制重复检测）；
 * - 检测间隔至少 10s，检测只在多 STA（wifi_sta_num > 1）环境下触发自动切换逻辑；
 * - 调用 wifi_check_internet(2000)（2s 超时）判断网络可达性：
 *     - INTERNET_OK：重置失败计数；
 *     - 非 OK：累加失败计数，达到阈值后调用 iot_wifi_auto_connect() 并清零计数；
 * - 阈值优先使用 CONFIG_WIFI_INTERNET_FAIL_THRESHOLD，缺省为 3 次。
 *
 * @param now_time 当前时间，单位毫秒（调用者需提供，例如系统 tick 转换）
 */
static void iot_wifi_check_internet(uint32_t now_time)
{
    static uint32_t start_check_time = 0;    /* 第一次开始计时点（用于 90s 延迟） */
    static uint32_t last_check_time = 0;     /* 上一次实际检测时间（用于 10s 间隔） */
    static uint16_t err_cnt = 0;             /* 累计失败次数 */

    /* 若未连接或已登录 MQTT，重置相关计数并清除抑制标志 */
    if (!wifi_info.flag.bit.sta_link_ap || iot_mqtt_login_state()) {
        last_check_time = now_time;
        start_check_time = now_time;
        err_cnt = 0;
        wifi_sta_internet_error_flag = false;
        return;
    }

    /* 首次等待 90s 再开始周期检测；若有上层设置的抑制标志则跳过 */
    if (((now_time - start_check_time) <= 60 * 1000)
        || wifi_sta_internet_error_flag ) {
        return;
    }

    /* 每次检测至少间隔 10s，避免频繁检测 */
    if ((now_time - last_check_time) <= 10 * 1000) {
        return;
    }
    last_check_time = now_time;

    /* 仅在多 STA 环境允许切换时启用自动切换逻辑 */
    if (wifi_sta_num <= 1) {
        err_cnt = 0;
        return;
    }

    /* 进行网络可达检测（2s 超时） */
    internet_status_t status = wifi_check_internet(2000);
    if (status == INTERNET_OK) {
        /* 网络正常，清零失败计数并更新时间戳，避免立即再次检测 */
        err_cnt = 0;
        last_check_time = now_time;
        start_check_time = now_time;
        ESP_LOGD(TAG, "Internet reachable");
        return;
    }

    /* 记录失败次数并判断是否超过阈值触发切换 */
    err_cnt++;

    /* 使用配置项作为阈值（默认 3 次） */
#ifdef CONFIG_WIFI_INTERNET_FAIL_THRESHOLD
    uint16_t threshold = CONFIG_WIFI_INTERNET_FAIL_THRESHOLD;
#else
    uint16_t threshold = 3;
#endif

    ESP_LOGW(TAG, "Internet check failed: %d, (%d/%d)", status, err_cnt, threshold);
    if (err_cnt >= threshold) {
        ESP_LOGI(TAG, "Internet unreachable for %u checks (threshold %u). Triggering auto-connect.", err_cnt, threshold);
        err_cnt = 0;
        wifi_sta_internet_error_flag = true;
        iot_wifi_auto_connect();
    }
}

/**
 * @brief 处理网络检测结果的计数与动作（不做检测，仅根据结果计数/重置并在阈值触发重连）
 *
 * @param status 网络检测结果，INTERNET_OK 表示可达，其他值视为失败
 *
 * 说明：
 * - 成功 -> 清零内部失败计数并清除 wifi_sta_internet_error_flag；
 * - 失败 -> 累加失败计数，达到阈值时置 wifi_sta_internet_error_flag = true，
 *   调用 iot_wifi_auto_connect() 发起重扫描/重连，并清零计数；
 * - 阈值优先使用 CONFIG_WIFI_INTERNET_FAIL_THRESHOLD，缺省为 3 次；
 * - 函数为轻量处理，适合由外部检测逻辑调用。
 */
void iot_wifi_process_internet_check_result(internet_status_t status)
{
    static uint16_t err_cnt = 0;

    /*已达到阈值*/
    if(wifi_sta_internet_error_flag) return;

    if (status == INTERNET_OK) {
        /* 网络可达：重置计数与抑制标志 */
        err_cnt = 0;
        wifi_sta_internet_error_flag = false;
        wifi_sta_reconnect_num = 0;
        ESP_LOGD(TAG, "Internet check OK, clear fail count");
        return;
    }

    /* 累计失败次数 */
    err_cnt++;

#ifdef CONFIG_WIFI_INTERNET_FAIL_THRESHOLD
    uint16_t threshold = CONFIG_WIFI_INTERNET_FAIL_THRESHOLD;
#else
    uint16_t threshold = 3;
#endif

    ESP_LOGW(TAG, "Internet check failed: %d, count %u/%u", status, err_cnt, threshold);

    if (err_cnt >= threshold) {
        ESP_LOGI(TAG, "Internet unreachable for %u checks (threshold %u). Triggering auto-connect.", err_cnt, threshold);
        err_cnt = 0;
        wifi_sta_internet_error_flag = true;
        iot_wifi_auto_connect();
    }
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_udp_status_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      根据WIFI状态判断UDP工作状态
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_wifi_udp_status_check(void)
{
#ifdef CONFIG_WLCC_ENABLE
    static wifi_flags_t old_flag = {0};

    // 判断STA连接状态
    if ( wifi_info.flag.bit.sta_link_ap != old_flag.bit.sta_link_ap ) {
        if ( wifi_info.flag.bit.sta_link_ap ) {
            // 开启WLCC通信
            wlcc_start(NETIF_WIFI_STA);
            ESP_LOGI(TAG,"WLCC Start.");
        } else {
            // 停止WLCC通信
            wlcc_stop();
            ESP_LOGI(TAG,"WLCC Stop.");
        }

        old_flag.bit.sta_link_ap = wifi_info.flag.bit.sta_link_ap;
    }
#endif
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_State_Machine
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI 连接状态机
  * @param[in]  void 
  * @param[out] None
  * @return     void
  */
void iot_wifi_State_Machine(void) 
{
	static uint32_t pre_time = 0;    
    static uint32_t pre_scan_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /*周期200ms执行*/
    if ((now_time - pre_time) < 200) return;
    pre_time = now_time;

    /*WIFI配置管理*/
    Net_Exchange_WIFI();

    /*检测当前连接信号是否持续偏弱，达到阈值后触发自动切换*/
    iot_wifi_check_weak_signal(now_time);

#if 0   // 暂不使用，直接借用MQTT登录过程判断
    /*周期性检测当前 STA 是否能访问互联网并在连续失败达到阈值时触发自动切换*/
    iot_wifi_check_internet(now_time);
#endif

#ifdef CONFIG_WLCC_ENABLE
    /*根据WIFI状态判断UDP工作状态*/
    iot_wifi_udp_status_check();
#endif

    // 无事件直接退出
    if ( !wifi_eventgroup ) return;

    /*WIFI连接逻辑*/
	if(event_wifi_register_flag)
	{
	    /*连接事件是否响应*/
	    if ((wifi_eventgroup & WIFI_STA_DISCONNECT_AP) 
            || (wifi_eventgroup & WIFI_STA_CONNECT_FAILED)
            || (wifi_eventgroup & WIFI_STA_CONNECTED_AP)) {
            if (wifi_connect_wdt) {
                esp_timer_stop(wifi_connect_wdt);
                wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_TIMEOUT);
            }	
        }
            
		if (wifi_eventgroup & WIFI_STA_SCAN_DONE)	/* wifi 扫描网络 */
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

            // 释放扫描空间
            curr_ap_list_num = 0;
            if ( curr_ap_list != NULL ) {
                free(curr_ap_list);
                curr_ap_list = NULL;
            }

            // 未成功扫描连接
            if ( !update_connect )
            {
                // 未连接时，若存在有效配置WIFI，则持续扫描
                if ( !wifi_info.flag.bit.sta_link_ap ) {
                    ESP_LOGW(TAG,"WIFI Scan error!");
                    if ((( 1 == wifi_mul_sta_en[0] ) && ( false == wifi_sta_stop_flag[0] )) 
                        || (( 1 == wifi_mul_sta_en[1] ) && ( false == wifi_sta_stop_flag[1] ))
                        || (( 1 == wifi_mul_sta_en[2] ) && ( false == wifi_sta_stop_flag[2] ))){                        
                        wifi_eventgroup |= WIFI_STA_PERIOD_SCAN;
                    } else {
                        ESP_LOGE(TAG,"All of the WIFI-STA is invalid!");
                        wifi_info.sta_index = WIFI_STA_INDEX_STOP;
                        wifi_eventgroup |= WIFI_STA_MULT_ALL_FAILED;
                    }
                }
            }
                        
			wifi_eventgroup &= ~((uint16_t)WIFI_STA_SCAN_DONE);
		}
		else if (wifi_eventgroup & WIFI_STA_DISCONNECT_AP)  /* WiFi断开网络 */
		{
            ESP_LOGW(TAG,"wifi disconnect ap.");
			wifi_info.flag.bit.sta_link_ap = 0;

#ifdef CONFIG_IOT_AUTO_LIGHT_SLEEP_ENABLE            
            if ( 1 == reals.IOT_Status_Flag.sBit.system_sleep_flag ) {
                reals.IOT_Status_Flag.sBit.wifi_reconnect_request_in_sleep = 1;
            } else {
                iot_wifi_connect_safe(); // 重新联网
            }
#else
            iot_wifi_connect_safe(); // 重新联网
#endif
			wifi_eventgroup &= ~((uint16_t)WIFI_STA_DISCONNECT_AP);
		}
		else if (wifi_eventgroup & WIFI_STA_CONNECT_FAILED) /* 连接WiFi热点失败 */
		{
            ESP_LOGE(TAG,"wifi connect fail.");
			wifi_info.flag.bit.sta_link_ap = 0;

            // wifi密码错误达到一定上限时
            if (password_err_cnt >= MAX_PASSWORD_ERR_CNT) {         
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
            
            // 重新扫描是否有可用WIFI
            iot_wifi_auto_connect();
            
			wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_FAILED);
		}
        else if (wifi_eventgroup & WIFI_STA_CONNECT_TIMEOUT)  /* WiFi 连接超时 */
        {
            wifi_info.flag.bit.sta_link_ap = 0;
            
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                // 驱动认为仍连接，先断开并在 DISCONNECTED 回调再连接
                esp_wifi_disconnect();
            }
            
            iot_wifi_connect_safe(); // 重新联网
            wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECT_TIMEOUT);
        }
		else if (wifi_eventgroup & WIFI_STA_CONNECTED_AP)	/* wifi 连接网络 */
		{
			ESP_LOGI(TAG,"wifi connect to the ap successfull");
			wifi_info.flag.bit.sta_link_ap = 1;
			wifi_eventgroup &= ~((uint16_t)WIFI_STA_CONNECTED_AP);

            // 连接过程中WIFI使能关闭，断开重新连接
            for ( uint8_t i = 0 ; i < WIFI_STA_INDEX_MAX ; i++ ) {
                if(wifi_info.sta_index == wifi_mmult_sta_addr[i].sta_index) {
                    if(1 != wifi_mul_sta_en[i]) {
                        /* 断开WiFi连接 */
                        wifi_info.flag.bit.sta_link_ap = 0;
                        esp_wifi_disconnect();

                        // 扫描是否有可用WIFI
                        iot_wifi_auto_connect();
                    }
                }
            }
		}
		else if (wifi_eventgroup & WIFI_CLOSE_STA)  /* 关闭WiFi STA模式 */
		{
			wifi_info.flag.bit.sta_link_ap = 0;
			wifi_info.flag.bit.sta_enable = 0;
			ESP_LOGW(TAG,"wifi sta closed");
			wifi_eventgroup &= ~((uint16_t)WIFI_CLOSE_STA);
		}
		else if (wifi_eventgroup & WIFI_OPEN_STA) /* 开启WiFi STA模式 */
		{            
            if ( 1 != wifi_info.flag.bit.sta_enable )
            {
                wifi_mode_t wifi_mode;
                esp_wifi_get_mode(&wifi_mode);
                if (WIFI_MODE_STA == wifi_mode || WIFI_MODE_APSTA == wifi_mode)
                {
                    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
                    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, wifi_info.sta_mac));
                    
                    // 更新STA基本参数
                    iot_wifi_base_cfg_update();
                    
                    wifi_info.flag.bit.sta_enable = 1;
                    wifi_info.flag.bit.sta_link_ap = 0;
                    esp_wifi_disconnect(); /* 断开WiFi连接 */
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                    
                    // 扫描是否有可用WIFI
                    iot_wifi_auto_connect();
                    
                    wifi_eventgroup &= ~((uint16_t)WIFI_OPEN_STA);
                }
            }
            else
            {
                wifi_eventgroup &= ~((uint16_t)WIFI_OPEN_STA);
            }
		}
		else if (wifi_eventgroup & WIFI_STA_NEW_AP) /* WiFi重新配网 */
		{
			password_err_cnt = 0;
			wifi_info.flag.bit.sta_link_ap = 0;
			esp_wifi_disconnect(); /* 断开WiFi连接 */

            // STA静态IP相关逻辑
            esp_err_t err = wifi_sta_dhcp_process(wifi_info.sta_index);
            if (err == ESP_OK) {
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                iot_wifi_connect_safe();  /* 开启WiFi连接 */
            } else {           
                wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
                password_err_cnt = 0;
                s_retry_num = 0;
            }

			wifi_eventgroup &= ~((uint16_t)WIFI_STA_NEW_AP);
		}       
		else if (wifi_eventgroup & WIFI_STA_PERIOD_SCAN)
        {
            /*周期1min执行*/
            if ((now_time - pre_scan_time) >= 60 * 1000) {
                pre_scan_time = now_time;

                // 扫描是否有可用WIFI
                iot_wifi_auto_connect();

                wifi_eventgroup &= ~((uint16_t)WIFI_STA_PERIOD_SCAN);
            }
        }
        else if ( wifi_eventgroup & WIFI_STA_MULT_ALL_FAILED )
        {
            // WIFI参数更新
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
                    
                wifi_eventgroup &= ~((uint16_t)WIFI_STA_MULT_ALL_FAILED);
            }
        }
#ifdef CONFIG_WIFI_AP_MODE_ENABLE  
		else if (wifi_eventgroup & WIFI_CLOSE_AP)  /* 关闭WiFi AP模式 */
		{
			wifi_info.flag.bit.ap_is_link = 0;
			wifi_info.flag.bit.ap_enable = 0;
			ESP_LOGW(TAG,"wifi ap closed");
			wifi_eventgroup &= ~((uint16_t)WIFI_CLOSE_AP);
		}
        else if (wifi_eventgroup & WIFI_OPEN_AP)
        {
            if ( 1 != wifi_info.flag.bit.ap_enable )
            {
                wifi_mode_t wifi_mode;
                esp_wifi_get_mode(&wifi_mode);
                if (WIFI_MODE_AP == wifi_mode || WIFI_MODE_APSTA == wifi_mode)
                {
                    ESP_LOGW(TAG,"wifi open ap");
                    wifi_info.flag.bit.ap_enable = 1;
                    wifi_info.flag.bit.ap_is_link = 0;
                
                    wifi_eventgroup &= ~((uint16_t)WIFI_OPEN_AP);
                    wifi_eventgroup |= WIFI_AP_NEW_AP;
                }
            }
            else
            {
                wifi_eventgroup &= ~((uint16_t)WIFI_OPEN_AP);
            }
        }
        else if (wifi_eventgroup & WIFI_AP_NEW_AP)
        {
            esp_wifi_deauth_sta(0); //踢掉所有STA
            esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap);//自己作为AP的 底层配置
            ESP_LOGI(TAG,"WIFI_OPEN_AP : ssid(%s), password(%s)",
                    wifi_config_ap.ap.ssid,wifi_config_ap.sta.password);
            
            // 修改AP模式下检测到Sta离线的超时时间为30s, ESP32默认为5min
            esp_wifi_set_inactive_time(WIFI_IF_AP, 30);
            esp_wifi_set_inactive_time(WIFI_IF_STA, 5);
            wifi_ap_netif_config();
            wifi_eventgroup &= ~((uint16_t)WIFI_AP_NEW_AP);
        }
#endif
		else
		{
		    /*正常不触发*/
            ESP_LOGE(TAG, "wifi_eventgroup unknown(%d)", wifi_eventgroup);
			wifi_eventgroup = 0;
		}
	}
    else
    {
        /*WIFI驱动已关闭*/
        if ((wifi_eventgroup & WIFI_CLOSE_STA) 
            || (wifi_eventgroup & WIFI_CLOSE_AP))/* 关闭WiFi*/
        {
            wifi_info.flag.bit.sta_link_ap = 0;
            wifi_info.flag.bit.sta_enable = 0;
            wifi_eventgroup &= ~((uint16_t)WIFI_CLOSE_STA);

			wifi_info.flag.bit.ap_is_link = 0;
			wifi_info.flag.bit.ap_enable = 0;
			wifi_eventgroup &= ~((uint16_t)WIFI_CLOSE_AP);
			ESP_LOGW(TAG,"Wifi closed");
        }
    }
}

/*
给输入参数赋值
*/
static void iot_wifi_get_cfg_ap(uint8_t *auth, char *ssid, char *pwd) {
    *auth = wifi_config.sta.threshold.authmode;
    memcpy(ssid, wifi_config.sta.ssid,     sizeof(wifi_config.sta.ssid));
    memcpy(pwd,  wifi_config.sta.password, sizeof(wifi_config.sta.password));
}

const wifi_info_t * iot_get_wifi_info(void) {
    iot_wifi_get_cfg_ap(&wifi_info.sta_auth, wifi_info.sta_ssid, wifi_info.sta_pwd); /* 更新密码 */
    return (const wifi_info_t *)&wifi_info;
}

/* 关闭WiFi(STA AP) */
void iot_wifi_close(uint8_t mode) {

    switch (mode)
    {
        case WIFI_Set_Mode_STA:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_STA);
            wifi_eventgroup |= WIFI_CLOSE_STA;
			wifi_info.flag.bit.sta_enable = 0;
        
            break;

        case WIFI_Set_Mode_AP:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_AP);
            wifi_eventgroup |= WIFI_CLOSE_AP;
            wifi_info.flag.bit.ap_enable = 0;
        
            break;

        case WIFI_Set_Mode_ALL:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_STA);
            wifi_eventgroup |= WIFI_CLOSE_STA;
			wifi_info.flag.bit.sta_enable = 0;
        
//            xEventGroupSetBits(wifi_eventgroup, WIFI_CLOSE_AP);
            wifi_eventgroup |= WIFI_CLOSE_AP;
            wifi_info.flag.bit.ap_enable = 0;
            
            break;
    }
}

/* 开启WiFi(STA AP) */
void iot_wifi_open(uint8_t mode) 
{
//    if (wifi_eventgroup == NULL) {
//        return ;
//    }

    switch (mode)
    {
        case WIFI_Set_Mode_STA:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_OPEN_STA);
            wifi_eventgroup |= WIFI_OPEN_STA;
			
            break;
        case WIFI_Set_Mode_AP:
//            xEventGroupSetBits(wifi_eventgroup, WIFI_OPEN_AP);
            wifi_eventgroup |= WIFI_OPEN_AP;
            break;
        case WIFI_Set_Mode_ALL:
            wifi_eventgroup |= WIFI_OPEN_AP|WIFI_OPEN_STA;
			
            break;
    }
}

void iot_wifi_disconnect(void) {
    /* 断开WiFi连接 */
    wifi_info.flag.bit.sta_link_ap = 0;
    esp_wifi_disconnect(); 
}

void iot_wifi_reset(void) {
    iot_wifi_close(WIFI_Set_Mode_ALL);
    vTaskDelay(pdMS_TO_TICKS(50));
    iot_wifi_open(WIFI_Set_Mode_STA);
    vTaskDelay(pdMS_TO_TICKS(50));
}

/**
 * @brief 配置WiFi-AP信息
 * @details 暂不支持配置IP网关等信息
 */
void iot_wifi_ap_config(uint8_t auth, const char *ssid, const char *pwd)
{
#ifdef CONFIG_WIFI_AP_MODE_ENABLE

    if (!strlen((const char *)ssid)) {
        return ;
    }
    
    ESP_LOGW(TAG, "iot wifi ap config, enable:%d ssid:%s, pwd:%s, auth:%d",
                    IotSetData.dev_info_t.on_off.bit.wifi_ap_enable, ssid, pwd, auth);

    memcpy(wifi_config_ap.ap.ssid, ssid, 32);
    wifi_config_ap.ap.ssid_len = strlen(ssid);
    wifi_config_ap.ap.ssid_hidden = 0;
    wifi_config_ap.ap.channel = 0;
    wifi_config_ap.ap.max_connection = 4;
    wifi_config_ap.ap.pmf_cfg.required = true;

    if (!strlen((const char *)pwd) || strlen((const char *)pwd) < 8)
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
    if (1 == IotSetData.dev_info_t.on_off.bit.wifi_ap_enable
        && 1 != wifi_info.flag.bit.ap_enable)
    {
        iot_wifi_open(WIFI_Set_Mode_AP);
    } else if (wifi_info.flag.bit.ap_enable == 1) {
        wifi_eventgroup |= WIFI_AP_NEW_AP;
    }
    
#endif    
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_ap_deinit
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI AP模式关闭
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_wifi_ap_deinit(void)
{
    /*关闭AP模式*/
    iot_wifi_close(WIFI_Set_Mode_AP);

    /*设置WIFI Mode*/
    if(IotSetData.dev_info_t.on_off.bit.wifi_sta_enable == 1)  {
        /*ESP-IDF 的 esp_wifi_set_mode() 支持在 STA 已连接的情况下，
        将模式从 WIFI_MODE_APSTA 切换为 WIFI_MODE_STA，此时STA 连接会保持，只会关闭 AP 功能*/
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    }

    ESP_LOGW(TAG, "WIFI AP Mode Close!");
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_ap_reinit
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI AP模式恢复
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_wifi_ap_reinit(void)
{
    /*设置WIFI Mode*/
    if(IotSetData.dev_info_t.on_off.bit.wifi_sta_enable == 1)  {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    }

    /*恢复AP模式*/
    iot_wifi_open(WIFI_Set_Mode_AP);

    ESP_LOGW(TAG, "WIFI AP Mode Open!");
}

/*
输入参数赋值给 wifi_config
*/
#if 0
    typedef enum {
        WIFI_AUTH_OPEN = 0,         /**< authenticate mode : open */
        WIFI_AUTH_WEP,              /**< authenticate mode : WEP */
        WIFI_AUTH_WPA_PSK,          /**< authenticate mode : WPA_PSK */
        WIFI_AUTH_WPA2_PSK,         /**< authenticate mode : WPA2_PSK */
        WIFI_AUTH_WPA_WPA2_PSK,     /**< authenticate mode : WPA_WPA2_PSK */
        WIFI_AUTH_WPA2_ENTERPRISE,  /**< authenticate mode : WPA2_ENTERPRISE */
        WIFI_AUTH_WPA3_PSK,         /**< authenticate mode : WPA3_PSK */
        WIFI_AUTH_WPA2_WPA3_PSK,    /**< authenticate mode : WPA2_WPA3_PSK */
        WIFI_AUTH_WAPI_PSK,         /**< authenticate mode : WAPI_PSK */
        WIFI_AUTH_MAX
    } wifi_auth_mode_t;
#endif
void iot_wifi_new_ap(uint8_t auth, const char *ssid, const char *pwd) {

    if (!strlen((const char *)ssid)) {
        return ;
    }

    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    
    if (!pwd || !strlen((const char *)pwd)) 
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN; // 密码认证方式
        memset(wifi_config.sta.password, '\0', sizeof(wifi_config.sta.password));
    } 
    else 
    {
        wifi_config.sta.threshold.authmode = auth; // 密码认证方式
        if (auth == WIFI_AUTH_OPEN) 
        {
            memset(wifi_config.sta.password, '\0', sizeof(wifi_config.sta.password));
        } 
        else
        {
            memcpy(wifi_config.sta.password, pwd, sizeof(wifi_config.sta.password)); 
        }
    }

    ESP_LOGW(TAG, "connect to a new AP, ssid: %s, pwd: %s, auth mode: %d",
            wifi_config.sta.ssid, wifi_config.sta.password, wifi_config.sta.threshold.authmode);
    
    iot_wifi_get_cfg_ap(&wifi_info.sta_auth, wifi_info.sta_ssid, wifi_info.sta_pwd); /* 更新密码 */

    if (wifi_info.flag.bit.sta_enable == 1) 
    { 
        /* 开启smartlink */
        wifi_eventgroup |= WIFI_STA_NEW_AP;
    }
}

int8_t iot_wifi_ap_rssi(void) {
    if (wifi_info.flag.bit.sta_link_ap == 1) {
        wifi_ap_record_t ap_info;
        esp_wifi_sta_get_ap_info(&ap_info);
        return ap_info.rssi;
    }
    return 0;
}

#define WIFI_RECONNECT_PERIOD_IN_SLEEP  30000   // 30s

/*------------------------------------------------------------------------------
 Function: iot_wifi_reconnect_in_sleep
 -----------------------------------------------------------------------------*/
/**
  * @brief      休眠状态重连处理
  * @param[in]  bool force  
  * @param[out] None
  * @return     void
  */
void iot_wifi_reconnect_in_sleep(bool force)
{
    static uint8_t last_reconnect_flag = 0;
	static uint32_t pre_time = 0;
    
    if ( 1 == reals.IOT_Status_Flag.sBit.wifi_reconnect_request_in_sleep ) {
        if ( force == true ) {
            iot_wifi_connect_safe(); // 要求立即重新联网
            reals.IOT_Status_Flag.sBit.wifi_reconnect_request_in_sleep = 0;
        } else {
            uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ( 0 == last_reconnect_flag ) {
                pre_time = now_time;    // 第一次请求，开始计时
                ESP_LOGW(TAG, "System is sleeping! Delay %d ms to reconnect......", WIFI_RECONNECT_PERIOD_IN_SLEEP);
            } else if ( (now_time - pre_time) > WIFI_RECONNECT_PERIOD_IN_SLEEP ) {
                iot_wifi_connect_safe(); // 超时重连
                reals.IOT_Status_Flag.sBit.wifi_reconnect_request_in_sleep = 0;
            }
        }
    }

    last_reconnect_flag = reals.IOT_Status_Flag.sBit.wifi_reconnect_request_in_sleep;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_new_connect
 -----------------------------------------------------------------------------*/
/**
  * @brief      发起新的STA连接
  * @param[in]  uint8_t STA_index  
  * @param[out] None
  * @return     void
  */
void iot_wifi_new_connect(uint8_t STA_index)
{
    if((!event_wifi_register_flag) || (1 != wifi_info.flag.bit.sta_enable)) {
        ESP_LOGE(TAG, "WIFI State Error : event_wifi_register_flag(%d), sta_enable(%d)", 
                 event_wifi_register_flag, wifi_info.flag.bit.sta_enable);
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
            iot_wifi_new_ap(*wifi_mmult_sta_addr[i].sta_auth, 
                            wifi_mmult_sta_addr[i].ssid, 
                            wifi_mmult_sta_addr[i].password);
            
            ESP_LOGW(TAG, "STA(%d) updated and enabled, connecting to STA(%d)", STA_index, wifi_info.sta_index);
            break;
        }
    }
    if (!found) {
        ESP_LOGE(TAG, "STA config for index %d not found, cannot connect.", STA_index);
    }
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_sta_index_get
 -----------------------------------------------------------------------------*/
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
 Function: iot_wifi_sta_link_status_get
 -----------------------------------------------------------------------------*/
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
 Function: iot_wifi_auto_connect
 -----------------------------------------------------------------------------*/
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
        
    esp_err_t err = esp_wifi_scan_start(&scan_config, false); // non-blocking
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi scan started...");
    } else {
        ESP_LOGE(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
        wifi_eventgroup |= WIFI_STA_CONNECT_FAILED;
    }
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_base_cfg_update
 -----------------------------------------------------------------------------*/
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
    if (1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en) sta_cnt++;
    if (1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en) sta_cnt++;
    if (1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en) sta_cnt++;
    wifi_sta_num = sta_cnt;

    // WIFI STA使能标志
    wifi_mul_sta_en[0] = IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en;
    wifi_mul_sta_en[1] = IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en;
    wifi_mul_sta_en[2] = IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en;

    // WIFI STA静态IP使能标志
    wifi_mul_sta_static_ip_en[0] = IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en;
    wifi_mul_sta_static_ip_en[1] = IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en;
    wifi_mul_sta_static_ip_en[2] = IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en;
    
    // 清除WIFI错误标志
    memset(wifi_sta_stop_flag, 0, sizeof(wifi_sta_stop_flag));
}

/*------------------------------------------------------------------------------
 Function: wifi_sta_update_flag_set
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI STA更新标志
  * @param[in]  bool set  
  * @param[out] None
  * @return     void
  */
void wifi_sta_update_flag_set(bool set) {
    wifi_sta_update_flag = set;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_on_off_status
 -----------------------------------------------------------------------------*/
/**
  * @brief      返回WIFI使能标志
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_wifi_on_off_status(void)
{
    uint8_t WifiEnable = 0;
    if((IotSetData.dev_info_t.on_off.bit.wifi_enable == 0x01) 
        && (wifi_sta_num > 0)) {
        WifiEnable = 1;
    } else {
        WifiEnable = 2;
    }

    return WifiEnable;
}

