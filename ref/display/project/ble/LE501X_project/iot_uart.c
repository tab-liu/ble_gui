/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName: iot_uart.c
Author: weiyt
Date: 2022/5/23
Description: 串口驱动、串口数据收发
Version: V1.00
Function List: none
***********************************************************/
#define LOG_TAG "IOT_UART"

#include "iot_uart.h"
#include "iot_at_cmd.h"
#include "iot_ble.h"
#include "iot_aes.h"
#include "iot_crc16.h"
#include "iot_xmodem_client.h"
#include "iot_modbus_slave.h"
#include "iot_xmodem.h"
#include "iot_bsp.h"
#include "ls_sys.h"
#include "iot_timer.h"

#define UART_TX_TIMEOUT 5 // timer units: ms

static uint8_t uart_rx_byte;
uint16_t Modbus_version;
uint16_t Index_addr;
uint8_t Modbus_value;
uint8_t FLAG_RECV = 0;
extern uint8_t pwd[6]; // 蓝牙直连密码
bool Check_Mod_ver;
bool Check_IOT_ver;
bool Xmoderm_start;
bool Xmoderm_ota;
// static iot_timer_id_t uart_timer_id = 0xFF;

// static void uart_timer_cb(void *param);

uart_date_t uart_date;

UART_HandleTypeDef UART_Config;
// uint8_t test_zone_a[RX_BUF_SIZE];

extern ble_save_para_t ble_save_para;

#define FCTY_TYPE_ADDR 29701 // 标定协议-设备类型地址
#define FCTY_SN_ADDR 29713 // 标定协议-设备SN地址
#define FCTY_CODE_ADDR 29717 // 标定协议-设备安全码地址

#define FCTY_HIGH_IN_FRONT_ENABLE // 标定协议-高位在前使能

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->UARTX == UART2) {
        // LOG_I("uart tx complate");
        uart_date.tx_size = 0;
        // if (uart_date.tx_state == TX_DOING) {
        //     uart_date.tx_state = TX_IDLE;
        // }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->UARTX == UART2) {
        /*接收一个字节会进入此回调函数*/
        // iot_timer_stop_task(uart_timer_id); /*开始定时器*/
        if (uart_date.rx_size < RX_BUF_SIZE) {
            uart_date.rx_buf[uart_date.rx_size++] = uart_rx_byte; /*获取内容*/
            uart_date.rx_state = RX_DOING;
        } else {
            LOG_I("rx buffer overflow!");
        }
        HAL_UART_Receive_IT(&UART_Config, &uart_rx_byte, 1); /*使能串口中断接收*/
        FLAG_RECV = 1;
        // LOG_I("HAL_UART_RxCpltCallback!!!!");
        // iot_timer_start_task(uart_timer_id); /*开始定时器*/
    }
}

#define MB_FUNCODE_ERROR 0x80 // modbus错误功能码
#define MB_ERROR_NOT_SUPPORTED 0X01 // modbus错误类型

#define UTIL_SWAP_16(N) ((((uint16_t)(N) & 0xff00) >> 8) | (((uint16_t)(N) & 0x00ff) << 8))

void change_to_modbus_data(uint8_t *buff_out, uint8_t *buff_in, uint16_t *out_len, uint16_t len) {
    if (len & 1) { // 如果是奇数个，直接返回，不处理，out_len为0
        *out_len = 0;
        return;
    }
    for (uint16_t i = 0; i < len; i += 2) {
        buff_out[i] = buff_in[i + 1];
        buff_out[i + 1] = buff_in[i];
    }
    *out_len = len;
}

/**
 * @brief 判断是否已进行过标定
 *
 * @return true 已标定
 * @return false 未标定
 */
bool is_fcty_calibrated(void) {
    return ble_save_para.iot_sn != IOT_SN_DEFAULT;
}

/**
 * @brief 标定错误响应
 *
 * @param buff_in 输入缓存
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
static void fcty_rsp_error(uint8_t *buff_in, uint8_t *buff_out, uint16_t *out_len) {
    uint8_t idx = 0;
    buff_out[idx++] = buff_in[0];
    buff_out[idx++] = buff_in[1] | MB_FUNCODE_ERROR;
    buff_out[idx++] = MB_ERROR_NOT_SUPPORTED;
    uint16_t crc = Modbus_Crc16Cal(buff_out, idx);
    buff_out[idx++] = crc;
    buff_out[idx++] = crc >> 8;
    *out_len = idx;
}

/**
  * @brief      数组反转
  * @param[in]  uint8_t* array
                int length
  * @param[out] None
  * @return     static void
  */
static void reverseArray(uint8_t *array, uint8_t reg_write_nums) {
    int start = 0;
    int end = reg_write_nums * 2 - 1;
    while (start < end) {
        uint8_t temp = array[start];
        array[start] = array[end];
        array[end] = temp;
        start++;
        end--;
    }
}

