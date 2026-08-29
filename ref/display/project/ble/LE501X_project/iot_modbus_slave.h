#ifndef IOT_MODBUS_SLAVE_H
#define IOT_MODBUS_SLAVE_H

#include "stdint.h"
#include "string.h"

// 添加这个编译指示，允许匿名结构体和联合体
#pragma anon_unions

// #define BMS_PACK_MAX_NUMBER     10

#define ADDR 0
#define FCN 1
#define REGHI 2
#define REGLO 3
#define NUMREGSHI 4
#define NUMREGSLO 5
#define OUTBYTES 2

#define FCN_NOT_SUPPORTED 1 // a Modbus function code we can't handle
#define BAD_ADDR_OR_CMD 2 // a Modbus "register" (command or address to us) we don't know
#define BAD_COUNT 3 // num regs != num bytes
#define CMD_NOT_COMPLETE 4 // command didn't complete successfully
#define ACK_MASTER_WAIT 5 // slave received command but need handle long time

#define DATA_REG_START_ADDRESS 0
#define ADV_DATA_REG_START_ADDRESS 70
#define HISTORY_REG_START_ADDRESS 2000
#define SET_REG_START_ADDRESS 3000
#define IAP_REG_ADDRESS 1080
#define IOT_REG_ADDRESS 5000
#define IOT_REG_WRITE_ADDRESS 5017
#define IOT_FACTORY_ADDRESS 9000

// #define MODBUS_REG_ADDRESS_06000 6000 // MODBUS CMD choose updata device
// #define MODBUS_REG_ADDRESS_15500 15500 // MODBUS CMD choose updata device
// #define MODBUS_REG_ADDRESS_15600 15600 // MODBUS CMD choose updata device

// MODBUS CMD choose updata device

#define DEVICE_TYPE_SRTING_LENGTH 12

typedef struct {
    uint16_t BMS_RunState;
    uint16_t BMS_SOC;
    uint16_t BMS_SOH;
    uint16_t BMS_LimitChgCurrent;
    uint16_t BMS_LimitChgVoltage;
    uint16_t BMS_LimitDsgCurrent;
    uint16_t SingleVol[16];
} BMSDATA;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t InvOverLoad : 1; // 逆变器过载	0
        uint16_t InvOverTemp : 1; // 逆变器过热	1
        uint16_t BatVoltHigh : 1; // 电池电压过高	2
        uint16_t BatVoltLow : 1; // 电池电压过低	3
        uint16_t InvOutputFault : 1; // 逆变输出故障	4
        uint16_t BmsCommFault : 1; // 电池组通讯故障	5
        uint16_t VoltCTFault : 1; // 电压传感器失效	6
        uint16_t CurrCTFault : 1; // 电流采样传感器失效	7
        uint16_t TransmitOverTemp : 1; // 变压器温度过高	8
        uint16_t DeviceOverTemp : 1; // 控制器温度过高	9
        uint16_t EnvironmentOverTemp : 1; // 环境温度过高	10
        uint16_t BatPackFault : 1; // 电池组故障	11
        uint16_t BatPackCommFail : 1; // 电池组通信失败	12
        uint16_t FanFault : 1; // 风扇故障	13
        uint16_t MainRlyFault : 1; // 主继电器失效	14
        uint16_t GridRlyFault : 1; // 电网继电器失效	15
    } BIT;
} Fault1;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t ChargerOverLoad : 1; // 充电器过载	0
        uint16_t ChargerFault : 1; // 充电器故障	1
        uint16_t ChargerFault1 : 1; // 第二充电器故障	2
        uint16_t MasterParamFail : 1; // 多主机错误	3
        uint16_t PhaseLoss : 1; // 缺相	4
        uint16_t ParallelCommFail : 1; // 多机通信异常	5
        uint16_t ParallelSynSignLoss : 1; // 多机同步异常	6
        uint16_t ParallelParamFail : 1; // 多机配置异常	7
        uint16_t DC13VoltHigh : 1; // DC输出过压	8
        uint16_t DC13CurrHigh : 1; // DC输出过流	9
        uint16_t DC13SoftStartFail : 1; // DC输出故障	10
        uint16_t AdcAdjZeroFail : 1; // 零点采样异常	11
        uint16_t GFCIFail : 1; // 漏电流检测失效	12
        uint16_t IsoFail : 1; // 绝缘检测失效	13
        uint16_t res14 : 1; // 保留	14
        uint16_t res15 : 1; // 保留	15
    } BIT;
} Fault2;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t ChargerOverTemp : 1; // 充电过温保护	0
        uint16_t DischargerOverTemp : 1; // 放电过温保护	1
        uint16_t ChargeLowTemp : 1; // 充电低温保护	2
        uint16_t DischargeLowTemp : 1; // 放电低温保护	3
        uint16_t PackOverVolt : 1; // 总电池过压保护	4
        uint16_t PackUnderVolt : 1; // 总电池欠压保护	5
        uint16_t CellOverVolt : 1; // 单体过压保护	6
        uint16_t CellUnderVolt : 1; // 单体欠压保护	7
        uint16_t OverCurr1 : 1; // 过流保护1	8
        uint16_t OverCurr2 : 1; // 过流保护2	9
        uint16_t OverCurr3 : 1; // 过流保护3	10
        uint16_t OverCurr4 : 1; // 过流保护4	11
        uint16_t PreChargeFail : 1; // 预充故障	12
        uint16_t BmsShortOut : 1; // 短路保护	13
        uint16_t res14 : 1; // 保留	14
        uint16_t res15 : 1; // 保留	15
    } BIT;
} Fault3;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t res0 : 8; //
        uint16_t res1 : 8; //
    } BIT;
} Fault4;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t GridVoltHigh : 1; // 电网电压高
        uint16_t GridVoltLow : 1; // 电网电压低
        uint16_t GridFreqHigh : 1; // 电网频率高
        uint16_t GridFreqLow : 1; // 电网频率低
        uint16_t GridSwing : 1; // 电网振荡	4
        uint16_t MeterCommFail : 1; // 电表通信错误	5
        uint16_t PvVoltHigh : 1; // PV过压
        uint16_t PvVoltLow : 1; // PV欠压
        uint16_t GenVoltLoss : 1; // 油机未接
        uint16_t res0 : 7; //
    } BIT;
} Warnning1;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t res0 : 8; //
        uint16_t res1 : 8; //
    } BIT;
} Warnning2;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t res0 : 8; //
        uint16_t res1 : 8; //
    } BIT;
} Warnning3;

