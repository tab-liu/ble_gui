#include "iot_period_task.h"
//#include "kvdb_eep.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_bit_defs.h"
#include <string.h>
#include <stdio.h>
#include "math.h"
#include "esp_wifi_types.h"
#include "iot_wifi_init.h"
#include "webserver.h"

#include "freertos/FreeRTOS.h"
#include "mbedtls/rsa.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"
#include "mbedtls/pk.h"
#include "mbedtls/psa_util.h"

#include "filesystem.h"
#include "Cellular_4g_handle.h"
#include "iot_mqtt.h"
#include "server2internet.h"
#include "do_di_ctrl.h"
#include "app_time.h"
#include "esp_random.h"
#include "ll_param_def.h"
#include "ll_param.h"
#include "uart_device_process.h"
#include "do_di_ctrl.h"
#include "http_client.h"
#include "modbus_slave.h"
#include "dev_discovery.h"
#include "udt_port.h"
#include "udp_multicast.h"
#include "can_protocol.h"
#include "can_transmit.h"

#include "comm_define.h"
#include "ble_adv.h"
#include "drv_nimble.h"
#include "ble_dev.h"
#include "app_ble.h"
#include "esp_netif.h"
#include "dev_data_record.h"
#include "ems_ctrl.h"
#include "modbus_tcp_slave.h"
#include "mesh_api.h"   //mesh_network
#include "iot_ota.h"
#include "wl_mesh.h"
#include "esp_bt.h"
#include "DisasterWarn.h"
#include "can_pack.h"
#include "iot_box_task.h"
#include "wlcc_process.h"
#include "general_build_get.h"
#include "iot_sleep.h"
#include "smart_tou_ctrl_v1.h"
#include "modbus_tlv.h"

#define MAIN_TIMER_INTERVAL     10//1000

//#define TAG "[MAIN]"
//#define TAG		"[iot_period_task]"
static const char *TAG = "[iot_period_task]";





//windy debug EBOX2344000010599
//factory_struct iot_factory = {
//    .iot_type = IOT_TYPE_STR,
//    .iot_sn = 2344000010599,//88889999,//100000000001,   //2022022609009  2022021605008
//    .safe_code = 123456,
//};//出厂统一默认值



//EXT_RAM_BSS_ATTR dev_info_t  SetData.dev_info_t = DEV_DEFAULT_VALUE;
EXT_RAM_BSS_ATTR RealS_STRUCT reals;
EXT_RAM_BSS_ATTR SetData_TypeDef SetData;//IOT自身,初始值自身代码初始化
EXT_RAM_BSS_ATTR SetData_Can_TypeDef SetData_Can;//下级can INV数据备份，初始值必须来自下级CAN

EXT_RAM_BSS_ATTR baseKWH_energy_t_u32_2   statistic;///累计统计量的实时缓存，不存储，
EXT_RAM_BSS_ATTR baseKWH_energy_t_u32_2   statistic_delta;///



extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];//test

QueueHandle_t can_cmd_queue = NULL;//从BLE/WIFI到CAN的透彻转发，队列
QueueHandle_t can_recv_cmd_queue = NULL; // CAN接收指令队列;接收来自 CAN总线 的针对自己的CAN rx信息tbd，如上位机通过CAN查询历史记录数据通道


extern void iot_mqtt_announce(void);//windy debu


/*
windy add
*/
void dump_buf_global(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}


/*------------------------------------------------------------------------------
 Function: app_sys_debug_info
 -----------------------------------------------------------------------------*/
/**
  * @brief      调试打印
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
 void app_sys_debug_info(void)
{
      ESP_LOGI(TAG, "RTC: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,(uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
        (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,(uint16_t)reals.rtc_time.sec);   
}

void calculate_md5(uint8_t *value, uint16_t value_size, uint8_t *out_md5)
{
    if (value == NULL || out_md5 == NULL || value_size == 0)
    {
        ESP_LOGE(TAG, "Invalid parameters for MD5 calculation");
        return;
    }

    mbedtls_md5_context md5_ctx;
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);

    mbedtls_md5_update(&md5_ctx, value, value_size);
    mbedtls_md5_finish(&md5_ctx, out_md5);
    mbedtls_md5_free(&md5_ctx);
}

/*
默认量产参数
*/
 void Default_iot_data_init(void) 
 {
	 SetData.dev_info_t.valid_iot =IOT1EPROM_READY_FLAG;
	 SetData.dev_info_t.valid_other =IOT2EPROM_READY_FLAG;
	 
	 SetData.dev_info_t.Wr_flag.word =0;
//	 SetData.dev_info_t.dev_sn = MASS_PRODUCTION_DEFAULT_DEV_SN;
	 
	 SetData.dev_info_t.on_off.bit.wifi_enable = 1;//1;
	 SetData.dev_info_t.on_off.bit._4g_enable = 2;//2;
	 SetData.dev_info_t.on_off.bit.Eth_enable = 2;//2;
	 
    //  SetData.dev_info_t.on_off.bit.wifi_sta_enable = 1;
	 SetData.dev_info_t.on_off.bit.wifi_ap_enable = 1;
	 SetData.dev_info_t.on_off.bit.ble_enable = 1;
	 SetData.dev_info_t.on_off.bit.ble_mash_enable = 0;
	 SetData.dev_info_t.on_off.bit.gps_enable = 1;
	 SetData.dev_info_t.on_off.bit.ble_adv_enable = 1;
	 
	 

	 SetData.dev_info_t.wifi_sta_auth = WIFI_AUTH_WPA2_PSK;
	 SetData.dev_info_t.sta_enable.all = 0x1A5; /* bit0~7=0xA5高32字节有效; bit8=1支持开放式WiFi */
     SetData.dev_info_t.data_report_cycle = 3600;//1h
	 SetData.dev_info_t.thunder_ctrl.thunder_enable = 1;
	 memset(&SetData.dev_info_t.sleep_ctx, 0, sizeof(SetData.dev_info_t.sleep_ctx));

	 
	 memcpy(SetData.dev_info_t.wifi_sta_ssid, DEF_SSID, sizeof(DEF_SSID));
	 memcpy(SetData.dev_info_t.wifi_sta_password, DEF_WIFI_STA_PASSWD, sizeof(DEF_WIFI_STA_PASSWD));
	 memcpy(SetData.dev_info_t.could_dns, BLUETTI_DNS, sizeof(BLUETTI_DNS));
#if 0
    //TODO: 待修改，AP SSID和密码默认值不写flash
    char wifi_ap_ssid[32] = {0};
    if (SetData.dev_info_t.Parallel_dev_sn)
    {
        snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid), "%s%llu", SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn);
    }
    else
    {
        snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);
    }
    memset(SetData.dev_info_t.wifi_ap_ssid, 0x00, sizeof(SetData.dev_info_t.wifi_ap_ssid));
    memcpy(SetData.dev_info_t.wifi_ap_ssid, wifi_ap_ssid, sizeof(wifi_ap_ssid));

    //注意: AP密码必须超过8个字符
    char wifi_ap_password[64] = {0};
    uint8_t md5_value[16] = {0};
    snprintf(wifi_ap_password, sizeof(wifi_ap_password), "%s%llu", iot_factory.iot_type, iot_factory.iot_sn);
    // 32位小写MD5
    calculate_md5((uint8_t *)wifi_ap_password, strlen(wifi_ap_password), md5_value);
    snprintf(wifi_ap_password, sizeof(wifi_ap_password), "%02x%02x%02x%02x%02x%02x%02x%02x",
        md5_value[0], md5_value[2], md5_value[8], md5_value[3], md5_value[4], md5_value[6], md5_value[12], md5_value[10]);
    memset(SetData.dev_info_t.wifi_ap_password, 0x00, sizeof(SetData.dev_info_t.wifi_ap_password));
    memcpy(SetData.dev_info_t.wifi_ap_password, wifi_ap_password, sizeof(wifi_ap_password));
#else
    memset(SetData.dev_info_t.wifi_ap_ssid, 0x00, sizeof(SetData.dev_info_t.wifi_ap_ssid));
    memset(SetData.dev_info_t.wifi_ap_password, 0x00, sizeof(SetData.dev_info_t.wifi_ap_password));
#endif
    // 设置初始默认值
    SetData.dev_info_t.ble_protocol.adv_en = 1;
    SetData.dev_info_t.ble_protocol.lcd_adv_en = 1;
    memcpy(SetData.dev_info_t.bles_adv_key, BLE_ADV_KEY_DEFAULT, sizeof(SetData.dev_info_t.bles_adv_key));

    SetData.dev_info_t.wifi_ap_auth = WIFI_AUTH_WPA2_PSK;

    SetData.dev_info_t.open_mqtt_enable.all = 0;
    SetData.dev_info_t.open_mqtt_report_cycle = 60;

    SetData.dev_info_t.modbus_tcp_enable.all = 0;
    SetData.dev_info_t.modbus_tcp_port = 502;

    ESP_LOGW(TAG, " default iot data, wifi sta auth:%d, ssid:%s, password:%s ", SetData.dev_info_t.wifi_sta_auth,
        SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password);
    ESP_LOGW(TAG, " default iot data, wifi ap auth:%d, ssid:%s, password: %s", SetData.dev_info_t.wifi_ap_auth,
        SetData.dev_info_t.wifi_ap_ssid, SetData.dev_info_t.wifi_ap_password);

    ESP_LOGW(TAG, " default iot data, cloud dns:%s ", SetData.dev_info_t.could_dns);

	for(uint16_t i = 0;i < 6; i++)
	{
		SetData.dev_info_t.app_password[i] =0;
	}

	memcpy(SetData.dev_info_t.Net_Server_address, MQTT_SERVER_ADDRESS_DEFAULT, sizeof(MQTT_SERVER_ADDRESS_DEFAULT));
	memcpy(SetData.dev_info_t.Net_Server_secret, MQTT_SERVER_PASSWD_DEFAULT, sizeof(MQTT_SERVER_PASSWD_DEFAULT));

	SetData.dev_info_t.ctrl_meter.all = ctrl_meter_default;
    SetData.dev_info_t.Meter_Select = 1; //0-invalid;1-入户侧;2-负载侧;（默认用户侧）
    
    SetData.dev_info_t.ctrl_feedback = 1;//默认允许馈电
    SetData.dev_info_t.CounterCurrentPower_Limit = 50;
	SetData.dev_info_t.support_mode.bit.support_ble_pwd=1;
	SetData.dev_info_t.support_mode.bit.visitor_mode=2;

	memset(SetData.dev_info_t.Sn_bind, 0, sizeof(SetData.dev_info_t.Sn_bind));


	SetData.dev_info_t.INV_dev_sn =0;
	SetData.dev_info_t.Parallel_dev_sn =0;
	SetData.dev_info_t.Parallel_dev_safetycode = 0;
	memset(SetData.dev_info_t.INV_dev_type, 0, sizeof(SetData.dev_info_t.INV_dev_type));
	memset(SetData.dev_info_t.Parallel_dev_type, 0, sizeof(SetData.dev_info_t.Parallel_dev_type));

	memset(SetData.dev_info_t.invDetailedInfo_SaveCount, 0, sizeof(SetData.dev_info_t.invDetailedInfo_SaveCount));
	memset(SetData.dev_info_t.invDetailedInfo_AddrIndex, 0, sizeof(SetData.dev_info_t.invDetailedInfo_AddrIndex));	

	memset(SetData.dev_info_t.disaster_Warm_Msg_Id, 0, sizeof(SetData.dev_info_t.disaster_Warm_Msg_Id));
	
	SetData.dev_info_t.http_get_host_valid=0;
	SetData.dev_info_t.http_get_old_port=0;
	memset(SetData.dev_info_t.http_get_old_host, 0, sizeof(SetData.dev_info_t.http_get_old_host));
	memset(SetData.dev_info_t.http_get_old_ipaddr, 0, sizeof(SetData.dev_info_t.http_get_old_ipaddr));
	memset(SetData.dev_info_t.meter_cfg, 0, sizeof(SetData.dev_info_t.meter_cfg));
    SetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag = 0;
    SetData.dev_info_t.Time_Span.power_off_count = 3; // 默认展示每月断电次数
	SetData.dev_info_t.Time_Span.save_money = 4; // 默认展示每年省钱金额
    SetData.dev_info_t.ctrl_lcd_active_time = 2; // 默认屏幕休眠时间30s
    SetData.dev_info_t.LCD_Mode.ble_lcd_active_time = 2; // 默认磁吸屏幕休眠时间30s
    SetData.dev_info_t.LCD_Mode.temperature_unit = 2; // 默认华氏度

//    SetData.dev_info_t.Energy_time_message.su8tm_year = reals.rtc_time.year;
//    SetData.dev_info_t.Energy_time_message.su8tm_mon = reals.rtc_time.mon;
//    SetData.dev_info_t.Energy_time_message.su8tm_mday = reals.rtc_time.day;
//    SetData.dev_info_t.Energy_time_message.su8tm_hour = reals.rtc_time.hour;
 g_self_data.mod_reg11000_IOT_info.Bind_SN =((uint16_t)SetData.dev_info_t.Sn_bind[1]<<8)|SetData.dev_info_t.Sn_bind[0];

 }

void lcd_mode_factory_reset(void)
{
    SetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag = 0;
    SetData.dev_info_t.LCD_Mode.ble_lcd_active_time = 2;
    SetData.dev_info_t.LCD_Mode.temperature_unit = 2;
    g_self_data.mod_reg12000_IOT_set.LCD_Mode.ble_lcd_active_time = 2;
    g_self_data.mod_reg12000_IOT_set.LCD_Mode.temperature_unit = 2;
    Inv_WR.mod_reg12000_IOT_set.LCD_Mode.ble_lcd_active_time = 2;
    Inv_WR.mod_reg12000_IOT_set.LCD_Mode.temperature_unit = 2;
    reals.SetDataWrFlag.sBit.mode_set = 1;
    ESP_LOGW(TAG, "factory reset LCD_Mode: temp_unit=2(F), ble_lcd_active_time=2(30s)");
}

 /*------------------------------------------------------------------------------
  Function: app_password_check
  -----------------------------------------------------------------------------*/
 /**
   * @brief      app密码检查
   * @param[in]  void  
   * @param[out] None
   * @return     void a
   */
 void app_password_check(void)
{
	uint8_t *old = SetData.dev_info_t.app_password;
	uint8_t *new = Inv_WR.mod_reg00000.app_password;
	if (memcmp(old, new, sizeof(SetData.dev_info_t.app_password)) != 0)
	{
		ESP_LOGW(TAG, "change app password, old:%02x %02x %02x %02x %02x %02x --->>> new: %02x %02x %02x %02x %02x %02x",
			old[0], old[1], old[2], old[3], old[4], old[5], new[0], new[1], new[2], new[3], new[4], new[5]);
		reals.SetDataWrFlag.sBit.app_password =1;
		memcpy(SetData.dev_info_t.app_password, Inv_WR.mod_reg00000.app_password, sizeof(SetData.dev_info_t.app_password));

	}
}
static struct {
	struct arg_int *enable;
	struct arg_int *cancel;
	struct arg_int *state;
    struct arg_end *end;
} dw_ctrl_args;


