#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "can_control.h"
#include "can_protocol.h"
#include "can_data.h"
#include "can_transmit.h"


typedef struct {
	uint32_t last;
	uint32_t now;
} report_time_t;

/* 用于主动上报超时检测 

单帧 报文超时缓存
超时发生，会清除can_ctrl_data[] rx 报文内容
*/
static report_time_t can_inv_report_timeout[DEV_MAIN_NODE_MAX][INV_MAX_NUM];
static report_time_t can_pack_report_timeout[DEV_MAIN_NODE_MAX][INV_MAX_NUM];

/* 综合控制帧应答指令初始化,在该数组中的指令才会被解析 

ID:0x08FAFFXX ，data[0]

*/
static can_reply_cmd_t reply_cmd_init[] = CAN_CTRL_REPLY_CMD_INIT;

/* 综合控制帧应答的数据缓存 */
static can_reply_data_t can_ctrl_data[DEV_MAIN_NODE_MAX][sizeof(reply_cmd_init)/sizeof(reply_cmd_init[0])];

/**
 * @brief CAN综合控制帧设置
 * 该函数发送控制指令及其相应的控制数据到底层设备
 *
 
 单帧TX,广播
 * @param node_id 主节点ID：CAN 总线序号，从0开始，EMS有3个CAN，其他仅1个CAN
 * @param can_ctrl 待发送的数据指针
 * @return 0：成功, -1：失败
 */
int can_overall_ctrl(uint8_t node_id, can_ctrl_t *can_ctrl)
{
	if(node_id >= DEV_MAIN_NODE_MAX) return -1;

	/* 填充CANID数据结构 */
	IdStruct canid;
	canid.bit.src = esp_canbus_myself_address();
	canid.bit.dst = CAN_BROADCAST_ADDRESS;
	canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
	canid.bit.page = CAN_CTRL_FRAME_PAGE;
	canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

	/* 发送单帧CAN数据 */
	if (!CanAckData(node_id, canid.all, (uint8_t *)can_ctrl, sizeof(can_ctrl_t))) {
		return -1;
	}

	return 0;
}

/**
 * @brief CAN综合控制帧应答数据获取
 *
 * @param node_id 主节点ID
 * @param reply_cmd 应答的指令类型
 * @return 返回应答数据结构,如果参数错误则返回空指针
 */
can_reply_data_t *can_overall_ctrl_get_reply(uint8_t node_id, can_reply_cmd_t reply_cmd)
{
	if(node_id >= DEV_MAIN_NODE_MAX) return NULL;

	/* 查询应答指令是否有效 */
	int cmd_valid = 0;
	int idx = 0;
	for ( ; idx < sizeof(reply_cmd_init)/sizeof(reply_cmd_init[0]); idx++)
	{
		if(reply_cmd == reply_cmd_init[idx])
		{
			cmd_valid = 1;
			break;
		}
	}
	if(cmd_valid == 0) return NULL;

	/* 返回应答数据 */
	return &can_ctrl_data[node_id][idx];
}

/**
 * @brief CAN综合控制帧应答数据解析
 * 该函数解析底层设备应答的综合控制帧数据并保存在本地缓存中
 *
 * @param node_id 主节点ID
 * @param can_id CAN帧ID
 * @param can_msg CAN帧消息
 * @param frame_len CAN帧消息长度
 * @return 0：成功，-1：失败
 */
static int can_overall_ctrl_parse(uint8_t node_id, uint32_t can_id, uint8_t *can_msg, uint8_t frame_len)
{
	if(node_id >= DEV_MAIN_NODE_MAX || can_msg == NULL || frame_len == 0) return -1;

	IdStruct canid;
	canid.all = can_id;

	/* 检查是否是综合控制应答帧 */
	if (canid.bit.funcode != CAN_CTRL_FRAME_FUNC_CODE) return -1;
	if (canid.bit.priority != CAN_CTRL_FRAME_PRIORITY) return -1;
	if (canid.bit.page != CAN_CTRL_FRAME_PAGE) return -1;

	/* 查询应答指令是否有效 */
	int cmd_valid = 0;
	int idx = 0;
	can_reply_cmd_t reply_cmd = (can_reply_cmd_t)(can_msg[0] & 0x7f);
	for ( ; idx < sizeof(reply_cmd_init)/sizeof(reply_cmd_init[0]); idx++)
	{
		if(reply_cmd == reply_cmd_init[idx])
		{
			cmd_valid = 1;
			break;
		}
	}
	if(cmd_valid == 0) 
		return -1;

	/* 保存应答数据到本地缓存 */
	can_reply_data_t ctrl_data = {0};
	memcpy(&ctrl_data, can_msg, frame_len);
	memcpy(&can_ctrl_data[node_id][idx], &ctrl_data, sizeof(can_reply_data_t));

	return 0;
}

