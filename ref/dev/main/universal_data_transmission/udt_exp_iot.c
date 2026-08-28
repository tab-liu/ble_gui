/**
 * 历史数据传输模块示例: 本示例为IOT节点功能
 *
 * 示例通信包括三个节点IOT、ARM、PACK,其通信架构如下所示:
 *		┌───────┐
 *	IOT	│  485  │
 *		└───┬───┘
 *			│
 *		┌───┴───┐
 *		│  485  │
 *	ARM	│       │
 *		│  CAN  │
 *		└───┬───┘
 *			│
 *		┌───┴───┐
 *	PACK│  CAN  │
 *		└───────┘
 *
 * IOT通过485与ARM通信,ARM通过CAN与PACK通信,IOT节点地址0,ARM节点地址1,PACK节点地址41
 * 对IOT来说,其上级设备是云服务器或PC端,下级设备是ARM
 * 对ARM来说,其上级设备是IOT或PC端,下级设备是PACK
 * 对PACK来说,其上级设备是ARM或PC端,无下级设备
 *
 * UDT(通用数据传输)模块包含三个功能:
 * 1、通用数据中继功能函数
 * - udt_relay_to_terminal: 将下级设备上报数据转发到终端(上级设备)
 * - udt_relay_to_device: 将终端(上级设备)下发的数据转发到下级设备
 * 2、通用数据收发功能函数
 * - udt_transmit: 上报本机数据到终端(上级设备)
 * - udt_receive: 接收终端(上级设备)下发给本机的数据
 * 3、PC端接口功能函数
 * - udt_pc_interface: 处理与PC端的通信
 *
 *
 * IOT节点功能:
 * - 实现历史记录通用数据中继:
 * - 接收到终端获取ARM/PACK的历史数据时,转发终端下发的指令
 * - 接收到ARM/PACK上报的历史数据时,转发ARM/PACK上报的数据
 *
 * ARM节点功能:
 * 1、实现历史记录通用数据收发,即接收终端下发的指令,根据指令将本地历史数据发送到终端
 * 2、实现历史记录通用数据中继:
 * - 接收到终端获取PACK的历史数据时,转发终端下发的指令
 * - 接收到PACK上报的历史数据时,转发PACK上报的数据
 *
 * PACK节点功能:
 * - 实现历史记录通用数据收发,即接收终端下发的指令,根据指令将本地历史数据发送到终端
 * - PACK节点无中继功能
 */

#include "udt_transfer.h"
#include "udt_port.h"
#include <string.h>
#include "dev_data_record.h"
#include "esp_system.h"
#include "esp_log.h"
#include "iot_period_task.h"

#define IOT_NODE_ADDR		0	// IOT节点地址
#define ARM_NODE_ADDR		1 	// ARM节点地址
#define PACK_NODE_ADDR		41 	// PACK节点地址
//static const char fname[] = "\0alarm\0\0\0\0,";	// 目录结构/alarm

#define TAG "[UDT_EXP_IOT]"

void uart_send_data(uint8_t *data, int size)
{
	// 串口发送
}

void wifi_send_data(uint8_t *data, int size)
{
	// 网络发送
}

/**
 * @brief 处理服务端下发的数据
 *
 * @param data 数据指针
 * @param size 数大小
 * @return 无
 */
void cloud_data_handler(uint8_t *data, int size)
{
	/* 中继服务端下发给下级设备的数据 */
	if (udt_relay_to_device(data, size) == 0) return;
}
/*
基于指定的文件名目录、内容偏移地址、返回数据内容和剩余长度
input:fname
input:offset

output:data
output:size

*/
uint8_t* get_data_from_storage(char *fname, uint32_t offset, uint8_t *data, uint32_t *size)
{
	// 从存储器中获取数据

	/* 获取从指文件定位置到文件结束的数据大小 */
	if(historic_data_get_size(fname, offset, size) < 0)  
	{
		return NULL;
	}
    
	data = heap_caps_malloc(*size, MALLOC_CAP_SPIRAM);
	if(data == NULL) 
	{
		return NULL;
	}

	/* 读取文件数据指定长度size到data */
    if ( fname[0] == 'L' )//历史记录偏移读取，单独处理
    {
        /* 读取文件数据指定长度size到data */
        if(historic_data_bias_read(fname, data, size, offset) < 0) 
        {
            free(data);
            return NULL;
        }   
    }
    else
    {
		if(historic_data_read(fname, data, size, offset) < 0) 
		{
			free(data);
			return NULL;
		}
	}
		

    return data;
}

/**
 * @brief 处理ARM/PACK设备上报的数据
 *
 * @param data 数据指针
 * @param size 数大小
 * @return 无
 */
void arm_pack_data_handler(uint8_t *data, int size)
{
	/* 中继下级设备上报给终端的数据 */
	udt_relay_to_terminal(data, size);
}

/**
 * @brief 查询文件响应
 *
 * @param evt 终端事件
 * @return 无
 */
