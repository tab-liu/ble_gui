/**
  ******************************************************************************
  * @file      uart_ota.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/5/8
  * @brief     uart升级
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/5/8   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "uart_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "xmodem.h"
#include "xmodem_transmitter.h"
#include "xmodem_client.h"
// #include "iot_period_task.h"
// #include "uart_device_process.h"
#include "crc.h"
// #include "iot_wifi_init.h"
#include "http_client.h"
#include "filesystem.h"
#include "iot_mqtt.h"
// #include "wl_mesh.h"
// #include "udp_multicast.h"
#include "xmodem_transmitter.h"
// #include "mesh_api.h"
#include "can_protocol.h"




#define TAG	"[UART_OTA]"

#if 0
//static SemaphoreHandle_t uart_recv_sem;				// 串口接收信号量
static ota_set_t uart_ota_set;						// ota设置参数
ota_state_t uart_ota_state;					// ota升级状态
//static int ota_flag;								// lcd模块升级标志
static int backup_flag = 0;	

//static int uart_num = UART1;

static uint8_t *recv_buff;			// 串口接收缓存
static uint8_t recv_len;							// 串口接收长度
//static char bin_file_name[30];						// 固件文件名
//static uint8_t *bin_ota_data;						// 固件数据指针
static int bin_data_size;							// 固件数据大小
//static uint8_t bin_data_flash_flag = 0;					// 固件是否在flash存储标志，1-yes-存储在flash暂存,0-no-存储在大RAM暂存

 uint8_t *xm_tx_buff;			// 串口发送缓存
static uint16_t xm_tx_len = 0;							// 串口接收长度




#define DELAY_CNT_3S	300//at 10ms cycle
/**
 * @brief uart发起升级
 first_init_flag:首次初始化
 * - 该函数将会向uart发起升级,通过modbus的700寄存器组写入数据实现
 * - lcd正确响应该包指令后后续开始进入xmodem升级流程
 * - 发送指令后等待1s的响应时间,如果未响应或响应错误则重复发送指令,最多发送5次
 *
 * @return self发起OTA，
 下级RTN OK，成功返回：1,
 超时5次返回：-1，
 未超时且未返回：0
 */
static int uart_ota_initiate(uint8_t  first_init_flag)//10ms cycle
{
	#define MD_WRITE			0x10			// modbus写多寄存器功能码
	#define MD_OTA_REG_ADDR		((uint16_t)700)	// OTA寄存器起始地址

	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t buff[100];
	uint16_t reg_nums = sizeof(ota_set_t) / sizeof(short);
	static int resend_cnt = 5;
	static uint16_t sdelaycnt_send = DELAY_CNT_3S;
    static uint8_t client_cnt = 0;	
	int rtn = 0;

	if(1 == first_init_flag)
	{
		resend_cnt =0;
		sdelaycnt_send = DELAY_CNT_3S;
		client_cnt =0;

	}
    
    buff[i++] = UART_OTA_MD_ADDR;
	buff[i++] = MD_WRITE;
	buff[i++] = MD_OTA_REG_ADDR >> 8;
	buff[i++] = (uint8_t)MD_OTA_REG_ADDR;
	buff[i++] = reg_nums >> 8;
	buff[i++] = (uint8_t)reg_nums;
	buff[i++] = (uint8_t)(reg_nums * 2);

	memcpy(&buff[i], &uart_ota_set, sizeof(ota_set_t));
    
	uint16_t *pdata = (uint16_t*)&buff[i];
	for (int n = 0; n < reg_nums; n++) {							// 交换高低字节
		pdata[n] = (pdata[n] >> 8) | (pdata[n] << 8);
	}
	i += sizeof(ota_set_t);
    
	uint16_t crc_mesh = crc16_modbus_calc(buff, i);
    buff[i++] = crc_mesh;
    buff[i++] = crc_mesh >> 8;

	/* 发送ota开始到uart,同时等待其正确的响应,每次等待超时3s钟,如果5次发送失败则退出升级 */

	if(++sdelaycnt_send >= DELAY_CNT_3S)//3s重发周期
	{
		sdelaycnt_send =0;
		if(++resend_cnt >= 5)//5次 max
		{
			resend_cnt =0;
			rtn =-1;//fail

		}
		recv_len = 0;
		xm_tx_len=0;
		
		uint8_t ret = 0;//0-发送失败,1-发送成功
		ESP_LOGW(TAG, "uart_ota_initiate  debug  xyz");

		xmd_write_data(i, buff);
		ESP_LOGW(TAG, "uart_ota_initiate  debug  BBB");
		
		xmd_uart_send(&ret);
	//		int ret = uart_write_bytes(uart_num, (const char*)buff, i); 		// 发起OTA开始指令
		ESP_LOGW(TAG, "modbus send ota start, ret = %d, len = %d, data:", ret, i);
		ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, i, ESP_LOG_WARN);

	}

	if(recv_len > 0)
	{
//				uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

		/* 检查CRC */
		uint16_t crc_old = ((uint16_t)recv_buff[recv_len-1] << 8) | recv_buff[recv_len-2];
		uint16_t crc_new = crc16_modbus_calc(recv_buff, (recv_len-2));
		if(crc_old != crc_new)
		{
			ESP_LOGE(TAG, "respond crc error, addr: %d, len :%d", recv_buff[0], recv_len);

		}
		else//ok
		{
			//正确收到客户端的应答, 计数器+1
			++client_cnt;
		
			// 搜到所有设备的应答, 直接退出
			if (client_cnt >= uart_ota_state.online_client_cnt)
			{
				ESP_LOGE(TAG, "all client response");
				rtn =1;//ok
			}
		
		}

		recv_len =0;//收到解析后清零
	}

