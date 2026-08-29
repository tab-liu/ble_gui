#ifndef __MODBUS_PROTOCOL_ALPHA_H__
#define __MODBUS_PROTOCOL_ALPHA_H__

#pragma pack(1)

typedef struct 
{     
	uint16_t reserve[6];//0-5
    uint16_t supportpassword; // 6
    uint8_t  password[6]; // 7-9
    char DeviceType[12];//10-15
    uint16_t MdVersion;//16 
    uint16_t DeviceSN[4];//17-20    
    uint16_t Agency;//21 
    uint16_t MCUBusy;//22
    uint16_t MCU1SoftWareL;//23
    uint16_t MCU1SoftWareH;//24
    uint16_t MCU2SoftWareL;//25
    uint16_t MCU2SoftWareH;//26
    uint16_t MCU3SoftWareL;//27
    uint16_t MCU3SoftWareH;//28
    uint16_t MCU4SoftWareL;//29
    uint16_t MCU4SoftWareH;//30
    uint16_t reserve1; // 31 
    uint16_t reserve2; // 32
    uint16_t reserve3; // 33
    uint16_t reserve4; // 34
    uint16_t iot_ver; // 35
    //-----------Base Data------------//
    uint16_t PVToBatPower;//36     1w
    uint16_t GirdToBatPower;//37   1w   
    uint16_t ACLoadPower;//38      1w
    uint16_t DCLoadPower;//39      1w
    uint16_t FeedBackPower;//40    1w
    uint16_t PVTotalGeneraedPowerL;//41  0.1kw/h
    uint16_t PVTotalGeneraedPowerH;//42  
    uint16_t BatSOC;//43              
    uint16_t PVIcon;//44
    uint16_t GirdIcon;//45
    uint16_t PVToBatLine;//46
    uint16_t GirdToBatLine;//47
    uint16_t BatToACLine;//48
    uint16_t BatToDCLine;//49
    uint16_t BatToGridLine;//50
    uint16_t GridToLoadLine;//51
    uint16_t PVToGridLine;//52
    uint16_t BatDisChargeState;//53 0:Idle, 1:charging, 2:discharging
    uint16_t SysWarnning1;//54 
    uint16_t SysWarnning2;//55
    uint16_t SysWarnning3;//56
    uint16_t SysWarnning4;//57
    uint16_t SysFault1;//58
    uint16_t SysFault2;//59
    uint16_t SysFault3;//60
    uint16_t SysFault4;//61
    uint16_t UpdatePercent;// 62  (0-100) 
    uint16_t resvd2[7];//63 - 69
    uint16_t InvWorkMode;//70    (0-5)
    uint16_t InvVoltage;//71      1v
    uint16_t InvCurrent;//72      0.1A
    uint16_t InvPower;//73        1w
    uint16_t InvFreq;//74         0.1Hz
    uint16_t LoadCurrent;//75     0.1A
    uint16_t LoadPower;//76       1w
    uint16_t GridVoltage;//77     1v
    uint16_t GridCurrent;//78     0.1A
    uint16_t GridPower;//79       1w
    uint16_t GridFreq;//80        0.1Hz
    uint16_t GridChargeState;//81  0:Idle, 1:charging, 2:discharging

	uint16_t GeneratorVoltage;//82  1v
    uint16_t DC13OutPutVoltage;//83 0.1v
    uint16_t DC13TotalCurrent;//84  0.1A
    uint16_t DC13TotalPower;//85    1w
    
    uint16_t PVVoltage;//86      1v
    uint16_t PVPower;//87        1w
    uint16_t PVCurrent;//88       0.1A
    uint16_t PVTotalGeneraedPowerLi;//89  0.1kw/h
    uint16_t PVTotalGeneraedPowerHi;//90  
    uint16_t BMSTotalNumber;//91 
    uint16_t BatTotalVoltage;//92   0.01V
    uint16_t BatTotalCurrent;//93   0.1A
    uint16_t BatTotalSoc;//94       1
    uint16_t BatTemp;//95             
    uint16_t BatCurrentIndex;//96 
    uint16_t BatRunStatus;//97   0:init 1:normal 2:stop
	uint16_t BatVoltage; //98
    uint16_t BatSoc;//99
    uint16_t BatChargingStatus;//100  0:IDLE, 1:Charging, 2:Discharging
    uint16_t BatSoh;//101
    uint16_t BatChargeLimitCurrent;//102  0.1A
    uint16_t BatChargeLimitVoltage;//103  0.1V
    uint16_t BatDischgLimitCurrent;//104  0.1A
    uint16_t SingleVoltage[16];//105 - 120    0.01V
    uint16_t Temp1;//121      1 
    uint16_t Temp2;//122
    uint16_t Temp3;//123
    uint16_t InvTotalPowerL;//124     0.1kwh
    uint16_t InvTotalPowerH;//125
    uint16_t FeedBackTotalPowerL;//126  0.1kwh
    uint16_t FeedBackTotalPowerH;//127
    uint16_t LoadTotalPowerL;//128      0.1kwh
    uint16_t LoadTotalPowerH;//129
    
    uint16_t L1InvWorkStatus;//130
    uint16_t L1InvOutPutVoltage;//131  1v
    uint16_t L1InvOutPutCurrent;//132  0.1A
    uint16_t L1InvOutPutPower;//133    1w
    uint16_t L1LoadCurrent;//134       0.1A
    uint16_t L1LoadPower;//135        1w
    uint16_t L1GridVoltage;//136       1v
    uint16_t L1GridCurrent;//137       0.1A
    uint16_t L1GridPower;//138         1w

    uint16_t L2InvWorkStatus;//139
    uint16_t L2InvOutPutVoltage;//140 1v
    uint16_t L2InvOutPutCurrent;//141 0.1A
    uint16_t L2InvOutPutPower;//142   1w
    uint16_t L2LoadCurrent;//143      0.1A
    uint16_t L2LoadPower;//144        1w
    uint16_t L2GridVoltage;//145      1v
    uint16_t L2GridCurrent;//146      0.1A
    uint16_t L2GridPower;//147        1w

    uint16_t L3InvWorkStatus;//148
    uint16_t L3InvOutPutVoltage;//149 1v
    uint16_t L3InvOutPutCurrent;//150 0.1A
    uint16_t L3InvOutPutPower;//151   1w
    uint16_t L3LoadCurrent;//152      0.1A
    uint16_t L3LoadPower;//153        1w
    uint16_t L3GridVoltage;//154      1v
    uint16_t L3GridCurrent;//155      0.1A
    uint16_t L3GridPower;//156        1w
    
    uint16_t PVOutPutVoltage;//157    0.1v
    uint16_t PVChargingStatus;//158
    uint16_t PVTemp;//159            
    uint16_t PVChargingTotalNum;//160

    uint16_t CG1ChargerStatus;//161
    uint16_t CG1PVInputVoltage;//162         1v
    uint16_t CG1PVChargerOutputVoltage;//163 0.1v
    uint16_t CG1PVChargerOutputCurrent;//164 0.1A
    uint16_t CG1PVChargerOutputPower;//165   1w
    uint16_t CG1Alarm;//166   
    uint16_t CG1Fault;//167

    uint16_t CG2ChargerStatus;//168
    uint16_t CG2PVInputVoltage;//169          1v
    uint16_t CG2PVChargerOutputVoltage;//170  0.1v
    uint16_t CG2PVChargerOutputCurrent;//171  0.1A
    uint16_t CG2PVChargerOutputPower;//172   1w
    uint16_t CG2Alarm;//173
    uint16_t CG2Fault;//174
    
    uint16_t CG3ChargerStatus;//175
    uint16_t CG3PVInputVoltage;//176          1v
    uint16_t CG3PVChargerOutputVoltage;//177  0.1v
    uint16_t CG3PVChargerOutputCurrent;//178  0.1A
    uint16_t CG3PVChargerOutputPower;//179    1w
    uint16_t CG3Alarm;//180
    uint16_t CG3Fault;//181            

    uint16_t CG4ChargerStatus;//182
    uint16_t CG4PVInputVoltage;//183          1v
    uint16_t CG4PVChargerOutputVoltage;//184  0.1v
    uint16_t CG4PVChargerOutputCurrent;//185  0.1A
    uint16_t CG4PVChargerOutputPower;//186    1w
    uint16_t CG4Alarm;//187
    uint16_t CG4Fault;//188
    uint16_t BleEnable;// 189
    uint16_t WifiEnable;//190
    
    uint16_t TotalPowerLi;//191
    uint16_t TotalPowerHi;//192
	uint16_t TotalChgCurrentLimit; //193
	uint16_t TotalChgVoltageLimit; //194
	uint16_t TotalDsgCurrentLimit; //195
	uint16_t SlaveMaxSingleVoltage; // 196
	uint16_t SlaveMinSingleVoltage; // 197
	uint16_t SlaveBatteryAlarm; //198
	uint16_t SlaveBatteryMosState; // 199
	uint16_t SlaveBatteryTemp; // 200
	uint16_t SlaveBatterySoftWare1L; // 201
	uint16_t SlaveBatterySoftWare1H; // 202
	uint16_t SlaveBatterySoftWare2L; // 203
	uint16_t SlaveBatterySoftWare2H; // 204
	uint16_t CurrentSubPackOnline; // 205
    // PackModel
    uint16_t PackModel1;  // 206
    uint16_t PackModel2;  // 207
    uint16_t PackModel3;  // 208
    uint16_t PackModel4;  // 209
    uint16_t PackModel5;  // 210
    uint16_t PackModel6;  // 211
    // PackSN
    uint16_t PackSN1;   // 212
    uint16_t PackSN2;   // 213
    uint16_t PackSN3;   // 214
    uint16_t PackSN4;   // 215

    uint16_t PackAlarm1; // 216 PACK 故障1
    uint16_t PackAlarm2; // 217 PACK 故障2
    uint16_t res4; // 218
    uint16_t res5; // 219
    uint16_t res6; // 220
    uint16_t res7; // 221
    uint16_t res8; // 222
    uint16_t res9; // 223
    uint16_t res10; // 224
    uint16_t res11; // 225
    uint16_t res12; // 226
    uint16_t res13; // 227
    uint16_t res14; // 228
    uint16_t res15; // 229
}md_alpha_reg0000_t;


