#include "iot_period_task.h"
#include "parameter.h"
#include "task_debug.h"
#include "can_transmit.h"
#include "app_bt.h"
#include "iot_timer.h"
#include "tou_relay_ctrl.h"
#include "can_protocol.h"
#include "iot_ota.h"
#include "iot_wifi.h"
#include "bt_adv.h"
#include "webserver.h"
#include "nvs_store.h"
#include "iot_mqtt.h"
#include "dev_data_record.h"
#include "http_client.h"
#include "wlcc_process.h"
#include "sync_time.h"
#include "DisasterWarn.h"
#include "iot_box_task.h"
#include "modbus_data.h"
#include "modbus_tcp_slave.h"
#include "grid_off_statistic.h"
#include "log_redirect.h"

#include <esp_log.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "ble_client_central.h"
#include "general_build_get.h"

#include "reg_change_log.h"
#include "dev_access_log.h"
#include "file_ota.h"
#include "energy_process.h"
#include "smart_tou_ctrl_v1.h"
#include "ac_ems_cloud.h"
#include "usb_host_modem.h"
#include "iot_sleep.h"
#include "usb_host_hid.h"
#include "bms_can.h"
#include "xmodem_client.h"
#include "disater_warn_simple.h"
#include "modbus_slave.h"
#include "open_mqtt.h"
#include "can_product.h"
#include "image_back.h"
#include "modbus_tlv.h"
#include "iot_temperature.h"
#include "off_grid_load_power.h"
#include "cloud_sync.h"
#include "aiot_can_log.h"
#include "udt_transfer.h"
#include "mqtt_log.h"

#define TAG "[Period_Task]"

extern int udt_period_check_and_report(void);
extern void log_task_stack_highwater(void);

/* helper: 6-byte little-endian -> u64 */
static inline uint64_t le6_to_u64(const uint8_t *b)
{
    uint64_t v = 0;
    /* 小端：低字节在前 */
    for (int i = 5; i >= 0; --i) {
        v = (v << 8) | (uint64_t)b[i];
    }
    return v;
}

/**
 * @brief 主循环定时标志
 */
void app_system_timer(void)
{
    static uint8_t ts_u16Cnt10ms = 0;
    static uint8_t ts_u16Cnt50ms = 0;
    static uint8_t ts_u16Cnt100ms = 0;
    static uint8_t ts_u16Cnt500ms = 0;
    static uint8_t ts_u16Cnt1000ms = 0;
    static uint8_t ts_u16Cnt1minute = 0;

    if (ts_u16Cnt10ms != reals.Timer_5msCount)
    {
        ts_u16Cnt10ms = reals.Timer_5msCount;
        reals.st_FlagTime.bits.b1FlagSys10ms = 1;
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys10ms = 0;
    }

    if (ts_u16Cnt50ms != reals.Timer_50msCount)
    {
        ts_u16Cnt50ms = reals.Timer_50msCount;
        reals.st_FlagTime.bits.b1FlagSys50ms = 1;
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys50ms = 0;
    }

    if (ts_u16Cnt100ms != reals.Timer_100msCount)
    {
        ts_u16Cnt100ms = reals.Timer_100msCount;
        reals.st_FlagTime.bits.b1FlagSys100ms = 1;
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys100ms = 0;
    }

    if (ts_u16Cnt500ms != reals.Timer_500msCount)
    {
        ts_u16Cnt500ms = reals.Timer_500msCount;
        reals.st_FlagTime.bits.b1FlagSys500ms = 1;
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys500ms = 0;
    }
    if (ts_u16Cnt1000ms != reals.Timer_1sCount)
    {
        ts_u16Cnt1000ms = reals.Timer_1sCount;
        reals.st_FlagTime.bits.b1FlagSys1s = 1;
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys1s = 0;
    }

    if (ts_u16Cnt1minute != reals.Timer_1MinuteCount)
    {
        ts_u16Cnt1minute = reals.Timer_1MinuteCount;
        {
            reals.st_FlagTime.bits.b1FlagSys1Minute = 1;
        }
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys1Minute = 0;
    }

    if (1 == reals.st_FlagTime.bits.b1FlagSys1s) // 触发set
    {
        reals.st_FlagTime.bits.flagA_1s = 1;
        reals.st_FlagTime.bits.flagC_1s = 1;
    }
}

static uint8_t iot_on_off_check(void)
{
	uint8_t update_cfg = 0;
    uint8_t update_wifi = 0;
	config_status *old_cfg = &IotSetData.dev_info_t.on_off;
	config_status *new_cfg = &top_modbus_wr.Inv.mod_reg12000_IOT_set.on_off;
	
    /*开关状态不同*/
	if (old_cfg->all != new_cfg->all)
	{
		if(old_cfg->bit.wifi_enable != new_cfg->bit.wifi_enable)			// wifi开关改变
		{
			if (new_cfg->bit.wifi_enable != 0)
			{
				ESP_LOGW(TAG, "wifi_enable on-off: %s", (new_cfg->bit.wifi_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.wifi_enable = new_cfg->bit.wifi_enable;
                if ( new_cfg->bit.wifi_enable == 0x01 ) {
                    if (old_cfg->bit.wifi_sta_enable == 0x01) iot_wifi_open(WIFI_Set_Mode_STA);
                    if (old_cfg->bit.wifi_ap_enable == 0x01) iot_wifi_open(WIFI_Set_Mode_AP);
                } else {
                    if (old_cfg->bit.wifi_sta_enable == 0x01) iot_wifi_close(WIFI_Set_Mode_STA);
                    if (old_cfg->bit.wifi_ap_enable == 0x01) iot_wifi_close(WIFI_Set_Mode_AP);
                }
				update_cfg = 1;
                update_wifi = 1;
			}
		}

		if(old_cfg->bit.wifi_sta_enable != new_cfg->bit.wifi_sta_enable)	// wifi station开关改变
		{
			if (new_cfg->bit.wifi_sta_enable != 0)
			{
				ESP_LOGW(TAG, "wifi_sta_enable on-off: %s", (new_cfg->bit.wifi_sta_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.wifi_sta_enable = new_cfg->bit.wifi_sta_enable;
                if ( old_cfg->bit.wifi_enable == 0x01 ) {
                    (new_cfg->bit.wifi_sta_enable==0x01) ? (iot_wifi_open(WIFI_Set_Mode_STA)) : (iot_wifi_close(WIFI_Set_Mode_STA));
                }
				update_cfg = 1;
                update_wifi = 1;
			}
		}

		if(old_cfg->bit.wifi_ap_enable != new_cfg->bit.wifi_ap_enable)		// wifi ap开关改变
		{
			if (new_cfg->bit.wifi_ap_enable != 0)
			{
				ESP_LOGW(TAG, "wifi_ap_enable on-off: %s", (new_cfg->bit.wifi_ap_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.wifi_ap_enable = new_cfg->bit.wifi_ap_enable;
                if ( old_cfg->bit.wifi_enable == 0x01 ) {
                    (new_cfg->bit.wifi_ap_enable==0x01) ? (iot_wifi_open(WIFI_Set_Mode_AP)) : (iot_wifi_close(WIFI_Set_Mode_AP));
                }
				update_cfg = 1;
                update_wifi = 1;
			}
		}

		if(old_cfg->bit.ble_enable != new_cfg->bit.ble_enable)				// ble开关改变
		{
			if (new_cfg->bit.ble_enable != 0)
			{
				ESP_LOGW(TAG, "ble_enable on-off: %s", (new_cfg->bit.ble_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.ble_enable = new_cfg->bit.ble_enable;
				(new_cfg->bit.ble_enable==0x01) ? (iot_ble_start()) : (iot_ble_stop());
				update_cfg = 1;
			}
		}

		if(old_cfg->bit.ble_adv_enable != new_cfg->bit.ble_adv_enable)		// ble广播开关改变
		{
			if (new_cfg->bit.ble_adv_enable != 0)
			{
				ESP_LOGW(TAG, "ble_adv_enable on-off: %s", (new_cfg->bit.ble_adv_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.ble_adv_enable = new_cfg->bit.ble_adv_enable;
				update_cfg = 1;
			}
		}

		if(old_cfg->bit.ble_mash_enable != new_cfg->bit.ble_mash_enable)	// ble mesh开关改变
		{
			if (new_cfg->bit.ble_mash_enable != 0)
			{
				ESP_LOGW(TAG, "ble_mash_enable on-off: %s", (new_cfg->bit.ble_mash_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.ble_mash_enable = new_cfg->bit.ble_mash_enable;
				update_cfg = 1;
			}
		}

		if(old_cfg->bit._4g_enable != new_cfg->bit._4g_enable)				// 4g开关改变
		{
			if (new_cfg->bit._4g_enable != 0)
			{
				ESP_LOGW(TAG, "_4g_enable on-off: %s", (new_cfg->bit._4g_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit._4g_enable = new_cfg->bit._4g_enable;
#ifdef CONFIG_USB_4G_MODULE_ENABLE
                (new_cfg->bit._4g_enable==0x01) ? (iot_4G_start()) : (iot_4G_stop());
#endif                
				update_cfg = 1;
			}
		}

		if(old_cfg->bit.gps_enable != new_cfg->bit.gps_enable)				// gps开关改变
		{
			if (new_cfg->bit.gps_enable != 0)
			{
				ESP_LOGW(TAG, "gps_enable on-off: %s", (new_cfg->bit.gps_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.gps_enable = new_cfg->bit.gps_enable;
				update_cfg = 1;
			}
		}
        if(old_cfg->bit.Eth_enable != new_cfg->bit.Eth_enable)				// Eth开关改变
		{
			if (new_cfg->bit.Eth_enable != 0)
			{
				ESP_LOGW(TAG, "Eth_enable on-off: %s", (new_cfg->bit.Eth_enable==0x01) ? ("open") : ("close"));
                old_cfg->bit.Eth_enable = new_cfg->bit.Eth_enable;
				update_cfg = 1;
			}
		}

        if ( update_cfg ) {
            reals.SetDataWrFlag.sBit.on_off = 1;
        }
	}

	if (update_cfg) {
		ESP_LOGW(TAG, "new on-off config: %08lx", new_cfg->all);
	}
    
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.on_off.all = IotSetData.dev_info_t.on_off.all;
    
	return update_wifi;
}

/*------------------------------------------------------------------------------
 Function: ble_password_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      app密码检查
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t ble_password_check(void)
{
    uint8_t update_cfg = 0;
	uint8_t *old = IotSetData.dev_info_t.app_password;
	uint8_t *new = top_modbus_wr.Inv.mod_reg00000.app_password;
	if (memcmp(old, new, sizeof(IotSetData.dev_info_t.app_password)) != 0)
	{
		ESP_LOGW(TAG, "change app password, old:%02x %02x %02x %02x %02x %02x --->>> new: %02x %02x %02x %02x %02x %02x",
			old[0], old[1], old[2], old[3], old[4], old[5], new[0], new[1], new[2], new[3], new[4], new[5]);
        memcpy(&IotSetData.dev_info_t.app_password, &top_modbus_wr.Inv.mod_reg00000.app_password, sizeof(IotSetData.dev_info_t.app_password));
        update_cfg |= 1;

#ifdef CONFIG_WEBSERVER_ENABLE
        web_server_config_push();
#endif
	}

    memcpy(&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.app_password, &IotSetData.dev_info_t.app_password, sizeof(IotSetData.dev_info_t.app_password));//蓝牙密码
    
    if(update_cfg) reals.SetDataWrFlag.sBit.app_password = 1;
    
    return update_cfg;
}

/*------------------------------------------------------------------------------
 Function: iot_clean_flag_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      能量和历史记录清零
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_clean_flag_check(void)
{
    // 清除全部数据
	if(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_all)
	{
		top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_all = 0;
        top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_dc = 0;
        top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_ac = 0;
        top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_pv = 0;
        top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_chg = 0;
        top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_dsg = 0;

#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
        // 恢复能量文件到出厂默认并初始化文件头并写入存储
        Energy_File_Restore_Factory_Defaults();
#endif
        
        top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_log = 1;
	}

#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
    
	if(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_dc)
	{
		top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_dc = 0;
#ifdef ENABLE_ENERGY_RAW_DC_LOAD        
        Energy_File_Clean_Specific_Data_By_Index(ENERGY_IDX_DC_LOAD);        
#endif
	}
    
	if(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_ac)
	{
		top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_ac = 0;
#ifdef ENABLE_ENERGY_RAW_AC_LOAD        
        Energy_File_Clean_Specific_Data_By_Index(ENERGY_IDX_AC_LOAD);        
#endif
	}
	
	
	if(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_pv)
	{
		top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_pv = 0;
#ifdef ENABLE_ENERGY_RAW_PV_TOTAL        
        Energy_File_Clean_Specific_Data_By_Index(ENERGY_IDX_PV_TOTAL);        
#endif        
	}
    
	if(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_chg)
	{
		top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_chg = 0;
#ifdef ENABLE_ENERGY_RAW_GRID_INPUT        
        Energy_File_Clean_Specific_Data_By_Index(ENERGY_IDX_GRID_INPUT);        
#endif        
	}
    
	if(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_dsg)
	{
		top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_grid_dsg = 0;
#ifdef ENABLE_ENERGY_RAW_GRID_OUTPUT        
        Energy_File_Clean_Specific_Data_By_Index(ENERGY_IDX_GRID_OUTPUT);        
#endif        
	}
    
#endif

	if(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_log)
	{
		top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_log = 0; 
        
#ifndef MODBUS_GET_HISTORY_FROM_ARM_ENABLE
        /*当使用ARM历史故障时，IOT历史故障作为备份，不可清除*/
		clear_log_file();
#endif
	}    
}

/*------------------------------------------------------------------------------
 Function: thunder_ctrl_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      风暴预警控制检查
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t thunder_ctrl_check(void)
{
    uint8_t update_cfg = 0;
    thunder_ctrl_t *old_cfg = &IotSetData.dev_info_t.thunder_ctrl;
    thunder_ctrl_t *new_cfg = &top_modbus_wr.Inv.mod_reg12000_IOT_set.thunder_ctrl;
    new_cfg->thunder_state = old_cfg->thunder_state;            // 状态不参与比较

    if (old_cfg->all != new_cfg->all)
    {
        if (new_cfg->thunder_enable != 0)
        {
            if ((old_cfg->thunder_enable != 2) && (new_cfg->thunder_enable == 2))   // 只有风暴模式使能开关变化时才存储到NVS
            {
                /* 风暴模式失能 */
                ESP_LOGW(TAG, "thunder storm mode disable, value: %02x", new_cfg->all);
                update_cfg = 1;
                old_cfg->thunder_enable = new_cfg->thunder_enable;
#if CONFIG_DISASTER_WARN_CHANNEL == 2                  
				SetStormWatchEnableRegisterData(new_cfg->thunder_enable);
#endif
            }

            if ((old_cfg->thunder_enable != 1) && (new_cfg->thunder_enable == 1))   // 只有风暴模式使能开关变化时才存储到NVS
            {
                /* 风暴模式使能 */
                ESP_LOGW(TAG, "thunder storm mode enable, value: %02x", new_cfg->all);
                update_cfg = 1;
                old_cfg->thunder_enable = new_cfg->thunder_enable;
#if CONFIG_DISASTER_WARN_CHANNEL == 2                  
				SetStormWatchEnableRegisterData(new_cfg->thunder_enable);
#endif
            }

            old_cfg->thunder_enable = new_cfg->thunder_enable;
        }

        if (new_cfg->thunder_cancel == 1)
        {
            /* 取消当前风暴事件 */
            ESP_LOGW(TAG, "cancel thunder storm event, value: %02x", new_cfg->all);
            old_cfg->thunder_cancel = new_cfg->thunder_cancel;
            new_cfg->thunder_cancel = 0;
#if CONFIG_DISASTER_WARN_CHANNEL == 2  
			SetAppCancelEventData(new_cfg->thunder_cancel);
#endif
        }
    }

    return update_cfg;
}  

 /*------------------------------------------------------------------------------
  Function: iot_wifi_mult_sta_cfg
  -----------------------------------------------------------------------------*/
 /**
   * @brief      wifi_sta使能设置
   * @param[in]  void  
   * @param[out] None
   * @return     static uint8_t
   */
static uint8_t iot_wifi_mult_sta_cfg(void)
{
     uint8_t mult_sta_update = 0;

     if (reals.ModbusCmdFlag.sBit.wifi_mul_sta == 1)
     {
         reals.ModbusCmdFlag.sBit.wifi_mul_sta = 0;
 
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta1_en > 0)
         {
             if (IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en
                 != top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta1_en)
             {
                 IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en = top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta1_en;
                 ESP_LOGW(TAG, "wifi mul sta sta1_en: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en);
                 mult_sta_update |= 1;
                 //TODO：检测当前sta1是否连接上了，如果已连接且sta1_en为0，则需要断开连接
             }
         }
 
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta2_en > 0)
         {
             if (IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en
                 != top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta2_en)
             {
                 IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en = top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta2_en;
                 ESP_LOGW(TAG, "wifi mul sta sta2_en: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en);
                 mult_sta_update |= 1;
                 //TODO：检测当前sta2是否连接上了，如果已连接且 sta2_en 为0，则需要断开连接
             }
         }
 
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta3_en > 0)
         {
             if (IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en
                 != top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta3_en)
             {
                 IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en = top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta3_en;
                 ESP_LOGW(TAG, "wifi mul sta sta3_en: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en);
                 mult_sta_update |= 1;
                 //TODO：检测当前sta2是否连接上了，如果已连接且 sta3_en 为0，则需要断开连接
             }
         }
 
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en > 0)
         {
             if (IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en
                 != top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en)
             {
                 IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en = top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta1_static_ip_en;
                 ESP_LOGW(TAG, "wifi mul sta sta1_static_ip_en: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en);
                 mult_sta_update |= 1;
                 //TODO：检测当前sta2是否连接上了，如果已连接且 sta1_static_ip_en 为0，则需要断开连接，重新获取IP
             }
         }
 
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en > 0)
         {
             if (IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en != top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en)
             {
                 IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en = top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta2_static_ip_en;
                 ESP_LOGW(TAG, "wifi mul sta sta2_en: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en);
                 mult_sta_update |= 1;
                 //TODO：检测当前sta2是否连接上了，如果已连接且 sta2_static_ip_en 为0，则需要断开连接，重新获取IP
             }
         }
 
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en > 0)
         {
             if (IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en != top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en)
             {
                 IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en = top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en.sta3_static_ip_en;
                 ESP_LOGW(TAG, "wifi mul sta sta3_static_ip_en: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en);
                 mult_sta_update |= 1;
                 //TODO：检测当前sta2是否连接上了，如果已连接且 sta3_static_ip_en 为0，则需要断开连接，重新获取IP
             }
         }
 
         top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_mul_sta_en = IotSetData.dev_info_t.wifi_mul_sta_en;
         if(mult_sta_update) reals.SetDataWrFlag.sBit.wifi_mul_sta = 1;
     }

     return mult_sta_update;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_sta1_cfg
 -----------------------------------------------------------------------------*/
/**
  * @brief      wifi_sta1设置
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_wifi_sta1_cfg(void)
{
    uint8_t wifi_sta_cfg_update = 0;
    
     /* WIFI配网变化名称 */
     if (reals.ModbusCmdFlag.sBit.wifi_sta_ssid == 1)
     {
         reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 0;
         if (strlen(top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid)) 
         {       
             if (strncmp(IotSetData.dev_info_t.wifi_sta_ssid, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid, sizeof(IotSetData.dev_info_t.wifi_sta_ssid))) 
             {
                 memcpy(IotSetData.dev_info_t.wifi_sta_ssid, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid, sizeof(IotSetData.dev_info_t.wifi_sta_ssid));
                 ESP_LOGW(TAG, "wifi name change to: %s", IotSetData.dev_info_t.wifi_sta_ssid);
                 memset(top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid, 0, sizeof(top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid));
                 wifi_sta_cfg_update |= 1;
                 reals.SetDataWrFlag.sBit.wifi_sta_ssid =1;
             }
         }
         memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_sta_ssid, IotSetData.dev_info_t.wifi_sta_ssid,  sizeof(IotSetData.dev_info_t.wifi_sta_ssid));
     }
 
     /* WIFI配网变化密码 */
     if (reals.ModbusCmdFlag.sBit.wifi_sta_password == 1)
     {
         reals.ModbusCmdFlag.sBit.wifi_sta_password = 0;
 //      if (strlen(top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password)) //存在空输入的可能(无密码wifi)
         {   
             if (strncmp(IotSetData.dev_info_t.wifi_sta_password, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password, sizeof(IotSetData.dev_info_t.wifi_sta_password))) 
             {
                 memcpy(IotSetData.dev_info_t.wifi_sta_password, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password, sizeof(IotSetData.dev_info_t.wifi_sta_password));
                 ESP_LOGW(TAG, "wifi password change to: %s", IotSetData.dev_info_t.wifi_sta_password);
                 memset(top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password, 0, sizeof(top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password));
                 wifi_sta_cfg_update |= 1;
                 reals.SetDataWrFlag.sBit.wifi_sta_password =1;
             }
         }
         memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_sta_password, IotSetData.dev_info_t.wifi_sta_password,  sizeof(IotSetData.dev_info_t.wifi_sta_password));
     }
 
     /* WiFi配网认证方式改变 */
     if (reals.ModbusCmdFlag.sBit.wifi_sta_auth == 1)
     {
         reals.ModbusCmdFlag.sBit.wifi_sta_auth = 0;
         if (IotSetData.dev_info_t.wifi_sta_auth != top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_auth) 
         {   /* WiFi配网认证方式改变 */
             if (top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_auth >= WIFI_AUTH_MAX) 
             {
                 ESP_LOGE(TAG, "sta auth(%d) error", top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_auth);
             } 
             else 
             {
                 IotSetData.dev_info_t.wifi_sta_auth = top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_auth;
                 ESP_LOGW(TAG, "wifi auth change to: %d", IotSetData.dev_info_t.wifi_sta_auth);
                 wifi_sta_cfg_update |= 1;
                 reals.SetDataWrFlag.sBit.wifi_sta_auth =1;
             }
         }
         memcpy(&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_sta_auth, &IotSetData.dev_info_t.wifi_sta_auth,  sizeof(IotSetData.dev_info_t.wifi_sta_auth));
     }

     if (reals.ModbusCmdFlag.sBit.wifi_sta1 == 1)
     {
         reals.ModbusCmdFlag.sBit.wifi_sta1 = 0;
         //TODO: 如果当前连接的是sta1, 需要同步更改IP
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_ip > 0)
         {
             if (IotSetData.dev_info_t.wifi_sta1_ip != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_ip)
             {
                 IotSetData.dev_info_t.wifi_sta1_ip = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_ip;
                 ESP_LOGW(TAG, "wifi_sta1_ip: %lu.%lu.%lu.%lu",
                     (IotSetData.dev_info_t.wifi_sta1_ip >> 0) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_ip >> 8) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_ip >> 16) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_ip >> 24) & 0xFF);
                 reals.SetDataWrFlag.sBit.wifi_sta1_ip =1;
                 wifi_sta_cfg_update |= 1;
             }
         }
     
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_mask > 0)
         {
             if (IotSetData.dev_info_t.wifi_sta1_mask != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_mask)
             {
                 IotSetData.dev_info_t.wifi_sta1_mask = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_mask;
                 ESP_LOGW(TAG, "wifi_sta1_mask: %lu.%lu.%lu.%lu",
                     (IotSetData.dev_info_t.wifi_sta1_mask >> 0) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_mask >> 8) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_mask >> 16) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_mask >> 24) & 0xFF);
                 reals.SetDataWrFlag.sBit.wifi_sta1_mask =1;
                 wifi_sta_cfg_update |= 1;
             }
         }
     
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_gw > 0)
         {
             if (IotSetData.dev_info_t.wifi_sta1_gw != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_gw)
             {
                 IotSetData.dev_info_t.wifi_sta1_gw = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_gw;
                 ESP_LOGW(TAG, "wifi_sta1_gw: %lu.%lu.%lu.%lu",
                     (IotSetData.dev_info_t.wifi_sta1_gw >> 0) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_gw >> 8) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_gw >> 16) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_gw >> 24) & 0xFF);
                 reals.SetDataWrFlag.sBit.wifi_sta1_gw =1;
                 wifi_sta_cfg_update |= 1;
             }
         }
     
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns1 > 0)
         {
             if (IotSetData.dev_info_t.wifi_sta1_dns1 != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns1)
             {
                 IotSetData.dev_info_t.wifi_sta1_dns1 = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns1;
                 ESP_LOGW(TAG, "wifi_sta1_dns1: %lu.%lu.%lu.%lu",
                     (IotSetData.dev_info_t.wifi_sta1_dns1 >> 0) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_dns1 >> 8) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_dns1 >> 16) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_dns1 >> 24) & 0xFF);
                 reals.SetDataWrFlag.sBit.wifi_sta1_dns1 =1;
                 wifi_sta_cfg_update |= 1;
             }
         }
     
         if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns2 > 0)
         {
             if (IotSetData.dev_info_t.wifi_sta1_dns2 != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns2)
             {
                 IotSetData.dev_info_t.wifi_sta1_dns2 = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns2;
                 ESP_LOGW(TAG, "wifi_sta1_dns2: %lu.%lu.%lu.%lu",
                     (IotSetData.dev_info_t.wifi_sta1_dns2 >> 0) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_dns2 >> 8) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_dns2 >> 16) & 0xFF,
                     (IotSetData.dev_info_t.wifi_sta1_dns2 >> 24) & 0xFF);
                 reals.SetDataWrFlag.sBit.wifi_sta1_dns2 =1;
                 wifi_sta_cfg_update |= 1;
             }
         }
     
         top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_ip = IotSetData.dev_info_t.wifi_sta1_ip;
         top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_mask = IotSetData.dev_info_t.wifi_sta1_mask;
         top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_gw = IotSetData.dev_info_t.wifi_sta1_gw;
         top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_dns1 = IotSetData.dev_info_t.wifi_sta1_dns1;
         top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_dns2 = IotSetData.dev_info_t.wifi_sta1_dns2;
     }

     return wifi_sta_cfg_update;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_sta2_cfg
 -----------------------------------------------------------------------------*/
