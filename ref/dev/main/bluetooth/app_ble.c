/**
  ******************************************************************************
  * @file      app_ble.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/1
  * @brief     蓝牙任务及接口函数
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/1   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "ble_dev.h"
#include "app_ble.h"


#include "modbus_protocol.h"
#include <string.h>
#include "iot_period_task.h"
#include "filesystem.h"
#include "uart_device_process.h"
#include "modbus_slave.h"
#include "iot_ble_encrypt.h"

#include "dev_discovery.h"
#include "udt_port.h"
#include "udt_transfer.h"
#include "utils.h"



#define TAG "[app_ble]"

uint16_t Modbus_Bind_Ask_Ble(void);

extern QueueHandle_t can_cmd_queue;
static QueueHandle_t xQueue_ble_ff03_tx = NULL;

static iot_ble_recv_struct iot_ble_recv = {
    .msg_queue = NULL,
};
	

 ble_encrypt_t ble_encrypt_info = {
    .flag.bit.is_enable = 0,
    .flag.bit.init_done = 0,      // 蓝牙初始化状态
    .flag.bit.is_adv_on = 0,      // 蓝牙广播状态
    .flag.bit.ble_connect = 0,   // 蓝牙连接状态
    .flag.bit.is_encrypted_fresh = 0,  //未绑定
    .flag.bit.is_encrypted = 0,  //未加密 	
	 .authenticate_state = BLE_AUTHENTICATE_REQUEST,
 };


 ble_authenticate_buf_t ble_authenticate_buf;
 static uint8_t retransmit_data[150]; //重传缓存
 static uint8_t retransmit_len = 0;
 static uint8_t ble_resp_data[260];



 static void ble_respond_debug(uint8_t *md_msg, uint8_t *rsp_msg, int16_t rsp_len)
 {
	 //调试ble响应的数据
	 uint16_t reg_addr = ((uint16_t)md_msg[2] << 8) | md_msg[3];
 
 // 	 ESP_LOGI(TAG, "");
		 ESP_LOGW(TAG, "ble respond, reg_addr:%d, data_len:%d, data:", reg_addr, rsp_len);
		 ESP_LOG_BUFFER_HEX(TAG, rsp_msg, rsp_len);
 }
 static void dump_buf(char *info, uint8_t *buf, uint32_t len)
 {
	 mbedtls_printf("%s", info);
	 for (uint32_t i = 0; i < len; i++) {
		 mbedtls_printf("%s%02X%s", i % 16 == 0 ? "\n	  ":" ", 
						 buf[i], i == len - 1 ? "\n":"");
	 }
 }
 
 
 
 /*------------------------------------------------------------------------
 iot_ble_response
 蓝牙发送报文
 
 *@brief  
 *@param[in]	 *rsp_data：BLE tx buf
 *@param[out]	 rsp_len:BLE tx len
 
 *@param[in] :TxChannel :BLE_FF01_CHAR_VAL/BLE_FF03_CHAR_VAL
 
 *@return		  
 
 */
void iot_ble_response(const uint8_t *rsp_data, uint16_t rsp_len,uint8_t TxChannel) 
 {
		 uint8_t *encrypt_data = NULL;
		 uint8_t *rsp_pdata = NULL;
		 uint16_t rsp_plen = 0;
		 uint16_t encrypt_len = 0;
	 
		//  ESP_LOGI(TAG, "iot_ble_response AA :%d",ble_encrypt_info.flag.bit.ble_connect);
		 if (!ble_encrypt_info.flag.bit.ble_connect) 
		 { /* 蓝牙连接断开 */
			 return ;
		 }
		 
	 
 
		//   ESP_LOGW(TAG, "iot_ble_response：respones len %d", rsp_len);
	 
		 if(ble_encrypt_info.flag.bit.is_encrypted == 1) //判断是否启用加密
		 {
			 encrypt_len = get_encrypt_sending_pack_len(rsp_len); //获取长度
			 if(ble_encrypt_info.authenticate_state == BLE_ENCRYPT_COMPLATE)
			 {
				 encrypt_len+=4; //加上随机数长度
			 }
			 encrypt_data = iot_calloc(encrypt_len * sizeof(char)); //申请内存
			 if(encrypt_data == NULL)
			 {
				 ESP_LOGI(TAG, "malloc fail");
				 return;
			 }
			 if(ble_encrypt_info.authenticate_state == BLE_ENCRYPT_COMPLATE)
			 {
				 //dump_buf("before encrypt", rsp_data, rsp_len);
				//  ESP_LOGI (TAG, "iot_ble_response : AES-CBC");
				 aes_cbc_encrypt_sending_pack(rsp_data, rsp_len, encrypt_data, &encrypt_len); //生成AES-CBC加密包
//				 ESP_LOGI(TAG, "after encrypt1 len %d", encrypt_len);
//				 
//				 dump_buf("after encrypt1", encrypt_data, encrypt_len);
			 }
			 else
			 {	 
				 ESP_LOGI (TAG, "iot_ble_response : AES-ECB");
				 encrypt_sending_pack(rsp_data, rsp_len, encrypt_data, &encrypt_len); //生成AES-ECB加密包
				 
//				 ESP_LOGI(TAG, "after encrypt2 len %d", encrypt_len);
//				 dump_buf("after encrypt2", encrypt_data, encrypt_len);
				 
			 }
			 //dump_buf("BLE TX real data:", rsp_data, rsp_len);
			 
	 // 		 ESP_LOGE(TAG, " BLE tx :reals.cnt_100ms = %lu", reals.cnt_100ms);
				 
			 rsp_pdata = encrypt_data;
			 rsp_plen = encrypt_len;
		 }
		 else  //未加密
		 {
			 ESP_LOGE (TAG, "iot_ble_response : unencrypted");
			 rsp_pdata = (uint8_t*)rsp_data;
			 rsp_plen = rsp_len;
		 }
 
 
		 if(BLE_FF01_CHAR_VAL == TxChannel)
		 {
			 ble_dev_send(&(ble_data_t){
				 .data = rsp_pdata,
				 .len = rsp_plen,
			 });
			 reals.ble_frame_tx_counter++;
		 }
		 else if(BLE_FF03_CHAR_VAL == TxChannel)
		 {
			 ble_dev_post(&(ble_data_t){
				 .data = rsp_pdata,
				 .len = rsp_plen,
			 });
			 reals.ble_frame_tx_counter++;
		 }
		 else
		 {
 
		 }
 
		 
	 if(encrypt_data != NULL)
	 {
		 free(encrypt_data);
		 encrypt_data = NULL;
	 }
 
 }


/**
 * @brief 打印hex数据
 *
 * @param tag 标签
 * @param buffer 数据指针
 * @param buff_len 数据长度
 * @param log_level 日志级别
 *
 * @return 无
 */
