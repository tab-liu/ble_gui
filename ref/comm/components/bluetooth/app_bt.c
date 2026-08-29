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
#include "app_bt.h"
#include "ble_dev.h"
#include "ble_client_central.h"
#include "task_debug.h"
#include "parameter.h"
#include "modbus_slave.h"
#include "md2can.h"
#include "can_data.h"
#include "can_protocol.h"
#include "ble_crypt.h"
#include "xmodem_client.h"
#include "gatts_svr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "udt_transfer.h"
#include "udt_port.h"

#include "comm_define.h"
#include "ble_client_central.h"
#include "utils.h"
#include "aiot_log.h"

#define TAG "[APP_BLE]"


static iot_ble_recv_struct iot_ble_recv = {
    .msg_queue = NULL,
};

ble_encrypt_t ble_encrypt_info = {
    .flag.bit.is_enable = 1,
    .flag.bit.init_done = 0,          // 蓝牙初始化状态
    .flag.bit.is_adv_on = 0,          // 蓝牙广播状态
    .flag.bit.ble_connect = 0,        // 蓝牙连接状态
    .flag.bit.is_encrypted_fresh = 0, // 未绑定
    .flag.bit.is_encrypted = 0,       // 未加密
    .authenticate_state = BLE_AUTHENTICATE_REQUEST,
};

ble_authenticate_buf_t ble_authenticate_buf = {0};
static USE_EXT_RAM_BSS uint8_t retransmit_data[150] = {0}; // 重传缓存
static USE_EXT_RAM_BSS uint8_t retransmit_len = 0;
static USE_EXT_RAM_BSS uint8_t ble_resp_data[260] = {0};

static void iot_ble_init(void);

typedef struct
{
    uint8_t *data;
    int data_len;
} ble_ff03_tx_struct;

static QueueHandle_t xQueue_ble_ff03_tx = NULL;

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    // #if 0
    // if(ble_encrypt_info.flag.bit.ble_connect == 1)
    // {
    // 	mbedtls_printf("%s", info);
    // 	for (uint32_t i = 0; i < len; i++) {
    // 		mbedtls_printf("%s%02X%s", i % 16 == 0 ? "\n	  ":" ",
    // 						buf[i], i == len - 1 ? "\n":"");
    // 	}
    // }
    // #else
    // if(ble_encrypt_info.flag.bit.ble_connect == 1)
    // {
    // 	ESP_LOGW(TAG, "%s", info);
    // 	ESP_LOG_BUFFER_HEX(TAG, buf, len);
    // }
    // #endif
}
 
 
/*------------------------------------------------------------------------------
 Function: iot_ble_response
 -----------------------------------------------------------------------------*/
/**
  * @brief      蓝牙发送报文
  * @param[in]  const uint8_t *rsp_data  
                uint16_t rsp_len         
                uint8_t TxChannel        
  * @param[out] None
  * @return     void
  */
void iot_ble_response(const uint8_t *rsp_data, uint16_t rsp_len, uint8_t TxChannel) //TODO: 放到bt server里面，提供接口
{
    uint8_t *encrypt_data = NULL;
    uint8_t *rsp_pdata = NULL;
    uint16_t rsp_plen = 0;
    uint16_t encrypt_len = 0;

    if (!ble_encrypt_info.flag.bit.ble_connect)
    {
		/*更新状态二次判断*/
        ble_encrypt_info.flag.bit.ble_connect = drv_ble_get_ConnectEvent();
        if (!ble_encrypt_info.flag.bit.ble_connect) 
        { 
	        /* 蓝牙连接断开 */
	        return;
    	}
	}
		 
	ESP_LOGD(TAG, "iot_ble_response：respones len %d", rsp_len);
	 
    if (ble_encrypt_info.flag.bit.is_encrypted == 1) // 判断是否启用加密
    {
        encrypt_len = get_encrypt_sending_pack_len(rsp_len); // 获取长度
        if (ble_encrypt_info.authenticate_state == BLE_ENCRYPT_COMPLATE)
        {
            encrypt_len += 4; // 加上随机数长度
        }
        encrypt_data = iot_calloc(encrypt_len * sizeof(char)); // 申请内存
        if (encrypt_data == NULL)
        {
            ESP_LOGI(TAG, "malloc fail");
            return;
        }
        if (ble_encrypt_info.authenticate_state == BLE_ENCRYPT_COMPLATE)
        {
            ESP_LOGI(TAG, "iot_ble_response : AES-CBC");
            aes_cbc_encrypt_sending_pack(rsp_data, rsp_len, encrypt_data, &encrypt_len); // 生成AES-CBC加密包
            dump_buf("after encrypt1", encrypt_data, encrypt_len);
        }
        else
        {
            ESP_LOGI(TAG, "iot_ble_response : AES-ECB");
            encrypt_sending_pack(rsp_data, rsp_len, encrypt_data, &encrypt_len); // 生成AES-ECB加密包
        }

        rsp_pdata = encrypt_data;
        rsp_plen = encrypt_len;
    }
    else // 未加密
    {
        ESP_LOGD(TAG, "iot_ble_response : unencrypted");
        rsp_pdata = (uint8_t *)rsp_data;
        rsp_plen = rsp_len;
    }

    if (BLE_FF01_CHAR_VAL == TxChannel)
    {
        ble_dev_send(&(ble_data_t){
            .data = rsp_pdata,
            .len = rsp_plen,
        });
        reals.ble_frame_tx_counter++;
    }
    else if (BLE_FF03_CHAR_VAL == TxChannel)
    {
        ble_dev_post(&(ble_data_t){
            .data = rsp_pdata,
            .len = rsp_plen,
        });
        reals.ble_frame_tx_counter++;
    }

    if (encrypt_data != NULL)
    {
        free(encrypt_data);
        encrypt_data = NULL;
    }
}

/**
 * @brief ble事件回调函数
 * @param evt ble事件
 * @param arg 事件参数
 */
