/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: 蓝牙任务处理
Version: V1.00
Function List: none
***********************************************************/

#include "iot_service.h"
#include "iot_ble.h"
#include "iot_uart.h"
#include "iot_at_cmd.h"
#include "iot_modbus_slave.h"
#include "iot_xmodem_client.h"
#include "iot_xmodem.h"
#include "iot_bsp.h"
#include "iot_gpio.h"
#include "iot_data_record.h"
#include "ls_hal_iwdg.h"
#include "iot_timer.h"
#include "iot_magnetic_screen.h"

#define SERVICE_TIMEOUT 1000 // timer units: ms
#define PROTOCAL_VER_TIMEOUT 98 // timer units: 3s

static iot_timer_id_t iot_service_timer_id = 0xFF;

// static uint8_t time_count = 0;
void Service_BleManagement(void);
static void iot_service_timer_cb(void *param);

/*蓝牙管理任务*/
// void Service_BleManagement(void) {
//     uint8_t *recvPtr = NULL;
//     uint16_t recvLen = 0;

//     recvPtr = BleRecvBytes(&recvLen); /*读取蓝牙数据*/
//     if (recvPtr) { /*判断是否有数据*/
//         LOG_I("ble data,len:%d", recvLen);
//         ServiceSendBytes(recvPtr, recvLen); /*串口发送*/
//     }
// }

extern bool Check_Mod_ver;
void device_management(void) {
    uint8_t *prt = NULL;
    uint16_t len = 0;
    // static uint16_t Ver_count = 0;
    // if (++Ver_count > PROTOCAL_VER_TIMEOUT && Check_Mod_ver == true) {
    //     Ver_count = 0;
    //     Check_Com_Version();
    //     LOG_I("XF Run here!!!!!!!");
    // }
    prt = ServiceRecvBytes(&len); /*读取串口数据*/
    if (prt != NULL && len != 0) {
        /*串口数据处理*/
        // LOG_I("service uart,len:%d",len);
        if (at_cmd_analyze((char *)prt, len)) /*判断是否为AT指令*/
        {
            // BleSendBytes(prt, len); /*发送到手机*/
            // LOG_I("to bluetti app");
        }
    }
}

// void ble_self_management(uint8_t *prt, uint16_t rx_len) {
//     uint8_t blebuf[500];
//     uint8_t *recvPtr = prt;
//     uint16_t recvLen = rx_len;
//     int len = 0;
//     if (recvPtr) {
//         len = Modbus_Slave(recvPtr, recvLen, blebuf); /*作为modbus从机解析modbus数据，生成回应*/
//         // LOG_I("XmodemClientInit");
//         if (len > 0) {
//             if (!self_BleSendBytes(blebuf, len)) { /*发送蓝牙数据*/
//                 // LOG_I("XmodemClientInit");
//                 gModbusUpdateRegs.UpdateDataStruct.DeviceType = 0;
//                 XmodemClientInit(gModbusUpdateRegs.UpdateDataStruct.DeviceType, BLE_CHANNEL);
//                 gModbusUpdateRegs.UpdateDataStruct.DeviceType = 0xFFFF;
//             }
//             return;
//         }
//     }
//     /*接收手机发送的固件包，校验保存到内部flash*/
//     blebuf[0] = XmodemClientTask(recvPtr, recvLen, BLE_CHANNEL);
//     if (blebuf[0]) {
//         /*升级应答*/
//         // LOG_I("ack:%02x",blebuf[0]);
//         self_BleSendBytes(blebuf, 1);
//     }
// }

// void Service_Ble_self_deal(void) {
//     uint8_t *prt = NULL;
//     uint16_t len = 0;
//     prt = self_BleRecvBytes(&len); /*读取蓝牙数据*/
//     if (prt != NULL && len != 0) {
//         // LOG_I("self_ble data,len:%d", len);
//         ble_self_management(prt, len);
//     } else {
//         ble_self_management(NULL, 0);
//     }
// }

// void Service_IotDataUpdate(void) {
//     if (TimerExpired(SYSTEM_CONFIG_TIMER, gSoftTimer)) { // 100ms
//         if (time_count++ > 20) {
//             if (get_ble_status()) { /*判断蓝牙连接状态*/
//                 system_run_led_ctrl(0);
//                 time_count = 0;
//             } else {
//                 system_run_led_ctrl(0);
//                 if (time_count > 22) {
//                     system_run_led_ctrl(1);
//                     time_count = 0;
//                 }
//             }
//         }
//     }
// }

