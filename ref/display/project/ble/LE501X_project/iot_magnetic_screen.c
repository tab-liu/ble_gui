#define LOG_TAG "IOT_MAGNETIC_SCREEN"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "iot_aes.h"
#include "iot_ble.h"
#include "iot_uart.h"
#include "iot_service.h"
#include "iot_crc16.h"
#include "iot_gpio.h"
#include "tinyfs.h"
#include "sleep.h"
#include "sys_stat.h"
#include "cpu.h"
#include "iot_gpio.h"
#include "ls_hal_iwdg.h"
#include "modem_rf_le501x.h"
#include "iot_ota.h"
#include "iot_timer.h"
#include "iot_magnetic_screen.h"
#include "iot_adc_battery.h"
#include "iot_iic.h"
#include "iot_service.h"
#include "iot_software_timer.h"
#include "Lcd.h"
#include "Lcd_Hw_cn91c4.h"
#include "rtc_aip8563.h"
#include "sensor_cht8325.h"

#define TEST_SCREEN 0 // 1: 测试屏幕 0: 正常屏幕
#define SHOW_ADV_DATA 0 // 1: 显示广播数据 0: 不显示

// 摄氏度转换为华氏度
#define CONVERT_TO_FAHRENHEIT(x) ((x) * 9 / 5 + 32)
// 华氏度转换为摄氏度
#define CONVERT_TO_CELSIUS(x) (((x) - 32) * 5 / 9)

static bool going_to_sleep = false;

static iot_timer_id_t magnetic_screen_display_timer_id = 0xFF;
static iot_timer_id_t magnetic_screen_control_timer_id = 0xFF;

static magnetic_screen_data_t magnetic_screen_data;

static uint8_t magnetic_screen_display_timeout_cfg = DISPLAY_TIMEOUT_DFT;

static uint32_t display_time_arr[] = {
    [0] = 30 * 1000,
    [DISPLAY_TIMEOUT_15S] = 15 * 1000,
    [DISPLAY_TIMEOUT_30S] = 30 * 1000,
    [DISPLAY_TIMEOUT_60S] = 60 * 1000,
    [DISPLAY_TIMEOUT_300S] = 300 * 1000,
    [DISPLAY_TIMEOUT_INF] = 0x0FFFFFFF,
    [DISPLAY_TIMEOUT_PRODUCTION] = 30 * 60 * 1000,
};

ble_connect_status_t get_app_ble_connect_status() {
    return magnetic_screen_data.ble_app_connect_status;
}

uint32_t magnetic_screen_get_display_time(uint16_t display_timeout_idx) {
    if (display_timeout_idx > DISPLAY_TIMEOUT_PRODUCTION) { // 开机时可能的脏数据
        LOG_D("display_timeout_idx: %d, set to 0", display_timeout_idx);
        magnetic_screen_display_timeout_cfg = DISPLAY_TIMEOUT_DFT;
        display_timeout_idx = magnetic_screen_display_timeout_cfg;
    }
    return display_time_arr[display_timeout_idx];
}

// ******************************* 以下部分设置显示内容 *********************************

/// 数字显示模块部分：

// 通用的数字显示函数，自动处理前置0的隐藏
void display_number_no_leading_zeros(uint16_t number,
                                     LCD_NUMBER_ID_E start_num_id,
                                     uint8_t max_digits) {
    uint8_t digits[4] = {0}; // 最多支持4位数
    uint16_t temp = number;

    // 提取各位数字（从低位到高位存储）
    for (int i = 0; i < max_digits; i++) {
        digits[i] = temp % 10;
        temp /= 10;
    }

    // 从高位开始找到第一个非0位
    int first_non_zero = 0;
    for (int i = max_digits - 1; i >= 0; i--) {
        if (digits[i] != 0) {
            first_non_zero = i;
            break;
        }
    }

    // 显示数字：start_num_id是从高位开始的
    for (int i = max_digits - 1; i >= 0; i--) {
        LCD_NUMBER_ID_E current_num_id = start_num_id + (max_digits - 1 - i);

        if (i <= first_non_zero) {
            // 显示有效位（从first_non_zero位开始到个位都显示）
            CDD_Lcd_SetNumberMode(current_num_id, digits[i], E_ICO_MODE_OPEN);
        } else {
            // 隐藏前置0（比first_non_zero更高的位）
            CDD_Lcd_SetNumberMode(current_num_id, 0, E_ICO_MODE_CLOSE);
        }
    }
}

// 简化的4位数显示函数
void display_4digit_number(uint16_t number, LCD_NUMBER_ID_E start_num_id) {
    display_number_no_leading_zeros(number, start_num_id, 4);
}

void display_3digit_number(uint16_t number, LCD_NUMBER_ID_E start_num_id) {
    display_number_no_leading_zeros(number, start_num_id, 3);
}

void update_offline_time() {
    if (magnetic_screen_data.ac_offline_cnt == 0) {
        CDD_Lcd_SetSigIcoMode(ICON_AC_OFFLINE_CNT, E_ICO_MODE_CLOSE); // 隐藏离线次数K单位
        CDD_Lcd_SetNumberMode(E_NUM_1, 0, E_ICO_MODE_CLOSE); // 隐藏十位
        CDD_Lcd_SetNumberMode(E_NUM_2, 0, E_ICO_MODE_CLOSE); // 隐藏个位
    } else if (magnetic_screen_data.ac_offline_cnt <= 99) {
        CDD_Lcd_SetSigIcoMode(ICON_AC_OFFLINE_CNT, E_ICO_MODE_CLOSE); // 隐藏离线次数K单位
        if (magnetic_screen_data.ac_offline_cnt < 10) {
            CDD_Lcd_SetNumberMode(E_NUM_1, 0, E_ICO_MODE_CLOSE); // 隐藏十位
        } else {
            CDD_Lcd_SetNumberMode(
                E_NUM_1, magnetic_screen_data.ac_offline_cnt / 10, E_ICO_MODE_OPEN); // 显示十位
        }
        CDD_Lcd_SetNumberMode(
            E_NUM_2, magnetic_screen_data.ac_offline_cnt % 10, E_ICO_MODE_OPEN); // 显示个位
    } else { // 溢出, 大于100次，显示"99+"
        CDD_Lcd_SetSigIcoMode(ICON_AC_OFFLINE_CNT, E_ICO_MODE_OPEN); // 显示离线次数K单位
        CDD_Lcd_SetNumberMode(E_NUM_1, 9, E_ICO_MODE_OPEN); // 显示十位
        CDD_Lcd_SetNumberMode(E_NUM_2, 9, E_ICO_MODE_OPEN); // 显示个位
    }
}

