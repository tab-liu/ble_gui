#include "modbus_protocol.h"
#include "esp_check.h"
#include "esp_log.h"
#include <string.h>

#include "inv_define.h"
// #include "uart_device_process.h"
#include "comm_define.h"
#include "filesystem.h"
#include "modbus_data.h"
#include "parameter.h"
#include "tou_relay_ctrl.h"
#include "iot_wifi.h"
#include "dev_data_record.h"
#include "iot_mqtt.h"
#include "can_protocol.h"
#include "reg_change_log.h"
#include "smart_tou_ctrl_v1.h"
#include "ac_ems_cloud.h"
#include "log_redirect.h"
#include "modbus_tlv.h"
#include "dev_modbus_s1_data_handler.h"
#include "dev_modbus_manage.h"
#include "cloud_sync.h"

#define TAG "[md_protocol]"

static SemaphoreHandle_t modbus_data_semaphore = NULL; //数据空闲信号量


/*------------------------------------------------------------------------------
 Function: modbus_data_semaphore_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      信号量初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void modbus_data_semaphore_init(void)
{
	modbus_data_semaphore = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(modbus_data_semaphore == NULL)
	{
        ESP_LOGE(TAG, "modbus_data_semaphore create failed");
		return;
	}
	xSemaphoreGive(modbus_data_semaphore);						/**< the resource is available after creation */
}

/*------------------------------------------------------------------------------
 Function: modbus_data_semaphore_Take
 -----------------------------------------------------------------------------*/
/**
 * @brief      获取信号量
 * @param[in]  void  
 * @param[out] None
 * @return     uint8_t
 */
uint8_t modbus_data_semaphore_Take(void)
{
    if (!modbus_data_semaphore || xSemaphoreTake(modbus_data_semaphore, pdMS_TO_TICKS(10)) != pdTRUE) // 不能延时
    {
        ESP_LOGE(TAG,"get modbus_data_semaphore failed");
        return 0; 
    }

    return 1;
}

/*------------------------------------------------------------------------------
 Function: modbus_data_semaphore_Give
 -----------------------------------------------------------------------------*/
/**
  * @brief      释放信号量
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void modbus_data_semaphore_Give(void)
{
    xSemaphoreGive(modbus_data_semaphore);  /*释放信号量*/
}

#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE

/*------------------------------------------------------------------------------
 Function: Modbus_Reg_Change_log_set
 -----------------------------------------------------------------------------*/
/**
  * @brief      寄存器修改日志生成
  * @param[in]  tbl_cb_data_t *cb_data  
                uint8_t *rd_data        
                uint8_t *wr_data        
  * @param[out] None
  * @return     static void
  */
static void Modbus_Reg_Change_log_set(tbl_cb_data_t *cb_data, uint8_t *rd_data, uint8_t *wr_data)
{
    // 1. 匹配修改记录来源
    uint8_t chl = DATA_SOURCE_UNKNOWN;
    switch ( cb_data->cb_chl )
    {
        case MD_CHL_BLE :
            chl = DATA_SOURCE_APP_BLE_SERVER;
            break;
        case MD_CHL_WIFI_CLOUD :
            chl = DATA_SOURCE_WIFI_MQTT;
            break;
        case MD_CHL_UART_DOWN :
            chl = DATA_SOURCE_UART;
            break;
        case MD_CHL_CAN :
            chl = DATA_SOURCE_CAN;
            break;
        case MD_CHL_SELF :
            chl = DATA_SOURCE_IOT_INTERNAL;
            break;
        case MD_CHL_WIFI_WLCC :
            chl = DATA_SOURCE_WIFI_UDP;
            break;
        case MD_CHL_TCP :
            chl = DATA_SOURCE_MODBUS_TCP;
            break;        
        case MD_CHL_CLOUD_SYNC:
            chl = DATA_SOURCE_CLOUD_SYNC;
            break;
        default:
            break;
    }
    
    // 2. 声明并填充参数结构体
    modbus_change_params_t log_params = {
        .slave_address       = cb_data->SlaveAddress,
        .start_address       = cb_data->reg_addr,
        .reg_count           = cb_data->reg_nums,
        .old_data            = (uint16_t *)(rd_data + (cb_data->reg_addr_offset)),
        .new_data            = (uint16_t *)(wr_data + (cb_data->reg_addr_offset)),
        .protocol_version    = PROTOCOL_MODBUS_BETA,
        .modification_source = chl,
    };
    
    // 3. 调用新函数，只传递一个参数
    if (Reg_Change_Log_Generate_By_Modbus(&log_params) != 0) {
        // 处理错误
    }
}

#endif

static uint32_t suppor_control_get_value(uint16_t cmd)
{
    for (int i = 0; i < sizeof(reals.supper_control.cmd_list) / sizeof(reals.supper_control.cmd_list[0]); i++)
    {
        // ESP_LOGI(TAG, "check supper control cmd list[%d]:%d", i, reals.supper_control.cmd_list[i].cmd);
        if (reals.supper_control.cmd_list[i].cmd == cmd)
        {
            ESP_LOGI(TAG, "get supper control cmd:%d, value:%lu", cmd, reals.supper_control.cmd_list[i].value);
            return reals.supper_control.cmd_list[i].value;
        }
    }

    return 0; // 或者其他默认值，表示未找到
}

static uint32_t suppor_control_set_value(uint16_t cmd, uint32_t value)
{
    for (int i = 0; i < sizeof(reals.supper_control.cmd_list) / sizeof(reals.supper_control.cmd_list[0]); i++)
    {
        if (reals.supper_control.cmd_list[i].cmd == cmd)
        {
            reals.supper_control.cmd_list[i].value = value;
            ESP_LOGW(TAG, "set supper control cmd:%d, value:%lu", cmd, value);
            return 1; // 设置成功
        }
    }
    
    ESP_LOGE(TAG, "Unknown set supper control cmd:%d, value:%lu", cmd, value);
    return 0; // 或者其他默认值，表示未找到
}

/**
 * @brief 设备数据表xx处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1
 定义回调函数包含同一寄存器的所有可能操作，如 uart micro inv和CAN big inv集合分支处理
 */
int Modbus_callback_00000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg00000 + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg00000 + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_00000 run,write  !");
        
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000, (uint8_t *)&top_modbus_wr.Inv.mod_reg00000);
#endif

        memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000 + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg00000 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);

		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg00000.app_password)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg00000.app_password))
		{
			reals.ModbusCmdFlag.sBit.app_password = 1;
		}

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg00000.support_mode)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg00000.support_mode))
        {
            reals.ModbusCmdFlag.sBit.support_mode = 1;
        }
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_00000 run,read !");
	}
	return 0;
}

int Modbus_callback_00100(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_100 run,write!");
      	// memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1 + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg00100_AppPage1 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_100 run,read!");
	}
	return 0;
}
int Modbus_callback_00700(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg00700_OTA + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg00700_OTA + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

	if(true == cb_data->is_write)
	{
		memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg00700_OTA + (cb_data->reg_addr_offset), cb_data->reg_nums*2);

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg00700_OTA.ota_mcu_count)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg00700_OTA.ota_mcu_count))
        {
            // 仅接受非零值写入
            if ( 0 != top_modbus_wr.Inv.mod_reg00700_OTA.ota_mcu_count )
            {
                reals.system_ota.ota_mcu_total_count = top_modbus_wr.Inv.mod_reg00700_OTA.ota_mcu_count;
                reals.system_ota.ota_mcu_curr_count = 0;
                ESP_LOGW(TAG, "APP OTA MCU Total count : %d", reals.system_ota.ota_mcu_total_count);
            }
        }

		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg00700_OTA.ota_cmd.begin)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg00700_OTA.ota_cmd.begin))
		{
            if ( 0 != top_modbus_wr.Inv.mod_reg00700_OTA.ota_cmd.begin ) {
                /*初始化*/
                memset(&can_ota_status,0,sizeof(CanOtaStruct));
                memset(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA, 0, sizeof(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA)); // 清除升级状态和升级阶段
                memcpy((uint8_t *)&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00700_OTA + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg00700_OTA + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
                ESP_LOGW(TAG, "APP Start OTA (%d)! ", top_modbus_wr.Inv.mod_reg00700_OTA.ota_cmd.begin);
            } else {
                can_ota_status.ready_timeout = 1;  // APP控制立即停止
                ESP_LOGW(TAG, "APP Stop OTA! ");
            }
		}
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_0700 run,read !");
	}

	return 0;
}
int Modbus_callback_01100(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1100 run,write  !"); // 不允许写入
		// memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base + (cb_data->reg_addr_offset),(uint8_t *)&top_modbus_wr.Inv.mod_reg01100_Inv_base + (cb_data->reg_addr_offset), cb_data->reg_nums*2);	
    }
	else
	{
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取1100段*/
            if(INV_TYPE_ABOUT_13H == reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;  
                    reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].can_type);// 
            }
        }
#endif
		ESP_LOGI(TAG, "callback Modbus_callback_1100 run,read !");
	}
	return 0;
}

