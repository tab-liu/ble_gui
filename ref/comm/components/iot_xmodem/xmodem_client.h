#ifndef XMODEM_CLIENT_H
#define XMODEM_CLIENT_H


#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "ota_define.h"
#include "comm_define.h"

#define     XMODEM_SOH                     0x01    // 128 byte transmission head
#define     XMODEM_STX                     0x02    // 1024 byte transmission head
#define     XMODEM_ETX                     0x03    // break transmission
#define     XMODEM_EOT                     0x04    // transmission finished
#define     XMODEM_ACK                     0x06    // response normal ack
#define     XMODEM_NAK                     0x15    // response error ack
#define     XMODEM_CAN                     0x18    // cancel transmission
#define     XMODEM_EOF                     0x1A    //
#define     XMODEM_CRC                     0x43    // mean crc16 verify 'C'

extern ota_mode_t ota_mode;

typedef struct {
    uint16_t paylen;
    uint8_t *payload;
}xblock_struct;

typedef struct {
    uint8_t  step;
    uint8_t  exit;
    uint8_t  errcode;
    uint8_t  block_seq;
    uint8_t  is_esp_ota;
    uint8_t  system_restart;
    uint8_t target_id;//CAN ID OTA 目的地址
    uint8_t file_type;//OtaFileType ,modbus 701
    uint32_t version;
    uint16_t file_size;
    channel_type  used_chl;
    uint16_t time_count;
    uint16_t resend_count;
    uint32_t crc32;
    xblock_struct block;
    TimerHandle_t timer;
    FILE *pfile;
}xmodem_struct;

#pragma pack(push, 2)

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
} ota_state_t;
#pragma pack(pop)

uint8_t vXmodemCmdCheck(uint8_t md_addr, uint8_t channel);
uint8_t vXmodemClientInit(int id, uint8_t file_type, uint16_t size, uint32_t version,  channel_type channel);
uint8_t vXmodemClient(channel_type channel, const uint8_t *pdata, uint16_t plen);
uint8_t vXmodemClientExit(channel_type channel);
int esp_ota_is_doing(void);
int xmodem_client_is_doing(void);
uint8_t* can_ota_buffer_addr_get(void);
void ota_status_err_set(void);
int can_ota_data_init(uint8_t type, uint32_t version, uint16_t filesize, uint16_t dev_id, uint32_t len);
int can_ota_begin(uint8_t id, uint8_t type, uint32_t version);

#endif
