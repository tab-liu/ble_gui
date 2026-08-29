/**
  ******************************************************************************
  * @file      bms_type.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/1/8
  * @brief     bms_alpha相关定义
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/8   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once
#include "comm_define.h"

/* ================================ 头文件宏定义 ================================ */

// 电池包数量
#define MAX_BMS_NUM         COMMON_PACK_MAX_NUM

#define MAX_CELL_NUM        16   // 每包最多16节电芯

#define CAN_ID_ALPHA_FLAG   0x0CF5 // CAN协议标志位

/* ============================== 头文件结构体定义 ================================ */

#pragma pack (1)

typedef union {  // 0x0CF50X35
    uint16_t all;
    struct {
        uint8_t chg_over_temp : 1;  // bit0 充电过温保护
        uint8_t dsg_over_temp : 1;  // bit1 放电过温保护
        uint8_t chg_under_temp : 1;  // bit2 充电低温保护
        uint8_t dsg_under_temp : 1;  // bit3 放电低温保护
        uint8_t tot_v_over   : 1;  // bit4 总压过压
        uint8_t tot_v_under  : 1;  // bit5 总电压欠压
        uint8_t cell_v_over  : 1;  // bit6 单节过压
        uint8_t cell_v_under : 1;  // bit7 单节欠压
        uint8_t curr_over_level1 : 1; // bit8 过流保护1
        uint8_t curr_over_level2 : 1; // bit9 过流保护2
        uint8_t curr_over_level3 : 1; // bit10 过流保护3
        uint8_t curr_over_level4 : 1; // bit11 过流保护4
        uint8_t pre_chg_error : 1; // bit12 预充故障
        uint8_t short_current : 1; // bit13 短路保护
        uint8_t chg_MOS_state : 1; // bit14 充电MOS状态
        uint8_t dsg_MOS_state : 1; // bit15 放电MOS状态
    }bit;
}bms_protect1_t; // 0x01

typedef union { // 0x0CF50X35
    uint16_t all;
    struct {
        uint8_t chg_over_curr1 : 1;  //   bit0 充电过流保护1
        uint8_t chg_over_curr2 : 1;  //   bit1 充电过流保护2
        uint8_t chg_over_curr3 : 1;  //   bit2 充电过流保护3
        uint8_t chg_over_curr4 : 1;  //   bit3 充电过流保护4
        uint8_t dsg_over_curr1 : 1;  //   bit4 放电过流保护1
        uint8_t dsg_over_curr2 : 1;  //   bit5 放电过流保护2
        uint8_t dsg_over_curr3 : 1;  //   bit6 放电过流保护3
        uint8_t dsg_over_curr4 : 1;  //   bit7 放电过流保护4
        uint8_t rsv8 : 1;   // Bit 8:    保留0
        uint8_t rsv9 : 1;   // Bit 9:    保留1
        uint8_t rsv10 : 1;  // Bit 10:   保留2
        uint8_t rsv11 : 1;  // Bit 11:   保留3
        uint8_t rsv12 : 1;  // Bit 12:   保留4
        uint8_t rsv13 : 1;  // Bit 13:   保留5
        uint8_t rsv14 : 1;  // Bit 14:   保留6
        uint8_t rsv15 : 1;  // Bit 15:   保留7
    }bit;
}bms_protect2_t; // 0x01

