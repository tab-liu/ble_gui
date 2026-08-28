#include <string.h>

#include "driver/uart.h"
#include <driver/gpio.h>
#include <sys/unistd.h>
#include <sys/select.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "app_uart.h"
#include "utils.h"
#include "iot_period_task.h"
#include "uart_device_process.h"

//#include "app_dsp.h"


static SemaphoreHandle_t uart1_semaphore;
//static SemaphoreHandle_t uart2_semaphore;

static QueueHandle_t uart0_event_queue;
static QueueHandle_t uart1_event_queue;
static QueueHandle_t uart2_event_queue;


uint32_t recv_wait_time(uart_port_t port)
{ 
    uint32_t baud = 0; 
    if (uart_get_baudrate(port, &baud) == ESP_OK) { 
        return (((1000.0f / baud) * 11 * RX_HW_FIFO_LEN) + 15); 
    } else { 
        return 150; 
    } 
} 

esp_err_t app_uart0_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART0_BAUD,             /* 波特率 19200 */
        .data_bits = UART_DATA_8_BITS,  /*8个数据位*/
        .parity = UART_PARITY_DISABLE,  /*没有奇偶校验*/
        .stop_bits = UART_STOP_BITS_1,  /*1个停止位*/
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, /*不用流控*/
        .source_clk = UART_SCLK_APB,    /*串口时钟源*/ 
    };

    if ( !uart_is_driver_installed(UART0) ) //CONFIG_ESP_CONSOLE_UART_NUM
	{
		//Install UART driver, and get the queue.
		ESP_ERROR_CHECK(uart_driver_install(UART0, UART0_RX_BUF_SIZE, UART0_TX_BUF_SIZE, 10, &uart0_event_queue, ESP_INTR_FLAG_SHARED)); // ESP_INTR_FLAG_IRAM
		ESP_ERROR_CHECK(uart_param_config(UART0, &uart_config));
	}

    ESP_ERROR_CHECK(uart_set_pin(UART0, UART0_TXD, UART0_RXD, UART0_RTS, UART0_CTS)); //设置串口引脚

    return ESP_OK;
}
esp_err_t app_uart0_deinit(void)
{
    // 删除UART驱动程序，释放资源
    esp_err_t err = uart_driver_delete(UART0);
    if (err != ESP_OK) {
        return err; // 如果删除失败，返回错误代码
    }
    ESP_LOGI(UART0_TAG, "app_uart0_driver deleted");
    return ESP_OK; // 成功撤销初始化
}

esp_err_t app_uart1_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART1_BAUD,             /* 波特率 19200 */
        .data_bits = UART_DATA_8_BITS,  /*8个数据位*/
        .parity = UART_PARITY_DISABLE,  /*没有奇偶校验*/
        .stop_bits = UART_STOP_BITS_1,  /*1个停止位*/
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, /*不用流控*/
        .source_clk = UART_SCLK_APB,    /*串口时钟源*/ 
    };

    //Install UART driver, and get the queue.
    ESP_ERROR_CHECK(uart_driver_install(UART1, UART1_RX_BUF_SIZE, UART1_TX_BUF_SIZE, 5, &uart1_event_queue, ESP_INTR_FLAG_SHARED)); //ESP_INTR_FLAG_SHARED ESP_INTR_FLAG_IRAM
//    ESP_ERROR_CHECK(uart_driver_install(UART1, 33, 100, 10, &uart1_event_queue, ESP_INTR_FLAG_SHARED)); // ESP_INTR_FLAG_IRAM
//    ESP_ERROR_CHECK(uart_driver_install(UART1, UART1_RX_BUF_SIZE, UART1_TX_BUF_SIZE, 0, NULL, 0)); //ESP_INTR_FLAG_SHARED ESP_INTR_FLAG_IRAM
    
    ESP_ERROR_CHECK(uart_param_config(UART1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART1, UART1_TXD, UART1_RXD, UART1_RTS, UART1_CTS)); //设置串口引脚


	uart1_semaphore = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(uart1_semaphore == NULL)
	{
        ESP_LOGE(UART1_TAG, "uart1_semaphore  create failed");
		return -1;
	}
	xSemaphoreGive(uart1_semaphore);						/**< the resource is available after creation */


    return ESP_OK;
}

