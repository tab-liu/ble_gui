#ifndef __DEV_DISCOVERY_H__
#define __DEV_DISCOVERY_H__
#include "esp_err.h"


#include "iot_period_task.h"
#include "filesystem.h"

/*公司产品条码的ASCII内容的代号-序号;配合绑定帧21000*/
/*便携*/
#define SN_TYPE_INV_AC200MAX         1
#define SN_TYPE_INV_AC200L           2  // AC200L/AC200MAXII共用编号2
#define SN_TYPE_INV_AC200MAXII       2
#define SN_TYPE_INV_AC300            3
#define SN_TYPE_INV_AC500            4
#define SN_TYPE_INV_AC70             5
#define SN_TYPE_INV_AP300            6
#define SN_TYPE_AC380_PLP023         SN_TYPE_INV_AP300
#define SN_TYPE_INV_PLP022           7
#define SN_TYPE_INV_ELITE200_V2      8
#define SN_TYPE_INV_RV5              9
#define SN_TYPE_INV_RV               SN_TYPE_INV_RV5
#define SN_TYPE_INV_FP               10
#define SN_TYPE_INV_EL300            11
#define SN_TYPE_INV_EB3A             12
#define SN_TYPE_INV_AC60             13
#define SN_TYPE_INV_EB55             14
#define SN_TYPE_INV_EB70             15
#define SN_TYPE_INV_AC180T_SINGLE    16  // AC180T（单包）
#define SN_TYPE_INV_AC180T_DUAL      17  // AC180T（双包）
#define SN_TYPE_INV_EP500            18
#define SN_TYPE_INV_EP500PRO         19
#define SN_TYPE_INV_AC2A             20
#define SN_TYPE_INV_AC50B            21
#define SN_TYPE_INV_AC60P            22
#define SN_TYPE_INV_AC180            23
#define SN_TYPE_INV_AC200P           24
#define SN_TYPE_INV_AC240            25
#define SN_TYPE_INV_HANDSFREE1       26
#define SN_TYPE_INV_HANDSFREE2       27
#define SN_TYPE_INV_EL320            28
#define SN_TYPE_INV_EL400            29
#define SN_TYPE_INV_EL80V2           30
#define SN_TYPE_INV_EL100            31
#define SN_TYPE_INV_EL30V2           32
#define SN_TYPE_INV_AC200PL          33
#define SN_TYPE_INV_PR002            34
#define SN_TYPE_INV_AC45             35
#define SN_TYPE_INV_AC50P            36
#define SN_TYPE_INV_AC55             37
#define SN_TYPE_INV_AC70P            38
#define SN_TYPE_INV_AC240P           39
#define SN_TYPE_INV_AC180P           40
#define SN_TYPE_INV_AC2P             41
#define SN_TYPE_INV_PREMIUM_20C      42
#define SN_TYPE_INV_KW1000           43
#define SN_TYPE_INV_LFP700           44
#define SN_TYPE_INV_PR30V2           45
#define SN_TYPE_INV_PR100V2          46
#define SN_TYPE_INV_PR200V2          47
#define SN_TYPE_INV_AORA10           48
#define SN_TYPE_INV_AORA30P          49
#define SN_TYPE_INV_AORA30V2         50
#define SN_TYPE_INV_AORA_80          51
#define SN_TYPE_INV_AORA_100         52
#define SN_TYPE_INV_AORA100V2        53
#define SN_TYPE_INV_AORA200          54
#define SN_TYPE_INV_AORA320          55
#define SN_TYPE_INV_AP500            56    // 1-PLP-029-A
#define SN_TYPE_AP500                SN_TYPE_INV_AP500
#define SN_TYPE_INV_POWER5           57    // 1-PLP-030-A
#define SN_TYPE_INV_HS5              58    // 1-RPS-016-A
#define SN_TYPE_INV_HS3              59    // 1-RPS-015-A
#define SN_TYPE_INV_HS2              60    // 1-RPS-014-A
#define SN_TYPE_INV_AORA400          61
#define SN_TYPE_INV_EL10             62
#define SN_TYPE_INV_EL100MINI        63    // PPS018
#define SN_TYPE_INV_EL30MINI         64    // PPS030
#define SN_TYPE_INV_EL200MINI        65    // PPS028
#define SN_TYPE_INV_AP200            69    // 1-PLP-031-A
#define SN_TYPE_INV_AP300V2          70    // 1-PLP-032-A

