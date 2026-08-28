#include "uart_device_process.h"
#include "iot_period_task.h"
#include "webserver.h"
#include "can_protocol.h"
#include "dev_discovery.h"
#include "iot_partition.h"
#include "iot_ota.h"
#include "modbus_slave.h"
#include "modbus_protocol.h"
#include "json_cmd.h"
#include "http_client.h"
#include "iot_wifi_init.h"

#include "esp_wifi_types.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "mbedtls/ctr_drbg.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include <sys/types.h>

#define TAG "[WebServer]"

#define MD_SLAVE_ADDR_SUM       (0) //汇总从机地址
#define CERT_PARTITION_SIZE     4096

typedef int16_t (*ws_svr_msg_handler)(cJSON *cjson_object, uint8_t **resp_data);

typedef struct 
{
    char *data;
    int len;
    int client_socket;
}ws_msg_t;

typedef struct _ws_svr_msg_t
{
    uint8_t msg_type[16];
    ws_svr_msg_handler handler;
}ws_svr_msg_t;

#define WS_CLIENT_QUANTITY_ASTRICT  5    //客户端数量, 主要用于服务端主动发送数据
#define WS_SERVER_MSG_MAX           15

#define WS_SVR_UPGRADE_RESP_TYPE   "upgradeRsp"

static httpd_handle_t web_server_handle = NULL;//ws服务器唯一句柄
static QueueHandle_t  ws_server_rece_queue = NULL;//收到的消息传给任务处理
// static QueueHandle_t  ws_server_send_queue = NULL;//异步发送队列

/*此处只是管理ws socket server发送时的对象，以确保多客户端连接的时候都能收到数据，并不能限制HTTP请求*/
static int ws_client_list[WS_CLIENT_QUANTITY_ASTRICT];//客户端套接字列表
static int ws_client_num = 0;   //实际连接数量

EXT_RAM_BSS_ATTR ws_svr_msg_t ws_server_msg[WS_SERVER_MSG_MAX];
#define reg_addr_rd_total Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)]

int hex_str_to_bytes(const char *hex_str, uint8_t *byte_array, size_t array_size)
{
    if (!hex_str || !byte_array || array_size == 0) {
        return -1;
    }

    size_t hex_len = strlen((char *)hex_str);
    size_t byte_count = 0;

    // 处理可能的0x/0X前缀
    size_t start_index = 0;
    if (hex_len >= 2 && hex_str[0] == '0' && 
        (hex_str[1] == 'x' || hex_str[1] == 'X'))
    {
        start_index = 2;
    }

    // 遍历十六进制字符串
    for (size_t i = start_index; i < hex_len; )
    {
        // 确保数组有足够空间
        if (byte_count >= array_size) {
            return -2;  // 数组空间不足
        }

        // 跳过空格和分隔符
        if (isspace((unsigned char)hex_str[i]) || hex_str[i] == '-' || hex_str[i] == ':') {
            i++;
            continue;
        }

        // 检查是否还有两个字符可用
        if (i + 1 >= hex_len) {
            return -3;  // 奇数个十六进制字符
        }

        // 转换两个字符为一个字节
        unsigned char high_nibble = hex_str[i];
        unsigned char low_nibble = hex_str[i + 1];

        // 验证字符有效性并转换为数值
        int valid = 1;
        unsigned char byte = 0;

        // 处理高4位
        if (high_nibble >= '0' && high_nibble <= '9') {
            byte = (high_nibble - '0') << 4;
        } else if (high_nibble >= 'A' && high_nibble <= 'F') {
            byte = (high_nibble - 'A' + 10) << 4;
        } else if (high_nibble >= 'a' && high_nibble <= 'f') {
            byte = (high_nibble - 'a' + 10) << 4;
        } else {
            valid = 0;
        }
        
        // 处理低4位
        if (low_nibble >= '0' && low_nibble <= '9') {
            byte |= low_nibble - '0';
        } else if (low_nibble >= 'A' && low_nibble <= 'F') {
            byte |= low_nibble - 'A' + 10;
        } else if (low_nibble >= 'a' && low_nibble <= 'f') {
            byte |= low_nibble - 'a' + 10;
        } else {
            valid = 0;
        }
        
        if (!valid) {
            return -4;  // 无效十六进制字符
        }
        
        // 保存到数组并更新索引
        byte_array[byte_count++] = byte;
        i += 2;  // 移动到下一对字符
    }
    
    return byte_count;  // 返回转换的字节数
}

char* hex_array_to_string(const uint8_t *hex_array, size_t len)
{
    // 分配内存: 每个字节需2字符 + 终止符
    char *str = (char*)heap_caps_malloc(len * 2 + 1, MALLOC_CAP_SPIRAM);
    if (str == NULL)
    {
        ESP_LOGE(TAG, "HEX array to string malloc error");
        return NULL;
    }

    const char hex_chars[] = "0123456789ABCDEF"; // 映射表

    for (size_t i = 0; i < len; i++) {
        // 处理高4位
        str[i * 2] = hex_chars[hex_array[i] >> 4];
        // 处理低4位
        str[i * 2 + 1] = hex_chars[hex_array[i] & 0x0F];
    }
    str[len * 2] = '\0'; // 终止字符串
    return str;
}

/**
 * @brief 添加客户端套接字到ws客户端列表
 * @param socket 客户端套接字
 * @return void
 */
static void ws_client_list_add(int socket)
{
    /*检查是否超出限制*/
    if (ws_client_num>=WS_CLIENT_QUANTITY_ASTRICT)
    {
        return;
    }

    /*检查是否重复*/
    for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++) 
    {
        if (ws_client_list[i] == socket) {
            return;
        } 
    }

    /*添加套接字至列表中*/
    for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++) 
    {
        if (ws_client_list[i] <= 0){
            ws_client_list[i] = socket; //获取返回信息的客户端套接字
            ESP_LOGI(TAG, "ws_client_list_add:%d\r\n",socket);
            ws_client_num++;
            return;
        }
    }
}

/**
 * @brief 从ws客户端列表中删除套接字
 * @param socket 客户端套接字
 * @return void
 */
static void ws_client_list_delete(int socket)
{
    for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++)
    {
        if (ws_client_list[i] == socket)
        {
            ws_client_list[i] = 0;
            ESP_LOGI(TAG, "ws_client_list_delete:%d",socket);
            ws_client_num--;
            if (ws_client_num < 0)
            {
                ws_client_num = 0;
            }
            break;
        }
    }
}

/**
 * @brief 在内存中查找子字符串, 可用于查找二进制数据
 * @param haystack 主字符串
 * @param haystack_len 主字符串长度
 * @param needle 子字符串
 * @param needle_len 子字符串长度
 * @return 返回子字符串在主字符串中的位置，如果未找到则返回NULL
 */
const char* memfind(const char* haystack, size_t haystack_len, 
                            const char* needle, size_t needle_len) {
    if (needle_len == 0) return haystack;
    if (haystack_len < needle_len)
    {
        ESP_LOGW(TAG, "memfind haystack_len: %u < needle_len: %u", haystack_len, needle_len);
        return NULL;
    }
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return haystack + i;
        }
    }

    return NULL;
}

/**
 * @brief 解析multipart/form-data数据
 * @param req httpd_req_t 请求对象
 * @param file_content 输出文件内容指针
 * @param file_len 输出文件长度
 * @param total_chunks 输出总分块数
 * @return >=0: 当前分片序号; <0: 错误
 */
static int16_t ws_server_multipart_parse(httpd_req_t *req, char **file_content, int *file_len, int *total_chunks)
{
    if (req == NULL)
    {
        ESP_LOGE(TAG, "ws server multipart parse req is NULL");
        return -1;
    }

    int chunk_index = 0;
    int file_size  = 0;
    char *content_type = NULL;

    size_t content_type_len = httpd_req_get_hdr_value_len(req, "Content-Type") + 1;
    if (content_type_len > 1) {
        content_type = (char*)heap_caps_malloc(content_type_len, MALLOC_CAP_SPIRAM);
        if (!content_type)
        {
            ESP_LOGI(TAG, "Content-Type malloc error");
            return -6;
        }

        if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, content_type_len) == ESP_OK) {
            ESP_LOGI(TAG, "Content-Type: %s", content_type);
        }
    }

    /* 检查是否为 multipart/form-data, 并且提取boundary信息
     multipart/form-data 使用boundary=后面的字符串分割数据*/
    const char *boundary_str = "boundary=";
    char *boundary = NULL;
    if (content_type && strstr(content_type, "multipart/form-data") != NULL) {
        char *b = strstr(content_type, boundary_str);
        if (b) {
            b += strlen(boundary_str);
            boundary = strdup(b);
        }
    }

    if (content_type)
    {
        free(content_type);
        content_type = NULL;
    }

    if (!boundary) {
        ESP_LOGE(TAG, "No boundary found in Content-Type");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "No boundary", HTTPD_RESP_USE_STRLEN);
        return -2;
    }

    // 读取body
    char *buf = (char*)heap_caps_malloc(req->content_len + 2, MALLOC_CAP_SPIRAM);
    if (!buf) {
        free(boundary);
        ESP_LOGE(TAG, "malloc failed");
        httpd_resp_set_status(req, "500 Internal Error");
        httpd_resp_send(req, "Server error", HTTPD_RESP_USE_STRLEN);
        return -3;
    }

    /* 解决分包问题 */
    int ret = 0;
    int remaining = req->content_len;
    char *data = buf;
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, data, remaining)) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            free(buf);
            free(boundary);
            ESP_LOGE(TAG, "httpd_req_recv failed");
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_send(req, "Receive failed", HTTPD_RESP_USE_STRLEN);
            return -4;
        }

        ESP_LOGI(TAG, "receive data len:%d remaining:%d", ret, remaining);
        remaining -= ret;
        data += ret;
    }
    buf[req->content_len] = 0;

    char *boundary_full = (char*)heap_caps_malloc(content_type_len + 2, MALLOC_CAP_SPIRAM);
    if (!boundary_full)
    {
        free(buf);
        free(boundary);
        ESP_LOGE(TAG, "boundary full malloc error");
        return -7;
    }

    snprintf(boundary_full, content_type_len + 2, "--%s", boundary);
    char *part = strstr(buf, boundary_full);
    while (part) {
        part += strlen(boundary_full);
        if (strncmp(part, "--", 2) == 0) break; // 结束
        char *header_end = strstr(part, "\r\n\r\n");
        if (!header_end)
        {
            ESP_LOGW(TAG, "multipart parse header_end error");
            break;
        }
        header_end += 4;

        char *next_boundary = memfind(header_end, req->content_len - (header_end - buf),
                                        boundary_full, strlen(boundary_full));
        if (!next_boundary)
        {
            ESP_LOGW(TAG, "multipart parse next_boundary error");
            break;
        }

        // 解析头部
        char *name_pos = strstr(part, "name=\"");
        if (name_pos && name_pos < header_end) {
            name_pos += 6;
            char *name_end = strchr(name_pos, '"');
            if (name_end && name_end < header_end) {
                char field_name[64] = {0};
                memcpy(field_name, name_pos, name_end - name_pos);

                // 取出内容
                size_t value_len = next_boundary - header_end - 2; // 去掉 \r\n
                char *value = (char*)heap_caps_malloc(value_len + 1, MALLOC_CAP_SPIRAM);
                if (value) {
                    memcpy(value, header_end, value_len);
                    value[value_len] = 0;
                    // 文件内容
                    if (0 == strcmp(field_name, "file"))
                    {
                        // ESP_LOGW(TAG, "Received file data len:%d", value_len);
                        // ESP_LOG_BUFFER_HEX(TAG, value, value_len);
                        *file_len = value_len;
                        *file_content = value;
                    }
                    else
                    {
                        char *endptr;
                        if(0 == strcmp(field_name, "chunkIndex"))
                        {
                            chunk_index = strtol(value, &endptr, 10);
                        }
                        else if(0 == strcmp(field_name, "totalChunks"))
                        {
                            *total_chunks = strtol(value, &endptr, 10);
                        }
                        else if(0 == strcmp(field_name, "fileSize"))
                        {
                            file_size = strtol(value, &endptr, 10);
                        }

                        free(value);
                        value = NULL;
                    }
                }
            }
        }
        part = next_boundary;
    }

    free(boundary_full);
    free(buf);
    free(boundary);

    if(chunk_index != 0 && *file_content == NULL)
    {
        return -5;
    }

    return chunk_index;
}

/**
 * @brief 证书完整性检查, 可正常解析即为完整
 * @param[in] file_type 证书类型: 0 - 私钥文件; 1 - 证书文件.
 * @param[in] buffer 文件内容
 * @param[in] buffer_len 文件长度
 * @return 0 - 失败; 1 - 私钥校验成功; 2 - 证书校验成功.
 */
static int16_t cert_integrity_check(int16_t file_type, char *buffer, int buffer_len)
{
    int16_t success = 0;
    int ret = 0;

    if(file_type == 0)
    {
        // 校验私钥
        mbedtls_pk_context pk;
        mbedtls_pk_init(&pk);
        mbedtls_ctr_drbg_context *ctr_drbg_ptr = (mbedtls_ctr_drbg_context *)heap_caps_malloc(sizeof(mbedtls_ctr_drbg_context)+1, MALLOC_CAP_SPIRAM);
        ret = mbedtls_pk_parse_key(&pk, (const unsigned char *)buffer, buffer_len + 1, NULL, 0, mbedtls_ctr_drbg_random, ctr_drbg_ptr);
        if(ret == 0)
        {
            ESP_LOGI(TAG,"PRIVATE KEY check finished (mbedtls)");
            success = 1;
        }
        else
        {
            ESP_LOGE(TAG,"PRIVATE KEY check wrong (mbedtls), ret = -0x%04X", -ret);
            success = 0;
        }
        mbedtls_pk_free(&pk);
    }
    else
    {
        // 校验证书
        mbedtls_x509_crt crt;

        mbedtls_x509_crt_init(&crt);
        ret = mbedtls_x509_crt_parse(&crt, (const unsigned char *)buffer, buffer_len + 1);
        if(ret == 0)
        {
            ESP_LOGI(TAG,"CERTIFICATE check finished (mbedtls)");
            success = 2;
        }
        else
        {
            ESP_LOGE(TAG,"CERTIFICATE check wrong (mbedtls), ret = -0x%04X", -ret);
            success = 0;
        }
        mbedtls_x509_crt_free(&crt);
    }
    return success;
}

/*HTML GET处理程序 */
static esp_err_t home_get_handler(httpd_req_t *req)
{
    /*获取脚本server.html的存放地址和大小，接受http请求时将脚本发出去*/
    extern const unsigned char upload_script_start[] asm("_binary_webserver_html_start");/*server.html文件在bin中的位置*/
    extern const unsigned char upload_script_end[]   asm("_binary_webserver_html_end");
    const size_t upload_script_size = (size_t)((uintptr_t)upload_script_end - (uintptr_t)upload_script_start);
    httpd_resp_send(req, (const char *)upload_script_start, upload_script_size);
    return ESP_OK;
}

