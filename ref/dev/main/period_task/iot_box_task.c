#include <stdint.h>
#include <string.h>
#include"comm_define.h"
#include"iot_box_task.h"
#include"iot_period_task.h"
#include "can_protocol.h"
#include "dev_discovery.h"
#include "can_pack.h"
#include "esp_littlefs.h"
#include "can_init.h"

#define TAG "[IOT_BOX_TASK]"

#ifdef INV_LOG_DETAILED_INFO_RECORD

// 逆变器log信息记录
QueueHandle_t xQueue_inv_Detailed_Info_record = NULL;//逆变器 log信息记录 

EXT_RAM_BSS_ATTR Inv_Detailed_Info_Datas inv_Detail_Info_Data[DEV_MAIN_NODE_MAX+1];	//逆变器详细数据记录

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}
 

/*------------------------------------------------------------------------------
 Function: SaveErrorDetailInfo
 -----------------------------------------------------------------------------*/
/**
  * @brief    将触发的故障或报警转换为对应的报警码并进行逆变log信息的记录存储
  * @param[in]  uint8_t HappenType   0：故障\报警消失  1：故障\报警发生
  * uint8_t errorGroup				 故障\报警组类型（故障：0~5  报警：6~9）
  * uint16_t FaultInformation        故障\报警信息
  * uint8_t node_id	                 设备序号（0~2：单机序号 3：汇总序号）
  * @param[out] 
  * @return  void   
  */
void SaveErrorDetailInfo(uint8_t happenType,uint8_t errorGroup,uint16_t faultInformation, uint8_t node_id)
{
	time_t now_date = time(NULL);

	ESP_LOGI(TAG,"now_date:%llu,errorGroup:%d,faultInformation:0x%x",now_date,errorGroup,faultInformation);
	uint16_t alarmCode=0;
	if(errorGroup<6)
	{
		for(uint8_t i=0;i<16;i++)
		{
			if(faultInformation&(1<<i))
			{
				alarmCode=i+errorGroup*16+INV_FAULTCODE_BASE;
				ESP_LOGI(TAG,"have faultcode:%u i:%d",alarmCode,i);
				Inv_Info_Update_OnErr(alarmCode,happenType,node_id,now_date);
			}
		}
	}
	else{
		for(uint8_t i=0;i<16;i++)
		{
			if(faultInformation&(1<<i))
			{
				alarmCode=i+(errorGroup-6)*16+INV_ALARMCODE_BASE;
				ESP_LOGI(TAG,"have alarmcode:%u i:%d",alarmCode,i);
				Inv_Info_Update_OnErr(alarmCode,happenType,node_id,now_date);
			}
		}
	}	
}

/*------------------------------------------------------------------------------
 Function: SaveEventInfo
 -----------------------------------------------------------------------------*/
/**
  * @brief    接收触发的事件码并进行逆变log信息的记录存储
  * @param[in]  uint8_t HappenType   0：事件消失  1：事件发生
  * uint16_t code				 事件码
  * uint8_t node_id	                 设备序号（1~3：单机序号 4：汇总序号）
  * @param[out] 
  * @return  void   
  */
void SaveEventInfo(uint8_t happenType,uint16_t code,uint8_t node_id)
{
	time_t now_date = time(NULL);

	ESP_LOGI(TAG,"now_date:%llu,happenType:%d,code:%u,node_id:%d ",now_date,happenType,code,node_id);
	Inv_Info_Update_OnErr(code,happenType,node_id,now_date);
}


/*------------------------------------------------------------------------------
 Function: Inv_Detailed_Info_Log_Pop
 -----------------------------------------------------------------------------*/
/**
  * @brief    逆变器详细log信息记录写入到文件系统
  * @param[in]  void
  * @param[out] 
  * @return  uint8_t  0:成功  非0（bit0=1：写入单机文件失败 bit1=1：写入汇总文件失败） 
  */
