#include "app_param.h"
#include "ll_param.h"
#include "can_data.h"
#include "can_protocol.h"
#include "iot_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_period_task.h"
#include "filesystem.h"
#include "smart_tou_ctrl_v1.h"


static const char *TAG = "[APP_PARAM]";

#define LL_PARAM_SYNC_INTV			10000
#define LL_PARAM_SYNC_EVT_TIMEOUT	1000


#define TIME_MS_60S      	60000//慢速超时结束，10ms cycle,1.5s

EXT_RAM_BSS_ATTR ctrl_mix_t remote_ctrl1;
EXT_RAM_BSS_ATTR ctrl_mix_t local_ctrl1;
EXT_RAM_BSS_ATTR ctrl_mix2_t remote_ctrl2;
EXT_RAM_BSS_ATTR ctrl_mix2_t local_ctrl2;

/*
用于判断 各个CAN设备数量是否变化，SN是否变化
如果变化，需要给CAN设备更新 set参数

 * @return 无
 */
static void Num_SN_change_check(void)
{
	static int s_cmd_idx =0;
	static uint8_t sonline_Inv_num_old = 0;//
	static uint8_t sonline_Pack_num_old = 0;//
	static uint8_t sonline_Iot_num_old = 0;//
	static uint32_t old_time = 0;

	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;


	if((now_time > (old_time+TIME_MS_60S))
		||(now_time <old_time))//循环一圈
	{
		old_time = now_time;
		if(sonline_Inv_num_old != reals.online_Inv_num)
		{
			sonline_Inv_num_old = reals.online_Inv_num;
//			reals.inv_can_set_flag =1;	
		
		}
		
		if(sonline_Pack_num_old != reals.online_Pack_num)
		{
			sonline_Pack_num_old = reals.online_Pack_num;
//			reals.pack_can_set_flag =1; 
		
		}
		
		if(sonline_Iot_num_old != reals.online_Iot_num)
		{
			sonline_Iot_num_old = reals.online_Iot_num;
//			reals.iot_can_set_flag =1;	
			
		
		}

	}







}


/**
 * @brief 发送参数到底层设备
 *
 * @param data 数据指针
 * @param len 数据长度
 * @param can_cmd can指令结构体指针
 *
 * @return 0:成功, -1:失败
 */
static int app_can_send_param(uint8_t *data, uint32_t len, can_send_cmd_t *can_cmd)
{
	uint16_t crc16;
    uint8_t send_len;
    uint8_t seq = 0;
    uint8_t *p_data = data;
	uint32_t data_len = len;
	DataStruct can_data;

	extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];
	if (can_ota_status[can_cmd->node_id].period_cmd_delay > 0) return -1; //CAN-OTA直接返回

	crc16 = calcu_crc16(p_data, data_len);

	/* 首先发送写开始指令 */
	can_cmd->can_id.bit.funcode = WRIET_DATA_START;	/**< 写开始指令功能码 */
    if (!CanAckStart(can_cmd->node_id, can_cmd->can_id.all, can_cmd->data_type, can_cmd->offset, data_len, crc16)) {
        return -1;
    }

	/* 其次发送写数据指令 */
    while(data_len)
    {
        send_len = (data_len >= FRAME_DATA_BYTES) ? (FRAME_DATA_BYTES) : (data_len);
        can_data.type = can_cmd->data_type;
        can_data.seq = seq;
        memcpy(can_data.bytes, p_data, send_len);

		can_cmd->can_id.bit.funcode = WRITE_DATA;	/**< 写数据指令功能码 */
        if (!CanAckData(can_cmd->node_id, can_cmd->can_id.all, (uint8_t *)&can_data, (send_len + FRAME_HEAD))) {
			return -1;
		}

		data_len -= send_len;
		p_data += send_len;
		seq++;
    }

	return 0;
}

/**
 * @brief 从底层设备读取参数
 *
 * @param can_cmd can指令结构体指针
 *
 * @return 0:成功, -1:失败
 */
static int app_can_read_param(can_send_cmd_t *can_cmd)
{
	extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];
	if (can_ota_status[can_cmd->node_id].period_cmd_delay > 0) return -1; //CAN-OTA直接返回

	if (CanGetTarget(can_cmd->node_id,
					0,
					can_cmd->data_type,
					can_cmd->offset,
					can_cmd->data_len,
					can_cmd->can_id.bit.dst) != 1) return -1;

	 return 0;
}


/**
零散通用变量 Can set同步封装函数
 uint8_t data_type,
 uint8_t *data,
 uint32_t offset,
 uint32_t len



 * @return 无
 */
