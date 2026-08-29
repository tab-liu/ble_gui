#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "comm_define.h"

typedef enum
{
    WIFI_AP_MODE_CLOSE = 0,     //未开启AP
    WIFI_AP_MODE_NORMAL = 1,    //wifi access point模式
    WIFI_AP_MODE_MESH = 2,      //wifi-mesh模式
}WiFi_AP_mode_t;

typedef union {
    uint8_t status;
    struct {
        uint8_t start       : 1; // 开始标志
        uint8_t running     : 1; // 运行标志
        uint8_t end         : 1; // 结束标志
        uint8_t update      : 1; // 更新标志
		uint8_t stop        : 2; // 中断标志
    }bit;
}send_flag_t;

typedef union {
    uint16_t all;
    struct {
        uint16_t timeout        : 1; // 时间结束current_time >= receive_end_time
        uint16_t app_cancel     : 1; // 用户取消DisasterSaveData.AppCancelStormEvent.EventEndFlag == 1
        uint16_t soc_reach      : 1; // SOC达到目标system_soc >= DisasterSaveData.SaveWebStormWatchApiData.SoC
        uint16_t disable        : 1; // 关闭预警开关StormWatchSetData.StormWatchEnable == 2
        uint16_t discharge      : 1; //电池放电或待机
        uint16_t new_event      : 1; // 新事件到来
        uint16_t reserved       : 10;
    } bit;
} storm_end_reason_t;

typedef struct {
    uint8_t soc_start;
    uint8_t soc_end;
    uint16_t revd1;
    uint32_t BackupStartTime;
    uint32_t BackupEndTime;
    send_flag_t  icloud_send_flag;
    send_flag_t  dev_send_flag;
	send_flag_t  send_flag;
    uint32_t total_chg_energy_begin;
    uint32_t total_chg_energy_end;
    uint16_t old_work_mode;
    uint16_t uuid;
	int32_t TimeZoneOffset;
	char msg_id[100];
    storm_end_reason_t reason;
    uint16_t revd[9];
} disaster_send_param_t;

typedef union {
    uint16_t all;
    struct {
        uint16_t bind_cmd           : 2; // bit1/0:绑定命令
        uint16_t temperature_unit   : 2; // bit3/2:温度单位(0:invalid, 1:℃, 2:℉)
        uint16_t lcd_active_time    : 4; // bit7/4 0：无效（APP不显示）1：15s；2: 30s；3: 1分；4：5分；5：常亮 ；6：跟随逆变器
        uint16_t revd2              : 8; // bit15/8:预留
    };
}lcd_mode_struct;

typedef union {
    uint16_t all;
    struct {
        /*0:invalid, 1:每日，2，每周，3，每月，4，每年，5-至今*/
        uint16_t gridpower_off      : 4; // bit3/0:断电次数时间跨度
        uint16_t money_save         : 4; // bit7/4:省钱参数时间跨度
        uint16_t revd               : 8; 
    };
}time_span_struct;

typedef union 
{
    uint16_t all;
    struct {
        uint8_t adv_en          : 4;         // bit0~3 server端广播数据使能
        uint8_t key_update      : 2;         // bit4~5 刷新公有广播秘钥，0:无效 1:刷新 2:恢复默认
        uint8_t general_adv_en  : 2;         // bit6~7 通用广播使能（record type为0x00~0x7F）
        uint8_t lcd_data_adv_en : 2;         // bit8~9 LCD数据广播使能(record type为0x80/0x81)
        uint8_t modbus_adv_en   : 2;         // bit10~11 广播modbus读写使能
        uint8_t revd            : 4;
    };
} ble_protocol_en_t;

typedef union 
{
    uint16_t all;
    struct {
        uint8_t sta1_en : 2;                // bit0~1 sta1 使能
        uint8_t sta2_en : 2;                // bit2~3 sta2 使能
        uint8_t sta3_en : 2;                // bit4~5 sta3 使能
        uint8_t sta1_static_ip_en : 2;     // bit6~7 sta1 静态IP使能
        uint8_t sta2_static_ip_en : 2;     // bit8~9 sta2 静态IP使能
        uint8_t sta3_static_ip_en : 2;     // bit10~11 sta3 静态IP使能
        uint8_t revd: 4;
    };
} wifi_mul_sta_en_t;

typedef union
{
    uint16_t all;
    struct {
        uint8_t enable : 2;          // bit0~1 使能
        uint8_t crypt_en : 2;        // bit2~3 加密使能
        uint8_t ctrl_en : 2;         // bit4~5 控制使能
        uint8_t report_en : 2;       // bit6~7 上报使能
        uint8_t revd: 8;
    };
} open_mqtt_en_t;

typedef union
{
    uint16_t all;
    struct {
        uint16_t enable : 2;          // bit0~1 使能
        uint16_t crypt_en : 2;        // bit2~3 加密使能
        uint16_t revd: 12;
    };
}modbus_tcp_en_t;

typedef union {
    uint32_t all;
    struct {
        uint32_t relay1  : 1;   // DC
        uint32_t relay2  : 1;   // AC
        uint32_t relay3  : 1;
        uint32_t relay4  : 1;
        uint32_t relay5  : 1;
        uint32_t relay6  : 1;
        uint32_t relay7  : 1;
        uint32_t relay8  : 1;
        uint32_t relay9  : 1;
        uint32_t relay10 : 1;
        uint32_t relay11 : 1;
        uint32_t relay12 : 1;
        uint32_t relay13 : 1;
        uint32_t relay14 : 1;
        uint32_t relay15 : 1;
        uint32_t relay16 : 1;
        uint32_t relay17 : 1;
        uint32_t relay18 : 1;
        uint32_t relay19 : 1;
        uint32_t relay20 : 1;
        uint32_t relay21 : 1;
        uint32_t relay22 : 1;
        uint32_t relay23 : 1;
        uint32_t relay24 : 1;
        uint32_t relay25 : 1;
        uint32_t relay26 : 1;
        uint32_t relay27 : 1;
        uint32_t relay28 : 1;
        uint32_t relay29 : 1;
        uint32_t relay30 : 1;
        uint32_t relay31 : 1;
        uint32_t relay32 : 1;
    }bit;
}iot_smart_ctrl_struct;

enum {
    METER_MFG_ID_UNKNOWN = 0,
    METER_MFG_ID_SHELLY = 1,
    METER_MFG_ID_ECOTRACKER = 2,

    METER_MFG_ID_BLUETTI = 200,
};

#define PANEL_IOT_CAN_ADDR      0xA4 //0xA4~0xA6    
#define EBOX_PBOX_CAN_ADDR      0x01

#pragma pack()



