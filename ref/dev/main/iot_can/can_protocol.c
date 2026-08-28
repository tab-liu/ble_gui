#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "can_init.h"
#include "can_transmit.h"
#include "can_data.h"
#include "can_protocol.h"
#include "esp_log.h"
#include "comm_define.h"
#include "iot_ota.h"
#include "crc.h"

//#include "ext_can.h"
#include "freertos/event_groups.h"
#include "esp_bit_defs.h"
#include "data_summary.h"
#include "can_control.h"
//#include "energy_data.h"
#include "modbus_data.h"
#include "ll_param.h"
#include "iot_wifi_init.h"
#include "iot_period_task.h"
#include "iot_mqtt.h"
//#include "iot_ble.h"
#include "Cellular_4g_handle.h"
#include "pppos_client_main.h"
#include "filesystem.h"
#include "app_param.h"
#include "app_ble.h"
#include "can_pack.h"
#include "can_d400s.h"
#include "iot_box_task.h"
#include "modbus_slave.h"
#include "comm_define.h"
#include "app_time.h"


#define TAG "[CAN_PROTOCOL]"
#define SUB_TIME(time, interval)    ((time) > (interval)) ? ((time) - (interval)) : (0)

/* define critical section */
static portMUX_TYPE data_lock = portMUX_INITIALIZER_UNLOCKED;
#define TASK_ENTER_CRITICAL()    	portENTER_CRITICAL(&data_lock)
#define TASK_EXIT_CRITICAL()     	portEXIT_CRITICAL(&data_lock)

#define FAST_POLL_INTV				8000// 8000 1000					// 快速轮询周期ms
#define SLOW_POLL_INTV				13000//10000					// 慢速轮询周期ms

#define SLOW_POLL_IOT				30000//ms  30000

#define INV_ABOUT_DATA_POLL_INTV	40000//8000		// 定义轮询逆变ABOUT数据周期

#define AC_HUB_DATA_POLL	10000//
#define DC_HUB_DATA_POLL	10000//
#define D400S_HUB_DATA_POLL	10000//


#define CAN_TIMER_INTERVAL 			200// timeout 200  					// 定时器周期200ms
#define POLL_CMD_INTV				100						// 轮询指令间隔时间 //M118 50ms
#define EXT_CAN0_RECV_EVT			BIT0 					//外部CAN0接收事件
#define EXT_CAN1_RECV_EVT			BIT1 					//外部CAN1接收事件
#define MAX_BYTES 					FRAME_DATA_BYTES * 0xFF




//#define	MAX_DATA_TYPE		4									// 最大电池包类型数
//#define	MAX_DATA_SLAVE		DEV_MAIN_NODE_MAX * (PACK_MAX_NUM-1)// 最大从包数
//#define TYPE_SIZE 			12									// 类型号大小
//#define VERSION_SIZE 		6 									// 包括小数点
#define DEBUG_1100			1

typedef struct {
    CmdStruct  cmd;
    cmd_status status;
    uint16_t err_code;
    uint8_t dst_addr;
    uint8_t page;

    int wait_time;
}can_cmd_struct;




/*
windy:
多字节写相关
*/
typedef struct {
    can_cmd_struct write_cmd;

    /* cmd 超时定时器 */
    TimerHandle_t timer;
}can_cmd_bk_struct;


// int period_cmd_delay = 0;
can_cmd_bk_struct  can_cmd_bk;    // 备份read cmd
TimerHandle_t   can_task_timer = NULL;

QueueHandle_t xCanBusQueue_Recv = NULL;//CAN底层 Rx队列
QueueHandle_t xCanBusQueue_Send = NULL;//CAN底层 Tx队列
extern QueueHandle_t can_cmd_queue;


EXT_RAM_BSS_ATTR CanOtaStruct CanotaStatus;
EXT_RAM_BSS_ATTR CanOtaStruct CanotaRecvStatus;
EXT_RAM_BSS_ATTR CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];




/*
modbus

modbus 各pack的单独信息，单独变量数组
*/
EXT_RAM_BSS_ATTR MOD_STRUCT_Inv Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)+1];//end元素用于汇总=modbus slave0，需要IOT自身额外拟合
//EXT_RAM_BSS_ATTR MOD_STRUCT_Inv	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)];//微逆设备modbus beta总变量结构体 ble->
EXT_RAM_BSS_ATTR MOD_STRUCT_Inv	Inv_WR;//modbus写缓存 [(INV_MAX_NUM*DEV_MAIN_NODE_MAX)]
EXT_RAM_BSS_ATTR MOD_STRUCT_Pack Inv_Pack[PACK_MAX_NUM*DEV_MAIN_NODE_MAX];//各pack的单独信息,从 Inv结构体抽离
EXT_RAM_BSS_ATTR MOD_STRUCT_Pack Inv_Pack_Slave[DEV_PACK_NUM_MAX];//各pack分配从机地址后的单独信息,
EXT_RAM_BSS_ATTR MOD_STRUCT_AcHub Inv_AcHub;//用于记录ACHUB需要单独记录的数据，当前使用：100段寄存器
EXT_RAM_BSS_ATTR MOD_STRUCT_D400S Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)+1];//用于记录D400s需要单独记录的数据，当前使用：100段寄存器
EXT_RAM_BSS_ATTR MOD_STRUCT_Meter Meter[METER_MAX_NUM + 1];
EXT_RAM_BSS_ATTR MOD_STRUCT_Meter Meter_wr;
/*
windy add for S1:从modbus slave address 100开始上报
*/
EXT_RAM_BSS_ATTR MOD_STRUCT_Inv Plug[PLUG_MAX_NUM+1];//


/*
can

*/

EXT_RAM_BSS_ATTR MOD_STRUCT_Inv_READ	Inv_can[DEV_MAIN_NODE_MAX+1];//can设备 INV+BAT beta总变量结构体；windy end+1存储汇总
EXT_RAM_BSS_ATTR MOD_STRUCT_Inv_WRITE	Inv_can_WR;//can写缓存

EXT_RAM_BSS_ATTR MOD_STRUCT_Inv_MIX_READ	Inv_can_mix;//一个大系统中 仅一个模块，杂项混合
EXT_RAM_BSS_ATTR MOD_STRUCT_Inv_MIX_WRITE	Inv_can_mix_WR;//一个大系统中 仅一个模块，杂项混合


static void iot_can_fault_flag_set(void);
void Device_Can_Address_Parallel_Bind(twai_message_t *frame,uint8_t addr); //100ms cycle




static uint8_t CanCheckSetStatus(void);

static void CanSetWaitRespones(const twai_message_t *frame);
static void CanGetWaitResponse(node_info_t *node, const twai_message_t *frame);

static void CanLocalDataIsGet(node_info_t *node, const twai_message_t *frame);
static void CanLocalDataIsSet(node_info_t *node, const twai_message_t *frame);
static void can_rx_write_cmd(uint8_t node_id, twai_message_t *frame, QueueHandle_t cmd_queue);

uint8_t CanAckStart(uint8_t node_id, uint32_t id, uint16_t type, uint16_t start, uint16_t len, uint16_t crc16);

static void CanListenBus(node_info_t *node, twai_message_t *frame);
static void CanListenBus_From_IOT(node_info_t *node, twai_message_t *frame) ;

static void CanTimerout_Cb(void);//(TimerHandle_t xTimer);
static void CanPollPackCmd(uint8_t node_id, uint8_t packId);
static void CanPollInvCmd(uint8_t node_id, uint8_t invId);
static void can_trigger_cmd_read(void);
static void inv_log_sync_check(void);

void iot_lockdev_task(void);//(void * pvParameters);
void iot_polldev_task(void);//(void * pvParameters);

void ota_can_pct(OtaDevice canOta[], uint8_t id, uint8_t type, uint8_t pct, uint8_t where, uint8_t errcode);
void ota_dev_timeout(OtaDevice canOta[]) ;

//void can_ota_summary(uint8_t node_id, uint8_t group, uint8_t grp_id);
void can_ota_summary(uint8_t node_id, uint8_t group, uint16_t type);

//void can_ota_progress(uint8_t node_id, ota_group_status ota_md[]);
void Can_Inv_Addr_Check(node_info_t *node, twai_message_t *frame);
void can_data_poll_fast_from_modbus(void);

#if 0
//testwx
typedef struct {
    char type_ascii[12]; // pack类型
    uint32_t version[10];
	uint8_t count_version;//一个类型号下面版本号数量
	uint8_t count_online;//一个类型号下面实际在线的电池包数量 6154
} Data;


typedef struct {
	char type_ascii[12]; 						// pack类型
	uint64_t BMU_SN[24];						// BMU SN码
	uint32_t BMU_Detail[24];					// BMU 故障和保护等信息
	uint16_t Single_Cell_and_Temp_Count[24];	// 单个BMU中电芯数量及NTC数量
	uint8_t BMU_Type[24];						// BMU类型 等同于pack类型
	uint32_t BMU_Version[24];					// 版本号
	uint8_t count_online;						// 一个类型号下面实际在线的电池包数量 6154
} Data_Pack;//某一个类型电池包7200段需要的信息


typedef struct {
    uint32_t version_ARM;
	uint32_t version_DSP;
	uint32_t version_BMS;
	uint32_t version_DCHub;
} Data_INV;

typedef struct {
    uint8_t type_ascii; // 软件类型	0x01:ARM 0x02:DSP 0x03:BMS
    uint32_t version[3];
	uint8_t count_version;//一个类型号下面版本号数量
} Data_INV2;
#endif
void parseTypeVersion(const char *data, char *type, char *version);
float getMinVersion(Data versions[], int count, const char *type);


static uint8_t lock_node_id = 0xff;		// 手机锁定时的节点ID
static uint8_t trigger_read_addr = 0xff;
static uint8_t trigger_read_type;

//EXT_RAM_BSS_ATTR uint8_t str_res_6300[800] = {0}; // 用于回复6300段可变长的数据，app需要多少个字节的数据，IOT根据偏移放到该数组中返回
EXT_RAM_BSS_ATTR uint8_t str_res_7200[400] = {0}; // 用于回复7200段可变长的数据，app需要多少个字节的数据，IOT根据偏移放到该数组中返回

/*
modbus beta转can的 每一行寄存器的can字节数 CAN表没有的长度设为0

共计 Can beta四块 数据
2000
2200
2300
2400
*/
uint8_t MOD_STRUCT_len_reg02000[] =
{
	0,//uint16_t SetInvSelect;//2000	2
	2,//uint16_t mon_year;//
	2,//uint16_t hour_day;
	2,//uint16_t sec_min;
	2,//uint16_t  res;//2004 时区
	1,//uint16_t  work_mode;     // 2005  00：默认；01：高级；02：eco；03：UPS在线；04：UPS后备；05：峰谷；06：离网
	2,//ctrl_event1  ctrl;       // 2006 一次性控制事件,
	1,//uint16_t  ctrl_led;       // 2007 控制LED //2
    1,//ctrl_meter_struct  ctrl_meter;     // 2008 控制电表 (未用) ;20240920 windy 除PLP024未启用
    1,//uint16_t  ctrl_pv;        // 2009 控制PV
    1,//uint16_t  ctrl_inv;     // 2010 控制逆变
    1,//uint16_t  ctrl_ac;  // 2011 控制ac
    1,//uint16_t  ctrl_dc;  // 2012 控制dc
    1,//uint16_t  ctrl_poweron;  // 2013 控制电源
    1,//uint16_t  ctrl_dc_eco;       // 2014 控制dc eco模式0：关闭，1：开启
    1,//uint16_t  ctrl_dc_eco_time;  // 2015 控制dc eco模式空闲时间
    2,//uint16_t eco_dc_power_value;    // 2016 dc eco模式触发运行功率
    1,//uint16_t  ctrl_ac_eco;       // 2017 控制ac eco模式 0：关闭，1：开启
    1,//uint16_t  ctrl_ac_eco_time; // 2018 控制ac eco模式空闲时间
    2,//uint16_t eco_ac_power_value; // 2019 ac eco模式触发运行功率
    1,//uint16_t  ctrl_chg_mode; // 2020 控制充电模式
    1,//uint16_t  ctrl_super_power; // 2021 控制超级输出模式
    1,//uint16_t  ctrl_low_cap_pct;  // 2022 系统低电量百分比
    1,//uint16_t  ctrl_high_cap_pct; // 2023 系统满电量百分比
    1,//uint16_t  ctrl_inv_mode; // 2024 单火线，双火线，并机，并机双火线
    1,//uint16_t  ctrl_dev_id; // 2025 给逆变设置编号
    1,// uint16_t  ctrl_all_energy_type; // 2026 历史能量
    1,// uint16_t  ctrl_now_energy_type; // 2027 当前能量
    1,// uint16_t  ctrl_log_page;   // 2028 控制历史记录页
    1,//uint16_t  ctrl_time_area; // 2029 控制时间分段管理
    1,//SetTimeCtrlLable1   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart1
    2,//SetTimeCtrlEnd1
    1,//SetTimeCtrlLable2   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart2
    2,//SetTimeCtrlEnd2
    1,//SetTimeCtrlLable3   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart3
    2,//SetTimeCtrlEnd3
    1,//SetTimeCtrlLable4   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart4
    2,//SetTimeCtrlEnd4
    1,//SetTimeCtrlLable5   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart5
    2,//SetTimeCtrlEnd5
    1,//SetTimeCtrlLable6   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart6
    2,//SetTimeCtrlEnd6
    1,//SetTimeCtrlLable7   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart7
    2,//SetTimeCtrlEnd7
    1,//SetTimeCtrlLable8   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart8
    2,//SetTimeCtrlEnd8
    1,//SetTimeCtrlLable9   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart9
    2,//SetTimeCtrlEnd9
    1,//SetTimeCtrlLable10   time_control1  ctrl_time[10];//2030~2059
    2,//SetTimeCtrlStart10
    2,//SetTimeCtrlEnd10
    1,//SetPv1Type  uint16_t ctrl_PvType[6];//2060~2065
    1,//SetPv2Type
    1,//SetPv3Type
    1,//SetPv4Type
    1,//SetPv5Type
    1,//SetPv6Type
    1,//uint16_t ctrl_alarm_voice;//2066
    1,//uint16_t ctrl_lcd_active_time;//2067
	0,//2068~2070
	0,
	0,//2070
	0,//2071
	2,//self_configuration_uart self_config;// 2072 自定义模式
	2,// remote_Set remoteSet;// 2073 远程设置
	2,//uint16_t remoteSoc;//2074 远程设置相关SOC阈值
	2,//rv_Soc_OwenerShip ownerShip;//2075 房车SOC保有量设置
	2,//level_Switch LevelSwitch;//2076 童锁开关的等级开关
    2,//2077 休眠剩余时间
    2,//2078 LED颜色设置
    2,//2079 远程设置功率阈值
    2,//2080 电池包数量
    2,//2081 INV数量
    2,//2082 DCDC数量
    2,//2083 soc最高保有量设置
	2,//2084 pv高级设置
	2,//2085 DC输出
	2,//2086 合规设置
	2,//2087 循环容量
	2,//2088 最大循环容量
	2,//2089 生效时间年月
	2,//2090 生效时间日时
	2,//2091 生效时间分秒
	2,//2092 ECO状态
	2,//2093 控制AC支路
	2,//2094 控制DC支路
};//MOD_STRUCT_reg02000;

uint8_t MOD_STRUCT_len_reg02200[] =
{
    2,//uint8_t password[8]; //2200~2203 设置区密码
	2,//
	2,//
	2,// 2203
    0,// uint16_t revd1[2];  //2204~2205	1
	0,// 	1
    1,// uint16_t ctrl_reset_factory; //2206 恢复出厂
    1,// uint16_t ctrl_grid;      //2207 控制电网
    1,// uint16_t ctrl_feedback;  //2208 控制馈电
    2,// uint16_t ctrl_output_inv_volt;  //2209 输出电压控制
    2,// uint16_t ctrl_output_inv_freq;   //2210 输出电压频率
    2,// uint16_t ctrl_chg_max_volt; //2211 最大充电电压
    2,// uint16_t ctrl_pv_max_curr; //2212 最大PV充电电流
    2,// uint16_t ctrl_grid_max_power;   //2213 电网最大功率
    2,// uint16_t ctrl_grid_max_curr; //2214 电网最大电流
    2,// uint16_t ctrl_feedback_max_power;   //2215 电网馈电最大功率
    2,// uint16_t ctrl_feedback_max_curr; //2216 电网馈电最大电流
    2,// uint16_t off_grid_micro_rated_power; //2217 离网交流PV额定功率（该参数设置为系统实际接入的微逆的额定功率，使得逆变根据实际情况更准确的调节微逆功率）
    1,// uint16_t  ctrl_user_area; //2218 用户地区设置
    1,// uint16_t  ctrl_pv_paralle[6]; //2219~2224 设置PV并联开启和关闭
	1,//
	1,//
	1,//
	1,//
	1,// 2224
    1,// uint16_t  ctrl_grid_plus;    //2225 电网增强模式
    1,// uint16_t  ctrl_save_power_state; //2226 保存AC,DC关机前状态
    1,// uint16_t  ctrl_meter_enable; //2227 电表开启
    1,// uint16_t  ctrl_meter_select; //2228 电表类型选择
    1,// uint16_t  ctrl_Inv_Multi_enable; //2229 逆变器并机模式
    1,// uint16_t  ctrl_Inv_addr_Set; //2230 设置逆变器地址
    2,// ct_test_struct1 ct_test;    //2231
	2,// ctrl_mix_t ctrl_mix;  //2232 ctrl_mix_struct1
	0,// bat_maintain_struct1 bat_maintain;  //2233		2
	0,// uint16_t P_inv_active_target_L1; //2234		2
	0,// uint16_t P_inv_reactive_target_L1;//2235		2
	0,// uint16_t P_inv_active_target_L2; //2236		2
	0,// uint16_t P_inv_reactive_target_L2;//2237		2
	0,// uint16_t P_inv_active_target_L3; //2238		2
	0,// uint16_t P_inv_reactive_target_L3;//2239		2
	0,// uint16_t P_PV_Limit;//2240			2
	0,// ems_ctrl_struct1 ems_ctrl;//2241	2
	2,// ctrl_mix2_struct1 ctrl_mix2;//2242
    2,// uint16_t    chargingpile_type;  //2243 充电桩型号
    2,// ct_ratio_struct1  ct_ratio;           //2244 CT比例
    0,// ct_test_struct1   ac_ct_test;         //2245 交流耦合电表CT检测	2
    2,// GenSet_struct GenSet; //2246
    2,// uint16_t Gen_SOC_Start;//2247
    2,// uint16_t Gen_SOC_Stop;//2248
    2,// SetTimeCtrl_struct2 SetTimeCtrl[3];//2249~2257
	2,//
	2,//
	2,//
	2,//
	2,//
	2,//
	2,//
	2,// 2257
    2,// Undervoltage_protection	uint16_t revd[8];//2258~2265 便携
	2,// Undervoltage_protection_time
	2,// Highvoltage_protection
	2,// Highvoltage_protection_time
	2,// Underfrequency_protection
	2,// Underfrequency_protection_time
	2,// Overvoltage_protection
	2,// Overvoltage_protection_time	2265
    0,// uint16_t CounterCurrentPower_Limit;//2266						2
    0,// ctrl_meter_struct  ctrl_meter;     // 2267 控制电表(同2008)	2
    0,// uint16_t Meter_Select;//2268 低字节：电表接入选择				2
	2,// uint16_t SetCtrlPv;//2269 设置PV开启关闭
	2,// uint16_t Phase_set;//2270 单相双火相设置
	2,// uint16_t DCHUB_set;//2271 DCHUB设置电压_AC380设置
	2,// uint16_t SetGridMaxCurrent_in;//2272 最大电网输入电流
	2,// uint16_t Func_Set;//2273 开关设置,
	2,//    uint16_t RvSettings;// 2274 房车场景及电池设置
};//MOD_STRUCT_reg02200;

uint8_t MOD_STRUCT_len_reg02300[] =
{
    2,//uint16_t SetGridUV1Value;        // 电网欠压LEVEL1保护值
    2,//uint16_t SetGridUV1Time;         // 电网欠压LEVEL1触发时间
    2,//uint16_t SetGridUV2Value;        // 电网欠压LEVEL2保护值
    2,//uint16_t SetGridUV2Time;         // 电网欠压LEVEL2触发时间
    2,//uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
    2,//uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
    2,//uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
    2,//uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
    2,//uint16_t SetGridUF1Value;        // 电网欠频保护值
    2,//uint16_t SetGridUF1Time;         // 电网欠频保护时间
    2,//uint16_t SetGridOF1Value;        // 电网过频保护值
    2,//uint16_t SetGridOF1Time;         // 电网过频保护时间
    2,//uint16_t SetFreqSlope;           // 降频斜率
    2,//uint16_t SetPowerPct;            // 功率变化率
    2,//uint16_t SetGridVoltMinValue;    // 电网重连电压下限值
    2,//uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
    2,//uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
    2,//uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
    2,//uint16_t SetGridRetryTime;       // 电网异常重连时间
    2,//uint16_t SetGridAdvance;         // 高级功能使能
    2,//uint16_t SetGridUF2Value;         // 电网欠频保护值
    2,//uint16_t SetGridUF2Time;          // 电网欠频保护时间
    2,//uint16_t SetGridOF2Value;         // 电网过频保护值
    2,//uint16_t SetGridOF2Time;          // 电网过频保护时间
    2,//uint16_t SetGridAVGOVValue;       // 均值过压保护值
    2//uint16_t SetGridArea;             // 电网认证使用地区
};//inv_set02_struct;

uint8_t MOD_STRUCT_len_reg02400[] =
{
    2,//uint16_t SetGridEnable;          // 高级功能使能
    2,//uint16_t SetGridCountry;         // 电网认证使用地区
    2,//uint16_t SetGridUV1Value;        // 电网欠压LEVEL1保护值
    2,//uint16_t SetGridUV1Time;         // 电网欠压LEVEL1触发时间
    2,//uint16_t SetGridUV2Value;        // 电网欠压LEVEL2保护值
    2,//uint16_t SetGridUV2Time;         // 电网欠压LEVEL2触发时间
	2,//uint16_t pf;					 // 功率因数
	2,//uint16_t set_region;			 // 地区
	2,//uint16_t mode_enable;			 // 模式使能
	2,//uint16_t power_rate_limit;		 // 每分钟功率变化比率
    2,//uint16_t SetGridAVGOVValue;      // 电网均值过压
    2,//uint16_t SetGridOV1Value;        // 电网高压LEVEL1保护值
    2,//uint16_t SetGridOV1Time;         // 电网高压LEVEL1保护时间
    2,//uint16_t SetGridOV2Value;        // 电网高压LEVEL2保护值
    2,//uint16_t SetGridOV2Time;         // 电网高压LEVEL2保护时间
	2,//uint16_t power_reactive_to_active_ratio; // 无功占额定有功比例
	2,//uint16_t vw1;					 // Volt–watt响应模式的参数,Vw1<Vw2
	2,//uint16_t vw2;					 // Volt–watt响应模式的参数,Vw1<Vw2
    2,//uint16_t nc2[1];
    2,//uint16_t SetGridUF1Value;        // 电网欠频LEVEL1保护值
    2,//uint16_t SetGridUF1Time;         // 电网欠频LEVEL1保护时间
    2,//uint16_t SetGridUF2Value;        // 电网欠频LEVEL2保护值
    2,//uint16_t SetGridUF2Time;         // 电网欠频LEVEL2保护时间
	2,//uint16_t vv1;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	2,//uint16_t vv2;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	2,//uint16_t vv3;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
	2,//uint16_t vv4;					 // V-var and volt-watt响应模式的参数，Vv1<Vv2<Vv3<Vv4
    2,//uint16_t SetGridOF1Value;        // 电网过频LEVEL1保护值
    2,//uint16_t SetGridOF1Time;         // 电网过频LEVEL1保护时间
    2,//uint16_t SetGridOF2Value;        // 电网过频LEVEL2保护值
    2,//uint16_t SetGridOF2Time;         // 电网过频LEVEL2保护时间
    2,//uint16_t nc4[0];//2431
    2,//uint16_t nc4[1];//2432
    2,//uint16_t nc4[2];//2433
    2,//uint16_t nc4[3];//2434
    2,//uint16_t SetGridVoltMinValue;    // 2435 电网重连电压下限值
    2,//uint16_t SetGridVoltMaxValue;    // 电网重连电压上限值
    2,//uint16_t SetGridFreqMinValue;    // 电网重连频率下限值
    2,//uint16_t SetGridFreqMaxValue;    // 电网重连频率上限值
    2,//uint16_t SetGridRetryTime;       // 电网异常重连时间
	2//uint16_t SetDefaultValue;        // 设置默认值
};//inv_set03_struct;












void iot_Inv_data_init(void)
{
	memset(Inv, 0, sizeof(Inv));
	memset(&Inv_WR, 0, sizeof(Inv_WR));
	memset(Inv_Pack, 0, sizeof(Inv_Pack));
	memset(Inv_Pack_Slave, 0, sizeof(Inv_Pack_Slave));
	memset(&Inv_AcHub, 0, sizeof(Inv_AcHub));
	memset(Inv_D400S, 0, sizeof(Inv_D400S));
}




void iot_can_data_init(void)
{
    memset(&CanotaStatus, 0, sizeof(CanOtaStruct));
    memset(&g_device_data, 0, sizeof(device_data_struct));
	memset(&CanotaRecvStatus, 0, sizeof(CanOtaStruct));
	memset(can_ota_status, 0, sizeof(can_ota_status));



	for(int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for(int i = 0; i < (INV_MAX_NUM+1); i++)
		{
			memset(&Inv_can[node].inv_data[i], 0x00, sizeof(Inv_can[node].inv_data[i]));
		}
		for(int i = 0; i < (PACK_MAX_NUM+1); i++)
		{
			memset(&Inv_can[node].pack_data[i], 0x00, sizeof(Inv_can[node].pack_data[i]));
		}
		for(int i = 0; i < IOT_MAX_NUM; i++)
		{
			memset(&Inv_can[node].iot_data[i], 0x00, sizeof(Inv_can[node].iot_data[i]));
		}
		for(int i = 0; i < (DC_HUB_MAX_NUM+1); i++)
		{
			memset(&Inv_can[node].dc_hub_data[i], 0x00, sizeof(Inv_can[node].dc_hub_data[i]));
		}
		for(int i = 0; i < (D400S_MAX_NUM+1); i++)
		{
			memset(&Inv_can[node].d400s_data[i], 0x00, sizeof(Inv_can[node].d400s_data[i]));
		}
	}
	reals.Addr_can_master = 0;//debug only
	reals.Step_can_dev_parallel=STEP_CAN_PARALLEL_AFTER;
}
//1秒执行一次判断，是否有在线逆变器设备未获取到SN码
static void CanInvSnCheck(void)
{
	if(reals.online_Inv_num&&reals.Addr_can_master)
	{
		for (int n = 0; n < DEV_MAIN_NODE_MAX; n++)
        {
            for (int m = 0; m < INV_MAX_NUM; m++)
            {
                if(Inv_can[n].inv_data[m].online&&!Inv_can[n].inv_data[m].inv_about.dev_sn)
                {
					ESP_LOGI(TAG,"Inv:%d dev_sn Need Get ",n);
					can_data_poll_index_set(INDEX_INV_TYPE_ABOUT_13H);
                }
            }

        }

	}
}

//200ms定时器
//static void CanTimerout_Cb(TimerHandle_t xTimer)
static void CanTimerout_Cb(void)
{
    if (can_cmd_bk.write_cmd.wait_time)
	{
        can_cmd_bk.write_cmd.wait_time = SUB_TIME(can_cmd_bk.write_cmd.wait_time, CAN_TIMER_INTERVAL);
    }

	uint8_t ota_doing = 0;
	for(int i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		if (can_ota_status[i].timecnt)
		{ /*IOT通过CNA总线给下位机升级时所用的定时器*/
			can_ota_status[i].timecnt = SUB_TIME(can_ota_status[i].timecnt, CAN_TIMER_INTERVAL);
		}

		if(can_ota_status[i].ota_doing)
		{
			ota_doing |= 1;
		}

//		node_info_t node_info = {	/* CAN节点掉线检测 */
//			.node_id = i,
//			.invs_info = &Inv_can[i].inv_data,
//			.packs_info = &Inv_can[i].pack_data,
//			.iot_info = &Inv_can[i].iot_data,
//		};
	}

	CanNodeOfflineCheck(CAN_TIMER_INTERVAL);
	iot_can_fault_flag_set();//windy tbd
	CanotaStatus.ota_doing = ota_doing;





//	for(int i = 0; i < DEV_MAIN_NODE_MAX; i++)
	int i = 0;
	{
		/* CanOtaSender开始时状态为非空闲,CanOtaSender结束后状态为空闲 */
		if ((can_ota_status[i].ota_doing != CAN_OTA_IDLE)&&(can_ota_status[i].ota_doing<=CAN_OTA_SILENT)) {
			can_ota_status[i].period_cmd_delay = 6000;
		}
		if(can_ota_status[i].period_cmd_delay > 0)
		{
			ESP_LOGI(TAG,"can_ota_status[%d].period_cmd_delay:%d  ota_doing:%d",i,can_ota_status[i].period_cmd_delay,can_ota_status[i].ota_doing);
			can_ota_status[i].period_cmd_delay = SUB_TIME(can_ota_status[i].period_cmd_delay, CAN_TIMER_INTERVAL);
			if(can_ota_status[i].period_cmd_delay == 0) {
				can_ota_status[i].ota_doing = 0;
				ota_dev_timeout(can_ota_status[i].devStatus);
			}

			// can_ota_summary(0, Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_type,Inv[0].mod_reg00700_OTA.ota_cmd.type);	// 汇总OTA状态
			//can_ota_summary(0, Inv[DEV_MAIN_NODE_MAX].mod_reg00700_OTA.ota_cmd.group.dev_type,Inv[DEV_MAIN_NODE_MAX].mod_reg00700_OTA.ota_cmd.type);	// 汇总OTA状态
			can_ota_summary(0, can_ota_status[i].sender_group,can_ota_status[i].sender_type);	// 汇总OTA状态
//			can_ota_progress(i, Inv[i].mod_reg00700_OTA.ota_group);			// 上报OTA进度
		}
	}

	if(CanotaStatus.ota_doing == 0)
	{
		CanCmdCountDown(CAN_TIMER_INTERVAL); /* 发送CAN周期查询指令 */
	}
}

#ifdef FUNC_DEV_TRIGGER_BIND
void Can_binding_triger_chk(void)
{
	static uint16_t Bind_Send_Count=0;
	uint8_t need_bind = 0;
	uint8_t zero_count=0;
	uint8_t buf_len=sizeof(SetData.dev_info_t.Sn_bind)/sizeof(SetData.dev_info_t.Sn_bind[0]);
	ESP_LOGI(TAG,"buf_len:%d",buf_len);
	for(uint8_t i=0;i<buf_len;i++)
	{
		if(SetData.dev_info_t.Sn_bind[i]!=0)
		{
			break;
		}
		zero_count++;
	}
	if(zero_count==buf_len)
		need_bind=1;


	if(need_bind)
	{
		Bind_Send_Count++;
		if(Bind_Send_Count>300) //未绑定SN码超过5分钟则主动下发一次绑定帧查询
		{
			Bind_Send_Count=0;
			g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND=1;
		}
	}
}
#endif


/*------------------------------------------------------------------------
*@Function :Can_Tx_mix_single

can tx 偶发 单帧发送

return:
0-fail
1-ok

*/
uint8_t Can_Tx_single_binding_ack(uint8_t* sn_bind) //100ms cycle
{
	/* 填充CANID数据结构 */
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
    static uint16_t sStepSendPeriodCnt = 0;//Step CAN发送周期


	if(reals.trigger_Bind_AckCount)
	{
		if(++sStepSendPeriodCnt >= 10)//1s
		{
			sStepSendPeriodCnt = 0;
			canid.bit.src = esp_canbus_myself_address();
			canid.bit.dst = CAN_BROADCAST_ADDRESS;//INV_CAN_ADDR+reals.Addr_can_self 绑定 是生产操作，要假定上下不一致地址
			canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
			canid.bit.page = CAN_CTRL_FRAME_PAGE;
			canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

			can_ctrl.data[0]=CAN_CTRL_CMD_99;
			can_ctrl.data[1]=12;
			can_ctrl.data[2]=0x41;
			can_ctrl.data[3]=sn_bind[0];
			can_ctrl.data[4]=sn_bind[1];
			can_ctrl.data[5]=sn_bind[2];
			can_ctrl.data[6]=sn_bind[3];
			can_ctrl.data[7]=sn_bind[4];

			reals.trigger_Bind_AckCount--;
			ESP_LOGW(TAG, "[12170 BIND] CAN TX ack remaining=%d sn=%02X %02X %02X %02X %02X",
				reals.trigger_Bind_AckCount,
				sn_bind[0], sn_bind[1], sn_bind[2], sn_bind[3], sn_bind[4]);
			/* 发送单帧CAN数据 */
			if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t)))
			{
				ESP_LOGE(TAG, "[12170 BIND] CAN TX ack FAIL");
				return 0;
			}
		}




	}

	return 1;
}

/*------------------------------------------------------------------------
*@Function :Can_Tx_mix_single

can tx 偶发 单帧发送

return:
0-fail
1-ok

*/
uint8_t Can_Tx_single_binding_triger(void) //100ms cycle
{
	/* 填充CANID数据结构 */
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};
    static uint16_t sStepSendPeriodCnt = 0;//Step CAN发送周期



	if(1 == g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND)
	{
		if(++sStepSendPeriodCnt >= 10)//1s
		{
			sStepSendPeriodCnt = 0;
			canid.bit.src = esp_canbus_myself_address();
			canid.bit.dst = CAN_BROADCAST_ADDRESS;//INV_CAN_ADDR+reals.Addr_can_self 绑定 是生产操作，要假定上下不一致地址
			canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
			canid.bit.page = CAN_CTRL_FRAME_PAGE;
			canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

			can_ctrl.data[0]=CAN_CTRL_CMD_99;
			can_ctrl.data[1]=10;
			can_ctrl.data[2]=CAN_CTRL_CMD_BIND_TRIGER;
			can_ctrl.data[3]=0;
			can_ctrl.data[4]=0;
			can_ctrl.data[5]=0;
			can_ctrl.data[6]=0;
			can_ctrl.data[7]=0;

			/* 发送单帧CAN数据 */
			if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t)))
			{
				ESP_LOGE(TAG, "[12170 BIND] CAN TX trigger FAIL: id=0x%08lX src=%u dst=%u cmd=%u/%u/0x%02X",
					(unsigned long)canid.all, canid.bit.src, canid.bit.dst,
					can_ctrl.data[0], can_ctrl.data[1], can_ctrl.data[2]);
				return 0;
			}
			ESP_LOGW(TAG, "[12170 BIND] CAN TX trigger OK: id=0x%08lX src=%u dst=%u data=%02X %02X %02X %02X %02X %02X %02X %02X",
				(unsigned long)canid.all, canid.bit.src, canid.bit.dst,
				can_ctrl.data[0], can_ctrl.data[1], can_ctrl.data[2], can_ctrl.data[3],
				can_ctrl.data[4], can_ctrl.data[5], can_ctrl.data[6], can_ctrl.data[7]);
		}




	}

	return 1;
}

