#ifndef __IOT_PERIOD_TASK_H__
#define __IOT_PERIOD_TASK_H__
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

//#include "esp_log.h"
#include "esp_event.h"//EXT_RAM_BSS_ATTR 需要
//#include "esp_smartconfig.h"
#include "can_data.h"
#include "pack_type.h"

#include "comm_define.h"
#include "app_uart.h"
#include "dev_data_record.h"
#include "udp_multicast.h"

#include "uart_device_process.h"
#include "inv_type.h"
#include "comm_define.h"


#pragma pack(1)



#define INV_CAN_MAX_POINT (INV_MAX_NUM*DEV_MAIN_NODE_MAX)//10//can inv最大节点数量 


#define WS_TO_100WH_UNIT   3600*100		//0.1KWH 3600s*100w
#define KWH_INFO_ALONE			1		//单机电量信息体序号
#define KWH_INFO_SUM			0		//并机汇总电量信息体序号
#define KWH_INFO_MAX_NUM			2		//电量信息体最大数量

#define LOG_DATA_PROTOCOL_VER		1		//协议版本号 当前版本为1


typedef union//2Byte
{
   // uint8_t all;
    uint16_t word;
    struct structTime
    {
        uint8_t b1FlagSys10ms          :1; // 判断函数10ms定时,10ms不准确  ,period_task线程
        uint8_t b1FlagSys50ms          :1; //        
        uint8_t b1FlagSys100ms         :1; //  
        uint8_t b1FlagSys500ms         :1; //  
        
        uint8_t b1FlagSys1s            :1; //  
        uint8_t b1FlagSys1Minute       :1; // 判断函数1分钟定时        
        uint8_t b1FlagSys10ms_2        :1; // 判断函数10ms定时,can tx 线程
        uint8_t b1FlagSys50ms_can_hi_level        :1; // can线程使用       
        
        uint8_t flagA_1s       :1;// set和clean不在一个线程,server2 MQTT
        uint8_t b1FlagSys1s_slow_period_task       :1;// 
        uint8_t flagC_1s       :1;// 
        uint8_t b1FlagSys100ms_2        :1; // can线程使用       

		
        uint8_t b1FlagSys100ms_can_hi_level        :1; // can线程使用       
        uint8_t b1FlagSys1s_can_hi_level        :1; // can线程使用       
        uint8_t b1FlagSys100ms_slow_period_task       :1;// 
        uint8_t revd4       :1;// 
        
		

    }bits;
}g_st_FlagTime;

typedef union//2Byte
{
   // uint8_t all;
    uint16_t word;
    struct struct_Flag1
    {

	 	uint8_t uart_Sub1G_fast_tx	:1;//快速发送标志
		uint8_t udp_fast_tx	:1;//快速发送标志       

		uint8_t cmd_fast_net_to_dev	:1;//网络到下级uart/CAN的快速命令，写      
		uint8_t cmd_fast_ble_to_dev	:1;//网络到下级uart/CAN的快速命令，写      
		
		uint8_t uart_revd	:4;// 
		
        uint8_t revd4       :8;// 
    }bits;
}g_st_Flag1;


typedef union//2Byte
{
   // uint8_t all;
    uint32_t word;
    struct struct_FlagLED
    {

	 	uint8_t LED_Net	:8;// 
	 	uint8_t LED_COMM	:8;// 
	 	uint8_t LED_Fault	:8;// 
	 	uint8_t revd		:8;// 

    }bits;
}g_st_FlagLED;



typedef union
{
    uint64_t Byte4;   // 
    struct
    {
        uint8_t SetDataUpdate_all       : 1;//   
        uint8_t SetDataUpdate_historycnt       : 1;//  1-更新存储在EEPROM里面的历史记录
        uint8_t SetDataUpdate_SN       : 1;//  
        uint8_t SetDataUpdate_Event_cnt       : 1;//  

        uint8_t wifi_sta_ssid       : 1;//  
        uint8_t wifi_sta_password       : 1;//  
        uint8_t thunder_ctrl       : 1;//  
        uint8_t on_off       : 1;//  
        
		uint8_t could_dns		 : 1;//  
		uint8_t mix			 : 1;//  临时不区分部分
		uint8_t Net_Server_secret			 : 1;//
		uint8_t Net_Server_address			 : 1;//


        uint8_t app_password        			: 1;//  
        uint8_t ctrl_meter       			: 1;//  
        uint8_t SetDataUpdate_DSP        			: 1;//  
        uint8_t ctrl_feedback        			: 1;//  

        uint16_t bind			: 1;//本地存储的SN绑定表格更新
        uint16_t res_2005		: 1;//存储上次写入时区信息
        uint16_t data_report_cycle : 1;
        uint16_t SetDataUpdate_invInfo_cnt			: 1;

        uint16_t ble_protocol   : 1;
        uint16_t ble_client     : 1;
        uint16_t ble_server     : 1;
        uint16_t wifi_sta_rssi  : 1;

        uint16_t wifi_mul_sta   : 1;
        uint16_t wifi_sta1      : 1;
        uint16_t wifi_sta2      : 1;
        uint16_t wifi_sta3      : 1;

        uint16_t wifi_ap        : 1;
        uint16_t mqtt           : 1;
        uint16_t modbus_tcp     : 1;
        uint16_t bind_sn			: 1;

        uint16_t kwh_mix		: 1;//u32少量 kwh存储到eeprom
        uint16_t meter_cfg : 1; // 电表参数变化
		uint16_t SetTimeZone			: 1;
        uint16_t Set_Host_Ip			: 1;

        uint16_t mode_set           : 1; // 断码屏设置
        uint16_t time_span          : 1; // 时间跨度设置
        uint16_t saved_money        : 1;
        uint16_t sleep_ctx          : 1; // 休眠上下文持久化
    } sBit;
} BitData_PARAMETER_Set;//NVS data set flag  BitData_Eprom;//EEPROM set flag



typedef union
{
    uint16_t Byte2;   // 
    struct
    {
//        uint16_t kwh_hist_PV		: 1;// 
//        uint16_t kwh_hist_Grid			: 1;
//        uint16_t kwh_hour_PV			: 1;
//        uint16_t kwh_hour_Grid			: 1;
        uint16_t revd4			: 4;

        uint16_t log_fault      		: 1;//INV历史记录产生
        uint16_t log_event			: 1;//IOT事件记录产生
        uint16_t iot_event			: 1;//not need
        uint16_t log_invdetailedinfo			: 1; //逆变详细数据

        uint16_t revd9			: 1;
        uint16_t revd10			: 1;
        uint16_t set_data_pack			: 1;//eeprom 参数存储
        uint16_t set_data_inv			: 1;

        uint16_t year_change			: 1;//用于kwh 统计hour，需要基于年变化循环覆盖
        uint16_t month_change			: 1;
        uint16_t day_change				: 1;
        uint16_t hour_change			: 1;
    } sBit;
} BitData_FILE_SET;//  set flag




