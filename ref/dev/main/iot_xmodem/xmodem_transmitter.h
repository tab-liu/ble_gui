#ifndef xmodem_transmitter_h
#define xmodem_transmitter_h
#include <inttypes.h>

#include "xmodem_client.h"

enum XMODEM_TRANSMIT_STATES {
	XMODEM_TRANSMIT_INITIAL,
    XMODEM_TRANSMIT_WAIT_FOR_C,
	XMODEM_TRANSMIT_WAIT_FOR_C_ACK,
	XMODEM_TRANSMIT_WRITE_BLOCK_FAILED,
	XMODEM_TRANSMIT_ABORT_TRANSFER,			// 该状态会一直维持
	XMODEM_TRANSMIT_WRITE_BLOCK,
	XMODEM_TRANSMIT_C_ACK_RECEIVED,
	XMODEM_TRANSMIT_COMPLETE,				// 该状态会一直维持
	XMODEM_TRANSMIT_WRITE_EOT,
	XMODEM_TRANSMIT_WAIT_FOR_EOT_ACK,
	XMODEM_TRANSMIT_TIMEOUT_EOT,
	XMODEM_TRANSMIT_WRITE_BLOCK_TIMEOUT,
	XMODEM_TRANSMIT_WRITE_ETB,
	XMODEM_TRANSMIT_WAIT_FOR_ETB_ACK,
	XMODEM_TRANSMIT_TIMEOUT_ETB,
	XMODEM_TRANSMIT_UNKNOWN
};

/*
windy add tbd 
*/
typedef struct {
    uint8_t  step;
    uint8_t  exit;//2-下级uart；3-下级CAN
    uint8_t  block_seq;
    uint8_t  is_esp_ota;//1-IOT本身OTA;0-下级设备OTA
    uint8_t  system_restart;
    uint8_t target_id;//CAN ID OTA 目的地址
    uint8_t file_type;//OtaFileType ,modbus 701
    uint32_t version;
    uint16_t file_size;
    channel_type  used_chl;
    uint16_t time_count;//x1ms
    uint16_t resend_count;
    uint32_t crc32;
    xblock_struct block;
//    TimerHandle_t timer;
    FILE *pfile;

	int ota_flag;//xmodem server OTA线程是否开启，1-开启；0-no

}xmodem_server_struct;
extern xmodem_server_struct gXmodem_server_Status;


typedef enum XMODEM_TRANSMIT_STATES xmodem_transmit_state_t;

typedef void(*write_callback_t) (const uint32_t requested_size, uint8_t * buffer);
typedef void(*read_callback_t) (const uint32_t requested_size, uint8_t * buffer, uint32_t * returned_size);
typedef void(*uart_send_callback_t) (uint8_t * write_status);
typedef int(*outbound_callback_t) (void);
typedef int(*inbound_callback_t) (void);
typedef void(*get_buffer_callback_t) (const uint32_t position, uint8_t * buffer, uint16_t buff_size);
typedef int(*state_update_callback_t) (void);
typedef int(*state_init_callback_t) (void);

extern write_callback_t xmd_write_data;

extern uart_send_callback_t xmd_uart_send;

uint8_t xmodem_transmit_init(uint32_t size);
//uint8_t xmodem_transmit_process(const uint32_t current_time);
uint8_t Xmodem_Server_top(channel_type channel,const uint32_t current_time) ;//xmodem_transmit_process

void xmodem_transmitter_cleanup(void);
void xmodem_transmitter_set_callback_write(write_callback_t callback);
void xmodem_transmitter_set_callback_uart_send(uart_send_callback_t callback);
void xmodem_transmitter_set_callback_read(read_callback_t callback);
void xmodem_transmitter_set_callback_is_outbound_full(outbound_callback_t callback);
void xmodem_transmitter_set_callback_is_inbound_empty(inbound_callback_t callback);
void xmodem_transmitter_set_callback_get_buffer(get_buffer_callback_t callback);
void xmodem_transmitter_set_callback_state_update(state_update_callback_t callback);
void xmodem_transmitter_set_callback_state_init(state_init_callback_t callback);
xmodem_transmit_state_t xmodem_transmit_state_get(void);
uint8_t xmodem_transmit_otapct_get(void);

#endif
