#include "modbus_protocol.h"
#include "util_swap.h"
//#include "crc_check.h"
#include "esp_check.h"
#include "esp_log.h"
#include <string.h>

#include "modbus_data.h"
#include "iot_mqtt.h"
#include "iot_period_task.h"
#include "uart_device_process.h"
//#include "cc1312_ota.h"
#include "comm_define.h"
#include "dev_data_record.h"
#include "app_time.h"
#include "can_protocol.h"
#include "iot_ota.h"

#include "can_data.h"
#include "modbus_tlv.h"

#define TAG "[md_protocol]"

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

void iot_modbus_data_init(void) 
{
	for(int i = 0; i < ((INV_MAX_NUM*DEV_MAIN_NODE_MAX)+1); i++)
	{
		memset(&Inv[i], 0x00, sizeof(Inv[i]));
	}	
	
	memset(&Inv_WR, 0x00, sizeof(Inv_WR));

	for(int i = 0; i < (PACK_MAX_NUM*DEV_MAIN_NODE_MAX+1); i++)
	{
		memset(&Inv_Pack[i], 0x00, sizeof(Inv_Pack[i]));
	}			
}

/**
 * @brief 设备数据表xx处理
 * 
 * @param tbl 数据表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * @return 成功返回0，失败返回-1


can set数据块，只有下面5行：
 MOD_STRUCT_reg02000 mod_reg02000_Inv_base_set; //2000~2067  基础设置项
 MOD_STRUCT_reg02200 mod_reg02200_Inv_advance_set; //2200~2245	每台逆变高级设置区
 MOD_STRUCT_reg02300 mod_reg02300_Inv_set02_struct; //2300~2400  电网认证区
 MOD_STRUCT_reg02400 mod_reg02400_Inv_certification; //2400~2449  认证设置项
 MOD_STRUCT_reg02500 mod_reg02500_Inv_advance_set2; //2500~2544  每台逆变高级设置区2（面向微逆）


 定义回调函数包含同一寄存器的所有可能操作，如 uart micro inv和CAN big inv集合分支处理
 */
int Modbus_callback_00000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	uint32_t address_begin = (uint32_t)&Inv_WR.mod_reg00000 + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&Inv_WR.mod_reg00000 + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_00000 run,write  !");
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000 + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg00000 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
//			memcpy((uint8_t *)&g_self_data_WR.mod_reg00000 + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg00000 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			memcpy((uint8_t *)&g_self_data.mod_reg00000 + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg00000 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		
			memcpy(&SetData.dev_info_t.support_mode,&g_self_data.mod_reg00000.support_mode,sizeof(g_self_data.mod_reg00000.support_mode));
			ESP_LOGI(TAG,"SetData.dev_info_t.support_mode:%u",SetData.dev_info_t.support_mode.all);
			reals.SetDataWrFlag.sBit.SetDataUpdate_all =1;
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg00000.app_password)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg00000.app_password))
			{
				reals.ModbusCmdFlag.sBit.app_password = 1;
			}

		}
		else//micro inv
		{
//			memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		
		} 

	}
	else
	{

	}


	//	app_password_check();

	ESP_LOGI(TAG, "callback Modbus_callback_00000 run !");
	return 0;
}
int Modbus_callback_00100(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_00100 run,write  !");
		//memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg00100_AppPage1,(uint8_t *)&MicroInv_WR[cb_data->SlaveAddress].mod_reg00100_AppPage1,sizeof(MicroInv_WR[cb_data->SlaveAddress].mod_reg00100_AppPage1));	
	}
	else
	{
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
//			Inv

		}
		else//micro inv
		{
//			//lxy debug(手机app所需信息 modbus从机地址0)
//			strcpy(MicroInv[0].mod_reg00100_AppPage1.dev_type,"A100S");
//			MicroInv[0].mod_reg00100_AppPage1.dev_sn = 0x00000001;
//	//		  for (uint8_t j = 0; j < (NET_SUB1G_MAX_POINT); j++)// 
//	//		  { 	 
//	//			  if(NET_POINT_OFFLINE == reals.net_point_base_Info[j].net_point_online)//有效MAC，待存储
//	//			  {
//	//				  MicroInv[0].mod_reg00100_AppPage1.inv_num = j;
//	//				  break;
//	//			  }
//	//		  }
//			MicroInv[0].mod_reg00100_AppPage1.inv_num = reals.Subnet_point_Num;
//			MicroInv[0].mod_reg00100_AppPage1.inv_online = (uint16_t)MicroInv[0].mod_reg29700_IOT_info.revd6;
		}	


		ESP_LOGI(TAG, "callback Modbus_callback_00100 run,read !");
	}
	return 0;
}
int Modbus_callback_00700(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	uint32_t address_begin = (uint32_t)&Inv_WR.mod_reg00700_OTA + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&Inv_WR.mod_reg00700_OTA + (cb_data->reg_addr_offset) + cb_data->reg_nums * 2;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_0700 run,write  !");
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00700_OTA + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg00700_OTA + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
//			memcpy((uint8_t *)&g_self_data_WR.mod_reg00700_OTA + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg00700_OTA + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			memcpy((uint8_t *)&g_self_data.mod_reg00700_OTA + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg00700_OTA + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		

			ESP_LOGW(TAG,"Modbus_callback_00700 Inv[0].mod_reg00700_OTA.ota_cmd.type:%u", Inv[0].mod_reg00700_OTA.ota_cmd.type);
#ifdef FUNC_ONECLICK_UPGRADE_EN
			if ((address_begin <= (uint32_t)&Inv_WR.mod_reg00700_OTA.ota_cmd)
				&& (address_end >= (uint32_t)&Inv_WR.mod_reg00700_OTA.ota_cmd)) {
				if (!Inv_WR.mod_reg00700_OTA.ota_cmd.begin) {
					Can_OneClickOta_End();
				}
			}
#endif
		}
		else//micro inv
		{
//			memcpy((uint8_t *)&MicroInv[0].mod_reg00700_OTA + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg00700_OTA + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		
		} 
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_0700 run,read !");

//        if (cc1312_ota_is_doing())
//        {            
//            for(uint8_t i = 0; i < reals.Subnet_point_Num; i++)
//            {
//                if (reals.net_point_ota_Info[i].is_ota == 1)//同步更新各子设备进度条
//                {
//                    memcpy(&MicroInv[0].mod_reg00700_OTA.ota_group[i], &MicroInv[i+1].mod_reg00700_OTA.ota_group[0], sizeof(MicroInv[0].mod_reg00700_OTA.ota_group[0])); 
//                }
//            }
//        }
	}

	return 0;
}
int Modbus_callback_01100(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1100 run,write  !");
        
//        if(cb_data->SlaveAddress == 0)
//        {
//            memcpy((uint8_t *)&g_self_data.mod_reg01100_Inv_base + (cb_data->reg_addr_offset),(uint8_t *)&g_self_data_WR.mod_reg01100_Inv_base + (cb_data->reg_addr_offset), cb_data->reg_nums*2);	
//        }
//        else
//        {
//            memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base + (cb_data->reg_addr_offset),(uint8_t *)&MicroInv_WR.mod_reg01100_Inv_base + (cb_data->reg_addr_offset), cb_data->reg_nums*2);	
//        }
    }
	else
	{
        
        if(cb_data->SlaveAddress == 0)
        {
            
//            #ifdef  CONFIG_HARDWARE_ATS
//    		g_self_data.mod_reg01100_Inv_base.software_total=2;//2
//            #elif defined  CONFIG_HARDWARE_DTU100
//    		g_self_data.mod_reg01100_Inv_base.software_total=1;//2
//            #endif	
//    			
//    		g_self_data.mod_reg01100_Inv_base.soft[0].type=RF;//RF
//    		g_self_data.mod_reg01100_Inv_base.soft[0].version=MicroInv[0].mod_reg11000_IOT_info.software_ver;//tbd from CC1312 ,BR
//            #ifdef  CONFIG_HARDWARE_ATS
//    		g_self_data.mod_reg01100_Inv_base.soft[1].type=INV_ARM;//ARM
//    		g_self_data.mod_reg01100_Inv_base.soft[1].version=0;//tbd from AT403
//            #endif	
        }
//        else//手机app调试
//        {
//            memset(MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base.InvType ,0, sizeof(MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base.InvType));
//            strcpy(MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base.InvType ,"A100S");
//            MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base.InvSN = (uint16_t)(reals.net_point_base_Info[cb_data->SlaveAddress-1].mac_union.MAC[6] << 8)|reals.net_point_base_Info[cb_data->SlaveAddress-1].mac_union.MAC[7];
//            
//            MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base.software_total = 2;
//            MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base.soft[1].type = RF;//INV_ARM;
//            MicroInv[cb_data->SlaveAddress].mod_reg01100_Inv_base.soft[1].version = MicroInv[cb_data->SlaveAddress].mod_reg11000_IOT_info.software_ver;
//        }
		ESP_LOGI(TAG, "callback Modbus_callback_1100 run,read !");
	}
	return 0;
}
int Modbus_callback_01200(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1200 run,write  !");
//		memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg01200_Inv_pv + (cb_data->reg_addr_offset),(uint8_t *)&MicroInv_WR.mod_reg01200_Inv_pv + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1200 run,read !");
	}
	return 0;
}
int Modbus_callback_01300(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1300 run,write  !");
//		memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg01300_Inv_grid + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg01300_Inv_grid + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1300 run,read !");
	}
	return 0;

}
int Modbus_callback_01400(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1400 run,write  !");
//		memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg01400_Inv_load + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg01400_Inv_load + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1400 run,read !");
	}
	return 0;
}
int Modbus_callback_01500(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1500 run,write  !");
//		memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg01500_Inv_inv + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg01500_Inv_inv + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1500 run,read !");
	}
	return 0;
}
int Modbus_callback_01600(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1600 run,write  !");
//		memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg01600_Inv_generator + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg01600_Inv_generator + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1600 run,read !");
	}
	return 0;
}