typedef union {
    uint16_t all;
    struct {
        uint8_t total_ov         : 1;  // Bit 0: 总压过压保护
        uint8_t reserved_1       : 1;  // Bit 1: 保留
        uint8_t chg_over_curr1  : 1;  // Bit 2: 充电过流保护1
        uint8_t chg_over_curr2  : 1;  // Bit 3: 充电过流保护2
        uint8_t chg_over_curr3  : 1;  // Bit 4: 充电过流保护3
        uint8_t chg_over_temp     : 1;  // Bit 5: 充电过温保护
        uint8_t chg_under_temp      : 1;  // Bit 6: 充电低温保护
        uint8_t cell_ov    : 1;  // Bit 7: 单节过压保护
        uint8_t high_soc : 1;  // Bit 8: 电池高电荷保护
        uint8_t reserved1     : 1;  // Bit 9: 充电故障保留
        uint8_t reserved2     : 1;  // Bit 10: 充电故障保留
        uint8_t reserved3     : 1;  // Bit 11: 充电故障保留
        uint8_t reserved4     : 1;  // Bit 12: 充电故障保留
        uint8_t reserved5     : 1;  // Bit 13: 充电故障保留
        uint8_t reserved6     : 1;  // Bit 14: 充电故障保留
        uint8_t reserved7     : 1;  // Bit 15: 充电故障保留
    }bit;
}md_show_protect1_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t total_uv        : 1;  // Bit 0: 总压欠压保护
        uint8_t reserved_2      : 1;  // Bit 1: 保留
        uint8_t dsg_over_curr1  : 1;  // Bit 2: 放电过流保护1
        uint8_t dsg_over_curr2  : 1;  // Bit 3: 放电过流保护2
        uint8_t dsg_over_curr3  : 1;  // Bit 4: 放电过流保护3
        uint8_t dsg_over_temp   : 1;  // Bit 5: 放电过温保护
        uint8_t dsg_under_temp  : 1;  // Bit 6: 放电低温保护
        uint8_t cell_uv       : 1;  // Bit 7: 单节欠压保护
        uint8_t reserved2     : 1;  // Bit 8: 保留1
        uint8_t reserved3     : 1;  // Bit 9: 保留2
        uint8_t reserved4     : 1;  // Bit 10: 保留3
        uint8_t reserved5     : 1;  // Bit 11: 保留4
        uint8_t reserved6     : 1;  // Bit 12: 保留5
        uint8_t reserved7     : 1;  // Bit 13: 保留6
        uint8_t reserved8     : 1;  // Bit 14: 保留7
        uint8_t reserved9     : 1;  // Bit 15: 保留8
    }bit;
}md_show_protect2_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t cell_v_diff_fault : 1;  // Bit 0: 单体压差故障
        uint8_t temp_diff_fault   : 1;  // Bit 1: 电池温差故障
        uint8_t cell_fail_fault   : 1;  // Bit 2: 单节失效故障
        uint8_t low_temp_fault    : 1;  // Bit 3: 低温故障
        uint8_t cluster_short     : 1;  // Bit 4: 电池簇短路故障
        uint8_t insulation_leak   : 1;  // Bit 5: 绝缘漏电故障
        uint8_t main_relay_stick  : 1;  // Bit 6: 主继电器黏连
        uint8_t main_relay_open   : 1;  // Bit 7: 主继电器开路故障
        uint8_t precharge_fault1  : 1;  // Bit 8: 预充故障1
        uint8_t power_down_fail   : 1;  // Bit 9: 下电失败
        uint8_t fuse_open         : 1;  // Bit 10: FUSE开路
        uint8_t relay_drv_short   : 1;  // Bit 11: 继电器驱动短路
        uint8_t hv_interlock      : 1;  // Bit 12: 高压互锁异常
        uint8_t bmu_comm_fault    : 1;  // Bit 13: BMU通信故障
        uint8_t power_line_off    : 1;  // Bit 14: 功率线未接
        uint8_t balance_mos_fail  : 1;  // Bit 15: 均衡MOS失效
    }bit;
}md_show_error1_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t balance_fail       : 1;  // Bit 0: 均衡失效
        uint8_t bmu_power_anomaly  : 1;  // Bit 1: BMU供电异常
        uint8_t addr_anomaly       : 1;  // Bit 2: 地址异常
        uint8_t param_init_fault   : 1;  // Bit 3: 参数初始化异常
        uint8_t safety_comm_fault  : 1;  // Bit 4: 安全核通信故障
        uint8_t hvboard_comm_fault : 1;  // Bit 5: 高压板通信故障
        uint8_t circuit_breaker    : 1;  // Bit 6: 断路器故障
        uint8_t safety_core_release: 1;  // Bit 7: 安全核脱扣
        uint8_t reverse_fault      : 1;  // Bit 8: 反接故障
        uint8_t pcs_comm_anomaly   : 1;  // Bit 9: pcs通信异常
        uint8_t bus_volt_calib_err : 1;  // Bit 10: 母线电压校准错误
        uint8_t sys5v_fault        : 1;  // Bit 11: 系统5V故障
        uint8_t heater_fail        : 1;  // Bit 12: 加热器失效
        uint8_t current_sample_err : 1;  // Bit 13: 电流采样异常
        uint8_t precharge_fault2   : 1;  // Bit 14: 预充故障2
        uint8_t master_comm_fault  : 1;  // Bit 15: 主从通信故障
    }bit;
}md_show_error2_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t sys_fault_rsv0 : 1;  // Bit 0: 系统故障保留0
        uint8_t sys_fault_rsv1 : 1;  // Bit 1: 系统故障保留1
        uint8_t sys_fault_rsv2 : 1;  // Bit 2: 系统故障保留2
        uint8_t sys_fault_rsv3 : 1;  // Bit 3: 系统故障保留3
        uint8_t sys_fault_rsv4 : 1;  // Bit 4: 系统故障保留4
        uint8_t sys_fault_rsv5 : 1;  // Bit 5: 系统故障保留5
        uint8_t sys_fault_rsv6 : 1;  // Bit 6: 系统故障保留6
        uint8_t sys_fault_rsv7 : 1;  // Bit 7: 系统故障保留7
        uint8_t sys_fault_rsv8 : 1;  // Bit 8: 系统故障保留8
        uint8_t sys_fault_rsv9 : 1;  // Bit 9: 系统故障保留9
        uint8_t sys_fault_rsv10: 1;  // Bit 10: 系统故障保留10
        uint8_t sys_fault_rsv11: 1;  // Bit 11: 系统故障保留11
        uint8_t sys_fault_rsv12: 1;  // Bit 12: 系统故障保留12
        uint8_t sys_fault_rsv13: 1;  // Bit 13: 系统故障保留13
        uint8_t sys_fault_rsv14: 1;  // Bit 14: 系统故障保留14
        uint8_t sys_fault_rsv15: 1;  // Bit 15: 系统故障保留15
    }bit;
}md_show_error3_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t total_over_volt : 1; //bit0 总压过压
        uint8_t chg_over_temp : 1; //bit1 充电过温
        uint8_t cell_over_volt : 1; //bit2 单节过压
        uint8_t chg_under_temp : 1; //bit3 充电低温
        uint8_t dsg_over_temp : 1; //bit4 放电过温
        uint8_t total_under_volt : 1;//bit5 总压欠压
        uint8_t dsg_under_temp : 1; //bit6 放电低温
		uint8_t cell_under_volt : 1; // bit7 单节欠压
        uint8_t soc_under : 1; //bit8 低电量
        uint8_t usb_alarm : 1; //bit9 USB输出告警
        uint8_t rsv : 6;
    }bit;
}md_show_alarm1_t; // 保留