static void udt_resp_query_file(udt_event_t *evt)
{
	udt_tx_data_t tx_data;
	uint16_t dir_file_len=0;
	uint8_t *dir_file_data = NULL;

//	char fname[30] = {0};
//	COMMON_FILE_PATH_YYMM(fname,KWH_RECORD_FOLDER_LEVEL1, reals.rtc_time.year, reals.rtc_time.mon);
	filename_list_get((char *)evt->param.qry_cont.fname);
	Filename_Compare_Sequence();


	dir_file_data = heap_caps_malloc(reals.file_nums * (FILE_DIR_LEN + 1), MALLOC_CAP_SPIRAM);

	/* 序列化文件名及目录名 */
	dir_file_len = (uint16_t)historic_record_serialize_query( dir_file_data);
    
	int offset = 0;
	int total = dir_file_len / UDT_ONCE_TX_MAX_SIZE;		// 计算传输的总包数
	if (dir_file_len % UDT_ONCE_TX_MAX_SIZE) 
	{
		total += 1;
	}

    vTaskDelay(pdMS_TO_TICKS(300)); // 增加延时，服务器准备接收（至少300ms相对稳定）

	/* 分包传输 */
	for (int i = 0; i < total; i++)
	{
		udt_tx_data_t tx_data;
		tx_data.funcode = UDT_FUNCODE_QUERY_FILE_RTN;	// 功能码
		tx_data.pkg_idx = i + 1;							// 包索引
		tx_data.pkg_total = total;							// 总包数
		tx_data.start_pos = offset;							// 数据偏移
		tx_data.data_size = (dir_file_len > UDT_ONCE_TX_MAX_SIZE) ? (UDT_ONCE_TX_MAX_SIZE) : (dir_file_len);	// 数据长度
		tx_data.data = &dir_file_data[offset];						// 数据
		udt_transmit(&tx_data);								// 发送数据到终端端

		offset += tx_data.data_size;						// 计算下次数据偏移
		dir_file_len -= tx_data.data_size;							// 计算剩余发送数据大小
	}
    
	free(dir_file_data);
    free(fsys_file_list);
    fsys_file_list = NULL;
	
}

/**
 * @brief 查询文件内容响应
 *
 * @param evt 终端事件
 * @return 无
 */
static void udt_resp_query_content(udt_event_t *evt)
{
	/* 判断读取的文件名是否有效 */

	uint8_t *pdata = NULL;
	uint32_t len = 0;
    ESP_LOGI(TAG,"fname:%s ,start_pos:%lu",(char *)evt->param.qry_cont.fname,evt->param.qry_cont.start_pos);
	pdata = get_data_from_storage((char *)evt->param.qry_cont.fname, evt->param.qry_cont.start_pos, pdata, &len);	// 从存储器中获取历史数据
	if(pdata == NULL) 
	{
	    ESP_LOGE(TAG, "[udt_resp_query_content] error");
		return;
	}

	int offset = 0;
	int total = len / UDT_ONCE_TX_MAX_SIZE;		// 计算传输的总包数
	if (len % UDT_ONCE_TX_MAX_SIZE) 
	{
		total += 1;
	}

    vTaskDelay(pdMS_TO_TICKS(300)); // 增加延时，服务器准备接收（至少300ms相对稳定）

	/* 分包传输 */
	for (int i = 0; i < total; i++)
	{
		udt_tx_data_t tx_data;
		tx_data.funcode = UDT_FUNCODE_QUERY_CONTENT_RTN;	// 功能码
		tx_data.pkg_idx = i + 1;							// 包索引
		tx_data.pkg_total = total;							// 总包数
		tx_data.start_pos = offset;							// 数据偏移
		tx_data.data_size = (len > UDT_ONCE_TX_MAX_SIZE) ? (UDT_ONCE_TX_MAX_SIZE) : (len);	// 数据长度
		tx_data.data = &pdata[offset];						// 数据
		udt_transmit(&tx_data);								// 发送数据到终端端

		offset += tx_data.data_size;						// 计算下次数据偏移
		len -= tx_data.data_size;							// 计算剩余发送数据大小
		
//		vTaskDelay(pdMS_TO_TICKS(500));	//tbd,发生帧间隔延时
//		// 发送完成后视情况延时(至少需要等待上级设备将数据中继完成)
	}
	free(pdata);
}


/**
windy 新增：
基于顶层目录(记录分类)查询目录下面所有记录的各个文件，顺序上报;
主动任性 自定义上报！
 * @brief 查询文件内容响应
 *
 * @param evt 终端事件
 * @return 无
 */