uint8_t Inv_Detailed_Info_Log_Pop(void)
{
	Log_Inv_Detailed_Info_Queue_Struct queue_msg ={NULL};
	char fname[30]={0};
	uint32_t file_bias=0;
	uint8_t rtn=0;
	if(xQueue_inv_Detailed_Info_record&& xQueueReceive(xQueue_inv_Detailed_Info_record,&queue_msg,3)==pdTRUE)
	{
		ESP_LOGI(TAG,"Inv_Log_History_Pop queue_msg.inv_dev_num:%d",queue_msg.inv_dev_num);

		ESP_LOGI(TAG," Inv_Detailed_Info_Log_Pop inv_Detail_Info_Data:");
		ESP_LOG_BUFFER_HEX_LEVEL(TAG, queue_msg.pdata, sizeof(Inv_Detailed_Info_Datas), ESP_LOG_WARN);

		uint8_t index=queue_msg.inv_dev_num;	//单个逆变器详细信息

		if(index==0)
		{
			goto _summary;
		}
		LOG_FILE_PATH_INV_INFO(fname,index,1);

		file_bias=(uint16_t)SetData.dev_info_t.invDetailedInfo_AddrIndex[index];

		if(0==historic_data_write(fname,(uint8_t*)queue_msg.pdata,file_bias,sizeof(Inv_Detailed_Info_Datas)))
		{
			dump_buf("historic_data_write：：", (uint8_t *)queue_msg.pdata, sizeof(Inv_Detailed_Info_Datas));

			ESP_LOGI(TAG, "File written ok (%s)",fname);

			SetData.dev_info_t.invDetailedInfo_AddrIndex[index]++;
			if(SetData.dev_info_t.invDetailedInfo_AddrIndex[index] >= INV_DETAILED_INFO_MAX_COUNT)
			{
				SetData.dev_info_t.invDetailedInfo_AddrIndex[index]=0;
			}
		
			SetData.dev_info_t.invDetailedInfo_SaveCount[index]++;
			if(SetData.dev_info_t.invDetailedInfo_SaveCount[index] > INV_DETAILED_INFO_MAX_COUNT )	 //100		 
			{
				SetData.dev_info_t.invDetailedInfo_SaveCount[index] = INV_DETAILED_INFO_MAX_COUNT;
			}
			ESP_LOGI(TAG,"Inv_Detailed_Info_Log_Pop index:%d invDetailedInfo_AddrIndex:%lu,invDetailedInfo_SaveCount:%lu",index,SetData.dev_info_t.invDetailedInfo_AddrIndex[index],SetData.dev_info_t.invDetailedInfo_SaveCount[index]);
			reals.SetDataWrFlag.sBit.SetDataUpdate_invInfo_cnt =1;	
		}
		else
		{
			ESP_LOGE(TAG, "File written fail:Logger_Pop_Event (%s)",fname );
			rtn|=0x1;
		}

_summary:
		LOG_FILE_PATH_INV_INFO(fname,0,1);

		file_bias=(uint16_t)SetData.dev_info_t.invDetailedInfo_AddrIndex[0];

		if(0==historic_data_write(fname,(uint8_t*)queue_msg.pdata,file_bias,sizeof(Inv_Detailed_Info_Datas)))
		{
			dump_buf("historic_data_write：：", (uint8_t *)queue_msg.pdata, sizeof(Inv_Detailed_Info_Datas));

			ESP_LOGI(TAG, "File written ok (%s)",fname);

			SetData.dev_info_t.invDetailedInfo_AddrIndex[0]++;
			if(SetData.dev_info_t.invDetailedInfo_AddrIndex[0] >= INV_DETAILED_INFO_MAX_COUNT)
			{
				SetData.dev_info_t.invDetailedInfo_AddrIndex[0]=0;
			}
		
			SetData.dev_info_t.invDetailedInfo_SaveCount[0]++;
			if(SetData.dev_info_t.invDetailedInfo_SaveCount[0] > INV_DETAILED_INFO_MAX_COUNT )	 //100		 
			{
				SetData.dev_info_t.invDetailedInfo_SaveCount[0] = INV_DETAILED_INFO_MAX_COUNT;
			}
		
			reals.SetDataWrFlag.sBit.SetDataUpdate_invInfo_cnt =1;	
			ESP_LOGI(TAG,"0 invDetailedInfo_AddrIndex:%lu,invDetailedInfo_SaveCount:%lu",SetData.dev_info_t.invDetailedInfo_AddrIndex[0],SetData.dev_info_t.invDetailedInfo_SaveCount[0]);
		}
		else
		{
			ESP_LOGE(TAG, "File written fail:Logger_Pop_Event (%s)",fname );
			rtn|=0x2;
		}
		
		if (queue_msg.pdata) 
		{
			free(queue_msg.pdata); /* 处理完成释放CAN数据帧 */
			queue_msg.pdata = NULL; 
	
		}
	}else{
		reals.flasWrFlag.sBit.log_invdetailedinfo =0;
	}

	return rtn;
}

/*------------------------------------------------------------------------------
Function: clear_DetailInfo_File
-----------------------------------------------------------------------------*/
/**
* @brief 清除已存储的逆变器详细log信息   
* @param[in]  void	
* @param[out]  
* @return	  0:清除成功
*/
int clear_DetailInfo_File(void)
{
	char path[50] = {0};
	char path2[30] = {0};
	int fd;

  /////////////////////log_history
//   LOG_FILE_PATH_INV(fname);
  
	for(uint8_t i = 0 ; i <= DEV_MAIN_NODE_MAX ; i++)
	{
		LOG_FILE_PATH_INV_INFO(path2,i,1);
		sprintf(path, "%s/%s", RECORD_ROOT_PATH, path2);
		ESP_LOGE(TAG, "clear_DetailInfo file name: %s", path);

		/* 只读格式打开文件 */
		fd = open(path, O_RDONLY);
		if(fd < 0)
		{
			ESP_LOGE(TAG, "[detailed_file] open error, %s", path);
			continue;
		}

		close(fd);
		if(remove(path) == 0) {
        	ESP_LOGI(TAG, "Removed %s successfully", path);
    	}else if( errno != ENOENT){
			ESP_LOGE(TAG, "Remove failed: %s", strerror(errno));
		} 
		SetData.dev_info_t.invDetailedInfo_AddrIndex[i]=0;
		SetData.dev_info_t.invDetailedInfo_SaveCount[i]=0;
		reals.SetDataWrFlag.sBit.SetDataUpdate_invInfo_cnt =1;	
		ESP_LOGI(TAG, "clear %s detailinfo ok", path);
	}
	return 0;
}

/*------------------------------------------------------------------------------
 Function: GetInvDetailInfo
 -----------------------------------------------------------------------------*/
/**
  * @brief      打印输出 inv逆变详细Log信息 调试用
  * @param[in]  void* udata
  *   			uint8_t index
  * 			uint16_t file_pos
  * @param[out] 
  * @return     int
  */
