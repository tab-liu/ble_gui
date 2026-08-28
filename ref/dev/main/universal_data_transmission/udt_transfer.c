#include "udt_transfer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>


#define TAG "[UDT_TRANSFER]"


static udt_transfer_t udt_transfer;					// 传输列表,包含PC端与服务端的传输函数
static udt_relay_t udt_relay;						// 中继列表,包含目标地址与其对应的发送函数
static uint8_t udt_local_addr;						// 本机地址
static uint16_t last_req_id;						// 上一次请求ID
static udt_event_t udt_event;						// 记录本次事件的类型与参数
static udt_evt_callback_t rx_evt_cb;				// 事件回调函数指针
static uint8_t udt_buffer[UDT_DATA_BUFF_SIZE];		// 数据缓存
static uint8_t udt_mode = UDT_MODE_SVC;				// 模式
static uint8_t pc_flg;
static SemaphoreHandle_t xUdtSemaphore = NULL;      // 申明互斥型信号量

/* modbus crc16 数值表 */
static const unsigned short md_crc_tbl[256] =
{
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xb981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
};

/**
 * @brief 查表法计算modbus crc
 *
 * @param ptr 数据指针
 * @param len 数据长度
 * @return modbus crc16值
 */
static uint16_t modbus_crc16_tbl(const uint8_t *ptr, uint16_t len)
{
	register uint16_t crc = 0xffff;
	while(len--)  {
		crc = ((crc >> 8) & 0xFF) ^ md_crc_tbl[(crc ^ (*(ptr++))) & 0xFF];
	}
	return (crc);
}

/**
 * @brief 通用数据传输协议检查
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return 成功返回0, 否则返回-1
 */
static int udt_protocol_check(uint8_t *data, int size)
{
	udt_protocol_t *udt_prot = (udt_protocol_t *)data;
	ESP_LOGI(TAG,"udt_protocol_check mark:%d ver:%d scene:%d",udt_prot->mark,udt_prot->version,udt_prot->scene);
	if ((udt_prot->mark != UDT_PROT_MARK) ||
		(udt_prot->version != UDT_PROT_VER) ||
		(udt_prot->scene != UDT_PROT_SCENE)) return -1;

	uint16_t crc_old = *(uint16_t *)&data[size-2];
	uint16_t crc_now = modbus_crc16_tbl(data, size-2);
	if (crc_old != crc_now) return -1;
	return 0;
}

/**
 * @brief 通用数据传输模块初始化
 *
 * @param local_addr 本机地址
 * @param transfer 传输列表
 * @param relay 中继列表
 * @param evt_cb 接收事件回调函数指针,当接收到服务端的事件时触发该函数
 * @return 无
 */
void udt_init(uint8_t local_addr, udt_transfer_t *transfer, udt_relay_t *relay, udt_evt_callback_t evt_cb)
{
	udt_local_addr = local_addr;

	if (transfer) {
		memcpy(&udt_transfer, transfer, sizeof(udt_transfer_t));
	}

	if (relay) {
		memcpy(&udt_relay, relay, sizeof(udt_relay_t));
	}

	if (evt_cb) {
		rx_evt_cb = evt_cb;
	}
}

/**
 * @brief 设置本机地址
 *
 * @param local_addr 本机地址
 * @return 无
 */
void udt_set_address(uint8_t local_addr)
{
	udt_local_addr = local_addr;
}

/**
 * @brief 向终端中继数据
 * - 在本机接收到下层设备的通用数据时,调用该函数向终端中继下层设备的数据
 *
 * @param data 数据指针
 * @param size 数据长度
 * @return 成功返回0, 否则返回-1
 */