int Modbus_callback_01200(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1200 run,write  !");
    }
	else
	{
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取1200段*/
            if(INV_TYPE_PV_14H == reals.can_poll[INDEX_INV_TYPE_PV_14H].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_PV_14H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_PV_14H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS; 
                    reals.can_poll[INDEX_INV_TYPE_PV_14H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_PV_14H].can_type);// 
            }
        }
#endif
		ESP_LOGI(TAG, "callback Modbus_callback_1200 run,read !");
	}
	return 0;
}

int Modbus_callback_01300(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1300 run,write  !");
    }
	else
	{
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取1300段*/
            if(INV_TYPE_GRID_15H == reals.can_poll[INDEX_INV_TYPE_GRID_15H].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_GRID_15H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_GRID_15H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;  
                    reals.can_poll[INDEX_INV_TYPE_GRID_15H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_GRID_15H].can_type);// 
            }  
        }  
#endif   	
		ESP_LOGI(TAG, "callback Modbus_callback_1300 run,read !");
	}
	return 0;

}

int Modbus_callback_01400(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1400 run,write  !");
    }
	else
	{
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取1400段*/
            if(INV_TYPE_LOAD_16H == reals.can_poll[INDEX_INV_TYPE_LOAD_16H].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_LOAD_16H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_LOAD_16H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;  
                    reals.can_poll[INDEX_INV_TYPE_LOAD_16H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_LOAD_16H].can_type);// 
            } 
        }  
#endif 	
		ESP_LOGI(TAG, "callback Modbus_callback_1400 run,read !");
	}
	return 0;
}
int Modbus_callback_01500(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1500 run,write  !");
    }
	else
	{
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取1500段*/
            if(INV_TYPE_DATA_17H == reals.can_poll[INDEX_INV_TYPE_DATA_17H].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_DATA_17H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_DATA_17H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;  
                    reals.can_poll[INDEX_INV_TYPE_DATA_17H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_DATA_17H].can_type);// 
            } 
        }    
#endif
		ESP_LOGI(TAG, "callback Modbus_callback_1500 run,read !");
	}
	return 0;
}
int Modbus_callback_01600(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1600 run,write  !");
    }
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1600 run,read !");
	}
	return 0;
}

int Modbus_callback_01700(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_01700 run,write  !");
    }
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_01700 run,read !");
	}
	return 0;
}

int Modbus_callback_02000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset) + cb_data->reg_nums*2;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_2000 run,write!");

#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成

        // 检查是否为需要忽略的授时操作 (写入地址在 2001-2004 范围内)
        bool is_sntp_time_setting = (1 == reals.rtc_flag.sBit.RTC_valid_from_SERVER) 
                                    && (2 != IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update)
                                    && (cb_data->reg_addr >= 2001) 
                                    && ((cb_data->reg_addr + cb_data->reg_nums) <= 2005);
        
        // 检查是否为需要忽略的地址
        bool is_addr_ignore = (cb_data->reg_addr == 2026)
                            || (cb_data->reg_addr == 2027) 
                            || (cb_data->reg_addr == 2028);

        // 只有当它不是以上任何一种需要忽略的情况时，才记录日志
        if (!is_sntp_time_setting && !is_addr_ignore) {
            Modbus_Reg_Change_log_set(cb_data, 
                                      (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set, 
                                      (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set);
        }
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
        // 更新参数更新记录
        if (( MD_CHL_CLOUD_SYNC != cb_data->cb_chl ) 
            && ( MD_CHL_WIFI_WLCC != cb_data->cb_chl )) {
            cloud_sync_queue_push(cb_data->SlaveAddress, cb_data->reg_addr, cb_data->reg_nums);    
        }
#endif

#ifdef INV_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE
        #if INV_MAX_NUM > 1
        if ( cb_data->SlaveAddress == 0 ) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        } else if(cb_data->SlaveAddress >= MD_INV_ADDR_START && cb_data->SlaveAddress < (MD_INV_ADDR_START + INV_MAX_NUM)) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[(cb_data->SlaveAddress - MD_INV_ADDR_START)].mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        }
        #else
        memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        #endif
#endif

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.mon)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.min))       
        {
			reals.rtc_flag.sBit.RTC_set_APP_to_IOT = 1;
        }   

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.SetTimeZone)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.SetTimeZone))       
        {
            /*仅服务器时间无效时*/
            if ( 0 == reals.rtc_flag.sBit.RTC_valid_from_SERVER )
            {
                //判断是否为无效写入
                if(IotSetData.dev_info_t.SetTimeZone.all != top_modbus_wr.Inv.mod_reg02000_Inv_base_set.SetTimeZone.all)
                {
                    IotSetData.dev_info_t.SetTimeZone.all = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.SetTimeZone.all;
                    reals.SetDataWrFlag.sBit.SetTimeZone = 1;
                } 
            }
        }     

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl))          
        {
            reals.ModbusCmdFlag.sBit.ctrl_2006 = 1;
        }
            
        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_poweron)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_poweron))          
        {
            reals.ModbusCmdFlag.sBit.ctrl_power = 1;
        }       

        if(
            ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set) && (address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set)) 
            || ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set_soc_limit) && (address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.Remote_set_soc_limit)) 
          )          
        {
            reals.ModbusCmdFlag.sBit.ctrl_remote = 1;
        } 
          
        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.LED_SET))          
        {
            reals.ModbusCmdFlag.sBit.ctrl_led = 1;
        }           
	}
	else
	{
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取2000段*/
            if(INV_TYPE_CONFIG00_1AH == reals.can_poll[INDEX_INV_TYPE_CONFIG00_1AH].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_CONFIG00_1AH].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_CONFIG00_1AH].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;
                    reals.can_poll[INDEX_INV_TYPE_CONFIG00_1AH].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_CONFIG00_1AH].can_type);// 
            }   
        }    
#endif
		ESP_LOGI(TAG, "callback Modbus_callback_2000 run,read !");
	}

	return 0;
}

int Modbus_callback_02200(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset) + cb_data->reg_nums*2;

    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2200 run,write  !");
        
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set, (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set);
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
        // 更新参数更新记录
        if (( MD_CHL_CLOUD_SYNC != cb_data->cb_chl ) 
            && ( MD_CHL_WIFI_WLCC != cb_data->cb_chl )) {
            cloud_sync_queue_push(cb_data->SlaveAddress, cb_data->reg_addr, cb_data->reg_nums);    
        }
#endif

#ifdef INV_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE
        #if INV_MAX_NUM > 1
        if ( cb_data->SlaveAddress == 0 ) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        } else if(cb_data->SlaveAddress >= MD_INV_ADDR_START && cb_data->SlaveAddress < (MD_INV_ADDR_START + INV_MAX_NUM)) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[(cb_data->SlaveAddress - MD_INV_ADDR_START)].mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        }
        #else
        memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        #endif
#endif

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.ctrl_reset_factory)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.ctrl_reset_factory))       
        {
            if (top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.ctrl_reset_factory != 0) {
                reals.ModbusCmdFlag.sBit.reset_factory = 1;
            }
        }   

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.bat_maintain)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.bat_maintain))       
        {
            if (top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.bat_maintain.all != 0) {
                reals.ModbusCmdFlag.sBit.bat_maintain_set = 1;

                // 清除电池老化结果状态
                if ( 1 == top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.bat_maintain.reset ) {
                    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.aging_status.all = 0;
                }
            }
        }  

#ifdef CONFIG_SMART_GENERATOR_ENABLE
        if (
            ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.GenSet)   ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Start && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Start) ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Stop && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.Gen_SOC_Stop))
           )
        {
            reals.ModbusCmdFlag.sBit.gen_set = 1;
        }
#endif           

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.AC_Input_Ctrl)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.AC_Input_Ctrl))       
        {
            // 开始自检前清除上次自检状态
            if (1 == top_modbus_wr.Inv.mod_reg02200_Inv_advance_set.AC_Input_Ctrl.Self_Check_Cmd) {
                top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.AC_Input_Status.all = 0;
            }
        }  
    }
    else
    {
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取2200段*/
            if(INV_TYPE_CONFIG01_1BH == reals.can_poll[INDEX_INV_TYPE_CONFIG01_1BH].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_CONFIG01_1BH].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_CONFIG01_1BH].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;
                    reals.can_poll[INDEX_INV_TYPE_CONFIG01_1BH].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_CONFIG01_1BH].can_type);// 
            }  
        }       
#endif
        ESP_LOGI(TAG, "callback Modbus_callback_2200 run,read !");
    }
    return 0;
}

int Modbus_callback_02300(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_02300 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_02300 run,read !");
    }
    return 0;
}

int Modbus_callback_02400(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2400 run,write  !");
#ifdef INV_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE
        #if INV_MAX_NUM > 1
        if ( cb_data->SlaveAddress == 0 ) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), cb_data->reg_nums*2);     
        } else if(cb_data->SlaveAddress >= MD_INV_ADDR_START && cb_data->SlaveAddress < (MD_INV_ADDR_START + INV_MAX_NUM)) {
            memcpy((uint8_t *)&top_modbus_rd.Inv[(cb_data->SlaveAddress - 1)].mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), cb_data->reg_nums*2);     
        }
        #else
        memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), cb_data->reg_nums*2);     
        #endif
