#pragma once

#include "comm_define.h"
#include "iot_define.h"
#include "pack_define.h"
#include "inv_define.h"
#include "relay_define.h"

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#pragma pack(1)

#define IOT_FACTORY "iot_factory" // 标定
#define DEV_FACTORY "dev_factory" // 标定

#define P1CHAR_TYPE_LEN ((uint16_t)1024 * 4) 
#define P2CHAR_TYPE_LEN ((uint16_t)1024 * 2)
#define IOT_EVENT_FUNC_04_TYPE_LEN ((uint16_t)20)

#define DISASTER_MAX_BACKUP_COUNT 10

enum {
    PARAM_TYPE_IOT      = (1U << 0),
    PARAM_TYPE_INV      = (1U << 1),
    PARAM_TYPE_PACK     = (1U << 2),
    PARAM_TYPE_RELAY    = (1U << 3),
    PARAM_TYPE_DISASTER = (1U << 4),
    PARAM_TYPE_ALL      = 0xFFFF
};

typedef struct
{
    char iot_type[12];    // iot出厂标定类型
    char revd[4];         // revd
    uint64_t iot_sn;      // iot出厂标定sn
    uint64_t safe_code;   // iot出厂标定安全随机码
    uint8_t cert_md5[48]; // 三份证书的MD5值
    uint8_t nc[38];       //  保留
} iot_factory_struct;         // 存储在 flash NVS

extern USE_EXT_RAM_BSS iot_factory_struct iot_factory;

typedef struct
{
    char dev_type[12];    // dev出厂标定类型
    uint64_t dev_sn;      // dev出厂标定sn
    uint8_t nc[24];       //  保留
} dev_factory_struct;         // 存储在 flash NVS

extern USE_EXT_RAM_BSS dev_factory_struct dev_factory;


typedef union // 2Byte
{
    // uint8_t all;
    uint16_t word;
    struct struct_Flag_EE_WR
    {

        uint8_t Wifi : 1;    //
        uint8_t server2 : 1; //
        uint8_t revd_1 : 1;  //
        uint8_t revd_2 : 1;  //

        uint8_t revd_xx : 4; //

        uint8_t revd99 : 8; //
    } bits;
} g_st_Flag_EE_WR;

typedef union
{
    uint32_t all;
    struct
    {
        // 1-enable;2-disable
        uint32_t wifi_enable : 2;     // WiFi 开关;windy目前使用此bit，表示sta en,未使用 wifi_sta_enable;HMI使用，将错就错
        uint32_t wifi_sta_enable : 2; // wifi-STA开关
        uint32_t wifi_ap_enable : 2;  // WiFi-AP开关
        uint32_t ble_enable : 2;      // 蓝牙开关

        uint32_t ble_adv_enable : 2;  // 蓝牙广播开关
        uint32_t ble_mash_enable : 2; // 蓝牙组网开关
        uint32_t _4g_enable : 2;      // 0：关闭，1：开启
        uint32_t gps_enable : 2;

        uint32_t Eth_enable : 2; // 有线以太网使能
        uint32_t WIFI_mesh : 2;  // wifi mesh使能
        uint32_t revd1 : 2;
        uint32_t new_dev_discovery : 2; // 新设备发现模式（combox启用）

        uint32_t revd2 : 8;

    } bit;
} iot_func_switch_t; // cmd

typedef struct
{
    uint32_t Pv_Total;           //	PV总的充电能量	1
    uint32_t GridChgin_Total;    //  电网充电能量         2
    uint32_t GridFeedback_Total; //	馈电到电网能量 3
    uint32_t ACLoad_Total;       //	AC负载总能量        4
    uint32_t DCLoad_Total;       //	DC负载总能量        5
    uint32_t revd[3];            //  6,7,8
    uint32_t PackDsg_Total;      //	Pack总放电能量 9
    uint32_t PVToload_Total;     //	PV到AC负载能量 10
    uint32_t PvCar_Total;        //  PV行车快充能量 11
} baseKWH_energy_t_u32_2;        // KWH变量基础结构体

typedef struct
{
    uint16_t Pv_Total;           //	PV总的充电能量	1
    uint16_t GridChgin_Total;    //  电网充电能量         2
    uint16_t GridFeedback_Total; //	馈电到电网能量 3
    uint16_t ACLoad_Total;       //	AC负载总能量        4
    uint16_t DCLoad_Total;       //	DC负载总能量        5
    uint16_t revd[3];            //  6,7,8
    uint16_t PackDsg_Total;      //	Pack总放电能量 9
    uint16_t PVToload_Total;     //	PV到AC负载能量 10
    uint16_t PvCar_Total;        //  PV行车快充能量 11
} baseKWH_energy_t_u16;          // KWH变量基础结构体

typedef struct
{
    uint8_t su8tm_year;
    uint8_t su8tm_mon;
    uint8_t su8tm_mday;
    uint8_t su8tm_hour;
} energy_time_t; // 能量信息存储时间戳

// 电网断电次数按时间段统计结构体
typedef struct
{
    uint32_t daily_count;   // 当日断电次数
    uint32_t weekly_count;  // 当周断电次数
    uint32_t monthly_count; // 当月断电次数
    uint32_t yearly_count;  // 当年断电次数
    uint32_t total_count;   // 至今总断电次数

    // 时间戳记录，用于判断是否需要清零统计
    uint64_t last_update_time; // 上次更新的时间戳，使用uint64_t避免平台差异
} grid_off_count_statistics_t;

// 断电事件队列记录结构体（用于批量上报）
typedef struct {
    uint32_t start_time;        // 断电开始时间（使用32位时间戳，节省4字节）
    uint32_t end_time;          // 断电结束时间（使用32位时间戳，节省4字节）
    uint8_t need_report;        // 是否需要上报: 0=不需要上报, 1=需要上报
    uint8_t remark;             // 事件备注编号（预留）
} power_off_record_t;

typedef struct {
    uint8_t http_get_host_valid;
    char http_get_old_host[120];
    uint16_t http_get_old_port;
    uint8_t http_get_old_ipaddr[4];
} old_mqtt_connect_data_t;

typedef struct {
    uint8_t ota_state;
    uint8_t ota_result;
    uint8_t ota_type;
	uint8_t target_id;
    uint32_t version;
    uint16_t file_size;
}bms_ota_struct;//存储在 文件系统