typedef union
{
    uint16_t Byte2;   // 
    struct
    {
		uint16_t year_DCLoad_Total 			: 1;// 
		uint16_t year_ACLoad_Total 			: 1;
		uint16_t year_Pv_Total 				: 1;
		uint16_t year_GridChgin_Total		: 1;
		
		uint16_t year_GridFeedback_Total	: 1;// 
		uint16_t year_PVToload_Total		: 1;// 
		uint16_t year_PackDsg_Total			: 1;// 
        uint16_t hour_file					: 1;

		uint16_t year_DCLoad_Total_Sum 			: 1;// 
		uint16_t year_ACLoad_Total_Sum 			: 1;
		uint16_t year_Pv_Total_Sum 				: 1;
		uint16_t year_GridChgin_Total_Sum		: 1;
		
		uint16_t year_GridFeedback_Total_Sum	: 1;// 
		uint16_t year_PVToload_Total_Sum		: 1;// 
		uint16_t year_PackDsg_Total_Sum			: 1;// 
        uint16_t hour_file_Sum					: 1;

    } sBit;
} BitData_KWH_FILE_SET;//  set flag




typedef union
{
    uint16_t Byte2;   // 
    struct
    {
        uint16_t RTC_valid_from_NTP		: 1;// 
        uint16_t RTC_valid_from_APP		: 1;
        uint16_t RTC_valid_from_CAN		: 1;
        uint16_t RTC_valid_from_SERVER	: 1;

        uint16_t RTC_set_IOT_to_INV     : 1;// 
        uint16_t RTC_set_APP_to_IOT		: 1;// 
        uint16_t RTC_set_SERVER_to_IOT  : 1;
		uint16_t Time_valid             : 1;

        uint16_t revd9			: 4;
        uint16_t revd11			: 4;
    } sBit;
} BitData_RTC;// RTC切换标志


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
        uint8_t revdx2        			: 2;//

        uint8_t meter_cfg0               : 1;    // meter相关参数变化
        uint8_t meter_cfg1               : 1;    // meter相关参数变化
        uint8_t meter_cfg2               : 1;    // meter相关参数变化
        uint8_t meter_cfg3               : 1;    // meter相关参数变化

        uint8_t mode_set                 : 1;    // 12205 断码屏设置
        uint8_t time_span                : 1;    // 12206 时间跨度
        uint8_t ctrl_power              : 1;    // 远程开关
        uint32_t revdx3                  : 25;
    } sBit;
} BitData_MODBUS_CMD;//来自BLE和WIFI的命令操作区分




//4G模块
enum 
{
    G4_MODULE_INIT = 0,
    G4_MODULE_GET_IN_DTATA_MODE,//ATD 进入数据模式
	G4_MODULE_GET_IN_CMD_MODE,//进入AT指令模式
	GET_IP_4G_READY,//连接到网络互联网
	LOST_IP_4G_READY,//连接到网络互联网
//    RESOLVE_DNS_4G,
//    TCP_CONNECT_4G,
//    TCP_SYNC_4G,
//    MQTT_START_4G,
//    MQTT_RECV_4G,//已完成 订阅主题流程
//    MQTT_STBY_4G,
	G4_MODULE_RESET,//需要重启4G模块
    
};

//SPI ethernet
enum 
{
    ETH_INIT = 0,
    ETH_MAC_LINK_UP,//	ETH PHY接入网线，底层通畅	
    ETH_GET_IP_READY,//已连接路由器局域网，并获得本地IP
    ETH_LOST_IP,

    
};



typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/
    int16_t AphB;/*40004 I*/
    int16_t AphC;/*40005 I*/
    uint16_t A_SF;/*40006*/
    int16_t PhV;/*40007 V*/
    int16_t PhVphA;/*40008 V*/
    int16_t PhVphB;/*40009 V*/
    int16_t PhVphC;/*40010 V*/
    int16_t PPV;/*40011 V*/
    int16_t PhVphAB;/*40012 V*/
    int16_t PhVphBC;/*40013 V*/
    int16_t PhVphCA;/*40014 V*/
    uint16_t V_SF;/*40015*/
    int16_t Freq;/*40016 F*/
    uint16_t Freq_SF;/*40017*/
    int16_t Total_W;/*40018 W*/
    int16_t WphA;/*40019  W*/
    int16_t WphB;/*40020 W*/
    int16_t WphC;/*40021 W*/
    uint16_t W_SF;/*40022*/

}AT403_InfoStruct1;

typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/


}AT403_InfoStruct2;

typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/


}AT403_InfoStruct3;

typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/


}AT403_InfoStruct4;


typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/


}Sub1GHz_InfoStruct1;
typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/


}Sub1GHz_InfoStruct2;

typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/


}RS485_InfoStruct1;


typedef union // 电表1Grid-40000
{
    uint16_t AT403_ModbusData1[sizeof(AT403_InfoStruct1)/2];//内容与modbus顺序反
    AT403_InfoStruct1   AT403_DataStruct1;//UINT16 RAM是 L在前
}AT403_UNION1;

typedef union // 电表1Grid-40000
{
    uint16_t AT403_ModbusData2[sizeof(AT403_InfoStruct2)/2];//内容与modbus顺序反
    AT403_InfoStruct2   AT403_DataStruct2;//UINT16 RAM是 L在前
}AT403_UNION2;

typedef union // 电表1Grid-40000
{
    uint16_t AT403_ModbusData3[sizeof(AT403_InfoStruct3)/2];//内容与modbus顺序反
    AT403_InfoStruct3   AT403_DataStruct3;//UINT16 RAM是 L在前
}AT403_UNION3;

typedef union // 电表1Grid-40000
{
    uint16_t AT403_ModbusData4[sizeof(AT403_InfoStruct4)/2];//内容与modbus顺序反
    AT403_InfoStruct4   AT403_DataStruct4;//UINT16 RAM是 L在前
}AT403_UNION4;


typedef union // 电表1Grid-40000
{
    uint16_t Sub1GHz_ModbusData1[sizeof(Sub1GHz_InfoStruct1)/2];//内容与modbus顺序反
    Sub1GHz_InfoStruct1   Sub1GHz_DataStruct1;//UINT16 RAM是 L在前
}Sub1GHz_UNION1;
typedef union // 电表1Grid-40000
{
    uint16_t Sub1GHz_ModbusData2[sizeof(Sub1GHz_InfoStruct2)/2];//内容与modbus顺序反
    Sub1GHz_InfoStruct2   Sub1GHz_DataStruct2;//UINT16 RAM是 L在前
}Sub1GHz_UNION2;

typedef union // 电表1Grid-40000
{
    uint16_t RS485_ModbusData1[sizeof(RS485_InfoStruct1)/2];//内容与modbus顺序反
    RS485_InfoStruct1   RS485_DataStruct1;//UINT16 RAM是 L在前
}RS485_UNION1;



typedef union
{
	uint16_t msgaddid[3];
	#if 0
	struct
	{
		unsigned ADDIDL	:16;		/* reserved */
		unsigned ADDIDH	:16;		/* reserved */
		unsigned ADDADD	:8;		/* com attribute */
		unsigned ADDOK	:8;		/* 1:continuous frame */
	}msgaddbit;
	#endif
}frame_Sn;//设备节点SN信息

