#pragma once

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

// 文件传输升级 or HTTP升级
#define CONFIG_LOCAL_UPGRADE_SUPPORT    1

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
void web_server_scan_device_report(uint8_t type, uint8_t mfg_id);

#ifdef __cplusplus
}
#endif
