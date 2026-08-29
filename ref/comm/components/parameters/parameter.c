#include "parameter.h"
#include "nvs_store.h"
#include "filesystem.h"
#include "utils.h"
#include "tou_relay_ctrl.h"
#include "can_protocol.h"
#include "bt_adv.h"
#include "can_product.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_wifi_types.h"
#include "stdint.h"
#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include "comm_define.h"

#define TAG "[Parameter]"

#define IOT1EPROM_READY_FLAG EPROM_READY_FLAG
#define IOT2EPROM_READY_FLAG EPROM_READY_FLAG
#define INV_EPROM_READY_FLAG EPROM_READY_FLAG
#define PACK_EPROM_READY_FLAG EPROM_READY_FLAG
#define TOURELAY_EPROM_READY_FLAG EPROM_READY_FLAG

USE_EXT_RAM_BSS SetData_TypeDef IotSetData = {0}; // IOT自身,初始值自身代码初始化
USE_EXT_RAM_BSS SetData_Can_TypeDef InvSetData = {0};//下级can INV数据备份，初始值必须来自下级CAN
/*本地存储TOU控制相关*/
USE_EXT_RAM_BSS Relay_SetData_TypeDef RelaySetData = {0};
USE_EXT_RAM_BSS disaster_save_param_t DisasterSaveData = {0};
USE_EXT_RAM_BSS iot_factory_struct iot_factory = {0};
USE_EXT_RAM_BSS dev_factory_struct dev_factory = {0};
USE_EXT_RAM_BSS RealS_STRUCT reals = {0};


extern uint32_t Get_IoT_Code_Build_U32(void);

/*------------------------------------------------------------------------------
 Function: parameter_iot_factory_read
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT出厂参数读取
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void parameter_iot_factory_read(void)
{
    size_t data_len;

    /*读取IOT工厂参数*/
    data_len = sizeof(iot_factory);
    if (nvs_store_read(IOT_FACTORY, (uint8_t *)&iot_factory, &data_len) != 0)
    {
        if (nvs_store_read(IOT_FACTORY, (uint8_t *)&iot_factory, &data_len) != 0)
        {
            ESP_LOGE(TAG, " IOT_FACTORY read error");
            memcpy(iot_factory.iot_type, IOT_FACTORY_DEFAULT_DEV_TYPE, sizeof(IOT_FACTORY_DEFAULT_DEV_TYPE));
            iot_factory.iot_sn = IOT_FACTORY_DEFAULT_DEV_SN;
            iot_factory.safe_code = IOT_FACTORY_DEFAULT_SAFE_CODE;
        }
    }

    /*未标定*/
    if ((strlen(iot_factory.iot_type) == 0) || (iot_factory.iot_sn == 0))
    {
        memcpy(iot_factory.iot_type, IOT_FACTORY_DEFAULT_DEV_TYPE, sizeof(IOT_FACTORY_DEFAULT_DEV_TYPE));
        iot_factory.iot_sn = IOT_FACTORY_DEFAULT_DEV_SN;
        iot_factory.safe_code = IOT_FACTORY_DEFAULT_SAFE_CODE;
    }

#ifdef CONFIG_CAN_FACTORY_WRITE_ENABLE
    // CAN标定数据初始化
    Can_Product_Data_Init(iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);
#endif
}

/*------------------------------------------------------------------------------
 Function: parameter_dev_factory_read
 -----------------------------------------------------------------------------*/
/**
  * @brief      DEV出厂参数读取
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void parameter_dev_factory_read(void)
{
    size_t data_len;

#ifdef CONFIG_IOT_2ND_GENERATION_MIX_ENABLE
    /*二代IOT标定参数即为设备参数*/
    memcpy(dev_factory.dev_type, iot_factory.iot_type, sizeof(dev_factory.dev_type));
    dev_factory.dev_sn = iot_factory.iot_sn;
#else
    /*读取INV工厂参数*/
    data_len = sizeof(dev_factory);
    if (nvs_store_read(DEV_FACTORY, (uint8_t *)&dev_factory, &data_len) != 0)
    {
        if (nvs_store_read(DEV_FACTORY, (uint8_t *)&dev_factory, &data_len) != 0)
        {
            ESP_LOGE(TAG, " DEV_FACTORY read error");
            memcpy(dev_factory.dev_type, MASS_PRODUCTION_DEFAULT_DEV_TYPE, sizeof(MASS_PRODUCTION_DEFAULT_DEV_TYPE));
            dev_factory.dev_sn = MASS_PRODUCTION_DEFAULT_DEV_SN;
        }
    }

    /*未标定*/
    if ((strlen(dev_factory.dev_type) == 0) || (dev_factory.dev_sn == 0))
    {
        memcpy(dev_factory.dev_type, MASS_PRODUCTION_DEFAULT_DEV_TYPE, sizeof(MASS_PRODUCTION_DEFAULT_DEV_TYPE));
        dev_factory.dev_sn = MASS_PRODUCTION_DEFAULT_DEV_SN;
    }
#endif    
}

