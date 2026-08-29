#include "iot_timer.h"
#include "parameter.h"
#include "comm_define.h"
#include "can_protocol.h"
#include "ble_client_central.h"
#include "rs485_transmit.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "[IoT_Timer]"

#define MAIN_TIMER_INTERVAL         5   //系统定时器自动重载周期 - 不可随意更改
#define MAIN_TIMER_PERIOD           (10/MAIN_TIMER_INTERVAL)    // 自定义基础定时器周期(10ms定时器)

static TimerHandle_t main_timer = NULL;
static esp_timer_handle_t s_main_esp_timer = NULL;

// 定时器层级关系配置
static const struct {
    volatile uint8_t *counter;
    uint16_t threshold;
    volatile uint8_t *next_counter;
} timer_cascade_config[] = {
    {&reals.Timer_10msCount, 5, &reals.Timer_50msCount},
    {&reals.Timer_50msCount, 2, &reals.Timer_100msCount},
    {&reals.Timer_100msCount, 5, &reals.Timer_500msCount},
    {&reals.Timer_500msCount, 2, &reals.Timer_1sCount},
    {&reals.Timer_1sCount, 60, &reals.Timer_1MinuteCount},
    {&reals.Timer_1MinuteCount, 60, NULL}
};

static const size_t timer_cascade_config_len = sizeof(timer_cascade_config) / sizeof(timer_cascade_config[0]);

/**
 * @brief 定时器回调函数, 按照定时器周期，为自定义软件定时器进行置位
 * @note 定时器内不做耗时操作，否则可能导致定时器不准, 影响系统性能.
 */
static void app_main_timer_cb(TimerHandle_t pxTimer)
{
    static uint8_t scnt = 0;

#ifdef CONFIG_UART_PORT_ENABLE
    app_uart1_rx_task();
#endif

#ifdef CONFIG_CAN_PORT_ENABLE
    esp_canbus_recv_task(NULL);
#endif

#ifdef CONFIG_RS485_PORT_ENABLE
    rs485_evt_porcess();
#endif

    if (++scnt >= MAIN_TIMER_PERIOD)
    {
        scnt = 0;
        
#ifdef BLE_CLIENT_ENABLE
        ble_client_Rx_buf_copy(0);
#endif

#ifdef CONFIG_CAN_PORT_ENABLE
        Can_Beta_Tx_Block_Timeout_Check(10); // can 总线发送超时检查
#endif

        // 10ms timer
        reals.Timer_10msCount++;

        // 处理所有定时器级联
        for (size_t i = 0; i < timer_cascade_config_len; i++) {
            if (*timer_cascade_config[i].counter >= timer_cascade_config[i].threshold) {
                *timer_cascade_config[i].counter = 0;
                if (timer_cascade_config[i].next_counter) {
                    (*timer_cascade_config[i].next_counter)++;
                }
            } else {
                break; // 当前级别未溢出，退出循环
            }
        }
    }
}

static void esp_main_timer_cb(void *arg)
{
    /* 直接调用原回调，传入 NULL（原回调一般不依赖 xTimer 参数） */
    app_main_timer_cb(NULL);
}

void iot_timer_init(void)
{
#if 0
    /**
     * @brief 创建定时器, 按照定时器重载周期(5ms)，置位自定义软件定时器。
     * @note ((MAIN_TIMER_INTERVAL) * configTICK_RATE_HZ ) 必须要 >= 1000, 否则会创建失败
     */
    main_timer = xTimerCreate("main timer", pdMS_TO_TICKS(MAIN_TIMER_INTERVAL), pdTRUE, NULL, app_main_timer_cb); 
    if (main_timer == NULL)
    {
        ESP_LOGE(TAG, "main timer create failed");
        return ;
    }
    xTimerStart(main_timer, pdMS_TO_TICKS(10));
#else
    if (s_main_esp_timer != NULL) {
        return;
    }

    const esp_timer_create_args_t cfg = {
        .callback = esp_main_timer_cb,
        .name = "main_timer"
    };

    esp_err_t err = esp_timer_create(&cfg, &s_main_esp_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %d", err);
        s_main_esp_timer = NULL;
        return;
    }

    /* 周期以微秒为单位 */
    err = esp_timer_start_periodic(s_main_esp_timer, (uint64_t)MAIN_TIMER_INTERVAL * 1000ULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_start_periodic failed: %d", err);
        esp_timer_delete(s_main_esp_timer);
        s_main_esp_timer = NULL;
        return;
    }
#endif
}

/*------------------------------------------------------------------------------
 Function: iot_main_timer_handle_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取定时器标签
  * @param[in]  void  
  * @param[out] None
  * @return     esp_timer_handle_t
  */
esp_timer_handle_t iot_main_timer_handle_get(void) {
    return s_main_esp_timer;
}