esp_err_t app_uart2_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART2_BAUD,             /* 波特率 19200 */
        .data_bits = UART_DATA_8_BITS,  /*8个数据位*/
        .parity = UART_PARITY_DISABLE,  /*没有奇偶校验*/
        .stop_bits = UART_STOP_BITS_1,  /*1个停止位*/
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, /*不用流控*/
        .source_clk = UART_SCLK_APB,    /*串口时钟源*/ 
    };

    //Install UART driver, and get the queue.
    ESP_ERROR_CHECK(uart_driver_install(UART2, UART2_RX_BUF_SIZE, UART2_TX_BUF_SIZE, 5, &uart2_event_queue, ESP_INTR_FLAG_SHARED)); //ESP_INTR_FLAG_SHARED ESP_INTR_FLAG_IRAM
    ESP_ERROR_CHECK(uart_param_config(UART2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART2, UART2_TXD, UART2_RXD, UART2_RTS, UART2_CTS)); //设置串口引脚

    return ESP_OK;
}

// Wait timeout for uart driver
#define PACKET_READ_TICS        (2000 / portTICK_PERIOD_MS)

/*
删除驱动
可能有问题
*/
void app_uart2_Deinit(void)
{
	vTaskDelay(pdMS_TO_TICKS(1000));

    uart_wait_tx_done(UART2, PACKET_READ_TICS);

	ESP_ERROR_CHECK(uart_driver_delete(UART2));
}

/*------------------------------------------------------------------------
*@Function： app_write_uart0_data
若控制明确双方主从，且发送只在单一线程执行，则不需要互锁信号辅助 uart1_semaphore
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
(-1) Parameter error
OTHERS (>=0) The number of bytes pushed to the TX FIFO
*/
int app_write_uart0_data(uint8_t *pdata, int len)
{
	int ret=0;
    if (!pdata || !len) {
        ESP_LOGE(UART0_TAG, "!data || !len"); // 无效参数
        return -1;
    }  
    
//    if (!uart1_semaphore || xSemaphoreTake(uart1_semaphore, timeout) != pdTRUE)
//    {
//        return -1; // pdMS_TO_TICKS()
//    }

    ret = uart_write_bytes(UART0, pdata, len); 
//    xSemaphoreGive(uart1_semaphore);  /*释放信号量*/
    return ret;
}

/*------------------------------------------------------------------------
*@Function： app_write_uart1_data
若控制明确双方主从，且发送只在单一线程执行，则不需要互锁信号辅助 uart1_semaphore
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
(-1) Parameter error
OTHERS (>=0) The number of bytes pushed to the TX FIFO
*/
int app_write_uart1_data(uint8_t *pdata, int len)
{
	int ret=0;
    if (!pdata || !len) {
        ESP_LOGE(UART1_TAG, "!data || !len"); // 无效参数
        return -1;
    }  
    
    if (!uart1_semaphore || xSemaphoreTake(uart1_semaphore, pdMS_TO_TICKS(3000)) != pdTRUE)//windy:底层uart_tx_all()有强制互斥锁，上层需要判断
    {
        return -1; // pdMS_TO_TICKS()
    }

    ret = uart_write_bytes(UART1, pdata, len); 
    xSemaphoreGive(uart1_semaphore);  /*释放信号量*/
    return ret;
}
/*------------------------------------------------------------------------
*@Function： app_write_uart2_data
若控制明确双方主从，且发送只在单一线程执行，则不需要互锁信号辅助 uart1_semaphore
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
(-1) Parameter error
OTHERS (>=0) The number of bytes pushed to the TX FIFO
*/
int app_write_uart2_data(uint8_t *pdata, int len)
{
	int ret=0;
    if (!pdata || !len) {
        ESP_LOGE(UART2_TAG, "!data || !len"); // 无效参数
        return -1;
    }  
    
//    if (!uart1_semaphore || xSemaphoreTake(uart1_semaphore, timeout) != pdTRUE)
//    {
//        return -1; // pdMS_TO_TICKS()
//    }
	memcpy(reals.struct_uart2.Txbuffer, pdata, len);
//	reals.struct_uart2.Txbuffer=pdata;

    ret = uart_write_bytes(UART2, pdata, len); 
	ESP_LOGW(UART2_TAG, " uart TX Len= %d, TX Len2= %d, Txbuff=%s",strlen((char *)reals.struct_uart2.Txbuffer),len,reals.struct_uart2.Txbuffer);
	
//    xSemaphoreGive(uart1_semaphore);  /*释放信号量*/
    return ret;
}


