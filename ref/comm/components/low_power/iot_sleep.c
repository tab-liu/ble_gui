/**
  ******************************************************************************
  * @file      iot_sleep.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/7/3
  * @brief     低功耗休眠
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/7/3   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/twai.h"

#include "esp_bit_defs.h"
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_wifi_types.h"
#include "esp_wifi.h"
#include "esp_mac.h"

#include "iot_sleep.h"
#include "comm_define.h"
#include "utils.h"

#include "can_transmit.h"
#include "can_protocol.h"
#include "parameter.h"
#include "modbus_tcp_slave.h"
#include "wlcc_process.h"
#include "usb_host_modem.h"
#include "iot_wifi.h"

static const char *TAG = "[iot_sleep]";

/*------------------------------------------------------------------------------*/

extern esp_timer_handle_t iot_main_timer_handle_get(void);
extern void iot_ble_start(void);
extern void iot_ble_stop(void);
extern void iot_ble_deinit(void);
extern void iot_ble_reinit(void);
extern void iot_wifi_ap_deinit(void);
extern void iot_wifi_ap_reinit(void);
extern void iot_wifi_reconnect_in_sleep(bool force);
extern void app_log_level_set(void);
extern void iot_period_task_by_sleep(void);
#ifdef CONFIG_ENABLE_MD_TCP_SLAVE
extern void modbus_tcp_stop(void);
extern void modbus_tcp_restart(uint8_t is_crypted, uint16_t port, uint8_t block);
extern bool modbus_tcp_is_running(void);
extern void modbus_tcp_start(uint8_t is_crypted, uint16_t port, uint8_t block);
#endif

/*------------------------------------------------------------------------------*/

/*函数声明*/
static uint8_t iot_sleep_twai_stop(void);
static uint8_t iot_sleep_twai_restart(void);

/*------------------------------------------------------------------------------*/

#ifdef LIGHT_SLEEP_DEBUG_LOG
#define MAX_WAKE_LOG 64

typedef struct {
    esp_sleep_wakeup_cause_t cause;
    int64_t sleep_time_us;
} wakeup_info_t;

static uint32_t light_sleep_wakeup_count = 0;
USE_EXT_RAM_BSS static wakeup_info_t wakeup_log[MAX_WAKE_LOG] = {0};
static uint32_t wakeup_log_idx = 0;
#endif

/*------------------------------------------------------------------------------*/

static uint8_t IOT_Sleep_Flag = 0;
static TaskHandle_t iot_sleep_task_handle = NULL;

/*------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 Function: iot_sleep_status_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取休眠标志
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t iot_sleep_status_get(void) {
    return IOT_Sleep_Flag;
}

#ifdef LIGHT_SLEEP_DEBUG_LOG


/*------------------------------------------------------------------------------ 
 Function: light_sleep_exit_cb 
 -----------------------------------------------------------------------------*/
/**
  * @brief      休眠退出回调
  * @param[in]  sleep_time_us  实际休眠时间
  * @param[in]  arg            用户参数
  * @return     ESP_OK
  */
static esp_err_t light_sleep_exit_cb(int64_t sleep_time_us, void *arg)
{
    light_sleep_wakeup_count++;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    wakeup_log[wakeup_log_idx].cause = cause;
    wakeup_log[wakeup_log_idx].sleep_time_us = sleep_time_us;
    wakeup_log_idx = (wakeup_log_idx + 1) % MAX_WAKE_LOG;
    // ESP_LOGI(TAG, "light_sleep_exit_cb: sleep_time_us=%lld, cause=%d", sleep_time_us, cause); // 屏蔽：避免在休眠回调中使用格式化输出导致崩溃
    return ESP_OK;
}

/*------------------------------------------------------------------------------
 Function: print_light_sleep_wakeup_log
 -----------------------------------------------------------------------------*/
