#ifndef __DATA_SUMMARY_H__
#define __DATA_SUMMARY_H__

#include <stdint.h>
#include "iot_period_task.h"


/**
 * @brief 交流耦合电表状态定义
 */
typedef enum {
	AC_METER_STA_ERR = 1,
	AC_METER_STA_NORMAL,
} ac_meter_status_t;

#define is_ACLoadPowerUpdate() (((is_invload_16_poll())&&(is_invdata_17_poll())&&(is_achub_42_poll())))
#define ACLOAD_UPDATE_MASK (CAN_ACK_AC_HUB_INFO_42H|CAN_ACK_INV_LOAD_16H|CAN_ACK_INV_DATA_17H)

/**
 * @brief 交流耦合电表数据结构定义
 */
// typedef power_meter_eu_data_t ac_couple_meter_data_t;
typedef struct
{
    uint16_t ID;//0
    uint16_t Reserve[7];
    uint16_t PhaseVoltageAN;//8
    uint16_t PhaseVoltageBN;//8
    uint16_t PhaseVoltageCN;//8
    uint16_t Reserve1[4];//M129 6
    int16_t VoltageScaleFactor;//15
    uint16_t Frequency;//16
    int16_t FrequencyScaleFactor;//17
    uint16_t Reserve2;//18
    int16_t WphA;//19
    int16_t WphB;//20
    int16_t WphC;//21
    int16_t RealPowerScaleFactor;//22
} ac_couple_meter_data_t;


void data_sum_inv(void);
void data_sum_inv_base_param(void);
void data_sum_pack(void);
void data_sum_pack_group(void);
void data_sum_ems(void);
 uint64_t data_min(uint32_t *buff, uint32_t len);//inline static

#endif


