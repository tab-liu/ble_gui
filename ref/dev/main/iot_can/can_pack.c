#include <stdint.h>
#include <string.h>
#include "can_init.h"
#include "esp_log.h"
#include "can_protocol.h"
#include "can_pack.h"

#include "uart_device_process.h"
#include"iot_period_task.h"
#include "http_client.h"


#define TAG "[CAN_PACK]"

//#define DEBUG_1100			1
#define PACK_DEBUG   0

#define PACK_WORK_NULL			0	//空闲状态
#define PACK_WORK_CHARGE		1	//充电状态
#define PACK_WORK_DISCHARGE		2	//放电状态



//Pack_InfosCtrl pack_infosCtrl[MAX_PACK_TYPE_NUM];
//Pack_Info uPack_infos[SLAVE_PACK_MAX_NUM];//电池包[0~29]

static EXT_RAM_BSS_ATTR Pack_InfosCtrl infosCtrl[MAX_PACK_TYPE_NUM];
static EXT_RAM_BSS_ATTR Pack_Info uPack_infos[SLAVE_PACK_MAX_NUM];//电池包[0~29]

static EXT_RAM_BSS_ATTR Slave_Point_Infos PointInfos[PACK_SALVE_MAX_POINT];	//节点信息
static EXT_RAM_BSS_ATTR MOD_STRUCT_Pack Inv_Pack_Slave_WR[DEV_PACK_NUM_MAX];//各pack分配从机地址后的单独信息,
static EXT_RAM_BSS_ATTR uint32_t packVersion[SLAVE_MIN_VERSION_COUNT];
static EXT_RAM_BSS_ATTR NewPack_Type_Ctrls New_Pack_Ctrl;//新产品类型电池包信息管理
//static SemaphoreHandle_t uPackInfo_update_Sem = NULL; 
#if 1


/*
电池包信息总结
*/
void pack_infos_init(void)
{
	memset(infosCtrl,0,sizeof(infosCtrl));
	memset(uPack_infos,0,sizeof(uPack_infos));
	memset(PointInfos,0,sizeof(PointInfos));
	memset(Inv_Pack_Slave_WR,0,sizeof(Inv_Pack_Slave_WR));
	memset(packVersion,0,sizeof(packVersion));
	memset(&New_Pack_Ctrl,0,sizeof(New_Pack_Ctrl));
	//pack_semaphore_init();
}

#ifdef FUNC_PACK_NEWTYPE_CTRL
/*------------------------------------------------------------------------------
 Function: app_log_level_set
 -----------------------------------------------------------------------------*/
/**
  * @brief      调试信息打印等级设置
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
uint8_t pack_new_type_check(char* type )
{
	uint8_t index=0xff;
	//未获取过新产品类型的电池包信息时，直接写入当前管理中,否则先判断是否已记录过该新增类型
	ESP_LOGI(TAG,"New_Pack_Ctrl.new_pack_num:%d",New_Pack_Ctrl.new_pack_num);
	if(!New_Pack_Ctrl.new_pack_num)
	{
		strncpy(New_Pack_Ctrl.typeinfo[New_Pack_Ctrl.new_pack_num].type,type,TYPE_SIZE);
		New_Pack_Ctrl.typeinfo[New_Pack_Ctrl.new_pack_num].index=New_Pack_Ctrl.new_pack_num+SLAVE_NEW_TYPE_INDEX;
		index=New_Pack_Ctrl.new_pack_num + SLAVE_NEW_TYPE_INDEX;
		New_Pack_Ctrl.new_pack_num++;
		goto _end;
	}else{
		for(int i=0;i<New_Pack_Ctrl.new_pack_num;i++)
		{
			if(0==strncmp(New_Pack_Ctrl.typeinfo[i].type,type,TYPE_SIZE))
			{
				ESP_LOGI(TAG,"type:%s already exist in New_Pack_Ctrl.typeinfo[%d]:%d",type,i,New_Pack_Ctrl.typeinfo[i].index);
				index=New_Pack_Ctrl.typeinfo[i].index;
				goto _end;
			}
		}
	}
	//该电池包类型不属于当前已记录过的任一台新增设备类型，需要新增记录
	if(New_Pack_Ctrl.new_pack_num<NEW_PACK_TYEP_NUM)
	{
		strncpy(New_Pack_Ctrl.typeinfo[New_Pack_Ctrl.new_pack_num].type,type,TYPE_SIZE);
		New_Pack_Ctrl.typeinfo[New_Pack_Ctrl.new_pack_num].index=New_Pack_Ctrl.new_pack_num+SLAVE_NEW_TYPE_INDEX;
		index=New_Pack_Ctrl.new_pack_num + SLAVE_NEW_TYPE_INDEX;
		New_Pack_Ctrl.new_pack_num++;
	}

_end:
	return index;
}
#endif
#if 0
/*------------------------------------------------------------------------------
 Function: Modbus_21000_semaphore_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      信号量初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void pack_semaphore_init(void)
{
	uPackInfo_update_Sem = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(uPackInfo_update_Sem == NULL)
	{
        ESP_LOGE(TAG, "uPackInfo_update_Sem create failed");
		return;
	}
	xSemaphoreGive(uPackInfo_update_Sem);						/**< the resource is available after creation */
}

/*------------------------------------------------------------------------------
 Function: Modbus_21000_semaphore_Take
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取信号量
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t pack_semaphore_Take(void)
{
    if (!uPackInfo_update_Sem || xSemaphoreTake(uPackInfo_update_Sem, pdMS_TO_TICKS(200)) != pdTRUE)
    {
        ESP_LOGE(TAG,"uPackInfo_update_Sem error");
        return 0; 
    }

    return 1;
}

/*------------------------------------------------------------------------------
 Function: pack_semaphore_Give
 -----------------------------------------------------------------------------*/
/**
  * @brief      释放信号量
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void pack_semaphore_Give(void)
{
    xSemaphoreGive(uPackInfo_update_Sem);  /*释放信号量*/
}
#endif
/*
电池包类型总结 获取一个电池类型的不同版本号数量和最小版本号
*/

/*
电池包保护状态转换
CAN_BETA:
Bit0=1表示有充电过温保护，Bit0=0表示无(E081)
Bit1=1表示有放电过温保护，Bit1=0表示无(E082)
Bit2=1表示有充电低温保护，Bit2=0表示无
Bit3=1表示有放电低温保护，Bit3=0表示无
Bit4=1表示有总电池过压保护，Bit4=0表示无
Bit5=1表示有总电池欠压保护，Bit5=0表示无
Bit6=1表示有单体过压保护，Bit6=0表示无
Bit7=1表示有单体欠压保护，Bit7=0表示无
Bit8=1表示有过流保护1，Bit8=0表示无：
Bit9=1表示有过流保护2，Bit9=0表示无：
Bit10=1表示有过流保护3，Bit10=0表示无：
Bit11=1表示有过流保护4，Bit11=0表示无：
Bit12=1表示有预充故障，Bit12=0表示无：
Bit13=1表示有短路保护，Bit13=0表示无：
Bit14=1表示有AFE故障， Bit14=0表示无(E095)

MODBUS:
电池保护状态1：
bit0	总压过压保护
bit1	保留
bit2	电池簇充电过流保护1
bit3	电池簇充电过流保护2
bit4	电池簇充电过流保护3
bit5	充电过温保护
bit6	充电低温保护
bit7	单节过压保护
bit8	电池高电荷保护

电池保护状态2：
bit0	总压欠压保护
bit1	保留
bit2	电池簇放电过流保护1
bit3	电池簇放电过流保护2
bit4	电池簇放电过流保护3
bit5	放电过温保护
bit6	放电低温保护
bit7	单节欠压保护
*/
uint32_t pack_procte_convert(const pack_base_struct* packbase)
{
	single_pack_protect protect; 
	protect.all=0;

	//电池保护状态1
	//bit0	总压过压保护
	protect.bit.total_overvoltage=packbase->protect_status.bit.total_overvoltage;

	if(packbase->chg_status==PACK_WORK_CHARGE)
	{
		//bit2	电池簇充电过流保护1
		protect.bit.charge_overcurrent1=packbase->protect_status.bit.overcurrent1;
		//bit3	电池簇充电过流保护2
		protect.bit.charge_overcurrent2=packbase->protect_status.bit.overcurrent2;
		//bit4	电池簇充电过流保护3
		protect.bit.charge_overcurrent3=packbase->protect_status.bit.overcurrent3;
	}
	//bit5	充电过温保护
	protect.bit.charge_overtemp=packbase->protect_status.bit.chargeovertemp;
	//bit6	充电低温保护
	protect.bit.charge_lowtemp=packbase->protect_status.bit.chargelowtemp;
	//bit7	单节过压保护
	protect.bit.single_overvoltage=packbase->protect_status.bit.single_overvoltage;	

	//电池保护状态2
	//bit0	总压欠压保护
	protect.bit.total_undervoltage=packbase->protect_status.bit.total_undervoltage;

	if(packbase->chg_status!=PACK_WORK_CHARGE)
	{
		//bit2	电池簇放电过流保护1
		protect.bit.discharge_overcurrent1=packbase->protect_status.bit.overcurrent1;
		//bit3	电池簇放电过流保护2
		protect.bit.discharge_overcurrent2=packbase->protect_status.bit.overcurrent2;
		//bit4	电池簇放电过流保护3
		protect.bit.discharge_overcurrent3=packbase->protect_status.bit.overcurrent3;
	}
	//bit5	放电过温保护
	protect.bit.discharge_overtemp=packbase->protect_status.bit.dischargeovertemp;	
	//bit6	放电低温保护
	protect.bit.discharge_lowtemp=packbase->protect_status.bit.dischargelowtemp;	
	//bit7	单节欠压保护
	protect.bit.single_undervoltage=packbase->protect_status.bit.single_undervoltage;

	//ESP_LOGI(TAG,"pack_procte_convert:protect_status 0x%x->protect 0x%lx",packbase->protect_status.all,protect.all);

	return protect.all;
}

