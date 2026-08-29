/**
 * log_redirect.c
 * 互斥模式日志重定向系统
 * 特性：1. 串口/UDP二选一 2. UDP模式时才创建socket 3. 串口模式时完全释放资源
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "parameter.h"
#include "comm_define.h"

#include "log_redirect.h"
#include "aiot_log.h"

#if CONFIG_LOG_UDP_REDIRECT == 1
// 新增：使用单独任务发送网络日志（避免在 vprintf 中做网络 I/O）
#define CONFIG_LOG_REDIRECT_TASK_ENABLE
#endif

const char *LogTypeString[] = 
{
    "[DEV_DATA_RECORD]", 	// bit0
	"[md_protocol]", 		// bit1
	"md_data", 				// bit2
	"BLE_MODEBUS",			// bit3
	"[ble_adv]", 			// bit4
	"[ble_dev]", 			// bit5
	"[APP_BLE]", 			// bit6
	"[CAN_PROTOCOL]", 		// bit7
	"[CAN_DATA]", 			// bit8
	"[wlcc_process]",		// bit9
    "[wlcc_interface]", 	// bit10
    "[MD_METER]", 			// bit11
    "[MD_S1]",				// bit12
    "[MD_OTHER_INV]",		// bit13
    "[EMS_AC2AC]",			// bit14
    "[APP_AC2AC]",			// bit15
};

const uint8_t LogTypeCnt = sizeof(LogTypeString) / sizeof(LogTypeString[0]);

static void udp_log_send(const char *data, int len);

// ==================== 配置 ====================
#define LOG_UDP_PORT      8888
#define LOG_BUFFER_SIZE   256
#define LOG_SERVER_IP     "192.168.120.49"
#define LOG_QUEUE_LEN     16

#define LOG_RETRY_MAX     3

// 保存ESP-IDF原始输出函数
static int (*esp_original_vprintf)(const char *, va_list) = NULL;

/* 安全日志：绕过 esp_log 回调，直接调用原始 vprintf，避免重入 */
static void safe_log(const char *fmt, ...)
{
    if (esp_original_vprintf == NULL) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    esp_original_vprintf(fmt, ap);
    va_end(ap);
}