/*------------------------------------------------------------------------
*@Function :Can_Tx_single_binding_Check

can tx 定时1小时发送一次 单帧发送

return:
0-fail
1-ok

*/
uint8_t Can_Tx_single_binding_Check(void) //100ms cycle
{
	/* 填充CANID数据结构 */
	IdStruct canid;
	can_ctrl_t can_ctrl = {0};

	//if(++sStepSendPeriodCnt >= 10)//1s
	{
		canid.bit.src = esp_canbus_myself_address();
		canid.bit.dst = CAN_BROADCAST_ADDRESS;//INV_CAN_ADDR+reals.Addr_can_self 绑定 是生产操作，要假定上下不一致地址
		canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
		canid.bit.page = CAN_CTRL_FRAME_PAGE;
		canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

		can_ctrl.data[0]=CAN_CTRL_CMD_99;
		can_ctrl.data[1]=10;
		can_ctrl.data[2]=CAN_CTRL_CMD_BIND_CHECK;
		can_ctrl.data[3]=0;
		can_ctrl.data[4]=0;
		can_ctrl.data[5]=0;
		can_ctrl.data[6]=0;
		can_ctrl.data[7]=0;

		ESP_LOGI(TAG, "Can_Tx_single_binding_Check ");//

		/* 发送单帧CAN数据 */
		if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t)))
		{
			return 0;
		}




	}

	return 1;
}


/**
 * @brief 接收CAN总线数据帧,并将数据帧存储地址，放入消息队列
 *
 * @param pvParameters : 接收CAN总线消息队列
 * @return ****** void
 */
void esp_canbus_base_recv_send_task(void)//(void * pvParameters)
{
//	static uint8_t stxcnt=0;//


}

/*
从BLE/WIFI到CAN的透彻转发

*/
void iot_to_can_cmd_task(void)
{

/**
 * @brief modbus的所有设置是写入设置缓存区，然后再将缓存区的数据转换为CAN命令发送给设备
 *	1、手机APP发送设置数据(modbus写入数据到IOT的设置缓存区，每个可以设置区间，都带有缓存区)
 *	2、数据写入缓存区时，就已经通过转换关系表得到了，需要缓存为CAN命令的数据类型，数据偏移，数据长度
 *	3、将modbus设置缓存区中的数据，生成对应的CAN命令发送给设备
 *	4、某些指令发送后还会激活与其相关的联动指令，如历史日志指令
 */
	can_cmd_queue_struct can_cmd_msg = {NULL, 0, 0};
	if (can_cmd_queue && xQueueReceive(can_cmd_queue, &can_cmd_msg, 0) == pdTRUE) //portMAX_DELAY
	{ /* 等待MODBUS接收设置命令的信号 */

		if(reals.BLE_WIFI_to_Can_Cnt > 0)
		{
			reals.BLE_WIFI_to_Can_Cnt--;

		}

		CanCmdGenerate(can_cmd_msg.md_addr, can_cmd_msg.cmd, can_cmd_msg.num); // 创建并发送CAN命令
		if (can_cmd_msg.cmd)
		{
			free(can_cmd_msg.cmd);
		}
//		vTaskDelay(pdMS_TO_TICKS(10)); // 本任务优先级高,适当延时可以避免，该任务堵死其他任务运行
	}

}

/*
CAN RX报文接收解析
*/
void CanRx_Parse_Task(void)
{
    can_queue_msg_struct queue_msg = {NULL};
    uint16_t handl_count = 0;
	uint8_t level1_addr = 0;
	uint8_t level2_addr = 0;

//	if(CanotaStatus.ota_doing)
//	{
//		task_delay = 10;
//	}
//	else
//	{
//		task_delay = 500;
//	}

	handl_count = 100;
	while(handl_count !=0)//单次大循环周期，最多只解析100帧，防止主循环死机
	{
		handl_count--;
			if(xCanBusQueue_Recv && xQueueReceive(xCanBusQueue_Recv, &queue_msg, 0) == pdTRUE)//pdMS_TO_TICKS(task_delay)
			{
				reals.canrx_pop_cnt++;
		//		queue_msg.pdata->identifier = CAN_ADDR_SWAP(queue_msg.pdata->identifier);  /* 交换目的地址和源地址,windy错误，不需要调换源和目标地址 */
				Can_ID_Check(queue_msg.pdata->identifier, &level1_addr ,&level2_addr);
				node_info_t node_info;
				node_info.node_id = 0;
				node_info.invs_info = &Inv_can[level1_addr].inv_data[0];//level2_addr
				node_info.dchubs_info = &Inv_can[level1_addr].dc_hub_data[0];//level2_addr
				node_info.achubs_info = &Inv_can_mix.ac_hub_data[0];//level2_addr
				node_info.packs_info = &Inv_can[level1_addr].pack_data[0];//level2_addr
				node_info.iot_info = &Inv_can[level1_addr].iot_data[0];
				node_info.d400s_s_info=&Inv_can[level1_addr].d400s_data[0];

				// node_info.invs_info = Inv_can[level1_addr].inv_data;
				// node_info.dchubs_info = Inv_can[level1_addr].dc_hub_data;//level2_addr
				// node_info.achubs_info = Inv_can_mix.ac_hub_data;//level2_addr
				// node_info.packs_info = Inv_can[level1_addr].pack_data;//level2_addr
				// node_info.iot_info = &Inv_can[level1_addr].iot_data[0];
				// node_info.d400s_s_info=Inv_can[level1_addr].d400s_data;
				//ESP_LOGW(TAG,"Test level1_addr == %d, level2_addr == %d", level1_addr, level2_addr);

				/* 接收发送给本机的CAN写指令,如果接收到期望的指令,
				 * 该函数返回后会清零CAN帧,后续功能模块将忽略此CAN帧以避免错误处理,
				 * 目前该函数只接收关于历史数据读取的CAN帧,其它CAN帧会忽略 */
		//		can_rx_write_cmd(0, queue_msg.pdata, can_recv_cmd_queue);//传入CAN队列，tbd


				// 打印十六进制数组

#ifdef DEBUG_PRINTF_ENABLE
		//		 printf("CanRx_Parse_Task,windy !\n" );

				char hex_data[queue_msg.pdata->data_length_code * 2 + 1];
				for (int i = 0; i < queue_msg.pdata->data_length_code; i++) {
					sprintf(hex_data + i * 2, "%02x", queue_msg.pdata->data[i]);
				}
				// 打印十六进制数字
				 ESP_LOGW(TAG, "[CanRx_Parse_Task] Received message: identifier = %lx,	data: %s", queue_msg.pdata->identifier, hex_data);

#endif
				//ESP_LOGI(TAG,"1 CanRx_Parse_Task self_config.all:0x%x ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.self_config.all);
				//ESP_LOGI(TAG,"1 CanRx_Parse_Task LevelSwitch.bit.level:%d ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.LevelSwitch.bit.level);
				//ESP_LOGI(TAG,"CanRx_Parse_Task SoftwareVersion:%d",(unsigned int)Inv_can_mix.ac_hub_data[0].ac_hub_info.SoftwareVersion);
				CanListenBus(&node_info, queue_msg.pdata);			/* 监听设备广播的数据 */
				CanListenBus_From_IOT(&node_info, queue_msg.pdata);

				CanLocalDataIsGet(&node_info, queue_msg.pdata); 	/* 本地数据被读取 */

				/* IOT设备的标定信息通过CAN写入 */
				CanLocalDataIsSet(&node_info, queue_msg.pdata); 	/* 本地数据被写入 */

				CanSetWaitRespones(queue_msg.pdata);				/* IOT发送写入设备后,等待设备的应答 */
				CanGetWaitResponse(&node_info, queue_msg.pdata);	/* IOT发送读取设备后,等待设备的应答 */
				Can_Inv_Addr_Check(&node_info, queue_msg.pdata);//windy add
				/* CAN综合控制帧处理 */
				// twai_message_t *twai_message = queue_msg.pdata;
				// can_overall_ctrl_handle(0, twai_message->identifier, twai_message->data, twai_message->data_length_code);

			}
			else
			{
//				handl_count = 0;
				queue_msg.pdata = NULL;
			}
#ifdef FUNC_ONECLICK_UPGRADE_EN
			if (!Can_OneClikeOta_Task(queue_msg.pdata, &can_ota_status[0], Inv_WR.mod_reg00700_OTA.ota_cmd.begin))
#endif
			{
				CanOtaSender(0, queue_msg.pdata, &can_ota_status[0]);	// can ota send
			}
			CanOtaReceiver(0, queue_msg.pdata, &can_ota_status[0]); // can ota recv

			if (queue_msg.pdata)
			{
				free(queue_msg.pdata); /* 处理完成释放CAN数据帧 */
				queue_msg.pdata = NULL;
//				if(CanotaStatus.ota_doing == 0 )
				//{
					//handl_count++;
//					if (handl_count >= 100) { /* 连续处理100帧数据后,延迟10ms在处理 */
//						handl_count = 0;
//						vTaskDelay(pdMS_TO_TICKS(50));	// 防止总线上数据太多，其他任务无法运行
//					}
				//}
			}
			else//NULL
			{
				break;//没有RX数据，退出循环
			}
	}

}
void iot_slef_data(void )
{

	for(int i = 0; i < INV_MAX_NUM; i++)
	{
		Inv_can[reals.Addr_can_self].iot_data[i].online =1;
		Inv_can[reals.Addr_can_self].iot_data[i].alive_time = IOT_OFFLINE_TIME; /* 离线检测时间 */

	}

}
/*
超时清除flag
*/
void iot_slef_cmd_flag_clean(void )//100ms cycle
{
	static uint8_t ts_u16Cnt = 0;

	if(reals.BLE_WIFI_to_Can_Cnt > 0)
	{
		if(++ts_u16Cnt >= 10)//假定十秒内必须转发完成，否则清除；补丁
		{
			ts_u16Cnt=0;
			reals.BLE_WIFI_to_Can_Cnt =0;

		}
	}


}


/*
为了解决手机APP快速刷新能量线；周期8s+ 快速变化量触发读取
*/
static void can_data_fast_poll( void ) //1s
{
	static uint16_t sTotalCurrent_bias_old[DEV_MAIN_NODE_MAX]={0};
	static uint16_t sdelaycnt=0;
	static uint8_t  ctrl_ac[DEV_MAIN_NODE_MAX]={0xFF};
	static uint8_t  ctrl_dc[DEV_MAIN_NODE_MAX]={0xFF};
	static uint8_t  ctrl_ac_switch=0;


	if(++sdelaycnt >= 3)
	{
		sdelaycnt=0;
		//电流变化，触发快速读取
		for(uint8_t i = 0; i < DEV_MAIN_NODE_MAX; i++)
		{
			if(abs(sTotalCurrent_bias_old[i] - Inv_can[i].pack_data[0].pack_announce.TotalCurrent_bias) > 30)//3A
			{
				sTotalCurrent_bias_old[i] =Inv_can[i].pack_data[0].pack_announce.TotalCurrent_bias;

				can_data_poll_index_set(INDEX_INV_TYPE_BASE_11H);
			}

			if(ctrl_ac[i] != Inv_can[i].inv_data[0].inv_set00.ctrl_ac)//3A
			{
				ctrl_ac[i] =Inv_can[i].inv_data[0].inv_set00.ctrl_ac;
				ESP_LOGI(TAG, "Inv_can[%d].inv_data[0].inv_set00.ctrl_ac =%d",i, Inv_can[i].inv_data[0].inv_set00.ctrl_ac);

				can_data_poll_index_set(INDEX_INV_TYPE_BASE_11H);
			}

			if(ctrl_dc[i] != Inv_can[i].inv_data[0].inv_set00.ctrl_dc)//3A
			{
				ctrl_dc[i] =Inv_can[i].inv_data[0].inv_set00.ctrl_dc;
				ESP_LOGI(TAG, "Inv_can[%d].inv_data[0].inv_set00.ctrl_dc =%d",i, Inv_can[i].inv_data[0].inv_set00.ctrl_dc);

				can_data_poll_index_set(INDEX_INV_TYPE_BASE_11H);
			}

		}

		if(ctrl_ac_switch != Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting.bit.ac_switch)//3A
		{
				ctrl_ac_switch =Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting.bit.ac_switch;
				ESP_LOGI(TAG, "Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting.bit.ac_switch =%x", Inv_can_mix.ac_hub_data[0].dc_ac_hub_setting.ac_hug_setting.bit.ac_switch);

				can_data_poll_index_set(INDEX_MODULE_TYPE_AC_HUB_INFO_40H);
		}
	}


}

//windy add 1
void iot_can_high_level_task(void * pvParameters)
{
	static uint8_t ts_u16Cnt50ms = 0;
	static uint8_t ts_u16Cnt100ms = 0;
	static uint8_t ts_u16Cnt1s = 0;

	static uint8_t sCnt = 0;

    while (1)
	{
		vTaskDelay(pdMS_TO_TICKS(20));//windy必须延时 50
		if(ts_u16Cnt50ms != reals.Timer_50msCount)
		{
			ts_u16Cnt50ms = reals.Timer_50msCount;
			reals.st_FlagTime.bits.b1FlagSys50ms_can_hi_level = 1;
		}
		else
		{
			reals.st_FlagTime.bits.b1FlagSys50ms_can_hi_level = 0;
		}

		if(ts_u16Cnt100ms != reals.Timer_100msCount)
		{
			ts_u16Cnt100ms = reals.Timer_100msCount;
			reals.st_FlagTime.bits.b1FlagSys100ms_can_hi_level = 1;
		}
		else
		{
			reals.st_FlagTime.bits.b1FlagSys100ms_can_hi_level = 0;
		}

		if(ts_u16Cnt1s != reals.Timer_1sCount)
		{
			ts_u16Cnt1s = reals.Timer_1sCount;
			reals.st_FlagTime.bits.b1FlagSys1s_can_hi_level = 1;
		}
		else
		{
			reals.st_FlagTime.bits.b1FlagSys1s_can_hi_level = 0;
		}


		if(1 == reals.st_FlagTime.bits.b1FlagSys1s_can_hi_level)//
		{
			iot_slef_data();
			Iot_Self_Data_To_Can_data();
			CAN_IOT_Parameter_WR();
			CAN_DEV_INV_ACHUB_SN_Get_to_Save();

			can_data_poll_fast_from_modbus();
			can_data_fast_poll();

		}

		if(1 == reals.st_FlagTime.bits.b1FlagSys100ms_can_hi_level)
		{
			iot_slef_cmd_flag_clean();
		}

//		if(1 == reals.st_FlagTime.bits.b1FlagSys50ms_can_hi_level)//
		{
			//BLE/WIFI-> CAN
			iot_to_can_cmd_task();

			iot_polldev_task();
			iot_lockdev_task();

			// ESP_LOGI(TAG,"out sCnt = %d",sCnt);
			if(++sCnt >= 4)//200ms
			{
				sCnt=0;
				// ESP_LOGI(TAG,"inner sCnt = %d",sCnt);
			}


		}
	}



}

//windy add 2
void iot_can_low_level_task(void * pvParameters)
{
	static uint8_t sCnt_100ms = 0;
	static uint8_t sCnt_200ms = 0;
	static uint8_t sCnt_1000ms = 0;
	static uint16_t sBindChk_100min = 50; 	//首次上电5秒后检测

	static uint8_t ts_u16Cnt10ms = 0;
	static uint8_t ts_u16Cnt100ms = 0;

    esp_canbus_init();

    /* 创建CAN总线,接收消息队列 */
    xCanBusQueue_Recv = xQueueCreate(300, sizeof(can_queue_msg_struct));
    if (!xCanBusQueue_Recv) {
        ESP_LOGE (TAG, "Can bus received queue create failed");
    }

    /* 创建CAN总线,发送消息队列 */
    xCanBusQueue_Send = xQueueCreate(200, sizeof(can_queue_msg_struct));
    if (!xCanBusQueue_Send) {
        ESP_LOGE (TAG, "Can bus send queue create failed");
    }



// printf("go in esp_canbus_base_recv_send_task!!!!!!!!!\n");
	while (1)
	{
		if(ts_u16Cnt10ms != reals.Timer_10msCount)
		{
			ts_u16Cnt10ms = reals.Timer_10msCount;
			reals.st_FlagTime.bits.b1FlagSys10ms_2 = 1;
		}
		else
		{
			reals.st_FlagTime.bits.b1FlagSys10ms_2 = 0;
		}

		if(ts_u16Cnt100ms != reals.Timer_100msCount)
		{
			ts_u16Cnt100ms = reals.Timer_100msCount;
			reals.st_FlagTime.bits.b1FlagSys100ms_2 = 1;
		}
		else
		{
			reals.st_FlagTime.bits.b1FlagSys100ms_2 = 0;
		}

		//CAN Rx 解析
		CanRx_Parse_Task();

		 if(1 == reals.st_FlagTime.bits.b1FlagSys100ms_2)
		{
			if(++sCnt_200ms >= 2)
			{
				sCnt_200ms=0;
				CanTimerout_Cb();  // can 总线读指令，超时定时器
				pack_infos_summary(&Inv_can[0]);
				d400s_infos_summary(&Inv_can[0]);
			}

			if(++sCnt_1000ms>=10)
			{
				sCnt_1000ms=0;
				CanInvSnCheck();
			}


			if(++sCnt_100ms >= 1)//100ms
			{
				sCnt_100ms=0;
				Device_Can_Address_Parallel_Step();
				Can_binding_triger_chk();
				Can_Tx_single_binding_triger();
				Can_Tx_single_binding_ack(SetData.dev_info_t.Sn_bind);
				//Device_Can_Address_Parallel_Triger();
			}
			sBindChk_100min--;
			if(sBindChk_100min==0)
			{
				sBindChk_100min=36000;//首次上电检测命令发送后，后续每1小时发送一次
				Can_Tx_single_binding_Check();
			}
			if(reals.Addr_can_Attr&Can_Addr_Parallel_SendBindChk)
			{
				reals.Addr_can_Attr&=~(Can_Addr_Parallel_SendBindChk);
				Can_Tx_single_binding_Check();
			}
			esp_canbus_state_change(); // 检测CAN总线状态并切换


		}



		// esp_canbus_recv_task();
		vTaskDelay(pdMS_TO_TICKS(30));//5 300
//		 vTaskDelay(pdMS_TO_TICKS(10));//5 300

	}

}





static void iot_can_fault_flag_set(void)//for modbus 109, wrong windy
{
	#define INV_CAN_FAULT_MASK		(1 << 0)	// 与逆变通信故障
	#define PACK_CAN_FAULT_MASK		(1 << 1)	// 与BMS通信故障

	int inv_all_online = 1;
	int pack_all_online = 1;
	// static uint8_t inv_online_flag[DEV_MAIN_NODE_MAX];
	// static uint8_t pack_online_flag[DEV_MAIN_NODE_MAX];

	// uint8_t inv_online_tmp[DEV_MAIN_NODE_MAX];
	// int8_t pack_online_tmp[DEV_MAIN_NODE_MAX];

	/* 检测CAN通信故障,任意一路CAN通信故障就设置故障标志 */
	for (uint8_t node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		if (!Inv_can[node_id].inv_data[0].online) //timeout
		{
			inv_all_online = 0;
		}

		if (!Inv_can[node_id].pack_data[0].online)
		{
			pack_all_online = 0;
		}
	}

	if (inv_all_online)
	{
		g_device_data.iot_dev_node.can_fault &= ~INV_CAN_FAULT_MASK;	// 无故障
	}
	else //0
	{
		g_device_data.iot_dev_node.can_fault |= INV_CAN_FAULT_MASK;		// 有故障
	}

	if (pack_all_online)
	{
		g_device_data.iot_dev_node.can_fault &= ~PACK_CAN_FAULT_MASK;
	}
	else
	{
		g_device_data.iot_dev_node.can_fault |= PACK_CAN_FAULT_MASK;
	}
}

/* 需要周期性的下发该帧到逆变器 */
static void can_enable_active_report(uint8_t node_id)
{
	// can_reply_data_t *reply_data;

#if 0	// 当前暂不开启PACK主动上报功能
	/* 查询pack是否开启主动上报 */
	reply_data = can_overall_ctrl_get_reply(node_id, CAN_REPLY_CMD_EN_PACK_REPORT);
	if(reply_data == NULL) return;

	if(reply_data->en_pack_report_reply.respond == 0)
	{
		can_ctrl_t can_ctrl = {0};
		can_ctrl.cmd = CAN_CTRL_CMD_EN_PACK_REPORT;
		can_overall_ctrl(node_id, &can_ctrl);	// 打开pack主动上报
	}
#endif

	/* 查询逆变是否开启主动上报 */
	// reply_data = can_overall_ctrl_get_reply(node_id, CAN_REPLY_CMD_EN_INV_REPORT);
	// if(reply_data == NULL) return;

	// if(reply_data->en_inv_report_reply.respond == 0)
	// {
		can_ctrl_t can_ctrl = {0};
		can_ctrl.cmd = CAN_CTRL_CMD_EN_INV_REPORT;
		can_overall_ctrl(node_id, &can_ctrl);	// 打开逆变主动上报
	// }
}

/*
IOT发单帧：
开启 INV/bat主动上报使能


*/
static void dev_enable_active_report(void)
{
	#define ENABLE_ACTIVE_REPORT_CMD_PERIOD		30000//ms  3000->30000,30s

	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	static uint32_t pre_time = 0;//(xTaskGetTickCount() * portTICK_PERIOD_MS) + (ENABLE_ACTIVE_REPORT_CMD_PERIOD >>1);//首次加速

	if ((now_time - pre_time) < ENABLE_ACTIVE_REPORT_CMD_PERIOD) return;
	pre_time = now_time;

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		if (can_ota_status[node_id].period_cmd_delay > 0) continue;				//CAN-OTA返回

		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			/* 该总线有逆变设备在线就开启主动上报 */
			if (Inv_can[node_id].inv_data[i].online)
			{
				ESP_LOGW(TAG,"can_enable_active_report");
				can_enable_active_report(node_id);
				break;
			}
		}
	}
}

static void inv_on_grid_meter_switch_process(void)
{
	#define CHECK_PERIOD		3000

	static uint32_t last_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if ((now_time - last_time) < CHECK_PERIOD) return;
	last_time = now_time;

	/* 并机开关打开时才进行底层AC电表开关状态检测 */
	inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;// ll_inv_param_get("inv_advance_param");

	// printf("multi_inv_off_grid_en:%d\n",advance_param->ctrl_mix.bit.multi_inv_off_grid_en);

	if(advance_param == NULL) return;
	if (!advance_param->ctrl_mix.bit.multi_inv_off_grid_en) return;

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		if (can_ota_status[node_id].period_cmd_delay > 0) continue;				//CAN-OTA返回

		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			if (!Inv_can[node_id].inv_data[i].online) continue;

			/* 在并机模式下检测到底层逆变的AC电表开启时主动关闭该逆变设备的电表开关 */
			inv_set01_struct *inv_set01 = &Inv_can[node_id].inv_data[i].inv_set01;
			if (inv_set01->ctrl_meter_enable != 0)
			{
				ESP_LOGW(TAG, "close inv(%d-%d) ac meter switch", node_id, i);
				uint8_t payload[] = {0};
				CanSetTarget(node_id, 0, INV_TYPE_CONFIG01_1BH, offsetof(inv_set01_struct, ctrl_meter_enable), payload, sizeof(payload), INV_CAN_ADDR+i);
				vTaskDelay(pdMS_TO_TICKS(20));
			}
		}
	}
}

/*
can单帧tx ctrl
9-同步下级主动上报时间戳

可能下级不用
本帧可以用于IOT的心跳帧，1分钟周期发送
*/
static void inv_sync_frame_send(void)
{
	#define SYNC_FRAME_PERIOD		60000

	static uint32_t pre_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if ((pre_time != 0) && ((now_time - pre_time) < SYNC_FRAME_PERIOD)) return;

	/* 并机开关打开时才进行逆变同步帧下发 */
	inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
	if(advance_param == NULL) return;
	if (!advance_param->ctrl_mix.bit.multi_inv_off_grid_en) return;

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		if (can_ota_status[node_id].period_cmd_delay > 0) continue;				//CAN-OTA返回

		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			/* 该总线有逆变设备在线就下发同步帧 */
			if (Inv_can[node_id].inv_data[i].online)
			{
				pre_time = now_time;
				can_ctrl_t can_ctrl = {0};
				can_ctrl.cmd = CAN_CTRL_CMD_SYNC_TIME_BASE;
				can_overall_ctrl(node_id, &can_ctrl);							// 下发同步帧
				break;
			}
		}
	}
}

/*
can单帧tx ctrl
ID:0x08FAFFXX	IOT->ALL	99	99	产品型号(ASCII码)的代号分配_L	产品型号(ASCII码)的代号分配_H	预留	预留	预留	预留

windy 20240704新增
心跳帧，用于多IOT通讯在线检测，周期30s

*/
#define IOT_INTERNAL_TYPE_CODE	3005//（逆变器内部）内置IOT
static void Iot_Heartbeat_frame_Send(void)
{
#ifdef FUNC_ONECLICK_UPGRADE_EN
	if (Can_OneClickState_Get()) {
		return;
	}
#endif

	static uint32_t pre_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if ((pre_time != 0) && ((now_time - pre_time) < HEART_BEAT_FRAME_PERIOD_MS)) return;

	pre_time = now_time;
	can_ctrl_t can_ctrl = {0};
	can_ctrl.data[0] =99;
	can_ctrl.data[1] =STEP_CAN_PARALLEL_FINISH;//99;
	can_ctrl.data[2] =IOT_INTERNAL_TYPE_CODE&0xFF;//产品型号(ASCII码)的代号分配_L
	can_ctrl.data[3] =(IOT_INTERNAL_TYPE_CODE>>8)&0xFF;//产品型号(ASCII码)的代号分配_H


	/* 填充CANID数据结构 */
	IdStruct canid;
	canid.bit.src = esp_canbus_myself_address();
	canid.bit.dst = CAN_BROADCAST_ADDRESS;
	canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
	canid.bit.page = CAN_CTRL_FRAME_PAGE;
	canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

	/* 发送单帧CAN数据 */
	if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t))) //fail
	{
//		return -1;
	}
}

static void sys_inv_version_check(void)
{
	inv_about_struct inv_base_ver = {0};

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		if (can_ota_status[node_id].period_cmd_delay > 0) continue;				//CAN-OTA返回

		for (int i = 0; i < INV_MAX_NUM; i++)
		{
			if (Inv_can[node_id].inv_data[i].online)
			{
				inv_about_struct *pdata = &Inv_can[node_id].inv_data[i].inv_about;
				if (pdata->software_total == 0) continue;

				if (inv_base_ver.software_total == 0)							// 取一台设备的版本作为基础版本
				{
					memcpy(&inv_base_ver, pdata, sizeof(inv_about_struct));
					continue;
				}

				for (int m = 0; m < inv_base_ver.software_total; m++)			// 后续设备与基础版本进行比较
				{
					for (int j = 0; j < pdata->software_total; j++)
					{
						if (inv_base_ver.soft[m].type == pdata->soft[j].type)	// 类型相同的设备才进行版本比较
						{
							if (inv_base_ver.soft[m].version != pdata->soft[j].version)
							{
								g_device_data.inv_ver_diff = 1;
								return;
							}
						}
					}
				}
			}
		}
	}

	g_device_data.inv_ver_diff = 0;
}

static void sys_pack_version_check(void)
{
	pack_about_struct pack_base_ver = {0};

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		if (can_ota_status[node_id].period_cmd_delay > 0) continue;				//CAN-OTA返回

		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))
			{
				pack_about_struct *pdata = &Inv_can[node_id].pack_data[i].pack_about;
				if (pdata->software_total == 0) continue;

				if (pack_base_ver.software_total == 0)							// 取一台设备的版本作为基础版本
				{
					memcpy(&pack_base_ver, pdata, sizeof(pack_about_struct));
					continue;
				}

				for (int m = 0; m < pack_base_ver.software_total; m++)			// 后续设备与基础版本进行比较
				{
					for (int j = 0; j < pdata->software_total; j++)
					{
						if (pack_base_ver.soft[m].type == pdata->soft[j].type)	// 类型相同的设备才进行版本比较
						{
							if (pack_base_ver.soft[m].version != pdata->soft[j].version)
							{
								g_device_data.pack_ver_diff = 1;
								ESP_LOGW(TAG,"pack_base_ver.soft[%d].version:%lu,pdata->soft[%d].version:%lu",m,pack_base_ver.soft[m].version,j,pdata->soft[j].version);
								return;
							}
						}
					}
				}
			}
		}
	}

	g_device_data.pack_ver_diff = 0;
}

static void sys_version_check(void)
{
	// #define VER_CHECK_PERIOD		10000
	#define VER_CHECK_PERIOD		1000

	static uint32_t last_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if ((now_time - last_time) < VER_CHECK_PERIOD) return;
	last_time = now_time;

	sys_inv_version_check();
	sys_pack_version_check();
}

/**
 * @brief 找到第一个在线的设备序号，用于 “汇总假值参考”
 *
 * @return 无
 */
void Inv_Pack_First_online_Index_Check(void)
{
	uint8_t flag=0;
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < INV_MAX_NUM; i++)
		{
			if(Inv_can[node].inv_data[i].online)
			{
				reals.online_X_inv_index = node;
				reals.online_Y_inv_index = i;
				flag=1;
//				online_main_node = node;
//				online_sub_node = i;
				break;

			}
		}
		if(flag)
			break;

	}

	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			if(Inv_can[node].pack_data[i].online)
			{
				reals.online_X_bat_index = node;
				reals.online_Y_bat_index = i;
				break;
			}
		}

	}


}

/*
逆变器在线，二维数组
当前仅使用一维

return:
1-有设备在线
0-没设备在线

*/
static int inv_is_online(int inv_id)
{
//	if (Inv_can[inv_id].inv_data[0].online)
//	{
//		return 1;
//	}
//	else
//	{
//		return 0;
//	}
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		if (Inv_can[node].inv_data[inv_id].online)
		{
			return 1;
		}
	}

}

static int pack_is_online(int pack_id)
{
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		if (Inv_can[node].pack_data[0].pack_announce.online & (1 << pack_id)) {
			return 1;
		}
	}

	return 0;
}
void Can_Beta_Tx_Block_Timeout_Check(void)//10ms cycle
{

	if(reals.Can_beta_block_send_timeout_Cnt > 0)
	{
		reals.Can_beta_block_send_timeout_Cnt --;
//		ESP_LOGI(TAG, "reals.Can_beta_block_send_timeout_Cnt	=%d",reals.Can_beta_block_send_timeout_Cnt);

	}

}

//#define FAST_POLL_STEP_INV				2//
//#define FAST_POLL_STEP_PACK				1//
//
//#define SLOW_POLL_STEP_INV				2//
//#define SLOW_POLL_STEP_PACK				1//
//
//#define INV_ABOUT_POLL_STEP				1//
//
//#define IOT_POLL_STEP				1//

/*
step查询，非阻塞

Can Beta多字节查询
快速短周期部分
首次上电，优先从INV CAN获取RTC时间

*/
static uint8_t power_on_get_RTC_poll_Inv_step(void)
{
	static uint32_t inv_timing = 0;
	static uint16_t Cnt_bigcycle = 0;
	static uint16_t Cnt = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t z = 0;

	uint16_t STEP_X_MAX=	DEV_MAIN_NODE_MAX;//最外for循环,
	uint16_t STEP_Y_MAX=	INV_MAX_NUM;//PACK_MAX_NUM ;//
	uint16_t STEP_Z_MAX=	1;//最内for循环
	uint8_t rtn = 0xFF;

	if((1 == reals.st_FlagTime.bits.b1FlagSys1s_can_hi_level)
		&&(0 == reals.rtc_flag.sBit.RTC_valid_from_CAN))
	{

		//每次循环仅执行一次
		x = (Cnt/(STEP_Y_MAX*STEP_Z_MAX))%STEP_X_MAX;
		y = (Cnt/STEP_Z_MAX)%STEP_Y_MAX;
		z = Cnt%STEP_Z_MAX;
		/* 轮询所有PACK节点数据并上报到服务器,每次上传一台PACK设备数据 */


//		if (Inv_can[x].inv_data[y].online)	// 检查当前设备是否在线
		{
			CanGetTarget(x, 0, INV_TYPE_CONFIG00_1AH, 0, 0, INV_CAN_ADDR + x*INV_MAX_NUM +y);
			reals.Can_beta_block_type =INV_TYPE_CONFIG00_1AH;
		// ESP_LOGI(TAG, "fast_data_poll_Inv_step ,reals.Can_beta_block_type=0x%02x, source id= 0x%02x", reals.Can_beta_block_type, INV_CAN_ADDR+inv_poll_id);// +i
		}

		if(++Cnt >= (STEP_X_MAX*STEP_Y_MAX*STEP_Z_MAX))//一个大循环执行完毕；
		{
			Cnt=0;
			rtn=0;
			return rtn;

		}
		else
		{
			rtn=1;
		}

	}

	return rtn;
}




/*
此结构体决定 多字节查询的 所有报文时序、周期、优先级
*/
void can_data_poll_fast_from_modbus(void) //1s cycle
{
	if((reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_FINISH) || (reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_AFTER))
	{
		for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
		{
			if(reals.can_poll[i].modbus_Rd_cnt > 0)
			{
				reals.can_poll[i].modbus_Rd_cnt--;
			}

			if(reals.can_poll[i].modbus_Rd_cnt_last != reals.can_poll[i].modbus_Rd_cnt)
			{
				if((0 == reals.can_poll[i].modbus_Rd_cnt_last)
					&&(0 != reals.can_poll[i].modbus_Rd_cnt))
				{
					reals.can_poll[i].Cnt_bigcycle =0;

				}
				reals.can_poll[i].modbus_Rd_cnt_last = reals.can_poll[i].modbus_Rd_cnt;

			}

		}

	}



}