int Modbus_callback_01700(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1700 run,write  !");
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1700 run,read !");
	}
	return 0;
}

int Modbus_callback_01900(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    uint32_t address_begin = (uint32_t)&Meter_wr.mod_reg01900_meter + (cb_data->reg_addr_offset);
    uint32_t address_end = (uint32_t)&Meter_wr.mod_reg01900_meter + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1900 run,write  !");
        if (cb_data->SlaveAddress == 0)
        {
            memcpy((uint8_t *)&Meter[METER_MAX_NUM].mod_reg01900_meter + (cb_data->reg_addr_offset),
                    (uint8_t *)&Meter_wr.mod_reg01900_meter + (cb_data->reg_addr_offset),
                    cb_data->reg_nums*2);
            reals.ModbusCmdFlag.sBit.meter_cfg3 = 1;
        }
        else if (cb_data->SlaveAddress <= METER_MAX_NUM)//normal ,0,can
        {
            memcpy((uint8_t *)&Meter[cb_data->SlaveAddress - 1].mod_reg01900_meter + (cb_data->reg_addr_offset),
                    (uint8_t *)&Meter_wr.mod_reg01900_meter + (cb_data->reg_addr_offset),
                    cb_data->reg_nums*2);

            if((address_begin <= (uint32_t)&Meter_wr.mod_reg01900_meter.dev_id)
                &&(address_end >= (uint32_t)&Meter_wr.mod_reg01900_meter.dev_id))
            {
                if (memcmp(Meter_wr.mod_reg01900_meter.dev_id, SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].dev_id,
                    sizeof(Meter_wr.mod_reg01900_meter.dev_id)))
                {
                    memcpy(SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].dev_id, Meter_wr.mod_reg01900_meter.dev_id,
                            sizeof(SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].dev_id));
                    reals.SetDataWrFlag.sBit.meter_cfg =1;
                }
            }

            if((address_begin <= (uint32_t)&Meter_wr.mod_reg01900_meter.dev_type)
                &&(address_end >= (uint32_t)&Meter_wr.mod_reg01900_meter.dev_type))
            {
                if ((Meter_wr.mod_reg01900_meter.dev_type != 0)
                    && (Meter_wr.mod_reg01900_meter.dev_type != SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].dev_type))
                {
                    SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].dev_type = Meter_wr.mod_reg01900_meter.dev_type;
                    reals.SetDataWrFlag.sBit.meter_cfg =1;
                }
            }

            if((address_begin <= (uint32_t)&Meter_wr.mod_reg01900_meter.mfg_id)
                &&(address_end >= (uint32_t)&Meter_wr.mod_reg01900_meter.mfg_id))
            {
                if ((Meter_wr.mod_reg01900_meter.mfg_id != 0)
                    && (Meter_wr.mod_reg01900_meter.mfg_id != SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].mfg_id))
                {
                    SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].mfg_id = Meter_wr.mod_reg01900_meter.mfg_id;
                    reals.SetDataWrFlag.sBit.meter_cfg =1;
                }
            }

            if((address_begin <= (uint32_t)&Meter_wr.mod_reg01900_meter.func)
                &&(address_end >= (uint32_t)&Meter_wr.mod_reg01900_meter.func))
            {
                if ((Meter_wr.mod_reg01900_meter.func != 0)
                    && (Meter_wr.mod_reg01900_meter.func != SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].func))
                {
                    SetData.dev_info_t.meter_cfg[cb_data->SlaveAddress - 1].func = Meter_wr.mod_reg01900_meter.func;
                    reals.SetDataWrFlag.sBit.meter_cfg =1;
                }
            }

            // if (cb_data->SlaveAddress == 1)
            // {
            //     reals.ModbusCmdFlag.sBit.meter_cfg0 = 1;
            // }
            // else if (cb_data->SlaveAddress == 2)
            // {
            //     reals.ModbusCmdFlag.sBit.meter_cfg1 = 1;
            // }
            // else if (cb_data->SlaveAddress == 3)
            // {
            //     reals.ModbusCmdFlag.sBit.meter_cfg2 = 1;
            // }
        }
        
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_1900 run,read !");
	}
	return 0;
}

int Modbus_callback_02000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    uint32_t address_begin = (uint32_t)&Inv_WR.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset);
    uint32_t address_end = (uint32_t)&Inv_WR.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_2000 run,write  !");
	 	if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
	        memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			ESP_LOGI(TAG,"2 callback Modbus_callback_2000 self_config.all:0x%x ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.self_config.all);
			ESP_LOGI(TAG,"2 callback Modbus_callback_2000 LevelSwitch.bit.level:%d ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.LevelSwitch.bit.level);
			ESP_LOGI(TAG,"2 callback Modbus_callback_2000 ctrl_dc:%d ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_dc);
		}    

        if(cb_data->SlaveAddress == 0)
        {
//			if(6 == (cb_data->reg_addr_offset>>1))
//			{
//				SetData.dev_info_t.ctrl_2006.all =Inv_WR.mod_reg02000_Inv_base_set.ctrl.all;
//				reals.SetDataWrFlag.sBit.mix = 1;
//			}

			if((cb_data->reg_addr_offset>>1)+(cb_data->reg_nums>>1)<=5) 			
			{
				reals.rtc_flag.sBit.RTC_set_APP_to_IOT =1;				
				
			}
//			memcpy((uint8_t *)&g_self_data_WR.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			
        //判断是否为无效写入
			ESP_LOGI(TAG,"callback_02000 valid from server:%d",reals.rtc_flag.sBit.RTC_valid_from_SERVER);
		    if(reals.rtc_flag.sBit.RTC_valid_from_SERVER==0)
			{
				if(SetData.dev_info_t.SetTimeZone.all != Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.all)
				{
					SetData.dev_info_t.SetTimeZone.all = Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.all;
					reals.SetDataWrFlag.sBit.res_2005 = 1;
					ESP_LOGE(TAG, "callback Modbus_callback_2000 write error, invaild write !");
				}
			}

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg02000_Inv_base_set.ctrl_lcd_active_time)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg02000_Inv_base_set.ctrl_lcd_active_time))
            {
                if(SetData.dev_info_t.ctrl_lcd_active_time != Inv_WR.mod_reg02000_Inv_base_set.ctrl_lcd_active_time)
                {
                    SetData.dev_info_t.ctrl_lcd_active_time = Inv_WR.mod_reg02000_Inv_base_set.ctrl_lcd_active_time;
                    reals.SetDataWrFlag.sBit.mode_set = 1; // 屏幕休眠时间设置保存
                }
            }
            if((address_begin <= (uint32_t)&Inv_WR.mod_reg02000_Inv_base_set.ctrl_poweron)
                &&(address_end > (uint32_t)&Inv_WR.mod_reg02000_Inv_base_set.ctrl_poweron))          
            {
                reals.ModbusCmdFlag.sBit.ctrl_power = 1;
            }
        }
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_2000 run,read !");
	}
	//ESP_LOGI(TAG, "callback 2000 run,write LevelSwitch:0x%x  !",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.LevelSwitch);
	return 0;
}

