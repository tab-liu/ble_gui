/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: 芯片初始化
Version: V1.00
Function List: none
***********************************************************/

#include "iot_bsp.h"
#include "iot_ble.h"
#include "iot_modbus_slave.h"
#include "iot_service.h"
#include "field_manipulate.h"
#include "reg_syscfg.h"
#include "reg_syscfg_type.h"

int main() {
    REG_FIELD_WR(SYSCFG->ANACFG0, SYSCFG_LVD_CTL, 5); // 根据Charge1调整档位5-2.15v
    REG_FIELD_WR(SYSCFG->CFG, SYSCFG_LVD33_INTE, 1);
    ModbusRegsInit(IOT_SYSTEM_VER); /*modbus初始化*/
    modbus_test();
    iot_ble_init(); /*iot蓝牙初始化*/
}
