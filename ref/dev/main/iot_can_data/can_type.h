#ifndef CAN_TYPE_H
#define CAN_TYPE_H

#include <stdint.h>

#pragma pack(1)

enum {
    IOT = 0,
    INV_ARM = 1,
    INV_DSP = 2,
    BMS = 3,
    BA = 4,
    PACK_BCU = 5,
    PACK_BMU = 6,
    PACK_BMS = 7,
    PACK_M1 = 8,
    PACK_SAFT = 9,
    PACK_HV = 10,
	IOT_LCD = 11,
	IOT_LCD2 = 12,
	RF = 13,
    dc_hub = 14,
    ac_hub =15,
};
// 0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块
typedef struct {
    uint8_t type; // UNKONW = 0,IOT = 1,INV_ARM = 2,INV_DSP = 3,PACK_BCU = 4,PACK_BMU = 5,PACK_BMS = 6,PACK_M1 = 7,
    uint32_t version; // mcu版本号
}soft_version;

typedef struct {
    uint16_t write_offset;
    uint16_t write_remain_len;
    uint16_t write_crc16;
    uint8_t  crc_valid;
    uint8_t  write_next_seq;
    uint8_t  devId;//CAN Rx ID源地址,在CanCmdUpdate()初始化
    uint8_t  can_type;//can beta ,can type block,windy add    
    uint8_t  *temp_buffer;
    #ifdef STATE_COUNT
    uint32_t send_cnt;
    uint32_t recv_cnt;
    uint32_t error_cnt;
    #endif
}rw_cmd_struct;


#pragma pack()




#endif