/* 便捷宏：按 ESP_LOG 风格构造格式并走 safe_log */
#define SAFE_LOG_LEVEL(level_char, tag, fmt, ...) \
    do { \
        if (esp_original_vprintf) { \
            safe_log("%c (%u) %s: " fmt "\n", (level_char), (unsigned)esp_log_timestamp(), (tag), ##__VA_ARGS__); \
        } \
    } while(0)

// 日志系统全局状态
static struct {
    log_mode_t current_mode;      // 当前模式
    log_mode_t target_mode;       // 目标模式（用于安全切换）
    char server_ip[16];           // UDP服务器IP
    int server_port;              // UDP服务器端口
    int udp_socket;               // UDP socket句柄（-1表示未创建）
    bool is_initialized;          // 系统是否已初始化
    bool is_switching;            // 是否正在切换模式
    network_status_t network_status;
    SemaphoreHandle_t lock;       // 线程安全锁
    struct {
        int failure_count;           // 连续失败次数（成功时清零）
        TickType_t last_failure_time; // 上次失败时间
        bool is_recovering;          // 是否正在恢复中
        TickType_t recovery_start;   // 恢复开始时间
    } smart_recovery;
} log_sys = {
    .current_mode = LOG_MODE_SERIAL,
    .target_mode = LOG_MODE_SERIAL,
    .server_ip = LOG_SERVER_IP,
    .server_port = LOG_UDP_PORT,
    .udp_socket = -1,
    .is_initialized = false,
    .is_switching = false,
    .network_status = NETWORK_DISCONNECTED,
    .lock = NULL
};

typedef struct {
    uint16_t len;
    char data[LOG_BUFFER_SIZE];
} log_item_t;

/* 简化的阻塞型 UDP 发送子系统（任务阻塞在 xQueueReceive，退出通过发送 NULL 哨兵） */
static QueueHandle_t log_queue = NULL;
static TaskHandle_t udp_sender_task_handle = NULL;

/* 任务：阻塞等待队列项，收到 NULL 指针时退出；退出前清理残留项并删除队列 */
static void udp_sender_task(void *pv)
{
    log_item_t *item_ptr;
    for (;;) {
        if (xQueueReceive(log_queue, &item_ptr, portMAX_DELAY) == pdTRUE) {
            if (item_ptr == NULL) {
                break; /* 退出信号 */
            }

#if CONFIG_LOG_UDP_REDIRECT == 1            
			udp_log_send(item_ptr->data, item_ptr->len);
#elif CONFIG_LOG_UDP_REDIRECT == 2
            supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
            if (1 == resource.log) {
                aiot_log_push((const uint8_t *)item_ptr->data, (uint16_t)item_ptr->len, LOG_TYPE_LOG);
            }
#endif			
            free(item_ptr);
        }
    }

    /* 退出前清空并释放队列中残留项（非阻塞） */
    while (xQueueReceive(log_queue, &item_ptr, 0) == pdTRUE) {
        if (item_ptr) free(item_ptr);
    }

    /* 删除队列并退出任务 */
    vQueueDelete(log_queue);
    log_queue = NULL;
    udp_sender_task_handle = NULL;
    vTaskDelete(NULL);
}

/* 启动（幂等） */
static void start_udp_sender(void)
{
    if (log_queue == NULL) {
        log_queue = xQueueCreate(LOG_QUEUE_LEN, sizeof(log_item_t *));
        if (log_queue == NULL) return;
    }
    if (udp_sender_task_handle == NULL) {
        xTaskCreate(udp_sender_task, "udp_sender", 3072, NULL, 1, &udp_sender_task_handle);
    }
}

/* 停止（幂等）：向队列发送 NULL 让任务退出，等待任务结束，若超时则强删并清理队列 */
static void stop_udp_sender(void)
{
    if (log_queue == NULL) return;

    if (udp_sender_task_handle != NULL) {
        /* 发送退出哨兵（不阻塞太久） */
        log_item_t *nullp = NULL;
        xQueueSend(log_queue, &nullp, pdMS_TO_TICKS(1000));

        /* 等待任务自行退出（最多等待 5s） */
        for (int i = 0; i < 50 && udp_sender_task_handle != NULL; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        /* 若仍未退出，强制删除（作为最后手段） */
        if (udp_sender_task_handle != NULL) {
            vTaskDelete(udp_sender_task_handle);
            udp_sender_task_handle = NULL;
        }
    }

    /* 若队列尚存在且未被任务删除，清理并删除 */
    if (log_queue) {
        log_item_t *p;
        while (xQueueReceive(log_queue, &p, 0) == pdTRUE) {
            if (p) free(p);
        }
        vQueueDelete(log_queue);
        log_queue = NULL;
    }
}

static esp_netif_t* System_netif_get(void){
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
    }

    return netif;
}

static bool check_network_connection(void) {

    esp_netif_t* netif = System_netif_get();
    if (netif == NULL) {
        return false;
    }
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return false;
    }
    
    // 检查是否获取到有效IP（不是0.0.0.0）
    return (ip_info.ip.addr != 0);
}

