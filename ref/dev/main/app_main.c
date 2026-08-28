/**
  ******************************************************************************
  * @file      app_main.c
  * @version   1.0
  * @author    wendan
  * @date      2024/7/5
  * @brief     系统初始化
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/5   <td>1.0     <td>wendan   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
//#include "drv_gd25q128.h"

#include "app_uart.h"
#include "do_di_ctrl.h"
#include "Cellular_4g_handle.h"
#include "ethernet_init.h"
//#include "ext_flash_littlefs.h"
#include "littlefs.h"

#include "can_protocol.h"
#include "iot_period_task.h"
#include "filesystem.h"
#include "ext_rtc.h"
#include "app_time.h"
#include "iot_wifi_init.h"
#include "iot_ble_encrypt.h"
#include "modbus_protocol.h"
#include "dev_discovery.h"
#include "dev_data_record.h"
#include "mesh_api.h"
#include "can_pack.h"
#include "DisasterWarn.h"
#include "iot_box_task.h"
#include "iot_sleep.h"
#include "modbus_tlv.h"

#define TAG "app-main"

// 北京时间:  2023-01-01 08:00:00 -> 1672531200
#define SYSTEM_START_TIME       1672506000  // 2023-01-01 01:00:00



extern void udt_configuration(void);

/*------------------------------------------------------------------------------
 Function: app_log_level_set
 -----------------------------------------------------------------------------*/
/**
  * @brief      调试信息打印等级设置
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void app_log_level_set(void)
{

#ifdef MASS_PRODUCTION_CONFIG_ENABLE
	/* 关闭所有日志输出，避免对上位机接收造成影响 */
	esp_log_level_set("*", ESP_LOG_NONE);
#else

//设置log打印等级,详细程度<=此等级的打印显示
	esp_log_level_set("[LCD_MOD]", ESP_LOG_WARN);
	esp_log_level_set("gpio", ESP_LOG_WARN);
	esp_log_level_set("system_api", ESP_LOG_WARN);
	esp_log_level_set("spi_flash", ESP_LOG_WARN);
	esp_log_level_set("uart", ESP_LOG_WARN);
	esp_log_level_set("[MQTTC]",ESP_LOG_DEBUG );//ESP_LOGE ESP_LOG_WARN ESP_LOG_INFO ESP_LOG_DEBUG
	esp_log_level_set("[MQTTC2]",ESP_LOG_NONE );// 
	esp_log_level_set("[APP_UART0]",ESP_LOG_NONE );//UART2_TAG "app_uart2"
	esp_log_level_set("[APP_UART1]",ESP_LOG_NONE );//
	esp_log_level_set("[APP_UART2]",ESP_LOG_NONE );//	
	// esp_log_level_set("[CANBUS]",ESP_LOG_WARN );//							
	esp_log_level_set("[4G_AT_CMD]",ESP_LOG_NONE );
	esp_log_level_set("[iot_period_task]",ESP_LOG_DEBUG );
	esp_log_level_set("[4G_HANDLE]",ESP_LOG_NONE );
	esp_log_level_set("[pppos_example]",ESP_LOG_NONE );
	
	esp_log_level_set("command_lib",ESP_LOG_NONE );
	esp_log_level_set("[MODBUS]",ESP_LOG_NONE );
	esp_log_level_set("[partition]",ESP_LOG_NONE );
	esp_log_level_set("[app_ble]",ESP_LOG_DEBUG );//ESP_LOG_DEBUG ESP_LOG_ERROR
