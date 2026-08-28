#include "cloud_diag.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "iot_period_task.h"
#include "iot_wifi_init.h"
#include <errno.h>
#include <string.h>

static const char *mqtt_error_type_str(int type)
{
    switch (type) {
    case MQTT_ERROR_TYPE_NONE:             return "NONE";
    case MQTT_ERROR_TYPE_TCP_TRANSPORT:    return "TCP_TRANSPORT";
    case MQTT_ERROR_TYPE_CONNECTION_REFUSED: return "CONNECTION_REFUSED";
    default:                               return "UNKNOWN";
    }
}

static const char *mqtt_refuse_code_str(int code)
{
    switch (code) {
    case MQTT_CONNECTION_REFUSE_PROTOCOL:        return "PROTOCOL";
    case MQTT_CONNECTION_REFUSE_ID_REJECTED:     return "ID_REJECTED";
    case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE: return "SERVER_UNAVAILABLE";
    case MQTT_CONNECTION_REFUSE_BAD_USERNAME:    return "BAD_USERNAME";
    case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:  return "NOT_AUTHORIZED";
    default:                                     return "OTHER";
    }
}

const char *cloud_diag_mqtt_step_name(uint8_t step)
{
    switch (step) {
    case 0:  return "RESTART";
    case 1:  return "HOST_DNS";
    case 2:  return "CREATE_TCP";
    case 3:  return "GET_HOST_TIME";
    case 4:  return "MQTT_LOGIN";
    case 5:  return "MSG_HANDLE";
    case 6:  return "EXIT";
    case 7:  return "CERT_CHECK";
    case 8:  return "CERT_EXPIRATION";
    case 9:  return "CERT_DOWNLOAD";
    case 10: return "CONNECT_FAIL";
    default: return "UNKNOWN";
    }
}

const char *cloud_diag_wifi_reason_str(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_UNSPECIFIED:              return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:              return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:               return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE:             return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY:            return "ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED:               return "NOT_AUTHED";
    case WIFI_REASON_ASSOC_LEAVE:              return "ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED:         return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:      return "DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:      return "DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC:   return "BSS_TRANSITION_DISASSOC";
    case WIFI_REASON_IE_INVALID:                return "IE_INVALID";
    case WIFI_REASON_MIC_FAILURE:              return "MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:    return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:  return "GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:        return "IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID:      return "GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:   return "PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID:              return "AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:     return "UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP:        return "INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED:        return "802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:     return "CIPHER_SUITE_REJECTED";
    case WIFI_REASON_BEACON_TIMEOUT:            return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:               return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:                 return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:                return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:         return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:           return "CONNECTION_FAIL";
    case WIFI_REASON_AP_TSF_RESET:              return "AP_TSF_RESET";
    case WIFI_REASON_ROAMING:                   return "ROAMING";
    case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG: return "ASSOC_COMEBACK_TIME_TOO_LONG";
    case WIFI_REASON_SA_QUERY_TIMEOUT:          return "SA_QUERY_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY: return "NO_AP_COMPAT_SEC";
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD: return "NO_AP_AUTH_THRESHOLD";
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD: return "NO_AP_RSSI_THRESHOLD";
    default:                                    return "OTHER";
    }
}

void cloud_diag_mqtt_step_change(uint8_t from_step, uint8_t to_step, const char *reason)
{
    uint8_t fail_cnt = 0;
    if (from_step == to_step) {
        return;
    }
    iot_mqtt_cloud_diag_state(NULL, NULL, &fail_cnt);
    ESP_LOGW(CLOUD_DIAG_TAG,
             "[MQTT1_STEP] %s -> %s | reason:%s | fail_cnt:%u | uptime:%lus",
             cloud_diag_mqtt_step_name(from_step),
             cloud_diag_mqtt_step_name(to_step),
             reason ? reason : "-",
             fail_cnt,
             (unsigned long)reals.now);
}

