#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "can_transmit.h"
#include "esp_log.h"
//#include "can_init.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "can_protocol.h"
#include "comm_define.h"
#include "iot_sleep.h"
#include "aiot_log.h"
#include "aiot_can_log.h"

#define TAG "[CAN_TRANSMIT]"


extern QueueHandle_t xCanBusQueue_Recv;
extern QueueHandle_t xCanBusQueue_Send;
 
#ifndef CONFIG_CAN_RX_GPIO_PIN
#define CONFIG_CAN_RX_GPIO_PIN         GPIO_NUM_47
#endif

#ifndef CONFIG_CAN_TX_GPIO_PIN
#define CONFIG_CAN_TX_GPIO_PIN         GPIO_NUM_48
#endif

#define CAN_RX_GPIO_PIN   CONFIG_CAN_RX_GPIO_PIN
#define CAN_TX_GPIO_PIN   CONFIG_CAN_TX_GPIO_PIN
 
#define CAN_IOT_ID          0x00000001
 
#define ACCEPT_ALL() {.acceptance_code = 0, .acceptance_mask = 0xFFFFFFFF, .single_filter = true}
 // #define TWAI_FILTER_CONFIG_ACCEPT_TARGET() {.acceptance_code = ~CAN_IOT_ID, .acceptance_mask = ~CAN_IOT_ID, .single_filter = true}
#define TWAI_FILTER_CONFIG_ACCEPT_TARGET() ACCEPT_ALL()

#if CONFIG_CAN_BAUD_KBITS == 1000
#define TWAI_TIMING_CONFIG_KBITS()  TWAI_TIMING_CONFIG_1MBITS() 
#elif CONFIG_CAN_BAUD_KBITS == 500
#define TWAI_TIMING_CONFIG_KBITS()  TWAI_TIMING_CONFIG_500KBITS()
#elif CONFIG_CAN_BAUD_KBITS == 250
#define TWAI_TIMING_CONFIG_KBITS()  TWAI_TIMING_CONFIG_250KBITS()
#elif CONFIG_CAN_BAUD_KBITS == 100
#define TWAI_TIMING_CONFIG_KBITS()  TWAI_TIMING_CONFIG_100KBITS()
#endif

// CAN驱动初始化标志
static bool Can_Driver_Install_Flag = false;

void esp_canbus_init(void)
{
	twai_general_config_t can_config	 = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO_PIN, CAN_RX_GPIO_PIN, TWAI_MODE_NORMAL);
	twai_timing_config_t  baud_config	 = TWAI_TIMING_CONFIG_KBITS();
	twai_filter_config_t  filter_config  = TWAI_FILTER_CONFIG_ACCEPT_TARGET(); //

	can_config.rx_queue_len = 512;  // 设置底层can接收缓存队列长度
	can_config.tx_queue_len = 256;  // 设置底层can发送缓存队列长度
	if (twai_driver_install(&can_config, &baud_config, &filter_config) != ESP_OK) {
		ESP_LOGE("Can_Init", "Failed to install driver");
	} else {
		if (twai_start() != ESP_OK) {
			ESP_LOGE("Can_Init", "Failed to start driver");
			return;
		}
		ESP_LOGW("Can_Init", "can start successfully");
        Can_Driver_Install_Flag = true;
	}
}

esp_err_t esp_canbus_deinit(void)
{
    esp_err_t ret;
    
    Can_Driver_Install_Flag = false;
        
    /*只停止TWAI驱动，不卸载以避免崩溃*/
    ret = twai_stop();
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW("Can_Init", "TWAI driver not in running state");
        } else {
            ESP_LOGE("Can_Init", "Failed to stop TWAI driver: %s", esp_err_to_name(ret));
            return 0;
        }
    } else {
        
        /*手动清空RX队列*/
        ret = twai_clear_receive_queue();
        if (ret == ESP_OK) {
            // ESP_LOGW("Can_Init", "TWAI RX queue cleared successfully");
        } else {
            ESP_LOGW("Can_Init", "Failed to clear TWAI RX queue: %s", esp_err_to_name(ret));
        }
    }

    ret = twai_driver_uninstall();
    if(ret != ESP_OK){
        ESP_LOGE("Can_Init", "Failed to uninstall TWAI driver: %s", esp_err_to_name(ret));
    }
    /*等待确保停止完全生效*/
    vTaskDelay(pdMS_TO_TICKS(200));
    
    ESP_LOGW("Can_Init", "TWAI driver stopped and uninstalled successfully");
    return ret;
}

