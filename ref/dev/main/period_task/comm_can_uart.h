#ifndef __COMM_CAN_UART_H__
#define __COMM_CAN_UART_H__
#include <stdint.h>
//#include "esp_log.h"
#include "esp_event.h"//EXT_RAM_BSS_ATTR 需要
//#include "esp_smartconfig.h"
#include "tou_relay_ctrl.h"


//#include "app_uart.h"
//
//#include <time.h>
//#include <sys/time.h>


#pragma pack(1)
typedef union {
    uint32_t all;
    struct {
        uint8_t wifi_sta : 1; // 1：联网,0：未联网
        uint8_t wifi_ap : 1;  // 1：连接,0：未连接
        uint8_t ble_connect : 1; // 1:连接,0：未连接
        uint8_t ble_mash : 1; // 1:连接,0：未连接 bit3:ble组网状态 0：断开；1：连接
        
        uint8_t _4g : 1; // 1:l连接,0：未连接
        uint8_t gps : 1; // 1:已定位,0：未定位
        uint8_t mqtt: 1; // 1:登录,0：未登录
        uint8_t wifi_mesh: 1; // 0: 断开； 1：连接
        
        uint8_t ble_mesh: 1; // 0: 断开； 1：连接
        uint8_t Eth_Stat: 1; //有线以太网 0: 断开； 1：连接

        uint8_t revd: 6; //

        uint8_t revd2: 8; //
        uint8_t revd3: 8; //        
    }bit;
}link_status;//state,beta modbus 11018

typedef union {
    uint16_t all;
    struct {
        uint16_t pairing_mode : 2; // bit1/0:配对模式设置 0- invalid;1-强制配对，关联指定的配对SN；2-自动配置（设备自行搜索配对，有误判概率）
        uint16_t temperature_unit : 2; // bit3/2:温度单位 0:invalid, 1:℃, 2:℉
        uint16_t ble_lcd_active_time : 4; // bit7/4: 0:无效（APP不显示）1：15s； 2：30s； 3:1分； 4：5分；5：常量； 6：跟随逆变器
        uint16_t reserved3 : 2; // bit9/8: 预留
        uint16_t reserved4 : 2; // bit11/10: 预留
        uint16_t reserved5 : 2; // bit13/12: 预留
        uint16_t reserved6 : 2; // bit15/14: 预留
    };
} lcd_mode_t;

typedef struct {
	union {
		uint16_t all;
		struct {
			uint16_t sta_passward_H32B_en : 8; // WIFI路由器密码高32字节有效标志,0xA5表示有效，否则无效
			uint16_t open_wifi_support : 1; // bit8: 是否支持开放式WiFi(无密码), 1-支持, 0-不支持
			uint16_t reserved : 7; // 预留
		};
	};
} sta_eanble_t;

typedef union {
    uint32_t all;
    struct {
		//1-enable;2-disable
        uint32_t wifi_enable : 2; // WiFi 开关;windy目前使用此bit，表示sta en,未使用 wifi_sta_enable;HMI使用，将错就错
        uint32_t revd1 : 2; // wifi-STA开关
 		uint32_t wifi_ap_enable : 2;    // WiFi-AP开关 与WIFI_mesh互斥,优先wifi_ap_enable
        uint32_t ble_enable : 2; // 蓝牙开关
        
        uint32_t ble_adv_enable : 2; // 蓝牙广播开关
        uint32_t ble_mash_enable : 2; // 蓝牙组网开关
        uint32_t _4g_enable : 2;//0：关闭，1：开启
        uint32_t gps_enable : 2;

        uint32_t Eth_enable : 2;//有线以太网使能
        uint32_t WIFI_mesh : 2;//wifi mesh使能
        uint32_t revd2 : 2;    // WiFi-AP开关 与WIFI_mesh互斥,优先wifi_ap_enable
		uint32_t new_dev_discovery : 2;//新设备发现模式（combox启用）
		
        uint32_t wifi_mesh_mode : 2; //wifi mesh mode
        uint32_t webserver_en : 2;  //webserver使能
        uint32_t mdns_en : 2;   //mDNS使能
        uint32_t revd3 : 2;
		
    }bit;
}config_status;//cmd