static bool validate_ip_address(const char *ip_str) {
    if (ip_str == NULL || strlen(ip_str) == 0) {
        return false;
    }
    
    // 简单的IP地址格式检查
    int a, b, c, d;
    if (sscanf(ip_str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        return false;
    }
    
    // 检查每个部分是否在0-255范围内
    if (a < 0 || a > 255 || b < 0 || b > 255 || 
        c < 0 || c > 255 || d < 0 || d > 255) {
        return false;
    }
    
    return true;
}


// ==================== UDP资源管理 ====================
static bool udp_resource_create(void) {
	// 1. 检查网络状态
    if (!check_network_connection()) {
        SAFE_LOG_LEVEL('E',"LOG", "网络未连接，无法创建UDP socket");
        return false;
    }
    
    // 2. 验证IP地址格式
    if (!validate_ip_address(log_sys.server_ip)) {
        SAFE_LOG_LEVEL('E',"LOG", "无效的IP地址: %s", log_sys.server_ip);
        return false;
    }
	
	if (log_sys.udp_socket >= 0) {
        // socket已存在，先关闭
        close(log_sys.udp_socket);
        log_sys.udp_socket = -1;
    }
    
    // 3.创建UDP socket
    log_sys.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (log_sys.udp_socket < 0) {
        SAFE_LOG_LEVEL('E',"LOG", "创建UDP socket失败: errno=%d", errno);
        return false;
    }

    // 4.use netif name bind to device
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    esp_netif_t* netif = System_netif_get();
    if (netif == NULL) {
        SAFE_LOG_LEVEL('E',"LOG", "no suitable netif found, skip SO_BINDTODEVICE");
        close(log_sys.udp_socket);
        log_sys.udp_socket = -1;
        return false;
    } else {
#if !CONFIG_LWIP_NETIF_API
        esp_netif_get_netif_impl_name(netif, ifr.ifr_name);
#else
        if_indextoname(esp_netif_get_netif_impl_index(netif), ifr.ifr_name);
#endif

        int ret = setsockopt(log_sys.udp_socket, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(struct ifreq));
        if (ret < 0) {
            SAFE_LOG_LEVEL('E',"LOG", "\"%s\" Unable to bind socket to specified interface: errno:%d, mean:%s", ifr.ifr_name, errno, strerror(errno));
            close(log_sys.udp_socket);
            log_sys.udp_socket = -1;
            return false;
        } 
    }
    
#if 1
    /* 5.设置为非阻塞（避免 sendto 阻塞） */
    int on = 1;
    ioctl(log_sys.udp_socket, FIONBIO, &on);
#else
    // 5.设置发送超时（500ms）
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 500000};
    setsockopt(log_sys.udp_socket, SOL_SOCKET, SO_SNDTIMEO, 
               &timeout, sizeof(timeout));
#endif

    // 6.设置发送缓冲区大小（可选）
    int send_buf_size = 2048;
    setsockopt(log_sys.udp_socket, SOL_SOCKET, SO_SNDBUF,
               &send_buf_size, sizeof(send_buf_size));
    
    SAFE_LOG_LEVEL('I',"LOG", "UDP资源创建成功，目标: %s:%d", 
             log_sys.server_ip, log_sys.server_port);
    return true;
}

static void udp_resource_destroy(void) {
    if (log_sys.udp_socket >= 0) {
        SAFE_LOG_LEVEL('I',"LOG", "释放UDP资源，关闭socket");
        close(log_sys.udp_socket);
        log_sys.udp_socket = -1;
    }
}

static void udp_log_send(const char *data, int len) {
    BaseType_t taken = pdFALSE;
    taken = xSemaphoreTake(log_sys.lock, 0);
    if (taken == pdFALSE) return;

    // 检查网络状态
    int net = check_network_connection();
    if (net == NETWORK_DISCONNECTED) {
        udp_resource_destroy();
        log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
        goto end;
    }

    /* 恢复处理：降频并尝试重建（仅在网络已连通） */
    if (log_sys.smart_recovery.is_recovering) {
        TickType_t now = xTaskGetTickCount();
        if ((now - log_sys.smart_recovery.recovery_start) < pdMS_TO_TICKS(2000)) {
            goto end;
        }
        if ((log_sys.udp_socket < 0) && (net == NETWORK_CONNECTED)) {
            if (udp_resource_create()) {
                log_sys.smart_recovery.is_recovering = false;
                log_sys.smart_recovery.failure_count = 0;
            } else {
                log_sys.smart_recovery.recovery_start = now;
                goto end;
            }
        }
    }

    if (log_sys.udp_socket < 0) {
        if ( LOG_MODE_UDP == log_sys.current_mode ) {
            log_sys.smart_recovery.is_recovering = true;
            log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
        }
        goto end;
    }
    
	// 检查IP地址是否有效
    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(log_sys.server_port);
    /* 更稳健的地址解析 */
    if (inet_pton(AF_INET, log_sys.server_ip, &dest_addr.sin_addr) != 1) {
        SAFE_LOG_LEVEL('W',"LOG", "invalid server_ip=%s", log_sys.server_ip);
        goto end;
    }
    
    /* 发送并处理错误（保存 errno 以确保日志正确） */
#if 1    
    int sent = sendto(log_sys.udp_socket, data, len, MSG_DONTWAIT, (struct sockaddr*)&dest_addr, sizeof(dest_addr));    // MSG_DONTWAIT(显性非阻塞)
#else
    int sent = sendto(log_sys.udp_socket, data, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));    
