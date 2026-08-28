#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "can_transmit.h"
#include "esp_log.h"
#include "can_init.h"
#include "iot_period_task.h"



#define TAG "[CAN_TRANSMIT]"


extern QueueHandle_t xCanBusQueue_Recv;
extern QueueHandle_t xCanBusQueue_Send;




 #define CAN_RECV_TEST
#ifdef CAN_RECV_TEST
QueueHandle_t can_test_queue;
void esp_can_recv_test_task(void * pvParameters)
{
	can_test_queue = xQueueCreate(200, sizeof(twai_message_t));

	while(1)
	{
		twai_message_t cam_msg;
		if(xQueueReceive(can_test_queue, &cam_msg, portMAX_DELAY) == pdTRUE)
		{
			if((uint8_t )(cam_msg.identifier >> 8) != 0xff)
			{
				static uint32_t can_recv_cnt = 0;
				printf("inner can rx_cnt:%" PRIu32 ", recv id: 0x%08" PRIx32 ", data: ", ++can_recv_cnt, cam_msg.identifier);

				for(int i = 0; i < cam_msg.data_length_code; i++) {
					printf("%02x ", cam_msg.data[i]);
				}
				printf("\n\n");
			}
		}
	}
}
#endif

void esp_canbus_recv_task(void * pvParameters)
//void esp_canbus_recv_task(void)
{
    twai_message_t can_message;
    can_queue_msg_struct queue_msg = {NULL};

	// QueueHandle_t xCanBusQueue_Recv = (QueueHandle_t)pvParameters;
	uint8_t scnt=0;
	scnt =5;//FIFO len
//	ESP_LOGI(TAG, "esp_canbus_recv_task  ----------000");//testwx

	while(1)
	// while(scnt--)
	{
		// ESP_LOGI(TAG, "esp_canbus_recv_task  ----------111");//testwx
		if (xCanBusQueue_Recv && twai_receive(&can_message, portMAX_DELAY) == ESP_OK) //portMAX_DELAY,非阻塞
		{
//20240710 需要解析和自己源地址相同报完，之后通过 地址重复区分；排地址也需要
//			if ((can_message.identifier & 0xFF) == esp_canbus_myself_address()) //源地址和自己地址相同，不解析，超时报错
//			{
//				// return;
//				continue;
//			}
			// ESP_LOGI(TAG, "xCanBusQueue_Recv  ----------111");//testwx
			// 分配一个消息空间
			queue_msg.pdata = (twai_message_t *)heap_caps_malloc(sizeof(twai_message_t), MALLOC_CAP_SPIRAM); // malloc MALLOC_CAP_SPIRAM
			if (!queue_msg.pdata) 
			{
				ESP_LOGE(TAG, "Received can message and malloc failed");
			} 
			else 
			{
				// ESP_LOGI(TAG, "xCanBusQueue_Recv  ----------222");//testwx
				memcpy(queue_msg.pdata, &can_message, sizeof(twai_message_t));
				// 消息保存到队列
				if (xQueueSendToBack((QueueHandle_t)xCanBusQueue_Recv, &queue_msg, portMAX_DELAY) != pdPASS) 
				{
					ESP_LOGE(TAG, "Can received message push queue failed");
					free(queue_msg.pdata);
					queue_msg.pdata = NULL;
				}
				else
				{
					reals.canrx_push_cnt++;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(1));
		}
		// else
		// {
		// 	ESP_LOGE(TAG, "esp_canbus_recv_task  twai_receive failed");//
		// 	break;
		// }
	}



}




// void esp_canbus_send_task(void)
// {
// 	twai_message_t can_message2 ={
// 		.extd = 1,
// 		.rtr = 0,
// 		.identifier = 0x01010101,//0x12345678 0x0070FF00 0x08601001
// 		.data_length_code = 8,
// 		.data[0]=0x0,
// 		.data[1]=0x1,
// 		.data[2]=0x0,
// 		.data[3]=0x1,
// 		.data[4]=0x0,
// 		.data[5]=0x1,
// 		.data[6]=0x0,
// 		.data[7]=0x1

// 	};

// 	twai_message_t *can_message2_p = &can_message2;

// 	esp_err_t ret = -1;
// 	ret = twai_transmit(can_message2_p, pdMS_TO_TICKS(1000));
// 	// if (twai_transmit(can_message2_p, pdMS_TO_TICKS(1000)) != ESP_OK) 
// 	if (ret != ESP_OK)
// 	{
// 		ESP_LOGE(TAG, "esp_canbus_send_task 1   ---------error:%x",ret);//testwx
// 	}
// 	else
// 	{
// 		ESP_LOGI(TAG, "esp_canbus_send_task 1    ---------success");//testwx
// 	}

// }

//void esp_canbus_send_task(void)
//{
//    twai_message_t *can_message;
//	uint32_t count = 0;
//	can_queue_msg_struct queue_msg = {NULL};
//	// printf("go in esp_canbus_send_task!!!!!!!!!\n");
//
//	if(xCanBusQueue_Send && xQueueReceive(xCanBusQueue_Send, &queue_msg, 0) == pdPASS ) //portMAX_DELAY	0
//	{
//		//testwx
////		printf("xCanBusQueue_Send && xQueueReceive ---queue_msg\n");
//		// char *p = can_message->data;
////		for(int i = 0;i < 8;i++)
////		{
////			printf("%02x ",queue_msg.pdata->data[i]);
////		}
////		printf("\n");
//		// ESP_LOGI(TAG, "esp_canbus_send_task    ---------111");//testwx
//		
//		if (queue_msg.pdata) {
//			can_message = (twai_message_t *)queue_msg.pdata;
//			// ESP_LOGI(TAG, "esp_canbus_send_task    ---------222");//testwx
//
//			//testwx
//			// printf("xCanBusQueue_Send && xQueueReceive ---can_message\n");
//			// // char *p = can_message->data;
//			// for(int i = 0;i < 8;i++)
//			// {
//			// 	printf("%02x ",can_message->data[i]);
//			// }
//			// printf("\n");
//
//			// ESP_ERROR_CHECK(twai_transmit(can_message, pdMS_TO_TICKS(1000)));
//			esp_err_t ret = -1;
//			ret = twai_transmit(can_message, pdMS_TO_TICKS(1000));
//			// if (twai_transmit(can_message2_p, pdMS_TO_TICKS(1000)) != ESP_OK) 
//			if (ret != ESP_OK)
//			{
//				// ESP_LOGE (TAG, "Send <Failed> [ID:0x%04x, Len:%d]", can_message->identifier, can_message->data_length_code);
//				// ESP_LOGE (TAG, "Send <Failed> [ID:0x, Len:%d]", can_message->data_length_code);
//				ESP_LOGE(TAG, "esp_canbus_send_task 2   ---------error:%x",ret);//testwx
//			}
//			else
//			{
////				ESP_LOGI(TAG, "esp_canbus_send_task 2    ---------success");//testwx
//			}
//			// ESP_LOGI(TAG, "esp_canbus_send_task    ---------444");//testwx
//			free(queue_msg.pdata);
//			queue_msg.pdata = NULL;
//			// ESP_LOGI(TAG, "esp_canbus_send_task    ---------555");//testwx
//
//			/* 加快OTA时的速度处理 */
//			int msg_size = uxQueueMessagesWaiting(xCanBusQueue_Send);//被查询 queue 现在包含的数据项的数目。如果返回 0 则 queue 为
//			if(1)//(msg_size == 0) //空队列，上一帧已发送完毕，下一帧等待5ms后发送
//			{
//				count = 0;
//				vTaskDelay(pdMS_TO_TICKS(10));//5
//			}
//			else //如果待发送队列数据很多，直接不延时，不给其他线程留时间：windy ：不合理
//			{
//				if((++count) > 200) 
//				{
//					count = 0;
//					vTaskDelay(pdMS_TO_TICKS(5));
//				}
//			}
//			// ESP_LOGI(TAG, "esp_canbus_send_task    ---------666");//testwx
//		}
//	}
//
//}


/**
 * @brief can总线发送任务
 *
 * @param pvParameters
 * @return ****** void
 */
void esp_canbus_send_task(void * pvParameters) 
{
    twai_message_t *can_message;
	uint32_t count = 0;
//    QueueHandle_t sendQueue = (QueueHandle_t)pvParameters;

    while (1) 
	{

        can_queue_msg_struct queue_msg = {NULL};
        if(xCanBusQueue_Send && xQueueReceive(xCanBusQueue_Send, &queue_msg, portMAX_DELAY ) == pdPASS ) 
		{
            if (queue_msg.pdata) {
                can_message = (twai_message_t *)queue_msg.pdata;

                if (twai_transmit(can_message, pdMS_TO_TICKS(1000)) != ESP_OK) 
				{
                    ESP_LOGE (TAG, "Send <Failed> [ID:0x%04lx, Len:%d]", can_message->identifier, can_message->data_length_code);
                }
                free(queue_msg.pdata);
                queue_msg.pdata = NULL;

				/* 加快OTA时的速度处理 */
				int msg_size = uxQueueMessagesWaiting(xCanBusQueue_Send);//被查询 queue 现在包含的数据项的数目。如果返回 0 则 queue 为
				if(msg_size == 0)//空队列，上一帧已发送完毕，下一帧等待5ms后发送
				{
					count = 0;
					vTaskDelay(pdMS_TO_TICKS(10));//5
				}
				else 
				{
					if((++count) > 200) 
					{
						count = 0;
						vTaskDelay(pdMS_TO_TICKS(10));//5
					}
				}
            }
        }
    }
}


//
////uint16_t can_count = 0;
///**
// * @brief 接收CAN总线数据帧,并将数据帧存储地址，放入消息队列
// *
// * @param pvParameters : 接收CAN总线消息队列
// * @return ****** void
// */
//void esp_canbus_recv_task(void * pvParameters)
//{
//    twai_message_t can_message;
//    can_queue_msg_struct queue_msg = {NULL};
//
//    QueueHandle_t recvQueue = (QueueHandle_t)pvParameters;
//
//#ifdef CAN_RECV_TEST
//	xTaskCreatePinnedToCore(esp_can_recv_test_task, "can-test", 1024 * 3, NULL, 5, NULL, 1);
//#endif
//
//    while (1) 
//	{
//
//        if (recvQueue && twai_receive(&can_message, portMAX_DELAY) == ESP_OK) 
//		{
//            if ((can_message.identifier & 0xFF) == esp_canbus_myself_address()) 
//			{
//                continue;
//            }
//
//		#ifdef CAN_RECV_TEST
//			if(can_test_queue)
//			{
//				if (xQueueSendToBack((QueueHandle_t)can_test_queue, &can_message, portMAX_DELAY) != pdPASS) 
//				{
//					ESP_LOGE(TAG, "push can_test_queue failed");
//				}
//			}
//		#endif
//
//            // 分配一个消息空间
//            queue_msg.pdata = (twai_message_t *)heap_caps_malloc(sizeof(twai_message_t), MALLOC_CAP_SPIRAM); // malloc MALLOC_CAP_SPIRAM
//            if (!queue_msg.pdata) 
//			{
//                ESP_LOGE(TAG, "Received can message and malloc failed");
//            } 
//			else 
//			{
//                memcpy(queue_msg.pdata, &can_message, sizeof(twai_message_t));
//                // 消息保存到队列
//                if (xQueueSendToBack((QueueHandle_t)recvQueue, &queue_msg, portMAX_DELAY) != pdPASS) 
//				{
//                    ESP_LOGE(TAG, "Can received message push queue failed");
//                    free(queue_msg.pdata);
//                    queue_msg.pdata = NULL;
//                }
//            }
//            vTaskDelay(pdMS_TO_TICKS(1));
//        }
//    }
//}



/* Response Data */
void windy_can_tx(void) 
{


	/* 内部CAN发送数据 */
//    can_queue_msg_struct queue_msg;
    twai_message_t queue_msg;



//    queue_msg.pdata->extd = 1;
//    queue_msg.pdata->rtr = 0;
//    queue_msg.pdata->identifier = 0x12345678;
//    queue_msg.pdata->data_length_code = 8;
//	queue_msg.pdata->data[0]=0xA;
//	queue_msg.pdata->data[1]=0xB;
//	queue_msg.pdata->data[2]=0xC;
//	queue_msg.pdata->data[3]=0xD;
//
//	queue_msg.pdata->data[4]=0x1;
//	queue_msg.pdata->data[5]=0x2;
//	queue_msg.pdata->data[6]=0x3;
//	queue_msg.pdata->data[7]=0x4;	

    queue_msg.extd = 1;
    queue_msg.rtr = 0;
    queue_msg.identifier = 0x12345678;
    queue_msg.data_length_code = 8;
	queue_msg.data[0]=0xA;
	queue_msg.data[1]=0xB;
	queue_msg.data[2]=0xC;
	queue_msg.data[3]=0xD;

	queue_msg.data[4]=0x1;
	queue_msg.data[5]=0x2;
	queue_msg.data[6]=0x3;
	queue_msg.data[7]=0x4;	


//    memcpy(queue_msg.pdata->data, raw, len);

//	can_message = (twai_message_t *)queue_msg.pdata;

	if (twai_transmit(&queue_msg, pdMS_TO_TICKS(1000)) != ESP_OK) {
//					  ESP_LOGE (TAG, "Send <Failed> [ID:0x%04x, Len:%d]", can_message->identifier, can_message->data_length_code);
	}
	vTaskDelay(pdMS_TO_TICKS(3000));

}
//
//
///**
// * @brief can总线发送任务
// *
// * @param pvParameters
// * @return ****** void
// */
//void esp_canbus_send_task(void * pvParameters) 
//{
//    twai_message_t *can_message;
//	uint32_t count = 0;
//    QueueHandle_t sendQueue = (QueueHandle_t)pvParameters;
//
//    while (1)
////    {
////		windy_can_tx();
////
////	}
////	if(0)
//	{
//
//        can_queue_msg_struct queue_msg = {NULL};
//        if(sendQueue && xQueueReceive(sendQueue, &queue_msg, portMAX_DELAY ) == pdPASS ) 
//		{
//            if (queue_msg.pdata) {
//                can_message = (twai_message_t *)queue_msg.pdata;
//
//                if (twai_transmit(can_message, pdMS_TO_TICKS(1000)) != ESP_OK) 
//				{
////                    ESP_LOGE (TAG, "Send <Failed> [ID:0x%04x, Len:%d]", can_message->identifier, can_message->data_length_code);
//                }
//                free(queue_msg.pdata);
//                queue_msg.pdata = NULL;
//
//				/* 加快OTA时的速度处理 */
//				int msg_size = uxQueueMessagesWaiting(sendQueue);//被查询 queue 现在包含的数据项的数目。如果返回 0 则 queue 为
//				if(1)//(msg_size == 0) //空队列，上一帧已发送完毕，下一帧等待5ms后发送
//				{
//					count = 0;
//					vTaskDelay(pdMS_TO_TICKS(10));//5
//				}
//				else //如果待发送队列数据很多，直接不延时，不给其他线程留时间：windy ：不合理
//				{
//					if((++count) > 200) 
//					{
//						count = 0;
//						vTaskDelay(pdMS_TO_TICKS(5));
//					}
//				}
//            }
//        }
//    }
//}

/*
CAN 底层异常恢复机制
*/
void esp_canbus_state_change(void) 
{

    twai_status_info_t status_info;
    if (twai_get_status_info(&status_info) == ESP_OK) 
	{

        switch (status_info.state) {
            case TWAI_STATE_STOPPED:
                ESP_LOGE (TAG, "Can Bus stop and restart can...");
                twai_start();
				reals.debug_Count_can_stop++;
				ESP_LOGI(TAG,"reals.debug_Count_can_stop:%lu",reals.debug_Count_can_stop);

                break;

            case TWAI_STATE_BUS_OFF:
                ESP_LOGE (TAG, "Can Bus offline and retry recovery");
                //vTaskDelay(pdMS_TO_TICKS(1500));
                twai_initiate_recovery();
				reals.debug_Count_can_busoff++;
				ESP_LOGI(TAG,"reals.debug_Count_can_busoff:%lu",reals.debug_Count_can_busoff);

                break;

            case TWAI_STATE_RECOVERING:
                ESP_LOGI (TAG, "Can Bus recovering...");
                //twai_start(); //
                break;

            default:
                break;
        }

        if (status_info.tx_error_counter >= 128) {
//            ESP_LOGE (TAG, "Can Bus error %ld", status_info.tx_error_counter); // 发送错误次数
        }
    }
}



