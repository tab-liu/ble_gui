/**
  ******************************************************************************
  * @file      app_main.c
  * @version   1.0.0
  * @author    yangjunhua
  * @date      2025/7/30
  * @brief     系统初始化
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/7/30   <td>1.0.0  <td>yangjunhua <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "nvs_store.h"
#include "filesystem.h"
#include "parameter.h"
#include "app_bt.h"
#include "can_protocol.h"
#include "modbus_protocol.h"
#include "modbus_data.h"
#include "iot_period_task.h"
#include "ble_crypt.h"
#include "iot_wifi.h"
#include "iot_mqtt.h"
#include "http_client.h"
#include "uart_device_process.h"
#include "sync_time.h"
#include "task_debug.h"
#include "iot_partition.h"
#include "iot_rsa.h"

#include <esp_log.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <stdio.h>
#include <string.h>

#include "comm_define.h"
#include "iot_sleep.h"
#include "dev_modbus_manage.h"
#include "usb_host_common.h"
#include "usb_host_hid.h"
#include "mqtt_payload_tlv.h"
#include "log_redirect.h"
#include "aiot_log.h"
#include "ext_rtc.h"
#include "eeprom_init.h"
#include "ext_flash.h"
#include "rs485_transmit.h"
#include "modbus_tlv.h"

#ifdef CONFIG_LCD_ENABLE  
#include "custom.h"
#endif

#define TAG "[app_main]"

// 数据通道初始化函数
extern void udt_configuration(void);

/**
  * @brief 调试信息打印等级设置
  */
void app_log_level_set(void)
{
#ifdef MASS_PRODUCTION_CONFIG_ENABLE
    // 生产模式默认日志不打印
    esp_log_level_set("*", ESP_LOG_NONE);
#else
    // 研发模式默认日志打印
    esp_log_level_set("*", ESP_LOG_INFO);
    apply_log_levels_from_table();
#endif

#if CONFIG_LOG_UDP_REDIRECT == 2
    // DevAssist日志初始化
    aiot_log_init();
    log_redirect_init(false);
#elif CONFIG_LOG_UDP_REDIRECT == 1
    // UDP日志重定向初始化(默认不开启)
    log_redirect_init(true);
#endif
}

void system_init(void)
{
    // 初始化日志等级
    app_log_level_set();

    /*初始化NVS存储*/
    nvs_store_init();

    /*休眠相关初始化*/
    iot_sleep_config_init();

#ifdef CONFIG_USER_SPI_EXT_FLASH_ENABLE
    /*外部flash初始化*/
    if(0 != ext_flash_init()) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_fault.bit.spi_flash_fault = 1;
    }
#endif

#ifdef CONFIG_IIC_BUS_EXT_RTC_ENABLE    
    /*外部rtc初始化*/
	ext_rtc_init();	
#endif  

#ifdef CONFIG_IIC_BUS_EXT_EEPROM_ENABLE
    /*外部eeprom初始化*/
    eeprom_init(); 
#endif

    /*挂载文件系统*/
    fs_mount();
    
    /* Init and register system/core components */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#ifdef CONFIG_UART0_SN_FACTORY_WRITE_ENABLE
    /*启动标定串口任务，标定完成或已标定会自动关闭*/
    fcty_cal_app();
#endif

    /*初始化参数, 若存在则读取，否则写入默认值*/
    parameter_init(); 

    /*系统时间/时区初始化*/
    system_time_init();

    /*加密初始化*/
    encrypt_data_init();
    
    /*刷新加密参数*/
    refresh_encrypt_param();
    
#ifdef CONFIG_CAN_PORT_ENABLE
    /*CAN变量初始化*/
    iot_can_data_init();
#endif
    
    /*Modbus寄存器初始化*/
    Modbus_beta_reg_table_register_init();
    Modbus_21000_semaphore_init();
    Modbus_21000_bind_msg_create();
    copy_data_from_set_to_modbus();
#ifdef CONFIG_MODBUS_REG_TLV_ENABLE
    modbus_tlv_init(0);
#endif

	/*Modbus设备列表初始化*/
	dev_modbus_addr_manage_init();
	
#ifdef  CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
    /*UDT数据通道初始化配置*/
    udt_configuration();
#endif	

#ifdef CONFIG_CAN_PORT_ENABLE
    /*CAN总线初始化*/
    can_init_top();
#endif

#ifdef CONFIG_MQTT_PAYLOAD_TLV_ENABLE
    /*MQTT TLV初始化配置*/
    mqtt_tlv_init();
#endif

#ifdef CONFIG_RS485_PORT_ENABLE
    /*rs485初始化*/
    rs485_init();
#endif

#ifdef CONFIG_USB_HOST_ENABLE
    /*USB接口HOST驱动初始化*/
    usb_host_module_init();
#endif

#ifdef CONFIG_USB_HOST_HID_ENABLE
    /*USB接口HID类驱动初始化*/
    hid_host_init();
#endif

#ifdef CONFIG_LCD_ENABLE
    /*显示屏初始化任务*/    
    ui_lvgl_init();