return rtn;
}

/**
 * @brief xmodem发送数据接口
 *
 * @param requested_size 请求的数据大小
 * @param buffer 数据缓存
 * @param first_byte_flag
 * - 用于指示本次发送是否为第一字节
 * @return 无
 */
static void uart_xmd_write(const uint32_t requested_size, uint8_t * buffer)
{
    #if 0       //lxy 仅写入txbuffer，不作发送，便于组帧
	int ret = uart_write_bytes(uart_num, (const char*)buffer, requested_size);
	if (ret < 0) *write_status = 0;
	else *write_status = 1;
    #endif
    
    memcpy(&xm_tx_buff[xm_tx_len], buffer, requested_size);
    xm_tx_len += requested_size;
}

/*------------------------------------------------------------------------------
 Function: uart_xmd_send
 -----------------------------------------------------------------------------*/
/**
  * @brief      计算crc，发送
  * @param[in]  uint8_t * write_status  
  * @param[out] None
  * @return     static void- 用于指示本次发送是否成功,0-发送失败,1-发送成功
  */
static void uart_xmd_send(uint8_t * write_status)
{
//    //lxy 计算crc，发送
//    UART_STRUCT *uart_structx = &reals.struct_uart1;
//    uint8_t j = 0;
//    
//    memset(uart_structx->Txbuffer, 0, UART_TX_BUF_SIZE);
//    uart_structx->TxBytesNum = 0;
//
//    memcpy(&uart_structx->Txbuffer[uart_structx->TxBytesNum], xm_tx_buff, xm_tx_len);
//    uart_structx->TxBytesNum += xm_tx_len;
	uint16_t temp_tx_len =0;

//    ESP_LOG_BUFFER_HEX_LEVEL(TAG,uart_structx->Txbuffer, uart_structx->TxBytesNum,ESP_LOG_WARN);
    
	int ret=0; 
	if(OTA_CH_SELF_TO_UART == gXmodem_server_Status.used_chl)
	{
//		ret = uart_write_bytes(uart_num, xm_tx_buff, xm_tx_len);
		// app_write_uart1_data(xm_tx_buff, xm_tx_len);		
		// memset(xm_tx_buff, 0, UART_TX_BUF_SIZE);
		//  xm_tx_len =0;
	}
	else if((OTA_CH_SELF_TO_WIFI_MESH == gXmodem_server_Status.used_chl))//&&((1029 == xm_tx_len)||(21 == xm_tx_len))
	{
		uint8_t tempsn[10];
		memset(tempsn, 0, 10);

		if(1029 == xm_tx_len || 1 == xm_tx_len)//xmodem tx
		{
			temp_tx_len = Udp_Singlecast_Modbus_MasterTxCmd(NULL, NULL, wireless_interface.data_tx_wifi_mesh, tempsn, MESH_FRAME_TYPE_XMODEM, xm_tx_len);
		}
		else//modbus 0x10
		{
			temp_tx_len = Udp_Singlecast_Modbus_MasterTxCmd(700, 6, wireless_interface.data_tx_wifi_mesh, tempsn, MESH_FRAME_TYPE_WRITE, xm_tx_len);
		}
		ret = send_wifi_mesh_data(0,0,  wireless_interface.data_tx_wifi_mesh, temp_tx_len);//SN_TYPE_S1 WIFI_UDP_FRAME_ADDR_MODBUS_HEAD
		memset(wireless_interface.data_tx_wifi_mesh, 0, MAX_TX_LEN_WIFI_MESH);
	}
	else
	{
		ret =-1;

	}
	
    ESP_LOGW(TAG, "uart_xmd_send,ret=%d, len:%u,gXmodem_server_Status.used_chl=%d",ret, xm_tx_len ,(uint8_t)gXmodem_server_Status.used_chl);
	
	if (ret < 0) 
		*write_status = 0;//fail
	else 
		*write_status = 1;//success

    xm_tx_len = 0;


	
}

