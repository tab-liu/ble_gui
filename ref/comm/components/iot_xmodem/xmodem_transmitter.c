#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "xmodem.h"
#include "xmodem_transmitter.h"
#include "esp_log.h"


#define TAG 	"[XMODEM_SERVER]"

 extern  uint8_t *xm_tx_buff;

USE_EXT_RAM_BSS xmodem_server_struct gXmodem_server_Status;//xmodem OTA 主机侧变量

// private functions
static inbound_callback_t is_inbound_empty;						// 是否接收到数据
static outbound_callback_t is_outbound_full;					// 是否发送缓存满
static read_callback_t xmd_read_data;//uart rx 获取data 回调函数
 write_callback_t xmd_write_data;//static  uart tx组帧
static get_buffer_callback_t xmd_get_buffer;
 uart_send_callback_t xmd_uart_send;//uart tx 回调函数uart_xmd_send() static
static state_update_callback_t xmd_state_update;
static state_init_callback_t xmd_state_init;

// private variables
static xmodem_transmit_state_t transmit_state;//自己作为xmodem server的状态机

static const uint32_t TRANSFER_C_TIMEOUT = 6000; 				// 6 seconds
static const uint32_t TRANSFER_ACK_TIMEOUT = 6000; 				// 6 seconds
static const uint32_t TRANSFER_EOT_TIMEOUT = 6000; 				// 6 seconds
static const uint32_t TRANSFER_ETB_TIMEOUT = 6000; 				// 6 seconds

//static const uint32_t TRANSFER_C_TIMEOUT = 5000; 				// 5 seconds
//static const uint32_t TRANSFER_ACK_TIMEOUT = 5000; 				// 5 seconds
//static const uint32_t TRANSFER_EOT_TIMEOUT = 5000; 				// 5 seconds
//static const uint32_t TRANSFER_ETB_TIMEOUT = 5000; 				// 5 seconds

static const uint32_t TRANSFER_WRITE_BLOCK_TIMEOUT = 2000; 		// 2 seconds
static const uint8_t WRITE_BLOCK_MAX_RETRIES = 10; 				// max 10 retries per block
static const uint8_t WRITE_ETB_MAX_RETRIES = 10; 				// max 10 retries for ETB ACK
static uint8_t control_character = 0;
static uint8_t write_success = false;
static uint32_t returned_size = 0;
static uint8_t inbound = 0;
static uint32_t payload_buffer_position = 0;
static uint32_t payload_size = 0;
static uint8_t current_packet_id = 0;
static uint8_t write_block_retries = 0;
static uint32_t write_block_timer = 0;
static uint8_t write_etb_retries = 0;
static uint32_t send_packet_idx;
static xmodem_packet_t *current_packet;
//static uint8_t *internal_buffer;

xmodem_transmit_state_t xmodem_transmit_state_get(void)
{
	return transmit_state;
}

uint8_t xmodem_transmit_otapct_get(void)
{
    int total_packet = payload_size / XMODEM_BLOCK_SIZE;
    uint8_t otapct = (uint8_t)(send_packet_idx*100/total_packet);
    
	return (otapct < 100) ? otapct : 100;
}

uint8_t xmodem_transmit_init(uint32_t size)
{
	uint8_t result = false;
	transmit_state = XMODEM_TRANSMIT_UNKNOWN;

	if ((0 != is_inbound_empty) &&
		(0 != is_outbound_full) &&
		(0 != xmd_read_data) &&
		(0 != xmd_write_data) &&
		(0 != xmd_uart_send) &&
		(0 != xmd_get_buffer) &&
//		(0 != xmd_state_update) &&
//		(0 != xmd_state_init) &&
		(0 == size % XMODEM_BLOCK_SIZE))
	{
		transmit_state = XMODEM_TRANSMIT_INITIAL;
		result = true;
		payload_size = size;
		payload_buffer_position = 0;
		write_block_retries = 0;
		write_block_timer = 0;
		write_etb_retries = 0;

		if (current_packet == NULL)
		{
			current_packet = iot_calloc(sizeof(xmodem_packet_t));
			if (current_packet == NULL)
			{
				ESP_LOGE(TAG, "memory allocate failed for xmodem current_packet");
				return false;
			}
		}

//		if (internal_buffer == NULL)
//		{
//			internal_buffer = malloc(XMODEM_BLOCK_SIZE);
//			if (internal_buffer == NULL)
//			{
//				ESP_LOGE(TAG, "memory allocate failed for xmodem internal_buffer");
//				return false;
//			}
//		}

		memset(current_packet, 0, sizeof(xmodem_packet_t));
		current_packet_id = 0;
		send_packet_idx = 0;
		payload_buffer_position = 0;
		// write_block_timer       = current_time;
		ESP_LOGI(TAG, "server initialized");
		ESP_LOGI(TAG, "server state: init");
	}

	return result;
}

