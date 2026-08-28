/**
 * @file tcp_server.c
 * @brief TCP server implementation for Modbus TCP communication
 */
#include "modbus_tcp_slave.h"
#include "comm_define.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

static const char *TAG = "[TCP_SERVER]";

#define SOCK_CLOEXEC                1
#define AI_ADDRCONFIG               1

/**
 * @brief Open a TCP server socket
 * @note default listening port is 502 (Modbus TCP standard port)
 * @param server modbus tcp server context
 * @return >=0: socket fd, <0: error code
 */
int tcp_server_open(modbus_tcp_server_t *server)
{
    int rc;
    struct addrinfo *ai_list;
    struct addrinfo *ai_ptr;
    struct addrinfo ai_hints;
    const char *node = NULL;
    const char *service;
    int new_s;

    ESP_LOGI(TAG, "tcp_server_open.");

    if (server->config.port[0] == 0)
    {
        service = "502";
    }
    else
    {
        service = server->config.port;
    }

    memset(&ai_hints, 0, sizeof(ai_hints));
    /* If node is not NULL, than the AI_PASSIVE flag is ignored. */
    ai_hints.ai_flags |= AI_PASSIVE;
#ifdef AI_ADDRCONFIG
    ai_hints.ai_flags |= AI_ADDRCONFIG;
#endif
    ai_hints.ai_family = AF_UNSPEC;
    ai_hints.ai_socktype = SOCK_STREAM;
    ai_hints.ai_addr = NULL;
    ai_hints.ai_canonname = NULL;
    ai_hints.ai_next = NULL;

    ai_list = NULL;
    rc = getaddrinfo(node, service, &ai_hints, &ai_list);
    if (rc != 0)
    {
#ifdef HAVE_GAI_STRERROR
        ESP_LOGE(TAG, "Error returned by getaddrinfo: %s\n", gai_strerror(rc));
#else
        ESP_LOGE(TAG, "Error returned by getaddrinfo: %d\n", rc);
#endif
        freeaddrinfo(ai_list);
        errno = ECONNREFUSED;
        return -1;
    }

    new_s = -1;
    for (ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next)
    {
        int flags = ai_ptr->ai_socktype;
        int s;

#ifdef SOCK_CLOEXEC
        flags |= SOCK_CLOEXEC;
#endif

        s = socket(ai_ptr->ai_family, flags, ai_ptr->ai_protocol);
        if (s < 0)
        {
            ESP_LOGE(TAG, "create socket error");
            continue;
        }
        else
        {
            int enable = 1;
            rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
            if (rc != 0)
            {
                close(s);
                ESP_LOGE(TAG, "setsockopt");
                continue;
            }
        }

        rc = bind(s, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (rc != 0)
        {
            close(s);
            ESP_LOGE(TAG, "bind error");
            continue;
        }

        new_s = s;
        break;
    }
    freeaddrinfo(ai_list);

    if (new_s < 0) {
        return -1;
    }

    fcntl(new_s, F_SETFL, fcntl(new_s, F_GETFL) | O_NONBLOCK);
    server->server_ctx.tcp.listen_fd = new_s;

    return new_s;
}

/**
 * @brief Listen on the TCP server socket
 * @param server modbus tcp server context
 * @return 0: success, <0: error code
 */
int tcp_server_listen(modbus_tcp_server_t *server)
{
    if (server == NULL)
    {
        ESP_LOGE(TAG, "tcp server listen error, server is NULL.");
        return -1;
    }

    // 只支持1个连接
    int rc = listen(server->server_ctx.tcp.listen_fd, 1);
    if (rc != 0)
    {
        close(server->server_ctx.tcp.listen_fd);
        ESP_LOGE(TAG, "listen error:%d, mean:%s", rc, strerror(rc));
        return -2;
    }

    return 0;
}

/**
 * @brief Accept a new client connection on the TCP server socket
 * @param server modbus tcp server context
 * @return >=0: client socket fd, <0: error code
 */
int tcp_server_accept(modbus_tcp_server_t *server)
{
    struct sockaddr_in6 addr;
    socklen_t addrlen;
    tcp_ctx_t *tcp_ctx = &server->server_ctx.tcp;

    addrlen = sizeof(addr);
    tcp_ctx->client_fd = accept(tcp_ctx->listen_fd, (struct sockaddr *) &addr, &addrlen);

    if (tcp_ctx->client_fd < 0)
    {
        tcp_ctx->client_fd = -1;

        if(errno == EWOULDBLOCK || errno == ECONNABORTED || errno == EPROTO)
        {
            return 0;
        }

        ESP_LOGE(TAG, "accept error:%d, mean:%s", errno, strerror(errno));
        return -1;
    }

    fcntl(tcp_ctx->client_fd, F_SETFL, fcntl(tcp_ctx->client_fd, F_GETFL) | O_NONBLOCK);

    /** 设置keepalive, 检测client掉线: 实测40s左右可检测到离线 */
    int keepalive = 1;
    int keepidle = 30; // 30秒后开始发送keep-alive
    int keepinterval = 5; // 每5秒发送一次
    int keepcount = 3; // 最多发送3次

    setsockopt(tcp_ctx->client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    setsockopt(tcp_ctx->client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(tcp_ctx->client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepinterval, sizeof(keepinterval));
    setsockopt(tcp_ctx->client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcount, sizeof(keepcount));

    // char buf[INET6_ADDRSTRLEN + 1] = {0};
    // if (inet_ntop(AF_INET, &(addr.sin6_addr), buf, INET6_ADDRSTRLEN) == NULL) {
    //     ESP_LOGW(TAG, "Client connection accepted from unparsable IP.\n");
    // } else {
    //     ESP_LOGW(TAG, "Client connection accepted from %s\n", buf);
    // }

    return tcp_ctx->client_fd;
}

/**
 * @brief Send data to the connected client
 * @param server modbus tcp server context
 * @param req data to send
 * @param req_length length of the data to send
 * @return >=0: number of bytes sent, <0: error code
 */
ssize_t tcp_server_send(modbus_tcp_server_t *server, const uint8_t *req, int req_length)
{
    if (server == NULL || server->server_ctx.tcp.client_fd <= 0)
    {
        ESP_LOGE(TAG, "tcp server send error.");
    }
    /* MSG_NOSIGNAL
       Requests not to send SIGPIPE on errors on stream oriented
       sockets when the other end breaks the connection.  The EPIPE
       error is still returned. */
    return send(server->server_ctx.tcp.client_fd, (const char *) req, req_length, MSG_NOSIGNAL);
}

/**
 * @brief Receive data from the connected client
 * @param server modbus tcp server context
 * @param rsp buffer to store the received data
 * @param rsp_length length of the buffer
 * @return >=0: number of bytes received, <0: error code
 */
ssize_t tcp_server_recv(modbus_tcp_server_t *server, uint8_t *rsp, int rsp_length)
{
    if (server == NULL || server->server_ctx.tcp.client_fd <= 0)
    {
        ESP_LOGE(TAG, "tcp server recv error.");
        return -1;
    }

    ssize_t ret = recv(server->server_ctx.tcp.client_fd, (char *) rsp, rsp_length, 0);
    if (ret < 0)
    {
        // 如果是中断或者没有数据可读，则返回0
        if ((errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
        {
            return 0;
        }
    }

    return ret;
}

/**
 * @brief Check if the TCP server socket is ready to read data
 * @param server modbus tcp server context
 * @param tv timeout value
 * @return >0: data is ready to read, 0: timeout, <0: error code
 */
int tcp_server_read_ready(modbus_tcp_server_t *server, struct timeval *tv)
{
    fd_set rset;
    tcp_ctx_t *tcp_ctx = &server->server_ctx.tcp;

    if (server == NULL || tcp_ctx->client_fd <= 0)
    {
        ESP_LOGE(TAG, "tcp server ready error.");
    }

    /* Add a file descriptor to the set */
    FD_ZERO(&rset);
    FD_SET((tcp_ctx->client_fd), &rset);

    int s_rc;
    while ((s_rc = select(tcp_ctx->client_fd + 1, &rset, NULL, NULL, tv)) == -1)
    {
        if (errno == EINTR)
        {
            ESP_LOGE(TAG, "A non blocked signal was caught\n");
            /* Necessary after an error */
            FD_ZERO(&rset);
            FD_SET((tcp_ctx->client_fd), &rset);
        }
        else
        {
            return -1;
        }
    }

    return s_rc;
}

/**
 * @brief Check if the TCP server is connected to a client
 * @param server modbus tcp server context
 * @return true: connected, false: not connected or error
 */
bool tcp_server_is_connected(modbus_tcp_server_t *server)
{
    if (server == NULL)
    {
        ESP_LOGE(TAG, "tcp server connected error.");
        return false;
    }

    return (server->server_ctx.tcp.client_fd >= 0) ? true:false;
}

/**
 * @brief Close the TCP server socket and client connection
 * @param server modbus tcp server context
 * @return void
 */
void tcp_server_close(modbus_tcp_server_t *server)
{
    tcp_ctx_t *tcp_ctx = &server->server_ctx.tcp;

    if (tcp_ctx->listen_fd >= 0)
    {
        shutdown(tcp_ctx->listen_fd, SHUT_RDWR);
        close(tcp_ctx->listen_fd);
        tcp_ctx->listen_fd = -1;
    }

    if (tcp_ctx->client_fd >= 0)
    {
        shutdown(tcp_ctx->client_fd, SHUT_RDWR);
        close(tcp_ctx->client_fd);
        tcp_ctx->client_fd = -1;
    }
}

/**
 * @brief Reset the TCP client connection
 * @param server modbus tcp server context
 * @return void
 */
void tcp_server_reset(modbus_tcp_server_t *server)
{
    tcp_ctx_t *tcp_ctx = &server->server_ctx.tcp;

    if (tcp_ctx->client_fd >= 0)
    {
        shutdown(tcp_ctx->client_fd, SHUT_RDWR);
        close(tcp_ctx->client_fd);
        tcp_ctx->client_fd = -1;
    }
}
