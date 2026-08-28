#ifndef IOT_TYPE_H
#define IOT_TYPE_H


#include <stdint.h>
#include "can_type.h"
#include "inv_type.h"
#include "comm_can_uart.h"

/*20240703
AC380支持多个IOT和多个INV/ARM在同一CAN总线

0x1~0xF
*/
#define IOT_MASTER_MAX_NUM    1//当前一个逆变器系统主控只有1个IOT
#define IOT_D400S_MAX_NUM    1//当前一个逆变器最多接入1个D400S，则有1个IOT节点

#define IOT_CAN_ADDR    0x1A//0x01 IOT CAN最小地址，windy 20241116 协议改变，AC380 IOT CAN地址范围：0x10+10=0x1A
#define IOT_MAX_NUM     1//一个逆变器系统固定有一个IOT节点
#define IOT_GROUP_MAX_NUM   IOT_MAX_NUM	//


#define HTTPS_DISASTER     "/api/bluiotdata/device/iotData/v1/reportDeviceBackupTime"

enum {
    IOT_TYPE_INFO_01H=0x01,//（IOT板信息）只读
    IOT_TYPE_SET_02H = 0x02,//（设置类变量）可读可写

};

#pragma pack(1)
////////////////////////////////////////ESP IOT////////////////////////////////////////////////
////////////////////////////////////////ESP IOT////////////////////////////////////////////////
////////////////////////////////////////ESP IOT////////////////////////////////////////////////

typedef union {
    uint16_t all;
    struct {
        uint16_t support_ble_pwd : 2; // 密码区有效
        uint16_t visitor_mode : 2; // 访客模式标志

        uint16_t revd1 : 4; //
        uint16_t revd2 : 8; //        
    }bit;
} iot_mode_struct;

/**
 * @brief Type = 0x00
 *
 */
typedef struct {
//    config_status cfg;     // IOT功能开启状态
    link_status   link;    // IOT功能连接状态
    rw_cmd_struct rw_cmd;
}iot_base_struct;



typedef union//2Byte
{
   // uint8_t all;
    uint16_t word;
    struct structBIND_DEV_STATE
    {
        uint8_t need_update          :1; //  
        uint8_t happen_alarm         :1; //  
        uint8_t happen_protect       :1;// 
        uint8_t revd1       		 :1;// 

        uint8_t revd2       :4;// 
        
        uint8_t revd4       :8;// 
    }bits;
}g_st_BIND_DEV_STATE;


typedef struct {
	uint8_t modbus_slave_address;
	uint8_t hreserved;
	g_st_BIND_DEV_STATE  dev_state;
	char SN[10];	
} Dev_bind_info_struct_t;




typedef struct {
	uint16_t version;//蓝牙/MQTT=2；
	uint16_t bias_dev_addr;//设备数量偏移;第一帧为0，第二帧为前面所有帧已传递节点数量的和，最大节点数量仅支持255个
	uint16_t onlineL16;//低16个节点在线状态，0~15bit分别顺序表示各个节点的在线状态，1-在线，0-不在线
	uint16_t onlineH16;//高16个节点在线状态
	
	Dev_bind_info_struct_t	dev_bind_info[32];//tbd
} Frame_bind_info_struct_t;


//网络通道使能，0-无效；1-仅使能WIFI；2-仅使能有线以太网；3-仅使能4G模块；4-自动选择；5-禁止所有上网
#define	NET_CH_INVALIDE		0//
#define	NET_CH_WIFI			1//
#define	NET_CH_SPI_ETH		2//
#define	NET_CH_4G			3//
#define	NET_CH_AUTO			4//
#define	NET_CH_FORBID_ALL	5//


typedef union//2Byte
{
   // uint8_t all;
    uint16_t word;
    struct structIOT_ENABLE
    {
        uint8_t net_ch       :4;// 网络通道使能，0-无效；1-仅使能WIFI；2-仅使能有线以太网；3-仅使能4G模块；4-自动选择；5-禁止所有上网
        uint8_t revd2       :4;// 
        
        uint8_t revd4       :8;// 
    }bits;
}g_st_IOT_ENABLE;





/**
 * @brief Type = 0x01
 *
 */
typedef struct {
    uint8_t wifi_sta_auth;      // wifi sta 加密认证方式
    char wifi_sta_ssid[32];     // WiFi sta ssid
    char wifi_sta_password[64]; // wifi sta 密码
    sta_eanble_t sta_enable;    // bit0~7:密码高32字节有效标志(0xA5有效); bit8:开放式WiFi支持(1-支持)
    char reserved[32];      // 预留
    char could_dns[128];        // 服务器地址
    char mobile_apn[60];        // apn
    uint8_t factory;            // 恢复出厂设置
	config_status on_off;
//	thunder_ctrl_t thunder_ctrl;
//	Net_Server_2RD_struct_t net_server_2rd;//22000，读+写
	
    rw_cmd_struct rw_cmd;
}iot_config_struct;