static void common_can_beta_data_block_set_IOT(uint8_t data_type,uint8_t *data,uint32_t offset, uint32_t len)
{
	for( int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < IOT_MAX_NUM; i++)
		{
			int online = Inv_can[node].iot_data[i].online;
			
			if (!online) continue;

			can_send_cmd_t can_cmd = {0};
			can_cmd.node_id = 0;

			can_cmd.offset = offset;
			can_cmd.can_id.bit.src = esp_canbus_myself_address();
			can_cmd.can_id.bit.dst = IOT_CAN_ADDR + + node*IOT_MAX_NUM +i;
			can_cmd.can_id.bit.priority = 0x02;
			can_cmd.data_type = data_type;

			app_can_send_param(data, len, &can_cmd);	// 发送指令
			ESP_LOGW(TAG, "common_can_beta_data_block_set_IOT, main_node: %d, sub_node: %d", node, i);
			vTaskDelay(pdMS_TO_TICKS(10));

			can_cmd.data_len = 0;
			app_can_read_param(&can_cmd);									// 回读数据
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}




/**
零散通用变量 Can set同步封装函数
 uint8_t data_type,
 uint8_t *data,
 uint32_t offset,
 uint32_t len



 * @return 无
 */
static void common_can_beta_data_block_set_INV(uint8_t data_type,uint8_t *data,uint32_t offset, uint32_t len)
{
	for( int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < INV_MAX_NUM; i++)
		{
			int online = Inv_can[node].inv_data[i].online;
			
			if (!online) continue;

			can_send_cmd_t can_cmd = {0};
			can_cmd.node_id = 0;

			can_cmd.offset = offset;
			can_cmd.can_id.bit.src = esp_canbus_myself_address();
			can_cmd.can_id.bit.dst = INV_CAN_ADDR + + node*INV_MAX_NUM +i;
			can_cmd.can_id.bit.priority = 0x02;
			can_cmd.data_type = data_type;

			app_can_send_param(data, len, &can_cmd);	// 发送指令
			ESP_LOGW(TAG, "common_can_beta_data_block_set_INV, main_node: %d, sub_node: %d", node, i);
			vTaskDelay(pdMS_TO_TICKS(10));

			can_cmd.data_len = 0;
			app_can_read_param(&can_cmd);									// 回读数据
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}




/**
零散通用变量 Can set同步封装函数
 uint8_t data_type,
 uint8_t *data,
 uint32_t offset,
 uint32_t len



 * @return 无
 */
static void common_can_beta_data_block_set_PACK(uint8_t data_type,uint8_t *data,uint32_t offset, uint32_t len)
{
	for( int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			int online = Inv_can[node].pack_data[i].online;
			
			if (!online) continue;

			can_send_cmd_t can_cmd = {0};
			can_cmd.node_id = 0;

			can_cmd.offset = offset;
			can_cmd.can_id.bit.src = esp_canbus_myself_address();
			can_cmd.can_id.bit.dst = PACK_CAN_ADDR + + node*PACK_MAX_NUM +i;
			can_cmd.can_id.bit.priority = 0x02;
			can_cmd.data_type = data_type;

			app_can_send_param(data, len, &can_cmd);	// 发送指令
			ESP_LOGW(TAG, "common_can_beta_data_block_set_PACK, main_node: %d, sub_node: %d", node, i);
			vTaskDelay(pdMS_TO_TICKS(10));

			can_cmd.data_len = 0;
			app_can_read_param(&can_cmd);									// 回读数据
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

/**
 * @brief 初始化逆变备份区参数
 *
 * @return 无
 */
static void bk_inv_param_init(void)
{
	pack_config_struct *bk_pack_config = &Inv_can_WR.bk_pack_dev_set.pack_config;
	memcpy(bk_pack_config, &SetData_Can.dev_info_t2.pack_config, sizeof(pack_config_param_t));

	/* 获取本地参数 */

	/* 初始化本地参数到ble modbus备份区 */
	inv_set00_struct *bk_inv_base = &Inv_can_WR.bk_inv_dev_set.inv_set00;
	inv_set01_struct *bk_inv_advance = &Inv_can_WR.bk_inv_dev_set.inv_set01;
	inv_set03_struct *bk_inv_auth = &Inv_can_WR.bk_inv_dev_set.inv_set03;
	memcpy(bk_inv_base, &SetData_Can.dev_info_t2.inv_set00, offsetof(inv_base_param_t, nc));
	memcpy(bk_inv_advance, &SetData_Can.dev_info_t2.inv_set01, offsetof(inv_advance_param_t, res1));
	memcpy(bk_inv_auth, &SetData_Can.dev_info_t2.inv_set03, offsetof(inv_auth_param_t, nc5));
}

/**
 * @brief 初始化PACK备份区参数
 *
 * @return 无
 */
static void bk_pack_param_init(void)
{
	/* 获取本地参数 */

	/* 初始化本地参数到ble modbus备份区 */
	pack_config_struct *bk_pack_config = &Inv_can_WR.bk_pack_dev_set.pack_config;
	memcpy(bk_pack_config, &SetData_Can.dev_info_t2.pack_config, sizeof(pack_config_param_t));
}

/**
 * @brief 备份区参数初始化
 *
 * @return 无
 */
static void bk_param_init(void)
{
	if(INV_EPROM_READY_FLAG == SetData_Can.dev_info_t2.valid_inv) 
	{
		bk_inv_param_init();
	}
	Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.all = 0;
	Inv_can_WR.bk_inv_dev_set.inv_set01.ct_test.ct_enable = 0;
	Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_mix.bit.reset_param = 0;
	Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_reset_factory = 0;

	if(PACK_EPROM_READY_FLAG == SetData_Can.dev_info_t2.valid_pack)
	{
		bk_pack_param_init();
	}
	
}


/**
 * @brief 使用其他IOT的参数初始化本地参数
 *
 * @return 无
 */
static void app_param_init_from_iot(void)
{
	for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < IOT_MAX_NUM; i++)
		{
			int online = Inv_can[node_id].iot_data[i].online;
			
			if((0 == online) 
				||(0b11011 != (Inv_can[node_id].iot_data[i].setdata_valid&0b11011)))//需要等待下级set数据全部获取后才能更新
			{
				continue;
			}
				

//			iot_can_node_struct_reg12000 *p_reg12000_IOT_set = &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set;


			if(node_id != reals.Addr_can_self)
			{
				continue;
			}
			/* 保存参数 */


			memcpy(&SetData.dev_info_t.factory, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.factory, sizeof(inv_set03_0x1D_struct_mini));
			memcpy(&SetData.dev_info_t.wifi_sta_auth, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.wifi_sta_auth, sizeof(inv_set03_0x1D_struct_mini));
			memcpy(&SetData.dev_info_t.wifi_sta_ssid[0], &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.wifi_sta_ssid[0], 32);
			memcpy(&SetData.dev_info_t.wifi_sta_password[0], &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.wifi_sta_password[0],64);
			memcpy(&SetData.dev_info_t.sta_enable, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.sta_enable, sizeof(sta_eanble_t));

			memcpy(&SetData.dev_info_t.revd1[0], &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.revd1[0], 32);
			memcpy(&SetData.dev_info_t.could_dns[0], &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.could_dns[0], 128);
			memcpy(&SetData.dev_info_t.mobile_apn[0], &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.mobile_apn[0], 60);
			memcpy(&SetData.dev_info_t.on_off, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.on_off, sizeof(config_status));
			memcpy(&SetData.dev_info_t.thunder_ctrl, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.thunder_ctrl, sizeof(thunder_ctrl_t));


			memcpy(&SetData.dev_info_t.period_report, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.period_report, 4);
			memcpy(&SetData.dev_info_t.IOT_Enable_mix1, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.IOT_Enable_mix1, sizeof(Struct_IOT_Enable_mix1));
			memcpy(&SetData.dev_info_t.IOT_Enable_mix2, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.IOT_Enable_mix2, 2);
			memcpy(&SetData.dev_info_t.Protocol_3r_Enable_mix1, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.Protocol_3r_Enable_mix1, 2);
			memcpy(&SetData.dev_info_t.Protocol_3r_Enable_mix2, &Inv_can[node_id].iot_data[i].mod_reg12000_IOT_set.Protocol_3r_Enable_mix2, 2);

			
			SetData.dev_info_t.valid_iot =IOT1EPROM_READY_FLAG;
			reals.flasWrFlag.sBit.set_data_inv =1;
			
			ESP_LOGW(TAG, "initialized inv parameters");
//			return;
		}
	}
}


/**
 * @brief 使用逆变器的参数初始化本地参数
 *
 * @return 无
 */
static void app_param_init_from_inv(void)
{
	for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			int online = Inv_can[node_id].inv_data[i].online;
			
			if((0 == online) 
				||(0b11011 != (Inv_can[node_id].inv_data[i].setdata_valid&0b11011)))//需要等待下级set数据全部获取后才能更新
			{
				continue;
			}

			inv_set00_struct *p_inv_base = &Inv_can[node_id].inv_data[i].inv_set00;
			inv_set01_struct *p_inv_advance = &Inv_can[node_id].inv_data[i].inv_set01;
			inv_set03_struct *p_inv_auth = &Inv_can[node_id].inv_data[i].inv_set03;

			/* 获取本地参数 */
			inv_base_param_t *base_param = &SetData_Can.dev_info_t2.inv_set00;//ll_inv_param_get("inv_base_param");
			inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
			inv_auth_param_t *auth_param = &SetData_Can.dev_info_t2.inv_set03;//ll_inv_param_get("inv_auth_param");


			/* 判断所有参数是否已从总线上获取完成 */
//			uint8_t zero[512] = {0};
			if(memcmp(p_inv_base, reals.zero, offsetof(inv_set00_struct, ctrl_inv_mode)) == 0) continue;
			if(memcmp(p_inv_advance, reals.zero, offsetof(inv_set01_struct, ctrl_feedback_max_curr)) == 0) continue;
			if(memcmp(p_inv_auth, reals.zero, offsetof(inv_set03_struct, nc5)) == 0) continue;

			memcpy(&Inv_can_WR.bk_inv_dev_set.inv_set00, &Inv_can[node_id].inv_data[i].inv_set00, sizeof(inv_set00_0x1A_struct_mini));
			memcpy(&Inv_can_WR.bk_inv_dev_set.inv_set01, &Inv_can[node_id].inv_data[i].inv_set01, sizeof(inv_set01_0x1B_struct_mini));
			memcpy(&Inv_can_WR.bk_inv_dev_set.inv_set03, &Inv_can[node_id].inv_data[i].inv_set03, sizeof(inv_set03_0x1D_struct_mini));


			if(node_id != reals.Addr_can_self)
			{
				continue;
			}
			memcpy(&SetData_Can.dev_info_t2.inv_set00, &Inv_can[node_id].inv_data[i].inv_set00, sizeof(inv_set00_0x1A_struct_mini));
			memcpy(&SetData_Can.dev_info_t2.inv_set01, &Inv_can[node_id].inv_data[i].inv_set01, sizeof(inv_set01_0x1B_struct_mini));
			memcpy(&SetData_Can.dev_info_t2.inv_set03, &Inv_can[node_id].inv_data[i].inv_set03, sizeof(inv_set03_0x1D_struct_mini));
			/* 保存参数 */
			base_param->ctrl.all = 0;
			base_param->ctrl_log_page = 0;
			base_param->ctrl_all_energy_type = 0;
			base_param->ctrl_now_energy_type = 0;
			advance_param->ct_test.ct_enable = 0;
			advance_param->ctrl_mix.bit.reset_param = 0;
			advance_param->ctrl_reset_factory = 0;

			base_param->time1 = 0;
			base_param->time2 = 0;
			base_param->time3 = 0;

			
			SetData_Can.dev_info_t2.valid_inv =INV_EPROM_READY_FLAG;
			reals.flasWrFlag.sBit.set_data_inv =1;
			
			ESP_LOGW(TAG, "initialized inv parameters");
//			return;
		}
	}
}

/**
 * @brief 使用PACK的参数初始化本地参数
 *
 * @return 无
 */
static void app_param_init_from_pack(void)
{
	for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < PACK_MAX_NUM; i++)
		{
			int online = Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i);
			
			if(!online) continue;

			pack_config_struct *p_pack_config = &Inv_can[node_id].pack_data[i].pack_config;

			/* 判断所有参数是否已从总线上获取完成 */