void cloud_diag_mqtt_ctrl(const char *op, const char *trigger,
                          uint8_t mqtt_is_login, uint8_t mqtt_step, uint8_t mqtt_connect_fail)
{
    ESP_LOGW(CLOUD_DIAG_TAG,
             "[MQTT1_CTRL] op:%s trigger:%s | login:%u step:%s fail_cnt:%u | "
             "wifi_sta:%u wifi_ap_en:%u ap_mode:%u uptime:%lus",
             op ? op : "-", trigger ? trigger : "-",
             mqtt_is_login, cloud_diag_mqtt_step_name(mqtt_step), mqtt_connect_fail,
             (unsigned)g_self_data.mod_reg11000_IOT_info.link.bit.wifi_sta,
             (unsigned)SetData.dev_info_t.on_off.bit.wifi_ap_enable,
             (unsigned)reals.wifi_ap_mode,
             (unsigned long)reals.now);
}

void cloud_diag_mqtt_event(int event_id, esp_mqtt_event_handle_t event,
                           uint8_t mqtt_is_login_before, uint8_t mqtt_step)
{
    const char *evt_name = "UNKNOWN";
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:     evt_name = "CONNECTED"; break;
    case MQTT_EVENT_DISCONNECTED: evt_name = "DISCONNECTED"; break;
    case MQTT_EVENT_SUBSCRIBED:    evt_name = "SUBSCRIBED"; break;
    case MQTT_EVENT_UNSUBSCRIBED:  evt_name = "UNSUBSCRIBED"; break;
    case MQTT_EVENT_PUBLISHED:     evt_name = "PUBLISHED"; break;
    case MQTT_EVENT_DATA:          evt_name = "DATA"; break;
    case MQTT_EVENT_ERROR:         evt_name = "ERROR"; break;
    case MQTT_EVENT_BEFORE_CONNECT: evt_name = "BEFORE_CONNECT"; break;
    default: break;
    }

    if (event_id == MQTT_EVENT_ERROR && event && event->error_handle) {
        ESP_LOGE(CLOUD_DIAG_TAG,
                 "[MQTT1_EVT] %s | login_before:%u step:%s | err_type:%s refuse:%s | "
                 "tls_esp:0x%x tls_stack:0x%x sock_errno:%d(%s) | uptime:%lus",
                 evt_name, mqtt_is_login_before, cloud_diag_mqtt_step_name(mqtt_step),
                 mqtt_error_type_str(event->error_handle->error_type),
                 mqtt_refuse_code_str(event->error_handle->connect_return_code),
                 event->error_handle->esp_tls_last_esp_err,
                 event->error_handle->esp_tls_stack_err,
                 event->error_handle->esp_transport_sock_errno,
                 strerror(event->error_handle->esp_transport_sock_errno),
                 (unsigned long)reals.now);
        return;
    }

    if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGE(CLOUD_DIAG_TAG,
                 "[MQTT1_EVT] DISCONNECTED | login_before:%u step:%s | "
                 "last_login_cloud:%lus last_disc_cloud:%lus | "
                 "wifi_sta_link:%u wifi_ap_en:%u ap_mode:%u | uptime:%lus",
                 mqtt_is_login_before, cloud_diag_mqtt_step_name(mqtt_step),
                 (unsigned long)reals.last_login_cloud_time,
                 (unsigned long)reals.last_disconnect_cloud_time,
                 (unsigned)g_self_data.mod_reg11000_IOT_info.link.bit.wifi_sta,
                 (unsigned)SetData.dev_info_t.on_off.bit.wifi_ap_enable,
                 (unsigned)reals.wifi_ap_mode,
                 (unsigned long)reals.now);
        cloud_diag_snapshot("mqtt_disconnected");
        return;
    }

    if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGW(CLOUD_DIAG_TAG,
                 "[MQTT1_EVT] CONNECTED(tcp) | login_before:%u step:%s | "
                 "waiting SUBSCRIBED(10s timeout) | uptime:%lus",
                 mqtt_is_login_before, cloud_diag_mqtt_step_name(mqtt_step),
                 (unsigned long)reals.now);
        return;
    }

    if (event_id == MQTT_EVENT_SUBSCRIBED) {
        ESP_LOGW(CLOUD_DIAG_TAG,
                 "[MQTT1_EVT] SUBSCRIBED(cloud_ready) | msg_id:%d | "
                 "login_before:%u step:%s | uptime:%lus",
                 event ? event->msg_id : -1,
                 mqtt_is_login_before, cloud_diag_mqtt_step_name(mqtt_step),
                 (unsigned long)reals.now);
        cloud_diag_snapshot("mqtt_subscribed");
        return;
    }

    ESP_LOGI(CLOUD_DIAG_TAG,
             "[MQTT1_EVT] %s | login_before:%u step:%s | uptime:%lus",
             evt_name, mqtt_is_login_before, cloud_diag_mqtt_step_name(mqtt_step),
             (unsigned long)reals.now);
}

