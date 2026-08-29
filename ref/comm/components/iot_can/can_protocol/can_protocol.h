#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>
#include <string.h>

#include "can_data.h"
#include "driver/twai.h"
#include "iot_ota.h"
#include "inv_define.h"
#include "relay_define.h"
#include "pack_define.h"
#include "parameter.h"
#include "modbus_define.h"

#define CAN_CTRL_FRAME_FUNC_CODE		0xFA	// 综合控制帧功能码
#define CAN_CTRL_FRAME_PRIORITY			0x02	// 综合控制帧优先级
#define CAN_CTRL_FRAME_PAGE				0x00	// 综合控制帧数据页


#define CAN_CTRL_REPORT_TIMEOUT			2000	// 主动上报超时时间

/* CAN综合控制帧设置指令定义 */
#define	CAN_CTRL_CMD_EN_PACK_REPORT		1 		// 使能pack上报
#define CAN_CTRL_CMD_DIS_PACK_REPORT	2 		// 关闭pack上报
#define CAN_CTRL_CMD_EN_INV_REPORT		3 		// 使能逆变上报
#define CAN_CTRL_CMD_DIS_INV_REPORT		4 		// 关闭逆变上报
#define CAN_CTRL_CMD_SYNC_TIME_BASE		9 		// 同步底层设备时基
#define CAN_CTRL_CMD_BATTERY_AGING		10 		// 电池老化
#define CAN_CTRL_CMD_EMS_CTRL1			11 		// EMS控制1
#define CAN_CTRL_CMD_EMS_CTRL2			12		// EMS控制2
#define CAN_CTRL_CMD_EMS_CTRL3			13 		// EMS控制3

#define CAN_CTRL_CMD_99			99 		//  

#define CAN_CTRL_CMD_BIND_TRIGER		0x55 		// 绑定帧命令
#define CAN_CTRL_CMD_BIND_CHECK			0xAA 		// 检测帧命令

/* CAN综合控制帧响应值定义 */
#define CAN_RESPOND_SUPPORT_OPEN		1 		// 支持并开启
#define CAN_RESPOND_SUPPORT_CLOSE		2 		// 支持并关闭
#define CAN_RESPOND_SUPPORT_INVALID		3 		// 无效/不动作



#define WRIET_DATA_START        0x60    // CAN 写入指令
#define WRITE_DATA              0x61
#define WIRTE_DATA_ACK          0x62


#define READ_DATA_CMD           0x63    // CAN 读取指令
#define READ_RESP_DATA_START    0x64
#define READ_RESP_DATA          0x65




//step:
#define	STEP_CAN_PARALLEL_INIT		0 //默认 
#define	STEP_CAN_PARALLEL_TRIGER	1 //	1-设备发现触发帧（STEP1）
#define	STEP_CAN_PARALLEL_SEND_SN	2 //	2-设备发现SN信息上报帧（STEP2）
#define	STEP_CAN_PARALLEL_FINISH	3 //	3-设备发现完成帧（STEP3）
#define	STEP_CAN_PARALLEL_AFTER		4 // 

#define	STEP_CAN_PARALLEL_DELAY1		30//step切换延迟，秒
#define	STEP_CAN_PARALLEL_DELAY2		10//

#define	STEP_CAN_SEND_PERIOD		3//300ms 主设备切换延迟
#define	STEP_CAN_PARALLEL_INTERVAL		100//10000ms 重排地址间隔

#define  CHANGE_CAN_TYPE_BY_FINISH_THIS	3//
#define  CHANGE_CAN_TYPE_BY_TIME_NOT_READY	5//

#define GET_CURRENT_TIME_MS()      (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS)


#define CAN_ID_ADDR_B300K_OTA_TYPE   (uint32_t)0x0CF50002//PR023 BMS定期发送0x0CF5xx02这帧 (xx：表示本机地址)，该帧数据的最后一个字节中的Bit5、Bit6表示升级方式