typedef union
{
	uint8_t MAC[8];
	uint64_t MAC_64;
//	uint8_t IP[4];//IPV4,顺序填充
	
	#if 0
	struct
	{
		unsigned ADDIDL	:16;		/* reserved */
		unsigned ADDIDH	:16;		/* reserved */
		unsigned ADDADD	:8;		/* com attribute */
		unsigned ADDOK	:8;		/* 1:continuous frame */
	}msgaddbit;
	#endif
}MAC_UNION;//设备节点SN信息


typedef struct
{

	MAC_UNION	mac_union;//设备节点MAC信息
	uint8_t net_point_online;//2-在线；1-短期不在线；0-不在线
	uint16_t net_point_TimeOut_cnt;//在线的设备节点


}NET_POINT_InfoStruct;

typedef struct
{

	uint8_t	is_ota;//子设备节点升级状态 1:正在升级 0：未升级/升级中出错忽略
	uint8_t packed_rtn;    //子设备xmodem回复
	uint8_t write_block_retries;//子设备nack或无回复后重发次数


}NET_POINT_OTA_InfoStruct;

typedef struct
{

	uint16_t P_set_level;//
	uint16_t P_real;//
	int16_t Pdelta;//P_set_level -P_real;//
	uint16_t Pability_remain;//P_rate-P_set_level，额定剩余发电能力

}NET_POINT_POWER_InfoStruct;

typedef struct
{

	uint8_t MAC[6];//IP
	uint8_t SN[10];//L8=SN,H2=type seq
	uint16_t priority;//
	uint8_t parallel_seq;//
	uint8_t net_point_online;//1-在线；0-不在线
	uint16_t net_point_TimeOut_cnt;//在线的设备节点	
	uint8_t *ptr_modbus_data;
	uint16_t modbus_data_len;//指向的modbus 结构体单元长度字节数
	
}DISCOVERY_POINT_InfoStruct;


///////RS485电表
typedef struct
{
    uint16_t ID;/*40000*/
    uint16_t LENGTH;/*40001*/
    int16_t Total_A;/*40002 I*/
    int16_t AphA;/*40003 I*/
    int16_t AphB;/*40004 I*/
    int16_t AphC;/*40005 I*/
    uint16_t A_SF;/*40006*/
    int16_t PhV;/*40007 V*/
    int16_t PhVphA;/*40008 V*/
    int16_t PhVphB;/*40009 V*/
    int16_t PhVphC;/*40010 V*/
    int16_t PPV;/*40011 V*/
    int16_t PhVphAB;/*40012 V*/
    int16_t PhVphBC;/*40013 V*/
    int16_t PhVphCA;/*40014 V*/
    uint16_t V_SF;/*40015*/
    int16_t Freq;/*40016 F*/
    uint16_t Freq_SF;/*40017*/
    int16_t Total_W;/*40018 W*/
    int16_t WphA;/*40019  W*/
    int16_t WphB;/*40020 W*/
    int16_t WphC;/*40021 W*/
    uint16_t W_SF;/*40022*/

}GridMeterInfoStruct;

typedef union // ADL400电表1Grid-40000
{
    uint16_t GridModbusData[sizeof(GridMeterInfoStruct)/2];//内容与modbus顺序反
    GridMeterInfoStruct   GridDataStruct;//UINT16 RAM是 L在前
}GRID_METER_UNION;



typedef struct {
    uint8_t revd;//slave_addr;//  
    uint8_t group_addr;//  绑定群组序号（用不同的非零数字表示设备属于不同的群组；下级可只区分，而自己编号；0-表示解绑此SN）;暂定从201开始排序
    uint16_t DevSn[5];// 	8B SN+2B type seq
}POINT_BIND_INFO_Store;

typedef union//2Byte
{
   // uint8_t all;
    uint16_t word;
    struct struct_Flag_EE_WR
    {

		uint8_t Wifi	:1;// 
		uint8_t server2	:1;// 
		uint8_t revd_1	:1;// 
		uint8_t revd_2	:1;// 
		
		uint8_t revd_xx	:4;// 
		
        uint8_t revd99       :8;// 
    }bits;
}g_st_Flag_EE_WR;

typedef struct {
    uint8_t dev_type;           // 设备型号
    uint8_t mfg_id;             // 制造商ID
    uint8_t dev_id[32];         // 设备唯一识别码
    uint16_t func;              // 端口号
    uint16_t revd[10];          // 预留
} meter_cfg_t;

#define	P1CHAR_TYPE_LEN	((uint16_t)1024*2)// 

typedef struct {
    uint8_t in_sleep;    /* 1=休眠中 */
    uint8_t sleep_soc;   /* 进入休眠时 00100 段 SOC(102) */
    char dev_type[12];   /* 进入休眠时 00100 段机型(110~115) */
    uint64_t dev_sn;     /* 进入休眠时 00100 段设备SN(116~119) */
} iot_sleep_ctx_t;