void app_uart_init(void)
{
//    app_uart0_init();
    app_uart1_init();
//    app_uart2_init();//4G

	reals.AT_Cmd_RxPointer =reals.struct_uart2.Rxbuffer;//指针指向数组

	
    //if (xTaskCreate(app_uart1_rx_task, "uart1 task", 4096, NULL, 5, NULL) != pdTRUE)
    // if (xTaskCreatePinnedToCore(app_uart0_rx_task, "app_uart0_rx_task",  1024*5, NULL, 4, NULL, 1) != pdPASS)
	// {
    //     ESP_LOGE(UART0_TAG, "app_uart0_rx_task create failed");
	// 	abort();
    // }
	// else
	// {
	// 	ESP_LOGI(UART0_TAG, "app_uart0_rx_task create Success");
	// }	
	
//    if (xTaskCreatePinnedToCore(app_uart1_rx_task, "app_uart1_rx_task",  1024*4, NULL, 4, NULL, 1) != pdPASS)
//	{
//        ESP_LOGE(UART1_TAG, "app_uart1_rx_task create failed");
//		abort();
//    }
//	else
//	{
//		ESP_LOGI(UART1_TAG, "app_uart1_rx_task create Success");
//	}
	vTaskDelay(pdMS_TO_TICKS(200));

//    if (xTaskCreatePinnedToCore(app_uart2_rx_task, "app_uart2_rx_task",  1024*4, NULL, 4, NULL, 1) != pdPASS)
//	{
//        ESP_LOGE(UART2_TAG, "app_uart2_rx_task create failed");
//		abort();
//    }
//	else
//	{
//		ESP_LOGI(UART2_TAG, "app_uart2_rx_task create Success");
//	}




	// if (xTaskCreatePinnedToCore(app_uart0_Tx_task, "app_uart0_Tx_task",  1024*4, NULL, 7, NULL, 1) != pdPASS)
	// {
	// 	ESP_LOGE(UART0_TAG, "app_uart0_Tx_task create failed");
	// 	abort();
	// }
	// else
	{
		ESP_LOGI(UART0_TAG, "app_uart0_Tx_task create Success");
	}

//	if (xTaskCreatePinnedToCore(app_uart1_Tx_task, "app_uart1_Tx_task",  1024*4, NULL, 7, NULL, 1) != pdPASS)
//	{
//		ESP_LOGE(UART1_TAG, "app_uart1_Tx_task create failed");
//		abort();
//	}
//	else
//	{
//		ESP_LOGI(UART1_TAG, "app_uart1_Tx_task create Success");
//	}

}

