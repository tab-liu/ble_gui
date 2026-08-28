/**
 * @file tcp_client.c
 * @brief TCP client implementation for Modbus TCP Master communication
 */
#include "modbus_tcp_master_common.h"
#include "tcp_client.h"

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
#include <fcntl.h>

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

static const char *TAG = "[TCP_CLIENT]";

int tcp_client_connect(modbus_tcp_client_t *client) {
    if (client == NULL) {
        ESP_LOGE(TAG, "tcp client connect error, client is NULL.");
        return -1;
    }

    // 如果已经连接，先断开
    if (client->client_ctx.tcp.socket_fd >= 0) {
        ESP_LOGW(TAG, "Client already has a socket, disconnecting first...");
        tcp_client_disconnect(client);
        vTaskDelay(pdMS_TO_TICKS(100)); // 等待断开完成
    }

    int rc;
    struct addrinfo *ai_list;
    struct addrinfo *ai_ptr;
    struct addrinfo ai_hints;
    const char *node = client->config.server_ip;
    const char *service = client->config.port;
    int new_s;

    ESP_LOGI(TAG, "tcp_client_connect to %s:%s", node, service);

    memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_family = AF_UNSPEC;
    ai_hints.ai_socktype = SOCK_STREAM;
    ai_hints.ai_addr = NULL;
    ai_hints.ai_canonname = NULL;
    ai_hints.ai_next = NULL;

    ai_list = NULL;
    rc = getaddrinfo(node, service, &ai_hints, &ai_list);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error returned by getaddrinfo: %d", rc);

        freeaddrinfo(ai_list);
        errno = ECONNREFUSED;
        return -1;
    }

    new_s = -1;
    for (ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        int flags = ai_ptr->ai_socktype;
        int s;

        s = socket(ai_ptr->ai_family, flags, ai_ptr->ai_protocol);
        if (s < 0) {
            ESP_LOGE(TAG, "create socket error");
            continue;
        }

        if (client->config.block == CLIENT_MODE_NO_BLOCK) {
            fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);

            struct timeval opt_on = {
                .tv_sec = 5, 
                .tv_usec = 0,
            };

            if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &opt_on, sizeof(opt_on)) < 0) // 设置阻塞超时时间
            {
                ESP_LOGE(TAG, "socket setsockopt set failed");
                if (shutdown(s, SHUT_RDWR) == -1) {
                    ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", s, errno, strerror(errno));
                }
                if (close(s) == -1) {
                    ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", s, errno, strerror(errno));
                    if ( errno == 9 )
                    {
                        /*Bad file number*/
                        s = -1;
                    }
                }
                else
                {
                    s = -1;
                }

                return 0;
            }
        }

        // 启用 TCP Keep-Alive
        int keepalive = 1;
        int keepidle = 30; // 30秒后开始发送keep-alive
        int keepinterval = 5; // 每5秒发送一次
        int keepcount = 3; // 最多发送3次

        setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
        setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
        setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &keepinterval, sizeof(keepinterval));
        setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &keepcount, sizeof(keepcount));

        rc = connect(s, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (rc != 0) {
            if (errno == EINPROGRESS) {
                ESP_LOGI(TAG, "progress, waiting for completion...");
                fd_set write_fds;
                struct timeval timeout;

                FD_ZERO(&write_fds);
                FD_SET(s, &write_fds);
                timeout.tv_sec = client->config.timeout_ms / 1000;
                timeout.tv_usec = (client->config.timeout_ms % 1000) * 1000;

                rc = select(s + 1, NULL, &write_fds, NULL, &timeout);
                if (rc > 0) {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                        new_s = s;
                        break;
                    } else {
                        ESP_LOGE(TAG, "Connection failed with error: %d", error);
                    }
                } else if (rc == 0) {
                    ESP_LOGE(TAG, "Connection timeout after %d ms", client->config.timeout_ms);
                } else {
                    ESP_LOGE(TAG, "select error: %s", strerror(errno));
                }
            } else if (errno == EALREADY) {
                ESP_LOGW(TAG, "Connection already in progress, closing socket and retrying...");
                close(s);
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            } else {
                ESP_LOGE(TAG, "connect error: %s (errno: %d)", strerror(errno), errno);
            }
            close(s);
            continue;
        }

        new_s = s;

        ESP_LOGI(TAG, "Connecte success to %s:%s, socket fd: %d", node, service, new_s);

        break;
    }
    freeaddrinfo(ai_list);

    if (new_s < 0) {
        ESP_LOGE(TAG, "Failed to connect to server");
        return -1;
    }

    client->client_ctx.tcp.socket_fd = new_s;
    client->client_ctx.state = TCP_CLIENT_STATE_CONNECTED;

    return 0;
}