typedef union {
    uint16_t WORD;
    struct {
        uint16_t res0 : 8; //
        uint16_t res1 : 8; //
    } BIT;
} Warnning4;

typedef struct {
    uint16_t YearMonth;
    uint16_t DayHour;
    uint16_t MinSec;
    uint16_t FaultNumber;
    uint16_t FaultCode;
} HistoryStruct;

typedef struct {
    uint16_t SysRsvd[10]; // 0-9
    uint16_t DeviceType[DEVICE_TYPE_SRTING_LENGTH / 2]; // 10-15
    uint16_t ComProtocolVersion; // 16      0.1
    uint16_t DeviceSNCode[4]; // 17-20
    uint16_t resvd0; // 21
    uint16_t MCUBusy; // 22
    uint16_t MCU1SoftWareL; // 23
    uint16_t MCU1SoftWareH; // 24
    uint16_t MCU2SoftWareL; // 25
    uint16_t MCU2SoftWareH; // 26
    uint16_t MCU3SoftWareL; // 27
    uint16_t MCU3SoftWareH; // 28
    uint16_t MCU4SoftWareL; // 29
    uint16_t MCU4SoftWareH; // 30
    uint16_t resvd1; // 31
    uint16_t resvd3; // 32
    uint16_t resvd4; // 33
    uint16_t resvd5; // 34
    uint16_t resvd6; // 35
} StaticStruct;

typedef struct {
    //-----------Base Data------------
    uint16_t PVToBatPower; // 36     1w
    uint16_t GirdToBatPower; // 37   1w
    uint16_t ACLoadPower; // 38      1w
    uint16_t DCLoadPower; // 39      1w
    uint16_t FeedBackPower; // 40    1w
    uint16_t PVTotalGeneraedPowerL; // 41  0.1kw/h
    uint16_t PVTotalGeneraedPowerH; // 42
    uint16_t BatSOC; // 43
    uint16_t PVIcon; // 44
    uint16_t GirdIcon; // 45
    uint16_t PVToBatLine; // 46
    uint16_t GirdToBatLine; // 47
    uint16_t BatToACLine; // 48
    uint16_t BatToDCLine; // 49
    uint16_t BatToGridLine; // 50
    uint16_t GridToLoadLine; // 51
    uint16_t PVToGridLine; // 52
    uint16_t BatDisChargeState; // 53 0:Idle, 1:charging, 2:discharging
    Warnning1 SysWarnning1; // 54
    Warnning2 SysWarnning2; // 55
    Warnning3 SysWarnning3; // 56
    Warnning4 SysWarnning4; // 57
    Fault1 SysFault1; // 58
    Fault2 SysFault2; // 59
    Fault3 SysFault3; // 60
    Fault4 SysFault4; // 61
    uint16_t UpdatePercent; // 62  (0-100)
    uint16_t resvd2[7]; // 63 - 69
} BasicStruct;

