#include "wlcc_process.h"
#include "wlcc_interface.h"
#include "wlcc_protocol.h"
#include "wlcc_crypt.h"
#include "wlcc_common.h"
#include "can_protocol.h"
#include "modbus_define.h"
#include "utils.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "modbus_slave.h"
#include "modbus_master.h"
#include "modbus_protocol.h"
#include "modbus_data.h"
#include "parameter.h"

#include "dev_modbus_manage.h"

#include "dev_modbus_s1_data_handler.h"
#include "dev_modbus_meter_data_handler.h"
#include "dev_modbus_other_inv_data_handler.h"

#define TAG "[wlcc_process]"

static uint8_t *plaintext = NULL;
static uint8_t *ciphertext = NULL;
static QueueHandle_t send_queue = NULL;
//static QueueHandle_t wlcc_recv_queue = NULL;

USE_EXT_RAM_BSS static wlcc_msg_pending_t gMsgPendingList[WLCC_MSG_PENDING_MAX];
USE_EXT_RAM_BSS wlcc_dev_t gWlccDevList[NET_WIFI_MAX_POINT];
USE_EXT_RAM_BSS wlcc_master_t gWlccMasterInfo;
static uint8_t gWlccDevNum = 0;
USE_EXT_RAM_BSS static uint8_t wlcc_report_data_buf[300];

USE_EXT_RAM_BSS static wlcc_tlv_block_t test_tlv_blocks[TLV_MAX_BLOCK_COUNT];
USE_EXT_RAM_BSS static uint8_t test_value_buf[512]; // 用于存储输入模拟数据的值
static uint16_t test_tlv_block_count = 0;

static uint8_t wlcc_modbus_poll_step = 0;

uint8_t wlcc_msg_pending_add(wlcc_msg_pending_t msg)
{
	uint8_t i;

	for(i=0; i<WLCC_MSG_PENDING_MAX; i++)
	{
		if((0 == gMsgPendingList[i].msg_id) && (0 == gMsgPendingList[i].timeout))
		{
			gMsgPendingList[i] = msg;
			return 0;
		}
	}

	return -1;
}

uint8_t wlcc_remove_pending_msg(wlcc_msg_pending_t msg_pending)
{
	uint8_t i;

	for(i=0; i<WLCC_MSG_PENDING_MAX; i++)
	{
		if((msg_pending.msg_id == gMsgPendingList[i].msg_id) 
			&& (msg_pending.devTpye == gMsgPendingList[i].devTpye)
			&& (msg_pending.devSn == gMsgPendingList[i].devSn))
		{
			memset(&gMsgPendingList[i], 0, sizeof(wlcc_msg_pending_t));
			ESP_LOGE(TAG, "remove_pending_msg:type[%d], Sn[%llu], msgId[%d] OK!", msg_pending.devTpye, msg_pending.devSn, msg_pending.msg_id);
			break;
		}
	}

	return 0;
}


wlcc_msg_pending_t wlcc_check_message_response(wlcc_dev_info_t rsp_addr)
{
	uint8_t i;
	wlcc_msg_pending_t msg_pending;

	memset(&msg_pending, 0, sizeof(wlcc_msg_pending_t));

	for(i=0; i<WLCC_MSG_PENDING_MAX; i++)
	{
		if((rsp_addr.msg_id == gMsgPendingList[i].msg_id) 
			&& ((rsp_addr.dev_type == gMsgPendingList[i].devTpye) || (0 == gMsgPendingList[i].devTpye))
			&& ((rsp_addr.dev_sn == gMsgPendingList[i].devSn) || (0 == gMsgPendingList[i].devSn)))
		{
			memcpy(&msg_pending, &gMsgPendingList[i], sizeof(wlcc_msg_pending_t));
			break;
		}
	}

	return msg_pending;
}


int wlcc_msg_send_to_queue(const uint8_t *data_buf, uint16_t len, const char *dst_ip_str, uint16_t dst_port)
{
	wlcc_msg_t msg_send;

	msg_send.data = iot_calloc(len);
	if(NULL == msg_send.data){
		ESP_LOGE (TAG, " wlcc send to queue malloc failed");
		return -1;
	}

	memcpy(msg_send.data, data_buf, len);
	msg_send.len = len;
	msg_send.dst_port = dst_port;
	
	// 设置目标IP地址
	if (dst_ip_str != NULL)
	{
		strncpy((char *)msg_send.dst_ip, dst_ip_str, sizeof(msg_send.dst_ip) - 1);
		msg_send.dst_ip[sizeof(msg_send.dst_ip) - 1] = '\0';
	}
	else
	{
		msg_send.dst_ip[0] = '\0';  // 空字符串表示组播
	}

	if (xQueueSendToBack(send_queue, &msg_send, pdMS_TO_TICKS(0)) != pdTRUE)
	{
		ESP_LOGE (TAG, " wlcc send to queue failed");
		free(msg_send.data);
		return -1;
	}

    return 0;
}

uint8_t wlcc_modbus_msg_send(uint8_t *income, uint16_t cmd_len, uint16_t dev_type, uint64_t dev_sn, modbus_addr_info_t src_addr)
{
	uint8_t tx_len = 0;
	uint8_t funcode = income[1];
	uint8_t frametype = 0;
	uint32_t dst_ip;
	uint16_t dst_port;
	wlcc_msg_pending_t msg_pending;

	if((funcode == 0x03) || (funcode == 0x06) || (funcode == 0x10))
	{
		if(funcode == 0x03){
			frametype = WLCC_FRAME_TYPE_READ;
		}else{
			frametype = WLCC_FRAME_TYPE_WRITE;
		}
		tx_len = wlcc_build_data_send_frame(frametype, dev_type, dev_sn, income, cmd_len, plaintext);
		if(tx_len > 0)
		{
			for (int i = 0; i < NET_WIFI_MAX_POINT; i++)
			{
				if((dev_type == gWlccDevList[i].dev_type) 
					&& (dev_sn == gWlccDevList[i].dev_sn))
				{
					if(wlcc_msg_send_to_queue(plaintext, tx_len, (char *)inet_ntoa(gWlccDevList[i].ip), gWlccDevList[i].port) == 0)
					{
						msg_pending.msg_id = wlcc_get_msg_id();
						msg_pending.devTpye = dev_type;
						msg_pending.devSn = dev_sn;
						msg_pending.timeout = WLCC_PENDING_TIMEOUT_MAX;
						msg_pending.src_addr = src_addr;
						wlcc_msg_pending_add(msg_pending);
					}
					break;
				}
			}
		}
		else{
			ESP_LOGE(TAG, "Failed to build wlcc modbus data!");
		}
	}
	else{
		ESP_LOGE(TAG, "funcode is err!");
	}

	return 0;
}

/**
 * @brief 发送21000帧（周期上报，不需要响应）
 * @param slave_address 从机地址
 * @param pIn 21000段内容
 * @param len 21000段内容长度
 * @param dev_type
 * @param dev_sn
 * @return 0成功，失败返回负数
 * @note 周期上报类型（WLCC_FRAME_TYPE_PERIOD），不需要等待响应，不添加到pending列表
 */