/* 将 CAN pack 实时数据同步到 Modbus 6100/6300，供 summary 整段 copy 前填充 */
static void pack_mod06100_sync_from_can(MOD_STRUCT_reg06100 *reg, const pack_base_struct *base,
	const pack_extend_struct *ext)
{
	/* 0x51 pack_base → 6111~6129、6144~6153、6160~6161 */
	reg->total_voltage = base->total_voltage;
	reg->total_current = base->total_current;
	reg->soc = base->soc;
	reg->soh = base->soh;
	reg->avg_temp = base->avg_temp;
	reg->min_cell_voltage = base->min_cell_voltage;
	reg->max_cell_voltage = base->max_cell_voltage;
	reg->min_cell_index = base->min_cell_index;
	reg->max_cell_index = base->max_cell_index;
	reg->min_temp_value = base->min_temp_value;
	reg->max_temp_value = base->max_temp_value;
	reg->min_temp_index = base->min_temp_index;
	reg->max_temp_index = base->max_temp_index;
	reg->work_status = base->work_status;
	reg->chg_status = base->chg_status;
	reg->relay = base->relay;
	reg->pack_cap_online = base->pack_cap_online;
	reg->protect.all = pack_procte_convert(base);
	memcpy(reg->SysErr, base->fault, sizeof(base->fault));
	memcpy(&reg->alarm1, base->alarm, sizeof(base->alarm));
	reg->chg_full_time = base->chg_full_time;
	reg->dsg_empty_time = base->dsg_empty_time;
	reg->pack_total_cell = base->pack_total_cell;
	reg->pack_total_ntc = base->pack_total_ntc;
	reg->pack_outsum_voltage = (uint16_t)base->vbus;
	reg->allow_max_chg_current = base->allow_max_chg_current;
	reg->allow_max_dsg_current = base->allow_max_dsg_current;

	/* 0x52 pack_extend → 6131~6143 */
	reg->capacity = ext->capacity;
	reg->total_chg_energy = ext->total_chg_energy;
	reg->total_dsg_energy = ext->total_dsg_energy;
	reg->once_chg_energy = ext->once_chg_energy;
	reg->once_dsg_energy = ext->once_dsg_energy;
	reg->sir_value = ext->sir_value;
	reg->inner_value = ext->inner_value;
	reg->cycle_count = ext->cycle_count;
}

static void pack_mod06300_sync_from_can(MOD_STRUCT_reg06300 *reg, const pack_debug1_struct *debug)
{
	memcpy(reg, debug, sizeof(MOD_STRUCT_reg06300));
}


void pack_group_summary(Pack_InfosCtrl*uCtrl, MOD_STRUCT_Inv_READ *invCan)
{
	uint8_t i,j,k;
	uint8_t count=0;
	uint8_t minCount=0;
	uint8_t uIndex=0;
	memset(packVersion,0,sizeof(packVersion));
	char type[TYPE_SIZE];
	//ESP_LOGI(TAG,"pack_group_summary onlineCount:%d",uCtrl->onlineCount);
	for(i=0;i<uCtrl->onlineCount;i++)
	{
		for(j=0;j<SLAVE_MIN_VERSION_COUNT;j++)
		{
			//ESP_LOGI(TAG,"uCtrl->pack_infos[%d].packSoftVersion[%d]:%lu",i,j,uCtrl->pack_infos[i].packSoftVersion[j]);
			if(uCtrl->pack_infos[i].packSoftVersion[j])
			{
				
				//ESP_LOGI(TAG,"packVersion[%d]:%ld",count,packVersion[count]);
				if(!count)
				{
					packVersion[count]=uCtrl->pack_infos[i].packSoftVersion[j];
					count++;
				}
				else
				{
					int ret=-1;
					for(k=0;k<count;k++)
					{
						//ESP_LOGI(TAG,"k:%d count:%d,uCtrl->pack_infos[%d].packSoftVersion[%d]:%lu",k,count,i,j,uCtrl->pack_infos[i].packSoftVersion[j]);
						ret=findFirstMatchVersion(packVersion,SLAVE_MIN_VERSION_COUNT,uCtrl->pack_infos[i].packSoftVersion[j]);
						if(ret>=0)
						{
							if(packVersion[ret]>uCtrl->pack_infos[i].packSoftVersion[j])
							{
								packVersion[ret]=uCtrl->pack_infos[i].packSoftVersion[j];
								minCount=i;
							}
							break;
						}
						//ESP_LOGI(TAG,"ret:%d",ret);
						if(ret==-1)
						{
							packVersion[count]=uCtrl->pack_infos[i].packSoftVersion[j];
							count++;
						}
					}
				}
			}
		}
	}

	uCtrl->versionCount=count;	//该电池包类型软件版本计数
	//ESP_LOGI(TAG,"versionCount:%d minCount:%d type_ascii:%s",uCtrl->versionCount,minCount,uCtrl->type_ascii);
	memcpy(&(uCtrl->minPack_Info),&(uCtrl->pack_infos[minCount]),sizeof(Pack_Info));
	for(i=0;i<count;i++)
	{
		uCtrl->minVersion[i]=packVersion[i];
	}

	strncpy(type, uCtrl->type_ascii, TYPE_SIZE);
	//ESP_LOGI(TAG,"pack_group_summary type:%s",type);
	//ESP_LOGW(TAG,"count == %d, versions[%d].count_version == %d",count,i,versions[i].count_version);
	uint8_t found = 0;
	if(strcmp(type,"B300") == 0)
	{
		uIndex=SLAVE_B300_GROUP_ADDR;
		found=1;
	}
	else if(strcmp(type,"B300S") == 0)
	{
		uIndex=SLAVE_B300S_GROUP_ADDR;
		found=1;
	}
	else if(strcmp(type,"B300K") == 0)
	{
		uIndex=SLAVE_B300K_GROUP_ADDR;
		found=1;	
	}
	else if(strcmp(type,"B500K") == 0)
	{
		uIndex=SLAVE_B500K_GROUP_ADDR;
		found=1;	
	}
#ifdef FUNC_PACK_NEWTYPE_CTRL			
	else{
		for(i=0;i<New_Pack_Ctrl.new_pack_num;i++)
		{
			if(0==strncmp(New_Pack_Ctrl.typeinfo[i].type,type,TYPE_SIZE))
			{
				uIndex=New_Pack_Ctrl.typeinfo[i].index;
				ESP_LOGI(TAG,"uIndex:%d,i:%d,type:%s",uIndex,i,type);
				found=1;
				break;
			}
		}
		
	}
#endif	
	if(found)
	{
		strncpy(Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.type_ascii,type,TYPE_SIZE);//传递pack类型 6101
		Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.software_total = uCtrl->versionCount;//每个类型号下面版本号数量 6173
		Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.PackBMUCnt =  uCtrl->onlineCount;//传递该pack类型下的在线电池包数量
		Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.sn_code = uCtrl->minPack_Info.packSn;   		//传递该pack类型下的第一个SN 6107
		for(j=0;j<PACK_SOFT_NUM;j++)
		{
			Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.soft[j].type = PACK_DEVICE_BMS;//传递pack固件类型 6174
			Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.soft[j].version = uCtrl->minVersion[j];//传递每个类型下pack固件版本号 6174
		}
		if(uCtrl->versionCount>1)// 相同电池包类型有不同版本号 
		{
			Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 0);//同一电池包类型固件里，前四位料号有差异
		}
		if((uIndex == SLAVE_B300S_GROUP_ADDR) || (uIndex == SLAVE_B300_GROUP_ADDR))
		{
			if(uCtrl->onlineCount > 1)
			{
				Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1);//料号相同，但为B300或B300S并包
			}
		}
		ESP_LOGI(TAG,"pack_group_summary 000 --- uindex == %d, j== %d, type == %s,sn=%llu;Inv_Pack_Slave_WR_version == %ld",
			uIndex,j,Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.type_ascii,Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.sn_code,Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.soft[j].version);
		{
			const Pack_Info *min_pack = &uCtrl->minPack_Info;
			pack_mod06100_sync_from_can(&Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each,
				&invCan[min_pack->node_idx].pack_data[min_pack->pack_idx].pack_base,
				&invCan[min_pack->node_idx].pack_data[min_pack->pack_idx].pack_extend);
			pack_mod06300_sync_from_can(&Inv_Pack_Slave_WR[uIndex].mod_reg06300_Pack_cell[0],
				&invCan[min_pack->node_idx].pack_data[min_pack->pack_idx].pack_debug1);
			Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.PackBMUCnt = uCtrl->onlineCount;
		}
	}
}