int GetInvDetailInfo(void* udata,uint8_t index,uint16_t file_pos)
{
	char path[30] = {0};
	char path2[19] = {0};
	int fd;
	uint16_t read_size=0;//读取长度
	
	//memset(&g_self_data.mod_reg03000_Inv_history, 0, sizeof(MOD_STRUCT_reg03000));
	

//     INV_FILE_PATH(path,1,1);
	//sprintf(path, "%s/%s%s%d_%02d", RECORD_ROOT_PATH, LOG_RECORD_FOLDER_LEVEL1, INV_RECORD_FILE_MARK, 15, 0);
	LOG_FILE_PATH_INV_INFO(path2,index,1);
	sprintf(path, "%s/%s", RECORD_ROOT_PATH, path2);
	ESP_LOGI(TAG,"GetInvDetailInfo %s",path);
	/* 只读格式打开文件 */
	fd = open(path, O_RDONLY);
	if(fd < 0)
	{
		ESP_LOGE(TAG, "[GetInvDetailInfo] open error(%s)",path);
		return 0;
	}

	if(index>3)
	{
		ESP_LOGE(TAG, "[GetInvDetailInfo] index error(%d)",index);
		return 0;
	}
	read_size=sizeof(Inv_Detailed_Info_Datas);

	// 移动到文件末尾
	off_t fileSize = lseek(fd, 0, SEEK_SET); // 获取文件大小
	ESP_LOGE(TAG, "index:0,invDetailedInfo_AddrIndex:%lu,invDetailedInfo_SaveCount:%lu", SetData.dev_info_t.invDetailedInfo_AddrIndex[0],SetData.dev_info_t.invDetailedInfo_SaveCount[0]);

	ESP_LOGE(TAG, "index:%d,invDetailedInfo_AddrIndex:%lu,invDetailedInfo_SaveCount:%lu",index, SetData.dev_info_t.invDetailedInfo_AddrIndex[index],SetData.dev_info_t.invDetailedInfo_SaveCount[index]);


	ESP_LOGE(TAG, "index is : %d", index);

	ESP_LOGW(TAG, "1 file_pos:%u,read_size:%u",file_pos,read_size);
	if(lseek(fd, file_pos, SEEK_SET) != file_pos) 
	{
		ESP_LOGW(TAG, "1 lseek");  
		goto end;
	}	 
	if(read(fd,udata, read_size) != read_size) 
	{
		ESP_LOGE(TAG, "[GetInvDetailInfo] read error(%s)",path);
		goto end;
	}
	Inv_Detailed_Info_Datas* info_Data=udata;
	ESP_LOGI(TAG," GetInvDetailInfo pre_DetailedInfo:");
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, &info_Data->pre_DetailedInfo, sizeof(info_Data->pre_DetailedInfo), ESP_LOG_WARN);
	ESP_LOGI(TAG," GetInvDetailInfo cur_DetailedInfo:");
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, &info_Data->cur_DetailedInfo, sizeof(info_Data->cur_DetailedInfo), ESP_LOG_WARN);
//////////

    end:

    close(fd);
     
    return 0xff;

}


/*------------------------------------------------------------------------------
 Function: Inv_Detailed_Log_Push
 -----------------------------------------------------------------------------*/
/**
  * @brief    逆变器详细log信息记录存储到队列缓存
  * @param[in]  uint8_t node_id 信息存储的文件序号 00：汇总 01~03：单机设备序号
  * 			void* udata 数据指针
  * @param[out] 
  * @return  uint8_t   0:成功 1：申请内存失败 2：发送队列失败 
  */
uint8_t Inv_Detailed_Log_Push(uint8_t node_id,void* udata)
{
	uint8_t rtn=0;
	Log_Inv_Detailed_Info_Queue_Struct queue_msg =  {NULL};
	ESP_LOGI(TAG,"Inv_Detailed_Log_Push node_id:%d",node_id);
	queue_msg.pdata = (Inv_Detailed_Info_Datas *)heap_caps_malloc(sizeof(Inv_Detailed_Info_Datas), MALLOC_CAP_SPIRAM); // malloc MALLOC_CAP_SPIRAM
	if (!queue_msg.pdata) 
	{
		ESP_LOGE(TAG, "xQueue_Log_record message and malloc failed");
		rtn=1;
	} 
	else 
	{
		// ESP_LOGI(TAG, "xCanBusQueue_Recv  ----------222");//testwx
		ESP_LOGI(TAG," Inv_Detailed_Log_Push inv_Detail_Info_Data:");
		ESP_LOG_BUFFER_HEX_LEVEL(TAG, udata, sizeof(Inv_Detailed_Info_Datas), ESP_LOG_WARN);
		memcpy(queue_msg.pdata, (Inv_Detailed_Info_Datas *) udata, sizeof(Inv_Detailed_Info_Datas));
		queue_msg.inv_dev_num = node_id;
		// 消息保存到队列
		if (xQueueSendToBack((QueueHandle_t)xQueue_inv_Detailed_Info_record, &queue_msg, pdMS_TO_TICKS(100)) != pdPASS) 
		{
			ESP_LOGE(TAG, "Inv_Fault_Log_Push message push queue failed");
			free(queue_msg.pdata);
			queue_msg.pdata = NULL;
			rtn=2;
		}
	}
	return rtn;
}

/*------------------------------------------------------------------------------
 Function: Inv_Detailed_Info_Refresh
 -----------------------------------------------------------------------------*/
/**
  * @brief    基于Inv_Can数据结构更新inv逆变详细Log信息
  * @param[in]  Inv_Detailed_Info_Datas*  detailedData 逆变log信息结构
  * 			MOD_STRUCT_Inv_READ* invCan  can设备 INV+BAT beta总变量结构
  * @param[out] 
  * @return    void 
  */
