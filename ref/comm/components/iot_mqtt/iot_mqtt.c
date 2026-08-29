/**
  ******************************************************************************
  * @file      iot_mqtt.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/24
  * @brief     mqtt相关接口及任务
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/24  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
//#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "netdb.h"
#include "esp_log.h"

#include "iot_mqtt.h"
#include "iot_wifi.h"
#include "crc.h"
#include "cJSON.h"
#include "iot_define.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "filesystem.h"

// #include "uart_device_process.h"
#include "iot_rsa.h"//A904503
#include "esp_http_client.h"
#include "esp_random.h"
#include "mbedtls/md.h"
#include "md5.h"
#include "mbedtls/platform_util.h"
#include <mbedtls/sha256.h>
#include "can_protocol.h"
#include "modbus_define.h"
#include "modbus_data.h"
#include "modbus_protocol.h"
#include "json_cmd.h"
#include "modbus_slave.h"
#include "modbus_master.h"
#include "http_client.h"
#include "xmodem_client.h"
#include "udt_transfer.h"
#include "udt_port.h"
#include "utils.h"
#include "comm_define.h"
#include "parameter.h"
#include "DisasterWarn.h"
#include "task_debug.h"
#include "usb_host_modem.h"
#include "bms_can.h"
#include "mqtt_payload_tlv.h"
#include "disater_warn_simple.h"
#include "open_mqtt.h"
#include "iot_partition.h"
#include "json_cmd.h"
#include "modbus_tlv.h"
#include "sync_time.h"
#include "cloud_sync.h"
#include "mqtt_log.h"

#define TAG   "[MQTTC]"

/*------------------------------------------------------------------------------------------------------------------------*/

#define MAX_HTTP_RECV_BUFFER        1024
#define MAX_HTTP_OUTPUT_BUFFER      1024
#define download_buffer_len         2048
#define PARTITION_SIZE              4096

#define ESP_TRANS_TIMEOUT			10000	// ESP传输层(TCP)收发超时时间定义
#define MQTT_TX_BUF_LEN             300
#define MQTT_RECV_MAX_LEN           16384

#define NO_STD_PROTOCOL_HEADER      4       // 非标协议头
#define BIND_LIST_MAX_NUM           16

/*------------------------------------------------------------------------------------------------------------------------*/

USE_EXT_RAM_BSS static uint8_t mqtt_tx_buf[MQTT_TX_BUF_LEN] = {0};//周期发送缓存
static SemaphoreHandle_t mqtt_semaphore = NULL;

static uint16_t old_dev_soc = 0;
static device_ctrl old_dev_ctrl = {0};
static uint8_t login_step = 0;//WIFI通道的MQTT登陆状态机
USE_EXT_RAM_BSS login_info_t login_info = {0};//static

static struct esp_mqtt_client_dump mqtt_client_dump = {0};

static TimerHandle_t mqtt_sub_timeout_tmr = NULL;   // 订阅超时定时器
static TimerHandle_t mqtt_recv_timeout_tmr = NULL;  // 接收超时定时器
static uint8_t *mqtt_recv_buf = NULL;
static uint16_t mqtt_recv_len = 0;
static QueueHandle_t xQueue_Mqtt_Recv = NULL;

USE_EXT_RAM_BSS esp_mqtt_client_config_t mqtt_cfg = {0};
static uint8_t exit_flag = 0;//断开连接

static QueueHandle_t xQueue_Mqtt_Active_Report = NULL;
static uint32_t Mqtt_Machine_Pause_Time = 0;
static bool Mqtt_Restart_Flag = false;

static uint32_t Mqtt_Machine_Connect_Time = 0;

/*------------------------------------------------------------------------------------------------------------------------*/

static uint8_t iot_tcp_connect_host(const char ipaddr[4], uint16_t port, int *sock_fd, int *sys_errno); 
static uint32_t iot_sync_host_time(int *sock, const char *iot_type, uint64_t iot_sn, int *sys_errno);
static uint64_t iot_login_password(uint32_t now_time, const char *iot_type, uint64_t iot_sn, uint64_t safetyCode);
static uint8_t iot_mqtt_login(login_info_t *login);

static void pack_data_head(uint8_t ver, uint8_t *head, uint8_t cause, uint16_t cycel, uint8_t total, uint8_t seq);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void Mqtt_Announce_Info_Clean(void);

/*------------------------------------------------------------------------------------------------------------------------*/

typedef struct {
    uint8_t need_bind;//1-CAN设备变化，需要绑定；0-和服务器TCP绑定交互完成
    uint8_t dev_nums;
    inv_info_t invs_list[BIND_LIST_MAX_NUM];
    bool  tcp_bind_flag;
} bind_dev_list_t;

typedef struct {
    uint8_t *data;
    int data_len;
} Mqtt_Active_Report_Queue_Struct;

USE_EXT_RAM_BSS static bind_dev_list_t bind_dev_list = {0};//绑定列表

/*------------------------------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------
*@Function： iot_bind_dump
仅打印，无功能
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
static void iot_bind_dump(void)
{
#if 0
	ESP_LOGI(TAG, "---------device bind dump---------");
	bind_dev_list_t *bind = &bind_dev_list;
	for (int i = 0; i <bind->dev_nums; i++) 
	{
		ESP_LOGI(TAG, "bind type: %s, sn: %lld, index: %d",
			bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn, bind->invs_list[i].index);
	}
	ESP_LOGI(TAG, "----------------------------------");
#endif    
}

static uint8_t iot_bind_update(char *type, uint64_t dev_sn, uint8_t idx)
{
	bind_dev_list_t *bind = &bind_dev_list;
    
    if ( bind->dev_nums == 0 ) {
        memset(bind->invs_list, 0, sizeof(bind->invs_list));
    }
        
	for(int i = 0; i < bind->dev_nums; i++)
	{
		/* 索引不同继续轮询下一个绑定的设备 */
		if (bind->invs_list[i].index != idx) {
			continue;
		}

		/* 索引相同设备类型与sn一致则绑定关系不需要改变直接退出*/
		if ((strcmp(type, bind->invs_list[i].dev_type) == 0) && (dev_sn == bind->invs_list[i].dev_sn)){
			return 0;
		}

		/* 索引相同但设备类型或sn不一致则需要重新绑定 */
		strcpy(bind->invs_list[i].dev_type, type);
		bind->invs_list[i].dev_sn = dev_sn;
		bind->invs_list[i].index = idx;
		return 1;
	}

	/* 将新的设备加入到绑定列表中 */
	strncpy(bind->invs_list[bind->dev_nums].dev_type, type, TYPE_SIZE);
	bind->invs_list[bind->dev_nums].dev_sn = dev_sn;
	bind->invs_list[bind->dev_nums].index = idx;
	bind->dev_nums++;
    return 1;
}

/*------------------------------------------------------------------------------
 Function: iot_bind_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      绑定检查
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_bind_check(void)
{
    uint8_t bind_need_flag = 0;
    static uint8_t pack_bind_num = 0;
    static uint8_t dchub_bind_num = 0;
    static uint8_t dcdc_bind_num = 0;

    // 检查设备数量，变动时更新上报
    if ( pack_bind_num != reals.online_Pack_num ) {
        pack_bind_num = reals.online_Pack_num;
        bind_dev_list.dev_nums = 0;
    }
#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE    
    if ( dchub_bind_num != reals.online_DCHUB_num ) {
        dchub_bind_num = reals.online_DCHUB_num;
        bind_dev_list.dev_nums = 0;
    }
#endif 
#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE    
    if ( dcdc_bind_num != reals.online_Dcdc_num ) {
        dcdc_bind_num = reals.online_Dcdc_num;
        bind_dev_list.dev_nums = 0;
    }
#endif 

    // 更新逆变信息
    if ((dev_factory.dev_sn > 0) && (strlen(dev_factory.dev_type) != 0)) {
        bind_need_flag |= iot_bind_update((const char *)dev_factory.dev_type, dev_factory.dev_sn, MODBUS_TATOL_ADDR); 
    } else {
        ESP_LOGE(TAG, "iot_bind_check: IoT_Factory Invalid!");
    }   	

    // 更新电池信息
#ifdef INTERNAL_PACK_ENABLE
    for ( int i = 1 ; i < PACK_MAX_NUM ; i++ ) // 忽略0号主包
#else
    for ( int i = 0 ; i < PACK_MAX_NUM ; i++ ) 
#endif
    {
        char *dev_type = NULL;
        uint64_t sn_code = 0;
        uint8_t online = 0;

#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
        bat_data_t *bms_data = get_bat_data_ptr();
        online = bms_data->bms_data[i].valid;
        if (online) {
            dev_type = bms_data->bms_data[i].type;
            sn_code = bms_data->bms_data[i].sn_code;
        }
#endif

#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE
        online = can_node_rd.Pack[i].online;
        if (online) {
            dev_type = can_node_rd.Pack[i].pack_about.type_ascii;
            sn_code = can_node_rd.Pack[i].pack_about.sn_code;
        }
#endif
        if ( dev_type ) {
            sn_code = (sn_code > 0) ? sn_code : dev_factory.dev_sn;
            bind_need_flag |= iot_bind_update((const char *)dev_type, sn_code, (i + MD_PACK_ADDR_START)); 
        }
    }

    // 更新DCHUB信息
#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    for ( int i = 0 ; i < DC_HUB_MAX_NUM ; i++ )
    {
        if((can_node_rd.DCHUB[i].online)
            && (0 != top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_sn)//非0，有节点内容
            )
        {
            bind_need_flag |= iot_bind_update(SN_TYPE_DCHUB_ASCII, top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_sn, (i + MD_DCHUB_ADDR_START)); 
        }
    }
#endif

    // 更新DCDC信息
#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
    for ( int i = 0 ; i < DCDC_MAX_NUM ; i++ )
    {
        if((can_node_rd.Dcdc[i].online)
            && (top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn > 0xFF)
            && (0 != strlen(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type))
            )
        {
            bind_need_flag |= iot_bind_update(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, 
                                        top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn, (i + MD_DCDC_ADDR_START)); 
        }
    }
#endif

    bind_dev_list.need_bind |= bind_need_flag;
}

/*------------------------------------------------------------------------
*@Function： mqtt_bind_dev
发送mqtt 报文：设备绑定信息
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
static void mqtt_bind_dev(void)
{
	int total_len;
	int bind_len;
	bind_dev_list_t *bind = &bind_dev_list;							// 获取绑定信息

	if (bind->need_bind == 0) return;
	if (bind->dev_nums == 0) return;
	if (mqtt_client_dump.transport == NULL) return;
    if ( bind->tcp_bind_flag == false ) return;

    //仅打印
    iot_bind_dump();
	
	char *buff = (char *)iot_calloc(sizeof(uint8_t) * 1024);
	if (buff == NULL)
	{
		ESP_LOGE(TAG, "malloc failed for mqtt bind device");
		return;
	}
	char *p_data = &buff[12];					// 指针跳过协议头
	p_data[0] = '\0';												// 清除缓存
	 
	/*产品SN绑定关系的16进制的ASCII字符。产品SN绑定格式为：
        {产品型号}&{产品SN},{产品型号}&{产品SN},{产品型号}&{产品SN}
        举例：D100P&12345678,A80&12345678,A80P&97654321      */
        
//	sprintf(p_data, "%s&%llu@%d", bind->invs_list[0].dev_type, bind->invs_list[0].dev_sn, bind->invs_list[0].index);
    sprintf(p_data, "%s&%llu", bind->invs_list[0].dev_type, bind->invs_list[0].dev_sn);
	for (int i = 1; i < bind->dev_nums; i++) 
	{
//		sprintf(&p_data[strlen(p_data)], ",%s&%llu@%d", bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn, bind->invs_list[i].index);
        sprintf(&p_data[strlen(p_data)], ",%s&%llu", bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn);
	}
	ESP_LOGW(TAG, "device bind to server: %s ",p_data);

	/* 设置绑定协议头 */
    bind_len = strlen(p_data);
	total_len = bind_len + 12;
    buff[0] = 0x0A; // 0x0A(10)： 表示整体更新绑定产品SN业务（2024/08/18新增）
    buff[1] = 0xF8;
    buff[2] = PUB_EVEVT;//0x02表示关键指标数据上报或控制
	buff[3] = total_len >> 8;
	buff[4] = total_len;
    buff[5] = 0;
    buff[6] = 0;
    buff[7] = 0;
    buff[8] = 0;
    buff[9] = 0;

	buff[10] = 0x01;//0x01：从机地址（地址域）
	buff[11] = 0x49;//0x49(73)：功能码

    uint16_t crc = ModbusCrc16((uint8_t *)buff,total_len);
    
    buff[total_len++] = (unsigned char) crc;
    buff[total_len++] = (unsigned char)(crc>>8);

    ESP_LOGI(TAG, "mqtt send bind data to server");
    if(iot_mqtt_send((const uint8_t *)buff, total_len) < 0)
    {
        ESP_LOGE(TAG, "mqtt bind device failed, errno=%d, %s", errno, strerror(errno));
        goto __exit;
    }
    else
    {
        ESP_LOGW(TAG, "mqtt bind device success");
        bind->need_bind = 0;                                // 绑定成功清除绑定标志
        free(buff);                                         // 释放缓存
        return;
    }

__exit:

    ESP_LOGE(TAG, "mqtt bind device failed, send data:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, total_len, ESP_LOG_WARN);

	free(buff);														// 释放缓存
}

/*------------------------------------------------------------------------
*@Function： tcp_bind_dev
发送TCP 报文：设备绑定信息
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
static void tcp_bind_dev(void)
{
	int len;
	int total_len;
	int bind_len;
	bind_dev_list_t *bind = &bind_dev_list;							// 获取绑定信息

	if (bind->need_bind == 0) return;
	if (bind->dev_nums == 0) return;
	if (mqtt_client_dump.transport == NULL) return;
    if ( bind->tcp_bind_flag == true ) return mqtt_bind_dev();

    //仅打印
    iot_bind_dump();
	
	char *buff = (char *)iot_calloc(sizeof(uint8_t) * 1024);
	if (buff == NULL)
	{
		ESP_LOGE(TAG, "malloc failed for mqtt bind device");
		return;
	}
	char *p_data = &buff[NO_STD_PROTOCOL_HEADER];					// 指针跳过协议头
	p_data[0] = '\0';												// 清除缓存

	/* 按照协议建立绑定关系
	 * 参考文档：http://doc.poweroak.ltd/docs/bluetti-cloud-analysis/bluetti-cloud-analysis-1dqah2v384ps0 */
	sprintf(p_data, "%s&%llu@%d", bind->invs_list[0].dev_type, bind->invs_list[0].dev_sn, bind->invs_list[0].index);
	for (int i = 1; i < bind->dev_nums; i++) 
	{
		sprintf(&p_data[strlen(p_data)], ",%s&%llu@%d", bind->invs_list[i].dev_type, bind->invs_list[i].dev_sn, bind->invs_list[i].index);
	}
	ESP_LOGW(TAG, "device bind to server: %s ",p_data);

	/* 设置绑定协议头 */
	bind_len = strlen(p_data);
	buff[0] = 0x00;
	buff[1] = 0x02;
	buff[2] = bind_len >> 8;
	buff[3] = bind_len;

	/* TCP发送绑定数据到服务器 */
	total_len = bind_len + NO_STD_PROTOCOL_HEADER;
	

	while (total_len > 0)
	{
		len = esp_transport_write(mqtt_client_dump.transport, (char *)buff, total_len, ESP_TRANS_TIMEOUT);
		if (len < 0)
		{
			ESP_LOGE(TAG, "esp_transport_write failed, errno=%d, %s", errno, strerror(errno));
			goto __exit;
		}
		else if (len == 0)
		{
			ESP_LOGE(TAG, "esp_transport_write timeout, errno=%d, %s", errno, strerror(errno));
			goto __exit;
		}
		total_len -= len;

//        ESP_LOGW(TAG, "esp_transport_write   cmdLen = %d", len);
//        ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, len, ESP_LOG_WARN);
	}
	
	/* 获取绑定响应数据 */
	ESP_LOGI(TAG, "tcp read bind reply from server");
	len = esp_transport_read(mqtt_client_dump.transport, (char *)buff, 8, ESP_TRANS_TIMEOUT);
	if (len < 0)
	{
		ESP_LOGE(TAG, "esp_transport_read error, errno=%d, %s", errno, strerror(errno));
		goto __exit;
	}
	else if (len == 0)
	{
		ESP_LOGE(TAG, "esp_transport_read nothing or EOF");
		goto __exit;
	}
    
//    ESP_LOGW(TAG, "esp_transport_read   cmdLen = %d", len);
//    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, len, ESP_LOG_WARN);


	/* 解析绑定响应数据 */
	if (buff[0] == 0x00 && buff[1] == 0x02)
	{
		len = ((int)buff[2] << 8) | buff[3];
		if (len == 4)
		{
			int result = *(int*)&buff[4];
			if (result == 0)
			{
				ESP_LOGI(TAG, "tcp bind device success");
				bind->need_bind = 0;								// 绑定成功清除绑定标志
				bind->tcp_bind_flag = true;
				free(buff);											// 释放缓存
				return;
			}
		}
	}
	ESP_LOGE(TAG, "tcp bind device failed, server respond data:");
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, buff, len, ESP_LOG_WARN);

__exit:
	free(buff);														// 释放缓存
}

static int thunder_mode_handle(uint8_t *buff, int len)
{
	int ret = 0;

    if ( buff == NULL ) return 1;
	uint8_t *pdata = buff;

	/* 非风暴模式指令直接退出 */
	if (!((pdata[0] == 0x00) && (pdata[1] == 0x04))) return 1;

#if CONFIG_DISASTER_WARN_CHANNEL == 2  

	/* 解析风暴指令json数据 */
    _SaveWebStormWatchApiData thunder_param = {0};
    if(parse_thunderstorm_alert((const char*)&pdata[NO_STD_PROTOCOL_HEADER], &thunder_param) != 0)
    {
        ESP_LOGW(TAG, "parse_thunderstorm_alert error");
        return -1;
    }

	ESP_LOGW(TAG, "thunder message: %02x %02x %02x %02x\n%s\n", pdata[0], pdata[1], pdata[2], pdata[3], &pdata[4]);
	thunder_param.StartTime += thunder_param.TimeZoneOffset;
	thunder_param.EndTime += thunder_param.TimeZoneOffset;

	SetStormWebData(&thunder_param, thunder_param.TimeZoneOffset, thunder_param.msg_id);		// 向EMS设置灾害告警参数
    if (ret == 0)
    {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.thunder_cancel = 0;
        top_modbus_wr.Inv.mod_reg12000_IOT_set.thunder_ctrl.thunder_cancel = 0;
    }

#elif CONFIG_DISASTER_WARN_CHANNEL == 1

    // 灾害预警响应
    ret = disater_warn_mode_handle(buff, len);

#else

    //不支持灾害预警
    ret = -1;

#endif

	return ret;
}


/*------------------------------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 Function: timeout_timer_cb
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT订阅响应超时回调
  * @param[in]  TimerHandle_t xTimer  
  * @param[out] None
  * @return     static void
  */
static void mqtt_sub_timeout_timer_cb(TimerHandle_t xTimer)
{
    memset(&mqtt_client_dump, 0x00, sizeof(struct esp_mqtt_client_dump));
    login_info.mqtt_is_login = 0;
    Mqtt_Machine_Pause_Time = 5 * 1000;
    if(login_step != STEP_MQTT_WAITING) login_step = STEP_MQTT_RESTART;  
    ESP_LOGE(TAG, "timeout_timer_cb : mqtt sub failed!");

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
    // MQTT日志生成
    mqtt_conn_fail_log_t record = {
        .svc_timestamp = login_info.now_time,
        .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
        .password = login_info.password,
        .reason = MQTT_FAIL_SUBSCRIBE,
        .network_type = login_info.current_netif_id,
        .retry_delay_ms = (Mqtt_Machine_Pause_Time < UINT16_MAX) ? Mqtt_Machine_Pause_Time : UINT16_MAX,
    };
    Mqtt_Log_Generate(&record);
#endif
}

/*------------------------------------------------------------------------------
 Function: mqtt_sub_timeout_timer_start
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT订阅响应超时
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void mqtt_sub_timeout_timer_start(void)
{
	/* MQTT订阅响应超时时间定义 */
	#define MQTT_SUB_RESP_TIMEOUT			10000

	if (mqtt_sub_timeout_tmr)
	{
		xTimerStart(mqtt_sub_timeout_tmr, 0);
		return;
	}

	/* 创建通信超时定时器 */
	mqtt_sub_timeout_tmr = xTimerCreate("mqtt sub timer", pdMS_TO_TICKS(MQTT_SUB_RESP_TIMEOUT), pdFALSE, NULL, mqtt_sub_timeout_timer_cb);
	if (mqtt_sub_timeout_tmr == NULL)
	{
		ESP_LOGE(TAG, "mqtt subscribe timeout timer create failed, Restarting");
		esp_restart();
		return;
	}
	xTimerStart(mqtt_sub_timeout_tmr, 0);
}

/*------------------------------------------------------------------------------
 Function: mqtt_sub_timeout_timer_stop
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT订阅响应超时停止
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void mqtt_sub_timeout_timer_stop(void)
{
    /*MQTT订阅响应成功*/
	if (mqtt_sub_timeout_tmr)
		xTimerStop(mqtt_sub_timeout_tmr, 0);
}

/**
 * @brief 接收超时定时器回调函数
 * - 当接收到超过上限大小的数据后，设置定时器等待下一帧数据超时，超时后将接收到的数据发送到接收队列中
 * 
 * @param xTimer 定时器句柄
 */
