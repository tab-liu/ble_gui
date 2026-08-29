#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "iot_ota.h"
#include "can_protocol.h"
#include "esp_log.h"
#include "crc.h"
// #include "xmodem_client.h"
#include "filesystem.h"
#include "comm_define.h"
#include "ota_define.h"
#include "image_handle.h"




#ifdef CONFIG_CAN_PORT_ENABLE

#define TAG "[CAN_OTA_SEND]"

static uint8_t OTA_SendFrame(uint8_t node_id, uint8_t cmd, const uint8_t *data, uint8_t len, uint8_t dst) {
    uint32_t canId = CAN_ID(2, 0, cmd, dst, esp_canbus_myself_address());
    return OTA_SendMessage(node_id, canId, data, len);
}

static void OTA_ErrCode_Set(CanOtaStruct *ota_status, uint8_t id, uint8_t errorcode) 
{
    for (uint8_t i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) 
	{
        ota_status->devStatus[i].isOta   = OTA_IDLE;
        ota_status->devStatus[i].errCode = errorcode;
    }
}

static void inv_fault_alarm_status_clr(void)
{
	/* 清除逆变告警与故障数据 */
	for (int i = 0; i < INV_MAX_NUM; i++)
	{
		inv_announce_struct *p_inv_announce = &can_node_rd.Inv[i].inv_announce;
		memset(p_inv_announce->alarm, 0x00, sizeof(p_inv_announce->alarm));
		memset(p_inv_announce->fault, 0x00, sizeof(p_inv_announce->fault));
		p_inv_announce->fault5 = 0;

		inv_base_struct *p_inv_base = &can_node_rd.Inv[i].inv_base;
		memset(p_inv_base->alarm, 0x00, sizeof(p_inv_base->alarm));
		memset(p_inv_base->fault, 0x00, sizeof(p_inv_base->fault));
		p_inv_base->fault5 = 0;
	}

	inv_announce_struct *p_inv_announce = &can_node_rd.Inv[INV_MAX_NUM].inv_announce;
	memset(p_inv_announce->alarm, 0x00, sizeof(p_inv_announce->alarm));
	memset(p_inv_announce->fault, 0x00, sizeof(p_inv_announce->fault));
	p_inv_announce->fault5 = 0;

	inv_base_struct *p_inv_base = &can_node_rd.Inv[INV_MAX_NUM].inv_base;
	memset(p_inv_base->alarm, 0x00, sizeof(p_inv_base->alarm));
	memset(p_inv_base->fault, 0x00, sizeof(p_inv_base->fault));
	p_inv_base->fault5 = 0;
}

/*------------------------------------------------------------------------------
 Function: CanOtaSender
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN OTA 任务
  * @param[in]  uint8_t node_id              
                const twai_message_t *frame  
                CanOtaStruct *ota_status     
  * @param[out] None
  * @return     void
  */
