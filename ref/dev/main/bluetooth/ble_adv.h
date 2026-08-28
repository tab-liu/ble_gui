#ifndef __IOT_ADV_H__
#define __IOT_ADV_H__

#include <stdint.h>

#include "host/ble_hs.h"

// 自定义广播包总长度，包括flag字段
#define BLE_ADV_TOTAL_LEN BLE_HCI_MAX_ADV_DATA_LEN
// 自定义广播包头部长度
#define BLE_ADV_HEADER_LEN  15
// Manufacturer Specific Data 有效负载数据长度
#define MFG_PAYLOAD_LEN (BLE_ADV_TOTAL_LEN-BLE_ADV_HEADER_LEN)
// 自定义厂商数据头部长度
#define BLE_MFG_HEADER_LEN 11

/* 公司向SIG申请的ID: SHENZHEN POWEROAK NEWENER CO., LTD,
https://www.bluetooth.com/specifications/assigned-numbers/ */
#define COMPANY_ID_UUID 0x0F06

#pragma pack(push, 1)

typedef struct {
    uint8_t len;    /**< type+value的长度, 固定为0x02 */
    uint8_t type;   /**< flag类型为0x01 */
    uint8_t value;  /**< flag的值 */
} ble_adv_flag_t;

typedef struct {
    uint16_t prefix;        /**< 前缀，小端，已连接:0x82F0, 未连接:0x02F0 */
    uint16_t device_id;     /**< 产品型号ID，小端 */
    uint8_t record_type;    /**< 指示data的数据格式, 从0x80开始 */
    uint16_t nonce;         /**< 随机数，小端??? */
    uint8_t key_0;          /**< 双方密钥第一个字节 key[0]，用于校验key的正确性 */
} ble_mfg_header_t;


/* LCD实时数据格式 - 0x80，快速更新 */
typedef struct {
    uint8_t soc;                /**< SOC电量百分比, byte 0 */
    uint16_t charge_time;       /**< 预计充满/放空时间(分钟), byte 1-2 */
    uint16_t energy_line;       /**< 能量线显示, byte 3-4 */
    uint16_t input_power;       /**< 输入功率(W), byte 5-6 */
                                /**< 输入功率 = PV功率 + 电网功率 */
    uint16_t output_power;      /**< 输出功率(W), byte 7-8 */
                                /**< 输出功率 = 电网功率 + DC负载功率 + AC负载功率 */
    union {
        struct {
            uint8_t alarm_status : 1;  /**< bit0: 告警状态 */
            uint8_t reserved1 : 1;     /**< bit1: 预留 */
            uint8_t charge_status : 2; /**< bit2-3: 充放电状态 */
            uint8_t disaster_status : 2;  /**< bit4-5: 风暴状态 */
            uint8_t reserved3 : 2;     /**< bit6-7: 预留 */
        };
        uint8_t status_byte;
    } status;

    uint8_t reserved[6];        /**< 预留字段, byte 10-15 */
} ble_lcd_realtime_t;


/* LCD设置显示数据格式 - 0x81，慢速更新 */
typedef struct {
    uint32_t timestamp;         /**< 时间戳, byte 0-3 */
    uint8_t inv_work_mode;      /**< 逆变工作模式2005, byte 4 */
                                /**< 00:默认 01:高级用户自定义 02:经济 03:UPS在线 04:UPS后备 */
                                /**< 05:峰谷 06:离网 07:全网模式 08:自用charge only 09:充放电 */
    uint32_t money_save;        /**< 省钱参数11101-11102, byte 5-8 */
    uint8_t power_off_count;    /**< 断电次数11000, byte 9 */

    union {
        struct {
            uint8_t screen_sleep_time : 4;  /**< bit0-3: 屏幕休眠时间2067 */
                                            /**< 0:无效 1:15s 2:30s 3:1分 4:5分 5:常亮 */
            uint8_t temp_unit : 2;          /**< bit4-5: 温度单位12205 0:invalid 1:℃ 2:℉ */
            uint8_t reserved_bits : 2;      /**< bit6-7: 预留 */
        };
        uint8_t screen_display_setting;
    } display_config;                       /**< 屏幕显示设置, byte 10 */
    
    union {
        struct {
            uint16_t ac_eco_enable : 2;     /**< bit0-1: AC ECO使能2014 0:关闭 1:开启普通 2:开启深度 */
            uint16_t dc_eco_enable : 2;     /**< bit2-3: DC ECO使能2017 0:关闭 1:开启普通 2:开启深度 */
            uint16_t charge_mode : 3;       /**< bit4-6: 充电模式2020 0:标准 1:静音 2:快充 3:超级快充 4:自定义 */
            uint16_t high_power_mode : 1;   /**< bit7: 大力士模式2021 0:关闭 1:开启 */
            uint16_t output_memory : 1;     /**< bit8: 输出记忆 0:关闭 1:开启 */
            uint16_t reserved_work : 7;     /**< bit9-15: 预留 */
        };
        uint16_t work_mode_config;
    } work_mode;                /**< 工作模式, byte 11-12 */
    
    uint8_t reserved[3];        /**< 预留, byte 13-15 */
} ble_lcd_set_show_t;

typedef union {
    uint8_t data[16];                   /**< 原始字节数组 */
    ble_lcd_realtime_t lcd_realtime;    /**< LCD实时数据 - 0x80，快速更新 */
    ble_lcd_set_show_t lcd_settings;    /**< LCD设置显示 - 0x81，慢速更新 */
} ble_mfg_data_t;

typedef struct {
    ble_adv_flag_t flags;               /**< 0x01 - Flags. */
    uint8_t mfg_len;                    /**< Manufacturer Specific Data Length */
    uint8_t mfg_type;                   /**< 0xff - Manufacturer specific data. */
    uint16_t company_id;                /**< Company ID, SIG分配 */
    ble_mfg_header_t header;            /**< 广播自定义数据头部 */
    ble_mfg_data_t data;
} ble_adv_container_t;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 更新bluetooth server广播数据
 */
void ble_adv_update(void);

/**
 * @brief 更新bluetooth server广播秘钥
 * @param flag 0: 不更新 - 1: 更新 - 2: 恢复出厂默认
 */
void ble_adv_key_update(uint8_t flag);

#ifdef __cplusplus
}
#endif

#endif