//			uint8_t zero[512] = {0};
			if(memcmp(p_pack_config, reals.zero, offsetof(pack_config_struct, rw_cmd)) == 0) continue;


			memcpy(&Inv_can_WR.bk_pack_dev_set.pack_config, &Inv_can[node_id].pack_data[i].pack_config, sizeof(pack_config_0x55_struct_mini));


			if((node_id != reals.Addr_can_self)
				&&(0 == i))//暂定只从主bat获取set
			{
				continue;
			}

			/* 保存参数 */
			memcpy(&SetData_Can.dev_info_t2.pack_config, &Inv_can[node_id].pack_data[i].pack_config, sizeof(pack_config_0x55_struct_mini));
			
			SetData_Can.dev_info_t2.valid_pack =PACK_EPROM_READY_FLAG;
			reals.flasWrFlag.sBit.set_data_pack =1;
			
			ESP_LOGI(TAG, "initialized pack parameters");
			return;
		}
	}
}
//
///**
// * @brief 同步本地参数到逆变设备
// * - APP下发的设置参数首先会保存到本地，之后再同步到每台逆变设备
// *
// * @return 无
// */
//static void app_param_sync_to_inv(void)
//{
//	static uint8_t sTypecnt=0;
//	uint8_t zero[512] = {0};
//	int param_len=0;
//
//	for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
//	{
//		for(int i = 0; i < INV_MAX_NUM; i++)
//		{
//			int online = Inv_can[node_id].inv_data[i].online;
//			
//			if(!online) continue;
//
//
//			inv_set00_struct *p_inv_base = &Inv_can[node_id].inv_data[i].inv_set00;
//			inv_set01_struct *p_inv_advance = &Inv_can[node_id].inv_data[i].inv_set01;
//			inv_set03_struct *p_inv_auth = &Inv_can[node_id].inv_data[i].inv_set03;
//
//			/* 获取本地参数 */
//			inv_base_param_t *base_param = &SetData_Can.dev_info_t2.inv_set00;//ll_inv_param_get("inv_base_param");
//			inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
//			inv_auth_param_t *auth_param = &SetData_Can.dev_info_t2.inv_set03;//ll_inv_param_get("inv_auth_param");
//
//			
//			if(base_param == NULL || advance_param == NULL || auth_param == NULL)
//			{
//				/* 清除初始化标志 */
////				ll_param_flag_t param_flag;
////				param_flag.pack_init_flag = ll_param_flag_get()->pack_init_flag;
////				param_flag.inv_init_flag = 0;
////				ll_param_flag_set(&param_flag);
////				ESP_LOGE(TAG, "synchronize inv parameters failed, the parameters are not initialized");
////				return;
//			}
//
//			can_send_cmd_t can_cmd = {0};
//			can_cmd.node_id = node_id;
//			can_cmd.can_id.bit.src = esp_canbus_myself_address();
//			can_cmd.can_id.bit.dst = INV_CAN_ADDR + i;
//			can_cmd.can_id.bit.priority = 0x02;
//
//			if (1 == sTypecnt)							// 同步基本参数到该逆变设备
//			{
//				/* 判断参数是否已从总线上获取完成 */
//				if(0 != memcmp(p_inv_base, zero, offsetof(inv_set00_struct, nc))) //非零
//				{
//					param_len = offsetof(inv_base_param_t, nc);				
//					if(memcmp(base_param, p_inv_base, param_len) != 0)
//					{
//						ESP_LOGW(TAG, "synchronize inv base parameters, main_node: %d, sub_node: %d", node_id, i);
//						can_cmd.data_type = INV_TYPE_CONFIG00;
//						if ((base_param->time1 != p_inv_base->time1) ||
//							(base_param->time2 != p_inv_base->time2) ||
//							(base_param->time3 != p_inv_base->time3))
//						{
//							can_cmd.offset = 0;
//							app_can_send_param((uint8_t*)base_param+can_cmd.offset, param_len, &can_cmd);
//							vTaskDelay(pdMS_TO_TICKS(10));
//					
//							can_cmd.data_len = 0;
//							app_can_read_param(&can_cmd);
//						}
//						else { // 当时间未修改时从led设置处开始同步参数
//							can_cmd.offset = offsetof(inv_base_param_t, ctrl_led);
//							app_can_send_param((uint8_t*)base_param+can_cmd.offset, param_len-can_cmd.offset, &can_cmd);
//							vTaskDelay(pdMS_TO_TICKS(10));
//					
//							can_cmd.data_len = 0;
//							app_can_read_param(&can_cmd);
//						}
//						vTaskDelay(pdMS_TO_TICKS(50));
//					}
//				}
//
//
//				
//
//			}
//			else if(2 == sTypecnt)					// 同步高级参数到该逆变设备
//			{
//				/* 判断参数是否已从总线上获取完成 */
//			
//				if(0 != memcmp(p_inv_advance, zero, offsetof(inv_set01_struct, res1)))
//				{
//					param_len = offsetof(inv_advance_param_t, res1);
//					if(memcmp(advance_param, p_inv_advance, param_len) != 0)
//					{
//						ESP_LOGW(TAG, "synchronize inv advance parameters, main_node: %d, sub_node: %d", node_id, i);
//						can_cmd.data_type = INV_TYPE_CONFIG01;
//						can_cmd.offset = 0;
//						app_can_send_param((uint8_t*)advance_param+can_cmd.offset, param_len, &can_cmd);
//						vTaskDelay(pdMS_TO_TICKS(10));
//					
//						can_cmd.data_len = 0;
//						app_can_read_param(&can_cmd);
//						vTaskDelay(pdMS_TO_TICKS(50));
//					}
//				}
//			
//
//			}
//			else if (3 == sTypecnt)						// 同步认证参数到该逆变设备
//			{
//				/* 判断参数是否已从总线上获取完成 */
//				if(0 != memcmp(p_inv_auth, zero, offsetof(inv_set03_struct, nc5)))
//				{
//					param_len = offsetof(inv_auth_param_t, nc5);
//					if(memcmp(auth_param, p_inv_auth, param_len) != 0)
//					{
//						ESP_LOGW(TAG, "synchronize inv auth parameters, main_node: %d, sub_node: %d", node_id, i);
//						can_cmd.data_type = INV_TYPE_CONFIG03;
//						can_cmd.offset = 0;
//						app_can_send_param((uint8_t*)auth_param, param_len, &can_cmd);
//						vTaskDelay(pdMS_TO_TICKS(10));
//					
//						can_cmd.data_len = 0;
//						app_can_read_param(&can_cmd);
//						vTaskDelay(pdMS_TO_TICKS(50));
//					}
//				}	
//			
//
//			}
//		}
//	}
//
//	if(++sTypecnt >= 3)
//	{
//		sTypecnt=0;
//
//	}	
//}

#define	IOT_SET_TYPE_SUM	1
#define	INV_SET_TYPE_SUM	3
#define	PACK_SET_TYPE_SUM	1

/*
非阻塞，主循环一次，只进行一次操作
广播，相同set

 * @brief 同步本地参数到逆变设备
 * - APP下发的设置参数首先会保存到本地，之后再同步到每台逆变设备
 *
windy:AC380项目约定，IOT不参与 INV之间ARMd/PACK的参数同步，
IOT只负责参数备份和来自手机等上级的更新同步备份
 
 * @return 无
 */
static void app_param_sync_to_inv_0xFF(void)
{
	static int s_cmd_idx =0;
	static int s_x_idx =0;
	static int s_y_idx =0;
	int param_len=0;
//	uint8_t zero[512] = {0};
	
	int cmd_idx = 0;
	int node = 0;
	int i = 0;	


	inv_set00_struct *p_inv_base = &Inv_can[reals.Addr_can_self].inv_data[0].inv_set00;
	inv_set01_struct *p_inv_advance = &Inv_can[reals.Addr_can_self].inv_data[0].inv_set01;
	inv_set03_struct *p_inv_auth = &Inv_can[reals.Addr_can_self].inv_data[0].inv_set03;
	
	/* 获取本地参数 */
	inv_base_param_t *base_param = &SetData_Can.dev_info_t2.inv_set00;//ll_inv_param_get("inv_base_param");
	inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
	inv_auth_param_t *auth_param = &SetData_Can.dev_info_t2.inv_set03;//ll_inv_param_get("inv_auth_param");


	can_send_cmd_t can_cmd = {0};
	can_cmd.node_id = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.dst = 0xFF;//INV_CAN_ADDR + i;广播set
	can_cmd.can_id.bit.priority = 0x02;


	node=s_x_idx;			
	i=s_y_idx;
	cmd_idx=s_cmd_idx;				

//	inv_poll_id =node*INV_MAX_NUM +i;
	int online = Inv_can[node].inv_data[i].online;
	if (0 == cmd_idx)							// 同步基本参数到该逆变设备
	{
		/* 判断参数是否已从总线上获取完成 */
		if(0 != memcmp(p_inv_base, reals.zero, offsetof(inv_set00_struct, nc))) //非零
		{
			param_len = offsetof(inv_base_param_t, nc); 			
			if(memcmp(base_param, p_inv_base, param_len) != 0)
			{
				ESP_LOGI(TAG, "synchronize inv base parameters, main_node: %d, sub_node: %d", node, i);
				can_cmd.data_type = INV_TYPE_CONFIG00_1AH;
				if (((0 != base_param->time1)||(0 != base_param->time1)||(0 != base_param->time1))//全非0
					&&((base_param->time1 != p_inv_base->time1) ||
						(base_param->time2 != p_inv_base->time2) ||
						(base_param->time3 != p_inv_base->time3)))
				{
					can_cmd.offset = 0;
					app_can_send_param((uint8_t*)base_param+can_cmd.offset, param_len, &can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(10));
//			
//					can_cmd.data_len = 0;
//					app_can_read_param(&can_cmd);
				}
				else { // 当时间未修改时从led设置处开始同步参数
					can_cmd.offset = offsetof(inv_base_param_t, ctrl_led);
					app_can_send_param((uint8_t*)base_param+can_cmd.offset, param_len-can_cmd.offset, &can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(10));
//			
//					can_cmd.data_len = 0;
//					app_can_read_param(&can_cmd);
				}
//				vTaskDelay(pdMS_TO_TICKS(50));
			}
		}
	
	
		
	
	}
	else if(1 == cmd_idx)					// 同步高级参数到该逆变设备
	{
		/* 判断参数是否已从总线上获取完成 */
	
		if(0 != memcmp(p_inv_advance, reals.zero, offsetof(inv_set01_struct, res1)))
		{
			param_len = offsetof(inv_advance_param_t, res1);
			if(memcmp(advance_param, p_inv_advance, param_len) != 0)
			{
				ESP_LOGW(TAG, "synchronize inv advance parameters, main_node: %d, sub_node: %d", node, i);
				can_cmd.data_type = INV_TYPE_CONFIG01_1BH;
				can_cmd.offset = 0;
				app_can_send_param((uint8_t*)advance_param+can_cmd.offset, param_len, &can_cmd);
//				vTaskDelay(pdMS_TO_TICKS(10));
//			
//				can_cmd.data_len = 0;
//				app_can_read_param(&can_cmd);
//				vTaskDelay(pdMS_TO_TICKS(50));
			}
		}
	
	
	}
	else if (2 == cmd_idx) 					// 同步认证参数到该逆变设备
	{
		/* 判断参数是否已从总线上获取完成 */
		if(0 != memcmp(p_inv_auth, reals.zero, offsetof(inv_set03_struct, nc5)))
		{
			param_len = offsetof(inv_auth_param_t, nc5);
			if(memcmp(auth_param, p_inv_auth, param_len) != 0)
			{
				ESP_LOGW(TAG, "synchronize inv auth parameters, main_node: %d, sub_node: %d", node, i);
				can_cmd.data_type = INV_TYPE_CONFIG03_1DH;
				can_cmd.offset = 0;
				app_can_send_param((uint8_t*)auth_param, param_len, &can_cmd);
//				vTaskDelay(pdMS_TO_TICKS(10));
//			
//				can_cmd.data_len = 0;
//				app_can_read_param(&can_cmd);
//				vTaskDelay(pdMS_TO_TICKS(50));
			}
		}	
	
	
	}


	//y->x->can type
	if(++s_y_idx >= INV_MAX_NUM)
	{
		s_y_idx =0;
		if(++s_x_idx >= 1)//DEV_MAIN_NODE_MAX
		{
			s_x_idx =0;
			if(++s_cmd_idx >= INV_SET_TYPE_SUM)
			{
				s_cmd_idx =0;
				reals.inv_can_set_flag =0;
				
			}
		}				
	}


}

