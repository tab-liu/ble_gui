#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "iot_ota.h"
#include "comm_define.h"
#include "can_protocol.h"
#include "esp_log.h"
#include "crc.h"
#include "xmodem_client.h"
#include "filesystem.h"

#include "ota_type.h"
#include "image_handle.h"
#include "http_client.h"
#include "iot_period_task.h"

extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];

static EXT_RAM_BSS_ATTR uint16_t can_ota_finsih_count=0;

#define TAG "[CAN_OTA]"

static uint8_t OTA_SendFrame(uint8_t node_id, uint8_t cmd, const uint8_t *data, uint8_t len, uint8_t dst) {
    uint32_t canId = CAN_ID(2, 0, cmd, dst, esp_canbus_myself_address());
    return OTA_SendMessage(node_id, canId, data, len);
}

static void OTA_ErrCode_Set(CanOtaStruct *ota_status, uint8_t id, uint8_t errorcode) 
{
    ESP_LOGI(TAG,"OTA_ErrCode_Set:%d",errorcode);
    for (uint8_t i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) 
	{
        ota_status->devStatus[i].isOta   = OTA_IDLE;
        ota_status->devStatus[i].errCode = errorcode;
    }
    if(gXmodem_Status.firmware_resend_step !=0)//还处于iot并机CAN升级中
    {
        gXmodem_Status.firmware_resend_step=0;
    }
}

void inv_fault_alarm_status_clr(uint8_t node_id)
{
	/* 清除逆变告警与故障数据 */
	for (int i = 0; i < INV_MAX_NUM; i++)
	{
		inv_announce_struct *p_inv_announce = &Inv_can[node_id].inv_data[i].inv_announce;
		memset(p_inv_announce->alarm, 0x00, sizeof(p_inv_announce->alarm));
		memset(p_inv_announce->fault, 0x00, sizeof(p_inv_announce->fault));
		p_inv_announce->fault5 = 0;

		inv_base_struct *p_inv_base = &Inv_can[node_id].inv_data[i].inv_base;
		memset(p_inv_base->alarm, 0x00, sizeof(p_inv_base->alarm));
		memset(p_inv_base->fault, 0x00, sizeof(p_inv_base->fault));
		p_inv_base->fault5 = 0;
	}

	inv_announce_struct *p_inv_announce = &Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce;
	memset(p_inv_announce->alarm, 0x00, sizeof(p_inv_announce->alarm));
	memset(p_inv_announce->fault, 0x00, sizeof(p_inv_announce->fault));
	p_inv_announce->fault5 = 0;

	inv_base_struct *p_inv_base = &Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base;
	memset(p_inv_base->alarm, 0x00, sizeof(p_inv_base->alarm));
	memset(p_inv_base->fault, 0x00, sizeof(p_inv_base->fault));
	p_inv_base->fault5 = 0;
}

