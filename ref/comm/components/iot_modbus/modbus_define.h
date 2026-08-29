#pragma once

#include "pack_define.h"
#include "inv_define.h"
#include "parts_type.h"
#include "relay_define.h"

enum {
    MODBUS_TATOL_ADDR = 0,
    MODBUS_MAX_ADDR = 255,
};

#define MODBUS_PACK_TYPE_MAX            20

#define	MOD_REG_START_ADDR_00000		(uint16_t)0
#define	MOD_REG_LEN_00000				((uint16_t)sizeof(MOD_STRUCT_reg00000)>>1)
#define	MOD_REG_START_ADDR_00100		(uint16_t)100
#define	MOD_REG_LEN_00100				((uint16_t)sizeof(MOD_STRUCT_reg00100)>>1)
#define	MOD_REG_START_ADDR_00700		(uint16_t)700//OTA cmd
#define	MOD_REG_LEN_00700				((uint16_t)sizeof(MOD_STRUCT_reg00700)>>1)
#define	MOD_REG_START_ADDR_01100		(uint16_t)1100
#define	MOD_REG_LEN_01100				((uint16_t)sizeof(MOD_STRUCT_reg01100)>>1)
#define	MOD_REG_START_ADDR_01200		(uint16_t)1200
#define	MOD_REG_LEN_01200				((uint16_t)sizeof(MOD_STRUCT_reg01200)>>1)
#define	MOD_REG_START_ADDR_01300		(uint16_t)1300
#define	MOD_REG_LEN_01300				((uint16_t)sizeof(MOD_STRUCT_reg01300)>>1)
#define	MOD_REG_START_ADDR_01400		(uint16_t)1400
#define	MOD_REG_LEN_01400				((uint16_t)sizeof(MOD_STRUCT_reg01400)>>1)
#define	MOD_REG_START_ADDR_01500		(uint16_t)1500
#define	MOD_REG_LEN_01500				((uint16_t)sizeof(MOD_STRUCT_reg01500)>>1)
#define	MOD_REG_START_ADDR_01600		(uint16_t)1600
#define	MOD_REG_LEN_01600				((uint16_t)sizeof(MOD_STRUCT_reg01600)>>1)
#define	MOD_REG_START_ADDR_01700		(uint16_t)1700//
#define	MOD_REG_LEN_01700				((uint16_t)sizeof(MOD_STRUCT_reg01700)>>1)//
#define	MOD_REG_START_ADDR_02000		(uint16_t)2000
#define	MOD_REG_LEN_02000				((uint16_t)sizeof(MOD_STRUCT_reg02000)>>1)
#define	MOD_REG_START_ADDR_02200		(uint16_t)2200
#define	MOD_REG_LEN_02200				((uint16_t)sizeof(MOD_STRUCT_reg02200)>>1)
#define	MOD_REG_START_ADDR_02300		(uint16_t)2300
#define	MOD_REG_LEN_02300				((uint16_t)sizeof(MOD_STRUCT_reg02300)>>1)
#define	MOD_REG_START_ADDR_02400		(uint16_t)2400
#define	MOD_REG_LEN_02400				((uint16_t)sizeof(MOD_STRUCT_reg02400)>>1)
#define	MOD_REG_START_ADDR_02500		(uint16_t)2500
#define	MOD_REG_LEN_02500				((uint16_t)sizeof(MOD_STRUCT_reg02500)>>1)
#define	MOD_REG_START_ADDR_03000		(uint16_t)3000
#define	MOD_REG_LEN_03000				((uint16_t)sizeof(MOD_STRUCT_reg03000)>>1)
#define	MOD_REG_START_ADDR_03500		(uint16_t)3500
#define	MOD_REG_LEN_03500				((uint16_t)sizeof(MOD_STRUCT_reg03500)>>1)
#define	MOD_REG_START_ADDR_03600		(uint16_t)3600
#define	MOD_REG_LEN_03600				((uint16_t)sizeof(MOD_STRUCT_reg03600)>>1)
#define	MOD_REG_START_ADDR_03700		(uint16_t)3700//
#define	MOD_REG_LEN_03700				((uint16_t)sizeof(MOD_STRUCT_reg03700)>>1)//
#define	MOD_REG_START_ADDR_04000		(uint16_t)4000
#define	MOD_REG_LEN_04000				((uint16_t)sizeof(MOD_STRUCT_reg04000)>>1)
#define	MOD_REG_START_ADDR_04050		(uint16_t)4050
#define	MOD_REG_LEN_04050				((uint16_t)sizeof(MOD_STRUCT_reg04050)>>1)
#define	MOD_REG_START_ADDR_04105		(uint16_t)4105
#define	MOD_REG_LEN_04105				((uint16_t)sizeof(MOD_STRUCT_reg04105)>>1)
#define	MOD_REG_START_ADDR_06000		(uint16_t)6000
#define	MOD_REG_LEN_06000				((uint16_t)sizeof(MOD_STRUCT_reg06000)>>1)
#define	MOD_REG_START_ADDR_06100		(uint16_t)6100
#define	MOD_REG_LEN_06100				((uint16_t)sizeof(MOD_STRUCT_reg06100)>>1)
#define	MOD_REG_START_ADDR_06300		(uint16_t)6300
#define	MOD_REG_LEN_06300				((uint16_t)sizeof(MOD_STRUCT_reg06300)>>1)
#define	MOD_REG_START_ADDR_07000		(uint16_t)7000
#define	MOD_REG_LEN_07000				((uint16_t)sizeof(MOD_STRUCT_reg07000)>>1)
#define	MOD_REG_START_ADDR_07200		(uint16_t)7200
#define	MOD_REG_LEN_07200				((uint16_t)sizeof(MOD_STRUCT_reg07200)>>1)
#define	MOD_REG_START_ADDR_11000		(uint16_t)11000
#define	MOD_REG_LEN_11000				((uint16_t)sizeof(MOD_STRUCT_reg11000)>>1)
#define	MOD_REG_START_ADDR_12000		(uint16_t)12000
#define	MOD_REG_LEN_12000				((uint16_t)sizeof(MOD_STRUCT_reg12000)>>1)
#define	MOD_REG_START_ADDR_13000		(uint16_t)13000
#define	MOD_REG_LEN_13000				((uint16_t)sizeof(MOD_STRUCT_reg13000)>>1)
#define	MOD_REG_START_ADDR_13500		(uint16_t)13500
#define	MOD_REG_LEN_13500				((uint16_t)sizeof(MOD_STRUCT_reg13500)>>1)
#define	MOD_REG_START_ADDR_13600		(uint16_t)13600
#define	MOD_REG_LEN_13600				((uint16_t)sizeof(MOD_STRUCT_reg13600)>>1)
#define	MOD_REG_START_ADDR_14000		(uint16_t)14000//
#define	MOD_REG_LEN_14000				((uint16_t)sizeof(MOD_STRUCT_reg14000)>>1)//
#define	MOD_REG_START_ADDR_14500		(uint16_t)14500//
#define	MOD_REG_LEN_14500				((uint16_t)sizeof(MOD_STRUCT_reg14500)>>1)
#define	MOD_REG_START_ADDR_14700		(uint16_t)14700//
#define	MOD_REG_LEN_14700				((uint16_t)sizeof(MOD_STRUCT_reg14700)>>1)
#define	MOD_REG_START_ADDR_15500		(uint16_t)15500
#define	MOD_REG_LEN_15500				((uint16_t)sizeof(MOD_STRUCT_reg15500)>>1)
#define	MOD_REG_START_ADDR_15600		(uint16_t)15600
#define	MOD_REG_LEN_15600				((uint16_t)sizeof(MOD_STRUCT_reg15600)>>1)
#define	MOD_REG_START_ADDR_15700		(uint16_t)15700
#define	MOD_REG_LEN_15700				((uint16_t)sizeof(MOD_STRUCT_reg15700)>>1)
#define	MOD_REG_START_ADDR_15750		(uint16_t)15750
#define	MOD_REG_LEN_15750				((uint16_t)sizeof(MOD_STRUCT_reg15750)>>1)
#define	MOD_REG_START_ADDR_19000		(uint16_t)19000
#define	MOD_REG_LEN_19000				((uint16_t)sizeof(MOD_STRUCT_reg19000)>>1)
#define	MOD_REG_START_ADDR_19100		(uint16_t)19100
#define	MOD_REG_LEN_19100				((uint16_t)sizeof(MOD_STRUCT_reg19100)>>1)
#define	MOD_REG_START_ADDR_19200		(uint16_t)19200
#define	MOD_REG_LEN_19200				((uint16_t)sizeof(MOD_STRUCT_reg19200)>>1)
#define	MOD_REG_START_ADDR_19300		(uint16_t)19300
#define	MOD_REG_LEN_19300				((uint16_t)sizeof(MOD_STRUCT_reg19300)>>1)
#define	MOD_REG_START_ADDR_21000		(uint16_t)21000
#define	MOD_REG_LEN_21000				((uint16_t)sizeof(MOD_STRUCT_reg21000)>>1)
#define	MOD_REG_START_ADDR_22000		(uint16_t)22000
#define	MOD_REG_LEN_22000				((uint16_t)sizeof(MOD_STRUCT_reg22000)>>1)
#define	MOD_REG_START_ADDR_29700		(uint16_t)29700
#define	MOD_REG_LEN_29700				((uint16_t)sizeof(MOD_STRUCT_reg29700)>>1)
#define	MOD_REG_START_ADDR_29800		(uint16_t)29800
#define	MOD_REG_LEN_29800				((uint16_t)sizeof(MOD_STRUCT_reg29800)>>1)
#define	MOD_REG_START_ADDR_30000		(uint16_t)30000
#define	MOD_REG_LEN_30000				((uint16_t)sizeof(MOD_STRUCT_reg30000)>>1)
#define	MOD_REG_START_ADDR_30900		(uint16_t)30900
#define	MOD_REG_LEN_30900				((uint16_t)sizeof(MOD_STRUCT_reg30900)>>1)
#define	MOD_REG_START_ADDR_40000		(uint16_t)40000
#define	MOD_REG_LEN_40000				((uint16_t)sizeof(MOD_STRUCT_reg40000)>>1)

