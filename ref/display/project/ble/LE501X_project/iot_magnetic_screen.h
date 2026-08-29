#ifndef __MAGNETIC_SCREEN_H__
#define __MAGNETIC_SCREEN_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define TEMPERATURE_UNIT_INVALID 0
#define TEMPERATURE_UNIT_C 1
#define TEMPERATURE_UNIT_F 2
#define TEMPERATURE_UNIT_DFT TEMPERATURE_UNIT_F

#define IOT_ADV_MSG_LEN 16

typedef enum {
    BLE_DIS_CONN = 0,
    BLE_CONNECTING,
    BLE_CONNECTED,
} ble_connect_status_t;

typedef enum {
    KEY_PRESS_HINT_NONE = 0,
    KEY_PRESS_HINT_FAST_MATCH, /* 3s：快速匹配提示，闪逆变器图标 */
    KEY_PRESS_HINT_PAIRING, /* 6s：蓝牙配对提示，闪蓝牙图标 */
} key_press_hint_t;

typedef struct {
    uint8_t reg_cnt;
    uint16_t start_addr;
    void (*parser)(uint16_t start_addr, uint8_t *data, uint16_t len);
} magnetic_screen_rsp_map_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t soc;
    uint16_t chg_full_time;
    uint16_t energy_line; // 能量线  bit9: 电网图标
    uint16_t input_power; // 输入功率
    uint16_t output_power; // 输出功率
    // bit0: 0: 无告警 1: 告警
    // bit2:3 充放电状态 0: 空闲 1: 充电 2: 放电（无充电就是放电）
    // bit4:5 风暴预警 0: 无效 1: 有效 2: 无效
    uint8_t status; // 状态
    uint8_t reserve[6]; // 保留
} magnetic_screen_adv_80_t;

typedef struct {
    uint32_t time_stamp;
    uint8_t ups_mode; // 是否为4
    uint32_t save_money;
    uint8_t ac_offline_cnt;
    // bit0:3 屏幕休眠时间 0: dft 1: 15s 2: 30s 3: 1min 4: 5min 5: inf
    // bit4:5 温度单位 0: dft 1: 摄氏度 2: 华氏度
    uint8_t display_mode;
    // bit2:3 ECO 0: 关闭 1: 普通 2: 深度
    // bit4:6 静音/快充 0: 标准 1: 静音 2: 快充 3: 超级快充
    // bit7 大力士 0: 关闭 1: 开启
    // bit8 输出记忆 0: 关闭 1: 开启
    uint16_t work_mode;

    // * 协议新增：图标显示状态由主机发，本地不做逻辑处理，如有该字则覆盖自己的逻辑
    // * 图标亮灭状态：0-invalid； 1-亮； 2-灭
    // * bit0~1 - 快充图标
    // * bit2~3 - 静音模式图标
    // * bit4~5 - eco图标
    // * bit6~7 - 储能备电图标
    uint8_t icons;
    uint8_t reserve[2]; // 保留
} magnetic_screen_adv_81_t;

#define FAST_MATCH_CMD_BIND 1
#define FAST_MATCH_CMD_UNBIND 2

typedef struct {
    uint8_t cmd; // 1: 绑定 2: 解绑
    uint8_t host_mac[6]; // 主机蓝牙MAC，解绑时无效
    uint8_t reserve[9];
} magnetic_screen_adv_85_t;

#pragma pack(pop)

typedef struct {
    uint16_t input_power; // 144: pv + (-grid)
    uint16_t output_power; // 152: ac + dc + grid

    uint8_t soc; // 102: 0-100%
    uint8_t charging_status; // 103: 0:空闲 1:充电 2：放电
    uint16_t chg_full_time; // 104: 充满、放空时间 min

    bool pv_online; // 123:8 pv在线
    bool ac_online; // 123:9 市电在线

    bool warning; // 126-139: 告警 或操作

    uint8_t year; // 2001: hi
    uint8_t month; // 2001: lo
    uint8_t day; // 2002: hi
    uint8_t hour; // 2002: lo 0-23
    uint8_t minute; // 2003: hi 0-59
    uint8_t second; // 2003: lo 0-59
    bool ups_mode; //! 2005: 储能备电, 风暴预警有效时
    bool eco; // 2017: 设置ECO;0：关闭；1：开启普通，2：开启深度
    uint8_t silent; // 2020: 静音 - 0：标准；1：静音；2：快充；3：超级快充；4：自定义
    uint8_t fast_charge; // 2020: 快充 - 0：标准；1：静音；2：快充；3：超级快充；4：自定义
    bool super_power_enable; // 2021: 大力士

    bool save_power_output_status; // 2226:输出记忆 0：不记录，1：记录
    bool storm_mode; // 12163: 3:2 0b10有效 风暴预警

    uint8_t ac_offline_cnt; // 11100 : 市电离线次数
    uint32_t money; // 11101-11102: 省钱参数

    uint8_t adc_battery_soc; // local: 0-100%

    uint8_t temperature_unit; // 0: 未定义 1: 摄氏度 2: 华氏度
    uint8_t humidity; // local: 0-100%
    float temperature_c; // local: 摄氏度
    float temperature_f; // local: 华氏度

    ble_connect_status_t ble_app_connect_status; // 断开，连接中，连接成功
    ble_connect_status_t ble_inv_connect_status; // 断开，连接中，连接成功
} magnetic_screen_data_t;

ble_connect_status_t get_app_ble_connect_status();
uint32_t magnetic_screen_get_display_time(uint16_t display_timeout_idx);

void update_icon_ble_con();
void update_icon_inv_con();
void magnetic_screen_adv_80_parse(uint8_t *data, uint8_t *nonce);
void magnetic_screen_adv_81_parse(uint8_t *data, uint8_t *nonce);
void magnetic_screen_adv_85_parse(uint8_t *data, uint8_t *nonce);
void magnetic_screen_update_temp_humi(float temperature, uint8_t humidity);
void magnetic_screen_update_adc_battery_soc(uint8_t adc_battery_soc);
void magnetic_screen_update_time(time_t time);
void magnetic_screen_init();
void magnetic_screen_set_ble_connect_status(ble_connect_status_t status);
void magnetic_screen_set_inv_connect_status(ble_connect_status_t status);
void magnetic_screen_set_sleep_mode(void);
void magnetic_screen_inv_offline(void);
void magnetic_screen_key_press_hint_show(key_press_hint_t hint);
void magnetic_screen_key_press_hint_clear(void);
void magnetic_screen_key_press_hint_dismiss(void);

#endif // __MAGNETIC_SCREEN_H__