/**
 * @brief 标定读数据处理
 *
 * @param buff_in 输入缓存
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
static void fcty_read_handler(uint8_t *buff_in, uint8_t *buff_out, uint16_t *out_len) {
    uint16_t reg_addr = ((uint16_t)buff_in[2] << 8) | buff_in[3];
    uint16_t reg_nums = ((uint16_t)buff_in[4] << 8) | buff_in[5];
    uint16_t reg_write_nums = 0;
    /* 组装modbus读响应数据 */
    uint8_t idx = 0;
    buff_out[idx++] = buff_in[0]; // 地址
    buff_out[idx++] = buff_in[1]; // 功能码
    buff_out[idx++] = reg_nums * 2; // 字节数

    for (uint8_t i = 0; i < reg_nums;) {
        reg_addr += reg_write_nums;
        uint16_t *pdata = (uint16_t *)&buff_out[idx];
        // 读设备类型
        if (reg_addr == FCTY_TYPE_ADDR) {
            buff_out[idx++] = ble_save_para.iot_type[1];
            buff_out[idx++] = ble_save_para.iot_type[0];
            buff_out[idx++] = ble_save_para.iot_type[3];
            buff_out[idx++] = ble_save_para.iot_type[2];
            buff_out[idx++] = ble_save_para.iot_type[5];
            buff_out[idx++] = ble_save_para.iot_type[4];
            buff_out[idx++] = ble_save_para.iot_type[7];
            buff_out[idx++] = ble_save_para.iot_type[6];
            buff_out[idx++] = ble_save_para.iot_type[9];
            buff_out[idx++] = ble_save_para.iot_type[8];
            buff_out[idx++] = ble_save_para.iot_type[11];
            buff_out[idx++] = ble_save_para.iot_type[10];
            i += 6;
        } else if (reg_addr == FCTY_SN_ADDR) { // 读设备SN
            /* 整形数据的高低字节序由标定文件定义 */
            memcpy(&buff_out[idx], &ble_save_para.iot_sn, sizeof(ble_save_para.iot_sn));
            reg_write_nums = (sizeof(ble_save_para.iot_sn) / 2);
            i += reg_write_nums;
#ifdef FCTY_HIGH_IN_FRONT_ENABLE
            reverseArray((uint8_t *)pdata, reg_write_nums);
#endif
            idx += sizeof(ble_save_para.iot_sn);
        } else if (reg_addr == FCTY_CODE_ADDR) { // 读设备安全码
            /* 整形数据的高低字节序由标定文件定义 */
            memcpy(&buff_out[idx], &ble_save_para.safe_code, sizeof(ble_save_para.safe_code));
            reg_write_nums = (sizeof(ble_save_para.safe_code) / 2);
            i += reg_write_nums;
#ifdef FCTY_HIGH_IN_FRONT_ENABLE
            reverseArray((uint8_t *)pdata, reg_write_nums);
#endif
            idx += sizeof(ble_save_para.safe_code);
        } else {
            fcty_rsp_error(buff_in, buff_out, out_len);
            return;
        }
    }

    uint16_t crc = Modbus_Crc16Cal(buff_out, idx);
    buff_out[idx++] = crc;
    buff_out[idx++] = crc >> 8;
    *out_len = idx;
}

static bool sn_write_flag = false;
static bool safecode_write_flag = false;

/**
 * @brief 标定写数据处理
 *
 * @param buff_in 输入缓存
 * @param in_len 输入长度
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
static void fcty_write_handler(uint8_t *buff_in,
                               uint16_t in_len,
                               uint8_t *buff_out,
                               uint16_t *out_len) {
    uint16_t reg_nums;
    uint16_t *pdata;
    uint16_t reg_addr = ((uint16_t)buff_in[2] << 8) | buff_in[3];
    uint16_t reg_write_nums = 0;
    // uint32_t k;

    /* 判断是否是写单个寄存器还是多个寄存器 */
    if (buff_in[1] == 0x06) {
        reg_nums = 1;
        pdata = (uint16_t *)&buff_in[4];
    } else {
        reg_nums = ((uint16_t)buff_in[4] << 8) | buff_in[5];
        pdata = (uint16_t *)&buff_in[7];
    }

    for (uint8_t i = 0; i < reg_nums; i++) {
        reg_addr += reg_write_nums;
        if (reg_addr == FCTY_SN_ADDR) { // 标定设备SN
            /* 整形数据的高低字节序由标定文件定义（此处为HSB格式） */
            reg_write_nums = (sizeof(ble_save_para.iot_sn) / 2);
            i += reg_write_nums;

#ifdef FCTY_HIGH_IN_FRONT_ENABLE
            reverseArray((uint8_t *)pdata, reg_write_nums);
#endif
            memcpy(&ble_save_para.iot_sn, pdata, sizeof(ble_save_para.iot_sn));

            // ble name: type + sn
            memcpy(ble_save_para.ble_name, IOT_TYPE_DEFAULT, strlen(IOT_TYPE_DEFAULT));
            // 拼接sn
            char sn_str[24] = {0};
            sprintf(sn_str, "%llu", ble_save_para.iot_sn);
            memcpy(ble_save_para.ble_name + strlen(IOT_TYPE_DEFAULT), sn_str, strlen(sn_str));

            pdata += sizeof(ble_save_para.iot_sn);
            sn_write_flag = true;
        } else if (reg_addr == FCTY_CODE_ADDR) { // 标定设备安全码
            /* 整形数据的高低字节序由标定文件定义（此处为HSB格式） */
            reg_write_nums = (sizeof(ble_save_para.safe_code) / 2);
            i += reg_write_nums;

#ifdef FCTY_HIGH_IN_FRONT_ENABLE
            reverseArray((uint8_t *)pdata, reg_write_nums);
#endif

            memcpy(&ble_save_para.safe_code, pdata, sizeof(ble_save_para.safe_code));
            pdata += sizeof(ble_save_para.safe_code);
            safecode_write_flag = true;
        } else {
            fcty_rsp_error(buff_in, buff_out, out_len);
            return;
        }
    }
    if (buff_in[1] == 0x06) { /* 响应写单个寄存器 */
        memcpy(buff_out, buff_in, in_len);
        *out_len = in_len;
    } else { /* 响应写多个寄存器 */
        memcpy(buff_out, buff_in, 6);
        *(uint16_t *)&buff_out[6] = Modbus_Crc16Cal(buff_out, 6);
        *out_len = 8;
    }
}