int wlcc_modbus_msg_send_21000(uint8_t slave_address, const uint8_t *pIn, uint16_t len, uint16_t dst_type, uint64_t dst_sn)
{
    uint16_t modbus_len = 0;
    uint16_t tx_len = 0;

	uint8_t *pModbusData = iot_calloc(len + 10);
	if(NULL == pModbusData){
		return -1;
	}

	uint8_t *pWlccData = iot_calloc(len + 60);
	if(NULL == pWlccData)
	{
		free(pModbusData);
		return -1;
	}

	modbus_len = Modbus_WriteCmd_06H_10H_Build(slave_address, 21000, (len>>1), pIn, pModbusData);
	tx_len = wlcc_build_data_send_frame(WLCC_FRAME_TYPE_PERIOD, dst_type, dst_sn, pModbusData, modbus_len, pWlccData);

    // 使用队列发送
    // 加密和实际发送在wlcc_send_task()中完成
    int ret = wlcc_msg_send_to_queue(pWlccData, tx_len, NULL, 0);
    if (ret == 0){
        ESP_LOGI(TAG, "TLV frame 21000 queued: %d bytes", tx_len);
    }
    else{
        ESP_LOGE(TAG, "Failed to queue TLV frame 21000: %d", ret);
    }

	free(pModbusData);
	free(pWlccData);

    // 周期上报不需要添加到pending列表，因为不需要等待响应
    return ret;
}

/**
 * @brief 发送TLV格式的21000帧（周期上报，不需要响应）
 * @param slave_address 从机地址
//  * @param dst_ip 目的IP地址，如果为NULL则使用组播
//  * @param dst_port 目的端口，如果为0则使用组播端口
	dev_type：目标设备类型
	dev_sn：目标设备SN
 * @param tlv_blocks TLV块数组
 * @param tlv_block_count TLV块数量
 * @return 0成功，失败返回负数
 * @note 周期上报类型（WLCC_FRAME_TYPE_PERIOD），不需要等待响应，不添加到pending列表
 */
int wlcc_modbus_msg_send_21000_tlv(const wlcc_tlv_block_t *tlv_blocks,
                                   uint16_t tlv_block_count,
                                   uint16_t dev_type,
                                   uint64_t dev_sn)
{
    uint16_t tx_len = 0;

    tx_len = wlcc_build_data_send_frame_21000_tlv(WLCC_FRAME_TYPE_PERIOD,
                                                  dev_type,
                                                  dev_sn,
                                                  tlv_blocks,
                                                  tlv_block_count,
                                                  plaintext);
    if (tx_len == 0)
    {
        ESP_LOGE(TAG, "Failed to build TLV frame 21000");
        return IOT_FAIL;
    }

    // 使用队列发送
    // 加密和实际发送在wlcc_send_task()中完成
    int ret = wlcc_msg_send_to_queue(plaintext, tx_len, NULL, 0);
    if (ret == 0)
    {
        ESP_LOGI(TAG, "TLV frame 21000 period queued: %d bytes", tx_len);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to queue TLV frame 21000: %d", ret);
    }

    // 周期上报不需要添加到pending列表，因为不需要等待响应
    return ret;
}

int wlcc_modbus_msg_send_file_context(const uint8_t *data, uint16_t len, uint16_t dst_type, uint64_t dst_sn)
{
    uint16_t tx_len = 0;

    uint8_t *pWlccData = iot_calloc(len + 60);
    if (NULL == pWlccData)
    {
        return -1;
    }

    tx_len = wlcc_build_data_send_frame(WLCC_FRAME_TYPE_FILE_CONTEXT, dst_type, dst_sn, data, len, pWlccData);

    // 发送到队列
    int ret = wlcc_msg_send_to_queue(pWlccData, tx_len, NULL, 0);

    free(pWlccData);

    // 周期上报不需要添加到pending列表，因为不需要等待响应
    return ret;
}

void wlcc_msg_pending_timeout_check(void)
{
	uint8_t i;

	for(i=0; i<WLCC_MSG_PENDING_MAX; i++)
	{
		if(gMsgPendingList[i].timeout >= 100){
			gMsgPendingList[i].timeout -= 100;
		}
		else
		{
			if(gMsgPendingList[i].msg_id || gMsgPendingList[i].devTpye || gMsgPendingList[i].devSn)
			{
				ESP_LOGE(TAG, "pending_msg:type[%d], Sn[%llu], msgId[%d] timeout!", 
								gMsgPendingList[i].devTpye, gMsgPendingList[i].devSn, gMsgPendingList[i].msg_id);
				memset(&gMsgPendingList[i], 0, sizeof(wlcc_msg_pending_t));
			}
		}
	}
}

uint8_t wlcc_is_master(void)
{
	// ESP_LOGI(TAG,"dev_priority:%u ,dev_sn:%llu,dev_type:%u",gWlccMasterInfo.master_info.dev_priority,gWlccMasterInfo.master_info.dev_sn,gWlccMasterInfo.master_info.dev_type);
	return gWlccMasterInfo.is_master;
}

uint8_t wlcc_dev_master_type(uint16_t dev_type, uint64_t dev_sn)
{
	if((WLCC_EMS_MODE_INVALID != gWlccMasterInfo.is_master)
		&& (dev_type == gWlccMasterInfo.master_info.dev_type) 
		&& (dev_sn == gWlccMasterInfo.master_info.dev_sn)){
		return 1;
	}

	return 0;
}

void wlcc_master_triger(void)
{
	time_t now;
	time(&now);
	
	gWlccMasterInfo.is_new_master = WLCC_EMS_MODE_INVALID;
	gWlccMasterInfo.last_parallel_time = 0;	
	memset(&gWlccMasterInfo.master_info, 0, sizeof(wlcc_dev_t));
	gWlccMasterInfo.master_info.dev_priority = DEV_PRIORITY_SELF;
	gWlccMasterInfo.master_info.dev_type = SN_TYPE_SELF;
	gWlccMasterInfo.master_info.dev_sn = dev_factory.dev_sn;
	gWlccMasterInfo.master_info.last_online_time = (long long)now;

	ESP_LOGD(TAG, "master_triger. last_online_time:%lu",gWlccMasterInfo.master_info.last_online_time);
}

void wlcc_master_need_update(void)
{
	time_t now;
	time(&now);
	
	gWlccMasterInfo.is_new_master = WLCC_EMS_MODE_INVALID;
	//gWlccMasterInfo.last_parallel_time = 0;	
	memset(&gWlccMasterInfo.master_info, 0, sizeof(wlcc_dev_t));
	gWlccMasterInfo.master_info.dev_priority = DEV_PRIORITY_SELF;
	gWlccMasterInfo.master_info.dev_type = SN_TYPE_SELF;
	gWlccMasterInfo.master_info.dev_sn = dev_factory.dev_sn;
	gWlccMasterInfo.master_info.last_online_time = (long long)now;

	ESP_LOGD(TAG, "wlcc_master_need_update. last_online_time:%lu",gWlccMasterInfo.master_info.last_online_time);
}

