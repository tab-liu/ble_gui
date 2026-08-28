/**@file iot_4g_handle.c
* @brief 4g模块应用层
* @details 此文件包含实现4G模块逻辑层的函数和结构定义它处理与4G通信模块的交互，包括初始化、数据传输和状态管理等。
* @author zhongdongming
* @date 2023-12-12
* @version V1.0
* @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
********************************************************************************
* @attention
* 硬件平台: ESP32
* SDK 版本: ESP_IDF_V4.4.6
* @par 修改日志:
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2023/12/12 <td>1.0 <td>zhongdongming <td>创建初始版本
* </table>
********************************************************************************
*/

#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Cellular_4g_handle.h"
#include "Cellular_4g_at.h"
//#include "iot_uart.h"
#include "esp_log.h"
#include "iot_mqtt.h"
#include "iot_period_task.h"
#include "filesystem.h"
#include "uart_device_process.h"


static const char *TAG = "[4G_HANDLE]";

#define WEEK_SECONDS            (604800000u)        // 24 * 60 * 60 * 7 * 1000ms






enum {
    PUB_PERIOD = 1,
    PUB_REALTIME = 2,
    PUB_PHONE = 3,
};


static const char *mqtt_step_str[] = {
    "RESET_4G_MODULE",
    "INIT_4G_MODULE",
    "GET_IP_4G",
    "RESOLVE_DNS_4G",
    "TCP_CONNECT_4G",
    "TCP_SYNC_4G",
    "MQTT_START_4G",
    "MQTT_RECV_4G",
    "MQTT_STBY_4G",
};

typedef struct {
    // bool vaild;
	char  dev_type[12];   		// ASCII 机型
	uint64_t dev_sn;      		// 设备唯一识别码
	uint8_t index;				// 索引
} inv_info_t;

typedef struct {
	uint8_t need_bind;
	uint8_t dev_nums;
	inv_info_t invs_list[2]; //
} bind_dev_list_t;

//-------------------- 函数列表 --------------------------------------------
static void mqtt_client_publish_inv_data(char *send_topic, uint8_t addr, uint16_t cycle);
//-------------------- 变量列表 --------------------------------------------
uint8_t Cellular4G_mqtt_step = INIT_4G_MODULE;//通过4G模块 AT指令方式网络交互的状态机


void create_iot_4g_task(void) 
{
    
//    if (xTaskCreate(iot_4g_task, "iot-4g-task",   1024 * 3, NULL, 4, NULL) != pdPASS) {  /* 4G任务 */
//        ESP_LOGE(TAG, "4g_uart_test_task create failed");
//    }
//    if (xTaskCreate(mqtt_client_ann_task, "4g mqtt ann", 1024 * 4, NULL, 3, NULL) != pdPASS) {
//        ESP_LOGE(TAG, "mqtt_client_ann_task create failed");
//    }
//    esp_log_level_set(TAG, ESP_LOG_NONE);
}

void set_mqtt_new_host(const char could_dns[128]) {
    memcpy(login_info.raw_url, could_dns, sizeof(login_info.raw_url)); // mqtt://mqtt.eclipse.org:1884
    Cellular4G_mqtt_step = GET_IP_4G; // MQTT 逻辑重新开始
} 

void set_mqtt_new_iot(const char new_type[12], uint64_t new_sn, uint64_t safetycode) {
    memcpy(login_info.iot_type, new_type, 12); // 设备的型号最长12个字符
    login_info.iot_sn = new_sn;
    login_info.safetyCode = safetycode;
    Cellular4G_mqtt_step = GET_IP_4G; // MQTT 逻辑重新开始
}

void set_mqtt_new_device(const char new_type[12], uint64_t new_sn) {
    memcpy(login_info.dev_type, new_type, 12); // 设备的型号最长12个字符
    login_info.dev_sn = new_sn;
    Cellular4G_mqtt_step = GET_IP_4G; // MQTT 逻辑重新开始
}

void stop_4g_task(void) {
    Cellular4G_mqtt_step = MQTT_STBY_4G;
    stop_4g_module();
}

void start_4g_task(void) {
    Cellular4G_mqtt_step = RESET_4G_MODULE;
    start_4g_module();
}