/*串口参数初始化*/
static void uart_init(void) {
    UART_Config.UARTX = UART2;
    UART_Config.Init.BaudRate = UART_BAUDRATE_115200;
    UART_Config.Init.MSBEN = 0;
    UART_Config.Init.Parity = UART_NOPARITY;
    UART_Config.Init.StopBits = UART_STOPBITS1;
    UART_Config.Init.WordLength = UART_BYTESIZE8;
    UART_Config.Init.HwFlowCtl = 0;
    HAL_UART_Init(&UART_Config);
}

static void delay_and_platform_reset(void *param) {
    platform_reset(0); // 重启, 自动验证
}

static uint8_t xmodem_start_delay_cnt = 0;
/*定时器回调函数,底层->IOT*/
void uart_timer_cb(void *param) {
    //	LOG_I("uart_timer_cb!!!!,1");
    if (Xmoderm_start == true) {
        if (xmodem_start_delay_cnt++ > 100) {
            Xmoderm_start = false;
            xmodem_start_delay_cnt = 0;

            uint8_t ACK_xmoderm;
            /*接收手机发送的固件包，校验保存到内部flash*/
            ACK_xmoderm = XmodemClientTask(uart_date.tx_buf, 14, BLE_CHANNEL);
            /*升级请求-43*/
            AES_BleSendBytes(&ACK_xmoderm, 1, true);
            // LOG_I("ble first ack:%02x", ACK_xmoderm);
            Xmoderm_ota = true;
        }
    }

    uart_date_t *pctx = &uart_date;
    if (!pctx->rx_size) {
        return;
    }
    // iot_timer_stop_task(uart_timer_id); /*停止定时器*/
    if (pctx->rx_state == RX_DOING) {
        pctx->rx_state = RX_CPMPLATE; /*标记接收完成*/

        // ------------- 解析接收数据 -------------
        uint8_t rsp_buff[256];
        uint16_t rsp_len;

        // LOG_I("uart recv data size: %d", pctx->rx_size);
        // LOG_HEX(pctx->rx_buf, pctx->rx_size);

        if (pctx->rx_buf[1] == 0x03) { /* 标定读数据处理 */
            fcty_read_handler(pctx->rx_buf, rsp_buff, &rsp_len);
        } else { /* 标定写数据处理 */
            fcty_write_handler(pctx->rx_buf, pctx->rx_size, rsp_buff, &rsp_len);
            update_to_tinyfs();
        }

        HAL_UART_Transmit_IT(&UART_Config, rsp_buff, rsp_len);

        // LOG_D("uart send data size: %d", rsp_len);
        // LOG_HEX(rsp_buff, rsp_len);

        memset(pctx->rx_buf, 0, RX_BUF_SIZE); /*清空接收缓存*/
        pctx->rx_state = RX_IDLE;
        pctx->rx_size = 0;

        if (sn_write_flag && safecode_write_flag) {
            LOG_I("write ok, going to reset...");
            iot_uart_deinit(); // 断开串口
            /* 串口关闭延迟500ms后重启, 否则上位机可能检测不到设备重启 */
            iot_timer_id_t reset_timer_id
                = iot_timer_add_task_ms(500, false, delay_and_platform_reset, 0);
            iot_timer_start_task(reset_timer_id);
        }
    }
    // LOG_I("uart_timer_cb,len");
}