void wlcc_master_online_time_update(wlcc_dev_info_t dev_info, uint32_t online_time)
{
	if((dev_info.dev_priority == gWlccMasterInfo.master_info.dev_priority)
		&& (dev_info.dev_type == gWlccMasterInfo.master_info.dev_type)
		&& (dev_info.dev_sn == gWlccMasterInfo.master_info.dev_sn))
	{
		gWlccMasterInfo.master_info.last_online_time = online_time;
		ESP_LOGD(TAG, "wlcc_master_online_time_update. last_online_time:%lu",gWlccMasterInfo.master_info.last_online_time);
	}
}

void wlcc_master_update(wlcc_dev_info_t dev_info, uint32_t online_time)
{
	gWlccMasterInfo.is_new_master = WLCC_EMS_MODE_SLAVER;

	gWlccMasterInfo.master_info.dev_priority = dev_info.dev_priority;
	gWlccMasterInfo.master_info.dev_type = dev_info.dev_type;
	gWlccMasterInfo.master_info.dev_sn = dev_info.dev_sn;
	gWlccMasterInfo.master_info.online_status = WLCC_STATUS_ONLINE;
	gWlccMasterInfo.master_info.last_online_time = online_time;
	gWlccMasterInfo.master_info.ip = dev_info.ip;
	gWlccMasterInfo.master_info.port = dev_info.port;
	
	ESP_LOGD(TAG, "master_update, master type[%d] sn[%llu], is_new_master:%d, is_master:%d,last_online_time:%lu", 
					gWlccMasterInfo.master_info.dev_type, gWlccMasterInfo.master_info.dev_sn, 
					gWlccMasterInfo.is_new_master, gWlccMasterInfo.is_master,gWlccMasterInfo.master_info.last_online_time);
}

// 竞争主机PK
void wlcc_master_pk(wlcc_dev_info_t dev_info, uint16_t dst_type, uint64_t dst_sn)
{
	time_t now;
	time(&now);

	ESP_LOGI(TAG, "dev_priority:%d  dev_type:%u  dev_sn:%llu", dev_info.dev_priority,dev_info.dev_type,dev_info.dev_sn);
	ESP_LOGI(TAG,"gWlccMasterInfo dev_priority:%u  dev_type:%u  dev_sn:%llu",gWlccMasterInfo.master_info.dev_priority,gWlccMasterInfo.master_info.dev_type,gWlccMasterInfo.master_info.dev_sn);
	if(dev_info.dev_priority && dev_info.dev_type && dev_info.dev_sn)
	{
		if((DEV_PRIORITY_SELF == gWlccMasterInfo.master_info.dev_priority)
			&& ((SN_TYPE_SELF == gWlccMasterInfo.master_info.dev_type) 
				|| (SN_TYPE_COMMON_INVERTER_WIRELESS == gWlccMasterInfo.master_info.dev_type))
			&& (dev_factory.dev_sn == gWlccMasterInfo.master_info.dev_sn))
		{
			ESP_LOGI(TAG, "MASTER:dev_priority:%d, dev_type:%d, dev_sn:%lld", gWlccMasterInfo.master_info.dev_priority,
																				gWlccMasterInfo.master_info.dev_type,
																				gWlccMasterInfo.master_info.dev_sn);
			gWlccMasterInfo.is_new_master = WLCC_EMS_MODE_MASTER;
		}
			
		if(dev_info.dev_priority > gWlccMasterInfo.master_info.dev_priority){
			ESP_LOGI(TAG, "dev_priority:%d, master_info.dev_priority:%d", dev_info.dev_priority, gWlccMasterInfo.master_info.dev_priority);
			wlcc_master_update(dev_info, (long long)now);
		}
		else if(dev_info.dev_priority == gWlccMasterInfo.master_info.dev_priority)
		{
			if(dev_info.dev_type < gWlccMasterInfo.master_info.dev_type){
				ESP_LOGI(TAG, "dev_type:%d, master_info.dev_type:%d", dev_info.dev_priority, gWlccMasterInfo.master_info.dev_priority);
				wlcc_master_update(dev_info, (long long)now);
			}
			else if((dev_info.dev_type == gWlccMasterInfo.master_info.dev_type)
				&& (dev_info.dev_sn < gWlccMasterInfo.master_info.dev_sn)){
				ESP_LOGI(TAG, "dev_sn:%lld, master_info.dev_sn:%lld", dev_info.dev_sn, gWlccMasterInfo.master_info.dev_sn);
				wlcc_master_update(dev_info, (long long)now);
			}
		}
	}
}

void wlcc_master_pk_timeout_check(void)
{
	ESP_LOGI(TAG,"gWlccMasterInfo.is_new_master:%d ,gWlccMasterInfo.is_master:%d,Step_dev_discovery:%d",gWlccMasterInfo.is_new_master ,gWlccMasterInfo.is_master, reals.Step_dev_discovery);
	if((gWlccMasterInfo.is_new_master != gWlccMasterInfo.is_master)
		|| (WLCC_EMS_MODE_INVALID == gWlccMasterInfo.is_new_master)
		|| (WLCC_EMS_MODE_INVALID == gWlccMasterInfo.is_master))
	{
		gWlccMasterInfo.last_parallel_time += 100;
		if(gWlccMasterInfo.last_parallel_time > 3000)
		{
			gWlccMasterInfo.last_parallel_time = 0;
			// 如果新主节点类型无效，说明没有竞争主节点，即单机运行
			if(WLCC_EMS_MODE_INVALID == gWlccMasterInfo.is_new_master){
				gWlccMasterInfo.is_new_master = WLCC_EMS_MODE_MASTER;
				gWlccMasterInfo.is_master = WLCC_EMS_MODE_MASTER;
			}
			else
			{
				// 进入到这个地方，说明有并机，可以根据类型判断是否做进一步处理
				gWlccMasterInfo.is_master = gWlccMasterInfo.is_new_master; 
			}
			ESP_LOGI(TAG, "master pk timeout. master mode:%d", gWlccMasterInfo.is_master);
		}
	}
}

void wlcc_master_offline_check(void)
{
	uint32_t now_time_s;
	uint32_t diff_time_s;
	time_t now;

    if(WLCC_EMS_MODE_SLAVER == gWlccMasterInfo.is_master)
	{
		time(&now);
		now_time_s = (long long)now;
		diff_time_s = now_time_s - gWlccMasterInfo.master_info.last_online_time;
		if(diff_time_s > 60)
		{
			ESP_LOGE(TAG, "master_offline. now_time_s:%lu ,last_online_time:%lu",(uint32_t)now_time_s,gWlccMasterInfo.master_info.last_online_time);
			reals.Step_dev_discovery = WLCC_FRAME_HEADER_TRIGER;
			wlcc_master_triger();
		}
	}
}

