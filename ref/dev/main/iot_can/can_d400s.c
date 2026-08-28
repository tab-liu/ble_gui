#include <stdint.h>
#include <string.h>
#include "can_init.h"
#include "esp_log.h"
#include "can_protocol.h"
#include "can_d400s.h"
#include "dev_discovery.h"
#include "uart_device_process.h"
#include"iot_period_task.h"


#define TAG "[CAN_D400S]"


void d400s_infos_summary(MOD_STRUCT_Inv_READ *invCan)
{
	uint8_t i,node;
	uint8_t index_mix=0xff;
	uint8_t d400s_num=0;
	uint8_t d400s_index=0xff;
	uint64_t d400s_sn[3];
	uint16_t d400s_type_1;
	uint16_t d400s_type_2;
	uint32_t version_d400s_iot=0;
	uint32_t version_d400s_dsp=0;
	uint16_t uFault_charger1=0;                 // dc1故障
    uint16_t uFault_dcdc=0;                 // dcdc故障
    uint16_t uProctect_dcdc=0;                 // dc保护
	//Inv_can[0].d400s_data[0].d400s_common_info.fault_dcdc=0xf;
	//Inv_can[0].d400s_data[0].d400s_common_info.ctrl_mode.bit.fault_exist=1;
	memset(d400s_sn,0,sizeof(d400s_sn));
	for(node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (i = 0; i < (D400S_MAX_NUM); i++) 
		{
			//ESP_LOGI(TAG,"Inv_can[%d].d400s_data[%d].online:%d d400s_sn:%llu",node,i,Inv_can[node].d400s_data[i].online,d400s_sn);
			if (invCan[node].d400s_data[i].online == 1)
			{
				d400s_sn[d400s_num]=GetUin64FromPtrSmall(invCan[node].d400s_data[i].d400s_common_info.d400s_sn);
				if(!d400s_sn[d400s_num])
					continue;

				if(index_mix>node)
					index_mix=node;

				d400s_num++;
				if(d400s_index==0xff)
					d400s_index=node;			

				d400s_fault_check(&invCan[node].d400s_data[i],node);
				ESP_LOGI(TAG,"invCan[%d].d400s_data[%d].d400s_common_info.dcdc_SoftwareVersion:%lu",node,i,invCan[node].d400s_data[i].d400s_common_info.dcdc_SoftwareVersion);
				if(invCan[node].d400s_data[i].d400s_common_info.dcdc_SoftwareVersion!=0)
				{
					if((version_d400s_dsp==0)||(version_d400s_dsp>invCan[node].d400s_data[i].d400s_common_info.dcdc_SoftwareVersion))
					{
						version_d400s_dsp=invCan[node].d400s_data[i].d400s_common_info.dcdc_SoftwareVersion;
					}
				}
				ESP_LOGI(TAG,"invCan[%d].d400s_data[%d].iot_can_11000.software_ver:%lu",node,i,invCan[node].d400s_data[i].iot_can_11000.software_ver);
				if(invCan[node].d400s_data[i].iot_can_11000.software_ver!=0)
				{
					if((version_d400s_iot==0)||(version_d400s_iot>invCan[node].d400s_data[i].iot_can_11000.software_ver))
					{
						version_d400s_iot=invCan[node].d400s_data[i].iot_can_11000.software_ver;
					}
				}
				uFault_charger1|= invCan[node].d400s_data[i].d400s_common_info.fault_charger1;
				uFault_dcdc|=invCan[node].d400s_data[i].d400s_common_info.fault_dcdc;
				uProctect_dcdc|=invCan[node].d400s_data[i].d400s_common_info.proctect_dcdc;
			}
		}
	}
	if(!d400s_num)
		return ;
	d400s_setting_summary(&invCan[d400s_index]);
	memcpy(invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.d400s_type,invCan[index_mix].d400s_data[0].d400s_common_info.d400s_type,sizeof(invCan[index_mix].d400s_data[0].d400s_common_info.d400s_type));
	memcpy(invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.d400s_sn,invCan[index_mix].d400s_data[0].d400s_common_info.d400s_sn,sizeof(uint64_t));
	//d400s_sn=GetUin64FromPtrSmall(invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.d400s_sn);
	invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dcdc_SoftwareType=DEVICE_DSP;
	invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dcdc_SoftwareVersion=version_d400s_dsp;
	invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.software_ver=version_d400s_iot;
	invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.fault_charger1=uFault_charger1;
	invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.fault_dcdc=uFault_dcdc;
	invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.proctect_dcdc=uProctect_dcdc;
	ESP_LOGI(TAG,"uFault_charger1:%u,uFault_dcdc:%u,uProctect_dcdc:%u",uFault_charger1,uFault_dcdc,uProctect_dcdc);		


	Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareType=invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dcdc_SoftwareType;
	Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareVersion=invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dcdc_SoftwareVersion;
	memcpy(Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.d400s_type,invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.d400s_type,sizeof(invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.d400s_type));
	memcpy(Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.d400s_sn,invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.d400s_sn,sizeof(uint64_t));
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.fault_charger1,(uint8_t *)&invCan[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.fault_charger1,(offsetof(d400s_common_info_struct,revd)-offsetof(d400s_common_info_struct,fault_charger1)));
	
	memcpy((uint8_t*)Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_type,(uint8_t*)Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.iot_type,sizeof(Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.iot_type));/*11000~11005*/
	Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_sn=Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.iot_sn; /*11006~11009*/
	Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_sn=Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.iot_sn; /*11010~11001安全码3*/
	Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.software_ver=Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.software_ver; /*11014~11015*/
	memcpy((uint8_t*)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.link,(uint8_t*)&Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.link,sizeof(Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.link));
	Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.Bind_SN=Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].iot_can_11000.Bind_SN; 
	
	ESP_LOGI(TAG,"Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.d400s_type:%s",Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.d400s_type);
	ESP_LOGI(TAG,"Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareVersion:%lu",Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dcdc_SoftwareVersion);
	//ESP_LOGI(TAG,"Inv_D400S[0].mod_reg15500_D400s_info.dcdc_SoftwareVersion:%lu",Inv_D400S[0].mod_reg15500_D400s_info.dcdc_SoftwareVersion);
	ESP_LOGI(TAG," d400s_sn:%llu",d400s_sn[0]);
	ESP_LOGI(TAG,"InvD400s uFault_charger1:%u,uFault_dcdc:%u,uProctect_dcdc:%u",Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.fault_charger1,Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.fault_dcdc,Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.proctect_dcdc);

	d400s_ota_check(invCan);
}