int Modbus_callback_02200(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2200 run,write  !");

	 	if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[cb_data->SlaveAddress].mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		}
		else//micro inv
		{
//			memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg02200_Inv_advance_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		}    
		
        if(cb_data->SlaveAddress == 0)
        {            
//            //判断是否为无效写入
//            if(SetData.dev_info_t.ctrl_meter.all != MicroInv[0].mod_reg02200_Inv_advance_set.ctrl_meter.all)
//            {
//                SetData.dev_info_t.ctrl_meter = MicroInv[0].mod_reg02200_Inv_advance_set.ctrl_meter;
//                reals.SetDataWrFlag.sBit.ctrl_meter = 1;
//                ESP_LOGE(TAG, "ctrl_meter_enable:%d, ctrl_meter_select:%d",SetData.dev_info_t.ctrl_meter.ctrl_meter_enable,SetData.dev_info_t.ctrl_meter.ctrl_meter_select);
//            }
//            
//            //判断是否为无效写入
//            if(SetData.dev_info_t.ctrl_feedback != MicroInv[0].mod_reg02200_Inv_advance_set.ctrl_feedback)
//            {
//                SetData.dev_info_t.ctrl_feedback = MicroInv[0].mod_reg02200_Inv_advance_set.ctrl_feedback;
//                reals.SetDataWrFlag.sBit.ctrl_feedback = 1;
//            }
//            
//            //判断是否为无效写入
//            if(SetData.dev_info_t.CounterCurrentPower_Limit != MicroInv[0].mod_reg02200_Inv_advance_set.CounterCurrentPower_Limit)
//            {
//                SetData.dev_info_t.CounterCurrentPower_Limit = MicroInv[0].mod_reg02200_Inv_advance_set.CounterCurrentPower_Limit;
//                reals.SetDataWrFlag.sBit.ctrl_feedback = 1;
//            }
//            
//            SetData.dev_info_t.ctrl_reset_factory_2206 =MicroInv_WR.mod_reg02200_Inv_advance_set.ctrl_reset_factory;
//    		SetData.dev_info_t.ctrl_mix_2232 =MicroInv_WR.mod_reg02200_Inv_advance_set.ctrl_mix;
//    		reals.SetDataWrFlag.sBit.mix = 1;	
        }
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2200 run,read !");
    }
    return 0;
}

int Modbus_callback_02400(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2400 run,write  !");
	 	if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[cb_data->SlaveAddress].mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), cb_data->reg_nums*2);

		}
		else//micro inv
		{
//			memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg02400_Inv_certification + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		}   		
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2400 run,read !");
    }
    return 0;

}
int Modbus_callback_02500(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2500 run,write  !");
	 	if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[cb_data->SlaveAddress].mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		}
		else//micro inv
		{
//			memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg02500_Inv_advance_set2 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		} 		
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_2500 run,read !");
    }
    return 0;

}
int Modbus_callback_03000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
//    if(true == cb_data->is_write)
//    {
//        ESP_LOGI(TAG, "callback Modbus_callback_3000 run,write  !");
//        memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg03000_Inv_history + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR[cb_data->SlaveAddress].mod_reg03000_Inv_history + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
//    }
//    else
//    {
//        ESP_LOGI(TAG, "callback Modbus_callback_3000 run,read !");
//        if(cb_data->SlaveAddress == 0)
//        {
//            //
//        }
//    }

    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3000 run,write  !");
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03000_Inv_history + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
//			memcpy((uint8_t *)&g_self_data_WR.mod_reg03000_Inv_history + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03000_Inv_history + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			memcpy((uint8_t *)&g_self_data.mod_reg03000_Inv_history + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03000_Inv_history + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		
		}
		else//micro inv
		{
//			memcpy((uint8_t *)&g_self_data.mod_reg03000_Inv_history + (cb_data->reg_addr_offset), (uint8_t *)&g_self_data_WR.mod_reg03000_Inv_history + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		
		} 		
    }
    else
    {
		uint8_t j=0;
		uint8_t i=0;
        ESP_LOGI(TAG, "callback Modbus_callback_3000 run,read ! Inv_can_WR.ctrl_log_page : %d, Inv_WR.ctrl_log_page : %d", Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_log_page, Inv_WR.mod_reg02000_Inv_base_set.ctrl_log_page);
        // g_self_data.mod_reg03000_Inv_history.total_page = GetHistoryLogFromLittlefs(g_self_data.mod_reg02000_Inv_base_set.ctrl_log_page);
        g_self_data.mod_reg03000_Inv_history.total_page = GetHistoryLogFromLittlefsReversal(Inv_WR.mod_reg02000_Inv_base_set.ctrl_log_page);
		ESP_LOGI(TAG,"total_page:%d",g_self_data.mod_reg03000_Inv_history.total_page);
		_log_struct1 log_buf[5];
		memset(log_buf,0,sizeof(log_buf));
		for(i=0;i<5;i++)
		 {
			ESP_LOGW(TAG, "log_data year:%d mounth:%d day:%d hour:%d min:%d sec:%d happened:%d type:%d fault:%d", g_self_data.mod_reg03000_Inv_history.log_data[i].month,g_self_data.mod_reg03000_Inv_history.log_data[i].year,g_self_data.mod_reg03000_Inv_history.log_data[i].hour,g_self_data.mod_reg03000_Inv_history.log_data[i].day,g_self_data.mod_reg03000_Inv_history.log_data[i].sec,g_self_data.mod_reg03000_Inv_history.log_data[i].min,g_self_data.mod_reg03000_Inv_history.log_data[i].each.bytes.happened,g_self_data.mod_reg03000_Inv_history.log_data[i].each.bytes.type,g_self_data.mod_reg03000_Inv_history.log_data[i].fault_code);
			if(g_self_data.mod_reg03000_Inv_history.log_data[4-i].fault_code==0)
				continue;

			memcpy(&log_buf[j++],&(g_self_data.mod_reg03000_Inv_history.log_data[4-i]),sizeof(_log_struct1));
		 }
		memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history, &g_self_data.mod_reg03000_Inv_history,sizeof(MOD_STRUCT_reg03000));//快速更新
        memcpy( &Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history.log_data[0], &log_buf[0],sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history.log_data));//更新logdata
        
    }
	
    return 0;

}
int Modbus_callback_03500(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	uint8_t tempaddr=0;
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3500 run,write  !");
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
//			memcpy((uint8_t *)&g_self_data_WR.mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			memcpy((uint8_t *)&g_self_data.mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		
		}
		else//micro inv
		{
//			memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg03500_Inv_yearX_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		
		}

    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3500 run,read !");
        if(cb_data->SlaveAddress == 0)
        {
			tempaddr =(INV_MAX_NUM*DEV_MAIN_NODE_MAX);
            if(Inv[0].mod_reg02000_Inv_base_set.ctrl_all_energy_type == PV_ENERGY_TYPE)//1-总PV(直流+交流PV）
            {
                Inv[tempaddr].mod_reg03500_Inv_yearX_statistic.energy_type = PV_ENERGY_TYPE;
                Inv[tempaddr].mod_reg03500_Inv_yearX_statistic.total_energy = SetData.dev_info_t.iot_kwh_info.KWH_all[0].Pv_dc;
                GetYearXIotDataFromExtFlash(PV_ENERGY_TYPE);
            }
            else if (Inv[0].mod_reg02000_Inv_base_set.ctrl_all_energy_type == GRID_ENERGY_INPUT_TYPE)//2-电网买电(GridInput)
            {
                Inv[tempaddr].mod_reg03500_Inv_yearX_statistic.energy_type = GRID_ENERGY_INPUT_TYPE;
                Inv[0].mod_reg03500_Inv_yearX_statistic.total_energy = SetData.dev_info_t.iot_kwh_info.KWH_all[0].GridChgin_Total;
                GetYearXIotDataFromExtFlash(GRID_ENERGY_INPUT_TYPE);
            }
            else//无效类型
            {
                memset((uint8_t *)&Inv[0].mod_reg02000_Inv_base_set.ctrl_all_energy_type, 0, sizeof(Inv[0].mod_reg02000_Inv_base_set.ctrl_all_energy_type));
            }
        }

    }
    return 0;

}
int Modbus_callback_03600(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	uint8_t tempaddr=0;

    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3600 run,write  !");
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
//			memcpy((uint8_t *)&g_self_data_WR.mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			memcpy((uint8_t *)&g_self_data.mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		
		}
		else//micro inv
		{
//			memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg03600_Inv_year1_statistic + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		
		}

    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_3600 run,read !");
        if(cb_data->SlaveAddress == 0)
        {
        	tempaddr =(INV_MAX_NUM*DEV_MAIN_NODE_MAX);
            if(Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == PV_ENERGY_TYPE)//1-总PV(直流+交流PV）
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_Pv_dc[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == GRID_ENERGY_INPUT_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_GridChgin_Total[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == GRID_ENERGY_OUTPUT_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_GridFeedback_Total[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == AC_LOAD_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_ACLoad_Total[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == DC_LOAD_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_DCLoad_Total[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == DC_PV_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_Pv_dc[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == BAT_TOTAL_DISCHARGE_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_PackDsg_Total[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == PV_TO_ACLOAD_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_PVToload_Total[0], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }

			
            else//无效类型
            {
                memset((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, 0, sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
        }
        else if(cb_data->SlaveAddress <= (IOT_MAX_NUM*DEV_MAIN_NODE_MAX))
        {
        	tempaddr =cb_data->SlaveAddress-1;
            if(Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == PV_ENERGY_TYPE)//1-总PV(直流+交流PV）
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_Pv_dc[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == GRID_ENERGY_INPUT_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_GridChgin_Total[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == GRID_ENERGY_OUTPUT_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_GridFeedback_Total[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == AC_LOAD_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_ACLoad_Total[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == DC_LOAD_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_DCLoad_Total[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == DC_PV_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_Pv_dc[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == BAT_TOTAL_DISCHARGE_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_PackDsg_Total[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
            else if (Inv[tempaddr].mod_reg02000_Inv_base_set.ctrl_now_energy_type == PV_TO_ACLOAD_ENERGY_TYPE)//2-电网买电(GridInput)
            {
                memcpy((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, (uint8_t *)&reals.energy_PVToload_Total[1], sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }

			
            else//无效类型
            {
                memset((uint8_t *)&Inv[tempaddr].mod_reg03600_Inv_year1_statistic, 0, sizeof(Inv[0].mod_reg03600_Inv_year1_statistic));
            }
        }

    }


    return 0;
}

int Modbus_callback_04000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4000 run,write  !");
//        memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg04000_Dsp_data + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg04000_Dsp_data + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4000 run,read !");
    }
    return 0;
}

int Modbus_callback_04050(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4050 run,write  !");
//        memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg04050_Dsp_set1 + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg04050_Dsp_set1 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4050 run,read !");
    }
    return 0;
}

int Modbus_callback_04105(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4105 run,write  !");
//        memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg04105_Dsp_set2 + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg04105_Dsp_set2 + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_4105 run,read !");
    }
    return 0;
}

int Modbus_callback_06000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_06000 run,read !");
	
	return 0;
}
int Modbus_callback_06100(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_06100 run,read !");
	
	return 0;
}
int Modbus_callback_06300(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_06300 run,read !");
	
	return 0;
}
int Modbus_callback_07200(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_07200 run,read !");
	
	return 0;
}
int Modbus_callback_11000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_11000 run,write  !");
		if(cb_data->SlaveAddress == 0)
		{


		}
		else
		{
		}
		
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
//			memcpy((uint8_t *)&g_self_data_WR.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
			memcpy((uint8_t *)&g_self_data.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		
		}
		else//micro inv
		{
//				memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg11000_IOT_info + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		
		}		
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_11000 run,read !");
    }
    return 0;

}

int Modbus_callback_12000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	uint32_t address_begin = (uint32_t)&Inv_WR.mod_reg12000_IOT_set + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&Inv_WR.mod_reg12000_IOT_set + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;
	uint8_t wifi_cfg_written = 0;

	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_12000 run,write  !");
		
		reals.ModbusCmdFlag.sBit.new_cfg = 1;			// iot 收到新的配置
//		memcpy((uint8_t *)&g_self_data.mod_reg12000_IOT_set, (uint8_t *)&g_self_data_WR.mod_reg12000_IOT_set, sizeof(g_self_data_WR.mod_reg12000_IOT_set));		
		if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
		{
			memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg12000_IOT_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);

			memcpy((uint8_t *)&g_self_data.mod_reg12000_IOT_set + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg12000_IOT_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		
			ESP_LOGI(TAG,"Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid::%s",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.wifi_sta_ssid);
			ESP_LOGI(TAG,"g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid::%s",g_self_data.mod_reg12000_IOT_set.wifi_sta_ssid);
			ESP_LOGI(TAG,"Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND:%d",Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND);
			ESP_LOGI(TAG,"g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND:%d",g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND);
			ESP_LOGW(TAG, "Inv_WR.mod_reg12000_IOT_set.could_dns: %s", Inv_WR.mod_reg12000_IOT_set.could_dns);
			// ESP_LOGE(TAG,"Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.all:%d",Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix1.all);
			// ESP_LOGE(TAG,"g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.all:%d",g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.all);
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_ssid))
			{
				reals.ModbusCmdFlag.sBit.wifi_sta_ssid = 1;
				wifi_cfg_written = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_password)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_password))
			{
				reals.ModbusCmdFlag.sBit.wifi_sta_password = 1;
				wifi_cfg_written = 1;
			}

			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_sta_auth))
			{
				reals.ModbusCmdFlag.sBit.wifi_sta_auth = 1;
				wifi_cfg_written = 1;
			}

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_ap_auth)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.wifi_AP_password[0]))
			{
				reals.ModbusCmdFlag.sBit.wifi_ap = 1;
			}

			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.could_dns[0])
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.could_dns[0]))
			{
				reals.ModbusCmdFlag.sBit.could_dns = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.mobile_apn[0])
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.mobile_apn[0]))
			{
				reals.ModbusCmdFlag.sBit.mobile_apn = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.on_off)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.on_off))
			{
				reals.ModbusCmdFlag.sBit.on_off = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.thunder_ctrl)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.thunder_ctrl))
			{
				reals.ModbusCmdFlag.sBit.thunder_ctrl = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.period_report)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.period_report))
			{
				reals.ModbusCmdFlag.sBit.period_report = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix1)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix1))
			{
				reals.ModbusCmdFlag.sBit.IOT_Enable_mix1 = 1;
				/* 12170 IOT-ARM绑定：蓝牙/Modbus已收到下设 */
				ESP_LOGW(TAG, "[12170 BIND] BLE/Modbus RX: all=0x%04X trigger_BIND=%d regroup_IOT=%d regroup_INV=%d regroup_PACK=%d slave=%d offset=%u nums=%u",
					g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.all,
					g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_BIND,
					g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_IOT,
					g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_INV,
					g_self_data.mod_reg12000_IOT_set.IOT_Enable_mix1.bit.trigger_regroup_PACK,
					cb_data->SlaveAddress, cb_data->reg_addr_offset, cb_data->reg_nums);
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix2)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.IOT_Enable_mix2))
			{
				reals.ModbusCmdFlag.sBit.IOT_Enable_mix2 = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.Protocol_3r_Enable_mix1))
			{
				reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix1 = 1;
			}
			if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.Protocol_3r_Enable_mix2)
				&&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.Protocol_3r_Enable_mix2))
			{
				reals.ModbusCmdFlag.sBit.Protocol_3r_Enable_mix2 = 1;
			}
            if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.LCD_Mode)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.LCD_Mode))
            {
                reals.ModbusCmdFlag.sBit.mode_set = 1;
            }
            if((address_begin <= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.Time_Span)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg12000_IOT_set.Time_Span))
            {
                reals.ModbusCmdFlag.sBit.time_span = 1;
            }
		}
		else//micro inv
		{
//				memcpy((uint8_t *)&MicroInv[cb_data->SlaveAddress].mod_reg11000_IOT_info + (cb_data->reg_addr_offset), (uint8_t *)&MicroInv_WR.mod_reg11000_IOT_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
		
		}		
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_12000 run,read !");
	}