static void thunder_state_update(void)
{
	thunder_ctrl_t *thunder_ctrl = &g_self_data.mod_reg12000_IOT_set.thunder_ctrl;
	thunder_ctrl->thunder_state = GetStormWatchDataState();			// 风暴状态由EMS模块给出
	thunder_ctrl->picture1 = GetStormPicture1State();		// 风暴状态由EMS模块给出

	Inv[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg12000_IOT_set.thunder_ctrl.thunder_state=thunder_ctrl->thunder_state;
}

static uint8_t thunder_ctrl_check(void)
{
	uint8_t update_cfg = 0;
	thunder_ctrl_t *old_cfg = &g_self_data.mod_reg12000_IOT_set.thunder_ctrl;
	thunder_ctrl_t *new_cfg = &Inv_WR.mod_reg12000_IOT_set.thunder_ctrl;
	//new_cfg_tmp.thunder_state = old_cfg_tmp.thunder_state;			// 状态不参与比较
	ESP_LOGI(TAG,"thunder_enable  new :%d  old:%d",new_cfg->thunder_enable,GetStormWatchEnableRegisterData());
	ESP_LOGI(TAG,"thunder_state  new :%d  ",new_cfg->thunder_state);
	ESP_LOGI(TAG,"thunder_cancel  new :%d  ",new_cfg->thunder_cancel);	
	if (new_cfg->thunder_enable !=GetStormWatchEnableRegisterData())
	{
		ESP_LOGW(TAG, "thunder storm mode disable, value: %02x", new_cfg->all);
		if (new_cfg->thunder_enable != 0)
		{
			if ((GetStormWatchEnableRegisterData() != 2) && (new_cfg->thunder_enable == 2))	// 只有风暴模式使能开关变化时才存储到NVS
			{
				/* 风暴模式失能 */
				update_cfg = 1;
				old_cfg->thunder_enable = new_cfg->thunder_enable;
				SetStormWatchEnableRegisterData(new_cfg->thunder_enable);
			}

			if ((GetStormWatchEnableRegisterData() != 1) && (new_cfg->thunder_enable == 1))	// 只有风暴模式使能开关变化时才存储到NVS
			{
				/* 风暴模式使能 */
				update_cfg = 1;
				old_cfg->thunder_enable = new_cfg->thunder_enable;
				SetStormWatchEnableRegisterData(new_cfg->thunder_enable);
			}
		}else{
			new_cfg->thunder_enable=GetStormWatchEnableRegisterData();
			old_cfg->thunder_enable=GetStormWatchEnableRegisterData();
		}

	}

	if ((new_cfg->thunder_cancel == 1)&&(new_cfg->thunder_cancel!=GetAppCancelEventEndFlag()))
	{
		/* 取消当前风暴事件 */
		ESP_LOGW(TAG, "cancel thunder storm event, value: %02x", new_cfg->all);
		SetAppCancelEventData(new_cfg->thunder_cancel);
		new_cfg->thunder_cancel = 0;
		old_cfg->thunder_cancel = 0;
	}

	//new_cfg->all = old_cfg->all;

	return update_cfg;
}
//
///*
//
//*/
//static uint8_t iot_on_off_check(void)
//{
//	uint8_t update_cfg = 0;
//
//	
//	config_status *old_cfg = &SetData.dev_info_t.on_off;
//	//config_status *old_cfg = &g_self_data.mod_reg12000_IOT_set.on_off;
//	config_status *new_cfg = &Inv_WR.mod_reg12000_IOT_set.on_off;
//
//	if (old_cfg->all != new_cfg->all)
//	{
//		if(old_cfg->bit.wifi_enable != new_cfg->bit.wifi_enable)			// wifi开关改变
//		{
//			if (new_cfg->bit.wifi_enable != 0)
//			{
////				ESP_LOGW(TAG, "wifi_enable on-off: %s", (new_cfg->bit.wifi_enable==0x01) ? ("open") : ("close"));
////				(new_cfg->bit.wifi_enable==0x01) ? (iot_wifi_open(WIFI_MODE_STA)) : (iot_wifi_close(WIFI_MODE_ALL));
//				update_cfg = 1;
//			}
//		}
//
//		if(old_cfg->bit.wifi_sta_enable != new_cfg->bit.wifi_sta_enable)	// wifi station开关改变
//		{
//			if (new_cfg->bit.wifi_sta_enable != 0)
//			{
//				ESP_LOGW(TAG, "wifi_sta_enable on-off: %s", (new_cfg->bit.wifi_sta_enable==0x01) ? ("open") : ("close"));
//				update_cfg = 1;
//			}
//		}
//
//		if(old_cfg->bit.wifi_ap_enable != new_cfg->bit.wifi_ap_enable)		// wifi ap开关改变
//		{
//			if (new_cfg->bit.wifi_ap_enable != 0)
//			{
//				ESP_LOGW(TAG, "wifi_ap_enable on-off: %s", (new_cfg->bit.wifi_ap_enable==0x01) ? ("open") : ("close"));
//				update_cfg = 1;
//			}
//		}
//
//		if(old_cfg->bit.ble_enable != new_cfg->bit.ble_enable)				// ble开关改变
//		{
//			if (new_cfg->bit.ble_enable != 0)
//			{
//				ESP_LOGW(TAG, "ble_enable on-off: %s", (new_cfg->bit.ble_enable==0x01) ? ("open") : ("close"));
////				(new_cfg->bit.ble_enable==0x01) ? (iot_ble_start()) : (iot_ble_stop());
//				update_cfg = 1;
//			}
//		}
//
//		if(old_cfg->bit.ble_adv_enable != new_cfg->bit.ble_adv_enable)		// ble广播开关改变
//		{
//			if (new_cfg->bit.ble_adv_enable != 0)
//			{
//				ESP_LOGW(TAG, "ble_adv_enable on-off: %s", (new_cfg->bit.ble_adv_enable==0x01) ? ("open") : ("close"));
//				update_cfg = 1;
//			}
//		}
//
//		if(old_cfg->bit.ble_mash_enable != new_cfg->bit.ble_mash_enable)	// ble mesh开关改变
//		{
//			if (new_cfg->bit.ble_mash_enable != 0)
//			{
//				ESP_LOGW(TAG, "ble_mash_enable on-off: %s", (new_cfg->bit.ble_mash_enable==0x01) ? ("open") : ("close"));
//				update_cfg = 1;
//			}
//		}
//
//		if(old_cfg->bit._4g_enable != new_cfg->bit._4g_enable)				// 4g开关改变
//		{
//			if (new_cfg->bit._4g_enable != 0)
//			{
//				ESP_LOGW(TAG, "_4g_enable on-off: %s", (new_cfg->bit._4g_enable==0x01) ? ("open") : ("close"));
//				update_cfg = 1;
//			}
//		}
//
//		if(old_cfg->bit.gps_enable != new_cfg->bit.gps_enable)				// gps开关改变
//		{
//			if (new_cfg->bit.gps_enable != 0)
//			{
//				ESP_LOGW(TAG, "gps_enable on-off: %s", (new_cfg->bit.gps_enable==0x01) ? ("open") : ("close"));
//				update_cfg = 1;
//			}
//		}
//        if(old_cfg->bit.Eth_enable != new_cfg->bit.Eth_enable)				// Eth开关改变
//		{
//			if (new_cfg->bit.Eth_enable != 0)
//			{
//				ESP_LOGW(TAG, "Eth_enable on-off: %s", (new_cfg->bit.Eth_enable==0x01) ? ("open") : ("close"));
//				update_cfg = 1;
//			}
//		}
//        if(old_cfg->bit.new_dev_discovery != new_cfg->bit.new_dev_discovery)				// 新设备发现模式开关改变（BR）
//		{
//			if (new_cfg->bit.new_dev_discovery != 0)
//			{
//				ESP_LOGW(TAG, "new_dev_discovery on-off: %s", (new_cfg->bit.new_dev_discovery==0x01) ? ("open") : ("close"));
//				//update_cfg = 1;//不保存
//			}
//		}
//
//	}
//
//	if (update_cfg) {
//		ESP_LOGW(TAG, "new on-off config: %08lx", new_cfg->all);
//	}
//
//
//
//
//	return update_cfg;
//}

/*
BLE开关逻辑
*/
void BLE_ON_OFF_check (void)
{
//////////ble enable
	static uint8_t sBleEnable = 0xFF;
	uint8_t BleEnable = 0;

	//ESP_LOGE(TAG," BLE SetData.dev_info_t.on_off.bit.ble_enable=%u",SetData.dev_info_t.on_off.bit.ble_enable);

#ifdef FORCE_BLE_WIFI_ON_ENABLE
	BleEnable =1;

#else
	if((SetData.dev_info_t.on_off.bit.ble_enable==0x01)&&(1 == reals.Addr_can_master))
	{
		BleEnable =1;
	}
	else if((SetData.dev_info_t.on_off.bit.ble_enable==0x02)||(0 == reals.Addr_can_master))
	{
		BleEnable =2;
	}
	else
	{
		BleEnable =0;
	}	

	// //异常恢复机制 380显示屏也能控制蓝牙、wifi，不需要再针对无效值进行恢复
	// if((SetData.dev_info_t.on_off.bit.ble_enable==0x00)&&(1 == reals.Addr_can_master))
	// {
	// 	SetData.dev_info_t.on_off.bit.ble_enable = 0x01;
	// }


#endif

	//ESP_LOGE(TAG," BLE sBleEnable=%u, BleEnable=%u",sBleEnable,BleEnable);
	
	if(sBleEnable != BleEnable)
	{
//		ESP_LOGI(TAG, "iot_on_off_check ，BleEnable=%d",BleEnable);
	
		sBleEnable = BleEnable;
		if(1 == BleEnable)
		{
			ESP_LOGW(TAG,"111--- ble start!");
			iot_ble_start();
//ESP_LOGI(TAG, "iot_on_off_check22 ，BleEnable=%d",BleEnable);
			
		}
		else if(2 == BleEnable)
		{
			if(reals.ota_happen == 0)// 升级时 不允许关掉蓝牙(achub升级时会出现短暂的Addr_can_master为0的情况)
			{
				ESP_LOGW(TAG,"111--- ble stop!");
				iot_ble_stop();//非主设备不开BLE/WIFI
			}

		}

	}

	if(2 == BleEnable)
	{
		drv_ble_advertise_stop2();
	}	


}

/*
WIFI 开关逻辑
*/
void WIFI_ON_OFF_check (void)
{
//////////ble enable
	static uint8_t sBleEnable = 0xFF;
	uint8_t BleEnable = 0;

	//ESP_LOGE(TAG,"WIFI SetData.dev_info_t.on_off.bit.wifi_enable=%u",SetData.dev_info_t.on_off.bit.wifi_enable);
#ifdef FORCE_BLE_WIFI_ON_ENABLE
	BleEnable =1;
	SetData.dev_info_t.on_off.bit.wifi_enable=1;
#else
	if((SetData.dev_info_t.on_off.bit.wifi_enable==0x01)&&(1 == reals.Addr_can_master))
	{
		BleEnable =1;
	}
	else if((SetData.dev_info_t.on_off.bit.wifi_enable==0x02)||(0 == reals.Addr_can_master))
	{
		BleEnable =2;
	}
	else
	{
		BleEnable =0;
	}	
	// BleEnable =1;//windy,因要默认使用WIFI MESH，所以 WIFI 必须使能	//380面板要求要能开关蓝牙、wifi
	
#endif

//	(new_cfg->bit.wifi_enable==0x01) ? (iot_wifi_open(WIFI_MODE_STA)) : (iot_wifi_close(WIFI_MODE_ALL));

	
	if(sBleEnable != BleEnable)
	{
		ESP_LOGI(TAG, "iot_on_off_check ，BleEnable=%d",BleEnable);
	
		sBleEnable = BleEnable;
		if(1 == BleEnable)
		{
			/* 启动阶段 iot_period_task 已 iot_wifi_open；CAN 并机约 15s 后才置位
			 * Addr_can_master，此处 BleEnable 0->1 不应再次 open（会 esp_wifi_stop 断网） */
			const wifi_info_t *wifi_st = iot_get_wifi_info();
			if (wifi_st == NULL || !wifi_st->flag.bit.sta_enable) {
				iot_wifi_open(WIFI_MODE_ALL);//WIFI_MODE_STA
			}
		}
		else if(2 == BleEnable)
		{
			iot_wifi_close(WIFI_MODE_ALL);//非主设备不开BLE/WIFI

		}
		ESP_LOGW(TAG, "wifi_enable on-off: %s", (BleEnable == 0x01) ? ("open") : ("close"));
	}

	// 此处是WiFi的开关逻辑 为什么要把蓝牙的在这也关了？
	// if(2 == BleEnable)
	// {
	// 	drv_ble_advertise_stop2();

	// }	


}

/*
生产标定
*/
static void iot_factory_SN_WR (void)
{
    uint8_t update_cfg = 0;//

//AP300-IOT通过CAN标定
	if((Inv_can_WR.factory.sn) && (Inv_can_WR.factory.safetCode))
	{
		if ((iot_factory.iot_sn != Inv_can_WR.factory.sn) 
		&& (iot_factory.safe_code != Inv_can_WR.factory.safetCode)){
			memcpy(iot_factory.iot_type, Inv_can_WR.factory.type, sizeof(iot_factory.iot_type));
			iot_factory.iot_sn = Inv_can_WR.factory.sn;
			iot_factory.safe_code = Inv_can_WR.factory.safetCode;
			update_cfg = 2;
		}
	}
	

	/* 如果出厂信息发生变化,保存参数到系统NVS出厂信息区
	 * 该区保存的是IOT自身的标定信息,通过ESP32内部CAN总线出厂时写入 
恢复默认值
	 */
    if (update_cfg == 2) 
	{
        size_t data_len = sizeof(iot_factory);
		if(ESP_OK == iot_wtite_dev_info(IOT_FACTORY, (const uint8_t *)&iot_factory, data_len))
		{

		}
		else//fail retry
		{
			vTaskDelay(pdMS_TO_TICKS(100));
			iot_wtite_dev_info(IOT_FACTORY, (const uint8_t *)&iot_factory, data_len);//
		}
        iot_read_dev_info(IOT_FACTORY, (uint8_t *)&iot_factory, &data_len); /* 读取IOT出厂信息 */
        ESP_LOGW (TAG, "iot type: %s, iot sn: %llu, iot safet: %llu",
                  iot_factory.iot_type,
                  iot_factory.iot_sn,
                  iot_factory.safe_code);

		/* 出厂信息发生变化,重新登录MQTT */
        iot_wifi_new_iot(iot_factory.iot_type,
                         iot_factory.iot_sn,
                         iot_factory.safe_code);

	 // 蓝牙开启-设置蓝牙广播名称
		 iot_ble_new_name(iot_factory.iot_type, iot_factory.iot_sn);
		//ESP_LOGI(TAG, "iot_about_update HUANGJI CCC type:%s -%d -%d",iot_factory.iot_type,  (unsigned int)iot_factory.iot_sn, (unsigned int)iot_factory.safe_code);

    }

	
		/* 出厂的时候通过CAN标定数据,修改的是g_device_data.iot_dev_node.iot_about的值
		 * 理论上下面的代码将永远不会执行 */
	
	
	//	  if (strlen(MicroInv_WR.mod_reg29700_IOT_info.dev_type)) {
	//		  if (strncmp(iot_factory.iot_type, MicroInv_WR.mod_reg29700_IOT_info.dev_type, sizeof(iot_factory.iot_type))) {
	//			  memcpy(iot_factory.iot_type, MicroInv_WR.mod_reg29700_IOT_info.dev_type, sizeof(iot_factory.iot_type));
	//			  update_cfg = 2;
	//		  }
	//	  }
	//	  if (convertToUint64(MicroInv_WR.mod_reg29700_IOT_info.iot_sn)) {
	//		  if (iot_factory.iot_sn != convertToUint64(MicroInv_WR.mod_reg29700_IOT_info.iot_sn)) {
	//			  iot_factory.iot_sn = convertToUint64(MicroInv_WR.mod_reg29700_IOT_info.iot_sn);
	//			  update_cfg = 2;
	//		  }
	//	  }
	//	  if (convertToUint64(MicroInv_WR.mod_reg29700_IOT_info.iot_safetyCode)) {
	//		  if (iot_factory.safe_code != convertToUint64(MicroInv_WR.mod_reg29700_IOT_info.iot_safetyCode)) {
	//			  iot_factory.safe_code = convertToUint64(MicroInv_WR.mod_reg29700_IOT_info.iot_safetyCode);
	//			  update_cfg = 2;
	//		  }
	//	  }

}

// 处理时间跨度设置检查
void iot_time_span_check(void)
{
    uint8_t update_cfg = 0;
    if(Inv_WR.mod_reg12000_IOT_set.Time_Span.all)
    {
        if(Inv_WR.mod_reg12000_IOT_set.Time_Span.power_off_count && (Inv_WR.mod_reg12000_IOT_set.Time_Span.power_off_count != SetData.dev_info_t.Time_Span.power_off_count))
        {
            SetData.dev_info_t.Time_Span.power_off_count = Inv_WR.mod_reg12000_IOT_set.Time_Span.power_off_count;
            g_self_data.mod_reg12000_IOT_set.Time_Span.power_off_count = Inv_WR.mod_reg12000_IOT_set.Time_Span.power_off_count;
            ESP_LOGW(TAG, "power_off_count change to: %d", SetData.dev_info_t.Time_Span.power_off_count);
            update_cfg = 1;
        }
        if(Inv_WR.mod_reg12000_IOT_set.Time_Span.save_money && (Inv_WR.mod_reg12000_IOT_set.Time_Span.save_money != SetData.dev_info_t.Time_Span.save_money))
        {
            SetData.dev_info_t.Time_Span.save_money = Inv_WR.mod_reg12000_IOT_set.Time_Span.save_money;
            g_self_data.mod_reg12000_IOT_set.Time_Span.save_money = Inv_WR.mod_reg12000_IOT_set.Time_Span.save_money;
            ESP_LOGW(TAG, "save_money change to: %d", SetData.dev_info_t.Time_Span.save_money);
            update_cfg = 1;
        }

        if(update_cfg)
        {
            reals.SetDataWrFlag.sBit.time_span = 1;
        }
    }
}

/**
* @brief 检查并执行电源控制指令
*
* 唤醒时通过io，休眠指令则通过can发命令即可。
*/
static void iot_power_ctrl_check(void)
{
    uint8_t ctrl_poweron = Inv_WR.mod_reg02000_Inv_base_set.ctrl_poweron;
	ESP_LOGW(TAG, "ctrl_poweron: %d", ctrl_poweron);
    if (ctrl_poweron == 2 && reals.IOT_Status_Flag.sBit.system_sleep_flag == 1) // 休眠开机
    {
        reals.IOT_Status_Flag.sBit.system_wakeup_flag = 1; // 通过GPIO唤醒
        ESP_LOGW(TAG, "system_wakeup_flag: by 2");
    }
	if (ctrl_poweron == 3) // 开机、唤醒
	{
		reals.IOT_Status_Flag.sBit.system_wakeup_flag = 1; // 通过GPIO唤醒
		ESP_LOGW(TAG, "system_wakeup_flag: by 3");
	}
}

/* 
NVS 本地变量更改写入函数

检查系统配置是否发生变化,某些参数变化后会将变化的参数保存在系统NVS区
 * dev_info表示系统本地参数,该函数会比较本地参数与APP或服务器端下发的参数是否发生变化 


windy :20240923
来自 modbus /CAN set IOT ,tbd check

 */
static void iot_cfg_check (void)
{
    uint8_t update_cfg = 0;//1-server 1-我司服务器;2-server 2-对公服务器;3-WIFI路由器配置
	uint8_t wifi_sta_cfg_changed = 0;
	static uint8_t sAddr_can_master_old =0;
    uint8_t webserver_cfg_update = 0;

	if (1 == reals.ModbusCmdFlag.sBit.iot_can_set_mix)
	{
		reals.ModbusCmdFlag.sBit.iot_can_set_mix = 0;
//		reals.ModbusCmdFlag.sBit.new_cfg = 1;

		
		memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid,Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.wifi_sta_ssid , sizeof(SetData.dev_info_t.wifi_sta_ssid));
		memcpy(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password,Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.wifi_sta_password , sizeof(SetData.dev_info_t.wifi_sta_password));
		Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth = Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.wifi_sta_auth;
		
		Inv_WR.mod_reg12000_IOT_set.period_report = Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.period_report;
		

		
		memcpy(Inv_WR.mod_reg12000_IOT_set.could_dns,Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.could_dns , sizeof(SetData.dev_info_t.could_dns));
//		memcpy(g_self_data_WR.mod_reg22000_net_server_2rd.Net_Server_address,Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.wifi_sta_password , sizeof(SetData.dev_info_t.Net_Server_address));
//		memcpy(g_self_data_WR.mod_reg22000_net_server_2rd.Net_Server_secret,Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set.wifi_sta_password , sizeof(SetData.dev_info_t.Net_Server_secret));

        webserver_cfg_update = 1;
	}

	
	if (reals.ModbusCmdFlag.sBit.wifi_sta_ssid == 1)
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 0;
		if (strlen(Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid)) 
		{		/* WIFI配网变化名称 */
			if (strncmp(SetData.dev_info_t.wifi_sta_ssid, Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid, sizeof(SetData.dev_info_t.wifi_sta_ssid))) 
			{
				memcpy(SetData.dev_info_t.wifi_sta_ssid, Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid, sizeof(SetData.dev_info_t.wifi_sta_ssid));
				ESP_LOGW(TAG, "wifi name change to: %s", SetData.dev_info_t.wifi_sta_ssid);
				update_cfg = 3;
				wifi_sta_cfg_changed = 1;
				reals.SetDataWrFlag.sBit.wifi_sta_ssid =1;
				
				SetData.dev_info_t.Wr_flag.bits.Wifi =1;
                webserver_cfg_update = 1;
			}
		}
	}

	if (reals.ModbusCmdFlag.sBit.wifi_sta_password == 1)
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_password = 0;
//		if (strlen(Inv_WR.mod_reg12000_IOT_set.wifi_sta_password)) //20240924 ,要支持空密码
		{	/* WIFI配网变化密码 */
			if (strncmp(SetData.dev_info_t.wifi_sta_password, Inv_WR.mod_reg12000_IOT_set.wifi_sta_password, sizeof(SetData.dev_info_t.wifi_sta_password))) 
			{
				memcpy(SetData.dev_info_t.wifi_sta_password, Inv_WR.mod_reg12000_IOT_set.wifi_sta_password, sizeof(SetData.dev_info_t.wifi_sta_password));
				ESP_LOGW(TAG, "wifi password change to: %s", SetData.dev_info_t.wifi_sta_password);
			    update_cfg = 3;
				wifi_sta_cfg_changed = 1;
				reals.SetDataWrFlag.sBit.wifi_sta_password =1;
				SetData.dev_info_t.Wr_flag.bits.Wifi =1;
                webserver_cfg_update = 1;
			}
		}
	}

	if (reals.ModbusCmdFlag.sBit.wifi_sta_auth == 1)
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_auth = 0;
		if (SetData.dev_info_t.wifi_sta_auth != Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth) 
		{	/* WiFi配网认证方式改变 */
			if (Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth >= WIFI_AUTH_MAX) 
			{
				ESP_LOGE(TAG, "sta auth(%d) error", Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth);
			} 
			else 
			{
				SetData.dev_info_t.wifi_sta_auth = Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth;
				ESP_LOGW(TAG, "wifi auth change to: %d", SetData.dev_info_t.wifi_sta_auth);
			    update_cfg = 3;
				wifi_sta_cfg_changed = 1;
				reals.SetDataWrFlag.sBit.mix =1;
				SetData.dev_info_t.Wr_flag.bits.Wifi =1;
                webserver_cfg_update = 1;
			}
		}
	}

	if (wifi_sta_cfg_changed
		&& (1 == SetData.dev_info_t.on_off.bit.wifi_enable)
		&& (strlen(SetData.dev_info_t.wifi_sta_ssid) > 0)) {
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
		iot_wifi_reconnect_sta(SetData.dev_info_t.wifi_sta_auth,
		                       SetData.dev_info_t.wifi_sta_ssid,
		                       SetData.dev_info_t.wifi_sta_password, NULL);
#else
		iot_wifi_reconnect_sta(SetData.dev_info_t.wifi_sta_auth,
		                       SetData.dev_info_t.wifi_sta_ssid,
		                       SetData.dev_info_t.wifi_sta_password);
#endif
	}

	if (reals.ModbusCmdFlag.sBit.could_dns == 1)
	{
		reals.ModbusCmdFlag.sBit.could_dns = 0;

		/* 比较服务器DNS是否发生变化 */
		if (strlen(Inv_WR.mod_reg12000_IOT_set.could_dns)) 
		{
			if (strncmp(SetData.dev_info_t.could_dns, Inv_WR.mod_reg12000_IOT_set.could_dns, sizeof(SetData.dev_info_t.could_dns))) 
			{
                ESP_LOGW(TAG, "old cloud dns: %s", SetData.dev_info_t.could_dns);
				memcpy(SetData.dev_info_t.could_dns, Inv_WR.mod_reg12000_IOT_set.could_dns, sizeof(SetData.dev_info_t.could_dns));
				ESP_LOGW(TAG, "new cloud dns: %s", SetData.dev_info_t.could_dns);
				/* 清除连接记录缓存，强制重新解析新 DNS */
				SetData.dev_info_t.http_get_host_valid = 0;
				SetData.dev_info_t.http_get_old_port = 0;
				memset(SetData.dev_info_t.http_get_old_host, 0, sizeof(SetData.dev_info_t.http_get_old_host));
				memset(SetData.dev_info_t.http_get_old_ipaddr, 0, sizeof(SetData.dev_info_t.http_get_old_ipaddr));
				reals.SetDataWrFlag.sBit.Set_Host_Ip = 1;
				iot_wifi_new_host(SetData.dev_info_t.could_dns);

				/* cloud dns 改变 */
				reals.SetDataWrFlag.sBit.could_dns =1;
                webserver_cfg_update = 1;
			}
		}
	}

	if (reals.ModbusCmdFlag.sBit.mobile_apn == 1)
	{
		reals.ModbusCmdFlag.sBit.mobile_apn = 0;
        webserver_cfg_update = 1;
	}

    if (reals.ModbusCmdFlag.sBit.mode_set == 1)
    {
        reals.ModbusCmdFlag.sBit.mode_set = 0;

        lcd_mode_t incoming_lcd_mode = Inv_WR.mod_reg12000_IOT_set.LCD_Mode;
        lcd_mode_t merged_lcd_mode = SetData.dev_info_t.LCD_Mode;
        // const lcd_mode_t before_lcd_mode = merged_lcd_mode; // 用于打印：来自持久化/当前值

        // /* 只打印 3 个有效子域，避免把 16bit all 打出来导致难以定位问题 */
        // ESP_LOGW(TAG,
        //          "LCD_Mode before: pairing=%u temp_unit=%u ble_time=%u",
        //          before_lcd_mode.pairing_mode,
        //          before_lcd_mode.temperature_unit,
        //          before_lcd_mode.ble_lcd_active_time);
        // ESP_LOGW(TAG,
        //          "LCD_Mode incoming: pairing=%u temp_unit=%u ble_time=%u",
        //          incoming_lcd_mode.pairing_mode,
        //          incoming_lcd_mode.temperature_unit,
        //          incoming_lcd_mode.ble_lcd_active_time);

        /* APP可能只下发LCD_Mode中的单个字段，其余位补0。这里按字段合并，避免互相清零。 */
        if ((incoming_lcd_mode.temperature_unit >= 1) && (incoming_lcd_mode.temperature_unit <= 2)) {
			merged_lcd_mode.temperature_unit = incoming_lcd_mode.temperature_unit;
			SetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag = 1;
			// ESP_LOGI("IOT_TASK", "APP set temperature unit: %d, flag set to 1",
			// 			merged_lcd_mode.temperature_unit);
			g_self_data.mod_reg12000_IOT_set.LCD_Mode.temperature_unit = incoming_lcd_mode.temperature_unit;
        }

        if ((incoming_lcd_mode.ble_lcd_active_time >= 1) && (incoming_lcd_mode.ble_lcd_active_time <= 6)) {
			merged_lcd_mode.ble_lcd_active_time = incoming_lcd_mode.ble_lcd_active_time;
			g_self_data.mod_reg12000_IOT_set.LCD_Mode.ble_lcd_active_time = incoming_lcd_mode.ble_lcd_active_time;
        }

        if ((incoming_lcd_mode.pairing_mode >= 1) && (incoming_lcd_mode.pairing_mode <= 2)) {
			merged_lcd_mode.pairing_mode = incoming_lcd_mode.pairing_mode;
			g_self_data.mod_reg12000_IOT_set.LCD_Mode.pairing_mode = incoming_lcd_mode.pairing_mode;
        }

        // ESP_LOGW(TAG,
        //          "LCD_Mode merged: pairing=%u temp_unit=%u ble_time=%u",
        //          merged_lcd_mode.pairing_mode,
        //          merged_lcd_mode.temperature_unit,
        //          merged_lcd_mode.ble_lcd_active_time);

		SetData.dev_info_t.LCD_Mode.all = merged_lcd_mode.all;
		g_self_data.mod_reg12000_IOT_set.LCD_Mode.all = merged_lcd_mode.all;
		Inv_WR.mod_reg12000_IOT_set.LCD_Mode.all = merged_lcd_mode.all;

		// ESP_LOGW(TAG,
		// 			"LCD_Mode written g_self_data: pairing=%u temp_unit=%u ble_time=%u",
		// 			g_self_data.mod_reg12000_IOT_set.LCD_Mode.pairing_mode,
		// 			g_self_data.mod_reg12000_IOT_set.LCD_Mode.temperature_unit,
		// 			g_self_data.mod_reg12000_IOT_set.LCD_Mode.ble_lcd_active_time);
		// ESP_LOGW(TAG,
		// 			"LCD_Mode written Inv_WR: pairing=%u temp_unit=%u ble_time=%u",
		// 			Inv_WR.mod_reg12000_IOT_set.LCD_Mode.pairing_mode,
		// 			Inv_WR.mod_reg12000_IOT_set.LCD_Mode.temperature_unit,
		// 			Inv_WR.mod_reg12000_IOT_set.LCD_Mode.ble_lcd_active_time);

        reals.SetDataWrFlag.sBit.mode_set = 1;
    }

    if (reals.ModbusCmdFlag.sBit.time_span == 1)
    {
        reals.ModbusCmdFlag.sBit.time_span = 0;

        // if (Inv_WR.mod_reg12000_IOT_set.Time_Span.all != SetData.dev_info_t.Time_Span.all)
        // {
        //     SetData.dev_info_t.Time_Span.all = Inv_WR.mod_reg12000_IOT_set.Time_Span.all;
        //     reals.SetDataWrFlag.sBit.time_span = 1;
        //     ESP_LOGW(TAG, "Time_Span change to: %u", SetData.dev_info_t.Time_Span.all);
        // }

        iot_time_span_check();
    }

	if (reals.ModbusCmdFlag.sBit.on_off == 1)
	{
		reals.ModbusCmdFlag.sBit.on_off = 0;
		reals.iot_smart_change = 1;
		/* iot on-off开关检查 */
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_enable)
		{
			if(SetData.dev_info_t.on_off.bit.wifi_enable != Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_enable)//与当前状态不一样才更新
			{
				ESP_LOGW(TAG,"iot on-off, update_cfg =3");
				update_cfg =3;
			}
			SetData.dev_info_t.on_off.bit.wifi_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}
		
		// if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_sta_enable)
		// {
		// 	SetData.dev_info_t.on_off.bit.wifi_sta_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_sta_enable;
		// 	reals.SetDataWrFlag.sBit.on_off =1;
			
		// }
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_ap_enable)
		{
            if (SetData.dev_info_t.on_off.bit.wifi_ap_enable != Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_ap_enable)
            {
                update_cfg = 4;
                ESP_LOGW(TAG, "iot on-of , update_cfg = 4");
            }
            SetData.dev_info_t.on_off.bit.wifi_ap_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.wifi_ap_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
			
		}
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.ble_enable)
		{
			SetData.dev_info_t.on_off.bit.ble_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.ble_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.ble_adv_enable)
		{
			SetData.dev_info_t.on_off.bit.ble_adv_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.ble_adv_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.ble_mash_enable)
		{
			SetData.dev_info_t.on_off.bit.ble_mash_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.ble_mash_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit._4g_enable)
		{
			SetData.dev_info_t.on_off.bit._4g_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit._4g_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.gps_enable)
		{
			SetData.dev_info_t.on_off.bit.gps_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.gps_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}

		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.Eth_enable)
		{
			SetData.dev_info_t.on_off.bit.Eth_enable = Inv_WR.mod_reg12000_IOT_set.on_off.bit.Eth_enable;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.revd1)
		{
			SetData.dev_info_t.on_off.bit.revd1 = Inv_WR.mod_reg12000_IOT_set.on_off.bit.revd1;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}
		
		if(Inv_WR.mod_reg12000_IOT_set.on_off.bit.new_dev_discovery)
		{
			SetData.dev_info_t.on_off.bit.new_dev_discovery = Inv_WR.mod_reg12000_IOT_set.on_off.bit.new_dev_discovery;
			reals.SetDataWrFlag.sBit.on_off =1;
            webserver_cfg_update = 1;
		}

		/* ARM 上电发蓝牙 on_off 后，回传当前 BLE/WiFi 连接状态； */
		link_report_set(1);
		ESP_LOGW(TAG, "arm on_off set, resend link & smart control");
	}

	if (reals.ModbusCmdFlag.sBit.thunder_ctrl == 1)
	{
		reals.ModbusCmdFlag.sBit.thunder_ctrl = 0;
		/* 风暴模式开关检查 */
		uint8_t thunder_changed;
		if ((thunder_changed = thunder_ctrl_check()) != 0)
		{
			SetData.dev_info_t.thunder_ctrl = Inv_WR.mod_reg12000_IOT_set.thunder_ctrl;
            //g_self_data.mod_reg12000_IOT_set.thunder_ctrl = g_self_data_WR.mod_reg12000_IOT_set.thunder_ctrl;
			reals.SetDataWrFlag.sBit.thunder_ctrl =1;
            webserver_cfg_update = 1;
		}
	}

	if (reals.ModbusCmdFlag.sBit.period_report == 1)
	{
		reals.ModbusCmdFlag.sBit.period_report = 0;

		if (Inv_WR.mod_reg12000_IOT_set.period_report)
        {   /* iot上报周期（仅4g） */
			if (SetData.dev_info_t.data_report_cycle != Inv_WR.mod_reg12000_IOT_set.period_report) 
            {
				SetData.dev_info_t.data_report_cycle = Inv_WR.mod_reg12000_IOT_set.period_report;
				ESP_LOGW(TAG, "data_report_cycle to: %lu", SetData.dev_info_t.data_report_cycle);
				reals.SetDataWrFlag.sBit.data_report_cycle =1;
                webserver_cfg_update = 1;
			}
		}

	}

    if (reals.ModbusCmdFlag.sBit.ble_protocol == 1)
    {
        reals.ModbusCmdFlag.sBit.ble_protocol = 0;

        /* 刷新密钥 */
        if (Inv_WR.mod_reg13600_open.ble_protocol.refresh_key > 0)
        {
            ble_adv_key_update(Inv_WR.mod_reg13600_open.ble_protocol.refresh_key);
            reals.SetDataWrFlag.sBit.ble_protocol = 1;
            Inv_WR.mod_reg13600_open.ble_protocol.refresh_key = 0;
            webserver_cfg_update = 1;
        }

        /* BLE协议相关参数变化检查 */
        if (Inv_WR.mod_reg13600_open.ble_protocol.adv_en > 0)
        {
            if (SetData.dev_info_t.ble_protocol.adv_en
                != Inv_WR.mod_reg13600_open.ble_protocol.adv_en)
            {
                SetData.dev_info_t.ble_protocol.adv_en = Inv_WR.mod_reg13600_open.ble_protocol.adv_en;
                ESP_LOGW(TAG, "ble server adv_en: %d", SetData.dev_info_t.ble_protocol.adv_en);
                reals.SetDataWrFlag.sBit.ble_protocol =1;
                webserver_cfg_update = 1;
            }
        }
    }

    //13600~13602 ble相关参数配置变化
    if (reals.ModbusCmdFlag.sBit.ble_client == 1)
    {
        reals.ModbusCmdFlag.sBit.ble_client = 0;
        if (Inv_WR.mod_reg13600_open.blec_rssi_threshold < 0)
        {
            if (SetData.dev_info_t.blec_rssi_th != Inv_WR.mod_reg13600_open.blec_rssi_threshold)
            {
                SetData.dev_info_t.blec_rssi_th = Inv_WR.mod_reg13600_open.blec_rssi_threshold;
                ESP_LOGW(TAG, "ble client rssi threshold: %d", SetData.dev_info_t.blec_rssi_th);

                reals.SetDataWrFlag.sBit.ble_client =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.blec_switch_interval > 0)
        {
            if (SetData.dev_info_t.blec_switch_int != Inv_WR.mod_reg13600_open.blec_switch_interval)
            {
                SetData.dev_info_t.blec_switch_int = Inv_WR.mod_reg13600_open.blec_switch_interval;
                ESP_LOGW(TAG, "ble client switch interval to: %u", SetData.dev_info_t.blec_switch_int);

                reals.SetDataWrFlag.sBit.ble_client =1;
                webserver_cfg_update = 1;
            }
        }
    }

    if (reals.ModbusCmdFlag.sBit.ble_server == 1)
    {
        reals.ModbusCmdFlag.sBit.ble_server = 0;

        if (strlen((char *)Inv_WR.mod_reg13600_open.bles_adv_key))
        {
            if (strncmp((char *)SetData.dev_info_t.bles_adv_key,
                    (char *)Inv_WR.mod_reg13600_open.bles_adv_key,
                    sizeof(SetData.dev_info_t.bles_adv_key)))
            {
                memcpy(SetData.dev_info_t.bles_adv_key, Inv_WR.mod_reg13600_open.bles_adv_key,
                            sizeof(SetData.dev_info_t.bles_adv_key));
                ESP_LOGW(TAG, "ble server adv key change to: %s", SetData.dev_info_t.bles_adv_key);
                reals.SetDataWrFlag.sBit.ble_server =1;
                webserver_cfg_update = 1;
            }
        }
    }

    //13603~13604 wifi 多sta切换相关参数配置变化
    if (reals.ModbusCmdFlag.sBit.wifi_sta_rssi == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_sta_rssi = 0;
        if (Inv_WR.mod_reg13600_open.wifi_sta_rssi_threshold < 0)
        {
            if (SetData.dev_info_t.wifi_sta_rssi_th != Inv_WR.mod_reg13600_open.wifi_sta_rssi_threshold)
            {
                SetData.dev_info_t.wifi_sta_rssi_th = Inv_WR.mod_reg13600_open.wifi_sta_rssi_threshold;
                ESP_LOGW(TAG, "wifi sta rssi threshold: %d", SetData.dev_info_t.wifi_sta_rssi_th);
                // update_cfg = 3;  //此值变化不需要重新连接WiFi
                reals.SetDataWrFlag.sBit.wifi_sta_rssi =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta_switch_interval > 0)
        {
            if (SetData.dev_info_t.wifi_sta_switch_int != Inv_WR.mod_reg13600_open.wifi_sta_switch_interval)
            {
                SetData.dev_info_t.wifi_sta_switch_int = Inv_WR.mod_reg13600_open.wifi_sta_switch_interval;
                ESP_LOGW(TAG, "wifi sta switch interval to: %u", SetData.dev_info_t.wifi_sta_switch_int);
                // update_cfg = 3;  //此值变化不需要重新连接WiFi
                reals.SetDataWrFlag.sBit.wifi_sta_rssi =1;
                webserver_cfg_update = 1;
            }
        }
    }

    if (reals.ModbusCmdFlag.sBit.wifi_mul_sta == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_mul_sta = 0;
        if (Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_en > 0)
        {
            if (SetData.dev_info_t.wifi_mul_sta_en.sta1_en
                != Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_en)
            {
                SetData.dev_info_t.wifi_mul_sta_en.sta1_en = Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_en;
                ESP_LOGW(TAG, "wifi mul sta sta1_en: %d", SetData.dev_info_t.wifi_mul_sta_en.sta1_en);
                reals.SetDataWrFlag.sBit.wifi_mul_sta =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_en > 0)
        {
            if (SetData.dev_info_t.wifi_mul_sta_en.sta2_en
                != Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_en)
            {
                SetData.dev_info_t.wifi_mul_sta_en.sta2_en = Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_en;
                ESP_LOGW(TAG, "wifi mul sta sta2_en: %d", SetData.dev_info_t.wifi_mul_sta_en.sta2_en);
                reals.SetDataWrFlag.sBit.wifi_mul_sta =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_en > 0)
        {
            if (SetData.dev_info_t.wifi_mul_sta_en.sta3_en
                != Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_en)
            {
                SetData.dev_info_t.wifi_mul_sta_en.sta3_en = Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_en;
                ESP_LOGW(TAG, "wifi mul sta sta3_en: %d", SetData.dev_info_t.wifi_mul_sta_en.sta3_en);
                reals.SetDataWrFlag.sBit.wifi_mul_sta =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en > 0)
        {
            if (SetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en
                != Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en)
            {
                SetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en = Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en;
                ESP_LOGW(TAG, "wifi mul sta sta1_static_ip_en: %d", SetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en);
                reals.SetDataWrFlag.sBit.wifi_mul_sta =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en > 0)
        {
            if (SetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en != Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en)
            {
                SetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en = Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en;
                ESP_LOGW(TAG, "wifi mul sta sta2_en: %d", SetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en);
                reals.SetDataWrFlag.sBit.wifi_mul_sta =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en > 0)
        {
            if (SetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en != Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en)
            {
                SetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en = Inv_WR.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en;
                ESP_LOGW(TAG, "wifi mul sta sta3_static_ip_en: %d", SetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en);
                reals.SetDataWrFlag.sBit.wifi_mul_sta =1;
                webserver_cfg_update = 1;
            }
        }
    }

    if (reals.ModbusCmdFlag.sBit.wifi_sta1 == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_sta1 = 0;
        if (Inv_WR.mod_reg13600_open.wifi_sta1_ip > 0)
        {
            if (SetData.dev_info_t.wifi_sta1_ip != Inv_WR.mod_reg13600_open.wifi_sta1_ip)
            {
                SetData.dev_info_t.wifi_sta1_ip = Inv_WR.mod_reg13600_open.wifi_sta1_ip;
                ESP_LOGW(TAG, "wifi sta1 IP: %lu", SetData.dev_info_t.wifi_sta1_ip);
                reals.SetDataWrFlag.sBit.wifi_sta1 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta1_mask > 0)
        {
            if (SetData.dev_info_t.wifi_sta1_mask != Inv_WR.mod_reg13600_open.wifi_sta1_mask)
            {
                SetData.dev_info_t.wifi_sta1_mask = Inv_WR.mod_reg13600_open.wifi_sta1_mask;
                ESP_LOGW(TAG, "wifi sta1 netmask: %lu", SetData.dev_info_t.wifi_sta1_mask);
                reals.SetDataWrFlag.sBit.wifi_sta1 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta1_gw > 0)
        {
            if (SetData.dev_info_t.wifi_sta1_gw != Inv_WR.mod_reg13600_open.wifi_sta1_gw)
            {
                SetData.dev_info_t.wifi_sta1_gw = Inv_WR.mod_reg13600_open.wifi_sta1_gw;
                ESP_LOGW(TAG, "wifi sta1 gateway: %lu", SetData.dev_info_t.wifi_sta1_gw);
                reals.SetDataWrFlag.sBit.wifi_sta1 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta1_dns1 > 0)
        {
            if (SetData.dev_info_t.wifi_sta1_dns1 != Inv_WR.mod_reg13600_open.wifi_sta1_dns1)
            {
                SetData.dev_info_t.wifi_sta1_dns1 = Inv_WR.mod_reg13600_open.wifi_sta1_dns1;
                ESP_LOGW(TAG, "wifi sta1 dns1: %lu", SetData.dev_info_t.wifi_sta1_dns1);
                reals.SetDataWrFlag.sBit.wifi_sta1 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta1_dns2 > 0)
        {
            if (SetData.dev_info_t.wifi_sta1_dns2 != Inv_WR.mod_reg13600_open.wifi_sta1_dns2)
            {
                SetData.dev_info_t.wifi_sta1_dns2 = Inv_WR.mod_reg13600_open.wifi_sta1_dns2;
                ESP_LOGW(TAG, "wifi sta1 dns2: %lu", SetData.dev_info_t.wifi_sta1_dns2);
                reals.SetDataWrFlag.sBit.wifi_sta1 =1;
                webserver_cfg_update = 1;
            }
        }
    }

    if (reals.ModbusCmdFlag.sBit.wifi_sta2 == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_sta2 = 0;

        if (SetData.dev_info_t.wifi_sta2_auth != Inv_WR.mod_reg13600_open.wifi_sta2_auth) 
        {
            if (Inv_WR.mod_reg13600_open.wifi_sta2_auth >= WIFI_AUTH_MAX)
            {
                ESP_LOGE(TAG, "sta2 auth(%d) error", Inv_WR.mod_reg13600_open.wifi_sta2_auth);
            }
            else
            {
                SetData.dev_info_t.wifi_sta2_auth = Inv_WR.mod_reg13600_open.wifi_sta2_auth;
                ESP_LOGW(TAG, "wifi sta2 auth change to: %d", SetData.dev_info_t.wifi_sta2_auth);
                // update_cfg = 3;  //需要检测当前连接是否为sta2
                 reals.SetDataWrFlag.sBit.wifi_sta2 = 1;
                 webserver_cfg_update = 1;
            }
        }

        if (strlen(Inv_WR.mod_reg13600_open.wifi_sta2_ssid))
        {
            if (strncmp((char *)SetData.dev_info_t.wifi_sta2_ssid, (char *)Inv_WR.mod_reg13600_open.wifi_sta2_ssid, sizeof(SetData.dev_info_t.wifi_sta2_ssid))) 
            {
                memcpy(SetData.dev_info_t.wifi_sta2_ssid, Inv_WR.mod_reg13600_open.wifi_sta2_ssid, sizeof(SetData.dev_info_t.wifi_sta2_ssid));
                ESP_LOGW(TAG, "wifi sta2 name change to: %s", SetData.dev_info_t.wifi_sta2_ssid);
                // update_cfg = 3;  //需要检测当前连接是否为sta2
                reals.SetDataWrFlag.sBit.wifi_sta2 =1;
                webserver_cfg_update = 1;
            }
        }

        //可以配置空密码
        if (strncmp(SetData.dev_info_t.wifi_sta2_password, Inv_WR.mod_reg13600_open.wifi_sta2_password, sizeof(SetData.dev_info_t.wifi_sta2_password))) 
        {
            memcpy(SetData.dev_info_t.wifi_sta2_password, Inv_WR.mod_reg13600_open.wifi_sta2_password, sizeof(SetData.dev_info_t.wifi_sta2_password));
            ESP_LOGW(TAG, "wifi sta2 password change to: %s", SetData.dev_info_t.wifi_sta2_password);
            // update_cfg = 3;  //需要检测当前连接是否为sta2
            reals.SetDataWrFlag.sBit.wifi_sta2 =1;
            webserver_cfg_update = 1;
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta2_ip > 0)
        {
            if (SetData.dev_info_t.wifi_sta2_ip != Inv_WR.mod_reg13600_open.wifi_sta2_ip)
            {
                SetData.dev_info_t.wifi_sta2_ip = Inv_WR.mod_reg13600_open.wifi_sta2_ip;
                ESP_LOGW(TAG, "wifi sta2 IP: %lu", SetData.dev_info_t.wifi_sta2_ip);
                reals.SetDataWrFlag.sBit.wifi_sta2 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta2_mask > 0)
        {
            if (SetData.dev_info_t.wifi_sta2_mask != Inv_WR.mod_reg13600_open.wifi_sta2_mask)
            {
                SetData.dev_info_t.wifi_sta2_mask = Inv_WR.mod_reg13600_open.wifi_sta2_mask;
                ESP_LOGW(TAG, "wifi sta2 netmask: %lu", SetData.dev_info_t.wifi_sta2_mask);
                reals.SetDataWrFlag.sBit.wifi_sta2 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta2_gw > 0)
        {
            if (SetData.dev_info_t.wifi_sta2_gw != Inv_WR.mod_reg13600_open.wifi_sta2_gw)
            {
                SetData.dev_info_t.wifi_sta2_gw = Inv_WR.mod_reg13600_open.wifi_sta2_gw;
                ESP_LOGW(TAG, "wifi sta2 gateway: %lu", SetData.dev_info_t.wifi_sta2_gw);
                reals.SetDataWrFlag.sBit.wifi_sta2 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta2_dns1 > 0)
        {
            if (SetData.dev_info_t.wifi_sta2_dns1 != Inv_WR.mod_reg13600_open.wifi_sta2_dns1)
            {
                SetData.dev_info_t.wifi_sta2_dns1 = Inv_WR.mod_reg13600_open.wifi_sta2_dns1;
                ESP_LOGW(TAG, "wifi sta2 dns1: %lu", SetData.dev_info_t.wifi_sta2_dns1);
                reals.SetDataWrFlag.sBit.wifi_sta2 =1;
                webserver_cfg_update = 1;
            }
        }

        // if (Inv_WR.mod_reg13600_open.wifi_sta1_dns2 > 0) // DNS2允许设置为0，即只使用DNS1
        {
            if (SetData.dev_info_t.wifi_sta2_dns2 != Inv_WR.mod_reg13600_open.wifi_sta2_dns2)
            {
                SetData.dev_info_t.wifi_sta2_dns2 = Inv_WR.mod_reg13600_open.wifi_sta2_dns2;
                ESP_LOGW(TAG, "wifi sta2 dns2: %lu", SetData.dev_info_t.wifi_sta2_dns2);
                reals.SetDataWrFlag.sBit.wifi_sta2 =1;
                webserver_cfg_update = 1;
            }
        }
    }

    if (reals.ModbusCmdFlag.sBit.wifi_sta3 == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_sta3 = 0;

        if (SetData.dev_info_t.wifi_sta3_auth != Inv_WR.mod_reg13600_open.wifi_sta3_auth) 
        {
            if (Inv_WR.mod_reg13600_open.wifi_sta3_auth >= WIFI_AUTH_MAX)
            {
                ESP_LOGE(TAG, "sta3 auth(%d) error", Inv_WR.mod_reg13600_open.wifi_sta3_auth);
            }
            else
            {
                SetData.dev_info_t.wifi_sta3_auth = Inv_WR.mod_reg13600_open.wifi_sta3_auth;
                ESP_LOGW(TAG, "wifi sta3 auth change to: %d", SetData.dev_info_t.wifi_sta3_auth);
                // update_cfg = 3;  //需要检测当前连接是否为sta3
                reals.SetDataWrFlag.sBit.wifi_sta3 =1;
                webserver_cfg_update = 1;
            }
        }

        if (strlen(Inv_WR.mod_reg13600_open.wifi_sta3_ssid))
        {
            if (strncmp(SetData.dev_info_t.wifi_sta3_ssid, Inv_WR.mod_reg13600_open.wifi_sta3_ssid,
                            sizeof(SetData.dev_info_t.wifi_sta3_ssid))) 
            {
                memcpy(SetData.dev_info_t.wifi_sta3_ssid, Inv_WR.mod_reg13600_open.wifi_sta3_ssid,
                            sizeof(SetData.dev_info_t.wifi_sta3_ssid));
                ESP_LOGW(TAG, "wifi sta3 name change to: %s", SetData.dev_info_t.wifi_sta3_ssid);
                // update_cfg = 3;  //需要检测当前连接是否为sta3
                reals.SetDataWrFlag.sBit.wifi_sta3 =1;
                webserver_cfg_update = 1;
            }
        }

        //可以配置空密码
        if (strncmp(SetData.dev_info_t.wifi_sta3_password, Inv_WR.mod_reg13600_open.wifi_sta3_password,
                        sizeof(SetData.dev_info_t.wifi_sta3_password))) 
        {
            memcpy(SetData.dev_info_t.wifi_sta3_password, Inv_WR.mod_reg13600_open.wifi_sta3_password,
                        sizeof(SetData.dev_info_t.wifi_sta3_password));
            ESP_LOGW(TAG, "wifi sta3 password change to: %s", SetData.dev_info_t.wifi_sta3_password);
            // update_cfg = 3;  //需要检测当前连接是否为sta2
            reals.SetDataWrFlag.sBit.wifi_sta3 =1;
            webserver_cfg_update = 1;
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta3_ip > 0)
        {
            if (SetData.dev_info_t.wifi_sta3_ip != Inv_WR.mod_reg13600_open.wifi_sta3_ip)
            {
                SetData.dev_info_t.wifi_sta3_ip = Inv_WR.mod_reg13600_open.wifi_sta3_ip;
                ESP_LOGW(TAG, "wifi sta3 IP: %lu", SetData.dev_info_t.wifi_sta3_ip);
                reals.SetDataWrFlag.sBit.wifi_sta3 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta3_mask > 0)
        {
            if (SetData.dev_info_t.wifi_sta3_mask != Inv_WR.mod_reg13600_open.wifi_sta3_mask)
            {
                SetData.dev_info_t.wifi_sta3_mask = Inv_WR.mod_reg13600_open.wifi_sta3_mask;
                ESP_LOGW(TAG, "wifi sta3 netmask: %lu", SetData.dev_info_t.wifi_sta3_mask);
                reals.SetDataWrFlag.sBit.wifi_sta3 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta3_gw > 0)
        {
            if (SetData.dev_info_t.wifi_sta3_gw != Inv_WR.mod_reg13600_open.wifi_sta3_gw)
            {
                SetData.dev_info_t.wifi_sta3_gw = Inv_WR.mod_reg13600_open.wifi_sta3_gw;
                ESP_LOGW(TAG, "wifi sta3 gateway: %lu", SetData.dev_info_t.wifi_sta3_gw);
                reals.SetDataWrFlag.sBit.wifi_sta3 =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.wifi_sta3_dns1 > 0)
        {
            if (SetData.dev_info_t.wifi_sta3_dns1 != Inv_WR.mod_reg13600_open.wifi_sta3_dns1)
            {
                SetData.dev_info_t.wifi_sta3_dns1 = Inv_WR.mod_reg13600_open.wifi_sta3_dns1;
                ESP_LOGW(TAG, "wifi sta3 dns1: %lu", SetData.dev_info_t.wifi_sta3_dns1);
                reals.SetDataWrFlag.sBit.wifi_sta3 =1;
                webserver_cfg_update = 1;
            }
        }

        // if (Inv_WR.mod_reg13600_open.wifi_sta3_dns2 > 0)  // DNS2允许设置为0，即只使用DNS1
        {
            if (SetData.dev_info_t.wifi_sta3_dns2 != Inv_WR.mod_reg13600_open.wifi_sta3_dns2)
            {
                SetData.dev_info_t.wifi_sta3_dns2 = Inv_WR.mod_reg13600_open.wifi_sta3_dns2;
                ESP_LOGW(TAG, "wifi sta3 dns2: %lu", SetData.dev_info_t.wifi_sta3_dns2);
                reals.SetDataWrFlag.sBit.wifi_sta3 =1;
                webserver_cfg_update = 1;
            }
        }
    }

    if (reals.ModbusCmdFlag.sBit.wifi_ap == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_ap = 0;

        if (SetData.dev_info_t.wifi_ap_auth != Inv_WR.mod_reg12000_IOT_set.wifi_ap_auth) 
        {
            if (Inv_WR.mod_reg12000_IOT_set.wifi_ap_auth >= WIFI_AUTH_MAX)
            {
                ESP_LOGE(TAG, "wifi ap auth(%d) error", Inv_WR.mod_reg12000_IOT_set.wifi_ap_auth);
            }
            else
            {
                SetData.dev_info_t.wifi_ap_auth = Inv_WR.mod_reg12000_IOT_set.wifi_ap_auth;
                ESP_LOGW(TAG, "wifi ap auth change to: %d", SetData.dev_info_t.wifi_ap_auth);
                update_cfg = 4;
                reals.SetDataWrFlag.sBit.wifi_ap =1;
                webserver_cfg_update = 1;
            }
        }

        if (strlen(Inv_WR.mod_reg12000_IOT_set.wifi_AP_ssid))
        {
            if (strncmp(SetData.dev_info_t.wifi_ap_ssid, Inv_WR.mod_reg12000_IOT_set.wifi_AP_ssid,
                            sizeof(SetData.dev_info_t.wifi_ap_ssid))) 
            {
                memcpy(SetData.dev_info_t.wifi_ap_ssid, Inv_WR.mod_reg12000_IOT_set.wifi_AP_ssid,
                            sizeof(SetData.dev_info_t.wifi_ap_ssid));
                ESP_LOGW(TAG, "wifi ap ssid change to: %s", SetData.dev_info_t.wifi_ap_ssid);
                update_cfg = 4;
                reals.SetDataWrFlag.sBit.wifi_ap =1;
                webserver_cfg_update = 1;
            }
        }

        //可以配置空密码
        if (strncmp(SetData.dev_info_t.wifi_ap_password, Inv_WR.mod_reg12000_IOT_set.wifi_AP_password,
                        sizeof(SetData.dev_info_t.wifi_ap_password))) 
        {
            memcpy(SetData.dev_info_t.wifi_ap_password, Inv_WR.mod_reg12000_IOT_set.wifi_AP_password,
                        sizeof(SetData.dev_info_t.wifi_ap_password));
            ESP_LOGW(TAG, "wifi ap password change to: %s", SetData.dev_info_t.wifi_ap_password);
            update_cfg = 4;
            reals.SetDataWrFlag.sBit.wifi_ap =1;
            webserver_cfg_update = 1;
        }
    }

    if (reals.ModbusCmdFlag.sBit.mqtt == 1)
    {
        reals.ModbusCmdFlag.sBit.mqtt = 0;

        if (Inv_WR.mod_reg13600_open.open_mqtt_enable.enable > 0)
        {
            SetData.dev_info_t.open_mqtt_enable.enable = Inv_WR.mod_reg13600_open.open_mqtt_enable.enable;
            reals.SetDataWrFlag.sBit.mqtt =1;
            webserver_cfg_update = 1;
        }
        if (Inv_WR.mod_reg13600_open.open_mqtt_enable.crypt_en > 0)
        {
            SetData.dev_info_t.open_mqtt_enable.crypt_en = Inv_WR.mod_reg13600_open.open_mqtt_enable.crypt_en;
            reals.SetDataWrFlag.sBit.mqtt =1;
            webserver_cfg_update = 1;
        }
        if (Inv_WR.mod_reg13600_open.open_mqtt_enable.ctrl_en > 0)
        {
            SetData.dev_info_t.open_mqtt_enable.ctrl_en = Inv_WR.mod_reg13600_open.open_mqtt_enable.ctrl_en;
            reals.SetDataWrFlag.sBit.mqtt =1;
            webserver_cfg_update = 1;
        }
        if (Inv_WR.mod_reg13600_open.open_mqtt_enable.report_en > 0)
        {
            SetData.dev_info_t.open_mqtt_enable.report_en = Inv_WR.mod_reg13600_open.open_mqtt_enable.report_en;
            reals.SetDataWrFlag.sBit.mqtt =1;
            webserver_cfg_update = 1;
        }

        if (strlen(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address))
        {
            if (strncmp(SetData.dev_info_t.Net_Server_address, Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address,
                            sizeof(SetData.dev_info_t.Net_Server_address))) 
            {
                memcpy(SetData.dev_info_t.Net_Server_address, Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address,
                            sizeof(SetData.dev_info_t.Net_Server_address));
                ESP_LOGW(TAG, "mqtt address change to: %s", SetData.dev_info_t.Net_Server_address);
                //TODO: 根据具体使能位变化, 决定是否需要重连MQTT
                reals.SetDataWrFlag.sBit.mqtt =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.open_mqtt_report_cycle > 0)
        {
            if (SetData.dev_info_t.open_mqtt_report_cycle != Inv_WR.mod_reg13600_open.open_mqtt_report_cycle)
            {
                SetData.dev_info_t.open_mqtt_report_cycle = Inv_WR.mod_reg13600_open.open_mqtt_report_cycle;
                ESP_LOGW(TAG, "open mqtt report cycle change to: %u", SetData.dev_info_t.open_mqtt_report_cycle);
                reals.SetDataWrFlag.sBit.mqtt =1;
                webserver_cfg_update = 1;
            }
        }
    }

    if (reals.ModbusCmdFlag.sBit.modbus_tcp == 1)
    {
        reals.ModbusCmdFlag.sBit.modbus_tcp = 0;

        if (Inv_WR.mod_reg13600_open.modbus_tcp_enable.all > 0)
        {
            SetData.dev_info_t.modbus_tcp_enable.all = Inv_WR.mod_reg13600_open.modbus_tcp_enable.all;
            //TODO: 根据具体使能位变化, 决定是否需要重连MQTT
            reals.SetDataWrFlag.sBit.modbus_tcp =1;
            webserver_cfg_update = 1;
        }

        if (Inv_WR.mod_reg13600_open.modbus_tcp_enable.enable > 0)
        {
            if (SetData.dev_info_t.modbus_tcp_enable.enable != Inv_WR.mod_reg13600_open.modbus_tcp_enable.enable)
            {
                SetData.dev_info_t.modbus_tcp_enable.enable = Inv_WR.mod_reg13600_open.modbus_tcp_enable.enable;
                ESP_LOGW(TAG, "modbus tcp enable change to: %u", SetData.dev_info_t.modbus_tcp_enable.enable);
                reals.SetDataWrFlag.sBit.modbus_tcp =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.modbus_tcp_enable.crypt_en > 0)
        {
            if (SetData.dev_info_t.modbus_tcp_enable.crypt_en != Inv_WR.mod_reg13600_open.modbus_tcp_enable.crypt_en)
            {
                SetData.dev_info_t.modbus_tcp_enable.crypt_en = Inv_WR.mod_reg13600_open.modbus_tcp_enable.crypt_en;
                ESP_LOGW(TAG, "modbus tcp crypt_en change to: %u", SetData.dev_info_t.modbus_tcp_enable.crypt_en);
                reals.SetDataWrFlag.sBit.modbus_tcp =1;
                webserver_cfg_update = 1;
            }
        }

        if (Inv_WR.mod_reg13600_open.modbus_tcp_port > 0)
        {
            if (SetData.dev_info_t.modbus_tcp_port != Inv_WR.mod_reg13600_open.modbus_tcp_port)
            {
                SetData.dev_info_t.modbus_tcp_port = Inv_WR.mod_reg13600_open.modbus_tcp_port;
                ESP_LOGW(TAG, "modbus tcp port change to: %u", SetData.dev_info_t.modbus_tcp_port);
                reals.SetDataWrFlag.sBit.modbus_tcp =1;
                webserver_cfg_update = 1;
            }
        }

        if (reals.SetDataWrFlag.sBit.modbus_tcp == 1)
        {
            if (1 == SetData.dev_info_t.modbus_tcp_enable.enable)
            {
                if (true == modbus_tcp_is_running())
                {
                    modbus_tcp_restart(SetData.dev_info_t.modbus_tcp_enable.crypt_en, SetData.dev_info_t.modbus_tcp_port, 0);
                }
                else
                {
                    modbus_tcp_start(SetData.dev_info_t.modbus_tcp_enable.crypt_en, SetData.dev_info_t.modbus_tcp_port, 0);
                }
            }
            else
            {
                modbus_tcp_stop();
            }
        }
    }

    // 配置发生改变，主动通知web更新页面
    if (webserver_cfg_update)
    {
        web_server_config_push();
    }

	if (reals.ModbusCmdFlag.sBit.IOT_Enable_mix1 == 1)
	{
		reals.ModbusCmdFlag.sBit.IOT_Enable_mix1 = 0;
	}	
	if (reals.ModbusCmdFlag.sBit.IOT_Enable_mix2 == 1)
	{
		reals.ModbusCmdFlag.sBit.IOT_Enable_mix2 = 0;
	}

	if (reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 == 1)
	{
		reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 = 0;
	}	
	if (reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 == 1)
	{
		reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 = 0;
	}

if(sAddr_can_master_old != reals.Addr_can_master)//主从切换时候，需要重新配置WIFI STA
{
	ESP_LOGW(TAG,"sAddr_can_master_old != reals.Addr_can_master");
	if (sAddr_can_master_old != 0 || reals.Addr_can_master == 0) {
		update_cfg = 3;
	} else {
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
		if (reals.Addr_can_master == 1 && false == is_root_node()) {
			set_mesh_node_type(MESH_NODE_TYPE_ROOT);
		}
#endif
	}
	sAddr_can_master_old = reals.Addr_can_master;
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
	reals.wifi_mesh_root = reals.Addr_can_master;
#endif
}
	
    if (reals.ModbusCmdFlag.sBit.meter_cfg0 == 1)
    {
        reals.ModbusCmdFlag.sBit.meter_cfg0 = 0;

        if (memcmp(Meter_wr.mod_reg01900_meter.dev_id, SetData.dev_info_t.meter_cfg[0].dev_id,
                    sizeof(Meter_wr.mod_reg01900_meter.dev_id)))
        {
            memcpy(SetData.dev_info_t.meter_cfg[0].dev_id, Meter_wr.mod_reg01900_meter.dev_id,
                    sizeof(SetData.dev_info_t.meter_cfg[0].dev_id));
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.dev_type != 0)
            && (Meter_wr.mod_reg01900_meter.dev_type != SetData.dev_info_t.meter_cfg[0].dev_type))
        {
            SetData.dev_info_t.meter_cfg[0].dev_type = Meter_wr.mod_reg01900_meter.dev_type;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.mfg_id != 0)
            && (Meter_wr.mod_reg01900_meter.mfg_id != SetData.dev_info_t.meter_cfg[0].mfg_id))
        {
            SetData.dev_info_t.meter_cfg[0].mfg_id = Meter_wr.mod_reg01900_meter.mfg_id;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.func != 0)
            && (Meter_wr.mod_reg01900_meter.func != SetData.dev_info_t.meter_cfg[0].func))
        {
            SetData.dev_info_t.meter_cfg[0].func = Meter_wr.mod_reg01900_meter.func;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }
    }
#if (METER_MAX_NUM >= 2)
    if (reals.ModbusCmdFlag.sBit.meter_cfg1 == 1)
    {
        reals.ModbusCmdFlag.sBit.meter_cfg1 = 0;

        if (memcmp(Meter_wr.mod_reg01900_meter.dev_id, SetData.dev_info_t.meter_cfg[1].dev_id,
                    sizeof(Meter_wr.mod_reg01900_meter.dev_id)))
        {
            memcpy(SetData.dev_info_t.meter_cfg[1].dev_id, Meter_wr.mod_reg01900_meter.dev_id,
                    sizeof(SetData.dev_info_t.meter_cfg[1].dev_id));
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.dev_type != 0)
            && (Meter_wr.mod_reg01900_meter.dev_type != SetData.dev_info_t.meter_cfg[1].dev_type))
        {
            SetData.dev_info_t.meter_cfg[1].dev_type = Meter_wr.mod_reg01900_meter.dev_type;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.mfg_id != 0)
            && (Meter_wr.mod_reg01900_meter.mfg_id != SetData.dev_info_t.meter_cfg[1].mfg_id))
        {
            SetData.dev_info_t.meter_cfg[1].mfg_id = Meter_wr.mod_reg01900_meter.mfg_id;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.func != 0)
            && (Meter_wr.mod_reg01900_meter.func != SetData.dev_info_t.meter_cfg[1].func))
        {
            SetData.dev_info_t.meter_cfg[1].func = Meter_wr.mod_reg01900_meter.func;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }
    }
#endif

#if (METER_MAX_NUM >= 3)
    if (reals.ModbusCmdFlag.sBit.meter_cfg2 == 1)
    {
        reals.ModbusCmdFlag.sBit.meter_cfg2 = 0;

        if (memcmp(Meter_wr.mod_reg01900_meter.dev_id, SetData.dev_info_t.meter_cfg[2].dev_id,
                    sizeof(Meter_wr.mod_reg01900_meter.dev_id)))
        {
            memcpy(SetData.dev_info_t.meter_cfg[2].dev_id, Meter_wr.mod_reg01900_meter.dev_id,
                    sizeof(SetData.dev_info_t.meter_cfg[1].dev_id));
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.dev_type != 0)
            && (Meter_wr.mod_reg01900_meter.dev_type != SetData.dev_info_t.meter_cfg[2].dev_type))
        {
            SetData.dev_info_t.meter_cfg[2].dev_type = Meter_wr.mod_reg01900_meter.dev_type;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.mfg_id != 0)
            && (Meter_wr.mod_reg01900_meter.mfg_id != SetData.dev_info_t.meter_cfg[2].mfg_id))
        {
            SetData.dev_info_t.meter_cfg[2].mfg_id = Meter_wr.mod_reg01900_meter.mfg_id;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }

        if ((Meter_wr.mod_reg01900_meter.func != 0)
            && (Meter_wr.mod_reg01900_meter.func != SetData.dev_info_t.meter_cfg[2].func))
        {
            SetData.dev_info_t.meter_cfg[2].func = Meter_wr.mod_reg01900_meter.func;
            reals.SetDataWrFlag.sBit.meter_cfg =1;
        }
    }
#endif
    // if (reals.ModbusCmdFlag.sBit.meter_cfg3 != 1)
    // {
    //     reals.ModbusCmdFlag.sBit.meter_cfg3 = 0;

    //     if (memcmp(Meter_wr.mod_reg01900_meter.dev_id, SetData.dev_info_t.meter_cfg[3].dev_id,
    //                 sizeof(Meter_wr.mod_reg01900_meter.dev_id)))
    //     {
    //         memcpy(SetData.dev_info_t.meter_cfg[3].dev_id, Meter_wr.mod_reg01900_meter.dev_id,
    //                 sizeof(SetData.dev_info_t.meter_cfg[1].dev_id));
    //         reals.SetDataWrFlag.sBit.meter_cfg =1;
    //     }

    //     if ((Meter_wr.mod_reg01900_meter.dev_type != 0)
    //         && (Meter_wr.mod_reg01900_meter.dev_type != SetData.dev_info_t.meter_cfg[3].dev_type))
    //     {
    //         SetData.dev_info_t.meter_cfg[3].dev_type = Meter_wr.mod_reg01900_meter.dev_type;
    //         reals.SetDataWrFlag.sBit.meter_cfg =1;
    //     }

    //     if ((Meter_wr.mod_reg01900_meter.mfg_id != 0)
    //         && (Meter_wr.mod_reg01900_meter.mfg_id != SetData.dev_info_t.meter_cfg[3].mfg_id))
    //     {
    //         SetData.dev_info_t.meter_cfg[3].mfg_id = Meter_wr.mod_reg01900_meter.mfg_id;
    //         reals.SetDataWrFlag.sBit.meter_cfg =1;
    //     }

    //     if ((Meter_wr.mod_reg01900_meter.func != 0)
    //         && (Meter_wr.mod_reg01900_meter.func != SetData.dev_info_t.meter_cfg[3].func))
    //     {
    //         SetData.dev_info_t.meter_cfg[3].func = Meter_wr.mod_reg01900_meter.func;
    //         reals.SetDataWrFlag.sBit.meter_cfg =1;
    //     }
    // }

	// wifi参数发生变化后重启WiFi与网络
	if (3 == update_cfg) 
	{
		update_cfg = 0;

		if (wifi_sta_cfg_changed) {
			/* iot_wifi_reconnect_sta 已在上面执行 */
		}
		else if(1 == reals.Addr_can_master)
		{
			if (1 == SetData.dev_info_t.on_off.bit.wifi_enable)
			{
                ESP_LOGW(TAG, "Addr_can_master SetData.dev_info_t.on_off.bit.wifi_enable");
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
                //从普通节点切换到root节点
                if (false == is_root_node())
                {
                    ESP_LOGW(TAG, "Addr_can_master switch to MESH_NODE_TYPE_ROOT");
                    esp_wifi_scan_stop();	
					vTaskDelay(pdMS_TO_TICKS(1000));			
                    set_mesh_node_type(MESH_NODE_TYPE_ROOT);
                }
				iot_wifi_new_ap(SetData.dev_info_t.wifi_sta_auth, SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password, NULL);
#else
                iot_wifi_new_ap(SetData.dev_info_t.wifi_sta_auth, SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password);
#endif
				ESP_LOGW(TAG, "Net_Server_address: %s",SetData.dev_info_t.Net_Server_address);
			}
		}
		else
		{
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
            // 从root节点切换为普通节点
            if (true == is_root_node())
            {
                ESP_LOGW(TAG, "Addr_can_master switch to MESH_NODE_TYPE_COMMON");
                set_mesh_node_type(MESH_NODE_TYPE_COMMON);
                // iot_wifi_new_ap(WIFI_AUTH_WPA2_PSK, wifi_sta_ssid, wifi_sta_password, NULL);
                            // 必须要在wifi start和connect之间扫描wifi
                esp_wifi_scan_stop();
                esp_wifi_disconnect();
                // vTaskDelay(pdMS_TO_TICKS(3000));
                esp_wifi_scan_start(get_wifi_scan_config(), false);
            }
#else
			/* 蓝牙配网时 Addr_can_master 可能尚未置位，仍用用户 WiFi 配置发起连接 */
			if (!wifi_sta_cfg_changed
				&& (1 == SetData.dev_info_t.on_off.bit.wifi_enable)
				&& (strlen(SetData.dev_info_t.wifi_sta_ssid) > 0))
			{
				iot_wifi_new_ap(SetData.dev_info_t.wifi_sta_auth,
				                SetData.dev_info_t.wifi_sta_ssid,
				                SetData.dev_info_t.wifi_sta_password);
			}
#endif
//			iot_wifi_new_ap(SetData.dev_info_t.wifi_sta_auth, SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password);
			
		}
		if(sAddr_can_master_old)
		{
			link_report_set(1);
			ESP_LOGI(TAG,"master 1 reals.dev_change_ctrl.bit.link_report:%d",reals.dev_change_ctrl.bit.link_report);
		}
	}
    else if (4 == update_cfg)
    {
        update_cfg = 0;
        if (1 == SetData.dev_info_t.on_off.bit.wifi_ap_enable)
        {
            iot_wifi_ap_config(SetData.dev_info_t.wifi_ap_auth, SetData.dev_info_t.wifi_ap_ssid,
                                SetData.dev_info_t.wifi_ap_password);
        }
    }

	if (reals.ModbusCmdFlag.sBit.new_cfg == 1)
	{
		reals.ModbusCmdFlag.sBit.new_cfg = 0;


	}


	if (reals.ModbusCmdFlag.sBit.Net_Server_address == 1)
	{
		reals.ModbusCmdFlag.sBit.Net_Server_address = 0;
		if (strlen(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address)) 
		{//
			if (strncmp(SetData.dev_info_t.Net_Server_address,Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address, sizeof(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address))) 
			{
				memcpy(SetData.dev_info_t.Net_Server_address, Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address, sizeof(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address));
				//memcpy(g_self_data.mod_reg22000_net_server_2rd.Net_Server_address, g_self_data_WR.mod_reg22000_net_server_2rd.Net_Server_address, sizeof(g_self_data_WR.mod_reg22000_net_server_2rd.Net_Server_address));
				ESP_LOGW(TAG, "Net_Server_address: %s",SetData.dev_info_t.Net_Server_address);
				update_cfg = 2;
				reals.SetDataWrFlag.sBit.Net_Server_address =1;
				server2_config_info_update();
		
			}
		}

	}
	
	if (reals.ModbusCmdFlag.sBit.Net_Server_secret == 1)
	{
		reals.ModbusCmdFlag.sBit.Net_Server_secret = 0;

		if (strlen(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret)) 
		{//
			if (strncmp(SetData.dev_info_t.Net_Server_secret,Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret, sizeof(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret))) 
			{
				memcpy(SetData.dev_info_t.Net_Server_secret, Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret, sizeof(Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret));
				//memcpy(g_self_data.mod_reg22000_net_server_2rd.Net_Server_secret, g_self_data_WR.mod_reg22000_net_server_2rd.Net_Server_secret, sizeof(g_self_data_WR.mod_reg22000_net_server_2rd.Net_Server_secret));
				ESP_LOGW(TAG, "Net_Server_secret: %s", Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret);
				update_cfg = 2;
				reals.SetDataWrFlag.sBit.Net_Server_secret =1;
				server2_config_info_update();
			}
		}

	}	

	
	if (1 == reals.ModbusCmdFlag.sBit.cfg_22000)
	{
		reals.ModbusCmdFlag.sBit.cfg_22000 = 0;

		
	}
	if (1 == reals.ModbusCmdFlag.sBit.app_password)
	{
		reals.ModbusCmdFlag.sBit.app_password = 0;
		app_password_check();

		
	}
	update_cfg = 0;

    /*检查并执行电源控制指令*/
    if ( 1 == reals.ModbusCmdFlag.sBit.ctrl_power ) {
        reals.ModbusCmdFlag.sBit.ctrl_power = 0;
        iot_power_ctrl_check();
    }
}

/*
modbus beta 2006 reg:
能量清除命令
*/
static void iot_ctrl_2006_check(void)
{

	uint8_t i=0;
// reset set
//	if(SetData.dev_info_t.ctrl_2006.bit.clear_all)
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_all)
	{
		memset((uint8_t *)&(reals.energy_DCLoad_Total[0]), 0, sizeof(reals.energy_DCLoad_Total)); //  
		memset((uint8_t *)&(reals.energy_ACLoad_Total[0]), 0, sizeof(reals.energy_ACLoad_Total)); //  
		memset((uint8_t *)&(reals.energy_Pv_dc[0]), 0, sizeof(reals.energy_Pv_dc)); //  
		memset((uint8_t *)&(reals.energy_GridChgin_Total[0]), 0, sizeof(reals.energy_GridChgin_Total)); //  
		memset((uint8_t *)&(reals.energy_GridFeedback_Total[0]), 0, sizeof(reals.energy_GridFeedback_Total)); //  
		memset((uint8_t *)&(reals.energy_PVToload_Total[0]), 0, sizeof(reals.energy_PVToload_Total)); //  
		memset((uint8_t *)&(reals.energy_PackDsg_Total[0]), 0, sizeof(reals.energy_PackDsg_Total)); //  


		memset((uint8_t *)&(SetData.dev_info_t.iot_kwh_info.KWH_all[0]),0,sizeof(SetData.dev_info_t.iot_kwh_info.KWH_all));
		memset((uint8_t *)&(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[0]),0,sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year));
		memset((uint8_t *)&(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[0]),0,sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month));
		memset((uint8_t *)&(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[0]),0,sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day));
		memset((uint8_t *)&(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[0]),0,sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour));
		SetData.dev_info_t.iot_kwh_info.kwh_years_num=0;
/*
			SetData.dev_info_t.iot_kwh_info.KWH_all[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[i].DCLoad_Total =0;

			SetData.dev_info_t.iot_kwh_info.KWH_all[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[i].ACLoad_Total =0;

			SetData.dev_info_t.iot_kwh_info.KWH_all[i].Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year.Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month.Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day.Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour.Pv_dc =0;

			SetData.dev_info_t.iot_kwh_info.KWH_all.GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year.GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month.GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day.GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour.GridChgin_Total =0;

			SetData.dev_info_t.iot_kwh_info.KWH_all.GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year.GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month.GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day.GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour.GridFeedback_Total =0;

			SetData.dev_info_t.iot_kwh_info.KWH_all.PVToload_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year.PVToload_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month.PVToload_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day.PVToload_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour.PVToload_Total =0;

			SetData.dev_info_t.iot_kwh_info.KWH_all.PackDsg_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year.PackDsg_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month.PackDsg_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day.PackDsg_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour.PackDsg_Total =0;
*/

		reals.kwhWrFlag.sBit.year_DCLoad_Total =1;
		reals.kwhWrFlag.sBit.year_ACLoad_Total =1;
		reals.kwhWrFlag.sBit.year_Pv_Total =1;
		reals.kwhWrFlag.sBit.year_GridChgin_Total =1;
		reals.kwhWrFlag.sBit.year_GridFeedback_Total =1;
		reals.kwhWrFlag.sBit.year_PVToload_Total =1;
		reals.kwhWrFlag.sBit.year_PackDsg_Total =1;
		reals.kwhWrFlag.sBit.hour_file =1;

		reals.kwhWrFlag.sBit.year_DCLoad_Total_Sum =1;
		reals.kwhWrFlag.sBit.year_ACLoad_Total_Sum =1;
		reals.kwhWrFlag.sBit.year_Pv_Total_Sum =1;
		reals.kwhWrFlag.sBit.year_GridChgin_Total_Sum =1;
		reals.kwhWrFlag.sBit.year_GridFeedback_Total_Sum =1;
		reals.kwhWrFlag.sBit.year_PVToload_Total_Sum =1;
		reals.kwhWrFlag.sBit.year_PackDsg_Total_Sum =1;
		reals.kwhWrFlag.sBit.hour_file_Sum =1;

		
		reals.SetDataWrFlag.sBit.kwh_mix =1;	
	
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_all =0;

		reals.SetDataWrFlag.sBit.SetDataUpdate_all =1;

		ESP_LOGI(TAG,"clear_kwh_file");
		ESP_LOGI(TAG,"1iot_period_task2006 ctrl:0x%x",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.all);
		clear_kwh_file();
		ESP_LOGI(TAG,"2 iot_period_task2006 ctrl:0x%x",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.all);

	}
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_dc)
	{

		memset((uint8_t *)&reals.energy_DCLoad_Total[0], 0, sizeof(reals.energy_DCLoad_Total)); // 
		for(i=0;i<KWH_INFO_MAX_NUM;i++)
		{ 
			SetData.dev_info_t.iot_kwh_info.KWH_all[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[i].DCLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[i].DCLoad_Total =0;
		}

		reals.kwhWrFlag.sBit.hour_file =1;
		reals.kwhWrFlag.sBit.year_DCLoad_Total =1;
		reals.kwhWrFlag.sBit.hour_file_Sum =1;		
		reals.kwhWrFlag.sBit.year_DCLoad_Total_Sum =1;
		reals.SetDataWrFlag.sBit.kwh_mix =1;	
	
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_dc =0;

	}
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_ac)
	{
		memset((uint8_t *)&reals.energy_ACLoad_Total, 0, sizeof(reals.energy_ACLoad_Total)); //  
		for(i=0;i<KWH_INFO_MAX_NUM;i++)
		{ 
			SetData.dev_info_t.iot_kwh_info.KWH_all[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[i].ACLoad_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[i].ACLoad_Total =0;
		}

		reals.kwhWrFlag.sBit.hour_file =1;
		reals.kwhWrFlag.sBit.year_ACLoad_Total =1;
		reals.kwhWrFlag.sBit.hour_file_Sum =1;
		reals.kwhWrFlag.sBit.year_ACLoad_Total_Sum =1;
		reals.SetDataWrFlag.sBit.kwh_mix =1;	

		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_ac =0;

	}
	
	
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_pv)
	{
		memset((uint8_t *)&reals.energy_Pv_dc, 0, sizeof(reals.energy_Pv_dc)); //
		for(i=0;i<KWH_INFO_MAX_NUM;i++)
		{ 		  
			SetData.dev_info_t.iot_kwh_info.KWH_all[i].Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[i].Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[i].Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[i].Pv_dc =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[i].Pv_dc =0;
		}
		reals.kwhWrFlag.sBit.hour_file =1;
		reals.kwhWrFlag.sBit.year_Pv_Total =1;
		reals.kwhWrFlag.sBit.hour_file_Sum =1;
		reals.kwhWrFlag.sBit.year_Pv_Total_Sum =1;
		reals.SetDataWrFlag.sBit.kwh_mix =1;	
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_pv =0;

	}
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_chg)
	{
		memset((uint8_t *)&reals.energy_GridChgin_Total, 0, sizeof(reals.energy_GridChgin_Total)); // 
		for(i=0;i<KWH_INFO_MAX_NUM;i++)
		{ 			 
			SetData.dev_info_t.iot_kwh_info.KWH_all[i].GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[i].GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[i].GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[i].GridChgin_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[i].GridChgin_Total =0;
		}

		reals.kwhWrFlag.sBit.hour_file =1;
		reals.kwhWrFlag.sBit.year_GridChgin_Total =1;
		reals.kwhWrFlag.sBit.hour_file_Sum =1;
		reals.kwhWrFlag.sBit.year_GridChgin_Total_Sum =1;
		reals.SetDataWrFlag.sBit.kwh_mix =1;		
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_chg =0;
	}
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_dsg)
	{
		memset((uint8_t *)&reals.energy_GridFeedback_Total, 0, sizeof(reals.energy_GridFeedback_Total)); // 
		for(i=0;i<KWH_INFO_MAX_NUM;i++)
		{ 		 
			SetData.dev_info_t.iot_kwh_info.KWH_all[i].GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[i].GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[i].GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[i].GridFeedback_Total =0;
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[i].GridFeedback_Total =0;
		}

		reals.kwhWrFlag.sBit.hour_file =1;
		reals.kwhWrFlag.sBit.year_GridFeedback_Total =1;
		reals.kwhWrFlag.sBit.hour_file_Sum =1;
		reals.kwhWrFlag.sBit.year_GridFeedback_Total_Sum =1;
		reals.SetDataWrFlag.sBit.kwh_mix =1;	
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_dsg =0;
	}
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_log)
	{
        ESP_LOGE (TAG, "***********Clear log history!*************");
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_log =0;
		SetData.dev_info_t.historyRecSaveCount= 0;//log记录条数 
		SetData.dev_info_t.historyAddrIndex= 0;//log记录条数 
		reals.historyRecSaveCount = 0;
		reals.historyAddrIndex = 0;
		
		reals.SetDataWrFlag.sBit.SetDataUpdate_historycnt = 1;
		
		reals.SetDataWrFlag.sBit.SetDataUpdate_Event_cnt = 0;
		clear_log_file();
	}
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_fault)
	{
	
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_fault =0;
	}
	if(Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_chg_dsg_time_period)
	{
	
		Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_chg_dsg_time_period =0;
	}
//	if(g_self_data_WR.mod_reg02000_Inv_base_set.ctrl_reset_factory)
//	{
//	
//		g_self_data_WR.mod_reg02000_Inv_base_set.ctrl_reset_factory =0;
//	}
//
//
//	if(g_self_data_WR.mod_reg02000_Inv_base_set.ctrl_mix.bit.reset_param)
//	{
//	
//		SetData_Can.dev_info_t2.inv_set01.ctrl_mix.bit.reset_param =0;
//	}
}