void wlcc_master_check(void)
{
	static uint8_t master_tick_cnt = 0;
	
	wlcc_master_pk_timeout_check();
	wlcc_master_offline_check();

	if(master_tick_cnt++ > 150)
	{
		master_tick_cnt = 0;
		ESP_LOGI(TAG, "master mode:%d", gWlccMasterInfo.is_master);
	}
}

void wlcc_modbus_dev_update(uint16_t dev_type, uint64_t dev_sn, uint32_t time)
{
	uint8_t dev_id[32]={};
	switch(dev_type)
	{	
		case SN_TYPE_S1:
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE		
			dev_modbus_s1_update(dev_sn, time);
#endif			
			break;
			
		case SN_TYPE_METER:
#ifdef CONFIG_WLCC_METER_SUPPLY_ENABLE		
			dev_modbus_meter_update(dev_type, dev_sn,dev_id, time,0);
#endif			
			break;
			
		case SN_TYPE_AC2AC:
		case SN_TYPE_NPP004:
		case SN_TYPE_NPP005:
		case SN_TYPE_COMMON_INVERTER_WIRELESS:
			dev_modbus_other_inv_update(dev_type, dev_sn, time);
			break;
		default:
			break;
	}
}


/**
 * @brief  设备发现信息处理
 * @param  data 新发现的设备信息
 */
void update_device_discovery_info(wlcc_dev_info_t dev_info, uint16_t dst_type, uint64_t dst_sn)
{
//    uint8_t ret = 0;
	uint8_t offline_index = NET_WIFI_MAX_POINT;
	struct in_addr addr;
	time_t now;

	if((0 == dev_info.dev_type) || (0 == dev_info.dev_sn)){
		ESP_LOGW(TAG, "invalid device, type[%d] sn(%llu)", dev_info.dev_type, dev_info.dev_sn);
		return;
	}

	time(&now);

	wlcc_master_online_time_update(dev_info, (long long)now);

    for (int i = 0; i < NET_WIFI_MAX_POINT; i++)
    {
    	if((dev_info.dev_type == gWlccDevList[i].dev_type) 
			&& (dev_info.dev_sn == gWlccDevList[i].dev_sn))
		{
			if(gWlccDevList[i].online_status == WLCC_STATUS_OFFLINE_HAL) {
                ESP_LOGI(TAG, "device update. sn(%llu)", gWlccDevList[i].dev_sn);
            }
			gWlccDevList[i].dev_priority = dev_info.dev_priority;
			gWlccDevList[i].online_status = WLCC_STATUS_ONLINE;
			//IP地址可能变化，随时更新
			if(dev_info.ip)
			{
				gWlccDevList[i].ip = dev_info.ip;
				gWlccDevList[i].port = dev_info.port;
			}
			gWlccDevList[i].last_online_time = (long long)now;
			addr.s_addr = htonl(gWlccDevList[i].ip); // 转换为网络字节序

			ESP_LOGI(TAG, "Update device(sn:%llu, index(%d)), ip:%s, port:%d",
                        gWlccDevList[i].dev_sn, i,
                        inet_ntoa(addr), gWlccDevList[i].port);

			wlcc_modbus_dev_update(dev_info.dev_type, dev_info.dev_sn, (long long)now);
			if(dev_info.dev_type && dev_info.dev_sn){
				dev_modbus_dev_add(dev_info.dev_type, dev_info.dev_sn, 0, 0, 0, 1, MD_CHL_WIFI_WLCC);
			}

            return;
		}
        else if(WLCC_STATUS_OFFLINE == gWlccDevList[i].online_status)//新设备填充到前面，不在线的设备位置
        {
        	if(NET_WIFI_MAX_POINT == offline_index){
				offline_index = i;
			}
        }
    }

    // 新设备填充
	if(NET_WIFI_MAX_POINT > offline_index)
	{
		gWlccDevList[offline_index].dev_type = dev_info.dev_type;
		gWlccDevList[offline_index].dev_sn = dev_info.dev_sn;
		gWlccDevList[offline_index].ip = dev_info.ip;
		gWlccDevList[offline_index].port = dev_info.port;
		gWlccDevList[offline_index].dev_priority = dev_info.dev_priority;

        gWlccDevList[offline_index].online_status = WLCC_STATUS_ONLINE;
        gWlccDevList[offline_index].last_online_time =(long long)now;

        addr.s_addr = htonl(gWlccDevList[offline_index].ip); // 转换为网络字节序

		ESP_LOGI(TAG, "add device(sn:%llu, index(%d)), ip:%s, port:%d",
                    gWlccDevList[offline_index].dev_sn, offline_index,
                    inet_ntoa(addr), gWlccDevList[offline_index].port);

		wlcc_modbus_dev_update(dev_info.dev_type, dev_info.dev_sn, (long long)now);
		if(dev_info.dev_type && dev_info.dev_sn){
			dev_modbus_dev_add(dev_info.dev_type, dev_info.dev_sn, 0, 0, 0, 1, MD_CHL_WIFI_WLCC);
		}
	}

    /*设备信息变化*/
//    reals.net_point_Comein = ret;
}

int wlcc_dev_online_check(uint16_t dev_type, uint64_t dev_sn)
{
	uint8_t i;
	ESP_LOGI(TAG,"wlcc_dev_online_check dev_type:%u,dev_sn:%llu",dev_type,dev_sn);
	for (i = 0; i < NET_WIFI_MAX_POINT; i++)
	{
		//ESP_LOGI(TAG,"i:%d,gWlccDevList[i].dev_type:%u,gWlccDevList[i].dev_sn:%llu,online_status:%d",i,gWlccDevList[i].dev_type,gWlccDevList[i].dev_sn,gWlccDevList[i].online_status);
		if((dev_type == gWlccDevList[i].dev_type) && (dev_sn == gWlccDevList[i].dev_sn))
		{
			if(WLCC_STATUS_OFFLINE != gWlccDevList[i].online_status){
				return 1;
			}
		}
	}

	return 0;
}

int wlcc_dev_detect_stable_conn_check(uint16_t dev_type, uint64_t dev_sn)
{
	uint8_t i;

	for (i = 0; i < NET_WIFI_MAX_POINT; i++)
	{
		if((dev_type == gWlccDevList[i].dev_type) && (dev_sn == gWlccDevList[i].dev_sn))
		{
			if(WLCC_STATUS_ONLINE == gWlccDevList[i].online_status){
				return 1;
			}
		}
	}

	return 0;
}

int wlcc_dev_online_time_get(uint16_t dev_type, uint64_t dev_sn)
{
	uint8_t i;

	for (i = 0; i < NET_WIFI_MAX_POINT; i++)
	{
		if((dev_type == gWlccDevList[i].dev_type) && (dev_sn == gWlccDevList[i].dev_sn)){
			return gWlccDevList[i].last_online_time;
		}
	}

	return 0;
}


