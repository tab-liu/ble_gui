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
#include "parameter.h"
#include "nvs_store.h"
#include "comm_define.h"
#include "modbus_protocol.h"
#include "crc.h"
#include "dev_data_record.h"
#include "parameter.h"
#include "utils.h"

#define UART0_TAG "[uart0]"

#define UART0           UART_NUM_0
#define UART0_TXD       (UART_PIN_NO_CHANGE)  
#define UART0_RXD       (UART_PIN_NO_CHANGE) 
#define UART0_RTS       (UART_PIN_NO_CHANGE)
#define UART0_CTS       (UART_PIN_NO_CHANGE)
#define UART_RX_BUF_SIZE         1100
#define UART_TX_BUF_SIZE         1300//1100//1024
#define UART0_BAUD	115200
#define UART0_BYTE_TRANSMISSION_TIME_MS (1000.0 / UART0_BAUD * 10) // 每字节传输时间（毫秒）
#define UART0_SUBSEQUENT_DATA_TIMEOUT (UART_HW_FIFO_SIZE * UART0_BYTE_TRANSMISSION_TIME_MS + 20) // 增加一些余量
#define UART_HW_FIFO_SIZE				120 			// UART硬件FIFO大小
#define UART_RX_TIMEOUT	10//ms,uart rx超时延时，容许报文分断间隔

/*------------------------------------------------------------------------------*/

typedef struct {
	uint8_t FlagRx_ok;//1-收到uart报文，发送前清零，解析完清零
	uint16_t rxBytesNum;//
	uint16_t TxBytesNum;
	
	uint32_t Rx_timeout_cnt;//rx超时计数器；100ms+1
	uint16_t RxOk_Delay_cnt;//rx收到后，要超时清零复位，1s；

	uint16_t ReadRegAddress;//作为modbus主的辅助解析变量，寄存器地址
	uint16_t ReadRegCnt;//作为modbus主的辅助解析变量，度寄存器数量
	uint16_t WriteRegAddress;//作为modbus主的辅助解析变量，寄存器地址
	uint16_t WriteRegCnt;//作为modbus主的辅助解析变量，度寄存器数量
	
	uint8_t	gFunCode;

	uint8_t slaveaddress;
    
	uint8_t Rxbuffer[UART_RX_BUF_SIZE];
	uint8_t Txbuffer[UART_TX_BUF_SIZE];

}UART_STRUCT;//UART1_STRUCT

/*------------------------------------------------------------------------------*/

static UART_STRUCT *struct_uart0 = NULL;
static QueueHandle_t uart0_event_queue;
static TaskHandle_t app_uart0_rx_task_handle = NULL;
static USE_EXT_RAM_BSS iot_factory_struct fcty_data_cache = {0};
static USE_EXT_RAM_BSS iot_factory_struct fcty_data_cache_WR = {0};
static uint8_t is_big_order = 0xff;

/*------------------------------------------------------------------------------*/

void serial_recv_callback(UART_STRUCT *uart_struct);

/*------------------------------------------------------------------------------*/

extern void app_log_level_set(void);
extern void parameter_iot_factory_read(void);

/*------------------------------------------------------------------------------*/

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

/*------------------------------------------------------------------------------
 Function: convertToUint64
 -----------------------------------------------------------------------------*/
/**
  * @brief      uint16_t i[4]转化为uint64_t （用于SN码标定）
  * @param[in]  uint16_t *i  
  * @param[out] None
  * @return     uint64_t
  */
static uint64_t convertToUint64(uint16_t *i) {
    uint64_t result = 0;
    for (int j = 0; j < 4; j++) {
        result |= ((uint64_t)i[j]) << (16 * (3-j));
    }
    return result;
}

/**
  * @brief      uint64_t转化为uint16_t i[4] （用于SN码解析）
  * @param[in]  uint64_t val  
  * @param[out] uint16_t *i
  * @return     None
  */
static void convertFromUint64(uint64_t val, uint16_t *i) {
    for (int j = 0; j < 4; j++) {
        i[j] = (val >> (16 * (3-j))) & 0xFFFF;
    }
}

#define UTIL_SWAP_16(N)     ((((uint16_t)(N) & 0xff00) >> 8) | \
                            (((uint16_t)(N) & 0x00ff) << 8))

/**
  * @brief cpu little or big endian judgment
  * 
  * @return big endian return 1, little endian return 0
  */