#endif        
    }
    else
    {
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取2400段*/
            if(INV_TYPE_CONFIG03_1DH == reals.can_poll[INDEX_INV_TYPE_CONFIG03_1DH].can_type) {
                if(0 == reals.can_poll[INDEX_INV_TYPE_CONFIG03_1DH].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_INV_TYPE_CONFIG03_1DH].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;
                    reals.can_poll[INDEX_INV_TYPE_CONFIG03_1DH].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_CONFIG03_1DH].can_type);// 
            }
        }     
#endif
        ESP_LOGI(TAG, "callback Modbus_callback_2400 run,read !");
    }
    return 0;

}

int Modbus_callback_02500(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2500 run,write  !");
        if ( cb_data->SlaveAddress == 0 )
        {
            // memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), (uint8_t *)&Modbus_WR.mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);   
        }
        else
        {
            // memcpy((uint8_t *)&Inv[(cb_data->SlaveAddress - 1)].mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), (uint8_t *)&Modbus_WR.mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);   
        }
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2500 run,read !");
    }
    return 0;

}

int Modbus_callback_03000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3000 run,write  !");
    }
    else
    {
#ifdef MODBUS_GET_HISTORY_FROM_ARM_ENABLE
        /*插队读取3000段*/
        if(INV_TYPE_LOG_20H == reals.can_poll[INDEX_INV_TYPE_LOG_20H].can_type) {
            if(0 == reals.can_poll[INDEX_INV_TYPE_LOG_20H].modbus_Rd_cnt_100ms) {
                reals.can_poll[INDEX_INV_TYPE_LOG_20H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;    
            }
        } else {
            ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_LOG_20H].can_type);// 
        }    
#else
        if ( cb_data->SlaveAddress == 0 )
        {
            top_modbus_rd.Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history.total_page = GetHistoryLogFromLittlefs(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_log_page, cb_data->SlaveAddress);
        }
        else
        {
            top_modbus_rd.Inv[(cb_data->SlaveAddress - MD_INV_ADDR_START)].mod_reg03000_Inv_history.total_page = GetHistoryLogFromLittlefs(top_modbus_wr.Inv.mod_reg02000_Inv_base_set.ctrl_log_page, cb_data->SlaveAddress);
        }
#endif

        ESP_LOGI(TAG, "callback Modbus_callback_3000 run,read ! ");
    }
	
    return 0;
}

int Modbus_callback_03500(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3500 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3500 run,read !");  
    }
    return 0;
}

int Modbus_callback_03600(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3600 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3600 run,read !");
    }
    return 0;
}

int Modbus_callback_03700(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_03700 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_03700 run,read !");
    }
    return 0;
}

int Modbus_callback_04000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4000 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4000 run,read !");
    }
    return 0;
}

int Modbus_callback_04050(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4050 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4050 run,read !");
    }
    return 0;
}

int Modbus_callback_04105(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4105 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4105 run,read !");
    }
    return 0;
}
int Modbus_callback_06000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_06000 run,read !");
	
	return 0;
}
int Modbus_callback_06100(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_06100 run,write !");
    }
    else
    {
#if defined(MODBUS_READ_TRIGER_CAN_READ_ENABLE) && defined(BAT_CAN_PROTOCOL_BETA_ENABLE)
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取6100段*/
            if(PACK_TYPE_ABOUT_54H == reals.can_poll[INDEX_PACK_TYPE_ABOUT_54H].can_type) {
                if(0 == reals.can_poll[INDEX_PACK_TYPE_ABOUT_54H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_PACK_TYPE_ABOUT_54H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;    
                    reals.can_poll[INDEX_PACK_TYPE_ABOUT_54H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_PACK_TYPE_ABOUT_54H].can_type);// 
            }
        }   
#endif

        ESP_LOGI(TAG, "callback Modbus_callback_06100 run,read !");
    }
    return 0;
}

int Modbus_callback_06300(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_06300 run,write !");
    }
    else
    {
#if defined(MODBUS_READ_TRIGER_CAN_READ_ENABLE) && defined(BAT_CAN_PROTOCOL_BETA_ENABLE)
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取6300段*/
            if(PACK_TYPE_DEBUG1_56H == reals.can_poll[INDEX_PACK_TYPE_DEBUG1_56H].can_type) {
                if(0 == reals.can_poll[INDEX_PACK_TYPE_DEBUG1_56H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_PACK_TYPE_DEBUG1_56H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;    
                    reals.can_poll[INDEX_PACK_TYPE_DEBUG1_56H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_PACK_TYPE_DEBUG1_56H].can_type);// 
            }
        }   
#endif

        ESP_LOGI(TAG, "callback Modbus_callback_06300 run,read !");
    }
	return 0;
}

int Modbus_callback_07000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
        ESP_LOGI(TAG, "callback Modbus_callback_07000 run,write  !");
        
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg07000_Pack_set, (uint8_t *)&top_modbus_wr.Pack.mod_reg07000_Pack_set);
#endif

#ifdef PACK_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE
        memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg07000_Pack_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Pack.mod_reg07000_Pack_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
#endif
   	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_07000 run,read !");
#if defined(MODBUS_READ_TRIGER_CAN_READ_ENABLE) && defined(BAT_CAN_PROTOCOL_BETA_ENABLE)
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取7000段*/
            if(PACK_TYPE_CONFIG_55H == reals.can_poll[INDEX_PACK_TYPE_CONFIG_55H].can_type) {
                if(0 == reals.can_poll[INDEX_PACK_TYPE_CONFIG_55H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_PACK_TYPE_CONFIG_55H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;    
                    reals.can_poll[INDEX_PACK_TYPE_CONFIG_55H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_PACK_TYPE_CONFIG_55H].can_type);// 
            }
        }   
#endif
	}
    return 0;
}

int Modbus_callback_07200(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_07200 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_07200 run,read !");
    }
    return 0;
}

int Modbus_callback_11000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_11000 run,write  !");
		memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);

    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_11000 run,read !");
    }
    return 0;

}

int Modbus_callback_12000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;
	
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_12000 run,write  !");
        
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set, (uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set);
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
        // 更新参数更新记录
        if (( MD_CHL_CLOUD_SYNC != cb_data->cb_chl ) 
            && ( MD_CHL_WIFI_WLCC != cb_data->cb_chl )) {
            cloud_sync_queue_push(cb_data->SlaveAddress, cb_data->reg_addr, cb_data->reg_nums);    
        }
#endif

		memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg12000_IOT_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		

		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.factory)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.factory))
		{
			reals.ModbusCmdFlag.sBit.iot_factory_cmd = 1;
		}

		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_ssid))
		{
			reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 1;
		}
            
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_password))
		{
			reals.ModbusCmdFlag.sBit.wifi_sta_password = 1;
		}

		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_auth)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_sta_auth))
		{
			reals.ModbusCmdFlag.sBit.wifi_sta_auth = 1;
		}
            
        if (
           ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_ap_auth   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_ap_auth)   ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_ssid   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_ssid)   ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_password   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.wifi_AP_password))
          )
        {
            reals.ModbusCmdFlag.sBit.wifi_ap = 1;
        }

		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.could_dns[0])
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.could_dns[0]))
		{
			reals.ModbusCmdFlag.sBit.could_dns = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.mobile_apn[0])
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.mobile_apn[0]))
		{
			reals.ModbusCmdFlag.sBit.mobile_apn = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.on_off)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.on_off))
		{
			reals.ModbusCmdFlag.sBit.on_off = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.thunder_ctrl)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.thunder_ctrl))
		{
			reals.ModbusCmdFlag.sBit.thunder_ctrl = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.period_report)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.period_report))
		{
			reals.ModbusCmdFlag.sBit.period_report = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.IOT_Enable_mix1)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.IOT_Enable_mix1))
		{
			reals.ModbusCmdFlag.sBit.IOT_Enable_mix1 = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.IOT_Enable_mix2)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.IOT_Enable_mix2))
		{
			reals.ModbusCmdFlag.sBit.IOT_Enable_mix2 = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1))
		{
			reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix2)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Protocol_3r_Enable_mix2))
		{
			reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.Time_Span))
		{
			reals.ModbusCmdFlag.sBit.Time_Span = 1;
		}
		if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode)
			&&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg12000_IOT_set.LCD_Mode))
		{
			reals.ModbusCmdFlag.sBit.LCD_Mode = 1;
		}
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_12000 run,read !");
	}

	return 0;
}

int Modbus_callback_13000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
   if(true == cb_data->is_write)
   {
       ESP_LOGI(TAG, "callback Modbus_callback_13000 run,write  !");
   }
   else
   {
       ESP_LOGI(TAG, "callback Modbus_callback_13000 run,read !");
   }

	return 0;
}

