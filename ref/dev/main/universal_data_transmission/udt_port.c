#include "udt_port.h"
#include "can_data.h"
#include "iot_ota.h"
#include "can_protocol.h"
#include "app_ble.h"
#include "utils.h"

#if 1
static int udt_port_can_send_data(uint8_t *data, uint32_t len, can_send_cmd_t *can_cmd)
{
	uint16_t crc16;
   uint8_t seq = 0;
   uint8_t *p_data = data;
	uint32_t data_len = len;
	DataStruct can_data;

	// if (can_ota_status[can_cmd->node_id].period_cmd_delay > 0) return -1;

	crc16 = calcu_crc16(p_data, data_len);

	/* 首先发送写开始指令 */
	can_cmd->can_id.bit.funcode = WRIET_DATA_START;	/**< 写开始指令功能码 */
   if (!CanAckStart(1, can_cmd->can_id.all, can_cmd->data_type, can_cmd->offset, data_len, crc16)) {
       return -1;
   }

	/* 其次发送写数据指令 */
   while(data_len)
   {
       uint8_t send_len = (data_len >= FRAME_DATA_BYTES) ? (FRAME_DATA_BYTES) : (data_len);
       can_data.type = can_cmd->data_type;
       can_data.seq = seq;
       memcpy(can_data.bytes, p_data, send_len);

		can_cmd->can_id.bit.funcode = WRITE_DATA;	/**< 写数据指令功能码 */
       if (!CanAckData(1, can_cmd->can_id.all, (uint8_t *)&can_data, (send_len + FRAME_HEAD))) {
			return -1;
		}

		data_len -= send_len;
		p_data += send_len;
		seq++;
   }

	return 0;
}

#ifdef DEV_TYPE_ARM
int udt_transfer_to_iot(uint8_t addr, uint8_t *data, int size)
{
	extern void uart_send_data(uint8_t *data, int size);
	uart_send_data(data, size);
	return 0;
}

int udt_transfer_to_pack(uint8_t addr, uint8_t *data, int size)
{
	extern void can_send_data(uint8_t addr, uint8_t *data, int size);
	can_send_data(addr, data, size);
	return 0;
}
#endif

#ifdef DEV_TYPE_PACK
int udt_transfer_to_arm(uint8_t addr, uint8_t *data, int size)
{
	extern void can_send_data(uint8_t addr, uint8_t *data, int size);
	can_send_data(addr, data, size);
	return 0;
}
#endif

#ifdef DEV_TYPE_IOT
#include "iot_mqtt.h"
#include "modbus_data.h"
#include "inv_type.h"
#include "pack_type.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG 	"[UDT_PORT]"
#define UDT_MQTT_QUEUE_SIZE		512
#define UDT_BLE_QUEUE_SIZE		25

typedef struct {
	uint8_t *data;
	int data_len;
} udt_report_data_t;

static QueueHandle_t udt_mqtt_queue =NULL;
static QueueHandle_t udt_ble_queue =NULL;
//static TaskHandle_t udt_task_handle;
void udt_queue_init(void)
{
	if (udt_mqtt_queue == NULL)
	{
		udt_mqtt_queue = xQueueCreate(UDT_MQTT_QUEUE_SIZE, sizeof(udt_report_data_t));
		if (udt_mqtt_queue == NULL)
		{
			ESP_LOGE(TAG, "udt_mqtt_queue create failed");
//			return -1;
		}
	}

	if (udt_ble_queue == NULL)
	{
		udt_ble_queue = xQueueCreate(UDT_BLE_QUEUE_SIZE, sizeof(udt_report_data_t));
		if (udt_ble_queue == NULL)
		{
			ESP_LOGE(TAG, "udt_ble_queue create failed");
		}
	}
}



/*
通用数据帧上传服务器

return:
1- 有通用数据帧需要上传服务器
0-无
*/
uint8_t udt_mqtt_Report_to_Cloud(void)
//static void udt_mqtt_task(void * pvParameters)
{
	uint8_t rtn=0;
#ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
	udt_report_data_t mqtt_data;
	if (udt_mqtt_queue&&xQueueReceive(udt_mqtt_queue, &mqtt_data,  0) == pdTRUE)//非阻塞
	{
	    ESP_LOGW(TAG, "udt_mqtt_Report_to_Cloud (%d)", mqtt_data.data_len);
		iot_mqtt_send(mqtt_data.data, mqtt_data.data_len);
		free(mqtt_data.data);
		rtn =1;
	}
#endif	

	return rtn;
}

/*------------------------------------------------------------------------------
 Function: udt_ble_Report_to_app
 -----------------------------------------------------------------------------*/