/*------------------------------------------------------------------------------
 Function: Default_iot_base_cfg_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      基础设置初始化
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Default_iot_base_cfg_init(void)
{
#ifdef CONFIG_USE_SPI_ETHERNET_ENABLE
    IotSetData.dev_info_t.on_off.bit.Eth_enable = 1;
#else
    IotSetData.dev_info_t.on_off.bit.Eth_enable = 2;
#endif

    // 4G模块选配，默认关闭
    IotSetData.dev_info_t.on_off.bit._4g_enable = 2;

    IotSetData.dev_info_t.sta_enable.sta_passward_H32B_en = 0xA5;

    IotSetData.dev_info_t.support_mode.bit.visitor_mode = 2;//访客模式默认使能
    IotSetData.dev_info_t.support_mode.bit.ble_server_encrypt = 2;//默认蓝牙加密
    IotSetData.dev_info_t.thunder_ctrl.thunder_enable = 2;//风暴预警默认不使能

    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable = 1;         // MQTT1默认使能
    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable = 2; // MQTT1加密默认禁止
    IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update = 1;  // 同步服务器时间默认使能
}

/*------------------------------------------------------------------------------
 Function: Default_iot_ble_cfg_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      蓝牙相关设置初始化
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Default_iot_ble_cfg_init(void)
{
    IotSetData.dev_info_t.on_off.bit.ble_enable = 1;
    IotSetData.dev_info_t.on_off.bit.ble_adv_enable = 0;
    
    IotSetData.dev_info_t.ble_protocol.adv_en = 1;
    ble_adv_key_update(2); // 模式2：恢复出厂默认密钥
    IotSetData.dev_info_t.ble_protocol.general_adv_en = 1;
    IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en = 1;
    IotSetData.dev_info_t.ble_protocol.modbus_adv_en = 2;
}

/*------------------------------------------------------------------------------
 Function: Default_iot_wifi_cfg_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI相关设置初始化
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Default_iot_wifi_cfg_init(void)
{
    IotSetData.dev_info_t.on_off.bit.wifi_enable = 1;
    IotSetData.dev_info_t.on_off.bit.wifi_sta_enable = 1;
    IotSetData.dev_info_t.on_off.bit.wifi_ap_enable = 2;

    memcpy(IotSetData.dev_info_t.could_dns, BLUETTI_DNS, sizeof(BLUETTI_DNS));

    // 可配置的 WIFI信号强度、间隔尝试延时（默认-80dbm,60s）
    IotSetData.dev_info_t.wifi_sta_rssi_th = -80;
    IotSetData.dev_info_t.wifi_sta_switch_int = 60;
    
    IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en = 1;
    IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en = 2;
    IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en = 2;
    
    IotSetData.dev_info_t.wifi_sta_auth = WIFI_AUTH_WPA_WPA2_PSK;
    memcpy(IotSetData.dev_info_t.wifi_sta_ssid, DEF_WIFI_STA1_SSID, sizeof(DEF_WIFI_STA1_SSID));
    memcpy(IotSetData.dev_info_t.wifi_sta_password, DEF_WIFI_STA1_PASSWD, sizeof(DEF_WIFI_STA1_PASSWD));

    IotSetData.dev_info_t.wifi_sta2_auth = WIFI_AUTH_WPA_WPA2_PSK;
    memcpy(IotSetData.dev_info_t.wifi_sta2_ssid, DEF_WIFI_STA2_SSID, sizeof(DEF_WIFI_STA2_SSID));
    memcpy(IotSetData.dev_info_t.wifi_sta2_password, DEF_WIFI_STA2_PASSWD, sizeof(DEF_WIFI_STA2_PASSWD));

    IotSetData.dev_info_t.wifi_sta3_auth = WIFI_AUTH_WPA_WPA2_PSK;
    memcpy(IotSetData.dev_info_t.wifi_sta3_ssid, DEF_WIFI_STA3_SSID, sizeof(DEF_WIFI_STA3_SSID));
    memcpy(IotSetData.dev_info_t.wifi_sta3_password, DEF_WIFI_STA3_PASSWD, sizeof(DEF_WIFI_STA3_PASSWD));

    char wifi_ap_ssid[32] = {0};
    snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid), "%s%llu", dev_factory.dev_type, (uint64_t)dev_factory.dev_sn);
    IotSetData.dev_info_t.wifi_ap_auth = WIFI_AUTH_WPA_WPA2_PSK;
    memcpy(IotSetData.dev_info_t.wifi_AP_ssid, wifi_ap_ssid, sizeof(wifi_ap_ssid));
    memcpy(IotSetData.dev_info_t.wifi_AP_password, DEF_WIFI_AP_PASSWD, sizeof(DEF_WIFI_AP_PASSWD));

    // 默认禁止静态IP
    IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en = 2;
    IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en = 2;
    IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en = 2;
    
    // STA1
    IotSetData.dev_info_t.wifi_sta1_ip = 0;
    IotSetData.dev_info_t.wifi_sta1_mask = 0;
    IotSetData.dev_info_t.wifi_sta1_gw = 0;
    IotSetData.dev_info_t.wifi_sta1_dns1 = 0;
    IotSetData.dev_info_t.wifi_sta1_dns2 = 0;
    
    // STA2
    IotSetData.dev_info_t.wifi_sta2_ip = 0;
    IotSetData.dev_info_t.wifi_sta2_mask = 0;
    IotSetData.dev_info_t.wifi_sta2_gw = 0;
    IotSetData.dev_info_t.wifi_sta2_dns1 = 0;
    IotSetData.dev_info_t.wifi_sta2_dns2 = 0;
    
    // STA3
    IotSetData.dev_info_t.wifi_sta3_ip = 0;
    IotSetData.dev_info_t.wifi_sta3_mask = 0;
    IotSetData.dev_info_t.wifi_sta3_gw = 0;
    IotSetData.dev_info_t.wifi_sta3_dns1 = 0;
    IotSetData.dev_info_t.wifi_sta3_dns2 = 0;
}

/*------------------------------------------------------------------------------
 Function: Default_iot_other_cfg_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      其他设置初始化
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Default_iot_other_cfg_init(void)
{
    IotSetData.dev_info_t.data_report_cycle = 60*60;     // 默认1h
    
    IotSetData.dev_info_t.Time_Span.power_off_count = 3; // 默认展示每月断电次数
    IotSetData.dev_info_t.Time_Span.save_money = 4;      // 默认展示每年省钱金额
    
    IotSetData.dev_info_t.open_mqtt_enable.enable = 2;  // 默认关闭
    IotSetData.dev_info_t.open_mqtt_enable.crypt_en = 2;
    IotSetData.dev_info_t.open_mqtt_enable.ctrl_en = 2;
    IotSetData.dev_info_t.open_mqtt_enable.report_en = 2;
    IotSetData.dev_info_t.open_mqtt_report_cycle = 60;
    
    IotSetData.dev_info_t.modbus_tcp_enable.enable = 2;
    IotSetData.dev_info_t.modbus_tcp_enable.crypt_en = 2;
    IotSetData.dev_info_t.modbus_tcp_port = 502;
    
	memcpy(IotSetData.dev_info_t.Net_Server_address, MQTT_SERVER_ADDRESS_DEFAULT, sizeof(MQTT_SERVER_ADDRESS_DEFAULT));
	memcpy(IotSetData.dev_info_t.Net_Server_secret, MQTT_SERVER_PASSWD_DEFAULT, sizeof(MQTT_SERVER_PASSWD_DEFAULT));

#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5
    IotSetData.dev_info_t.Remote_set.Remote_set_enable = 2;     // 默认禁止
    IotSetData.dev_info_t.Remote_set.Remote_set_valid = 1;      // 系统支持
    IotSetData.dev_info_t.Remote_set.Automatic_sleep_days = 0;  // 自动休眠预留
    IotSetData.dev_info_t.Remote_set_soc_limit = 20;             // 休眠阈值SOC默认为20
#endif

#ifdef CONFIG_IOT_LED_SET_ENABLE
    // IOT端LED控制
    IotSetData.dev_info_t.LED_SET.bit.color_set = 0;            // 1:暖色；2：冷色；(0无效)          
    IotSetData.dev_info_t.LED_SET.bit.brightness_set = 101;     // LED亮度 （0无效 1~101表示0-100）         
#endif

#ifdef CONFIG_SMART_GENERATOR_ENABLE
    // 油机智能控制
    IotSetData.dev_info_t.Gen_ctrl.soc_min = 20;
    IotSetData.dev_info_t.Gen_ctrl.soc_max = 80;
    IotSetData.dev_info_t.Gen_ctrl.ctrl_mode = 3;   // 0：无效；1：智能混动；2：定时；3：手动
    IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl = 2;
#endif
}

/*------------------------------------------------------------------------------
 Function: parameter_iot_default
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT基本配置参数初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void parameter_iot_default(void)
{
    memset(&IotSetData, 0, sizeof(IotSetData));
    IotSetData.dev_info_t.valid_iot = IOT1EPROM_READY_FLAG;
    IotSetData.dev_info_t.valid_other = IOT2EPROM_READY_FLAG;

    /*基础设置初始化*/
    Default_iot_base_cfg_init();

    /*蓝牙相关设置初始化*/
    Default_iot_ble_cfg_init();

    /*WIFI相关设置初始化*/
    Default_iot_wifi_cfg_init();

    /*其他设置初始化*/
    Default_iot_other_cfg_init();    
}