static void log_hex_custom(const char *tag, const void *buffer, uint16_t buff_len, esp_log_level_t log_level)
{
    /* 设置LOG_D的颜色为紫色 */
    #undef LOG_COLOR_D
    #define LOG_COLOR_D LOG_COLOR(LOG_COLOR_PURPLE)

    /* 设置每行字节数 */
	#define BYTES_PER_LINE 16

    if (buff_len == 0) return;

    char *pbuffer = (char*)buffer;
    char hex_buffer[3 * BYTES_PER_LINE + 1];
    const char *ptr_line;
    uint16_t bytes_cur_line;

    do {
        if (buff_len > BYTES_PER_LINE) {
            bytes_cur_line = BYTES_PER_LINE;
        } else {
            bytes_cur_line = buff_len;
        }
        ptr_line = pbuffer;

        for (int i = 0; i < bytes_cur_line; i ++) {
            sprintf(hex_buffer + 3 * i, "%02x ", ptr_line[i]);
        }
        ESP_LOG_LEVEL(log_level, tag, "%s", hex_buffer);
        pbuffer += bytes_cur_line;
        buff_len -= bytes_cur_line;
    } while (buff_len);
}

/**
 * @brief ble事件回调函数
 * 
 * @param evt ble事件
 */
static void app_ble_evt_callback(uint8_t evt)
{
	/* 当前蓝牙重新广播时表示蓝牙已中断，停止升级 */
	if (evt == BLE_EVT_ADV) {
//		xmd_ota_stop(BLE_OTA);
        vXmodemClientExit(OTA_CH_BLE_TO_SELF); /* 蓝牙断线,XMODEM协议退出 */
	}
}

/**
 * @brief 蓝牙升级响应函数
 * 
 * @param data 数据
 * @param len 长度
 * @return 成功返回0，失败返回-1
 */
static int ble_ota_data_reponse(void *data, int len)
{
	return ble_dev_send(&(ble_data_t){
		.data = (uint8_t*)data,
		.len = len,
	});
}
//
///**
// * @brief app modbus数据处理
// * 
// * @param data 数据
// * @param len 长度
//
// rx ->tx
// */
//static void app_modbus_data_handle(uint8_t *data, int len)
//{
//	/* modbus协议检查 */
//	if (md_protocol_check(data, len) != 0) return;
//
//	uint8_t *out_buffer = calloc(1024, 1);
//	assert(out_buffer);
//	md_buff_t md_buff = {0};
//	md_buff.in_buff = data;
//	md_buff.in_len = len;
//	md_buff.out_buff = out_buffer;
//	
//	/* 该数据由ota数据表处理 */
//	const md_priv_data_t priv_data = {
//		.channel = 0,//tbd BLE_OTA,						// ota通道类型
//		.ota_response = ble_ota_data_reponse,	// 传递给xmodem升级的响应函数
//	};
//
//	/* modbus数据读写处理 */
//	uint8_t funcode = data[1];
//	if (funcode == MB_READ_HOLD_REG) {
////		md_read_handle(&md_buff, (void *)&priv_data);
//	}
//	else {
////		md_write_handle(&md_buff, (void *)&priv_data);
//	}
//
//	/* modbus响应 */	
//	ble_dev_send(&(ble_data_t){
//		.data = md_buff.out_buff,
//		.len = md_buff.out_len,
//	});
//	ESP_LOGV(TAG, "response data len: %d, value:", *md_buff.out_len);
//	log_hex_custom(TAG, md_buff.out_buff, md_buff.out_len, ESP_LOG_DEBUG);
//	free(out_buffer);
//}


/**
windy
 * @brief app modbus数据处理
 * 
 * @param data 数据
 * @param len 长度

 rx ->tx
 */
static int app_modbus_data_handle(uint8_t *data, int len)
{
	int16_t rsp_len;
	uint8_t md_addr=0;
	int rst=0;

	/* modbus协议检查 */
//	if (md_protocol_check(data, len) != 0) 
//		return;
 	//ESP_LOGI(TAG,"app_modbus_data_handle DATA:%d-%d-%d-%d-%d-%d-%d",data[0],data[1],data[2],data[3],data[4],data[5],data[6]);
	rst = Modbus_Format_Check(data, len);
	//ESP_LOGI(TAG,"Modbus_Format_Check rst:%d",rst);
	if (rst >= 0)//功能码
	{
		 md_addr= data[0];
		/* 为modbus转can指令申请内存 */
		// can_cmd_queue_struct can_cmd = {NULL, 2, 0};//10，windy实际只有一次转发1帧
		can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};
		uint8_t can_cmd_flag = 0;
		if ((rst == 0x06 || rst == 0x10) && can_cmd_queue) 
		{ // 当MODBUS为设置指令时,才需要开辟空间
			can_cmd.cmd = heap_caps_malloc(sizeof(can_data_label) * can_cmd.num, MALLOC_CAP_SPIRAM);
			if (!can_cmd.cmd) {
				ESP_LOGE (TAG, "ble to can malloc failed");
			}
		}
		
		reg_position_list_t *position_list = NULL;
		rsp_len = Modbus_Slave(data, (uint16_t)len, ble_resp_data,	can_cmd.cmd, &can_cmd.num, MD_CHL_BLE, &position_list); /* modbus handle */
		ESP_LOGI(TAG,"rsp_len:%u",rsp_len);
		if (rsp_len > 0) 
		{
		
			len = 0;
		
			/* modbus响应 */	
			iot_ble_response(ble_resp_data, rsp_len,(uint8_t)BLE_FF01_CHAR_VAL);  // modbus 响应给手机

			printf("Ble response data len: %d, value:\n", rsp_len);
			// esp_log_buffer_hex(TAG, ble_resp_data, rsp_len);
		
		
			if (vXmodemCmdCheck(data[0], OTA_CH_BLE_TO_SELF)) //初始化xmodem升级变量
			{
				ESP_LOGW(TAG, "ble received xmodem start cmd");
			}
		
			/* modbus指令转换为can指令发送到队列 */
			if (can_cmd.cmd != NULL && can_cmd.num != 0) 
			{
				can_cmd.md_addr = md_addr;
				if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(100)) == pdTRUE) 
				{
					can_cmd_flag = 1;
					reals.BLE_WIFI_to_Can_Cnt++;
					ESP_LOGE(TAG, "BLE_WIFI_to_Can_Cnt HUANGJI AAA:%d", reals.BLE_WIFI_to_Can_Cnt);
				}
			}
			else if (NULL != position_list) 
			{
//				sys_new_position_and_transmit(position_list);//透传转发给下级uart
			}	
		
			if (!can_cmd_flag && can_cmd.cmd) 
			{
				free(can_cmd.cmd);
			}		
		}
	}else{
		return rst;
	}

	return rsp_len;

}


