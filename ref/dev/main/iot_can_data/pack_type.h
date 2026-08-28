#ifndef PACK_TYPE_H
#define PACK_TYPE_H

#include <stdint.h>
#include "can_type.h"

#define PACK_CAN_ADDR           0x60
#define PACK_MAX_NUM            8
#define PACK_GROUP_MAX_NUM           PACK_MAX_NUM	//一个逆变器对应的电池包群组的包数量
#define PACK_SOFT_NUM           10          //电池包软件版本号数量
#define PACK_TYPE_MAX_NUM       10

#define PACK_ANNOUNCE1          0x9B
#define PACK_ANNOUNCE2          0x9C
#define PACK_ANNOUNCE3          0x9D
#define PACK_ANNOUNCE4          0x9E
#define PACK_ANNOUNCE5          0x9F
#define PACK_ANNOUNCE6          0xA0
#define PACK_ANNOUNCE7          0x65//2023.5新增 CAN ID单帧
#define PACK_ANNOUNCE8          0x66
#define PACK_ANNOUNCE9          0x67

enum {
    PACK_TYPE_ANNOUNCE_50H = 0x50,
    PACK_TYPE_BASE_51H = 0x51,
    PACK_TYPE_EXTEND_52H = 0x52,
    PACK_TYPE_NUKONW_53H = 0x53,
    PACK_TYPE_ABOUT_54H = 0x54,
    PACK_TYPE_CONFIG_55H = 0x55,
    PACK_TYPE_DEBUG1_56H = 0x56,
    PACK_TYPE_BMUSTATE_57H = 0x57,
};

typedef enum {
    PACK_WORK_INIT =0x0,  //初始化
    PACK_WORK_STANDYBY =0x11,  //待机       
    PACK_WORK_RUN =0x22,  //运行
    PACK_WORK_POWEROFF =0x33,  //关机                     
    PACK_WORK_SELF_TEST =0x44,  //自检
}pack_work_state;

#pragma pack(1)
typedef union {
    uint8_t all;
    struct {
        uint8_t chg_enable : 1;     // 1：允许充电；0：禁止
        uint8_t dsg_enable : 1;     // 1：允许放电；0：禁止
        uint8_t chg_now : 1;        // 1：紧急补电
        uint8_t chg_protect : 1;    // 1：充电进入保护
        uint8_t dgs_protect : 1;    // 1：放电进入保护
        uint8_t fault : 1;          // 1：系统故障
		uint8_t battery_heat : 1;   // 1：电池加热
		uint8_t battery_balance : 1;// 1：电池均衡
    }bit;
}status_byte1;

typedef union {
    uint8_t all;
    struct {
        uint8_t chg_alarm : 1; // 1：充电告警；0：消除
        uint8_t dsg_alarm : 1; // 1：放电告警；0：消除
    }bit;
}status_byte2;

typedef union {
    uint8_t all;
    struct {
        uint8_t status : 4;
        uint8_t progress : 4;
    };
}aging_status_t;

/**
 * @brief 0x50
 *
 */
typedef struct {

    uint16_t total_voltage; // 0.1V  总电压
    uint16_t total_current; // 0.1A  总电流
    uint8_t soc;            // 1% 系统电流
    uint8_t avg_temp;       // 平均温度
    uint8_t work_status;    // 工作状态
    uint8_t chg_status;     // 充电状态

    uint16_t max_chg_voltage; // 最大充电电压
    uint16_t max_chg_current; // 为0时，不能充电
    uint16_t max_dsg_current; // 为0时，不能放电
    status_byte1 status1;     // 状态标记位1
    status_byte2 status2;     // 状态标记位2

    uint8_t soh;        // 0-100 1%
    uint16_t online;    // 最大并机16台，响应的位置1表示，在线 - 通讯并机，bit0~15依次表示不同bat状态;从 CAN ID 0x189dXXYY获取
    uint16_t cap_online; // 最大并机16台，响应的位置1表示，主mos输出打开 - 容量并机
    uint8_t pack_cnt;   // pack并机数量
    uint16_t soc2;       // 0-1000 0.1%,与soc是10倍关系

    uint16_t chg_full_time;  // 预计充满时间
    uint16_t dsg_empty_time; // 预计放空时间
    uint8_t is_high_volt;  // 所属电压平台
	uint16_t TotalCurrent_bias;
	uint8_t res1[1];

    uint16_t cell_min_volt;
    uint16_t cell_max_volt;
    uint32_t res2;       // 保留

	uint16_t protect_status;
	uint16_t fault;
	uint32_t res3;

	aging_status_t aging_status;
	uint16_t aging_current;
	uint8_t res4[5];

	uint8_t res5[8];

	uint8_t res6[8];

    rw_cmd_struct   rw_cmd;
}pack_announce_struct;