//IOT自身参数
typedef union tagSetData//union内部 struct等效,需要为   
{
	struct
	{ 
		uint8_t	CfgCharData[P1CHAR_TYPE_LEN];		// 
//		float	CfgFloatData[FLOAT_TYPE_LEN];		// 
	}ArrayData;			/*设置数据结构*/




	struct sNameData// 
	{
		uint16_t valid_iot;//参数有效标志,iot本身，必须
		uint16_t valid_other;//参数有效标志，其他，辅助
		g_st_Flag_EE_WR Wr_flag;//参数区变量被人工或外部修改的标志，部分关键
		uint16_t valid_revd;//revd

///////valid_iot,12000

//#define	IOT_SET_STRUCT_COMMON_REG_12000_0x2 
		uint16_t factory;			 /*12000 恢复出厂设置*/
		uint16_t wifi_sta_auth; 	 /*12001 wifi sta 加密认证方式 wifi 作为sta时 需要连接的路由器加密方式*/
		char wifi_sta_ssid[32]; 	/*12002~12017 WiFi sta ssid  wifi 作为sta时 需要连接的路由器名称*/
		char wifi_sta_password[64]; /*12018~12049 wifi sta 密码 wifi 作为sta时 需要连接的路由器密码*/
		sta_eanble_t sta_enable;	/*12050 bit0~7:密码高32字节有效标志(0xA5有效); bit8:开放式WiFi支持(1-支持)*/
		uint16_t revd1[16]; 	 /*12051~12066 预留*/
		char could_dns[128];		/*12067~12130 服务器地址*/
		char mobile_apn[60];		/*12131~12160 apn*/
		config_status on_off;/*12161~12162*/
		thunder_ctrl_t thunder_ctrl;/*12163*/
		uint32_t period_report;/*12164~12165 数据上报服务器周期（休眠周期，4G模块省流量目的）	2	秒数	R/W*/\
		uint64_t Network_name_BR_set;/*12166~12169*/
		Struct_IOT_Enable_mix1 IOT_Enable_mix1;/*12170*/
		uint16_t IOT_Enable_mix2;/*12171*/
		uint16_t Protocol_3r_Enable_mix1;/*12172*/
		uint16_t Protocol_3r_Enable_mix2;/*12173*/

		uint8_t revd11[92]; 

///////valid_other


		uint32_t protocol_ver; // 协议版本号
		uint32_t hardware_ver; // 硬件版本号
		uint32_t software_ver; // 软件版本号
//		char	 iot_type[12];
//		uint64_t iot_sn;		// sn码
//		uint64_t safe_code; 	// 安全码
//		uint8_t cert_md5[48]; //三份证书的MD5值 
	
		/* 系统架构发生变化后,使用0号主节点上的0号逆变设备的设备类型与SN码 */
		char	 INV_dev_type[12];	// 下游设备SN码，从CAN INV ARM获取
		uint64_t INV_dev_sn;		// 下游设备SN码
		char	 Parallel_dev_type[12];	// 下游设备SN码，从CAN ACHUB ARM获取，多并机中，要求从ACHUB获取一个公共不变的SN作为蓝牙广播名
		uint64_t Parallel_dev_sn;		// 下游设备SN码
		uint64_t Parallel_dev_safetycode;		// ACHUB安全码			
//		char iot_type[12]; // iot出厂标定类型
//		uint64_t iot_sn; // iot出厂标定sn
	
//		uint8_t wifi_sta_auth;		// wifi 作为sta时 需要连接的路由器加密方式
//		char wifi_sta_ssid[32]; 	// wifi 作为sta时 需要连接的路由器名称
//		char wifi_sta_password[64]; // wifi 作为sta时 需要连接的路由器密码
//		sta_eanble_t sta_enable;	   // bit0~7:密码高32字节有效标志(0xA5有效); bit8:开放式WiFi支持(1-支持)
//		char could_dns[128];		// bluetti 服务器地址
//		config_status on_off;//cmd,12161
//		thunder_ctrl_t thunder_ctrl;

//		uint8_t ctrl_reset_factory_2206; // 恢复出厂 2206:0-无效;1-美规；2-日规；3-欧规；4-澳规；5-中国,6-非洲；7-德国;
//		ctrl_mix_t ctrl_mix_2232;//2B
//		ctrl_event	ctrl_2006;		// 一次性控制事件,// 2006 逆变设备清除事件,2B


		
//		uint16_t crc16;
		uint8_t  app_password[6];
		char Net_Server_address[128];	//22005 服务器网址 ，windy增加第二服务器
		char Net_Server_secret[64]; //22069 登录服务器密码	


		ctrl_meter_struct ctrl_meter;//2228 电表类型选择

        uint16_t ctrl_feedback;//2208 控制馈电开关
        uint16_t CounterCurrentPower_Limit;//2266 Grid防逆流功率阈值

        uint16_t Meter_Select;
		

//kwh fast record
	//		uint8_t pv_index_energy;//存储pv_now_energy的数组序号，当前年
	//		uint8_t grid_index_energy;//存储pv_now_energy的数组序号，当前年
	//		uint8_t Hour_index_energy;//存储pv_now_energy的数组序号，当前小时数组偏移,记录后的地址偏移条目序号：0~max-1
//			uint8_t Hour_energySaveCount;// 记录条数	
	//		uint16_t pv_hour_enengy[ENERGY_CNT_MAX_HOUR]; // 每小时的总能量*近30天
	//		uint16_t grid_hour_enengy[ENERGY_CNT_MAX_HOUR]; // 每小时的总能量*近30天

		uint32_t historyRecSaveCount;//log记录条数 ,modbus 3000用辅助
		uint32_t historyAddrIndex;//log记录后的地址偏移条目序号：0~max-1,10w ,modbus 3000用辅助

		uint32_t Event_SaveCount;//log记录条数 ,windy add,data channel
		uint32_t Event_AddrIndex;//log记录后的地址偏移条目序号：0~max-1,10w ,windy add,data channel
	
//		baseKWH_energy_t_u32_2 KWH_all;//长期永久累计
//		baseKWH_energy_t_u32_2 DeltaKWH_Year;//新增加年的累计能量；在年切换清零，在下电瞬间更新记录
//		baseKWH_energy_t_u32_2 DeltaKWH_Month;//		
//		baseKWH_energy_t_u16 DeltaKWH_Day;//
//		baseKWH_energy_t_u16 DeltaKWH_Hour;//
//        energy_time_t Energy_time_message;//上述电量信息存储时间戳
//		uint8_t kwh_years_num;
		POINT_KWH_INFO_Store iot_kwh_info;
//        POINT_LOG_INFO_Store iot_log_info;   

		
		uint8_t Sn_bind[5]; //用于CAN并机群组绑定的关联群组设备SN  
        time_res_struct SetTimeZone;//时区,因INV本身不保存时区，所以IOT要保存
        uint32_t data_report_cycle;

		
		POINT_BIND_INFO_Store point_bind_info[BIND_ARRAY_MAX][BIND_POINT_IN1ARRAY_MAX];//本地EEPROM存储 ，绑定表格，二维数组
		
		uint8_t Addr_can_self;

		iot_mode_struct support_mode;

		uint32_t invDetailedInfo_SaveCount[2];//逆变器历史数据log记录条数 
		uint32_t invDetailedInfo_AddrIndex[2];//逆变器历史数据log记录后的地址偏移条目序号

		char disaster_Warm_Msg_Id[100];

		uint8_t http_get_host_valid;
		char http_get_old_host[120] ;
		uint16_t http_get_old_port ;
		uint8_t http_get_old_ipaddr[4];

		uint8_t u8temp;
        ble_protocol_en_t ble_protocol; // BLE 协议使能控制
        int16_t blec_rssi_th; // BLE客户端切换RSSI阈值
        uint16_t blec_switch_int; // BLE客户端切换间隔
        uint8_t bles_adv_key[16];   //BLE server广播数据的加密秘钥

        int16_t wifi_sta_rssi_th; // STA RSSI阈值
        uint16_t wifi_sta_switch_int; // STA切换间隔

        wifi_mul_sta_en_t wifi_mul_sta_en;      // 多STA使能
        uint32_t wifi_sta1_ip; // STA1静态IP地址
        uint32_t wifi_sta1_mask; // STA1静态子网掩码
        uint32_t wifi_sta1_gw; // STA1静态网关地址
        uint32_t wifi_sta1_dns1; // STA1静态DNS1地址
        uint32_t wifi_sta1_dns2; // STA1静态DNS2地址

        uint16_t wifi_sta2_auth;
        char wifi_sta2_ssid[32];
        char wifi_sta2_password[64];
        uint32_t wifi_sta2_ip; // STA2静态IP地址
        uint32_t wifi_sta2_mask; // STA2静态子网掩码
        uint32_t wifi_sta2_gw; // STA2静态网关地址
        uint32_t wifi_sta2_dns1; // STA2静态DNS1地址
        uint32_t wifi_sta2_dns2; // STA2静态DNS2地址

        uint16_t wifi_sta3_auth;
        char wifi_sta3_ssid[32];
        char wifi_sta3_password[64];
        uint32_t wifi_sta3_ip; // STA3静态IP地址
        uint32_t wifi_sta3_mask; // STA3静态子网掩码
        uint32_t wifi_sta3_gw; // STA3静态网关地址
        uint32_t wifi_sta3_dns1; // STA3静态DNS1地址
        uint32_t wifi_sta3_dns2; // STA3静态DNS2地址

        uint16_t wifi_ap_auth;
        char wifi_ap_ssid[32];
        char wifi_ap_password[64];
        uint32_t wifi_ap_ip; // wifi ap静态IP地址, 暂不支持修改
        uint32_t wifi_ap_mask; // wifi ap静态子网掩码, 暂不支持修改
        uint32_t wifi_ap_gw; // wifi ap静态网关地址, 暂不支持修改

        open_mqtt_en_t open_mqtt_enable; // MQTT使能
        uint16_t open_mqtt_report_cycle; // MQTT上报间隔, 仅在report_en=1时有效

        modbus_tcp_en_t modbus_tcp_enable; // Modbus TCP使能
        uint16_t modbus_tcp_port; // Modbus TCP端口号

        meter_cfg_t meter_cfg[METER_MAX_NUM];  /*1905~1922 电表配置*/
        lcd_mode_t LCD_Mode;//12205 断码屏设置
        Time_Span_t Time_Span;//12206 时间跨度
        temp_unit_control_t temp_unit_control;      // 温度单位控制
        uint16_t powerOff_Nums;     /*断电次数 11100*/
        uint32_t saveMoneyNums;     /*省钱参数 11101~11102*/
        uint16_t ctrl_lcd_active_time; // 屏幕休眠时间
        iot_sleep_ctx_t sleep_ctx;     // 休眠状态、SOC、机型与SN持久化
	}dev_info_t;//NameData;	//设置数据结构
}SetData_TypeDef;	/*设置数据联合*//*16*192*/