//static void iot_factory_to_can (void) 
//{
//    g_device_data.iot_dev_node.factory.sn = iot_factory.iot_sn;
//    g_device_data.iot_dev_node.factory.safetCode = iot_factory.safe_code;
//    g_device_data.iot_dev_node.factory.software_ver = IOT_VERSION;
//    memcpy(g_device_data.iot_dev_node.factory.type, iot_factory.iot_type, sizeof(iot_factory.iot_type));
//}

static void iot_about_update(void) 
{
//    g_device_data.iot_dev_node.iot_about_cantyp_0x02.status = 0;
//    g_device_data.iot_dev_node.iot_about_cantyp_0x02.protocol_ver = SetData.dev_info_t.protocol_ver; // 协议版本
//    g_device_data.iot_dev_node.iot_about_cantyp_0x02.hardware_ver = SetData.dev_info_t.hardware_ver; // 硬件版本
    g_self_data.mod_reg11000_IOT_info.software_ver = SetData.dev_info_t.software_ver; // 软件版本

    g_self_data.mod_reg11000_IOT_info.safe_code = iot_factory.safe_code; // IOT 出厂随机安全码
    g_self_data.mod_reg11000_IOT_info.iot_sn = iot_factory.iot_sn; // IOT出厂 SN码
//    g_device_data.iot_dev_node.iot_about_cantyp_0x02.dev_sn = SetData.dev_info_t.dev_sn; // 下游设备出厂 SN码

    memcpy(g_self_data.mod_reg11000_IOT_info.iot_type, iot_factory.iot_type, 12); // 设备类型字符最大12个
	//ESP_LOGI(TAG, "iot_about_update HUANGJI BBB type:%s -%d -%d",iot_factory.iot_type,  (unsigned int)iot_factory.iot_sn, (unsigned int)iot_factory.safe_code);
	//ESP_LOGI(TAG, "iot_about_update HUANGJI BBB type:%s -%d -%d",SetData.dev_info_t.Parallel_dev_type,  (unsigned int)SetData.dev_info_t.Parallel_dev_sn, (unsigned int)SetData.dev_info_t.Parallel_dev_safetycode);
//    memcpy(g_device_data.iot_dev_node.iot_about.dev_type, SetData.dev_info_t.dev_type, 12); // 设备类型字符最大12个
}