void update_time() {
    if (magnetic_screen_data.hour >= 12) {
        CDD_Lcd_SetSigIcoMode(ICON_TIME_AM, E_ICO_MODE_CLOSE); // 隐藏AM
        CDD_Lcd_SetSigIcoMode(ICON_TIME_PM, E_ICO_MODE_OPEN); // 显示PM
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_TIME_AM, E_ICO_MODE_OPEN); // 显示AM
        CDD_Lcd_SetSigIcoMode(ICON_TIME_PM, E_ICO_MODE_CLOSE); // 隐藏PM
    }

    CDD_Lcd_SetSigIcoMode(ICON_TIME_COLON, E_ICO_MODE_OPEN); // 时间冒号

    // 12小时制转换
    uint8_t hour_12 = magnetic_screen_data.hour % 12;
    if (hour_12 == 0)
        hour_12 = 12; // 0点显示为12点

    // 显示小时（可以隐藏前置0）
    uint8_t hour_tens = hour_12 / 10;
    uint8_t hour_units = hour_12 % 10;

    if (hour_tens == 0) {
        CDD_Lcd_SetNumberMode(E_NUM_3, 0, E_ICO_MODE_CLOSE); // 隐藏小时十位前置0
    } else {
        CDD_Lcd_SetNumberMode(E_NUM_3, hour_tens, E_ICO_MODE_OPEN); // 显示小时十位
    }
    CDD_Lcd_SetNumberMode(E_NUM_4, hour_units, E_ICO_MODE_OPEN); // 显示小时个位

    // 分钟总是显示2位（包括前置0）
    CDD_Lcd_SetNumberMode(E_NUM_5, magnetic_screen_data.minute / 10, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_6, magnetic_screen_data.minute % 10, E_ICO_MODE_OPEN);
}

void update_temperature() {
    CDD_Lcd_SetSigIcoMode(ICON_TEMP, E_ICO_MODE_OPEN); // 显示温度图标
    CDD_Lcd_SetSigIcoMode(ICON_TEMP_POINT, E_ICO_MODE_OPEN); // 显示小数点
    CDD_Lcd_SetSigIcoMode(ICON_TEMP_C_F_4, E_ICO_MODE_OPEN); // 温度符号 T8 共用部分

    if (magnetic_screen_para.temp_unit == TEMPERATURE_UNIT_C) { // 摄氏度 C 符号
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_C_F_1, E_ICO_MODE_OPEN); // 温度符号 T11
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_C_F_2, E_ICO_MODE_OPEN); // 温度符号 T10
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_C_F_3, E_ICO_MODE_CLOSE); // 温度符号 T9
    } else { // 华氏度 F 符号
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_C_F_1, E_ICO_MODE_CLOSE); // 温度符号 T11
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_C_F_2, E_ICO_MODE_OPEN); // 温度符号 T10
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_C_F_3, E_ICO_MODE_OPEN); // 温度符号 T9
    }

    // 根据温度单位选择显示的温度值
    float display_temp = (magnetic_screen_para.temp_unit == TEMPERATURE_UNIT_C)
                             ? magnetic_screen_data.temperature_c
                             : magnetic_screen_data.temperature_f;

    // 处理负数：显示负号并取绝对值
    if (display_temp < 0) {
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_NEG, E_ICO_MODE_OPEN); // 显示负号
        display_temp = -display_temp; // 取绝对值
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_NEG, E_ICO_MODE_CLOSE); // 隐藏负号
    }

    // 转换为整数（保留一位小数，所以乘以10）
    int temp_int = (int)(display_temp * 10 + 0.5); // 四舍五入

    // 分别提取各位数字
    int hundreds = temp_int / 1000; // 百位（对于三位数温度）
    int tens = (temp_int / 100) % 10; // 十位
    int units = (temp_int / 10) % 10; // 个位
    int decimal = temp_int % 10; // 小数位

    // 显示温度数字（根据实际需要决定是否显示百位）
    if (hundreds == 1) {
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_100, E_ICO_MODE_OPEN);
        CDD_Lcd_SetNumberMode(E_NUM_7, tens, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_TEMP_100, E_ICO_MODE_CLOSE);
        if (tens == 0) {
            CDD_Lcd_SetNumberMode(E_NUM_7, 0, E_ICO_MODE_CLOSE);
        } else {
            CDD_Lcd_SetNumberMode(E_NUM_7, tens, E_ICO_MODE_OPEN);
        }
    }
    CDD_Lcd_SetNumberMode(E_NUM_8, units, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_9, decimal, E_ICO_MODE_OPEN);
}

void update_humidity() {
    CDD_Lcd_SetSigIcoMode(ICON_HUMI, E_ICO_MODE_OPEN); // 显示湿度图标
    CDD_Lcd_SetSigIcoMode(ICON_HUMI_PER, E_ICO_MODE_OPEN); // 显示湿度百分号

    uint8_t humidity_int = (uint8_t)(magnetic_screen_data.humidity + 0.5);
    if (humidity_int >= 100) {
        humidity_int = 99;
    }

    // 直接处理2位数显示
    uint8_t tens = humidity_int / 10;
    uint8_t units = humidity_int % 10;

    if (tens == 0) {
        CDD_Lcd_SetNumberMode(E_NUM_10, 0, E_ICO_MODE_CLOSE); // 隐藏十位前置0
    } else {
        CDD_Lcd_SetNumberMode(E_NUM_10, tens, E_ICO_MODE_OPEN); // 显示十位
    }
    CDD_Lcd_SetNumberMode(E_NUM_11, units, E_ICO_MODE_OPEN); // 显示个位
}