/*iot串口初始化*/
void iot_uart_init(void) {
    uart_date_t *pctx = &uart_date;
    pctx->rx_offset = 0;
    pctx->rx_size = 0;
    pctx->rx_state = RX_IDLE;
    pctx->tx_size = 0;
    pctx->tx_offset = 0;
    // pctx->tx_state = TX_IDLE;

    pinmux_uart2_init(PB00, PB01); /*使用UART2，TX为PB08，RX为PB09*/
    io_pull_write(PB01, IO_PULL_UP); /*设置PB09为上拉输入模式*/
    uart_init(); /*串口初始*/
    HAL_UART_Receive_IT(&UART_Config, &uart_rx_byte, 1); /*使能串口中断接收*/
    // uart_timer_id = iot_timer_add_task_ms(UART_TX_TIMEOUT, false, uart_timer_cb, NULL);
}

/*串口接收数据*/
uint8_t *ServiceRecvBytes(uint16_t *msgLen) {
    uart_date_t *pctx = &uart_date;
    if (pctx->rx_state == RX_CPMPLATE && pctx->rx_size != 0) {
        pctx->rx_state = RX_IDLE;
        *msgLen = pctx->rx_size;
        // LOG_I("Recv:%d",pctx->rx_size);
        pctx->rx_size = 0;
        return pctx->rx_buf;
    } else {
        *msgLen = 0;
        return NULL;
    }
}

/*上电检查通信版本*/
void Check_Com_Version(void) {
    Check_Mod_ver = true;
    // 模拟APP查询通讯版本的MODBUS指令
    uint8_t check_cmd[8] = {0x01, 0x03, 0x00, 0x10, 0x00, 0x01, 0x85, 0xCF};
    HAL_UART_Transmit_IT(&UART_Config, check_cmd, sizeof(check_cmd)); /*串口中断发送*/
}

/*数据发送到串口*/
uint8_t ServiceSendBytes_Uart_buf(uint8_t *data, uint16_t len) {
    if (len != 0 && data != NULL) {
        memset(uart_date.tx_buf, 0, BLE_TX_SIZE);
        memcpy(uart_date.tx_buf, data, len);
        uart_date.tx_size = len;

        // 取明文数据长度
        uint16_t text_len = ((uint16_t)uart_date.tx_buf[0] << 8) + uart_date.tx_buf[1];
        Aes_Cbc_decry(uart_date.tx_buf + 2, text_len); // AES_cbc解密
        // uart_date.tx_state = TX_DOING; /*标记发送状态*/
        // LOG_I("Recv text len is %d:\n", text_len);
        // for (uint8_t i = 0; i < text_len; i++) {
        //     printf("%x,", uart_date.tx_buf[i + 6]);
        // }
        // 测试OTA升级功能，匹配EB3A2429004806666
        // debug_test(uart_date.tx_buf);
        // uint8_t ack_buf[500] = {0};

        if (uart_date.tx_buf[7] == 0x03) { // MODBUS查询指令
            uint16_t Recover_addr
                = (uint16_t)(uart_date.tx_buf[8] << 8) + uart_date.tx_buf[9]; /*询问的首地址*/
            uint16_t Num_addr
                = (uint16_t)(uart_date.tx_buf[10] << 8) + uart_date.tx_buf[11]; /*询问的地址个数*/

            /*询问指令包含IOT版本(EB3A)(地址：30)*/
            if ((Recover_addr < 0x1E) && ((Recover_addr + Num_addr) >= 0x1E)) {
                Index_addr = Recover_addr;
                Check_IOT_ver = true;
            }
            /*询问IOT版本modbus指令(PR008)(地址：11014)*/
            else if ((Recover_addr < 0x2b06) && ((Recover_addr + Num_addr) >= 0x2b06)) {
                FillNew_IotVer(Recover_addr, Num_addr);
                return 1;
            } else if (Recover_addr == 0x01) { /*询问密码*/
                Pass_Query(Num_addr, uart_date.tx_buf[6]);
                return 1;
            }
        } else if (uart_date.tx_buf[7] == 0x06) {
            uint16_t Recover_addr
                = (uint16_t)(uart_date.tx_buf[8] << 8) + uart_date.tx_buf[9]; /*设置的首地址*/
            // LOG_D("0x06 --> Recover_addr: %d", Recover_addr);
            if (Recover_addr == 12162) { // 0x2f82 配置连接模式 1: 指定设备 2: 自动连接
                // LOG_D("0x06 --> Set_Screen_pairing_mode, %d", uart_date.tx_buf[10]);
                Set_Screen_pairing_mode(uart_date.tx_buf[10]);
                return 1;
            }
        } else if (uart_date.tx_buf[7] == 0x10) { // MODBU设置指令 设置配置
            uint16_t Recover_addr
                = (uint16_t)(uart_date.tx_buf[8] << 8) + uart_date.tx_buf[9]; /*设置的首地址*/
            // LOG_D("0x10 --> Recover_addr: %d", Recover_addr);

            // EB3A-APP升级指令：目标指令-1080,升级设备ARM-01/IOT-00
            if ((Recover_addr == 0x0438) && (uart_date.tx_buf[14] == 00)) {
                OTA_start();
                return 1;
            }
            // PR008-APP升级指令：目标指令-700
            else if ((Recover_addr == 0x02bc) && (uart_date.tx_buf[16] == 00)) {
                OTA_start();
                return 1;
            } else if (Recover_addr == 0x07) { // 设置密码
                Pass_Setting();
                return 1;
            } else if (Recover_addr == 12162) { // 0x2f82 配置连接模式 1: 指定设备 2: 自动连接
                // LOG_D("0x10 --> Set_Screen_pairing_mode");
                Set_Screen_pairing_mode(uart_date.tx_buf[13]);
                return 1;
            } else if (Recover_addr == 21000) { // 0x5208
                Screen_param_rw();
                return 1;
            }
        }
        if (Xmoderm_ota == true) { // 升级
            /*接收手机发送的固件包，校验保存到内部flash*/
            uint8_t ACK_xmoderm = XmodemClientTask(uart_date.tx_buf + 6, text_len, BLE_CHANNEL);
            LOG_I("ble ack:%02x", ACK_xmoderm);
            if (ACK_xmoderm != 0) {
                /*升级应答-06*/
                AES_BleSendBytes(&ACK_xmoderm, 1, true);
            }
            return 1;
        }
        // HAL_UART_Transmit_IT(&UART_Config, &uart_date.tx_buf[6], text_len); /*串口中断发送*/

        // ble 透传数据
        // if (is_client_encryption_required()) { // 加密数据透传
        //     ble_client_send_data_aes(uart_date.tx_buf + 6, text_len, CLIENT_DATA_KEY);
        // } else { // 明文数据透传
        //     ble_client_send_data(uart_date.tx_buf + 6, text_len); // ble 透传数据
        // }
    }
    return 1;
}

