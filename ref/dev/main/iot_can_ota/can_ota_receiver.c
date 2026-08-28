#include "iot_ota.h"
#include "can_protocol.h"
#include "esp_log.h"
#include "crc.h"
//#include "filesystem.h"
#include "xmodem_client.h"
#include "can_data.h"
#define TAG "[OTA-RECV]"

#if 1
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

    if (ota_status->pfile) {
        fclose(ota_status->pfile);
        ota_status->pfile = NULL;
 //       unlink(ota_status->curr_name);
    }
}
#endif

void CanOtaReceiver(uint8_t node_id, const twai_message_t *frame, CanOtaStruct *ota_status) {
#if 1
    uint8_t resp[8] = {0};
    static uint16_t recv_frame_seq = 0;
    static int32_t wait_can_Id = -1;
    static bool is_esp_ota = false;
    IdStruct canId ;

    /* can ota sender正在运行 */
    if (ota_status->ota_doing == CAN_OTA_DEV) {
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
			ESP_LOGI(TAG, "ota_status->ota_doing is %d",ota_status->ota_doing);
            ESP_LOGI(TAG, "Can OTA Recv Time Out Exit");
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
            ota_status->timecnt = 10000;         /* 保持10s静默(10s超时解除静默) */
            ota_status->ota_doing = 1;          //add by yjh: 开始OTA标记---与IOT OTA标记重合？
            ESP_LOGI(TAG, "Can OTA Ready");
            break;

        case OTA_START:
            wait_can_Id = -1;
            is_esp_ota = false;
            ota_status->timecnt = 10000;         /* 10s超时 */
			ota_status->period_cmd_delay = 10000;         /* 10s超时 */

            ESP_LOGI(TAG, "Can OTA Start");

            memcpy(&ota_status->start, frame->data, sizeof(OtaStart));
            if (ota_status->start.fileType > TypeCnt) { /* CAN-OTA非法文件,不接收本次升级不做应答 */
                ESP_LOGE(TAG, "File type unkonw %d (File rang: 0-%d)", ota_status->start.fileType, TypeCnt);
                break;
            }

            /**
             * 1、检查本次CAN-OTA文件是否存在，如果存在则删除
             * 2、重新创建新的文件
            */

            // 检查并旧的CAN-OTA文件，并删除
//            memcpy(ota_status->curr_name, FileTypeString[ota_status->start.fileType], strlen(FileTypeString[ota_status->start.fileType])+1);
//            find_file(ota_bin_path, ota_status->curr_name, "r", true); // 删除文件
//            snprintf(ota_status->curr_name, sizeof(ota_status->curr_name), "%s/%s.bin", ota_bin_path, FileTypeString[ota_status->start.fileType]);


            if ((ota_status->start.fileType == IOT)&&((ota_status->start.fileVersion/100)==(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver/100))) {
                /* CAN-OTA升级IOT本身，不需要创建文件 */
                iot_ota_abort(); /* 停止上一次的OTA升级 */
                is_esp_ota = true;
                if (iot_ota_begin() != ESP_OK) {
                    ESP_LOGE(TAG, "IOT OTA begin failed");
                    resp[0] = DATA_EXIT;
                    resp[1] = OTA_ERR_FLASH_WRITE;
                }
            } else {
                 /* 除IOT以外的节点升级 */
                // if (ota_status->pfile) {
                //     fclose(ota_status->pfile);
                //     ota_status->pfile = NULL;
                // }

                // // 创建新的文件
                // ota_status->pfile = fopen(ota_status->curr_name, "w+b");
                // if (!ota_status->pfile) {
                //     resp[0] = DATA_EXIT;
                //     resp[1] = OTA_ERR_FLASH_WRITE;
                //     ESP_LOGE(TAG, "Create file %s failed!", ota_status->curr_name);  /* 可读写方式创建 */
                // } else {
                //     ESP_LOGI(TAG, "Create file %s done!", ota_status->curr_name); /* 文件创建完成 */
                //     file_list(ota_bin_path);  // 显示文件列表
                // }
                break;
            }

//            ESP_LOGI(TAG, "CAN-OTA File %s [Ver:%u.%d size: %uKb]", ota_status->curr_name,
//                            ota_status->start.fileVersion/100, ota_status->start.fileVersion%100,
//                            ota_status->start.fileSize);

			ESP_LOGI(TAG, "[Ver:%lu.%ld size: %uKb]", 
							ota_status->start.fileVersion/100, ota_status->start.fileVersion%100,
							ota_status->start.fileSize);


            if (OTA_SendFrame(node_id, OTA_START_ACK , resp, 8, canId.bit.dst) == 1) { // block response
                if (resp[0] == DATA_NORMAL) {
                    wait_can_Id = OTA_BLOCK_START;  // 标记可以接收 OTA数据块
                    ota_status->crc32 = ~0;
                    ota_status->ota_doing = CAN_OTA_IOT; /* 标记IOT将进行OTA */
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
            ESP_LOGI(TAG, "OTA Block Seq(%d)", pblock->blockSeq);

            if (!ota_status->pblock_buf) { // 创建接收数据块缓存区  （一块数据的有效长度：1024字节）
                ota_status->pblock_buf = (uint8_t *)heap_caps_malloc(CAN_PAYLOAD_LEN * pblock->frameTotal, MALLOC_CAP_SPIRAM); //
                if (ota_status->pblock_buf) {
                    ESP_LOGI(TAG, "Block buffer malloc successful (size = %dbytes)", CAN_PAYLOAD_LEN * pblock->frameTotal);
                }
            }

            if (ota_status->pblock_buf) {
                if (ota_status->block.blockSeq != pblock->blockSeq) {  // 数据块编号检查
                    resp[0] = DATA_RESEND;  // 请求重发
                    resp[1] = OTA_ERR_SEQ;  // 序号错误
                    ESP_LOGE(TAG, "Block error (need seq = %d), but request seq:%d", ota_status->block.blockSeq, pblock->blockSeq);
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

                //重新收到数据开始帧且符合条件，重新赋值
                is_esp_ota = true;
                ota_status->ota_doing = CAN_OTA_IOT; /* 标记IOT将进行OTA */
            } else {
                // CAN-OTA数据块不能正常接收,发起异常响应
                if (resp[0] == DATA_EXIT) { // 退出升级
                    can_ota_exit(ota_status);
                    if (is_esp_ota == true) {
                        is_esp_ota = false;
                        iot_ota_abort();
                    }
                }

                if (!OTA_SendFrame(node_id, OTA_BLOCK_DATA_ACK , resp, 8, canId.bit.dst)) {
                    ESP_LOGE(TAG, "Send Block ack failed");
                }
            }
            break;

        case OTA_BLOCK_DATA:

            if (wait_can_Id != OTA_BLOCK_DATA) {
                break;
            }

            ota_status->timecnt = 10000; // 刷新10s超时定时器
            OtaData *blockData = (OtaData *)frame->data;

            if (frame->data_length_code < 2) {
                resp[0] = DATA_RESEND;  // 请求重发
                resp[1] = OTA_ERR_SEQ;  // CAN帧错误
                ESP_LOGE(TAG, "frame too short (%dbytes)", frame->data_length_code);
            } else {

                uint8_t len = frame->data_length_code - 2;
                if (ota_status->pblock_buf && (recv_frame_seq == blockData->frameSeq)) { // 帧序号检查
                    memcpy(ota_status->pblock_buf + ota_status->pblock_len, blockData->bytes, len);
                    ota_status->pblock_len += len;
                    recv_frame_seq++;
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
                            if (iot_ota_write(ota_status->pblock_buf, ota_status->pblock_len) != ESP_OK)
                            {
                                resp[0] = DATA_EXIT;
                                resp[1] = OTA_ERR_FLASH_WRITE;
                                ESP_LOGE(TAG, "File write flash failed");
                            }
                        } else {
                            // fseek(ota_status->pfile, 0L, SEEK_END); /* 偏移带文件末尾 */
                            // if (fwrite(ota_status->pblock_buf, 4, ota_status->pblock_len/4, ota_status->pfile) != ota_status->pblock_len/4) {
                            //     resp[0] = DATA_EXIT;
                            //     resp[1] = OTA_ERR_FLASH_WRITE;
                            //     ESP_LOGE(TAG, "File write flash failed");
                            // }
                        }

                        if (resp[0] == DATA_NORMAL) {
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
                    // if (ota_status->pfile) {
                    //     fclose(ota_status->pfile);
                    //     ota_status->pfile = NULL;
                    // }

                    // /* 对文件进行重命名 */
                    // char new_name[200];
                    // snprintf(new_name, sizeof(new_name), "%s/"FILE_NAME_FORMAT, ota_bin_path, FileTypeString[end->fileType],
                    //                                                         ota_status->start.fileVersion/100,
                    //                                                         ota_status->start.fileVersion%100);
                    // if (rename(ota_status->curr_name, new_name) == 0) {
                    //     ESP_LOGI(TAG, "File %s rename to %s successfull", ota_status->curr_name, new_name);
                    // } else {
                    //     ESP_LOGE(TAG, "File %s rename to %s failed", ota_status->curr_name, new_name);
                    // }
                    // file_list(ota_bin_path); // display all file
                }

            }
            ESP_LOGI(TAG, "OTA END ACK: %d", resp[1]);
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
//                esp_canbus_send_task();
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
            break;
    }
#endif
}