/*
此结构体决定 多字节查询的 所有报文时序、周期、优先级
*/
void can_data_poll_init(void)
{
	uint8_t index = 0;

	//0
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_BASE_11H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =FAST_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//1
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_DATA_17H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =FAST_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	reals.can_poll[index].poll_ack_flag	=CAN_ACK_INV_DATA_17H;
	index++;

	//2
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_LINE_12H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//3
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_PV_14H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//4
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_GRID_15H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//5
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_LOAD_16H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	reals.can_poll[index].poll_ack_flag	=CAN_ACK_INV_LOAD_16H;
	index++;

	//6
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_METER_18H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//7
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_GEN_19H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//8
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_CONFIG00_1AH;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//9
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_CONFIG01_1BH;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//10
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_CONFIG03_1DH;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//11
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_WIFI_REPORT_24H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//12
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_WIFI_PARAM_25H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//13
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_CERT_27H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

	//14
	reals.can_poll[index].can_dev =CAN_DEV_PACK;
	reals.can_poll[index].can_type = PACK_TYPE_BASE_51H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =PACK_MAX_NUM;
	index++;

	//15
	reals.can_poll[index].can_dev =CAN_DEV_PACK;
	reals.can_poll[index].can_type = PACK_TYPE_EXTEND_52H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =PACK_MAX_NUM;
	index++;

	//16
	reals.can_poll[index].can_dev =CAN_DEV_PACK;
	reals.can_poll[index].can_type = PACK_TYPE_ABOUT_54H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =PACK_MAX_NUM;
	index++;

	//17
	reals.can_poll[index].can_dev =CAN_DEV_PACK;
	reals.can_poll[index].can_type = PACK_TYPE_CONFIG_55H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =PACK_MAX_NUM;
	index++;

	//18
	reals.can_poll[index].can_dev =CAN_DEV_PACK;
	reals.can_poll[index].can_type = PACK_TYPE_DEBUG1_56H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =PACK_MAX_NUM;
	index++;

	//19
	reals.can_poll[index].can_dev =CAN_DEV_PACK;
	reals.can_poll[index].can_type = PACK_TYPE_BMUSTATE_57H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =PACK_MAX_NUM;
	index++;

	//20
	reals.can_poll[index].can_dev =CAN_DEV_IOT;
	reals.can_poll[index].can_type = IOT_TYPE_INFO_01H;
	reals.can_poll[index].range_level =0;
	reals.can_poll[index].period =SLOW_POLL_IOT;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =IOT_MAX_NUM;
	index++;

	//21
	reals.can_poll[index].can_dev =CAN_DEV_IOT;
	reals.can_poll[index].can_type = IOT_TYPE_SET_02H;
	reals.can_poll[index].range_level =2;
	reals.can_poll[index].period =SLOW_POLL_IOT;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =IOT_MAX_NUM;
	index++;

	//22
	reals.can_poll[index].can_dev =CAN_DEV_INV;
	reals.can_poll[index].can_type = INV_TYPE_ABOUT_13H;
	reals.can_poll[index].range_level =3;
	reals.can_poll[index].period =INV_ABOUT_DATA_POLL_INTV;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =INV_MAX_NUM;
	index++;

////////23
	reals.can_poll[index].can_dev =CAN_DEV_AC_HUB;
	reals.can_poll[index].can_type = MODULE_TYPE_AC_HUB_INFO_42H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =AC_HUB_DATA_POLL;
	reals.can_poll[index].max_x =1;//DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =AC_HUB_MAX_NUM;
	reals.can_poll[index].poll_ack_flag	=CAN_ACK_AC_HUB_INFO_42H;
	index++;

	//24
	reals.can_poll[index].can_dev =CAN_DEV_DC_HUB;
	reals.can_poll[index].can_type = MODULE_TYPE_DC_HUB_INFO_41H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =DC_HUB_DATA_POLL;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =DC_HUB_MAX_NUM;
	index++;

	//25
	reals.can_poll[index].can_dev =CAN_DEV_AC_HUB;
	reals.can_poll[index].can_type = MODULE_TYPE_DC_AC_HUB_SET_40H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =AC_HUB_DATA_POLL;
	reals.can_poll[index].max_x =1;
	reals.can_poll[index].max_y =AC_HUB_MAX_NUM;
	index++;

	//26
	reals.can_poll[index].can_dev =CAN_DEV_D400S;
	reals.can_poll[index].can_type = MODULE_TYPE_D400S_INFO_48H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =D400S_HUB_DATA_POLL;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =D400S_MAX_NUM;
	index++;

	//27
	reals.can_poll[index].can_dev =CAN_DEV_D400S;
	reals.can_poll[index].can_type = MODULE_TYPE_D400S_SET_49H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =D400S_HUB_DATA_POLL;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =D400S_MAX_NUM;
	index++;

	//28
	reals.can_poll[index].can_dev =CAN_DEV_D400S;
	reals.can_poll[index].can_type = IOT_TYPE_INFO_01H;
	reals.can_poll[index].range_level =10;
	reals.can_poll[index].period =D400S_HUB_DATA_POLL;
	reals.can_poll[index].max_x =DEV_MAIN_NODE_MAX;
	reals.can_poll[index].max_y =D400S_MAX_NUM;
	index++;


	if(index != MAX_NUM_CAN_TYPE)
	{
		 ESP_LOGI(TAG, "can_data_poll_init ,index=%d ",index);//
	}


	for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
	{
		reals.can_poll[i].run_flag =0;
		reals.can_poll[i].time_old =0;
		reals.can_poll[i].Cnt_bigcycle =0;
		reals.can_poll[i].Cnt_smallcycle =0;
		reals.can_poll[i].modbus_Rd_cnt =0;
		reals.can_poll[i].modbus_Rd_cnt_last =0;

	}

}


#define  CHANGE_CAN_TYPE_BY_FINISH_THIS	3//
#define  CHANGE_CAN_TYPE_BY_TIME_NOT_READY	5//

/*
setmask:等待轮询结果的对应掩码位
flag:1:设置对应掩码位
*/
void can_data_poll_mask_set(uint32_t uMask,uint8_t flag)
{
	ESP_LOGI(TAG,"can_data_poll_mask_set mask:%lu flag:%d",uMask,flag);
	if(flag)
	{
		reals.poll_ack_mask|=uMask;
	}else{
		reals.poll_ack_mask&=~(uMask);
	}
}

/*
step查询，非阻塞

Can Beta多字节查询
快速短周期部分

三维循环改单次执行
return:
3-一个大循环执行完毕；
1-一个大循环正在执行；
5-本次循环时间未到，需要改变查询type,继续查其他type；

0-其他情况的不执行
*/
static uint8_t can_data_poll_common_step(can_poll_struct *can_poll )
{
//	static uint16_t Cnt = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t rtn = 0xFF;

	if((0 == can_poll->max_x)
		||(0 == can_poll->max_y)
		)//退出循环，不再轮询，优先切到透传写
	{
		  ESP_LOGI(TAG, "can_data_poll_common_step, ERROR !!!!");// +i

		   abort();
	}

	if((0 != reals.Can_beta_block_send_timeout_Cnt)
		||(reals.BLE_WIFI_to_Can_Cnt)
		)//退出循环，不再轮询，优先切到透传写
	{
		//ESP_LOGI(TAG,"Can_beta_block_send_timeout_Cnt:%d,BLE_WIFI_to_Can_Cnt:%d",reals.Can_beta_block_send_timeout_Cnt,reals.BLE_WIFI_to_Can_Cnt);
		return 0;//block间隔延时未到，则不继续发送
	}
	for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
	{
		if((reals.can_poll[i].run_flag)
			&&(reals.can_poll[i].range_level >= can_poll->range_level)//高优先级发送任务可以挤占低优先级任务 >=
			&&(reals.can_poll[i].can_type != can_poll->can_type)//test
//			&&( != i)
		)
		{
			ESP_LOGI(TAG,"reals.can_poll[%d].run_flag:%d",i,reals.can_poll[i].run_flag);
			ESP_LOGI(TAG,"range_level:%d,%d",reals.can_poll[i].range_level,can_poll->range_level);
			ESP_LOGI(TAG,"can_type:%d,:%d",reals.can_poll[i].can_type,can_poll->can_type);
			return 0;//block间隔延时未到，则不继续发送

		}

	}

	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

	x = (can_poll->Cnt_smallcycle/(can_poll->max_y))%can_poll->max_x;
	y = (can_poll->Cnt_smallcycle)%can_poll->max_y;
	//ESP_LOGI(TAG,"x:%d,y:%d,now_time:%lu,time_old:%lu,period:%d,Cnt_bigcycle:%d,run_flag:%d",x,y,now_time,can_poll->time_old,can_poll->period,can_poll->Cnt_bigcycle,can_poll->run_flag);
	if((((now_time > can_poll->time_old)&&((now_time - can_poll->time_old) >= can_poll->period))
		||((now_time < can_poll->time_old)&&((can_poll->time_old - now_time) >= can_poll->period))//周期查询
		||(0 == can_poll->Cnt_bigcycle))//首次启动快速获取
		&&(0 == can_poll->run_flag)
		&&(0 == x)
		&&(0 == y)
		)//大周期切换
	{
		can_poll->time_old = now_time;
		can_poll->run_flag =1;
	}

	if(1 == can_poll->run_flag)
	{
		reals.Can_beta_block_send_timeout_Cnt = CAN_BLOCK_RX_TIME_OUT_SLOW;//tbd
		//ESP_LOGE(TAG, "Can_beta_block_send_timeout_Cnt HUANGJI CCC:%d", reals.Can_beta_block_send_timeout_Cnt);
	}
	else
	{
		rtn =CHANGE_CAN_TYPE_BY_TIME_NOT_READY;
		return rtn;//block间隔延时未到，则不继续发送
	}
	 //ESP_LOGI(TAG, "fast_data_poll_Inv_step ,reals.Can_beta_block_fast_poll_flag=%d, now_time= %0ld", reals.Can_beta_block_fast_poll_flag, now_time);// +i


	do{

		//每次循环仅执行一次
		x = (can_poll->Cnt_smallcycle/(can_poll->max_y))%can_poll->max_x;
		y = (can_poll->Cnt_smallcycle)%can_poll->max_y;
		/* 轮询所有PACK节点数据并上报到服务器,每次上传一台PACK设备数据 */
		// ESP_LOGW(TAG,"Inv_can[%d].dc_hub_data[%d].online=%d",x,y,Inv_can[x].dc_hub_data[y].online);
		// ESP_LOGW(TAG,"Inv_can_mix.ac_hub_data[0].online=%d",Inv_can_mix.ac_hub_data[0].online);
		// 检查当前设备是否在线
		{
			if((CAN_DEV_INV == can_poll->can_dev)&&(Inv_can[x].inv_data[y].online))
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, INV_CAN_ADDR + x*INV_MAX_NUM +y);
				reals.Can_beta_block_type =can_poll->can_type;

			}
			else if((CAN_DEV_PACK == can_poll->can_dev)&&(Inv_can[x].pack_data[0].pack_announce.online & ((uint16_t)1 << y)))
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, PACK_CAN_ADDR + x*PACK_MAX_NUM +y);
				reals.Can_beta_block_type =can_poll->can_type;

			}
			else if((CAN_DEV_IOT == can_poll->can_dev )&&(Inv_can[x].iot_data[y].online))
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, IOT_CAN_ADDR + x*IOT_MAX_NUM +y);
				reals.Can_beta_block_type =can_poll->can_type;

			}
			else if((CAN_DEV_AC_HUB == can_poll->can_dev )&&(Inv_can_mix.ac_hub_data[0].online))
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, AC_HUB_CAN_ADDR);
				reals.Can_beta_block_type =can_poll->can_type;

			}
			else if((CAN_DEV_DC_HUB == can_poll->can_dev )&&(Inv_can[x].dc_hub_data[y].online))
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, DC_HUB_CAN_ADDR + x*DC_HUB_MAX_NUM +y);
				reals.Can_beta_block_type =can_poll->can_type;

			}
			else if((CAN_DEV_D400S == can_poll->can_dev )&&(Inv_can[x].d400s_data[y].online))
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, D400S_CAN_ADDR + x*D400S_MAX_NUM +y);
				reals.Can_beta_block_type =can_poll->can_type;
			}
			else
			{
				 //ESP_LOGI(TAG, "can_data_poll_common_step NULL");
			}



		//  ESP_LOGI(TAG, "can_data_poll_common_step ,reals.Can_beta_block_type=0x%02x, x= %d,y= %d", reals.Can_beta_block_type, x,y);//
		}

		if(++can_poll->Cnt_smallcycle >= (can_poll->max_x*can_poll->max_y))//一个大循环执行完毕；
		{
			can_poll->Cnt_smallcycle=0;
			rtn=CHANGE_CAN_TYPE_BY_FINISH_THIS;
//			reals.one_poll_finish =rtn;
			can_poll->run_flag =0;//FAST_POLL_STEP_PACK;

			if(++can_poll->Cnt_bigcycle >= 0xFF00)
			{
				can_poll->Cnt_bigcycle= 0xFF00;

			}
			return rtn;

		}
		else
		{
			rtn=1;
//			reals.one_poll_finish =rtn;
		}
		//每次循环仅执行一次
		x = (can_poll->Cnt_smallcycle/(can_poll->max_y))%can_poll->max_x;
		y = (can_poll->Cnt_smallcycle)%can_poll->max_y;

	}
	while(!(((CAN_DEV_INV == can_poll->can_dev)&&(Inv_can[x].inv_data[y].online))
		||((CAN_DEV_PACK == can_poll->can_dev)&&(Inv_can[x].pack_data[0].pack_announce.online & ((uint16_t)1 << y)))
		||((CAN_DEV_IOT == can_poll->can_dev )&&(Inv_can[x].iot_data[y].online))
		||((CAN_DEV_AC_HUB == can_poll->can_dev )&&(Inv_can_mix.ac_hub_data[0].online))
		||((CAN_DEV_DC_HUB == can_poll->can_dev )&&(Inv_can[x].dc_hub_data[y].online))
		||((CAN_DEV_D400S == can_poll->can_dev )&&(Inv_can[x].d400s_data[y].online))
		));//预判下次不在线，要直接下一步


	return rtn;
}

void can_data_poll_index_set(int8_t index)
{
	uint8_t i=0;
	static uint8_t lastIdx=0;
	for(i=0;i<CAN_DATA_POLL_INDEX_MAXNUM;i++)
	{
		if((-1) != reals.fast_to_Can_poll_index[i])
		{
			continue;
		}
		else if( reals.fast_to_Can_poll_index[i]==index)
		{
			ESP_LOGI(TAG,"same index:%d",i);
			lastIdx=i;
			break;
		}
		else
		{
			reals.fast_to_Can_poll_index[i]=index;
			lastIdx=i;
			break;
		}
	}
	if(i>=2)
	{
		lastIdx=(CAN_DATA_POLL_INDEX_MAXNUM-1)-lastIdx;	//lastinde 最大为CAN_DATA_POLL_INDEX_MAXNUM-1
		reals.fast_to_Can_poll_index[lastIdx]=index;
	}
	ESP_LOGI(TAG,"set ,reals.fast_to_Can_poll_index[%d]:%d addcount:%d",lastIdx, reals.fast_to_Can_poll_index[lastIdx],i);
}

static void can_data_poll_all( void )
{
	static uint8_t i = 0;
	static uint8_t i_old = 0;
//	static uint16_t period_old=0xFF;
	static int8_t sfast_to_Can_poll_index[CAN_DATA_POLL_INDEX_MAXNUM] ={-1,-1};
	//memset(sfast_to_Can_poll_index,-1,sizeof(sfast_to_Can_poll_index));
	uint8_t rtn = 0;
	rtn = can_data_poll_common_step(&reals.can_poll[i]);

	//ESP_LOGI(TAG,"can_data_poll_all i:%d  rtn:%d",i,rtn);
	if((CHANGE_CAN_TYPE_BY_FINISH_THIS == rtn)
		||(CHANGE_CAN_TYPE_BY_TIME_NOT_READY == rtn))
	{

		for(uint8_t j=0;j<CAN_DATA_POLL_INDEX_MAXNUM;j++)
		{
			//ESP_LOGI(TAG, "sfast_to_Can_poll_index[%d]:%d,reals.fast_to_Can_poll_index[%d]=%d",j,sfast_to_Can_poll_index[j],j,reals.fast_to_Can_poll_index[j]);
			if(sfast_to_Can_poll_index[j] != reals.fast_to_Can_poll_index[j])
			{
				sfast_to_Can_poll_index[j] =reals.fast_to_Can_poll_index[j];
				if((-1) != reals.fast_to_Can_poll_index[j])//active INV_TYPE_BASE_11H
				{
					i_old =i;
					ESP_LOGI(TAG, "can_data_poll_all,reals.fast_to_Can_poll_index[%d]=%d",j,reals.fast_to_Can_poll_index[j]);
					i =reals.fast_to_Can_poll_index[j];
					reals.fast_to_Can_poll_index[j] =-1;
	//					period_old =reals.can_poll[i].period;

					reals.can_poll[i].period =0;//FAST_POLL_INTV;临时加速

					return;
				}
				else
				{
					i =i_old;
					//ESP_LOGI(TAG, "i:%d,can_data_poll_init,reals.fast_to_Can_poll_index[%d]=%d",i,j,reals.fast_to_Can_poll_index[j]);
	//					reals.can_poll[0].period =FAST_POLL_INTV;
					can_data_poll_init();//恢复初始值
				}

			}
		}

		if(++i >= MAX_NUM_CAN_TYPE)
		{
			i=0;
		}
	}


}


static aging_status_t keep_bat_status;
static int keep_flag = 0;

aging_status_t* bat_maintain_status_get(void)
{
	return &keep_bat_status;
}

static void bat_maintain_status_check(void)
{
	bat_maintain_t *old = &g_device_data.iot_dev_node.bat_maintain;
	bat_maintain_t *new = &g_device_data.bk_iot_dev_node.bat_maintain;
	aging_status_t aging_status = g_device_data.pack_dev_node.pack_announce.aging_status;

	/* 电池维护完成后保存维护完成时刻的状态 */
	if ((aging_status.status == 3) || (aging_status.status == 4))
	{
		keep_bat_status = aging_status;		// 电池维护结束后app没有发送清除指令就一直保持维护结束时的状态
		keep_flag = 1;
	}

	/* app发送清除维护状态指令,所有变量清零 */
	if (new->reset == 1)
	{
		new->all = 0;
		old->all = 0;
		keep_flag = 0;
		aging_status.all = 0;
		g_device_data.pack_dev_node.pack_announce.aging_status.all = 0;
		ESP_LOGW(TAG, "clear battery maintain status");
	}

	/* 非电池完成状态使用电池包上传的维护状态 */
	if (keep_flag == 0) {
		keep_bat_status.all = aging_status.all;
	}
}

static void bat_maintain_cmd_check(void)
{
	bat_maintain_t *old = &g_device_data.iot_dev_node.bat_maintain;
	const bat_maintain_t *new = &g_device_data.bk_iot_dev_node.bat_maintain;

	if (old->enable != new->enable)
	{
		can_ctrl_t can_ctrl = {0};
		can_ctrl.cmd = CAN_CTRL_CMD_BATTERY_AGING;
		can_ctrl.battery_aging.param = new->enable;

		for(uint8_t node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
		{
			for(int i = 0; i < PACK_MAX_NUM; i++)
			{
				/* 该总线有PACK设备在线才发送数据 */
				if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))
				{
					can_overall_ctrl(node_id, &can_ctrl);	// 发送电池维护指令
					break;
				}
			}
		}

		old->enable = new->enable;
		ESP_LOGI(TAG, "send battery maintain commamd:%d", new->enable);
	}
}

static void bat_maintain_handle(void)
{
	bat_maintain_cmd_check();
	bat_maintain_status_check();
}