/*------------------------------------------------------------------------
*@Function： iot_4g_task
4G模块登录状态机

-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void iot_4g_task(void *pvParameters)
{
    uint8_t fail_count = 0;
    Cellular4G_mqtt_step = INIT_4G_MODULE;
    ESP_LOGI(TAG, "wait 7s for 4g module init..."); 
    vTaskDelay(pdMS_TO_TICKS(7000));    // 上电后4g模块需要等待7s


	
    while(1) 
    {   
		vTaskDelay(pdMS_TO_TICKS(1000));//windy add debug
    
        switch (Cellular4G_mqtt_step) 
        {
        case RESET_4G_MODULE:
            iot_4g_reset();
            ESP_LOGI(TAG, "wait 7s for 4g module reset...");
            vTaskDelay(pdMS_TO_TICKS(7000));
            Cellular4G_mqtt_step = INIT_4G_MODULE;
        break;

        case INIT_4G_MODULE:
            if(Init4GModule() == ESP_OK){ // 模块初始化
                Cellular4G_mqtt_step = GET_IP_4G;
                vTaskDelay(pdMS_TO_TICKS(1000));
            } else {
                ESP_LOGE(TAG, "4g module init failed");
                Cellular4G_mqtt_step = RESET_4G_MODULE;
            }
        break;

        case GET_IP_4G:  // 获取IP
            mqtt_client_close(); // 先关闭后打开
            vTaskDelay(pdMS_TO_TICKS(500));
            if(get_local_ip() == ESP_OK){
                fail_count = 0;
                Cellular4G_mqtt_step = RESOLVE_DNS_4G; 
//			At_4G_Get_In_PPP_Mode();
			if(!At_4G_Get_In_PPP_Mode()) //fail
			{
				ESP_LOGE(TAG, "At_4G_Get_In_PPP_Mode fail");
				
				Cellular4G_mqtt_step = INIT_4G_MODULE;
				
				continue;
			}
			else
			{
//				app_uart2_Deinit();
				ESP_LOGI(TAG, "windy after app_uart2_Deinit   ");
			}



			while(1) 
			{	
				vTaskDelay(pdMS_TO_TICKS(1000));//windy add debug
			}

				
            } else {
                fail_count++;
                if(fail_count >= 100){
                    Cellular4G_mqtt_step = RESET_4G_MODULE; // 一直无法获取IP重启模块
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        break;

        case RESOLVE_DNS_4G: // DNS解析主机地址
            if(mqtt_client_dns(&login_info) == ESP_OK){
                Cellular4G_mqtt_step = TCP_CONNECT_4G;
            } else {
                Cellular4G_mqtt_step = GET_IP_4G;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        break;

        case TCP_CONNECT_4G: // 创建TCP连接后台
            if(mqtt_client_create_tcp(&login_info) == ESP_OK){
                Cellular4G_mqtt_step = TCP_SYNC_4G;
            } else {
                Cellular4G_mqtt_step = RESOLVE_DNS_4G;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        break;

        case TCP_SYNC_4G: // 获取后台时间
            login_info.now_time = mqtt_client_sync_time(&login_info);
            if (login_info.now_time > 0) {
                Cellular4G_mqtt_step = MQTT_START_4G;
            } else {
                Cellular4G_mqtt_step = GET_IP_4G;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        break;
        
        case MQTT_START_4G:    // 启动MQTT
            if (mqtt_client_start(&login_info) == ESP_OK) 
			{
                Cellular4G_mqtt_step = MQTT_RECV_4G;
//windy dis debug                tcp_bind_dev();     // 每次登录MQTT都需要绑定设备
                vTaskDelay(pdMS_TO_TICKS(1000));
            } 
			else 
			{
                Cellular4G_mqtt_step = GET_IP_4G; // 重新开始
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        break;

        case MQTT_RECV_4G: // MQTT启动完成，准备接收数据
            if(mqtt_client_data_handle(&login_info) == ESP_FAIL) 
			{
                Cellular4G_mqtt_step = GET_IP_4G;
                vTaskDelay(pdMS_TO_TICKS(1000));
            } 
        break;

        case MQTT_STBY_4G: 
			vTaskDelay(pdMS_TO_TICKS(1000)); 
			break;
        
        default: 
            ESP_LOGI(TAG, "iot mqtt client step error (%d)", Cellular4G_mqtt_step); 
            Cellular4G_mqtt_step = RESET_4G_MODULE;
        break;
        }
    }
    vTaskDelete(NULL); 
}

//windy debug
//void Cellular_4G_Test(void )
#if 0
void iot_4g_task(void *pvParameters)
{
    uint8_t fail_count = 0;
	uint8_t buff[100]={"windy ABC EFD"};
	uint8_t Rxdata_ptr[100];
	uint16_t total_len=20;
	uint8_t len2;

	
    Cellular4G_mqtt_step = INIT_4G_MODULE;
    ESP_LOGI(TAG, "wait 7s for 4g module init..."); 
    vTaskDelay(pdMS_TO_TICKS(7000));    // 上电后4g模块需要等待7s
    Init4GModule();
	
	vTaskDelay(pdMS_TO_TICKS(3000));//windy add debug
	get_local_ip();
	
	vTaskDelay(pdMS_TO_TICKS(3000));//windy add debug
//	mqtt_client_create_tcp(&login_info)
		//120.76.100.197
    if(Send4GCmd("AT+MIPSEND?","+MIPSEND: 0", 1, AT_WAIT_TIME)){ // 没有正在使用的socket
        snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MIPOPEN=1,,\"%d.%d.%d.%d\",%d,0",120, 
            76, 100, 197, 10002); //  建立TCP连接
        if(!Send4GCmd(ATSendBuff,"+MIPOPEN: 1,1",AT_RETRY_TIMES, 60000)){ // 需要等待60s，失败重新解析DNS
            ESP_LOGE(TAG, "TCP connect failed");
        } 
		else 
		{
        }
    }    
    while(1) 
    {   
		vTaskDelay(pdMS_TO_TICKS(10000));//windy add debug


	vTaskDelay(pdMS_TO_TICKS(3000));//windy add debug
	AT_TCP_Send_Data(buff, total_len,Rxdata_ptr );
	vTaskDelay(pdMS_TO_TICKS(3000));//windy add debug
	
	AT_TCP_Send_Data(buff, total_len,Rxdata_ptr );
	vTaskDelay(pdMS_TO_TICKS(3000));//windy add debug

	AT_TCP_Send_Data(buff, total_len,Rxdata_ptr );
	vTaskDelay(pdMS_TO_TICKS(3000));//windy add debug

    }
}
#endif

/*------------------------------------------------------------------------
*@Function： mqtt_client_4G_Module_task
4G模块  MQTTT 有效信息周期上报

-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return         
*/
void mqtt_client_4G_Module_task(void * pvParameters) 
{
    iot_4g_state_t iot_4g_mqtt_state;
    uint16_t ann_cycle = 0;
    uint16_t time_count = 0;
    vTaskDelay(pdMS_TO_TICKS(10000)); 
    while (1)
    {   
        iot_4g_mqtt_state = get_iot_4g_state();
        if (iot_4g_mqtt_state.bit.mqtt_login_fail == 1) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            // ESP_LOGW(TAG, "ann note: mqtt client disconnect");
            continue;
        }
        
//        tcp_bind_dev(); // 检查是否有设备需要绑定

//        if (time_count % 2 == 0) {
//            at_query_rssi(); // 每2s查询一次信号强度
//            vTaskDelay(pdMS_TO_TICKS(1000));
//        }
//
//        if (time_count % 60 == 0) {
//            at_query_gps(); // 每60s查询一次GPS
//            vTaskDelay(pdMS_TO_TICKS(1000));
//        }

        if (time_count % 300 == 0) 
		{ // 5min
            if (++ann_cycle >= 0xFFFF) 
			{ 
                ann_cycle = 1; // 满了从1开始
            }
            // 按设备modbus地址上报数据 eg: 0x01
			if(MQTT_RECV_4G == Cellular4G_mqtt_step)
			{
				mqtt_client_publish_inv_data(login_info.pub_topic, 0x01, ann_cycle); 
			}
        }

        time_count++;
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

uint16_t LookupAnnRegMaxNum(uint16_t addr){
    // TODO 根据modbus地址查找寄存器数量
    return 0;
}

int ModbusMasterAnnDataPack(uint16_t addr, uint16_t reg_addr, uint16_t reg_num, uint8_t *outdata) {
    // TODO填充modbus数据到outdata中 功能码0x10
    return 0;
}

static void mqtt_client_publish_inv_data(char *send_topic, uint8_t addr, uint16_t cycle) 
{

    uint8_t buf[610];
    uint16_t reg_addr;
    uint16_t reg_num = 0;
    uint16_t max_segment = 11; // 11段modbus数据
    ESP_LOGI(TAG, "mqtt publish inv data, id: %d", addr);
    for (uint8_t i = 0; i < max_segment; i++) {
        // 按需求上报数据到服务器
        vTaskDelay(pdMS_TO_TICKS(1000));
        switch (i) {
            case 0:  reg_addr = 000;   break;
            case 1:  reg_addr = 100;   break;
            case 2:  reg_addr = 1100;  break;
            case 3:  reg_addr = 1200;  break;
            case 4:  reg_addr = 1300;  break;
            case 5:  reg_addr = 1500;  break;
            case 6:  reg_addr = 2000;  break;
            case 7:  reg_addr = 2200;  break;
            case 8:  reg_addr = 2400;  break;
            case 9:  reg_addr = 2500;  break;
            case 10: reg_addr = 5000;  break;
            default: return;
        }
        
        uint16_t len = 0;
        len += mqtt_data_head(0x01, buf, PUB_PERIOD, cycle, max_segment, (i + 1)); // 
        reg_num = LookupAnnRegMaxNum(reg_addr);
        len += ModbusMasterAnnDataPack(addr, reg_addr, reg_num, &buf[MQTT_FORMAT_HEAD]); // 封装modbus数据
        if (len > MQTT_FORMAT_HEAD) {
            AT_MQTT_Send_Public_Data(send_topic, buf, len);
            //esp_log_buffer_hex(TAG, buf, len);
        }
    }
}

void Cellular_init(void)
{


	set_mqtt_new_host(SetData.dev_info_t.could_dns); // 设置云端服务器登录地址
	set_mqtt_new_device(iot_factory.iot_type, iot_factory.iot_sn); // 设置订阅MQTT主题
	set_mqtt_new_iot(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code); // 设置IOT设备登陆信息

    ESP_LOGI(TAG, " HUANGJI FFF type:%s -%d -%d",iot_factory.iot_type,  (unsigned int)iot_factory.iot_sn, (unsigned int)iot_factory.safe_code);
	ESP_LOGI(TAG, " HUANGJI FFF type:%s -%d -%d",SetData.dev_info_t.Parallel_dev_type,  (unsigned int)SetData.dev_info_t.Parallel_dev_sn, (unsigned int)SetData.dev_info_t.Parallel_dev_safetycode);
}