void update_save_money() {
    if (magnetic_screen_data.money == 0) { // 0 不显示图标，隐藏图标
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY, E_ICO_MODE_CLOSE); // 隐藏省钱参数图标
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_POINT, E_ICO_MODE_CLOSE); // 隐藏小数点
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_K, E_ICO_MODE_CLOSE); // 隐藏K单位
        CDD_Lcd_SetNumberMode(E_NUM_12, 0, E_ICO_MODE_CLOSE); // 隐藏十位
        CDD_Lcd_SetNumberMode(E_NUM_13, 0, E_ICO_MODE_CLOSE); // 隐藏个位
        CDD_Lcd_SetNumberMode(E_NUM_14, 0, E_ICO_MODE_CLOSE); // 隐藏小数位
    } else if (magnetic_screen_data.money <= 999) {
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_POINT, E_ICO_MODE_CLOSE); // 隐藏小数点
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_K, E_ICO_MODE_CLOSE); // 隐藏K单位

        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY, E_ICO_MODE_OPEN); // 显示省钱参数图标

        display_3digit_number(magnetic_screen_data.money, E_NUM_12);
    } else if (magnetic_screen_data.money <= 99949) {
        // 溢出, 大于999，显示"x.y k", 以k为单位，保留一位小数
        uint32_t money_int = (magnetic_screen_data.money + 50) / 100;
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY, E_ICO_MODE_OPEN); // 显示省钱参数图标
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_POINT, E_ICO_MODE_OPEN); // 显示小数点
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_K, E_ICO_MODE_OPEN); // 显示K单位

        if (money_int < 100) {
            CDD_Lcd_SetNumberMode(E_NUM_12, 0, E_ICO_MODE_CLOSE); // 隐藏十位
        } else {
            CDD_Lcd_SetNumberMode(E_NUM_12, money_int / 100, E_ICO_MODE_OPEN); // 显示十位
        }
        CDD_Lcd_SetNumberMode(E_NUM_13, money_int / 10 % 10, E_ICO_MODE_OPEN); // 显示个位
        CDD_Lcd_SetNumberMode(E_NUM_14, money_int % 10, E_ICO_MODE_OPEN); // 显示小数位
    } else { // 溢出, 大于99949，显示"999.9 k"
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY, E_ICO_MODE_OPEN); // 显示省钱参数图标
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_POINT, E_ICO_MODE_OPEN); // 显示小数点
        CDD_Lcd_SetSigIcoMode(ICON_SAVE_MONEY_K, E_ICO_MODE_OPEN); // 显示K单位
        CDD_Lcd_SetNumberMode(E_NUM_12, 9, E_ICO_MODE_OPEN); // 显示十位
        CDD_Lcd_SetNumberMode(E_NUM_13, 9, E_ICO_MODE_OPEN); // 显示个位
        CDD_Lcd_SetNumberMode(E_NUM_14, 9, E_ICO_MODE_OPEN); // 显示小数位
    }
}

void update_soc() {
    CDD_Lcd_SetSigIcoMode(ICON_SOC_PER, E_ICO_MODE_OPEN); // 显示SOC百分比符号
    CDD_Lcd_SetSigIcoMode(ICON_SOC_PER_FRAME, E_ICO_MODE_OPEN); // 显示SOC外圈R0

    // 显示SOC数值（0-100%）
    uint8_t soc = (magnetic_screen_data.soc > 100) ? 100 : magnetic_screen_data.soc;

    // 如果是100%，显示百位的1
    if (soc == 100) {
        CDD_Lcd_SetSigIcoMode(ICON_SOC_NUM100, E_ICO_MODE_OPEN);
        CDD_Lcd_SetNumberMode(E_NUM_15, 0, E_ICO_MODE_OPEN);
        CDD_Lcd_SetNumberMode(E_NUM_16, 0, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_SOC_NUM100, E_ICO_MODE_CLOSE);

        // 直接处理SOC 2位数显示
        uint8_t soc_tens = soc / 10;
        uint8_t soc_units = soc % 10;

        if (soc_tens == 0) {
            CDD_Lcd_SetNumberMode(E_NUM_15, 0, E_ICO_MODE_CLOSE); // 隐藏十位
        } else {
            CDD_Lcd_SetNumberMode(E_NUM_15, soc_tens, E_ICO_MODE_OPEN); // 显示十位
        }
        // 如果SOC小于等于5%，且**不在**充电中，则闪烁个位
        if (soc <= 5 && magnetic_screen_data.charging_status != 1) {
            CDD_Lcd_SetNumberMode(E_NUM_16, soc_units, E_ICO_MODE_FAST_BLINK); // 闪烁个位
        } else {
            CDD_Lcd_SetNumberMode(E_NUM_16, soc_units, E_ICO_MODE_OPEN); // 显示个位
        }
    }

    // 处理跑马灯
    if (magnetic_screen_data.charging_status == 1) { // 103: 0:空闲 1:充电 2：放电
        if (soc <= 90) {
            CDD_Lcd_SetSocPerMode(soc, E_LCD_PER_MODE_CHG);
        } else if (soc < 100) {
            CDD_Lcd_SetSocPerMode(soc, E_LCD_PER_MODE_HYS);
        } else if (soc == 100) {
            CDD_Lcd_SetSocPerMode(soc, E_LCD_PER_MODE_CONST);
        }
    } else {
        CDD_Lcd_SetSocPerMode(soc, E_LCD_PER_MODE_CONST);
    }
}

void update_input_power() {
    CDD_Lcd_SetSigIcoMode(ICON_INPUT_W, E_ICO_MODE_OPEN); // 显示输入功率图标
    display_4digit_number(magnetic_screen_data.input_power, E_NUM_20);
}

void update_output_power() {
    CDD_Lcd_SetSigIcoMode(ICON_OUTPUT_W, E_ICO_MODE_OPEN); // 显示输出功率图标
    display_4digit_number(magnetic_screen_data.output_power, E_NUM_24);
}

void update_chg_time() {
    CDD_Lcd_SetSigIcoMode(ICON_CHG_TIME_H, E_ICO_MODE_OPEN); // 显示充放电时间H单位

    // chg_full_time单位是分钟，使用整数运算避免浮点
    uint16_t minutes = magnetic_screen_data.chg_full_time;
    // (分钟 * 10 + 30) / 60：转换为小时并保留1位小数，999 表示 99.9h
    uint32_t chg_time_x10 = (minutes * 10 + 30) / 60;

    uint8_t digit_hundreds;
    uint8_t digit_tens;
    uint8_t digit_units;

    if (chg_time_x10 > 999) { // >99.9h：整数小时显示，最大999h，不显示小数点
        CDD_Lcd_SetSigIcoMode(ICON_CHG_TIME_POINT, E_ICO_MODE_CLOSE);

        uint32_t chg_time_hours = (minutes + 30) / 60;
        if (chg_time_hours > 999) {
            chg_time_hours = 999;
        }

        digit_hundreds = chg_time_hours / 100;
        digit_tens = (chg_time_hours / 10) % 10;
        digit_units = chg_time_hours % 10;
    } else { // <=99.9h：保留1位小数显示
        CDD_Lcd_SetSigIcoMode(ICON_CHG_TIME_POINT, E_ICO_MODE_OPEN);

        digit_hundreds = chg_time_x10 / 100;
        digit_tens = (chg_time_x10 / 10) % 10;
        digit_units = chg_time_x10 % 10;
    }

    if (digit_hundreds == 0) {
        CDD_Lcd_SetNumberMode(E_NUM_17, 0, E_ICO_MODE_CLOSE); // 隐藏百位(实际是屏幕十位)
    } else {
        CDD_Lcd_SetNumberMode(E_NUM_17, digit_hundreds, E_ICO_MODE_OPEN);
    }
    CDD_Lcd_SetNumberMode(E_NUM_18, digit_tens, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_19, digit_units, E_ICO_MODE_OPEN);
}

