/**
  ******************************************************************************
  * @file      rs485_transmit.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/3/26
  * @brief     RS485通信驱动
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/3/26  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <math.h>

/* ======================== 本地模块文件引用（可选） ============================= */

#include "rs485_transmit.h"
#include "modbus_data.h"
#include "parameter.h"
#include "utils.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[RS485_TRANSMIT]"

/* =============================== 文件内全局变量 ================================ */

USE_EXT_RAM_BSS static RS485_TRANSMIT_STRUCT rs485_data = {0};
USE_EXT_RAM_BSS static int rs485_subsequent_data_timeout = 0;
static QueueHandle_t uart_evt_queue = NULL;

/* ================================ 模块函数定义 ================================ */

static inline int uart_subsequent_data_timeout_ms(uint32_t baud, uint32_t fifo_size)
{
    if (baud == 0 || fifo_size == 0) return 0;

    /* 每位时间（微秒），使用浮点保证精度 */
    double bit_time_us = 1000000.0 / (double)baud;

    /* 每字节按 10 bit（1 start + 8 data + 1 stop） */
    double byte_time_us = bit_time_us * 10.0;

    /* 总微秒数 -> 毫秒 */
    double total_ms = (byte_time_us * (double)fifo_size) / 1000.0;

    const double margin_ms = 20.0;
    double result_ms = total_ms + margin_ms;

    /* 最后一步向上取整并做溢出保护 */
    double ceil_ms = ceil(result_ms);
    if (ceil_ms >= (double)INT_MAX) return INT_MAX;
    return (int)ceil_ms;
}

// rs485接收事件处理
void rs485_evt_porcess(void)
{
    static uint32_t wait_time = UINT32_MAX;  
    static uint32_t pre_time;
    uart_event_t event;
    
    if(uart_evt_queue && xQueueReceive(uart_evt_queue, (void * )&event, 0) == pdTRUE)
    {
        switch(event.type) 
        {
            case UART_DATA: /*接收串口数据事件*/
                ESP_LOGD(TAG, "UART_DATA");

                if (rs485_data.rxBytesNum + event.size >= RS485_RX_BUF_SIZE) {
                    rs485_data.rxBytesNum = 0;
                }

                uint32_t len = uart_read_bytes(RS485_UART_NUM, &rs485_data.Rxbuffer[rs485_data.rxBytesNum], event.size, 0);
                if(len > 0)
                {
                    rs485_data.rxBytesNum += len;

                    /* when the received data length is less than uart fifo size, then considered transmission
                     * finished, and goto receiving finish process after setting a short delay */
                    if(len < RS485_HW_FIFO_SIZE) 
                    {
                        wait_time = RS485_RX_TIMEOUT;
                        pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;;
                    }
                    /* uart will trigger a data interrupt when it received uart fifo size bytes of data,
                     * set a time to wait for any subsequent data to be received */
                    else 
                    {
                        wait_time = rs485_subsequent_data_timeout;
                        pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;;
                    }
                }                   
                break;
            case UART_FIFO_OVF: /*硬件FIFO溢出检测*/
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(RS485_UART_NUM); /*清除输入缓存数据，丢弃环形缓冲区的数据*/
                xQueueReset(uart_evt_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL: /*接收缓冲区满*/
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(RS485_UART_NUM);
                xQueueReset(uart_evt_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                break;
            case UART_DATA_BREAK:
                ESP_LOGI(TAG, "UART_DATA_BREAK");
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
        }
    }
    else //接收超时后解析
    {
        if ( wait_time != UINT32_MAX )
        {
            uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ( (now_time - pre_time) >= wait_time )
            {
                /* uart receiving data finished */
                if(rs485_data.rxBytesNum > 0)
                {
                    rs485_data.rxMessSplicFlag = 1;
                    ESP_LOGW(TAG, "Received len: %"PRIu16", buff=%s",rs485_data.rxBytesNum, rs485_data.Rxbuffer);
                                    
                    if(1 == rs485_data.rxMessSplicFlag) {
                        
                        // 此处添加解析函数
                        
                        rs485_data.rxMessSplicFlag = 0;
                        rs485_data.rxBytesNum = 0;
                        memset(rs485_data.Rxbuffer, 0x00, sizeof(rs485_data.Rxbuffer));
                    }
                } 
                
                wait_time = UINT32_MAX;
            }
        }
    }

    // 发送完成检查
    if ( rs485_data.txSendingFlag 
        && (ESP_OK == uart_wait_tx_done(RS485_UART_NUM, 0))) {
        rs485_data.txSendingFlag = 0;
        gpio_set_level(CONFIG_RS485_DE_GPIO_PIN, 0);
    }
}

esp_err_t rs485_send_nonblocking(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) return ESP_ERR_INVALID_ARG;

    // 使能驱动（发送）
    gpio_set_level(CONFIG_RS485_DE_GPIO_PIN, 1);

    // 写入 uart 缓冲区并返回（不等待 TX 完成）
    int written = uart_write_bytes(RS485_UART_NUM, (const char *)data, len);
    if (written < 0) {
        gpio_set_level(CONFIG_RS485_DE_GPIO_PIN, 0); // 出错回退
        ESP_LOGE(TAG, "uart_write_bytes failed");
        return ESP_FAIL;
    }
    rs485_data.txSendingFlag = 1;
    return ESP_OK;
}

esp_err_t rs485_send_blocking(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) return ESP_ERR_INVALID_ARG;

    // 使能驱动（发送）
    gpio_set_level(CONFIG_RS485_DE_GPIO_PIN, 1);

    // 写入 uart 缓冲区并返回（等待 TX 完成）
    uart_write_bytes(RS485_UART_NUM, (const char *)data, len);
    uart_wait_tx_done(RS485_UART_NUM, pdMS_TO_TICKS(200)); // 等待发送完成
    gpio_set_level(CONFIG_RS485_DE_GPIO_PIN, 0);

    return ESP_OK;
}

// RS485初始化
esp_err_t rs485_init(void)
{
    // 配置 DE 引脚为输出，初始为接收（低）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_RS485_DE_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(CONFIG_RS485_DE_GPIO_PIN, 0);

    uart_config_t rs485_uart_config = {
        .baud_rate = CONFIG_RS485_BAUD,             /* 波特率*/
        .data_bits = UART_DATA_8_BITS,              /*8个数据位*/
        .parity = UART_PARITY_DISABLE,              /*没有奇偶校验*/
        .stop_bits = UART_STOP_BITS_1,              /*1个停止位*/
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      /*不用流控*/
        .source_clk = UART_SCLK_XTAL,               /*串口时钟源*/ 
    };

	//Install UART driver, and get the queue.
	ESP_ERROR_CHECK(uart_driver_install(RS485_UART_NUM, RS485_RX_BUF_SIZE, RS485_TX_BUF_SIZE, 10, &uart_evt_queue, ESP_INTR_FLAG_SHARED)); // ESP_INTR_FLAG_IRAM
	ESP_ERROR_CHECK(uart_param_config(RS485_UART_NUM, &rs485_uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_NUM, RS485_TXD, RS485_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)); //设置串口引脚

    // 计算超时
    rs485_subsequent_data_timeout = uart_subsequent_data_timeout_ms(CONFIG_RS485_BAUD, RS485_HW_FIFO_SIZE);

    ESP_LOGI(TAG, "rs485 init ok. (baud: %d, fifo: %d, timeout: %d ms)", (int)CONFIG_RS485_BAUD, (int)RS485_HW_FIFO_SIZE, rs485_subsequent_data_timeout);
    return ESP_OK;
}

