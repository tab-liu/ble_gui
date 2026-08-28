#ifndef __EMS_CTRL_H__
#define __EMS_CTRL_H__












#define MAXPARLLELNUM    (uint16_t)3
#define MAXINVPHASE      (uint16_t)3
#define TimePeriod_MAX_NUM      6//定义使用时间段

#pragma pack(1)//A123
typedef enum
{
    STOP_WORK = 0,//0：停止工作
    GRID_TO_LOAD,//1：电网带载
    GRID_TO_INV,//2：电网带载向逆变充电
    INV_TO_GRID,//3：逆变带载且向电网放电  限值在电网口
    MATCH_LOAD,//4：匹配负载
    PV_TO_GRID,//5：PV带载且向电网充电  需要知道PV最多可出多大功率  限值在PV口  借鉴全额上网
    PV_TO_INV,//6：PV能量转移  无电网干预     需要知道PV最多可出多大功率
    PV_TO_LOAD,//7：PV带部分负载  需要调节电池充电电流于电池总流
    URGENT_CHG,//8：紧急充电
    OFF_GRID,//9：切离网
}PARALLEL_MODE;

/*时间段属性*/
typedef struct 
{
    uint8_t Minute;
    uint8_t Hour;    
}SectionTimeStruct;

typedef union
{
    uint16_t  all;
    SectionTimeStruct bit;
}SectionTime;

typedef struct 
{
    uint8_t Lable; // 0：不启用, 1：充电时间区间，2：放电时间区间
    SectionTime Start; // 开启时间 (时:分)
    SectionTime End;   // 结束时间 (时:分)
}SetWorkTime;

struct _WorkMode
{
    //备电模式
    uint16_t BackupStandbyMode;//能量在任何时候都是保留备用
    // 自发自用模式
    uint16_t SelfConsumptionMode;
    // 削峰填谷模式
    uint16_t PeakLoadMode;
    // 紧急备电模式
    uint16_t EmergencyBackupMode;//灾害预警：风暴预警
    // 用户自定义模式
    uint16_t UserDefinedMode;
};

struct PARLLEL_SET
{
    uint16_t GridChgSwitch;//电网充电开关
    uint16_t FeedBackSwitch;//电网馈电开关
    uint16_t SetGridChgeMaxPower;   // 电网最大充电功率
    uint16_t SetGridChgeMaxCurrent;//电网最大充电电流
    uint16_t SetGridDischgeMaxPower;//电网最大放电功率
    uint16_t SetGridDischgeMaxCurrent;//电网最大放电电流
    uint16_t SetLowCapacity;//系统低电量设置值
    uint16_t SetHighCapacity;//系统高电量设置值
    uint16_t SetTimeCtrlEnable;//时间开关
    uint16_t CtrlWorkMode;
    SetWorkTime TimePeriod[TimePeriod_MAX_NUM];//6段时间
    uint16_t CurrentHour;
    uint16_t currentMin;
    uint16_t CyclePeriodTime;
    uint16_t MutiInvEnable;
    uint16_t MutiInvNum;//根据运行状态统计所有并网机器数量    
    uint16_t SetCountry;
    uint16_t EmsCtrlMode;//ems控制模式
    uint16_t ATS_Enable;//ATS使能 A139.4.3

    //uint16_t TotalMaxChgPower;//ems控制器计算已有效率折扣
    //uint16_t TotalMaxDisChgPower;//ems控制器计算已有效率折扣
    //uint16_t InitSetParamFlag;
    uint16_t INV_Online_Flag[MAXINVPHASE];
    uint16_t Online_Count;
    int32_t  CurrentDifferValue;
    uint16_t BatteryChargePowerLast;
    uint16_t EnergyLinePeriodTime;
    uint16_t EmsUploadPeriod;
    int16_t  Window_Period;
    uint16_t EmsPowerScheduleMaxTime;