static void iot_wifi_update(void) 
{

    const wifi_info_t *wifi_info = iot_get_wifi_info();

    iot_mqtt_state(); // 更新一下MQTT状态
    g_self_data.mod_reg11000_IOT_info.sta_rssi = iot_wifi_ap_rssi(); // wifi ap rssi
//    g_self_data.mod_reg11000_IOT_info.sta_rssi =g_device_data.iot_dev_node.iot_wifi.sta_rssi;
//    g_device_data.iot_dev_node.iot_wifi_cantyp_0x03.sta_fault = 0; // WiFi 故障
//    g_device_data.iot_dev_node.iot_wifi_cantyp_0x03.sta_fault_code = wifi_info->fault_code; // 故障码
	// ESP_LOGI(TAG,"iot_wifi_update Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid::%s",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid);
	// ESP_LOGI(TAG,"iot_wifi_update g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid:%s",g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid);
    // ESP_LOGI(TAG,"iot_wifi_update wifi_info->sta_ssid:%s,wifi_info->sta_pwd:%s",wifi_info->sta_ssid,wifi_info->sta_pwd);
	memcpy(g_self_data.mod_reg11000_IOT_info.sta_ipv4, wifi_info->sta_ipv4, sizeof(g_self_data.mod_reg11000_IOT_info.sta_ipv4)); // ip
    memcpy(g_self_data.mod_reg11000_IOT_info.sta_gw,   wifi_info->sta_gw,   sizeof(g_self_data.mod_reg11000_IOT_info.sta_gw));   // gateway
    memcpy(g_self_data.mod_reg11000_IOT_info.sta_mask, wifi_info->sta_mask, sizeof(g_self_data.mod_reg11000_IOT_info.sta_mask)); // mask
    memcpy(g_self_data.mod_reg11000_IOT_info.sta_mac,  wifi_info->sta_mac,  sizeof(g_self_data.mod_reg11000_IOT_info.sta_mac));  // mac

#ifdef CONFIG_MORE_WIFI_STA_LINK_ENABLE
    g_self_data.mod_reg11000_IOT_info.wifi_mult_sta_flag.bit.ext_wifi_enable_num = 2; // 额外支持STA2和STA3
    memcpy(g_self_data.mod_reg11000_IOT_info.wifi_station_ssid_now,  wifi_info->sta_ssid,  sizeof(wifi_info->sta_ssid));  // 当前连接WIFI名称
#endif

    // WIFI 开关位状态
//    g_device_data.iot_dev_node.iot_base.cfg.bit.wifi_enable = wifi_info->flag.bit.enable;    // WiFi 开关状态
//    g_device_data.iot_dev_node.iot_base.cfg.bit.wifi_sta_enable = wifi_info->flag.bit.sta_enable; // STA 开关状态
//    g_device_data.iot_dev_node.iot_base.cfg.bit.wifi_ap_enable  = wifi_info->flag.bit.ap_enable;  // AP 开关状态

    g_self_data.mod_reg12000_IOT_set.wifi_sta_auth = SetData.dev_info_t.wifi_sta_auth;
    memcpy(g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid,
           SetData.dev_info_t.wifi_sta_ssid, sizeof(SetData.dev_info_t.wifi_sta_ssid));
    memcpy(g_self_data.mod_reg12000_IOT_set.wifi_sta_password,
           SetData.dev_info_t.wifi_sta_password, sizeof(SetData.dev_info_t.wifi_sta_password));

    // ap 未使用
//    g_device_data.iot_dev_node.iot_config.wifi_ap_auth = 0; // 认证类型
//    memset(g_device_data.iot_dev_node.iot_config.wifi_ap_ssid, 0, sizeof(g_device_data.iot_dev_node.iot_config.wifi_ap_ssid)); // ssid max len 32
//    memset(g_device_data.iot_dev_node.iot_config.wifi_ap_password, 0, sizeof(g_device_data.iot_dev_node.iot_config.wifi_ap_password));

    // 服务器地址
    memcpy(g_self_data.mod_reg12000_IOT_set.could_dns, iot_wifi_get_host(), 128);
}

static void iot_ble_update(void) //1s cycle
{
    const ble_encrypt_t  *ble_info = iot_get_ble_info();
//    g_device_data.iot_dev_node.iot_ble_cantyp_0x04.fault = ble_info->ble_err_code;
//    g_device_data.iot_dev_node.iot_ble_cantyp_0x04.ver = ble_info->ble_version;


//    memcpy(g_device_data.iot_dev_node.iot_ble_cantyp_0x04.name, reals.ble_name, sizeof(reals.ble_name));
//Inv_can


    ble_sta_t ble_sta;
    ble_get_status(&ble_sta);
	//ble_info->flag.bit.ble_connect =drv_ble_info_get_hj();
	ESP_LOGI(TAG,"iot_ble_update EN:0x%x  ble_connect:0x%x  ble_sta:%d",SetData.dev_info_t.on_off.bit.ble_enable,ble_info->flag.bit.ble_connect,ble_sta.connected);
	// iot_ble_rename();

    memcpy(g_self_data.mod_reg11000_IOT_info.ble_mac, ble_sta.mac_addr, sizeof(ble_sta.mac_addr));

}

/* 计算堆碎片率，返回 0-100 的百分比（四舍五入） */
static uint8_t calc_heap_frag_pct(size_t free_size, size_t largest)
{
    if (free_size == 0) {
        return 0;
    }
    size_t wasted = (free_size > largest) ? (free_size - largest) : 0;
    uint32_t pct = (uint32_t)(((uint64_t)wasted * 100 + free_size / 2) / free_size); /* 四舍五入 */
    return (pct > 100) ? 100 : (uint8_t)pct;
}

/* 计算内部堆碎片率，返回 0-100 的百分比（四舍五入） */
static uint8_t calc_internal_heap_frag_pct(void)
{
    return calc_heap_frag_pct(heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                              heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}

/*------------------------------------------------------------------------------
 Function: iot_debug_info_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT调试信息更新
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_debug_info_update(void)
{
    g_self_data_wlcc.mod_reg40000_IOT_Debug.param_index = 300;
    
    /************************ 设备基本数据 ************************/
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.wifi_signal_strength = iot_wifi_ap_rssi();                         // WIFI信号强度（UINT8）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.ble_server_signal_strength = reals.ble_server_signal_strength;     // 自己作为BLE client去访问BLE server从设备的信号强度
    if (1 == reals.rtc_flag.sBit.Time_valid) {
        g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.device_rtc_time = reals.now;                                   // 设备RTC时间（UINT32秒计数器）
    } else {
        g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.device_rtc_time = 0;                                           // 设备RTC时间（UINT32秒计数器）
    }
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.device_heartbeat_counter = reals.MCUPoweronCnt;                    // 设备心跳计数器（UINT32秒计数器）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_login_cloud_time = reals.last_login_cloud_time;               // 最近一次登录云服务器时间（UINT32）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_disconnect_cloud_time = reals.last_disconnect_cloud_time;     // 最近一次断开云服务器时间（UINT32）

    /************************ 设备在线数量 ************************/
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_online_num = reals.online_Iot_num;                // IOT在线数量（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.inverter_online_num = reals.online_Inv_num;           // 逆变器在线数量（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.pack_online_num = reals.online_Pack_num;              // PACK在线数量（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.wifi_local_device_num = wlcc_dev_online_num_get();    // WIFI局域网内我方设备节点数量（UINT16）

    /************************ CAN总线统计 ************************/
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_inv_rx_counter = COUNTER_DELTA_U16(reals.can_inv_rx_counter, reals.last_can_inv_rx_counter);     // CAN总线INV设备报文RX计数器（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_pack_rx_counter = COUNTER_DELTA_U16(reals.can_pack_rx_counter, reals.last_can_pack_rx_counter);  // CAN总线PACK设备报文RX计数器（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_iot_rx_counter = COUNTER_DELTA_U16(reals.can_iot_rx_counter, reals.last_can_iot_rx_counter);     // CAN总线IOT设备报文RX计数器（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_x_rx_counter = COUNTER_DELTA_U16(reals.can_x_rx_counter, reals.last_can_x_rx_counter);           // CAN总线X设备报文RX计数器（UINT16）

    /************************ 内存与网络统计 ************************/
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_remaining_ram_in_kb = MIN((heap_caps_get_free_size(MALLOC_CAP_INTERNAL)>>10), 0xFFFF);  // IOT剩余内置RAM（KB，UINT16）单位1KB，最大显示0xFFFF
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_remaining_ram_frag_pct = calc_internal_heap_frag_pct();    // IOT内置RAM碎片率（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_remaining_ram_ext_10kb = MIN((heap_caps_get_free_size(MALLOC_CAP_SPIRAM)>>10), 0xFFFF);// IOT剩余外置RAM（KB，UINT16）   单位10KB，最大显示0xFFFF
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.net_frame_tx_counter = COUNTER_DELTA_U16(reals.net_frame_tx_counter, reals.last_net_frame_tx_counter); // 网络报文帧TX计数器（MQTT/HTTP等，UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.net_frame_rx_counter = COUNTER_DELTA_U16(reals.net_frame_rx_counter, reals.last_net_frame_rx_counter); // 网络报文帧RX计数器（MQTT/HTTP等，UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.ble_frame_tx_counter = COUNTER_DELTA_U16(reals.ble_frame_tx_counter, reals.last_ble_frame_tx_counter); // BLE报文帧TX计数器（UINT16）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.ble_frame_rx_counter = COUNTER_DELTA_U16(reals.ble_frame_rx_counter, reals.last_ble_frame_rx_counter); // BLE报文帧RX计数器（UINT16）

    /************************ 设备连接时间戳 ************************/
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_build_time = Get_IoT_Build_Time_U32();                         // IOT固件编译时间（UINT32）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_login_router_time = reals.last_login_router_time;             // 最后成功登录路由器时间（UINT32）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_disconnect_router_time = reals.last_disconnect_router_time;   // 最后断开路由器时间（UINT32）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_client_connect_time = reals.last_ble_client_connect_time; // 最后成功连接BLE Client(手机)时间（UINT32）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_client_disconn_time = reals.last_ble_client_disconn_time; // 最后断开BLE Client(手机)时间（UINT32）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_server_connect_time = reals.last_ble_server_connect_time; // 最后成功连接BLE Server(储能设备)时间（UINT32）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_server_disconn_time = reals.last_ble_server_disconn_time; // 最后断开BLE Server(储能设备)时间（UINT32）

    /************************ RTC时间同步 ************************/
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_rtc_sync_ble = reals.last_rtc_sync_ble;             // 最后一次RTC同步时间（来自BLE，UINT32）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_rtc_sync_cloud = reals.last_rtc_sync_cloud;         // 最后一次RTC同步时间（来自CLOUD，UINT32）
    g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_rtc_sync_local = reals.last_rtc_sync_local;         // 最后一次RTC同步时间（来自LOCAL，UINT32）

    /************************ 线程最长运行时间（单位：us）************************/
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread1_max_runtime_us = reals.thread1_max_runtime_us;  // 线程1最长运行时间（us）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread2_max_runtime_us = reals.thread2_max_runtime_us;  // 线程2最长运行时间（us）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread3_max_runtime_us = reals.thread3_max_runtime_us;  // 线程3最长运行时间（us）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread4_max_runtime_us = reals.thread4_max_runtime_us;  // 线程4最长运行时间（us）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread5_max_runtime_us = reals.thread5_max_runtime_us;  // 线程5最长运行时间（us）
    // g_self_data_wlcc.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread6_max_runtime_us = reals.thread6_max_runtime_us;  // 线程6最长运行时间（us）
}

static void iot_link_status_check(void)
{
	const ble_encrypt_t *ble_info = iot_get_ble_info();
	const wifi_info_t *wifi_info = iot_get_wifi_info();
	//ESP_LOGI(TAG,"iot_link_status_check ENABLE:%d  sta_link_ap:%d",SetData.dev_info_t.on_off.bit.wifi_enable, wifi_info->flag.bit.sta_link_ap);
	//test 使01 IOT只读区也变成写，发出让arm知道wifi、ble是否改变

    // 蓝牙 连接状态
    if (SetData.dev_info_t.on_off.bit.ble_enable == 1)
    {
        g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect =drv_ble_get_ConnectEvent();//drv_ble_get_ConnectEvent();// ble_info->flag.bit.ble_connect;
        //ESP_LOGI(TAG,"g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect :0x%x", g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect );
		g_self_data.mod_reg11000_IOT_info.link.bit.ble_mash = 0;

		//当底层选择关闭蓝牙开关时，g_self_data 12000段同步更改(因为在Copy_Data_From_Set_To_Modbus处只会在上电时更改)，
		//Inv 12000在Iot_Self_Data_To_Can_data同步更改
		g_self_data.mod_reg12000_IOT_set.on_off.bit.ble_enable = 1;
    }
    else
    {
        g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect = 0;
        g_self_data.mod_reg11000_IOT_info.link.bit.ble_mash = 0;

		g_self_data.mod_reg12000_IOT_set.on_off.bit.ble_enable = 2;
    }


    // WIFI 连接状态
    if (SetData.dev_info_t.on_off.bit.wifi_enable == 1)
    {
        g_self_data.mod_reg11000_IOT_info.link.bit.wifi_sta = wifi_info->flag.bit.sta_link_ap;
        g_self_data.mod_reg11000_IOT_info.link.bit.wifi_ap  = wifi_info->flag.bit.ap_is_link;
        g_self_data.mod_reg11000_IOT_info.link.bit.mqtt = wifi_info->flag.bit.mqtt_login;

		//当底层选择关闭wifi开关时，g_self_data 12000段同步更改，Inv 12000在Iot_Self_Data_To_Can_data同步更改
		g_self_data.mod_reg12000_IOT_set.on_off.bit.wifi_enable = 1;
    }
    else
    {
        g_self_data.mod_reg11000_IOT_info.link.bit.wifi_sta = 0;
        g_self_data.mod_reg11000_IOT_info.link.bit.wifi_ap  = 0;
        g_self_data.mod_reg11000_IOT_info.link.bit.mqtt = 0;

		g_self_data.mod_reg12000_IOT_set.on_off.bit.wifi_enable = 2;
    }

	// ESP_LOGW(TAG, "g_self_data system link status: %08lx", g_self_data.mod_reg11000_IOT_info.link.all);
	// ESP_LOGI(TAG,"g_self_data.mod_reg12000_IOT_set.on_off.all: %08lx",g_self_data.mod_reg12000_IOT_set.on_off.all);
	// ESP_LOGI(TAG,"g_self_data.mod_reg12000_IOT_set.on_off.wifi_enable: %u",g_self_data.mod_reg12000_IOT_set.on_off.bit.wifi_enable);
	// ESP_LOGI(TAG,"g_self_data.mod_reg12000_IOT_set.on_off.ble_enable: %u",g_self_data.mod_reg12000_IOT_set.on_off.bit.ble_enable);

	// //test IOT的蓝牙、WiFi状态改变，发出让arm知道
	// if((old_ble_stutus != g_self_data.mod_reg11000_IOT_info.link.bit.ble_connect) || 
	// 	(old_wifi_sta != g_self_data.mod_reg11000_IOT_info.link.bit.wifi_sta))
	// {
	// 	CanSetTarget(0, 0, IOT_TYPE_INFO_01H, 36, &g_self_data.mod_reg11000_IOT_info.link, 4, CAN_BROADCAST_ADDRESS);
	// }

//    g_self_data.mod_reg11000_IOT_info.link.bit._4g = 0;
    g_self_data.mod_reg11000_IOT_info.link.bit.gps = 0;
//    g_self_data.mod_reg11000_IOT_info.link.bit.Eth_Stat = 0;

	static link_status last_link_sta = {0};
	static uint8_t link_first_report = 1;
	/* 开机首次也要上报：重启后 BLE/WiFi 多为断开(0)，与 last_link_sta 初值相同，
	 * 仅靠变化检测会漏报，ARM 会一直显示升级前的在线图标 */
	if ((link_first_report || (last_link_sta.all != g_self_data.mod_reg11000_IOT_info.link.all))
		&& (!reals.ota_happen))
	{
		link_first_report = 0;
		last_link_sta = g_self_data.mod_reg11000_IOT_info.link;
		ESP_LOGW(TAG, "system link status: %08lx", last_link_sta.all);

		//test IOT的蓝牙、WiFi状态该变，发出让arm知道
		//CanSetTarget(0, 0, IOT_TYPE_INFO_01H, 36, &g_self_data.mod_reg11000_IOT_info.link, 4, CAN_BROADCAST_ADDRESS);
		link_report_set(1);
	}
}

static void iot_link_status_update(void)
{
#ifdef FUNC_ONECLICK_UPGRADE_EN
	if (Can_OneClickState_Get()) {
		return;
	}
#endif
	if(reals.dev_change_ctrl.bit.link_report)
	{
		ESP_LOGW(TAG, "update system link status: %08lx", g_self_data.mod_reg11000_IOT_info.link.all);

		//test IOT的蓝牙、WiFi状态该变，发出让arm知道
		CanSetTarget(0, 0, IOT_TYPE_INFO_01H, 36, &g_self_data.mod_reg11000_IOT_info.link, 4, CAN_BROADCAST_ADDRESS);
		link_report_set(0);
	}
}

/*------------------------------------------------------------------------------
 Function: iot_smart_control_status_check
-----------------------------------------------------------------------------*/
/**
* @brief      更新智能控制状态
* @param[in]  void  
* @param[out] None
* @return     static void
*/
static void iot_smart_control_status_check(void)
{
	static uint32_t last_status = 0;
	uint32_t iot_smart_control_status = 0;

	/*判断是否需要进行时间段控制*/
	if ((Inv[INV_MAX_NUM].mod_reg02000_Inv_base_set.work_mode == 1) 
		&& (Inv[INV_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time_area == 1 )) {
		Smart_Tou_Ctrl_Time_Quantum_Enable_Set(true);
	} else {
		Smart_Tou_Ctrl_Time_Quantum_Enable_Set(false);
	}

	/*智能控制状态*/
	iot_smart_control_status |= Smart_Tou_Ctrl_Status_Get()->all;

#ifdef CONFIG_RELAY_CTRL_ENABLE
	/*TOU控制状态*/
	iot_smart_control_status |= Relay_TimeCtrl_Enable_Status_Get();
#endif

	g_self_data.mod_reg11000_IOT_info.iot_smart_control.all = iot_smart_control_status;
	
	// 下发EMS控制指令
	if ((last_status != g_self_data.mod_reg11000_IOT_info.iot_smart_control.all) 
		|| (1 == reals.iot_smart_change)) 
	{
		last_status = g_self_data.mod_reg11000_IOT_info.iot_smart_control.all;
		ESP_LOGW(TAG, "System smart control status: %08lx", last_status);
		reals.iot_smart_change = 0;

		/*状态变动，更新至总线（仅主机）*/
		if((1 == reals.Addr_can_master)&&(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_FINISH)) {
			//test IOT的dc/ac定时任务，让ARM知道
			CanSetTarget(0, 0, IOT_TYPE_INFO_01H, 206, &g_self_data.mod_reg11000_IOT_info.iot_smart_control.all, 4, CAN_BROADCAST_ADDRESS);
		}
	}
}

/*
主循环定时标志
*/
void App_SysTime(void)//
{
	static uint8_t ts_u16Cnt10ms = 0;
	static uint8_t ts_u16Cnt50ms = 0;	
	static uint8_t ts_u16Cnt100ms = 0;	
	static uint8_t ts_u16Cnt500ms = 0;
	static uint8_t ts_u16Cnt1000ms = 0;
	static uint8_t ts_u16Cnt1minute = 0;


	
	if(ts_u16Cnt10ms != reals.Timer_10msCount)						   
	{
		ts_u16Cnt10ms = reals.Timer_10msCount;
		reals.st_FlagTime.bits.b1FlagSys10ms = 1;						  
	}
	else
	{
		reals.st_FlagTime.bits.b1FlagSys10ms = 0;
	}
	
	if(ts_u16Cnt50ms != reals.Timer_50msCount)						   
	{
		ts_u16Cnt50ms = reals.Timer_50msCount;
		reals.st_FlagTime.bits.b1FlagSys50ms = 1;						  
	}
	else
	{
		reals.st_FlagTime.bits.b1FlagSys50ms = 0;
	}

	if(ts_u16Cnt100ms != reals.Timer_100msCount)								
	{
		ts_u16Cnt100ms = reals.Timer_100msCount;
		reals.st_FlagTime.bits.b1FlagSys100ms = 1; 			
	}
	else
	{
		reals.st_FlagTime.bits.b1FlagSys100ms = 0;
	}	
	
	if(ts_u16Cnt500ms != reals.Timer_500msCount)								
	{
		ts_u16Cnt500ms = reals.Timer_500msCount;
		reals.st_FlagTime.bits.b1FlagSys500ms = 1; 				
	}
	else
	{
		reals.st_FlagTime.bits.b1FlagSys500ms = 0;
	}	
	if(ts_u16Cnt1000ms != reals.Timer_1sCount)							  
	{
		ts_u16Cnt1000ms = reals.Timer_1sCount;
		reals.st_FlagTime.bits.b1FlagSys1s = 1;				 
	}
	else
	{
		reals.st_FlagTime.bits.b1FlagSys1s = 0;
	}		

	if(ts_u16Cnt1minute != reals.Timer_1MinuteCount)								
	{
		ts_u16Cnt1minute = reals.Timer_1MinuteCount;
		if(0 == reals.Timer_1MinuteCount)
		{
			reals.st_FlagTime.bits.b1FlagSys1Minute=1;
		}
	}
	else
	{
		reals.st_FlagTime.bits.b1FlagSys1Minute=0;
	}	



	if(1==reals.st_FlagTime.bits.b1FlagSys10ms)
	{

	}


	if(1 == reals.st_FlagTime.bits.b1FlagSys1s)//触发set
	{
		
		reals.st_FlagTime.bits.flagA_1s =1;
		reals.st_FlagTime.bits.flagC_1s =1;
//		reals.st_FlagTime.bits.flagD_1s =1;

	}

	
}


/*
windy add
10ms timer cycle
*/
static void app_main_timer_cb(TimerHandle_t pxTimer) 
{

//reals.Timer_1msCount++;									// 1ms timer
//if(reals.Timer_1msCount >= 10)
{
//	reals.Timer_1msCount = 0;
//if((reals.BleRx_TimeCnt)
//	&&(reals.BleRx_TimeCnt < 200))//非零才计数器增加
//{
//	reals.BleRx_TimeCnt++;
//}
	
	Can_Beta_Tx_Block_Timeout_Check();
	reals.Timer_10msCount++;								// 10ms timer
	if(0 == ((reals.Timer_10msCount+1)%5))
	{
		reals.Timer_50msCount++;					   // 50ms timer
	}
		
	if(reals.Timer_10msCount >= 10)
	{
		reals.Timer_10msCount = 0;

			
		reals.Timer_100msCount++; 					   // 100ms timer
		if(reals.Timer_100msCount >= 5)
		{
			reals.Timer_100msCount = 0;
			reals.Timer_500msCount++; 					// 500ms timer
			if(reals.Timer_500msCount >= 2)
			{		
				reals.Timer_500msCount = 0;
				reals.Timer_1sCount++ ;			   // 1s timer
				reals.cnt_1s++;
				reals.MCUPoweronCnt++;
				if(reals.Timer_1sCount >= 2)
				{						
					reals.Timer_1sCount = 0;
					reals.Timer_1MinuteCount++;//1minute timer
					if(reals.Timer_1MinuteCount >= 30)
					{
						reals.Timer_1MinuteCount=0;

					}
				}					
			}				
		}						
	}				 
}


}
void app_main_timer_init(void)
{
    TimerHandle_t main_timer = NULL;
    main_timer = xTimerCreate("main timer", pdMS_TO_TICKS(MAIN_TIMER_INTERVAL), pdTRUE, NULL, app_main_timer_cb); 
    if (main_timer == NULL) 
    {
        ESP_LOGE(TAG, "main timer create failed");
        return ;
    } 
    xTimerStart(main_timer, pdMS_TO_TICKS(10));
}

extern void uart_tx_debug(void);
 
 /*
 windy:
 伪装逆变器信息，用于手机APP配置路由器密码开启
 */
 void inv_data_imitate(void)
 {
	#define IMITATEINV_TYPE				"EP900"
 
 //  extern int sys_dbg;
 //  if (!(sys_dbg & 0x01)) return;
 
	 inv_node_struct *p_inv_node = &g_device_data.inv_dev_node;
	 strcpy(p_inv_node->inv_about.dev_type, IMITATEINV_TYPE);
	 p_inv_node->inv_about.dev_sn = 112233445566;
	 p_inv_node->inv_base.inv_num = 1;
	 p_inv_node->inv_base.inv_online = 0x01;
 
	 uint32_t total_power = 0;
	 p_inv_node->inv_grid.grid_phase_number = 2;
	 for(int i = 0; i < 2; i++)
	 {
		 p_inv_node->inv_grid.grid_detail[i].input_power = 58000 + (i+1)*100;
		 p_inv_node->inv_grid.grid_detail[i].input_current = 55000 + (i+1)*100;
		 p_inv_node->inv_grid.grid_detail[i].input_voltage = 50000 + (i+1)*100;
		 total_power += p_inv_node->inv_grid.grid_detail[i].input_power;
	 }
	 p_inv_node->inv_grid.total_chg_power = total_power;
	 p_inv_node->inv_base.GridAllTotalPower = p_inv_node->inv_grid.total_chg_power;

//	 MicroInv[0].mod_reg00100_AppPage1.dev_sn =p_inv_node->inv_about.dev_sn;
//	 memcpy((uint8_t *)&MicroInv[0].mod_reg00100_AppPage1.dev_type,(uint8_t *)p_inv_node->inv_about.dev_type,sizeof(p_inv_node->inv_about.dev_type));//debug  

//	 MicroInv[0].mod_reg00100_AppPage1.inv_num =p_inv_node->inv_base.inv_num;	  //  120  逆变数量
//	 MicroInv[0].mod_reg00100_AppPage1.inv_online =p_inv_node->inv_base.inv_online;  //  121	逆变在线地址
//	 MicroInv[0].mod_reg01300_Inv_grid.grid_phase_number =p_inv_node->inv_grid.grid_phase_number;
//	 total_power = 0;
//	 for(int i = 0; i < 2; i++)
//	 {
//		 MicroInv[0].mod_reg01300_Inv_grid.grid_detail[i].input_power = 58000 + (i+1)*100;
//		 MicroInv[0].mod_reg01300_Inv_grid.grid_detail[i].input_current = 55000 + (i+1)*100;
//		 MicroInv[0].mod_reg01300_Inv_grid.grid_detail[i].input_voltage = 50000 + (i+1)*100;
//		 total_power += MicroInv[0].mod_reg01300_Inv_grid.grid_detail[i].input_power;
//	 }
//	 MicroInv[0].mod_reg01300_Inv_grid.total_chg_power = total_power;
//	 MicroInv[0].mod_reg00100_AppPage1.GridAllTotalPower = MicroInv[0].mod_reg01300_Inv_grid.total_chg_power;

	 
 }

 /*--------------Modbus_Read_Info_Process
基于上级Modbus 查询的本地内容处理

 *@brief  
 *@param[in]	 None
 *@param[out]	 None
 *@return		  
 */
void Modbus_Read_Info_Process(const uint8_t *income)
{
	uint8_t md_addr = income[0];

	/* 读数据时根据modbus地址切换不同的数据 */
	if (income[1] == 0x03)
	{
		// app_energy_handler();												// 电量数据处理

/*
windy tbd:
要将Inv_can 赋给 Inv

		*/
//		Inv
//
//		if (IS_READ_SUMMARY_DATA(md_addr)) 
//		{								// 读汇总数据
//			g_device_data.inv_dev_node = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM];
//			g_device_data.pack_dev_node = Inv_can[DEV_MAIN_NODE_MAX].pack_data[INV_MAX_NUM];
//		}
//		else if (IS_READ_PACK_SUM_DATA(md_addr)) 
//		{							// 读电池分组汇总数据
//			uint8_t grp = PACK_GRP_TO_IDX(md_addr);
//			if (grp < DEV_MAIN_NODE_MAX) {
//				g_device_data.pack_dev_node = g_device_data.pack_grp_summary[grp];
//			}
//			else {															// 电池其它分组数据设置为0
//				bzero(&g_device_data.pack_dev_node, sizeof(g_device_data.pack_dev_node));
//			}
//		}
//		else if (IS_READ_INV_DATA(md_addr)) 
//		{								// 读逆变数据
//			if (INV_ADDR_TO_IDX(md_addr) <= (INV_MAX_NUM*DEV_MAIN_NODE_MAX)) 
//			{
//				uint8_t grp = (md_addr - MD_INV_ADDR_START) / INV_MAX_NUM;
//				uint8_t idx = (md_addr - MD_INV_ADDR_START) % INV_MAX_NUM;
//				g_device_data.inv_dev_node = Inv_can[grp].inv_data[idx];
//				inv_node_data_process(grp);
//
//				/* 支持APP通过逆变地址读取该逆变下挂载的电池组数据 */
//				g_device_data.pack_dev_node = g_device_data.pack_grp_summary[grp];
//			}
//			else 
//			{
//				bzero(&g_device_data.inv_dev_node, sizeof(g_device_data.inv_dev_node));
//				bzero(&g_device_data.pack_dev_node, sizeof(g_device_data.pack_dev_node));
//			}
//		}
//		else if (IS_READ_PACK_DATA(md_addr)) 
//		{								// 读电池数据
//			if (PACK_ADDR_TO_IDX(md_addr) < (PACK_MAX_NUM*DEV_MAIN_NODE_MAX)) 
//			{
//				uint8_t grp = PACK_ADDR_TO_GRP(md_addr);
//				uint8_t idx = PACK_ADDR_TO_IDX(md_addr);
//				g_device_data.pack_dev_node = Inv_can[grp].pack_data[idx];
//			}
//			else 
//			{
//				bzero(&g_device_data.pack_dev_node, sizeof(g_device_data.pack_dev_node));
//			}
//		}

	}

	inv_data_imitate();

//windy debug
//	g_device_data.inv_dev_node.inv_base.GridAllTotalPower=reals.struct_uart2.Rx_timeout_cnt;

}

#define INV_MIN_OUTPUT 20//预留微逆最小输出而不关机的最小功率值，*1w；假定微逆0w输出和非0w输出会让逆变器切换困难，就需要预留此非零值，否则不需要
#define INV_MAX_RATE_OUTPUT ((int16_t)250)//  w
#define INV_RATE_ERROR_RANGE ((int16_t)5)//  w


#define ENERGY_STORE_THRESHOLD		(5)		// pv存储阈值,单位(0.1kwh)


/**
 * @brief 检测各能量是否达到阈值
 *
 * @return 超过阈值返回真,否则返回假
 */
static bool energy_threshold_check(void)
{
	int count = sizeof(baseKWH_energy_t_u32_2) / sizeof(uint32_t);
	uint32_t *p_now = (uint32_t *)&statistic_delta;
//	uint64_t *p_old = (uint64_t *)&bk_energy_data;

	for (int i = 0; i < count; i++)
	{
		int64_t diff = (int64_t)(p_now[i]);
//		int64_t diff = (int64_t)(p_now[i] - p_old[i]);
		
		if (diff <= 0) continue;

		/* 能量值转换为0.1kwh后再与阈值比较 */
//		if ((int64_t)((double)diff / (100 * 3600)) >= ENERGY_STORE_THRESHOLD) 
		if ((int64_t)((double)diff ) >= ENERGY_STORE_THRESHOLD) 
		{
			return true;
		}
	}

	return false;
}

/*------------------------------------------------------------------------------
 Function: iot_kwh_status_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查kwh电量信息，确认增量
  * @param[in]  void  
  * @param[out] 1:变化 0：无变化
  * @return     static void
  */
static uint8_t iot_kwh_status_check(void)
{
//    static statistic_struct statistic = 
//    {
//        .pv_energy = UINT_MAX,
//        .grid_chg_energy = UINT_MAX,
//        .grid_fd_energy = UINT_MAX,
//    };
        
    static uint8_t sDelta_minute = 0xFF;

    static uint16_t pv_energy_Delta = 0;
    static uint16_t grid_chg_energy_Delta = 0;
    static uint16_t grid_fd_energy_Delta = 0;

    static uint8_t ret = 0;
    

	
	/*
	两部分来源：
	A.INV；
	B.EMS模块整理
	
	
	对于下级设备生成KWH的变量，从下级设备取增量：
	DCLoadTotalEnergy	4		0.1kwh
	ACLoadTotalEnergy	4		0.1kwh
	PvTotalChargingEnergy	4		0.1kwh
	GridTotalChargingEnergy 4		0.1kwh
	FeedbackEnergy	4		0.1kwh
	PVToACloadEnergy	4		0.1kwh
	
	对于下级设备不生成KWH的变量，本地拟合
	
			*/
		/*判断能量是否变化*/
	if ((statistic.DCLoad_Total < Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_dc_load_energy)
		&&(statistic.DCLoad_Total != 0))
	{
		statistic_delta.DCLoad_Total += (Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_dc_load_energy - statistic.DCLoad_Total);
	}
	if ((statistic.ACLoad_Total < Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_ac_load_energy)
		&&(statistic.ACLoad_Total != 0))
	{
		statistic_delta.ACLoad_Total += (Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_ac_load_energy - statistic.ACLoad_Total);
	}

	if ((statistic.Pv_dc < Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.total_chg_energy)
		&&(statistic.Pv_dc != 0))
	{
		statistic_delta.Pv_dc += (Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.total_chg_energy - statistic.Pv_dc);
	}
	if ((statistic.PVToload_Total < Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.PvToACLoadEnergy)
		&&(statistic.PVToload_Total != 0))
	{
		statistic_delta.PVToload_Total += (Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.PvToACLoadEnergy - statistic.PVToload_Total);
	}

	if ((statistic.GridChgin_Total < Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_chg_energy)
		&&(statistic.GridChgin_Total != 0))
	{
		statistic_delta.GridChgin_Total += (Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_chg_energy - statistic.GridChgin_Total);
	}
	if ((statistic.GridFeedback_Total < Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_fb_energy)
		&&(statistic.GridFeedback_Total != 0))
	{
		statistic_delta.GridFeedback_Total += (Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_fb_energy - statistic.GridFeedback_Total);
	}

	if ((statistic.PackDsg_Total < Inv[reals.Addr_can_self].mod_reg06000_Pack_sum.total_chg_energy)
		&&(statistic.PackDsg_Total != 0))
	{
		statistic_delta.PackDsg_Total += (Inv[reals.Addr_can_self].mod_reg06000_Pack_sum.total_chg_energy - statistic.PackDsg_Total);
	}

		

	/*能量状态更新*/
	// statistic.DCLoad_Total = Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_dc_load_energy;
	// statistic.ACLoad_Total = Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_ac_load_energy;
	// statistic.Pv_dc = Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.total_chg_energy;
	// statistic.PVToload_Total = Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.PvToACLoadEnergy;
	// statistic.GridChgin_Total = Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_chg_energy;
	// statistic.GridFeedback_Total = Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_fb_energy;
	// statistic.PackDsg_Total = Inv[reals.Addr_can_self].mod_reg06000_Pack_sum.total_chg_energy;


				/* 能量阈值检测 */
//				bool exceed_thres = energy_threshold_check();


    /*确认增量*/ //test dc_ac_pv
    //if(fabs((int8_t)sDelta_minute - (int8_t)reals.rtc_time.min) >= 5)//最快5分钟写一次
    {
    
		//sDelta_minute = reals.rtc_time.min;//test dc_ac_pv
		
        if(statistic_delta.DCLoad_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].DCLoad_Total+=statistic_delta.DCLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].DCLoad_Total+=statistic_delta.DCLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].DCLoad_Total+=statistic_delta.DCLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].DCLoad_Total+=statistic_delta.DCLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].DCLoad_Total+=statistic_delta.DCLoad_Total;// 			
			statistic_delta.DCLoad_Total =0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
        }
         if(statistic_delta.ACLoad_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].ACLoad_Total+=statistic_delta.ACLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].ACLoad_Total+=statistic_delta.ACLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].ACLoad_Total+=statistic_delta.ACLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].ACLoad_Total+=statistic_delta.ACLoad_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].ACLoad_Total+=statistic_delta.ACLoad_Total;// 			
			statistic_delta.ACLoad_Total =0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }
        if(statistic_delta.Pv_dc > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].Pv_dc+=statistic_delta.Pv_dc;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].Pv_dc+=statistic_delta.Pv_dc;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].Pv_dc+=statistic_delta.Pv_dc;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].Pv_dc+=statistic_delta.Pv_dc;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].Pv_dc+=statistic_delta.Pv_dc;// 			
			statistic_delta.Pv_dc =0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }		 
         if(statistic_delta.GridChgin_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].GridChgin_Total+=statistic_delta.GridChgin_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].GridChgin_Total+=statistic_delta.GridChgin_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].GridChgin_Total+=statistic_delta.GridChgin_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridChgin_Total+=statistic_delta.GridChgin_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].GridChgin_Total+=statistic_delta.GridChgin_Total;// 			
			statistic_delta.GridChgin_Total =0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }
         if(statistic_delta.GridFeedback_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].GridFeedback_Total+=statistic_delta.GridFeedback_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].GridFeedback_Total+=statistic_delta.GridFeedback_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].GridFeedback_Total+=statistic_delta.GridFeedback_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridFeedback_Total+=statistic_delta.GridFeedback_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].GridFeedback_Total+=statistic_delta.GridFeedback_Total;// 			
			statistic_delta.GridFeedback_Total =0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }
        if(statistic_delta.PVToload_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].PVToload_Total+=statistic_delta.PVToload_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].PVToload_Total+=statistic_delta.PVToload_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].PVToload_Total+=statistic_delta.PVToload_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PVToload_Total+=statistic_delta.PVToload_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].PVToload_Total+=statistic_delta.PVToload_Total;// 			
			statistic_delta.PVToload_Total =0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        } 
        if(statistic_delta.PackDsg_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].PackDsg_Total+=statistic_delta.PackDsg_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].PackDsg_Total+=statistic_delta.PackDsg_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].PackDsg_Total+=statistic_delta.PackDsg_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PackDsg_Total+=statistic_delta.PackDsg_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].PackDsg_Total+=statistic_delta.PackDsg_Total;// 			
			statistic_delta.PackDsg_Total =0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }   		
    }

	if((fabs((int8_t)sDelta_minute - (int8_t)reals.rtc_time.min) >= 30)&&(ret))//最快30分钟写一次
	{
		ESP_LOGI(TAG,"iot_kwh_status_check Write flash");
		sDelta_minute = reals.rtc_time.min;//test dc_ac_pv
		reals.SetDataWrFlag.sBit.kwh_mix =1;
		ret=0;
	}

	statistic.DCLoad_Total = Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_dc_load_energy;
	statistic.ACLoad_Total = Inv[reals.Addr_can_self].mod_reg01400_Inv_load.total_ac_load_energy;
	statistic.Pv_dc = Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.total_chg_energy;
	statistic.PVToload_Total = Inv[reals.Addr_can_self].mod_reg01200_Inv_pv.PvToACLoadEnergy;
	statistic.GridChgin_Total = Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_chg_energy;
	statistic.GridFeedback_Total = Inv[reals.Addr_can_self].mod_reg01300_Inv_grid.total_fb_energy;
	statistic.PackDsg_Total = Inv[reals.Addr_can_self].mod_reg06000_Pack_sum.total_chg_energy;


	// ESP_LOGI(TAG,"single INV CAN DCLoadTotalEnergy:%lu",Inv_can[reals.Addr_can_self].inv_data[0].inv_base.DCLoadTotalEnergy);
	// ESP_LOGI(TAG,"singel INV CAN ACLoadTotalEnergy:%lu",Inv_can[reals.Addr_can_self].inv_data[0].inv_base.ACLoadTotalEnergy);
	// ESP_LOGI(TAG,"single INV CAN PvTotalChargingEnergy:%lu",Inv_can[reals.Addr_can_self].inv_data[0].inv_base.PvTotalChargingEnergy);
	// ESP_LOGI(TAG,"single INV CAN GridTotalChargingEnergy:%lu",Inv_can[reals.Addr_can_self].inv_data[0].inv_base.GridTotalChargingEnergy);

	// ESP_LOGI(TAG, "hour SetData.DCLoad_Total:%lu --%u",SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].DCLoad_Total,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].DCLoad_Total);
	// ESP_LOGI(TAG, "hour SetData.ACLoad_Total:%lu --%u",SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].ACLoad_Total,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].ACLoad_Total);
	// ESP_LOGI(TAG, "hour SetData.Pv_dc:%lu --%lu",SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].Pv_dc,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].Pv_dc);
	// ESP_LOGI(TAG, "hour SetData.GridChgin_Total:%lu --%u",SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_ALONE].GridChgin_Total,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridChgin_Total);

    return ret;
}