//void iot_lockdev_task(void * pvParameters)
void iot_lockdev_task(void)
{
	#define SUMMARY_PERIOD	1000		//数据汇总周期
	static uint32_t pre_time = 0;
    uint8_t lock_pack_id = 0; 			// 手机锁定时PACK的ID
    uint8_t lock_inv_id = 0;  			// 手机锁定时INV的ID
	static uint8_t step=0;
	static uint8_t sCnt=0;

	if(0 == step)
	{
		step=1;
		g_device_data.app_node_id = 0xff;	// 初始化的时候不锁定任何设备

	}
	else if(1 == step)
	{
		if(++sCnt >= 10)
		{
			sCnt =10;
			step=2;
		}

//			vTaskDelay(pdMS_TO_TICKS(1000));
	}
	else if(2 == step)

    {
		//ESP_LOGI(TAG,"iot_lockdev_task can_ota_status[%d] ota_doing:%d ,pct:%d,isOta:%d,type:%d",0,can_ota_status[0].ota_doing,can_ota_status[0].devStatus[0].pct,
		//can_ota_status[0].devStatus[0].isOta,can_ota_status[0].devStatus[0].type);
		int is_ota = 0;
		for(uint8_t node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++) {
			if (can_ota_status[node_id].period_cmd_delay > 0) {	//CAN-OTA 不查询
				is_ota = 1;
			}
		}
#ifdef FUNC_ONECLICK_UPGRADE_EN
		if (Can_OneClickState_Get()) {
			is_ota = 1;
		}
#endif
		if (is_ota)
		{
//				vTaskDelay(pdMS_TO_TICKS(1000));
			return;
		}

//		iot_can_fault_flag_set();			// CAN通信故障标志设置
//		bat_maintain_handle();				// 电池维护处理

		/* 每隔一段时间汇总APP首页数据与PACK数据
		 * EMS控制策略中需要某些汇总后的数据 */
		uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

		if((now_time - pre_time) >= SUMMARY_PERIOD)
		{
/*
代码的临界段也称为临界区，一旦这部分代码开始执行，则不允许任何中断打断。为确保临界段代码
的执行不被中断，在进入临界段之前须关中断，而临界段代码执行完毕后，要立即开中断

*/

//			TASK_ENTER_CRITICAL();
			data_sum_inv();
			data_sum_pack();
//			data_sum_pack_group();
			// data_sum_ems();
//			TASK_EXIT_CRITICAL();
			pre_time = now_time;
		}

		int max_node = DEV_MAIN_NODE_MAX*INV_MAX_NUM;
		if ((lock_node_id < max_node) && can_ota_status[lock_node_id].period_cmd_delay > 0)	//CAN-OTA 不查询
		{
//				vTaskDelay(pdMS_TO_TICKS(10));
			return;
		}
#ifdef	MODBUS_READ_TRIGER_CAN_READ_ENABLE
//AC380等新架构，逆变器ARM不存储旧协议的历史记录，全改为数据通道
		can_trigger_cmd_read();									// 读取can触发指令
		inv_log_sync_check();									// 逆变历史故障同步检查

#endif
		// const ble_info_t *ble_info = iot_get_ble_info();
		// if (!ble_info->flag.bit.is_connected)					// 蓝牙不在线时退出锁定读取
		// {
		// 	lock_node_id = 0xff;
		// 	vTaskDelay(pdMS_TO_TICKS(20));
		// 	continue;
		// }

#if 0//windy,不启用来自APP的锁定插队CAN读取协议，简化
		lock_node_id = g_device_data.app_node_id;				// 切换锁定节点

		if ((lock_node_id < max_node) &&
			((lock_node_id != g_device_data.app_node_id) || (lock_inv_id != g_device_data.inv_id)))
		{
			lock_inv_id = g_device_data.inv_id;					// 手机APP切换不同逆变包
			CanCmdInvReset();									// APP切换时全部读取一次逆变数据
		}

		if ((lock_node_id < max_node) &&
			((lock_node_id != g_device_data.app_node_id) || (lock_pack_id != g_device_data.pack_id)))
		{
			lock_pack_id = g_device_data.pack_id;				// 手机APP切换不同PACK包
			CanCmdPackReset(); 									// APP切换时全部读取一次PACK数据
		}

		/* 每条指令都有不同的读取时间间隔，根据时间间隔读取锁定设备的数据。
		 * 周期定时器会更新每条指令的剩余时间，无剩余时间时会置位相应指令的expired标志。 */
		if ((lock_node_id < max_node) && Inv_can[lock_node_id].inv_data[lock_inv_id].online) {
			CanPollInvCmd(lock_node_id, g_device_data.inv_id);
		}

		if ((lock_node_id < max_node) && (Inv_can[lock_node_id].pack_data[0].pack_announce.online & (1 << lock_pack_id))) {
			CanPollPackCmd(lock_node_id, g_device_data.pack_id);
		}
#endif
//	        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief 轮询设备数据
 * - 设备主要分为被锁定和未被锁定两种状态
 * - 1、被锁定是指被手机APP指定读取的设备
 * - 2、未锁定是指除锁定以外别的设备
 * - 3、设备分为PACK和逆变等
 * - 4、CAN总线处于OTA期间,不发起查询
 *
 * @param pvParameters 线程参数
 *
 * @return 无
 */
//void iot_polldev_task(void * pvParameters)
void iot_polldev_task(void)
{
static uint8_t step=0;
static uint8_t sCnt=0;


	//ESP_LOGI(TAG,"iot_polldev_task step:%d  sCnt:%d",step,sCnt);
	if(0 == step)
	{
		step=1;
		/* IoT启动后发送指令开启一次逆变与pack数据上报 */
		for (uint8_t node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
		{
			can_ctrl_t can_ctrl = {0};
			can_ctrl.cmd = CAN_CTRL_CMD_EN_PACK_REPORT;
			can_overall_ctrl(node_id, &can_ctrl);

			can_ctrl.cmd = CAN_CTRL_CMD_EN_INV_REPORT;
			can_overall_ctrl(node_id, &can_ctrl);
		}

		can_data_poll_init();
	//	vTaskDelay(pdMS_TO_TICKS(1000));
	}
	else if(1 == step)
	{
		if(++sCnt >= 10)
		{
			sCnt =10;
			step=2;
		}

	}
	else if(2 == step)
    {
		int is_ota = 0;
		for (uint8_t node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
		{
			if (can_ota_status[node_id].period_cmd_delay > 0) {	//CAN-OTA 不查询
				is_ota = 1;
			}
		}
#ifdef FUNC_ONECLICK_UPGRADE_EN
		/* OneClick 静默期间同样视为升级中，冻结离线检测（保持 online_Inv_num） */
		if (Can_OneClickState_Get()) {
			is_ota = 1;
		}
#endif
		if (is_ota)
		{
			reals.ota_happen =1;
//			vTaskDelay(pdMS_TO_TICKS(1000));
			return;
		}
		else
		{
			reals.ota_happen =0;
			gXmodem_Status.firmware_resend_step=0;
		}


		if((1 == reals.Addr_can_master)//多个IOT并机厂家，只允许主IOT发查询报文，其他IOT只监听
		&&((reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_FINISH) || (reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_AFTER)))
		{

//windy 20240819 :区分2个档位，除fast外，其他都是慢；慢的之间可以互斥等待
//			slow_data_poll_Iot_step();
//			inv_about_data_poll_step();					// 逆变ABOUT数据轮询
//			fast_data_poll_Inv_step();						// 快速数据轮询
//			fast_data_poll_Pack_step();						// 快速数据轮询,暂无 fast pack
//			slow_data_poll_Inv_step();						// 慢速数据轮询
//			slow_data_poll_Pack_step();						// 慢速数据轮询


			can_data_poll_all();


			dev_enable_active_report(); 			// 主动上报使能
			inv_on_grid_meter_switch_process(); 	// 并机状态下逆变AC电表开关处理
			inv_sync_frame_send();					// 周期发逆变同步帧,maybe INV not care

			/*时间检查，CAN总线时间有误时同步（1s）*/
            RTC_Update_iot_to_can();

		}
		else if(reals.MCUPoweronCnt < 30)
		{

			power_on_get_RTC_poll_Inv_step();

		}

		if((reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_FINISH) || (reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_AFTER))
		{
			app_ll_param_thread();
		}
		sys_version_check();					// 系统版本异常检测
		// energy_data_handle();				// 能量数据处理
//		vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#ifdef STATE_COUNT
extern void CanLookupTypePosition(uint8_t devId, uint8_t type, uint8_t **ptr, uint32_t *maxlen, rw_cmd_struct **cmd);
#endif

#ifdef	MODBUS_READ_TRIGER_CAN_READ_ENABLE

static void can_broadcast_trigger_read_inv(uint8_t idx)
{
	ESP_LOGW(TAG, "read inv trigger cmd through broadcast mode, cmd type: %02x", g_cmd_array_inv[idx].abstract->type);
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			if (Inv_can[node_id].inv_data[i].online)		// 该总线有逆变设备在线才发送数据
			{
				CanGetTarget(node_id, 0, g_cmd_array_inv[idx].abstract->type, 0, g_cmd_array_inv[idx].abstract->max_len, CAN_BROADCAST_ADDRESS);
				break;
			}
		}
	}

	vTaskDelay(pdMS_TO_TICKS(POLL_CMD_INTV));
}

static void can_unicasst_trigger_read_inv(uint8_t idx, uint8_t md_addr)
{
	if (!IS_READ_INV_DATA(md_addr)) return;

	uint8_t node = (md_addr - MD_INV_ADDR_START) / INV_MAX_NUM;
	uint8_t dev_idx = (md_addr - MD_INV_ADDR_START) % INV_MAX_NUM;
	uint8_t dev_id = dev_idx + INV_CAN_ADDR;

	if (Inv_can[node].inv_data[dev_idx].online)
	{
		ESP_LOGW(TAG, "read inv trigger cmd through unicasst mode, node_id: %d, dev_id: %d, cmd type: %02x",
				 node, dev_id, g_cmd_array_inv[idx].abstract->type);
		CanGetTarget(node, 0, g_cmd_array_inv[idx].abstract->type, 0, g_cmd_array_inv[idx].abstract->max_len, dev_id);
	}

	vTaskDelay(pdMS_TO_TICKS(POLL_CMD_INTV));
}

static void can_poll_inv_trigger_cmd(uint8_t idx, uint8_t md_addr)
{
	if (md_addr == 0x00)
	{						// 以广播方式读取触发指令
		can_broadcast_trigger_read_inv(idx);
	}
	else
	{										// 以单播方式读取触发指令
		can_unicasst_trigger_read_inv(idx, md_addr);
	}
}

/*
modbus读取触发 CAN 多字节（插队）读取

下级历史记录相关
*/
static void can_trigger_cmd_read(void)
{
	if (trigger_read_addr == 0xff) return;
	uint8_t md_addr = trigger_read_addr;
	trigger_read_addr = 0xff;

	for (int i = 0; i < inv_cmd_count; i++)
	{
		/* 读取逆变的触发指令 */
		if (g_cmd_array_inv[i].abstract->type == trigger_read_type)
		{
			can_poll_inv_trigger_cmd(i, md_addr);
			break;
		}
	}

	/* 触发指令回读完成后汇总一次数据 */
	TASK_ENTER_CRITICAL();
	data_sum_inv();
	data_sum_pack();
	//data_sum_ems();
	TASK_EXIT_CRITICAL();
}
#endif
/*
can beta 下级逆变器本地历史记录存储的读取：


*/
static void inv_log_sync_check(void)
{
	static uint32_t last_time[DEV_MAIN_NODE_MAX][INV_MAX_NUM] = {0};

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			if (Inv_can[node_id].inv_data[i].online)
			{
				/* 查询设置的历史页与回读的历史页是否一致 */
				uint16_t set_page = Inv_can[node_id].inv_data[i].inv_set00.ctrl_log_page;
				uint16_t get_page = Inv_can[node_id].inv_data[i].inv_log.current_page_seq;
				if (set_page == get_page)
				{
					last_time[node_id][i] = 0;//只有查询不同页面才新读取;windy 有问题，如果相同页面，但是下级设备记录更新， 则又更新不匹配
					continue;
				}

				uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
				if (last_time[node_id][i] == 0) //windy无法进入此条件
				{
					last_time[node_id][i] = now_time;
				}
				else
				{
					/* 当设置的历史页与回读的历史页不一致时读取逆变的历史数据 */
					if ((now_time - last_time[node_id][i]) > 500)
					{
						ESP_LOGW(TAG, "read inv(%d_%d) log, cmd type: %02x, now log page: %d, expected log page: %d",
								 node_id, i, INV_TYPE_LOG_20H, get_page, set_page);
						last_time[node_id][i] = now_time;
						CanGetTarget(node_id, 0, INV_TYPE_LOG_20H, 0, 0, INV_CAN_ADDR+i);
					}
				}
			}
		}
	}
}
//
///*
//CAN 数组的元素偏移，从0开始
//*/
//static void CanPollInvCmd(uint8_t node_id, uint8_t invId)
//{
//    uint8_t i = 0;
//    uint8_t max = inv_cmd_count;
//	if(node_id >= DEV_MAIN_NODE_MAX) return;
//
//    while (max--)
//	{
//        i++;
//        i = i % inv_cmd_count;
//
//		/* g_abstract_array_inv[14](0x20)、g_abstract_array_inv[15](0x21)、g_abstract_array_inv[16](0x22)
//		 * 是触发读取指令，不需要周期读取。当触发指令需要读取时，在CanCmdActivte中激活后才会在该函数中被读取 */
//        if (g_cmd_array_inv[i].is_expire != 1) { // 命令到期
//            continue ;
//        }
//
//        g_cmd_array_inv[i].is_expire = 0;
//        uint8_t devId = g_cmd_array_inv[i].abstract->addr;//CAN ID源地址的基地址
//
//		/* 以下类型指令不用主动读取 */
//        if ((g_cmd_array_inv[i].abstract->type == INV_TYPE_ANNOUNCE_10H) ||
//			(g_cmd_array_inv[i].abstract->type == INV_TYPE_CONFIG02_1CH) ||
//			(g_cmd_array_inv[i].abstract->type == INV_TYPE_WIFI_23H)) continue;
//
//		/* 逆变触发指令不在此处读取 */
//		if ((g_cmd_array_inv[i].abstract->type == INV_TYPE_LOG_20H) ||
//			(g_cmd_array_inv[i].abstract->type == INV_TYPE_ALL_ENERGY_21H) ||
//			(g_cmd_array_inv[i].abstract->type == INV_TYPE_CURR_ENERGY_22H)) continue;
//
//		ESP_LOGI(TAG, "lock read inv node:%d, devid:%02x, type:%02x", node_id, devId+invId, g_cmd_array_inv[i].abstract->type);
//        if (CanGetTarget(node_id, 0, g_cmd_array_inv[i].abstract->type, 0, g_cmd_array_inv[i].abstract->max_len, (devId + invId))) { /* 发送命令读取逆变数据 */
//        #ifdef STATE_COUNT
//            uint8_t *ptr;
//            uint32_t len;
//            rw_cmd_struct *rw_cmd;
//            CanLookupTypePosition((devId + invId), g_cmd_array_inv[i].abstract->type, &ptr, &len, &rw_cmd);
//            if (rw_cmd)
//                rw_cmd->send_cnt++;
//        #endif
//            // ESP_LOGI (TAG, "invId: 0x%x, type 0x%x", (devId + invId), g_cmd_array_inv[i].abstract->type);
//            if (g_cmd_array_inv[i].is_auto == false) {
//                if (g_cmd_array_inv[i].used_cnt < g_cmd_array_inv[i].usable_cnt) {
//                    g_cmd_array_inv[i].used_cnt++; // 记录命令发送次数
//                }
//            }
//
//            vTaskDelay(pdMS_TO_TICKS(POLL_CMD_INTV));
//        }
//    }
//}
//
//static void CanPollPackCmd(uint8_t node_id, uint8_t packId)
//{
//    uint8_t i = 0;
//    uint8_t max = pack_cmd_count;
//	if(node_id >= DEV_MAIN_NODE_MAX) return;
//
//    while (max--)
//	{
//        i++;
//        i = i % pack_cmd_count;
//
//        if (g_cmd_array_pack[i].is_expire != 1) { // 命令到期
//            continue ;
//        }
//        g_cmd_array_pack[i].is_expire = 0;
//
//        uint8_t devId = g_cmd_array_pack[i].abstract->addr;
//        if (g_cmd_array_pack[i].abstract->type == PACK_TYPE_ANNOUNCE_50H) {
//            continue;
//        }
//
//		ESP_LOGI(TAG, "lock read pack node:%d, devid:%02x, type:%02x", node_id, devId+packId, g_cmd_array_inv[i].abstract->type);
//        if (CanGetTarget(node_id, 0, g_cmd_array_pack[i].abstract->type, 0, g_cmd_array_pack[i].abstract->max_len, (devId + packId))) {
//            #ifdef STATE_COUNT
//            uint8_t *ptr;
//            uint32_t len;
//            rw_cmd_struct *rw_cmd;
//            CanLookupTypePosition((devId + packId), g_cmd_array_pack[i].abstract->type,
//                                    &ptr, &len, &rw_cmd);
//            if (rw_cmd)
//                rw_cmd->send_cnt++;
//            #endif
//            // ESP_LOGI (TAG, "packId: 0x%x, type 0x%x", (devId + packId), g_cmd_array_pack[i].abstract->type);
//            if (g_cmd_array_pack[i].is_auto == false) {
//                if (g_cmd_array_pack[i].used_cnt < g_cmd_array_pack[i].usable_cnt) {
//                    g_cmd_array_pack[i].used_cnt++; // 记录命令发送次数
//                }
//            }
//            vTaskDelay(pdMS_TO_TICKS(POLL_CMD_INTV));
//            // break ;
//        }
//    }
//}

static void do_operate(uint8_t do_set)
{
	#define DO_PIN 	26
	static uint8_t last_do_sta = 0xff;
	if (last_do_sta != do_set)
	{
		ESP_LOGW(TAG, "set DO(digital output) %s", (do_set)?("enable"):("disable"));
		(do_set) ? (gpio_set_level(DO_PIN, 1)) : (gpio_set_level(DO_PIN, 0));
		last_do_sta = do_set;
	}
}

/*
单帧接收解析

接收并解析下层设备周期广播的数据

*/
static void CanListenBus(node_info_t *node, twai_message_t *frame)
{
    IdStruct canId;
    uint8_t index = 0;
    // static uint8_t count = 0;
    static OtaBoot prev_boot_info;
	uint8_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;

	uint8_t addr_source = 0;

    if (!frame)
		return ;

    uint32_t offset;
    uint8_t *pdata;

    canId.all = frame->identifier;//can rx ID
	pf_fun =(canId.all>>16)&0xFF;
	Id_p = (canId.all>>26)&0x7;
	Id_edp = (canId.all>>25)&0x1;
	Id_dp = (canId.all>>24)&0x1;
	addr_source =canId.all&0xFF;

	OtaBoot *payload = (OtaBoot *)frame->data;

    switch (canId.bit.funcode)
    {
        /* CAN-OTA数据帧 */
        case 0x70: /* CAN-OTA准备帧,设备进入静默 */
        case 0x71: /* CAN-OTA开始帧 */
        case 0x72: /* CAN-OTA开始响应 */
        case 0x73: /* CAN-OTA数据开始 */
        case 0x74: /* CAN-OTA数据 */
        case 0x75: /* CAN-OTA数据响应 */
        case 0x76: /* CAN-OTA结束帧 */
			can_ota_status[node->node_id].period_cmd_delay = 6000;
			// can_ota_status[node->node_id].ota_doing = 1;	//modify by yjh: 升级后状态由具体功能码部分置位
			break;
        case 0x77: /* CAN-OTA结束,应答 */


			// node->node_id固定为0
			// 保证当前某一个固件出现异常升级后，立即退出升级状态(底层77帧不会传进度上来，主要看有没有错误码传上来)
			OtaEndAck *dev_pct = (OtaEndAck *)frame->data;
			if(dev_pct->errcode != 0)
			{
				// 收到77后代表应该结束此次升级，升级位置0，时间置6s表示需要时间将错误码赋值上传，
				// 但又防止ota_doing为1时将period_cmd_delay反复置为6s，导致IOT无法正常轮询、无法正常退出升级状态
				can_ota_status[node->node_id].period_cmd_delay = 6000;
				can_ota_status[node->node_id].ota_doing = 0;
			}
            break;  /* 5秒 */

        case 0x78: /* 底层设备进行估计安装时发送的进度 */ //ac380的升级进度都由IOT自己统计 底层不会传进度值上来
			ESP_LOGW(TAG,"0x78 OTA progress frame, node: %d, devId: 0x%x",node->node_id, canId.bit.src);
            break;

        case 0x79: /* 底层设备进入停留在BOOT,请求上层升级 */
            if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src <= (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))
            {
				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
//				index=0;
                // (*node->invs_info)[index].inv_about.software_total = 1;
                // (*node->invs_info)[index].inv_about.soft[0].type   = prev_boot_info.fileType;
                // (*node->invs_info)[index].inv_about.soft[0].version = prev_boot_info.version;
				ESP_LOGW(TAG, "ARM payload->fileType: %u, payload->version:%lu",payload->fileType,payload->version);

				reals.If_ARM_SingleBoot = 1;//底层ARM若处于单boot状态，版本号从0x79获取，并立即执行一次type=13的轮询获取设备类型及SN
				// (*node->invs_info)[index].inv_about.soft[0].type   = payload->fileType;
				// (*node->invs_info)[index].inv_about.soft[0].version = payload->version;

				reals.ARM_SingleBoot_version = payload->version;


                (*node->invs_info)[index].online = 1;
                (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;

				Inv_can[canId.bit.src - INV_CAN_ADDR].inv_data[0].inv_announce.online = 1;//收到0x79 boot，表示设备在线，此值给xmodem升级使用

				//test 0x79 boot
				CanGetTarget(0, 0, INV_TYPE_ABOUT_13H, 0, 0, canId.bit.src);

				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[0].type = (*node->invs_info)[index].inv_about.soft[0].type;// ARM
				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[0].version = (*node->invs_info)[index].inv_about.soft[0].version;

				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[1].type = (*node->invs_info)[index].inv_about.soft[1].type;;// DSP
				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[1].version = (*node->invs_info)[index].inv_about.soft[1].version;

				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[2].type = (*node->invs_info)[index].inv_about.soft[3].type;;// DCHub
				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[2].version = (*node->invs_info)[index].inv_about.soft[3].version;

				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[3].type = (*node->invs_info)[index].inv_about.soft[2].type;;// BMS
				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[3].version = (*node->invs_info)[index].inv_about.soft[2].version;
            }
            else if ((canId.bit.src >= PACK_CAN_ADDR) && (canId.bit.src <= (PACK_CAN_ADDR + (PACK_MAX_NUM*DEV_MAIN_NODE_MAX))))
            {
				index = (canId.bit.src - PACK_CAN_ADDR)%PACK_GROUP_MAX_NUM;
                (*node->packs_info)[index].pack_about.software_total = 1;
                // (*node->packs_info)[index].pack_about.soft[0].type   = prev_boot_info.fileType;
                // (*node->packs_info)[index].pack_about.soft[0].version = prev_boot_info.version;

				(*node->invs_info)[index].inv_about.soft[0].type   = payload->fileType;
				(*node->invs_info)[index].inv_about.soft[0].version = payload->version;

								index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
				//				index=0;

                (*node->invs_info)[index].online = 1;
                (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
            }
			else if(canId.bit.src == AC_HUB_CAN_ADDR)
			{
				index = (canId.bit.src - AC_HUB_CAN_ADDR)%AC_HUB_MAX_NUM;
				ESP_LOGW(TAG, "ACHUB payload->fileType: %u, payload->version:%lu",payload->fileType,payload->version);

				reals.If_AC_HUB_SingleBoot = 1;//底层ACHUB若处于单boot状态，版本号从0x79获取
				reals.AC_HUB_SingleBoot_version = payload->version;
				ESP_LOGI(TAG,"reals.AC_HUB_SingleBoot_version:%d",(unsigned int)reals.AC_HUB_SingleBoot_version);

                (*node->achubs_info)[index].online = 1;
                (*node->achubs_info)[index].alive_time = (AC_HUB_OFFLINE_TIME * 2);

				// //ACHub只有一个;//收到0x79 boot，表示设备在线，此值给xmodem升级使用  -> 改为开关状态
				// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online |= 1 << 1;

			}
            break;

        case INV_ANNOUNCE1:

//			ble_on_off_can_debug(frame->data[0]);
//			ESP_LOGE(TAG, "INV_ANNOUNCE1,can rx 0x10 ID, data[0] %d", frame->data[0]);


        case INV_ANNOUNCE2:
        case INV_ANNOUNCE3:
        case INV_ANNOUNCE4:
        case INV_ANNOUNCE5:
		case INV_ANNOUNCE6:
				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
//				index=0;
			ESP_LOGI(TAG,"index:%d,canId.bit.funcode:%x",index,canId.bit.funcode);
            if (index < INV_MAX_NUM)
			{
                offset = canId.bit.funcode - INV_ANNOUNCE1;
				pdata = ((uint8_t *)&(*node->invs_info)[index].inv_announce) + (offset * 8);
                memcpy(pdata, frame->data, frame->data_length_code);
                (*node->invs_info)[index].online = 1;
                (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;

				(*node->invs_info)[index].inv_announce.online = 1;
				if ( canId.bit.funcode == INV_ANNOUNCE2)
				{
					ESP_LOGI(TAG,"CanListenBus id_index:%d,year:%d mouth:%d day:%d hour:%d min:%d",index,(*node->invs_info)[index].inv_announce.date.year,(*node->invs_info)[index].inv_announce.date.month,
					(*node->invs_info)[index].inv_announce.date.day,(*node->invs_info)[index].inv_announce.date.hour,
					(*node->invs_info)[index].inv_announce.date.minute);
					/*时间校验*/
					RTC_Valid_Check(canId.bit.src - INV_CAN_ADDR);
				}
            }

			/* ATS点火信号检测 */
//			if (canId.bit.funcode == INV_ANNOUNCE1)
//			{
//				uint8_t inv_flag2 = 0;
//				for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
//				{
//					for (int i = 0; i < INV_MAX_NUM; i++)
//					{
//						if (!Inv_can[node_id].inv_data[i].online) continue;
//						inv_flag2 |= Inv_can[node_id].inv_data[i].inv_announce.inv_flag2.flag; // 汇总逆变状态标志2,该状态含有ATS信号
//					}
//				}
//				inv_flag2_t *pdata = (inv_flag2_t*)&inv_flag2;
//				do_operate(pdata->bit.ats_fire_sig);	// ATS点火信号输出
//			}
            break;

		case INV_ANNOUNCE7:
		case INV_ANNOUNCE8:
		case INV_ANNOUNCE9:
			if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src <= (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
			{
				reals.If_ARM_SingleBoot = 0;//收到逆变正常周期上报帧，退出逆变单boot状态
				if (canId.bit.page != 1)
					break; // 该处的功能码属于第一页数据
	//			index = canId.bit.src - INV_CAN_ADDR;

				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
				if (index < INV_MAX_NUM) {
					offset = canId.bit.funcode - INV_ANNOUNCE7;
					pdata = (uint8_t *)&(*node->invs_info)[index].inv_announce.l1_chg_limit + (offset * 8);
					memcpy(pdata, frame->data, frame->data_length_code);
					(*node->invs_info)[index].online = 1;
					(*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
				}
			}
			else if((canId.bit.src >= DC_HUB_CAN_ADDR) && (canId.bit.src <= (DC_HUB_CAN_ADDR + DC_HUB_MAX_NUM*DEV_MAIN_NODE_MAX)))// DCHub
			{
				index = (canId.bit.src - DC_HUB_CAN_ADDR)%DC_HUB_MAX_NUM;
				if (index < DC_HUB_MAX_NUM) {
					offset = canId.bit.funcode - INV_ANNOUNCE7;
					pdata = (uint8_t *)&(*node->dchubs_info)[index].dc_hub_info.input_power + (offset * 8);
					memcpy(pdata, frame->data, frame->data_length_code);
					(*node->dchubs_info)[index].online = 1;
					(*node->dchubs_info)[index].alive_time = DC_HUB_OFFLINE_TIME;
					// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online |= 1 << 0;//DCHub只要有一个在线，该位就置一 171
					// Inv[index].mod_reg00100_AppPage1.Parts_online |= 1 << 0;//app需要分别用从机地址01、02读取DCHUB在线状态
					ESP_LOGI(TAG,"online0:%d,online1:%d,online2:%d",Inv_can[0].dc_hub_data[0].online,Inv_can[1].dc_hub_data[0].online,Inv_can[2].dc_hub_data[0].online);
					Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online.bit.dc_hub_online = 1;//DCHub只要有一个在线，该位就置一 171
					Inv[index].mod_reg00100_AppPage1.Parts_online.bit.dc_hub_online = 1;//app需要分别用从机地址01、02读取DCHUB在线状态
					//ESP_LOGI(TAG,"dc_hub_online:%d",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online.bit.dc_hub_online);
				}
			}
			else if(canId.bit.src == AC_HUB_CAN_ADDR)// ACHub
			{
				reals.If_AC_HUB_SingleBoot = 0;//收到achub正常周期上报帧，退出achub单boot状态
				index = (canId.bit.src - AC_HUB_CAN_ADDR)%AC_HUB_MAX_NUM;
				if (index < AC_HUB_MAX_NUM) {
					offset = canId.bit.funcode - INV_ANNOUNCE7;
					switch(offset)
					{
						case 0:
						{
							pdata = (uint8_t *)&(*node->achubs_info)[index].ac_hub_info.Power_load[0];
							memcpy(pdata, frame->data, frame->data_length_code - 2);
							memcpy((uint8_t *)&(*node->achubs_info)[index].ac_hub_info.alarm_State,
									(frame->data)+6, 2);
						}
							break;
						default:
							pdata = NULL;
							break;
					}
					//memcpy(pdata, frame->data, frame->data_length_code);
					(*node->achubs_info)[index].online = 1;
					(*node->achubs_info)[index].alive_time = AC_HUB_OFFLINE_TIME;

					// Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Parts_online |= 1 << 1;//ACHub只有一个 171 -> 改为开关状态

					// ESP_LOGW(TAG,"login_info.dev_type:%s, Inv_can_mix.ACHUB_safe_code:%llu",login_info.dev_type,Inv_can_mix.ac_hub_data[0].ac_hub_info.ACHUB_safe_code);
					//当检测到AC_HUB在线，且此时MQTT登录的类型不为HA1时(并机状态)，主IOT重新以AC_HUB的信息启动MQTT
					if((strcmp(login_info.dev_type, IOT_TYPE_HA1) != 0) && (reals.online_ACHUB_num == 1) && (reals.Addr_can_master == 1))
					{
						ESP_LOGI(TAG, "CanListenBus ACHUB online, update MQTT type:%s,sn:%llu ,safecode:%llu",SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, SetData.dev_info_t.Parallel_dev_safetycode);
						//reals.mqttChange_flag=2;
						//iot_mqtt_ChageFlagSet(2);
						//iot_wifi_new_iot(SetData.dev_info_t.Parallel_dev_type, SetData.dev_info_t.Parallel_dev_sn, SetData.dev_info_t.Parallel_dev_safetycode);
						//iot_wifi_new_dev(SetData.dev_info_t.Parallel_dev_type,  SetData.dev_info_t.Parallel_dev_sn);
					}
				}
			}
			else if ((addr_source >= D400S_CAN_ADDR) && (addr_source < (D400S_CAN_ADDR + DEV_MAIN_NODE_MAX*D400S_MAX_NUM)) )
			{
				ESP_LOGI(TAG,"D400S_CAN_ADDR CAN GET src:%d",canId.bit.src);
				index = (canId.bit.src - D400S_CAN_ADDR)%D400S_MAX_NUM;
				if (index < D400S_CAN_ADDR) {
					//offset = canId.bit.funcode - INV_ANNOUNCE7;
					//pdata = (uint8_t *)&(*node->d400s_s_info)[index].d400s_common_info.input_power + (offset * 8);
					//memcpy(pdata, frame->data, frame->data_length_code);
					(*node->d400s_s_info)[index].online = 1;
					(*node->d400s_s_info)[index].alive_time = D400S_OFFLINE_TIME;
					(*node->d400s_s_info)[index].d400s_common_info.dc_info[2].dc_power=(uint16_t)((frame->data[1])|(frame->data[2]<<8));
					(*node->d400s_s_info)[index].d400s_common_info.dc_info[2].dc_voltage=(uint16_t)((frame->data[3])|(frame->data[4]<<8));
					(*node->d400s_s_info)[index].d400s_common_info.dc_info[0].dc_voltage=(uint16_t)((frame->data[5])|(frame->data[6]<<8));
					//ESP_LOGI(TAG,"D400S_CAN_ADDR CAN GET dc3 power:%u",(*node->d400s_s_info)[index].d400s_common_info.dc_info[2].dc_power);
					//ESP_LOGI(TAG,"D400S_CAN_ADDR CAN GET dc3 dc_voltage:%u",(*node->d400s_s_info)[index].d400s_common_info.dc_info[2].dc_voltage);
					//ESP_LOGI(TAG,"D400S_CAN_ADDR CAN GET dc1 dc_voltage:%u",(*node->d400s_s_info)[index].d400s_common_info.dc_info[0].dc_voltage);
				}
			}

			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
			break;

		case INV_ANNOUNCEA:
		{
			if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src <= (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
			{
				if (canId.bit.page != 1)
					break;
				// 该处的功能码属于第一页数据 逆变器基于故障变化触发式上报
	//			index = canId.bit.src - INV_CAN_ADDR;

				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
				//offset = canId.bit.funcode - INV_ANNOUNCE7;
				uint8_t dataindex=frame->data[0]-1;
					pdata = (uint8_t *)&(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex];
					//从data的第二位数组索引开始读取，只获取6个字节
					memcpy(pdata, &frame->data, frame->data_length_code);
					(*node->invs_info)[index].online = 1;
					(*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
					ESP_LOGI(TAG,"(*node->invs_info)[%d].inv_detailedinfo.inv_transer_buf[%d].dataindex:%d",index,dataindex,(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.dataindex);
					ESP_LOGI(TAG,"buf1_L:%d buf1_H:%d",(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.buf1_L,(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.buf1_H);
					ESP_LOGI(TAG,"buf2_L:%d buf2_H:%d",(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.buf2_L,(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.buf2_H);
					ESP_LOGI(TAG,"buf3_L:%d buf3_H:%d",(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.buf3_L,(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.buf3_H);
					ESP_LOGI(TAG,"revd:%d ",(*node->invs_info)[index].inv_detailedinfo.inv_transer_buf[dataindex].transer_struct.revd);
			}
		}
		break;


		case INV_ANNOUNCE_0xFA:
		    if ( (0xFA == pf_fun)//IOT主动上报new
				&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp)
				&&(addr_source >= INV_CAN_ADDR) && (addr_source < (INV_CAN_ADDR + INV_MAX_NUM*DEV_MAIN_NODE_MAX)
				)//(0x08FA0000 != (canId.all&0xFFFF0000))
			)
			{
				// if(((addr_source -INV_CAN_ADDR) == reals.Addr_can_self))//test bind
				if((addr_source >= INV_CAN_ADDR) && (addr_source <= (INV_CAN_ADDR + 2)))//test bind
				{
					if((99 == frame->data[0])
						&&(11 == frame->data[1]))//11-群组主机上报本群组SN
					{
						ESP_LOGW(TAG, "[12170 BIND] CAN RX bind frame: src=%u data=%02X %02X %02X %02X %02X %02X %02X %02X",
							addr_source,
							frame->data[0], frame->data[1], frame->data[2], frame->data[3],
							frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
						Device_Can_Address_Parallel_Bind(frame,addr_source);

					}
				}


			}

			break;




        case PACK_ANNOUNCE1:
        case PACK_ANNOUNCE2:
        case PACK_ANNOUNCE3:
        case PACK_ANNOUNCE4:
        case PACK_ANNOUNCE5:
        case PACK_ANNOUNCE6:
            index = (canId.bit.src - PACK_CAN_ADDR)%PACK_GROUP_MAX_NUM; // 设备索引
			ESP_LOGI(TAG,"CanListenBus index:%d",index);
            if (index < PACK_MAX_NUM) {
                offset = canId.bit.funcode - PACK_ANNOUNCE1; //
                pdata  = ((uint8_t *)&(*node->packs_info)[index].pack_announce) + (offset * 8);
                memcpy(pdata, frame->data, frame->data_length_code);
                (*node->packs_info)[index].online = 1;
                (*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;
            }
            break;

		case PACK_ANNOUNCE7:
		case PACK_ANNOUNCE8:
		case PACK_ANNOUNCE9:
			if (canId.bit.page != 1)
				break; // 该处的功能码属于第一页数据
			index =  (uint8_t)(canId.bit.src - PACK_CAN_ADDR)%PACK_GROUP_MAX_NUM;
            if (index < PACK_MAX_NUM) {
				offset = canId.bit.funcode - PACK_ANNOUNCE7;
				pdata = ((uint8_t *)&(*node->packs_info)[index].pack_announce.aging_status) + (offset * 8);
                memcpy(pdata, frame->data, frame->data_length_code);
                (*node->packs_info)[index].online = 1;
                (*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;
            }
			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
			 * 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
			break;


        default: break;
    }
}



/*
windy add
CAN beta ID解析

input:
CanId:
output:
level1_addr:返回的 数组结构体一维序号,从0开始
level2_addr:返回的 数组结构体二维序号,从0开始


return:
0:合法 CAN ID
非0：非法 CAN ID
*/
int8_t Can_ID_Check_IOT(uint32_t CanId, uint8_t *level1_addr)
{
    int8_t rtn = 0xFF;

    uint8_t pf_fun = 0;
    uint8_t Id_p = 0;
    uint8_t Id_edp = 0;
    uint8_t Id_dp = 0;

    uint8_t addr_source = 0;

	pf_fun =(CanId>>16)&0xFF;
    Id_p = (CanId>>26)&0x7;
    Id_edp = (CanId>>25)&0x1;
    Id_dp = (CanId>>24)&0x1;


	addr_source =CanId&0xFF;

    if ( (0xFA == pf_fun)//IOT主动上报new
		&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp)
		&&(addr_source >= IOT_CAN_ADDR) && (addr_source < (IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX))
	)
	{
		*level1_addr = addr_source - IOT_CAN_ADDR; // IOT
		rtn=0;
	}
	else
	{
//        ESP_LOGE(TAG, "unkown dev addr: 0x%lx", CanId);
        rtn= -1;
    }
    return rtn;
}


//////////////////////////////////////////////////20240528 windy
/*------------------------------------------------------------------------
*@Function :Device_Discovery_Step

AC380的CAN并机地址排序由ARM决定，IOT仅做服从绑定处理
即使用生产绑定的 群组地址（逆变器地址）作为自己的排序依据

*@return
0- fail
no 0: tx len
*/
void Device_Can_Address_Parallel_Bind(twai_message_t *frame,uint8_t addr) //100ms cycle
{
		uint8_t can_iot_buff=0;
		static uint64_t revSnRem[3];//当前最多3个并机 SN绑定码记忆
		if(frame->data[7]==CAN_CTRL_CMD_BIND_TRIGER)
		{
			memcpy(SetData.dev_info_t.Sn_bind, &frame->data[2], 5);
			reals.SetDataWrFlag.sBit.bind_sn =1;

			g_self_data.mod_reg11000_IOT_info.Bind_SN =((uint16_t)SetData.dev_info_t.Sn_bind[1]<<8)|SetData.dev_info_t.Sn_bind[0];


//			g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND=0;//改到写入flash后清零
			ESP_LOGW(TAG, "[12170 BIND] CAN RX BIND_TRIGER(0x55) OK: addr=%u Bind_SN=%u Sn_bind=%02X %02X %02X %02X %02X",
				addr, g_self_data.mod_reg11000_IOT_info.Bind_SN,
				SetData.dev_info_t.Sn_bind[0], SetData.dev_info_t.Sn_bind[1],
				SetData.dev_info_t.Sn_bind[2], SetData.dev_info_t.Sn_bind[3],
				SetData.dev_info_t.Sn_bind[4]);

		}else if(frame->data[7]==CAN_CTRL_CMD_BIND_CHECK)
		{
			uint8_t SnRev[5]={0};
			uint64_t curBindSn=0;
			uint8_t compareCnt=0;
			memcpy(SnRev, &frame->data[2], 5);
			ESP_LOGI(TAG,"frame->data: %d-%d-%d-%d-%d",frame->data[2],frame->data[3],frame->data[4],frame->data[5],frame->data[6]);
			if(memcmp(SnRev,SetData.dev_info_t.Sn_bind, 5)==0)
			{
				can_iot_buff=addr-INV_CAN_ADDR;
				if(reals.Addr_can_Buff!=can_iot_buff)
				{
					ESP_LOGW(TAG,"Addr_can_Buff%d >> can_iot_buff:%d",reals.Addr_can_Buff,can_iot_buff);
					reals.Addr_can_Buff=can_iot_buff;
				}
			}
			memcpy((uint8_t*)&curBindSn, (uint8_t*)&SetData.dev_info_t.Sn_bind[0], 5);//源 SN
			for(uint8_t i=0;i<reals.online_Iot_num;i++)
			{
				if(revSnRem[i]==0)
				{
					revSnRem[i]=curBindSn;
					break;
				}else if(revSnRem[i]!=curBindSn)
				{
					compareCnt++;
				}else if(revSnRem[i]==curBindSn)
				{
					break;
				}
			}
			if(compareCnt>=reals.online_Iot_num)
				reals.Addr_can_Attr|=Can_Addr_Parallel_SendBindErr;
			else
				reals.Addr_can_Attr&=~(Can_Addr_Parallel_SendBindErr);
		}

}

/*------------------------------------------------------------------------
*@Function :  设置Step_can_dev_parallel值,设备重新排序  当前已弃用
*/
uint8_t Device_Can_Address_Parallel_SetTriger(uint16_t time)
{
	uint8_t addrChageCount=0;
	if(reals.Step_can_dev_parallel!=STEP_CAN_GROUP_PARALLEL_TRIGER&&(addrChageCount<3)
	&&!reals.Addr_can_Parallel_Interval)
	{
		//reals.MonSnSame=1;
		reals.Addr_can_Parallel_Interval=time;
		reals.Addr_can_Attr|=(Can_Addr_Parallel_SameAlarm|Can_Addr_Parallel_SendBindChk);
		//reals.Step_can_dev_parallel=STEP_CAN_GROUP_PARALLEL_TRIGER;
		addrChageCount++;
	}

	return 0;
}

/*------------------------------------------------------------------------
*@Function :


*/
void Device_Can_Address_Parallel_Triger(void) //100ms cycle
{

		if(1 == g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_IOT)
		{

			//reals.Step_can_dev_parallel = STEP_CAN_GROUP_PARALLEL_TRIGER;
			g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_IOT=0;
			ESP_LOGI(TAG,"STEP_CAN_GROUP_PARALLEL_TRIGER	windy CCC");

		}
		else if(1 == g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_INV)
		{

			g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_INV=0;

		}
		else if(1 == g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_PACK)
		{

			g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_PACK=0;

		}

}

/*
单帧接收解析

接收并解析下层设备周期广播的数据

*/
static void CanListenBus_From_IOT(node_info_t *node, twai_message_t *frame)
{
    IdStruct canId;
    uint8_t index = 0;
	uint8_t level1_addr=0;
    static OtaBoot prev_boot_info;
    int8_t rtn = 0xFF;

    if (!frame)
	{

	}
		return ;


	rtn =Can_ID_Check_IOT(frame->identifier,&level1_addr);
	if(0 != rtn)//合法CAN ID
	{
		return ;
	}


    uint32_t offset;
    uint8_t *pdata;

    canId.all = frame->identifier;//can rx ID
    switch (frame->data[0])
    {
        case 99: /* 底层设备进行估计安装时发送的进度 */
//			if(11 == frame->data[1])//11-群组主机上报本群组SN
//			{
//
//			}

            break;
		default:
			break;

	}
}


static void dump_err_can_frame(const twai_message_t *frame)
{
	uint32_t can_id = frame->identifier;
	can_id = CAN_ADDR_SWAP(can_id);
//	ESP_LOGW(TAG, "received error can frame, id: %08x, data:", can_id);
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, frame->data, frame->data_length_code, ESP_LOG_WARN);
}

/*
Can Beta多字节读取， 接收解析

IOT发送读取设备后,等待设备的应答

*/

static void CanGetWaitResponse(node_info_t *node, const twai_message_t *frame) {
    IdStruct canId ;
    if (!frame) return ;

    canId.all = frame->identifier;
    if (canId.bit.dst != esp_canbus_myself_address() &&
        canId.bit.dst != CAN_BROADCAST_ADDRESS ) {
            return ; // 当前CAN帧,不是发送给本机,不处理
    }
	if(0 != Can_ID_Check_Beta_Multi_Read(canId.all))
	{
		return ; //
	}

    switch (canId.bit.funcode)
    {
        case READ_RESP_DATA_START:
        {
            CmdStruct *cmd = (CmdStruct *)frame->data;


            int err = CanVerifyCmd(node, canId.all, 1, canId.bit.src, cmd, NULL);  /* 检测数据是否在范围,如果正常，记录命令 */
			if (err == ERR_TYPE) {
				dump_err_can_frame(frame);
			}
        }
        break;

        case READ_RESP_DATA:
        {
            DataStruct *data = (DataStruct *)frame->data;
            int err = CanVerifyData(node, canId.all, 1, canId.bit.src, data->type, data->seq, frame->data_length_code-FRAME_HEAD, data->bytes);
			if (err == ERR_TYPE) {
				dump_err_can_frame(frame);
			}
		}
        break;

        default: break;
    }
    return ;
}
/*
Can Beta多字节写， 接收解析
IOT发送写入设备后,等待设备的应答


*/

static void CanSetWaitRespones(const twai_message_t *frame) {
    AckStruct *ack;
    IdStruct canId ;

    if (!frame) return ;

    canId.all = frame->identifier;
    if (canId.bit.dst != esp_canbus_myself_address() &&
        canId.bit.dst != CAN_BROADCAST_ADDRESS ) {
            return ; // 当前CAN帧,不是发送给本机,不处理
    }

    if (can_cmd_bk.write_cmd.status == COMPLETE) { /* 无等待设置响应 */
        return ;
    }

	if(0 != Can_ID_Check_Beta_Multi_Write(canId.all))
	{
		return ; //
	}

    if ((canId.bit.funcode != WIRTE_DATA_ACK) ||
        (canId.bit.page != can_cmd_bk.write_cmd.page) ||
        (canId.bit.dst  != can_cmd_bk.write_cmd.dst_addr)) {
        return ;
    }

    ack = (AckStruct *)frame->data;
    if ((ack->type  != can_cmd_bk.write_cmd.cmd.type) ||
        (ack->start != can_cmd_bk.write_cmd.cmd.start)) {
        return ;
    }

    if (!ack->total)  { /* 反馈收到的字节为0，表示接收错误 */
        can_cmd_bk.write_cmd.status = COMPLETE;
        can_cmd_bk.write_cmd.err_code = ack->append; /* error code */
        can_cmd_bk.write_cmd.wait_time = 0;
    }
}

/*
上位机或主IOT CAN读取 我IOT，多字节读

本地CAN数据被读取
 * 参数：twai_message_t *frame, 接收到的CAN总线数据
 * 无返回值
 */
static void CanLocalDataIsGet(node_info_t *node, const twai_message_t *frame) {
    uint16_t errCode = ERR_NONE;
    dev_data payload ;
    IdStruct TxcanId ;
    IdStruct RxcanId ;

    if (!frame)
		return ;

    RxcanId.all = frame->identifier;
    TxcanId.all = RxcanId.all&0xFFFF0000;//截取ID高U16
	TxcanId.bit.src = esp_canbus_myself_address();
	TxcanId.bit.dst = RxcanId.bit.src;

    if (RxcanId.bit.dst != esp_canbus_myself_address() &&
        RxcanId.bit.dst != CAN_BROADCAST_ADDRESS ) {
            return ; // 当前CAN帧,不是发送给本机,不处理
    }

    if (RxcanId.bit.funcode != READ_DATA_CMD) {  /* Read Cmd */
        return ;  // 不是读取帧不处理
    }

    CmdStruct *cmd = (CmdStruct *)frame->data;
	cmd->crc16 = 0;
    errCode = CanVerifyCmd(node, RxcanId.all, 0, RxcanId.bit.src, cmd, &payload); // 校验读取命令
	if (errCode == ERR_TYPE) {
		dump_err_can_frame(frame);
	}
	ESP_LOGI (TAG, "CanLocalDataIsGet  windy11  ,RxcanId.all=0x%lx ",RxcanId.all);

    if (errCode != ERR_NONE)  {
        if (payload.pdata) {
            free(payload.pdata);
        }
        TxcanId.bit.funcode = READ_RESP_DATA_START;

        CanAckStart(node->node_id, TxcanId.all, cmd->type, cmd->start, 0, errCode); // 数据区域为0,末尾为错误码
        return;
    }
//	ESP_LOGI (TAG, "CanLocalDataIsGet windy22 ");

    if (payload.pdata == NULL) {
        return;
    }
//	ESP_LOGI (TAG, "CanLocalDataIsGet windy33 ");

    TxcanId.bit.funcode = READ_RESP_DATA_START;
    uint16_t crc16 = calcu_crc16(payload.pdata, payload.len);    // crc16
	ESP_LOGI (TAG, "CanLocalDataIsGet 1 ,TxcanId.all=0x%lx,crc16=0x%x,payload.len=%d, ",TxcanId.all,crc16,payload.len);

    if (!CanAckStart(node->node_id, TxcanId.all, cmd->type, cmd->start, payload.len, crc16)) {
        free(payload.pdata);
        return;
    }

    uint8_t sendLen;
    uint8_t seq = 0;
    uint8_t *ptr = payload.pdata;


//	dump_buf_global("Inv_can[0].inv_data[0].mod_reg12000_IOT_set", &Inv_can[0].iot_data[0].mod_reg12000_IOT_set, sizeof(iot_can_node_struct_reg12000)-sizeof(rw_cmd_struct));
//	dump_buf_global("Inv_can[1].inv_data[0].mod_reg12000_IOT_set", &Inv_can[1].iot_data[0].mod_reg12000_IOT_set, sizeof(iot_can_node_struct_reg12000)-sizeof(rw_cmd_struct));
//	dump_buf_global("Inv_can[2].inv_data[0].mod_reg12000_IOT_set", &Inv_can[2].iot_data[0].mod_reg12000_IOT_set, sizeof(iot_can_node_struct_reg12000)-sizeof(rw_cmd_struct));
//	dump_buf_global("ptr ", ptr, payload.len);

    DataStruct canData;
    TxcanId.bit.funcode = READ_RESP_DATA;
    while(payload.len) // total send bytes
    {
        sendLen = (payload.len >= FRAME_DATA_BYTES) ? (FRAME_DATA_BYTES) : (payload.len);

        canData.type = cmd->type;
        canData.seq = seq;
        memcpy(canData.bytes, ptr, sendLen);
		//ESP_LOGI (TAG, "CanLocalDataIsGet ,TxcanId.all=0x%lx,type=0x%x,seq=%d, ",TxcanId.all,canData.type,canData.seq);

        if (CanAckData(node->node_id, TxcanId.all, (uint8_t *)&canData, (sendLen + FRAME_HEAD)))  {
            payload.len -= sendLen;
            ptr += sendLen;
            seq++;
        } else {
            ESP_LOGE (TAG, "Can Send Failed");
            break; // 发送数据失败
        }
    }

    free(payload.pdata);
}

/* 查找目标数据是否存储数组中 */
static int find_cmd(uint8_t target, uint8_t *data, int len)
{
	for(int i = 0; i < len; i++)
	{
		if(data[i] == target) {
			return 0;
		}
	}
	return -1;
}

/**
 * @brief 接收CAN写数据指令
 *
 * 该函数将接收CAN线上的写本机指令数据,某帧数据接收完成后通过队列发送给其它任务处理
 * 该函数目前只接收与历史数据记录有关的CAN帧数据
 * 当接收到期望的CAN帧数据后,函数返回时将设置CAN帧ID为0,这将会导致后续功能模块忽略此CAN帧以避免错误处理
 *
 * @param node_id 指示当前CAN数据属于哪一路CAN
 * @param frame CAN原始数据帧
 * @param cmd_queue 保存CAN写指令的队列
 *
 * @return 无
 */
static void can_rx_write_cmd(uint8_t node_id, twai_message_t *frame, QueueHandle_t cmd_queue)
{
	static CmdStruct cmd = {0};											// CAN写开始指令结构变量
	static uint8_t can_rx_temp[CAN_RX_TEMP_SIZE] = {0};					// CAN接收缓存
	static uint8_t expected_cmd_type[] = CAN_RX_CMD_TYPE_INIT;			// 期望接收的数据类型

	if(node_id >= DEV_MAIN_NODE_MAX || frame == NULL || cmd_queue == NULL) return;

	IdStruct canId;
    canId.all = frame->identifier;

	/* 不是发送给本机的数据不处理 */
    if((canId.bit.dst != esp_canbus_myself_address())
		&& (canId.bit.dst != CAN_BROADCAST_ADDRESS))
		return;

	/* 不是写数据指令不处理 */
	if((canId.bit.funcode != WRIET_DATA_START) || (canId.bit.funcode != WRITE_DATA)) return;

	/* 查找指令类型,不是需要的指令类型不处理 */
	if(find_cmd(frame->data[0], expected_cmd_type, sizeof(expected_cmd_type)/sizeof(expected_cmd_type[0])) < 0) return;

	canId.bit.src = esp_canbus_myself_address();

	/* 写开始指令处理 */
	if(canId.bit.funcode == WRIET_DATA_START)
	{
		canId.bit.funcode = WIRTE_DATA_ACK;
		memcpy(&cmd, frame->data, sizeof(CmdStruct));	// 保存写开始指令

		/* 数据长度为0发送错误响应 */
		if(!cmd.total || cmd.total > CAN_RX_TEMP_SIZE) {
			CanAckStart(node_id, canId.all, cmd.type, cmd.start, 0, ERR_RANGE);
		}
	}
	/* 写数据指令处理 */
	else
	{
		canId.bit.funcode = WIRTE_DATA_ACK;
		DataStruct *data = (DataStruct *)frame->data;
		uint8_t err_code = 0;

		/* 校验数据类型与数据长度 */
		if(cmd.type != data->type) {
			err_code = ERR_TYPE;
		}
		else if (frame->data_length_code < FRAME_HEAD){
			err_code = ERR_LOSE;
		}

		/* 校验失败发送错误响应 */
		if(err_code) {
			CanAckStart(node_id, canId.all, cmd.type, cmd.start, 0, err_code);
		}
		/* 正常写数据指令处理 */
		else {
			/* 保存数据到缓存 */
			memcpy(&can_rx_temp[data->seq*6], data->bytes, frame->data_length_code-FRAME_HEAD);

			uint16_t data_len = data->seq * 6 + frame->data_length_code - FRAME_HEAD;
			if(cmd.total == data_len) 	// 判断数据是否接收完成(对比写开始指令中的数据长度与本次接收的总数据长度)
			{
				uint16_t crc16 = calcu_crc16(can_rx_temp, data_len);
				if(crc16 == cmd.crc16)
				{
					/* 回复写指令成功 */
					CanAckStart(node_id, canId.all, cmd.type, cmd.start, cmd.total, 0);

					/* 填充指令数据结构信息 */
					can_recv_cmd_t recv_cmd;
					recv_cmd.node_id = node_id;
					recv_cmd.can_id.all = canId.all;
					recv_cmd.data_type = cmd.type;
					recv_cmd.offset = cmd.start;
					recv_cmd.data_len = cmd.total;
					recv_cmd.data = (uint8_t *)heap_caps_malloc(cmd.total, MALLOC_CAP_SPIRAM);
					if(recv_cmd.data != NULL)
					{
						memcpy(recv_cmd.data, can_rx_temp, cmd.total); //拷贝CAN缓冲区数据到指令数据结构

						/* 发送指令数据到队列 */
						if (xQueueSend(cmd_queue, &recv_cmd, pdMS_TO_TICKS(100)) != pdPASS) {
							free(recv_cmd.data);	//队列发送失败清除申请的数据缓存
						}
					}
				}
			}
		}
	}

	/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
	 * 因为其它功能模块可能不识别该帧而回复错误响应 */
	frame->identifier = 0;
}

/*
上位机CAN写入 我IOT，多字节写

IOT设备的标定信息通过CAN写入

*/

static void CanLocalDataIsSet(node_info_t *node, const twai_message_t *frame)
{
    uint16_t errCode = ERR_NONE;
    static CmdStruct cmd;
    static uint8_t wait_data = 0;
    IdStruct TxcanId ;
    IdStruct RxcanId ;

    /* 未接收到CAN帧 */
    if (!frame) {
		return;
	}

    RxcanId.all = frame->identifier;
    TxcanId.all = RxcanId.all&0xFFFF0000;//截取ID高U16
	TxcanId.bit.src = esp_canbus_myself_address();


    if (RxcanId.bit.dst != esp_canbus_myself_address() &&
        RxcanId.bit.dst != CAN_BROADCAST_ADDRESS ) {
            return ; // 当前CAN帧,不是发送给本机,不处理
    }
    // ESP_LOGI(TAG, "be setting canid: 0x%x", canId.all);
    switch (RxcanId.bit.funcode)
    {
        case WRIET_DATA_START: // Set Data Start
        {
            TxcanId.bit.funcode = WIRTE_DATA_ACK;
            cmd = *((CmdStruct *)frame->data);
            errCode = CanVerifyCmd(node, RxcanId.all, 1, RxcanId.bit.src, &cmd, NULL);
			if (errCode == ERR_TYPE) {
				dump_err_can_frame(frame);
			}
            if (errCode != ERR_NONE)  {
                wait_data = 0;
                CanAckStart(node->node_id, TxcanId.all, cmd.type, cmd.start, 0, errCode);
            } else {
                wait_data = 1;
            }
        }
        break;

        case WRITE_DATA: // Set Data
        {
            if (wait_data != 1) {
                break;
            }

            TxcanId.bit.funcode = WIRTE_DATA_ACK;
            DataStruct *data = (DataStruct *)frame->data;

            if (cmd.type != data->type) {
                break;
            }

            if (frame->data_length_code < FRAME_HEAD) {
                CanAckStart(node->node_id, TxcanId.all, cmd.type, cmd.start, 0, ERR_LOSE);
                break;
            }
            //esp_log_buffer_hex(TAG, data->bytes, frame->data_length_code-FRAME_HEAD);
            errCode = CanVerifyData(node, RxcanId.all, 1, RxcanId.bit.src, data->type, data->seq, frame->data_length_code-FRAME_HEAD, data->bytes);
			if (errCode == ERR_TYPE) {
				dump_err_can_frame(frame);
			}
			if (errCode != ERR_NONE && errCode != ERR_WAIT) {
                CanAckStart(node->node_id, TxcanId.all, cmd.type, cmd.start, 0, errCode);
                wait_data = 0;
            } else if (errCode == ERR_NONE){
                CanAckStart(node->node_id, TxcanId.all, cmd.type, cmd.start, cmd.total, 0);
                wait_data = 0;
            } else {
				ESP_LOGE(TAG, "received unknown data:errCode=%d",errCode);
				ESP_LOG_BUFFER_HEX(TAG, frame->data, frame->data_length_code);
			}
        }
        break;
    }
}

static inline uint8_t CanCheckSetStatus(void) {
    if (can_cmd_bk.write_cmd.status != COMPLETE) { // 操作未完成
        return ERR_WAIT; // 需等待
    }
    return can_cmd_bk.write_cmd.err_code; // 操作完成,返回操作代码
}

/* 设置目标设备的数据

node_id:总线序号，AC380 not use

can beta,多字节写，自己做主,tx

return:
0-fail
*/
uint8_t CanSetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, const uint8_t *pdata, uint16_t data_len, uint8_t dst_addr) {
    uint8_t seq = 0;
    uint16_t ucrc16;
    uint16_t total = data_len;
    IdStruct canId;
    DataStruct canData;

    if ( !pdata || !total || (total > MAX_BYTES) ) {
        return 0;
    }
    canId.bit.page  = page;
    canId.bit.dst   = dst_addr;                	    // 目的地址
    canId.bit.src   = esp_canbus_myself_address();  // 源地址
    canId.bit.funcode  = WRIET_DATA_START;	        // 信息类型
    canId.bit.priority = LEVEL2;        	        // 传送优先级(默认读写为2级)

    ucrc16 = calcu_crc16(pdata, total);
    if (!CanAckStart(node_id, canId.all, type, start, total, ucrc16)) {
        return 0;
    }
	// /* dump modbus转can的指令 */
	// CmdStruct cmd_data_test;
	// cmd_data_test.type  = type;
	// cmd_data_test.start = start;
	// cmd_data_test.total = total;
	// cmd_data_test.crc16 = ucrc16;
	// cmd_data_test.nc    = 0;
	// printf("md to can start, node: %d, send message id:%08x, type:%02x, data:\n", node_id, canId.all, type);
	// for(int i = 0; i < sizeof(cmd_data_test); i++)
	// {
	// 	uint8_t *p_can_data = (uint8_t*)&cmd_data_test;
	// 	printf("%02x ", p_can_data[i]);
	// }
	// printf("\n");

    uint8_t sendLen;
    canId.bit.funcode = WRITE_DATA;

    while (total) {
        canData.type = type;
        canData.seq  = seq;
        sendLen = (total >= FRAME_DATA_BYTES) ? (FRAME_DATA_BYTES) : (total);

        memcpy(canData.bytes, pdata, sendLen);
        if (!CanAckData(node_id, canId.all, (const uint8_t *)&canData, sendLen+2))
		{
           return 0; // send failed
        }
		// /* dump发送的数据 */
		// // if(type == 0x1a)
		// // {
		// 	uint8_t *p_can_data = (uint8_t *)&canData;
		// 	printf("md to can data, node: %d, id: %08x, type:%02x, data:\n", node_id, canId.all, type);
		// 	for(int i = 0; i < sendLen+2; i++)
		// 	{
		// 		printf("%02x ", p_can_data[i]);
		// 	}
		// 	printf("\n\n");
		// // }

        total -= sendLen;
        pdata += sendLen;
        seq++;
    }

     /* 记录本次发送的命令 */
    can_cmd_bk.write_cmd.cmd.type  = type;
    can_cmd_bk.write_cmd.cmd.start = start;
    can_cmd_bk.write_cmd.cmd.total = data_len;

    can_cmd_bk.write_cmd.status   = WAITTING; // 等待ACK
    can_cmd_bk.write_cmd.err_code = ERR_NONE;
    can_cmd_bk.write_cmd.wait_time = 1000;

    return 1;
}

/* 读取目标设备的数据
can beta,多字节读取，自己做主,tx
node_id：CAN总线序号，not use

TX：READ_DATA_CMD

node_id: CAN总线序号，EMS有3个；IOT/ATS只有一个，即忽略

*/
uint8_t CanGetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, uint16_t data_len, uint8_t dst_addr)
{
    IdStruct canId;
    if (data_len > MAX_BYTES)
	{
        return 0;
    }

    canId.bit.page  = page;
    canId.bit.dst   = dst_addr;                	    // 目的地址
    canId.bit.src   = esp_canbus_myself_address();  // 源地址
    canId.bit.funcode  = READ_DATA_CMD;	            // 命令码
    canId.bit.priority = LEVEL2;        	        // 传送优先级(默认读写为2级)

    if (!CanAckStart(node_id, canId.all, type, start, data_len, 0)) {
        return 0;
    }

    return 1;
}

/* Response Start
组帧发送，can beta
填充命令帧

node_id：CAN总线序号，not use

id:can tx ID
type:tx data[0]
start:tx data[1/2]
len:tx data[3/4]
crc16:tx data[5/6]




return:
0-fail
1-ok
*/
uint8_t CanAckStart(uint8_t node_id, uint32_t id, uint16_t type, uint16_t start, uint16_t len, uint16_t crc16)
{

	/* 内部CAN发送开始指令 */
    can_queue_msg_struct queue_msg;
    if (!xCanBusQueue_Send)
	{
        ESP_LOGE(TAG, "Can Bus Send Queue is NULL");
        return 0;
    }

    queue_msg.pdata = (twai_message_t *)heap_caps_malloc(sizeof(twai_message_t), MALLOC_CAP_SPIRAM); // MALLOC_CAP_SPIRAM
    if (!queue_msg.pdata) {
        ESP_LOGE(TAG, "malloc frame failed");
        return 0;
    }

    CmdStruct *cmd_data = (CmdStruct *)queue_msg.pdata->data;

    queue_msg.pdata->extd = 1;
    queue_msg.pdata->rtr = 0;
    queue_msg.pdata->self = 0;//windy

    queue_msg.pdata->identifier = id;
    queue_msg.pdata->data_length_code = 8;

    cmd_data->type  = type;
    cmd_data->start = start;
    cmd_data->total = len;
    cmd_data->crc16 = crc16;
    cmd_data->nc    = 0;//0x63 ，读取偏移量后全部数据

    if (xQueueSend(xCanBusQueue_Send, &queue_msg, pdMS_TO_TICKS(100)) != pdPASS)
	{
        free(queue_msg.pdata);
        return 0;
    }
    return 1;
}

/* Response Data
组帧发送，can beta
填充数据帧

node_id:CAN总线序号，固定0，不引用


return:
0-fail
1-ok

*/
uint8_t CanAckData(uint8_t node_id, uint32_t id, const uint8_t *raw, uint8_t len)
{


	/* 内部CAN发送数据 */
    can_queue_msg_struct queue_msg;

    if (!xCanBusQueue_Send) {
        ESP_LOGE(TAG, "Can Bus Send Queue is NULL");
        return 0;
    }

    queue_msg.pdata = (twai_message_t *)heap_caps_malloc(sizeof(twai_message_t), MALLOC_CAP_SPIRAM); //  MALLOC_CAP_SPIRAM
    if (!queue_msg.pdata) {
        ESP_LOGE(TAG, "malloc frame failed");
        return 0;
    }

	// ESP_LOGI(TAG,"id = 0x%lX",id);

    queue_msg.pdata->extd = 1;
    queue_msg.pdata->rtr = 0;
    queue_msg.pdata->self = 0;//windy

    queue_msg.pdata->identifier = id;
    queue_msg.pdata->data_length_code = len;

    memcpy(queue_msg.pdata->data, raw, len);

    if (xQueueSend(xCanBusQueue_Send, &queue_msg, pdMS_TO_TICKS(100)) != pdPASS) {
        free(queue_msg.pdata);
        return 0;
    }
    return 1;
}

/*

BLE给 CAN的转发
设置参数查表

*/

static const uint8_t* lookup_new_config(uint8_t type, uint16_t offset) {
    uint8_t *payload = NULL;
	ESP_LOGI(TAG,"lookup_new_config() type:%02x,offset:%d",
			type,offset);
    switch (type)
	{
        case PACK_TYPE_CONFIG_55H : payload = ((uint8_t*)&Inv_can_WR.bk_pack_dev_set.pack_config) + offset; break;
        case INV_TYPE_CONFIG00_1AH: payload = ((uint8_t*)&Inv_can_WR.bk_inv_dev_set.inv_set00)    + offset; break;
        case INV_TYPE_CONFIG01_1BH: payload = ((uint8_t*)&Inv_can_WR.bk_inv_dev_set.inv_set01)    + offset; break;
        case INV_TYPE_CONFIG02_1CH: payload = ((uint8_t*)&Inv_can_WR.bk_inv_dev_set.inv_set02)    + offset; break;
        case INV_TYPE_CONFIG03_1DH: payload = ((uint8_t*)&Inv_can_WR.bk_inv_dev_set.inv_set03)    + offset; break;
        case INV_TYPE_WIFI_23H:     payload = ((uint8_t*)&Inv_can_WR.bk_inv_dev_set.inv_wifi)     + offset; break;
		case INV_TYPE_CERT_27H:     payload = ((uint8_t*)&Inv_can_WR.bk_inv_dev_set.auth_param)   + offset; break;
		case IOT_TYPE_SET_02H : payload = ((uint8_t*)&Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set) + offset; break;
		case MODULE_TYPE_DC_AC_HUB_SET_40H:payload = ((uint8_t*)&Inv_can_mix_WR.dc_ac_hub_setting)   + offset; break;
		case MODULE_TYPE_D400S_SET_49H:payload = ((uint8_t*)&Inv_can_mix_WR.d400s_hub_sets)   + offset; break;
		default:  break;
    }
    return payload;
}

/*
将g_self_data结构体内容赋值给Inv[reals.Addr_can_self]，用于modbus beta 结构化统一交互
此处是IOT modbus表的具体赋值

当前一期：
仅实现主IOT和BLE/WIFI交互，
从IOT的 历史记录等需要通过CAN转给主IOT，即主IOT 主动查询 从IOT本地记录等

*/
void Iot_Self_Data_To_Can_data(void)
{
	uint8_t *ptr = 0;
	if(reals.Addr_can_self < DEV_MAIN_NODE_MAX)
	{
		//ESP_LOGI(TAG,"Iot_Self_Data_To_Can_data Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid::%s",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid);
		//ESP_LOGI(TAG,"Iot_Self_Data_To_Can_data g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid:%s",g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid);

		memcpy( &Inv[reals.Addr_can_self].mod_reg00000,&g_self_data.mod_reg00000, sizeof(MOD_STRUCT_reg00000));
		memcpy( &Inv[reals.Addr_can_self].mod_reg00700_OTA,&g_self_data.mod_reg00700_OTA, sizeof(MOD_STRUCT_reg00700));
		memcpy( &Inv[reals.Addr_can_self].mod_reg03000_Inv_history, &g_self_data.mod_reg03000_Inv_history,sizeof(MOD_STRUCT_reg03000));
		memcpy( &Inv[reals.Addr_can_self].mod_reg03500_Inv_yearX_statistic,&g_self_data.mod_reg03500_Inv_yearX_statistic, sizeof(MOD_STRUCT_reg03500));
		memcpy( &Inv[reals.Addr_can_self].mod_reg03600_Inv_year1_statistic,&g_self_data.mod_reg03600_Inv_year1_statistic, sizeof(MOD_STRUCT_reg03600));
		memcpy( &Inv[reals.Addr_can_self].mod_reg11000_IOT_info,&g_self_data.mod_reg11000_IOT_info, sizeof(MOD_STRUCT_reg11000));
		memcpy( &Inv[reals.Addr_can_self].mod_reg12000_IOT_set,&g_self_data.mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));


		memcpy( &Inv_can[reals.Addr_can_self].iot_data[0].mod_reg11000_IOT_info,&g_self_data.mod_reg11000_IOT_info, sizeof(MOD_STRUCT_reg11000));
		memcpy( &Inv_can[reals.Addr_can_self].iot_data[0].mod_reg12000_IOT_set,&g_self_data.mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));
		// 单机从机地址01、02也要赋值
		memcpy(&Inv[reals.Addr_can_self].mod_reg00000.match_stander.voltage_level, &Inv_can[reals.online_X_inv_index].inv_data[0].inv_about.voltage_lable,
			sizeof(Inv_can[0].inv_data[0].inv_about.voltage_lable));// 1 低字节
		memcpy(&Inv[reals.Addr_can_self].mod_reg00000.match_stander.area, &Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.ctrl_user_area,
			sizeof(Inv_can[0].inv_data[0].inv_set01.ctrl_user_area));// 1 高字节

//		if(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_FINISH)
//		{
//			ptr =(uint8_t*)&Inv_can[reals.Addr_can_self].iot_data[0].mod_reg12000_IOT_set;
//			for(uint16_t i = 0; i < sizeof(MOD_STRUCT_reg12000); i++)
//			{
//				ptr[i] =i;
//
//			}
//		}


	}



	memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000,&g_self_data.mod_reg00000, sizeof(MOD_STRUCT_reg00000));
	memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA,&g_self_data.mod_reg00700_OTA, sizeof(MOD_STRUCT_reg00700));
	memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history, &g_self_data.mod_reg03000_Inv_history,sizeof(MOD_STRUCT_reg03000));
	memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03500_Inv_yearX_statistic,&g_self_data.mod_reg03500_Inv_yearX_statistic, sizeof(MOD_STRUCT_reg03500));
	memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03600_Inv_year1_statistic,&g_self_data.mod_reg03600_Inv_year1_statistic, sizeof(MOD_STRUCT_reg03600));
	memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info,&g_self_data.mod_reg11000_IOT_info, sizeof(MOD_STRUCT_reg11000));
	memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set,&g_self_data.mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));
    memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open, &g_self_data.mod_reg13600_open, sizeof(MOD_STRUCT_reg13600));
	ESP_LOGI(TAG,"Iot_Self_Data_To_Can_data %s-%d ",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.iot_type,(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.iot_sn);
	// 该寄存器的值需与底层保持一致
	memcpy(&Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.voltage_level, &Inv_can[reals.online_X_inv_index].inv_data[0].inv_about.voltage_lable,
			sizeof(Inv_can[0].inv_data[0].inv_about.voltage_lable));// 1 低字节
	memcpy(&Inv[DEV_MAIN_NODE_MAX].mod_reg00000.match_stander.area, &Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.ctrl_user_area,
			sizeof(Inv_can[0].inv_data[0].inv_set01.ctrl_user_area));// 1 高字节

}


//windy add tbd
/*------------------------------------------------------------------------
*@Function :vLookupDataTab_Can
can beta查表

-------------------------------------------------------------------------*/
/**
*@brief
master_source:1- BLE/WIFI;2-ESP32->CC1312 uart

uint8_t SlaveAddr:modbus从机地址

*@param[iReadAddr]     寄存器起始地址
*@param[iReadNum]    寄存器数量
*@param[is_write]    1-WR;0-RD
*@param[* reg_position]    仅当WR有效，输出向下级传递的辅助信息

执行接收前，获取modbus INV全局变量指针地址
包含 下级CAN 和 IOT自身变量两部分



本函数不处理
can_label /type等CAN转发属性，
can_label /type 的写多字节 部分，集中到Modbus_To_CAN_Write_Info_After_Process()
can_label /type 的读取部分，基于 具体的读取表格和读取解析表格 CanLookupTypePosition()

该函数的主要功能是接受上层APP、上位机的读写指令，根据指令的地址和数量，返回对应的modbus表数据指针，不涉及表格的具体赋值




*@return
*/
const uint16_t* vLookupDataTab_Can(uint8_t master_source,uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, can_data_label *can_label, uint16_t *can_cmd_num, reg2_position_t* reg_position)
{
	uint16_t start = 0;
	const uint16_t *reg_ptr = NULL;

    ESP_LOGW(TAG, "vLookupDataTab_Can:  SlaveAddr=%d,startAddress=%u,writeRegsCnt=%u ",SlaveAddr,iReadAddr,iReadNum);
	// ESP_LOGW(TAG,"MOD_REG_LEN_40000=%d",MOD_REG_LEN_40000);
	// if((NULL == can_label) && (1 == is_write))//		||(SlaveAddr)
	// {
	// 	ESP_LOGE(TAG,"vLookupDataTab_Can:  can_label is NULL");
	// 	return NULL;
	// }

	if ((iReadAddr >= MOD_REG_START_ADDR_00000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000)))
	{
		start = MOD_REG_START_ADDR_00000;     /* 可读 */
		if (is_write != true) //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg00000;
			}
		}
		else //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg00000;
		}
	}

	else if ((iReadAddr >= MOD_REG_START_ADDR_00100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100)))
	{
	  	start = MOD_REG_START_ADDR_00100;	  /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg00100_AppPage1 read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1;

			}
			else//单INV
			{
				ESP_LOGI(TAG,"vLookupDataTab_Can Inv[%d].mod_reg00100_AppPage1.PvTotalChargingEnergy:%lu",SlaveAddr-1,Inv[SlaveAddr-1].mod_reg00100_AppPage1.PvTotalChargingEnergy);
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg00100_AppPage1;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_00700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00700 + MOD_REG_LEN_00700)))
	{
		start = MOD_REG_START_ADDR_00700;	  /* 可读 */
		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg00700_OTA write");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg00700_OTA;
			//app每次下发700OTA指令就初始化升级进度
			// ESP_LOGI(TAG,"000 ---Inv[3].mod_reg00700_OTA.ota_group[0].pct == %d, can_ota_status[0].ota_summary.pct ==%d, can_ota_status[0].devStatus[16].pct ==%d",
			// 	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[0].pct,can_ota_status[0].ota_summary.pct,can_ota_status[0].devStatus[16].pct);
			//当不处于can升级状态时开启新升级
			//if(can_ota_status[0].sender_work==0)
			{
				Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[0].pct = 0;
				can_ota_status[0].ota_summary.pct = 0;
				//memset(can_ota_status[0].devStatus,0,sizeof(can_ota_status[0].devStatus));
				if (can_ota_status[0].pblock_buf) {
					free(can_ota_status[0].pblock_buf);
					can_ota_status[0].pblock_buf = NULL;
					ESP_LOGW(TAG,"PRE OTA Buf Need Clean");
				}
				memset(&can_ota_status[0],0,sizeof(can_ota_status));
			}
			// //720
			// memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_cmd_group,(uint8_t *)&Inv_WR.mod_reg00700_OTA.ota_cmd.group,sizeof(Inv_WR.mod_reg00700_OTA.ota_cmd.group));

			// ESP_LOGI(TAG,"111 ---Inv[3].mod_reg00700_OTA.ota_group[0].pct == %d, can_ota_status[0].ota_summary.pct ==%d, can_ota_status[0].devStatus[16] ==%d",
			// 	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[0].pct,can_ota_status[0].ota_summary.pct,can_ota_status[0].devStatus[16].pct);

			// ESP_LOGW(TAG,"write Inv_WR.mod_reg00700_OTA:");
			// esp_log_buffer_hex(TAG, (uint8_t *)&Inv_WR.mod_reg00700_OTA, sizeof(Inv_WR.mod_reg00700_OTA));
			// ESP_LOGW(TAG,"write 000 ---Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA:");
			// esp_log_buffer_hex(TAG, (uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA, sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA));
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg00700_OTA read");
			// test 720
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_cmd_group,(uint8_t *)&Inv_WR.mod_reg00700_OTA.ota_cmd.group,sizeof(Inv_WR.mod_reg00700_OTA.ota_cmd.group));// 720
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_cmd.group,(uint8_t *)&Inv_WR.mod_reg00700_OTA.ota_cmd.group,sizeof(Inv_WR.mod_reg00700_OTA.ota_cmd.group));// 700~705

			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA;
				ESP_LOGW(TAG,"In mod_reg00700_OTA Reply:");
				esp_log_buffer_hex(TAG,reg_ptr,sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA));
				ESP_LOGI(TAG,"Inv[3].mod_reg00700_OTA.ota_cmd_group.dev_type == %02x, Inv[3].mod_reg00700_OTA.ota_cmd_group.dev_id == %02x",
					Inv[(DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_cmd_group.dev_type, Inv[3].mod_reg00700_OTA.ota_cmd_group.dev_id);
				ESP_LOGI(TAG,"Inv[3].mod_reg00700_OTA.ota_group[0].isOta == %02x, Inv[3].mod_reg00700_OTA.ota_group[0].where == %02x",
					Inv[(DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[0].isOta, Inv[3].mod_reg00700_OTA.ota_group[0].where);
				ESP_LOGI(TAG,"Inv[3].mod_reg00700_OTA.ota_group[0].type == %02x, Inv[3].mod_reg00700_OTA.ota_group[0].level == %02x",
					Inv[(DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[0].type, Inv[3].mod_reg00700_OTA.ota_group[0].level);
				ESP_LOGI(TAG,"Inv[3].mod_reg00700_OTA.ota_group[0].pct == %02x, Inv[3].mod_reg00700_OTA.ota_group[0].errCode == %02x",
					Inv[(DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[0].pct, Inv[3].mod_reg00700_OTA.ota_group[0].errCode);
				ESP_LOGI(TAG,"Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type == %02x, Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_id == %02x",
					Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type, Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_id);
				ESP_LOGI(TAG,"Inv[3].mod_reg00700_OTA.ota_cmd.group.dev_type == %02x, Inv[3].mod_reg00700_OTA.ota_cmd.group.dev_id == %02x",
					Inv[(DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_cmd.group.dev_type, Inv[3].mod_reg00700_OTA.ota_cmd.group.dev_id);
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg00700_OTA;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100)))
	{
		start = MOD_REG_START_ADDR_01100;	  /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			if(INV_TYPE_ABOUT_13H == reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].can_type)
			{
				if(0 == reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_cnt)
				{
					reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].modbus_Rd_cnt= 5;
				}
			}
			else//error bug
			{
				ESP_LOGE(TAG, "reals.can_poll[x].can_type=%d ",reals.can_poll[INDEX_INV_TYPE_ABOUT_13H].can_type);//
			}

			ESP_LOGI(TAG,"get in mod_reg01100_Inv_base read");
			// judge_pack_min_version();
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg01100_Inv_base;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01200) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01200 + MOD_REG_LEN_01200)))
	{
		start = MOD_REG_START_ADDR_01200;	  /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg01200_Inv_pv read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01200_Inv_pv;
				ESP_LOGI(TAG,"can_data mod_reg01200_Inv_pv.pv_detail[0].pv_number:%d",Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_number.pv_total);
				ESP_LOGI(TAG,"can_data mod_reg01200_Inv_pv.pv_detail[0].status:%u",Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_detail[0].status);
				ESP_LOGI(TAG,"can_data mod_reg01200_Inv_pv.pv_detail[0].input_type:%u",Inv[DEV_MAIN_NODE_MAX].mod_reg01200_Inv_pv.pv_detail[0].input_type);
			}
			else//单INV
			{
				ESP_LOGI(TAG,"mod_reg01200_Inv_pv.pv_number.dc_pv_numbers:%d",Inv[(SlaveAddr-1)].mod_reg01200_Inv_pv.pv_number.dc_pv_numbers);

				ESP_LOGI(TAG,"mod_reg01200_Inv_pv.pv_number.status 2:%d",Inv[(SlaveAddr-1)].mod_reg01200_Inv_pv.pv_detail[2].status);
				ESP_LOGI(TAG,"mod_reg01200_Inv_pv.pv_number.input_type 2:%u",Inv[(SlaveAddr-1)].mod_reg01200_Inv_pv.pv_detail[2].input_type);
				ESP_LOGI(TAG,"mod_reg01200_Inv_pv.pv_number.input_power 2:%u",Inv[(SlaveAddr-1)].mod_reg01200_Inv_pv.pv_detail[2].input_power);
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg01200_Inv_pv;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300)))
	{
		start = MOD_REG_START_ADDR_01300;	  /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg01300_Inv_grid read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01300_Inv_grid;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg01300_Inv_grid;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01400) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400)))
	{
		start = MOD_REG_START_ADDR_01400;	  /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg01400_Inv_load read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01400_Inv_load;
				ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_phase_number:%d",Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_phase_number);
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg01400_Inv_load;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01500 + MOD_REG_LEN_01500)))
	{
		start = MOD_REG_START_ADDR_01500;	  /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg01500_Inv_inv read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01500_Inv_inv;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg01500_Inv_inv;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01600 + MOD_REG_LEN_01600)))
	{
		start = MOD_REG_START_ADDR_01600;	  /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg01600_Inv_generator read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01600_Inv_generator;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg01600_Inv_generator;
			}
		}
	}
    else if ((iReadAddr >= MOD_REG_START_ADDR_01700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01700 + MOD_REG_LEN_01700)))
    {
        start = MOD_REG_START_ADDR_01700;	  /* 可读 */
        if (true == is_write) //write
        {

        }
        else //read
        {
            ESP_LOGI(TAG,"get in mod_reg01700_meter read");
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&Meter[METER_MAX_NUM].mod_reg01700_meter;
            }
            else//单INV
            {
                reg_ptr = (const uint16_t*)&Meter[SlaveAddr-1].mod_reg01700_meter;
            }
        }
    }
    else if ((iReadAddr >= MOD_REG_START_ADDR_01900) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01900 + MOD_REG_LEN_01900)))
    {
        start = MOD_REG_START_ADDR_01900;	  /* 可读 */
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&Meter_wr.mod_reg01900_meter;
        }
        else //read
        {
            ESP_LOGI(TAG,"get in mod_reg01700_meter read");
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&Meter[METER_MAX_NUM].mod_reg01900_meter;
            }
            else//单INV
            {
                reg_ptr = (const uint16_t*)&Meter[SlaveAddr-1].mod_reg01900_meter;
            }
        }
    }
	else if ((iReadAddr >= MOD_REG_START_ADDR_02000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000)))
	{
		start = MOD_REG_START_ADDR_02000;	  /* 可读 */
		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg02000_Inv_base_set write");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg02000_Inv_base_set;
			// if(NULL != can_label)
			// {
			// 	can_label->type = INV_TYPE_CONFIG00;
			// 	can_label->offset = ((iReadAddr -start)*2);//这个到底是谁的偏移量 ->((iReadAddr -start)*2)相对于modbus的，can下发命令时用来查自己时看偏移了多少
			// 	can_label->len = iReadNum*2;				//modbus表的偏移量拿来给CAN表用？？？ 用后面的Modbus_Write_Info_Process覆盖offset 并修正发送字节数
			// 	can_label->active_can_cmd_type = 0; //tbd
			// }
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg02000_Inv_base_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set;
				//ESP_LOGI(TAG,"get in mod_reg02000_Inv_base_set self_config:0x%x",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.self_config.all);
				//ESP_LOGI(TAG,"get in mod_reg02000_Inv_base_set level:%d",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.LevelSwitch.bit.level);
				ESP_LOGI(TAG,"get in mod_reg02000_Inv_base_set pv_senior_set:%u",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.pv_senior_set.all);
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg02000_Inv_base_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_02200) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200)))
	{
		start = MOD_REG_START_ADDR_02200;	  /* 可读 */
		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg02200_Inv_advance_set write");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg02200_Inv_advance_set;
			// if(NULL != can_label)
			// {
			// 	can_label->type = INV_TYPE_CONFIG01;
			// 	can_label->offset = (iReadAddr -start)*2;//用后面的Modbus_Write_Info_Process覆盖offset 并修正发送字节数
			// 	can_label->len = iReadNum*2;
			// 	can_label->active_can_cmd_type = 0; //tbd
			// }
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg02200_Inv_advance_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02200_Inv_advance_set;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg02200_Inv_advance_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_02300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02300 + MOD_REG_LEN_02300)))
	{
		start = MOD_REG_START_ADDR_02300;	  /* 可读 */
		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg02300_Inv_set02_struct write");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg02300_Inv_set02_struct;
			// if(NULL != can_label)
			// {
			// 	can_label->type = INV_TYPE_CONFIG02;
			// 	can_label->offset = (iReadAddr -start)*2;
			// 	can_label->len = iReadNum*2;
			// 	can_label->active_can_cmd_type = 0; //tbd
			// }
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg02300_Inv_set02_struct read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02300_Inv_set02_struct;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg02300_Inv_set02_struct;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_02400) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400)))
	{
		start = MOD_REG_START_ADDR_02400;	  /* 可读 */
		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg02400_Inv_certification write");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg02400_Inv_certification;
			// if(NULL != can_label)
			// {
			// 	can_label->type = INV_TYPE_CONFIG03;
			// 	can_label->offset = (iReadAddr -start)*2;
			// 	can_label->len = iReadNum*2;
			// 	can_label->active_can_cmd_type = 0; //tbd
			// }

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg02400_Inv_certification read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02400_Inv_certification;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg02400_Inv_certification;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_02500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02500 + MOD_REG_LEN_02500)))
	{
		start = MOD_REG_START_ADDR_02500;     /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg02500_Inv_advance_set2;
		}
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02500_Inv_advance_set2;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg02500_Inv_advance_set2;
			}
		}
	}


	else if ((iReadAddr >= MOD_REG_START_ADDR_03000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_03000 + MOD_REG_LEN_03000)))//read
	{
		start = MOD_REG_START_ADDR_03000;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg03000_Inv_history;
		}
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg03000_Inv_history;
			}

		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_03500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_03500 + MOD_REG_LEN_03500)))//read
	{
		start = MOD_REG_START_ADDR_03500;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg03500_Inv_yearX_statistic;
		}
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03500_Inv_yearX_statistic;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg03500_Inv_yearX_statistic;
			}

		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_03600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_03600 + MOD_REG_LEN_03600)))//read
	{
		start = MOD_REG_START_ADDR_03600;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg03600_Inv_year1_statistic;
		}
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03600_Inv_year1_statistic;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg03600_Inv_year1_statistic;
			}

		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_06000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_06000 + MOD_REG_LEN_06000)))//read
	{
		start = MOD_REG_START_ADDR_06000;	  /* 可读 */
		if (true == is_write) //write
		{
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg06000_Pack_sum read");
			//single_pack_information();
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg06000_Pack_sum;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_06100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_06100 + MOD_REG_LEN_06100)))//read
	{
		start = MOD_REG_START_ADDR_06100;	  /* 可读 */
		if (true == is_write) //write
		{
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg06100_Pack_each read");
			judge_pack_min_version();
			//single_pack_information();
			//pack_infos_update();
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each;//Inv
				ESP_LOGI(TAG,"SlaveAddr 0 sn_code:%llu",Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.sn_code);
			}
			else if(SlaveAddr<(PACK_MAX_NUM*DEV_MAIN_NODE_MAX))//单INV
			{
				reg_ptr = (const uint16_t*)&Inv_Pack[SlaveAddr-1].mod_reg06100_Pack_each;
			}
			else if(SlaveAddr>=SLAVE_PACK_READ_ADDR)
			{
				reg_ptr = (const uint16_t*)&Inv_Pack_Slave[SlaveAddr-SLAVE_PACK_READ_ADDR].mod_reg06100_Pack_each;
			}
		}
	}
	// else if ((iReadAddr >= MOD_REG_START_ADDR_06300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_06300 + MOD_REG_LEN_06300 * (iReadNum-2)/20)))//动态读取大小
	else if ((iReadAddr >= MOD_REG_START_ADDR_06300) && (iReadAddr < MOD_REG_START_ADDR_07000))
	{
		if((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_06300 + MOD_REG_LEN_06300 * (iReadNum-2)/20) || (iReadNum == 0))//动态读取大小 iReadNum为0时 也要返回0的报文
		{
			start = MOD_REG_START_ADDR_06300;	  /* 可读 */
			if (true == is_write) //write
			{
			}
			else //read
			{
				ESP_LOGI(TAG,"get in mod_reg06300_Pack_cell read");
				//ESP_LOGI(TAG,"eneter mod_reg06300_Pack can_ota_status[%d] ota_doing:%d ,pct:%d,isOta:%d,type:%d",0,can_ota_status[0].ota_doing,can_ota_status[0].devStatus[0].pct,
					//can_ota_status[0].devStatus[0].isOta,can_ota_status[0].devStatus[0].type);
				//single_pack_information();
				//pack_infos_update();
				if(0 == SlaveAddr)//汇总
				{
					// reg_ptr = (const uint16_t*)&Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06300_Pack_cell;//Inv
					//Pack_6300_offset(str_res_6300,(iReadNum-2)/20,(INV_MAX_NUM*DEV_MAIN_NODE_MAX));//(iReadNum-2)/10
					//reg_ptr = (const uint16_t*)str_res_6300;
					reg_ptr = (const uint16_t*)&Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06300_Pack_cell[0];//Inv
				}
				else if(SlaveAddr<(PACK_MAX_NUM*DEV_MAIN_NODE_MAX))
				{
					//Pack_6300_offset(str_res_6300,(iReadNum-2)/20,SlaveAddr-1);//
					//reg_ptr = (const uint16_t*)str_res_6300;
					reg_ptr = (const uint16_t*)&Inv_Pack[SlaveAddr-1].mod_reg06300_Pack_cell[0];
				}
				else if((SlaveAddr>=SLAVE_PACK_READ_ADDR)&&(SlaveAddr<(SLAVE_PACK_READ_ADDR+SLAVE_PACK_MAX_NUM)))//单INV
				{
					// reg_ptr = (const uint16_t*)&Inv_Pack[SlaveAddr-1].mod_reg06300_Pack_cell;
					reg_ptr = (const uint16_t*)&Inv_Pack_Slave[SlaveAddr-SLAVE_PACK_READ_ADDR].mod_reg06300_Pack_cell[0];
				}
				ESP_LOGI(TAG,"after mod_reg06300_Pack can_ota_status[%d] ota_doing:%d ,pct:%d,isOta:%d,type:%d",0,can_ota_status[0].ota_doing,can_ota_status[0].devStatus[0].pct,
				can_ota_status[0].devStatus[0].isOta,can_ota_status[0].devStatus[0].type);
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_07000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_07000 + MOD_REG_LEN_07000)))
	{
		start = MOD_REG_START_ADDR_07000;     /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg07000_Pack_set;
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg07000_Pack_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07000_Pack_set;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg07000_Pack_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_07200) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_07200 + (MOD_REG_LEN_07200 * iReadNum/10))))//动态读取大小
	{
		start = MOD_REG_START_ADDR_07200;     /* 可读 */
		if (true == is_write) //write
		{

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg07200_Pack_bmu read");
			//single_pack_information();
			//pack_infos_update();
			if(0 == SlaveAddr)//汇总
			{
				Pack_7200_offset(str_res_7200,iReadNum/10,(INV_MAX_NUM*DEV_MAIN_NODE_MAX)); //要读的寄存器数量/除以10，得到要读的pack数量 此处(INV_MAX_NUM*DEV_MAIN_NODE_MAX):3表示类型为内置电池包
				reg_ptr = (const uint16_t*)str_res_7200;
			}
			else if(SlaveAddr<SLAVE_PACK_READ_ADDR)//单INV // 此处从机地址表示pack类型
			{
				Pack_7200_offset(str_res_7200,iReadNum/10, SlaveAddr-1); //要读的寄存器数量/除以10，得到要读的pack数量 此处(SlaveAddr-1):0、1、2表示类型分别为B300K、B300S、B300
				reg_ptr = (const uint16_t*)str_res_7200;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_11000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_11000 + MOD_REG_LEN_11000)))//IOT自身数据，不往下转发
	{
		start = MOD_REG_START_ADDR_11000;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg11000_IOT_info;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg11000_IOT_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info;
			}
			else if((SlaveAddr>0)&&(SlaveAddr<=DEV_MAIN_NODE_MAX))//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg11000_IOT_info;