    uint16_t ChgEfficiency;
    uint16_t DisChgEfficiency;
    uint16_t LastSetLowCapacity;//系统低电量设置值
    uint16_t LastSetHighCapacity;//系统高电量设置值
    uint16_t FilterDataPeriod;
    uint16_t OnGridInitScheduleTime;

    //4钟工作模式  
    struct _WorkMode WorkMode;
    // uint16_t StormWatchModelsFlag;
    uint16_t LastGridChgSwitch;//上一次的电网充电开关
    uint16_t total_dc_pv_numbers;
    uint16_t OffGridInitScheduleTime;
    uint16_t EMS_System_Fault_Code;

    //
    uint32_t ReturnTiming[MAXINVPHASE];
    // int16_t  LastStormWatchModelsFlag;
    uint16_t LastCtrlWorkMode;
};

struct INV_LIM
{
    uint16_t PhaseMaxChgPower[MAXINVPHASE];//每相最大充电功率
    uint16_t PhaseMaxDisChgPower[MAXINVPHASE];//最大放电功率
    uint16_t TotalMaxChgPower;//ems控制器计算已有效率折扣
    uint16_t TotalMaxDisChgPower;//ems控制器计算已有效率折扣
};

struct INV_DETAIL
{
    uint16_t InvSoc;//电池电量
    uint16_t SocThousand;//A103
    uint16_t PackRunState;
    uint16_t PackMaxChgCurrent;
    uint16_t PackMaxDisCurrent;
    uint16_t PackTotalVoltage;
    int16_t  PackTotalCurrent;
    uint16_t TotalMainPackCnts;
    uint16_t LowTempChgFlag;
    uint16_t BmsUpGradeFlag;
    uint16_t EmergencyChgFlag;
    uint16_t InvType;//机型
    uint64_t MachineSN;
    uint16_t EMSWorkMode;//工作模式暂定义 PV优先 UPS
    uint16_t EMSFaultCode;//后续改用位  底层发给控制器的 可包含所有相关故障位
    uint16_t ParallelInvState;//逆变器工作状态
    uint16_t DC_PVPower;
//    uint16_t DC_PVVoltage[MAXPVNUM];//M123 
    uint16_t GridFre;//电网频率
    uint16_t GridVoltage;//电网电压
    int16_t  InvPower[MAXINVPHASE];//M107 uint16_t
    uint16_t ELoadApparent[MAXINVPHASE];
    int16_t GridPower[MAXINVPHASE];//M123 uint16_t
    uint16_t AC_PvPower[MAXINVPHASE];
    uint16_t InvUpgradeFlag;
    uint16_t PvLimitFlag; 

    int16_t PackTotalMaxChgCurrent;
};

struct PARLLEL_EMS_PUBLIC
{
    struct INV_LIM    InvLimit;
    struct INV_DETAIL InvDetail;
    int16_t  TargetPower[MAXINVPHASE];
    uint16_t OffGridFlag;
    float Target_Power_Delta[MAXINVPHASE];
};


struct LOAD_POWER_DATA
{
    uint16_t  Detail[MAXPARLLELNUM][MAXINVPHASE];//重要负载1\2的每相负载功率 M139.3.3 uint16_t testwx 
    uint16_t  NonCritical[MAXINVPHASE];//非重要负载
    uint16_t  PhaseTotal[MAXINVPHASE];//每相负载总功率(含非重要负载) M139.3.3 uint16_t testwx
    uint16_t  TotalSingle[MAXPARLLELNUM];//单机总功率[不含非重要负载]
    uint16_t  TotalParallel;//并机总功率(含非重要负载)
    int16_t   PhaseTotalDetail[MAXINVPHASE];//每相重要负载总功率
};