/*HTML*/
static const httpd_uri_t home = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = home_get_handler,
    .user_ctx  = NULL
};

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // 检查错误代码是否为404
    if (err == HTTPD_404_NOT_FOUND) {
        // 设置响应头为HTML类型
        httpd_resp_set_type(req, "text/html");
        // 自定义的404 HTML页面内容
        // const char* html_404 = "<!DOCTYPE html><html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>The page you requested was not found.</p></body></html>";
        const char *html_404 = 
        "<!DOCTYPE html> \
        <html> \
        <head> \
            <title>404 Not Found</title> \
            <style> \
                body {  \
                    font-family: Arial, sans-serif;  \
                    background-color: #f0f0f0; \
                    text-align: center; \
                    padding-top: 50px; \
                } \
                .container { \
                    background-color: white; \
                    border-radius: 10px; \
                    padding: 30px; \
                    margin: 0 auto; \
                    width: 80%; \
                    max-width: 600px; \
                    box-shadow: 0 0 10px rgba(0,0,0,0.1); \
                } \
                h1 { color: #d32f2f; } \
                a { color: #1976d2; text-decoration: none; } \
            </style> \
        </head> \
        <body> \
            <div class=\"container\"> \
                <h1>404 - Page Not Found</h1> \
                <p>The requested URL was not found on this server.</p> \
                <p>Return to <a href=\"/\">Home Page</a></p> \
            </div> \
        </body> \
        </html>";
        // 发送HTML响应
        httpd_resp_send(req, html_404, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // 对于其他错误，使用默认处理
    return ESP_FAIL;
}

/**
 * @brief  WebSocket服务器接收数据
 * @param  req httpd_req_t类型的请求
 * @return ESP_OK表示成功，其他值表示失败
 */
static esp_err_t ws_server_recv_data(httpd_req_t *req)
{
    if (req == NULL)
    {
        ESP_LOGE(TAG, "ws server recv data req is NULL");
        return ESP_FAIL;
    }

    if (req->method == HTTP_GET)
    {
        ws_client_list_add(httpd_req_to_sockfd(req));
        ESP_LOGI(TAG, "ws server recv data method is http GET.");
        return ESP_OK;
    }

    ws_msg_t ws_recv_msg = {NULL, 0, 0};
    esp_err_t ret = ESP_FAIL;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    //获取ws帧长度
    ws_pkt.payload = NULL;
    ret = httpd_ws_recv_frame(req, &ws_pkt, 0);//设置参数max_len = 0来获取帧长度, 实际不读数据
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ws server recv data data len receiving failure!");
        return ret;
    }

    if (ws_pkt.len <= 0)
    {
        ESP_LOGE(TAG, "ws_pkt len error:%d",ws_pkt.len);
        return ESP_FAIL;
    }

    ws_recv_msg.data = (char*)heap_caps_malloc(ws_pkt.len + 1, MALLOC_CAP_SPIRAM); //分配内存
    // ws_recv_msg.data = (char *)calloc(ws_pkt.len + 1, 1);
    if (ws_recv_msg.data == NULL)
    {
        ESP_LOGE(TAG, "ws server recv data malloc failure!");
        return ESP_FAIL;
    }

    memset(ws_recv_msg.data, 0x0, ws_pkt.len + 1);
    ws_pkt.payload = (uint8_t*)ws_recv_msg.data;   //指向缓存区
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK)
    {
        free(ws_recv_msg.data);
        ESP_LOGE(TAG, "ws server recv data data receiving failure!");
        return ret;
    }

    ws_recv_msg.len = ws_pkt.len;
    ws_recv_msg.client_socket = httpd_req_to_sockfd(req);
    if (ws_server_rece_queue && xQueueSend(ws_server_rece_queue, &ws_recv_msg, pdMS_TO_TICKS(1)) == pdPASS)
    {
        ret = ESP_OK;
    }
    else
    {
        free(ws_recv_msg.data);
    }

    return ret;
}

/*websocket, 所有websocket数据均使用此URI, 内部通过消息类型字段区分*/
static const httpd_uri_t ws = {
    .uri        = "/ws",
    .method     = HTTP_GET,
    .handler    = ws_server_recv_data,
    .user_ctx   = NULL,
    .is_websocket = true
};

#if CONFIG_LOCAL_UPGRADE_SUPPORT
/**
 * @brief WebSocket服务器升级处理函数
 * @details 处理升级请求，解析multipart/form-data格式的数据
 * @param req httpd_req_t类型的请求
 * @return ESP_OK表示成功，其他值表示失败
 */