//	wifi_sta_config_check(income.msg_ptr);					// wifi station配置检查
	if (!wifi_cfg_written) {
		wifi_sta_config_flag = 1;
	}


	return 0;
}



int Modbus_callback_13500(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_13500_ run,write  !");
        memcpy((uint8_t *)&g_self_data.mod_reg13500_mesh + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg13500_mesh + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_13500_ run,read !");
    }

	return 0;
}

int Modbus_callback_13600(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    uint32_t address_begin = (uint32_t)&Inv_WR.mod_reg13600_open + (cb_data->reg_addr_offset);
    uint32_t address_end = (uint32_t)&Inv_WR.mod_reg13600_open + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_13600 run,write  !");
        
        reals.ModbusCmdFlag.sBit.new_cfg = 1;               // iot 收到新的配置
        if(cb_data->SlaveAddress < MODBUS_SLAVE_ADDR_WIFI_START)//normal ,0,can
        {
            memcpy((uint8_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg13600_open + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
            memcpy((uint8_t *)&g_self_data.mod_reg13600_open + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg13600_open + (cb_data->reg_addr_offset), cb_data->reg_nums*2);		

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.ble_protocol)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.ble_protocol))
            {
                reals.ModbusCmdFlag.sBit.ble_protocol = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.blec_rssi_threshold)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.blec_switch_interval))
            {
                reals.ModbusCmdFlag.sBit.ble_client = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.bles_adv_key)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.bles_adv_key))
            {
                reals.ModbusCmdFlag.sBit.ble_server = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta_rssi_threshold)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta_switch_interval))
            {
                reals.ModbusCmdFlag.sBit.wifi_sta_rssi = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_mul_sta_en)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_mul_sta_en))
            {
                reals.ModbusCmdFlag.sBit.wifi_mul_sta = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta1_ip)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta1_dns2))
            {
                reals.ModbusCmdFlag.sBit.wifi_sta1 = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta2_auth)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta2_dns2))
            {
                reals.ModbusCmdFlag.sBit.wifi_sta2 = 1;
            }

            //TODO: 无法触发
            if(((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta3_auth)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.wifi_sta3_auth)))
            {
                reals.ModbusCmdFlag.sBit.wifi_sta3 = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.open_mqtt_enable)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.open_mqtt_enable))
            {
                reals.ModbusCmdFlag.sBit.mqtt = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.open_mqtt_report_cycle)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.open_mqtt_report_cycle))
            {
                reals.ModbusCmdFlag.sBit.mqtt = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.modbus_tcp_enable)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.modbus_tcp_enable))
            {
                reals.ModbusCmdFlag.sBit.modbus_tcp = 1;
            }

            if((address_begin <= (uint32_t)&Inv_WR.mod_reg13600_open.modbus_tcp_port)
                &&(address_end >= (uint32_t)&Inv_WR.mod_reg13600_open.modbus_tcp_port))
            {
                reals.ModbusCmdFlag.sBit.modbus_tcp = 1;
            }
        }
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_12000 run,read !");
    }

    // 如果密码错误达到重连次数，配置wifi后触发重连.
    wifi_sta_config_flag = 1;

    return 0;
}