/*

ota升级完成处理

*/
void CanOtaFinsihedCtrl(void)
{
    if(can_ota_finsih_count)
    {
        can_ota_finsih_count--;
        if(!can_ota_finsih_count)//计时结束重启
        {
            set_mqtt_stop();
            ESP_LOGI(TAG, "CanOtaFinsihedCtrlgXmodem_Status.firmware_resend_step:%d",gXmodem_Status.firmware_resend_step );
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }
    }

    if(reals.iot_ota_flag==3)
    {
        gXmodem_Status.firmware_resend_step=0;
        set_mqtt_stop();
        ESP_LOGI(TAG, "Delay 500ms Prepare to restart system!");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    if(reals.iot_ota_end_count)
    {
        reals.iot_ota_end_count--;
        ESP_LOGI(TAG,"reals.iot_ota_end_count:%d",reals.iot_ota_end_count);
        if(!reals.iot_ota_end_count)
        {
           reals.iot_ota_flag=3;
        }
    }
}

void CanOtaStartCtrl(uint8_t type)
{
    switch(type)
    {
        case DEVICE_IOT:
        case DEVICE_ARM:
        case DEVICE_DSP:
        case DEVICE_BMS:
        case DEVICE_DC_DC:
        case DEVICE_DC_HUB:
        ESP_LOGI(TAG,"CanOtaStartCtrl type:%d",type);
            for(uint8_t num = 0;num < DEV_MAIN_NODE_MAX;num++)// 
            {
                uint8_t numpack =0;
                ESP_LOGI(TAG,"CanOtaStartCtrl num:%d pv_total:%d",num,Inv_can[num].inv_data[numpack].inv_pv.pv_number.pv_total);
                if(Inv_can[num].inv_data[numpack].online==1)
                {
                    // if(type == 0x50) //pack广播消息，主包汇总//单逆变器系统的主PACK 序号=0， pack_announce
                // memset((uint8_t*)&Inv_can[num].inv_data[numpack].inv_pv.pv_detail[2],0,sizeof(pv_detail_struct));
                    Inv_can[num].inv_data[numpack].inv_base.PVAllTotalPower=0;
                    Inv[num].mod_reg00100_AppPage1.PVAllTotalPower=0;
                    for(uint8_t i=0;i<Inv_can[num].inv_data[numpack].inv_pv.pv_number.pv_total;i++)
                    {
                        Inv_can[num].inv_data[numpack].inv_pv.total_chg_power=0;
                        Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_power=0;
                        Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_voltage=0;
                        Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_current=0;
                        ESP_LOGI(TAG,"CanOtaStartCtrl:%d, pv power:%lu --%u",num,Inv_can[num].inv_data[numpack].inv_base.PVAllTotalPower,Inv_can[num].inv_data[numpack].inv_pv.pv_detail[i].input_power);
                    }
                }
            }
            Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower=0;
            Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PVAllTotalPower=0;
        break;
        default:
        break;
    }
}

/*

frame: can rx ID+ data

node_id:CAN总线序号，固定0，不引用;仅EMS盒用

*/
void CanOtaSender(uint8_t node_id, const twai_message_t *frame, CanOtaStruct *ota_status) {
    IdStruct canId;
    uint8_t i = 0;
    static uint8_t flag_first = 0;//test httpOTA


    if (!ota_status->sender_work) { /* CAN OTA总线处于升级IOT中 */
        ota_status->main_step = 0;
        return ;
    }
    ESP_LOGI(TAG,"ota_status->main_step:%d",ota_status->main_step);
    switch (ota_status->main_step)
    {
        case 0:
			ESP_LOGI(TAG, "node: %d, step %d: can-ota start", node_id, ota_status->main_step);
//			if (ota_status->ota_data == NULL)
//			{
//				ota_status->sender_work = 0;
//                OTA_ErrCode_Set(ota_status, ota_status->sender_target_id, OTA_ERR_FILE_TYPE);
//                ESP_LOGE(TAG, "upgrade data stored in RAM is not exist");
//                break;
//			}

            ota_status->pct_78H.type = ota_status->sender_type;
            ota_status->pct_78H.errcode = 0;
            ota_status->pct_78H.where = 0;

            for (uint8_t i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
                ota_status->devStatus[i].type = ota_status->sender_type;
                ota_status->devStatus[i].pct = 0;
                ota_status->devStatus[i].errCode = 0;
            }

            if (ota_status->sender_type > TypeCnt) { /* 非法文件退出升级 */
                ota_status->sender_work = 0;
                OTA_ErrCode_Set(ota_status, ota_status->sender_target_id, OTA_ERR_FILE_TYPE);
                ESP_LOGE(TAG, "File type unkonw %d (File rang: 0-%d)", ota_status->sender_type, TypeCnt);
                break;
            }

            for (uint8_t i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
                ota_status->devStatus[i].level = IOT_TO_DEV;
                ota_status->devStatus[i].type = ota_status->start.fileType;
                ota_status->devStatus[i].errCode = DATA_NORMAL;
                ota_status->devStatus[i].pct = 0;

                ota_status->devStatus[i].where = IOT_TO_DEV;//testwx
            }

			ota_status->resendCnt = 0;
            ota_status->main_step++;
			ota_status->retry_max_count = 5;
            ota_status->ota_doing = CAN_OTA_DEV;
			ota_status->period_cmd_delay = 4000;
            ESP_LOGI(TAG, "File version: %lu, size: %dKb", ota_status->start.fileVersion, ota_status->start.fileSize);
            break;

        case 1: // Send OTA Ready
            // 广播发送ready
            if (ota_status->resendCnt >= ota_status->retry_max_count) { /* 重发失败 */
                ESP_LOGE(TAG, "OTA ready failed, can bus is busy resendCnt:%d retry_max_count:%d",ota_status->resendCnt,ota_status->retry_max_count);
				ota_status->resendCnt = 0;
                ota_status->retry_max_count = 1;
                ota_status->main_step = 8;// EXIT OTA
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_BUSY); /* CAN总线发送失败 */
                break;
            }

			ota_status->can_busy = 0;
            if (!OTA_SendFrame(node_id, OTA_READY, (uint8_t *)&ota_status->start, 8, CAN_BROADCAST_ADDRESS)) {
				ota_status->sender_work = 0;
                ESP_LOGE(TAG, "CMD ready failed and exit ota");
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_ERR); /* CAN总线发送失败 */
                break;
            }

			ESP_LOGI(TAG, "node: %d, step %d: ,type:%d ,CMD ready Send", node_id, ota_status->main_step,ota_status->devStatus[0].type);
			ota_status->resendCnt++;
            ota_status->timecnt = 2000;				// 2s 在2s内等待总线空闲
            ota_status->main_step++;				// 下一步
			ota_status->period_cmd_delay = 4000;
			inv_fault_alarm_status_clr(node_id);	// 升级的时候清除该总线上逆变设备的故障与状态信息,待升级完成后设备重新上传
            break;

        case 2: // 延迟2000
            if (!ota_status->timecnt)
            {
				ESP_LOGI(TAG, "node: %d, step %d: delay 2s end, can busy: %d ,type:%d",
				 node_id, ota_status->main_step, ota_status->can_busy,ota_status->devStatus[0].type);
				if(ota_status->can_busy) {
				    ota_status->main_step--;
				}
				else {
					ota_status->resendCnt = 0;
					ota_status->main_step++;
					ota_status->retry_max_count = 5;
				}
            }

			if(frame) {
                canId.all = frame->identifier;//can rx ID
                ESP_LOGI(TAG,"OTA RECV :0x%x",canId.bit.funcode);
                if(canId.bit.funcode!=0x78)
				    {ota_status->can_busy = 1;}
			}
            break;

        case 3: // Send OTA Start
            if (ota_status->resendCnt >= ota_status->retry_max_count) { /* 重发失败 */
                ota_status->resendCnt = 0;
                ESP_LOGE(TAG, "OTA start send max cnt failed");
                ota_status->retry_max_count = 1;
                ota_status->main_step = 8;// EXIT OTA
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_NO_RESP); /* CAN总线发送失败 */
                break;
            }
            CanOtaStartCtrl(ota_status->sender_type);
            /* 发送一个假进度,让CAN总线保持静默状态 */
            // OTA_SendFrame(node_id, OTA_PCT, (uint8_t *)&ota_status->block, 8, CAN_BROADCAST_ADDRESS);
            // if (!OTA_SendFrame(node_id, OTA_START, (uint8_t *)&ota_status->start, 8, ota_status->sender_target_id))

            if (!OTA_SendFrame(node_id, OTA_START, (uint8_t *)&ota_status->start, 8, ota_status->sender_target_id))
            {
                ota_status->retry_max_count = 1;
                ota_status->main_step = 8;// EXIT OTA
                ESP_LOGE(TAG, "CMD start failed and exit ota");
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_ERR); /* CAN总线错误 */
                break;
            }

            ESP_LOGI(TAG, "node: %d, step %d: CMD Send Start", node_id, ota_status->main_step);
            ota_status->resendCnt++;
            ota_status->devCnt = 0; // 记录有几个设备应答
            ota_status->timecnt = 4000; // 发送完后等待应答，5s超时
            ota_status->main_step++;
			inv_fault_alarm_status_clr(node_id);	// 升级的时候清除该总线上逆变设备的故障与状态信息,待升级完成后设备重新上传
            break;

        case 4: // 等待Start ack
            if (!ota_status->timecnt) 
            {
                if (ota_status->devCnt == 0) {  // 无应答
                    ota_status->main_step --;
                    ESP_LOGI(TAG, "CMD start none ack");
                }
                else // 有应答
                {
                    ota_status->offset = 0;
                    memset(&ota_status->block, 0, sizeof(OtaBlock));
                    ota_status->pblock_len = 1024;
                    if (!ota_status->pblock_buf) {
                        ota_status->pblock_buf = heap_caps_malloc(ota_status->pblock_len, MALLOC_CAP_SPIRAM);
                        ESP_LOGI(TAG, "------CanOtaSender malloc block buffer");
                    }

                    if (!ota_status->pblock_buf)
                    {
                        ESP_LOGE(TAG, "Ota send buffer allocate failed (size=%u bytes)", ota_status->pblock_len);
                        OTA_ErrCode_Set(ota_status, ota_status->sender_target_id, OTA_ERR_MEMONY);
                        ota_status->retry_max_count = 1;
                        ota_status->main_step = 8;// EXIT OTA
                    }
                    else
                    {
                        ota_status->main_step++;
                    }
                }
                break;
            }

            if (!frame) {
                break;
            }

            canId.all = frame->identifier;
            if (canId.bit.funcode == OTA_START_ACK) {
				ESP_LOGI(TAG, "node: %d, step %d: ota start ack", node_id, ota_status->main_step);
                ota_status->timecnt = 2000; // 5s
                if (frame->data[0] == DATA_NORMAL)  // 设备正常
                {
                    ESP_LOGI(TAG, "CanOtaSender: func 4, sender_target_id:%u canId.bit.src:0x%x", ota_status->sender_target_id,canId.bit.src);
                    ota_status->devStatus[canId.bit.src].errCode = DATA_NORMAL;
                    if (ota_status->sender_target_id == CAN_BROADCAST_ADDRESS) {  /* 广播升级 */
                        ota_status->devCnt++;
                        ota_status->devStatus[canId.bit.src].isOta = OTA_DOING;
                    } else if (ota_status->sender_target_id == canId.bit.src) {
                        ota_status->devCnt++;
                        ota_status->devStatus[canId.bit.src].isOta = OTA_DOING;
                        ota_status->timecnt = 100;
                    }
                } else if (frame->data[0] == DATA_EXIT) { /* 有设备请求退出 */
                    ESP_LOGE(TAG, "OTA devId: 0x%x >>> exit", canId.bit.src);
                    if (ota_status->sender_target_id == CAN_BROADCAST_ADDRESS) {
                        ota_status->devStatus[ canId.bit.src ].isOta = OTA_IDLE;
                        ota_status->devStatus[ canId.bit.src ].errCode = frame->data[1]; // 错误代码
                    } else if (ota_status->sender_target_id == canId.bit.src) {
                        ota_status->devStatus[ canId.bit.src ].isOta = OTA_IDLE;
                        ota_status->devStatus[ canId.bit.src ].errCode = frame->data[1]; // 错误代码
                        ota_status->main_step = 8; // EXIT OTA
                        ota_status->resendCnt = 0;
                        ota_status->retry_max_count = 1;
                    }
                }
            }
            break;

        case 5: // read file block
            if (((ota_status->offset/1024) < ota_status->start.fileSize) && ota_status->pblock_buf)
			{
				ESP_LOGI(TAG, "node: %d, step %d ,pblock_len %d: load ota data", node_id, ota_status->main_step, ota_status->pblock_len);

                int rtn=0;

                //IoT 与DSP/ARM等，写入地址不同
                if ((ota_status->sender_type == IOT)&&(ota_status->sender_group==GROUP_IOT))
                {
                    iot_image_read((ota_status->offset), ota_status->pblock_buf, ota_status->pblock_len);
                }
                else 
                {
                    if (ota_status->pblock_buf)
                    {
                        rtn=iot_image_read((IMAGE_FLASH_AREA_ADDRESS + ota_status->offset), ota_status->pblock_buf, ota_status->pblock_len);	
                        ESP_LOGW(TAG,"iot_image_read rtn :%d",(unsigned int)rtn);
                    }else
                    {
                        ESP_LOGE(TAG, "Ota send buffer  NULL (size=%u bytes)", ota_status->pblock_len);
                    }
                }
                //memcpy(ota_status->pblock_buf, Ota_temp_buffer, ota_status->pblock_len);				
            }
			else if((ota_status->offset/1024) == ota_status->start.fileSize)	//最后一块数据已经发送完成
			{
               ESP_LOGI(TAG, "node: %d, step %d: Read file finish exCRC: 0x%04lx",
                    node_id, ota_status->main_step, ota_status->end.fileCrc32);
                ota_status->main_step = 8; // EXIT OTA (文件读取结束)
                ota_status->resendCnt = 0;
                ota_status->retry_max_count = 5;
                break;
            }

            ota_status->block.blockCrc16 = CanBusCrc16(ota_status->pblock_buf, ota_status->pblock_len);
            ota_status->block.frameTotal = (ota_status->pblock_len + CAN_PAYLOAD_LEN - 1) / CAN_PAYLOAD_LEN;
            ota_status->main_step++; // 下一步,发送block数据
            break;

        case 6: // Send OTA Block
            if (ota_status->resendCnt >= ota_status->retry_max_count) { // 发送次数超限
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_NO_RESP);
                ota_status->main_step = 8;
                ota_status->resendCnt = 0;
                ota_status->retry_max_count = 1;
                break;
            }

            /* 广播播升级 */
            if (ota_status->sender_target_id == CAN_BROADCAST_ADDRESS) {
                ota_status->devCnt = 0;
                for (i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) { // 统计正常OTA设备数量
                    if (ota_status->devStatus[i].isOta == OTA_DOING) {
                        ota_status->sender_target_id = i;
                        ota_status->devCnt++;
                        ota_status->devStatus[i].waitAck = 1;
                    }
                    else
                    {
                        ota_status->devStatus[i].waitAck = 0;
                    }
                }
                ESP_LOGW(TAG,"ota_status->devCnt = %d", ota_status->devCnt);
                if (!ota_status->devCnt) {
                    ota_status->main_step = 8;
                    ESP_LOGE(TAG, "OTA devCnt = 0, resendCnt = 0, exit ota");
                    ota_status->resendCnt = 0;
                    ota_status->retry_max_count = 1;
                    break; // 没有设备需要升级
                } else if (ota_status->devCnt >= 1) // >= AC380单机也走全广播升级(包括achub的升级)
                {
                    ota_status->sender_target_id = CAN_BROADCAST_ADDRESS; // 广播升级
                }
            }

            /* 发送一个假进度,让CAN总线保持静默状态 */
            // OTA_SendFrame(node_id, OTA_PCT, (uint8_t *)&ota_status->block, 8, CAN_BROADCAST_ADDRESS);
            OTA_SendFrame(node_id, OTA_PCT, (uint8_t *)&ota_status->pct_78H, 8, CAN_BROADCAST_ADDRESS);

            // Send Block Start
            if (!OTA_SendFrame(node_id, OTA_BLOCK_START, (uint8_t *)&ota_status->block, 8, ota_status->sender_target_id)) {
                ESP_LOGE(TAG, "BLOCK START send failed and exit ota");
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_ERR); /* CAN总线发送失败 */
                ota_status->main_step = 8;
                ota_status->resendCnt = 0;
                ota_status->retry_max_count = 1;
                return ;
            }

            if(flag_first == 0)
            {
                vTaskDelay(pdMS_TO_TICKS(500));//test httpOTA 首个0x73到0x74之间延迟500ms，http升级时ac380底层对数据的第一包需要反应时间
                flag_first = 1;
                ESP_LOGI(TAG, "node: %d, step %d: first delay 500ms", node_id, ota_status->main_step);
            }
            

            uint8_t  len = 0;
            uint8_t buf[8] = {0};
            uint8_t *pdata      = ota_status->pblock_buf;
            uint16_t remaining  = ota_status->pblock_len;
            while(remaining) { // CAN帧数

                len = ((remaining >= CAN_PAYLOAD_LEN) ? CAN_PAYLOAD_LEN : remaining);
                memcpy(buf + 2, pdata, len);
                if (!OTA_SendFrame(node_id, OTA_BLOCK_DATA, buf, (len + 2), ota_status->sender_target_id)) {  // Send Data
                    ESP_LOGE(TAG, "BLOCK DATA send failed and exit ota");
                    ota_status->main_step = 8;
                    ota_status->resendCnt = 0;
                    ota_status->retry_max_count = 1;
                    OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_ERR); /* CAN总线发送失败 */
                    return ;
                }
                (*(uint16_t *)&buf[0])++;
                pdata += len;
                remaining -= len;
            }

            ESP_LOGI(TAG, "node: %d, step %d: send block: %u [crc16: 0x%04x]",
				node_id, ota_status->main_step, ota_status->block.blockSeq, ota_status->block.blockCrc16);
            ota_status->block.retryCnt = ota_status->resendCnt; //当前数据块重发次数
            ota_status->main_step++;
            ota_status->resendCnt++;
            ota_status->timecnt = 5000; // 等待应答超时5s
            break;


        case 7:
            if (!ota_status->timecnt) {
                ota_status->main_step--; // 重发
                ota_status->period_cmd_delay += 1000; // 重发发送完后等待应答，此处用作重发时 can_ota_summary 获取数据                
                ESP_LOGI(TAG, "node: %d, step %d: Block resend %d", node_id, ota_status->main_step, ota_status->resendCnt);
                break;
            }

            if (!frame) {
                break;
            }

            canId.all = frame->identifier;
            // ESP_LOGI(TAG, "block wait 0x%x", canId.all);
            if (canId.bit.funcode == OTA_BLOCK_DATA_ACK) { // block ack
				ESP_LOGI(TAG, "node: %d, step %d: ota block ack", node_id, ota_status->main_step);
                if (ota_status->devStatus[canId.bit.src].isOta == OTA_DOING) { //
                    ota_status->timecnt = 2000; // 等待应答超时2s
                    switch (frame->data[0])
                    {
                        case DATA_NORMAL: //  设备全部应答
                            ota_status->devStatus[ canId.bit.src ].waitAck = 0;
                            ota_status->devStatus[ canId.bit.src ].errCode = 0;
                            uint8_t pct = (ota_status->block.blockSeq * 100) / ota_status->start.fileSize;
                            ESP_LOGI(TAG,"huangji --- ota ---blockSeq:%u,fileSize:%u",ota_status->block.blockSeq,ota_status->start.fileSize);
                            (pct > 100) ? (ota_status->devStatus[ canId.bit.src ].pct = 100) : (ota_status->devStatus[ canId.bit.src ].pct = pct);
                            ota_status->pct_78H.pct = pct;
                            // set_ota_pct_info(config->model, config->dev_target_id, config->type, 1, 3, pct, OTA_ERR_NORMAL); // 升级进度填充到modbus中
                            ESP_LOGI(TAG, "node: %d, devId: 0x%x pct:%d%%....", node_id, canId.bit.src, ota_status->devStatus[ canId.bit.src ].pct);
							for (i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
                                if (ota_status->devStatus[i].isOta == OTA_DOING &&
                                    ota_status->devStatus[i].waitAck == 1) { // 某个设备还未应答
                                    ESP_LOGI(TAG, "wait devId: 0x%x ack", i);
                                    break;
                                }
                            }

                            if (i == NUM_MAX_CAN_OTA_DEVICE_STATUS)
                            {
                                ota_status->main_step -= 2; // 全应答
                                ota_status->resendCnt = 0;
                                ota_status->offset += ota_status->pblock_len;
                                ota_status->block.blockSeq ++;
                                if (ota_status->sender_group==GROUP_CHARGE)//(ota_status->sender_type == IOT)&&(
                                {
                                    //ESP_LOGI(TAG,"d400s iot ota delay 300ms");
                                  //  vTaskDelay(pdMS_TO_TICKS(300)); // 延迟300ms再发送下一帧
                                }

                            }
                            break;

                        case DATA_RESEND: //  设备要求重发,延迟100ms重发
                            ota_status->sub_step = 0;
                            // ota_status->devStatus[ canId.bit.dst ].errCode = frame->data[1];
                            ESP_LOGI(TAG, "devId: 0x%x ack(resend) errcode 0x%x", canId.bit.src, frame->data[1]);
                            vTaskDelay(pdMS_TO_TICKS(100)); // 延迟
                            break;

                        case DATA_EXIT: //  设备终止OTA
                            ota_status->resendCnt = 0;
                            ota_status->devStatus[ canId.bit.src ].isOta = OTA_IDLE;
                            ota_status->devStatus[ canId.bit.src ].waitAck = 0;
                            ota_status->devStatus[ canId.bit.src ].errCode = frame->data[1]; // 某个设备停止升级
                            ESP_LOGE(TAG, "devId:0x%x ack(exit) errcode(%d)", canId.bit.src, frame->data[1]);
                            // return -1;
                            break;
                    }
                }
            }else if (canId.bit.funcode == OTA_END_ACK) 
            {
                
            }
        break;

        case 8: // Send OTA END
        ESP_LOGW(TAG,"ota_status->resendCnt = %d, ota_status->retry_max_count = %d",ota_status->resendCnt,ota_status->retry_max_count);
            if (ota_status->resendCnt >= ota_status->retry_max_count) { // 发送次数超限
                ESP_LOGE(TAG, "OTA End send max cnt failed");
                ota_status->resendCnt = 0;
				ota_status->main_step = 10;
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_NO_RESP);

                // 记录云端升级的id
                image_info_t *image_info = get_image_info();

                // 超时无响应，云端升级失败，返回升级状态
                if (strlen(image_info->recordId))
                {
                    ESP_LOGE(__func__, "HTTPS_OTA_FAILED: %d", OTA_ERR_NO_RESP);
                    https_ota_state_to_host(image_info->recordId, HTTPS_OTA_FAILED, "no resp"/*OTA_ERR_NO_RESP*/);
                } 
                break;
            }

			ESP_LOGI(TAG, "node: %d, step %d: CMD end Send", node_id, ota_status->main_step);
            ota_status->end.fileType = ota_status->sender_type;
            if (!OTA_SendFrame(node_id, OTA_END, (uint8_t *)&ota_status->end, 8, CAN_BROADCAST_ADDRESS)) {
                ESP_LOGE(TAG, "OTA End send failed and exit ota");
                ota_status->resendCnt = 0;
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_ERR); /* CAN总线发送失败 */
                break;
            }

            ota_status->devCnt = 0;
            for (i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
                if (ota_status->devStatus[i].isOta == OTA_DOING) {
                    ota_status->devStatus[i].waitAck = 1;   // 标记需要应答的设备
                    ota_status->devStatus[i].errCode = OTA_ERR_NORMAL;
                    ota_status->devCnt++;
                }
            }

            if (ota_status->devCnt)
            {
                ota_status->resendCnt++;
                ota_status->timecnt = 4000; // 5s
                ota_status->main_step++;
                ESP_LOGI(TAG, "CMD End send %d times", ota_status->resendCnt);
            }
            else
            {
                ota_status->main_step = 10;
                ota_status->resendCnt = 0;
                ota_status->sender_work = 0;
            }
            break;


        case 9:
            if (!ota_status->timecnt) {
                ota_status->main_step--; // 超时未接收到数据
                break;
            }

            if (!frame) {
                break;
            }

            canId.all = frame->identifier;
            if (canId.bit.funcode == OTA_END_ACK) {
                ESP_LOGI(TAG, "node: %d, step: %d, devId: 0x%x end ack", node_id, ota_status->main_step, canId.bit.src);
                ota_status->timecnt = 2000; // 2s
                ota_status->devStatus[canId.bit.src].waitAck = 0;

                // 记录云端升级的id
                image_info_t *image_info = get_image_info();                

                if (frame->data[1]) 
                {
                    ota_status->devStatus[canId.bit.src].isOta = OTA_IDLE;
                    ota_status->devStatus[canId.bit.src].errCode = frame->data[1]; // OTA错误
                    ESP_LOGE(TAG, "devId: 0x%x error code (%d)", canId.bit.src, frame->data[1]);

                    // 云端升级失败后，返回升级状态
                    if (strlen(image_info->recordId))
                    {
                        ESP_LOGE(__func__, "HTTPS_OTA_FAILED: %d", ota_status->devStatus[canId.bit.src].errCode);
                        https_ota_state_to_host(image_info->recordId, HTTPS_OTA_FAILED, "invalid image"/*OTA_ERR_FILE*/ );//ota_status->devStatus[canId.bit.src].errCode);
                        ESP_LOGI(TAG,"HTTPS_OTA_FAILED AFTER");
                    }                   
                } 
                else 
                {
                    uint8_t pct = (ota_status->block.blockSeq * 100) / ota_status->start.fileSize;
                    (pct > 100) ? (ota_status->devStatus[canId.bit.src].pct = 100) : (ota_status->devStatus[canId.bit.src].pct = pct);

                    ota_status->pct_78H.pct = pct;
                    /* 发送一个假进度,让CAN总线保持静默状态 */
                    // OTA_SendFrame(node_id, OTA_PCT, (uint8_t *)&ota_status->block, 8, CAN_BROADCAST_ADDRESS);
                    OTA_SendFrame(node_id, OTA_PCT, (uint8_t *)&ota_status->pct_78H, 8, CAN_BROADCAST_ADDRESS);//100%

                    ESP_LOGI(TAG, "END_ACK node: %d, devId: 0x%x pct: %d%%...", node_id, canId.bit.src, ota_status->devStatus[canId.bit.src].pct);

                    // 云端升级完成后，返回升级状态
                    ESP_LOGI(TAG,"image_info->recordId len: %d", strlen(image_info->recordId));
                    if (strlen(image_info->recordId))
                    {
                        https_ota_state_to_host(image_info->recordId, HTTPS_OTA_SUCCESS, "");
                    }                   
                    
                    ota_status->devStatus[canId.bit.src].isOta  = OTA_IDLE;
                }

                for (i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
                    if (ota_status->devStatus[i].isOta == OTA_DOING && ota_status->devStatus[i].waitAck == 1) {
                        break; /* 还有未应答的设备 */
                    }
                }

                if (i == NUM_MAX_CAN_OTA_DEVICE_STATUS) {
					ESP_LOGW(TAG, "node: %d, clear phrase 1 device status, set level to phrase 2", node_id);
                    for (i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
                        if (ota_status->devStatus[i].isOta == OTA_DOING) {
                            // ota_status->devStatus[i].pct    = 0;
                            ota_status->devStatus[i].isOta  = OTA_IDLE;//canId.bit.src
                            ota_status->devStatus[i].errCode = 0;
                            // ota_status->devStatus[i].level  = DEV_TO_SUB;	//清除状态,设置为等待子设备状态
                        }
                    }
                    ota_status->main_step++;
                }
            }
            break;

        case 10:
			ESP_LOGW(TAG, "node: %d, step %d: exit can-ota", node_id, ota_status->main_step);
            ota_status->sender_work = 0;
            ota_status->main_step = 0; // exit OTA
            ota_status->resendCnt = 0;


            flag_first = 0;//test httpOTA
            reals.ota_happen = 0;
            can_ota_status[0].ota_doing = 0;
