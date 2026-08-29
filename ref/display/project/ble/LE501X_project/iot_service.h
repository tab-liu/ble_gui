/*************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
File name: iot_service.h
Author: weiyt
Version: V1.0
Date: 2022.5.25
Description: none
*************************************************/

#ifndef IOT_SERVICE_H
#define IOT_SERVICE_H

#include <string.h>
#include <stdio.h>
#include "prf_diss.h"
#include "log.h"
#include "ls_dbg.h"
#include "cpu.h"
#include "builtin_timer.h"

extern void iot_serivice_init(void);
extern void modbus_test(void);
extern void stop_iot_service_timer(void);

#endif