// T27 -- 电池图标, T33-T37 -- 电池格数
void update_adc_battery() {
    bool is_chg = is_charging();

    CDD_Lcd_SetSigIcoMode(ICON_BATTERY, E_ICO_MODE_OPEN); // 显示电池图标

    // 先关闭所有电池格
    CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV5, E_ICO_MODE_CLOSE); // T33
    CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV4, E_ICO_MODE_CLOSE); // T34
    CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV3, E_ICO_MODE_CLOSE); // T35
    CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV2, E_ICO_MODE_CLOSE); // T36
    CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV1, E_ICO_MODE_CLOSE); // T37

    // 根据SOC显示电池格数
    if (magnetic_screen_data.adc_battery_soc >= 80) { // 显示所有5格
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV1, E_ICO_MODE_OPEN); // T37
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV2, E_ICO_MODE_OPEN); // T36
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV3, E_ICO_MODE_OPEN); // T35
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV4, E_ICO_MODE_OPEN); // T34
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV5,
                              is_chg ? E_ICO_MODE_SLOW_BLINK : E_ICO_MODE_OPEN); // T33
    } else if (magnetic_screen_data.adc_battery_soc >= 60) { // 显示4格
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV1, E_ICO_MODE_OPEN); // T37
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV2, E_ICO_MODE_OPEN); // T36
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV3, E_ICO_MODE_OPEN); // T35
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV4,
                              is_chg ? E_ICO_MODE_SLOW_BLINK : E_ICO_MODE_OPEN); // T34
    } else if (magnetic_screen_data.adc_battery_soc >= 40) { // 显示3格
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV1, E_ICO_MODE_OPEN); // T37
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV2, E_ICO_MODE_OPEN); // T36
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV3,
                              is_chg ? E_ICO_MODE_SLOW_BLINK : E_ICO_MODE_OPEN); // T35
    } else if (magnetic_screen_data.adc_battery_soc >= 20) { // 显示2格
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV1, E_ICO_MODE_OPEN); // T37
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV2,
                              is_chg ? E_ICO_MODE_SLOW_BLINK : E_ICO_MODE_OPEN); // T36
    } else if (magnetic_screen_data.adc_battery_soc > 5) { // 显示1格
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV1,
                              is_chg ? E_ICO_MODE_SLOW_BLINK : E_ICO_MODE_OPEN); // T37
    } else { // 小于5%，闪烁显示低电量, 充电中慢闪，非充电中快闪
        CDD_Lcd_SetSigIcoMode(ICON_BATTERY_LV1,
                              is_chg ? E_ICO_MODE_SLOW_BLINK
                                     : E_ICO_MODE_FAST_BLINK); // T37 闪烁提示低电量
    }
}

/// 图标显示模块部分：

// T0 -- 风暴预警
void update_icon_storm_warning() {
    if (magnetic_screen_data.storm_mode) {
        CDD_Lcd_SetSigIcoMode(ICON_STORM_WARNING, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_STORM_WARNING, E_ICO_MODE_CLOSE);
    }
}

// T1 -- 市电离线, T31 -- 市电在线
void update_icon_ac_online() {
    if (magnetic_screen_data.ac_online) { // 在线
        CDD_Lcd_SetSigIcoMode(ICON_AC_ONLINE, E_ICO_MODE_OPEN); // 显示在线图标
        CDD_Lcd_SetSigIcoMode(ICON_AC_OFFLINE, E_ICO_MODE_CLOSE); // 隐藏离线图标
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_AC_ONLINE, E_ICO_MODE_CLOSE); // 隐藏在线图标
        CDD_Lcd_SetSigIcoMode(ICON_AC_OFFLINE, E_ICO_MODE_OPEN); // 显示离线图标
    }
}

// T14 -- 警告
void update_icon_warning() {
    if (magnetic_screen_data.warning) {
        CDD_Lcd_SetSigIcoMode(ICON_WARNING, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_WARNING, E_ICO_MODE_CLOSE);
    }
}

// T15 -- 大力士
void update_icon_super_power() {
    if (magnetic_screen_data.super_power_enable) {
        CDD_Lcd_SetSigIcoMode(ICON_SUPER_POWER, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_SUPER_POWER, E_ICO_MODE_CLOSE);
    }
}

// T16 -- 快充, T17 -- 静音
void update_icon_fast_charge() { // 0：标准；1：静音；2：快充；3：超级快充；
    if (magnetic_screen_data.fast_charge == 0 || magnetic_screen_data.fast_charge == 4) {
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_CLOSE); // 隐藏快充图标
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_CLOSE); // 隐藏静音图标
    } else if (magnetic_screen_data.fast_charge == 1) { // 1: 静音
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_OPEN); // 显示静音图标
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_CLOSE); // 隐藏快充图标
    } else if (magnetic_screen_data.fast_charge == 2
               || magnetic_screen_data.fast_charge == 3) { // 2:快充 3:超级快充
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_OPEN); // 显示快充图标
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_CLOSE); // 隐藏静音图标
    }
}

// T17 -- 静音, T16 -- 快充
void update_icon_silent() {
    if (magnetic_screen_data.silent == 0 || magnetic_screen_data.silent == 4) { // 0: 标准 4: 自定义
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_CLOSE); // 隐藏快充图标
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_CLOSE); // 隐藏静音图标
    } else if (magnetic_screen_data.silent == 1) { // 1: 静音
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_OPEN); // 显示静音图标
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_CLOSE); // 隐藏快充图标
    } else if (magnetic_screen_data.silent == 2
               || magnetic_screen_data.silent == 3) { // 2:快充 3:超级快充
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_OPEN); // 显示快充图标
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_CLOSE); // 隐藏静音图标
    }
}

// T18 -- ECO
void update_icon_eco() {
    if (magnetic_screen_data.eco) { // 2017: 设置ECO;0：关闭；1：开启普通，2：开启深度
        CDD_Lcd_SetSigIcoMode(ICON_ECO_MODE, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_ECO_MODE, E_ICO_MODE_CLOSE);
    }
}

// T19 -- 输出记忆
void update_icon_output_memory() {
    if (magnetic_screen_data.save_power_output_status) {
        CDD_Lcd_SetSigIcoMode(ICON_OUTPUT_MEMORY, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_OUTPUT_MEMORY, E_ICO_MODE_CLOSE);
    }
}

// T20 -- 储能备电
void update_icon_ups_mode() {
    if (magnetic_screen_data.ups_mode && magnetic_screen_data.storm_mode
        && magnetic_screen_data.soc
               < 100) { // 只有ups模式，且SOC小于100%，且风暴预警有效时，才显示储能备电图标
        CDD_Lcd_SetSigIcoMode(ICON_UPS_MODE, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_UPS_MODE, E_ICO_MODE_CLOSE);
    }
}

static key_press_hint_t key_press_hint_active = KEY_PRESS_HINT_NONE;