void Inv_Detailed_Info_Refresh(Inv_Detailed_Info_Datas* detailedData,MOD_STRUCT_Inv_READ* invCan)
{
	detailedData->cur_DetailedInfo.ver=1;
	detailedData->cur_DetailedInfo.alarmCode=0;

	detailedData->cur_DetailedInfo.invWorkState=invCan->inv_data[0].inv_base.inv_work_state;
	detailedData->cur_DetailedInfo.setCtrlWorkMode=invCan->inv_data[0].inv_set00.work_mode;
	detailedData->cur_DetailedInfo.gridFreq=invCan->inv_data[0].inv_grid.freq;

	detailedData->cur_DetailedInfo.grid1Voltage=invCan->inv_data[0].inv_grid.grid_detail[0].input_voltage;
	//detailedData->cur_DetailedInfo.grid1Current=invCan->inv_data[0].inv_grid.grid_detail[0].input_current;
	detailedData->cur_DetailedInfo.grid2Voltage=invCan->inv_data[0].inv_grid.grid_detail[1].input_voltage;
	//detailedData->cur_DetailedInfo.grid2Current=invCan->inv_data[0].inv_grid.grid_detail[1].input_current;
	ESP_LOGI(TAG,"Inv_Detailed_Info_Refresh input_current:%u",invCan->inv_data[0].inv_grid.grid_detail[0].input_current);

	detailedData->cur_DetailedInfo.inv1Voltage=invCan->inv_data[0].inv_data.inv_detail[0].voltage;
	//detailedData->cur_DetailedInfo.inv1Current=invCan->inv_data[0].inv_data.inv_detail[0].current;
	detailedData->cur_DetailedInfo.inv2Voltage=invCan->inv_data[0].inv_data.inv_detail[1].voltage;
	//detailedData->cur_DetailedInfo.inv2Current=invCan->inv_data[0].inv_data.inv_detail[1].current;
	ESP_LOGI(TAG,"Inv_Detailed_Info_Refresh power:%+hd",invCan->inv_data[0].inv_data.inv_detail[0].power);
	ESP_LOGI(TAG,"Inv_Detailed_Info_Refresh current:%u",invCan->inv_data[0].inv_data.inv_detail[0].current);

	if(invCan->inv_data[0].inv_data.inv_detail[0].power>0)
	{
		detailedData->cur_DetailedInfo.inv1Current=-(int16_t)invCan->inv_data[0].inv_data.inv_detail[0].current;
		detailedData->cur_DetailedInfo.inv2Current=-(int16_t)invCan->inv_data[0].inv_data.inv_detail[1].current;
		detailedData->cur_DetailedInfo.grid1Current=-(int16_t)invCan->inv_data[0].inv_grid.grid_detail[0].input_current;
		detailedData->cur_DetailedInfo.grid2Current=-(int16_t)invCan->inv_data[0].inv_grid.grid_detail[1].input_current;
	}else if(invCan->inv_data[0].inv_data.inv_detail[0].power<0){
		detailedData->cur_DetailedInfo.inv1Current=invCan->inv_data[0].inv_data.inv_detail[0].current;
		detailedData->cur_DetailedInfo.inv2Current=invCan->inv_data[0].inv_data.inv_detail[1].current;
		detailedData->cur_DetailedInfo.grid1Current=invCan->inv_data[0].inv_grid.grid_detail[0].input_current;
		detailedData->cur_DetailedInfo.grid2Current=invCan->inv_data[0].inv_grid.grid_detail[1].input_current;
	}else{
		detailedData->cur_DetailedInfo.inv1Current=0;
		detailedData->cur_DetailedInfo.inv2Current=0;
		detailedData->cur_DetailedInfo.grid1Current=0;
		detailedData->cur_DetailedInfo.grid2Current=0;
	}

	ESP_LOGI(TAG,"inv1Current:%+hd,inv2Current:%+hd, grid1Current:%+hd,grid2Current:%+hd",detailedData->cur_DetailedInfo.inv1Current,detailedData->cur_DetailedInfo.inv2Current,detailedData->cur_DetailedInfo.grid1Current,detailedData->cur_DetailedInfo.grid2Current);

	detailedData->cur_DetailedInfo.acLoad1Voltage=invCan->inv_data[0].inv_load.ac_load[0].load_voltage;
	detailedData->cur_DetailedInfo.acLoad1Current=invCan->inv_data[0].inv_load.ac_load[0].load_current;
	detailedData->cur_DetailedInfo.acLoad2Voltage=invCan->inv_data[0].inv_load.ac_load[1].load_voltage;
	detailedData->cur_DetailedInfo.acLoad2Current=invCan->inv_data[0].inv_load.ac_load[1].load_current;

	detailedData->cur_DetailedInfo.pv1Voltage=invCan->inv_data[0].inv_pv.pv_detail[0].input_voltage;
	detailedData->cur_DetailedInfo.pv1Current=invCan->inv_data[0].inv_pv.pv_detail[0].input_current;
	detailedData->cur_DetailedInfo.pv2Voltage=invCan->inv_data[0].inv_pv.pv_detail[1].input_voltage;
	detailedData->cur_DetailedInfo.pv2Current=invCan->inv_data[0].inv_pv.pv_detail[1].input_current;

	detailedData->cur_DetailedInfo.ambientTemp=invCan->inv_data[0].inv_data.AmbientTemp;
	detailedData->cur_DetailedInfo.invMaxTemp=invCan->inv_data[0].inv_data.InvMaxTemp;
	detailedData->cur_DetailedInfo.pvDcdcMaxTemp=invCan->inv_data[0].inv_data.PvDcdcMaxTemp;
	detailedData->cur_DetailedInfo.packTotalVoltage=invCan->pack_data[0].pack_announce.total_voltage;
	ESP_LOGI(TAG,"chg_status:%d",invCan->pack_data[0].pack_announce.chg_status);
	if(invCan->pack_data[0].pack_announce.chg_status==1)
	{detailedData->cur_DetailedInfo.packTotalCurrent=(int16_t)invCan->pack_data[0].pack_announce.total_current;}
	else if(invCan->pack_data[0].pack_announce.chg_status==2)
	{detailedData->cur_DetailedInfo.packTotalCurrent=-(int16_t)invCan->pack_data[0].pack_announce.total_current;}	
	else
	{detailedData->cur_DetailedInfo.packTotalCurrent=0;}
	ESP_LOGI(TAG,"packTotalCurrent:%+hd",detailedData->cur_DetailedInfo.packTotalCurrent);			
	detailedData->cur_DetailedInfo.soc=invCan->pack_data[0].pack_announce.soc;
	detailedData->cur_DetailedInfo.soh=invCan->pack_data[0].pack_announce.soh;	
	detailedData->cur_DetailedInfo.packRunStatus=invCan->pack_data[0].pack_announce.work_status;		
	detailedData->cur_DetailedInfo.cellMinVoltage=invCan->pack_data[0].pack_announce.cell_min_volt;		
	detailedData->cur_DetailedInfo.cellMaxVoltage=invCan->pack_data[0].pack_announce.cell_max_volt;		
	detailedData->cur_DetailedInfo.cellMinTemp=invCan->pack_data[0].pack_base.min_temp_value;		
	detailedData->cur_DetailedInfo.cellMaxTemp=invCan->pack_data[0].pack_base.max_temp_value;		
	detailedData->cur_DetailedInfo.packCycle=invCan->pack_data[0].pack_extend.cycle_count;
	ESP_LOGI(TAG,"cycle_count:%u",invCan->pack_data[0].pack_extend.cycle_count);
	for(int j=0;j<5;j++)
	{
		memcpy((uint8_t*)&detailedData->cur_DetailedInfo.register_transmission_data[j*3],
		(uint8_t*)&(invCan->inv_data[0].inv_detailedinfo.inv_transer_buf[j].transer_struct.buf1_L),6);			
	}
}
/*------------------------------------------------------------------------------
 Function: Inv_Detailed_Info_update
 -----------------------------------------------------------------------------*/
