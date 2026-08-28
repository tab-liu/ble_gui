/**
 * @file tcps_server.c
 * @brief TCP server over TLS/SSL implementation for Modbus TCP communication
 */

#include "tcps_server.h"
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

#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif

static const char *TAG = "[TCPS_SERVER]";

#pragma pack(1)

/* Mbedtls加解密需要使用到的内容 */
typedef struct {
    mbedtls_net_context listen_fd;
    mbedtls_net_context client_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context cache;
#endif
}tcps_server_ctx_t;

#pragma pack()

static void my_debug(void *ctx, int level,
                     const char *file, int line,
                     const char *str)
{
    ((void) level);

    mbedtls_fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

/**
 * @brief Open the TCP server socket and initialize the mbedtls context
 * @param server modbus tcp server context
 * @return 0: success, <0: error code
 */
int tcps_server_open(modbus_tcp_server_t *server)
{
    if (server == NULL)
    {
        ESP_LOGE(TAG, "tcps server init error, server is NULL.");
        return -1;
    }

    if (!server_ca_cert_ptr_len || !server_cert_ptr_len || !server_key_ptr_len)
    {
        ESP_LOGE(TAG, "tcps server init error, cert or key is NULL or empty.");
        return -12;
    }

    int ret = 0;
    const char *pers = "bluetti";
    tcps_ctx_t *tcps_ctx = &server->server_ctx.tcps;

    if (tcps_ctx->tls_ctx == NULL)
    {
        // server->server_ctx = heap_caps_malloc(sizeof(tcps_server_ctx_t), MALLOC_CAP_SPIRAM);
        tcps_ctx->tls_ctx = (uint8_t *)iot_calloc(sizeof(tcps_server_ctx_t));
        if (tcps_ctx->tls_ctx == NULL)
        {
            ESP_LOGE(TAG, "tcps server init error, malloc server ctx failed.");
            return -2;
        }
    }

    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *)tcps_ctx->tls_ctx;
    mbedtls_net_init(&(ctx->listen_fd));
    mbedtls_net_init(&ctx->client_fd);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_init(&ctx->cache);
#endif
    mbedtls_x509_crt_init(&ctx->srvcert);
    mbedtls_pk_init(&ctx->pkey);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to initialize PSA Crypto implementation: %d", (int) status);
        return -3;
    }