extern SetData_TypeDef SetData;

typedef struct {
	INV_SET00_STRUCT_COMMON_0x1A

} inv_set00_0x1A_struct_mini;

typedef struct {
	INV_SET01_STRUCT_COMMON_0x1B

} inv_set01_0x1B_struct_mini;

typedef struct {
	INV_SET02_STRUCT_COMMON_0x1C//新产品不使用

} inv_set02_0x1C_struct_mini;
typedef struct {
	INV_SET03_STRUCT_COMMON_0x1D

} inv_set03_0x1D_struct_mini;	

typedef struct {
	PACK_CONFIG_STRUCT_COMMON_0x55

} pack_config_0x55_struct_mini;	




#define	P2CHAR_TYPE_LEN	((uint16_t)1024*4)// 

//IOT自身参数
typedef union tagSetData2//union内部 struct等效,需要为   
{
	struct
	{ 
		uint8_t	CfgCharData[P2CHAR_TYPE_LEN];		// 
//		float	CfgFloatData[FLOAT_TYPE_LEN];		// 
	}ArrayData;			/*设置数据结构*/




	struct sNameData2// 
	{
		uint16_t valid_inv;//参数有效标志
		uint16_t valid_pack;//参数有效标志
		
//		MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067	基础设置项
//		MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区
//		MOD_STRUCT_reg02300 mod_reg02300_Inv_set02_struct; //2300~2400	电网认证区
//		MOD_STRUCT_reg02400 mod_reg02400_Inv_certification; //2400~2449  认证设置项
//		MOD_STRUCT_reg02500 mod_reg02500_Inv_advance_set2; //2500~2544	每台逆变高级设置区2（面向微逆）


		inv_set00_0x1A_struct_mini	inv_set00;	// inv设置区00
		inv_set01_0x1B_struct_mini	inv_set01;	// inv设置区01
		inv_set02_0x1C_struct_mini	inv_set02;	// inv设置区02
		inv_set03_0x1D_struct_mini	inv_set03;	// inv设置区03
		pack_config_0x55_struct_mini pack_config;    // pack 参数配置区
//		inv_wifi_struct 	inv_wifi;	// inv第三方wifi
//		auth_struct 		auth_param; //认证参数，modbus 40000透传
		
		uint8_t u8temp; //  
///////////////////		
//		float	CfgFloatData[FLOAT_TYPE_LEN];		// 

	}dev_info_t2;//NameData;	//设置数据结构
}SetData_Can_TypeDef;	 
//Inv_can

extern SetData_Can_TypeDef SetData_Can;


typedef enum {
    CAN_ACK_AC_HUB_INFO_42H=0x1,
    CAN_ACK_INV_LOAD_16H=0x2,
    CAN_ACK_INV_DATA_17H=0x4,
} Can_Poll_Ack_Mask;


#define is_invload_16_poll()    (reals.poll_ack_mask&CAN_ACK_INV_LOAD_16H)
#define is_invdata_17_poll() 	(reals.poll_ack_mask&CAN_ACK_INV_DATA_17H)
#define is_achub_42_poll() 		(reals.poll_ack_mask&CAN_ACK_AC_HUB_INFO_42H)


//extern EXT_RAM_BSS_ATTR History1Data_TypeDef HistoryData1;

typedef struct {
    uint8_t     can_dev;//iot/inv/pack类型
	uint8_t 	can_type;    
    uint8_t     range_level;//报文重要程度，0-最低，非零-依次增加；高等级的可对低等级的插队，反之不能，同等级也不能插队
    uint8_t     run_flag;//1-当前can_type查询正在执行，0-完成
	uint32_t	time_old;// 
	uint16_t 	Cnt_bigcycle;//执行计数器，首次启动快速获取，0-表示首次/立刻执行
	uint16_t 	Cnt_smallcycle;//当前can_type切换 不同 地址用计数器

	uint16_t 	period;//X1000s
	uint8_t 	max_x;
	uint8_t 	max_y;	
	
	uint16_t modbus_Rd_cnt;//BLE快速读取用的倒计时计数器，0到非0瞬间 发送，延时清零做读取间隔,1秒 -1
	uint16_t modbus_Rd_cnt_last;//modbus_Rd_cnt上一次的旧值
	uint32_t poll_ack_flag;//轮询应答掩码

}can_poll_struct;//iot read


typedef union 
{
	uint16_t all;

	struct{
		uint16_t point_online : 1; //1-active;0-inactive 
        uint16_t update_need : 1; // 升级需求 
        uint16_t alarm_exist : 1; // 告警发生 
        uint16_t protect_fault_exist : 1; // 保护/故障发生 

        uint16_t bit4 : 1; //  
        uint16_t bit5 : 1; //  
        uint16_t bit6 : 1; //  
        uint16_t bit7 : 1; //  

        uint16_t bit8 : 1; //  
        uint16_t bit9 : 1; //  
        uint16_t bit10 : 1; //  
        uint16_t bit11 : 1; //  
        
        uint16_t bit12 : 1; //  
        uint16_t bit13 : 1; //  
        uint16_t bit14 : 1; //  
        uint16_t bit15 : 1; //
	}state;
} SlavePointStates;	//从机设备节点状态


typedef struct
{
	uint8_t slaveIndex;//存储地址序号
	uint8_t slaveAddr;//从机单地址
	uint8_t groupIndex;//群组序号
	uint8_t sumAddr;//汇总信息从机地址
    SlavePointStates pointState;//节点状态

}Slave_Point_Infos;	//从机设备 节点信息

typedef union
{
	uint16_t all;

	struct {
		 uint16_t link_report:1 ;	 	//连接状态广播
		 uint16_t revd:15;
	 }bit;
} dev_update_ctrl;

typedef enum
{
    WIFI_AP_MODE_CLOSE = 0,     //未开启AP
    WIFI_AP_MODE_NORMAL = 1,    //wifi access point模式
    WIFI_AP_MODE_MESH = 2,      //wifi-mesh模式
}WiFi_AP_mode_t;

