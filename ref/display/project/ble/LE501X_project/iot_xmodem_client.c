/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: 解析xmodem客户端的数据包
Version: V1.00
Function List: none
***********************************************************/
#include "iot_xmodem_client.h"
#include "iot_xmodem.h"
// #include "iot_iap.h"
#include "iot_crc32.h"
#include "iot_ota.h"
#include "iot_service.h"

static uint8_t gClientStep = 0;
// static uint16_t gTarget = 0xFFFF;
ClientStruct gXmodemClient;
uint8_t pre_wifi_siwtch_State = 0;
extern bool Xmoderm_ota;

/*xmodem初始化*/
void XmodemClientInit(uint16_t target, uint8_t channel) {
    if (gXmodemType == SERVER) {
        gXmodemClient.port = 0xFF;
        return;
    }

    switch (target) {
    case DEVICE_IOT:
        gXmodemClient.fileAddress = BIN_FILE_IOT_ADDRESS; /*地址*/
        // LOG_I("BIN_FILE_IOT_ADDRESS");
        break;
    default:
        return; /*跳出*/
    }
    // LOG_I("CRC_ResetDR");
    CRC_ResetDR(); /*复位CRC*/
    gClientStep = 0;
    gXmodemClient.retry = 0;
    gXmodemClient.finished = 0;
    gXmodemClient.packSeq = 1;
    gXmodemClient.code[0].used = 0;
    gXmodemClient.code[0].crc = 0;
    gXmodemClient.code[0].seq = 1;
    gXmodemClient.code[0].data = buf1;
    gXmodemClient.code[1].used = 0;
    gXmodemClient.code[1].crc = 0;
    gXmodemClient.code[1].seq = 1;
    gXmodemClient.code[1].data = buf2;
    gXmodemClient.port = channel;
    gXmodemType = CLIENT;

    TimerStop(XMODEM_TIMER, gSoftTimer);
    // gTarget = target;
    // EsareCode(target);
}

static uint8_t XmodemClientUnpack(const uint8_t *code, uint16_t len) {
    uint16_t crc16;
    uint16_t targetLen;

    if (!code || len < 133) { // 128 + 5
        return 0;
    }

    if (code[0] == SOH) {
        targetLen = 128;
    } else if (code[0] == STX) {
        targetLen = 1024;
    } else {
        return 0;
    }

    if ((code[1] + code[2]) != 0xFF || code[1] != gXmodemClient.packSeq || (targetLen + 5) != len) {
        // LOG_I("NAK_1");
        // LOG_I("code[1]:%x,code[2]:%x",code[1],code[2]);
        // LOG_I("gXmodemClient.packSeq:%d", gXmodemClient.packSeq);
        // LOG_I("targetLen:%d,len:%d", targetLen,len);
        return NAK;
    }

    crc16 = Xmodem_Crc16Cal(&code[3], targetLen);
    if ((code[len - 2] << 8 | code[len - 1]) != crc16) {
        // LOG_I("NAK_2,crc error");
        return NAK;
    }

    if (!gXmodemClient.code[0].used) {
        gXmodemClient.code[0].used = 1;
        gXmodemClient.code[0].seq = gXmodemClient.packSeq;
        gXmodemClient.code[0].len = targetLen;
        memcpy(gXmodemClient.code[0].data, &code[3], targetLen);
    } else if (!gXmodemClient.code[1].used) {
        gXmodemClient.code[1].used = 1;
        gXmodemClient.code[1].seq = gXmodemClient.packSeq;
        gXmodemClient.code[1].len = targetLen;
        memcpy(gXmodemClient.code[1].data, &code[3], targetLen);
    } else {
        // LOG_I("NAK_3");
        return NAK;
    }

    gXmodemClient.packSeq++;
    return ACK;
}

static uint8_t XmodemClientProcotol(const uint8_t *income, uint16_t inLen) {
    uint8_t respones = 0;
    if (!income || !inLen) {
        return 0;
    }

    switch (income[0]) {
    case SOH:
    case STX:
        for (uint8_t i = 0; i < 2; i++) {
            if (gXmodemClient.code[i].used && !gXmodemClient.code[i].crc) {
                gXmodemClient.code[i].crc
                    = CalcCodeCrc32(0, gXmodemClient.code[i].data, gXmodemClient.code[i].len);
            }
        }
        respones = XmodemClientUnpack(income, inLen);
        break;

    case ETX:
        if (inLen == 1) {
            gXmodemClient.finished = 0;
            respones = ACK;
        }
        break;

    case EOT:
        if (inLen == 1) {
            for (uint8_t i = 0; i < 2; i++) {
                if (gXmodemClient.code[i].used && !gXmodemClient.code[i].crc) {
                    gXmodemClient.code[i].crc
                        = CalcCodeCrc32(1, gXmodemClient.code[i].data, gXmodemClient.code[i].len);
                }
            }

            if (gXmodemClient.code[0].crc == 2 || gXmodemClient.code[1].crc == 2) {
                Xmoderm_ota = false;
                // LOG_I("CRC32 is not right!!!");
                respones = CAN; // crc32 error and cannel protocol
                break;
            }
            gXmodemClient.finished = 1;
            respones = ACK;
        }
        break;
    default:
        break;
    }
    return respones;
}