/*------------------------------------------------------------------------------
 Function: Initdata_Special_Ble_Adv
 -----------------------------------------------------------------------------*/
/**
  * @brief      蓝牙广播数据相关参数初始化
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Initdata_Special_Ble_Adv(void)
{
    /*BLE开放使能无效，默认为1*/
    if ( IotSetData.dev_info_t.ble_protocol.adv_en == 0 ) {
        ESP_LOGW(TAG, "[InitData] Ble_Adv is invalid, init!");
        
        IotSetData.dev_info_t.ble_protocol.adv_en = 1;
        ble_adv_key_update(2); // 模式2：恢复出厂默认密钥

        // 更新本地参数，部分初始化
        reals.SetDataWrFlag.sBit.SetDataUpdate_all = 1;
    }
    
    /* 通用广播使能（record type为0x00~0x7F）, 默认使能 */
    if ( IotSetData.dev_info_t.ble_protocol.general_adv_en == 0 ) IotSetData.dev_info_t.ble_protocol.general_adv_en = 1;

    /* LCD数据广播使能(record type为0x80/0x81) ，默认使能*/
    if ( IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en == 0 ) IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en = 1;

    /*断电次数时间跨度无效，默认每月*/
    if ( IotSetData.dev_info_t.Time_Span.power_off_count == 0 ) IotSetData.dev_info_t.Time_Span.power_off_count = 3;

    /*省钱参数时间跨度无效，默认每年*/
    if ( IotSetData.dev_info_t.Time_Span.save_money == 0 ) IotSetData.dev_info_t.Time_Span.save_money = 4;
}

/*------------------------------------------------------------------------------
 Function: Initdata_Special_Ext_WIFI_STA
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI STA 1~3数据相关参数初始化
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Initdata_Special_Ext_WIFI_STA(void)
{
    // WIFI STA 额外配置无效，执行部分初始化
    if ( IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en == 0 )
    {
        ESP_LOGW(TAG, "[InitData] Ext_WIFI_STA is invalid, init!");
        
        // 可配置的 WIFI信号强度、间隔尝试延时（默认-80dbm,60s）
        IotSetData.dev_info_t.wifi_sta_rssi_th = -80;
        IotSetData.dev_info_t.wifi_sta_switch_int = 60;

        // 默认STA1使能（不允许禁止），STA2和STA3默认禁止
        IotSetData.dev_info_t.wifi_mul_sta_en.sta1_en = 1;
        IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en = 2;
        IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en = 2;

#if 0   //保留原设置
        // STA1 设置
        IotSetData.dev_info_t.wifi_sta_auth = WIFI_AUTH_WPA_WPA2_PSK;
        memcpy(IotSetData.dev_info_t.wifi_sta_ssid, DEF_WIFI_STA1_SSID, sizeof(DEF_WIFI_STA1_SSID));
        memcpy(IotSetData.dev_info_t.wifi_sta_password, DEF_WIFI_STA1_PASSWD, sizeof(DEF_WIFI_STA1_PASSWD));
#endif

        // STA2 设置
        IotSetData.dev_info_t.wifi_sta2_auth = WIFI_AUTH_WPA_WPA2_PSK;
        memcpy(IotSetData.dev_info_t.wifi_sta2_ssid, DEF_WIFI_STA2_SSID, sizeof(DEF_WIFI_STA2_SSID));
        memcpy(IotSetData.dev_info_t.wifi_sta2_password, DEF_WIFI_STA2_PASSWD, sizeof(DEF_WIFI_STA2_PASSWD));
        
        // STA3 设置
        IotSetData.dev_info_t.wifi_sta3_auth = WIFI_AUTH_WPA_WPA2_PSK;
        memcpy(IotSetData.dev_info_t.wifi_sta3_ssid, DEF_WIFI_STA3_SSID, sizeof(DEF_WIFI_STA3_SSID));
        memcpy(IotSetData.dev_info_t.wifi_sta3_password, DEF_WIFI_STA3_PASSWD, sizeof(DEF_WIFI_STA3_PASSWD));

        // 更新本地参数，部分初始化
        reals.SetDataWrFlag.sBit.SetDataUpdate_all = 1;
    }
}

/*------------------------------------------------------------------------------
 Function: Initdata_Special_WIFI_STA_Disable_DHCP
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI STA静态IP设置
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Initdata_Special_WIFI_STA_Disable_DHCP(void)
{
    // WIFI STA 静态IP设置无效，执行部分初始化
    if ( IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en == 0 )
    {
        ESP_LOGW(TAG, "[InitData] WIFI_STA_Disable_DHCP is invalid, init!");

        // 默认禁止静态IP
        IotSetData.dev_info_t.wifi_mul_sta_en.sta1_static_ip_en = 2;
        IotSetData.dev_info_t.wifi_mul_sta_en.sta2_static_ip_en = 2;
        IotSetData.dev_info_t.wifi_mul_sta_en.sta3_static_ip_en = 2;

        // STA1
        IotSetData.dev_info_t.wifi_sta1_ip = 0;
        IotSetData.dev_info_t.wifi_sta1_mask = 0;
        IotSetData.dev_info_t.wifi_sta1_gw = 0;
        IotSetData.dev_info_t.wifi_sta1_dns1 = 0;
        IotSetData.dev_info_t.wifi_sta1_dns2 = 0;

        // STA2
        IotSetData.dev_info_t.wifi_sta2_ip = 0;
        IotSetData.dev_info_t.wifi_sta2_mask = 0;
        IotSetData.dev_info_t.wifi_sta2_gw = 0;
        IotSetData.dev_info_t.wifi_sta2_dns1 = 0;
        IotSetData.dev_info_t.wifi_sta2_dns2 = 0;

        // STA3
        IotSetData.dev_info_t.wifi_sta3_ip = 0;
        IotSetData.dev_info_t.wifi_sta3_mask = 0;
        IotSetData.dev_info_t.wifi_sta3_gw = 0;
        IotSetData.dev_info_t.wifi_sta3_dns1 = 0;
        IotSetData.dev_info_t.wifi_sta3_dns2 = 0;

        // 更新本地参数，部分初始化
        reals.SetDataWrFlag.sBit.SetDataUpdate_all = 1;
    }
}


/*------------------------------------------------------------------------------
 Function: Initdata_Special_WIFI_AP
 -----------------------------------------------------------------------------*/
