#include "can_data.h"
#include "esp_log.h"
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "crc.h"
#include "comm_define.h"
// #include "uart_device_process.h"
#include "can_protocol.h"
#include "udt_transfer.h"
#include "sync_time.h"
#include "inv_define.h"
#include "sync_time.h"
// #include "bms_type.h"
#include "nvs_store.h"

#include "can_type.h"
#include "modbus_data.h"
#include "iot_define.h"
#include "parameter.h"
#include "reg_change_log.h"
#include "dev_access_log.h"
#include "bms_can.h"

#define TAG "[CAN_DATA]"


#ifdef CONFIG_CAN_PORT_ENABLE

extern void iot_ble_new_name(const char *dev_type, uint64_t dev_sn);
extern void iot_mqtt_new_dev(const char *dev_type, uint64_t dev_sn);

void Inv_Single_Data_Can_to_Modbus(MOD_STRUCT_Inv *Inv_Modbus_Data, can_node_struct_Inv *Inv_Can_Data, uint8_t type);

/*
CAN 写 和 modbus写 ，使用相同标志，统一到同一执行位置
*/
void Can_iot_callback_set_type_0x2(uint16_t write_offset, uint16_t data_len)
{
	uint32_t address_begin = (uint32_t)&can_node_wr.Iot.mod_reg12000_IOT_set + write_offset;
	uint32_t address_end = (uint32_t)&can_node_wr.Iot.mod_reg12000_IOT_set + write_offset + data_len;
	iot_can_node_struct_reg12000 *Ponter;

	Ponter = &can_node_wr.Iot.mod_reg12000_IOT_set;

	if((0 == write_offset)
		&&((sizeof(iot_can_node_struct_reg12000_mini)*2) == data_len))//如果收到其他设备的因SN变化的IOT参数同步，则本机不在主动同步（WR）
	{
		reals.T_delay_iot_para_can_wr =-1;

	}

	if((address_begin <= (uint32_t)&Ponter->factory)
		&&(address_end > (uint32_t)&Ponter->factory))
	{
		reals.ModbusCmdFlag.sBit.iot_factory_cmd = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.factory, (uint8_t *)&Ponter->factory, sizeof(Ponter->factory));
	}

	if((address_begin <= (uint32_t)&Ponter->wifi_sta_ssid)
		&&(address_end > (uint32_t)&Ponter->wifi_sta_ssid))
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid, (uint8_t *)&Ponter->wifi_sta_ssid, sizeof(Ponter->wifi_sta_ssid));
		
	}
	if((address_begin <= (uint32_t)&Ponter->wifi_sta_password)
		&&(address_end > (uint32_t)&Ponter->wifi_sta_password))
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_password = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password , (uint8_t *)&Ponter->wifi_sta_password, sizeof(Ponter->wifi_sta_password));
		
	}

	if((address_begin <= (uint32_t)&Ponter->wifi_sta_auth)
		&&(address_end > (uint32_t)&Ponter->wifi_sta_auth))
	{
		reals.ModbusCmdFlag.sBit.wifi_sta_auth = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_auth , (uint8_t *)&Ponter->wifi_sta_auth, sizeof(Ponter->wifi_sta_auth));
		
	}


	if((address_begin <= (uint32_t)&Ponter->could_dns[0])
		&&(address_end > (uint32_t)&Ponter->could_dns[0]))
	{
		reals.ModbusCmdFlag.sBit.could_dns = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.could_dns[0] , (uint8_t *)&Ponter->could_dns[0], sizeof(Ponter->could_dns[0]));
		
	}
	if((address_begin <= (uint32_t)&Ponter->mobile_apn[0])
		&&(address_end > (uint32_t)&Ponter->mobile_apn[0]))
	{
		reals.ModbusCmdFlag.sBit.mobile_apn = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.mobile_apn[0] , (uint8_t *)&Ponter->mobile_apn[0], sizeof(Ponter->mobile_apn[0]));
		
	}
	if((address_begin <= (uint32_t)&Ponter->on_off)
		&&(address_end > (uint32_t)&Ponter->on_off))
	{
		reals.ModbusCmdFlag.sBit.on_off = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.on_off , (uint8_t *)&Ponter->on_off, sizeof(Ponter->on_off));
        ESP_LOGW(TAG, "Can set on-off config: %08lx", Ponter->on_off.all);
	}
	if((address_begin <= (uint32_t)&Ponter->thunder_ctrl)
		&&(address_end > (uint32_t)&Ponter->thunder_ctrl))
	{
		reals.ModbusCmdFlag.sBit.thunder_ctrl = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.thunder_ctrl , (uint8_t *)&Ponter->thunder_ctrl, sizeof(Ponter->thunder_ctrl));
		
	}
	if((address_begin <= (uint32_t)&Ponter->period_report)
		&&(address_end > (uint32_t)&Ponter->period_report))
	{
		reals.ModbusCmdFlag.sBit.period_report = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.period_report , (uint8_t *)&Ponter->period_report, sizeof(Ponter->period_report));
		
	}
	if((address_begin <= (uint32_t)&Ponter->IOT_Enable_mix1)
		&&(address_end > (uint32_t)&Ponter->IOT_Enable_mix1))
	{
		reals.ModbusCmdFlag.sBit.IOT_Enable_mix1 = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.IOT_Enable_mix1 , (uint8_t *)&Ponter->IOT_Enable_mix1, sizeof(Ponter->IOT_Enable_mix1));
		
	}
	if((address_begin <= (uint32_t)&Ponter->IOT_Enable_mix2)
		&&(address_end > (uint32_t)&Ponter->IOT_Enable_mix2))
	{
		reals.ModbusCmdFlag.sBit.IOT_Enable_mix2 = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.IOT_Enable_mix2 , (uint8_t *)&Ponter->IOT_Enable_mix2, sizeof(Ponter->IOT_Enable_mix2));
		
	}
	if((address_begin <= (uint32_t)&Ponter->Protocol_3r_Enable_mix1)
		&&(address_end > (uint32_t)&Ponter->Protocol_3r_Enable_mix1))
	{
		reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1 , (uint8_t *)&Ponter->Protocol_3r_Enable_mix1, sizeof(Ponter->Protocol_3r_Enable_mix1));
		
	}
	if((address_begin <= (uint32_t)&Ponter->Protocol_3r_Enable_mix2)
		&&(address_end > (uint32_t)&Ponter->Protocol_3r_Enable_mix2))
	{
		reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix2 , (uint8_t *)&Ponter->Protocol_3r_Enable_mix2, sizeof(Ponter->Protocol_3r_Enable_mix2));
		
	}
	if((address_begin <= (uint32_t)&Ponter->Time_Span)
		&&(address_end > (uint32_t)&Ponter->Time_Span))
	{
		reals.ModbusCmdFlag.sBit.Time_Span = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span , (uint8_t *)&Ponter->Time_Span, sizeof(Ponter->Time_Span));
	}
	if((address_begin <= (uint32_t)&Ponter->LCD_Mode)
		&&(address_end > (uint32_t)&Ponter->LCD_Mode))
	{
		reals.ModbusCmdFlag.sBit.LCD_Mode = 1;
		memcpy((uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode , (uint8_t *)&Ponter->LCD_Mode, sizeof(Ponter->LCD_Mode));
	}
}

/*------------------------------------------------------------------------------
 Function: update_online_counts
 -----------------------------------------------------------------------------*/
/**
  * @brief      更新在线设备数
  * @param[in]  int temp_iot_online_cnt    
                int temp_inv_online_cnt    
                int temp_pack_online_cnt   
                int temp_achub_online_cnt  
                int temp_dchub_online_cnt  
                int temp_panel_online_cnt  
  * @param[out] None
  * @return     void
  */
void update_online_counts(int temp_iot_online_cnt, int temp_inv_online_cnt, int temp_pack_online_cnt, 
                                    int temp_achub_online_cnt, int temp_dchub_online_cnt, int temp_dcdc_online_cnt) 
{
    // 保存旧值
    int old_online_Iot_num = reals.online_Iot_num;
    int old_online_Inv_num = reals.online_Inv_num;
    int old_online_Pack_num = reals.online_Pack_num;
    int old_online_ACHUB_num = reals.online_ACHUB_num;
    int old_online_DCHUB_num = reals.online_DCHUB_num;
    int old_online_DCDC_num = reals.online_Dcdc_num;

    // 更新值
    reals.online_Iot_num = temp_iot_online_cnt + 1; // 包括自己
    reals.online_Inv_num = temp_inv_online_cnt;
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE    
    reals.online_Pack_num = get_bat_data_ptr()->all_data.bat_num;
#endif    
#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE    
    reals.online_Pack_num = temp_pack_online_cnt;
#endif
    reals.online_ACHUB_num = temp_achub_online_cnt;
#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    reals.online_DCHUB_num = temp_dchub_online_cnt;
#endif
    reals.online_Dcdc_num = temp_dcdc_online_cnt;

    // 检查是否有值发生变动
    bool changed = (reals.online_Iot_num != old_online_Iot_num) ||
                   (reals.online_Inv_num != old_online_Inv_num) ||
                   (reals.online_Pack_num != old_online_Pack_num) ||
                   (reals.online_ACHUB_num != old_online_ACHUB_num) ||
                   (reals.online_DCHUB_num != old_online_DCHUB_num) || 
                   (reals.online_Dcdc_num != old_online_DCDC_num);

    // 设置标志
    if (changed) {
        reals.net_point_Comein = 1;
        
#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5       
        Iot_EMS_Ctrl_Func_5_Send(0, 0, 0);
#endif
    } 
}


void modbus_inv_value_clean(uint8_t num) 
{
	//INV
	MOD_STRUCT_Inv *Inv = &top_modbus_rd.Inv[num];
	
	memset(&Inv->mod_reg00100_AppPage1.line_event, 0x00, (offsetof(MOD_STRUCT_reg00100, DCLoadTotalEnergy) - offsetof(MOD_STRUCT_reg00100, line_event))); // 清除首页实时数据123~150

	memset(&Inv->mod_reg01100_Inv_base, 0x00, sizeof(MOD_STRUCT_reg01100));
	memset(&Inv->mod_reg01200_Inv_pv, 0x00, sizeof(MOD_STRUCT_reg01200));

	memset(&Inv->mod_reg01300_Inv_grid, 0x00, sizeof(MOD_STRUCT_reg01300));
	memset(&Inv->mod_reg01400_Inv_load, 0x00, sizeof(MOD_STRUCT_reg01400));
	memset(&Inv->mod_reg01500_Inv_inv, 0x00, sizeof(MOD_STRUCT_reg01500));
	memset(&Inv->mod_reg01600_Inv_generator, 0x00, sizeof(MOD_STRUCT_reg01600));
	memset(&Inv->mod_reg02000_Inv_base_set, 0x00, sizeof(MOD_STRUCT_reg02000));

	memset(&Inv->mod_reg02200_Inv_advance_set, 0x00, sizeof(MOD_STRUCT_reg02200));
	memset(&Inv->mod_reg02400_Inv_certification, 0x00, sizeof(MOD_STRUCT_reg02400));
	memset(&Inv->mod_reg03500_Inv_yearX_statistic, 0x00, sizeof(MOD_STRUCT_reg03500));
	memset(&Inv->mod_reg03600_Inv_year1_statistic, 0x00, sizeof(MOD_STRUCT_reg03600));
	memset(&Inv->mod_reg40000_transparent, 0x00, sizeof(MOD_STRUCT_reg40000));
}
									
/*------------------------------------------------------------------------------
 Function: CanNodeOfflineCheck
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN总线节点在线数量及超时状态更新
  * @param[in]  uint16_t interval  
  * @param[out] None
  * @return     void
  */
void CanNodeOfflineCheck(uint16_t interval) 
{
	uint8_t temp_inv_online_cnt = 0;
	uint8_t temp_pack_online_cnt = 0;
	uint8_t temp_achub_online_cnt = 0;
	uint8_t temp_iot_online_cnt = 0;
	uint8_t temp_dchub_online_cnt = 0;
	uint8_t temp_dcdc_online_cnt = 0;

    /*升级时候，保持最后的设备在线数量，不更新*/
	if(sys_is_updating() 
        || (can_ota_status.ready_timeout > 0)) return;

#ifdef CONFIG_SYSTEM_SLEEP_CAN_SILENT_ENABLE
    /*休眠期间不更新*/
    if(1 == reals.IOT_Status_Flag.sBit.system_sleep_flag) return;
#endif

    //inv 超时判断
    for (uint8_t i = 0; i < (INV_MAX_NUM); i++) 
    {
        if((can_node_rd.Inv[i].online == 1))
        {
            // 设备基础信息有效时才做计数
            if ((0 != strlen(can_node_rd.Inv[i].inv_about.dev_type)) 
                && (0 != can_node_rd.Inv[i].inv_about.dev_sn))
            {
                temp_inv_online_cnt++;

                // 生成设备接入记录
                if((can_node_rd.Inv[i].dev_access_state == 0) 
                    && (1 == reals.rtc_flag.sBit.Time_valid)) {
                    can_node_rd.Inv[i].dev_access_state = 1;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                    // 更新记录参数
                    dev_access_params_t record = {0};
                    record.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
                    record.parent_address = 0xFF; // 无效
                    record.local_address = INV_CAN_ADDR + i;
                    record.operation_attribute = DEVICE_EVENT_OP_AUTO_ADD;
                    record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                    record.identifier_info.sn_info.dev_type = can_node_rd.Inv[i].inv_about.dev_type;
                    record.identifier_info.sn_info.dev_sn = &can_node_rd.Inv[i].inv_about.dev_sn;

                    // 生成记录到队列
                    Dev_Access_Log_Generate(&record);
#endif                
                    reals.need_new_version_flag.sBit.inv = 1;
                }
            }
                
            if (can_node_rd.Inv[i].alive_time >= interval) 
            {
                can_node_rd.Inv[i].alive_time -= interval;
            } 
            else 
            {
                if(can_node_rd.Inv[i].dev_access_state == 1) {
                    // 生成设备接入记录
                    can_node_rd.Inv[i].dev_access_state = 0;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE                           
                    // 更新记录参数
                    dev_access_params_t record = {0};
                    record.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
                    record.parent_address = 0xFF; // 无效
                    record.local_address = INV_CAN_ADDR + i;
                    record.operation_attribute = DEVICE_EVENT_OP_TIMEOUT_OFFLINE;
                    record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                    record.identifier_info.sn_info.dev_type = can_node_rd.Inv[i].inv_about.dev_type;
                    record.identifier_info.sn_info.dev_sn = &can_node_rd.Inv[i].inv_about.dev_sn;
                    
                    // 生成记录到队列
                    Dev_Access_Log_Generate(&record);
#endif     
                }
                
                can_node_rd.Inv[i].alive_time = 0;
                can_node_rd.Inv[i].online = 0;
                memset(&can_node_rd.Inv[i], 0x00, sizeof(can_node_rd.Inv[i]));				
				modbus_inv_value_clean(i);
                ESP_LOGE(TAG, "inv	offline");
            }
        }
    }

#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE

    //pack 超时判断
    for (uint8_t i = 0; i < (PACK_MAX_NUM); i++) 
    {
        if((can_node_rd.Pack[i].online == 1))
        {
            // 设备基础信息有效时才做计数
            if ((0 != strlen(can_node_rd.Pack[i].pack_about.type_ascii)) 
                // && (0 != can_node_rd.Pack[i].pack_about.sn_code) 可能存在电池包SN为零
                )
            {
                temp_pack_online_cnt++;
                      
                // 生成设备接入记录
                if((can_node_rd.Pack[i].dev_access_state == 0) 
                    && (1 == reals.rtc_flag.sBit.Time_valid)) {
                    can_node_rd.Pack[i].dev_access_state = 1;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE     
                    // 更新记录参数
                    dev_access_params_t record = {0};
                    record.record_type = DEVICE_EVENT_REC_TYPE_DEV_FROM_CAN;
                    record.parent_address = 0xFF; // 无效
                    record.local_address = i + PACK_CAN_ADDR;
                    record.operation_attribute = DEVICE_EVENT_OP_AUTO_ADD;
                    record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                    record.identifier_info.sn_info.dev_type = can_node_rd.Pack[i].pack_about.type_ascii;
                    record.identifier_info.sn_info.dev_sn = &can_node_rd.Pack[i].pack_about.sn_code;

                    // 生成记录到队列
                    Dev_Access_Log_Generate(&record);
#endif 
                }
            }
                            
            if (can_node_rd.Pack[i].alive_time >= interval) 
            {
                can_node_rd.Pack[i].alive_time -= interval;
            } 
            else 
            {
                if(can_node_rd.Pack[i].dev_access_state == 1) {
                    // 生成设备接入记录
                    can_node_rd.Pack[i].dev_access_state = 0;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                    // 更新记录参数
                    dev_access_params_t record = {0};
                    record.record_type = DEVICE_EVENT_REC_TYPE_DEV_FROM_CAN;
                    record.parent_address = 0xFF; // 无效
                    record.local_address = i + PACK_CAN_ADDR;
                    record.operation_attribute = DEVICE_EVENT_OP_TIMEOUT_OFFLINE;
                    record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                    record.identifier_info.sn_info.dev_type = can_node_rd.Pack[i].pack_about.type_ascii;
                    record.identifier_info.sn_info.dev_sn = &can_node_rd.Pack[i].pack_about.sn_code;
                    
                    // 生成记录到队列
                    Dev_Access_Log_Generate(&record);
#endif             
                }
                
                can_node_rd.Pack[i].alive_time = 0;
                can_node_rd.Pack[i].online = 0;
                memset(&can_node_rd.Pack[i], 0x00, sizeof(can_node_rd.Pack[i]));
                memset(&top_modbus_rd.Pack[i], 0x00, sizeof(top_modbus_rd.Pack[i]));
                ESP_LOGE(TAG, "Pack offline");
            }
        }
    }

#endif

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE

    //dchub 超时判断
    for (uint8_t i = 0; i < (DC_HUB_MAX_NUM); i++) 
    {
        if((can_node_rd.DCHUB[i].online == 1))
        {
            // 设备基础信息有效时才做计数
            if ((0 != can_node_rd.DCHUB[i].dc_hub_info.DCHubSN)
                && (0 != can_node_rd.DCHUB[i].dc_hub_info.SoftVersion))
            {
                temp_dchub_online_cnt++;

                // 生成设备接入记录
                if((can_node_rd.DCHUB[i].dev_access_state == 0) 
                    && (1 == reals.rtc_flag.sBit.Time_valid)) {
                    can_node_rd.DCHUB[i].dev_access_state = 1;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                    // 更新记录参数
                    dev_access_params_t record = {0};
                    record.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
                    record.parent_address = 0xFF; // 无效
                    record.local_address = DC_HUB_CAN_ADDR + i;
                    record.operation_attribute = DEVICE_EVENT_OP_AUTO_ADD;
                    record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                    record.identifier_info.sn_info.dev_type = SN_TYPE_DCHUB_ASCII;
                    record.identifier_info.sn_info.dev_sn = &can_node_rd.DCHUB[i].dc_hub_info.DCHubSN;

                    // 生成记录到队列
                    Dev_Access_Log_Generate(&record);
#endif   
                    reals.need_new_version_flag.sBit.dchub = 1;
                }             
            }
                
            if (can_node_rd.DCHUB[i].alive_time >= interval) 
            {
                can_node_rd.DCHUB[i].alive_time -= interval;
            } 
            else 
            {
                if(can_node_rd.DCHUB[i].dev_access_state == 1) {
                    // 生成设备接入记录
                    can_node_rd.DCHUB[i].dev_access_state = 0;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                    // 更新记录参数
                    dev_access_params_t record = {0};
                    record.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
                    record.parent_address = 0xFF; // 无效
                    record.local_address = DC_HUB_CAN_ADDR + i;
                    record.operation_attribute = DEVICE_EVENT_OP_TIMEOUT_OFFLINE;
                    record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                    record.identifier_info.sn_info.dev_type = SN_TYPE_DCHUB_ASCII;
                    record.identifier_info.sn_info.dev_sn = &can_node_rd.DCHUB[i].dc_hub_info.DCHubSN;
                    
                    // 生成记录到队列
                    Dev_Access_Log_Generate(&record);
#endif     
                }
                
                can_node_rd.DCHUB[i].alive_time = 0;
                can_node_rd.DCHUB[i].online = 0;
                memset(&can_node_rd.DCHUB[i], 0x00, sizeof(can_node_rd.DCHUB[i]));              
                memset(&top_modbus_rd.DcHub[i], 0x00, sizeof(top_modbus_rd.DcHub[i]));
                ESP_LOGE(TAG, "dchub offline");
            }
        }
    }
    
#endif

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
    
        //DCDC 超时判断
        for (uint8_t i = 0; i < (DCDC_MAX_NUM); i++) 
        {
            if((can_node_rd.Dcdc[i].online == 1))
            {
                // 设备基础信息有效时才做计数
                if ((can_node_rd.Dcdc[i].dcdc_common_info.dev_sn > 0xFF)
                    && (0 != strlen(can_node_rd.Dcdc[i].dcdc_common_info.dev_type))
                    && (0 != can_node_rd.Dcdc[i].dcdc_common_info.dcdc_SoftwareVersion))
                {
                    temp_dcdc_online_cnt++;
    
                    // 生成设备接入记录
                    if((can_node_rd.Dcdc[i].dev_access_state == 0) 
                        && (1 == reals.rtc_flag.sBit.Time_valid)) {
                        can_node_rd.Dcdc[i].dev_access_state = 1;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                        // 更新记录参数
                        dev_access_params_t record = {0};
                        record.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
                        record.parent_address = 0xFF; // 无效
                        record.local_address = DCDC_ARM_CAN_ADDR + i;
                        record.operation_attribute = DEVICE_EVENT_OP_AUTO_ADD;
                        record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                        record.identifier_info.sn_info.dev_type = can_node_rd.Dcdc[i].dcdc_common_info.dev_type;
                        record.identifier_info.sn_info.dev_sn = &can_node_rd.Dcdc[i].dcdc_common_info.dev_sn;
    
                        // 生成记录到队列
                        Dev_Access_Log_Generate(&record);
#endif                
                        reals.need_new_version_flag.sBit.dcdc = 1;
                    }
                }
                    
                if (can_node_rd.Dcdc[i].alive_time >= interval) 
                {
                    can_node_rd.Dcdc[i].alive_time -= interval;
                } 
                else 
                {
                    if(can_node_rd.Dcdc[i].dev_access_state == 1) {
                        // 生成设备接入记录
                        can_node_rd.Dcdc[i].dev_access_state = 0;
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                        // 更新记录参数
                        dev_access_params_t record = {0};
                        record.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
                        record.parent_address = 0xFF; // 无效
                        record.local_address = DCDC_ARM_CAN_ADDR + i;
                        record.operation_attribute = DEVICE_EVENT_OP_TIMEOUT_OFFLINE;
                        record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                        record.identifier_info.sn_info.dev_type = can_node_rd.Dcdc[i].dcdc_common_info.dev_type;
                        record.identifier_info.sn_info.dev_sn = &can_node_rd.Dcdc[i].dcdc_common_info.dev_sn;
                        
                        // 生成记录到队列
                        Dev_Access_Log_Generate(&record);
#endif     
                    }
                    
                    can_node_rd.Dcdc[i].alive_time = 0;
                    can_node_rd.Dcdc[i].online = 0;
                    memset(&can_node_rd.Dcdc[i], 0x00, sizeof(can_node_rd.Dcdc[i]));              
                    memset(&top_modbus_rd.Dcdc[i], 0x00, sizeof(top_modbus_rd.Dcdc[i]));
                    ESP_LOGE(TAG, "Dcdc offline");
                }
            }
        }
        
#endif

    /*更新设备数量*/
    update_online_counts(temp_iot_online_cnt, temp_inv_online_cnt, temp_pack_online_cnt,
                            temp_achub_online_cnt, temp_dchub_online_cnt, temp_dcdc_online_cnt);
}