#pragma pack(1)

typedef struct {
    MOD_STRUCT_reg00000 mod_reg00000; //  6~16, 其他INV内部的IOT
    MOD_STRUCT_reg00100 mod_reg00100_AppPage1; //100~168  APP首页数据（汇总）    
    MOD_STRUCT_reg00700 mod_reg00700_OTA; //700~768  OTA设置区
    MOD_STRUCT_reg01100 mod_reg01100_Inv_base; //1100~1154  每台逆变器基本信息
    MOD_STRUCT_reg01200 mod_reg01200_Inv_pv; //1200~1289  每台逆变器PV信息
    MOD_STRUCT_reg01300 mod_reg01300_Inv_grid; //1300~1330 每台逆变器电网信息 
    MOD_STRUCT_reg01400 mod_reg01400_Inv_load; //1400~1447  每台逆变器负载信息
    MOD_STRUCT_reg01500 mod_reg01500_Inv_inv; //1500~1529  每台逆变器逆变信息
    MOD_STRUCT_reg01600 mod_reg01600_Inv_generator; //1600~1613 每台逆变器发电机信息
    MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067  基础设置项
    MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245  每台逆变高级设置区
    // MOD_STRUCT_reg02300 mod_reg02300_Inv_set02_struct; //2300~2400  电网认证区
    MOD_STRUCT_reg02400 mod_reg02400_Inv_certification; //2400~2449  认证设置项
    // MOD_STRUCT_reg02500 mod_reg02500_Inv_advance_set2; //2500~2544  每台逆变高级设置区2（面向微逆）
    MOD_STRUCT_reg03000 mod_reg03000_Inv_history; //3000~3026  每台逆变器历史记录区
    MOD_STRUCT_reg03500 mod_reg03500_Inv_yearX_statistic; //3500~3547    每台逆变器累计年电量区
    MOD_STRUCT_reg03600 mod_reg03600_Inv_year1_statistic; //3600~3658  每台逆变器当年电量区
    MOD_STRUCT_reg03700 mod_reg03700_Inv_day_energy;//3700~ 逆变器当日能量区
//    MOD_STRUCT_reg04000 mod_reg04000_Dsp_data;//4000~4049 dsp数据区
//    MOD_STRUCT_reg04050 mod_reg04050_Dsp_set1;//4050~4104 dsp设置区1
//    MOD_STRUCT_reg04105 mod_reg04105_Dsp_set2;//4105~4177 dsp设置区2
//    MOD_STRUCT_reg06000 mod_reg06000_Pack_sum; //6000~6030    pack主包汇总信息
//    MOD_STRUCT_reg06100 mod_reg06100_Pack_each; //6100~6203  pack单包信息
    // MOD_STRUCT_reg06300 mod_reg06300_Pack_cell[18]; //6300~xxx  pack电芯信息
//    MOD_STRUCT_reg07000 mod_reg07000_Pack_set; //7000~7004  pack设置区
    // MOD_STRUCT_reg07200 mod_reg07200_Pack_bmu[18]; //7200~xxx      BMU信息区
    MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040    IOT信息区（只读，面向客户）
    MOD_STRUCT_reg12000 mod_reg12000_IOT_set; //12000~12999 IOT设置区
    // MOD_STRUCT_reg13000 mod_reg13000_3rd_WIFI; //第3方WiFi接入信息管理
    MOD_STRUCT_reg13500 mod_reg13500_mesh; //13500~13511   WIFI MESH区
    MOD_STRUCT_reg13600 mod_reg13600_open; //13600~13810 开放区
//    MOD_STRUCT_reg14000 mod_reg14000_HMI_info;////14000~14013    HMI信息区    
//    MOD_STRUCT_reg14500 mod_reg14500_SmartPlug_info;//14500~14525 智能插座只读区（周期上报）
//    MOD_STRUCT_reg14700 mod_reg14700_SmartPlug_set;//14700~14731 智能插座设置区
    // MOD_STRUCT_reg15700 mod_reg15700_Dc_Hub_info;//15700~15749 dchub信息区
    // MOD_STRUCT_reg15750 mod_reg15750_Dc_Ac_Hub_set;//15750~15760 dc、achub设置区
    // MOD_STRUCT_reg15800 mod_reg15800_Ac_Hub_info;//15800~15850 achub信息区
    // MOD_STRUCT_reg16000 mod_reg16000_Panel_dc_info;//16000 Panel DC信息区
    // MOD_STRUCT_reg16150 mod_reg16150_Panel_ac_info;//16150 Panel AC信息区
    // MOD_STRUCT_reg16300 mod_reg16300_Panel_protect;//16300 Panel 保护设置区（预留）
    // MOD_STRUCT_reg16400 mod_reg16400_Panel_set;    //16400 Panel 设置区
    MOD_STRUCT_reg19000 mod_reg19000_Relay_ctrl;//19000~19031 继电器智能控制区1
    MOD_STRUCT_reg19100 mod_reg19100_Relay_ctrl_2;//19100~19139 继电器智能控制区2
    MOD_STRUCT_reg19200 mod_reg19200_Backup_power;//19200~19231 预约备电区
    MOD_STRUCT_reg19300 mod_reg19300_Relay_ctrl_3;//19300~19331 继电器智能控制区3
	MOD_STRUCT_reg21000	mod_reg21000_data;//21000 通用数据
	MOD_STRUCT_reg21000_WR	mod_reg21000_bind_WR;
	MOD_STRUCT_reg22000	mod_reg22000_net_server_2rd;
//    MOD_STRUCT_reg29700 mod_reg29700_IOT_info;//29700~29720 IOT标定区（从模式写入）
    MOD_STRUCT_reg29800 mod_reg29800_softver_info;//软件版本汇总上报（29801~29900）
	MOD_STRUCT_reg30900 mod_reg30900_test;
    MOD_STRUCT_reg40000 mod_reg40000_transparent;//40000~40511 透传区域
}MOD_STRUCT_Inv;//INV设备modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg01700 meter_info; //1700~1790 每台电表的详细信息    
}MOD_STRUCT_Meter;//电表总变量结构体