/**
  * @brief      wifi_sta2设置
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_wifi_sta2_cfg(void)
{
    uint8_t wifi_sta_cfg_update = 0;
    
    if (reals.ModbusCmdFlag.sBit.wifi_sta2 == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_sta2 = 0;

        if (IotSetData.dev_info_t.wifi_sta2_auth != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_auth) 
        {
            if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_auth >= WIFI_AUTH_MAX)
            {
                ESP_LOGE(TAG, "sta2 auth(%d) error", top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_auth);
            }
            else
            {
                IotSetData.dev_info_t.wifi_sta2_auth = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_auth;
                ESP_LOGW(TAG, "wifi sta2 auth change to: %d", IotSetData.dev_info_t.wifi_sta2_auth);
                reals.SetDataWrFlag.sBit.wifi_sta2_auth = 1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (strlen(top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ssid))
        {
            if (strncmp((char *)IotSetData.dev_info_t.wifi_sta2_ssid, (char *)top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ssid, sizeof(IotSetData.dev_info_t.wifi_sta2_ssid))) 
            {
                memcpy(IotSetData.dev_info_t.wifi_sta2_ssid, top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ssid, sizeof(IotSetData.dev_info_t.wifi_sta2_ssid));
                ESP_LOGW(TAG, "wifi sta2 name change to: %s", IotSetData.dev_info_t.wifi_sta2_ssid);
                reals.SetDataWrFlag.sBit.wifi_sta2_ssid =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        //可以配置空密码
        if (strncmp(IotSetData.dev_info_t.wifi_sta2_password, top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_password, sizeof(IotSetData.dev_info_t.wifi_sta2_password))) 
        {
            memcpy(IotSetData.dev_info_t.wifi_sta2_password, top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_password, sizeof(IotSetData.dev_info_t.wifi_sta2_password));
            ESP_LOGW(TAG, "wifi sta2 password change to: %s", IotSetData.dev_info_t.wifi_sta2_password);
            reals.SetDataWrFlag.sBit.wifi_sta2_password =1;
            wifi_sta_cfg_update |= 1;
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ip > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta2_ip != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ip)
            {
                IotSetData.dev_info_t.wifi_sta2_ip = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ip;
                ESP_LOGW(TAG, "wifi_sta2_ip: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta2_ip >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_ip >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_ip >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_ip >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta2_ip =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_mask > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta2_mask != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_mask)
            {
                IotSetData.dev_info_t.wifi_sta2_mask = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_mask;
                ESP_LOGW(TAG, "wifi_sta2_mask: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta2_mask >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_mask >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_mask >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_mask >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta2_mask =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_gw > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta2_gw != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_gw)
            {
                IotSetData.dev_info_t.wifi_sta2_gw = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_gw;
                ESP_LOGW(TAG, "wifi_sta2_gw: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta2_gw >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_gw >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_gw >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_gw >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta2_gw =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns1 > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta2_dns1 != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns1)
            {
                IotSetData.dev_info_t.wifi_sta2_dns1 = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns1;
                ESP_LOGW(TAG, "wifi_sta2_dns1: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta2_dns1 >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_dns1 >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_dns1 >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_dns1 >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta2_dns1 =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        // if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns2 > 0) // DNS2允许设置为0，即只使用DNS1
        {
            if (IotSetData.dev_info_t.wifi_sta2_dns2 != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns2)
            {
                IotSetData.dev_info_t.wifi_sta2_dns2 = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns2;
                ESP_LOGW(TAG, "wifi_sta2_dns2: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta2_dns2 >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_dns2 >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_dns2 >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta2_dns2 >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta2_dns2 =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_auth = IotSetData.dev_info_t.wifi_sta2_auth;
        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_ssid, IotSetData.dev_info_t.wifi_sta2_ssid, sizeof(IotSetData.dev_info_t.wifi_sta2_ssid));
        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_password, IotSetData.dev_info_t.wifi_sta2_password, sizeof(IotSetData.dev_info_t.wifi_sta2_password));
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_ip = IotSetData.dev_info_t.wifi_sta2_ip;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_mask = IotSetData.dev_info_t.wifi_sta2_mask;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_gw = IotSetData.dev_info_t.wifi_sta2_gw;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_dns1 = IotSetData.dev_info_t.wifi_sta2_dns1;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_dns2 = IotSetData.dev_info_t.wifi_sta2_dns2;
    }

    return wifi_sta_cfg_update;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_sta3_cfg
 -----------------------------------------------------------------------------*/
/**
  * @brief      wifi_sta3设置
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_wifi_sta3_cfg(void)
{
    uint8_t wifi_sta_cfg_update = 0;
    
    if (reals.ModbusCmdFlag.sBit.wifi_sta3 == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_sta3 = 0;

        if (IotSetData.dev_info_t.wifi_sta3_auth != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_auth) 
        {
            if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_auth >= WIFI_AUTH_MAX)
            {
                ESP_LOGE(TAG, "sta3 auth(%d) error", top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_auth);
            }
            else
            {
                IotSetData.dev_info_t.wifi_sta3_auth = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_auth;
                ESP_LOGW(TAG, "wifi sta3 auth change to: %d", IotSetData.dev_info_t.wifi_sta3_auth);
                reals.SetDataWrFlag.sBit.wifi_sta3_auth =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (strlen(top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ssid))
        {
            if (strncmp(IotSetData.dev_info_t.wifi_sta3_ssid, top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ssid,
                            sizeof(IotSetData.dev_info_t.wifi_sta3_ssid))) 
            {
                memcpy(IotSetData.dev_info_t.wifi_sta3_ssid, top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ssid,
                            sizeof(IotSetData.dev_info_t.wifi_sta3_ssid));
                ESP_LOGW(TAG, "wifi sta3 name change to: %s", IotSetData.dev_info_t.wifi_sta3_ssid);
                reals.SetDataWrFlag.sBit.wifi_sta3_ssid =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        //可以配置空密码
        if (strncmp(IotSetData.dev_info_t.wifi_sta3_password, top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_password,
                        sizeof(IotSetData.dev_info_t.wifi_sta3_password))) 
        {
            memcpy(IotSetData.dev_info_t.wifi_sta3_password, top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_password,
                        sizeof(IotSetData.dev_info_t.wifi_sta3_password));
            ESP_LOGW(TAG, "wifi sta3 password change to: %s", IotSetData.dev_info_t.wifi_sta3_password);
            reals.SetDataWrFlag.sBit.wifi_sta3_password =1;
            wifi_sta_cfg_update |= 1;
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ip > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta3_ip != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ip)
            {
                IotSetData.dev_info_t.wifi_sta3_ip = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ip;
                ESP_LOGW(TAG, "wifi_sta3_ip: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta3_ip >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_ip >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_ip >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_ip >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta3_ip =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_mask > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta3_mask != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_mask)
            {
                IotSetData.dev_info_t.wifi_sta3_mask = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_mask;
                ESP_LOGW(TAG, "wifi_sta3_mask: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta3_mask >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_mask >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_mask >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_mask >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta3_mask =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_gw > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta3_gw != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_gw)
            {
                IotSetData.dev_info_t.wifi_sta3_gw = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_gw;
                ESP_LOGW(TAG, "wifi_sta3_gw: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta3_gw >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_gw >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_gw >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_gw >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta3_gw =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns1 > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta3_dns1 != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns1)
            {
                IotSetData.dev_info_t.wifi_sta3_dns1 = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns1;
                ESP_LOGW(TAG, "wifi_sta3_dns1: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta3_dns1 >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_dns1 >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_dns1 >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_dns1 >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta3_dns1 =1;
                wifi_sta_cfg_update |= 1;
            }
        }

        // if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns2 > 0)  // DNS2允许设置为0，即只使用DNS1
        {
            if (IotSetData.dev_info_t.wifi_sta3_dns2 != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns2)
            {
                IotSetData.dev_info_t.wifi_sta3_dns2 = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns2;
                ESP_LOGW(TAG, "wifi_sta3_dns2: %lu.%lu.%lu.%lu",
                    (IotSetData.dev_info_t.wifi_sta3_dns2 >> 0) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_dns2 >> 8) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_dns2 >> 16) & 0xFF,
                    (IotSetData.dev_info_t.wifi_sta3_dns2 >> 24) & 0xFF);
                reals.SetDataWrFlag.sBit.wifi_sta3_dns2 =1;
                wifi_sta_cfg_update |= 1;
            }
        }
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_auth = IotSetData.dev_info_t.wifi_sta3_auth;
        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_ssid, IotSetData.dev_info_t.wifi_sta3_ssid, sizeof(IotSetData.dev_info_t.wifi_sta3_ssid));
        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_password, IotSetData.dev_info_t.wifi_sta3_password, sizeof(IotSetData.dev_info_t.wifi_sta3_password));
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_ip = IotSetData.dev_info_t.wifi_sta3_ip;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_mask = IotSetData.dev_info_t.wifi_sta3_mask;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_gw = IotSetData.dev_info_t.wifi_sta3_gw;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_dns1 = IotSetData.dev_info_t.wifi_sta3_dns1;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_dns2 = IotSetData.dev_info_t.wifi_sta3_dns2;
    }

    return wifi_sta_cfg_update;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_ap_cfg
 -----------------------------------------------------------------------------*/
/**
  * @brief      wifi_ap设置
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_wifi_ap_cfg(void)
{
    uint8_t wifi_ap_cfg_update = 0;
    
    if (reals.ModbusCmdFlag.sBit.wifi_ap == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_ap = 0;

        if (IotSetData.dev_info_t.wifi_ap_auth != top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_ap_auth) 
        {
            if (top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_ap_auth >= WIFI_AUTH_MAX)
            {
                ESP_LOGE(TAG, "wifi ap auth(%d) error", top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_ap_auth);
            }
            else
            {
                IotSetData.dev_info_t.wifi_ap_auth = top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_ap_auth;
                ESP_LOGW(TAG, "wifi ap auth change to: %d", IotSetData.dev_info_t.wifi_ap_auth);
                wifi_ap_cfg_update |= 1;
                reals.SetDataWrFlag.sBit.wifi_ap_auth =1;
            }
        }

        if (strlen(top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_ssid))
        {
            if (strncmp(IotSetData.dev_info_t.wifi_AP_ssid, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_ssid,
                            sizeof(IotSetData.dev_info_t.wifi_AP_ssid))) 
            {
                memcpy(IotSetData.dev_info_t.wifi_AP_ssid, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_ssid,
                            sizeof(IotSetData.dev_info_t.wifi_AP_ssid));
                ESP_LOGW(TAG, "wifi ap ssid change to: %s", IotSetData.dev_info_t.wifi_AP_ssid);
                wifi_ap_cfg_update |= 1;
                reals.SetDataWrFlag.sBit.wifi_ap_ssid =1;
            }
        }

        //可以配置空密码
        if (strncmp(IotSetData.dev_info_t.wifi_AP_password, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_password,
                        sizeof(IotSetData.dev_info_t.wifi_AP_password))) 
        {
            memcpy(IotSetData.dev_info_t.wifi_AP_password, top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_password,
                        sizeof(IotSetData.dev_info_t.wifi_AP_password));
            ESP_LOGW(TAG, "wifi ap password change to: %s", IotSetData.dev_info_t.wifi_AP_password);
            wifi_ap_cfg_update |= 1;
            reals.SetDataWrFlag.sBit.wifi_ap_password =1;
        }

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_ap_auth = IotSetData.dev_info_t.wifi_ap_auth;
        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_AP_ssid, IotSetData.dev_info_t.wifi_AP_ssid, sizeof(IotSetData.dev_info_t.wifi_AP_ssid));
        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_AP_password, IotSetData.dev_info_t.wifi_AP_password, sizeof(IotSetData.dev_info_t.wifi_AP_password));
    }

    return wifi_ap_cfg_update;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_other_cfg
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI其他配置项
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_wifi_other_cfg(void)
{
    uint8_t wifi_other_update = 0;
    
    //13603~13604 wifi 多sta切换相关参数配置变化
    if (reals.ModbusCmdFlag.sBit.wifi_sta_rssi == 1)
    {
        reals.ModbusCmdFlag.sBit.wifi_sta_rssi = 0;
        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_rssi_threshold < 0)
        {
            if (IotSetData.dev_info_t.wifi_sta_rssi_th != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_rssi_threshold)
            {
                IotSetData.dev_info_t.wifi_sta_rssi_th = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_rssi_threshold;
                ESP_LOGW(TAG, "wifi sta rssi threshold: %d", IotSetData.dev_info_t.wifi_sta_rssi_th);
                reals.SetDataWrFlag.sBit.wifi_sta_rssi_th =1;
                wifi_other_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_switch_interval > 0)
        {
            if (IotSetData.dev_info_t.wifi_sta_switch_int != top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_switch_interval)
            {
                IotSetData.dev_info_t.wifi_sta_switch_int = top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_switch_interval;
                ESP_LOGW(TAG, "wifi sta switch interval to: %u", IotSetData.dev_info_t.wifi_sta_switch_int);
                reals.SetDataWrFlag.sBit.wifi_sta_switch_int =1;
                wifi_other_update |= 1;
            }
        }

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta_rssi_threshold = IotSetData.dev_info_t.wifi_sta_rssi_th;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta_switch_interval = IotSetData.dev_info_t.wifi_sta_switch_int;
    }

    /* 服务器DNS是否发生变化 */
	if (reals.ModbusCmdFlag.sBit.could_dns == 1)
	{
		reals.ModbusCmdFlag.sBit.could_dns = 0;
		/* 比较服务器DNS是否发生变化 */
		if (strlen(top_modbus_wr.Inv.mod_reg12000_IOT_set.could_dns)) 
		{
			if (strncmp(IotSetData.dev_info_t.could_dns, top_modbus_wr.Inv.mod_reg12000_IOT_set.could_dns, sizeof(IotSetData.dev_info_t.could_dns))) 
			{
                ESP_LOGW(TAG, "old cloud dns: %s", IotSetData.dev_info_t.could_dns);
				memcpy(IotSetData.dev_info_t.could_dns, top_modbus_wr.Inv.mod_reg12000_IOT_set.could_dns, sizeof(IotSetData.dev_info_t.could_dns));
				ESP_LOGW(TAG, "new cloud dns: %s", IotSetData.dev_info_t.could_dns);
				iot_mqtt_new_host(IotSetData.dev_info_t.could_dns);

                // 清除连接记录缓存
                memset(&IotSetData.dev_info_t.old_mqtt_connect_data, 0, sizeof(IotSetData.dev_info_t.old_mqtt_connect_data));
                reals.SetDataWrFlag.sBit.old_mqtt_connect = 1;
                
                /* cloud dns 改变 */
				reals.SetDataWrFlag.sBit.could_dns =1;
                wifi_other_update |= 1;
			}
		}

        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.could_dns, IotSetData.dev_info_t.could_dns,  sizeof(IotSetData.dev_info_t.could_dns));
	}

    /*mobile_apn发生变化*/
	if (reals.ModbusCmdFlag.sBit.mobile_apn == 1)
	{
		reals.ModbusCmdFlag.sBit.mobile_apn = 0;
		if (strlen(top_modbus_wr.Inv.mod_reg12000_IOT_set.mobile_apn)) 
		{
			if (strncmp(IotSetData.dev_info_t.mobile_apn, top_modbus_wr.Inv.mod_reg12000_IOT_set.mobile_apn, sizeof(IotSetData.dev_info_t.mobile_apn))) 
			{
                ESP_LOGW(TAG, "old mobile_apn: %s", IotSetData.dev_info_t.mobile_apn);
				memcpy(IotSetData.dev_info_t.mobile_apn, top_modbus_wr.Inv.mod_reg12000_IOT_set.mobile_apn, sizeof(IotSetData.dev_info_t.mobile_apn));
				ESP_LOGW(TAG, "new mobile_apn: %s", IotSetData.dev_info_t.mobile_apn);
#ifdef CONFIG_USB_4G_MODULE_ENABLE
                iot_4g_set_apn(IotSetData.dev_info_t.mobile_apn);
#endif

				/* mobile_apn 改变 */
				reals.SetDataWrFlag.sBit.mobile_apn =1;
                wifi_other_update |= 1;
			}
		}

        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.mobile_apn, IotSetData.dev_info_t.mobile_apn,  sizeof(IotSetData.dev_info_t.mobile_apn));
	}	
    
    /* iot上报周期（仅4g） */
	if (reals.ModbusCmdFlag.sBit.period_report == 1)
	{
		reals.ModbusCmdFlag.sBit.period_report = 0;

		if (top_modbus_wr.Inv.mod_reg12000_IOT_set.period_report)
        {   
			if (IotSetData.dev_info_t.data_report_cycle != top_modbus_wr.Inv.mod_reg12000_IOT_set.period_report) 
            {
				IotSetData.dev_info_t.data_report_cycle = top_modbus_wr.Inv.mod_reg12000_IOT_set.period_report;
				ESP_LOGW(TAG, "data_report_cycle to: %lu", IotSetData.dev_info_t.data_report_cycle);
				reals.SetDataWrFlag.sBit.data_report_cycle =1;
			}
		}

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.period_report = IotSetData.dev_info_t.data_report_cycle;
	}

    return wifi_other_update;
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_all_cfg_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI设置检查
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_wifi_all_cfg_check(void)
{
    wifi_mul_sta_en_t old_wifi_mul_sta_en = {0};
    uint8_t wifi_base_update_cfg = 0;
    uint8_t wifi_sta1_cfg_update = 0;
    uint8_t wifi_sta2_cfg_update = 0;
    uint8_t wifi_sta3_cfg_update = 0;
    uint8_t wifi_ap_cfg_update = 0;

    // 暂存修改前状态
    old_wifi_mul_sta_en = IotSetData.dev_info_t.wifi_mul_sta_en;

    /*WIFI辅助使能配置*/
    wifi_base_update_cfg |= iot_wifi_mult_sta_cfg();
    
    /*wifi_sta1设置*/
    wifi_sta1_cfg_update |= iot_wifi_sta1_cfg();

    /*wifi_sta2设置*/
    wifi_sta2_cfg_update |= iot_wifi_sta2_cfg();

    /*wifi_sta3设置*/
    wifi_sta3_cfg_update |= iot_wifi_sta3_cfg();

    /*wifi_ap设置*/
    wifi_ap_cfg_update |= iot_wifi_ap_cfg();

    /*WIFI相关杂项设置*/
    iot_wifi_other_cfg();

    // wifi参数发生变化后重启WiFi与网络
    // 检测WiFi参数变化后，自动判断并优先连接本次有更新且使能的STA。
    // 如果基础配置变化，自动切换或重连当前STA，确保网络正常。
    // 无效或无关修改仅保存配置，不执行重新连接操作。
    // 优先级：本次有参数更新且使能的STA > 当前使用的STA被禁用或静态IP使能变化 > 当前未连接且有新STA使能 > 仅保存配置
    if ((wifi_base_update_cfg | wifi_sta1_cfg_update | wifi_sta2_cfg_update | wifi_sta3_cfg_update) != 0) {
        if (1 == IotSetData.dev_info_t.on_off.bit.wifi_enable) {
            // WIFI基本参数更新
            iot_wifi_base_cfg_update();
            if (1 == IotSetData.dev_info_t.on_off.bit.wifi_sta_enable) {
                // 如果某个 STA 的参数本次被修改且该 STA 被使能，优先连接该 STA（按 sta1->sta2->sta3 优先级）
                if((1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en) && (wifi_sta1_cfg_update)) {    
                    iot_wifi_new_connect(WIFI_STA_INDEX1);
                } else if((1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en) && (wifi_sta2_cfg_update)) {    
                    iot_wifi_new_connect(WIFI_STA_INDEX2);
                } else if((1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en) && (wifi_sta3_cfg_update)) {    
                    iot_wifi_new_connect(WIFI_STA_INDEX3);
                } else {
                    // STA具体参数未修改或为无效修改，以使能状态变化或基础配置为准处理
                    if(wifi_base_update_cfg) {
                        // 获取当前连接的STA索引
                        uint8_t curr_sta_index = iot_wifi_sta_index_get();

                        // 检查当前使用的STA设置, 如果当前使用的 STA 被禁用，关闭并触发自动连接逻辑
                        if ((( curr_sta_index == WIFI_STA_INDEX1 ) && ( 1 != IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en )) 
                            || (( curr_sta_index == WIFI_STA_INDEX2 ) && ( 1 != IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en ))
                            || (( curr_sta_index == WIFI_STA_INDEX3 ) && ( 1 != IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en ))) {
                            // 关闭当前使用的WIFI STA, 重新执行自动连接逻辑
                            iot_wifi_disconnect();
                            iot_wifi_auto_connect();
                        }
                        // 如果当前 STA 的静态 IP 使能发生变化，重新连接当前 STA 以应用新配置
                        else if ((( curr_sta_index == WIFI_STA_INDEX1 ) && ( old_wifi_mul_sta_en.sta1_static_ip_en != IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en )) 
                                    || (( curr_sta_index == WIFI_STA_INDEX2 ) && ( old_wifi_mul_sta_en.sta2_static_ip_en != IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en ))
                                    || (( curr_sta_index == WIFI_STA_INDEX3 ) && ( old_wifi_mul_sta_en.sta3_static_ip_en != IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en ))) {
                            // 当前STA静态IP使能改变，重新连接当前STA        
                            iot_wifi_new_connect(curr_sta_index);
                        } 
                        // 当前未连接或 MQTT 未登录，可根据新增使能立即切换到新增 STA
                        else if ( !iot_wifi_sta_link_status_get() 
                                    || !iot_mqtt_login_state()) {
                            /* 优化可读性：用中间变量表达判断意图 */
                            bool sta1_now_enabled = (1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en);
                            bool sta1_new_enable_or_ip_changed = (1 != old_wifi_mul_sta_en.sta1_en) 
                                                                 || (old_wifi_mul_sta_en.sta1_static_ip_en != IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en);
                            
                            bool sta2_now_enabled = (1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en);
                            bool sta2_new_enable_or_ip_changed = (1 != old_wifi_mul_sta_en.sta2_en) 
                                                                 || (old_wifi_mul_sta_en.sta2_static_ip_en != IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en);
                            
                            bool sta3_now_enabled = (1 == IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en);
                            bool sta3_new_enable_or_ip_changed = (1 != old_wifi_mul_sta_en.sta3_en) 
                                                                 || (old_wifi_mul_sta_en.sta3_static_ip_en != IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en);
                            
                            if (sta1_now_enabled && sta1_new_enable_or_ip_changed) {
                                /* 本次启用或静态IP状态变化 -> 优先连接 STA1 */
                                iot_wifi_new_connect(WIFI_STA_INDEX1);
                            } else if (sta2_now_enabled && sta2_new_enable_or_ip_changed) {
                                /* 本次启用或静态IP状态变化 -> 优先连接 STA2 */
                                iot_wifi_new_connect(WIFI_STA_INDEX2);
                            } else if (sta3_now_enabled && sta3_new_enable_or_ip_changed) {
                                /* 本次启用或静态IP状态变化 -> 优先连接 STA3 */
                                iot_wifi_new_connect(WIFI_STA_INDEX3);
                            } else {
                                /* 无新增或关键变化，仅保存配置 */
                                ESP_LOGW(TAG, "No new STA enabled, only saving config, no connection operation");
                            }
                        } else {
                            // 当前WIFI连接成功，且修改使能位与当前STA无关，仅重新扫描，根据结果决定是否切换
                            iot_wifi_auto_connect();
                        }
                    } else {
                        // 修改了非使能标签的STA参数，无效修改，仅保存，不执行任何操作
                        ESP_LOGW(TAG, "Only non-enable STA parameters changed, only saving config, no connection operation");
                    }
                }
            }
            // 标记设置更新
            wifi_sta_update_flag_set(true);
        }
    }
    else if (1 == wifi_ap_cfg_update)
    {
        if (1 == IotSetData.dev_info_t.on_off.bit.wifi_enable) {
            // 重启AP
            if (1 == IotSetData.dev_info_t.on_off.bit.wifi_ap_enable) {
                iot_wifi_ap_config(IotSetData.dev_info_t.wifi_ap_auth, IotSetData.dev_info_t.wifi_AP_ssid, IotSetData.dev_info_t.wifi_AP_password);
            }
        }
    }   
    old_wifi_mul_sta_en.all = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_mul_sta_en.all;
    return (wifi_base_update_cfg | wifi_sta1_cfg_update | wifi_sta2_cfg_update | wifi_sta3_cfg_update | wifi_ap_cfg_update);
}