// 100ms tick
void wlcc_dev_offline_check(void)
{
    uint8_t dev_num = 0;
	uint32_t now_time_s = 0;
	uint32_t diff_time_s = 0;
	time_t now;

	time(&now);
	now_time_s = (long long)now;
	
	for (int i = 0; i < NET_WIFI_MAX_POINT; i++)
    {
        if(WLCC_STATUS_OFFLINE != gWlccDevList[i].online_status)
		{
			diff_time_s = now_time_s - gWlccDevList[i].last_online_time;
			ESP_LOGI(TAG,"now_time_s(%lu)-gWlccDevList[%d].last_online_time(%lu)=diff_time_s(%lu)",now_time_s,i,gWlccDevList[i].last_online_time,diff_time_s);			
			if(diff_time_s > 300)
			{
				ESP_LOGD(TAG,"wlcc_dev_offline_check 300s:gWlccDevList[%d].dev_type:%u,dev_sn:%llu",i,gWlccDevList[i].dev_type,gWlccDevList[i].dev_sn);
				gWlccDevList[i].online_status = WLCC_STATUS_OFFLINE;
				dev_modbus_dev_del(gWlccDevList[i].dev_type, gWlccDevList[i].dev_sn);
                
				switch(gWlccDevList[i].dev_type)
				{
                    case SN_TYPE_COMMON_INVERTER_WIRELESS:
                    case SN_TYPE_NPP004:
                    case SN_TYPE_NPP005:
#ifdef CONFIG_WLCC_INV_SUPPLY_ENABLE                        
                        dev_modbus_other_inv_del(gWlccDevList[i].dev_sn);
#endif
                        break;
					default:
						break;
				}
			} else if (diff_time_s > 120) {
				ESP_LOGD(TAG,"wlcc_dev_offline_check 120s:%d dev_type:%u,dev_sn:%llu",i,gWlccDevList[i].dev_type,gWlccDevList[i].dev_sn);
				switch(gWlccDevList[i].dev_type)
				{				
                    case SN_TYPE_S1:
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE	
                        gWlccDevList[i].online_status = WLCC_STATUS_OFFLINE;
                        dev_modbus_dev_del(gWlccDevList[i].dev_type, gWlccDevList[i].dev_sn);
                        dev_modbus_s1_del(gWlccDevList[i].dev_sn);
#endif						
                        break;
						
					default:
						break;
				}
            } else if (diff_time_s > 60) {
                ESP_LOGD(TAG,"wlcc_dev_offline_check 60s:%d dev_type:%u,dev_sn:%llu",i,gWlccDevList[i].dev_type,gWlccDevList[i].dev_sn);
                switch(gWlccDevList[i].dev_type)
                {               
                    case SN_TYPE_METER:
#ifdef CONFIG_WLCC_METER_SUPPLY_ENABLE	
                        gWlccDevList[i].online_status = WLCC_STATUS_OFFLINE;
                        dev_modbus_dev_del(gWlccDevList[i].dev_type, gWlccDevList[i].dev_sn);
                        dev_modbus_meter_offline_set(gWlccDevList[i].dev_sn);
#endif						
                        break;
                        
                    default:
                        break;
                }
			} else if(diff_time_s > 30){
				ESP_LOGD(TAG,"wlcc_dev_offline_check 30s:gWlccDevList[%d].dev_type:%u,dev_sn:%llu",i,gWlccDevList[i].dev_type,gWlccDevList[i].dev_sn);
				gWlccDevList[i].online_status = WLCC_STATUS_OFFLINE_HAL;
                dev_num++;
			} else {
                dev_num++;
			}
        }
    }

    gWlccDevNum = dev_num;
}

uint8_t wlcc_dev_online_num_get(void) {
    return gWlccDevNum;
}

/**
 * @brief 生成TLV上报数据
 * @param blocks 输出TLV块数组（需要预先分配，至少3个元素）
 * @param block_count 输出实际生成的TLV块数量
 * @param value_buf 值缓冲区（用于存储TLV块的值数据）
 * @param value_buf_size 值缓冲区大小
 */
int wlcc_tlv_generate_data_iot_heartbeat_reg40000(wlcc_tlv_block_t *blocks,
                                uint16_t *block_count,
                                uint8_t *value_buf,
                                uint16_t value_buf_size)
{
	extern STRUCT_CAN_READ can_parallel_node_rd[];
	
    if (blocks == NULL || block_count == NULL || value_buf == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return IOT_ERR_INVALID_ARG;
    }

    if (value_buf_size < 20)
    {
        ESP_LOGE(TAG, "Value buffer too small: need at least 20 bytes");
        return IOT_ERR_INVALID_SIZE;
    }

    *block_count = 0;
    uint16_t value_offset = 0;
	uint16_t reg_value = 0;

	// TLV块:
    blocks[*block_count].addr = MOD_REG_START_ADDR_40000; // 工作模式
    blocks[*block_count].length = sizeof(MOD_STRUCT_reg40000_IotDebugStatus_t) + 2;
    blocks[*block_count].value = (const uint8_t *)&top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug;
	value_offset += blocks[*block_count].length;
	*block_count += 1;

    ESP_LOGI(TAG, "Generated %d TLV blocks", *block_count);
    return IOT_OK;
}

/**
 * @brief 获取指定类型在线设备数量
 * @param dev_type 设备类型
 * @return 在线设备数量
 */
uint16_t wlcc_online_dev_info_get(uint16_t dev_type, wlcc_dev_t *dev_infos, uint16_t max_num)
{
    uint8_t i = 0;
    uint16_t count = 0;

    for (i = 0; i < sizeof(gWlccDevList)/sizeof(gWlccDevList[0]); i++)
    {
        /* 设备在线、SN非零并且设备型号相同才认为是相同类型设备 */
        if(WLCC_STATUS_OFFLINE != gWlccDevList[i].online_status
            && 0 != gWlccDevList[i].dev_sn
            && dev_type == gWlccDevList[i].dev_type)
        {
            if (dev_infos != NULL && count < max_num)
            {
                memcpy(&dev_infos[count], &gWlccDevList[i], sizeof(wlcc_dev_t));
				ESP_LOGI(TAG,"wlcc_online_dev_info_get i:%d,online_status:%d,dev_sn:%llu dev_type:%u--%u",i,gWlccDevList[i].online_status,gWlccDevList[i].dev_sn,dev_type,gWlccDevList[i].dev_type);
				count++;
            }
            
        }
    }

    return count;
}


