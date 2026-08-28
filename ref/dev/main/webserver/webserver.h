#pragma once

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

// 本地更新固件: 浏览器浏览本地文件系统，选择固件文件进行更新
#define CONFIG_LOCAL_UPGRADE_SUPPORT    0

enum _upgrade_state_t
{
    UPGRADE_STATE_SUCCESS = 0,
    UPGRADE_STATE_DOING,
    UPGRADE_STATE_FAIL,
};

#ifdef __cplusplus
extern "C" {
#endif

void web_server_start(void);
void web_server_task(void);
uint16_t web_server_config_push(void);
void ws_server_ping(void);
void web_server_upgrade_state_report(uint8_t result, uint8_t pct);
void web_server_version_report(void);

#ifdef __cplusplus
}
#endif