/*------------------------------------------------------------------------------
 Function: iot_lcd_mode_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      磁吸屏联动相关配置
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_lcd_mode_check(void)
{
    /* 断码屏设置检查 */
    if (reals.ModbusCmdFlag.sBit.LCD_Mode == 1)
    {
        reals.ModbusCmdFlag.sBit.LCD_Mode = 0;
        uint8_t lcd_mode_update = 0;

#ifdef CONFIG_BLE_ADV_ENABLE
        // 绑定命令
        if (top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.bind_cmd != 0) {
            ble_adv_trigger_update(0x85, top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.bind_cmd);
        }
#endif

        // 温度单位
        if (top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.temperature_unit != 0)
        {
            if (IotSetData.dev_info_t.lcd_mode.temperature_unit
                != top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.temperature_unit)
            {
                // APP设置记录
                if ( IotSetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag == 0 ) {
                    IotSetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag = 1;
                    reals.SetDataWrFlag.sBit.temp_unit_control = 1;
                }
                
                IotSetData.dev_info_t.lcd_mode.temperature_unit = top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.temperature_unit;
                ESP_LOGW(TAG, "LCD_Mode temperature_unit: %d", IotSetData.dev_info_t.lcd_mode.temperature_unit);
                lcd_mode_update |= 1;
            }
        }

        // 休眠时间
        if (top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.lcd_active_time != 0)
        {
            if (IotSetData.dev_info_t.lcd_mode.lcd_active_time
                != top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.lcd_active_time)
            {
                IotSetData.dev_info_t.lcd_mode.lcd_active_time = top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode.lcd_active_time;
                ESP_LOGW(TAG, "LCD_Mode lcd_active_time: %d", IotSetData.dev_info_t.lcd_mode.lcd_active_time);
                lcd_mode_update |= 1;
            }
        }
        
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.LCD_Mode.all = IotSetData.dev_info_t.lcd_mode.all;
        if(lcd_mode_update) reals.SetDataWrFlag.sBit.lcd_mode = 1;
    }

    /* 时间跨度设置检查 */
    if (reals.ModbusCmdFlag.sBit.Time_Span == 1)
    {
        reals.ModbusCmdFlag.sBit.Time_Span = 0;
        uint8_t Time_Span_update = 0;

        /*断电次数时间跨度*/
        if (top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span.gridpower_off != 0)
        {
            if (IotSetData.dev_info_t.Time_Span.power_off_count
                != top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span.gridpower_off)
            {
                IotSetData.dev_info_t.Time_Span.power_off_count = top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span.gridpower_off;
                ESP_LOGW(TAG, "Time_Span.gridpower_off: %d", IotSetData.dev_info_t.Time_Span.power_off_count);
                Time_Span_update |= 1;
            }
        }

        /*省钱参数时间跨度*/
        if (top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span.money_save != 0)
        {
            if (IotSetData.dev_info_t.Time_Span.save_money
                != top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span.money_save)
            {
                IotSetData.dev_info_t.Time_Span.save_money = top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span.money_save;
                ESP_LOGW(TAG, "Time_Span.money_save: %d", IotSetData.dev_info_t.Time_Span.save_money);
                Time_Span_update |= 1;
            }
        }
        
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Time_Span.all = IotSetData.dev_info_t.Time_Span.all;
        if(Time_Span_update) reals.SetDataWrFlag.sBit.time_span = 1;
    }
}

/*------------------------------------------------------------------------------
 Function: iot_ble_adv_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      蓝牙广播相关设置
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_ble_adv_check(void)
{
    if (reals.ModbusCmdFlag.sBit.ble_protocol == 1)
    {
        reals.ModbusCmdFlag.sBit.ble_protocol = 0;
        uint8_t ble_protocol_update = 0;
        
        /* server端广播数据使能 */
        if (top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.adv_en != 0)
        {
            if (IotSetData.dev_info_t.ble_protocol.adv_en
                != top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.adv_en)
            {
                IotSetData.dev_info_t.ble_protocol.adv_en = top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.adv_en;
                ESP_LOGW(TAG, "ble server adv_en: %d", IotSetData.dev_info_t.ble_protocol.adv_en);
                ble_protocol_update |= 1;
            }
        }

        /* 刷新公有广播秘钥 */
        if (top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.key_update != 0)
        {
            ble_adv_key_update(top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.key_update);
            top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.key_update = 0;
        }

        /* 通用广播使能（record type为0x00~0x7F） */
        if (top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.general_adv_en != 0)
        {
            if (IotSetData.dev_info_t.ble_protocol.general_adv_en
                != top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.general_adv_en)
            {
                IotSetData.dev_info_t.ble_protocol.general_adv_en = top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.general_adv_en;
                ESP_LOGW(TAG, "ble server general_adv_en: %d", IotSetData.dev_info_t.ble_protocol.general_adv_en);
                ble_protocol_update |= 1;
            }
        }

        /* LCD数据广播使能(record type为0x80/0x81) */
        if (top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.lcd_data_adv_en != 0)
        {
            if (IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en
                != top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.lcd_data_adv_en)
            {
                IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en = top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.lcd_data_adv_en;
                ESP_LOGW(TAG, "ble server lcd_data_adv_en: %d", IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en);
                ble_protocol_update |= 1;
            }
        }

        /* 广播modbus读写使能 */
        if (top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.modbus_adv_en != 0)
        {
            if (IotSetData.dev_info_t.ble_protocol.modbus_adv_en
                != top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.modbus_adv_en)
            {
                IotSetData.dev_info_t.ble_protocol.modbus_adv_en = top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.modbus_adv_en;
                ESP_LOGW(TAG, "ble server modbus_adv_en: %d", IotSetData.dev_info_t.ble_protocol.modbus_adv_en);
                ble_protocol_update |= 1;
            }
        }

        top_modbus_wr.Inv.mod_reg13600_open.ble_protocol.all = 0;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.ble_protocol = IotSetData.dev_info_t.ble_protocol;
        if(ble_protocol_update) reals.SetDataWrFlag.sBit.ble_protocol = 1;
    }

    //13600~13602 ble相关参数配置变化
    if (reals.ModbusCmdFlag.sBit.ble_client == 1)
    {
        reals.ModbusCmdFlag.sBit.ble_client = 0;
        uint8_t ble_client_update = 0;
        
        if (top_modbus_wr.Inv.mod_reg13600_open.blec_rssi_threshold < 0)
        {
            if (IotSetData.dev_info_t.blec_rssi_th != top_modbus_wr.Inv.mod_reg13600_open.blec_rssi_threshold)
            {
                IotSetData.dev_info_t.blec_rssi_th = top_modbus_wr.Inv.mod_reg13600_open.blec_rssi_threshold;
                ESP_LOGW(TAG, "ble client rssi threshold: %d", IotSetData.dev_info_t.blec_rssi_th);
                ble_client_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.blec_switch_interval > 0)
        {
            if (IotSetData.dev_info_t.blec_switch_int != top_modbus_wr.Inv.mod_reg13600_open.blec_switch_interval)
            {
                IotSetData.dev_info_t.blec_switch_int = top_modbus_wr.Inv.mod_reg13600_open.blec_switch_interval;
                ESP_LOGW(TAG, "ble client switch interval to: %u", IotSetData.dev_info_t.blec_switch_int);
                ble_client_update |= 1;
            }
        }

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.blec_rssi_threshold = IotSetData.dev_info_t.blec_rssi_th;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.blec_switch_interval = IotSetData.dev_info_t.blec_switch_int;
        if(ble_client_update) reals.SetDataWrFlag.sBit.ble_client = 1;
    }

    if (reals.ModbusCmdFlag.sBit.ble_server == 1)
    {
        reals.ModbusCmdFlag.sBit.ble_server = 0;

#if 0   // ble server 广播数据加密秘钥, 对外只读
        if (strlen((char *)top_modbus_wr.mod_reg13600_open.bles_adv_key))
        {
            if (strncmp((char *)IotSetData.dev_info_t.bles_adv_key,
                    (char *)top_modbus_wr.mod_reg13600_open.bles_adv_key,
                    sizeof(IotSetData.dev_info_t.bles_adv_key)))
            {
                memcpy(IotSetData.dev_info_t.bles_adv_key, top_modbus_wr.mod_reg13600_open.bles_adv_key,
                            sizeof(IotSetData.dev_info_t.bles_adv_key));
                ESP_LOGW(TAG, "ble server adv key change to: %s", IotSetData.dev_info_t.bles_adv_key);
                reals.IotSetDataWrFlag.sBit.ble_server_adv_key = 1;
            }
        }
#endif    

        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.bles_adv_key, IotSetData.dev_info_t.bles_adv_key,  sizeof(IotSetData.dev_info_t.bles_adv_key));
    }
}

/*------------------------------------------------------------------------------
 Function: iot_open_mqtt_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      开放第二MQTT设置
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_open_mqtt_check(void)
{
    uint8_t open_mqtt_update = 0;
    
    if (reals.ModbusCmdFlag.sBit.mqtt == 1)
    {
        reals.ModbusCmdFlag.sBit.mqtt = 0;

        if (top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.enable > 0)
        {
            if ( IotSetData.dev_info_t.open_mqtt_enable.enable != top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.enable )
            {
                IotSetData.dev_info_t.open_mqtt_enable.enable = top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.enable;
                reals.SetDataWrFlag.sBit.mqtt_en =1;
#ifdef CONFIG_OPEN_MQTT_ENABLE
                if (1 == IotSetData.dev_info_t.open_mqtt_enable.enable) {    
                    open_mqtt_start();
                } else {
                    set_open_mqtt_stop();
                }
#endif            
                open_mqtt_update |= 1;
            }
        }
        if (top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.crypt_en > 0)
        {
            if (IotSetData.dev_info_t.open_mqtt_enable.crypt_en != top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.crypt_en)
            {
                IotSetData.dev_info_t.open_mqtt_enable.crypt_en = top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.crypt_en;
                reals.SetDataWrFlag.sBit.mqtt_en =1;
                open_mqtt_update |= 1;
            }
        }
        if (top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.ctrl_en > 0)
        {
            if (IotSetData.dev_info_t.open_mqtt_enable.ctrl_en != top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.ctrl_en)
            {
                IotSetData.dev_info_t.open_mqtt_enable.ctrl_en = top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.ctrl_en;
                reals.SetDataWrFlag.sBit.mqtt_en =1;
                open_mqtt_update |= 1;
            }
        }
        if (top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.report_en > 0)
        {
            if (IotSetData.dev_info_t.open_mqtt_enable.report_en != top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.report_en)
            {
                IotSetData.dev_info_t.open_mqtt_enable.report_en = top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable.report_en;
                reals.SetDataWrFlag.sBit.mqtt_en =1;
                open_mqtt_update |= 1;
            }
        }

        if (top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_report_cycle > 0)
        {
            if (IotSetData.dev_info_t.open_mqtt_report_cycle != top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_report_cycle)
            {
                IotSetData.dev_info_t.open_mqtt_report_cycle = top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_report_cycle;
                ESP_LOGW(TAG, "open mqtt report cycle change to: %u", IotSetData.dev_info_t.open_mqtt_report_cycle);
                reals.SetDataWrFlag.sBit.mqtt_report_cycle =1;
                open_mqtt_update |= 1;
            }
        }

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.open_mqtt_enable.all = IotSetData.dev_info_t.open_mqtt_enable.all;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.open_mqtt_report_cycle = IotSetData.dev_info_t.open_mqtt_report_cycle;
    }

    /*第二服务器网址发生改变*/
	if (reals.ModbusCmdFlag.sBit.Net_Server_address == 1)
	{
		reals.ModbusCmdFlag.sBit.Net_Server_address = 0;
		if (strlen(top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_address)) 
		{
			if (strncmp(IotSetData.dev_info_t.Net_Server_address,top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_address, sizeof(top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_address))) 
			{
				memcpy(IotSetData.dev_info_t.Net_Server_address, top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_address, sizeof(top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_address));
				ESP_LOGW(TAG, "Net_Server_address: %s", IotSetData.dev_info_t.Net_Server_address);
				reals.SetDataWrFlag.sBit.Net_Server_address =1;
#ifdef CONFIG_OPEN_MQTT_ENABLE                
                open_mqtt_new_host(IotSetData.dev_info_t.Net_Server_address);
#endif
                open_mqtt_update |= 1;
			}
		}

        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg22000_net_server_2rd.Net_Server_address, IotSetData.dev_info_t.Net_Server_address,  sizeof(IotSetData.dev_info_t.Net_Server_address));
	}

    /*第二服务器密码发生改变*/
	if (reals.ModbusCmdFlag.sBit.Net_Server_secret == 1)
	{
		reals.ModbusCmdFlag.sBit.Net_Server_secret = 0;
		if (strlen(top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_secret)) 
		{
			if (strncmp(IotSetData.dev_info_t.Net_Server_secret,top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_secret, sizeof(top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_secret))) 
			{
				memcpy(IotSetData.dev_info_t.Net_Server_secret, top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_secret, sizeof(top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_secret));
				ESP_LOGW(TAG, "Net_Server_secret: %s", top_modbus_wr.Inv.mod_reg22000_net_server_2rd.Net_Server_secret);
				reals.SetDataWrFlag.sBit.Net_Server_secret =1;
#ifdef CONFIG_OPEN_MQTT_ENABLE                
                open_mqtt_new_password(IotSetData.dev_info_t.Net_Server_secret);
#endif                
                open_mqtt_update |= 1;
			}
		}

        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg22000_net_server_2rd.Net_Server_secret, IotSetData.dev_info_t.Net_Server_secret,  sizeof(IotSetData.dev_info_t.Net_Server_secret));
	}	

    return open_mqtt_update;
}