void pack_infos_summary(MOD_STRUCT_Inv_READ *invCan)
{
	pack_bmsinfos_summary(invCan);
	pack_Extinfos_summary(invCan);
}

//内置电池包信息总计
void pack_bmsinfos_summary(MOD_STRUCT_Inv_READ *invCan)
{
	int ret=0;
	memset(Inv_Pack_Slave_WR,0,sizeof(Inv_Pack_Slave_WR));
	for(int i = 0;i < DEV_MAIN_NODE_MAX;i++)
	{
		//ESP_LOGI(TAG,"pack_bmsinfos_summary ONLINE:%d  sn:%llu",invCan[i].pack_data[0].online,Inv_can[0].pack_data[0].pack_about.sn_code);
		if(invCan[i].pack_data[0].online == 1)//&&(invCan[i].pack_data[0].pack_about.sn_code))
		{	
			uint16_t count=0;
			for(uint8_t k=0;k<PACK_SOFT_NUM;k++)
			{
				if((invCan[i].pack_data[0].pack_about.soft[k].version))
				{
					if(!Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count].version)
					{
						Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count].type= PACK_DEVICE_BMS;
						Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count].version= invCan[i].pack_data[0].pack_about.soft[k].version;
						count++;
					}else{
						if((Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count-1].version/100)==invCan[i].pack_data[0].pack_about.soft[k].version)
						{
							if(Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count-1].version>invCan[i].pack_data[0].pack_about.soft[k].version)
							{
								Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count-1].type= PACK_DEVICE_BMS;
								Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count-1].version=invCan[i].pack_data[0].pack_about.soft[k].version;
							}
						}else
						{
								Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count].type= PACK_DEVICE_BMS;
								Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.soft[count].version=invCan[i].pack_data[0].pack_about.soft[k].version;
								count++;
						}
					}

				}
			}
			strncpy(Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.type_ascii,invCan[i].pack_data[0].pack_about.type_ascii,TYPE_SIZE);//传递pack类型 6101
			ESP_LOGI(TAG,"pack_bmsinfos_summary[%d] sn:%llu type:%s-%s soft version:%ld",i,invCan[i].pack_data[0].pack_about.sn_code,Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.type_ascii,invCan[i].pack_data[0].pack_about.type_ascii,invCan[i].pack_data[0].pack_about.soft[0].version);
			Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.software_total = count;	//每个类型号下面版本号数量 6173
			//Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.PackBMUCnt = infosCtrl[uIndex2].onlineCount;		//传递该pack类型下的在线电池包数量
			Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.sn_code = invCan[i].pack_data[0].pack_about.sn_code;   		//传递该pack的SN 6107
			pack_mod06100_sync_from_can(&Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each,
				&invCan[i].pack_data[0].pack_base, &invCan[i].pack_data[0].pack_extend);
			ESP_LOGI(TAG,"invCan[%d].pack_data[0].pack_base.chg_status:%d",i,invCan[i].pack_data[0].pack_base.chg_status);
			pack_mod06300_sync_from_can(&Inv_Pack_Slave_WR[i].mod_reg06300_Pack_cell[0],
				&invCan[i].pack_data[0].pack_debug1);
			//ESP_LOGI(TAG,"pack_debug1.cells:%d",invCan[i].pack_data[0].pack_debug1.cells);
			//memcpy((uint8_t *)&Inv_Pack[i].mod_reg06300_Pack_cell[0],(uint8_t *)&invCan[i].pack_data[j].pack_debug1,sizeof(pack_debug1_struct),sizeof(MOD_STRUCT_reg06300));
			memcpy((uint8_t *)&(Inv_Pack[i].mod_reg06100_Pack_each),(uint8_t *)&(Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each),sizeof(MOD_STRUCT_reg06100));
			memcpy((uint8_t *)&(Inv_Pack[i].mod_reg06300_Pack_cell[0]),(uint8_t *)&(Inv_Pack_Slave_WR[i].mod_reg06300_Pack_cell[0]),sizeof(MOD_STRUCT_reg06300));
		}
	}

    //ESP_LOGI(TAG,"Inv_Pack[%d].mod_reg06100_Pack_each sn_code:%llu",reals.Addr_can_self,Inv_Pack[reals.Addr_can_self].mod_reg06100_Pack_each.sn_code);
	memcpy((uint8_t *)&(Inv_Pack[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each),(uint8_t *)&(Inv_Pack[reals.Addr_can_self].mod_reg06100_Pack_each),sizeof(MOD_STRUCT_reg06100));
	memcpy((uint8_t *)&(Inv_Pack[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg06300_Pack_cell[0]),(uint8_t *)&(Inv_Pack[reals.Addr_can_self].mod_reg06300_Pack_cell[0]),sizeof(MOD_STRUCT_reg06300));
	memcpy((uint8_t *)&(Inv_Pack[INV_MAX_NUM*DEV_MAIN_NODE_MAX].mod_reg07000_Pack_set),(uint8_t *)&(Inv_Pack[reals.Addr_can_self].mod_reg07000_Pack_set),sizeof(MOD_STRUCT_reg07000));	
	//ESP_LOGI(TAG,"Inv_Pack[INV_MAX_NUM*DEV_MAIN_NODE_MAX] sn_code:%llu",Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.sn_code);
}



//外置电池包信息总结
void pack_Extinfos_summary(MOD_STRUCT_Inv_READ *invCan)
{
	uint8_t count = 0;//电池包数量 0
	uint8_t packType_count=0;//电池类型数量
	uint8_t flag_mix_version = 0;//相同类型电池包有不同版本混杂标志

	uint8_t uAddr=SLAVE_SINGLE_PACK_READ_ADDR;
	uint8_t uType=0;
	uint8_t i,j,k;
	//Pack_InfosCtrl infosCtrl[MAX_PACK_TYPE_NUM];
	//Pack_Info uPack_infos[SLAVE_PACK_MAX_NUM];//电池包[0~29]
	Data versions[MAX_DATA_SLAVE] = {0};//存储各个类型号，及每个类型号下在线的电池包数量(考虑到出现所有电池包全是不同类型的情况)
	char type[TYPE_SIZE];
	uint8_t uIndex=0;
	uint8_t uIndex2=0;
	uint8_t uB300orB300S_Mix_Flag=0;
	uint8_t dev_diff_flag=0;
	//if(!pack_semaphore_Take())
	//{
		//return;
	//}

	memset(infosCtrl,0,sizeof(infosCtrl));
	memset(uPack_infos,0,sizeof(uPack_infos));
	memset(PointInfos,0,sizeof(PointInfos));
	memset(Inv_Pack_Slave_WR,0,sizeof(Inv_Pack_Slave_WR));
	memset(&New_Pack_Ctrl,0,sizeof(New_Pack_Ctrl));
	
	for(i=0;i<MAX_PACK_TYPE_NUM;i++)
	{
		infosCtrl[SLAVE_B300K_INDEX+i].minGroupAddr=SLAVE_PACK_READ_ADDR+i;
		//ESP_LOGI(TAG,"infosCtrl[%d].minGroupAddr:0x%x",SLAVE_B300K_INDEX+i,infosCtrl[SLAVE_B300K_INDEX+i].minGroupAddr);
	}
	#if 1
	// 外置电池包
	for(i = 0;i < DEV_MAIN_NODE_MAX;i++)
	{
		uint8_t B300orB300Scount=0;
		uint8_t dev_diff_pack_mask=0;
		uint8_t dev_diff_pack_count=0;
		//ESP_LOGI(TAG,"111sn:%llu type:%s soft version:%ld",invCan[i].pack_data[0].pack_about.sn_code,invCan[i].pack_data[0].pack_about.type_ascii,invCan[i].pack_data[0].pack_about.soft[0].version);
		for(j = 1;j < PACK_MAX_NUM;j++)//不包含主包			
		{
			uint32_t newVersion;
			if((invCan[i].pack_data[j].online == 1)&&(invCan[i].pack_data[j].pack_about.sn_code))
			{				
				strncpy(type, invCan[i].pack_data[j].pack_about.type_ascii, TYPE_SIZE);
				newVersion = invCan[i].pack_data[j].pack_about.soft[0].version;
				ESP_LOGI(TAG,"i:%d,j:%d,111sn:%llu type:%s soft version:%ld",i,j,invCan[i].pack_data[j].pack_about.sn_code,invCan[i].pack_data[j].pack_about.type_ascii,invCan[i].pack_data[j].pack_about.soft[0].version);	
				// 检查是否已存在此类型号
				int found = 0;

				if(strcmp(type,"B300") == 0)
				{
					uIndex=SLAVE_B300_INDEX;
					uType=PACK_TYPE_B300;
					B300orB300Scount++;
					found=1;
					dev_diff_pack_mask|=SLAVE_B300_MASK;
				}
				else if(strcmp(type,"B300S") == 0)
				{
					uIndex=SLAVE_B300S_INDEX;
					uType=PACK_TYPE_B300S;
					B300orB300Scount++;
					found=1;
					dev_diff_pack_mask|=SLAVE_B300S_MASK;
				}
				else if(strcmp(type,"B300K") == 0)
				{
					uIndex=SLAVE_B300K_INDEX;
					uType=PACK_TYPE_B300K;
					found=1;
					dev_diff_pack_mask|=SLAVE_COMMON_MASK;					
				}
				else if(strcmp(type,"B500K") == 0)
				{
					uIndex=SLAVE_B500K_INDEX;
					uType=PACK_TYPE_B500K;
					found=1;
					dev_diff_pack_mask|=SLAVE_COMMON_MASK;	
				}	
#ifdef FUNC_PACK_NEWTYPE_CTRL			
				else{
					uIndex=pack_new_type_check(type);
					uType=	PACK_TYPE_NEW;	//新电池包类型
					found=1;
					dev_diff_pack_mask|=SLAVE_COMMON_MASK;	
					ESP_LOGI(TAG,"NEW TYPE :%s index:%d",type,uIndex);
				}
#endif
				ESP_LOGI(TAG,"invCan[%d].pack_data[%d].online:%d type:%s found:%d",i,j,invCan[i].pack_data[j].online,type,found);
				if(found)
				{	
					memcpy(infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].type_ascii,type,sizeof(infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].type_ascii)); 
					memcpy(uPack_infos[count].type_ascii,type,sizeof(uPack_infos[count].type_ascii)); 
					for(k=0;k<PACK_SOFT_NUM;k++)
					{
						if(invCan[i].pack_data[j].pack_about.soft[k].version)
						{
							infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].packSoftVersion[k]=invCan[i].pack_data[j].pack_about.soft[k].version;
							uPack_infos[count].packSoftVersion[k]=invCan[i].pack_data[j].pack_about.soft[k].version;
						}
					}
					strncpy(infosCtrl[uIndex].type_ascii,type, TYPE_SIZE);
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].packSn=invCan[i].pack_data[j].pack_about.sn_code;
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].packDetail= pack_procte_convert(&invCan[i].pack_data[j].pack_base);
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].single_Cell_and_Temp_Count = (invCan[i].pack_data[j].pack_base.pack_total_ntc << 8) | (invCan[i].pack_data[j].pack_base.pack_total_cell & 0xFF);
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].packSlaveInfo.Info.packSlaveAddr = (uAddr+count);//uAddr+infosCtrl[uIndex].onlineCount;
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].packType = uType;
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].soc=invCan[i].pack_data[j].pack_base.soc;
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].chg_status =invCan[i].pack_data[j].pack_base.chg_status;		// 传递该pack包的充电状态 6125	
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].node_idx = i;
					infosCtrl[uIndex].pack_infos[infosCtrl[uIndex].onlineCount].pack_idx = j;


					uPack_infos[count].packSn=invCan[i].pack_data[j].pack_about.sn_code;
					uPack_infos[count].packDetail= pack_procte_convert(&invCan[i].pack_data[j].pack_base);
					uPack_infos[count].single_Cell_and_Temp_Count = (invCan[i].pack_data[j].pack_base.pack_total_ntc << 8) | (invCan[i].pack_data[j].pack_base.pack_total_cell & 0xFF);
					uPack_infos[count].packType = uType;
					uPack_infos[count].soc = invCan[i].pack_data[j].pack_base.soc;
					uPack_infos[count].packSlaveInfo.Info.packSlaveAddr = uAddr+count;//uAddr+infosCtrl[uIndex].onlineCount;
					uPack_infos[count].packSlaveInfo.Info.uGroupIndex=uIndex+SLAVE_PACK_READ_ADDR;//infosCtrl[uIndex].minGroupAddr;
					uPack_infos[count].groupAddr=uIndex+SLAVE_PACK_READ_ADDR;//infosCtrl[uIndex].minGroupAddr;
					uPack_infos[count].chg_status =invCan[i].pack_data[j].pack_base.chg_status;		// 传递该pack包的充电状态 6125	
					uPack_infos[count].node_idx = i;
					uPack_infos[count].pack_idx = j;
					memcpy((uint8_t *)&uPack_infos[count].packDebug1,(uint8_t *)&invCan[i].pack_data[j].pack_debug1,sizeof(pack_debug1_struct));

					infosCtrl[uIndex].onlineCount++;
					count++;
				}
			}
			if(B300orB300Scount>1)
			{
				uB300orB300S_Mix_Flag++;	//有任意一个逆变器接超过2台B300S或B300，则不能升级
				ESP_LOGI(TAG,"uB300orB300S_Mix_Flag:%d",uB300orB300S_Mix_Flag);
			}
		}
		ESP_LOGI(TAG,"dev_diff_pack_mask:0x%x",dev_diff_pack_mask);
		for(uint8_t n=0;n<SLAVE_PACK_MAX_COUNT;n++ )
		{
			if(dev_diff_pack_mask&(1<<n))
			{
				dev_diff_pack_count++;
			}
		}
		ESP_LOGI(TAG,"dev_diff_pack_count:%d",dev_diff_pack_count);
		if(dev_diff_pack_count>1){dev_diff_flag=1;}

	}//end for
	//
	#endif
	ESP_LOGI(TAG,"count:%d",count);
	#if 1
	for(i=0;i<MAX_PACK_TYPE_NUM;i++)
	{
		pack_group_summary(&infosCtrl[i], invCan);
		// if(infosCtrl[i].onlineCount)
		// {
		// 	//packType_count++;	//不同电池包类型记数
		// 	ESP_LOGI(TAG,"infosCtrl[%d].onlineCount:%d,packType_count:%d",i,infosCtrl[i].onlineCount,packType_count);
		// }
		if(infosCtrl[i].versionCount>1)
		{
			ESP_LOGI(TAG,"infosCtrl[%d].versionCount:%d",i,infosCtrl[i].versionCount);
			flag_mix_version=1;//相同电池包类型有不同版本号
		}
	}
	// 6172 当同一类型电池包(如B300K)有不同的版本号时(如1014.xx和1021.xx)也不允许升级；当仅为B300或B300S其中一种时，如果并包数量大于1也不允许升级；判断为混包时无法升级；
	for(uint i = 0;i < MAX_PACK_TYPE_NUM;i++)
	{
		Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each.pack_ver_diff = 0;
	}

	for(i = 0;i < count;i++)
	{
		uint8_t count_cell = 0;// 某个类型电池包总的电芯数量
		uint8_t count_temp = 0;// 某个类型电池包总的探头数量
		strncpy(type, uPack_infos[i].type_ascii, TYPE_SIZE);
		uIndex= SLAVE_SINGLE_PACK_ADDR+i;	//Inv_Pack_Slave_WR 使用地址从SLAVE_SINGLE_PACK_ADDR开始
		if(strcmp(type,"B300") == 0)
			uIndex2=SLAVE_B300_INDEX;
		else if(strcmp(type,"B300S") == 0)
			uIndex2=SLAVE_B300S_INDEX;
		else if(strcmp(type,"B300K") == 0)
			uIndex2=SLAVE_B300K_INDEX;
		else if(strcmp(type,"B500K") == 0)
			uIndex2=SLAVE_B500K_INDEX;
		if(uIndex2<SLAVE_MAX_INDEX)
		{
			strncpy(Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.type_ascii,uPack_infos[i].type_ascii,TYPE_SIZE);//传递pack类型 6101
			Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.software_total = 1;	//每个类型号下面版本号数量 6173
			//Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.PackBMUCnt = infosCtrl[uIndex2].onlineCount;		//传递该pack类型下的在线电池包数量
			Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.sn_code = uPack_infos[i].packSn;   		//传递该pack的SN 6107
			pack_mod06100_sync_from_can(&Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each,
				&invCan[uPack_infos[i].node_idx].pack_data[uPack_infos[i].pack_idx].pack_base,
				&invCan[uPack_infos[i].node_idx].pack_data[uPack_infos[i].pack_idx].pack_extend);
			pack_mod06300_sync_from_can(&Inv_Pack_Slave_WR[uIndex].mod_reg06300_Pack_cell[0],
				&invCan[uPack_infos[i].node_idx].pack_data[uPack_infos[i].pack_idx].pack_debug1);
			PointInfos[i].slaveIndex=uIndex;
			PointInfos[i].slaveAddr=uPack_infos[i].packSlaveInfo.Info.packSlaveAddr;
			PointInfos[i].sumAddr=PointInfos[i].groupIndex=uPack_infos[i].packSlaveInfo.Info.uGroupIndex;
			PointInfos[i].pointState.state.point_online=1;
			PointInfos[i].pointState.state.update_need=Get_ExtPack_Version(uPack_infos[i].packSoftVersion[0]);
	
			ESP_LOGI(TAG,"pack_Extinfos_summary i == %d, slaveAddr== 0x%x, sumAddr == 0x%x, groupIndex == 0x%x",
				i,PointInfos[i].slaveAddr,PointInfos[i].sumAddr,PointInfos[i].groupIndex);
			if(flag_mix_version == 1)// 混包bit0必定置1
			{
				for(int num=0;num<MAX_PACK_TYPE_NUM;num++)
				{
					Inv_Pack_Slave_WR[num].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 0);//同一电池包类型固件里，前四位料号有差异	
				}
				 //Inv_Pack_Slave_WR[SLAVE_B500K_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 0);//同一电池包类型固件里，前四位料号有差异
				// Inv_Pack_Slave_WR[SLAVE_B300K_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 0);//同一电池包类型固件里，前四位料号有差异
				// Inv_Pack_Slave_WR[SLAVE_B300S_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 0);//同一电池包类型固件里，前四位料号有差异
				// Inv_Pack_Slave_WR[SLAVE_B300_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 0);//同一电池包类型固件里，前四位料号有差异
				Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 0);//同一电池包类型固件里，前四位料号有差异
			}
			if(dev_diff_flag)
			{
				for(int num=0;num<MAX_PACK_TYPE_NUM;num++)
				{
					Inv_Pack_Slave_WR[num].mod_reg06100_Pack_each.pack_ver_diff |= ((1 << 2)|(1 << 0));//混包
				}
				// Inv_Pack_Slave_WR[SLAVE_B500K_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |=((1 << 2)|(1 << 0));//混包
				// Inv_Pack_Slave_WR[SLAVE_B300K_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= ((1 << 2)|(1 << 0));//混包
				// Inv_Pack_Slave_WR[SLAVE_B300S_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= ((1 << 2)|(1 << 0));//混包
				// Inv_Pack_Slave_WR[SLAVE_B300_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= ((1 << 2)|(1 << 0));//混包
				Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.pack_ver_diff |= ((1 << 2)|(1 << 0));//混包
			}	
			if(uB300orB300S_Mix_Flag)
			{
				for(int num=0;num<MAX_PACK_TYPE_NUM;num++)
				{
					Inv_Pack_Slave_WR[num].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1);//单个逆变器接多个B300S或B300
				}
				// Inv_Pack_Slave_WR[SLAVE_B500K_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1);//单个逆变器接多个B300S或B300
				// Inv_Pack_Slave_WR[SLAVE_B300K_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1);//单个逆变器接多个B300S或B300
				// Inv_Pack_Slave_WR[SLAVE_B300S_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1);//单个逆变器接多个B300S或B300
				// Inv_Pack_Slave_WR[SLAVE_B300_GROUP_ADDR].mod_reg06100_Pack_each.pack_ver_diff |= (1 << 1);//单个逆变器接多个B300S或B300
			}			   

			for(j = 0;j < PACK_SOFT_NUM;j++)
			{
				if(uPack_infos[i].packSoftVersion[j] != 0)
				{
					Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.soft[j].type = PACK_DEVICE_BMS;//传递pack固件类型 6174
					Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.soft[j].version = uPack_infos[i].packSoftVersion[j];//传递每个类型下pack固件版本号 6174
					//ESP_LOGI(TAG,"pack_Extinfos_summary 000 --- uindex == %d, i== %d, type == %s, Inv_Pack_Slave_WR_version == %ld",
						//uIndex,i,uPack_infos[i].type_ascii,Inv_Pack_Slave_WR[uIndex].mod_reg06100_Pack_each.soft[j].version);
				}
			}

		}
	}
	//复制电池包寄存器数据
	for(i=0;i<(count+SLAVE_SINGLE_PACK_ADDR);i++)
	{
		memcpy((uint8_t *)&(Inv_Pack_Slave[i].mod_reg06100_Pack_each),(uint8_t *)&(Inv_Pack_Slave_WR[i].mod_reg06100_Pack_each),sizeof(MOD_STRUCT_reg06100));
		memcpy((uint8_t *)&(Inv_Pack_Slave[i].mod_reg06300_Pack_cell[0]),(uint8_t *)&(Inv_Pack_Slave_WR[i].mod_reg06300_Pack_cell[0]),sizeof(Inv_Pack_Slave_WR[i].mod_reg06300_Pack_cell));
		memcpy((uint8_t *)&(Inv_Pack_Slave[i].mod_reg07000_Pack_set),(uint8_t *)&(Inv_Pack_Slave_WR[i].mod_reg07000_Pack_set),sizeof(MOD_STRUCT_reg07000));	
	}
	//复制电池包节点信息
	if (count > reals.packPoinitNum) {
		reals.need_new_version_flag.sBit.pack = 1;
	} else if (count > 0 && count == reals.packPoinitNum) {
		for (i = 0; i < count; i++) {
			uint64_t old_sn = Inv_Pack_Slave[reals.packPointInfos[i].slaveIndex].mod_reg06100_Pack_each.sn_code;
			if (uPack_infos[i].packSn != 0 && old_sn != 0 && uPack_infos[i].packSn != old_sn) {
				reals.need_new_version_flag.sBit.pack = 1;
				ESP_LOGI(TAG, "pack SN changed, trigger PACK version re-check");
				break;
			}
		}
	}
	if(reals.packPoinitNum!=count)
	{
		reals.net_point_Comein=1;
		reals.packPoinitNum=count;
	}
	//ESP_LOGI(TAG,"reals.packPoinitNum:%d",reals.packPoinitNum);
	memcpy(reals.packPointInfos,PointInfos,sizeof(reals.packPointInfos));
	#endif
	//pack_semaphore_Give();

}