/**
  * @brief      WIFI AP 初始化检查
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Initdata_Special_WIFI_AP(void)
{
    // WIFI AP 配置无效，执行部分初始化
    if ( IotSetData.dev_info_t.on_off.bit.wifi_ap_enable == 0 )
    {
        ESP_LOGW(TAG, "[InitData] WIFI AP is invalid, init!");

        // 默认禁止WIFI AP模式
        IotSetData.dev_info_t.on_off.bit.wifi_ap_enable = 2;
        
        char wifi_ap_ssid[32] = {0};
        snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid), "%s%llu", dev_factory.dev_type, dev_factory.dev_sn);
        IotSetData.dev_info_t.wifi_ap_auth = WIFI_AUTH_WPA2_PSK;
        memcpy(IotSetData.dev_info_t.wifi_AP_ssid, wifi_ap_ssid, sizeof(wifi_ap_ssid));
        memcpy(IotSetData.dev_info_t.wifi_AP_password, DEF_WIFI_AP_PASSWD, sizeof(DEF_WIFI_AP_PASSWD));

        // 更新本地参数，部分初始化
        reals.SetDataWrFlag.sBit.SetDataUpdate_all = 1;
    }
}

/*------------------------------------------------------------------------------
 Function: Initdata_Special_Debug
 -----------------------------------------------------------------------------*/
/**
  * @brief      调试设置
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Initdata_Special_Debug(void)
{
#if 0
//    // debug
//    if ( dev_factory.dev_sn == MASS_PRODUCTION_DEFAULT_DEV_SN )
//    {
//        memcpy(dev_factory.dev_type, "EL200mini", sizeof("EL200mini"));
//        dev_factory.dev_sn = 123456789;
//    }
//    memcpy(dev_factory.dev_type, "EL300", sizeof("EL300"));
//    dev_factory.dev_sn = 2527000001201;
//    memcpy(dev_factory.dev_type, "Balco500", sizeof("Balco500"));
//    dev_factory.dev_sn = 2602000001609;    
//    memcpy(iot_factory.iot_type, "Balco500", sizeof("Balco500"));
//    iot_factory.iot_sn = 2602000001609;  
//    memcpy(iot_factory.iot_type, "POWER5", sizeof("POWER5"));
//    iot_factory.iot_sn = 2552000003642;
//    iot_factory.safe_code = 484060352;
//    IotSetData.dev_info_t.on_off.bit._4g_enable = 1;
//    nvs_store_write(IOT_FACTORY, (uint8_t *)&iot_factory, sizeof(iot_factory));
//    IotSetData.dev_info_t.ble_protocol.adv_en = 1;
//    IotSetData.dev_info_t.ble_protocol.general_adv_en = 1;
//    IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en = 1;
//    memcpy(dev_factory.dev_type, "EL200mini", sizeof("EL200mini"));
//    dev_factory.dev_sn = 2605000001678; 
//    nvs_store_write(DEV_FACTORY, (uint8_t *)&dev_factory, sizeof(dev_factory));
#endif
}

/*------------------------------------------------------------------------------
 Function: Parameter_IoT_Special_Process
 -----------------------------------------------------------------------------*/
/**
  * @brief      上电初始化信息特殊处理
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void Parameter_IoT_Special_Process(void)
{
    /*软件版本定义*/
    IotSetData.dev_info_t.software_ver = IOT_VERSION;

    /*协议版本定义*/
    IotSetData.dev_info_t.protocol_ver = MODBUS_VERSION;

    /*蓝牙密码支持*/
    IotSetData.dev_info_t.support_mode.bit.support_ble_pwd = 1;

    /*访客模式无效，打开默认使能（避免旧版本使能为0，不支持）*/
    if (( IotSetData.dev_info_t.support_mode.bit.visitor_mode == 0 ) || ( IotSetData.dev_info_t.support_mode.bit.visitor_mode > 2 )) {
        IotSetData.dev_info_t.support_mode.bit.visitor_mode = 2;
    }

    /*蓝牙加密模式无效，当前版本默认加密*/
    if ( IotSetData.dev_info_t.support_mode.bit.ble_server_encrypt == 0 ) {
        IotSetData.dev_info_t.support_mode.bit.ble_server_encrypt = 2;
    }

    /*WIFI STA开关*/
    if ( 0 == IotSetData.dev_info_t.on_off.bit.wifi_sta_enable ) {
        IotSetData.dev_info_t.on_off.bit.wifi_sta_enable = 1;
    }

    /*磁吸屏温度单位*/
    if ( 0 == IotSetData.dev_info_t.lcd_mode.temperature_unit ) {
        IotSetData.dev_info_t.lcd_mode.temperature_unit = 2; // 默认温度单位为F
    }

    /*磁吸屏休眠时间*/
    if ( 0 == IotSetData.dev_info_t.lcd_mode.lcd_active_time ) {
        IotSetData.dev_info_t.lcd_mode.lcd_active_time = 6; // 默认跟随逆变器
    }

