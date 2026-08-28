#ifndef CLOUD_DIAG_H
#define CLOUD_DIAG_H

#include <stdint.h>
#include "mqtt_client.h"

/**
 * 云端连接诊断日志，统一 TAG=CLOUD_DIAG，便于串口/云端日志过滤。
 * 过滤示例: CLOUD_DIAG
 */
#define CLOUD_DIAG_TAG "CLOUD_DIAG"

void iot_mqtt_cloud_diag_state(uint8_t *step, uint8_t *is_login, uint8_t *connect_fail);

/** MQTT 登录状态机步骤名（与 iot_mqtt.c 中 enum 值一致） */
const char *cloud_diag_mqtt_step_name(uint8_t step);

/** WiFi STA 断开原因可读字符串 */
const char *cloud_diag_wifi_reason_str(uint8_t reason);

/** MQTT 状态机步骤切换 */
void cloud_diag_mqtt_step_change(uint8_t from_step, uint8_t to_step, const char *reason);

/** MQTT 客户端控制：start / delete / new_host 等 */
void cloud_diag_mqtt_ctrl(const char *op, const char *trigger,
                          uint8_t mqtt_is_login, uint8_t mqtt_step, uint8_t mqtt_connect_fail);

/** MQTT 库事件（CONNECTED / DISCONNECTED / ERROR / SUBSCRIBED 等） */
void cloud_diag_mqtt_event(int event_id, esp_mqtt_event_handle_t event,
                           uint8_t mqtt_is_login_before, uint8_t mqtt_step);

/** 订阅 ACK 超时（CONNECTED 后 10s 内未 SUBSCRIBED） */
void cloud_diag_mqtt_sub_timeout(uint8_t mqtt_step);

/** WiFi STA 断开（驱动层事件） */
void cloud_diag_wifi_sta_disconnected(uint8_t reason, int8_t rssi, uint8_t retry_num);

/** WiFi STA 获取 IP */
void cloud_diag_wifi_got_ip(const char *ip_str, uint8_t ap_mode, uint8_t ap_enable);

/** WiFi 任务层事件（连接成功 / 断开处理 / MQTT 联动） */
void cloud_diag_wifi_task_event(const char *event, const char *detail,
                                uint8_t sta_link_before, uint8_t mqtt_deleted);

/** 打印当前连接上下文快照（wifi/mqtt/失败计数等） */
void cloud_diag_snapshot(const char *tag);

#endif
