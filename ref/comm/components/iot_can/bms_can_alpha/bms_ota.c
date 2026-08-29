/**
  ******************************************************************************
  * @file      bms_ota.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/1/8
  * @brief     48V BMS-CAN协议升级（移植钟工）
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/8   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "utils.h"
#include "filesystem.h"
#include "bms_can.h"
#include "bms_ota.h"
#include "iot_ota.h"
#include "image_handle.h"
#include "http_client.h"
#include "comm_define.h"
#include "can_protocol.h"
#include "bms_type.h"
#include "dev_data_record.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[CAN_OTA_ALPHA]"

// 包序号最大大小
#define MAX_PACK_NUM    128

// 升级阶段状态
#define	ALPHA_OTA_STEP_SEND_READY		0//0
#define	ALPHA_OTA_STEP_WAIT_ACK_READY	1//1
#define	ALPHA_OTA_STEP_SEND_START		2//2
#define	ALPHA_OTA_STEP_WAIT_ACK_START	3//3
#define	ALPHA_OTA_STEP_READ_IMAGE_DATA	4//4
#define	ALPHA_OTA_STEP_SEND_DATA		5//5
#define	ALPHA_OTA_STEP_WAIT_IMAGE_DATA	6//6
#define	ALPHA_OTA_STEP_SEND_END			7//7
#define	ALPHA_OTA_STEP_WAIT_ACK_END		8//8

/* =============================== 文件内全局变量 ================================ */

// 升级相关变量
static uint16_t pack_num = 0;
static uint8_t step = 0;
static uint32_t image_remaining = 0;
static uint32_t image_total = 0;
static uint32_t image_offset = 0;
static uint8_t pct_value = 0;
static uint8_t retry_count = 0;
static uint8_t image_type = 0;
static bool bms_ota_doing = false;
static int target_addr = 0;//BMS OTA对象的设备数组[x]的x, 0~max-1
static uint8_t bms_addr = 0;
static uint32_t bat_version = 0; // 软件料号前四位
static int Can_rx_timeout = 0;//can rx bms 倒计时计数器，0表示rx超时

// 升级buffer
static USE_EXT_RAM_BSS uint8_t temp_data[768] = {0};

/* ================================ 模块函数定义 ================================ */

/**
 * @brief 判断当前是否正在进行BMS OTA升级
 *
 * @return true 正在升级，false 未升级
 */
bool bms_ota_is_doing(void) {
    return bms_ota_doing;
}

/**
 * @brief OTA相关CAN接收超时计时处理
 *
 * @param[in] interval 本次递减的时间间隔（单位与Can_rx_timeout一致）
 */
void bms_ota_time_handle(int interval) {
    if (Can_rx_timeout > interval) {
        Can_rx_timeout -= interval;
    } else {
        Can_rx_timeout = 0;
    }
}

/**
 * @brief 初始化BMS OTA升级流程
 *
 * @param[in] type        升级类型
 * @param[in] totalKbyte  升级总大小（单位：KB）
 * @param[in] index       APP传入的电池包索引（从0开始）
 * @param[in] addr        实际电池包CAN地址
 * @param[in] version     升级目标版本号
 * @return int 初始化结果，0成功，其他为错误码
 *
 * 调用场景：HTTP OTA、BLE OTA、掉电重发等
 */