ESP_LOGI(TAG, "g_self_data.mod_reg11000_IOT_info =%d,%d,%d,%d,",g_self_data.mod_reg11000_IOT_info.sta_ipv4[0],g_self_data.mod_reg11000_IOT_info.sta_ipv4[1],g_self_data.mod_reg11000_IOT_info.sta_ipv4[2],g_self_data.mod_reg11000_IOT_info.sta_ipv4[3]);
ESP_LOGI(TAG, "Inv[SlaveAddr-1].mod_reg11000_IOT_info =%d,%d,%d,%d,",Inv[SlaveAddr-1].mod_reg11000_IOT_info.sta_ipv4[0],Inv[SlaveAddr-1].mod_reg11000_IOT_info.sta_ipv4[1],Inv[SlaveAddr-1].mod_reg11000_IOT_info.sta_ipv4[2],Inv[SlaveAddr-1].mod_reg11000_IOT_info.sta_ipv4[3]);

			}
			else if((SlaveAddr>=D400S_SALVE_ADDR)&&(SlaveAddr<(D400S_SALVE_ADDR+DEV_MAIN_NODE_MAX)))
			{
				ESP_LOGI(TAG,"d400s mod_reg11000_IOT_info iot_sn:%llu",Inv_D400S[SlaveAddr-D400S_SALVE_ADDR].mod_reg11000_IOT_info.iot_sn);
				ESP_LOGI(TAG,"d400s mod_reg11000_IOT_info software_ver:%lu",Inv_D400S[SlaveAddr-D400S_SALVE_ADDR].mod_reg11000_IOT_info.software_ver);
				reg_ptr = (const uint16_t*)&Inv_D400S[SlaveAddr-D400S_SALVE_ADDR].mod_reg11000_IOT_info;
			}else if(SlaveAddr==D400S_GROUP_ADDR)
			{
				reg_ptr = (const uint16_t*)&Inv_D400S[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_12000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000)))//IOT自身数据，不往下转发
	{
		start = MOD_REG_START_ADDR_12000;	  /* 可读 */
		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg12000_IOT_set write");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg12000_IOT_set;
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg12000_IOT_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg12000_IOT_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_13000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_13000 + MOD_REG_LEN_13000)))//read
	{
		start = MOD_REG_START_ADDR_13000;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg13000_3rd_WIFI;

		}
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13000_3rd_WIFI;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg13000_3rd_WIFI;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_13500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_13500 + MOD_REG_LEN_13500)))
	{
		start = MOD_REG_START_ADDR_13500;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg13500_mesh;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg13500_mesh read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13500_mesh;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg13500_mesh;
			}
		}
	}
    else if ((iReadAddr >= MOD_REG_START_ADDR_13600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_13600 + MOD_REG_LEN_13600)))
	{
		start = MOD_REG_START_ADDR_13600;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg13600_open;
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg13600_open read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg13600_open;
			}
		}
	}