typedef struct {
    //-----------Advanced data------------
    uint16_t InvWorkMode; // 70    (0-5)
    uint16_t InvVoltage; // 71      1v
    uint16_t InvCurrent; // 72      0.1A
    uint16_t InvPower; // 73        1w
    uint16_t InvFreq; // 74         0.1Hz
    uint16_t LoadCurrent; // 75     0.1A
    uint16_t LoadPower; // 76       1w
    uint16_t GridVoltage; // 77     1v
    uint16_t GridCurrent; // 78     0.1A
    uint16_t GridPower; // 79       1w
    uint16_t GridFreq; // 80        0.1Hz
    uint16_t GridChargeState; // 81  0:Idle, 1:charging, 2:discharging
    uint16_t GeneratorVoltage; // 82  1v
    uint16_t DC13OutPutVoltage; // 83 0.1v
    uint16_t DC13TotalCurrent; // 84  0.1A
    uint16_t DC13TotalPower; // 85    1w

    uint16_t PVVoltage; // 86      1v
    uint16_t PVPower; // 87        1w
    uint16_t PVCurrent; // 88       0.1A
    uint16_t PVTotalGeneraedPowerLi; // 89  0.1kw/h
    uint16_t PVTotalGeneraedPowerHi; // 90
    uint16_t BMSTotalNumber; // 91
    uint16_t BatTotalVoltage; // 92   0.01V
    uint16_t BatTotalCurrent; // 93   0.1A
    uint16_t BatTotalSoc; // 94       1
    uint16_t BatTemp; // 95
    uint16_t BatCurrentIndex; // 96
    uint16_t BatRunStatus; // 97   0:init 1:normal 2:stop
    uint16_t BatVoltage; // 98
    uint16_t BatSoc; // 99
    uint16_t BatChargingStatus; // 100  0:IDLE, 1:Charging, 2:Discharging
    uint16_t BatSoh; // 101
    uint16_t BatChargeLimitCurrent; // 102  0.1A
    uint16_t BatChargeLimitVoltage; // 103  0.1V
    uint16_t BatDischgLimitCurrent; // 104  0.1A
    uint16_t SingleVoltage[16]; // 105 - 120    0.01V
    uint16_t Temp1; // 121      1
    uint16_t Temp2; // 122
    uint16_t Temp3; // 123
    uint16_t InvTotalPowerL; // 124     0.1kwh
    uint16_t InvTotalPowerH; // 125
    uint16_t FeedBackTotalPowerL; // 126  0.1kwh
    uint16_t FeedBackTotalPowerH; // 127
    uint16_t LoadTotalPowerL; // 128      0.1kwh
    uint16_t LoadTotalPowerH; // 129

    uint16_t L1InvWorkStatus; // 130
    uint16_t L1InvOutPutVoltage; // 131  1v
    uint16_t L1InvOutPutCurrent; // 132  0.1A
    uint16_t L1InvOutPutPower; // 133    1w
    uint16_t L1LoadCurrent; // 134       0.1A
    uint16_t L1LoadPower; // 135        1w
    uint16_t L1GridVoltage; // 136       1v
    uint16_t L1GridCurrent; // 137       0.1A
    uint16_t L1GridPower; // 138         1w

    uint16_t L2InvWorkStatus; // 139
    uint16_t L2InvOutPutVoltage; // 140 1v
    uint16_t L2InvOutPutCurrent; // 141 0.1A
    uint16_t L2InvOutPutPower; // 142   1w
    uint16_t L2LoadCurrent; // 143      0.1A
    uint16_t L2LoadPower; // 144        1w
    uint16_t L2GridVoltage; // 145      1v
    uint16_t L2GridCurrent; // 146      0.1A
    uint16_t L2GridPower; // 147        1w

    uint16_t L3InvWorkStatus; // 148
    uint16_t L3InvOutPutVoltage; // 149 1v
    uint16_t L3InvOutPutCurrent; // 150 0.1A
    uint16_t L3InvOutPutPower; // 151   1w
    uint16_t L3LoadCurrent; // 152      0.1A
    uint16_t L3LoadPower; // 153        1w
    uint16_t L3GridVoltage; // 154      1v
    uint16_t L3GridCurrent; // 155      0.1A
    uint16_t L3GridPower; // 156        1w

    uint16_t PVOutPutVoltage; // 157    0.1v
    uint16_t PVChargingStatus; // 158
    uint16_t PVTemp; // 159
    uint16_t PVChargingTotalNum; // 160

    uint16_t CG1ChargerStatus; // 161
    uint16_t CG1PVInputVoltage; // 162         1v
    uint16_t CG1PVChargerOutputVoltage; // 163 0.1v
    uint16_t CG1PVChargerOutputCurrent; // 164 0.1A
    uint16_t CG1PVChargerOutputPower; // 165   1w
    uint16_t CG1Alarm; // 166
    uint16_t CG1Fault; // 167

    uint16_t CG2ChargerStatus; // 168
    uint16_t CG2PVInputVoltage; // 169          1v
    uint16_t CG2PVChargerOutputVoltage; // 170  0.1v
    uint16_t CG2PVChargerOutputCurrent; // 171  0.1A
    uint16_t CG2PVChargerOutputPower; // 172   1w
    uint16_t CG2Alarm; // 173
    uint16_t CG2Fault; // 174

    uint16_t CG3ChargerStatus; // 175
    uint16_t CG3PVInputVoltage; // 176          1v
    uint16_t CG3PVChargerOutputVoltage; // 177  0.1v
    uint16_t CG3PVChargerOutputCurrent; // 178  0.1A
    uint16_t CG3PVChargerOutputPower; // 179    1w
    uint16_t CG3Alarm; // 180
    uint16_t CG3Fault; // 181

    uint16_t CG4ChargerStatus; // 182
    uint16_t CG4PVInputVoltage; // 183          1v
    uint16_t CG4PVChargerOutputVoltage; // 184  0.1v
    uint16_t CG4PVChargerOutputCurrent; // 185  0.1A
    uint16_t CG4PVChargerOutputPower; // 186    1w
    uint16_t CG4Alarm; // 187
    uint16_t CG4Fault; // 188
    uint16_t BleEnable; // 189
    uint16_t WifiEnable; // 190
    uint16_t TotalPowerLi; // 191
    uint16_t TotalPowerHi; // 192
    uint16_t TotalChgCurrentLimit; // 193
    uint16_t TotalChgVoltageLimit; // 194
    uint16_t TotalDsgCurrentLimit; // 195
    uint16_t SlaveMaxSingleVoltage; // 196
    uint16_t SlaveMinSingleVoltage; // 197
    uint16_t SlaveBatteryAlarm; // 198
    uint16_t SlaveBatteryMosState; // 199
    uint16_t SlaveBatteryTemp; // 200
    uint16_t SlaveBatterySoftWare1L; // 201
    uint16_t SlaveBatterySoftWare1H; // 202
    uint16_t SlaveBatterySoftWare2L; // 203
    uint16_t SlaveBatterySoftWare2H; // 204
    uint16_t CurrentSubPackOnline; // 205
    uint16_t res2; // 206
    uint16_t res3; // 207
    uint16_t res4; // 208
    uint16_t res5; // 209
} AdvancedStruct;