/*
缓存命令初始化
*/
void CanCmdUpdate(rw_cmd_struct *cmd, uint8_t devId, uint8_t can_type, uint16_t offset, uint16_t len, uint16_t crc16) 
{
    if (!cmd)  return ;
    cmd->devId = devId;
    cmd->can_type = can_type;
    cmd->write_offset = offset;
    cmd->write_remain_len = len;
    cmd->write_crc16 = crc16;
    cmd->write_next_seq = 0;
    cmd->crc_valid = 0;
}


//can
#define PACK_CAN_ADDR_GROUP1           0x60	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP2           0x68	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP3           0x70	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP4           0x78	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP5           0x80	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP6           0x88	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP7           0x90	//pack 并机群组 首地址
#define PACK_CAN_ADDR_GROUP8           0x98	//pack 并机群组 首地址
//modbus
#define PACK_MODBUS_ADDR_GROUP1           41	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP2           49	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP3           57	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP4           65	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP5           73	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP6           81	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP7           89	//pack 并机群组 modbus slave首地址
#define PACK_MODBUS_ADDR_GROUP8           97	//pack 并机群组 modbus slave首地址

/*------------------------------------------------------------------------------
 Function: Can_ID_Announce_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查是否为主动上报帧
  * @param[in]  uint32_t CanId  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t Can_ID_Announce_Check(uint32_t CanId)
{
    uint8_t pf_fun = 0;
    uint8_t Id_p = 0;
    uint8_t Id_edp = 0;
    uint8_t Id_dp = 0;

	pf_fun =(CanId>>16)&0xFF;   //功能码
    Id_p = (CanId>>26)&0x7;     //优先级
    Id_edp = (CanId>>25)&0x1;   //扩展数据页
    Id_dp = (CanId>>24)&0x1;    //数据页

    if (
        (
            // INV/pack 主动上报 old 条件
            (
                ((pf_fun >= 0x10) && (pf_fun <= 0x15)) || 
                ((pf_fun >= 0x9B) && (pf_fun <= 0xA0))
            ) && 
            (6 == Id_p) && 
            (0 == Id_edp) && 
            (0 == Id_dp)
        ) || 
        (
            // INV/pack 主动上报 new 条件
            (pf_fun >= 1) && 
            (pf_fun <= 200) && 
            (6 == Id_p) && 
            (0 == Id_edp) && 
            (1 == Id_dp)
        )
    )
    {
        return 1;
    }

    /*不符合要求*/
    return 0;
}
/*
windy add
CAN beta ID解析

input:
CanId:
output:
level1_addr:返回的 数组结构体一维序号,从0开始,注：组号，每组INV_MAX_NUM个逆变器
level2_addr:返回的 数组结构体二维序号,从0开始，每组INV_MAX_NUM个逆变器中，第几个逆变器


return:
0:合法 CAN ID
非0：非法 CAN ID
*/
static uint8_t Can_ID_Check(uint32_t CanId)
{
    uint8_t pf_fun = 0;
    uint8_t Id_p = 0;
    uint8_t Id_edp = 0;
    uint8_t Id_dp = 0;

	pf_fun =(CanId>>16)&0xFF;   //功能码
    Id_p = (CanId>>26)&0x7;     //优先级
    Id_edp = (CanId>>25)&0x1;   //扩展数据页
    Id_dp = (CanId>>24)&0x1;    //数据页

    if (
        (
            //OTA, block read/write， IOT综合控制帧
            (
                ((pf_fun >= 0x71) && (pf_fun <= 0x82)) || 
                ((pf_fun >= 0x60) && (pf_fun <= 0x66)) || 
                (0xFA == pf_fun)
            ) && 
            (2 == Id_p) && 
            (0 == Id_edp) && 
            (0 == Id_dp)
        ) || 
        (
            // INV/pack 主动上报 old 条件
            (
                ((pf_fun >= 0x10) && (pf_fun <= 0x15)) || 
                ((pf_fun >= 0x9B) && (pf_fun <= 0xA0))
            ) && 
            (6 == Id_p) && 
            (0 == Id_edp) && 
            (0 == Id_dp)
        ) || 
        (
            // INV/pack 主动上报 new 条件
            (pf_fun >= 1) && 
            (pf_fun <= 200) && 
            (6 == Id_p) && 
            (0 == Id_edp) && 
            (1 == Id_dp)
        )
    )
    {
        return 1;
    }

    /*不符合要求*/
    return 0;
}



/*------------------------------------------------------------------------------
 Function: Can_ID_Analyze
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN beta ID解析
  * @param[in]  uint32_t CanId        
                uint8_t *level1_addr  返回的 数组结构体一维序号,从0开始
                uint8_t *level2_addr  返回的 数组结构体二维序号,从0开始
  * @param[out] None
  * @return     int8_t  0:合法 CAN ID     非0：非法 CAN ID
  */
int8_t Can_ID_Analyze(uint32_t CanId, uint8_t *level1_addr, uint8_t *level2_addr)
{
    int8_t rtn = 0xFF;

    uint8_t addr_source = 0;
    static uint8_t sSameIDcnt = 0;

	addr_source =CanId&0xFF;    //来源地址

    /*CAN ID检查(can beta 协议)*/
    if (Can_ID_Check(CanId))
	{
		if ((addr_source >= INV_CAN_ADDR) && (addr_source < (INV_CAN_ADDR + DEV_MAIN_NODE_MAX*INV_MAX_NUM)) )
		{
			*level1_addr = (addr_source - INV_CAN_ADDR)/INV_GROUP_MAX_NUM; // 逆变
			*level2_addr = (addr_source - INV_CAN_ADDR)%INV_GROUP_MAX_NUM;
			rtn=0;

			can_node_rd.Inv[*level2_addr].online =1;
			can_node_rd.Inv[*level2_addr].alive_time = INV_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_inv_rx_counter++;
		}
        else if ((addr_source >= INV_CAN_DSP_ADDR) && (addr_source < (INV_CAN_DSP_ADDR + DEV_MAIN_NODE_MAX*INV_MAX_NUM)) ) // 和DSP不直接交互，仅用于OTA
        {
            *level1_addr = (addr_source - INV_CAN_DSP_ADDR)/INV_GROUP_MAX_NUM; // 逆变
            *level2_addr = (addr_source - INV_CAN_DSP_ADDR)%INV_GROUP_MAX_NUM;
            rtn=0;

			reals.can_inv_rx_counter++;
        }
#ifdef COMMON_INV_TI137_CAN_ADDR
        else if ((addr_source >= COMMON_INV_TI137_CAN_ADDR) && (addr_source < (COMMON_INV_TI137_CAN_ADDR + DEV_MAIN_NODE_MAX*INV_MAX_NUM)) ) // 不直接交互，仅用于OTA
        {
            *level1_addr = (addr_source - COMMON_INV_TI137_CAN_ADDR)/INV_GROUP_MAX_NUM; // 逆变
            *level2_addr = (addr_source - COMMON_INV_TI137_CAN_ADDR)%INV_GROUP_MAX_NUM;
            rtn=0;

			reals.can_inv_rx_counter++;
        }
#endif
		else if ((addr_source >= PACK_CAN_ADDR) &&( addr_source < (PACK_CAN_ADDR + PACK_MAX_NUM)) )
		{	
			*level1_addr = (addr_source - PACK_CAN_ADDR)/PACK_INGROUP_MAX_NUM; // PACK
			*level2_addr = (addr_source - PACK_CAN_ADDR)%PACK_INGROUP_MAX_NUM; // PACK
			
			rtn=0;
			can_node_rd.Pack[*level2_addr].online =1;
            if ( Can_ID_Announce_Check(CanId) )
            {
                /*堆主动上报*/
                can_node_rd.Pack[*level2_addr].alive_time = PACK_OFFLINE_TIME; /* 离线检测时间 */
            }
            else
            {
                /*基本数据交互*/
			    can_node_rd.Pack[*level2_addr].alive_time = PACK_OFFLINE_TIME; /* 离线检测时间 */
            }

			reals.can_pack_rx_counter++;
		} 
		else if((addr_source >= INV_IOT_CAN_ADDR) && (addr_source < (INV_IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX)) ) 
		{
			*level1_addr = (addr_source - INV_IOT_CAN_ADDR)/DEV_MAIN_NODE_MAX; // IOT
			*level2_addr = (addr_source - INV_IOT_CAN_ADDR)%DEV_MAIN_NODE_MAX; // IOT
			
			rtn=0;	
			can_node_rd.Iot[*level2_addr].online =1;
			can_node_rd.Iot[*level2_addr].alive_time = IOT_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_iot_rx_counter++;

            /*在IOT主从已明确情况下收到其他IOT从机的报文*/
			if(((reals.Addr_can_self+1) == addr_source)
				&&(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)
				&&(DEV_MAIN_NODE_MAX > 1))				
			{
				
				if(++sSameIDcnt >= 3)//重复多次滤波
				{
					sSameIDcnt =0;

                    /*重新执行IOT主从判断逻辑*/
					reals.MonSnSame=1;
					reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_TRIGER;
				}

			}
		} 
		else if ((addr_source >= DC_HUB_CAN_ADDR) && (addr_source < (DC_HUB_CAN_ADDR + DEV_MAIN_NODE_MAX*DC_HUB_MAX_NUM)) )
		{
		 	*level1_addr = (addr_source - DC_HUB_CAN_ADDR)/DC_HUB_GROUP_MAX_NUM; // dchub
		 	*level2_addr = (addr_source - DC_HUB_CAN_ADDR)%DC_HUB_GROUP_MAX_NUM;
		 	rtn=0;

		 	can_node_rd.DCHUB[*level2_addr].online =1;
		 	can_node_rd.DCHUB[*level2_addr].alive_time = DC_HUB_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_x_rx_counter++;
		}
		else if ((addr_source >= DCDC_IOT_CAN_ADDR) && (addr_source < (DCDC_IOT_CAN_ADDR + DEV_MAIN_NODE_MAX*DCDC_MAX_NUM)) )
		{
		 	*level1_addr = (addr_source - DCDC_IOT_CAN_ADDR)/DCDC_MAX_NUM; // dcdc
		 	*level2_addr = (addr_source - DCDC_IOT_CAN_ADDR)%DCDC_MAX_NUM;
		 	rtn=0;

		 	can_node_rd.Dcdc[*level2_addr].online =1;
		 	can_node_rd.Dcdc[*level2_addr].alive_time = DCDC_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_iot_rx_counter++;
		}        
		else if ((addr_source >= DCDC_ARM_CAN_ADDR) && (addr_source < (DCDC_ARM_CAN_ADDR + DEV_MAIN_NODE_MAX*DCDC_MAX_NUM)) )
		{
		 	*level1_addr = (addr_source - DCDC_ARM_CAN_ADDR)/DCDC_MAX_NUM; // dcdc
		 	*level2_addr = (addr_source - DCDC_ARM_CAN_ADDR)%DCDC_MAX_NUM;
		 	rtn=0;

		 	can_node_rd.Dcdc[*level2_addr].online =1;
		 	can_node_rd.Dcdc[*level2_addr].alive_time = DCDC_OFFLINE_TIME; /* 离线检测时间 */
			reals.can_x_rx_counter++;
		}
#if 0        
		else if (addr_source == AC_HUB_CAN_ADDR)
		{
			*level1_addr = 0; // achub
			*level2_addr = (addr_source - AC_HUB_CAN_ADDR)%AC_HUB_GROUP_MAX_NUM;
			// rtn=0;
            // TODO
			// Inv_can_mix.ac_hub_data[*level2_addr].online =1;
			// Inv_can_mix.ac_hub_data[*level2_addr].alive_time = AC_HUB_OFFLINE_TIME; /* 离线检测时间 */
			
		}
		else if (addr_source == PANEL_ARM_CAN_ADDR)
		{
			*level1_addr = 0; // achub
			*level2_addr = (addr_source - PANEL_ARM_CAN_ADDR)%PANEL_GROUP_MAX_NUM;
			// rtn=0;

			// Inv_can_mix.panel_data[*level2_addr].online =1;
			// Inv_can_mix.panel_data[*level2_addr].alive_time = PANEL_OFFLINE_TIME; /* 离线检测时间 */
			
		}
#endif        
        else if ( Can_ID_Non_native_IOT_Check(addr_source) == 0 )
        {
			*level1_addr = DEV_MAIN_NODE_MAX; // 其他IOT
			*level2_addr = 0;
			rtn=0;	
			reals.can_iot_rx_counter++;
        }
		else if (addr_source == 0x00)
		{
			*level1_addr = DEV_MAIN_NODE_MAX; // 上位机
			*level2_addr = 0;
			rtn=0;		  
		}
		else 
		{
	        // ESP_LOGE(TAG, "unkown can dev source addr: 0x%lx", CanId);
	        rtn= -1;        		
	    }		
    } 
	else 
	{
        // ESP_LOGE(TAG, "unkown CAN ID: 0x%lx", CanId);
        rtn= -1;        		
    }
    return rtn;
}
/*------------------------------------------------------------------------------
 Function: Can_ID_Non_native_IOT_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      非本机IOT地址判断
  * @param[in]  uint8_t addr_source  
  * @param[out] None
  * @return     int8_t  0:合法 CAN ID     非0：非法 CAN ID
  */
int8_t Can_ID_Non_native_IOT_Check(uint8_t addr_source)
{
    int8_t rtn = 0xFF;

    if ((addr_source == EBOX_PBOX_CAN_ADDR)
        || ((addr_source >= HMI_CAN_ADDR) && (addr_source < (HMI_CAN_ADDR + DEV_MAIN_NODE_MAX*HMI_MAX_NUM)) )
        || ((addr_source >= INV_IOT_CAN_ADDR) && (addr_source < (INV_IOT_CAN_ADDR + DEV_MAIN_NODE_MAX*IOT_MAX_NUM)) )
        || ((addr_source >= PANEL_IOT_CAN_ADDR) && (addr_source < (PANEL_IOT_CAN_ADDR + DEV_MAIN_NODE_MAX*PANEL_MAX_NUM)) )
        || ((addr_source >= DCDC_IOT_CAN_ADDR) && (addr_source < (DCDC_IOT_CAN_ADDR + DEV_MAIN_NODE_MAX*DCDC_MAX_NUM)) )
        )
    {
        rtn=0;        
    }

    return rtn;
}