#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5
    /*远程休眠参数*/
    if ( 0 == IotSetData.dev_info_t.Remote_set.Remote_set_enable ) {
        IotSetData.dev_info_t.Remote_set.Remote_set_enable = 2;     // 默认禁止
        IotSetData.dev_info_t.Remote_set.Remote_set_valid = 1;      // 系统支持
        IotSetData.dev_info_t.Remote_set_soc_limit = 5;             // 休眠阈值SOC默认为5
    }
#endif

#ifdef CONFIG_IOT_LED_SET_ENABLE
    /*LED亮度*/
    if ( 0 == IotSetData.dev_info_t.LED_SET.bit.brightness_set ) {
        IotSetData.dev_info_t.LED_SET.bit.brightness_set = 101;     // LED亮度 （0无效 1~101表示0-100） 
    }
#endif

#ifdef CONFIG_SMART_GENERATOR_ENABLE
    // 油机智能控制
    if ( 0 == IotSetData.dev_info_t.Gen_ctrl.ctrl_mode ) {
        IotSetData.dev_info_t.Gen_ctrl.soc_min = 20;
        IotSetData.dev_info_t.Gen_ctrl.soc_max = 80;
        IotSetData.dev_info_t.Gen_ctrl.ctrl_mode = 3;   // 0：无效；1：智能混动；2：定时；3：手动
        IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl = 2;
    }
#endif

    /*MQTT1使能参数*/
    if ( 0 == IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable ) {
        IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable = 1;         // MQTT1默认使能
        IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable = 2; // MQTT1加密默认禁止
        IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update = 1;  // 同步服务器时间默认使能
    }

    /*Modbus TCP相关参数*/
    if ( 0 == IotSetData.dev_info_t.modbus_tcp_enable.enable ) {
        IotSetData.dev_info_t.modbus_tcp_enable.enable = 2;
        IotSetData.dev_info_t.modbus_tcp_enable.crypt_en = 2;
        IotSetData.dev_info_t.modbus_tcp_port = 502;
    }

    /*蓝牙广播数据相关参数初始化*/
    Initdata_Special_Ble_Adv();

    /*WIFI STA 1~3数据相关参数初始化*/
    Initdata_Special_Ext_WIFI_STA();

    /*STA静态IP设置初始化*/
    Initdata_Special_WIFI_STA_Disable_DHCP();

    /*WIFI AP 初始化检查*/
    Initdata_Special_WIFI_AP();

#ifndef MASS_PRODUCTION_CONFIG_ENABLE
    /*调试设置*/
    Initdata_Special_Debug();
#endif
}