// T26 -- BLE连接
void update_icon_ble_con() {
    if (key_press_hint_active == KEY_PRESS_HINT_PAIRING) {
        CDD_Lcd_SetSigIcoMode(ICON_BLE_CON, E_ICO_MODE_FAST_BLINK);
        return;
    }
    if (magnetic_screen_data.ble_app_connect_status == BLE_CONNECTED) {
        CDD_Lcd_SetSigIcoMode(ICON_BLE_CON, E_ICO_MODE_OPEN);
    } else if (magnetic_screen_data.ble_app_connect_status == BLE_CONNECTING) {
        CDD_Lcd_SetSigIcoMode(ICON_BLE_CON, E_ICO_MODE_FAST_BLINK);
    } else { // BLE_DIS_CONN
        CDD_Lcd_SetSigIcoMode(ICON_BLE_CON, E_ICO_MODE_CLOSE);
    }
}

/* 按键按住提示：仅更新显示，松手后再执行对应动作 */
void magnetic_screen_key_press_hint_show(key_press_hint_t hint) {
    key_press_hint_active = hint;
    update_icon_inv_con();
    update_icon_ble_con();
}

void magnetic_screen_key_press_hint_clear(void) {
    if (key_press_hint_active == KEY_PRESS_HINT_NONE) {
        return;
    }
    key_press_hint_active = KEY_PRESS_HINT_NONE;
    update_icon_inv_con();
    update_icon_ble_con();
}

/* 结束按键提示状态，不改当前图标（进入快速匹配时沿用长按闪烁） */
void magnetic_screen_key_press_hint_dismiss(void) {
    key_press_hint_active = KEY_PRESS_HINT_NONE;
}

// T30 -- PV接入
void update_icon_pv_in() {
    if (magnetic_screen_data.pv_online) {
        CDD_Lcd_SetSigIcoMode(ICON_PV_IN, E_ICO_MODE_OPEN);
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_PV_IN, E_ICO_MODE_CLOSE);
    }
}

// T31 --> T1

// T32 -- 逆变器连接
void update_icon_inv_con() {
    if (key_press_hint_active == KEY_PRESS_HINT_PAIRING) {
        CDD_Lcd_SetSigIcoMode(ICON_INV_CON, E_ICO_MODE_CLOSE);
        return;
    }
    // 按键提示或快速匹配：快闪优先，不受广播在线状态影响
    if (key_press_hint_active == KEY_PRESS_HINT_FAST_MATCH || is_fast_match_mode()) {
        CDD_Lcd_SetSigIcoMode(ICON_INV_CON, E_ICO_MODE_FAST_BLINK);
        return;
    }
    if (magnetic_screen_data.ble_inv_connect_status == BLE_CONNECTED) {
        CDD_Lcd_SetSigIcoMode(ICON_INV_CON, E_ICO_MODE_OPEN); // 显示逆变器连接图标
    } else {
        CDD_Lcd_SetSigIcoMode(ICON_INV_CON, E_ICO_MODE_CLOSE); // 隐藏逆变器连接图标
    }
}

// *^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ 以上部分设置显示内容 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

void magnetic_screen_set_ble_connect_status(ble_connect_status_t status) {
    magnetic_screen_data.ble_app_connect_status = status;
    update_icon_ble_con();
}

void magnetic_screen_set_inv_connect_status(ble_connect_status_t status) {
    if (is_fast_match_mode()) {
        return; // 快速匹配阶段不更新逆变器在线状态，避免非目标广播干扰显示
    }
    magnetic_screen_data.ble_inv_connect_status = status;
    update_icon_inv_con();
}

void magnetic_screen_update_temp_humi(float temperature, uint8_t humidity) {
    magnetic_screen_data.temperature_c = temperature;
    magnetic_screen_data.temperature_f = CONVERT_TO_FAHRENHEIT(temperature);
    magnetic_screen_data.humidity = humidity;
    update_temperature();
    update_humidity();
}

void magnetic_screen_update_adc_battery_soc(uint8_t adc_battery_soc) {
    magnetic_screen_data.adc_battery_soc = adc_battery_soc;
    update_adc_battery();
}

void magnetic_screen_update_time(time_t time) {
    struct tm tm;
    localtime_r(&time, &tm);
    magnetic_screen_data.year = tm.tm_year - 100; // year从2000开始，但是tm_year是从1900年开始的
    magnetic_screen_data.month = tm.tm_mon + 1;
    magnetic_screen_data.day = tm.tm_mday;
    magnetic_screen_data.hour = tm.tm_hour;
    magnetic_screen_data.minute = tm.tm_min;
    magnetic_screen_data.second = tm.tm_sec;

    // LOG_I("update time: %d-%d-%d %d:%d:%d",
    //       magnetic_screen_data.year,
    //       magnetic_screen_data.month,
    //       magnetic_screen_data.day,
    //       magnetic_screen_data.hour,
    //       magnetic_screen_data.minute,
    //       magnetic_screen_data.second);

    update_time();
}

#if defined(SHOW_ADV_DATA) && SHOW_ADV_DATA == 1
void show_magnetic_adv_80_data() {
    LOG_D("adv 80\nsoc: %d, chg_full_time: %d, pv_online: %d, ac_online: %d, input_power: %d, "
          "output_power: %d, warning: %d, charging_status: %d, storm_mode: %d",
          magnetic_screen_data.soc,
          magnetic_screen_data.chg_full_time,
          magnetic_screen_data.pv_online,
          magnetic_screen_data.ac_online,
          magnetic_screen_data.input_power,
          magnetic_screen_data.output_power,
          magnetic_screen_data.warning,
          magnetic_screen_data.charging_status,
          magnetic_screen_data.storm_mode);
}

void show_magnetic_adv_81_data() {
    LOG_D("adv 81\ntime_stamp: %d-%d-%d %d:%d:%d, ups_mode: %d, save_money: %d, ac_offline_cnt: "
          "%d, "
          "display_mode: %d, temperature_unit: %d, eco: %d, silent: %d, fast_charge: %d, "
          "super_power: %d, save_power_output_status: %d, ups_mode: %d, money: %d, ac_offline_cnt: "
          "%d, display_mode: %d, temperature_unit: %d",
          magnetic_screen_data.year,
          magnetic_screen_data.month,
          magnetic_screen_data.day,
          magnetic_screen_data.hour,
          magnetic_screen_data.minute,
          magnetic_screen_data.second,
          magnetic_screen_data.ups_mode,
          magnetic_screen_data.money,
          magnetic_screen_data.ac_offline_cnt,
          magnetic_screen_display_timeout_cfg,
          magnetic_screen_data.temperature_unit,
          magnetic_screen_data.eco,
          magnetic_screen_data.silent,
          magnetic_screen_data.fast_charge,
          magnetic_screen_data.super_power_enable,
          magnetic_screen_data.save_power_output_status,
          magnetic_screen_data.ups_mode,
          magnetic_screen_data.money,
          magnetic_screen_data.ac_offline_cnt,
          magnetic_screen_display_timeout_cfg,
          magnetic_screen_data.temperature_unit);
}
#endif