typedef union {
    uint32_t all;
    struct
    {
        uint8_t soc_min;
        uint8_t soc_max;
        
        uint8_t ctrl_mode : 2;
        uint8_t Unuse_time_ctrl : 2;
    };
}gen_ctrl_struct;

typedef struct {
    uint16_t dev_type;           // 设备型号
    uint64_t dev_sn;             // 设备SN
    uint16_t revd[40];          // 预留
} plug_cfg_t;

// IOT自身参数
typedef union tagSetData // union内部 struct等效,需要为
{
    struct
    {
        uint8_t CfgCharData[P1CHAR_TYPE_LEN]; //
    } ArrayData;                              /*设置数据结构*/

    struct sNameData
    {
        uint16_t valid_iot;      // 参数有效标志,iot本身，必须
        uint16_t valid_other;    // 参数有效标志，其他，辅助
        g_st_Flag_EE_WR Wr_flag; // 参数区变量被人工或外部修改的标志，部分关键
        uint16_t valid_revd;     // revd

        union 
        {
            uint16_t base_info[21];//modbus 0~20
            struct 
            {
                uint16_t protocol_ver; // 16 协议版本号
                uint16_t iot_protocol_ver; // 15 协议版本号
                Machine_standard_struct Machine_standard;//1 机器规格标志
                scene_struct Scene_Flag;//5
                ems_ctrl_struct ems_ctrl;//4
                iot_mode_struct support_mode; // 6
            };  
        };  

        uint16_t factory;            /*12000 恢复出厂设置*/
        uint16_t wifi_sta_auth;      /*12001 wifi sta 加密认证方式 wifi 作为sta时 需要连接的路由器加密方式*/
        char wifi_sta_ssid[32];      /*12002~12017 WiFi sta ssid  wifi 作为sta时 需要连接的路由器名称*/
        char wifi_sta_password[64];  /*12018~12049 wifi sta 密码 wifi 作为sta时 需要连接的路由器密码*/
        sta_eanble_t sta_enable;     /*12050 WIFI路由器密码高32字节有效标志,0xA5表示有效，否则无效*/
        uint16_t revd1[16];          /*12051~12066 预留*/
        char could_dns[128];         /*12067~12130 服务器地址*/
        char mobile_apn[60];         /*12131~12160 apn*/
        config_status on_off;        /*12161~12162*/
        thunder_ctrl_t thunder_ctrl; /*12163*/
        uint32_t revd5;
        Struct_IOT_Enable_mix1 IOT_Enable_mix1;
        uint16_t IOT_Enable_mix2;
        Protocol_north_ctrl_t Protocol_3r_Enable_mix1;
        Protocol_south_ctrl_t Protocol_3r_Enable_mix2;
        Time_Span_t Time_Span;
        lcd_mode_struct lcd_mode;  // 温度单位
        uint32_t saved_money; // 省钱金额
        temp_unit_control_t temp_unit_control;// 温度单位写入标志
        uint16_t revd11[45];

        ///////valid_other
        uint32_t hardware_ver; // 硬件版本号
        uint32_t software_ver; // 软件版本号

        uint16_t revd2[10];

        uint8_t app_password[6];
        char Net_Server_address[128]; // 22005 服务器网址 ，windy增加第二服务器
        char Net_Server_secret[64];   // 22069 登录服务器密码
        
        ctrl_meter_struct ctrl_meter; // 2228 电表类型选择
        uint16_t ctrl_feedback;
        uint16_t CounterCurrentPower_Limit;
        uint16_t Meter_Select;
        
        uint32_t historyRecSaveCount; // log记录条数 ,modbus 3000用辅助
        uint32_t historyAddrIndex;    // log记录后的地址偏移条目序号：0~max-1,10w ,modbus 3000用辅助

        uint32_t Event_SaveCount; // log记录条数 ,windy add,data channel
        uint32_t Event_AddrIndex; // log记录后的地址偏移条目序号：0~max-1,10w ,windy add,data channel

        time_res_struct SetTimeZone; // 时区,因INV本身不保存时区，所以IOT要保存
        uint8_t revd4;
        uint32_t data_report_cycle;

        uint8_t Sn_bind[5];                              // 用于CAN并机群组绑定的关联群组设备SN

        uint16_t revd3[90];
        
        grid_off_count_statistics_t grid_off_statistics; // 电网断电次数按时间段统计
        
        uint16_t invDetailedInfo_AddrIndex[1];           // 逆变器详细数据记录地址索引 只记录本机逆变器详细数据 资源受限未预留
        uint16_t invDetailedInfo_SaveCount[1];           // 逆变器详细数据记录保存计数 只记录本机逆变器详细数据 资源受限未预留
        uint8_t revd[82];

        uint8_t u8temp; //

        ble_protocol_en_t ble_protocol; // BLE 协议使能控制
        int16_t blec_rssi_th;           // BLE客户端切换RSSI阈值
        uint16_t blec_switch_int;       // BLE客户端切换间隔
        uint8_t bles_adv_key[16];       // BLE server广播数据的加密秘钥

        int16_t wifi_sta_rssi_th;     // STA RSSI阈值
        uint16_t wifi_sta_switch_int; // STA切换间隔

        wifi_mul_sta_en_t wifi_mul_sta_en; // 多STA使能
        /* Wi-Fi sta1 auth ssid password等配置使用原有的配置 */
        uint32_t wifi_sta1_ip;   // STA1静态IP地址
        uint32_t wifi_sta1_mask; // STA1静态子网掩码
        uint32_t wifi_sta1_gw;   // STA1静态网关地址
        uint32_t wifi_sta1_dns1; // STA1静态DNS1地址
        uint32_t wifi_sta1_dns2; // STA1静态DNS2地址

        uint16_t wifi_sta2_auth;
        char wifi_sta2_ssid[32];
        char wifi_sta2_password[64];
        uint32_t wifi_sta2_ip;           // STA2静态IP地址
        uint32_t wifi_sta2_mask;         // STA2静态子网掩码
        uint32_t wifi_sta2_gw;           // STA2静态网关地址
        uint32_t wifi_sta2_dns1;         // STA2静态DNS1地址
        uint32_t wifi_sta2_dns2;         // STA2静态DNS2地址
        open_mqtt_en_t open_mqtt_enable; // MQTT使能
        uint16_t open_mqtt_report_cycle; // MQTT上报间隔, 仅在report_en=1时有效

        uint16_t wifi_sta3_auth;
        char wifi_sta3_ssid[32];
        char wifi_sta3_password[64];
        uint32_t wifi_sta3_ip;   // STA3静态IP地址
        uint32_t wifi_sta3_mask; // STA3静态子网掩码
        uint32_t wifi_sta3_gw;   // STA3静态网关地址
        uint32_t wifi_sta3_dns1; // STA3静态DNS1地址
        uint32_t wifi_sta3_dns2; // STA3静态DNS2地址

        uint16_t wifi_ap_auth;     // 12951
        char wifi_AP_ssid[32];     // 12952~12967
        char wifi_AP_password[64]; // 12968~12999

        modbus_tcp_en_t modbus_tcp_enable; // Modbus TCP使能
        uint16_t modbus_tcp_port;          // Modbus TCP端口号

        old_mqtt_connect_data_t old_mqtt_connect_data;  // 上次云端连接地址

		bms_ota_struct bms_ota_info; //电池包升级状态

        uint16_t mqtt_heartbeat;//mqtt心跳
        
        Remote_set_struct Remote_set;
        uint8_t Remote_set_soc_limit;
        led_set_struct LED_SET; 
        gen_ctrl_struct Gen_ctrl;
        
        uint16_t WIFI_Mesh_ID[3];//6字节，用于区分不同MESH网络；(顶层网络名称，当前网络实际值)
        plug_cfg_t plug_cfg[NET_WIFI_S1_POINT];

        time_t mqtt_http_ip_date;   // MQTT域名更新时间
        
        uint8_t temp;
    } dev_info_t;                          // NameData;	//设置数据结构
} SetData_TypeDef; /*设置数据联合*/        /*16*192*/