/*------------------------------------------------------------------------------
 Function: iot_modbus_tcp_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      modbus tcp设置
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_modbus_tcp_check(void)
{
    uint8_t modbus_tcp_update = 0;

    if (reals.ModbusCmdFlag.sBit.modbus_tcp == 1)
    {
        reals.ModbusCmdFlag.sBit.modbus_tcp = 0;

        if (top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable.enable > 0)
        {
            if (IotSetData.dev_info_t.modbus_tcp_enable.enable != top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable.enable)
            {
                IotSetData.dev_info_t.modbus_tcp_enable.enable = top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable.enable;
                ESP_LOGW(TAG, "modbus tcp enable change to: %u", IotSetData.dev_info_t.modbus_tcp_enable.enable);
                reals.SetDataWrFlag.sBit.modbus_tcp_en =1;
                modbus_tcp_update |= 1;
            }
        }

#ifdef CONFIG_MODBUS_TCP_ENCRYPT_ENABLE
        if (top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable.crypt_en > 0)
        {
            if (IotSetData.dev_info_t.modbus_tcp_enable.crypt_en != top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable.crypt_en)
            {
                IotSetData.dev_info_t.modbus_tcp_enable.crypt_en = top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable.crypt_en;
                ESP_LOGW(TAG, "modbus tcp crypt_en change to: %u", IotSetData.dev_info_t.modbus_tcp_enable.crypt_en);
                reals.SetDataWrFlag.sBit.modbus_tcp_en =1;
                modbus_tcp_update |= 1;
            }
        }
#else
        IotSetData.dev_info_t.modbus_tcp_enable.crypt_en = 2;
#endif

        if (top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_port > 0)
        {
            if (IotSetData.dev_info_t.modbus_tcp_port != top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_port)
            {
                IotSetData.dev_info_t.modbus_tcp_port = top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_port;
                ESP_LOGW(TAG, "modbus tcp port change to: %u", IotSetData.dev_info_t.modbus_tcp_port);
                reals.SetDataWrFlag.sBit.modbus_tcp_port =1;
                modbus_tcp_update |= 1;
            }
        }

        if (reals.SetDataWrFlag.sBit.modbus_tcp_en == 1
            || reals.SetDataWrFlag.sBit.modbus_tcp_port == 1)
        {
            if (1 == IotSetData.dev_info_t.modbus_tcp_enable.enable)
            {
                if (true == modbus_tcp_is_running())
                {
                    modbus_tcp_restart(IotSetData.dev_info_t.modbus_tcp_enable.crypt_en, IotSetData.dev_info_t.modbus_tcp_port, 0);
                }
                else
                {
                    modbus_tcp_start(IotSetData.dev_info_t.modbus_tcp_enable.crypt_en, IotSetData.dev_info_t.modbus_tcp_port, 0);
                }
            }
            else
            {
                modbus_tcp_stop();
            }
        }

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.modbus_tcp_enable.all = IotSetData.dev_info_t.modbus_tcp_enable.all;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.modbus_tcp_port = IotSetData.dev_info_t.modbus_tcp_port;
    }

    return modbus_tcp_update;
}

/*------------------------------------------------------------------------------
 Function: support_mode_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT support模式检查
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void support_mode_check(void)
{
    uint8_t support_mode_update = 0;

    // 访客模式检查
    if ( 0 != top_modbus_wr.Inv.mod_reg00000.support_mode.bit.visitor_mode )
    {
        if (IotSetData.dev_info_t.support_mode.bit.visitor_mode 
            != top_modbus_wr.Inv.mod_reg00000.support_mode.bit.visitor_mode)
        {
            ESP_LOGW(TAG, "change support_mode, old:%d --->>> new: %d",
                IotSetData.dev_info_t.support_mode.bit.visitor_mode, top_modbus_wr.Inv.mod_reg00000.support_mode.bit.visitor_mode);
            IotSetData.dev_info_t.support_mode.bit.visitor_mode = top_modbus_wr.Inv.mod_reg00000.support_mode.bit.visitor_mode;
            support_mode_update |= 1;
        }
    }

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.support_mode.bit.support_ble_pwd = IotSetData.dev_info_t.support_mode.bit.support_ble_pwd; //蓝牙密码设置区
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.support_mode.bit.visitor_mode = IotSetData.dev_info_t.support_mode.bit.visitor_mode; //访客模式设置
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.support_mode.bit.ble_server_encrypt = IotSetData.dev_info_t.support_mode.bit.ble_server_encrypt; //蓝牙加密模式设置

    if(support_mode_update) reals.SetDataWrFlag.sBit.support_mode = 1;
}

/**
 * @brief 检查并执行电源控制指令
 *
 * 根据modbus写寄存器中的电源控制指令(ctrl_poweron)执行对应的EMS控制操作。
 * 支持关机、开机、休眠、唤醒等多种控制场景。
 */
static void iot_power_ctrl_check(void)
{
    uint8_t ctrl_poweron = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_poweron;

    switch (ctrl_poweron)
    {
        case 1: // 关机（自杀全断电）
            Iot_EMS_Ctrl_Func_5_Send(2, 0, 0);
            break;

        case 2: // 20250630前提测试项目使用：休眠关/开机
            if (reals.IOT_Status_Flag.sBit.system_sleep_flag == 1) {
#ifdef SYSTEM_WAKEUP_BY_GPIO_ENABLE
                reals.IOT_Status_Flag.sBit.system_wakeup_flag = 1; // 通过GPIO唤醒
#else
                Iot_EMS_Ctrl_Func_5_Send(1, 0, 0); // 开机
#endif
            } else {
                Iot_EMS_Ctrl_Func_5_Send(3, 0, 0); // 进入休眠
            }
            break;

        case 3: // 系统开机/唤醒/恢复工作
#ifdef SYSTEM_WAKEUP_BY_GPIO_ENABLE
            reals.IOT_Status_Flag.sBit.system_wakeup_flag = 1; // 通过GPIO唤醒
#else
            Iot_EMS_Ctrl_Func_5_Send(1, 0, 0); // 开机
#endif
            break;

        case 4: // 进入休眠模式（只保持MCU在线）
            Iot_EMS_Ctrl_Func_5_Send(3, 0, 0);
            break;

        case 5: // 停止工作（EPO急停/远程开关），切断所有负载和输入
            // 暂不处理
            break;

        default:
            break;
    }

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.ctrl_poweron = ctrl_poweron;
}

/**
 * @brief 检查远程控制参数是否发生变化并同步更新
 *
 * 对比当前设备的远程休眠设置和SOC阈值与Modbus写寄存器中的值，
 * 如有变化则同步更新本地参数，并触发EMS控制指令。
 */
static void Remote_Ctrl_Check(void)
{
    uint8_t update_flag = 0;

#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5   
    // 休眠开关设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Remote_set_enable ) {
        if (IotSetData.dev_info_t.Remote_set.Remote_set_enable != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Remote_set_enable) {
            ESP_LOGW(TAG, "Remote_set_enable change, old:%d --->>> new: %d",
                IotSetData.dev_info_t.Remote_set.Remote_set_enable, top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Remote_set_enable);
            IotSetData.dev_info_t.Remote_set.Remote_set_enable = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Remote_set_enable;
            update_flag |= 1;
        }
    }

    // 休眠支持设置（暂不允许设置）
    IotSetData.dev_info_t.Remote_set.Remote_set_valid = 1;

    // 自动休眠天数设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Automatic_sleep_days ) {
        if (IotSetData.dev_info_t.Remote_set.Automatic_sleep_days != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Automatic_sleep_days) {
            ESP_LOGW(TAG, "Automatic_sleep_days change, old:%d --->>> new: %d",
                IotSetData.dev_info_t.Remote_set.Automatic_sleep_days, top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Automatic_sleep_days);
            IotSetData.dev_info_t.Remote_set.Automatic_sleep_days = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set.Automatic_sleep_days;
            update_flag |= 1;
        }
    }

    // 休眠SOC阈值
    if (1) { // 允许设置为0  ( 0 != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set_soc_limit ) {
        if (IotSetData.dev_info_t.Remote_set_soc_limit != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set_soc_limit) {
            ESP_LOGW(TAG, "Remote_set_soc_limit change, old:%d --->>> new: %d",
                IotSetData.dev_info_t.Remote_set_soc_limit, top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set_soc_limit);
            IotSetData.dev_info_t.Remote_set_soc_limit = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set_soc_limit;
            update_flag |= 1;
        }
    }

    // 如果有参数更新，则设置写标志并下发EMS控制指令
    if(update_flag) {
        reals.SetDataWrFlag.sBit.remote = 1;
        Iot_EMS_Ctrl_Func_5_Send(0, 0, 0);
    }

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.Remote_set.all = IotSetData.dev_info_t.Remote_set.all;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.Remote_set_soc_limit = IotSetData.dev_info_t.Remote_set_soc_limit;
#endif    
}

/**
 * @brief 检查LED参数是否发生变化并同步更新
 *
 * 对比当前设备的LED设置与Modbus写寄存器中的值，
 * 如有变化则同步更新本地参数，并触发EMS控制指令。
 */
static void Led_Ctrl_Check(void)
{
#ifdef CONFIG_IOT_LED_SET_ENABLE

    uint8_t update_flag = 0;

    // LED颜色设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.color_set ) {
        if (IotSetData.dev_info_t.LED_SET.bit.color_set != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.color_set) {
            ESP_LOGW(TAG, "color_set change, old:%d --->>> new: %d",
                IotSetData.dev_info_t.LED_SET.bit.color_set, top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.color_set);
            IotSetData.dev_info_t.LED_SET.bit.color_set = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.color_set;
            update_flag |= 1;
        }
    }

    // LED亮度设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.brightness_set ) {
        if (IotSetData.dev_info_t.LED_SET.bit.brightness_set != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.brightness_set) {
            ESP_LOGW(TAG, "brightness_set change, old:%d --->>> new: %d",
                IotSetData.dev_info_t.LED_SET.bit.brightness_set, top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.brightness_set);
            IotSetData.dev_info_t.LED_SET.bit.brightness_set = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET.bit.brightness_set;
            update_flag |= 1;
        }
    }

    // 如果有参数更新，则设置写标志并下发EMS控制指令
    if(update_flag) {
        reals.SetDataWrFlag.sBit.led_set = 1;
        Iot_EMS_Ctrl_Func_5_Send(0, 0, 0);
    }

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.LED_SET.all = IotSetData.dev_info_t.LED_SET.all;
#endif    
}

/**
 * @brief 检查发电机参数是否发生变化并同步更新
 *
 * 对比当前设备的发电机设置与Modbus写寄存器中的值，
 * 如有变化则同步更新本地参数
 */
static void Gen_Ctrl_Check(void)
{
#ifdef CONFIG_SMART_GENERATOR_ENABLE

    uint8_t update_flag = 0;

    // 发电机智能控制设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.ctrl_mode ) {
        if (IotSetData.dev_info_t.Gen_ctrl.ctrl_mode != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.ctrl_mode) {
            ESP_LOGW(TAG, "Gen_ctrl.ctrl_mode change, old:%d --->>> new: %d",
                IotSetData.dev_info_t.Gen_ctrl.ctrl_mode, top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.ctrl_mode);
            IotSetData.dev_info_t.Gen_ctrl.ctrl_mode = top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.ctrl_mode;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.GenSet.ctrl_mode = IotSetData.dev_info_t.Gen_ctrl.ctrl_mode;
            update_flag |= 1;
        }
    }

    // 发电机禁用时间段设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl ) {
        if (IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl) {
            ESP_LOGW(TAG, "Gen_ctrl.Unuse_time_ctrl change, old:%d --->>> new: %d",
                IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl, top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl);
            IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl = top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl = IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl;
            update_flag |= 1;
        }
    }

    // 发电机智能控制SOC设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Start ) {
        if (IotSetData.dev_info_t.Gen_ctrl.soc_min != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Start) {
            ESP_LOGW(TAG, "Gen_SOC_Start, old:%d --->>> new: %d",
                IotSetData.dev_info_t.Gen_ctrl.soc_min, top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Start);
            IotSetData.dev_info_t.Gen_ctrl.soc_min = top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Start;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.Gen_SOC_Start = IotSetData.dev_info_t.Gen_ctrl.soc_min;
            update_flag |= 1;
        }
    }

    // 发电机智能控制SOC设置
    if ( 0 != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Stop ) {
        if (IotSetData.dev_info_t.Gen_ctrl.soc_max != top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Stop) {
            ESP_LOGW(TAG, "Gen_SOC_Stop, old:%d --->>> new: %d",
                IotSetData.dev_info_t.Gen_ctrl.soc_max, top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Stop);
            IotSetData.dev_info_t.Gen_ctrl.soc_max = top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Stop;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.Gen_SOC_Stop = IotSetData.dev_info_t.Gen_ctrl.soc_max;
            update_flag |= 1;
        }
    }

    // 如果有参数更新
    if(update_flag) {
        reals.SetDataWrFlag.sBit.gen_set = 1;
    }

#endif    
}

/**
 * @brief 检查参数是否发生变化并同步更新
 *
 * 对比当前设备设置与Modbus写寄存器中的值，
 * 如有变化则同步更新本地参数。
 */
static void IoT_Protocol_Mix_Check(void)
{
    uint8_t update_flag = 0;

    // 北向协议设置
    if (IotSetData.dev_info_t.Protocol_3r_Enable_mix1.all != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.all) {
        // MQTT1服务器主动登录上报使能
        if ( 0 != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_enable ) {
            if(IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_enable) {
                ESP_LOGW(TAG, "mqtt1_enable change, old:%d --->>> new: %d",
                    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable, top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_enable);
                IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable = top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_enable;
                update_flag |= 1;
            }
        }
        
        // MQTT1加密使能
        if ( 0 != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable ) {
            if(IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable) {
                ESP_LOGW(TAG, "mqtt1_encrypt_enable change, old:%d --->>> new: %d",
                    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable, top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable);
                IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable = top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable;
                update_flag |= 1;
                
                if(1 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable) iot_mqtt_start();
            }
        }
        
        // 同步时间使能（iot不主动通过BLUETTI服务器网络获取时间）
        if ( 0 != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.bluetti_time_update ) {
            if(IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.bluetti_time_update) {
                ESP_LOGW(TAG, "bluetti_time_update change, old:%d --->>> new: %d",
                    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update, top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.bluetti_time_update);
                IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update = top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.bluetti_time_update;
                update_flag |= 1;
            }
        }
    }

    // 南向协议设置
    if (IotSetData.dev_info_t.Protocol_3r_Enable_mix2.all != top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix2.all) {
        // 预留
    }

    // 如果有参数更新，则设置写标志
    if(update_flag) {
        reals.SetDataWrFlag.sBit.protocol_enable = 1;
    }

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.all = IotSetData.dev_info_t.Protocol_3r_Enable_mix1.all; 
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Protocol_3r_Enable_mix2.all = IotSetData.dev_info_t.Protocol_3r_Enable_mix2.all; 
}

/*------------------------------------------------------------------------------
 Function: iot_config_set_default
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT参数初始化
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_config_set_default(void)
{
    // 断电次数/省钱参数时间跨度
    IotSetData.dev_info_t.Time_Span.power_off_count = 3; // 默认每月
    IotSetData.dev_info_t.Time_Span.save_money = 4;      // 默认每年
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Time_Span.all = IotSetData.dev_info_t.Time_Span.all;

    // 断电次数清零
    memset(&IotSetData.dev_info_t.grid_off_statistics, 0, sizeof(IotSetData.dev_info_t.grid_off_statistics)); 

    // 清除磁吸屏相关设置
    IotSetData.dev_info_t.temp_unit_control.app_temp_unit_set_flag = 0;
    IotSetData.dev_info_t.lcd_mode.temperature_unit = 2; // 默认温度单位为F
    IotSetData.dev_info_t.lcd_mode.lcd_active_time = 6;  // 与逆变器一致
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.LCD_Mode.all = IotSetData.dev_info_t.lcd_mode.all; // 断码屏模式设置

    // 清除连接记录缓存
    memset(&IotSetData.dev_info_t.old_mqtt_connect_data, 0, sizeof(IotSetData.dev_info_t.old_mqtt_connect_data));

    // 恢复出厂默认key
    ble_adv_key_update(2); 

#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5
    IotSetData.dev_info_t.Remote_set.Remote_set_enable = 2;     // 默认禁止
    IotSetData.dev_info_t.Remote_set.Remote_set_valid = 1;      // 系统支持
    IotSetData.dev_info_t.Remote_set.Automatic_sleep_days = 0;  // 自动休眠预留
    IotSetData.dev_info_t.Remote_set_soc_limit = 20;             // 休眠阈值SOC默认为20
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.Remote_set.all = IotSetData.dev_info_t.Remote_set.all;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.Remote_set_soc_limit = IotSetData.dev_info_t.Remote_set_soc_limit;
#endif

#ifdef CONFIG_IOT_LED_SET_ENABLE
    // IOT端LED控制
    IotSetData.dev_info_t.LED_SET.bit.color_set = 0;            // 1:暖色；2：冷色；(0无效)          
    IotSetData.dev_info_t.LED_SET.bit.brightness_set = 101;     // LED亮度 （0无效 1~101表示0-100） 
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.LED_SET.all = IotSetData.dev_info_t.LED_SET.all;
#endif

#ifdef CONFIG_SMART_GENERATOR_ENABLE
    // 油机智能控制
    IotSetData.dev_info_t.Gen_ctrl.soc_min = 20;
    IotSetData.dev_info_t.Gen_ctrl.soc_max = 80;
    IotSetData.dev_info_t.Gen_ctrl.ctrl_mode = 3;   // 0：无效；1：智能混动；2：定时；3：手动
    IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl = 2;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.GenSet.ctrl_mode = IotSetData.dev_info_t.Gen_ctrl.ctrl_mode;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl = IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.Gen_SOC_Start = IotSetData.dev_info_t.Gen_ctrl.soc_min;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.Gen_SOC_Stop = IotSetData.dev_info_t.Gen_ctrl.soc_max;
#endif

    // MQTT1配置
    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable = 1;         // MQTT1默认使能
    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable = 2; // MQTT1加密默认禁止
    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update = 1;  // 同步服务器时间默认使能
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.all = IotSetData.dev_info_t.Protocol_3r_Enable_mix1.all; 

    // 灾害预警设置
    IotSetData.dev_info_t.thunder_ctrl.thunder_enable = 2;//风暴预警默认不使能
    memcpy(&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.all, &IotSetData.dev_info_t.thunder_ctrl.all,  sizeof(IotSetData.dev_info_t.thunder_ctrl.all));

    // 恢复出厂设置时，能量和历史记录清零
    top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.bit.clear_all = 1; 
    iot_clean_flag_check();

#ifdef CONFIG_RELAY_CTRL_ENABLE
    // 清除relay参数文件
    clear_relay_parameter_file(); 
#endif

#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
    // 恢复 Smart TOU 配置为出厂默认
    Smart_Tou_Restore_Factory_Defaults();
#endif

    // 下发至BMS
    Iot_EMS_Ctrl_Func_5_Send(0, 1, 1);

    reals.SetDataWrFlag.sBit.SetDataUpdate_all = 1;   
}

/*------------------------------------------------------------------------------
 Function: iot_factory_cmd_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT出厂命令处理
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_factory_cmd_process(void)
{
    // 检查命令触发标志
    if ( reals.ModbusCmdFlag.sBit.iot_factory_cmd )
    {
        reals.ModbusCmdFlag.sBit.iot_factory_cmd = 0;
        ESP_LOGW(TAG, "iot_factory_cmd_process : CMD(0x%x)", top_modbus_wr.Inv.mod_reg12000_IOT_set.factory);
            
        switch ( top_modbus_wr.Inv.mod_reg12000_IOT_set.factory )
        {
            case 0xAA01 :
                // 0xAA01：恢复出厂设置
                iot_config_set_default();
                break;
                
            case 0xAA02 :
                // 0xAA02：清除能量记录
#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
                // 恢复能量文件到出厂默认并初始化文件头并写入存储
                Energy_File_Restore_Factory_Defaults();
#endif
                break;
                
            case 0xAA03 :
                // 0xAA03：清除历史故障记录
#ifndef MODBUS_GET_HISTORY_FROM_ARM_ENABLE
                /*当使用ARM历史故障时，IOT历史故障作为备份，不可清除*/
                clear_log_file();
#endif
                break;
                
            default:
                top_modbus_wr.Inv.mod_reg12000_IOT_set.factory = 0;
        }

        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.factory = top_modbus_wr.Inv.mod_reg12000_IOT_set.factory;
    }
}

/*------------------------------------------------------------------------------
 Function: iot_cfg_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      设置项检查
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_cfg_check (void)
{
    uint8_t webserver_cfg_update = 0;

    // WIFI相关设置检查
    webserver_cfg_update |= iot_wifi_all_cfg_check();

    // 磁吸屏相关设置
    iot_lcd_mode_check();

    // 蓝牙广播相关设置
    iot_ble_adv_check();

    // MQTT第二服务器相关设置
    webserver_cfg_update |= iot_open_mqtt_check();

    // modbus tcp 相关设置
    webserver_cfg_update |= iot_modbus_tcp_check();
    
    /* iot on-off开关检查 */
    if ( 1 == reals.ModbusCmdFlag.sBit.on_off )
    {
        reals.ModbusCmdFlag.sBit.on_off = 0;
        if (iot_on_off_check()) {
            webserver_cfg_update |= 1;
        }
        reals.iot_link_change = 1;
        reals.iot_smart_change = 1;
    }

    /* 风暴模式开关检查 */
    if (reals.ModbusCmdFlag.sBit.thunder_ctrl == 1)
    {
        reals.ModbusCmdFlag.sBit.thunder_ctrl = 0;
        uint8_t thunder_changed;
        if ((thunder_changed = thunder_ctrl_check()) != 0) {
            reals.SetDataWrFlag.sBit.thunder_ctrl = 1;
        }
    }

    /*蓝牙密码发生改变*/
    if (1 == reals.ModbusCmdFlag.sBit.app_password)
    {
        reals.ModbusCmdFlag.sBit.app_password = 0;
        webserver_cfg_update |= ble_password_check();
    }

    /*访客模式发生改变*/
    if (1 == reals.ModbusCmdFlag.sBit.support_mode)
    {
        reals.ModbusCmdFlag.sBit.support_mode = 0;
        support_mode_check();
    }

#ifdef CONFIG_WEBSERVER_ENABLE
    // 配置发生改变，主动通知web更新页面
    if (webserver_cfg_update) {
        web_server_config_push();
    }