//	esp_log_level_set("[iot_ble_encrypt]",ESP_LOG_DEBUG );//ESP_LOG_DEBUG ESP_LOG_NONE
	esp_log_level_set("[ble_dev]",ESP_LOG_NONE );//
	esp_log_level_set("[drv_nimble]",ESP_LOG_NONE );//
	esp_log_level_set("[gatts_svr]",ESP_LOG_NONE );//
	
	esp_log_level_set("[ETH_INIT]",ESP_LOG_NONE );
	esp_log_level_set("[ETH_APP]",ESP_LOG_NONE );
		
	esp_log_level_set("[CAN_TRANSMIT]",ESP_LOG_NONE );
	esp_log_level_set("[CAN_INIT]", ESP_LOG_NONE);
	esp_log_level_set("[CAN_PROTOCOL]", ESP_LOG_DEBUG);//ESP_LOG_DEBUG
	esp_log_level_set("[CAN]", ESP_LOG_NONE);
	esp_log_level_set("[CAN_DATA]", ESP_LOG_DEBUG);
	esp_log_level_set("[data_summary]", ESP_LOG_NONE);
	esp_log_level_set("[LL_PARAM]", ESP_LOG_NONE);
	
	esp_log_level_set("[EST_EEPROM]",ESP_LOG_NONE );
	esp_log_level_set("[EXT_FLASH]",ESP_LOG_NONE );//SPI NOR flash 
	esp_log_level_set("[RTC-8563]",ESP_LOG_NONE );//
	esp_log_level_set("[APP_TIME]",ESP_LOG_NONE );//
	esp_log_level_set("[UART_DEV_PROCESS]",ESP_LOG_NONE );//
	esp_log_level_set("[md_protocol]",ESP_LOG_DEBUG);//
	esp_log_level_set("[WIFI]",ESP_LOG_NONE );//
	esp_log_level_set("[modbus_slave_data]",ESP_LOG_NONE );//
	
	esp_log_level_set("[GD_DRV]",ESP_LOG_NONE );//
	esp_log_level_set("[HTTPS]",ESP_LOG_DEBUG );//
    esp_log_level_set("[SUB1G_OTA]", ESP_LOG_NONE);
    esp_log_level_set("[XMODEM]", ESP_LOG_NONE);

    esp_log_level_set("[udp_multicast]", ESP_LOG_NONE);
    esp_log_level_set("[UDT_TRANSFER]", ESP_LOG_NONE);
    esp_log_level_set("[UDT_EXP_IOT]", ESP_LOG_NONE);
    esp_log_level_set("[DEV_DATA_RECORD]", ESP_LOG_DEBUG);
	esp_log_level_set("[DEV_DISCOVERY]", ESP_LOG_NONE);

    esp_log_level_set("[mcast-ipv4:modbus]", ESP_LOG_NONE);
    esp_log_level_set("[mcast-ipv4]", ESP_LOG_NONE);

    esp_log_level_set("[APP_PARAM]", ESP_LOG_DEBUG);
    esp_log_level_set("[multicast]", ESP_LOG_NONE);
    esp_log_level_set("[MESH]", ESP_LOG_NONE);
	
	esp_log_level_set("[ems_ctrl]", ESP_LOG_NONE);
	esp_log_level_set("[CAN_PACK]", ESP_LOG_NONE);
	esp_log_level_set("[Wl_mesh]", ESP_LOG_NONE);
	esp_log_level_set("[XMD_SENDER]", ESP_LOG_NONE);
	esp_log_level_set("[HTTP_CLIENT]", ESP_LOG_DEBUG);
	esp_log_level_set("[CAN_OTA]", ESP_LOG_DEBUG);
	esp_log_level_set("[DisasterWarn]",ESP_LOG_NONE);
	esp_log_level_set("[JSONs]",ESP_LOG_NONE);
	esp_log_level_set("[CAN_D400S]",ESP_LOG_NONE);
	esp_log_level_set("[UDT_PORT]",ESP_LOG_NONE);
	esp_log_level_set("[IOT_BOX_TASK]",ESP_LOG_NONE);
	esp_log_level_set("[OTA-RECV]",ESP_LOG_NONE);
    esp_log_level_set("[WebServer]",ESP_LOG_NONE);
    esp_log_level_set("[ble_adv]",ESP_LOG_NONE);
    esp_log_level_set("[TCP_SERVER]",ESP_LOG_NONE);
    esp_log_level_set("[TCPS_SERVER]",ESP_LOG_NONE);
    esp_log_level_set("NimBLE",ESP_LOG_NONE);

    esp_log_level_set("[TCP_CLIENT]",ESP_LOG_NONE);
    esp_log_level_set("[TCPS_CLIENT]",ESP_LOG_NONE);