uint8_t ServiceSendBytes(uint8_t *data, uint16_t len) {
    // if(uart_date.tx_state == TX_IDLE && len != 0 && data != NULL)
    if (len != 0 && data != NULL) {
        memset(uart_date.tx_buf, 0, BLE_TX_SIZE);
        memcpy(uart_date.tx_buf, data, len);
        uart_date.tx_size = len;
        // uart_date.tx_state = TX_DOING; /*标记发送状态*/
        HAL_UART_Transmit_IT(&UART_Config, uart_date.tx_buf, len); /*串口中断发送*/
    }
    return 1;
}

/*串口反初始化*/
void iot_uart_deinit(void) {
    // iot_timer_stop_task(uart_timer_id); /*停止定时器*/
    // iot_timer_remove_task(uart_timer_id); /*删除定时器*/
    // uart_timer_id = 0xFF;
    HAL_UART_DeInit(&UART_Config);
    pinmux_uart2_deinit();
}

/*获取通信协议版本*/
void Get_ModbusVer(const uint8_t *cmd_buf) {
    if (Check_Mod_ver == true) { // 获取版本，只获取一次
        Check_Mod_ver = false;
        uint16_t temp = cmd_buf[3];
        temp = (temp << 8) + cmd_buf[4];
        Modbus_version = temp;
        if (temp < 2000) {
            Modbus_value = v_1022;
        } else {
            Modbus_value = v_2002;
        }
    }
}

/*填充旧协议IOT版本1019*/
void FillOld_IotVer(uint8_t *cmd_buf, uint16_t cmd_len) {
    /*通信版本是1016~1022，并且发给手机的这条指令是版本上报指令*/
    if ((Modbus_value == v_1022) && (Check_IOT_ver == true)) {
        Check_IOT_ver = false;
        uint16_t Res_crc;
        Index_addr = (0x1D - Index_addr) * 2 + 3;
        cmd_buf[Index_addr] = (uint16_t)IOT_SYSTEM_VER >> 8; /*填充IOT版本*/
        cmd_buf[Index_addr + 1] = (uint16_t)IOT_SYSTEM_VER & 0xFF;
        cmd_buf[Index_addr + 2] = ((uint16_t)(IOT_SYSTEM_VER >> 16)) >> 8;
        cmd_buf[Index_addr + 3] = ((uint16_t)(IOT_SYSTEM_VER >> 16)) & 0xFF;
        Res_crc = Modbus_Crc16Cal(cmd_buf, cmd_len - 2); // 本身的CRC位不校验
        cmd_buf[cmd_len - 2] = (uint8_t)(Res_crc & 0xFF);
        cmd_buf[cmd_len - 1] = (uint8_t)(Res_crc >> 8);
    }
}

