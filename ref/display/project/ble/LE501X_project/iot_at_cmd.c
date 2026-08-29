/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt
Date: 2022/5/25
Description: 解析AT指令（兼容部分百瑞蓝牙模块BR2262e的AT指令）
Version: V1.00
Function List: none
***********************************************************/

#include "iot_at_cmd.h"
#include "iot_uart.h"
#include "iot_ble.h"
#include "iot_bsp.h"

struct pair_feature init_test_feat = {BLE_GAP_IO_CAPS_NONE,
                                      0x0,
                                      AUTH_NONE,
                                      0x10,
                                      (KDIST_ENCKEY | KDIST_IDKEY),
                                      (KDIST_ENCKEY | KDIST_IDKEY)};
extern ble_para_t ble_para;
extern identify_handr_t myident;
extern bool make_ecc_key_startup;

/*解析AT指令*/
int8_t at_cmd_analyze(char *data, uint8_t len) {
    char *p = data;
    int8_t result = -1;
    char respond_buf[60];
    uint16_t respond_len = 0;
    if ((strncmp((char *)"AT+", p, 3) == 0)
        && ((*(p + len - 1) == '\r') || (*(p + len - 1) == '\n'))) { /*判断是否为AT指令*/
        // 开机时未完成ECC密钥生成，AT指令不处理
        if (!make_ecc_key_startup) {
            return result;
        }
        *(p + len - 1) = 0; /*去掉'\r'或者'\n'*/
        if ((*(p + len - 2)) == '\r'
            || (*(p + len - 2)) == '\n') { /*判断倒数第二个是否为'\r'或者'\n'*/
            *(p + len - 2) = 0;
        }
        p += 3;
        if (strncmp((char *)"NAME", p, 4) == 0) { /*蓝牙名称*/
            p += 4;
            if (strncmp((char *)"=", p, 1) == 0) {
                p += 1;
                // LOG_I("AT+NAME=");
                // LOG_I("name len:%d", strlen(p));
                if (strlen(p) <= 29) { /*判断名字长度*/
                    /*比较长度和内容*/
                    if (strlen(p) != strlen((char *)ble_para.device_name)
                        || memcmp(p,
                                  (char *)ble_para.device_name,
                                  strlen((char *)ble_para.device_name))) {
                        modify_scanRespon_name(p); /*修改名字*/
                        save_to_tinyfs();
                        // LOG_I("change name:%s", p);
                    }
                    respond_len = sprintf(respond_buf, "\r\r\nOK\r\r\n");
                } else {
                    // LOG_I("name is too long!");
                }
            } else if (strncmp((char *)"?", p, 1) == 0) {
                respond_len = sprintf(
                    respond_buf, "\r\r\n+NAME:%s\r\r\nOK\r\r\n", (char *)ble_para.device_name);
            }
        } else if (strncmp((char *)"ADV", p, 3) == 0) { /*蓝牙广播*/
            p += 3;
            if (strncmp((char *)"=", p, 1) == 0) {
                p += 1;
                // LOG_I("AT+ADV=");
                if (*p == '1') {
                    adv_switch(1);
                    respond_len = sprintf(respond_buf, "\r\r\nOK\r\r\n");
                } else if (*p == '0') {
                    adv_switch(0);
                    respond_len = sprintf(respond_buf, "\r\r\nOK\r\r\n");
                }
            } else if (strncmp((char *)"?", p, 1) == 0) {
                // LOG_I("AT+ADV?");
                respond_len = sprintf(respond_buf,
                                      "\r\r\n+ADV:%c\r\r\nOK\r\r\n",
                                      ble_para.adv_enable > 0 ? '1' : '0');
            }
        } else if (strncmp((char *)"GVER", p, 4) == 0) { /*软件版本号*/
            // LOG_I("AT+GVER");
            respond_len = sprintf(respond_buf, "\r\r%d\r\r\nOK\r\r\n", IOT_SYSTEM_VER);
        } else if (strncmp((char *)"LBDADDR?", p, 8) == 0) { /*蓝牙地址*/
            LOG_I("AT+LBDADDR?");
            respond_len = sprintf(respond_buf,
                                  "\r\r\n+LBDADDR:%02X%02X%02X%02X%02X%02X\r\r\nOK\r\r\n",
                                  ble_para.ble_mac[5],
                                  ble_para.ble_mac[4],
                                  ble_para.ble_mac[3],
                                  ble_para.ble_mac[2],
                                  ble_para.ble_mac[1],
                                  ble_para.ble_mac[0]);
        } else if (strncmp((char *)"BAUD?", p, 5) == 0) { /*当前通信波特率*/
            // LOG_I("BAUD?");
            respond_len = sprintf(respond_buf, "\r\r\n+BAUD:115200\r\r\nOK\r\r\n");
        } else if (strncmp((char *)"INITBOND", p, 8) == 0) { /*当前通信波特率*/
            init_bond();
            respond_len = sprintf(respond_buf, "\r\r\n+INITBOND_ok\r\r\nOK\r\r\n");
        } else if (strncmp((char *)"FACTORY=1", p, 8) == 0) { /*当前通信波特率*/
            LOG_I("AT+FACTORY=1");
            myident.Fct_test = true;
            respond_len = sprintf(respond_buf, "\r\r\n+OK\r\r\n");
        } else {
            // LOG_I("unknow cmd");
        }
        result = 0;
        if (respond_len) { /*判断是否需要发送*/
            LOG_I("send_here!");
            ServiceSendBytes((uint8_t *)respond_buf, respond_len); /*串口发送*/
        }
    }
    return result;
}

/*指示通知*/
uint8_t at_indicate(char *respond, uint8_t state) {
    uint8_t len = 0;
    switch (state) { /*判断步骤*/
    case IM_READY: /*蓝牙初始化就绪*/
        len = sprintf(respond, "\r\r\nIM_READY\r\r\n");
        break;
    case IM_CONN: /*蓝牙连接*/
        len = sprintf(respond, "\r\r\nIM_CONN:8\r\r\n");
        break;
    case IM_DISC: /*蓝牙断开连接*/
        len = sprintf(respond, "\r\r\nIM_DISC:8\r\r\n");
        break;
    default:
        break;
    }
    return len;
}

void send_at_indicate(uint8_t state) {
    char buf[50];
    uint8_t len = 0;
    len = at_indicate(buf, state);
    if (len) {
        ServiceSendBytes((uint8_t *)buf, len);
    }
}
