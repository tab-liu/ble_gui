#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "do_di_ctrl.h"
#include "iot_period_task.h"
#include "iot_mqtt.h"
#include "xmodem_client.h"
#include "http_client.h"
//#include "cc1312_ota.h"
//#include "drv_24cxx.h"
#include "dev_discovery.h"
#include "drv_pcf8563.h"
#include "dev_data_record.h"

#include "uart_device_process.h"


//#include "app_data.h"
//#include "ota_manager.h"
//#include "app_wifi.h"

/**
 * LED几种模式定义
 * 0、初始模式：红绿交替 0.25Hz 优先级0
 * 1、配网模式：黄灯快闪 0.5Hz  优先级1
 * 2、升级模式：绿灯快闪 0.5Hz  优先级2
 * 3、故障模式：红灯闪烁 1Hz    优先级3
 * 4、告警模式：黄灯常亮        优先级4
 * 5、断网模式：黄灯闪烁 1Hz    优先级5 (保留)
 * 6、正常运行：绿灯闪烁 1Hz    优先级6
*/
#define TAG     "[do_di_ctrl]"









#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


//windy 20240118 config
/*
差异：
	ATS-		;	DTU
IO8:LED_heart;	AI_VBUS_USB
IO3:/		 ;	button_DI
IO13:LED_GRID;	LED_Ethernet
IO14:LED_BACKUP;LED_inverter
IO21:LED_COMM;	LED_error
IO47:CAN_RX;	EN_PSU_Aux_power_en_DO(active L)
IO45:LED_FAULT;	/
IO41:Uart1_RX(AT403);Uart1_RX(Sub1GHz)
IO42:Uart1_TX(AT403);Uart1_TX(Sub1GHz)
U0RXD:UART0_RX(1GHz);UART0_RX(RS485)
U0TXD:UART0_TX(1GHz);UART0_TX(RS485)
IO1:/;				 DI_DRM



相同：
IO38：DO_4G_Rst
IO2： DO_Reset_Sub1GHz


*/

#ifdef  CONFIG_HARDWARE_ATS
#define LED1_GPIO           GPIO_NUM_45//FAULT
#define LED2_GPIO          	GPIO_NUM_21//COMM
#define LED3_GPIO           GPIO_NUM_14//BACKUP
#define LED4_GPIO          	GPIO_NUM_13//GRID
#define HEART_GPIO          GPIO_NUM_8//heart

#define LED_ON                  1
#define LED_OFF                 0

#elif defined  CONFIG_HARDWARE_DTU100
#define LED2_GPIO          		GPIO_NUM_21//LED_error
#define LED3_GPIO            	GPIO_NUM_14//LED_inverter
#define LED4_GPIO          		GPIO_NUM_13//LED_Ethernet

#define LED_ON                  0
#define LED_OFF                 1

#define DI_DRM_GPIO          		GPIO_NUM_1//DI
#define DI_BUTTON_GPIO          	GPIO_NUM_3//DI

#define DO_EN_PSU_Aux_power_GPIO  	GPIO_NUM_47//DO (active L)
#define AI_VBUS_USB_GPIO           	GPIO_NUM_8//AI



#endif




#define LED_BLACK               0
#define LED_GREEN               1
#define LED_YELLOW              2
#define LED_RED                 3

enum 
{
    INIT_LED_MODE = 0, // 系统初始化模式
    PROVISION_LED_MODE = 1,
    OTA_LED_MODE = 2,
    FAULT_LED_MODE = 3,
    DISCONNECT_LED_MODE = 4,
    RUNNING_LED_MODE = 5,
    ALERT_LED_MODE = 6
}led_model_t;
#if 0
static void app_state_led_color_ctrl(uint8_t color)
{
    switch (color) 
    {
        case LED_BLACK: // 全部关闭
            gpio_set_level(LED1_GPIO,    LED_OFF);
            gpio_set_level(LED2_GPIO,  LED_OFF);
            break;

        case LED_GREEN: // 绿灯亮起
            gpio_set_level(LED2_GPIO,  LED_ON);
            gpio_set_level(LED1_GPIO,    LED_OFF);
            break;

        case LED_YELLOW: // 黄灯亮起
            gpio_set_level(LED1_GPIO,    LED_ON);
            gpio_set_level(LED2_GPIO,  LED_ON);
            break;

        case LED_RED:   // 红灯亮起
            gpio_set_level(LED1_GPIO,    LED_ON);
            gpio_set_level(LED2_GPIO,  LED_OFF);
            break;

        default: // 异常模式关闭LED
            gpio_set_level(LED1_GPIO,    LED_OFF);
            gpio_set_level(LED2_GPIO,  LED_OFF);
            break;
    }
}