/*------------------------------------------------------------------------------
 Function: parameter_iot_read
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT本地配置参数读取
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void parameter_iot_read(void)
{
    int ret = fs_file_read(PARAMETER_FILE_PATH_IOT(0, 00), (uint8_t *)&IotSetData, sizeof(IotSetData));
    if (ret != 0 || IOT1EPROM_READY_FLAG != IotSetData.dev_info_t.valid_iot)
    {
        ESP_LOGE(TAG, "parameter_iot_read error, flag:0x%X, ret:%d", IotSetData.dev_info_t.valid_iot, ret);

        // 研发-生产版本切换时保留部分参数
        if (( DEV_SET_DATA_VALID_FLAG == IotSetData.dev_info_t.valid_iot ) 
            || ( MASS_PRODUCTION_SET_DATA_VALID_FLAG == IotSetData.dev_info_t.valid_iot ))
        {
            uint32_t historyRecSaveCount = IotSetData.dev_info_t.historyRecSaveCount;
            uint32_t historyAddrIndex = IotSetData.dev_info_t.historyAddrIndex;
            uint32_t Event_SaveCount = IotSetData.dev_info_t.Event_SaveCount;
            uint32_t Event_AddrIndex = IotSetData.dev_info_t.Event_AddrIndex;
            uint16_t invDetailedInfo_AddrIndex = IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0];
            uint16_t invDetailedInfo_SaveCount = IotSetData.dev_info_t.invDetailedInfo_SaveCount[0];
            uint16_t SetTimeZone = IotSetData.dev_info_t.SetTimeZone.all;
            
            // 初始化默认值
            parameter_iot_default();

            IotSetData.dev_info_t.historyRecSaveCount = historyRecSaveCount;
            IotSetData.dev_info_t.historyAddrIndex = historyAddrIndex;
            IotSetData.dev_info_t.Event_SaveCount = Event_SaveCount;
            IotSetData.dev_info_t.Event_AddrIndex = Event_AddrIndex;
            IotSetData.dev_info_t.invDetailedInfo_AddrIndex[0] = invDetailedInfo_AddrIndex;
            IotSetData.dev_info_t.invDetailedInfo_SaveCount[0] = invDetailedInfo_SaveCount;
            IotSetData.dev_info_t.SetTimeZone.all = SetTimeZone;
        }
        else
        {
            // 初始化默认值
            parameter_iot_default();
        }
        
        // 写入默认值
        if (0 != fs_file_write(PARAMETER_FILE_PATH_IOT(0, 00), (uint8_t *)&IotSetData, 0, sizeof(IotSetData))) {
            ESP_LOGE(TAG, "parameter_iot_write error");
        }
    }

    /*上电初始化信息特殊处理*/
    Parameter_IoT_Special_Process();

    ESP_LOGI(TAG, "****************************DATA_INIT_FROM_LITTLEFS_DEBUG_LOG*********************************");
    ESP_LOGI(TAG, "[InitData] iot_factory: %s%013llu, safety Code: %lld", iot_factory.iot_type, iot_factory.iot_sn, iot_factory.safe_code);
    ESP_LOGI(TAG, "[InitData] inv_factory: %s%013llu", dev_factory.dev_type, dev_factory.dev_sn);
    ESP_LOGI(TAG, "[InitData] software version: %d, iot_protocol_verion: %d", IOT_VERSION, IOT_PROTOCOL_VERSION);
    ESP_LOGI(TAG, "[InitData] build_date_code: %lu", Get_IoT_Code_Build_U32());
    ESP_LOGI(TAG, "[InitData] server address: %s", IotSetData.dev_info_t.could_dns);
    ESP_LOGI(TAG, "[InitData] Modem APN: %s", IotSetData.dev_info_t.mobile_apn);
    ESP_LOGI(TAG, "[InitData] protocol_ver: %d", IotSetData.dev_info_t.protocol_ver);
    ESP_LOGI(TAG, "[InitData] IOT_OnOff_Set: %ld", IotSetData.dev_info_t.on_off.all);
    ESP_LOGI(TAG, "[InitData] Wi-Fi STA1: Enable: %d, SSID: %s, Password: %s, Wifi_Auth_Mode: %d", IotSetData.dev_info_t.on_off.bit.wifi_sta_enable, 
                                       IotSetData.dev_info_t.wifi_sta_ssid, IotSetData.dev_info_t.wifi_sta_password, IotSetData.dev_info_t.wifi_sta_auth);
    ESP_LOGI(TAG, "[InitData] Wi-Fi STA2: Enable: %d, SSID: %s, Password: %s, Wifi_Auth_Mode: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta2_en, 
                                       IotSetData.dev_info_t.wifi_sta2_ssid, IotSetData.dev_info_t.wifi_sta2_password, IotSetData.dev_info_t.wifi_sta2_auth);
    ESP_LOGI(TAG, "[InitData] Wi-Fi STA3: Enable: %d, SSID: %s, Password: %s, Wifi_Auth_Mode: %d", IotSetData.dev_info_t.wifi_mul_sta_en.sta3_en, 
                                       IotSetData.dev_info_t.wifi_sta3_ssid, IotSetData.dev_info_t.wifi_sta3_password, IotSetData.dev_info_t.wifi_sta3_auth);
    ESP_LOGI(TAG, "[InitData] WiFi Other: WIFI Enable(%d), wifi_sta_rssi_threshold: %d dbm, wifi_sta_switch_interval: %d s", IotSetData.dev_info_t.on_off.bit.wifi_enable,
                                       IotSetData.dev_info_t.wifi_sta_rssi_th, IotSetData.dev_info_t.wifi_sta_switch_int);
    ESP_LOGI(TAG, "[InitData] Wi-Fi AP  : Enable: %d, SSID: %s, Password: %s, Wifi_Auth_Mode: %d", IotSetData.dev_info_t.on_off.bit.wifi_ap_enable,
                                       IotSetData.dev_info_t.wifi_AP_ssid, IotSetData.dev_info_t.wifi_AP_password, IotSetData.dev_info_t.wifi_ap_auth);
    uint8_t *pw = (uint8_t *)IotSetData.dev_info_t.app_password;
    ESP_LOGI(TAG, "[InitData] App BLE   : Enable: %d, Password: %02x %02x %02x %02x %02x %02x", IotSetData.dev_info_t.on_off.bit.ble_enable, pw[0], pw[1], pw[2], pw[3], pw[4], pw[5]);
    ESP_LOGI(TAG, "[InitData] Net_Server_address: %s",IotSetData.dev_info_t.Net_Server_address);
    ESP_LOGI(TAG, "[InitData] Net_Server_secret: %s",IotSetData.dev_info_t.Net_Server_secret);
    ESP_LOGI(TAG, "[InitData] Thunder Mode : enable(%d), status(%d)", IotSetData.dev_info_t.thunder_ctrl.thunder_enable, IotSetData.dev_info_t.thunder_ctrl.thunder_state);
    ESP_LOGI(TAG, "[InitData] Support_mode : visitor_mode(%d), ble_server_encrypt(%d)", IotSetData.dev_info_t.support_mode.bit.visitor_mode, IotSetData.dev_info_t.support_mode.bit.ble_server_encrypt);
    ESP_LOGI(TAG, "[InitData] Ble_Adv: adv_en(%d), general_adv_en(%d), lcd_data_adv_en(%d), modbus_adv_en(%d)", IotSetData.dev_info_t.ble_protocol.adv_en, 
                                       IotSetData.dev_info_t.ble_protocol.general_adv_en, IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en, IotSetData.dev_info_t.ble_protocol.modbus_adv_en);
    ESP_LOGI(TAG, "[InitData] MQTT1: Enable(%d), Encrypt(%d), TimeUpdate(%d)",
                                       IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_enable, IotSetData.dev_info_t.Protocol_3r_Enable_mix1.mqtt1_encrypt_enable, 
                                       IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update);
    ESP_LOGI(TAG, "[InitData] ModbusTCP: Enable(%d), Encrypt(%d), Port(%d)",
                                       IotSetData.dev_info_t.modbus_tcp_enable.enable, IotSetData.dev_info_t.modbus_tcp_enable.crypt_en, 
                                       IotSetData.dev_info_t.modbus_tcp_port);
    ESP_LOGI(TAG, "[InitData] Other: temp(%d), Time_Span_Grid_Poweroff(%d), Time_Span_money_save(%d), lcd_active_time(%d)",
                                       IotSetData.dev_info_t.lcd_mode.temperature_unit, IotSetData.dev_info_t.Time_Span.power_off_count, IotSetData.dev_info_t.Time_Span.save_money, 
                                       IotSetData.dev_info_t.lcd_mode.lcd_active_time);
    ESP_LOGI(TAG, "**********************************************************************************************");
}

void parameter_inv_default(void)
{
    memset((uint8_t *)&InvSetData, 0, sizeof(InvSetData));
    InvSetData.inv_can_info_t.valid_inv = INV_EPROM_READY_FLAG;
}

void parameter_inv_read(void)
{
    int ret = fs_file_read(PARAMETER_FILE_PATH_INV(0, 00), (uint8_t *)&InvSetData, sizeof(InvSetData));
    if (ret != 0 || INV_EPROM_READY_FLAG != InvSetData.inv_can_info_t.valid_inv)
    {
        ESP_LOGE(TAG, "parameter_inv_read error, flag:%d", InvSetData.inv_can_info_t.valid_inv);
        // 初始化默认值
        parameter_inv_default();
        // 写入默认值
        fs_file_write(PARAMETER_FILE_PATH_INV(0, 00), (uint8_t *)&InvSetData, 0, sizeof(InvSetData));
    }
}