typedef union {
    uint16_t val;       // 整体16位数值
    struct {
        uint16_t vol_total_high    :1;  // bit0 总压过高 E081
        uint16_t vol_total_low     :1;  // bit1 总压过低 E082
        uint16_t vol_cell_high     :1;  // bit2 单体过压 E083
        uint16_t vol_cell_low      :1;  // bit3 单体欠压 E084
        uint16_t temp_charge_high  :1;  // bit4 充电过温 E085
        uint16_t temp_charge_low   :1;  // bit5 充电欠温 E086
        uint16_t temp_discharge_high:1; // bit6 放电过温 E087
        uint16_t temp_discharge_low :1; // bit7 放电欠温 E088
        uint16_t curr_charge_over  :1;  // bit8 充电过流 E089
        uint16_t curr_discharge_over:1; // bit9 放电过流 E090
        uint16_t precharge_fail    :1;  // bit10 预充故障 E091
        uint16_t short_circuit     :1;  // bit11 短路保护 E092
        uint16_t afe_err           :1;  // bit12 模拟前端故障 E093
        uint16_t host_conflict     :1;  // bit13 电池包主机冲突 E094
        uint16_t curr_protect1     :1;  // bit14 过流保护1 E095
        uint16_t curr_protect2     :1;  // bit15 过流保护2 E096
    } bit;
} md_show_protect3_t;