_Static_assert(sizeof(SetData_TypeDef) == P1CHAR_TYPE_LEN, "SetData_TypeDef size mismatch");

extern USE_EXT_RAM_BSS SetData_TypeDef IotSetData;

//IOT自身参数
typedef union tagSetData2//union内部 struct等效,需要为   
{
    struct
    { 
        uint8_t	CfgCharData[P2CHAR_TYPE_LEN];
    }ArrayData;         /*设置数据结构*/

    struct sNameData2// 
    {
        uint16_t valid_inv;//参数有效标志
        uint16_t valid_pack;//参数有效标志

        union
        {
            uint16_t Set1[200];
            inv_set00_0x1A_struct_mini  inv_set00;  // inv设置区00
        };
        union
        {
            uint16_t Set2[100];
            inv_set01_0x1B_struct_mini  inv_set01;  // inv设置区01
        };
        union
        {
            uint16_t Set3[100];
            inv_set02_0x1C_struct_mini  inv_set02;  // inv设置区02
        };
        union
        {
            uint16_t Set4[100];
            inv_set03_0x1D_struct_mini  inv_set03;  // inv设置区03
        };
        union
        {
            uint16_t Set5[100];
            pack_config_0x55_struct_mini pack_config;    // pack 参数配置区
        };

        uint8_t u8temp;
    }inv_can_info_t;
}SetData_Can_TypeDef;

_Static_assert(sizeof(SetData_Can_TypeDef) == P2CHAR_TYPE_LEN, "SetData_Can_TypeDef size mismatch");

typedef enum
{
    Immediate = 0,
    Expected,
    Future,
    Unknown1
}_URGENCY;//紧急度

typedef enum
{
    Extreme = 0,
    Severe,
    Moderate,
    Minor,
    Unknown2
}_SEVERITY;//严重度

typedef enum
{
    Observed = 0,
    Likely,
    Possible,
    Unlikely,
    Unknown3
}_CERTAINTY;//确信度

typedef union
{
    uint8_t all;
    struct 
    {
        uint8_t first_flag  : 1;
    }bits;
}thunder_state_t;

typedef struct 
{
    uint32_t StartTime;
    uint32_t EndTime;
    _URGENCY Urgency;//紧急度
    _SEVERITY Severity;//严重度
    _CERTAINTY Certainty;//确信度
    uint16_t Event;//警报事件
    uint16_t SoC;//Severity对应目标电量
    int32_t TimeZoneOffset;
    uint16_t uuid;
	char msg_id[100];
    thunder_state_t state;
    storm_end_reason_t reason;
    uint8_t revd[17];
}_SaveWebStormWatchApiData;//API data

typedef struct
{
    uint16_t DataIdentificationCode;
    uint8_t EventEndFlag;
    uint16_t LastSetLowCapacity;        //系统低电量设置值
    uint16_t LastSetHighCapacity;       //系统高电量设置值
    uint16_t LastGridChgSwitch;         //上一次的电网充电开关

    uint16_t CancelEvent[DISASTER_MAX_BACKUP_COUNT];
    uint16_t CancelEventRunTime[DISASTER_MAX_BACKUP_COUNT];
}_AppCancelStormEvent;

typedef union {
    uint8_t all[1024];

    struct __attribute__((packed)) {
        _SaveWebStormWatchApiData SaveWebStormWatchApiData;
        _AppCancelStormEvent AppCancelStormEvent;
        disaster_send_param_t disaster_send_param;
    };
} disaster_save_param_t;

_Static_assert(sizeof(disaster_save_param_t) == 1024, "disaster_save_param_t size mismatch");

typedef union // 2Byte
{
    // uint8_t all;
    uint16_t word;
    struct structTime
    {
        uint8_t b1FlagSys10ms : 1;  // 判断函数10ms定时,10ms不准确  ,period_task线程
        uint8_t b1FlagSys50ms : 1;  //
        uint8_t b1FlagSys100ms : 1; //
        uint8_t b1FlagSys500ms : 1; //

        uint8_t b1FlagSys1s : 1;                //
        uint8_t b1FlagSys1Minute : 1;           // 判断函数1分钟定时
        uint8_t b1FlagSys10ms_2 : 1;            // 判断函数10ms定时,can tx 线程
        uint8_t b1FlagSys50ms_can_hi_level : 1; // can线程使用

        uint8_t flagA_1s : 1;         // set和clean不在一个线程,server2 MQTT
        uint8_t flagC_1s : 1;         //
        uint8_t b1FlagSys100ms_2 : 1; // can线程使用
        uint8_t revd4 : 1;            //

        uint8_t b1FlagSys100ms_data_process : 1;     // 
        uint8_t b1FlagSys1s_data_process : 1;        // 
        uint8_t b1FlagSys100ms_slow_period_task : 1; //
        uint8_t b1FlagSys1s_slow_period_task : 1;    //
    } bits;
} g_st_FlagTime;