typedef struct // 2000
{
    uint16_t TotalHistoryPage;
    uint16_t CurrentHistoryPage;
    HistoryStruct Log[5];
} HistoryLogStruct;

typedef struct {
    uint16_t MainSwitch; // 3000
    uint16_t SystemWorkMode; // 3001
    uint16_t FirmwareUpdate; // 3002
    uint16_t Frequency; // 3003
    uint16_t DeviceMode; // 3004
    uint16_t DeviceAddress; // 3005
    uint16_t BatIndex; // 3006
    uint16_t InvSwicth; // 3007
    uint16_t DCSwitch; // 3008
    uint16_t PVSwitch; // 3009
    uint16_t FeedBackSwitch; // 3010
    uint16_t GridChargingSwitch; // 3011
    uint16_t MeterEnableSwitch; // 3012
    uint16_t TimeControlSwitch; // 3013
    uint16_t PVLimitCurrent; // 3014
    uint16_t DeviceLowPowerValue; // 3015
    uint16_t DeviceHighPowerValue; // 3016
    uint16_t DeviceChargingLimitCurrent; // 3017
    uint16_t DeviceDischargingLimitCurrent; // 3018
    uint16_t GridChargingLimitCurrent; // 3019
    uint16_t GridDischargingLimitCurrent; // 3020
    uint16_t ClearHistoryFaultLog; // 3021
    uint16_t ClearPowerData; // 3022

    uint16_t ChgStartTimeHour1; // 3023
    uint16_t ChgStartTimeMin1; // 3024
    uint16_t ChgEndChgTimeHour1; // 3025
    uint16_t ChgEndChgTimeMin1; // 3026

    uint16_t DsgStartTimeHour1; // 3027
    uint16_t DsgStartTimeMin1; // 3028
    uint16_t DsgEndTimeHour1; // 3029
    uint16_t DsgEndTimeMin1; // 3030

    uint16_t DeviceTime1; // 3031
    uint16_t DeviceTime2; // 3032
    uint16_t DeviceTime3; // 3033
    uint16_t DeviceLedState; // 3034
    uint16_t UPSMode; // 3035
    uint16_t BleConnectStatus; // 3036
    uint16_t WifiLoackConnectStatus; // 3037
    uint16_t WifiRemoteConnectStatus; // 3038

    uint16_t TimeLabel1; // 3039
    uint16_t StartTime1; // 3040
    uint16_t EndTime1; // 3041

    uint16_t TimeLabel2; // 3042
    uint16_t StartTime2; // 3043
    uint16_t EndTime2; // 3044

    uint16_t TimeLabel3; // 3045
    uint16_t StartTime3; // 3046
    uint16_t EndTime3; // 3047
    uint16_t TimeLabel4; // 3048
    uint16_t StartTime4; // 3049
    uint16_t EndTime4; // 3050
    uint16_t TimeLabel5; // 3051
    uint16_t StartTime5; // 3052
    uint16_t EndTime5; // 3053

    uint16_t TimeLabel6; // 3054
    uint16_t StartTime6; // 3055
    uint16_t EndTime6; // 3056

    uint16_t SetChgMaxPower; // 3057
    uint16_t SetDsgMaxPower; // 3058
    uint16_t HistoryNextPage; // 3059
    uint16_t PowerOff; // 3060
    uint16_t LcdDisplayTime; // 3061
    uint16_t setSsytemFactoryReset; // 3062
    uint16_t setECOEnable; // 3063
    uint16_t setECOTime; /*3064*/
    uint16_t setChargerMode; /*3065*/
    uint16_t setPowerOutput; /*3066*/
    uint16_t setAC_ECOEnable; /*3067*/
    uint16_t setAC_ECOTime; /*3068*/
    uint16_t setDC_ECOPower; /*3069*/
    uint16_t setAC_ECOPower; /*3070*/
    uint16_t setBuzzer; /*3071*/
    uint16_t setWIFISwitch; /*3072*/
    uint16_t setBlueToothSwitch; /*3073*/
    uint16_t setPvParllerEnable; /*3074*/
    uint16_t setPv1Type; /*3075*/
    uint16_t setPv2Type; /*3076*/
    uint16_t setPv3Type; /*3077*/
    uint16_t setPv4Type; /*3078*/
    uint16_t AC_Output_Voltage; /*3079*/
    uint16_t Deep_Discharge; /*3080*/
    uint16_t Splite_Phase_Max_Output_Power; /*3081*/
    uint16_t reserve; /*3082*/
} SetInfoStruct;