#endif

    // 等待前方任务初始化完成
    vTaskDelay(pdMS_TO_TICKS(100));

    // 其他系统初始化代码可以放在这里
    ESP_LOGI(TAG, "System initialization complete.");
}

static TaskHandle_t iot_ble_task_handle = NULL;
static TaskHandle_t iot_cloud_process_task_handle = NULL;
static TaskHandle_t iot_period_fast_task_handle = NULL;
static TaskHandle_t iot_period_slow_task_handle = NULL;
static TaskHandle_t System_File_Data_Process_Task_handle = NULL;
static TaskHandle_t https_period_task_handle = NULL;

/*------------------------------------------------------------------------------
 Function: log_task_stack_highwater
 -----------------------------------------------------------------------------*/
/**
  * @brief      打印任务线程内存消耗高水平位置
  * @param[in]  void *pvParameters  
  * @param[out] None
  * @return     void
  */
void log_task_stack_highwater(void)
{
    struct { TaskHandle_t h; const char *name; } tasks[] = {
        { iot_ble_task_handle,               "iot_ble_task_handle" },
        { iot_cloud_process_task_handle,     "iot_cloud_process_task_handle" },
        { iot_period_fast_task_handle,       "iot_period_fast_task_handle" },
        { iot_period_slow_task_handle,       "iot_period_slow_task_handle" },
        { System_File_Data_Process_Task_handle, "System_File_Data_Process_Task_handle" },
        { https_period_task_handle,          "https_period_task_handle" },
    };

#if 0   //  print    
    ESP_LOGI(TAG, "sizeof(StackType_t) = %u", (unsigned)sizeof(StackType_t));
    for (size_t i = 0; i < sizeof(tasks)/sizeof(tasks[0]); ++i) {
        if (tasks[i].h) {
            UBaseType_t words = uxTaskGetStackHighWaterMark(tasks[i].h);
            ESP_LOGI(TAG, "%-36s : %5u words (%5u bytes)", tasks[i].name, (unsigned)words, (unsigned)(words * sizeof(StackType_t)));
        } else {
            ESP_LOGI(TAG, "%-36s : not created", tasks[i].name);
        }
    }
#else   // check
    for (size_t i = 0; i < sizeof(tasks)/sizeof(tasks[0]); ++i) {
        if (tasks[i].h) {
            UBaseType_t words = uxTaskGetStackHighWaterMark(tasks[i].h);
            if(words <= 1024) {
                ESP_LOGW(TAG, "%-36s : %5u words (%5u bytes)", tasks[i].name, (unsigned)words, (unsigned)(words * sizeof(StackType_t)));
            }
        }
    }
#endif
}

/*------------------------------------------------------------------------------
 Function: task_create
 -----------------------------------------------------------------------------*/
/**
  * @brief      任务创建函数
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void task_create(void)
{
    struct {
        TaskFunction_t task_func;
        const char *name;
        uint32_t stack_size;
        void *param;
        UBaseType_t priority;
        TaskHandle_t *handle;
        BaseType_t core_id;
    } tasks[] = {
        {iot_ble_task,                  "iot_ble_task",         1024 * 5,   NULL, 3, &iot_ble_task_handle,                  1},
        {iot_cloud_process_task,        "iot-cloud",            1024 * 7,   NULL, 1, &iot_cloud_process_task_handle,        1},
        {iot_period_fast_task,          "iot-period-fast",      1024 * 5,   NULL, 2, &iot_period_fast_task_handle,          1},
        {iot_period_slow_task,          "iot-period-slow",      1024 * 6,   NULL, 1, &iot_period_slow_task_handle,          1},
        {System_File_Data_Process_Task, "System-File-Process",  1024 * 6,   NULL, 1, &System_File_Data_Process_Task_handle, 1},
        {https_period_task,             "http period task",     1024 * 6,   NULL, 1, &https_period_task_handle,             1},
    };

    for (size_t i = 0; i < sizeof(tasks)/sizeof(tasks[0]); ++i) {
        if (CREATE_TASK(tasks[i].task_func, tasks[i].name, tasks[i].stack_size, tasks[i].param, tasks[i].priority, tasks[i].handle, tasks[i].core_id) != pdPASS) {
            ESP_LOGE(TAG, "%s create failed", tasks[i].name);
            abort();
        }
    }    
}

/**
  * @brief 系统启动入口
  */
void app_main(void)
{
    /*系统初始化*/
    system_init();
    
    /*任务创建*/
    task_create();

    // 任务时间统计声明和初始化
    TASK_TIME_STATS_DECLARE(app_main);

	while(1)
	{
		TASK_TIME_START(app_main);
#ifdef CONFIG_CAN_PORT_ENABLE
        /*CAN总线数据解析*/
	    CanRx_Parse_Task(); // 需要快速处理，否则会丢帧
#endif
	    TASK_TIME_END(app_main);
	    TASK_TIME_PRINT(app_main, 1000);
        TASK_TIME_RUNS_MAX_TO_FIELD(app_main, reals.thread1_max_runtime_us);
        TASK_TIME_TIMEOUT_CLEAN(app_main, TASK_COMMON_CLEAN_TIMEOUT_MS);
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
