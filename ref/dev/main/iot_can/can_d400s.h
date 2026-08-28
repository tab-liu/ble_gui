#ifndef CAN_D400S_H
#define CAN_D400S_H


#include <stdint.h>
#include <string.h>

#include "can_data.h"
#include "can_control.h"
#include "comm_define.h"

#include "uart_device_process.h"

//extern EXT_RAM_BSS_ATTR MOD_STRUCT_Pack Inv_Pack[PACK_MAX_NUM*DEV_MAIN_NODE_MAX];



void d400s_infos_summary(MOD_STRUCT_Inv_READ *invCan);
void d400s_fault_check(d400s_can_node_struct *d400s_node,uint8_t index);
void d400s_setting_summary(MOD_STRUCT_Inv_READ *invCan);
void d400s_ota_check(MOD_STRUCT_Inv_READ *invCan);
#endif