#define CAN_ID_ADDR_CTRL   (uint32_t)0x0CF501FC
#define CAN_ID_INV_PV      (uint32_t)0x0CF501FD
#define CAN_ID_NOW_TIME    (uint32_t)0x0CF501FE
#define CAN_ID_INV_STATE   (uint32_t)0x0CF501FF
#define CAN_ID_AGING_CMD   (uint32_t)0x0CF50137
#define CAN_ID_B230_BOOT   (uint32_t)0x0CF5FF3E
#define CAN_ID_DCDC_PV     (uint32_t)0x1901FF20

#define CAN_BMS_ID         (uint32_t)(0x0CF50000<<3)   // 配置过滤器仅接收0x0CF5开头的帧
#define CAN_ID_MASK        (uint32_t)0x0007FFFE    
#define BAT_TIMEOUT         5000        // 5s
#define LOW_SOC_TIMEOUT     3600000     // 改为一个小时关机 60*60*1000 ms


#define ENERGY_ANNOUNCE_FUNC    0x60

typedef enum {
    /* 全局/汇总 (1..9) */
    ENERGY_LABEL_DCLOAD_TOTAL            = 1,   /* DCLoadTotalEnergy */
    ENERGY_LABEL_ACLOAD_TOTAL            = 2,   /* ACLoadTotalEnergy */
    ENERGY_LABEL_PV_TOTAL_CHARGING       = 3,   /* PVTotalChargingEnergy */
    ENERGY_LABEL_GRID_TOTAL_CHARGING     = 4,   /* GridTotalChargingEnergy */
    ENERGY_LABEL_FEEDBACK_ENERGY         = 5,   /* FeedbackEnergy */
    ENERGY_LABEL_PV_TO_AC_LOAD           = 6,   /* PVToACloadEnergy */
    ENERGY_LABEL_PACK_TOTAL_DSG          = 7,   /* PackTotalDsgEnergy */
    ENERGY_LABEL_TOTAL_ENERGY_CAR        = 8,   /* TotalEnergy_Car */
    ENERGY_LABEL_PACK_TOTAL_CHARGE       = 9,   /* PackTotalChargeEnergy */

    /* INV 段 (20..28) */
    ENERGY_LABEL_INV_PV_TOTAL_CHARGING   = 20,  /* PvTotalChargingEnergy (INV) */
    ENERGY_LABEL_INV_PV_TO_AC_LOAD       = 21,  /* PVToACloadEnergy (INV) */
    ENERGY_LABEL_INV_GRID_TOTAL_CHARGING = 22,  /* GridTotalChargingEnergy (INV) */
    ENERGY_LABEL_INV_GRID_FEEDBACK       = 23,  /* GridFeedbackEnergy (INV) */
    ENERGY_LABEL_INV_DCLOAD_TOTAL        = 24,  /* DCLoadTotalEnergy (INV) */
    ENERGY_LABEL_INV_ACLOAD_TOTAL        = 25,  /* ACLoadTotalEnergy (INV) */
    ENERGY_LABEL_INV_TOTAL               = 26,  /* InvTotalEnergy */
    ENERGY_LABEL_INV_GEN_TOTAL           = 27,  /* GenTotalEnergy */
    ENERGY_LABEL_INV_RESERVED_28         = 28,

    /* DCDC 段 (40..52) */
    ENERGY_LABEL_DCDC_TOTAL_INPUT        = 40,  /* TotalInputEnergy */
    ENERGY_LABEL_DC1_ENERGY_INPUT        = 41,  /* DC1EnergyInput */
    ENERGY_LABEL_DC1_ENERGY_OUTPUT       = 42,  /* DC1EnergyOutput */
    ENERGY_LABEL_DC2_ENERGY_INPUT        = 43,  /* DC2EnergyInput */
    ENERGY_LABEL_DC2_ENERGY_OUTPUT       = 44,  /* DC2EnergyOutput */
    ENERGY_LABEL_DC3_ENERGY_INPUT        = 45,  /* DC3EnergyInput */
    ENERGY_LABEL_DC3_ENERGY_OUTPUT       = 46,  /* DC3EnergyOutput */
    ENERGY_LABEL_DC4_ENERGY_INPUT        = 47,  /* DC4EnergyInput */
    ENERGY_LABEL_DC4_ENERGY_OUTPUT       = 48,  /* DC4EnergyOutput */
    ENERGY_LABEL_DC5_ENERGY_INPUT        = 49,  /* DC5EnergyInput */
    ENERGY_LABEL_DC5_ENERGY_OUTPUT       = 50,  /* DC5EnergyOutput */
    ENERGY_LABEL_DC6_ENERGY_INPUT        = 51,  /* DC6EnergyInput */
    ENERGY_LABEL_DC6_ENERGY_OUTPUT       = 52,  /* DC6EnergyOutput */

    /* Panel 段 (60..71) */
    ENERGY_LABEL_PANEL_DCLOAD_TOTAL      = 60,  /* DCLoadTotalEnergy (Panel) */
    ENERGY_LABEL_PANEL_ACLOAD_TOTAL      = 61,  /* ACLoadTotalEnergy (Panel) */
    ENERGY_LABEL_PANEL_ENERGY_DC1        = 62,  /* EnergyDC1 */
    ENERGY_LABEL_PANEL_ENERGY_DC2        = 63,  /* EnergyDC2 */
    ENERGY_LABEL_PANEL_ENERGY_DC3        = 64,  /* EnergyDC3 */
    ENERGY_LABEL_PANEL_ENERGY_DC4        = 65,  /* EnergyDC4 */
    ENERGY_LABEL_PANEL_ENERGY_DC5        = 66,  /* EnergyDC5 */
    ENERGY_LABEL_PANEL_ENERGY_DC6        = 67,  /* EnergyDC6 */
    ENERGY_LABEL_PANEL_ENERGY_AC1        = 68,  /* EnergyAC1 */
    ENERGY_LABEL_PANEL_ENERGY_AC2        = 69,  /* EnergyAC2 */
    ENERGY_LABEL_PANEL_ENERGY_AC3        = 70,  /* EnergyAC3 */
    ENERGY_LABEL_PANEL_ENERGY_AC4        = 71,  /* EnergyAC4 */

    /* AT1 段 (80..109) - 智能负载/备份等 (选取常见项，按表格顺序命名) */
    ENERGY_LABEL_AT1_GRID_CHARGE_WACTIVE       = 80,
    ENERGY_LABEL_AT1_BACKUP_INV_SUPPORT_WACT   = 81,
    ENERGY_LABEL_AT1_BACKUP_LOAD_SUPPORT_WACT  = 82,
    ENERGY_LABEL_AT1_SMARTLOAD1_L1_SUPPORT_W   = 83,
    ENERGY_LABEL_AT1_SMARTLOAD1_L2_SUPPORT_W   = 84,
    ENERGY_LABEL_AT1_SMARTLOAD1_L3_SUPPORT_W   = 85,
    ENERGY_LABEL_AT1_SMARTLOAD2_L1_SUPPORT_W   = 86,
    ENERGY_LABEL_AT1_SMARTLOAD2_L2_SUPPORT_W   = 87,
    ENERGY_LABEL_AT1_SMARTLOAD2_L3_SUPPORT_W   = 88,
    ENERGY_LABEL_AT1_SMARTLOAD3_L1_SUPPORT_W   = 89,
    ENERGY_LABEL_AT1_SMARTLOAD3_L2_SUPPORT_W   = 90,
    ENERGY_LABEL_AT1_SMARTLOAD3_L3_SUPPORT_W   = 91,
    ENERGY_LABEL_AT1_SMARTLOAD4_L1_SUPPORT_W   = 92,
    ENERGY_LABEL_AT1_SMARTLOAD4_L2_SUPPORT_W   = 93,
    ENERGY_LABEL_AT1_SMARTLOAD4_L3_SUPPORT_W   = 94,
    ENERGY_LABEL_AT1_GRID_FEEDBACK_WACTIVE     = 95,
    ENERGY_LABEL_AT1_BACKUP_INV_CONSUME_WACT   = 96,
    ENERGY_LABEL_AT1_BACKUP_LOAD_CONSUME_WACT  = 97,
    ENERGY_LABEL_AT1_SMARTLOAD1_L1_CONSUME_W   = 98,
    ENERGY_LABEL_AT1_SMARTLOAD1_L2_CONSUME_W   = 99,
    ENERGY_LABEL_AT1_SMARTLOAD1_L3_CONSUME_W   = 100,
    ENERGY_LABEL_AT1_SMARTLOAD2_L1_CONSUME_W   = 101,
    ENERGY_LABEL_AT1_SMARTLOAD2_L2_CONSUME_W   = 102,
    ENERGY_LABEL_AT1_SMARTLOAD2_L3_CONSUME_W   = 103,
    ENERGY_LABEL_AT1_SMARTLOAD3_L1_CONSUME_W   = 104,
    ENERGY_LABEL_AT1_SMARTLOAD3_L2_CONSUME_W   = 105,
    ENERGY_LABEL_AT1_SMARTLOAD3_L3_CONSUME_W   = 106,
    ENERGY_LABEL_AT1_SMARTLOAD4_L1_CONSUME_W   = 107,
    ENERGY_LABEL_AT1_SMARTLOAD4_L2_CONSUME_W   = 108,
    ENERGY_LABEL_AT1_SMARTLOAD4_L3_CONSUME_W   = 109,

    ENERGY_LABEL_MAX
} can_energy_label_t;