/**
 * @brief RTC时间结构体定义
 */
typedef struct
{
    struct
    {
        uint16_t mon : 8;
        uint16_t year : 8; // 2000年为基准
    };

    struct
    {
        uint16_t hour : 8; // 0~23
        uint16_t day : 8;
    };

    struct
    {
        uint16_t sec : 8;
        uint16_t min : 8;
    };
} rtc_time_t;

typedef union
{
    uint16_t Byte2; //
    struct
    {
        uint16_t RTC_valid_from_NTP : 1; //
        uint16_t RTC_valid_from_APP : 1;
        uint16_t RTC_valid_from_CAN : 1;
        uint16_t RTC_valid_from_SERVER : 1;

        uint16_t RTC_valid_from_RTC : 1;
        uint16_t Time_valid : 1;
        uint16_t revd8 : 2;

        uint16_t RTC_set_IOT_to_INV : 1; //
        uint16_t RTC_set_APP_to_IOT : 1; //
        uint16_t RTC_set_SERVER_to_IOT : 1;
        uint16_t revd9 : 1;

        uint16_t revd11 : 4;
    } sBit;
} BitData_RTC; // RTC切换标志

typedef union
{
    uint16_t Byte2; //
    struct
    {
        uint8_t sub1g_net : 1; //   iot part set 12000
        uint8_t wifi_net : 1;  //
        uint8_t turn_on : 1;   //
        uint8_t turn_off : 1;  //

        uint8_t period : 1; //
        uint8_t revd6 : 1;  //
        uint8_t revd7 : 1;  //
        uint8_t revd8 : 1;  //

        uint8_t revd9 : 1;  //
        uint8_t revd10 : 1; //
        uint8_t revd11 : 1; //
        uint8_t revd12 : 1; //

        uint8_t revd : 4; //

    } sBit;
} BitData_EVENT; // 来自BLE和WIFI的命令操作区分

typedef union
{
    uint64_t Byte8[2];
    struct
    {
        // 1
        uint32_t SetDataUpdate_all : 1;        //
        uint32_t SetDataUpdate_historycnt : 1; //  1-更新存储在EEPROM里面的历史记录
        uint32_t SetDataUpdate_SN : 1;         //
        uint32_t SetDataUpdate_Event_cnt : 1;  //
        uint32_t wifi_sta_ssid : 1;     //
        uint32_t wifi_sta_password : 1; //
        uint32_t thunder_ctrl : 1;      //
        uint32_t on_off : 1;            //

        // 2
        uint32_t could_dns : 1;          //
        uint32_t mobile_apn : 1;          
        uint32_t Net_Server_secret : 1;  //
        uint32_t Net_Server_address : 1; //
        uint32_t app_password : 1;      //
        uint32_t ctrl_meter : 1;        //
        uint32_t SetDataUpdate_DSP : 1; //
        uint32_t ctrl_feedback : 1;     //

        // 3
        uint32_t bind : 1;     // 本地存储的SN绑定表格更新
        uint32_t SetTimeZone : 1; // 存储上次写入时区信息
        uint32_t data_report_cycle : 1;
        uint32_t ctrl_2006 : 1;
        uint32_t time_span : 1;
        uint32_t saved_money : 1;         // 省钱金额
        uint32_t lcd_mode : 1;            // 断码屏模式设置
        uint32_t grid_off_statistics : 1; // 电网断电次数统计

        // 4
        uint32_t support_mode : 1;
        uint32_t reset_factory : 1;
        uint32_t sleep_mode : 1; // 预留
        uint32_t SetDataUpdate_invInfo_cnt : 1;
        uint32_t update_status : 1;
        uint32_t protocol_enable : 1;
        uint32_t bind_sn : 1;
        uint32_t kwh_mix : 1; // u32少量 kwh存储到eeprom

        // 5
        uint8_t ble_protocol       		: 1;//BLE协议
        uint8_t ble_client       		: 1;//ble client
        uint8_t ble_server_adv_key      : 1;//ble server
        uint8_t reserved4               : 1; // 保留位
        uint8_t eth                     : 1;
        uint8_t wifi_ap_ssid            : 1;
        uint8_t wifi_ap_password        : 1;
        uint8_t wifi_ap_auth            : 1;

        // 6
        uint8_t wifi_sta_rssi_th        : 1;//wifi sta rssi阈值
        uint8_t wifi_sta_switch_int     : 1;//wifi sta rssi切换间隔
        uint8_t wifi_mul_sta            : 1;//wifi 多sta使能
        uint8_t wifi_sta1_ip            : 1;
        uint8_t wifi_sta1_mask          : 1;
        uint8_t wifi_sta1_gw            : 1;
        uint8_t wifi_sta1_dns1          : 1;
        uint8_t wifi_sta1_dns2          : 1;

        // 7
        /* wifi sta2静态IP相关参数变化 */
        uint8_t wifi_sta2_ssid          : 1;
        uint8_t wifi_sta2_password      : 1;
        uint8_t wifi_sta2_auth          : 1;
        uint8_t wifi_sta2_ip            : 1;
        uint8_t wifi_sta2_mask          : 1;
        uint8_t wifi_sta2_gw            : 1;
        uint8_t wifi_sta2_dns1          : 1;
        uint8_t wifi_sta2_dns2          : 1;

        // 8
        /* wifi sta3静态IP相关参数变化 */
        uint8_t wifi_sta3_ssid          : 1;
        uint8_t wifi_sta3_password      : 1;
        uint8_t wifi_sta3_auth          : 1;
        uint8_t wifi_sta3_ip            : 1;
        uint8_t wifi_sta3_mask          : 1;
        uint8_t wifi_sta3_gw            : 1;
        uint8_t wifi_sta3_dns1          : 1;
        uint8_t wifi_sta3_dns2          : 1;

        // 9
        uint8_t mqtt_en                 : 1;
        uint8_t mqtt_report_cycle       : 1;
        uint8_t modbus_tcp_en             : 1;
        uint8_t modbus_tcp_port           : 1;
        uint8_t wifi_sta_auth             : 1;
        uint8_t old_mqtt_connect             : 1;
        uint8_t temp_unit_control       : 1;
        uint8_t remote                  : 1;
        
        // 10
        uint8_t led_set                 : 1;
        uint8_t mqtt_heartbeat          : 1;
        uint8_t gen_set                 : 1;
        uint8_t plug_cfg                : 1;
        uint8_t mesh_id                 : 1;
    } sBit;
} BitData_PARAMETER_Set; // NVS data set flag  BitData_Eprom;//EEPROM set flag