typedef struct
{
	uint8_t main_node;
	uint8_t sub_node;
	char *name;
	char *type;
	uint8_t *data;
	uint32_t len;
	uint8_t sync_flag;
} sync_param_t;

//#define SYNC_PARAM_SET(_param, _name, _data, _len)	
//_param->name = _name;							

#define SYNC_PARAM_SET(_param, _data, _len)	\	
	_param->data = (uint8_t*)_data;					\
	_param->len = _len;

/**
 * @brief 同步参数到逆变设备
 *
 * @param sync_param 参数数据结构
 * @param offset 参数偏移
 * @param cmd can指令数据结构
 * @return 无
 */
static void inv_param_sync(sync_param_t *sync_param, uint32_t offset, can_send_cmd_t *cmd)
{
//	ESP_LOGW(TAG, "sync inv %s: %s, main_node: %d, sub_node: %d", sync_param->type, sync_param->name, sync_param->main_node, sync_param->sub_node);
	ESP_LOGW(TAG, "AAA sync inv %s: offset= %ld, len= %ld, main_node: %d, sub_node: %d", sync_param->type,offset,sync_param->len ,  sync_param->main_node, sync_param->sub_node);	
	cmd->offset = offset;
	app_can_send_param((uint8_t*)sync_param->data, sync_param->len, cmd);
	vTaskDelay(pdMS_TO_TICKS(10));
	sync_param->sync_flag = 1;
}

/**
 * @brief 同步基本参数到逆变设备
 *
 * @param sync_param 参数数据结构
 * @param local 本地参数指针
 * @param remote 远端逆变参数指针
 * @param cmd can指令数据结构
 * @return 无
 */
static void inv_base_param_sync_0x1A(sync_param_t *sync_param, inv_base_param_t *local, inv_set00_struct *remote, can_send_cmd_t *cmd)
{
	cmd->data_type = INV_TYPE_CONFIG00_1AH;
	sync_param->type = "base param";
	sync_param->sync_flag = 0;

	if (local->work_mode != remote->work_mode)					// 同步工作模式
	{
		SYNC_PARAM_SET(sync_param, &local->work_mode, sizeof(local->work_mode));
		
		inv_param_sync(sync_param, offsetof(inv_base_param_t, work_mode), cmd);
		ESP_LOGI(TAG, "inv_base_param_sync_0x1A ==  1 ,");	
		
	}

	else if (local->ctrl_meter != remote->ctrl_meter)				// 同步电表开关
	{
		SYNC_PARAM_SET(sync_param, &local->ctrl_meter, sizeof(local->ctrl_meter));
		inv_param_sync(sync_param, offsetof(inv_base_param_t, ctrl_meter), cmd);
		ESP_LOGI(TAG, "inv_base_param_sync_0x1A ==  2 ,");	
		
	}

	else if (local->ctrl_low_cap_pct != remote->ctrl_low_cap_pct)	// 同步低电量百分比
	{
		SYNC_PARAM_SET(sync_param,  &local->ctrl_low_cap_pct, sizeof(local->ctrl_low_cap_pct));
		inv_param_sync(sync_param, offsetof(inv_base_param_t, ctrl_low_cap_pct), cmd);
		ESP_LOGI(TAG, "inv_base_param_sync_0x1A == 3  ,");	
		
	}

	else if (local->ctrl_high_cap_pct != remote->ctrl_high_cap_pct)	// 同步高电量百分比
	{
		SYNC_PARAM_SET(sync_param,  &local->ctrl_high_cap_pct, sizeof(local->ctrl_high_cap_pct));
		inv_param_sync(sync_param, offsetof(inv_base_param_t, ctrl_high_cap_pct), cmd);
		ESP_LOGI(TAG, "inv_base_param_sync_0x1A ==  4 ,");	
		
	}

	else if (local->ctrl_alarm_voice != remote->ctrl_alarm_voice)	// 同步报警声音开关, windy inv value=0
	{
		SYNC_PARAM_SET(sync_param,  &local->ctrl_alarm_voice, sizeof(local->ctrl_alarm_voice));
		inv_param_sync(sync_param, offsetof(inv_base_param_t, ctrl_alarm_voice), cmd);
		ESP_LOGI(TAG, "inv_base_param_sync_0x1A == 5  ,local->ctrl_alarm_voice =%d,remote->ctrl_alarm_voice=%d",local->ctrl_alarm_voice,remote->ctrl_alarm_voice);	
		
	}

	else if(memcmp(&local->ctrl_time_area, &remote->ctrl_time_area, sizeof(local->ctrl_time)+sizeof(local->ctrl_time_area)) != 0)	// 同步充放电时段设置
	{
		SYNC_PARAM_SET(sync_param,  &local->ctrl_time_area, sizeof(local->ctrl_time)+sizeof(local->ctrl_time_area));
		inv_param_sync(sync_param, offsetof(inv_base_param_t, ctrl_time_area), cmd);
		ESP_LOGI(TAG, "inv_base_param_sync_0x1A ==  6 ,");	
		
	}

	if (sync_param->sync_flag)
	{
		sync_param->sync_flag =0;
		cmd->offset = 0;
		cmd->data_len = 0;
//		app_can_read_param(cmd);
//		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

enum {
	PEAK_CHK_IDLE,
	PEAK_CHK_START,
	PEAK_CHK_RUN,
};

static int inv_peak_chk = PEAK_CHK_IDLE;	// 参数极值检测状态
static int inv_peak_set = 0;				// 参数极值设置标志

/**
 * @brief 极值参数处理
 * 当检测到逆变器的参数发生变化时,使用逆变器参数覆盖本地参数
 *
 * @return 0:检测进行中, 1:参数处理完成
 */
static int inv_peak_param_handle(void)
{
	for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < INV_MAX_NUM; i++)
		{
			if (!Inv_can[node_id].inv_data[i].online) continue;

			inv_set01_struct *remote_param = &Inv_can[node_id].inv_data[i].inv_set01;
			inv_advance_param_t *local_param = &SetData_Can.dev_info_t2.inv_set01;

			if (inv_peak_set == 0)
			{
				if ((remote_param->ctrl_grid_max_power == local_param->ctrl_grid_max_power) &&
					(remote_param->ctrl_grid_max_curr == local_param->ctrl_grid_max_curr) &&
					(remote_param->ctrl_feedback_max_power == local_param->ctrl_feedback_max_power) &&
					(remote_param->ctrl_feedback_max_curr == local_param->ctrl_feedback_max_curr)) {
					inv_peak_set = 1;
				}
				return 0;
			}

			if ((remote_param->ctrl_grid_max_power != local_param->ctrl_grid_max_power) ||
				(remote_param->ctrl_grid_max_curr != local_param->ctrl_grid_max_curr) ||
				(remote_param->ctrl_feedback_max_power != local_param->ctrl_feedback_max_power) ||
				(remote_param->ctrl_feedback_max_curr != local_param->ctrl_feedback_max_curr))
			{
				inv_advance_param_t inv_advance_param;
				memcpy(&inv_advance_param, local_param, offsetof(inv_advance_param_t, res1));
				inv_advance_param.ctrl_grid_max_power = remote_param->ctrl_grid_max_power;
				inv_advance_param.ctrl_grid_max_curr = remote_param->ctrl_grid_max_curr;
				inv_advance_param.ctrl_feedback_max_power = remote_param->ctrl_feedback_max_power;
				inv_advance_param.ctrl_feedback_max_curr = remote_param->ctrl_feedback_max_curr;
				Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_grid_max_power = remote_param->ctrl_grid_max_power;
				Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_grid_max_curr = remote_param->ctrl_grid_max_curr;
				Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_feedback_max_power = remote_param->ctrl_feedback_max_power;
				Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_feedback_max_curr = remote_param->ctrl_feedback_max_curr;
				reals.flasWrFlag.sBit.set_data_inv =1;
				return 1;
			}
			return 0;
		}
	}

	return 0;
}

/**
 * @brief 极值参数检查
 * 当APP设置的参数超过逆变器阈值时,该函数使用逆变器的参数覆盖本地参数
 *
 * @return 0:空闲, 1:正在进行极值检测
 */
static int inv_peak_param_check(void)
{
	#define MAX_PEAK_CHK_TIME	12000	// 每次极值检测最多12秒
	static uint32_t last_time = 0;
	int ret = 0;

	if (inv_peak_chk == PEAK_CHK_IDLE)
	{
		last_time = 0;
		goto __exit;
	}
	else if(inv_peak_chk == PEAK_CHK_START)
	{
		last_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
		inv_peak_chk = PEAK_CHK_RUN;
		inv_peak_set = 0;
		ret = 1;
	}
	else if(inv_peak_chk == PEAK_CHK_RUN)
	{
		ret = 1;
		uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
		if ((now_time - last_time) >= MAX_PEAK_CHK_TIME)
		{
			inv_peak_chk = PEAK_CHK_IDLE;
			goto __exit;
		}

		if (inv_peak_param_handle()) {
			inv_peak_chk = PEAK_CHK_IDLE;
		}
	}

__exit:
	return ret;
}

