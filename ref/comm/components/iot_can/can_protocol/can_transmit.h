#ifndef CAN_TRANSMIT_H
#define CAN_TRANSMIT_H

//#include "can_init.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
#include "driver/twai.h"
#include "stdint.h"

#define CAN_ADDR_SWAP( id )     ((id) & 0xFFFF0000) | (((id) & 0xFF00) >> 8) | (((id) & 0x00FF) << 8)
#define CAN_BROADCAST_ADDRESS   0xFF

typedef struct {
    twai_message_t *pdata;
}can_queue_msg_struct;

void esp_canbus_send_task(void);//(void * pvParameters) ;
// void esp_canbus_recv_task(void);
void esp_canbus_recv_task(void * pvParameters);

void esp_canbus_state_change(void);

// void esp_canbus_base_recv_send_tas(void);//k(void * pvParameters);
void esp_canbus_init(void);
esp_err_t esp_canbus_deinit(void);

void print_twai_status(void);

#endif