#endif

    /*恢复出厂标志发生改变*/
    if ( 1 == reals.ModbusCmdFlag.sBit.reset_factory )
    {
        reals.ModbusCmdFlag.sBit.reset_factory = 0;

        // IOT参数初始化
        iot_config_set_default();
    }

    /*清除标志发生改变*/
    if ( 1 == reals.ModbusCmdFlag.sBit.ctrl_2006 )
    {
        reals.ModbusCmdFlag.sBit.ctrl_2006 = 0;
        if ( 0 != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl.all )
        {
            // 能量和历史记录清零
            iot_clean_flag_check();

            // 下发至BMS
            Iot_EMS_Ctrl_Func_5_Send(0, 0, 1);
        }
    }

    /*电池维护设置*/
	if (reals.ModbusCmdFlag.sBit.bat_maintain_set == 1) {
		reals.ModbusCmdFlag.sBit.bat_maintain_set = 0;
		Iot_set_bat_maintain_once(top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.bat_maintain.enable);
	}

    /*检查并执行电源控制指令*/
    if ( 1 == reals.ModbusCmdFlag.sBit.ctrl_power ) {
        reals.ModbusCmdFlag.sBit.ctrl_power = 0;
        iot_power_ctrl_check();
    }

    /*检查远程控制参数*/
    if ( 1 == reals.ModbusCmdFlag.sBit.ctrl_remote ) {
        reals.ModbusCmdFlag.sBit.ctrl_remote = 0;
        Remote_Ctrl_Check();
    }    

    /*检查LED参数*/
    if ( 1 == reals.ModbusCmdFlag.sBit.ctrl_led ) {
        reals.ModbusCmdFlag.sBit.ctrl_led = 0;
        Led_Ctrl_Check();
    }    

    /*检查Gen配置参数*/
    if ( 1 == reals.ModbusCmdFlag.sBit.gen_set ) {
        reals.ModbusCmdFlag.sBit.gen_set = 0;
        Gen_Ctrl_Check();
    }   
    
    /*协议使能设置*/
    if (( 1 == reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 ) 
        || ( 1 == reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 )){
        reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 = 0;
        reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 = 0;
        IoT_Protocol_Mix_Check();
    }
        
    /*DCDC设置更新*/
    if ( 1 == reals.ModbusCmdFlag.sBit.dcdc_set1 ) {
        reals.ModbusCmdFlag.sBit.dcdc_set1 = 0;
#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
        Iot_Dcdc_Ctrl_frame_Send(true);
#endif
    }  

    /*IOT出厂命令处理*/
    iot_factory_cmd_process();
}

/*------------------------------------------------------------------------------
 Function: iot_wifi_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      更新WIFI相关信息
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_wifi_update(void)
{
    const wifi_info_t *wifi_info = iot_get_wifi_info();

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_rssi = (int16_t)(iot_wifi_ap_rssi());

    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4, wifi_info->sta_ipv4, sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4)); // ip
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_gw,   wifi_info->sta_gw,   sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_gw));   // gateway
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_mask, wifi_info->sta_mask, sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_mask)); // mask
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_mac,  wifi_info->sta_mac,  sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_mac));  // mac
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_dns1,  wifi_info->sta_dns1,  sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_dns1));  // dns1
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_dns2,  wifi_info->sta_dns2,  sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_dns2));  // dns2

#ifdef CONFIG_MORE_WIFI_STA_LINK_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.wifi_mult_sta_flag.bit.ext_wifi_enable_num = 2; // 额外支持STA2和STA3
    if ( wifi_info->flag.bit.sta_link_ap ) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.wifi_mult_sta_flag.bit.curr_sta_index = wifi_info->sta_index;
        memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.wifi_station_ssid_now,  wifi_info->sta_ssid,  sizeof(wifi_info->sta_ssid));  // 当前连接WIFI名称
    } else {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.wifi_mult_sta_flag.bit.curr_sta_index = 0;
        memset(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.wifi_station_ssid_now,  0,  sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.wifi_station_ssid_now));
    }
#endif

#ifdef CONFIG_WIFI_STA_UNDHCP_ENABLE
    /*支持WIFI静态IP相关设置*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.sta_enable.sta_undhcp_enable = 1;
#endif

    /*支持WIFI无密码模式*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.sta_enable.sta_authmode_open = 1;
}

/*------------------------------------------------------------------------------
 Function: iot_4g_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      更新4g相关信息
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_4g_update(void)
{
    MOD_STRUCT_reg11000 *Md_data = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info;
    const usb_4g_info_t *_4g_info = iot_4g_info_get();

    memcpy(Md_data->IMEI, _4g_info->IMEI, sizeof(Md_data->IMEI));
    memcpy(Md_data->IMSI, _4g_info->IMSI, sizeof(Md_data->IMSI));
    memcpy(Md_data->ICCID, _4g_info->ICCID, sizeof(Md_data->ICCID));

#ifdef CONFIG_CAN_FACTORY_WRITE_ENABLE
    // CAN标定4G相关信息更新
    Can_Product_4G_Data_Update((char *)_4g_info->IMEI,
                                (char *)_4g_info->IMSI,
                                (char *)_4g_info->ICCID);
#endif

    memcpy(Md_data->IP_4G, _4g_info->ipv4, sizeof(Md_data->IP_4G));

    Md_data->signal_4G = _4g_info->rssi;
    Md_data->link.bit._4g = _4g_info->net_status ? 1 : 0;
    Md_data->SIM_ready = _4g_info->sim_card_status ? 1 : 0;
    Md_data->GPS_X = _4g_info->GPS_X;
    Md_data->GPS_Y = _4g_info->GPS_Y;
    Md_data->link.bit.gps = (_4g_info->GPS_X && _4g_info->GPS_Y) ? 1 : 0;
    Md_data->SatellitesNum.gps_num = _4g_info->gps_num;
    Md_data->SatellitesNum.glonass_num = _4g_info->gl_num;
    Md_data->SatellitesNum.galileo_num = _4g_info->gal_num;
    Md_data->SatellitesNum.beidou_num = _4g_info->bd_num;
}

/*------------------------------------------------------------------------------
 Function: iot_softver_info_by_29800_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      29800版本信息更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_softver_info_by_29800_update(void)
{
    /*更新IOT软件编号列表*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg29800_softver_info.softver[0].SW_Code_Ver = IOT_VERSION;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg29800_softver_info.softver[0].Ctrl_Code = Get_IoT_Code_Build_U32();

    /*计算总版本号数量*/
    uint8_t softver_num = 1 + top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base.software_total + top_modbus_rd.Pack[reals.Addr_can_self].mod_reg06100_Pack_each.software_total;
    
    /*更新软件编号列表*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg29800_softver_info.softver_num = (softver_num > SOFTVER_29800_NUM_MAX) ? SOFTVER_29800_NUM_MAX : softver_num;

    uint8_t index = 1;

    /*更新逆变版本号*/
    for ( uint8_t i = 0 ; i < top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base.software_total ; i++ ) {
        if(index < SOFTVER_29800_NUM_MAX) {
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg29800_softver_info.softver[index++].SW_Code_Ver = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base.soft[i].version;
        } else {
            break;
        }
    }

    /*更新PACK版本号*/
    for ( uint8_t i = 0 ; i < top_modbus_rd.Pack[reals.Addr_can_self].mod_reg06100_Pack_each.software_total ; i++ ) {
        if(index < SOFTVER_29800_NUM_MAX) {
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg29800_softver_info.softver[index++].SW_Code_Ver = top_modbus_rd.Pack[reals.Addr_can_self].mod_reg06100_Pack_each.soft[i].version;
        } else {
            break;
        }
    }
}

/**
  * @brief      更新IOT信息至modbus寄存器
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_about_update(void) 
{    
    // 蓝牙信息更新
    iot_ble_update();

    // WIFI信息更新
    iot_wifi_update();

#ifdef CONFIG_USB_4G_MODULE_ENABLE
    // 4G信息更新
    iot_4g_update();
#endif

    /*1149/2218->1 机器标准*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.Machine_standard.voltage = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base.DevVoltageLable;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.Machine_standard.country = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.ctrl_user_area;

    /*2241->4 EMS运行模式*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.ems_ctrl.all = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.ems_ctrl.all;

#ifdef CONFIG_GRID_OFF_COUNT_ENABLE 
    // 断电次数，根据Time_Span设置显示对应时间段的统计
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.Power_Off_Nums = Get_Grid_Off_Count_By_Timespan();
#endif

#ifdef CONFIG_FILE_DATA_OTA_ENABLE  
    // 获取文件资源版本号
    static bool file_update_status = false;
    if (( 0 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver2 )
        || ( file_update_status != Get_File_OTA_Status() ))
    {
        if (!Get_File_OTA_Status()) {
            // top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver2 = Get_File_Data_Version();
        }

        file_update_status = Get_File_OTA_Status();
    }
#endif        

    // 29800版本信息更新
    iot_softver_info_by_29800_update();
}

/* 计算内部堆碎片率，返回 0-100 的百分比（四舍五入） */
static uint8_t calc_internal_heap_frag_pct(void)
{
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (internal_free == 0) {
        return 0;
    }
    size_t wasted = (internal_free > internal_largest) ? (internal_free - internal_largest) : 0;
    uint32_t pct = (uint32_t)(((uint64_t)wasted * 100 + internal_free / 2) / internal_free); /* 四舍五入 */
    return (pct > 100) ? 100 : (uint8_t)pct;
}

/*------------------------------------------------------------------------------
 Function: iot_test_index_0xB0_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT测试信息更新(index = 0xB0)
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void iot_test_index_0xB0_update(void)
{
    MOD_STRUCT_reg30900 *Md_data = &top_modbus_rd.Self_IoT.mod_reg30900_IoT_Test;
    const usb_4g_info_t *_4g_info = iot_4g_info_get();

    memcpy(Md_data->IMEI, _4g_info->IMEI, sizeof(Md_data->IMEI));
    memcpy(Md_data->IMSI, _4g_info->IMSI, sizeof(Md_data->IMSI));
    memcpy(Md_data->ICCID, _4g_info->ICCID, sizeof(Md_data->ICCID));    
    memset(Md_data->ip, 0, sizeof(Md_data->ip));
    snprintf(Md_data->ip, sizeof(Md_data->ip), "%u.%u.%u.%u", 
        _4g_info->ipv4[0], _4g_info->ipv4[1], _4g_info->ipv4[2], _4g_info->ipv4[3]);
    
    Md_data->rssi = (_4g_info->rssi == 99) ? 0 : ((int16_t)-113 + (int16_t)_4g_info->rssi * 2);
    Md_data->net_status = _4g_info->net_status ? 1 : 0;
    Md_data->sim_card_status = _4g_info->sim_card_status ? 1 : 0;
    Md_data->GPS_X = _4g_info->GPS_X;
    Md_data->GPS_Y = _4g_info->GPS_Y;
    Md_data->gps_num = _4g_info->gps_num;
    Md_data->gl_num = _4g_info->gl_num;
    Md_data->gal_num = _4g_info->gal_num;
    Md_data->bd_num = _4g_info->bd_num;
    Md_data->fix = (_4g_info->GPS_X && _4g_info->GPS_Y) ? 1 : 0;
}   

/*------------------------------------------------------------------------------
 Function: iot_test_period_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT测试模式
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_test_period_process(void)
{
    switch ( top_modbus_rd.Self_IoT.mod_reg30900_IoT_Test.test_work_mode )
    {
        case 0xB0 :
            // 4G测试模式
            iot_test_index_0xB0_update();
            break;

        case 0xA0 :
            // 单板测试模式
            Iot_Heartbeat_frame_Send(true); // 快速发送心跳，便于总线识别IOT
            break;
            
        default:
            break;
    }
}

/**
  * @brief      IOT测试信息标签检查
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_test_index_valid(uint16_t index)
{
    uint8_t support_flag = 0;
    
    if ( 0xB0 == index ) {
        // 4G测试模式
        support_flag = 1;
    } else if ( 0xA0 == index ) {
        // 单板测试模式
        support_flag = 1;    
    } else {
        return 0;
    }

    // 有效更新
    if(support_flag) {
        // 类型变动时立即更新，否则等待周期刷新
        if ( top_modbus_rd.Self_IoT.mod_reg30900_IoT_Test.test_work_mode != index ) {
            top_modbus_rd.Self_IoT.mod_reg30900_IoT_Test.test_work_mode = index;
            iot_test_period_process();
        }
    }
    
    return support_flag;
}

/*
typedef enum {
    ESP_RST_UNKNOWN,    = 0  // 无法确定
    ESP_RST_POWERON,    = 1  // 电源上电
    ESP_RST_EXT,        = 2  // 外部引脚复位 (ESP32不适用)
    ESP_RST_SW,         = 3  // 软件复位 (esp_restart)
    ESP_RST_PANIC,      = 4  // 内核panic复位
    ESP_RST_INT_WDT,    = 5  // 中断看门狗复位
    ESP_RST_TASK_WDT,   = 6  // 任务看门狗复位
    ESP_RST_WDT,        = 7  // 其他看门狗复位
    ESP_RST_DEEPSLEEP,  = 8  // 深度睡眠退出复位
    ESP_RST_BROWNOUT,   = 9  // 欠压复位 (软件或硬件)
    ESP_RST_SDIO,       = 10 // SDIO复位
    ESP_RST_USB,        = 11 // USB外设复位
    ESP_RST_JTAG,       = 12 // JTAG复位
    ESP_RST_EFUSE,      = 13 // efuse错误复位
    ESP_RST_PWR_GLITCH, = 14 // 电源毛刺复位
    ESP_RST_CPU_LOCKUP  = 15 // CPU死锁复位 (双重异常)
} esp_reset_reason_t;
*/

/*------------------------------------------------------------------------------
 Function: iot_debug_index_300_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT调试信息更新(index = 300)
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void iot_debug_index_300_update(void)
{    
    /************************ 设备基本数据 ************************/
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.wifi_signal_strength = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_rssi;        // WIFI信号强度（UINT8）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.ble_server_signal_strength = reals.ble_server_signal_strength;     // 自己作为BLE client去访问BLE server从设备的信号强度
    if (1 == reals.rtc_flag.sBit.Time_valid) {
        top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.device_rtc_time = reals.now;                                   // 设备RTC时间（UINT32秒计数器）
    } else {
        top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.device_rtc_time = 0;                                           // 设备RTC时间（UINT32秒计数器）
    }
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.device_heartbeat_counter = reals.MCUPoweronCnt;                    // 设备心跳计数器（UINT32秒计数器）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_login_cloud_time = reals.last_login_cloud_time;               // 最近一次登录云服务器时间（UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_disconnect_cloud_time = reals.last_disconnect_cloud_time;     // 最近一次断开云服务器时间（UINT32）

    /************************ 设备在线数量 ************************/
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_online_num = reals.online_Iot_num;                // IOT在线数量（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.inverter_online_num = reals.online_Inv_num;           // 逆变器在线数量（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.pack_online_num = reals.online_Pack_num;              // PACK在线数量（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.wifi_local_device_num = wlcc_dev_online_num_get();    // WIFI局域网内我方设备节点数量（UINT16）

    /************************ CAN总线统计 ************************/
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_inv_rx_counter = COUNTER_DELTA_U16(reals.can_inv_rx_counter, reals.last_can_inv_rx_counter);     // CAN总线INV设备报文RX计数器（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_pack_rx_counter = COUNTER_DELTA_U16(reals.can_pack_rx_counter, reals.last_can_pack_rx_counter);  // CAN总线PACK设备报文RX计数器（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_iot_rx_counter = COUNTER_DELTA_U16(reals.can_iot_rx_counter, reals.last_can_iot_rx_counter);     // CAN总线IOT设备报文RX计数器（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.can_x_rx_counter = COUNTER_DELTA_U16(reals.can_x_rx_counter, reals.last_can_x_rx_counter);           // CAN总线X设备报文RX计数器（UINT16）

    /************************ 内存与网络统计 ************************/
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_remaining_ram_in_kb = MIN((heap_caps_get_free_size(MALLOC_CAP_INTERNAL)>>10), 0xFFFF);  // IOT剩余内置RAM（KB，UINT16）单位1KB，最大显示0xFFFF
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_remaining_ram_frag_pct = calc_internal_heap_frag_pct();    // IOT内置RAM碎片率（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_remaining_ram_ext_10kb = MIN((heap_caps_get_free_size(MALLOC_CAP_SPIRAM)>>10), 0xFFFF);// IOT剩余外置RAM（KB，UINT16）   单位10KB，最大显示0xFFFF
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.net_frame_tx_counter = COUNTER_DELTA_U16(reals.net_frame_tx_counter, reals.last_net_frame_tx_counter); // 网络报文帧TX计数器（MQTT/HTTP等，UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.net_frame_rx_counter = COUNTER_DELTA_U16(reals.net_frame_rx_counter, reals.last_net_frame_rx_counter); // 网络报文帧RX计数器（MQTT/HTTP等，UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.ble_frame_tx_counter = COUNTER_DELTA_U16(reals.ble_frame_tx_counter, reals.last_ble_frame_tx_counter); // BLE报文帧TX计数器（UINT16）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.ble_frame_rx_counter = COUNTER_DELTA_U16(reals.ble_frame_rx_counter, reals.last_ble_frame_rx_counter); // BLE报文帧RX计数器（UINT16）

    /************************ 设备连接时间戳 ************************/
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_build_time = Get_IoT_Build_Time_U32();                         // IOT固件编译时间（UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_login_router_time = reals.last_login_router_time;             // 最后成功登录路由器时间（UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_disconnect_router_time = reals.last_disconnect_router_time;   // 最后断开路由器时间（UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_client_connect_time = reals.last_ble_client_connect_time; // 最后成功连接BLE Client(手机)时间（UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_client_disconn_time = reals.last_ble_client_disconn_time; // 最后断开BLE Client(手机)时间（UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_server_connect_time = reals.last_ble_server_connect_time; // 最后成功连接BLE Server(储能设备)时间（UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_ble_server_disconn_time = reals.last_ble_server_disconn_time; // 最后断开BLE Server(储能设备)时间（UINT32）

    /************************ RTC时间同步 ************************/
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_rtc_sync_ble = reals.last_rtc_sync_ble;             // 最后一次RTC同步时间（来自BLE，UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_rtc_sync_cloud = reals.last_rtc_sync_cloud;         // 最后一次RTC同步时间（来自CLOUD，UINT32）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.last_rtc_sync_local = reals.last_rtc_sync_local;         // 最后一次RTC同步时间（来自LOCAL，UINT32）

    /************************ 线程最长运行时间（单位：us）************************/
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread1_max_runtime_us = reals.thread1_max_runtime_us;  // 线程1最长运行时间（us）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread2_max_runtime_us = reals.thread2_max_runtime_us;  // 线程2最长运行时间（us）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread3_max_runtime_us = reals.thread3_max_runtime_us;  // 线程3最长运行时间（us）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread4_max_runtime_us = reals.thread4_max_runtime_us;  // 线程4最长运行时间（us）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread5_max_runtime_us = reals.thread5_max_runtime_us;  // 线程5最长运行时间（us）
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.thread6_max_runtime_us = reals.thread6_max_runtime_us;  // 线程6最长运行时间（us）

    /************************ 设备其他数据 ************************/
#ifdef CONFIG_IOT_INTERNAL_TEMPERATURE_GET_ENABLE
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_internal_temp = IoT_Internal_Temperature_GetCache();  // 模组内部温度
#endif
    top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.IoT_Debug_Data.iot_reset_reason = (uint16_t)esp_reset_reason();       // 模块复位原因
}

/*------------------------------------------------------------------------------
 Function: iot_debug_period_process
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT调试模式
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_debug_period_process(void)
{
    switch ( top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.param_index )
    {
        case 300 :
            // IOT运维数据模式
            iot_debug_index_300_update();
            break;
        
        default:
            break;
    }
}

/**
  * @brief      IOT调试信息标签检查
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_debug_index_valid(uint16_t index)
{   
    uint8_t support_flag = 0;
    
    if ( 300 == index ) {
        // IOT运维数据模式
        support_flag = 1;
    } else {
        return 0;
    }

    // 有效更新
    if(support_flag) {
        // 类型变动时立即更新，否则等待周期刷新
        if ( top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.param_index != index ) {
            top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug.param_index = index;
            iot_debug_period_process();
        }
    }
    
    return support_flag;
}

/**
  * @brief      更新IOT相关状态至modbus寄存器
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_link_status_check(void)
{
    const wifi_info_t *wifi_info = iot_get_wifi_info();
    static link_status last_link_sta = {.all = UINT_MAX};

    // 蓝牙 连接状态
    if (IotSetData.dev_info_t.on_off.bit.ble_enable == 1) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.ble_connect = iot_ble_get_ConnectStatus();
    } else {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.ble_connect = 0;
    }

    // WIFI 连接状态
    if (IotSetData.dev_info_t.on_off.bit.wifi_enable == 1) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.wifi_sta = wifi_info->flag.bit.sta_link_ap;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.wifi_ap  = wifi_info->flag.bit.ap_is_link;
    } else {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.wifi_sta = 0;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.wifi_ap  = 0;
    }

    // 云端连接状态
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.mqtt = iot_mqtt_login_state();
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.time_lastreport = reals.last_disconnect_cloud_time;

    /*固件传输状态*/
    if ((CHANNEL_UNKOWN != xmodem_client_is_doing()) || http_ota_is_doing()) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.firmware_transfer = 1;
    } else {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.firmware_transfer = 0;
    }
    
#ifdef CONFIG_CAN_OTA_READY_TIMEOUT 
    // 总线静默逻辑不广播该状态
    last_link_sta.bit.firmware_transfer = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.firmware_transfer;
#endif

    if ((last_link_sta.all != top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.all) 
        || (1 == reals.iot_link_change))
    {
        /*ARM复位后快速读取SN及版本号(仅主机)*/
        if (1 == reals.iot_link_change)
        {
			if(INV_TYPE_ABOUT_13H == reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].can_type) {
				if(0 == reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_cnt_100ms) {
					reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_cnt_100ms = 15; //1.5s	
				}
			} else {
				ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].can_type);// 
			}	
        }
        
        reals.iot_link_change = 0;
		ESP_LOGW(TAG, "System link status: %08lx -> %08lx", last_link_sta.all, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.all);
        last_link_sta = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link;
        
        /*状态变动，更新至总线（仅主机）*/
		if((1 == reals.Addr_can_master)&&(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_FINISH)) {
            IoT_Status_Parameter_update(offsetof(iot_can_node_struct_reg11000, link), sizeof(link_status));
        }
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

