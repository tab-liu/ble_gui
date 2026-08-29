#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct modbus_tcp_server modbus_tcp_server_t;

// 函数指针类型定义
typedef int   (*modbus_open_func)(modbus_tcp_server_t *server);
typedef int   (*modbus_listen_func)(modbus_tcp_server_t *server);
typedef int   (*modbus_accept_func)(modbus_tcp_server_t *server);
typedef ssize_t   (*modbus_send_func)(modbus_tcp_server_t *server, const uint8_t *req, int req_length);
typedef ssize_t   (*modbus_recv_func)(modbus_tcp_server_t *server, uint8_t *rsp, int rsp_length);
typedef bool  (*modbus_is_connected)(modbus_tcp_server_t *server);
typedef int  (*modbus_is_read_ready)(modbus_tcp_server_t *server, struct timeval *tv);
typedef void  (*modbus_reset_func)(modbus_tcp_server_t *server);
typedef void  (*modbus_close_func)(modbus_tcp_server_t *server);

typedef struct
{
    int listen_fd;
    int client_fd;
} tcp_ctx_t;     // TCP配置

typedef struct 
{
    void *tls_ctx; // server 上下文指针
} tcps_ctx_t;   // TCP over TLS/SSL配置

typedef enum
{
    SERVER_MODE_NO_BLOCK = 1,
    SERVER_MODE_BLOCK = 0,
} tcp_block_mode_t;

typedef enum
{
    TCP_STATE_INIT = 0,        // 初始化状态
    TCP_STATE_OPEN,            // 打开状态
    TCP_STATE_LISTENNING,      // 监听状态
    TCP_STATE_ACCEPT,          // 接受连接状态
    TCP_STATE_READY,            // 接收数据状态
    TCP_STATE_RESET,            // 重启状态
} tcp_state_t;

struct modbus_tcp_server
{
    uint8_t *query;
    struct server_conf_t
    {
        char port[8];
        uint8_t crypt_en; // 是否加密
        uint8_t block;  // 是否非阻塞
    } config;

    /* tcp连接使用的上下文 */
    struct 
    {
        tcp_ctx_t tcp;      // TCP
        tcps_ctx_t tcps;    // TCP over TLS/SSL
        tcp_state_t next_state; // 连接报错时，下一个状态
    } server_ctx;           // 服务器上下文

    // 函数指针
    modbus_open_func open;
    modbus_listen_func listen;
    modbus_accept_func accept;
    modbus_send_func send;
    modbus_recv_func recv;
    modbus_is_connected is_connected;
    modbus_is_read_ready is_read_ready;
    modbus_reset_func reset;
    modbus_close_func close;
};