static void app_state_led_update(uint8_t mode, int time_ms)
{
    switch (mode) 
    {
        case INIT_LED_MODE:     // 初始化模式 - 黄绿交替 0.25Hz
            if ((time_ms % 500) < 250) 
                app_state_led_color_ctrl(LED_GREEN);
            else 
                app_state_led_color_ctrl(LED_RED);
            break;

        case DISCONNECT_LED_MODE:
        case PROVISION_LED_MODE: // 配网模式 - 黄灯快闪 0.5Hz
            if ((time_ms % 1000) < 500) 
                app_state_led_color_ctrl(LED_YELLOW);
            else 
                app_state_led_color_ctrl(LED_BLACK);
            break;

        case OTA_LED_MODE:      // 升级模式 - 绿灯快闪 0.5Hz
            if ((time_ms % 1000) < 500)
                app_state_led_color_ctrl(LED_GREEN);
            else 
                app_state_led_color_ctrl(LED_BLACK);
            break;

        case FAULT_LED_MODE:    // 故障模式 - 红灯慢闪 1Hz
            if ((time_ms % 2000) < 1000)
                app_state_led_color_ctrl(LED_RED);
            else 
                app_state_led_color_ctrl(LED_BLACK);
            break;

        case RUNNING_LED_MODE: // 正常运行 - 绿灯慢闪 1Hz
            if ((time_ms % 2000) < 1000)
                app_state_led_color_ctrl(LED_GREEN);
            else 
                app_state_led_color_ctrl(LED_BLACK);
            break;

        case ALERT_LED_MODE:
            app_state_led_color_ctrl(LED_YELLOW);
            break;
            
        default:  break;
    }
}
#endif





//#define LED_DBUG

/*------------------------------------------------------------------------------
 Function: Microinv_fault_alarm_get
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取逆变报警状态
                0：正常 1：存在逆变故障
  * @param[in]  None
  * @param[out] None
  * @return     uint8_t
  */
uint8_t Microinv_fault_alarm_get()
{
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t result = 0;

//    for (i = 0; i < NET_SUB1G_MAX_POINT; i++)
//    {
//        if (NET_POINT_ONLINE == reals.net_point_base_Info[i].net_point_online)
//        {
//            if(Inv[i+1].mod_reg01100_Inv_base.fault[0] || Inv[i+1].mod_reg01100_Inv_base.fault[1] 
//                || Inv[i+1].mod_reg01100_Inv_base.fault[2] || Inv[i+1].mod_reg01100_Inv_base.fault[3] 
//                || Inv[i+1].mod_reg01100_Inv_base.fault[4])
//            {
//                result = 1;
//                break;
//            }
//            else if(Inv[i+1].mod_reg01100_Inv_base.alarm[0] || Inv[i+1].mod_reg01100_Inv_base.alarm[1] 
//                || Inv[i+1].mod_reg01100_Inv_base.alarm[2] || Inv[i+1].mod_reg01100_Inv_base.alarm[3])
//            {
//                result = 1;
//                break;
//            }
//        }
//    }

    return result;
}


#ifdef  CONFIG_HARDWARE_DTU100

//#define LED_DBUG	
//#define LED_OLD_SPECIFICATION	
#define LED_HEMAI_SPECIFICATION	