/*填充新协议IOT版本2002*/
void FillNew_IotVer(uint16_t Rec_add, uint16_t Num_add) {
    uint8_t *Iot_version = NULL;
    uint8_t data_len = Num_add * 2 + 5;
    uint16_t Res_crc;
    Iot_version = malloc(data_len * sizeof(uint8_t));
    if (Iot_version == NULL) {
        LOG_I("Iot_version malloc failed!");
        return;
    }
    memset(Iot_version, 0, data_len);
    memcpy(Iot_version, &uart_date.tx_buf[6], 2);
    Iot_version[2] = Num_add * 2;
    Index_addr = (0x2B06 - Rec_add) * 2 + 3;
    Iot_version[Index_addr] = (uint16_t)IOT_SYSTEM_VER >> 8; /*填充IOT版本*/
    Iot_version[Index_addr + 1] = (uint16_t)IOT_SYSTEM_VER & 0xFF;
    Iot_version[Index_addr + 2] = ((uint16_t)(IOT_SYSTEM_VER >> 16)) >> 8;
    Iot_version[Index_addr + 3] = ((uint16_t)(IOT_SYSTEM_VER >> 16)) & 0xFF;

    Res_crc = Modbus_Crc16Cal(Iot_version, data_len - 2); // 本身的CRC位不校验
    Iot_version[data_len - 2] = (uint8_t)(Res_crc & 0xFF);
    Iot_version[data_len - 1] = (uint8_t)(Res_crc >> 8);
    AES_BleSendBytes(Iot_version, data_len, true); // 回复IOT版本信息
    free(Iot_version);
}

/*启动OTA*/
void OTA_start(void) {
    uint8_t Modbus_ack[8];
    uint16_t Res_crc;
    memcpy(Modbus_ack, &uart_date.tx_buf[6], 6);
    Res_crc = Modbus_Crc16Cal(Modbus_ack, 6); // 本身的CRC位不校验
    Modbus_ack[6] = (uint8_t)(Res_crc & 0xFF);
    Modbus_ack[7] = (uint8_t)(Res_crc >> 8);
    AES_BleSendBytes(&Modbus_ack[0], sizeof(Modbus_ack), true);
    // uart_date.tx_state = TX_IDLE;
    gModbusUpdateRegs.UpdateDataStruct.DeviceType = 0;
    XmodemClientInit(gModbusUpdateRegs.UpdateDataStruct.DeviceType, BLE_CHANNEL);
    Xmoderm_start = true;
    // iot_timer_set_period(uart_timer_id, MS_TO_TICKS(3000)); /*设置定时器周期*/
    // iot_timer_start_task(uart_timer_id); /*开始定时器*/
}

/*APP端密码设置*/
void Pass_Setting(void) {
    uint8_t Modbus_ack[8];
    uint16_t Res_crc;
    for (uint8_t j = 0; j < 6; j += 2) {
        pwd[4 - j] = uart_date.tx_buf[13 + j];
        pwd[5 - j] = uart_date.tx_buf[14 + j];
    }
    // LOG_I("set password is:\n");
    // for (uint8_t i = 0; i < sizeof(pwd); i++) {
    //     LOG_I("%d", pwd[i]);
    // }
    savePwd_to_tinyfs();
    memcpy(Modbus_ack, &uart_date.tx_buf[6], 6);
    Res_crc = Modbus_Crc16Cal(Modbus_ack, 6); // 本身的CRC位不校验
    Modbus_ack[6] = (uint8_t)(Res_crc & 0xFF);
    Modbus_ack[7] = (uint8_t)(Res_crc >> 8);
    AES_BleSendBytes(&Modbus_ack[0], sizeof(Modbus_ack), true);
}

/*ARM端密码重置*/
void Pass_Reseting(void) {
    uint8_t Modbus_ack[8];
    uint16_t Res_crc;
    for (uint8_t j = 0; j < 6; j += 2) {
        pwd[4 - j] = uart_date.rx_buf[7 + j];
        pwd[5 - j] = uart_date.rx_buf[8 + j];
    }
    // LOG_I("set password is:\n");
    // for (uint8_t i = 0; i < sizeof(pwd); i++) {
    //     LOG_I("%d", pwd[i]);
    // }
    savePwd_to_tinyfs();
    memcpy(Modbus_ack, uart_date.rx_buf, 6);
    Res_crc = Modbus_Crc16Cal(Modbus_ack, 6); // 本身的CRC位不校验
    Modbus_ack[6] = (uint8_t)(Res_crc & 0xFF);
    Modbus_ack[7] = (uint8_t)(Res_crc >> 8);
    HAL_UART_Transmit_IT(&UART_Config, Modbus_ack, sizeof(Modbus_ack)); /*串口中断发送*/
}

void Set_Screen_pairing_mode(uint8_t mode) {
    uint8_t Modbus_ack[8];
    uint16_t Res_crc;

    Set_Magnetic_Screen_Para_pairing_mode(mode);

    memcpy(Modbus_ack, &uart_date.tx_buf[6], 6);
    Res_crc = Modbus_Crc16Cal(Modbus_ack, 6); // 本身的CRC位不校验
    Modbus_ack[6] = (uint8_t)(Res_crc & 0xFF);
    Modbus_ack[7] = (uint8_t)(Res_crc >> 8);
    AES_BleSendBytes(&Modbus_ack[0], sizeof(Modbus_ack), true);
}