#ifdef CONFIG_ENABLE_CAN_RECV_TASK
void esp_canbus_recv_task(void * pvParameters)
{
    twai_message_t can_message;

	while(1)
	{

	    can_queue_msg_struct queue_msg = {NULL};
		if (Can_Driver_Install_Flag && xCanBusQueue_Recv && twai_receive(&can_message, portMAX_DELAY) == ESP_OK) //portMAX_DELAY,非阻塞
		{
			// ESP_LOGI(__func__, "can_message.identifier: 0x%08lx, type: 0x%02x", can_message.identifier, can_message.data[0]);

			queue_msg.pdata = (twai_message_t *)iot_calloc(sizeof(twai_message_t));
			if (!queue_msg.pdata) 
			{
				ESP_LOGE(__FUNCTION__, "Received can message and malloc failed");
			} 
			else 
			{
				memcpy(queue_msg.pdata, &can_message, sizeof(twai_message_t));
				// 消息保存到队列
				if (xQueueSendToBack((QueueHandle_t)xCanBusQueue_Recv, &queue_msg, portMAX_DELAY) != pdPASS) 
				{
					ESP_LOGE(__FUNCTION__, "Can received message push queue failed");
					free(queue_msg.pdata);
					queue_msg.pdata = NULL;
				}
				else
				{
					// reals.canrx_push_cnt++;
				}
			}
		}
	}
}

#else
void esp_canbus_recv_task(void * pvParameters)
{
    twai_message_t can_message;
	can_queue_msg_struct queue_msg = {NULL};

	uint8_t scnt=0;
	scnt = 10;//FIFO len

	while(scnt--)
	{
		if (Can_Driver_Install_Flag && xCanBusQueue_Recv && twai_receive(&can_message, 0) == ESP_OK) //portMAX_DELAY
		{
			queue_msg.pdata = (twai_message_t *)iot_calloc(sizeof(twai_message_t));
			if (!queue_msg.pdata) 
			{
				ESP_LOGE(__FUNCTION__, "Received can message and malloc failed");
			} 
			else 
			{
				memcpy(queue_msg.pdata, &can_message, sizeof(twai_message_t));
				if (xQueueSendToBack((QueueHandle_t)xCanBusQueue_Recv, &queue_msg, 0) != pdPASS) //portMAX_DELAY
				{
					ESP_LOGE(__FUNCTION__, "Can received message push queue failed");
					free(queue_msg.pdata);
					queue_msg.pdata = NULL;
				}
				else
				{
//					reals.canrx_push_cnt++;
				}
			}
		}
	}
}

#endif

/**
 * @brief can总线发送任务
 *
 * @param pvParameters
 * @return ****** void
 */
void esp_canbus_send_task(void ) 
{
    twai_message_t *can_message;
	uint32_t count = 1;

	while(count != 0) // 1
	{
		can_queue_msg_struct queue_msg = {NULL};
		if(Can_Driver_Install_Flag && xCanBusQueue_Send && xQueueReceive(xCanBusQueue_Send, &queue_msg, 0 ) == pdPASS ) // portMAX_DELAY
		{
			if (queue_msg.pdata) {
				can_message = (twai_message_t *)queue_msg.pdata;
				if (twai_transmit(can_message, pdMS_TO_TICKS(5)) != ESP_OK) // pdMS_TO_TICKS(10)
				{
                    ESP_LOGE(TAG,
                        "Send <Failed> [ID:0x%04lx, Len:%d], data: %02X %02X %02X %02X %02X %02X %02X %02X",
                        can_message->identifier,
                        can_message->data_length_code,
                        can_message->data[0], can_message->data[1], can_message->data[2], can_message->data[3],
                        can_message->data[4], can_message->data[5], can_message->data[6], can_message->data[7]
                    );
				}

#if CONFIG_LOG_UDP_REDIRECT == 2 
                // wlcc日志
                supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
                supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
                if ((1 == resource.can) && (1 == direction.tx))
                {
                    aiot_log_can_frame_t log_frame = {.id.value = 0, .data = {0}};
                    log_frame.id.identifier = can_message->identifier & 0x1FFFFFFF; // 仅保留29位ID
                    log_frame.id.tx = 1; // 标记为发送帧
                    memcpy(log_frame.data, can_message->data, sizeof(can_message->data));
                    aiot_log_push((const uint8_t *)&log_frame, (uint16_t)sizeof(log_frame), LOG_TYPE_CAN_TX);
                }

                // can log本地存储
                super_cmd_can_log_cfg_t can_log_cfg = { .reg_val = reals.supper_control.cmd_list[SUPPER_CMD_CAN_LOG_SAVE].value };
                if ((CAN_LOG_STATUS_RECORDING == can_log_cfg.bit.record_status) 
                    && ((CAN_LOG_DIR_TX_ONLY == can_log_cfg.bit.can_direction)
                        || (CAN_LOG_DIR_TXRX_ALL == can_log_cfg.bit.can_direction)))
                {
                    aiot_log_can_frame_t log_frame = {.id.value = 0, .data = {0}};
                    log_frame.id.identifier = can_message->identifier & 0x1FFFFFFF; // 仅保留29位ID
                    log_frame.id.tx = 1; // 标记为发送帧
                    memcpy(log_frame.data, can_message->data, sizeof(can_message->data));
                    AIoT_Can_Log_Push((const uint8_t *)&log_frame, (uint16_t)sizeof(log_frame), LOG_TYPE_CAN_TX);
                }
#endif

				free(queue_msg.pdata);
				queue_msg.pdata = NULL;

				/* 加快OTA时的速度处理 */
				int msg_size = uxQueueMessagesWaiting(xCanBusQueue_Send);//被查询 queue 现在包含的数据项的数目。如果返回 0 则 queue 为
				if(msg_size == 0)//空队列，上一帧已发送完毕，下一帧等待5ms后发送
				{
					count = 0;
					// vTaskDelay(pdMS_TO_TICKS(10));
				}
				else //OTA
				{
					if((++count) > 200) 
					{
						count = 0;
						// vTaskDelay(pdMS_TO_TICKS(10));
					}
				}
			}
		}
		else
		{
			count = 0;
		}				
	}
}