static void mqtt_recv_timeout_timer_cb(TimerHandle_t xTimer)
{
    queue_msg_t mqtt_msg;
    mqtt_msg.msg_len = mqtt_recv_len;
    mqtt_msg.msg_ptr = mqtt_recv_buf;    
    if (!xQueue_Mqtt_Recv || xQueueSendToBack(xQueue_Mqtt_Recv, &mqtt_msg, pdMS_TO_TICKS(100)) != pdPASS) {
        free(mqtt_msg.msg_ptr);
        ESP_LOGE(TAG, "mqtt message handle queue full"); // MQTT发送过来的消息太多,就会处理不及时队列满
    }
    mqtt_recv_buf = NULL;
    mqtt_recv_len = 0;
}

/*------------------------------------------------------------------------------
 Function: mqtt_recv_callback
 -----------------------------------------------------------------------------*/
/**
  * @brief      Mqtt接收回调
  * @param[in]  QueueHandle_t *revd_queue  
                uint8_t *data              
                uint8_t len                
  * @param[out] None
  * @return     static void
  */
static void mqtt_recv_callback(QueueHandle_t revd_queue, uint8_t *data, uint16_t len)
{
    /*参数检查*/
    if ( !revd_queue || !data || 0 == len ) {
        ESP_LOGE(TAG, "mqtt recv msg malloc failed");
        return;
    }
    
    /* 申请接收缓存 */
    if ( !mqtt_recv_buf ) {
        if (len < (CONFIG_MQTT_BUFFER_SIZE - 29)) {
            /*无需拼包，仅申请当前空间*/
            mqtt_recv_buf = (uint8_t *)iot_calloc(len);
        } else {
            /*需要拼包，申请最大缓存*/
            mqtt_recv_buf = (uint8_t *)iot_calloc(MQTT_RECV_MAX_LEN);
        }

        if (mqtt_recv_buf == NULL) {
            ESP_LOGE(TAG, "mqtt recv msg malloc failed");
            return;
        }
        mqtt_recv_len = 0;
    }
    
    // 拷贝数据
    uint16_t copy_len = ((mqtt_recv_len + len) >= MQTT_RECV_MAX_LEN ) ? (MQTT_RECV_MAX_LEN - mqtt_recv_len) : len;
    memcpy(mqtt_recv_buf + mqtt_recv_len, data, copy_len);
    mqtt_recv_len += copy_len;
    
    /* 单次长度小于最大长度认为数据接收完成，总长度大于接收最大值认为数据接收完成*/
    if ((len < (CONFIG_MQTT_BUFFER_SIZE - 29)) || ((mqtt_recv_len + len) >= MQTT_RECV_MAX_LEN ))
    {        
        queue_msg_t mqtt_msg;
        mqtt_msg.msg_len = mqtt_recv_len;
        mqtt_msg.msg_ptr = mqtt_recv_buf;    
        xQueue_Mqtt_Recv = revd_queue;
        if (!xQueue_Mqtt_Recv || xQueueSendToBack(xQueue_Mqtt_Recv, &mqtt_msg, pdMS_TO_TICKS(100)) != pdPASS) {
            free(mqtt_msg.msg_ptr);
            ESP_LOGE(TAG, "mqtt message handle queue full"); // MQTT发送过来的消息太多,就会处理不及时队列满
        }
        mqtt_recv_buf = NULL;
        mqtt_recv_len = 0;
        xTimerStop(mqtt_recv_timeout_tmr, portMAX_DELAY);
    }
    else
    {
        /* 设置等待下一帧数据超时时间 */
        xTimerChangePeriod(mqtt_recv_timeout_tmr, pdMS_TO_TICKS(100), portMAX_DELAY); // 调用此函数后定时器会立即运行
        xQueue_Mqtt_Recv = revd_queue;
    }
}

static void log_error_if_nonzero(const char *message, int error_code) //static
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_log_error_details(esp_mqtt_event_handle_t event)
{
    if (event == NULL || event->error_handle == NULL) {
        ESP_LOGE(TAG, "mqtt error_handle is NULL");
        return;
    }

    switch (event->error_handle->error_type)
    {
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            ESP_LOGE(TAG, "MQTT transport error");

            log_error_if_nonzero("reported from esp-tls",
                                 event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack",
                                 event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",
                                 event->error_handle->esp_transport_sock_errno);
            Mqtt_Machine_Pause_Time = 10 * 1000;
            break;

        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
            switch (event->error_handle->connect_return_code)
            {
                case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
                    ESP_LOGE(TAG, "MQTT connection refused: bad username");
                    Mqtt_Machine_Pause_Time = 2 * 60 * 1000;
                    break;

                case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
                    ESP_LOGE(TAG, "MQTT connection refused: not authorized / bad password");
                    Mqtt_Machine_Pause_Time = 2 * 60 * 1000;
                    break;

                case MQTT_CONNECTION_REFUSE_PROTOCOL:
                    ESP_LOGE(TAG, "MQTT connection refused: protocol error");
                    Mqtt_Machine_Pause_Time = 30 * 1000;
                    break;

                case MQTT_CONNECTION_REFUSE_ID_REJECTED:
                    ESP_LOGE(TAG, "MQTT connection refused: client id rejected");
                    Mqtt_Machine_Pause_Time = 30 * 1000;
                    break;

                case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
                    ESP_LOGE(TAG, "MQTT connection refused: server unavailable");
                    Mqtt_Machine_Pause_Time = 30 * 1000;
                    break;

                default:
                    ESP_LOGE(TAG, "MQTT connection refused: code=%d",
                             event->error_handle->connect_return_code);
                    Mqtt_Machine_Pause_Time = 30 * 1000;
                    break;
            }
            break;

        case MQTT_ERROR_TYPE_SUBSCRIBE_FAILED:
            ESP_LOGE(TAG, "MQTT subscribe failed");
            Mqtt_Machine_Pause_Time = 10 * 1000;
            break;

        default:
            ESP_LOGE(TAG, "MQTT unknown error_type=%d, transport_sock_errno=%d, tls_stack_err=%d",
             event->error_handle->error_type,
             event->error_handle->esp_transport_sock_errno,
             event->error_handle->esp_tls_stack_err);
            Mqtt_Machine_Pause_Time = 10 * 1000;
            break;
    }
}

#ifdef CONFIG_MQTT1_TCP_ENCRYPT_ENABLE	

static USE_EXT_RAM_BSS char http_output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
static USE_EXT_RAM_BSS char serial_data[33] = {0};//留出空字符的位置
static char *download_buffer = NULL;
static uint8_t serial_flag = 0;//请求新的证书序列号成功标志位

//json数据解析函数
static int16_t json_get_down_cert_info(char *text)
{
    int16_t success=0;

    //截取有效json
    char *index = strchr(text,'{');
    if (NULL == index) {
        ESP_LOGE(TAG, " json_get_down_cert_info text error:%s", text);
        return 0;
    }

    strcpy(text,index);
    ESP_LOGD(TAG,"text=%s",text);
    cJSON *root = cJSON_Parse(text);

    uint32_t serial=0;
    char serial_number[33];//32 testwx 空字符在33位
    if(root != NULL)
    {
        char *sys_info = cJSON_Print(root);  
        ESP_LOGD(TAG,"sys_info:%s",sys_info);

        int msgCode = cJSON_GetObjectItem(root, "msgCode")->valueint;
        cJSON* string = cJSON_GetObjectItemCaseSensitive(root, "data");//cJSON_GetObjectItem          
        if(NULL == string)
        {
            ESP_LOGE(TAG, "json not get data");
            goto exit;
        }

        const char *str_value = cJSON_GetStringValue(string); // 获取字符串值
        if(NULL == str_value)
        {
            ESP_LOGE(TAG, "json not get data value");
            goto exit;
        }

        size_t str_len = strnlen(str_value, sizeof(serial_number) - 1); // 使用strnlen确保不会读取超过serial_number大小 

        if (str_len < sizeof(serial_number)) 
        {  
            // 安全地复制字符串到serial_number数组  
            strncpy(serial_number, str_value, sizeof(serial_number) - 1); // 减去1是为了给字符串末尾的空字符'\0'留出空间  
            serial_number[str_len] = '\0'; // 确保字符串以空字符结尾  
            ESP_LOGI(TAG,"Serial number: %s", serial_number);  
        } 
        else 
        {  
            // 字符串太长，无法放入serial_number数组  
            ESP_LOGE(TAG, "Error: Serial number is too long to fit in the array.");  
        }

        if(msgCode == 0)
        {
            memcpy(serial_data, serial_number,sizeof(serial_number));
            serial_flag = 1;
            success = 1;
        }
        else
        {
            serial_flag = 0;
        }
    }
exit:
    if (NULL != root)
    {
        cJSON_Delete(root);
        root = NULL;
    }
    return success;
}


//json数据解析函数
static int16_t cjson_to_serial_info(char *text)
{
    int16_t success=0;
    //截取有效json
    char *index=strchr(text,'{');
    strcpy(text,index);
    ESP_LOGD(TAG,"text=%s",text);
    cJSON *root = cJSON_Parse(text);

    uint32_t serial=0;
    char serial_number[33];//32 testwx 空字符在33位
    if(root != NULL)
    {
        char *sys_info = cJSON_Print(root);  
        ESP_LOGD(TAG,"sys_info:%s",sys_info);

        int msgCode = cJSON_GetObjectItem(root, "msgCode")->valueint;
        cJSON* string = cJSON_GetObjectItemCaseSensitive(root, "data");//cJSON_GetObjectItem          
        if(NULL == string)
        {
            ESP_LOGE(TAG, "json not get data");
            goto exit;
        }

        const char *str_value = cJSON_GetStringValue(string); // 获取字符串值
        if(NULL == str_value)
        {
            ESP_LOGE(TAG, "json not get data value");
            goto exit;
        }

        size_t str_len = strnlen(str_value, sizeof(serial_number) - 1); // 使用strnlen确保不会读取超过serial_number大小 

        if (str_len < sizeof(serial_number)) 
        {  
            // 安全地复制字符串到serial_number数组  
            strncpy(serial_number, str_value, sizeof(serial_number) - 1); // 减去1是为了给字符串末尾的空字符'\0'留出空间  
            serial_number[str_len] = '\0'; // 确保字符串以空字符结尾  
            ESP_LOGD(TAG,"Serial number: %s\n", serial_number);  
        } 
        else 
        {  
            // 字符串太长，无法放入serial_number数组  
            ESP_LOGE(TAG, "Error: Serial number is too long to fit in the array.");  
        }  
        ESP_LOGD(TAG,"msgCode:%d,data:%s",msgCode,serial_number);// testwx

        if(msgCode == 0)
        {
            memcpy(serial_data,serial_number,sizeof(serial_number));
            serial_flag = 1;
            success = 1;
        }
        else
        {
            serial_flag = 0;
        }
    }
exit:
    if (NULL != root)
    {
        cJSON_Delete(root);
        root = NULL;
    }
    return success;
}

/**
 * @brief 检测当前IoT的证书是否存在
 * @param login_info MQTTS登录信息
 * @return 0:不存在(需要下载)，1:存在
 */
static uint32_t http_iot_cert_exist(login_info_t login_info)
{
    if (login_info.iot_sn == 0) {
        ESP_LOGE(TAG,"http_iot_cert_exist error");
        return 0;
    }

    if (NULL == iot_cert_ptr || 0 == iot_cert_ptr_len || NULL == private_key_ptr || 0 == private_key_ptr_len) {
        ESP_LOGW(TAG, "mqtts cert is null, need to download");
        return 0;
    }

    int16_t len=0;
    int16_t success = 0;

    /***** 密文提取 *****/
    char sys_info_all[33] = {0};// 64 32
    sprintf(sys_info_all,"%s%013llu", login_info.iot_type, login_info.iot_sn);//sys_info
    sys_info_all[32] = '\0';//防止越界

    int16_t sys_info_len = strlen(sys_info_all);//sys_info
    ESP_LOGD(TAG,"sys_info_all:%s, sys_info_len=%d", sys_info_all, sys_info_len);//sys_info

    /* 提取证书CommonName, 与当前MQTT登录信息对比，不一致则需要下载 */
    char common_name[33] = {0};
    len = cer_ras_get_common_name(common_name, sizeof(common_name));
    if(!len) {
        ESP_LOGE(TAG,"cer_ras_get_common_name failed!");
        return 0;
    }
    else
    {
        common_name[len]='\0';//防止越界
        len++;
        char common_name_str[33]={0};//32 100
        sprintf(common_name_str,"%s",common_name);
        common_name_str[32] = '\0';//防止越界
        ESP_LOGD(TAG,"common_name len=%d, strlen(common_name_str):%d, common_name_str:%s", len, strlen(common_name_str), common_name_str);
        len = (len > 0) ? len:15;

        /* 检测当前证书是否为当前MQTTS登录所使用的IOT */
        if (sys_info_len == strlen(common_name_str) && 0 == memcmp(sys_info_all, common_name, sys_info_len))
        {
            success = 1;
        }
    }

    return success;
}


//获取证书序列号
static uint32_t http_get_cert_sn(login_info_t login_info)
{
    if ((login_info.iot_sn == 0)) {
        ESP_LOGE(TAG,"http_get_cert_sn error");
        return 0;
    }

    int16_t len = 0;
    int16_t success = 0;

    int content_length = 0;

    //http请求包
    static const char *REQUEST = CERT_HTTP_URL_FIRST;

    // 初始化HTTP客户端
    esp_http_client_config_t config = {
        .url = REQUEST,
        .method = HTTP_METHOD_POST,
    };
    // 创建HTTP客户端实例
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG,"Failed to initialize HTTP client\n");
        goto exit;
    }
    
    // 设置通用请求头
    if(set_http_header(client) != 0) {
        ESP_LOGE(TAG, "Failed to set HTTP headers");
        goto exit;
    }

    esp_http_client_set_url(client, REQUEST);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_authtype(client,HTTP_AUTH_TYPE_NONE);

    esp_err_t err = esp_http_client_open(client, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        int wlen = esp_http_client_write(client, NULL, 0);
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write (esp_http_client_write) failed");
        }
        content_length = esp_http_client_fetch_headers(client);
        if(content_length < 0) 
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        }
        else 
        {
            memset(http_output_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
            int data_read = esp_http_client_read_response(client, http_output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGD(TAG, "http get cert sn:%s", http_output_buffer);
            
                success = json_get_down_cert_info(http_output_buffer);
                ESP_LOGI(TAG,"Success to json_get_down_cert_info.");
          
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }

    if(client != NULL)
    {
        esp_http_client_cleanup(client); 
    }
exit:

    return success;
}


//请求证书更新获取新序列号
static uint32_t http_iot_update_cert(login_info_t login_info) 
{
    if (login_info.iot_sn == 0) {
        ESP_LOGE(TAG,"http_iot_update_cert error");
        return 0;
    }

    int16_t ret = 0;
    int16_t len = 0;
    int16_t success = 0;
    char *post_data=NULL;

    /***** 密文提取 *****/
    char sys_info_all[118]={0};// 64 32
    sprintf(sys_info_all,"%s%013llu,%llu", login_info.iot_type, login_info.iot_sn, login_info.safetyCode);//sys_info
    sys_info_all[117] = '\0';//防止越界

    int16_t sys_info_len = strlen(sys_info_all);//sys_info
    ESP_LOGD(TAG,"sys_info_all:%s,sys_info_len=%d",sys_info_all,sys_info_len);//sys_info

    //使用设备的X.509公钥证书对设备身份信息deviceIdentity 进行加密，得到加密后的设备身份信息ciphertext
    unsigned char plaintext[118]= {0};// A103 = {0} 117 200 32
	unsigned char *ciphertext = NULL;
    ciphertext = (unsigned char*)iot_calloc(471);
	
    if(ciphertext == NULL)
    {
        mbedtls_printf( "ciphertext calloc faild\n");
        return 0;
    }

    memcpy(plaintext, sys_info_all, sys_info_len);//M msg //sys_info
    plaintext[sys_info_len] = '\0';//防止越界  +1
    if((sys_info_len > 117) || (sys_info_len < 0))
    {
        perror("msg length must be less than 116 bytes!\n");
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }
        return 0;
    }

    ret = cer_ras_encrypt(plaintext,sys_info_len,ciphertext);//M904503 sys_info

    memset(plaintext,0,sizeof(plaintext));//testwx 对原明文区域初始化重新赋初值
    memcpy(plaintext, sys_info_all, sys_info_len);//M msg //sys_info

    if(ret == 0)
    {
        ESP_LOGE(TAG,"priv_rsa_sign failed");
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }

        return 0;
    }
    ESP_LOGD(TAG,"ciphertext:%s",ciphertext);
    /***** 密文提取 *****/


    /***** 数字签名提取 *****/
    //使用设备的私钥对设备身份信息deviceIdentity 进行数字签名，得到设备身份信息的签名指纹signature
	unsigned char *signature = NULL;
    signature = (unsigned char*)iot_calloc(471);
    if(signature == NULL)
    {
        ESP_LOGE(TAG,"signature calloc faild");
        goto exit;
    }

    ret = priv_rsa_sign(plaintext, sys_info_len, signature);
    if(ret == 0)
    {
        ESP_LOGE(TAG,"priv_rsa_sign failed");
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }
        if(signature != NULL)
        {
            free(signature);
        }

        return 0;
    }
    ESP_LOGD(TAG,"signature:%s",signature);
    /***** 数字签名提取 *****/


    /***** 序列号提取 *****/
    //已过期的X509数字证书的序列号
    char x509CertSn[33];//32
    len = cer_ras_get_serial_number(x509CertSn);
    
    if(!len)
    {
        ESP_LOGE(TAG,"cer_ras_get_serial_number failed");
        if(ciphertext != NULL)
        {
            free(ciphertext);
        }
        if(signature != NULL)
        {
            free(signature);
        }

        return 0;
    }
    else
    {
        x509CertSn[len]='\0';//防止越界
        len++;
        char x509CertSn_str[33]={0};//32 100
        sprintf(x509CertSn_str,"%s",x509CertSn);
        x509CertSn_str[32] = '\0';//防止越界
        ESP_LOGD(TAG,"cer_ras_get_serial_number:len=%d,x509CertSn_str:%s",len,x509CertSn_str);
        len = (len > 0) ? len:15; //9
    }
    /***** 序列号提取 *****/
    

    /***** POST Request body data *****/
    post_data = (char*)iot_calloc(1024);
    
    if(post_data == NULL)
    {
        ESP_LOGE(TAG,"post_data calloc faild");
        goto exit;
    }

    if((ciphertext != NULL)&&(signature != NULL))
    {
        len = sprintf(post_data,"{\"x509CertSn\":\"%s\",\"ciphertext\":\"%s\",\"signature\":\"%s\"}", x509CertSn,ciphertext,signature);//035cf17fc6802000 035CF17FC6802000
        free(ciphertext);
        free(signature);
        ciphertext = NULL;
        signature = NULL;
    }
    else
    {
        ESP_LOGE(TAG,"ciphertext or signature calloc failed");
        goto exit;
    }
    
    if(len < 0)
    {
        ESP_LOGW(TAG,"Error len=%d",len);
        goto exit;
    }
    
    /***** POST Request body data *****/

    int content_length = 0;

    //http请求包
    static const char *REQUEST = CERT_HTTP_URL;

    // 初始化HTTP客户端
    esp_http_client_config_t config = {
        .url = REQUEST,
        .method = HTTP_METHOD_POST,
    };
    // 创建HTTP客户端实例
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG,"Failed to initialize HTTP client\n");
        goto exit;
    }
    
    // 设置通用请求头
    if(set_http_header(client) != 0) {
        ESP_LOGE(TAG, "Failed to set HTTP headers");
        goto exit;
    }

    esp_http_client_set_url(client, REQUEST);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_authtype(client,HTTP_AUTH_TYPE_NONE);

    ESP_LOGD(TAG,"http_iot_update_cert:len:%d",len);
    esp_err_t err = esp_http_client_open(client, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGD(TAG,"1_post_data_update:%s, len:%d",post_data, len);
        int wlen = esp_http_client_write(client, (const char *)post_data, len);//M strlen(post_data)
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write failed(%d)", wlen);
        }
        content_length = esp_http_client_fetch_headers(client);
        if(content_length < 0) 
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        }
        else 
        {
            memset(http_output_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
            int data_read = esp_http_client_read_response(client, http_output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGD(TAG, "http_iot_update_cert : %s", http_output_buffer);

                success = cjson_to_serial_info(http_output_buffer);
                ESP_LOGI(TAG,"Success to cjson_to_serial_info.");
          
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }

    if(client != NULL)
    {
        esp_http_client_cleanup(client); 
    }
exit:   
    if(ciphertext != NULL)
    {
        free(ciphertext);
    }
    if(signature != NULL)
    {
        free(signature);
    }
    if(post_data != NULL)
    {
        free(post_data);
    }

    return success;
}


//验证收到的证书是否为完整的PEM格式的函数 PKCS#8 标准定义
static uint8_t check_integrity(int16_t download_type, char *buffer, int buffer_len)
{
    uint8_t success = 0;
    if(download_type == 0)
    {
        //-----BEGIN PRIVATE KEY-----比较前27个开头格式的字符和末尾25个字符(不包含回车和空字符) 移动数组指针去比较末尾字符
        if((!memcmp("-----BEGIN PRIVATE KEY-----", buffer, 27)) && (!strncmp("-----END PRIVATE KEY-----", buffer + buffer_len - 25, 25)))
        {
            ESP_LOGI(TAG,"PRIVATE KEY check finished");
            success = 1;
        }
        else
        {
            ESP_LOGE(TAG,"PRIVATE KEY check wrong");
            success = 0;
        }
    }
    else
    {
        //-----BEGIN CERTIFICATE-----
        if((!memcmp("-----BEGIN CERTIFICATE-----", buffer, 27)) && (!strncmp("-----END CERTIFICATE-----", buffer + buffer_len - 25, 25)))
        {
            ESP_LOGI(TAG,"CERTIFICATE check finished");
            success = 2;
        }
        else
        {
            ESP_LOGE(TAG,"CERTIFICATE check wrong");
            success = 0;
        }
    }
    return success;
}

//下载新证书函数
static uint32_t http_iot_download_cert(login_info_t login_info) 
{
    if (login_info.iot_sn == 0) {
        ESP_LOGE(TAG,"http_iot_download_cert error");
        return 0;
    }

    int16_t len = 0;
    int16_t success = 0;
    int16_t download_file_type=0;//0-PrivateKey,1-Certificate
    char holder[33];
    char type[32];
    int content_length = 0;
    unsigned char *password = NULL;
    password = (unsigned char*)iot_calloc(471);
    
    if(password == NULL)
    {
        ESP_LOGE(TAG,"password calloc faild");
        goto exit;
    }    

    //使用SHA256加密算法对设备的SecurityCode进行加密后的结果
    unsigned char plaintext[33];// 117 
    char sys_info[33];// 128
    int16_t sys_info_len = sprintf(sys_info,"%llu",login_info.safetyCode);
    memcpy(plaintext, sys_info, sys_info_len);//M msg
    plaintext[sys_info_len] = '\0';//防止越界
    sys_info_len = sys_info_len+1;
    ESP_LOGD(TAG,"sys_info:%s,sys_info_len:%d",sys_info,sys_info_len);
    if((sys_info_len > 117) || (sys_info_len < 0))
    {
        perror("msg length must be less than 116 bytes!\n");
        goto exit;
    }
    cer_ras_encrypt(plaintext,sys_info_len,(unsigned char*)password);//M904503 sys_info 将安全码用公钥加密
    ESP_LOGD(TAG,"password:%s", password);

    //http请求包
    static const char *REQUEST = CERT_HTTP_DOWN_URL;

    // 初始化HTTP客户端
    esp_http_client_config_t config = {
        .url = REQUEST,//"/path/to/your/resource"
        .method = HTTP_METHOD_POST,
    };
    // 创建HTTP客户端实例
    esp_http_client_handle_t client = esp_http_client_init(&config);

download_again:
    sprintf(holder,"%s",serial_data);
    if(download_file_type == 0)
    {
        sprintf(type,"PrivateKey");
    }
    else
    {
        sprintf(type,"Certificate");
    }

    //POST Request
    char *post_data=NULL;
    post_data = (char*)iot_calloc(1024);
	
	if(post_data == NULL)
    {
        ESP_LOGE(TAG,"post_data calloc faild");
        goto exit;
    }
    
    len = sprintf(post_data,"{\"holder\":\"%s\",\"password\":\"%s\",\"type\":\"%s\"}", holder,password,type);
    ESP_LOGD(TAG,"post_data_download:%s, len:%d",post_data, len);

    if(len < 0)
    {
        ESP_LOGW(TAG,"Error len=%d",len);
        goto exit;
    }

    /***** POST Headers data *****/
    
    // 设置通用请求头
    if(set_http_header(client) != 0) {
        ESP_LOGE(TAG, "Failed to set HTTP headers");
        goto exit;
    }

    esp_http_client_set_url(client, REQUEST);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_authtype(client,HTTP_AUTH_TYPE_NONE);

    esp_err_t err = esp_http_client_open(client, len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        int wlen = esp_http_client_write(client, (const char *)post_data, len);//M strlen(post_data)
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write(%d) failed", wlen);
        }
        content_length = esp_http_client_fetch_headers(client);
        if(content_length < 0) 
        {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } 
        else 
        {
            download_buffer = (char*)iot_calloc(download_buffer_len);			
            if(download_buffer == NULL)
            {
                ESP_LOGE(TAG, "download_buffer calloc faild");
                goto exit;
            }

            memset(download_buffer, 0x00, download_buffer_len);
            int data_read = esp_http_client_read_response(client, download_buffer, download_buffer_len);
            if (data_read >= 0) 
            {
                ESP_LOGD(TAG, "download file type:%d, buffer data:%s", download_file_type, download_buffer);
                ESP_LOGD(TAG,"download_buffer:%s,download_buffer_len:%d,len:%lld",
                    download_buffer,download_buffer_len,esp_http_client_get_content_length(client));
                
                //验证收到的证书是否为完整的PEM格式的函数
                success = check_integrity(download_file_type,download_buffer,(esp_http_client_get_content_length(client) - 1));//-1有个空字符
                //要将公钥、私钥证书全部校验完后再存入
                //先写入私钥，再写入公钥
#if ENCRYPT_CERT_USE_FILE_SYSTEM
                if(success == 1)
                {
                    write_cert_to_file(PRIVATE_AREA, (uint8_t*)download_buffer, strlen(download_buffer));
                }
                else if(success == 2)
                {
                    write_cert_to_file(IOT_CERTIFICATE_AREA, (uint8_t*)download_buffer, strlen(download_buffer));
                }
#else
                partition_reinit(UPDATE_AREA, PARTITION_SIZE);
                if(success == 1)
                {
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, UPDATE_AREA);//先存入缓存区
                    /*此处最好再有一个检验证书的步骤 暂时从缓存区读*/
                    memset(download_buffer, 0, strlen(download_buffer));

                    uint32_t len = get_partition_plaintext_len(UPDATE_AREA); //获取长度
                    // partition_read_decrypt((uint8_t*)read_buffer, len, 0, UPDATE_AREA);
                    // partition_write_encrypt((uint8_t*)read_buffer, strlen(read_buffer), 0, PRIVATE_AREA);//覆盖原私钥区
                    partition_read_decrypt((uint8_t*)download_buffer, len, 0, UPDATE_AREA);
                    partition_reinit(PRIVATE_AREA, PARTITION_SIZE);
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, PRIVATE_AREA);//覆盖原私钥区
                    // memset(download_buffer, 0, strlen(download_buffer));
                    // free(read_buffer);
                    // read_cert();
                    // ESP_LOGI(TAG,"PrivateKey download done");
                }
                else if(success == 2)
                {                    
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, UPDATE_AREA);//先存入缓存区
                    /*此处最好再有一个检验证书的步骤*/
                    memset(download_buffer, 0, strlen(download_buffer));

                    uint32_t len = get_partition_plaintext_len(UPDATE_AREA); //获取长度
                    // partition_read_decrypt((uint8_t*)read_buffer, len, 0, UPDATE_AREA);
                    // partition_write_encrypt((uint8_t*)read_buffer, strlen(read_buffer), 0, IOT_CERTIFICATE_AREA);//覆盖原公钥区
                    partition_read_decrypt((uint8_t*)download_buffer, len, 0, UPDATE_AREA);
                    partition_reinit(IOT_CERTIFICATE_AREA, PARTITION_SIZE);
                    partition_write_encrypt((uint8_t*)download_buffer, strlen(download_buffer), 0, IOT_CERTIFICATE_AREA);//覆盖原公钥区
                    // memset(download_buffer, 0, strlen(download_buffer));
                    // free(read_buffer);
                    // read_cert();
                    // ESP_LOGI(TAG,"Certificate download done");
                }                
#endif
                ESP_LOGD(TAG,"cjson_to_serial_info:success=%d",success);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
        if(success == 1)
        {
            ESP_LOGI(TAG,"PrivateKey done, continue to download Certificate");
            download_file_type = 1;//继续下载
            memset(download_buffer, 0, download_buffer_len);
            goto download_again;
        }
        else if(success == 2)
        {
            ESP_LOGI(TAG,"Certificate done, all files done");
			if(download_buffer != NULL)
			{
				free(download_buffer);
			}            
        }
        else
        {
            ESP_LOGE(TAG,"File download failed");
            if(download_buffer != NULL)
			{
				free(download_buffer);
			}
        }
    }    
    esp_http_client_cleanup(client);  
    if(post_data != NULL)
    {
        free(post_data);
    }