typedef union
{
    uint16_t Byte2; //
    struct
    {
        //        uint16_t kwh_hist_PV		: 1;//
        //        uint16_t kwh_hist_Grid			: 1;
        //        uint16_t kwh_hour_PV			: 1;
        //        uint16_t kwh_hour_Grid			: 1;
        uint16_t log_invdetailedinfo : 1; // 历史记录内容
        uint16_t revd4 : 3;

        uint16_t log_fault : 1;      // 历史记录内容
        uint16_t Set_historycnt : 1; // 历史记录序号
        uint16_t iot_event : 1;      // not need
        uint16_t revd8 : 1;

        uint16_t revd9 : 1;
        uint16_t set_data_panel : 1;
        uint16_t set_data_pack : 1; // eeprom 参数存储
        uint16_t set_data_inv : 1;

        uint16_t year_change : 1; // 用于kwh 统计hour，需要基于年变化循环覆盖
        uint16_t month_change : 1;
        uint16_t day_change : 1;
        uint16_t hour_change : 1;

        uint16_t disaster_warn : 1;     // 灾害预警
    } sBit;
} BitData_FILE_SET; //  set flag

typedef union
{
    uint16_t Byte2; //
    struct
    {
        uint16_t year_DCLoad_Total : 1; //
        uint16_t year_ACLoad_Total : 1;
        uint16_t year_Pv_Total : 1;
        uint16_t year_GridChgin_Total : 1;

        uint16_t year_GridFeedback_Total : 1; //
        uint16_t year_PVToload_Total : 1;     //
        uint16_t year_PackDsg_Total : 1;      //
        uint16_t hour_file : 1;

        uint16_t year_PVCAR_Total : 1;
        uint16_t revd1 : 3;

        uint16_t revd2 : 4;
    } sBit;
} BitData_KWH_FILE_SET; //  set flag

typedef struct
{
    uint16_t ReadRegAddress;  // 作为modbus主的辅助解析变量，寄存器地址
    uint16_t ReadRegCnt;      // 作为modbus主的辅助解析变量，度寄存器数量
    uint16_t WriteRegAddress; // 作为modbus主的辅助解析变量，寄存器地址
    uint16_t WriteRegCnt;     // 作为modbus主的辅助解析变量，度寄存器数量
} SERVER_STRUCT;

typedef union
{
    uint64_t word;   // 
    struct
    {
        uint8_t new_cfg       				: 1;//   iot part set 12000
        uint8_t wifi_sta_auth       		: 1;//12001 wifi sta 加密认证方式
        uint8_t wifi_sta_ssid       		: 1;//12002~12017 WiFi sta ssid
        uint8_t wifi_sta_password       	: 1;//12018~12049 wifi sta 密码

        uint8_t could_dns       		: 1;//12067~12130 服务器地址
        uint8_t mobile_apn       		: 1;//12131~12160 apn
        uint8_t on_off       			: 1;//12161~12162
        uint8_t thunder_ctrl       		: 1;//12163
		
        uint8_t period_report       	: 1;//12164~12165 数据上报服务器周期（休眠周期，4G模块省流量目的）	2	秒数	R/W
        uint8_t IOT_Enable_mix1      	 : 1;
        uint8_t IOT_Enable_mix2       	: 1;
        uint8_t Protocol_3r_Enable_mix1   : 1;
		
        uint8_t Protocol_3r_Enable_mix2    : 1;;
        uint8_t cfg_22000       		: 1;//   
        uint8_t iot_can_set_mix       	: 1;//   CAN总线多字节写
        uint8_t Net_Server_address      : 1;//  22005 服务器网址  
        
        uint8_t Net_Server_secret       : 1;//   22069 登录服务器密码
        uint8_t app_password    		: 1;
        uint8_t support_mode        	: 1;//  
        uint8_t dcdc_set1        		: 1;// 
        
        uint8_t Time_Span        			: 1;//  断码屏相关时间跨度设置
        uint8_t LCD_Mode        			: 1;//  断码屏模式设置
        uint8_t sleep_mode        			: 1;//  休眠模式
        uint8_t mesh_id        			    : 1;//  
		
        uint8_t ble_protocol       		: 1;//BLE协议
        uint8_t ble_client       		: 1;//ble client
        uint8_t ble_server       		: 1;//ble server
        uint8_t wifi_sta_rssi           : 1;//wifi sta rssi阈值和切换间隔

        uint8_t wifi_mul_sta            : 1;//wifi 多sta使能
        uint8_t wifi_sta1               : 1;
        uint8_t wifi_sta2               : 1;//wifi sta2静态IP相关参数变化
        uint8_t wifi_sta3               : 1;//wifi sta3静态IP相关参数变化

        uint8_t eth                     : 1;//eth静态IP相关参数变化
        uint8_t wifi_ap                 : 1;//wifi ap相关参数变化
        uint8_t mqtt                    : 1;//mqtt相关参数变化
        uint8_t modbus_tcp              : 1;//modbus tcp相关参数变化

        uint8_t bat_maintain_set        : 1;//电池包维护模式设置
        uint8_t reset_factory           : 1;//恢复出厂设置
        uint8_t ctrl_2006               : 1;// 2006寄存器
        uint8_t ctrl_power              : 1;

        uint8_t ctrl_remote             : 1;
        uint8_t ctrl_led                : 1;
        uint8_t gen_set                 : 1;
        uint8_t iot_factory_cmd         : 1;
        
        uint32_t revd3                 : 20;//保留位
    } sBit;
} BitData_MODBUS_CMD;//来自BLE和WIFI的命令操作区分

typedef struct
{
    uint8_t to_uart_flag; // 是否正在透传标志
    int64_t to_uart_time; // 透传发送时间时间戳(单位ms)
} BLE_WIFI_TO_UART_INFO;

typedef struct
{
    union
    {
        uint16_t all;

        struct
        {
            uint16_t FaultSeq : 8;   // 低字节：故障/告警编号
            uint16_t FaultState : 8; // 高字节：故障发生（1）或消失（0）
        };
    };
} LogFaultSeq_STRUCT;

