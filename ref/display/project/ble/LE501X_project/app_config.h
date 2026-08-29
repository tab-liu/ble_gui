#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#define SDK_DEEP_SLEEP_ENABLE 1

#define SDK_MAX_PROFILE_NUM 1

#define IOT_DOG_ENABLE 1 // 看门狗使能

#define LOG_UART_TXD (PB08)
#define LOG_UART_RXD (PA09)
#define LOG_UART_BAUDRATE UART_BAUDRATE_115200

// #define SDK_USER_BUILTIN_TIMER_NUM_MAX 10 // 8

// 厂商ID
#define COMPANY_ID 0x0F06

// 产品ID
// #define PRODUCT_ID 0x000a

// 产品前缀(最高位为1表示设备ble已经连接，0表示设备ble未连接，不应该影响数据解析)
// #define PRODUCT_PREFIX 0x82F0

// 设置是否使用client角色
#define MASTER_CLIENT_ROLE 1

#if MASTER_CLIENT_ROLE == 1
#define SDK_MAX_CONN_NUM 2 // 如果添加client角色，最大连接数需要加1

// 标定设备类型
#define IOT_TYPE_DEFAULT "Display 1"
#define IOT_SN_DEFAULT 1234567890UL
#define IOT_SAFECODE_DEFAULT 0

// 广播包默认密钥
#define IOT_ADV_KEY_DEFAULT "BluettiBluetooth"

#endif

#endif