typedef struct {
    MOD_STRUCT_reg06000 mod_reg06000_Pack_sum; //6000~6030    pack主包汇总信息
    MOD_STRUCT_reg06100 mod_reg06100_Pack_each; //6100~6203  pack单包信息
    MOD_STRUCT_reg06300 mod_reg06300_Pack_cell; //6300~xxx  pack电芯信息 [18]
    MOD_STRUCT_reg07000 mod_reg07000_Pack_set; //7000~7004  pack设置区
    MOD_STRUCT_reg07200 mod_reg07200_Pack_bmu; //7200~xxx      BMU信息区	[18]
    MOD_STRUCT_reg30000 mod_reg30000_Pack_paygo;//30000~30099
    MOD_STRUCT_reg40000 mod_reg40000_transparent;//40000~40511 透传区域
}MOD_STRUCT_Pack;//电池包 modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg06100 mod_reg06100_Pack_each; //6100~6203  pack单包信息
}MOD_STRUCT_Type_Pack;//电池包类型汇总

typedef struct {
    MOD_STRUCT_reg13600 mod_reg13600_open; //13600~13810 开放区
    MOD_STRUCT_reg14500 mod_reg14500_SmartPlug_info;//14500~14525 智能插座只读区（周期上报）
    MOD_STRUCT_reg14700 mod_reg14700_SmartPlug_set;//14700~14731 智能插座设置区
}MOD_STRUCT_PLUG;//S1 plug设备modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg00100 mod_reg00100_AppPage1; //100~168  APP首页数据（汇总） 
    MOD_STRUCT_reg01100 mod_reg01100_Inv_base; //1100~1154  每台逆变器基本信息
	MOD_STRUCT_reg15700 mod_reg15700_Dc_Hub_info;//15700~15749 dchub信息区
	MOD_STRUCT_reg15750 mod_reg15750_Dc_Ac_Hub_set;//15750~15760 dc、achub设置区
}MOD_STRUCT_DCHUB;//DcHUB设备modbus beta总变量结构体