static void app_ble_evt_callback(uint8_t evt, void *arg)
{
    switch (evt)
    {
        case BLE_EVT_CONNECT:
            ESP_LOGI(TAG, "BLE_EVT_CONNECT");
            ble_encrypt_info.flag.bit.ble_connect = 1;
            reals.last_ble_client_connect_time = reals.now;
            break;
        case BLE_EVT_DISCONNECT:
            int reason = 0;
            if (arg != NULL)
            {
                reason = *(int *)arg;
            }
            ESP_LOGI(TAG, "BLE_EVT_DISCONNECT, reason=%d", reason);
            ble_encript_disconnect_reset();
			
            /* 蓝牙已中断，停止升级 */
            vXmodemClientExit(CHANNEL_BLE); /* 蓝牙断线,XMODEM协议退出 */
            reals.last_ble_client_disconn_time = reals.now;
            break;
        case BLE_EVT_SUBSCRIBE:
            ESP_LOGI(TAG, "BLE_EVT_SUBSCRIBE");
            if (arg != NULL)
            {
                uint8_t notify_flag1 = *(uint8_t *)arg;
                ble_encript_part(notify_flag1);
            }
            break;
        case BLE_EVT_ADV:
			ESP_LOGI(TAG, "BLE_EVT_ADV");

            break;

        default:
            ESP_LOGI(TAG, "BLE_EVT_UNKNOWN, evt=%d", evt);
        break;
    }
}


/*------------------------------------------------------------------------------
 Function: app_modbus_data_handle
 -----------------------------------------------------------------------------*/
/**
  * @brief      app modbus数据处理
  * @param[in]  uint8_t *data  
                int len        
  * @param[out] None
  * @return     static int
  */