// 1s
void wlcc_period_read_handle(void)
{
	static uint8_t tick_100ms_cnt = 0;
	static uint8_t last_dev_index = 0;
	static uint8_t read_next = 0;

	uint16_t dev_type ; //机型序号(机型压缩序号)
	uint64_t dev_sn;
	uint8_t dev_index = 0;
	md_read_t modbus_read = {.reg_addr=0, .reg_num=0};
	uint8_t modbusBuff[256];
	modbus_addr_info_t src_addr;

	uint16_t input_len = 0;
	uint8_t tx_len = 0;
	int ret = 0;

	tick_100ms_cnt++;
	if(read_next)
	{
		if(tick_100ms_cnt < 30){
			return;
		}
		read_next = 0;
	}
	
	if(tick_100ms_cnt > 200){
		tick_100ms_cnt = 0;
	}

	if(0 == (tick_100ms_cnt%10))
	{
		if(WLCC_STATUS_OFFLINE != gWlccDevList[read_next].online_status)
		{
			dev_type = gWlccDevList[read_next].dev_type;
			dev_sn = gWlccDevList[read_next].dev_sn;

			if(dev_type && dev_sn)
			{
				switch(dev_type)
				{
					case SN_TYPE_S1:
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE					
						modbus_read = dev_modbus_s1_polling_read_rtn_handle(dev_type, dev_sn, wlcc_modbus_poll_step);
#endif						
						break;
					case SN_TYPE_METER:
#ifdef CONFIG_WLCC_METER_SUPPLY_ENABLE					
						modbus_read = dev_modbus_meter_polling_read_handle(dev_type, dev_sn, wlcc_modbus_poll_step);
#endif						
						break;
					case SN_TYPE_AC2AC:
						modbus_read = dev_modbus_other_inv_polling_read_handle(dev_type, dev_sn, wlcc_modbus_poll_step);
						break;
					default:
						break;
				}

				ESP_LOGI(TAG, "Poll reg_addr = %d, reg_num = %d", modbus_read.reg_addr, modbus_read.reg_num);

				if(modbus_read.reg_num > 0)
				{
					wlcc_modbus_poll_step++;
					tx_len = Modbus_MasterReadCmd_03H(modbus_read.reg_addr, modbus_read.reg_num, modbusBuff, 0, 0);
					ESP_LOGE(TAG, "Poll md len=%d!", tx_len);
					src_addr.channel = MD_CHL_SELF;
					src_addr.dev_type = SN_TYPE_SELF;
					src_addr.dev_sn = dev_factory.dev_sn;
					src_addr.regAddr = modbus_read.reg_addr;
					src_addr.regNum = modbus_read.reg_num;
					wlcc_modbus_msg_send(modbusBuff, tx_len, dev_type, dev_sn, src_addr);
				}

                // TODO:调用点暂时放在这里
                // 每10秒发送一次TLV格式的21000帧（周期上报）
                static uint8_t tlv_send_counter = 0;
                if (++tlv_send_counter >= 10)  // 每10秒发送一次
                {
                    tlv_send_counter = 0;
                    // 使用测试数据发送（NULL, 0）
//                    wlcc_modbus_msg_send_21000_tlv(NULL, 0, dev_type, dev_sn);
                }

			}
		}
		
		for (int i = (last_dev_index+1); i < NET_WIFI_MAX_POINT; i++)
	    {
	        if(WLCC_STATUS_OFFLINE != gWlccDevList[i].online_status){
	        	dev_index = i;
				break;
	        }
	    }

		ESP_LOGE(TAG, "Poll switch [%d] to next dev!", last_dev_index);
		ESP_LOGI(TAG, "self_ip = %d.%d.%d.%d", top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[0], 
														top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[1],
														top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[2],
														top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[3]);

		read_next = 1;
		tick_100ms_cnt = 0;
		wlcc_modbus_poll_step = 0;
		last_dev_index = dev_index;
	}
}

/**
 * @brief 生成TLV上报数据
 * @param blocks 输出TLV块数组（需要预先分配，至少3个元素）
 * @param block_count 输出实际生成的TLV块数量
 * @param value_buf 值缓冲区（用于存储TLV块的值数据）
 * @param value_buf_size 值缓冲区大小
 */
int wlcc_build_tlv_report_data(wlcc_tlv_block_t *blocks,
                                uint16_t *block_count,
                                uint8_t *value_buf,
                                uint16_t value_buf_size)
{
	extern STRUCT_CAN_READ can_parallel_node_rd[];
	
    if (blocks == NULL || block_count == NULL || value_buf == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return IOT_ERR_INVALID_ARG;
    }

    if (value_buf_size < 20)
    {
        ESP_LOGE(TAG, "Value buffer too small: need at least 20 bytes");
        return IOT_ERR_INVALID_SIZE;
    }

    *block_count = 0;
    uint16_t value_offset = 0;
	uint16_t reg_value = 0;

    // TLV块:SOC，用于和S1进行SOC联动控制
    blocks[*block_count].addr = 102; // SOC
    blocks[*block_count].length = 2;
    blocks[*block_count].value = &value_buf[value_offset];
	reg_value = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.soc;
    value_buf[value_offset++] = reg_value&0xFF;
    value_buf[value_offset++] = (reg_value>>8)&0xFF;
	*block_count += 1;

    ESP_LOGI(TAG, "Generated %d TLV blocks", *block_count);
    return IOT_OK;
}

void wlcc_report_tick(void)
{
	if (IOT_OK != is_ready_wlcc_network()){
		return;
	}

    static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 30秒上报一次
    if((now_time - pre_time) < 30000)
    {
        return ;
    }
    pre_time = now_time;

	wlcc_tlv_block_t tlv_blocks[TLV_MAX_BLOCK_COUNT] = {0};
    uint16_t tlv_block_count = 0;
    uint8_t value_buf[256]; // 用于存储数据的值
	uint16_t offset = 0;
	uint16_t u16Tempdata = 0;
    uint8_t *data_buf = wlcc_report_data_buf;

	wlcc_build_tlv_report_data(tlv_blocks, &tlv_block_count, value_buf, sizeof(value_buf));
	data_buf[offset++] = TLV_PROTOCOL_VERSION_REPORT & 0xFF;
	data_buf[offset++] = (TLV_PROTOCOL_VERSION_REPORT>>8) & 0xFF;
	
	data_buf[offset++] = 0; // 帧序号
	data_buf[offset++] = 0; // 帧序号

	data_buf[offset++] = 1; // 设备总数低字节
	data_buf[offset++] = 0; // 设备总数高字节

	memcpy((uint8_t *)&data_buf[offset], (uint8_t *)&dev_factory.dev_sn, 8); // 源 SN
    offset += 8;

    // 机型序号_源设备
    u16Tempdata = SN_TYPE_SELF;
    data_buf[offset++] = u16Tempdata & 0xFF;
    data_buf[offset++] = u16Tempdata >> 8;

	// 计算TLV数据总长度
    uint16_t tlv_total_length = wlcc_tlv_calculate_length(tlv_blocks, tlv_block_count);

    // 后续数据长度 (2B) - TLV数据总长度
    data_buf[offset++] = (uint8_t)(tlv_total_length & 0xFF);
    data_buf[offset++] = (uint8_t)(tlv_total_length >> 8);

    // 编码TLV数据
    if (tlv_total_length > 0 )
    {
        int tlv_encoded_len = wlcc_tlv_encode(tlv_blocks,
                                               tlv_block_count,
                                               &data_buf[offset],
                                               sizeof(data_buf) - offset);
        if (tlv_encoded_len < 0)
        {
            ESP_LOGE(TAG, "TLV encode failed: %d", tlv_encoded_len);
            return;
        }
        offset += tlv_encoded_len;
    }
	
	wlcc_modbus_msg_send_21000(0, data_buf, offset, 0, 0);
}