void cloud_diag_mqtt_sub_timeout(uint8_t mqtt_step)
{
    ESP_LOGE(CLOUD_DIAG_TAG,
             "[MQTT1_TIMEOUT] subscribe_ack_10s | step:%s | "
             "CONNECTED but no SUBSCRIBED -> RESTART | uptime:%lus",
             cloud_diag_mqtt_step_name(mqtt_step), (unsigned long)reals.now);
    cloud_diag_snapshot("mqtt_sub_timeout");
}

void cloud_diag_wifi_sta_disconnected(uint8_t reason, int8_t rssi, uint8_t retry_num)
{
    ESP_LOGE(CLOUD_DIAG_TAG,
             "[WIFI_STA] disconnected | reason:%u(%s) rssi:%d retry:%u | "
             "ap_mode:%u ap_en:%u | last_disc_router:%lus uptime:%lus",
             reason, cloud_diag_wifi_reason_str(reason), rssi, retry_num,
             (unsigned)reals.wifi_ap_mode,
             (unsigned)SetData.dev_info_t.on_off.bit.wifi_ap_enable,
             (unsigned long)reals.last_disconnect_router_time,
             (unsigned long)reals.now);
}

void cloud_diag_wifi_got_ip(const char *ip_str, uint8_t ap_mode, uint8_t ap_enable)
{
    ESP_LOGW(CLOUD_DIAG_TAG,
             "[WIFI_STA] got_ip:%s | ap_mode:%u ap_en:%u | will_trigger_mqtt_start | uptime:%lus",
             ip_str ? ip_str : "-", ap_mode, ap_enable, (unsigned long)reals.now);
}

void cloud_diag_wifi_task_event(const char *event, const char *detail,
                                uint8_t sta_link_before, uint8_t mqtt_deleted)
{
    ESP_LOGW(CLOUD_DIAG_TAG,
             "[WIFI_TASK] %s | %s | sta_link_before:%u mqtt_deleted:%u | uptime:%lus",
             event ? event : "-", detail ? detail : "-",
             sta_link_before, mqtt_deleted, (unsigned long)reals.now);
}

void cloud_diag_snapshot(const char *tag)
{
    uint8_t step = 0;
    uint8_t is_login = 0;
    uint8_t fail_cnt = 0;
    iot_mqtt_cloud_diag_state(&step, &is_login, &fail_cnt);

    ESP_LOGW(CLOUD_DIAG_TAG,
             "[SNAPSHOT] tag:%s | mqtt_login:%u mqtt_is_login:%u mqtt_step:%s | "
             "wifi_sta:%u wifi_connect_ap:%u sta_link:%u | "
             "ap_en:%u ap_mode:%u mqtt_fail:%u | "
             "last_login_cloud:%lus last_disc_cloud:%lus last_disc_router:%lus | uptime:%lus",
             tag ? tag : "-",
             (unsigned)g_self_data.mod_reg11000_IOT_info.link.bit.mqtt,
             is_login,
             cloud_diag_mqtt_step_name(step),
             (unsigned)g_self_data.mod_reg11000_IOT_info.link.bit.wifi_sta,
             (unsigned)reals.wifi_connect_ap,
             (unsigned)wifi_info.flag.bit.sta_link_ap,
             (unsigned)SetData.dev_info_t.on_off.bit.wifi_ap_enable,
             (unsigned)reals.wifi_ap_mode,
             fail_cnt,
             (unsigned long)reals.last_login_cloud_time,
             (unsigned long)reals.last_disconnect_cloud_time,
             (unsigned long)reals.last_disconnect_router_time,
             (unsigned long)reals.now);
}