/*------------------------------------------------------------------------------
 Function: iot_kwh_status_check_sum
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查kwh并机汇总电量信息，确认增量
  * @param[in]  void  
  * @param[out] 1:变化 0：无变化
  * @return     static void
  */
static uint8_t iot_kwh_sum_status_check(void)
{

	static uint8_t sDelta_minute = 0xFF;
	/*
	电量来源：
	A.AC负载功率每秒累计；
	B.DC负载功率每秒累计；
	C.PV负载功率每秒累计；
	D.电网功率每秒累计
	*/
	static uint32_t dCLoad_Total_s=0;		//	DC负载总能量 每秒累计总量	单位：w*s
	static uint32_t aCLoad_Total_s=0;		//	AC负载+AC-HUB总能量  
	static uint32_t pvChgin_Total_s=0;	//	PV总的充电能量  
	static uint32_t gridChgin_Total_s=0;	// 电网充电能量 

	static uint32_t dCLoad_Total=0;		//	DC负载总能量 每秒累计总量	单位：kwh
	static uint32_t aCLoad_Total=0;		//	AC负载+AC-HUB总能量  
	static uint32_t pvChgin_Total=0;	//	PV总的充电能量  
	static uint32_t gridChgin_Total=0;	// 电网充电能量 
	static uint32_t GridFeedback_Total=0;		// 馈电到电网能量
	static uint32_t PVToload_Total=0;			// PV到AC负载能量 	
	static uint32_t PackDsg_Total=0;  	// PACK总放电能量 

	static uint32_t dCLoad_delta_Total=0;		//	DC负载总能量 每五分钟做比较并更新 单位：kwh
	static uint32_t aCLoad_delta_Total=0;		//	AC负载总能量 
	static uint32_t pvChgin_delta_Total=0;		//	PV总的充电能量  
	static uint32_t gridChgin_delta_Total=0;	// 电网充电能量 
	static uint32_t GridFeedback_delta_Total=0;		// 馈电到电网能量
	static uint32_t PVToload_delta_Total=0;			// PV到AC负载能量 	
	static uint32_t PackDsg_delta_Total=0;  	// PACK总放电能量 

	uint32_t uAddVal=0;
    static uint8_t ret = 0;

	dCLoad_Total_s+=Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.DCLoadAllTotalPower;	//140 DC负载总功率 	1W
	if(dCLoad_Total_s>=WS_TO_100WH_UNIT)
	{
		dCLoad_Total_s-=WS_TO_100WH_UNIT;
		dCLoad_Total+=1;
	}
	aCLoad_Total_s+=Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ACLoadAllTotalPower;	//142 AC负载总功率	1W
	if(aCLoad_Total_s>=WS_TO_100WH_UNIT)
	{
		aCLoad_Total_s-=WS_TO_100WH_UNIT;
		aCLoad_Total+=1;
	}
	pvChgin_Total_s+=Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PVAllTotalPower;	//144 PV负载总功率	1W
	if(pvChgin_Total_s>=WS_TO_100WH_UNIT)
	{
		pvChgin_Total_s-=WS_TO_100WH_UNIT;
		pvChgin_Total+=1;
	}
	gridChgin_Total_s+=Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.GridAllTotalPower;	//146 电网充电总功率	1W
	if(gridChgin_Total_s>=WS_TO_100WH_UNIT)
	{
		gridChgin_Total_s-=WS_TO_100WH_UNIT;
		gridChgin_Total+=1;
	}


	if ((GridFeedback_Total < Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_fb_energy)
		&&(statistic.GridFeedback_Total != 0))
	{
		GridFeedback_delta_Total += (Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_fb_energy - GridFeedback_Total);
	}
	if ((PVToload_Total < Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.PvToACLoadEnergy)
		&&(statistic.PVToload_Total != 0))
	{
		PVToload_delta_Total += (Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.PvToACLoadEnergy - PVToload_Total);
	}

	if ((PackDsg_Total < Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum.total_chg_energy)
		&&(statistic.PackDsg_Total != 0))
	{
		PackDsg_delta_Total += (Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum.total_chg_energy - PackDsg_Total);
	}

	/*确认增量*/ //test dc_ac_pv
   // if(fabs((int8_t)sDelta_minute - (int8_t)reals.rtc_time.min) >= 5)//最快5分钟写一次
    {
		//sDelta_minute = reals.rtc_time.min;//test dc_ac_pv
		
        if(dCLoad_delta_Total< dCLoad_Total)
        {    
			uAddVal=dCLoad_Total-dCLoad_delta_Total;

			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].DCLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].DCLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].DCLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].DCLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].DCLoad_Total+=uAddVal;// 			

			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
        }
         if(aCLoad_delta_Total<aCLoad_Total )
        {                
			uAddVal=aCLoad_Total-aCLoad_delta_Total;

			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].ACLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].ACLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].ACLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].ACLoad_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].ACLoad_Total+=uAddVal;// 			

			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }
        if(pvChgin_delta_Total<pvChgin_Total )
        {          
			uAddVal=pvChgin_Total-pvChgin_delta_Total;

			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].Pv_dc+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].Pv_dc+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].Pv_dc+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].Pv_dc+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].Pv_dc+=uAddVal;// 			

			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }		 
        if(gridChgin_delta_Total< gridChgin_Total)
        {                
			uAddVal=gridChgin_Total-gridChgin_delta_Total;

			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].GridChgin_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].GridChgin_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].GridChgin_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridChgin_Total+=uAddVal;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].GridChgin_Total+=uAddVal;// 			

			//reals.SetDataWrFlag.sBit.kwh_mix =1;

			ret =1;	
        }

         if(GridFeedback_delta_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].GridFeedback_Total+=GridFeedback_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].GridFeedback_Total+=GridFeedback_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].GridFeedback_Total+=GridFeedback_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridFeedback_Total+=GridFeedback_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].GridFeedback_Total+=GridFeedback_delta_Total;// 			
			GridFeedback_delta_Total=0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }
        if(PVToload_delta_Total > 0)
        {                
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].PVToload_Total+=PVToload_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].PVToload_Total+=PVToload_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].PVToload_Total+=PVToload_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PVToload_Total+=PVToload_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].PVToload_Total+=PVToload_delta_Total;// 			
			PVToload_delta_Total=0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        } 

        if(PackDsg_delta_Total >0)
        {           
			uAddVal=Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum.total_chg_energy-PackDsg_delta_Total;

			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].PackDsg_Total+=PackDsg_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].PackDsg_Total+=PackDsg_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].PackDsg_Total+=PackDsg_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PackDsg_Total+=PackDsg_delta_Total;// 
			SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].PackDsg_Total+=PackDsg_delta_Total;// 			
			PackDsg_delta_Total=0;
			//reals.SetDataWrFlag.sBit.kwh_mix =1;
			ret =1;
			
        }  
		dCLoad_delta_Total=dCLoad_Total;
		aCLoad_delta_Total=aCLoad_Total;
		pvChgin_delta_Total=pvChgin_Total;
		gridChgin_delta_Total=gridChgin_Total;
	}
 	if((fabs((int8_t)sDelta_minute - (int8_t)reals.rtc_time.min) >= 30)&&(ret))//最快30分钟写一次
	{
		ESP_LOGI(TAG,"iot_kwh_sum_status_check Write flash");
		sDelta_minute = reals.rtc_time.min;//test dc_ac_pv
		reals.SetDataWrFlag.sBit.kwh_mix =1;
		ret=0;
	}

	GridFeedback_Total=Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_energy;
	PVToload_Total=Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_fb_energy;
	PackDsg_Total=Inv[DEV_MAIN_NODE_MAX].mod_reg06000_Pack_sum.total_chg_energy;
	if(reals.online_ACHUB_num == 1)
	{
		ESP_LOGI(TAG,"ACHUB Energy statis");
	//dc负载能量 并机用IOT功率积分，避免ARM终身值汇总跳变
		Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_dc_load_energy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].DCLoad_Total;
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.DCLoadTotalEnergy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].DCLoad_Total;
	//ac负载能量
		Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_energy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].ACLoad_Total;
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ACLoadTotalEnergy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].ACLoad_Total;
	//pv充电能量 并机用IOT功率积分，避免ARM终身值汇总跳变
		Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.total_chg_energy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].Pv_dc;
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PvTotalChargingEnergy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].Pv_dc;
	//电网充电能量 当前改为从achub获取(版本高于908705  908109)
		if(!reals.achub_grid_transfer_flag)
		{
			ESP_LOGI(TAG,"sum grid kwh iot check:%lu",SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].GridChgin_Total);
			Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_energy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].GridChgin_Total;
			Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.GridTotalChargingEnergy=SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].GridChgin_Total;
		}
	}
	// ESP_LOGI(TAG,"INV CAN DCLoadTotalEnergy:%lu",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.DCLoadTotalEnergy);
	// ESP_LOGI(TAG,"INV CAN ACLoadTotalEnergy:%lu",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ACLoadTotalEnergy);
	// ESP_LOGI(TAG,"INV CAN PvTotalChargingEnergy:%lu",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PvTotalChargingEnergy);
	// ESP_LOGI(TAG,"INV CAN GridTotalChargingEnergy:%lu",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.GridTotalChargingEnergy);

	// ESP_LOGI(TAG, "hour sum DCLoad_Total :%lu,s=%lu,setdata:%lu --%u",dCLoad_delta_Total,dCLoad_Total_s,SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].DCLoad_Total,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].DCLoad_Total);
	// ESP_LOGI(TAG, "hour sum ACLoad_Total :%lu,s=%lu,setdata:%lu --%u",aCLoad_delta_Total,aCLoad_Total_s,SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].ACLoad_Total,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].ACLoad_Total);
	// ESP_LOGI(TAG, "hour sum Pv_dc:%lu,s=%lu,setdata:%lu --%lu",pvChgin_delta_Total,pvChgin_Total_s,SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].Pv_dc,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].Pv_dc);
	// ESP_LOGI(TAG, "hour sum GridChgin_Total	:%lu,s=%lu,setdata:%lu --%u",gridChgin_delta_Total,gridChgin_Total_s,SetData.dev_info_t.iot_kwh_info.KWH_all[KWH_INFO_SUM].GridChgin_Total,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridChgin_Total);
	return ret;
}
/*------------------------------------------------------------------------------
 Function: iot_kwh_data_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      更新信息至缓存，预备写入文件系统
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_kwh_data_update(void)
{
    
    uint8_t need_to_flash_WR_Hour=0; 
	uint8_t need_to_flash_WR_DCLoad_Total = 0;	
	uint8_t need_to_flash_WR_ACLoad_Total = 0;
    uint8_t need_to_flash_WR_Pv_Total = 0;	
	uint8_t need_to_flash_WR_GridChgin_Total = 0;	
	uint8_t need_to_flash_WR_GridFeedback_Total = 0;
    uint8_t need_to_flash_WR_PVToload_Total = 0;	
    uint8_t need_to_flash_WR_PackDsg_Total = 0;	

	static baseKWH_energy_t_u16 DeltaKWH_Hour_temp;//
    static uint8_t srunflag=0; 

	if(0 == srunflag)
	{
		srunflag =1;
		DeltaKWH_Hour_temp.DCLoad_Total  = UINT_MAX;//	 
		DeltaKWH_Hour_temp.ACLoad_Total = UINT_MAX;//	 
		DeltaKWH_Hour_temp.Pv_dc = UINT_MAX;//  
		DeltaKWH_Hour_temp.Pv_ac = UINT_MAX;//  
		
		DeltaKWH_Hour_temp.GridChgin_Total = UINT_MAX;// 
		DeltaKWH_Hour_temp.GridFeedback_Total = UINT_MAX;// 
		DeltaKWH_Hour_temp.PVToload_Total = UINT_MAX;// 	 
		DeltaKWH_Hour_temp.PackDsg_Total = UINT_MAX;//	 

	}

    /*更新但不写入*/
    if(DeltaKWH_Hour_temp.DCLoad_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].DCLoad_Total)
    {
        DeltaKWH_Hour_temp.DCLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].DCLoad_Total;
        need_to_flash_WR_DCLoad_Total = 1;
        need_to_flash_WR_Hour = 1;
    }
    
    if(DeltaKWH_Hour_temp.ACLoad_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].ACLoad_Total)
    {
        DeltaKWH_Hour_temp.ACLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].ACLoad_Total;
        need_to_flash_WR_ACLoad_Total = 1;
        need_to_flash_WR_Hour = 1;
    }


    if(DeltaKWH_Hour_temp.Pv_dc != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].Pv_dc)
    {
        DeltaKWH_Hour_temp.Pv_dc = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].Pv_dc;
        need_to_flash_WR_Pv_Total = 1;
        need_to_flash_WR_Hour = 1;
    }

    if(DeltaKWH_Hour_temp.GridChgin_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridChgin_Total)
    {
        DeltaKWH_Hour_temp.GridChgin_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridChgin_Total;
        need_to_flash_WR_GridChgin_Total = 1;
        need_to_flash_WR_Hour = 1;
    }
    
    if(DeltaKWH_Hour_temp.GridFeedback_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridFeedback_Total)
    {
        DeltaKWH_Hour_temp.GridFeedback_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridFeedback_Total;
        need_to_flash_WR_GridFeedback_Total = 1;
        need_to_flash_WR_Hour = 1;
    }


    if(DeltaKWH_Hour_temp.PVToload_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PVToload_Total)
    {
        DeltaKWH_Hour_temp.PVToload_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PVToload_Total;
        need_to_flash_WR_PVToload_Total = 1;
        need_to_flash_WR_Hour = 1;
    }

    if(DeltaKWH_Hour_temp.PackDsg_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PackDsg_Total)
    {
        DeltaKWH_Hour_temp.PackDsg_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PackDsg_Total;
        need_to_flash_WR_PackDsg_Total = 1;
        need_to_flash_WR_Hour = 1;
    }



    if(1 == need_to_flash_WR_DCLoad_Total)
    {
        reals.energy_DCLoad_Total[KWH_INFO_ALONE].type = DC_LOAD_ENERGY_TYPE;
        reals.energy_DCLoad_Total[KWH_INFO_ALONE].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_DCLoad_Total[KWH_INFO_ALONE].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].DCLoad_Total;
        
        reals.energy_DCLoad_Total[KWH_INFO_ALONE].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].DCLoad_Total; // 月份 1~12
        reals.energy_DCLoad_Total[KWH_INFO_ALONE].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].DCLoad_Total;; // 日期 1~31
    }
    
    if(1 == need_to_flash_WR_ACLoad_Total)
    {
        reals.energy_ACLoad_Total[KWH_INFO_ALONE].type = AC_LOAD_ENERGY_TYPE;
        reals.energy_ACLoad_Total[KWH_INFO_ALONE].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_ACLoad_Total[KWH_INFO_ALONE].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].ACLoad_Total;
        
        reals.energy_ACLoad_Total[KWH_INFO_ALONE].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].ACLoad_Total; // 月份 1~12
        reals.energy_ACLoad_Total[KWH_INFO_ALONE].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].ACLoad_Total;; // 日期 1~31
    }
    if(1 == need_to_flash_WR_Pv_Total)
    {
        reals.energy_Pv_dc[KWH_INFO_ALONE].type = PV_ENERGY_TYPE;
        reals.energy_Pv_dc[KWH_INFO_ALONE].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_Pv_dc[KWH_INFO_ALONE].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].Pv_dc;
        
        reals.energy_Pv_dc[KWH_INFO_ALONE].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].Pv_dc; // 月份 1~12
        reals.energy_Pv_dc[KWH_INFO_ALONE].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].Pv_dc;; // 日期 1~31

    }
    
    if(1 == need_to_flash_WR_GridChgin_Total)
    {
        reals.energy_GridChgin_Total[KWH_INFO_ALONE].type = GRID_ENERGY_INPUT_TYPE;
        reals.energy_GridChgin_Total[KWH_INFO_ALONE].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_GridChgin_Total[KWH_INFO_ALONE].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].GridChgin_Total;
        
        reals.energy_GridChgin_Total[KWH_INFO_ALONE].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].GridChgin_Total; // 月份 1~12
        reals.energy_GridChgin_Total[KWH_INFO_ALONE].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].GridChgin_Total;; // 日期 1~31
    }
    if(1 == need_to_flash_WR_GridFeedback_Total)
    {
        reals.energy_GridFeedback_Total[KWH_INFO_ALONE].type = GRID_ENERGY_OUTPUT_TYPE;
        reals.energy_GridFeedback_Total[KWH_INFO_ALONE].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_GridFeedback_Total[KWH_INFO_ALONE].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].GridFeedback_Total;
        
        reals.energy_GridFeedback_Total[KWH_INFO_ALONE].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].GridFeedback_Total; // 月份 1~12
        reals.energy_GridFeedback_Total[KWH_INFO_ALONE].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].GridFeedback_Total;; // 日期 1~31
    }
    
    if(1 == need_to_flash_WR_PVToload_Total)
    {
        reals.energy_PVToload_Total[KWH_INFO_ALONE].type = PV_TO_ACLOAD_ENERGY_TYPE;
        reals.energy_PVToload_Total[KWH_INFO_ALONE].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_PVToload_Total[KWH_INFO_ALONE].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].PVToload_Total;
        
        reals.energy_PVToload_Total[KWH_INFO_ALONE].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].PVToload_Total; // 月份 1~12
        reals.energy_PVToload_Total[KWH_INFO_ALONE].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].PVToload_Total;; // 日期 1~31
    }	

    if(1 == need_to_flash_WR_PackDsg_Total)
    {
        reals.energy_PackDsg_Total[KWH_INFO_ALONE].type = BAT_TOTAL_DISCHARGE_ENERGY_TYPE;
        reals.energy_PackDsg_Total[KWH_INFO_ALONE].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_PackDsg_Total[KWH_INFO_ALONE].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_ALONE].PackDsg_Total;
        
        reals.energy_PackDsg_Total[KWH_INFO_ALONE].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_ALONE].PackDsg_Total; // 月份 1~12
        reals.energy_PackDsg_Total[KWH_INFO_ALONE].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_ALONE].PackDsg_Total;; // 日期 1~31
    }	

    
    if(1 == need_to_flash_WR_Hour)
    {
        reals.hour_energy[KWH_INFO_ALONE].DCLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].DCLoad_Total;
        reals.hour_energy[KWH_INFO_ALONE].ACLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].ACLoad_Total;
        reals.hour_energy[KWH_INFO_ALONE].Pv_dc = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].Pv_dc;
        reals.hour_energy[KWH_INFO_ALONE].GridChgin_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridChgin_Total;
        reals.hour_energy[KWH_INFO_ALONE].GridFeedback_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].GridFeedback_Total;
        reals.hour_energy[KWH_INFO_ALONE].PVToload_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PVToload_Total;
        reals.hour_energy[KWH_INFO_ALONE].PackDsg_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_ALONE].PackDsg_Total;

		
        memcpy((uint8_t *)&reals.hour_energy_time, (uint8_t *)&SetData.dev_info_t.iot_kwh_info.Energy_time_message, sizeof(reals.hour_energy_time));
    }

//	ESP_LOGI(TAG, "hour DCLoad_Total alone	=%d",(unsigned int)reals.hour_energy[KWH_INFO_ALONE].DCLoad_Total);
//	ESP_LOGI(TAG, "hour ACLoad_Total alone	=%d",(unsigned int)reals.hour_energy[KWH_INFO_ALONE].ACLoad_Total);
//	ESP_LOGI(TAG, "hour Pv_dc alone	=%d",(unsigned int)reals.hour_energy[KWH_INFO_ALONE].Pv_dc);
//	ESP_LOGI(TAG, "hour GridChgin_Total alone	=%d",(unsigned int)reals.hour_energy[KWH_INFO_ALONE].GridChgin_Total);

}