int bms_ota_init(uint8_t type, uint32_t totalKbyte, uint8_t index, uint8_t addr, uint32_t version) {
     char model[12] = {0}; //penney add
    int result = 0;
    bat_data_t *bat_data = get_bat_data_ptr();
    bat_version = version / 100;
    step = 0;
    if (bat_version == 1014 || bat_version == 1024 || bat_version == 1021 || bat_version == 1047 || bat_version == 1022) {
        bms_addr = 0x01; // B300和B300S只接收0x01地址
        target_addr = 0;
    } else if (bat_version == 1018 || bat_version == 1011) { // B230
        uint32_t bat_sw_ver = 0;
        // iot_read_flash(B230_VERSION, (uint8_t *)&bat_sw_ver, 4); // 读取flash
        if(bat_sw_ver/100 != bat_version){
            // iot_write_flash(B230_VERSION, (const uint8_t *)&version, 4); // 记录flash
        }
        bms_addr = 0xFF; // B230只接收0xFF地址
        target_addr = 0;
        if(bat_data->all_data.bat_num == 0)
		{ // B230停留在boot直接升级不校验
            image_offset = IMAGE_FLASH_AREA_ADDRESS;
            image_total = totalKbyte * 1024;
            image_remaining = image_total;
            image_type = type;
            retry_count = 0;
            bms_ota_doing = true;
//            memset(ota_data.group.state, 0, sizeof(ota_data.group.state));
            // bms_sys_fault_to_dsp();// 电池包升级需要发送给DSP停机
            ESP_LOGI(TAG, "can ota image type: %hhu, size: %lukbytes", type, totalKbyte);
//            vTaskDelay(pdMS_TO_TICKS(1500)); // 先下发停机到DSP
            return 0; // 在函数末尾返回
        }
    } else {
        bms_addr = addr;
        target_addr = bms_addr_to_index(addr);
		target_addr = bms_version_to_index(bat_version);		
    }

    ESP_LOGI(TAG, "index:%d, addr:%d, BMS version:%lu, BMS addr:%d", target_addr, addr, bat_version, bms_addr);
    ESP_LOGI(TAG,"bat_num:%d, is_busy:%d,is_mix_err:%d", bat_data->all_data.bat_num, bat_data->state.bit.is_busy, bat_data->state.bit.is_mix_err);
    ESP_LOGI(TAG,"version:%lu, bat_version:%lu", bat_data->bms_data[target_addr].version[0], bat_version);
    if (target_addr < 0) 
	{
        ESP_LOGE(TAG, "index:%d, addr:%d, BMS addr not found, exit OTA", target_addr, addr);
        set_ota_pct_info(model,target_addr, type, 0, 1, 0, OTA_ERR_CAN_ERR);  // where = 1：IOT发送给设备，2：设备发送给子设备 TODO增加互斥锁
        result = -1;
    } 
	else if (bat_data->all_data.bat_num == 0 || bat_data->state.bit.is_busy == 1 || bat_data->state.bit.is_mix_err == 1) {
        ESP_LOGE(TAG, "index:%d, addr:%d, BMS is busy, exit OTA", target_addr, bms_addr);
        set_ota_pct_info(model,target_addr, type, 0, 1, 0, OTA_ERR_CAN_BUSY);
        result = -1;
    } 
	else if ((type == DEVICE_BMS && bat_data->bms_data[target_addr].version[0] / 100 != bat_version) ||
				(type == DEVICE_PACK_BMS && bat_data->bms_data[target_addr].version[0] / 100 != bat_version) ||
                (type == DEVICE_PACK_M1 && bat_data->bms_data[target_addr].version[1] / 100 != bat_version)) {
        ESP_LOGE(TAG, "index:%d, addr:%d, BMS version:%lu is not match:%lu00, exit OTA", target_addr, addr, bat_data->bms_data[target_addr].version[type == DEVICE_PACK_BMS ? 0 : 1], bat_version);
        set_ota_pct_info(model,target_addr, type, 0, 1, 0, OTA_ERR_FILE);
        result = -1;
    } 
	else 
	{
        image_offset = IMAGE_FLASH_AREA_ADDRESS;
        image_total = totalKbyte * 1024;
        image_remaining = image_total;
        image_type = type;
        retry_count = 0;
        bms_ota_doing = true;
//        memset(ota_data.group.state, 0, sizeof(ota_data.group.state));
        // bms_sys_fault_to_dsp();
        ESP_LOGI(TAG, "can ota image type: %hhu, size: %lukbytes", type, totalKbyte);
//        vTaskDelay(pdMS_TO_TICKS(1500)); // 先下发停机到DSP
    }
    return result; // 在函数末尾返回
}


/**
 * @brief 处理BMS OTA相关CAN帧
 *
 * @param[in] Rx_can_frame 接收到的CAN帧指针
 */