int udt_relay_to_terminal(uint8_t *data, int size)
{
	/* 参数检测 */
	if ((data == NULL) || (size == 0)) return -1;

	/* 服务端传输检测 */
	if (udt_mode == UDT_MODE_SVC) {
		if (udt_transfer.to_server == NULL) return -1;
	}

	/* PC端传输检测 */
	if (udt_mode == UDT_MODE_PC) {
		if (udt_transfer.to_pc == NULL) return -1;
	}

	/* 蓝牙端传输检测 */
	if (udt_mode == UDT_MODE_BLE) {
		if (udt_transfer.to_ble == NULL) return -1;
	}

	/* 协议检测 */
	if (udt_protocol_check(data, size) != 0) return -1;

	/* 判断功能码是否是由设备端到终端 */
	udt_protocol_t *udt_prot = (udt_protocol_t *)data;
	if ((udt_prot->funcode != UDT_FUNCODE_QUERY_FILE_RTN) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT_RTN)) 
		return -1;

	/* 如果设备地址是本机地址该数据无效 */
	if (udt_prot->address == udt_local_addr) 
		return -1;

	/* 向终端中继数据 */
	if (udt_mode == UDT_MODE_SVC) 
	{
		return udt_transfer.to_server(udt_prot->address, data, size);
	}else if (udt_mode == UDT_MODE_BLE) 
	{
        return udt_transfer.to_ble(udt_prot->address, data, size);
	} 
	else
	{
		return udt_transfer.to_pc(udt_prot->address, data, size);
	}
}

/**
 * @brief 向下层设备中继数据
 * - 在本机接收到终端的查询指令时,调用该函数向下层设备中继终端的查询指令
 *
 * @param data 数据指针
 * @param size 数据长度
 * @return 成功返回0, 否则返回-1
 */
int udt_relay_to_device(uint8_t *data, int size)
{
	/* 参数检测 */
	if ((data == NULL) || (size == 0) || (udt_relay.nums == 0)) return -1;

	/* 协议检测 */
	if (udt_protocol_check(data, size) != 0) return -1;

	// if (pc_flg == 0) {
	// 	udt_mode = UDT_MODE_SVC;		// 设置为服务端模式
	// }

	/* 判断功能码是否是由服务端到设备端 */
	udt_protocol_t *udt_prot = (udt_protocol_t *)data;
	ESP_LOGI(TAG,"udt_relay_to_device udt_prot->funcode:%d  address:%d,udt_local_addr:%d",udt_prot->funcode,udt_prot->address,udt_local_addr);
	if ((udt_prot->funcode != UDT_FUNCODE_QUERY_FILE) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT_SVC_ACK)) 
		return -1;

	/* 如果设备地址是本机地址该数据无效 */
	if (udt_prot->address == udt_local_addr) 
	return -1;

	/* 在列表中查找与目标设备地址相关的发送函数 */
	data_transfer_t transfer = NULL;
	for (int i = 0; i < udt_relay.nums; i++)
	{
		udt_relay_list_t *list = &udt_relay.list[i];
		if ((udt_prot->address >= list->addr1) && (udt_prot->address <= list->addr2))
		{
			transfer = list->transfer;
			break;
		}
	}

	/* 未找到发送函数该数据无效 */
	if (!transfer) return -1;

	/* 向下层中继数据 */
	return transfer(udt_prot->address, data, size);
}

/**
 * @brief 通用数据发送
 *
 * @param tx_data 发送数据结构指针
 * @param transfer 发送函数
 * @return 成功返回0, 否则返回-1
 */