void parameter_disaster_read(void)
{
    int ret = fs_file_read(PARAMETER_FILE_PATH_DISASTER(0, 00), (uint8_t *)&DisasterSaveData, sizeof(DisasterSaveData));
    if (ret != 0)
    {
        ESP_LOGE(TAG, "parameter_disaster_read error, ret:%d", ret);
        // 初始化默认值
        memset(&DisasterSaveData, 0, sizeof(DisasterSaveData));
        // 写入默认值
        fs_file_write(PARAMETER_FILE_PATH_DISASTER(0, 00), (uint8_t *)&DisasterSaveData, 0, sizeof(DisasterSaveData));
    }
}

static void parameter_relay_default(void)
{
    memset(&RelaySetData, 0, sizeof(RelaySetData));
    RelaySetData.Relay_info_t.valid_Relay = TOURELAY_EPROM_READY_FLAG;

    Relay_Ctrl_Factory_Parameter01_Update();
    Relay_Ctrl_Factory_Parameter02_Update();
    Relay_Ctrl_Factory_Parameter03_Update();
    Relay_Ctrl_Factory_Parameter04_Update();
}

/**
 * @brief  读取继电器参数
 */
void parameter_relay_read(void)
{
    int ret = fs_file_read(PARAMETER_FILE_PATH_RELAY(0, 00), (uint8_t *)&RelaySetData, sizeof(RelaySetData));
    if (ret != 0 || INV_EPROM_READY_FLAG != RelaySetData.Relay_info_t.valid_Relay)
    {
        ESP_LOGE(TAG, "parameter_relay_read error, flag:%d", RelaySetData.Relay_info_t.valid_Relay);
        // 初始化默认值
        parameter_relay_default();
        // 写入默认值
        ret = fs_file_write(PARAMETER_FILE_PATH_RELAY(0, 00), (uint8_t *)&RelaySetData, 0, sizeof(RelaySetData));
        if (ret == 0)
        {
            ESP_LOGI(TAG, "parameter_iot_write success");
            // 初始化完成置位 TODO: 
            Relay_ctrl.bit.soc_ctrl_init = 1;
            Relay_ctrl.bit.time_ctrl_init = 1;
            Relay_ctrl.bit.delay_ctrl_init = 1;
            Relay_ctrl.bit.plan_ctrl_init = 1;
        }
        else
        {
            ESP_LOGE(TAG, "parameter_iot_write fail");
            //TODO: 
            Relay_ctrl.bit.soc_ctrl_init = 0;
            Relay_ctrl.bit.time_ctrl_init = 0;
            Relay_ctrl.bit.delay_ctrl_init = 0;
            Relay_ctrl.bit.plan_ctrl_init = 0;
        }
    }
    else
    {
        Relay_ctrl.bit.soc_ctrl_init = 1;
        Relay_ctrl.bit.time_ctrl_init = 1;
        Relay_ctrl.bit.delay_ctrl_init = 1;
        Relay_ctrl.bit.plan_ctrl_init = 1;
    }
}

/*------------------------------------------------------------------------------
 Function: parameter_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      系统参数初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void parameter_init(void)
{
    fs_file_check();                // 文件系统检查

    parameter_iot_factory_read();   // 读取IOT标定参数
    parameter_dev_factory_read();   // 读取DEV标定参数
    parameter_iot_read();           // 读取IOT参数
    parameter_inv_read();           // 读取下级CAN INV参数
    
#ifdef CONFIG_RELAY_CTRL_ENABLE    
    parameter_relay_read();         // 读取继电器参数
#endif    

#if CONFIG_DISASTER_WARN_CHANNEL > 0
    parameter_disaster_read();      // 读取灾害预警参数
#endif    
}

int parameter_save(uint16_t type)
{
    uint32_t file_bias = 0;
    uint32_t len = 0;
    int ret = 0;

    /* IOT参数 */
    if (((reals.SetDataWrFlag.Byte8[0] != 0) || (reals.SetDataWrFlag.Byte8[1] != 0)) && (type & PARAM_TYPE_IOT))
    {
        // 写入默认值
        if (0 == fs_file_write(PARAMETER_FILE_PATH_IOT(0, 00), (uint8_t *)&IotSetData, 0, sizeof(IotSetData)))
        {
            memset(reals.SetDataWrFlag.Byte8, 0, sizeof(reals.SetDataWrFlag.Byte8));
            ESP_LOGW(TAG, "Save IoT parameter success");
        }
        else
        {
            ESP_LOGE(TAG, "Save IoT parameter failure");
            ret = -1;
        }
    }

    /* 逆变参数 */
    if (0 != reals.INVFlasWrFlag.Byte2 && (type & PARAM_TYPE_INV))
    {
        file_bias = offsetof(SetData_Can_TypeDef, inv_can_info_t.inv_set00);
        len = offsetof(SetData_Can_TypeDef, inv_can_info_t.pack_config) - offsetof(SetData_Can_TypeDef, inv_can_info_t.inv_set00);

        if (0 == fs_file_write(PARAMETER_FILE_PATH_INV(0, 00), (uint8_t *)&InvSetData.inv_can_info_t.inv_set00, file_bias, len)
            && 0 == fs_file_write(PARAMETER_FILE_PATH_INV(0, 00), (uint8_t *)&InvSetData.inv_can_info_t.valid_inv, 0, 2))
        {
            reals.INVFlasWrFlag.Byte2 = 0;
            ESP_LOGW(TAG, "Save inv parameter[%lu:%lu] success", file_bias, len);
        }
        else
        {
            ESP_LOGE(TAG, "Save inv parameter[%lu:%lu] fail", file_bias, len);
            ret = -2;
        }
    }

    /* pack参数 */
    if (1 == reals.flasWrFlag.sBit.set_data_pack && (type & PARAM_TYPE_PACK))
    {
        file_bias = offsetof(SetData_Can_TypeDef, inv_can_info_t.pack_config);
        len = sizeof(pack_config_0x55_struct_mini);

        if (0 == fs_file_write(PARAMETER_FILE_PATH_INV(0, 00), (uint8_t *)&InvSetData.inv_can_info_t.pack_config, file_bias, len)
            && 0 == fs_file_write(PARAMETER_FILE_PATH_INV(0, 00), (uint8_t *)&InvSetData.inv_can_info_t.valid_pack, 2, 2))
        {
            ESP_LOGW(TAG, "Save pack parameter[%lu:%lu] success", file_bias, len);
            reals.flasWrFlag.sBit.set_data_pack = 0;
        }
        else
        {
            ESP_LOGE(TAG, "Save pack parameter[%lu:%lu] fail", file_bias, len);
            ret = -3;
        }
    } 