typedef union {
    uint16_t all;
    struct {
        uint16_t cell_cnt : 8; // BMU中的电芯数量
        uint16_t temp_cnt : 8; // BMU中的温度数量
    }bit;
} bmu_count_t;


typedef union {
    uint16_t all;
    struct {
        uint16_t chargeovertemp : 1; // bit0 充电过温保护
        uint16_t dischargeovertemp : 1; // bit1 放电过温保护
        uint16_t chargelowtemp : 1; // bit2 充电低温保护
        uint16_t dischargelowtemp : 1; // bit3 放电低温保护
        uint16_t total_overvoltage : 1; // bit4 总电池过压保护
        uint16_t total_undervoltage : 1; // bit5 总电池欠压保护
        uint16_t single_overvoltage : 1; // bit6 单体过压保护
        uint16_t single_undervoltage : 1; // bit7 单体欠压保护    
        uint16_t overcurrent1 : 1; // bit8 过流保护1
        uint16_t overcurrent2 : 1; // bit9 过流保护2
        uint16_t overcurrent3 : 1; // bit10 过流保护3
        uint16_t overcurrent4 : 1; // bit11 过流保护4
        uint16_t prechargefault : 1; // bit12 预充故障
        uint16_t shortcircuit : 1; // bit13 短路保护
        uint16_t AFE_fault : 1; // bit14 AFE故障
    }bit;
} pack_protect_state;

/**
 * @brief 0x51
 *
 */
typedef struct {
    uint16_t total_voltage; // 0.1V  单pack电压
    uint16_t total_current; // 0.1A  单pack电流
    uint8_t  soc;           // 单pack soc
    uint8_t  soh;           // 单pack soh
    uint8_t  avg_temp;      // 平均温度
    uint16_t min_cell_voltage; // 最小电芯电压
    uint16_t max_cell_voltage; // 最大电芯电压
    uint8_t  min_cell_index;    // 最小电芯位置
    uint8_t  max_cell_index;    // 最大电芯位置
    uint8_t  min_temp_value;    // 最小温度值
    uint8_t  max_temp_value;    // 最大温度值
    uint8_t  min_temp_index;    // 最小温度位置
    uint8_t  max_temp_index;    // 最大温度位置
    uint8_t  work_status;       // 工作状态
    uint8_t  chg_status;        // 充电状态
    uint8_t protect[4];         // pack 保护
    uint8_t fault[6];           // pack 故障
    uint8_t alarm[2];           // pack 告警
    uint8_t relay;              // 继电器状态：主继电器Bit0-Bit1、预充继电器Bit2-Bit3、加热继电器Bit4-Bit5.
    uint8_t  pack_cap_online;     // pack 容量并机状态
    uint16_t pack_total_cell;   // 电芯总数
    uint8_t  pack_total_ntc;    // NTC总数
    uint16_t chg_full_time;     // 预计充满时间
    uint16_t dsg_empty_time;    // 预计放空时间
	uint16_t allow_max_chg_voltage;		// 允许最大充电电压
	uint16_t allow_max_chg_current;		// 允许最大充电电流
	uint16_t allow_max_dsg_current;		// 允许最大放电电流
	pack_protect_state protect_status;			// 保护状态
	uint32_t vbus;						// 外部端口电压
	uint16_t ibus;						// 端口电流
	uint16_t stop_reason;				// 关机原因
	uint8_t min_cell_bcu_idx;
	uint8_t max_cell_bcu_idx;
	uint8_t min_temp_bcu_idx;
	uint8_t max_temp_bcu_idx;
	uint8_t bmu_nums;
	bmu_count_t bmu_cnt[30];
    rw_cmd_struct   rw_cmd;
}pack_base_struct;