/**
* @brief :uart_rx_timeout_clean
* @param[in] :	   
* @param[out] : 
* @return :	 
100ms cycle
*/
void uart_rx_timeout_clean (void)
{
	reals.struct_uart0.Rx_timeout_cnt++;
	reals.struct_uart1.Rx_timeout_cnt++;
	reals.struct_uart2.Rx_timeout_cnt++;
	
	if(1 == reals.struct_uart0.FlagRx_ok)
	{
		reals.struct_uart0.RxOk_Delay_cnt++;
		if(reals.struct_uart0.RxOk_Delay_cnt >= 10)//1s
		{
			reals.struct_uart0.FlagRx_ok =0;
			reals.struct_uart0.RxOk_Delay_cnt=0;
			reals.struct_uart0.rxBytesNum=0;
			memset(reals.struct_uart0.Rxbuffer, 0x00, sizeof(reals.struct_uart0.Rxbuffer));
			
		}
	}
	
	if(1 == reals.struct_uart1.FlagRx_ok)
	{
		reals.struct_uart1.RxOk_Delay_cnt++;
		if(reals.struct_uart1.RxOk_Delay_cnt >= 10)//1s
		{
			reals.struct_uart1.FlagRx_ok =0;
			reals.struct_uart1.RxOk_Delay_cnt=0;
			reals.struct_uart1.rxBytesNum=0;
			memset(reals.struct_uart1.Rxbuffer, 0x00, sizeof(reals.struct_uart1.Rxbuffer));
			
		}
	}
	
	if(1 == reals.struct_uart2.FlagRx_ok)
	{
		reals.struct_uart2.RxOk_Delay_cnt++;
		if(reals.struct_uart2.RxOk_Delay_cnt >= 10)//1s
		{
			reals.struct_uart2.FlagRx_ok =0;
			reals.struct_uart2.RxOk_Delay_cnt=0;			
			reals.struct_uart2.rxBytesNum=0;
			memset(reals.struct_uart2.Rxbuffer, 0x00, sizeof(reals.struct_uart2.Rxbuffer));
			
		}
	}	


}

// 定义全局变量来保存任务句柄
static TaskHandle_t app_uart0_rx_task_handle = NULL;
static TaskHandle_t app_uart0_tx_task_handle = NULL;
static UART_STRUCT *struct_uart0 = NULL;