exit:
    return success;
}

#endif

 /*------------------------------------------------------------------------------
  Function: Mqtt_Active_Report_Queue_Init
  -----------------------------------------------------------------------------*/
 /**
   * @brief      队列初始化
   * @param[in]  void  
   * @param[out] None
   * @return     void
   */
 static void Mqtt_Active_Report_Queue_Init(void)
 {
     /*队列发送*/
     if (xQueue_Mqtt_Active_Report == NULL)
     {
         xQueue_Mqtt_Active_Report = xQueueCreate(64, sizeof(Mqtt_Active_Report_Queue_Struct));
         if (xQueue_Mqtt_Active_Report == NULL)
         {
             ESP_LOGE(TAG, "Mqtt_Active_Report_Queue_Init create failed");
         }
     }
 }

 /*------------------------------------------------------------------------------
  Function: mqtt_task_init
  -----------------------------------------------------------------------------*/
 /**
   * @brief      MQTT初始化
   * @param[in]  void  
   * @param[out] None
   * @return     void
   */
 int mqtt_task_init(void)
 {
    if (login_info.mqtt_msg_queue == NULL) {
        login_info.mqtt_msg_queue = xQueueCreate(64, sizeof(queue_msg_t));
        if (login_info.mqtt_msg_queue == NULL) {
            ESP_LOGE(TAG, "mqtt recv message queue create failed");
            return -1;
        }
    }

	mqtt_semaphore = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(mqtt_semaphore == NULL) {
        ESP_LOGE(TAG, "mqtt_semaphore  create failed");
		return -1;
	}
	xSemaphoreGive(mqtt_semaphore);						/**< the resource is available after creation */

    /*上报队列初始化*/
    Mqtt_Active_Report_Queue_Init();

#ifdef CONFIG_MQTT_RECV_CALLBACK_ENABLE
    /* 创建接收超时定时器 */
    if (!mqtt_recv_timeout_tmr) {
        mqtt_recv_timeout_tmr = xTimerCreate("mqtt timer",			// 定时器名称
                                    1000, 				            // 时间,此处参数无效，将会被覆盖
                                    pdFALSE,			            // 自动重载
                                    NULL, 				            // 定时器ID
                                    mqtt_recv_timeout_timer_cb);	// 回调函数
        assert(mqtt_recv_timeout_tmr);
    }
#endif

	int16_t ret =0;
    login_step = STEP_MQTT_WAITING;
	login_info.sock = -1;
    login_info.mqtt_client = NULL;
    login_info.current_netif_id = 0xFF;

    /*服务器地址配置*/
    iot_mqtt_new_host(IotSetData.dev_info_t.could_dns);

    /*MQTT登录*/
    iot_mqtt_new_iot(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);

    /*MQTT订阅*/
    if ((0 != dev_factory.dev_sn) 
        && (MASS_PRODUCTION_DEFAULT_DEV_SN != dev_factory.dev_sn)) 
    {
        iot_mqtt_new_dev(dev_factory.dev_type,  dev_factory.dev_sn);
    } else {
        /*未标定，默认用IOT的SN*/
        iot_mqtt_new_dev(iot_factory.iot_type,  iot_factory.iot_sn);
    }   

    return 0;
 }

/*------------------------------------------------------------------------------
 Function: mqtt_exit_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      退出MQTT
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void mqtt_exit_process(void)
{
    // 1. 标记状态，清除TCP绑定列表
     memset(&mqtt_client_dump, 0x00, sizeof(struct esp_mqtt_client_dump));
     login_info.mqtt_is_login = 0;
     login_info.mqtt_is_start = 0;
          
    // 2. 清理自定义 socket（如果有且不属于 MQTT 客户端）
     if ((login_info.sock != 0xff) && (login_info.sock >= 3)) {
         // 先调用 shutdown 再调用 close
         if (shutdown(login_info.sock, SHUT_RDWR) == -1) {
             ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", login_info.sock, errno, strerror(errno));
         }
         if (close(login_info.sock) == -1) {
             ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", login_info.sock, errno, strerror(errno));
         }
         login_info.sock = -1;
     }
     
     // 3. 如果客户端存在，断开连接
     if (login_info.mqtt_client != NULL)
     {
         esp_err_t stop_ret = esp_mqtt_client_stop(login_info.mqtt_client);
         ESP_LOGD(TAG, "esp_mqtt_client_stop ret=%d (%s)",
                  stop_ret, esp_err_to_name(stop_ret));

         esp_err_t destroy_ret = esp_mqtt_client_destroy(login_info.mqtt_client);
         ESP_LOGD(TAG, "esp_mqtt_client_destroy ret=%d (%s)",
                  destroy_ret, esp_err_to_name(destroy_ret));

         login_info.mqtt_client = NULL;
         ESP_LOGW(TAG, "mqtt client stop");

         // 连接时间有效，说明未触发MQTT_EVENT
         if ( Mqtt_Machine_Connect_Time ) {
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
             // MQTT日志生成
             mqtt_conn_fail_log_t record = {
                 .svc_timestamp = login_info.now_time,
                 .uptime_seconds = (Mqtt_Machine_Connect_Time > 0) ? ((xTaskGetTickCount() * portTICK_PERIOD_MS - Mqtt_Machine_Connect_Time) / 1000) : 0,
                 .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                 .password = login_info.password,
                 .reason = MQTT_DEVICE_STOP,
                 .network_type = login_info.current_netif_id,
             };
             Mqtt_Log_Generate(&record);
             Mqtt_Machine_Connect_Time = 0;
#endif
         }
     }
}

/*------------------------------------------------------------------------------
 Function: iot_net_status_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      系统网络检查
  * @param[in]  void  
  * @param[out] None
  * @return     static bool
  */
bool iot_net_status_check(void)
{
    uint8_t wifi_status = 0, eth_status = 0, _4g_status = 0;

    /*WIFI连接状态*/
    wifi_status = iot_get_wifi_info()->flag.bit.sta_link_ap;

#ifdef CONFIG_USE_SPI_ETHERNET_ENABLE
    /*以太网连接状态*/
    eth_status = iot_eth_net_get_status();
#endif

#ifdef CONFIG_USB_4G_MODULE_ENABLE
    /*4G网络状态*/
    _4g_status = iot_4g_net_get_status();
#endif

    ESP_LOGD(TAG, "iot_net_status_check : wifi_status(%d), eth_status(%d), _4g_status(%d)", wifi_status, eth_status, _4g_status);
    return (wifi_status || eth_status || _4g_status) ? true : false;
}

/*------------------------------------------------------------------------------
 Function: iot_netif_is_connected
 -----------------------------------------------------------------------------*/
/**
  * @brief      当前网卡网络在线状态
  * @param[in]  void  
  * @param[out] None
  * @return     bool
  */
bool iot_netif_is_connected(uint8_t current_netif_id)
{    
    if (NETIF_TYPE_ETH == current_netif_id) {
#ifdef CONFIG_USE_SPI_ETHERNET_ENABLE        
        return iot_eth_net_get_status();
#endif
    } else if (NETIF_TYPE_WIFI_STA == current_netif_id) {
        return ((iot_get_wifi_info()->flag.bit.sta_link_ap) ? true : false);
    } else if (NETIF_TYPE_PPP == current_netif_id) {
#ifdef CONFIG_USB_4G_MODULE_ENABLE        
        return iot_4g_net_get_status();
#endif
    } else if (NETIF_TYPE_WIFI_AP == current_netif_id) {
#ifdef CONFIG_WIFI_AP_MODE_ENABLE
        // 暂不支持
#endif
    }

    return false;
}

/*------------------------------------------------------------------------------
 Function: set_mqtt_client_netif
 -----------------------------------------------------------------------------*/
/**
  * @brief      切换网口使用
  * @param[in]  uint8_t netif_type  
  * @param[out] None
  * @return     static void
  */
static void set_mqtt_client_netif(uint8_t netif_type)
{
    char *netif_key[NETIF_TYPE_MAX] = {NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP, NETIF_KEY_WIFI_AP};
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(netif_key[netif_type]);

    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to get network interface handle for %s", netif_key[netif_type]);
        return;
    }

    // 设置网络接口为默认接口
    esp_netif_set_default_netif(netif);

#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(netif, login_info.netif_req.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(netif), login_info.netif_req.ifr_name);
#endif

    mqtt_cfg.network.if_name = &(login_info.netif_req);
    login_info.current_netif_id = netif_type;
    ESP_LOGI(TAG, "set_mqtt_client_netif:%s", mqtt_cfg.network.if_name->ifr_name);

    set_http_client_netif(netif_type); // http使用当前mqtt网口

#ifdef CONFIG_OPEN_MQTT_ENABLE
    set_open_mqtt_client_netif(netif_type);
#endif
}

/*------------------------------------------------------------------------------
 Function: iot_switch_netif
 -----------------------------------------------------------------------------*/
/**
  * @brief      网口智能选择逻辑
  * @param[in]  void  
  * @param[out] None
  * @return     static bool
  *
  * 该函数用于检测所有网络接口的连接状态变化，并根据实际需求进行切换或处理。
  */
static bool iot_switch_netif(void)
{
    static bool old_status[NETIF_TYPE_MAX] = {false};
    bool new_status[NETIF_TYPE_MAX] = {false};
    bool changed = false;

    for (uint8_t i = 0; i < NETIF_TYPE_MAX; i++) {
        new_status[i] = iot_netif_is_connected(i);
        if (new_status[i] != old_status[i]) {
            changed = true;
            ESP_LOGW(TAG, "Netif %d status changed: %s -> %s", i,
                     old_status[i] ? "CONNECTED" : "DISCONNECTED",
                     new_status[i] ? "CONNECTED" : "DISCONNECTED");

            // 当前使用网络断开，等待重新选择网口
            if (( false == new_status[i] ) 
                && ( i == login_info.current_netif_id )) {
                login_info.current_netif_id = 0xFF;
            }
        }
    }

    // 网络状态变动，根据优先级选择主用网络接口
    if (changed) {
        for (uint8_t i = 0; i < NETIF_TYPE_MAX; i++) {
            if (new_status[i]) {
                if ( i != login_info.current_netif_id ) {
                    if ( NETIF_TYPE_WIFI_AP != i ) {                        
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                        if ( login_info.mqtt_is_login )
                        {
                            // MQTT日志生成
                            mqtt_conn_fail_log_t record = {
                                .svc_timestamp = login_info.now_time,
                                .uptime_seconds = (Mqtt_Machine_Connect_Time > 0) ? ((xTaskGetTickCount() * portTICK_PERIOD_MS - Mqtt_Machine_Connect_Time) / 1000) : 0,
                                .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                                .password = login_info.password,
                                .reason = MQTT_FAIL_NETWORK_CHANGE,
                                .network_type = login_info.current_netif_id,
                            };
                            Mqtt_Log_Generate(&record);
                            Mqtt_Machine_Connect_Time = 0;
                        }
#endif                        
                        // 重新映射网络接口
                        ESP_LOGW(TAG, "Switch to netif %d", i);
                        set_mqtt_client_netif(i);
                        // 预备重新发起云端连接
                        login_step = STEP_MQTT_WAITING;
                    }
                } else {
                    // 当前网口优先级最高，不做切换
                }
                break;
            }
        }
        // 更新旧状态
        memcpy(old_status, new_status, sizeof(old_status));
    }

    if ( login_info.current_netif_id < NETIF_TYPE_MAX) {
        return true;
    } else {
        return false;
    }
}

/*------------------------------------------------------------------------------
 Function: iot_curr_netif_id_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      当前网口获取
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
uint8_t iot_curr_netif_id_get(void)
{
    return login_info.current_netif_id;
}

/*------------------------------------------------------------------------------
 Function: iot_mqtt_State_Machine
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT连接状态机
  * @param[in]  void  
  * @param[out] None
  * @return     static
  */