/**
  * @brief      休眠日志打印
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void print_light_sleep_wakeup_log(void)
{
    if(light_sleep_wakeup_count == 0) return;
    ESP_LOGI(TAG, "+-------+----------------------+---------------------+");
    ESP_LOGI(TAG, "| Index | Wakeup Cause         | Sleep Time (us)     |");
    ESP_LOGI(TAG, "+-------+----------------------+---------------------+");
    for (uint32_t i = 0; i < MAX_WAKE_LOG; i++) {
        uint32_t idx = (wakeup_log_idx + i) % MAX_WAKE_LOG;
        if (wakeup_log[idx].sleep_time_us == 0) continue; // 跳过未写入的记录
        ESP_LOGI(TAG, "| %-5u | %-20d | %-19lld |",
                 (unsigned int)idx,
                 (int)wakeup_log[idx].cause,
                 (long long)wakeup_log[idx].sleep_time_us);
    }
    ESP_LOGI(TAG, "+-------+----------------------+---------------------+");
}

#endif

/*------------------------------------------------------------------------------
 Function: print_pm_locks
 -----------------------------------------------------------------------------*/
/**
  * @brief      打印当前所有 PM lock 的状态，确认是否有锁禁止系统进入 light sleep
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void print_pm_locks(void)
{
    /*
    Name            Type            Arg    Active  
    wifi            APB_FREQ_MAX    0      1       
    bt              APB_FREQ_MAX    0      1       
    btLS            NO_LIGHT_SLEEP  0      1       
    uart_driver     APB_FREQ_MAX    0      0       
    rtos0           CPU_FREQ_MAX    0      1   
    */
    esp_pm_dump_locks(stdout);
}

/*------------------------------------------------------------------------------
 Function: print_esp_timer
 -----------------------------------------------------------------------------*/
/**
  * @brief      打印所有定时器状态
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void print_esp_timer(void)
{

    esp_timer_dump(stdout);
}

/*------------------------------------------------------------------------------
 Function: auto_sleep_config
 -----------------------------------------------------------------------------*/
/**
  * @brief      DFS+Auto-Light-Sleep
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void auto_sleep_config(void)
{
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_PM_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_PM_MIN_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };

    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

#ifdef LIGHT_SLEEP_DEBUG_LOG 
    /* 注册休眠唤醒回调，记录日志 */
    esp_pm_sleep_cbs_register_config_t cbs_conf = {
        .enter_cb = NULL,
        .exit_cb = light_sleep_exit_cb,
        .enter_cb_user_arg = NULL,
        .exit_cb_user_arg = NULL,
        .enter_cb_prior = 5,
        .exit_cb_prior = 5,
    };
    ESP_ERROR_CHECK(esp_pm_light_sleep_register_cbs(&cbs_conf));
#endif
}

/*------------------------------------------------------------------------------
 Function: auto_sleep_deconfig
 -----------------------------------------------------------------------------*/
/**
  * @brief      取消配置
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void auto_sleep_deconfig(void)
{
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_DEFAULT_CPU_FREQ_MHZ, // 或你的最大频率
        .min_freq_mhz = CONFIG_DEFAULT_CPU_FREQ_MHZ, // 与最大频率一致，禁止降频
        .light_sleep_enable = false // 禁用light sleep
    };

    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

/*------------------------------------------------------------------------------
 Function: suspend_other_tasks
 -----------------------------------------------------------------------------*/
/**
  * @brief      挂起除当前任务外所有任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static uint8_t suspend_other_tasks(void)
{
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_status_array = (TaskStatus_t *)iot_calloc(num_tasks * sizeof(TaskStatus_t));
    if (!task_status_array) {
        ESP_LOGE(TAG, "Failed to allocate memory for task status array");
        return 0;
    }

    UBaseType_t real_num = uxTaskGetSystemState(task_status_array, num_tasks, NULL);
    TaskHandle_t current = iot_sleep_task_handle;

    for (UBaseType_t i = 0; i < real_num; i++) {
        TaskHandle_t handle = task_status_array[i].xHandle;
        if ((handle != current)
            && (strcmp(task_status_array[i].pcTaskName, "iot-cloud") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "mqtt_task") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "wifi") != 0)
#ifdef CONFIG_USB_HOST_ENABLE
            && (strcmp(task_status_array[i].pcTaskName, "usb_lib") != 0)
#endif
#ifdef CONFIG_USB_HOST_HID_ENABLE
            && (strcmp(task_status_array[i].pcTaskName, "USB HID Host") != 0)
#endif
#ifdef CONFIG_USB_4G_MODULE_ENABLE            
            && (strcmp(task_status_array[i].pcTaskName, "modem_daemon") != 0)
#endif            
#ifndef SLEEP_NIMBLE_DEINIT_ENABLE
            && (strcmp(task_status_array[i].pcTaskName, "iot_ble_task") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "nimble_host") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "btController") != 0)
#endif
            && (strcmp(task_status_array[i].pcTaskName, "esp_timer") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "IDLE0") != 0 )
            && (strcmp(task_status_array[i].pcTaskName, "IDLE1") != 0 )
            && (strcmp(task_status_array[i].pcTaskName, "Tmr Svc") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "tiT") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "sys_evt") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "ipc0") != 0)
            && (strcmp(task_status_array[i].pcTaskName, "ipc1") != 0)
           ) {
            vTaskSuspend(handle);
        }
    }

    free(task_status_array);
    return 1;
}

/*------------------------------------------------------------------------------
 Function: resume_other_tasks
 -----------------------------------------------------------------------------*/