typedef struct {
    uint16_t InternetStatus; // 5000
    uint16_t MqttLinkStatus; // 5001
    int16_t DeviceUpdatePrecent; // 5002
    int16_t DeviceUpdateType; // 5003
    uint16_t WirelessIP12; // 5004
    uint16_t WirelessIP34; // 5005
    uint16_t WirelessMac12; // 5006
    uint16_t WirelessMac34; // 5007
    uint16_t WirelessMac56; // 5008
    uint16_t DeviceSecurityCode[4]; // 5009 - 5012
    uint16_t IotSN[4]; // 5013 - 5016
    uint16_t InternetName[16]; // 5017 - 5032
    uint16_t InternetPassword[16]; // 5033 - 5048
    uint16_t WifiAPSignal; // 5059
    uint16_t MqttReset; // 5050
    uint16_t IotType[6]; // 5051 - 5056
} IotInfoStruct;

typedef struct {
    uint16_t DeviceType; // 1080
    uint16_t uwBaudRate; // 1081
    uint16_t DeviceMainAddress; // 1082
    uint16_t DeviceSubAddress; // 1083
    uint16_t uUpdateFileSizeLo; // 1084
    uint16_t resvd2[10]; //
} UpdateStruct;

typedef struct {
    uint16_t TestResult; // 9000
    uint16_t FactoryMode; // 9001
    uint16_t IotSn[4]; // 9002 - 9005
    uint16_t SafetyCode[4]; // 9006 - 9009
} FactoryInfoStruct;