// 回复21000，5的读取数据，回复为30002
void Screen_param_read(void) {
    uint16_t out_len = 0;
    uint16_t Res_crc;
    uint8_t Modbus_ack[55] = {0}; // 一组共21个寄存器 + 两个头寄存器
    Modbus_ack[1] = 0x10;
    Modbus_ack[2] = 21000 >> 8;
    Modbus_ack[3] = 21000 & 0xFF;
    Modbus_ack[4] = 0;
    Modbus_ack[5] = 23; // 一组共 2 + 21个寄存器
    Modbus_ack[6] = 46;
    Modbus_ack[7] = 30002 >> 8;
    Modbus_ack[8] = 30002 & 0xFF;
    Modbus_ack[9] = 0x00; // shift为0
    Modbus_ack[10] = 0x00;
    change_to_modbus_data(
        Modbus_ack + 11, (uint8_t *)magnetic_screen_para.target_name, &out_len, 12);
    change_to_modbus_data(Modbus_ack + 23, (uint8_t *)&magnetic_screen_para.target_sn, &out_len, 8);
    change_to_modbus_data(Modbus_ack + 31, magnetic_screen_para.target_key, &out_len, 16);
    change_to_modbus_data(Modbus_ack + 47, magnetic_screen_para.target_mac, &out_len, 6);
    // memcpy(Modbus_ack + 11, magnetic_screen_para.target_name, 12);
    // memcpy(Modbus_ack + 23, &magnetic_screen_para.target_sn, 8);
    // memcpy(Modbus_ack + 31, magnetic_screen_para.target_key, 16);
    // memcpy(Modbus_ack + 47, magnetic_screen_para.target_mac, 6);
    Res_crc = Modbus_Crc16Cal(Modbus_ack, 53); // 本身的CRC位不校验
    Modbus_ack[53] = (uint8_t)(Res_crc & 0xFF);
    Modbus_ack[54] = (uint8_t)(Res_crc >> 8);
    AES_BleSendBytes(&Modbus_ack[0], sizeof(Modbus_ack), true);
    LOG_D("Screen_param_read");
    LOG_D("read target_name: %s", magnetic_screen_para.target_name);
    LOG_D("read target_sn: %lu", magnetic_screen_para.target_sn);
    LOG_D("read target_key:");
    LOG_HEX(magnetic_screen_para.target_key, 16);
    LOG_D("read target_mac:");
    LOG_HEX(magnetic_screen_para.target_mac, 6);
}

void Screen_param_write(void) {
    uint8_t Modbus_ack[8] = {0};
    uint16_t Res_crc;
    memcpy(Modbus_ack, &uart_date.tx_buf[6], 6);

    uint16_t reg_num = (uart_date.tx_buf[10] << 8) | uart_date.tx_buf[11];
    if (reg_num < 23) { // 不足一组，不设置,
        Modbus_ack[1] |= 0x80;
        Modbus_ack[2] = 0x03; // 数据错误
        Res_crc = Modbus_Crc16Cal(Modbus_ack, 3); // 本身的CRC位不校验
        Modbus_ack[3] = (uint8_t)(Res_crc & 0xFF);
        Modbus_ack[4] = (uint8_t)(Res_crc >> 8);
        AES_BleSendBytes(&Modbus_ack[0], 5, true);
        return;
    }
    // 只处理第一组 - 15,16表示设备数量 - 数据从17开始
    Set_Magnetic_Screen_Para_target(uart_date.tx_buf + 17);
    Res_crc = Modbus_Crc16Cal(Modbus_ack, 6); // 本身的CRC位不校验
    Modbus_ack[6] = (uint8_t)(Res_crc & 0xFF);
    Modbus_ack[7] = (uint8_t)(Res_crc >> 8);
    AES_BleSendBytes(&Modbus_ack[0], sizeof(Modbus_ack), true);
}

void Screen_param_rw(void) {
    uint16_t reg_num = (uart_date.tx_buf[10] << 8) | uart_date.tx_buf[11];
    uint8_t rsp_len = uart_date.tx_buf[12];
    if (reg_num * 2 != rsp_len) { // 回复app 0x10指令后，对方回复的ack信息这里不处理
        LOG_E("reg_num * 2 != rsp_len, reg_num = %d, rsp_len = %d", reg_num, rsp_len);
        return;
    }
    uint16_t Rec_type = (uart_date.tx_buf[13] << 8) | uart_date.tx_buf[14];
    LOG_D("Screen_param_rw, Rec_type: %d", Rec_type);
    LOG_HEX(uart_date.tx_buf, 16);
    if (Rec_type == 5) { // 读取操作
        Screen_param_read();
    } else if (Rec_type == 30002) { // 写入操作
        Screen_param_write();
    }
}