/**
 * @brief 同步高级参数到逆变设备
 *
 * @param sync_param 参数数据结构
 * @param local 本地参数指针
 * @param remote 远端逆变参数指针
 * @param cmd can指令数据结构
 * @return 无
 */
static void inv_advance_param_sync_0x1B(sync_param_t *sync_param, inv_advance_param_t *local, inv_set01_struct *remote, can_send_cmd_t *cmd)
{
	cmd->data_type = INV_TYPE_CONFIG01_1BH;
	sync_param->type = "advance param";
	sync_param->sync_flag = 0;

	if (local->ctrl_save_power_state != remote->ctrl_save_power_state)					// 同步记忆功能
	{
		SYNC_PARAM_SET(sync_param,  &local->ctrl_save_power_state, sizeof(local->ctrl_save_power_state));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_save_power_state), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==   1,");	
		
	}

	else if (local->ctrl_grid != remote->ctrl_grid)											// 同步电网开关
	{
		SYNC_PARAM_SET(sync_param,   &local->ctrl_grid, sizeof(local->ctrl_grid));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_grid), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  2 ,");	
		
	}

	else if (local->ctrl_feedback != remote->ctrl_feedback)									// 同步馈电开关, windy inv ctrl_feedback=0
	{
		SYNC_PARAM_SET(sync_param,   &local->ctrl_feedback, sizeof(local->ctrl_feedback));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_feedback), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  3 ,local->ctrl_feedback=%d,remote->ctrl_feedback=%d",local->ctrl_feedback,remote->ctrl_feedback);	
		
	}

	else if (local->ctrl_output_inv_volt != remote->ctrl_output_inv_volt)					// 同步电压等级
	{
		SYNC_PARAM_SET(sync_param,  &local->ctrl_output_inv_volt, sizeof(local->ctrl_output_inv_volt));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_output_inv_volt), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  4 ,");	
		
	}

	else if (local->ctrl_output_inv_freq != remote->ctrl_output_inv_freq)					// 同步频率等级
	{
		SYNC_PARAM_SET(sync_param,   &local->ctrl_output_inv_freq, sizeof(local->ctrl_output_inv_freq));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_output_inv_freq), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  5 ,");	
		
	}

	else if (local->ctrl_grid_plus != remote->ctrl_grid_plus)								// 同步电网增强模式
	{
		SYNC_PARAM_SET(sync_param,   &local->ctrl_grid_plus, sizeof(local->ctrl_grid_plus));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_grid_plus), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  6 ,");	
		
	}

	// if (local->ctrl_Inv_Multi_enable != remote->ctrl_Inv_Multi_enable)					// 同步电网侧并机使能
	// {
	// 	SYNC_PARAM_SET(sync_param, "grid parallel enable", &local->ctrl_mix, sizeof(local->ctrl_mix));
	// 	inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_mix), cmd);
	// }

	else if (local->ctrl_grid_max_curr != remote->ctrl_grid_max_curr)						// 同步电网最大输入电流
	{
		SYNC_PARAM_SET(sync_param,   &local->ctrl_grid_max_curr, sizeof(local->ctrl_grid_max_curr));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_grid_max_curr), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  7 ,");	
		
	}

	else if (local->ctrl_feedback_max_curr != remote->ctrl_feedback_max_curr)				// 同步电网最大馈电电流
	{
		SYNC_PARAM_SET(sync_param,  &local->ctrl_feedback_max_curr, sizeof(local->ctrl_feedback_max_curr));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_feedback_max_curr), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==   8,");	
		
	}

	else if (local->ctrl_grid_max_power != remote->ctrl_grid_max_power)						// 同步电网最大输入功率
	{
		SYNC_PARAM_SET(sync_param,   &local->ctrl_grid_max_power, sizeof(local->ctrl_grid_max_power));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_grid_max_power), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  9 ,");	
		
	}

	else if (local->ctrl_feedback_max_power != remote->ctrl_feedback_max_power)				// 同步电网最大馈电功率
	{
		SYNC_PARAM_SET(sync_param,   &local->ctrl_feedback_max_power, sizeof(local->ctrl_feedback_max_power));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_feedback_max_power), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  10 ,");	
		
	}

	remote_ctrl1.all = remote->ctrl_mix.all;
	local_ctrl1.all = local->ctrl_mix.all;
	for (int i = 0; i < 8; i++)
	{
		/* 本地为0或为3的开关设置不参与比较 */
		if ((((local_ctrl1.all >> (i*2)) & 0x03) == 0) || (((local_ctrl1.all >> (i*2)) & 0x03) == 3))
		{
			local_ctrl1.all &= (uint16_t)(~(0x03 << (i*2)));
			local_ctrl1.all |= (remote_ctrl1.all & (0x03 << (i*2)));
		}
	}
	remote_ctrl1.bit.reset_param = 0;	// 该参数不参与比较
	local_ctrl1.bit.reset_param = 0;
	if (local_ctrl1.all != remote_ctrl1.all)											// 同步杂项控制1
	{
		SYNC_PARAM_SET(sync_param,   &local_ctrl1.all, sizeof(local_ctrl1.all));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_mix), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  11 ,");	
		
	}


	remote_ctrl2.all = remote->ctrl_mix2.all;
	local_ctrl2.all = local->ctrl_mix2.all;
	for (int i = 0; i < 8; i++)
	{
		/* 本地为0或为3的开关设置不参与比较 */
		if ((((local_ctrl2.all >> (i*2)) & 0x03) == 0) || (((local_ctrl2.all >> (i*2)) & 0x03) == 3))
		{
			local_ctrl2.all &= (uint16_t)(~(0x03 << (i*2)));
			local_ctrl2.all |= (local_ctrl2.all & (0x03 << (i*2)));
		}
	}
	if (local_ctrl2.all != remote_ctrl2.all)									// 同步杂项控制2
	{
		SYNC_PARAM_SET(sync_param,   &local_ctrl2.all, sizeof(local_ctrl2.all));
		inv_param_sync(sync_param, offsetof(inv_advance_param_t, ctrl_mix2), cmd);
		ESP_LOGI(TAG, "inv_advance_param_sync_0x1B ==  12 ,");	
		
	}

	if (sync_param->sync_flag)
	{
	
		sync_param->sync_flag =0;
		cmd->offset = 0;
		cmd->data_len = 0;
//		app_can_read_param(cmd);
//		vTaskDelay(pdMS_TO_TICKS(50));
	}
}
//
//
//
///**
// * @brief 逆变参数查询与同步
// * - APP下发的设置参数首先会保存到本地，之后再同步到每台逆变设备
// *
// * @return 无
// */
//static void inv_param_check_and_sync(void)
//{
//	static uint32_t old_time = 0;
//	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
//	static uint8_t sTypecnt=0;
//
//	if (inv_peak_param_check()) return;
//	if((now_time - old_time) < LL_PARAM_SYNC_INTV) return;
//	old_time = now_time;
//
//
//
//	for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
//	{
//		for(int i = 0; i < INV_MAX_NUM; i++)
//		{
//			int online = Inv_can[node_id].inv_data[i].online;
//			if(!online) continue;
//
//
//			inv_set00_struct *p_inv_base = &Inv_can[node_id].inv_data[i].inv_set00;
//			inv_set01_struct *p_inv_advance = &Inv_can[node_id].inv_data[i].inv_set01;
//			inv_set03_struct *p_inv_auth = &Inv_can[node_id].inv_data[i].inv_set03;
//
//			/* 判断所有参数是否已从总线上获取完成 */
//			uint8_t zero[512] = {0};
//			if(memcmp(p_inv_base, zero, offsetof(inv_set00_struct, nc)) == 0) continue;
//			if(memcmp(p_inv_advance, zero, offsetof(inv_set01_struct, res1)) == 0) continue;
//			if(memcmp(p_inv_auth, zero, offsetof(inv_set03_struct, nc5)) == 0) continue;
//
//			/* 获取本地参数 */
//			
//			
//			inv_base_param_t *base_param = &SetData_Can.dev_info_t2.inv_set00;//ll_inv_param_get("inv_base_param");
//			inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
//			inv_auth_param_t *auth_param = &SetData_Can.dev_info_t2.inv_set02;//ll_inv_param_get("inv_auth_param");
//
//			if(base_param == NULL || advance_param == NULL || auth_param == NULL)
//			{
//				/* 清除初始化标志 */
////				ll_param_flag_t param_flag;
////				param_flag.pack_init_flag = ll_param_flag_get()->pack_init_flag;
////				param_flag.inv_init_flag = 0;
////				ll_param_flag_set(&param_flag);
////				return;
//			}
//
//			can_send_cmd_t can_cmd = {0};
//			can_cmd.node_id = node_id;
//			can_cmd.offset = 0;
//			can_cmd.can_id.bit.src = esp_canbus_myself_address();
//			can_cmd.can_id.bit.dst = INV_CAN_ADDR + i;
//			can_cmd.can_id.bit.priority = 0x02;
//
//			sync_param_t sync_param;
//			sync_param.main_node = node_id;
//			sync_param.sub_node = i;
//
//
//
//			/* 同步认证参数到该逆变设备 */
//
//			if(1 == sTypecnt) //(param_type == PACK_CONFIG_PARAM)		// 同步配置参数到该PACK设备
//			{
//				/* 判断参数是否已从总线上获取完成 */
////				uint8_t zero[512] = {0};
////				if(memcmp(p_pack_config, zero, offsetof(pack_config_struct, rw_cmd)) == 0) continue;
//
//				/* 同步基本参数到逆变设备 */
//				inv_base_param_sync_0x1A(&sync_param, base_param, p_inv_base, &can_cmd);
//				
//				if(memcmp(&SetData_Can.dev_info_t2.inv_set00, &Inv_can[node_id].inv_data[i].inv_set00, sizeof(inv_base_param_t)) != 0)
//				{
//					ESP_LOGW(TAG, "synchronize inv_base_param_t parameters, main_node: %d, sub_node: %d", node_id, i);
//					can_cmd.data_type = INV_TYPE_CONFIG00;
//					app_can_send_param((uint8_t*)&SetData_Can.dev_info_t2.inv_set00, sizeof(inv_base_param_t), &can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(20));
//
//					can_cmd.data_len = 0;
//					app_can_read_param(&can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(20));
//				}
//
//			
//			}
//			else if(2 == sTypecnt)
//			{
//				/* 同步高级参数到逆变设备 */
//				inv_advance_param_sync_0x1B(&sync_param, advance_param, p_inv_advance, &can_cmd);
//
//				if(memcmp(&SetData_Can.dev_info_t2.inv_set01, &Inv_can[node_id].inv_data[i].inv_set01, sizeof(inv_advance_param_t)) != 0)
//				{
//					ESP_LOGW(TAG, "synchronize inv_advance_param_t parameters, main_node: %d, sub_node: %d", node_id, i);
//					can_cmd.data_type = INV_TYPE_CONFIG01;
//					app_can_send_param((uint8_t*)&SetData_Can.dev_info_t2.inv_set01, sizeof(inv_advance_param_t), &can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(20));
//
//					can_cmd.data_len = 0;
//					app_can_read_param(&can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(20));
//				}
//
//			}
//			else if(3 == sTypecnt)
//			{
//				if(memcmp(&SetData_Can.dev_info_t2.inv_set03, &Inv_can[node_id].inv_data[i].inv_set03, sizeof(inv_auth_param_t)) != 0)
//				{
//					ESP_LOGW(TAG, "synchronize inv_auth_param_t parameters, main_node: %d, sub_node: %d", node_id, i);
//					can_cmd.data_type = INV_TYPE_CONFIG03;
//					app_can_send_param((uint8_t*)&SetData_Can.dev_info_t2.inv_set03, sizeof(inv_auth_param_t), &can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(20));
//
//					can_cmd.data_len = 0;
//					app_can_read_param(&can_cmd);
//					vTaskDelay(pdMS_TO_TICKS(20));
//				}
//			}
//
//
//
//
//			if (sync_param.sync_flag) {
//				inv_peak_chk = PEAK_CHK_START;
//			}
//		}
//	}
//	
//	if(++sTypecnt >= 3)
//	{
//		sTypecnt=0;
//
//	}	
//}