void iot_mqtt_State_Machine(void)
{
    static bool wait_state = false;
    static uint32_t pre_wait_time = 0;
    static uint32_t wait_time = 0;
    int sys_errno = 0;
    
    /**
     * @brief WiFi网络连后MQTT登录逻辑
     * 1、解析主机域名
     * 2、和主机建立tcp连接 (通过连接获取主机当前的时间)
     * 3、发送指令读取主机时间
     * 4、计算登录密码，发起登录
     * 5、处理MQTT交互信息
     */
    if (!iot_net_status_check()
        || (reals.IOT_Status_Flag.sBit.system_off_flag == 1)
        || (login_info.iot_sn == IOT_FACTORY_DEFAULT_DEV_SN)
        || (login_info.safetyCode == IOT_FACTORY_DEFAULT_SAFE_CODE)
        || (2 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable)
        || (exit_flag)
        )
    {            
        if ( login_info.mqtt_client ) {
            login_step = STEP_MQTT_EXIT;
        } else {
            login_step = STEP_MQTT_WAITING;

            /*处理网口状态*/
            iot_switch_netif();
        }
    }
    else
    {
        /*等待状态结束，重新发起连接*/
        if ( login_step == STEP_MQTT_WAITING || Mqtt_Restart_Flag) {
            login_step = STEP_MQTT_RESTART;

            // 状态机记录清空
            Mqtt_Restart_Flag = false;
            Mqtt_Machine_Pause_Time = 0;
            wait_state = false;
            pre_wait_time = 0;
            wait_time = 0;
        }

        /*判断是否有外部延时请求*/
        if ( Mqtt_Machine_Pause_Time ) {
            wait_state = true;
            pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            wait_time = Mqtt_Machine_Pause_Time;
            Mqtt_Machine_Pause_Time = 0;
        }
        
        /*延时等待执行下一步*/
        if ( wait_state == true ) {
            uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ( (now_time - pre_wait_time) <= wait_time ) {
                return;
            }
            
            wait_state = false;
            pre_wait_time = 0;
            wait_time = 0;
        }
		
        /*处理各网口状态，选择最优网口*/
        if(!iot_switch_netif()) {
            return;
        }
    }

    switch (login_step) 
    {
        case STEP_MQTT_WAITING:
            // 系统状态不满足云端任务启动条件，暂不执行
        break;
        
        /**
         * @brief 停止MQTT客服端
         */
        case STEP_MQTT_RESTART:
            mqtt_exit_process();
            Mqtt_Announce_Info_Clean();
            login_step = STEP_HOST_DNS;
        break;

        case STEP_HOST_DNS:  /* 域名解析 */
            ESP_LOGI(TAG, "mqtt client go to DNS");
            if (mqtt_client_UTC_IP_GET(&login_info) == 1) {
                if(login_info.now_time != 0){
                    // 处理网络检测结果
                    if(NETIF_TYPE_WIFI_STA == login_info.current_netif_id) {
                        iot_wifi_process_internet_check_result(INTERNET_OK);
                    }
#ifdef CONFIG_MQTT1_TCP_ENCRYPT_ENABLE       
                    uint8_t next_step = 0;
                    if ( 1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable ) {
                        next_step = STEP_MQTT_CertCheck;
                    } else {
                        next_step = STEP_MQTT_LOGIN;
                    }
#else
                    uint8_t next_step = STEP_MQTT_LOGIN;
#endif

#ifdef CONFIG_MQTT_CONNECT_HTTP_TOKEN_ENABLE
                    connection_token_t *connect_token_info = http_conn_token_info_get();
                    switch ( connect_token_info->state )
                    {
                        case 0 :
                            // 0: 服务器现在可接收客户端的连接，客户端可立即连接到服务器
                            login_step = next_step;
                            ESP_LOGI(TAG, "Successfully mqtt_client_UTC_IP_GET, now_time: %lld, mqtt client go to next_step", login_info.now_time);
                            break;
                        case 1 :
                            // 1: 服务器拒绝客户端连接，此时值需要根据 `waitSec` 值做出下一步逻辑处理。
                            if ( connect_token_info->waitSec > 0 ) {
                                // >0: 以服务器告知的等待时长为准
                                wait_time = connect_token_info->waitSec * 1000;
                            } else if ( connect_token_info->waitSec == 0 ) {
                                // 0:  由客户端自主决定等待的秒数，如固定30秒或60秒内随机。（暂定30s~60s随机）
                                wait_time = (30 + rand() % 31) * 1000;
                            } else if ( connect_token_info->waitSec == -1 ) {
                                // -1: 一直永远不要发起服务器的TCP连接 (客户端系统下次上电重启时，可以重新向服务器获取最新的等待秒数)。
                                // 保险起见，不允许永远不发起连接，暂定等待3min后重试
                                wait_time = 3 * 60 * 1000;
                            } else {
                                // 其他值，当前无法解析，为保证兼容性默认允许直接连接
                                login_step = next_step;
                                ESP_LOGW(TAG, "Unknown conn_token_waitSec(%d), now_time: %lld, mqtt client go to next_step", connect_token_info->waitSec, login_info.now_time);
                                break;
                            }
                            wait_state = true;
                            pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                            login_step = STEP_MQTT_RESTART;
                            ESP_LOGE(TAG, "Server refused connect! Waiting to reconnect(%ld s)...", (wait_time / 1000));
                            break;
                        default:
                            // 其他值，当前无法解析，为保证兼容性默认允许直接连接
                            login_step = next_step;
                            ESP_LOGW(TAG, "Unknown conn_token_state(%d), now_time: %lld, mqtt client go to next_step", connect_token_info->state, login_info.now_time);
                            break;
                    }
#else
                    login_step = next_step;
                    ESP_LOGI(TAG, "http_utc_ip_get Success, now_time: %lld, mqtt client go to next_step", login_info.now_time);
#endif                    
                } else {
                    login_step = STEP_CREATE_TCP;
                    ESP_LOGI(TAG, "mqtt client go to TCP connect");
                }
            } else {
                // 处理网络检测结果
                if(NETIF_TYPE_WIFI_STA == login_info.current_netif_id) {
                    iot_wifi_process_internet_check_result(INTERNET_DNS_FAIL);
                }
                
                // 域名解析失败8s后再尝试
                wait_state = true;
                wait_time = 8000;
                pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .reason = MQTT_FAIL_DNS,
                    .network_type = login_info.current_netif_id,
                    .retry_delay_ms = (uint16_t)wait_time,
                };
                Mqtt_Log_Generate(&record);
#endif
            }
        break;

        case STEP_CREATE_TCP: /* 建立tcp连接 */
            if (iot_tcp_connect_host((const char *)login_info.ipaddr, login_info.port, &login_info.sock, &sys_errno) == 1) 
            {
                login_step = STEP_GET_HOST_TIME; /* 下一步 */
                ESP_LOGI(TAG, "mqtt client go to get host time");
                
                // 处理网络检测结果
                if(NETIF_TYPE_WIFI_STA == login_info.current_netif_id) {
                    iot_wifi_process_internet_check_result(INTERNET_OK);
                }
            } 
            else 
            {
                ESP_LOGW(TAG, "STEP_CREATE_TCP failed");
                
                /* 网络在线回到上一步否则退出 */
                iot_netif_is_connected(login_info.current_netif_id) ? (login_step = STEP_HOST_DNS) : (login_step = STEP_MQTT_CONNECT_FAIL);

                // 处理网络检测结果
                if(NETIF_TYPE_WIFI_STA == login_info.current_netif_id) {
                    iot_wifi_process_internet_check_result(INTERNET_NO_ROUTE);
                }
                
                // 失败8s后再尝试
                wait_state = true;
                wait_time = 8000;
                pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .reason = MQTT_FAIL_TCP,
                    .sys_errno = sys_errno,
                    .network_type = login_info.current_netif_id,
                    .retry_delay_ms = (uint16_t)wait_time,
                };
                Mqtt_Log_Generate(&record);
#endif
            }
        break;

        case STEP_GET_HOST_TIME: /* 获取服务器时间(同步) */
            login_info.now_time = iot_sync_host_time(&login_info.sock, login_info.iot_type, login_info.iot_sn, &sys_errno);
            if(login_info.now_time <= 0) {
                ESP_LOGE(TAG,"STEP_GET_HOST_TIME Failed!");
                
                /* 网络在线回到上一步否则退出 */
                iot_netif_is_connected(login_info.current_netif_id) ? (login_step = STEP_CREATE_TCP) : (login_step = STEP_MQTT_CONNECT_FAIL);

                // 失败5s后再尝试
                wait_state = true;
                wait_time = 5000;
                pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .reason = MQTT_FAIL_TCP_TIME,
                    .sys_errno = sys_errno,
                    .network_type = login_info.current_netif_id,
                    .retry_delay_ms = (uint16_t)wait_time,
                };
                Mqtt_Log_Generate(&record);
#endif
            } else {
#ifdef CONFIG_MQTT1_TCP_ENCRYPT_ENABLE	
                if ( 1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable ) {
                    ESP_LOGI(TAG,"STEP_GET_HOST_TIME success, now_time:= %lld, go to STEP_MQTT_CertCheck",login_info.now_time);
                    login_step = STEP_MQTT_CertCheck; /* 下一步 */ 
                } else {
                    ESP_LOGI(TAG,"STEP_GET_HOST_TIME success, now_time:= %lld, go to STEP_MQTT_LOGIN",login_info.now_time);
                    //断开tcp连接，启动mqtt登录连接
                    login_step = STEP_MQTT_LOGIN; /* 下一步 */ 
                }
#else
                ESP_LOGI(TAG,"STEP_GET_HOST_TIME success, now_time:= %lld, go to STEP_MQTT_LOGIN",login_info.now_time);
                //断开tcp连接，启动mqtt登录连接
                login_step = STEP_MQTT_LOGIN; /* 下一步 */ 
#endif                
            }
        break;

        /* 计算MQTT登录密码,并且登录 */
        case STEP_MQTT_LOGIN:
            /* 按照登录流程计算两个密码,MQTT登录用户名与密码如下格式:
             * username: EBOX132564873121(SN)
             * passwrod: password,IOT@password2 */
            ESP_LOGD(TAG,"STEP_MQTT_LOGIN : Start to calc password.");
            login_info.password = iot_login_password((uint32_t)login_info.now_time, login_info.iot_type, login_info.iot_sn, login_info.safetyCode);
            login_info.password2 = iot_login_password((uint32_t)login_info.now_time, "IOT", login_info.iot_sn, login_info.safetyCode);
            if (iot_mqtt_login(&login_info) == 1) 
            {
                if ( STEP_MQTT_LOGIN == login_step ) {
                    login_info.mqtt_is_start = 1;
                    login_step = STEP_MQTT_MSG_HANDLE;
                    ESP_LOGI(TAG, "mqtt client login successfully and go to message handle");
                } else {           
                    // 连接时有极低概率在连接过程中触发MQTT_EVENT，原因未知
                    ESP_LOGE(TAG,"STEP_MQTT_LOGIN ERROR!");
                    
                    // 失败5s后再尝试
                    wait_state = true;
                    wait_time = 5000;
                    pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    login_step = STEP_MQTT_CONNECT_FAIL;
                }
            } 
            else 
            {
                ESP_LOGE(TAG,"STEP_MQTT_LOGIN failed");
                
                /* 网络在线重新进行mqtt登录否则退出 */
                iot_netif_is_connected(login_info.current_netif_id) ? (login_step = STEP_MQTT_RESTART) : (login_step = STEP_MQTT_CONNECT_FAIL);

                // 失败5s后再尝试
                wait_state = true;
                wait_time = 5000;
                pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .svc_timestamp = login_info.now_time,
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .password = login_info.password,
                    .reason = MQTT_FAIL_LOGIN,
                    .network_type = login_info.current_netif_id,
                    .retry_delay_ms = (uint16_t)wait_time,
                };
                Mqtt_Log_Generate(&record);
#endif
            }
        break;

        /* MQTT数据处理 */
        case STEP_MQTT_MSG_HANDLE:
            if (login_info.mqtt_client == NULL) {
                ESP_LOGE(TAG,"iot_tcp_connect_host:login_info.mqtt_client == NULL");            
                break;
            }
            iot_mqtt_msg_handle(login_info.mqtt_client, login_info.mqtt_msg_queue, login_info.pub_topic);
        break;

        case STEP_MQTT_CONNECT_FAIL:
            
            /*MQTT连接失败，预备重新连接*/
            login_step = STEP_MQTT_RESTART;
        
        break;
        
        case STEP_MQTT_EXIT:
            
            /*已提前执行退出*/
            if(exit_flag) break;
            
            /*MQTT断开*/
            mqtt_exit_process();
            
        break;
            
#ifdef CONFIG_MQTT1_TCP_ENCRYPT_ENABLE	
        case STEP_MQTT_CertCheck:
            // 检查证书是否存在
            if (http_iot_cert_exist(login_info))
            {
                // 已存在，跳转到证书过期检查
                // 每次与服务器比较证书时间是否过期时，都要先刷新，防止MQTT异常退出重连时还使用旧证书的时间
                cert_utc_sec = calculate_certificate_utc_sec(iot_cert_ptr);
                if (login_info.is_need_down_cert || login_info.now_time > cert_utc_sec) 
                {
                    // 证书过期
                    ESP_LOGE(TAG,"STEP_MQTT_CertCheck failed, cert_utc_sec(%lu), now_time(%llu), go to STEP_MQTT_CerExpiration", cert_utc_sec, login_info.now_time);
                    login_step = STEP_MQTT_CerExpiration; /* 下一步 */
                }
                else
                {
                    // 证书有效，开始登录
                    ESP_LOGI(TAG,"STEP_MQTT_CertCheck success, cert_utc_sec(%lu), now_time(%llu), go to STEP_MQTT_LOGIN", cert_utc_sec, login_info.now_time);
                    login_step = STEP_MQTT_LOGIN; /* 下一步 */ 
                }
            }
            else
            {
                // 首次下载，使用http获取证书的序列号
                if (http_get_cert_sn(login_info))
                {
                    login_step = STEP_MQTT_CerDownload;
                    ESP_LOGW(TAG,"mqtt cert not exist, go to STEP_MQTT_CerDownload");
                }
                else 
                {
                    ESP_LOGE(TAG,"STEP_MQTT_CertCheck failed, http_get_cert_sn failed, Delay to reconnect...");
                    
                    /* 网络在线重新进行mqtt登录否则退出 */
                    iot_netif_is_connected(login_info.current_netif_id) ? (login_step = STEP_MQTT_RESTART) : (login_step = STEP_MQTT_CONNECT_FAIL);
                
                    // 失败15s后再尝试
                    wait_state = true;
                    wait_time = 15000;
                    pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                    // MQTT日志生成
                    mqtt_conn_fail_log_t record = {
                        .svc_timestamp = login_info.now_time,
                        .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                        .reason = MQTT_FAIL_CERT,
                        .network_type = login_info.current_netif_id,
                        .retry_delay_ms = (uint16_t)wait_time,
                    };
                    Mqtt_Log_Generate(&record);
#endif
                }
            }
        break;

        case STEP_MQTT_CerExpiration:
            if(http_iot_update_cert(login_info) > 0)
            {
                //下载最新的设备证书
                ESP_LOGI(TAG,"STEP_MQTT_CerExpiration success, go to STEP_MQTT_CerDownload");
                login_step = STEP_MQTT_CerDownload; /* 下一步 */ 
            }
            else
            {
                //尝试重新更新最新的设备证书                        
                ESP_LOGE(TAG,"STEP_MQTT_CerExpiration failed, try to get again"); 
                login_step = STEP_MQTT_CerExpiration; /* 重新发起HTTP请求，获取证书 */ 

                // 失败10s后再尝试
                wait_state = true;
                wait_time = 10000;
                pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .svc_timestamp = login_info.now_time,
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .reason = MQTT_FAIL_CERT,
                    .network_type = login_info.current_netif_id,
                    .retry_delay_ms = (uint16_t)wait_time,
                };
                Mqtt_Log_Generate(&record);
#endif
            }

        break;

        case STEP_MQTT_CerDownload:
            if (serial_flag > 0)
            {
                ESP_LOGD(TAG,"STEP_MQTT_CerDownload,serial_data=%s", serial_data);
                if(http_iot_download_cert(login_info) == 2)//两份证书都下载完毕
                {
                    if(load_all_cer())//重载证书
                    {
                        ESP_LOGI(TAG,"Certificate reload success, next step is STEP_MQTT_LOGIN");
                        login_info.is_need_down_cert = false;
                        login_step = STEP_MQTT_LOGIN;
                    }
                    else
                    {
                        ESP_LOGE(TAG,"Failed to reload Certificate, try again");
                        login_step = STEP_CREATE_TCP; /* 重新创建tcp连接 */ 
                        
                        // 失败8s后再尝试
                        wait_state = true;
                        wait_time = 8000;
                        pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    }
                }
                else
                {
                    ESP_LOGE(TAG,"STEP_MQTT_CerDownload failed, lost some of them, try again");
                    login_step = STEP_CREATE_TCP;
                    
                    // 失败8s后再尝试
                    wait_state = true;
                    wait_time = 8000;
                    pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                }                    
            } 
            else 
            {
                ESP_LOGE(TAG,"STEP_MQTT_CerDownload failed, try again");//A904503
                login_step = STEP_CREATE_TCP; /* 重新创建tcp连接 */ 
                
                // 失败8s后再尝试
                wait_state = true;
                wait_time = 8000;
                pre_wait_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }
            
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
            if ( login_info.is_need_down_cert ) {
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .svc_timestamp = login_info.now_time,
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .reason = MQTT_FAIL_CERT,
                    .network_type = login_info.current_netif_id,
                    .retry_delay_ms = (uint16_t)wait_time,
                };
                Mqtt_Log_Generate(&record);
            }
#endif

            break;
#endif				

        default:  break;
    }
}

/*------------------------------------------------------------------------------
 Function: iot_mqtt_login_step_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取MQTT状态机状态
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_mqtt_login_step_get(void)
{
    return login_step;//外部文件调用
}

/*------------------------------------------------------------------------------
 Function: iot_get_cloud_dns
 -----------------------------------------------------------------------------*/
/**
  * @brief      DNS解析，从URL中解析出主机名、端口和IP地址
  * @param[in]  const char *url - 输入URL，格式：host:port
  * @param[out] char *host - 输出主机名缓冲区
  * @param[out] uint16_t *port - 输出端口号
  * @param[out] uint8_t ipaddr[4] - 输出IPv4地址数组
  * @return     uint8_t (1-成功, 0-失败)
  */
 uint8_t iot_get_cloud_dns(const char *url, char *host, uint16_t *port, uint8_t ipaddr[])
{
#if 0
    ip_addr_t target_addr;// ip4_addr_t ip_addr_t
    struct in_addr ip_addr4;
    const struct addrinfo hints = {
        .ai_family = AF_INET,           /* 指定返回地址的协议簇，AF_INET(IPv4)、AF_INET6(IPv6)、AF_UNSPEC(IPv4 and IPv6)*/
        .ai_socktype = SOCK_STREAM,     /* 设定返回地址的socket类型，流式套接字 */
    };

    if (!strlen(url) || !port || !host || !ipaddr) {
        ESP_LOGE(TAG, "host url is null can not analyze");
        return 0;
    }

    sscanf(url, "%255[^:]:%hu", host, port);
    ESP_LOGI(TAG,"raw url: %s: %d", host, *port);

    struct addrinfo *result = NULL;
    int SetTimeZone = getaddrinfo(host, NULL, &hints, &result); /* 解析服务器地址 */
    if(SetTimeZone != 0) {
        ESP_LOGE(TAG, "dns analyze failed err=%d SetTimeZone=%p", SetTimeZone, result);
        iot_wifi_connect_status_check();
        if (result) freeaddrinfo(result); 
        return 0;
    }

    ip_addr4 = ((struct sockaddr_in *) (result->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &ip_addr4);
    freeaddrinfo(result); // 解析完成释放开辟的资源
	ESP_LOGI(TAG,"got host ipv4: %s port: %u", inet_ntoa(target_addr.u_addr.ip4) , *port);
    return 1;
    
#else

    struct addrinfo *result = NULL;
    struct in_addr ip_addr4;
    char url_copy[256] = {0};

    // 参数验证
    if (!url || !host || !port || !ipaddr) {
        ESP_LOGE(TAG, "Invalid parameters: url=%p, host=%p, port=%p, ipaddr=%p", 
                 url, host, port, ipaddr);
        return 0;
    }

    if (strlen(url) == 0 || strlen(url) >= sizeof(url_copy)) {
        ESP_LOGE(TAG, "Invalid URL length: %zu", strlen(url));
        return 0;
    }

    // 初始化输出参数
    *port = 0;
    memset(ipaddr, 0, 4);

    // 复制URL进行解析（避免修改原始字符串）
    strncpy(url_copy, url, sizeof(url_copy) - 1);
    url_copy[sizeof(url_copy) - 1] = '\0';

    // 解析URL格式：host:port
    char *colon_pos = strchr(url_copy, ':');
    if (colon_pos) {
        *colon_pos = '\0';  // 分割字符串
        strcpy(host, url_copy);
        
        // 解析端口号
        char *endptr;
        long port_val = strtol(colon_pos + 1, &endptr, 10);
        if (*endptr != '\0' || port_val <= 0 || port_val > 65535) {
            ESP_LOGE(TAG, "Invalid port number: %s", colon_pos + 1);
            return 0;
        }
        *port = (uint16_t)port_val;
    } else {
        // 没有端口号，只有主机名
        strcpy(host, url_copy);
        ESP_LOGW(TAG, "No port specified in URL, port remains: %d", *port);
    }

    ESP_LOGI(TAG, "Parsing URL: '%s' -> host: '%s', port: %u", url, host, *port);

    // 检查是否为IP地址格式
    struct in_addr addr_buf;
    if (inet_aton(host, &addr_buf) == 1) {
        // 输入已经是IP地址
        uint32_t ip = ntohl(addr_buf.s_addr);
        ipaddr[0] = (ip >> 24) & 0xFF;
        ipaddr[1] = (ip >> 16) & 0xFF;
        ipaddr[2] = (ip >> 8) & 0xFF;
        ipaddr[3] = ip & 0xFF;
        
        ESP_LOGI(TAG, "Input is already IP address: %d.%d.%d.%d:%u", 
                 ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3], *port);
        return 1;
    }

    // DNS解析
    const struct addrinfo hints = {
        .ai_family = AF_INET,       // 只支持IPv4
        .ai_socktype = SOCK_STREAM, // TCP socket
    };

    ESP_LOGI(TAG, "Starting DNS resolution for: %s", host);

    int dns_result = getaddrinfo(host, NULL, &hints, &result);
    if (dns_result != 0) {
        ESP_LOGE(TAG, "DNS resolution failed for '%s': error %d", 
                 host, dns_result);
        
        if (result) {
            freeaddrinfo(result);
        }
        return 0;
    }

    if (!result || !result->ai_addr) {
        ESP_LOGE(TAG, "DNS resolution returned null result");
        if (result) {
            freeaddrinfo(result);
        }
        return 0;
    }

    // 提取IPv4地址
    if (result->ai_family != AF_INET) {
        ESP_LOGE(TAG, "Unexpected address family: %d", result->ai_family);
        freeaddrinfo(result);
        return 0;
    }

    ip_addr4 = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;

    // 转换为数组格式
    uint32_t ip = ntohl(ip_addr4.s_addr);
    ipaddr[0] = (ip >> 24) & 0xFF;
    ipaddr[1] = (ip >> 16) & 0xFF;
    ipaddr[2] = (ip >> 8) & 0xFF;
    ipaddr[3] = ip & 0xFF;

    ESP_LOGI(TAG, "DNS resolution successful: %s -> %d.%d.%d.%d:%u", 
             host, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3], *port);

    // 释放资源
    freeaddrinfo(result);

    return 1;

#endif
}

/*------------------------------------------------------------------------------
 Function: iot_last_host_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      上次连接域名获取（返回上次缓存的host、port、ipaddr）
  * @param[out] char *host        输出：上次连接的host
  * @param[out] uint16_t *port    输出：上次连接的端口
  * @param[out] uint8_t ipaddr[]  输出：上次连接的IP地址（长度4）
  * @return     static uint8_t           1-有有效缓存，0-无缓存
  */