/*密码查询*/
void Pass_Query(uint16_t Num_add, uint8_t slave_addr) {
    uint8_t *Modbus_ack = NULL;
    uint8_t data_len = Num_add * 2 + 5;
    uint16_t Res_crc;
    Modbus_ack = malloc(data_len * sizeof(uint8_t));
    if (Modbus_ack == NULL) {
        LOG_E("Modbus_ack malloc failed!");
        return;
    }
    memset(Modbus_ack, 0, data_len);
    Modbus_ack[0] = slave_addr;
    Modbus_ack[1] = 0x03;
    Modbus_ack[2] = Num_add << 1;
    Modbus_ack[14] = 0x01; // Reserved Flag
    readPwd_from_tinyfs(); // 读取文件系统密码
    for (uint8_t j = 0; j < 6; j += 2) {
        Modbus_ack[12 + 3 + j] = pwd[4 - j];
        Modbus_ack[13 + 3 + j] = pwd[5 - j];
    }
    // 磁吸屏没有串口端获取版本号，版本号这里固定为2010
    uint16_t modbus_version_2010 = 2010;
    Modbus_ack[data_len - 4] = (uint8_t)(modbus_version_2010 >> 8);
    Modbus_ack[data_len - 3] = (uint8_t)(modbus_version_2010 & 0xFF);
    // Modbus_ack[data_len - 4] = (uint8_t)(Modbus_version >> 8);
    // Modbus_ack[data_len - 3] = (uint8_t)(Modbus_version & 0xFF);
    Res_crc = Modbus_Crc16Cal(Modbus_ack, data_len - 2); // 本身的CRC位不校验;
    Modbus_ack[data_len - 2] = (uint8_t)(Res_crc & 0xFF);
    Modbus_ack[data_len - 1] = (uint8_t)(Res_crc >> 8);
    // LOG_I("save password is:\n");
    // for (uint8_t i = 0; i < sizeof(pwd); i++) {
    //     LOG_I("%d", pwd[i]);
    // }
    AES_BleSendBytes(Modbus_ack, data_len, true); // 回复IOT密码信息
    free(Modbus_ack);
}

/*EB3A调试模块，针对开发板调试，模拟ARM回复APP信息进入主页*/
// uint8_t ver_ack[] = {0x01, 0x03, 0x02, 0x03, 0xFB, 0xF9, 0x37};
uint8_t ver_ack[] = {0x01, 0x03, 0x02, 0x07, 0xDA, 0x00, 0x00};
uint8_t ver2_ack[] = {0x01, 0x03, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xDB, 0xFC, 0xD1};
uint8_t ver_35_ack[] = {
    0x01, 0x03, 0x6A, 0x45, 0x42, 0x33, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0xFB, 0xED, 0x0A, 0x8B, 0xF7, 0x02, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x39, 0xCE, 0x00,
    0x03, 0x39, 0x69, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0xCD, 0x99, 0x00, 0x0D, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCD, 0xDB, 0x00,
    0x0D, 0x00, 0x01, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87, 0x14};
uint8_t ver_43_ack[] = {
    0x01, 0x03, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0xE0, 0x7A};
/*iot调试 模拟ARM回应*/
void debug_test(uint8_t *data) {
    uint16_t Res_crc = 0;
    Res_crc = Modbus_Crc16Cal(ver_35_ack, sizeof(ver_35_ack) - 2); // 本身的CRC位不校验;
    ver_35_ack[sizeof(ver_35_ack) - 2] = (uint8_t)(Res_crc & 0xFF);
    ver_35_ack[sizeof(ver_35_ack) - 1] = (uint8_t)(Res_crc >> 8);

    Res_crc = Modbus_Crc16Cal(ver_ack, sizeof(ver_ack) - 2); // 本身的CRC位不校验;
    ver_ack[sizeof(ver_ack) - 2] = (uint8_t)(Res_crc & 0xFF);
    ver_ack[sizeof(ver_ack) - 1] = (uint8_t)(Res_crc >> 8);

    uart_date_t *pctx = &uart_date;
    if (data[7] == 0x03) { // MODBUS查询指令
        uint16_t Recover_addr = (uint16_t)(data[8] << 8) + data[9]; /*询问的首地址*/
        if (Recover_addr == 0x10) {
            memcpy(pctx->rx_buf, ver_ack, sizeof(ver_ack));
            AES_BleSendBytes(pctx->rx_buf, sizeof(ver_ack), true);
        } else if (Recover_addr == 0x0A) {
            memcpy(pctx->rx_buf, ver_35_ack, sizeof(ver_35_ack));
            AES_BleSendBytes(pctx->rx_buf, sizeof(ver_35_ack), true);
        }
        //        else if(Recover_addr == 0x01)
        //        {
        //            memcpy(pctx->rx_buf, ver2_ack, sizeof(ver2_ack));
        //			AES_BleSendBytes(pctx->rx_buf, sizeof(ver2_ack), true);
        //        }
        else if (Recover_addr == 0xbb8) {
            memcpy(pctx->rx_buf, ver_43_ack, sizeof(ver_43_ack));
            AES_BleSendBytes(pctx->rx_buf, sizeof(ver_43_ack), true);
        }
    }
}