void magnetic_screen_adv_80_parse(uint8_t *data, uint8_t *nonce) {
    if (magnetic_screen_para.pairing_mode != PAIRING_MODE_SPECIFIC) { // 自动连接模式
        aes_ctr_decrypt(data, IOT_ADV_KEY_DEFAULT, nonce);
    } else { // 指定连接模式
        aes_ctr_decrypt(data, magnetic_screen_para.target_key, nonce);
    }
    // LOG_D("adv 80, nonce: %02x %02x", nonce[0], nonce[1]);
    // LOG_HEX(data, 16);

    magnetic_screen_adv_80_t *msg = (magnetic_screen_adv_80_t *)data;
    magnetic_screen_data.soc = msg->soc;
    magnetic_screen_data.chg_full_time = msg->chg_full_time;
    magnetic_screen_data.pv_online = (msg->energy_line >> 8) & 0x01; // bit8: pv图标
    magnetic_screen_data.ac_online = (msg->energy_line >> 9) & 0x01; // bit9: 电网图标
    magnetic_screen_data.input_power = msg->input_power;
    magnetic_screen_data.output_power = msg->output_power;
    magnetic_screen_data.warning = msg->status & 0x01; // bit0: 0: 无告警 1: 告警
    magnetic_screen_data.charging_status
        = (msg->status >> 2) & 0x03; // bit2:3 充放电状态 0: 空闲 1: 充电 2: 放电（无充电就是放电）
    magnetic_screen_data.storm_mode
        = ((msg->status >> 4) & 0x03) == 1; // bit4:5 风暴预警 0: 无效 1: 有效 2: 无效

    update_soc(); // soc + charging_status
    update_chg_time();
    update_icon_pv_in();
    update_icon_ac_online();
    update_input_power();
    update_output_power();
    update_icon_warning();
    update_icon_storm_warning();
#if defined(SHOW_ADV_DATA) && SHOW_ADV_DATA == 1
    show_magnetic_adv_80_data();
#endif
}

void magnetic_screen_adv_81_parse(uint8_t *data, uint8_t *nonce) {
    if (magnetic_screen_para.pairing_mode != PAIRING_MODE_SPECIFIC) { // 自动连接模式
        aes_ctr_decrypt(data, IOT_ADV_KEY_DEFAULT, nonce);
    } else { // 指定连接模式
        aes_ctr_decrypt(data, magnetic_screen_para.target_key, nonce);
    }
    magnetic_screen_adv_81_t *msg = (magnetic_screen_adv_81_t *)data;

    if (rtc_time_set_datetime(msg->time_stamp) == 0) {
        // LOG_I("RTC time set successfully: %d-%d-%d %d:%d:%d",
        //       magnetic_screen_data.year,
        //       magnetic_screen_data.month,
        //       magnetic_screen_data.day,
        //       magnetic_screen_data.hour,
        //       magnetic_screen_data.minute,
        //       magnetic_screen_data.second);
    } else {
        LOG_E("Failed to set RTC time");
    }

    magnetic_screen_data.ups_mode = msg->ups_mode == 4;
    magnetic_screen_data.money = msg->save_money;
    magnetic_screen_data.ac_offline_cnt = msg->ac_offline_cnt;
    // 显示模式
    uint8_t display_mode_tmp
        = msg->display_mode
          & 0x07; // bit0:3 屏幕休眠时间 0: dft 1: 15s 2: 30s 3: 1min 4: 5min 5: inf
    uint8_t display_mode = display_mode_tmp < 1 || display_mode_tmp > 5 ? 0 : display_mode_tmp;

    if (magnetic_screen_display_timeout_cfg != display_mode) { // 有变化
        magnetic_screen_display_timeout_cfg = display_mode;
        uint32_t display_time = magnetic_screen_get_display_time(display_mode);
        // 重置并更新显示时间
        iot_timer_stop_task(magnetic_screen_display_timer_id);
        iot_timer_set_period(magnetic_screen_display_timer_id, MS_TO_TICKS(display_time));
        iot_timer_start_task(magnetic_screen_display_timer_id);
    }
    // bit4:5 温度单位 0: dft 1: 摄氏度 2: 华氏度
    magnetic_screen_data.temperature_unit = (msg->display_mode >> 4) & 0x03;
    if (magnetic_screen_para.temp_unit != magnetic_screen_data.temperature_unit) {
        magnetic_screen_para.temp_unit = magnetic_screen_data.temperature_unit;
        save_magnetic_screen_para_to_tinyfs();
    }
    // bit0~1: AC ECO使能2014：0：关闭；1：开启普通，2：开启深度
    // bit2:3: DC ECO使能2017：0: 关闭 1: 开启普通 2: 开启深度
    uint8_t eco_ac = (msg->work_mode >> 0) & 0x03;
    uint8_t eco_dc = (msg->work_mode >> 2) & 0x03;
    magnetic_screen_data.eco = eco_dc == 1 || eco_dc == 2 || eco_ac == 1 || eco_ac == 2;
    // bit4:6 静音/快充 0: 标准 1: 静音 2: 快充 3: 超级快充
    magnetic_screen_data.silent = (msg->work_mode >> 4) & 0x07;
    // bit4:6 静音/快充 0: 标准 1: 静音 2: 快充 3: 超级快充
    magnetic_screen_data.fast_charge = (msg->work_mode >> 4) & 0x07;
    // bit7 大力士 0: 关闭 1: 开启
    magnetic_screen_data.super_power_enable = (msg->work_mode >> 7) & 0x01;
    // bit8 输出记忆 0: 关闭 1: 开启
    magnetic_screen_data.save_power_output_status = (msg->work_mode >> 8) & 0x01;

    update_icon_ups_mode();
    update_save_money();
    update_offline_time();
    update_temperature(); // 也可只在读取温度时再更新
    update_icon_eco();
    update_icon_silent();
    update_icon_fast_charge();
    update_icon_super_power();
    update_icon_output_memory();

    // * 协议新增：图标显示状态由主机发，本地不做逻辑处理，如有该字则覆盖自己的逻辑
    // * 图标亮灭状态：0-invalid； 1-亮； 2-灭
    // * bit0~1 - 快充图标
    if (msg->icons & 1) { // 开启快充图标
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_OPEN);
    } else if (msg->icons & 2) { // 关闭快充图标
        CDD_Lcd_SetSigIcoMode(ICON_FAST_CHARGE, E_ICO_MODE_CLOSE);
    }

    // * bit2~3 - 静音模式图标
    if ((msg->icons >> 2) & 1) { // 开启静音模式图标
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_OPEN);
    } else if ((msg->icons >> 2) & 2) { // 关闭静音模式图标
        CDD_Lcd_SetSigIcoMode(ICON_QUIET_MODE, E_ICO_MODE_CLOSE);
    }

    // * bit4~5 - eco图标
    if ((msg->icons >> 4) & 1) { // 开启eco图标
        CDD_Lcd_SetSigIcoMode(ICON_ECO_MODE, E_ICO_MODE_OPEN);
    } else if ((msg->icons >> 4) & 2) { // 关闭eco图标
        CDD_Lcd_SetSigIcoMode(ICON_ECO_MODE, E_ICO_MODE_CLOSE);
    }

    // * bit6~7 - 储能备电图标
    if ((msg->icons >> 6) & 1) { // 开启储能备电图标
        CDD_Lcd_SetSigIcoMode(ICON_UPS_MODE, E_ICO_MODE_OPEN);
    } else if ((msg->icons >> 6) & 2) { // 关闭储能备电图标
        CDD_Lcd_SetSigIcoMode(ICON_UPS_MODE, E_ICO_MODE_CLOSE);
    }