static int swap_is_cpu_big_endian(void)
{
    union {
        uint32_t i;
        uint8_t s[4];
    } c;

    c.i = 0x12345678;
    return (0x12 == c.s[0]);
}

/**
 * @brief 16-bit host to net byte order conversion
 * 
 * @param host hsot byte order data
 * @return net hsot byte order data
 */
static uint16_t swap_htons(uint16_t host)
{
	if(is_big_order == 0xff) {
		is_big_order = swap_is_cpu_big_endian();
	}
   return is_big_order ? host : UTIL_SWAP_16(host);
}

/*-----------------------------------------------------------------------------------------------------------------------*/

#define FCTY_TYPE_ADDR	    29701   // 标定协议-设备类型地址
#define FCTY_SN_ADDR	    29713   // 标定协议-设备SN地址
#define FCTY_CODE_ADDR	    29717   // 标定协议-设备安全码地址
#define FCTY_TEST_MODE_SET  30901   // 0xA2:重启, 0xA3:IOT退出标定模式

#define FCTY_HIGH_IN_FRONT_ENABLE   // 标定协议-高位在前使能

static uint8_t cal_running = 0;		// 标定运行标志
static uint16_t fcty_test_mode_set_WR = 0;

static esp_err_t app_uart0_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART0_BAUD,             /* 波特率 115200 */
        .data_bits = UART_DATA_8_BITS,  /*8个数据位*/
        .parity = UART_PARITY_DISABLE,  /*没有奇偶校验*/
        .stop_bits = UART_STOP_BITS_1,  /*1个停止位*/
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, /*不用流控*/
        .source_clk = UART_SCLK_XTAL,    /*串口时钟源*/ 
    };

	//Install UART driver, and get the queue.
	ESP_ERROR_CHECK(uart_driver_install(UART0, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 10, &uart0_event_queue, ESP_INTR_FLAG_SHARED)); // ESP_INTR_FLAG_IRAM
	ESP_ERROR_CHECK(uart_param_config(UART0, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(UART0, UART0_TXD, UART0_RXD, UART0_RTS, UART0_CTS)); //设置串口引脚

    return ESP_OK;
}

static esp_err_t app_uart0_deinit(void)
{
    // 删除UART驱动程序，释放资源
    esp_err_t err = uart_driver_delete(UART0);
    if (err != ESP_OK) {
        return err; // 如果删除失败，返回错误代码
    }
    ESP_LOGI(UART0_TAG, "app_uart0_driver deleted");
    return ESP_OK; // 成功撤销初始化
}


/**
* @brief :app_uart0_rx_task
从uart rx 队列
* @param[in] :	   
* @param[out] : 
* @return :	 
*/
static void app_uart0_rx_task(void *pvParameters)
{
    uint32_t wait_time = portMAX_DELAY;
    uint32_t len = 0;

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
//				Modbus_SlaveRespones(struct_uart0, struct_uart0->Rxbuffer, struct_uart0->rxBytesNum);
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

static void app_uart0_close_tasks(void)
{
    if (app_uart0_rx_task_handle != NULL)
    {
        vTaskDelete(app_uart0_rx_task_handle);
        app_uart0_rx_task_handle = NULL;
        ESP_LOGI(UART0_TAG, "app_uart0_rx_task deleted");
    }

//    if (app_uart0_tx_task_handle != NULL)
//    {
//        vTaskDelete(app_uart0_tx_task_handle);
//        app_uart0_tx_task_handle = NULL;
//        ESP_LOGI(UART0_TAG, "app_uart0_Tx_task deleted");
//    }

    app_uart0_deinit();
    
    if(struct_uart0 != NULL)
    {
        free(struct_uart0);
        struct_uart0 = NULL;
        ESP_LOGI(UART0_TAG, "struct_uart0 deleted");
    }
}

static void app_uart0_init_task(void)
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
}

static void fcty_cal_init(void)
{
    app_uart0_init_task();
}

static void fcty_cal_deinit(void)
{
    app_uart0_close_tasks();
}