typedef union {
	uint16_t all;//uint8_t
	struct {
		uint8_t thunder_enable : 2;
		uint8_t thunder_state : 2;
		uint8_t thunder_cancel : 2;
		uint8_t picture1 : 2;
	};
} thunder_ctrl_t;

typedef union {
	uint16_t all;
	struct {
		uint16_t power_off_count : 4; // bit0~3 断电次数时间跨度0:invalid, 1:每日，2，每周，3，每月，4，每年，5-至今
        uint16_t save_money : 4; // bit4~7 省钱参数时间跨度
        uint16_t revd : 8; // bit8~15 预留
	};
} Time_Span_t;

typedef struct {
    uint8_t app_temp_unit_set_flag : 1;  // APP温度单位设置标志: 0=未设置, 1=已设置
    uint8_t reserved : 7;                // 预留位
} temp_unit_control_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t ext_wifi_enable_num; // 低字节：当前设备支持的额外WIFI设置数 （0：无额外新增，1~N：额外支持N个）
    }bit;
}iot_wifi_mult_sta_flag_struct;

typedef union {
	uint16_t all;
	struct {
//		"0-invalid;1-enable;2-disable;
		uint8_t trigger_regroup_IOT : 2;//bit1/0:触发IOT模块并机排序
		uint8_t trigger_regroup_INV : 2;//bit3/2:触发INV模块并机排序
		uint8_t trigger_regroup_PACK : 2;//bit5/4:触发PACK模块并机排序
		uint8_t trigger_BIND : 2;//bit7/6:设备绑定触发命令（生产、运维超级用户操作，条件：在单机条件下，将IOT硬件和逆变器硬件关联）

		uint8_t revd : 8;//bit15~6:预留
	}bit;
} Struct_IOT_Enable_mix1;

typedef union {
    uint16_t all;
    struct {
        uint8_t spi_flash_fault : 1; // SPI flash故障
        uint8_t eeprom_fault : 1; // EEPROM故障
        uint8_t sub_1ghz_fault : 1;  // Sub 1GHz(CC1312)通信超时
        uint8_t _4g_fault : 1; // 4G模块通信超时
        uint8_t meter_fault : 1; // meter(BL6552)通信超时
        uint8_t arm_fault : 1; // ARM(IOT板内)通信超时
        uint8_t rtc_fault : 1; // RTC(IOT板内)通信故障
        
        uint16_t revd : 9; //
    }bit;
}iot_fault_struct;

/**
 * @brief Type = 0x1 ,iot
 *
 windy 20240814:168 len
 */