/**
  * @brief      唤醒挂起任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static uint8_t resume_other_tasks(void)
{
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_status_array = (TaskStatus_t *)iot_calloc(num_tasks * sizeof(TaskStatus_t));
    if (!task_status_array) {
        ESP_LOGE(TAG, "Failed to allocate memory for task status array");
        return 0;
    }

    UBaseType_t real_num = uxTaskGetSystemState(task_status_array, num_tasks, NULL);

    for (UBaseType_t i = 0; i < real_num; i++) {
        TaskHandle_t handle = task_status_array[i].xHandle;
        vTaskResume(handle);
    }

    free(task_status_array);
    return 1;
}


/*------------------------------------------------------------------------------
 Function: iot_pm_lock_config
 -----------------------------------------------------------------------------*/
/**
  * @brief      配置电源锁
  * @param[in]  bool state  
  * @param[out] None
  * @return     static void
  */
static void iot_pm_lock_config(bool state)
{
    static esp_pm_lock_handle_t pm_lock = NULL;

    if ( state == true )
    {
        /*创建电源锁，暂时禁止auto-light-sleep，确保串口正常(只创建一次电源锁)*/
        if (pm_lock == NULL) {
            esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "no_ls", &pm_lock);
        }
        esp_pm_lock_acquire(pm_lock); // 禁止进入light sleep
    }
    else
    {
        /*释放该电源锁(只释放，不删除锁)*/
        if (pm_lock) {
            esp_pm_lock_release(pm_lock);
        }
    }
}

#if 0

/*LEDC配置相关*/
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_TIMER          LEDC_TIMER_0

/*------------------------------------------------------------------------------
 Function: output_1hz_5cycles_ledc_once
 -----------------------------------------------------------------------------*/
/**
  * @brief      GPIO唤醒
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void output_1hz_5cycles_ledc_once(void)
{
    // LEDC初始化
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_TIMER_1_BIT,
        .freq_hz          = 1,
        .clk_cfg          = LEDC_USE_XTAL_CLK, // 使用XTAL时钟源
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num       = WAVE_GPIO,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .duty           = 1, // 50%占空比
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    /*开启电源锁*/
    iot_pm_lock_config(true);

    // 输出5个周期（5秒）
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 停止输出，保持低电平
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);

    // 取消初始化（卸载LEDC通道和定时器）
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0); // 停止通道输出并拉低
    ledc_timer_rst(LEDC_LOW_SPEED_MODE, LEDC_TIMER); // 重置定时器

    /*关闭电源锁*/
    iot_pm_lock_config(false);
}

#else

static uint8_t wave_count = 0;
static bool wave_high = false;
static esp_timer_handle_t wave_timer = NULL;


/*------------------------------------------------------------------------------
 Function: wave_timer_callback
 -----------------------------------------------------------------------------*/
/**
  * @brief      定时器回调
  * @param[in]  void* arg  
  * @param[out] None
  * @return     static void
  */
static void wave_timer_callback(void* arg)
{
    if (wave_count < 10) { // 1Hz方波，5周期=10次（高低各5次）
        wave_high = !wave_high;
        GPIO_SET_LEVEL_HOLD(WAVE_DO_GPIO, wave_high ? 1 : 0);
        wave_count++;
    } else {
        GPIO_SET_LEVEL_HOLD(WAVE_DO_GPIO, 0); // 保持低电平
        esp_timer_stop(wave_timer);
        esp_timer_delete(wave_timer);
        wave_timer = NULL;
        wave_count = 0;
        wave_high = false;
        ESP_LOGW(TAG, "Wave_timer End!");

#if 0   // 唤醒动作完成不能视为退出休眠状态，若下级未实际唤醒可能导致异常
        reals.IOT_Status_Flag.sBit.system_sleep_flag = 0;
        can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.sleep_on = 0;
        can_node_rd.Pack[0].pack_announce.pack_set_status.status.sleep_status = 0;
#endif        
    }
}

