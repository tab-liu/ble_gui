
#include <string.h>
#include "can_init.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "iot_period_task.h"


#define TAG "[CAN_INIT]"

#define CAN_RX_GPIO_PIN     GPIO_NUM_47
#define CAN_TX_GPIO_PIN     GPIO_NUM_48

// #define CAN_RX_GPIO_PIN     GPIO_NUM_39
// #define CAN_TX_GPIO_PIN     GPIO_NUM_38

// #define CAN_RX_GPIO_PIN     GPIO_NUM_14
// #define CAN_TX_GPIO_PIN     GPIO_NUM_13

// #define CAN_RX_GPIO_PIN     GPIO_NUM_16
// #define CAN_TX_GPIO_PIN     GPIO_NUM_17

#define CAN_IOT_ID          0x00000001

// 测试发现CAN控制器不会接收自身发出的数据
#define ACCEPT_ALL() {.acceptance_code = 0, .acceptance_mask = 0xFFFFFFFF, .single_filter = true}
// #define TWAI_FILTER_CONFIG_ACCEPT_TARGET() {.acceptance_code = ~CAN_IOT_ID, .acceptance_mask = ~CAN_IOT_ID, .single_filter = true}
#define TWAI_FILTER_CONFIG_ACCEPT_TARGET() ACCEPT_ALL()

// CAN控制器的mask与其它厂家mask设置相反，其它厂家mask的bit置位表示过滤器比较该bit，而ESP32的mask置位表示过滤器不关心该bit
// 根据数据手册，code与mask的值在扩展帧及单滤波模式下需要左移3位，而标准帧模式下需要左移5位
// 如下扩展帧mask设置（~(uint32_t)(0xff00<<3)）表示can过滤器比较报文的ID8~ID15bits是否与设置的code：0x66(0x6600<<3)相同
// #define TWAI_FILTER_CONFIG_ACCEPT_TARGET() {.acceptance_code = (0x6600<<3), .acceptance_mask = ~(uint32_t)(0xff00<<3), .single_filter = true}
// 双滤波模式下，可以设置两个标准帧滤波器，但设置两个扩展帧滤波器时，每个滤波器只能过滤CAN帧的ID13~ID28bit

void esp_canbus_init(void)
{
    twai_general_config_t can_config    = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO_PIN, CAN_RX_GPIO_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t  baud_config   = TWAI_TIMING_CONFIG_250KBITS();  // 250k
    twai_filter_config_t  filter_config = TWAI_FILTER_CONFIG_ACCEPT_TARGET(); // 不接收 IOT自己发出的帧

	can_config.rx_queue_len = 200;	// 设置底层can接收缓存队列长度
	ESP_LOGI(TAG,"can_config.intr_flags:%d",can_config.intr_flags);


//#if CONFIG_XTAL_FREQ == 32   // TWAI_CLK_SRC_XTAL = 32M
//#define TWAI_TIMING_CONFIG_25KBITS()    {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 400000, .brp = 0, .tseg_1 = 11, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_50KBITS()    {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 1000000, .brp = 0, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_100KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 2000000, .brp = 0, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_125KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 4000000, .brp = 0, .tseg_1 = 23, .tseg_2 = 8, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_250KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 4000000, .brp = 0, .tseg_1 = 11, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_500KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 8000000, .brp = 0, .tseg_1 = 11, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_800KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 16000000, .brp = 0, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_1MBITS()     {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 16000000, .brp = 0, .tseg_1 = 11, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//	
//#elif CONFIG_XTAL_FREQ == 40   // TWAI_CLK_SRC_XTAL = 40M
//#define TWAI_TIMING_CONFIG_25KBITS()    {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 625000, .brp = 0, .tseg_1 = 16, .tseg_2 = 8, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_50KBITS()    {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 1000000, .brp = 0, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_100KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 2000000, .brp = 0, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_125KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 2500000, .brp = 0, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
//#define TWAI_TIMING_CONFIG_250KBITS()   {.clk_src = TWAI_CLK_SRC_DEFAULT, .quanta_resolution_hz = 5000000, .brp = 0, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}


//windy tbd	.
//	baud_config.tseg_1 = 15;
//	baud_config.tseg_2 = 4;
////	baud_config.sjw = 3;
//	baud_config.triple_sampling = 1;//false;//0-单次采样，1-3次采样

    if (twai_driver_install(&can_config, &baud_config, &filter_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install driver");
    } else {
        if (twai_start() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start driver");
            return;
        }
        ESP_LOGI(TAG, "can start successfully");
    }
}

inline uint8_t esp_canbus_myself_address(void)
{
//    return CAN_IOT_ID;
	return reals.Addr_can_self +IOT_CAN_ADDR;
}