static uint8_t iot_last_host_get(char *host, uint16_t *port, uint8_t ipaddr[])
{
    old_mqtt_connect_data_t *old_data = &IotSetData.dev_info_t.old_mqtt_connect_data;

    // 检查参数有效性
    if (!host || !port || !ipaddr) {
        ESP_LOGE(TAG, "iot_last_host_get: invalid output pointer");
        return 0;
    }

    // 检查缓存是否有效
    if (old_data->http_get_host_valid &&
        old_data->http_get_old_host[0] != '\0' &&
        old_data->http_get_old_port != 0 &&
        (old_data->http_get_old_ipaddr[0] | old_data->http_get_old_ipaddr[1] | old_data->http_get_old_ipaddr[2] | old_data->http_get_old_ipaddr[3]) != 0) {
        strncpy(host, old_data->http_get_old_host, 119);
        host[119] = '\0';
        *port = old_data->http_get_old_port;
        memcpy(ipaddr, old_data->http_get_old_ipaddr, 4);
        ESP_LOGI(TAG, "iot_last_host_get: use cached host=%s, port=%u, ip=%d.%d.%d.%d, mqtt_http_ip_date=%llu",
                 host, *port, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3],
                 IotSetData.dev_info_t.mqtt_http_ip_date);
        return 1;
    }

    ESP_LOGE(TAG, "iot_last_host_get: no valid cache");
    return 0;
}

 /*------------------------------------------------------------------------------
  Function: iot_last_host_set
  -----------------------------------------------------------------------------*/
 /**
  * @brief      保存本次连接的host、port、ipaddr到缓存
  * @param[in]  const char *host
  * @param[in]  uint16_t port
  * @param[in]  const uint8_t ipaddr[4]
  * @return     static void
  */
static void iot_last_host_set(const char *host, const uint16_t *port, const uint8_t ipaddr[])
{
    old_mqtt_connect_data_t *old_data = &IotSetData.dev_info_t.old_mqtt_connect_data;
    old_mqtt_connect_data_t new_data = {0};
    time_t now = time(NULL);

    if (host && strlen(host) > 0 && port && *port != 0 && ipaddr) {
        strncpy(new_data.http_get_old_host, host, 119);
        new_data.http_get_old_host[119] = '\0';
        new_data.http_get_old_port = *port;
        memcpy(new_data.http_get_old_ipaddr, ipaddr, 4);
        new_data.http_get_host_valid = 1;
        if (( memcmp(&new_data, old_data, sizeof(old_mqtt_connect_data_t)) != 0) 
            || (now > (86400 + IotSetData.dev_info_t.mqtt_http_ip_date)))
        {
            memcpy(old_data, &new_data, sizeof(old_mqtt_connect_data_t));
            IotSetData.dev_info_t.mqtt_http_ip_date = get_local_midnight();
            reals.SetDataWrFlag.sBit.old_mqtt_connect = 1;
            ESP_LOGI(TAG, "iot_last_host_set: cache updated host=%s, port=%u, ip=%d.%d.%d.%d, mqtt_http_ip_date=%llu", 
                host, *port, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3], 
                IotSetData.dev_info_t.mqtt_http_ip_date);
        }
    } else {
        ESP_LOGE(TAG, "iot_last_host_set: invalid input, cache cleared");
    }
}


 // 先通过HTTP获取UTC时区和IP地址，如果失败则通过DNS解析获取IP地址
 uint8_t mqtt_client_UTC_IP_GET(login_info_t *mqtt_info) 
 {
     static bool first_request_status = false;
     time_t now = time(NULL);
     
     char url[128] = {0};
     uint64_t server_time = 0;
     uint8_t ret = 0;
     
     mqtt_info->now_time = 0;

     // 4G模式下仅当日的首次上电从HTTP获取，降低弱网情况下的网络重连流量消耗
     if (( NETIF_TYPE_PPP == mqtt_info->current_netif_id ) 
        && (( true == first_request_status ) 
            || (now <= (H24_HOUR_INTERVAL + IotSetData.dev_info_t.mqtt_http_ip_date))))
     {
         /*获取本地存储的HOST*/
         ret = iot_last_host_get(mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
         if( ret ) return ret;
     }
        
     // 先通过HTTP获取UTC时区和IP地址
     if(http_get_utc_time_and_ip(url, 128, &server_time) == 0){
         mqtt_info->now_time = server_time;
         
#if HTTP_UTC_TIME_VERSION == 2
         /*V2接口：返回连接域名*/
         ret = iot_get_cloud_dns(url, mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
#else
         /*V1接口，返回IP地址*/
         int n1 = sscanf(url, "%120[^:] %*[:]%hu",  mqtt_info->host, &mqtt_info->port); // 将输入的HOST地址解析出地址和端口
         int n2 = sscanf(mqtt_info->host, "%hhu.%hhu.%hhu.%hhu", &mqtt_info->ipaddr[0], &mqtt_info->ipaddr[1], \
                                                 &mqtt_info->ipaddr[2], &mqtt_info->ipaddr[3]); // 将输入的HOST地址解析出地址和端口
         ret = (n1 == 2 && n2 == 4) ? 1 : 0;
#endif

         /*缓存上次连接接口*/
         if (ret) {
            iot_last_host_set(mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
            first_request_status = true;
         }
     }

     if (!ret) {
         /*获取本地存储的HOST*/
         ret = iot_last_host_get(mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
            
         // 如果失败则通过DNS解析获取IP地址
         if(!ret) return iot_get_cloud_dns(mqtt_info->raw_url, mqtt_info->host, &mqtt_info->port, mqtt_info->ipaddr);
     }
     
     return ret;
 }


/*------------------------------------------------------------------------------
 Function: app_mqtt_client_tcp_connect
 -----------------------------------------------------------------------------*/
/**
  * @brief      通用 TCP创建
  * @param[in]  int *sock               
                const char *broker_uri  
                uint16_t port           
  * @param[out] None
  * @return     static uint64_t
  */
static uint64_t app_mqtt_client_tcp_connect(int *sock, const char *broker_uri, uint16_t port) 
{   
      struct addrinfo *address_info;                                               
      struct addrinfo hints = {0};
      char raw_uri[128] = {0};

      hints.ai_family = AF_UNSPEC;         /* 指定返回地址的协议簇，AF_INET(IPv4)、AF_INET6(IPv6)、AF_UNSPEC(IPv4 and IPv6) */
      hints.ai_socktype = SOCK_STREAM;     /* 设定返回地址的socket类型，流式套接字 tcp*/     
      if (broker_uri == NULL  || strlen(broker_uri) < 5)
      {
          ESP_LOGE(TAG,"broker_uri == NULL || model == NULL || sncode == 0");
          return 0;
      }

      ESP_LOGD(TAG,"Raw broker: '%s'", broker_uri);
      memcpy(raw_uri, broker_uri, strlen(broker_uri) % 128);

      const char *uri = strtok((char *)raw_uri, ":"); 
      const char *port_str = strtok(NULL, ":");
      if (port_str) {
          port = atoi(port_str);
      } else {
          ESP_LOGE(TAG,"broker uri port invaild (not value)");
      }
      ESP_LOGD(TAG,"broker: '%s', port: %d", uri, port);
          
      int SetTimeZone = 0;
      SetTimeZone = getaddrinfo(uri, NULL, &hints, &address_info); /* 解析服务器地址 */  
      if(SetTimeZone != 0 || address_info == NULL) {
          ESP_LOGE(TAG, "couldn't get hostname for `%s` "
                        "getaddrinfo() returns %d, addrinfo=%p", broker_uri, SetTimeZone, address_info);
          if (address_info) freeaddrinfo(address_info);              
          return 0;
      }

      struct in_addr addr4;
      if (address_info->ai_family == AF_INET) {
          addr4 = ((struct sockaddr_in *) (address_info->ai_addr))->sin_addr;
          ESP_LOGD(TAG, "broker IPv4 address: %s", inet_ntoa(addr4.s_addr));
      } else {
          ESP_LOGE(TAG, "Unsupported protocol family %d", address_info->ai_family);
          freeaddrinfo(address_info);
          return 0;
      }

      *sock = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);//int
      if( *sock < 0 )
      {
          ESP_LOGE(TAG, "socket create failed, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
          freeaddrinfo(address_info);
          return 0;
      }
      ESP_LOGI(TAG, "app_mqtt_client_tcp_connect : sockfd is %d", *sock);
      freeaddrinfo(address_info);

      /*检查套接字格式*/
      int type;
      socklen_t len = sizeof(type);
      if (getsockopt(*sock, SOL_SOCKET, SO_TYPE, &type, &len) == -1) {
          ESP_LOGE(TAG, "getsockopt failed");
          goto cleanup;
      } else if (type != SOCK_STREAM) {
          ESP_LOGE(TAG, "Socket type error(%d)", type);
          goto cleanup;
      }

    //     /* 启用 TCP Keep-Alive */
    //     int enable = 1;
    //     if (setsockopt(*sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
    //         ESP_LOGE(TAG, "Failed to set SO_KEEPALIVE, errno:%d, mean:%s", errno, strerror(errno));
    //         goto cleanup;
    //     }

      struct timeval opt_on = {
          .tv_sec = 5, 
          .tv_usec = 0,
      };

      // use netif name bind to device
      struct ifreq ifr;
      char *netif_key[NETIF_TYPE_MAX] = {NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP, NETIF_KEY_WIFI_AP};
#if !CONFIG_LWIP_NETIF_API
      esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[login_info.current_netif_id]), ifr.ifr_name);
#else
      if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[login_info.current_netif_id])), ifr.ifr_name);
#endif
   
      int ret = setsockopt(*sock, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(struct ifreq));
      if (ret < 0) {
          ESP_LOGE(TAG, "\"%s\" Unable to bind socket to specified interface: errno:%d, mean:%s", ifr.ifr_name, errno, strerror(errno));
      }

      if (setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &opt_on, sizeof(opt_on)) < 0) // 设置socket非阻塞
      { 
          ESP_LOGE(TAG, "socket setsockopt set failed");
          goto cleanup;
      } 

      ESP_LOGI(TAG, "connect '%s : %d' >> IPv4: %s", uri, port, inet_ntoa(addr4.s_addr));
      struct sockaddr_in dest_addr_ip4;
      dest_addr_ip4.sin_addr.s_addr = addr4.s_addr;
      dest_addr_ip4.sin_family = AF_INET;
      dest_addr_ip4.sin_port = htons(port);
      if (connect(*sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4)) < 0)//windy 后续的send需要此句柄login_info.sock
      {
          ESP_LOGE(TAG, "[sock=%d] Failed to connect", *sock);
          ESP_LOGE(TAG, "Socket connect failed %d: %s", errno, strerror(errno));
          goto cleanup;
      }
      
      return 1;

    cleanup:
      if (shutdown(*sock, SHUT_RDWR) == -1) {
          ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
      }
      if (close(*sock) == -1) {
          ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", *sock, errno, strerror(errno));
          if (errno == EBADF) {
              /*Bad file number*/
              *sock = -1;
          }
      } else {
          *sock = -1;
      }
      return 0;
}


/*------------------------------------------------------------------------------
 Function: iot_tcp_connect_host
 -----------------------------------------------------------------------------*/
/**
  * @brief      TCP连接主机
  * @param[in]  const char ipaddr[4] - 服务器IPv4地址数组 [192, 168, 1, 100]
  * @param[in]  uint16_t port - 端口号
  * @param[out] int *sock_fd - 返回的socket文件描述符
  * @return     uint8_t (1-成功, 0-失败)
  */
static uint8_t iot_tcp_connect_host(const char ipaddr[4], uint16_t port, int *sock_fd, int *sys_errno) //static
{
#if 0
	return app_mqtt_client_tcp_connect(sock_fd, IotSetData.dev_info_t.could_dns, port);
#else
    int sock = -1;
    struct sockaddr_in server_addr;
    
    // 参数验证
    if (!ipaddr || !sock_fd || port == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return 0;
    }
    
    *sock_fd = -1;
    
    // 创建socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket creation failed: errno %d (%s)", errno, strerror(errno));
        if(sys_errno) *sys_errno = errno;
        return 0;
    }
    
    ESP_LOGD(TAG, "Socket created: fd=%d", sock);
    
    // 验证socket类型
    int socket_type;
    socklen_t len = sizeof(socket_type);
    if (getsockopt(sock, SOL_SOCKET, SO_TYPE, &socket_type, &len) == -1) {
        ESP_LOGE(TAG, "getsockopt failed: errno %d (%s)", errno, strerror(errno));
        goto tcp_sock_cleanup;
    }
    
    if (socket_type != SOCK_STREAM) {
        ESP_LOGE(TAG, "Invalid socket type: %d", socket_type);
        goto tcp_sock_cleanup;
    }
    
    // 设置socket选项
    struct timeval timeout = {
        .tv_sec = 5,   // 5秒超时
        .tv_usec = 0,
    };
        
    // use netif name bind to device
    struct ifreq ifr;
    char *netif_key[NETIF_TYPE_MAX] = {NETIF_KEY_ETH, NETIF_KEY_WIFI_STA, NETIF_KEY_PPP, NETIF_KEY_WIFI_AP};
#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[login_info.current_netif_id]), ifr.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[login_info.current_netif_id])), ifr.ifr_name);
#endif

    int ret = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,  (void*)&ifr, sizeof(struct ifreq));
    if (ret < 0) {
        ESP_LOGE(TAG, "\"%s\" Unable to bind socket to specified interface: errno:%d, mean:%s", ifr.ifr_name, errno, strerror(errno));
        goto tcp_sock_cleanup;
    }    
    
    // 设置接收超时
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(TAG, "Failed to set SO_RCVTIMEO: errno %d (%s)", errno, strerror(errno));
        goto tcp_sock_cleanup;
    }

    // 设置发送超时
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGE(TAG, "Failed to set SO_SNDTIMEO: errno %d (%s)", errno, strerror(errno));
        goto tcp_sock_cleanup;
    }

#if 0    
    // 启用TCP Keep-Alive (可选)
    int keepalive = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_KEEPALIVE: errno %d (%s)", errno, strerror(errno));
        // 不影响连接，继续执行
    }
    
    // 设置Keep-Alive参数 (可选)
    int keepidle = 60;      // 60秒后开始发送keep-alive包
    int keepintvl = 10;     // keep-alive包间隔10秒
    int keepcnt = 3;        // 最多发送3个keep-alive包
    
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
#endif

    // 构建服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // 将IP地址数组转换为网络字节序
    uint32_t ip_addr = (uint32_t)ipaddr[0] << 24 |
                       (uint32_t)ipaddr[1] << 16 |
                       (uint32_t)ipaddr[2] << 8  |
                       (uint32_t)ipaddr[3];
    server_addr.sin_addr.s_addr = htonl(ip_addr);
    
    // 建立TCP连接
    ESP_LOGI(TAG, "Attempting to connect to %d.%d.%d.%d:%d", 
             ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3], port);
    
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Connection failed: errno %d (%s)", errno, strerror(errno));
        goto tcp_sock_cleanup;
    }
    
    // 连接成功，获取本地地址信息
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr *)&local_addr, &addr_len) == 0) {
        ESP_LOGI(TAG, "Local address: %s:%d", 
                 inet_ntoa(local_addr.sin_addr), ntohs(local_addr.sin_port));
    }
    
    ESP_LOGI(TAG, "Successfully connected to %d.%d.%d.%d:%d (fd=%d)", 
             ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3], port, sock);
    
    *sock_fd = sock;
    return 1;
    
tcp_sock_cleanup:
    if(sys_errno) *sys_errno = errno;
    if (shutdown(sock, SHUT_RDWR) == -1) {
        ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", sock, errno, strerror(errno));
    }
    if (close(sock) == -1) {
        ESP_LOGE(TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", sock, errno, strerror(errno));
        if (errno == EBADF) {
            /*Bad file number*/
            sock = -1;
        }
    } else {
        sock = -1;
    }
    return 0;

#endif    
}

/*------------------------------------------------------------------------------
 Function: iot_sync_host_time
 -----------------------------------------------------------------------------*/
/**
  * @brief      TCP连接获取服务器时间
  * @param[in]  int *sock_fd          
                const char *iot_type  
                uint64_t iot_sn       
                int *sys_errno        
  * @param[out] None
  * @return     static uint32_t
  */
static uint32_t iot_sync_host_time(int *sock_fd, const char *iot_type, uint64_t iot_sn, int *sys_errno) {

    if ( !sock_fd || (*sock_fd < 0) || (iot_type == NULL) || (iot_sn == 0)) {
        return 0;
    }

    int rx_len = 0;
    uint8_t rx_buffer[128] = {0};
    char dev_str[128] = {0};
    snprintf(dev_str, sizeof(dev_str), "%s%013llu", iot_type, iot_sn);

    int tx_len = 4 + strlen(dev_str);
    uint8_t *tx_buffer = iot_calloc(tx_len); // 分配发送缓存 4字节头 + 设备信息字符串(类型+SN字符串)
    if (tx_buffer == NULL) {
        close(*sock_fd);
        shutdown(*sock_fd,SHUT_RDWR);
        *sock_fd = -1;
        ESP_LOGE(TAG, "tcp get time cmd memony (%dbytes) malloc failed", tx_len);
        return 0;
    }

    tx_buffer[0] = 0x00;
    tx_buffer[1] = 0x01;
    tx_buffer[2] = (strlen(dev_str) >> 8);  // 负载长度高8bit
    tx_buffer[3] = (strlen(dev_str));       // 负载长度低8bit
    memcpy(tx_buffer + 4, dev_str, strlen(dev_str));

    /* tcp发送数据 */
    if (send(*sock_fd, tx_buffer, tx_len, 0) < 0) {
        shutdown(*sock_fd,SHUT_RDWR);
        close(*sock_fd);
        *sock_fd = -1;
        free(tx_buffer);
        if(sys_errno) *sys_errno = errno;
        ESP_LOGE(TAG, "tcp Error occurred during sending: errno %d: %s", errno, strerror(errno));
        return 0;
    } else {
        free(tx_buffer);
    }

    /* 等待接收tcp数据 - 5s超时 */
    uint32_t time = 0;
    rx_len = recv(*sock_fd, rx_buffer, sizeof(rx_buffer),  0);
    if (rx_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ESP_LOGW(TAG, "Socket receive timeout(errno %d: %s)", errno, strerror(errno));
        } else {
            ESP_LOGE(TAG, "tcp recv error: errno %d: %s", errno, strerror(errno));
        }
    } else if (rx_len == 0) {
        ESP_LOGE(TAG, "tcp disconnected %d: %s", errno, strerror(errno));
    } else {

        /* 接收到主机数据 */
        host_time_t  nowtime;
        nowtime.uword = 0;
        if (rx_buffer[0] == 0x00 && rx_buffer[1] == 0x01) {
            int len = (((uint16_t)rx_buffer[2]<<8) | rx_buffer[3]);
            if (len == 4) {
                memcpy(nowtime.byte, rx_buffer + 4, len);
                time = htonl(nowtime.uword);
                ESP_LOGI(TAG, "host time: %lu", time);
            }
        }
    }
    if(sys_errno) *sys_errno = errno;
    shutdown(*sock_fd,SHUT_RDWR);
    close(*sock_fd);
    *sock_fd = -1;
    return time; /* 返回获取到的主机时间 */
}

static uint64_t iot_login_password(uint32_t now_time, const char *iot_type, uint64_t iot_sn, uint64_t safetyCode) {
    char iot_string[64] = {0};
    snprintf(iot_string, sizeof(iot_string), "%s%013llu", iot_type, iot_sn);
    uint64_t pwd = CreateEncryptPassword(iot_string, safetyCode, now_time); /* 计算MQTT登录密码 */
    return pwd;
}

static uint8_t iot_mqtt_login(login_info_t *login) 
{
    if (login == NULL) return 0;

#ifdef CONFIG_MQTT1_TCP_ENCRYPT_ENABLE	
    char host[150] = {0};
    if ( 1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable ) {
	    memcpy(host, "mqtts://", sizeof("mqtts://"));
    } else {
	    memcpy(host, "mqtt://", sizeof("mqtt://"));
    }
#else
	char host[150] = "mqtt://";
#endif	

    char username[64] = {0};
    char password[100] = {0};
    uint16_t keepalive_dafault = 0;
    
    if ( NETIF_TYPE_PPP == login->current_netif_id ) {
        keepalive_dafault = (IotSetData.dev_info_t.mqtt_heartbeat > 0) ? IotSetData.dev_info_t.mqtt_heartbeat : (60*5);   // 4G模式下默认为5分钟
    } else {
        keepalive_dafault = 120;     // 默认为60s
    }

    char password_extend1[10] = {0};
#ifdef CONFIG_MQTT_CONNECT_EXTEND_CHL
    /*MQTT CONNECT(连接认证)扩展 : IOT物联网的联网方式*/
    if ( NETIF_TYPE_WIFI_STA == login->current_netif_id ) {
        snprintf(password_extend1, sizeof(password_extend1), ",1:%u", MQTT_EXTEND_CHL_WIFI);
    } else if ( NETIF_TYPE_ETH == login->current_netif_id ) {
        snprintf(password_extend1, sizeof(password_extend1), ",1:%u", MQTT_EXTEND_CHL_ETHERNET);
    } else if ( NETIF_TYPE_PPP == login->current_netif_id ) {
        snprintf(password_extend1, sizeof(password_extend1), ",1:%u", MQTT_EXTEND_CHL_PPP_4G);
    } else {
        memset(password_extend1, 0, sizeof(password_extend1));
    }
#endif

    char password_extend2[10] = {0};
#if 0
    /*非通用扩展：仅限早期1代IOT控制器升级到2代PBOX能量盒子的固件升级场景，且仅针对当时已经出货流通在外的产品*/
    snprintf(password_extend2, sizeof(password_extend2), ",IOT@%08llu", login->password2);
#endif

    strcat(host, login->host);
    snprintf(username, sizeof(username), "%s%013llu", login->iot_type, login->iot_sn);
	snprintf(password, sizeof(password), "%08llu%s%s", login->password, password_extend1, password_extend2);
    
#if 0
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri  = host,               /*!< MQTT host */
        .port = login->port,        /*!< MQTT port */
        .username = username,       /*!< MQTT username */
        .password = password,       /*!< MQTT password */
        .client_id = username,      /*!< MQTT clientId */
        .disable_auto_reconnect = false, /*!< MQTT auto reconnect  */
        .keepalive = 50,            /* 50s */
    };