typedef enum
{
    COMPLETE = 0,
    WAITTING,
    RECEIVING,
}cmd_status;


typedef enum
{
    HIGHEST = 0,
    LEVEL0 = 0,
    LEVEL1,
    LEVEL2,
    LEVEL3,
    LEVEL4,
    LEVEL5,
    LEVEL6,
    LOWEST = LEVEL6,
    INVLVEL,
}cmd_priority;


	
//并机排序属性
typedef enum
{
	Can_Addr_Parallel_SnSame=0x1,	//CAN sn 重复
	Can_Addr_Parallel_SameAlarm=0x2,	//IOT并机重复报警
	Can_Addr_Parallel_SendBindChk=0x4,	//IOT发送一次0xAA查询指令
	Can_Addr_Parallel_SendBindErr=0x8,	//IOT绑定SN码和本机ARM绑定SN码不一致
}Can_Addr_Parallel_Attr;

#pragma pack(1)

typedef union
{
    uint8_t all;
    struct
    {
    	uint8_t DCDC_work_state : 1;       // DCDC工作状态
    	uint8_t DCDC_module_fault : 1;     // DCDC模块内部故障
    	uint8_t PV_online : 1;       // PV 在线状态
    	uint8_t Res : 1;       // 预留
		uint8_t DCDC_work_model : 4;       // DCDC工作模式    	     
		
    }bit;
}DCDCDevInfo;