/*
CAN 底层异常恢复机制
*/
void esp_canbus_state_change(void) 
{

    twai_status_info_t status_info;
    if (twai_get_status_info(&status_info) == ESP_OK) 
	{
		// ESP_LOGI(TAG, "canbus_state:%d", canbus_state);	
        switch (status_info.state) {
            case TWAI_STATE_STOPPED:
                ESP_LOGE (TAG, "Can Bus stop and restart can...");
                twai_start();
//				reals.debug_Count_can_stop++;
//				ESP_LOGI(TAG,"reals.debug_Count_can_stop:%lu",reals.debug_Count_can_stop);
                break;

            case TWAI_STATE_BUS_OFF:
                ESP_LOGE (TAG, "Can Bus offline and retry recovery");
                //vTaskDelay(pdMS_TO_TICKS(1500));
                twai_initiate_recovery();
//				reals.debug_Count_can_busoff++;
//				ESP_LOGI(TAG,"reals.debug_Count_can_busoff:%lu",reals.debug_Count_can_busoff);
                break;

            case TWAI_STATE_RECOVERING:
                ESP_LOGI (TAG, "Can Bus recovering...");
                //twai_start(); //
                break;

            default:
                break;
        }

        if (status_info.tx_error_counter >= 128) {
//            ESP_LOGE (TAG, "Can Bus error %ld", status_info.tx_error_counter); 
        }
    }
}

void print_twai_status(void) {
    twai_status_info_t status_info;
    esp_err_t ret = twai_get_status_info(&status_info);

    if (ret == ESP_OK) {
        if(status_info.msgs_to_rx > 100){
            ESP_LOGW("TWAI", "RX Queue msgs_to_rx: %lu", status_info.msgs_to_rx); // 调试底层接收队列拥挤度
        }
#if 0
        // 打印当前状态
        ESP_LOGI("TWAI", "Current Status:");
        switch (status_info.state) {
            case TWAI_STATE_STOPPED:
                ESP_LOGI("TWAI", "State: STOPPED");
                break;
            case TWAI_STATE_RUNNING:
                ESP_LOGI("TWAI", "State: RUNNING");
                break;
            case TWAI_STATE_BUS_OFF:
                ESP_LOGI("TWAI", "State: BUS OFF");
                break;
            case TWAI_STATE_RECOVERING:
                ESP_LOGI("TWAI", "State: RECOVERING");
                break;
        }

        // 打印发送队列状态
        ESP_LOGI("TWAI", "TX Queue Status:");
		ESP_LOGI("TWAI", "msgs_to_tx: %lu", status_info.msgs_to_tx);
        ESP_LOGI("TWAI", "Messages to transmit: %lu", status_info.msgs_to_tx);
        ESP_LOGI("TWAI", "TX failed count: %lu", status_info.tx_failed_count);
        ESP_LOGI("TWAI", "TX error counter: %lu", status_info.tx_error_counter);
        ESP_LOGI("TWAI", "Arbitration lost count: %lu", status_info.arb_lost_count);

        // 打印接收队列状态
        ESP_LOGI("TWAI", "RX Queue Status:");
		ESP_LOGI("TWAI", "msgs_to_rx: %lu", status_info.msgs_to_rx);
        ESP_LOGI("TWAI", "Messages in RX queue: %lu", status_info.msgs_to_rx);
        ESP_LOGI("TWAI", "RX missed count: %lu", status_info.rx_missed_count);
        ESP_LOGI("TWAI", "RX overrun count: %lu", status_info.rx_overrun_count);
        ESP_LOGI("TWAI", "RX error counter: %lu", status_info.rx_error_counter);

        // 打印总线错误状态
        ESP_LOGI("TWAI", "Bus Status:");
        ESP_LOGI("TWAI", "Bus error count: %lu", status_info.bus_error_count);
#endif
    } 
}