/**
 * @brief xmodem读取数据接口
 *
 * @param requested_size 请求的数据大小
 * @param buffer 数据缓存
 * @param returned_size 实际返回的数据大小
 * @return 无
 */
static void uart_xmd_read(const uint32_t requested_size, uint8_t * buffer, uint32_t * returned_size)
{
    if (recv_len < requested_size)
	{
        ESP_LOGE(TAG, "recv_len < requested_size");
		*returned_size = 0;
		return;
	}

	*returned_size = recv_len;
	memcpy(buffer, recv_buff, requested_size);
	recv_len = 0;
    
    ESP_LOGW(TAG, "");
    ESP_LOGW(TAG, "uart_xmd_read, requested_size:%lu, returned_size:%lu, data", requested_size, *returned_size);
    ESP_LOG_BUFFER_HEX(TAG, recv_buff, *returned_size);
	
	if (recv_buff != NULL)
	 {
		 free(recv_buff);
		 recv_buff = NULL;
	 }
	
}

/**
 * @brief xmodem串口发送缓存满标志获取接口
 *
 * @return 返回0(假)
 */
static int uart_xmd_outbound_full(void)
{
	return 0;
}

/**
 * @brief xmodem串行接收空标志获取接口
 *
 * @return 当接收到串口数据响应时返回0(假),否则返回1(真)
 */
static int uart_xmd_inbound_empty(void)
{
	if (recv_len) return 0;
	return 1;
}

/**
 * @brief xmodem获取升级数据接口
 *
 * @param position 数据位置偏移
 * @param buffer 用于保存数据的缓存
 * @param buff_size 缓存大小
 * @return 无
 */
static void uart_xmd_get_buffer(const uint32_t position, uint8_t * buffer, uint16_t buff_size)
{
    if ((position + buff_size) > bin_data_size) return;
    
//    if (bin_data_flash_flag == 1)//本地flash存储
    {  
        iot_image_read((IMAGE_FLASH_AREA_ADDRESS + position), buffer, buff_size);
    }
//    else//内存缓存
//    {
//    	memcpy(buffer, &bin_ota_data[position], buff_size);
//    }

    uart_ota_state.progress = (uint8_t)((position + buff_size) * 100 / bin_data_size);
}