#define	IOT_INFO_STRUCT_COMMON_REG_11000_0x1 \
	    char 	iot_type[12];/*11000~11005*/\
		uint64_t iot_sn;   /*11006~11009 sn码*/\
		uint64_t safe_code; /*11010~11013 安全码*/\
		uint32_t software_ver; /* 软件版本号*/\
		uint16_t revd1[2];\
		link_status   link;    /* IOT功能连接状态*/\
		uint8_t sta_ipv4[4];\
		uint8_t sta_gw[4];\
		uint8_t sta_mask[4];\
		uint16_t sta_rssi; \
		uint8_t sta_mac[6];\
		uint8_t ble_mac[6];\
		uint16_t mobile_rssi; \
		uint8_t MqttConnectMode;/*0：无效 1：以太网 2：WIFI 3：4G*/\
		uint8_t revd2;\
		iot_fault_struct iot_fault;/*iot主要告警*/\
		uint16_t revd3[3];\
		uint16_t can_bus_error; /* 11039 IOT CAN总线错误*/\
		uint16_t wifi_disconnect_reasion; /* 11040 WIFI断开原因*/\
		uint8_t IP_4G[4]; /* 11041*/\
		uint8_t IP_ETH[4]; /* 11043*/\
		uint16_t period_report;/*数据上报服务器周期（休眠周期，4G模块省流量目的）	2	秒数	R/W*/\
		uint32_t time_lastreport;/*最后联网（设备与服务器通讯）时间 2	基于1970年初始日期的秒计数器； R  11045~11046*/\
		uint8_t signal_4G;/*信号强度	1	L8bit:信号强度（0-31或者99）	 11047*/\
		uint8_t SIM_ready;/*  */\
		uint32_t GPS_X;/*uint16_t	11048~11049*/\
		uint32_t GPS_Y;/*uint16_t	11050~11051*/\
		uint8_t IMEI[20];/*IMEI号码(移动设备识别号码) 10	15~17位数字,ASCII	R*/\
		uint8_t IMSI[20];/*IMSI号码(移动用户识别号码) 10	15位数字,ASCII	R*/\
		uint8_t ICCID[20];/*ICCID号码(集成电路/SIM卡识别号码)	10	20个字符组成（19位数字+最后1位英文字母）,ASCII */\
		uint16_t Bind_SN;/*绑定群组代号*/\
		uint16_t wifi_Mesh_Id[3];/*用于区分不同MESH网络；(顶层网络名称，当前网络实际值) 11083~11085*/\
		uint16_t eth1_Mac[3];/*以太网MAC地址 11086~11088*/\
		uint16_t connected_BLE_Server_SN;/*作为BLE Client,蓝牙配对已接入的Server广播SN 11089*/\
		uint16_t revd4[10];/*预留 11090~11099*/\
		uint16_t powerOff_Nums;/*断电次数 11100*/\
		uint32_t saveMoneyNums;/*省钱参数 11101~11102*/\
        iot_smart_ctrl_struct iot_smart_control;\
        uint16_t DI_State;\
        iot_wifi_mult_sta_flag_struct wifi_mult_sta_flag;\
        char wifi_station_ssid_now[32];\
        uint16_t revd5[10];



