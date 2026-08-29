#include "iot_ota.h"
#include "can_protocol.h"
#include "esp_log.h"
#include "crc.h"
#include "filesystem.h"
#include "xmodem_client.h"
#include "comm_define.h"
#include "image_handle.h"

#define TAG "[CAN_OTA_RECV]"


#ifdef CONFIG_CAN_PORT_ENABLE

static uint8_t OTA_SendFrame(uint8_t node_id, uint8_t cmd, uint8_t *data, uint8_t len, uint8_t dst) {
    uint32_t canId = CAN_ID(2, 0, cmd, dst, esp_canbus_myself_address());
    return OTA_SendMessage(node_id, canId, data, len);
}

void can_ota_exit(CanOtaStruct *ota_status) {
    ota_status->fileOK = 0;
    ota_status->ota_doing = CAN_OTA_IDLE;

    if (ota_status->pblock_buf != NULL) { // 释放开辟的block缓存
        free(ota_status->pblock_buf);
        ota_status->pblock_buf = NULL;
    }
}

void CanOtaReceiver(uint8_t node_id, const twai_message_t *frame, CanOtaStruct *ota_status) {
    uint8_t resp[8] = {0};
    static uint16_t recv_frame_seq = 0;
    static int32_t wait_can_Id = -1;
    static bool is_esp_ota = false;//1-IOT本身升级
    IdStruct canId = {0} ;

    /* can ota sender正在运行 */
    if ((ota_status->ota_doing == CAN_OTA_DEV) || ((ota_status->sender_work != 0))) {
        return ;
    }

    /* 接收CAN OTA超时检查 */
    if (ota_status->ota_doing == CAN_OTA_IOT ||
        ota_status->ota_doing == CAN_OTA_SILENT) {

        if (!ota_status->timecnt) {  // 超时
            ota_status->ota_doing = CAN_OTA_IDLE;

            can_ota_exit(ota_status); /* 退出OTA */
            if (is_esp_ota == true) {
                is_esp_ota = false;
                iot_ota_abort();
            }
            ESP_LOGE(TAG, "Can OTA Recv Time Out Exit");
        }
    }

    if (!frame) {
        return ;
    }

    canId.all = frame->identifier;

    switch (canId.bit.funcode)
    {
        case OTA_READY: // 接收到CAN-OTA Ready命令
            wait_can_Id = -1;
            is_esp_ota = false;
            ota_status->period_cmd_delay = 10000;         /* 保持10s静默(10s超时解除静默) */
            ESP_LOGI(TAG, "Can OTA Ready");
            break;

        case OTA_START:
            wait_can_Id = -1;
            is_esp_ota = false;
            ota_status->period_cmd_delay = 10000;         /* 保持10s静默(10s超时解除静默) */
            ESP_LOGI(TAG, "Can OTA Start");

            memcpy(&ota_status->start, frame->data, sizeof(OtaStart));
            if (ota_status->start.fileType > TypeCnt) { /* CAN-OTA非法文件,不接收本次升级不做应答 */
                ESP_LOGE(TAG, "File type unkonw %d (File rang: 0-%d)", ota_status->start.fileType, TypeCnt);
                break;
            }

            ESP_LOGI(TAG, "CAN-OTA File (%d) [Ver:%lu.%lu size: %uKb]", ota_status->start.fileType,
                            (uint32_t)(ota_status->start.fileVersion/100), (uint32_t)(ota_status->start.fileVersion%100),
                            ota_status->start.fileSize);

            if (ota_status->start.fileType == IOT) {
                if ((IOT_VERSION / 100) != (ota_status->start.fileVersion / 100)) {
                    /*非本节点IOT，不做响应，仅保持静默*/
                    ESP_LOGE(TAG, "software_ver error(new:%lu, old:%lu)", ota_status->start.fileVersion, (uint32_t)IOT_VERSION);
                    break;
                }
                
                is_esp_ota = true;
                if (iot_ota_begin() != ESP_OK) {
                    ESP_LOGE(TAG, "IOT OTA begin failed");
                    resp[0] = DATA_EXIT;
                    resp[1] = OTA_ERR_FLASH_WRITE;
                } else {
                    resp[0] = DATA_NORMAL;
                    resp[3] = 1;    // 深度
                    resp[4] = (uint8_t)(ota_status->start.fileVersion & 0xFF);       // 低 8 位
                    resp[5] = (uint8_t)((ota_status->start.fileVersion >> 8) & 0xFF); // 高 8 位
                }
            } else {
                 /* 除IOT以外的节点升级(暂不实现) */
                break;
            }

            if (OTA_SendFrame(node_id, OTA_START_ACK , resp, 8, canId.bit.dst) == 1) { // block response
                if (resp[0] == DATA_NORMAL) {
                    wait_can_Id = OTA_BLOCK_START;  // 标记可以接收 OTA数据块
                    ota_status->crc32 = ~0;
                    ota_status->ota_doing = CAN_OTA_IOT; /* 标记IOT将进行OTA */
                    ota_status->timecnt = 10000;         /* 10s超时 */
                    memset(&ota_status->block, 0, sizeof(OtaBlock));
                }
                 ESP_LOGI(TAG, "Send start ack successfully");
            } else {
                ESP_LOGE(TAG, "Send start ack failed");
            }
            break;

        case OTA_BLOCK_START:

            if (wait_can_Id != OTA_BLOCK_START) { // 检查是否需要接收OTA数据块
                break;
            }
            ota_status->timecnt = 10000;         /* 刷新10s超时 */

            OtaBlock *pblock = (OtaBlock *)frame->data;
            // ESP_LOGI(TAG, "File Block Seq(%d)", pblock->blockSeq);

            if (!ota_status->pblock_buf) { // 创建接收数据块缓存区  （一块数据的有效长度：1024字节）
                ota_status->pblock_buf = (uint8_t *)iot_calloc(CAN_PAYLOAD_LEN * pblock->frameTotal); //
                if (ota_status->pblock_buf) {
                    ESP_LOGI(TAG, "Block buffer malloc successful (size = %dbytes)", CAN_PAYLOAD_LEN * pblock->frameTotal);
                }
            }

            if (ota_status->pblock_buf) {
                if (ota_status->block.blockSeq != pblock->blockSeq) {  // 数据块编号检查
                    resp[0] = DATA_RESEND;  // 请求重发
                    resp[1] = OTA_ERR_SEQ;  // 序号错误
                    ESP_LOGE(TAG, "Block error (need seq = %d)", ota_status->block.blockSeq);
                }
            } else {
                resp[0] = DATA_EXIT;        // 请求退出
                resp[1] = OTA_ERR_MEMONY;   // ram 空间溢出
                ESP_LOGE(TAG, "Block buffer malloc failed (size = %dbytes)", CAN_PAYLOAD_LEN * pblock->frameTotal);
            }

            if (resp[0] == DATA_NORMAL) {
                recv_frame_seq = 0;
                ota_status->pblock_len = 0;
                ota_status->block = *pblock;
                wait_can_Id = OTA_BLOCK_DATA;
            } else if (resp[0] == DATA_EXIT) { // 退出升级
                // CAN-OTA数据块不能正常接收,发起异常响应
                can_ota_exit(ota_status);
                if (is_esp_ota == true) {
                    is_esp_ota = false;
                    iot_ota_abort();
                }
                // 退出升级指令发送
                if (!OTA_SendFrame(node_id, OTA_BLOCK_DATA_ACK , resp, 8, canId.bit.dst)) {
                    ESP_LOGE(TAG, "Send Block ack failed");
                }
            } else if (resp[0] == DATA_RESEND) { // 请求重发
                // 考虑到广播升级不同设备接收差异性，此时不请求重发，等待上级主动重发
            }
            break;

        case OTA_BLOCK_DATA:

            if (wait_can_Id != OTA_BLOCK_DATA) {
                break;
            }

            ota_status->timecnt = 10000; // 刷新10s超时定时器
            OtaData *blockData = (OtaData *)frame->data;

            if (frame->data_length_code < 2) {
                // 异常帧，不处理
                ESP_LOGE(TAG, "frame too short (%dbytes)", frame->data_length_code);
            } else {

                uint8_t len = frame->data_length_code - 2;

                if (ota_status->pblock_buf) {
                    if (recv_frame_seq != blockData->frameSeq) {  // 帧序号检查
                        resp[0] = DATA_RESEND;  // 请求重发
                        resp[1] = OTA_ERR_SEQ;  // 序号错误
                        wait_can_Id = OTA_BLOCK_START;
                        if (!OTA_SendFrame(node_id, OTA_BLOCK_DATA_ACK , resp, 8, canId.bit.dst)) { // block response
                            ESP_LOGE(TAG, "Send Block ack failed");
                        }
                        ESP_LOGE(TAG, "Block error (need seq = %d)", ota_status->block.blockSeq);
                        break;
                    } else {
                        memcpy(ota_status->pblock_buf + ota_status->pblock_len, blockData->bytes, len);
                        ota_status->pblock_len += len;
                        recv_frame_seq++;
                    }
                } 

                if (recv_frame_seq == ota_status->block.frameTotal) { /* 接收到全部帧 */
                    wait_can_Id = OTA_BLOCK_START;

                    uint16_t crc16 = CanBusCrc16(ota_status->pblock_buf, ota_status->pblock_len); // crc16校验
                    if (crc16 != ota_status->block.blockCrc16) {
                        resp[0] = DATA_RESEND;      // 请求重发
                        resp[1] = OTA_ERR_CRC16;    // CAN帧错误
                        ESP_LOGE(TAG, "Block(%d) crc16 error [My:0x%02x Your:0x%02x]",recv_frame_seq, crc16, ota_status->block.blockCrc16);
                    } else {
                        if (is_esp_ota == true) {  /* IOT本身升级，直接写入IOT代码区 */
                            if (iot_ota_write(ota_status->pblock_buf, ota_status->pblock_len) != ESP_OK) {
                                resp[0] = DATA_EXIT;
                                resp[1] = OTA_ERR_FLASH_WRITE;
                                ESP_LOGE(TAG, "File write flash failed");
                            }
                        } else {
                            /* 除IOT以外的节点升级(暂不实现) */
                            break;
                        }

                        if (resp[0] == DATA_NORMAL) {
                            uint8_t pct = (ota_status->block.blockSeq * 100) / ota_status->start.fileSize;
                            resp[1] = 0;
                            resp[2] = (uint8_t)(ota_status->block.blockSeq & 0xFF);       // 低 8 位
                            resp[3] = (uint8_t)((ota_status->block.blockSeq >> 8) & 0xFF); // 高 8 位
                            resp[4] = (pct > 100) ? 100 : pct;
                            resp[5] = 1;
                            ESP_LOGI(TAG, "node: %d, recv block: %u, pct:%d%%....[crc16: 0x%04x]",
                                node_id, ota_status->block.blockSeq, pct, ota_status->block.blockCrc16);
                            ota_status->block.blockSeq++;
                        }
                    }


                    if (resp[0] == DATA_EXIT) {
                        // 退出升级
                        can_ota_exit(ota_status);
                        if (is_esp_ota == true) {
                            is_esp_ota = false;
                            iot_ota_abort();
                        }
                    }

                    if (!OTA_SendFrame(node_id, OTA_BLOCK_DATA_ACK , resp, 8, canId.bit.dst)) { // block response
                        ESP_LOGE(TAG, "Send Block ack failed");
                    }
                }
            }
            break;

        case OTA_END: /* OTA传输结束 */
            ESP_LOGI (TAG, "OTA Cmd(End)");

            wait_can_Id = -1;
            ota_status->timecnt = 5000; // 刷新5s超时定时器

            if (ota_status->ota_doing != CAN_OTA_IOT) {
                break;
            }

            OtaEnd *end = (OtaEnd *)frame->data;
            if (end->fileType != ota_status->start.fileType) { // 本次end是否匹配我们的升级文件
                break;
            }

            if (ota_status->pfile && !ota_file_check(ota_status->pfile)) {
                // 文件CRC32校验错误
                resp[0] = end->fileType;
                resp[1] = OTA_ERR_CRC32;
            } else {
                if (is_esp_ota == true) {
                    if (iot_ota_end() != ESP_OK) {
                        resp[0] = end->fileType;
                        resp[1] = OTA_ERR_FILE;
                    }
                } else {
                    /* 除IOT以外的节点升级(暂不实现) */
                    break;
                }
            }

            if (OTA_SendFrame(node_id, OTA_END_ACK , resp, 8, canId.bit.dst)) { // block response
                ota_status->ota_doing = CAN_OTA_IDLE;
                ESP_LOGI(TAG, "send end ack successfully");
            } else {
                ESP_LOGE(TAG, "send end ack failed");
            }

            ota_status->timecnt = 0;
            can_ota_exit(ota_status);
            if (is_esp_ota == true) {
                is_esp_ota = false;
                ESP_LOGI(TAG, "Delay 1s Prepare to restart system!");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
            break;
            
        default:
            break;
    }
}

#endif