#if defined(SHOW_ADV_DATA) && SHOW_ADV_DATA == 1
    show_magnetic_adv_81_data();
#endif
}

void magnetic_screen_adv_85_parse(uint8_t *data, uint8_t *nonce) {
    aes_ctr_decrypt(data, IOT_ADV_KEY_DEFAULT, nonce);

    magnetic_screen_adv_85_t *msg = (magnetic_screen_adv_85_t *)data;
    LOG_I("fast match cmd: %d", msg->cmd);

    if (msg->cmd == FAST_MATCH_CMD_BIND) {
        memcpy(magnetic_screen_para.target_mac, msg->host_mac, 6);
        memcpy(magnetic_screen_para.target_key, IOT_ADV_KEY_DEFAULT, 16);
        magnetic_screen_para.pairing_mode = PAIRING_MODE_SPECIFIC;
        magnetic_screen_para.save_flag = 0xaabb;
        save_magnetic_screen_para_to_tinyfs();
        LOG_I("fast match bind, host mac:");
        LOG_HEX(magnetic_screen_para.target_mac, 6);
        set_fast_match_mode(false);
    } else if (msg->cmd == FAST_MATCH_CMD_UNBIND) {
        memset(magnetic_screen_para.target_mac, 0, 6);
        memset(magnetic_screen_para.target_key, 0, 16);
        magnetic_screen_para.pairing_mode = PAIRING_MODE_AUTOMATIC;
        magnetic_screen_para.save_flag = 0xaabb;
        save_magnetic_screen_para_to_tinyfs();
        LOG_I("fast match unbind");
        set_fast_match_mode(false);
    }
}

void magnetic_screen_display_on() {
    // CDD_Lcd_Hw_SetEN(1);

    /** 开启显示 */
    // CDD_Lcd_Hw_SetBriSta(E_LCD_BRIGHT_OPEN);
    lcd_backlight_io_on();
}

void magnetic_screen_display_off() {
    /** 关闭显示 */
    lcd_backlight_io_off();

    // CDD_Lcd_Hw_SetBriSta(E_LCD_BRIGHT_CLOSE);

    // CDD_Lcd_Hw_SetEN(0);
}

void magnetic_screen_display_timeout_cb(void *param) {
    // 超时关闭屏幕， 如果正在连接到app，或者正在连接时，则延迟关屏
    bool should_delay = false;
    if (io_read_pin(BLE_SLEEP_PIN) == 0) {
        should_delay = true;
        LOG_D("sleep pin is pressed, delay 10s to stop display");
    }
    if (magnetic_screen_data.ble_app_connect_status == BLE_CONNECTED) {
        should_delay = true;
        LOG_D("app ble is connected, delay 10s to stop display");
    }
    if (magnetic_screen_data.ble_app_connect_status == BLE_CONNECTING) {
        should_delay = true;
        LOG_D("waiting app ble connect, delay 10s to stop display");
    }
    if (is_fast_match_mode()) {
        should_delay = true;
        LOG_D("fast match mode active, delay 10s to stop display");
    }
    if (should_delay) {
        iot_timer_set_period(magnetic_screen_display_timer_id, MS_TO_TICKS(10 * 1000));
        iot_timer_start_task(magnetic_screen_display_timer_id);
        return;
    }
    deep_sleep_lp3();
}

// 显示主机超时/离线时的状态
void magnetic_screen_inv_offline(void) {
    CDD_Lcd_Clear(); // 先清空所有数据
    CDD_Lcd_SetSocPerMode(0, E_LCD_PER_MODE_CONST); // 清空SoC进度条
    update_icon_ble_con(); // 显示BLE连接图标
    update_adc_battery(); // 显示ADC电池
    update_temperature(); // 显示温度
    update_humidity(); // 显示湿度
    update_time(); // 显示时间
    // 开机页面初始化
    CDD_Lcd_SetNumberMode(E_NUM_15, 0, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_16, 0, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_20, 0, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_21, 0, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_24, 0, E_ICO_MODE_OPEN);
    CDD_Lcd_SetNumberMode(E_NUM_25, 0, E_ICO_MODE_OPEN);
    CDD_Lcd_SetSigIcoMode(ICON_SOC_PER, E_ICO_MODE_OPEN);

    return;
}

// 测试显示屏数据
#if defined(TEST_SCREEN) && TEST_SCREEN == 1
extern uint32_t get_trng_value(void);
// 测试断电次数显示
static void test_ac_offline_cnt_display() {
    static uint8_t ac_off_cnt = 0;
    magnetic_screen_data.ac_offline_cnt = ac_off_cnt++;
    update_offline_time();
    if (ac_off_cnt > 110) {
        ac_off_cnt = 0;
    }
}

// 测试时间
static void test_time_display() {
    static uint8_t hour = 0;
    static uint8_t minute = 0;
    magnetic_screen_data.hour = hour++;
    magnetic_screen_data.minute = minute++;
    update_time();

    if (hour > 23) {
        hour = 0;
    }
    if (minute > 59) {
        minute = 0;
    }
}

// 测试温度显示
static void test_temp_display() {
    static uint8_t temp_unit = 0;
    static float temperature = -120.0;
    magnetic_screen_data.temperature_c = temperature;
    magnetic_screen_data.temperature_f = CONVERT_TO_FAHRENHEIT(temperature);
    magnetic_screen_data.temperature_unit = temp_unit;

    update_temperature();

    if (temperature > -20 && temperature < 20) {
        temperature += 1.3;
    } else {
        temperature += 4.7;
    }
    if (temperature > 120.0) {
        temperature = -120.0;
        temp_unit = !temp_unit;
    }
}