/*
判断CAN ID，
输出在线标志和定时器计数器

devId:源地址


return:
0~x:CAN 数组结构体索引
*/
uint8_t CanDevIdCheck(node_info_t *node, uint8_t devId)
{
    uint8_t index = 0xFF;
    if ((devId >= INV_CAN_ADDR) && (devId < (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - INV_CAN_ADDR)%INV_GROUP_MAX_NUM; // 逆变 
    }
	else if ((devId >= DC_HUB_CAN_ADDR) && (devId < (DC_HUB_CAN_ADDR + (DC_HUB_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - DC_HUB_CAN_ADDR)%DC_HUB_GROUP_MAX_NUM; // dchub
    }
	else if (devId == AC_HUB_CAN_ADDR)
	{
        index = (devId - AC_HUB_CAN_ADDR)%AC_HUB_MAX_NUM; // achub
    }
	else if ((devId >= DCDC_ARM_CAN_ADDR) && (devId < (DCDC_ARM_CAN_ADDR + (DCDC_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - DCDC_ARM_CAN_ADDR)%DCDC_MAX_NUM; // dcdc
    }
	else if (devId == PANEL_ARM_CAN_ADDR)
	{
        index = (devId - PANEL_ARM_CAN_ADDR)%PANEL_MAX_NUM; // panel
    }
	else if ((devId >= PACK_CAN_ADDR) &&( devId < (PACK_CAN_ADDR + PACK_MAX_NUM)) )
	{
        index = (devId - PACK_CAN_ADDR)%PACK_INGROUP_MAX_NUM; // PACK
    } 
	else if((devId >= INV_IOT_CAN_ADDR) && (devId < (INV_IOT_CAN_ADDR + (IOT_MAX_NUM*DEV_MAIN_NODE_MAX)) ) )
	{
        index = (devId - INV_IOT_CAN_ADDR)%INV_GROUP_MAX_NUM; // IOT 
    } 
    else if ( Can_ID_Non_native_IOT_Check(devId) == 0 )
    {
        index = 0;  // 其他IOT
    }
	else if (devId == 0x00)
	{
        index = 0; // 上位机
    } 
	else 
	{
        ESP_LOGE(TAG, "CanDevIdCheck: unkown dev addr: 0x%x", devId);
    }
    return index;
}


/*
windy add
CAN beta ID解析

input:
CanId:
output:

return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check_Beta_Multi_Read(uint32_t CanId)
{
	int8_t rtn = 0xFF;

	uint8_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;
	
	// uint8_t addr_source = 0;

	pf_fun =(CanId>>16)&0xFF;
	Id_p = (CanId>>26)&0x7;
	Id_edp = (CanId>>25)&0x1;
	Id_dp = (CanId>>24)&0x1;

if (((pf_fun >= 0x63) && (pf_fun <= 0x66))
	&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//OTA,block read/write
	{
		rtn = 0;//valid
	}
	else
	{
		rtn = 1;
	}
return rtn;
}


/*
windy add
CAN beta ID解析

input:
CanId:
output:

return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check_Beta_Multi_Write(uint32_t CanId)
{
	int8_t rtn = 0xFF;

	uint8_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;
	
	// uint8_t addr_source = 0;

	pf_fun =(CanId>>16)&0xFF;
	Id_p = (CanId>>26)&0x7;
	Id_edp = (CanId>>25)&0x1;
	Id_dp = (CanId>>24)&0x1;

if (((pf_fun >= 0x60) && (pf_fun <= 0x62))
	&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//OTA,block read/write
	{
		rtn = 0;//valid
	}
	else
	{
		rtn = 1;
	}
return rtn;
}



/*
windy add
CAN beta ID解析

input:
CanId:
output:

return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check_Beta_Multi_OTA(uint32_t CanId)
{
	int8_t rtn = 0xFF;

	uint8_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;
	
	// uint8_t addr_source = 0;

	pf_fun =(CanId>>16)&0xFF;
	Id_p = (CanId>>26)&0x7;
	Id_edp = (CanId>>25)&0x1;
	Id_dp = (CanId>>24)&0x1;

if (((pf_fun >= 0x71) && (pf_fun <= 0x82))
	&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))//OTA,block read/write
	{
		rtn = 0;//valid
	}
	else
	{
		rtn = 1;
	}
return rtn;
}

// Modbus地址转CanID
uint8_t Modbus_Addr_To_Can_Id(uint8_t md_addr)
{
    // 默认为0（无效）
    uint8_t dev_id = 0;
    
	if (IS_INV_ADDR(md_addr)) {
		dev_id = INV_ADDR_TO_IDX(md_addr) + INV_CAN_ADDR;
	} else if (IS_PACK_ADDR(md_addr)) {
		dev_id = PACK_ADDR_TO_GRP(md_addr) * PACK_GROUP_SIZE + PACK_ADDR_TO_IDX(md_addr) + PACK_CAN_ADDR;
	}

    return dev_id;
}

/*------------------------------------------------------------------------------
 Function: CanLookupTypePosition
 -----------------------------------------------------------------------------*/
/**
  * @brief      Can Beta查表 报文存储的全局变量指针
                找到该数据类型在本地缓存中的位置,指针ptr指向缓存位置 
  * @param[in]  uint8_t Writeflag    
                node_info_t *node    输入，操作结构体
                uint8_t devId        CAN Rx ID源地址
                uint8_t type         
                uint8_t **ptr        输出，CAN全局变量指针
                uint32_t *maxlen     输出
                rw_cmd_struct **cmd  输出
  * @param[out] None
  * @return     void
  */
void CanLookupTypePosition(uint8_t Writeflag, node_info_t *node, uint8_t devId, uint8_t type, uint8_t **ptr, uint32_t *maxlen, rw_cmd_struct **cmd) 
{
    *ptr = NULL;
    *maxlen = 0;
    *cmd = NULL;

    /*根据devid判断存储 level2_addr 的值 （level1_addr 已在 CanRx_Parse_Task 锁定）*/
    uint8_t index = CanDevIdCheck(node, devId);
    if (index == 0xFF) {
        return ;
    }

	switch (type)
	{
		case IOT_TYPE_INFO_01H: 
            if(Writeflag == 1)
			{
                /*被写入，无效*/
			}
            else if ( Writeflag == 2 )
            {
                /*读取其他设备*/
            }
            else{
                *ptr = (uint8_t *)&can_node_rd.Iot[reals.Addr_can_self].mod_reg11000_IOT_info;	*maxlen = sizeof(iot_can_node_struct_reg11000);  *cmd = &can_node_rd.Iot[reals.Addr_can_self].mod_reg11000_IOT_info.rw_cmd;	
            }

		break;
		case IOT_TYPE_SET_02H: //set
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&can_node_wr.Iot.mod_reg12000_IOT_set; *maxlen = sizeof(iot_can_node_struct_reg12000);  *cmd = &can_node_wr.Iot.mod_reg12000_IOT_set.rw_cmd; 
			}            
            else if ( Writeflag == 2 )
            {
			    /*查询主机后设置自身（暂不同步）*/
            }
			else//read
			{
				*ptr = (uint8_t *)&can_node_rd.Iot[reals.Addr_can_self].mod_reg12000_IOT_set;	*maxlen = sizeof(iot_can_node_struct_reg12000);  *cmd = &can_node_rd.Iot[reals.Addr_can_self].mod_reg12000_IOT_set.rw_cmd; 
			}
		break;

		/* inv data type */
		case INV_TYPE_ANNOUNCE_10H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_announce;	 *maxlen = sizeof(inv_announce_struct);   *cmd = &(*node->invs_info)[index].inv_announce.rw_cmd;


		break;
		case INV_TYPE_BASE_11H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_base;		 *maxlen = sizeof(inv_base_struct); 	  *cmd = &(*node->invs_info)[index].inv_base.rw_cmd;   


		break;
		case INV_TYPE_LINE_12H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_line;		 *maxlen = sizeof(inv_line_struct); 	  *cmd = &(*node->invs_info)[index].inv_line.rw_cmd;   


		break;
		case INV_TYPE_ABOUT_13H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_about; 	 *maxlen = sizeof(inv_about_struct);	  *cmd = &(*node->invs_info)[index].inv_about.rw_cmd;  


		break;
		case INV_TYPE_PV_14H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_pv;		 *maxlen = sizeof(inv_pv_struct);		  *cmd = &(*node->invs_info)[index].inv_pv.rw_cmd;	  


		break;
		case INV_TYPE_GRID_15H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_grid;		 *maxlen = sizeof(inv_grid_struct); 	  *cmd = &(*node->invs_info)[index].inv_grid.rw_cmd;  


		break;
		case INV_TYPE_LOAD_16H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_load;		 *maxlen = sizeof(inv_load_struct); 	  *cmd = &(*node->invs_info)[index].inv_load.rw_cmd;  


		break;
		case INV_TYPE_DATA_17H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_data;		 *maxlen = sizeof(inv_data_struct); 	  *cmd = &(*node->invs_info)[index].inv_data.rw_cmd;  


		break;
		case INV_TYPE_METER_18H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_meter; 	 *maxlen = sizeof(inv_meter_struct);	  *cmd = &(*node->invs_info)[index].inv_meter.rw_cmd; 


		break;
		case INV_TYPE_GEN_19H: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_gen;		 *maxlen = sizeof(inv_gen_struct);		  *cmd = &(*node->invs_info)[index].inv_gen.rw_cmd;   


		break;
		case INV_TYPE_CONFIG00_1AH: //set
			if(1 == Writeflag)//write
			{
				ESP_LOGE(TAG,"!!!!!!!!!!!write in INV_TYPE_CONFIG00_1AH");
				*ptr = (uint8_t *)&can_node_wr.Inv.inv_set00; *maxlen = sizeof(inv_set00_struct);  *cmd = &can_node_wr.Inv.inv_set00.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set00; 	 *maxlen = sizeof(inv_set00_struct);	  *cmd = &(*node->invs_info)[index].inv_set00.rw_cmd; 
			}

		break;
		case INV_TYPE_CONFIG01_1BH: //set
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&can_node_wr.Inv.inv_set01; *maxlen = sizeof(inv_set01_struct);  *cmd = &can_node_wr.Inv.inv_set01.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set01; 	 *maxlen = sizeof(inv_set01_struct);	  *cmd = &(*node->invs_info)[index].inv_set01.rw_cmd; 
			}

		break;
		case INV_TYPE_CONFIG02_1CH: //set
			if(1 == Writeflag)//write
			{
			    *ptr = (uint8_t *)&can_node_wr.Inv.inv_set02; *maxlen = sizeof(inv_set02_struct);  *cmd = &can_node_wr.Inv.inv_set02.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set02; 	 *maxlen = sizeof(inv_set02_struct);	  *cmd = &(*node->invs_info)[index].inv_set02.rw_cmd; 
			}

		break;
		case INV_TYPE_CONFIG03_1DH: //set
			if(1 == Writeflag)//write
			{
			
				*ptr = (uint8_t *)&can_node_wr.Inv.inv_set03; *maxlen = sizeof(inv_set03_struct);  *cmd = &can_node_wr.Inv.inv_set03.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].inv_set03; 	 *maxlen = sizeof(inv_set03_struct);	  *cmd = &(*node->invs_info)[index].inv_set03.rw_cmd; 
			}

		break;
		case 0x20: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_log;		 *maxlen = sizeof(inv_log_struct);		  *cmd = &(*node->invs_info)[index].inv_log.rw_cmd;   


		break;
		case 0x21: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_all_energy; *maxlen = sizeof(inv_all_energy_struct); *cmd = &(*node->invs_info)[index].inv_all_energy.rw_cmd; 


		break;
		case 0x22: 
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_energy;	 *maxlen = sizeof(inv_energy_struct);	  *cmd = &(*node->invs_info)[index].inv_energy.rw_cmd; 
		break;
#if 0        
		case INV_TYPE_WIFI_23H: //福达
			*ptr = (uint8_t *)&(*node->invs_info)[index].inv_wifi;		 *maxlen = sizeof(inv_wifi_struct); 	  *cmd = &(*node->invs_info)[index].inv_wifi.rw_cmd;   

		break;
		case 0x24: //福达
			*ptr = (uint8_t *)&(*node->invs_info)[index].wifi_report;	 *maxlen = sizeof(wifi_report_struct);	  *cmd = &(*node->invs_info)[index].wifi_report.rw_cmd; 


		break;
		case 0x25: //福达
			if(1 == Writeflag)//write
			{
			
				*ptr = (uint8_t *)&can_node_wr.Inv.inv_wifi; *maxlen = sizeof(wifi_param_struct);  *cmd = &can_node_wr.Inv.inv_wifi.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->invs_info)[index].wifi_param;	 *maxlen = sizeof(wifi_param_struct);	  *cmd = &(*node->invs_info)[index].wifi_param.rw_cmd; 
			}

		break;
#endif            
		case COMMON_TYPE_TRANSMIT_27H: //set
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&can_node_wr.Iot.common_transmission; *maxlen = sizeof(common_transmit_struct);  *cmd = &can_node_wr.Iot.common_transmission.rw_cmd; 
			}
			else//read
			{
                if ((devId >= INV_CAN_ADDR) && (devId < (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))) )
                {
                    *ptr = (uint8_t *)&(*node->invs_info)[index].common_transmission;     *maxlen = sizeof(common_transmit_struct);     *cmd = &(*node->invs_info)[index].common_transmission.rw_cmd; 
                }
                else if ((devId >= PACK_CAN_ADDR) &&( devId < (PACK_CAN_ADDR + PACK_MAX_NUM)) )
                {
                    *ptr = (uint8_t *)&(*node->packs_info)[index].common_transmission;     *maxlen = sizeof(common_transmit_struct);     *cmd = &(*node->packs_info)[index].common_transmission.rw_cmd; 
                } 
                else
                {
                    ESP_LOGE(TAG, "type(0x%x), error node(%d)", type, devId); 
                }
			}

		break;

#if 0

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE

		case MODULE_TYPE_DC_AC_HUB_SET_40H: //set tbd
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&can_node_wr.HUB.dc_ac_hub_setting; *maxlen = sizeof(dc_ac_hub_set);  *cmd = &can_node_wr.HUB.dc_ac_hub_setting.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->dchubs_info)[index].dc_ac_hub_setting;	 *maxlen = sizeof(dc_ac_hub_set); 	*cmd = &(*node->dchubs_info)[index].dc_ac_hub_setting.rw_cmd; 
			}
		
		break;


		case MODULE_TYPE_DC_HUB_INFO_41H: 
			*ptr = (uint8_t *)&(*node->dchubs_info)[index].dc_hub_info; 	*maxlen = sizeof(dc_hub_info_struct);	  *cmd = &(*node->dchubs_info)[index].dc_hub_info.rw_cmd; 


		break;

#endif

#ifdef CONFIG_CAN_ACHUB_SUPPLY_ENABLE		
		case MODULE_TYPE_AC_HUB_INFO_42H: 
			*ptr = (uint8_t *)&(*node->achubs_info)[index].ac_hub_info; 	*maxlen = sizeof(ac_hub_info_struct);	  *cmd = &(*node->achubs_info)[index].ac_hub_info.rw_cmd; 


		break;
#endif

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE

		case MODULE_TYPE_DCDC_INFO_48H: //set
			*ptr = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info;	*maxlen = sizeof(dcdc_common_info_struct);  *cmd = &(*node->dcdc_s_info)[index].dcdc_common_info.rw_cmd;	

				
		break;
		case MODULE_TYPE_DCDC_SET_49H: //set
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&can_node_wr.Dcdc.dcdc_common_set; *maxlen = sizeof(dcdc_common_set_struct);	*cmd = &can_node_wr.Dcdc.dcdc_common_set.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_set; *maxlen = sizeof(dcdc_common_set_struct);  *cmd = &(*node->dcdc_s_info)[index].dcdc_common_set.rw_cmd;	
			}
		
		break;

#endif

#endif

		/* pack data type */
		case PACK_TYPE_ANNOUNCE_50H: 
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_announce; *maxlen = sizeof(pack_announce_struct);  *cmd = &(*node->packs_info)[index].pack_announce.rw_cmd; 


		break;
		case PACK_TYPE_BASE_51H: 
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_base;	 *maxlen = sizeof(pack_base_struct);	  *cmd = &(*node->packs_info)[index].pack_base.rw_cmd;	 


		break;
		case PACK_TYPE_EXTEND_52H: 
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_extend;	 *maxlen = sizeof(pack_extend_struct);	  *cmd = &(*node->packs_info)[index].pack_extend.rw_cmd; 


		break;
		case PACK_TYPE_ABOUT_54H: 
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_about;	 *maxlen = sizeof(pack_about_struct);	  *cmd = &(*node->packs_info)[index].pack_about.rw_cmd;  


		break;
		case PACK_TYPE_CONFIG_55H: //set
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&can_node_wr.Pack.pack_config; *maxlen = sizeof(pack_config_struct);  *cmd = &can_node_wr.Pack.pack_config.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->packs_info)[index].pack_config;	 *maxlen = sizeof(pack_config_struct);	  *cmd = &(*node->packs_info)[index].pack_config.rw_cmd; 
			}

		break;
        case PACK_TYPE_DEBUG1_56H: 
			*ptr = (uint8_t *)&(*node->packs_info)[index].pack_debug1.payload[0]; *maxlen = sizeof(pack_debug1_struct)-4; *cmd = &(*node->packs_info)[index].pack_debug1.rw_cmd; 
            
        break;
#ifdef CONFIG_FUNC_PAYGO_ENABLE        
		case PACK_TYPE_BMS_PAYGO_INFO_5BH:
			if(1 == Writeflag)
			{
				*ptr = (uint8_t *)&can_node_wr.Pack.pack_paygo; *maxlen = sizeof(bms_paygo_info_t);  *cmd = &can_node_wr.Pack.pack_paygo.rw_cmd; 
			}
			else//read
			{
				*ptr = (uint8_t *)&(*node->packs_info)[index].pack_paygo;	 *maxlen = sizeof(bms_paygo_info_t);	  *cmd = &(*node->packs_info)[index].pack_paygo.rw_cmd; 
			}
		break;
#endif

#ifdef CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION		
		/* file data type */
		case UDT_QUERY_CMD: 
		 	*ptr = (uint8_t *)&can_node_wr.Iot.iot_can_master_file;	*maxlen = sizeof(iot_file_can_masetr_struct);  *cmd = &can_node_wr.Iot.iot_can_master_file.rw_cmd; 
		break;
		
		case UDT_RESP_CMD: 
			*ptr = (uint8_t *)&can_node_wr.Iot.iot_can_slave_file;	*maxlen = sizeof(iot_file_can_slave_struct);   *cmd = &can_node_wr.Iot.iot_can_slave_file.rw_cmd;  

		break;
#endif

#ifdef CONFIG_CAN_FACTORY_WRITE_ENABLE
        /* iot factory type:生产CAN标定 */
		case CAN_FACTORY_CMD: 
			if(1 == Writeflag)//write
			{
				*ptr = (uint8_t *)&can_node_wr.Iot.factory; *maxlen = sizeof(device_registers_t);  *cmd = &can_node_wr.Iot.factory.rw_cmd; 
			}            
            else if ( Writeflag == 2 )
            {
			    /*查询主机后设置自身（暂不同步）*/
            }
			else//read
			{
				*ptr = (uint8_t *)&can_node_rd.Iot[reals.Addr_can_self].factory;	*maxlen = sizeof(device_registers_t);  *cmd = &can_node_rd.Iot[reals.Addr_can_self].factory.rw_cmd; 
			}
		break;
#endif

		default: ESP_LOGE(TAG, "unkown type(0x%x) node(%d)", type, devId); 

		return ;
	}


    if (*maxlen >= sizeof(rw_cmd_struct))
        *maxlen -= sizeof(rw_cmd_struct);
}

/*------------------------------------------------------------------------------
 Function: CanVerifyCmd
 -----------------------------------------------------------------------------*/
/**
  * @brief      多字节操作，命令报文
                初始化多字节操作的结构体和指针环境
  * @param[in]  node_info_t *node  
                uint32_t CanId      CAN Rx ID
                uint8_t isWrite     2-自己做主，多字节读其他设备；
                                    1-自己做从，被其他主设备写
		                            0-自己做从，被其他主设备读取  
                uint8_t devId       CAN Rx ID源地址
                CmdStruct *p_cmd    将 CAN  RX data结构为结构体，解析
                dev_data *payload  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t CanVerifyCmd(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, CmdStruct *p_cmd, dev_data *payload) 
{
	uint8_t Writeflag=0;

    IdStruct RxcanId ;
	RxcanId.all = CanId;

	if (!p_cmd) return ERR_TYPE;

	uint8_t type = p_cmd->type;
	uint16_t offset = p_cmd->start;
	uint16_t len = p_cmd->total;
	uint16_t crc16 = p_cmd->crc16;
    uint32_t maxlen = 0;
    uint8_t *ptr = NULL;//从CAN全局变量表格获得的待操作全局变量指针
    rw_cmd_struct *cmd = NULL;

    if (payload) {
        payload->len = 0;
        payload->pdata = NULL;
    }
	
	if(WRIET_DATA_START == RxcanId.bit.funcode) {
	    /*自己做从，被其他主设备写*/
		Writeflag = 1;
    } else if(READ_RESP_DATA_START == RxcanId.bit.funcode) {
        /*自己做主，多字节读其他设备*/
        Writeflag = 2;
	} else {
		Writeflag = 0;
	}
    
    /* 找到该数据类型在本地缓存中的位置,指针ptr指向缓存位置 */
	CanLookupTypePosition(Writeflag, node, devId, type, &ptr, &maxlen, &cmd);

    /*寻址失败*/
    if (!cmd || !ptr || !maxlen) {
        return ERR_TYPE;
    }

    /*需要操作的长度超过范围*/
    if ((maxlen - offset) < len) 
	{
        ESP_LOGE(TAG, "devId:0x%x type:0x%x offset:%d too long(max=%ld, recv=%d)", devId, type, offset, maxlen, len);
        if (isWrite == 1) {
            /*擦除记录*/
            CanCmdUpdate(cmd, devId, 0, 0, 0, 0); 
        }
        return ERR_RANGE;
    }

    /* 数据写入本地 */
    if (isWrite == 1) 
	{ 
        /*消除命令记录*/
        CanCmdUpdate(cmd, devId, 0, 0, 0, 0);

        /* 数据开始,可能上次开辟的空间不足,需要重新开辟空间 */
        if (cmd->temp_buffer != NULL) {
            free(cmd->temp_buffer);
            cmd->temp_buffer = NULL;
        }

#ifdef	CAN_MASTER_POLL_ENABLE
        /*收到查询回复，暂缓轮询，缓解总线压力*/
		if(READ_RESP_DATA_START == ((CanId>>16)&0xFF)) {
			if(0 == len) {
				reals.Can_beta_block_send_timeout_Cnt = CAN_BLOCK_RX_TIME_OUT_FAST;//空回复，短延时
			} else {
				reals.Can_beta_block_send_timeout_Cnt = CAN_BLOCK_RX_TIME_OUT_SLOW;//有效回复
			}
		}
#endif

        if ((cmd->temp_buffer == NULL) && (len != 0) )
		{
            #ifdef STATE_COUNT
            cmd->recv_cnt++;
            #endif
            
			/* 为写入的数据分配缓存 */
            cmd->temp_buffer = iot_calloc(len);
            if (cmd->temp_buffer == NULL) {
                ESP_LOGE(TAG, "can type: 0x%x memony malloc failed %dbytes", type, len);
                return ERR_MEMONY;
            }
            
            /*生成命令记录（设备ID，偏移量,长度,校验）*/
            CanCmdUpdate(cmd, devId, type, offset, len, crc16);
        }
    } 
    /* 从本地获取数据 */
	else 
	{
        if (payload == NULL) {
            return ERR_MEMONY;
        }
		
        /*如果对方查询指定长度，就取指定长度，否则 自己计算最大长度，全部上传*/
        payload->len = (len) ? (len) : (maxlen - offset);
        uint8_t *tmp_data = iot_calloc(payload->len);
        if (tmp_data == NULL) {
            ESP_LOGE(TAG, "can type: 0x%x memony malloc failed %dbytes (tmp_data)", type, payload->len);
            return ERR_MEMONY;
        }

        /*数据传递*/
        payload->pdata = tmp_data;
        memcpy(payload->pdata, (ptr + offset), payload->len);
    }
    
    return ERR_NONE;
}

