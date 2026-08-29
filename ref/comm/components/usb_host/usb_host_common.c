/**
  ******************************************************************************
  * @file      usb_host_common.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/31
  * @brief     USB主机通用配置
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/31 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */


/* ================================ 库文件引用 ================================ */
 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "usb_host_common.h"
#include "utils.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[USB_HOST_COMMON]"

/* =============================== 文件内全局变量 ================================ */

static bool usb_host_driver_status = false;

/* ================================ 模块函数定义 ================================ */


/**
 * @brief 初始化USB模块电源控制GPIO
 */
static void USB_POWER_GPIO_INIT(uint8_t level)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << (USB_POWER_ON_GPIO)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(USB_POWER_ON_GPIO, level);                // 设置输出电平
}

/**
 * @brief USB模块电源复位
 */
void USB_POWER_RESET(void)
{
#ifdef CONFIG_USB_POWER_ENABLE
    USB_POWER_GPIO_INIT(0);    
    vTaskDelay(pdMS_TO_TICKS(500));
    USB_POWER_GPIO_INIT(1);        
#endif
}

/*------------------------------------------------------------------------------
 Function: usb_lib_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      USB主机任务栈
  * @param[in]  void *arg  
  * @param[out] None
  * @return     static void
  */
static void usb_lib_task(void *arg)
{
    // Install USB Host driver. Should only be called once in entire application
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    //Signalize the usbh_cdc_driver_install, the USB host library has been installed
    xTaskNotifyGive(arg);

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (ESP_OK == usb_host_device_free_all()) {
                ESP_LOGI(TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "Wait for the FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "Get FLAGS_ALL_FREE");
            has_clients = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices, uninstall USB Host library");

    // Clean up USB Host
    vTaskDelay(100); // Short delay to allow clients clean-up
    usb_host_uninstall();
    ESP_LOGD(TAG, "USB Host library is uninstalled");
    vTaskDelete(NULL);
}

/**
 * @brief  初始化USB Host模块，启动USB主机任务并等待库安装完成
 *
 * @return ESP_OK    初始化成功
 * @return ESP_FAIL  初始化失败或USB主机库未安装
 */
esp_err_t usb_host_module_init(void)
{
#ifdef CONFIG_USB_POWER_ENABLE
    // 断电状态下初始化，避免异常
    USB_POWER_GPIO_INIT(0);    
    vTaskDelay(pdMS_TO_TICKS(500));
#endif

    BaseType_t core_id = (CONFIG_USBH_TASK_CORE_ID < 0) ? tskNO_AFFINITY : CONFIG_USBH_TASK_CORE_ID;
    BaseType_t task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), CONFIG_USBH_TASK_BASE_PRIORITY, NULL, core_id);
    ESP_RETURN_ON_FALSE(task_created == pdPASS, ESP_FAIL, TAG, "xTaskCreatePinnedToCore failed");
    // Wait unit the USB host library is installed
    uint32_t notify_value = ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));
    if (notify_value == 0) {
        ESP_LOGE(TAG, "USB host library not installed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "USB host library install successfully.");
    usb_host_driver_status = true;
    
#ifdef CONFIG_USB_POWER_ENABLE
    USB_POWER_GPIO_INIT(1);
#endif

    return ESP_OK;
}