///**
// * @brief 从文件系统中获取升级数据
// *
// * @param data 保存数据的指针
// * @param size 文件大小
// * @return 成功返回0,否则返回-1
// */
//static int uart_get_ota_data(uint8_t *data, int size)
//{
//	/* 只读格式打开文件 */
//	FILE *fp = fopen(bin_file_name, "rb");
//	if(fp == NULL) return -1;
//
//	/* 读数据到缓存 */
//	if(fread(data, 1, size, fp) != size) {
//		goto __error;
//	}
//
//	fclose(fp);
//
//	return 0;
//
//__error:
//	fclose(fp);
//	return -1;
//}

/**
 * @brief 备份任务
 */
void iot_backup_thread(void * pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    /*重新备份当前运行区域代码至备份区*/
    backup_firmware();
    
    backup_flag = 0;
    
	vTaskDelete(NULL);	
}


/**
 * @brief 创建备份任务
 *
 * @return 0 if successful, otherwise -1
 */
static int backup_task_create(void)
{
    backup_flag = 1;
	if (xTaskCreate(iot_backup_thread, "iot_backup_thread",  1024 * 4, NULL, 1, NULL) != pdPASS)
	{
		ESP_LOGE(TAG, "iot_backup_thread create failed");
        backup_flag = 0;
		return -1;
	}

	return 0;
}
/**
自己作为xmodem server 给下级uart或 WIFI MESH OTA
改为非阻塞

 * @brief uart升级任务
 * - 1、通过modbus协议向lcd发起升级请求
 * - 2、设置xmodem服务接口
 * - 3、启动xmodem服务开始xmodem升级
 * - 4、升级退出时释放申请的内存资源同时删除本任务
 * - 升级结束后meta协议的升级状态仍然会保持,直到下次升级开始时清除
 *
 * @param pvParameters 任务参数
 * @return 无
 */
void uart_ota_thread(void)//(void * pvParameters) 10ms cycle
{
    static channel_type  sold_used_chl =0xFF;
    static uint8_t  first_init_flag =0;//首次进入，初始化
    uint8_t  exit_to_clean =0;//退出升级过程
	uint32_t ota_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

	
    
//    /*开辟xmodem传输缓存*/
//    xm_tx_buff = iot_calloc(UART_TX_BUF_SIZE);
//   recv_buff = iot_calloc(255);

	if(0 == gXmodem_server_Status.ota_flag)
	{
		return;
	}

	if(sold_used_chl != gXmodem_server_Status.used_chl)
	{
		sold_used_chl =gXmodem_server_Status.used_chl;

		if(OTA_CH_SELF_TO_UART == gXmodem_server_Status.used_chl)
		{
			// xm_tx_buff = reals.struct_uart1.Txbuffer;
			// recv_buff = reals.struct_uart1.Rxbuffer;
		}
		else if(OTA_CH_SELF_TO_WIFI_MESH == gXmodem_server_Status.used_chl)
		{
			xm_tx_buff = &wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD];
			//modify by yjh: WIFI_UDP_FRAME_ADDR_MODBUS_HEAD+3,modbus阶段不需要+3，否则crc校验不通过。
		//	recv_buff = &wireless_interface.data_rx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD];//非标xmodem,rtn+3 len，之后在uart_ota_recv()更新
			ESP_LOGE(TAG, "Xmodem_Server_top  outside...WWW");
		}

		if((OTA_CH_SELF_TO_UART == gXmodem_server_Status.used_chl)
			||(OTA_CH_SELF_TO_WIFI_MESH == gXmodem_server_Status.used_chl))
		{
		
			first_init_flag =1;
			//	iot_wifi_close(WIFI_MODE_ALL); // 升级时，关掉WiFi保证升级的稳定性和速度
			//	set_mqtt_stop();
			//	vTaskDelay(pdMS_TO_TICKS(3000));
			
				/* 设置xmodem服务端回调接口 */
				xmodem_transmitter_set_callback_write(uart_xmd_write);
				xmodem_transmitter_set_callback_uart_send(uart_xmd_send);
				xmodem_transmitter_set_callback_read(uart_xmd_read);

				xmodem_transmitter_set_callback_is_outbound_full(uart_xmd_outbound_full);
				xmodem_transmitter_set_callback_is_inbound_empty(uart_xmd_inbound_empty);
				xmodem_transmitter_set_callback_get_buffer(uart_xmd_get_buffer);
				
		}		

	}
	
	int xmd_state = xmodem_transmit_state_get();
		static int transmit_state_old = 0xFF;
    
    //ESP_LOGW(TAG, "server state: current_time:%ld",current_time);
			if(transmit_state_old != xmd_state)
				{
					transmit_state_old =xmd_state;
					ESP_LOGE(TAG, "xmd_state=:%ld,first_init_flag =%d,sold_used_chl =%d",(uint32_t)xmd_state,first_init_flag,sold_used_chl);
				}	

	if((XMODEM_TRANSMIT_INITIAL == xmd_state)//XMODEM_TRANSMIT_UNKNOWN
		&&(0 != first_init_flag))
	{
		int rtn = 0;
		rtn = uart_ota_initiate(first_init_flag);
		if(++first_init_flag >=5)
		{
			first_init_flag=5;

		}

		/* 通过modbus协议发起升级请求 */
		if ((-1) == rtn)//fail
		{
			ESP_LOGE(TAG, "uart_ota_initiate failed");
	        
	        uart_ota_state.is_ota = RF_OTA_STA_CPLT;                  // 升级结束状态设置为已完成
			uart_ota_state.err_code = ERR_FILE_INVALID;
	        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta = RF_OTA_STA_IDLE;
	        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode = ERR_FILE_INVALID;
	        set_ota_pct_info(NULL,NULL,NULL,NULL,2,0,ERR_FILE_INVALID);
			exit_to_clean =1;
		}
		else if (1 == rtn)//ok
		{
			ESP_LOGE(TAG, "Xmodem_Server_top  outside...BB");
			
			/* 获取升级数据,数据可能存在文件中,也可能存在内存中 */
			bin_data_size = uart_ota_set.file_size * 1024;
		
			/* 初始化xmodem服务 */
			xmodem_transmit_init(bin_data_size);
			first_init_flag =6;

		}
		else//nothing
		{

		}		

	}



