#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

typedef struct modbus_tcp_client modbus_tcp_client_t;

// Master模式的函数指针类型定义
typedef int (*modbus_connect_func)(modbus_tcp_client_t *client);
typedef int (*modbus_disconnect_func)(modbus_tcp_client_t *client);
typedef int (*modbus_send_func)(modbus_tcp_client_t *client, const void *buf, size_t len);
typedef int (*modbus_recv_func)(modbus_tcp_client_t *client, void *buf, size_t len);
typedef bool (*modbus_is_connected)(modbus_tcp_client_t *client);
typedef int (*modbus_is_read_ready)(modbus_tcp_client_t *client, struct timeval *tv);
typedef int (*modbus_is_write_ready)(modbus_tcp_client_t *client, struct timeval *tv);
typedef void (*modbus_reset_func)(modbus_tcp_client_t *client);
typedef void (*modbus_close_func)(modbus_tcp_client_t *client);

typedef struct {
    int socket_fd;
    char server_ip[16];
    uint16_t server_port;
} tcp_client_ctx_t; // TCP客户端配置

typedef struct {
    void *tls_ctx; // client TLS上下文指针
} tcps_client_ctx_t; // TCP over TLS/SSL客户端配置

typedef enum {
    CLIENT_MODE_NO_BLOCK = 1,
    CLIENT_MODE_BLOCK = 0,
} tcp_client_block_mode_t;

typedef enum {
    TCP_CLIENT_STATE_INIT = 0, // 初始化状态
    TCP_CLIENT_STATE_DISCONNECTED, // 断开连接状态
    TCP_CLIENT_STATE_CONNECTING, // 连接中状态
    TCP_CLIENT_STATE_CONNECTED, // 已连接状态
    TCP_CLIENT_STATE_SENDING, // 发送数据状态
    TCP_CLIENT_STATE_RECEIVING, // 接收数据状态
    TCP_CLIENT_STATE_ERROR, // 错误状态
} tcp_client_state_t;

struct modbus_tcp_client {
    uint8_t *request; // 请求缓冲区
    uint8_t *response; // 响应缓冲区

    struct client_conf_t {
        char server_ip[16]; // 服务器IP地址
        char port[8]; // 服务器端口
        uint8_t crypt_en; // 是否加密
        uint8_t block; // 是否阻塞模式
        uint16_t timeout_ms; // 超时时间(毫秒)
        uint8_t retry_count; // 重试次数
    } config;

    /* TCP连接使用的上下文 */
    struct {
        tcp_client_ctx_t tcp; // TCP
        tcps_client_ctx_t tcps; // TCP over TLS/SSL
        tcp_client_state_t state; // 当前状态
        uint16_t transaction_id; // 事务ID
    } client_ctx; // 客户端上下文

    modbus_connect_func connect;
    modbus_disconnect_func disconnect;
    modbus_send_func send;
    modbus_recv_func recv;
    modbus_is_connected is_connected;
    modbus_is_read_ready is_read_ready;
    modbus_is_write_ready is_write_ready;
    modbus_reset_func reset;
    modbus_close_func close;
};

// Modbus TCP Master功能码
#define MODBUS_FC_READ_COILS 0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS 0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS 0x03
#define MODBUS_FC_READ_INPUT_REGISTERS 0x04
#define MODBUS_FC_WRITE_SINGLE_COIL 0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER 0x06
#define MODBUS_FC_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10

// Modbus TCP协议相关定义
#define MODBUS_TCP_MAX_ADU_LENGTH 262
#define MODBUS_TCP_HEADER_LENGTH 7
#define MODBUS_TCP_DATA_OFFSET 6

// 错误码定义
#define MODBUS_TCP_SUCCESS 0
#define MODBUS_TCP_ERROR_CONNECTION -1
#define MODBUS_TCP_ERROR_TIMEOUT -2
#define MODBUS_TCP_ERROR_INVALID_RESPONSE -3
#define MODBUS_TCP_ERROR_SEND_FAILED -4
#define MODBUS_TCP_ERROR_RECV_FAILED -5