#ifdef FUNC_ONECLICK_UPGRADE_EN
            Can_OneClick_DataClean();
#endif

            if (ota_status->pblock_buf) {
                free(ota_status->pblock_buf);
                ota_status->pblock_buf = NULL;
            }
            ESP_LOGI(TAG, "exit can-ota can_ota_status[0].sender_type:%d",can_ota_status[0].sender_type);
            //IOT OTA CAN转发完成后，需要重启自己，完成自己的升级
			if((IOT ==  can_ota_status[0].sender_type)&&(GROUP_IOT==ota_status->sender_group))    //(1 == gXmodem_Status.firmware_resend_step)
			{
				if (ESP_OK == iot_ota_end()) 
				{
					gXmodem_Status.firmware_resend_step =2;
                    can_ota_finsih_count=10;//10秒后必须重启
                    reals.iot_ota_flag=1;
				}
                ESP_LOGI(TAG, "exit can-ota gXmodem_Status.firmware_resend_step:%d",gXmodem_Status.firmware_resend_step );
                //vTaskDelay(pdMS_TO_TICKS(500));
                //esp_restart();
            }else{
                gXmodem_Status.firmware_resend_step=0;
            }

#ifdef FUNC_ONECLICK_UPGRADE_EN
            Can_OneClickOta_Start_Can(ota_status);