typedef union
{
    uint8_t all;
    struct
    {
    	uint8_t Inv_DC_sw : 1;       // 
    	uint8_t Res : 1;       // 预留
		uint8_t Res1 : 6;       // 预留    	     
		
    }bit;
}InvCtrInfoStruct;

//extern InvCtrInfoStruct InvCtrInfo; 

typedef union
{
    uint32_t all;
    struct
    {
        uint8_t src : 8;       // 8bit 源地址
        uint8_t dst : 8;       // 8bit 目标地址
        uint8_t funcode  : 8;  // 8bit CAN帧功能
        uint8_t page : 2;      // 2bit 页编码(0-3) 每页255帧
        uint8_t priority : 3;  // 3bit
    }bit;
}IdStruct;

typedef union {
    uint8_t data[8]; // 单帧CAN数据

    struct {
        uint8_t cmd;

        union {

            uint8_t cmd_data[7];
            
            struct {
                // data[1]
                uint8_t maintain_enable : 2; // bit0/1 电池维护使能 0-invalid; 1-enable; 2-disable
                uint8_t reserved2_3     : 2; // bit2/3 预留
                uint8_t reserved4_5     : 2; // bit4/5 预留
                uint8_t reserved6_7     : 2; // bit6/7 预留
                
                // data[2]~data[7] 预留
                uint8_t reserved[6]; 
            } battery_aging;

            struct {
                uint16_t l1_active_power_limit;
                uint16_t l1_reactive_power_limit;
                uint16_t pv_power_limit;
                uint8_t inv_sum;
            } ems_ctrl1;

            struct {
                uint16_t l2_active_power_limit;
                uint16_t l2_reactive_power_limit;
            } ems_ctrl2;

            struct {
                uint16_t l3_active_power_limit;
                uint16_t l3_reactive_power_limit;
            } ems_ctrl3;

            struct {
                uint8_t DisaterWarnCharge : 1;
                uint8_t bt_connect        : 1;
                uint8_t revd              : 6;
            } ems_ctrl4;

            struct {
                // DATA[1]
                uint8_t sleepSet      : 2;
                uint8_t powerOffCtrl  : 2;
                uint8_t iot_ctrl_mask : 2;
                uint8_t revd : 2;
                // DATA[2]
                uint8_t remote_Soc;
                // DATA[3]
                uint8_t reset_factory : 2;
                uint8_t reset_energy  : 2;
                uint8_t revd2         : 4;
                // DATA[4]
                uint8_t led_brightness_set;
            } ems_ctrl5;
        };
    };
} can_ctrl_t; // 0x08FAFFXX 逆变器CAN 单帧控制，data[8]