int tcp_client_disconnect(modbus_tcp_client_t *client) {
    if (client == NULL) {
        ESP_LOGE(TAG, "tcp client disconnect error, client is NULL.");
        return -1;
    }

    if (client->client_ctx.tcp.socket_fd >= 0) {
        shutdown(client->client_ctx.tcp.socket_fd, SHUT_RDWR);
        close(client->client_ctx.tcp.socket_fd);
        client->client_ctx.tcp.socket_fd = -1;
        client->client_ctx.state = TCP_CLIENT_STATE_DISCONNECTED;
        ESP_LOGI(TAG, "TCP client disconnected");
    }

    return 0;
}

int tcp_client_send(modbus_tcp_client_t *client, const void *buf, size_t len) {
    if (client == NULL || client->client_ctx.tcp.socket_fd < 0) {
        ESP_LOGE(TAG, "tcp client send error, client not connected.");
        return -1;
    }

    client->client_ctx.state = TCP_CLIENT_STATE_SENDING;

    ssize_t sent = send(client->client_ctx.tcp.socket_fd, buf, len, MSG_NOSIGNAL);
    if (sent < 0) {
        ESP_LOGE(TAG, "send error: %s", strerror(errno));
        client->client_ctx.state = TCP_CLIENT_STATE_ERROR;
        return -1;
    }

    client->client_ctx.state = TCP_CLIENT_STATE_CONNECTED;
    return sent;
}

int tcp_client_recv(modbus_tcp_client_t *client, void *buf, size_t len) {
    if (client == NULL || client->client_ctx.tcp.socket_fd < 0) {
        ESP_LOGE(TAG, "tcp client recv error, client not connected.");
        return -1;
    }

    client->client_ctx.state = TCP_CLIENT_STATE_RECEIVING;

    ssize_t received = recv(client->client_ctx.tcp.socket_fd, buf, len, 0);
    if (received < 0) {
        ESP_LOGE(TAG, "recv error: %s", strerror(errno));
        client->client_ctx.state = TCP_CLIENT_STATE_ERROR;
        return -1;
    }

    client->client_ctx.state = TCP_CLIENT_STATE_CONNECTED;
    return received;
}

int tcp_client_is_write_ready(modbus_tcp_client_t *client, struct timeval *tv) {
    if (client == NULL || client->client_ctx.tcp.socket_fd < 0) {
        ESP_LOGE(TAG, "tcp client read ready error, client not connected.");
        return -1;
    }

    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(client->client_ctx.tcp.socket_fd, &rset);

    int rc = select(client->client_ctx.tcp.socket_fd + 1, NULL,  &rset, NULL, tv);
    if (rc < 0) {
        if (errno == EINTR) {
            ESP_LOGW(TAG, "select interrupted by signal");
            return 0;
        }
        ESP_LOGE(TAG, "select error: %s", strerror(errno));
        return -1;
    }

    return rc;
}

int tcp_client_is_read_ready(modbus_tcp_client_t *client, struct timeval *tv) {
    if (client == NULL || client->client_ctx.tcp.socket_fd < 0) {
        ESP_LOGE(TAG, "tcp client read ready error, client not connected.");
        return -1;
    }

    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(client->client_ctx.tcp.socket_fd, &rset);

    int rc = select(client->client_ctx.tcp.socket_fd + 1, &rset, NULL, NULL, tv);
    if (rc < 0) {
        if (errno == EINTR) {
            ESP_LOGW(TAG, "select interrupted by signal");
            return 0;
        }
        ESP_LOGE(TAG, "select error: %s", strerror(errno));
        return -1;
    }

    return rc;
}

bool tcp_client_is_connected(modbus_tcp_client_t *client) {
    if (client == NULL) {
        return false;
    }
    return (client->client_ctx.tcp.socket_fd >= 0)
           && (client->client_ctx.state == TCP_CLIENT_STATE_CONNECTED);
}

void tcp_client_reset(modbus_tcp_client_t *client) {
    if (client == NULL) {
        return;
    }

    if (client->client_ctx.tcp.socket_fd >= 0) {
        shutdown(client->client_ctx.tcp.socket_fd, SHUT_RDWR);
        close(client->client_ctx.tcp.socket_fd);
        client->client_ctx.tcp.socket_fd = -1;
    }

    client->client_ctx.state = TCP_CLIENT_STATE_DISCONNECTED;
}

void tcp_client_close(modbus_tcp_client_t *client) {
    if (client == NULL) {
        return;
    }

    tcp_client_disconnect(client);
    client->client_ctx.state = TCP_CLIENT_STATE_INIT;
}
