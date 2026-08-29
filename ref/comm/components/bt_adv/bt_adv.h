/**
 ******************************************************************************
 * @file      bt_adv.h
 * @version   1.0
 * @author    zdm
 * @date      2025年8月8日
 * @brief     BLE自定义广播协议头文件
 * @details   定义了BLE广播数据结构、常量和函数接口
 *            实现基于AES-CTR加密的安全广播协议
 *            支持实时数据、设置数据、默认数据三种广播类型
 * @par       功能特性
 *            - 多类型数据广播：实时数据(0x80)、设置数据(0x81)、默认数据
 *            - AES-CTR加密：128位密钥确保数据传输安全
 *            - 动态密钥管理：支持密钥生成、更新和恢复出厂设置
 *            - 智能广播调度：根据数据类型采用不同的广播间隔策略
 *            - 连接状态感知：根据BLE连接状态调整广播行为
 * @note      公共函数仅供参考，需要根据实际需求进行修改。蓝牙广播间隔由驱动ble_gap_adv_start函数adv_params参数决定。
 *            此处为广播数据刷新间隔，单位为毫秒。
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 ******************************************************************************
 */
#pragma once

#include <stdint.h>
#include <assert.h>

#include "host/ble_hs.h"


/* ================================ 数据结构定义 ================================ */
#pragma pack(push, 1)

/** 
 * @brief 广播间隔时间定义(毫秒)
 * @details 表示状态机在周期内的各个时间点
 */
typedef enum {
    ADV_STATE_SEND_1 = 0,           // 发送数据广播1 
    ADV_STATE_SEND_2 = 1,           // 发送数据广播2 
    ADV_STATE_SEND_3 = 2,           // 发送数据广播3 
    ADV_STATE_SEND_4 = 3,           // 发送数据广播4 
    ADV_STATE_SEND_5 = 4,           // 发送数据广播5 
    ADV_STATE_SEND_6 = 5,           // 发送数据广播6 
    ADV_STATE_IDLE              // 周期内空闲或等待下一个周期
} adv_cycle_state_t;

/** 
 * @brief 状态机基本信息
 * @details 维护状态机的当前状态
 */
typedef struct {
    adv_cycle_state_t current_state;    // 当前状态
    uint8_t rotation_counter[6];          // 轮替广播的计数器
} adv_state_machine_t;

/** 
 * @brief 广播触发参数
 */
typedef struct {
    uint8_t type;
    uint32_t start_time;
    uint8_t cmd;
} ble_adv_trigger_t;

/**
 * @brief BLE广播Flag字段结构体
 * @details 标准BLE广播包中的Flag字段，用于指示设备能力和模式
 */
typedef struct {
    uint8_t len;    /**< type+value的长度, 固定为0x02 */
    uint8_t type;   /**< flag类型为0x01 */
    uint8_t value;  /**< flag的值，通常为BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP */
} ble_adv_flag_t;

typedef union {
    uint16_t all;
    struct {
        uint16_t resv : 13; // bit 0-12
        uint16_t encrypt : 2; // bit 13-14 0: 无效 1: 非加密 2: 加密 3：预留
        uint16_t connect : 1; // bit 15 0: 未连接 1: 已连接
    };
} ble_adv_prefix_t;


/**
 * @brief BLE广播自定义数据头部结构体
 * @details 定义自定义广播数据的头部格式，包含连接状态、设备ID、数据类型等信息
 */
typedef struct {
    ble_adv_prefix_t prefix;        /**< 连接状态前缀，小端格式
                                 已连接: 0x82F0
                                 未连接: 0x02F0 */
    uint16_t device_id;     /**< 产品型号ID，小端格式，来自《产品型号(ASCII码)的代号分配表格》 */
    uint8_t record_type;    /**< 数据类型标识，指示data字段的数据格式
                                 0x80: LCD实时数据
                                 0x81: LCD设置数据 */
    uint16_t nonce;         /**< AES加密随机数，小端格式，每次广播生成新值 */
    uint8_t key_0;          /**< 加密密钥第一个字节，用于接收端校验密钥正确性 */
} ble_mfg_header_t;


/**
 * @brief LCD实时数据结构体 - 记录类型0x80
 * @details 用于快速更新的实时系统状态数据
 *          数据来源：Modbus寄存器 mod_reg00100_AppPage1
 *          总长度：16字节，经过AES-CTR加密传输
 */