#endif
	
	
}


void app_main(void)
{
	ESP_LOGI(TAG, "app_main start");
    /*设置打印等级*/
    app_log_level_set();

	/*初始化主任务变量*/
 	memset(&reals, 0, sizeof(reals)); 
	memset(reals.fast_to_Can_poll_index,-1,sizeof(reals.fast_to_Can_poll_index));

 	/*初始化Sub1g子设备列表序号*/
//    for (uint8_t j = 0; j < NET_SUB1G_MAX_POINT; j++)
//    {
//		reals.Subseq_index[j] =j;
//        reals.Pseq_index[j] =j;
//    }	
    
    /*初始化网络设备列表序号*/
    for (uint8_t j = 0; j < NET_WIFI_MAX_POINT; j++)
    {
		reals.Topseq_index[j] =j;
    }


	for(uint8_t i = 0; i < PLUG_MAX_NUM; i++)// 
	{
		reals.Acphase_seq_S1[i] = 1;
	}

    /*初始化网络状态变量*/
	memset(&wifi_info, 0, sizeof(wifi_info_t));
	memset(&_4g_info, 0, sizeof(wifi_info_t)); 	
	memset(&eth_info, 0, sizeof(wifi_info_t)); 

    /*网络设备状态缓存初始化*/
//	Sub_Net_Point_Clean();
	iot_Inv_data_init();
    iot_can_data_init();
	iot_modbus_data_init();
	Modbus_beta_reg_table_register_init();
	pack_infos_init();

	
    /*初始化绑定帧对列*/
    Modbus_21000_semaphore_init();
#ifdef CONFIG_MODBUS_REG_TLV_ENABLE
    modbus_tlv_queue_init(0);
#endif
	

/* 挂载文件系统*/
	mount_littlefs(); 
	app_main_timer_init();
	initialize_nvs();	

	system_time_init();											// 初始化系统时间

    /*休眠相关初始化*/
    iot_sleep_config_init();

    /*智能TOU控制模块初始化*/
    Smart_Tou_Ctrl_Module_Init();

//	example_flash_init();
//    while(1)
//    {   
//        vTaskDelay(pdMS_TO_TICKS(1000));
//   	}

	
	 Read_Data_From_NVS();

	/*启动标定串口任务，标定完成或已标定会自动关闭*/
    fcty_cal_app();	

	set_power_flag();											// 设置电源标志

     GetInitDataFromLittlefs();//须在获取littlefs数据（存储时间戳）后才可执行
	 Copy_Data_From_Set_To_Modbus();
	 iot_sleep_state_restore_from_setdata();
//	 reals.iot_sn_sum[0] =iot_factory.iot_sn;

#ifdef  DEBUG_FORCE_WIFI_MESH_SLAVE
	
		reals.ModbusCmdFlag.sBit.wifi_sta_password = 1;
		reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 1;
		memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid, MESH_AP_SSID, sizeof(MESH_AP_SSID));//MESH_AP_SSID DEF_SSID
		memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password, DEF_WIFI_STA_PASSWD, sizeof(DEF_WIFI_STA_PASSWD));
	
#endif



    encrypt_data_init();		

#ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION

    /*通用数据通道初始化*/
	udt_configuration();

#endif	

    /*延迟2s等待RTC稳定*/
    vTaskDelay(pdMS_TO_TICKS(2000));

    /*所有工作任务初始化*/
	sum_task_init();//
	
    /*1秒延迟，等待前面任务运行正常*/
	vTaskDelay(pdMS_TO_TICKS(1000)); 							

    /*串口任务初始化*/
    app_uart_init();

    /*SNTP初始化*/
	app_sntp_init();// 需要在WIFI任务之后调用


	while(1)
	{
		vTaskDelay(pdMS_TO_TICKS(200));
		//led_Start_DTU();
	}	
}

