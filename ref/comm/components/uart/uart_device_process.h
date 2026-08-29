#ifndef __UART_DEVICE_PROCESS_H__
#define __UART_DEVICE_PROCESS_H__

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 出厂标定应用程序
 * - 该程序会判断该设备是否已被标定，如果已标定则直接退出
 * - 否则进入标定程序运行状态。如果在一定时间未接收到标定上位机的数据，则退出
 * - 当标定完成后，需要手动复位设备（标定时关闭关机校验）
 * @details 串口配置 uart0 115200 8 N 1 no flowctrl
 */
void fcty_cal_app(void);

#endif /* __UART_DEVICE_PROCESS_H__ */