static int app_modbus_data_handle(uint8_t *data, int len)
{
    int rsp_len = 0;
    int rst = 0;
    uint8_t md_addr = 0;

    /* modbus协议检查 */
    rst = Modbus_Format_Check(data, len);

    if (rst >= 0) // 功能码
    {
#if CONFIG_LOG_UDP_REDIRECT == 2 
        supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
        supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
        if ((1 == resource.ble) && (1 == direction.rx)) {
            aiot_log_push((const uint8_t *)data, (uint16_t)len, LOG_TYPE_BLE_RX);
        }
#endif

        md_addr = data[0];
        /* 为modbus转can指令申请内存 */
        can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};

#ifdef CONFIG_CAN_PORT_ENABLE
        // 当MODBUS为设置指令时,才需要开辟空间
        if ((rst == 0x06 || rst == 0x10)) { 
            can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
            if (!can_cmd.cmd) {
                ESP_LOGE(TAG, "ble to can malloc failed");
            }
        }
#endif

        ESP_LOGI("BLE_MODEBUS", "iot_ble_recieve len %d", len);
        // ESP_LOG_BUFFER_HEX_LEVEL("BLE_MODEBUS", data, len, ESP_LOG_WARN);

        reg_position_list_t *position_list = NULL;
        rsp_len = Modbus_Slave(data, (uint16_t)len, ble_resp_data, can_cmd.cmd, &can_cmd.num, MD_CHL_BLE, &position_list); /* modbus handle */
        if (rsp_len > 0)
        {
#if CONFIG_LOG_UDP_REDIRECT == 2 
            supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
            supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
            if ((1 == resource.ble) && (1 == direction.tx)) {
                aiot_log_push((const uint8_t *)data, (uint16_t)len, LOG_TYPE_BLE_TX);
            }
#endif

            ESP_LOGI("BLE_MODEBUS", "iot_ble_response: response len %d", len);
            // ESP_LOG_BUFFER_HEX_LEVEL("BLE_MODEBUS", ble_resp_data, rsp_len, ESP_LOG_INFO);
            /* modbus响应 */
            iot_ble_response(ble_resp_data, rsp_len, (uint8_t)BLE_FF01_CHAR_VAL); // modbus 响应给手机

            /* modbus指令转换为can指令发送到队列 */
            if (can_cmd.cmd) 
            {
#ifdef CONFIG_CAN_PORT_ENABLE
                sys_new_can_data_resend(&can_cmd, md_addr);
#else
                free(can_cmd.cmd);
#endif
            }
            
            if (NULL != position_list)
            {
#ifdef CONFIG_UART_PORT_ENABLE
                sys_new_position_and_transmit(position_list);
#else
                delete_set_position(position_list);  
#endif
            }

            if (vXmodemCmdCheck(data[0], CHANNEL_BLE)) // 初始化xmodem升级变量
            {
                ESP_LOGW(TAG, "ble received xmodem start cmd");
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
    else
    {
        rsp_len = rst;
    }

    return rsp_len;
}


/*
windy add
加密 处理辅助
*/
void ble_encript_disconnect_reset(void)
{
    ble_encrypt_info.flag.bit.is_encrypted_fresh = 0; // 绑定配对状态
    ble_encrypt_info.flag.bit.is_encrypted = 0;       // 加密状态
    ble_encrypt_info.flag.bit.ble_connect = 0;

    ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
    //	memset(ble_encrypt_info.remote_mac, 0, sizeof(ble_info.remote_mac));

    if (!ble_encrypt_info.flag.bit.is_encrypted_fresh) // 如果密钥参数未刷新
    {
        ble_authenticate_buf.status = BLE_REFRESH_ENCRYPT;
        ble_encrypt_info.flag.bit.is_encrypted_fresh = 1;
    }
}

/*
windy add
加密 处理辅助
*/
void ble_encript_part(uint8_t notify)
{
    if (notify == 0x0001) // 通知
    {
        // 打开notify通知
        if (ble_encrypt_info.authenticate_state == BLE_AUTHENTICATE_REQUEST)
        {
            ble_authenticate_buf.status = BLE_AUTHENTICATE_REQUEST;
        }
        ESP_LOGI(TAG, "ff01_notify : notify enable");
    }
    else if (notify == 0x0002) // 指示
    {
        ESP_LOGI(TAG, "ff01_notify : indicate enable");
    }
    else if (notify == 0x0000) // 关闭
    {
        if (ble_encrypt_info.authenticate_state != BLE_AUTHENTICATE_REQUEST)
        {
            ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
            ble_encrypt_info.flag.bit.is_encrypted = 0;
        }
        ESP_LOGI(TAG, "ff01_notify : notify/indicate disable ");
        if (!ble_encrypt_info.flag.bit.is_encrypted_fresh) // 如果密钥参数未刷新
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



/*------------------------------------------------------------------------------
 Function: ble_rx_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      app modbus数据处理
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void ble_rx_process(void)
{
	msg_struct income = {NULL, 0};//BLE rx 队列解析

    if (iot_ble_recv.msg_queue && xQueueReceive(iot_ble_recv.msg_queue, &income, pdMS_TO_TICKS(0)) == pdTRUE)
	{
        reals.ble_frame_rx_counter++;
		if(app_modbus_data_handle(income.msg_ptr, income.msg_len) >= 0) 
        {
            goto end;
        }
        else if ((income.msg_ptr[0] == UDT_PROT_MARK) && (income.msg_ptr[1] == UDT_PROT_VER))//服务器通用数据通道协议
        {
            ESP_LOGW(TAG, "udt received ble data and relay it to device, data length: %d, slave_addr: %d, funcode: %d",
                    income.msg_len, income.msg_ptr[10], income.msg_ptr[11]);

#ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
            /*数据通道*/
            udt_recv_push(income.msg_ptr, income.msg_len, UDT_MODE_BLE);
#endif
            goto end;
        }
    }

    ble_resp_data[0] = vXmodemClient(CHANNEL_BLE, income.msg_ptr, income.msg_len); /* xmodem运行 */
    if (ble_resp_data[0])
    {
        if (ble_resp_data[0] != 0x06) // XMODEM_ACK
        {
            ESP_LOGW(TAG, "resp:0x%x", ble_resp_data[0]);
        }
        /* 通过modbus发送xmodem客户端的响应数据 */
        iot_ble_response(ble_resp_data, 1, (uint8_t)BLE_FF01_CHAR_VAL);
    }

end:
    if (income.msg_ptr != NULL)
    {
        free(income.msg_ptr);
        income.msg_ptr = NULL;
    }
}

/**
 * @brief uart rx data透传给BLE
 *
 * @param data 数据
 * @param len 长度
 */
static void uart_data_to_ble(uint8_t *data, int len)
{
#if CONFIG_UART_PORT_ENABLE
    md_buff_t md_buff = {0};
    md_buff.in_buff = data;
    md_buff.in_len = len;
    //	md_buff.out_buff = out_buffer;

    reals.struct_uart1.FlagRx_ok = 0;
    //	memcpy(md_buff.out_buff, reals.struct_uart1.Rxbuffer, reals.struct_uart1.rxBytesNum);
    md_buff.out_buff = reals.struct_uart1.Rxbuffer;

    md_buff.out_len = reals.struct_uart1.rxBytesNum;

    /* modbus响应 */
    ble_dev_send(&(ble_data_t){
        .data = md_buff.out_buff,
        .len = md_buff.out_len,
    });
    ESP_LOGV(TAG, "response data len: %d, value:", *md_buff.out_len);
    log_hex_custom(TAG, md_buff.out_buff, md_buff.out_len, ESP_LOG_DEBUG);
#endif
}

#define ECDH_TX_LEN 134

void ble_authenticate_task_step(void)   //TODO: 移到bt server中
{
    static uint8_t ble_authenticate_buf_old = 0xFF;
    uint8_t data[ECDH_TX_LEN];
    uint8_t len = 0;
    static uint8_t sauthenticate_state_old = 0xFF;

    if (sauthenticate_state_old != ble_encrypt_info.authenticate_state)
    {
        sauthenticate_state_old = ble_encrypt_info.authenticate_state;
        if (BLE_ENCRYPT_COMPLATE == ble_encrypt_info.authenticate_state)
        {
            if (ble_authenticate_buf.data)
            {
                free(ble_authenticate_buf.data);
                ble_authenticate_buf.data = NULL;
            }
        }
    }

    if (ble_authenticate_buf_old != ble_authenticate_buf.status)
    {
        ble_authenticate_buf_old = ble_authenticate_buf.status;

        ESP_LOGI(TAG, "ble_authenticate_buf.status=:%d:", ble_authenticate_buf.status);

        switch (ble_authenticate_buf.status)
        {
#if 1
        case BLE_REFRESH_ENCRYPT: // 刷新密钥
        {
            refresh_encrypt_param(); // 刷新加密参数
            ble_encrypt_info.flag.bit.is_encrypted_fresh = 0;
            ble_encrypt_info.flag.bit.is_encrypted = 0,
            ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
        }
        break;

        case BLE_ENCRYPT_PACK_RETRANSMITSSION: // 鉴权请求包重传

            break;

        case BLE_AUTHENTICATE_REQUEST: // 鉴权请求
        {
            len = iot_ble_authen_request(data); // 鉴权请求包
            retransmit_len = len;
            memcpy(retransmit_data, data, retransmit_len); // 保存当前数据到重传包缓存
            iot_ble_response(data, len, (uint8_t)BLE_FF01_CHAR_VAL);
            ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_RESPOND; // 下一步，鉴权响应
            ESP_LOGI(TAG, "Authenicate request");
        }
        break;
        case BLE_AUTHENTICATE_RESPOND: // 鉴权响应
        {
            ESP_LOGI(TAG, "BLE_AUTHENTICATE_RESPOND");
            if (get_pack_type(ble_authenticate_buf.data, ble_authenticate_buf.len) == AUTHEN_RESOPN) // 检查类型
            {
                if (iot_ble_check_authen_respond(ble_authenticate_buf.data, ble_authenticate_buf.len, data, &len)) // ok
                {
                    ESP_LOGI(TAG, "Authenicate success!");
                    ble_encrypt_info.authenticate_state = BLE_ECDH_REQUEST; // ECDH密钥协商
                    iot_ble_response(data, len, (uint8_t)BLE_FF01_CHAR_VAL);
                    if (ble_encrypt_info.flag.bit.is_encrypted == 0)
                    {
                        ble_encrypt_info.flag.bit.is_encrypted = 1; // 启用aes加密通信
                    }
                    ble_authenticate_buf.status = BLE_ECDH_REQUEST;
                }
                else
                {
                    ESP_LOGI(TAG, "Authenicate fail!, so disconnect!");
                    ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
                    iot_ble_response(data, len, (uint8_t)BLE_FF01_CHAR_VAL);
                    // vTaskDelay(pdMS_TO_TICKS(10));
                    // 主动断开连接
                    if (ble_disconnect() == ESP_OK)
                    {
                        ble_encrypt_info.flag.bit.ble_connect = 0;
                    }
                }
            }
            else
            {
                ESP_LOGI(TAG, "no BLE_AUTHENTICATE_RESPOND type");
            }
        }
        break;

        case BLE_ECDH_REQUEST: // ECDH密钥协商请求
        {
            //			 uint8_t ecdh_request[134];
            ESP_LOGI(TAG, "ECDH REQUEST!");
            iot_ble_authen_ecdh_key_agreement_request(data); // 生成ECDH密钥协商请求包
            ESP_LOGI(TAG, "agreement_request");

            retransmit_len = ECDH_TX_LEN;                  // sizeof(ecdh_request);
            memcpy(retransmit_data, data, retransmit_len); // 保存当前数据到重传包缓存
            iot_ble_response(data, ECDH_TX_LEN, (uint8_t)BLE_FF01_CHAR_VAL);
            dump_buf("ecdh_request", data, ECDH_TX_LEN);
            ble_encrypt_info.authenticate_state = BLE_ECDH_RESPOND; // 下一步，鉴权响应
        }
        break;

        case BLE_ECDH_RESPOND: // ECDH密钥协商回应
        {
            ESP_LOGI(TAG, "BLE_ECDH_RESPOND!,%d", ble_authenticate_buf.len);
            // ESP_LOGI(TAG, "BLE_ECDH_RESPOND, len:%d", ble_authenticate_buf.len);
            dump_buf("mobile respond data", ble_authenticate_buf.data, ble_authenticate_buf.len);
            if (ble_encrypt_info.authenticate_state == BLE_ECDH_RESPOND)
            {
                if (iot_ble_authen_ecdh_key_agreement_respond_check(ble_authenticate_buf.data, ble_authenticate_buf.len, data, &len))
                {
                    ESP_LOGI(TAG, "agreement success!");
                    retransmit_len = len;
                    memcpy(retransmit_data, data, retransmit_len); // 保存当前数据到重传包缓存
                    dump_buf("after:mobile respond data", retransmit_data, retransmit_len);
                    iot_ble_response(retransmit_data, retransmit_len, (uint8_t)BLE_FF01_CHAR_VAL);
                    ble_encrypt_info.authenticate_state = BLE_AES_CBC_ENCRYPT; // AES-CBC加密
                }
                else
                {
                    ESP_LOGI(TAG, "agreement fail, so disconnect!");
                    iot_ble_response(data, len, (uint8_t)BLE_FF01_CHAR_VAL);
                    ble_encrypt_info.authenticate_state = BLE_AUTHENTICATE_REQUEST;
                    // vTaskDelay(pdMS_TO_TICKS(30));
                    // 主动断开连接
                    if (ble_disconnect() == ESP_OK)
                    {
                        ble_encrypt_info.flag.bit.ble_connect = 0;
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
#endif
        default:
            break;
        }
    }
}
/*
蓝牙鉴权任务
50ms cycle
*/
#if 1
void ble_authenticate_resend(void)
{
    static uint32_t pre_time = 0;
    static uint16_t retry_times_1 = 0;
    static uint16_t retry_times_2 = 0;
    static uint16_t retry_times_3 = 0;

    if (( BLE_REFRESH_ENCRYPT == ble_encrypt_info.authenticate_state ) 
        || (BLE_ENCRYPT_COMPLATE == ble_encrypt_info.authenticate_state))
    {
        retry_times_1 = 0;
        retry_times_2 = 0;
        retry_times_3 = 0;
        return;
    }
        
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if((now_time - pre_time) < 50) return;
    pre_time = now_time;

    reals.authenticate_state = ble_encrypt_info.authenticate_state;
    reals.ble_authenticate_buf_status = ble_authenticate_buf.status;
    reals.ble_encrypt_info_flag = ble_encrypt_info.flag.all;

    if (BLE_AUTHENTICATE_RESPOND == ble_encrypt_info.authenticate_state) // 等待鉴权回复
    {
        if (++retry_times_1 > 100) // 5s
        {
            retry_times_1 = 100;
            ESP_LOGI(TAG, "retry_times:%d,can not receive mobile authentical respond", retry_times_1);
            //主动断开连接
            if(ble_disconnect() == ESP_OK) 
            {
                ble_encrypt_info.flag.bit.ble_connect = 0;
            } 
        }
        else if ((retry_times_1 > 10) && (retry_times_1 <= 15)) // 次数为5次。第1次重发间隔500ms，后4次重发间隔为50ms。
        {
            ESP_LOGI(TAG, "retransmit_len:%d", retransmit_len);
            dump_buf("retransmit", retransmit_data, retransmit_len);
            ESP_LOGI(TAG, "AUTHENTICATE_RESPOND retry_times:%dms", retry_times_1 * 50);
            iot_ble_response(retransmit_data, retransmit_len, (uint8_t)BLE_FF01_CHAR_VAL);
        }
        retry_times_2 = 0;
        retry_times_3 = 0;
    }
    else if (BLE_ECDH_RESPOND == ble_encrypt_info.authenticate_state) // 等待鉴权响应
    {
        if (++retry_times_2 > 100) // 5s
        {
            retry_times_2 = 100;
            ESP_LOGI(TAG, "retry_times:%d,can not receive mobile ECDH respond", retry_times_2);
            //主动断开连接
            if(ble_disconnect() == ESP_OK) 
            {
                ble_encrypt_info.flag.bit.ble_connect = 0;
            } 
        }
        else if ((retry_times_2 > 10) && (retry_times_2 <= 15)) // 次数为5次。第1次重发间隔500ms，后4次重发间隔为50ms。
        {
            ESP_LOGI(TAG, "retransmit_len:%d", retransmit_len);
            dump_buf("retransmit", retransmit_data, retransmit_len);
            ESP_LOGI(TAG, "BLE_ECDH_RESPOND retry_times:%dms", retry_times_2 * 50);
            iot_ble_response(retransmit_data, retransmit_len, (uint8_t)BLE_FF01_CHAR_VAL);
        }

        retry_times_1 = 0;
        retry_times_3 = 0;
    }
    else if (BLE_AES_CBC_ENCRYPT == ble_encrypt_info.authenticate_state) // 等待手机正式数据(因APP需要额外操作，导致本次数据会延时发送到IOT)
    {
        if (++retry_times_3 > 220) // 11s
        {
            retry_times_3 = 220;
            ESP_LOGI(TAG, "retry_times:%d,can not receive mobile modbus", retry_times_3);
            //主动断开连接
            if(ble_disconnect() == ESP_OK) 
            {
                ble_encrypt_info.flag.bit.ble_connect = 0;
            } 
        }
        else if ((retry_times_3 >= 100) && (0 == (retry_times_3 % 20))) // 次数为5次。第1次重发间隔5s，后4次重发间隔为1s。
        {
            ESP_LOGI(TAG, "retransmit_len:%d", retransmit_len);
            dump_buf("retransmit", retransmit_data, retransmit_len);
            ESP_LOGI(TAG, "BLE_AES_CBC_ENCRYPT，retry_times:%dms ,can not receive mobile modbus", retry_times_3 * 50);
            iot_ble_response(retransmit_data, retransmit_len, (uint8_t)BLE_FF01_CHAR_VAL);
        }

        retry_times_1 = 0;
        retry_times_2 = 0;
    }
    else
    {
        retry_times_1 = 0;
        retry_times_2 = 0;
        retry_times_3 = 0;
    }
}

#else
void ble_authenticate_resend(void)
{

    static uint16_t retry_times_1 = 0;
    static uint16_t retry_times_2 = 0;
    static uint16_t retry_times_3 = 0;

    reals.authenticate_state = ble_encrypt_info.authenticate_state;
    reals.ble_authenticate_buf_status = ble_authenticate_buf.status;
    reals.ble_encrypt_info_flag = ble_encrypt_info.flag.all;

    if (BLE_AUTHENTICATE_RESPOND == ble_encrypt_info.authenticate_state) // 等待鉴权回复
    {
        if (++retry_times_1 >= 3000)
        {
            retry_times_1 = 3000;
            ESP_LOGI(TAG, "retry_times:%d,can not receive mobile authentical respond", retry_times_1);
            ble_disconnect(); // 断开连接
        }
        if (0 == (retry_times_1 % 100))
        {
            ESP_LOGI(TAG, "retransmit_len:%d", retransmit_len);
            dump_buf("retransmit", retransmit_data, retransmit_len);
            ESP_LOGI(TAG, "AUTHENTICATE_RESPOND retry_times:%d", retry_times_1);
            iot_ble_response(retransmit_data, retransmit_len, (uint8_t)BLE_FF01_CHAR_VAL);
        }
        retry_times_2 = 0;
        retry_times_3 = 0;
    }
    else if (BLE_ECDH_RESPOND == ble_encrypt_info.authenticate_state) // 等待鉴权响应
    {
        if (++retry_times_2 >= 3000)
        {
            retry_times_2 = 3000;
            ESP_LOGI(TAG, "retry_times:%d,can not receive mobile ECDH respond", retry_times_2);
            ble_disconnect(); // 断开连接
        }
        if (0 == (retry_times_2 % 100))
        {
            ESP_LOGI(TAG, "retransmit_len:%d", retransmit_len);
            dump_buf("retransmit", retransmit_data, retransmit_len);
            ESP_LOGI(TAG, "AUTHENTICATE_RESPOND retry_times:%d", retry_times_2);
            iot_ble_response(retransmit_data, retransmit_len, (uint8_t)BLE_FF01_CHAR_VAL);
        }

        retry_times_1 = 0;
        retry_times_3 = 0;
    }
    else if (BLE_AES_CBC_ENCRYPT == ble_encrypt_info.authenticate_state) // 等待手机正式数据
    {
        if (++retry_times_3 >= 3000)
        {
            retry_times_3 = 3000;
            ESP_LOGI(TAG, "retry_times:%d,can not receive mobile modbus", retry_times_3);
            ble_disconnect(); // 断开连接
        }
        if (0 == (retry_times_3 % 100))
        {
            ESP_LOGI(TAG, "retransmit_len:%d", retransmit_len);
            dump_buf("retransmit", retransmit_data, retransmit_len);
            ESP_LOGI(TAG, "BLE_AES_CBC_ENCRYPT，retry_times:%d,can not receive mobile modbus", retry_times_3);
            iot_ble_response(retransmit_data, retransmit_len, (uint8_t)BLE_FF01_CHAR_VAL);
        }

        retry_times_1 = 0;
        retry_times_2 = 0;
    }
    else
    {
        retry_times_1 = 0;
        retry_times_2 = 0;
        retry_times_3 = 0;
    }
}

#endif

/*------------------------------------------------------------------------
*@Function： ble_data_give
蓝牙接收，填充接收队列


*pdata：原始报文

income:解密后报文
iot_ble_recv.msg_queue：解密后压入队列

*@return
*/
void ble_data_give(const uint8_t *pdata, uint16_t len) // static
{
    msg_struct income = {NULL, 0};
    // uint16_t rec_len = 0;
#if 1
    // ESP_LOGE(TAG, "ble_data_give,len:%d", len);
    // 如果启用加密，并且授权通过
    if ((ble_encrypt_info.flag.bit.is_encrypted == 1)                    // 判断是否启用加密
        && (ble_encrypt_info.authenticate_state >= BLE_AES_CBC_ENCRYPT)) // 加密握手成功才解析报文
    {
        if (ble_encrypt_info.authenticate_state == BLE_AES_CBC_ENCRYPT)
        {
            ble_encrypt_info.authenticate_state = BLE_ENCRYPT_COMPLATE;
            ESP_LOGI(TAG, "BLE_ENCRYPT_COMPLATE");
        }
        if (len >= 22 && len < 1500) // 接收长度限制
        {
            income.msg_ptr = iot_calloc(len * sizeof(char));
            if (income.msg_ptr == NULL)
            {
                ESP_LOGE(TAG, "malloc fail");
                return;
            }
            if (ble_encrypt_info.authenticate_state == BLE_ENCRYPT_COMPLATE) // BLE_AES_CBC_ENCRYPT
            {
                ESP_LOGD(TAG, "before decrypt len:%d", len);
                // dump_buf("before decrypt", pdata, len);
                aes_cbc_decrypt_receive_pack(pdata, len, income.msg_ptr, &income.msg_len); // cbc解密
                ESP_LOGD(TAG, "after decrypt len:%d", income.msg_len);
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, income.msg_ptr, income.msg_len, ESP_LOG_DEBUG);
            }

            if (income.msg_len > len) // 如果明文长度超过接收长度，提示非法长度，限制长度，防止溢出
            {
                ESP_LOGE(TAG, "over len:%d", income.msg_len);
                income.msg_len = len;
            }
            if (iot_ble_recv.msg_queue && xQueueSend(iot_ble_recv.msg_queue, &income, pdMS_TO_TICKS(100)) != pdPASS)
            {
                free(income.msg_ptr);
                income.msg_ptr = NULL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "invalid len: %d", len);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, pdata, len, ESP_LOG_ERROR);
        }
    }
    else if (BLE_AUTHENTICATE_RESPOND == ble_encrypt_info.authenticate_state) // 鉴权回应
    {
        if (ble_authenticate_buf.data) // 不要重复开辟内存
        {
            free(ble_authenticate_buf.data);
            ble_authenticate_buf.data = NULL;
        }
        ble_authenticate_buf.data = (uint8_t *)iot_calloc(sizeof(uint8_t) * 1024); // 在加密开始开辟内存，在加密过程完成后释放，减少RAM占用
        if (NULL != ble_authenticate_buf.data)                                    //
        {
            ble_authenticate_buf.len = len;
            ble_authenticate_buf.status = BLE_AUTHENTICATE_RESPOND; // 鉴权结果
            memcpy(ble_authenticate_buf.data, pdata, ble_authenticate_buf.len);
            ESP_LOGI(TAG, "send BLE_AUTHENTICATE_RESPOND");
        }
        else
        {
            ESP_LOGE(TAG, "malloc fail");
        }
    }
    else if (BLE_ECDH_RESPOND == ble_encrypt_info.authenticate_state) // ECDH密钥回应
    {
        if (get_pack_type((uint8_t *)pdata, len) != AUTHEN_RESOPN) // 如果不是鉴权回应包
        {

            if (NULL != ble_authenticate_buf.data)
            {
                decrypt_receive_pack(pdata, len, ble_authenticate_buf.data, &ble_authenticate_buf.len); // AES-ECB解密
                if (get_pack_type(ble_authenticate_buf.data, ble_authenticate_buf.len) == ECDH_AGRESSMENT_RESPOND) // 类型为密钥协商回应
                {
                    ble_authenticate_buf.status = BLE_ECDH_RESPOND; // ECDH密钥协商响应
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
        else // 鉴权回应包，丢弃
        {
            ESP_LOGE(TAG, "Repeated authen respond, abandon");
        }
    }
    else // 未加密
    {

        ESP_LOGE(TAG, "without authenicate verify!");
    }
#else
    income.msg_ptr = iot_calloc(len * sizeof(char));
    if (income.msg_ptr == NULL)
    {
        ESP_LOGE(TAG, "malloc fail");
        return;
    }

    memcpy(income.msg_ptr, pdata, len);
    income.msg_len = len;

    if (iot_ble_recv.msg_queue && xQueueSend(iot_ble_recv.msg_queue, &income, pdMS_TO_TICKS(100)) != pdPASS)
    {
        free(income.msg_ptr);
        income.msg_ptr = NULL;
    }
#endif
}

/**
 * @brief 蓝牙通信线程
 *
 * @param param 线程参数
 */
void iot_ble_task(void *param)
{
    // 任务时间统计声明和初始化
    TASK_TIME_STATS_DECLARE(iot_ble_task);

#ifdef BLE_CLIENT_ENABLE
	ble_client_init();
#endif

    /*蓝牙初始化配置*/
    iot_ble_init();

    while (1)
    {
        // 开始时间统计
        TASK_TIME_START(iot_ble_task);

#ifdef BLE_CLIENT_ENABLE
		ble_client_Rx_data_prase();
#endif

        /* 尝试接收数据 */
        ble_data_t pdata = {0};
        int ret = ble_dev_recv(&pdata, 10);
        if (ret == 0) {
            ble_data_give(pdata.data, pdata.len);
        } else {
            /*蓝牙鉴权*/
            ble_authenticate_task_step();

            /*蓝牙鉴权超时重发*/
            ble_authenticate_resend();

            /*蓝牙主动上报任务*/
            iot_ble_actively_report_task();
        }

        /* 数据处理 */
        ble_rx_process();

        /* 释放接收蓝牙数据时分配的内存 */
        if (pdata.data)
        {
            free(pdata.data);
            pdata.data = NULL;
        }

        // 结束时间统计并打印
        TASK_TIME_END(iot_ble_task);
        TASK_TIME_PRINT(iot_ble_task, 1000); // 每1000次循环打印一次
        TASK_TIME_RUNS_MAX_TO_FIELD(iot_ble_task, reals.thread2_max_runtime_us);
        TASK_TIME_TIMEOUT_CLEAN(iot_ble_task, TASK_COMMON_CLEAN_TIMEOUT_MS);
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
        return;
    }
    char new_name[30] = {0};
    snprintf(new_name, sizeof(new_name), "%s%013llu", dev_type, dev_sn);
    uint8_t name_len = strlen(new_name);
    if (name_len > 29)
    {
        name_len = 29;
    }
    ESP_LOGI("ble advertise", "new_name:%s", new_name);
    if (ble_set_name(new_name) != 0)
    {
        ESP_LOGE(TAG, "ble_set_name error");
    }
}

inline const ble_encrypt_t *iot_get_ble_info(void)
{
    return &ble_encrypt_info;
}

/*------------------------------------------------------------------------------
 Function: iot_ble_on_off_status
 -----------------------------------------------------------------------------*/
/**
  * @brief      返回蓝牙使能标志
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_ble_on_off_status(void)
{
    return IotSetData.dev_info_t.on_off.bit.ble_enable;
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
	
	ble_encrypt_info.flag.bit.is_enable = 1;
}

/*------------------------------------------------------------------------------
 Function: iot_ble_stop
 -----------------------------------------------------------------------------*/
/**
  * @brief      关闭蓝牙
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_ble_stop(void)
{
    if(ble_advertise_stop() != 0)
    {
        ESP_LOGE(TAG, "iot_ble_stop error");
    }

	ble_encrypt_info.flag.bit.is_enable = 0;
}

/*------------------------------------------------------------------------------
 Function: iot_ble_deinit
 -----------------------------------------------------------------------------*/
/**
  * @brief      蓝牙逆初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_ble_deinit(void) {
    ble_dev_deinit();
	ble_encrypt_info.flag.bit.is_enable = 0;
    ble_encrypt_info.flag.bit.init_done = 0;
}

/*------------------------------------------------------------------------------
 Function: iot_ble_reinit
 -----------------------------------------------------------------------------*/
/**
  * @brief      蓝牙重新初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_ble_reinit(void) {
    /* 设置蓝牙名称 */
    char ble_name[40];
    
    if ((0 != dev_factory.dev_sn) 
        && (MASS_PRODUCTION_DEFAULT_DEV_SN != dev_factory.dev_sn))
    {
        snprintf(ble_name, sizeof(ble_name), "%s%013llu", dev_factory.dev_type, dev_factory.dev_sn);
        ESP_LOGI(TAG, "INV_dev_sn iot_ble_task init,ble_name =%s", ble_name);
        assert(!ble_dev_init(ble_name, app_ble_evt_callback)); // 蓝牙初始化
    }
    else
    {
        // 逆变SN无效时直接使用IOT SN
        snprintf(ble_name, sizeof(ble_name), "%s%013llu", iot_factory.iot_type, iot_factory.iot_sn);
        ESP_LOGI(TAG, "iot_sn iot_ble_task init,ble_name =%s", ble_name);
        assert(!ble_dev_init(ble_name, app_ble_evt_callback)); // 蓝牙初始化
    }

    /* 蓝牙接收消息队列 */
    if (!iot_ble_recv.msg_queue) {
        iot_ble_recv.msg_queue = xQueueCreate(5, sizeof(msg_struct));
        if (!iot_ble_recv.msg_queue) {
            ESP_LOGE(TAG, "ble received queue create failed");
        }
    }

    ble_dev_reinit(ble_name, app_ble_evt_callback); // 蓝牙初始化

	ble_encrypt_info.flag.bit.is_enable = 1;
    ble_encrypt_info.flag.bit.init_done = 1;

    if (IotSetData.dev_info_t.on_off.bit.ble_enable != 1) {
        /*蓝牙停止广播*/
        iot_ble_stop();
    }
}

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
        xQueue_ble_ff03_tx = xQueueCreate(32, sizeof(ble_ff03_tx_struct));
        if (xQueue_ble_ff03_tx == NULL)
        {
            ESP_LOGE(TAG, "ble_ff03_tx_queue_init create failed");
        }
    }
}

/**
 * @brief      紧急发送存储到队列缓存
 * @param[in]  void
 * @param[out] None
 * @return     void
 */
void ble_ff03_tx_queue_push(uint8_t *pdata, uint16_t len)
{
    if (0 == is_chr3_notify_open())
        return;
    
    ble_ff03_tx_struct queue_msg;
    queue_msg.data = (uint8_t *)iot_calloc(len);

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
            queue_msg.data = NULL;
            ESP_LOGE(TAG, "ble_ff03_tx_queue_push failed");
        }
    }
}

/**
 * @brief      ble ff03主动上报任务
 */
void ble_ff03_report_task(void)
{
    ble_ff03_tx_struct queue_msg;

    if (1 == ble_encrypt_info.flag.bit.ble_connect)
    {
        // 21000
        if (xQueue_ble_ff03_tx && xQueueReceive(xQueue_ble_ff03_tx, &queue_msg, 0) == pdTRUE) // 非阻塞
        {
            if (queue_msg.data)
            {
#if CONFIG_LOG_UDP_REDIRECT == 2 
                supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
                supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
                if ((1 == resource.ble) && (1 == direction.tx)) {
                    aiot_log_push((const uint8_t *)queue_msg.data, (uint16_t)queue_msg.data_len, LOG_TYPE_BLE_TX);
                }
#endif

                ESP_LOGD(TAG, "ble_ff03_report_task, data_len:%d", queue_msg.data_len);
                iot_ble_response(queue_msg.data, queue_msg.data_len, (uint8_t)BLE_FF03_CHAR_VAL);
                free(queue_msg.data);
                queue_msg.data = NULL;
            }
        }
    }
}

/**
 * @brief      主动上报绑定帧
 * @param[in]  void
 * @param[out] None
 * @return     void
 */
void Modbus_Self_21000_Report_Ble(void)
{    
	uint16_t rsp_len = 0;
	uint16_t pointcnt = 0;	
	uint16_t point_tx_nums = 0;	
	uint16_t ver = 0;
	uint16_t point_bias = 0;	
    uint8_t *tx_buff = NULL;
    uint16_t buffer_len = 0;

    if (0 == ble_encrypt_info.flag.bit.ble_connect)
        return; // 蓝牙未连接
    ver = reals.modbus_self_report_ble;
    if (ver == 0)
        return;
    if (!Modbus_21000_semaphore_Take())
        return;

    ESP_LOGI(TAG, "Modbus_Self_21000_Report_Ble");

    switch (ver)
    {
    case 1:
        /*已组网设备信息帧响应*/
        pointcnt = Modbus_21000_1_Net_Point_Frame();
        break;

    case 3:
        /*升级状态查询响应*/
        pointcnt = Modbus_21000_3_Net_Point_Frame();
        break;

    case 5 :
        /*上报所有设备，包括已绑定和未绑定的*/
        pointcnt = Modbus_21000_5_Net_Point_Frame();
        break;

    default:
        ESP_LOGE(TAG, "Modbus_Self_21000_Report_Ble : Unknown Ver(%d)", ver);
        goto end;
    }

    ESP_LOGW(TAG, "Modbus_Self_21000_Report_Ble(%d)  pointcnt:%d", ver, pointcnt);

    if(pointcnt >= BIND_READ_MAX_DEV_NUM) {
        buffer_len = 20 + (BIND_READ_MAX_DEV_NUM * sizeof(POINT_BIND_INFO));
    } else if(pointcnt > 0) {
        buffer_len = 20 + (pointcnt * sizeof(POINT_BIND_INFO));
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

        ESP_LOGD(TAG, "Modbus_Self_21000_Report_Ble, reg_addr:%d, data_len:%d, data:", 21000, rsp_len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, tx_buff, rsp_len, ESP_LOG_DEBUG);

        ble_ff03_tx_queue_push(tx_buff, rsp_len);
    } while (pointcnt);

    if (tx_buff != NULL) free(tx_buff);

end:

    if(ver == reals.modbus_self_report_ble) reals.modbus_self_report_ble = 0; 
    Modbus_21000_semaphore_Give();
}

/**
 * @brief      蓝牙上报任务
 * @param[in]  void
 * @param[out] None
 * @return     void i
 */
void iot_ble_actively_report_task(void)
{
    /*检查蓝牙加密状态*/
    if (ble_encrypt_info.authenticate_state != BLE_ENCRYPT_COMPLATE) {
        return;
    }

    /*BLE FF03主动上报发送任务*/
    ble_ff03_report_task();

    /*文件读取增加流控检查，避免完全占用mbuf影响正常交互*/
    if ( gatts_flowctrl() )
    {
        /*蓝牙数据通道*/
        udt_ble_Report_to_app();
    }
}

#define BLE_INIT_TAG "[ble init]"

/**
 * @brief      蓝牙初始化配置
 * @param[in]  None
 * @param[out] None
 * @return     void
 */
static void iot_ble_init(void)
{
    /* 设置蓝牙名称 */
    char ble_name[40];
    
    if ((0 != dev_factory.dev_sn) 
        && (MASS_PRODUCTION_DEFAULT_DEV_SN != dev_factory.dev_sn))
    {
        snprintf(ble_name, sizeof(ble_name), "%s%013llu", dev_factory.dev_type, dev_factory.dev_sn);
        ESP_LOGI(BLE_INIT_TAG, "INV_dev_sn iot_ble_task init,ble_name =%s", ble_name);
        assert(!ble_dev_init(ble_name, app_ble_evt_callback)); // 蓝牙初始化
    }
    else
    {
        // 逆变SN无效时直接使用IOT SN
        snprintf(ble_name, sizeof(ble_name), "%s%013llu", iot_factory.iot_type, iot_factory.iot_sn);
        ESP_LOGI(BLE_INIT_TAG, "iot_sn iot_ble_task init,ble_name =%s", ble_name);
        assert(!ble_dev_init(ble_name, app_ble_evt_callback)); // 蓝牙初始化
    }

    /* 蓝牙接收消息队列 */
    if (!iot_ble_recv.msg_queue) {
        iot_ble_recv.msg_queue = xQueueCreate(5, sizeof(msg_struct));
        if (!iot_ble_recv.msg_queue) {
            ESP_LOGE(BLE_INIT_TAG, "ble received queue create failed");
        }
    }

    /*21000周期上报队列初始化*/
    ble_ff03_tx_queue_init();

    ble_encrypt_info.flag.bit.init_done = 1;

    if (IotSetData.dev_info_t.on_off.bit.ble_enable != 1) {
        /*蓝牙停止广播*/
        iot_ble_stop();
    }
}

uint8_t iot_ble_get_ConnectStatus(void)
{
    ble_get_status();
    return ble_encrypt_info.flag.bit.ble_connect;
}

/**
  * @brief 更新ble相关状态至modbus寄存器
  */
void iot_ble_update(void)
{
    ble_sta_t ble_sta;
    ble_get_mac_addr(&ble_sta);

    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.ble_mac, ble_sta.mac_addr, sizeof(ble_sta.mac_addr));
}

/**
 * @brief BLE广播状态检查
 */
void ble_adv_status_check(void)
{
    static uint8_t restart_adv_cnt = 0;                 // 广播异常恢复计数

    // 蓝牙驱动异常
    if(!drv_ble_get_init_flag()) return; 
    
    // 获取当前广播进行状态（0：未广播，1：正在广播）
    bool adv_running = drv_ble_advertise_status();  
    ble_encrypt_info.flag.bit.is_adv_on = adv_running ? 1 : 0;
    
    // 广播状态检查（仅在蓝牙已使能且未连接状态下）
    if (ble_encrypt_info.flag.bit.is_enable 
        && !ble_encrypt_info.flag.bit.ble_connect) {
        // 广播已暂停
        if(!adv_running) {
            restart_adv_cnt++;
            // 恢复广播
            if ( restart_adv_cnt > 2 ) {
                drv_ble_advertise_restart();
                ESP_LOGW(TAG, "ble_adv_status_check: Broadcasting is restart.");
            }
        } else {
            restart_adv_cnt = 0;
        }
    } 
}