/*------------------------------------------------------------------------------
 Function: CanVerifyData
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN多字节读取的 接收 解析，赋值到CAN 全局变量结构体
  * @param[in]  node_info_t *node  总线上设备节点
                uint32_t CanId     can ID all
                uint8_t isWrite    读写操作 1：写 0：读  
                uint8_t devId      can ID rx源地址  
                uint8_t type       设备类型 DSP ARM IOT
  * @param[out] Buff中的数据
  * @return     u4byte
  */
uint8_t CanVerifyData(node_info_t *node, uint32_t CanId, uint8_t isWrite, uint8_t devId, uint8_t type, uint8_t seq, uint8_t len, const uint8_t *payload) {
	uint8_t Writeflag=0;// isWrite = 1,表示数据是给到到本机存储
   
    IdStruct RxcanId ;
	RxcanId.all = CanId;

    if (!isWrite || !payload) return ERR_TYPE;
    
	uint32_t maxlen = 0;
    uint8_t *ptr = NULL;//从CAN全局变量表格获得的待操作全局变量指针
    rw_cmd_struct *cmd = NULL;
    
	if(WRITE_DATA == RxcanId.bit.funcode) {
	    /*自己做从，被其他主设备写*/
		Writeflag = 1;
    } else if(READ_RESP_DATA == RxcanId.bit.funcode) {
        /*自己做主，多字节读其他设备*/
        Writeflag = 2;
	} else {
		Writeflag = 0;
	}
    
    /* 找到该数据类型在本地缓存中的位置,指针ptr指向缓存位置 */
	CanLookupTypePosition(Writeflag, node, devId, type, &ptr, &maxlen, &cmd);

    /*寻址失败*/
    if (!cmd || !ptr) return ERR_TYPE;

    /*设备ID号不同*/
    if (cmd->devId != devId) {
    	ESP_LOGE(TAG,"CanId: 0x%lX",CanId);
        ESP_LOGE(TAG,"cmd->devId != devId , cmd->devId : %d, devId : %d, seq: %d", cmd->devId, devId, seq);
        return ERR_WAIT;
    }

    /*can type不同*/
    if (cmd->can_type != type) {
        ESP_LOGE(TAG,"CanId: 0x%lX",CanId);
        ESP_LOGE(TAG,"cmd->can_type != type , cmd->can_type : %d, type : %d, seq: %d", cmd->can_type, type, seq);
        return ERR_WAIT;
    }

    /*等待的帧序号不相同*/
    if (cmd->write_next_seq != seq) {
    	ESP_LOGE(TAG,"CanId: 0x%lX, type: 0x%x, cmd->write_next_seq: %d, seq: %d", CanId, type, cmd->write_next_seq, seq);
        return ERR_WAIT;
    }

    /*无法缓存数据*/
    if (cmd->temp_buffer == NULL) return ERR_MEMONY;

#ifdef	CAN_MASTER_POLL_ENABLE
    /*收到查询回复，暂缓轮询，缓解总线压力*/
	if(READ_RESP_DATA == ((CanId>>16)&0xFF)) {
		if((cmd->write_remain_len == len)||(cmd->write_remain_len <= FRAME_DATA_BYTES)) {
			reals.Can_beta_block_send_timeout_Cnt =CAN_BLOCK_RX_TIME_OUT_FAST;//最后一帧，短延时
		} else {
			reals.Can_beta_block_send_timeout_Cnt =CAN_BLOCK_RX_TIME_OUT_SLOW;//
		}
	}
#endif

    /*数据在有效范围（每个CAN帧最多6字节数据）*/
    if ((cmd->write_remain_len >= len) && (len <= FRAME_DATA_BYTES)) 
	{
	    /*数据暂存*/
        memcpy((cmd->temp_buffer + (seq * FRAME_DATA_BYTES)), payload, len);
        cmd->write_next_seq++;
        cmd->write_remain_len -= len;

		/* 数据未接收完整 */
        if (cmd->write_remain_len != 0) {
            return ERR_WAIT;
        }

        /* 接收完成，计算总长及CRC校验 */
        cmd->write_next_seq = 0xFF; 
        uint16_t data_len = ((uint16_t)seq * FRAME_DATA_BYTES) + len;
        uint16_t crc16 = calcu_crc16(cmd->temp_buffer, data_len);

		/* 数据接收完成校验成功后写入到本地缓存 */
        if (cmd->write_crc16 == crc16) 
		{
		    /* crc正确 */
            cmd->crc_valid = 1;

            /*检查是否有写入越界*/
            if ((cmd->write_offset + data_len) <= maxlen) 
			{                
			    /*获取数据同步至CAN全局变量*/
                memcpy((ptr + cmd->write_offset), cmd->temp_buffer, data_len);
                
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
                if (( 1 == Writeflag ) 
                    && ((type == IOT_TYPE_SET_02H) 
                        || (type == INV_TYPE_CONFIG00_1AH) 
                        || (type == INV_TYPE_CONFIG01_1BH) 
                        || (type == INV_TYPE_CONFIG02_1CH) 
                        || (type == INV_TYPE_CONFIG03_1DH) 
                        || (type == PACK_TYPE_CONFIG_55H))
                   )
                {
                    // 1. 查找本地参数结构体
                    uint8_t *rd_ptr = NULL;
                    rw_cmd_struct *rd_cmd = NULL;
                    uint32_t rd_maxlen = 0;
                    CanLookupTypePosition(0, node, devId, type, &rd_ptr, &rd_maxlen, &rd_cmd);
                    
                    if (rd_cmd && rd_ptr && (cmd->write_offset + data_len <= rd_maxlen)) {
                        // 2. 声明并填充参数结构体
                        can_change_params_t log_params = {
                            .source_id           = RxcanId.bit.src, // CAN源地址
                            .target_id           = RxcanId.bit.dst, // CAN目标地址
                            .can_type            = cmd->can_type, // PGN或功能码
                            .offset              = cmd->write_offset,
                            .length              = data_len,
                            .old_data            = (uint8_t *)(rd_ptr + cmd->write_offset),
                            .new_data            = (uint8_t *)(ptr + cmd->write_offset),
                            .protocol_version    = PROTOCOL_CAN_BETA,
                            .modification_source = DATA_SOURCE_CAN,
                        };
                        
                        // 3. 调用新函数，只传递一个参数
                        if (Reg_Change_Log_Generate_By_Can(&log_params) != 0) {
                            ESP_LOGE(TAG, "Failed to generate CAN change log.");
                        }
                    }
                }
#endif
                /*数据通道*/
				if (type == UDT_QUERY_CMD)
				{
					ESP_LOGW(TAG, "udt pc interface, received data length: %d, slave_addr: %d, funcode: %d",
							data_len, cmd->temp_buffer[10], cmd->temp_buffer[11]);
					// ESP_LOG_BUFFER_HEX(TAG, cmd->temp_buffer, cmd->write_total_len);
#ifdef CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
                    udt_recv_push(cmd->temp_buffer, data_len, UDT_MODE_PC);
#endif
				}
				else if (type == UDT_RESP_CMD)
				{
#ifdef CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
					ESP_LOGW(TAG, "udt received device data and relay it to terminal(%d)", udt_relay_mode_get());
					udt_protocol_t *udt_prot = (udt_protocol_t *)cmd->temp_buffer;
					ESP_LOGW(TAG, "dev_id: %02x, data_len: %d, slave_addr: %d, funcode: %d, pack_idx: %d, pack_total: %d\n",
							devId, data_len, udt_prot->address, udt_prot->funcode, UDT_SWAP16(udt_prot->pkg_idx), UDT_SWAP16(udt_prot->pkg_total));
					// ESP_LOG_BUFFER_HEX(TAG, cmd->temp_buffer, 20);
					udt_relay_to_terminal(cmd->temp_buffer, data_len);
#endif
				}
				else if (type == IOT_TYPE_SET_02H)
				{
				    /*上位机写入本IOT*/
					if(WRITE_DATA == RxcanId.bit.funcode)
					{
						Can_iot_callback_set_type_0x2(cmd->write_offset, data_len);
					}				
				}	
				else if (type == CAN_FACTORY_CMD)
				{
				    /*上位机写入本IOT*/
					if(WRITE_DATA == RxcanId.bit.funcode)
					{
#ifdef CONFIG_CAN_FACTORY_WRITE_ENABLE					
						Can_iot_callback_set_type_0xFF(cmd->write_offset, data_len);
#endif
					}				
				}	
            } 
			else 
			{
                #ifdef STATE_COUNT
                cmd->error_cnt++;
                #endif
                
                ESP_LOGE(TAG, "devId: 0x%02x type: 0x%02x offset: %d len: %d,maxlen=%ld, error", devId, type, cmd->write_offset, data_len,maxlen);
                
                if (cmd->temp_buffer != NULL)
                {
                    free(cmd->temp_buffer);
                    cmd->temp_buffer = NULL;
                }

                return ERR_RANGE; /* 越界 */
                }
        } 
		else 
		{
            cmd->crc_valid = 0; /* crc错误 */
            ESP_LOGE(TAG, "devId: 0x%02x type: 0x%02x len: %d crcA: 0x%04x, calc: 0x%04x", devId, type, data_len, cmd->write_crc16, crc16);
            //ESP_LOG_BUFFER_HEX_LEVEL(TAG, cmd->temp_buffer, data_len, ESP_LOG_WARN);
            
            if (cmd->temp_buffer != NULL)
            {
                free(cmd->temp_buffer);
                cmd->temp_buffer = NULL;
            }
            
            return ERR_CRC; /* crc错误 */
        }

        /*更新下级CAN设备 set区域参数接收进度*/
		if(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)// 
		{
			if((uint32_t)ptr == (uint32_t)&can_node_rd.Inv[reals.Addr_can_self].inv_set00)//地址相同
			{
				can_node_rd.Inv[reals.Addr_can_self].setdata_valid |=1<<CAN_INV_SETDATA_set00;
			}
			else if((uint32_t)ptr == (uint32_t)&can_node_rd.Inv[reals.Addr_can_self].inv_set01)//地址相同
			{
				can_node_rd.Inv[reals.Addr_can_self].setdata_valid |=1<<CAN_INV_SETDATA_set01;
			}
			else if((uint32_t)ptr == (uint32_t)&can_node_rd.Inv[reals.Addr_can_self].inv_set02)//地址相同
			{
				can_node_rd.Inv[reals.Addr_can_self].setdata_valid |=1<<CAN_INV_SETDATA_set02;
			}
			else if((uint32_t)ptr == (uint32_t)&can_node_rd.Inv[reals.Addr_can_self].inv_set03)//地址相同
			{
				can_node_rd.Inv[reals.Addr_can_self].setdata_valid |=1<<CAN_INV_SETDATA_set03;
			}
		}

		/*更新CAN全局变量至对外全局变量(can->modbus)*/
        CAN_To_Modbus_Read_Info_Process_Beta(type, devId);//
        if (cmd->temp_buffer != NULL)
        {
            free(cmd->temp_buffer);
            cmd->temp_buffer = NULL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "devId: 0x%02x, type: 0x%02x, offset: %d, write_remain_len: %d, len: %d, error", devId, type, cmd->write_offset, cmd->write_remain_len, len);
        return ERR_RANGE; /* 越界 */
    }
    
    return ERR_NONE;
}

/*------------------------------------------------------------------------------
 Function: Pack_Single_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      电池包单机数据赋值（根据获取到的CAN_TYPE）
  * @param[in]  MOD_STRUCT_Pack *Pack_Modbus_Data  
                pack_node_struct *Pack_Can_Data    
                uint8_t type                       
  * @param[out] None
  * @return     void
  */
void Pack_Single_Data_Can_to_Modbus(MOD_STRUCT_Pack *Pack_Modbus_Data, can_node_struct_Pack *Pack_Can_Data, uint8_t type, uint8_t devId, uint8_t numpack)
{
    if(type == 0x51)
    {
        /*6100~6110*/
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,total_voltage), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,total_voltage),(offsetof(pack_base_struct,soc) - offsetof(pack_base_struct,total_voltage)));// 6111~6112
#ifdef PACK_VOLTAGE_LOW_LEVEL_ENABLE
        Pack_Modbus_Data->mod_reg06100_Pack_each.total_voltage = Pack_Modbus_Data->mod_reg06100_Pack_each.total_voltage/10;
#endif
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,soc), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,soc),sizeof(Pack_Can_Data->pack_base.soc));// 6113
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,soh), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,soh),sizeof(Pack_Can_Data->pack_base.soh));// 6114
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,avg_temp), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,avg_temp),sizeof(Pack_Can_Data->pack_base.avg_temp));// 6115
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_cell_voltage), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,min_cell_voltage),(offsetof(pack_base_struct,min_cell_index) - offsetof(pack_base_struct,min_cell_voltage)));// 6116~6117
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_cell_index), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,min_cell_index),sizeof(Pack_Can_Data->pack_base.min_cell_index));// 6118
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_cell_index), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,max_cell_index),sizeof(Pack_Can_Data->pack_base.max_cell_index));// 6119
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_temp_value), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,min_temp_value),sizeof(Pack_Can_Data->pack_base.min_temp_value));// 6120
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_temp_value), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,max_temp_value),sizeof(Pack_Can_Data->pack_base.max_temp_value));// 6121
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,min_temp_index), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,min_temp_index),sizeof(Pack_Can_Data->pack_base.min_temp_index));// 6122
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,max_temp_index), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,max_temp_index),sizeof(Pack_Can_Data->pack_base.max_temp_index));// 6123
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,work_status), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,work_status),sizeof(Pack_Can_Data->pack_base.work_status));// 6124
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,chg_status), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,chg_status),sizeof(Pack_Can_Data->pack_base.chg_status));// 6125
        // memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_online), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,pack_cap_online),sizeof(Pack_Can_Data->pack_base.pack_cap_online));// 6127
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,relay), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,relay),sizeof(Pack_Can_Data->pack_base.relay));// 6128
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_cap_online), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,pack_cap_online),sizeof(Pack_Can_Data->pack_base.pack_cap_online));// 6129
        // 6130 pack_canbus_error
        // /*6131~6143*/ 0x52

        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,protect), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,protect),(offsetof(pack_base_struct,relay) - offsetof(pack_base_struct,protect)));// 6144~6149
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,chg_full_time), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,chg_full_time),(offsetof(pack_base_struct,allow_max_chg_voltage) - offsetof(pack_base_struct,chg_full_time)));// 6150~6151
        
        // 6152~6153
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_total_cell), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,pack_total_cell),sizeof(Pack_Can_Data->pack_base.pack_total_cell));// 6152
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_total_ntc), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,pack_total_ntc),sizeof(Pack_Can_Data->pack_base.pack_total_ntc));// 6153
        
        // 6154 PackBMUCnt 
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,PackBMUCnt), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,bmu_nums),sizeof(Pack_Can_Data->pack_base.bmu_nums));// 6153
        // 6157
        // memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,pack_outsum_voltage), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,vbus),sizeof(Pack_Can_Data->pack_base.vbus));// 6158
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,allow_max_chg_current), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,allow_max_chg_current),(offsetof(pack_base_struct,protect_status) - offsetof(pack_base_struct,allow_max_chg_current)));// 6160~6161

        // 6164
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,alarm2), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,protect_status),sizeof(Pack_Can_Data->pack_base.protect_status));
        // 6165
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,alarm3), (uint8_t *)&Pack_Can_Data->pack_base + offsetof(pack_base_struct,protect_status2),sizeof(Pack_Can_Data->pack_base.protect_status2));
    }
    else if(type == 0x52)
    {
        // /*6131~6142*/ 0x52
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,capacity), (uint8_t *)&Pack_Can_Data->pack_extend + offsetof(pack_extend_struct,capacity),(offsetof(pack_extend_struct,cycle_count) - offsetof(pack_extend_struct,capacity)));// 6131~6142
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,cycle_count), (uint8_t *)&Pack_Can_Data->pack_extend + offsetof(pack_extend_struct,cycle_count),sizeof(Pack_Can_Data->pack_extend.cycle_count));// 6143
    }
    else if(type == 0x54)
    {
#ifdef INTERNAL_PACK_ENABLE    
        if(numpack == 0) {
            memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,type_ascii), (uint8_t *)&dev_factory.dev_type, sizeof(dev_factory.dev_type));// 6101~6106
            Pack_Modbus_Data->mod_reg06100_Pack_each.sn_code = dev_factory.dev_sn; // 6107~6110
        } else {
            memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,type_ascii), (uint8_t *)&Pack_Can_Data->pack_about + offsetof(pack_about_struct,type_ascii),(offsetof(pack_about_struct,software_total) - offsetof(pack_about_struct,type_ascii)));// 6101~6110
        }
#else
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,type_ascii), (uint8_t *)&Pack_Can_Data->pack_about + offsetof(pack_about_struct,type_ascii),(offsetof(pack_about_struct,software_total) - offsetof(pack_about_struct,type_ascii)));// 6101~6110
#endif
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,software_total), (uint8_t *)&Pack_Can_Data->pack_about + offsetof(pack_about_struct,software_total),sizeof(Pack_Can_Data->pack_about.software_total));// 6173
        // 6174~6203
        for(int i = 0;i < 10;i++)
        {
            memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each.soft[i].type, (uint8_t *)&Pack_Can_Data->pack_about.soft[i].type,sizeof(Pack_Can_Data->pack_about.soft[i].type));
            memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06100_Pack_each.soft[i].version, (uint8_t *)&Pack_Can_Data->pack_about.soft[i].version,sizeof(Pack_Can_Data->pack_about.soft[i].version));
        }                
    }
    else if(type == 0x55)// 可写
    {
        // 7000
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,SetPvHeat), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,pack_heat_enable),sizeof(Pack_Can_Data->pack_config.pack_heat_enable));// 7001
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,SetPackHeat), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,ctr_heat_enable),sizeof(Pack_Can_Data->pack_config.ctr_heat_enable));// 7002
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,SetPackUnlock), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,unlock_failed_flags),sizeof(Pack_Can_Data->pack_config.unlock_failed_flags));// 7003
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,PACK_Parallel_InOneGroup), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,max_parallel_nums),sizeof(Pack_Can_Data->pack_config.max_parallel_nums));// 7004               
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,on_off_set), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,on_off_set),sizeof(Pack_Can_Data->pack_config.on_off_set));// 7005
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,ReservationTime), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,ReservationTime),sizeof(Pack_Can_Data->pack_config.ReservationTime));// 7006~7007
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,NormalTempSocMinLimit), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,NormalTempSocMinLimit),sizeof(Pack_Can_Data->pack_config.NormalTempSocMinLimit));// 7008
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,LowTempSocMinLimit), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,LowTempSocMinLimit),sizeof(Pack_Can_Data->pack_config.LowTempSocMinLimit));// 7009      
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,BMS_Protol_Type), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,BMS_Protol_Type),sizeof(Pack_Can_Data->pack_config.BMS_Protol_Type));// 7010
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,BatteryChargingPower), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,BatteryChargingPower),sizeof(Pack_Can_Data->pack_config.BatteryChargingPower));// 7011  
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,BatteryDischargingPower), (uint8_t *)&Pack_Can_Data->pack_config + offsetof(pack_config_struct,BatteryDischargingPower),sizeof(Pack_Can_Data->pack_config.BatteryDischargingPower));// 7012  
    }
    else if(type == 0x56)
    {
        // 6300
        Pack_Modbus_Data->mod_reg06300_Pack_cell.PackCellCnt = Pack_Modbus_Data->mod_reg06100_Pack_each.pack_total_cell;
        Pack_Modbus_Data->mod_reg06300_Pack_cell.PackNTCCnt = Pack_Modbus_Data->mod_reg06100_Pack_each.pack_total_ntc;
        memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06300_Pack_cell + offsetof(MOD_STRUCT_reg06300,payload), (uint8_t *)&Pack_Can_Data->pack_debug1 + offsetof(pack_debug1_struct,payload), DEBUG_REG_NUM);

        #if 0
        ESP_LOGW(TAG, "Pack_Single_Data_Can_to_Modbus : type(%s),sn(%llu), cell(%d), ntc(%d) ", 
            Pack_Modbus_Data->mod_reg06100_Pack_each.type_ascii,
            Pack_Modbus_Data->mod_reg06100_Pack_each.sn_code,
            Pack_Modbus_Data->mod_reg06300_Pack_cell.PackCellCnt,
            Pack_Modbus_Data->mod_reg06300_Pack_cell.PackNTCCnt);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, &Pack_Modbus_Data->mod_reg06300_Pack_cell.payload[0], 
            (Pack_Modbus_Data->mod_reg06300_Pack_cell.PackCellCnt*2 + Pack_Modbus_Data->mod_reg06300_Pack_cell.PackNTCCnt), ESP_LOG_WARN);
        #endif
    }    
}