#endif

#if 1

#if PACK_DEBUG
#else


void judge_pack_min_version(void)
{
	uint8_t count = 0;//类型号数量 0
	uint8_t flag_mix_version = 0;//相同类型电池包有不同版本混杂标志
	uint8_t flag_B300_or_B300S = 0;//是否出现B300或B300S的电池包
	int8_t ret1 = 0;//版本号索引，第一个版本号前四位相同的位置
	int8_t ret2 = 0;//版本号索引，第一个为0的位置
	Data versions[MAX_DATA_SLAVE] = {0};//存储各个类型号，及每个类型号下在线的电池包数量(考虑到出现所有电池包全是不同类型的情况)
	
	
	// 外置电池包
	for(int i = 0;i < DEV_MAIN_NODE_MAX;i++)
	{
		for(int j = 1;j < PACK_MAX_NUM;j++)//不包含主包			
		{
			char type[TYPE_SIZE];
			uint32_t newVersion;
			
			if(Inv_can[i].pack_data[j].online == 1)
			{				
				strncpy(type, Inv_can[i].pack_data[j].pack_about.type_ascii, TYPE_SIZE);
				newVersion = Inv_can[i].pack_data[j].pack_about.soft[0].version;
			

				// 检查是否已存在此类型号
				int found = 0;
				for (int k = 0; k < count; k++) 
				{
					if (strcmp(versions[k].type_ascii, type) == 0) 
					{	
						// ESP_LOGI(TAG,"K == %d, go to ret1 = findFirstMatchVersion",k);
						versions[k].count_online++;
						ret1 = findFirstMatchVersion(versions[k].version,10,newVersion);//比较的是版本号的前4位是否一样
						if(ret1 == -1)// 没有找到一样的，返回 -1
						{
							// ESP_LOGI(TAG,"Go to ret2, K == %d findFirstMatchVersion",k);							
							ret2 = findFirstMatchVersion(versions[k].version,10,0);
							versions[k].version[ret2] = newVersion;
							versions[k].count_version++;
							flag_mix_version = 1;//相同类型电池包有不同版本混合(如1014.xx和1021.xx)
							// ESP_LOGI(TAG,"No find newType and add newVersion, versions[%d].count_version:%d",k,versions[k].count_version);
						}
						else
						{
							if(versions[k].version[ret1] > newVersion)
							{
								versions[k].version[ret1] = newVersion;
								// ESP_LOGI(TAG,"No find newType and renew MinVersion, newVersion:%ld",newVersion);
							}
							else
							{
								// ESP_LOGI(TAG,"No find newType and no update Version");
							}							
						}
						found = 1;
						break;
					}
				}

				// 如果未找到则添加新条目(类型，如B300K、B300S...)
				if (!found) 
				{
					if (count < MAX_DATA_SLAVE) 
					{
						strcpy(versions[count].type_ascii, type);
						versions[count].version[0] = newVersion;
						versions[count].count_version++;
						versions[count].count_online++;
						// ESP_LOGI(TAG,"Find newType and add newVersion, versions[%d].count_version:%d",count,versions[count].count_version);
						count++;
						if((strcmp(type,"B300S") == 0) || (strcmp(type,"B300") == 0))
						{
							flag_B300_or_B300S = 1;
						}
					}
				}
			}
		}
	}//end for
	
	// ESP_LOGW(TAG,"flag_mix_version:%u, flag_B300_or_B300S:%u, count:%u",flag_mix_version,flag_B300_or_B300S,count);
	// ESP_LOGW(TAG,"Inv_Pack[0].mod_reg06100_Pack_each.pack_ver_diff:%u, Inv_Pack[%d].mod_reg06100_Pack_each.pack_ver_diff:%u",
	// 	Inv_Pack[0].mod_reg06100_Pack_each.pack_ver_diff,DEV_MAIN_NODE_MAX,Inv_Pack[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each.pack_ver_diff);


	//6000段总的 外置电池包
	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.pack_cnt = count;//传递pack类型数量	总的
	if (count > 0 && count <= 8)
	{
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.online = (1 << count) - 1;// 将online的低count位置一
		// ESP_LOGI(TAG,"Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.online = %d",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.online);
	}
	else
	{
		ESP_LOGE(TAG,"count is out of range:%d",count);
	}
	//

}