uint8_t XmodemClientTask(const uint8_t *income, uint16_t inLen, uint8_t channel) {
    uint8_t respones = 0;
    uint8_t i;

    if (gXmodemType != CLIENT || gXmodemClient.port != channel) {
        return 0;
    }
    // LOG_I("gClientStep:%d",gClientStep);
    // LOG_I("gXmodemClient.packSeq:%d",gXmodemClient.packSeq);
    switch (gClientStep) {
    case 0:
        (gXmodemClient.packSeq == 1) ? (respones = 'C') : (respones = 0); // xmodem is nothings data
        // LOG_I("send C!");
        gClientStep++;
        break;
    case 1:
        // LOG_I("case 0");
        if (!income || !inLen) {
            // LOG_I("case 0_1");
            // 3s timeout
            if (TimerExpired(XMODEM_TIMER, gSoftTimer) || TimerStopped(XMODEM_TIMER, gSoftTimer)) {
                // LOG_I("3s timeout");
                gXmodemClient.retry++;
                TimerStart(XMODEM_TIMER, 3000, gSoftTimer);
            }
            break;
        }
    case 2:
        // LOG_I("case 1");
        respones = XmodemClientProcotol(income, inLen);
        if (respones != 0) {
            // LOG_I("case 1_1");
            // LOG_I("inLen:%d",inLen);
            gXmodemClient.retry = 0;
        }
        if (respones == ACK) {
            // LOG_I("case 1_2");
            if (gXmodemClient.code[0].crc == 2 || gXmodemClient.code[1].crc == 2) {
                respones = CAN;
                /*如果OTA对象是ESP32*/
                if (gXmodemClient.fileAddress == 0x98fff || gXmodemClient.fileAddress == 0x98000) {
                    iot_ota_abort();
                }
                break;
            }

            for (i = 0; i < 2; i++) {
                // LOG_I("case 1_3");
                // LOG_I("usd:%d,crc:%d", gXmodemClient.code[i].used, gXmodemClient.code[i].crc);

                //  the code write to flash
                if (gXmodemClient.code[i].used == 1 && gXmodemClient.code[i].crc == 1) {
                    // LOG_I("case 1_3_1");
                    gXmodemClient.code[i].used = 0;
                    gXmodemClient.code[i].crc = 0;
                    /*保存固件到flash中*/

                    /*ESP32 IOT板*/
                    if (gXmodemClient.fileAddress == 0x98fff
                        || gXmodemClient.fileAddress == 0x98000) {
                        if (gXmodemClient.fileAddress == 0x98fff) {
                            iot_ota_write(gXmodemClient.code[i].data, gXmodemClient.code[i].len);
                        } else {
                            LOG_I("OTA START");
                            iot_begin_ota(); /*启动OTA升级*/
                            /*写入第一包*/
                            iot_ota_write(gXmodemClient.code[i].data, gXmodemClient.code[i].len);
                            gXmodemClient.fileAddress += 0xFFF;
                        }
                    }
                }
            }

            if (i == 2 && gXmodemClient.finished) {
                // LOG_I("finish"); /*前半部分接收完成*/
                gXmodemClient.finished = 0;
                gXmodemType = IDLE;
                // LOG_I("resume 2");

                gXmodemFile.precent = 0;
                TimerStart(XMODEM_TIMER, 20, gSoftTimer);

                if (gXmodemClient.fileAddress == 0x98fff) {
                    LOG_I("iot_end_ota");
                    Xmoderm_ota = false;
                    if (iot_end_ota() == OTA_FAIL) {
                        respones = CAN;
                    } /*结束OTA*/
                }
            }
        }
        break;

    default:
        break;
    }

    if (respones == CAN || gXmodemClient.retry >= 10) { /*CAN或者超时（10*3=30秒）*/
        gXmodemClient.retry = 0;
        gXmodemType = IDLE;
        gXmodemFile.precent = -1; // pct error
        Xmoderm_ota = false;
        if (gXmodemClient.fileAddress == 0x98fff
            || gXmodemClient.fileAddress == 0x98000) { /*如果OTA对象是ESP32*/
            iot_ota_abort();
        }
        // LOG_I("resume 3");
        if (respones == CAN) {
            // BinaryFileInfoWrite(gTarget, 0, FILE_EMPTY);  // file is empty
        } else if (gXmodemClient.retry >= 20) {
            // BinaryFileInfoWrite(gTarget, 0, FIlE_TIMOUT); // file wait timeout
        }
        gXmodemFile.binFile.type = -1;
    }
    return respones;
}