static esp_err_t ws_server_iot_upgrade(httpd_req_t *req)
{
    if (req == NULL)
    {
        ESP_LOGE(TAG, "ws_server_iot_upgrade req is NULL");
        return ESP_FAIL;
    }

    char *file_content = NULL;
    int file_len = 0;
    int total_chunks = 0;
    static int current_chunk = 0;

    int16_t ret = ws_server_multipart_parse(req, &file_content, &file_len, &total_chunks);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "parse multipart error:%d", ret);
        return ESP_FAIL;
    }

    if (file_content == NULL)
    {
        ESP_LOGE(TAG, "parse multipart file content is null");

        httpd_resp_set_status(req, "200 Success");
        httpd_resp_send(req, "FormData parsed", HTTPD_RESP_USE_STRLEN);

        return ESP_OK;
    }

    // 第一个包
    if (ret == 0)
    {
        //TODO: 版本校验

        current_chunk = 0;
        ESP_LOGE(TAG, "iot ota begin");
        if (iot_ota_begin() != ESP_OK)
        {
            ESP_LOGE(TAG, "iot ota begin error");
            free(file_content);
            file_content = NULL;
            return ESP_FAIL;
        }
    }

    if (ret != current_chunk)
    {
        free(file_content);
        file_content = NULL;
        ESP_LOGE(TAG, "iot image pack req error:%d != %d", ret, current_chunk);
        httpd_resp_set_status(req, "402 chunk seq error");
        httpd_resp_send(req, "Check chunk seq error", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    if (iot_ota_write((uint8_t *)file_content, file_len) != ESP_OK)
    {
        current_chunk = 0;
        free(file_content);
        file_content = NULL;
        httpd_resp_set_status(req, "501 OTA write error");
        httpd_resp_send(req, "OTA IoT Write Error!", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    else
    {
        current_chunk++;
    }

    free(file_content);
    file_content = NULL;
    //TODO：超时处理

    //升级结束
    if (ret == total_chunks -1)
    {
        if (iot_ota_end() != ESP_OK)
        {
            ESP_LOGE(TAG, "IoT end error!");
            httpd_resp_set_status(req, "502 OTA end error");
            httpd_resp_send(req, "OTA IoT End Error!", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }

        httpd_resp_set_status(req, "200 Upgrade Success");
        httpd_resp_send(req, "OTA success", HTTPD_RESP_USE_STRLEN);

        ESP_LOGI(TAG, "webserver OTA success, will restart system!");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    httpd_resp_set_status(req, "200 Success");
    httpd_resp_send(req, "OTA upgrading", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* IoT固件更新 */
static const httpd_uri_t upgrade_iot = {
    .uri        = "/upgrade/iot",
    .method     = HTTP_POST,
    .handler    = ws_server_iot_upgrade,
    .user_ctx   = NULL,
    .is_websocket = true
};
#endif

#if ENCRYPT_CERT_USE_FILE_SYSTEM
void ws_server_cert_save(uint8_t file_area, uint8_t *file_content, uint32_t file_len)
{
    write_cert_to_file(file_area, file_content, file_len);
}
#else
void ws_server_cert_save(uint file_area, uint8_t *file_content, uint32_t file_len)
{
    /* 先存到缓存区 */
    partition_reinit(UPDATE_AREA, CERT_PARTITION_SIZE);
    partition_write_encrypt((uint8_t*)file_content, file_len, 0, UPDATE_AREA);

    /* 从缓存区读取 */
    memset(file_content, 0, file_len);
    uint32_t len = get_partition_plaintext_len(UPDATE_AREA); //获取长度
    partition_read_decrypt((uint8_t*)file_content, len, 0, UPDATE_AREA);

    /* 写入目的区 */
    partition_reinit(file_area, CERT_PARTITION_SIZE);
    partition_write_encrypt((uint8_t*)file_content, file_len, 0, file_area);//覆盖原私钥区
}
#endif

/**
 * @brief WebSocket服务器更新CA证书消息处理
 * @details 处理升级请求，解析multipart/form-data格式的数据
 * @param req httpd_req_t类型的请求
 * @return ESP_OK表示成功，其他值表示失败
 */
static esp_err_t ws_server_update_ca_cert_file(httpd_req_t *req)
{
    if (req == NULL)
    {
        ESP_LOGE(TAG, "ws_server_update_ca_cert_file req is NULL");
        return ESP_FAIL;
    }

    char *file_content = NULL;
    int file_len = 0;
    int total_chunks = 0;
    int16_t ret = ws_server_multipart_parse(req, &file_content, &file_len, &total_chunks);
    if (ret < 0 || file_content == NULL)
    {
        ESP_LOGE(TAG, "parse multipart error:%d", ret);
        return ESP_FAIL;
    }

    //校验证书是否完整
    ret = cert_integrity_check(1, file_content, file_len);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "CA cert file error!");
        httpd_resp_set_status(req, "401 File Error");
        httpd_resp_send(req, "File error", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ws_server_cert_save(MD_TCP_SERVER_CA_AREA, (uint8_t*)file_content, file_len);

    free(file_content);
    file_content = NULL;

    // ESP_LOGW(TAG, "update ca cert file: ");
    // print_partition_info(CA_CERTIFICATE_AREA, NORMAL_INFO);

    httpd_resp_set_status(req, "200 Success");
    httpd_resp_send(req, "FormData parsed", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* TLS文件 */
static const httpd_uri_t ca_cert_file = {
    .uri        = "/tlscert/ca_cert",
    .method     = HTTP_POST,
    .handler    = ws_server_update_ca_cert_file,
    .user_ctx   = NULL,
    .is_websocket = true
};

/**
 * @brief WebSocket服务器更新客户端证书消息处理
 * @details 处理升级请求，解析multipart/form-data格式的数据
 * @param req httpd_req_t类型的请求
 * @return ESP_OK表示成功，其他值表示失败
 */
static esp_err_t ws_server_update_server_cert_file(httpd_req_t *req)
{
    if (req == NULL)
    {
        ESP_LOGE(TAG, "ws_server_update_client_cert_file req is NULL");
        return ESP_FAIL;
    }

    char *file_content = NULL;
    int file_len = 0;
    int total_chunks = 0;
    int16_t ret = ws_server_multipart_parse(req, &file_content, &file_len, &total_chunks);
    if (ret < 0 || file_content == NULL)
    {
        ESP_LOGE(TAG, "parse multipart error:%d", ret);
        return ESP_FAIL;
    }

    //校验证书是否完整
    ret = cert_integrity_check(1, file_content, file_len);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Client cert file error!");
        httpd_resp_set_status(req, "401 File Error");
        httpd_resp_send(req, "File error", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ws_server_cert_save(MD_TCP_SERVER_CERT_AREA, (uint8_t*)file_content, file_len);

    free(file_content);
    file_content = NULL;

    // ESP_LOGW(TAG, "update client cert file: ");
    // print_partition_info(IOT_CERTIFICATE_AREA, NORMAL_INFO);

    httpd_resp_set_status(req, "200 Success");
    httpd_resp_send(req, "FormData parsed", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* TLS文件 */
static const httpd_uri_t server_cert_file = {
    .uri        = "/tlscert/server_cert",
    .method     = HTTP_POST,
    .handler    = ws_server_update_server_cert_file,
    .user_ctx   = NULL,
    .is_websocket = true
};

/**
 * @brief WebSocket服务器更新客户端秘钥消息处理
 * @details 处理升级请求，解析multipart/form-data格式的数据
 * @param req httpd_req_t类型的请求
 * @return ESP_OK表示成功，其他值表示失败
 */
static esp_err_t ws_server_update_server_key_file(httpd_req_t *req)
{
    if (req == NULL)
    {
        ESP_LOGE(TAG, "ws_server_update_client_key_file req is NULL");
        return ESP_FAIL;
    }

    char *file_content = NULL;
    int file_len = 0;
    int total_chunks = 0;
    int16_t ret = ws_server_multipart_parse(req, &file_content, &file_len, &total_chunks);
    if (ret < 0 || file_content == NULL)
    {
        ESP_LOGE(TAG, "parse multipart error:%d", ret);
        return ESP_FAIL;
    }

    //校验证书是否完整: 这里采用强校验, 可正确解析才可以.
    ret = cert_integrity_check(0, file_content, file_len);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Client key file error!");
        httpd_resp_set_status(req, "401 File Error");
        httpd_resp_send(req, "File error", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ws_server_cert_save(MD_TCP_SERVER_PRIVATE_AREA, (uint8_t*)file_content, file_len);

    httpd_resp_set_status(req, "200 Success");
    httpd_resp_send(req, "FormData parsed", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* TLS文件 */
static const httpd_uri_t server_key_file = {
    .uri        = "/tlscert/server_key",
    .method     = HTTP_POST,
    .handler    = ws_server_update_server_key_file,
    .user_ctx   = NULL,
    .is_websocket = true
};

/*http事件处理*/
static void ws_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTP_SERVER_EVENT )
    {
        switch (event_id)
        {
            case HTTP_SERVER_EVENT_DISCONNECTED  ://连接已断开
                esp_http_server_event_data* event = (esp_http_server_event_data*)event_data;
                ws_client_list_delete(event->fd);
                break;

            default:
                // ESP_LOGW(TAG, "Unhandled HTTP server event: %lu", event_id);
                break;
        }
    }
}

#if 1
void ws_send_async_cb(esp_err_t err, int socket, void *arg)
{
    if (ESP_OK == err)
    {
        ESP_LOGI(TAG, "ws server send data success, socket:%d", socket);
    }
    else
    {
        ESP_LOGE(TAG, "ws server:%d send data error %d, mean:%s", socket, err, esp_err_to_name(err));
    }
}

static void ws_server_send_async(const uint8_t *data ,uint32_t len , int client_socket)
{
    if (NULL == data || 0 == len || 0 >= client_socket)
    {
        ESP_LOGE(TAG, "ws server send data argument invalid!");
        return;
    }

    httpd_ws_frame_t ws_pkt ={0};
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // 改为同步发送，使用异步发送在频发数据时，会导致数据包错误，比如升级进度频繁上报
    // httpd_ws_send_data_async(web_server_handle, client_socket, &ws_pkt, NULL, NULL);
    httpd_ws_send_data(web_server_handle, client_socket, &ws_pkt);
}
#else
/*异步发送函数，将其放入HTTPD工作队列*/
static void ws_async_send(void *arg)
{
    ws_msg_t async_buffer = {NULL, 0, 0};
    if (xQueueReceive(ws_server_send_queue, &async_buffer,0))
    {
        httpd_ws_frame_t ws_pkt ={0};
        ws_pkt.payload = (uint8_t*)async_buffer.data;
        ws_pkt.len = async_buffer.len;
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        httpd_ws_send_frame_async(web_server_handle, async_buffer.client_socket, &ws_pkt);
        // httpd_ws_send_frame(web_server_handle, async_buffer.client_socket, &ws_pkt);
    }

    if (async_buffer.data != NULL)
    {
        free(async_buffer.data);
        async_buffer.data = NULL;
    }
}

/*ws 发送函数*/
static void ws_server_send(const uint8_t *data ,uint32_t len , int client_socket)
{
    if (data == NULL || len = 0 || client_socket <= 0)
    {
        ESP_LOGE(TAG, "ws server send data argument invalid!");
        return;
    }

    ws_msg_t send_buffer = {NULL, 0, 0};
    memset(&send_buffer, 0, sizeof(send_buffer));
    send_buffer.client_socket = client_socket;
    send_buffer.len = len;

    send_buffer.data = (char*)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM); //分配内存
    if (send_buffer.data == NULL)
    {
        ESP_LOGE(TAG, "ws_server_send malloc failure!");
        return;
    }

    memcpy(send_buffer.data, (char *)data, len);

    xQueueSend(ws_server_send_queue ,&send_buffer,pdMS_TO_TICKS(1));
    httpd_queue_work(web_server_handle, ws_async_send, NULL);//进入排队
}
#endif


/**
 * @brief 填充响应消息格式
 * @param msg_type 消息类型
 * @param err_code 错误码, 浏览器根据err_code判断成功或失败
 *  . 0: 成功
 *  . >0: 失败, 错误码
 * @param msg 响应消息, err_code不为0时会显示到UI弹窗
 * @param resp_data 响应数据
 * @return 响应数据长度
 * @note resp_data 需要调用者释放
 */
int16_t ws_server_msg_response(char *msg_type, uint16_t err_code, char *msg, uint8_t **resp_data)
{
    cJSON *cjson_resp = cJSON_CreateObject();
    if (cjson_resp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_resp is NULL");
        return -2;
    }

    cJSON_AddItemToObject(cjson_resp, "type", cJSON_CreateString(msg_type));

    cJSON *cjson_result = cJSON_CreateObject();
    if (cjson_result == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_result is NULL");
        return -2;
    }
    cJSON_AddItemToObject(cjson_result, "errCode", cJSON_CreateNumber(err_code));
    cJSON_AddItemToObject(cjson_result, "message", cJSON_CreateString(msg));

    cJSON_AddItemToObject(cjson_resp, "result", cjson_result);
    *resp_data = (uint8_t *)cJSON_PrintUnformatted(cjson_resp);
    cJSON_Delete(cjson_resp);

    return strlen((char *)*resp_data);
}

/**
 * @brief 处理Wi-Fi Sta设置消息 wifi_sta_set
 * @param[in] cjson_object 待处理cJSON对象消息
 * @param[out] resp_data 响应数据
 * @return 响应数据长度
 */
int16_t ws_svr_msg_wifi_sta1_set_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_WIFI_STA1_SET_RESP_TYPE   "setWiFiSta1Rsp"

    bool enable = false;
    char *ssid = NULL;
    char *pwd = NULL;
    int authmode = 0;
    bool static_ip_en = false;
    char *ip = NULL;
    char *mask = NULL;
    char *gw = NULL;
    char *dns1 = NULL;
    char *dns2 = NULL;
    uint32_t addr_int = 0;
    int ret = 0;

    cJSON *cjson_en = cJSON_GetObjectItem(cjson_object, "enable");
    cJSON *cjson_ssid = cJSON_GetObjectItem(cjson_object, "ssid");
    cJSON *cjson_pwd = cJSON_GetObjectItem(cjson_object, "password");
    cJSON *cjson_auth = cJSON_GetObjectItem(cjson_object, "authmode");
    cJSON *cjson_ip_en = cJSON_GetObjectItem(cjson_object, "static_ip_enable");
    cJSON *cjson_ip = cJSON_GetObjectItem(cjson_object, "ip");
    cJSON *cjson_mask = cJSON_GetObjectItem(cjson_object, "netmask");
    cJSON *cjson_gw = cJSON_GetObjectItem(cjson_object, "gateway");
    cJSON *cjson_dns1 = cJSON_GetObjectItem(cjson_object, "dns1");
    cJSON *cjson_dns2 = cJSON_GetObjectItem(cjson_object, "dns2");

    if (cjson_en == NULL || !cJSON_IsBool(cjson_en))
    {
        ESP_LOGE(TAG, "wifi sta set enable failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 1, "Wi-Fi1 Sta enable is error", resp_data);
    }
    enable = cjson_en->valueint;

    if (cjson_ssid == NULL || !cJSON_IsString(cjson_ssid))
    {
        ESP_LOGE(TAG, "wifi sta set ssid failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 2, "Wi-Fi1 Sta ssid is error", resp_data);
    }
    ssid = cjson_ssid->valuestring;

    if (cjson_pwd == NULL || !cJSON_IsString(cjson_pwd))
    {
        ESP_LOGE(TAG, "wifi sta set password failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 3, "Wi-Fi1 Sta password is error", resp_data);
    }
    pwd = cjson_pwd->valuestring;

    if (cjson_auth == NULL || !cJSON_IsNumber(cjson_auth) || cjson_auth->valueint < 0 || cjson_auth->valueint >= WIFI_AUTH_MAX)
    {
        ESP_LOGE(TAG, "wifi sta set authmode failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 4, "Wi-Fi1 Sta authmode is error", resp_data);
    }
    authmode = cjson_auth->valueint;

    ESP_LOGI(TAG, "Enable:%d, SSID:%s, PWD:%s", enable, ssid, pwd);

    if (cjson_ip_en == NULL || !cJSON_IsBool(cjson_ip_en))
    {
        ESP_LOGE(TAG, "wifi sta set static ip enable failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 5, "Wi-Fi1 Sta staic IP enable error", resp_data);
    }
    static_ip_en = cjson_ip_en->valueint;

    if (true == static_ip_en)
    {
        if (cjson_ip == NULL || !cJSON_IsString(cjson_ip))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 6, "Wi-Fi1 Sta static ip is error", resp_data);
        }
        ip = cjson_ip->valuestring;

        if (cjson_mask == NULL || !cJSON_IsString(cjson_mask))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 7, "Wi-Fi1 Sta password is error", resp_data);
        }
        mask = cjson_mask->valuestring;

        if (cjson_gw == NULL || !cJSON_IsString(cjson_gw))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 8, "Wi-Fi1 Sta password is error", resp_data);
        }
        gw = cjson_gw->valuestring;

        if (cjson_dns1 == NULL || !cJSON_IsString(cjson_dns1))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 9, "Wi-Fi1 Sta password is error", resp_data);
        }
        dns1 = cjson_dns1->valuestring;

        if (cjson_dns2 != NULL && !cJSON_IsString(cjson_dns2))  //允许不设置DNS2
        {
            ESP_LOGE(TAG, "wifi sta set dns2 failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 10, "Wi-Fi1 Sta DNS2 is error", resp_data);
        }
        dns2 = cjson_dns2->valuestring;

        ESP_LOGI(TAG, "static_ip_en:%d, IP:%s, netmask:%s, gateway:%s, dns1:%s, dns2:%s",
                    static_ip_en, ip, mask, gw, dns1, dns2);
    }

    Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth = authmode;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_auth = authmode;
    g_self_data.mod_reg12000_IOT_set.wifi_sta_auth = authmode;
    memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid, ssid,
                MIN(strlen(ssid), sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid)));
    memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid, ssid,
                MIN(strlen(ssid), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid)));
    memcpy(g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid, ssid,
                MIN(strlen(ssid), sizeof(g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid)));

    if (strlen(pwd) > 0)
    {
        memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password, pwd,
                MIN(strlen(pwd), sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password)));
        memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_password, pwd,
                MIN(strlen(pwd), sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password)));
        memcpy(g_self_data.mod_reg12000_IOT_set.wifi_sta_password, pwd,
                MIN(strlen(pwd), sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password)));
    }
    else
    {
        memset(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password, 0, sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password));
        memset(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_password, 0, sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_password));
        memset(g_self_data.mod_reg12000_IOT_set.wifi_sta_password, 0, sizeof(g_self_data.mod_reg12000_IOT_set.wifi_sta_password));
    }

    Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_enable = enable == true ? 1 : 2; //WiFi sta1 使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.on_off.bit.wifi_enable = enable == true ? 1 : 2; //WiFi sta1 使能
    g_self_data.mod_reg12000_IOT_set.on_off.bit.wifi_enable = enable == true ? 1 : 2; //WiFi sta1 使能

    // Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_en = enable == true ? 1 : 2; //WiFi sta1 使能
    // Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_mul_sta_en.sta1_en = enable == true ? 1 : 2; //WiFi sta1 使能
    // g_self_data.mod_reg13600_open.wifi_mul_sta_en.sta1_en = enable == true ? 1 : 2; //WiFi sta1 使能

    Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en = static_ip_en == true ? 1 : 2; //WiFi sta1 静态IP使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en = static_ip_en == true ? 1 : 2; //WiFi sta1 静态IP使能
    g_self_data.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en = static_ip_en == true ? 1 : 2; //WiFi sta1 静态IP使能

    if (true == static_ip_en)
    {
        ret = inet_pton(AF_INET, ip, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta IP address is invalid: %s", ip);
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 10, "Wi-Fi Sta IP address is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta1_ip = addr_int; //WiFi sta1 ip地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta1_ip = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta1_ip = addr_int;

        ret = inet_pton(AF_INET, mask, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta netmask is invalid: %s", mask);
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 11, "Wi-Fi Sta netmask is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta1_mask = addr_int; //WiFi sta1 网络掩码地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta1_mask = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta1_mask = addr_int;

        ret = inet_pton(AF_INET, gw, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta gateway is invalid: %s", gw);
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 12, "Wi-Fi Sta gateway is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta1_gw = addr_int; //WiFi sta1 网关地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta1_gw = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta1_gw = addr_int;

        ret = inet_pton(AF_INET, dns1, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta DNS1 is invalid: %s", dns1);
            return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 13, "Wi-Fi Sta DNS1 is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta1_dns1 = addr_int; //WiFi sta1 网关地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta1_dns1 = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta1_dns1 = addr_int;

        if (dns2 != NULL)
        {
            ret = inet_pton(AF_INET, dns2, &addr_int);
            if (ret == 0)
            {
                ESP_LOGE(TAG, "Wi-Fi Sta DNS2 is invalid: %s", gw);
                return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 14, "Wi-Fi Sta DNS2 is invalid", resp_data);
            }
            Inv_WR.mod_reg13600_open.wifi_sta1_dns2 = addr_int; //WiFi sta1 网关地址
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta1_dns2 = addr_int;
            g_self_data.mod_reg13600_open.wifi_sta1_dns2 = addr_int;
        }
        else
        {
            Inv_WR.mod_reg13600_open.wifi_sta1_dns2 = 0; //WiFi sta1 DNS2 地址
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta1_dns2 = 0;
            g_self_data.mod_reg13600_open.wifi_sta1_dns2 = 0;
        }

        reals.ModbusCmdFlag.sBit.wifi_sta1 = 1;
    }

    reals.ModbusCmdFlag.sBit.new_cfg = 1;   // iot 收到新的配置
    reals.ModbusCmdFlag.sBit.wifi_mul_sta = 1;
    reals.ModbusCmdFlag.sBit.wifi_sta_auth = 1;
    reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 1;
    reals.ModbusCmdFlag.sBit.wifi_sta_password = 1;

    return ws_server_msg_response(WS_SVR_WIFI_STA1_SET_RESP_TYPE, 0, "Success", resp_data);
}

int16_t ws_svr_msg_wifi_sta2_set_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_WIFI_STA2_SET_RESP_TYPE   "setWiFiSta2Rsp"

    bool enable = false;
    char *ssid = NULL;
    char *pwd = NULL;
    int authmode = 0;
    bool static_ip_en = false;
    char *ip = NULL;
    char *mask = NULL;
    char *gw = NULL;
    char *dns1 = NULL;
    char *dns2 = NULL;
    uint32_t addr_int = 0;
    int ret = 0;

    cJSON *cjson_en = cJSON_GetObjectItem(cjson_object, "enable");
    cJSON *cjson_ssid = cJSON_GetObjectItem(cjson_object, "ssid");
    cJSON *cjson_pwd = cJSON_GetObjectItem(cjson_object, "password");
    cJSON *cjson_auth = cJSON_GetObjectItem(cjson_object, "authmode");
    cJSON *cjson_ip_en = cJSON_GetObjectItem(cjson_object, "static_ip_enable");
    cJSON *cjson_ip = cJSON_GetObjectItem(cjson_object, "ip");
    cJSON *cjson_mask = cJSON_GetObjectItem(cjson_object, "netmask");
    cJSON *cjson_gw = cJSON_GetObjectItem(cjson_object, "gateway");
    cJSON *cjson_dns1 = cJSON_GetObjectItem(cjson_object, "dns1");
    cJSON *cjson_dns2 = cJSON_GetObjectItem(cjson_object, "dns2");

    if (cjson_en == NULL || !cJSON_IsBool(cjson_en))
    {
        ESP_LOGE(TAG, "wifi sta set enable failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 1, "Wi-Fi1 Sta enable is error", resp_data);
    }
    enable = cjson_en->valueint;

    if (cjson_ssid == NULL || !cJSON_IsString(cjson_ssid))
    {
        ESP_LOGE(TAG, "wifi sta set ssid failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 2, "Wi-Fi1 Sta ssid is error", resp_data);
    }
    ssid = cjson_ssid->valuestring;

    if (cjson_pwd == NULL || !cJSON_IsString(cjson_pwd))
    {
        ESP_LOGE(TAG, "wifi sta set password failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 3, "Wi-Fi1 Sta password is error", resp_data);
    }
    pwd = cjson_pwd->valuestring;

    if (cjson_auth == NULL || !cJSON_IsNumber(cjson_auth) || cjson_auth->valueint < 0 || cjson_auth->valueint >= WIFI_AUTH_MAX)
    {
        ESP_LOGE(TAG, "wifi sta set authmode failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 4, "Wi-Fi1 Sta authmode is error", resp_data);
    }
    authmode = cjson_auth->valueint;

    ESP_LOGI(TAG, "Enable:%d, SSID:%s, PWD:%s", enable, ssid, pwd);

    if (cjson_ip_en == NULL || !cJSON_IsBool(cjson_ip_en))
    {
        ESP_LOGE(TAG, "wifi sta set static ip enable failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 5, "Wi-Fi1 Sta staic IP enable error", resp_data);
    }
    static_ip_en = cjson_ip_en->valueint;

    if (true == static_ip_en)
    {
        if (cjson_ip == NULL || !cJSON_IsString(cjson_ip))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 6, "Wi-Fi1 Sta static ip is error", resp_data);
        }
        ip = cjson_ip->valuestring;

        if (cjson_mask == NULL || !cJSON_IsString(cjson_mask))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 7, "Wi-Fi1 Sta password is error", resp_data);
        }
        mask = cjson_mask->valuestring;

        if (cjson_gw == NULL || !cJSON_IsString(cjson_gw))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 8, "Wi-Fi1 Sta password is error", resp_data);
        }
        gw = cjson_gw->valuestring;

        if (cjson_dns1 == NULL || !cJSON_IsString(cjson_dns1))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 9, "Wi-Fi1 Sta password is error", resp_data);
        }
        dns1 = cjson_dns1->valuestring;

        if (cjson_dns2 != NULL && !cJSON_IsString(cjson_dns2))  //允许不设置DNS2
        {
            ESP_LOGE(TAG, "wifi sta set dns2 failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 10, "Wi-Fi1 Sta DNS2 is error", resp_data);
        }
        dns2 = cjson_dns2->valuestring;

        ESP_LOGI(TAG, "static_ip_en:%d, IP:%s, netmask:%s, gateway:%s, dns1:%s, dns2:%s",
                    static_ip_en, ip, mask, gw, dns1, dns2);
    }

    Inv_WR.mod_reg13600_open.wifi_sta2_auth = authmode; //WiFi sta1 认证方式
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_auth = authmode;
    g_self_data.mod_reg13600_open.wifi_sta2_auth = authmode;

    memcpy(Inv_WR.mod_reg13600_open.wifi_sta2_ssid, ssid,
            MIN(strlen(ssid), sizeof(Inv_WR.mod_reg13600_open.wifi_sta2_ssid)));
    memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_ssid, ssid,
            MIN(strlen(ssid), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_ssid)));
    memcpy(g_self_data.mod_reg13600_open.wifi_sta2_ssid, ssid,
            MIN(strlen(ssid), sizeof(g_self_data.mod_reg13600_open.wifi_sta2_ssid)));

    if (strlen(pwd) > 0)
    {
        memcpy(Inv_WR.mod_reg13600_open.wifi_sta2_password, pwd,
                MIN(strlen(pwd), sizeof(Inv_WR.mod_reg13600_open.wifi_sta2_password)));
        memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_password, pwd,
                MIN(strlen(pwd), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_password)));
        memcpy(g_self_data.mod_reg13600_open.wifi_sta2_password, pwd,
                MIN(strlen(pwd), sizeof(g_self_data.mod_reg13600_open.wifi_sta2_password)));
    }
    else
    {
        memset(Inv_WR.mod_reg13600_open.wifi_sta2_password, 0x0, sizeof(Inv_WR.mod_reg13600_open.wifi_sta2_password));
        memset(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_password, 0x0, sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_password));
        memset(g_self_data.mod_reg13600_open.wifi_sta2_password, 0x0, sizeof(g_self_data.mod_reg13600_open.wifi_sta2_password));
    }

    Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_en = enable == true? 1 : 2; //WiFi sta1 使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_mul_sta_en.sta2_en = enable == true? 1 : 2;
    g_self_data.mod_reg13600_open.wifi_mul_sta_en.sta2_en = enable == true? 1 : 2;

    Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en = static_ip_en == true? 1 : 2; //WiFi sta1 静态IP使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en = static_ip_en == true? 1 : 2;
    g_self_data.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en = static_ip_en == true? 1 : 2;

    if (true == static_ip_en)
    {
        ret = inet_pton(AF_INET, ip, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta IP address is invalid: %s", ip);
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 10, "Wi-Fi Sta IP address is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta2_ip = addr_int; //WiFi sta1 ip地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_ip = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta2_ip = addr_int;

        ret = inet_pton(AF_INET, mask, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta netmask is invalid: %s", mask);
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 11, "Wi-Fi Sta netmask is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta2_mask = addr_int; //WiFi sta1 网络掩码地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_mask = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta2_mask = addr_int;

        ret = inet_pton(AF_INET, gw, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta gateway is invalid: %s", gw);
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 12, "Wi-Fi Sta gateway is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta2_gw = addr_int; //WiFi sta1 网关地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_gw = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta2_gw = addr_int;

        ret = inet_pton(AF_INET, dns1, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta DNS1 is invalid: %s", dns1);
            return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 13, "Wi-Fi Sta DNS1 is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta2_dns1 = addr_int; //WiFi sta1 网关地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_dns1 = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta2_dns1 = addr_int;

        if (dns2 != NULL)
        {
            ret = inet_pton(AF_INET, dns2, &addr_int);
            if (ret == 0)
            {
                ESP_LOGE(TAG, "Wi-Fi Sta DNS2 is invalid: %s", gw);
                return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 14, "Wi-Fi Sta DNS2 is invalid", resp_data);
            }
            Inv_WR.mod_reg13600_open.wifi_sta2_dns2 = addr_int; //WiFi sta1 网关地址
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_dns2 = addr_int;
            g_self_data.mod_reg13600_open.wifi_sta2_dns2 = addr_int;
        }
        else
        {
            Inv_WR.mod_reg13600_open.wifi_sta2_dns2 = 0; //WiFi sta1 DNS2 地址
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta2_dns2 = 0;
            g_self_data.mod_reg13600_open.wifi_sta2_dns2 = 0;
        }

        reals.ModbusCmdFlag.sBit.wifi_sta2 = 1;
    }

    reals.ModbusCmdFlag.sBit.new_cfg = 1;   // iot 收到新的配置
    reals.ModbusCmdFlag.sBit.wifi_mul_sta = 1;
    reals.ModbusCmdFlag.sBit.wifi_sta2 = 1;

    return ws_server_msg_response(WS_SVR_WIFI_STA2_SET_RESP_TYPE, 0, "Success", resp_data);
}

int16_t ws_svr_msg_wifi_sta3_set_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_WIFI_STA3_SET_RESP_TYPE   "setWiFiSta3Rsp"

    bool enable = false;
    char *ssid = NULL;
    char *pwd = NULL;
    int authmode = 0;
    bool static_ip_en = false;
    char *ip = NULL;
    char *mask = NULL;
    char *gw = NULL;
    char *dns1 = NULL;
    char *dns2 = NULL;
    uint32_t addr_int = 0;
    int ret = 0;

    cJSON *cjson_en = cJSON_GetObjectItem(cjson_object, "enable");
    cJSON *cjson_ssid = cJSON_GetObjectItem(cjson_object, "ssid");
    cJSON *cjson_pwd = cJSON_GetObjectItem(cjson_object, "password");
    cJSON *cjson_auth = cJSON_GetObjectItem(cjson_object, "authmode");
    cJSON *cjson_ip_en = cJSON_GetObjectItem(cjson_object, "static_ip_enable");
    cJSON *cjson_ip = cJSON_GetObjectItem(cjson_object, "ip");
    cJSON *cjson_mask = cJSON_GetObjectItem(cjson_object, "netmask");
    cJSON *cjson_gw = cJSON_GetObjectItem(cjson_object, "gateway");
    cJSON *cjson_dns1 = cJSON_GetObjectItem(cjson_object, "dns1");
    cJSON *cjson_dns2 = cJSON_GetObjectItem(cjson_object, "dns2");

    if (cjson_en == NULL || !cJSON_IsBool(cjson_en))
    {
        ESP_LOGE(TAG, "wifi sta set enable failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 1, "Wi-Fi1 Sta enable is error", resp_data);
    }
    enable = cjson_en->valueint;

    if (cjson_ssid == NULL || !cJSON_IsString(cjson_ssid))
    {
        ESP_LOGE(TAG, "wifi sta set ssid failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 2, "Wi-Fi1 Sta ssid is error", resp_data);
    }
    ssid = cjson_ssid->valuestring;

    if (cjson_pwd == NULL || !cJSON_IsString(cjson_pwd))
    {
        ESP_LOGE(TAG, "wifi sta set password failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 3, "Wi-Fi1 Sta password is error", resp_data);
    }
    pwd = cjson_pwd->valuestring;

    if (cjson_auth == NULL || !cJSON_IsNumber(cjson_auth) || cjson_auth->valueint < 0 || cjson_auth->valueint >= WIFI_AUTH_MAX)
    {
        ESP_LOGE(TAG, "wifi sta set authmode failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 4, "Wi-Fi1 Sta authmode is error", resp_data);
    }
    authmode = cjson_auth->valueint;

    ESP_LOGI(TAG, "Enable:%d, SSID:%s, PWD:%s", enable, ssid, pwd);

    if (cjson_ip_en == NULL || !cJSON_IsBool(cjson_ip_en))
    {
        ESP_LOGE(TAG, "wifi sta set static ip enable failed");
        return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 5, "Wi-Fi1 Sta staic IP enable error", resp_data);
    }
    static_ip_en = cjson_ip_en->valueint;

    if (true == static_ip_en)
    {
        if (cjson_ip == NULL || !cJSON_IsString(cjson_ip))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 6, "Wi-Fi1 Sta static ip is error", resp_data);
        }
        ip = cjson_ip->valuestring;

        if (cjson_mask == NULL || !cJSON_IsString(cjson_mask))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 7, "Wi-Fi1 Sta password is error", resp_data);
        }
        mask = cjson_mask->valuestring;

        if (cjson_gw == NULL || !cJSON_IsString(cjson_gw))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 8, "Wi-Fi1 Sta password is error", resp_data);
        }
        gw = cjson_gw->valuestring;

        if (cjson_dns1 == NULL || !cJSON_IsString(cjson_dns1))
        {
            ESP_LOGE(TAG, "wifi sta set password failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 9, "Wi-Fi1 Sta password is error", resp_data);
        }
        dns1 = cjson_dns1->valuestring;

        if (cjson_dns2 != NULL && !cJSON_IsString(cjson_dns2))  //允许不设置DNS2
        {
            ESP_LOGE(TAG, "wifi sta set dns2 failed");
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 10, "Wi-Fi1 Sta DNS2 is error", resp_data);
        }
        dns2 = cjson_dns2->valuestring;

        ESP_LOGI(TAG, "static_ip_en:%d, IP:%s, netmask:%s, gateway:%s, dns1:%s, dns2:%s",
                    static_ip_en, ip, mask, gw, dns1, dns2);
    }

    Inv_WR.mod_reg13600_open.wifi_sta3_auth = authmode; //WiFi sta1 认证方式
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_auth = authmode;
    g_self_data.mod_reg13600_open.wifi_sta3_auth = authmode;

    memcpy(Inv_WR.mod_reg13600_open.wifi_sta3_ssid, ssid,
            MIN(strlen(ssid), sizeof(Inv_WR.mod_reg13600_open.wifi_sta3_ssid)));
    memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_ssid, ssid,
            MIN(strlen(ssid), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_ssid)));
    memcpy(g_self_data.mod_reg13600_open.wifi_sta3_ssid, ssid,
            MIN(strlen(ssid), sizeof(g_self_data.mod_reg13600_open.wifi_sta3_ssid)));

    memcpy(Inv_WR.mod_reg13600_open.wifi_sta3_password, pwd,
            MIN(strlen(pwd), sizeof(Inv_WR.mod_reg13600_open.wifi_sta3_password)));
    memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_password, pwd,
            MIN(strlen(pwd), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_password)));
    memcpy(g_self_data.mod_reg13600_open.wifi_sta3_password, pwd,
            MIN(strlen(pwd), sizeof(g_self_data.mod_reg13600_open.wifi_sta3_password)));

    Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_en = enable == true? 1 : 2; //WiFi sta1 使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_mul_sta_en.sta3_en = enable == true? 1 : 2;
    g_self_data.mod_reg13600_open.wifi_mul_sta_en.sta3_en = enable == true? 1 : 2;

    Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en = static_ip_en == true? 1 : 2; //WiFi sta1 静态IP使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en = static_ip_en == true? 1 : 2;
    g_self_data.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en = static_ip_en == true? 1 : 2;

    if (true == static_ip_en)
    {
        ret = inet_pton(AF_INET, ip, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta IP address is invalid: %s", ip);
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 10, "Wi-Fi Sta IP address is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta3_ip = addr_int; //WiFi sta1 ip地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_ip = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta3_ip = addr_int;

        ret = inet_pton(AF_INET, mask, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta netmask is invalid: %s", mask);
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 11, "Wi-Fi Sta netmask is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta3_mask = addr_int; //WiFi sta1 网络掩码地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_mask = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta3_mask = addr_int;

        ret = inet_pton(AF_INET, gw, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta gateway is invalid: %s", gw);
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 12, "Wi-Fi Sta gateway is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta3_gw = addr_int; //WiFi sta1 网关地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_gw = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta3_gw = addr_int;

        ret = inet_pton(AF_INET, dns1, &addr_int);
        if (ret == 0)
        {
            ESP_LOGE(TAG, "Wi-Fi Sta DNS1 is invalid: %s", dns1);
            return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 13, "Wi-Fi Sta DNS1 is invalid", resp_data);
        }
        Inv_WR.mod_reg13600_open.wifi_sta3_dns1 = addr_int; //WiFi sta1 网关地址
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_dns1 = addr_int;
        g_self_data.mod_reg13600_open.wifi_sta3_dns1 = addr_int;

        if (dns2 != NULL)
        {
            ret = inet_pton(AF_INET, dns2, &addr_int);
            if (ret == 0)
            {
                ESP_LOGE(TAG, "Wi-Fi Sta DNS2 is invalid: %s", gw);
                return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 14, "Wi-Fi Sta DNS2 is invalid", resp_data);
            }
            Inv_WR.mod_reg13600_open.wifi_sta3_dns2 = addr_int; //WiFi sta1 网关地址
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_dns2 = addr_int;
            g_self_data.mod_reg13600_open.wifi_sta3_dns2 = addr_int;
        }
        else
        {
            Inv_WR.mod_reg13600_open.wifi_sta3_dns2 = 0; //WiFi sta1 DNS2 地址
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.wifi_sta3_dns2 = 0;
            g_self_data.mod_reg13600_open.wifi_sta3_dns2 = 0;
        }

        reals.ModbusCmdFlag.sBit.wifi_sta3 = 1;
    }

    reals.ModbusCmdFlag.sBit.new_cfg = 1;   // iot 收到新的配置
    reals.ModbusCmdFlag.sBit.wifi_mul_sta = 1;
    reals.ModbusCmdFlag.sBit.wifi_sta3 = 1;

    return ws_server_msg_response(WS_SVR_WIFI_STA3_SET_RESP_TYPE, 0, "Success", resp_data);
}