#endif

    mqtt_cfg.broker.address.uri = host;                             /*!< MQTT host */
    mqtt_cfg.broker.address.port = login->port;
#if !MQTT_DEBUG
    mqtt_cfg.credentials.username = username;                       /*!< MQTT username */        
    mqtt_cfg.credentials.authentication.password = password;        /*!< MQTT password */
#endif
    mqtt_cfg.credentials.client_id = username;                      /*!< MQTT clientId */
    mqtt_cfg.network.disable_auto_reconnect = true;                 /*!< MQTT auto reconnect(应用层自己完全管理重连，应当关掉组件层自动重连) */
    mqtt_cfg.network.reconnect_timeout_ms = 50;                     // 默认值为10000ms，关闭组件层自动重连后可以调小来优化错误断连后esp_mqtt_client_stop时长
    mqtt_cfg.network.timeout_ms = 20000;                            // 默认值为10000ms，网络操作超时 (连接/发送/接收), 从 10000ms 增加到 20000ms
    mqtt_cfg.session.keepalive = keepalive_dafault;                 /*!< MQTT keep alive */
#ifdef CONFIG_MQTT1_TCP_ENCRYPT_ENABLE
    if ( 1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable ) {
        mqtt_cfg.credentials.authentication.certificate = (const char *)iot_cert_ptr;  //客户端证书
        mqtt_cfg.credentials.authentication.key = (const char *)private_key_ptr;  //客户端密钥
        // mqtt_cfg.broker.verification.certificate = (const char *)ca_cert_ptr,  //服务器证书(服务器部署后再启用) 
    } else {
        mqtt_cfg.credentials.authentication.certificate = NULL;
        mqtt_cfg.credentials.authentication.key = NULL;
        mqtt_cfg.broker.verification.certificate = NULL;
    }
#endif		
#ifdef CONFIG_MQTT_CLIENT_SEND_BY_ENQUEUE
    /* 控制应用层允许积压的总量，实际消息数据会优先放到 PSRAM */
    mqtt_cfg.outbox.limit = 16 * 1024;
#endif

    ESP_LOGI(TAG, "********mqtt login info*********");
	if(mqtt_cfg.broker.address.uri) {
		ESP_LOGI(TAG, "hosturl:  %s",  mqtt_cfg.broker.address.uri);
	}
    ESP_LOGI(TAG, "hostport: %ld",  mqtt_cfg.broker.address.port);
    ESP_LOGI(TAG, "now_time：%llu ", login->now_time);
    ESP_LOGI(TAG, "ClientId: %s",  mqtt_cfg.credentials.client_id);
    ESP_LOGI(TAG, "username: %s",  mqtt_cfg.credentials.username);
    ESP_LOGI(TAG, "password: %s",  mqtt_cfg.credentials.authentication.password);
    ESP_LOGI(TAG, "ifr_name: %s",   mqtt_cfg.network.if_name->ifr_name);
    ESP_LOGI(TAG, "keepalive: %d",  mqtt_cfg.session.keepalive);
    
    if (login->mqtt_client == NULL) {
        login->mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        if (login->mqtt_client != NULL) {
            /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
            ESP_ERROR_CHECK(esp_mqtt_client_register_event(login->mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, login));
            ESP_ERROR_CHECK(esp_mqtt_client_start(login->mqtt_client));
            ESP_LOGI(TAG, "init mqtt client and start");
            return 1;
        }
    } else {
        ESP_ERROR_CHECK(esp_mqtt_set_config(login->mqtt_client, &mqtt_cfg));
        ESP_ERROR_CHECK(esp_mqtt_client_start(login->mqtt_client));
		ESP_LOGI(TAG, "mqtt client start");
        return 1;
    }

    ESP_LOGI(TAG, "mqtt client start failed");
    return 0;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
//    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    login_info_t *login_info = (login_info_t *)handler_args;
    int msg_id = 0;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "mqtt connected");
            login_info->mqtt_connect_fail = 0;
            
			memcpy(&mqtt_client_dump, client, sizeof(struct esp_mqtt_client_dump));
            
#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
			iot_bind_check();										// 检查系统绑定信息
			bind_dev_list.need_bind = 1;							// 每次登录都要绑定一次信息
			bind_dev_list.tcp_bind_flag = false;                    // 首次绑定使用tcp绑定
			tcp_bind_dev();											// 通过tcp方式绑定设备
#endif

            snprintf(login_info->sub_topic, sizeof(login_info->sub_topic), SUBSCRIBE_TOPIC, login_info->dev_type, login_info->dev_sn);
            snprintf(login_info->pub_topic, sizeof(login_info->pub_topic), PUBLISH_TOPIC,   login_info->dev_type, login_info->dev_sn);
			char all_public_topic[100];
			snprintf(all_public_topic, sizeof(all_public_topic), ALL_PUBLIC_TOPIC,  login_info->dev_type);

            msg_id = esp_mqtt_client_subscribe(client, login_info->sub_topic, 0);	// 订阅接收服务器命令主题
            ESP_LOGI(TAG, "subscribe receive topic, msg_id=%d topic: %s", msg_id, login_info->sub_topic);
			ESP_LOGI(TAG, "data publish topic: %s, and waiting for MQTT_EVENT_SUBSCRIBED", login_info->pub_topic); // 数据发送主题

#if 0   // 公共主题当前未使用
            msg_id = esp_mqtt_client_subscribe(client, all_public_topic, 0);		// 订阅公共主题
            ESP_LOGI(TAG, "subscribe public topic, msg_id=%d topic: %s", msg_id, all_public_topic);
#endif
            
			mqtt_sub_timeout_timer_start();
            
            break;

        case MQTT_EVENT_DISCONNECTED: // MQTT链接断开事件
			mqtt_sub_timeout_timer_stop();
            ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
            login_info->mqtt_is_login = 0;
            reals.last_disconnect_cloud_time = reals.now;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.MqttConnectMode = 0;

            /*on disconnection event, if the disconnection is due to an error, 
            the error_handle field of the event data will be non-NULL, otherwise it will be NULL.*/
            if (event->error_handle == NULL) {
                // 正常断开：协议层关闭或客户端主动断开
                Mqtt_Machine_Pause_Time = 10 * 1000;
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .svc_timestamp = login_info->now_time,
                    .uptime_seconds = (Mqtt_Machine_Connect_Time > 0) ? ((xTaskGetTickCount() * portTICK_PERIOD_MS - Mqtt_Machine_Connect_Time) / 1000) : 0,
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .password = login_info->password,
                    .reason = MQTT_DISCONNECT,
                    .network_type = login_info->current_netif_id,
                    .retry_delay_ms = (Mqtt_Machine_Pause_Time < UINT16_MAX) ? Mqtt_Machine_Pause_Time : UINT16_MAX,
                };
                Mqtt_Log_Generate(&record);
                Mqtt_Machine_Connect_Time = 0;
#endif
            } else {
                // 异常断开：MQTT 错误类型
                mqtt_log_error_details(event);
    
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
                // MQTT日志生成
                mqtt_conn_fail_log_t record = {
                    .svc_timestamp = login_info->now_time,
                    .uptime_seconds = (Mqtt_Machine_Connect_Time > 0) ? ((xTaskGetTickCount() * portTICK_PERIOD_MS - Mqtt_Machine_Connect_Time) / 1000) : 0,
                    .use_mqtts = (1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) ? 1 : 0,
                    .password = login_info->password,
                    .reason = (event->error_handle->error_type) ? MQTT_FAIL_MQTT_UNACCEPTABLE : ((Mqtt_Machine_Connect_Time) ? MQTT_POLL_TIMEOUT : MQTT_CONNECT_EOF_OR_TIMEOUT),
                    .sys_errno = event->error_handle->esp_transport_sock_errno,
                    .error_type = event->error_handle->error_type,
                    .connect_return_code = event->error_handle->connect_return_code,
                    .esp_tls_last_esp_err = event->error_handle->esp_tls_last_esp_err,
                    .esp_tls_stack_err = event->error_handle->esp_tls_stack_err,
                    .esp_tls_cert_verify_flags = event->error_handle->esp_tls_cert_verify_flags,
                    .network_type = login_info->current_netif_id,
                    .retry_delay_ms = (Mqtt_Machine_Pause_Time < UINT16_MAX) ? Mqtt_Machine_Pause_Time : UINT16_MAX,
                };
                Mqtt_Log_Generate(&record);
                Mqtt_Machine_Connect_Time = 0;
#endif
            }            
            
            // 重新开始连接
            if(login_step != STEP_MQTT_WAITING) login_step = STEP_MQTT_RESTART;
            
            break;

        case MQTT_EVENT_SUBSCRIBED: /* 订阅完成 */
			mqtt_sub_timeout_timer_stop();
            ESP_LOGW(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

            // 该逻辑意图未知，新项目禁用
            // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.MqttConnectMode = login_info->current_netif_id + 1;
            reals.last_login_cloud_time = reals.now;
            Mqtt_Machine_Connect_Time = xTaskGetTickCount() * portTICK_PERIOD_MS;

            if ( 0 == login_info->mqtt_is_login ) {
                if(NETIF_TYPE_PPP == login_info->current_netif_id) {
#ifdef CONFIG_MQTT_PAYLOAD_TLV_ENABLE
                    // 4G网络下发送网络数据
                    mqtt_tlv_report_4g_info((const char *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.IMEI, 
                                            (const char *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.IMSI, 
                                            (const char *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.ICCID);
#endif
                } else {
#ifdef HTTP_CHECK_FIRMWARE_UPDATE              
                    // 连接到MQTT后，触发一次检查是否有新版本可升级
                    update_firmware_check();
#endif
                }
            }

            login_info->mqtt_is_login = 1;
    
            break;

        case MQTT_EVENT_UNSUBSCRIBED: /* 取消订阅 */
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            // ESP-MQTT 中一个已知的行为：当接收到的消息超过 MQTT 缓冲区大小时，会被拆分成多个 MQTT_EVENT_DATA 事件分发，后续分片中 event->topic 为 NULL
            if (event->topic == NULL) {
#ifdef CONFIG_MQTT_RECV_CALLBACK_ENABLE
                // TODO：后续分片中 event->topic 为 NULL，按默认sub_topic解析，以正确处理拼包逻辑
                mqtt_recv_callback(login_info->mqtt_msg_queue, (uint8_t *)event->data, event->data_len);
#else  
                ESP_LOGE(TAG, "recv data error (no topic)");
#endif
                break;
            }
            
            ESP_LOGD(TAG, "topic: %s recv message", event->topic);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, event->data, event->data_len, ESP_LOG_DEBUG);
            reals.net_frame_rx_counter++;
            
            if (!strncmp(event->topic, ALL_PUBLIC_TOPIC, strlen(ALL_PUBLIC_TOPIC))) {
                // 公共主题-数据接收
                ESP_LOGI(TAG, "PUBLIC_TOPIC(%s) : recv message(len = %u)", event->topic, event->data_len);
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, event->data, event->data_len, ESP_LOG_INFO);
            } else if (!strncmp(event->topic, login_info->sub_topic, strlen(login_info->sub_topic)))  {
                // 设备私有主题-数据接收
                if ( login_info->mqtt_msg_queue == NULL) {
                    ESP_LOGE(TAG, "mqtt msg queue is null");
                    break;
                }
#ifdef CONFIG_MQTT_RECV_CALLBACK_ENABLE
                // 回调处理，含拼包
                mqtt_recv_callback(login_info->mqtt_msg_queue, (uint8_t *)event->data, event->data_len);
#else                
                queue_msg_t mqtt_msg;
                mqtt_msg.msg_len = event->data_len; //
                mqtt_msg.msg_ptr = (uint8_t *)iot_calloc(event->data_len);
                if (mqtt_msg.msg_ptr == NULL) {
                    ESP_LOGI(TAG, "mqtt recv msg malloc (%dbytes) failed", mqtt_msg.msg_len);
                    break;
                }

                memcpy(mqtt_msg.msg_ptr, event->data, event->data_len);
                if (xQueueSendToBack((QueueHandle_t)login_info->mqtt_msg_queue, &mqtt_msg, pdMS_TO_TICKS(100)) != pdPASS) 
				{
                    free(mqtt_msg.msg_ptr);
                    ESP_LOGE(TAG, "mqtt message handle queue full"); // MQTT发送过来的消息太多,就会处理不及时队列满
                }
#endif                
            } else {
                // 未知主题
                ESP_LOGE(TAG, "mqtt unkonw topic");
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            
            /*Error event always accompanies a disconnect event, 
            i.e. MQTT_EVENT_DISCONNECTED will be called right after MQTT_EVENT_ERROR*/
            if(event->error_handle) ESP_LOGE(TAG, "ERROR event received, type=%d", event->error_handle->error_type);
            
            break;

        case MQTT_EVENT_DELETED:
            ESP_LOGE(TAG, "MQTT_EVENT_DELETED, msg_id=%d", event->msg_id);
            break;
            
        default: ESP_LOGI(TAG, "Other event id:%d", event->event_id); break;
    }
}


/*------------------------------------------------------------------------
*@Function： pack_data_head
互联网载荷协议V1.2,帧头组合
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
static void pack_data_head(uint8_t ver, uint8_t *head, uint8_t cause, uint16_t cycle, uint8_t total, uint8_t seq) {
    head[0] = ver; // 0x01 modbus格式，0x02字符串格式
    head[1] = 0xF8;
    head[2] = cause;
    head[3] = cycle ;
    head[4] = (cycle >> 8) & 0xFF;
    head[5] = seq;
    head[6] = total;
    if ( NETIF_TYPE_WIFI_STA == login_info.current_netif_id ) {
        head[7] = iot_wifi_ap_rssi();
    } else if ( NETIF_TYPE_PPP == login_info.current_netif_id ) {
        head[7] = iot_4g_rssi();
    } else {
        head[7] = 0;
    }
    head[8] = 0;
    head[9] = 0;
}

#if 0
int wifi_sta_config_flag;

void wifi_sta_config_check(uint8_t *md_msg)
{
	#define WIFI_STA_CFG_START_ADDR		12001		// wifi station配置开始地址
	#define WIFI_STA_CFG_END_ADDR		12033		// wifi station配置结束地址
	#define WIFI_STA64_CFG_END_ADDR		12049		// wifi station配置结束地址
	int code = md_msg[1];

	if (code == 0x06 || code == 0x10)
	{
		uint16_t reg_nums = 1;
		uint16_t start_addr = ((uint16_t)md_msg[2]<<8) | md_msg[3];

		if(code == 0x10) 
		{
			reg_nums = ((uint16_t)md_msg[4]<<8) | md_msg[5];
		}
		else if(code == 0x06) 
		{
			reg_nums = 1;
		}

		uint16_t end_addr = start_addr + reg_nums - 1;

		/* 检测是否是wifi station配置 */
		if (!((start_addr > WIFI_STA_CFG_END_ADDR) || (end_addr < WIFI_STA_CFG_START_ADDR))) {
			wifi_sta_config_flag = 1;
		}
	}
}

#endif

/* 将原有处理逻辑抽成函数，负责：格式检查、Modbus 解析、mqtt 回复、can 转发与位置列表处理。
 * 手机APP先使用MQTT V1.0载荷读取
 * IOT返回V1.2载荷协议,告知手机APP,IOT已经支持到V1.2版本
 * 下一次手机使用V1.2载荷协议开始交互
 * 参数说明：
 *  - pdata/plen: 接收到的 Modbus 原始帧
 *  - Frame_type: 载荷协议版本（1 或 其他）
 *  - income_msg_ptr: 原始 mqtt/income 缓冲
 */
static void process_modbus_payload_from_mqtt(const uint8_t *pdata, int plen, int Frame_type, uint8_t *income_msg_ptr)
{
    if (!pdata || plen <= 0) {
        ESP_LOGE(TAG, "process_modbus_payload_from_mqtt : invalid cfg.");
        return;
    }

    int rst = Modbus_Format_Check(pdata, plen);
    if (rst < 0) {
        ESP_LOGE(TAG, "process_modbus_payload_from_mqtt : Modbus_Format_Check ERROR.");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, pdata, plen, ESP_LOG_ERROR);
        return;
    }

    uint8_t md_addr = pdata[0];
    can_cmd_queue_struct can_cmd = { NULL, MODBUS_TO_CAN_MAX_NUM, 0 };

#ifdef CONFIG_CAN_PORT_ENABLE
    if ((rst == 0x06 || rst == 0x10) && can_cmd_queue) {
        can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
        if (!can_cmd.cmd) {
            ESP_LOGE(TAG, "mqtt to can malloc failed");
            return;
        }
    }
#endif

    reg_position_list_t *position_list = NULL;
    uint8_t rsp_buf[270] = {0};
    int rsp_len = 0;
    int len = 0;

    if (1 == Frame_type) {
        rsp_buf[0] = 0x01;
        len = Modbus_Slave(pdata, plen, &rsp_buf[1], can_cmd.cmd, &can_cmd.num, MD_CHL_WIFI_CLOUD, &position_list);
        rsp_len = len + 1;
    } else {
        uint16_t flag = 0;
        if (income_msg_ptr && income_msg_ptr[2] == 0x0F) {
            flag = ((uint16_t)income_msg_ptr[4] << 8) | income_msg_ptr[3];
        }
        pack_data_head(0x01, rsp_buf, income_msg_ptr ? income_msg_ptr[2] : 0, flag, 0, 0);
        len = Modbus_Slave(pdata, plen, &rsp_buf[10], can_cmd.cmd, &can_cmd.num, MD_CHL_WIFI_CLOUD, &position_list);
        rsp_len = len + 10;
    }

    if (len > 0) {
        iot_mqtt_send(rsp_buf, rsp_len);

        /* modbus指令转换为can指令发送到队列 */
        if (can_cmd.cmd) 
        {
#ifdef CONFIG_CAN_PORT_ENABLE
            sys_new_can_data_resend(&can_cmd, md_addr);
#else
            free(can_cmd.cmd);
#endif
        }

        if (NULL != position_list) {
#ifdef CONFIG_UART_PORT_ENABLE
            sys_new_position_and_transmit(position_list);
#else
            delete_set_position(position_list);  
#endif
        }
    } else {
        if (can_cmd.cmd) {
            free(can_cmd.cmd);
        }
        if (NULL != position_list) {
            delete_set_position(position_list);  
        }
    }
}

/*------------------------------------------------------------------------------
 Function: iot_mqtt_msg_handle
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT接收数据处理
  * @param[in]  esp_mqtt_client_handle_t client  
                QueueHandle_t msg_queue          
                const char *rsp_topic            
  * @param[out] None
  * @return     void i
  */
 void iot_mqtt_msg_handle(esp_mqtt_client_handle_t client, QueueHandle_t msg_queue, const char *rsp_topic ) 
{
    uint8_t Frame_type = 0;
    uint16_t plen = 0;
    const uint8_t *pdata = NULL;

    if ((client == NULL) || (msg_queue == NULL)) { /* mqtt 消息队列不存在 */
        ESP_LOGE(TAG, "client or mqtt msg queue");
        return ; /* 退出 */
    }

    queue_msg_t income = {NULL, 0};
    if(xQueueReceive(msg_queue, &income, 0) == pdTRUE )
	{        
		/* 风暴模式处理 */
	   	if (thunder_mode_handle(income.msg_ptr, income.msg_len) < 1) {
			if (income.msg_ptr) {
				free(income.msg_ptr);
			}
			return;
		} else if ((income.msg_ptr[0] == 0x01) && (income.msg_ptr[1] < 0xF8)) {
            // 0 ~ 247时：表示Modbus从机地址，APP、服务端按V1.0载荷协议解码数据
            pdata = (const uint8_t *)(income.msg_ptr + 1);
            plen = (income.msg_len - 1);
			Frame_type = 1;
        } else if ((income.msg_ptr[0] == 0x01) && income.msg_ptr[1] == 0xF8) {
            // 248 (0xF8)：表示扩展的V1.2版载荷协议
            pdata = (const uint8_t *)(income.msg_ptr + 10);
            plen = (income.msg_len - 10);
			Frame_type = 2;
        } else if ((income.msg_ptr[0] == UDT_PROT_MARK) && (income.msg_ptr[1] == UDT_PROT_VER)) {
            // 服务器通用数据通道协议
			ESP_LOGW(TAG, "udt received server data and relay it to device, data length: %d, slave_addr: %d, funcode: %d",
					income.msg_len, income.msg_ptr[10], income.msg_ptr[11]);

#ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
            /*数据通道*/
			udt_recv_push(income.msg_ptr, income.msg_len, UDT_MODE_SVC);
#endif
			Frame_type =3;

		} else if ((income.msg_len > 5) && (income.msg_ptr[0] == 0x00) && (income.msg_ptr[1] == 0x09)) {  
#ifdef CONFIG_ENABLE_HTTP_OTA
            plen = (income.msg_ptr[2] << 8) | income.msg_ptr[3];
            ESP_LOGW (TAG, "https_ota_cmd_parse");
            https_ota_cmd_parse((char *)&income.msg_ptr[4], plen); // https 升级命令
#endif            
            pdata = NULL; 
            plen = 0;
			Frame_type = 4;
        } else if ((income.msg_ptr[0] == MQTT_TLV_MARK) 
            && ((income.msg_ptr[1] == MQTT_TLV_TAG_FLAG) || (income.msg_ptr[1] == MQTT_TLV_FIELD_FLAG))) {
            // MQTT载荷中传输TLV报文
			ESP_LOGW(TAG, "mqtt_tlv_protocol received server data, data length: %d", income.msg_len);

#ifdef  CONFIG_MQTT_PAYLOAD_TLV_ENABLE
            /*MQTT TLV*/
			mqtt_tlv_protocol_handle(income.msg_ptr, income.msg_len);
#endif
			Frame_type = 5;
		}	        
		else 
        {
            ESP_LOGE (TAG, "iot_mqtt_msg_handle: unknown data!");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, income.msg_ptr, income.msg_len, ESP_LOG_ERROR);
        }

        // Modbus协议响应
		if((1 == Frame_type) || (2 == Frame_type)) {
            /* 将原大段逻辑替换为单独函数调用*/
            process_modbus_payload_from_mqtt(pdata, plen, Frame_type, income.msg_ptr);
        }

        iot_free(income.msg_ptr);
    }
}