/*
windy add
加密 处理辅助
*/
void ble_encript_disconnect_reset(void)
{
	ble_encrypt_info.flag.bit.is_encrypted_fresh = 0; //绑定配对状态
	ble_encrypt_info.flag.bit.is_encrypted = 0; //加密状态
	ble_encrypt_info.flag.bit.ble_connect = 0;
	
	ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
//	memset(ble_encrypt_info.remote_mac, 0, sizeof(ble_info.remote_mac));
	
	if(!ble_encrypt_info.flag.bit.is_encrypted_fresh) //如果密钥参数未刷新
	{ 
		ble_authenticate_buf.status = BLE_REFRESH_ENCRYPT;
		ble_encrypt_info.flag.bit.is_encrypted_fresh = 1;
	}

}
void ble_encript_connected(void)
{
//	ble_encrypt_info.flag.bit.ble_connect = 1;
	
	ESP_LOGI(TAG, "ble_encript_connected");
}


/*
windy add
加密 处理辅助
*/
void ble_encript_part(uint8_t notify)
{
	if (notify == 0x0001)//通知
	{
		//打开notify通知
		if(ble_encrypt_info.authenticate_state == BLE_AUTHENTICATE_REQUEST)
		{
			ble_authenticate_buf.status = BLE_AUTHENTICATE_REQUEST;
		}
		ESP_LOGI(TAG, "ff01_notify : notify enable"); 
	}
	else if (notify == 0x0002)//指示
	{ 
		ESP_LOGI(TAG, "ff01_notify : indicate enable");
	}
	else if (notify == 0x0000)//关闭
	{ 
		if(ble_encrypt_info.authenticate_state != BLE_AUTHENTICATE_REQUEST)
		{
			ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
			ble_encrypt_info.flag.bit.is_encrypted = 0;
		}
		ESP_LOGI(TAG, "ff01_notify : notify/indicate disable ");
		if(!ble_encrypt_info.flag.bit.is_encrypted_fresh) //如果密钥参数未刷新
		{ 
			ble_authenticate_buf.status = BLE_REFRESH_ENCRYPT;
			ble_encrypt_info.flag.bit.is_encrypted_fresh = 1;
		}
	}
	else
	{
//		ESP_LOGE(TAG, "ff01_notify : unknown descr value");
//		esp_log_buffer_hex(TAG, param->write.value, param->write.len);
	}


}



/**
windy
 * @brief app modbus数据处理
 * 
 * @param data 数据
 * @param len 长度

 rx ->tx
 */