/*
非阻塞，主循环一次，只进行一次操作
单个设备差异化发送set
部分少数变量更新


 * @brief 同步本地参数到逆变设备
 * - APP下发的设置参数首先会保存到本地，之后再同步到每台逆变设备
 *
 * @return 无
 */
static void inv_param_check_and_sync_each(void)
{
	static int s_cmd_idx =0;
	static int s_x_idx =0;
	static int s_y_idx =0;
	int param_len=0;
//	uint8_t zero[512] = {0};
	
	int cmd_idx = 0;
	int node = 0;
	int i = 0;	


	inv_set00_struct *p_inv_base = &Inv_can[node].inv_data[i].inv_set00;
	inv_set01_struct *p_inv_advance = &Inv_can[node].inv_data[i].inv_set01;
	inv_set03_struct *p_inv_auth = &Inv_can[node].inv_data[i].inv_set03;
	
	/* 获取本地参数 */
	inv_base_param_t *base_param = &SetData_Can.dev_info_t2.inv_set00;//ll_inv_param_get("inv_base_param");
	inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
	inv_auth_param_t *auth_param = &SetData_Can.dev_info_t2.inv_set03;//ll_inv_param_get("inv_auth_param");


	can_send_cmd_t can_cmd = {0};
	can_cmd.node_id = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.dst = INV_CAN_ADDR + node*INV_MAX_NUM +i;//0xFF;// 广播set
	can_cmd.can_id.bit.priority = 0x02;

	sync_param_t sync_param;
	sync_param.main_node = node;
	sync_param.sub_node = i;

	node=s_x_idx;			
	i=s_y_idx;
	cmd_idx=s_cmd_idx;				

//	inv_poll_id =node*INV_MAX_NUM +i;
	int online = Inv_can[node].inv_data[i].online;
	if(0 != online)
	{
		reals.Can_beta_block_fast_poll_flag =0;//;
		return;
	}

	if (0 == cmd_idx)							// 同步基本参数到该逆变设备
	{
		/* 判断参数是否已从总线上获取完成 */
		if(0 != memcmp(p_inv_base, reals.zero, offsetof(inv_set00_struct, nc))) //非零
		{
			/* 同步基本参数到逆变设备 */
			inv_base_param_sync_0x1A(&sync_param, base_param, p_inv_base, &can_cmd);

		}
	
	
		
	
	}
	else if(1 == cmd_idx)					// 同步高级参数到该逆变设备
	{
		/* 判断参数是否已从总线上获取完成 */
	
		if(0 != memcmp(p_inv_advance, reals.zero, offsetof(inv_set01_struct, res1)))
		{
			/* 同步高级参数到逆变设备 */
			inv_advance_param_sync_0x1B(&sync_param, advance_param, p_inv_advance, &can_cmd);
		}
	
	
	}
	else if (2 == cmd_idx) 					// 同步认证参数到该逆变设备
	{
		/* 判断参数是否已从总线上获取完成 */
		if(0 != memcmp(p_inv_auth, reals.zero, offsetof(inv_set03_struct, nc5)))
		{

		
		}	
	
	
	}


	//y->x->can type
	if(++s_y_idx >= INV_MAX_NUM)
	{
		s_y_idx =0;
		if(++s_x_idx >= DEV_MAIN_NODE_MAX)
		{
			s_x_idx =0;
			if(++s_cmd_idx >= INV_SET_TYPE_SUM)
			{
				s_cmd_idx =0;
				reals.Can_beta_block_fast_poll_flag =0;//FAST_POLL_STEP_PACK;
				
			}
		}				
	}


}


/**
 * @brief 其他IOT参数查询与同步
 * - APP下发的设置参数首先会保存到本地，之后再同步到每台逆变设备
 *
 * @return 无
 */
static void iot_param_check_and_sync_each(void)
{
	static int s_cmd_idx =0;
	static int s_x_idx =0;
	static int s_y_idx =0;
	int param_len=0;
//	uint8_t zero[512] = {0};
	
	int cmd_idx = 0;
	int node = 0;
	int i = 0;	



	can_send_cmd_t can_cmd = {0};
	can_cmd.node_id = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.dst = IOT_CAN_ADDR + node*IOT_MAX_NUM +i;//0xFF;// 广播set
	can_cmd.can_id.bit.priority = 0x02;

	sync_param_t sync_param;
	sync_param.main_node = node;
	sync_param.sub_node = i;

	node=s_x_idx;			
	i=s_y_idx;
	cmd_idx=s_cmd_idx;				

//	inv_poll_id =node*INV_MAX_NUM +i;
	int online = Inv_can[node].iot_data[i].online;
	if(0 != online)
	{
		if (0 == cmd_idx)							// 同步基本参数到该逆变设备
		{
			/* 判断参数是否已从总线上获取完成 */
			if(0 != memcmp(&SetData.dev_info_t.factory, reals.zero, sizeof(iot_can_node_struct_reg12000_mini))) //非零
			{
				if(memcmp(&SetData.dev_info_t.factory, &Inv_can[node].iot_data[i].mod_reg12000_IOT_set, sizeof(iot_can_node_struct_reg12000_mini)) != 0)
				{
					ESP_LOGW(TAG, "synchronize inv_base_param_t parameters, main_node: %d, sub_node: %d", node, i);
					can_cmd.data_type = INV_TYPE_CONFIG00_1AH;
					app_can_send_param((uint8_t*)&SetData.dev_info_t.factory, sizeof(iot_can_node_struct_reg12000_mini), &can_cmd);
					vTaskDelay(pdMS_TO_TICKS(20));
				
					can_cmd.data_len = 0;
					app_can_read_param(&can_cmd);
					vTaskDelay(pdMS_TO_TICKS(20));
				}
		
			}
		
		
			
		
		}
	}

	

	//y->x->can type
	if(++s_y_idx >= IOT_MAX_NUM)
	{
		s_y_idx =0;
		if(++s_x_idx >= DEV_MAIN_NODE_MAX)
		{
			s_x_idx =0;
			if(++s_cmd_idx >= IOT_SET_TYPE_SUM)
			{
				s_cmd_idx =0;
				reals.iot_can_set_flag =0;
				
			}
		}				
	}


}


/*
非阻塞，主循环一次，只进行一次操作
广播，相同set

 * @brief 同步本地参数到逆变设备
 * - APP下发的设置参数首先会保存到本地，之后再同步到每台逆变设备
 *
 * @return 无
 */