#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE    
    /*智能控制状态*/
    iot_smart_control_status |= Smart_Tou_Ctrl_Status_Get()->all;
#endif

#ifdef CONFIG_RELAY_CTRL_ENABLE
    /*TOU控制状态*/
    iot_smart_control_status |= Relay_TimeCtrl_Enable_Status_Get();
#endif

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_smart_control.all = iot_smart_control_status;
    
    // 下发EMS控制指令
	if ((last_status != top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_smart_control.all) 
        || (1 == reals.iot_smart_change)) 
    {
		last_status = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_smart_control.all;
		ESP_LOGW(TAG, "System smart control status: %08lx", last_status);
        reals.iot_smart_change = 0;

        /*状态变动，更新至总线（仅主机）*/
		if((1 == reals.Addr_can_master)&&(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_FINISH)) {
            IoT_Status_Parameter_update(offsetof(iot_can_node_struct_reg11000, iot_smart_control), sizeof(iot_smart_ctrl_struct));
#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5                
            Iot_EMS_Ctrl_Func_5_Send(0, 0, 0);
#endif
        }    
	}
}

/*------------------------------------------------------------------------------
 Function: iot_fault_status_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT故障状态更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_fault_status_update(void)
{    
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

#ifdef CONFIG_USB_4G_MODULE_ENABLE
    /*11035  4G通信故障(bit3)*/
    if ( ( 1 == Inv_Modbus_Data->mod_reg12000_IOT_set.on_off.bit._4g_enable ) 
        && ( 0 == strlen((char *)Inv_Modbus_Data->mod_reg11000_IOT_info.IMEI) ) 
        && (( 60000 < (now_time - iot_4g_init_time_get())) 
            && (0 != iot_4g_init_time_get()))
        ) {
        Inv_Modbus_Data->mod_reg11000_IOT_info.iot_fault.bit._4g_fault = 1;
    } else {
        Inv_Modbus_Data->mod_reg11000_IOT_info.iot_fault.bit._4g_fault = 0;
    }
#endif

    /*11035  ARM通信故障(bit5)*/
    if ( Inv_Modbus_Data->mod_reg11000_IOT_info.can_bus_error 
        || ((reals.online_Inv_num == 0)&&(reals.MCUPoweronCnt > 15))
        ) {
        Inv_Modbus_Data->mod_reg11000_IOT_info.iot_fault.bit.arm_fault = 1;
    } else {
        Inv_Modbus_Data->mod_reg11000_IOT_info.iot_fault.bit.arm_fault = 0;
    }
}

/*------------------------------------------------------------------------------
 Function: IOT_Module_SelfCheck_Status
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT自检
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void IOT_Module_SelfCheck_Status(void)
{
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
    static uint16_t old_iot_check_status = 0;
    static uint16_t old_inv_check_status = 0;
    static uint16_t old_pack_check_status = 0;

    /*185  Panel连接检测(bit0)*/
    if ( 0 ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.panel_connection_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.panel_connection_detection = 0;
    }
    
    /*185  HMI连接检测(bit1)*/
    if ( 1 != reals.Addr_can_master ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.hmi_connection_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.hmi_connection_detection = 0;
    }
    
    /*185  云连接通讯检测(bit2)*/
    if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.mqtt ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.cloud_connection_communication_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.cloud_connection_communication_detection = 0;
    }

    /*185  ExFlash检测(bit3)*/
    if ( 0 ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.exflash_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.exflash_detection = 0;
    }

    /*185  RTC检测(bit4)*/
    if ( reals.rtc_flag.sBit.RTC_valid_from_RTC ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.rtc_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.rtc_detection = 0;
    }

    /*185  组网连接检测(bit5)*/
    if ( 0 ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.networking_connection_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.networking_connection_detection = 0;
    }

    /*185  蓝牙状态检测(bit6)*/
    if ( 1 == iot_ble_on_off_status() ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.bluetooth_status_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.bluetooth_status_detection = 0;
    }
    
    /*185  WiFi状态检测(bit7)*/
    if ( 1 == iot_wifi_on_off_status() ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.wifi_status_detection = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.wifi_status_detection = 0;
    }

    if (( old_iot_check_status != Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.all ) 
        || ( old_inv_check_status != Inv_Modbus_Data->mod_reg00100_AppPage1.INV_TEST.all ) 
        || ( old_pack_check_status != Inv_Modbus_Data->mod_reg00100_AppPage1.PACK_TEST.all ))
    {
        old_iot_check_status = Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.all;
        old_inv_check_status = Inv_Modbus_Data->mod_reg00100_AppPage1.INV_TEST.all;
        old_pack_check_status = Inv_Modbus_Data->mod_reg00100_AppPage1.PACK_TEST.all;
            
		ESP_LOGW(TAG, "IOT_Module_SelfCheck_Status : IOT(%d),  INV(%d), PACK(%d)", 
            Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_TEST.all, 
            Inv_Modbus_Data->mod_reg00100_AppPage1.INV_TEST.all, 
            Inv_Modbus_Data->mod_reg00100_AppPage1.PACK_TEST.all);
    }
}

/*------------------------------------------------------------------------------
 Function: system_status_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      系统状态更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void system_status_update(void)
{
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
    MOD_STRUCT_Pack *Pack_Modbus_Data = &top_modbus_rd.Pack[PACK_MAX_NUM];

    /*174  升级状态(bit1)*/
    if (sys_is_updating()) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_state.bit.update = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_state.bit.update = 0;
    }

    /*124->174  休眠状态(bit3)*/
    Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_state.bit.remote_switch = reals.IOT_Status_Flag.sBit.system_sleep_flag;

    /*11035->174  IOT故障状态(bit5)*/
    if (Inv_Modbus_Data->mod_reg11000_IOT_info.iot_fault.all) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_state.bit.iot_fault_status = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_state.bit.iot_fault_status = 0;
    }

    /*6029~6030/6036~6041->174  PACK故障状态(bit6)*/
    if (Pack_Modbus_Data->mod_reg06000_Pack_sum.all_pack_alarm1 
        || Pack_Modbus_Data->mod_reg06000_Pack_sum.all_pack_alarm2 
#if 0 // 6036~6041对应原单包故障，暂不检查       
        || Pack_Modbus_Data->mod_reg06000_Pack_sum.PackProtect 
        || Pack_Modbus_Data->mod_reg06000_Pack_sum.PackSysErr[0] 
        || Pack_Modbus_Data->mod_reg06000_Pack_sum.PackSysErr[1] 
        || Pack_Modbus_Data->mod_reg06000_Pack_sum.PackSysErr[2]
        || Pack_Modbus_Data->mod_reg06000_Pack_sum.PackAlarm
#endif        
    ) {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_state.bit.pack_fault_status = 1;
    } else {
        Inv_Modbus_Data->mod_reg00100_AppPage1.IOT_state.bit.pack_fault_status = 0;
    }

    /*IOT自检*/
    IOT_Module_SelfCheck_Status();
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
static void system_off_or_sleep_check(void)
{
    static uint8_t old_power_down_flag = 0;
    uint8_t new_power_down_flag;

    uint16_t pack_work_status = 0;
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
    pack_work_status |= get_bat_data_ptr()->all_data.run_state;
#endif

#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE
    pack_work_status |= can_node_rd.Pack[0].pack_announce.work_status;
#endif    
    if ((can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.system_off == 1) 
         || (pack_work_status == 0x33)) {
        new_power_down_flag = 1;
    } else {
        new_power_down_flag = 0;
    }

    /*关机状态检查*/
    if (old_power_down_flag != new_power_down_flag)
    {
        if (!new_power_down_flag)
        {
            /*关机状态消失（理论上不可能触发）*/
            ESP_LOGE(TAG, "system_off_or_sleep_check : SYSTEM ON! (Inv: %d, Pack: %d)", 
                    can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.system_off, pack_work_status);
            reals.EventFlag.sBit.turn_on = 1;
            reals.IOT_Status_Flag.sBit.system_off_flag = 0;
        }
        else
        {
            /*关机状态生成*/
            ESP_LOGE(TAG, "system_off_or_sleep_check : SYSTEM OFF! (Inv: %d, Pack: %d)", 
                can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.system_off, pack_work_status);
            reals.EventFlag.sBit.turn_off = 1;
            reals.IOT_Status_Flag.sBit.system_off_flag = 1;

#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
            // 下电前存储临时数据（根据不同项目下电时间自行决定是否写入）
            Energy_File_Header_Save();
#endif
        }

        old_power_down_flag = new_power_down_flag;
    }

    static uint8_t old_sleep_flag = 0;
    uint8_t new_sleep_flag;
    if ((can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.sleep_on == 1) 
        || (can_node_rd.Pack[0].pack_announce.pack_set_status.status.sleep_status == 1)) {
        new_sleep_flag = 1;
    } else {
        new_sleep_flag = 0;
    }

    /*休眠状态检查*/
    if (old_sleep_flag != new_sleep_flag)
    {
        if (!new_sleep_flag)
        {
            ESP_LOGE(TAG, "system_off_or_sleep_check : SLEEP END! (Inv: %d, Pack: %d)", 
                    can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.sleep_on,
                    can_node_rd.Pack[0].pack_announce.pack_set_status.status.sleep_status);
            reals.IOT_Status_Flag.sBit.system_sleep_flag = 0;
        }
        else
        {
            ESP_LOGE(TAG, "system_off_or_sleep_check : SLEEP START! (Inv: %d, Pack: %d)", 
                    can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.sleep_on,
                    can_node_rd.Pack[0].pack_announce.pack_set_status.status.sleep_status);
            reals.IOT_Status_Flag.sBit.system_sleep_flag = 1;
        }

        old_sleep_flag = new_sleep_flag;
    }

    /*复位状态检查*/
    if ( 1 == reals.IOT_Status_Flag.sBit.system_restart_flag ) {
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
        while(1);
    }
}

/*------------------------------------------------------------------------------
 Function: inv_mcu_ver_in_boot_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      逆变版本滞留boot检查
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static modbus_soft_version* inv_mcu_ver_in_boot_check(void)
{
    MOD_STRUCT_reg01100 *data = &top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base;
    
    for ( uint8_t i = 0 ; i < MD_SOFT_MAIN_CAP ; i++ )
    {
        if (( data->soft[i].type != 0 ) 
            && ( back_image_type_to_index(data->soft[i].type) > 0 )      
            && ( data->soft[i].version/100 != 0 )
            && ( data->soft[i].version%100 == 0 )
            )
        {
            return &data->soft[i];
        }
    }

    for ( uint8_t i = 0 ; i < MD_SOFT_BACK_CAP ; i++ )
    {
        if (( data->soft_back[i].type != 0 ) 
            && ( back_image_type_to_index(data->soft_back[i].type) > 0 )      
            && ( data->soft_back[i].version/100 != 0 )
            && ( data->soft_back[i].version%100 == 0 )
            )
        {
            return &data->soft_back[i];
        }
    }

    return NULL;
}

/*------------------------------------------------------------------------------
 Function: MCU_Slave_Update_Check_In_Boot
 -----------------------------------------------------------------------------*/
/**
  * @brief      下级MCU卡死boot检查并自动升级逻辑
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void MCU_Slave_Update_Check_In_Boot(void)
{
#ifdef CONFIG_MCU_AUTO_UPDATE_IN_BOOT
    static uint32_t pre_time = 0;
    static bool Off_flag = false;
    static uint32_t last_update_ver = 0;
    static uint8_t retry_cnt = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 升级过程中不检查
    if ( sys_is_updating() 
        || (can_ota_status.period_cmd_delay > 0)
        || (can_ota_status.ready_timeout > 0))
    {
        pre_time = now_time;

        if((CHANNEL_UNKOWN != xmodem_client_is_doing())
            || (http_ota_is_doing()))
        {
            // 本地暂存固件更新，清除停止标志
            Off_flag = false;
            retry_cnt = 0;
        }
    }
    // 判断是否允许检查
    else if (((now_time - pre_time) > 15000 )   // 周期检查
            || ((last_update_ver == 0) && (reals.online_Inv_num > 0))   // 上电/更新后首次检查
            )
    {
        modbus_soft_version *ver_data = NULL; 

        // 获取BOOT固件信息
        if(ver_data == NULL) ver_data = inv_mcu_ver_in_boot_check();

        // 固件信息有效，检查本地升级代码
        if ( ver_data != NULL )
        {
            pre_time = now_time;
            if ( last_update_ver != ver_data->version )
            {
                // 有新的待检查固件
                last_update_ver = ver_data->version;
                retry_cnt = 0;
                Off_flag = false;
                ESP_LOGI(TAG, "Auto OTA : New firmware version detected: %lu", ver_data->version);
            }
            
            if ( Off_flag == false )
            {
                if(retry_cnt >= 3) {
                    // 升级多次失败，禁止检查
                    Off_flag = true;
                    retry_cnt = 0;
                    ESP_LOGE(TAG, "Auto OTA : OTA upgrade failed more than 3 times, disable further checks.");
                }
                else if ( 0 == back_image_firmware_check_and_ready(ver_data->type, ver_data->version, &can_ota_status) )
                {
                    // 发起升级
                    ESP_LOGI(TAG, "Auto OTA : Start CAN OTA upgrade.");
                    CanOtaSender_Begin(can_ota_status.start.dev_id, can_ota_status.start.fileType, can_ota_status.start.fileVersion, &can_ota_status);
                    retry_cnt++;
                }
                else
                {
                    // 检查失败，禁止检查
                    Off_flag = true;
                    retry_cnt = 0;
                    ESP_LOGE(TAG, "Auto OTA : Firmware check failed, disable further checks.");
                }
            }
        }
        else
        {
            // 无MCU处于BOOT
            retry_cnt = 0;
        }
    }
#endif            
}

void iot_period_fast_task(void *pvParameters)
{
    // 任务时间统计声明和初始化
    TASK_TIME_STATS_DECLARE(iot_period_fast_task);
    iot_timer_init();

    // 设备状态更新
    iot_about_update();

#if CONFIG_DISASTER_WARN_CHANNEL == 2
    uint8_t enable = IotSetData.dev_info_t.thunder_ctrl.thunder_enable;
    SetStormWatchEnableRegisterData(enable);
#endif

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));

        // 开始时间统计
        TASK_TIME_START(iot_period_fast_task);
        
        /* 更新主循环定时 */
        app_system_timer();

#ifdef CONFIG_CAN_PORT_ENABLE
        /*轮询发送状态机*/
        iot_polldev_task();

        // Can发送任务
        esp_canbus_send_task();
#endif

#ifdef CONFIG_UART_PORT_ENABLE
        /*蓝牙通道处理*/
        uart_to_ble_queue_task();

        // uart Rx 解析
        App_Uart_1_Rx_Prase();
        uart_ota_thread();
#endif

        if (1 == reals.st_FlagTime.bits.b1FlagSys50ms)
        {           
#ifdef CONFIG_UART_PORT_ENABLE
            /*串口发送任务*/
            app_uart1_Tx_task();

            /*透传回复超时*/
            ble_mqtt_to_uart_timeout_check();
#endif
        }

        if (1 == reals.st_FlagTime.bits.b1FlagSys100ms)
        {
#ifdef CONFIG_CAN_PORT_ENABLE
            /*modbus 快速读取处理（非插队，实际在iot_polldev_task发送）*/
            can_data_poll_fast_from_modbus();

            /*CAN 100MS超时定时器*/
            CanTimerout_Cb();
#endif        
            /*设备状态更新*/
            iot_about_update();

            /*IOT连接状态及设置检查*/
            iot_link_status_check();

            /*更新智能控制状态*/
            iot_smart_control_status_check();

            /*IOT故障状态更新*/
            iot_fault_status_update();
            
            /*更新自身在线状态*/
            system_status_update();

            /*系统关机及休眠状态检查*/
            system_off_or_sleep_check();
            
            /*系统DO唤醒状态检查*/
            system_do_wakeup_check();

            /*IOT被唤醒检查*/
            system_be_wakened_check();
        }

        if (1 == reals.st_FlagTime.bits.b1FlagSys1s)
        {
            // 心跳累计
            reals.MCUPoweronCnt++;
            
            /*更新本机时间*/
            if (0 == (reals.MCUPoweronCnt % 86400)) {
                time_sync_system_to_local(1);
            } else {
                time_sync_system_to_local(0);
            }
        }

        // 结束时间统计并打印
        TASK_TIME_END(iot_period_fast_task);
        TASK_TIME_PRINT(iot_period_fast_task, 500); // 每100次循环打印一次
        TASK_TIME_RUNS_MAX_TO_FIELD(iot_period_fast_task, reals.thread4_max_runtime_us);
        TASK_TIME_TIMEOUT_CLEAN(iot_period_fast_task, TASK_COMMON_CLEAN_TIMEOUT_MS);
    }
}

void App_Slow_Task_SysTime(void)
{
    static uint8_t ts_u16Cnt1000ms = 0;
    static uint8_t ts_u16Cnt100ms = 0;

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
}

/** 'tasks' command prints the list of tasks and related information */
static int tasks_info(void)
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = iot_calloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
	printf("*************************task information*************************\n");
	printf("* Task Name: the name of the task\n");
	printf("*    Status: the running status of the task\n");
	printf("*      -(X): running\n");
	printf("*      -(B): blocked\n");
	printf("*      -(R): ready\n");
	printf("*      -(D): delete\n");
	printf("*      -(S): suspended\n");
	printf("*      Prio: the priority of the task\n");
	printf("*       HWM: minimum memory size during task execution\n");
	printf("*     Task#: task number\n");
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
	printf("*  Affinity: which CPU core is the task bound to\n");
	printf("*     -(0):  CPU core 0\n");
	printf("*     -(1):  CPU core 1\n");
	printf("*    -(-1):  both CPUs\n");
#endif
	printf("******************************************************************\n");
    // fputs("Task Name\t\tStatus\tPrio\tHWM\tTask#", stdout);
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}

static void thunder_state_update(void)
{
#if CONFIG_DISASTER_WARN_CHANNEL == 2
    thunder_ctrl_t *thunder_ctrl = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl;
    thunder_ctrl->thunder_state = GetStormWatchDataState();         // 风暴状态由EMS模块给出
#endif    
}

/*------------------------------------------------------------------------------
 Function: print_mem_info
 -----------------------------------------------------------------------------*/
/**
  * @brief      打印内部RAM、外部PSRAM以及总体的堆内存使用情况。
  * @param[in]  void
  * @param[out] None
  * @return     static void
  */
static void print_mem_info(void)
{
    // 1. 内部 RAM (Internal RAM)
    uint32_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t internal_used = internal_total - internal_free;
    uint32_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    float internal_frag = internal_free ? 100.0f * (1.0f - ((float)internal_largest / internal_free)) : 0.0f;

    // 2. 外部 PSRAM (External PSRAM)
    uint32_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_used = psram_total - psram_free;
    uint32_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    float psram_frag = psram_free ? 100.0f * (1.0f - ((float)psram_largest / psram_free)) : 0.0f;

    // 3. 总计 (Total)
    uint32_t total_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    uint32_t total_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t total_used = total_total - total_free;
    uint32_t total_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint32_t total_min_free = esp_get_minimum_free_heap_size();
    float total_frag = total_free ? 100.0f * (1.0f - ((float)total_largest / total_free)) : 0.0f;

    // 使用 %-14s 对第一列进行左对齐，并为数字列使用 %10s 或 %8s 进行右对齐
    ESP_LOGI(TAG, "---------------------------------------------------------------------------------------------");
    ESP_LOGI(TAG, "| %-14s | %10s | %10s | %10s | %10s | %10s | %7s |",
             "Memory Region", "Total", "Used", "Free", "Largest", "Min Free", "Frag.");
    ESP_LOGI(TAG, "---------------------------------------------------------------------------------------------");

    ESP_LOGI(TAG, "| %-14s | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %10s | %6.2f%% |",
             "Internal RAM", internal_total, internal_used, internal_free, internal_largest, "----", internal_frag);

    if (psram_total > 0) {
        ESP_LOGI(TAG, "| %-14s | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %10s | %6.2f%% |",
                 "External PSRAM", psram_total, psram_used, psram_free, psram_largest, "----", psram_frag);
    } else {
        ESP_LOGI(TAG, "| %-14s | %10s | %10s | %10s | %10s | %10s | %7s |",
                 "External PSRAM", "----", "----", "----", "----", "----", "----");
    }

    ESP_LOGI(TAG, "---------------------------------------------------------------------------------------------");
    ESP_LOGI(TAG, "| %-14s | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %10" PRIu32 " | %6.2f%% |",
             "Total", total_total, total_used, total_free, total_largest, total_min_free, total_frag);
    ESP_LOGI(TAG, "---------------------------------------------------------------------------------------------");
}

