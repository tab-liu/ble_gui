/**
  ******************************************************************************
  * @file      can_product.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/2/24
  * @brief     CAN标定模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/2/24  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */
 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "utils.h"
#include "can_product.h"
#include "comm_define.h"
#include "can_data.h"
#include "parameter.h"
#include "nvs_store.h"
#include "app_bt.h"
#include "iot_mqtt.h"
#include "can_protocol.h"
#include "modbus_data.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[CAN_PRODUCT]"

/* =============================== 文件内全局变量 ================================ */


/* ============================== 文件内引用外部接口 ============================= */


/* ================================ 模块函数定义 ================================ */

/**
  * @brief      CAN标定数据初始化
  * @param[in]  char *model        
                uint64_t sn        
                uint64_t safecode  
  * @param[out] None
  * @return     void
  */
void Can_Product_Data_Init(char *model, uint64_t sn, uint64_t safecode)
{
	device_registers_t *Ponter = &can_node_rd.Iot[reals.Addr_can_self].factory;

    // 标定协议号
    Ponter->id.protocol_id = 0;

    // 产品支持
    Ponter->id.product_support.bits.bt = 1;
    Ponter->id.product_support.bits.wifi = 1;
#ifdef CONFIG_USB_4G_MODULE_ENABLE    
    Ponter->id.product_support.bits._4g = 1;
#endif
#ifdef CONFIG_FUNC_PAYGO_ENABLE
    Ponter->id.product_support.bits.paygo = 1;
#endif
#ifdef CONFIG_FUNC_MATTER_ENABLE
    Ponter->id.product_support.bits.matter = 1;
#endif

    // 型号
    memcpy(Ponter->id.model, model, TYPE_SIZE);

    // SN数字码
    Ponter->id.sn = sn;

    // IOT版本
    Ponter->fw.iot_version = IOT_VERSION;
    
    // 安全码
    Ponter->iot.security = safecode;

    // MAC
    esp_read_mac(Ponter->iot.mac, ESP_MAC_WIFI_STA);
}

/**
  * @brief      CAN标定4G相关信息更新
  * @param[in]  char *imei   
                char *imsi   
                char *iccid  
  * @param[out] None
  * @return     void
  */
void Can_Product_4G_Data_Update(char *imei, char *imsi, char *iccid)
{
	device_registers_t *Ponter = &can_node_rd.Iot[reals.Addr_can_self].factory;

    // IMEI
    memcpy(Ponter->cell.imei, imei, sizeof(Ponter->cell.imei));
    
    // IMSI
    memcpy(Ponter->cell.imsi, imsi, sizeof(Ponter->cell.imsi));
    
    // ICCID
    memcpy(Ponter->cell.iccid, iccid, sizeof(Ponter->cell.iccid));
}

/**
  * @brief      CAN标定数据写入回调
  * @param[in]  uint16_t write_offset  
                uint16_t data_len      
  * @param[out] None
  * @return     void
  */