#endif
    if (sent >= 0) {
        // 发送成功：重置所有失败状态
        log_sys.smart_recovery.failure_count = 0;
        log_sys.smart_recovery.is_recovering = false;
        goto end;
    }

    /* 发送失败：记录 errno 并更新恢复状态 */
    int err = errno;
    SAFE_LOG_LEVEL('E',"LOG", "UDP send failed: errno=%d (%s), len=%d", err, strerror(err), (int)len);
    log_sys.smart_recovery.failure_count++;
    log_sys.smart_recovery.last_failure_time = xTaskGetTickCount();
    
    /* 退避与恢复策略 */
    switch (err) {
        case EBADF:
        case ENOTSOCK:
            /* 致命 socket 错误：立即释放资源并进入恢复模式 */
            udp_resource_destroy();
            log_sys.smart_recovery.is_recovering = true;
            log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
            SAFE_LOG_LEVEL('I',"LOG", "socket致命错误，进入恢复模式");
            break;
            
        case ENETUNREACH:
        case ENETDOWN:
        case EHOSTUNREACH:
            /* 网络错误：累计到阈值才进入恢复，避免瞬时波动触发 */
            if (log_sys.smart_recovery.failure_count >= LOG_RETRY_MAX) {
                udp_resource_destroy();
                log_sys.smart_recovery.is_recovering = true;
                log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
                SAFE_LOG_LEVEL('W',"LOG", "连续网络错误，进入恢复模式");
            }
            break;
            
        case EAGAIN:
            /* 资源临时不可用：更高阈值再恢复 */
            if (log_sys.smart_recovery.failure_count >= LOG_RETRY_MAX) {
                udp_resource_destroy();
                log_sys.smart_recovery.is_recovering = true;
                log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
                SAFE_LOG_LEVEL('W',"LOG", "持续 EAGAIN，进入恢复模式");
            }
            break;
            
        default:
            /* 其他错误：使用通用阈值保护 */
            if (log_sys.smart_recovery.failure_count >= LOG_RETRY_MAX) {
                udp_resource_destroy();
                log_sys.smart_recovery.is_recovering = true;
                log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
                SAFE_LOG_LEVEL('W',"LOG", "连续未知错误，进入恢复模式 err=%d", err);
            }
            break;
    }

end:    

    xSemaphoreGive(log_sys.lock);
}