typedef struct 
{
    uint16_t DeviceType; // 1080
    uint16_t BaudRate; // 1081  
    uint16_t MainAddress; // 1082  
    uint16_t SubAddress; // 1083  
    uint16_t FileSize; // 1084
}md_alpha_reg1080_t;


typedef struct
{
    uint16_t MainSwitch; //3000
    uint16_t SystemWorkMode;//3001
    uint16_t FirmwareUpdate;//3002 
    uint16_t Frequency;//3003
    uint16_t DeviceMode;//3004
    uint16_t DeviceAddress;//3005
    uint16_t BatIndex;//3006
    uint16_t InvSwicth;//3007
    uint16_t DCSwitch;//3008
    uint16_t PVSwitch;//3009
    uint16_t FeedBackSwitch;//3010
    uint16_t GridChargingSwitch;//3011
    uint16_t MeterEnableSwitch;//3012
    uint16_t TimeControlSwitch;//3013
    uint16_t PVLimitCurrent;//3014
    uint16_t DeviceLowPowerValue;//3015
    uint16_t DeviceHighPowerValue;//3016
    uint16_t DeviceChargingLimitCurrent;//3017
    uint16_t DeviceDischargingLimitCurrent;//3018
    uint16_t GridChargingLimitCurrent;//3019
    uint16_t GridDischargingLimitCurrent;//3020
    uint16_t ClearHistoryFaultLog;//3021
    uint16_t ClearPowerData;//3022
    
    uint16_t ChgStartTimeHour1;//3023
    uint16_t ChgStartTimeMin1;//3024
    uint16_t ChgEndChgTimeHour1;//3025
    uint16_t ChgEndChgTimeMin1;//3026
    
    uint16_t DsgStartTimeHour1;//3027
    uint16_t DsgStartTimeMin1;//3028
    uint16_t DsgEndTimeHour1;//3029
    uint16_t DsgEndTimeMin1;//3030

    uint16_t DeviceTime1;//3031
    uint16_t DeviceTime2;//3032
    uint16_t DeviceTime3;//3033 
    uint16_t DeviceLedState;//3034
    uint16_t UPSMode;//3035
    uint16_t BleConnectStatus;// 3036
    uint16_t WifiLoackConnectStatus;//3037 WiFi联网
    uint16_t WifiRemoteConnectStatus;//3038 WiFi连接服务器
    
    uint16_t TimeLabel1; //3039
    uint16_t StartTime1;//3040
    uint16_t EndTime1;  //3041
    
    uint16_t TimeLabel2; //3042
    uint16_t StartTime2;//3043
    uint16_t EndTime2;  //3044
    
    uint16_t TimeLabel3; //3045
    uint16_t StartTime3;//3046
    uint16_t EndTime3;  //3047
    uint16_t TimeLabel4; //3048
    uint16_t StartTime4;//3049
    uint16_t EndTime4;  //3050
    uint16_t TimeLabel5; //3051
    uint16_t StartTime5;//3052
    uint16_t EndTime5;  //3053
    
    uint16_t TimeLabel6; //3054
    uint16_t StartTime6;//3055
    uint16_t EndTime6;  //3056
    
    uint16_t SetChgMaxPower;//3057
    uint16_t SetDsgMaxPower;// 3058
    uint16_t HistoryNextPage;// 3059
    uint16_t PowerOff;// 3060
    uint16_t LcdDisplayTime;// 3061
    uint16_t setSsytemFactoryReset; //3062
    uint16_t setECOEnable; //3063
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
    uint16_t SetGridUVValue; /*3082*/
    uint16_t SetGridUVTime; /*3083*/
    uint16_t SetGridOVValue; /*3084*/
    uint16_t SetGridOVTime; /*3085*/
    uint16_t SetGridUFValue; /*3086*/
    uint16_t SetGridUFTime; /*3087*/
    uint16_t SetGridOF1Value; /*3088*/
    uint16_t SetGridOF1Time; /*3089*/
    uint16_t reserve; /*3090*/
}md_alpha_reg3000_t;