int Modbus_callback_14000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	ESP_LOGI(TAG, "callback Modbus_callback_14000 run !");
	
	return 0;
}

int Modbus_callback_14500(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_14500 run,write  !");
        memcpy((uint8_t *)&Inv[cb_data->SlaveAddress].mod_reg14500_SmartPlug_info + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg14500_SmartPlug_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_14500 run,read !");
    }

	return 0;
}

int Modbus_callback_14700(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_14700 run,write  !");
        memcpy((uint8_t *)&Inv[cb_data->SlaveAddress].mod_reg14700_SmartPlug_set + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg14700_SmartPlug_set + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_14700 run,read !");
    }

	return 0;
}
int Modbus_callback_15500(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15500 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15500 run,read !");
    }

	return 0;
}
int Modbus_callback_15600(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15600 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15600 run,read !");
    }

	return 0;
}
int Modbus_callback_15700(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15700 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15700 run,read !");
    }

	return 0;
}

int Modbus_callback_15750(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15750 run,write  !");
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_15750 run,read !");
    }

	return 0;
}

int Modbus_callback_19000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{

   if(true == cb_data->is_write)
   {
       ESP_LOGI(TAG, "callback Modbus_callback_19000 run,write  !");
	   Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&Inv_WR.mod_reg19000_Relay_ctrl.GeneralPowerRelaySoc, POWERRELAY_SMART_SET01, cb_data->reg_addr_offset, cb_data->reg_nums*2);
   }
   else
   {
       ESP_LOGI(TAG, "callback Modbus_callback_19000 run,read !");
   }
    return 0;
}

int Modbus_callback_19100(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{

   if(true == cb_data->is_write)
   {
	
       ESP_LOGI(TAG, "callback Modbus_callback_19100 run,write  !");
	   Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&Inv_WR.mod_reg19100_Relay_ctrl_2.GeneralPowerRelay_Delay_Time_En, POWERRELAY_SMART_SET02, cb_data->reg_addr_offset, cb_data->reg_nums*2);
   }
   else
   {
       ESP_LOGI(TAG, "callback Modbus_callback_19100 run,read !");
   }
    return 0;
}

int Modbus_callback_19200(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{

    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19200 run,write  !");
		Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&Inv_WR.mod_reg19200_Backup_power, POWERRELAY_SMART_SET04, cb_data->reg_addr_offset, cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_19200 run,read !");
    }

    return 0;
}


int Modbus_callback_19300(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{

   if(true == cb_data->is_write)
   {
	
       ESP_LOGI(TAG, "callback Modbus_callback_19300 run,write  !");
	   Relay_Ctrl_Setdata_Get_From_Modbus((uint32_t)&Inv_WR.mod_reg19300_Relay_ctrl_3.GeneralPowerRelay_Set_Time_En, POWERRELAY_SMART_SET03, cb_data->reg_addr_offset, cb_data->reg_nums*2);
   }
   else
   {
       ESP_LOGI(TAG, "callback Modbus_callback_19300 run,read !");
   }
    return 0;
}

int Modbus_callback_21000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{

	// if(true == cb_data->is_write)
	// {
	// 	ESP_LOGI(TAG, "callback Modbus_callback_21000 run,write  !");
	// 	reals.bind_triger =1;
	// 	reals.set_bind_cnt=	(cb_data->reg_nums -2)/6;
	// }
	if(true == cb_data->is_write)
	{
		ESP_LOGI(TAG, "callback Modbus_callback_21000 run,write  !");

        if (Inv_WR.mod_reg21000_bind_WR.ver == 30001)
        {
            /*绑定帧*/
            ESP_LOGW(TAG, "protocol version : %d", Inv_WR.mod_reg21000_bind_WR.ver);
		    reals.set_bind_cnt=	(cb_data->reg_nums -2)/6;
            reals.bind_triger = 1;
			g_self_data.mod_reg21000_bind.ver = Inv_WR.mod_reg21000_bind_WR.ver;	
			reals.modbus_self_report_ble = g_self_data.mod_reg21000_bind.ver;//根据此值判断是读取上报还是主动上报
        }
        else if (Inv_WR.mod_reg21000_bind_WR.ver == 30002)
        {
            /*升级进度/状态上报（借用绑定帧框架）*/
            ESP_LOGW(TAG, "protocol version : %d", Inv_WR.mod_reg21000_bind_WR.ver);
            //reals.modbus_self_report_ble = 3;
        }
#ifdef CONFIG_MODBUS_REG_TLV_ENABLE        
        else if ((Inv_WR.mod_reg21000_bind_WR.ver == MODBUS_21000_FUNC_TLV_DATA)
            || (Inv_WR.mod_reg21000_bind_WR.ver == MODBUS_21000_FUNC_TLV_READ_CMD))
        {
            if ((0 == cb_data->reg_addr_offset) && (0 < cb_data->reg_nums)) {
                modbus_tlv_queue_push((const uint8_t *)&Inv_WR.mod_reg21000_bind_WR.ver, (cb_data->reg_nums * 2), cb_data->cb_chl);
            }
        }
#endif
        else
        {
            ESP_LOGE(TAG, "write protocol version : %d ", Inv_WR.mod_reg21000_bind_WR.ver);
           // reals.modbus_self_report_ble = Inv_WR.mod_reg21000_bind_WR.ver;
			if ( MD_CHL_BLE == cb_data->cb_chl)
            {
                reals.modbus_self_report_ble = Inv_WR.mod_reg21000_bind_WR.ver;	
            }
            else if ( MD_CHL_WIFI_CLOUD == cb_data->cb_chl )
            {
                reals.modbus_self_report_mqtt = Inv_WR.mod_reg21000_bind_WR.ver;	
            }	

            //g_self_data.mod_reg21000_bind.ver = Inv_WR.mod_reg21000_bind_WR.ver;	
			//reals.modbus_self_report_ble = g_self_data.mod_reg21000_bind.ver;//根据此值判断是读取上报还是主动上报
        }
	}
	else
	{
		ESP_LOGI(TAG, "callback Modbus_callback_21000 run,read !");
	}


	return 0;
}

int Modbus_callback_22000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	uint32_t address_begin = (uint32_t)&Inv_WR.mod_reg22000_net_server_2rd + (cb_data->reg_addr_offset);
	uint32_t address_end = (uint32_t)&Inv_WR.mod_reg22000_net_server_2rd + (cb_data->reg_addr_offset) +cb_data->reg_nums*2;

	ESP_LOGI(TAG, "callback Modbus_callback_22000 run !");
    if(true == cb_data->is_write)
    {
    	ESP_LOGI(TAG, "callback Modbus_callback_22000 run,write  !");
    	
    	reals.ModbusCmdFlag.sBit.cfg_22000 = 1;			// iot 收到新的配置
    	memcpy((uint8_t *)&g_self_data.mod_reg22000_net_server_2rd + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg22000_net_server_2rd + (cb_data->reg_addr_offset), cb_data->reg_nums*2);	

		if((address_begin <= (uint32_t)&Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address)
			&&(address_end >= (uint32_t)&Inv_WR.mod_reg22000_net_server_2rd.Net_Server_address))
		{
			reals.ModbusCmdFlag.sBit.Net_Server_address = 1;
		}
			
		if((address_begin <= (uint32_t)&Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret)
			&&(address_end >= (uint32_t)&Inv_WR.mod_reg22000_net_server_2rd.Net_Server_secret))
		{
			reals.ModbusCmdFlag.sBit.Net_Server_secret = 1;
		}			



	}
    else
    {
    	ESP_LOGI(TAG, "callback Modbus_callback_22000 run,read !");
    }	
	return 0;
}

int Modbus_callback_26000(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_26000 run,write  !");
        Smart_Tou_Ctrl_Config_Commit(cb_data->reg_addr_offset, (cb_data->reg_nums * 2));
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_26000 run,read !");
    }
    return 0;
}