/*------------------------------------------------------------------------------
 Function: heap_info_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      内存状态检查
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void heap_info_check(void) {
    // 获取总的可用堆大小
    uint32_t total_free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // 获取最大的连续可用堆块大小
    uint32_t max_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    if (total_free_size < 20480)
    {
        ESP_LOGE(TAG, "Internal Total free heap size: %"PRIu32" bytes", total_free_size);
    }
    
    if (max_free_block < 10240)
    {
        ESP_LOGE(TAG, "Internal Largest free block: %"PRIu32" bytes", max_free_block);
    }    
}

/*------------------------------------------------------------------------------
 Function: File_OTA_Status_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      文件升级进度处理
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void File_OTA_Status_Update(void)
{
#ifdef CONFIG_FILE_DATA_OTA_ENABLE    

    static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
      
    // 文件系统资源升级进度
    if (Get_File_OTA_Status())
    {
        set_ota_pct_info(NULL, NULL, reals.current_ota_info.type, Get_File_OTA_Status(), 
            1, Get_File_OTA_Pct(), Get_File_OTA_Errcode());
    }    
    // 超时清除
    else
    {
        if ((pre_time) && ((now_time - pre_time) > 20000))
        {
            ESP_LOGW(TAG, "File_OTA_Status_Timeout(%ld s) clean!", ((now_time - pre_time) / 1000));
            set_ota_pct_info(NULL, 0, 0, 0, 0, 0, 0);
            pre_time = 0;
        }

        return;
    }

    // 正在提供升级进度
    pre_time = now_time;
    return;
    
#endif    
}

/*------------------------------------------------------------------------------
 Function: Smart_Tou_Ctrl_Data_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      IF-THEN智能控制数据更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Smart_Tou_Ctrl_Data_Update(void)
{
#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE

    // 获取智能TOU控制模块的下一条控制命令
    smart_tou_ctrl_result_t tou_ctrl_result = {0};
    if(Smart_Tou_Ctrl_Command_Get(&tou_ctrl_result)) {
        ESP_LOGI(TAG, "TOU命令获取成功，reg_type=%d, device_id=%d, reg_addr=%d, reg_value=%d",
                 tou_ctrl_result.reg_type,
                 tou_ctrl_result.device_id,
                 tou_ctrl_result.reg_addr,
                 tou_ctrl_result.reg_value);
        
        // 0-modbus beta
        if ( TOU_REG_TYPE_MODBUS_BETA == tou_ctrl_result.reg_type ){
            Single_Reg_Set_to_IoT(tou_ctrl_result.device_id, tou_ctrl_result.reg_addr, tou_ctrl_result.reg_value);
        }
            
    #if 0   // 调试
        uint16_t debug_old_data = 0;
        // 声明并填充参数结构体
        modbus_change_params_t log_params = {
            .slave_address       = tou_ctrl_result.device_id,
            .start_address       = tou_ctrl_result.reg_addr,
            .reg_count           = 1,
            .old_data            = (uint16_t *)&debug_old_data,
            .new_data            = (uint16_t *)&tou_ctrl_result.reg_value,
            .protocol_version    = PROTOCOL_MODBUS_BETA,
            .modification_source = DATA_SOURCE_IOT_INTERNAL,
        };
        
        // 3. 调用新函数，只传递一个参数
        if (Reg_Change_Log_Generate_By_Modbus(&log_params) != 0) {
            // 处理错误
        }
    #endif
    };
    
#endif
}

/**
 * @brief 获取 Smart TOU 的寄存器值（用于回调注册）
 *
 * 说明（详尽）：
 * - 该函数根据传入的协议类型 proto_ver 从相应的数据源获取寄存器值并写入 reg_value。
 * - 目前仅支持 TOU_REG_TYPE_MODBUS_BETA（modbus beta）协议分支。
 * - 函数对输入参数做基本校验并将错误以 false 返回。
 *
 * 参数约定：
 * - proto_ver: 协议类型标识（例如 TOU_REG_TYPE_MODBUS_BETA）。
 * - dev_addr:  目标设备地址（Modbus 从机地址）。
 * - reg_addr:  要读取的寄存器地址（16-bit）。
 * - reg_value: 输出参数，函数将读取到的寄存器值写入 *reg_value。调用方必须保证 reg_value 非 NULL。
 *
 * 返回值：
 * - true  ：成功读取并将值写入 *reg_value。
 * - false ：参数错误、读取失败或不支持的协议类型。
 */
static bool Smart_Tou_Modbus_Reg_Value_Get(uint8_t proto_ver, uint8_t dev_addr, uint16_t reg_addr, uint16_t *reg_value)
{
    /* 输出指针不能为空 */
    if (reg_value == NULL) return false;

#if 1
    /* 支持：0 - modbus beta */
    if (TOU_REG_TYPE_MODBUS_BETA == proto_ver) {
        /* Single_Reg_Get_From_IoT(SlaveAddress, RegAddress, value)
         * - 返回 true：已成功把寄存器值写入 *reg_value
         * - 返回 false：读取失败（表不存在/回调失败等）
         */
        return Single_Reg_Get_From_IoT(dev_addr, reg_addr, reg_value);
    }
#else
    /* 备用实现示例
     * NOTE: 直接赋值，便于各项目自行开发
     */
    if ((TOU_REG_TYPE_MODBUS_BETA == proto_ver) 
        && (dev_addr == 0)) {
        switch (reg_addr) 
        {
            case 7011:
                *reg_value = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg07000_Pack_set.BatteryChargingPower;
                break;
            case 7012:
                *reg_value = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg07000_Pack_set.BatteryDischargingPower;
                break;
            default:
                return false;
        }
        return true;
    }
#endif
    /* 不支持的协议版本，记录警告以便定位 */
    ESP_LOGE(TAG, "Smart_Tou_Modbus_Reg_Value_Get: unsupported proto_ver=%u dev=%u reg=%u",
             (unsigned)proto_ver, (unsigned)dev_addr, (unsigned)reg_addr);

    /* 其它协议类型未实现或不支持 */
    return false;
}

/*------------------------------------------------------------------------------
 Function: Ac_Ems_Cloud_Data_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      云端AC-EMS智能控制数据更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Ac_Ems_Cloud_Data_Update(void)
{
#ifdef CONFIG_AC_EMS_CLOUD_ENABLE

    // 获取云端AC_EMS控制模块的下一条控制命令
    ac_ems_cloud_ctrl_result_t ctrl_result = {0};
    if(Ac_Ems_Cloud_Command_Get(&ctrl_result)) {
        // 可在此处进一步处理ctrl_result
         ESP_LOGI(TAG, "AC_EMS命令获取成功，reg_type=%d, device_id=%d, channel_power[0]=%ld ...",
                  ctrl_result.reg_type,
                  ctrl_result.device_id,
                  (long)ctrl_result.channel_power[0]);
    };
    
#endif
}

/*------------------------------------------------------------------------------
 Function: keyboard_data_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      键盘数据更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void keyboard_data_update(void)
{
#ifdef CONFIG_USB_HOST_HID_ENABLE

    char buf[128] = {0};
    size_t len = 0;

    // 从键盘输入队列内获取数据
    if (hid_receive_key_input_from_queue(buf, sizeof(buf), &len, 0)) {
        ESP_LOGI(TAG, "Keyboard Received: %s, len=%u", buf, (unsigned)len);
        // 获取成功，可在这里执行对应操作
    }
    
#endif
}

/*------------------------------------------------------------------------------
 Function: energy_data_from_modbus_calc
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus数据来源能量计算
  * @param[in]  kwh_energy_u32_t *energy_data  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t energy_data_from_modbus_calc(kwh_energy_u32_t *energy_data)
{
#if INV_MAX_NUM > 1
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[INV_MAX_NUM];
#else
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
#endif
    MOD_STRUCT_Pack *Pack_Modbus_Data = &top_modbus_rd.Pack[PACK_MAX_NUM];

    // 能量更新
#ifdef ENABLE_ENERGY_RAW_PV_TOTAL
    energy_data->energy_u32[ENERGY_IDX_PV_TOTAL] = Inv_Modbus_Data->mod_reg00100_AppPage1.PvTotalChargingEnergy;       // 154~155
#endif
    
#ifdef ENABLE_ENERGY_RAW_GRID_INPUT
    energy_data->energy_u32[ENERGY_IDX_GRID_INPUT] = Inv_Modbus_Data->mod_reg00100_AppPage1.GridTotalChargingEnergy;   // 156~157
#endif
    
#ifdef ENABLE_ENERGY_RAW_GRID_OUTPUT
    energy_data->energy_u32[ENERGY_IDX_GRID_OUTPUT] = Inv_Modbus_Data->mod_reg00100_AppPage1.FeedbackEnergy;           // 158~159
#endif
    
#ifdef ENABLE_ENERGY_RAW_AC_LOAD
    energy_data->energy_u32[ENERGY_IDX_AC_LOAD] = Inv_Modbus_Data->mod_reg00100_AppPage1.ACLoadTotalEnergy;            // 152~153
#endif
    
#ifdef ENABLE_ENERGY_RAW_DC_LOAD
    energy_data->energy_u32[ENERGY_IDX_DC_LOAD] = Inv_Modbus_Data->mod_reg00100_AppPage1.DCLoadTotalEnergy;            // 150~151
#endif
    
#ifdef ENABLE_ENERGY_RAW_AC_PV
    energy_data->energy_u32[ENERGY_IDX_AC_PV] = 0;
#endif
    
#ifdef ENABLE_ENERGY_RAW_PV_TOTAL_AGAIN
    energy_data->energy_u32[ENERGY_IDX_PV_TOTAL_AGAIN] = 0;
#endif
    
#ifdef ENABLE_ENERGY_RAW_BATTERY_CHARGE
    energy_data->energy_u32[ENERGY_IDX_BATTERY_CHARGE] = Pack_Modbus_Data->mod_reg06000_Pack_sum.total_chg_energy;    // 6019~6020
#endif
    
#ifdef ENABLE_ENERGY_RAW_BATTERY_DISCHARGE
    energy_data->energy_u32[ENERGY_IDX_BATTERY_DISCHARGE] = Pack_Modbus_Data->mod_reg06000_Pack_sum.total_dsg_energy; // 6021~6022
#endif
    
#ifdef ENABLE_ENERGY_RAW_PV_TO_AC_LOAD
    energy_data->energy_u32[ENERGY_IDX_PV_TO_AC_LOAD] = Inv_Modbus_Data->mod_reg00100_AppPage1.PvToACLoadEnergy;       // 162~163
#endif
    
#ifdef ENABLE_ENERGY_RAW_CAR_CHARGE
    energy_data->energy_u32[ENERGY_IDX_CAR_CHARGE] = Inv_Modbus_Data->mod_reg00100_AppPage1.TotalEnergy_Car;           // 176~177
#endif

    return 100;
}

/*------------------------------------------------------------------------------ 
 Function: energy_data_from_can_calc
 -----------------------------------------------------------------------------*/
/**
  * @brief      Can数据来源能量计算（从 CAN 6 字节小端能量字段提取低 32 位）
  * @param[in]  kwh_energy_u32_t *energy_data  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t energy_data_from_can_calc(kwh_energy_u32_t *energy_data)
{
#if INV_MAX_NUM > 1
    can_node_struct_Inv *Inv_Can_Data = &can_node_rd.Inv[INV_MAX_NUM];
#else
    can_node_struct_Inv *Inv_Can_Data = &can_node_rd.Inv[reals.Addr_can_self];
#endif

    uint64_t tmp64;

#ifdef ENABLE_ENERGY_RAW_PV_TOTAL
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_PV_TOTAL_CHARGING].energy);
    energy_data->energy_u32[ENERGY_IDX_PV_TOTAL] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_INPUT
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_GRID_TOTAL_CHARGING].energy);
    energy_data->energy_u32[ENERGY_IDX_GRID_INPUT] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_OUTPUT
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_FEEDBACK_ENERGY].energy);
    energy_data->energy_u32[ENERGY_IDX_GRID_OUTPUT] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_AC_LOAD
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_ACLOAD_TOTAL].energy);
    energy_data->energy_u32[ENERGY_IDX_AC_LOAD] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_DC_LOAD
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_DCLOAD_TOTAL].energy);
    energy_data->energy_u32[ENERGY_IDX_DC_LOAD] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_AC_PV
    energy_data->energy_u32[ENERGY_IDX_AC_PV] = 0;
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TOTAL_AGAIN
    energy_data->energy_u32[ENERGY_IDX_PV_TOTAL_AGAIN] = 0;
#endif

#ifdef ENABLE_ENERGY_RAW_BATTERY_CHARGE
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_PACK_TOTAL_CHARGE].energy);
    energy_data->energy_u32[ENERGY_IDX_BATTERY_CHARGE] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_BATTERY_DISCHARGE
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_PACK_TOTAL_DSG].energy);
    energy_data->energy_u32[ENERGY_IDX_BATTERY_DISCHARGE] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TO_AC_LOAD
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_PV_TO_AC_LOAD].energy);
    energy_data->energy_u32[ENERGY_IDX_PV_TO_AC_LOAD] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

#ifdef ENABLE_ENERGY_RAW_CAR_CHARGE
    tmp64 = le6_to_u64(Inv_Can_Data->inv_today_energy[ENERGY_LABEL_TOTAL_ENERGY_CAR].energy);
    energy_data->energy_u32[ENERGY_IDX_CAR_CHARGE] = (uint32_t)(tmp64 & 0xFFFFFFFFu);
#endif

    return 1;
}

/*------------------------------------------------------------------------------
 Function: energy_data_debug_calc
 -----------------------------------------------------------------------------*/
/**
  * @brief      能量计算测试
  * @param[in]  kwh_energy_u32_t *energy_data  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t energy_data_debug_calc(kwh_energy_u32_t *energy_data)
{
    uint32_t energy = (uint32_t)((reals.MCUPoweronCnt / 10) & 0xFFFFFFFFu);

    // 能量更新
#ifdef ENABLE_ENERGY_RAW_PV_TOTAL
    energy_data->energy_u32[ENERGY_IDX_PV_TOTAL] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_GRID_INPUT
    energy_data->energy_u32[ENERGY_IDX_GRID_INPUT] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_GRID_OUTPUT
    energy_data->energy_u32[ENERGY_IDX_GRID_OUTPUT] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_AC_LOAD
    energy_data->energy_u32[ENERGY_IDX_AC_LOAD] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_DC_LOAD
    energy_data->energy_u32[ENERGY_IDX_DC_LOAD] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_AC_PV
    energy_data->energy_u32[ENERGY_IDX_AC_PV] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_PV_TOTAL_AGAIN
    energy_data->energy_u32[ENERGY_IDX_PV_TOTAL_AGAIN] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_BATTERY_CHARGE
    energy_data->energy_u32[ENERGY_IDX_BATTERY_CHARGE] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_BATTERY_DISCHARGE
    energy_data->energy_u32[ENERGY_IDX_BATTERY_DISCHARGE] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_PV_TO_AC_LOAD
    energy_data->energy_u32[ENERGY_IDX_PV_TO_AC_LOAD] = energy;
#endif
    
#ifdef ENABLE_ENERGY_RAW_CAR_CHARGE
    energy_data->energy_u32[ENERGY_IDX_CAR_CHARGE] = energy;
#endif

    return 1;
}

/*------------------------------------------------------------------------------
 Function: energy_data_calc_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      能量计算数据更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void energy_data_calc_update(void)
{
    kwh_energy_u32_t energy_data = {0};
    uint16_t to_1wh = 0;

#if 0

    /*能量计算测试*/
    to_1wh = energy_data_debug_calc(&energy_data);

#else

#ifdef CONFIG_CAN_ENERGY_ANNOUNCE_ENABLE
    /*Can数据来源能量计算*/
    to_1wh = energy_data_from_can_calc(&energy_data);
#else
    /*Modbus数据来源能量计算*/
    to_1wh = energy_data_from_modbus_calc(&energy_data);
#endif

#endif

    /*更新至能量存储模块*/
    Energy_File_Raw_Data_Update(&energy_data, to_1wh, NULL);
}

/*------------------------------------------------------------------------------
 Function: Modbus_Reg_3700_Data_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      Modbus 3700段数据更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Modbus_Reg_3700_Data_Update(void)
{
    uint32_t tmp;
    uint16_t out;

#ifdef ENABLE_ENERGY_RAW_BATTERY_DISCHARGE
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_BATTERY_DISCHARGE) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.PackTotalDsgEnergy.all = out;    // 3700
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TOTAL
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_PV_TOTAL) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.PvTotalChargingEnergy.all = out; // 3701
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TO_AC_LOAD
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_PV_TO_AC_LOAD) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.PVToACloadEnergy.all = out;      // 3702
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_INPUT
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_GRID_INPUT) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.GridTotalChargingEnergy.all = out; // 3703
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_OUTPUT
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_GRID_OUTPUT) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.GridFeedbackEnergy.all = out;     // 3704
#endif

#ifdef ENABLE_ENERGY_RAW_DC_LOAD
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_DC_LOAD) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.DCLoadTotalEnergy.all = out;       // 3705
#endif

#ifdef ENABLE_ENERGY_RAW_AC_LOAD
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_AC_LOAD) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.ACLoadTotalEnergy.all = out;       // 3706
#endif

    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.InvTotalEnergy.all = 0;    // 3707
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.GenTotalEnergy.all = 0;    // 3708

#ifdef ENABLE_ENERGY_RAW_CAR_CHARGE
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_CAR_CHARGE) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.TotalEnergy_Car.all = out;    // 3709
#endif

#ifdef ENABLE_ENERGY_RAW_BATTERY_CHARGE
    tmp = Energy_Day_Data_Wh_Get(ENERGY_IDX_BATTERY_CHARGE) / 10;
    out = 0x8000u | (uint16_t)(tmp & 0x7FFFu);
    top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy.PackTotalChargeEnergy.all = out; // 3710
#endif
}


/*------------------------------------------------------------------------------
 Function: iot_energy_data_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT能量数据更新
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void iot_energy_data_update(void)
{
    /*能量计算数据更新*/
    energy_data_calc_update();

#ifdef ENABLE_ENERGY_RAW_PV_TOTAL
    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.PvTotalChargingEnergyToday = (uint16_t)(Energy_Day_Data_Wh_Get(ENERGY_IDX_PV_TOTAL) / 100);    // 189
    top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.PvTotalChargingEnergyMonth = (uint16_t)Energy_Month_Data_100Wh_Get(ENERGY_IDX_PV_TOTAL);       // 190
#endif 

#ifdef CONFIG_MODBUS_REG_ADDR_3700_ENABLE
    /*Modbus 3700段数据更新*/
    Modbus_Reg_3700_Data_Update();
#endif
}

/*------------------------------------------------------------------------------
 Function: Smart_Generator_Ctrl_Process
 -----------------------------------------------------------------------------*/
/**
  * @brief      智能发电机控制任务
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Smart_Generator_Ctrl_Process(void)
{
    static uint32_t pre_time = 0;
    uint8_t Gen_Enable = 0;
    
    // 升级过程中不执行
    if ( sys_is_updating() 
        || (can_ota_status.period_cmd_delay > 0)
        || (can_ota_status.ready_timeout > 0))
    {
        return;
    }

    // 仅在“交流输入源”设置为“智能发电机模式”时生效
    if ( 2 != top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.AC_Input_Ctrl.Source_Mode ) {
        return;
    }
    
    // 0：无效；1：智能混动；2：定时；3：手动
    switch ( IotSetData.dev_info_t.Gen_ctrl.ctrl_mode )
    {
        case 1 :
            // 禁用时间段生效，强制关闭
            if (( 1 == IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl ) 
                && ( 1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.Gen_Time_Status2 )) {
                Gen_Enable = 2;
            } else {
                if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.soc < IotSetData.dev_info_t.Gen_ctrl.soc_min ) {
                    Gen_Enable = 1; // 低于SOC阈值，强制开启
                } else if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.soc > IotSetData.dev_info_t.Gen_ctrl.soc_max ) {
                    Gen_Enable = 2; // 高于SOC阈值，强制关闭
                } else {
                    Gen_Enable = 0;
                }
            }
            break;
            
        case 2 :
            if ( 1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.Gen_Time_Status1  ) {
                Gen_Enable = 1; // 启用时间段生效，强制开启
            } else {
                Gen_Enable = 2; // 启用时间段失效，强制关闭
            }
            break;

        case 3 :
            Gen_Enable = 0;
            break;
        
        default:
            return;
    }

    // 检查是否需要更改发电机开关
    if ( Gen_Enable && (Gen_Enable != top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.ctrl_mix.bit.gen_en)) {
        uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if ( (now_time - pre_time) > 10000 ) {
            pre_time = now_time;

            // 下发发电机开关设置
            ctrl_mix_t Config_Set = {0};
            Config_Set.bit.gen_en = Gen_Enable;
            Single_Reg_Set_to_IoT(MD_SUMMARY_ADDR, 2232, Config_Set.all);
        }
    } else {
        pre_time = 0;
    }
}

// 本地数据计算线程定时
static void Data_Process_SysTime(void)
{
    static uint8_t ts_u16Cnt1000ms = 0;
    static uint8_t ts_u16Cnt100ms = 0;

    if(ts_u16Cnt100ms != reals.Timer_100msCount)
    {
        ts_u16Cnt100ms = reals.Timer_100msCount;
        reals.st_FlagTime.bits.b1FlagSys100ms_data_process = 1;
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys100ms_data_process = 0;
    }
    
    if(ts_u16Cnt1000ms != reals.Timer_1sCount)
    {
        ts_u16Cnt1000ms = reals.Timer_1sCount;
        reals.st_FlagTime.bits.b1FlagSys1s_data_process = 1;
    }
    else
    {
        reals.st_FlagTime.bits.b1FlagSys1s_data_process = 0;
    }
}

#define SUPER_CMD_TAG   "Super_Control"

/*------------------------------------------------------------------------------
 Function: Log_Value_Set
 -----------------------------------------------------------------------------*/
/**
  * @brief      日志等级设置
  * @param[in]  bool flag  
  * @param[out] None
  * @return     static void
  */
static void Log_Value_Set(bool flag)
{
    static bool status = false;
    if ( status != flag ) {
        if ( flag ) {
            // 开启日志打印
            esp_log_level_set("*", ESP_LOG_INFO);
            apply_log_levels_from_table();
        } else {
            // 关闭日志打印
            esp_log_level_set("*", ESP_LOG_NONE);
        }
        status = flag;
    }
}

/*------------------------------------------------------------------------------
 Function: Super_Control_Cmd_Process
 -----------------------------------------------------------------------------*/