int16_t ws_svr_msg_wifi_ap_set_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_WIFI_AP_SET_RESP_TYPE   "setWiFiApRsp"
    bool enable = false;
    char *ssid = NULL;
    char *pwd = NULL;
    int auth = 0;

    cJSON *cjson_en = cJSON_GetObjectItem(cjson_object, "enable");
    cJSON *cjson_ssid = cJSON_GetObjectItem(cjson_object, "ssid");
    cJSON *cjson_pwd = cJSON_GetObjectItem(cjson_object, "password");
    cJSON *cjson_auth = cJSON_GetObjectItem(cjson_object, "authmode");
    if (cjson_en == NULL || !cJSON_IsBool(cjson_en))
    {
        ESP_LOGE(TAG, "wifi ap set enable failed");
        return ws_server_msg_response(WS_SVR_WIFI_AP_SET_RESP_TYPE, 1, "Wi-Fi AP enable is error", resp_data);
    }
    enable = cjson_en->valueint;

    if (cjson_ssid == NULL || !cJSON_IsString(cjson_ssid))
    {
        ESP_LOGE(TAG, "wifi ap set ssid failed");
        return ws_server_msg_response(WS_SVR_WIFI_AP_SET_RESP_TYPE, 2, "Wi-Fi AP ssid is error", resp_data);
    }
    ssid = cjson_ssid->valuestring;

    if (cjson_pwd == NULL || !cJSON_IsString(cjson_pwd))
    {
        ESP_LOGE(TAG, "wifi ap set password failed");
        return ws_server_msg_response(WS_SVR_WIFI_AP_SET_RESP_TYPE, 3, "Wi-Fi AP password is error", resp_data);
    }
    pwd = cjson_pwd->valuestring;

    if (cjson_auth == NULL || !cJSON_IsNumber(cjson_auth) || cjson_auth->valueint < 0 || cjson_auth->valueint >= WIFI_AUTH_MAX)
    {
        ESP_LOGE(TAG, "wifi ap set authmode failed");
        return ws_server_msg_response(WS_SVR_WIFI_AP_SET_RESP_TYPE, 4, "Wi-Fi AP authmode is error", resp_data);
    }
    auth = cjson_auth->valueint;

    ESP_LOGI(TAG, "Enable:%d, SSID:%s, PWD:%s, auth:%d", enable, ssid, pwd, auth);

    Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_ap_enable = enable == true? 1 : 2;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.on_off.bit.wifi_ap_enable = enable == true? 1 : 2;
    g_self_data.mod_reg12000_IOT_set.on_off.bit.wifi_ap_enable = enable == true? 1 : 2;

    memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_AP_ssid, ssid,
            MIN(strlen(ssid), sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_AP_ssid)));
    memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_AP_ssid, ssid,
            MIN(strlen(ssid), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_AP_ssid)));
    memcpy(g_self_data.mod_reg12000_IOT_set.wifi_AP_ssid, ssid,
            MIN(strlen(ssid), sizeof(g_self_data.mod_reg12000_IOT_set.wifi_AP_ssid)));

    //AP密码长度必须超过8个字符
    if (auth != WIFI_AUTH_OPEN && strlen(pwd) < WIFI_AP_PWD_LEN_MIN)
    {
        ESP_LOGE(TAG, "wifi ap set password length is less than %d", WIFI_AP_PWD_LEN_MIN);
        char err_msg[64] = {0};
        snprintf(err_msg, sizeof(err_msg), "Wi-Fi AP password length is less than %d", WIFI_AP_PWD_LEN_MIN);
        return ws_server_msg_response(WS_SVR_WIFI_AP_SET_RESP_TYPE, 5, err_msg, resp_data);
    }

    if (strlen(pwd) > 0)
    {
        memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_AP_password, pwd,
                MIN(strlen(pwd), sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_AP_password)));
        memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_AP_password, pwd,
                MIN(strlen(pwd), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_AP_password)));
        memcpy(g_self_data.mod_reg12000_IOT_set.wifi_AP_password, pwd,
                MIN(strlen(pwd), sizeof(g_self_data.mod_reg12000_IOT_set.wifi_AP_password)));

        Inv_WR.mod_reg12000_IOT_set.wifi_ap_auth = auth;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_ap_auth = auth;
        g_self_data.mod_reg12000_IOT_set.wifi_ap_auth = auth;
    }
    else
    {
        memset(Inv_WR.mod_reg12000_IOT_set.wifi_AP_password, 0x0,sizeof(Inv_WR.mod_reg12000_IOT_set.wifi_AP_password));
        memset(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_AP_password, 0x0,sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_AP_password));
        memset(g_self_data.mod_reg12000_IOT_set.wifi_AP_password, 0x0,sizeof(g_self_data.mod_reg12000_IOT_set.wifi_AP_password));

        // 密码为空，则鉴权模式强制设置为open，无密码
        Inv_WR.mod_reg12000_IOT_set.wifi_ap_auth = auth;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_ap_auth = auth;
        g_self_data.mod_reg12000_IOT_set.wifi_ap_auth = auth;
    }

    reals.ModbusCmdFlag.sBit.on_off = 1;
    reals.ModbusCmdFlag.sBit.wifi_ap = 1;

    return ws_server_msg_response(WS_SVR_WIFI_AP_SET_RESP_TYPE, 0, "Success", resp_data);
}