int udt_transmit(udt_tx_data_t *tx_data)
{
	/* 参数检测 */
	if ((tx_data->data_size > UDT_ONCE_TX_MAX_SIZE) || (tx_data->pkg_total == 0) || (tx_data->data == NULL)) 
    {
        ESP_LOGE(TAG, "[udt_transmit] tx_data = NULL, data_size = %lu, pkg_total = %d",tx_data->data_size, tx_data->pkg_total);
        return -1;
    }
	if ((tx_data->funcode != UDT_FUNCODE_QUERY_FILE_RTN) && (tx_data->funcode != UDT_FUNCODE_QUERY_CONTENT_RTN)) 
    {
        ESP_LOGE(TAG, "[udt_transmit] funcode ERROR (%d)",tx_data->funcode);
        return -1;
    }

	/* 服务端传输检测 */
	if (udt_mode == UDT_MODE_SVC) {
		if (udt_transfer.to_server == NULL) return -1;
	}

	/* PC端传输检测 */
	if (udt_mode == UDT_MODE_PC) {
		if (udt_transfer.to_pc == NULL) return -1;
	}

	/* 蓝牙端传输检测 */
	if (udt_mode == UDT_MODE_BLE) {
		if (udt_transfer.to_ble == NULL) return -1;
	}

	/* 安装协议 */
	int len = sizeof(udt_protocol_t);
	udt_protocol_t *udt_prot = (udt_protocol_t *)udt_buffer;//串行载荷协议报文
	udt_prot->mark = UDT_PROT_MARK;
	udt_prot->version = UDT_PROT_VER;
	udt_prot->scene = UDT_PROT_SCENE;
	udt_prot->req_id = UDT_SWAP16(last_req_id);
	udt_prot->pkg_idx = UDT_SWAP16(tx_data->pkg_idx);
	udt_prot->pkg_total = UDT_SWAP16(tx_data->pkg_total);
	udt_prot->rsvd = 0;
	udt_prot->address = udt_local_addr;
	udt_prot->funcode = tx_data->funcode;
	udt_prot->start_pos = UDT_SWAP32(tx_data->start_pos);
	udt_prot->data_size = UDT_SWAP32(tx_data->data_size);
	memcpy(&udt_buffer[len], tx_data->data, tx_data->data_size);	// 拷贝数据到本地缓存
	len += tx_data->data_size;
	uint16_t crc_md = modbus_crc16_tbl(udt_buffer, len);			// 计算modbus crc
	udt_buffer[len++] = crc_md;
	udt_buffer[len++] = crc_md >> 8;

    ESP_LOGW(TAG, "[udt_transmit] funcode : %d ,udt_prot->data_size:%lu,udt_mode:%d",udt_prot->funcode,udt_prot->data_size,udt_mode);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, udt_buffer, len, ESP_LOG_WARN);

	/* 向终端发送本机数据 */
	if (udt_mode == UDT_MODE_SVC) 
	{
		return udt_transfer.to_server(udt_prot->address, udt_buffer, len);
	}else if (udt_mode == UDT_MODE_BLE) 
	{
        return udt_transfer.to_ble(udt_prot->address, udt_buffer, len);
	}
	else
	{
		return udt_transfer.to_pc(udt_prot->address, udt_buffer, len);
	}
}

/**

数据通道协议， 接收数据解析，本地处理
 *
 * @param data 数据指针
 * @param size 数据长度
 * @return 成功返回0, 否则返回-1
 */

int udt_receive(uint8_t *data, int size)
{
	/* 参数检测 */
	if ((data == NULL) || (size == 0) || (!rx_evt_cb)) return -1;

	/* 协议检测 */
	if (udt_protocol_check(data, size) != 0) 
    {
        ESP_LOGE(TAG, "udt_protocol_check ERROR");
        return -1;
    }

	// if (pc_flg == 0) {
	// 	udt_mode = UDT_MODE_SVC;		// 设置为服务端模式
	// }

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, size, ESP_LOG_WARN);

	/* 判断功能码是否是由服务端到设备端 */
	udt_protocol_t *udt_prot = (udt_protocol_t *)data;
	ESP_LOGI(TAG,"udt_receive udt_prot->funcode:%d,udt_prot->address:%d,udt_local_addr:%d",udt_prot->funcode,udt_prot->address,udt_local_addr);
	if ((udt_prot->funcode != UDT_FUNCODE_QUERY_FILE) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT_SVC_ACK)) 
    {   
        ESP_LOGE(TAG, "udt_prot->funcode ERROR (%d)",udt_prot->funcode);
		return -1;
    }
        
	/* 如果设备地址不是本机地址该数据无效 */
	if (udt_prot->address != udt_local_addr) 
    {
        ESP_LOGE(TAG, "udt_prot->address ERROR (%d)",udt_prot->address);
		return -1;
    }

    memset(&udt_event, 0, sizeof(udt_event));//清除上次缓存
    
	/* 保存事件及其对应参数 */
	udt_event.funcode = udt_prot->funcode;
	last_req_id = UDT_SWAP16(udt_prot->req_id);
	udt_prot->start_pos = UDT_SWAP32(udt_prot->start_pos);
	udt_prot->data_size = UDT_SWAP32(udt_prot->data_size);

	switch (udt_event.funcode)
	{
		case UDT_FUNCODE_QUERY_FILE:					// 终端查询文件
			break;

		case UDT_FUNCODE_QUERY_CONTENT:					// 终端查询文件内容
			udt_event.param.qry_cont.start_pos = udt_prot->start_pos;
			udt_event.param.qry_cont.rsvd = udt_prot->rsvd;
			memcpy(udt_event.param.qry_cont.fname, &data[sizeof(udt_protocol_t)], udt_prot->data_size);
			break;

		case UDT_FUNCODE_QUERY_CONTENT_SVC_ACK:			// 终端响应本次查询事件
			udt_event.param.svc_ack.pkg_total = UDT_SWAP16(*(uint16_t*)&data[sizeof(udt_protocol_t)]);
			udt_event.param.svc_ack.last_pkg_seq = UDT_SWAP16(*(uint16_t*)&data[sizeof(udt_protocol_t)+2]);
			break;

		default:
			return -1;
	}

	/* 调用接收事件回调函数 */
	return rx_evt_cb(&udt_event);
}

