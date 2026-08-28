
#ifndef _DISASTER_WARN_H_
#define _DISASTER_WARN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iot_type.h>
#define MaxBackupCount              10
#pragma pack(1) 
typedef enum
{
    Immediate = 0,
    Expected,
    Future,
    Unknown1
}_URGENCY;//紧急度
typedef enum
{
    Extreme = 0,
    Severe,
    Moderate,
    Minor,
    Unknown2
}_SEVERITY;//严重度
typedef enum
{
    Observed = 0,
    Likely,
    Possible,
    Unlikely,
    Unknown3
}_CERTAINTY;//确信度

typedef union {
    uint8_t status;
    struct {
        uint8_t start       : 1; // 开始标志
        uint8_t end         : 1; // 结束标志
        uint8_t stop         : 2; // 中断标志
    }bit;
}send_flag_t;

typedef union{
    uint16_t all;
    struct {
        uint16_t recv_state                : 2;  
        uint16_t res                : 14;  
    }bit;
}push_picture_t;

typedef struct {
    uint8_t soc_start;
    uint8_t soc_end;
    uint32_t BackupStartTime;
    uint32_t BackupEndTime;
    send_flag_t  send_flag;
    uint32_t total_chg_energy_begin;
    uint32_t total_chg_energy_end;
    uint16_t uuid;
    char msg_id[32]; // 消息ID
} disaster_send_param_t;

typedef struct 
{
    uint8_t StormWatchEnable;
    uint8_t StormWatchState;//1
    uint8_t StormWatchCancel;//1
    uint8_t StormWatchStateSingle;//1
    disaster_send_param_t StormSendParam;
    push_picture_t picture;
}_StormWatchSetData;

typedef struct 
{
    uint32_t StartTime;
    uint32_t EndTime;
    _URGENCY Urgency;//紧急度
    _SEVERITY Severity;//严重度
    _CERTAINTY Certainty;//确信度
    uint16_t Event;//警报事件
    uint16_t SoC;//Severity对应目标电量
}_SaveWebStormWatchApiData;//API data
//风暴预警 时间观察存储信息

typedef struct
{
    uint16_t DataIdentificationCode;
    uint8_t EventEndFlag;
    uint16_t LastSetLowCapacity;//系统低电量设置值
    uint16_t LastSetHighCapacity;//系统高电量设置值
    uint16_t LastGridChgSwitch;//上一次的电网充电开关

    uint16_t CancelEvent[MaxBackupCount];
    uint16_t CancelEventRunTime[MaxBackupCount];
}_AppCancelStormEvent;


typedef union {
    uint16_t all;
    struct {
        uint16_t set_workmode_flag         : 1; // current_time >= receive_end_time
        uint16_t reserved        : 15;
    } bit;
} work_flag_t;

typedef struct {
    _SaveWebStormWatchApiData SaveWebStormWatchApiData;					
    uint8_t LastWorkMode;				
    _AppCancelStormEvent AppCancelStormEvent;
    disaster_send_param_t disaster_send_param;
    work_flag_t work_flag;
    // uint8_t disaster_send_param.soc_start;
    // uint8_t disaster_send_param.soc_end;
    // uint32_t disaster_send_param.BackupStartTime;
    // uint32_t disaster_send_param.BackupEndTime;
    uint8_t nc[4];
} disaster_save_param_t;

typedef enum {
    STORM_STATE_INACTIVE = 0,   // 未激活
    STORM_STATE_ACTIVE  = 1,    // 激活中
    STORM_STATE_ENDED   = 2     // 已结束
} storm_state_t;

typedef enum {
    CHARGE_STATE_INACTIVE = 0,
    CHARGE_STATE_ACTIVE   = 1
} charge_state_t;

#define MAX_RETRY_COUNT  5

#pragma pack()

extern uint8_t power_on_flag;
extern disaster_save_param_t DisasterSaveData;
extern _StormWatchSetData StormWatchSetData; //实时状态，掉电丢失

void set_power_flag(void);
uint8_t GetStormWatchDataState(void);
uint8_t GetStormWatchEnableRegisterData(void);
void SetStormWatchEnableRegisterData(uint8_t SetData);
uint8_t  GetAppCancelEventEndFlag(void);
void SetAppCancelEventData(uint8_t SetData);
//void SetStormWebData(void* SetData);
void SetStormWebData(void* SetData, char *id);
void SaveWebStormWatchData(void);
void GetWebStormData(void);
void SaveAppCancelEventData(void);
void GetAppCancelEventData(void);
void DisasterJudge(void);
uint16_t GetStormWatchTargetSoc(void);
uint8_t GetStormPicture1State(void);
// void InitStormWatchTimer(void);
#endif