// 添加结构体对齐指令，确保结构体成员按2字节对齐
// #pragma pack(2)

// typedef union {
//     uint16_t all;
//     struct {
//         uint8_t chg_enable : 1; // 1：允许充电；0：禁止
//         uint8_t dsg_enable : 1; // 1：允许放电；0：禁止
//         uint8_t chg_now : 1; // 1：紧急补电
//         uint8_t chg_protect : 1; // 1：充电进入保护
//         uint8_t dgs_protect : 1; // 1：放电进入保护
//         uint8_t fault : 1; // 1：系统故障
//         uint8_t battery_heat : 1; // 1：电池加热
//         uint8_t battery_balance : 1; // 1：电池均衡
//     } bit;
// } status1_byte1;

// typedef union {
//     uint16_t all;
//     struct {
//         uint8_t chg_alarm : 1; // 1：充电告警；0：消除
//         uint8_t dsg_alarm : 1; // 1：放电告警；0：消除
//     } bit;
// } status1_byte2;

// typedef struct {
//     uint16_t is_high_volt; // 6000 所属电压平台
//     uint16_t pack_cnt; // 6001 pack并机数量
//     uint16_t online; // 6002 响应的位置1表示在线 -通讯并机，bit0~15依次表示不同bat状态
//     uint16_t total_voltage; // 6003 0.1V  总电压
//     uint16_t total_current; // 6004 0.1A  总电流
//     uint16_t soc; // 6005 1% 系统电流
//     uint16_t soh; // 6006 0-100 1%
//     uint16_t avg_temp; // 6007 平均温度
//     uint16_t work_status; // 6008工作状态
//     uint16_t chg_status; // 6009 充电状态
//     uint16_t max_chg_voltage; // 6010最大充电电压
//     uint16_t max_chg_current; // 6011 为0时，不能充电
//     uint16_t max_dsg_current; // 6012 为0时，不能放电
//     status1_byte1 status1; // 6013 状态标记位1
//     status1_byte2 status2; // 6014 状态标记位2
//     uint16_t revd1; // 6015
//     uint16_t cap_online; // 6016最大并机16台，响应的位置1表示，主mos输出打开-容量并机
//     uint16_t chg_full_time; // 6017预计放空时间
//     uint16_t dsg_empty_time; // 6018 预计充满时间
//     uint32_t total_chg_energy; // 6019~6020 修正拼写错误：uint32_ttotal_chg_energy -> uint32_t
//                                // total_chg_energy
//     uint32_t total_dsg_energy; // 6021~6022
//     uint16_t TotalCurrent_bias; // 6023
//     uint16_t revd3; // 6024
//     uint16_t all_pack_control; // 6025
//     uint16_t buckle_status; // 6026
//     uint16_t merge_status; // 6027
//     uint16_t heat_status; // 6028
//     uint16_t all_pack_alarm1; // 6029
//     uint16_t all_pack_alarm2; // 6030
//     uint16_t BCU_State; // 6031
//     uint16_t revd4; // 6032
//     uint16_t BcuBalanceSumPwr; // 6033
// } MOD_STRUCT_reg06000;

// typedef struct {
//     union {
//         uint16_t all;
//         struct {
//             uint8_t CHARGER_direction : 2; // bit0: 车流向CHARGER 1(DC1)  bit1: CHARGER
//                                            // 1流向储能产品(DC3)
//             uint8_t DC1_EN : 2; // 0-无；1-进入DCDC模块；2-从DCDC模块输出
//             uint8_t DC2_EN : 2;
//             uint8_t DC3_EN : 2;
//             uint8_t DC4_EN : 2;
//             uint8_t DC5_EN : 2;
//             uint8_t DC6_EN : 2;
//             uint8_t revd : 2; // 预留
//         };
//     };
// } line_sate_struct; // DCDC模块通用 信息区

// typedef struct {
//     union {
//         uint16_t all;
//         struct {
//             uint8_t DCDC_work_state : 1;
//             uint8_t DCDC_module_fault : 1;
//             uint8_t PV_online : 1;
//             uint8_t work_model : 4;
//             uint8_t res : 1;
//             uint8_t res1 : 8;
//         };
//     };
// } dev_info_struct;

// typedef struct {
//     uint16_t write_offset;
//     uint16_t write_remain_len;
//     uint16_t write_crc16;
//     uint8_t crc_valid;
//     uint8_t write_next_seq;
//     uint8_t devId; // CAN Rx IDćşĺ°ĺ,ĺ¨CanCmdUpdate()ĺĺ§ĺ?
//     uint8_t can_type; // can beta ,can type block,windy add
//     uint8_t *temp_buffer;
// #ifdef STATE_COUNT
//     uint32_t send_cnt;
//     uint32_t recv_cnt;
//     uint32_t error_cnt;
// #endif
// } rw_cmd_struct;

