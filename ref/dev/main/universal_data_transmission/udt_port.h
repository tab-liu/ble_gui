#ifndef __UDT_PORT_H__
#define __UDT_PORT_H__
#include <stdint.h>


#define CAN_TYPE_UDT_QUERY_CMD			0xFA	// 查询历史数据指令
#define CAN_TYPE_UDT_RESP_CMD			0xFB	// 响应历史数据指令



#define DEV_TYPE_IOT
// #define DEV_TYPE_ARM
// #define DEV_TYPE_PACK
#define DEV_TYPE_PC

#ifdef DEV_TYPE_ARM
int udt_transfer_to_iot(uint8_t addr, uint8_t *data, int size);
int udt_transfer_to_pack(uint8_t addr, uint8_t *data, int size);
#endif

#ifdef DEV_TYPE_PACK
int udt_transfer_to_arm(uint8_t addr, uint8_t *data, int size);
#endif

#ifdef DEV_TYPE_IOT
int udt_transfer_to_cloud(uint8_t addr, uint8_t *data, int size);
int udt_transfer_to_inv_pack(uint8_t addr, uint8_t *data, int size);
int udt_transfer_to_ble(uint8_t md_addr, uint8_t *data, int size);
int udt_transfer_to_IotMaster(uint8_t md_addr, uint8_t *data, int size);
#endif

#ifdef DEV_TYPE_PC
int udt_transfer_to_pc(uint8_t addr, uint8_t *data, int size);
#endif

int udt_transfer_to_hmi(uint8_t addr, uint8_t *data, int size);
uint8_t udt_mqtt_Report_to_Cloud(void);
uint8_t udt_ble_Report_to_app(void);
void udt_queue_init(void);

#endif