typedef struct {
    /*11000~11015*/
    char     iot_type[12];//11000~11005
    uint64_t iot_sn;   //11006~11009 sn码
    uint64_t safe_code; //11010~11013 安全码
    uint32_t software_ver; // 软件版本号
    /*11016~11017*/
    uint16_t revd1[2];
    /*11018~11019*/
    //config_status1 cfg;     // IOT功能开启状态
    link_status   link;    // IOT功能连接状态
    /*11020~11029*/
    uint8_t sta_ipv4[4];
    uint8_t sta_gw[4];
    uint8_t sta_mask[4];
    uint16_t sta_rssi; //
    uint8_t sta_mac[6];
    /*11030~11033*/
    uint8_t ble_mac[6];
    uint16_t mobile_rssi; //
    /*11034~11038*/
	uint8_t MqttConnectMode;//0：无效 1：以太网 2：WIFI 3：4G
    uint8_t revd2;
	iot_fault_struct iot_fault;//iot主要告警
    uint16_t revd3[3];
    // uint16_t longitude_direct; // 经度纬度
    // uint32_t lagitue_value; //纬度
    // uint32_t longitude_value; // 经度
    /*11039~11040*/
    uint16_t can_bus_error; // 11039 IOT CAN总线错误
    uint16_t wifi_disconnect_reasion; // 11040 WIFI断开原因
    uint8_t IP_4G[4]; // 11041
    uint8_t IP_ETH[4]; // 11043

	uint16_t period_report;//数据上报服务器周期（休眠周期，4G模块省流量目的）	2	秒数	R/W
	uint32_t time_lastreport;//最后联网（设备与服务器通讯）时间	2	基于1970年初始日期的秒计数器；	R //11045~11046

	uint8_t signal_4G;//信号强度	1	L8bit:信号强度（0-31或者99）    //11047
	/*
	bit0：SIM卡是否在位
	bit7~1:预留	R
	*/
	uint8_t SIM_ready;//  

	uint32_t GPS_X;//uint16_t   11048~11049
	uint32_t GPS_Y;//uint16_t   11050~11051
	uint8_t IMEI[20];//IMEI号码(移动设备识别号码)	10	15~17位数字,ASCII	R
	uint8_t IMSI[20];//IMSI号码(移动用户识别号码)	10	15位数字,ASCII	R
	uint8_t ICCID[20];//ICCID号码(集成电路/SIM卡识别号码)	10	20个字符组成（19位数字+最后1位英文字母）,ASCII	
//windy 20240808 add
	uint16_t Bind_SN;//绑定群组代号  11082

	uint16_t wifi_Mesh_Id[3];/*用于区分不同MESH网络；(顶层网络名称，当前网络实际值) 11083~11085*/
	uint16_t eth1_Mac[3];/*以太网MAC地址 11086~11088*/
	uint16_t connected_BLE_Server_SN;/*作为BLE Client,蓝牙配对已接入的Server广播SN 11089*/
	uint16_t revd4[10];/*预留 11090~11099*/
	uint16_t powerOff_Nums;/*断电次数 11100*/
	uint32_t saveMoneyNums;/*省钱参数 11101~11102*/
	/*11103~11104*/
	iot_smart_ctrl_struct iot_smart_control;//TOU控制有效状态 注：个别项目里下级ARM需要这个标志
	/*11105*/
	uint16_t DI_State;
	/*11106*/
	iot_wifi_mult_sta_flag_struct wifi_mult_sta_flag;//多WIFI标志
	/*11107~11122*/
	char wifi_station_ssid_now[32];     //当前连接的wifi ssid
	uint16_t revd5[4]; // 11123~11126
	char BLE_Server_Type[32]; //11127~11142已接入的设备蓝牙广播SN
	uint16_t revd6[10];
}MOD_STRUCT_reg11000;

/**
 * @brief Type = 0x1 ,iot
 *
 20240814 windy 1999 len
 */

#define	IOT_SET_STRUCT_COMMON_REG_12000_0x2 \
	uint16_t factory;			 /*12000 恢复出厂设置*/\
	uint16_t wifi_sta_auth; 	 /*12001 wifi sta 加密认证方式*/\
	char wifi_sta_ssid[32]; 	/*12002~12017 WiFi sta ssid*/\
	char wifi_sta_password[64]; /*12018~12049 wifi sta 密码*/\
	sta_eanble_t sta_enable;	/*12050 bit0~7:密码高32字节有效标志(0xA5有效); bit8:开放式WiFi支持(1-支持)*/\
	uint16_t revd1[16]; 	 /*12051~12066 预留*/\
	char could_dns[128];		/*12067~12130 服务器地址*/\
	char mobile_apn[60];		/*12131~12160 apn*/\
	config_status on_off;/*12161~12162*/\
	thunder_ctrl_t thunder_ctrl;/*12163*/\
	uint32_t period_report;/*12164~12165 数据上报服务器周期（休眠周期，4G模块省流量目的）	2	秒数	R/W*/\
	uint64_t Network_name_BR_set;/*12166~12169*/\
	Struct_IOT_Enable_mix1 IOT_Enable_mix1;/*12170*/\
	uint16_t IOT_Enable_mix2;/*12171*/\
	uint16_t Protocol_3r_Enable_mix1;/*12172*/\
	uint16_t Protocol_3r_Enable_mix2;/*12173*/\
    uint32_t ETH1_netmask;		/*12174~12175 */\
    uint32_t ETH1_gw;           /*12176~12177 */\
    uint32_t ETH1_DNS1;         /*12178~12179 */\
    uint32_t ETH1_DNS2;         /*12180~12181 */\
    uint32_t Internal_Server_IP;/*12182~12183 */\
    uint16_t Internal_Server_Port; /*12184 */\
    uint16_t revd2[20];				/*12185~12204 */\
    lcd_mode_t LCD_Mode;          /*12205 */\
    Time_Span_t Time_Span;         /*12206 */\
    uint32_t Wired_Ethernet_IP;		/*12207~12208 */\
    uint16_t revd3[742];/* 12209~12950 */ \
    uint16_t wifi_ap_auth;/*12951 */\
    char wifi_AP_ssid[32];/*12952~12967*/\
    char wifi_AP_password[64];/*12968~12999 */