// typedef struct {
//     char dc_dev_type[12]; // 15500 DCDC设备型号
//     uint64_t dc_dev_sn; // 15506 DCDC设备SN
//     uint16_t dc_input_voltage; // 15510 DCDC输入电压 0.1V
//     uint16_t dc_output_voltage; // 15511 DCDC输出电压 0.1V
//     uint16_t dc_output_current; // 15512 DCDC输出电流 0.1A
//     uint16_t dc_output_power; // 15513 DCDC输出功率 1W
//     line_sate_struct line_sate; // 15514 能量流动条
//     uint16_t bat_type; // 15515 电池类型
//     uint16_t DC1_fault; // 15516 DC1故障
//     uint16_t DC2_fault; // 15517 DC2故障
//     uint16_t DC3_alarm; // 15518 DC3故障
//     uint16_t DC4_fault; // 15519 DC4故障
//     uint16_t DC5_fault; // 15520 DC5故障
//     uint16_t DC6_fault; // 15521 DC6故障
//     uint16_t res[4]; // 15522 - 15525 保留
//     dev_info_struct Dev_info; // 15526
//     int32_t TotalInputPower; // 15527 - 15528 输入功率
//     int32_t TotalOutputPower; // 15529 - 15530 输出功率
//     int16_t DC1Voltage; // 15531 DC1电压 0.1V
//     int32_t DC1Current; // 15532 - 15533 DC1电流 0.01A
//     int16_t DC1Power; // 15534 DC1功率 1W
//     int16_t DC2Voltage; // 15535 DC2电压 0.1V
//     int32_t DC2Current; // 15536 - 15537 DC2电流 0.01A
//     int16_t DC2Power; // 15538 DC2功率 1W
//     int16_t DC3Voltage; // 15539 DC3电压 0.1V
//     int32_t DC3Current; // 15540 - 15541 DC3电流 0.01A
//     int16_t DC3Power; // 15542 DC3功率 1W
//     int16_t DC4Voltage; // 15543 DC4电压 0.1V
//     int32_t DC4Current; // 15544 - 15545 DC4电流 0.01A
//     int16_t DC4Power; // 15546 DC4功率 1W
//     int16_t DC5Voltage; // 15547 DC5电压 0.1V
//     int32_t DC5Current; // 15548 - 15549 DC5电流 0.01A
//     int16_t DC5Power; // 15550 DC5功率 1W
//     int16_t DC6Voltage; // 15551 DC6电压 0.1V
//     int32_t DC6Current; // 15552 - 15553 DC6电流 0.01A
//     int16_t DC6Power; // 15554 DC6功率 1W
//     int32_t TotalInputEnergy; // 15555 DCDC输入总电量 0.1KWh
//     int32_t DC1EnergyInput; // 15557 DC1输入电量 0.1KWh
//     int32_t DC1EnergyOutput; // 15559 DC1输出电量 0.1KWh
//     int32_t DC2EnergyInput; // 15561 DC2输入电量 0.
//     int32_t DC2EnergyOutput; // 15563 DC2输出电量 0.1KWh
//     int32_t DC3EnergyInput; // 15565 DC3输入电量 0.1KWh
//     int32_t DC3EnergyOutput; // 15567 DC3输出电量 0.1KWh
//     int32_t DC4EnergyInput; // 15569 DC4输入总电量 0.1KWh
//     int32_t DC4EnergyOutput; // 15571 DC4输出总电量 0.1KWh
//     int32_t DC5EnergyInput; // 15573 DC5输入总电量 0.1KWh
//     int32_t DC5EnergyOutput; // 15575 DC5输出总电量 0.1KWh
//     int32_t DC6EnergyInput; // 15577 DC6输入总电量 0.1KWh
//     int32_t DC6EnergyOutput; // 15579 DC6输出总电量 0.1KWh
//     uint16_t dev_type; // 15581
//     uint32_t version; // 15582 - 15583
//     // rw_cmd_struct rw_cmd;
// } MOD_STRUCT_reg15500;

// typedef struct {
//     union {
//         uint16_t all;
//         struct {
//             uint8_t DC_sw : 2; // bit0: ON  bit1: OFF
//             uint8_t On_Sound_En : 2; // 0-无；1-进入DCDC模块；2-从DCDC模块输出
//             uint8_t FctRst_en : 2;
//             uint8_t Self_Adaption : 2;
//             uint8_t PV_model : 2; //
//             uint8_t revd : 6; // 预留
//         };
//     };
// } DCDC_set_struct; // DCDC模块通用