void single_pack_information(void)
{
	uint8_t count = 0;// 类型号数量 0
	int8_t ret2 = 0;// 版本号索引，第一个为0的位置
	uint8_t count_B300K = 0;// 计数6300段里Inv_Pack的B300K类型的第几个电池包
	uint8_t count_B300S = 0;// 计数6300段里Inv_Pack的B300S类型的第几个电池包
	uint8_t count_B300 = 0;// 计数6300段里Inv_Pack的B300类型的第几个电池包
	uint8_t count_B500K = 0;// 计数6300段里Inv_Pack的B300类型的第几个电池包
	uint8_t count_temp_6300 = 0;// 计数6300段里的临时变量
	Data_Pack versions_pack[MAX_DATA_TYPE] = {0};// 分别存储各个类型下电池包的SN码、故障和保护等信息、单个BMU中电芯数量及NTC数量、类型号，目前假设最多就4种类型 7200

	// 外置电池包
	for(int i = 0;i < DEV_MAIN_NODE_MAX;i++)
	{
		for(int j = 1;j < PACK_MAX_NUM;j++)	//不包含主包
		{
			char type[TYPE_SIZE];
			uint64_t newBMU_SN;
			uint32_t newBMU_Detail;
			uint16_t newSingle_Cell_and_Temp_Count;
			uint8_t newBMU_Type;
			uint32_t newBMU_Version;
			
			if(Inv_can[i].pack_data[j].online == 1)
			{				
				strncpy(type, Inv_can[i].pack_data[j].pack_about.type_ascii, TYPE_SIZE);
				newBMU_Version = Inv_can[i].pack_data[j].pack_about.soft[0].version;
				newBMU_SN = Inv_can[i].pack_data[j].pack_about.sn_code;
				newBMU_Detail = pack_procte_convert(&Inv_can[i].pack_data[0].pack_base);//Inv_can[i].pack_data[j].pack_base.protect_status.all;
				newSingle_Cell_and_Temp_Count = (Inv_can[i].pack_data[j].pack_base.pack_total_ntc << 8) | (Inv_can[i].pack_data[j].pack_base.pack_total_cell & 0xFF);
				// ESP_LOGW(TAG,"newSingle_Cell_and_Temp_Count = %d, Inv_can[%d].pack_data[%d].pack_base.pack_total_ntc=%d, Inv_can[%d].pack_data[%d].pack_base.pack_total_cell=%d",
				// 	newSingle_Cell_and_Temp_Count,i,j,Inv_can[i].pack_data[j].pack_base.pack_total_ntc,i,j,Inv_can[i].pack_data[j].pack_base.pack_total_cell);
				//
				if(strcmp(type,"B300K") == 0)// 在此判断类型，并根据约定赋值 2:B300K  3:B300S  4:B300;  根据类型分类存放6300的电芯、探头信息
				{
					newBMU_Type = 2;
					// versions_pack[x] -> x=0,1,2分别表示B300K、B300S、B300 仅限6300、7200段
					// memcpy((uint8_t *)&versions_pack[0].mod_reg06300_Pack_cell[count_B300K],
					// 		(uint8_t *)&Inv_can[i].pack_data[j].pack_debug1,sizeof(MOD_STRUCT_reg06300));
					count_B300K++;
				}
				else if(strcmp(type,"B300S") == 0)
				{
					newBMU_Type = 3;
					// memcpy((uint8_t *)&versions_pack[1].mod_reg06300_Pack_cell[count_B300S],
					// 		(uint8_t *)&Inv_can[i].pack_data[j].pack_debug1,sizeof(MOD_STRUCT_reg06300));
					count_B300S++;
				}
				else if(strcmp(type,"B300") == 0)
				{
					newBMU_Type = 4;
					// memcpy((uint8_t *)&versions_pack[2].mod_reg06300_Pack_cell[count_B300],
					// 		(uint8_t *)&Inv_can[i].pack_data[j].pack_debug1,sizeof(MOD_STRUCT_reg06300));
					count_B300++;
				}
				else
				{
					ESP_LOGE(TAG,"Type is out of range:%s",type);
					newBMU_Type = 0;
				}

				// 检查是否已存在此类型号
				int found = 0;				
				for (int k = 0; k < count; k++) 
				{
					if (strcmp(versions_pack[k].type_ascii, type) == 0) 
					{												
						ret2 = findFirstMatchVersion(versions_pack[k].BMU_Version,24,0);
						if(ret2 >= 0)
						{
							versions_pack[k].BMU_SN[ret2] = newBMU_SN;
							versions_pack[k].BMU_Detail[ret2] = newBMU_Detail;
							versions_pack[k].Single_Cell_and_Temp_Count[ret2] = newSingle_Cell_and_Temp_Count;
							versions_pack[k].BMU_Type[ret2] = newBMU_Type;
							versions_pack[k].BMU_Version[ret2] = newBMU_Version;
							versions_pack[k].count_online++;
						}
						found = 1;
						break;
					}
				}

				// 如果未找到则添加新条目
				if (!found) 
				{
					if (count < MAX_DATA_TYPE) 
					{
						strcpy(versions_pack[count].type_ascii, type);
						versions_pack[count].BMU_SN[0] = newBMU_SN;
						versions_pack[count].BMU_Detail[0] = newBMU_Detail;
						versions_pack[count].Single_Cell_and_Temp_Count[0] = newSingle_Cell_and_Temp_Count;
						versions_pack[count].BMU_Type[0] = newBMU_Type;
						versions_pack[count].BMU_Version[0] = newBMU_Version;
						versions_pack[count].count_online++;
						count++;
					}
				}
			}
		}
	}//end for


	#if 1
	// 外置电池包 传递给6300段 为了和7200段外置电池包保持一致，且app读取只会下发从机地址和读取个数，需要靠这个i区分app要的是哪一个类型的pack
	for (int i = 0; i < count; i++) 
	{
		count_temp_6300 = 0;
		for(int j = 0;j < DEV_MAIN_NODE_MAX;j++)
		{
			for(int k = 1;k < PACK_MAX_NUM;k++)	//不包含主包
			{
				if(Inv_can[j].pack_data[k].online == 1)
				{
					if (((strcmp(versions_pack[i].type_ascii, "B300K") == 0) && (strcmp(Inv_can[j].pack_data[k].pack_about.type_ascii, "B300K") == 0)))
					{
						memcpy((uint8_t *)&Inv_Pack[i].mod_reg06300_Pack_cell[count_temp_6300],
							(uint8_t *)&Inv_can[j].pack_data[k].pack_debug1,sizeof(MOD_STRUCT_reg06300));
						count_temp_6300++;
					}
					else if (((strcmp(versions_pack[i].type_ascii, "B300S") == 0) && (strcmp(Inv_can[j].pack_data[k].pack_about.type_ascii, "B300S") == 0)))
					{
						memcpy((uint8_t *)&Inv_Pack[i].mod_reg06300_Pack_cell[count_temp_6300],
							(uint8_t *)&Inv_can[j].pack_data[k].pack_debug1,sizeof(MOD_STRUCT_reg06300));
						count_temp_6300++;
					}
					else if (((strcmp(versions_pack[i].type_ascii, "B300") == 0) && (strcmp(Inv_can[j].pack_data[k].pack_about.type_ascii, "B300") == 0)))
					{
						memcpy((uint8_t *)&Inv_Pack[i].mod_reg06300_Pack_cell[count_temp_6300],
							(uint8_t *)&Inv_can[j].pack_data[k].pack_debug1,sizeof(MOD_STRUCT_reg06300));
						count_temp_6300++;
					}
				}
			}
		}
	}//end for
	#endif

	#if 1
	// 外置电池包 传递给7200段
	for (int i = 0; i < count; i++) 
	{
		uint8_t count_cell = 0;// 某个类型电池包总的电芯数量
		uint8_t count_temp = 0;// 某个类型电池包总的探头数量
		if (strcmp(versions_pack[i].type_ascii, "B300K") == 0) 
		{
			for (int j = 0; j < versions_pack[i].count_online; j++) 
			{
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_SN = versions_pack[i].BMU_SN[j];                                            // 传递 BMU SN码
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Detail = versions_pack[i].BMU_Detail[j];                                    // 传递 BMU 故障和保护等信息
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].Single_Cell_and_Temp_Count = versions_pack[i].Single_Cell_and_Temp_Count[j];    // 传递 单个BMU中电芯数量及NTC数量
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Type.bit.BMU_Type1 = versions_pack[i].BMU_Type[j];                          // 传递 BMU类型 等同于pcak类型
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Type.bit.BMU_Type2 = versions_pack[i].BMU_Type[j];                          // 传递 BMU类型 等同于pcak类型
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Version = versions_pack[i].BMU_Version[j];                                  // 传递 BMU 版本号
				count_cell += (versions_pack[i].Single_Cell_and_Temp_Count[j] & 0xFF);
				count_temp += ((versions_pack[i].Single_Cell_and_Temp_Count[j] >> 8) & 0xFF);
			}
			Inv_Pack[i].mod_reg06100_Pack_each.sn_code = versions_pack[i].BMU_SN[0];											 	 // 传递该pack类型下的第一个SN 6107
			Inv_Pack[i].mod_reg06100_Pack_each.pack_total_cell = count_cell;											 			 // 传递该pack类型下总的电芯数量 6152
			Inv_Pack[i].mod_reg06100_Pack_each.pack_total_ntc = count_temp;												 			 // 传递该pack类型下总的探头数量 6153
			Inv_Pack[i].mod_reg06100_Pack_each.PackBMUCnt = versions_pack[i].count_online;											 // 传递该pack类型下的在线电池包数量 6154
			memcpy(Inv_Pack[i].mod_reg06100_Pack_each.type_ascii, "B300K", sizeof("B300K"));										 // 传递该pack类型下的类型号(要先通过6100显示)
		} 
		else if (strcmp(versions_pack[i].type_ascii, "B300S") == 0) 
		{
			for (int j = 0; j < versions_pack[i].count_online; j++) 
			{
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_SN = versions_pack[i].BMU_SN[j];                                            // 传递 BMU SN码
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Detail = versions_pack[i].BMU_Detail[j];                                    // 传递 BMU 故障和保护等信息
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].Single_Cell_and_Temp_Count = versions_pack[i].Single_Cell_and_Temp_Count[j];    // 传递 单个BMU中电芯数量及NTC数量
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Type.bit.BMU_Type1 = versions_pack[i].BMU_Type[j];                          // 传递 BMU类型 等同于pcak类型
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Type.bit.BMU_Type2 = versions_pack[i].BMU_Type[j];                          // 传递 BMU类型 等同于pcak类型
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Version = versions_pack[i].BMU_Version[j];                                  // 传递 BMU 版本号
				count_cell += (versions_pack[i].Single_Cell_and_Temp_Count[j] & 0xFF);
				count_temp += ((versions_pack[i].Single_Cell_and_Temp_Count[j] >> 8) & 0xFF);
			}
			Inv_Pack[i].mod_reg06100_Pack_each.sn_code = versions_pack[i].BMU_SN[0];											 	 // 传递该pack类型下的第一个SN 6107
			Inv_Pack[i].mod_reg06100_Pack_each.pack_total_cell = count_cell;											 			 // 传递该pack类型下总的电芯数量 6152
			Inv_Pack[i].mod_reg06100_Pack_each.pack_total_ntc = count_temp;												 			 // 传递该pack类型下总的探头数量 6153
			Inv_Pack[i].mod_reg06100_Pack_each.PackBMUCnt = versions_pack[i].count_online;											 // 传递该pack类型下的在线电池包数量 6154
			memcpy(Inv_Pack[i].mod_reg06100_Pack_each.type_ascii, "B300S", sizeof("B300S"));										 // 传递该pack类型下的类型号(要先通过6100显示)
		} 
		else if (strcmp(versions_pack[i].type_ascii, "B300") == 0) 
		{
			for (int j = 0; j < versions_pack[i].count_online; j++) 
			{
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_SN = versions_pack[i].BMU_SN[j];                                            // 传递 BMU SN码
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Detail = versions_pack[i].BMU_Detail[j];                                    // 传递 BMU 故障和保护等信息
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].Single_Cell_and_Temp_Count = versions_pack[i].Single_Cell_and_Temp_Count[j];    // 传递 单个BMU中电芯数量及NTC数量
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Type.bit.BMU_Type1 = versions_pack[i].BMU_Type[j];                          // 传递 BMU类型 等同于pcak类型
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Type.bit.BMU_Type2 = versions_pack[i].BMU_Type[j];                          // 传递 BMU类型 等同于pcak类型
				Inv_Pack[i].mod_reg07200_Pack_bmu[j].BMU_Version = versions_pack[i].BMU_Version[j];                                  // 传递 BMU 版本号
				count_cell += (versions_pack[i].Single_Cell_and_Temp_Count[j] & 0xFF);
				count_temp += ((versions_pack[i].Single_Cell_and_Temp_Count[j] >> 8) & 0xFF);
			}
			Inv_Pack[i].mod_reg06100_Pack_each.sn_code = versions_pack[i].BMU_SN[0];											 	 // 传递该pack类型下的第一个SN 6107
			Inv_Pack[i].mod_reg06100_Pack_each.pack_total_cell = count_cell;											 			 // 传递该pack类型下总的电芯数量 6152
			Inv_Pack[i].mod_reg06100_Pack_each.pack_total_ntc = count_temp;												 			 // 传递该pack类型下总的探头数量 6153
			Inv_Pack[i].mod_reg06100_Pack_each.PackBMUCnt = versions_pack[i].count_online;											 // 传递该pack类型下的在线电池包数量 6154
			memcpy(Inv_Pack[i].mod_reg06100_Pack_each.type_ascii, "B300", sizeof("B300"));										 	 // 传递该pack类型下的类型号(要先通过6100显示)
		}
	}
	#endif
	// 内置电池包 传递给6300段
	for(int i = 0;i < DEV_MAIN_NODE_MAX;i++)
	{
		if(Inv_can[i].pack_data[0].online == 1)
		{
			memcpy((uint8_t *)&Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06300_Pack_cell[i],
					(uint8_t *)&Inv_can[i].pack_data[0].pack_debug1,sizeof(MOD_STRUCT_reg06300));
			// printf("Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06300_Pack_cell:\n");
			// esp_log_buffer_hex(TAG,(uint8_t *)&Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06300_Pack_cell[i],sizeof(MOD_STRUCT_reg06300));
		}
		

	}

	#if 1
		// 内置电池包 传递给7200段
		uint8_t count_cell = 0;// 内置电池包总的电芯数量
		uint8_t count_temp = 0;// 内置电池包总的探头数量
		uint8_t count_online = 0;// 内置电池包在线数量
		for(int i = 0;i < DEV_MAIN_NODE_MAX;i++)
		{
			if(Inv_can[i].pack_data[0].online == 1)
			{
				Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_SN = Inv_can[i].pack_data[0].pack_about.sn_code;             // 传递 BMU SN码
				Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_Detail = pack_procte_convert(&Inv_can[i].pack_data[0].pack_base);//Inv_can[i].pack_data[0].pack_base.protect_status.all;   // 传递 BMU 故障和保护等信息
				Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].Single_Cell_and_Temp_Count = \
					(Inv_can[i].pack_data[0].pack_base.pack_total_ntc << 8) | (Inv_can[i].pack_data[0].pack_base.pack_total_cell & 0xFF);    		// 传递 单个BMU中电芯数量及NTC数量 注意pack_total_cell在协议是2字节此处只取低八位
				Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_Type.bit.BMU_Type1 = 5;                                      // 传递 BMU类型 AC380
				Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_Type.bit.BMU_Type2 = 5;                                      // 传递 BMU类型 AC380
				Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_Version = Inv_can[i].pack_data[0].pack_about.soft[0].version;// 传递 BMU 版本号

				count_cell += Inv_can[i].pack_data[0].pack_base.pack_total_cell;
				count_temp += Inv_can[i].pack_data[0].pack_base.pack_total_ntc;
				count_online++;

                memset(Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.type_ascii, 0,
                        sizeof(Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.type_ascii));
                memcpy(Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.type_ascii,
                        SetData.dev_info_t.INV_dev_type,
                        sizeof(Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.type_ascii));		// 动态使用当前设备类型
			}
			Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.pack_total_cell = count_cell;											// 传递该pack类型下总的电芯数量 6152
			Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.pack_total_ntc = count_temp;											// 传递该pack类型下总的探头数量 6153
			Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.PackBMUCnt = count_online;												// 传递该pack类型下的在线电池包数量 内置电池包数量等于在线逆变数量 6154 i+1
			// ESP_LOGW(TAG,"Inner B300K ---> Inv_Pack[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each.pack_total_cell:%d, Inv_Pack[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each.pack_total_ntc:%d",
			// 		Inv_Pack[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each.pack_total_cell,Inv_Pack[DEV_MAIN_NODE_MAX].mod_reg06100_Pack_each.pack_total_ntc);
		}
	#endif
	

	//6000段总的 外置电池包 传递类型数量
	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.pack_cnt = count;//传递pack类型数量	总的
	if (count > 0 && count <= 8)
	{
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.online = (1 << count) - 1;// 将online的低count位置一
		// ESP_LOGI(TAG,"Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.online = %d",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.online);
	}
	else
	{
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06000_Pack_sum.online = 0;
		ESP_LOGE(TAG,"count is out of range:%d",count);
	}
	//



	// for (int i = 0; i < count; i++) //count
	// {
	// 	printf("Out Pack ---single_pack_information current i == %d\n",i);
    //     printf("Type ASCII: %s\n", versions_pack[i].type_ascii);
	// 	for (int j = 0; j < 24; j++)
	// 	{
	// 		if(versions_pack[i].BMU_SN[j] != 0)
	// 		{
	// 			printf("versions_pack[%d].BMU_SN[%d]: %llu\n", i,j,versions_pack[i].BMU_SN[j]);
	// 		}
	// 		if(versions_pack[i].BMU_Detail[j] != 0)
	// 		{
	// 			printf("versions_pack[%d].BMU_Detail[%d]: %lu\n", i,j,versions_pack[i].BMU_Detail[j]);
	// 		}
	// 		if(versions_pack[i].Single_Cell_and_Temp_Count[j] != 0)
	// 		{
	// 			printf("versions_pack[%d].Single_Cell_and_Temp_Count[%d]: %u\n", i,j,versions_pack[i].Single_Cell_and_Temp_Count[j]);
	// 		}
	// 		if(versions_pack[i].BMU_Type[j] != 0)
	// 		{
	// 			printf("versions_pack[%d].BMU_Type[%d]: %u\n", i,j,versions_pack[i].BMU_Type[j]);
	// 		}
	// 		if(versions_pack[i].BMU_Version[j] != 0)
	// 		{
	// 			printf("versions_pack[%d].BMU_Version[%d]: %lu\n", i,j,versions_pack[i].BMU_Version[j]);
	// 		}
	// 	}        
    //     printf("Count:%d, number of versions_pack count_online: %u\n", i,versions_pack[i].count_online);//每个类型号下面在线的pack数量
    //     printf("----------------------------------\n\n");
    // }

	// for(int i = 0;i < DEV_MAIN_NODE_MAX;i++)
	// {
	// 	if(Inv_can[i].pack_data[0].online == 1)
	// 	{
	// 		printf("Inner Pack ---single_pack_information current i == %d\n",i);
	// 		printf("Type ASCII: %s\n", Inv_can[i].pack_data[0].pack_about.type_ascii);
	// 		printf("BMU_SN: %llu\n", Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_SN);
	// 		printf("BMU_Detail: %lu\n", Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_Detail);
	// 		printf("Single_Cell_and_Temp_Count: %u\n", Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].Single_Cell_and_Temp_Count);
	// 		printf("BMU_Type: %u\n", Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_Type.bit.BMU_Type1);
	// 		printf("BMU_Version: %lu\n", Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg07200_Pack_bmu[i].BMU_Version);
	// 		printf("PackBMUCnt: %u\n", Inv_Pack[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg06100_Pack_each.PackBMUCnt);
	// 		printf("----------------------------------\n\n");
	// 	}
	// }

}

