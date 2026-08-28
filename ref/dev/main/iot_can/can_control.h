#ifndef __CAN_CONTROL_H__
#define __CAN_CONTROL_H__
#include <stdint.h>


#define CAN_CTRL_FRAME_FUNC_CODE		0xFA	// 综合控制帧功能码
#define CAN_CTRL_FRAME_PRIORITY			0x02	// 综合控制帧优先级
#define CAN_CTRL_FRAME_PAGE				0x00	// 综合控制帧数据页

#define CAN_REPORT_FRAME_FUNC_CODE		0x01	// 周期主动上报帧功能码
#define CAN_REPORT_FRAME_PRIORITY		0x06	// 周期主动上报帧优先级
#define CAN_REPORT_FRAME_PAGE			0x01	// 周期主动上报帧数据页


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
#define CAN_CTRL_CMD_EMS_CTRL4			14 		// EMS控制4
#define CAN_CTRL_CMD_EMS_CTRL5			50 		// EMS控制5

#define CAN_CTRL_CMD_99			99 		//  

#define CAN_CTRL_CMD_BIND_TRIGER		0x55 		// 绑定帧命令
#define CAN_CTRL_CMD_BIND_CHECK			0xAA 		// 检测帧命令


/* CAN综合控制帧响应值定义 */
#define CAN_RESPOND_SUPPORT_OPEN		1 		// 支持并开启
#define CAN_RESPOND_SUPPORT_CLOSE		2 		// 支持并关闭
#define CAN_RESPOND_SUPPORT_INVALID		3 		// 无效/不动作

/* 逆变主动上报帧功能码 */
#define CAN_CTRL_INV_REPORT_PF {		\
	0x01, 0x02, 0x03					\
}

/* PACK主动上报帧功能码 */
#define CAN_CTRL_PACK_REPORT_PF {		\
	0x65, 0x66, 0x67					\
}

/* 应答指令初始化,用于内部变量初始化 */
#define CAN_CTRL_REPLY_CMD_INIT	{		\
	CAN_REPLY_CMD_EN_PACK_REPORT,		\
	CAN_REPLY_CMD_DIS_PACK_REPORT,		\
	CAN_REPLY_CMD_EN_INV_REPORT,		\
	CAN_REPLY_CMD_DIS_INV_REPORT,		\
}

/**
 * @brief CAN综合控制帧应答指令定义
 */
typedef enum {
	CAN_REPLY_CMD_EN_PACK_REPORT = CAN_CTRL_CMD_EN_PACK_REPORT,			// 使能pack上报响应
	CAN_REPLY_CMD_DIS_PACK_REPORT = CAN_CTRL_CMD_DIS_PACK_REPORT,		// 关闭pack上报响应
	CAN_REPLY_CMD_EN_INV_REPORT = CAN_CTRL_CMD_EN_INV_REPORT,			// 使能逆变上报响应
	CAN_REPLY_CMD_DIS_INV_REPORT = CAN_CTRL_CMD_DIS_INV_REPORT,			// 关闭逆变上报响应
} can_reply_cmd_t;

/**
 * @brief CAN综合控制帧控制结构定义
 */
#pragma pack(push, 1)
typedef union {
	uint64_t all;
	uint8_t data[8];//windy add

	struct {
		uint8_t cmd;

		union {
			struct {
				uint8_t param;
			} battery_aging;						// 电池老化数据结构

			struct {
				uint16_t l1_active_power_limit;
				uint16_t l1_reactive_power_limit;
				uint16_t pv_power_limit;
				uint8_t inv_sum;
			} ems_ctrl1;							// EMS控制1数据结构

			struct {
				uint16_t l2_active_power_limit;
				uint16_t l2_reactive_power_limit;
			} ems_ctrl2;							// EMS控制2数据结构

			struct {
				uint16_t l3_active_power_limit;
				uint16_t l3_reactive_power_limit;
			} ems_ctrl3;							// EMS控制3数据结构

			struct {
				uint8_t DisaterWarnCharge:1;		// bit0 风暴预警充电命令
				uint8_t bt_connect:1;				// bit1 蓝牙连接状态
				uint8_t revd:6;						//预留
			} ems_ctrl4;							// EMS控制4数据结构

			struct {
				uint8_t sleepSet:2;					//bit0~1 休眠模式设置	1：启用休眠模式  2：禁用休眠模式
				uint8_t powerOffCtrl:2;				//bit2~3 0:无效 1：开机 2：关机 3：休眠	
				uint8_t iot_ctrl_mask:2;			//bit：0：为0无效 为1表示1900段控制的AC开关  为2表示关闭
				uint8_t revd:2;
				uint8_t remote_Soc;				//SOC阈值
			} ems_ctrl5;							// EMS控制5数据结构
		};
	};
} can_ctrl_t;//0x08FAFFXX 逆变器CAN 单帧控制， data[8]
#pragma pack()

/**
 * @brief CAN综合控制帧控制结构定义
 */
#pragma pack(push, 1)
typedef union {
	uint64_t all;
	uint8_t data[8];//windy add

	struct {
		uint16_t power_l1;
        uint16_t power_l2;
        uint16_t power_l3;
	};
} can_ems_t;//0x1901ffxx 逆变器CAN 单帧控制， data[8]
#pragma pack()

/**
 * @brief CAN综合控制帧数据结构定义
 */
#pragma pack(push, 1)
typedef union {
	uint64_t all;

	struct {
		uint8_t cmd;

		union {
			struct {
				uint8_t respond;
			} en_pack_report_reply;					// 使能pack上报应答数据结构

			struct {
				uint8_t respond;
			} dis_pack_report_reply;				// 关闭pack上报应答数据结构

			struct {
				uint8_t respond;
			} en_inv_report_reply;					// 使能逆变上报应答数据结构

			struct {
				uint8_t respond;
			} dis_inv_report_reply;					// 关闭逆变上报应答数据结构
		};
	};
} can_reply_data_t;
#pragma pack()

int can_overall_ctrl(uint8_t node_id, can_ctrl_t *can_ctrl);
can_reply_data_t *can_overall_ctrl_get_reply(uint8_t node_id, can_reply_cmd_t reply_cmd);
void can_overall_ctrl_handle(uint8_t node_id, uint32_t can_id, uint8_t *can_msg, uint8_t frame_len);

#endif