/**
 * @brief Type = 0x02
 *
 */
typedef struct {
    char     dev_type[12]; // 下游设备SN码
    uint64_t dev_sn;       // 下游设备SN码

    char     iot_type[12];
    uint64_t iot_sn;   // sn码
    uint64_t safe_code; // 安全码

    uint8_t  status;
    uint32_t protocol_ver; // 协议版本号
    uint32_t hardware_ver; // 硬件版本号
    uint32_t software_ver; // 软件版本号
    rw_cmd_struct rw_cmd;
}iot_about_struct;

/**
 * @brief Type = 0x03
 *
 */
typedef struct {
    uint8_t sta_fault;
    uint8_t ap_fault;
    uint8_t sta_ipv4[4];
    uint8_t sta_gw[4];
    uint8_t sta_mask[4];
    uint8_t sta_mac[6];
     int8_t sta_rssi; //
    uint8_t sta_fault_code;
    rw_cmd_struct rw_cmd;
}iot_wifi_struct;

/**
 * @brief Type = 0x04
 *
 */
typedef struct {
    uint8_t fault;
    uint8_t mac[6];
    uint8_t ver; // 0：ble4.0,1：ble4.2,2：ble5.0
    char name[29]; // 蓝牙名
    rw_cmd_struct rw_cmd;
}iot_ble_struct;

/**
 * @brief Type = 0x05
 *
 */
typedef struct {
    uint8_t fault; // 4g状态
     int8_t rssi;   // 基站信号强度
    uint8_t type[20]; // 移动基站网络类型
    uint16_t mcc; // 移动设备所在国家代码
    uint8_t mnc;    // 移动设备网络代码
    uint32_t station_code;  // 基站编号
    uint8_t sim_iccid[20];  //
    char network_operator[40]; // 运营商
    rw_cmd_struct rw_cmd;
}iot_4g_struct;

/**
 * @brief Type = 0x06
 *
 */
typedef struct {
    uint8_t status; // 状态
    uint8_t longitude_direct; // 经度纬度
    uint32_t lagitue_value; //纬度
    uint32_t longitude_value; // 经度
    rw_cmd_struct rw_cmd;
}iot_local_struct;

typedef struct {
    uint32_t software_ver; // 保留
    char  type[12];  // 被标定的类型
    uint64_t sn;        // 被标定的SN
    uint64_t safetCode; // 被标定的安全码
    uint8_t cert_md5[48]; //三份证书的MD5值    
    rw_cmd_struct rw_cmd;
}iot_factory_struct;



/**
 * @brief type= 0x1
 *
 */
typedef struct {
//	MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040	IOT信息区（只读，面向客户）
	IOT_INFO_STRUCT_COMMON_REG_11000_0x1

	rw_cmd_struct   rw_cmd;
}iot_11000_can_struct;

/**
 * @brief type= 0x2
 *
 */
typedef struct {
//	MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
	IOT_SET_STRUCT_COMMON_REG_12000_0x2
    rw_cmd_struct   rw_cmd;
}iot_12000_can_struct;

typedef struct {
//iot can blcok ,can type	windy 20240718 disable，重构，因之前未使用到产品
//    iot_base_struct     iot_base_cantyp_0x00;   // iot基本数据信息
//    iot_config_struct   iot_config_cantyp_0x01; // iot配置信息，读+写
//    iot_about_struct    iot_about_cantyp_0x02;  // iot出厂信息
//    iot_wifi_struct     iot_wifi_cantyp_0x03;   // WiFi信息
//    iot_ble_struct      iot_ble_cantyp_0x04;    // 蓝牙信息
//    iot_4g_struct       iot_4g_cantyp_0x05;     // 4G信息
//    iot_local_struct    iot_local_cantyp_0x06;  // iot定位信息
    uint8_t              devId;
    uint8_t              online;
    uint16_t             alive_time;
    uint16_t			 setdata_valid;//windy add 每一bit表示来自can下级设备的 set 区域的参数是否接收完整，1-完整；
    iot_11000_can_struct iot_can_11000;  //  
    iot_12000_can_struct iot_can_12000;  //  

/////////

//    iot_factory_struct  factory; // iot标定信息
//	uint8_t 			app_password[6];
	uint16_t 			can_fault;
	bat_maintain_t 		bat_maintain;	
    iot_mode_struct     support_mode;
    ct_test_t           ac_ct_test;//test_ac_ct_wx	
	
}iot_node_struct;//windy按照含义集合，而非按照地址集合,只读

typedef struct {
	char hmi_model[12];
	uint64_t hmi_sn;
	uint32_t mcu_ver;
	uint32_t flash_ver;
} hmi_node_struct_t;