typedef union {
    uint16_t val;
    struct {
        uint16_t curr_protect3     :1;  // bit0 过流保护3 E097
        uint16_t curr_protect4     :1;  // bit1 过流保护4 E098
        uint16_t sop_charge        :1;  // bit2 充电SOP保护 E099
        uint16_t high_soc_protect  :1;  // bit3 电池高电荷保护 E100
        uint16_t device_temp_over  :1;  // bit4 器件过温保护 E101
        uint16_t reserve0          :1;  // bit5 预留 E102
        uint16_t reserve1          :1;  // bit6 预留 E103
        uint16_t reserve2          :1;  // bit7 预留 E104
        uint16_t reserve3          :1;  // bit8 预留 E105
        uint16_t reserve4          :1;  // bit9 预留 E106
        uint16_t reserve5          :1;  // bit10 预留 E107
        uint16_t reserve6          :1;  // bit11 预留 E108
        uint16_t reserve7          :1;  // bit12 预留 E109
        uint16_t reserve8          :1;  // bit13 预留 E110
        uint16_t reserve9          :1;  // bit14 预留 E111
        uint16_t reserve10         :1;  // bit15 预留 E112
    } bit;
} md_show_protect4_t;

typedef union {
    uint16_t all;
    struct {
        uint16_t volt : 15;
        uint8_t bal : 1;
    }bit;
}cell_info;

typedef union {
    uint16_t all;
    struct {
        uint8_t enable_chg : 1; //bit0 0: 不能充，1:能充
        uint8_t enable_dsg : 1; //bit1 0: 不能放，1:能放
        uint8_t now_need_chg: 1;//bit2 0: 不需要，1:需要立即充电
        uint8_t limit_chg : 1; //bit3 0:无    1:-10℃不建议大载放电
        uint8_t low_energy: 1; //bit4 低能量
        uint8_t sync_doing: 1; //bit5 并机中
        uint8_t temp_alarm: 1; //bit6温度异常
        uint8_t rsv : 1; //bit7
    }bit;
}chg_enable_t;

typedef union {
    uint16_t all;
    struct {
        uint8_t stop_chg : 1; //bit0 1: 不能充，0:能充
        uint8_t stop_dsg : 1; //bit1 1: 不能放，0:能放
    }bit;
}to_state_t;

typedef union{
    uint16_t all;
    struct {
        uint8_t is_ver_diff : 1; // bit0 1:固件版本有差异 0:固件版本一致
        uint8_t is_busy : 1; // bit1 1:忙碌 0:空闲 忙碌不允许升级
    }bit;
} IOT_FACTORY_t;

// 电池包型号来源定义 - 优先级从高到低
typedef enum {
    TYPE_SOURCE_NONE = 0,        // 无来源
    TYPE_SOURCE_0X10 = 1,        // 最低优先级：0x10帧
    TYPE_SOURCE_VERSION = 2,     // 中等优先级：依赖软件版本号(0x3D等帧)
    TYPE_SOURCE_0X56 = 3,        // 最高优先级：0x56帧
} type_source_e;