/*------------------------------------------------------------------------------
 Function: iot_kwh_data_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      更新信息至缓存，预备写入文件系统
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_kwh_sum_data_update(void)
{
    
    uint8_t need_to_flash_WR_Hour_Sum=0; 
	uint8_t need_to_flash_WR_DCLoad_Total_Sum = 0;	
	uint8_t need_to_flash_WR_ACLoad_Total_Sum = 0;
    uint8_t need_to_flash_WR_Pv_Total_Sum = 0;	
	uint8_t need_to_flash_WR_GridChgin_Total_Sum = 0;	
	uint8_t need_to_flash_WR_GridFeedback_Total_Sum = 0;
    uint8_t need_to_flash_WR_PVToload_Total_Sum = 0;	
    uint8_t need_to_flash_WR_PackDsg_Total_Sum = 0;	

	static baseKWH_energy_t_u16 DeltaKWH_Hour_temp_Sum;//
    static uint8_t srunflag=0; 

	if(0 == srunflag)
	{
		srunflag =1;
		DeltaKWH_Hour_temp_Sum.DCLoad_Total  = UINT_MAX;//	 
		DeltaKWH_Hour_temp_Sum.ACLoad_Total = UINT_MAX;//	 
		DeltaKWH_Hour_temp_Sum.Pv_dc = UINT_MAX;//  
		DeltaKWH_Hour_temp_Sum.Pv_ac = UINT_MAX;//  
		
		DeltaKWH_Hour_temp_Sum.GridChgin_Total = UINT_MAX;// 
		DeltaKWH_Hour_temp_Sum.GridFeedback_Total = UINT_MAX;// 
		DeltaKWH_Hour_temp_Sum.PVToload_Total = UINT_MAX;// 	 
		DeltaKWH_Hour_temp_Sum.PackDsg_Total = UINT_MAX;//	 

	}

    /*更新但不写入*/
    if(DeltaKWH_Hour_temp_Sum.DCLoad_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].DCLoad_Total)
    {
        DeltaKWH_Hour_temp_Sum.DCLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].DCLoad_Total;
        need_to_flash_WR_DCLoad_Total_Sum = 1;
        need_to_flash_WR_Hour_Sum = 1;
    }
    
    if(DeltaKWH_Hour_temp_Sum.ACLoad_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].ACLoad_Total)
    {
        DeltaKWH_Hour_temp_Sum.ACLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].ACLoad_Total;
        need_to_flash_WR_ACLoad_Total_Sum = 1;
        need_to_flash_WR_Hour_Sum = 1;
    }


    if(DeltaKWH_Hour_temp_Sum.Pv_dc != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].Pv_dc)
    {
        DeltaKWH_Hour_temp_Sum.Pv_dc = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].Pv_dc;
        need_to_flash_WR_Pv_Total_Sum = 1;
        need_to_flash_WR_Hour_Sum = 1;
    }

    if(DeltaKWH_Hour_temp_Sum.GridChgin_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridChgin_Total)
    {
        DeltaKWH_Hour_temp_Sum.GridChgin_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridChgin_Total;
        need_to_flash_WR_GridChgin_Total_Sum = 1;
        need_to_flash_WR_Hour_Sum = 1;
    }
    
    if(DeltaKWH_Hour_temp_Sum.GridFeedback_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridFeedback_Total)
    {
        DeltaKWH_Hour_temp_Sum.GridFeedback_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridFeedback_Total;
        need_to_flash_WR_GridFeedback_Total_Sum = 1;
        need_to_flash_WR_Hour_Sum = 1;
    }


    if(DeltaKWH_Hour_temp_Sum.PVToload_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PVToload_Total)
    {
        DeltaKWH_Hour_temp_Sum.PVToload_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PVToload_Total;
        need_to_flash_WR_PVToload_Total_Sum = 1;
        need_to_flash_WR_Hour_Sum = 1;
    }

    if(DeltaKWH_Hour_temp_Sum.PackDsg_Total != SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PackDsg_Total)
    {
        DeltaKWH_Hour_temp_Sum.PackDsg_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PackDsg_Total;
        need_to_flash_WR_PackDsg_Total_Sum = 1;
        need_to_flash_WR_Hour_Sum = 1;
    }



    if(1 == need_to_flash_WR_DCLoad_Total_Sum)
    {
        reals.energy_DCLoad_Total[KWH_INFO_SUM].type = DC_LOAD_ENERGY_TYPE;
        reals.energy_DCLoad_Total[KWH_INFO_SUM].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_DCLoad_Total[KWH_INFO_SUM].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].DCLoad_Total;
        
        reals.energy_DCLoad_Total[KWH_INFO_SUM].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].DCLoad_Total; // 月份 1~12
        reals.energy_DCLoad_Total[KWH_INFO_SUM].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].DCLoad_Total;; // 日期 1~31
    }
    
    if(1 == need_to_flash_WR_ACLoad_Total_Sum)
    {
        reals.energy_ACLoad_Total[KWH_INFO_SUM].type = AC_LOAD_ENERGY_TYPE;
        reals.energy_ACLoad_Total[KWH_INFO_SUM].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_ACLoad_Total[KWH_INFO_SUM].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].ACLoad_Total;
        
        reals.energy_ACLoad_Total[KWH_INFO_SUM].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].ACLoad_Total; // 月份 1~12
        reals.energy_ACLoad_Total[KWH_INFO_SUM].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].ACLoad_Total;; // 日期 1~31
    }
    if(1 == need_to_flash_WR_Pv_Total_Sum)
    {
        reals.energy_Pv_dc[KWH_INFO_SUM].type = PV_ENERGY_TYPE;
        reals.energy_Pv_dc[KWH_INFO_SUM].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_Pv_dc[KWH_INFO_SUM].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].Pv_dc;
        
        reals.energy_Pv_dc[KWH_INFO_SUM].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].Pv_dc; // 月份 1~12
        reals.energy_Pv_dc[KWH_INFO_SUM].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].Pv_dc;; // 日期 1~31

    }
    
    if(1 == need_to_flash_WR_GridChgin_Total_Sum)
    {
        reals.energy_GridChgin_Total[KWH_INFO_SUM].type = GRID_ENERGY_INPUT_TYPE;
        reals.energy_GridChgin_Total[KWH_INFO_SUM].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_GridChgin_Total[KWH_INFO_SUM].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].GridChgin_Total;
        
        reals.energy_GridChgin_Total[KWH_INFO_SUM].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].GridChgin_Total; // 月份 1~12
        reals.energy_GridChgin_Total[KWH_INFO_SUM].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].GridChgin_Total;; // 日期 1~31
    }
    if(1 == need_to_flash_WR_GridFeedback_Total_Sum)
    {
        reals.energy_GridFeedback_Total[KWH_INFO_SUM].type = GRID_ENERGY_OUTPUT_TYPE;
        reals.energy_GridFeedback_Total[KWH_INFO_SUM].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_GridFeedback_Total[KWH_INFO_SUM].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].GridFeedback_Total;
        
        reals.energy_GridFeedback_Total[KWH_INFO_SUM].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].GridFeedback_Total; // 月份 1~12
        reals.energy_GridFeedback_Total[KWH_INFO_SUM].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].GridFeedback_Total;; // 日期 1~31
    }
    
    if(1 == need_to_flash_WR_PVToload_Total_Sum)
    {
        reals.energy_PVToload_Total[KWH_INFO_SUM].type = PV_TO_ACLOAD_ENERGY_TYPE;
        reals.energy_PVToload_Total[KWH_INFO_SUM].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_PVToload_Total[KWH_INFO_SUM].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].PVToload_Total;
        
        reals.energy_PVToload_Total[KWH_INFO_SUM].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].PVToload_Total; // 月份 1~12
        reals.energy_PVToload_Total[KWH_INFO_SUM].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].PVToload_Total;; // 日期 1~31
    }	

    if(1 == need_to_flash_WR_PackDsg_Total_Sum)
    {
        reals.energy_PackDsg_Total[KWH_INFO_SUM].type = BAT_TOTAL_DISCHARGE_ENERGY_TYPE;
        reals.energy_PackDsg_Total[KWH_INFO_SUM].year = SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year+2000;
        reals.energy_PackDsg_Total[KWH_INFO_SUM].total_energy = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[KWH_INFO_SUM].PackDsg_Total;
        
        reals.energy_PackDsg_Total[KWH_INFO_SUM].mouth_energy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[KWH_INFO_SUM].PackDsg_Total; // 月份 1~12
        reals.energy_PackDsg_Total[KWH_INFO_SUM].day_enengy[SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday-1] = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[KWH_INFO_SUM].PackDsg_Total;; // 日期 1~31
    }	

    
    if(1 == need_to_flash_WR_Hour_Sum)
    {
        reals.hour_energy[KWH_INFO_SUM].DCLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].DCLoad_Total;
        reals.hour_energy[KWH_INFO_SUM].ACLoad_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].ACLoad_Total;
        reals.hour_energy[KWH_INFO_SUM].Pv_dc = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].Pv_dc;
        reals.hour_energy[KWH_INFO_SUM].GridChgin_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridChgin_Total;
        reals.hour_energy[KWH_INFO_SUM].GridFeedback_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].GridFeedback_Total;
        reals.hour_energy[KWH_INFO_SUM].PVToload_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PVToload_Total;
        reals.hour_energy[KWH_INFO_SUM].PackDsg_Total = SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[KWH_INFO_SUM].PackDsg_Total;

		// ESP_LOGI(TAG, "hour DCLoad_Total sum	=%d",(unsigned int)reals.hour_energy[KWH_INFO_SUM].DCLoad_Total);
		// ESP_LOGI(TAG, "hour ACLoad_Total sum	=%d",(unsigned int)reals.hour_energy[KWH_INFO_SUM].ACLoad_Total);
		// ESP_LOGI(TAG, "hour Pv_dc sum	=%d",(unsigned int)reals.hour_energy[KWH_INFO_SUM].Pv_dc);
		// ESP_LOGI(TAG, "hour GridChgin_Total sum	=%d",(unsigned int)reals.hour_energy[KWH_INFO_SUM].GridChgin_Total);

        memcpy((uint8_t *)&reals.hour_energy_time, (uint8_t *)&SetData.dev_info_t.iot_kwh_info.Energy_time_message, sizeof(reals.hour_energy_time));
    }

}


/*------------------------------------------------------------------------------
 Function: kwh_locol_storage_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      kwh数据存储
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_kwh_local_storage_task(void)//1s cycle
{ 
    static int change_rtc_year = 0;  
    static int change_rtc_month = 0;
	uint8_t i=0; 

	if(1==reals.st_FlagTime.bits.b1FlagSys1s)
	{
		/*更新kwh电量信息及增量*/
        iot_kwh_status_check();
		iot_kwh_sum_status_check();
        
        //避免特殊情况频繁写入时，上次数据未写入完成就被刷新
        if (0 == reals.kwhWrFlag.Byte2)
        {
            if (change_rtc_year != 0)
            {
				for(i=0;i<KWH_INFO_MAX_NUM;i++)
				{
					memset((uint8_t *)&(reals.energy_DCLoad_Total[i].mouth_energy[0]), 0, sizeof(reals.energy_DCLoad_Total[i].mouth_energy));
					memset((uint8_t *)&(reals.energy_ACLoad_Total[i].mouth_energy[0]), 0, sizeof(reals.energy_ACLoad_Total[i].mouth_energy));
					memset((uint8_t *)&(reals.energy_Pv_dc[i].mouth_energy[0]), 0, sizeof(reals.energy_Pv_dc[i].mouth_energy));
					memset((uint8_t *)&(reals.energy_GridChgin_Total[i].mouth_energy[0]), 0, sizeof(reals.energy_GridChgin_Total[i].mouth_energy));
					memset((uint8_t *)&(reals.energy_GridFeedback_Total[i].mouth_energy[0]), 0, sizeof(reals.energy_GridFeedback_Total[i].mouth_energy));
					memset((uint8_t *)&(reals.energy_PVToload_Total[i].mouth_energy[0]), 0, sizeof(reals.energy_PVToload_Total[i].mouth_energy));
					memset((uint8_t *)&(reals.energy_PackDsg_Total[i].mouth_energy[0]), 0, sizeof(reals.energy_PackDsg_Total[i].mouth_energy));
				}
                change_rtc_year = 0;

                /*年文件内部偏移*/
                SetData.dev_info_t.iot_kwh_info.kwh_years_num++;
				reals.SetDataWrFlag.sBit.kwh_mix =1;
            }
            
            if (change_rtc_month != 0)
            {
				for(i=0;i<KWH_INFO_MAX_NUM;i++)
				{
					memset((uint8_t *)&(reals.energy_DCLoad_Total[i].day_enengy[0]), 0, sizeof(reals.energy_DCLoad_Total[i].day_enengy));
					memset((uint8_t *)&(reals.energy_ACLoad_Total[i].day_enengy[0]), 0, sizeof(reals.energy_ACLoad_Total[i].day_enengy));
					memset((uint8_t *)&(reals.energy_Pv_dc[i].day_enengy[0]), 0, sizeof(reals.energy_Pv_dc[i].day_enengy));
					memset((uint8_t *)&(reals.energy_GridChgin_Total[i].day_enengy[0]), 0, sizeof(reals.energy_GridChgin_Total[i].day_enengy));
					memset((uint8_t *)&(reals.energy_GridFeedback_Total[i].day_enengy[0]), 0, sizeof(reals.energy_GridFeedback_Total[i].day_enengy));
					memset((uint8_t *)&(reals.energy_PVToload_Total[i].day_enengy[0]), 0, sizeof(reals.energy_PVToload_Total[i].day_enengy));
					memset((uint8_t *)&(reals.energy_PackDsg_Total[i].day_enengy[0]), 0, sizeof(reals.energy_PackDsg_Total[i].day_enengy));
				}
			   
			    change_rtc_month = 0;
            }

            /*更新信息至缓存，预备写入文件系统*/
            iot_kwh_data_update();
            iot_kwh_sum_data_update();

            uint8_t update_flag = 0;
          
        	if(SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year != reals.rtc_time.year)//年变化，创建新当年文件 tm_year
        	{
        	    update_flag = 1;
        	}	
        	else if(SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon != reals.rtc_time.mon)//月变化，要覆盖当年文件，创建新当月文件 tm_mon
        	{
        	    update_flag = 2;
        	}	
        	else if(SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday != reals.rtc_time.day)//日变化，要覆盖当年文件 tm_mday
        	{
        	    update_flag = 3;
        	}	
        	else if(SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_hour != reals.rtc_time.hour)//时变化，要偏移写入当月文件 tm_hour
        	{
        	    update_flag = 4;
        	}	
			ESP_LOGI(TAG, "update_flag	=%d",update_flag);
			//ESP_LOGI(TAG, "su8tm_year	=%d",SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year);
            //ESP_LOGI(TAG, "su8tm_hour	=%d",SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_hour);
			/*时间变化写入，更新时间戳*/
            if ( update_flag )
            {
                memcpy((uint8_t *)&reals.hour_energy_time, (uint8_t *)&SetData.dev_info_t.iot_kwh_info.Energy_time_message, sizeof(reals.hour_energy_time));
        
                switch (update_flag)//不使用break，从最高变化级从上往下依次执行
                {
                    case 1 ://year
                        
                        if(SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year != 0)
                        {
                            change_rtc_year = reals.rtc_time.year - SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year;
                        }
                        SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_year = reals.rtc_time.year;
						reals.SetDataWrFlag.sBit.kwh_mix =1;
                        ESP_LOGI(TAG, "rtc_time.year	=%d",reals.rtc_time.year);
                        memset(&SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year[0], 0, sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Year));
        //                break;
                        
                    case 2 ://month
            
                        change_rtc_month = reals.rtc_time.mon - SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon;
                        SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mon = reals.rtc_time.mon;
						reals.SetDataWrFlag.sBit.kwh_mix =1;
                        
                        memset(&SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month[0], 0, sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Month));
        //                break;
        
                    case 3 ://day
                        
                		SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_mday = reals.rtc_time.day;
						reals.SetDataWrFlag.sBit.kwh_mix =1;

                        memset(&SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day[0], 0, sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Day));

				        reals.kwhWrFlag.sBit.year_DCLoad_Total=1;
				        reals.kwhWrFlag.sBit.year_ACLoad_Total=1;
				        reals.kwhWrFlag.sBit.year_Pv_Total=1;
				        reals.kwhWrFlag.sBit.year_GridChgin_Total=1;
				        reals.kwhWrFlag.sBit.year_GridFeedback_Total=1;
				        reals.kwhWrFlag.sBit.year_PVToload_Total=1;
				        reals.kwhWrFlag.sBit.year_PackDsg_Total=1;

						reals.kwhWrFlag.sBit.year_DCLoad_Total_Sum=1;
				        reals.kwhWrFlag.sBit.year_ACLoad_Total_Sum=1;
				        reals.kwhWrFlag.sBit.year_Pv_Total_Sum=1;
				        reals.kwhWrFlag.sBit.year_GridChgin_Total_Sum=1;
				        reals.kwhWrFlag.sBit.year_GridFeedback_Total_Sum=1;
				        reals.kwhWrFlag.sBit.year_PVToload_Total_Sum=1;
				        reals.kwhWrFlag.sBit.year_PackDsg_Total_Sum=1;
        
                    case 4 ://hour
                        
                		SetData.dev_info_t.iot_kwh_info.Energy_time_message.su8tm_hour = reals.rtc_time.hour;
						reals.SetDataWrFlag.sBit.kwh_mix =1;
						ESP_LOGI(TAG,"hour 0 update pv:%lu grid:%u",SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[0].Pv_dc,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[0].GridChgin_Total);
						ESP_LOGI(TAG,"hour 1 update pv:%lu grid:%u",SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[1].Pv_dc,SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[1].GridChgin_Total);
                        memset(&SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour[0], 0, sizeof(SetData.dev_info_t.iot_kwh_info.DeltaKWH_Hour));

				        reals.kwhWrFlag.sBit.hour_file=1;
				        reals.kwhWrFlag.sBit.hour_file_Sum=1;
        //                break;
        
                    default:
        
                        break;
                }
            }
        }
    }
}

/*------------------------------------------------------------------------------
 Function: print_heap_info
 -----------------------------------------------------------------------------*/
/**
  * @brief      调试打印
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
void print_heap_info(void) {
    uint32_t total_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t total_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint32_t total_min = esp_get_minimum_free_heap_size();

    uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    uint32_t internal_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);

    uint32_t external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t external_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    uint32_t external_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "+------------+------------+------------+------------+------+");
    ESP_LOGI(TAG, "| Region     | Free(B)    | Largest(B) | Min(B)     | Frag |");
    ESP_LOGI(TAG, "+------------+------------+------------+------------+------+");
    ESP_LOGI(TAG, "| Total      | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %3u%% |",
             total_free, total_largest, total_min, calc_heap_frag_pct(total_free, total_largest));
    ESP_LOGI(TAG, "| Internal   | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %3u%% |",
             internal_free, internal_largest, internal_min,
             calc_heap_frag_pct(internal_free, internal_largest));
    ESP_LOGI(TAG, "| External   | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %3u%% |",
             external_free, external_largest, external_min,
             calc_heap_frag_pct(external_free, external_largest));
    ESP_LOGI(TAG, "+------------+------------+------------+------------+------+");
}

/*------------------------------------------------------------------------------
 Function: print_task_status
 -----------------------------------------------------------------------------*/
/**
  * @brief      任务状态监控功能
  * @param[in]  None
  * @param[out] None
  * @return     void
  */
void print_task_status() {
    TaskStatus_t *task_array;
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    task_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));

    if (task_array != NULL) {
        task_count = uxTaskGetSystemState(task_array, task_count, NULL);
        ESP_LOGI(TAG, "Task count: %d", task_count);
        for (UBaseType_t i = 0; i < task_count; i++) {
            ESP_LOGI(TAG, "Task: %s, State: %d, Priority: %d, Stack: %lu",
                     task_array[i].pcTaskName,
                     task_array[i].eCurrentState,
                     task_array[i].uxCurrentPriority,
                     task_array[i].usStackHighWaterMark);
        }
        vPortFree(task_array);
    }
}

/*------------------------------------------------------------------------------
 Function: printRemainingFileDescriptors
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取当前进程的资源使用情况
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void printRemainingFileDescriptors(void) {
    // 获取当前进程的资源使用情况
//    struct rlimit rlim;
//    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) 
    {
    static uint16_t  sold=0;
//        // 打印软限制和当前使用的文件描述符数量
//        printf("文件描述符软限制: %lu\n", (unsigned long)rlim.rlim_cur);
//        printf("文件描述符硬限制: %lu\n", (unsigned long)rlim.rlim_max);

        // 获取当前打开的文件描述符数量
        uint16_t openFds = 0;
        for (int fd = 0; fd < 1024; fd++) 
		{
            if (fcntl(fd, F_GETFL) != -1) 
			{
                printf("%d ",fd);
                openFds++;
            }
        }
        printf("\n ");
        ESP_LOGW(TAG, "The number of currently open file descriptors : %d", openFds);
//        ESP_LOGW(TAG, "Number of remaining file descriptors : %lu", (unsigned long)(1024 - openFds));
		if(sold != openFds)
		{
			sold = openFds;
			SaveOneEvent(openFds,0,CODE_ALARM_SPI_FLASH_ERR,0);			
		}

    } 
//    else 
//    {
//        perror("getrlimit");
//    }
}



void Cellular_4G_State_Print(void) 
{
//	ESP_LOGI(TAG, "4G running step: %s", mqtt_step_str[Cellular4G_mqtt_step]); 
	
//	ESP_LOGI(TAG, "reals.Cellular4G_step =%d",reals.Cellular4G_step);
//	ESP_LOGI(TAG, "reals.Ethernet_SPI_step =%d",reals.Ethernet_SPI_step);
//
//	ESP_LOGI(TAG, "reals.event_eth_register_flag =%d",reals.event_eth_register_flag);
//	ESP_LOGI(TAG, "reals.event_ppp_register_flag =%d",reals.event_ppp_register_flag);
//	ESP_LOGI(TAG, "reals.event_wifi_register_flag =%d",reals.event_wifi_register_flag);
//	
//		ESP_LOGI(TAG, "SetData.dev_info_t.on_off.all =0x%lx",SetData.dev_info_t.on_off.all);
//	
//
//		
/*
	ESP_LOGI(TAG, "Inv_can[0].inv_pv[0].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[0].inv_data[0].inv_pv.pv_detail[0].input_power,Inv_can[0].inv_data[0].inv_pv.pv_detail[0].input_voltage);
	ESP_LOGI(TAG, "Inv_can[0].inv_pv[1].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[0].inv_data[0].inv_pv.pv_detail[1].input_power,Inv_can[0].inv_data[0].inv_pv.pv_detail[1].input_voltage);
	
	ESP_LOGI(TAG, "Inv_can[1].inv_pv[0].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[1].inv_data[0].inv_pv.pv_detail[0].input_power,Inv_can[1].inv_data[0].inv_pv.pv_detail[0].input_voltage);
	ESP_LOGI(TAG, "Inv_can[1].inv_pv[1].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[1].inv_data[0].inv_pv.pv_detail[1].input_power,Inv_can[1].inv_data[0].inv_pv.pv_detail[1].input_voltage);

	ESP_LOGI(TAG, "Inv_can[2].inv_pv[0].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[2].inv_data[0].inv_pv.pv_detail[0].input_power,Inv_can[2].inv_data[0].inv_pv.pv_detail[0].input_voltage);
	ESP_LOGI(TAG, "Inv_can[2].inv_pv[1].input_voltage =%u,inv_pv.input_voltage =%u",Inv_can[2].inv_data[0].inv_pv.pv_detail[1].input_power,Inv_can[2].inv_data[0].inv_pv.pv_detail[1].input_voltage);
	*/
	//ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_dc:%d ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_dc);
	//ESP_LOGI(TAG, "reals.debug1  Udp_singlecast_step =%d",reals.debug1);

	//ESP_LOGI(TAG, "reals.debug2  Udp_Multicast_step =%d",reals.debug2);
	ESP_LOGI(TAG,"reals.achub_grid_transfer_flag:%d",reals.achub_grid_transfer_flag);
	#if 0
	 // 获取当前 Wi-Fi 发射功率
	 int8_t tx_power;
    esp_wifi_get_max_tx_power(&tx_power);
    printf("Current Wi-Fi TX power: %d dBm\n", tx_power);

	    // 获取广播功率
    esp_power_level_t adv_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
    printf("Advertising TX power: %d dBm\n", adv_power);

    // 获取扫描功率
    esp_power_level_t scan_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_SCAN);
    printf("Scanning TX power: %d dBm\n", scan_power);

    // 获取默认功率
    esp_power_level_t default_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
    printf("Default TX power: %d dBm\n", default_power);
	// 获取当前蓝牙发射功率
	esp_power_level_t CH0_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_CONN_HDL0);
    printf("CONN_HDL0 TX power: %d dBm\n", CH0_power);

	// 获取当前蓝牙发射功率
	esp_power_level_t CH1_power = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_CONN_HDL1);
    printf("CONN_HDL1 TX power: %d dBm\n", CH1_power);
	#endif
	// ESP_LOGI(TAG,"inv :0x%x line_event:0x%x -0x%x",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.line_event,Inv_can[reals.Addr_can_self].inv_data[0].inv_base.line_event,Inv_can[reals.Addr_can_self].inv_data[0].inv_line.line_event );
	// for(int num=0;num<3;num++)
	// {
	// ESP_LOGI(TAG,"pv%d power:0x%u,volt:%u,current:%u",num,Inv_can[reals.Addr_can_self].inv_data[0].inv_pv.pv_detail[num].input_power,
	// 		Inv_can[reals.Addr_can_self].inv_data[0].inv_pv.pv_detail[num].input_voltage,
	// 		Inv_can[reals.Addr_can_self].inv_data[0].inv_pv.pv_detail[num].input_current);
	// }
	// ESP_LOGI(TAG,"PVAllTotalPower Inv:%lu  inv_Can:%lu  %lu",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PVAllTotalPower,Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower,Inv_can[reals.Addr_can_self].inv_data[0].inv_base.PVAllTotalPower);

	// ESP_LOGW(TAG,"can self addr:%d", reals.Addr_can_self);
	// 打印当前6000段的work_status
	// ESP_LOGW(TAG,"Inv[reals.Addr_can_self].mod_reg06000_Pack_sum.work_status:%x", Inv[reals.Addr_can_self].mod_reg06000_Pack_sum.work_status);
	// ESP_LOGI(TAG,"can_ota_status[0].sender_work:%d",can_ota_status[0].sender_work);
	// ESP_LOGW(TAG,"Inv[0].mod_reg00100_AppPage1:%u-%u-%u-%u-%u-%u",Inv[0].mod_reg00100_AppPage1.Iot_State.all,Inv[0].mod_reg00100_AppPage1.Fault,Inv[0].mod_reg00100_AppPage1.Alarm,Inv[0].mod_reg00100_AppPage1.Parts_online.all,Inv[0].mod_reg00100_AppPage1.Rated_Frequency,Inv[0].mod_reg00100_AppPage1.Rated_Voltage);
	// ESP_LOGW(TAG,"Inv[1].mod_reg00100_AppPage1:%u-%u-%u-%u-%u-%u",Inv[1].mod_reg00100_AppPage1.Iot_State.all,Inv[1].mod_reg00100_AppPage1.Fault,Inv[1].mod_reg00100_AppPage1.Alarm,Inv[1].mod_reg00100_AppPage1.Parts_online.all,Inv[1].mod_reg00100_AppPage1.Rated_Frequency,Inv[1].mod_reg00100_AppPage1.Rated_Voltage);
	// ESP_LOGW(TAG,"Inv[2].mod_reg00100_AppPage1:%u-%u-%u-%u-%u-%u",Inv[2].mod_reg00100_AppPage1.Iot_State.all,Inv[2].mod_reg00100_AppPage1.Fault,Inv[2].mod_reg00100_AppPage1.Alarm,Inv[2].mod_reg00100_AppPage1.Parts_online.all,Inv[2].mod_reg00100_AppPage1.Rated_Frequency,Inv[2].mod_reg00100_AppPage1.Rated_Voltage);
	// ESP_LOGW(TAG,"Inv[3].mod_reg00100_AppPage1:%u-%u-%u-%u-%u-%u",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.all,Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Fault,Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Alarm,Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Parts_online.all,Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Rated_Frequency,Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Rated_Voltage);
	// ESP_LOGI(TAG,"SetData.dev_info_t.could_dns:%s",SetData.dev_info_t.could_dns);
	// ESP_LOGI(TAG,"SetData.dev_info_t.mobile_apn:%s",SetData.dev_info_t.mobile_apn);
	#if 0
	ESP_LOGI(TAG,"Inv[0].mod_reg01100_Inv_base.gridL1AcOutputRatedCurrent:%u",Inv[0].mod_reg01100_Inv_base.gridL1AcOutputRatedCurrent);
	ESP_LOGI(TAG,"Inv[0].mod_reg01100_Inv_base.gridL2AcOutputRatedCurrent:%u",Inv[0].mod_reg01100_Inv_base.gridL2AcOutputRatedCurrent);
	ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base.gridL1AcOutputRatedCurrent:%u",Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base.gridL1AcOutputRatedCurrent);
	ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base.gridL2AcOutputRatedCurrent:%u",Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base.gridL2AcOutputRatedCurrent);
	#endif
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg11000_IOT_info.software_ver:%lu",Inv_D400S[0].mod_reg11000_IOT_info.software_ver);
	// ESP_LOGI(TAG,"Inv_D400S[1].mod_reg11000_IOT_info.software_ver:%lu",Inv_D400S[1].mod_reg11000_IOT_info.software_ver);
	// ESP_LOGI(TAG,"Inv_D400S[2].mod_reg11000_IOT_info.software_ver:%lu",Inv_D400S[2].mod_reg11000_IOT_info.software_ver);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg11000_IOT_info.iot_sn:%llu",Inv_D400S[0].mod_reg11000_IOT_info.iot_sn);
	// ESP_LOGI(TAG,"Inv_D400S[1].mod_reg11000_IOT_info.iot_sn:%llu",Inv_D400S[1].mod_reg11000_IOT_info.iot_sn);
	// ESP_LOGI(TAG,"Inv_D400S[2].mod_reg11000_IOT_info.iot_sn:%llu",Inv_D400S[2].mod_reg11000_IOT_info.iot_sn);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg11000_IOT_info.iot_type:%s",Inv_D400S[0].mod_reg11000_IOT_info.iot_type);
	// ESP_LOGI(TAG,"Inv_D400S[1].mod_reg11000_IOT_info.iot_type:%s",Inv_D400S[1].mod_reg11000_IOT_info.iot_type);
	// ESP_LOGI(TAG,"Inv_D400S[2].mod_reg11000_IOT_info.iot_type:%s",Inv_D400S[2].mod_reg11000_IOT_info.iot_type);	
	
	//ESP_LOGI(TAG,"Inv[3].mod_reg02200_Inv_advance_set ctrl_output_inv_freq:%u ctrl_output_inv_volt:%u",Inv[3].mod_reg02200_Inv_advance_set.ctrl_output_inv_freq,Inv[3].mod_reg02200_Inv_advance_set.ctrl_output_inv_volt);
	//ESP_LOGI(TAG,"SetData_Can.dev_info_t2.inv_set00.ctrl_ac:%u",SetData_Can.dev_info_t2.inv_set00.ctrl_ac);
	//ESP_LOGI(TAG,"SetData_Can.dev_info_t2.inv_set00.ctrl_dc:%u",SetData_Can.dev_info_t2.inv_set00.ctrl_dc);
	// ESP_LOGI(TAG,"Inv_can[reals.Addr_can_self].inv_data[0].inv_set00.ctrl_ac:%u",Inv_can[reals.Addr_can_self].inv_data[0].inv_set00.ctrl_ac);
	// ESP_LOGI(TAG,"Inv_can[reals.Addr_can_self].inv_data[0].inv_set00.ctrl_dc:%u",Inv_can[reals.Addr_can_self].inv_data[0].inv_set00.ctrl_dc);
	#if 0
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.dc_info.dc_voltage:%u",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.dc_info[2].dc_voltage);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.dc_info.dc_current:%lu",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.dc_info[2].dc_current);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.dc_info.dc_power:%u",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.dc_info[2].dc_power);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.dcdc_SoftwareType:%u",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.dcdc_SoftwareType);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.dcdc_SoftwareVersion:%lu",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.dcdc_SoftwareVersion);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.total_input_power:%lu",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.total_input_power);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.total_output_power:%lu",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.total_output_power);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.TotalInputEnergy:%lu",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.TotalInputEnergy);
	// ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.energy_line.all:%x",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.energy_line.all);
	ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.fault_charger1:%u",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.fault_charger1);
	ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.fault_dcdc:%u",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.fault_dcdc);
	ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.proctect_dcdc:%u",Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.proctect_dcdc);

	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.dc_info.dc_voltage:%u",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.dc_info[2].dc_voltage);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.dc_info.dc_current:%lu",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.dc_info[2].dc_current);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.dc_info.dc_power:%u",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.dc_info[2].dc_power);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.dcdc_SoftwareType:%u",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.dcdc_SoftwareType);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.dcdc_SoftwareVersion:%lu",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.dcdc_SoftwareVersion);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.total_input_power:%lu",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.total_input_power);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.total_output_power:%lu",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.total_output_power);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.TotalInputEnergy:%lu",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.TotalInputEnergy);
	// ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.energy_line.all:%x",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.energy_line.all);
	ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.fault_charger1:%u",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.fault_charger1);
	ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.fault_dcdc:%u",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.fault_dcdc);
	ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.proctect_dcdc:%u",Inv_D400S[reals.online_X_inv_index].mod_reg15500_D400s_info.proctect_dcdc);


	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.dc_info.dc_voltage:%u",Inv_D400S[3].mod_reg15500_D400s_info.dc_info[2].dc_voltage);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.dc_info.dc_current:%lu",Inv_D400S[3].mod_reg15500_D400s_info.dc_info[2].dc_current);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.dc_info.dc_power:%u",Inv_D400S[3].mod_reg15500_D400s_info.dc_info[2].dc_power);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.dcdc_SoftwareType:%u",Inv_D400S[3].mod_reg15500_D400s_info.dcdc_SoftwareType);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.dcdc_SoftwareVersion:%lu",Inv_D400S[3].mod_reg15500_D400s_info.dcdc_SoftwareVersion);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.total_input_power:%lu",Inv_D400S[3].mod_reg15500_D400s_info.total_input_power);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.total_output_power:%lu",Inv_D400S[3].mod_reg15500_D400s_info.total_output_power);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.TotalInputEnergy:%lu",Inv_D400S[3].mod_reg15500_D400s_info.TotalInputEnergy);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.energy_line.all:%x",Inv_D400S[3].mod_reg15500_D400s_info.energy_line.all);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.fault_charger1:%u",Inv_D400S[3].mod_reg15500_D400s_info.fault_charger1);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.fault_dcdc:%u",Inv_D400S[3].mod_reg15500_D400s_info.fault_dcdc);
	// ESP_LOGI(TAG,"Inv_D400S[3].mod_reg15500_D400s_info.proctect_dcdc:%u",Inv_D400S[3].mod_reg15500_D400s_info.proctect_dcdc);
	#endif
	ESP_LOGI(TAG,"achubset:0x%x  ",Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting.all);
	//ESP_LOGI(TAG,"Inv_can[reals.online_X_inv_index].pack_data[reals.online_Y_inv_index].pack_announce.work_status:0x%x",Inv_can[reals.online_X_inv_index].pack_data[reals.online_Y_inv_index].pack_announce.work_status);
	//ESP_LOGI(TAG,"login_info.port:%u",login_info.port);
	ESP_LOGI(TAG,"Inv[3].mod_reg15500_D400s_info.d400s_type:%s",Inv[3].mod_reg15500_D400s_info.d400s_type);
	uint64_t d400sSn=GetUin64FromPtrSmall(Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info.d400s_sn);
    ESP_LOGI(TAG,"Inv_can[0].d400s_data[0].d400s_common_info.d400s_sn::%llu",d400sSn);
	ESP_LOGI(TAG,"reals.online_D400S_num:%d",reals.online_D400S_num);
	
	ESP_LOGI(TAG,"SetData_Can.dev_info_t2.inv_set01.Func_Set.all:0x%x",SetData_Can.dev_info_t2.inv_set01.Func_Set.all);
	ESP_LOGI(TAG,"Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set.all:0x%x",Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set.all);
	ESP_LOGI(TAG, "reals.Step_dev_discovery   =%d",reals.Step_dev_discovery);
	ESP_LOGI(TAG, "reals.wifi_connect_STA   =%d",reals.wifi_connect_STA);
	ESP_LOGI(TAG, "reals.wifi_connect_ap    =%d",reals.wifi_connect_ap);
    ESP_LOGI(TAG, "MQTT login status:%d", iot_mqtt_login_state());

	ESP_LOGI(TAG, "SetData.dev_info_t.on_off.bit.wifi_enable    =%d",SetData.dev_info_t.on_off.bit.wifi_enable);
	ESP_LOGI(TAG, "reals.wifi_eventgroup    =%d",reals.wifi_eventgroup);
		
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
	ESP_LOGI(TAG, "reals.wifi_mesh_root    =%d",reals.wifi_mesh_root);
