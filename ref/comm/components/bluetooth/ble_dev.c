#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "ble_dev.h"
#include "utils.h"

//#include "libdarray.h"
#include "esp_check.h"
#include "esp_log.h"
#include <string.h>
#include "app_bt.h"


#define TAG "[ble_dev]"

#define BLE_RX_QUEUE_SZ     48              // 接收队列大小

static QueueHandle_t ble_rx_queue = NULL;           // 蓝牙数据接收队列（缓存APP下发的指令）
static ble_data_t ble_data = {0};                 	// ble数据缓存
static TimerHandle_t ble_recv_timeout_tmr = NULL;   // 接收超时定时器
static bool ble_dev_inited = false;                 // 蓝牙设备是否初始化

/**
 * @brief 蓝牙数据备份到接收队列
 将多段RX报文拼接后，才一起存入队列
 * 
 * @param data 数据
 * @param len 长度
 */
static void ble_data_backup_to_queue(uint8_t *data, int len)
{
    if (len == 0) return;

    /* 为数据申请缓存 */
    ble_data_t rx_buff;
    rx_buff.data = (uint8_t *)iot_calloc(len);
    if (rx_buff.data == NULL)
    {
        ESP_LOGE(TAG, "memory malloc failed, size: %d", len);
        return;
    }

    /* 拷贝数据到缓存 */
    memcpy(rx_buff.data, data, len);
    rx_buff.len = len;
    
    /* 数据暂存到队列中 */
    if (xQueueSend(ble_rx_queue, &rx_buff, pdMS_TO_TICKS(10)) != pdPASS) 
    {
        ESP_LOGE(TAG, "send data to ble_rx_queue timeout");
        free(rx_buff.data); // 发送失败释放申请的缓存
    }
	else
	{
		ESP_LOGI(TAG, "Send to ble_rx_queue, len=%d", len);
		// ESP_LOG_BUFFER_HEX(TAG, rx_buff.data, rx_buff.len);
	}
}

/**
 * @brief 接收超时定时器回调函数
 * - 当接收到MTU-3大小的数据后，设置定时器等待下一帧数据超时，超时后将接收到的数据发送到接收队列中
 * 
 * @param xTimer 定时器句柄
 */
static void timeout_timer_cb(TimerHandle_t xTimer)
{
    ble_data_backup_to_queue(ble_data.data, ble_data.len);//等待
    ble_data.len = 0;
}

/**
 * @brief 蓝牙应用数据接收回调函数
 * - APP发送数据时触发该函数执行
 * - 判断蓝牙数据接收完成有三种情况：
 * - 1、蓝牙单次发送数据小于MTU值
 * - 2、蓝牙数据总接收长度大于一包指令数据的最大值
 * - 3、蓝牙接收到数据后等待下一帧数据超时
 * 
 * @param data 数据指针
 * @param len 数据长度


  等同于data_rx_callback(),FF02：ESP32 RX
 * 
 */
static void ble_app_rx_callback(uint8_t *data, uint8_t len)
{
    memcpy(&ble_data.data[ble_data.len], data, len);//收到任何数据先拼接到缓存
    ble_data.len += len;
    // ESP_LOGE(TAG, "ble_app_rx_callback, len=%d, timestamp=%ld", len, (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));

    /* 单次长度小于MTU长度认为数据接收完成，总长度大于接收最大值认为数据接收完成 （MTU<=247）*/
    if ((len < (drv_ble_info_get()->mtu_size - 3)) || (ble_data.len >= (BLE_RX_MAX_SZ - drv_ble_info_get()->mtu_size)))//本次接收数据较短或超出最大长度，则立刻存入队列
    {
        ble_data_backup_to_queue(ble_data.data, ble_data.len);
        ble_data.len = 0;
        xTimerStop(ble_recv_timeout_tmr, portMAX_DELAY);
    }
    /* 设置等待下一帧数据超时时间 */
    else//本次接收数据为单帧切割长度，则等待
    {
        uint32_t rx_timeout = pdMS_TO_TICKS(drv_ble_info_get()->rx_timeout);
        xTimerChangePeriod(ble_recv_timeout_tmr, rx_timeout, portMAX_DELAY); // 调用此函数后定时器会立即运行
    }
}

/**
 * @brief 蓝牙设备初始化
 * 
 * @param name 设备名
 * - 若为空则使用默认设备名
 * 
 * @param event_cb 事件回调函数
 * @return 成功返回0，失败返回-1
 */