int Modbus_callback_13500(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
   uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg13500_mesh + (cb_data->reg_addr_offset);
   uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg13500_mesh + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;
   if(true == cb_data->is_write)
   {
        ESP_LOGI(TAG, "callback Modbus_callback_13500_ run,write  !");
        memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg13500_mesh + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh, (uint8_t *)&top_modbus_wr.Inv.mod_reg13500_mesh);
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
       // 更新参数更新记录
       if (( MD_CHL_CLOUD_SYNC != cb_data->cb_chl ) 
           && ( MD_CHL_WIFI_WLCC != cb_data->cb_chl )) {
           cloud_sync_queue_push(cb_data->SlaveAddress, cb_data->reg_addr, cb_data->reg_nums);    
       }
#endif

       ESP_LOGI(TAG, "callback Modbus_callback_13500_ run,write  !");
       if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13500_mesh.mesh_id)
       &&(address_end >= (uint32_t)&top_modbus_wr.Inv.mod_reg13500_mesh.mesh_id))
       {
           if (memcmp(top_modbus_wr.Inv.mod_reg13500_mesh.mesh_id, IotSetData.dev_info_t.WIFI_Mesh_ID, 6) != 0) {
               memcpy((uint8_t *)IotSetData.dev_info_t.WIFI_Mesh_ID, (uint8_t *)top_modbus_wr.Inv.mod_reg13500_mesh.mesh_id, 6);
               memcpy((uint8_t *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id, (uint8_t *)top_modbus_wr.Inv.mod_reg13500_mesh.mesh_id, 6);
               reals.SetDataWrFlag.sBit.mesh_id = 1;
               reals.ModbusCmdFlag.sBit.mesh_id = 1;
               
               ESP_LOGW(TAG,"mesh_id:%d-%d-%d-%d-%d-%d",top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id[0],
                   top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id[1],
                   top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id[2],
                   top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id[3],
                   top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id[4],
                   top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id[5]);

           }
       }
   }
   else
   {
       ESP_LOGI(TAG, "callback Modbus_callback_13500_ run,read !");
   }

	return 0;
}

int Modbus_callback_13600(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{

    uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open + (cb_data->reg_addr_offset);
    uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_13600 run,write  !");

#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open, (uint8_t *)&top_modbus_wr.Inv.mod_reg13600_open);
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
        // 更新参数更新记录
        if (( MD_CHL_CLOUD_SYNC != cb_data->cb_chl ) 
            && ( MD_CHL_WIFI_WLCC != cb_data->cb_chl )) {
            cloud_sync_queue_push(cb_data->SlaveAddress, cb_data->reg_addr, cb_data->reg_nums);    
        }
#endif

        memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open + (cb_data->reg_addr_offset),
                (uint8_t *)&top_modbus_wr.Inv.mod_reg13600_open + (cb_data->reg_addr_offset),
                cb_data->reg_nums*2);

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.ble_protocol)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.ble_protocol))
        {
            reals.ModbusCmdFlag.sBit.ble_protocol = 1;
        }

        if (
            ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.blec_rssi_threshold   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.blec_rssi_threshold)   ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.blec_switch_interval && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.blec_switch_interval))
           )
        {
            reals.ModbusCmdFlag.sBit.ble_client = 1;
        }

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.bles_adv_key)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.bles_adv_key))
        {
            reals.ModbusCmdFlag.sBit.ble_server = 1;
        }

        if (
            ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_rssi_threshold   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_rssi_threshold)   ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_switch_interval && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta_switch_interval))
           )
        {
            reals.ModbusCmdFlag.sBit.wifi_sta_rssi = 1;
        }

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_mul_sta_en))
        {
            reals.ModbusCmdFlag.sBit.wifi_mul_sta = 1;
        }

        if (
            ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_ip   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_ip)   ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_mask && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_mask) ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_gw   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_gw)   ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns1 && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns1) ||
             (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns2 && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta1_dns2))
           )
        {
            reals.ModbusCmdFlag.sBit.wifi_sta1 = 1;
        }

        // wifi_sta2
        if (
          ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_auth   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_auth)   ||
           (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ssid   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ssid)   ||
           (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_password && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_password) ||
           (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ip     && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_ip)     ||
           (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_mask   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_mask)   ||
           (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_gw     && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_gw)     ||
           (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns1   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns1)   ||
           (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns2   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta2_dns2))
         )
        {
            reals.ModbusCmdFlag.sBit.wifi_sta2 = 1;
        }

        // wifi_sta3
        if (
           ((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_auth   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_auth)   ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ssid   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ssid)   ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_password && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_password) ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ip     && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_ip)     ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_mask   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_mask)   ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_gw     && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_gw)     ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns1   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns1)   ||
            (address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns2   && address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.wifi_sta3_dns2))
          )
        {
            reals.ModbusCmdFlag.sBit.wifi_sta3 = 1;
        }


        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_enable))
        {
            reals.ModbusCmdFlag.sBit.mqtt = 1;
        }

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_report_cycle)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.open_mqtt_report_cycle))
        {
            reals.ModbusCmdFlag.sBit.mqtt = 1;
        }

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_enable))
        {
            reals.ModbusCmdFlag.sBit.modbus_tcp = 1;
        }

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_port)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg13600_open.modbus_tcp_port))
        {
            reals.ModbusCmdFlag.sBit.modbus_tcp = 1;
        }
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_13600 run,read !");
    }

    return 0;
}

int Modbus_callback_14000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_14000 run !");
	
	return 0;
}

int Modbus_callback_14500(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
   if(true == cb_data->is_write)
   {
        ESP_LOGI(TAG, "callback Modbus_callback_14500 run,write  !");
   }
   else
   {
        ESP_LOGI(TAG, "callback Modbus_callback_14500 run,read !");
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE
        if((cb_data->SlaveAddress >= DEV_MODBUS_ADDR_START) && (cb_data->SlaveAddress <= DEV_MODBUS_ADDR_END)) {
            dev_modbus_plug_data_read_handle(gDevModbusAddrList[cb_data->SlaveAddress - DEV_MODBUS_ADDR_START].dev_type, 
                gDevModbusAddrList[cb_data->SlaveAddress - DEV_MODBUS_ADDR_START].dev_sn);
        } 
#endif       
   }

	return 0;
}

int Modbus_callback_14700(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
   if(true == cb_data->is_write)
   {
       ESP_LOGI(TAG, "callback Modbus_callback_14700 run,write  !");
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
       // 寄存器修改日志生成
       Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&g_other_rd.Plug[PLUG_MAX_NUM].mod_reg14700_SmartPlug_set, (uint8_t *)&g_other_wr.Plug.mod_reg14700_SmartPlug_set);
#endif

#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE
      // 写入设置触发回读
      if((cb_data->SlaveAddress >= DEV_MODBUS_ADDR_START) && (cb_data->SlaveAddress <= DEV_MODBUS_ADDR_END)) {
          dev_modbus_plug_set_read_handle(gDevModbusAddrList[cb_data->SlaveAddress - DEV_MODBUS_ADDR_START].dev_type, 
              gDevModbusAddrList[cb_data->SlaveAddress - DEV_MODBUS_ADDR_START].dev_sn, true);
      } 
#endif 

   }
   else
   {
       ESP_LOGI(TAG, "callback Modbus_callback_14700 run,read !");
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE
       if((cb_data->SlaveAddress >= DEV_MODBUS_ADDR_START) && (cb_data->SlaveAddress <= DEV_MODBUS_ADDR_END)) {
           dev_modbus_plug_set_read_handle(gDevModbusAddrList[cb_data->SlaveAddress - DEV_MODBUS_ADDR_START].dev_type, 
               gDevModbusAddrList[cb_data->SlaveAddress - DEV_MODBUS_ADDR_START].dev_sn, false);
       } 
#endif  
   }

	return 0;
}

int Modbus_callback_15500(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
       ESP_LOGI(TAG, "callback Modbus_callback_15500 run,write  !");

    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15500 run,read !");
    }
    return 0;

}