int Modbus_callback_29700(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        ESP_LOGI(TAG, "callback Modbus_callback_29700 run,write  !");
        memcpy((uint8_t *)&Inv[0].mod_reg29700_IOT_info + (cb_data->reg_addr_offset), (uint8_t *)&Inv_WR.mod_reg29700_IOT_info + (cb_data->reg_addr_offset), cb_data->reg_nums*2);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_29700 run,read !");
    }
    return 0;

}
int Modbus_callback_40000(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
    if(true == cb_data->is_write)
    {
        MOD_STRUCT_Inv *p_inv = NULL;

        if(0 == cb_data->SlaveAddress)
        {
            p_inv = &Inv[(INV_MAX_NUM * DEV_MAIN_NODE_MAX)];
        }
        else
        {
            p_inv = &Inv[cb_data->SlaveAddress - 1];
        }

        memcpy((uint8_t *)&p_inv->mod_reg40000_transparent + cb_data->reg_addr_offset,
               (uint8_t *)&Inv_WR.mod_reg40000_transparent + cb_data->reg_addr_offset,
               cb_data->reg_nums * 2);
        ESP_LOGI(TAG, "callback Modbus_callback_40000 run,write, addr:%u, nums:%u", cb_data->reg_addr, cb_data->reg_nums);
    }
    else
    {
        ESP_LOGI(TAG, "callback Modbus_callback_40000 run,read !");
    }

	return 0;
}
//static md_data_t md_data[MD_TBL_MAX];		// modbus数据表

//const EXT_RAM_BSS_ATTR md_data_t md_data[]
#define LEN_MD_DATA_ARRAY	42//41+1 relay soc ctrl 19000
//windy：试验表明，EXT_RAM_BSS_ATTR定义的大块变量，需要在函数中明示赋值，而非定义时候赋值
 EXT_RAM_BSS_ATTR md_data_t md_data[LEN_MD_DATA_ARRAY];

void md_data_init(void)
{
	uint16_t i=0;
	//		{{MOD_REG_START_ADDR_00100, (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100), false, Modbus_callback_00100},NULL},	// 
	//		{{MOD_REG_START_ADDR_00700, (MOD_REG_START_ADDR_00700 + MOD_REG_LEN_00700), false, Modbus_callback_00700},NULL},	// 
	//		{{MOD_REG_START_ADDR_01100, (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100), false, Modbus_callback_01100},NULL},	// 
	//		{{MOD_REG_START_ADDR_01200, (MOD_REG_START_ADDR_01200 + MOD_REG_LEN_01200), false, Modbus_callback_01200},NULL},	// 
	//		{{MOD_REG_START_ADDR_01300, (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300), false, Modbus_callback_01300},NULL},	// 
	//		{{MOD_REG_START_ADDR_01400, (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400), false, Modbus_callback_01400},NULL},	// 
	//		{{MOD_REG_START_ADDR_01500, (MOD_REG_START_ADDR_01500 + MOD_REG_LEN_01500), false, Modbus_callback_01500},NULL},	// 
	//		{{MOD_REG_START_ADDR_01600, (MOD_REG_START_ADDR_01600 + MOD_REG_LEN_01600), false, Modbus_callback_01600},NULL},	// 
	//		{{MOD_REG_START_ADDR_02000, (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000), false, Modbus_callback_02000},NULL},	// 
	//		
	//		{{MOD_REG_START_ADDR_02200, (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200), false, Modbus_callback_02200},NULL},	// 
	//		{{MOD_REG_START_ADDR_02400, (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400), false, Modbus_callback_02400},NULL},	// 
	//		{{MOD_REG_START_ADDR_02500, (MOD_REG_START_ADDR_02500 + MOD_REG_LEN_02500), false, Modbus_callback_03000},NULL},	// 
	//		{{MOD_REG_START_ADDR_03000, (MOD_REG_START_ADDR_03000 + MOD_REG_LEN_03000), false, Modbus_callback_03500},NULL},	// 
	//		{{MOD_REG_START_ADDR_03500, (MOD_REG_START_ADDR_03500 + MOD_REG_LEN_03500), false, Modbus_callback_03600},NULL},	// 
	//		{{MOD_REG_START_ADDR_03600, (MOD_REG_START_ADDR_03600 + MOD_REG_LEN_03600), false, Modbus_callback_06000},NULL},	// 
	//		{{MOD_REG_START_ADDR_06000, (MOD_REG_START_ADDR_06000 + MOD_REG_LEN_06000), false, Modbus_callback_06000},NULL},	// 
	//		{{MOD_REG_START_ADDR_06100, (MOD_REG_START_ADDR_06100 + MOD_REG_LEN_06100), false, Modbus_callback_06100},NULL},	// 
	//		{{MOD_REG_START_ADDR_11000, (MOD_REG_START_ADDR_11000 + MOD_REG_LEN_11000), false, Modbus_callback_11000},NULL},	// 
	//		{{MOD_REG_START_ADDR_12000, (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000), false, Modbus_callback_12000},NULL},	// 
	//		{{MOD_REG_START_ADDR_13500, (MOD_REG_START_ADDR_13500 + MOD_REG_LEN_13500), false, Modbus_callback_13500},NULL},	// 

    //	0
    md_data[i].tbl.start = MOD_REG_START_ADDR_00000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000);
    md_data[i].tbl.is_write= false; 
    md_data[i].tbl.tbl_cb=Modbus_callback_00000;
    md_data[i].data=NULL;	//
    i++;
    //	1
    md_data[i].tbl.start = MOD_REG_START_ADDR_00100;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100);
    md_data[i].tbl.is_write= false; 
    md_data[i].tbl.tbl_cb=Modbus_callback_00100;
    md_data[i].data=NULL;	//
    i++;
    //	2
    md_data[i].tbl.start = MOD_REG_START_ADDR_00700;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_00700 + MOD_REG_LEN_00700);
    md_data[i].tbl.is_write= false; 
    md_data[i].tbl.tbl_cb=Modbus_callback_00700;
    md_data[i].data=NULL;	//
    i++;
    //	3
    md_data[i].tbl.start = MOD_REG_START_ADDR_01100;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100);
    md_data[i].tbl.is_write= false, 
    md_data[i].tbl.tbl_cb=Modbus_callback_01100;
    md_data[i].data=NULL;	//
    i++;
    //	4
    md_data[i].tbl.start = MOD_REG_START_ADDR_01200;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01200 + MOD_REG_LEN_01200);
    md_data[i].tbl.is_write= false; 
    md_data[i].tbl.tbl_cb=Modbus_callback_01200;
    md_data[i].data=NULL;	//
    i++;

    //	5
    md_data[i].tbl.start = MOD_REG_START_ADDR_01300;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300);
    md_data[i].tbl.is_write= false; 
    md_data[i].tbl.tbl_cb=Modbus_callback_01300;
    md_data[i].data=NULL;	//
    i++;
    //	6
    md_data[i].tbl.start = MOD_REG_START_ADDR_01400;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400);
    md_data[i].tbl.is_write= false; 
    md_data[i].tbl.tbl_cb=Modbus_callback_01400;
    md_data[i].data=NULL;	//
    i++;
    //	7
    md_data[i].tbl.start = MOD_REG_START_ADDR_01500;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01500 + MOD_REG_LEN_01500);
    md_data[i].tbl.is_write= false, 
    md_data[i].tbl.tbl_cb=Modbus_callback_01500;
    md_data[i].data=NULL;	//
    i++;
    //	8
    md_data[i].tbl.start = MOD_REG_START_ADDR_01600;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01600 + MOD_REG_LEN_01600);
    md_data[i].tbl.is_write= false, 
    md_data[i].tbl.tbl_cb=Modbus_callback_01600;
    md_data[i].data=NULL;	//
    i++;

    //	9
    md_data[i].tbl.start = MOD_REG_START_ADDR_01700;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01700 + MOD_REG_LEN_01700);
    md_data[i].tbl.is_write= false, 
    md_data[i].tbl.tbl_cb=Modbus_callback_01700;
    md_data[i].data=NULL;	//
    i++;

    //	10
    md_data[i].tbl.start = MOD_REG_START_ADDR_01900;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_01900 + MOD_REG_LEN_01900);
    md_data[i].tbl.is_write= false, 
    md_data[i].tbl.tbl_cb=Modbus_callback_01900;
    md_data[i].data=NULL;	//
    i++;

    //	11
    md_data[i].tbl.start = MOD_REG_START_ADDR_02000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_02000;
    md_data[i].data=NULL;	//
    i++;

    //	12
    md_data[i].tbl.start = MOD_REG_START_ADDR_02200;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_02200;
    md_data[i].data=NULL;	//
    i++;
    //	13
    md_data[i].tbl.start = MOD_REG_START_ADDR_02400;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_02400;
    md_data[i].data=NULL;	//
    i++;
    //	14
    md_data[i].tbl.start = MOD_REG_START_ADDR_02500;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_02500 + MOD_REG_LEN_02500);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_02500;
    md_data[i].data=NULL;	//
    i++;
    //	15
    md_data[i].tbl.start = MOD_REG_START_ADDR_03000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_03000 + MOD_REG_LEN_03000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_03000;
    md_data[i].data=NULL;	//
    i++;
    //	16
    md_data[i].tbl.start = MOD_REG_START_ADDR_03500;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_03500 + MOD_REG_LEN_03500);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_03500;
    md_data[i].data=NULL;	//
    i++;

    //	17
    md_data[i].tbl.start = MOD_REG_START_ADDR_03600;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_03600 + MOD_REG_LEN_03600);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_03600;
    md_data[i].data=NULL;	//
    i++;
    //	18
    md_data[i].tbl.start = MOD_REG_START_ADDR_04000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_04000 + MOD_REG_LEN_04000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_04000;
    md_data[i].data=NULL;	//
    i++;
    //	19
    md_data[i].tbl.start = MOD_REG_START_ADDR_04050;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_04050 + MOD_REG_LEN_04050);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_04050;
    md_data[i].data=NULL;	//
    i++;
    //20
    md_data[i].tbl.start = MOD_REG_START_ADDR_04105;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_04105 + MOD_REG_LEN_04105);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_04105;
    md_data[i].data=NULL;	//
    i++;
    //21
    md_data[i].tbl.start = MOD_REG_START_ADDR_06000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_06000 + MOD_REG_LEN_06000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_06000;
    md_data[i].data=NULL;	//
    i++;
    //	22
    md_data[i].tbl.start = MOD_REG_START_ADDR_06100;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_06100 + MOD_REG_LEN_06100);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_06100;
    md_data[i].data=NULL;	//
    i++;
    //	23
    md_data[i].tbl.start = MOD_REG_START_ADDR_06300;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_06300 + MOD_REG_LEN_06300);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_06300;
    md_data[i].data=NULL;	//
    i++;
    //	24
    md_data[i].tbl.start = MOD_REG_START_ADDR_07200;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_07200 + MOD_REG_LEN_07200);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_07200;
    md_data[i].data=NULL;	//
    i++;
    //	25
    md_data[i].tbl.start = MOD_REG_START_ADDR_11000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_11000 + MOD_REG_LEN_11000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_11000;
    md_data[i].data=NULL;	//
    i++;
    //	26
    md_data[i].tbl.start = MOD_REG_START_ADDR_12000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_12000;
    md_data[i].data=NULL;	//
    i++;
    //	27
    md_data[i].tbl.start = MOD_REG_START_ADDR_13500;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_13500 + MOD_REG_LEN_13500);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_13500;
    md_data[i].data=NULL;	//
    i++;
    //	28
    md_data[i].tbl.start = MOD_REG_START_ADDR_13600;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_13600 + MOD_REG_LEN_13600);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_13600;
    md_data[i].data=NULL;	//
    i++;
    //29
    md_data[i].tbl.start = MOD_REG_START_ADDR_14000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_14000 + MOD_REG_LEN_14000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_14000;
    md_data[i].data=NULL;	//
    i++;
    //30
    md_data[i].tbl.start = MOD_REG_START_ADDR_14500;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_14500;
    md_data[i].data=NULL;	//
    i++;
    //31
    md_data[i].tbl.start = MOD_REG_START_ADDR_14700;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_14700;
    md_data[i].data=NULL;	//
    i++;
    //32
    md_data[i].tbl.start = MOD_REG_START_ADDR_15500;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_15500 + MOD_REG_LEN_15500);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_15500;
    md_data[i].data=NULL;	//
    i++;
    //33
    md_data[i].tbl.start = MOD_REG_START_ADDR_15600;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_15600;
    md_data[i].data=NULL;	//
    i++;
    //34
    md_data[i].tbl.start = MOD_REG_START_ADDR_15700;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_15700 + MOD_REG_LEN_15700);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_15700;
    md_data[i].data=NULL;	//
    i++;
    //35
    md_data[i].tbl.start = MOD_REG_START_ADDR_15750;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_15750 + MOD_REG_LEN_15750);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_15750;
    md_data[i].data=NULL;	//
    i++;
    //36
    md_data[i].tbl.start = MOD_REG_START_ADDR_19000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_19000;
    md_data[i].data=NULL;	//
    i++;

    //	37
    md_data[i].tbl.start = MOD_REG_START_ADDR_21000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_21000 + MOD_REG_LEN_21000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_21000;
    md_data[i].data=NULL;	//
    i++;
    //	
    //38
    md_data[i].tbl.start = MOD_REG_START_ADDR_22000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_22000 + MOD_REG_LEN_22000);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_22000;
    md_data[i].data=NULL;	//
    i++;
    //39
    md_data[i].tbl.start = SMART_TOU_REG_START_ADDR;
    md_data[i].tbl.end = (SMART_TOU_REG_START_ADDR + SMART_TOU_REG_MAX_LEN);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_26000;
    md_data[i].data=NULL;	//
    i++;
    //40
    md_data[i].tbl.start = MOD_REG_START_ADDR_29700;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_29700 + MOD_REG_LEN_29700);
    md_data[i].tbl.is_write= false;
    md_data[i].tbl.tbl_cb=Modbus_callback_29700;
    md_data[i].data=NULL;	//
    i++;	
    //41
    md_data[i].tbl.start = MOD_REG_START_ADDR_40000;
    md_data[i].tbl.end = (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000);
    md_data[i].tbl.is_write= true;
    md_data[i].tbl.tbl_cb=Modbus_callback_40000;
    md_data[i].data=NULL;	//
    i++;
    // total: 41
    if(i != LEN_MD_DATA_ARRAY)
    {
        ESP_LOGE(TAG, "md_data_init ,index=%d ",i);// 
    }

}