/*------------------------------------------------------------------------------
 Function: output_1hz_5cycles_timer
 -----------------------------------------------------------------------------*/
/**
  * @brief      GPIO唤醒
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void output_1hz_5cycles_timer(void)
{
    // 创建并启动定时器
    if (wave_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &wave_timer_callback,
            .name = "wave_timer"
        };
        esp_timer_create(&timer_args, &wave_timer);
        wave_count = 0;
        wave_high = false;
        esp_timer_start_periodic(wave_timer, 500000); // 500ms切换一次电平
        ESP_LOGW(TAG, "Wave_timer Start!");
    }
}

#endif

#if 0

/*低功耗模式下每次唤醒运行时间*/
#define TIMER_RUNNING_TIME_US   (20 * 1000 * 1000)

/*低功耗模式下每次休眠持续时间*/
#define TIMER_WAKEUP_TIME_US    (20 * 1000 * 1000)

/*------------------------------------------------------------------------------
 Function: example_register_timer_wakeup
 -----------------------------------------------------------------------------*/
/**
  * @brief      配置唤醒源
  * @param[in]  void  
  * @param[out] None
  * @return     static esp_err_t
  */
static esp_err_t example_register_timer_wakeup(void)
{
    ESP_RETURN_ON_ERROR(esp_sleep_enable_timer_wakeup(TIMER_WAKEUP_TIME_US), TAG, "Configure timer as wakeup source failed");
    ESP_LOGW(TAG, "timer wakeup source is ready");
    return ESP_OK;
}

/*------------------------------------------------------------------------------
 Function: example_register_gpio_wakeup
 -----------------------------------------------------------------------------*/
/**
  * @brief      配置唤醒源
  * @param[in]  void  
  * @param[out] None
  * @return     static esp_err_t
  */
static esp_err_t example_register_gpio_wakeup(void)
{
    // /* Initialize GPIO */
    // gpio_config_t config = {
    //         .pin_bit_mask = BIT64(GPIO_WAKEUP_NUM),
    //         .mode = GPIO_MODE_INPUT,
    //         .pull_down_en = false,
    //         .pull_up_en = false,
    //         .intr_type = GPIO_INTR_DISABLE
    // };
    // ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "Initialize GPIO%d failed", GPIO_WAKEUP_NUM);

    // /* Enable wake up from GPIO */
    // ESP_RETURN_ON_ERROR(gpio_wakeup_enable(GPIO_WAKEUP_NUM, GPIO_WAKEUP_LEVEL == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL),
    //                     TAG, "Enable gpio wakeup failed");
    // ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(), TAG, "Configure gpio as wakeup source failed");

    // /* Make sure the GPIO is inactive and it won't trigger wakeup immediately */
    // example_wait_gpio_inactive();
    // ESP_LOGI(TAG, "gpio wakeup source is ready");

    return ESP_OK;
}

/*------------------------------------------------------------------------------
 Function: example_gpio_output
 -----------------------------------------------------------------------------*/
/**
  * @brief      调试（GPIO）
  * @param[in]  uint8_t link  
  * @param[out] None
  * @return     void
  */
void example_gpio_output(uint8_t link)
{
    static uint8_t first_flag = 0;
    
    if(first_flag == 0) {
        first_flag = 1;
        gpio_reset_pin(GPIO_NUM_7); 
        gpio_set_direction(GPIO_NUM_7, GPIO_MODE_OUTPUT);     // 输出高低电平
    }

    if ( link != 0 ) {
        gpio_set_level(GPIO_NUM_7, 1);
    } else {
        gpio_set_level(GPIO_NUM_7, 0);
    }
}

/*------------------------------------------------------------------------------
 Function: example_sleep_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      调试
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void example_sleep_task(void)
{
    static uint8_t first_flag = 0;
    static int64_t t_wake_up_us = 0;
    
    if(first_flag == 0) {
        first_flag = 1;
        example_register_timer_wakeup();
    }

    /* Get timestamp before entering sleep */
    int64_t t_before_us = esp_timer_get_time();

    if ((t_before_us - t_wake_up_us) <= TIMER_RUNNING_TIME_US) return;