#define LED_LEVEL_CNT_4S	20//4s	
#define LED_LEVEL_CNT_2S	10//2s	
#define LED_LEVEL_CNT_1S2	6//1.2s	
#define LED_LEVEL_CNT_0S4	2//0.4s

void led_Start_DTU(void)//200ms
{
    static uint8_t sLED_cnt=0;
    static uint8_t LED_cnt_level=LED_LEVEL_CNT_2S;

	static uint8_t time_cnt_1200ms = 0;
	
    
	if(time_cnt_1200ms < 3)//仅执行3*2000ms
	{
		if(++sLED_cnt >= LED_cnt_level)
		{
			sLED_cnt=0;
			time_cnt_1200ms++;
		}
		
		if((sLED_cnt%10) < 5)//1000ms on;1000ms off
		{
			if(time_cnt_1200ms == 0)
			{
				gpio_set_level(LED4_GPIO,  LED_ON);
			}
			else if(time_cnt_1200ms == 1)
			{
				gpio_set_level(LED3_GPIO,  LED_ON);
			}
			else if(time_cnt_1200ms == 2)
			{
				gpio_set_level(LED2_GPIO,  LED_ON);
			}
	    }
		else
		{
			if(time_cnt_1200ms == 0)
			{
				gpio_set_level(LED4_GPIO,  LED_OFF);
			}
			else if(time_cnt_1200ms == 1)
			{
				gpio_set_level(LED3_GPIO,  LED_OFF);
			}
			else if(time_cnt_1200ms == 2)
			{
				gpio_set_level(LED2_GPIO,  LED_OFF);
			}
	    }
		
		return;
	}
}

#endif

#ifdef  CONFIG_HARDWARE_DTU100

