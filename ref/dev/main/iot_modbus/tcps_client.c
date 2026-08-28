/**
 * @file tcps_client.c
 * @brief TCP client over TLS/SSL implementation for Modbus TCP Master communication
 */

#include "tcps_client.h"
#include "iot_partition.h"
#include "utils.h"

#include <errno.h>
#include "mbedtls/build_info.h"
#include "mbedtls/platform.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif

static const char *TAG = "[TCPS_CLIENT]";

// Internal mbedtls context structure
typedef struct {
    mbedtls_net_context socket_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_key;
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context cache;
#endif
} mbedtls_tls_client_ctx_t;

static void my_debug(void *ctx, int level, const char *file, int line, const char *str) {
    ((void)level);
    mbedtls_fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *)ctx);
}

int tcps_client_connect(modbus_tcp_client_t *client) {
    if (client == NULL) {
        ESP_LOGE(TAG, "tcps client connect error, client is NULL.");
        return -1;
    }

    int ret = 0;
    const char *pers = "bluetti_client";
    mbedtls_tls_client_ctx_t *ctx;

    if (client->client_ctx.tcps.tls_ctx == NULL) {
        client->client_ctx.tcps.tls_ctx = (uint8_t *)iot_calloc(sizeof(mbedtls_tls_client_ctx_t));
        if (client->client_ctx.tcps.tls_ctx == NULL) {
            ESP_LOGE(TAG, "tcps client connect error, malloc client ctx failed.");
            return -2;
        }
    }

    ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;
    mbedtls_net_init(&ctx->socket_fd);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_x509_crt_init(&ctx->cacert);
    mbedtls_x509_crt_init(&ctx->client_cert);
    mbedtls_pk_init(&ctx->client_key);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);

    if ((ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg,
                                     mbedtls_entropy_func,
                                     &ctx->entropy,
                                     (const unsigned char *)pers,
                                     strlen(pers)))
        != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        return -4;
    }

    if ((ret = mbedtls_net_connect(
             &ctx->socket_fd, client->config.server_ip, client->config.port, MBEDTLS_NET_PROTO_TCP))
        != 0) {
        ESP_LOGE(TAG, "mbedtls_net_connect error returned %d", ret);
        return -8;
    }

    if (client->config.block == CLIENT_MODE_NO_BLOCK) {
        mbedtls_net_set_nonblock(&ctx->socket_fd);
    } else {
        mbedtls_net_set_block(&ctx->socket_fd);
    }

    if ((ret = mbedtls_ssl_config_defaults(&ctx->conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT))
        != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        return -9;
    }

    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    mbedtls_ssl_conf_dbg(&ctx->conf, my_debug, stdout);

    if ((ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned %d", ret);
        return -11;
    }

    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->socket_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    while ((ret = mbedtls_ssl_handshake(&ctx->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake error returned %d", ret);
            return -12;
        }
    }

    ESP_LOGI(TAG, "TLS client handshake completed successfully");
    client->client_ctx.state = TCP_CLIENT_STATE_CONNECTED;

    return 0;
}

int tcps_client_disconnect(modbus_tcp_client_t *client) {
    if (client == NULL || client->client_ctx.tcps.tls_ctx == NULL) {
        ESP_LOGE(TAG, "tcps client disconnect error, client is NULL.");
        return -1;
    }

    mbedtls_tls_client_ctx_t *ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;

    mbedtls_ssl_close_notify(&ctx->ssl);
    mbedtls_net_free(&ctx->socket_fd);
    client->client_ctx.state = TCP_CLIENT_STATE_DISCONNECTED;

    ESP_LOGI(TAG, "TLS client disconnected");
    return 0;
}

int tcps_client_send(modbus_tcp_client_t *client, const void *buf, size_t len) {
    if (client == NULL || client->client_ctx.tcps.tls_ctx == NULL) {
        ESP_LOGE(TAG, "tcps client send error, client is NULL.");
        return -1;
    }

    int ret = 0;
    mbedtls_tls_client_ctx_t *ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;
    client->client_ctx.state = TCP_CLIENT_STATE_SENDING;

    while ((ret = mbedtls_ssl_write(&ctx->ssl, buf, len)) <= 0) {
        if (ret == MBEDTLS_ERR_NET_CONN_RESET) {
            ESP_LOGE(TAG, "peer closed the connection");
            client->client_ctx.state = TCP_CLIENT_STATE_ERROR;
            return ret;
        }

        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned %d", ret);
            client->client_ctx.state = TCP_CLIENT_STATE_ERROR;
            return ret;
        }
    }

    client->client_ctx.state = TCP_CLIENT_STATE_CONNECTED;
    ESP_LOGI(TAG, "TLS client sent %d bytes", ret);
    return ret;
}