static void app_param_sync_to_iot_0xFF(void)
{
	static int s_cmd_idx =0;
	static int s_x_idx =0;
	static int s_y_idx =0;
	int param_len=0;
//	uint8_t zero[512] = {0};
	
	int cmd_idx = 0;
	int node = 0;
	int i = 0;	


	can_send_cmd_t can_cmd = {0};
	can_cmd.node_id = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.dst = 0xFF;//INV_CAN_ADDR + i;广播set
	can_cmd.can_id.bit.priority = 0x02;


	node=s_x_idx;			
	i=s_y_idx;
	cmd_idx=s_cmd_idx;				

//	inv_poll_id =node*INV_MAX_NUM +i;
	int online = Inv_can[node].iot_data[i].online;
	if (0 == cmd_idx)							// 同步基本参数到该逆变设备
	{
		/* 判断参数是否已从总线上获取完成 */
		{
			ESP_LOGW(TAG, "synchronize inv_base_param_t parameters, main_node: %d, sub_node: %d", node, i);
			can_cmd.data_type = IOT_TYPE_SET_02H;
			app_can_send_param((uint8_t*)&SetData.dev_info_t.factory, sizeof(iot_can_node_struct_reg12000_mini), &can_cmd);
			vTaskDelay(pdMS_TO_TICKS(20));
		
			can_cmd.data_len = 0;
			app_can_read_param(&can_cmd);
			vTaskDelay(pdMS_TO_TICKS(20));
		}

	
		
	
	}



	//y->x->can type
	if(++s_y_idx >= IOT_MAX_NUM)
	{
		s_y_idx =0;
		if(++s_x_idx >= 1)//DEV_MAIN_NODE_MAX
		{
			s_x_idx =0;
			if(++s_cmd_idx >= IOT_SET_TYPE_SUM)
			{
				s_cmd_idx =0;
				reals.iot_can_set_flag =0;
				
			}
		}				
	}


}

/**
 * @brief 同步本地参数到PACK设备
 * - APP下发的设置参数首先会保存到本地，之后再同步到每台PACK设备
 *

 一次主循环只执行一个can type
 * @return 无
 */
static void app_param_sync_to_pack(void)
{
	static uint8_t sTypecnt=0;


	for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for(int i = 0; i < PACK_MAX_NUM; i++)
		{
			int online = Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i);
			if(!online) continue;
			
			can_send_cmd_t can_cmd = {0};
			can_cmd.node_id = node_id;
			can_cmd.offset = 0;
			can_cmd.can_id.bit.src = esp_canbus_myself_address();
			can_cmd.can_id.bit.dst = PACK_CAN_ADDR + i;
			can_cmd.can_id.bit.priority = 0x02;

			if(1 == sTypecnt) //(param_type == PACK_CONFIG_PARAM)		// 同步配置参数到该PACK设备
			{
				/* 判断参数是否已从总线上获取完成 */
//				uint8_t zero[512] = {0};
//				if(memcmp(p_pack_config, zero, offsetof(pack_config_struct, rw_cmd)) == 0) continue;
			
				if(memcmp(&SetData_Can.dev_info_t2.pack_config, &Inv_can[node_id].pack_data[i].pack_config, sizeof(pack_config_param_t)) != 0)
				{
					ESP_LOGW(TAG, "synchronize pack config parameters, main_node: %d, sub_node: %d", node_id, i);
					can_cmd.data_type = PACK_TYPE_CONFIG_55H;
					app_can_send_param((uint8_t*)&SetData_Can.dev_info_t2.pack_config, sizeof(pack_config_param_t), &can_cmd);
					vTaskDelay(pdMS_TO_TICKS(20));

					can_cmd.data_len = 0;
					app_can_read_param(&can_cmd);
					vTaskDelay(pdMS_TO_TICKS(20));
				}
			}
			else
			{

			}
		}
	}

	if(++sTypecnt >= 1)
	{
		sTypecnt=0;

	}	
}
/*
非阻塞，主循环一次，只进行一次操作
广播，相同set

 * @brief 同步本地参数到逆变设备
 * - APP下发的设置参数首先会保存到本地，之后再同步到每台逆变设备
 *
 * @return 无
 */
static void app_param_sync_to_pack_0xFF(void)
{
	static int s_cmd_idx =0;
	static int s_x_idx =0;
	static int s_y_idx =0;
	int param_len=0;
//	uint8_t zero[512] = {0};
	
	int cmd_idx = 0;
	int node = 0;
	int i = 0;	


	can_send_cmd_t can_cmd = {0};
	can_cmd.node_id = 0;
	can_cmd.can_id.bit.src = esp_canbus_myself_address();
	can_cmd.can_id.bit.dst = 0xFF;//PACK_CAN_ADDR + i;广播set
	can_cmd.can_id.bit.priority = 0x02;


	node=s_x_idx;			
	i=s_y_idx;
	cmd_idx=s_cmd_idx;				

//	inv_poll_id =node*INV_MAX_NUM +i;
	int online = Inv_can[node].pack_data[0].pack_announce.online & (1 << i);

	if(memcmp(&SetData_Can.dev_info_t2.pack_config, &Inv_can[reals.Addr_can_self].pack_data[0].pack_config, sizeof(pack_config_param_t)) != 0)
	{
		ESP_LOGI(TAG, "synchronize pack config parameters, main_node: %d, sub_node: %d", node, i);
		can_cmd.data_type = PACK_TYPE_CONFIG_55H;
		app_can_send_param((uint8_t*)&SetData_Can.dev_info_t2.pack_config, sizeof(pack_config_param_t), &can_cmd);
//		vTaskDelay(pdMS_TO_TICKS(20));
//	
//		can_cmd.data_len = 0;
//		app_can_read_param(&can_cmd);
//		vTaskDelay(pdMS_TO_TICKS(20));
	}



	//y->x->can type
	if(++s_y_idx >= PACK_MAX_NUM)
	{
		s_y_idx =0;
		if(++s_x_idx >= 1)//DEV_MAIN_NODE_MAX
		{
			s_x_idx =0;
			if(++s_cmd_idx >= PACK_SET_TYPE_SUM)
			{
				s_cmd_idx =0;
//				reals.Can_beta_block_fast_poll_flag =0;//FAST_POLL_STEP_PACK;
				
			}
		}				
	}


}

/**
 * @brief PACK参数查询与同步
 * - APP下发的设置参数首先会保存到本地，之后再同步到每台PACK设备
 *
 * @return 无
 */
static void pack_param_check_and_sync(void)
{
	static uint32_t old_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

	if((now_time - old_time) < LL_PARAM_SYNC_INTV) return;
	old_time = now_time;

	app_param_sync_to_pack();
}

/**
 * @brief 恢复参数检查
 * - 当APP下发恢复默认参数指令后,该函数清除参数初始化标志,之后从底层设备获取初始化参数
 *
 * @return 无
 */
static uint8_t s_factory_reset_iot_done = 0;

static void app_factory_reset_iot_side(void)
{
	if (s_factory_reset_iot_done) {
		return;
	}
	s_factory_reset_iot_done = 1;

	SetData_Can.dev_info_t2.valid_inv = 0;
	SetData_Can.dev_info_t2.valid_pack = 0;
	reals.flasWrFlag.sBit.set_data_inv = 1;
	reals.flasWrFlag.sBit.set_data_pack = 1;

	Default_tou_ctrl_data_init();
	if (Smart_Tou_Restore_Factory_Defaults() != 0) {
		ESP_LOGW(TAG, "Smart TOU (26000) factory reset skipped or failed");
	}
	ESP_LOGE(TAG, "***********Clear log history!*************");
	Inv_WR.mod_reg02000_Inv_base_set.ctrl.bit.clear_log = 0;
	SetData.dev_info_t.historyRecSaveCount = 0;
	SetData.dev_info_t.historyAddrIndex = 0;
	reals.historyRecSaveCount = 0;
	reals.historyAddrIndex = 0;
	reals.SetDataWrFlag.sBit.SetDataUpdate_historycnt = 1;
	reals.SetDataWrFlag.sBit.SetDataUpdate_Event_cnt = 0;
	clear_log_file();
	lcd_mode_factory_reset();
	ESP_LOGW(TAG, "reset factory settings");
}

void app_reset_factory_after_can_tx(void)
{
	bk_inv_node_set *p_inv_bk = &Inv_can_WR.bk_inv_dev_set;
	uint8_t factory_val = p_inv_bk->inv_set01.ctrl_reset_factory;

	if (factory_val == 0) {
		return;
	}

	app_factory_reset_iot_side();

	ESP_LOGW(TAG, "reg2206 ctrl_reset_factory cleared after CAN TX, was=0x%02x(%u)",
		factory_val, factory_val);

	SetData_Can.dev_info_t2.inv_set01.ctrl_reset_factory = 0;
	p_inv_bk->inv_set01.ctrl_reset_factory = 0;
	Inv_WR.mod_reg02200_Inv_advance_set.ctrl_reset_factory = 0;
	can_data_poll_index_set(INDEX_INV_TYPE_CONFIG00_1AH);
	can_data_poll_index_set(INDEX_INV_TYPE_CONFIG01_1BH);
}

/**
 * @brief AC 按钮：恢复 19000 继电器控制与 26000 Smart TOU 出厂默认
 * @note ARM 用 WRITE_DATA(0x61) 写到 Inv_can_WR.bk_inv_dev_set，不在 Inv_can[] 轮询缓存里
 */
void app_ac_button_sign_handle(void)
{
	inv_set00_struct *inv_set00_bk = &Inv_can_WR.bk_inv_dev_set.inv_set00;
	inv_set00_struct *inv_set00_rx = &Inv_can[0].inv_data[0].inv_set00;
	uint8_t cur = inv_set00_bk->AC_button_sign;

	/* 兼容：若将来改为轮询回读(0x65) 落到 Inv_can，也一并识别 */
	if (cur != 1) {
		cur = inv_set00_rx->AC_button_sign;
	}

	if (cur != 1) {
		return;
	}

	Default_tou_ctrl_data_init();
	// Smart_Tou_Restore_Factory_Defaults();
	inv_set00_bk->AC_button_sign = 0;
	inv_set00_rx->AC_button_sign = 0;
}