typedef struct {
    uint16_t valid;
    uint16_t timeout;
    uint16_t devId;
    uint8_t dev_access_state;   // 设备接入标志，用于设备接入日志模块 0:无效/离线，1：接入
    char type[12];  // 6101~6110
    uint16_t sync;
    uint64_t sn_code;
    uint16_t mcu_num;
    uint16_t mcu_type[2];
    uint32_t version[2];
    uint8_t ver_u8[3]; // 对应CAN协议01、02软件版本号，计算方式：[0] + [1] * 100 + [2] * 10000
    uint16_t total_voltage; // 6111~6112 单包总电压 0.01 
    uint16_t total_current; // 单包总电流 0.1
    uint16_t soc; //6113
    uint16_t soh; //6114   
    uint32_t curr_chg_energy;  // 0.001Kwh
    uint32_t curr_dsg_energy;
    uint16_t PackInner;
    uint32_t total_chg_energy;  // 0.01Kwh
    uint32_t total_dsg_energy;
    uint16_t min_cell_volt;  // 6116~6117
    uint16_t max_cell_volt;
    uint16_t min_cell_index; // 6118
    uint16_t max_cell_index; // 6119
    uint16_t avg_temp;     //6115
    uint16_t min_temp_value;// 6120
    uint16_t max_temp_value;// 6121
    uint16_t min_temp_index;// 6122
    uint16_t max_temp_index;// 6123
    uint16_t run_state; //6124  11,22,33
    uint16_t charging_state;//6125  0：空闲；1：充电；2：放电
    uint16_t sync_bit; //6129 电池并容
    uint16_t online_bit; // 电池通讯在线
    uint16_t used_cycle;
    bms_protect1_t protect1; //  CAN 0x35
    bms_protect2_t protect2; // CAN 0x35
    md_show_protect1_t show_protect1; // modbus用于显示给手机  6144
    md_show_protect2_t show_protect2; // modbus用于显示给手机  6145
    md_show_error1_t show_fault1; // modbus用于显示给手机  6146
    md_show_error2_t show_fault2; // modbus用于显示给手机  6147
    md_show_error3_t show_fault3; // modbus用于显示给手机  6148
    md_show_alarm1_t show_alarm1; // modbus用于显示给手机  6149
    md_show_protect3_t show_protect3; // modbus用于显示给手机  6164
    md_show_protect4_t show_protect4; // modbus用于显示给手机  6165
	 uint16_t chg_full_time; //6150 电池充满时间
    uint16_t dsg_empty_time; //6151 电池放空时间
    uint16_t total_cell_num;//6152
    uint16_t total_ntc_num;//6153
    uint16_t reboot_rsp;
    cell_info cell_volt[MAX_CELL_NUM];
	uint8_t ntc_Group[6];
    chg_enable_t chg_ctrl;         
    uint16_t bat_num; // 电池总数
    uint16_t chg_max_volt; //保留2位小数 200.00v;X100V
    uint16_t chg_max_curr; //6160 保留1位小数 20.0A;X10A
    uint16_t dsg_max_curr; //6161保留1位小数 20.0A
    uint16_t total_volt; // 0.01V
    uint16_t total_curr; // 0.1A
    to_state_t state;
    IOT_FACTORY_t ota_state; 
    uint8_t type_source; // 型号来源：TYPE_SOURCE_NONE/0X10/VERSION/0X56
    uint16_t type_source_timeout; // 型号来源超时计数，递减到0时清除type_source
    uint32_t standard_capacity;
    uint8_t pack_type_num;
    uint16_t pack_volt;
    uint8_t is_ver_used_canid52;
}bms_data_t;

typedef union {
    uint16_t all;
    struct{
        uint8_t is_busy : 1; // 0-空闲；1-繁忙
        uint8_t is_boot : 1; // 0-正常；1-开机
        uint8_t is_mix_err : 1; // bit2 1:混包 0:无混包 B300系列和B230混包置1
        uint8_t is_b300_boot : 1; // bit3 B300或B300S在boot
    }bit;
}bat_all_state_t;

typedef struct {
    uint16_t bat_index; // 用于兼容电池序号切包
    bat_all_state_t state; // 电池组状态
    bms_data_t bms_data[MAX_BMS_NUM]; // Bat1-Batx
    bms_data_t all_data; // 电池总包
//    bms_data_t show_data; // 手机用于读取
//    bms_data_t ann_data; // MQTT连接后台后，用于上报的数据 
    uint8_t bat_data_to_uart_flag;//电池包数据uart转给dcdc模块
}bat_data_t;

#pragma pack ()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