#if 1

    ESP_LOGW(TAG, "Entering light sleep\n");

    /* Enter sleep mode */
    esp_light_sleep_start();

    /* Get timestamp after waking up from sleep */
    int64_t t_after_us = esp_timer_get_time();

    ESP_LOGW(TAG, "Returned from light sleep, t=%lld ms, slept for %lld ms\n",
    t_after_us / 1000, (t_after_us - t_before_us) / 1000);

    t_wake_up_us = t_after_us;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    // wakeup_log[wakeup_log_idx].cause = cause;
    // wakeup_log[wakeup_log_idx].sleep_time_us = sleep_time_us;
    // wakeup_log_idx = (wakeup_log_idx + 1) % MAX_WAKE_LOG;
    ESP_LOGI(TAG, "light_sleep_exit_cb: sleep_time_us=0, cause=%d", cause);

#else

    /* Enter deep sleep */
    esp_deep_sleep_start();

#endif
}

#endif

/*------------------------------------------------------------------------------
 Function: iot_sleep_config_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      上电初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_sleep_config_init(void)
{
    /*上电后默认禁止休眠及动态降频，避免外设异常*/
    auto_sleep_deconfig();

#ifdef SYSTEM_WAKEUP_BY_GPIO_ENABLE
    /*初始化唤醒DO管脚为低电平*/
    CONFIGURE_GPIO(WAVE_DO_GPIO, GPIO_MODE_OUTPUT);
    GPIO_SET_LEVEL_HOLD(WAVE_DO_GPIO, 0);
#endif

#ifdef SLEEP_MODE_POWER_ENABLE
    /*初始化休眠模式辅源使能DO管脚为高电平*/
    CONFIGURE_GPIO(SLEEP_POWER_DO_GPIO, GPIO_MODE_OUTPUT);
    GPIO_SET_LEVEL_HOLD(SLEEP_POWER_DO_GPIO, 1);
#endif

#ifdef IOT_WAKEUP_BY_GPIO_ENABLE
    /*初始化DI管脚*/
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << WAVE_DI_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
#endif

#ifndef SLEEP_NIMBLE_DEINIT_ENABLE
    /*保证外部 40 MHz 晶振在休眠时不断电*/
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_ON));
#endif
}

/*------------------------------------------------------------------------------
 Function: iot_sleep_start
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT休眠启动
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_sleep_start(void)
{
    uint8_t sleep_status = 0;
    
#ifdef SLEEP_NIMBLE_DEINIT_ENABLE
    /*停止蓝牙广播*/
    if(1 == IotSetData.dev_info_t.on_off.bit.ble_enable) iot_ble_stop();

    /*蓝牙驱动逆初始化*/
    iot_ble_deinit();
#endif

#ifdef CONFIG_ENABLE_MD_TCP_SLAVE
    /*停止MODBUS TCP*/
    modbus_tcp_stop();
#endif

#ifdef CONFIG_WLCC_ENABLE
    /*停止WLCC*/
    wlcc_stop();
#endif

#ifdef CONFIG_USB_4G_MODULE_ENABLE
    /*关闭4G任务*/
    if(1 == IotSetData.dev_info_t.on_off.bit._4g_enable) iot_4G_stop();
#endif

#ifdef CONFIG_WIFI_AP_MODE_ENABLE    
    /*关闭WIFI  AP模式*/
    if(1 == IotSetData.dev_info_t.on_off.bit.wifi_ap_enable) iot_wifi_ap_deinit();    
#endif 

#ifdef SLEEP_WIFI_MAX_MODEM_ENABLE
    /*最大调制解调器省电模式*/
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
#endif

#ifdef IOT_OTHER_TASK_SUSPEND_ENABLE
    /*等待处理完毕*/
    vTaskDelay(pdMS_TO_TICKS(1000));

    /*无关任务挂起*/
    suspend_other_tasks();
    
    /*立即停止系统定时器*/
    esp_timer_stop(iot_main_timer_handle_get());
#endif

#ifdef CONFIG_CAN_PORT_ENABLE
    /*停止TWAI驱动，释放twai锁*/
    iot_sleep_twai_stop(); // 确保所有任务停止后再卸载驱动
    
    /*等待TWAI驱动完全卸载*/
    vTaskDelay(pdMS_TO_TICKS(100));