// ==================== 自定义输出函数 ====================
static int udp_mode_vprintf(const char *format, va_list args) {
    char local_buf[LOG_BUFFER_SIZE];
    int ret = 0;

    /* 1) 串口/原始输出（安全调用原 vprintf） */
    if (esp_original_vprintf) {
        va_list args_uart;
        va_copy(args_uart, args);
        ret = esp_original_vprintf(format, args_uart);
        va_end(args_uart);
    }

    /* 2) 格式化到局部缓冲，避免 static 竞态 */
    va_list args_fmt;
    va_copy(args_fmt, args);
    int n = vsnprintf(local_buf, LOG_BUFFER_SIZE, format, args_fmt);
    va_end(args_fmt);

    /* 3) 计算可发送长度并检查错误/截断 */
    size_t send_len;
    if (n < 0) {
        send_len = 0;
    } else if ((size_t)n >= LOG_BUFFER_SIZE) {
        send_len = LOG_BUFFER_SIZE - 1; /* 截断，保留最后的 NUL 空位 */
    } else {
        send_len = (size_t)n;
    }
    local_buf[send_len] = '\0';

    /* 4) 仅在 UDP 模式且有内容时处理发送*/
    if (send_len > 0 && log_sys.current_mode == LOG_MODE_UDP) {
        /*
        IP_EVENT_PPP_GOT_IP 的回调/事件通常在 lwIP 的 TCPIP 线程（或 PPP 相关线程）执行。
        esp_log 的 vprintf 可能在同一线程/中断上下文被调用（例如 lwIP 回调中产生日志），
        此时在回调里再调用 socket/sendto 会造成 lwIP 的重入、竞态或内部状态异常，
        导致 PPP 的 IP 事件丢失或无法正常处理
        */
#ifdef CONFIG_LOG_REDIRECT_TASK_ENABLE
        if ( log_queue ) {
            log_item_t *p = (log_item_t *)heap_caps_calloc(sizeof(log_item_t), sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (p != NULL) {
                if (send_len > sizeof(p->data) - 1) send_len = sizeof(p->data) - 1;
                p->len = (uint16_t)send_len;
                memcpy(p->data, local_buf, p->len);
                p->data[p->len] = '\0';
                if (xQueueSend(log_queue, &p, 0) != pdTRUE) {
                    /* 队列已满，丢弃并释放 */
                    free(p);
                }
            }
        }
#else

#if CONFIG_LOG_UDP_REDIRECT == 1            
		udp_log_send(local_buf, (int)send_len);
#elif CONFIG_LOG_UDP_REDIRECT == 2
        supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
        if (1 == resource.log) {
            aiot_log_push((const uint8_t *)local_buf, (uint16_t)send_len, LOG_TYPE_LOG);
        }
#endif

#endif
    }

    return ret;
}


// ==================== 核心：模式切换 ====================
bool log_switch_mode(log_mode_t new_mode) {
    if (!log_sys.is_initialized) {
        SAFE_LOG_LEVEL('E',"LOG", "日志系统未初始化");
        return false;
    }
    
    xSemaphoreTake(log_sys.lock, portMAX_DELAY);
    
    // 检查是否已经在目标模式
    if (log_sys.current_mode == new_mode) {
        xSemaphoreGive(log_sys.lock);
        return true;
    }
    
    // 设置切换标志
    log_sys.is_switching = true;
    log_sys.target_mode = new_mode;
    
    SAFE_LOG_LEVEL('I',"LOG", "开始切换日志模式: %s -> %s",
            (log_sys.current_mode == LOG_MODE_SERIAL) ? "串口" : "UDP",
            (new_mode == LOG_MODE_SERIAL) ? "串口" : "UDP");
    
    // 执行模式切换
    switch (new_mode) {
        case LOG_MODE_UDP:
            // 切换到UDP模式
#if CONFIG_LOG_UDP_REDIRECT == 1			
            SAFE_LOG_LEVEL('I',"LOG", "已切换到UDP模式，UDP资源正在申请");
            if(!udp_resource_create()) {
                log_sys.udp_socket = -1;
            }
#elif CONFIG_LOG_UDP_REDIRECT == 2
        	SAFE_LOG_LEVEL('I',"LOG", "已切换到UDP模式");
#endif

#ifdef CONFIG_LOG_REDIRECT_TASK_ENABLE
            start_udp_sender();
#endif

            // 等待稳定后开始输出或尝试恢复
            log_sys.smart_recovery.is_recovering = true;
            log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
            log_sys.current_mode = LOG_MODE_UDP;
            esp_log_set_vprintf(udp_mode_vprintf);
            
            // 默认日志打印
            esp_log_level_set("*", ESP_LOG_INFO);
            apply_log_levels_from_table();
            break;
            
		 case LOG_MODE_SERIAL:
		 default:
            // 切换到串口模式
            esp_log_set_vprintf(esp_original_vprintf);
#if CONFIG_LOG_UDP_REDIRECT == 1			
            udp_resource_destroy();  // 释放UDP资源
			SAFE_LOG_LEVEL('I',"LOG", "已切换到串口模式，UDP资源已释放");
#elif CONFIG_LOG_UDP_REDIRECT == 2
        	SAFE_LOG_LEVEL('I',"LOG", "已切换到串口模式");
#endif
            
#ifdef CONFIG_LOG_REDIRECT_TASK_ENABLE
            stop_udp_sender();
#endif            
            log_sys.current_mode = LOG_MODE_SERIAL;
            break;
    }
    
    log_sys.target_mode = new_mode;
    log_sys.is_switching = false;
    
    xSemaphoreGive(log_sys.lock);
    return (log_sys.current_mode == new_mode);
}

// ==================== 初始化函数 ====================
void log_redirect_init(bool is_product) {
    if (log_sys.is_initialized) {
        return;
    }
    
    // 1. 创建互斥锁
    log_sys.lock = xSemaphoreCreateMutex();
    if (log_sys.lock == NULL) {
        ESP_LOGE("LOG", "创建互斥锁失败");
        return;
    }
    
    xSemaphoreTake(log_sys.lock, portMAX_DELAY);
    
    // 2. 保存ESP-IDF原始输出函数
    esp_original_vprintf = esp_log_set_vprintf(NULL);
    if (esp_original_vprintf == NULL) {
        // 获取失败，使用标准vprintf
        esp_original_vprintf = vprintf;
        SAFE_LOG_LEVEL('W',"LOG", "使用标准vprintf作为原始输出函数");
    }

    if ( is_product ) {
        // 3. 初始状态：串口模式，UDP资源未创建
        log_sys.udp_socket = -1;
        log_sys.current_mode = LOG_MODE_SERIAL;
        log_sys.target_mode = LOG_MODE_SERIAL;
        
        // 4. 生产模式，默认使用串口输出
        esp_log_set_vprintf(esp_original_vprintf);
        
        SAFE_LOG_LEVEL('I',"LOG", "日志重定向系统初始化完成: 串口输出");
    } else {
        // 3. 初始状态：UDP模式，UDP资源未创建
        log_sys.udp_socket = -1;
        log_sys.current_mode = LOG_MODE_UDP;
        log_sys.target_mode = LOG_MODE_UDP;
        log_sys.smart_recovery.is_recovering = true;
        log_sys.smart_recovery.recovery_start = xTaskGetTickCount();
        
#ifdef CONFIG_LOG_REDIRECT_TASK_ENABLE
        start_udp_sender();
#endif
        // 4. 研发模式，默认使用UDP输出
        esp_log_set_vprintf(udp_mode_vprintf);
        
        SAFE_LOG_LEVEL('I',"LOG", "日志重定向系统初始化完成: UDP输出");
    }

    log_sys.is_initialized = true;
    
    xSemaphoreGive(log_sys.lock);
}

// ==================== 获取当前状态 ====================
log_mode_t log_get_current_mode(void) {
    return log_sys.current_mode;
}

bool log_is_udp_active(void) {
    return (log_sys.current_mode == LOG_MODE_UDP && log_sys.udp_socket >= 0);
}

const char* log_get_server_ip(void) {
    return log_sys.server_ip;
}

int log_get_server_port(void) {
    return log_sys.server_port;
}

void log_server_ip_set(const char* ip_str) 
{
	SAFE_LOG_LEVEL('W',"LOG", "new ip:%s", ip_str);
	if(validate_ip_address(ip_str))
	{
		memset(log_sys.server_ip, 0, sizeof(log_sys.server_ip));
		strncpy(log_sys.server_ip, ip_str, sizeof(log_sys.server_ip));
	}
	else{
		SAFE_LOG_LEVEL('E',"LOG", "set ip fail");
	}
}

void log_server_port_set(int port) {
    log_sys.server_port = port;
}

void log_type_set(uint32_t type_mark) 
{
	for(int i=0; i<LogTypeCnt; i++)
	{
		if(type_mark & (1<<i)){
			esp_log_level_set(LogTypeString[i], ESP_LOG_DEBUG);
		}
	}
}

/* 表驱动的日志等级设置 */
typedef struct {
    const char *tag;
    esp_log_level_t level;
} log_level_entry_t;

/* 默认表（可按需扩展或从配置加载） */
static const log_level_entry_t g_log_level_table[] = {
    { "[md_protocol]",  ESP_LOG_WARN  },
    { "md_data",        ESP_LOG_NONE  },
    { "BLE_MODEBUS",    ESP_LOG_NONE  },
    { "[gatts_svr]",    ESP_LOG_ERROR },
    { "[ble_adv]",      ESP_LOG_WARN  },
    { "[ble_dev]",      ESP_LOG_ERROR },
    { "[APP_BLE]",      ESP_LOG_NONE  },
    { "[drv_nimble]",   ESP_LOG_ERROR },
    { "NimBLE",         ESP_LOG_NONE  },
    { "[CAN_PROTOCOL]", ESP_LOG_DEBUG },
    { "[CAN_DATA]",     ESP_LOG_DEBUG },
    { "[CAN_TRANSMIT]", ESP_LOG_WARN  },
    { "[wlcc_process]", ESP_LOG_NONE  },
    { "[wlcc_protocol]",ESP_LOG_NONE  },
    { "[wlcc_interface]",ESP_LOG_NONE },
    { "[wlcc_tlv]",     ESP_LOG_NONE  },
    { "[MD_DEV]",       ESP_LOG_NONE  },
    { "[MQTT_OPEN]",    ESP_LOG_NONE  },
    { "[CAN_PRODUCT]",  ESP_LOG_DEBUG },
    { "[MD_OTHER_INV]", ESP_LOG_ERROR },
    { "JSONs",          ESP_LOG_ERROR },
    { "[OTA]",          ESP_LOG_ERROR },
    { "[MD_METER]",     ESP_LOG_NONE  },
    { "[MD_S1]",        ESP_LOG_WARN  },
    { "[MODBUS_TLV]",   ESP_LOG_INFO  },
};

/* 应用表中的所有日志等级 */
void apply_log_levels_from_table(void)
{
    for (size_t i = 0; i < sizeof(g_log_level_table)/sizeof(g_log_level_table[0]); ++i) {
        esp_log_level_set(g_log_level_table[i].tag, g_log_level_table[i].level);
    }
}

// ==================== 使用示例 ====================
//void app_main(void) {
//    // 初始化日志系统
//    log_redirect_init();
//    
//    // 设置日志级别
//    esp_log_level_set("*", ESP_LOG_INFO);
//    
//    printf("\n=== ESP32 互斥模式日志系统演示 ===\n");
//    
//    // 初始状态
//    printf("初始状态: 串口输出模式，UDP资源未创建\n");
//    ESP_LOGI("MAIN", "应用程序启动 - 串口模式");
//    vTaskDelay(pdMS_TO_TICKS(1000));
//    
//    // 演示切换到UDP模式
//    printf("\n>>> 切换到UDP模式 <<<\n");
//    log_handle_command("mode udp");
//    vTaskDelay(pdMS_TO_TICKS(100));
//    
//    printf("现在输出日志将通过UDP发送...\n");
//    ESP_LOGI("DEMO", "这条日志通过UDP发送到 %s:%d", 
//             log_sys.server_ip, log_sys.server_port);
//    vTaskDelay(pdMS_TO_TICKS(1000));
//    
//    // 测试UDP模式
//    printf("\n>>> 测试UDP模式 <<<\n");
//    log_handle_command("test");
//    vTaskDelay(pdMS_TO_TICKS(2000));
//    
//    // 切换回串口模式
//    printf("\n>>> 切换回串口模式 <<<\n");
//    log_handle_command("mode serial");
//    vTaskDelay(pdMS_TO_TICKS(100));
//    
//    printf("现在输出日志仅到串口，UDP资源已释放\n");
//    ESP_LOGI("DEMO", "这条日志仅输出到串口，UDP socket已关闭");
//    
//    // 显示最终状态
//    printf("\n>>> 最终状态 <<<\n");
//    log_handle_command("status");
//    
//    printf("\n=== 演示结束 ===\n");
//    printf("可以继续使用命令控制日志模式\n");
//    
//    // 主循环
//    int counter = 0;
//    while (1) {
//        // 根据当前模式输出状态
//        if (log_get_current_mode() == LOG_MODE_SERIAL) {
//            ESP_LOGI("STATUS", "串口模式运行中，计数: %d", counter);
//        } else {
//            ESP_LOGI("STATUS", "UDP模式运行中，计数: %d", counter);
//        }
//        counter++;
//        
//        vTaskDelay(pdMS_TO_TICKS(3000));
//    }
//}

//状态
//    printf("\n>>> 最终状态 <<<\n");
//    log_handle_command("status");
//    
//    printf("\n=== 演示结束 ===\n");
//    printf("可以继续使用命令控制日志模式\n");
//    
//    // 主循环
//    int counter = 0;
//    while (1) {
//        // 根据当前模式输出状态
//        if (log_get_current_mode() == LOG_MODE_SERIAL) {
//            ESP_LOGI("STATUS", "串口模式运行中，计数: %d", counter);
//        } else {
//            ESP_LOGI("STATUS", "UDP模式运行中，计数: %d", counter);
//        }
//        counter++;
//        
//        vTaskDelay(pdMS_TO_TICKS(3000));
//    }
//}