//	ESP_LOGE(TAG, "Xmodem_Server_top  outside...CC");



//    while(1)
	if((0 == exit_to_clean)
		&&(6 == first_init_flag))
    {
//		vTaskDelay(pdMS_TO_TICKS(10));
//		first_init_flag =2;
		

		Xmodem_Server_top(gXmodem_server_Status.used_chl ,(const uint32_t)(time(NULL) * 1000));	// 启动xmodem服务内核

        int xmd_ota_pct  = xmodem_transmit_otapct_get();

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd_group.dev_id = uart_ota_set.group.dev_id;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd_group.dev_type = uart_ota_set.group.dev_type;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].level = 1;//
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta = RF_OTA_STA_DOING;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].where = 2;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].type = uart_ota_set.file_type;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].pct = xmd_ota_pct;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode = ERR_UPGRADE_NORMAL;

		if (xmd_state == XMODEM_TRANSMIT_ABORT_TRANSFER)				// xmodem终止升级
		{
			ESP_LOGE(TAG, "arm ota failed!");
			uart_ota_state.err_code = ERR_DEV_ABORT;
			uart_ota_state.is_ota = RF_OTA_STA_CPLT;					// 升级结束状态设置为已完成

            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta = RF_OTA_STA_IDLE;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode = ERR_DEV_ABORT;

            set_ota_pct_info(NULL,NULL,NULL,NULL,2,xmd_ota_pct,ERR_DEV_ABORT);

			exit_to_clean =1;
		}
		else if (xmd_state == XMODEM_TRANSMIT_COMPLETE)					// xmodem正常升级完成
		{
			uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ESP_LOGW(TAG, "arm ota success, elapsed: %lu seconds!", (now_time-ota_start_time)/1000);
			uart_ota_state.err_code = ERR_UPGRADE_NORMAL;
			uart_ota_state.is_ota = RF_OTA_STA_CPLT;					// 升级结束状态设置为已完成

            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta = RF_OTA_STA_IDLE;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].pct = 100;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode = ERR_UPGRADE_NORMAL;
            
            set_ota_pct_info(NULL,NULL,NULL,NULL,2,100,ERR_UPGRADE_NORMAL);
            
			exit_to_clean =1;
		}
    }

	if(1 == exit_to_clean)
	{
	
		sold_used_chl =0xFF;
		first_init_flag =0;
		xmodem_transmitter_cleanup();
	   if (recv_buff != NULL)
		{
			free(recv_buff);
			recv_buff = NULL;
		}
		
//		if (bin_ota_data != NULL)
//		{
//			free(bin_ota_data);
//			bin_ota_data = NULL;
//		}
	
		bin_data_size = 0;
//		bin_data_flash_flag = 0;
		
		/*重新备份当前运行区域代码至备份区*/
	//	  backup_task_create();   
		gXmodem_server_Status.ota_flag = 0;
		ESP_LOGW(TAG, "arm ota thread exit!");
//		esp_log_level_set("[MCU_CORE]", ESP_LOG_WARN);
	//	iot_wifi_open(WIFI_MODE_STA);						// 重新开启WiFi
//			iot_mqtt_start();
	//	vTaskDelete(NULL);									// 删除lcd升级任务

	}

    
}