typedef struct {
    MOD_STRUCT_reg11000 mod_reg11000_IOT_info; //11000~11040    IOT信息区（只读，面向客户）
	MOD_STRUCT_reg15500 mod_reg15500_DCDC_info;// 	DCDC模块通用 信息区
	MOD_STRUCT_reg15600 mod_reg15600_DCDC_set;// 	DCDC模块通用 设置区
}MOD_STRUCT_Dcdc;// modbus beta总变量结构体,read 

typedef struct {
	MOD_STRUCT_reg30900 mod_reg30900_IoT_Test;
    MOD_STRUCT_reg40000 mod_reg40000_IOT_Debug;
}MOD_STRUCT_IOTSELF;// modbus beta总变量结构体,read 

typedef struct {
    MOD_STRUCT_IOTSELF Self_IoT;
	MOD_STRUCT_Inv Inv[(INV_MAX_NUM)+1];//top_modbus_rd.Inv
	MOD_STRUCT_Pack Pack[PACK_MAX_NUM+1];//分3段：前：INV内部集成的PACK；中：独立PACK；尾部：汇总
	MOD_STRUCT_Type_Pack Type_Pack[MODBUS_PACK_TYPE_MAX];//不同类型电池包汇总
	MOD_STRUCT_DCHUB DcHub[DC_HUB_MAX_NUM+1];
    MOD_STRUCT_Dcdc Dcdc[DCDC_MAX_NUM+1];
}STRUCT_TOP_MODBUS_READ;