int16_t ws_svr_msg_ble_server_set_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_BLE_SVR_SET_RESP_TYPE   "setBleServerRsp"

    // 蓝牙使能
    cJSON *cjson_en = cJSON_GetObjectItem(cjson_object, "enable");
    if (cjson_en == NULL || !cJSON_IsBool(cjson_en))
    {
        ESP_LOGE(TAG, "ble server set enable failed");
        return ws_server_msg_response(WS_SVR_BLE_SVR_SET_RESP_TYPE, 1, "Bluetooth server enable is error", resp_data);
    }

    // 蓝牙密码
    cJSON *cjson_pwd = cJSON_GetObjectItem(cjson_object, "password");
    if (cjson_pwd == NULL || !cJSON_IsString(cjson_pwd))
    {
        ESP_LOGE(TAG, "ble server set password failed");
        return ws_server_msg_response(WS_SVR_BLE_SVR_SET_RESP_TYPE, 2, "Bluetooth server password is error", resp_data);
    }

    // 广播使能
    cJSON *cjson_adv_en = cJSON_GetObjectItem(cjson_object, "adv_enable");
    if (cjson_adv_en == NULL || !cJSON_IsBool(cjson_adv_en))
    {
        ESP_LOGE(TAG, "ble server set password failed");
        return ws_server_msg_response(WS_SVR_BLE_SVR_SET_RESP_TYPE, 3, "Bluetooth server advertising eanble is error", resp_data);
    }
    // 广播加密秘钥 TODO：测试使用
    cJSON *cjson_adv_key = cJSON_GetObjectItem(cjson_object, "adv_key");
    if (cjson_adv_key == NULL || !cJSON_IsString(cjson_adv_key))
    {
        ESP_LOGE(TAG, "ble server set password failed");
        return ws_server_msg_response(WS_SVR_BLE_SVR_SET_RESP_TYPE, 4, "Bluetooth server advertising is error", resp_data);
    }

    const bool enable = cjson_en->valueint;
    const char *pwd = cjson_pwd->valuestring;
    const bool adv_enable = cjson_adv_en->valueint;
    const char *adv_key = cjson_adv_key->valuestring;

    ESP_LOGI(TAG, "Enable:%d, password:%s:%d, adv_enable:%d, adv_key:%s", enable, pwd, strlen(pwd), adv_enable, adv_key);

    //设置BLE使能: 同时设置Inv_WR、Inv和g_self_data
    Inv_WR.mod_reg12000_IOT_set.on_off.bit.ble_enable = enable == true? 1 : 2;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.on_off.bit.ble_enable = enable == true? 1 : 2;
    g_self_data.mod_reg12000_IOT_set.on_off.bit.ble_enable = enable == true? 1 : 2;

    Inv_WR.mod_reg13600_open.ble_protocol.adv_en = adv_enable == true? 1 : 2;
    Inv_WR.mod_reg13600_open.ble_protocol.lcd_adv_en = adv_enable == true? 1 : 2;

    if (strlen(pwd))
    {
        memcpy((char *)Inv_WR.mod_reg00000.app_password, pwd,
                MIN(strlen(pwd), sizeof(Inv_WR.mod_reg00000.app_password)));
        memcpy((char *)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000.app_password, pwd,
                MIN(strlen(pwd), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000.app_password)));
        memcpy((char *)g_self_data.mod_reg00000.app_password, pwd,
                MIN(strlen(pwd), sizeof(g_self_data.mod_reg00000.app_password)));
    }
    else
    {
        memset((char *)Inv_WR.mod_reg00000.app_password, 0x0, sizeof(Inv_WR.mod_reg00000.app_password));
        memset((char *)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000.app_password, 0x0, sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000.app_password));
        memset((char *)g_self_data.mod_reg00000.app_password, 0x0, sizeof(g_self_data.mod_reg00000.app_password));
    }

    //TODO: 临时调试功能, 后续删除
    uint8_t ble_adv_key[16] = {0};
    int ret = hex_str_to_bytes(adv_key, ble_adv_key, sizeof(ble_adv_key));
    ESP_LOGW(TAG, "adv key hex to bytes ret:%d, key:%s", ret, ble_adv_key);
    if (ret <= 0)
    {
        memset(Inv_WR.mod_reg13600_open.bles_adv_key, 0x00, sizeof(Inv_WR.mod_reg13600_open.bles_adv_key));
        memset(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.bles_adv_key, 0x00, sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.bles_adv_key));
    }
    else
    {
        memcpy((char *)Inv_WR.mod_reg13600_open.bles_adv_key, ble_adv_key,
                MIN(ret, sizeof(Inv_WR.mod_reg13600_open.bles_adv_key)));
        memcpy((char *)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.bles_adv_key, ble_adv_key,
                MIN(ret, sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.bles_adv_key)));
    }

    reals.ModbusCmdFlag.sBit.on_off = 1;
    reals.ModbusCmdFlag.sBit.app_password = 1;
    reals.ModbusCmdFlag.sBit.ble_server = 1;
    reals.ModbusCmdFlag.sBit.ble_protocol = 1;

    return ws_server_msg_response(WS_SVR_BLE_SVR_SET_RESP_TYPE, 0, "Success", resp_data);
}

int16_t ws_svr_msg_mqtt_set_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_MQTT_SET_RESP_TYPE   "setMqttRsp"

    bool enable = false;
    char *url = NULL;
    bool ctrl_en = false;
    bool report_en = false;
    bool crypt_en = false;
    uint16_t report_cycle = 0;

    cJSON *cjson_en = cJSON_GetObjectItem(cjson_object, "enable");
    cJSON *cjson_url = cJSON_GetObjectItem(cjson_object, "url");
    cJSON *cjson_ctrl_en = cJSON_GetObjectItem(cjson_object, "ctrl_enable");
    cJSON *cjson_report_en = cJSON_GetObjectItem(cjson_object, "report_enable");
    cJSON *cjson_crypt_en = cJSON_GetObjectItem(cjson_object, "crypt_enable");
    cJSON *cjson_report_cycle = cJSON_GetObjectItem(cjson_object, "report_cycle");

    if (cjson_en == NULL || !cJSON_IsBool(cjson_en))
    {
        ESP_LOGE(TAG, "mqtt set enable failed");
        return ws_server_msg_response(WS_SVR_MQTT_SET_RESP_TYPE, 1, "MQTT enable is error", resp_data);
    }
    enable = cjson_en->valueint;

    if (cjson_url == NULL || !cJSON_IsString(cjson_url))
    {
        ESP_LOGE(TAG, "mqtt url set ssid failed");
        return ws_server_msg_response(WS_SVR_MQTT_SET_RESP_TYPE, 2, "MQTT URL is error", resp_data);
    }
    url = cjson_url->valuestring;

    if (cjson_ctrl_en == NULL || !cJSON_IsBool(cjson_ctrl_en))
    {
        ESP_LOGE(TAG, "mqtt ctrl enable failed");
        return ws_server_msg_response(WS_SVR_MQTT_SET_RESP_TYPE, 1, "MQTT ctrl enable is error", resp_data);
    }
    ctrl_en = cjson_ctrl_en->valueint;

    if (cjson_report_en == NULL || !cJSON_IsBool(cjson_report_en))
    {
        ESP_LOGE(TAG, "mqtt report enable failed");
        return ws_server_msg_response(WS_SVR_MQTT_SET_RESP_TYPE, 1, "MQTT report enable is error", resp_data);
    }
    report_en = cjson_report_en->valueint;

    if (cjson_report_cycle == NULL || !cJSON_IsNumber(cjson_report_cycle)
        || cjson_report_cycle->valueint < 1 || cjson_report_cycle->valueint > 65535)
    {
        ESP_LOGE(TAG, "mqtt report cycle set failed");
        return ws_server_msg_response(WS_SVR_MQTT_SET_RESP_TYPE, 3, "MQTT report cycle is error", resp_data);
    }
    report_cycle = cjson_report_cycle->valueint;
    
    if (cjson_crypt_en == NULL || !cJSON_IsBool(cjson_crypt_en))
    {
        ESP_LOGE(TAG, "mqtt crypt enable failed");
        return ws_server_msg_response(WS_SVR_MQTT_SET_RESP_TYPE, 1, "MQTT crypt enable is error", resp_data);
    }
    crypt_en = cjson_crypt_en->valueint;

    Inv_WR.mod_reg13600_open.open_mqtt_enable.enable = enable == true? 1 : 2; //MQTT使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.open_mqtt_enable.enable = enable == true? 1 : 2;
    g_self_data.mod_reg13600_open.open_mqtt_enable.enable = enable == true? 1 : 2;

    Inv_WR.mod_reg13600_open.open_mqtt_enable.crypt_en = crypt_en == true? 1 : 2; //MQTT加密使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.open_mqtt_enable.crypt_en = crypt_en == true? 1 : 2;
    g_self_data.mod_reg13600_open.open_mqtt_enable.crypt_en = crypt_en == true? 1 : 2;

    Inv_WR.mod_reg13600_open.open_mqtt_enable.ctrl_en = ctrl_en == true? 1 : 2; //MQTT控制使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.open_mqtt_enable.ctrl_en = ctrl_en == true? 1 : 2;
    g_self_data.mod_reg13600_open.open_mqtt_enable.ctrl_en = ctrl_en == true? 1 : 2;

    Inv_WR.mod_reg13600_open.open_mqtt_enable.report_en = report_en == true? 1 : 2; //MQTT上报使能
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.open_mqtt_enable.report_en = report_en == true? 1 : 2;
    g_self_data.mod_reg13600_open.open_mqtt_enable.report_en = report_en == true? 1 : 2;

    Inv_WR.mod_reg13600_open.open_mqtt_report_cycle = report_cycle; //MQTT上报周期
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.open_mqtt_report_cycle = report_cycle;
    g_self_data.mod_reg13600_open.open_mqtt_report_cycle = report_cycle;

    memcpy(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address, url,
            MIN(strlen(url), sizeof(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address)));
    memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg22000_net_server_2rd.Net_Server_address, url,
            MIN(strlen(url), sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg22000_net_server_2rd.Net_Server_address)));
    memcpy(g_self_data.mod_reg22000_net_server_2rd.Net_Server_address, url,
            MIN(strlen(url), sizeof(g_self_data.mod_reg22000_net_server_2rd.Net_Server_address)));

    reals.ModbusCmdFlag.sBit.mqtt = 1;

    return ws_server_msg_response(WS_SVR_MQTT_SET_RESP_TYPE, 0, "Success", resp_data);
}

int16_t ws_svr_msg_modbustcp_set_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_MD_TCP_SET_RESP_TYPE   "setModbusTcpRsp"

    bool enable = false;
    uint16_t port = 0;
    bool crypt_en = false;

    cJSON *cjson_en = cJSON_GetObjectItem(cjson_object, "enable");
    cJSON *cjson_port = cJSON_GetObjectItem(cjson_object, "port");
    cJSON *cjson_crypt_en = cJSON_GetObjectItem(cjson_object, "crypt_enable");

    if (cjson_en == NULL || !cJSON_IsBool(cjson_en))
    {
        ESP_LOGE(TAG, "modbus tcp set enable failed");
        return ws_server_msg_response(WS_SVR_MD_TCP_SET_RESP_TYPE, 1, "Modbus TCP enable is error", resp_data);
    }
    enable = cjson_en->valueint;

    if (cjson_crypt_en == NULL || !cJSON_IsBool(cjson_crypt_en))
    {
        ESP_LOGE(TAG, "modbus tcp crypt enable failed");
        return ws_server_msg_response(WS_SVR_MD_TCP_SET_RESP_TYPE, 2, "Modbus TCP crypt enable is error", resp_data);
    }
    crypt_en = cjson_crypt_en->valueint;

    if (cjson_port == NULL || !cJSON_IsNumber(cjson_port)
        || cjson_port->valueint < 1 || cjson_port->valueint > 65535)
    {
        ESP_LOGE(TAG, "modbus TCP report cycle set failed");
        return ws_server_msg_response(WS_SVR_MD_TCP_SET_RESP_TYPE, 3, "Modbus TCP port is error", resp_data);
    }
    port = cjson_port->valueint;

    Inv_WR.mod_reg13600_open.modbus_tcp_enable.enable = enable == true? 1 : 2;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.modbus_tcp_enable.enable = enable == true? 1 : 2;
    g_self_data.mod_reg13600_open.modbus_tcp_enable.enable = enable == true? 1 : 2;

    Inv_WR.mod_reg13600_open.modbus_tcp_enable.crypt_en = crypt_en == true? 1 : 2;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.modbus_tcp_enable.crypt_en = crypt_en == true? 1 : 2;
    g_self_data.mod_reg13600_open.modbus_tcp_enable.crypt_en = crypt_en == true? 1 : 2;

    Inv_WR.mod_reg13600_open.modbus_tcp_port = port;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.modbus_tcp_port = port;
    g_self_data.mod_reg13600_open.modbus_tcp_port = port;

    reals.ModbusCmdFlag.sBit.modbus_tcp = 1;

    return ws_server_msg_response(WS_SVR_MD_TCP_SET_RESP_TYPE, 0, "Success", resp_data);
}

