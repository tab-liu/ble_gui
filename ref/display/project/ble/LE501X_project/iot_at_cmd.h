/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_at_cmd.h
Author: weiyt
Version: V1.0
Date: 2022.5.25
Description: none
*************************************************/

#ifndef IOT_AT_CMD_H
#define IOT_AT_CMD_H

#include <string.h>
#include <stdio.h>
#include "common.h"
#include "platform.h"
#include "log.h"

enum {
    IM_READY = 1, /*准备完毕*/
    IM_CONN, /*蓝牙连接*/
    IM_DISC, /*蓝牙断开连接*/
};

extern int8_t at_cmd_analyze(char *data, uint8_t len);
extern void send_at_indicate(uint8_t state);

#endif