#if CONFIG_DISASTER_WARN_CHANNEL > 0    
    else if (1 == reals.flasWrFlag.sBit.disaster_warn && (type & PARAM_TYPE_DISASTER))
    {
        ESP_LOGI(TAG, "Save disaster parameter");
        reals.flasWrFlag.sBit.disaster_warn = 0;
        // 写入默认值
        if (0 == fs_file_write(PARAMETER_FILE_PATH_DISASTER(0, 00), (uint8_t *)&DisasterSaveData, 0, sizeof(DisasterSaveData)))
        {
            ESP_LOGW(TAG, "Save disaster parameter success");
        }
        else
        {
            ESP_LOGE(TAG, "Save disaster parameter failure");
            ret = -4;
        }
    } 
#endif    
#ifdef CONFIG_RELAY_CTRL_ENABLE
    if (1 == Relay_File_W_Flag.sBit.relay_data_soc_ctrl)
    {
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.PowerRelay_SmartSet01);
        len = sizeof(MOD_STRUCT_reg19000);

        /*检查复位标志*/
        if ((0 == fs_file_write(PARAMETER_FILE_PATH_RELAY(0, 00), (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet01, file_bias, len)))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_soc_ctrl File written ok");
            Relay_File_W_Flag.sBit.relay_data_soc_ctrl = 0;
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail");
        }
    }
    if (1 == Relay_File_W_Flag.sBit.relay_data_time_ctrl)
    {
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.PowerRelay_SmartSet02);
        len = sizeof(MOD_STRUCT_reg19100);

        if ((0 == fs_file_write(PARAMETER_FILE_PATH_RELAY(0, 00), (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet02, file_bias, len)))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_time_ctrl File written ok");
            Relay_File_W_Flag.sBit.relay_data_time_ctrl = 0;
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail");
        }
    }
    if (1 == Relay_File_W_Flag.sBit.relay_data_delay_ctrl)
    {
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.PowerRelay_SmartSet03);
        len = sizeof(MOD_STRUCT_reg19300);

        if ((0 == fs_file_write(PARAMETER_FILE_PATH_RELAY(0, 00), (uint8_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet03, file_bias, len)))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_delay_ctrl File written ok");
            Relay_File_W_Flag.sBit.relay_data_delay_ctrl = 0;
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail");
        }
    }
    if (1 == Relay_File_W_Flag.sBit.relay_data_plan_ctrl)
    {
        file_bias = offsetof(Relay_SetData_TypeDef, Relay_info_t.Backup_power_set);
        len = sizeof(MOD_STRUCT_reg19200);

        if ((0 == fs_file_write(PARAMETER_FILE_PATH_RELAY(0, 00), (uint8_t *)&RelaySetData.Relay_info_t.Backup_power_set, file_bias, len)))
        {
            ESP_LOGI(TAG, "SetData_Relay set_data_plan_ctrl File written ok");
            Relay_File_W_Flag.sBit.relay_data_plan_ctrl = 0;
        }
        else
        {
            ESP_LOGE(TAG, "SetData_Relay File written fail");
        }
    }
#endif
    return ret;
}

/*------------------------------------------------------------------------------
 Function: clear_relay_parameter_file
 -----------------------------------------------------------------------------*/
/**
  * @brief      清除relay参数文件
  * @param[in]  void  
  * @param[out] None
  * @return     int
  */
int clear_relay_parameter_file(void)
{
    char path[50] = {0};
    int fd;

    // 生成文件名
    sprintf(path, "%s", PARAMETER_FILE_PATH_RELAY(0, 00));
    ESP_LOGI(TAG, "Attempting to clear relay parameter file: %s", path);

    // 检查文件是否存在
    if (!fs_file_exists(path)) {
        ESP_LOGW(TAG, "[clear_relay_parameter_file] File not found: %s", path);
        return -1;
    }

    // 删除文件
    if (fs_file_delete(path) == 0)
    {
        ESP_LOGI(TAG, "[clear_relay_parameter_file] Successfully deleted: %s", path);
        
        // 重置relay相关标志位和数据（不重置有效标志）
        // 保存当前的有效标志
        uint16_t valid_flag = RelaySetData.Relay_info_t.valid_Relay;
        
        // 恢复工厂默认参数
        Relay_Ctrl_Factory_Parameter01_Update();
        Relay_Ctrl_Factory_Parameter02_Update();
        Relay_Ctrl_Factory_Parameter03_Update();
        Relay_Ctrl_Factory_Parameter04_Update();
        // 恢复有效标志（不重置）
        RelaySetData.Relay_info_t.valid_Relay = valid_flag;
        
        // 重置文件写入标志
        Relay_File_W_Flag.sBit.relay_data_soc_ctrl = 1;
        Relay_File_W_Flag.sBit.relay_data_time_ctrl = 1;
        Relay_File_W_Flag.sBit.relay_data_delay_ctrl = 1;
        Relay_File_W_Flag.sBit.relay_data_plan_ctrl = 1;
        
        // 重置控制结构体标志
        Relay_ctrl.bit.soc_ctrl_init = 1;
        Relay_ctrl.bit.time_ctrl_init = 1;
        Relay_ctrl.bit.delay_ctrl_init = 1;
        Relay_ctrl.bit.plan_ctrl_init = 1;
        Relay_ctrl.bit.soc_ctrl_target = 0;
        Relay_ctrl.bit.time_ctrl_target = 0;
        Relay_ctrl.bit.delay_ctrl_target = 0;
        Relay_ctrl.bit.plan_ctrl_target = 0;
        
        ESP_LOGI(TAG, "[clear_relay_parameter_file] Relay parameters reset to factory defaults");
        
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "[clear_relay_parameter_file] Failed to delete: %s", path);
        return -1;
    }
}