/*------------------------------------------------------------------------------
 Function: Paygo_Single_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      Paygo单机数据赋值
  * @param[in]  MOD_STRUCT_Pack *Pack_Modbus_Data  
                pack_node_struct *Pack_Can_Data    
                uint8_t type                       
  * @param[out] None
  * @return     void
  */
void Paygo_Single_Data_Can_to_Modbus(MOD_STRUCT_Pack *Pack_Modbus_Data, can_node_struct_Pack *Pack_Can_Data)
{
    // 30001~30007
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg30000_Pack_paygo + offsetof(MOD_STRUCT_reg30000, PAYG_ActCodeInput), 
        (uint8_t *)&Pack_Can_Data->pack_paygo + offsetof(bms_paygo_info_t, PAYG_ActCodeInput), 
        (offsetof(bms_paygo_info_t, PAYG_LastActTime) - offsetof(bms_paygo_info_t, PAYG_ActCodeInput)));

    // 30008~30011
    Pack_Modbus_Data->mod_reg30000_Pack_paygo.PAYG_SOH = Pack_Modbus_Data->mod_reg06100_Pack_each.soh;
    Pack_Modbus_Data->mod_reg30000_Pack_paygo.PAYG_SOC = Pack_Modbus_Data->mod_reg06100_Pack_each.soc;
    Pack_Modbus_Data->mod_reg30000_Pack_paygo.PAYG_SumPowerOutput = Pack_Modbus_Data->mod_reg06100_Pack_each.total_dsg_energy;

    // 30012~30017
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg30000_Pack_paygo + offsetof(MOD_STRUCT_reg30000, PAYG_LastActTime), 
        (uint8_t *)&Pack_Can_Data->pack_paygo + offsetof(bms_paygo_info_t, PAYG_LastActTime), 
        (offsetof(bms_paygo_info_t, CurrentTime) - offsetof(bms_paygo_info_t, PAYG_LastActTime)));

    // 30018~30021
    Pack_Modbus_Data->mod_reg30000_Pack_paygo.GPS_longitude = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.GPS_X;
    Pack_Modbus_Data->mod_reg30000_Pack_paygo.GPS_latitude = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.GPS_Y;

    // 30022~30027
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg30000_Pack_paygo + offsetof(MOD_STRUCT_reg30000, current_saved_timestamp), 
        (uint8_t *)&Pack_Can_Data->pack_paygo + offsetof(bms_paygo_info_t, CurrentTime), 
        (offsetof(bms_paygo_info_t, PayGo_ID) - offsetof(bms_paygo_info_t, CurrentTime)));

    // 30028~30029
    Pack_Modbus_Data->mod_reg30000_Pack_paygo.PayGo_ID = Pack_Can_Data->pack_paygo.PayGo_ID;
}

/*------------------------------------------------------------------------------
 Function: Parts_Single_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      配件单机数据赋值（根据获取到的CAN_TYPE）
  * @param[in]  uint8_t devId       
                uint8_t type  
  * @param[out] None
  * @return     void
  */
void Parts_Single_Data_Can_to_Modbus(uint8_t devId, uint8_t type)
{
    int numpack = 0;

#if 0   // 配件类全部使用私有协议

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    if((devId >= DC_HUB_CAN_ADDR) && (devId < (DC_HUB_CAN_ADDR + DC_HUB_MAX_NUM)))
    {
        uint8_t index = (devId - DC_HUB_CAN_ADDR) % DC_HUB_MAX_NUM;
        if(type == 0x40)//  DC_Hub设置区
        {
            memcpy((uint8_t *)&top_modbus_rd.DcHub[index].mod_reg15750_Dc_Ac_Hub_set, (uint8_t *)&can_node_rd.DCHUB[index].dc_ac_hub_setting, offsetof(dc_ac_hub_set,rw_cmd));// 15750~15751
        }    
        else if(type == 0x41)//  DC_Hub信息区
        {
            memcpy((uint8_t *)&top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info, (uint8_t *)&can_node_rd.DCHUB[index].dc_hub_info,(offsetof(dc_hub_info_struct,input_power) - offsetof(dc_hub_info_struct,dc_hub_type)));// 15700~15709
            // 15710~15711 来源于主动上报、不从主动轮询获取
            memcpy((uint8_t *)&top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info + offsetof(MOD_STRUCT_reg15700,input_current), (uint8_t *)&can_node_rd.DCHUB[index].dc_hub_info,(offsetof(dc_hub_info_struct,rw_cmd) - offsetof(dc_hub_info_struct,input_current)));// 15712~15749
        }
    }
#endif
#ifdef CONFIG_CAN_ACHUB_SUPPLY_ENABLE    
    else if((devId >= AC_HUB_CAN_ADDR) && (devId < (AC_HUB_CAN_ADDR + AC_HUB_MAX_NUM)))
    {
        uint8_t index = (devId - AC_HUB_CAN_ADDR) % AC_HUB_MAX_NUM;
        if(type == 0x42)//  AC_Hub信息区
        {
            // memcpy((uint8_t *)&top_modbus_rd.Inv[index].mod_reg15800_Ac_Hub_info, (uint8_t *)&Inv_can_mix.ac_hub_data[index].ac_hub_info,(offsetof(ac_hub_info_struct,SoftwareType) - offsetof(ac_hub_info_struct,ac_hub_type)));// 15800~15809
            //achub版本号统一放在judge_inv_min_version处理
        }
    }
#endif
#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
    else if ((devId >= DCDC_ARM_CAN_ADDR) && (devId < (DCDC_ARM_CAN_ADDR + DCDC_MAX_NUM)))
    {
        uint8_t index = (devId - DCDC_ARM_CAN_ADDR) % DCDC_MAX_NUM; // DCDC
        MOD_STRUCT_Dcdc *DCDC_Modbus_Data = &top_modbus_rd.Dcdc[index];
        can_node_struct_DCDC *DCDC_Can_Data = &can_node_rd.Dcdc[index];
        
        if(type == 0x48) // dcdc信息区
        {
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dev_type), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dev_type), (offsetof(dcdc_common_info_struct,battery_type) - offsetof(dcdc_common_info_struct,dev_type)));//15500~15509
            // 15510~15513 疑似未用
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,energy_line), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, energy_line), sizeof(DCDC_Can_Data->dcdc_common_info.energy_line));//15514
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,battery_type), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, battery_type), sizeof(DCDC_Can_Data->dcdc_common_info.battery_type));//15515
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,fault_charger1), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, fault_charger1), (offsetof(dcdc_common_info_struct,ctrl_mode) - offsetof(dcdc_common_info_struct,fault_charger1)));//15516~15525
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,ctrl_mode), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, ctrl_mode), sizeof(DCDC_Can_Data->dcdc_common_info.ctrl_mode));//15526
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,total_input_power), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, total_input_power), sizeof(DCDC_Can_Data->dcdc_common_info.total_input_power));//15527~15528
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,total_output_power), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, total_output_power), sizeof(DCDC_Can_Data->dcdc_common_info.total_output_power));//15529~15530
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dc_info), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dc_info), sizeof(DCDC_Can_Data->dcdc_common_info.dc_info));//15531~15554
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,TotalInputEnergy), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, TotalInputEnergy), sizeof(DCDC_Can_Data->dcdc_common_info.TotalInputEnergy));//15555~15556
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,energy_info), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, energy_info), sizeof(DCDC_Can_Data->dcdc_common_info.energy_info));//15557~15580
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dcdc_SoftwareType), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dcdc_SoftwareType), sizeof(DCDC_Can_Data->dcdc_common_info.dcdc_SoftwareType));//15581
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dcdc_SoftwareVersion), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dcdc_SoftwareVersion), sizeof(DCDC_Can_Data->dcdc_common_info.dcdc_SoftwareVersion));//15582~15583
        }
        else if(type == 0x49) // dcdc设置区
        {
            // Modbus和CAN协议对应顺序相同
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15600_DCDC_set, (uint8_t *)&DCDC_Can_Data->dcdc_common_set, offsetof(dcdc_common_set_struct, rw_cmd));//15600~
        }
    }
#endif

#endif

}

// 通用传输数据赋值
void Common_Transmit_Data_Can_to_Modbus(uint8_t devId)
{
    uint8_t index = 0;
    if ((devId >= INV_CAN_ADDR) && (devId < (INV_CAN_ADDR + INV_MAX_NUM)))
    {
        index = devId - INV_CAN_ADDR;
        memcpy(&top_modbus_rd.Inv[index].mod_reg40000_transparent, &can_node_rd.Inv[index].common_transmission, (sizeof(common_transmit_struct) - sizeof(rw_cmd_struct))); 
    }
    else if ((devId >= PACK_CAN_ADDR) &&( devId < (PACK_CAN_ADDR + PACK_MAX_NUM)))
    {
        index = devId - PACK_CAN_ADDR;
        memcpy(&top_modbus_rd.Pack[index].mod_reg40000_transparent, &can_node_rd.Pack[index].common_transmission, (sizeof(common_transmit_struct) - sizeof(rw_cmd_struct))); 
    } 
    else
    {
        ESP_LOGE(TAG, "Common_Transmit_Data_Can_to_Modbus : devId error(%d)", devId); 
    }
}

/*------------------------------------------------------------------------------
 Function: Single_CAN_To_Modbus_Read_Info_Process
 Can Beta，多字节读写协议
 -----------------------------------------------------------------------------*/
/**
  * @brief      此函数用于把从ARM读取的CAN数据赋值给将要发给app、蓝牙上位机的modbus表，顺序以CAN结构体向下排列
                Inv_can ->Inv (can->modbus)
  * @param[in]  uint8_t type  
  * @param[out] None
  * @return     void
  */
void CAN_To_Modbus_Read_Info_Process_Beta(uint8_t type, uint8_t devId)
{	
    int numpack = 0;
    int num = 0;
    uint8_t find_flag = 0;

    if (type == 0x01 || type == 0x02) {
        /*配件IOT信息（根据获取到的CAN_TYPE）*/
        // Parts_IoT_Single_Data_Can_to_Modbus(type);
        find_flag = 1;
    } else if ((type >= 0x10) && (type <= 0x26)) {
        for (numpack = 0; numpack < INV_MAX_NUM; numpack++) {
            /*逆变单机数据赋值（根据获取到的CAN_TYPE）*/
            Inv_Single_Data_Can_to_Modbus(&top_modbus_rd.Inv[numpack], &can_node_rd.Inv[numpack], type);
            find_flag = 1;
        }
    } else if (type == 0x27) {
        /*通用传输数据赋值*/
        Common_Transmit_Data_Can_to_Modbus(devId);
        find_flag = 1;
    } else if (((type >= 0x40) && (type <= 0x45)) 
                || ((type >= 0x48) && (type <= 0x4A))){
        /*配件单机数据赋值（根据获取到的CAN_TYPE）*/
        Parts_Single_Data_Can_to_Modbus(devId, type);
        find_flag = 1;
    } else if (((type >= 0x50) && (type <= 0x5A)) && ((devId >= PACK_CAN_ADDR) && (devId < (PACK_CAN_ADDR + PACK_MAX_NUM)))) {
        num = devId - PACK_CAN_ADDR; // PACK
#ifdef	BAT_CAN_PROTOCOL_BETA_ENABLE
        /*电池包单机数据赋值（根据获取到的CAN_TYPE）*/
        Pack_Single_Data_Can_to_Modbus(&top_modbus_rd.Pack[num], &can_node_rd.Pack[num], type, devId, num);
#endif
        find_flag = 1;
    } else if(type == 0x5B) {
#ifdef CONFIG_FUNC_PAYGO_ENABLE
        /*Paygo数据更新*/
        Paygo_Single_Data_Can_to_Modbus(&top_modbus_rd.Pack[0], &can_node_rd.Pack[0]);
#endif
        find_flag = 1;
    } else if (type == 0xFF || type == 0xFB || type == 0xFA) {
        // 标定信息、文件信息
        find_flag = 1;
    }

    if (!find_flag) {
        ESP_LOGE(TAG, "Single_CAN_To_Modbus_Read_Info_Process : type error!(type = %x)", type);
    }
}


inline uint8_t esp_canbus_myself_address(void)
{
	return reals.Addr_can_self + INV_IOT_CAN_ADDR;
}

 /*
modbus透传转发给CAN
*/
void sys_new_can_data_resend(can_cmd_queue_struct *can_cmd, uint8_t modbus_slave_address) 
{

    /* modbus指令转换为can指令发送到队列 */
    uint8_t can_cmd_flag = 0;
    if (can_cmd->cmd != NULL && can_cmd->num != 0) 
    {
        can_cmd->md_addr = modbus_slave_address;
        if (xQueueSendToBack(can_cmd_queue, can_cmd, pdMS_TO_TICKS(100)) == pdTRUE) 
        {
            can_cmd_flag = 1;
            reals.BLE_WIFI_to_Can_Cnt++;
        } 
    }
    if (can_cmd_flag == 0 && can_cmd->cmd != NULL)
    {
        free(can_cmd->cmd);
        can_cmd->cmd = NULL;
    }
}

/*------------------------------------------------------------------------------
 Function: CAN_DEV_SN_Get_to_Save
 -----------------------------------------------------------------------------*/
/**
  * @brief      设备SN检查
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void  CAN_DEV_SN_Get_to_Save(void)//1s cycle
{ 
	uint8_t change_flag =0;

    if((reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)// get INV  SN
            &&(reals.online_Inv_num > 0)    //      
        &&(0 != can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_sn)
        &&(0 != strlen(can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_type)) //非零
        &&((dev_factory.dev_sn != can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_sn)
        ||(0 != memcmp(dev_factory.dev_type,  can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_type,  sizeof(can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_type)))) //变化
        )
    {
        dev_factory.dev_sn = can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_sn;
        memcpy(dev_factory.dev_type,  can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_type,  sizeof(can_node_rd.Inv[reals.Addr_can_self].inv_about.dev_type));
        nvs_store_write(DEV_FACTORY, (const uint8_t *)&dev_factory, sizeof(dev_factory));   // 变化时立即写入flash，避免重复标定可能产生的问题
        change_flag = 1;//广播逆变器SN码蓝牙名
        
        char ssid[30] = {0};
        snprintf(ssid, sizeof(ssid), "%s%llu", dev_factory.dev_type, dev_factory.dev_sn);
        strncpy(IotSetData.dev_info_t.wifi_AP_ssid, ssid, sizeof(IotSetData.dev_info_t.wifi_AP_ssid));
        reals.SetDataWrFlag.sBit.wifi_ap_ssid = 1;
        ESP_LOGW(TAG, "CAN_DEV_SN_Get_to_Save : %s", ssid);
    }

	if( change_flag)
	{
		ESP_LOGI(TAG,"change_flag:%d",change_flag);
		iot_ble_new_name(dev_factory.dev_type, dev_factory.dev_sn);
        iot_mqtt_new_dev(dev_factory.dev_type, dev_factory.dev_sn);
		change_flag=0;
	}
}

/*------------------------------------------------------------------------------
 Function: Inv_Single_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      逆变单机数据赋值（根据获取到的CAN_TYPE）
  * @param[in]  MOD_STRUCT_Inv *Inv_Modbus_Data  
                inv_node_struct *Inv_Can_Data    
                uint8_t type                     
  * @param[out] None
  * @return     void
  */
void Inv_Single_Data_Can_to_Modbus(MOD_STRUCT_Inv *Inv_Modbus_Data, can_node_struct_Inv *Inv_Can_Data, uint8_t type)
{
    if(type == 0x11) 
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_num), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,inv_num),sizeof(Inv_Can_Data->inv_base.inv_num));// 120
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_online), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,inv_online),sizeof(Inv_Can_Data->inv_base.inv_online));// 121
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_power_rang), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_Can_Data->inv_base.inv_power_rang));// 122
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,inv_power_rang), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_Can_Data->inv_base.inv_power_rang));// 1111(单机)
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,line_event), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,line_event),sizeof(Inv_Can_Data->inv_base.line_event));// 123
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,line_event), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,line_event),sizeof(Inv_Can_Data->inv_base.line_event));// 1131
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,ctrl_status), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,ctrl_status),sizeof(Inv_Can_Data->inv_base.ctrl_status));// 124
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,ctrl_status_L), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,ctrl_status),sizeof(Inv_Can_Data->inv_base.ctrl_status));// 1132
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,GridandMachineSOC), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,grid_par_soc),sizeof(Inv_Can_Data->inv_base.grid_par_soc));// 125
#if 0 // 100段使用主动上报数据      
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,alarm), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,alarm),sizeof(Inv_Can_Data->inv_base.alarm));// 126~129
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,fault),sizeof(Inv_Can_Data->inv_base.fault));// 133~136 故障信息
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault[4]), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,fault5),sizeof(Inv_Can_Data->inv_base.fault5));// 137 故障信息
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault[5]), (uint8_t *)&Inv_Can_Data->inv_announce + offsetof(inv_announce_struct,fault6),sizeof(Inv_Can_Data->inv_announce.fault6));// 138 故障信息
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,alarm), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,alarm),sizeof(Inv_Can_Data->inv_base.alarm));// 1134~1137
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,fault), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,fault),sizeof(Inv_Can_Data->inv_base.fault));// 1141~1144 故障信息
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,fault[4]), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,fault5),sizeof(Inv_Can_Data->inv_base.fault5));// 1145 故障信息
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,fault[5]), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,fault6),sizeof(Inv_Can_Data->inv_base.fault6));// 1146 故障信息

        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,DCLoadAllTotalPower), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,DCLoadAllTotalPower),(offsetof(inv_base_struct,fault5) - offsetof(inv_base_struct,DCLoadAllTotalPower)));// 140~159
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_work_state), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,inv_work_state),sizeof(Inv_Can_Data->inv_base.inv_work_state));// 161
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PvToACLoadEnergy), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,PvToACLoadEnergy),sizeof(Inv_Can_Data->inv_base.PvToACLoadEnergy));// 162~163
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,SelfConsumptionPercent), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,SelfConsumptionPercent),sizeof(Inv_Can_Data->inv_base.SelfConsumptionPercent));// 164
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PVToACloadPower), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,PVToACloadPower),sizeof(Inv_Can_Data->inv_base.PVToACloadPower));// 165~166
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackTotalDsgEnergy), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,PackTotalDsgEnergy),sizeof(Inv_Can_Data->inv_base.PackTotalDsgEnergy));// 167~168
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,TotalEnergy_Car), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,TotalEnergy_Car),sizeof(Inv_Can_Data->inv_base.TotalEnergy_Car));// 176-177
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Sleep_endurance), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,Sleep_endurance),sizeof(Inv_Can_Data->inv_base.Sleep_endurance));// 178-179
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackTotalChgEnergy), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,PackTotalChgEnergy),sizeof(Inv_Can_Data->inv_base.PackTotalChgEnergy));// 180-181
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,CarTotalPower), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,CarTotalPower),sizeof(Inv_Can_Data->inv_base.CarTotalPower));// 182
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,INV_TEST), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,INV_TEST),sizeof(Inv_Can_Data->inv_base.INV_TEST));// 186
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Switch_memory_states), (uint8_t *)&Inv_Can_Data->inv_base + offsetof(inv_base_struct,Switch_memory_states),sizeof(Inv_Can_Data->inv_base.Switch_memory_states));// 191
    }
    else if(type == 0x12)
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,line_event), (uint8_t *)&Inv_Can_Data->inv_line + offsetof(inv_line_struct,line_event),sizeof(Inv_Can_Data->inv_line.line_event));// 123
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,line_event), (uint8_t *)&Inv_Can_Data->inv_line + offsetof(inv_line_struct,line_event),sizeof(Inv_Can_Data->inv_line.line_event));// 1131
    }
    else if(type == 0x13)
    {
#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
        memcpy(Inv_Modbus_Data->mod_reg00100_AppPage1.dev_type, dev_factory.dev_type, TYPE_SIZE);// 110~115  ASCII 机型
        Inv_Modbus_Data->mod_reg00100_AppPage1.dev_sn = dev_factory.dev_sn;// 116~119  设备唯一识别SN码
        memcpy(Inv_Modbus_Data->mod_reg01100_Inv_base.InvType, dev_factory.dev_type, TYPE_SIZE);// 1101~1106  ASCII 机型(单个)
        Inv_Modbus_Data->mod_reg01100_Inv_base.InvSN = dev_factory.dev_sn;// 1107~1110  设备唯一识别SN码(单个)
#else
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,dev_type), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_Can_Data->inv_about.dev_type));// 110~115  ASCII 机型
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,dev_sn), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_Can_Data->inv_about.dev_sn));// 116~119  设备唯一识别SN码
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvType), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_Can_Data->inv_about.dev_type));// 1101~1106  ASCII 机型(单个)
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvSN), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_Can_Data->inv_about.dev_sn));// 1107~1110  设备唯一识别SN码(单个)
        CAN_DEV_SN_Get_to_Save();
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Rated_Voltage), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,Rated_Voltage),sizeof(Inv_Can_Data->inv_about.Rated_Voltage));// 169
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,Rated_Frequency), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,Rated_Frequency),sizeof(Inv_Can_Data->inv_about.Rated_Frequency));// 170
        
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,DevVoltageLable), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,voltage_lable),sizeof(Inv_Can_Data->inv_about.voltage_lable));// 1149
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,WorkTimeNumber), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,time_area_num),sizeof(Inv_Can_Data->inv_about.time_area_num));// 1148
#if 0
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,software_total), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,software_total),sizeof(Inv_Can_Data->inv_about.software_total));// 1112
        // 1113~1130
        for(int i = 0;i < Inv_Can_Data->inv_about.software_total && i < 6;i++) {
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base.soft[i].type, (uint8_t *)&Inv_Can_Data->inv_about.soft[i].type,sizeof(Inv_Can_Data->inv_about.soft[i].type));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base.soft[i].version, (uint8_t *)&Inv_Can_Data->inv_about.soft[i].version,sizeof(Inv_Can_Data->inv_about.soft[i].version));
        }