static void udt_resp_query_content2(udt_event_t *evt)
{
	/* 判断读取的文件名是否有效 */

	uint8_t *pdata = NULL;
	uint32_t len = 0;
	uint16_t i = 0;
	
	uint32_t file_nums = 0;

	
	for (file_nums = 0; i < reals.file_nums; i++)
	{
		len = 0;
		get_data_from_storage((char *)fsys_file_list[file_nums].name, 0, pdata, &len); // 从存储器中获取历史数据
		
		int offset = 0;
		int total = len / UDT_ONCE_TX_MAX_SIZE; 	// 计算传输的总包数
		if (len % UDT_ONCE_TX_MAX_SIZE) 
		{
			total += 1;
		}
		
		/* 分包传输 */
		for (int i = 0; i < total; i++)
		{
			udt_tx_data_t tx_data;
			tx_data.funcode = UDT_FUNCODE_QUERY_CONTENT_RTN;	// 功能码
			tx_data.pkg_idx = i + 1;							// 包索引
			tx_data.pkg_total = total;							// 总包数
			tx_data.start_pos = offset; 						// 数据偏移
			tx_data.data_size = (len > UDT_ONCE_TX_MAX_SIZE) ? (UDT_ONCE_TX_MAX_SIZE) : (len);	// 数据长度
			tx_data.data = &pdata[offset];						// 数据
			udt_transmit(&tx_data); 							// 发送数据到终端端
		
			offset += tx_data.data_size;						// 计算下次数据偏移
			len -= tx_data.data_size;							// 计算剩余发送数据大小
			// 发送完成后视情况延时(至少需要等待上级设备将数据中继完成)
		}
        
		free(pdata);
        free(fsys_file_list);
        fsys_file_list = NULL;
	}
	
}

/**
 * @brief 事件处理
 *
 * @param evt 终端事件
 * @return 无
 */
static void udt_evt_process(udt_event_t *evt)
{
	switch (evt->funcode)
	{
	case UDT_FUNCODE_QUERY_FILE:					// 终端查询文件
	
	    ESP_LOGI(TAG, "UDT_FUNCODE_QUERY_FILE : %d",evt->funcode);
		udt_resp_query_file(evt);
		break;

	case UDT_FUNCODE_QUERY_CONTENT:					// 终端查询文件内容
	
	    ESP_LOGI(TAG, "UDT_FUNCODE_QUERY_CONTENT : %d",evt->funcode);
		udt_resp_query_content(evt);
		break;

	case UDT_FUNCODE_QUERY_CONTENT_SVC_ACK:			// 终端响应本次查询事件
	
		// 本次历史数据传输完成处理
		ESP_LOGI(TAG, "UDT_FUNCODE_QUERY_CONTENT_SVC_ACK (%d) : %d / %d",evt->funcode, evt->param.svc_ack.last_pkg_seq, evt->param.svc_ack.pkg_total);
		break;

	default:
		return;
	}
    
    reals.file_nums = 0;
}

/**
 * @brief 事件回调函数
 *
 * @param evt 终端事件
 * @return 无
 */
static int udt_evt_callback(udt_event_t *evt)
{
	udt_evt_process(evt);
	return 0;
}

/**
 * @brief 通用数据协议配置
 *
 * @return 无
 */
//void udt_configuration(void)
//{
//	/* 本示例中IOT节点无收发功能只有中继功能
//	 * 配置中继列表, 当接收到服务端的指令时,如果指令中包含的地址在该
//	 * 配置的地址范围内,则调用相应的发送函数中继服务端的数据 */
//	udt_relay_t relay;
//	relay.nums = 2;
//	relay.list[0].addr1 = ARM_NODE_ADDR;
//	relay.list[0].addr2 = ARM_NODE_ADDR;
//	relay.list[0].transfer = udt_transfer_to_arm_pack;
//	relay.list[1].addr1 = PACK_NODE_ADDR;
//	relay.list[1].addr2 = PACK_NODE_ADDR;
//	relay.list[1].transfer = udt_transfer_to_arm_pack;
//
//	/* 设置传输函数 */
//	udt_transfer_t transfer;
//	transfer.to_server = udt_transfer_to_cloud;		// 传输到服务端
//	transfer.to_pc = NULL;							// 传输到PC端
//	udt_init(IOT_NODE_ADDR, &transfer, &relay, NULL);
//}

/**
 * @brief 通用数据协议配置
 *
 * @return 无
 */
void udt_configuration(void)//universal_data_transmission
{
	/* COMBOX 本地存储log，面向BLE/WIFI上报数据通道信息 */
	udt_relay_t relay;
	relay.nums = 1;
	relay.list[0].addr1 = 0x01;
	relay.list[0].addr2 = 0xFF;
	relay.list[0].transfer = udt_transfer_to_inv_pack;

	
	/* 设置传输函数 */
	udt_transfer_t transfer;
	transfer.to_server = udt_transfer_to_cloud; 	// 传输到服务端->MQTT
	transfer.to_pc = udt_transfer_to_pc;			// 传输到PC端->CAN
	transfer.to_iot = udt_transfer_to_IotMaster;			// 传输到PC端->CAN
	transfer.to_ble = udt_transfer_to_ble;			// 传输到蓝牙端
	udt_init(IOT_NODE_ADDR, &transfer,  &relay,udt_evt_callback);
	udt_queue_init();
}