typedef struct
{
    rtc_time_t LogTime;             // 故障发生时间
    LogFaultSeq_STRUCT LogFaultSeq; // 故障/告警编号
    uint16_t LogFaultCode;          // 故障/告警代码
} LOG_FAULT_STRUCT;                 // modbus 3000历史记录

typedef struct
{
    union
    {
        uint8_t all[128];

        struct
        {
            uint16_t type;             // 能量类型
            uint16_t year;             // 哪一年
            uint32_t total_energy;     // 当年总能量
            uint32_t mouth_energy[12]; // 每个月的总能量
            uint16_t day_enengy[31];   // 当月每天的总能量

            uint16_t crc16;
        };
    };
} year_energy_t;

typedef struct
{
    union
    {
        uint8_t CfgCharData[IOT_EVENT_FUNC_04_TYPE_LEN];

        struct
        {
            uint32_t time_s;
            uint8_t ver_protocol;

            union
            {
                uint16_t Fault;

                struct
                {
                    uint16_t FaultCode : 15;
                    uint16_t FaultState : 1; // 1-happen;0-clear
                };
            };

            union
            {
                uint16_t Warn;

                struct
                {
                    uint16_t WarnCode : 15;
                    uint16_t WarnState : 1; // 1-happen;0-clear
                };
            };

            uint8_t Num_IOT;             // Num_IOT IOT数量			1
            uint8_t Num_INV;             // Num_INV 逆变器数量 		1
            uint8_t Num_PACK;            // Num_PACK	PACK数量			1
            uint8_t Num_udp_net;         // Num_up_net	WIFI局域网我司设备节点数量			1
            uint8_t Num_sub1g_net;       // Num_sub1g_net	Sub1G网络我司设备节点数量 		1
            uint16_t time_wifi_connect;  // time_up_connect 北向(WIFI)联网持续时间	分钟		2
            uint16_t time_sub1g_connect; // time_sub1g_connect	Sub1G联网持续时间 分钟		2
        };
    };
} EventHistoryData_Struct; // 本地事件记录

typedef struct
{
    uint8_t isOta; // 0：未升级，1：升级中，  2：升级成功 3：升级失败
    uint8_t type;
    uint8_t pct;
    uint8_t errCode; // 错误码

    uint32_t soft_version;
} current_ota_info_t;

typedef union
{
    uint16_t Byte2; //
    struct
    {
        /*顺序不可变动*/
        uint8_t devassist : 1;
        uint8_t iot : 1;
        uint8_t inv : 1;
        uint8_t pack : 1;
        uint8_t dcdc : 1;
        uint8_t dchub : 1;

    } sBit;
} Need_new_version_flag_t;

typedef struct
{
    uint8_t can_dev;         // iot/inv/pack类型
    uint8_t can_type;        // 数据类型
    uint8_t range_level;     // 报文重要程度，0-最低，非零-依次增加；高等级的可对低等级的插队，反之不能，同等级也不能插队
    uint8_t run_flag;        // 1-当前can_type查询正在执行，0-完成
    uint32_t time_old;       //
    uint16_t Cnt_bigcycle;   // 执行计数器，首次启动快速获取，0-表示首次/立刻执行
    uint16_t Cnt_smallcycle; // 当前can_type切换 不同 地址用计数器
    uint8_t  fast_read;     //快速回读标志
    uint8_t fast_sum;       //快速汇总标志
        
    uint16_t period; // X1000s
    uint8_t max_x;
    uint8_t max_y; // 一维数组，不需要y

    uint16_t modbus_Rd_SlaveAddr;      
    uint16_t modbus_Rd_cnt_last; // modbus_Rd_cnt上一次的旧值
    uint16_t modbus_Rd_cnt_100ms;// BLE快速读取用的倒计时计数器，0到非0瞬间 发送，延时清零做读取间隔,1秒 -1

} can_poll_struct; // iot read

typedef union
{
    uint64_t Byte8; 
    struct
    {
        uint8_t wifi_connect_ap                     : 1;    // 1-已通过WIFI连接路由器局域网，并获得本地IP        
        uint8_t smart_ctrl_state_change             : 1;    // iot智能控制状态 0:无变化 1：有变化
		uint8_t system_off_flag                     : 1;    // 系统关机标志
        uint8_t system_sleep_flag                   : 1;    // 系统休眠标志

        uint8_t system_wakeup_flag                  : 1;    // 系统休眠唤醒标志
        uint8_t wifi_reconnect_request_in_sleep     : 1;    // 休眠状态下WIFI重新连接请求
        uint8_t system_restart_flag                 : 1;    // 系统复位标志
    } sBit;
} BitData_IOT_Status_Flag;


typedef union
{
    uint16_t Byte2;   // 
    struct
    {
        uint16_t set_data_inv_2000	    : 1;
        uint16_t set_data_inv_2200	    : 1;
        uint16_t set_data_inv_2300	    : 1;
        uint16_t set_data_inv_2400      : 1;

        uint16_t reset                  : 1;
        
    } sBit;
} BitData_INV_FILE_SET;

typedef struct
{
    uint8_t ota_mcu_total_count;
    uint8_t ota_mcu_curr_count;
    uint8_t ota_total_pct;
} system_ota_info_struct;

typedef union
{
    uint16_t value;
    struct
    {
        uint8_t log : 2;        // bit1~0:串口打印；
        uint8_t can : 2;        // bit2/3:CAN;
        uint8_t ble : 2;        // bit4/5:BLE;
        uint8_t mqtt1 : 2;      // bit6/7:MQTT1;
        uint8_t modbus_tcp : 2; // bit8/9:modbus TCP
    };
} supper_control_resource_t; // 远程log通道输入源

typedef union
{
    uint16_t value;
    struct
    {
        uint8_t tx : 2;     // bit1~0:TX
        uint8_t rx : 2;     // bit2/3:RX
    };
} supper_control_direction_t; // LOG文件包含的收发方向使能

typedef union
{
    uint16_t value;
    struct
    {
        uint8_t mqtt1 : 2;     // bit1~0:MQTT1
        uint8_t wlcc : 2;      // bit2/3:UDP
        uint8_t mqtt2 : 2;     // bit4/5:MQTT2
    };
} supper_control_channel_t; // 远程LOG通道-设备底层输出协议使能