typedef struct
{
    uint16_t StartTime[TimePeriod_MAX_NUM] ;
    uint16_t EndTime[TimePeriod_MAX_NUM] ;
    uint16_t TimeSwitch[TimePeriod_MAX_NUM];
    uint16_t SystemCurrentTime;
    uint16_t SystemYear;
    uint16_t SystemMonth;
    uint16_t SystemDate;
    uint16_t SystemHour;
    uint16_t SystemMinute;
    uint16_t SystemSecond;
    uint8_t SetSystemTimeFlag;
    uint8_t TimeChargeFlag ;
    uint8_t TimeDisChargeFlag ;
    uint8_t TimeStandbyFlag;
}EmsTimeEnable;

struct SOC_DETAIL
{
    uint16_t Value;
    uint16_t Seq;
    uint16_t FullChgFlag;
    uint16_t FullDisChgFlag;
    int16_t ForcedCharge;    
};

struct SoC_DETAIL_Two
{
    uint16_t Value;
    uint16_t Seq;
    uint16_t SoftIndex;
};

struct PARLLEL_EMS_PRIVATE
{
    struct   LOAD_POWER_DATA LoadPowerInfo;
    uint8_t  StartWorkFlag;//允许执行

    uint16_t  PowerMode;
    uint16_t  LastPowerMode;
    struct    SOC_DETAIL SocList[MAXPARLLELNUM];//  SOC具体值 对应序号  SOC由大到小排序 机器序号  SOC  Fullchg  DisFulchg
    int16_t   DefaultSeq;//默认序号  第一次为1 每两年切换一次
    uint16_t  FullChgFlag[MAXPARLLELNUM];//非匹配负载模式下  不允许AC侧充电 //DIssable
    uint16_t  DisFullChgFlag[MAXPARLLELNUM];//非匹配负载模式下 不允许AC侧放电 //DIssable

    uint8_t   SOCBalanceFlag;//SOC均衡标志
    uint16_t  PeriodTime;//策略执行周期 充放电 
    uint16_t  PeriodTimeMatch;//匹配负载和PV馈电的策略

    uint16_t  SingleMaxChgPower[MAXPARLLELNUM];
    uint16_t  SingleMaxDisChgPower[MAXPARLLELNUM];
    uint16_t  TotalBMSChgPower;
    uint16_t  TotalBMSDisChgPower;
    uint16_t  ACMaxChgPower;//电网最终最大允许充电功率
    uint16_t  ACMaxDisChgPower;//电网最终最大允许放电功率

    uint16_t  RateGridChgPower;//机器电网入口侧额定电网充电功率
    uint16_t  RateGridDisChgPower;
    
    uint16_t  SysPhase;

    uint16_t  TotalPhaseMaxChgPower[MAXPARLLELNUM];//总的每相最大充电功率
    uint16_t  TotalPhaseMaxDisChgPower[MAXPARLLELNUM];//总的每相最大放电功率

    int16_t   TargetPowerDetail[MAXPARLLELNUM][MAXINVPHASE];

    uint16_t  BMSChargeProtect[MAXPARLLELNUM];//充电保护
    uint16_t  InvChargeProtect[MAXPARLLELNUM];//充电保护

    EmsTimeEnable gTimeEnable;
    uint16_t PeriodCnt;

    uint16_t FineJustCnt;//匹配负载周期调节计数器
    uint16_t Data1;//临时变量1
    uint16_t Data2;//临时变量2
    uint16_t InvNum;//临时变量1
    uint16_t PhaseNum;//临时变量2
    int16_t TargetPhasePower[MAXINVPHASE];
    int16_t LastPhasePower[MAXINVPHASE];
    uint16_t LastInvOnlineNum;
    
    int16_t GridPower[MAXINVPHASE];
    
    int16_t AC_PVPower[MAXINVPHASE];
    int16_t AC_PVTotalPower;
    int16_t AC_PVRemainPVPower;
    int16_t AC_PVChgGridPower;//交流耦合情况下电网给逆变器的充电功率
    int16_t AC_PVRemainLoadPower;
    int16_t AC_PVDisChgGridPower;

    int16_t AC_PVCutLoad[MAXINVPHASE];
    int16_t AbsoluteValueOfLoad;
    int16_t AbsoluteValueOfACPV;