int16_t ws_svr_msg_version_get_handler(cJSON *cjson_object, uint8_t **resp_data)
{
extern ota_cmd_list_t *new_firmware_list;

#define WS_SVR_VERSION_GET_RESP_TYPE   "getVersionRsp"
    int16_t ret = 0;
    cJSON *cjson_resp = NULL;
    cJSON *cjson_data = NULL;
    cJSON *cjson_temp = NULL;
    cJSON *cjson_version = NULL;
    char str_tmp[128] = {0};

    cjson_resp = cJSON_CreateObject();
    if (cjson_resp == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_resp is NULL");
        return -1;
    }

    cJSON_AddItemToObject(cjson_resp, "type", cJSON_CreateString(WS_SVR_VERSION_GET_RESP_TYPE));
    cjson_data = cJSON_CreateObject();
    if (cjson_data == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_data is NULL");
        ret = -2;
        goto err;
    }

    cjson_version = cJSON_CreateArray();
    if (cjson_version == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_ap is NULL");
        ret = -3;
        goto err;
    }

    // 目前仅支持IOT升级
    if (new_firmware_list)
    {
        //遍历固件信息列表, 填充到"firmwares"数组中
        ota_cmd_list_t  *current = new_firmware_list;
        while (current)
        {
            cjson_temp = cJSON_CreateObject();
            cJSON_AddStringToObject(cjson_temp, "model", current->object.model);
            snprintf(str_tmp, sizeof(str_tmp), "%lu", g_self_data.mod_reg11000_IOT_info.software_ver);
            cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
            snprintf(str_tmp, sizeof(str_tmp), "%lu", current->object.version);
            cJSON_AddStringToObject(cjson_temp, "newVersion", str_tmp);
            cJSON_AddItemToArray(cjson_version, cjson_temp);

            ESP_LOGW(TAG, "----- firmware info, model:%s, version:%lu, newVersion:%lu", current->object.model,
                    g_self_data.mod_reg11000_IOT_info.software_ver, current->object.version);

            current = current->next;
        }
    }
    else
    {
        cjson_temp = cJSON_CreateObject();
        cJSON_AddStringToObject(cjson_temp, "model", g_self_data.mod_reg11000_IOT_info.iot_type);
        snprintf(str_tmp, sizeof(str_tmp), "%lu", g_self_data.mod_reg11000_IOT_info.software_ver);
        cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
        snprintf(str_tmp, sizeof(str_tmp), "%lu", g_self_data.mod_reg11000_IOT_info.software_ver);
        cJSON_AddStringToObject(cjson_temp, "newVersion", str_tmp);
        cJSON_AddItemToArray(cjson_version, cjson_temp);
    }

    cJSON_AddItemToObject(cjson_data, "firmwares", cjson_version);
    cJSON_AddItemToObject(cjson_resp, "data", cjson_data);

    *resp_data = (uint8_t *)cJSON_PrintUnformatted(cjson_resp);
    cJSON_Delete(cjson_resp);
    cjson_resp = NULL;

    return strlen((char *)*resp_data);
err:
    if (cjson_resp != NULL)
    {
        cJSON_Delete(cjson_resp);
    }
    if (cjson_data != NULL)
    {
        cJSON_Delete(cjson_data);
    }
    if (cjson_version != NULL)
    {
        cJSON_Delete(cjson_version);
    }

    return ret;
}

int16_t ws_server_upgrade_progress(uint16_t err_code, uint16_t progress, uint8_t **resp_data)
{
    cJSON *cjson_resp = cJSON_CreateObject();
    if (cjson_resp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_resp is NULL");
        return -2;
    }

    cJSON_AddItemToObject(cjson_resp, "type", cJSON_CreateString(WS_SVR_UPGRADE_RESP_TYPE));

    cJSON *cjson_result = cJSON_CreateObject();
    if (cjson_result == NULL)
    {
        cJSON_Delete(cjson_resp);
        ESP_LOGE(TAG, "ws_server_msg_response cjson_result is NULL");
        return -2;
    }
    cJSON_AddItemToObject(cjson_result, "errCode", cJSON_CreateNumber(err_code));
    cJSON_AddItemToObject(cjson_result, "progress", cJSON_CreateNumber(progress));

    cJSON_AddItemToObject(cjson_resp, "result", cjson_result);
    *resp_data = (uint8_t *)cJSON_PrintUnformatted(cjson_resp);
    cJSON_Delete(cjson_resp);

    return strlen((char *)*resp_data);
}

int16_t ws_svr_msg_upgrade_handler(cJSON *cjson_object, uint8_t **resp_data)
{
    char *model = NULL;
    char *version = NULL;
    int ret = 0;

    cJSON *cjson_model = cJSON_GetObjectItem(cjson_object, "model");
    cJSON *cjson_version = cJSON_GetObjectItem(cjson_object, "version");

    if (cjson_model == NULL || !cJSON_IsString(cjson_model))
    {
        ESP_LOGE(TAG, "upgrade start failed, model error");
        return ws_server_msg_response(WS_SVR_UPGRADE_RESP_TYPE, UPGRADE_STATE_FAIL, "Upgrade model is error", resp_data);
    }
    model = cjson_model->valuestring;

    if (cjson_version == NULL || !cJSON_IsString(cjson_version))
    {
        ESP_LOGE(TAG, "upgrade start failed, version error");
        return ws_server_msg_response(WS_SVR_UPGRADE_RESP_TYPE, UPGRADE_STATE_FAIL, "Upgrade version is error", resp_data);
    }
    version = cjson_version->valuestring;

    firmware_upgrade_start(model, version);

    return ws_server_upgrade_progress(UPGRADE_STATE_DOING, 0, resp_data);
}

/**
 * @brief 添加实时监控信息
 * @param cjson_obj 监控信息的JSON对象
 * @param id 监控信息的ID, 相同group内根据ID排序显示
 * @param group 监控信息的分组
 * @param name 监控信息的名称
 * @param value 监控信息的值
 * @param unit 监控信息的单位
 * @param desc 监控信息的描述
 */
void add_monitor_info(cJSON *cjson_obj, uint8_t id, const char *group, const char *name,
                        const char *value, const char *unit, const char *desc)
{
    char str_tmp[128] = {0};
    cJSON *cjson_temp = NULL;

    if (cjson_obj == NULL || cJSON_IsNull(cjson_obj) || !cJSON_IsArray(cjson_obj))
    {
        ESP_LOGE(TAG, "add_monitor_info cjson_obj is NULL");
        return;
    }

    cjson_temp = cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "add_monitor_info cjson_temp is NULL");
        return;
    }

    snprintf(str_tmp, sizeof(str_tmp), "%d", id);
    cJSON_AddStringToObject(cjson_temp, "id", str_tmp);
    cJSON_AddStringToObject(cjson_temp, "group", group);
    cJSON_AddStringToObject(cjson_temp, "name", name);
    cJSON_AddStringToObject(cjson_temp, "value", value);
    cJSON_AddStringToObject(cjson_temp, "unit", unit);
    cJSON_AddStringToObject(cjson_temp, "desc", desc);
    cJSON_AddItemToArray(cjson_obj, cjson_temp);
}

int16_t ws_svr_msg_monitor_info_get_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_MONITOR_INFO_GET_RESP_TYPE   "monitorInfo"
    int16_t ret = 0;
    cJSON *cjson_resp = NULL;
    cJSON *cjson_data = NULL;
    cJSON *cjson_temp = NULL;
    cJSON *cjson_device = NULL;
    char str_tmp[128] = {0};

    cjson_resp = cJSON_CreateObject();
    if (cjson_resp == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_resp is NULL");
        return -1;
    }

    cJSON_AddItemToObject(cjson_resp, "type", cJSON_CreateString(WS_SVR_MONITOR_INFO_GET_RESP_TYPE));
    cjson_data = cJSON_CreateObject();
    if (cjson_data == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_data is NULL");
        ret = -2;
        goto err;
    }

    cjson_device = cJSON_CreateArray();
    if (cjson_device == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_ap is NULL");
        ret = -3;
        goto err;
    }

    //Pack
    snprintf(str_tmp, sizeof(str_tmp), "%d", reg_addr_rd_total.mod_reg00100_AppPage1.soc&0xFF);
    add_monitor_info(cjson_device, 1, "Pack", "Soc", str_tmp, "%", "Remaining battery");
    if (0 == reg_addr_rd_total.mod_reg00100_AppPage1.chg_status)
    {
        add_monitor_info(cjson_device, 6, "Pack", "Status", "Idle", "", "Status of the battery pack");
    }
    else if (1 == reg_addr_rd_total.mod_reg00100_AppPage1.chg_status)
    {
        add_monitor_info(cjson_device, 6, "Pack", "Status", "Charging", "", "Status of the battery pack");
    }
    else if (2 == reg_addr_rd_total.mod_reg00100_AppPage1.chg_status)
    {
        add_monitor_info(cjson_device, 6, "Pack", "Status", "Discharging", "", "Status of the battery pack");
    }
    snprintf(str_tmp, sizeof(str_tmp), "%d.%d", reg_addr_rd_total.mod_reg00100_AppPage1.total_voltage/10, reg_addr_rd_total.mod_reg00100_AppPage1.total_voltage%10);
    add_monitor_info(cjson_device, 2, "Pack", "Voltage", str_tmp, "V", "Total Battery Voltage");
    snprintf(str_tmp, sizeof(str_tmp), "%d.%d", reg_addr_rd_total.mod_reg00100_AppPage1.total_current/10, reg_addr_rd_total.mod_reg00100_AppPage1.total_current%10);
    add_monitor_info(cjson_device, 3, "Pack", "Current", str_tmp, "A", "Total Battery Current");
    // snprintf(str_tmp, sizeof(str_tmp), "%d", reg_addr_rd_total.mod_reg00100_AppPage1.chg_full_time);
    // add_monitor_info(cjson_device, 4, "Pack", "Time to full", str_tmp, "min", "Estimated time to full charge");
    // snprintf(str_tmp, sizeof(str_tmp), "%d", reg_addr_rd_total.mod_reg00100_AppPage1.dsg_empty_time);
    // add_monitor_info(cjson_device, 5, "Pack", "Time to empty", str_tmp, "min", "Estimated discharge time");

    //PV
    snprintf(str_tmp, sizeof(str_tmp), "%lu", reg_addr_rd_total.mod_reg00100_AppPage1.PVAllTotalPower);
    add_monitor_info(cjson_device, 1, "PV", "Power", str_tmp, "W", "PV total power");
    snprintf(str_tmp, sizeof(str_tmp), "%lu.%lu", reg_addr_rd_total.mod_reg00100_AppPage1.PvTotalChargingEnergy/10,
                                                reg_addr_rd_total.mod_reg00100_AppPage1.PvTotalChargingEnergy%10);
    add_monitor_info(cjson_device, 1, "PV", "Charging Energy", str_tmp, "kwh", "PV total charging power");
    snprintf(str_tmp, sizeof(str_tmp), "%lu.%lu", reg_addr_rd_total.mod_reg00100_AppPage1.PvToACLoadEnergy/10,
                                                reg_addr_rd_total.mod_reg00100_AppPage1.PvToACLoadEnergy%10);
    add_monitor_info(cjson_device, 1, "PV", "Load Energy", str_tmp, "kwh", "PV to AC total load power");
    snprintf(str_tmp, sizeof(str_tmp), "%lu", reg_addr_rd_total.mod_reg00100_AppPage1.PVToACloadPower);
    add_monitor_info(cjson_device, 1, "PV", "Load Power", str_tmp, "W", "PV to AC total load power");

    // ESP_LOGI(TAG, "PV Power: %lu, PV Charging Energy: %lu, PV Load Energy: %lu, PV Load Power: %lu",
    //         reg_addr_rd_total.mod_reg00100_AppPage1.PVAllTotalPower,
    //         reg_addr_rd_total.mod_reg00100_AppPage1.PvTotalChargingEnergy,
    //         reg_addr_rd_total.mod_reg00100_AppPage1.PvToACLoadEnergy,
    //         reg_addr_rd_total.mod_reg00100_AppPage1.PVToACloadPower);

    cJSON_AddItemToObject(cjson_data, "monitorInfos", cjson_device);
    cJSON_AddItemToObject(cjson_resp, "data", cjson_data);

    *resp_data = (uint8_t *)cJSON_PrintUnformatted(cjson_resp);
    cJSON_Delete(cjson_resp);
    cjson_resp = NULL;

    return strlen((char *)*resp_data);
err:
    if (cjson_resp != NULL)
    {
        cJSON_Delete(cjson_resp);
    }
    if (cjson_data != NULL)
    {
        cJSON_Delete(cjson_data);
    }
    if (cjson_device != NULL)
    {
        cJSON_Delete(cjson_device);
    }

    return ret;
}

/**
 * @brief 处理Wi-Fi Sta查询消息
 * @details json中的key需要与前端保持一致，否则存在解析问题
 * @param[in] cjson_object 待处理cJSON对象消息
 * @param[out] resp_data 响应数据
 * @return 响应数据长度
 */