#else
        uint8_t total = 0;
        // 1113~1130
        for(int i = 0;i < Inv_Can_Data->inv_about.software_total && i < 6;i++) {
            if ( 0 != Inv_Can_Data->inv_about.soft[i].version ) {
                memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base.soft[total].type, (uint8_t *)&Inv_Can_Data->inv_about.soft[i].type,sizeof(Inv_Can_Data->inv_about.soft[i].type));
                memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base.soft[total].version, (uint8_t *)&Inv_Can_Data->inv_about.soft[i].version,sizeof(Inv_Can_Data->inv_about.soft[i].version));
                total++;
            }
        }
        Inv_Modbus_Data->mod_reg01100_Inv_base.software_total = total;
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,P_INV_chg_limit_L1), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,P_INV_chg_limit_L1),(3*sizeof(Inv_Can_Data->inv_about.P_INV_chg_limit_L1))); //1155~1157
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,P_INV_disg_limit_L1), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,P_INV_disg_limit_L1),(3*sizeof(Inv_Can_Data->inv_about.P_INV_disg_limit_L1))); //1158~1160
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machine_l1_ac_input_rated_current), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,machine_l1_ac_input_rated_current),(3*sizeof(Inv_Can_Data->inv_about.machine_l1_ac_input_rated_current))); //1161~1163
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,machine_l1_ac_output_rated_current), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,machine_l1_ac_output_rated_current),(3*sizeof(Inv_Can_Data->inv_about.machine_l1_ac_output_rated_current))); //1164~1166
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,grid_l1_ac_output_rated_current), (uint8_t *)&Inv_Can_Data->inv_about + offsetof(inv_about_struct,grid_l1_ac_output_rated_current),(3*sizeof(Inv_Can_Data->inv_about.grid_l1_ac_output_rated_current))); //1167~1169
    }
    else if(type == 0x14)
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,total_chg_power), (uint8_t *)&Inv_Can_Data->inv_pv + offsetof(inv_pv_struct,total_chg_power),(offsetof(inv_pv_struct,pv_number) - offsetof(inv_pv_struct,total_chg_power)));// 1200~1203
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01200_Inv_pv + offsetof(MOD_STRUCT_reg01200,pv_number), (uint8_t *)&Inv_Can_Data->inv_pv + offsetof(inv_pv_struct,pv_number),sizeof(Inv_Can_Data->inv_pv.pv_number));// 1209
        for(int i = 0;i < 10;i++)// 1210~1289
        {
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[i].status, (uint8_t *)&Inv_Can_Data->inv_pv.pv_detail[i].status,sizeof(Inv_Can_Data->inv_pv.pv_detail[i].status));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[i].input_type, (uint8_t *)&Inv_Can_Data->inv_pv.pv_detail[i].input_type,sizeof(Inv_Can_Data->inv_pv.pv_detail[i].input_type));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[i].input_power, (uint8_t *)&Inv_Can_Data->inv_pv.pv_detail[i].input_power,sizeof(Inv_Can_Data->inv_pv.pv_detail[i].input_power));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[i].input_voltage, (uint8_t *)&Inv_Can_Data->inv_pv.pv_detail[i].input_voltage,sizeof(Inv_Can_Data->inv_pv.pv_detail[i].input_voltage));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01200_Inv_pv.pv_detail[i].input_current, (uint8_t *)&Inv_Can_Data->inv_pv.pv_detail[i].input_current,sizeof(Inv_Can_Data->inv_pv.pv_detail[i].input_current));
        }
    }
    else if(type == 0x15)
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,freq), (uint8_t *)&Inv_Can_Data->inv_grid + offsetof(inv_grid_struct,freq),(offsetof(inv_grid_struct,grid_phase_number) - offsetof(inv_grid_struct,freq)));// 1300~1306
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,pf), (uint8_t *)&Inv_Can_Data->inv_grid + offsetof(inv_grid_struct,pf),sizeof(Inv_Can_Data->inv_grid.pf));// 1311
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid + offsetof(MOD_STRUCT_reg01300,grid_phase_number), (uint8_t *)&Inv_Can_Data->inv_grid + offsetof(inv_grid_struct,grid_phase_number),sizeof(Inv_Can_Data->inv_grid.grid_phase_number));// 1312
        for(int i = 0;i < 3;i++)// 1313~1330
        {
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[i].input_power, (uint8_t *)&Inv_Can_Data->inv_grid.grid_detail[i].input_power,sizeof(Inv_Can_Data->inv_grid.grid_detail[i].input_power));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[i].input_voltage, (uint8_t *)&Inv_Can_Data->inv_grid.grid_detail[i].input_voltage,sizeof(Inv_Can_Data->inv_grid.grid_detail[i].input_voltage));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[i].input_current, (uint8_t *)&Inv_Can_Data->inv_grid.grid_detail[i].input_current,sizeof(Inv_Can_Data->inv_grid.grid_detail[i].input_current));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.GridPower_U32[i], (uint8_t *)&Inv_Can_Data->inv_grid.GridPower_U32[i],sizeof(Inv_Can_Data->inv_grid.GridPower_U32[i]));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.GridPapparent_U32[i], (uint8_t *)&Inv_Can_Data->inv_grid.GridPapparent_U32[i],sizeof(Inv_Can_Data->inv_grid.GridPapparent_U32[i]));
        }
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[0].input_papparent, (uint8_t *)&Inv_Can_Data->inv_grid.Grid1Papparent,sizeof(Inv_Can_Data->inv_grid.Grid1Papparent));
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[1].input_papparent, (uint8_t *)&Inv_Can_Data->inv_grid.Grid2Papparent,sizeof(Inv_Can_Data->inv_grid.Grid2Papparent));
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01300_Inv_grid.grid_detail[2].input_papparent, (uint8_t *)&Inv_Can_Data->inv_grid.Grid3Papparent,sizeof(Inv_Can_Data->inv_grid.Grid3Papparent));
    }
    else if(type == 0x16)
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,total_dc_load_power), (uint8_t *)&Inv_Can_Data->inv_load + offsetof(inv_load_struct,total_dc_load_power),(offsetof(inv_load_struct,total_ac_load_power) - offsetof(inv_load_struct,total_dc_load_power)));// 1400~1409
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,DCLoadTotalVoltage), (uint8_t *)&Inv_Can_Data->inv_load + offsetof(inv_load_struct,DCLoadTotalVoltage),sizeof(Inv_Can_Data->inv_load.DCLoadTotalVoltage));// 1412
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,DCLoadTotalCurrent), (uint8_t *)&Inv_Can_Data->inv_load + offsetof(inv_load_struct,DCLoadTotalCurrent),sizeof(Inv_Can_Data->inv_load.DCLoadTotalCurrent));// 1413
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,pf), (uint8_t *)&Inv_Can_Data->inv_load + offsetof(inv_load_struct,pf),sizeof(Inv_Can_Data->inv_load.pf));// 1419
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,total_ac_load_power), (uint8_t *)&Inv_Can_Data->inv_load + offsetof(inv_load_struct,total_ac_load_power),(offsetof(inv_load_struct,ac_phase_number) - offsetof(inv_load_struct,total_ac_load_power)));// 1420~1423
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load + offsetof(MOD_STRUCT_reg01400,ac_phase_number), (uint8_t *)&Inv_Can_Data->inv_load + offsetof(inv_load_struct,ac_phase_number),sizeof(Inv_Can_Data->inv_load.ac_phase_number));// 1429
        for(int i = 0;i < 3;i++)// 1430~1447
        {
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[i].load_power, (uint8_t *)&Inv_Can_Data->inv_load.ac_load[i].load_power,sizeof(Inv_Can_Data->inv_load.ac_load[i].load_power));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[i].load_voltage, (uint8_t *)&Inv_Can_Data->inv_load.ac_load[i].load_voltage,sizeof(Inv_Can_Data->inv_load.ac_load[i].load_voltage));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[i].load_current, (uint8_t *)&Inv_Can_Data->inv_load.ac_load[i].load_current,sizeof(Inv_Can_Data->inv_load.ac_load[i].load_current));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ACLoadPower_U32[i], (uint8_t *)&Inv_Can_Data->inv_load.ACLoadPower_U32[i],sizeof(Inv_Can_Data->inv_load.ACLoadPower_U32[i]));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ACLoadPapparent_U32[i], (uint8_t *)&Inv_Can_Data->inv_load.ACLoadPapparent_U32[i],sizeof(Inv_Can_Data->inv_load.ACLoadPapparent_U32[i]));
        }
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[0].load_papparent, (uint8_t *)&Inv_Can_Data->inv_load.ACLoad1Papparent,sizeof(Inv_Can_Data->inv_load.ACLoad1Papparent));
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[1].load_papparent, (uint8_t *)&Inv_Can_Data->inv_load.ACLoad2Papparent,sizeof(Inv_Can_Data->inv_load.ACLoad2Papparent));
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[2].load_papparent, (uint8_t *)&Inv_Can_Data->inv_load.ACLoad3Papparent,sizeof(Inv_Can_Data->inv_load.ACLoad3Papparent));
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[0].load_preactive, (uint8_t *)&Inv_Can_Data->inv_load.ACLoad1Preactive,sizeof(Inv_Can_Data->inv_load.ACLoad1Preactive));
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[1].load_preactive, (uint8_t *)&Inv_Can_Data->inv_load.ACLoad2Preactive,sizeof(Inv_Can_Data->inv_load.ACLoad2Preactive));
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01400_Inv_load.ac_load[2].load_preactive, (uint8_t *)&Inv_Can_Data->inv_load.ACLoad3Preactive,sizeof(Inv_Can_Data->inv_load.ACLoad3Preactive));
    }
    else if(type == 0x17)
    {        
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,freq), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,freq),sizeof(Inv_Can_Data->inv_data.freq));// 1500
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,total_energy), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,total_energy),sizeof(Inv_Can_Data->inv_data.total_energy));// 1501~1502 逆变的总能量
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,phase_number), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,phase_number),sizeof(Inv_Can_Data->inv_data.phase_number));// 1508 相位数量 最多3相
        for(int i = 0;i < 3;i++)// 1509~1529 每相详细信息 最多3相
        {
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[i].work_status, (uint8_t *)&Inv_Can_Data->inv_data.inv_detail[i].work_status,sizeof(Inv_Can_Data->inv_data.inv_detail[i].work_status));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[i].power, (uint8_t *)&Inv_Can_Data->inv_data.inv_detail[i].power,sizeof(Inv_Can_Data->inv_data.inv_detail[i].power));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[i].voltage, (uint8_t *)&Inv_Can_Data->inv_data.inv_detail[i].voltage,sizeof(Inv_Can_Data->inv_data.inv_detail[i].voltage));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv.inv_detail[i].current, (uint8_t *)&Inv_Can_Data->inv_data.inv_detail[i].current,sizeof(Inv_Can_Data->inv_data.inv_detail[i].current));
        }
        
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,AmbientTemp), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,AmbientTemp),sizeof(Inv_Can_Data->inv_data.AmbientTemp));        // 1530 逆变器环境温度
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,InvMaxTemp), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,InvMaxTemp),sizeof(Inv_Can_Data->inv_data.InvMaxTemp));           // 1531 逆变器最高温度
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,PvDcdcMaxTemp), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,PvDcdcMaxTemp),sizeof(Inv_Can_Data->inv_data.PvDcdcMaxTemp));  // 1532 光伏DCDC优化器最高温度
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,Bms_current_in_VPP), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,Bms_current_in_VPP),sizeof(Inv_Can_Data->inv_data.Bms_current_in_VPP));  // 1533
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01500_Inv_inv + offsetof(MOD_STRUCT_reg01500,MeterSynchronization), (uint8_t *)&Inv_Can_Data->inv_data + offsetof(inv_data_struct,MeterSynchronization),sizeof(Inv_Can_Data->inv_data.MeterSynchronization));  // 1534
    }
    else if(type == 0x18)
    {

    }
    else if(type == 0x19)
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg01600_Inv_generator + offsetof(MOD_STRUCT_reg01600,total_energy), (uint8_t *)&Inv_Can_Data->inv_gen + offsetof(inv_gen_struct,total_energy),(offsetof(inv_gen_struct,rw_cmd) - offsetof(inv_gen_struct,total_energy)));// 1600~1604
    }
    else if(type == 0x1a)// 可写
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,ctrl_chg_mode), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_chg_mode),sizeof(Inv_Can_Data->inv_set00.ctrl_chg_mode));// 160

#if 0 // 时间完全由IOT控制       
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,mon), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,time1),(offsetof(inv_set00_struct,work_mode) - offsetof(inv_set00_struct,time1)));// 2000~2004
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,work_mode), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,work_mode),sizeof(Inv_Can_Data->inv_set00.work_mode));// 2005
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl),sizeof(Inv_Can_Data->inv_set00.ctrl));// 2006
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_led), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_led),sizeof(Inv_Can_Data->inv_set00.ctrl_led));// 2007
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_meter), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_meter),sizeof(Inv_Can_Data->inv_set00.ctrl_meter));//  2008
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_pv), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_pv),sizeof(Inv_Can_Data->inv_set00.ctrl_pv));// 2009
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_inv),sizeof(Inv_Can_Data->inv_set00.ctrl_inv));// 2010
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_ac),sizeof(Inv_Can_Data->inv_set00.ctrl_ac));// 2011
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_dc),sizeof(Inv_Can_Data->inv_set00.ctrl_dc));// 2012
#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5    
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_poweron), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_poweron),sizeof(Inv_Can_Data->inv_set00.ctrl_poweron));// 2013
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco),sizeof(Inv_Can_Data->inv_set00.ctrl_dc_eco));// 2014
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco_time), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco_time),sizeof(Inv_Can_Data->inv_set00.ctrl_dc_eco_time));// 2015
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_dc_power_value), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,eco_dc_power_value),sizeof(Inv_Can_Data->inv_set00.eco_dc_power_value));// 2016
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco),sizeof(Inv_Can_Data->inv_set00.ctrl_ac_eco));// 2017
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco_time), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco_time),sizeof(Inv_Can_Data->inv_set00.ctrl_ac_eco_time));// 2018
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_ac_power_value), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,eco_ac_power_value),sizeof(Inv_Can_Data->inv_set00.eco_ac_power_value));// 2019
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_chg_mode), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_chg_mode),sizeof(Inv_Can_Data->inv_set00.ctrl_chg_mode));// 2020
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_super_power), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_super_power),sizeof(Inv_Can_Data->inv_set00.ctrl_super_power));// 2021
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_low_cap_pct), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_low_cap_pct),sizeof(Inv_Can_Data->inv_set00.ctrl_low_cap_pct));// 2022
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_high_cap_pct), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_high_cap_pct),sizeof(Inv_Can_Data->inv_set00.ctrl_high_cap_pct));// 2023
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv_mode), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_inv_mode),sizeof(Inv_Can_Data->inv_set00.ctrl_inv_mode));// 2024
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dev_id), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_dev_id),sizeof(Inv_Can_Data->inv_set00.ctrl_dev_id));// 2025
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_all_energy_type), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_all_energy_type),sizeof(Inv_Can_Data->inv_set00.ctrl_all_energy_type));// 2026
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_now_energy_type), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_now_energy_type),sizeof(Inv_Can_Data->inv_set00.ctrl_now_energy_type));// 2027
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_log_page), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_log_page),sizeof(Inv_Can_Data->inv_set00.ctrl_log_page));// 2028
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time_area), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_time_area),sizeof(Inv_Can_Data->inv_set00.ctrl_time_area));// 2029
        for(int i = 0;i < 10;i++)// 2030~2059
        {
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set.ctrl_time[i].lable, (uint8_t *)&Inv_Can_Data->inv_set00.ctrl_time[i].lable,sizeof(Inv_Can_Data->inv_set00.ctrl_time[i].lable));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set.ctrl_time[i].start, (uint8_t *)&Inv_Can_Data->inv_set00.ctrl_time[i].start,sizeof(Inv_Can_Data->inv_set00.ctrl_time[i].start));
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set.ctrl_time[i].end, (uint8_t *)&Inv_Can_Data->inv_set00.ctrl_time[i].end,sizeof(Inv_Can_Data->inv_set00.ctrl_time[i].end));
        }
        for(int i = 0;i < 6;i++)// 2060~2065
        {
            memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set.ctrl_PvType[i], (uint8_t *)&Inv_Can_Data->inv_set00.ctrl_PvType[i],sizeof(Inv_Can_Data->inv_set00.ctrl_PvType[i]));
        }
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_alarm_voice), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ctrl_alarm_voice),sizeof(Inv_Can_Data->inv_set00.ctrl_alarm_voice));// 2066
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_lcd_active_time), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,setLcdActiveTime),sizeof(Inv_Can_Data->inv_set00.setLcdActiveTime));// 2067 
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Custom_mode_set), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Custom_mode_set),sizeof(Inv_Can_Data->inv_set00.Custom_mode_set));// 2072
#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5 
        Inv_Modbus_Data->mod_reg02000_Inv_base_set.Remote_set.all = IotSetData.dev_info_t.Remote_set.all;   // 2073
        Inv_Modbus_Data->mod_reg02000_Inv_base_set.Remote_set_soc_limit = IotSetData.dev_info_t.Remote_set_soc_limit;   // 2074
#else
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Remote_set), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Remote_set),sizeof(Inv_Can_Data->inv_set00.Remote_set));// 2073
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Remote_set_soc_limit), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Remote_set_soc_limit),sizeof(Inv_Can_Data->inv_set00.Remote_set_soc_limit));// 2074
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Rv_SOC_Ownership), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Rv_SOC_Ownership),sizeof(Inv_Can_Data->inv_set00.Rv_SOC_Ownership));// 2075
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,LevelSwitch), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,LevelSwitch),sizeof(Inv_Can_Data->inv_set00.LevelSwitch));// 2076
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,sleep_remain_time), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,sleep_remain_time),sizeof(Inv_Can_Data->inv_set00.sleep_remain_time));// 2077
#ifdef CONFIG_IOT_LED_SET_ENABLE
        Inv_Modbus_Data->mod_reg02000_Inv_base_set.LED_SET.all = IotSetData.dev_info_t.LED_SET.all;   // 2078