typedef union
{
    uint16_t Byte2;   // 
    struct
    {
        /*顺序不可变动*/
        uint8_t revd       : 1;
        uint8_t iot        : 1;
        uint8_t inv        : 1;
        uint8_t pack       : 1; 
		uint8_t achub       : 1; 
        uint8_t dchub       : 1; 				 
        uint8_t d400s       : 1;  
		
    } sBit;
} Need_new_version_flag_t;

typedef struct {
	g_st_FlagTime st_FlagTime;
	rtc_time_t rtc_time;
	time_t now;
	struct tm tm;	
    uint8_t time_init_flag;//时间初始化标志 0：未初始化/失败， 1：成功; 来自网络NTP、蓝牙设置、下级INV CAN获取都算
	BitData_RTC rtc_flag;
	volatile uint8_t  Timer_1msCount;
	volatile uint8_t  Timer_10msCount;
	volatile uint8_t  Timer_50msCount;
	
	volatile uint8_t  Timer_100msCount;
	volatile uint8_t Timer_500msCount;
	volatile uint8_t Timer_1sCount;
	volatile uint8_t Timer_1MinuteCount;
	UART_STRUCT struct_uart0;
	UART_STRUCT struct_uart1;
	UART2_STRUCT struct_uart2;
//	BitData_Eprom  SetDataWrFlag; 
	BitData_PARAMETER_Set  SetDataWrFlag; //SetData结构体内变量写入
	BitData_FILE_SET  flasWrFlag; // flah文件写标志
	BitData_KWH_FILE_SET kwhWrFlag;//KWH flah文件写标志

    BitData_IOT_Status_Flag IOT_Status_Flag;
	BitData_MODBUS_CMD ModbusCmdFlag;//self_cmd;//临时命令集合
	uint8_t  E2PROMPageRead1[16];//E2PROM连续读缓冲区 temp
	uint8_t  E2PROMPageRead2[16];//E2PROM连续读缓冲区
	uint8_t  E2PROMPageRead3[16];//E2PROM连续读缓冲区
	
	uint8_t *AT_Cmd_RxPointer;//4G AT指令接收指针，初始化指向uart rx buf，发送前清零

	uint8_t TimeOutCnt_Meter1;//通讯超时计数器
	uint32_t Cnt_task[10];//	
	uint8_t  Channel_Internet;//1-WIFI;2-SPI ETH;3-4G AT module
//	uint8_t TxBuff_TCP_Temp[4096];//	
//	uint8_t RxBuff_TCP_Temp[4096];//	
//    uint8_t BleRxdata[1200];//BLE_RX_SIZE
//	uint8_t BleRxLen;////BLE RX接收缓存长度
	uint8_t BleRx_TimeCnt;//BLE rx报文超时，拼接完成，待解析,非零表示触发计时器，每隔10ms +1	;1-收到报文并触发超时计数器
	uint32_t cnt_1s;//
	uint16_t debug1;//
	uint16_t debug2;//
	uint16_t debug3;//
	uint16_t debug4;//
	
	uint32_t  cnt_100ms;
	uint8_t Cellular4G_step;//	
	uint16_t Cellular4G_AT_fail_cnt;//	向4G模块发送命令AT命令的失败/ERROR次数
	
	uint16_t wifi_eventgroup;//将阻塞改为非阻塞	
	uint8_t Ethernet_SPI_step;//	
	uint8_t  wifi_connect_ap;//1-已通过WIFI连接路由器局域网(不含MESH 网络)，并获得本地IP;
	uint8_t  wifi_connect_STA;//1-自己作为AP接入其他STA
	uint8_t  wifi_mesh_root;//	1-作为MESH网络根节点（允许接入外部路由器MQTT登录服务器），0-非根节点（不允许接入外部路由器MQTT登录服务器）
	g_st_Flag1 flag_array1;
	g_st_FlagLED LED_stat;//DTU

	uint8_t event_ppp_register_flag;//4G,1-已注册；0-未注册，互斥用
	uint8_t event_wifi_register_flag;//1-已注册；0-未注册，互斥用
	uint8_t event_eth_register_flag;//SPI eth,1-已注册；0-未注册，互斥用

	uint16_t UART1_TXCount;	//临时紧急的uart发送
//	uint8_t UART1_TXBuffer[TX_LEN];//临时紧急的uart发送缓存	
	uint16_t UART2_TXCount;	//临时紧急的uart发送
//	uint8_t UART2_TXBuffer[TX_LEN];//临时紧急的uart发送缓存	
	 
	AT403_UNION1   AT403_DataUnion1;// uart 设备变量结构体
	AT403_UNION2   AT403_DataUnion2;// 
	AT403_UNION3   AT403_DataUnion3;// 
	AT403_UNION4   AT403_DataUnion_Set4;// write，设置变量
	AT403_UNION4   AT403_DataUnion4_Setback;// write，设置变量的反向读取
	

	Sub1GHz_UNION1   Sub1GHz_DataUnion1;// 
	Sub1GHz_UNION2   Sub1GHz_DataUnion2;// 
	
	RS485_UNION1   RS485_DataUnion1;// 
	MAC_UNION	mac_union;//接收来的设备节点MAC信息缓存
//	uint8_t  revd[10];//debug
	MAC_UNION	local_mac_union;//本地设备节点MAC信息缓存
//	NET_POINT_InfoStruct	net_point_base_Info[NET_SUB1G_MAX_POINT];//Sub1GHz 网络节点的最基本信息要素集合，排序缓存;只更新和左移填空，不排序，
//	uint8_t Subseq_index[NET_SUB1G_MAX_POINT];//Sub1GHz ,设备SN缓存的排序后序号：从小到大;[x]-上报的顺序 modbus slave 序号；seq_index-RAM缓存序号，关联下级uart设备和上级Modbus 协议
//	uint8_t Subnet_point_Num;//Sub1Ghz net 在线的设备节点


	uint8_t Step_dev_discovery ;//1-triger,2-report SN,3-finish
	DISCOVERY_POINT_InfoStruct	discovery_net_Info[NET_WIFI_MAX_POINT];//同一WIFI路由器下面所有我司设备节点信息（不含子节点）：S1(16) +MIX WIFI(20)
	DISCOVERY_POINT_InfoStruct  discovery_Info;//本地缓存，临时存储最新接收节点信息
	uint8_t Topseq_index[NET_WIFI_MAX_POINT];//discovery_net_Info[].SN按照从小到大的排序序号，空SN在尾部
	uint8_t Topnet_point_Num;//WIFI net 在线的设备节点
	uint8_t Topseq_Invbat_index[NET_WIFI_INVBAT_POINT];//	
	uint8_t Topseq_S1_index[NET_WIFI_S1_POINT];//从Topseq_index[]分离出的S1序号
	uint8_t Topseq_mix_index[NET_WIFI_MIX_POINT];//从Topseq_index[]分离出的invbat序号
	uint8_t Topnet_point_Num_S1;//WIFI net 在线的设备节点
	uint8_t Topnet_point_Num_mix;//WIFI net 在线的设备节点
	uint8_t Topnet_point_Num_invbat;//WIFI net 在线的设备节点
	uint8_t Acphase_seq_S1[NET_WIFI_S1_POINT];//1-phase 1;2-phase 2;3-phase 3;其他无效
	int16_t PowerSum_Acseq_S1[3];//分别表示 S1在三相AC功率线上的 功率，w



    NET_POINT_OTA_InfoStruct net_point_ota_Info[NET_SUB1G_MAX_POINT];//子设备节点ota状态	
	NET_POINT_POWER_InfoStruct	net_point_power[NET_SUB1G_MAX_POINT];//
//	uint8_t Pseq_index[NET_SUB1G_MAX_POINT];//用于微逆并机功率调节的序号缓存	

	uint64_t iot_sn_sum[DEV_MAIN_NODE_MAX*IOT_MAX_NUM];//CAN总线上的所有IOT的SN汇总，用于IOT CAN参数同步辅助， 超时清零
//	uint8_t flag_in_can_inv_sn;//can INV设备 有新的SN出现，即设备增加，1-happen
	int8_t T_delay_iot_para_can_wr;//因新设备出现，需要同步IOT配置参数的倒计时，在设备新增时刻赋非0值; 0-时间到，准备发送；-1-无效


	
	uint8_t net_point_Comein;//	新设备发现/接入，需要立刻上报更新
    uint8_t modbus_self_report_ble;//蓝牙上报标志
    uint8_t modbus_self_report_mqtt;//mqtt上报标志

	uint8_t bind_state_ask;//0-空闲, else-收到上级读取21000绑定帧标志(1:蓝牙版本,3:升级状态)
        
	uint8_t new_inv_discovery;//	新设备发现/接入，需要立刻下发设置

    uint8_t new_inv_drm0s_set;//新设备发现，广播DRM0s设置
    uint8_t new_inv_rtc_set;//新设备接入，广播RTC时间
    uint8_t inv_set_send_slaveaddr;//下发inv设置地址
    
//	uint16_t net_point_TimeOut_cnt[MAX_NET_POINT];//
	uint8_t SubNet_point_rx_get;//1-串口收到；0-在主循环解析完毕;忽略Sub 1GHz的BR
    uint8_t Wifi_Udp_point_rx_get;//1-udp收到；0-在主循环解析完毕;
    
	int adc_raw;//[2][10]; ADC 采用
	int voltage;//[2][10];
/*
	0xAA:使能老化模式
	0x55:进入正常模式
	0x40:进入单板测试模式
	0xA1:预留
	0xA2:重启
	*/	
	uint16_t test_mode;//30901
//    unsigned char hash[128];//debug	
//    uint8_t sig_buff[400];    
//debug
//    dsp_reg0300_t   reg0300;
	uint8_t ADL200_AD_Grid ;

	uint8_t GridAddr_done;//1-电表识别完成；0-未完成
	uint8_t GridAddr_timeout_cnt;//电表超时未回复次数
	uint16_t GridAddr ;//用于自动识别电表 modbus slave addr
	//uint8_t SetMeterType;//电表型号
	GRID_METER_UNION   GridDataUnion;//GridDataStruct

	//uint8_t SN[10];//L8=SN,H2=type seq
	uint8_t bind_triger;//0-空闲, 1-收到上级下发的21000绑定帧，2-绑定帧处理完毕，等待下发自定义网络名至白名单设备
	uint16_t set_bind_cnt;//设置绑定帧的节点数量

	uint8_t uart_cc1312_read_state;//1-收到modbus03rtn，准备执行下次发送 0-等待rtn

	uint8_t inv_drm0_state; //0：空闲  1：向下级所有inv发送开机指令 2：向下级所有inv发送关机指令

	uint8_t button_state;//0：空闲 1：蓝牙配对、配网 2：四个指示灯循环闪烁 3：恢复出厂设置
	uint8_t button_timeout_cnt;//按键触发/按下时间 100ms

    LOG_FAULT_STRUCT log_fault_info;//故障信息暂存
	uint16_t historyRecSaveCount;//log记录条数 
	uint16_t historyAddrIndex;//log记录后的地址偏移条目序号：0~max-1,10w 

	uint32_t Event_SaveCount;//log记录条数 ,windy add,data channel
	uint32_t Event_AddrIndex;//log记录后的地址偏移条目序号：0~max-1,10w ,windy add,data channel
	
//	uint32_t pv_last_hour_enengy;//用于PV每小时kwh累计的临时变量
//	uint32_t pv_hour_enengy_delta;//一个小时的kwh
//	year_energy_t energy_grid; // 当前grid能量信息

//[0]汇总能量 [1]单机能量
	year_energy_t energy_DCLoad_Total[2];//	DC负载总能量低,1w*s;	0.1kwh
	year_energy_t energy_ACLoad_Total[2];//	AC负载总能量低	
	year_energy_t energy_Pv_dc[2];//	PV总的充电能量低 		 
	year_energy_t energy_pv_ac[2]; // 外部微逆，
	
	year_energy_t energy_GridChgin_Total[2];// 电网充电能量低 
	year_energy_t energy_GridFeedback_Total[2];//	馈电到电网能量低 
	year_energy_t energy_PVToload_Total[2];//	PV到AC负载能量低		 
	year_energy_t energy_PackDsg_Total[2];//	Pack总放电能量低	

    baseKWH_energy_t_u16 hour_energy[2];

    energy_time_t hour_energy_time;//小时能量存储时间戳
        
	uint32_t file_nums;			// 文件数

	uint8_t Step_can_dev_parallel ;//1-triger,2-report SN,3-finish
	uint8_t Step_can_Cnt ;//step can的秒计数器	
	//uint8_t MonSnSame;//CAN  SN重复
	uint8_t Addr_can_Attr;// Can并机排序属性
	uint8_t Addr_can_master;//CAN并机地址，主设备,1-master

	uint8_t Addr_can_Buff;//can并机排地址Addr_can_self的临时地址变量,0~(max-1)
	uint8_t Addr_can_Small;//动态地址变量,0~(max-1)，由于每次最小地址标志基于0x5心跳 重新计算，因此即使最小主模块拔出，次小编号REC会接管主模块工作
	uint8_t Addr_can_self;// 本MON CAN地址编号：0~(max-1)
	uint16_t Addr_can_Parallel_Interval;// Can并机排序操作等待时间


	
	uint64_t INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX+1];//can总线获取SN，用于并机地址排序  INV_CAN_MAX_POINT,IOT排序
	uint64_t MCUPoweronCnt;//ESP32上电后计数器，秒单位