/*户用*/
#define SN_TYPE_EP600    	1000//
#define SN_TYPE_EP760    	1001//
#define SN_TYPE_EP800    	1002//
#define SN_TYPE_EP900    	1003//
#define SN_TYPE_EP2000   	1004//
#define SN_TYPE_EP13K    	1005//
#define SN_TYPE_EP6K     	1006//
#define SN_TYPE_EP18K     	1007//
#define SN_TYPE_EP5K     	1008//
#define SN_TYPE_EP5K5     	1009//
#define SN_TYPE_AC2AC       1050

/*微逆*/
#define SN_TYPE_A80    		2000//
#define SN_TYPE_D100S    	2001//
#define SN_TYPE_A100_Sub1G  2002//
#define SN_TYPE_A100_WIFI   2003//
#define SN_TYPE_D100P       2004//
#define SN_TYPE_Will        2005//

/*配件杂项*/
#define SN_TYPE_S1   		3000//
#define SN_TYPE_AT1   		3001//
#define SN_TYPE_COMBOX      3002//
#define SN_TYPE_PBOX   		3003//
#define SN_TYPE_EBOX   		3004//
#define SN_TYPE_HMI           3005  // 房车hmi/Epad
#define SN_TYPE_PANEL         3006  // Epanel
#define SN_TYPE_DCHUB         3007  // DCHUB
#define SN_TYPE_ACHUB         3008  // ACHUB
#define SN_TYPE_SOLARX4K      3009  // D400S
#define SN_TYPE_D400S         SN_TYPE_SOLARX4K
#define SN_TYPE_CHARGER1      3010  // CHARGER1
#define SN_TYPE_CHARGER2      3011  // CHARGER2
#define SN_TYPE_PACK_BOX      3012  // Edock
#define SN_TYPE_BLE_HMI       3013  // BLE_HMI(冰箱宝断码屏)
#define SN_TYPE_IOT_INSIDE    3014  // 逆变器AP300内部IOT
#define SN_TYPE_SHELLY_METER  3015  // shelly电表
#define SN_TYPE_SHELLY_PRO3EM SN_TYPE_SHELLY_METER
#define SN_TYPE_METER         3018	// 自研电表


/*电池包 */
#define SN_TYPE_B500          4000
#define SN_TYPE_B500H         4001
#define SN_TYPE_IB500         4002
#define SN_TYPE_PACK_MIN_START SN_TYPE_IB500
#define SN_TYPE_B1210         4003  //2
#define SN_TYPE_B4810         4004  //3
#define SN_TYPE_B300          4005  //4
#define SN_TYPE_B300K         4006  //5
#define SN_TYPE_B300S         4007  //6
#define SN_TYPE_B1232         4008  //7
#define SN_TYPE_LEADACID      4009  //8
#define SN_TYPE_LFP	          4010  //9
#define SN_TYPE_B500A         4012
#define SN_TYPE_BC200         4013
#define SN_TYPE_B230          4014
#define SN_TYPE_B210          4015
#define SN_TYPE_B500K         4016
#define SN_TYPE_B900          4017
#define SN_TYPE_HB500         4018
#define SN_TYPE_BC260         4019
#define SN_TYPE_EK900         4020
#define SN_TYPE_IB800JP       4021
#define SN_TYPE_B500PRO       4022
#define SN_TYPE_B300PRO       4023
#define SN_TYPE_IB800         4024
#define SN_TYPE_HB500S        4025
#define SN_TYPE_BH500E        4026
#define SN_TYPE_B4805         4027