void CanOtaSender(uint8_t node_id, const twai_message_t *frame, CanOtaStruct *ota_status) {
    IdStruct canId = {0};
    uint8_t i = 0;
    uint8_t period_buf[8] = {0};
    static uint8_t flag_first = 0;

    if (!ota_status->sender_work) { 
        ota_status->main_step = 0;//未在升级
        return ;
    }

    /* CAN OTA总线处于升级中 */
    switch (ota_status->main_step)
    {
        case 0:

            /* 非法文件退出升级 */
            if (ota_status->sender_type >= TypeCnt) { 
                ota_status->sender_work = 0;
                OTA_ErrCode_Set(ota_status, ota_status->sender_target_id, OTA_ERR_FILE_TYPE);
                ESP_LOGE(TAG, "File type unkonw %d (File rang: 0-%d)", ota_status->sender_type, TypeCnt);
                break;
            }

            /*进度初始化*/
            for (uint8_t i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
                ota_status->devStatus[i].level = 1;
                ota_status->devStatus[i].type = ota_status->start.fileType;
                ota_status->devStatus[i].errCode = DATA_NORMAL;
                ota_status->devStatus[i].where = IOT_TO_DEV;
                ota_status->devStatus[i].pct = 0;
            }

			ota_status->resendCnt = 0;
            ota_status->main_step++;
			ota_status->retry_max_count = 5;
            ota_status->ota_doing = CAN_OTA_DEV;
			ota_status->period_cmd_delay = CAN_OTA_PERIOD_TIMEOUT;
            ESP_LOGI(TAG, "File version: %lu, size: %dKb", ota_status->start.fileVersion, ota_status->start.fileSize);
            ESP_LOGI(TAG, "node: %d, step %d: can-ota start(init ok)", node_id, ota_status->main_step);
            break;

        case 1: /* 广播发送ready*/
        
            if (ota_status->resendCnt >= ota_status->retry_max_count) { /* 重发失败 */
                ESP_LOGE(TAG, "OTA ready failed, can bus is busy");
				ota_status->resendCnt = 0;
                ota_status->retry_max_count = 1;
                ota_status->main_step = 8;// EXIT OTA
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_BUSY); /* CAN总线发送失败 */
                break;
            }

            if (!OTA_SendFrame(node_id, OTA_READY, (uint8_t *)&ota_status->start, 8, CAN_BROADCAST_ADDRESS)) {
				ota_status->sender_work = 0;
                ESP_LOGE(TAG, "CMD ready failed and exit ota");
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_CAN_ERR); /* CAN总线发送失败 */
                break;
            }

			ESP_LOGI(TAG, "node: %d, step %d: CMD ready Send", node_id, ota_status->main_step);
            ota_status->can_busy = 0;
			ota_status->resendCnt++;
            ota_status->timecnt = 3000;				// 在3s内等待总线空闲
            ota_status->main_step++;				// 下一步
			ota_status->period_cmd_delay = CAN_OTA_PERIOD_TIMEOUT;
			inv_fault_alarm_status_clr();	// 升级的时候清除该总线上逆变设备的故障与状态信息,待升级完成后设备重新上传
            break;

        case 2: /* 延时等待总线空闲*/
            
            if (!ota_status->timecnt)
            {
				ESP_LOGI(TAG, "node: %d, step %d: delay 3s end, can busy: %d",
					node_id, ota_status->main_step, ota_status->can_busy);
				if(ota_status->can_busy) {
					ota_status->main_step--;
				} else {
					ota_status->resendCnt = 0;
					ota_status->main_step++;
					ota_status->retry_max_count = 5;
				}
            }

			if(frame) {
                canId.all = frame->identifier;

                // 20260408: 0x79帧不作为总线繁忙的标志
                if (canId.bit.funcode != OTA_IN_BOOT) ota_status->can_busy = 1;
			}
            break;

        case 3: /* Send OTA Start 0x71*/
        
            if (ota_status->resendCnt >= ota_status->retry_max_count) { /* 重发失败 */
                ota_status->resendCnt = 0;
                ESP_LOGE(TAG, "OTA start send max cnt failed");
                ota_status->retry_max_count = 1;
                ota_status->main_step = 8;// EXIT OTA
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_NO_RESP); /* CAN总线发送失败 */
                break;
            }
            
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
            ota_status->timecnt = 5000; // 发送完后等待应答，5s超时
            ota_status->main_step++;
			inv_fault_alarm_status_clr();	// 升级的时候清除该总线上逆变设备的故障与状态信息,待升级完成后设备重新上传
            break;

        case 4: /* 等待Start ack    0x72*/
            if (!ota_status->timecnt) {
                if (ota_status->devCnt == 0) {  // 无应答
                    ota_status->main_step --;
                    ESP_LOGE(TAG, "CMD start none ack");
                }
                else // 有应答
                {
                    ota_status->offset = 0;
                    memset(&ota_status->block, 0, sizeof(OtaBlock));
                    ota_status->pblock_len = 1024;
                    if (!ota_status->pblock_buf) {
                        ota_status->pblock_buf = iot_calloc(ota_status->pblock_len);
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
				ESP_LOGI(TAG, "node: %d, step %d: load ota data", node_id, ota_status->main_step);

		        iot_image_read((IMAGE_FLASH_AREA_ADDRESS + ota_status->offset), ota_status->pblock_buf, ota_status->pblock_len);	
//				memcpy(ota_status->pblock_buf, Ota_temp_buffer, ota_status->pblock_len);				
            }
			else if((ota_status->offset/1024) == ota_status->start.fileSize)	//最后一块数据已经发送完成
			{
//                ESP_LOGI(TAG, "node: %d, step %d: Read file finish exCRC: 0x%04lx",
//					node_id, ota_status->main_step, ota_status->end.fileCrc32);
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

            /* 广播升级 */
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

                if (!ota_status->devCnt) {
                    ota_status->main_step = 8;
                    ota_status->resendCnt = 0;
                    ota_status->retry_max_count = 1;
                    break; // 没有设备需要升级
                } else if (ota_status->devCnt >= 1) {
                    ota_status->sender_target_id = CAN_BROADCAST_ADDRESS; // 单台设备时也伪装为广播升级（张，罗）
                }
            }


            /* IOT计算升级进度 */
            ota_status->pct_78H.type = ota_status->start.fileType;
            ota_status->pct_78H.pct = (ota_status->block.blockSeq * 100) / ota_status->start.fileSize;
#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE
            /*IOT处理系统升级进度*/
            reals.system_ota.ota_total_pct = System_ota_total_pct_update(ota_status->pct_78H.pct, false, reals.system_ota.ota_mcu_curr_count, reals.system_ota.ota_mcu_total_count, ota_status->is_only_stage);
            ota_status->pct_78H.pct = reals.system_ota.ota_total_pct;
#endif
            /* 发送进度,让CAN总线保持静默状态 */
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
                vTaskDelay(pdMS_TO_TICKS(500));//test httpOTA 首个0x73到0x74之间延迟500ms，http升级时底层对数据的第一包需要反应时间
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
            ota_status->timecnt = 6000; // 等待应答超时6s
            break;


        case 7:
            if (!ota_status->timecnt) {
                ota_status->main_step--; // 重发
                ESP_LOGW(TAG, "node: %d, step %d: Block resend %d", node_id, ota_status->main_step, ota_status->resendCnt);
                break;
            }

            if (!frame) {
                break;
            }

            canId.all = frame->identifier;
            // ESP_LOGI(TAG, "block wait 0x%x", canId.all);
            if (canId.bit.funcode == OTA_BLOCK_DATA_ACK) { // block ack
				ESP_LOGD(TAG, "node: %d, step %d: ota block ack", node_id, ota_status->main_step);
                if (ota_status->devStatus[canId.bit.src].isOta == OTA_DOING) { //
                    ota_status->timecnt = 2000; // 等待应答超时2s
                    switch (frame->data[0])
                    {
                        case DATA_NORMAL: //  设备全部应答
                            #if 0   //暂不执行
                            uint16_t AckSeq = (uint16_t)((frame->data[3] << 8) | frame->data[2]);   //检查包序号，只处理正确包序号的正常应答
                            if ( ota_status->block.blockSeq == AckSeq )
                            #endif    
                            {
                                ota_status->devStatus[ canId.bit.src ].waitAck = 0;
                                ota_status->devStatus[ canId.bit.src ].errCode = 0;
                                uint8_t pct = (ota_status->block.blockSeq * 100) / ota_status->start.fileSize;
                                (pct > 100) ? (ota_status->devStatus[ canId.bit.src ].pct = 100) : (ota_status->devStatus[ canId.bit.src ].pct = pct);
                                ESP_LOGI(TAG, "node: %d, devId: 0x%x pct:%d%%....", node_id, canId.bit.src, ota_status->devStatus[ canId.bit.src ].pct);
                            }
                            
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
                            }
                            break;

                        case DATA_RESEND: //  设备要求重发
                            ESP_LOGE(TAG, "devId: 0x%x ack(resend) errcode 0x%x", canId.bit.src, frame->data[1]);
                            vTaskDelay(pdMS_TO_TICKS(300)); // 延迟300ms重发
                            clear_canbus_rx_queue();      // 清空接收队列，预备重新发送
                            ota_status->main_step--; // 重发
                            break;

                        case DATA_EXIT: //  设备终止OTA
                            ota_status->resendCnt = 0;
                            ota_status->devStatus[ canId.bit.src ].isOta = OTA_IDLE;
                            ota_status->devStatus[ canId.bit.src ].waitAck = 0;
                            ota_status->devStatus[ canId.bit.src ].errCode = frame->data[1]; // 某个设备停止升级
                            ESP_LOGE(TAG, "devId:0x%x ack(exit) errcode(%d)", canId.bit.src, frame->data[1]);
                            // return -1;
                            break;
                        default:
                            ESP_LOGE(TAG, "devId:0x%x ack(unknown: %d)", canId.bit.src, frame->data[0]);
                            break;
                    }
                }
            }
        break;

        case 8: // Send OTA END
            if (ota_status->resendCnt >= ota_status->retry_max_count) { // 发送次数超限
                ESP_LOGE(TAG, "OTA End send max cnt failed");
                ota_status->resendCnt = 0;
				ota_status->main_step = 10;
                OTA_ErrCode_Set(ota_status, CAN_BROADCAST_ADDRESS, OTA_ERR_NO_RESP);
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

            if (!ota_status->devCnt)
            {
                /*
                2025.02.11 lxy 沟通罗工
                不存在正在升级设备时（设备无响应或主动终止），不发送0x0876终止报文，避免下级设备误认为此次升级由IOT主动终止，产生错误码故障
                */
                ota_status->main_step = 10;
                ota_status->resendCnt = 0;
                ota_status->sender_work = 0;
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
            
            ota_status->resendCnt++;
            ota_status->timecnt = 5000; // 5s
            ota_status->main_step++;
            ESP_LOGI(TAG, "CMD End send %d times", ota_status->resendCnt);
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
                if (frame->data[1]) {
                    ota_status->devStatus[canId.bit.src].isOta = OTA_IDLE;
                    ota_status->devStatus[canId.bit.src].errCode = frame->data[1]; // OTA错误
                    ESP_LOGE(TAG, "devId: 0x%x error code (%d)", canId.bit.src, frame->data[1]);
                } else {
                    /* IOT控制升级进度 */
                    ota_status->pct_78H.type = ota_status->start.fileType;
                    ota_status->pct_78H.pct = (ota_status->block.blockSeq * 100) / ota_status->start.fileSize;
                    (ota_status->pct_78H.pct > 100) ? (ota_status->devStatus[canId.bit.src].pct = 100) : (ota_status->devStatus[canId.bit.src].pct = ota_status->pct_78H.pct);
                    ESP_LOGI(TAG, "END_ACK node: %d, devId: 0x%x pct: %d%%...", node_id, canId.bit.src, ota_status->devStatus[canId.bit.src].pct);
#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE
                    reals.system_ota.ota_total_pct = System_ota_total_pct_update(ota_status->pct_78H.pct, false, reals.system_ota.ota_mcu_curr_count, reals.system_ota.ota_mcu_total_count, ota_status->is_only_stage);
                    ota_status->pct_78H.pct = reals.system_ota.ota_total_pct;
#endif 
                    OTA_SendFrame(node_id, OTA_PCT, (uint8_t *)&ota_status->pct_78H, 8, CAN_BROADCAST_ADDRESS);
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

            if (ota_status->pblock_buf) {
                free(ota_status->pblock_buf);
                ota_status->pblock_buf = NULL;
            }

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
void CanOtaSender_Begin(uint8_t id, uint8_t type, uint32_t version, CanOtaStruct *ota_status)
{
	if ((ota_status->sender_work != 0) || (ota_status->ota_doing != CAN_OTA_IDLE))
	{
		ESP_LOGE(TAG, "device(addr: %02x, type: %d) is being upgraded", id, type);
		return;
	}

	ota_status->sender_work = 1;					// 正在升级标志
	ota_status->sender_type = type;             	// 芯片所属类型
	ota_status->sender_main_ver = version / 100;  	// 设备需要发送的版本号
	ota_status->sender_target_id = id;				// 升级的目标ID
}

#endif