//	uint8_t BLE_WIFI_to_Can_flag;//L4bit-BLE send to can :;H4bit-wifi send to can :添加队列时候置1，执行
	uint8_t BLE_WIFI_to_Can_Cnt;//来自BLE和WIFI的CAN转发写数据的队列计数器
	int8_t fast_to_Can_poll_index[CAN_DATA_POLL_INDEX_MAXNUM];//-1:invalid,临时需要快速读取CAN设备数据的can_poll[]序号

    UDP_MODBUS_STRUCT udp_modbus;//udp_modbus辅助变量
    uint8_t udp_read_state;//1-收到modbus03rtn，准备执行下次发送 0-等待rtn



//    char    ble_name[29];  // 蓝牙广播名称
    uint8_t authenticate_state;  
	uint8_t ble_authenticate_buf_status;
	uint16_t ble_encrypt_info_flag;


	uint8_t  Can_beta_block_step;//
	can_poll_struct can_poll[MAX_NUM_CAN_TYPE];//
	uint32_t poll_ack_mask;

	uint16_t Can_beta_block_send_timeout_Cnt;//用于can type block块间隔延时
//	uint8_t one_poll_finish;//CAN多字节发送查询一个can type的多个设备对象，1-正在发送；0-发送完成
	uint8_t  Can_beta_block_type;//用于自己主动发can type block块读取的can type，在 tx fun=0x63赋值
	uint8_t  Can_beta_block_fast_poll_flag;//0-no poll,1-poll pack,2-poll inv
	uint8_t  Can_beta_block_inv_slow_poll_flag;//0-no poll,1-poll pack,2-poll inv
	uint8_t  Can_beta_block_inv_about_poll_flag;//0-no poll,1-poll pack,2-poll inv
	uint8_t  Can_beta_block_iot_poll_flag;//0-no poll,1-poll pack,2-poll inv

	uint8_t online_X_inv_index;//第一个在线的设备序号,CAN总线设备在线索引[x][y]
	uint8_t online_X_bat_index;//	
	
	uint8_t online_Y_inv_index;
	uint8_t online_Y_bat_index;

	uint8_t online_Inv_num;//
	uint8_t online_Inv_bit;//按位记录Inv在线状态
	uint8_t online_Pack_num;//
	uint8_t online_Iot_num;//
	uint8_t online_ACHUB_num;//
	uint8_t online_DCHUB_num;//
	uint8_t online_DCHUB_bit;//按位记录DCHUB在线状态
	uint8_t online_AT1_num;//
	uint8_t online_EBOX_num;//
	uint8_t online_D400S_num;//
	uint8_t online_D400S_bit;//按位记录D400S在线状态
	uint8_t ota_happen;//1-正在升级

	uint8_t iot_can_set_flag;//需要给CAN设备下发set参数
	uint8_t inv_can_set_flag;//需要给CAN设备下发set参数
	uint8_t pack_can_set_flag;//需要给CAN设备下发set参数

	EventHistoryData_Struct HistoryData_event;//windy 新增 iot，数据通道传输
	uint32_t canrx_push_cnt;//
	uint32_t canrx_pop_cnt;//
	uint8_t zero[512];//比对用，改到外部RAM开销 = {0};