#endif

    /*DFS+Auto-light-sleep 配置*/
    auto_sleep_config();
    ESP_LOGW(TAG, "IOT Sleep Mode Start!");
    sleep_status = 1;
    
    return sleep_status;
}

/*------------------------------------------------------------------------------
 Function: iot_sleep_end
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT休眠结束
  * @param[in]  void  
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t iot_sleep_end(void)
{
    uint8_t sleep_status = 1;

    /*DFS+Auto-light-sleep 撤销配置*/
    auto_sleep_deconfig();

#ifdef IOT_OTHER_TASK_SUSPEND_ENABLE    
    /*恢复系统定时器*/
    esp_timer_start_periodic(iot_main_timer_handle_get(), (uint64_t)5 * 1000ULL);
#endif
    
#ifdef CONFIG_WIFI_AP_MODE_ENABLE    
    /*恢复WIFI  AP模式*/
    if(1 == IotSetData.dev_info_t.on_off.bit.wifi_ap_enable) iot_wifi_ap_reinit();    
#endif

#ifdef SLEEP_WIFI_MAX_MODEM_ENABLE
    /*最小调制解调器省电模式*/
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
#endif

#ifdef CONFIG_CAN_PORT_ENABLE
    /*重启TWAI驱动*/
    iot_sleep_twai_restart();

    /*等待TWAI驱动完全重启*/
    vTaskDelay(pdMS_TO_TICKS(100));
#endif

#ifdef SLEEP_NIMBLE_DEINIT_ENABLE
    /*蓝牙驱动重新初始化*/
    iot_ble_reinit();
    
    /*恢复蓝牙广播*/
    if(1 == IotSetData.dev_info_t.on_off.bit.ble_enable) iot_ble_start();
#endif

#ifdef CONFIG_ENABLE_MD_TCP_SLAVE
    /*恢复MODBUS TCP*/
    if (true == modbus_tcp_is_running()) {
        modbus_tcp_restart(IotSetData.dev_info_t.modbus_tcp_enable.crypt_en, IotSetData.dev_info_t.modbus_tcp_port, 0);
    } else {
        modbus_tcp_start(IotSetData.dev_info_t.modbus_tcp_enable.crypt_en, IotSetData.dev_info_t.modbus_tcp_port, 0);
    }
#endif

#ifdef CONFIG_WLCC_ENABLE
    /*重启WLCC*/
    if(iot_get_wifi_info()->flag.bit.sta_link_ap) wlcc_start(NETIF_WIFI_STA);
#endif
    
#ifdef CONFIG_USB_4G_MODULE_ENABLE
    /*打开4G任务*/
    if(1 == IotSetData.dev_info_t.on_off.bit._4g_enable) iot_4G_stop();
#endif

#ifdef IOT_OTHER_TASK_SUSPEND_ENABLE
    // 等待操作结束
    vTaskDelay(pdMS_TO_TICKS(1000));

    /*挂起任务释放*/
    resume_other_tasks();
#endif
    
#ifdef LIGHT_SLEEP_DEBUG_LOG
    /*打印休眠唤醒记录*/
    print_light_sleep_wakeup_log();
#endif

    sleep_status = 0;
    ESP_LOGW(TAG, "IOT Sleep Mode End!");
    
    return sleep_status;
}

/*------------------------------------------------------------------------------
 Function: system_wakened_by_DI
 -----------------------------------------------------------------------------*/