#define SN_TYPE_COMMON_DEVICE                30000  // 通用设备(不区分电池、逆变器等差异)
#define SN_TYPE_COMMON_BATTERY               30001  // 通用电池
#define SN_TYPE_COMMON_PACK                  SN_TYPE_COMMON_BATTERY
#define SN_TYPE_COMMON_INVERTER              30002  // 通用逆变器
#define SN_TYPE_COMMON_ARM                   SN_TYPE_COMMON_INVERTER
#define SN_TYPE_COMMON_DC_CHARGER            30003  // 通用DC充电设备
#define SN_TYPE_COMMON_DC_DISCHARGE          SN_TYPE_COMMON_DC_CHARGER
#define SN_TYPE_COMMON_DC_DISCHARGER         30004  // 通用DC放电设备
#define SN_TYPE_COMMON_DC_CHARGE             SN_TYPE_COMMON_DC_DISCHARGER
#define SN_TYPE_COMMON_DEVICE_CAN            30005  // 通用设备(CAN,不区分电池、逆变器等差异)
#define SN_TYPE_COMMON_BATTERY_CAN           30006  // 通用电池(CAN)
#define SN_TYPE_COMMON_INVERTER_CAN          30007  // 通用逆变器(CAN)
#define SN_TYPE_COMMON_DC_CHARGER_CAN        30008  // 通用DC充电设备(CAN)
#define SN_TYPE_COMMON_DC_DISCHARGER_CAN     30009  // 通用DC放电设备(CAN)
#define SN_TYPE_COMMON_DEVICE_WIRELESS       30010  // 通用设备(无线,不区分电池、逆变器等差异)
#define SN_TYPE_COMMON_BATTERY_WIRELESS      30011  // 通用电池(无线)
#define SN_TYPE_COMMON_INVERTER_WIRELESS     30012  // 通用逆变器(无线)
#define SN_TYPE_COMMON_DC_CHARGER_WIRELESS   30013  // 通用DC充电设备(无线)
#define SN_TYPE_COMMON_DC_DISCHARGER_WIRELESS 30014 // 通用DC放电设备(无线)

/**********************************************************************************/

