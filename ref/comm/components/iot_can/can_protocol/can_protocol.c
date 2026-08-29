#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "can_transmit.h"
#include "can_data.h"
#include "can_protocol.h"
#include "esp_log.h"
#include "iot_ota.h"
#include "crc.h"
#include "freertos/event_groups.h"
#include "esp_bit_defs.h"
#include "data_summary.h"
#include "modbus_data.h"
#include "modbus_protocol.h"
#include "filesystem.h"
#include "parameter.h"
#include "http_client.h"
#include "comm_define.h"
#include "sync_time.h"
#include "DisasterWarn.h"
#include "tou_relay_ctrl.h"
#include "ble_dev.h"
#include "bms_can.h"
#include "bms_ota.h"
#include "aiot_log.h"
#include "aiot_can_log.h"

#define TAG "[CAN_PROTOCOL]"
#define SUB_TIME(time, interval)    ((time) > (interval)) ? ((time) - (interval)) : (0)


#define SUPER_FAST_POLL_INTV		1500					// 超级快速轮询周期ms
#define FAST_POLL_INTV				6000					// 快速轮询周期ms
#define COMMON_POLL_INTV		    10000					// 标准轮询周期ms
#define SLOW_POLL_INTV				30000					// 慢速轮询周期ms
#define SLOW_POLL_IOT				30000                   // 定义轮询IOT数据周期
#define INV_ABOUT_DATA_POLL_INTV	15000                   // 定义轮询逆变ABOUT数据周期
#define AC_HUB_DATA_POLL	        0
#define DC_HUB_DATA_POLL	        0
#define DCDC_HUB_DATA_POLL	        0

#define CAN_TIMER_INTERVAL 			100// timeout 200  					// 定时器周期ms
#define MAX_BYTES 					FRAME_DATA_BYTES * 0xFF

/*
阶段	描述	CAN ID	data[0]/子功能码	data[1]/
step	data[2]	data[3]	data[4]	data[5]	data[6]	data[7]
step1	并机排地址触发0x08FAFFXX	99	1		/	/	/	/	/
step2	本机设备SN上报0x08FAFFXX	99	2	SN_L1(最低字节	SN_2	SN_3	SN_4	SN_5(最高字节)	/
step3	排地址完成（正常心跳帧0x08FAFFXX	99	3	/	/	/	/	/	群组编号(预留)
*/

#define	STEP_CAN_PARALLEL_DELAY1		30//step切换延迟，秒
#define	STEP_CAN_PARALLEL_DELAY2		10//
#define	STEP_CAN_SEND_PERIOD		    3//300ms 主设备切换延时
#define	DCDC_CTRL_OTHER_INVALID		    2//0-无效;
#define	DCDC_CTRL_OTHER_SLAVE_MODE	    2//1-放电服从调度模式;
#define	DCDC_CTRL_OTHER_ADDITION_MODE	2//2-放电补充模式;
#define	DCDC_CTRL_OTHER_MASTER_MODE	    3//3-强制主动调度模式

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
    /* cmd 超时定时器*/
    TimerHandle_t timer;
}can_cmd_bk_struct;


can_cmd_bk_struct  can_cmd_bk = {0};    // 备份read cmd
TimerHandle_t   can_task_timer = NULL;

QueueHandle_t xCanBusQueue_Recv = NULL;//CAN底层 Rx队列
QueueHandle_t xCanBusQueue_Send = NULL;//CAN底层 Tx队列,=10*10^-3/(1/(250*1000)*8*13)=24,250kbps,10ms发24帧
QueueHandle_t can_cmd_queue = NULL;//从BLE/WIFI到CAN的透彻转发，队列

USE_EXT_RAM_BSS CanOtaStruct can_ota_status = {0};//[DCDC_MAX_NUM];//uart ota(仅用can_ota_status[0])和 can  ota共用，

USE_EXT_RAM_BSS STRUCT_CAN_READ can_node_rd = {0};//CAN总线所有设备 汇总
USE_EXT_RAM_BSS STRUCT_CAN_WRITE can_node_wr = {0};

USE_EXT_RAM_BSS STRUCT_UART_DATA uart_data_wr = {0};//内部uart modbus汇总，D400S专用
USE_EXT_RAM_BSS STRUCT_UART_DATA uart_data_rd = {0};

static uint8_t iot_setting_update_flag = 0;

void Device_Can_Address_Parallel_Bind(twai_message_t *frame);
int can_overall_ctrl(uint8_t node_id, can_ctrl_t *can_ctrl);
static uint8_t CanCheckSetStatus(void);
static void CanSetWaitRespones(const twai_message_t *frame);
static void CanGetWaitResponse(node_info_t *node, const twai_message_t *frame);
static void CanLocalDataIsGet(node_info_t *node, const twai_message_t *frame);
static void CanLocalDataIsSet(node_info_t *node, const twai_message_t *frame);
uint8_t CanAckStart(uint8_t node_id, uint32_t id, uint16_t type, uint16_t start, uint16_t len, uint16_t crc16);
static void CanListenBus(node_info_t *node, twai_message_t *frame);
static void CanListenBus_From_IOT(node_info_t *node, twai_message_t *frame) ;
static void CanCmdGenerate(uint8_t md_addr, can_data_label *cmd_label, uint16_t num);
static void iot_can_bind_send_and_check(void);

void ota_can_pct(OtaDevice canOta[], uint8_t id, uint8_t type, uint8_t pct, uint8_t where, uint8_t errcode);
void ota_dev_timeout(OtaDevice canOta[]) ;
void can_ota_summary(uint8_t node_id, uint8_t group);
void Can_Inv_Addr_Check(node_info_t *node, twai_message_t *frame); 
void  CAN_IOT_ID_Get(uint8_t devId);
void RTC_Update_iot_to_can(void);

extern uint8_t iot_test_index_valid(uint16_t index);

#ifdef CONFIG_CAN_PORT_ENABLE

void iot_can_data_init(void) 
{
#if 0
    memset(&can_ota_status, 0, sizeof(can_ota_status));
    memset(&can_node_rd, 0, sizeof(can_node_rd));
    memset(&can_node_wr, 0, sizeof(can_node_wr));
#endif

	reals.Addr_can_master = 1;
    
#if DEV_MAIN_NODE_MAX > 1
	reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_AFTER;
#endif
}

/*------------------------------------------------------------------------------
 Function: CanTimerout_Cb
 -----------------------------------------------------------------------------*/
/**
  * @brief      100ms定时器
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
void CanTimerout_Cb(void) 
{
    /*指令超时计时*/
    if (can_cmd_bk.write_cmd.wait_time) {
        can_cmd_bk.write_cmd.wait_time = SUB_TIME(can_cmd_bk.write_cmd.wait_time, CAN_TIMER_INTERVAL);
    }

    /*OTA超时检测*/
	if (can_ota_status.timecnt) { 
        /*IOT通过CAN总线给下位机升级时所用的定时器*/
		can_ota_status.timecnt = SUB_TIME(can_ota_status.timecnt, CAN_TIMER_INTERVAL);
	}

	/* CanOtaSender开始时状态为非空闲,CanOtaSender结束后状态为空闲 */
	if ((can_ota_status.ota_doing != CAN_OTA_IDLE)&&(can_ota_status.ota_doing <= CAN_OTA_SILENT)) {
		can_ota_status.period_cmd_delay = CAN_OTA_PERIOD_TIMEOUT;
	    can_ota_status.ready_timeout = UINT16_MAX;
    
        /*汇总OTA状态*/
		can_ota_summary(0, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type);
	}

    /*CAN总线升级静默超时*/
	if(can_ota_status.period_cmd_delay > 0) {
		can_ota_status.period_cmd_delay = SUB_TIME(can_ota_status.period_cmd_delay, CAN_TIMER_INTERVAL);

        // 实际升级时超时才执行
		if((can_ota_status.period_cmd_delay == 0) && (can_ota_status.ota_doing != CAN_OTA_IDLE)) {
			can_ota_status.ota_doing = CAN_OTA_IDLE;
			ota_dev_timeout(can_ota_status.devStatus);
            ESP_LOGW(TAG, "CanTimerout_Cb : OTA period_cmd_delay timeout! ");
            
            /*汇总OTA状态*/
            can_ota_summary(0, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type); 
		}
	}

    // 超时清除升级状态
    if ((!can_ota_status.ota_doing)&&( can_ota_status.ready_timeout > 0 )) {   
        // 固件传输时不计算超时
        if ( top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.firmware_transfer ) {
            can_ota_status.ready_timeout = UINT16_MAX;
        } else {
            if ( can_ota_status.ready_timeout == UINT16_MAX ) {
                /*汇总OTA状态*/
                can_ota_summary(0, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type); 
                if ( 0 != top_modbus_wr.Inv.mod_reg00700_OTA.ota_cmd.begin ) {
                    can_ota_status.ready_timeout = 30000;  // 30s (蓝牙升级间隔)
                } else {
                    can_ota_status.ready_timeout = 60000;  // 60s (云端升级间隔)
                }
            }
            can_ota_status.ready_timeout = SUB_TIME(can_ota_status.ready_timeout, CAN_TIMER_INTERVAL);
        }
        
        if (( can_ota_status.ready_timeout == 0 ) || can_ota_status.is_only_stage) {
            /*初始化*/
            memset(&can_ota_status, 0, sizeof(can_ota_status));
            memset(&reals.system_ota, 0, sizeof(reals.system_ota));
            set_ota_pct_info(NULL, 0, 0, 0, 0, 0, 0);
            ESP_LOGW(TAG, "System OTA End, OTA status clean! ");

            /*发送EMS控制帧，同步休眠设置*/
            Iot_EMS_Ctrl_Func_5_Send(0, 0, 0);
        }
    }

	/*CAN节点通信超时清零*/
	CanNodeOfflineCheck(CAN_TIMER_INTERVAL);
}

/*------------------------------------------------------------------------------
 Function: can_ota_is_doing
 -----------------------------------------------------------------------------*/
/**
  * @brief      指示can-ota状态，0: 空闲，1：ota正在进行
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t can_ota_is_doing(void)
{
    return can_ota_status.ota_doing;
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
	IdStruct canid = {0};
	can_ctrl_t can_ctrl = {0};
    static uint16_t sStepSendPeriodCnt = 0;//Step CAN发送周期

	if(1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND)
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

			ESP_LOGI(TAG, "Can_Tx_single_binding_triger ");// 
			
			/* 发送单帧CAN数据 */
			if (!CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t))) 
			{
				return 0;
			}		
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
	IdStruct canid = {0};
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

/*------------------------------------------------------------------------------
 Function: get_dev_ota_announce_status_address
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取主动上报数组地址
  * @param[in]  None
  * @param[out] None
  * @return     OtaDevice
  */
OtaDevice get_dev_ota_announce_status_address(void) 
{
    return can_ota_status.ota_summary;
}


/**
 * @brief modbus的所有设置是写入设置缓存区，然后再将缓存区的数据转换为CAN命令发送给设备
 *	1、手机APP发送设置数据(modbus写入数据到IOT的设置缓存区，每个可以设置区间，都带有缓存区)
 *	2、数据写入缓存区时，就已经通过转换关系表得到了，需要缓存为CAN命令的数据类型，数据偏移，数据长度
 *	3、将modbus设置缓存区中的数据，生成对应的CAN命令发送给设备
 *	4、某些指令发送后还会激活与其相关的联动指令，如历史日志指令
 */

/*------------------------------------------------------------------------------
 Function: iot_to_can_cmd_task
 -----------------------------------------------------------------------------*/
/**
  * @brief      从BLE/WIFI到CAN的透彻转发
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_to_can_cmd_task(void)
{
	can_cmd_queue_struct can_cmd_msg = {NULL, 0, 0};
    
	/* 等待MODBUS接收设置命令的信*/
	if (can_cmd_queue && xQueueReceive(can_cmd_queue, &can_cmd_msg, 0) == pdTRUE) //portMAX_DELAY
	{ 
		if(reals.BLE_WIFI_to_Can_Cnt > 0) {
			reals.BLE_WIFI_to_Can_Cnt--;
		}
		// 创建并发送CAN命令
		CanCmdGenerate(can_cmd_msg.md_addr, can_cmd_msg.cmd, can_cmd_msg.num); 
		if (can_cmd_msg.cmd) {
			free(can_cmd_msg.cmd);
		}
	}
    else
    {
		if(reals.BLE_WIFI_to_Can_Cnt > 0) {
            ESP_LOGE(TAG, "iot_to_can_cmd_task : BLE_WIFI_to_Can_Cnt(%d) ERROR! ", reals.BLE_WIFI_to_Can_Cnt); 
			reals.BLE_WIFI_to_Can_Cnt = 0;
		}
    }
}

/*------------------------------------------------------------------------------
 Function: CanOtaTask
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN_OTA任务
  * @param[in]  uint8_t node_id              
                const twai_message_t *frame  
                CanOtaStruct *ota_status     
  * @param[out] None
  * @return     static void
  */
static void CanOtaTask(uint8_t node_id, const twai_message_t *frame, CanOtaStruct *ota_status)
{
    /*CAN主动升级任务*/
    CanOtaSender(node_id, frame, ota_status);   // can ota send

#ifdef IOT_CAN_RECEIVER_USE_ENABLE

    /*CAN被动升级任务（当前仅支持自身）*/
    CanOtaReceiver(node_id, frame, ota_status);  // can ota receive
    
#endif
}

/*------------------------------------------------------------------------------
 Function: CanRx_Parse_Task
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN RX报文接收解析
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void CanRx_Parse_Task(void)
{
    can_queue_msg_struct queue_msg = {NULL};
    uint16_t handl_count = 0;
	uint8_t level1_addr = 0;
	uint8_t level2_addr = 0;

	handl_count = 50;
#ifndef MASS_PRODUCTION_CONFIG_ENABLE
	// 测试CAN队列拥挤度
	uint16_t remaining = uxQueueMessagesWaiting(xCanBusQueue_Recv);
	if(remaining > 120) {
		ESP_LOGW(TAG, "xCanBusQueue_Recv remaining: %d", remaining);
	}
#endif
	while(0 != handl_count)//单次大循环周期，最多只解析50帧，防止主循环死
	{
		handl_count--;
		if(xCanBusQueue_Recv && xQueueReceive(xCanBusQueue_Recv, &queue_msg, pdMS_TO_TICKS(0)) == pdTRUE)//pdMS_TO_TICKS(task_delay)
		{
#if CONFIG_LOG_UDP_REDIRECT == 2 
            super_cmd_can_log_cfg_t can_log_cfg = { .reg_val = reals.supper_control.cmd_list[SUPPER_CMD_CAN_LOG_SAVE].value };
            if ((CAN_LOG_STATUS_RECORDING == can_log_cfg.bit.record_status) 
                && ((CAN_LOG_DIR_RX_ONLY == can_log_cfg.bit.can_direction)
                    || (CAN_LOG_DIR_TXRX_ALL == can_log_cfg.bit.can_direction)))
            {
                /*不过滤CANid，全部处理*/
                aiot_log_can_frame_t log_frame = {.id.value = 0, .data = {0}};
                log_frame.id.identifier = queue_msg.pdata->identifier & 0x1FFFFFFF; // 仅保留29位ID
                log_frame.id.tx = 0; // 标记为接收帧
                memcpy(log_frame.data, queue_msg.pdata->data, sizeof(queue_msg.pdata->data));
                AIoT_Can_Log_Push((const uint8_t *)&log_frame, (uint16_t)sizeof(log_frame), LOG_TYPE_CAN_RX);
            }
#endif  

	        /*CAN beta ID解析*/
			if(Can_ID_Analyze(queue_msg.pdata->identifier, &level1_addr ,&level2_addr) != 0)
            {
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
                // 接收到CAN帧 alpha协议
            	bms_can_handle(queue_msg.pdata); 
#endif            
                goto end;
            }
			node_info_t node_info;
			node_info.node_id = 0;
			node_info.iots_info = &can_node_rd.Iot;
			node_info.invs_info = &can_node_rd.Inv;
			node_info.packs_info = &can_node_rd.Pack;
			node_info.dcdc_s_info = &can_node_rd.Dcdc;
			node_info.dchubs_info = &can_node_rd.DCHUB;

#if CONFIG_LOG_UDP_REDIRECT == 2 
            supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
            supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
            if ((1 == resource.can) && (1 == direction.rx))
            {
                /*当前CAN帧,不是发送给本机,不处理*/
                IdStruct canId = { .all = queue_msg.pdata->identifier};
                if (canId.bit.dst == esp_canbus_myself_address() 
                    || canId.bit.dst == CAN_BROADCAST_ADDRESS ) 
                {
                    aiot_log_can_frame_t log_frame = {.id.value = 0, .data = {0}};
                    log_frame.id.identifier = queue_msg.pdata->identifier & 0x1FFFFFFF; // 仅保留29位ID
                    log_frame.id.tx = 0; // 标记为接收帧
                    memcpy(log_frame.data, queue_msg.pdata->data, sizeof(queue_msg.pdata->data));
                    aiot_log_push((const uint8_t *)&log_frame, (uint16_t)sizeof(log_frame), LOG_TYPE_CAN_RX);
                }
            }