// 超时回调函数
void iot_service_timer_cb(void *param) {
    // LOG_I("iot_service_timer_cb");
    // device_management();
    // Service_BleManagement();
    // Service_Ble_self_deal();
    // Service_IotDataUpdate();
    update_icon_ble_con();
    iot_timer_start_task(iot_service_timer_id); /*重启定时器*/
#if IOT_DOG_ENABLE
    HAL_IWDG_Refresh(); /*喂狗*/
#endif
}

/*服务初始化*/
void iot_serivice_init(void) {
    iot_service_timer_id
        = iot_timer_add_task_ms(SERVICE_TIMEOUT, false, iot_service_timer_cb, NULL);
    iot_timer_start_task(iot_service_timer_id);
}

void stop_iot_service_timer(void) {
    iot_timer_stop_task(iot_service_timer_id);
}

void modbus_test(void) {
    // char wifi_name[] = "Hello_Kitty";
    // char wifi_pwd[] = "12345678";
    memset(gModbusDataRegs.DataStruct.StaticData.DeviceType,
           0,
           sizeof(gModbusDataRegs.DataStruct.StaticData.DeviceType));
    // memcpy(gModbusDataRegs.DataStruct.StaticData.DeviceType, "EP500", strlen("EP500"));
    gModbusDataRegs.DataStruct.StaticData.DeviceType[0]
        = ntohs(gModbusDataRegs.DataStruct.StaticData.DeviceType[0]);
    gModbusDataRegs.DataStruct.StaticData.DeviceType[1]
        = ntohs(gModbusDataRegs.DataStruct.StaticData.DeviceType[1]);
    gModbusDataRegs.DataStruct.StaticData.DeviceType[2]
        = ntohs(gModbusDataRegs.DataStruct.StaticData.DeviceType[2]);
    gModbusDataRegs.DataStruct.StaticData.DeviceType[3]
        = ntohs(gModbusDataRegs.DataStruct.StaticData.DeviceType[3]);
    // gModbusDataRegs.DataStruct.StaticData.DeviceType[4] &= 0x0000;

    gModbusDataRegs.DataStruct.StaticData.ComProtocolVersion = 1010;
    // gModbusDataRegs.DataStruct.StaticData.DeviceSNCode[0] = 0xe0ce;
    // gModbusDataRegs.DataStruct.StaticData.DeviceSNCode[1] = 0x2d5b;
    // gModbusDataRegs.DataStruct.StaticData.DeviceSNCode[2] = 0x01e9;

    // gModbusIotRegs.IotDataStruct.WifiAPSignal = 0xED; /*当前AP信号强度*/
    gModbusDataRegs.DataStruct.StaticData.MCU4SoftWareL = (IOT_SYSTEM_VER) & 0xFFFF; /*IOT的版本号*/
    gModbusDataRegs.DataStruct.StaticData.MCU4SoftWareH = (IOT_SYSTEM_VER >> 16) & 0xFFFF;

    //	gModbusIotRegs.IotDataStruct.WirelessIP12   = 0xA8C0;    // network IP address
    //	gModbusIotRegs.IotDataStruct.WirelessIP34   = 0x1213;    // network IP address
    //	gModbusIotRegs.IotDataStruct.WirelessMac12  = 0x1122;    // network MAC address
    //	gModbusIotRegs.IotDataStruct.WirelessMac34  = 0x3344;    // network MAC address
    //	gModbusIotRegs.IotDataStruct.WirelessMac56  = 0x5566;    // network MAC address
    //	memset(gModbusIotRegs.IotDataStruct.InternetName, 0,
    // sizeof(gModbusIotRegs.IotDataStruct.InternetName));
    //	memset(gModbusIotRegs.IotDataStruct.InternetPassword, 0,
    // sizeof(gModbusIotRegs.IotDataStruct.InternetPassword));
    //	memcpy(gModbusIotRegs.IotDataStruct.InternetName, wifi_name,
    // sizeof(gModbusIotRegs.IotDataStruct.InternetName));
    //	memcpy(gModbusIotRegs.IotDataStruct.InternetPassword, wifi_pwd,
    // sizeof(gModbusIotRegs.IotDataStruct.InternetPassword));
}