void fcty_cal_app(void)
{
    // const uint64_t Default_Safe_code = 0;
    const uint32_t Delay_time_ms = 20000;

    parameter_iot_factory_read();
    fcty_cal_init();
    vTaskDelay(pdMS_TO_TICKS(500));                            // 500ms延迟，等待运行正常

	/* 如果设备SN不是默认SN，则设备已被标定，直接返回 */
	if((strlen(iot_factory.iot_type) == 0)
        ||(iot_factory.iot_sn == IOT_FACTORY_DEFAULT_DEV_SN)
        ||(iot_factory.safe_code == IOT_FACTORY_DEFAULT_SAFE_CODE)
        ||(cal_running == 1))
    {
        uint8_t update_cfg = 0;
        uint8_t update_cfg_sn = 0;
        uint8_t update_cfg_safecode = 0;
        size_t data_len = sizeof(iot_factory);
        
        /*设备类型无效，恢复默认*/
        if(strlen(iot_factory.iot_type) == 0) {
            memcpy(iot_factory.iot_type, IOT_FACTORY_DEFAULT_DEV_TYPE, sizeof(IOT_FACTORY_DEFAULT_DEV_TYPE));
        }    
        
        /*初始化modbus读取变量*/
        memcpy(&fcty_data_cache, &iot_factory, sizeof(iot_factory));
        memcpy(&fcty_data_cache_WR, &iot_factory, sizeof(iot_factory));
        
        ESP_LOGW (UART0_TAG, "old iot type: %.*s, iot sn: %llu, iot safet: %llu",
          (int)strnlen(iot_factory.iot_type, TYPE_SIZE),
          iot_factory.iot_type,
          iot_factory.iot_sn,
          iot_factory.safe_code);
    	ESP_LOGW(UART0_TAG, "enter factory calibration procedure...");
        
    	/* 关闭所有日志输出，避免对上位机接收造成影响 */
    	esp_log_level_set("*", ESP_LOG_NONE);

    	/* 记录标定程序开始时间 */
    	uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
    	for ( ; ; )
    	{
            /*要求强制退出标定模式*/
            if(fcty_test_mode_set_WR == 0xA3) {
                fcty_cal_deinit();
                app_log_level_set();
                ESP_LOGW(UART0_TAG, "Cmd to exit factory calibration procedure.");   
                break;
            }

            /*检查设备类型是否更新*/
            if (strncmp(fcty_data_cache.iot_type, fcty_data_cache_WR.iot_type, sizeof(fcty_data_cache_WR.iot_type))) {
                ESP_LOGW (UART0_TAG, "new iot type: %s", fcty_data_cache_WR.iot_type);
                nvs_store_write(IOT_FACTORY, (const uint8_t *)&fcty_data_cache_WR, data_len);   // 变化时立即写入flash，避免重复标定可能产生的问题
                update_cfg = 1;
            }

    	    /* 检查设备SN是否更新 */
            if (fcty_data_cache.iot_sn != fcty_data_cache_WR.iot_sn) {
                ESP_LOGW (UART0_TAG, "new iot sn: %llu", fcty_data_cache_WR.iot_sn);
                nvs_store_write(IOT_FACTORY, (const uint8_t *)&fcty_data_cache_WR, data_len);   // 变化时立即写入flash，避免重复标定可能产生的问题
                update_cfg = 1;
                update_cfg_sn = 1;
            }
            
    	    /* 检查设备安全码是否更新 */
            if (fcty_data_cache.safe_code != fcty_data_cache_WR.safe_code) {
                ESP_LOGW (UART0_TAG, "new iot safecode: %llu", fcty_data_cache_WR.safe_code);
                nvs_store_write(IOT_FACTORY, (const uint8_t *)&fcty_data_cache_WR, data_len);   // 变化时立即写入flash，避免重复标定可能产生的问题
                update_cfg = 1;
                update_cfg_safecode = 1;
            }

            if (update_cfg)
            {
                /* 设备信息变动，回读IOT出厂信息 */
                nvs_store_read(IOT_FACTORY, (uint8_t *)&iot_factory, &data_len);
                memcpy(&fcty_data_cache, &iot_factory, sizeof(iot_factory));

                update_cfg = 0;
            }
            
    		uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

            /*上位机交互中，清除超时*/
            if (cal_running == 1) {
                cal_running = 0;
                start_time = now_time;
            }

            /*设备信息更新完成，执行对应操作*/
            if (update_cfg_sn & update_cfg_safecode) 
            {
#ifndef MASS_PRODUCTION_CONFIG_ENABLE  // 调试模式：软件复位关机校验
                
                /*停止串口交互，预备关机校验*/
                vTaskDelay(pdMS_TO_TICKS(1000));
                fcty_cal_deinit();
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
                while(1);
                
#else                                  // 出厂模式：等待人工下电关机校验（预留软件复位协议接口）
            
                if(fcty_test_mode_set_WR == 0xA2) {
                    esp_restart();
                    while(1);
                }
                
#endif
            }
            else
            {
                /* 当启动后，如果在一定时间内未进行标定操作，则退出标定流程 */
                if ((now_time - start_time) >= Delay_time_ms)//20s
                {
                    /*上位机断开连接，超时退出*/
                    fcty_cal_deinit();
                    app_log_level_set();
                    ESP_LOGW(UART0_TAG, "Timeout! Exit factory calibration procedure.");   
                    break;
                }
            }

    		vTaskDelay(pdMS_TO_TICKS(50));
    	}
    }
    else
    {
        /*已标定，不执行标定程序*/
        fcty_cal_deinit();
    }
}