static void ble_rx_process(void)
{
	msg_struct income = {NULL, 0};//BLE rx 队列解析
	int rst=0;
	// ESP_LOGW(TAG,"ble_rx_process");
	if(iot_ble_recv.msg_queue && xQueueReceive(iot_ble_recv.msg_queue, &income, pdMS_TO_TICKS(40)) == pdTRUE )//400
	{
		reals.ble_frame_rx_counter++;
		rst=app_modbus_data_handle(income.msg_ptr, income.msg_len);
		if(rst> 0) 
        {
			ESP_LOGI(TAG,"rst:%d",rst);
        }
        else if ((income.msg_ptr[0] == UDT_PROT_MARK) && (income.msg_ptr[1] == UDT_PROT_VER))//服务器通用数据通道协议
        {
            ESP_LOGW(TAG, "udt received ble data and relay it to device, data length: %d, slave_addr: %d, funcode: %d",
                    income.msg_len, income.msg_ptr[10], income.msg_ptr[11]);

			#ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
            	/*数据通道*/
            	udt_process_handle(income.msg_ptr, income.msg_len, UDT_MODE_BLE);
			#endif
			goto _end;
        }
        ble_resp_data[0] = Xmodem_Client_top(OTA_CH_BLE_TO_SELF, income.msg_ptr, income.msg_len); /* xmodem运行 */
        if (ble_resp_data[0]) 
        {
            if (ble_resp_data[0] != 0x06) 
            {
                ESP_LOGI(TAG, "resp:0x%x", ble_resp_data[0]);
            }
            /* 通过modbus发送xmodem客户端的响应数据 */
            iot_ble_response(ble_resp_data, 1,(uint8_t)BLE_FF01_CHAR_VAL);
        }
    }

_end:
    if (income.msg_ptr != NULL) 
    {
        free(income.msg_ptr);
        income.msg_ptr = NULL;
    }

	if (gXmodem_Status.system_restart == 1) {
        gXmodem_Status.system_restart = 0;
		ESP_LOGI(TAG,"resatrt MQTT STOP");
		set_mqtt_stop();
        ESP_LOGI(TAG, "Only one iot Delay 500ms Prepare to restart system!");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
}

/**
 * @brief uart rx data透传给BLE
 * 
 * @param data 数据
 * @param len 长度

 rx ->tx
 */
static void uart_data_to_ble(uint8_t *data, int len)
{

	md_buff_t md_buff = {0};
	md_buff.in_buff = data;
	md_buff.in_len = len;
//	md_buff.out_buff = out_buffer;
	

	reals.struct_uart1.FlagRx_ok =0;
//	memcpy(md_buff.out_buff, reals.struct_uart1.Rxbuffer, reals.struct_uart1.rxBytesNum);
	md_buff.out_buff=reals.struct_uart1.Rxbuffer;

	md_buff.out_len =reals.struct_uart1.rxBytesNum;

	/* modbus响应 */	
	ble_dev_send(&(ble_data_t){
		.data = md_buff.out_buff,
		.len = md_buff.out_len,
	});
	ESP_LOGV(TAG, "response data len: %d, value:", md_buff.out_len);
	log_hex_custom(TAG, md_buff.out_buff, md_buff.out_len, ESP_LOG_DEBUG);
}

#define  ECDH_TX_LEN 134
/*

10ms cycle
*/
 void ble_authenticate_task_step(void )
{
	static uint8_t ble_authenticate_buf_old = 0xFF;//status
	uint16_t offset = 0;
	uint8_t data[ECDH_TX_LEN];//output ,[20];
	uint8_t len = 0;
    static uint8_t sauthenticate_state_old = 0xFF;
	
	static uint8_t sble_connect_old =0;
	static uint16_t sCntDelay =0;

	if(sauthenticate_state_old != ble_encrypt_info.authenticate_state)
	{
		sauthenticate_state_old = ble_encrypt_info.authenticate_state;
		if(BLE_ENCRYPT_COMPLATE == ble_encrypt_info.authenticate_state )
		{
			free(ble_authenticate_buf.data);
		}
	}


	 if(ble_authenticate_buf_old != ble_authenticate_buf.status)
	 {
		 ble_authenticate_buf_old = ble_authenticate_buf.status;

	 
		 ESP_LOGI(TAG, "ble_authenticate_buf.status=:%d:", ble_authenticate_buf.status);

	 
		 switch (ble_authenticate_buf.status)
		 {
 
		 case BLE_REFRESH_ENCRYPT: //刷新密钥
		 {
			 refresh_encrypt_param(); //刷新加密参数
			 ble_encrypt_info.flag.bit.is_encrypted_fresh = 0;
			 ble_encrypt_info.flag.bit.is_encrypted = 0,
			 ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
		 }
			 break;
 
		 case BLE_ENCRYPT_PACK_RETRANSMITSSION: //鉴权请求包重传

			 break;
 
		 case BLE_AUTHENTICATE_REQUEST: //鉴权请求
		 {		  
			 len = iot_ble_authen_request(data); //鉴权请求包
			 retransmit_len = len;
			 memcpy(retransmit_data, data, retransmit_len); //保存当前数据到重传包缓存
			 iot_ble_response(data, len,(uint8_t)BLE_FF01_CHAR_VAL);
			 ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_RESPOND; //下一步，鉴权响应
			 ESP_LOGI(TAG, "Authenicate request");
		 }
			 break;
		 case BLE_AUTHENTICATE_RESPOND: //鉴权响应
		 {
			 ESP_LOGI(TAG, "BLE_AUTHENTICATE_RESPOND");
			 if(get_pack_type(ble_authenticate_buf.data, ble_authenticate_buf.len) == AUTHEN_RESOPN) //检查类型
			 {
				 if(iot_ble_check_authen_respond(ble_authenticate_buf.data, ble_authenticate_buf.len, data, &len))//ok
				 {
					 ESP_LOGI(TAG, "Authenicate success!");
					 ble_encrypt_info.authenticate_state = BLE_ECDH_REQUEST; //ECDH密钥协商
					 iot_ble_response(data, len,(uint8_t)BLE_FF01_CHAR_VAL);
					 if(ble_encrypt_info.flag.bit.is_encrypted == 0)
					 {
						 ble_encrypt_info.flag.bit.is_encrypted = 1; //启用aes加密通信
					 }
					 ble_authenticate_buf.status = BLE_ECDH_REQUEST;
				 }
				 else
				 {
					 ESP_LOGI(TAG, "Authenicate fail!, so disconnect!");
					 ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
					 iot_ble_response(data, len,(uint8_t)BLE_FF01_CHAR_VAL);
					 //vTaskDelay(pdMS_TO_TICKS(10));
					 //主动断开连接
					 if(ble_disconnect() == ESP_OK) 
					 {
//						 ble_encrypt_info.flag.bit.ble_connect = 0;
					 } 
				 }
			 }
			 else
			 {
				 ESP_LOGI(TAG, "no BLE_AUTHENTICATE_RESPOND type");
			 }
		 }
			 break;
		 
		 case BLE_ECDH_REQUEST: //ECDH密钥协商请求
		 {
//			 uint8_t ecdh_request[134];
			 ESP_LOGI(TAG, "ECDH REQUEST!");
			 iot_ble_authen_ecdh_key_agreement_request(data); //生成ECDH密钥协商请求包
			 ESP_LOGI(TAG, "agreement_request");
 
			 retransmit_len = ECDH_TX_LEN;//sizeof(ecdh_request);
			 memcpy(retransmit_data, data, retransmit_len); //保存当前数据到重传包缓存
			 iot_ble_response(data, ECDH_TX_LEN,(uint8_t)BLE_FF01_CHAR_VAL);
			 dump_buf("ecdh_request", data, ECDH_TX_LEN);
			 ble_encrypt_info.authenticate_state = BLE_ECDH_RESPOND; //下一步，鉴权响应
 
			 //开启定时器
 
 
			 
		 }	 
			 break;
		 
		 case BLE_ECDH_RESPOND: //ECDH密钥协商回应
		 {
			 ESP_LOGI(TAG, "BLE_ECDH_RESPOND!,%d", ble_authenticate_buf.len);
			 //ESP_LOGI(TAG, "BLE_ECDH_RESPOND, len:%d", ble_authenticate_buf.len);
			 dump_buf("mobile respond data", ble_authenticate_buf.data, ble_authenticate_buf.len);
			 if(ble_encrypt_info.authenticate_state == BLE_ECDH_RESPOND)
			 {
				 if(iot_ble_authen_ecdh_key_agreement_respond_check(ble_authenticate_buf.data, ble_authenticate_buf.len, data, &len))
				 {
					 ESP_LOGI(TAG, "agreement success!");
					 retransmit_len = len;
					 memcpy(retransmit_data, data, retransmit_len); //保存当前数据到重传包缓存
					 dump_buf("after:mobile respond data", retransmit_data, retransmit_len);
					 iot_ble_response(data, len,(uint8_t)BLE_FF01_CHAR_VAL);
					 ble_encrypt_info.authenticate_state = BLE_AES_CBC_ENCRYPT; //AES-CBC加密
				 }
				 else
				 {
					 ESP_LOGI(TAG, "agreement fail, so disconnect!");
					 iot_ble_response(data, len,(uint8_t)BLE_FF01_CHAR_VAL);
					 ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
					 //vTaskDelay(pdMS_TO_TICKS(30));
					 //主动断开连接
					 if(ble_disconnect() == ESP_OK) 
					 {
//						 ble_encrypt_info.flag.bit.ble_connect = 0;
					 } 
				 }
			 dump_buf("respond data", data, len);
			 }
			 else
			 {
				 ESP_LOGI(TAG, "re-receive BLE_ECDH_RESPOND");
			 }
			 
		 }	 
			 break;
		 
 
		 default:
			 break;
		 }
 
 
	 }


} 
 //蓝牙鉴权任务
 /*
 windy:线程转while
 100ms cycle
 参考 lxy:加速断开
 */
 void ble_authenticate_task(void )
 {
	 

	 static uint16_t retry_times_1 = 0;
	 static uint16_t retry_times_2 = 0;
	 static uint16_t retry_times_3 = 0;

	 reals.authenticate_state= ble_encrypt_info.authenticate_state;
	 reals.ble_authenticate_buf_status=  ble_authenticate_buf.status;
	 reals.ble_encrypt_info_flag= ble_encrypt_info.flag.all;

	if(BLE_AUTHENTICATE_RESPOND == ble_encrypt_info.authenticate_state) //等待鉴权回复
	{
		if(++retry_times_1 > 50)//5s
		{
			retry_times_1 = 50;

			ESP_LOGI(TAG, "retry_times:%d,can not receive mobile authentical respond", retry_times_1);
			ble_disconnect();//断开连接
			
		}
		else if((retry_times_1 > 5)&&(retry_times_1 <= 10))//次数为5次。第1次重发间隔500ms，后4次重发间隔为50ms。
		{
			ESP_LOGI(TAG, "retransmit_len:%d",retransmit_len);				
			dump_buf("retransmit", retransmit_data, retransmit_len);
			ESP_LOGI(TAG, "AUTHENTICATE_RESPOND retry_times:%d",retry_times_1);
			  iot_ble_response(retransmit_data, retransmit_len,(uint8_t)BLE_FF01_CHAR_VAL);
		}
		retry_times_2 =0;
		retry_times_3 =0;

	}
	else if(BLE_ECDH_RESPOND == ble_encrypt_info.authenticate_state) //等待鉴权响应
	{
		if(++retry_times_2 > 50)//5s
		{
			retry_times_2 = 50;

			ESP_LOGI(TAG, "retry_times:%d,can not receive mobile ECDH respond", retry_times_2);
			ble_disconnect();//断开连接
			
		}
		else if((retry_times_2 > 5)&&(retry_times_2 <= 10))//次数为5次。第1次重发间隔500ms，后4次重发间隔为50ms。
		{
			ESP_LOGI(TAG, "retransmit_len:%d",retransmit_len);		
			dump_buf("retransmit", retransmit_data, retransmit_len);
			ESP_LOGI(TAG, "AUTHENTICATE_RESPOND retry_times:%d",retry_times_2);
			  iot_ble_response(retransmit_data, retransmit_len,(uint8_t)BLE_FF01_CHAR_VAL);
		}

		retry_times_1 =0;
		retry_times_3 =0;

	}
	else if(BLE_AES_CBC_ENCRYPT == ble_encrypt_info.authenticate_state) //等待手机正式数据
	{
		if(++retry_times_3 > 110)//11s
		{
			retry_times_3 = 110;

			ESP_LOGI(TAG, "retry_times:%d,can not receive mobile modbus", retry_times_3);
			ble_disconnect();//断开连接
			
		}
		else if((retry_times_3 >= 50)&&(0 == (retry_times_3%10)))//次数为5次。第1次重发间隔5s，后4次重发间隔为1s。
		{
			ESP_LOGI(TAG, "retransmit_len:%d",retransmit_len);		
			dump_buf("retransmit", retransmit_data, retransmit_len);
			ESP_LOGI(TAG, "BLE_AES_CBC_ENCRYPT，retry_times:%d,can not receive mobile modbus", retry_times_3);
			  iot_ble_response(retransmit_data, retransmit_len,(uint8_t)BLE_FF01_CHAR_VAL);
		}

		retry_times_1 =0;
		retry_times_2 =0;

	}
	else
	{
		retry_times_1 =0;
		retry_times_2 =0;
		retry_times_3 =0;
		

	}



 

	 
 }

/*------------------------------------------------------------------------
*@Function： ble_data_give
蓝牙接收，填充接收队列


*pdata：原始报文

income:解密后报文
iot_ble_recv.msg_queue：解密后压入队列

*@return		  
*/
 void ble_data_give(const uint8_t *pdata, uint16_t len) //static
{
    msg_struct income = {NULL, 0};
    uint16_t rec_len = 0;

    //ESP_LOGE(TAG, "ble_data_give,len:%d", len);
    //如果启用加密，并且授权通过
    if((ble_encrypt_info.flag.bit.is_encrypted == 1 )//判断是否启用加密
		&& (ble_encrypt_info.authenticate_state >= BLE_AES_CBC_ENCRYPT) )//加密握手成功才解析报文
    {
        if(ble_encrypt_info.authenticate_state == BLE_AES_CBC_ENCRYPT)
		{
			ble_encrypt_info.authenticate_state = BLE_ENCRYPT_COMPLATE;
			ESP_LOGI(TAG, "BLE_ENCRYPT_COMPLATE");
		}
        if(len >= 22 && len < 1500) //接收长度限制
        {
            income.msg_ptr = iot_calloc(len * sizeof(char));
            if (income.msg_ptr == NULL) 
            {
                ESP_LOGE(TAG, "malloc fail");
                return ;
            }
            if(ble_encrypt_info.authenticate_state == BLE_ENCRYPT_COMPLATE)//BLE_AES_CBC_ENCRYPT
            {
                ESP_LOGW(TAG, "before decrypt len:%d", len);
                //dump_buf("before decrypt", pdata, len);
                aes_cbc_decrypt_receive_pack(pdata, len, income.msg_ptr, &income.msg_len); //cbc解密
                ESP_LOGW(TAG, "after decrypt len:%d", income.msg_len);
//                ESP_LOG_BUFFER_HEX_LEVEL(TAG, income.msg_ptr, income.msg_len, ESP_LOG_WARN);
            }
           // else
            //{
                //decrypt_receive_pack(pdata, len, income.msg_ptr, &income.msg_len); 
            //}
            
            if(income.msg_len > len) //如果明文长度超过接收长度，提示非法长度，限制长度，防止溢出
            {
                ESP_LOGE(TAG, "over len:%d", income.msg_len);
                income.msg_len = len;
            }
			if (iot_ble_recv.msg_queue && xQueueSend(iot_ble_recv.msg_queue, &income, pdMS_TO_TICKS(100)) != pdPASS) {
				free(income.msg_ptr);
			}
			
        }
        else
        {
            ESP_LOGE(TAG, "invalid len:%d", len);
        }
            
    }
    else if(BLE_AUTHENTICATE_RESPOND == ble_encrypt_info.authenticate_state) //鉴权回应
    {
        ble_authenticate_buf.data = NULL;
        ble_authenticate_buf.data = (uint8_t*)iot_calloc(sizeof(uint8_t) * 1024);//在加密开始开辟内存，在加密过程完成后释放，减少RAM占用
        if(NULL != ble_authenticate_buf.data )// 
        {	
			ble_authenticate_buf.len = len;
        	ble_authenticate_buf.status = BLE_AUTHENTICATE_RESPOND; //鉴权结果
        	memcpy(ble_authenticate_buf.data, pdata, ble_authenticate_buf.len);
			ESP_LOGI(TAG, "send BLE_AUTHENTICATE_RESPOND");
        }
		else
		{
			ESP_LOGE(TAG, "malloc fail");
		}
    }
    else if(BLE_ECDH_RESPOND == ble_encrypt_info.authenticate_state) //ECDH密钥回应
    {
        //ESP_LOGE(TAG, "receive BLE_ECDH_RESPOND, len:%d", len);
        dump_buf("receive BLE_ECDH_RESPOND", pdata, len);
        if(get_pack_type(pdata, len) != AUTHEN_RESOPN) //如果不是鉴权回应包
        {

			if(NULL != ble_authenticate_buf.data )// 
        	{		
				decrypt_receive_pack(pdata, len, ble_authenticate_buf.data, &ble_authenticate_buf.len); //AES-ECB解密
				dump_buf("befor aes_cbc128 decrypt", ble_authenticate_buf.data, ble_authenticate_buf.len);		    
				
				//ESP_LOGI(TAG, "type:%d",get_pack_type(ble_authenticate_buf.data, ble_authenticate_buf.len));
				if(get_pack_type(ble_authenticate_buf.data, ble_authenticate_buf.len) == ECDH_AGRESSMENT_RESPOND) //类型为密钥协商回应
				{
					ble_authenticate_buf.status = BLE_ECDH_RESPOND; //ECDH密钥协商响应
				}
				else
				{
					ESP_LOGE(TAG, "no ECDH_AGRESSMENT_RESPOND type");
				}
        	}
			else
			{
				ESP_LOGE(TAG, "malloc fail");
			}
        }
        else //鉴权回应包，丢弃
        {
            ESP_LOGE(TAG, "Repeated authen respond, abandon");
        }
    }
    else //未加密
    {

        ESP_LOGE(TAG, "without authenicate verify!");
    }
    
}



/**
 * @brief 蓝牙通信线程
 * 
 * @param param 线程参数
 */
void iot_ble_task(void *pvParameters)
{
    /* 设置蓝牙名称 */
    char ble_name1[30];
    char ble_name2[30];
    char ble_name3[30];

	ble_ff03_tx_queue_init();
//	char dev_sn[8];
//	uint64_t Parallel_dev_sn;		//
//	uint64_t INV_dev_sn;		//
//	uint64_t iot_sn;		//

	
//	Parallel_dev_sn = SetData.dev_info_t.Parallel_dev_sn;		//
//	INV_dev_sn = SetData.dev_info_t.INV_dev_sn;		//
//	iot_sn = iot_factory.iot_sn ;		//


//testwx ble
	snprintf(ble_name1, sizeof(ble_name1), "%s%llu", SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);//用ACHUB的SN
	snprintf(ble_name2, sizeof(ble_name2), "%s%llu", SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn);//用INV的SN
	snprintf(ble_name3, sizeof(ble_name3), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);//暂时用IOT自身的SN IOT_TYPE_BLE_DEFAULT

    // snprintf(&ble_name, sizeof(ble_name), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);
//sprintf(ble_name, sizeof(ble_name), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);//暂时用IOT自身的SN IOT_TYPE_BLE_DEFAULT

//BLE 广播SN 优先级 ACHUB> INV > IOT

//testwx ble
//windy:规避编译器gcc  错误长度限制
	if(0 != SetData.dev_info_t.Parallel_dev_sn )
	{
		// sprintf(dev_sn, "%" PRIu64 "", (uint64_t)SetData.dev_info_t.Parallel_dev_sn);//用INV的SN
////		sprintf(dev_sn, "%" PRIu64 "", (unsigned long long)SetData.dev_info_t.Parallel_dev_sn);//用INV的SN		
	// sprintf(dev_sn, "%llu", (uint64_t)SetData.dev_info_t.Parallel_dev_sn);//用INV的SN	
	// 	snprintf(ble_name, sizeof(ble_name), "%s%s", (char *)SetData.dev_info_t.Parallel_dev_type, dev_sn);//用INV的SN
	// 	snprintf(ble_name, sizeof(ble_name), "%s%llu", (char *)SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);//用INV的SN
	// 	snprintf(ble_name, sizeof(ble_name), "%s%llu", SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);//用INV的SN
	ESP_LOGI (TAG, "Parallel_dev_sn iot_ble_task init,ble_name =%s" ,ble_name1);
    assert(!ble_dev_init(ble_name1, app_ble_evt_callback)); // 蓝牙初始化
		
	}
	else if(0 != SetData.dev_info_t.INV_dev_sn )
	// if(0 != SetData.dev_info_t.INV_dev_sn )
	{
//		snprintf(ble_name, sizeof(ble_name), "%s%llu", SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn);//用INV的SN
		ESP_LOGI (TAG, "INV_dev_sn iot_ble_task init,ble_name =%s" ,ble_name2);
		assert(!ble_dev_init(ble_name2, app_ble_evt_callback)); // 蓝牙初始化

	}	
	else //if(0 == iot_factory.iot_sn )//未标定，默认用IOT的SN
	{
//		snprintf(ble_name, sizeof(ble_name), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);//暂时用IOT自身的SN IOT_TYPE_BLE_DEFAULT
		ESP_LOGI (TAG, "iot_sn iot_ble_task init,ble_name =%s" ,ble_name3);
		assert(!ble_dev_init(ble_name3, app_ble_evt_callback)); // 蓝牙初始化

	}				


    /* 蓝牙接收消息队列 */ 
	iot_ble_recv.msg_queue = xQueueCreate(5, sizeof(msg_struct)); 
	if (!iot_ble_recv.msg_queue) 
	{
        ESP_LOGE (TAG, "ble received queue create failed");
	}

#ifdef FORCE_BLE_WIFI_ON_ENABLE
	
	
#else
    if ((SetData.dev_info_t.on_off.bit.ble_enable==0x02)||(0 == reals.Addr_can_master))
    {
		ESP_LOGW(TAG,"000--- ble stop!");
        /*蓝牙停止广播*/
        iot_ble_stop();
    }	
	
#endif


	while(1)
	{

		/* 尝试接收数据：短超时以便周期任务异步写入 FF03 队列后仍能唤醒本任务发送
		 * （TLV 分帧在 modbus_tlv_process_task 入队，晚于本轮 ble_rx_process）。 */
		ble_data_t pdata = {0};
		int ret = ble_dev_recv(&pdata, 10);
		if (ret == 0)//ok
		{
			ESP_LOGV(TAG, "ble received data len: %d, value:", pdata.len);
//			log_hex_custom(TAG, pdata.data, pdata.len, ESP_LOG_DEBUG);
			ble_data_give(pdata.data, pdata.len);
		
		}
        // ESP_LOGE (TAG, "enter ble_rx_process:%d",ret);
    	/* 数据处理 */
		ble_rx_process();

		/*BLE FF03主动上报任务*/
		ble_ff03_report_task();	
        

		/* 释放接收蓝牙数据时分配的内存 */
		if (pdata.data) {
			free(pdata.data);
			pdata.data=NULL;
		}
	}
}


/*------------------------------------------------------------------------------
 Function: iot_ble_new_name
 -----------------------------------------------------------------------------*/
/**
  * @brief      重新设置蓝牙名称
  * @param[in]  const char *dev_type  
                uint64_t dev_sn       
  * @param[out] None
  * @return     void
  */
void iot_ble_new_name(const char *dev_type, uint64_t dev_sn)
{
    if (!dev_type || !strlen(dev_type))
	{
        ESP_LOGE(TAG, "ble unkonw dev");
        return ;
    }
    char new_name[30] = {0};
    snprintf(new_name, sizeof(new_name), "%s%llu", dev_type, dev_sn);
    uint8_t name_len = strlen(new_name);
    if (name_len > 29) {
        name_len = 29;
    }
    if(ble_set_name(new_name) != 0)
    {
        ESP_LOGE(TAG, "ble_set_name error");
    }
}

inline const ble_encrypt_t *iot_get_ble_info(void) 
{
    return &ble_encrypt_info;
}

/*------------------------------------------------------------------------------
 Function: iot_ble_start
 -----------------------------------------------------------------------------*/
/**
  * @brief      蓝牙开启
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_ble_start(void)
{
    if(ble_advertise_start() != 0)
    {
        ESP_LOGE(TAG, "iot_ble_start error");
    }
	
	ble_encrypt_info.flag.bit.is_enable =1;
	
	ESP_LOGI(TAG, "iot_ble_start ,ble_encrypt_info.flag.bit.is_enable= %d",ble_encrypt_info.flag.bit.is_enable);
}

/*------------------------------------------------------------------------------
 Function: iot_ble_stop
 -----------------------------------------------------------------------------*/
/**
  * @brief      关闭蓝牙:广播和连接
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_ble_stop(void)
{
    /* 先清使能标志，避免异步 DISCONNECT 回调里又恢复广播 */
	ble_encrypt_info.flag.bit.is_enable =0;

    if(ble_advertise_stop() != 0)
    {
        ESP_LOGE(TAG, "iot_ble_stop error");
    }
//	if(ble_disconnect() != ESP_OK) 
//    {
//        ESP_LOGE(TAG, "ble_disconnect error");
//    }
}