//	  else if ((iReadAddr >= MOD_REG_START_ADDR_14000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14000 + MOD_REG_LEN_14000)))
//	{
//		start = MOD_REG_START_ADDR_14000;	  /* 可读 */
//		if (is_write != true) //read
//		{
//			reg_ptr = (const uint16_t*)&MicroInv[SlaveAddr].mod_reg14000_HMI_info;
//		}
//		else //write
//		{
//			reg_ptr = (const uint16_t*)&MicroInv_WR[SlaveAddr].mod_reg14000_HMI_info;
//		}
//	}
//
	else if ((iReadAddr >= MOD_REG_START_ADDR_14500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500)))
	{
		start = MOD_REG_START_ADDR_14500;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg14500_SmartPlug_info;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg14500_SmartPlug_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg14500_SmartPlug_info;
			}
			else if(SlaveAddr >= MODBUS_SLAVE_ADDR_WIFI_S1_START)//plug从 200开始
			{
				reg_ptr = (const uint16_t*)&Plug[SlaveAddr - MODBUS_SLAVE_ADDR_WIFI_S1_START].mod_reg14500_SmartPlug_info;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_14700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700)))
	{
		start = MOD_REG_START_ADDR_14700;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg14700_SmartPlug_set;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg14700_SmartPlug_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg14700_SmartPlug_set;
			}
			else if(SlaveAddr >= MODBUS_SLAVE_ADDR_WIFI_S1_START)//plug从 200开始
			{
				reg_ptr = (const uint16_t*)&Plug[SlaveAddr - MODBUS_SLAVE_ADDR_WIFI_S1_START].mod_reg14700_SmartPlug_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_15500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15500 + MOD_REG_LEN_15500)))
	{
		start = MOD_REG_START_ADDR_15500;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg15500_D400s_info;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg15500_Dc_Hub_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15500_D400s_info;
			}
			else if((SlaveAddr>0)&&(SlaveAddr<DEV_MAIN_NODE_MAX))//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg15500_D400s_info;
			}
			else if((SlaveAddr>=D400S_SALVE_ADDR)&&(SlaveAddr<(D400S_SALVE_ADDR+DEV_MAIN_NODE_MAX)))//单INV
			{
				reg_ptr = (const uint16_t*)&Inv_D400S[SlaveAddr-D400S_SALVE_ADDR].mod_reg15500_D400s_info;
			}else if(SlaveAddr==D400S_GROUP_ADDR)
			{
				reg_ptr = (const uint16_t*)&Inv_D400S[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info;
				ESP_LOGI(TAG,"Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareType:%u",Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareType);
				ESP_LOGI(TAG,"Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareVersion:%lu",Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareVersion);
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_15600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600)))
	{
		start = MOD_REG_START_ADDR_15600;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg15600_D400s_set;
		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg15600_Dc_Hub_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15600_D400s_set;
			}
			else if((SlaveAddr>0)&&(SlaveAddr<DEV_MAIN_NODE_MAX))//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg15600_D400s_set;
			}
			else if((SlaveAddr>=D400S_SALVE_ADDR)&&(SlaveAddr<(D400S_SALVE_ADDR+DEV_MAIN_NODE_MAX)))//单INV
			{
				reg_ptr = (const uint16_t*)&Inv_D400S[SlaveAddr-D400S_SALVE_ADDR].mod_reg15600_D400s_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_15700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15700 + MOD_REG_LEN_15700)))
	{
		start = MOD_REG_START_ADDR_15700;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg15700_Dc_Hub_info;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg15700_Dc_Hub_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15700_Dc_Hub_info;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg15700_Dc_Hub_info;
				ESP_LOGI(TAG,"BBB Inv[SlaveAddr-1].mod_reg15700_Dc_Hub_info.cig2_output_power:%u ",Inv[SlaveAddr-1].mod_reg15700_Dc_Hub_info.cig2_output_power);
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_15750) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15750 + MOD_REG_LEN_15750)))
	{
		start = MOD_REG_START_ADDR_15750;	  /* 可读 */

		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg15750_Dc_Ac_Hub_set write");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg15750_Dc_Ac_Hub_set;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg15750_Dc_Ac_Hub_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15750_Dc_Ac_Hub_set;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg15750_Dc_Ac_Hub_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_15800) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15800 + MOD_REG_LEN_15800)))
	{
		start = MOD_REG_START_ADDR_15800;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg15800_Ac_Hub_info;//MOD_STRUCT_reg15800

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg15800_Ac_Hub_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15800_Ac_Hub_info;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg15800_Ac_Hub_info;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_19000) && ((iReadAddr + iReadNum) <= (MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000)))
	{
		start = MOD_REG_START_ADDR_19000;
		if (true == is_write)
		{
			reg_ptr = (const uint16_t *)&Inv_WR.mod_reg19000_Relay_ctrl;
		}
		else
		{
			reg_ptr = (const uint16_t *)&RelaySetData.Relay_info_t.PowerRelay_SmartSet01;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_26000) && ((iReadAddr + iReadNum) <= (MOD_REG_START_ADDR_26000 + MOD_REG_LEN_26000)))
	{
		start = MOD_REG_START_ADDR_26000;
		if (true == is_write)
		{
			reg_ptr = (const uint16_t *)Smart_Tou_Ctrl_Config_Get_Ptr(1);
		}
		else
		{
			reg_ptr = (const uint16_t *)Smart_Tou_Ctrl_Config_Get_Ptr(0);
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_21000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_21000 + MOD_REG_LEN_21000)))//公版网页服务器：配置区
	{
		start = MOD_REG_START_ADDR_21000;
		//读和写的含义不同

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg21000_bind_WR;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg21000_bind_WR read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind_WR;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg21000_bind_WR;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_22000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_22000 + MOD_REG_LEN_22000)))//公版网页服务器：配置区
	{
		start = MOD_REG_START_ADDR_22000;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg22000_net_server_2rd;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg22000_net_server_2rd read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg22000_net_server_2rd;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg22000_net_server_2rd;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_29700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_29700 + MOD_REG_LEN_29700)))
	{
		ESP_LOGE(TAG,"get in mod_reg29700_IOT_info");
		start = MOD_REG_START_ADDR_29700;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg29700_IOT_info;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg29700_IOT_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg29700_IOT_info;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg29700_IOT_info;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_40000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000)))
	{

		ESP_LOGE(TAG,"get in mod_reg40000_transparent");
		start = MOD_REG_START_ADDR_40000;	  /* 可读 */

		if (true == is_write) //write
		{
			ESP_LOGE(TAG,"get in mod_reg40000_transparent write!!!!!!!!");
			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg40000_transparent;

		}
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg40000_transparent read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg40000_transparent;
			}
			else//单INV
			{
				reg_ptr = (const uint16_t*)&Inv[SlaveAddr-1].mod_reg40000_transparent;
			}
		}
	}

	else
	{
//		start = MOD_REG_START_ADDR_03600;	  /* 可读 */
//		if (true == is_write) //write
//		{
//		}
//		else //read
//		{
//			reg_ptr = (const uint16_t*)&Inv_WR.mod_reg03600_Inv_year1_statistic;
//
//			can_label->type = INV_TYPE_CURR_ENERGY;
//			can_label->offset = (iReadAddr -start)*2;
//			can_label->len = iReadNum*2;
//			can_label->active_can_cmd_type = 0; //tbd
//		}
		ESP_LOGE(TAG,"vLookupDataTab_Can   ADDRESS  NULL  ERROR !!! startAddress:%u",iReadAddr);

	}


	if(NULL != reg_position)
	{
			reg_position->reg_addr = start;
		//		reg_position->can_type=0;
			reg_position->offset= (iReadAddr -start)*2;
			reg_position->len=iReadNum*2;
// ESP_LOGI(TAG, "windy tab_can :reg_position->reg_addr=%d,reg_position->offset=%d,reg_position->len=%d  ",reg_position->reg_addr,reg_position->offset,reg_position->len);
// 	ESP_LOGI(TAG, "windy tab_can22 ：iReadAddr=%d,start=%d  ",iReadAddr,start);

	}


	if (reg_ptr)
	{
		return (reg_ptr + (iReadAddr - start));
	}
	else
	{
		return NULL;
	}
}

/*
先写入，后回读
can 多节点操作，多字节写，自己做主

*/
static void can_broadcast_cmd_inv(can_data_label *cmd_label, const uint8_t *payload)
{
//	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
//	{
//		for(int i = 0; i < INV_MAX_NUM; i++)
//		{
//			if (Inv_can[node_id].inv_data[i].online)		// 该总线有逆变设备在线才发送数据
//			{
//				CanSetTarget(node_id, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);
//				break;
//			}
//		}
//	}

	CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);

	vTaskDelay(pdMS_TO_TICKS(100));
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			if (Inv_can[node_id].inv_data[i].online)		// 该总线有逆变设备在线才回读数据
			{
				CanGetTarget(node_id, 0, cmd_label->type, cmd_label->offset, cmd_label->len, INV_CAN_ADDR+node_id);//wIndy:回读意义不太大
				break;
			}
		}
	}
}

/*
先写入，后回读
can 多节点操作，多字节写，自己做主

*/
static void can_broadcast_cmd_iot(can_data_label *cmd_label, const uint8_t *payload)
{
//	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
//	{
//		for(int i = 0; i < IOT_MAX_NUM; i++)
//		{
//			if (Inv_can[node_id].iot_data[i].online)		// 该总线有逆变设备在线才发送数据
//			{
//				CanSetTarget(node_id, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);
//				break;
//			}
//		}
//	}

	CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);

	//windy屏蔽反向读取，减少拥堵

	vTaskDelay(pdMS_TO_TICKS(100));
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < IOT_MAX_NUM; i++)
		{
			if ((Inv_can[node_id].iot_data[i].online)		// 该总线有逆变设备在线才回读数据
			&&(node_id != reals.Addr_can_self))//排除自己
			{
				CanGetTarget(0, 0, cmd_label->type, cmd_label->offset, cmd_label->len, IOT_CAN_ADDR+i);//wIndy:回读意义不太大
				break;
			}
		}
	}
}

/*
先写入，后回读
can 多节点操作，多字节写，自己做主

*/
static void can_broadcast_cmd_pack(can_data_label *cmd_label, const uint8_t *payload)
{
//	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
//	{
//		for(int i = 0; i < PACK_MAX_NUM; i++)
//		{
//			if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))		// 该总线有PACK设备在线才发送数据
//			{
//				CanSetTarget(node_id, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, PACK_CAN_ADDR+i);
//				vTaskDelay(pdMS_TO_TICKS(20));
//				continue;
//			}
//		}
//	}
	CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);

	//windy屏蔽反向读取，减少拥堵

	vTaskDelay(pdMS_TO_TICKS(50));
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < PACK_MAX_NUM; i++)
		{
			if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))		// 该总线有PACK设备在线才回读数据
			{
				CanGetTarget(node_id, 0, cmd_label->type, cmd_label->offset, cmd_label->len, PACK_CAN_ADDR+i);
				vTaskDelay(pdMS_TO_TICKS(20));
				continue;
			}
		}
	}
}

/*
先写入，后回读
can 多节点操作，多字节写，自己做主

*/
static void can_broadcast_cmd_dc_ac_parts(can_data_label *cmd_label, const uint8_t *payload)
{
	CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);

	vTaskDelay(pdMS_TO_TICKS(50));

	if (reals.online_ACHUB_num == 1)		// 该总线有ACHUB设备在线才回读数据
	{
		CanGetTarget(0, 0, cmd_label->type, cmd_label->offset, cmd_label->len, AC_HUB_CAN_ADDR);
		vTaskDelay(pdMS_TO_TICKS(20));
	}

}

/*
先写入，后回读
can 多节点操作，多字节写，自己做主

*/
static void can_broadcast_cmd_d400s_set(can_data_label *cmd_label, const uint8_t *payload)
{
	CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);

	vTaskDelay(pdMS_TO_TICKS(50));

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			if (Inv_can[node_id].d400s_data[i].online==1)		// 该总线有ACHUB设备在线才回读数据
			{
				CanGetTarget(node_id, 0, cmd_label->type, cmd_label->offset, cmd_label->len, D400S_CAN_ADDR+node_id);
				vTaskDelay(pdMS_TO_TICKS(20));
			}
		}
	}
}

/*
can 单节点操作，多字节写，自己做主

*/
static void can_unicasst_cmd(uint8_t md_addr, can_data_label *cmd_label, const uint8_t *payload)
{
	uint8_t dev_id;			//
	uint8_t node = 0xff;//can结构体一维地址
	uint8_t idx = 0xff;//can结构体二维地址

	/*modbus slave addr转换为can  目标地址 ，根据地址得到节点号与设备索引 */
	if (IS_INV_ADDR(md_addr))//IS_IOT_ADDR
	{
		node = (md_addr - MD_INV_ADDR_START) / INV_MAX_NUM;
		idx = (md_addr - MD_INV_ADDR_START) % INV_MAX_NUM;
		ESP_LOGI(TAG, "INV_ADDR node=%d, idx=%d", node, idx);
	}
	else if (IS_PACK_ADDR(md_addr))
	{
		node = PACK_ADDR_TO_GRP(md_addr);
		idx = PACK_ADDR_TO_IDX(md_addr);
		ESP_LOGI(TAG, "PACK_ADDR node=%d, idx=%d", node, idx);
	}

	if ((node != 0xff) && (idx != 0xff))
	{
		// printf("can_unicasst_cmd --------00000000\n");
		uint8_t dev_online = 0;

		/* 是逆变设置指令 */
		if ((cmd_label->type >= INV_TYPE_CONFIG00_1AH) && (cmd_label->type <= INV_TYPE_CERT_27H))
		{
			// printf("can_unicasst_cmd --------1111111111\n");
			 if (Inv_can[node].inv_data[idx].online)
			{
				dev_id = node*INV_MAX_NUM +idx + INV_CAN_ADDR;// idx
				dev_online = 1;
				ESP_LOGI(TAG, "INV node=%d, idx=%d, dev_id=%d", node, idx, dev_id);
			}
		}
		/* 是IOT设置指令 */
		else if ((cmd_label->type >= IOT_TYPE_SET_02H) && (cmd_label->type <= IOT_TYPE_SET_02H))
		{
			// printf("can_unicasst_cmd --------1111111111\n");
			 if (Inv_can[node].iot_data[idx].online)
			{
				dev_id = node*IOT_MAX_NUM +idx + IOT_CAN_ADDR;// idx
				dev_online = 1;
				ESP_LOGI(TAG, "IOT node=%d, idx=%d, dev_id=%d", node, idx, dev_id);
			}
		}
		/* 是PACK设置指令 */
		else if (cmd_label->type == PACK_TYPE_CONFIG_55H)
		{
			if (Inv_can[node].pack_data[0].pack_announce.online & ((uint16_t)1 << idx))
			{
				dev_id = node*PACK_MAX_NUM +idx + PACK_CAN_ADDR;
				dev_online = 1;
				ESP_LOGI(TAG, "PACK node=%d, idx=%d, dev_id=0x%x", node, idx, dev_id);

			}
		}

		/* 相应设备在线才发送设置指令 */
		if (dev_online)
		{
			if (CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, dev_id))	// CAN设置新设置,ok
			{
			//windy屏蔽反向读取，减少拥堵
				vTaskDelay(pdMS_TO_TICKS(50));
				CanGetTarget(0, 0, cmd_label->type, cmd_label->offset, cmd_label->len, dev_id);			// CAN回读新设置
			}
			else//fail
			{
				ESP_LOGE(TAG, "cmd_label->type=0x%x,  dev_id=0x%x", cmd_label->type, dev_id);
			}
		}
	}
}

/*

BLE给 CAN的转发

md_addr:modbus slave addr

num：需要转换成x条CAN命令

*/
void CanCmdGenerate(uint8_t md_addr, can_data_label *cmd_label, uint16_t num)
{
	ESP_LOGI(TAG,"CanCmdGenerate");
    if (!cmd_label || !num) return; 							// 参数检查
	ESP_LOGI(TAG,"CanCmdGenerate 2 num:%d",num);
    for (uint16_t i = 0; i < num; i++)
	{
//		if (cmd_label[i].type == 0x01)
//		{
//            return ;
//		}
	ESP_LOGI(TAG,"cmd_label[%d].type=%d,cmd_label[%d].offset=%d", i, cmd_label[i].type, i, cmd_label[i].offset);//testwx
		if (((IOT_TYPE_SET_02H == cmd_label[i].type)&&(reals.online_Iot_num > 1))
			||((PACK_TYPE_CONFIG_55H == cmd_label[i].type)&&(reals.online_Pack_num > 0))
			||(((cmd_label[i].type >= INV_TYPE_CONFIG00_1AH) && (cmd_label[i].type <= INV_TYPE_CERT_27H))
				&&(reals.online_Inv_num > 0))
			||((MODULE_TYPE_DC_AC_HUB_SET_40H == cmd_label[i].type)&&(reals.online_ACHUB_num > 0))
			||((MODULE_TYPE_D400S_SET_49H == cmd_label[i].type))//&&(reals.online_D400S_num > 0))
			)
		{


		/* 离网并机模式下AC电表使能开关不下发给底层设备 */
//		inv_advance_param_t *advance_param = ll_inv_param_get("inv_advance_param");
//		if(advance_param != NULL)
//		{
//			if (advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1)
//			{
//				if ((cmd_label[i].type == INV_TYPE_CONFIG01) && (cmd_label[i].offset == offsetof(inv_set01_struct, ctrl_meter_enable))) {
//					continue;
//				}
//			}
//		}

		/* 下发的数据不存在则继续查找下一条指令 */
		ESP_LOGI(TAG,"cmd_label[%d].type=%d,cmd_label[%d].offset=%d", i, cmd_label[i].type, i, cmd_label[i].offset);//testwx
        const uint8_t *payload = lookup_new_config(cmd_label[i].type, cmd_label[i].offset); // 查找需要下发的数据
		//ESP_LOGI(TAG,"payload[0]:%d,[1]:%d,[2]:%d,[3]:%d,[4]:%d,[5]:%d", payload[0],payload[1],payload[2],payload[3],payload[4],payload[5]);//testwx
		// if(SetData_Can.dev_info_t2.inv_set00.ctrl.all!=Inv_can[reals.online_Y_inv_index].inv_data[reals.online_Y_inv_index].inv_set00.ctrl.all)
		// {
		// 	Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.all=SetData_Can.dev_info_t2.inv_set00.ctrl.all;
		// }
		ESP_LOGI(TAG,"huangji 2006 ctrl_all:%d",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.bit.clear_all);
		if (payload == NULL) continue;

		/* 由于历史原因部分设备不识别杂项使能的高字节,当该项高字节为0时不往下转发 */
		if (cmd_label[i].type == INV_TYPE_CONFIG01_1BH)
		{
			if ((cmd_label[i].offset == offsetof(inv_set01_struct, ctrl_mix)) && (cmd_label[i].len == 2))
			{
				if (payload[1] == 0)	// 高字节为0时不发送高字节数据
				{
					ESP_LOGW(TAG, "ctrl_mix length set to 1");
					cmd_label[i].len = 1;
				}
			}
		}
		ESP_LOGW(TAG, "modbus --->>> can: modbus addr=0x%x, type=0x%x, offset=%d, len=%d, i=%d",
						md_addr, cmd_label[i].type, cmd_label[i].offset, cmd_label[i].len, i);

			/* 广播指令读取 */
			 if(cmd_label[i].type == IOT_TYPE_SET_02H) //IOT直接广播
			{
				ESP_LOGW(TAG, "go to  can_broadcast_cmd_iot");
				can_broadcast_cmd_iot(&cmd_label[i], payload);
			}
			else if (IS_SUMMARY_ADDR(md_addr))
			{
				if ((cmd_label[i].type >= INV_TYPE_CONFIG00_1AH) && (cmd_label[i].type <= INV_TYPE_CERT_27H))
				{
					ESP_LOGW(TAG, "go to can_broadcast_cmd_inv");
					can_broadcast_cmd_inv(&cmd_label[i], payload);
				}
				else if(cmd_label[i].type == PACK_TYPE_CONFIG_55H)
				{
					ESP_LOGW(TAG, "go to  can_broadcast_cmd_pack");
					can_broadcast_cmd_pack(&cmd_label[i], payload);
				}
				else if(cmd_label[i].type == MODULE_TYPE_DC_AC_HUB_SET_40H)
				{
					ESP_LOGW(TAG, "go to  can_broadcast_cmd_dc_ac_hub");
					can_broadcast_cmd_dc_ac_parts(&cmd_label[i], payload);
				}
				else if(cmd_label[i].type == MODULE_TYPE_D400S_SET_49H)
				{
					ESP_LOGW(TAG, "go to  can_broadcast_d400s_hub_set");
					can_broadcast_cmd_d400s_set(&cmd_label[i], payload);
				}

			}
			/* 单播指令读取 */
			else
			{
				ESP_LOGW(TAG, "go to can_unicasst_cmd");
				can_unicasst_cmd(md_addr, &cmd_label[i], payload);
			}

			if ((cmd_label[i].type == INV_TYPE_CONFIG01_1BH)
				&& (cmd_label[i].offset == offsetof(inv_set01_struct, ctrl_reset_factory)))
			{
				app_reset_factory_after_can_tx();
			}

		/* 设置完成后延时一段时间再汇总数据加快APP回读响应 */
		vTaskDelay(pdMS_TO_TICKS(100));
//		TASK_ENTER_CRITICAL();
//		data_sum_inv();
//		data_sum_pack();
//		data_sum_pack_group();
//		//data_sum_ems();
//		TASK_EXIT_CRITICAL();

			/* 有新设置完成下发,检查是否需要激活特联动的指令 */
	        if (cmd_label[i].active_can_cmd_type != 0xFFFF)
			{
	            CanCmdActivte(cmd_label[i].active_can_cmd_type); 	// modbus设置某个操作之后，需要激活联动的指令获取数据
				ESP_LOGW(TAG, "active binding cmd, type: %02x\n", cmd_label[i].active_can_cmd_type);
				trigger_read_type = cmd_label[i].active_can_cmd_type;
				trigger_read_addr = md_addr;
	        }
    	}

		bk_inv_node_set *p_inv_bk = &Inv_can_WR.bk_inv_dev_set;
		if ((p_inv_bk->inv_set00.ctrl.all != 0)&&((cmd_label[i].type==0x1a)&&(cmd_label[i].offset==9))) { // 设置下发后设置区清零,避免下次设置时使用旧的设置导致意外事件
			p_inv_bk->inv_set00.ctrl.all = 0;
			ESP_LOGI(TAG,"CLEAR CTRL");
		}
    }

}




/*
return: min
*/
static uint32_t OTA_INV_Percent_Get_Min(void)
{
	uint32_t buff[DEV_MAIN_NODE_MAX];//INV_MAX_NUM PACK_MAX_NUM
	uint8_t i = 0;
	uint32_t rtn = 0;
	uint8_t online_cnt=0;

	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		if ((Inv_can[i].inv_data[0].online)&&
		((can_ota_status[0].devStatus[INV_CAN_ADDR +i].isOta == OTA_DOING)||(can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct!=0)))
		{
			ESP_LOGI(TAG,"devStatus[%d].pct:%d",INV_CAN_ADDR +i,can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct);
			buff[i] =can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct;
			online_cnt++;
		}
		else//offline，填充0xFF,取不到最小
		{
			buff[i] = 0xFF;
		}
	}

	if(0 == online_cnt)
	{
		rtn=0;
	}
	else
	{
		rtn =data_min(buff, i);
	}
	return rtn;
}


/*
return: min
AC380 系统，下级CAN BMS OTA，ARM内置电池包升级，取ARM状态数量，即下级汇总上报,
不能只取ARM状态数量，来判断，还需要判断ARM下的外置电池包数量
*/
static uint32_t OTA_PACKS_Percent_Get_Min(uint8_t *onilne_seq)
{
	uint32_t buff[DEV_MAIN_NODE_MAX];//INV_MAX_NUM PACK_MAX_NUM
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t n = 0;
	uint32_t rtn = 0;
	uint8_t online_cnt=0;

	uint8_t pack_online=0xff;

	//==============================================================================
	int ret=0;
    int ret2=-1;
	uint32_t softVersion[10]={0};

	for (i = 0 ; i < 6 ; i++ )
	{
		if (Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type==3)//BMS类型
		{
			if((reals.preSoftVersion/100)==(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].version/100))//内置电池包类型
			{
				//ESP_LOGI(TAG,"OTA_BMS_Percent_Get_Min ,reals.preSoftVersion:%lu",reals.preSoftVersion);
				ret=1;		//内置电池包
				goto _final;
			}
		}
	}

	if(!ret)//外置电池包类型
	{
		for(i=0;i<MAX_PACK_TYPE_NUM;i++)
		{
			for(j=1;j<PACK_MAX_NUM;j++)
			{
				if(Inv_can[i].pack_data[j].online == 1)
				{
					for(n=0;n<PACK_SOFT_NUM;n++)
					{
						softVersion[n]=Inv_can[i].pack_data[j].pack_about.soft[n].version;
						//ESP_LOGI(TAG,"i:%d,j:%d,softVersion[%d]:%lu",i,j,n,softVersion[n]);
					}
					ret2=findFirstMatchVersion(softVersion,SLAVE_MIN_VERSION_COUNT,reals.preSoftVersion);
					ESP_LOGI(TAG,"ret2:%d",ret2);
					if(ret2>=0)
					{
						if(pack_online==0xff)
						{
							pack_online=i;
						}
						ret=2;		//外置电池包
						goto _final;
					}
				}
			}
		}
	}