void d400s_fault_check(d400s_can_node_struct* d400s_node,uint8_t index)
{
	ESP_LOGI(TAG,"index:%d fault charger1:%u dcdc:%u protect:%u",index,d400s_node->d400s_common_info.fault_charger1,d400s_node->d400s_common_info.fault_dcdc,d400s_node->d400s_common_info.proctect_dcdc);
	reals.d400sPointInfos[index].pointState.all=INV_D400s_State_Update();
	reals.d400sPointInfos[index].pointState.state.point_online=1;
	if(d400s_node->d400s_common_info.fault_charger1||d400s_node->d400s_common_info.fault_dcdc||d400s_node->d400s_common_info.proctect_dcdc)
	{
		reals.d400sPointInfos[index].pointState.state.protect_fault_exist=1;
	}else{
		reals.d400sPointInfos[index].pointState.state.protect_fault_exist=0;
	}
	// 以下不处理 ctrl_mode.bit.fault_exist逻辑，因AP300相关字段处理与协议不同步
	// 这里故障字段有数据已经可能表明有故障存在，也无需再处理 ctrl_mode.bit.fault_exist逻辑
	// reals.d400sPointInfos[index].pointState.state.protect_fault_exist|=d400s_node->d400s_common_info.ctrl_mode.bit.fault_exist;
	ESP_LOGI(TAG,"reals.d400sPointInfos[index].pointState.all:0x%x",reals.d400sPointInfos[index].pointState.all);

}


