// log_redirect.h
#ifndef __LOG_REDIRECT_H__
#define __LOG_REDIRECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef enum {
    LOG_MODE_SERIAL = 0,  // 串口输出模式（默认）
    LOG_MODE_UDP          // UDP输出模式
} log_mode_t;

typedef enum {
    NETWORK_DISCONNECTED = 0,
    NETWORK_CONNECTED
} network_status_t;

// 初始化函数
void log_redirect_init(bool is_product);

// 模式切换函数
bool log_switch_mode(log_mode_t new_mode);

void log_server_ip_set(const char* ip_str);
void log_server_port_set(int port);
void log_type_set(uint32_t type_mark);

/* 应用表中的所有日志等级 */
void apply_log_levels_from_table(void);

#ifdef __cplusplus
}
#endif

#endif // __LOG_REDIRECT_H__