///**
// * @brief 创建uart升级任务
// *
// * @return 0 if successful, otherwise -1
// */
//static int uart_ota_task_create(void)
//{
//	if (xTaskCreate(uart_ota_thread, "uart_ota_thread",  1024 * 4, NULL, 4, NULL) != pdPASS)
//	{
//		ESP_LOGE(TAG, "arm ota thread create failed");
//		return -1;
//	}
//
//	return 0;
//}

/**
 * @brief uart升级开始接口
 * - 当外部模块调用该接口时uart升级内核开始运行
 *
 * @param ota_set 升级设置参数
 * @param bin_path 固件路劲
 * @param bin_data 固件数据指针
 * @param bin_flash_flag 固件数据存储在flash备份区存储标志（1有效）
 * - bin_flash_flag, bin_path与bin_data可三选一,如果同时存在则取bin_flash_flag作为升级数据源
 *
 * @return 成功返回0否则返回-1
 */
int uart_ota_start(ota_set_t *ota_set, char *bin_path, uint8_t *bin_data, uint8_t bin_flash_flag)
{
	if (gXmodem_server_Status.ota_flag) 
	{
		ESP_LOGW(TAG, "uart_ota_start:gXmodem_server_Status.ota_flag =%d",gXmodem_server_Status.ota_flag);
		return -1;
	}
	
	if ((ota_set == NULL) || ((bin_path == NULL) && (bin_data == NULL) && (bin_flash_flag == 0)))
	{
		uart_ota_state.err_code = ERR_FILE_INVALID;
		return -1;
	}

	memcpy(&uart_ota_set, ota_set, sizeof(ota_set_t));
      
    ESP_LOGW(TAG, "lxy sub1g debug : Group[%d : %d] type=%u, version=%lu, size=%u",
    uart_ota_set.group.dev_type, uart_ota_set.group.dev_id,
    uart_ota_set.file_type,
    ((uint32_t)(uart_ota_set.ver_high << 16) | uart_ota_set.ver_low),
    uart_ota_set.file_size);
    
    if (bin_flash_flag == 1) 
	{
//        bin_data_flash_flag = 1;
    } 
	else if (bin_path) 
	{
//        strncpy(bin_file_name, bin_path, sizeof(bin_file_name) - 1);
//        bin_file_name[sizeof(bin_file_name) - 1] = '\0'; // 确保字符串以null结尾
    } else {
//        bin_ota_data = bin_data;
    }

	memset(&uart_ota_state, 0x00, sizeof(ota_state_t));
	uart_ota_state.group = uart_ota_set.group;
	uart_ota_state.is_ota = RF_OTA_STA_DOING;			// 正在升级
	uart_ota_state.level = 1;							// iot传递给设备
	uart_ota_state.target_pos = uart_ota_set.file_type;	// ota的目标位置
	uart_ota_state.now_pos = uart_ota_set.file_type;		// 当前位置
	if(OTA_CH_SELF_TO_UART == gXmodem_server_Status.used_chl)
	{
		uart_ota_state.online_client_cnt =1;

	}
	else
	{
		//	  uart_ota_state.online_client_cnt = get_mesh_node_cnt(SN_TYPE_S1); //默认当前设备均在线，可升级  ，待更新MESH库，否则编译失败
	}

//	if (uart_ota_task_create() != 0)
//	{
//        uart_ota_state.is_ota = RF_OTA_STA_CPLT;                  // 升级结束状态设置为已完成
//		uart_ota_state.err_code = ERR_OUT_OF_MEMORY;
//        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta = RF_OTA_STA_IDLE;
//        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode = ERR_OUT_OF_MEMORY;
//        set_ota_pct_info(NULL,NULL,NULL,NULL,2,0,ERR_OUT_OF_MEMORY);
//		return -1;
//	}

	gXmodem_server_Status.ota_flag = 1;
	ESP_LOGI(TAG, "arm ota start");
	return 0;
}