//	{tbl.start, tbl.end, tbl.is_write, tbl.tbl_cb, data},	// 元素定义
//const EXT_RAM_BSS_ATTR md_data_t md_data[] = {
//	{{MOD_REG_START_ADDR_00000, (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000), false, Modbus_callback_00000},NULL},	// 
//	{{MOD_REG_START_ADDR_00100, (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100), false, Modbus_callback_00100},NULL},	// 
//	{{MOD_REG_START_ADDR_00700, (MOD_REG_START_ADDR_00700 + MOD_REG_LEN_00700), false, Modbus_callback_00700},NULL},	// 
//	{{MOD_REG_START_ADDR_01100, (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100), false, Modbus_callback_01100},NULL},	// 
//	{{MOD_REG_START_ADDR_01200, (MOD_REG_START_ADDR_01200 + MOD_REG_LEN_01200), false, Modbus_callback_01200},NULL},	// 
//	{{MOD_REG_START_ADDR_01300, (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300), false, Modbus_callback_01300},NULL},	// 
//	{{MOD_REG_START_ADDR_01400, (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400), false, Modbus_callback_01400},NULL},	// 
//	{{MOD_REG_START_ADDR_01500, (MOD_REG_START_ADDR_01500 + MOD_REG_LEN_01500), false, Modbus_callback_01500},NULL},	// 
//	{{MOD_REG_START_ADDR_01600, (MOD_REG_START_ADDR_01600 + MOD_REG_LEN_01600), false, Modbus_callback_01600},NULL},	// 
//	{{MOD_REG_START_ADDR_02000, (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000), false, Modbus_callback_02000},NULL},	// 
//
//	{{MOD_REG_START_ADDR_02200, (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200), false, Modbus_callback_02200},NULL},	// 
//	{{MOD_REG_START_ADDR_02400, (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400), false, Modbus_callback_02400},NULL},	// 
//	{{MOD_REG_START_ADDR_02500, (MOD_REG_START_ADDR_02500 + MOD_REG_LEN_02500), false, Modbus_callback_03000},NULL},	// 
//	{{MOD_REG_START_ADDR_03000, (MOD_REG_START_ADDR_03000 + MOD_REG_LEN_03000), false, Modbus_callback_03500},NULL},	// 
//	{{MOD_REG_START_ADDR_03500, (MOD_REG_START_ADDR_03500 + MOD_REG_LEN_03500), false, Modbus_callback_03600},NULL},	// 
//	{{MOD_REG_START_ADDR_03600, (MOD_REG_START_ADDR_03600 + MOD_REG_LEN_03600), false, Modbus_callback_06000},NULL},	// 
//	{{MOD_REG_START_ADDR_06000, (MOD_REG_START_ADDR_06000 + MOD_REG_LEN_06000), false, Modbus_callback_06000},NULL},	// 
//	{{MOD_REG_START_ADDR_06100, (MOD_REG_START_ADDR_06100 + MOD_REG_LEN_06100), false, Modbus_callback_06100},NULL},	// 
//	{{MOD_REG_START_ADDR_11000, (MOD_REG_START_ADDR_11000 + MOD_REG_LEN_11000), false, Modbus_callback_11000},NULL},	// 
//	{{MOD_REG_START_ADDR_12000, (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000), false, Modbus_callback_12000},NULL},	// 
//	{{MOD_REG_START_ADDR_13500, (MOD_REG_START_ADDR_13500 + MOD_REG_LEN_13500), false, Modbus_callback_13500},NULL},	// 
//};