#endif

			// unlink(ota_status->curr_name);      // 不管升级成功还是失败删除本次的文件
			// ESP_LOGI(TAG, "Delete file %s", ota_status->curr_name);
			// memset(ota_status->curr_name, 0, sizeof(ota_status->curr_name));
            break;

        default:
            ota_status->main_step = 0;
            ota_status->resendCnt = 0;
            break;
    }

    if (ota_status->sender_work == 0)
	{
        vTaskDelay(pdMS_TO_TICKS(600));
		ESP_LOGI(TAG, "node: %d, step %d: stop can-ota task", node_id, ota_status->main_step);
        ota_status->ota_doing = CAN_OTA_IDLE;
    }
}

/*
type:OtaFileType ,modbus 701

*/
void CanOtaSender_Begin(uint8_t id, uint8_t type, uint32_t version, CanOtaStruct *ota_status,uint8_t group)
{
	if (ota_status->sender_work != 0)
	{
		ESP_LOGI(TAG, "device(addr: %02x, type: %d) is being upgraded", id, type);
		return;
	}

	ota_status->sender_work = 1;					// 正在升级标志
	ota_status->sender_type = type;             	// 芯片所属类型
	ota_status->sender_main_ver = version / 100;  	// 设备需要发送的版本号
	ota_status->sender_target_id = id;				// 升级的目标ID
    ota_status->sender_group = group;					// 正在升级标志
}