typedef struct {
    uint16_t  pile_type[20];   // 充电桩型号
    uint16_t  sncode[20];      // 充电桩SN
}charging_pile_about_struct;

typedef struct {
    uint16_t input_voltage;  //输入交流电压
    int16_t  input_current;  //输入交流电流
    int16_t  input_power;   // 输入交流有功功率 w
}charging_pile_detail_struct;

typedef struct {
    #if(0)
    uint16_t input_L1_voltage;   // 输入L1交流电压
    uint16_t input_L2_voltage;   // 输入L1交流电压
    uint16_t input_L3_voltage;   // 输入L1交流电压
    int16_t  input_L1_current;   // 输入L1交流电压
    int16_t  input_L2_current;   // 输入L1交流电压
    int16_t  input_L3_current;   // 输入L1交流电压
    int16_t  input_L1_power;   // 输入功率 w
    int16_t  input_L2_power;   // 输入功率 w
    int16_t  input_L3_power;   // 输入功率 w
    uint8_t  status;        // 工作状态
    uint8_t  input_type;    // 接入类型 0--PV,1--CAR,2--Adpater,3--other
    uint16_t input_power;   // 输入功率 w
    uint16_t input_voltage; // 输入电压 0.1V
    uint16_t input_current; // 输入电流 0.1A
    #endif
    charging_pile_detail_struct charging_pile_L1;  //LIN
    charging_pile_detail_struct charging_pile_L2;  //L2N
    charging_pile_detail_struct charging_pile_L3;  //L3N
    charging_pile_detail_struct input_L1_sum;   //总
    int16_t  input_L1_sum_reactive_power;   //总无功功率实际值
    int16_t  input_L1_sum_active_power;    //总视在功率实际值
    uint16_t input_PF;          //交流PF
    uint16_t input_f;           //交流频率
    int16_t  T_gun_head;        //枪头温度
    int16_t  T_box;             //枪体内部温度
    uint16_t pail_fault;        //充电桩故障码代号
    uint16_t gun_state;         //枪工作状态代号
    uint16_t pail_state;        //状态
    uint16_t work_time_L;       //已充电时长_L（本次）
    uint16_t work_time_H; 
    int16_t  Charge_Energy;     //已充电电量
    uint16_t TimeCar_Begin_L;   //汽车充电开始时间
    uint16_t TimeCar_Begin_H; 
    uint16_t TimeCar_End_L;     //汽车充电结束时间
    uint16_t TimeCar_End_H; 
    uint16_t CP_Volt;           //交流枪头CP信号
    uint16_t CC_PWM_duty;       //交流枪头CC信号占空比
}charging_pile_meg_struct;

typedef struct {
	union {
		uint16_t all;
		struct {
			uint8_t charge_en : 2;		//充电开关			
			uint8_t discharge_en : 2;	//放点开关
			uint8_t supply_phases : 4;	//供电相数数量设置	
        };	
	};
} pile_set_t;

typedef struct {
	union {
		uint16_t all;
		struct {
			uint8_t charge_start_en : 2;	//充电时间设置使能				
			uint8_t charge_end_en : 2;	    //结束充电时间设置使能
			uint8_t pile_get_pack_en : 2;	//允许充电桩从储能电池取电；	
            uint8_t pile_gridout_en : 2;	//允许充电桩在电网断电情况下给车桩充电			
			uint8_t gun_en : 2;	            //使能插枪自动充电功能
            uint8_t pile_en : 2;	        //使能插枪自动充电功能
		};
	};
} pile_enable_t;

typedef struct {
    uint16_t PF_set;                        //PF设置值
    pile_set_t  pile_set;                   //设置
    int16_t Pactive_set;                    //有功功率设置目标
} charging_pile_set_struct;

typedef struct {
    uint16_t SetTimeStartCarCharge_L;      //汽车充电开始设置时间	
    uint16_t SetTimeStartCarCharge_H;
    uint16_t SetTimeEndCarCharge_L;        //汽车充电结束设置时间
    uint16_t SetTimeEndCarCharge_H;
    pile_enable_t pile_enable;             //使能设置
    uint16_t SOC_Low_level_ChargeCar;      //汽车充电从储能电池取电的储能最小限值
} charging_pile_ems_struct;

typedef struct {
    charging_pile_about_struct   charging_pile;
    charging_pile_meg_struct     charging_pile_meg;  //充电桩信息区
    charging_pile_set_struct     charging_pile_set;  //充电桩设置区    
    charging_pile_ems_struct     charging_pile_ems;  //充电桩配合EMS
} charging_pile_struct;

typedef struct {
//    uint8_t new_cfg;
    iot_config_struct   iot_config; // iot配置信息备份区
	uint8_t 			app_password[6];
	bat_maintain_t 		bat_maintain;
    iot_mode_struct     support_mode; //访客模式及蓝牙密码备份
} bk_iot_node_set;
#pragma pack()
#endif