/**
 * @brief BLE广播状态检查（未连接且蓝牙使能时，广播异常停掉则尝试恢复）
 */
void ble_adv_status_check(void)
{
    static uint8_t restart_adv_cnt = 0;

    bool adv_running = drv_ble_advertise_status();
    ble_encrypt_info.flag.bit.is_adv_on = adv_running ? 1 : 0;

    if (ble_encrypt_info.flag.bit.is_enable
        && !ble_encrypt_info.flag.bit.ble_connect) {
        if (!adv_running) {
            restart_adv_cnt++;
            if (restart_adv_cnt > 2) {
                drv_ble_advertise_restart();
                ESP_LOGW(TAG, "ble_adv_status_check: Broadcasting is restart.");
            }
        } else {
            restart_adv_cnt = 0;
        }
    } else {
        restart_adv_cnt = 0;
    }
}

/*
动态更改BLE 广播名称
*/
void iot_ble_rename(void) 
{
		/* 设置蓝牙名称 */
//	char ble_name[30];
	uint64_t temp_sn=0;	
	static uint8_t s_cnt= 0; 
	static uint8_t s_pingpang= 0; 
	static uint8_t s_debugcnt= 0; 

//	if(++s_cnt >= 30)//5s
//	{
//		s_cnt =0;
//		s_debugcnt++;
//
//		if(0 == s_pingpang)
//		{
//			s_pingpang =1;
//			iot_ble_stop();// 
//			
//			memcpy(SetData.dev_info_t.Parallel_dev_type, "PARAL_TYPE",sizeof("PARAL_TYPE"));
//			SetData.dev_info_t.Parallel_dev_sn = 10000 + s_debugcnt;
//			iot_ble_new_name(SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);
//			ESP_LOGE(TAG, " ble rename: %s%llu", SetData.dev_info_t.Parallel_dev_type,SetData.dev_info_t.Parallel_dev_sn);
//			ESP_LOGE(TAG, "1 ble dev_name: %s, second=%d", drv_ble_name_get() ,reals.rtc_time.sec);
//
//			iot_ble_start();
//
//		}
//		else
//		{
//			s_pingpang =0;
//			iot_ble_stop();// 
//			
//			memcpy(SetData.dev_info_t.INV_dev_type, "INV_TYPE",sizeof("INV_TYPE"));
//			SetData.dev_info_t.INV_dev_sn = 10000 + s_debugcnt;
//			iot_ble_new_name(SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn);
//			ESP_LOGE(TAG, " ble rename: %s%llu", SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.INV_dev_sn);
//			ESP_LOGE(TAG, "2 ble dev_name: %s, second=%d", drv_ble_name_get() ,reals.rtc_time.sec);
//			iot_ble_start();
//
//		}
//
//	}
//	else
//	{
//		return;
//	}



	//testwx ble
	if(0 != SetData.dev_info_t.Parallel_dev_sn )
	{
		iot_ble_stop();// 
		iot_ble_new_name(SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);
		ESP_LOGE(TAG, " ble rename: %s%llu", SetData.dev_info_t.Parallel_dev_type,SetData.dev_info_t.Parallel_dev_sn);
		ESP_LOGE(TAG, "1 ble dev_name: %s, second=%d", drv_ble_name_get() ,reals.rtc_time.sec);
		iot_ble_start();
		
	}
	else if(0 != SetData.dev_info_t.INV_dev_sn )
	// if(0 != SetData.dev_info_t.INV_dev_sn )
	{
		iot_ble_stop();// 
		iot_ble_new_name(SetData.dev_info_t.INV_dev_type, SetData.dev_info_t.INV_dev_sn);
		// iot_ble_new_name(SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);//test
		ESP_LOGE(TAG, " ble rename: %s%llu", SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.INV_dev_sn);
		ESP_LOGE(TAG, "2 ble dev_name: %s, second=%d", drv_ble_name_get() ,reals.rtc_time.sec);
		iot_ble_start();
	
	}	
	else if(0 != iot_factory.iot_sn )// 
	{
		iot_ble_stop();// 
		iot_ble_new_name(iot_factory.iot_type, iot_factory.iot_sn);
		ESP_LOGI(TAG, "HUANGJI GGG type:%s -%d -%d",iot_factory.iot_type,  (unsigned int)iot_factory.iot_sn, (unsigned int)iot_factory.safe_code);
		ESP_LOGI(TAG, "HUANGJI GGG type:%s -%d -%d",SetData.dev_info_t.Parallel_dev_type,  (unsigned int)SetData.dev_info_t.Parallel_dev_sn, (unsigned int)SetData.dev_info_t.Parallel_dev_safetycode);
		ESP_LOGE(TAG, " ble rename: %s%llu", iot_factory.iot_type,iot_factory.iot_sn);
		ESP_LOGE(TAG, "3 ble dev_name: %s, second=%d", drv_ble_name_get() ,reals.rtc_time.sec);
		iot_ble_start();

	}		
	else //if(0 == iot_factory.iot_sn )//未标定，默认用IOT的SN
	{
		iot_ble_stop();// 
		iot_ble_new_name("IOT", 123456);
		ESP_LOGE(TAG, " ble rename: %s", BLE_DEFAULT_NAME);
		ESP_LOGE(TAG, "4 ble dev_name: %s, second=%d", drv_ble_name_get() ,reals.rtc_time.sec);
		iot_ble_start();

	}	

	
}


 /*------------------------------------------------------------------------------
  Function: ble_ask_tx_queue_init
  -----------------------------------------------------------------------------*/
 /**
   * @brief      队列初始化
   * @param[in]  void  
   * @param[out] None
   * @return     void
   */
 void ble_ff03_tx_queue_init(void)
 {
     /*队列发送*/
     if (xQueue_ble_ff03_tx == NULL)
     {
         xQueue_ble_ff03_tx = xQueueCreate(20, sizeof(ble_ff03_tx_struct));
         if (xQueue_ble_ff03_tx == NULL)
         {
             ESP_LOGE(TAG, "ble_ff03_tx_queue_init create failed");
         }
     }
 }
 
 /*------------------------------------------------------------------------------
  Function: ble_ask_tx_queue_push
  -----------------------------------------------------------------------------*/
 /**
   * @brief      紧急发送存储到队列缓存
   * @param[in]  void  
   * @param[out] None
   * @return     void
   */
 void ble_ff03_tx_queue_push(uint8_t *pdata, uint16_t len)
 {
     ble_ff03_tx_struct queue_msg;
 
     queue_msg.data = (uint8_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
 
     if (!queue_msg.data) 
     {
         ESP_LOGE(TAG, "ble_ff03_tx_queue_push calloc failed");
     } 
     else 
     {
         memcpy(queue_msg.data, pdata, len);
         queue_msg.data_len = len;
         
         /*消息保存到队列*/
         if (xQueueSendToBack(xQueue_ble_ff03_tx, &queue_msg, 0) != pdPASS) 
         {
             free(queue_msg.data);
         }
     }
 }

/*------------------------------------------------------------------------------
 Function: ble_ask_report_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      ble ff03主动上报任务
  * @param[in]  void  
  * @param[out] None
  * @return     void b
  */
 uint8_t ble_ff03_report_task(void)
 {
	uint8_t rtn=0;
	ble_ff03_tx_struct queue_msg;

	if(1 == ble_encrypt_info.flag.bit.ble_connect)
	{
		/* 连续取出并发送：同一轮内可能已积压多包；配合 ble_dev_recv 短超时，
		 * 避免 TLV 等在其它任务入队多帧后仍长时间堵在单次 xQueueReceive。 */
		while (xQueue_ble_ff03_tx && xQueueReceive(xQueue_ble_ff03_tx, &queue_msg, 0) == pdTRUE)
		{
			if (queue_msg.data)
			{
				iot_ble_response(queue_msg.data, queue_msg.data_len, (uint8_t)BLE_FF03_CHAR_VAL);
				free(queue_msg.data);
				queue_msg.data = NULL;
				if(reals.iot_ota_flag==2)//&&(gXmodem_Status.firmware_resend_step ==2))
				{
					ESP_LOGI(TAG,"HUANGJI AAA ble gXmodem_Status.firmware_resend_step:%d",gXmodem_Status.firmware_resend_step);
					//gXmodem_Status.firmware_resend_step=3;
					reals.iot_ota_flag=3;
					reals.iot_ota_end_count=10;//10秒
				}
				rtn = 1;
			}
		}
	}

	if(!rtn)
		rtn=udt_ble_Report_to_app();
	
	return rtn;
 }


uint16_t Modbus_Bind_Ask_Ble(void) 
{
    uint16_t rsp_len = 0;
	uint8_t pointcnt = 0;
	uint8_t sendcnt = 0;
	uint8_t point_tx_nums = 0;		
    uint16_t ver = 0;
	uint8_t point_bias = 0;	
	uint8_t is_ota_reply=0;

    if(0 == ble_encrypt_info.flag.bit.ble_connect) return 0;  //蓝牙未连接
	if(!Modbus_21000_semaphore_Take())return 0;

	//if( reals.bind_state_ask)
	 	//ver = reals.bind_state_ask;
	//if(reals.modbus_self_report_ble)
	ver=reals.modbus_self_report_ble;
    //ESP_LOGW(TAG,"test get in Modbus_Bind_Ask_Ble:%d",ver);
    switch ( ver )
    { 
        case 1 :

            /*绑定帧响应*/
            pointcnt=Modbus_21000_1_Net_Point_Frame();
			break;
            
        case 3 :

            /*升级状态查询响应*/
            pointcnt=Modbus_21000_3_Net_Point_Frame();
			is_ota_reply=1;
			ESP_LOGW(TAG, "Modbus_Bind_Ask_Ble(03)  pointcnt:%d", pointcnt);
			/*
			rsp_len = Modbus_ReadRegs_Bind_Ack_Frame(ble_resp_data, pointcnt*(sizeof(POINT_BIND_INFO)/2), ver);
			
			ESP_LOGW(TAG, "Modbus_Bind_Ask_Ble, reg_addr:%d, data_len:%d, data:", 21000, rsp_len);
			ESP_LOG_BUFFER_HEX_LEVEL(TAG, ble_resp_data, rsp_len, ESP_LOG_WARN);
			
			//iot_ble_response(ble_resp_data, rsp_len,(uint8_t)BLE_FF01_CHAR_VAL);  // modbus 响应给手机 
			ble_ff03_tx_queue_push(ble_resp_data, rsp_len);	
            // ESP_LOGW(TAG, "Modbus_Bind_Ask_Ble(03)  Subnet_point_Num:%d", reals.Subnet_point_Num);
			ESP_LOGW(TAG, "Modbus_Bind_Ask_Ble(03)  Subnet_point_Num:%d",pointcnt);
			*/
            break;
            
        default:

           // rsp_len = Modbus_Error(ble_resp_data, CMD_NOT_COMPLETE);
            //iot_ble_response(ble_resp_data, rsp_len,(uint8_t)BLE_FF01_CHAR_VAL);  // modbus 响应给手机 	
            
           break;
    }

	do
	{
		//ESP_LOGW(TAG, "Modbus_Self_Report_Ble  pointcnt:%d sendcnt:%d", pointcnt,sendcnt);
		
		if(pointcnt >= BIND_READ_MAX_DEV_NUM)
		{
			pointcnt -= BIND_READ_MAX_DEV_NUM;
			point_tx_nums = BIND_READ_MAX_DEV_NUM;
		}
		else if(pointcnt > 0)
		{
			point_tx_nums = pointcnt;	
			pointcnt = 0;
			
		}		
		else
		{
			break;
		}
		//Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias=pointcnt;
		g_self_data.mod_reg21000_bind.bias=sendcnt;
		//Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias = point_bias;
		point_bias += point_tx_nums;
        uint8_t reg_cnt = 0;

        reg_cnt = point_tx_nums * (sizeof(POINT_BIND_INFO) / 2);

		rsp_len = Modbus_WriteMultiRegs_Report_Frame(ble_resp_data, reg_cnt, ver);
		
		ESP_LOGW(TAG, "Modbus_Self_Report_Ble:%d, reg_addr:21000, Ver:%u, data_len:%d, bias:%d, pointcnt:%d-%d",
                reals.modbus_self_report_ble, ver, rsp_len, point_bias, pointcnt, reg_cnt);
		ESP_LOG_BUFFER_HEX_LEVEL(TAG, ble_resp_data, rsp_len, ESP_LOG_WARN);

		ble_ff03_tx_queue_push(ble_resp_data, rsp_len);
		sendcnt+=point_tx_nums;
	}
	while (pointcnt);	

	
    //if((reals.bind_state_ask)&&(ver==reals.bind_state_ask))
		//reals.bind_state_ask = 0;
	if((reals.modbus_self_report_ble)&&(ver==reals.modbus_self_report_ble))
    {
		reals.modbus_self_report_ble=0;
        ESP_LOGI(TAG, "Clear reals.modbus_self_report_ble");
    }
	if(is_ota_reply&&(reals.iot_ota_flag==1))//(gXmodem_Status.firmware_resend_step ==2))
	{
		ESP_LOGI(TAG,"ble reals.iot_ota_flag:%d",reals.iot_ota_flag);
		reals.iot_ota_flag=2;
	}

	Modbus_21000_semaphore_Give();

    return rsp_len;
}