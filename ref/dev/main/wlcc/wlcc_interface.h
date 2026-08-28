#pragma once

#include "wlcc_common.h"
#include "comm_define.h"
#include "lwip/sockets.h"

#define MAKE_UINT16(lo, hi) (uint16_t)(lo | (hi << 8))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建无线设备间通信协议网络
 * @param netif_type 网络接口类型
 * @return IOT_OK 成功, 否则返回错误码
 */
int create_wlcc_network(netif_type_t netif_type);

/**
 * @brief 销毁无线设备间通信网络
 */
void destroy_wlcc_network(void);

/**
 * @brief 检查无线设备间通信网络是否就绪
 * @return int IOT_OK 如果网络就绪, 否则返回错误码
 */
int is_ready_wlcc_network(void);

/**
 * @brief 接收无线设备间通信协议帧
 * @param[out] rx_buf 接收缓冲区
 * @param[in] rx_buf_size 接收缓冲区大小
 * @param[out] src_ip_str 源设备IP地址字符串，如果为NULL则不返回
 * @param[out] src_port 源设备端口，如果为NULL则不返回
 * @return int 接收到的字节数，成功时返回接收到的字节数，失败时返回错误码
 */
int recv_wlcc(uint8_t *rx_buf, uint16_t rx_buf_size, char *src_ip_str, uint16_t *src_port);

/**
 * @brief 发送数据到指定的IP和端口
 * @param[in] data       发送的数据缓冲区
 * @param[in] data_len   发送的数据长度
 * @param[in] dst_ip     目的IP地址，如果为NULL则使用组播地址
 * @param[in] dst_port   目的端口，如果为0则使用组播端口
 * @return int 发送的字节数，成功时返回发送的字节数，失败时返回错误码
 */
int send_wlcc(const uint8_t *data, uint16_t data_len, const char *dst_ip, uint16_t dst_port);

#ifdef __cplusplus
}
#endif