#endif
	ESP_LOGI(TAG, "login_info.current_netif_id    =%d",login_info.current_netif_id);

	
	ESP_LOGI(TAG, "AA wifi name SetData: %s, password: %s, auth mode: %d", SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password, SetData.dev_info_t.wifi_sta_auth);

	ESP_LOGI(TAG, "AA wifi name:g_self_data  %s, password: %s, auth mode: %d", g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid, g_self_data.mod_reg12000_IOT_set.wifi_sta_password, g_self_data.mod_reg12000_IOT_set.wifi_sta_auth);

	ESP_LOGI(TAG, "AA wifi name:g_self_data_WR  %s, password: %s, auth mode: %d", Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid, Inv_WR.mod_reg12000_IOT_set.wifi_sta_password, Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth);
	
	ESP_LOGI(TAG,"Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid:%s,wifi_sta_password:%s",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_password);

	ESP_LOGI(TAG,"g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid::%s ,wifi_sta_password:%s",g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid,g_self_data.mod_reg12000_IOT_set.wifi_sta_password);
	ESP_LOGI(TAG,"wifi_sta_auth:%u,SetData.dev_info_t.wifi_sta_ssid:%s,SetData.dev_info_t.wifi_sta_password:%s",SetData.dev_info_t.wifi_sta_auth, SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password);
	// ESP_LOGI(TAG, "reals.self_wifi_ap_ip =%d,%d,%d,%d,",reals.self_wifi_ap_ip[0],reals.self_wifi_ap_ip[1],reals.self_wifi_ap_ip[2],reals.self_wifi_ap_ip[3]);

	// ESP_LOGI(TAG, "reals ip =%d,%d,%d,%d,",g_self_data.mod_reg11000_IOT_info.sta_ipv4[0],g_self_data.mod_reg11000_IOT_info.sta_ipv4[1],g_self_data.mod_reg11000_IOT_info.sta_ipv4[2],g_self_data.mod_reg11000_IOT_info.sta_ipv4[3]);
	// ESP_LOGI(TAG, "Inv[0].mod_reg11000_IOT_info =%d,%d,%d,%d,",Inv[0].mod_reg11000_IOT_info.sta_ipv4[0],Inv[0].mod_reg11000_IOT_info.sta_ipv4[1],Inv[0].mod_reg11000_IOT_info.sta_ipv4[2],Inv[0].mod_reg11000_IOT_info.sta_ipv4[3]);
	// ESP_LOGI(TAG, "Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info =%d,%d,%d,%d,",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.sta_ipv4[0],Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.sta_ipv4[1],Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.sta_ipv4[2],Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.sta_ipv4[3]);

	
	// for(int i = 0; i < PLUG_MAX_NUM; i++)
	// {
	// 	if( 0 != Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN)
	// 	{
	// 		ESP_LOGI(TAG, "Plug[%d].mod_reg14500_SmartPlug_info.SmartPlug_SN  =%lld",i,Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
	// 		ESP_LOGI(TAG, "Plug[%d].mod_reg14500_SmartPlug_info.SmartPlug_Software_Version	=%ld",i,Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Software_Version);
	// //		ESP_LOGI(TAG, "Plug[%d].mod_reg14500_SmartPlug_info.SmartPlug_Nums	=%d",i,Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Nums);
	// 		ESP_LOGI(TAG, "Plug[%d].mod_reg14500_SmartPlug_info.SmartPlug_Voltage  =%d",i,Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Voltage);
	// 		ESP_LOGI(TAG, "Plug[%d].mod_reg14500_SmartPlug_info.SmartPlug_Frequency  =%d",i,Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Frequency);
	// 		ESP_LOGI(TAG, "Plug[%d].mod_reg14500_SmartPlug_info.SmartPlug_Power  =%ld",i,Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power);		
	// 	}
	// }
	// ESP_LOGI(TAG, "Plug[PLUG_MAX_NUM].mod_reg14500_SmartPlug_info.SmartPlug_Power  =%ld",Plug[PLUG_MAX_NUM].mod_reg14500_SmartPlug_info.SmartPlug_Power);

	
// 	for (int j = 0; j < NET_WIFI_MAX_POINT; j++)// 
// 	{
// 		if(NET_POINT_OFFLINE != reals.discovery_net_Info[j].net_point_online)
// 		{
// 			ESP_LOGI(TAG, "reals.discovery_net_Info[%d]，net_point_TimeOut_cnt  =%d，net_point_online  =%d，SN  =%lld，",j,reals.discovery_net_Info[j].net_point_TimeOut_cnt,
// 				reals.discovery_net_Info[j].net_point_online,*(uint64_t *)reals.discovery_net_Info[j].SN);
// 			dump_buf_global("reals.discovery_net_Info[j].SN=", reals.discovery_net_Info[j].SN, 10);

// 		}
// 	}
	
// 	ESP_LOGI(TAG, "reals.Topseq_index[0]  =%d;reals.Topseq_index[1]  =%d;reals.Topseq_index[2]  =%d;",reals.Topseq_index[0],reals.Topseq_index[1],reals.Topseq_index[2]);
// 	for (int j = 0; j < NET_WIFI_MAX_POINT; j++)// 
// 	{
// 		if(NET_POINT_OFFLINE != reals.discovery_net_Info[j].net_point_online)
// 		{
// 			ESP_LOGI(TAG, "reals.discovery_net_Info[reals.Topseq_index[%d]]，net_point_TimeOut_cnt  =%d，net_point_online  =%d，SN  =%lld，",reals.Topseq_index[j],reals.discovery_net_Info[reals.Topseq_index[j]].net_point_TimeOut_cnt,
// 				reals.discovery_net_Info[reals.Topseq_index[j]].net_point_online,*(uint64_t *)reals.discovery_net_Info[reals.Topseq_index[j]].SN);
// //			dump_buf_global("reals.discovery_net_Info[j].SN=", reals.discovery_net_Info[j].SN, 10);
			
		
// 		}
// 	}	
	
//		ESP_LOGE(TAG, "reals.udp_modbus.slaveaddress =%d",reals.udp_modbus.slaveaddress);


	// ESP_LOGI(TAG, "reals.debug3  login_step =%d",reals.debug3);
	// ESP_LOGI(TAG, "reals.debug4  login2_step =%d",reals.debug4);
	// ESP_LOGI(TAG, "SetData.dev_info_t.Wr_flag.word =%d",SetData.dev_info_t.Wr_flag.word);
//	ESP_LOGI(TAG, "reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_cnt =%d",reals.can_poll[22].modbus_Rd_cnt);//INDEX_INV_TYPE_ABOUT_13H

	ESP_LOGI(TAG, "SetData.dev_info_t.Parallel_dev_sn =%lld",SetData.dev_info_t.Parallel_dev_sn);
	ESP_LOGI(TAG, "SetData.dev_info_t.Parallel_dev_type =%s",SetData.dev_info_t.Parallel_dev_type);
	ESP_LOGI(TAG, "SetData.dev_info_t.Parallel_dev_safetycode =%lld",SetData.dev_info_t.Parallel_dev_safetycode);
	ESP_LOGI(TAG, "SetData.dev_info_t.INV_dev_sn =%lld",SetData.dev_info_t.INV_dev_sn);
	
		

	