const char *iot_mqtt_get_host(void) {
    return login_info.raw_url;
}

void iot_mqtt_new_host(const char *new_host) {
    memcpy(login_info.raw_url, new_host, sizeof(login_info.raw_url));
    Mqtt_Restart_Flag = true;
}

///////////
void iot_mqtt_new_dev(const char *dev_type, uint64_t dev_sn) {
    memcpy(login_info.dev_type, dev_type, sizeof(login_info.dev_type));
    login_info.dev_sn = dev_sn;
    Mqtt_Restart_Flag = true;

#ifdef CONFIG_OPEN_MQTT_ENABLE
    open_mqtt_new_dev(dev_type, dev_sn);
#endif
}

void iot_mqtt_new_iot(const char *iot_type, uint64_t iot_sn, uint64_t safety_code) {
    memcpy(login_info.iot_type, iot_type, sizeof(login_info.iot_type));
    login_info.iot_sn     = iot_sn;
    login_info.safetyCode = safety_code;
    Mqtt_Restart_Flag = true;
}

void iot_mqtt_delete(void) 
{
    login_step = STEP_MQTT_EXIT;
}

void iot_mqtt_start(void) {
    /* MQTT登录处理任务 */
    exit_flag = 0;
    Mqtt_Restart_Flag = true;
}
 
void set_mqtt_stop(void) 
{
    /*MQTT断开*/
    mqtt_exit_process();
	exit_flag = 1;
}

void iot_mqtt_wait(uint32_t wait_time) {
    /* MQTT延迟登录 */
    if ( Mqtt_Restart_Flag ) {
        Mqtt_Restart_Flag = false;
        login_step = STEP_MQTT_RESTART;
    }

    Mqtt_Machine_Pause_Time = wait_time;
}

void set_mqtt_heartbeat(uint16_t heartbeat)
{
    if ( heartbeat != IotSetData.dev_info_t.mqtt_heartbeat )
    {
        ESP_LOGW(TAG, "mqtt_heartbeat to: %u", heartbeat);
        IotSetData.dev_info_t.mqtt_heartbeat = heartbeat;
        reals.SetDataWrFlag.sBit.mqtt_heartbeat = 1;

        // 当前4G联网情况下立即重新登录生效
        if ( NETIF_TYPE_PPP == login_info.current_netif_id ) {
            iot_mqtt_start();
#ifdef CONFIG_OPEN_MQTT_ENABLE
            if (1 == IotSetData.dev_info_t.open_mqtt_enable.enable) open_mqtt_start();
#endif            
        }
    }
}

uint8_t iot_mqtt_login_state(void) 
{
    return login_info.mqtt_is_login; /* MQTT连接状态 */
}

/**
 * @brief 当设备有以下特定事件发生时,IOT主动发布事件信息给到后台
 * - 1、AC输出变化
 * - 2、DC输出变化
 * - 3、PV充电变化
 * - 4、电网接入变化
 * - 5、SOC变化
 * 这些事件针对整个系统而不是针对哪一台设备
 *
 * @return 无
 */
int iot_mqtt_dev_event_ann(void) {

    int ret = 0;
    
	/*wifi未连接或mqtt未登录*/
	if(0 == login_info.mqtt_is_login) 
    {
        old_dev_ctrl.all = 0;
        old_dev_soc = 0;
        return ret; // MQTT登录断开不在进行事件推送
    }
    
    uint8_t buf[100] = {0};
	uint16_t now_dev_soc = top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg00100_AppPage1.soc;				// 当前系统soc
	device_ctrl now_dev_status = top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg00100_AppPage1.ctrl_status;	// 当前系统开关状态

	/* 检测系统soc与开关状态是否发生变化 */
	if ((now_dev_status.all != old_dev_ctrl.all) && (now_dev_soc != old_dev_soc))
	{
        old_dev_ctrl.all = now_dev_status.all;
        old_dev_soc = now_dev_soc;

		/* 上报给后台的数据需要使用MQTT负载协议Payload V1.2,(参考负载协议) */
		pack_data_head(0x01, buf, PUB_EVEVT, 0, 0, 0);
		buf[10] = 0x01;
		buf[11] = 0x49;

		/* 填充系统开关状态与soc */
        snprintf((char *)&buf[12], sizeof(buf)-12, "\"ac_sw=%01d,dc_sw=%01d,pv_sw=%01d,grid_sw=%01d,soc=%d\"",
				 old_dev_ctrl.bit.ac_on,
				 old_dev_ctrl.bit.dc_on,
				 old_dev_ctrl.bit.pv_on,
				 old_dev_ctrl.bit.grid_on,
				 old_dev_soc);
        
        ESP_LOGW(TAG, "iot_mqtt_dev_event_ann: %s ",(char *)&buf[12]);
        
		uint16_t data_len = 2 + strlen((const char*)&buf[12]);// + 1;不使用空字符
		uint16_t crc16 = crc16_modbus_calc(&buf[10], data_len);
		buf[10 + data_len + 0] = crc16 & 0xFF;
		buf[10 + data_len + 1] = (crc16>>8) & 0xFF;
        
        iot_mqtt_send(buf, 10+data_len+2);
        ret = 10+data_len+2;
	}
	else if (now_dev_soc != old_dev_soc)
	{
        old_dev_soc = now_dev_soc;

		/* 上报给后台的数据需要使用MQTT负载协议Payload V1.2,(参考负载协议) */
		pack_data_head(0x01, buf, PUB_EVEVT, 0, 0, 0);
		buf[10] = 0x01;
		buf[11] = 0x49;

		/* 填充系统开关状态与soc */
        snprintf((char *)&buf[12], sizeof(buf)-12, "\"soc=%d\"", old_dev_soc);
        
        ESP_LOGW(TAG, "iot_mqtt_dev_event_ann: %s ",(char *)&buf[12]);
        
		uint16_t data_len = 2 + strlen((const char*)&buf[12]);// + 1;不使用空字符
		uint16_t crc16 = crc16_modbus_calc(&buf[10], data_len);
		buf[10 + data_len + 0] = crc16 & 0xFF;
		buf[10 + data_len + 1] = (crc16>>8) & 0xFF;
        
        iot_mqtt_send(buf, 10+data_len+2);
        ret = 10+data_len+2;
	}
	else if (now_dev_status.all != old_dev_ctrl.all)
	{
        old_dev_ctrl.all = now_dev_status.all;

		/* 上报给后台的数据需要使用MQTT负载协议Payload V1.2,(参考负载协议) */
		pack_data_head(0x01, buf, PUB_EVEVT, 0, 0, 0);
		buf[10] = 0x01;
		buf[11] = 0x49;

		/* 填充系统开关状态与soc */
        snprintf((char *)&buf[12], sizeof(buf)-12, "\"ac_sw=%01d,dc_sw=%01d,pv_sw=%01d,grid_sw=%01d\"",
				 old_dev_ctrl.bit.ac_on,
				 old_dev_ctrl.bit.dc_on,
				 old_dev_ctrl.bit.pv_on,
				 old_dev_ctrl.bit.grid_on);
        
        ESP_LOGW(TAG, "iot_mqtt_dev_event_ann: %s ",(char *)&buf[12]);
        
		uint16_t data_len = 2 + strlen((const char*)&buf[12]);// + 1;不使用空字符
		uint16_t crc16 = crc16_modbus_calc(&buf[10], data_len);
		buf[10 + data_len + 0] = crc16 & 0xFF;
		buf[10 + data_len + 1] = (crc16>>8) & 0xFF;
        
        iot_mqtt_send(buf, 10+data_len+2);
        ret = 10+data_len+2;
	}

     return ret;
}

// 调试上报使能
//#define MQTT_ANNOUNCE_DEBUG 

#ifdef MQTT_ANNOUNCE_DEBUG

// 临时调试使用
static void iot_mqtt_debug_announce(void)
{
//    if ( 1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit._4g )
//    {
//        mqtt_tlv_report_4g_info((const char *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.IMEI, 
//                                (const char *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.IMSI, 
//                                (const char *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.ICCID);
//    }
//    modbus_tlv_mqtt_announce(System_TLV_Data_ctx, System_TLV_Data_ctx_total, 1024);
//    mqtt_tlv_request_server_and_dst(22737358, 114412014);
    mqtt_tlv_request_server_and_dst(0, 0);
}

#endif

/*MQTT主动上报寄存器地址：INV*/
static const uint16_t Mqtt_period_inv_addr[] = {
    MOD_REG_START_ADDR_00000,
    MOD_REG_START_ADDR_00100,
    MOD_REG_START_ADDR_01100,
    MOD_REG_START_ADDR_01200,
    MOD_REG_START_ADDR_01300,
    MOD_REG_START_ADDR_01400,
    MOD_REG_START_ADDR_01500,
    MOD_REG_START_ADDR_02000,
    MOD_REG_START_ADDR_02200,
#ifdef CONFIG_MODBUS_REG_ADDR_3700_ENABLE    
    MOD_REG_START_ADDR_03700,
#endif    
    MOD_REG_START_ADDR_06000,
    REP_REG_START_ADDR_11000,
    MOD_REG_START_ADDR_12000,
    MOD_REG_START_ADDR_13600,
#ifdef CONFIG_FUNC_PAYGO_ENABLE
    MOD_REG_START_ADDR_30000,
#endif    
#ifdef CONFIG_MQTT1_WLCC_REG_40000_IOT_DEBUG_ENABLE
    MOD_REG_START_ADDR_40000
#endif
};
static const size_t Mqtt_period_inv_total = sizeof(Mqtt_period_inv_addr) / sizeof(Mqtt_period_inv_addr[0]);

/*MQTT主动上报寄存器地址：PACK*/
static const uint16_t Mqtt_period_pack_addr[] = {
    MOD_REG_START_ADDR_06100,
    MOD_REG_START_ADDR_06300
};
static const size_t Mqtt_period_pack_total = sizeof(Mqtt_period_pack_addr) / sizeof(Mqtt_period_pack_addr[0]);

/*MQTT主动上报寄存器地址：DCDC*/
static const uint16_t Mqtt_period_dcdc_addr[] = {
    MOD_REG_START_ADDR_15500
};
static const size_t Mqtt_period_dcdc_total = sizeof(Mqtt_period_dcdc_addr) / sizeof(Mqtt_period_dcdc_addr[0]);

/*MQTT主动上报寄存器地址：DCHUB*/
static const uint16_t Mqtt_period_dchub_addr[] = {
    MOD_REG_START_ADDR_15700
};
static const size_t Mqtt_period_dchub_total = sizeof(Mqtt_period_dchub_addr) / sizeof(Mqtt_period_dchub_addr[0]);

/*上报记录变量*/
USE_EXT_RAM_BSS static periodic_pub_ctx_t inv_period_ctx = {0};
USE_EXT_RAM_BSS static periodic_pub_ctx_t pack_period_ctx = {0};
USE_EXT_RAM_BSS static periodic_pub_ctx_t dcdc_period_ctx = {0};
USE_EXT_RAM_BSS static periodic_pub_ctx_t dchub_period_ctx = {0};

// 整体上报信息
USE_EXT_RAM_BSS static mqtt_report_ctx_t mqtt_report_ctx = {
    .sreport_step    = 0,
    .send_ready_cnt  = 0ULL,
    .cycle           = 1,
    .sRunflag        = 0,
    .len             = 0,
    .index           = 0
};

/* 通用周期上报函数（复用 inv/pack 逻辑） */
static uint16_t iot_mqtt_publish_periodic_info(uint16_t cycle, uint8_t slaveaddr,
                                               const uint16_t *addr_list, size_t addr_total,
                                               periodic_pub_ctx_t *ctx)
{
    uint16_t start_addr = 0;
    uint16_t reg_num = 0;
    uint8_t *tx_buff = mqtt_tx_buf;

    if (ctx == NULL || addr_list == NULL || addr_total == 0) return 0;

    /* 检查上报配置，首次进入或 index==0 时更新协议版本与总段数 */
    if (ctx->index == 0) {
        uint16_t cur_ver = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver;
        if (ctx->md_protocol_ver != cur_ver) {
            ctx->md_protocol_ver = cur_ver;
            uint16_t count = 0;
            for (size_t i = 0; i < addr_total; i++) {
                uint16_t rn = Get_Regnum_By_Protocol_Ver(addr_list[i], ctx->md_protocol_ver, slaveaddr);
                if (rn > 0) {
                    count += (rn + 127 - 1) / 127;
                }
            }
            ctx->total_period_count = count;
            ESP_LOGW(TAG, "Mqtt Announce: Type(%d), protocol_ver(%d), total_period_count(%d)", ctx->type, ctx->md_protocol_ver, ctx->total_period_count);
        }
    }

    /* 查找上报寄存器段地址及长度（延续未完成段或取下一个段） */
    if (ctx->remain_regnum > 0) {
        start_addr = ctx->continue_addr;
        reg_num = (ctx->remain_regnum > 127) ? 127 : ctx->remain_regnum;
        ctx->continue_addr += reg_num;
        ctx->remain_regnum -= reg_num;
    } else if (ctx->index < addr_total) {
        start_addr = addr_list[ctx->index];
        uint16_t real_reg_num = Get_Regnum_By_Protocol_Ver(addr_list[ctx->index], ctx->md_protocol_ver, slaveaddr);
        reg_num = (real_reg_num > 127) ? 127 : real_reg_num;
        ctx->continue_addr = start_addr + reg_num;
        ctx->remain_regnum = (real_reg_num > reg_num) ? (real_reg_num - reg_num) : 0;
        ctx->index++;
    } else {
        /* 本次大循环完成，重置状态 */
        ctx->index = 0;
        ctx->send_seq = 0;
        return 0;
    }

    /* 组帧并发送 */
    if (reg_num > 0) {
        pack_data_head(0x01, tx_buff, PUB_PERIOD, cycle, ctx->total_period_count, ++ctx->send_seq);
        uint16_t len = Modbus_MasterWriteCmd_06H_10H(start_addr, reg_num, false, tx_buff + 10, slaveaddr, MD_CHL_SELF);
        if (len > 0) {
            iot_mqtt_send(tx_buff, (len + 10));
        }
        return len;
    }

    return 0;
}

#ifdef CONFIG_MQTT_4G_ANNOUNCE_MODBUS_TLV_ENABLE   

// 4G状态下TLV上报名单
static const modbus_tlv_announce_t System_TLV_Data_ctx[] = {
    {0,     15,     2   }, // 协议版本
    {0,     100,    6   }, // 系统PACK基本信息
    {0,     126,    14  }, // 系统故障告警信息
    {0,     140,    20  }, // 系统能量功率信息
    {0,     1112,   19  }, // 逆变器版本区域A
    {0,     6019,   4   }, // PACK能量信息
    {0,     6114,   10  }, // PACK关键信息
    {0,     11014,  2   }, // IOT软件版本
    {0,     11045,  37  }, // 4G关键信息(含GPS)
#ifdef CONFIG_FUNC_PAYGO_ENABLE    
    {0,     30004,  4   }, // Paygo关键信息1
    {0,     30012,  6   }, // Paygo关键信息2
#endif    
    {0,     40014,  2   }, // IOT调试信息1
    {0,     40018,  8   }, // IOT调试信息2
    {0,     40039,  6   }, // IOT调试信息3
    {0,     40057,  2   }, // IOT调试信息4
};

static const size_t System_TLV_Data_ctx_total = sizeof(System_TLV_Data_ctx) / sizeof(System_TLV_Data_ctx[0]);

#endif

/**
 * @brief MQTT周期性上报设备信息（主机和电池包）
 *
 * 功能说明：
 * 1. 判断MQTT连接和上电时间，未满足条件不上报。
 * 2. 500ms为周期线程，控制上报频率。
 * 3. 根据网络类型限制上报周期，防止流量过大。
 * 4. 分阶段上报主机和所有在线电池包数据。
 * 5. 每次上报完成后进入下一个阶段或下一个电池包。
 *
 * @return uint16_t 是否正在上报（1正在上报，0未上报）
 */
uint16_t iot_mqtt_announce(void)
{
	/*云端未连接或升级状态下暂停上报*/
    if ((login_info.mqtt_is_login == 0) 
        || (sys_is_updating())
        || (reals.MCUPoweronCnt < 30)) return 0;

    // 500ms周期线程控制
    static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if ((now_time - pre_time) >= 500) {
        pre_time = now_time;
    } else {
        return 0;
    }

    // 限制上报流量，根据网络类型设置周期
    uint64_t ReportCycle = 0;
#ifdef MQTT_ANNOUNCE_DEBUG
    ReportCycle = 20;
#else
    if (NETIF_TYPE_PPP == login_info.current_netif_id) {
        ReportCycle = IotSetData.dev_info_t.data_report_cycle * 2;
    } else {
        ReportCycle = 60; // 默认30s
    }
#endif

    // 周期未到则不上报
    if (!mqtt_report_ctx.sRunflag) {
        if (++mqtt_report_ctx.send_ready_cnt < ReportCycle) return 0;
        mqtt_report_ctx.send_ready_cnt = 0;
        mqtt_report_ctx.sRunflag = 1;

        bool debug_flag = false;
#ifdef MQTT_ANNOUNCE_DEBUG
        debug_flag = true;
#endif
        if ( debug_flag ) {
            mqtt_report_ctx.sreport_step = 0xFE;
        } else if (NETIF_TYPE_PPP == login_info.current_netif_id) {
#ifdef CONFIG_MQTT_4G_ANNOUNCE_MODBUS_TLV_ENABLE            
            mqtt_report_ctx.sreport_step = 0xF0;
#endif
        } else {
#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
            // 上报前检查系统绑定信息
            iot_bind_check();                                       
            if ( bind_dev_list.need_bind ) mqtt_bind_dev(); 
#endif        
        }
    }

    switch ( mqtt_report_ctx.sreport_step )
    {
        /*主机上报*/
        case 0 :
            
            /*发送INV数据信息*/
            inv_period_ctx.type = mqtt_report_ctx.sreport_step;
            mqtt_report_ctx.len = iot_mqtt_publish_periodic_info(mqtt_report_ctx.cycle, MD_SUMMARY_ADDR, Mqtt_period_inv_addr, Mqtt_period_inv_total, &inv_period_ctx);
            break;
        
        /*电池上报*/
        case 1 :
            
            /*发送PACK数据信息*/
            if( mqtt_report_ctx.index >= reals.online_Pack_num ) {
                mqtt_report_ctx.len = 0; // 所有电池包上报完成，进入下一个阶段
                break; 
            } else {
                pack_period_ctx.type = mqtt_report_ctx.sreport_step;
                mqtt_report_ctx.len = iot_mqtt_publish_periodic_info(mqtt_report_ctx.cycle, (mqtt_report_ctx.index + MD_PACK_ADDR_START), Mqtt_period_pack_addr, Mqtt_period_pack_total, &pack_period_ctx);
                if ( 0 == mqtt_report_ctx.len ) {
                    mqtt_report_ctx.index++; // 当前电池包上报完成，进入下一个电池包
                    return 1;
                }
            }
            
            break;    
            
        /*DCDC上报*/
        case 2 :
            
            /*发送DCDC数据信息*/
            if( mqtt_report_ctx.index >= reals.online_Dcdc_num ) {
                mqtt_report_ctx.len = 0; // 所有设备上报完成，进入下一个阶段
                break; 
            } else {
                dcdc_period_ctx.type = mqtt_report_ctx.sreport_step;
                mqtt_report_ctx.len = iot_mqtt_publish_periodic_info(mqtt_report_ctx.cycle, (mqtt_report_ctx.index + MD_DCDC_ADDR_START), Mqtt_period_dcdc_addr, Mqtt_period_dcdc_total, &dcdc_period_ctx);
                if ( 0 == mqtt_report_ctx.len ) {
                    mqtt_report_ctx.index++; // 当前设备上报完成，进入下一个设备
                    return 1;
                }
            }
            
            break;   
            
        /*DCHUB上报*/
        case 3 :
            
            /*发送DCHUB数据信息*/
            if( mqtt_report_ctx.index >= reals.online_DCHUB_num ) {
                mqtt_report_ctx.len = 0; // 所有设备上报完成，进入下一个阶段
                break; 
            } else {
                dchub_period_ctx.type = mqtt_report_ctx.sreport_step;
                mqtt_report_ctx.len = iot_mqtt_publish_periodic_info(mqtt_report_ctx.cycle, (mqtt_report_ctx.index + MD_DCHUB_ADDR_START), Mqtt_period_dchub_addr, Mqtt_period_dchub_total, &dchub_period_ctx);
                if ( 0 == mqtt_report_ctx.len ) {
                    mqtt_report_ctx.index++; // 当前设备上报完成，进入下一个设备
                    return 1;
                }
            }
            
            break;

#ifdef CONFIG_MQTT_4G_ANNOUNCE_MODBUS_TLV_ENABLE    
        /*4G模式下数据上报*/    
        case 0xF0:
            /*发送关键数据信息*/
            modbus_tlv_mqtt_announce(System_TLV_Data_ctx, System_TLV_Data_ctx_total, 1024);
            mqtt_report_ctx.len = 0;
            break;
#endif

#ifdef MQTT_ANNOUNCE_DEBUG
        /*调试数据上报*/    
        case 0xFE:
            iot_mqtt_debug_announce();
            mqtt_report_ctx.len = 0;
            break;
#endif

        default:
            
            // 所有阶段完成，重置状态
            mqtt_report_ctx.sreport_step = 0;
            mqtt_report_ctx.sRunflag = 0;
            return mqtt_report_ctx.sRunflag;
    }

    // 如果本阶段数据上报完成，进入下一个周期
	if(0 == mqtt_report_ctx.len) {
        if (mqtt_report_ctx.cycle == 65535) mqtt_report_ctx.cycle = 0;
        mqtt_report_ctx.cycle++;
        mqtt_report_ctx.sreport_step++;
        mqtt_report_ctx.index = 0;
	}
    
    return mqtt_report_ctx.sRunflag;    
}