/**
 * @brief 标定错误响应
 *
 * @param buff_in 输入缓存
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
static void fcty_rsp_error(uint8_t *buff_in, uint8_t *buff_out, int *out_len)
{
	uint8_t idx = 0;
	buff_out[idx++] = buff_in[0];
	buff_out[idx++] = buff_in[1] | MB_FUNCODE_ERROR;
	buff_out[idx++] = MB_ERROR_NOT_SUPPORTED;
	uint16_t crc = crc16_modbus_calc(buff_out, idx);
	buff_out[idx++] = crc;
	buff_out[idx++] = crc >> 8;
	*out_len = idx;
}

/*------------------------------------------------------------------------------
 Function: reverseArray
 -----------------------------------------------------------------------------*/
/**
  * @brief      数组反转
  * @param[in]  uint8_t* array  
                int length      
  * @param[out] None
  * @return     static void
  */
static void reverseArray(uint8_t* array, int reg_write_nums) {
    int start = 0;
    int end = reg_write_nums*2 - 1;
    while (start < end) {
        // 交换元素
        uint8_t temp = array[start];
        array[start] = array[end];
        array[end] = temp;
        // 移动指针
        start++;
        end--;
    }
}

/**
 * @brief 标定读数据处理
 * 
 * @param buff_in 输入缓存
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
static void fcty_read_handler(uint8_t *buff_in, uint8_t *buff_out, int *out_len)
{
	uint16_t reg_addr = ((uint16_t)buff_in[2] << 8) | buff_in[3];
	uint16_t reg_nums = ((uint16_t)buff_in[4] << 8) | buff_in[5];
    uint16_t reg_write_nums = 0;
	/* 组装modbus读响应数据 */
	int idx = 0;
    int k;
	buff_out[idx++] = buff_in[0];		// 地址
	buff_out[idx++] = buff_in[1];		// 功能码
	buff_out[idx++] = reg_nums * 2;		// 字节数

	for (int i = 0; i < reg_nums; )
	{
		reg_addr += reg_write_nums;
		uint16_t *pdata = (uint16_t*)&buff_out[idx];

		if (reg_addr == FCTY_TYPE_ADDR)			// 读设备类型
		{
			memcpy(&buff_out[idx], iot_factory.iot_type, sizeof(iot_factory.iot_type));
            reg_write_nums = (sizeof(iot_factory.iot_type) / 2);
			i += reg_write_nums;

			/* 每个寄存器高低字节交换 */
			for (int cnt = 0, k = reg_addr; k < (reg_addr + reg_write_nums); k++, cnt++) {
				pdata[cnt] = swap_htons(pdata[cnt]);
			}

            idx += sizeof(iot_factory.iot_type);
		}
		/* 整形数据的高低字节序由标定文件定义 */
		else if (reg_addr == FCTY_SN_ADDR)		// 读设备SN
		{
			memcpy(&buff_out[idx], &iot_factory.iot_sn, sizeof(iot_factory.iot_sn));
            reg_write_nums = (sizeof(iot_factory.iot_sn) / 2);
			i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
            
			reverseArray((uint8_t*)pdata, reg_write_nums);

            #endif
            
			idx += sizeof(iot_factory.iot_sn);
		}
		/* 整形数据的高低字节序由标定文件定义 */
		else if (reg_addr == FCTY_CODE_ADDR)	// 读设备安全码
		{
			memcpy(&buff_out[idx], &iot_factory.safe_code, sizeof(iot_factory.safe_code));
            reg_write_nums = (sizeof(iot_factory.safe_code) / 2);
			i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
            
			reverseArray((uint8_t*)pdata, reg_write_nums);

            #endif
            
			idx += sizeof(iot_factory.safe_code);
		}
		else
		{
			fcty_rsp_error(buff_in, buff_out, out_len);
			return;
		}
	}

	/* 计算数据CRC */
	uint16_t crc = crc16_modbus_calc(buff_out, idx);
	buff_out[idx++] = crc;
	buff_out[idx++] = crc >> 8;
	*out_len = idx;
}