int Modbus_callback_15600(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	uint32_t address_begin = (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_15600 run,write  !");
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15600_DCDC_set, (uint8_t *)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set);
#endif

#ifdef PARTS_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE
        if ( cb_data->SlaveAddress == 0 ) {
            memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15600_DCDC_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        } else if(cb_data->SlaveAddress >= MD_DCDC_ADDR_START && cb_data->SlaveAddress < (MD_DCDC_ADDR_START + DCDC_MAX_NUM)) {
            memcpy((uint8_t *)&top_modbus_rd.Dcdc[(cb_data->SlaveAddress - MD_DCDC_ADDR_START)].mod_reg15600_DCDC_set + (cb_data->reg_addr_offset), 
                (uint8_t *)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        }
#endif

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
		if((address_begin <= (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.charger_set)
			&&(address_end > (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.charger_set))
		{
			reals.ModbusCmdFlag.sBit.dcdc_set1 = 1;
            for ( uint8_t index = 0 ; index < DCDC_MAX_NUM ; index++ ) {
                if ( can_node_rd.Dcdc[index].online ) {
                    can_node_rd.Dcdc[index].dcdc_common_set.charger_set.bit.dc_out_onoff = top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.charger_set.bit.dc_out_onoff;
                }
            }
		}

		if((address_begin <= (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.mode4_set)
			&&(address_end > (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.mode4_set))
		{
			reals.ModbusCmdFlag.sBit.dcdc_set1 = 1;
            for ( uint8_t index = 0 ; index < DCDC_MAX_NUM ; index++ ) {
                if ( can_node_rd.Dcdc[index].online ) {
                    can_node_rd.Dcdc[index].dcdc_common_set.mode4_set.bit.long_short_line = top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.mode4_set.bit.long_short_line;
                }
            }
		}

        for ( uint8_t i = 0 ; i < 6; i++ )
        {
            bool send_flag = false;
            if((address_begin <= (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_voltage_set)
                &&(address_end > (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_voltage_set))
            {
                send_flag |= true;
                for ( uint8_t index = 0 ; index < DCDC_MAX_NUM ; index++ ) {
                    if ( can_node_rd.Dcdc[index].online ) {
                        can_node_rd.Dcdc[index].dcdc_common_set.dc_val_set[i].dc_voltage_set = top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_voltage_set;
                    }
                }
            } else {
                top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_voltage_set = 0xFFFF;
            }
            
            if((address_begin <= (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_current_set)
                &&(address_end > (uint32_t)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_current_set))
            {
                send_flag |= true;
                for ( uint8_t index = 0 ; index < DCDC_MAX_NUM ; index++ ) {
                    if ( can_node_rd.Dcdc[index].online ) {
                        can_node_rd.Dcdc[index].dcdc_common_set.dc_val_set[i].dc_current_set = top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_current_set;
                    }
                }
            } else {
                top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_current_set = 0xFFFF;
            }
            
            /*DCDC设置更新*/
            if ( send_flag ) {
                Iot_Dcdc_Common_Target_Send(i + 1, top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_voltage_set, 
                    top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.dc_val_set[i].dc_current_set);
            }
        }
#endif        
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_15600 run,read !");
	}

	return 0;
}

int Modbus_callback_15700(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_15700 run,write  !");
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_15700 run,read !");
#if 0 // 使用私有协议
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取*/
            if(MODULE_TYPE_DC_HUB_INFO_41H == reals.can_poll[INDEX_MODULE_TYPE_DC_HUB_INFO_41H].can_type) {
                if(0 == reals.can_poll[INDEX_MODULE_TYPE_DC_HUB_INFO_41H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_MODULE_TYPE_DC_HUB_INFO_41H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_MODULE_TYPE_DC_HUB_INFO_41H].can_type);// 
            }  
        }       
#endif        
	}

	return 0;
}

int Modbus_callback_15750(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_15750 run,write  !");
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg15750_Dc_Ac_Hub_set, (uint8_t *)&top_modbus_wr.DcHub.mod_reg15750_Dc_Ac_Hub_set);
#endif

#ifdef PARTS_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE
        if ( cb_data->SlaveAddress == 0 ) {
            memcpy((uint8_t *)&top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg15750_Dc_Ac_Hub_set + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.DcHub.mod_reg15750_Dc_Ac_Hub_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        } else if(cb_data->SlaveAddress >= MD_DCHUB_ADDR_START && cb_data->SlaveAddress < (MD_DCHUB_ADDR_START + DC_HUB_MAX_NUM)) {
            memcpy((uint8_t *)&top_modbus_rd.DcHub[(cb_data->SlaveAddress - MD_DCHUB_ADDR_START)].mod_reg15750_Dc_Ac_Hub_set + (cb_data->reg_addr_offset), 
                (uint8_t *)&top_modbus_wr.DcHub.mod_reg15750_Dc_Ac_Hub_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
        }
#endif
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_15750 run,read !");
#if 0 // 使用私有协议
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取*/
            if(MODULE_TYPE_DC_AC_HUB_SET_40H == reals.can_poll[INDEX_MODULE_TYPE_DC_AC_HUB_SET_40H].can_type) {
                if(0 == reals.can_poll[INDEX_MODULE_TYPE_DC_AC_HUB_SET_40H].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_MODULE_TYPE_DC_AC_HUB_SET_40H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_MODULE_TYPE_DC_AC_HUB_SET_40H].can_type);// 
            }  
        }       
#endif
	}

	return 0;
}

int Modbus_callback_19000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19000 run,write  !");
       
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet01, (uint8_t *)&top_modbus_wr.Inv.mod_reg19000_Relay_ctrl);
#endif

	    Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&top_modbus_wr.Inv.mod_reg19000_Relay_ctrl.GeneralPowerRelaySoc, POWERRELAY_SMART_SET01, cb_data->reg_addr_offset, cb_data->reg_nums*2);
    }
    else
    {
       ESP_LOGI(TAG, "callback Modbus_callback_19000 run,read !");
    }
    return 0;
}

int Modbus_callback_19100(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19100 run,write  !");
       
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet02, (uint8_t *)&top_modbus_wr.Inv.mod_reg19100_Relay_ctrl_2);
#endif       

	    Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&top_modbus_wr.Inv.mod_reg19100_Relay_ctrl_2.GeneralPowerRelay_Delay_Time_En, POWERRELAY_SMART_SET02, cb_data->reg_addr_offset, cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19100 run,read !");
    }
    return 0;
}

int Modbus_callback_19200(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19200 run,write  !");
        
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&RelaySetData.Relay_info_t.Backup_power_set, (uint8_t *)&top_modbus_wr.Inv.mod_reg19200_Backup_power);
#endif 

		Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&top_modbus_wr.Inv.mod_reg19200_Backup_power, POWERRELAY_SMART_SET04, cb_data->reg_addr_offset, cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19200 run,read !");
    }

    return 0;
}


int Modbus_callback_19300(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19300 run,write  !");
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet03, (uint8_t *)&top_modbus_wr.Inv.mod_reg19300_Relay_ctrl_3);
#endif 
	    Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&top_modbus_wr.Inv.mod_reg19300_Relay_ctrl_3.GeneralPowerRelay_Set_Time_En, POWERRELAY_SMART_SET03, cb_data->reg_addr_offset, cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19300 run,read !");
    }
    return 0;
}

int Modbus_callback_21000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_21000 run,write  !");
		if (top_modbus_wr.Inv.mod_reg21000_bind_WR.ver == 30001)
        {
            /*绑定帧*/
            ESP_LOGW(TAG, "protocol version : %d", top_modbus_wr.Inv.mod_reg21000_bind_WR.ver);
            uint8_t bind_num = (cb_data->reg_nums -2)/6;
            if ( bind_num > 0 ) {
                Modbus_21000_bind_msg_push(bind_num, &top_modbus_wr.Inv.mod_reg21000_bind_WR.point_bind_info[0]);
            }
        }
#ifdef CONFIG_MODBUS_REG_TLV_ENABLE        
        else if (( top_modbus_wr.Inv.mod_reg21000_bind_WR.ver == MODBUS_21000_FUNC_TLV_DATA ) 
            || ( top_modbus_wr.Inv.mod_reg21000_bind_WR.ver == MODBUS_21000_FUNC_TLV_READ_CMD ))
        {
            if (( 0 == cb_data->reg_addr_offset ) && ( 0 < cb_data->reg_nums )) {
                modbus_tlv_queue_push((const uint8_t *)&top_modbus_wr.Inv.mod_reg21000_bind_WR.ver, (cb_data->reg_nums * 2), cb_data->cb_chl);
            }
        }
#endif            
        else
        {
            ESP_LOGI(TAG, "write protocol version : %d",top_modbus_wr.Inv.mod_reg21000_bind_WR.ver);
            if ( MD_CHL_BLE == cb_data->cb_chl) {
            	ESP_LOGI(TAG, "MD_CHL_BLE");
                reals.modbus_self_report_ble = top_modbus_wr.Inv.mod_reg21000_bind_WR.ver;	
            } else if ( MD_CHL_WIFI_CLOUD == cb_data->cb_chl ) {
                reals.modbus_self_report_mqtt = top_modbus_wr.Inv.mod_reg21000_bind_WR.ver;	
            }
        }

	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_21000 run,read !");
	}

	return 0;
}

int Modbus_callback_22000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_22000 run !");
    if(true == cb_data->is_write)
    {
    	ESP_LOGI(TAG, "callback Modbus_callback_22000 run,write  !");
    	
    	reals.ModbusCmdFlag.sBit.cfg_22000 = 1;			// iot 收到新的配置
    	memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg22000_net_server_2rd + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg22000_net_server_2rd + (cb_data->reg_addr_offset), cb_data->reg_nums*2);	
    }
    else
    {
    	ESP_LOGI(TAG, "callback Modbus_callback_22000 run,read !");
    }	
	return 0;
}

int Modbus_callback_26000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_26000 run,write  !");
#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
        Smart_Tou_Ctrl_Config_Commit(cb_data->reg_addr_offset, (cb_data->reg_nums * 2));
#endif

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE
        // 更新参数更新记录
        if (( MD_CHL_CLOUD_SYNC != cb_data->cb_chl ) 
            && ( MD_CHL_WIFI_WLCC != cb_data->cb_chl )) {
            cloud_sync_queue_push(cb_data->SlaveAddress, cb_data->reg_addr, cb_data->reg_nums);    
        }
#endif

    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_26000 run,read !");
    }
    return 0;
}

int Modbus_callback_28000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_28000 run,write  !");
#ifdef CONFIG_AC_EMS_CLOUD_ENABLE
        Ac_Ems_Cloud_Config_Commit(cb_data->reg_addr_offset, (cb_data->reg_nums * 2));
#endif
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_28000 run,read !");
    }
    return 0;
}

int Modbus_callback_29800(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_29800 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_29800 run,read !");
    }
    return 0;
}