#endif  

			/* 接收发送给本机的CAN写指如果接收到期望的指令,
			* 该函数返回后会清零CAN,后续功能模块将忽略此CAN帧以避免错误处理 */
			CanListenBus(&node_info, queue_msg.pdata);			/* 监听设备广播的数*/
			CanListenBus_From_IOT(&node_info, queue_msg.pdata); /* 监听其他IoT数据 */
			CanLocalDataIsGet(&node_info, queue_msg.pdata); 	/* 本地数据被读*/

			/* IOT设备的标定信息通过CAN写入 */
			CanLocalDataIsSet(&node_info, queue_msg.pdata); 	/* 本地数据被写*/
			CanSetWaitRespones(queue_msg.pdata);				/* IOT发送写入设备后,等待设备的应*/
			CanGetWaitResponse(&node_info, queue_msg.pdata);	/* IOT发送读取设备后,等待设备的应*/

#ifdef CAN_ADDRESS_PARALLEL_ENABLE
			Can_Inv_Addr_Check(&node_info, queue_msg.pdata);// 
#endif	          
		} 
		else 
		{
			queue_msg.pdata = NULL;
		}
	
        /*CAN升级任务*/
        CanOtaTask(0, queue_msg.pdata, &can_ota_status);

        end:

#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
        // CAN BMS OTA 
    	bms_ota_handle(queue_msg.pdata); 
#endif
		if (queue_msg.pdata) 
		{
			free(queue_msg.pdata); /* 处理完成释放CAN数据 */
			queue_msg.pdata = NULL;
		}
		else//NULL
		{
			break;//没有RX数据，退出循环
		}
	}

}

/**
 * @brief 创建 BMS CAN 任务。
 *
 * 该函数初始化 CAN 总线，并创建用于接收和发送 CAN 帧的队列。
 * 如果任何队列创建失败，将记录错误消息并返回。
 *
 * @return 无。
 */
void can_init_top(void )
{
	/* 初始化 CAN 总线 */
	esp_canbus_init();

	/* 创建CAN总线,接收消息队列 */
	xCanBusQueue_Recv = xQueueCreate(256, sizeof(can_queue_msg_struct));
	if (!xCanBusQueue_Recv) {
		ESP_LOGE (TAG, "Can bus received queue create failed");
	}

	/* 创建CAN总线,发送消息队*/
	xCanBusQueue_Send = xQueueCreate(256, sizeof(can_queue_msg_struct)); 
	if (!xCanBusQueue_Send) {
		ESP_LOGE (TAG, "Can bus send queue create failed");
	}

	// 消息队列,modbus的设置通过消息队列通知任务转换成CAN命令
	can_cmd_queue = xQueueCreate(12, sizeof(can_cmd_queue_struct));
	if (can_cmd_queue == NULL) {
		ESP_LOGE(TAG, "can_cmd_queue create failed");
		abort();
	}

}

/*------------------------------------------------------------------------------
 Function: Can_Beta_Tx_Block_Timeout_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      超时计算
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Can_Beta_Tx_Block_Timeout_Check(uint8_t interval)//10ms cycle
{
#ifdef	CAN_MASTER_POLL_ENABLE
	
	if(reals.Can_beta_block_send_timeout_Cnt > interval)
	{
		reals.Can_beta_block_send_timeout_Cnt -= interval;
	}
	else
	{
		reals.Can_beta_block_send_timeout_Cnt = 0;
	}
#endif
}

/*------------------------------------------------------------------------------
 Function: can_data_poll_fast_from_modbus
 -----------------------------------------------------------------------------*/
/**
  * @brief      modbus 快速读取处理（非插队）
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */	
void can_data_poll_fast_from_modbus(void)
{  
    static uint32_t pre_time = 0;

    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if((now_time - pre_time) >= 100)
    {
        pre_time = now_time;
        if(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_FINISH)
        {
            for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
            {
                /*modbus_Rd_cnt_100ms 来源于vLookupDataTab_Can，表示几秒后快速读取（非插队）*/
                if(reals.can_poll[i].modbus_Rd_cnt_100ms > 0)     //仅在正常周期运行时计算超时
                {
                    reals.can_poll[i].modbus_Rd_cnt_last = reals.can_poll[i].modbus_Rd_cnt_100ms;
                    reals.can_poll[i].modbus_Rd_cnt_100ms--;        
                }
                
                if(reals.can_poll[i].modbus_Rd_cnt_last != reals.can_poll[i].modbus_Rd_cnt_100ms)
                {       
                    if((0 == reals.can_poll[i].modbus_Rd_cnt_100ms)
                        &&(0 != reals.can_poll[i].modbus_Rd_cnt_last))            
                    {
                        /*置位，等待iot_polldev_task发送*/
                        reals.can_poll[i].Cnt_bigcycle = 0;
                        reals.can_poll[i].modbus_Rd_cnt_last = reals.can_poll[i].modbus_Rd_cnt_100ms;
                        //ESP_LOGI(TAG, "can_data_poll_fast_from_modbus : type(%d)", reals.can_poll[i].can_type);
                    }   
                }
            }
        }
    }
}

// 自动生成配置表
typedef struct {
    uint8_t can_dev;
    uint8_t can_type;
    uint8_t range_level;
    uint32_t period;
    uint8_t max_x;
    uint8_t max_y;
} can_poll_cfg_t;
#define ENTRY_STRUCT(dev, type, range, period, max_x, max_y) {dev, type, range, period, max_x, max_y},
static const can_poll_cfg_t can_poll_table[] = {
    CAN_POLL_TABLE(ENTRY_STRUCT)
};
#undef ENTRY_STRUCT

/*------------------------------------------------------------------------------
 Function: can_data_poll_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      初始化多字节查询的所有报文时序、周期、优先级（表格循环优化版）
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void can_data_poll_init(void) 
{
    for (uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++) {
        reals.can_poll[i].can_dev     = can_poll_table[i].can_dev;
        reals.can_poll[i].can_type    = can_poll_table[i].can_type;
        reals.can_poll[i].range_level = can_poll_table[i].range_level;
        reals.can_poll[i].period      = can_poll_table[i].period;
        reals.can_poll[i].max_x       = can_poll_table[i].max_x;
        reals.can_poll[i].max_y       = can_poll_table[i].max_y;
        
        reals.can_poll[i].run_flag = 0;
        reals.can_poll[i].time_old = 0;
        if ( reals.can_poll[i].period > 0 ) {
            reals.can_poll[i].Cnt_bigcycle = 0;
        } else {
            reals.can_poll[i].Cnt_bigcycle = 1;
        }
        reals.can_poll[i].Cnt_smallcycle = 0;
        reals.can_poll[i].modbus_Rd_cnt_last = 0;
        reals.can_poll[i].modbus_Rd_cnt_100ms = 0;

        reals.can_poll[i].modbus_Rd_SlaveAddr = 0;

        reals.can_poll[i].fast_read = 0;
        reals.can_poll[i].fast_sum = 0;
    }
}

/*------------------------------------------------------------------------------
 Function: can_data_poll_common_step
 -----------------------------------------------------------------------------*/
/**
  * @brief      Can Beta多字节查询(快速短周期部分，每次仅发送一条)
  * @param[in]  can_poll_struct *can_poll  
  * @param[out] None
  * @return     static uint8_t      0-本轮未执行；
                                    1-本轮发送成功；
                                    2-本轮全部轮询结束；
  */
static uint8_t can_data_poll_common_step(can_poll_struct *can_poll ) 
{
	uint8_t x = 0;
	uint8_t y = 0;
    uint8_t sendflag = 0;
    uint8_t dev_type = 0;

    if (can_poll == NULL) {
        ESP_LOGE(TAG, "can_poll is NULL");
        return 0;
    }

    /*判断设备查询维度*/
	if((0 == can_poll->max_x)||(0 == can_poll->max_y))
	{
		 ESP_LOGE(TAG, "can_data_poll_common_step : max_x or max_y ERROR !");//理论上不会出现
		 return 0;
	}

	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /*判断是否需要读取*/
	if(((now_time > can_poll->time_old)&&((now_time - can_poll->time_old) >= can_poll->period))	
		||((now_time < can_poll->time_old)&&((can_poll->time_old - now_time) >= can_poll->period))//周期查询
		||(0 == can_poll->Cnt_bigcycle)//首次启动快速获取
		||(1 == can_poll->fast_read))//快速回读标志
	{
	    /*临时插队读取*/
	    if ((( 0 == can_poll->Cnt_bigcycle ) || ( 1 == can_poll->fast_read )) 
            && ( 0 != can_poll->modbus_Rd_SlaveAddr ))
	    {
	        // 检索对应CAN ID，成功后发送
	        uint8_t dev_id = Modbus_Addr_To_Can_Id(can_poll->modbus_Rd_SlaveAddr);
            if(0 != dev_id) CanGetTarget(0, 0, can_poll->can_type, 0, 0, dev_id);
            
            // 设置项回读后立即汇总
            if(1 == can_poll->fast_read) can_poll->fast_sum = 1;
            
            /*设置间隔延时，缓解总线压力*/
            reals.Can_beta_block_send_timeout_Cnt = CAN_BLOCK_RX_TIME_OUT_SLOW;

	        can_poll->Cnt_bigcycle++;
            can_poll->fast_read = 0;
            can_poll->modbus_Rd_SlaveAddr = 0;
	        return 1;
	    }

        /*正常轮询策略*/
	    if (( can_poll->period > 0 ) 
            || ( 0 == can_poll->Cnt_bigcycle ) 
            || (1 == can_poll->fast_read))
	    {
            /*大周期切换，记录本轮开始时间*/
            if (0 == can_poll->run_flag) {
                can_poll->time_old = now_time;
            }
            
            can_poll->run_flag = 1;
	    }
	}

	if(1 == can_poll->run_flag) {
        /*存在高优先级发送任务可以挤占低优先级任务*/
        if ( 1 == can_poll->fast_read ) {
            for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
            {
                if((( 1 == reals.can_poll[i].fast_read)) // 高优先级任务需要立即读取
                    &&(reals.can_poll[i].range_level > can_poll->range_level) //高优先级直接抢占
                    &&(reals.can_poll[i].can_type != can_poll->can_type))
                {
                    return 0;
                }
            }
        } else {
            for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
            {
                if(((reals.can_poll[i].run_flag) || (reals.can_poll[i].fast_read)) // 高优先级任务正在读取或需要立即读取
                    &&(reals.can_poll[i].range_level > can_poll->range_level) //高优先级直接抢占
                    &&(reals.can_poll[i].can_type != can_poll->can_type))
                {
                    return 0;
                }
            }
        }
	} else {
		return 0;
	}

    /* 轮询所有节点数据,每次发送一帧 */
	do{
        /*混合类型地址偏移单独处理*/
        if ( CAN_DEV_MIX == can_poll->can_dev ) {
            uint8_t index = (can_poll->Cnt_smallcycle)%can_poll->max_y;
            if ( index < INV_MAX_NUM ) {
                x = 0;
                y = index;
                dev_type = CAN_DEV_INV;
            } else if ( index < (INV_MAX_NUM + PACK_MAX_NUM) ) {
                index = index - INV_MAX_NUM;
                x = (index/(PACK_INGROUP_MAX_NUM))%PACK_GROUP_NUM;
                y = (index)%PACK_INGROUP_MAX_NUM;
                dev_type = CAN_DEV_PACK;
            }
        } else {
            /*计算轮询设备地址偏移*/
            x = (can_poll->Cnt_smallcycle/(can_poll->max_y))%can_poll->max_x;
            y = (can_poll->Cnt_smallcycle)%can_poll->max_y;
            dev_type = can_poll->can_dev;
        }
    
		/*检查当前设备是否在线*/
		if (((CAN_DEV_INV == dev_type)&&(can_node_rd.Inv[y].online))
#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE            
		||((CAN_DEV_PACK == dev_type)&&(can_node_rd.Pack[x * can_poll->max_y].pack_announce.online & ((uint16_t)1 << y)))
#endif		
#ifdef CONFIG_FUNC_PAYGO_ENABLE            
        ||((CAN_DEV_PAYGO == dev_type)&&(can_node_rd.Pack[x * can_poll->max_y].online || get_bat_data_ptr()->bms_data[x * can_poll->max_y].valid))
#endif	
		)
		{
			if(CAN_DEV_INV == dev_type && (reals.If_ARM_SingleBoot != 1))
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, INV_CAN_ADDR + x*INV_MAX_NUM + y);
			}
			else if(CAN_DEV_PACK == dev_type)
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, PACK_CAN_ADDR + x*PACK_INGROUP_MAX_NUM + y);
			}
			else if(CAN_DEV_PAYGO == dev_type)
			{
				CanGetTarget(x, 0, can_poll->can_type, 0, 0, PACK_CAN_ADDR + x*PACK_INGROUP_MAX_NUM + y);
			}

            // 记录发送成功标志
            sendflag = 1;

            // 设置项回读后立即汇总
            if(1 == can_poll->fast_read) can_poll->fast_sum = 1;
            
            /*设置间隔延时，缓解总线压力*/
            reals.Can_beta_block_send_timeout_Cnt = CAN_BLOCK_RX_TIME_OUT_SLOW;
		}

        /*一个大循环执行完毕*/
		if(++can_poll->Cnt_smallcycle >= (can_poll->max_x*can_poll->max_y))
		{
			can_poll->Cnt_smallcycle = 0;
			can_poll->run_flag = 0;
            can_poll->fast_read = 0;

			if(++can_poll->Cnt_bigcycle >= 0xFF00) {
				can_poll->Cnt_bigcycle = 0xFF00;
			}
            
			if(!sendflag) return 2;
		}
	}
	while(!sendflag);//每次只发送一帧，未发送则继续轮询

	return 1;
}

/*------------------------------------------------------------------------------
 Function: can_data_poll_all
 -----------------------------------------------------------------------------*/
/**
  * @brief      周期轮询状态机
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
static void can_data_poll_all(void)
{
    /*存在发送间隔延时或上位机设置透传，暂停轮询*/
    if((0 != reals.Can_beta_block_send_timeout_Cnt)||(reals.BLE_WIFI_to_Can_Cnt)) return;

#if 1

    static uint8_t index = 0;
    uint8_t ret = 0;

    /*一次仅发送一个查询报文，遵守一问一答原则*/
    for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
    {
        /*Can Beta多字节查询(快速短周期部分，每次仅发送一条)*/
        ret = can_data_poll_common_step(&reals.can_poll[index]);

        /*从上次退出的地方轮询，避免因为部分TYPE轮询间隔太短导致其他TYPE无法轮循到*/
        if( ++index >= MAX_NUM_CAN_TYPE ) {
            index = 0;
        }

        /*发送成功强制退出本轮*/
        if ( ret == 1 ) {
            break;
        }
    }

#else

    /*一次轮询所有TYPE（受时间间隔控制），每个TYPE只轮询一个设备*/
    for(uint8_t i = 0; i < MAX_NUM_CAN_TYPE; i++)
    {
        /*Can Beta多字节查询(快速短周期部分，每次仅发送一条)*/
        can_data_poll_common_step(&reals.can_poll[i]);
    }

#endif
}


/*------------------------------------------------------------------------------
 Function: can_enable_active_report
 -----------------------------------------------------------------------------*/
/**
  * @brief      开启 INV/bat主动上报使能
  * @param[in]  uint8_t node_id  
  * @param[out] None
  * @return     static void
  */
static void can_enable_active_report(uint8_t node_id)
{
    #if 0
	can_reply_data_t *reply_data;

	/* 查询逆变是否开启主动上报 */
	reply_data = can_overall_ctrl_get_reply(node_id, CAN_REPLY_CMD_EN_INV_REPORT);
	if(reply_data == NULL) return;

	if(reply_data->en_inv_report_reply.respond == 0)
	{
	    ESP_LOGW(TAG,"can enable inv active report");
		can_ctrl_t can_ctrl = {0};
		can_ctrl.cmd = CAN_CTRL_CMD_EN_INV_REPORT;
		can_overall_ctrl(node_id, &can_ctrl);	// 打开逆变主动上报
	}
    
    /* 查询pack是否开启主动上报 */
	reply_data = can_overall_ctrl_get_reply(node_id, CAN_REPLY_CMD_EN_PACK_REPORT);
	if(reply_data == NULL) return;

	if(reply_data->en_pack_report_reply.respond == 0)
	{
		can_ctrl_t can_ctrl = {0};
		can_ctrl.cmd = CAN_CTRL_CMD_EN_PACK_REPORT;
		can_overall_ctrl(node_id, &can_ctrl);	// 打开pack主动上报
	}
    #else
    
    /* 强制周期性的下发该帧到逆变器 */
    can_ctrl_t can_ctrl = {0};
    can_ctrl.cmd = CAN_CTRL_CMD_EN_PACK_REPORT;
    can_overall_ctrl(node_id, &can_ctrl);
    
    can_ctrl.cmd = CAN_CTRL_CMD_EN_INV_REPORT;
    can_overall_ctrl(node_id, &can_ctrl);

    #endif
}