/*公司产品条码的ASCII内容*/
/*便携*/
#define SN_TYPE_INV_AC200MAX_ASCII       "AC200MAX"
#define SN_TYPE_INV_AC200L_ASCII         "AC200L"
#define SN_TYPE_INV_AC200MAXII_ASCII     "AC200MAXII"
#define SN_TYPE_INV_AC300_ASCII          "AC300"
#define SN_TYPE_INV_AC500_ASCII          "AC500"
#define SN_TYPE_INV_AC70_ASCII           "AC70"
#define SN_TYPE_AC380_ASCII    	         "AC380"
#define SN_TYPE_INV_AP300_ASCII          "AP300"
#define SN_TYPE_INV_AP200_ASCII          "AP200"
#define SN_TYPE_INV_ELITE200_V2_ASCII    "Elite 200 V2"
#define SN_TYPE_INV_PR200V2_ASCII        "PR200V2"
#define SN_TYPE_INV_AORA200_ASCII        "AORA200"
#define SN_TYPE_INV_RV5_ASCII            "RV5"
#define SN_TYPE_INV_RV_ASCII             SN_TYPE_INV_RV5_ASCII
#define SN_TYPE_FP_ASCII                 "FP"
#define SN_TYPE_INV_EB3A_ASCII           "EB3A"
#define SN_TYPE_INV_AC60_ASCII           "AC60"
#define SN_TYPE_INV_EB55_ASCII           "EB55"
#define SN_TYPE_INV_EB70_ASCII           "EB70"
#define SN_TYPE_INV_AC180T_SINGLE_ASCII  "AC180T-S"  // 单包
#define SN_TYPE_INV_AC180T_DUAL_ASCII    "AC180T-D"  // 双包
#define SN_TYPE_INV_EP500_ASCII          "EP500"
#define SN_TYPE_INV_EP500PRO_ASCII       "EP500Pro"
#define SN_TYPE_INV_AC2A_ASCII           "AC2A"
#define SN_TYPE_INV_AC50B_ASCII          "AC50B"
#define SN_TYPE_INV_AC60P_ASCII          "AC60P"
#define SN_TYPE_INV_AC180_ASCII          "AC180"
#define SN_TYPE_INV_AC200P_ASCII         "AC200P"
#define SN_TYPE_INV_AC240_ASCII          "AC240"
#define SN_TYPE_INV_HANDSFREE1_ASCII     "Handsfree1"
#define SN_TYPE_INV_HANDSFREE2_ASCII     "Handsfree2"
#define SN_TYPE_INV_EL300_ASCII          "EL300"
#define SN_TYPE_INV_AORA300_ASCII        "AORA300"
#define SN_TYPE_INV_EL300P_ASCII         "EL300P"
#define SN_TYPE_INV_EL320_ASCII          "EL320"
#define SN_TYPE_INV_AORA320_ASCII        "AORA320"
#define SN_TYPE_INV_EL400_ASCII          "EL400"
#define SN_TYPE_INV_AORA400_ASCII        "AORA400"
#define SN_TYPE_INV_EL80V2_ASCII         "EL80V2"
#define SN_TYPE_INV_EL100V2_ASCII        "EL100V2"
#define SN_TYPE_INV_PR100V2_ASCII        "PR100V2"
#define SN_TYPE_INV_AORA100V2_ASCII      "AORA100V2"
#define SN_TYPE_INV_EL30V2_ASCII         "EL30V2"
#define SN_TYPE_INV_PR30V2_ASCII         "PR30V2"
#define SN_TYPE_INV_AORA30V2_ASCII       "AORA30V2"
#define SN_TYPE_INV_AC200PL_ASCII        "AC200PL"
#define SN_TYPE_INV_PR002_ASCII          "PR002"
#define SN_TYPE_INV_AC45_ASCII           "AC45"
#define SN_TYPE_INV_AC50P_ASCII          "AC50P"
#define SN_TYPE_INV_AC55_ASCII           "AC55"
#define SN_TYPE_INV_AC70P_ASCII          "AC70P"
#define SN_TYPE_INV_AC240P_ASCII         "AC240P"
#define SN_TYPE_INV_AC180P_ASCII         "AC180P"
#define SN_TYPE_INV_AC2P_ASCII           "AC2P"
#define SN_TYPE_INV_PREMIUM_20C_ASCII    "Premium 20C"
#define SN_TYPE_INV_KW1000_ASCII         "KW1000"
#define SN_TYPE_INV_LFP700_ASCII         "LFP700"
#define SN_TYPE_INV_AORA10_ASCII         "AORA10"
#define SN_TYPE_INV_AORA30P_ASCII        "AORA30P"
#define SN_TYPE_INV_AORA_80_ASCII        "AORA 80"
#define SN_TYPE_INV_AORA_100_ASCII       "AORA 100"
#define SN_TYPE_INV_PLP022_ASCII         "PINa"
#define SN_TYPE_INV_AP500_ASCII          "AP500"
#define SN_TYPE_INV_POWER5_ASCII         "POWER5"
#define SN_TYPE_INV_HS5_ASCII            "HS5"
#define SN_TYPE_INV_HS3_ASCII            "HS3"
#define SN_TYPE_INV_HS2_ASCII            "HS2"
#define SN_TYPE_INV_EL10_ASCII           "EL10"
#define SN_TYPE_INV_EL100MINI_ASCII      "EL100mini"
#define SN_TYPE_INV_EL30MINI_ASCII       "EL30mini"
#define SN_TYPE_INV_EL200MINI_ASCII      "EL200mini"
#define SN_TYPE_INV_AP300V2_ASCII        "AP300V2"

/* 户用 ASCII 定义 */
#define SN_TYPE_EP600_ASCII         "EP600"
#define SN_TYPE_EP760_ASCII         "EP760"
#define SN_TYPE_EP800_ASCII         "EP800"
#define SN_TYPE_EP900_ASCII         "EP900"
#define SN_TYPE_EP2000_ASCII        "EP2000"
#define SN_TYPE_EP13K_ASCII         "EP13K"
#define SN_TYPE_EP6K_ASCII          "EP6K"
#define SN_TYPE_EP18K_ASCII         "EP18K"
#define SN_TYPE_EP5K_ASCII          "EP5K"
#define SN_TYPE_EP5K5_ASCII         "EP5K5"

/* 微逆 ASCII 定义 */
#define SN_TYPE_A80_ASCII           "A80"
#define SN_TYPE_D100S_ASCII         "D100S"
#define SN_TYPE_A100_Sub1G_ASCII    "A100S"
#define SN_TYPE_A100_WIFI_ASCII     "A80P"
#define SN_TYPE_D100P_ASCII         "D100P"
#define SN_TYPE_Will_ASCII          "Will"