int Modbus_callback_30000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_30000 run,write !");
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
        // 寄存器修改日志生成
        Modbus_Reg_Change_log_set(cb_data, (uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg30000_Pack_paygo, (uint8_t *)&top_modbus_wr.Pack.mod_reg30000_Pack_paygo);
#endif

#ifdef PACK_MODBUS_WRITE_DATA_PREDICT_CHANGE_ENABLE
        memcpy((uint8_t *)&top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg30000_Pack_paygo + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Pack.mod_reg30000_Pack_paygo + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
#endif
    }
    else
    {
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if ( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD )
        {
            /*插队读取30000段*/
            if(PACK_TYPE_BMS_PAYGO_INFO_5BH == reals.can_poll[INDEX_PACK_TYPE_BMS_PAYGO_INFO_5BH].can_type) {
                if(0 == reals.can_poll[INDEX_PACK_TYPE_BMS_PAYGO_INFO_5BH].modbus_Rd_cnt_100ms) {
                    reals.can_poll[INDEX_PACK_TYPE_BMS_PAYGO_INFO_5BH].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;    
                    reals.can_poll[INDEX_PACK_TYPE_BMS_PAYGO_INFO_5BH].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
                }
            } else {
                ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_PACK_TYPE_BMS_PAYGO_INFO_5BH].can_type);// 
            }
        }   
#endif
        ESP_LOGI(TAG, "callback Modbus_callback_30000 run,read !");
    }
	return 0;
}

int Modbus_callback_30900(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	uint32_t address_begin = (uint32_t)&top_modbus_wr.Inv.mod_reg30900_test + (cb_data->reg_addr_offset);
    uint32_t address_end = (uint32_t)&top_modbus_wr.Inv.mod_reg30900_test + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_30900 run,write	!");
#if CONFIG_LOG_UDP_REDIRECT == 1		
		MOD_STRUCT_reg30900 data = {0};
		int reg_nums = cb_data->reg_nums;
		uint16_t data_offset = cb_data->reg_addr_offset;
		while (reg_nums > 0)
		{
			switch (data_offset)
			{
				case offsetof(MOD_STRUCT_reg30900, log_mark):
					data_offset += sizeof(data.log_mark);
					reg_nums -= (sizeof(data.log_mark) / 2);             
					log_type_set(top_modbus_wr.Inv.mod_reg30900_test.log_mark);
					break;
				
				case offsetof(MOD_STRUCT_reg30900, log_mode):
					data_offset += sizeof(data.log_mode);
					reg_nums -= (sizeof(data.log_mode) / 2);              
                    log_switch_mode(top_modbus_wr.Inv.mod_reg30900_test.log_mode);
					break;
					
				case offsetof(MOD_STRUCT_reg30900, log_port):
					data_offset += sizeof(data.log_port);
					reg_nums -= (sizeof(data.log_port) / 2);               
					log_server_port_set(top_modbus_wr.Inv.mod_reg30900_test.log_port);
					break;

				case offsetof(MOD_STRUCT_reg30900, log_dst_ip):
					data_offset += sizeof(data.log_dst_ip);
					reg_nums -= (sizeof(data.log_dst_ip) / 2);            
					log_server_ip_set(top_modbus_wr.Inv.mod_reg30900_test.log_dst_ip);
					break;
					
				default:
					ESP_LOGW(TAG, "unknown modbus register: %d", (MOD_REG_START_ADDR_30900+data_offset/2));
					data_offset += sizeof(uint16_t);
					reg_nums -= 1;
					break;
			}
		}
#elif CONFIG_LOG_UDP_REDIRECT == 2
        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg30900_test.test_work_mode)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg30900_test.test_work_mode))
        {
            if (0x2138 == top_modbus_wr.Inv.mod_reg30900_test.test_work_mode)
            {
                reals.supper_control.mode = 1;
                ESP_LOGI(TAG, "enter supper control mode !");
            }
            else if (0x2139 == top_modbus_wr.Inv.mod_reg30900_test.test_work_mode)
            {
                reals.supper_control.mode = 2;
                ESP_LOGI(TAG, "exit supper control mode !");
            }
        }

        if((address_begin <= (uint32_t)&top_modbus_wr.Inv.mod_reg30900_test.cmd_value)
            &&(address_end > (uint32_t)&top_modbus_wr.Inv.mod_reg30900_test.cmd_value))
        {
            if (1 == reals.supper_control.mode)
            {
                suppor_control_set_value(top_modbus_wr.Inv.mod_reg30900_test.super_cmd,
                                        top_modbus_wr.Inv.mod_reg30900_test.cmd_value);
            }
        }
#endif
		
		memcpy((uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg30900_test + (cb_data->reg_addr_offset), (uint8_t *)&top_modbus_wr.Inv.mod_reg30900_test + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
	}
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_30900 run,read !");
#if CONFIG_LOG_UDP_REDIRECT == 2		
        // 超级远程控制模式
        if (1 == reals.supper_control.mode)
        {
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg30900_test.cmd_value 
                = suppor_control_get_value(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg30900_test.super_cmd);
        }
#endif		
    }
    return 0;
}

int Modbus_callback_40000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_40000 run,write  !");
	}
	else
	{
#ifdef MODBUS_READ_TRIGER_CAN_READ_ENABLE
        if (( cb_data->cb_chl == MD_CHL_BLE || cb_data->cb_chl == MD_CHL_WIFI_CLOUD ) 
            && (0 != cb_data->SlaveAddress))
        {
		    /*插队读取40000段*/
			if(COMMON_TYPE_TRANSMIT_27H == reals.can_poll[INDEX_COMMON_TYPE_TRANSMIT_27H].can_type) {
				if(0 == reals.can_poll[INDEX_COMMON_TYPE_TRANSMIT_27H].modbus_Rd_cnt_100ms) {
					reals.can_poll[INDEX_COMMON_TYPE_TRANSMIT_27H].modbus_Rd_cnt_100ms = MODBUS_READ_TRIGER_CAN_READ_TIME_CNT_100MS;	
                    reals.can_poll[INDEX_COMMON_TYPE_TRANSMIT_27H].modbus_Rd_SlaveAddr = cb_data->SlaveAddress;
				}
			} else {
				ESP_LOGE(TAG, "error : reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_COMMON_TYPE_TRANSMIT_27H].can_type);// 
			}	
        }
#endif	
		ESP_LOGI(TAG, "callback Modbus_callback_40000 run,read !");
	}

	return 0;
}

#if 0

static uint16_t md_data_len;						  // 表数量
md_data_t md_data[42];