#ifdef FUNC_ONECLICK_UPGRADE_EN
static uint8_t OneClick_State=0;
static uint8_t OneClick_Type=0;
static uint8_t OneClick_Task_Type=0;
static uint16_t OneClick_Stay_Count=0;

#define ONE_CLICK_OTA_TASK_TYPE_BT    1
#define ONE_CLICK_OTA_TASK_TYPE_WIFI    2

static bool OneClick_IsSupported(void)
{
    if ((strcmp(SetData.dev_info_t.INV_dev_type, IOT_TYPE_AP300) == 0) ||
        (strcmp(SetData.dev_info_t.INV_dev_type, IOT_TYPE_AP300V2) == 0)) {
        return false;
    }
    return true;
}

void Can_OneClick_DataClean(void)
{
    int i, node;

    if (!OneClick_IsSupported()) {
        return;
    }

    for (node = 0; node < DEV_MAIN_NODE_MAX; node++) {
        for (i = 0; i < INV_MAX_NUM; i++) {
            Inv_can[node].inv_data[i].inv_base.line_event = 0;
            Inv[node * INV_MAX_NUM + i].mod_reg00100_AppPage1.line_event.all = 0;
        }
    }
}

bool Can_OneClickState_Get(void)
{
    if (!OneClick_IsSupported()) {
        return false;
    }
    return (OneClick_State == 1);
}