/**
 * @brief 0x52
 *
 */
typedef struct {
    uint32_t capacity; // 0.001Ah 电池包容量
    uint32_t total_chg_energy; // 1Wh 电池累计充电能量
    uint32_t total_dsg_energy; // 1Wh 电池累计放电能量
    uint32_t once_chg_energy; // 1Wh 电池当前充电能量
    uint32_t once_dsg_energy; // 1Wh 电池当前放电能量
    uint16_t sir_value;     // 0.1kR 绝缘包内阻
    uint16_t inner_value; // 1mR 电池包内阻
    uint16_t cycle_count; // 循环次数
    uint32_t maxPackHistoryChgEnergy;//单次最大历史充电容量
    uint32_t maxPackHistoryDsgEnergy;//单次最大历史放电容量
    rw_cmd_struct   rw_cmd;
}pack_extend_struct;



/**
 * @brief 0x54
 *
 */
typedef struct {
    char    type_ascii[12];
    uint64_t sn_code;
    uint8_t software_total; // 软件版本总数量（相同版本只显示最低版本号）
    soft_version soft[PACK_SOFT_NUM]; // 20组软件版本信息
    rw_cmd_struct   rw_cmd;
}pack_about_struct;

/**
 * @brief 0x55
 *
 */

#define	PACK_CONFIG_STRUCT_COMMON_0x55 \
    uint8_t pack_heat_enable;   /* pv接入时是否开启加热功能 0：关闭；1：开启 */\
    uint8_t ctr_heat_enable;    /* 是否开启加热电池最佳温度功能0：关闭；1：开启 （开启此功能App需要提示可能会消耗电池电）*/\
    uint8_t unlock_failed_flags;/* 电池包因故障锁，重启不恢复，下发命令解锁电池包。普通用户应无此权限*/\
    uint8_t max_parallel_nums;/* 每套电池包的最大支持数量*/


typedef struct {
//    uint8_t pack_heat_enable;   // pv接入时是否开启加热功能 0：关闭；1：开启
//    uint8_t ctr_heat_enable;    // 是否开启加热电池最佳温度功能0：关闭；1：开启 （开启此功能App需要提示可能会消耗电池电）
//    uint8_t unlock_failed_flags;// 电池包因故障锁，重启不恢复，下发命令解锁电池包。普通用户应无此权限
//    uint8_t max_parallel_nums;// 每套电池包的最大支持数量
	PACK_CONFIG_STRUCT_COMMON_0x55

    rw_cmd_struct   rw_cmd;
}pack_config_struct;

/**
 * @brief 0x56
 *
 */
typedef union {
    uint16_t value;
    struct {
        uint16_t volt : 14;
        uint8_t flag : 2;
    };
}cell_struct;

typedef union {
    struct {
        int16_t ntc1 : 8;
        int16_t ntc2 : 8;
    };
}ntc_struct;

typedef struct {
	uint16_t cells;
	uint16_t ntc_cnt;
    uint16_t payload[384];
    rw_cmd_struct   rw_cmd;
}pack_debug1_struct;


typedef struct {
    uint8_t              devId;
    uint8_t              online;
    uint16_t             alive_time;
    pack_announce_struct pack_announce;  // pack主机的广播信息
    pack_base_struct     pack_base;      // pack单包主要信息,type=0x51
    pack_extend_struct   pack_extend;    // pack 扩展数据，type=0x52
    pack_about_struct    pack_about;     // 关于pack
    pack_config_struct   pack_config;    // pack 参数配置区
    pack_debug1_struct   pack_debug1;    // pack 调试数据,type= 0x56
}pack_node_struct;


typedef struct {
    pack_config_struct   pack_config;   // pack配置信息
}bk_pack_node_set;
#pragma pack()
#endif