typedef struct {
    uint8_t soc;                /**< SOC电量百分比 (0-100), byte 0
                                     数据源: mod_reg00100_AppPage1.soc */
    uint16_t charge_time;       /**< 预计充满/放空时间(分钟), byte 1-2
                                     数据源: mod_reg00100_AppPage1.chg_full_time
                                     充电中显示预计充满时间，其他显示放空时间 */
    uint16_t energy_line;       /**< 能量线显示状态, byte 3-4
                                     数据源: mod_reg00100_AppPage1.line_event.all */
    uint16_t input_power;       /**< 系统总输入功率(W), byte 5-6
                                     计算公式: PV功率 + 电网输入功率
                                     数据源: PVAllTotalPower + GridAllTotalPower(负值) */
    uint16_t output_power;      /**< 系统总输出功率(W), byte 7-8
                                     计算公式: AC负载 + DC负载 + 电网输出
                                     数据源: ACLoadAllTotalPower + DCLoadAllTotalPower + GridAllTotalPower(正值) */
    union {
        struct {
            uint8_t alarm_status : 1;    /**< bit0: 系统告警状态
                                              0: 无告警, 1: 有告警
                                              基于fault[0-4]数组的或运算结果 */
            uint8_t reserved1 : 1;       /**< bit1: 预留位 */
            uint8_t charge_status : 2;   /**< bit2-3: 充放电状态
                                              数据源: mod_reg00100_AppPage1.chg_status */
            uint8_t disaster_status : 2; /**< bit4-5: 风暴预警状态
                                              数据源: mod_reg12000_IOT_set.thunder_ctrl.thunder_state */
            uint8_t reserved3 : 2;       /**< bit6-7: 预留位 */
        };
        uint8_t status_byte;             /**< 状态字节的整体访问方式 */
    } status;                            /**< 系统状态位集合, byte 9 */

    uint8_t reserved[6];        /**< 预留字段用于未来扩展, byte 10-15 */
} ble_lcd_realtime_t;


/**
 * @brief LCD设置显示数据结构体 - 记录类型0x81
 * @details 用于慢速更新的系统配置和设置数据
 *          数据来源：多个Modbus寄存器和系统配置
 *          总长度：16字节，经过AES-CTR加密传输
 */
typedef struct {
    uint32_t timestamp;         /**< UTC时间戳, byte 0-3
                                     本地时间 + 时区偏移 = UTC时间戳 */
    uint8_t inv_work_mode;      /**< 逆变器工作模式, byte 4
                                     0x00:默认 0x01:高级用户自定义 0x02:经济
                                     0x03:UPS在线 0x04:UPS后备 0x05:峰谷
                                     0x06:离网 0x07:全网模式 0x08:自用charge only 0x09:充放电 */
    uint32_t money_save;        /**< 省钱参数(累计金额), 默认每年，可通过modbusbeta地址12206设置，byte 5-8
                                     数据源: /api/bluiotdata/dashboard/v1/getEnergyStorageElectricCostSave */
    uint8_t power_off_count;    /**< 电网断电次数,根据电网在线标记变化统计，默认每月，可通过modbusbeta地址12206设置，byte 9 */
    
    union {
        struct {
            uint8_t screen_sleep_time : 4;  /**< bit0-3: 屏幕休眠时间设置
                                                 数据源: IotSetData.dev_info_t.ctrl_lcd_active_time
                                                 0:无效 1:15s 2:30s 3:1分 4:5分 5:常亮 */
            uint8_t temp_unit : 2;          /**< bit4-5: 温度单位设置
                                                 数据源: /api/bluiotdata/dashboard/v1/getEnergyStorageElectricCostSave
                                                 0:invalid 1:℃ 2:℉ */
            uint8_t reserved_bits : 2;      /**< bit6-7: 预留位 */
        };
        uint8_t screen_display_setting;     /**< 显示设置字节的整体访问方式 */
    } display_config;           /**< 屏幕显示配置, byte 10 */
    
    union {
        struct {
            uint16_t ac_eco_enable : 2;     /**< bit0-1: AC ECO使能状态
                                                 数据源: mod_reg02000_Inv_base_set.ctrl_ac_eco
                                                 0:关闭 1:开启普通 2:开启深度 */
            uint16_t dc_eco_enable : 2;     /**< bit2-3: DC ECO使能状态
                                                 数据源: mod_reg02000_Inv_base_set.ctrl_dc_eco
                                                 0:关闭 1:开启普通 2:开启深度 */
            uint16_t charge_mode : 3;       /**< bit4-6: 充电模式选择
                                                 数据源: mod_reg02000_Inv_base_set.ctrl_chg_mode
                                                 0:标准 1:静音 2:快充 3:超级快充 4:自定义 */
            uint16_t high_power_mode : 1;   /**< bit7: 大力士模式状态
                                                 数据源: mod_reg02000_Inv_base_set.ctrl_super_power
                                                 0:关闭 1:开启 */
            uint16_t output_memory : 1;     /**< bit8: 输出记忆功能
                                                 数据源: mod_reg02200_Inv_advance_set.ctrl_save_power_state
                                                 0:关闭 1:开启 */
            uint16_t reserved_work : 7;     /**< bit9-15: 预留位 */
        };
        uint16_t work_mode_config;          /**< 工作模式配置字的整体访问方式 */
    } work_mode;                /**< 系统工作模式配置, byte 11-12 */
    
    uint8_t reserved[3];        /**< 预留字段用于未来扩展, byte 13-15 */
} ble_lcd_set_show_t;

/**
 * @brief LCD绑定设置命令结构体 - 记录类型0x85
 * @details 用于绑定/解绑磁吸屏设备
 *          总长度：16字节，经过AES-CTR加密传输
 */