/**
 * @brief 轮询设备数据
 * - 设备主要分为被锁定和未被锁定两种状
 * - 1、被锁定是指被手机APP指定读取的设
 * - 2、未锁定是指除锁定以外别的设
 * - 3、设备分为PACK和逆变
 * - 4、CAN总线处于OTA期间,不发起查
 *
 * @param pvParameters 线程参数
 *
 * @return 
 */
void iot_polldev_task(void)
{
	static uint8_t step=0;
	static uint8_t sCnt=0;

	if(0 == step)
	{
		step = 1;

        /* 初始化多字节查询的 所有报文时序、周期、优先级 */
		can_data_poll_init(); 

		/* IoT启动后发送指令开启一次逆变与pack数据上报 */
		for (uint8_t node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
		{
			can_enable_active_report(node_id);
            ESP_LOGW(TAG, "[iot_polldev_task] Inv and pack report enable!");
		}

        /*发送EMS控制帧，同步休眠设置*/
        Iot_EMS_Ctrl_Func_5_Send(0, 0, 0);
	}
	else if(1 == step)
	{
	    /*延时等待1s，不使用delay*/
		if(++sCnt >= 100)    // 当前调用周期10ms
		{
			step = 2;
		}
	}
	else if(2 == step)
    {
        /* 根据 OTA 状态判断是否跳过查询并保持总线静默 */
        if ((can_ota_status.period_cmd_delay > 0) 
            || bms_ota_is_doing() 
            || can_ota_is_doing() 
#ifdef CONFIG_CAN_OTA_READY_TIMEOUT                
            || (can_ota_status.ready_timeout > 0) 
            || (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.firmware_transfer == 1)
#endif            
            ) {
      
            /* 若处于 ready_timeout 或 正在进行固件透传（firmware_transfer）*/
            if ((can_ota_status.ready_timeout > 0 && can_ota_status.ready_timeout < UINT16_MAX) ||
                (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.firmware_transfer == 1)) {
#ifdef CONFIG_CAN_OTA_READY_TIMEOUT   
                /* 保持总线静默 */
                Iot_can_ota_ready_period();
#else
                /*总线无需静默，保持心跳*/            
                Iot_Heartbeat_frame_Send(false); 
#endif
            }

            /* OTA 正在进行或需要静默，跳过后续查询 */
            return;
        }

        /*APP设置转发（BLE/WIFI-> CAN），CAN升级时暂不转发 */
        iot_to_can_cmd_task();

        /*设备广播帧检查发送*/
        iot_can_bind_send_and_check();

#ifdef CONFIG_SYSTEM_SLEEP_CAN_SILENT_ENABLE
        /*休眠期间暂停轮询*/
        if(1 == reals.IOT_Status_Flag.sBit.system_sleep_flag) return;
#endif

        /*多个IOT并机，只允许主IOT发查询报文，其他IOT只监听*/
		if((1 == reals.Addr_can_master)
			&&((reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_FINISH) 
				|| (reals.Step_can_dev_parallel == STEP_CAN_PARALLEL_AFTER)))
		{
#ifdef CAN_MASTER_POLL_ENABLE
		    /*周期轮询状态机*/
			can_data_poll_all();
#endif				
            /*时间检查，CAN总线时间有误时同步（1s）*/
            RTC_Update_iot_to_can();

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
            if( reals.online_Dcdc_num > 0 ) {
                /*DCDC控制帧*/
                Iot_Dcdc_Ctrl_frame_Send(false);
            }
#endif
		}
    }
}

/*------------------------------------------------------------------------------
 Function: IoT_Status_Parameter_update
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT状态广播至总线
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void IoT_Status_Parameter_update(uint16_t offset, uint16_t len)
{
    /* 为can指令申请内存 */
    can_cmd_queue_struct can_cmd = {NULL, 1, 0};
    uint8_t can_cmd_flag = 0;
    if (can_cmd_queue) 
    {
        can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
        if (!can_cmd.cmd) {
            ESP_LOGE (TAG, "IoT_Status_Parameter_update malloc failed");
            return;
        }
    }
    else
    {
        ESP_LOGE (TAG, "IoT_Status_Parameter_update null!");
        return;
    }
    
    /*配置发送设置*/
    can_cmd.md_addr = 0;//广播
    can_cmd.num = 1;
    
    /*配置发送数据格式地址*/
    can_cmd.cmd->type = IOT_TYPE_INFO_01H;
    can_cmd.cmd->active_can_cmd_type = 0; //tbd
    can_cmd.cmd->len = len;
    can_cmd.cmd->offset = offset;

    /*同步本地参数至CAN*/
    memcpy( &can_node_rd.Iot[reals.Addr_can_self].mod_reg11000_IOT_info, 
            &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info, 
            (sizeof(iot_can_node_struct_reg11000) - sizeof(rw_cmd_struct)));
    
    /* 发送到队列 */
    if (can_cmd.cmd != NULL && can_cmd.num != 0) 
    {
        if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(100)) == pdTRUE) 
        {
            can_cmd_flag = 1;
            reals.BLE_WIFI_to_Can_Cnt++;
            ESP_LOGI(TAG, "Sending IoT Status(offset: %d, len: %d)", can_cmd.cmd->offset, can_cmd.cmd->len);
        }
    }
    
    /*未发送*/
    if (!can_cmd_flag && can_cmd.cmd) 
    {
        free(can_cmd.cmd);
        ESP_LOGE (TAG, "IoT_Status_Parameter_update Error!");
    }       
}

/*------------------------------------------------------------------------------
 Function: CanListenBus
 -----------------------------------------------------------------------------*/
/**
  * @brief      单帧接收解析 
                接收并解析下层设备周期广播的数据 
  * @param[in]  node_info_t *node      
                twai_message_t *frame  
  * @param[out] None
  * @return     static void
  */