enum supper_cmd_e
{
    SUPPER_CMD_RESTART = 0,        // 重启
    SUPPER_CMD_WIFI_RESTART = 1,   // WIFI重启
    SUPPER_CMD_BLE_RESTART = 2,    // BLE重启
    SUPPER_CMD_CAN_RESTART = 3,    // CAN重启
    SUPPER_CMD_LOG_RESOURCE = 4,   // 远程log通道输入源设置
    SUPPER_CMD_LOG_DIRECTION = 5,  // LOG文件包含的收发方向设置
    SUPPER_CMD_CONSOLE_LOG = 6,    // 控制台日志分模块差异化配置使能显示
    SUPPER_CMD_LOG_CHANNEL = 7,    // 远程LOG通道-设备底层输出协议使能
    SUPPER_CMD_LOG_FRAME = 8,      // 远程LOG通道-设备报文输出格式使能
    SUPPER_CMD_SERVER_IP = 9,      // 服务器IP地址设置
    SUPPER_CMD_SERVER_PORT = 10,   // 服务器端口设置
    SUPPER_CMD_LOCAL_NETIF = 11,   // 本地网络接口设置
    SUPPER_CMD_OTA_START = 12,     // OTA升级开始
    SUPPER_CMD_CAN_LOG_SAVE = 13,  // CAN LOG触发存储机制
    
    SUPPER_CMD_MAX = 20,                
};

enum {
    // 0-无OTA操作，1-OTA升级准备中，2-OTA升级中，3-OTA成功，4-OTA失败，5-OTA取消
    DEVASSIST_OTA_IDLE = 0,
    DEVASSIST_OTA_READY,
    DEVASSIST_OTA_DOING,
    DEVASSIST_OTA_SUCCESS,
    DEVASSIST_OTA_FAIL,
    DEVASSIST_OTA_EXIT,
};

typedef struct
{
    uint16_t cmd;
    uint32_t value;
} supper_cmd_t;

typedef struct
{
    uint16_t mode;
    supper_cmd_t cmd_list[SUPPER_CMD_MAX];  // 增加到20个元素，支持命令10000-10019
    uint16_t ota_status; // 0-无OTA操作，1-OTA升级准备中，2-OTA升级中，3-OTA成功，4-OTA失败，5-OTA取消
} supper_control_t;