/**
 * @brief 获取uart模块升级状态
 *
 * @return 0-未升级, 1-正在升级
 */
int uart_ota_is_doing(void)
{
	return gXmodem_server_Status.ota_flag;
}

/**
 * @brief 获取备份状态
 *
 * @return 0-未在备份, 1-正在备份
 */
int backup_is_doing(void)
{
	return backup_flag;
}

/**
 * @brief uart升级模块数据接收接口
 *
 * @param data 接收到的数据指针
 * @param len 数据长度
 *
 * @return 无
 */
void uart_ota_recv(const uint8_t *data, int len)//自己做xmodem server/主机的 接收
{
	if (0== gXmodem_server_Status.ota_flag) return;

		if(NULL == recv_buff)
		{
			recv_buff = iot_calloc(len);
		}
		
		if(NULL != recv_buff)
		{
			memcpy(recv_buff, data, len);//指针已赋值
		}
		else
		{
			ESP_LOGE(TAG, "recv_buff malloc fail");
	
		}
	
		recv_len = len;
	
	
	//	if(OTA_CH_SELF_TO_UART == gXmodem_server_Status.used_chl)
	//	{
	//
	//	}
	//	else if(OTA_CH_SELF_TO_WIFI_MESH == gXmodem_server_Status.used_chl)
	//	{
	//		  if (NULL != data)
	//		  {
	//			  recv_buff = data;//windy recv_buff此处未开辟内存
	//		  }
	//		  else//预留
	//		  {
	////			recv_buff = &wireless_interface.data_rx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + 3];
	//		  }
	//	}

    // ESP_LOGW(TAG, "");
    // ESP_LOGW(TAG, "uart_ota_recv, recv_len:%d, data", recv_len);
    // ESP_LOG_BUFFER_HEX(TAG, recv_buff, recv_len);

//	if (uart_recv_sem) 
//    {
//	    xSemaphoreGive(uart_recv_sem);
//    }
}

/**
 * @brief 获取uart beta协议升级状态
 *
 * @return 返回状态指针
 */
ota_state_t* uart_ota_state_get(void)
{
	return &uart_ota_state;
}

/**
 * @brief 清除uart beta协议状态
 *
 * @return 无
 */
void uart_ota_state_clear(void)
{
	memset(&uart_ota_state, 0x00, sizeof(ota_state_t));
}

#else



int uart_ota_start(ota_set_t *ota_set, char *bin_path, uint8_t *bin_data, uint8_t bin_flash_flag) {
    // TODO: Implement the function
    return 0;
}

int uart_ota_is_doing(void) {
    // TODO: Implement the function
    return 0;
}

int backup_is_doing(void) {
    // TODO: Implement the function
    return 0;
}

void uart_ota_recv(const uint8_t *data, int len) {
    // TODO: Implement the function
}

ota_state_t* uart_ota_state_get(void) {
    // TODO: Implement the function
    return NULL;
}

void uart_ota_state_clear(void) {
    // TODO: Implement the function
}

void uart_ota_thread(void) {
    // TODO: Implement the function
}


#endif