/**
  * @brief      超级命令处理任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void Super_Control_Cmd_Process(void)
{
    for (uint16_t index = 0; index < SUPPER_CMD_MAX; index++)
    {
        supper_cmd_t *super_cmd = &reals.supper_control.cmd_list[index];
        switch ( index )
        {
            // 重启
            case SUPPER_CMD_RESTART :
                if ( 1 == super_cmd->value ) {
                    super_cmd->value = 0;
                    ESP_LOGW(SUPER_CMD_TAG, "Delay 1s Prepare to restart system....");
                    set_mqtt_stop(); 
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart(); 
                    while(1);
                }
                break;

            // WIFI重连
            case SUPPER_CMD_WIFI_RESTART :
                if ( 1 == super_cmd->value ) {
                    super_cmd->value = 0;                
                    if ((1 == IotSetData.dev_info_t.on_off.bit.wifi_enable) && (1 == IotSetData.dev_info_t.on_off.bit.wifi_sta_enable)) {
                        ESP_LOGW(SUPER_CMD_TAG, "Disconnect and restart WiFi Scan....");
                        if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.wifi_sta ) {
                            // 断开当前使用的WIFI STA
                            iot_wifi_disconnect();
                        }
                        // 重新执行自动连接逻辑
                        iot_wifi_auto_connect();
                    }
                }
                break;

            // 蓝牙重启    
            case SUPPER_CMD_BLE_RESTART :
                if ( 1 == super_cmd->value ) {
                    super_cmd->value = 0;
                    ESP_LOGW(SUPER_CMD_TAG, "BLE Driver restart init....");
                    iot_ble_deinit();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    iot_ble_reinit(); 
                }
                break;

            // CAN驱动重启
            case SUPPER_CMD_CAN_RESTART :
                if ( 1 == super_cmd->value ) {
                    super_cmd->value = 0;
                    ESP_LOGW(SUPER_CMD_TAG, "Can Driver restart init....");
                    esp_canbus_deinit();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_canbus_init();
                }
                break;
                
            // 升级
            case SUPPER_CMD_OTA_START :
                if ( 1 == super_cmd->value ) {
                    super_cmd->value = 0;
#ifdef CONFIG_DEVASSIST_OTA_SUPPORT_ENABLE
                    reals.supper_control.ota_status = DEVASSIST_OTA_READY;
                    reals.need_new_version_flag.sBit.devassist = 1;
                    ESP_LOGW(SUPER_CMD_TAG, "HTTP OTA Trigger Activated!");
#endif
                }
                break;

            // 日志    
            case SUPPER_CMD_LOG_RESOURCE :
                supper_control_resource_t resource = { .value = super_cmd->value };
                
                // 系统log
                if (1 == resource.log) {
                    Log_Value_Set(true);
                } else {
                    Log_Value_Set(false);
                }
                break;

            // CAN LOG触发存储机制
            case SUPPER_CMD_CAN_LOG_SAVE :     
#ifdef AIOT_CAN_LOG_SAVE_ENABLE                
                super_cmd->value = AIoT_Can_Log_Process(super_cmd->value);
#endif
                break;

            // 下列命令非立即生效，此处仅列举表示支持
            case SUPPER_CMD_LOG_DIRECTION :    
            case SUPPER_CMD_SERVER_IP :
            case SUPPER_CMD_SERVER_PORT :
                break;
            
            // 以下命令暂未支持
            case SUPPER_CMD_CONSOLE_LOG :
            case SUPPER_CMD_LOG_CHANNEL :    
            case SUPPER_CMD_LOG_FRAME :
            case SUPPER_CMD_LOCAL_NETIF : 
                if ( 0 != super_cmd->value ) {
                    super_cmd->value = 0;
                    ESP_LOGE(SUPER_CMD_TAG, "Unsupported Cmd (%d)", super_cmd->cmd);
                }
                break;
            
            default:
                if ( 0 != super_cmd->value ) {
                    super_cmd->value = 0;
                    ESP_LOGE(SUPER_CMD_TAG, "Unknown Cmd (%d)", super_cmd->cmd);
                }
                break;
        }
    }
}

/*------------------------------------------------------------------------------
 Function: System_File_Data_Process_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      本地数据计算任务，允许高延时
  * @param[in]  void * pvParameters  
  * @param[out] None
  * @return     void
  */
void System_File_Data_Process_Task (void * pvParameters)
{
    // 任务时间统计声明和初始化
    TASK_TIME_STATS_DECLARE(System_File_Data_Process_Task);

#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
    /*寄存器修改日志模块初始化*/
    Reg_Change_Module_Init();
#endif

#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE    
    /*设备接入日志处理模块初始化*/
    Dev_Access_Module_Init();    
#endif

#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
    /*能量存储模块初始化*/
    Energy_Module_Init();
#endif

#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
    /*智能TOU控制模块初始化*/
    Smart_Tou_Ctrl_Module_Init();
#endif

#ifdef CONFIG_AC_EMS_CLOUD_ENABLE
    /*云端AC_EMS控制模块初始化*/
    Ac_Ems_Cloud_Module_Init();
#endif

#ifdef INV_LOG_DETAILED_INFO_RECORD
    /*黑匣子任务*/
    box_task_init();
#endif

    // 本地数据记录初始化
    history_queue_init();

#ifdef CONFIG_MCU_AUTO_UPDATE_IN_BOOT
    // 备份模块初始化
    ImageBackInit();
#endif

#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
    // 支持时间段控制
    Smart_Tou_Ctrl_Time_Quantum_Enable_Set(true);

    /*注册/设置设备寄存器访问回调并指定检查时间间隔（毫秒）*/
    device_reg_cb_set(Smart_Tou_Modbus_Reg_Value_Get, (60*1000));
#endif

#ifdef CONFIG_IOT_INTERNAL_TEMPERATURE_GET_ENABLE
    /*首次运行执行初始化*/
    IoT_Internal_Temperature_Update();
#endif

#ifdef CONFIG_OFF_GRID_LOAD_POWER_DISTRIBUTION
    /*离网负载功率模块初始化*/
    off_grid_load_power_init();
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
    /*云边参数同步模块初始化*/
    CloudSyncInit();
#endif

#ifdef CONFIG_GRID_OFF_COUNT_ENABLE     
    /*断电统计模块初始化*/
    Grid_Off_Statistic_Init();
#endif

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
    /*MQTT登录日志模块初始化*/
    Mqtt_Log_Module_Init();
#endif

    while ( 1 )
    {
        vTaskDelay(pdMS_TO_TICKS(50));

        // 开始时间统计
        TASK_TIME_START(System_File_Data_Process_Task);  

        // 本地数据计算线程定时
        Data_Process_SysTime();

        /*IOT设置检查*/
        iot_cfg_check();

        /*系统设置检查*/
        parameter_save(PARAM_TYPE_ALL);

        // 上电后10s暂不执行，等待系统稳定
        if( reals.MCUPoweronCnt < 10 ) {
            continue;
        }
            
        if ( 1 == reals.st_FlagTime.bits.b1FlagSys100ms_data_process )
        {
            /*时间有效时才执行*/
            if (1 == reals.rtc_flag.sBit.Time_valid)
            {                 
                /*事件检查*/
                event_check();

                // 升级期间不执行
                if ( !sys_is_updating() )
                {
                    /*INV历史记录检查*/
                    dev_log_check();
                    
#ifdef CONFIG_GRID_OFF_COUNT_ENABLE            
                    /* 电网断电次数统计 */
                    Grid_Off_Event_Task();
#endif
                    
#ifdef INV_LOG_DETAILED_INFO_RECORD
                    /*黑匣子记录写入*/
                    Inv_Detailed_Info_Log_Pop();
#endif
                }
            }
        }

        if ( 1 == reals.st_FlagTime.bits.b1FlagSys1s_data_process )            
        {
#if CONFIG_LOG_UDP_REDIRECT == 2	
            /*超级命令处理逻辑*/
            Super_Control_Cmd_Process();
#endif

#ifdef CONFIG_MCU_AUTO_UPDATE_IN_BOOT
            /*下级MCU滞留boot检查并自动升级逻辑*/                            
            MCU_Slave_Update_Check_In_Boot();
#endif        

#ifdef CONFIG_SMART_GENERATOR_ENABLE
            /*智能发电机控制任务*/
            Smart_Generator_Ctrl_Process();
#endif

#ifdef CONFIG_IOT_INTERNAL_TEMPERATURE_GET_ENABLE
            // 周期更新模组内温度
            IoT_Internal_Temperature_Update();
#endif

#ifdef CONFIG_OFF_GRID_LOAD_POWER_DISTRIBUTION
    #ifdef OFF_GRID_LOAD_POWER_TEST
            if( reals.MCUPoweronCnt % 10 == 0 ) {
                /*模块非阻塞自测*/
                off_grid_load_power_self_test();
            }
    #else
            /*离网负载功率主调度*/
            off_grid_load_power_schedule();
    #endif
#endif

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
            /*MQTT登录日志*/
            Mqtt_Log_Save_Task();
#endif
            /*时间有效时才执行*/
            if (1 == reals.rtc_flag.sBit.Time_valid)
            {            
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
                /*寄存器修改日志检查*/
                Reg_Change_Log_Save_Task();
#endif        
                // 升级期间不执行
                if ( !sys_is_updating() )
                {
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE   
                    /*设备接入记录*/
                    Dev_Access_Log_Save_Task();
#endif   
            
#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
                    /*能量数据文件主处理任务*/
                    Energy_File_Data_Process();
#endif

#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
                    // 智能TOU控制模块周期处理函数
                    Smart_Tou_Ctrl_Module_Process();
#endif

#ifdef CONFIG_AC_EMS_CLOUD_ENABLE
                    // 云端AC_EMS控制模块周期处理函数
                    Ac_Ems_Cloud_Module_Process();
#endif

#ifdef CONFIG_RELAY_CTRL_ENABLE
                    // TOU控制模块周期处理函数
                    Relay_Ctrl_Main_Task();
#endif

#if CONFIG_DISASTER_WARN_CHANNEL == 1
                    // 灾害预警备电任务
                    disater_warn_process_task();
#elif CONFIG_DISASTER_WARN_CHANNEL == 2            
                    // 风暴状态更新
                    DisasterJudge();
                    thunder_state_update(); 
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
                    // 本地参数更新记录存储
                    cloud_sync_record_process();
#endif
                }
            }
        }

        // 结束时间统计并打印
        TASK_TIME_END(System_File_Data_Process_Task);
        TASK_TIME_PRINT(System_File_Data_Process_Task, 20); // 每20次循环打印一次
        TASK_TIME_RUNS_MAX_TO_FIELD(System_File_Data_Process_Task, reals.thread6_max_runtime_us);
        TASK_TIME_TIMEOUT_CLEAN(System_File_Data_Process_Task, TASK_COMMON_CLEAN_TIMEOUT_MS);
    }
}

/*------------------------------------------------------------------------------
 Function: modem_4g_task_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      4G模块初始化配置
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void modem_4g_task_init(void)
{
#ifdef CONFIG_USB_4G_MODULE_ENABLE

    /*配置用户自定义APN*/
    iot_4g_set_apn(IotSetData.dev_info_t.mobile_apn);
    
    /*4G开关检查*/
    if (1 == IotSetData.dev_info_t.on_off.bit._4g_enable) { 	
        iot_4G_start();
    }

#endif
}

/**
  * @brief 主任务，对时间要求不高的周期任务
  * @note 不允许延时
  * @param[in]  pvParameters
  */
void iot_period_slow_task(void * pvParameters)
{
    // 任务时间统计声明和初始化
    TASK_TIME_STATS_DECLARE(iot_period_slow_task);

    static uint32_t debugcnt_100ms = 0;
    static uint32_t debugcnt_1s = 0;
	static uint8_t remain_report_cnt = 0;

    /*WIFI初始化*/
    wifi_task_init();

#ifdef CONFIG_USB_4G_MODULE_ENABLE
    /*4G模块初始化*/
    modem_4g_task_init();
#endif

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(50));

        // 开始时间统计
        TASK_TIME_START(iot_period_slow_task);

        // 线程定时
        App_Slow_Task_SysTime();

#ifdef CONFIG_WLCC_ENABLE
        /*UDP通讯任务*/
        wlcc_task();
#endif

#ifdef CONFIG_MODBUS_REG_TLV_ENABLE
        // 处理 modbus TLV 接收队列中的消息
        modbus_tlv_process_task();
#endif

#ifdef CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
        // 数据通道接收处理任务
        udt_recv_process_task();
#endif

        if (1 == reals.st_FlagTime.bits.b1FlagSys100ms_slow_period_task)
        {			
#ifdef CONFIG_CAN_PORT_ENABLE
            /*汇总设备数据*/
            iot_sumdev_task();
#endif

            /*最新时间设置检查（APP）*/
            time_new_set_check();

#ifdef CONFIG_FILE_DATA_OTA_ENABLE
            /*文件升级进度处理*/
            File_OTA_Status_Update();
#endif

            /*WIFI状态机*/
            iot_wifi_State_Machine();
    
#ifdef CONFIG_USB_4G_MODULE_ENABLE
            /*USB 4G模块状态机*/
            Usb_4G_Module_State_Machine();
#endif

#ifdef CONFIG_MODBUS_21000_USE_ENABLE
            /*21000段主动上报(发送至队列，在上报任务中发送)*/
            Modbus_Self_21000_Report_Ble();
            Modbus_Self_21000_Report_MQTT();

            /*21000段设备绑定解析*/
            Modbus_21000_bind_msg_process();
#endif

#ifdef CONFIG_BLE_ADV_ENABLE
            /*蓝牙广播更新*/
            ble_adv_update();
#else
            /*蓝牙广播状态检查*/
            ble_adv_status_check();
#endif

            /*绑定帧主动上报*/
            if(reals.net_point_Comein == 1) {
                reals.net_point_Comein = 0;
                remain_report_cnt = 1;  //指定上报次数
            }
            
            // 当前数据刷新时开启蓝牙和MQTT立即上报，在4g网络模式下mqtt仅支持在收到modbus读指令时会主动上报（节省流量）
            if ( remain_report_cnt > 0 ) {
                remain_report_cnt--;
                reals.modbus_self_report_ble = 1;
                if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.MqttConnectMode != 3 ) {
                    reals.modbus_self_report_mqtt = 1;
                }
            }

#ifdef UART_PORT_ENABLE
            /*mqtt通道处理*/
            uart_to_mqtt_queue_task();
#endif

#if CONFIG_LED_CONTROL
            iot_led_update(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.line_event.bit.Grid_online, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.line_event.bit.PV_online);
#endif

#ifdef BLE_CLIENT_ENABLE	
			ble_client_task();
#endif
 
#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
            // IF-THEN智能控制数据更新
            Smart_Tou_Ctrl_Data_Update();
#endif

#ifdef CONFIG_AC_EMS_CLOUD_ENABLE
            // 云端AC-EMS智能控制数据更新
            Ac_Ems_Cloud_Data_Update();
#endif

#ifdef CONFIG_USB_HOST_HID_ENABLE
            // 更新键盘数据
            keyboard_data_update();
#endif

            if (++debugcnt_100ms >= 5)
            {
                debugcnt_100ms = 0;
            
                /* IOT调试模式 */
                iot_debug_period_process();
                
                /* IOT测试模式 */
                iot_test_period_process();
            }
        }

        if (1 == reals.st_FlagTime.bits.b1FlagSys1s_slow_period_task) // 1s
        {
#ifdef CONFIG_MODBUS_21000_USE_ENABLE        
            /*检查所有设备状态*/
            Dev_All_Point_State_Check();
#endif

#ifdef CONFIG_CAN_PORT_ENABLE
            /*检测CAN总线状态并切换*/
            esp_canbus_state_change();
#endif

#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
            /*Alpha协议周期任务*/            
            bms_can_send_task_1s();
#endif

            /*版本检查超时清除*/
            https_new_image_timeout_clean();

#ifdef CONFIG_RELAY_CTRL_ENABLE
            /*TOU工作参数更新*/
            Relay_Ctrl_Parameter_update(reals.rtc_flag.sBit.Time_valid, top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1.soc);
#endif

#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
            /*IOT能量数据更新*/
            iot_energy_data_update();
#endif

#ifdef INV_LOG_DETAILED_INFO_RECORD
            // 黑匣子周期数据更新
            Inv_Detailed_Info_update();
#endif
            
#ifndef MASS_PRODUCTION_CONFIG_ENABLE
            if (++debugcnt_1s >= 10)
            {
                debugcnt_1s = 0;

                print_twai_status();
                log_task_stack_highwater();
                heap_info_check();
                // print_socket_count();
                // tasks_info();
                print_mem_info();   
                // ESP_LOGI(TAG, "GPIO pin %d level = %d", WAVE_DI_GPIO, gpio_get_level(WAVE_DI_GPIO));
            }
#endif
        }

#ifdef CONFIG_IOT_AUTO_LIGHT_SLEEP_ENABLE
        if ( 1 == reals.IOT_Status_Flag.sBit.system_sleep_flag ) {
            // 创建休眠任务
            Sleep_Task_Create();
        }
#endif

        // 结束时间统计并打印
        TASK_TIME_END(iot_period_slow_task);
        TASK_TIME_PRINT(iot_period_slow_task, 100); // 每20次循环打印一次
        TASK_TIME_RUNS_MAX_TO_FIELD(iot_period_slow_task, reals.thread5_max_runtime_us);
        TASK_TIME_TIMEOUT_CLEAN(iot_period_slow_task, TASK_COMMON_CLEAN_TIMEOUT_MS);
    }
}

/*------------------------------------------------------------------------------
 Function: iot_period_task_by_sleep
 -----------------------------------------------------------------------------*/
/**
  * @brief      休眠状态下备用主任务，与实际主任务互斥
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_period_task_by_sleep(void)
{
    static uint32_t pre_time1 = 0;
    static uint32_t pre_time2 = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /*系统DO唤醒检查*/
    system_do_wakeup_check();
    
    /*IOT被唤醒检查*/
    system_be_wakened_check();
    
    if((now_time - pre_time1) >= 100)
    {
        pre_time1 = now_time;
        
        /*设备状态更新*/
        iot_about_update();

        /*IOT连接状态及设置检查*/
        iot_link_status_check();
        
        /*更新自身在线状态*/
        system_status_update();

        /*系统关机及休眠状态检查*/
        system_off_or_sleep_check();

        /*IOT设置检查*/
        iot_cfg_check();

        /*WIFI状态机*/
        iot_wifi_State_Machine();
        
#ifdef CONFIG_USB_4G_MODULE_ENABLE
        /*USB 4G模块状态机*/
        Usb_4G_Module_State_Machine();
#endif

#if defined(CONFIG_BLE_ADV_ENABLE) && !defined(SLEEP_NIMBLE_DEINIT_ENABLE)
        /* BLE广播内容交替更新 */
        ble_adv_update();
#elif !defined(SLEEP_NIMBLE_DEINIT_ENABLE)
        /*蓝牙广播状态检查*/
        ble_adv_status_check();
#endif
    }

    if((now_time - pre_time2) >= 100)
    {
        pre_time2 = now_time;
        
#if 0
        /*调试打印*/
        tasks_info();
#endif

        /*更新本机时间*/
        if (0 == (++reals.MCUPoweronCnt % 86400)) {
            time_sync_system_to_local(1);
        } else {
            time_sync_system_to_local(0);
        }
    }
}

/*------------------------------------------------------------------------------
 Function: iot_cloud_process_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      物联网处理逻辑
  * @param[in]  void * pvParameters  
  * @param[out] None
  * @return     void
  */
void iot_cloud_process_task(void * pvParameters) 
{
    // 任务时间统计声明和初始化
    TASK_TIME_STATS_DECLARE(iot_cloud_process_task);

    /*MQTT初始化*/
    mqtt_task_init();

#ifdef CONFIG_OPEN_MQTT_ENABLE
    /*MQTT开放服务器初始化*/
    open_mqtt_task_init();
#endif

#ifdef CONFIG_WEBSERVER_ENABLE
    /* 本地Web服务器初始化*/
    web_server_start();
#endif

#ifdef CONFIG_ENABLE_MD_TCP_SLAVE
	/* Modbus TCP初始化*/
    modbus_tcp_start(IotSetData.dev_info_t.modbus_tcp_enable.crypt_en, IotSetData.dev_info_t.modbus_tcp_port, 0);
#endif
    
    while(1) 
	{
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // 开始时间统计
        TASK_TIME_START(iot_cloud_process_task);    
        
        /*MQTT状态机*/
        iot_mqtt_State_Machine();

#ifdef CONFIG_OPEN_MQTT_ENABLE        
        /*开放MQTT状态机*/
        open_mqtt_State_Machine();
#endif

        /*MQTT上报任务*/
        iot_mqtt_actively_report_task();

#ifdef CONFIG_WEBSERVER_ENABLE
        /* webserver */
        web_server_task();
#endif

#ifdef CONFIG_ENABLE_MD_TCP_SLAVE
        /* Modbus TCP slave */
        modbus_tcp_task();
#endif

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
        wl_mesh_task();
#endif

#ifdef CONFIG_UDT_PERIOD_INCREMENTAL_ANNOUNCE_ENABLE
        if (( 1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.mqtt )
            && ( 3 != top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.MqttConnectMode )
            && ( 0 == reals.IOT_Status_Flag.sBit.system_sleep_flag )
            && ( 1 == reals.rtc_flag.sBit.Time_valid ))
        {
            /*数据通道周期增量上报使能*/
            udt_period_check_and_report();
        }
#endif

        // MQTT周期检查上报任务
        iot_mqtt_period_check_and_report();

        // 结束时间统计并打印
        TASK_TIME_END(iot_cloud_process_task);
        TASK_TIME_PRINT(iot_cloud_process_task, 100); // 每100次循环打印一次
        TASK_TIME_RUNS_MAX_TO_FIELD(iot_cloud_process_task, reals.thread3_max_runtime_us);
        TASK_TIME_TIMEOUT_CLEAN(iot_cloud_process_task, TASK_COMMON_CLEAN_TIMEOUT_MS);
    }
}