// num是要读取的电池包个数 此处的Inv_Pack已是每个类型汇总后的数据
void Pack_6300_offset(uint8_t *str, uint8_t num, uint8_t seq)
{
	uint16_t count_TotalPackCellCnt = 0;
	uint16_t count_TotalPackNTCCnt = 0;
	uint16_t Size_PackCellCnt = sizeof(Inv_Pack[0].mod_reg06300_Pack_cell[0].PackCellCnt);
	uint16_t Size_PackNTCCnt = sizeof(Inv_Pack[0].mod_reg06300_Pack_cell[0].PackNTCCnt);
	uint16_t Size_CellVoltage = sizeof(Inv_Pack[0].mod_reg06300_Pack_cell[0].CellVoltage);
	uint16_t Size_NTCGroup = sizeof(Inv_Pack[0].mod_reg06300_Pack_cell[0].NTCGroup);

	ESP_LOGW(TAG,"num == %d, seq == %d",num,seq);

	for(uint8_t i = 0;i < Inv_Pack[seq].mod_reg06100_Pack_each.PackBMUCnt;i++)
	{
		count_TotalPackCellCnt += Inv_Pack[seq].mod_reg06300_Pack_cell[i].PackCellCnt;
		count_TotalPackNTCCnt += Inv_Pack[seq].mod_reg06300_Pack_cell[i].PackNTCCnt;
	}

	memset(str, 0, 360);
	str[0] = count_TotalPackCellCnt & 0xFF; // 取低八位
	str[1] = (count_TotalPackCellCnt >> 8) & 0xFF; // 取高八位
	str[2] = count_TotalPackNTCCnt & 0xFF; // 取低八位
	str[3] = (count_TotalPackNTCCnt >> 8) & 0xFF; // 取高八位

	for(uint8_t i = 0;i < num;i++)//Inv_Pack[seq].mod_reg06100_Pack_each.PackBMUCnt
	{
		memcpy(&str[4 + i*Size_CellVoltage], 
				(uint8_t *)&Inv_Pack[seq].mod_reg06300_Pack_cell[i].CellVoltage, Size_CellVoltage);
		memcpy(&str[4 + num*Size_CellVoltage + i*(Size_NTCGroup/2)], 
				(uint8_t *)&Inv_Pack[seq].mod_reg06300_Pack_cell[i].NTCGroup, Size_NTCGroup);

	}
	// printf("str:\n");
	// esp_log_buffer_hex(TAG, str, 100);
}