// typedef struct {
//     DCDC_set_struct DCDC_setings; // 15600
//     uint16_t DC1_Voltage_set; // 15601 DC1电压设置 0.1V
//     uint16_t DC1_Current_set; // 15602 DC1电流设置 0.1A
//     uint16_t DC2_Voltage_set; // 15603 DC2电压设置 0.1V
//     uint16_t DC2_Current_set; // 15604 DC2电流设置 0.1A
//     uint16_t DC3_Voltage_set; // 15605 DC3电压设置 0.1V
//     uint16_t DC3_Current_set; // 15606 DC3电流设置 0.1A
//     uint16_t DC4_Voltage_set; // 15607 DC4电压设置 0.1V
//     uint16_t DC4_Current_set; // 15608 DC4电流设置 0.1A
//     uint16_t DC5_Voltage_set; // 15609 DC5电压设置 0.1V
//     uint16_t DC5_Current_set; // 15610 DC5电流设置 0.1A
//     uint16_t DC6_Voltage_set; // 15611 DC6电压设置 0.1V
//     uint16_t DC6_Current_set; // 15612 DC6电流设置 0.1A
// } MOD_STRUCT_reg15600;

// #pragma pack() // 取消结构体对齐指令
typedef union // 0
{
    uint8_t ModbusData[sizeof(StaticStruct) + sizeof(BasicStruct) + sizeof(AdvancedStruct)];
    struct {
        //-----------Const Data------------
        StaticStruct StaticData;
        //-----------Base Data------------
        BasicStruct BasicData;
        //-----------Advanced data------------
        AdvancedStruct AdvancedData;
    } DataStruct;
} MODBUS_DATA_UNION;

typedef union // 2000
{
    uint8_t ModbusData[sizeof(HistoryLogStruct)];
    struct {
        HistoryLogStruct HistortData;
    } DataStruct;
} MODBUS_HISTORY_UNION;

typedef union // 3000
{
    uint8_t SetModbusData[sizeof(SetInfoStruct)];
    SetInfoStruct SetDataStruct;
} MODBUS_SET_UNION;

typedef union // 5000
{
    uint8_t ModbusData[sizeof(IotInfoStruct)];
    IotInfoStruct IotDataStruct;
} MODBUS_IOT_UNION;

typedef union // 1080
{
    uint8_t ModbusData[sizeof(UpdateStruct)]; // 1080
    UpdateStruct UpdateDataStruct;
} MODBUS_IAP_UNION;

typedef union // 9000
{
    uint8_t ModbusData[sizeof(FactoryInfoStruct)];
    FactoryInfoStruct FactoryInfo;
} MODBUS_FACTORY_UNION;

// typedef union // 6000
// {
//     uint8_t ModbusData[sizeof(MOD_STRUCT_reg06000)];
//     MOD_STRUCT_reg06000 Reg06000;
// } MODBUS_REG06000_UNION;

// typedef union // 15500
// {
//     uint8_t ModbusData[sizeof(MOD_STRUCT_reg15500)];
//     MOD_STRUCT_reg15500 Reg15500;
// } MODBUS_REG15500_UNION;

// typedef union // 15600
// {
//     uint8_t ModbusData[sizeof(MOD_STRUCT_reg15600)];
//     MOD_STRUCT_reg15600 Reg15600;
// } MODBUS_REG15600_UNION;

extern MODBUS_IOT_UNION gModbusIotRegs;
extern MODBUS_DATA_UNION gModbusDataRegs;
extern MODBUS_HISTORY_UNION gModbusHistoryRegs;
extern MODBUS_SET_UNION gModbusSetRegs;
extern MODBUS_FACTORY_UNION gModbusFactortSetRegs;
extern MODBUS_IAP_UNION gModbusUpdateRegs;
// extern MODBUS_REG06000_UNION gModbusReg06000;
// extern MODBUS_REG15500_UNION gModbusReg15500;
// extern MODBUS_REG15600_UNION gModbusReg15600;

void ModbusRegsInit(uint32_t systemVer);
int Modbus_Slave(uint8_t *cmdBuf, uint16_t cmdLen, uint8_t *ackBuf);
void ApplyMutex(void);
uint8_t GetMutex(void);
void FreeMutex(void);
uint8_t ModbusRespon_Check(const uint8_t *cmdBuf, uint16_t cmdLen);

#endif