/**
  * @brief    inv逆变详细Log信息实时刷新，刷新间隔：1s
  * @param[in]  void  
  * @param[out] 
  * @return    void 
  */
void Inv_Detailed_Info_update(void)
{
	static time_t recored_Time=0;
	uint8_t change_flag=0;
	uint8_t i=0;
	time_t now_date = time(NULL);
	//ESP_LOGI(TAG,"Inv_Detailed_Info_update now_date1:%llu",now_date);
		/* 日期转换为时间结构 */
	struct tm tm_now;
	struct tm tm_now1;
//	static uint16_t scnt=0;
	memcpy(&tm_now, localtime(&now_date), sizeof(struct tm));
	memcpy(&tm_now1, localtime(&recored_Time), sizeof(struct tm));
	
	ESP_LOGI(TAG, "Inv_Detailed_Info_update pre ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now1.tm_mon+1,tm_now1.tm_mday,tm_now1.tm_hour,tm_now1.tm_min,tm_now1.tm_sec);
	ESP_LOGI(TAG, "Inv_Detailed_Info_update cur ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now.tm_mon+1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);

	if(!recored_Time)
	{
		recored_Time=now_date;
		change_flag=1;
	}
	if((now_date>recored_Time)&&(tm_now.tm_sec!=tm_now1.tm_sec)){
		recored_Time=now_date;
		change_flag=2;
	}

	if(change_flag)
	{
		for(i = 0;i < DEV_MAIN_NODE_MAX;i++)
		{
			if((Inv_can[i].inv_data[0].online == 1)&&(Inv_can[i].inv_data[0].inv_about.dev_sn))
			{
				ESP_LOGI(TAG,"change_flag:%d",change_flag);
				if(change_flag==2)
				{
					memcpy((uint8_t*)&inv_Detail_Info_Data[i+1].pre_DetailedInfo,(uint8_t*)&inv_Detail_Info_Data[i+1].cur_DetailedInfo,sizeof(Inv_Detailed_Info));
				}
				inv_Detail_Info_Data[i+1].cur_DetailedInfo.curTime=recored_Time;
				inv_Detail_Info_Data[i+1].cur_DetailedInfo.device_seq.bit.inv_parallel_seq=i;
				inv_Detail_Info_Data[i+1].cur_DetailedInfo.device_seq.bit.pack_parallel_seq=((i<<8)+SLAVE_PACK_READ_ADDR);
				Inv_Detailed_Info_Refresh(&inv_Detail_Info_Data[i+1],&Inv_can[i]);
			}
		}	
	}

	return;

}

/*------------------------------------------------------------------------------
 Function: Inv_Info_Update_OnErr
 -----------------------------------------------------------------------------*/
/**
  * @brief    基于触发故障\报警事件做log信息存储
  * @param[in]  uint16_t alarmCode
  *  uint8_t happen
  * uint8_t node_id 0:汇总  1~3：单机
  * uint64_t now_date
  * @param[out] 
  * @return    void 
  */
void Inv_Info_Update_OnErr(uint16_t alarmCode,uint8_t happen,uint8_t node_id,uint64_t now_date)
{
	ESP_LOGI(TAG,"Inv_Detailed_Info_update now_date1:%llu,node_id:%d",now_date,node_id);
	struct tm tm_now;
	struct tm tm_now1;
	if((DEV_MAIN_NODE_MAX+1)==node_id)
	{
		node_id=0;
	}

	time_t cur_time=now_date;
	time_t pre_time=inv_Detail_Info_Data[node_id].cur_DetailedInfo.curTime;
//	static uint16_t scnt=0;
	memcpy(&tm_now, localtime(&cur_time), sizeof(struct tm));
	memcpy(&tm_now1, localtime(&pre_time), sizeof(struct tm));
	
	ESP_LOGI(TAG, "Inv_Info_Update_OnErr pre ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now1.tm_mon+1,tm_now1.tm_mday,tm_now1.tm_hour,tm_now1.tm_min,tm_now1.tm_sec);
	ESP_LOGI(TAG, "Inv_Info_Update_OnErr cur ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now.tm_mon+1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);

	if((now_date>inv_Detail_Info_Data[node_id].cur_DetailedInfo.curTime)&&(tm_now.tm_sec!=tm_now1.tm_sec))
	{
		memcpy((uint8_t*)&inv_Detail_Info_Data[node_id].pre_DetailedInfo,(uint8_t*)&inv_Detail_Info_Data[node_id].cur_DetailedInfo,sizeof(Inv_Detailed_Info));
		inv_Detail_Info_Data[node_id].cur_DetailedInfo.curTime=cur_time;
		inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.inv_parallel_seq=node_id-1;
		inv_Detail_Info_Data[node_id].cur_DetailedInfo.device_seq.bit.pack_parallel_seq=(((node_id-1)<<8)+SLAVE_PACK_READ_ADDR);
		Inv_Detailed_Info_Refresh(&inv_Detail_Info_Data[node_id],&Inv_can[node_id-1]);
	}

	inv_Detail_Info_Data[node_id].cur_DetailedInfo.alarmCode=alarmCode;
	if(happen)
		inv_Detail_Info_Data[node_id].cur_DetailedInfo.alarmCode|=0x8000;

	ESP_LOGI(TAG," Inv_Info_Update_OnErr pre_DetailedInfo:");
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, &inv_Detail_Info_Data[node_id].pre_DetailedInfo, sizeof(inv_Detail_Info_Data[node_id].pre_DetailedInfo), ESP_LOG_WARN);
	ESP_LOGI(TAG," Inv_Info_Update_OnErr cur_DetailedInfo:");
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, &inv_Detail_Info_Data[node_id].cur_DetailedInfo, sizeof(inv_Detail_Info_Data[node_id].cur_DetailedInfo), ESP_LOG_WARN);
	Inv_Detailed_Log_Push(node_id,&inv_Detail_Info_Data[node_id]);
	reals.flasWrFlag.sBit.log_invdetailedinfo=1;
}

/*------------------------------------------------------------------------------
 Function: BoxSettingTargetPower
 -----------------------------------------------------------------------------*/
/**
  * @brief    发送馈电功率，仅欧规样机特殊调试用，实际出厂程序没有用处
  * @param[in]  uint8_t Inv_index
  *  uint16_t ACPhase_cnt
  * int16_t  *TargetPower
  * @param[out] 
  * @return    void 
  */
int8_t BoxSettingTargetPower(uint8_t Inv_index, uint16_t ACPhase_cnt, int16_t  *TargetPower)
{
	uint8_t dst=0;
	IdStruct canid;
    int8_t  RET_SUCCESS = 1;
    can_ems_t can_ems = {0};

	if(Inv_index < 16)
	{
		dst = INV_CAN_ADDR + Inv_index;
	}
	else
	{
		dst = 0xFF;
	}
	
    // if(1 == ACPhase_cnt)// 
	{
		/* 填充CANID数据结构: 0x1901xxXX */
		canid.bit.src = esp_canbus_myself_address();
		canid.bit.dst = dst;//CAN_BROADCAST_ADDRESS;
		canid.bit.funcode = CAN_REPORT_FRAME_FUNC_CODE;
		canid.bit.page = CAN_REPORT_FRAME_PAGE;
		canid.bit.priority = CAN_REPORT_FRAME_PRIORITY;

		can_ems.power_l1 =TargetPower[0];
		can_ems.power_l2 = TargetPower[1];
        can_ems.power_l3 = TargetPower[2];

		/* 发送单帧CAN数据 */
		if (!CanAckData(0, canid.all, (uint8_t *)&can_ems, sizeof(can_ems_t))) 
		{
			RET_SUCCESS = -1;
		}
		can_ems.all = 0;
		}
    return RET_SUCCESS;
}

/*------------------------------------------------------------------------------
 Function: setTargetPowerForSoc
 -----------------------------------------------------------------------------*/
/**
  * @brief    基于SOC值自动开关馈电 仅欧规样机特殊调试用，实际出厂程序没有用处
  * @param[in]  void
  * @param[out] 
  * @return    void 
  */
void setTargetPowerForSoc(void)
{
	static uint8_t feedFlag=0;
	static uint16_t feedSendCount=0; 
	uint16_t testTartgetPower[4];
	feedSendCount++;
	ESP_LOGI(TAG,"setTargetPowerForSoc:%d",Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_base.soc);
	if(Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_base.soc>=70)
	{
		if(feedFlag==0)
		{
			feedFlag=1;
			testTartgetPower[0]=0x5dc;
			testTartgetPower[1]=0x5dc;
			testTartgetPower[3]=1;
			BoxSettingTargetPower(0xFF, 1, (int16_t*)testTartgetPower);
			ESP_LOGI(TAG,"FEED START ");
		}
	}else if(Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_base.soc<50)
	{
		if(feedFlag==1)
		{
			feedFlag=0;
			testTartgetPower[0]=0;
			testTartgetPower[1]=0;
			testTartgetPower[3]=1;
			BoxSettingTargetPower(0xFF, 1, (int16_t*)testTartgetPower);
			ESP_LOGI(TAG,"FEED CLOSE ");
		}

	}
	if(feedSendCount>60)
	{
		feedSendCount=0;
		if(feedFlag==0)
		{
			testTartgetPower[0]=0;
			testTartgetPower[1]=0;
			testTartgetPower[3]=1;
			BoxSettingTargetPower(0xFF, 1, (int16_t*)testTartgetPower);
			ESP_LOGI(TAG,"FEED START2 ");
		}else{
			testTartgetPower[0]=0x5dc;
			testTartgetPower[1]=0x5dc;
			testTartgetPower[3]=1;
			BoxSettingTargetPower(0xFF, 1, (int16_t*)testTartgetPower);
			ESP_LOGI(TAG,"FEED CLOSE2");
		}	
	}
}

/*------------------------------------------------------------------------------
 Function: detailed_test
 -----------------------------------------------------------------------------*/
/**
  * @brief    inv逆变详细Log信息调试函数，可根据上位机设置的变量打印和清除记录的log信息
  * @param[in]  void  
  * @param[out] 
  * @return    void 
  */
void detailed_test(void)
{
	// EXT_RAM_BSS_ATTR static Inv_Detailed_Info_Datas info_data;
	// if(Inv[3].mod_reg12000_IOT_set.testdetailed&&Inv[3].mod_reg12000_IOT_set.testdetailed<0xff)
	// {
	// 	ESP_LOGI(TAG,"testdetailed:%u",Inv[3].mod_reg12000_IOT_set.testdetailed);
	// 	GetInvDetailInfo(&info_data,1,(sizeof(Inv_Detailed_Info_Datas)*(Inv[3].mod_reg12000_IOT_set.testdetailed-1)));
	// 	GetInvDetailInfo(&info_data,0,(sizeof(Inv_Detailed_Info_Datas)*(Inv[3].mod_reg12000_IOT_set.testdetailed-1)));
	// }else if(Inv[3].mod_reg12000_IOT_set.testdetailed==0xff)
	// {
	// 	ESP_LOGI(TAG,"clear_DetailInfo_File");
	// 	clear_DetailInfo_File();
	// }
	// if(Inv[3].mod_reg12000_IOT_set.testTartgetPower[3])
	// {
	// 	BoxSettingTargetPower(0xFF, 1, (int16_t*)Inv[3].mod_reg12000_IOT_set.testTartgetPower);
	// 	memset(g_self_data.mod_reg12000_IOT_set.testTartgetPower,0,sizeof(g_self_data.mod_reg12000_IOT_set.testTartgetPower));
	// 	memset(Inv[3].mod_reg12000_IOT_set.testTartgetPower,0,sizeof(Inv[3].mod_reg12000_IOT_set.testTartgetPower));
	// }
	// Inv[3].mod_reg12000_IOT_set.testdetailed=0;
	// g_self_data.mod_reg12000_IOT_set.testdetailed=0;

	// setTargetPowerForSoc();
	
	// uint64_t datalen=sizeof(Inv_Detailed_Info_Datas);
	// ESP_LOGI(TAG,"datalen:%llu",datalen);
}


// void check_fs_space() {
//     // 获取文件系统信息
//     size_t total = 0, used = 0;
//     esp_err_t ret = esp_littlefs_info(NULL, &total, &used);

//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "获取文件系统信息失败: %s", esp_err_to_name(ret));
//         return;
//     }

//     ESP_LOGI(TAG, "总空间: %d KB", total / 1024);
//     ESP_LOGI(TAG, "已用空间: %d KB", used / 1024);
//     ESP_LOGI(TAG, "剩余空间: %d KB", (total - used) / 1024);
// }

/*------------------------------------------------------------------------------
 Function: box_task_init
 -----------------------------------------------------------------------------*/
/**
  * @brief    创建iot_box_task任务、逆变器log信息队列
  * @param[in]  void  
  * @param[out] 
  * @return    void 
  */
void box_task_init(void)
{
	xQueue_inv_Detailed_Info_record= xQueueCreate(10, sizeof(Log_Inv_Detailed_Info_Queue_Struct));
	if (!xQueue_inv_Detailed_Info_record) {
		ESP_LOGE (TAG, "xQueue_inv_Detailed_Info_record queue create failed");
		abort();
	}

	// if (xTaskCreatePinnedToCore(iot_box_task, "iot-box", 1024 * 4, NULL, 1, NULL, 1) != pdPASS) {  /* 周期更新WiFi+BLE状态到CAN数据 */
	// 	ESP_LOGE(TAG, "iot_box_task create failed");
		
	// 	abort();
	// }
}

#define  LEVEL_I_ON_PV	20//20A
#define  DELTA_I_CHANGE_PV	20//20A

// 定义电流变化事件标志的枚举类型
typedef enum {
    CHARGING_START =1,//>0
    CHARGING_END,
    DISCHARGING_START,//<0
    DISCHARGING_END,
    CHARGING_CURRENT_INCREASE,
    CHARGING_CURRENT_DECREASE,
	DISCHARGING_CURRENT_INCREASE,
	DISCHARGING_CURRENT_DECREASE
    
} CurrentEventFlag;


// 函数用于根据当前电流值判断电流变化的事件标志
CurrentEventFlag get_current_event_flag(int16_t *I_new,int16_t *I_old,uint16_t *level_zero,uint16_t *delta_change) 
{
    CurrentEventFlag event =0;

    // 充电开始：上一次电流小于等于 0 且当前电流大于 0
    if ((*I_old <= 0) && (*I_new >  *level_zero)) {
        event = CHARGING_START;
    } 
    // 充电结束：上一次电流大于 0 且当前电流小于等于 0
    else if ((*I_old > 0 )&& (*I_new <= 0) ){
        event = CHARGING_END;
    } 
    // 放电开始：上一次电流大于等于 0 且当前电流小于 0
    else if ((*I_old >= 0) && (*I_new < -(int16_t)*level_zero)) {
        event = DISCHARGING_START;
    } 
    // 放电结束：上一次电流小于 0 且当前电流大于等于 0
    else if ((*I_old < 0) && (*I_new >= 0)) {
        event = DISCHARGING_END;
    } 
    // 充电电流增加：上一次电流小于 0 且当前电流小于上一次电流（绝对值更大）
    else if ((*I_old > 0) && (*I_new > (*I_old +*delta_change)) )
	{
        event = CHARGING_CURRENT_INCREASE;
    } 
    // 充电电流减少：上一次电流小于 0 且当前电流大于上一次电流（绝对值更小）
    else if ((*I_old > 0) && ((*I_new +*delta_change)< *I_old) )
	{
        event = CHARGING_CURRENT_DECREASE;
    } 
    // 放电电流增加：上一次电流小于 0 且当前电流小于上一次电流（绝对值更大）
    else if ((*I_old < 0) && (*I_new < (*I_old -*delta_change)) )
	{
        event = DISCHARGING_CURRENT_INCREASE;
    } 
    // 放电电流减少：上一次电流小于 0 且当前电流大于上一次电流（绝对值更小）
    else if ((*I_old < 0) && (*I_new > (*I_old+*delta_change)) )
	{
        event = DISCHARGING_CURRENT_DECREASE;
    } 	
    else {
        // 无明显事件变化
        // 这里可以根据需求添加默认处理逻辑，例如返回一个特殊的枚举值
        // 为了简单起见，这里不做处理
    }
	if(event)
	{
		// 更新上一次的电流值
		*I_old = *I_new;
	}


    return event;
}

/*------------------------------------------------------------------------------
 Function: SaveInfoOn_Current_Change
 -----------------------------------------------------------------------------*/
/**
  * @brief    黑匣子数据处理任务 
  * @param[in]  void  
  * @param[out] 
  * @return    void 
  */
void SaveInfoOn_Current_Change(void)
{
	static int16_t I_PV_old=0;
	static int16_t I_bat_old=0;
	
	uint16_t tempdata =0;
	uint8_t node_id =0;
	uint16_t temp_zero =0;
	uint16_t temp_delta =0;

	 temp_zero =LEVEL_I_ON_PV;
	 temp_delta =DELTA_I_CHANGE_PV;

    node_id =reals.Addr_can_self;
	tempdata =get_current_event_flag((int16_t*)&Inv_can[reals.Addr_can_self].pack_data[PACK_MAX_NUM].pack_announce.total_current,&I_bat_old,&temp_zero,&temp_delta);
	if(0 != tempdata)
	{
		if(CHARGING_START == tempdata)
		{
			SaveEventInfo(1,Battery_Charge_Start,node_id+1);
		}
		else if(CHARGING_END == tempdata)
		{
			SaveEventInfo(0,Battery_Charge_Start,node_id+1);
		}
		else if(DISCHARGING_START == tempdata)
		{
			SaveEventInfo(1,Battery_Discharge_Start,node_id+1);
		}
		else if(DISCHARGING_END == tempdata)
		{
			SaveEventInfo(0,Battery_Discharge_Start,node_id+1);
		}
		else if(CHARGING_CURRENT_INCREASE == tempdata)
		{
			SaveEventInfo(1,Battery_Power_chg_change,node_id+1);
		}
		else if(CHARGING_CURRENT_DECREASE == tempdata)
		{
			SaveEventInfo(0,Battery_Power_chg_change,node_id+1);
		}
		else if(DISCHARGING_CURRENT_INCREASE == tempdata)
		{
			SaveEventInfo(1,Battery_Power_disg_change,node_id+1);
		}
		else if(DISCHARGING_CURRENT_DECREASE == tempdata)
		{
			SaveEventInfo(0,Battery_Power_disg_change,node_id+1);
		}

	}

	 temp_zero =LEVEL_I_ON_PV;
	 temp_delta =DELTA_I_CHANGE_PV;

	tempdata =get_current_event_flag((int16_t*)&Inv_can[reals.Addr_can_self].inv_data[INV_MAX_NUM].inv_pv.pv_detail[0].input_current,&I_PV_old,&temp_zero,&temp_delta);
	if(0 != tempdata)
	{
		if(CHARGING_START == tempdata)
		{
			SaveEventInfo(1,PV_Charge_Start,node_id+1);
		}
		else if(CHARGING_END == tempdata)
		{
			SaveEventInfo(0,PV_Charge_Start,node_id+1);
		}
		else if(CHARGING_CURRENT_INCREASE == tempdata)
		{
			SaveEventInfo(1,PV_Power_change,node_id+1);
		}
		else if(CHARGING_CURRENT_DECREASE == tempdata)
		{
			SaveEventInfo(0,PV_Power_change,node_id+1);
		}

	}


}


/*------------------------------------------------------------------------------
 Function: iot_box_task
 -----------------------------------------------------------------------------*/
/**
  * @brief    黑匣子数据处理任务 
  * @param[in]  void  
  * @param[out] 
  * @return    void 
  */
void iot_box_task(void)//100ms
{
	static uint8_t powerOn_Count=1;
	static uint16_t ts_u16Cnt10min=0;
	static uint8_t ts_u16100ms_Count=0;

	if(++ts_u16100ms_Count >= 10)//1s
	{
		ts_u16100ms_Count=0; 

		//每秒刷新一次log详细信息
		Inv_Detailed_Info_update();	

		//AC380没有RTC ,需要从外部获取准确时间
		if(((1 == reals.rtc_flag.sBit.RTC_valid_from_NTP)
			||(1 == reals.rtc_flag.sBit.RTC_valid_from_APP)
			||(1 == reals.rtc_flag.sBit.RTC_valid_from_SERVER))
		&&(1 == reals.rtc_flag.sBit.RTC_valid_from_CAN)
		)
		{
			if(powerOn_Count)
			{
				ESP_LOGI(TAG,"powerOn_Count:%d",powerOn_Count);
				//记录IOT首次上电事件
				SaveInfoOnPowerOn(reals.Addr_can_self);
				powerOn_Count=0;
			}

			//SaveInfoOn_Current_Change();

			if(++ts_u16Cnt10min >= 43200)//每12小时进行一次定时记录
			{
				ESP_LOGI(TAG,"ts_u16Cnt10min:%u",ts_u16Cnt10min);
				ts_u16Cnt10min=0;
				SaveInfoOnCycleRecord(reals.Addr_can_self);
			}
		}
		 
		//逆变详细Log信息调试，根据上位机设置的变量打印和清除记录的log信息
		//detailed_test();
		//实时检测Inv_AcHub变量中故障信息，故障发生和消失时触发逆变log信息的数据存储
		//inv_fault_check();
	}
}


#endif