_final:
	ESP_LOGI(TAG,"pack_online:%d",pack_online);
	if(pack_online!=0xff)
		{*onilne_seq=pack_online;}

	ESP_LOGI(TAG,"OTA_PACKS_Percent_Get_Min ret:%d,reals.preSoftVersion:%lu *onilne_seq:%d",ret,reals.preSoftVersion,*onilne_seq);
	//==============================================================================
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		ESP_LOGI(TAG,"i:%d,pack_announce.online:0x%x isOta:%d",i,Inv_can[i].pack_data[0].pack_announce.online,can_ota_status[0].devStatus[INV_CAN_ADDR +i].isOta);
		//pack与dchub的进度获取使用相同的处理逻辑：进度是由底层每回复一个0x75帧计算得出，有回复的pct就应当认为是在线
		if ((((ret==1)&&(Inv_can[i].pack_data[0].pack_announce.online & 0x1))||
			((ret==2)&&(Inv_can[i].pack_data[0].pack_announce.online & 0xFE))) &&
			 ((can_ota_status[0].devStatus[INV_CAN_ADDR +i].isOta == OTA_DOING)||(can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct!=0)))
		{
			ESP_LOGW(TAG,"BMS 000 ---can_ota_status[0].devStatus[INV_CAN_ADDR +i:%d].pct == %d, buff[%d] == %lu",
			 	INV_CAN_ADDR +i,can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct,i,buff[i]);
			buff[i] =can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct;//PACK_CAN_ADDR ac380升级时电池包的升级进度也由arm汇总返回
			online_cnt++;
		}
		else//offline，填充0xFF,取不到最小
		{
			buff[i] = 0xFF;
		}
	}


	if(0 == online_cnt)
	{
		rtn=0;
	}
	else
	{
		rtn =data_min(buff, i);
	}
	return rtn;
}

/*
return: min
AC380 系统，下级CAN PACK OTA，不能只取ARM状态数量，来判断，还需要判断ARM下的外置电池包数量
*/
static uint32_t OTA_PACK_Percent_Get_Min(void)
{
	uint32_t buff[DEV_MAIN_NODE_MAX];//INV_MAX_NUM PACK_MAX_NUM
	uint8_t i = 0;
	uint32_t rtn = 0;
	uint8_t online_cnt=0;

	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		//pack与dchub的进度获取使用相同的处理逻辑：进度是由底层每回复一个0x75帧计算得出，有回复的pct就应当认为是在线
		//if (((Inv_can[i].inv_data[0].online)&&(Inv_can[i].pack_data[0].pack_announce.online & 0xFE)) || (can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct != 0))
		if ((Inv_can[i].inv_data[0].online) || (can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct != 0))
		{
			ESP_LOGW(TAG,"000 ---can_ota_status[0].devStatus[INV_CAN_ADDR +i:%d].pct == %d, buff[%d] == %lu",
			 	INV_CAN_ADDR +i,can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct,i,buff[i]);
			buff[i] =can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct;//PACK_CAN_ADDR ac380升级时电池包的升级进度也由arm汇总返回
			online_cnt++;
		}
		else//offline，填充0xFF,取不到最小
		{
			buff[i] = 0xFF;
		}
	}


	if(0 == online_cnt)
	{
		rtn=0;
	}
	else
	{
		rtn =data_min(buff, i);
	}
	return rtn;
}

/*
return: min
AC380 系统，下级CAN PACK OTA，不能只取ARM状态数量，来判断，还需要判断ARM下的外置电池包数量
*/
static uint32_t OTA_IOT_Percent_Get_Min(void)
{
	uint32_t buff[DEV_MAIN_NODE_MAX];//INV_MAX_NUM PACK_MAX_NUM
	uint8_t i = 0;
	uint32_t rtn = 0;
	uint8_t online_cnt=0;
	if((reals.online_Iot_num==1)&&(reals.Addr_can_master==1))
	{
		ESP_LOGW(TAG,"111 self---can_ota_status[0].devStatus[IOT_CAN_ADDR +i:%d].pct == %d, buff[%d] == %lu",
				IOT_CAN_ADDR +reals.online_X_inv_index,can_ota_status[0].devStatus[IOT_CAN_ADDR +reals.online_X_inv_index].pct,i,buff[reals.online_X_inv_index]);
		buff[reals.online_X_inv_index] =can_ota_status[0].devStatus[IOT_CAN_ADDR +reals.online_X_inv_index].pct;//PACK_CAN_ADDR ac380升级时电池包的升级进度也由arm汇总返回
		online_cnt++;
	}
	else if(reals.online_Iot_num>1)
	{
		for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
		{
			ESP_LOGI(TAG,"OTA_IOT_Percent_Get_Min i:%d ,onilne:%d,isOta:%d, pct:%d",i,Inv_can[i].iot_data[0].online,can_ota_status[0].devStatus[IOT_CAN_ADDR +i].isOta,can_ota_status[0].devStatus[IOT_CAN_ADDR +i].pct);
			//pack与dchub的进度获取使用相同的处理逻辑：进度是由底层每回复一个0x75帧计算得出，有回复的pct就应当认为是在线
			//if (((Inv_can[i].inv_data[0].online)&&(Inv_can[i].pack_data[0].pack_announce.online & 0xFE)) || (can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct != 0))
			if ((reals.online_X_inv_index!=i)&&
			((Inv_can[i].iot_data[0].online) &&
			((can_ota_status[0].devStatus[IOT_CAN_ADDR +i].isOta == OTA_DOING)||(can_ota_status[0].devStatus[IOT_CAN_ADDR +i].pct!=0))))
			{
				ESP_LOGW(TAG,"111 ---can_ota_status[0].devStatus[IOT_CAN_ADDR +i:%d].pct == %d, buff[%d] == %lu",
					IOT_CAN_ADDR +i,can_ota_status[0].devStatus[IOT_CAN_ADDR +i].pct,i,buff[i]);
				buff[i] =can_ota_status[0].devStatus[IOT_CAN_ADDR +i].pct;//PACK_CAN_ADDR ac380升级时电池包的升级进度也由arm汇总返回
				online_cnt++;
			}
			else//offline，填充0xFF,取不到最小
			{
				buff[i] = 0xFF;
			}
		}
	}


	if(0 == online_cnt)
	{
		rtn=0;
	}
	else
	{
		rtn =data_min(buff, i);
	}
	return rtn;
}

/*
return: min
*/
static uint32_t OTA_DCHUB_Percent_Get_Min(void)
{
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t i = 0;
	uint32_t rtn = 0;
	uint8_t online_cnt=0;

	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		// ESP_LOGW(TAG,"000 ---can_ota_status[0].devStatus[INV_CAN_ADDR +i:%d].pct == %d, buff[%d] == %lu, Inv_can[%d].dc_hub_data[0].online == %d",
		// 	INV_CAN_ADDR +i,can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct,i,buff[i],i,Inv_can[i].dc_hub_data[0].online);
		if ((reals.online_DCHUB_bit == 1<<i) || (can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct != 0))
		{
			buff[i] =can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct;
			online_cnt++;
		}
		else//offline，填充0xFF,取不到最小
		{
			buff[i] = 0xFF;
		}
	}


	if(0 == online_cnt)
	{
		rtn=0;
	}
	else
	{
		rtn =data_min(buff, i);
	}
	return rtn;
}


/*
return: min
*/
static uint32_t OTA_D400S_Percent_Get_Min(void)
{
	uint32_t buff[DEV_MAIN_NODE_MAX];//INV_MAX_NUM PACK_MAX_NUM
	uint8_t i = 0;
	uint32_t rtn = 0;
	uint8_t online_cnt=0;

	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		if ((Inv_can[i].d400s_data[0].online)&&
		((can_ota_status[0].devStatus[INV_CAN_ADDR +i].isOta == OTA_DOING)||(can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct!=0)))
		{
			ESP_LOGI(TAG,"devStatus[%d].pct:%d",INV_CAN_ADDR +i,can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct);
			buff[i] =can_ota_status[0].devStatus[INV_CAN_ADDR +i].pct;
			online_cnt++;
		}
		else//offline，填充0xFF,取不到最小
		{
			buff[i] = 0xFF;
		}
	}

	if(0 == online_cnt)
	{
		rtn=0;
	}
	else
	{
		rtn =data_min(buff, i);
	}
	return rtn;
}

/*
group:选择需要升级的组（1：INV-OTA；2：PACK-OTA；3: IOT-OTA; 4-LCD）
grp_id:选择该组下哪个ID需要升级0~0xFF(从0开始，0表示#1；0xFF表示广播)


AC380因是单一CAN总线多并机，电池包在ARM下级，因此可统一使用 ARM数量+ INV/PACK source ID作为各个子设备的进度

INV:INV_CAN_ADDR~INV_CAN_ADDR~+3
PACK:PACK_CAN_ADDR~PACK_CAN_ADDR+3


上下约定，暂时都只报汇总
*/
void can_ota_summary(uint8_t node_id, uint8_t group, uint16_t type)
{
	extern ota_mode_t ota_mode;
//	if (ota_mode == CAN_BROADCAST_ADDRESS) return;	// 不支持广播升级
//	if (ota_mode == OTA_UNICAST) node_id = 0;

//	uint8_t dev_id = grp_id;
	uint8_t can_bus_seq = 0;
	uint8_t i = 0;
	static uint8_t first_online_seq = DEV_MAIN_NODE_MAX;		//记录第一个在线的设备
	static uint8_t first_online_seq_dchub = DEV_MAIN_NODE_MAX;	//记录第一个在线的设备
	static uint8_t first_online_seq_d400s = DEV_MAIN_NODE_MAX;	//记录第一个在线的d400s设备

	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		if (Inv_can[i].inv_data[0].online)
		{
			first_online_seq =i;
			break;
		}
	}
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		if (Inv_can[i].dc_hub_data[0].online)
		{
			first_online_seq_dchub =i;
			break;
		}
	}
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		if (Inv_can[i].d400s_data[0].online)
		{
			first_online_seq_d400s =i;
			break;
		}
	}
	// if((group==GROUP_PACK)&&(type==3))
	// {
	// 	first_online_seq=Get_Pack_Online_Index(reals.preSoftVersion);
	// }
	// if(reals.online_Inv_num)//ac380的升级都需要靠逆变转发，逆变不在线则无法升级
	// {
	// 	first_online_seq = 0;
	// }



	ESP_LOGW(TAG,"first_online_seq ==%d, group ==%d, type == %d",first_online_seq,group,type);
	// esp_log_buffer_hex("first_online_seq", &can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq],
		// sizeof(can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq]));
	// esp_log_buffer_hex("first_online_seq_dchub", &can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_dchub],
	// 	sizeof(can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_dchub]));

	switch (group)
	{
		case 1 :
//			dev_id += INV_CAN_ADDR;
			// dchub信息靠逆变转发，逆变不在线则无法升级
			if(type == 14)
			{
				can_ota_status[can_bus_seq].ota_summary.pct = OTA_DCHUB_Percent_Get_Min();
				if (first_online_seq_dchub < DEV_MAIN_NODE_MAX)
				{//percetn取最小，其他取第一个在线设备
					can_ota_status[can_bus_seq].ota_summary.isOta = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_dchub].isOta;
					can_ota_status[can_bus_seq].ota_summary.errCode = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_dchub].errCode;
				//	can_ota_status[can_bus_seq].ota_summary.pct = can_ota_status[can_bus_seq].devStatus[dev_id].pct;
					can_ota_status[can_bus_seq].ota_summary.where = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_dchub].where;
					can_ota_status[can_bus_seq].ota_summary.type = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_dchub].type;
					can_ota_status[can_bus_seq].ota_summary.level = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_dchub].level;
					ESP_LOGW(TAG,"111-type14 can_ota_status[%d].ota_summary.type:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.type);
					ESP_LOGW(TAG,"111-type14 can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
				}
			}
			else if(type == 15)
			{
				// achub只有一个，不需要取最小
				if (reals.online_ACHUB_num)
				{
					can_ota_status[can_bus_seq].ota_summary.isOta = can_ota_status[can_bus_seq].devStatus[AC_HUB_CAN_ADDR].isOta;
					can_ota_status[can_bus_seq].ota_summary.errCode = can_ota_status[can_bus_seq].devStatus[AC_HUB_CAN_ADDR].errCode;
					can_ota_status[can_bus_seq].ota_summary.pct = can_ota_status[can_bus_seq].devStatus[AC_HUB_CAN_ADDR].pct;
					can_ota_status[can_bus_seq].ota_summary.where = can_ota_status[can_bus_seq].devStatus[AC_HUB_CAN_ADDR].where;
					can_ota_status[can_bus_seq].ota_summary.type = can_ota_status[can_bus_seq].devStatus[AC_HUB_CAN_ADDR].type;
					can_ota_status[can_bus_seq].ota_summary.level = can_ota_status[can_bus_seq].devStatus[AC_HUB_CAN_ADDR].level;
					ESP_LOGW(TAG,"111-type15 can_ota_status[%d].ota_summary.type:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.type);
					ESP_LOGW(TAG,"111-type15 can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
				}
			}
			else
			{
				can_ota_status[can_bus_seq].ota_summary.pct = OTA_INV_Percent_Get_Min();
				if (first_online_seq < DEV_MAIN_NODE_MAX)
				{//percetn取最小，其他取第一个在线设备
					can_ota_status[can_bus_seq].ota_summary.isOta = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].isOta;
					can_ota_status[can_bus_seq].ota_summary.errCode = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].errCode;
				//	can_ota_status[can_bus_seq].ota_summary.pct = can_ota_status[can_bus_seq].devStatus[dev_id].pct;
					can_ota_status[can_bus_seq].ota_summary.where = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].where;
					can_ota_status[can_bus_seq].ota_summary.type = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].type;
					can_ota_status[can_bus_seq].ota_summary.level = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].level;
					ESP_LOGW(TAG,"111-else aaa can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
					ESP_LOGW(TAG,"111-else aaa can_ota_status[%d].ota_summary.type:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.type);
					ESP_LOGW(TAG,"111-else can_ota_status[%d].ota_summary.pct:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.pct );
					ESP_LOGW(TAG,"111-else can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
					ESP_LOGW(TAG,"111-else can_ota_status[%d].ota_summary.errCode:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.errCode);
				}
			}

		break;
		case 2 :
//			dev_id += PACK_CAN_ADDR;
			// pack除升级信息靠逆变地址发出，其余信息按照can协议上的pack地址发
			can_ota_status[can_bus_seq].ota_summary.pct = OTA_PACKS_Percent_Get_Min(&first_online_seq);
			if (first_online_seq < DEV_MAIN_NODE_MAX)
			{//percetn取最小，其他取第一个在线设备 //PACK_CAN_ADDR ac380升级时电池包的升级状态也由arm汇总返回
				can_ota_status[can_bus_seq].ota_summary.isOta = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].isOta;
				can_ota_status[can_bus_seq].ota_summary.errCode = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].errCode;
			//	can_ota_status[can_bus_seq].ota_summary.pct = can_ota_status[can_bus_seq].devStatus[dev_id].pct;
				can_ota_status[can_bus_seq].ota_summary.where = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].where;
				can_ota_status[can_bus_seq].ota_summary.type = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].type;
				can_ota_status[can_bus_seq].ota_summary.level = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].level;
				ESP_LOGW(TAG,"222 can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
				ESP_LOGW(TAG,"222 can_ota_status[%d].ota_summary.type:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.type);
				ESP_LOGW(TAG,"222 can_ota_status[%d].ota_summary.pct:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.pct );
				ESP_LOGW(TAG,"222 can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
				// ESP_LOGE(TAG,"can_ota_status[0].devStatus[INV_CAN_ADDR +first_online_seq:%d].isOta:%d; errCode:%d",
				// 		INV_CAN_ADDR +first_online_seq,can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].isOta,can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq].errCode);
			}
		break;
		case 3 :
		{
			uint8_t first_iot_ota_seq=0;
//			dev_id += IOT_CAN_ADDR;
			can_ota_status[can_bus_seq].ota_summary.pct = OTA_IOT_Percent_Get_Min();
			if((reals.online_Iot_num==1)&&(reals.Addr_can_master==1))
			{
				first_iot_ota_seq=first_online_seq;
			}else if(reals.online_Iot_num>1)
			{
				for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
				{
					if ((reals.online_X_inv_index!=i)&&
					((Inv_can[i].iot_data[0].online) &&
					((can_ota_status[0].devStatus[IOT_CAN_ADDR +i].isOta == OTA_DOING)||(can_ota_status[0].devStatus[IOT_CAN_ADDR +i].pct!=0))))
					{
						first_iot_ota_seq=i;
					}
				}
			}
			ESP_LOGI(TAG,"first_iot_ota_seq:%d",first_iot_ota_seq);
			if (first_iot_ota_seq < DEV_MAIN_NODE_MAX)
			{//percetn取最小，其他取第一个在线设备
				can_ota_status[can_bus_seq].ota_summary.isOta = can_ota_status[can_bus_seq].devStatus[IOT_CAN_ADDR +first_iot_ota_seq].isOta;
				can_ota_status[can_bus_seq].ota_summary.errCode = can_ota_status[can_bus_seq].devStatus[IOT_CAN_ADDR +first_iot_ota_seq].errCode;
			//	can_ota_status[can_bus_seq].ota_summary.pct = can_ota_status[can_bus_seq].devStatus[dev_id].pct;
				can_ota_status[can_bus_seq].ota_summary.where = can_ota_status[can_bus_seq].devStatus[IOT_CAN_ADDR +first_iot_ota_seq].where;
				can_ota_status[can_bus_seq].ota_summary.type = can_ota_status[can_bus_seq].devStatus[IOT_CAN_ADDR +first_iot_ota_seq].type;
				can_ota_status[can_bus_seq].ota_summary.level = can_ota_status[can_bus_seq].devStatus[IOT_CAN_ADDR +first_iot_ota_seq].level;
				ESP_LOGW(TAG,"333 can_ota_status[%d].ota_summary.type:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.type);
				ESP_LOGW(TAG,"333 can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
			}
		}
		break;
		case GROUP_CHARGE:
		{
			can_ota_status[can_bus_seq].ota_summary.pct = OTA_D400S_Percent_Get_Min();
			if (first_online_seq_d400s < DEV_MAIN_NODE_MAX)
			{//percetn取最小，其他取第一个在线设备 //PACK_CAN_ADDR ac380升级时电池包的升级状态也由arm汇总返回
				can_ota_status[can_bus_seq].ota_summary.isOta = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_d400s].isOta;
				can_ota_status[can_bus_seq].ota_summary.errCode = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_d400s].errCode;
			//	can_ota_status[can_bus_seq].ota_summary.pct = can_ota_status[can_bus_seq].devStatus[dev_id].pct;
				can_ota_status[can_bus_seq].ota_summary.where = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_d400s].where;
				can_ota_status[can_bus_seq].ota_summary.type = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_d400s].type;
				can_ota_status[can_bus_seq].ota_summary.level = can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_d400s].level;
				ESP_LOGW(TAG,"222 can_ota_status[%d].ota_summary.type:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.type);
				ESP_LOGW(TAG,"222 can_ota_status[%d].ota_summary.pct:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.pct );
				ESP_LOGW(TAG,"222 can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
				ESP_LOGE(TAG,"can_ota_status[0].devStatus[INV_CAN_ADDR +first_online_seq:%d].isOta:%d; errCode:%d",
						INV_CAN_ADDR +first_online_seq_d400s,can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_d400s].isOta,can_ota_status[can_bus_seq].devStatus[INV_CAN_ADDR +first_online_seq_d400s].errCode);
			}
		}
		break;
		default:
		return ;
	}

	if(DEV_MAIN_NODE_MAX == first_online_seq)//无设备在线，则清零
	{
		ESP_LOGE(TAG,"no device online, clear ota status");
		can_ota_status[can_bus_seq].ota_summary.isOta = 0;
		can_ota_status[can_bus_seq].ota_summary.errCode = DATA_NORMAL;
		can_ota_status[can_bus_seq].ota_summary.pct = 0;
		can_ota_status[can_bus_seq].ota_summary.where = 0;
		can_ota_status[can_bus_seq].ota_summary.type = 0;
		can_ota_status[can_bus_seq].ota_summary.level = 0;
		ESP_LOGW(TAG,"aaa can_ota_status[%d].ota_summary.type:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.type);
		ESP_LOGW(TAG,"aaa can_ota_status[%d].ota_summary.isOta:%d",(unsigned int)can_bus_seq,can_ota_status[can_bus_seq].ota_summary.isOta);
	}


	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[can_bus_seq].isOta = can_ota_status[can_bus_seq].ota_summary.isOta;
	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[can_bus_seq].errCode = can_ota_status[can_bus_seq].ota_summary.errCode;
	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[can_bus_seq].pct = can_ota_status[can_bus_seq].ota_summary.pct;
	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[can_bus_seq].where = can_ota_status[can_bus_seq].ota_summary.where;
	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[can_bus_seq].type = can_ota_status[can_bus_seq].ota_summary.type;
	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[can_bus_seq].level = can_ota_status[can_bus_seq].ota_summary.level;

	// memcpy(&g_self_data.mod_reg00700_OTA.ota_group,&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group, sizeof(MOD_STRUCT_reg00700));
	// 将升级进度等信息同步到g_self_data结构体里
	g_self_data.mod_reg00700_OTA.ota_group[can_bus_seq].isOta = can_ota_status[can_bus_seq].ota_summary.isOta;
	g_self_data.mod_reg00700_OTA.ota_group[can_bus_seq].errCode = can_ota_status[can_bus_seq].ota_summary.errCode;
	g_self_data.mod_reg00700_OTA.ota_group[can_bus_seq].pct = can_ota_status[can_bus_seq].ota_summary.pct;
	g_self_data.mod_reg00700_OTA.ota_group[can_bus_seq].where = can_ota_status[can_bus_seq].ota_summary.where;
	g_self_data.mod_reg00700_OTA.ota_group[can_bus_seq].type = can_ota_status[can_bus_seq].ota_summary.type;
	g_self_data.mod_reg00700_OTA.ota_group[can_bus_seq].level = can_ota_status[can_bus_seq].ota_summary.level;


}
//
//void can_ota_progress(uint8_t node_id, ota_group_status ota_md[])
//{
//	extern ota_mode_t ota_mode;
//	if (ota_mode == CAN_BROADCAST_ADDRESS) return;	// 不支持广播升级
//	if (ota_mode == OTA_UNICAST) node_id = 0;
//
//	ota_md[node_id].isOta = can_ota_status[node_id].ota_summary.isOta;
//	ota_md[node_id].errCode = can_ota_status[node_id].ota_summary.errCode;
//	ota_md[node_id].pct = can_ota_status[node_id].ota_summary.pct;
//	ota_md[node_id].where = can_ota_status[node_id].ota_summary.where;
//	ota_md[node_id].type = can_ota_status[node_id].ota_summary.type;
//	ota_md[node_id].level = can_ota_status[node_id].ota_summary.level;
//}

void ota_can_pct(OtaDevice canOta[], uint8_t id, uint8_t type, uint8_t pct, uint8_t where, uint8_t errcode) {

    /* CAN总线广播出来的OTA进度,IOT监听并收集进度 */
    canOta[id].isOta = OTA_DOING;
    canOta[id].type = type;
    canOta[id].errCode = errcode;
    canOta[id].where = where;
    canOta[id].pct = pct;
    canOta[id].level = DEV_TO_SUB; /* 设备发送给子设备 */

    /* 底层设备进度100或底层出现故障不在升级 */
    if (canOta[ id ].pct >= 100 || canOta[ id ].errCode) {
        canOta[ id ].isOta = OTA_IDLE;
    }
}

void ota_dev_timeout(OtaDevice canOta[])
{
    for (uint8_t i = 0; i < 0xFF; i++)
	{
        if (canOta[i].isOta == OTA_DOING)
		{
            canOta[i].isOta = OTA_IDLE;
            canOta[i].errCode = OTA_ERR_NO_RESP;
        }
    }
}



//////////////////////////////////////////////////////windy add
/*
封装can beta多字节写函数

TX:WRIET_DATA_START
TX:WRITE_DATA

RX:在 can rx独立线程接收，不等待

write step:
0x0860xxXX	写数据块开始帧				上位机->DOWN
0x0861xxXX	数据负载帧				上位机->DOWN
0x0862xxXX	数据应答帧				 DOWN->上位机

*/
static int app_can_send_data(uint8_t *data, uint32_t len, can_recv_cmd_t *can_cmd)
{
	uint16_t crc16;
    uint8_t send_len;
    uint8_t seq = 0;
    uint8_t *p_data = data;
	uint32_t data_len = len;
	DataStruct can_data;

	extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];
	if (can_ota_status[can_cmd->node_id].period_cmd_delay > 0)
		return -1; //CAN-OTA直接返回

	crc16 = calcu_crc16(p_data, data_len);

	/* 发送写开始指令 */
	can_cmd->can_id.bit.funcode = WRIET_DATA_START;
    if (!CanAckStart(can_cmd->node_id, can_cmd->can_id.all, can_cmd->data_type, can_cmd->offset, can_cmd->data_len, crc16))
	{
        return -1;//fail
    }
	/* 循环发送CAN数据 */
    while(data_len)
    {
		/* 填充数据结构,每帧的有效数据载荷长度为FRAME_DATA_BYTES */
        send_len = (data_len >= FRAME_DATA_BYTES) ? (FRAME_DATA_BYTES) : (data_len);
        can_data.type = can_cmd->data_type;
        can_data.seq = seq;
        memcpy(can_data.bytes, p_data, send_len);

		/* 发送单帧CAN数据 */
		can_cmd->can_id.bit.funcode = WRITE_DATA;	// 写数据功能码
		//
        if (!CanAckData(can_cmd->node_id, can_cmd->can_id.all, (uint8_t *)&can_data, (send_len + FRAME_HEAD)))
		{
			return -1;//fail
		}

		data_len -= send_len;
		p_data += send_len;
		seq++;
    }

	return 0;
}

/*
封装can beta多字节读函数

TX:READ_DATA_CMD
RX:在 can rx独立线程接收，不等待


read step:
0x0863xxXX	读取指令				上位机->DOWN
0x0864xxXX	数据开始帧（读取）			DOWN->上位机
0x0865xxXX	数据负载帧（读取）			DOWN->上位机



 * @brief 从下级设备读取数据
 *
 * @param can_cmd can指令结构体指针
 *
return 0:成功,
 -1:失败
 */
static int app_can_read_data(can_send_cmd_t *can_cmd)
{
	if (can_ota_status[can_cmd->node_id].period_cmd_delay > 0)
		return -1; //CAN-OTA直接返回

	if (CanGetTarget(can_cmd->node_id,
					0,
					can_cmd->data_type,
					can_cmd->offset,
					can_cmd->data_len,
					can_cmd->can_id.bit.dst) != 1)
					return -1;

	 return 0;
}

/*
阶段	描述	CAN ID	data[0]/子功能码	data[1]/
step	data[2]	data[3]	data[4]	data[5]	data[6]	data[7]
step1	并机排地址触发帧	0x08FAFFXX	99	1		/	/	/	/	/
step2	本机设备SN上报帧	0x08FAFFXX	99	2	SN_L1(最低字节)	SN_2	SN_3	SN_4	SN_5(最高字节)	/
step3	排地址完成（正常心跳帧）	0x08FAFFXX	99	3	/	/	/	/	/	群组编号(预留)


*/





/*------------------------------------------------------------------------
*@Function :Device_Discovery_Step

设备发现广播帧动作时序:基于超时进入下一阶段

*@return
0- fail
no 0: tx len
*/
void Device_Can_Address_Parallel_Step(void) //100ms cycle
{
    uint16_t i = 0;
    static uint16_t sMasterChgcnt = CAN_MASTER_CHANGE_DELAY>>1;
    static uint16_t scnt3 = 3;
    static uint16_t sStepSendPeriodCnt = 0;//Step CAN发送周期
	static uint16_t sStepWaitSortCnt = 0;//Step 并机重排等待时间

    uint64_t iot_sn_temp =0;
    uint16_t Step_delay = 0;

	can_ctrl_t can_ctrl = {0};

	//重新排序等待间隔
	if(reals.Addr_can_Parallel_Interval)
	{
		reals.Addr_can_Parallel_Interval--;
		if(!reals.Addr_can_Parallel_Interval)
		{
			ESP_LOGI(TAG,"Can Addr parallel Sort Limit Remove");
		}
	}


	if(++sStepSendPeriodCnt >= STEP_CAN_SEND_PERIOD)
	{
		sStepSendPeriodCnt = 0;
		if(STEP_CAN_GROUP_PARALLEL_TRIGER == reals.Step_can_dev_parallel)
		{
			can_ctrl.data[0]=CAN_CTRL_CMD_99;
			can_ctrl.data[1]=reals.Step_can_dev_parallel;
			can_ctrl.data[2]=0;
			can_ctrl.data[3]=0;
			can_ctrl.data[4]=0;
			can_ctrl.data[5]=0;
			can_ctrl.data[6]=0;
			can_ctrl.data[7]=0;

			can_overall_ctrl(0, &can_ctrl);
		}
		else if(STEP_CAN_GROUP_PARALLEL_SEND_SN == reals.Step_can_dev_parallel)//
		{
			can_ctrl.data[0]=CAN_CTRL_CMD_99;
			can_ctrl.data[1]=reals.Step_can_dev_parallel;

#ifdef	PARALLEL_IOT_CAN_ADDR_FROM_ARM_ENALBE
			memcpy((uint8_t*)&can_ctrl.data[2], (uint8_t*)&SetData.dev_info_t.Sn_bind[0], 5);//源 SN
#else//debug
			iot_sn_temp =iot_factory.iot_sn/1000;//删除低三个十进制数（随机数）
			memcpy((uint8_t*)&can_ctrl.data[2], (uint8_t*)&iot_sn_temp, 5);//源 SN
#endif




	//		can_ctrl.data[2]=0;
	//		can_ctrl.data[3]=0;
	//		can_ctrl.data[4]=0;
	//		can_ctrl.data[5]=0;
	//		can_ctrl.data[6]=0;
			can_ctrl.data[7]=0;

			can_overall_ctrl(0, &can_ctrl);
		}
		else if(STEP_CAN_PARALLEL_FINISH == reals.Step_can_dev_parallel)//
		{
			can_ctrl.data[0]=CAN_CTRL_CMD_99;
			can_ctrl.data[1]=reals.Step_can_dev_parallel;
			can_ctrl.data[2]=0;
			can_ctrl.data[3]=0;
			can_ctrl.data[4]=0;
			can_ctrl.data[5]=0;
			can_ctrl.data[6]=0;
			can_ctrl.data[7]=0;

			can_overall_ctrl(0, &can_ctrl);
		}

	}





/////////////
//	if((reals.Step_can_dev_parallel < STEP_CAN_PARALLEL_FINISH)
//		&&(reals.Step_can_dev_parallel > STEP_CAN_PARALLEL_INIT))
//	{
//		Step_delay =STEP_CAN_PARALLEL_DELAY1;//slow
//
//	}
//	else
//	{
//		Step_delay =STEP_CAN_PARALLEL_DELAY2;//fast
//	}

	Step_delay =STEP_CAN_PARALLEL_DELAY1;//slow

	if(++reals.Step_can_Cnt >= Step_delay)//5s step
	{
		reals.Step_can_Cnt =0;
		if(STEP_CAN_PARALLEL_INIT == reals.Step_can_dev_parallel)
		{
			//reals.Step_can_dev_parallel = STEP_CAN_GROUP_PARALLEL_TRIGER;
			ESP_LOGI(TAG,"STEP_CAN_GROUP_PARALLEL_TRIGER	windy BBB");

		}
		else if(STEP_CAN_GROUP_PARALLEL_TRIGER == reals.Step_can_dev_parallel)
		{
			reals.Step_can_dev_parallel = STEP_CAN_GROUP_PARALLEL_SEND_SN;
		}
		else if(STEP_CAN_GROUP_PARALLEL_SEND_SN == reals.Step_can_dev_parallel)
		{
			reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_FINISH;
		}
		else if(STEP_CAN_PARALLEL_FINISH == reals.Step_can_dev_parallel)
		{
			reals.Addr_can_Attr&=~Can_Addr_Parallel_SameAlarm;
			reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_AFTER;
		}
		// ESP_LOGW(TAG,"Device_Can_Address_Parallel_Step reals.Step_can_dev_parallel = %d, reals.Step_can_Cnt = %d",
			// reals.Step_can_dev_parallel,reals.Step_can_Cnt);

	}

/////////////

	if((STEP_CAN_GROUP_PARALLEL_TRIGER == reals.Step_can_dev_parallel)//clean
	||(reals.Addr_can_Attr&Can_Addr_Parallel_SnSame))
	{
		ESP_LOGE(TAG,"000--- master == 0");
		//reals.MonSnSame = 0;
		reals.Addr_can_Attr&=~Can_Addr_Parallel_SnSame;
		reals.Addr_can_master = 0;
		reals.Addr_can_Buff =0;
		reals.Addr_can_Small =IOT_MAX_NUM*DEV_MAIN_NODE_MAX;
		reals.Addr_can_self=0;
		for (i = 0; i < (IOT_MAX_NUM*DEV_MAIN_NODE_MAX+1); i++)
		{
			reals.INV_SN_can[i] =0;
		}
//		ESP_LOGI(TAG,"STEP_CAN_GROUP_PARALLEL_TRIGER  windy 1");

	}
	if((reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_FINISH) || (reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_AFTER))//step2后要立刻更新自己CAN地址
	{
		//refresh the new address
		if (reals.Addr_can_self != reals.Addr_can_Buff)//windy 模块自身的地址=收到的所有地址数量+1
		{
			reals.Addr_can_self = reals.Addr_can_Buff;
		}
	}

	if(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)//主设备的判定，要在step2延迟3秒，作为防止误判缓存时间
	{
		if(++sMasterChgcnt >= CAN_MASTER_CHANGE_DELAY/100)//每隔5秒更新主机，关联IOT的周期上报帧周期
		{
			sMasterChgcnt =0;

			uint8_t ats_flag=0;
			for(uint8_t i=0;i<ATS_GROUP_MAX_NUM;i++)
			{
				if(Inv_can_mix.ATS_data[i].online ==1)
				{
					ats_flag=1;
					break;
				}
			}
			if ((reals.Addr_can_self < reals.Addr_can_Small)&& !ats_flag )//windy仅主模块发,master
			{
				reals.Addr_can_master = 1;
			}
			else
			{
				ESP_LOGE(TAG,"111--- master == 0  ats_flag:%d",ats_flag);
				reals.Addr_can_master = 0;
			}
			reals.Addr_can_Small =IOT_MAX_NUM*DEV_MAIN_NODE_MAX;  //10秒自动更新一次，更新周期和心跳周期必须相同

			if((1 == reals.online_Inv_num)
				&&(1 == reals.online_Iot_num)
				&&(reals.Addr_can_self !=reals.online_X_inv_index)
				//&&(0 == Inv_can[reals.Addr_can_self].inv_data[0].online)//单机情况下，如果 IOT 和INV的 CAN顺序不匹配，要触发地址重排
				)
			{

			//reals.Step_can_dev_parallel = STEP_CAN_GROUP_PARALLEL_TRIGER;
			reals.Addr_can_Attr|=(Can_Addr_Parallel_SameAlarm|Can_Addr_Parallel_SendBindChk);
			ESP_LOGI(TAG,"STEP_CAN_GROUP_PARALLEL_TRIGER %d %d	windy AAA",reals.Addr_can_self,reals.online_X_inv_index);


			}

		}

	 }


//	if(//(reals.MCUPoweronCnt > 20)&&
//		(reals.MCUPoweronCnt < 50))
//	{
//		if(++scnt3 >= 30)// 3s
//		{
//			scnt3 =0;
//		}
//	}
//	else
//	{
//		if(++scnt3 >= 30)// 30s
//		{
//			scnt3 =0;
//		}
//	}
	if((reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)&&!reals.ota_happen)//排地址完成后再发心跳
	{
		Iot_Heartbeat_frame_Send();
	}else
	{
		ESP_LOGI(TAG,"Not heart frame send Step_can_dev_parallel:%d  ota_happen:%d",reals.Step_can_dev_parallel ,reals.ota_happen);
	}



}