typedef union {
    uint8_t all[16];                    // 最长16字节
    struct {
        uint8_t cmd;                    // 绑定命令	(1-绑定；2-解绑；3-预留)
        uint8_t ble_mac[6];             // 待绑定设备的BLE MAC地址，小端模式（先传右侧地址）
        uint8_t reserved[9];            // 预留填0
    };
} ble_lcd_bind_cmd_t;

/**
 * @brief DCDC数据结构体
 * @details 用于慢速更新的数据
 *          数据来源：多个Modbus寄存器
 *          总长度：16字节，经过AES-CTR加密传输
 */
typedef union {
    uint8_t all[16];                    // 最长16字节
    struct {
        uint16_t alarm_code;            // 16 bits
        uint16_t battery_voltage;       // 16 bits
        uint16_t battery_current;       // 16 bits
        uint16_t yield_today;           // 16 bits
        uint16_t pv_power;              // 16 bits
        uint16_t load_current : 9;      // 9 bits
        uint64_t unused       : 39;     // 39 bits
    };
} ble_mfg_data_dcdc_0x01_t;

/**
 * @brief INV数据结构体
 * @details 用于慢速更新的数据
 *          数据来源：多个Modbus寄存器
 *          总长度：16字节，经过AES-CTR加密传输
 */
typedef union {
    uint8_t all[16];                    // 最长16字节
    struct {
        uint16_t alarm_code;            // 16 bits
        int16_t  battery_current;       // 16 bits
        uint16_t battery_voltage : 14;  // 14 bits
        uint16_t ac_port_index   : 2;   // 2 bits
        int16_t  ac_port_power;         // 16 bits
        int16_t  ac_out_power;          // 16 bits
        uint16_t pv_power;              // 16 bits
        uint16_t yield_today;           // 16 bits
        uint8_t  soc;                   // 8 bits
        uint8_t  unused;                // 8 bits
    };
} ble_mfg_data_inv_0x0B_t;

/**
 * @brief BLE广播有效负载数据联合体
 * @details 16字节的有效负载，可以是不同类型的数据结构
 */
typedef union {
    uint8_t data[16];                   /**< 原始字节数组形式访问 */
    ble_lcd_realtime_t lcd_realtime;    /**< LCD实时数据结构 - 记录类型0x80 */
    ble_lcd_set_show_t lcd_settings;    /**< LCD设置数据结构 - 记录类型0x81 */
    ble_lcd_bind_cmd_t lcd_bind_cmd;    /**< LCD绑定设置结构 - 记录类型0x85 */
	ble_mfg_data_dcdc_0x01_t dcdc_data; /**< DCDC基本数据    - 记录类型0x01 */
	ble_mfg_data_inv_0x0B_t inv_data;   /**< INV基本数据     - 记录类型0x0B */
} ble_mfg_data_t;


// 编译时强制校验结构体大小。 
_Static_assert(sizeof(ble_mfg_data_t) == 16, "ble_mfg_data_t size must be 16 bytes!");

/**
 * @brief BLE广播数据包完整容器结构体
 * @details 包含完整的31字节BLE广播数据包格式
 *          结构：Flag(3) + MFG_Len(1) + MFG_Type(1) + Company_ID(2) + Header(7) + Data(16) = 30字节
 */
typedef struct {
    ble_adv_flag_t flags;               /**< BLE标准Flag字段 - 3字节 */
    uint8_t mfg_len;                    /**< 厂商特定数据长度字段 */
    uint8_t mfg_type;                   /**< 厂商特定数据类型标识 - 0xFF */
    uint16_t company_id;                /**< 公司ID，蓝牙SIG分配 */
    ble_mfg_header_t header;            /**< 自定义数据头部 - 7字节 */
    ble_mfg_data_t data;                /**< 加密的有效负载数据 - 16字节 */
} ble_adv_container_t;

#pragma pack(pop)

/* ================================ 公共函数声明 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BLE广播LCD数据更新主函数
 * @details 根据时间间隔和优先级策略更新不同类型的广播数据
 *          - 实时数据
 *          - 设置数据 
 *          - 默认数据
 * @note 在系统休眠或BLE禁用时自动停止广播
 *       需要在主循环或定时任务中周期性调用
 */
void ble_adv_update(void);

/**
 * @brief BLE广播密钥管理函数
 * @param flag 密钥操作类型 对应modbus beta协议寄存器13600 bit4~5
 *        - 0: 不执行任何操作
 *        - 1: 生成新的随机密钥(使用MD5哈希)
 *        - 2: 恢复出厂默认密钥
 * @details 密钥管理策略：
 *          - 生成模式：使用随机数 + MD5哈希生成128位密钥
 *          - 确保新密钥第一字节与默认密钥不同
 *          - 同步更新所有相关数据结构中的密钥
 */
void ble_adv_key_update(uint8_t flag);

/**
 * @brief 更新广播触发参数，同时清零开始时间
 * @param new_type 触发类型
 * @param new_cmd 触发命令
 */
void ble_adv_trigger_update(uint8_t new_type, uint8_t new_cmd);

#ifdef __cplusplus
}
#endif