#endif

    /* 1. Seed the random number generator */
    if ((ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy,
                                     (const unsigned char *) pers, strlen(pers))) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        return -4;
    }

    /* 2. Load the certificates and private RSA key */
    /* 加载证书链: CA证书和Server证书 */
    ret = mbedtls_x509_crt_parse(&ctx->srvcert, (const unsigned char *) server_cert_ptr,
                                 (1 + strlen((char *)server_cert_ptr)));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse iot cert failed returned %d", ret);
        return -5;
    }

    ret = mbedtls_x509_crt_parse(&ctx->srvcert, (const unsigned char *) server_ca_cert_ptr,
                                 (1 + strlen((char *)server_ca_cert_ptr)));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse ca cert returned %d", ret);
        return -6;
    }

    /* 加载Server端秘钥 */
    ret =  mbedtls_pk_parse_key(&ctx->pkey,
                                (const unsigned char *) server_key_ptr,
                                (1+strlen((char *)server_key_ptr)), NULL, 0,
                                mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_pk_parse_key returned %d", ret);
        return -7;
    }

    /* 3. Setup the "listening" TCP socket */
    if ((ret = mbedtls_net_bind(&ctx->listen_fd, NULL, server->config.port, MBEDTLS_NET_PROTO_TCP)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_net_bind error returned %d", ret);
        return -8;
    }

    // 设置阻塞模式
    if (server->config.block == 0)
    {
        mbedtls_net_set_nonblock(&ctx->listen_fd);
    }
    else
    {
        mbedtls_net_set_block(&ctx->listen_fd);
    }

    /* 4. Setup stuff */
    if ((ret = mbedtls_ssl_config_defaults(&ctx->conf,
                                           MBEDTLS_SSL_IS_SERVER,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        return -9;
    }

    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    mbedtls_ssl_conf_dbg(&ctx->conf, my_debug, stdout);

    // mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_conf_session_cache(&ctx->conf, &ctx->cache,
                                   mbedtls_ssl_cache_get,
                                   mbedtls_ssl_cache_set);
#endif

    mbedtls_ssl_conf_ca_chain(&ctx->conf, ctx->srvcert.next, NULL);
    if ((ret = mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->srvcert, &ctx->pkey)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_conf_own_cert returned %d", ret);
        return -10;
    }

    if ((ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned %d", ret);
        return -11;
    }

    mbedtls_net_free(&ctx->client_fd);
    mbedtls_ssl_session_reset(&ctx->ssl);

    return 0;
}

/**
 * @brief Listen on the TCP server socket
 * @param server modbus tcp server context
 * @return 0: success, <0: error code
 */
int tcps_server_listen(modbus_tcp_server_t *server)
{
    /* mbedtls使用mbedtls_net_accept函数，同时包括listen和accept. */
    return 0;
}

/**
 * @brief Accept a new client connection on the TCP server socket
 * @param server modbus tcp server context
 * @return >=0: client socket fd, <0: error code
 */
int tcps_server_accept(modbus_tcp_server_t *server)
{
    if (server == NULL || server->server_ctx.tcps.tls_ctx == NULL)
    {
        ESP_LOGE(TAG, "tcps server listen error, server is NULL.");
        return -1;
    }

    int ret = 0;
    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;

    /* 5. Wait until a client connects */
    if ((ret = mbedtls_net_accept(&ctx->listen_fd, &ctx->client_fd,
                                  NULL, 0, NULL)) != 0)
    {
        if (MBEDTLS_ERR_SSL_WANT_READ == ret) // 非阻塞模式未accept到连接时, 返回MBEDTLS_ERR_SSL_WANT_READ
        {
            return 0; // 继续等待连接
        }

        mbedtls_net_free(&ctx->client_fd);
        mbedtls_ssl_session_reset(&ctx->ssl);
        ESP_LOGE(TAG, "mbedtls_net_accept error returned %d", ret);
        return -2;
    }
    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    /* 6. Handshake */
    while ((ret = mbedtls_ssl_handshake(&ctx->ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake error returned %d", ret);
            mbedtls_net_free(&ctx->client_fd);
            mbedtls_ssl_session_reset(&ctx->ssl);
            return -3;
        }
    }

    if (ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED)
    {
        ESP_LOGE(TAG, "hello verification requested");
        mbedtls_net_free(&ctx->client_fd);
        mbedtls_ssl_session_reset(&ctx->ssl);
        ret = 0;
        return -4;
    }
    else if (ret != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", (unsigned int) -ret);
        mbedtls_net_free(&ctx->client_fd);
        mbedtls_ssl_session_reset(&ctx->ssl);
        return -5;
    }

    ESP_LOGI(TAG, " mbedtls_ssl_handshake ok");
    mbedtls_net_set_nonblock(&ctx->client_fd);

    return 1;
}

/**
 * @brief Send data to the connected client
 * @param server modbus tcp server context
 * @param req data to send
 * @param req_length length of the data to send
 * @return >=0: number of bytes sent, <0: error code
 */
ssize_t tcps_server_send(modbus_tcp_server_t *server, const uint8_t *req, int req_length)
{
    if (server == NULL || server->server_ctx.tcps.tls_ctx == NULL)
    {
        ESP_LOGE(TAG, "tcps server send error, server is NULL.");
        return -1;
    }

    int ret = 0;
    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;

    while ((ret = mbedtls_ssl_write(&ctx->ssl, req, req_length)) <= 0)
    {
        if (ret == MBEDTLS_ERR_NET_CONN_RESET)
        {
            ESP_LOGE(TAG, "peer closed the connection");
           return ret;
        }

        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned %d", ret);
            return ret;
        }
    }

    return ret;
}

/**
 * @brief Receive data from the connected client
 * @param server modbus tcp server context
 * @param rsp buffer to store the received data
 * @param rsp_length length of the buffer
 * @return >=0: number of bytes received, <0: error code
 */
ssize_t tcps_server_recv(modbus_tcp_server_t *server, uint8_t *rsp, int rsp_length)
{
    if (server == NULL || server->server_ctx.tcps.tls_ctx == NULL)
    {
        ESP_LOGE(TAG, "tcps server send error, server is NULL.");
        return -1;
    }

    int ret = 0;
    // int retry_count = 0;
    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;
    do
    {
        ret = mbedtls_ssl_read(&ctx->ssl, rsp, rsp_length);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            // ESP_LOGW(TAG, "mbedtls_ssl_read want read or write, continue. ret:%d", ret);
            // retry_count++;
            // if (retry_count >= 30)
            // {
            //     ESP_LOGE(TAG, "mbedtls_ssl_read want read or write too many times, break.");
            //     return 0; // 超过30次还没有数据, 认为是错误
            // }
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (ret <= 0)
        {
            ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", (unsigned int) -ret);
            return ret;
        }

        break;
    } while (1);

    ESP_LOGI(TAG, "tcps server recv len: %d", ret);

    return ret;
}

/**
 * @brief Check if the TCP server socket is ready to read data
 * @note 这里只能判断tcp层是否有数据, 无法判断SSL层是否有数据. 比如握手包也是TCP数据
 * @param server modbus tcp server context
 * @param tv 超时时间
 * @return >0: 有数据可读取, 0: 超时, <0: 错误码
 */
#if 0
int tcps_server_read_ready(modbus_tcp_server_t *server, struct timeval *tv)
{
    if(server == NULL || server->server_ctx.tcps.tls_ctx == NULL)
    {
        ESP_LOGE(TAG, "tcps server read ready error, server is NULL.");
        return -1;
    }

    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;
    
}
#else
int tcps_server_read_ready(modbus_tcp_server_t *server, struct timeval *tv)
{
    if(server == NULL || server->server_ctx.tcps.tls_ctx == NULL)
    {
        ESP_LOGE(TAG, "tcps server read ready error, server is NULL.");
        return -1;
    }
#if 0
    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;
    int type = MBEDTLS_NET_POLL_READ;
    return mbedtls_net_poll(&(ctx->client_fd), type, 0);
#else
    /* 使用 mbedtls_net_poll, 编译报错: undefined reference to `mbedtls_net_poll'
       可能是当前编译环境的mbedtls版本不支持 mbedtls_net_poll(), 这里先使用select代替.*/
    fd_set rset;
    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;
    int socket_fd = ctx->client_fd.fd;

    // ESP_LOGI(TAG, "tcps server read ready, socket fd: %d", socket_fd);
    /* Add a file descriptor to the set */
    FD_ZERO(&rset);
    FD_SET(socket_fd, &rset);

    int s_rc;
    while ((s_rc = select(socket_fd + 1, &rset, NULL, NULL, tv)) == -1)
    {
        if (errno == EINTR)
        {
            ESP_LOGE(TAG, "A non blocked signal was caught\n");
            /* Necessary after an error */
            FD_ZERO(&rset);
            FD_SET(socket_fd, &rset);
        }
        else
        {
            ESP_LOGE(TAG, "select -1");
            return -1;
        }
    }
    // ESP_LOGI(TAG, "tcps server read ready, select return: %d", s_rc);

    return s_rc;
#endif
    // uint32_t timeout = 0;
    // if (tv)
    // {
    //     timeout = tv->tv_sec * 1000 + tv->tv_usec / 1000; // Convert to milliseconds
    // }
    // int type = MBEDTLS_NET_POLL_READ;
    // return mbedtls_net_poll(&(ctx->client_fd), type, 0);
}
#endif

/**
 * @brief Check if the TCP server is connected to a client
 * @param server modbus tcp server context
 * @return true: connected, false: not connected or error
 */
bool tcps_server_is_connected(modbus_tcp_server_t *server)
{
    if (server == NULL)
    {
        ESP_LOGE(TAG, "tcp server connected error.");
        return false;
    }

    return (server->server_ctx.tcps.tls_ctx == NULL) ? false:true;
}

/**
 * @brief Close the TCP server socket and client connection
 * @param server modbus tcp server context
 * @return void
 */
void tcps_server_close(modbus_tcp_server_t *server)
{
    if (server == NULL || server->server_ctx.tcps.tls_ctx == NULL)
    {
        ESP_LOGE(TAG, "tcps server close error, server is NULL.");
        return;
    }

    int ret = 0;
    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;

    mbedtls_ssl_session_reset(&ctx->ssl);

    mbedtls_net_free(&ctx->client_fd);
    mbedtls_net_free(&ctx->listen_fd);

    mbedtls_x509_crt_free(&ctx->srvcert);
    mbedtls_pk_free(&ctx->pkey);
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_free(&ctx->cache);
#endif
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    mbedtls_psa_crypto_free();
#endif /* MBEDTLS_USE_PSA_CRYPTO */

    if (ctx)
    {
        free(ctx);
        ctx = NULL;
    }

    ESP_LOGI(TAG, "tcps server closed.");
}

/**
 * @brief Reset the TCP client connection
 * @param server modbus tcp server context
 * @return void
 */
void tcps_server_reset(modbus_tcp_server_t *server)
{
    if (server == NULL || server->server_ctx.tcps.tls_ctx == NULL)
    {
        ESP_LOGE(TAG, "tcps server close error, server is NULL.");
        return;
    }

    tcps_server_ctx_t *ctx = (tcps_server_ctx_t *) server->server_ctx.tcps.tls_ctx;

    mbedtls_net_free(&ctx->client_fd);
    mbedtls_ssl_session_reset(&ctx->ssl);

    ESP_LOGI(TAG, "tcps client reset.");
}