void Can_OneClickState_Set(uint8_t state)
{
    if (!OneClick_IsSupported()) {
        return;
    }
    if (OneClick_State != state) {
        OneClick_State = state;
    }
}

void Can_OneClickOta_Start(CanOtaStruct *ota_status, uint8_t upgrade_type)
{
    if (!OneClick_IsSupported()) {
        return;
    }

    OneClick_State = 1;
    OneClick_Type = upgrade_type;
    switch (upgrade_type) {
    case ONECLICK_TYPE_BT:
        OneClick_Task_Type = ONE_CLICK_OTA_TASK_TYPE_BT;
        break;
    case ONECLICK_TYPE_WIFI:
        OneClick_Task_Type = ONE_CLICK_OTA_TASK_TYPE_WIFI;
        break;
    case ONECLICK_TYPE_CAN:
    default:
        break;
    }
    OneClick_Stay_Count = 8;
    ota_status->timecnt = 0;
    ESP_LOGI(TAG, "Can_OneClickOta_Start upgrade_type:%d, stay_count:%u", upgrade_type, OneClick_Stay_Count);
}

void Can_OneClickOta_Keep(void)
{
    if (!OneClick_IsSupported()) {
        return;
    }

    ESP_LOGI(TAG, "Can_OneClickOta_Keep");
    OneClick_State = 1;
    OneClick_Stay_Count = 8;
}