/**
  * @brief      系统DI唤醒
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void system_wakened_by_DI(void)
{
    int level = gpio_get_level(WAVE_DI_GPIO);
    if (( 1 == level ) 
        && ( 1 == reals.IOT_Status_Flag.sBit.system_sleep_flag )) {
        ESP_LOGI(TAG, "GPIO pin %d level = %d", WAVE_DI_GPIO, level);

        // 退出休眠状态
        reals.IOT_Status_Flag.sBit.system_sleep_flag = 0;
        can_node_rd.Inv[reals.Addr_can_self].inv_base.ctrl_status.bit.sleep_on = 0;
        can_node_rd.Pack[0].pack_announce.pack_set_status.status.sleep_status = 0;

#if defined(SLEEP_MODE_POWER_ENABLE) && !defined(CONFIG_IOT_AUTO_LIGHT_SLEEP_ENABLE)
        // 系统唤醒，关闭对应辅源使能
        GPIO_SET_LEVEL_HOLD(SLEEP_POWER_DO_GPIO, 0);
#endif
    }
}

/*------------------------------------------------------------------------------
 Function: system_be_wakened_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT被唤醒检查
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
void system_be_wakened_check(void)
{
#ifdef IOT_WAKEUP_BY_GPIO_ENABLE
    // 系统DI唤醒
    system_wakened_by_DI();
#endif
}

/*------------------------------------------------------------------------------
 Function: system_do_wakeup_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      系统DO唤醒
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void system_do_wakeup_check(void)
{
#ifdef SYSTEM_WAKEUP_BY_GPIO_ENABLE
    /*符合条件时执行DO唤醒*/
    if ( 1 == reals.IOT_Status_Flag.sBit.system_wakeup_flag ) {
        // 系统开机（系统开机当前为预留）或退出休眠状态或恢复工作；（上电启动，唤醒）
        output_1hz_5cycles_timer();
        reals.IOT_Status_Flag.sBit.system_wakeup_flag = 0;
    }
#endif    
}

/*------------------------------------------------------------------------------
 Function: iot_sleep_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      休眠任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
static void iot_sleep_task(void * Parameters)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));		

        /*进入休眠*/
        if ( IOT_Sleep_Flag == 0 ) {
            IOT_Sleep_Flag = iot_sleep_start();
        }

        /*休眠运行中*/
        if ( IOT_Sleep_Flag == 1 ) {
            if ( 1 == reals.IOT_Status_Flag.sBit.system_sleep_flag ) {
#ifdef IOT_OTHER_TASK_SUSPEND_ENABLE
                /*休眠备用主任务*/
                iot_period_task_by_sleep();
#endif
                /*休眠时WIFI重连检查*/
                iot_wifi_reconnect_in_sleep(false);
            } else {
                /*退出休眠*/
                IOT_Sleep_Flag = iot_sleep_end();
    
                /*退出休眠WIFI重连检查*/
                iot_wifi_reconnect_in_sleep(true);
    
#ifdef SLEEP_MODE_POWER_ENABLE   
                // 系统唤醒，关闭对应辅源使能
                GPIO_SET_LEVEL_HOLD(SLEEP_POWER_DO_GPIO, 0);
#endif
                // 成功退出，删除任务
                if(!IOT_Sleep_Flag) goto __exit;
            }
        }

#if 0
        if (iot_sleep_task_handle != NULL) {
            ESP_LOGI(TAG, "iot_sleep_task_handle stack high water mark: %u", uxTaskGetStackHighWaterMark(iot_sleep_task_handle));
        }
#endif        
    }
    
__exit:
    ESP_LOGW(TAG, "iot_sleep_task exit!");
    iot_sleep_task_handle = NULL;
    vTaskDelete(NULL); 
}

/*------------------------------------------------------------------------------
 Function: Sleep_Task_Create
 -----------------------------------------------------------------------------*/
/**
  * @brief      创建休眠任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Sleep_Task_Create(void)
{
    if ( !iot_sleep_task_handle && (1 == reals.IOT_Status_Flag.sBit.system_sleep_flag)) {
        if (CREATE_TASK(iot_sleep_task, "iot_sleep_task", 3584, NULL, 1, &iot_sleep_task_handle, 1) == pdPASS) {
            ESP_LOGW(TAG, "iot_sleep_task create successfully.");
        }
    }
}

/*------------------------------------------------------------------------------
 Function: iot_sleep_twai_stop
 -----------------------------------------------------------------------------*/
/**
  * @brief      休眠时停止TWAI驱动，释放twai锁
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t 1-成功 0-失败
  */
static uint8_t iot_sleep_twai_stop(void)
{
    esp_err_t ret = esp_canbus_deinit();
    if(ret != ESP_OK){
        return 0;
    }
    return 1;
}

/*------------------------------------------------------------------------------
 Function: iot_sleep_twai_restart
 -----------------------------------------------------------------------------*/
/**
  * @brief      休眠结束时重启TWAI驱动
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t 1-成功 0-失败
  */
static uint8_t iot_sleep_twai_restart(void)
{    
    esp_canbus_init();
    return 1;
}