typedef struct {
	MOD_STRUCT_Inv Inv;//top_modbus_rd.Inv
	MOD_STRUCT_Pack Pack;//top_modbus_rd.Pack
	MOD_STRUCT_DCHUB DcHub;
    MOD_STRUCT_Dcdc Dcdc;
}STRUCT_TOP_MODBUS_WRITE;

typedef struct {
    OTHER_INV_DATA_STRUCT  bind_dev; // 需要绑定的设备，例如蓝牙绑定逆变器
	MOD_STRUCT_PLUG Plug[PLUG_MAX_NUM + 1];
	MOD_STRUCT_Meter Meter[NET_WIFI_METER_POINT + 1];
    OTHER_INV_BETA_STRUCT other_inv[NET_WIFI_INVBAT_POINT_MINI+1];
}STRUCT_OTHER_READ;
extern STRUCT_OTHER_READ g_other_rd;

typedef struct {
    OTHER_INV_DATA_STRUCT bind_dev; 
	OTHER_INV_BETA_STRUCT other_inv;
    MOD_STRUCT_PLUG Plug;
//plug
}STRUCT_OTHER_WRITE;// 

#pragma pack()

/**
 * modbus 读写总变量结构体，modbus协议读/写变量均操作此部分内容
 */
extern STRUCT_TOP_MODBUS_READ top_modbus_rd;
extern STRUCT_TOP_MODBUS_WRITE top_modbus_wr;
extern STRUCT_OTHER_WRITE g_other_wr;