//	uint16_t ModbusHistTimeoutCnt[5] ;//用于上级BLE到下级CAN 转发读取历史记录辅助，（上级需要2次查询，第1次的范围0内容）
//	uint8_t ModbusHistRdCnt[5] ;//用于上级

	uint8_t If_ARM_SingleBoot;//底层ARM是否处于单boot状态
	uint8_t If_AC_HUB_SingleBoot;//底层ACHUB是否处于单boot状态
    uint32_t ARM_SingleBoot_version; //
    uint32_t AC_HUB_SingleBoot_version; // 

	uint32_t debug_Count_can_busoff;//
	uint32_t debug_Count_can_stop;//
	uint8_t self_wifi_ap_ip[4];//自己作为WIFI AP IP
	uint32_t preSoftVersion;  //之前固件类型的软件版本号
	Slave_Point_Infos achubPointInfos;	//achub节点信息
	Slave_Point_Infos dchubPointInfos;	//dchub节点信息
	Slave_Point_Infos d400sPointInfos[D400S_SALVE_MAX_POINT];	//d400s节点信息
	uint16_t  packPoinitNum;	//电池包在线节点数量
	Slave_Point_Infos packPointInfos[PACK_SALVE_MAX_POINT];	//电池包节点信息
	
	uint64_t wifimesh_self_dev_sn;				// 自身设备sn
    uint16_t wifimesh_self_dev_type_code; 			// 自身设备机型序号
	uint8_t mqttChange_flag;			//mqtt变化标志	
	dev_update_ctrl dev_change_ctrl;			//设备状态更新处理标志
	uint8_t iot_ota_flag;				//iot并机升级
	uint8_t iot_ota_end_count;

	uint32_t invDetailedInfo_SaveCount[2];//逆变器历史数据log记录条数 
	uint32_t invDetailedInfo_AddrIndex[2];//逆变器历史数据log记录后的地址偏移条目序号

	uint8_t trigger_Bind_AckCount;//绑定响应
	WiFi_AP_mode_t wifi_ap_mode;
	Need_new_version_flag_t need_new_version_flag;//是否需要获取最新版本信息标志

	uint8_t achub_grid_transfer_flag;		//achub 电网信息传输标志 1:电网功率以及能量由achub传输

    uint8_t iot_smart_change;          // iot智能控制状态 0:无 1：需要下发

	// wlcc debug
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
}RealS_STRUCT;








extern RealS_STRUCT reals;

//typedef struct Power_statistic {
//	float pv_energy;//*100WH，取 micro inv inv out power
//	float grid_energy;//*100WH
//	
////	float Time_Mon_work;//监控累计运行时间,小时	
//
//
//} statistic_struct;
extern baseKWH_energy_t_u32_2   statistic;///累计统计量的实时缓存，不存储，
extern baseKWH_energy_t_u32_2   statistic_delta;///


#pragma pack()

// typedef struct {
//     uint8_t  type;					//modbus转can的指令数据类型
//     uint16_t offset;				//数据偏移 是相对某一块CAN数据的偏移(eg:0x1a)
//     uint16_t len;    				//数据长度

// 	/* 发送该指令后需要激活的指令类型
// 	 * 例如要读取逆变的历史日志，先设置历史日志页指令（type=INV_TYPE_CONFIG00，offset=offsetof(inv_set00_struct, ctrl_log_page)，len=sizeof(ctrl_log_page)）
// 	 * 然后激活历史日志上传指令（active_can_cmd_type=0x20,因为历史日志是触发读取）*/
//     uint16_t active_can_cmd_type;
// }can_data_label;




void iot_period_task (void * pvParameters);
void app_main_timer_init(void);
void sum_task_init(void);
void iot_mqtt_client_task2(void * pvParameters); 
void app_password_check(void);
void Modbus_Read_Info_Process(const uint8_t *income);
void Modbus_Write_Info_Process(const uint8_t *income);
void Default_iot_data_init(void);
void lcd_mode_factory_reset(void);
void iot_sleep_state_restore_from_setdata(void);
void iot_sleep_ctx_restore_mod_reg00100(MOD_STRUCT_reg00100 *page1);
void Grid_Forbid_Back_Ctrl_Cmd(void);
void  Sub_Net_Point_Power_Serial_Order(void);//1s cycle

void ble_on_off_can_debug(uint8_t input );
void Inv_Pack_First_online_Index_Check(void);
void iot_wifi_eth_4g_switch_user(uint8_t mode);
void dump_buf_global(char *info, uint8_t *buf, uint32_t len);
void app_sys_debug_info(void);
//大端模式获取8字节长度的数据
uint64_t  GetUin64FromPtrBig( const uint16_t* uBufPoint);
//小端模式获取8字节长度的数据
uint64_t  GetUin64FromPtrSmall( const uint16_t* uBufPoint);
void link_report_set(uint8_t ble_report);
void iot_can_subdev_online_rise_notify(void);
void calculate_md5(uint8_t *value, uint16_t value_size, uint8_t *out_md5);

#endif