    int16_t BmsTotalPower[MAXPARLLELNUM];
    //int16_t TotalPower[MAXPARLLELNUM];
    //uint16_t TotalDisChgPower[MAXPARLLELNUM];
    
    uint16_t BmsLastTotalPower[MAXPARLLELNUM];
    //uint16_t LastTotalPower[MAXPARLLELNUM];
    //uint16_t LastTotalDisChgPower[MAXPARLLELNUM];
    int16_t  DeltaBmsPower[MAXPARLLELNUM];
    uint16_t PVSchedulPower[MAXPARLLELNUM];
    int16_t  PVTotalSchedulPower;

    //A139.7
    int16_t  PvTotalPower[MAXPARLLELNUM];
    uint16_t PvLastTotalPower[MAXPARLLELNUM];
    int16_t  DeltaPvPower[MAXPARLLELNUM];

    uint16_t SocInteger[MAXPARLLELNUM];
    uint8_t  PVFeedBackFlag[MAXPARLLELNUM];

    uint16_t UrgentChgNum;//紧急充电总个数
    uint16_t UrgentChgCnt[MAXINVPHASE];//周期紧急充电计数器 分多相
    int16_t  GridChgRemainPower[MAXINVPHASE];
    int16_t  UrgentChgAvgFlag[MAXINVPHASE];//紧急充电平均值标志位

    uint16_t EMSFaultCode;

    int16_t INVSerialIndex[MAXINVPHASE];
    uint64_t INVSerialSN[MAXINVPHASE];
    
    int16_t MatchBatteryPower[MAXPARLLELNUM];
    int16_t TotalMatchBatteryPower;
    int16_t MatchBatteryCurrent[MAXPARLLELNUM];
    int16_t TotalTargetPower;//M110 uint16_t
    int16_t PackTotalCurrent;
    //int16_t PackTotalMaxChgCurrent;

    uint16_t EnergyOnLineBoundryPower;  
    uint16_t EnergyOffLineBoundryPower;
    uint16_t EnergyLineCalibration;
    uint16_t MeterTimeout;
    int16_t  DefaultSoftwareSeq;
    // struct   SOC_DETAIL FixMaxSocList;//A119
    struct   SoC_DETAIL_Two LastMaxSocList;
    struct   SoC_DETAIL_Two FixMinSocList;
    struct   SoC_DETAIL_Two LastMinSocList;
    uint16_t MaxSoCrepetition;
    uint16_t MinSoCrepetition;

    int16_t WphA;
    int16_t WphB;
    int16_t WphC;
    int16_t WphATab[10];
    int16_t WphBTab[10];
    int16_t WphCTab[10];
    int16_t DC_PVRemainPVPower;

    uint16_t SingleCapacityPower[MAXPARLLELNUM];
    uint16_t CapacityRatio[MAXPARLLELNUM];
    int16_t ReturnDifferenceChargeEvent;
    int16_t ReturnDifferenceDisChargeEvent;// A139.2 wx
};

struct PARLLEL_SETTING 
{
    uint8_t InverterStartOrStopControlCommand;
    uint8_t DCPVOutputLimit;
    uint16_t L1ActiveTargetPower;
    uint16_t L1ReactiveTargetPower;
    uint16_t L2ActiveTargetPower;
    uint16_t L2ReactiveTargetPower;
    uint16_t L3ActiveTargetPower;
    uint16_t L3ReactiveTargetPower;
};


typedef struct {
	// PID控制器参数 
	float Kp; // 比例系数
	float Ki; // 积分系数
	float Kd; // 微分系数
	
	// PID控制器状态
	float integral;
	float last_error;
	float derivative_filtered;
	float alpha;

}EMS_PID_STRUCT;

extern EXT_RAM_BSS_ATTR EMS_PID_STRUCT ems_pid[3];

#pragma pack()//A123

////////////////////
void ems_stop_reverse(void);

#endif