static uint16_t tbl_cnt;						// 表数量


/**
* @brief :Modbus_beta_reg_table_register
初始化modbus beta寄存器表格的寄存器块和对应的回调函数关联

* @param[in] :	   
* @param[out] : 
* @return :	 
*/
void Modbus_beta_reg_table_register_init(void)
{
	uint16_t i=0;
//	uint16_t cnt=0;
	md_tbl_t md_tbl;

//    md_add_tbl(&(md_tbl_t){
//        .start = BLE_SET_TBL_START,
//        .end = BLE_SET_TBL_END,
//        .is_write = TBL_READ_WRITE,
//        .tbl_cb = ble_set_tbl_handler,
//    });
	md_data_init();

	tbl_cnt=sizeof(md_data)/sizeof(md_data[0]);

	for ( i = 0; i < tbl_cnt; i++) 
	{
//		md_data[i].tbl.tbl_cb = Modbus_beta_reg_callback_handler;
			
	}		

	
	ESP_LOGE(TAG, "windy tbl_cnt =%u ",tbl_cnt);
}

/**
 * @brief modbus错误响应
 *
 * @param in_buff 数据缓存
 * @param out_buff 响应缓存
 * @param len 数据长度
 * @return 无
 */
static void mb_rsp_error(uint8_t *in_buff, uint8_t *out_buff, int *len)
{
	uint8_t idx = 0;
	out_buff[idx++] = in_buff[0];
	out_buff[idx++] = in_buff[1] | MB_FUNCODE_ERROR;
	out_buff[idx++] = MB_ERROR_NOT_SUPPORTED;
	uint16_t crc = CalcCrc16_modbus(out_buff, idx);
	out_buff[idx++] = crc;
	out_buff[idx++] = crc >> 8;
	*len = idx;
}

/**
 * @brief 根据寄存器地址查找寄存器表
 *
 * @param reg_addr 寄存器地址
 * @return 成功返回寄存器表地址，失败返回NULL
 */
md_data_t *md_tbl_find(uint16_t reg_addr)
{
	
	ESP_LOGI(TAG,"tbl_cnt:%d",tbl_cnt);
	/* 查找寄存器表 */
	for (uint16_t i = 0; i < tbl_cnt; i++)
	{
		if ((reg_addr >= md_data[i].tbl.start) && (reg_addr <= md_data[i].tbl.end)) 
		{
			 ESP_LOGW(TAG, "windy1 in md_tbl_find:tbl_cnt=%u,i=%u,md_data[i].tbl.start=%u,md_data[i].tbl.end=%u",tbl_cnt,i, md_data[i].tbl.start,md_data[i].tbl.end);
		
			return &md_data[i];
		}
		// ESP_LOGW(TAG, "windy1 in md_tbl_find:tbl_cnt=%u,i=%u,md_data[i].tbl.start=%u,md_data[i].tbl.end=%u",tbl_cnt,i, md_data[i].tbl.start,md_data[i].tbl.end);
	}
	
	ESP_LOGE(TAG, "windy2 in md_tbl_find:tbl_cnt=%u,md_data[0].tbl.start=%u,md_data[0].tbl.end=%u",tbl_cnt, md_data[0].tbl.start,md_data[0].tbl.end);
	
	return NULL;
}


/**
windy 改进20240330
 * @brief 根据寄存器地址查找寄存器表对应的回调函数
 *
 @   *tbl：查表框架输入，not use
 @   *cb_data：当前寄存器输入索引
 @   *priv_data:私有特殊参数


 
 * @return 成功返回寄存器表地址，失败返回NULL
 */
md_data_t *Modbus_beta_reg_callback_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data)
{
	/* 查找寄存器表 */
//	for (int i = 0; i < tbl_cnt; i++)
//	{
//		if ((cb_data->reg_addr >= md_data[i].tbl.start) && (cb_data->reg_addr <= md_data[i].tbl.end)) 
//		{
//			if( == i)
//			{
//				Modbus_callback_00000;
//			}
//			else if( == i)
//			{
//				Modbus_callback_00000;
//			}		
//			return &md_data[i];
//		}
//	}
	return NULL;
}



/**
 * @brief 向modbus添加数据表
 *
 * @param p_tbl 数据表结构
 * @return 成功返回0，失败返回-1
 */
//int md_add_tbl(md_tbl_t *p_tbl)
//{
//	if (tbl_cnt >= MD_TBL_MAX) return -1;
//	if ((p_tbl == NULL) || (p_tbl->start > p_tbl->end)) return -1;
//
//	/* 判断新增的数据表是否与已添加的重复 */
//	for (int i = 0; i < tbl_cnt; i++)
//	{
//		if (!((p_tbl->end < md_data[i].tbl.start) || ( md_data[i].tbl.end < p_tbl->start)))
//		{
//			ESP_LOGE(TAG, "register address duplication");
//			return -1;
//		}
//	}
//
//	/* 保存寄存器表 */
//	memcpy(&md_data[tbl_cnt].tbl, p_tbl, sizeof(md_tbl_t));
//
//	/* 为新添加的寄存器表申请内存空间 */
//	uint16_t reg_nums = p_tbl->end - p_tbl->start + 1;
//	md_data[tbl_cnt].data = malloc(reg_nums * sizeof(uint16_t));
//	ESP_RETURN_ON_FALSE(md_data[tbl_cnt].data, -1, TAG, "memory malloc faield");
//
//	ESP_LOGW(TAG, "successfully registered modbus table, start: %d, end: %d, attribute: %s",
//			p_tbl->start, p_tbl->end, (p_tbl->is_write)?("read-write"):("read-only"));
//	tbl_cnt++;
//	return 0;
//}

/**
 * @brief modbus协议检查
 *
 * @param buff 指令数据缓存
 * @param len 数据长度
 * @return 成功返回0，失败返回-1
 */
int md_protocol_check(uint8_t *buff, int len)
{
    /* modbus协议检测及crc校验 */
    if(buff[0] != MB_ADDR) return -1;
    if((buff[1] != MB_WRITE_MULTI_HOLD_REG) &&
	   (buff[1] != MB_WRITE_ONE_HOLD_REG) &&
	   (buff[1] != MB_READ_HOLD_REG)) return -1;

    uint16_t crc_old = ((uint16_t)buff[len-1] << 8) | buff[len-2];
    uint16_t crc_new = CalcCrc16_modbus(buff, (len-2));
	return (crc_old==crc_new) ? (0) : (-1);
}

/**
 * @brief 从内部寄存器表中获取数据
 *
 * @param reg_addr 寄存器地址
 * @param data 数据
 * @param reg_nums 寄存器数
 * @return 成功返回0，失败返回-1
 */
int md_reg_data_get(uint16_t reg_addr, uint16_t *data, int reg_nums)
{
	md_data_t *p_data;
	uint16_t actual_nums = reg_nums;

	p_data = md_tbl_find(reg_addr);
	ESP_RETURN_ON_FALSE(p_data, -1, TAG, "find register table failure, line: %d", __LINE__);

	/* 如果寄存器数量大于寄存器表定义的大小,则使用实际寄存器大小,未定义的寄存器不动作 */
	uint16_t remain_len = p_data->tbl.end - reg_addr + 1;
	if (reg_nums > remain_len) {
		actual_nums = remain_len;
	}

	/* 从寄存器表中取出数据 */
	uint16_t *ptr = &p_data->data[reg_addr-p_data->tbl.start];
	memcpy(data, ptr, sizeof(uint16_t)*actual_nums);
	if (reg_nums > actual_nums) {	// 多余的数据清零
		memset(data+actual_nums, 0x00, sizeof(uint16_t)*(reg_nums-actual_nums));
	}
	return 0;
}

/**
 * @brief 设置数据到内部寄存器表
 *
 * @param reg_addr 寄存器地址
 * @param data 数据缓存
 * @param reg_nums 寄存器数
 * @return 成功返回0，失败返回-1
 */
int md_reg_data_set(uint16_t reg_addr, uint16_t *data, int reg_nums)
{
	md_data_t *p_data;
	uint16_t actual_nums = reg_nums;

	p_data = md_tbl_find(reg_addr);
	ESP_RETURN_ON_FALSE(p_data, -1, TAG, "find register table failure, line: %d", __LINE__);

	/* 如果设置的寄存器数量大于寄存器表定义的大小,则使用实际寄存器大小,未定义的寄存器不动作 */
	uint16_t remain_len = p_data->tbl.end - reg_addr + 1;
	if (reg_nums > remain_len) {
		actual_nums = remain_len;
	}

	/* 数据复制到寄存器表中 */
	uint16_t *ptr = &p_data->data[reg_addr-p_data->tbl.start];
	memcpy(ptr, data, sizeof(uint16_t)*actual_nums);
	return 0;
}



