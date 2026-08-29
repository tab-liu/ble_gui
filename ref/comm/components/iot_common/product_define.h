#pragma once

#include <stdint.h>

#define DEBUG_REG_NUM   320 // (16*2+4*2)*8=320

#pragma pack(1)

typedef  struct {
    uint8_t year;   // 年 - 2000偏移
    uint8_t month;  // 月
    uint8_t day;    // 日
    uint8_t hour;   // 时
    uint8_t minute; // 分
    uint8_t second; // 秒
}iot_date_t;

typedef struct {
    uint8_t type; // UNKONW = 0,IOT = 1,INV_ARM = 2,INV_DSP = 3,PACK_BCU = 4,PACK_BMU = 5,PACK_BMS = 6,PACK_M1 = 7,
    uint32_t version; // mcu版本
}can_soft_version;

typedef struct {
    uint16_t type; // 0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块
    uint32_t version; // mcu版本号
}modbus_soft_version;

typedef struct {
    uint16_t write_offset;
    uint16_t write_remain_len;
    uint16_t write_crc16;
    uint8_t  crc_valid;
    uint8_t  write_next_seq;
    uint8_t  devId; //CAN Rx ID源地址
    uint8_t  can_type;//can beta ,can type block,windy add    
    uint8_t  *temp_buffer;
#ifdef STATE_COUNT
    uint32_t send_cnt;
    uint32_t recv_cnt;
    uint32_t error_cnt;
#endif
}rw_cmd_struct;

#pragma pack()