/**
* @brief :app_uart0_rx_task
从uart rx 队列
* @param[in] :	   
* @param[out] : 
* @return :	 
*/
static void app_uart0_rx_task(void *pvParameters)
{   
    //uint32_t rxBytesNum = 0;
    uint32_t wait_time = portMAX_DELAY;
    //uint8_t *buffer = calloc(RX_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	//static   uint8_t buffer[UART0_RX_BUF_SIZE];// -未初始化，或初始化为0，放在外部RAM; EXT_RAM_ATTR
	uint32_t len=0;
//    uint32_t time = recv_wait_time(UART0);

    while (uart0_event_queue)
    {  
        uart_event_t event;
        if(xQueueReceive(uart0_event_queue, (void * )&event, wait_time) == pdTRUE)
		{
            switch(event.type) 
			{
                case UART_DATA: /*接收串口数据事件*/
                    ESP_LOGI(UART0_TAG, "lxy app_uart0_rx_task  UART_DATA");
					struct_uart0->Rx_timeout_cnt = 0;
					
                    //ESP_LOGD(UART1_TAG, "data event len: %d", event.size);
                    if (struct_uart0->rxBytesNum + event.size >= UART_RX_BUF_SIZE)
                    {
                        struct_uart0->rxBytesNum = 0;
                    }

/*
rtn:
(-1) Error
OTHERS (>=0) The number of bytes read from UART buffer
*/
                    len = uart_read_bytes(UART0, &struct_uart0->Rxbuffer[struct_uart0->rxBytesNum], event.size, 0);
                    if(len > 0)
                    {
                        struct_uart0->rxBytesNum += len;

						/* when the received data length is less than uart fifo size, then considered transmission
						 * finished, and goto receiving finish process after setting a short delay */
						if(len < UART_HW_FIFO_SIZE) 
						{
							wait_time = pdMS_TO_TICKS(UART_RX_TIMEOUT);// 
						}
						/* uart will trigger a data interrupt when it received uart fifo size bytes of data,
						 * set a time to wait for any subsequent data to be received */
						else 
						{
							wait_time = pdMS_TO_TICKS(UART0_SUBSEQUENT_DATA_TIMEOUT);
						}
                    }					
                    break;
                case UART_FIFO_OVF: /*硬件FIFO溢出检测*/
                    ESP_LOGI(UART0_TAG, "hw fifo overflow");
                    uart_flush_input(UART0); /*清除输入缓存数据，丢弃环形缓冲区的数据*/
                    xQueueReset(uart0_event_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL: /*接收缓冲区满*/
                    ESP_LOGI(UART0_TAG, "ring buffer full");
                    uart_flush_input(UART0);
                    xQueueReset(uart0_event_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(UART0_TAG, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(UART0_TAG, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(UART0_TAG, "uart frame error");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    break;
				case UART_DATA_BREAK:
					 ESP_LOGI(UART0_TAG, "UART_DATA_BREAK");
					break;
                //Others
                default:
                    ESP_LOGI(UART0_TAG, "uart event type: %d", event.type);
                    break;
            }
        }
        else //接收超时后解析
        {
            ESP_LOGD(UART0_TAG, "Received len: %"PRIu16"", struct_uart0->rxBytesNum);
			/* uart receiving data finished */
            if(struct_uart0->rxBytesNum > 0)
            {
				struct_uart0->FlagRx_ok =1;
				ESP_LOGW(UART0_TAG, " uart rx Received len: %"PRIu16", buff=%s",struct_uart0->rxBytesNum, struct_uart0->Rxbuffer);
				

				serial_recv_callback(struct_uart0);

				if(1 == struct_uart0->FlagRx_ok)
				{
						struct_uart0->FlagRx_ok =0;
						struct_uart0->rxBytesNum=0;
						memset(struct_uart0->Rxbuffer, 0x00, sizeof(struct_uart0->Rxbuffer));
				}
            }	
            wait_time = portMAX_DELAY;
			
        }
    }
    vTaskDelete(NULL);
}

/**
* @brief :app_uart0_Tx_task
uart tx
* @param[in] :	   
* @param[out] : 
* @return :	 
*/
static void app_uart0_Tx_task(void *pvParameters)
{   
	vTaskDelay(pdMS_TO_TICKS(3000)); //  
	
	while(1) 
	{ 
        vTaskDelay(pdMS_TO_TICKS(200)); //  
    }
}


void app_uart0_init_task(void)
{
    app_uart0_init();

    struct_uart0 = (UART_STRUCT*)iot_calloc(sizeof(UART_STRUCT));
    if(struct_uart0 == NULL)
    {
        ESP_LOGE(UART0_TAG, "app_uart0_rx_task create failed");
        return;
    }


    if (xTaskCreate(app_uart0_rx_task, "app_uart0_rx_task",  1024*5, NULL, 4, &app_uart0_rx_task_handle) != pdPASS)
    {
        ESP_LOGE(UART0_TAG, "app_uart0_rx_task create failed");
        abort();
    }
    else
    {
        ESP_LOGI(UART0_TAG, "app_uart0_rx_task create Success");
    }

    if (xTaskCreate(app_uart0_Tx_task, "app_uart0_Tx_task",  1024*4, NULL, 7, &app_uart0_tx_task_handle) != pdPASS)
    {
        ESP_LOGE(UART0_TAG, "app_uart0_Tx_task create failed");
        abort();
    }
    else
    {
        ESP_LOGI(UART0_TAG, "app_uart0_Tx_task create Success");
    }
}

void app_uart0_close_tasks(void)
{
    if (app_uart0_rx_task_handle != NULL)
    {
        vTaskDelete(app_uart0_rx_task_handle);
        app_uart0_rx_task_handle = NULL;
        ESP_LOGI(UART0_TAG, "app_uart0_rx_task deleted");
    }

    if (app_uart0_tx_task_handle != NULL)
    {
        vTaskDelete(app_uart0_tx_task_handle);
        app_uart0_tx_task_handle = NULL;
        ESP_LOGI(UART0_TAG, "app_uart0_Tx_task deleted");
    }

    app_uart0_deinit();
    
    if(struct_uart0 != NULL)
    {
        free(struct_uart0);
        struct_uart0 = NULL;
        ESP_LOGI(UART0_TAG, "struct_uart0 deleted");
    }
}