int tcps_client_recv(modbus_tcp_client_t *client, void *buf, size_t len) {
    if (client == NULL || client->client_ctx.tcps.tls_ctx == NULL) {
        ESP_LOGE(TAG, "tcps client recv error, client is NULL.");
        return -1;
    }

    int ret = 0;
    mbedtls_tls_client_ctx_t *ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;
    client->client_ctx.state = TCP_CLIENT_STATE_RECEIVING;

    do {
        ret = mbedtls_ssl_read(&ctx->ssl, buf, len);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (ret <= 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", (unsigned int)-ret);
            client->client_ctx.state = TCP_CLIENT_STATE_ERROR;
            return ret;
        }

        break;
    } while (1);

    client->client_ctx.state = TCP_CLIENT_STATE_CONNECTED;
    ESP_LOGI(TAG, "TLS client received %d bytes", ret);
    return ret;
}

int tcps_client_is_read_ready(modbus_tcp_client_t *client, struct timeval *tv) {
    if (client == NULL || client->client_ctx.tcps.tls_ctx == NULL) {
        ESP_LOGE(TAG, "tcps client read ready error, client is NULL.");
        return -1;
    }

    mbedtls_tls_client_ctx_t *ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;
    int socket_fd = ctx->socket_fd.fd;

    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(socket_fd, &rset);

    int s_rc;
    while ((s_rc = select(socket_fd + 1, &rset, NULL, NULL, tv)) == -1) {
        if (errno == EINTR) {
            ESP_LOGE(TAG, "A non blocked signal was caught\n");
            FD_ZERO(&rset);
            FD_SET(socket_fd, &rset);
        } else {
            ESP_LOGE(TAG, "select -1");
            return -1;
        }
    }

    return s_rc;
}

int tcps_client_is_write_ready(modbus_tcp_client_t *client, struct timeval *tv) {
    if (client == NULL || client->client_ctx.tcps.tls_ctx == NULL) {
        ESP_LOGE(TAG, "tcps client read ready error, client is NULL.");
        return -1;
    }

    mbedtls_tls_client_ctx_t *ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;
    int socket_fd = ctx->socket_fd.fd;

    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(socket_fd, &rset);

    int s_rc;
    while ((s_rc = select(socket_fd + 1, NULL, &rset, NULL, tv)) == -1) {
        if (errno == EINTR) {
            ESP_LOGE(TAG, "A non blocked signal was caught\n");
            FD_ZERO(&rset);
            FD_SET(socket_fd, &rset);
        } else {
            ESP_LOGE(TAG, "select -1");
            return -1;
        }
    }

    return s_rc;
}

bool tcps_client_is_connected(modbus_tcp_client_t *client) {
    if (client == NULL) {
        return false;
    }
    return (client->client_ctx.tcps.tls_ctx != NULL)
           && (client->client_ctx.state == TCP_CLIENT_STATE_CONNECTED);
}

void tcps_client_reset(modbus_tcp_client_t *client) {
    if (client == NULL || client->client_ctx.tcps.tls_ctx == NULL) {
        return;
    }

    mbedtls_tls_client_ctx_t *ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;
    mbedtls_net_free(&ctx->socket_fd);
    mbedtls_ssl_session_reset(&ctx->ssl);
    client->client_ctx.state = TCP_CLIENT_STATE_DISCONNECTED;

    ESP_LOGI(TAG, "TLS client reset");
}

void tcps_client_close(modbus_tcp_client_t *client) {
    if (client == NULL || client->client_ctx.tcps.tls_ctx == NULL) {
        return;
    }

    mbedtls_tls_client_ctx_t *ctx = (mbedtls_tls_client_ctx_t *)client->client_ctx.tcps.tls_ctx;

    mbedtls_ssl_session_reset(&ctx->ssl);
    mbedtls_net_free(&ctx->socket_fd);

    mbedtls_x509_crt_free(&ctx->cacert);
    mbedtls_x509_crt_free(&ctx->client_cert);
    mbedtls_pk_free(&ctx->client_key);
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);

    if (ctx) {
        free(ctx);
        ctx = NULL;
    }

    client->client_ctx.state = TCP_CLIENT_STATE_INIT;
    ESP_LOGI(TAG, "TLS client closed.");
}