int ble_dev_init(const char *name, ble_event_cb_t event_cb)
{
    if (ble_dev_inited) return 0;

    /* 初始化蓝牙驱动 */
    assert(drv_ble_init(name, ble_app_rx_callback, event_cb) == 0);

    /* 创建接收消息队列 */
    if (!ble_rx_queue) {
        ble_rx_queue = xQueueCreate(BLE_RX_QUEUE_SZ, sizeof(ble_data_t));
        assert(ble_rx_queue != NULL);
    }
    
    /* 创建接收超时定时器 */
    if (!ble_recv_timeout_tmr) {
        ble_recv_timeout_tmr = xTimerCreate("ble timer",			// 定时器名称
                                    1000, 				// 时间,此处参数无效，将会被覆盖
                                    pdFALSE,			// 自动重载
                                    NULL, 				// 定时器ID
                                    timeout_timer_cb);	// 回调函数
        assert(ble_recv_timeout_tmr);
    }
	
    /* 蓝牙单次数据接收缓存 */
    if (!ble_data.data) {
        ble_data.data = (uint8_t *)iot_calloc(BLE_RX_MAX_SZ);
        assert(ble_data.data != NULL);
    }

    ble_dev_inited = true;
    return 0;
}

void ble_dev_deinit(void)
{
    if (drv_ble_deinit() != 0) {
        ESP_LOGE(TAG, "drv_ble_deinit failed");
        return;
    }
    ble_dev_inited = false;
    ESP_LOGW(TAG, "BLE device deinitialized");
}

void ble_dev_reinit(const char *name, ble_event_cb_t event_cb)
{
    if (drv_ble_init(name, ble_app_rx_callback, event_cb) != 0) {
        ESP_LOGE(TAG, "drv_ble_init failed");
        return;
    }
    ble_dev_inited = true;
    ESP_LOGW(TAG, "BLE device reinitialized, name: %s", name);
}

/**
 * @brief 断开蓝牙连接
 * 
 * @return 成功返回0，失败返回-1
 */
int ble_disconnect(void)
{
    return drv_ble_disconnect();
}

/**

FF02 rx
 * @brief 蓝牙数据接收
 * - 该函数从内部接收数据队列中取出接收到的蓝牙数据
 * - 应用层处理完数据后需要释放数据内存
 * 
 * @param data 数据结构指针
 * @param timeout 接收超时
 * @return 成功返回0，失败/超时返回-1
 */
int ble_dev_recv(ble_data_t *data, int timeout)
{
    int ret = xQueueReceive(ble_rx_queue, data, pdMS_TO_TICKS(timeout));
    return (ret==pdTRUE) ? (0) : (-1);
}


/**
FF01 tx
 * @brief 蓝牙数据发送
 * 
 * @param data 数据指针
 * @return 成功返回0，失败返回-1
 */
int ble_dev_send(ble_data_t *data)
{
    return drv_ble_chr1_send(data->data, data->len);
}

/**
FF03 tx
 * @brief 蓝牙数据推送
 * 
 * @param data 数据指针
 * @return 成功返回0，失败返回-1
 */
int ble_dev_post(ble_data_t *data)
{
    return drv_ble_chr3_send(data->data, data->len);
}

/**
 * @brief 设置蓝牙广播名称
 * 
 * @param name 广播名
 * @return 成功返回0，失败返回-1
 */
int ble_set_name(const char *name)
{
    return drv_ble_set_dev_name(name);
}

/**
 * @brief 获取蓝牙MAC信息
 * 
 * @param sta 返回的NAC信息
 */
void ble_get_mac_addr(ble_sta_t *sta)
{
    drv_ble_get_mac_addr(sta->mac_addr);
}

/**
 * @brief 获取蓝牙状态信息
 * 
 * @param sta 返回的状态信息
 */
void ble_get_status(void)
{
    uint8_t connect = 0;
    if (ble_dev_inited == false) {
        connect = 0;
    }
    else {
        // 新增广播数据, 无法通过是否有广播数据来判断是否已连接.
        // connect = drv_ble_info_get()->conn_sta;
        connect = drv_ble_get_ConnectEvent();
    }
	ble_encrypt_info.flag.bit.ble_connect = connect;
}

/**
 * @brief 关闭蓝牙广播
 * - 如果蓝牙已连接则立即断开连接并停止当前广播
 * - 如果未处于连接状态则停止当前广播
 * 
 * @return 成功返回0，失败返回-1
 */
int ble_advertise_stop(void)
{
    return drv_ble_advertise_stop();
}

/**
 * @brief 开启蓝牙广播
 * 
 * @return 成功返回0，失败返回-1
 */
int ble_advertise_start(void)
{
    return drv_ble_advertise_start();
}



/**
 * @brief ble数据调试接口
 * 
 * @param data 数据指针
 * @param len 数据长度
 */
void ble_debug(uint8_t *data, int len)
{
    ble_data_backup_to_queue(data, len);
}