// 测试湿度显示
static void test_humi_display() {
    static uint8_t humidity = 0;
    magnetic_screen_data.humidity = humidity;
    update_humidity();
    if (humidity < 10) {
        humidity += 1;
    } else {
        humidity += 11;
    }
    if (humidity > 100) {
        humidity = 0;
    }
}

// 测试省钱参数显示
static void test_save_money_display() {
    static uint32_t money = 0;
    magnetic_screen_data.money = money;
    update_save_money();

    if (money < 999) {
        money += 23;
    } else if (money < 30 * 1000) {
        money += 2345;
    } else {
        money += 23456;
    }
    if (money > 200 * 1000) {
        money = 0;
    }
}

// 测试SOC显示
static void test_soc_display() {
    static uint8_t soc = 0;
    magnetic_screen_data.soc = soc;
    update_soc();
    // 显示进度条
    CDD_Lcd_SetSocPerMode(soc, E_LCD_PER_MODE_CONST);
    soc += 1;
    if (soc > 100) {
        soc = 0;
    }
}

// 测试充电时间显示
static void test_chg_time_display() {
    static uint16_t chg_time = 0;
    magnetic_screen_data.chg_full_time = chg_time;
    update_chg_time();
    chg_time += 10;
    if (chg_time > 5000) {
        chg_time = 0;
    }
}

// 测试输入功率显示
static void test_input_power_display() {
    static uint16_t input_power = 0;
    magnetic_screen_data.input_power = input_power;
    update_input_power();
    input_power += 38;
    if (input_power > 9900) {
        input_power = 0;
    }
}

// 测试输出功率显示
static void test_output_power_display() {
    static uint16_t output_power = 0;
    magnetic_screen_data.output_power = output_power;
    update_output_power();
    output_power += 37;
    if (output_power > 9900) {
        output_power = 0;
    }
}

// 测试adc电池显示
static void test_adc_battery_display() {
    static uint8_t acc = 0; // 更新变慢些，方便观察
    static uint8_t adc_battery_soc = 0;
    magnetic_screen_data.adc_battery_soc = adc_battery_soc;
    update_adc_battery();
    acc = !acc;
    adc_battery_soc += acc;
    if (adc_battery_soc > 100) {
        adc_battery_soc = 0;
    }
}

// 测试风暴模式显示 T0
static void test_storm_mode_display() {
    magnetic_screen_data.storm_mode = get_trng_value() & 0x01;
    update_icon_storm_warning();
}

// 测试市电离线显示 T1 T31
static void test_ac_offline_display() {
    magnetic_screen_data.ac_online = get_trng_value() & 0x01;
    update_icon_ac_online();
}

// 测试警告显示 T14
static void test_warning_display() {
    magnetic_screen_data.warning = get_trng_value() & 0x01;
    update_icon_warning();
}

// 测试大力士显示 T15
static void test_super_power_display() {
    magnetic_screen_data.super_power_enable = get_trng_value() & 0x01;
    update_icon_super_power();
}

// 测试快充显示 T16 T17
static void test_fast_charge_display() {
    static uint8_t fast_charge = 0;
    magnetic_screen_data.fast_charge = fast_charge++;
    update_icon_fast_charge();
    if (fast_charge > 3) {
        fast_charge = 0;
    }
}

// 测试eco显示 T18
static void test_eco_display() {
    magnetic_screen_data.eco = get_trng_value() & 0x01;
    update_icon_eco();
}

// 测试输出记忆显示 T19
static void test_output_memory_display() {
    magnetic_screen_data.save_power_output_status = get_trng_value() & 0x01;
    update_icon_output_memory();
}

// 测试ups模式显示 T20
static void test_ups_mode_display() {
    magnetic_screen_data.ups_mode = get_trng_value() & 0x01;
    update_icon_ups_mode();
}

// 测试ble连接显示 T26
static void test_ble_con_display() {
    static uint8_t nop = 0;
    nop = !nop;
    if (nop) {
        return;
    }
    magnetic_screen_data.ble_app_connect_status = get_trng_value() & 0x11;
    if (magnetic_screen_data.ble_app_connect_status == 3) {
        magnetic_screen_data.ble_app_connect_status = 1;
    }
    update_icon_ble_con();
}

// 测试pv连接显示 T30
static void test_pv_con_display() {
    magnetic_screen_data.pv_online = get_trng_value() & 0x01;
    update_icon_pv_in();
}

// 测试逆变器连接显示 T32
static void test_inv_con_display() {
    magnetic_screen_data.ble_inv_connect_status = (get_trng_value() & 0x01) << 1;
    update_icon_inv_con();
}

// 测试显示
static void test_display() {
    // -- 数值显示
    test_ac_offline_cnt_display();
    test_time_display();
    test_temp_display();
    test_humi_display();
    test_save_money_display();
    test_soc_display();
    test_chg_time_display();
    test_input_power_display();
    test_output_power_display();
    // -- 图标显示
    test_adc_battery_display();
    test_storm_mode_display();
    test_ac_offline_display();
    test_warning_display();
    test_super_power_display();
    test_fast_charge_display();
    test_eco_display();
    test_output_memory_display();
    test_ups_mode_display();
    test_ble_con_display();
    test_pv_con_display();
    test_inv_con_display();
}

#endif // TEST_SCREEN && TEST_SCREEN == 1

void magnetic_screen_set_sleep_mode(void) {
    going_to_sleep = true;
}

void magnetic_screen_control_cb(void *param) {
    if (going_to_sleep) { // 如果准备进入睡眠，则清屏
        CDD_Lcd_Clear();
        CDD_Lcd_SetSocPerMode(0, E_LCD_PER_MODE_CONST); // 清空充电进度条
    }
    CDD_Lcd_Run();
    CDD_Lcd_Hw_ScanData();

#if defined(TEST_SCREEN) && TEST_SCREEN == 1
    static uint32_t verify_counter = 0;
    if (verify_counter++ > 100 && verify_counter % 10 == 0) {
        test_display();
    }
#endif
}

void magnetic_screen_init() {
    lcd_backlight_io_init();

    memset(&magnetic_screen_data, 0, sizeof(magnetic_screen_data));
    CDD_Lcd_Init();
    // LOG_I("LCD init done!");

    CDD_Lcd_SetBrightSta(E_BRIGHT_OPEN);
    // LOG_I("LCD bright done!");

    CDD_Lcd_SetMovie(E_LCD_MOVIE_INIT);

    uint32_t display_time = magnetic_screen_get_display_time(magnetic_screen_display_timeout_cfg);

    magnetic_screen_display_timer_id
        = iot_timer_add_task_ms(display_time, false, magnetic_screen_display_timeout_cb, NULL);
    magnetic_screen_control_timer_id
        = iot_timer_add_task_ms(10, true, magnetic_screen_control_cb, NULL);

    iot_timer_start_task(magnetic_screen_display_timer_id);
    iot_timer_start_task(magnetic_screen_control_timer_id);
}