int16_t ws_svr_msg_config_get_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_CONFIG_GET_RESP_TYPE   "getConfigRsp"

    int16_t ret = 0;
    cJSON *cjson_resp = NULL;
    cJSON *cjson_data = NULL;
    cJSON *cjson_temp = NULL;
    char ip_str[16] = {0};

    ESP_LOGI(TAG, "ws_svr_msg_wifi_get_handler");
    cjson_resp = cJSON_CreateObject();
    if (cjson_resp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_resp is NULL");
        return -1;
    }

    cJSON_AddItemToObject(cjson_resp, "type", cJSON_CreateString(WS_SVR_CONFIG_GET_RESP_TYPE));

    cjson_data = cJSON_CreateObject();
    if (cjson_data == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_data is NULL");
        ret = -2;
        goto err;
    }

    /* WiFi AP信息 */
    cjson_temp = cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_ap is NULL");
        ret = -3;
        goto err;
    }

    cJSON_AddItemToObject(cjson_temp, "enable", cJSON_CreateBool(SetData.dev_info_t.on_off.bit.wifi_ap_enable == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "ssid", cJSON_CreateString((char *)SetData.dev_info_t.wifi_ap_ssid));
    // cJSON_AddItemToObject(cjson_temp, "password", cJSON_CreateString(SetData.dev_info_t.wifi_ap_password));
    cJSON_AddItemToObject(cjson_temp, "authmode", cJSON_CreateNumber(SetData.dev_info_t.wifi_ap_auth));
    cJSON_AddItemToObject(cjson_data, "wifiAp", cjson_temp);

    /* WiFi  Sta1信息 */
    cjson_temp = cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_sta1 is NULL");
        ret = -4;
        goto err;
    }
    cJSON_AddItemToObject(cjson_temp, "enable", cJSON_CreateBool(SetData.dev_info_t.on_off.bit.wifi_enable == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "ssid", cJSON_CreateString((char *)SetData.dev_info_t.wifi_sta_ssid));
    // cJSON_AddItemToObject(cjson_temp, "password", cJSON_CreateString(SetData.dev_info_t.wifi_sta_password));
    cJSON_AddItemToObject(cjson_temp, "authmode", cJSON_CreateNumber(SetData.dev_info_t.wifi_sta_auth));
    cJSON_AddItemToObject(cjson_temp, "static_ip_enable", cJSON_CreateBool(SetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en == 1? true : false));
    if (1 == SetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en)
    {
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta1_ip, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "ip", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta1_gw, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "gateway", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta1_mask, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "netmask", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta1_dns1, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "dns1", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta1_dns2, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "dns2", cJSON_CreateString(ip_str));
    }
    cJSON_AddItemToObject(cjson_data, "wifiSta1", cjson_temp);

    /* WiFi  Sta2信息 */
    cjson_temp = cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_sta2 is NULL");
        ret = -5;
        goto err;
    }
    cJSON_AddItemToObject(cjson_temp, "enable", cJSON_CreateBool(SetData.dev_info_t.wifi_mul_sta_en.sta2_en == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "ssid", cJSON_CreateString((char *)SetData.dev_info_t.wifi_sta2_ssid));
    // cJSON_AddItemToObject(cjson_temp, "password", cJSON_CreateString((char *)SetData.dev_info_t.wifi_sta2_password));
    cJSON_AddItemToObject(cjson_temp, "authmode", cJSON_CreateNumber(SetData.dev_info_t.wifi_sta2_auth));
    cJSON_AddItemToObject(cjson_temp, "static_ip_enable", cJSON_CreateBool(SetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en == 1? true : false));
    if (1 == SetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en)
    {
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta2_ip, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "ip", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta2_gw, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "gateway", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta2_mask, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "netmask", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta2_dns1, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "dns1", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta2_dns2, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "dns2", cJSON_CreateString(ip_str));
    }
    cJSON_AddItemToObject(cjson_data, "wifiSta2", cjson_temp);

    /* WiFi  Sta3信息 */
    cjson_temp= cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_sta3 is NULL");
        ret = -6;
        goto err;
    }
    cJSON_AddItemToObject(cjson_temp, "enable", cJSON_CreateBool(SetData.dev_info_t.wifi_mul_sta_en.sta3_en == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "ssid", cJSON_CreateString((char *)SetData.dev_info_t.wifi_sta3_ssid));
    // cJSON_AddItemToObject(cjson_temp, "password", cJSON_CreateString((char *)SetData.dev_info_t.wifi_sta3_password));
    cJSON_AddItemToObject(cjson_temp, "authmode", cJSON_CreateNumber(SetData.dev_info_t.wifi_sta3_auth));
    cJSON_AddItemToObject(cjson_temp, "static_ip_enable", cJSON_CreateBool(SetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en == 1? true : false));
    if (1 == SetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en)
    {
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta3_ip, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "ip", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta3_gw, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "gateway", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta3_mask, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "netmask", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta3_dns1, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "dns1", cJSON_CreateString(ip_str));
        inet_ntop(AF_INET, &SetData.dev_info_t.wifi_sta3_dns2, ip_str, sizeof(ip_str));
        cJSON_AddItemToObject(cjson_temp, "dns2", cJSON_CreateString(ip_str));
    }
    cJSON_AddItemToObject(cjson_data, "wifiSta3", cjson_temp);

    /* Bluetooth信息 */
    cjson_temp= cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_ble is NULL");
        ret = -7;
        goto err;
    }
    cJSON_AddItemToObject(cjson_temp, "enable", cJSON_CreateBool(SetData.dev_info_t.on_off.bit.ble_enable == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "adv_enable", cJSON_CreateBool(SetData.dev_info_t.ble_protocol.adv_en == 1? true : false));
    // cJSON_AddItemToObject(cjson_temp, "password", cJSON_CreateString((char *)SetData.dev_info_t.app_password));
    // HEX转换成字符串格式
    char *hex_str = hex_array_to_string(SetData.dev_info_t.bles_adv_key, 16);
    cJSON_AddItemToObject(cjson_temp, "adv_key", cJSON_CreateString(hex_str));
    cJSON_AddItemToObject(cjson_data, "bluetooth", cjson_temp);
    free(hex_str);
    hex_str = NULL;
#if 0
    /* Ethernet信息 */
    cjson_temp= cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_eth is NULL");
        ret = -7;
        goto err;
    }
    cJSON_AddItemToObject(cjson_temp, "enable", cJSON_CreateBool(SetData.dev_info_t.on_off.bit.Eth_enable == 1? true : false));
    cJSON_AddItemToObject(cjson_data, "ethernet", cjson_temp);
#endif
    /* Open MQTT信息 */
    cjson_temp= cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response cjson_mqtt is NULL");
        ret = -7;
        goto err;
    }
    cJSON_AddItemToObject(cjson_temp, "enable",
            cJSON_CreateBool(SetData.dev_info_t.open_mqtt_enable.enable == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "crypt_enable",
            cJSON_CreateBool(SetData.dev_info_t.open_mqtt_enable.crypt_en == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "ctrl_enable",
            cJSON_CreateBool(SetData.dev_info_t.open_mqtt_enable.ctrl_en == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "report_enable",
            cJSON_CreateBool(SetData.dev_info_t.open_mqtt_enable.report_en == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "url", cJSON_CreateString((char *)SetData.dev_info_t.Net_Server_address));
    cJSON_AddItemToObject(cjson_temp, "report_cycle", cJSON_CreateNumber(SetData.dev_info_t.open_mqtt_report_cycle));
    cJSON_AddItemToObject(cjson_data, "mqtt", cjson_temp);

    /*Modbus TCP信息 */
    cjson_temp= cJSON_CreateObject();
    if (cjson_temp == NULL)
    {
        ESP_LOGE(TAG, "ws_server_msg_response modbus tcp is NULL");
        ret = -7;
        goto err;
    }
    cJSON_AddItemToObject(cjson_temp, "enable",
            cJSON_CreateBool(SetData.dev_info_t.modbus_tcp_enable.enable == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "crypt_enable",
            cJSON_CreateBool(SetData.dev_info_t.modbus_tcp_enable.crypt_en == 1? true : false));
    cJSON_AddItemToObject(cjson_temp, "port", cJSON_CreateNumber(SetData.dev_info_t.modbus_tcp_port));
    cJSON_AddItemToObject(cjson_data, "modbus_tcp", cjson_temp);

    cJSON_AddItemToObject(cjson_resp, "data", cjson_data);
    *resp_data = (uint8_t *)cJSON_PrintUnformatted(cjson_resp);
    cJSON_Delete(cjson_resp);
    cjson_resp = NULL;

    return strlen((char *)*resp_data);

err:
    if (cjson_resp != NULL)
    {
        cJSON_Delete(cjson_resp);
    }
    if (cjson_data != NULL)
    {
        cJSON_Delete(cjson_data);
    }
    if (cjson_temp != NULL)
    {
        cJSON_Delete(cjson_temp);
    }

    return ret;
}

/**
 * @brief 处理设备信息查询消息 device_info
 * @details json中的key需要与前端保持一致，否则存在解析问题
 * @param[in] cjson_object 待处理cJSON对象消息
 * @param[out] resp_data 响应数据
 * @return 响应数据长度
 */
int16_t ws_svr_msg_device_info_get_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_DEVICE_INFO_GET_RESP_TYPE   "device_info"
    int16_t ret = 0;
    cJSON *cjson_resp = NULL;
    cJSON *cjson_data = NULL;
    cJSON *cjson_temp = NULL;
    cJSON *cjson_device = NULL;
    char str_tmp[128] = {0};

    cjson_resp = cJSON_CreateObject();
    if (cjson_resp == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_resp is NULL");
        return -1;
    }

    cJSON_AddItemToObject(cjson_resp, "type", cJSON_CreateString(WS_SVR_DEVICE_INFO_GET_RESP_TYPE));
    cjson_data = cJSON_CreateObject();
    if (cjson_data == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_data is NULL");
        ret = -2;
        goto err;
    }

    cjson_device = cJSON_CreateArray();
    if (cjson_device == NULL)
    {
        ESP_LOGE(TAG, "ws server device info cjson_ap is NULL");
        ret = -3;
        goto err;
    }

    //IOT
    for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; ++node_id)
    {
        if (Inv[node_id].mod_reg11000_IOT_info.iot_sn > 0)
        {
            cjson_temp = cJSON_CreateObject();
            snprintf(str_tmp, sizeof(str_tmp), "%lld", Inv[node_id].mod_reg11000_IOT_info.iot_sn);
            cJSON_AddStringToObject(cjson_temp, "sn", str_tmp);
            snprintf(str_tmp, sizeof(str_tmp), "V%lu", Inv[node_id].mod_reg11000_IOT_info.software_ver);
            cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
            cJSON_AddStringToObject(cjson_temp, "model", Inv[node_id].mod_reg11000_IOT_info.iot_type);
            cJSON_AddItemToArray(cjson_device, cjson_temp);

            ESP_LOGW(TAG, "----- IoT info:%d, sn:%lld", node_id, Inv[node_id].mod_reg11000_IOT_info.iot_sn);
        }
    }

    //D400S
    if(reals.online_D400S_num)
    {
        for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
        {
            if (Inv_D400S[node_id].mod_reg11000_IOT_info.iot_sn > 0)
            {
                cjson_temp = cJSON_CreateObject();
                snprintf(str_tmp, sizeof(str_tmp), "%lld", Inv_D400S[node_id].mod_reg11000_IOT_info.iot_sn);
                cJSON_AddStringToObject(cjson_temp, "sn", str_tmp);
                snprintf(str_tmp, sizeof(str_tmp), "V%lu", Inv_D400S[node_id].mod_reg11000_IOT_info.software_ver);
                cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
                cJSON_AddStringToObject(cjson_temp, "model", Inv_D400S[node_id].mod_reg11000_IOT_info.iot_type);
                cJSON_AddItemToArray(cjson_device, cjson_temp);

                ESP_LOGW(TAG, "----- Inv_D400S info:%d, sn:%lld", node_id, Inv_D400S[node_id].mod_reg11000_IOT_info.iot_sn);
            }
        }
    }

    if (reals.online_ACHUB_num )
    {
        //ACHUB
        for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
        {
            if (Inv[node_id].mod_reg01100_Inv_base.InvSN > 0)
            {
                cjson_temp = cJSON_CreateObject();
                snprintf(str_tmp, sizeof(str_tmp), "%lld", Inv[node_id].mod_reg01100_Inv_base.InvSN);
                cJSON_AddStringToObject(cjson_temp, "sn", str_tmp);

                //AP300只有ARM DSP BMS
                snprintf(str_tmp, sizeof(str_tmp), "V%lu,V%lu,V%lu",
                                                Inv[node_id].mod_reg01100_Inv_base.soft[0].version,
                                                Inv[node_id].mod_reg01100_Inv_base.soft[1].version,
                                                Inv[node_id].mod_reg01100_Inv_base.soft[2].version);
                cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
                cJSON_AddStringToObject(cjson_temp, "model", Inv[node_id].mod_reg01100_Inv_base.InvType);
                cJSON_AddItemToArray(cjson_device, cjson_temp);

                ESP_LOGW(TAG, "----- ACHUB info node_id:%d, sn:%lld", node_id, Inv[node_id].mod_reg01100_Inv_base.InvSN);
            }
        }
    }
    else
    {
        //ARM、DSP、DCHUB
        for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
        {
            for (int inv_id = 0; inv_id < INV_MAX_NUM; ++inv_id)
            {
                if (Inv_can[node_id].inv_data[inv_id].inv_about.dev_sn > 0)
                {
                    cjson_temp = cJSON_CreateObject();
                    snprintf(str_tmp, sizeof(str_tmp), "%lld", Inv_can[node_id].inv_data[inv_id].inv_about.dev_sn);
                    cJSON_AddStringToObject(cjson_temp, "sn", str_tmp);
                    snprintf(str_tmp, sizeof(str_tmp), "V%lu,V%lu,V%lu",
                                                    Inv_can[node_id].inv_data[0].inv_about.soft[0].version,
                                                    Inv_can[node_id].inv_data[inv_id].inv_about.soft[1].version,
                                                    Inv_can[node_id].inv_data[inv_id].inv_about.soft[2].version);
                    cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
                    cJSON_AddStringToObject(cjson_temp, "model", Inv_can[node_id].inv_data[inv_id].inv_about.dev_type);
                    cJSON_AddItemToArray(cjson_device, cjson_temp);

                    ESP_LOGW(TAG, "----- ARM、DSP、DCHUB info node_id:%d, inv_id:%d, sn:%lld", node_id, inv_id, Inv_can[node_id].inv_data[inv_id].inv_about.dev_sn);
                }
            }
        }
    }

    //BMS
    for(int node_id = 0; node_id < DEFAULT_PACK_TYPE_NUM; node_id++)
    {
        if (Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.sn_code > 0)
        {
            cjson_temp = cJSON_CreateObject();
            snprintf(str_tmp, sizeof(str_tmp), "%lld", Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.sn_code);
            cJSON_AddStringToObject(cjson_temp, "sn", str_tmp);
            snprintf(str_tmp, sizeof(str_tmp), "V%lu,V%lu,V%lu,V%lu,V%lu,V%lu,V%lu,V%lu,V%lu,V%lu",
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[0].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[1].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[2].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[3].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[4].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[5].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[6].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[7].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[8].version,
                                            Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.soft[9].version);
            cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
            cJSON_AddStringToObject(cjson_temp, "model", Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.type_ascii);
            cJSON_AddItemToArray(cjson_device, cjson_temp);

            ESP_LOGW(TAG, "----- BMS info node_id:%d, sn:%lld", node_id, Inv_Pack_Slave[node_id].mod_reg06100_Pack_each.sn_code);
        }
    }

    //PACK BMS
    for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
    {
        if (Inv[node_id].mod_reg06100_Pack_each.sn_code > 0)
        {
            cjson_temp = cJSON_CreateObject();
            snprintf(str_tmp, sizeof(str_tmp), "%lld", Inv[node_id].mod_reg06100_Pack_each.sn_code);
            cJSON_AddStringToObject(cjson_temp, "sn", str_tmp);

            if((strcmp(Inv[node_id].mod_reg06100_Pack_each.type_ascii,"B300")==0)
                ||(strcmp(Inv[node_id].mod_reg06100_Pack_each.type_ascii,"B300S")==0))
            {
                for (int j = 0 ; j < 6 ; j++ )
                {
                    if (Inv[node_id].mod_reg01100_Inv_base.soft[j].type == DEVICE_PACK_BMS)
                    {
                        snprintf(str_tmp, sizeof(str_tmp), "V%lu", Inv[node_id].mod_reg01100_Inv_base.soft[j].version);
                        break;
                    }
                }
            }
            else if(strcmp(Inv[node_id].mod_reg06100_Pack_each.type_ascii,"B300K")==0)
            {
                snprintf(str_tmp, sizeof(str_tmp), "V%lu", Inv[node_id].mod_reg06100_Pack_each.soft[0].version);
            }
            else
            {
                snprintf(str_tmp, sizeof(str_tmp), "Unknown");
            }

            cJSON_AddStringToObject(cjson_temp, "version", str_tmp);
            cJSON_AddStringToObject(cjson_temp, "model", Inv[node_id].mod_reg06100_Pack_each.type_ascii);
            cJSON_AddItemToArray(cjson_device, cjson_temp);

            ESP_LOGW(TAG, "----- BMS info node_id:%d, sn:%lld", node_id, Inv[node_id].mod_reg06100_Pack_each.sn_code);
        }
    }

    // for (uint8_t i = 0; i < 8; i++) {
    //     cjson_temp = cJSON_CreateObject();
    //     cJSON_AddStringToObject(cjson_temp, "sn", "123123");
    //     cJSON_AddStringToObject(cjson_temp, "version", "V1.0.0");
    //     cJSON_AddStringToObject(cjson_temp, "model", "AC380");
    //     cJSON_AddItemToArray(cjson_device, cjson_temp);
    // }
    cJSON_AddItemToObject(cjson_data, "devices", cjson_device);
    cJSON_AddItemToObject(cjson_resp, "data", cjson_data);

    *resp_data = (uint8_t *)cJSON_PrintUnformatted(cjson_resp);
    cJSON_Delete(cjson_resp);
    cjson_resp = NULL;

    return strlen((char *)*resp_data);
err:
    if (cjson_resp != NULL)
    {
        cJSON_Delete(cjson_resp);
    }
    if (cjson_data != NULL)
    {
        cJSON_Delete(cjson_data);
    }
    if (cjson_device != NULL)
    {
        cJSON_Delete(cjson_device);
    }

    return ret;
}

/**
 * @brief 验证登录信息
 * @param cjson_object 待处理cJSON对象消息
 * @param[out] resp_data 响应数据
 * @return 响应数据长度
 */
int16_t ws_svr_msg_login_handler(cJSON *cjson_object, uint8_t **resp_data)
{
#define WS_SVR_MD_LOGIN_RESP_TYPE   "loginRsp"

    char *username = NULL;
    char *password = NULL;

    cJSON *cjson_user_name = cJSON_GetObjectItem(cjson_object, "userName");
    cJSON *cjson_pwd = cJSON_GetObjectItem(cjson_object, "password");

    if (cjson_user_name == NULL || !cJSON_IsString(cjson_user_name))
    {
        ESP_LOGE(TAG, "modbus tcp set enable failed");
        return ws_server_msg_response(WS_SVR_MD_LOGIN_RESP_TYPE, 1, "UserName is empty", resp_data);
    }
    username = cjson_user_name->valuestring;

    if (cjson_pwd == NULL || !cJSON_IsString(cjson_pwd))
    {
        ESP_LOGE(TAG, "modbus tcp crypt enable failed");
        return ws_server_msg_response(WS_SVR_MD_LOGIN_RESP_TYPE, 2, "Password is empty", resp_data);
    }
    password = cjson_pwd->valuestring;

    if (0 != strncmp(username, "admin", strlen(username)) || strlen(username) != strlen("admin"))
    {
        ESP_LOGE(TAG, "username is error");
        return ws_server_msg_response(WS_SVR_MD_LOGIN_RESP_TYPE, 1, "UserName is error", resp_data);
    }

    // 只有设置了app_password才需要验证
    if(SetData.dev_info_t.app_password[0] != '\0')
    {
        int pwd_len = 0;
        // 计算APP密码长度
        for (pwd_len = 0; pwd_len < sizeof(SetData.dev_info_t.app_password); pwd_len++)
        {
            if (SetData.dev_info_t.app_password[pwd_len] == '\0')
            {
                break;
            }
        }

        // 密码不匹配
        if( strlen(password) != pwd_len
            || memcmp(SetData.dev_info_t.app_password, password, strlen(password)) != 0)
        {
            ESP_LOGE(TAG, "Login password is error");
            return ws_server_msg_response(WS_SVR_MD_LOGIN_RESP_TYPE, 3, "Password is error", resp_data);
        }
    }

    return ws_server_msg_response(WS_SVR_MD_LOGIN_RESP_TYPE, 0, "Success", resp_data);
}

/**
 * @brief 注册消息处理函数
 * @param msg_type 消息类型
 * @param handler 消息处理函数
 * @return 无
 */
void ws_server_msg_register(char *msg_type, ws_svr_msg_handler handler)
{
    if (msg_type == NULL || handler == NULL)
    {
        ESP_LOGE(TAG, "websocket message type or handler is NULL");
        return;
    }

    for (int i = 0; i < WS_SERVER_MSG_MAX; i++)
    {
        if (ws_server_msg[i].handler == NULL)
        {
            strncpy((char *)ws_server_msg[i].msg_type, msg_type, sizeof(ws_server_msg[i].msg_type) - 1);
            ws_server_msg[i].handler = handler;
            ESP_LOGI(TAG, "websocket register message:%s", msg_type);
            return;
        }
    }

    ESP_LOGE(TAG, "websocket register message failed, ws_server_msg is full");
}

/**
 * @brief 消息类型字段和处理函数的map映射
 * @notice ws_server_msg最大值为WS_SERVER_MSG_MAX(15)
 */
void ws_server_msg_init(void)
{
    ws_server_msg_register("login", ws_svr_msg_login_handler);
    ws_server_msg_register("getDevicesInfo", ws_svr_msg_device_info_get_handler);
    ws_server_msg_register("getConfig", ws_svr_msg_config_get_handler);
    ws_server_msg_register("setWiFiSta1", ws_svr_msg_wifi_sta1_set_handler);
    // ws_server_msg_register("setWiFiSta2", ws_svr_msg_wifi_sta2_set_handler);
    // ws_server_msg_register("setWiFiSta3", ws_svr_msg_wifi_sta3_set_handler);
    ws_server_msg_register("setWiFiAp", ws_svr_msg_wifi_ap_set_handler);
    ws_server_msg_register("setBle", ws_svr_msg_ble_server_set_handler);
    // ws_server_msg_register("setMqtt", ws_svr_msg_mqtt_set_handler);
    ws_server_msg_register("setModbusTcp", ws_svr_msg_modbustcp_set_handler);
    ws_server_msg_register("getVersion", ws_svr_msg_version_get_handler);
    ws_server_msg_register("upgradeStart", ws_svr_msg_upgrade_handler);
    ws_server_msg_register("getMonitorInfo", ws_svr_msg_monitor_info_get_handler);
}

/**
 * @brief 检查接收数据的合法性
 * @details 主要检查是否符合json, 以及是否包含必要的type和data字段
 * @param msg 待检查的消息
 * @return cJSON对象指针, 成功返回cJSON对象, 失败返回NULL
 */
cJSON *ws_server_data_check(ws_msg_t *msg)
{
    if (NULL == msg || NULL == msg->data || 0 == msg->len)
    {
        ESP_LOGE(TAG, "ws_server_data_check msg is NULL");
        return NULL;
    }

    /* 格式化为cJSON格式 */
    cJSON *cjson_object = cJSON_Parse(msg->data);
    if (cjson_object == NULL)
    {
        ESP_LOGE(TAG, "json parse failed");
        return NULL;
    }

    /* type: 消息类型字段; 根据此字段匹配对应的处理函数 */
    const cJSON *cjson_type = cJSON_GetObjectItem(cjson_object, "type");
    if (cjson_type == NULL || !cJSON_IsString(cjson_type))
    {
        ESP_LOGE(TAG, "json parse failed, without type or type is not a string");
        cJSON_Delete(cjson_object);
        return NULL;
    }

    /* data: 消息内容; 不同消息内容不同，由各消息处理函数自行处理 */
    const cJSON *cjson_data = cJSON_GetObjectItem(cjson_object, "data");
    if (cjson_data == NULL || !cJSON_IsObject(cjson_data))
    {
        ESP_LOGE(TAG, "ws server data parse failed, without data or data is not a string");
        cJSON_Delete(cjson_object);
        return NULL;
    }

    return cjson_object;
}

/**
 * @brief 处理接收的数据, 根据消息类型, 调用对应的处理函数
 * @param msg 待处理的消息
 * @param resp_data 响应数据
 * @return >0: 响应数据长度
 *        <=0: 失败错误码
 */
int16_t ws_server_data_handler(ws_msg_t *msg, uint8_t **resp_data)
{
    int16_t ret = 0;

    cJSON *cjson_object = ws_server_data_check(msg);
    if (cjson_object == NULL)
    {
        ESP_LOGE(TAG, "ws_server_data_check failed");
        return -1;
    }

    // ws_server_data_check()中已经检测了type和data字段，此处不再重复检查
    const cJSON *cjson_type = cJSON_GetObjectItem(cjson_object, "type");
    const cJSON *cjson_data = cJSON_GetObjectItem(cjson_object, "data");

    const char *msg_type = cjson_type->valuestring;
    for (int i = 0; i < WS_SERVER_MSG_MAX; i++)
    {
        if (strcmp(msg_type, (char *)ws_server_msg[i].msg_type) == 0)
        {
            ESP_LOGI(TAG, "ws server data handler msg type:%s", msg_type);
            if (ws_server_msg[i].handler == NULL)
            {
                ESP_LOGE(TAG, "ws server data handler msg type:%s handler is NULL", msg_type);
                ret = -2;
                break;
            }

            // 调用对应的处理函数
            ret = ws_server_msg[i].handler(cjson_data, resp_data);
            break;
        }
    }

    cJSON_Delete(cjson_object);

    return ret;
}

void ws_server_ping(void)
{
    for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++)
    {
        if (ws_client_list[i] > 0)
        {
            httpd_ws_frame_t ws_pkt = {0};
            ws_pkt.type = HTTPD_WS_TYPE_PING;
            ws_pkt.payload = NULL;
            ws_pkt.len = 0;
            esp_err_t err = httpd_ws_send_frame_async(web_server_handle, ws_client_list[i], &ws_pkt);
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "WebSocket PING send failed, client[%d]:%d, err:0x%x", i, ws_client_list[i], err);
            }
            else
            {
                ESP_LOGD(TAG, "WebSocket PING sent to client[%d]:%d", i, ws_client_list[i]);
            }
        }
    }
}