/*------------------------------------------------------------------------------
 Function: Mqtt_Announce_Info_Clean
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT周期上报记录清空
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Mqtt_Announce_Info_Clean(void)
{
    static uint16_t cycle = 0;

    // 记录上报序号
    if ( mqtt_report_ctx.sRunflag ) {
        cycle = mqtt_report_ctx.cycle + 1;
    } else {
        cycle = mqtt_report_ctx.cycle;
    }

    // 清空上报记录
    memset(&mqtt_report_ctx, 0, sizeof(mqtt_report_ctx));
    memset(&inv_period_ctx, 0, sizeof(inv_period_ctx));
    memset(&pack_period_ctx, 0, sizeof(pack_period_ctx));
    memset(&dcdc_period_ctx, 0, sizeof(dcdc_period_ctx));
    memset(&dchub_period_ctx, 0, sizeof(dchub_period_ctx));

    mqtt_report_ctx.cycle = cycle;
}

/*------------------------------------------------------------------------------
 Function: iot_mqtt_send
 -----------------------------------------------------------------------------*/
/**
  * @brief      mqtt发送函数（信号量互斥锁）
  * @param[in]  const uint8_t *data  
                int len              
  * @param[out] None
  * @return     
  -1:fail
  0:ok
  */
int iot_mqtt_send(const uint8_t *data, int len)
{
	if ((!iot_net_status_check()) || (login_info.mqtt_is_login == 0)) 
	{
	    ESP_LOGE(TAG,"net error or mqtt error");
		return -1;	// wifi未连接或mqtt未登录
	}
    
    if (!mqtt_semaphore || xSemaphoreTake(mqtt_semaphore, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG,"mqtt_semaphore error");
        return -1; 
    }

#ifdef CONFIG_MQTT_CLIENT_SEND_BY_ENQUEUE
    //  ret > 0 成功、-2 outbox 满、-1 失败
    int ret = esp_mqtt_client_enqueue(login_info.mqtt_client, login_info.pub_topic, (const char *)data, len, 0, 0, true);
    if (ret == -2) {
        ESP_LOGE(TAG, "mqtt outbox full, topic:%s", login_info.pub_topic);
    } else if (ret < 0) {
        ESP_LOGE(TAG, "topic:%s enqueue failed", login_info.pub_topic);
    } else {
        reals.net_frame_tx_counter++;
    }
#else
    int ret = esp_mqtt_client_publish(login_info.mqtt_client, login_info.pub_topic, (const char *)data, len, 0, 0);
    if ( ret == -1 ) {
        ESP_LOGE(TAG, "topic:%s publish failed", login_info.pub_topic);
    } else {
        reals.net_frame_tx_counter++;
    }
#endif

    xSemaphoreGive(mqtt_semaphore);  /*释放信号量*/
	return ret;
}

#ifdef CONFIG_UART_PORT_ENABLE

/*------------------------------------------------------------------------------
 Function: uart_to_mqtt_send
 -----------------------------------------------------------------------------*/
/**
  * @brief      串口报文二次组帧发送
  * @param[in]  uint8_t *data  
                int len        
  * @param[out] None
  * @return     void
  */
void uart_to_mqtt_send(uart_to_server_data_t *uart_data)
{
	if ((!iot_net_status_check()) 
        || (login_info.mqtt_is_login == 0)
        || (uart_data == NULL)) {
	    ESP_LOGE(TAG,"wifi error or mqtt error");
		return;	// wifi未连接或mqtt未登录
	}

	uint8_t *Tx_data = mqtt_tx_buf;
	int16_t rsp_len = uart_data->data_len + 10;
    uint8_t funcode = uart_data->data[1] & 0x7F;
    uint8_t cause = 0;
    uint16_t cycle = 0;
   
    #if 1
    /*modbus v1.2*/

    if (funcode == 0x03) {
        cause = 0x0F;   // 读取
        if(uart_data->uart_modbus_info.vaild == 1) cycle = uart_data->uart_modbus_info.index;
    } else if (funcode == 0x06) {
        cause = PUB_PHONE; // 写入
    } else if (funcode == 0x10) {
        if(uart_data->data_len == 8) {
            cause = PUB_PHONE; // 写入
        } else {
            cause = PUB_EVEVT; // 特殊主动上报
        }
    } else {
        goto end; // 违规modbus功能码
    }
    
	pack_data_head(0x01, Tx_data, cause, cycle, 0, 0);	// 封装mqtt数据1.2v协议头

    if ( cause == PUB_EVEVT )
    {
        uint8_t regaddress = 0;
        char ascii_tx[64] = {0};
        
        regaddress = ((uint16_t)uart_data->data[2]<<8) | uart_data->data[3];
        uint16_t tx_len = 10;
        Tx_data[tx_len++] = 0x01;
        Tx_data[tx_len++] = 0x49;
        if ( regaddress == 102 )
        {
            old_dev_soc = top_modbus_rd.mod_reg00100_AppPage1.soc;
            sprintf(ascii_tx, "\"soc=%d\"", top_modbus_rd.mod_reg00100_AppPage1.soc); 
            ESP_LOGW(TAG, "PUB_EVEVT(reg:%d) : %s", regaddress, ascii_tx);
        }
        else if (( regaddress == 123 ) || ( regaddress == 124 ))
        {
            old_dev_ctrl.all = top_modbus_rd.mod_reg00100_AppPage1.ctrl_status.all;
            if ( top_modbus_rd.mod_reg00100_AppPage1.ctrl_status.bit.system_off == 1)
            {
                /*远程关机，提前断开MQTT*/
                set_mqtt_stop();
                goto end;
            }
            sprintf(ascii_tx, "\"ac_sw=%01d,dc_sw=%01d,pv_sw=%01d,grid_sw=%01d\"", 
                top_modbus_rd.mod_reg00100_AppPage1.ctrl_status.bit.ac_on, 
                top_modbus_rd.mod_reg00100_AppPage1.ctrl_status.bit.dc_on,
                top_modbus_rd.mod_reg00100_AppPage1.ctrl_status.bit.pv_on,
                top_modbus_rd.mod_reg00100_AppPage1.ctrl_status.bit.grid_on); 

            ESP_LOGW(TAG, "PUB_EVEVT(reg:%d) : %s", regaddress, ascii_tx);
        }
        else
        {
            goto end;
        }
        
        memcpy(&Tx_data[tx_len], (uint8_t *)ascii_tx, strlen(ascii_tx));
        tx_len += strlen(ascii_tx);

//        Tx_data[tx_len++] = 0x00;//字符串结束符(旧版本使用，新版使用CRC时暂不使用字符串结束符)
        
        uint16_t crc = ModbusCrc16((uint8_t *)&Tx_data[10], (tx_len - 10));
        Tx_data[tx_len++] = (uint8_t) crc;
        Tx_data[tx_len++] = (uint8_t)(crc>>8);

        iot_mqtt_send(Tx_data, tx_len);
        goto end;
        
    }
    else
    {
        memcpy(&Tx_data[10], uart_data->data, uart_data->data_len);
        iot_mqtt_send(Tx_data, rsp_len);
    }

    #else
    
    /*modbus v1.0*/
   	Tx_data[0] = 0x01;

    memcpy(&Tx_data[1], data, len);

    iot_mqtt_send(Tx_data, (len + 1));
    ESP_LOGW(TAG, "uart_to_mqtt_send   cmdLen = %d", len + 1);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, Tx_data, len + 1, ESP_LOG_WARN);
    
    #endif

end:
    
    return;
}

#endif
  
  /*------------------------------------------------------------------------------
   Function: Mqtt_Active_Report_Queue_Push
   -----------------------------------------------------------------------------*/
  /**
    * @brief      紧急发送存储到队列缓存
    * @param[in]  void  
    * @param[out] None
    * @return     void
    */
void Mqtt_Active_Report_Queue_Push(uint8_t *pdata, uint16_t len, uint8_t scene, uint16_t cycle, uint8_t total, uint8_t seq)
{
    Mqtt_Active_Report_Queue_Struct queue_msg;

    if (xQueue_Mqtt_Active_Report == NULL) {
        ESP_LOGE(TAG, "xQueue_Mqtt_Active_Report is NULL!");
        return;
    }

    queue_msg.data = (uint8_t *)iot_calloc(len + 10);

    if (!queue_msg.data) 
    {
        ESP_LOGE(TAG, "Mqtt_Active_Report_Queue_Push calloc failed");
    } 
    else 
    {
        pack_data_head(0x01, queue_msg.data, scene, cycle, total, seq);   // 封装mqtt数据1.2v协议头
        memcpy(&queue_msg.data[10], pdata, len);
        queue_msg.data_len = len + 10;

        /*消息保存到队列*/
        if (!xQueue_Mqtt_Active_Report || xQueueSendToBack(xQueue_Mqtt_Active_Report, &queue_msg, 0) != pdPASS) 
        {
            ESP_LOGE(TAG, "Mqtt_Active_Report_Queue_Push failed");
            free(queue_msg.data);
            queue_msg.data = NULL;
        }
    }
}
 
 /*------------------------------------------------------------------------------
  Function: Mqtt_Active_Report_Queue_Report
  -----------------------------------------------------------------------------*/
 /**
   * @brief      主动上报任务
   * @param[in]  void  
   * @param[out] None
   * @return     void b
   */
  static int Mqtt_Active_Report_Queue_Report(void)
  {
      int ret = -1;
      Mqtt_Active_Report_Queue_Struct queue_msg;
 
      if(0 != login_info.mqtt_is_login)
      {
          if (xQueue_Mqtt_Active_Report && xQueueReceive(xQueue_Mqtt_Active_Report, &queue_msg,  0) == pdTRUE)//非阻塞
          {
              if (queue_msg.data) 
              {
                  ret = iot_mqtt_send(queue_msg.data, queue_msg.data_len);  
                  free(queue_msg.data);
              }
          }
      }
 
      return ret;
  }

 /*------------------------------------------------------------------------
 *@Function :Modbus_Self_21000_Report_MQTT 
 主动上报绑定帧
主动上报和基于RX查询的TX的 mqtt通道，二者不同
 */
void Modbus_Self_21000_Report_MQTT(void) 
{
	uint16_t rsp_len = 0;
	uint16_t pointcnt = 0;	
	uint16_t point_tx_nums = 0;	
	uint16_t ver = 0;
	uint16_t point_bias = 0;	
    uint8_t *tx_buff = NULL;
    uint16_t buffer_len = 0;
    
    if(iot_mqtt_login_state() == 0) return;	//mqtt未登录

	if(((0 == reals.modbus_self_report_ble)||(0 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.ble_connect)) //蓝牙未连接或未在发送绑定帧
        &&(0 == reals.bind_state_ask)) ver = reals.modbus_self_report_mqtt;
    
    if ( ver == 0 ) return;
    if(!Modbus_21000_semaphore_Take()) return;
    
    switch ( ver )
    {    
        case 1 :

            /*已组网设备信息帧响应*/
            pointcnt = Modbus_21000_1_Net_Point_Frame();
            break;
            
        case 3 :

            /*升级状态查询响应*/
            pointcnt = Modbus_21000_3_Net_Point_Frame();
            break;
            
        case 5 :
            
            /*上报所有设备，包括已绑定和未绑定的*/
            pointcnt = Modbus_21000_5_Net_Point_Frame();
            break;
        
        default:
            ESP_LOGE(TAG, "Modbus_Self_21000_Report_MQTT : Unknown Ver(%d)", ver);
            goto end;
    }
    
    ESP_LOGW(TAG, "Modbus_Self_21000_Report_MQTT(%d)  pointcnt:%d", ver, pointcnt);
    
    if(pointcnt >= BIND_READ_MAX_DEV_NUM) {
        buffer_len = 50 + (BIND_READ_MAX_DEV_NUM * sizeof(POINT_BIND_INFO));
    } else if(pointcnt > 0) {
        buffer_len = 50 + (pointcnt * sizeof(POINT_BIND_INFO));
    } 
    
	do {
	    if(pointcnt >= BIND_READ_MAX_DEV_NUM) {
		    pointcnt -= BIND_READ_MAX_DEV_NUM;
		    point_tx_nums = BIND_READ_MAX_DEV_NUM;
	    } else if(pointcnt > 0) {
		    point_tx_nums = pointcnt;	
		    pointcnt = 0;					   
	    } else {
            break;
        }
        
        if(tx_buff == NULL) {
            tx_buff = iot_calloc(buffer_len * sizeof(char));
            if (tx_buff == NULL) {
                ESP_LOGE(TAG, "malloc fail");
                goto end;
            }
        } else {
            memset(tx_buff, 0, buffer_len);
        }

		top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data.bias = point_bias;
		point_bias += point_tx_nums;
		rsp_len = Modbus_WriteMultiRegs_Report_Frame(tx_buff, point_tx_nums * (sizeof(POINT_BIND_INFO) / 2), ver);

        //打印
        ESP_LOGD(TAG, "Modbus_Self_21000_Report_MQTT, reg_addr:%d, data_len:%d, data:", 21000, rsp_len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, tx_buff, rsp_len, ESP_LOG_DEBUG);
        Mqtt_Active_Report_Queue_Push(tx_buff, rsp_len, PUB_Multi_DEV, 0, 0, 0);
	}
	while (pointcnt);
    
    if (tx_buff != NULL) free(tx_buff);

end:

    if(ver == reals.modbus_self_report_mqtt) reals.modbus_self_report_mqtt = 0;	
    Modbus_21000_semaphore_Give();
}

/*------------------------------------------------------------------------------
 Function: iot_mqtt_actively_report_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT上报任务(多功能上报，控制优先级及间隔)
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_mqtt_actively_report_task(void)
{
    static bool idle_flag = true;
	static uint32_t pre_time = 0;

    /*wifi未连接或mqtt未登录*/
	if ((!iot_net_status_check()) || (login_info.mqtt_is_login == 0)) {
		return;
	}

    /*非空闲时计算延时*/
    if ( idle_flag == false )
    {
        uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if((now_time - pre_time) >= 20) {
            idle_flag = true;
        } else {
            return;
        }
    }

    do {
        
#ifdef CONFIG_UART_PORT_ENABLE       
        /*透传发送队列*/
        if (uart_to_mqtt_queue_task()) {
            idle_flag = false;
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }
#endif 

        /*关键上报*/
        if (iot_mqtt_dev_event_ann()) {
            idle_flag = false;
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }

        /*设备特殊主动上报*/
        if (Mqtt_Active_Report_Queue_Report() > 0) {
            idle_flag = false;
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }

#ifdef CONFIG_MQTT_PAYLOAD_TLV_ENABLE        
        /*MQTT TLV响应/上报*/
        if (mqtt_tlv_protocol_report_to_cloud()) {
            idle_flag = false;
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }
#endif

#ifdef CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION        
        /*数据通道响应*/
        if (udt_mqtt_Report_to_Cloud()) {
            idle_flag = false;
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }
#endif

        /*空闲周期上报*/
        if (iot_mqtt_announce()) {
            idle_flag = false;
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }

#ifdef CONFIG_OPEN_MQTT_ENABLE
        /*调试服务器空闲周期上报*/
        if (Open_Mqtt_Module_Announce()) {
            idle_flag = false;
            pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
        }
#endif    
    } while (0);
}

/*------------------------------------------------------------------------------
 Function: iot_mqtt_period_check_and_report
 -----------------------------------------------------------------------------*/
/**
  * @brief      MQTT周期检查上报任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_mqtt_period_check_and_report(void)
{
    static uint32_t pre_dst_update_time = 0;
    static uint32_t pre_fix_update_time = 0;
    static uint32_t pre_cloud_sync_time = 0;

	/*云端未连接或升级状态下暂停上报*/
    if ((login_info.mqtt_is_login == 0) 
        || (sys_is_updating())) return;

    // 1s周期线程控制
    static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if ((now_time - pre_time) >= 1000) {
        pre_time = now_time;
    } else {
        return;
    }

    if(((now_time - pre_dst_update_time) >= H24_HOUR_INTERVAL) 
        || ((0 == pre_dst_update_time) && (0 == reals.rtc_flag.sBit.RTC_valid_from_SERVER)))
    {
#if TIME_GET_FROM_SERVER_PERIOD_CHL == 2    
        /*云端校时*/
        if ( 2 != IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update ) {
            if ( 1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.gps ) {
                mqtt_tlv_request_server_and_dst(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.GPS_Y, 
                    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.GPS_X);
            } else {
                mqtt_tlv_request_server_and_dst(0, 0);
            }
        }
#endif            
        pre_dst_update_time = now_time;
    }
    else if ((((now_time - pre_fix_update_time) >= ONE_HOUR_INTERVAL) || (0 == pre_fix_update_time)) 
        && (1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.gps))
    {
#ifdef CONFIG_MQTT_PAYLOAD_TLV_ENABLE
        // 周期上报定位数据
        mqtt_tlv_report_location_info(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.GPS_Y, 
                                top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.GPS_X, 
                                top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.signal_4G,
                                top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit._4g);
#endif
        pre_fix_update_time = now_time;
    }
    else if ((((now_time - pre_cloud_sync_time) >= ONE_MINUTE_INTERVAL) 
            || (0 == pre_cloud_sync_time)
            || (1 == reals.ModbusCmdFlag.sBit.mesh_id))
        && (NETIF_TYPE_PPP != login_info.current_netif_id))
    {
#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
        if (( 0 == pre_cloud_sync_time ) 
            || (1 == reals.ModbusCmdFlag.sBit.mesh_id)){
            // 首次连接或MeshID变化，请求云端最新参数更新推送
            cloud_sync_revd_param_cmd(true);

            // 更新后立即检查本地参数，并更新至云端
            cloud_sync_push_param_cmd(true);
        } else {
            // 检查本地参数，并更新至云端
            cloud_sync_push_param_cmd(true);
        }
#endif
        reals.ModbusCmdFlag.sBit.mesh_id = 0;
        pre_cloud_sync_time = now_time;
    }
}

static void HexToAscii( uint8_t *str, const uint8_t *hex , uint16_t hex_len)
{
    uint16_t i=0;
	uint16_t j=0;
    uint8_t  high = 0;
    uint8_t  low = 0;

    while( j < hex_len ) // to ascii
    {
        high = hex[ j ]/0x10;
        low  = hex[ j ]%0x10;
        str[i] = ( high < 10 ) ? (high + '0')  : (high-0x0A + 'A');
        str[i+1] = ( low < 10 ) ?  (low + '0') : (low-0x0A + 'A');

        i += 2;
        j++;
    }
}

#define START_TIME          1371517200  // 2013-06-18 09:00:00
#define TIME_STEP           30          // 30s --> 30 *1000 ms
#define SHARED_PRIVATE_KEY  "ga3sa4hj6kfl"
USE_EXT_RAM_BSS static uint8_t gBytesSecret[48] = {0};

static void CreatePasswordText(uint8_t *text, uint8_t *src, uint8_t length)
{
	MD5_CTX   md5;
	uint8_t buf[16] = {0};

    MD5Init(&md5);
    MD5Update( &md5, src, length ); /* md5 calac */
    MD5Final( buf, &md5 );  //windy issue
    HexToAscii(text, (const uint8_t *)buf, 16); // shift capital letter ASCII
}

static uint8_t CreateKarray(char *name, uint64_t code)
{
    uint8_t i = 0, j = 0;
    char codeStr[100] = {0};
    const char *ptr = SHARED_PRIVATE_KEY;
    uint8_t len ;
    
    snprintf(codeStr, sizeof(codeStr), "%llu", code);
    
    gBytesSecret[i++] = name[0]; // sn byte0
    gBytesSecret[i++] = name[1]; // sn byte1
    
    len = strlen(SHARED_PRIVATE_KEY)/2;
    for(j = 0; j < len; j++)
    {
        gBytesSecret[i++] = ptr[j];
    }
    
    len = strlen(codeStr);
    for(j = 0; j < len; j++)
    {
        gBytesSecret[i++] = codeStr[j];
    } 
    
    len = strlen(SHARED_PRIVATE_KEY)/2;
    for(j = 0; j < len; j++)
    {
        gBytesSecret[i++] = ptr[j + len];
    }
    
    len = strlen((const char *)name);
    gBytesSecret[i++] = name[len-2]; // sn byte n-1
    gBytesSecret[i++] = name[len-1]; // sn byte n

    return i;
}

uint32_t CreateEncryptPassword(char *name, uint64_t code, uint32_t nowTime)
{
    uint8_t  i, j;
    uint8_t  kText[32];
    uint8_t  cText[32];
    uint8_t  rawPw[64];
    uint8_t  bytesTime[8];
    uint32_t password = 0;
    uint64_t time=0;

    time = (nowTime - START_TIME) / TIME_STEP;

    for(i = 8; i > 0; i--)
    {
        bytesTime[i - 1] = (time & 0xFF);
        time >>= 8;
    }

    i = CreateKarray(name, code);

    CreatePasswordText(kText, gBytesSecret, i);
    CreatePasswordText(cText, bytesTime,    8);

    for (i = 0, j = 0; i < 32; i++)
    {
        rawPw[j]     = kText[i];
        rawPw[j + 1] = cText[i];
        j += 2;
    }

    i = rawPw[63] & 0x0F;

    password |= (uint32_t)(rawPw[i+0] & 0x7F) << 24;
    password |= (uint32_t)(rawPw[i+1] & 0xFF) << 16;
    password |= (uint32_t)(rawPw[i+2] & 0xFF) << 8;
    password |= (uint32_t)(rawPw[i+3] & 0xFF) ;

    password %= 100000000;
    return password;
}