void md_data_init(void)
{
	uint16_t i=0;

	md_data[i].tbl.start = MOD_REG_START_ADDR_00000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000);
	md_data[i].tbl.is_write= false; 
	md_data[i].tbl.tbl_cb=Modbus_callback_00000;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_00100;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100);
	md_data[i].tbl.is_write= false; 
	md_data[i].tbl.tbl_cb=Modbus_callback_00100;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_00700;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_00700 + MOD_REG_LEN_00700);
	md_data[i].tbl.is_write= false; 
	md_data[i].tbl.tbl_cb=Modbus_callback_00700;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_01100;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100);
	md_data[i].tbl.is_write= false, 
	md_data[i].tbl.tbl_cb=Modbus_callback_01100;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_01200;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_01200 + MOD_REG_LEN_01200);
	md_data[i].tbl.is_write= false; 
	md_data[i].tbl.tbl_cb=Modbus_callback_01200;
	md_data[i].data=NULL;	//
	i++;

		
	md_data[i].tbl.start = MOD_REG_START_ADDR_01300;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300);
	md_data[i].tbl.is_write= false; 
	md_data[i].tbl.tbl_cb=Modbus_callback_01300;
	md_data[i].data=NULL;	//
	i++;
		
	md_data[i].tbl.start = MOD_REG_START_ADDR_01400;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400);
	md_data[i].tbl.is_write= false; 
	md_data[i].tbl.tbl_cb=Modbus_callback_01400;
	md_data[i].data=NULL;	//
	i++;
		
	md_data[i].tbl.start = MOD_REG_START_ADDR_01500;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_01500 + MOD_REG_LEN_01500);
	md_data[i].tbl.is_write= false, 
	md_data[i].tbl.tbl_cb=Modbus_callback_01500;
	md_data[i].data=NULL;	//
	i++;
	//	
	////md_data[i].tbl.start = MOD_REG_START_ADDR_01600;
	////md_data[i].tbl.end = (MOD_REG_START_ADDR_01600 + MOD_REG_LEN_01600);
	////md_data[i].tbl.is_write= false, 
	////md_data[i].tbl.tbl_cb=Modbus_callback_01600;
	////md_data[i].data=NULL;	//
	////i++;
	//
	////	
	md_data[i].tbl.start = MOD_REG_START_ADDR_02000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_02000;
	md_data[i].data=NULL;	//
	i++;

	////	
	md_data[i].tbl.start = MOD_REG_START_ADDR_02200;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_02200;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_02400;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_02400;
	md_data[i].data=NULL;	//
	i++;
		
	md_data[i].tbl.start = MOD_REG_START_ADDR_02500;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_02500 + MOD_REG_LEN_02500);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_02500;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_03000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_03000 + MOD_REG_LEN_03000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_03000;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_03500;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_03500 + MOD_REG_LEN_03500);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_03500;
	md_data[i].data=NULL;	//
	i++;

	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_03600;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_03600 + MOD_REG_LEN_03600);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_03600;
	md_data[i].data=NULL;	//
	i++;
	//	
	//md_data[i].tbl.start = MOD_REG_START_ADDR_04000;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_04000 + MOD_REG_LEN_04000);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_04000;
	//md_data[i].data=NULL;	//
	//i++;
		
	//md_data[i].tbl.start = MOD_REG_START_ADDR_04050;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_04050 + MOD_REG_LEN_04050);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_04050;
	//md_data[i].data=NULL;	//
	//i++;

	//md_data[i].tbl.start = MOD_REG_START_ADDR_04105;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_04105 + MOD_REG_LEN_04105);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_04105;
	//md_data[i].data=NULL;	//
	//i++;

	md_data[i].tbl.start = MOD_REG_START_ADDR_06000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_06000 + MOD_REG_LEN_06000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_06000;
	md_data[i].data=NULL;	//
	i++;
		
	md_data[i].tbl.start = MOD_REG_START_ADDR_06100;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_06100 + MOD_REG_LEN_06100);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_06100;
	md_data[i].data=NULL;	//
	i++;

	md_data[i].tbl.start = MOD_REG_START_ADDR_06300;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_06300 + MOD_REG_LEN_06300);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_06300;
	md_data[i].data=NULL;	//
	i++;

	//
	md_data[i].tbl.start = MOD_REG_START_ADDR_07000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_07000 + MOD_REG_LEN_07000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_07000;
	md_data[i].data=NULL;	//
	i++;

	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_11000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_11000 + MOD_REG_LEN_11000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_11000;
	md_data[i].data=NULL;	//
	i++;
	//	
	
	md_data[i].tbl.start = MOD_REG_START_ADDR_12000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_12000;
	md_data[i].data=NULL;	//
	i++;

    md_data[i].tbl.start = MOD_REG_START_ADDR_13600;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_13600 + MOD_REG_LEN_13600);
    md_data[i].tbl.is_write = false;
    md_data[i].tbl.tbl_cb = Modbus_callback_13600;
    md_data[i].data=NULL;
    i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_13500;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_13500 + MOD_REG_LEN_13500);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_13500;
	md_data[i].data=NULL;	//
	i++;

	//md_data[i].tbl.start = MOD_REG_START_ADDR_14000;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_14000 + MOD_REG_LEN_14000);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_14000;
	//md_data[i].data=NULL;	//
	//i++;

	md_data[i].tbl.start = MOD_REG_START_ADDR_14500;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_14500;
	md_data[i].data=NULL;	//
	i++;

	//md_data[i].tbl.start = MOD_REG_START_ADDR_14700;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_14700;
	//md_data[i].data=NULL;	//
	//i++;
	md_data[i].tbl.start = MOD_REG_START_ADDR_15500;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_15500 + MOD_REG_LEN_15500);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_15500;
	md_data[i].data=NULL;	//
	i++;
	//	
	md_data[i].tbl.start = MOD_REG_START_ADDR_19000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_19000;
	md_data[i].data=NULL;	//
	i++;

	md_data[i].tbl.start = MOD_REG_START_ADDR_19100;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_19100 + MOD_REG_LEN_19100);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_19100;
	md_data[i].data=NULL;	//
	i++;

	md_data[i].tbl.start = MOD_REG_START_ADDR_19200;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_19200 + MOD_REG_LEN_19200);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_19200;
	md_data[i].data=NULL;	//
	i++;

	md_data[i].tbl.start = MOD_REG_START_ADDR_19300;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_19300 + MOD_REG_LEN_19300);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_19300;
	md_data[i].data=NULL;	//
	i++;

	md_data[i].tbl.start = MOD_REG_START_ADDR_15600;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_15600;
	md_data[i].data=NULL;	//
	i++;
		
	md_data[i].tbl.start = MOD_REG_START_ADDR_21000;
	md_data[i].tbl.end = (MOD_REG_START_ADDR_21000 + MOD_REG_LEN_21000);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_21000;
	md_data[i].data=NULL;	//
	i++;
		

	//md_data[i].tbl.start = MOD_REG_START_ADDR_22000;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_22000 + MOD_REG_LEN_22000);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_22000;
	//md_data[i].data=NULL;	//
	//i++;

#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
	md_data[i].tbl.start = SMART_TOU_REG_START_ADDR;
	md_data[i].tbl.end = (SMART_TOU_REG_START_ADDR + SMART_TOU_REG_MAX_LEN);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_26000;
	md_data[i].data=NULL;	//
	i++;
#endif

#ifdef CONFIG_AC_EMS_CLOUD_ENABLE
	md_data[i].tbl.start = AC_EMS_CLOUD_REG_START_ADDR;
	md_data[i].tbl.end = (AC_EMS_CLOUD_REG_START_ADDR + AC_EMS_CLOUD_REG_MAX_LEN);
	md_data[i].tbl.is_write= false;
	md_data[i].tbl.tbl_cb=Modbus_callback_28000;
	md_data[i].data=NULL;	//
	i++;
#endif

	//md_data[i].tbl.start = MOD_REG_START_ADDR_29700;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_29700 + MOD_REG_LEN_29700);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_29700;
	//md_data[i].data=NULL;	//
	//i++;	

	//md_data[i].tbl.start = MOD_REG_START_ADDR_40000;
	//md_data[i].tbl.end = (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000);
	//md_data[i].tbl.is_write= false;
	//md_data[i].tbl.tbl_cb=Modbus_callback_40000;
	//md_data[i].data=NULL;	//
	//i++;		

}

#else

// 静态modbus寄存器表定义
static const md_data_t md_data[] = {
    { .tbl = { MOD_REG_START_ADDR_00000, MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000, false, Modbus_callback_00000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_00100, MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100, false, Modbus_callback_00100 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_00700, MOD_REG_START_ADDR_00700 + MOD_REG_LEN_00700, false, Modbus_callback_00700 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_01100, MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100, false, Modbus_callback_01100 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_01200, MOD_REG_START_ADDR_01200 + MOD_REG_LEN_01200, false, Modbus_callback_01200 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_01300, MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300, false, Modbus_callback_01300 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_01400, MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400, false, Modbus_callback_01400 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_01500, MOD_REG_START_ADDR_01500 + MOD_REG_LEN_01500, false, Modbus_callback_01500 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_01600, MOD_REG_START_ADDR_01600 + MOD_REG_LEN_01600, false, Modbus_callback_01600 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_01700, MOD_REG_START_ADDR_01700 + MOD_REG_LEN_01700, false, Modbus_callback_01700 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_02000, MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000, false, Modbus_callback_02000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_02200, MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200, false, Modbus_callback_02200 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_02300, MOD_REG_START_ADDR_02300 + MOD_REG_LEN_02300, false, Modbus_callback_02300 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_02400, MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400, false, Modbus_callback_02400 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_02500, MOD_REG_START_ADDR_02500 + MOD_REG_LEN_02500, false, Modbus_callback_02500 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_03000, MOD_REG_START_ADDR_03000 + MOD_REG_LEN_03000, false, Modbus_callback_03000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_03500, MOD_REG_START_ADDR_03500 + MOD_REG_LEN_03500, false, Modbus_callback_03500 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_03600, MOD_REG_START_ADDR_03600 + MOD_REG_LEN_03600, false, Modbus_callback_03600 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_03700, MOD_REG_START_ADDR_03700 + MOD_REG_LEN_03700, false, Modbus_callback_03700 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_04000, MOD_REG_START_ADDR_04000 + MOD_REG_LEN_04000, false, Modbus_callback_04000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_04050, MOD_REG_START_ADDR_04050 + MOD_REG_LEN_04050, false, Modbus_callback_04050 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_04105, MOD_REG_START_ADDR_04105 + MOD_REG_LEN_04105, false, Modbus_callback_04105 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_06000, MOD_REG_START_ADDR_06000 + MOD_REG_LEN_06000, false, Modbus_callback_06000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_06100, MOD_REG_START_ADDR_06100 + MOD_REG_LEN_06100, false, Modbus_callback_06100 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_06300, MOD_REG_START_ADDR_06300 + MOD_REG_LEN_06300, false, Modbus_callback_06300 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_07000, MOD_REG_START_ADDR_07000 + MOD_REG_LEN_07000, false, Modbus_callback_07000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_07200, MOD_REG_START_ADDR_07200 + MOD_REG_LEN_07200, false, Modbus_callback_07200 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_11000, MOD_REG_START_ADDR_11000 + MOD_REG_LEN_11000, false, Modbus_callback_11000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_12000, MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000, false, Modbus_callback_12000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_13000, MOD_REG_START_ADDR_13000 + MOD_REG_LEN_13000, false, Modbus_callback_13000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_13500, MOD_REG_START_ADDR_13500 + MOD_REG_LEN_13500, false, Modbus_callback_13500 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_13600, MOD_REG_START_ADDR_13600 + MOD_REG_LEN_13600, false, Modbus_callback_13600 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_14500, MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500, false, Modbus_callback_14500 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_14700, MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700, false, Modbus_callback_14700 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_15500, MOD_REG_START_ADDR_15500 + MOD_REG_LEN_15500, false, Modbus_callback_15500 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_15600, MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600, false, Modbus_callback_15600 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_15700, MOD_REG_START_ADDR_15700 + MOD_REG_LEN_15700, false, Modbus_callback_15700 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_15750, MOD_REG_START_ADDR_15750 + MOD_REG_LEN_15750, false, Modbus_callback_15750 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_19000, MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000, false, Modbus_callback_19000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_19100, MOD_REG_START_ADDR_19100 + MOD_REG_LEN_19100, false, Modbus_callback_19100 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_19200, MOD_REG_START_ADDR_19200 + MOD_REG_LEN_19200, false, Modbus_callback_19200 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_19300, MOD_REG_START_ADDR_19300 + MOD_REG_LEN_19300, false, Modbus_callback_19300 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_21000, MOD_REG_START_ADDR_21000 + MOD_REG_LEN_21000, false, Modbus_callback_21000 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_22000, MOD_REG_START_ADDR_22000 + MOD_REG_LEN_22000, false, Modbus_callback_22000 }, .data = NULL },
#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE    
    { .tbl = { SMART_TOU_REG_START_ADDR, SMART_TOU_REG_START_ADDR + SMART_TOU_REG_MAX_LEN, false, Modbus_callback_26000 }, .data = NULL },
#endif
#ifdef CONFIG_AC_EMS_CLOUD_ENABLE
    { .tbl = { AC_EMS_CLOUD_REG_START_ADDR, AC_EMS_CLOUD_REG_START_ADDR + AC_EMS_CLOUD_REG_MAX_LEN, false, Modbus_callback_28000 }, .data = NULL },
#endif    
    { .tbl = { MOD_REG_START_ADDR_29800, MOD_REG_START_ADDR_29800 + MOD_REG_LEN_29800, false, Modbus_callback_29800 }, .data = NULL },    
#ifdef CONFIG_FUNC_PAYGO_ENABLE
    { .tbl = { MOD_REG_START_ADDR_30000, MOD_REG_START_ADDR_30000 + MOD_REG_LEN_30000, false, Modbus_callback_30000 }, .data = NULL },
#endif
    { .tbl = { MOD_REG_START_ADDR_30900, MOD_REG_START_ADDR_30900 + MOD_REG_LEN_30900, false, Modbus_callback_30900 }, .data = NULL },
    { .tbl = { MOD_REG_START_ADDR_40000, MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000, false, Modbus_callback_40000 }, .data = NULL },
    // ...如有更多项，继续补充
};