/**
 * @brief 发送40000帧（周期上报，不需要响应）
 * @return 0成功，失败返回负数
 * @note 周期上报类型（WLCC_FRAME_TYPE_PERIOD），不需要等待响应，不添加到pending列表
 */
static int wlcc_modbus_iot_heartbeat_report(void)
{
	uint16_t len = sizeof(MOD_STRUCT_reg40000_IotDebugStatus_t) + 2;
	uint8_t *pModbusData = iot_calloc(len + 10);
	if(NULL == pModbusData) {
		return -1;
	}

	uint8_t *pWlccData = iot_calloc(len + 60);
	if(NULL == pWlccData) {
		free(pModbusData);
		return -1;
	}

	uint8_t regNum = (len + 1) / 2;		
    uint16_t modbus_len = Modbus_MasterWriteCmd_06H_10H(MOD_REG_START_ADDR_40000, regNum, false, pModbusData, MD_SUMMARY_ADDR, MD_CHL_SELF);
	uint16_t tx_len = wlcc_build_data_send_frame(WLCC_FRAME_TYPE_PERIOD, 0, 0, pModbusData, modbus_len, pWlccData);

    // 使用队列发送
    // 加密和实际发送在wlcc_send_task()中完成
    int ret = wlcc_msg_send_to_queue(pWlccData, tx_len, NULL, 0);
    if (ret == 0){
        ESP_LOGI(TAG, "iot heartbeat report queued: %d bytes, reg num:%d", tx_len, regNum);
    }
    else{
        ESP_LOGE(TAG, "iot heartbeat report failed: %d", ret);
    }

	free(pModbusData);
	free(pWlccData);

    // 周期上报不需要添加到pending列表，因为不需要等待响应
    return ret;
}

/**
 * @brief 发送40000帧（周期上报，不需要响应）
 * @return 0成功，失败返回负数
 * @note 周期上报类型（WLCC_FRAME_TYPE_PERIOD），不需要等待响应，不添加到pending列表
 */
static int wlcc_modbus_reg40000_param_report(void)
{
	uint16_t len = sizeof(MOD_STRUCT_reg40000);
	uint8_t *pModbusData = iot_calloc(len + 10);
	if(NULL == pModbusData) {
		return -1;
	}

	uint8_t *pWlccData = iot_calloc(len + 60);
	if(NULL == pWlccData) {
		free(pModbusData);
		return -1;
	}

	uint8_t regNum = (len + 1) / 2;		
    uint16_t modbus_len = Modbus_MasterWriteCmd_06H_10H(MOD_REG_START_ADDR_40000, regNum, false, pModbusData, MD_INV_ADDR_START, MD_CHL_WIFI_WLCC);
	uint16_t tx_len = wlcc_build_data_send_frame(WLCC_FRAME_TYPE_PERIOD, 0, 0, pModbusData, modbus_len, pWlccData);

    // 使用队列发送
    // 加密和实际发送在wlcc_send_task()中完成
    int ret = wlcc_msg_send_to_queue(pWlccData, tx_len, NULL, 0);
    if (ret == 0){
        ESP_LOGI(TAG, "inv reg40000 report queued: %d bytes, reg num:%d", tx_len, regNum);
    }
    else{
        ESP_LOGE(TAG, "inv reg40000 report failed: %d", ret);
    }

	free(pModbusData);
	free(pWlccData);

    // 周期上报不需要添加到pending列表，因为不需要等待响应
    return ret;
}

/*------------------------------------------------------------------------------
 Function: iot_tx_rx_counter_save
 -----------------------------------------------------------------------------*/
/**
  * @brief      当前收发情况暂存，便于计算指定时间内帧收发情况
  * @param[out] None
  * @return     void
  */
static void iot_tx_rx_counter_save(void)
{
    reals.last_can_inv_rx_counter     = reals.can_inv_rx_counter;
    reals.last_can_pack_rx_counter    = reals.can_pack_rx_counter;
    reals.last_can_iot_rx_counter     = reals.can_iot_rx_counter;
    reals.last_can_x_rx_counter       = reals.can_x_rx_counter;
    reals.last_net_frame_tx_counter   = reals.net_frame_tx_counter;
    reals.last_net_frame_rx_counter   = reals.net_frame_rx_counter;
    reals.last_ble_frame_tx_counter   = reals.ble_frame_tx_counter;
    reals.last_ble_frame_rx_counter   = reals.ble_frame_rx_counter;
}

/*
周期上报调试心跳
*/
void wlcc_iot_heartbeat_report(void)
{
	static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if((now_time - pre_time) >= 60000) {
		pre_time = now_time;
#if 0        
		int ret = wlcc_tlv_generate_data_iot_heartbeat_reg40000(test_tlv_blocks, 
											&test_tlv_block_count, 
											test_value_buf, 
											sizeof(test_value_buf));					
		wlcc_modbus_msg_send_21000_tlv(test_tlv_blocks, test_tlv_block_count, 0, 0);
#else
        // Modbus发送40000帧（周期上报，不需要响应）
        wlcc_modbus_iot_heartbeat_report();
#endif

        // 收发情况暂存，便于计算指定时间内帧收发情况
        iot_tx_rx_counter_save();
	}
}

/*
周期上报40000段透传数据，不同于IOT的心跳帧
*/
void wlcc_inv_reg40000_Param_report(void)
{
	static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if((now_time - pre_time) >= 5000) {
		pre_time = now_time;
#if 0        
		int ret = wlcc_tlv_generate_data_iot_heartbeat_reg40000(test_tlv_blocks, 
											&test_tlv_block_count, 
											test_value_buf, 
											sizeof(test_value_buf));					
		wlcc_modbus_msg_send_21000_tlv(test_tlv_blocks, test_tlv_block_count, 0, 0);
#else
        // Modbus发送40000帧（周期上报，不需要响应）
        wlcc_modbus_reg40000_param_report();
#endif

        // 收发情况暂存，便于计算指定时间内帧收发情况
        iot_tx_rx_counter_save();
	}
}

/**
 * @brief UDP无线设备间协议初始化
 */
void wlcc_init(void)
{
	memset(&gWlccDevList, 0, sizeof(gWlccDevList));
	wlcc_master_triger();
	
    if (NULL == ciphertext)
    {
        ciphertext = (uint8_t *)heap_caps_malloc(WLCC_MAX_FRAME_SIZE, MALLOC_CAP_SPIRAM);
        if (NULL == ciphertext)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for recv_buf");
            return;
        }
    }

    if (NULL == send_queue)
    {
        send_queue = xQueueCreate(10, sizeof(wlcc_msg_t));
        assert(send_queue != NULL);
    }

    if (NULL == plaintext)
    {
        plaintext = (uint8_t *)heap_caps_malloc(WLCC_MAX_FRAME_SIZE, MALLOC_CAP_SPIRAM);
        if (NULL == plaintext)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for recv_buf");
            return;
        }
    }
}

/**
 * @brief UDP无线设备间协议开始
 */
void wlcc_start(netif_type_t netif_type)
{
    wlcc_init();
    create_wlcc_network(netif_type);
}

/**
 * @brief 无线设备间协议停止
 */