/**
 * @brief PC端数据交互接口
 * - 在本机接收到PC端的查询指令时,调用该函数向下层设备中继PC端的查询指令
 *
 * @param data 数据指针
 * @param size 数据长度
 * @return 成功返回0, 否则返回-1
 */
int udt_pc_interface(uint8_t *data, int size)
{
	/* 参数检测 */
	if ((data == NULL) || (size == 0)) return -1;

	/* 协议检测 */
	if (udt_protocol_check(data, size) != 0) return -1;

	int ret = 0;
	pc_flg = 1;						// PC端标志
	udt_mode = UDT_MODE_PC;			// 设置为PC端模式

	/* 判断功能码是否是由终端到设备端 */
	udt_protocol_t *udt_prot = (udt_protocol_t *)data;
	if ((udt_prot->funcode != UDT_FUNCODE_QUERY_FILE) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT_SVC_ACK))
	{
		ret = -1;
		goto __exit;
	}

	ESP_LOGI(TAG, "udt_pc_interface udt_prot->address == %d, udt_local_addr == %d\n", udt_prot->address, udt_local_addr);
	if (udt_prot->address == udt_local_addr) 		// 如果设备地址是本机地址
	{
		ret = udt_receive(data, size);				// 本机接收
		goto __exit;
	}

	/* 中继到下级设备 */
	ret = udt_relay_to_device(data, size);

__exit:
	pc_flg = 0;
	return ret;
}


/**
 * @brief 服务器端数据交互接口
 * - 在本机接收到服务器端的查询指令时,调用该函数向下层设备中继服务器端的查询指令
 *
 * @param data 数据指针
 * @param size 数据长度
 * @return 成功返回0, 否则返回-1
 */
int udt_svc_interface(uint8_t *data, int size)
{
	/* 参数检测 */
	if ((data == NULL) || (size == 0)) return -1;

	/* 协议检测 */
	if (udt_protocol_check(data, size) != 0) {
        ESP_LOGE(TAG, "udt_protocol_check ERROR");
        return -1;
    }

	int ret = 0;
    udt_mode = UDT_MODE_SVC;        // 设置为服务端模式

	/* 判断功能码是否是由终端到设备端 */
	udt_protocol_t *udt_prot = (udt_protocol_t *)data;
	if ((udt_prot->funcode != UDT_FUNCODE_QUERY_FILE) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT_SVC_ACK))
	{
		ret = -1;
        ESP_LOGE(TAG, "udt_prot->funcode ERROR (%d)",udt_prot->funcode);
		goto __exit;
	}

	if (udt_prot->address == udt_local_addr) 		// 如果设备地址是本机地址
	{
		ret = udt_receive(data, size);				// 本机接收
		goto __exit;
	}

	/* 中继到下级设备 */
	ret = udt_relay_to_device(data, size);

__exit:
    
	return ret;
}

/**
 * @brief 蓝牙端数据交互接口
 * - 在本机接收到蓝牙端的查询指令时,调用该函数向下层设备中继蓝牙端的查询指令
 *
 * @param data 数据指针
 * @param size 数据长度
 * @return 成功返回0, 否则返回-1
 */