static const size_t md_data_len = sizeof(md_data) / sizeof(md_data[0]);

#endif

/**
* @brief :Modbus_beta_reg_table_register
初始化modbus beta寄存器表格的寄存器块和对应的回调函数关联

* @param[in] :	   
* @param[out] : 
* @return :	 
*/
void Modbus_beta_reg_table_register_init(void)
{
    /*信号量初始化*/    
    modbus_data_semaphore_init();

#if 0
    /*初始化modbus数据表*/
	md_data_init();

    /*计算modbus寄存器段数量*/
	md_data_len = sizeof(md_data)/sizeof(md_data[0]);
#endif	

	ESP_LOGI(TAG, "[Modbus_beta_reg_table_register_init] md_data_len = %u ", md_data_len);
}

/**
 * @brief modbus错误响应
 *
 * @param in_buff 数据缓存
 * @param out_buff 响应缓存
 * @param len 数据长度
 * @return 无
 */
 static void mb_rsp_error(uint8_t *in_buff, uint8_t *out_buff, int *len)
 {
 	uint8_t idx = 0;
 	out_buff[idx++] = in_buff[0];
 	out_buff[idx++] = in_buff[1] | MB_FUNCODE_ERROR;
 	out_buff[idx++] = MB_ERROR_NOT_SUPPORTED;
 	uint16_t crc = CalcCrc16_modbus(out_buff, idx);
 	out_buff[idx++] = crc;
 	out_buff[idx++] = crc >> 8;
 	*len = idx;
 }
 
 /**
  * @brief 根据寄存器地址查找寄存器表
  *
  * @param reg_addr 寄存器地址
  * @return 成功返回寄存器表地址，失败返回NULL
  */
 md_data_t *md_tbl_find(uint16_t reg_addr)
 {
     /* 查找寄存器表 */
     for (uint16_t i = 0; i < md_data_len; i++)
     {
         if ((reg_addr >= md_data[i].tbl.start) && (reg_addr <= md_data[i].tbl.end)) 
         {       
             return &md_data[i];
         }
     }
     ESP_LOGE(TAG, "md_tbl_find error : reg_addr(%u)", reg_addr);
     return NULL;
 }

/**
 * @brief modbus协议检查
 *
 * @param buff 指令数据缓存
 * @param len 数据长度
 * @return 成功返回0，失败返回-1
 */
int md_protocol_check(uint8_t *buff, int len)
{
    /* modbus协议检测及crc校验 */
    if(buff[0] != MB_ADDR) return -1;
    if((buff[1] != MB_WRITE_MULTI_HOLD_REG) &&
	   (buff[1] != MB_WRITE_ONE_HOLD_REG) &&
	   (buff[1] != MB_READ_HOLD_REG)) return -1;

    uint16_t crc_old = ((uint16_t)buff[len-1] << 8) | buff[len-2];
    uint16_t crc_new = CalcCrc16_modbus(buff, (len-2));
	return (crc_old==crc_new) ? (0) : (-1);
}

/*------------------------------------------------------------------------
*@Function： Modbus_Format_Check
判断接收报文是否为modbus格式

*@param[in]     *income
*@param[out]    inlen
*@return         
-1： fail
other:功能码

*/
int Modbus_Format_Check(const uint8_t *income, uint16_t inlen) {
    if (!income || (inlen < 5)) {
        return -1; // modbus unknown pack
    }

    if ((income[1] != 0x03) && (income[1] != 0x06) && (income[1] != 0x10)) {
        return -1;
    }

    uint16_t crc16 = ModbusCrc16(income, (inlen - 2));
    if (crc16 != ((income[inlen - 1]<<8) | income[inlen - 2])) { // crc check
        // esp_log_buffer_hex(TAG, income, inlen);
        return -1; // modbus unknown pack
    }

    return income[1]; /* 返回接收的功能码 */
}

int Modbus_Rebuild_Frame_With_Addr(uint8_t slaveAddr, uint8_t *pIn, uint8_t inLen, uint8_t *pOut)
{
	uint16_t crc;
	
	pOut[0] = slaveAddr;
	memcpy(pOut+1, pIn+1, inLen-3);

    crc = ModbusCrc16(pOut, inLen-2);

	pOut[inLen-2] = (uint8_t) crc;
    pOut[inLen-1] = (uint8_t)(crc>>8);

	return 0;
}

/**
 * @brief 从内部寄存器表中获取数据
 *
 * @param reg_addr 寄存器地址
 * @param data 数据
 * @param reg_nums 寄存器数
 * @return 成功返回0，失败返回-1
 */
int md_reg_data_get(uint16_t reg_addr, uint16_t *data, int reg_nums)
{
	md_data_t *p_data;
	uint16_t actual_nums = reg_nums;

	p_data = md_tbl_find(reg_addr);
	// ESP_RETURN_ON_FALSE(p_data, -1, TAG, "find register table failure, line: %d", __LINE__);

	/* 如果寄存器数量大于寄存器表定义的大小,则使用实际寄存器大小,未定义的寄存器不动作 */
	uint16_t remain_len = p_data->tbl.end - reg_addr + 1;
	if (reg_nums > remain_len) {
		actual_nums = remain_len;
	}

	/* 从寄存器表中取出数据 */
	uint16_t *ptr = &p_data->data[reg_addr-p_data->tbl.start];
	memcpy(data, ptr, sizeof(uint16_t)*actual_nums);
	if (reg_nums > actual_nums) {	// 多余的数据清零
		memset(data+actual_nums, 0x00, sizeof(uint16_t)*(reg_nums-actual_nums));
	}
	return 0;
}

/**
 * @brief 设置数据到内部寄存器表
 *
 * @param reg_addr 寄存器地址
 * @param data 数据缓存
 * @param reg_nums 寄存器数
 * @return 成功返回0，失败返回-1
 */
int md_reg_data_set(uint16_t reg_addr, uint16_t *data, int reg_nums)
{
	md_data_t *p_data;
	uint16_t actual_nums = reg_nums;

	p_data = md_tbl_find(reg_addr);
	// ESP_RETURN_ON_FALSE(p_data, -1, TAG, "find register table failure, line: %d", __LINE__);

	/* 如果设置的寄存器数量大于寄存器表定义的大小,则使用实际寄存器大小,未定义的寄存器不动作 */
	uint16_t remain_len = p_data->tbl.end - reg_addr + 1;
	if (reg_nums > remain_len) {
		actual_nums = remain_len;
	}

	/* 数据复制到寄存器表中 */
	uint16_t *ptr = &p_data->data[reg_addr-p_data->tbl.start];
	memcpy(ptr, data, sizeof(uint16_t)*actual_nums);
	return 0;
}



