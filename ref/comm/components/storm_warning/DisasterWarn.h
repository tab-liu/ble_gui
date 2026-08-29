
#ifndef _DISASTER_WARN_H_
#define _DISASTER_WARN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iot_define.h>

#pragma pack(1) 

typedef struct 
{
    uint8_t StormWatchEnable;
    uint8_t StormWatchState;//1
    uint8_t StormWatchCancel;//1
    uint8_t StormWatchStateSingle;//1
    disaster_send_param_t StormSendParam;
}_StormWatchSetData;

#pragma pack()

// extern uint8_t power_on_flag;

extern _StormWatchSetData StormWatchSetData; //实时状态，掉电丢失

// void set_power_flag(void);
uint8_t GetStormWatchDataState(void);
void SetStormWatchEnableRegisterData(uint8_t IotSetData);
void SetAppCancelEventData(uint8_t IotSetData);
void SetStormWebData(void* IotSetData, int32_t Offset, char *id);
// void SaveWebStormWatchData(void);
// void GetWebStormData(void);

// void GetAppCancelEventData(void);
void DisasterJudge(void);
uint16_t GetStormWatchTargetSoc(void);
// void InitStormWatchTimer(void);
#endif