int udt_ble_interface(uint8_t *data, int size)
{
	/* 参数检测 */
	if ((data == NULL) || (size == 0)) return -1;

	/* 协议检测 */
	if (udt_protocol_check(data, size) != 0) {
        ESP_LOGE(TAG, "udt_protocol_check ERROR");
        return -1;
    }

	int ret = 0;
    udt_mode = UDT_MODE_BLE;        // 设置为蓝牙端模式

	/* 判断功能码是否是由终端到设备端 */
	udt_protocol_t *udt_prot = (udt_protocol_t *)data;
	if ((udt_prot->funcode != UDT_FUNCODE_QUERY_FILE) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT) &&
		(udt_prot->funcode != UDT_FUNCODE_QUERY_CONTENT_SVC_ACK))
	{
		ret = -1;
        ESP_LOGE(TAG, "udt_prot->funcode ERROR (%d)",udt_prot->funcode);
		goto __exit;
	}
	ESP_LOGI(TAG,"udt_ble_interface udt_prot->address:%d,udt_local_addr:%d",udt_prot->address,udt_local_addr);
	if (udt_prot->address == udt_local_addr) 		// 如果设备地址是本机地址
	{
		ret = udt_receive(data, size);				// 本机接收
		goto __exit;
	}

	/* 中继到下级设备 */
	ret = udt_relay_to_device(data, size);

__exit:
    
	return ret;
}

/**
 * @brief 获取当前模式
 *
 * @return 返回当前模式
 */
int udt_mode_get(void)
{
	return udt_mode;
}

/**
 * @brief IOT模拟服务端读取数据块，并机时从机(服务端)->主机
 *
 * @return 无
 */
void udt_IotRead(void)//universal_data_transmission
{
	/* 安装协议 */
	int len = sizeof(udt_protocol_t);
	udt_protocol_t *udt_prot = (udt_protocol_t *)udt_buffer;//串行载荷协议报文
	udt_prot->mark = UDT_PROT_MARK;//09
	udt_prot->version = UDT_PROT_VER;//0xF8
	udt_prot->scene = UDT_PROT_SCENE;//场景码
	udt_prot->req_id = UDT_SWAP16(0x01);
	udt_prot->pkg_idx = UDT_SWAP16(0x01);
	udt_prot->pkg_total = UDT_SWAP16(0x01);
	udt_prot->rsvd = 0;
	udt_prot->address = 0;//默认选取第一个从机读取数据块，第二个从机监听
	udt_prot->funcode = UDT_FUNCODE_QUERY_FILE;
	udt_prot->start_pos = 0;
	udt_prot->data_size = 0;
	uint16_t crc_md = modbus_crc16_tbl(udt_buffer, len);			// 计算modbus crc
	udt_buffer[len++] = crc_md;
	udt_buffer[len++] = crc_md >> 8;

    ESP_LOGW(TAG, "slave device read master data, funcode : %d",udt_prot->funcode);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, udt_buffer, len, ESP_LOG_WARN);

	/* 向终端发送本机数据-CAN发送 */
	udt_transfer.to_iot(udt_prot->address, udt_buffer, len);
}

/*------------------------------------------------------------------------------
 Function: udt_process_handle
 -----------------------------------------------------------------------------*/
/**
  * @brief      数据通道命令接收
  * @param[in]  uint8_t *data  
                int size       
                uint8_t chl    
  * @param[out] None
  * @return     int
  */
int udt_process_handle(uint8_t *data, int size, uint8_t chl)
{
	/* 参数检测 */
	if ((data == NULL) || (size == 0)) return -1;

    if (xUdtSemaphore == NULL) { // MODBUS互斥锁,防止多任务同时访问
        xUdtSemaphore = xSemaphoreCreateMutex();
        if (xUdtSemaphore == NULL) {
            ESP_LOGE(TAG, "udt_process_handle Semaphore Create failed");
            return 0;
        }
    }

    /* 获取信号量 */
    if (xSemaphoreTake( xUdtSemaphore, pdMS_TO_TICKS(300) ) != pdPASS) {
        ESP_LOGE(TAG, "udt_process_handle xSemaphoreTake failed");
        return 0;
    }

    int ret = 0;
    ESP_LOGW(TAG, "udt_process_handle : len(%d), chl(%d)", size, chl);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, size, ESP_LOG_WARN);
    
    /*判断访问端口，执行对应操作*/
    switch ( chl )
    {
        case UDT_MODE_SVC :
            /*服务器端*/
            ret = udt_svc_interface(data, size);
            break;
        
        case UDT_MODE_PC :
            /*PC端*/
            ret = udt_pc_interface(data, size);
            break;
        
        case UDT_MODE_BLE :
            /*蓝牙端*/
            ret = udt_ble_interface(data, size);
            break;
        
        default:
            /*访问端口无效，默认使用服务器端*/
            ret = udt_svc_interface(data, size);
    }
    
    xSemaphoreGive(xUdtSemaphore);  /* 释放信号量 */

    return ret;
}