// num是要读取的电池包个数
void Pack_7200_offset(uint8_t *str, uint8_t num, uint8_t seq)
{
	uint16_t Size_BMU_SN = sizeof(Inv_Pack[0].mod_reg07200_Pack_bmu[0].BMU_SN);
	uint16_t Size_BMU_Detail = sizeof(Inv_Pack[0].mod_reg07200_Pack_bmu[0].BMU_Detail);
	uint16_t Size_Single_Cell_and_Temp_Count = sizeof(Inv_Pack[0].mod_reg07200_Pack_bmu[0].Single_Cell_and_Temp_Count);
	uint16_t Size_BMU_Type = sizeof(Inv_Pack[0].mod_reg07200_Pack_bmu[0].BMU_Type);
	uint16_t Size_BMU_Version = sizeof(Inv_Pack[0].mod_reg07200_Pack_bmu[0].BMU_Version);

	ESP_LOGW(TAG,"7200 num == %d, seq == %d",num,seq);

	memset(str, 0, 360);
	for(uint8_t i = 0;i < num;i++)
	{
		memcpy(&str[i*Size_BMU_SN], 
				(uint8_t *)&Inv_Pack[seq].mod_reg07200_Pack_bmu[i].BMU_SN, Size_BMU_SN);

		memcpy(&str[num*Size_BMU_SN + i*Size_BMU_Detail], 
				(uint8_t *)&Inv_Pack[seq].mod_reg07200_Pack_bmu[i].BMU_Detail, Size_BMU_Detail);

		memcpy(&str[num*(Size_BMU_SN + Size_BMU_Detail) + i*Size_Single_Cell_and_Temp_Count], 
				(uint8_t *)&Inv_Pack[seq].mod_reg07200_Pack_bmu[i].Single_Cell_and_Temp_Count, Size_Single_Cell_and_Temp_Count);

		if(i%2)
		{
			uint8_t temp_type_H = Inv_Pack[seq].mod_reg07200_Pack_bmu[i].BMU_Type.bit.BMU_Type1;
			// ESP_LOGW(TAG,"temp_type_H == %d",temp_type_H);
			memcpy(&str[num*(Size_BMU_SN + Size_BMU_Detail + Size_Single_Cell_and_Temp_Count) + i], 
				(uint8_t *)&temp_type_H, 1);
		}
		else
		{
			uint8_t temp_type_L = Inv_Pack[seq].mod_reg07200_Pack_bmu[i].BMU_Type.bit.BMU_Type2;
			// ESP_LOGW(TAG,"temp_type_L == %d",temp_type_L);
			memcpy(&str[num*(Size_BMU_SN + Size_BMU_Detail + Size_Single_Cell_and_Temp_Count) + i], 
				(uint8_t *)&temp_type_L, 1);
		}

		if(num%2 == 0)
		{
			ESP_LOGI(TAG,"000----- num/2 == %d",num/2);
			memcpy(&str[num*(Size_BMU_SN + Size_BMU_Detail + Size_Single_Cell_and_Temp_Count) + (num/2)*Size_BMU_Type + i*Size_BMU_Version], 
				(uint8_t *)&Inv_Pack[seq].mod_reg07200_Pack_bmu[i].BMU_Version, Size_BMU_Version);
		}
		else
		{
			ESP_LOGI(TAG,"111----- num/2 == %d",num/2);
			memcpy(&str[num*(Size_BMU_SN + Size_BMU_Detail + Size_Single_Cell_and_Temp_Count) + ((num/2)+1)*Size_BMU_Type + i*Size_BMU_Version], 
				(uint8_t *)&Inv_Pack[seq].mod_reg07200_Pack_bmu[i].BMU_Version, Size_BMU_Version);
		}
		
	}
}

int findFirstMatchVersion(const uint32_t strings[], size_t count, const uint32_t target) 
{
	//ESP_LOGI(TAG,"strings[0] == %ld ,target:%ld",strings[0],target);//当前类型下的各版本号
    for (uint8_t i = 0; i < count; i++) 
	{
		//ESP_LOGI(TAG,"strings[%d] == %ld",i,strings[i]);//当前类型下的各版本号
		//ESP_LOGW(TAG,"strings[%d]/100 == %ld, target/100 == %ld",i,strings[i]/100,target/100);//当前类型下的各版本号
        if ((strings[i]/100) == (target/100)) 
		{
			//ESP_LOGW(TAG,"Find Eq strings[%d] == %ld",i,strings[i]);//当前类型下的各版本号
            return (int)i; // 返回匹配的索引
        }
    }
    return -1; // 如果没有找到匹配项，则返回 -1
}
#endif

#endif