/* 配件杂项 ASCII 定义 */
#define SN_TYPE_S1_ASCII            "S1"
#define SN_TYPE_AT1_ASCII           "AT1"
#define SN_TYPE_COMBOX_ASCII        "COMBOX"
#define SN_TYPE_PBOX_ASCII          "PBOX"
#define SN_TYPE_EBOX_ASCII          "EBOX"
#define SN_TYPE_HMI_ASCII           "Epad"
#define SN_TYPE_PANEL_ASCII         "Epanel"
#define SN_TYPE_DCHUB_ASCII         "HD1"
#define SN_TYPE_ACHUB_ASCII         "HA1"
#define SN_TYPE_SOLARX4K_ASCII      "SolarX 4K"
#define SN_TYPE_CHARGER1_ASCII      "CHARGER 1"
#define SN_TYPE_CHARGER2_ASCII      "CHARGER 2"
#define SN_TYPE_PACK_BOX_ASCII      "Edock"
#define SN_TYPE_BLE_HMI_ASCII       "Display 1"
#define SN_TYPE_IOT_INSIDE_ASCII    "AP300_IOT"
#define SN_TYPE_SHELLY_METER_ASCII  "SHELLY_METER"

/* 电池包 ASCII 定义 */
#define SN_TYPE_B500_ASCII          "B500"
#define SN_TYPE_B500H_ASCII         "B500H"
#define SN_TYPE_IB500_ASCII         "IB500"
#define SN_TYPE_B1210_ASCII         "B1210"
#define SN_TYPE_B4810_ASCII         "B4810"
#define SN_TYPE_B300_ASCII          "B300"
#define SN_TYPE_B300K_ASCII         "B300K"
#define SN_TYPE_B300S_ASCII         "B300S"
#define SN_TYPE_B1232_ASCII         "B1232"
#define SN_TYPE_LEADACID_ASCII      "LeadAcid"
#define SN_TYPE_LFP_ASCII           "LFP"
#define SN_TYPE_B500A_ASCII         "B500A"
#define SN_TYPE_BC200_ASCII         "BC200"
#define SN_TYPE_B230_ASCII          "B230"
#define SN_TYPE_B210_ASCII          "B210"
#define SN_TYPE_B500K_ASCII         "B500K"
#define SN_TYPE_B900_ASCII          "B900"
#define SN_TYPE_HB500_ASCII         "HB500"
#define SN_TYPE_BC260_ASCII         "BC260"
#define SN_TYPE_EK900_ASCII         "EK900"
#define SN_TYPE_IB800JP_ASCII       "IB800JP"
#define SN_TYPE_B500PRO_ASCII       "B500Pro"
#define SN_TYPE_B300PRO_ASCII       "B300Pro"
#define SN_TYPE_IB800_ASCII         "IB800"
#define SN_TYPE_HB500S_ASCII        "HB500S"
#define SN_TYPE_BH500E_ASCII        "BH500E"
#define SN_TYPE_B4805_ASCII         "B4805"

/*通用设备伪装ASCII*/
#define SN_TYPE_COMMON_DEVICE_ASCII        "Unknown00"
#define SN_TYPE_COMMON_BATTERY_ASCII       "Unknown01"
#define SN_TYPE_COMMON_INVERTER_ASCII      "Unknown02"
#define SN_TYPE_COMMON_DC_CHARGER_ASCII    "Unknown03"
#define SN_TYPE_COMMON_DC_DISCHARGER_ASCII "Unknown04"
#define SN_TYPE_COMMON_DEVICE_CAN_ASCII    "Unknown05"
#define SN_TYPE_COMMON_BATTERY_CAN_ASCII   "Unknown06"
#define SN_TYPE_COMMON_INVERTER_CAN_ASCII  "Unknown07"
#define SN_TYPE_COMMON_DCCHG_CAN_ASCII     "Unknown08"
#define SN_TYPE_COMMON_DCDCHG_CAN_ASCII    "Unknown09"
#define SN_TYPE_COMMON_DEV_WL_ASCII        "Unknown10"
#define SN_TYPE_COMMON_BAT_WL_ASCII        "Unknown11"
#define SN_TYPE_COMMON_INV_WL_ASCII        "Unknown12"
#define SN_TYPE_COMMON_DCCHG_WL_ASCII      "Unknown13"
#define SN_TYPE_COMMON_DCDCHG_WL_ASCII     "Unknown14"

