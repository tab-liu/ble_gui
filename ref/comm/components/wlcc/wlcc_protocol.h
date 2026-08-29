#pragma once

#include "comm_define.h"
#include "modbus_data.h"
#include "parameter.h"
#include "wlcc_common.h"
#include "wlcc_tlv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t rx_bytes_num; //接收报文字节数
    uint16_t tx_bytes_num;
    uint16_t reg_addr;//作为modbus主的辅助解析变量，寄存器地址
    uint16_t reg_cnt;//作为modbus主的辅助解析变量，度寄存器数量
    uint8_t slave_addr; //只在 UDP RX 报文接收时候，和reals.discovery_net_Info[]比对查表时候赋值，用于21000上报和 Inv[]数组序号关联
}modbus_handle_t;


#define SN_TYPE_SELF  SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type)// 本机SN type

uint16_t wlcc_get_msg_id(void);

int is_own_message(wlcc_dev_info_t dev_info, uint16_t dst_type, uint64_t dst_sn);
int wlcc_dev_online_check(uint16_t dev_type, uint64_t dev_sn);

int handle_wlcc_frame(uint8_t *income, uint16_t cmdLen, char *income_ip_str, uint16_t inport);
int device_discovery_is_ready(uint8_t *buff, const uint8_t *src_ip);
int build_device_discovery_frame(uint8_t *outbuf, uint8_t step, uint8_t *src_ip);
uint16_t wlcc_build_data_send_frame(uint8_t frametype, uint16_t dev_type, uint64_t dev_sn, 
											uint8_t *inbuf, uint16_t inlen, uint8_t *outbuf);

int wlcc_modbus_to_self_handle(uint8_t *data, int len, wlcc_dev_info_t src_addr);

uint16_t build_common_modbus_frame(uint8_t broadcast,
                                   uint16_t regAddress,
                                   uint8_t regNum,
                                   uint8_t *outbuf,
                                   uint16_t dev_type,
                                   uint64_t dev_sn,
                                   uint8_t slave_address,
                                   uint8_t frametype,
                                   uint16_t *input_regdata,
                                   channel_modbus chl);

uint16_t wlcc_build_data_send_frame_21000_tlv(uint8_t frametype, 
                                              uint16_t dev_type,
                                              uint64_t dev_sn,
                                              const wlcc_tlv_block_t *tlv_blocks,
                                              uint16_t tlv_block_count,
                                              uint8_t *outbuf);

extern EXT_RAM_BSS_ATTR modbus_handle_t g_modbus_handle; // 发送临时辅助变量



#ifdef __cplusplus
}
#endif