void Can_OneClickOta_End(void)
{
    if (!OneClick_IsSupported()) {
        return;
    }

    ESP_LOGI(TAG, "Can_OneClickOta_End");
    OneClick_State = 2;
    OneClick_Stay_Count = 0;
}

void Can_OneClickOta_Pause(void)
{
    if (!OneClick_IsSupported()) {
        return;
    }

    OneClick_State = 0;
    OneClick_Stay_Count = 0;
}

uint8_t Can_OneClikeOta_Task(const twai_message_t *frame, CanOtaStruct *ota_status, uint16_t OtaStartState)
{
    OtaStart start = {0};
    OtaEnd end = {0};

    (void)frame;
    (void)OtaStartState;

    if (!OneClick_IsSupported() || !OneClick_State) {
        return 0;
    }

    ESP_LOGI(TAG, "OneClick_State:%d", OneClick_State);
    switch (OneClick_State) {
    case 0:
        break;
    case 1:
        if (!ota_status->timecnt) {
            ESP_LOGI(TAG, "OneClick Send");
            OTA_SendFrame(0, OTA_READY, (uint8_t *)&start, 8, CAN_BROADCAST_ADDRESS);
            ota_status->timecnt = 2000;
            if (OneClick_Type == ONECLICK_TYPE_CAN) {
                ota_status->period_cmd_delay = 4000;
            }
            OneClick_Stay_Count--;
            if (!OneClick_Stay_Count) {
                OneClick_State = 2;
            }
        }
        break;
    case 2:
        ESP_LOGI(TAG, "OneClick end Send");
        if (!OTA_SendFrame(0, OTA_END, (uint8_t *)&end, 8, CAN_BROADCAST_ADDRESS)) {
            break;
        }
        OneClick_State = 0;
        OneClick_Type = 0;
        ota_status->ota_doing = CAN_OTA_IDLE;
        ota_status->timecnt = 1000;
        break;
    default:
        break;
    }

    return 1;
}

uint8_t Can_OneClikeOta_Status_Get(void)
{
    if (!OneClick_IsSupported()) {
        return 0;
    }

    if (1 == OneClick_State) {
        if ((ONECLICK_TYPE_CAN == OneClick_Type) && (ONE_CLICK_OTA_TASK_TYPE_WIFI == OneClick_Task_Type)) {
            return 0;
        }
        return 1;
    }
    return 0;
}
#endif