// WiFi
typedef struct
{
    uint16_t InternetStatus; // 5000
    uint16_t MqttLinkStatus; // 5001
    int16_t DeviceUpdatePrecent; // 5002
    int16_t DeviceUpdateType; // 5003
    uint16_t WirelessIP12;// 5004
    uint16_t WirelessIP34;// 5005
    uint16_t WirelessMac12;// 5006
    uint16_t WirelessMac34;// 5007
    uint16_t WirelessMac56;// 5008
    uint16_t DeviceSecurityCode[4];//5009 - 5012
    uint16_t IotSN[4]; //5013 - 5016
    uint16_t InternetName[16];// 5017 - 5032
    uint16_t InternetPassword[32]; // 5033 - 5048
    uint16_t WifiAPSignal;//5049
    uint16_t MqttReset;//5050
    uint16_t IotType[6]; // 5051 - 5056
    uint16_t Host[64]; 
    uint16_t auth;
}md_alpha_reg5000_t;

typedef struct
{
    uint16_t TestResult; // 9000
    uint16_t FactoryMode;// 9001 设置为1时表示需要标定
    uint16_t IotSn[4]; //9002 - 9005
    uint16_t SafetyCode[4]; // 9006 - 9009
}md_alpha_reg9000_t;



#define	ALPHA_MOD_REG_START_ADDR_00000		(uint16_t)0
#define	ALPHA_MOD_REG_LEN_00000				((uint16_t)sizeof(md_alpha_reg0000_t)>>1)

#define	ALPHA_MOD_REG_START_ADDR_03000	(uint16_t)3000
#define	ALPHA_MOD_REG_LEN_03000			((uint16_t)sizeof(md_alpha_reg3000_t)>>1)


#pragma pack()


#endif  // END __MODBUS_PROTOCOL_ALPHA_H__