/**
 * @brief CAN主动上报帧检测
 *
 * @param node_id 节点ID
 * @param can_id CAN Rx ID
 * @param can_msg CAN消息指针
 * @param frame_len CAN消息长度
 */
static void can_overall_ctrl_report_check(uint8_t node_id, uint32_t can_id, uint8_t *can_msg, uint8_t frame_len)
{
	if(node_id >= DEV_MAIN_NODE_MAX || can_msg == NULL || frame_len == 0) return;

	IdStruct canid;
	canid.all = can_id;

	/* 非本机地址不处理 */
    if(canid.bit.dst != esp_canbus_myself_address()) 
		return;

	uint8_t index = 0;
	report_time_t *p_time = NULL;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

	if((canid.bit.src >= INV_CAN_ADDR) && (canid.bit.src < (INV_CAN_ADDR + DEV_MAIN_NODE_MAX*INV_MAX_NUM)))
	{
		index = canid.bit.src - INV_CAN_ADDR;
		uint8_t	online = Inv_can[node_id].inv_data[index].online;
		if(online) {
			p_time = &can_inv_report_timeout[node_id][index];
		}
	}
	else if((canid.bit.src >= PACK_CAN_ADDR) && (canid.bit.src < (PACK_CAN_ADDR + DEV_MAIN_NODE_MAX*PACK_MAX_NUM)))
	{
		index = canid.bit.src - PACK_CAN_ADDR;
		uint8_t	online = Inv_can[node_id].pack_data[0].pack_announce.online & (1 << index);
		if(online) {
			p_time = &can_pack_report_timeout[node_id][index];
		}
	}
	else 
	{
		return;
	}

	/* 底层设备主动上报帧超时检测 */
	if(p_time)
	{
		if(p_time->last == 0) {		// 初始化
			p_time->last = now_time;
		}

		p_time->now = now_time;
		if((p_time->now - p_time->last) > CAN_CTRL_REPORT_TIMEOUT) // 上报超时判断
		{
			/* 超时后清除节点响应缓存,当其它任务检测到缓存清零后会重新下发‘打开主动上报帧’ */
			memset(can_ctrl_data[node_id], 0x00, sizeof(can_ctrl_data[node_id]));
			p_time->last = p_time->now;
		}
	}

	/* 主动上报帧功能码 */
	const uint8_t inv_report_pf[] = CAN_CTRL_INV_REPORT_PF;
	const uint8_t pack_report_pf[] = CAN_CTRL_PACK_REPORT_PF;

	/* 检测是否是主动上报帧 */
	for(int i = 0; i < sizeof(inv_report_pf); i++)
	{
		if(canid.bit.funcode == inv_report_pf[i]) {
			goto __update;
		}
	}

	for(int i = 0; i < sizeof(inv_report_pf); i++)
	{
		if(canid.bit.funcode == pack_report_pf[i]) {
			goto __update;
		}
	}

	/* 非主动上报帧退出 */
	return;

__update:
	/* 更新超时时间 */
	if(p_time) {
		p_time->last = p_time->now;
	}
}

/**
 * @brief CAN综合控制帧处理
 * 	- 处理响应数据
 *  - 检测主动上报帧是否超时
 *
 * @param node_id 节点ID
 * @param can_id CAN Rx ID
 * @param can_msg CAN消息指针
 * @param frame_len CAN消息长度
 */
void can_overall_ctrl_handle(uint8_t node_id, uint32_t can_id, uint8_t *can_msg, uint8_t frame_len)
{
	/* 响应数据解析 */
	can_overall_ctrl_parse(node_id, can_id, can_msg, frame_len);

	/* 主动上报帧超时检测 */
	can_overall_ctrl_report_check(node_id, can_id, can_msg, frame_len);
}