typedef struct
{
    g_st_FlagTime st_FlagTime;
    rtc_time_t rtc_time; // 系统本地时间，已叠加时区偏移
    BitData_RTC rtc_flag;

    time_t now; // 不含时区
    struct tm tm;

    // uint8_t time_init_flag;//时间初始化标志 0：未初始化/失败， 1：成功
    BitData_EVENT EventFlag;
    volatile uint8_t Timer_1msCount;
    volatile uint8_t Timer_5msCount;
    volatile uint8_t Timer_10msCount;
    volatile uint8_t Timer_50msCount;
    volatile uint8_t Timer_100msCount;
    volatile uint8_t Timer_500msCount;
    volatile uint8_t Timer_1sCount;
    volatile uint8_t Timer_1MinuteCount;

    BitData_PARAMETER_Set SetDataWrFlag; // SetData结构体内变量写入
    BitData_FILE_SET flasWrFlag;         // flah文件写标志
    uint64_t MCUPoweronCnt;               // ESP32上电后计数器，秒单位
    BitData_INV_FILE_SET INVFlasWrFlag;

    BitData_IOT_Status_Flag IOT_Status_Flag;

    BitData_MODBUS_CMD ModbusCmdFlag;
    uint8_t uart_to_arm_read_state;   // 1-收到modbus03rtn，准备执行下次发送 0-等待rtn
    uint8_t iot_link_change;          // iot连接状态 0:无 1：需要下发
    uint8_t iot_smart_change;          // iot智能控制状态 0:无 1：需要下发

    uint8_t net_point_Comein;        //	新设备发现/接入，需要立刻上报更新
    uint8_t modbus_self_report_ble;  // 蓝牙上报标志 0-空闲, else-收到上级读取21000绑定帧标志(1:蓝牙版本,3:升级状态)
    uint8_t modbus_self_report_mqtt; // mqtt上报标志
    uint8_t bind_state_ask;          // 0-空闲, else-收到上级读取21000绑定帧标志(1:蓝牙版本,3:升级状态)

    LOG_FAULT_STRUCT inv_log_fault_info; // 故障信息暂存

    uint32_t file_nums; // 文件数

    uint8_t authenticate_state;
    uint8_t ble_authenticate_buf_status;
    uint16_t ble_encrypt_info_flag;

    uint8_t Step_can_dev_parallel; // 1-triger,2-report SN,3-finish
    uint8_t Step_can_Cnt;          // step can的秒计数器
    //	uint8_t MonSnSame;//CAN  SN重复
    uint8_t Addr_can_Attr; // Can并机排序属性

    uint8_t Addr_can_master; // CAN并机地址，主设备,1-master

    uint8_t Addr_can_Buff;  // can并机排地址Addr_can_self的临时地址变量,0~(max-1)
    uint8_t Addr_can_Small; // 动态地址变量,0~(max-1)，由于每次最小地址标志基于0x5心跳 重新计算，因此即使最小主模块拔出，次小编号REC会接管主模块工作
    uint8_t Addr_can_self;  // 本MON CAN地址编号：0~(max-1)
    uint8_t Addr_can_slave; // 0:无效   else:总线上有优先级更高的IOT设备
    uint32_t Addr_can_slave_timeout;

    uint64_t INV_SN_can[DCDC_MAX_NUM + 1]; // can总线获取SN，用于并机地址排序  INV_CAN_MAX_POINT,IOT排序
    uint16_t Addr_can_Parallel_Interval;   // Can并机排序操作等待时间

    int8_t T_delay_iot_para_can_wr; // 因新设备出现，需要同步IOT配置参数的倒计时，在设备新增时刻赋非0值; 0-时间到，准备发送；-1-无效
    uint8_t MonSnSame;              // CAN  SN重复

    // can
    uint8_t Can_beta_block_step;                //
    can_poll_struct can_poll[MAX_NUM_CAN_TYPE]; //
    uint8_t Can_beta_block_type;                // 用于自己主动发can type block块读取的can type，在 tx fun=0x63赋值

    uint8_t online_Dcdc_num; //

    uint8_t online_Inv_num;   //
    uint8_t online_Pack_num;  //
    uint8_t online_Iot_num;   //
    uint8_t online_ACHUB_num; //
    uint8_t online_DCHUB_num; //
    uint8_t online_DCHUB_bit; // 按位记录DCHUB在线状态
    uint8_t online_AT1_num;   //
    uint8_t online_EBOX_num;  //
    uint8_t ota_happen;       // 1-正在升级

    uint8_t iot_can_set_flag;  // 需要给CAN设备下发set参数
    uint8_t inv_can_set_flag;  // 需要给CAN设备下发set参数
    uint8_t pack_can_set_flag; // 需要给CAN设备下发set参数

    uint16_t Can_beta_block_send_timeout_Cnt;                  // 用于can type block块间隔延时
    uint8_t BLE_WIFI_to_Can_Cnt;                               // 队列计数器

    /** 无线设备间协议有关 */
    uint8_t Step_dev_discovery; // 1-triger,2-report SN,3-finish
    // wlcc_dev_info_t discovery_net_Info[NET_WIFI_MAX_POINT];//同一WIFI路由器下面所有我司设备节点信息（不含子节点）：S1(16) +MIX WIFI(20)
    // wlcc_dev_info_t discovery_Info;//本地缓存，临时存储最新接收节点信息
    // uint8_t Topseq_index[NET_WIFI_MAX_POINT];//discovery_net_Info[].SN按照从小到大的排序序号，空SN在尾部
    // uint8_t Topnet_point_Num;//WIFI net 在线的设备节点
    // uint8_t Topseq_Invbat_index[NET_WIFI_INVBAT_POINT];//	
    // uint8_t Topseq_S1_index[NET_WIFI_S1_POINT];//从Topseq_index[]分离出的S1序号
    // uint8_t Topseq_mix_index[NET_WIFI_MIX_POINT];//从Topseq_index[]分离出的invbat序号
    // uint8_t Topnet_point_Num_S1;//WIFI net 在线的设备节点
    // uint8_t Topnet_point_Num_mix;//WIFI net 在线的设备节点
    // uint8_t Topnet_point_Num_invbat;//WIFI net 在线的设备节点
    // uint8_t Acphase_seq_S1[NET_WIFI_S1_POINT];//1-phase 1;2-phase 2;3-phase 3;其他无效
    // int16_t PowerSum_Acseq_S1[3]; // 分别表示 S1在三相AC功率线上的 功率，w

    /*升级文件信息记录*/
    current_ota_info_t current_ota_info;
    /*MQTT二次绑定标志*/
    // int8_t bind_update_flag;
    Need_new_version_flag_t need_new_version_flag; // 是否需要获取最新版本信息标志

    /*
    0-->无效（兼容之前产品 通过设备型号识别）
    1-->支持旧协议并包升级（依据本机地址升级）
    2-->支持新协议升级并包升级（支持广播升级）
    3-->支持新旧协议并包升级升级
    */
    uint8_t B300K_OTA_type_find; // 基于0x0CF5xx02发现有支持Can beta的BMS

    uint8_t If_ARM_SingleBoot;          // 底层ARM是否处于单boot状态
    uint8_t If_AC_HUB_SingleBoot;       // 底层ACHUB是否处于单boot状态
    uint32_t ARM_SingleBoot_version;    //
    uint32_t AC_HUB_SingleBoot_version; //
    /*来源于逆变的pack汇总上报 0：无 1：有*/
    uint8_t inv_pack_announce;
    WiFi_AP_mode_t wifi_ap_mode;

    uint32_t wifi_connect_timestemp;

    uint32_t last_login_cloud_time;
    uint32_t last_disconnect_cloud_time;
    uint32_t last_login_router_time;
    uint32_t last_disconnect_router_time;
    uint32_t last_ble_client_connect_time;
    uint32_t last_ble_client_disconn_time;
    uint32_t last_ble_server_connect_time;
    uint32_t last_ble_server_disconn_time;
    int8_t ble_server_signal_strength;

    uint32_t last_can_inv_rx_counter;
    uint32_t last_can_pack_rx_counter;
    uint32_t last_can_iot_rx_counter;
    uint32_t last_can_x_rx_counter;
    uint32_t last_net_frame_tx_counter;
    uint32_t last_net_frame_rx_counter;
    uint32_t last_ble_frame_tx_counter;
    uint32_t last_ble_frame_rx_counter;
    volatile uint32_t can_inv_rx_counter;
    volatile uint32_t can_pack_rx_counter;
    volatile uint32_t can_iot_rx_counter;
    volatile uint32_t can_x_rx_counter;
    volatile uint32_t net_frame_tx_counter;
    volatile uint32_t net_frame_rx_counter;
    volatile uint32_t ble_frame_tx_counter;
    volatile uint32_t ble_frame_rx_counter;

    uint32_t last_rtc_sync_ble;
    uint32_t last_rtc_sync_cloud;
    uint32_t last_rtc_sync_local;

    volatile uint32_t thread1_max_runtime_us;
    volatile uint32_t thread2_max_runtime_us;
    volatile uint32_t thread3_max_runtime_us;
    volatile uint32_t thread4_max_runtime_us;
    volatile uint32_t thread5_max_runtime_us;
    volatile uint32_t thread6_max_runtime_us;

    system_ota_info_struct system_ota;

    supper_control_t supper_control; // 超级命令，用于调试
} RealS_STRUCT;

#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 参数初始化
 * @note - 标定参数
 * @note - IOT参数
 * @note - 逆变参数
 * @note - 继电器参数
 */
void parameter_init(void);

/**
 * @brief 参数保存
 * @param type 参数类型
 *          - PARAM_TYPE_IOT IOT参数;
 *           - PARAM_TYPE_INV 逆变参数;
 *           - PARAM_TYPE_PACK 逆变pack参数;
 *           - PARAM_TYPE_RELAY 继电器参数;
 *           - PARAM_TYPE_DISASTER 灾害参数;
 *           - PARAM_TYPE_ALL 所有参数;
 */
int parameter_save(uint16_t type);

/**
 * @brief 参数初始化
 * @note - 继电器参数
 */
int clear_relay_parameter_file(void);

extern SetData_Can_TypeDef InvSetData;
extern Relay_SetData_TypeDef RelaySetData;
extern RealS_STRUCT reals;
extern disaster_save_param_t DisasterSaveData; // modbus表使用

#ifdef __cplusplus
}
#endif
