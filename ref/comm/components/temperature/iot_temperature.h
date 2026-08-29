/**
  ******************************************************************************
  * @file      iot_temperature.h
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

#pragma once

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 周期更新模组内温度
 *
 * 该函数适合放在周期任务中调用：
 * 1. 首次调用时自动完成初始化；
 * 2. 后续调用时仅读取温度值；
 * 3. 读取成功后更新缓存并打印日志；
 * 4. 读取失败时打印错误日志，但不重复 install/uninstall。
 */
void IoT_Internal_Temperature_Update(void);

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
int16_t IoT_Internal_Temperature_GetCache(void);

#ifdef __cplusplus
}
#endif

