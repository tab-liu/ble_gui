#include "udt_port.h"
#include "iot_ota.h"
#include "iot_mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "can_protocol.h"
#include "modbus_data.h"
#include "app_bt.h"

#define TAG 	"[UDT_PORT]"

typedef struct {
	uint8_t *data;
	int data_len;
} udt_report_data_t;

#define UDT_QUEUE_SIZE		512

static QueueHandle_t udt_mqtt_queue = NULL;
static QueueHandle_t udt_ble_queue = NULL;

static int udt_port_can_send_data(uint8_t *data, uint32_t len, can_send_cmd_t *can_cmd)
{
	uint16_t crc16;
    uint8_t seq = 0;
    uint8_t *p_data = data;
	uint32_t data_len = len;
	DataStruct can_data;

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

/*------------------------------------------------------------------------------
 Function: udt_queue_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      队列初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void udt_queue_init(void)
{
	if (udt_mqtt_queue == NULL)
	{
		udt_mqtt_queue = xQueueCreate(UDT_QUEUE_SIZE, sizeof(udt_report_data_t));
		if (udt_mqtt_queue == NULL)
		{
			ESP_LOGE(TAG, "udt_mqtt_queue create failed");
		}
	}

	if (udt_ble_queue == NULL)
	{
		udt_ble_queue = xQueueCreate(UDT_QUEUE_SIZE, sizeof(udt_report_data_t));
		if (udt_ble_queue == NULL)
		{
			ESP_LOGE(TAG, "udt_ble_queue create failed");
		}
	}
}

/*------------------------------------------------------------------------------
 Function: udt_mqtt_Report_to_Cloud
 -----------------------------------------------------------------------------*/
/**
  * @brief      通用数据帧上传服务器
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t udt_mqtt_Report_to_Cloud(void)
{
	uint8_t rtn = 0;
	udt_report_data_t mqtt_data;
	if (udt_mqtt_queue && (xQueueReceive(udt_mqtt_queue, &mqtt_data,  0) == pdTRUE))//非阻塞
	{
	    ESP_LOGD(TAG, "udt_mqtt_Report_to_Cloud (Len: %d)", mqtt_data.data_len);
	    ESP_LOG_BUFFER_HEX_LEVEL(TAG, mqtt_data.data, mqtt_data.data_len, ESP_LOG_DEBUG);
        
		iot_mqtt_send(mqtt_data.data, mqtt_data.data_len);
		free(mqtt_data.data);
		rtn = 1;
	}

	return rtn;
}

/*------------------------------------------------------------------------------
 Function: udt_transfer_to_cloud
 -----------------------------------------------------------------------------*/
/**
  * @brief      数据缓存到队列中,之后通过MQTT任务发送到服-
                务端
  * @param[in]  uint8_t md_addr  
                uint8_t *data    
                int size         
  * @param[out] None
  * @return     int
  */
int udt_transfer_to_cloud(uint8_t md_addr, uint8_t *data, int size)
{
	udt_report_data_t mqtt_data;
	mqtt_data.data = iot_calloc(size); /*  */
	
	if (mqtt_data.data == NULL)
	{
		ESP_LOGE(TAG, "malloc(%d) failed", size);
		return -1;
	}

	memcpy(mqtt_data.data, data, size);
	mqtt_data.data_len = size;

	if (udt_mqtt_queue && xQueueSendToBack(udt_mqtt_queue, &mqtt_data, pdMS_TO_TICKS(100)) != pdTRUE)
	{
		ESP_LOGE(TAG, "send message to queue timeout, queue used:%d", uxQueueMessagesWaiting(udt_mqtt_queue));
		free(mqtt_data.data);
		return -1;
	}
    
	return 0;
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
	    ESP_LOGD(TAG, "udt_ble_Report_to_app (Len: %d)", ble_data.data_len);
	    ESP_LOG_BUFFER_HEX_LEVEL(TAG, ble_data.data, ble_data.data_len, ESP_LOG_DEBUG);

        iot_ble_response(ble_data.data, ble_data.data_len, (uint8_t)BLE_FF03_CHAR_VAL);
		free(ble_data.data);
		rtn = 1;
	}

	return rtn;
}

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
	
	if (ble_data.data == NULL)
	{
		ESP_LOGE(TAG, "malloc(%d) failed", size);
		return -1;
	}

	memcpy(ble_data.data, data, size);
	ble_data.data_len = size;

	if (udt_ble_queue && xQueueSendToBack(udt_ble_queue, &ble_data, pdMS_TO_TICKS(100)) != pdTRUE)
	{
		ESP_LOGE(TAG, "send message to queue timeout, queue used:%d", uxQueueMessagesWaiting(udt_ble_queue));
		free(ble_data.data);
		return -1;
	}
    
	return 0;
}


int udt_transfer_to_can(uint8_t md_addr, uint8_t *data, int size)
{
	uint8_t dev_addr;
    
#if 0    
	if (IS_INV_ADDR(md_addr)) {
		dev_addr = INV_ADDR_TO_IDX(md_addr) + INV_CAN_ADDR;		        // 得到实际的逆变器设备地址
	} else if(IS_PACK_ADDR(md_addr)) {
		dev_addr = PACK_ADDR_TO_IDX(md_addr) + PACK_CAN_ADDR;	        // 得到实际的电池包设备地址
    } else if(IS_DCDC_ADDR(md_addr)) {
        dev_addr = md_addr - MD_DCDC_ADDR_START + DCDC_ARM_CAN_ADDR;    // 得到实际的DCDC设备地址
    } else if(IS_DCHUB_ADDR(md_addr)) {
        dev_addr = md_addr - MD_DCHUB_ADDR_START + DC_HUB_CAN_ADDR;     // 得到实际的DCDC设备地址        
	} else {
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
	return udt_port_can_send_data(data, size, &can_cmd); 				// 发送数据到设备
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
	return udt_port_can_send_data(data, size, &can_cmd); 				// 发送数据到PC
}

#endif