_Static_assert(sizeof(can_ctrl_t) == 8, "can_ctrl_t size mismatch");

typedef struct
{
    uint8_t  type;
    uint8_t  seq;
    uint8_t  bytes[6];
}DataStruct;


typedef struct
{
    uint8_t  type;
    uint16_t start;
    uint16_t total;
    uint16_t append;    // 附加信息,如果total = 0, 附加信息为错误码,反之,附加信息为crc16
}AckStruct;

typedef struct
{
    uint8_t  waitComplete;
    uint8_t  type;
    uint8_t  page;
	uint8_t	 dst;
    uint16_t start;
    uint16_t total;
    uint16_t errCode;
}HandleInfo;

typedef struct {
    uint8_t node_id; // not use,0
    IdStruct can_id;
    uint8_t data_type;
    uint32_t offset;
    uint32_t data_len;
    uint8_t *data;
} can_tx_rx_cmd_t;

typedef can_tx_rx_cmd_t can_recv_cmd_t;
typedef can_tx_rx_cmd_t can_send_cmd_t;

enum {
    CAN_RX_TYPE_HISTORIC_DATA = 0xFA,
};

#define CAN_RX_CMD_TYPE_INIT { \
    CAN_RX_TYPE_HISTORIC_DATA, \
}

#define CAN_RX_TEMP_SIZE 1100

typedef struct {
    int16_t Voltage;//电压 0.1V
    int32_t Current;//电流 0.01A
    int16_t Power;//功率 1W
}DCDC_info_struct;

typedef struct {
    uint16_t Voltage;//电压 
    uint16_t Current;//电流 
}BAT_info_struct;

extern STRUCT_CAN_READ can_node_rd;
extern STRUCT_CAN_WRITE can_node_wr;

typedef struct {
	MOD_STRUCT_reg15500 mod_reg15500_DCDC_info;// 	DCDC模块通用 信息区
	MOD_STRUCT_reg15600 mod_reg15600_DCDC_set;// 	DCDC模块通用 设置区
	MOD_STRUCT_reg40000 mod_reg40000_DCDC_set;// 	DCDC模块私有 设置区
}STRUCT_UART_DATA;// 
// extern STRUCT_UART_DATA uart_data_wr;
// extern STRUCT_UART_DATA uart_data_rd;