static void CanListenBus(node_info_t *node, twai_message_t *frame) 
{
    IdStruct canId = {0};
    uint8_t index = 0;
	uint16_t pf_fun = 0;
	uint8_t Id_p = 0;
	uint8_t Id_edp = 0;
	uint8_t Id_dp = 0;
	
	uint8_t addr_source = 0;

    if (!frame) return ;

    uint32_t offset;
    uint8_t *pdata;

    canId.all = frame->identifier;  //can rx ID
	pf_fun =(canId.all>>16)&0xFF;   //功能码
	Id_p = (canId.all>>26)&0x7;     //优先级
	Id_edp = (canId.all>>25)&0x1;   //扩展数据页
	Id_dp = (canId.all>>24)&0x1;    //数据页
	addr_source =canId.all&0xFF;    //来源地址

	OtaBoot *payload = (OtaBoot *)frame->data;  //数据
	
    switch (canId.bit.funcode)
    {
        /* CAN-OTA数据*/
        case 0x70: /* CAN-OTA准备设备进入静默 */
        case 0x71: /* CAN-OTA开始帧 */
        case 0x72: /* CAN-OTA开始响*/
        case 0x73: /* CAN-OTA数据开*/
        case 0x74: /* CAN-OTA数据 */
        case 0x75: /* CAN-OTA数据响应 */
        case 0x76: /* CAN-OTA结束*/
		case 0x77: /* CAN-OTA结束,应答 */
			can_ota_status.period_cmd_delay = CAN_OTA_PERIOD_TIMEOUT;
            break; 

        case 0x78: /* 底层设备进行估计安装时发送的进度 */
            can_ota_status.period_cmd_delay = CAN_OTA_PERIOD_TIMEOUT;
#if 0        
			OtaPct *dev_pct = (OtaPct *)frame->data;
			ota_can_pct(can_ota_status.devStatus,
                        canId.bit.src,
						dev_pct->type,
						dev_pct->pct,
						dev_pct->where,
						dev_pct->errcode);
            ESP_LOGW(TAG, "ota progress frame, node: %d, devId: 0x%x, pct:%d where: %d, errcode: %d",
					node->node_id, canId.bit.src, dev_pct->pct, dev_pct->where, dev_pct->errcode);            
#endif            
            break;

        case 0x79: /* 底层设备进入停留在BOOT,请求上层升级 */

            // 升级期间暂不解析
            if ( sys_is_updating() 
                || (can_ota_status.ready_timeout > 0) ) {
                break;
            }
                
            if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src < (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))
            {
				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
				ESP_LOGW(__func__, "ARM boot, payload->fileType: %u, payload->version:%lu",payload->fileType,payload->version);

                if(!reals.If_ARM_SingleBoot) (*node->invs_info)[index].inv_about.software_total = 1;
				reals.If_ARM_SingleBoot = 1;//底层ARM若处于单boot状态，版本号从0x79获取，并立即执行一次type=13的轮询获取设备类型及SN
				(*node->invs_info)[index].inv_about.soft[0].type   = payload->fileType;
				(*node->invs_info)[index].inv_about.soft[0].version = payload->version;
                (*node->invs_info)[index].online = 1;
                (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;

				can_node_rd.Inv[index].inv_announce.online = 1;//收到0x79 boot，表示设备在线，此值给xmodem升级使用
                top_modbus_rd.Inv[index].mod_reg01100_Inv_base.software_total = (*node->invs_info)[index].inv_about.software_total;
                top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[0].type = (*node->invs_info)[index].inv_about.soft[0].type;
                top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[0].version = (*node->invs_info)[index].inv_about.soft[0].version;
                
				// CanGetTarget(0, 0, INV_TYPE_ABOUT_13H, 0, 0, canId.bit.src); // ARM boot数据发送有问题不使用
            }
            else if ((canId.bit.src >= PACK_CAN_ADDR) && (canId.bit.src < (PACK_CAN_ADDR + (PACK_MAX_NUM*DEV_MAIN_NODE_MAX))))
            {
				index = (canId.bit.src - PACK_CAN_ADDR)%PACK_INGROUP_MAX_NUM;

                (*node->packs_info)[index].pack_about.software_total = 1;
				(*node->packs_info)[index].pack_about.soft[0].type   = payload->fileType;
				(*node->packs_info)[index].pack_about.soft[0].version = payload->version;
				memcpy((uint8_t *)&top_modbus_rd.Pack[index].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,software_total), (uint8_t *)& (*node->packs_info)[index].pack_about + offsetof(pack_about_struct,software_total),sizeof( (*node->packs_info)[index].pack_about.software_total));// 6173
				// 6174~6203
				for(int i = 0;i < 1;i++)
				{
					memcpy((uint8_t *)&top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].type, (uint8_t *)& (*node->packs_info)[index].pack_about.soft[i].type,sizeof( (*node->packs_info)[index].pack_about.soft[i].type));
					memcpy((uint8_t *)&top_modbus_rd.Pack[index].mod_reg06100_Pack_each.soft[i].version, (uint8_t *)& (*node->packs_info)[index].pack_about.soft[i].version,sizeof( (*node->packs_info)[index].pack_about.soft[i].version));
				}

                (*node->packs_info)[index].online = 1;
                (*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;
				(*node->packs_info)[index].pack_announce.online = 1;
				if(canId.bit.src == PACK_CAN_ADDR) // 主包boot不支持0x54获取类型和SN
				{
					// 主包填充IOT型号和SN
					strncpy((*node->packs_info)[index].pack_about.type_ascii, dev_factory.dev_type, 12);
					(*node->packs_info)[index].pack_about.sn_code = dev_factory.dev_sn; 
					memcpy((uint8_t *)&top_modbus_rd.Pack[index].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,type_ascii), (uint8_t *)&dev_factory.dev_type, sizeof(dev_factory.dev_type));// 6101~6106
					top_modbus_rd.Pack[index].mod_reg06100_Pack_each.sn_code = dev_factory.dev_sn; // 6107~6110
				} 
				else 
				{
					CanGetTarget(0, 0, PACK_TYPE_ABOUT_54H, 0, 0, canId.bit.src); // 获取pack类型及SN
					memcpy((uint8_t *)&top_modbus_rd.Pack[index].mod_reg06100_Pack_each + offsetof(MOD_STRUCT_reg06100,type_ascii), (uint8_t *)&can_node_rd.Pack[index].pack_about + offsetof(pack_about_struct,type_ascii),(offsetof(pack_about_struct,software_total) - offsetof(pack_about_struct,type_ascii)));// 6101~6110
				
				}
            } 
			else if ((canId.bit.src >= INV_CAN_DSP_ADDR) && (canId.bit.src < (INV_CAN_DSP_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX)))) 
			{
			    if ( 1 == reals.If_ARM_SingleBoot )
			    {
                    index = (canId.bit.src - INV_CAN_DSP_ADDR)%INV_MAX_NUM;
                    ESP_LOGW(__func__, "DSP boot, payload->fileType: %u, payload->version:%lu",payload->fileType,payload->version);

                    if((*node->invs_info)[index].inv_about.software_total < 2) (*node->invs_info)[index].inv_about.software_total = 2;
                    (*node->invs_info)[index].inv_about.soft[1].type   = payload->fileType;
                    (*node->invs_info)[index].inv_about.soft[1].version = payload->version;
                    (*node->invs_info)[index].online = 1;
                    (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
                    
                    can_node_rd.Inv[index].inv_announce.online = 1;//收到0x79 boot，表示设备在线，此值给xmodem升级使用
                    top_modbus_rd.Inv[index].mod_reg01100_Inv_base.software_total = (*node->invs_info)[index].inv_about.software_total;
                    top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[1].type = (*node->invs_info)[index].inv_about.soft[1].type;
                    top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[1].version = (*node->invs_info)[index].inv_about.soft[1].version;
			    }
			}
#ifdef COMMON_INV_TI137_CAN_ADDR                    
            else if ((canId.bit.src >= COMMON_INV_TI137_CAN_ADDR) && (canId.bit.src < (COMMON_INV_TI137_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX)))) 
            {
                if ( 1 == reals.If_ARM_SingleBoot )
                {
                    index = (canId.bit.src - COMMON_INV_TI137_CAN_ADDR)%INV_MAX_NUM;
                    ESP_LOGW(__func__, "PV boot, payload->fileType: %u, payload->version:%lu",payload->fileType,payload->version);
            
                    if((*node->invs_info)[index].inv_about.software_total < 3) (*node->invs_info)[index].inv_about.software_total = 3;
                    (*node->invs_info)[index].inv_about.soft[2].type   = payload->fileType;
                    (*node->invs_info)[index].inv_about.soft[2].version = payload->version;
                    (*node->invs_info)[index].online = 1;
                    (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
                    
                    can_node_rd.Inv[index].inv_announce.online = 1;//收到0x79 boot，表示设备在线，此值给xmodem升级使用
                    top_modbus_rd.Inv[index].mod_reg01100_Inv_base.software_total = (*node->invs_info)[index].inv_about.software_total;
                    top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[2].type = (*node->invs_info)[index].inv_about.soft[2].type;
                    top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[2].version = (*node->invs_info)[index].inv_about.soft[2].version;
                }
            }
#endif
            break;
            
        /* CAN-逆变周期上报数据帧 0x1810XXYY~0x1815XXYY*/
        case INV_ANNOUNCE1:		
        case INV_ANNOUNCE2:
        case INV_ANNOUNCE3:
        case INV_ANNOUNCE4:
        case INV_ANNOUNCE5:
		case INV_ANNOUNCE6:
            if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src < (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
            {
				reals.If_ARM_SingleBoot = 0;
    			index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
                if (index < INV_MAX_NUM) 
    			{
                    if (canId.bit.page == 0)
    			    {
                        offset = canId.bit.funcode - INV_ANNOUNCE1;
                        pdata = ((uint8_t *)&(*node->invs_info)[index].inv_announce) + (offset * 8);
                        memcpy(pdata, frame->data, frame->data_length_code);
                        (*node->invs_info)[index].online = 1;
                        (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
    
                        (*node->invs_info)[index].inv_announce.online = 1;
    
#if CONFIG_RTC_TIME_SOURCE_CHANGE == 1 || CONFIG_RTC_TIME_SOURCE_CHANGE == 3
                        if ( canId.bit.funcode == INV_ANNOUNCE2) {
                            /*时间校验*/
                            RTC_Valid_Check(index);
                        }
#endif 
    			    }  
                    else if (canId.bit.page == 1)
                    {
                        switch ( canId.bit.funcode )
                        {
                            case INV_ANNOUNCE5 :
                                top_modbus_rd.Inv[index].mod_reg00100_AppPage1.AC_Input_Status.status = frame->data[0] & 0x0F;
                                top_modbus_rd.Inv[index].mod_reg00100_AppPage1.AC_Input_Status.progress = (frame->data[0] >> 4) & 0x0F;
                                top_modbus_rd.Inv[index].mod_reg00100_AppPage1.AC_Input_Status.fault_state = frame->data[1] & 0x0F;
                                (*node->invs_info)[index].online = 1;
                                (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
                                (*node->invs_info)[index].inv_announce.online = 1;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            
			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
            break;
            
        /* CAN-逆变周期上报数据帧 新增 0x1901FFYY~0x1964FFYY*/
		case INV_ANNOUNCE7:
		case INV_ANNOUNCE8:
		case INV_ANNOUNCE9:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
			if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src <= (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
			{
				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
				if (index < INV_MAX_NUM) {
					offset = canId.bit.funcode - INV_ANNOUNCE7;
					pdata = (uint8_t *)&(*node->invs_info)[index].inv_announce.l1_chg_limit + (offset * 8);
					memcpy(pdata, frame->data, frame->data_length_code);
					(*node->invs_info)[index].online = 1;
					(*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
				}
			}
#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
			else if (((addr_source >= DCDC_ARM_CAN_ADDR) && (addr_source < (DCDC_ARM_CAN_ADDR + DCDC_MAX_NUM))) 
                        || ((addr_source >= DCDC_IOT_CAN_ADDR) && (addr_source < (DCDC_IOT_CAN_ADDR + DCDC_MAX_NUM))))
			{
				if (canId.bit.page != 1) 
					break; // 该处的功能码属于第一页数据

                // 不区分该数据来源于DCDC的哪个MCU，均作处理，以便兼容D400S和CHARGE2
			    if((addr_source >= DCDC_ARM_CAN_ADDR) && (addr_source < (DCDC_ARM_CAN_ADDR + DCDC_MAX_NUM))) {
				    index = (canId.bit.src - DCDC_ARM_CAN_ADDR)%DCDC_MAX_NUM;
                } else {
				    index = (canId.bit.src - DCDC_IOT_CAN_ADDR)%DCDC_MAX_NUM;
                }
                
				if (index < DCDC_MAX_NUM) {
					offset = canId.bit.funcode - INV_ANNOUNCE7;

                    // 参考文档：“D400S&Charger2与AP300通讯协议（DCDC和逆变器设备的直流耦合协议）-20260114.docx”
					switch(offset)
					{
						case 0: // 0x1901FFXX
                            switch ( frame->data[0] )
                            {
                                // 数据包编号
                                case 1 :
                                    if ( STR_EQ_EXACT((*node->dcdc_s_info)[index].dcdc_common_info.dev_type, SN_TYPE_SOLARX4K_ASCII)) {
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[2].dc_power = (uint16_t)((frame->data[1])|(frame->data[2]<<8));
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[2].dc_voltage = (uint16_t)((frame->data[3])|(frame->data[4]<<8));
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[0].dc_voltage = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    } else if ( STR_EQ_EXACT((*node->dcdc_s_info)[index].dcdc_common_info.dev_type, SN_TYPE_CHARGER2_ASCII)) {
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[0].dc_power = (uint16_t)((frame->data[1])|(frame->data[2]<<8));
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[0].dc_voltage = (uint16_t)((frame->data[3])|(frame->data[4]<<8));
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[1].dc_power = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    }

                                    // 保存高字节，更新低字节
                                    (*node->dcdc_s_info)[index].dcdc_common_info.ctrl_mode.all = ((*node->dcdc_s_info)[index].dcdc_common_info.ctrl_mode.all & 0xFF00) | frame->data[7];

                                    // 光伏使能状态
                                    if((*node->dcdc_s_info)[index].dcdc_common_info.ctrl_mode.bit.pv_enable){
                                        (*node->dcdc_s_info)[index].dcdc_common_set.charger_set.bit.dc_out_onoff = 1;
                                    } else {
                                        (*node->dcdc_s_info)[index].dcdc_common_set.charger_set.bit.dc_out_onoff = 2;
                                    }
                                    break;
                                    
                                case 2 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.dev_type[0];
					                memcpy(pdata, &frame->data[1], 6);
                                    break;
                                    
                                case 3 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.dev_type[6];
                                    memcpy(pdata, &frame->data[1], 6);
                                    // 只更新最低字节
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dev_sn = ( (*node->dcdc_s_info)[index].dcdc_common_info.dev_sn & ~0xFFULL ) | frame->data[7];
                                    break;
                                    
                                case 4 :
                                    // 更新 dev_sn 的高7字节
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.dev_sn + 1;
                                    memcpy(pdata, &frame->data[1], 7);
                                    break;
                                        
                                case 5 :
                                    (*node->dcdc_s_info)[index].dcdc_common_info.energy_line.all = (uint16_t)((frame->data[1])|(frame->data[2]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.fault_dcdc = (uint16_t)((frame->data[3])|(frame->data[4]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.proctect_dcdc = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_set.mode4_set.bit.long_short_line = frame->data[7] & 0x03;
                                    break;

                                case 6 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.energy_info[2].dc_energy_output;
                                    memcpy(pdata, &frame->data[1], 4);
                                    if ( STR_EQ_EXACT((*node->dcdc_s_info)[index].dcdc_common_info.dev_type, SN_TYPE_SOLARX4K_ASCII)) {
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[0].dc_power = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    }
                                    break;

                                case 7 :
                                    if ( DEVICE_IOT == frame->data[1] ) {
                                        pdata = (uint8_t *)&(*node->dcdc_s_info)[index].iot_info.software_ver;
                                    } else {
                                        (*node->dcdc_s_info)[index].dcdc_common_info.dcdc_SoftwareType = frame->data[1];
                                        pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.dcdc_SoftwareVersion;
                                    }
                                    memcpy(pdata, &frame->data[2], 4);
                                    break;

                                case 8 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.energy_info[0].dc_energy_output;
                                    memcpy(pdata, &frame->data[1], 4);
                                    (*node->dcdc_s_info)[index].dcdc_common_info.fault_dc1 = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    break;
                                    
                                case 9 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.energy_info[1].dc_energy_output;
                                    memcpy(pdata, &frame->data[1], 4);
                                    (*node->dcdc_s_info)[index].dcdc_common_info.fault_dc3 = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    break;
                                        
                                case 10 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.energy_info[3].dc_energy_output;
                                    memcpy(pdata, &frame->data[1], 4);
                                    break;
                                        
                                case 11 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.energy_info[4].dc_energy_output;
                                    memcpy(pdata, &frame->data[1], 4);
                                    break;
                                        
                                case 12 :
                                    pdata = (uint8_t *)&(*node->dcdc_s_info)[index].dcdc_common_info.energy_info[5].dc_energy_output;
                                    memcpy(pdata, &frame->data[1], 4);
                                    break;
                                        
                                case 13 :
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[1].dc_power = (uint16_t)((frame->data[1])|(frame->data[2]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[1].dc_voltage = (uint16_t)((frame->data[3])|(frame->data[4]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.fault_dc2 = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    break;
                                        
                                case 14 :
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[3].dc_power = (uint16_t)((frame->data[1])|(frame->data[2]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[3].dc_voltage = (uint16_t)((frame->data[3])|(frame->data[4]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.fault_dc4 = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    break;
                                        
                                case 15 :
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[4].dc_power = (uint16_t)((frame->data[1])|(frame->data[2]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[4].dc_voltage = (uint16_t)((frame->data[3])|(frame->data[4]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.fault_dc5 = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    break;
                                        
                                case 16 :
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[5].dc_power = (uint16_t)((frame->data[1])|(frame->data[2]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.dc_info[5].dc_voltage = (uint16_t)((frame->data[3])|(frame->data[4]<<8));
                                    (*node->dcdc_s_info)[index].dcdc_common_info.fault_dc6 = (uint16_t)((frame->data[5])|(frame->data[6]<<8));
                                    break;
                                    
                                case 25:
                                    (*node->dcdc_s_info)[index].dcdc_common_set.dc_val_set[2].dc_current_set = (uint16_t)((frame->data[4])|(frame->data[5]<<8));
                                    break;
                                    
                                default:
                                    // 其他命令码暂未支持，直接轮询获取数据
                                    pdata = NULL;
                                    break;
                            }

							break;

						default:
							pdata = NULL;
							break;
					}

					(*node->dcdc_s_info)[index].online = 1;
					(*node->dcdc_s_info)[index].alive_time = DCDC_OFFLINE_TIME;
				}
			}
#endif

			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
			break;

        case INV_ANNOUNCE10:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
			if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src <= (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
			{
				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
				if (index < INV_MAX_NUM) {

                    // DataCode索引值
                    switch ( frame->data[0] )
                    {
                        case 0 :
                            pdata = (uint8_t *)&(*node->invs_info)[index].inv_announce.system_all_status;
                            memcpy(pdata, frame->data, frame->data_length_code);
                            break;

                        case 75 :
                            uint8_t pv_index = frame->data[1];
                            if ( pv_index < 3 ) {
                                (*node->invs_info)[index].inv_announce.Pv_Chg_Power[pv_index] = (uint16_t)((frame->data[2])|(frame->data[3]<<8));
                            }
                            break;
                        default:
                            break;
                    }

					(*node->invs_info)[index].online = 1;
					(*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
				}
			}

			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
			break;

        case INV_ANNOUNCE11:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
            if (reals.If_ARM_SingleBoot == 1) {
            	if(((canId.bit.src >= INV_CAN_DSP_ADDR) && (canId.bit.src <= (INV_CAN_DSP_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
                    && (frame->data[0] == 20)) // 特殊帧，当ARM在boot时，DSP固件需要从私有协议获取DSP固件版本号
            	{
                    index = (canId.bit.src - INV_CAN_DSP_ADDR)%INV_MAX_NUM;
                    if (index < INV_MAX_NUM) {
                		uint32_t dsp_version = (uint32_t)(frame->data[5]<<24)|(uint32_t)(frame->data[4]<<16)|(uint32_t)(frame->data[3]<<8)|(uint32_t)(frame->data[2]);
                		ESP_LOGW(__func__, "ARM boot, DSP payload->fileType: %u, payload->version:%lu", DEVICE_DSP, dsp_version);

                        if((*node->invs_info)[index].inv_about.software_total < 2) (*node->invs_info)[index].inv_about.software_total = 2;
                		(*node->invs_info)[index].online = 1;
                		(*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
                		(*node->invs_info)[index].inv_announce.online = 1;//收到0x79 boot，表示设备在线，此值给xmodem升级使用	
                		(*node->invs_info)[index].inv_about.soft[1].type   = DEVICE_DSP;
                		(*node->invs_info)[index].inv_about.soft[1].version = dsp_version;

                        top_modbus_rd.Inv[index].mod_reg01100_Inv_base.software_total = (*node->invs_info)[index].inv_about.software_total;
                        top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[1].type = (*node->invs_info)[index].inv_about.soft[1].type;
                        top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[1].version = (*node->invs_info)[index].inv_about.soft[1].version;
                    }
            	}
#ifdef COMMON_INV_TI137_CAN_ADDR                    
                else if(((canId.bit.src >= COMMON_INV_TI137_CAN_ADDR) && (canId.bit.src <= (COMMON_INV_TI137_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
                    && (frame->data[0] == 95)) // 特殊帧，当ARM在boot时，PV固件需要从私有协议获取PV固件版本号
                {
                    index = (canId.bit.src - COMMON_INV_TI137_CAN_ADDR)%INV_MAX_NUM;
                    if (index < INV_MAX_NUM) {
                        uint32_t pv_version = (uint32_t)(frame->data[5]<<24)|(uint32_t)(frame->data[4]<<16)|(uint32_t)(frame->data[3]<<8)|(uint32_t)(frame->data[2]);
                        ESP_LOGW(__func__, "ARM boot, PV payload->fileType: %u, payload->version:%lu", DEVICE_PACK_M1, pv_version);
                
                        if((*node->invs_info)[index].inv_about.software_total < 3) (*node->invs_info)[index].inv_about.software_total = 3;
                        (*node->invs_info)[index].online = 1;
                        (*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
                        (*node->invs_info)[index].inv_announce.online = 1;//收到0x79 boot，表示设备在线，此值给xmodem升级使用  
                        (*node->invs_info)[index].inv_about.soft[2].type   = DEVICE_PACK_M1;
                        (*node->invs_info)[index].inv_about.soft[2].version = pv_version;

                        top_modbus_rd.Inv[index].mod_reg01100_Inv_base.software_total = (*node->invs_info)[index].inv_about.software_total;
                        top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[2].type = (*node->invs_info)[index].inv_about.soft[2].type;
                        top_modbus_rd.Inv[index].mod_reg01100_Inv_base.soft[2].version = (*node->invs_info)[index].inv_about.soft[2].version;
                    }
                }
#endif                    
            }
        
            /* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
                * 因为其它功能模块可能不识别该帧而回复错误响应 */
            frame->identifier = 0;
            break;

        case DCHUB_ANNOUNCE1:		
        case DCHUB_ANNOUNCE2:
        case DCHUB_ANNOUNCE3:
        case DCHUB_ANNOUNCE4:
        case DCHUB_ANNOUNCE5:
		case DCHUB_ANNOUNCE6:
        case DCHUB_ANNOUNCE7:
        case DCHUB_ANNOUNCE8:
        case DCHUB_ANNOUNCE9:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE            
            if ((canId.bit.src >= DC_HUB_CAN_ADDR) 
                && (canId.bit.src <= (DC_HUB_CAN_ADDR + (DC_HUB_MAX_NUM*DEV_MAIN_NODE_MAX)))
             )
            {
                index = (canId.bit.src - DC_HUB_CAN_ADDR)%DC_HUB_MAX_NUM;
                if (index < DC_HUB_MAX_NUM) 
                {
                    // 参考文档：“AC380ARM_DC HUB扩展CAN协议1.5”
					offset = canId.bit.funcode - DCHUB_ANNOUNCE1;
					pdata = (uint8_t *)&(*node->dchubs_info)[index].dc_hub_info + (offset * 8);
					memcpy(pdata, frame->data, frame->data_length_code);
					(*node->dchubs_info)[index].online = 1;
					(*node->dchubs_info)[index].alive_time = DC_HUB_OFFLINE_TIME;
                }
            }
#endif            
            /* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
                * 因为其它功能模块可能不识别该帧而回复错误响应 */
            frame->identifier = 0;
            break;
        
        /* CAN-上级(IOT)综合控制帧*/
		case INV_ANNOUNCE_0xFA:
		    if ((0xFA == pf_fun)//IOT主动上报new
				&&(2 == Id_p)&&(0 == Id_edp)&&(0 == Id_dp))
			{
				if((99 == frame->data[0])
					&&(11 == frame->data[1]))//11-群组主机上报本群组SN
				{
				    if ((addr_source >= INV_IOT_CAN_ADDR) 
				        && (addr_source < (INV_IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX)))
				    {
                        Device_Can_Address_Parallel_Bind(frame);

                        /* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
                            * 因为其它功能模块可能不识别该帧而回复错误响应 */
                        frame->identifier = 0;
				    }
				}
                else if(3 == frame->data[0])
                {
                    if (canId.bit.dst == esp_canbus_myself_address() 
                        || canId.bit.dst == CAN_BROADCAST_ADDRESS ) 
                    {
                        // 生产测试命令
                        top_modbus_wr.Inv.mod_reg30900_test.test_work_mode = (uint16_t)((frame->data[6])|(frame->data[7]<<8));
                        
                        // 部分模式需要立即生效
                        iot_test_index_valid(top_modbus_wr.Inv.mod_reg30900_test.test_work_mode);

                        /* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
                            * 因为其它功能模块可能不识别该帧而回复错误响应 */
                        frame->identifier = 0;
                    }
                }
			}

			break;
            
        /* CAN-pack周期上报数据帧 0x189BFFYY~0x18A0FFYY*/
        case PACK_ANNOUNCE1:
        case PACK_ANNOUNCE2:
        case PACK_ANNOUNCE3:
        case PACK_ANNOUNCE4:
        case PACK_ANNOUNCE5:    
        case PACK_ANNOUNCE6:
            if ((canId.bit.src >= PACK_CAN_ADDR) && (canId.bit.src < (PACK_CAN_ADDR + PACK_MAX_NUM)))
            {
                index = (canId.bit.src - PACK_CAN_ADDR)%PACK_MAX_NUM; // 设备索引
                if (index < PACK_MAX_NUM) {
                    offset = canId.bit.funcode - PACK_ANNOUNCE1; //
                    pdata  = ((uint8_t *)&(*node->packs_info)[index].pack_announce) + (offset * 8);
                    memcpy(pdata, frame->data, frame->data_length_code);
                    (*node->packs_info)[index].online = 1;
                    (*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;
                }
            }
			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
            break;
            
        /* CAN-pack周期上报数据帧 新增 0x1965FFYY~0x19C8FFYY*/
		case PACK_ANNOUNCE7:
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
            if ((canId.bit.page == 1) 
                && (canId.bit.src == PACK_CAN_ADDR) 
                && (canId.bit.funcode == PACK_ANNOUNCE7)) {
                aging_status_struct1 aging_status = {0};
                aging_status.status = frame->data[0] & 0x0F;
                aging_status.progress = (frame->data[0] >> 4) & 0x0F;
                aging_status.fault_state = frame->data[3] & 0x0F;
                bms_alpha_aging_status_update(aging_status.all);
            }
#endif            
		case PACK_ANNOUNCE8:
		case PACK_ANNOUNCE9:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
            if ((canId.bit.src >= PACK_CAN_ADDR) && (canId.bit.src < (PACK_CAN_ADDR + PACK_MAX_NUM)))
            {
    			index = (uint8_t)(canId.bit.src - PACK_CAN_ADDR)%PACK_MAX_NUM;
                if (index < PACK_MAX_NUM) {
    				offset = canId.bit.funcode - PACK_ANNOUNCE7;
    				pdata = ((uint8_t *)&(*node->packs_info)[index].pack_announce.aging_status) + (offset * 8);
                    memcpy(pdata, frame->data, frame->data_length_code);
                    (*node->packs_info)[index].online = 1;
                    (*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;
                }
            }
            
			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
			break;
            
        case PACK_ANNOUNCE6D:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
            if ((canId.bit.src >= PACK_CAN_ADDR) && (canId.bit.src < (PACK_CAN_ADDR + PACK_MAX_NUM)))
            {
    			index = (uint8_t)(canId.bit.src - PACK_CAN_ADDR)%PACK_MAX_NUM;
                if (index < PACK_MAX_NUM) 
                {
                    switch ( frame->data[0] )
                    {
                        case 80 :
                            pdata = (uint8_t *)&(*node->packs_info)[index].pack_announce.pack_ver;
                            memcpy(pdata, frame->data, frame->data_length_code);
                            break;
                        case 81 :
                            pdata = (uint8_t *)&(*node->packs_info)[index].pack_announce.pack_flag;
                            memcpy(pdata, frame->data, frame->data_length_code);
                            break;
                        default:
                            break;
                    }
                    (*node->packs_info)[index].online = 1;
                    (*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;
                }
            }
            
			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
			break;
        
		case PACK_ANNOUNCE6E:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
			if ((canId.bit.src >= PACK_CAN_ADDR) && (canId.bit.src < (PACK_CAN_ADDR + PACK_MAX_NUM)))// 电池包
            {
    			index = (canId.bit.src - PACK_CAN_ADDR)%PACK_MAX_NUM;
                if (index < PACK_MAX_NUM) 
				{
				    switch ( frame->data[0] )
				    {
				        case 0 :
                            memcpy((uint8_t *)&(*node->packs_info)[index].pack_announce.pack_date, &frame->data[1], 6);
#if CONFIG_RTC_TIME_SOURCE_CHANGE == 2                    
                            /*时间校验（只取主包时间）*/
                            if(index == 0) {
                                RTC_Valid_Check(index);
                            }
#endif  
				            break;
				        default:
				            break;
				    }
                    
					(*node->packs_info)[index].online = 1;
					(*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;                  
				}
            }
            
			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
		    break;
            
		case PACK_ANNOUNCE6F:
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
			if ((canId.bit.src >= PACK_CAN_ADDR) && (canId.bit.src < (PACK_CAN_ADDR + PACK_MAX_NUM)))// 电池包
            {
    			index = (canId.bit.src - PACK_CAN_ADDR)%PACK_MAX_NUM;
                if (index < PACK_MAX_NUM) 
				{ 
				    switch ( frame->data[0] )
				    {
				        case 0 :
                            pdata = ((uint8_t *)&(*node->packs_info)[index].pack_announce.pack_set_status);
                            memcpy(pdata, frame->data, frame->data_length_code);
				            break;
				        default:
				            break;
				    }
                    
					(*node->packs_info)[index].online = 1;
					(*node->packs_info)[index].alive_time = PACK_OFFLINE_TIME;
				}
			}
            
			/* 本次CAN帧已处理,设置ID为0以避免其它功能模块再处理一次
				* 因为其它功能模块可能不识别该帧而回复错误响应 */
			frame->identifier = 0;
		    break;

        // 设备当日能量上报
        case ENERGY_ANNOUNCE_FUNC:    
            if (canId.bit.page != 1) 
                break; // 该处的功能码属于第一页数据
                
			if ((canId.bit.src >= INV_CAN_ADDR) && (canId.bit.src <= (INV_CAN_ADDR + (INV_MAX_NUM*DEV_MAIN_NODE_MAX))))// 逆变
			{
				index = (canId.bit.src - INV_CAN_ADDR)%INV_MAX_NUM;
				if (index < INV_MAX_NUM) {

                    // DataCode索引值
                    if(frame->data[0] < 40) {
                        pdata = (uint8_t *)&(*node->invs_info)[index].inv_today_energy[frame->data[0]];
                        memcpy(pdata, frame->data, frame->data_length_code);
                    }

					(*node->invs_info)[index].online = 1;
					(*node->invs_info)[index].alive_time = INV_OFFLINE_TIME;
				}
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
		&&(addr_source >= INV_IOT_CAN_ADDR) && (addr_source < (INV_IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX))
	)
	{
		*level1_addr = addr_source - INV_IOT_CAN_ADDR; // IOT		
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
void Device_Can_Address_Parallel_Bind(twai_message_t *frame) //100ms cycle
{
	
	if(1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND)
	{
		memcpy(IotSetData.dev_info_t.Sn_bind, &frame->data[2], 5);
		reals.SetDataWrFlag.sBit.bind_sn =1;
			
		top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.Bind_SN =((uint16_t)IotSetData.dev_info_t.Sn_bind[1]<<8)|IotSetData.dev_info_t.Sn_bind[0];

	
//			top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND=0;//改到写入flash后清零
	ESP_LOGI(TAG, "Device_Can_Address_Parallel_Bind  Bind_SN=%d",  top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.Bind_SN);//testwx

	}
}
/*------------------------------------------------------------------------
*@Function :  设置Step_can_dev_parallel值,设备重新排序  当前已弃用
*/
uint8_t Device_Can_Address_Parallel_SetTriger(uint16_t time) 
{
	uint8_t addrChageCount=0;
	if(reals.Step_can_dev_parallel!=STEP_CAN_PARALLEL_TRIGER&&(addrChageCount<3)
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
	
		if(1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_IOT)
		{
		
			//reals.Step_can_dev_parallel = STEP_CAN_GROUP_PARALLEL_TRIGER;
			top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_IOT=0;
			ESP_LOGI(TAG,"STEP_CAN_GROUP_PARALLEL_TRIGER	windy CCC");

		}
		else if(1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_INV)
		{
		
			top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_INV=0;

		}
		else if(1 == top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_PACK)
		{
		
			top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_PACK=0;

		}

}

/* 
单帧接收解析

接收并解析下层设备周期广播的数据

*/
static void CanListenBus_From_IOT(node_info_t *node, twai_message_t *frame) 
{
	// TODO
}


static void dump_err_can_frame(const twai_message_t *frame)
{
	uint32_t can_id = frame->identifier;
	can_id = CAN_ADDR_SWAP(can_id);
	ESP_LOGE(TAG, "received error can frame, id: %08lx, data:", can_id);
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, frame->data, frame->data_length_code, ESP_LOG_ERROR);
}

/* 
Can Beta多字节读取， 接收解析
IOT发送读取设备后,等待设备的应答 
*/
static void CanGetWaitResponse(node_info_t *node, const twai_message_t *frame) {
    IdStruct canId = {0} ;
    if (!frame) return ;

    canId.all = frame->identifier;

	/*当前CAN帧,不是发送给本机,不处理(TODO监听发送给其他IOT的轮询响应)*/
    if (canId.bit.dst != esp_canbus_myself_address() &&
        canId.bit.dst != CAN_BROADCAST_ADDRESS ) 
		// && (Can_ID_Non_native_IOT_Check(canId.bit.dst) != 0 )
	{
//        ESP_LOGE(TAG, "can id error:0x%08lx", canId.all);
		return ; // 当前CAN帧,不是发送给本机,不处理
    }
		
	if(0 != Can_ID_Check_Beta_Multi_Read(canId.all))
	{
		return; //
	}
	
    switch (canId.bit.funcode)
    {
        case READ_RESP_DATA_START:
        {
            CmdStruct *cmd = (CmdStruct *)frame->data;
            int err = CanVerifyCmd(node, canId.all, 1, canId.bit.src, cmd, NULL);  /* 检测数据是否在范围,如果正常，记录命*/
			if (err != ERR_NONE) {
				// ESP_LOGE(TAG, "CanGetWaitResponse start received error can frame, id: %08lx, error: %d, type: %d", canId.all, err, frame->data[0]);
			}
        }
        break;

        case READ_RESP_DATA:
        {
            DataStruct *data = (DataStruct *)frame->data;
            int err = CanVerifyData(node, canId.all, 1, canId.bit.src, data->type, data->seq, frame->data_length_code-FRAME_HEAD, data->bytes);
			if (err == ERR_TYPE) {
				// ESP_LOGE(TAG, "CanGetWaitResponse received error can frame, id: %08lx, error: %d, type: %d", canId.all, err, frame->data[0]);
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
    IdStruct canId = {0} ;

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

本地CAN数据被读
 * 参数：twai_message_t *frame, 接收到的CAN总线数据
 * 无返回
 */
static void CanLocalDataIsGet(node_info_t *node, const twai_message_t *frame) {
    uint16_t errCode = ERR_NONE;
    dev_data payload ;
    IdStruct TxcanId = {0} ;
    IdStruct RxcanId = {0};

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
	
    if (errCode != ERR_NONE)  {
        if (payload.pdata) {
            free(payload.pdata);
			payload.pdata = NULL;
        }
        TxcanId.bit.funcode = READ_RESP_DATA_START;
		
        CanAckStart(node->node_id, TxcanId.all, cmd->type, cmd->start, 0, errCode); // 数据区域,末尾为错误码
        return;
    }

    if (payload.pdata == NULL) {
        return;
    }

    TxcanId.bit.funcode = READ_RESP_DATA_START;
    uint16_t crc16 = calcu_crc16(payload.pdata, payload.len);    // crc16
    
    if (!CanAckStart(node->node_id, TxcanId.all, cmd->type, cmd->start, payload.len, crc16)) {
        free(payload.pdata);
		payload.pdata = NULL;
        return;
    }

    uint8_t sendLen;
    uint8_t seq = 0;
    uint8_t *ptr = payload.pdata;

    DataStruct canData;
    TxcanId.bit.funcode = READ_RESP_DATA;
    while(payload.len) // total send bytes
    {
        sendLen = (payload.len >= FRAME_DATA_BYTES) ? (FRAME_DATA_BYTES) : (payload.len);

        canData.type = cmd->type;
        canData.seq = seq;
        memcpy(canData.bytes, ptr, sendLen);
		
        if (CanAckData(node->node_id, TxcanId.all, (uint8_t *)&canData, (sendLen + FRAME_HEAD)))  {
            payload.len -= sendLen;
            ptr += sendLen;
            seq++;
        } else {
            ESP_LOGE (TAG, "Can Send Failed");
            break; // 发送数据失
        }
    }

    free(payload.pdata);
}


/* 
上位机CAN写入 我IOT，多字节

IOT设备的标定信息通过CAN写入 

*/

static void CanLocalDataIsSet(node_info_t *node, const twai_message_t *frame) 
{
	/* 未接收到CAN*/
	if (!frame) return ;

    uint16_t errCode = ERR_NONE;
    static CmdStruct cmd = {0};
    static uint8_t wait_data = 0;
    IdStruct TxcanId = {0};
    IdStruct RxcanId = {0};

    RxcanId.all = frame->identifier;
    TxcanId.all = RxcanId.all&0xFFFF0000;//截取ID高U16
	TxcanId.bit.src = esp_canbus_myself_address();
	TxcanId.bit.dst = RxcanId.bit.src;

    if (RxcanId.bit.dst != esp_canbus_myself_address() &&
        RxcanId.bit.dst != CAN_BROADCAST_ADDRESS ) {
            return ; // 当前CAN不是发送给本机,不处
    }
    // ESP_LOGI(TAG, "be setting canid: 0x%x", canId.all);

    switch (RxcanId.bit.funcode)
    {
        case WRIET_DATA_START: // Set Data Start
        {
            TxcanId.bit.funcode = WIRTE_DATA_ACK;
            cmd = *((CmdStruct *)frame->data);
            errCode = CanVerifyCmd(node, RxcanId.all, 1, RxcanId.bit.src, &cmd, NULL);
			// if (errCode == ERR_TYPE) {
			// 	dump_err_can_frame(frame);
			// }
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
			// if (errCode == ERR_TYPE) {
			// 	dump_err_can_frame(frame);
			// }
			if (errCode == ERR_NONE) {
                CanAckStart(node->node_id, TxcanId.all, cmd.type, cmd.start, cmd.total, 0);
                wait_data = 0;
            } else if (errCode != ERR_WAIT) {
                CanAckStart(node->node_id, TxcanId.all, cmd.type, cmd.start, 0, errCode);
                wait_data = 0;
            } else {
				// ESP_LOGE(TAG, "received unknown data:errCode=%d",errCode);
				// ESP_LOG_BUFFER_HEX(TAG, frame->data, frame->data_length_code);
			}
        }
        break;
		default:break;
    }
}

static inline uint8_t CanCheckSetStatus(void) {
    if (can_cmd_bk.write_cmd.status != COMPLETE) { // 操作未完
        return ERR_WAIT; // 需等待
    }
    return can_cmd_bk.write_cmd.err_code; // 操作完成,返回操作代码
}

/* 设置目标设备的数据

node_id:总线序号，AC380 not use

can beta,多字节写，自己做主tx

return:
0-fail
*/
uint8_t CanSetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, const uint8_t *pdata, uint16_t data_len, uint8_t dst_addr) {
    uint8_t seq = 0;
    uint16_t ucrc16;
    uint16_t total = data_len;
    IdStruct canId = {0};
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

node_id: CAN总线序号，EMS个；IOT/ATS只有一个，即忽略

*/
uint8_t CanGetTarget(uint8_t node_id, uint8_t page, uint8_t type, uint16_t start, uint16_t data_len, uint8_t dst_addr) 
{
    IdStruct canId = {0};
    if (data_len > MAX_BYTES) 
	{
        return 0;
    }

    canId.bit.page  = page;
    canId.bit.dst   = dst_addr;                	    // 目的地址
    canId.bit.src   = esp_canbus_myself_address();  // 源地址
    canId.bit.funcode  = READ_DATA_CMD;	            // 命令
    canId.bit.priority = LEVEL2;        	        // 传送优先级
	// ESP_LOGI(TAG, "CanGetTarget: id: 0x%lx, type: 0x%x, data_len: %d", canId.all, type, data_len);
    if (!CanAckStart(node_id, canId.all, type, start, data_len, 0)) {
        return 0;
    }

    return 1;
}

/* Response Start 
组帧发送，can beta
填充命令

node_id：CAN总线序号，not use

id:can tx ID
type:tx data[0]
start:tx data[1/2]
len:tx data[3/4]
crc16:tx data[5/6]

return:
0-fail，1-ok
*/
uint8_t CanAckStart(uint8_t node_id, uint32_t id, uint16_t type, uint16_t start, uint16_t len, uint16_t crc16) 
{

	/* 内部CAN发送开始指令*/
    can_queue_msg_struct queue_msg;
    if (!xCanBusQueue_Send) 
	{
        ESP_LOGE(TAG, "Can Bus Send Queue is NULL");
        return 0;
    }

    queue_msg.pdata = (twai_message_t *)iot_calloc(sizeof(twai_message_t)); // MALLOC_CAP_SPIRAM
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

    if (xQueueSend(xCanBusQueue_Send, &queue_msg, pdMS_TO_TICKS(50)) != pdPASS) // 不能为0，否则会丢帧
	{
		ESP_LOGE(TAG, "Can Bus Send Queue Send Failed, id: 0x%lX", id);
        free(queue_msg.pdata);
        return 0;
    }
    return 1;
}

/* Response Data 
组帧发送，can beta
填充数据

node_id:CAN总线序号，固定，不引用

return:
0-fail，1-ok
*/
uint8_t CanAckData(uint8_t node_id, uint32_t id, const uint8_t *raw, uint8_t len) 
{
	/* 内部CAN发送数据*/
    can_queue_msg_struct queue_msg;

    if (!xCanBusQueue_Send) {
        ESP_LOGE(TAG, "Can Bus Send Queue is NULL");
        return 0;
    }

    queue_msg.pdata = (twai_message_t *)iot_calloc(sizeof(twai_message_t)); 
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

    if (xQueueSend(xCanBusQueue_Send, &queue_msg, pdMS_TO_TICKS(50)) != pdPASS) { // 不能为0，否则升级时会丢帧
		ESP_LOGE(TAG, "Can Bus Send Queue Send Failed, id: 0x%lX", id);
        free(queue_msg.pdata);
        return 0;
    }
    return 1;
}

/*------------------------------------------------------------------------------
 Function: clear_canbus_rx_queue
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查队列内消息数量，不为零则清空该队列
  * @param[in]  QueueHandle_t xCanBusQueue_Send  
  * @param[out] None
  * @return     void
  */
void clear_canbus_rx_queue(void) {
    if (xCanBusQueue_Recv == NULL) {
        ESP_LOGE(TAG, "xCanBusQueue_Recv handle is NULL");
        return;
    }

    // 检查队列中的消息数量
    int msg_size = uxQueueMessagesWaiting(xCanBusQueue_Recv);
    if (msg_size > 0) {
        ESP_LOGW(TAG, "xCanBusQueue_Recv contains %d messages, clearing...", msg_size);

        // 清空队列
        void *dummy_msg; // 用于接收队列中的消息
        while (uxQueueMessagesWaiting(xCanBusQueue_Recv) > 0) {
            if (xQueueReceive(xCanBusQueue_Recv, &dummy_msg, 0) == pdTRUE) {
                //ESP_LOGD(TAG, "Removed a message from the queue");
            }
        }

        ESP_LOGI(TAG, "xCanBusQueue_Recv cleared successfully.");
    } else {
        ESP_LOGI(TAG, "xCanBusQueue_Recv is already empty.");
    }
}

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
	/* 填充CANID数据结构 */
	IdStruct canid = {0};
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

/*------------------------------------------------------------------------------
 Function: lookup_new_config
 -----------------------------------------------------------------------------*/
/**
  * @brief      CAN的转发设置参数查表
  * @param[in]  uint8_t type     
                uint16_t offset  
  * @param[out] None
  * @return     static const uint8_t*
  */
static const uint8_t* lookup_new_config(uint8_t type, uint16_t offset) {
    uint8_t *payload = NULL;
    switch (type) 
	{
        case PACK_TYPE_CONFIG_55H : payload = ((uint8_t*)&can_node_wr.Pack.pack_config) + offset; break;
        case INV_TYPE_CONFIG00_1AH: payload = ((uint8_t*)&can_node_wr.Inv.inv_set00)    + offset; break;
        case INV_TYPE_CONFIG01_1BH: payload = ((uint8_t*)&can_node_wr.Inv.inv_set01)    + offset; break;
        case INV_TYPE_CONFIG02_1CH: payload = ((uint8_t*)&can_node_wr.Inv.inv_set02)    + offset; break;
        case INV_TYPE_CONFIG03_1DH: payload = ((uint8_t*)&can_node_wr.Inv.inv_set03)    + offset; break;
		case COMMON_TYPE_TRANSMIT_27H: payload = ((uint8_t*)&can_node_wr.Iot.common_transmission)   + offset; break;
        case IOT_TYPE_INFO_01H : payload = ((uint8_t *)&can_node_rd.Iot[reals.Addr_can_self].mod_reg11000_IOT_info) + offset; break;
		case IOT_TYPE_SET_02H : payload = ((uint8_t*)&can_node_wr.Iot.mod_reg12000_IOT_set) + offset; break;
		case MODULE_TYPE_DC_AC_HUB_SET_40H:payload = ((uint8_t*)&can_node_wr.HUB.dc_ac_hub_setting)  + offset; break;
        case MODULE_TYPE_DCDC_SET_49H : payload = ((uint8_t*)&can_node_wr.Dcdc.dcdc_common_set)   + offset; break;
        case PACK_TYPE_BMS_PAYGO_INFO_5BH : payload = ((uint8_t*)&can_node_wr.Pack.pack_paygo)   + offset; break;
		default:  break;
    }

    return payload;
}

/*
先写入，后回
can 多节点操作，多字节写，自己做

*/
static void can_broadcast_cmd_inv(can_data_label *cmd_label, const uint8_t *payload)
{
    /*INV设置指令*/
	if (CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS))
    {   
        /*设置回读*/
        #ifdef CAN_Set_READ_BACK_ENABLE

    	vTaskDelay(pdMS_TO_TICKS(50));
    	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
    	{
    		for(int i = 0; i < INV_MAX_NUM; i++)
    		{
    			if (can_node_rd.Inv[i].online)		// 该总线有逆变设备在线才回读数据
    			{
    			    uint8_t dst_addr = INV_CAN_ADDR + (node_id * INV_MAX_NUM) + i;
    				CanGetTarget(node_id, 0, cmd_label->type, cmd_label->offset, cmd_label->len, dst_addr);
    				vTaskDelay(pdMS_TO_TICKS(20));
    				continue;
    			}
    		}
    	}

        #endif
    }
}

/*
先写入，后回
can 多节点操作，多字节写，自己做

*/
static void can_broadcast_cmd_iot(can_data_label *cmd_label, const uint8_t *payload)
{
    /*IOT设置指令*/
	if (CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS))
    {   
        /*设置回读*/
        #ifdef CAN_Set_READ_BACK_ENABLE
        
    	vTaskDelay(pdMS_TO_TICKS(50));
    	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
    	{
    		for(int i = 0; i < IOT_MAX_NUM; i++)
    		{
    			if ((can_node_rd.Iot[i].online)		// 该总线有IOT设备在线才回读数据
    			&&(node_id != reals.Addr_can_self))             // 排除自己
    			{
    			    uint8_t dst_addr = INV_IOT_CAN_ADDR + (i * IOT_MAX_NUM) + i;
    				CanGetTarget(node_id, 0, cmd_label->type, cmd_label->offset, cmd_label->len, dst_addr);
    				vTaskDelay(pdMS_TO_TICKS(20));
    				continue;
    			}
    		}
    	}
    
        #endif
    }
}

/*
先写入，后回
can 多节点操作，多字节写，自己做

*/
static void can_broadcast_cmd_pack(can_data_label *cmd_label, const uint8_t *payload)
{
    /*pack设置指令*/
	if (CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS))
    {   
        /*设置回读*/
        #ifdef CAN_Set_READ_BACK_ENABLE

    	vTaskDelay(pdMS_TO_TICKS(50));
        uint8_t level1_addr = 0;
        uint8_t level2_addr = 0;
        
        for (int i = 0; i < PACK_MAX_NUM; i++)
        {
            level1_addr = i / PACK_INGROUP_MAX_NUM; // PACK
            level2_addr = i % PACK_INGROUP_MAX_NUM; // PACK
        
            if (can_node_rd.Pack[level2_addr].pack_announce.online)
			{
			    uint8_t dst_addr = PACK_CAN_ADDR + i;
				CanGetTarget(level1_addr, 0, cmd_label->type, cmd_label->offset, cmd_label->len, dst_addr);
				vTaskDelay(pdMS_TO_TICKS(20));
				continue;
			}
		}

        #endif
    }
}

static void can_broadcast_cmd_dc_ac_parts(can_data_label *cmd_label, const uint8_t *payload)
{
    // 暂未实现，TODO:由ARM控制
}

static void can_broadcast_cmd_dcdc(can_data_label *cmd_label, const uint8_t *payload)
{
    CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);
}

/*
can 单节点操作，多字节写，自己做主
*/
static void can_unicasst_cmd(uint8_t md_addr, can_data_label *cmd_label, const uint8_t *payload)
{
 	uint8_t dev_id = Modbus_Addr_To_Can_Id(md_addr);
    if ( md_addr ) {
        ESP_LOGW(TAG, "cmd_label->type=0x%x,  dev_id=0x%x", cmd_label->type, dev_id);
        CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, dev_id);
    }
}

/*------------------------------------------------------------------------------
 Function: CanCmdGenerate
 -----------------------------------------------------------------------------*/
/**
  * @brief      创建并发送CAN命令
  * @param[in]  uint8_t md_addr (modbus slave addr)           
                can_data_label *cmd_label  
                uint16_t num  (需要转换成x条CAN命令)             
  * @param[out] None
  * @return     static void
  */
static void CanCmdGenerate(uint8_t md_addr, can_data_label *cmd_label, uint16_t num)
{
    if (!cmd_label || !num) return; 							// 参数检查
    if (0 == reals.Addr_can_master) return;
    
    for (uint16_t i = 0; i < num; i++)
	{
		if ((((cmd_label[i].type >= IOT_TYPE_INFO_01H) && (cmd_label[i].type <= IOT_TYPE_SET_02H))&&(reals.online_Iot_num >= 1))
			||(((cmd_label[i].type >= PACK_TYPE_CONFIG_55H) && (cmd_label[i].type <= PACK_TYPE_BMS_PAYGO_INFO_5BH)))
			||(((cmd_label[i].type >= INV_TYPE_CONFIG00_1AH) && (cmd_label[i].type <= INV_TYPE_WIFI_PARAM_25H)))
			||(cmd_label[i].type == COMMON_TYPE_TRANSMIT_27H)
			||((cmd_label[i].type == MODULE_TYPE_DC_AC_HUB_SET_40H) && (reals.online_DCHUB_num > 0))
			||((cmd_label[i].type == MODULE_TYPE_DCDC_SET_49H) && (reals.online_Dcdc_num > 0))
			) 
		{	
			ESP_LOGW(TAG, "modbus --> can: modbus addr=0x%x, type=0x%x, offset=%d, len=%d, i=%d", 
					md_addr, cmd_label[i].type, cmd_label[i].offset, cmd_label[i].len, i);
            
		    /*查找需要下发的数据*/
	        const uint8_t *payload = lookup_new_config(cmd_label[i].type, cmd_label[i].offset);

            /* 下发的数据不存在则继续查找下一条指令 */
			if (payload == NULL) 
            {
                ESP_LOGE(TAG, "lookup_new_config error, payload = NULL");
                continue;
            }

			/* 由于历史原因部分设备不识别杂项使能的高字节当该项高字节时不往下转*/
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

			/* 广播指令读取 */
			if (IS_SUMMARY_ADDR(md_addr)) 
			{
				if((cmd_label[i].type == IOT_TYPE_SET_02H)                 
				|| (cmd_label[i].type == IOT_TYPE_INFO_01H)) //IOT直接广播
				{
					ESP_LOGW(TAG, "go to  can_broadcast_cmd_iot");
					can_broadcast_cmd_iot(&cmd_label[i], payload);
				}						
				else if ((cmd_label[i].type >= INV_TYPE_CONFIG00_1AH) && (cmd_label[i].type <= INV_TYPE_WIFI_PARAM_25H)) 
				{
					ESP_LOGW(TAG, "go to can_broadcast_cmd_inv");
					can_broadcast_cmd_inv(&cmd_label[i], payload);
				}
				else if((cmd_label[i].type >= PACK_TYPE_CONFIG_55H) && (cmd_label[i].type <= PACK_TYPE_BMS_PAYGO_INFO_5BH)) 
				{
					ESP_LOGW(TAG, "go to  can_broadcast_cmd_pack");
					can_broadcast_cmd_pack(&cmd_label[i], payload);
				}
				else if(cmd_label[i].type == MODULE_TYPE_DC_AC_HUB_SET_40H)
				{
					ESP_LOGW(TAG, "go to can_broadcast_cmd_dc_ac_hub");
					can_broadcast_cmd_dc_ac_parts(&cmd_label[i], payload);
				}
				else if(cmd_label[i].type == MODULE_TYPE_DCDC_SET_49H)
				{
					ESP_LOGW(TAG, "go to can_broadcast_cmd_dcdc");
					can_broadcast_cmd_dcdc(&cmd_label[i], payload);
				}
                else
                {
                    ESP_LOGE(TAG, "Unknown: dev_id(0xFF), type(0x%x)", cmd_label[i].type);
                    continue;
                    // CanSetTarget(0, 0, cmd_label->type, cmd_label->offset, payload, cmd_label->len, CAN_BROADCAST_ADDRESS);
                }
			}
			/* 单播指令读取 */
			else 
			{
				ESP_LOGW(TAG, "go to can_unicasst_cmd");
				can_unicasst_cmd(md_addr, &cmd_label[i], payload);
			}

			/* 设置完成后延时一段时间再汇总数据加快APP回读响应 */
#ifdef CAN_CMD_SET_READ_BACK_ENABLE
            for ( uint8_t index = 0 ; index < MAX_NUM_CAN_TYPE ; index++ )
            {
                if(cmd_label[i].type == reals.can_poll[index].can_type) 
                {
                    reals.can_poll[index].fast_read = 1;  
                    reals.can_poll[index].modbus_Rd_SlaveAddr = md_addr;
                    goto readback;
                }
            }
            
            readback:
#endif

            #if 0
			/* 有新设置完成下发,检查是否需要激活特联动的指令*/
	        if (cmd_label[i].active_can_cmd_type != 0xFFFF)
			{
	            CanCmdActivte(cmd_label[i].active_can_cmd_type); 	// modbus设置某个操作之后，需要激活联动的指令获取数据
				ESP_LOGW(TAG, "active binding cmd, type: %02x\n", cmd_label[i].active_can_cmd_type);
				trigger_read_type = cmd_label[i].active_can_cmd_type;
				trigger_read_addr = md_addr;
	        }
            #endif
    	}
        else
        {
            if ( cmd_label[i].type != 0 )
            {
                ESP_LOGE(TAG, "modbus to can error : modbus addr=0x%x, type=0x%x, offset=%d, len=%d, i=%d", 
                    md_addr, cmd_label[i].type, cmd_label[i].offset, cmd_label[i].len, i);
            }
        }
    }
}


/*------------------------------------------------------------------------------
 Function: Can_inv_send_DCDC_state
 -----------------------------------------------------------------------------*/
/**
  单帧 主动上报，间隔5秒周期一个队列发送，帧间隔100ms

  * @brief      识别到新逆变器 CAN_WITH_NEW_INV，周期上报设备信息
  				DCDC->CAN  0x0x1901xxXX
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Can_inv_send_DCDC_state(void) //dcdc_info_pv
{
	static uint16_t snct_big =0;
	static uint16_t snct_small =0;

	/*5s大循环周期*/
	if (snct_big++ >= 50)
	{
		if (snct_small++ < 7)
		{
			dcdc_send_can_data_poll(snct_small);
		}
		else
		{
			snct_small = 0;
			snct_big = 0;
		}
	}
	
}
/*------------------------------------------------------------------------------
 Function: split_u64_to_u8_array
 -----------------------------------------------------------------------------*/
/**
  * @brief      uint64_t拆分成8个uint8_t
  * @param[in]  void  
  * @param[out] input
  * @param[out] output
  * @return     void
  */
void split_u64_to_u8_array(uint64_t input, uint8_t output[8]) {
    for (int i = 0; i < 8; i++) {
        output[i] = (input >> (i * 8)) & 0xFF;
    }
}

#define IOT_INTERNAL_PRIORITY	20800 // 通用逆变器优先级

/*------------------------------------------------------------------------------
 Function: IOT_INTERNAL_TYPE_CODE
 -----------------------------------------------------------------------------*/
/**
  * @brief      ID:0x08FAFFXX	IOT->ALL	
                心跳帧，用于多IOT通讯在线检测
  * @param[in]  None
  * @param[out] None
  * @return     #define
  */
void Iot_Heartbeat_frame_Send(bool fast_flag)
{
    if(can_ota_is_doing() || bms_ota_is_doing()) return;

	static uint32_t pre_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if ( !fast_flag ) {
        if ((pre_time != 0) && ((now_time - pre_time) < HEART_BEAT_FRAME_PERIOD_MS)) return;
    }

    uint16_t type_idx = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
	pre_time = now_time;
	can_ctrl_t can_ctrl = {0};
	can_ctrl.data[0] = 99;
	can_ctrl.data[1] = STEP_CAN_PARALLEL_FINISH;
	can_ctrl.data[2] = IOT_INTERNAL_PRIORITY & 0xFF; // 优先级_L
	can_ctrl.data[3] = (IOT_INTERNAL_PRIORITY >> 8) & 0xFF;  // 优先级_H
	can_ctrl.data[4] = 0;    // 预留
	can_ctrl.data[5] = type_idx & 0xFF;    // 设备代号_L
	can_ctrl.data[6] = (type_idx >> 8) & 0xFF;    // 设备代号_H
	can_ctrl.data[7] = 0;    // 群组编号
	
	/* 填充CANID数据结构 */
	IdStruct canid = {0};
	canid.bit.src = esp_canbus_myself_address();
	canid.bit.dst = CAN_BROADCAST_ADDRESS;
	canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
	canid.bit.page = CAN_CTRL_FRAME_PAGE;
	canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

	/* 发送单帧CAN数据 */
	CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
}

/*------------------------------------------------------------------------------
 Function: Iot_can_ota_ready_period
 -----------------------------------------------------------------------------*/
/**
  * @brief      保持总线静默
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Iot_can_ota_ready_period(void)
{
    if(can_ota_is_doing() || bms_ota_is_doing()) return;
    if(1 != reals.Addr_can_master) return;

    static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if ((pre_time != 0) && ((now_time - pre_time) < 1000)) return;
    pre_time = now_time;

    // 初始化CAN控制帧结构体
    OtaReady dev_ready = {0};
    uint32_t canId = 0;

    // 70帧总线静默
    if (( reals.current_ota_info.soft_version != 0 ) 
        && ( reals.current_ota_info.soft_version % 10000 < 100 )) {
        dev_ready.nc = 1;   // BOOT升级标志
    }
    canId = CAN_ID(2, 0, OTA_READY, CAN_BROADCAST_ADDRESS, esp_canbus_myself_address());
    OTA_SendMessage(0, canId, (uint8_t *)&dev_ready, 8);

#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE
    // 78帧系统升级进度
    OtaPct dev_pct = {0};
    dev_pct.type = reals.current_ota_info.type;
    dev_pct.pct = reals.system_ota.ota_total_pct;
    dev_pct.errcode = reals.current_ota_info.errCode;
    canId = CAN_ID(2, 0, OTA_PCT, CAN_BROADCAST_ADDRESS, esp_canbus_myself_address());
    OTA_SendMessage(0, canId, (uint8_t *)&dev_pct, 8);
    ESP_LOGI(TAG, "System OTA: Type: %d(%d), Pct: %d%% ( %d / %d ), Errorcode: %d", dev_pct.type, dev_ready.nc, dev_pct.pct, 
        reals.system_ota.ota_mcu_curr_count, reals.system_ota.ota_mcu_total_count, dev_pct.errcode);
#endif
}

/**
 * @brief 发送EMS控制功能5的CAN控制帧
 *
 * 用于通过CAN总线发送EMS控制功能5（系统开关、智能开关、休眠、出厂设置等）相关指令。
 *
 * @param PowerCtrl      系统开关指令（0:无效 1:开机 2:关机 3:休眠）
 * @param reset_factory  恢复出厂设置指令（0:无效 1:使能 2:恢复出厂设置完成）
 * @param reset_energy   能量初始化指令（0:无效 1:使能 2:能量清除完成）
 */
void Iot_EMS_Ctrl_Func_5_Send(uint8_t PowerCtrl, uint8_t reset_factory, uint8_t reset_energy)
{
	if(sys_is_updating()) return;

#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5    
    can_ctrl_t can_ctrl = {0}; // 初始化CAN控制帧结构体
    can_ctrl.cmd = 50;  // 功能码：50，对应EMS控制功能5

    // 填充EMS控制5相关字段
    can_ctrl.ems_ctrl5.sleepSet = IotSetData.dev_info_t.Remote_set.Remote_set_enable;   // 休眠模式开关
    can_ctrl.ems_ctrl5.powerOffCtrl = PowerCtrl;    // 系统开关指令
    can_ctrl.ems_ctrl5.iot_ctrl_mask = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_smart_control.bit.relay2 ? 1 : 2;   // AC定时开关状态        
    can_ctrl.ems_ctrl5.remote_Soc = IotSetData.dev_info_t.Remote_set_soc_limit; // 休眠SOC阈值 
    can_ctrl.ems_ctrl5.reset_factory = reset_factory; // 恢复出厂设置指令
    can_ctrl.ems_ctrl5.reset_energy = reset_energy;   // 能量初始化指令
#ifdef CONFIG_IOT_LED_SET_ENABLE
    can_ctrl.ems_ctrl5.led_brightness_set = IotSetData.dev_info_t.LED_SET.bit.brightness_set; // LED亮度设置
#else
    can_ctrl.ems_ctrl5.led_brightness_set = 0; // 无效
#endif

    // 填充CANID数据结构
    IdStruct canid = {0};
    canid.bit.src = esp_canbus_myself_address();      // 源地址：本机CAN地址
    canid.bit.dst = CAN_BROADCAST_ADDRESS;            // 目的地址：广播
    canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;     // 功能码
    canid.bit.page = CAN_CTRL_FRAME_PAGE;             // 页码
    canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;     // 优先级

    // ESP_LOGI(TAG, "Iot_EMS_Ctrl_Func_5_Send [ID:0x%04lx, Len:%d]", canid.all, sizeof(can_ctrl_t));
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t), ESP_LOG_INFO);

    // 发送单帧CAN数据
    CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
#endif    
}

/**
 * @brief 功率电压转电流原始值
 * @param power_raw 功率原始值，u16，单位1W
 * @param volt_raw  电压原始值，u16，单位0.1V
 * @return 电流原始值，u16，单位0.1A
 */
static uint16_t calc_current_raw(uint16_t power_raw, uint16_t volt_raw)
{
    // 1. 防除零保护：电压为0时直接返回0（避免程序崩溃）
    if(volt_raw == 0)
    {
        return 0;
    }

    // 2. 转32位中间变量，防止100*power_raw溢出16位
    // （u16最大值65535，100*65535=6553500，远大于u16上限65535）
    uint32_t p = power_raw;
    uint32_t u = volt_raw;

    // 3. 核心换算：100*p/u，加u/2实现四舍五入（替代直接截断的整数除法）
    uint32_t current_raw = (100 * p + u / 2) / u;

    // 4. 饱和处理：防止结果超过u16最大值，避免溢出截断
    if(current_raw > UINT16_MAX)
    {
        current_raw = UINT16_MAX;
    }

    return (uint16_t)current_raw;
}

/*------------------------------------------------------------------------------
 Function: Iot_Disaster_frame_Send
 -----------------------------------------------------------------------------*/
/**
  * @brief      周期发送的DCDC控制帧
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Iot_Dcdc_Ctrl_frame_Send(bool send_flag)
{
	if(sys_is_updating()) return;

	static uint32_t pre_time = 1500;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

	if ((!send_flag) && (pre_time != 0) && ((now_time - pre_time) < 5000)) return;
	pre_time = now_time;
    
	uint8_t status = 0;
#if CONFIG_DISASTER_WARN_CHANNEL == 2     
	status |= (GetStormWatchDataState() == 1) ? 1 : 0; // bit0: 风暴预警状态
#else
    status |= (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.thunder_work_mode_state == 1) ? 1 : 0; // bit0: 风暴预警状态
#endif
	status |= Get_Plan_Backup_Power_Status(); // bit0: 预约备电激活状态
	status |= top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.link.bit.ble_connect << 1; // bit1: 蓝牙连接

	uint16_t max_chg_voltage = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.max_chg_voltage;
#if 0   // 电流不能直接取BMS，需从INV获取功率计算
    uint16_t max_chg_current = top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.max_chg_current;
#else
    uint16_t max_chg_current = calc_current_raw(can_node_rd.Inv[reals.Addr_can_self].inv_announce.Pv_Chg_Power[2], 
                                        top_modbus_rd.Pack[PACK_MAX_NUM].mod_reg06000_Pack_sum.max_chg_voltage);
#endif

    uint8_t ctrl_byte1 = 0;
    uint8_t ctrl_byte2 = 0;

    // 根据不同DCDC设备执行对应逻辑
    if ( STR_EQ_EXACT(can_node_rd.Dcdc[0].dcdc_common_info.dev_type, SN_TYPE_SOLARX4K_ASCII)) {
        // 上电时默认发0无效
        ctrl_byte1 |= top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.charger_set.bit.dc_out_onoff;         // Bit0/1:   DC3输出开关（对应APP“光伏使能”）
        ctrl_byte1 |= top_modbus_wr.Dcdc.mod_reg15600_DCDC_set.mode4_set.bit.long_short_line << 2;   // Bit2/3:   长短线设置（ 1-小电流（对应长线）；2-大电流（对应短线））
        ctrl_byte2 |= 1; // bit0 额定最大输出电流存放位置(1:存放于D400S（后续兼容D400S的逆变器该位置1）
    } else if ( STR_EQ_EXACT(can_node_rd.Dcdc[0].dcdc_common_info.dev_type, SN_TYPE_CHARGER2_ASCII)) {
        ctrl_byte1 |= 0;         // Bit0/1:   DC3输出开关（TODO：未使用）
        ctrl_byte1 |= 1 << 2;    // Bit2/3:   DC3输入开关（20260627：产品决议默认开启）
    } else {
        ESP_LOGW(TAG, "Iot_Dcdc_Ctrl_frame_Send : Unknown Device(%s)", can_node_rd.Dcdc[0].dcdc_common_info.dev_type);
    }
    
	can_ctrl_t can_ctrl = {0};
	can_ctrl.data[0] = 14; // 功能码14
	can_ctrl.data[1] = status;
	can_ctrl.data[2] = max_chg_voltage & 0xFF; // 最大允许充电电压低字节
	can_ctrl.data[3] = (max_chg_voltage >> 8) & 0xFF; // 最大允许充电电压高字节
	can_ctrl.data[4] = max_chg_current & 0xFF; // 最大允许充电电流低字节
	can_ctrl.data[5] = (max_chg_current >> 8) & 0xFF; // 最大允许充电电流高字节
	can_ctrl.data[6] = ctrl_byte1;
	can_ctrl.data[7] = ctrl_byte2;
    
	/* 填充CANID数据结构 */
	IdStruct canid = {0};
	canid.bit.src = esp_canbus_myself_address();
	canid.bit.dst = CAN_BROADCAST_ADDRESS;
	canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
	canid.bit.page = CAN_CTRL_FRAME_PAGE;
	canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

    // ESP_LOGI(TAG, "Iot_Dcdc_Ctrl_frame_Send [ID:0x%04lx, Len:%d]", canid.all, sizeof(can_ctrl_t));
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t), ESP_LOG_INFO);

    // 发送单帧CAN数据
    CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
}

/*------------------------------------------------------------------------------
 Function: Iot_Dcdc_Common_Target_Send
 -----------------------------------------------------------------------------*/
/**
  * @brief      触发式发送的DCDC通用帧
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Iot_Dcdc_Common_Target_Send(uint8_t index, uint16_t v_max, uint16_t i_max)
{
	if(sys_is_updating()) return;
        
	can_ctrl_t can_ctrl = {0};
	can_ctrl.data[0] = 27; // 功能码27
	can_ctrl.data[1] = index;   // 子索引序号
	can_ctrl.data[2] = v_max & 0xFF; // 额定最大输出电压L位
	can_ctrl.data[3] = (v_max >> 8) & 0xFF; // 额定最大输出电压H位
	can_ctrl.data[4] = i_max & 0xFF; // 额定最大输出电流L位
	can_ctrl.data[5] = (i_max >> 8) & 0xFF; // 额定最大输出电流H位
    
	/* 填充CANID数据结构 */
	IdStruct canid = {0};
	canid.bit.src = esp_canbus_myself_address();
	canid.bit.dst = CAN_BROADCAST_ADDRESS;
	canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;
	canid.bit.page = CAN_CTRL_FRAME_PAGE;
	canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;

    ESP_LOGI(TAG, "Iot_Dcdc_Common_Target_Send [ID:0x%04lx, Len:%d]", canid.all, sizeof(can_ctrl_t));
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t), ESP_LOG_INFO);

    // 发送单帧CAN数据
    CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
}

/**
 * @brief 发送电池维护（老化）控制指令
 *
 * 向CAN总线广播电池维护（老化）控制命令，设置维护使能状态。
 * 若系统正在升级，则不发送。
 *
 * @param maintain_enable 维护使能（0-invalid，1-enable，2-disable）
 */
void Iot_set_bat_maintain_once(uint8_t maintain_enable)
{
    if(sys_is_updating()) return;

    can_ctrl_t can_ctrl = {0}; // 初始化CAN控制帧结构体
    can_ctrl.cmd = 10;  // 功能码：10，对应电池老化帧

    // 填充电池老化帧相关字段
    can_ctrl.battery_aging.maintain_enable = maintain_enable;

    // 填充CANID数据结构
    IdStruct canid = {0};
    canid.bit.src = esp_canbus_myself_address();      // 源地址：本机CAN地址
    canid.bit.dst = CAN_BROADCAST_ADDRESS;            // 目的地址：广播
    canid.bit.funcode = CAN_CTRL_FRAME_FUNC_CODE;     // 功能码
    canid.bit.page = CAN_CTRL_FRAME_PAGE;             // 页码
    canid.bit.priority = CAN_CTRL_FRAME_PRIORITY;     // 优先级

    // 发送单帧CAN数据
    CanAckData(0, canid.all, (uint8_t *)&can_ctrl, sizeof(can_ctrl_t));
}


/*------------------------------------------------------------------------------
 Function: OTA_Percent_Get_Min
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取最小升级进度
  * @param[in]  void  
  * @param[out] None
  * @return     static uint32_t
  */
static uint32_t OTA_Percent_Get_Min(void)
{
    uint32_t buff[NUM_MAX_CAN_OTA_DEVICE_STATUS];
    uint8_t i = 0;
    uint32_t rtn = 0;
    uint8_t online_cnt = 0;

    for (i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++)
    {
        if (((can_ota_status.devStatus[i].isOta == OTA_DOING) ||
             (can_ota_status.devStatus[i].pct != 0)))
        {
            buff[i] = can_ota_status.devStatus[i].pct;
            online_cnt++;
        }
        else // offline，填xFF,取不到最
        {
            buff[i] = 0xFF;
        }
    }

    if (0 == online_cnt)
    {
        rtn = 0;
    }
    else
    {
        rtn = data_min(buff, i);
    }
    return rtn;
}

/*------------------------------------------------------------------------------
 Function: can_ota_summary
 -----------------------------------------------------------------------------*/
/**
  * @brief      升级进度汇总
  * @param[in]  uint8_t node_id  
                uint8_t group    
  * @param[out] None
  * @return     void
  */
void can_ota_summary(uint8_t node_id, uint8_t group)
{
	uint8_t can_bus_seq = 0;
	uint8_t i = 0;

    //pct取最小，其他取第一个在线设备
	can_ota_status.ota_summary.pct = OTA_Percent_Get_Min();
	for(i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++) {
		if(can_ota_status.devStatus[i].where != 0) { // 有值代表正在升级
			can_ota_status.ota_summary.isOta = can_ota_status.devStatus[i].isOta;
			can_ota_status.ota_summary.errCode = can_ota_status.devStatus[i].errCode;
		//	can_ota_status[can_bus_seq].ota_summary.pct = can_ota_status[can_bus_seq].devStatus[dev_id].pct;
			can_ota_status.ota_summary.where = can_ota_status.devStatus[i].where;
			can_ota_status.ota_summary.type = can_ota_status.devStatus[i].type;
			can_ota_status.ota_summary.level = can_ota_status.devStatus[i].level;
			break;
		}
	}

    set_ota_pct_info(NULL, 0, can_ota_status.ota_summary.type, can_ota_status.ota_summary.isOta, can_ota_status.ota_summary.where, can_ota_status.ota_summary.pct, can_ota_status.ota_summary.errCode);
}

/*------------------------------------------------------------------------------
 Function: ota_can_pct
 -----------------------------------------------------------------------------*/
/**
  * @brief      0x0878
  * @param[in]  OtaDevice canOta[]  
                uint8_t id          
                uint8_t type        
                uint8_t pct     
                uint8_t where
                uint8_t errcode     
  * @param[out] None
  * @return     void
  */
void ota_can_pct(OtaDevice canOta[], uint8_t id, uint8_t type, uint8_t pct, uint8_t where, uint8_t errcode) {

    /* CAN总线广播出来的OTA进度,IOT监听并收集进度 */
    canOta[id].type = type;
    canOta[id].errCode = errcode;
    canOta[id].pct = pct;
    canOta[id].where = where;

    /* 底层设备进度100或底层出现故障不在升级 */
    if (canOta[ id ].pct >= 100 || canOta[ id ].errCode) {
        canOta[ id ].isOta = OTA_IDLE;
    }
}

/*------------------------------------------------------------------------------
 Function: ota_dev_timeout
 -----------------------------------------------------------------------------*/
/**
  * @brief      升级超时处理
  * @param[in]  OtaDevice canOta[]  
  * @param[out] None
  * @return     void
  */
void ota_dev_timeout(OtaDevice canOta[])
{
    for (uint8_t i = 0; i < NUM_MAX_CAN_OTA_DEVICE_STATUS; i++)
	{
        if (canOta[i].isOta == OTA_DOING)
		{
            canOta[i].isOta = OTA_IDLE;

            /*对应地址存在0x0878帧上报的错误码，优先使用*/
            canOta[i].errCode = (can_ota_status.devStatus[i].errCode != 0) ? can_ota_status.devStatus[i].errCode : OTA_ERR_NO_RESP;
        }
    }
}

/*------------------------------------------------------------------------
*@Function :Device_Discovery_Step 

设备发现广播帧动作时 基于超时进入下一阶段

*@return		 
0- fail
no 0: tx len
*/
void Device_Can_Address_Parallel_Step(void) //100ms cycle
{
    uint16_t i = 0;
    static uint16_t sMasterChgcnt = CAN_MASTER_CHANGE_DELAY>>1;
    // static uint16_t scnt3 = 3;
    static uint16_t sStepSendPeriodCnt = 0;//Step CAN发送周期
	// static uint16_t sStepWaitSortCnt = 0;//Step 并机重排等待时间
	
    uint64_t iot_sn_temp =0;
    // uint16_t Step_delay = 0;

	can_ctrl_t can_ctrl = {0};

	// CAN OTA升级中不发送
	if ((can_ota_status.period_cmd_delay > 0) || (bms_ota_is_doing())) {	//CAN-OTA 不进行查询
		return;
	}

	//重新排序等待间隔
	if(reals.Addr_can_Parallel_Interval)
	{	
		reals.Addr_can_Parallel_Interval--;
		if(!reals.Addr_can_Parallel_Interval)
		{
			ESP_LOGI(TAG,"Can Addr parallel Sort Limit Remove");
		}
	}
	if (DEV_MAIN_NODE_MAX > 1 )
	{
        /*执行间隔：STEP_CAN_SEND_PERIOD（300ms）*/
    	if(++sStepSendPeriodCnt >= STEP_CAN_SEND_PERIOD)
    	{
    		sStepSendPeriodCnt = 0;

            /*并机排地址触发帧*/
    		if(STEP_CAN_PARALLEL_TRIGER == reals.Step_can_dev_parallel)
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
            /*设备SN上报帧*/
    		else if(STEP_CAN_PARALLEL_SEND_SN == reals.Step_can_dev_parallel)//
    		{
    			can_ctrl.data[0]=CAN_CTRL_CMD_99;
    			can_ctrl.data[1]=reals.Step_can_dev_parallel;

                /*can_ctrl.data[2~6] SN*/
                #ifdef	PARALLEL_IOT_CAN_ADDR_FROM_ARM_ENALBE
                
    			    memcpy((uint8_t*)&can_ctrl.data[2], (uint8_t*)&IotSetData.dev_info_t.Sn_bind[0], 5);//源 SN
    			
                #else//debug
                
        			iot_sn_temp =iot_factory.iot_sn/1000;//删除低三个十进制数（随机数）
        			memcpy((uint8_t*)&can_ctrl.data[2], (uint8_t*)&iot_sn_temp, 5);//源 SN
    			
                #endif
    			
    			can_ctrl.data[7]=0;
    			
    			can_overall_ctrl(0, &can_ctrl);
    		}
            /*排地址完成（正常心跳帧）*/
    		else if(STEP_CAN_PARALLEL_FINISH == reals.Step_can_dev_parallel)//
    		{
                Iot_Heartbeat_frame_Send(false); 
				
    		}
    	}

        /*并机排地址状态切换逻辑*/
    	if(++reals.Step_can_Cnt >= STEP_CAN_PARALLEL_DELAY1)
    	{
    		reals.Step_can_Cnt =0;
    		if(STEP_CAN_PARALLEL_INIT == reals.Step_can_dev_parallel)
    		{
    			reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_TRIGER;
    		}
    		else if(STEP_CAN_PARALLEL_TRIGER == reals.Step_can_dev_parallel)
    		{
    			reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_SEND_SN;
    		}
    		else if(STEP_CAN_PARALLEL_SEND_SN == reals.Step_can_dev_parallel)
    		{
    			reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_FINISH;
    		}	
    		else if(STEP_CAN_PARALLEL_FINISH == reals.Step_can_dev_parallel)
    		{
    			reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_AFTER;
    		}				
    	}

        /*并机排地址触发/收到从机报文重新排地址/SN重复*/
    	if((STEP_CAN_PARALLEL_TRIGER == reals.Step_can_dev_parallel)//clean
    	    ||(1 == reals.MonSnSame))
    	{
    		reals.MonSnSame = 0;
    		reals.Addr_can_master = 0;
    		reals.Addr_can_Buff =0;
    		reals.Addr_can_Small =IOT_MAX_NUM*DEV_MAIN_NODE_MAX;	
    		reals.Addr_can_self=0;
    		for (i = 0; i < (IOT_MAX_NUM*DEV_MAIN_NODE_MAX+1); i++)		
    		{												
    			reals.INV_SN_can[i] =0;
    		}
    	}

        /*更新自己CAN地址*/
    	if(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_FINISH)
    	{
			//refresh the new address
			if (reals.Addr_can_self != reals.Addr_can_Buff)
			{
				/*模块自身的地址 = 收到的所有地址数量*/
				reals.Addr_can_self = reals.Addr_can_Buff; 			 
			}
    	}
    }
    else
    {
        /*不考虑并机*/
        reals.Step_can_dev_parallel = STEP_CAN_PARALLEL_AFTER;
        reals.Addr_can_Small =IOT_MAX_NUM*DEV_MAIN_NODE_MAX;
    }
    
    /*主设备的判定*/
    if(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER)
    {
        /*存在高优先级设备时不考虑并机结果*/
        if ( reals.Addr_can_slave == 1 )
        {
            reals.Addr_can_slave_timeout = (reals.Addr_can_slave_timeout > 100) ? (reals.Addr_can_slave_timeout - 100) : 0;
            
            if ( reals.Addr_can_slave_timeout > 0 )
            {
                /*存在高优先级IOT，保持静默*/
                reals.Addr_can_master = 0;
//                ESP_LOGI(TAG,"Device_Can_Address_Parallel_Step : Addr_can_slave_timeout = %d", reals.Addr_can_slave_timeout );
            }
            else
            {
                reals.Addr_can_slave = 0; 
                reals.Addr_can_master = 1;
            }
        }
        else
        {
            /*每隔5秒依据并机排序结果更新主机，关联IOT的周期上报帧周期*/
            if(++sMasterChgcnt >= CAN_MASTER_CHANGE_DELAY/100)
            {
                sMasterChgcnt =0;    
                if (reals.Addr_can_self < reals.Addr_can_Small)
                {
                    /*除主模块外其他IOT保持静默*/
                    reals.Addr_can_master = 1;
                }
                else
                {
                    reals.Addr_can_master = 0;
                }   
            
                /*10秒自动更新一次，更新周期和心跳周期必须相同*/
                reals.Addr_can_Small =IOT_MAX_NUM*DEV_MAIN_NODE_MAX;
            }
        }
    }

	/*排地址完成后再发心跳*/
	if((reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER))
	{
		Iot_Heartbeat_frame_Send(false);
	}
}

static void iot_can_bind_send_and_check(void)
{
	static uint32_t pre_time = 0;
	static uint16_t sBindChk_100min = 50; 	//首次上电5秒后检测
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if ((pre_time != 0) && ((now_time - pre_time) < 100)) return;

#ifdef CAN_ADDRESS_PARALLEL_ENABLE
    // 如果OTA进行中，不进行地址并行
    if (can_ota_status.period_cmd_delay == 0) {
        // 设备广播帧
        Device_Can_Address_Parallel_Step();

#if DEV_MAIN_NODE_MAX > 1
        // 设备绑定帧
        Can_Tx_single_binding_triger();
#endif
    }
    
#if DEV_MAIN_NODE_MAX > 1
    sBindChk_100min--;
    if (sBindChk_100min == 0) {
        sBindChk_100min = 36000; // 首次上电检测命令发送后，后续每1小时发送一次
        Can_Tx_single_binding_Check();
    }
    if (reals.Addr_can_Attr & Can_Addr_Parallel_SendBindChk) {
        reals.Addr_can_Attr &= ~(Can_Addr_Parallel_SendBindChk);
        Can_Tx_single_binding_Check();
    }
#endif

#endif
}

/*------------------------------------------------------------------------------
 Function: Can_Addr_Sequence
 -----------------------------------------------------------------------------*/
/**
  * @brief      can并机地址排序，获取最小地址，
                只存储比自己SN小的其他设备SN
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Can_Addr_Sequence(void)//地址自动识别
{
	uint64_t u32IDSelf=0;//UINT32,64bit
	// uint64_t u32IDReceive=0;
	uint8_t i=0;
	uint8_t u16IdExistFlag=0;//0-之前不存在/未获得，1-已经获得/已存在

	u16IdExistFlag = 0;

	/*收到新的IOT地址消息*/
	if(reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX])	  
	{
		u32IDSelf = iot_factory.iot_sn/1000;

        /*收到与自身重复的SN*/
		if(reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] == u32IDSelf)
		{
		    ESP_LOGE(TAG,"Can_Addr_Sequence : The SN that is the same as its own is received" );
			reals.MonSnSame=1;
		}

		/*windy：只接收比自己SN小的编号，忽略较大编号，从而最小编号的模块接收的SN数量为0，为主模块*/
		if((reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] < u32IDSelf) && (reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX]))
		{		
			for (i = 0; i < IOT_MAX_NUM*DEV_MAIN_NODE_MAX; i++)			
			{	
			    /*收到的其他模块的地址*/
				if(reals.INV_SN_can[i])
				{
					if(reals.INV_SN_can[i] == reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX])
					{
					    /*已存在此报文的模块地址*/
						u16IdExistFlag = 1;
						ESP_LOGI(TAG,"Can_Addr_Sequence : The module address of the packet already exists. Procedure" );
					
						break;
					}
				}
			}		

            /*之前不存在此报文的模块ID，则存储下来*/
			if(u16IdExistFlag == 0)//
			{			
				reals.Addr_can_Buff++;
				ESP_LOGI(TAG,"Can_Addr_Sequence : Add module address, all(%d)", reals.Addr_can_Buff);
                
                for (i = 0; i < IOT_MAX_NUM*DEV_MAIN_NODE_MAX; i++)         
                {   
                    /*寻找空位置存储*/
                    if(!reals.INV_SN_can[i])
                    {
                        /*存储地址*/
                        reals.INV_SN_can[i] = reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX];
                        u16IdExistFlag = 1;
                        ESP_LOGW(TAG,"Can_Addr_Sequence : Store the new module address : %llu", reals.INV_SN_can[i]);
                    
                        break;
                    }
                }       
			}
		}

        /*清除缓存*/
		reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] = 0;
	}		
}

/* 接收并解析Can报文
判断逆变器并机地址
*/
void Can_Inv_Addr_Check(node_info_t *node, twai_message_t *frame) 
{
    IdStruct canId = {0};
    uint8_t index = 0;
	uint64_t can_Sn=0;
    uint8_t i = 0;

    if (!frame) 
		return ;

    canId.all = frame->identifier;//can rx ID

    if (0x08FA0000 == (canId.all&0xFFFF0000))//0x08FAFFXX
    {
		if(99 == frame->data[0]&&(DEV_MAIN_NODE_MAX > 1))
		{
			ESP_LOGI(TAG,"can rx 99 =:%d",frame->data[1] );
		
		 	if(STEP_CAN_PARALLEL_SEND_SN== frame->data[1])
			{
				memcpy((uint8_t*)&can_Sn, (uint8_t*)&frame->data[2], 5);//SN
				ESP_LOGI(TAG,"Can_Inv_Addr_Check : can_Sn =:%llu",can_Sn );
				if(0 != can_Sn)
				{
					reals.INV_SN_can[IOT_MAX_NUM*DEV_MAIN_NODE_MAX] = can_Sn;
					Can_Addr_Sequence();
				}
			}
			else if(STEP_CAN_PARALLEL_TRIGER== frame->data[1])
			{
				reals.MonSnSame = 0;
				reals.Addr_can_master = 0;
				reals.Addr_can_Buff =0;
				reals.Addr_can_Small =IOT_MAX_NUM*DEV_MAIN_NODE_MAX;	
				reals.Addr_can_self=0;
				for (i = 0; i < (IOT_MAX_NUM*DEV_MAIN_NODE_MAX+1); i++) 		
				{												
					reals.INV_SN_can[i] =0;
				}

				if(STEP_CAN_PARALLEL_TRIGER != reals.Step_can_dev_parallel)
				{
					reals.Step_can_dev_parallel =STEP_CAN_PARALLEL_SEND_SN;
					reals.Step_can_Cnt=0;
				}
			}		 
		}
	}

/*任何报文（更新最小CAN地址）*/
	if((STEP_CAN_PARALLEL_AFTER == reals.Step_can_dev_parallel)
		&&(canId.bit.src >= INV_IOT_CAN_ADDR) && (canId.bit.src < (INV_IOT_CAN_ADDR + IOT_MAX_NUM*DEV_MAIN_NODE_MAX)))
	{
		index = canId.bit.src - INV_IOT_CAN_ADDR;
		if (index < reals.Addr_can_Small)//CAN总线其他IOT CAN地址最小值，在排地址之外的正常时段
		{
			reals.Addr_can_Small = index;
            ESP_LOGI(TAG,"Can_Inv_Addr_Check : get Addr_can_Small = %d", index);
		}
	}
    else
    {
        /*检测到更高优先级的IOT设备*/
        if((canId.bit.src == EBOX_PBOX_CAN_ADDR)
            || ((canId.bit.src >= HMI_CAN_ADDR) && (canId.bit.src < (HMI_CAN_ADDR + HMI_GROUP_MAX_NUM)))
          )
        {
            if ( reals.Addr_can_slave == 0 )
            {
                ESP_LOGI(TAG,"Can_Inv_Addr_Check : get Addr_can_slave");
            }
            reals.Addr_can_slave = 1;
            reals.Addr_can_slave_timeout = IOT_OFFLINE_TIME;
        }
    }

    /*汇总IOT地址*/
    if(reals.Addr_can_slave == 0) CAN_IOT_ID_Get(canId.bit.src);
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

#endif

/*最大IOT地址数量*/
#define IOT_CAN_ID_MAX_NUM      16
#define IOT_CAN_ID_TIMEOUT_S    (IOT_OFFLINE_TIME/1000)
EXT_RAM_BSS_ATTR static uint8_t can_iot_id_sum[IOT_CAN_ID_MAX_NUM] = {0};
EXT_RAM_BSS_ATTR static uint8_t can_iot_id_timeout[IOT_CAN_ID_MAX_NUM] = {0};
/*------------------------------------------------------------------------------
 Function: CAN_IOT_ID_Get
 -----------------------------------------------------------------------------*/
/**
  * @brief      IOT地址汇总存储
  * @param[in]  uint8_t devId  
  * @param[out] None
  * @return     void
  */
void  CAN_IOT_ID_Get(uint8_t devId)
{ 
    /*判断源地址是否为IOT地址*/
    if (0 == Can_ID_Non_native_IOT_Check(devId))
    {
        /*查询地址是否已存储*/
        for ( uint8_t i = 0 ; i < IOT_CAN_ID_MAX_NUM ; i++ )
        {
            if ( devId == can_iot_id_sum[i] )
            {
                can_iot_id_timeout[i] = IOT_CAN_ID_TIMEOUT_S;
                return;
            }
        }

        /*未存储，寻找空位存储*/
        for ( uint8_t i = 0 ; i < IOT_CAN_ID_MAX_NUM ; i++ )
        {
            if ( 0 == can_iot_id_sum[i] )
            {
                can_iot_id_sum[i] = devId;
                can_iot_id_timeout[i] = IOT_CAN_ID_TIMEOUT_S;
                ESP_LOGW(TAG, "CAN_IOT_ID_Get : devId(0x%02x)", can_iot_id_sum[i]);
                iot_setting_update_flag = 1;
                return;
            }
        }
    }
}


/* define critical section */
static portMUX_TYPE data_lock = portMUX_INITIALIZER_UNLOCKED;
#define TASK_ENTER_CRITICAL()    	portENTER_CRITICAL(&data_lock)
#define TASK_EXIT_CRITICAL()     	portEXIT_CRITICAL(&data_lock)

/*------------------------------------------------------------------------------
 Function: iot_sumdev_task
 -----------------------------------------------------------------------------*/
/**
  * @brief 汇总读取设备数据
  * - 设备主要分为被锁定和未被锁定两种状态
  * - 1、被锁定是指被手机APP指定读取的设备
  * - 2、未锁定是指除锁定以外别的设备
  * - 3、设备分为PACK和逆变等
  * - 4、CAN总线处于OTA期间,不发起查询
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void iot_sumdev_task(void)
{
	#define SUMMARY_PERIOD	500		//数据汇总周期
	//#define TASK_CRITICAL_ENABLE        //锁线程使能
	static uint32_t pre_time = 0;

	/* 每隔一段时间汇总APP首页数据与PACK数据
	 * EMS控制策略中需要某些汇总后的数据 */
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if((now_time - pre_time) >= SUMMARY_PERIOD)
	{
	    if (modbus_data_semaphore_Take())
	    {
            pre_time = now_time;   

#ifdef TASK_CRITICAL_ENABLE
            TASK_ENTER_CRITICAL();
            Sum_CAN_To_Modbus_Read_Info_Process();
            TASK_EXIT_CRITICAL();
#else
            Sum_CAN_To_Modbus_Read_Info_Process();
#endif
            modbus_data_semaphore_Give();
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
	if ( 1 == reals.rtc_flag.sBit.RTC_set_IOT_to_INV )
	{
#if CONFIG_RTC_TIME_SOURCE_CHANGE == 3   
        static uint32_t pre_time = 0;
        uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if((now_time - pre_time) < 1000) return;

        /*周期发送时间帧*/
        bms_ann_now_time();
        pre_time = now_time;
#else
		if(can_cmd_queue == NULL)
		{
			ESP_LOGE (TAG, "can_cmd_queue null!");
			return;
		}
		/* 为can指令申请内存 */
		can_cmd_queue_struct can_cmd = {NULL, 5, 0};
		uint8_t can_cmd_flag = 0;

		can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
		if (!can_cmd.cmd) {
			ESP_LOGE (TAG, "RTC_Update_iot_to_can malloc failed");
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
		memcpy((uint8_t *)&can_node_wr.Inv.inv_set00.time1, (uint8_t *)&reals.rtc_time, sizeof(reals.rtc_time));
		memcpy((uint8_t *)&can_node_wr.Inv.inv_set00.res, (uint8_t *)&IotSetData.dev_info_t.SetTimeZone, sizeof(IotSetData.dev_info_t.SetTimeZone));
		ESP_LOGD (TAG, "RTC_Update_iot_to_can, data : ");
		ESP_LOG_BUFFER_HEX_LEVEL(TAG, &can_node_wr.Inv.inv_set00.time1, 8, ESP_LOG_DEBUG);
		
		/* 发送到队列 */
		if (can_cmd.cmd != NULL && can_cmd.num != 0) 
		{
			if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(0)) == pdTRUE) 
			{
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
#endif        
	}
}