#define	IOT_SET_STRUCT_COMMON_REG_file \
	uint8_t file_data_flag;			 /*文件数据标识*/\
	uint8_t protocol_ver;			 /*载荷协议版本*/\
	uint8_t state_cod;			 	 /*场景代码*/\
	uint16_t RequestId;			 	 /*RequestId*/\
	uint16_t data_num;			 	 /*第几包数据*/\
	uint16_t data_total;			 /*数据包总数*/\
	uint8_t rsvd;			 		 /*预留*/\
	uint8_t slave_addr;			 	 /*从机地址*/\
	uint8_t func_code;			 	 /*功能码*/\
	uint32_t data_start_pos;		 /*文件数据开始位*/\
	uint32_t available_data;		 /*本次指令有效数据字节数*/\
	uint8_t *data_temp;		 		 /*有效数据*/\
	uint16_t crc;			 		 /*CRC*/



typedef struct {
    uint16_t factory;            //12000 恢复出厂设置
    uint16_t wifi_sta_auth;      //12001 wifi sta 加密认证方式
    char wifi_sta_ssid[32];     //12002~12017 WiFi sta ssid
    char wifi_sta_password[64]; //12018~12049 wifi sta 密码
    sta_eanble_t sta_enable;    //12050 bit0~7:密码高32字节有效标志(0xA5有效); bit8:开放式WiFi支持(1-支持)
    uint16_t revd1[16];      //12051~12066 预留
    char could_dns[128];        //12067~12130 服务器地址
    char mobile_apn[60];        //12131~12160 apn
	config_status on_off;//12161~12162
	thunder_ctrl_t thunder_ctrl;//12163

	uint32_t period_report;//12164~12165 数据上报服务器周期（休眠周期，4G模块省流量目的）	2	秒数	R/W
	uint64_t Network_name_BR_set;//12166~12169
	Struct_IOT_Enable_mix1 IOT_Enable_mix1;//12170
	uint16_t IOT_Enable_mix2;//12171
	uint16_t Protocol_3r_Enable_mix1;//12172
	uint16_t Protocol_3r_Enable_mix2;//12173

	uint32_t ETH1_netmask;		//12174~12175
    uint32_t ETH1_gw;           // 12176~12177
    uint32_t ETH1_DNS1;         // 12178~12179
    uint32_t ETH1_DNS2;         // 12180~12181
    uint32_t Internal_Server_IP;// 12182~12183
    uint16_t Internal_Server_Port; // 12184
    // char Set_BLE_Server_SN[20]; // 12185~12194
    // char Connected_BLE_Client_SN[20]; // 12195~12204
	uint16_t revd2[20];				//12185~12204
    lcd_mode_t LCD_Mode;          // 12205
    Time_Span_t Time_Span;         // 12206
	uint32_t Wired_Ethernet_IP;		//12207~12208

//tbd :can beta协议 的多字节读和写 协议限制最大块字节数：255*6=1530
    //uint16_t revd2[744];//12174~12950
	uint16_t revd3[742];//12209~12950
    uint16_t wifi_ap_auth;//12951
    char wifi_AP_ssid[32];//12952~12967
    char wifi_AP_password[64];//12968~12999
}MOD_STRUCT_reg12000;

#pragma pack()

#endif