uint16_t SN_TYPE_ASCII_TO_NUM(char *type);
#define SN_TYPE_SELF  SN_TYPE_ASCII_TO_NUM(SetData.dev_info_t.INV_dev_type)// 本机SN type

/*
局域网设备发现，设备优先级
初步定义建议值，数值越大，优先级越高；数值间隙预留

*/
#define DEV_PRIORITY_INVALID   		0//0-无效
#define DEV_PRIORITY_AT1   		30300//AT1(ATS):30300
#define DEV_PRIORITY_EMS   		30200//EMS:30200
#define DEV_PRIORITY_COMBOX   		30100//COMBOX:30100
#define DEV_PRIORITY_D100S  		30000//D100S:30000
#define DEV_PRIORITY_A100   		20900//A80/A100:20900
#define DEV_PRIORITY   		65535//超级强制主设备-65535（只允许特殊短暂发送）



#define	MESH_FRAME_HEADER_COMMON	0//默认的普通无线设备间报文
#define	MESH_FRAME_HEADER_TRIGER	1//	1-设备发现触发帧（组播,STEP1）
#define	MESH_FRAME_HEADER_SEND_SN	2//	2-设备发现SN信息上报帧（组播,STEP2）
#define	MESH_FRAME_HEADER_FINISH	3//	3-设备发现完成帧（组播,STEP3）
#define	MESH_FRAME_HEADER_FINISH_AFTER	4//设备发现完成后，进入定时周期上报模式，上报自身SN

#define	MESH_FRAME_VERSION_WIFI		1//
#define	MESH_FRAME_VERSION_SUB1GHZ	2//



typedef struct {
	MAC_UNION mac_addr;

    union
    {
        uint16_t DevSn[5];
        struct
        {
            uint64_t SN_64;
            uint16_t Dev_Type;
        };
    };
}new_dev_info;

void sub_dev_info_queue_Push(uint64_t MAC_64, uint64_t SN_64, uint16_t Dev_Type);
void top_dev_info_queue_Push(DISCOVERY_POINT_InfoStruct *data);

void Sub_Net_Point_Bind(void);

void  Sub_Net_Point_TimeOut_Check(void);
void  Top_Net_Point_TimeOut_Check(void);

void  Sub_Net_Point_Serial_Order(uint8_t net_point_update);
void  Top_Net_Point_Serial_Order(uint8_t net_point_update);

void  Sub_Net_Point_Clean(void);
void  Top_Net_Point_Clean(void);


void Modbus_21000_semaphore_init(void);
uint8_t Modbus_21000_semaphore_Take(void);
void Modbus_21000_semaphore_Give(void);
uint8_t Modbus_21000_1_Net_Point_Frame(void);
uint8_t Modbus_21000_3_Net_Point_Frame(void);

uint16_t Udp_multicast_Device_Discovery_TxFrame(uint8_t *outbuf, uint8_t step, uint8_t current_netif_id);
void Device_Discovery_Step(uint8_t *buff,uint16_t *Len, uint8_t current_netif_id); //1s cycle
uint8_t Device_Discovery_Get_Frame( uint8_t *income, uint16_t cmdLen, char *income_ip_str, uint16_t inport);


uint8_t Top_Net_Point_Rx_Get(DISCOVERY_POINT_InfoStruct *queue_msg);
uint8_t  Sub_Net_Point_Rx_Get(new_dev_info *queue_msg);


uint8_t  Sub_Net_Point_Bubble_Sort(void);
uint8_t  Top_Net_Point_Bubble_Sort(void);

uint16_t INV_Point_State_Update(uint8_t x, uint8_t y);
uint16_t INV_DcHub_State_Update(void);
uint16_t INV_AcHub_State_Update(void);
uint16_t INV_D400s_State_Update(void);

bool SN_TYPE_NUM_TO_ASCII(uint16_t type_num, char *out_buffer, size_t buffer_size);

#endif