/*
MonAddressCal
can并机地址排序，获取最小地址，
只存储比自己SN小的其他设备SN

*/
void  Can_Addr_Sequence(void)//地址自动识别
{
	uint64_t u32IDSelf=0;//UINT32,64bit
	uint64_t u32IDReceive=0;
	uint8_t i=0;
	uint8_t u16IdExistFlag=0;//0-之前不存在/未获得，1-已经获得/已存在

	u16IdExistFlag = 0;


//ESP_LOGI(TAG,"Can_Addr_Sequence  : 1" );


	//if receive other module`s id
	if(reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX])
	{
#ifdef	PARALLEL_IOT_CAN_ADDR_FROM_ARM_ENALBE
		memcpy((uint8_t*)&u32IDSelf, (uint8_t*)&SetData.dev_info_t.Sn_bind[0], 5);//源 SN
#else//debug
		u32IDSelf = iot_factory.iot_sn/1000;//删除低三个十进制数（随机数）
#endif
		ESP_LOGE(TAG,"!!!!!!!!!!!!!Can_Addr_Sequence  :2 u32IDSelf=%llu " ,u32IDSelf);
		//the receive id is same to module,address overlap,set IDERR warning
		if(reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] == u32IDSelf)
		{
			ESP_LOGI(TAG,"Can_Addr_Sequence  :3 " );
			//reals.MonSnSame=1;
			reals.Addr_can_Attr|=Can_Addr_Parallel_SnSame;
		}

		/*
		  windy 只接收比自己SN小的编号，忽略较大编号，从而最小编号的模块接收的SN数量为0，为主模块！！！！！

		  */
		if((reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] < u32IDSelf) && (reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX]))
		{
ESP_LOGI(TAG,"Can_Addr_Sequence  :4 " );

			for (i = 0; i < IOT_MAX_NUM*DEV_MAIN_NODE_MAX; i++)
			{
				if(reals.INV_SN_can[i])//收到的其他模块的地址
				{
					if(reals.INV_SN_can[i] == reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX])
					{
						u16IdExistFlag = 1;//已存在此报文的模块地址
						ESP_LOGI(TAG,"Can_Addr_Sequence  :5 " );

						break;

					}
				}
				else//empty
				{
					ESP_LOGI(TAG,"Can_Addr_Sequence  : 6" );

					break;

				}

			}

			if(u16IdExistFlag == 0)//之前不存在此报文的模块ID，则存储下来
			{

				reals.Addr_can_Buff = reals.Addr_can_Buff + 1;
				ESP_LOGI(TAG,"Can_Addr_Sequence  : 7,reals.Addr_can_Buff =%d", reals.Addr_can_Buff);

				if(i < IOT_MAX_NUM*DEV_MAIN_NODE_MAX)
				{
					reals.INV_SN_can[i]  = reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX];
				}
			}
		}

		reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] = 0;
//		ESP_LOGI(TAG,"Can_Addr_Sequence  : 8" );

	}
}

/* 接收并解析Can报文
判断逆变器并机地址
*/
void Can_Inv_Addr_Check(node_info_t *node, twai_message_t *frame)
{
    IdStruct canId;
    uint8_t index = 0;
	uint64_t can_Sn=0;
    uint8_t i = 0;

    if (!frame)
		return ;



    canId.all = frame->identifier;//can rx ID
	//uint8_t addr_source =canId.all&0xFF;

    if (0x08FA0000 == (canId.all&0xFFFF0000))//0x08FAFFXX
	//&&(addr_source >= IOT_CAN_ADDR) && (addr_source < (IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX)))//only iot parallel
    {
		if(99 == frame->data[0])
		{
			ESP_LOGI(TAG,"can rx 99 =:%d",frame->data[1] );

		 	if(STEP_CAN_GROUP_PARALLEL_SEND_SN== frame->data[1])
			{
				memcpy((uint8_t*)&can_Sn, (uint8_t*)&frame->data[2], 5);//SN
				ESP_LOGI(TAG,"can_Sn =:%02llx",can_Sn );

//				if(0 != can_Sn)
				{
					reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] =((uint64_t)frame->data[6]<<32)|((uint64_t)frame->data[5]<<24)
						|((uint64_t)frame->data[4]<<16)|((uint64_t)frame->data[3]<<8)|((uint64_t)frame->data[2]);


				}
				ESP_LOGI(TAG,"frame->data[2] =:%02x",frame->data[2] );
				ESP_LOGI(TAG,"frame->data[3] =:%02x",frame->data[3] );
				ESP_LOGI(TAG,"frame->data[4] =:%02x",frame->data[4] );
				ESP_LOGI(TAG,"frame->data[5] =:%02x",frame->data[5] );
				ESP_LOGI(TAG,"frame->data[6] =:%02x",frame->data[6] );

				ESP_LOGI(TAG,"reals.INV_SN_can[INV_CAN_MAX_POINT] =:%02llx",reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] );
				if(0 != reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX])
				{
					Can_Addr_Sequence();
				}

			}
			else if(STEP_CAN_GROUP_PARALLEL_TRIGER== frame->data[1])
			{
				ESP_LOGE(TAG,"222--- master == 0");
				//reals.MonSnSame = 0;
				reals.Addr_can_Attr&=~Can_Addr_Parallel_SnSame;
				reals.Addr_can_master = 0;
				reals.Addr_can_Buff =0;
				reals.Addr_can_Small =IOT_MAX_NUM*DEV_MAIN_NODE_MAX;
				reals.Addr_can_self=0;
				for (i = 0; i < (IOT_MAX_NUM*DEV_MAIN_NODE_MAX+1); i++)
				{
					reals.INV_SN_can[i] =0;
				}
//				ESP_LOGI(TAG,"STEP_CAN_GROUP_PARALLEL_TRIGER	windy 2");

				if(STEP_CAN_GROUP_PARALLEL_TRIGER != reals.Step_can_dev_parallel)
				{
					reals.Step_can_dev_parallel =STEP_CAN_GROUP_PARALLEL_SEND_SN;
					reals.Step_can_Cnt=0;
					ESP_LOGI(TAG,"reals.Step_can_dev_parallel =STEP_CAN_GROUP_PARALLEL_SEND_SN	windy 2");
				}
			}
		}



	}

	//任何报文
	if((STEP_CAN_PARALLEL_AFTER == reals.Step_can_dev_parallel)
		&&(canId.bit.src >= IOT_CAN_ADDR) && (canId.bit.src < (IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX)))
	{
		index = canId.bit.src - IOT_CAN_ADDR;
		if (index < reals.Addr_can_Small)//CAN总线其他IOT CAN地址最小值，在排地址之外的正常时段
		{
			reals.Addr_can_Small = index;
		}

	}

}

#define INV_SOFT_PER_NODE_MAX   6
#define INV_SOFT_SUMMARY_MAX    12

#ifndef CAN_DEBUG_LOG_INV_SOFT_SUMMARY
#define CAN_DEBUG_LOG_INV_SOFT_SUMMARY 0
#endif

#if CAN_DEBUG_LOG_INV_SOFT_SUMMARY
static const char *inv_soft_type_name(uint16_t type)
{
	switch (type)
	{
	case DEVICE_ARM: return "ARM(1)";
	case DEVICE_DSP: return "DSP(2)";
	case DEVICE_BMS: return "BMS(3)";
	case DEVICE_DC_HUB: return "DCHUB(14)";
	case DEVICE_AC_HUB: return "ACHUB(15)";
	default: return "other";
	}
}

static void inv_soft_debug_log_inv_can_source(void)
{
	ESP_LOGI(TAG, "[soft_sum] Inv_can source online_Inv_num=%u X=%d Y=%d Addr_self=%d",
	         (unsigned)reals.online_Inv_num, reals.online_X_inv_index, reals.online_Y_inv_index,
	         reals.Addr_can_self);

	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		inv_about_struct *about = &Inv_can[node].inv_data[0].inv_about;

		ESP_LOGI(TAG, "[soft_sum] Inv_can[%d] online=%d dev_type=%.12s sw_total=%u",
		         node, Inv_can[node].inv_data[0].online, about->dev_type, about->software_total);
		for (int j = 0; j < INV_SOFT_PER_NODE_MAX; j++)
		{
			ESP_LOGI(TAG, "  soft[%d] type=%u(%s) ver=%lu part=%lu sub=%lu",
			         j, about->soft[j].type, inv_soft_type_name(about->soft[j].type),
			         (unsigned long)about->soft[j].version,
			         (unsigned long)(about->soft[j].version / 100U),
			         (unsigned long)(about->soft[j].version % 100U));
		}
	}
}

static void inv_soft_debug_log_list(const char *stage, const soft_version1 *list, uint8_t count)
{
	ESP_LOGI(TAG, "[soft_sum] %s count=%u", stage, (unsigned)count);
	for (uint8_t i = 0; i < count; i++)
	{
		ESP_LOGI(TAG, "  list[%u] type=%u(%s) ver=%lu",
		         (unsigned)i, list[i].type, inv_soft_type_name(list[i].type),
		         (unsigned long)list[i].version);
	}
}

static void inv_soft_debug_log_dst(const MOD_STRUCT_reg01100 *dst)
{
	ESP_LOGI(TAG, "[soft_sum] dst Inv[%d] software_total=%u InvType=%.12s InvSN=%llu",
	         (INV_MAX_NUM * DEV_MAIN_NODE_MAX), dst->software_total, dst->InvType,
	         (unsigned long long)dst->InvSN);
	for (uint8_t i = 0; i < dst->software_total && i < INV_SOFT_SUMMARY_MAX; i++)
	{
		const soft_version1 *sv = (i < INV_SOFT_PER_NODE_MAX) ? &dst->soft[i] : &dst->soft_b[i - INV_SOFT_PER_NODE_MAX];

		ESP_LOGI(TAG, "  dst.soft[%u] type=%u(%s) ver=%lu",
		         (unsigned)i, sv->type, inv_soft_type_name(sv->type), (unsigned long)sv->version);
	}
}
#endif /* CAN_DEBUG_LOG_INV_SOFT_SUMMARY */

static void achub_grid_transfer_flag_update(void)
{
	if (Inv_can_mix.ac_hub_data[0].online == 1)
	{
		uint32_t ver = Inv_can_mix.ac_hub_data[0].ac_hub_info.SoftwareVersion;
		uint8_t ver_num = 0;

		if ((ver / 100) == AC_HUB_HIGHVOLT_VERSION)
		{
			ver_num = ver % 100;
			if (ver_num >= 4) // 高压版本4及以上支持电网能量传输
			{
				reals.achub_grid_transfer_flag = 1;
			}
		}
		else if ((ver / 100) == AC_HUB_LOWVOLT_VERSION)
		{
			ver_num = ver % 100;
			if (ver_num >= 9)
			{
				reals.achub_grid_transfer_flag = 1;
			}
		}
	}
}

static int find_part_no_index(const soft_version1 *list, uint8_t count, uint32_t version)
{
	uint32_t part = version / 100;

	for (uint8_t i = 0; i < count; i++)
	{
		if ((list[i].version / 100) == part)
		{
			return (int)i;
		}
	}

	return -1;
}

static uint32_t merge_version_by_part(uint32_t ver_a, uint32_t ver_b)
{
	uint32_t part = ver_a / 100;
	uint32_t sub_a = ver_a % 100;
	uint32_t sub_b = ver_b % 100;
	uint32_t sub = (sub_a < sub_b) ? sub_a : sub_b;

	return part * 100 + sub;
}

static void soft_list_merge_entry(soft_version1 *list, uint8_t *count, uint16_t type, uint32_t version)
{
	int idx;

	if (version == 0)
	{
		return;
	}

	idx = find_part_no_index(list, *count, version);
	if (idx >= 0)
	{
		list[idx].version = merge_version_by_part(list[idx].version, version);
	}
	else if (*count < INV_SOFT_SUMMARY_MAX)
	{
		list[*count].type = type;
		list[*count].version = version;
		(*count)++;
	}
}

/* 扫描所有 online==1 的 node，soft[0..5] 按料号 ver/100 合并 */
static void inv_soft_collect_from_online_nodes(soft_version1 *list, uint8_t *count)
{
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		inv_about_struct *about;

		if (Inv_can[node].inv_data[0].online != 1)
		{
			continue;
		}

		about = &Inv_can[node].inv_data[0].inv_about;
		for (int j = 0; j < INV_SOFT_PER_NODE_MAX; j++)
		{
			uint16_t type = about->soft[j].type;
			uint32_t version = about->soft[j].version;

			if (type == DEVICE_ARM || type == DEVICE_DSP || type == DEVICE_BMS
			    || type == DEVICE_DC_HUB)
			{
				soft_list_merge_entry(list, count, type, version);
			}
		}
	}
}

static void write_soft_to_summary(MOD_STRUCT_reg01100 *dst, const soft_version1 *list, uint8_t count)
{
	memset(dst->soft, 0, sizeof(dst->soft));
	memset(dst->soft_b, 0, sizeof(dst->soft_b));

	if (count > INV_SOFT_SUMMARY_MAX)
	{
		count = INV_SOFT_SUMMARY_MAX;
	}

	dst->software_total = count;

	for (uint8_t i = 0; i < count; i++)
	{
		if (i < INV_SOFT_PER_NODE_MAX)
		{
			dst->soft[i] = list[i];
		}
		else
		{
			dst->soft_b[i - INV_SOFT_PER_NODE_MAX] = list[i];
		}
	}
}

static void soft_entry_move_to_front(soft_version1 *list, uint8_t from_idx)
{
	soft_version1 entry;

	if (from_idx == 0)
	{
		return;
	}

	entry = list[from_idx];
	memmove(&list[1], &list[0], from_idx * sizeof(soft_version1));
	list[0] = entry;
}

static int find_type_index(const soft_version1 *list, uint8_t count, uint16_t type)
{
	for (uint8_t i = 0; i < count; i++)
	{
		if (list[i].type == type)
		{
			return (int)i;
		}
	}

	return -1;
}

static void achub_soft_entry_inject(soft_version1 *list, uint8_t *count)
{
	uint32_t achub_ver = 0;
	int idx;

	if (Inv_can_mix.ac_hub_data[0].online != 1)
	{
		return;
	}

	if (reals.If_AC_HUB_SingleBoot == 0)
	{
		achub_ver = Inv_can_mix.ac_hub_data[0].ac_hub_info.SoftwareVersion;
		if (achub_ver == 0)
		{
			return;
		}
	}
	else
	{
		achub_ver = reals.AC_HUB_SingleBoot_version;
		if (achub_ver == 0)
		{
			return;
		}
	}

	idx = find_type_index(list, *count, DEVICE_AC_HUB);
	if (idx < 0)
	{
		idx = find_part_no_index(list, *count, achub_ver);
	}

	if (idx >= 0)
	{
		list[idx].type = DEVICE_AC_HUB;
		list[idx].version = merge_version_by_part(list[idx].version, achub_ver);
		soft_entry_move_to_front(list, (uint8_t)idx);
	}
	else if (*count < INV_SOFT_SUMMARY_MAX)
	{
		memmove(&list[1], &list[0], (*count) * sizeof(soft_version1));
		list[0].type = DEVICE_AC_HUB;
		list[0].version = achub_ver;
		(*count)++;
	}
}

static void soft_entries_move_type_to_back(soft_version1 *list, uint8_t count, uint16_t type)
{
	soft_version1 ordered[INV_SOFT_SUMMARY_MAX];
	uint8_t n = 0;
	uint8_t i;

	if (count == 0 || count > INV_SOFT_SUMMARY_MAX)
	{
		return;
	}

	for (i = 0; i < count; i++)
	{
		if (list[i].type != type)
		{
			ordered[n++] = list[i];
		}
	}
	for (i = 0; i < count; i++)
	{
		if (list[i].type == type)
		{
			ordered[n++] = list[i];
		}
	}

	memcpy(list, ordered, count * sizeof(soft_version1));
}

void judge_inv_min_version(void)
{
#if DEBUG_1100
	MOD_STRUCT_reg01100 *dst = &Inv[(INV_MAX_NUM * DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base;
	soft_version1 list[INV_SOFT_SUMMARY_MAX];
	uint8_t count = 0;

#if CAN_DEBUG_LOG_INV_SOFT_SUMMARY
	inv_soft_debug_log_inv_can_source();
#endif

	achub_grid_transfer_flag_update();

	inv_soft_collect_from_online_nodes(list, &count);
#if CAN_DEBUG_LOG_INV_SOFT_SUMMARY
	inv_soft_debug_log_list("after online nodes merge", list, count);
#endif

	achub_soft_entry_inject(list, &count);
	soft_entries_move_type_to_back(list, count, DEVICE_BMS);
#if CAN_DEBUG_LOG_INV_SOFT_SUMMARY
	inv_soft_debug_log_list("final", list, count);
#endif
	write_soft_to_summary(dst, list, count);
#if CAN_DEBUG_LOG_INV_SOFT_SUMMARY
	inv_soft_debug_log_dst(dst);
#endif
#endif


// #if !DEBUG_1100	// 未完成
// 	// 比较版本号(如果存在多个不同的ARM、DSP、BMS版本号的前四位，一共就会有9个版本号)

// 	uint8_t count = 0;//类型号数量 0
// 	int8_t ret1 = 0;//版本号索引，第一个版本号前四位相同的位置
// 	int8_t ret2 = 0;//版本号索引，第一个为0的位置
// 	Data_INV2 versions[9] = {0};//存储各个类型号，及每个类型号下每个唯一料号的最低版本号

// 	for(int i = 0;i < DEV_MAIN_NODE_MAX;i++)
// 	{
// 		uint8_t type;
// 		uint32_t newVersion;

// 		if(Inv_can[i].inv_data[0].online == 1)// 有设备在线
// 		{
// 			for(int j = 0;j < 6;j++)// 最多6个版本号
// 			{
// 				if(Inv_can[i].inv_data[0].inv_about.soft[j].type == 1)// ARM
// 				{
// 					type = 1;
// 					newVersion = Inv_can[i].inv_data[0].inv_about.soft[j].version;
// 				}
// 				else if(Inv_can[i].inv_data[0].inv_about.soft[j].type == 2)// DSP
// 				{
// 					type = 2;
// 					newVersion = Inv_can[i].inv_data[0].inv_about.soft[j].version;
// 				}
// 				else if(Inv_can[i].inv_data[0].inv_about.soft[j].type == 3)// BMS
// 				{
// 					type = 3;
// 					newVersion = Inv_can[i].inv_data[0].inv_about.soft[j].version;
// 				}

// 				// 检查是否已存在此类型号
// 				int found = 0;
// 				for (int k = 0; k < count; k++)
// 				{
// 					if (versions[k].type == type)
// 					{
// 						ret1 = findFirstMatchVersion(versions[k].version,10,newVersion);//比较的是版本号的前4位是否一样
// 						if(ret1 == -1)
// 						{
// 							ret2 = findFirstMatchVersion(versions[k].version,10,0);
// 							versions[k].version[ret2] = newVersion;
// 							versions[k].count_version++;
// 						}
// 						else
// 						{
// 							if(versions[k].version[ret1] > newVersion)
// 							{
// 								versions[k].version[ret1] = newVersion;
// 							}
// 						}
// 						found = 1;
// 						break;
// 					}
// 				}

// 				// 如果未找到则添加新条目
// 				if (!found)
// 				{
// 					if (count < 9)
// 					{
// 						versions[count].type = type;
// 						versions[count].version[0] = newVersion;
// 						versions[count].count_version++;
// 						count++;
// 					}
// 				}
// 			}
// 		}

// 		//分别传递每个类型(ARM...)下固件版本号
// 		for(int i = 0;i < count;i++)
// 		{
// 			Inv[i].mod_reg01100_Inv_base.software_total = versions[i].count_version;//每个类型号下面版本号数量
// 			// ESP_LOGI(TAG,"count == %d, versions[%d].count_version == %d",count,i,versions[i].count_version);
// 			for(int j = 0;j < 10;j++)
// 			{
// 				if(versions[i].version[j] != 0)
// 				{
// 					Inv[i].mod_reg01100_Inv_base.soft[j].type = 3;//传递对应固件类型 i
// 					Inv[i].mod_reg01100_Inv_base.soft[j].version = versions[i].version[j];//传递每个类型下对应固件版本号 i
// 					// ESP_LOGI(TAG,"000 --- i == %d, j== %d, type == %s, version == %ld",i,j,versions[i].type_ascii,versions[i].version[j]);
// 				}
// 			}
// 			memcpy(Inv[i].mod_reg01100_Inv_base.type_ascii,versions[i].type_ascii,sizeof(versions[i].type_ascii));//传递pack类型
// 		}
// 	}


// #endif

}





/*------------------------------------------------------------------------------
 Function: CAN_DEV_INV_SN_Get_to_Save
 存储 CAN SN到 setadata
 -----------------------------------------------------------------------------*/
/**
  * @brief		设备SN 收集
  devId:源地址
  * @param[out] None
  * @return 	uint8_t  Top


  */
void  CAN_DEV_INV_ACHUB_SN_Get_to_Save(void)//1s cycle
{
	uint8_t i;
	uint64_t u64ac_hub_sn=0;
	uint64_t u64ac_hub_safetycode=0;
	static uint8_t delaycnt1=0;
	static uint8_t delaycnt2=0;
	uint8_t change_flag =0;

	if(++delaycnt1 >= 1)
	{
		delaycnt1 =0;
		if((reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)// get INV  SN
				&&(reals.online_Inv_num > 0)	//
			&&(0 != Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_sn)
			&&(0 != memcmp(Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_type, reals.zero, sizeof(Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_type))) //非零
			&&((SetData.dev_info_t.INV_dev_sn != Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_sn)
			||(0 != memcmp(SetData.dev_info_t.INV_dev_type,  Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_type,  sizeof(Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_type)))) //变化

			)
		{
				SetData.dev_info_t.INV_dev_sn = Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_sn;
				memcpy(SetData.dev_info_t.INV_dev_type,  Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_type,  sizeof(Inv_can[reals.Addr_can_self].inv_data[0].inv_about.dev_type));
				reals.SetDataWrFlag.sBit.SetDataUpdate_all =1;
				change_flag =1;//广播逆变器SN码蓝牙名

			ESP_LOGE(TAG, " CAN_DEV_INV_ACHUB_SN_Get_to_Save  AAAA");
			ESP_LOGE(TAG, " huangji AAAA INV_dev_type:%s-%llu",SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.INV_dev_sn);
		}

//			u64ac_hub_sn = Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn[0]
//							|((uint64_t)Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn[1]<<16)
//							|((uint64_t)Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn[2]<<32)
//							|((uint64_t)Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn[3]<<48);
//
			u64ac_hub_sn = Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn;
			u64ac_hub_safetycode = Inv_can_mix.ac_hub_data[0].ac_hub_info.ACHUB_safe_code;

			ESP_LOGI(TAG,"u64ac_hub_sn:%llu ,u64ac_hub_safetycode:%llu",u64ac_hub_sn,u64ac_hub_safetycode);

			if((reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)//get ACHUB  SN
			&&(reals.online_ACHUB_num > 0)	//
			&&(0 != u64ac_hub_sn)
			&&(0 != strlen(Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type)) //非零
			&&((SetData.dev_info_t.Parallel_dev_sn != u64ac_hub_sn)
			||(SetData.dev_info_t.Parallel_dev_safetycode != u64ac_hub_safetycode)
			||(0 != memcmp((uint8_t *)&SetData.dev_info_t.Parallel_dev_type[0] , (uint8_t *)&Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type[0],    sizeof(SetData.dev_info_t.Parallel_dev_type)))) //变化

			)
		{
				SetData.dev_info_t.Parallel_dev_sn = u64ac_hub_sn;
				memcpy((uint8_t *)&SetData.dev_info_t.Parallel_dev_type[0] , (uint8_t *)&Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type[0],  sizeof(SetData.dev_info_t.Parallel_dev_type));
				SetData.dev_info_t.Parallel_dev_safetycode = Inv_can_mix.ac_hub_data[0].ac_hub_info.ACHUB_safe_code;
				reals.SetDataWrFlag.sBit.SetDataUpdate_all =1;
				change_flag =2;//进入并机，广播ACHUB SN码蓝牙名
				ESP_LOGE(TAG, " CAN_DEV_INV_ACHUB_SN_Get_to_Save  BBB");
				ESP_LOGE(TAG, " huangji BBBB INV_dev_type:%s-%llu",SetData.dev_info_t.INV_dev_type,SetData.dev_info_t.INV_dev_sn);

		}

	}


	if(++delaycnt2 >= 1)//2s  ACHUB  clean
	{
		delaycnt2 =0;
		//ESP_LOGI(TAG, " Parallel_dev_type:%s  CCC",&SetData.dev_info_t.Parallel_dev_type[0]);
		//ESP_LOGI(TAG, " ac_hub_type:%s  CCC",&Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type[0]);
		//ESP_LOGI(TAG, " Step_can_dev_parallel:%d,online_ACHUB_num:%d, ac_hub_type:%d, Parallel_dev_sn:%llu,CCC",(unsigned int)reals.Step_can_dev_parallel,(unsigned int)reals.online_ACHUB_num,(unsigned int)strlen(Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type),SetData.dev_info_t.Parallel_dev_sn);

		if((reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)//
		&&(0 == reals.online_ACHUB_num)//clean 慢速
		&&(0 == strlen(Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type)) //非零
		&&((SetData.dev_info_t.Parallel_dev_sn != 0)
		||(0 != memcmp((uint8_t *)&SetData.dev_info_t.Parallel_dev_type[0] , (uint8_t *)&Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type[0],    sizeof(SetData.dev_info_t.Parallel_dev_type)))) //变化

		)
		{
			SetData.dev_info_t.Parallel_dev_sn = 0;
			SetData.dev_info_t.Parallel_dev_safetycode = 0;
			memset((uint8_t *)&SetData.dev_info_t.Parallel_dev_type[0] , 0,  sizeof(SetData.dev_info_t.Parallel_dev_type));
			reals.SetDataWrFlag.sBit.SetDataUpdate_all =1;
			change_flag =1;//退出并机，重新广播逆变器SN码蓝牙名
			ESP_LOGE(TAG, " CAN_DEV_INV_ACHUB_SN_Get_to_Save  CCC");
			ESP_LOGE(TAG, " CAN_DEV_INV_ACHUB_SN_Get_to_Save (uint8_t *)&SetData.dev_info_t.Parallel_dev_type[0] =%s", (uint8_t *)&SetData.dev_info_t.Parallel_dev_type[0]);
			ESP_LOGE(TAG, " CAN_DEV_INV_ACHUB_SN_Get_to_Save (uint8_t *)&Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type[0] =%s", (uint8_t *)&Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_type[0]);

		}
	}

	if( change_flag)
	{
		ESP_LOGI(TAG,"change_flag:%d",change_flag);
		iot_ble_rename();
		iot_mqtt_ChageFlagSet(change_flag);
	}


//	return ret;
}

#define  SECOND_10S	10
/*------------------------------------------------------------------------------
 Function: CAN_DEV_SN_Get
 -----------------------------------------------------------------------------*/
/**
  * @brief		设备SN 收集
  devId:源地址
  * @param[out] None
  * @return 	uint8_t  Top


 inv_about.dev_sn在 CanNodeOfflineCheck()超时清零


  IOT CAN 参数同步：
  每个IOT存储总线上的所有IOT SN
  SN新增，触发同步，所有IOT参数有效的设备基于延时 Tx+ n_can_seq*T0给其他设备广播，接收方 参数有效标志置1；其他有效参数IOT在收到广播参数同步后，清除自身的待发送标志（即一次SN变化，实际只有一个有效IOT发送参数广播）
  */
void  CAN_DEV_SN_Get(uint8_t devId)
{
	uint8_t i;
//	uint8_t j;
//	uint8_t temp;
//	uint8_t ret = 0;



    uint8_t index = 0xFF;
    if ((devId >= INV_CAN_ADDR) && (devId < (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))) )
	{
        index = (devId - INV_CAN_ADDR)%INV_GROUP_MAX_NUM; // 逆变 第几个逆变不靠index区分，靠前面的level1_addr(包含在node->invs_info里)区分


		ESP_LOGE(TAG,"reals.iot_sn_sum[0]==%llu",reals.iot_sn_sum[0]);
		ESP_LOGE(TAG,"reals.iot_sn_sum[1]==%llu",reals.iot_sn_sum[1]);
		ESP_LOGE(TAG,"reals.iot_sn_sum[2]==%llu",reals.iot_sn_sum[2]);
		for (i = 0; i < (DEV_MAIN_NODE_MAX); i++)// IOT_MAX_NUM
		{

			// dump_buf_global("SetData_Can.dev_info_t2.inv_set00", &Inv_can[index].inv_data[0].inv_about.dev_sn, sizeof(SetData_Can.dev_info_t2.inv_set00));
			if(reals.iot_sn_sum[i] == Inv_can[index].inv_data[0].inv_about.dev_sn)//same
			{
				break;
			}

			if((0 == reals.iot_sn_sum[i])//
			&&(0 != Inv_can[index].inv_data[0].inv_about.dev_sn)
			&&(reals.iot_sn_sum[i] != Inv_can[index].inv_data[0].inv_about.dev_sn)
			)//SN新增
			{
				ESP_LOGE(TAG,"get in new SN!!!!!!!!!!!!!!");//test
				reals.iot_sn_sum[i] = Inv_can[index].inv_data[0].inv_about.dev_sn;
				reals.T_delay_iot_para_can_wr =SECOND_10S + reals.Addr_can_self*2;//基于CAN顺序延时发送
				break;
			}


		 }
	}




//	return ret;
}

/*------------------------------------------------------------------------------
 Function: CAN_IOT_Parameter_WR
 -----------------------------------------------------------------------------*/
/**
  因设备新增，需要广播 iot 参数
  devId:源地址
  * @param[out] None
  * @return 	uint8_t  Top


  */
void CAN_IOT_Parameter_WR(void)//1s cycle
{
	if(reals.T_delay_iot_para_can_wr > 0 )
	{
		reals.T_delay_iot_para_can_wr--;

	}
	else if(0 == reals.T_delay_iot_para_can_wr)
	{
		reals.T_delay_iot_para_can_wr = -1;
		reals.iot_can_set_flag =1;

	}

}


/*------------------------------------------------------------------------------
 Function: CAN_Dev_PowerOff_Check
 -----------------------------------------------------------------------------*/
/**
  当检测到主机BMS关机后，进行相关操作，主动断开mqtt连接

  */
void CAN_Dev_PowerOff_Check(void)
{
	static uint8_t powerOff=1;
	if(Inv_can[reals.online_X_inv_index].pack_data[reals.online_Y_inv_index].pack_announce.work_status==PACK_WORK_POWEROFF )
	{
		ESP_LOGI(TAG,"CLOSE SYSTEM MQTT STOP");
		set_mqtt_stop();
		if(powerOff)
		{
			powerOff=0;
			//记录IOT首次下电关机事件
			SaveInfoOnPowerOff(reals.Addr_can_self);
			reals.SetDataWrFlag.sBit.kwh_mix =1;
		}
	}



}

/*------------------------------------------------------------------------------
 Function: RTC_Update_iot_to_can
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN来源的时间与服务器/APP下发时间存在误差(1s)
  * @param[in]  void
  * @param[out] None
  * @return     void
  */
void RTC_Update_iot_to_can(void)
{
	static uint8_t send1MinCount=0;
	if(1 == reals.st_FlagTime.bits.b1FlagSys1s_can_hi_level)
	{
		send1MinCount++;
        if ( (1 == reals.rtc_flag.sBit.RTC_set_IOT_to_INV)&&(send1MinCount>=60) )//ARM RTC生效需要6秒以上，改为1分钟发送一次
        {
			send1MinCount=0;
            /* 为can指令申请内存 */
            can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};
            uint8_t can_cmd_flag = 0;
            if (can_cmd_queue)
            {
                can_cmd.cmd = heap_caps_malloc(sizeof(can_data_label) * can_cmd.num, MALLOC_CAP_SPIRAM);
                if (!can_cmd.cmd) {
                    ESP_LOGE (TAG, "RTC_Update_iot_to_can malloc failed");
                    return;
                }
            }
            else
            {
                ESP_LOGE (TAG, "can_cmd_queue null!");
                return;
            }

            /*配置发送设置*/
            can_cmd.md_addr = 0;//广播
            can_cmd.num = 1;

            /*配置发送数据格式地址*/
            can_cmd.cmd->type = INV_TYPE_CONFIG00_1AH;
            can_cmd.cmd->active_can_cmd_type = 0; //tbd
            can_cmd.cmd->len = 8;//2001~2004
            can_cmd.cmd->offset = offsetof(inv_set00_struct,time1);

            /*更新时间设置*/
            memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,time1), (uint8_t *)&reals.rtc_time, sizeof(reals.rtc_time));
            memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,res), (uint8_t *)&SetData.dev_info_t.SetTimeZone, sizeof(SetData.dev_info_t.SetTimeZone));
            ESP_LOGW (TAG, "RTC_Update_iot_to_can, data : ");
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, &Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,time1), 8, ESP_LOG_WARN);

            /* 发送到队列 */
            if (can_cmd.cmd != NULL && can_cmd.num != 0)
            {
                if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(100)) == pdTRUE)
                {
					ESP_LOGI(TAG,"RTC_Update_iot_to_can success");
                    can_cmd_flag = 1;
                    reals.BLE_WIFI_to_Can_Cnt++;
                    reals.rtc_flag.sBit.RTC_set_IOT_to_INV = 0;
                }
            }

            /*未发送*/
            if (!can_cmd_flag && can_cmd.cmd)
            {
                free(can_cmd.cmd);
            }
        }
    }
}