void Can_iot_callback_set_type_0xFF(uint16_t write_offset, uint16_t data_len)
{
	device_registers_t *Ponter_Rd = &can_node_rd.Iot[reals.Addr_can_self].factory;
	device_registers_t *Ponter_Wr = &can_node_wr.Iot.factory;
	uint32_t address_begin = (uint32_t)Ponter_Wr + write_offset;
	uint32_t address_end = (uint32_t)Ponter_Wr + write_offset + data_len;
    size_t product_data_len = sizeof(iot_factory);
    bool id_change_flag = false;

    // 标定模式控制
    if((address_begin <= (uint32_t)&Ponter_Wr->preset.calibration_mode_ctrl)
        &&(address_end > (uint32_t)&Ponter_Wr->preset.calibration_mode_ctrl))
    {
        ESP_LOGW(TAG, "new calibration_mode_ctrl: %u, old: %u", Ponter_Wr->preset.calibration_mode_ctrl, Ponter_Rd->preset.calibration_mode_ctrl);   
        Ponter_Rd->preset.calibration_mode_ctrl = Ponter_Wr->preset.calibration_mode_ctrl;
    }   

    // 未进行过首次标定或主动进入标定模式
	if((strlen(Ponter_Rd->id.model) == 0)
        || ( Ponter_Rd->id.sn == IOT_FACTORY_DEFAULT_DEV_SN )
        || ( Ponter_Rd->iot.security == IOT_FACTORY_DEFAULT_SAFE_CODE )
        || ( 1 == Ponter_Rd->preset.calibration_mode_ctrl ))
    {
        // 型号
        if((address_begin <= (uint32_t)&Ponter_Wr->id.model)
            &&(address_end > (uint32_t)&Ponter_Wr->id.model))
        {
            if ((strlen(Ponter_Wr->id.model)) 
                && (strncmp(iot_factory.iot_type, Ponter_Wr->id.model, sizeof(Ponter_Wr->id.model))))
            {
    			ESP_LOGW(TAG, "new model: %s, old model: %s", Ponter_Wr->id.model, iot_factory.iot_type);       
    			memcpy(iot_factory.iot_type, Ponter_Wr->id.model, sizeof(iot_factory.iot_type));
                nvs_store_write(IOT_FACTORY, (uint8_t *)&iot_factory, product_data_len);
                nvs_store_read(IOT_FACTORY, (uint8_t *)&iot_factory, &product_data_len);
    			memcpy(Ponter_Rd->id.model, iot_factory.iot_type, sizeof(Ponter_Rd->id.model));
                id_change_flag = true;
            }
        }    

        // SN数字码
        if((address_begin <= (uint32_t)&Ponter_Wr->id.sn)
            &&(address_end > (uint32_t)&Ponter_Wr->id.sn))
        {
            if ((0 != Ponter_Wr->id.sn) 
                && (iot_factory.iot_sn != Ponter_Wr->id.sn))
            {
                ESP_LOGW(TAG, "new sn: %llu, old sn: %llu", Ponter_Wr->id.sn, iot_factory.iot_sn);       
                iot_factory.iot_sn = Ponter_Wr->id.sn;
                nvs_store_write(IOT_FACTORY, (uint8_t *)&iot_factory, product_data_len);
                nvs_store_read(IOT_FACTORY, (uint8_t *)&iot_factory, &product_data_len);
                Ponter_Rd->id.sn = iot_factory.iot_sn;
                id_change_flag = true;
            }
        }    

        // 安全码
        if((address_begin <= (uint32_t)&Ponter_Wr->iot.security)
            &&(address_end > (uint32_t)&Ponter_Wr->iot.security))
        {
            if ((0 != Ponter_Wr->iot.security) 
                && (iot_factory.safe_code != Ponter_Wr->iot.security))
            {
                ESP_LOGW(TAG, "new safe_code: %llu, old safe_code: %llu", Ponter_Wr->iot.security, iot_factory.safe_code);       
                iot_factory.safe_code = Ponter_Wr->iot.security;
                nvs_store_write(IOT_FACTORY, (uint8_t *)&iot_factory, product_data_len);
                nvs_store_read(IOT_FACTORY, (uint8_t *)&iot_factory, &product_data_len);
                Ponter_Rd->iot.security = iot_factory.safe_code;
                id_change_flag = true;
            }
        } 

        if ( id_change_flag )
        {
#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
            /*更新设备类型及SN（二代IOT直接使用自身）*/
            memcpy(dev_factory.dev_type, iot_factory.iot_type, TYPE_SIZE);
            dev_factory.dev_sn = iot_factory.iot_sn;

            /* 出厂信息发生变化,重新蓝牙广播 */
            iot_ble_new_name(dev_factory.dev_type, dev_factory.dev_sn);
            
            /* 出厂信息发生变化,重新订阅MQTT */
            iot_mqtt_new_dev(dev_factory.dev_type, dev_factory.dev_sn);

            /*更新WIFI AP名称*/
            char ssid[30] = {0};
            snprintf(ssid, sizeof(ssid), "%s%llu", dev_factory.dev_type, dev_factory.dev_sn);
            strncpy(IotSetData.dev_info_t.wifi_AP_ssid, ssid, sizeof(IotSetData.dev_info_t.wifi_AP_ssid));
            reals.SetDataWrFlag.sBit.wifi_ap_ssid = 1;
#endif            

            /* 出厂信息发生变化,重新登录MQTT */
            iot_mqtt_new_iot(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);  

            /*CAN标定后延迟10s登录，等待标定上位机同步标定结果至服务器，避免登陆过早被服务器拉黑*/
            iot_mqtt_wait(10000);

            /*更新到寄存器*/
            memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_type, iot_factory.iot_type, TYPE_SIZE);
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_sn = iot_factory.iot_sn;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.safe_code = iot_factory.safe_code;
        }            
    }

    // 重启指令
    if((address_begin <= (uint32_t)&Ponter_Wr->post_ctrl.reboot_cmd)
        &&(address_end > (uint32_t)&Ponter_Wr->post_ctrl.reboot_cmd))
    {
        if ( 0 != Ponter_Wr->post_ctrl.reboot_cmd ) {
            reals.IOT_Status_Flag.sBit.system_restart_flag = 1;
            ESP_LOGW(TAG, "Delay to restart...");       
        }
    }  
}