void wlcc_stop(void)
{
    // 停止wlcc通信
    destroy_wlcc_network();

    // 清除所有wlcc设备
    dev_modbus_dev_del_by_channel(MD_CHL_WIFI_WLCC);
	memset(&gWlccDevList, 0, sizeof(gWlccDevList));
}

/**
 * @brief 接收并处理数据
 */
void wlcc_recv_task(void)
{
    char src_ip[32] = {0};
    uint16_t src_port = 0;
    int ret = recv_wlcc(ciphertext, WLCC_MAX_FRAME_SIZE, src_ip, &src_port);
    if (ret < 0)
    {
        destroy_wlcc_network();
        ESP_LOGI(TAG, "Failed to receive wlcc data: %d", ret);
        return;
    }

    if (0 == ret){
        return;
    }

	uint32_t src_ip_num = inet_addr(src_ip);
	uint32_t self_ip_num = 0;

	memcpy(&self_ip_num, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4, 4);

	if(self_ip_num == src_ip_num){
		ESP_LOGI(TAG, "self[%s] send data, Ignore!", src_ip);
		return;
	}

    uint16_t plaintext_len = 0;
    wlcc_decrypt(ciphertext, ret, plaintext, &plaintext_len);

    ESP_LOGI(TAG, "Received wlcc data[%d] from %s:%d", plaintext_len, src_ip, src_port);
//    ESP_LOG_BUFFER_HEX_LEVEL(TAG, plaintext, plaintext_len, ESP_LOG_INFO);

    // 处理接收到的数据
    handle_wlcc_frame(plaintext, plaintext_len, src_ip, src_port);
}

/**
 * @brief UDP无线设备间协议发送消息任务
 * @note 该任务会从send_queue中取出数据，并调用send_wlcc发送数据
 */
void wlcc_send_task(void)
{
    wlcc_msg_t data;

    if (send_queue && pdTRUE == xQueueReceive(send_queue, &data, 0))
    {
        ESP_LOGI(TAG, "Sending wlcc data[%d] to %s:%d", data.len, data.dst_ip, data.dst_port);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, data.data, data.len, ESP_LOG_INFO);

        uint16_t ciphertext_len = 0;
        wlcc_encrypt(data.data, data.len, ciphertext, &ciphertext_len);

        int ret = send_wlcc(ciphertext, ciphertext_len, (char *)data.dst_ip, data.dst_port);
        if (ret < 0)
        {
            ESP_LOGE(TAG, "Failed to send wlcc data: %d", ret);
        }
        else
        {
            ESP_LOGI(TAG, "Sent %d bytes of wlcc data", ret);
        }

        // 释放数据内存
        if (data.data)
        {
            free(data.data);
            data.data = NULL;
        }
    }
}

/**
 * @brief UDP无线设备间协议任务
 * @note 包括收发数据和数据处理
 */
void wlcc_task(void)
{
	static uint8_t master_triger_flag = 0;
	static uint8_t wlcc_task_tick_cnt = 0;
	int len = 0;

	static uint32_t pre_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

	if (IOT_OK == is_ready_wlcc_network())
	{
		wlcc_recv_task();
		wlcc_send_task();
	}

	// 100ms执行一次
	if((now_time - pre_time) >= 100) 
	{
		pre_time = now_time;

		wlcc_msg_pending_timeout_check();
		wlcc_dev_offline_check();

#ifdef WLCC_ADDRESS_PARALLEL_ENABLE
        wlcc_master_check();
#else
        gWlccMasterInfo.is_master = WLCC_EMS_MODE_INVALID;
        reals.Step_dev_discovery = WLCC_FRAME_HEADER_COMMON;
#endif

		if (IOT_OK != is_ready_wlcc_network()) {
			master_triger_flag = 0;
			return;
		} else {
            // 连接成功时周期上报IOT调试心跳
            wlcc_iot_heartbeat_report();

#if 0
			// 周期上报逆变透传段数据
			wlcc_inv_reg40000_Param_report();
#endif

            // 周期上报TLV关键信息
            wlcc_report_tick();
		}

#ifdef WLCC_ADDRESS_PARALLEL_ENABLE
		if(0 == master_triger_flag)
		{
			master_triger_flag = 1;
			if((WLCC_EMS_MODE_MASTER == wlcc_is_master())
				|| (WLCC_EMS_MODE_INVALID == wlcc_is_master()))
			{
				wlcc_master_triger();
				reals.Step_dev_discovery = WLCC_FRAME_HEADER_TRIGER;
			}
		}

		if(WLCC_EMS_MODE_MASTER == wlcc_is_master()){
	//		 wlcc_period_read_handle();
		}

		// 1s执行一次
		if(++wlcc_task_tick_cnt > 10)
		{
			wlcc_task_tick_cnt = 0;
			if (plaintext)
			{
				len = device_discovery_is_ready(plaintext, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4);
				if (len > 0)
				{
					ESP_LOGE(TAG, "Send device_discovery_is_ready, len: %d, data:", len);
					ESP_LOG_BUFFER_HEX_LEVEL(TAG, plaintext, len, ESP_LOG_INFO);

					uint16_t ciphertext_len = 0;
					wlcc_encrypt(plaintext, len, ciphertext, &ciphertext_len);

					send_wlcc(ciphertext, ciphertext_len, NULL, 0);
				}
			}
		}

#endif        
	}
}

/**
 * @brief 主动发送modbus消息
 *
 * @param regAddress 寄存器地址
 * @param regNum 寄存器数量
 * @param slave_address 从机地址
 * @param dst_ip 目的IP地址
 * @param dst_port 目的端口
 */
int wlcc_send_modbus_msg(uint16_t regAddress,
                         uint8_t regNum,
                         uint16_t dev_type,
                         uint64_t dev_sn,
                         uint8_t slave_address,
                         uint8_t *dst_ip,
                         uint16_t dst_port,
                         channel_modbus chl) {
    uint16_t len = 0;
    uint16_t tx_len = 0;
    bool broadcast = true;

    tx_len = build_common_modbus_frame(broadcast,
                                       regAddress,
                                       regNum,
                                       plaintext,
                                       dev_type,
                                       dev_sn,
                                       slave_address,
                                       WLCC_FRAME_TYPE_WRITE,
                                       &len,
                                       chl);
    if ( 0 == tx_len ) {
        ESP_LOGE(TAG, "Failed to send wlcc data");
        return -IOT_ERR_WLCC_FORMAT;
    }
    
    uint16_t ciphertext_len = 0;

    ESP_LOGI(TAG, "Sent %d bytes of wlcc modbus data:", tx_len);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, plaintext, tx_len, ESP_LOG_INFO);

    wlcc_encrypt(plaintext, tx_len, ciphertext, &ciphertext_len);
    int ret = send_wlcc(ciphertext, ciphertext_len, (const char *)dst_ip, dst_port);

    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to send wlcc data: %d", ret);
    } else {
        ESP_LOGI(TAG, "Sent %d bytes of wlcc data", ret);
    }
    return ret;
}

