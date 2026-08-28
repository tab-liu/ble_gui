#pragma once

#include "wlcc_common.h"
#include "comm_define.h"
// #include "parameter.h"
#include "wlcc_tlv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 最大协议帧长度: Modbus  */
#define WLCC_MAX_FRAME_SIZE     1500

/* 最大等待队列数量  */
#define WLCC_MSG_PENDING_MAX   50

/* 最大等待返回时间  */
#define WLCC_PENDING_TIMEOUT_MAX   3000


typedef struct {
    uint8_t *data;          // 数据指针
    int len;                // 数据长度
    uint8_t dst_ip[32];     // 目的IP地址
    uint16_t dst_port;      // 目的端口
} wlcc_msg_t;

typedef struct {
    uint16_t msg_id; 	// 消息ID
    uint16_t devTpye; 	// 目标设备类型
    uint64_t devSn; 	// 目标设备SN
    uint32_t timeout;   // 消息超时时间
	modbus_addr_info_t src_addr; 	// 消息来源通道
} wlcc_msg_pending_t;

typedef struct {
    uint16_t dev_type; 		// 设备类型
	uint64_t dev_sn;		// 设备序列号
	uint16_t dev_priority;	// 设备优先级
	uint32_t last_online_time;//设备节点	最后一次在线时间,时间戳,相对1970年的秒数
	uint8_t online_status;	// 设备在线状态
	uint32_t ip;
	uint16_t port;
} wlcc_dev_t;


typedef struct {
    uint8_t is_master;				/**< 当前设备是否为主设备 0:invalid, 1:master, 2:slave */
	uint8_t is_new_master;			/**< 当前设备是否为主设备 0:invalid, 1:master, 2:slave */
	uint64_t last_parallel_time;
	wlcc_dev_t master_info;
} wlcc_master_t;



uint8_t wlcc_remove_pending_msg(wlcc_msg_pending_t msg_pending);
wlcc_msg_pending_t wlcc_check_message_response(wlcc_dev_info_t src_addr);

uint8_t wlcc_is_master(void);
uint8_t wlcc_dev_master_type(uint16_t dev_type, uint64_t dev_sn);
void wlcc_master_triger(void);
void wlcc_master_pk(wlcc_dev_info_t dev_info);
void wlcc_master_online_time_update(wlcc_dev_info_t dev_info, uint32_t online_time);


void wlcc_start(netif_type_t netif_type);
void wlcc_stop(void);
void wlcc_task(void);

void update_device_discovery_info(wlcc_dev_info_t dev_info);

int wlcc_dev_online_time_get(uint16_t dev_type, uint64_t dev_sn);

uint8_t wlcc_modbus_msg_send(uint8_t *income, uint16_t cmd_len, uint16_t dev_type, uint64_t dev_sn, modbus_addr_info_t src_addr);
int wlcc_msg_send_to_queue(const uint8_t *data_buf, uint16_t len, const char *dst_ip_str, uint16_t dst_port);

int wlcc_modbus_msg_send_21000_tlv(const wlcc_tlv_block_t *tlv_blocks,
                                   uint16_t tlv_block_count,
                                   uint16_t dev_type,
                                   uint64_t dev_sn);
int wlcc_modbus_msg_send_21000(uint8_t slave_address, const uint8_t *pIn, uint16_t len, uint16_t dst_type, uint64_t dst_sn);
uint8_t wlcc_dev_online_num_get(void);

#ifdef __cplusplus
}
#endif