//	ESP_LOGI(TAG, "reals.wifi_eventgroup    =%0X",reals.wifi_eventgroup);
//
//	ESP_LOGI(TAG, "bk_iot_dev_node. .Net_Server_address    =%s",g_self_data.mod_reg22000_net_server_2rd.Net_Server_address);
//	ESP_LOGI(TAG, "bk_iot_dev_node. .Net_Server_secret    =%s",g_self_data.mod_reg22000_net_server_2rd.Net_Server_secret);
//	ESP_LOGI(TAG, "iot_dev_node. .Net_Server_address    =%s",g_self_data.mod_reg22000_net_server_2rd.Net_Server_address);
//	ESP_LOGI(TAG, "iot_dev_node. .Net_Server_secret    =%s",g_self_data.mod_reg22000_net_server_2rd.Net_Server_secret);
//	
//	ESP_LOGI(TAG, "iot_enable    =%d",g_self_data.mod_reg22000_net_server_2rd.iot_enable.word);
////	ESP_LOGI(TAG, "reals.Enable    =%d",reals.Enable.word);
//
//	ESP_LOGI(TAG, "reals.MCUPoweronCnt    =%d",reals.MCUPoweronCnt);
	ESP_LOGI(TAG,"g_self_data.mod_reg11000_IOT_info.link.all:0x%lx",g_self_data.mod_reg11000_IOT_info.link.all);
	ESP_LOGI(TAG, "reals.Addr_can_Attr	=0x%x",reals.Addr_can_Attr);
	ESP_LOGI(TAG, "reals.Addr_can_Small	=%d",reals.Addr_can_Small);
	ESP_LOGI(TAG, "reals.Addr_can_Buff	=%d",reals.Addr_can_Buff);
	ESP_LOGI(TAG, "reals.Addr_can_master    =%d",reals.Addr_can_master);
    ESP_LOGI(TAG, "SetData.dev_info_t.on_off.bit.ble_enable	=%d", SetData.dev_info_t.on_off.bit.ble_enable);
	ESP_LOGI(TAG, "reals.Addr_can_self    =%d",reals.Addr_can_self);
	ESP_LOGI(TAG, "reals.Step_can_dev_parallel    =%d",reals.Step_can_dev_parallel);
	ESP_LOGI(TAG, "SetData.dev_info_t.Addr_can_self    =%d",SetData.dev_info_t.Addr_can_self);
	ESP_LOGI(TAG, "reals.online_X_inv_index    =%d",reals.online_X_inv_index);

	ESP_LOGI(TAG, "can_ota_status[0].ota_summary.isOta	=%d",can_ota_status[0].ota_summary.isOta);
	ESP_LOGI(TAG, "can_ota_status[0].ota_summary.errCode    =%d",can_ota_status[0].ota_summary.errCode);
	uint64_t dcSn=GetUin64FromPtrBig(Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_type);
    ESP_LOGI(TAG,"dcSn:%d",(unsigned int)dcSn);
	// for(uint8_t m=0;m<4;m++)
	// {
	// 	ESP_LOGI(TAG, "Inv[%d].mod_reg01100_Inv_base.InvSN: %llu",m,Inv[m].mod_reg01100_Inv_base.InvSN);
	// 	ESP_LOGI(TAG, "Inv[%d].mod_reg06100_Pack_each.sn_code: %d",m,(unsigned int)Inv[m].mod_reg06100_Pack_each.sn_code);
	// 	ESP_LOGI(TAG, "Inv[%d].mod_reg06100_Pack_each.type_ascii: %s",m,(char *)&(Inv[m].mod_reg06100_Pack_each.type_ascii[0]));
	// 	ESP_LOGI(TAG, "Inv[%d].mod_reg06100_Pack_each.soft[0].version=%d",m,(unsigned int)Inv[m].mod_reg06100_Pack_each.soft[0].version);
	// }
	ESP_LOGI(TAG, "mod_reg11000_IOT_info.software_ver=%d",(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver);
	for(uint8_t j=0;j<6;j++)
	{
        ESP_LOGI(TAG, " mod_reg01100_Inv_base.soft[%d].type=%d",j,(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].type);    
       ESP_LOGI(TAG, " mod_reg01100_Inv_base.soft[%d].version=%d",j,(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version);
	}
	//ESP_LOGW(TAG,"ota_cmd.type:%d,%d, reals.preSoftVersion;:%d",(unsigned int)g_self_data.mod_reg00700_OTA.ota_cmd.type,(unsigned int)Inv[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg00700_OTA.ota_cmd.type,(unsigned int)reals.preSoftVersion);
	//ESP_LOGI(TAG,"mod_reg06300_Pack_cell:%u ",(unsigned int)sizeof(Inv_Pack[0].mod_reg06300_Pack_cell));
	//ESP_LOGI(TAG,"mod_reg07200_Pack_bmu:%u  ",(unsigned int)sizeof(Inv_Pack[0].mod_reg07200_Pack_bmu));
	//uint64_t dcSn=GetDWordFromPtrBig(Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn);
    //ESP_LOGI(TAG,"dcSn:%d",(unsigned int)dcSn);
	//ESP_LOGW(TAG," Cellular_4G_State_Print can_ota_status[0].ota_summary.type:%d",can_ota_status[0].ota_summary.type);

//	ESP_LOGI(TAG, "ble_encrypt_info.flag.all    =%d",ble_encrypt_info.flag.all);

	// ESP_LOGI(TAG, "SetData.dev_info_t.historyRecSaveCount    =%ld",SetData.dev_info_t.historyRecSaveCount);
	// ESP_LOGI(TAG, "SetData.dev_info_t.historyAddrIndex    =%ld",SetData.dev_info_t.historyAddrIndex);

//	ESP_LOGI(TAG, "Inv[reals.Addr_can_self].mod_reg00100_AppPage1.fault[0] =0x%x",Inv[reals.Addr_can_self].mod_reg00100_AppPage1.fault[0]);
//	ESP_LOGI(TAG, "Inv[reals.Addr_can_self].mod_reg00100_AppPage1.alarm[0] =0x%x",Inv[reals.Addr_can_self].mod_reg00100_AppPage1.alarm[0]);
	
//	ESP_LOGI(TAG, "reals.time_init_flag =%d",reals.time_init_flag);

	// ESP_LOGI(TAG, "reals.Topnet_point_Num =%d",reals.Topnet_point_Num);
	// ESP_LOGI(TAG, "reals.Topnet_point_Num_S1 =%d",reals.Topnet_point_Num_S1);
	// ESP_LOGI(TAG, "reals.Topnet_point_Num_mix =%d",reals.Topnet_point_Num_mix);
	// ESP_LOGI(TAG, "reals.Topnet_point_Num_invbat =%d",reals.Topnet_point_Num_invbat);


//	ESP_LOGI(TAG, "windy hello " );

	
	ESP_LOGI(TAG, "reals.online_Pack_num =%d",reals.online_Pack_num);
	ESP_LOGI(TAG, "reals.online_Inv_num =%d",reals.online_Inv_num);
	ESP_LOGI(TAG, "reals.online_Iot_num =%d",reals.online_Iot_num);
	ESP_LOGI(TAG, "reals.online_ACHUB_num =%d",reals.online_ACHUB_num);

	
	ESP_LOGI(TAG, "Inv_can[0].iot_data[0].online =%d",Inv_can[0].iot_data[0].online);
	ESP_LOGI(TAG, "Inv_can[1].iot_data[0].online =%d",Inv_can[1].iot_data[0].online);
	ESP_LOGI(TAG, "Inv_can[2].iot_data[0].online =%d",Inv_can[2].iot_data[0].online);

	ESP_LOGI(TAG, "Inv_can[0].iot_data[0].iot_sn =%llu",Inv_can[0].iot_data[0].mod_reg11000_IOT_info.iot_sn);
	ESP_LOGI(TAG, "Inv_can[1].iot_data[0].iot_sn =%llu",Inv_can[1].iot_data[0].mod_reg11000_IOT_info.iot_sn);
	ESP_LOGI(TAG, "Inv_can[2].iot_data[0].iot_sn =%llu",Inv_can[2].iot_data[0].mod_reg11000_IOT_info.iot_sn);


	// ESP_LOGI(TAG, "Inv_can[0].pack_data[0].pack_announce.online =0x%x",Inv_can[0].pack_data[0].pack_announce.online);
	// ESP_LOGI(TAG, "Inv_can[1].pack_data[0].pack_announce.online =0x%x",Inv_can[1].pack_data[0].pack_announce.online);
	// ESP_LOGI(TAG, "Inv_can[2].pack_data[0].pack_announce.online =0x%x",Inv_can[2].pack_data[0].pack_announce.online);

	


//	ESP_LOGI(TAG, "reals.Can_beta_block_send_timeout_Cnt =%d",reals.Can_beta_block_send_timeout_Cnt);
	// ESP_LOGI(TAG, "reals.BLE_WIFI_to_Can_Cnt =%d",reals.BLE_WIFI_to_Can_Cnt);
	// ESP_LOGI(TAG, "reals.T_delay_iot_para_can_wr =%d",reals.T_delay_iot_para_can_wr);
	

	
//
//	ESP_LOGI(TAG, "reals.Can_beta_block_fast_poll_flag    =%d",reals.Can_beta_block_fast_poll_flag);
//	ESP_LOGI(TAG, "reals.Can_beta_block_inv_slow_poll_flag    =%d",reals.Can_beta_block_inv_slow_poll_flag);
//	ESP_LOGI(TAG, "reals.Can_beta_block_inv_about_poll_flag    =%d",reals.Can_beta_block_inv_about_poll_flag);
//	ESP_LOGI(TAG, "reals.Can_beta_block_iot_poll_flag    =%d",reals.Can_beta_block_iot_poll_flag);
//	ESP_LOGI(TAG, "reals.Can_beta_block_type    =0x%x",reals.Can_beta_block_type);
//	ESP_LOGI(TAG, "reals.Can_beta_block_send_timeout_Cnt    =%d",reals.Can_beta_block_send_timeout_Cnt);
//
//	ESP_LOGI(TAG, "reals.canrx_push_cnt    =%ld",reals.canrx_push_cnt);
//	ESP_LOGI(TAG, "reals.canrx_pop_cnt    =%ld",reals.canrx_pop_cnt);
//
//	ESP_LOGI(TAG, "Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.online    =0x%x",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.online);
//	ESP_LOGI(TAG, "Inv_can[0].inv_data[INV_MAX_NUM].inv_announce.online    =0x%x",Inv_can[0].inv_data[INV_MAX_NUM].inv_announce.online);
//	ESP_LOGI(TAG, "Inv_can[1].inv_data[INV_MAX_NUM].inv_announce.online    =0x%x",Inv_can[1].inv_data[INV_MAX_NUM].inv_announce.online);
//
//	ESP_LOGI(TAG, "Inv_can[0].pack_data[0].pack_announce.online    =0x%x",Inv_can[0].pack_data[0].pack_announce.online);
//	ESP_LOGI(TAG, "Inv_can[1].pack_data[0].pack_announce.online    =0x%x",Inv_can[1].pack_data[0].pack_announce.online);
//	ESP_LOGI(TAG, "Inv_can[2].pack_data[0].pack_announce.online    =0x%x",Inv_can[2].pack_data[0].pack_announce.online);

	ESP_LOGI(TAG, "AA Device_Can_Address_Parallel_Bind  Bind_SN=%d",  g_self_data.mod_reg11000_IOT_info.Bind_SN);//testwx
	ESP_LOGI(TAG, "g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.all=%d",  g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.all);//testwx
//	ESP_LOGI(TAG, "g_self_data_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.all=%d",  g_self_data_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.all);//testwx
//
//	ESP_LOGI(TAG, "trigger_regroup_IOT=%d",  g_self_data_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_IOT);// 
//	ESP_LOGI(TAG, "trigger_regroup_INV=%d",  g_self_data_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_INV);// 
//	ESP_LOGI(TAG, "trigger_regroup_PACK=%d",  g_self_data_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_PACK);// 
//	ESP_LOGI(TAG, "trigger_BIND=%d",  g_self_data_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND);// 
//	ESP_LOGI(TAG, "revd=%d",  g_self_data_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.revd);// 
//
//
//
//	
//	ESP_LOGI(TAG, "Inv[0].mod_reg12000_IOT_set.IOT_Enable_mix1.all=%d",  Inv[0].mod_reg12000_IOT_set.IOT_Enable_mix1.all);//testwx
//	ESP_LOGI(TAG, "Inv[1].mod_reg12000_IOT_set.IOT_Enable_mix1.all=%d",  Inv[1].mod_reg12000_IOT_set.IOT_Enable_mix1.all);//testwx

	

//	for(int i = 0; i < DEV_MAIN_NODE_MAX; i++)
//	{
//		for (uint8_t j = 0; j < (PACK_MAX_NUM); j++) 
//		{
//		
//		ESP_LOGI(TAG, "Inv_can[i=%d].pack_data[j=%d].alive_time=%d, online =%d",i,j ,Inv_can[i].pack_data[j].alive_time,Inv_can[i].pack_data[j].online);
//		}
//	}



//	ESP_LOGI(TAG, "g_self_data.mod_reg11000_IOT_info.ble_mac	  =%0x,%0x,%0x:%0x,%0x,%0x,",
//		g_self_data.mod_reg11000_IOT_info.ble_mac[0],
//		g_self_data.mod_reg11000_IOT_info.ble_mac[1],
//		g_self_data.mod_reg11000_IOT_info.ble_mac[2],
//		g_self_data.mod_reg11000_IOT_info.ble_mac[3],
//		g_self_data.mod_reg11000_IOT_info.ble_mac[4],
//		g_self_data.mod_reg11000_IOT_info.ble_mac[5]		);
//
////	Print_debug1();
//	printf("esp_get_free_heap_size: %ld\n", esp_get_free_heap_size());
//

//
//	ESP_LOGI(TAG, "Inv_can[0].pack_data[0].pack_announce.online    =%d",Inv_can[0].pack_data[0].pack_announce.online);
//	ESP_LOGI(TAG, "Inv_can[1].pack_data[0].pack_announce.online    =%d",Inv_can[1].pack_data[0].pack_announce.online);
//	ESP_LOGI(TAG, "Inv_can[2].pack_data[0].pack_announce.online    =%d",Inv_can[2].pack_data[0].pack_announce.online);
//	

	
	
	


//	for(int i = 0; i < DEV_MAIN_NODE_MAX; i++)
//	{
//		ESP_LOGI(TAG, "Inv_can[i=%d]. inv_num,inv_online=%d, inv_power=%d,_rang=%d",i,Inv_can[i].inv_data[0].inv_base.inv_num,
//			Inv_can[i].inv_data[0].inv_base.inv_online,
//			Inv_can[i].inv_data[0].inv_base.inv_power_rang);
//
//		ESP_LOGI(TAG, "Inv[i=%d]. inv_num,inv_online=%d, inv_power=%d,_rang=%d",i, Inv[i].mod_reg00100_AppPage1.inv_num,
//			Inv[i].mod_reg00100_AppPage1.inv_online,
//			Inv[i].mod_reg00100_AppPage1.inv_power_rang);
//
//		  for (uint8_t j = 0; j < (INV_MAX_NUM); j++) 
//		{
//		
//			ESP_LOGI(TAG, "Inv_can[i=%d].inv_data[j=%d].alive_time=%d, online =%d",i,j ,Inv_can[i].inv_data[j].alive_time,Inv_can[i].inv_data[j].online);
//		}
//		  
//		  for (uint8_t j = 0; j < (PACK_MAX_NUM); j++) 
//		{
//
//			ESP_LOGI(TAG, "Inv_can[i=%d].pack_data[j=%d].alive_time=%d, online =%d",i,j ,Inv_can[i].pack_data[j].alive_time,Inv_can[i].pack_data[j].online);
//		}		  
//	}

//	dump_buf_global("SetData_Can.dev_info_t2.inv_set00", &SetData_Can.dev_info_t2.inv_set00, sizeof(SetData_Can.dev_info_t2.inv_set00));
//	dump_buf_global("Inv_can[0].inv_data[0].inv_set00", &Inv_can[0].inv_data[0].inv_set00, sizeof(Inv_can[0].inv_data[0].inv_set00));

//
//	dump_buf_global("Inv_can[0].inv_data[0].inv_base", &Inv_can[0].inv_data[0].inv_base, sizeof(Inv_can[0].inv_data[0].inv_base));
//	dump_buf_global("Inv_can[DEV_MAIN_NODE_MAX].inv_data[0].inv_base", &Inv_can[DEV_MAIN_NODE_MAX].inv_data[0].inv_base, sizeof(Inv_can[0].inv_data[0].inv_base));

	
	// ESP_LOGI(TAG, "SetData_Can.dev_info_t2.valid_inv == 0x%x, SetData_Can.dev_info_t2.valid_pack == 0x%x",SetData_Can.dev_info_t2.valid_inv, SetData_Can.dev_info_t2.valid_pack);
	// ESP_LOGI(TAG, "Inv_can[0].inv_data[0].setdata_valid == 0x%x ",Inv_can[0].inv_data[0].setdata_valid);
	// ESP_LOGI(TAG, "Inv_can[1].inv_data[0].setdata_valid == 0x%x ",Inv_can[1].inv_data[0].setdata_valid);
	// ESP_LOGI(TAG, "reals.flasWrFlag.Byte2== 0x%x ",reals.flasWrFlag.Byte2);


	ESP_LOGI(TAG, "reals.online_DCHUB_num == %d ",reals.online_DCHUB_num);
	ESP_LOGI(TAG, "reals.online_DCHUB_bit == %d ",reals.online_DCHUB_bit);
	ESP_LOGI(TAG, "reals.online_Inv_bit == %d ",reals.online_Inv_bit);

//	dump_buf_global("Inv_can[0].inv_data[0].inv_set00", &Inv_can[0].inv_data[0].inv_set00, sizeof(Inv_can[0].inv_data[0].inv_set00));
//	dump_buf_global("Inv_can[0].inv_data[0].inv_set01", &Inv_can[0].inv_data[0].inv_set01, sizeof(Inv_can[0].inv_data[0].inv_set01));
//
//
//	dump_buf_global("SetData_Can.dev_info_t2.inv_set00", &SetData_Can.dev_info_t2.inv_set00, sizeof(SetData_Can.dev_info_t2.inv_set00));
//
//	dump_buf_global("SetData_Can.dev_info_t2.inv_set01", &SetData_Can.dev_info_t2.inv_set01, sizeof(SetData_Can.dev_info_t2.inv_set01));
//	dump_buf_global("SetData_Can.dev_info_t2.inv_set02", &SetData_Can.dev_info_t2.inv_set02, sizeof(SetData_Can.dev_info_t2.inv_set02));
//	dump_buf_global("SetData_Can.dev_info_t2.inv_set03", &SetData_Can.dev_info_t2.inv_set03, sizeof(SetData_Can.dev_info_t2.inv_set03));

ESP_LOGI(TAG, "IOT SN ==%llu",iot_factory.iot_sn);
ESP_LOGI(TAG, "IOT safety Code ==%llu",iot_factory.safe_code);
// for (uint8_t node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
// {
// 	ESP_LOGI(TAG, "can_ota_status[%d].timecnt == %d ",node_id,can_ota_status[node_id].timecnt);
// 	ESP_LOGI(TAG, "can_ota_status[%d].period_cmd_delay == %d ",node_id,can_ota_status[node_id].period_cmd_delay);
// 	ESP_LOGI(TAG, "can_ota_status[%d].ota_doing == %d ",node_id,can_ota_status[node_id].ota_doing);	
// }
ESP_LOGI(TAG, "reals.ota_happen ==%u",reals.ota_happen);
for(uint i = 0;i<= DEV_MAIN_NODE_MAX;i++)
{
	ESP_LOGI(TAG, "Inv_can[%d].inv_data[0].inv_about.dev_sn ==%llu",i,Inv_can[i].inv_data[0].inv_about.dev_sn);
}
//ESP_LOGI(TAG,"on_off.bit.ble_enable:0x%x  mod_reg11000_IOT_info.link.bit.ble_connec:%d",SetData.dev_info_t.on_off.bit.ble_enable,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.link.bit.ble_connect);
// ESP_LOGI(TAG, "reals.If_ARM_SingleBoot ==%u",reals.If_ARM_SingleBoot);
// ESP_LOGI(TAG, "reals.If_AC_HUB_SingleBoot ==%u",reals.If_AC_HUB_SingleBoot);
// ESP_LOGI(TAG, "reals.net_point_Comein ==%u",reals.net_point_Comein);
// ESP_LOGI(TAG, "reals.modbus_self_report_ble ==%u",reals.modbus_self_report_ble);

// ESP_LOGI(TAG, "Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX):");
// esp_log_buffer_hex(TAG,(const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15800_Ac_Hub_info,sizeof(MOD_STRUCT_reg15800));
// ESP_LOGI(TAG, "Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX).mod_reg15800_Ac_Hub_info.alarm_State:");
// esp_log_buffer_hex(TAG,(const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15800_Ac_Hub_info.alarm_State,2);

// (uint8_t *)&Inv_can[i].inv_data[0].inv_grid
// for(uint i = 0;i<= DEV_MAIN_NODE_MAX;i++)
// {
// 	ESP_LOGI(TAG, "Inv_can[%d].inv_data[0].inv_grid:",i);
// 	esp_log_buffer_hex(TAG,(uint8_t *)&Inv_can[i].inv_data[0].inv_grid,sizeof(inv_grid_struct));
// }

	//ESP_LOGI(TAG,"match_stander.area:0x%x",Inv[reals.Addr_can_self].mod_reg00000.match_stander.area);

	ESP_LOGW(TAG, "SetData.dev_info_t.SetTimeZone:%u ,inv_set00.res:%u",SetData.dev_info_t.SetTimeZone.all,SetData_Can.dev_info_t2.inv_set00.res);//testwx
	esp_log_buffer_hex(TAG, SetData.dev_info_t.Sn_bind, 5);//testwx
	#if 1
	time_t now_date = time(NULL);
	ESP_LOGI(TAG,"now_date1:%llu",now_date);
	/* 日期转换为时间结构 */
	struct tm tm_now;
	struct tm tm_now1;
//	static uint16_t scnt=0;
	memcpy(&tm_now, localtime(&now_date), sizeof(struct tm));

	ESP_LOGI(TAG, "ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now.tm_mon+1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
	memcpy(&tm_now1, gmtime(&now_date), sizeof(struct tm));
	ESP_LOGI(TAG, "NowdateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now1.tm_mon+1,tm_now1.tm_mday,tm_now1.tm_hour,tm_now1.tm_min,tm_now1.tm_sec);
	
	#endif
	/*
	now_date=0;
	now_date=mktime(&tm_now1);
	ESP_LOGI(TAG,"now_date2:%llu",now_date);
	memcpy(&tm_now, localtime(&now_date), sizeof(struct tm));
	ESP_LOGI(TAG, "ChinadateTime2:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now.tm_mon+1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
	*/
printf("\n\n");


}

/*------------------------------------------------------------------------------

  */
void debug_force_alarm_create(void)
{  
	uint8_t FaultNumber = 0;
	static uint8_t scnt = 0;

	if(++scnt >= 10)
	{
		scnt=0;
		for(FaultNumber=0;FaultNumber<4;FaultNumber++)
		{
			Inv[reals.Addr_can_self].mod_reg00100_AppPage1.alarm[FaultNumber]= 0xFFFF;


		}
		for(FaultNumber=0;FaultNumber<5;FaultNumber++)
		{
			Inv[reals.Addr_can_self].mod_reg00100_AppPage1.fault[FaultNumber] = 0xFFFF;

		}

	}

	if(5== scnt)
	{
		for(FaultNumber=0;FaultNumber<4;FaultNumber++)
		{
			Inv[reals.Addr_can_self].mod_reg00100_AppPage1.alarm[FaultNumber]= 0;


		}
		for(FaultNumber=0;FaultNumber<5;FaultNumber++)
		{
			Inv[reals.Addr_can_self].mod_reg00100_AppPage1.fault[FaultNumber] = 0;

		}

	}

}

/* HTTP 周期请求抖动：真随机均匀落在给定区间，避免批量设备同刻打云端 */
#define HTTP_JITTER_INTERVAL_MIN_MS     (10UL * 1000UL)

/**
 * @brief 在 [min_ms, max_ms] 上均匀取下次间隔（硬件真随机）
 */
static uint32_t http_jitter_next_interval_ms(uint32_t min_ms, uint32_t max_ms)
{
    uint32_t span;

    if (max_ms < min_ms) {
        max_ms = min_ms;
    }
    span = max_ms - min_ms + 1UL;
    return min_ms + (esp_random() % span);
}

/*------------------------------------------------------------------------------
 Function: http_version_update_timeout
 -----------------------------------------------------------------------------*/
/**
  * @brief      版本检查计时（随机间隔 10s~24h，每次到期后重新抽样）
  * @param[in]  void
  * @param[out] None
  * @return     static void
  */
static void http_version_update_timeout(void)
{
    static uint32_t pre_time = 0;
    static uint32_t next_interval_ms = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (pre_time == 0 || next_interval_ms == 0) {
        pre_time = now_time;
        next_interval_ms = http_jitter_next_interval_ms(HTTP_JITTER_INTERVAL_MIN_MS, H24_HOUR_INTERVAL);
        ESP_LOGI(TAG, "version check schedule first interval=%lu ms (~%lu min)",
                 (unsigned long)next_interval_ms,
                 (unsigned long)(next_interval_ms / 60000UL));
        return;
    }

    if ((now_time - pre_time) >= next_interval_ms) {
        reals.need_new_version_flag.sBit.iot = 1;
        if (0 != reals.online_Inv_num) {
            reals.need_new_version_flag.sBit.inv = 1;
        }
        if (0 != reals.online_Pack_num) {
            reals.need_new_version_flag.sBit.pack = 1;
        }
        if (0 != reals.online_DCHUB_num) {
            reals.need_new_version_flag.sBit.dchub = 1;
        }
        if (0 != reals.online_ACHUB_num) {
            reals.need_new_version_flag.sBit.achub = 1;
        }
        if (0 != reals.online_D400S_num) {
            reals.need_new_version_flag.sBit.d400s = 1;
        }

        pre_time = now_time;
        next_interval_ms = http_jitter_next_interval_ms(HTTP_JITTER_INTERVAL_MIN_MS, H24_HOUR_INTERVAL);
        ESP_LOGI(TAG, "version check triggered, next interval=%lu ms (~%lu min)",
                 (unsigned long)next_interval_ms,
                 (unsigned long)(next_interval_ms / 60000UL));
    }
}
/*------------------------------------------------------------------------------
 Function: http_dst_time_update_timeout
 -----------------------------------------------------------------------------*/
/**
  * @brief      时间更新计时
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void http_dst_time_update_timeout(void)
{
	static uint32_t pre_time = 0;

#ifdef TIME_GET_FROM_SERVER_BY_HTTP_ENABLE

	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	ESP_LOGI(TAG,"now_time:%lu,pre_time:%lu,sys_is_updating():%d",now_time,pre_time,sys_is_updating());
	if(((now_time - pre_time) >= H24_HOUR_INTERVAL) && (!sys_is_updating()))
	{
        /*仅在连接服务器（即网络可用）运行*/
        if ( g_self_data.mod_reg11000_IOT_info.link.bit.mqtt )
        {
    	    if ( pre_time != 0 )
    	    {
                ESP_LOGW(TAG, "http_dst_time_update!"); 
                http_dst_time_update();
    	    }
        }
        pre_time = now_time;
    }

#endif
}

static void http_period_update(void)
{
    static uint32_t pre_save_money_update_time = 0;
    static uint32_t next_save_money_interval_ms = 0;
    static bool first_save_money = true;

    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /*仅在连接服务器（即网络可用）运行*/
    if (!g_self_data.mod_reg11000_IOT_info.link.bit.mqtt || sys_is_updating()) {
        return;
    }

    /* 更新节省金额和温度单位：首次连上立即拉一次，之后 10s~1h 真随机间隔 */
    if (first_save_money ||
        ((pre_save_money_update_time != 0) &&
         ((now_time - pre_save_money_update_time) >= next_save_money_interval_ms))) {
        ESP_LOGI(TAG, "http_update_money_save_data! first=%d", first_save_money);
        http_update_money_save_data();
        pre_save_money_update_time = now_time;
        first_save_money = false;
        next_save_money_interval_ms = http_jitter_next_interval_ms(HTTP_JITTER_INTERVAL_MIN_MS, ONE_HOUR_INTERVAL);
        ESP_LOGI(TAG, "save money/temp next interval=%lu ms (~%lu min)",
                 (unsigned long)next_save_money_interval_ms,
                 (unsigned long)(next_save_money_interval_ms / 60000UL));
    }
}

/*------------------------------------------------------------------------------
 Function: system_off_or_sleep_check
 -----------------------------------------------------------------------------*/
/**
* @brief      检查系统状态
* @param[in]  void  
* @param[out] None
* @return     static void
*/
static uint8_t s_sleep_old_flag = 0;

static void iot_sleep_ctx_persist(uint8_t in_sleep, const MOD_STRUCT_reg00100 *page1)
{
	if (!in_sleep) {
		if ((SetData.dev_info_t.sleep_ctx.in_sleep == 0)
			&& (SetData.dev_info_t.sleep_ctx.sleep_soc == 0)
			&& (SetData.dev_info_t.sleep_ctx.dev_sn == 0)
			&& (SetData.dev_info_t.sleep_ctx.dev_type[0] == 0)) {
			return;
		}
		memset(&SetData.dev_info_t.sleep_ctx, 0, sizeof(SetData.dev_info_t.sleep_ctx));
		reals.SetDataWrFlag.sBit.sleep_ctx = 1;
		return;
	}

	if (page1 == NULL) {
		return;
	}

	if ((SetData.dev_info_t.sleep_ctx.in_sleep == 1)
		&& (SetData.dev_info_t.sleep_ctx.sleep_soc == (uint8_t)page1->soc)
		&& (SetData.dev_info_t.sleep_ctx.dev_sn == page1->dev_sn)
		&& (memcmp(SetData.dev_info_t.sleep_ctx.dev_type, page1->dev_type,
			sizeof(page1->dev_type)) == 0)) {
		return;
	}

	SetData.dev_info_t.sleep_ctx.in_sleep = 1;
	SetData.dev_info_t.sleep_ctx.sleep_soc = (uint8_t)page1->soc;
	SetData.dev_info_t.sleep_ctx.dev_sn = page1->dev_sn;
	memcpy(SetData.dev_info_t.sleep_ctx.dev_type, page1->dev_type, sizeof(page1->dev_type));
	reals.SetDataWrFlag.sBit.sleep_ctx = 1;
}

void iot_sleep_ctx_restore_mod_reg00100(MOD_STRUCT_reg00100 *page1)
{
	if (page1 == NULL) {
		return;
	}
	if (!reals.IOT_Status_Flag.sBit.system_sleep_flag
		&& (SetData.dev_info_t.sleep_ctx.in_sleep != 1)) {
		return;
	}

	if ((page1->soc == 0) && (SetData.dev_info_t.sleep_ctx.sleep_soc != 0)) {
		page1->soc = SetData.dev_info_t.sleep_ctx.sleep_soc;
	}
	if ((page1->dev_type[0] == '\0') && (SetData.dev_info_t.sleep_ctx.dev_type[0] != '\0')) {
		memcpy(page1->dev_type, SetData.dev_info_t.sleep_ctx.dev_type, sizeof(page1->dev_type));
	}
	if ((page1->dev_sn == 0) && (SetData.dev_info_t.sleep_ctx.dev_sn != 0)) {
		page1->dev_sn = SetData.dev_info_t.sleep_ctx.dev_sn;
	}
}

void iot_sleep_state_restore_from_setdata(void)
{
	uint8_t addr;
	uint8_t sleep_soc;

	if (SetData.dev_info_t.sleep_ctx.in_sleep != 1) {
		return;
	}

	sleep_soc = SetData.dev_info_t.sleep_ctx.sleep_soc;
	reals.IOT_Status_Flag.sBit.system_sleep_flag = 1;
	s_sleep_old_flag = 1;

	Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 1;
	Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Parts_online.bit.remote_off_en = 1;
	Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.soc = sleep_soc;
	memcpy(Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.dev_type,
		SetData.dev_info_t.sleep_ctx.dev_type, sizeof(Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.dev_type));
	Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.dev_sn = SetData.dev_info_t.sleep_ctx.dev_sn;

	addr = SetData.dev_info_t.Addr_can_self;
	if (addr < DEV_MAIN_NODE_MAX) {
		Inv[addr].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 1;
		Inv[addr].mod_reg00100_AppPage1.Parts_online.bit.remote_off_en = 1;
		Inv[addr].mod_reg00100_AppPage1.soc = sleep_soc;
		memcpy(Inv[addr].mod_reg00100_AppPage1.dev_type,
			SetData.dev_info_t.sleep_ctx.dev_type, sizeof(Inv[addr].mod_reg00100_AppPage1.dev_type));
		Inv[addr].mod_reg00100_AppPage1.dev_sn = SetData.dev_info_t.sleep_ctx.dev_sn;
	}

	ESP_LOGW(TAG, "sleep_ctx restore: in_sleep=1 soc=%u sn=%llu", sleep_soc,
		(unsigned long long)SetData.dev_info_t.sleep_ctx.dev_sn);
}

static void system_off_or_sleep_check(void)
{
	uint8_t ws;
	uint8_t new_sleep_flag;
	uint8_t addr;

	addr = reals.Addr_can_self;
	if (addr >= DEV_MAIN_NODE_MAX) {
		addr = 0;
	}
	ws = Inv[addr].mod_reg06000_Pack_sum.work_status;

	if (ws == 0x55) {
		new_sleep_flag = 1;
	} else if (ws == 0x22) {
		new_sleep_flag = 0;
	} else if (reals.IOT_Status_Flag.sBit.system_sleep_flag
		|| (SetData.dev_info_t.sleep_ctx.in_sleep == 1)) {
		new_sleep_flag = 1;
	} else {
		new_sleep_flag = 0;
	}

	if (new_sleep_flag) {
		Inv[addr].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 1;
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 1;
		iot_sleep_ctx_restore_mod_reg00100(&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1);
		iot_sleep_ctx_restore_mod_reg00100(&Inv[addr].mod_reg00100_AppPage1);
	} else {
		Inv[addr].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 0;
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 0;
	}

	if (s_sleep_old_flag != new_sleep_flag) {
		if (!new_sleep_flag) {
			ESP_LOGE(TAG, "system_off_or_sleep_check : SLEEP END! (Inv: %d, Pack: 0x%x)",
				Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state, ws);
			reals.IOT_Status_Flag.sBit.system_sleep_flag = 0;
			iot_sleep_ctx_persist(0, NULL);
		} else {
			ESP_LOGE(TAG, "system_off_or_sleep_check : SLEEP START! (Inv: %d, Pack: 0x%x)",
				Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state, ws);
			reals.IOT_Status_Flag.sBit.system_sleep_flag = 1;
			iot_sleep_ctx_persist(1, &Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1);
		}
		s_sleep_old_flag = new_sleep_flag;
	} else if (new_sleep_flag) {
		reals.IOT_Status_Flag.sBit.system_sleep_flag = 1;
	}
}

void iot_period_task (void * pvParameters)
{
	static uint32_t debugcnt=0;
	static uint8_t scnt_50ms=0;
	static uint8_t scnt_100ms=0;
	static uint8_t scnt_1s=0;
//    (void)pvParameters;


	/* 目前仅有这两个开关起作用 */
//	SetData.dev_info_t.on_off.bit.ble_enable = 1;				// 默认开启蓝牙
//	SetData.dev_info_t.on_off.bit.ble_adv_enable = 1;
//	SetData.dev_info_t.on_off.bit.wifi_enable = 1;				// 默认开启WiFi
//	SetData.dev_info_t.on_off.bit.wifi_sta_enable = 1;

    reals.wifi_ap_mode = WIFI_AP_MODE_CLOSE;
    if (SetData.dev_info_t.on_off.bit.wifi_ap_enable == 1)
    {
        reals.wifi_ap_mode = WIFI_AP_MODE_NORMAL;
    }
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
    else if (SetData.dev_info_t.on_off.bit.WIFI_mesh == 1)
    {
        reals.wifi_ap_mode = WIFI_AP_MODE_MESH;
    }

    iot_wifi_new_ap(SetData.dev_info_t.wifi_sta_auth, SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password, NULL);
#else
	// WiFi配置
	iot_wifi_new_ap(SetData.dev_info_t.wifi_sta_auth, SetData.dev_info_t.wifi_sta_ssid, SetData.dev_info_t.wifi_sta_password);
#endif
    ESP_LOGW(TAG, "WIFI AP mode:%d, wifi ap enable:%d", reals.wifi_ap_mode, SetData.dev_info_t.on_off.bit.wifi_ap_enable);
    iot_wifi_new_host(SetData.dev_info_t.could_dns);

    if (WIFI_AP_MODE_CLOSE != reals.wifi_ap_mode)
    {
        iot_wifi_ap_config(SetData.dev_info_t.wifi_ap_auth, SetData.dev_info_t.wifi_ap_ssid, SetData.dev_info_t.wifi_ap_password);
    }

	if(reals.online_ACHUB_num>0)
	{
		if((strcmp(login_info.iot_type, IOT_TYPE_HA1) != 0) 
        && (reals.Addr_can_master == 1)
        &&(SetData.dev_info_t.Parallel_dev_sn != 0)
        &&(strlen(SetData.dev_info_t.Parallel_dev_type)!=0)
        &&(SetData.dev_info_t.Parallel_dev_safetycode!=0))
        {
			ESP_LOGI(TAG, "ACHUB online, use parallel mode");
			ESP_LOGI(TAG, "Init type:%s -%llu -%llu",SetData.dev_info_t.Parallel_dev_type,  SetData.dev_info_t.Parallel_dev_sn, SetData.dev_info_t.Parallel_dev_safetycode);
            iot_wifi_new_iot(SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, SetData.dev_info_t.Parallel_dev_safetycode);		
            iot_wifi_new_dev(SetData.dev_info_t.Parallel_dev_type,  SetData.dev_info_t.Parallel_dev_sn);
            iot_bind_clean();
        }
	}
    else if(reals.online_ACHUB_num==0)
	{
        bool inv_id_valid = (reals.online_Inv_num > 0)
                        && (SetData.dev_info_t.INV_dev_sn != 0)
                        && (0 != memcmp(SetData.dev_info_t.INV_dev_type, reals.zero, sizeof(SetData.dev_info_t.INV_dev_type)));
        bool inv_id_changed = (0 != memcmp(login_info.dev_type, SetData.dev_info_t.INV_dev_type, sizeof(login_info.dev_type)))
                           || (login_info.dev_sn != SetData.dev_info_t.INV_dev_sn);

		if(
		(inv_id_valid && inv_id_changed)
		||((strcmp(login_info.iot_type, IOT_TYPE_IOT) != 0) && (reals.Addr_can_master == 1))//变化
		)
		{
			ESP_LOGI(TAG, "ACHUB offline, use single mode");
			ESP_LOGI(TAG, "Init iot type:%s -%llu -%llu",iot_factory.iot_type,  iot_factory.iot_sn, iot_factory.safe_code);
			ESP_LOGI(TAG, "Init dev type:%s -%llu ",SetData.dev_info_t.INV_dev_type,  SetData.dev_info_t.INV_dev_sn);
			iot_wifi_new_iot(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);	
			iot_wifi_new_dev(SetData.dev_info_t.INV_dev_type,  SetData.dev_info_t.INV_dev_sn);
			iot_bind_clean();
		}
	}
	else
	{
		ESP_LOGE(TAG, "ACHUB online num error, reals.online_ACHUB_num=%d", reals.online_ACHUB_num);
	}

    

#ifdef FORCE_BLE_WIFI_ON_ENABLE
	SetData.dev_info_t.on_off.bit.wifi_enable=1;

	// WiFi开启
	iot_wifi_open(WIFI_MODE_STA);
#else
    if (1 == SetData.dev_info_t.on_off.bit.wifi_ap_enable)
    {
        iot_wifi_open(WIFI_MODE_ALL);// WIFI_MODE_STA
    }
    else
    {
        iot_wifi_open(WIFI_MODE_STA);
    }
#endif


//    iot_factory_to_can();  /* IOT出厂标定信息更新到can */
////    iot_4g_update();
    iot_ble_update();
    iot_wifi_update();
    iot_about_update();

	

	/* 初始化时清除风暴模式的状态 */
	g_self_data.mod_reg12000_IOT_set.thunder_ctrl.all = 0;
	g_self_data.mod_reg12000_IOT_set.thunder_ctrl.thunder_enable = SetData.dev_info_t.thunder_ctrl.thunder_enable;
	uint8_t enable = SetData.dev_info_t.thunder_ctrl.thunder_enable;
	SetStormWatchEnableRegisterData(enable);

	g_self_data.mod_reg12000_IOT_set.on_off = SetData.dev_info_t.on_off;
//	g_device_data.bk_iot_dev_node.iot_config = g_device_data.iot_dev_node.iot_config_cantyp_0x01;

#ifdef  TCP_SERVER2_ENABLE	
    
    /*调试用服务器状态更新*/
    server2_config_info_update();

#endif

//windy 要加delay,否则系统执行出错，用时间标志分断执行
    while (1)
	{
	
		vTaskDelay(pdMS_TO_TICKS(10));//windy必须延时

		ble_authenticate_task_step();
		App_SysTime();
		//ESP_LOGI(TAG,"iot_period_task2006 ctrl:0x%x",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.all);
		if(1==reals.st_FlagTime.bits.b1FlagSys50ms)
		{
//		    /*Sub1GHz在线设备信息处理*/
//			Sub_Net_Point_Serial_Order();	
//
//            /*modbus21000绑定帧处理*/
//            Sub_Net_Point_Bind();

/*
            if(reals.net_point_Comein == 1)
            {
                //当前数据刷新时仅开启蓝牙立即上报，mqtt仅支持固定周期上报及在收到modbus读指令时会主动上报
//                Modbus_21000_Net_Point_Frame();
                reals.net_point_Comein = 0;
                reals.modbus_self_report_ble = 1;
//                    reals.modbus_self_report_mqtt = 1;
            }
*/

		}
        
		if(1==reals.st_FlagTime.bits.b1FlagSys100ms)
		{
		    reals.cnt_100ms++;
			time_have_new_set();//加速，防止g_self_data_WR变量被覆盖
            
            // add by debug:
            ems_stop_reverse();

//
//            /*WIFI网络设备信息处理*/
			Top_Net_Point_Serial_Order(0);
			if(++scnt_100ms >= 2)// 
			{
				scnt_100ms=0;
			}
			
          
		
			ble_authenticate_task();		
//						iot_4g_update();
			iot_cfg_check();
			iot_wifi_update();
			iot_about_update();
			// iot_mqtt_update();
			iot_ctrl_2006_check();
			iot_factory_SN_WR();
			iot_link_status_check();				// IOT连接状态检查
			iot_link_status_update();

			/*更新智能控制状态*/
			iot_smart_control_status_check();

					// dev_history_data_log();				// 历史数据存储
			//		system_time_sync(); 					// 系统时间同步
			//		hmi_soft_info_check();					// HMI软件信息检查
			thunder_state_update(); 				// 风暴模式状态更新
			//		di_status_check();						// DI状态检查
			//		mcu_reset_cmd_check();					// MCU按键复位检测
		}

        if (1 == reals.st_FlagTime.bits.b1FlagSys500ms)
        {
            ble_adv_update();
        }

		if(1 == reals.st_FlagTime.bits.b1FlagSys1s)
		{
//			debug_force_alarm_create();
			iot_ble_update();
			ble_adv_status_check();
		
			BLE_ON_OFF_check();
			WIFI_ON_OFF_check();
			DisasterJudge();	// 初始化风暴模式定时器
			time_sync_to_local();
			Inv_Pack_First_online_Index_Check();
			//Inv_Detailed_Info_update();
			//detailed_test();
			iot_box_task();
			if(++scnt_1s >= 120)//120s
			{
				scnt_1s=0;
			}

//			Udp_Singlecast_Heartbeat_TxFrame();
//			
//			if(0 == udt_mqtt_Report_to_Cloud())//http_client数据通道响应
//			{
//				iot_mqtt_announce();//然后周期上报
//			}
		}
		
            /*UDP组播发送*/
//			Udp_Multicast_Tx();
		iot_kwh_local_storage_task();
		//逆变详细log信息刷新

		
		if(1==reals.st_FlagTime.bits.b1FlagSys1s)
		{
			// iot_kwh_local_storage_task();//test kwh
//			Udp_multicast_task();//20240517 add
//			Udp_Multicast_Tx();
			uart_rx_timeout_clean();
//			Sub_Net_Point_TimeOut_Check();
			//if(!reals.ota_happen)
			{
				Top_Net_Point_TimeOut_Check();
			}
			//ESP_LOGI(TAG,"huangji-test can-ota-iot");
//			ESP_LOGW(TAG, "windy debug 1S cycle,  uart1.rxBytesNum=%d,FlagRx_ok=%d",reals.struct_uart1.rxBytesNum,reals.struct_uart1.FlagRx_ok);
			if(++debugcnt >= 5)
			{
				debugcnt=0;
//				Sub_Net_Point_Bind();
//                Modbus_21000_Net_Point_Frame();

				// Cellular_4G_State_Print();		
					/*调试*/
				print_heap_info();
			//	print_task_status();
				// printRemainingFileDescriptors();


			}
            /*UDP mesh网络心跳帧发送(单播)*/
//    		Udp_Singlecast_Heartbeat_TxFrame();	

            
		}
		CAN_Dev_PowerOff_Check();

		/*系统休眠状态检查*/
		system_off_or_sleep_check();
		/*系统DO唤醒状态检查*/
		system_do_wakeup_check();
        //debug 运行计时
//        if(1==reals.st_FlagTime.bits.b1FlagSys1Minute)
//		{
//            for (uint8_t j = 0; j < (NET_SUB1G_MAX_POINT); j++)// 
//            {      
//                if(0 != reals.net_point_base_Info[j].mac_union.MAC_64)//有效MAC，待存储
//                {
//                    MicroInv[j+1].mod_reg29700_IOT_info.revd1++;;
//                }
//            }
//		}
    }
}

/*------------------------------------------------------------------------------
 Function: iot_period_slow_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      慢速任务，允许延时
  * @param[in]  void * pvParameters  
  * @param[out] None
  * @return     void
  */
void iot_period_slow_task (void * pvParameters)
{
	static uint32_t debugcnt_100ms=0;
	static uint32_t debugcnt2_100ms=0;
	static uint8_t ts_u16Cnt1000ms = 0;
	static uint8_t ts_u16Cnt100ms = 0;
	static uint16_t ts_u16Report100ms = 0;
	static uint16_t scnt_1s = 0;
	
    web_server_start();

#ifdef CONFIG_ENABLE_MD_TCP_SLAVE
    //modbus tcp slave
    modbus_tcp_start(SetData.dev_info_t.modbus_tcp_enable.crypt_en, SetData.dev_info_t.modbus_tcp_port, 0);
#endif

    while (1)
    {
		if(ts_u16Cnt100ms != reals.Timer_100msCount)							  
		{
			ts_u16Cnt100ms = reals.Timer_100msCount;
			reals.st_FlagTime.bits.b1FlagSys100ms_slow_period_task = 1;				 
		}
		else
		{
			reals.st_FlagTime.bits.b1FlagSys100ms_slow_period_task = 0;
		}
		
    	if(ts_u16Cnt1000ms != reals.Timer_1sCount)							  
		{
			ts_u16Cnt1000ms = reals.Timer_1sCount;
			reals.st_FlagTime.bits.b1FlagSys1s_slow_period_task = 1;				 
		}
		else
		{
			reals.st_FlagTime.bits.b1FlagSys1s_slow_period_task = 0;
		}
//	ESP_LOGI(TAG, "iot_period_slow_task 	");
		vTaskDelay(pdMS_TO_TICKS(50));//

        web_server_task();

        modbus_tcp_task();

#ifdef CONFIG_MODBUS_REG_TLV_ENABLE
        // 处理 modbus TLV 接收队列中的消息
        modbus_tlv_process_task();
#endif

//		if(reals.time_init_flag == 1)//时间有效，debug disable
		{
			inv_fault_check();
			inv_achub_fault_check();
			inv_alarm_check();
			inv_achub_alarm_check();
			Logger_event_check();
		}


		if(1==reals.st_FlagTime.bits.b1FlagSys1s_slow_period_task)
		{
			if ((1 == reals.rtc_flag.sBit.RTC_valid_from_NTP)
			||(1 == reals.rtc_flag.sBit.RTC_valid_from_APP)
			||(1 == reals.rtc_flag.sBit.RTC_valid_from_SERVER)
			||(1 == reals.rtc_flag.sBit.RTC_valid_from_CAN))
			{
				reals.time_init_flag = 1; // 系统时间有效标志
			}
			Relay_Ctrl_Parameter_update(reals.time_init_flag, Inv[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.soc);
			Relay_Ctrl_Main_Task();
			// 智能TOU控制模块周期处理函数
			Smart_Tou_Ctrl_Module_Process();
            // IF-THEN智能控制数据更新
            Smart_Tou_Ctrl_Data_Update();

            /*系统内所有设备状态检查*/
            //Dev_All_Point_State_Check();

			CanOtaFinsihedCtrl();

			/*版本检查超时清除*/
			https_new_image_timeout_clean();

            http_period_update();

            if ( ++scnt_1s > 60 )
            {
                scnt_1s = 0;

                /*版本检查计时*/
                http_version_update_timeout();

				/*时间更新计时*/
				http_dst_time_update_timeout();	
			}
		}

		if(1 == reals.st_FlagTime.bits.b1FlagSys100ms_slow_period_task)
		{
			History_Cnt_Read_Flag_Update();
            /*UDP组播发送*/
			wifi_mesh_Top_Tx_task();	
//			app_sys_debug_info();
			
			/*绑定帧主动上报*/
        	if((++ts_u16Report100ms>=600)||(reals.net_point_Comein == 1))//每一分钟或节点设备变化触发上报
        	{
				//当前数据刷新时开启蓝牙和MQTT立即上报，在4g网络模式下mqtt仅支持在收到modbus读指令时会主动上报（节省流量）
				ts_u16Report100ms=0;
				reals.net_point_Comein = 0;
				reals.modbus_self_report_ble = 1;
				if ( g_self_data.mod_reg11000_IOT_info.MqttConnectMode != 3 ) {
                    reals.modbus_self_report_mqtt = 1;
                }
        	}

			
		    /*UDP信息接收及处理*/
		    // UDP_Rx_Task();	

			if(++debugcnt_100ms >= 50)//5s
			{
				debugcnt_100ms=0;
				// time_sync_rtc_to_local();			
			}
			else if(0 == (debugcnt_100ms%2))//200ms
			{
	//			Write_Eeprom_Top_App(); 			
			}
			else if(1 == (debugcnt_100ms%2))//200ms
			{
				Flash_Update_Scan();
				Ext_Flash_Store();				
			}
			if(++debugcnt2_100ms >= 30)//3s
			{
				debugcnt2_100ms=0;
	
	//			debug_print_data_from_storage(0);
				/*NVS基本参数更新*/
			}


		
			 /*蓝牙主动上报任务*/
			//ESP_LOGW(TAG,"test go to Modbus_Bind_Ask_Ble");
			Modbus_Bind_Ask_Ble(); 

			/*MQTT上报任务*/
			iot_mqtt_actively_report_task();   

		}
	}
}



/*
windy 整合EMS函数
*/
void sum_task_init(void)
{

/*
FreeRTOS的任务优先级是，任务优先级数值越小，优先级越低

*/
	// 消息队列,modbus的设置通过消息队列通知任务转换成CAN命令
	can_cmd_queue = xQueueCreate(5, sizeof(can_cmd_queue_struct));
	if (can_cmd_queue == NULL) {
		ESP_LOGE(TAG, "can_cmd_queue create failed");
		abort();
	}
	
	// CAN接收指令队列,系统中可能有多个功能模块处理接收指令,该队列中目前只处理与历史数据相关的CAN指令
	can_recv_cmd_queue = xQueueCreate(5, sizeof(can_recv_cmd_t));
	if (can_recv_cmd_queue == NULL) {
		ESP_LOGE(TAG, "can_recv_cmd_queue create failed");
		abort();
	}
	

    /* 创建log存储消息队列 */
    xQueue_Log_record = xQueueCreate(10, sizeof(LOG_FAULT_STRUCT_queue_struct));
    if (!xQueue_Log_record) {
        ESP_LOGE (TAG, "xQueue_Log_record queue create failed");
		abort();
		
    }
    xQueue_iot_historydata_record = xQueueCreate(10, sizeof(LOG_EventHistoryData_Struct));
    if (!xQueue_iot_historydata_record) {
        ESP_LOGE (TAG, "xQueue_iot_historydata_record queue create failed");
		abort();
		
    }

	//box_task_init();
	#ifdef INV_LOG_DETAILED_INFO_RECORD
		xQueue_inv_Detailed_Info_record= xQueueCreate(10, sizeof(Log_Inv_Detailed_Info_Queue_Struct));
		if (!xQueue_inv_Detailed_Info_record) {
			ESP_LOGE (TAG, "xQueue_inv_Detailed_Info_record queue create failed");
			abort();
		}
	#endif

	Cellular_init();


#ifdef CAN_PORT_ENABLE
	if (xTaskCreatePinnedToCore(iot_can_low_level_task,	 "iot-can-low",  1024 * 4, NULL, 1, NULL, 1) != pdPASS) { /* can 总线任务 */
		ESP_LOGE(TAG, "iot_can_low_level_task create failed");
		
		abort();
	}
	if (xTaskCreatePinnedToCore(esp_canbus_recv_task,	 "iot-can-Rx",  1024 * 4, NULL, 1, NULL, 1) != pdPASS) { /* can recv任务 */
		ESP_LOGE(TAG, "esp_canbus_recv_task create failed");
		
		abort();
	}
	if (xTaskCreatePinnedToCore(esp_canbus_send_task,	 "can-send",  1024 * 3, NULL, 1, NULL, 1) != pdPASS) { /* can send任务 */
		ESP_LOGE(TAG, "esp_canbus_recv_task create failed");
		
		abort();
	}

	

	
//填充输入到队列	
	if (xTaskCreatePinnedToCore(iot_can_high_level_task, "iot-can-high",  1024 * 8, NULL, 1, NULL, 1) != pdPASS) {	/*6*/
		ESP_LOGE(TAG, "iot_can_high_level_task create failed");
		
		abort();
	}

#endif

    /* ble 蓝牙任务 */
	if (xTaskCreatePinnedToCore(iot_ble_task,	 "iot-ble",  1024 * 5, can_cmd_queue, 5, NULL,	1) != pdPASS) 
    {
		ESP_LOGE(TAG, "iot_ble_task create failed");
        
		abort();
	}
	vTaskDelay(pdMS_TO_TICKS(200));

	/* wifi sta任务 */
	if (xTaskCreatePinnedToCore(iot_wifi_task,	 "iot-wifi", 1024 * 4, can_cmd_queue, 5, NULL,	1) != pdPASS) 
    {
		ESP_LOGE(TAG, "iot_wifi_task create failed");
		
		abort();
	}
	vTaskDelay(pdMS_TO_TICKS(200));

	if (xTaskCreatePinnedToCore(iot_period_task, "iot-period", 1024 * 4, NULL, 1, NULL, 1) != pdPASS) {  /* 周期更新WiFi+BLE状态到CAN数据 */
		ESP_LOGE(TAG, "iot_period_task create failed");
		
		abort();
	}
	
	if (xTaskCreatePinnedToCore(iot_period_slow_task, "iot-period2", 1024 * 8, NULL,1, NULL, 1) != pdPASS) {  /* 周期更新WiFi+BLE状态到CAN数据 */
		ESP_LOGE(TAG, "iot_period_slow_task create failed");
		
		abort();
	}	
	
	//解析输出队列A

	vTaskDelay(pdMS_TO_TICKS(200));
	
	/* 任务优先级设置的高于mqtt库内部的任务优先级(5) */
    if (xTaskCreatePinnedToCore(iot_mqtt_client_task, "iot-mqtt1", 1024 * 10, can_cmd_queue, 7, NULL, 1) != pdPASS) 
	{
        ESP_LOGE(TAG, "iot_wifi_mqtt_task create failed");
		abort();
    }
	vTaskDelay(pdMS_TO_TICKS(200));

#ifdef     TCP_SERVER2_ENABLE	
	// if (xTaskCreatePinnedToCore(iot_mqtt_client_task2, "iot-mqtt2", 1024 * 4, can_cmd_queue, 7, NULL, 1) != pdPASS) 
	// {
	// 	ESP_LOGE(TAG, "iot_mqtt_client_task2 create failed");
	// 	abort();
	// }
	
#endif	

#ifdef  CONFIG_ENABLE_HTTP_OTA

   create_https_task(); // https下载文件

#endif

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
    if (xTaskCreatePinnedToCore(wl_mesh_task, "IoT-mesh", 1024 * 8, NULL, 7, NULL, 1) != pdPASS)
    {
        ESP_LOGE(TAG, "create iot mesh task failed");
        abort();
    }
    vTaskDelay(pdMS_TO_TICKS(200));
#endif
}


//大端模式获取16字节长度的数据
uint64_t  GetUin64FromPtrBig( const uint16_t* uBufPoint)
{
    return (uint64_t)((((uint64_t)uBufPoint[0])<<48)&0xffff000000000000)|( (((uint64_t)uBufPoint[1]<<32)&0xffff00000000))|((((uint64_t)uBufPoint[2]<<16)&0xffff0000))|(((uint64_t)uBufPoint[3]&0xffff));
} 

//小端模式获取16字节长度的数据
uint64_t  GetUin64FromPtrSmall( const uint16_t* uBufPoint)
{
    return (uint64_t)((((uint64_t)uBufPoint[3])<<48)&0xffff000000000000)|( (((uint64_t)uBufPoint[2]<<32)&0xffff00000000))|((((uint64_t)uBufPoint[1]<<16)&0xffff0000))|(((uint64_t)uBufPoint[0]&0xffff));
} 

void link_report_set(uint8_t ble_report)
{
	if(reals.dev_change_ctrl.bit.link_report!=(ble_report&0x1))
	{
		reals.dev_change_ctrl.bit.link_report=ble_report&0x1;
	}
}

void iot_can_subdev_online_rise_notify(void)
{
	if (!reals.Addr_can_master || reals.ota_happen) {
		return;
	}
	link_report_set(1);
	reals.iot_smart_change = 1;
	ESP_LOGW(TAG, "can subdev online 0->1, resend link & smart control");
}

//
//
///*------------------------------------------------------------------------------
// Function: iot_wifi_eth_4g_switch_user
// -----------------------------------------------------------------------------*/
///**
//  * @brief      切换网络使用（0：wifi，1：以太网，2：4g）
//  * @param[in]  void  
//  * @param[out] None
//  * @return     void
//  */
//void iot_wifi_eth_4g_switch_user(uint8_t mode)
//{
//    /*维持其他设置不变*/
//    memcpy(&g_self_data_WR.mod_reg12000_IOT_set.on_off, &g_self_data.mod_reg12000_IOT_set.on_off, sizeof(g_self_data.mod_reg12000_IOT_set.on_off));
//    
//    if (mode == 1)
//    {
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit.wifi_enable = 2;
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit._4g_enable = 2;
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit.Eth_enable = 1;
//    }
//    else if (mode == 2)
//    {
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit.wifi_enable = 2;
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit._4g_enable = 1;
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit.Eth_enable = 2;
//    }
//    else
//    {
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit.wifi_enable = 1;
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit._4g_enable = 2;
//        g_self_data_WR.mod_reg12000_IOT_set.on_off.bit.Eth_enable = 2;
//    }
//    reals.ModbusCmdFlag.sBit.new_cfg = 1;
//}