//extern EXT_RAM_BSS_ATTR MOD_STRUCT_Inv_MIX_READ	    Inv_can_mix;//一个大系统中 仅一个模块，杂项混合
//extern EXT_RAM_BSS_ATTR MOD_STRUCT_Inv_MIX_WRITE	Inv_can_mix_WR;//一个大系统中 仅一个模块，杂项混合
extern QueueHandle_t can_cmd_queue;//从BLE/WIFI到CAN的透彻转发，队列

typedef struct {
    uint16_t type; // 0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS；4-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块；10-PACK高压模块11-内置HMI模块；12-内置HMI模块2；13--RF穿墙模块；14-DC-HUB；15-AC-HUB；16-DC-DC；
    uint32_t version; // mcu版本号
}soft_version2;


typedef struct {
    char type_ascii[12]; // pcak类型
    uint64_t dev_sn;
	soft_version2 soft[6];
	uint8_t count_version;//一个类型号下面版本号数
	uint8_t count_online;//一个类型号下面实际在线的电池包数量 6154
} pack_base_t;


#pragma pack()

extern CanOtaStruct can_ota_status;


void iot_can_data_init(void);
void iot_to_can_cmd_task(void);//(void * pvParameters);

uint8_t CanGetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, uint16_t data_len, uint8_t dst_addr);
uint8_t CanSetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, const uint8_t *pdata, uint16_t data_len, uint8_t dst_addr);
void iot_can_cmd_check(void);
uint8_t CanAckData(uint8_t node_id, uint32_t id, const uint8_t *raw, uint8_t len);
uint8_t CanAckStart(uint8_t node_id, uint32_t id, uint16_t type, uint16_t start, uint16_t len, uint16_t crc16);
void Device_Can_Address_Parallel_Step(void);

void Can_Beta_Tx_Block_Timeout_Check(uint8_t interval);

// void judge_inv_min_version(void);//判断每组系统里最小的arm、dsp、bms版本号
// void judge_pack_min_version(void);//判断一组电池包里最小的从包版本号
void single_pack_information(void);//单包信息7200 
int findFirstMatchVersion(const uint32_t strings[], size_t count, const uint32_t target);
void Pack_6300_offset(uint8_t *str, uint8_t num, uint8_t seq);
void Pack_7200_offset(uint8_t *str, uint8_t num, uint8_t seq);
void CAN_IOT_Parameter_WR(void);//1s cycle
void CAN_DEV_SN_Get(uint8_t devId);
void can_init_top(void );
void CanRx_Parse_Task(void);
void CanTimerout_Cb(void);
void iot_polldev_task(void);
void Can_inv_send_DCDC_state(void);

void dcdc_send_can_data_poll(uint16_t type) ;
void current_ota_info_update(uint8_t isOta, uint8_t type, uint8_t pct, uint8_t errCode, uint32_t soft_version);
OtaDevice get_dev_ota_announce_status_address(void);

uint8_t Device_Can_Address_Parallel_SetTriger(uint16_t time) ;
uint8_t Can_Tx_single_binding_triger(void); //100ms cycle
uint8_t Can_Tx_single_binding_Check(void); //100ms cycle
void CAN_Dev_PowerOff_Check(void);
void iot_sumdev_task(void);
void Iot_set_bat_maintain_once(uint8_t maintain_enable);
void can_data_poll_fast_from_modbus(void); 
void IoT_Status_Parameter_update(uint16_t offset, uint16_t len);
void clear_canbus_rx_queue(void);

void Iot_EMS_Ctrl_Func_5_Send(uint8_t PowerCtrl, uint8_t reset_factory, uint8_t reset_energy);
uint8_t can_ota_is_doing(void);
void Iot_can_ota_ready_period(void);
void Iot_Dcdc_Ctrl_frame_Send(bool send_flag);
void Iot_Dcdc_Common_Target_Send(uint8_t index, uint16_t v_max, uint16_t i_max);
void Iot_Heartbeat_frame_Send(bool fast_flag);

#endif
