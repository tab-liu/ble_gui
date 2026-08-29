/**
  ******************************************************************************
  * @file      iot_temperature.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/4/20
  * @brief     IOT模块温度检测
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/4/20  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "driver/temperature_sensor.h"
#include "iot_temperature.h"
#include <esp_log.h>

#define TAG "[Temperature]"

/* =============================== 文件内全局变量 ================================ */

/* 模组内温度传感器句柄 */
static temperature_sensor_handle_t s_internal_temp_handle = NULL;

/* 模组内温度传感器初始化标志 */
static bool s_internal_temp_inited = false;

/* 最近一次读取到的模组内温度，单位：摄氏度 */
static float s_internal_temp_celsius = 0.0f;

/**
 * @brief 初始化模组内温度传感器
 *
 * 该函数仅在首次调用时安装并使能 ESP32-S3 片内温度传感器。
 * 若已经初始化成功，则直接返回成功。
 *
 * @return ESP_OK               初始化成功
 * @return ESP_ERR_INVALID_STATE 传感器安装或使能失败
 */
static esp_err_t IoT_Internal_Temperature_Init(void)
{
    esp_err_t err = ESP_OK;

    if (s_internal_temp_inited) {
        return ESP_OK;
    }

    temperature_sensor_config_t temp_cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    err = temperature_sensor_install(&temp_cfg, &s_internal_temp_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "temperature_sensor_install failed: %s", esp_err_to_name(err));
        s_internal_temp_handle = NULL;
        return ESP_ERR_INVALID_STATE;
    }

    err = temperature_sensor_enable(s_internal_temp_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "temperature_sensor_enable failed: %s", esp_err_to_name(err));
        temperature_sensor_uninstall(s_internal_temp_handle);
        s_internal_temp_handle = NULL;
        return ESP_ERR_INVALID_STATE;
    }

    s_internal_temp_inited = true;
    ESP_LOGI(TAG, "internal temperature sensor init success");

    return ESP_OK;
}

/**
 * @brief 读取一次模组内温度
 *
 * 读取前会检查温度传感器是否已初始化；若未初始化则返回失败。
 *
 * @param[out] temp_celsius 输出当前温度值，单位：摄氏度
 *
 * @return ESP_OK               读取成功
 * @return ESP_ERR_INVALID_ARG  输出指针为空
 * @return ESP_ERR_INVALID_STATE 传感器未初始化或读取失败
 */
static esp_err_t IoT_Internal_Temperature_Read(float *temp_celsius)
{
    esp_err_t err = ESP_OK;

    if (temp_celsius == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((!s_internal_temp_inited) || (s_internal_temp_handle == NULL)) {
        ESP_LOGE(TAG, "internal temperature sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    err = temperature_sensor_get_celsius(s_internal_temp_handle, temp_celsius);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "temperature_sensor_get_celsius failed: %s", esp_err_to_name(err));
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

/**
 * @brief 反初始化模组内温度传感器
 *
 * 关闭并卸载片内温度传感器，释放驱动资源。
 *
 * @return ESP_OK 处理完成
 */
static esp_err_t IoT_Internal_Temperature_Deinit(void)
{
    if (s_internal_temp_handle != NULL) {
        temperature_sensor_disable(s_internal_temp_handle);
        temperature_sensor_uninstall(s_internal_temp_handle);
        s_internal_temp_handle = NULL;
    }

    s_internal_temp_inited = false;
    s_internal_temp_celsius = 0.0f;

    return ESP_OK;
}

/**
 * @brief 周期更新模组内温度
 *
 * 该函数适合放在周期任务中调用：
 * 1. 首次调用时自动完成初始化；
 * 2. 后续调用时仅读取温度值；
 * 3. 读取成功后更新缓存并打印日志；
 * 4. 读取失败时打印错误日志，但不重复 install/uninstall。
 */
void IoT_Internal_Temperature_Update(void)
{
    float temp_celsius = 0.0f;
    esp_err_t err = ESP_OK;

    if (!s_internal_temp_inited) {
        err = IoT_Internal_Temperature_Init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "internal temperature init failed");
            return;
        }
    }

    err = IoT_Internal_Temperature_Read(&temp_celsius);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "internal temperature read failed");
        return;
    }

    s_internal_temp_celsius = temp_celsius;
    ESP_LOGD(TAG, "Chip temperature: %.2f C", s_internal_temp_celsius);
}

/**
 * @brief 获取最近一次缓存的模组内温度
 *
 * 返回值单位为 0.01 摄氏度，即：
 * - 25.34°C -> 2534
 * - -5.25°C -> -525
 *
 * 注意：
 * - 当前返回类型为 int16_t；
 * - 若换算结果超过 int16_t 表示范围，则进行饱和截断：
 *   - 高于 327.67°C 返回 32767
 *   - 低于 -327.68°C 返回 -32768
 *
 * @return int16_t 最近一次更新的温度值，单位：0.01 摄氏度
 */
int16_t IoT_Internal_Temperature_GetCache(void)
{
    float temp_celsius = s_internal_temp_celsius;
    float temp_value = temp_celsius * 100.0f;

    if (temp_value >= 32767.0f) {
        return 32767;
    }

    if (temp_value <= -32768.0f) {
        return -32768;
    }

    if (temp_value >= 0.0f) {
        return (int16_t)(temp_value + 0.5f);
    } else {
        return (int16_t)(temp_value - 0.5f);
    }
}