/**
 * @brief 标定写数据处理
 * 
 * @param buff_in 输入缓存
 * @param in_len 输入长度
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
static void fcty_write_handler(uint8_t *buff_in, int in_len, uint8_t *buff_out, int *out_len)
{
	uint16_t reg_nums;
	uint16_t *pdata;
	uint16_t reg_addr = ((uint16_t)buff_in[2] << 8) | buff_in[3];
    uint16_t reg_write_nums = 0;
    int k;

	/* 判断是否是写单个寄存器还是多个寄存器 */
	if (buff_in[1] == MB_WRITE_ONE_HOLD_REG)
	{
		reg_nums = 1;
		pdata = (uint16_t*)&buff_in[4];
	}
	else
	{
		reg_nums = ((uint16_t)buff_in[4] << 8) | buff_in[5];
		pdata = (uint16_t*)&buff_in[7];
	}

	for (int i = 0; i < reg_nums; i++)
	{
		reg_addr += reg_write_nums;

		/* 整形数据的高低字节序由标定文件定义（此处为HSB格式） */
		if (reg_addr == FCTY_SN_ADDR)			// 标定设备SN
		{
		    reg_write_nums = (sizeof(fcty_data_cache_WR.iot_sn) / 2);
            i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
        	reverseArray((uint8_t*)pdata, reg_write_nums);
            #endif
            
			memcpy(&fcty_data_cache_WR.iot_sn, pdata, sizeof(fcty_data_cache_WR.iot_sn));
			pdata += sizeof(fcty_data_cache_WR.iot_sn);
		}
		/* 整形数据的高低字节序由标定文件定义（此处为HSB格式） */
		else if (reg_addr == FCTY_CODE_ADDR)	// 标定设备安全码
		{
		    reg_write_nums = (sizeof(fcty_data_cache_WR.safe_code) / 2);
		    i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
            
        	reverseArray((uint8_t*)pdata, reg_write_nums);

            #endif
            
			memcpy(&fcty_data_cache_WR.safe_code, pdata, sizeof(fcty_data_cache_WR.safe_code));
			pdata += sizeof(fcty_data_cache_WR.safe_code);
		}
        /* 此处为标准Modbus交互，基于Modbus Beta协议*/
        else if ((reg_addr == FCTY_TEST_MODE_SET ) && (reg_nums == 1)) // 测试设置区
        {
		    i += reg_nums;
            fcty_test_mode_set_WR = (((*pdata) >> 8) | ((*pdata) << 8));
            pdata += sizeof(fcty_test_mode_set_WR);
        }
		else
		{
			fcty_rsp_error(buff_in, buff_out, out_len);
			return;
		}
	}

	/* 响应写单个寄存器 */
	if (buff_in[1] == MB_WRITE_ONE_HOLD_REG)
	{
		memcpy(buff_out, buff_in, in_len);
		*out_len = in_len;
	}
	/* 响应写多个寄存器 */
	else
	{
		memcpy(buff_out, buff_in, 6);
		*(uint16_t*)&buff_out[6] = crc16_modbus_calc(buff_out, 6);
		*out_len = 8;
	}
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
static int app_write_uart0_data(uint8_t *pdata, int len)
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

/**
 * @brief 串口接收回调函数
 * 
 * @param handle 串口对象句柄
 * @param buff 接收缓存
 * @param len 数据大小
 */
void serial_recv_callback(UART_STRUCT *uart_struct)
{
    uint8_t *buff = uart_struct->Rxbuffer;
    int len = uart_struct->rxBytesNum;
    
	/* 标定协议检测 */
	if (md_protocol_check(buff, len) != 0) return;

	/* 置位标定运行状态 */
	cal_running = 1;

	uint8_t rsp_buff[256] = {0};
	int rsp_len;

	/* 标定读数据处理 */
	if (buff[1] == MB_READ_HOLD_REG) {
		fcty_read_handler(buff, rsp_buff, &rsp_len);
	}
	/* 标定写数据处理 */
	else {
		fcty_write_handler(buff, len, rsp_buff, &rsp_len);
	}

	/* 发送标定响应数据 */
	app_write_uart0_data(rsp_buff, rsp_len);
}