static void app_reset_param_check(void)
{
//	inv_set01_struct *inv_advance = &Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01;
//	inv_set01_struct *bk_inv_advance = &Inv_can_WR.bk_inv_dev_set.inv_set01;

	bk_inv_node_set *p_inv_bk = &Inv_can_WR.bk_inv_dev_set;
	bk_pack_node_set *p_pack_bk = &Inv_can_WR.bk_pack_dev_set;

	app_ac_button_sign_handle();

	if (p_inv_bk->inv_set01.ctrl_reset_factory == 0) {
		s_factory_reset_iot_done = 0;
	}

	/* 等待底层接收到恢复默认参数指令；恢复出厂在 CAN 下发成功后处理，见 app_reset_factory_after_can_tx() */
	if (p_inv_bk->inv_set01.ctrl_mix.bit.reset_param == 1)
	{
		/* 清除初始化标志(本地的参数被清除) */
		SetData_Can.dev_info_t2.valid_inv =0;//clear
		SetData_Can.dev_info_t2.valid_pack =0;//clear
		reals.flasWrFlag.sBit.set_data_inv =1;	
		reals.flasWrFlag.sBit.set_data_pack =1;	
		ESP_LOGW(TAG, "reset default parameters");

		/* 清除设置标志 */
		SetData_Can.dev_info_t2.inv_set01.ctrl_mix.bit.reset_param = 0;
		p_inv_bk->inv_set01.ctrl_mix.bit.reset_param = 0;
	}

	//tbd
	//p_inv_bk->inv_set00.ctrl.all = 0;									// 控制事件参数不保存
	p_inv_bk->inv_set00.ctrl_log_page = 0;								// 日志翻页参数不保存
	p_inv_bk->inv_set00.ctrl_all_energy_type = 0;						// 历史能量参数不保存
	p_inv_bk->inv_set00.ctrl_now_energy_type = 0;						// 当前能量参数不保存
	p_inv_bk->inv_set01.ct_test.all = 0; 							// CT测试使能参数不保存
	p_inv_bk->inv_set01.ctrl_mix.bit.reset_param = 0;					// 恢复默认参数设置不保存
}

/**
 * @brief 关闭逆变并机开关
 * - 当本地并机开关关闭时,如果底层逆变的并机开关是打开状态
 * - 则关闭底层逆变的并机开关以使得底层的开关状态与本地一致
 *
 * @return 无
 */
static void inv_parallel_switch_off(void)
{
	for( int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for (int i = 0; i < INV_MAX_NUM; i++)
		{
			int online = Inv_can[node_id].inv_data[i].online;
			
			if (!online) continue;

			inv_set01_struct *p_inv_advance = &Inv_can[node_id].inv_data[i].inv_set01;
			if ((p_inv_advance->ctrl_mix.bit.multi_inv_off_grid_en == 0) ||
				(p_inv_advance->ctrl_mix.bit.multi_inv_off_grid_en == 2)) continue;

			can_send_cmd_t can_cmd = {0};
			can_cmd.node_id = node_id;
			can_cmd.offset = offsetof(inv_advance_param_t, ctrl_mix);
			can_cmd.can_id.bit.src = esp_canbus_myself_address();
			can_cmd.can_id.bit.dst = INV_CAN_ADDR + i;
			can_cmd.can_id.bit.priority = 0x02;
			can_cmd.data_type = INV_TYPE_CONFIG01_1BH;
			uint8_t misc1 = 0x08;											// 关闭离网并机开关
			app_can_send_param((uint8_t*)&misc1, sizeof(misc1), &can_cmd);	// 发送指令
			ESP_LOGW(TAG, "close inv off-grid parallel switch, main_node: %d, sub_node: %d", node_id, i);
			vTaskDelay(pdMS_TO_TICKS(10));

			can_cmd.data_len = 0;
			app_can_read_param(&can_cmd);									// 回读数据
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

/**
 * @brief 底层设备参数处理线程
 *
 * @param pvParameters 线程参数
 *
 * @return 无
 */
//static void app_ll_param_thread(void *pvParameters)
void app_ll_param_thread(void)
{
	static uint8_t sflag=0;

	if(0 == sflag)
	{
		sflag =1;
		ll_param_init();	// 参数初始化
		// bk_param_init();	// 备份区参数初始化
	}

//	while(1)
	{
		// inv_about_struct *p_inv_about = &Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about;
		// if (p_inv_about->dev_type[0] == 0) 
		// 	return;

		/* 设备类型为EP600时关闭离网并机开关 */
		inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
//		if (strstr(p_inv_about->dev_type, "EP600") != NULL)//表示后者是前者的子集
//		{
//			if (advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1)
//			{
//				inv_advance_param_t inv_advance_param;
//				memcpy(&inv_advance_param, advance_param, offsetof(inv_advance_param_t, res1));
//				inv_advance_param.ctrl_mix.bit.multi_inv_off_grid_en = 0;
//				Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_mix.bit.multi_inv_off_grid_en = 0;
//				reals.flasWrFlag.sBit.set_data_pack =1;
//			}
//
//			inv_parallel_switch_off();	// 关闭设备并机开关以使设备状态与本地一致
//			return;
//		}
//
//		/* 并机未使能不同步底层设备参数 */
//		if ((advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 0) ||
//			(advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 2))
//		{
//			if (advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 2) 	// 本地并机开关未打开
//			{
//				inv_parallel_switch_off();								// 关闭设备并机开关以使设备状态与本地一致
//				return;
//			}
//
//			/* 如果本地开关状态为无效(0),且底层设备开关状态为打开(1),本地开关状态设置为打开(1) */
//			if (Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_mix.bit.multi_inv_off_grid_en == 1) 
//			{
//				Inv_can_WR.bk_inv_dev_set.inv_set01.ctrl_mix.bit.multi_inv_off_grid_en = 1;
//				advance_param->ctrl_mix.bit.multi_inv_off_grid_en = 1;
//				inv_advance_param_t inv_advance_param;
//				memcpy(&inv_advance_param, advance_param, offsetof(inv_advance_param_t, res1));
//				reals.flasWrFlag.sBit.set_data_pack =1;
//			}
//			/* 如果本地开关状态为无效(0),且底层设备开关状态为无效(0)或者关闭(2),不进行参数同步 */
//			else 
//			{
//				return;
//			}
//		}

		Num_SN_change_check();

		app_reset_param_check();										// 处理参数复位逻辑


//		int ret = ll_param_wait_sync(LL_PARAM_SYNC_EVT_TIMEOUT);//delay
		/* 有同步事件发生，开始同步本地参数到底层设备 */
//		if(0)//(ret == 0)
		if((INV_EPROM_READY_FLAG == SetData_Can.dev_info_t2.valid_inv) 
			&&(1 == reals.inv_can_set_flag)
			&&(0 != reals.online_Inv_num)			
			)
		{		// 同步事件发生时暂时不做处理
			//  app_param_sync_to_inv_0xFF();
			reals.inv_can_set_flag =0;
		}
		else if((PACK_EPROM_READY_FLAG == SetData_Can.dev_info_t2.valid_pack) 
			&&(1 == reals.pack_can_set_flag)
			&&(0 != reals.online_Pack_num)			
		)				
		{
			// app_param_sync_to_pack_0xFF();
			reals.pack_can_set_flag =0;
			
		}
		else if((IOT1EPROM_READY_FLAG == SetData.dev_info_t.valid_iot) 
			&&(SetData.dev_info_t.Wr_flag.word)
			&&(1 == reals.iot_can_set_flag)
			&&(reals.online_Iot_num > 1)			
		)				
		{
			ESP_LOGE(TAG, "iot_can_set_flag!!!!!!!!!!!!!!!!!!");
			app_param_sync_to_iot_0xFF();
			reals.iot_can_set_flag =0;
			
		}

		/* 无同步事件发生，周期查询底层参数是否同步成功 */
		else
		{
			
			/* 如果初始化标志无效，则使用逆变设备的参数初始化本地逆变参数 */
			if((INV_EPROM_READY_FLAG != SetData_Can.dev_info_t2.valid_inv) 
//				||((0 == reals.Addr_can_master)&&(reals.Step_can_dev_parallel >= STEP_CAN_PARALLEL_AFTER))

			)//从机，可实时更新本机IOT
			{
				app_param_init_from_inv();
			}
			/* 周期查询底层设备参数是否与本地缓存一致，如果不一致则同步本地参数到底层设备 */
			else 
			{
//				inv_param_check_and_sync_each();//self->CAN
			}

			/* 如果初始化标志无效，则使用PACK设备的参数初始化本地PACK参数 */
			if(PACK_EPROM_READY_FLAG != SetData_Can.dev_info_t2.valid_pack) 
			{
				app_param_init_from_pack();
			}
			/* 周期查询底层设备参数是否与本地缓存一致，如果不一致则同步本地参数到底层设备 */
			else 
			{
//				pack_param_check_and_sync();
			}

			/* 如果初始化标志无效，则使用逆变设备的参数初始化本地逆变参数 */
			if(IOT1EPROM_READY_FLAG != SetData.dev_info_t.valid_iot) 
			{
//				app_param_init_from_iot();
			}
			/* 周期查询底层设备参数是否与本地缓存一致，如果不一致则同步本地参数到底层设备 */
			else 
			{
//				iot_param_check_and_sync_each();
			}			
		}
	}
}
//
///**
// * @brief 创建底层参数任务
// *
// * @return ESP错误码, ESP_OK:成功, ESP_FAIL:失败
// */
//esp_err_t app_ll_param_task(void)
//{
//
//    if (xTaskCreatePinnedToCore(app_ll_param_thread, "ll param thread",  1024*4, NULL, 4, NULL, 1) != pdPASS)
//	{
//        ESP_LOGE(TAG, "ll param task create failed");
//		return ESP_FAIL;
//    }
//
//	return ESP_OK;
//}












