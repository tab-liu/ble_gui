#define LOG_TAG "ADC_BATTERY"

#include <string.h>
#include <stdlib.h>
#include "ls_hal_adc.h"
#include "platform.h"
#include "ls_soc_gpio.h"
#include "ls_hal_dmac.h"
#include "reg_syscfg.h"
#include "log.h"
#include "iot_timer.h"
#include "iot_magnetic_screen.h"
#include "iot_gpio.h"

// clang-format off
static uint16_t voltage_soc_map[21] = {
    3000, /* 0% */
    3350, /* 5% */
    3410, /* 10% */
    3456, /* 15% */
    3495, /* 20% */
    3521, /* 25% */
    3543, /* 30% */
    3561, /* 35% */
    3581, /* 40% */
    3602, /* 45% */
    3626, /* 50% */
    3656, /* 55% */
    3696, /* 60% */
    3744, /* 65% */
    3794, /* 70% */
    3845, /* 75% */
    3903, /* 80% */
    3962, /* 85% */
    4020, /* 90% */
    4096, /* 95% */
    4200, /* 100% */
};
// clang-format on

static void Error_Handler(void);

static ADC_HandleTypeDef hadc;
static iot_timer_id_t adc_battery_check_timer_id = 0xFF;

// 电压滑动平均滤波器
#define VOLTAGE_FILTER_WINDOW 8
static uint32_t voltage_samples[VOLTAGE_FILTER_WINDOW] = {0};
static uint8_t sample_index = 0;
static uint8_t valid_sample_count = 0;
static uint32_t voltage_sum = 0;

// 初始化电压滤波器
static void voltage_filter_init(void) {
    sample_index = 0;
    valid_sample_count = 0;
    voltage_sum = 0;
    memset(voltage_samples, 0, sizeof(voltage_samples));
}

// 电压滤波处理：输入原始电压，输出滤波后的平均电压
static uint32_t voltage_filter_process(uint32_t raw_voltage_mv) {
    // 增量更新：sum = sum - old + new
    if (valid_sample_count >= VOLTAGE_FILTER_WINDOW) {
        // 窗口已满，减去要被替换的旧值
        voltage_sum = voltage_sum - voltage_samples[sample_index] + raw_voltage_mv;
    } else {
        // 窗口未满，直接累加
        voltage_sum += raw_voltage_mv;
        valid_sample_count++;
    }

    // 保存新值
    voltage_samples[sample_index] = raw_voltage_mv;
    sample_index = (sample_index + 1) % VOLTAGE_FILTER_WINDOW;

    // 计算并返回平均值
    return voltage_sum / valid_sample_count;
}

static void adc_init(void) {
    pinmux_adc12b_in0_init();

    hadc.Instance = LSADC;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc.Init.NbrOfConversion = 1;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.NbrOfDiscConversion = 1;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.TrigType = ADC_INJECTED_SOFTWARE_TRIGT;
    hadc.Init.AdcCkDiv = ADC_CLOCK_DIV32;
    hadc.Init.Vref = ADC_VREF_INSIDE;

    if (HAL_ADC_Init(&hadc) != HAL_OK) {
        Error_Handler();
    }

    ADC_InjectionConfTypeDef sConfigInjected = {0};

    sConfigInjected.InjectedChannel = ADC_CHANNEL_0;

    sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
    sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_15CYCLES;
    sConfigInjected.InjectedOffset = 0;
    sConfigInjected.InjectedNbrOfConversion = 1;
    sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
    sConfigInjected.AutoInjectedConv = DISABLE;

    if (HAL_ADCEx_InjectedConfigChannel(&hadc, &sConfigInjected) != HAL_OK) {
        Error_Handler();
    }
}

static void adc_deinit() {
    HAL_ADC_DeInit(&hadc);
    pinmux_adc12b_in0_deinit();
}

static void Error_Handler(void) {
    LOG_D("ADC battery: Error_Handler");
}

#define VREF_VOLTAGE 1400 // 参考电压
#define MAX_VOLTAGE 4200 // 最大电压
#define MIN_VOLTAGE 3000 // 最小电压

// 电池电压转换为soc
// 电池电压范围 3v - 4.2v, 分压比为3:1，测量范围是1v - 1.4v，对应adc值为2925 - 4095
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc) {
    uint8_t adc_soc = 0;
    uint32_t adc_value = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
    uint32_t raw_mv = (VREF_VOLTAGE * adc_value / 4095) * 3;

    if (is_charging()) { // 充电中, 测量电压应该比实际电压低70mv左右
        raw_mv -= 70;
    }

    // 电压滤波处理
    uint32_t filtered_mv = voltage_filter_process(raw_mv);

    // 根据滤波后的电压计算SOC
    for (int i = 0; i < 21; i++) {
        if (filtered_mv <= voltage_soc_map[i]) {
            adc_soc = i;
            break;
        }
    }
    adc_soc *= 5; // 5%一个等级

    // LOG_I("charge: %d || raw: %d mv, filtered: %d mv -- soc: %d%%",
    //       is_charging(),
    //       raw_mv,
    //       filtered_mv,
    //       adc_soc);
    magnetic_screen_update_adc_battery_soc(adc_soc);
}

static void adc_battery_check_timer_cb(void *param) {
    HAL_ADCEx_InjectedStart_IT(&hadc);
}

void adc_battery_check_init() {
    voltage_filter_init(); // 初始化滤波器
    adc_init();
    adc_battery_check_timer_id
        = iot_timer_add_task_ms(1000, true, adc_battery_check_timer_cb, NULL);
}

void adc_battery_check_deinit() {
    if (adc_battery_check_timer_id != 0xFF) {
        iot_timer_stop_task(adc_battery_check_timer_id);
        iot_timer_remove_task(adc_battery_check_timer_id);
        adc_battery_check_timer_id = 0xFF;
    }
    adc_deinit();
}

void adc_battery_check_start() {
    adc_battery_check_timer_cb(NULL);
    iot_timer_start_task(adc_battery_check_timer_id);
}

void adc_battery_check_stop() {
    iot_timer_stop_task(adc_battery_check_timer_id);
}