void d400s_setting_summary(MOD_STRUCT_Inv_READ *invCan)
{
	memcpy((uint8_t *)&invCan[DEV_MAIN_NODE_MAX].d400s_data[INV_MAX_NUM].d400s_charger_set,(uint8_t *)&(invCan->d400s_data[0].d400s_charger_set),sizeof( d400s_charger_set_struct));
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.charger_set,(uint8_t *)(&invCan->d400s_data[0].d400s_charger_set.charger_set),sizeof(d400s_charger_set_t));
	memcpy(Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.dc_val_set,invCan->d400s_data[0].d400s_charger_set.dc_val_set,sizeof(invCan->d400s_data[0].d400s_charger_set.dc_val_set));
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.memory_val_set, (uint8_t *)&(invCan->d400s_data[0].d400s_charger_set.memory_val_set),sizeof(invCan->d400s_data[0].d400s_charger_set.memory_val_set));//  //15613 dc记忆开关模式设置
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.mode2_set, (uint8_t *)&(invCan->d400s_data[0].d400s_charger_set.mode2_set),sizeof(invCan->d400s_data[0].d400s_charger_set.mode2_set));//15614 dc充电模式设置1
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.mode3_set, (uint8_t *)&(invCan->d400s_data[0].d400s_charger_set.mode3_set),sizeof(invCan->d400s_data[0].d400s_charger_set.mode3_set));//15615 dc充电模式设置2
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.batteryCapacity_L, (uint8_t *)&(invCan->d400s_data[0].d400s_charger_set.batteryCapacity_L),sizeof(invCan->d400s_data[0].d400s_charger_set.batteryCapacity_L));//15616 铅酸电池容量 0.1AH
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.batteryCapacity_H, (uint8_t *)&(invCan->d400s_data[0].d400s_charger_set.batteryCapacity_H),sizeof(invCan->d400s_data[0].d400s_charger_set.batteryCapacity_H));//15617 铅酸电池容量 0.1AH
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.battery_Type, (uint8_t *)&invCan->d400s_data[0].d400s_charger_set.battery_Type,sizeof(invCan->d400s_data[0].d400s_charger_set.battery_Type));//15618 电池类型
	memcpy((uint8_t *)Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.dc_Power_Set, (uint8_t *)invCan->d400s_data[0].d400s_charger_set.dc_Power_Set,sizeof(invCan->d400s_data[0].d400s_charger_set.dc_Power_Set));//15619~15623 流入DC为正，流出为负
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.dc_Total_Power_Set, (uint8_t *)&invCan->d400s_data[0].d400s_charger_set.dc_Total_Power_Set,sizeof(invCan->d400s_data[0].d400s_charger_set.dc_Total_Power_Set));//15624
	memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15600_D400s_set.mode4_set, (uint8_t *)&invCan->d400s_data[0].d400s_charger_set.mode4_set,sizeof(invCan->d400s_data[0].d400s_charger_set.mode4_set));//15625
}

void d400s_ota_check(MOD_STRUCT_Inv_READ *invCan)
{
	//升级过程中清空D400S功率
	if(reals.ota_happen)
	{
		ESP_LOGI(TAG,"d400s_ota_check");
		memset(invCan->d400s_data[0].d400s_common_info.dc_info,0,sizeof(invCan->d400s_data[0].d400s_common_info.dc_info));
		memcpy((uint8_t *)&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.dc_info[0], (uint8_t *)&(Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dc_info[0]),sizeof(Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dc_info));  //dc 电压 、电流、功率     15531~15554
		Inv_can->inv_data[0].inv_base.line_event&=~(0x1);
		memset((uint8_t*)&Inv_can->inv_data[0].inv_pv.pv_detail[2],0,sizeof(pv_detail_struct));
		Inv_can->inv_data[0].inv_base.PVAllTotalPower=0;
	}
}