#else         
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,LED_SET), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,led_set),sizeof(Inv_Can_Data->inv_set00.led_set));// 2078
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Remote_set_power), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,remote_set_power),sizeof(Inv_Can_Data->inv_set00.remote_set_power));// 2079
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Pack_Set_Show), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,pack_set_show),sizeof(Inv_Can_Data->inv_set00.pack_set_show));// 2080
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,INV_Set_Show), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,inv_set_show),sizeof(Inv_Can_Data->inv_set00.inv_set_show));// 2081
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DCDC_Set_Show), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,dcdc_set_show),sizeof(Inv_Can_Data->inv_set00.dcdc_set_show));// 2082
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,SOC_H), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,soc_h),sizeof(Inv_Can_Data->inv_set00.soc_h));// 2083
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,PV_SET), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,pv_set),sizeof(Inv_Can_Data->inv_set00.pv_set));// 2084
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DC_output), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,dc_output),sizeof(Inv_Can_Data->inv_set00.dc_output));// 2085
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Regulatory_set), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Regulatory_set),sizeof(Inv_Can_Data->inv_set00.Regulatory_set));// 2086
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_capacity), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Cycle_capacity),sizeof(Inv_Can_Data->inv_set00.Cycle_capacity));// 2087
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_max_capacity), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Cycle_max_capacity),sizeof(Inv_Can_Data->inv_set00.Cycle_max_capacity));// 2088
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ym), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Effective_time_mon), sizeof(Inv_Can_Data->inv_set00.Effective_time_mon)*2);// 2089
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_dh), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Effective_time_hour), sizeof(Inv_Can_Data->inv_set00.Effective_time_hour)*2);// 2090
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ms), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,Effective_time_sec), sizeof(Inv_Can_Data->inv_set00.Effective_time_sec)*2);// 2091
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ECO_status), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,ECO_status),sizeof(Inv_Can_Data->inv_set00.ECO_status));// 2092
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_AC_branch), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,set_AC_branch),sizeof(Inv_Can_Data->inv_set00.set_AC_branch));// 2093
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_DC_branch), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,set_DC_branch),sizeof(Inv_Can_Data->inv_set00.set_DC_branch));// 2094
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DC_SET), (uint8_t *)&Inv_Can_Data->inv_set00 + offsetof(inv_set00_struct,DC_SET),sizeof(Inv_Can_Data->inv_set00.DC_SET));// 2095

#if INV_MAX_NUM == 1
        // 写入回读逻辑，立即汇总
        if ( 1 == reals.can_poll[INDEX_INV_TYPE_CONFIG00_1AH].fast_sum ) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg02000_Inv_base_set, (uint8_t *)&Inv_Modbus_Data->mod_reg02000_Inv_base_set, sizeof(MOD_STRUCT_reg02000));
            reals.can_poll[INDEX_INV_TYPE_CONFIG00_1AH].fast_sum = 0;
        }
#endif
    }
    else if(type == 0x1b)// 可写
    {        
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,password), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,password),sizeof(Inv_Can_Data->inv_set01.password));// 2200~2203
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_reset_factory), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_reset_factory),sizeof(Inv_Can_Data->inv_set01.ctrl_reset_factory));// 2206
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_grid),sizeof(Inv_Can_Data->inv_set01.ctrl_grid));// 2207
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_feedback), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_feedback),sizeof(Inv_Can_Data->inv_set01.ctrl_feedback));// 2208
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_output_inv_volt), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_output_inv_volt),(offsetof(inv_set01_struct,ctrl_user_area) - offsetof(inv_set01_struct,ctrl_output_inv_volt)));// 2209~2217
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_user_area), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_user_area),sizeof(Inv_Can_Data->inv_set01.ctrl_user_area));// 2218
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle),sizeof(Inv_Can_Data->inv_set01.ctrl_pv_paralle));// 2219~2224
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid_plus), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_grid_plus),sizeof(Inv_Can_Data->inv_set01.ctrl_grid_plus));// 2225
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_save_power_state), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_save_power_state),sizeof(Inv_Can_Data->inv_set01.ctrl_save_power_state));// 2226
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_enable), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_meter_enable),sizeof(Inv_Can_Data->inv_set01.ctrl_meter_enable));// 2227
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_select), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_meter_select),sizeof(Inv_Can_Data->inv_set01.ctrl_meter_select));// 2228
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_Multi_enable), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_Multi_enable),sizeof(Inv_Can_Data->inv_set01.ctrl_Inv_Multi_enable));// 2229
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_addr_Set),sizeof(Inv_Can_Data->inv_set01.ctrl_Inv_addr_Set));// 2230
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_test), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ct_test),(offsetof(inv_set01_struct,ctrl_mix2) - offsetof(inv_set01_struct,ct_test)));// 2231~2232
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ems_ctrl), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ems_ctrl),sizeof(Inv_Can_Data->inv_set01.ems_ctrl));// 2241
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_mix2), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ctrl_mix2),sizeof(Inv_Can_Data->inv_set01.ctrl_mix2));// 2242
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ChargingPile_SET), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ChargingPile_SET),sizeof(Inv_Can_Data->inv_set01.ChargingPile_SET));// 2243
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_ratio), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,ct_ratio),sizeof(Inv_Can_Data->inv_set01.ct_ratio));// 2244
#ifdef CONFIG_SMART_GENERATOR_ENABLE
        Inv_Modbus_Data->mod_reg02200_Inv_advance_set.GenSet.ctrl_mode = IotSetData.dev_info_t.Gen_ctrl.ctrl_mode;              // 2246
        Inv_Modbus_Data->mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl = IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl;  // 2246
        Inv_Modbus_Data->mod_reg02200_Inv_advance_set.Gen_SOC_Start = IotSetData.dev_info_t.Gen_ctrl.soc_min;                   // 2247
        Inv_Modbus_Data->mod_reg02200_Inv_advance_set.Gen_SOC_Stop = IotSetData.dev_info_t.Gen_ctrl.soc_max;                    // 2248
#else
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,GenSet), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,GenSet),(offsetof(inv_set01_struct,Undervoltage_protection) - offsetof(inv_set01_struct,GenSet)));// 2246~2257   res1
#endif
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection),sizeof(Inv_Can_Data->inv_set01.Undervoltage_protection));// 2258
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection_time), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection_time),sizeof(Inv_Can_Data->inv_set01.Undervoltage_protection_time));// 2259
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection),sizeof(Inv_Can_Data->inv_set01.Highvoltage_protection));// 2260
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection_time), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection_time),sizeof(Inv_Can_Data->inv_set01.Highvoltage_protection_time));// 2261
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection),sizeof(Inv_Can_Data->inv_set01.Underfrequency_protection));// 2262
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection_time), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection_time),sizeof(Inv_Can_Data->inv_set01.Underfrequency_protection_time));// 2263
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection),sizeof(Inv_Can_Data->inv_set01.Overvoltage_protection));// 2264
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection_time), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection_time),sizeof(Inv_Can_Data->inv_set01.Overvoltage_protection_time));// 2265
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,SetCtrlMeter),sizeof(Inv_Can_Data->inv_set01.SetCtrlMeter));// 2267
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetCtrlPv), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,setting_pv),sizeof(Inv_Can_Data->inv_set01.setting_pv));// 2269
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Phase_set), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,Phase_set),sizeof(Inv_Can_Data->inv_set01.Phase_set));// 2270
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,DCHUB_set), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,DCHUB_set),sizeof(Inv_Can_Data->inv_set01.DCHUB_set));// 2271
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetGridMaxCurrent_in), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,SetGridMaxCurrent_in),sizeof(Inv_Can_Data->inv_set01.SetGridMaxCurrent_in));// 2272
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,FUNC_set), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,FUNC_set),sizeof(Inv_Can_Data->inv_set01.FUNC_set));// 2273
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvSettings), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,RvSettings),sizeof(Inv_Can_Data->inv_set01.RvSettings));// 2274
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,AltDelays_ShutdownTime), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,AltDelays_ShutdownTime),sizeof(Inv_Can_Data->inv_set01.AltDelays_ShutdownTime));// 2275
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvOnOff), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,RvOnOff),sizeof(Inv_Can_Data->inv_set01.RvOnOff));// 2276
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvBatteryCapacity), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,RvBatteryCapacity),sizeof(Inv_Can_Data->inv_set01.RvBatteryCapacity));// 2277~2278
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,BatterySettings), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,BatterySettings),sizeof(Inv_Can_Data->inv_set01.BatterySettings));// 2279
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,heat_pump_auto_set_Active_Power), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,heat_pump_auto_set_Active_Power),sizeof(Inv_Can_Data->inv_set01.heat_pump_auto_set_Active_Power));// 2282
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,heat_pump_auto_set_minimum_Runtime), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,heat_pump_auto_set_minimum_Runtime),sizeof(Inv_Can_Data->inv_set01.heat_pump_auto_set_minimum_Runtime));// 2283
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,heat_pump_auto_set_Delay_Start_Time), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,heat_pump_auto_set_Delay_Start_Time),sizeof(Inv_Can_Data->inv_set01.heat_pump_auto_set_Delay_Start_Time));// 2284
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetFeedbackMode), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,SetFeedbackMode),sizeof(Inv_Can_Data->inv_set01.SetFeedbackMode));// 2303
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,PvMultiPeak), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,PvMultiPeak),sizeof(Inv_Can_Data->inv_set01.PvMultiPeak));// 2304
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,inv_meter_interaction), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,inv_meter_interaction),sizeof(Inv_Can_Data->inv_set01.inv_meter_interaction));// 2305
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,AC_Input_Ctrl), (uint8_t *)&Inv_Can_Data->inv_set01 + offsetof(inv_set01_struct,AC_Input_Ctrl),sizeof(Inv_Can_Data->inv_set01.AC_Input_Ctrl));// 2306

#if INV_MAX_NUM == 1
        // 写入回读逻辑，立即汇总
        if ( 1 == reals.can_poll[INDEX_INV_TYPE_CONFIG01_1BH].fast_sum ) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg02200_Inv_advance_set, (uint8_t *)&Inv_Modbus_Data->mod_reg02200_Inv_advance_set, sizeof(MOD_STRUCT_reg02200));
            reals.can_poll[INDEX_INV_TYPE_CONFIG01_1BH].fast_sum = 0;
        }
#endif
    }
    else if(type == 0x1c)// 可写
    {
#if 0 // 该段暂不使用   
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02300_Inv_set02_struct, (uint8_t *)&Inv_Can_Data->inv_set02,(offsetof(inv_set02_struct,rw_cmd) - offsetof(inv_set02_struct,SetGridUV1Value)));// 2300~
#endif
    }
    else if(type == 0x1d)// 可写
    {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg02400_Inv_certification, (uint8_t *)&Inv_Can_Data->inv_set03,(offsetof(inv_set03_struct,rw_cmd) - offsetof(inv_set03_struct,SetGridEnable)));// 2400~
    }
    else if(type == 0x20)//  历史记录
    {
        /*modbus 3000*/
#ifdef MODBUS_GET_HISTORY_FROM_ARM_ENABLE
        Inv_Modbus_Data->mod_reg03000_Inv_history.total_page = Inv_Can_Data->inv_log.total_page;//3000
        Inv_Modbus_Data->mod_reg03000_Inv_history.current_page_seq = Inv_Can_Data->inv_log.current_page_seq;//3001
        for(int i = 0; i < 5; i++) // 3002~3026
        {
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].month = Inv_Can_Data->inv_log.log_data[i].month;
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].year = Inv_Can_Data->inv_log.log_data[i].year;
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].hour = Inv_Can_Data->inv_log.log_data[i].hour;
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].day = Inv_Can_Data->inv_log.log_data[i].day;
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].sec = Inv_Can_Data->inv_log.log_data[i].sec;
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].min = Inv_Can_Data->inv_log.log_data[i].min;
            
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].each.all = Inv_Can_Data->inv_log.log_data[i].each.all;
            Inv_Modbus_Data->mod_reg03000_Inv_history.log_data[i].fault_code = Inv_Can_Data->inv_log.log_data[i].fault_code;
        }
#endif
    }
}

/*------------------------------------------------------------------------------
 Function: INV_Announce_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      INV主动上报段信息处理
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void INV_Announce_Data_Can_to_Modbus(void)
{
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
    can_node_struct_Inv *Inv_Can_Data = &can_node_rd.Inv[reals.Addr_can_self];

    /*modbus 100*/
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,alarm), (uint8_t *)&Inv_Can_Data->inv_announce + offsetof(inv_announce_struct,alarm),sizeof(Inv_Can_Data->inv_announce.alarm));// 126~129
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault), (uint8_t *)&Inv_Can_Data->inv_announce + offsetof(inv_announce_struct,fault),sizeof(Inv_Can_Data->inv_announce.fault));// 133~136 故障信息
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault[4]), (uint8_t *)&Inv_Can_Data->inv_announce + offsetof(inv_announce_struct,fault5),sizeof(Inv_Can_Data->inv_announce.fault5));// 137 故障信息
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,fault[5]), (uint8_t *)&Inv_Can_Data->inv_announce + offsetof(inv_announce_struct,fault6),sizeof(Inv_Can_Data->inv_announce.fault6));// 138 故障信息
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,inv_flag), (uint8_t *)&Inv_Can_Data->inv_announce + offsetof(inv_announce_struct,status_flags),sizeof(Inv_Can_Data->inv_announce.status_flags));// 188
}

/*------------------------------------------------------------------------------
 Function: PACK_Announce_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      PACK主动上报段信息处理
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void PACK_Announce_Data_Can_to_Modbus(void)
{    
    // 只汇总主包信息
    can_node_struct_Pack *Pack_Can_Data = &can_node_rd.Pack[0];
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[reals.Addr_can_self];
    MOD_STRUCT_Pack *Pack_Modbus_Data = &top_modbus_rd.Pack[PACK_MAX_NUM];

    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,total_voltage), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,total_voltage),sizeof(Pack_Can_Data->pack_announce.total_voltage) + sizeof(Pack_Can_Data->pack_announce.total_current));// 100~101
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,soc), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,soc),sizeof(Pack_Can_Data->pack_announce.soc));// 102
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,chg_status), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,chg_status),sizeof(Pack_Can_Data->pack_announce.chg_status));// 103

    // 如果正在充电则发chg_full_time，否则发dsg_empty_time
    if(Pack_Can_Data->pack_announce.chg_status == 0x01) { 
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,chg_full_time), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,chg_full_time),sizeof(Pack_Can_Data->pack_announce.chg_full_time));// 104
    } else {
        memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,chg_full_time), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,dsg_empty_time),sizeof(Pack_Can_Data->pack_announce.dsg_empty_time));// 104
    }
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,dsg_empty_time), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,dsg_empty_time),sizeof(Pack_Can_Data->pack_announce.dsg_empty_time));// 105

    // 原子性赋值aging_status，避免位域赋值的非原子性问题
    aging_status_struct1 temp_aging_status = {0};
    temp_aging_status.progress = Pack_Can_Data->pack_announce.aging_status.progress;
    temp_aging_status.status = Pack_Can_Data->pack_announce.aging_status.status;
    temp_aging_status.fault_state = Pack_Can_Data->pack_announce.maintain_fault;
    
    // 一次性原子赋值
    Inv_Modbus_Data->mod_reg00100_AppPage1.aging_status.all = temp_aging_status.all;    // 106

    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackCnts), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,pack_cnt),sizeof(Pack_Can_Data->pack_announce.pack_cnt));// 107
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PackOnline), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,online),sizeof(Pack_Can_Data->pack_announce.online));// 108
    memcpy((uint8_t *)&Inv_Modbus_Data->mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PACK_TEST), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,PACK_TEST),sizeof(Pack_Can_Data->pack_announce.PACK_TEST));// 187
    
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,is_high_volt), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,is_high_volt),sizeof(Pack_Can_Data->pack_announce.is_high_volt));// 6000
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,pack_cnt), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,pack_cnt),sizeof(Pack_Can_Data->pack_announce.pack_cnt));// 6001
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,online), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,online),sizeof(Pack_Can_Data->pack_announce.online));// 6002
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,total_voltage), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,total_voltage),(offsetof(pack_announce_struct,soc) - offsetof(pack_announce_struct,total_voltage)));// 6003~6004
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,soc), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,soc),sizeof(Pack_Can_Data->pack_announce.soc));// 6005
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,soh), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,soh),sizeof(Pack_Can_Data->pack_announce.soh));// 6006
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,avg_temp), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,avg_temp),sizeof(Pack_Can_Data->pack_announce.avg_temp));// 6007
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,work_status), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,work_status),sizeof(Pack_Can_Data->pack_announce.work_status));// 6008
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,chg_status), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,chg_status),sizeof(Pack_Can_Data->pack_announce.chg_status));// 6009
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,max_chg_voltage), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,max_chg_voltage),(offsetof(pack_announce_struct,status1) - offsetof(pack_announce_struct,max_chg_voltage)));// 6010~6012
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,status1), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,status1),(offsetof(pack_announce_struct,soh) - offsetof(pack_announce_struct,status1)));// 6013~6014
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,cap_online), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,cap_online),sizeof(Pack_Can_Data->pack_announce.cap_online));// 6016
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,chg_full_time), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,chg_full_time),(offsetof(pack_announce_struct,is_high_volt) - offsetof(pack_announce_struct,chg_full_time)));// 6017~6018
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,TotalCurrent_bias), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,TotalCurrent_bias),sizeof(Pack_Can_Data->pack_announce.TotalCurrent_bias));// 6023
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,all_pack_alarm1), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,protect_status1),sizeof(Pack_Can_Data->pack_announce.protect_status1));// 6029
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,all_pack_alarm2), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,protect_status2),sizeof(Pack_Can_Data->pack_announce.protect_status2));// 6030
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,BCU_State), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,BCU_State),sizeof(Pack_Can_Data->pack_announce.BCU_State));// 6031
    memcpy((uint8_t *)&Pack_Modbus_Data->mod_reg06000_Pack_sum + offsetof(MOD_STRUCT_reg06000,BcuBalanceSumPw), (uint8_t *)&Pack_Can_Data->pack_announce + offsetof(pack_announce_struct,BcuBalanceSumPw),sizeof(Pack_Can_Data->pack_announce.BcuBalanceSumPw));// 6033

#if PACK_MAX_NUM == 1
    // 仅支持单个电池包时，直接使用该电池包数据
    Pack_Modbus_Data->mod_reg06000_Pack_sum.total_chg_energy = Pack_Can_Data->pack_extend.total_chg_energy / 100; // 6019-6020
    Pack_Modbus_Data->mod_reg06000_Pack_sum.total_dsg_energy = Pack_Can_Data->pack_extend.total_dsg_energy / 100; // 6021-6022 
#else
    // TODO：特殊，从逆变器获取
	Pack_Modbus_Data->mod_reg06000_Pack_sum.total_chg_energy = Inv_Modbus_Data->mod_reg00100_AppPage1.PackTotalChgEnergy;  // 6019-6020
	Pack_Modbus_Data->mod_reg06000_Pack_sum.total_dsg_energy = Inv_Modbus_Data->mod_reg00100_AppPage1.PackTotalDsgEnergy;  // 6021-6022  
#endif

#ifdef PACK_VOLTAGE_LOW_LEVEL_ENABLE
    Pack_Modbus_Data->mod_reg06000_Pack_sum.total_voltage = Pack_Modbus_Data->mod_reg06000_Pack_sum.total_voltage/10;
#endif
}

/*------------------------------------------------------------------------------
 Function: Pack_Modbus_Data_to_Sum
 -----------------------------------------------------------------------------*/
/**
  * @brief      电池信息汇总
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Pack_Modbus_Data_to_Sum(void)
{
    MOD_STRUCT_Pack *Pack_Modbus_Data = &top_modbus_rd.Pack[PACK_MAX_NUM];

    /*PACK主动上报段信息处理（仅主包上报）*/
    PACK_Announce_Data_Can_to_Modbus();

#ifdef INTERNAL_PACK_ENABLE
    /*存在主包，汇总段使用主包数据*/
    memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06100_Pack_each, (uint8_t *)&top_modbus_rd.Pack[0].mod_reg06100_Pack_each, sizeof(MOD_STRUCT_reg06100));
    memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06300_Pack_cell, (uint8_t *)&top_modbus_rd.Pack[0].mod_reg06300_Pack_cell, sizeof(MOD_STRUCT_reg06300));
    memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg07000_Pack_set, (uint8_t *)&top_modbus_rd.Pack[0].mod_reg07000_Pack_set, sizeof(MOD_STRUCT_reg07000));
    #ifdef CONFIG_FUNC_PAYGO_ENABLE
    memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg30000_Pack_paygo, (uint8_t *)&top_modbus_rd.Pack[0].mod_reg30000_Pack_paygo, sizeof(MOD_STRUCT_reg30000));
    #endif
#endif 

    /*6172 B300,B300S不支持并包升级*/
    if ( get_pack_ver_busy() ) {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1); // 6172
    } else {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 1);
    }
    
    /* 6172 判断为混包时无法升级*/
    if ( get_pack_ver_mix() ) {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff |= (1 << 2);
    } else {
        Pack_Modbus_Data->mod_reg06100_Pack_each.pack_ver_diff &= ~(1 << 2);
    }
}

/*------------------------------------------------------------------------------
 Function: Paygo_Modbus_Data_to_Sum
 -----------------------------------------------------------------------------*/