void bms_ota_handle(const twai_message_t *Rx_can_frame) 
{
    char model[12] = {0}; //penney add
    static uint8_t last_pct = 0;
    if (bms_ota_doing == false) {
        return ; // bms ota 未启动
    }

    memset(temp_data, 0, sizeof(temp_data));

    static twai_message_t frame = {
        .extd = 1,
        .data_length_code = 8,
        .data = {0},
    };
    static uint8_t frame_len = 0;

	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
	
	static uint8_t step_old;
	if ((step_old != step) && (step != 4) && (step != 6))
	{
		step_old = step;
    	ESP_LOGI(TAG, "index:%d, addr:%d, Step: %d, Retry: %d", target_addr, bms_addr, step, retry_count);		
	}
  
    switch (step)
    {
        case ALPHA_OTA_STEP_SEND_READY: //0 send ready
        case ALPHA_OTA_STEP_SEND_START: //2 send start
        case ALPHA_OTA_STEP_SEND_END: //7 send end
            if (retry_count > 10) { // 重发次数达到上线
                ESP_LOGE(TAG, "index: %d, addr:%d, Step: %d, Retry Max Exit OTA", target_addr, bms_addr, step);
                retry_count = 0; 
                step = 0xFF;
                set_ota_pct_info(model,target_addr, image_type, 0, 1, pct_value, OTA_ERR_NO_RESP);
                break;
            }
            
            last_pct = 0;
            pct_value = 0;
            pack_num = 0;
            memset(&frame.data[0], 0, 8);
            if (ALPHA_OTA_STEP_SEND_READY == step) //0
			{
                frame.identifier = BMS_OTA_READY; // 准备帧
                if (image_type == DEVICE_PACK_M1) {
                    frame.data[0] = 1; // 表示升级M1
                } else {
                    frame.data[0] = 0; // 表示升级BMS
                }
                ESP_LOGI(TAG, "send ready frame");
            } 
			else if (ALPHA_OTA_STEP_SEND_START == step)//2
            {
                frame.identifier = BMS_OTA_START; // 开始帧
                if (image_type == DEVICE_PACK_M1) {
                frame.data[0] = 1; // 表示升级M1
                } else {
                    frame.data[0] = 0; // 表示升级BMS
                }
                ESP_LOGI(TAG, "send start frame");
            } 
			else //ALPHA_OTA_STEP_SEND_END
			{
                frame.identifier = BMS_OTA_END;   // 结束帧
                frame.data[0] = 1; // 表示升级成功
                ESP_LOGW(TAG, "BAT OTA success, pct first");
//                set_ota_pct_info(model,target_addr, image_type, 0, 1, 100, OTA_ERR_NORMAL);
                ESP_LOGI(TAG, "index:%d, addr:%d, Image file: %d, pct: %3d%% (retry: %d)", target_addr, bms_addr,image_type, 100, retry_count);
//                vTaskDelay(pdMS_TO_TICKS(1000)); // 模拟一个虚假进度，让APP可以直接退出进度条
                set_ota_pct_info(model,target_addr, image_type, 0, 2, 100, OTA_ERR_NORMAL); 
//                set_image_invaild();
//                vTaskDelay(pdMS_TO_TICKS(2000)); // 模拟一个虚假进度，让APP可以直接退出进度条
            }

            frame.identifier |= (bms_addr << 8);
			frame.data_length_code = 8; // 避免被数据帧修改

			
			canid.all = frame.identifier;
            memcpy(can_ctrl.data, frame.data, 8);
			
			/* 发送单帧CAN数据 */
			if (1 == CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t))) //ok
			{
				step++;
				Can_rx_timeout = 3000; // 等待应答超时3s
				ESP_LOGI(TAG, "index:%d, addr: %d, next step: %d, file: %d, (retry: %d)", target_addr, bms_addr, step, image_type, retry_count);
				reals.current_ota_info.soft_version = IotSetData.dev_info_t.bms_ota_info.version;
			}	
				
            retry_count++;
        break;

        case ALPHA_OTA_STEP_WAIT_ACK_READY: //1 wait ready ack
        case ALPHA_OTA_STEP_WAIT_ACK_START: //3 wait start ack
        case ALPHA_OTA_STEP_WAIT_ACK_END: //8 wait end ack

            // 应答帧内容必须和发送帧内容完全一致
            if ((Rx_can_frame ) 
               && (Rx_can_frame->identifier == frame.identifier) 
               && (0 == memcmp(Rx_can_frame->data, frame.data, 8) ) )//rx== old tx, same 
            {
				step++;
				retry_count = 0;
                ESP_LOGI(TAG, "wait ack success");
            } 
			else if(0 == Can_rx_timeout)
		   	{
                step--; // ack timeout，重发
                ESP_LOGI(TAG, "wait ack timeout");
		    }
            // if (step == 8) { // B300\B300S\B230电池包不应答
                // pct_value = (image_total - image_remaining) * 100 / image_total;
                // set_ota_pct_info(target_addr, image_type, 0, 1, pct_value, OTA_ERR_NORMAL);
                // ESP_LOGI(TAG, "index:%d, addr:%d, Image file: %d, pct: %3d%% (retry: %d)", target_addr, bms_addr,image_type, pct_value, retry_count);
                // vTaskDelay(pdMS_TO_TICKS(1000)); // 模拟一个虚假进度，让APP可以直接退出进度条
                // set_ota_pct_info(target_addr, image_type, 0, 2, 100, OTA_ERR_NORMAL); 
                // set_image_invaild();
            // }

        break;

        case ALPHA_OTA_STEP_READ_IMAGE_DATA: //4 read image data
            if (image_remaining == 0) { 
                retry_count = 0; 
                step = 7;
                ESP_LOGI(TAG,"image send complete");
				IotSetData.dev_info_t.bms_ota_info.ota_state = OTA_STATE_COMPLETE;
                reals.SetDataWrFlag.sBit.update_status = 1;
				
				set_ota_pct_info(model,target_addr, image_type, 1, 1, 100, OTA_ERR_NORMAL);
                break; // 文件发送完成
            } 

            frame_len =  (image_remaining > 6) ? 6 : image_remaining;
            frame.identifier = BMS_OTA_DATA;
            frame.identifier |= (bms_addr << 8);
            frame.data_length_code = frame_len + 2;
            frame.data[0] = pack_num / MAX_PACK_NUM;
            frame.data[1] = pack_num % MAX_PACK_NUM; // 每128包 pack序号需要归0
            if(pack_num % MAX_PACK_NUM == 0) 
			{
                int len = (image_remaining > 768) ? 768 : image_remaining; // 每次读取6字节

                ESP_LOGI(TAG, "image file read len: %d", len);
                ESP_LOGI(TAG, "image file read offset: %lu", image_offset);
                if (iot_image_read(image_offset, temp_data, len) != ESP_OK) 
				{
                    ESP_LOGE(TAG, "image file read failed");
                    step = 0xFF;
                    set_ota_pct_info(model,target_addr, image_type, 0, 1, pct_value, OTA_ERR_FLASH_READ);
                    break;
                } 
				else
				{
                }
            }
            memcpy(frame.data + 2, temp_data + (pack_num % MAX_PACK_NUM) * 6, 6);
			
            step++; 
            retry_count = 0; 
        /* fall through */ // 这里不需要break;

        case ALPHA_OTA_STEP_SEND_DATA: //5 send data
            if (retry_count > 8) { // 重发次数达到上线
                ESP_LOGE(TAG, "Step: %d, Retry Max Exit OTA", step);
                retry_count = 0;
                step = 0xFF;
                set_ota_pct_info(model,target_addr, image_type, 0, 1, pct_value, OTA_ERR_NO_RESP);
                break;
            }
			
			canid.all = frame.identifier;
			memcpy(can_ctrl.data, frame.data, 8);			
			/* 发送单帧CAN数据 */
			if (1 == CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t))) //ok
			{
				Can_rx_timeout = 5000; // 等待应答超时5s
				step++;
			}			
            retry_count++; // 发送次数增加
        break;
    
        case ALPHA_OTA_STEP_WAIT_IMAGE_DATA: //6 wait data ack
            // 应答帧内容必须和发送帧内容完全一致
            if ((Rx_can_frame ) 
               && (Rx_can_frame->identifier == frame.identifier) 
               && (0 == memcmp(Rx_can_frame->data, frame.data, frame_len) ) )//rx== old tx, same 
            {// 发了多少字节数据检查多少字节
				pack_num++; // 发送序号增加
				image_offset += (frame.data_length_code - 2); // 数据存储地址增加
				image_remaining = (image_remaining < (frame.data_length_code - 2)) ? 0 : (image_remaining - (frame.data_length_code - 2)); // 剩余字节减少
				pct_value = (image_total - image_remaining) * 100 / (image_total + 1);
				set_ota_pct_info(model,target_addr, image_type, 1, 1, pct_value, OTA_ERR_NORMAL);
				if (last_pct != pct_value) {
					last_pct = pct_value;
					ESP_LOGI(TAG, "index:%d, addr:%d, Image file: %d, pct: %3d%% (retry: %d)", target_addr, bms_addr, image_type, pct_value, retry_count);
				}
				step = ALPHA_OTA_STEP_READ_IMAGE_DATA; //4 进入step4 读取下个段数据
            } 
			else if(0 == Can_rx_timeout)
		   	{
                step--; // ack timeout，重发
		    }
        break;

        default: bms_ota_doing = false; 
		ESP_LOGE(TAG, "Step: %d, BMS Exit OTA", step);
		break;
    }
}