/**
  * @brief      通用数据帧上传蓝牙端
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t udt_ble_Report_to_app(void)
{
	uint8_t rtn = 0;
	udt_report_data_t ble_data;
	if (udt_ble_queue && (xQueueReceive(udt_ble_queue, &ble_data,  0) == pdTRUE))//非阻塞
	{
	    ESP_LOGW(TAG, "udt_ble_Report_to_app (Len: %d)", ble_data.data_len);
//	    ESP_LOG_BUFFER_HEX_LEVEL(TAG, ble_data.data, ble_data.data_len, ESP_LOG_WARN);

        iot_ble_response(ble_data.data, ble_data.data_len, (uint8_t)BLE_FF03_CHAR_VAL);
		free(ble_data.data);
		rtn = 1;
	}

	return rtn;
}

/* CAN数据缓存到队列中,之后通过MQTT任务发送到服务端 */
int udt_transfer_to_cloud(uint8_t md_addr, uint8_t *data, int size)
{
	udt_report_data_t mqtt_data;
	mqtt_data.data = iot_malloc(size); /*  */
	
	if (mqtt_data.data == NULL)
	{
		ESP_LOGE(TAG, "malloc(%d) failed", size);
		return -1;
	}

	memcpy(mqtt_data.data, data, size);
	mqtt_data.data_len = size;
	
    /*延时，等待服务器解析*/
    vTaskDelay(pdMS_TO_TICKS(300));//

	if (udt_mqtt_queue && xQueueSendToBack(udt_mqtt_queue, &mqtt_data, pdMS_TO_TICKS(5)) != pdTRUE)
	{
		ESP_LOGE(TAG, "send message to queue timeout, queue used:%d", uxQueueMessagesWaiting(udt_mqtt_queue));
		free(mqtt_data.data);
		return -1;
	}
	return 0;
}

int udt_transfer_to_inv_pack(uint8_t md_addr, uint8_t *data, int size)
{
	uint8_t dev_addr;
#if 0
	if (IS_INV_ADDR(md_addr)) {
		dev_addr = INV_ADDR_TO_IDX(md_addr) + INV_CAN_ADDR;		// 得到实际的逆变器设备地址
	}
	else if(IS_PACK_ADDR(md_addr)) {
		dev_addr = PACK_ADDR_TO_IDX(md_addr) + PACK_CAN_ADDR;	// 得到实际的电池包设备地址
	}
	else {
		return -1;
	}
#else
	dev_addr = md_addr;
#endif

	can_send_cmd_t can_cmd = {0};
	can_cmd.data_type = CAN_TYPE_UDT_QUERY_CMD;							// 查询历史数据功能码
	can_cmd.offset = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.priority = 0x02;
	can_cmd.can_id.bit.dst = dev_addr;							// 目标设备地址
	udt_port_can_send_data(data, size, &can_cmd); 				// 发送数据到设备
	return 0;
}
#endif

#ifdef DEV_TYPE_PC
int udt_transfer_to_pc(uint8_t md_addr, uint8_t *data, int size)
{
	can_send_cmd_t can_cmd = {0};
	can_cmd.data_type = CAN_TYPE_UDT_RESP_CMD;							// 响应历史数据功能码
	can_cmd.offset = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.priority = 0x02;
	can_cmd.can_id.bit.dst = 0x00;								// PC端CAN地址
	udt_port_can_send_data(data, size, &can_cmd); 				// 发送数据到PC
	return 0;
}

int udt_transfer_to_IotMaster(uint8_t md_addr, uint8_t *data, int size)
{
	can_send_cmd_t can_cmd = {0};
	can_cmd.data_type = CAN_TYPE_UDT_QUERY_CMD;							// 响应历史数据功能码
	can_cmd.offset = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.priority = 0x02;
	can_cmd.can_id.bit.dst = 0x01;								// 主机端CAN地址
	udt_port_can_send_data(data, size, &can_cmd); 				// 发送数据到PC
	return 0;
}
#endif

int udt_transfer_to_hmi(uint8_t addr, uint8_t *data, int size)
{
	return 0;
}
#endif

/*------------------------------------------------------------------------------
 Function: udt_transfer_to_ble
 -----------------------------------------------------------------------------*/
/**
  * @brief      数据缓存到队列中,之后通过BLE FF03发送
  * @param[in]  uint8_t md_addr  
                uint8_t *data    
                int size         
  * @param[out] None
  * @return     int
  */
int udt_transfer_to_ble(uint8_t md_addr, uint8_t *data, int size)
{
	udt_report_data_t ble_data;
	ble_data.data = iot_calloc(size); /*  */
	ESP_LOGI(TAG,"udt_transfer_to_ble md_addr:%d",md_addr);
	if (ble_data.data == NULL)
	{
		ESP_LOGE(TAG, "malloc(%d) failed", size);
		return -1;
	}

	memcpy(ble_data.data, data, size);
	ble_data.data_len = size;

	if (udt_ble_queue && xQueueSendToBack(udt_ble_queue, &ble_data, pdMS_TO_TICKS(300)) != pdTRUE) // 堵塞等待300ms，避免文件过大导致数据丢失（此时会影响响应线程）
	{
		ESP_LOGE(TAG, "send message to queue timeout, queue used:%d", uxQueueMessagesWaiting(udt_ble_queue));
		free(ble_data.data);
		return -1;
	}
    
	return 0;
}