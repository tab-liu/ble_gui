#ifndef XMODEM_CLIENT_H
#define XMODEM_CLIENT_H


#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#define JUMP_BYTES          20


enum {
    GROUP_INV=1, 
    GROUP_PACK=2, 
    GROUP_IOT=3, 
    GROUP_LCD=4,
    GROUP_ATS=5,
    GROUP_PANEL=6,
    GROUP_CHARGE=7            
    };

typedef enum {
    OTA_CH_SELF_TO_UART = 0 ,   //xmodem,server
    OTA_CH_SELF_TO_CAN,         //CAN beta
    OTA_CH_SELF_TO_WIFI_MESH,   //xmodem,server
    OTA_CH_BLE_TO_SELF,         //xmodem,client
    OTA_CH_CLOUD_TO_SELF,       //HTTP
    OTA_CH_WIFI_MESH_TO_SELF,   //xmodem,client
    OTA_CH_CAN_TO_SELF,         //CAN beta
    OTA_CH_UART_TO_SELF,        //xmodem,client
    OTA_CH_UNKOWN = 0xFF,       //CHANNEL_UNKOWN
}channel_type;


#define     XMODEM_SOH                     0x01    // 128 byte transmission head
#define     XMODEM_STX                     0x02    // 1024 byte transmission head
#define     XMODEM_ETX                     0x03    // break transmission
#define     XMODEM_EOT                     0x04    // transmission finished
#define     XMODEM_ACK                     0x06    // response normal ack
#define     XMODEM_NAK                     0x15    // response error ack
#define     XMODEM_CAN                     0x18    // cancel transmission
#define     XMODEM_EOF                     0x1A    //
#define     XMODEM_CRC                     0x43    // mean crc16 verify 'C'




typedef struct {
    uint16_t paylen;
    uint8_t *payload;
}xblock_struct;

typedef struct {
    uint8_t  step;
    uint8_t  exit;
    uint8_t  block_seq;
    uint8_t  is_esp_ota;
    uint8_t  firmware_resend_step;//windy add, IOT/DSP固件需要分2步骤依次OTA：1-object1(DSP-UART;IOT-CAN); 2-object2(DSP-CAN;IOT-self) ；0-无效不转发
    uint8_t  system_restart;
    uint8_t target_id;//CAN ID OTA 目的地址
    uint8_t file_type;//OtaFileType ,modbus 701
    uint32_t version;
    uint16_t group;//产品组类型
    uint16_t file_size;
    channel_type  used_chl;
    uint16_t time_count;
    uint16_t resend_count;
    uint32_t crc32;
    xblock_struct block;
    TimerHandle_t timer;
    FILE *pfile;
}xmodem_struct;
extern  uint8_t TypeCnt;//const
extern xmodem_struct gXmodem_Status;


#pragma pack(push, 2)
/**
 * @brief ota组定义
 */
typedef union {
    uint16_t all;
    struct {
        uint8_t dev_id : 8;		// 设备id(索引),选择该组下哪个ID需要升级0~0xFF(从0开始，0表示#1；0xFF表示广播)
        uint8_t dev_type : 8;	// 设备类型，选择需要升级的组（1：INV-OTA；2：PACK-OTA；3: IOT-OTA; 4-LCD）
    };
} ota_group_t;

/**
 * @brief ota参数设置结构体定义
 */
typedef struct {
	uint16_t start_flag;
	uint16_t file_type;
	uint16_t ver_low;
	uint16_t ver_high;
	uint16_t file_size;			// 文件大小单位kb
	ota_group_t group;
} ota_set_t;

/**
 * @brief ota升级状态结构体定义
 */
typedef struct {
	ota_group_t group;
	struct {
		uint16_t level : 8;
		uint16_t is_ota : 8;
	};
	struct {
		uint16_t now_pos : 8;
		uint16_t target_pos : 8;
	};
	struct {
		uint16_t err_code : 8;
		uint16_t progress : 8;
	};
    uint16_t online_client_cnt;    // 批量升级：当前在线的客户端数量
} ota_state_t;
#pragma pack(pop)

uint8_t vXmodemCmdCheck(uint8_t md_addr, uint8_t channel);
uint8_t vXmodemClientInit(int id, uint8_t file_type, uint16_t size, uint32_t version, uint16_t group, channel_type channel);
uint8_t Xmodem_Client_top(channel_type channel, const uint8_t *pdata, uint16_t plen);
uint8_t vXmodemClientExit(channel_type channel);
int esp_ota_is_doing(void);
int xmodem_client_is_doing(void);
uint8_t* can_ota_buffer_addr_get(void);
void ota_status_err_set(void);
int can_ota_data_init(uint8_t type, uint32_t version, uint16_t filesize, uint16_t dev_id, uint32_t len);
int can_ota_begin(uint8_t id, uint8_t type, uint32_t version);

#endif