uint16_t web_server_config_push(void)
{
    uint8_t *resp_data = NULL;
    int16_t ret = 0;

    ret = ws_svr_msg_config_get_handler(NULL, &resp_data);
    if (0 < ret)
    {
        ESP_LOGI(TAG, "web server push config:%s", resp_data);
        // 发送响应数据
        for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++)
        {
            if (ws_client_list[i] > 0)
            {
                ws_server_send_async(resp_data, ret, ws_client_list[i]);
            }
        }
    }

    if (resp_data)
    {
        free(resp_data);
        resp_data = NULL;
    }

    return ret;
}

uint16_t web_server_monitor_update(void)
{
    uint8_t *resp_data = NULL;
    int16_t ret = 0;

    ret = ws_svr_msg_monitor_info_get_handler(NULL, &resp_data);
    if (0 < ret)
    {
        // ESP_LOGI(TAG, "web server update monitor:%s", resp_data);
        // 发送响应数据
        for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++)
        {
            if (ws_client_list[i] > 0)
            {
                ws_server_send_async(resp_data, ret, ws_client_list[i]);
            }
        }
    }

    if (resp_data)
    {
        free(resp_data);
        resp_data = NULL;
    }

    return ret;
}

/**
 * @brief 处理websocket接收的数据
 * @details 从接收队列中获取数据，并根据消息类型字段匹配并调用对应的处理函数
 */
void web_server_task(void)
{
    static uint16_t update_monitor_info_timer = 0;
    ws_msg_t recv_buffer = {NULL, 0, 0};

    if(ws_server_rece_queue && xQueueReceive(ws_server_rece_queue, &recv_buffer, 0)== pdTRUE)
    {
        ESP_LOGI(TAG, "socket : %d\tdata len : %d\tpayload : %s", recv_buffer.client_socket,
                                    recv_buffer.len, recv_buffer.data);

        // 处理接收的数据
        uint8_t *resp_data = NULL;
        int16_t len = ws_server_data_handler(&recv_buffer, &resp_data);
        if (len > 0)
        {
            // 发送响应数据
            ws_server_send_async(resp_data, len, recv_buffer.client_socket);
            ESP_LOGI(TAG, "ws server send resp data:%s", resp_data);
        }

        if (resp_data != NULL)
        {
            free(resp_data);
            resp_data = NULL;
        }
    }

    if (recv_buffer.data != NULL)
    {
        free(recv_buffer.data);
        recv_buffer.data = NULL;
    }

    /* 更新实时监控信息 */
    if (++update_monitor_info_timer >=300)
    {
        update_monitor_info_timer = 0;
        web_server_monitor_update(); // 更新监控信息
    }
}

void web_server_upgrade_state_report(uint8_t result, uint8_t pct)
{
    uint8_t *resp_data = NULL;
    int16_t ret = 0;

    ret = ws_server_upgrade_progress((uint16_t)result, (uint16_t)pct, &resp_data);
    if (0 < ret)
    {
        ESP_LOGI(TAG, "web server push upgrade progress:%s", resp_data);
        // 发送响应数据
        for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++)
        {
            if (ws_client_list[i] > 0)
            {
                ws_server_send_async(resp_data, ret, ws_client_list[i]);
            }
        }
    }

    if (resp_data)
    {
        free(resp_data);
        resp_data = NULL;
    }
}

void web_server_version_report(void)
{
    uint8_t *resp_data = NULL;
    int16_t ret = 0;

    ret = ws_svr_msg_version_get_handler(NULL, &resp_data);
    if (0 < ret)
    {
        ESP_LOGI(TAG, "web server push version info:%s", resp_data);
        // 发送响应数据
        for (size_t i = 0; i < WS_CLIENT_QUANTITY_ASTRICT; i++)
        {
            if (ws_client_list[i] > 0)
            {
                ws_server_send_async(resp_data, ret, ws_client_list[i]);
            }
        }
    }

    if (resp_data)
    {
        free(resp_data);
        resp_data = NULL;
    }
}

/**
 * @brief 初始化web服务器
 * @details 启动httpd服务器，注册uri处理程序，创建接收和发送队列，创建接收处理任务
 */
void web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80; //设置服务器端口为80
    /* 设置链路保活参数 */
    config.keep_alive_enable = true;
    config.keep_alive_idle = 30;        //空闲5秒后发送keep-alive包
    config.keep_alive_interval = 10;    //发送keep-alive包的超时间隔时间为5秒
    config.keep_alive_count = 3;        //发送keep-alive包的重试次数为3次，超过次数未响应则断开连接

    // 启动httpd服务器
    if (httpd_start(&web_server_handle, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "web_server_start\r\n");
        httpd_register_err_handler(web_server_handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else
    {
        ESP_LOGE(TAG, "Error starting ws server!");
        return;
    }

    esp_event_handler_instance_register(ESP_HTTP_SERVER_EVENT, ESP_EVENT_ANY_ID, &ws_event_handler,NULL,NULL);//注册处理程序

    /* 注册http uri处理程序, HTTPD_DEFAULT_CONFIG配置中最大支持8个uri */
    httpd_register_uri_handler(web_server_handle, &home);
    httpd_register_uri_handler(web_server_handle, &ws);
    // httpd_register_uri_handler(web_server_handle, &upgrade_iot);
    httpd_register_uri_handler(web_server_handle, &ca_cert_file);
    httpd_register_uri_handler(web_server_handle, &server_cert_file);
    httpd_register_uri_handler(web_server_handle, &server_key_file);

    /*创建接收队列*/
    ws_server_rece_queue = xQueueCreate(3 , sizeof(ws_msg_t));
    if (ws_server_rece_queue == NULL )
    {
        ESP_LOGE(TAG, "ws_server_rece_queue ERROR\r\n");
    }

    ws_server_msg_init();
}