/*------------------------------------------------------------------------------
 Function: DRM0_Stat_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      DRM0s状态检查
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void DRM0_Stat_Check(void)
{
    static uint8_t old_state = 1;
    static uint8_t new_state = 1;

    old_state = new_state;
    new_state = gpio_get_level(DI_DRM_GPIO);

//    ESP_LOGW(TAG, "%d",new_state);

    if (new_state == 0)//关闭inv
    {
        if (old_state != new_state)//跳变时下发关机指令
        {
            reals.inv_drm0_state = 2;
            reals.inv_set_send_slaveaddr = 0;
            ESP_LOGW(TAG, "old_state != new_state  :  inv_drm0_state(2)");
        }
        else if(reals.new_inv_drm0s_set == 1)//设备更新时下发
        {
            reals.new_inv_drm0s_set = 0;
            reals.inv_drm0_state = 2;
            reals.inv_set_send_slaveaddr = 0;
            ESP_LOGW(TAG, "reals.new_inv_drm0s_set == 1  :  inv_drm0_state(2)");
        }
    }
    else
    {
        if (old_state != new_state)//跳变时下发开机指令
        {
            reals.inv_drm0_state = 1;
            reals.inv_set_send_slaveaddr = 0;
            ESP_LOGW(TAG, "old_state != new_state  :  inv_drm0_state(1)");
        }
        else if(reals.new_inv_drm0s_set == 1)//设备更新时下发
        {
            reals.new_inv_drm0s_set = 0;
            reals.inv_drm0_state = 1;
            reals.inv_set_send_slaveaddr = 0;
            ESP_LOGW(TAG, "reals.new_inv_drm0s_set == 1  :  inv_drm0_state(1)");
        }
    }
}


#endif


void app_DO_DI_init(void)
{
#ifdef  CONFIG_HARDWARE_ATS
	 gpio_reset_pin(LED1_GPIO); 	  // 
	 gpio_reset_pin(LED2_GPIO); 	//
	 gpio_reset_pin(LED3_GPIO); 	  // 
	 gpio_reset_pin(LED4_GPIO); 	//
	 gpio_reset_pin(HEART_GPIO);	 //
	 
	 
	 gpio_set_direction(LED1_GPIO, GPIO_MODE_OUTPUT);	  // 输出高低电平
	 gpio_set_direction(LED2_GPIO, GPIO_MODE_OUTPUT);	// 输出高低电平
	 gpio_set_direction(LED3_GPIO, GPIO_MODE_OUTPUT);	  // 输出高低电平
	 gpio_set_direction(LED4_GPIO, GPIO_MODE_OUTPUT);	// 输出高低电平
	 gpio_set_direction(HEART_GPIO, GPIO_MODE_OUTPUT);	 // 输出高低电平
	 
	 
	 gpio_set_level(LED1_GPIO,	LED_OFF);
	 gpio_set_level(LED2_GPIO,	LED_OFF);
	 gpio_set_level(LED3_GPIO,	LED_OFF);
	 gpio_set_level(LED4_GPIO,	LED_OFF);	 
	 gpio_set_level(HEART_GPIO,  LED_OFF);
	 
	 
#elif defined  CONFIG_HARDWARE_DTU100
	 gpio_reset_pin(LED2_GPIO); 	//
	 gpio_reset_pin(LED3_GPIO); 	  // 
	 gpio_reset_pin(LED4_GPIO); 	//
	 
	 
	 gpio_set_direction(LED2_GPIO, GPIO_MODE_OUTPUT);	// 输出高低电平
	 gpio_set_direction(LED3_GPIO, GPIO_MODE_OUTPUT);	  // 输出高低电平
	 gpio_set_direction(LED4_GPIO, GPIO_MODE_OUTPUT);	// 输出高低电平
	 gpio_set_direction(DO_EN_PSU_Aux_power_GPIO, GPIO_MODE_OUTPUT);	// 输入	 
	 gpio_set_direction(DI_DRM_GPIO, GPIO_MODE_INPUT);	// 输入
	 gpio_set_direction(DI_BUTTON_GPIO, GPIO_MODE_INPUT);	// 输入
	 
	 
	 gpio_set_level(LED2_GPIO,	LED_OFF);
	 gpio_set_level(LED3_GPIO,	LED_OFF);
	 gpio_set_level(LED4_GPIO,	LED_OFF);	 
	 gpio_set_level(DO_EN_PSU_Aux_power_GPIO,	LED_ON);	 




#endif
//	  if (xTaskCreatePinnedToCore(app_state_led_task,  "led task", 4 * 1024, NULL, 2, NULL,1) != pdTRUE)
//	  { 
//		  ESP_LOGE(TAG, "app_state_led_task create failed"); 
//		  return ;
//	  }
}
#ifdef  CONFIG_HARDWARE_DTU100

    //ADC1 Channels
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_7
#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_11	//ADC_ATTEN_DB_0,衰减
/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}


/*
差异：
	ATS-		;	DTU
IO8:LED_heart;	AI_VBUS_USB
IO3:/		 ;	button_DI

IO47:CAN_RX;	EN_PSU_Aux_power_en_DO(active L)

IO1:/;				 DI_DRM



相同：
IO38：DO_4G_Rst
IO2： DO_Reset_Sub1GHz

//#define AI_VBUS_USB_GPIO           	GPIO_NUM_8//AI 

*/

adc_cali_handle_t adc1_cali_chan0_handle = NULL;
bool do_calibration1_chan0;
adc_oneshot_unit_handle_t adc1_handle;

void app_ADC_init(void)
{
//-------------ADC1 Init---------------//
//adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
};
ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

//-------------ADC1 Config---------------//
adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = EXAMPLE_ADC_ATTEN,
};
ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

//-------------ADC1 Calibration Init---------------//
do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);
	 

//	//Tear Down
//	ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
//	example_adc_calibration_deinit(adc1_cali_chan0_handle);
	

}

//	C:\Espressif\frameworks\esp-idf-v5.1.2\examples\peripherals\adc\continuous_read\main
void app_ADC_check(void)
{
		esp_err_t ret;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &reals.adc_raw));
        //ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, reals.adc_raw);
        if (do_calibration1_chan0) 
		{
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, reals.adc_raw, &reals.voltage));
            //ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, reals.voltage);
        }
//        vTaskDelay(pdMS_TO_TICKS(1000));





}
#endif