/**
  * @brief      Paygo信息汇总
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Paygo_Modbus_Data_to_Sum(void)
{
    memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg30000_Pack_paygo, (uint8_t *)&top_modbus_rd.Pack[0].mod_reg30000_Pack_paygo, sizeof(MOD_STRUCT_reg30000));
}

/* 将 src 中的项合并到目标的主区与备份区中，返回实际拷贝数量 */
static uint16_t merge_soft_versions(modbus_soft_version *dst_main, uint16_t main_cap,
                                    modbus_soft_version *dst_back, uint16_t back_cap,
                                    uint16_t dst_total, const modbus_soft_version *src, uint16_t src_total)
{
    uint16_t copied = 0;
    uint16_t dst_used_main = (dst_total >= main_cap) ? main_cap : dst_total;
    uint16_t dst_used_back = (dst_total > main_cap) ? (dst_total - main_cap) : 0;
    uint16_t space_main = (dst_used_main < main_cap) ? (main_cap - dst_used_main) : 0;
    uint16_t space_back = (dst_used_back < back_cap) ? (back_cap - dst_used_back) : 0;
    uint16_t src_left = src_total;

    /* 先填充主区剩余 */
    uint16_t first = (src_left > space_main) ? space_main : src_left;
    if (first > 0) {
        memcpy(&dst_main[dst_used_main], src, first * sizeof(modbus_soft_version));
        copied += first;
        src += first;
        src_left -= first;
    }

    /* 再填充备份区 */
    uint16_t second = (src_left > space_back) ? space_back : src_left;
    if (second > 0) {
        memcpy(&dst_back[dst_used_back], src, second * sizeof(modbus_soft_version));
        copied += second;
    }

    return copied;
}

/*------------------------------------------------------------------------------
 Function: Inv_Modbus_Data_to_Sum
 -----------------------------------------------------------------------------*/
/**
  * @brief      逆变信息汇总
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void Inv_Modbus_Data_to_Sum(void)
{
    MOD_STRUCT_Inv *Inv_Modbus_Data = &top_modbus_rd.Inv[INV_MAX_NUM];

    memcpy(Inv_Modbus_Data, &top_modbus_rd.Inv[reals.Addr_can_self], offsetof(MOD_STRUCT_Inv, mod_reg03000_Inv_history)); // 复制3000段之前的部分
    memcpy((uint8_t *)Inv_Modbus_Data + offsetof(MOD_STRUCT_Inv, mod_reg11000_IOT_info), 
           (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self] + offsetof(MOD_STRUCT_Inv, mod_reg11000_IOT_info), 
           sizeof(MOD_STRUCT_Inv) - offsetof(MOD_STRUCT_Inv, mod_reg11000_IOT_info)); // 复制11000段之后的部分

#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5
    /*IOT->2073/2074 休眠设置*/
    Inv_Modbus_Data->mod_reg02000_Inv_base_set.Remote_set.all = IotSetData.dev_info_t.Remote_set.all;
    Inv_Modbus_Data->mod_reg02000_Inv_base_set.Remote_set_soc_limit = IotSetData.dev_info_t.Remote_set_soc_limit;
#endif

#ifdef CONFIG_IOT_LED_SET_ENABLE
    /*IOT->2078 LED设置*/
    Inv_Modbus_Data->mod_reg02000_Inv_base_set.LED_SET.all = IotSetData.dev_info_t.LED_SET.all;
#endif 

    /*2274->175 房车使用场景*/
    Inv_Modbus_Data->mod_reg00100_AppPage1.SceneConfigFlag.map_set = Inv_Modbus_Data->mod_reg02200_Inv_advance_set.RvSettings.map_set;

    /*2073->171 是否支持远程关机*/
    Inv_Modbus_Data->mod_reg00100_AppPage1.Parts_online.bit.remote_switch = Inv_Modbus_Data->mod_reg02000_Inv_base_set.Remote_set.Remote_set_valid;

    uint16_t inv_software_total = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base.software_total;
    uint16_t actually_copied = 0;
    
#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    /*171 DCHUB在线状态*/
    Inv_Modbus_Data->mod_reg00100_AppPage1.Parts_online.bit.DC_HUB = (reals.online_DCHUB_num > 0) ? 1 : 0;

    /* 合并ARM和DCHUB软件版本号 */
    if (can_node_rd.DCHUB[DC_HUB_MAX_NUM].dc_hub_info.SoftVersion != 0) {
        modbus_soft_version dst = {0};
        dst.type = DEVICE_DC_HUB;
        dst.version = can_node_rd.DCHUB[DC_HUB_MAX_NUM].dc_hub_info.SoftVersion;
        actually_copied = merge_soft_versions(
            Inv_Modbus_Data->mod_reg01100_Inv_base.soft,    MD_SOFT_MAIN_CAP,
            Inv_Modbus_Data->mod_reg01100_Inv_base.soft_back, MD_SOFT_BACK_CAP,
            inv_software_total,
            &dst, 1
        );
        inv_software_total += actually_copied;
        if (inv_software_total > MD_SOFT_MAX_CAP) inv_software_total = MD_SOFT_MAX_CAP;
        Inv_Modbus_Data->mod_reg01100_Inv_base.software_total = inv_software_total;
    }
#endif 

#ifdef INTERNAL_PACK_ENABLE
    /* 合并ARM和BMS[0](即机器内部电池包)软件版本号（放到最后） */
    uint16_t pack_software_total = top_modbus_rd.Pack[0].mod_reg06100_Pack_each.software_total;
    if ( pack_software_total > 0 ) {
        actually_copied = merge_soft_versions(
            Inv_Modbus_Data->mod_reg01100_Inv_base.soft,    MD_SOFT_MAIN_CAP,
            Inv_Modbus_Data->mod_reg01100_Inv_base.soft_back, MD_SOFT_BACK_CAP,
            inv_software_total,
            top_modbus_rd.Pack[0].mod_reg06100_Pack_each.soft, pack_software_total
        );
        inv_software_total += actually_copied;
        if (inv_software_total > MD_SOFT_MAX_CAP) inv_software_total = MD_SOFT_MAX_CAP;
        Inv_Modbus_Data->mod_reg01100_Inv_base.software_total = inv_software_total;
    }
#endif  

#if 0   // 调试数据
//    Inv_Modbus_Data->mod_reg01100_Inv_base.software_total = MD_SOFT_MAX_CAP;
//    for ( uint8_t i = 0 ; i < MD_SOFT_MAIN_CAP ; i++ )
//    {
//        Inv_Modbus_Data->mod_reg01100_Inv_base.soft[i].type = i + 1;
//        Inv_Modbus_Data->mod_reg01100_Inv_base.soft[i].version = 100480101 + i * 10000;
//    }
//    for ( uint8_t i = 0 ; i < MD_SOFT_BACK_CAP ; i++ )
//    {
//        Inv_Modbus_Data->mod_reg01100_Inv_base.soft_back[i].type = i + 1 + MD_SOFT_MAIN_CAP;
//        Inv_Modbus_Data->mod_reg01100_Inv_base.soft_back[i].version = 100480101 + (i + MD_SOFT_MAIN_CAP) * 10000;
//    }
    Inv_Modbus_Data->mod_reg01100_Inv_base.software_total = 1;
    Inv_Modbus_Data->mod_reg01100_Inv_base.soft[0].type = 1;
    Inv_Modbus_Data->mod_reg01100_Inv_base.soft[0].version = 100630100;
//    Inv_Modbus_Data->mod_reg01100_Inv_base.soft[1].type = 2;
//    Inv_Modbus_Data->mod_reg01100_Inv_base.soft[1].version = 100490101;
//    Inv_Modbus_Data->mod_reg02000_Inv_base_set.work_mode = 1;
//    Inv_Modbus_Data->mod_reg02000_Inv_base_set.ctrl_time_area = 1;
#endif
}

/*------------------------------------------------------------------------------
 Function: DcHub_Announce_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      DCHUB数据汇总
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void DcHub_Announce_Data_Can_to_Modbus(void)
{
    for ( uint8_t index = 0 ; index < DC_HUB_MAX_NUM ; index++ )
    {
        if ( can_node_rd.DCHUB[index].online ) {
            // 15700~15709
            memcpy(top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.dc_hub_type, SN_TYPE_DCHUB_ASCII, sizeof(SN_TYPE_DCHUB_ASCII));
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.dc_hub_sn = can_node_rd.DCHUB[index].dc_hub_info.DCHubSN;
            // 15710~15712 wry:实际上都没有，默认0
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.input_power = 0;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.input_voltage = 0;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.input_current = 0;
            // 15713~15715
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.output_power = can_node_rd.DCHUB[index].dc_hub_info.DCLoadTotalPower;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.output_voltage = can_node_rd.DCHUB[index].dc_hub_info.DCLoadTotalVolt;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.output_current = can_node_rd.DCHUB[index].dc_hub_info.DCLoadTotalCurr;
            // 15716~15718
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.cig1_output_power = can_node_rd.DCHUB[index].dc_hub_info.Cigar1Power;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.cig1_output_voltage = can_node_rd.DCHUB[index].dc_hub_info.Cigar1Volt;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.cig1_output_current = can_node_rd.DCHUB[index].dc_hub_info.Cigar1Curr;
            // 15719~15721
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.cig2_output_power = can_node_rd.DCHUB[index].dc_hub_info.Cigar2Power;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.cig2_output_voltage = can_node_rd.DCHUB[index].dc_hub_info.Cigar2Volt;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.cig2_output_current = can_node_rd.DCHUB[index].dc_hub_info.Cigar2Curr;
            // 15722~15724
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.USB_A_output_power = can_node_rd.DCHUB[index].dc_hub_info.USBAPower;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.USB_A_output_voltage = can_node_rd.DCHUB[index].dc_hub_info.USBAVolt;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.USB_A_output_current = can_node_rd.DCHUB[index].dc_hub_info.USBACurr;
            // 15725~15727
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Type_C_output_power = can_node_rd.DCHUB[index].dc_hub_info.PD1Power;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Type_C_output_voltage = can_node_rd.DCHUB[index].dc_hub_info.PD1Volt;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Type_C_output_current = can_node_rd.DCHUB[index].dc_hub_info.PD1Curr;
            // 15728~15730
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Type_C2_output_power = can_node_rd.DCHUB[index].dc_hub_info.PD2Power;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Type_C2_output_voltage = can_node_rd.DCHUB[index].dc_hub_info.PD2Volt;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Type_C2_output_current = can_node_rd.DCHUB[index].dc_hub_info.PD2Curr;
            // 15731~15733
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Anderson_output_power = can_node_rd.DCHUB[index].dc_hub_info.RVPower;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Anderson_output_voltage = can_node_rd.DCHUB[index].dc_hub_info.RVVolt;
            top_modbus_rd.DcHub[index].mod_reg15700_Dc_Hub_info.Anderson_output_current = can_node_rd.DCHUB[index].dc_hub_info.RVCurr;

            // 15750
            top_modbus_rd.DcHub[index].mod_reg15750_Dc_Ac_Hub_set.dc_hug_setting.bit.dc_switch = can_node_rd.DCHUB[index].dc_hub_info.event_bits.DCOnOffState;
            if ( can_node_rd.DCHUB[index].dc_hub_info.event_bits.DC24VState ) {
                top_modbus_rd.DcHub[index].mod_reg15750_Dc_Ac_Hub_set.dc_hug_setting.bit.dc_set_voltage = 2;
            } else {
                top_modbus_rd.DcHub[index].mod_reg15750_Dc_Ac_Hub_set.dc_hug_setting.bit.dc_set_voltage = 1;
            }
                
            // 1100
            memcpy(top_modbus_rd.DcHub[index].mod_reg01100_Inv_base.InvType, SN_TYPE_DCHUB_ASCII, sizeof(SN_TYPE_DCHUB_ASCII));
            top_modbus_rd.DcHub[index].mod_reg01100_Inv_base.InvSN = can_node_rd.DCHUB[index].dc_hub_info.DCHubSN;
            top_modbus_rd.DcHub[index].mod_reg01100_Inv_base.software_total = 1;
            top_modbus_rd.DcHub[index].mod_reg01100_Inv_base.soft[0].type = DEVICE_DC_HUB;
            top_modbus_rd.DcHub[index].mod_reg01100_Inv_base.soft[0].version = can_node_rd.DCHUB[index].dc_hub_info.SoftVersion;
            
            // 100 (DCHUB告警由逆变器上报处理，此处仅做标志和读取，不做本地存储)
            memcpy(top_modbus_rd.DcHub[index].mod_reg00100_AppPage1.dev_type, SN_TYPE_DCHUB_ASCII, sizeof(SN_TYPE_DCHUB_ASCII));
            top_modbus_rd.DcHub[index].mod_reg00100_AppPage1.dev_sn = can_node_rd.DCHUB[index].dc_hub_info.DCHubSN;
            top_modbus_rd.DcHub[index].mod_reg00100_AppPage1.fault[4] = can_node_rd.DCHUB[index].dc_hub_info.status & 0x1F;
            top_modbus_rd.DcHub[index].mod_reg00100_AppPage1.fault[1] = can_node_rd.DCHUB[index].dc_hub_info.status_bits.DCOverLoad << 10;
        }
    }

    // 当前仅支持一个DCHUB
    memcpy(&can_node_rd.DCHUB[DC_HUB_MAX_NUM], &can_node_rd.DCHUB[0], sizeof(dc_hub_can_node_struct));
    memcpy(&top_modbus_rd.DcHub[DC_HUB_MAX_NUM], &top_modbus_rd.DcHub[0], sizeof(MOD_STRUCT_DCHUB));
}

/*------------------------------------------------------------------------------
 Function: DcDc_Announce_Data_Can_to_Modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      DCDC数据汇总
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void DcDc_Announce_Data_Can_to_Modbus(void)
{
    for ( uint8_t index = 0 ; index < DCDC_MAX_NUM ; index++ )
    {
        if ( can_node_rd.Dcdc[index].online ) {
            MOD_STRUCT_Dcdc *DCDC_Modbus_Data = &top_modbus_rd.Dcdc[index];
            can_node_struct_DCDC *DCDC_Can_Data = &can_node_rd.Dcdc[index];

            // 11000
            memcpy(DCDC_Modbus_Data->mod_reg11000_IOT_info.iot_type, DCDC_Can_Data->iot_info.iot_type, sizeof(TYPE_SIZE));
            DCDC_Modbus_Data->mod_reg11000_IOT_info.iot_sn = DCDC_Can_Data->iot_info.iot_sn;
            DCDC_Modbus_Data->mod_reg11000_IOT_info.software_ver = DCDC_Can_Data->iot_info.software_ver;

            // 15500
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dev_type), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dev_type), (offsetof(dcdc_common_info_struct,battery_type) - offsetof(dcdc_common_info_struct,dev_type)));//15500~15509
            DCDC_Modbus_Data->mod_reg15500_DCDC_info.dc1_input_voltage = DCDC_Can_Data->dcdc_common_info.dc_info[0].dc_voltage; // 15510
            DCDC_Modbus_Data->mod_reg15500_DCDC_info.dc3_output_voltage = DCDC_Can_Data->dcdc_common_info.dc_info[2].dc_voltage; // 15511
            DCDC_Modbus_Data->mod_reg15500_DCDC_info.dc3_output_current = DCDC_Can_Data->dcdc_common_info.dc_info[2].dc_current; // 15512
            DCDC_Modbus_Data->mod_reg15500_DCDC_info.dc3_output_power = DCDC_Can_Data->dcdc_common_info.dc_info[2].dc_power; // 15513
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,energy_line), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, energy_line), sizeof(DCDC_Can_Data->dcdc_common_info.energy_line));//15514
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,battery_type), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, battery_type), sizeof(DCDC_Can_Data->dcdc_common_info.battery_type));//15515
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,fault_dc1), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, fault_dc1), (offsetof(dcdc_common_info_struct,ctrl_mode) - offsetof(dcdc_common_info_struct,fault_dc1)));//15516~15525
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,ctrl_mode), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, ctrl_mode), sizeof(DCDC_Can_Data->dcdc_common_info.ctrl_mode));//15526
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,total_input_power), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, total_input_power), sizeof(DCDC_Can_Data->dcdc_common_info.total_input_power));//15527~15528
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,total_output_power), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, total_output_power), sizeof(DCDC_Can_Data->dcdc_common_info.total_output_power));//15529~15530
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dc_info), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dc_info), sizeof(DCDC_Can_Data->dcdc_common_info.dc_info));//15531~15554
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,TotalInputEnergy), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, TotalInputEnergy), sizeof(DCDC_Can_Data->dcdc_common_info.TotalInputEnergy));//15555~15556
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,energy_info), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, energy_info), sizeof(DCDC_Can_Data->dcdc_common_info.energy_info));//15557~15580
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dcdc_SoftwareType), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dcdc_SoftwareType), sizeof(DCDC_Can_Data->dcdc_common_info.dcdc_SoftwareType));//15581
            memcpy((uint8_t *)&DCDC_Modbus_Data->mod_reg15500_DCDC_info + offsetof(MOD_STRUCT_reg15500,dcdc_SoftwareVersion), (uint8_t *)&DCDC_Can_Data->dcdc_common_info + offsetof(dcdc_common_info_struct, dcdc_SoftwareVersion), sizeof(DCDC_Can_Data->dcdc_common_info.dcdc_SoftwareVersion));//15582~15583

            // 15600
            DCDC_Modbus_Data->mod_reg15600_DCDC_set.charger_set.bit.dc_out_onoff = DCDC_Can_Data->dcdc_common_set.charger_set.bit.dc_out_onoff; // 15600
            // 15601~15612
            for ( uint8_t i = 0 ; i < 6 ; i++ ) {
                DCDC_Modbus_Data->mod_reg15600_DCDC_set.dc_val_set[i].dc_voltage_set = DCDC_Can_Data->dcdc_common_set.dc_val_set[i].dc_voltage_set;
                DCDC_Modbus_Data->mod_reg15600_DCDC_set.dc_val_set[i].dc_current_set = DCDC_Can_Data->dcdc_common_set.dc_val_set[i].dc_current_set;
            }
            DCDC_Modbus_Data->mod_reg15600_DCDC_set.mode4_set.bit.long_short_line = DCDC_Can_Data->dcdc_common_set.mode4_set.bit.long_short_line;   // 15625
        }
    }

    // 当前仅支持一个DCDC
    memcpy(&can_node_rd.Dcdc[DCDC_MAX_NUM], &can_node_rd.Dcdc[0], sizeof(can_node_struct_DCDC));
    memcpy(&top_modbus_rd.Dcdc[DCDC_MAX_NUM], &top_modbus_rd.Dcdc[0], sizeof(MOD_STRUCT_Dcdc));
}

// 汇总can数据
void sum_can_node_rd_data(void){
    // 不考虑并机，直接复制
    memcpy(&can_node_rd.Pack[PACK_MAX_NUM], &can_node_rd.Pack[0], sizeof(can_node_struct_Pack));
    memcpy(&can_node_rd.Inv[INV_MAX_NUM], &can_node_rd.Inv[reals.Addr_can_self], sizeof(can_node_struct_Inv));
}

/*------------------------------------------------------------------------------
 Function: Sum_CAN_To_Modbus_Read_Info_Process
 -----------------------------------------------------------------------------*/
/**
  * @brief      此函数用于把从ARM读取的CAN数据赋值给将要发给app、蓝牙上位机的modbus表，顺序以CAN结构体向下排列
                Inv_can ->Inv (can->modbus)
  * @param[in]  uint8_t type  
  * @param[out] None
  * @return     void
  */
void Sum_CAN_To_Modbus_Read_Info_Process(void)
{	
    // 汇总can_node_rd数据
    sum_can_node_rd_data();
    
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE

    /*电池包数据汇总*/
    update_bat_data_to_modbus();

#endif

#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE  

    /*电池包数据汇总*/
    Pack_Modbus_Data_to_Sum();

#endif

#if 0  // 配件暂不汇总，直接通过对应从机地址获取数据

    /*配件数据汇总*/
    Parts_Modbus_Data_to_Sum();

#endif

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE

    /*DCHUB数据处理*/
    DcHub_Announce_Data_Can_to_Modbus();

#endif

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE

    /*DCDC数据处理*/
    DcDc_Announce_Data_Can_to_Modbus();

#endif

#ifdef CONFIG_FUNC_PAYGO_ENABLE

    /*Paygo数据汇总*/
    Paygo_Modbus_Data_to_Sum();

#endif

    /*INV主动上报段信息处理*/
    INV_Announce_Data_Can_to_Modbus();

    /*逆变信息汇总*/
    Inv_Modbus_Data_to_Sum();
}

#endif