void xmodem_transmitter_cleanup(void)
{
	transmit_state = XMODEM_TRANSMIT_INITIAL;
	payload_buffer_position = 0;
	inbound = 0;
	control_character = 0;
	write_block_retries = 0;
	write_block_timer = 0;
	write_success = false;
	returned_size = 0;
	ESP_LOGI(TAG, "server cleanup");

	if (current_packet != NULL)
		{
			free(current_packet);
			current_packet = NULL;
		}
}





/*
xmodem OTA协议，自己作为服务端，主动发出接收1024B
return:
输出串口返回发送报文
*/
uint8_t Xmodem_Server_top(channel_type channel,const uint32_t current_time) //xmodem_transmit_process
//uint8_t xmodem_transmit_process(const uint32_t current_time)
{
	static uint32_t stopwatch = 0;
	static uint32_t stopwatch_eot = 0;
	static uint32_t stopwatch_etb = 0;
	static xmodem_transmit_state_t transmit_state_old = 0xFF;
    
    //ESP_LOGW(TAG, "server state: current_time:%ld",current_time);
			if(transmit_state_old != transmit_state)
				{
					transmit_state_old =transmit_state;
					ESP_LOGE(TAG, "transmit_state=:%ld",(uint32_t)transmit_state);
				}	
//	gXmodem_server_Status.used_chl =channel;

	switch (transmit_state)
	{
		case XMODEM_TRANSMIT_INITIAL:							// 初始化
		{
		//   transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK;//XMODEM_TRANSMIT_WAIT_FOR_C;
			transmit_state = XMODEM_TRANSMIT_WAIT_FOR_C;
			write_block_timer = current_time;

			

			
			ESP_LOGI(TAG, "server state: wait for 'C'");

            //xmd_state_init();
            
			break;
		}

		case XMODEM_TRANSMIT_WAIT_FOR_C:						// 等待客户端的'C'字符
		{
		#if 1
			if (current_time > (write_block_timer + TRANSFER_C_TIMEOUT))   
			{
//				if(is_inbound_empty())
				{
					transmit_state = XMODEM_TRANSMIT_ABORT_TRANSFER;
					ESP_LOGE(TAG, "server state: abort transmission, due to timeout waiting for 'C'");
				}
                // break;
			}	
			else if (!is_inbound_empty())
			{
                xmd_read_data(1, &inbound, &returned_size);

                if (returned_size > 0 && C == inbound)
                {
                    transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK;
                    current_packet_id = 1;
                    send_packet_idx = 1;
                    payload_buffer_position = 0;
                    write_block_timer = current_time;
                    ESP_LOGI(TAG, "server state: write block, payload_buffer_position:%lu", payload_buffer_position);
                }
                else
                {
                    ESP_LOGE(TAG, "server state: read data error");
                }
                //不break, 继续执行，直接发送下一个数据包
                // break;
			}
         #else
         	if (current_time > (write_block_timer + TRANSFER_C_TIMEOUT))    //lxy：强制等待TRANSFER_C_TIMEOUT后直接发送
			{
				transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK;
				current_packet_id = 1;
				send_packet_idx = 1;
				payload_buffer_position = 0;
				write_block_timer = current_time;
				ESP_LOGI(TAG, "server state: write block");
			}
         #endif    
			break;
		}

		case XMODEM_TRANSMIT_WRITE_BLOCK:						// 写数据块
		{
			if (current_time > (write_block_timer + TRANSFER_WRITE_BLOCK_TIMEOUT))
			{
				transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK_TIMEOUT;
				ESP_LOGE(TAG, "server state: write block timeout");
			}
			else
			{
                ESP_LOGI(TAG, "server state: write block start");

//                xmd_state_update();
                
				/* setup current packet */
				current_packet->preamble = STX;
				current_packet->id = current_packet_id;
				current_packet->id_complement = 0xFF - current_packet_id;
				memset(current_packet->data, 0, XMODEM_BLOCK_SIZE);//借用RAM
				xmd_get_buffer(payload_buffer_position, current_packet->data, XMODEM_BLOCK_SIZE);
				xmodem_calculate_crc(current_packet->data, XMODEM_BLOCK_SIZE, &current_packet->crc);
				xmd_write_data(1, (uint8_t *) & (current_packet->preamble));
				xmd_write_data(1, (uint8_t *) & (current_packet->id));
				xmd_write_data(1, (uint8_t *) & (current_packet->id_complement));
				xmd_write_data(XMODEM_BLOCK_SIZE, current_packet->data);
				uint8_t crc_high = (current_packet->crc) >> 8;
				uint8_t crc_low = (current_packet->crc) & (0x00FF);
				xmd_write_data(1, (uint8_t *) & (crc_high));
				xmd_write_data(1, (uint8_t *) & (crc_low));
                xmd_uart_send(&write_success);
				if (write_success)      // check if the output buffer had room
				{
					int total_packet = payload_size / XMODEM_BLOCK_SIZE;
					ESP_LOGW(TAG, "write block success, packet id: %lu, total: %d, progress: %d",
						send_packet_idx, total_packet, (uint8_t)(send_packet_idx*100/total_packet));

					/* increment for next packet */
					++current_packet_id;
					++send_packet_idx;
					payload_buffer_position = payload_buffer_position + XMODEM_BLOCK_SIZE;
					transmit_state = XMODEM_TRANSMIT_WAIT_FOR_C_ACK; // end of document
					stopwatch = current_time;
					ESP_LOGI(TAG, "server state: wait for ACK");
				}
			}
			break;
		}

		case XMODEM_TRANSMIT_WRITE_BLOCK_TIMEOUT:				// 写数据块超时
		{
			transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK_FAILED;
			ESP_LOGW(TAG, "server state: write block failed, due to timeout");
			break;
		}

		#if 0
		case XMODEM_TRANSMIT_SEND_REQUEST_FOR_TRANSFER:
			{
			static uint8_t outbound = SOH;
			static uint32_t delivered_size = 0;

			if (!is_outbound_full()) //END_OF_TRANSFER_RECE
			{
				xmd_write_data(1, &outbound, &delivered_size);

				if (0 < delivered_size) {
				transmit_state = XMODEM_TRANSMIT_WAIT_FOR_TRANSFER_ACK;
				stopwatch = current_time; // start the stopwatch to watch for a TRANSFER_ACK TIMEOUT
				}
			}
			break;
			}
		#endif

		case XMODEM_TRANSMIT_WAIT_FOR_C_ACK:					// 等待数据ACK
		{
			if (current_time > (stopwatch + TRANSFER_ACK_TIMEOUT))      
			{
//				if(is_inbound_empty())
				{
					transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK_FAILED;
					ESP_LOGE(TAG, "server state: write block failed, due to timeout waiting for ack");
				}
            }
			else if (!is_inbound_empty())
			{
//				else
				{
					xmd_read_data(1, &inbound, &returned_size);

					if (returned_size > 0)
					{
						if (ACK == inbound)
						{
							transmit_state = XMODEM_TRANSMIT_C_ACK_RECEIVED;
							ESP_LOGI(TAG, "server state: received ack");
						}
						else if (NACK == inbound)
						{
							transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK_FAILED;
							ESP_LOGE(TAG, "server state: write block faield, due to receive nack");
						}
						// else if (EOT == inbound)
						// {
						// 	transmit_state = XMODEM_TRANSMIT_COMPLETE;
						// 	ESP_LOGI(TAG, "server state: transmission complete");
						// }
						else if (CAN == inbound)
						{
							transmit_state = XMODEM_TRANSMIT_ABORT_TRANSFER;
							ESP_LOGE(TAG, "server state: abort transmission, due to slave cancel requestion");
						}
					}
				}
			}
			break;
		}

		case XMODEM_TRANSMIT_WRITE_BLOCK_FAILED:				// 写数据块失败
		{
			if (WRITE_BLOCK_MAX_RETRIES <= write_block_retries)
			{
				transmit_state = XMODEM_TRANSMIT_ABORT_TRANSFER;
				ESP_LOGI(TAG, "server state: abort transmission, due to retry maximun");
			}
			else
			{
				transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK;
				write_block_timer = current_time;
				++write_block_retries;
				current_packet_id--;
				send_packet_idx--;
				payload_buffer_position -= XMODEM_BLOCK_SIZE;
				ESP_LOGW(TAG, "server state: write block, due to write failed and re-write: %d", write_block_retries);
			}
			break;
		}

		case XMODEM_TRANSMIT_ABORT_TRANSFER:					// 终止传输
		{
//		    xmd_state_update();
			control_character = CAN;
			uint8_t result = false;
			xmd_write_data(1, &control_character);
            xmd_uart_send(&result);
			ESP_LOGI(TAG, "server state: cancel transmission");
			//final state
			break;
		}

		case XMODEM_TRANSMIT_C_ACK_RECEIVED:					// 接收到ACK
		{
			if (payload_buffer_position >= payload_size)
			{
				transmit_state = XMODEM_TRANSMIT_WRITE_EOT;
				ESP_LOGI(TAG, "server state: write EOT");
			}
			else
			{
				transmit_state = XMODEM_TRANSMIT_WRITE_BLOCK;
				write_block_timer = current_time;
				write_block_retries = 0;
				ESP_LOGI(TAG, "server state: write block, and clear retry count");
			}
			break;
		}

		case XMODEM_TRANSMIT_WRITE_EOT:						// 写EOT控制符(传输完成)
		{
//		    xmd_state_update();
			control_character = EOT;
			uint8_t result = false;
			xmd_write_data(1, &control_character);
            xmd_uart_send(&result);

			if (result)
			{
				transmit_state = XMODEM_TRANSMIT_WAIT_FOR_EOT_ACK;
				stopwatch_eot = current_time;
				ESP_LOGI(TAG, "server state: wait for EOT ack");
			}
			break;
		}

		case XMODEM_TRANSMIT_WAIT_FOR_EOT_ACK:				// 等待EOT(传输完成)的ACK响应
		{
			if (current_time > (stopwatch_eot + TRANSFER_EOT_TIMEOUT))   
			{
//				if(is_inbound_empty())
				{
					transmit_state = XMODEM_TRANSMIT_TIMEOUT_EOT;
					ESP_LOGE(TAG, "server state: wait for EOT timeout");
				}
            }
			else if (!is_inbound_empty())
			{
//				else
				{
					xmd_read_data(1, &inbound, &returned_size);

					if (returned_size > 0)
					{
						if (ACK == inbound)
						{
							transmit_state = XMODEM_TRANSMIT_COMPLETE; //XMODEM_TRANSMIT_WRITE_ETB;
							ESP_LOGI(TAG, "server state: transmission complete");
						}
						else if (NACK == inbound)
						{
							transmit_state = XMODEM_TRANSMIT_ABORT_TRANSFER;
							ESP_LOGI(TAG, "server state: abort transmission");
						}
					}
				}
			}
			break;
		}

		case XMODEM_TRANSMIT_TIMEOUT_EOT:						// 等待EOT(传输完成)响应超时
		{
			transmit_state = XMODEM_TRANSMIT_ABORT_TRANSFER;
			ESP_LOGW(TAG, "server state: abort transmission, due to EOT timeout");
			break;
		}

		case XMODEM_TRANSMIT_WRITE_ETB:						// 结束发送阻塞
		{
//		    xmd_state_update();
			control_character = ETB;
			uint8_t result = false;
			xmd_write_data(1, &control_character);
            xmd_uart_send(&result);
			ESP_LOGI(TAG, "write ETB");

			if (result)
			{
				transmit_state = XMODEM_TRANSMIT_WAIT_FOR_ETB_ACK;
				stopwatch_etb = current_time;
				ESP_LOGI(TAG, "server state: wait for ETB ack");
			}
			break;
		}

		case XMODEM_TRANSMIT_WAIT_FOR_ETB_ACK:				// 等待ETB ACK
		{
			if (current_time > (stopwatch_etb + TRANSFER_ETB_TIMEOUT))  
			{
//				if(is_inbound_empty())
				{
					transmit_state = XMODEM_TRANSMIT_TIMEOUT_ETB;
					ESP_LOGE(TAG, "server state: wait for ETB ack timeout");
				}
            }
			else if (!is_inbound_empty())
			{
//				else
				{
					xmd_read_data(1, &inbound, &returned_size);

					if (returned_size > 0)
					{
						if (ACK == inbound)
						{
							transmit_state = XMODEM_TRANSMIT_COMPLETE;
							ESP_LOGI(TAG, "server state: transmission complete");
						}
						else if (NACK == inbound)
						{
							transmit_state = XMODEM_TRANSMIT_ABORT_TRANSFER;
							ESP_LOGI(TAG, "server state: abort transmission");
						}
					}
				}
			}
			break;
		}

		case XMODEM_TRANSMIT_TIMEOUT_ETB:						// 等待ETB ACK超时
		{
			if (WRITE_ETB_MAX_RETRIES >= write_etb_retries)
			{
				++write_etb_retries;
				transmit_state = XMODEM_TRANSMIT_WRITE_ETB;
				ESP_LOGE(TAG, "server state: write ETB, etb_retries:%d", write_etb_retries);
			}
			else
			{
				transmit_state = XMODEM_TRANSMIT_COMPLETE;
				ESP_LOGI(TAG, "server state: transmission complete");
			}
		}//FALL THROUGH

		case XMODEM_TRANSMIT_COMPLETE:						// 传输完成
		{
//			control_character = EOT;
//			xmd_write_data(1, &control_character);
//            xmd_uart_send(&write_success);
//			//final state
//			ESP_LOGI(TAG, "write EOT");
			break;
		}

		case XMODEM_TRANSMIT_UNKNOWN:							// 未知错误
		{
			transmit_state = XMODEM_TRANSMIT_ABORT_TRANSFER;
			ESP_LOGI(TAG, "server state: abort transmission, due to unknown state");
			break;
		}

		default:
		{
			transmit_state = XMODEM_TRANSMIT_UNKNOWN;
			ESP_LOGI(TAG, "server state: unknown state");
		}
	}

	return true;
}

void xmodem_transmitter_set_callback_write(write_callback_t callback)
{
	xmd_write_data = callback;
}

void xmodem_transmitter_set_callback_uart_send(uart_send_callback_t callback)
{
	xmd_uart_send = callback;
}

void xmodem_transmitter_set_callback_read(read_callback_t callback)
{
	xmd_read_data = callback;
}

void xmodem_transmitter_set_callback_is_outbound_full(outbound_callback_t callback)
{
	is_outbound_full = callback;
}

void xmodem_transmitter_set_callback_is_inbound_empty(inbound_callback_t callback)
{
	is_inbound_empty = callback;
}

void xmodem_transmitter_set_callback_get_buffer(get_buffer_callback_t callback)
{
	xmd_get_buffer = callback;
}

void xmodem_transmitter_set_callback_state_update(state_update_callback_t callback)
{
	xmd_state_update = callback;
}

void xmodem_transmitter_set_callback_state_init(state_init_callback_t callback)
{
	xmd_state_init = callback;
}

