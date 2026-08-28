#include "ll_param.h"
//#include "kvdb_eep.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_bit_defs.h"
#include <string.h>
#include <stdio.h>
#include "iot_period_task.h"


static const char *TAG = "[LL_PARAM]";

#define LL_PARAM_SYNC_EVT 	BIT0

typedef void (*param_set_default)(int param_idx);

/**
 * @brief 参数列表数据结构定义
 */
typedef struct {
	char *name;
	int size;
} ll_param_list_t;

typedef struct {
	char *listname;
	char *savename;
} ll_param_list_map;

#define INV_PARAM_COUNT		sizeof(inv_param_list) / sizeof(ll_param_list_t)		// 逆变参数数量
static ll_param_list_t inv_param_list[] = INV_PARAM_LIST;							// 逆变参数列表
static uint8_t *inv_data_list[INV_PARAM_COUNT];										// 逆变参数数据
//
//#define PACK_PARAM_COUNT	 sizeof(pack_param_list) / sizeof(ll_param_list_t)		// PACK参数数量
//static ll_param_list_t pack_param_list[] = PACK_PARAM_LIST;							// PACK参数列表
//static uint8_t *pack_data_list[PACK_PARAM_COUNT];									// PACK参数数据

#define IOT_PARAM_COUNT	 sizeof(iot_param_list) / sizeof(ll_param_list_t)		// IOT参数数量
static ll_param_list_t iot_param_list[] = IOT_PARAM_LIST;							// IOT参数列表
static uint8_t *iot_data_list[IOT_PARAM_COUNT];									// IOT参数数据

EXT_RAM_BSS_ATTR static param_type_t config_param_type;												// 将要配置的参数类型

EXT_RAM_BSS_ATTR static EventGroupHandle_t ll_param_evt;

EXT_RAM_BSS_ATTR static uint8_t iot_buf[IOT_PARAM_COUNT][255];
EXT_RAM_BSS_ATTR static uint8_t inv_buf[INV_PARAM_COUNT][255];

#define PARRAM_LIST_NUM (IOT_PARAM_COUNT+INV_PARAM_COUNT)

static const ll_param_list_map name_map[PARRAM_LIST_NUM]=
{
	{LL_DISASTER_PARAM_NAME, LL_DISASTER_SAVE_NAME},
	{LL_INVBASE_PARAM_NAME, LL_INVBASE_SAVE_NAME},	
	{LL_INVADVANCE_PARAM_NAME, LL_INVADVANCE_SAVE_NAME},
	{LL_INVAUTH_PARAM_NAME, LL_INVAUTH_SAVE_NAME},			
};

static int list_name_mapping(char* name,char*savename)
{
	if((name==NULL)||(savename==NULL))
	{
		ESP_LOGE(TAG,"savename OR name null");
		return -1;
	}
	uint8_t i;

	for(i=0;i<PARRAM_LIST_NUM;i++)
	{
		ESP_LOGI(TAG,"name:%s,name_map[%d].listname:%s",name,i,name_map[i].listname);
		if(strcmp(name,name_map[i].listname)==0)
		{
			ESP_LOGI(TAG,"savename:%s,strlen(name_map[i].savename):%d",savename,strlen(name_map[i].savename));
			strncpy(savename,name_map[i].savename,strlen(name_map[i].savename));
			return	0;
		}
	}
	ESP_LOGW(TAG,"Not Find MAP Name");
	return-1; 
}

/**
 * @brief iot基本参数默认值
 *
 * @param param_idx 参数在列表中的索引
 * @return 无
 */
static void iot_param_set_default(int param_idx)
{
	if(param_idx >= IOT_PARAM_COUNT) return;
	disaster_param_t iot_param = {0};
	iot_param.AppCancelStormEvent.DataIdentificationCode = 0xAA55;
	memcpy(iot_data_list[param_idx], &iot_param, sizeof(disaster_param_t));
}

static param_set_default iot_set_default_param[IOT_PARAM_COUNT] = {
	iot_param_set_default,
};

/**
 * @brief 查找iot参数在参数列表中的索引
 *
 * @param name iot参数名
 * @param index 返回的参数索引
 * @return 查找成功返回0，否则返回-1
 */
static int find_iot_param_index(char *name, int *index)
{
	if((name==NULL)||(index==NULL))
	{
		ESP_LOGE(TAG,"find_iot_param_index NULL");
		return -1;
	}

	for(int i = 0; i < IOT_PARAM_COUNT; i++)
	{
		ESP_LOGI(TAG,"find_iot_param_index :%d",i);
		ESP_LOGI(TAG,"find_iot_param_index NAME :%s",iot_param_list[i].name);
		if(strcmp(name, iot_param_list[i].name) == 0)
		{
			*index = i;
			return 0;
		}
	}
	return -1;
}

/**
 * @brief iot参数设置
 * - 设置成功后的参数将会保存到kvdb中
 *
 * @param name 参数名
 * @param data 参数数据指针
 * @param data_len 参数大小
 * @return 成功返回0，否则返回-1
 */
int ll_iot_param_set(char *name, void *data, int data_len, char *tag)
{
	// if (!ll_param_evt) return -1;

	int param_idx;
	int ret = 0;

	if(find_iot_param_index(name, &param_idx) != 0)
	{
		ESP_LOGE(TAG, "parameter not found, name:%s", name);
		ret = -1;
		goto __exit;
	}

	int param_len = iot_param_list[param_idx].size;
	if(iot_data_list[param_idx] == NULL)
	{
		ESP_LOGE(TAG, "set error, name:%s, expect len:%d, actual len:%d, data pointer:%lu",
			name, param_len, data_len, (uint32_t)iot_data_list[param_idx]);
		ret = -1;
		goto __exit;
	}

	/* 设置的参数与本地参数一致，直接返回成功 */
	if(memcmp(iot_data_list[param_idx], data, data_len) == 0) {
		goto __exit;
	}

	ESP_LOGW(TAG, "%s set %s parameters", tag, name);
	ret = SaveFileileWrite(LL_DISASTER_SAVE_NAME, data,param_idx,0, data_len);
	memcpy(iot_data_list[param_idx], data, data_len);
	// config_param_type = (param_type_t)(INV_PARAM_START + param_idx + 1);
	// xEventGroupSetBits(ll_param_evt, LL_PARAM_SYNC_EVT);

__exit:
	return ret;
}

void* ll_iot_param_get(char *name)
{
	// if (!ll_param_evt) return NULL;
	ESP_LOGE(TAG, "ll_iot_param_get1");
	int param_idx;
	if(find_iot_param_index(name, &param_idx) != 0)
	{
		ESP_LOGE(TAG, "parameter not found, name:%s", name);
		return NULL;
	}
	if(iot_data_list[param_idx]!=NULL)
	{
		return iot_data_list[param_idx];
	}
	else
	{
		ESP_LOGE(TAG, "iot_data_list[%d] NULL", param_idx);
		return NULL;
	}
}


/**
 * @brief iot参数初始化
 * - 该函数从存储空间中获取保存的逆变参数到本地缓存，如果参数不存在，则使用默认值
 *
 * @return 无
 */
static void iot_param_init(void)
{
	memset(iot_buf,0,sizeof(iot_buf));
	for (int i = 0; i < IOT_PARAM_COUNT; i++)
	{
		int sava_len, param_len;
		char buff[512];
		char name[15]={0};
		char *key = iot_param_list[i].name;
		param_len = iot_param_list[i].size;
		ESP_LOGI(TAG,"param_len:%d",param_len);
		if(param_len>=255)
		{
			ESP_LOGW(TAG,"Err i:%d param_len:%d ",i,param_len);
			return ;
		}
		/* 申请一块内存用于存储kvdb中保存的设备参数 */
		//uint8_t *pdata = heap_caps_malloc(param_len, MALLOC_CAP_SPIRAM);
		//if(pdata == NULL) return;
		//iot_data_list[i] = pdata;
		iot_data_list[i] = &iot_buf[i];
		if(list_name_mapping(key,name)!=0)
		{
			ESP_LOGW(TAG,"list_name_mapping GET ERR i:%d key:%s",i ,key);
			continue;
		}
		//iot_set_default_param[i](i);	// 先使用默认值装载,这样当新增数据成员到末尾时旧数据不会对新增加成员数据造成影响

		if(FileReadForName(name, buff,i,param_len*i, param_len) == 0)		// 从kvdb中读取key对应的数据
		{
			ESP_LOGI(TAG, "load iot parameters[%s] from filesystem :%s", key,name);
			memcpy(&iot_buf[i], buff, param_len);									// 保存kvdb中的配置数据到本地
		}
		else
		{
			ESP_LOGW(TAG, "use default values for iot paramters[%s]", key);
			iot_set_default_param[i](i);									// key值不存在或读取失败时重新装载默认值
		}
	}
}

#if 1	
/**
 * @brief 逆变参数初始化
 * - 该函数从kvdb中获取保存的逆变参数到本地缓存，如果参数不存在，则使用默认值
 *
 * @return 无
 */
static void inv_param_init(void)
{
	for (int i = 0; i < INV_PARAM_COUNT; i++)
	{
		int sava_len, param_len;
		char buff[512];
		char name[15];
		memset(name,0,sizeof(name));
		char *key = inv_param_list[i].name;
		param_len = inv_param_list[i].size;

		/* 申请一块内存用于存储kvdb中保存的设备参数 */
		//uint8_t *pdata = heap_caps_malloc(param_len, MALLOC_CAP_SPIRAM);
		//if(pdata == NULL) return;
		//inv_data_list[i] = pdata;
		inv_data_list[i]=&inv_buf[i];
		if(list_name_mapping(key,name)!=0)
		{
			ESP_LOGW(TAG,"list_name_mapping GET ERR i:%d key:%s",i ,key);
			continue;
		}
		//ep900_set_default_param[i](i);	// 先使用默认值装载,这样当新增数据成员到末尾时旧数据不会对新增加成员数据造成影响

		if(FileReadForName(name, buff,i,param_len*i, param_len) == 0)		// 从文件系统中读取key对应的数据
		{
			ESP_LOGI(TAG, "load inv parameters[%s] from filesystem :%s", key,name);
			memcpy(&inv_buf[i], buff, param_len);									// 保存kvdb中的配置数据到本地
		}
		else
		{
			ESP_LOGW(TAG, "use default values for inv paramters[%s]", key);
			//ep900_set_default_param[i](i);									// key值不存在或读取失败时重新装载默认值
		}
	}
}

/**
 * @brief 查找逆变参数在参数列表中的索引
 *
 * @param name 逆变参数名
 * @param index 返回的参数索引
 * @return 查找成功返回0，否则返回-1
 */
static int find_inv_param_index(char *name, int *index)
{
	if((name==NULL)||(index==NULL))
	{
		ESP_LOGE(TAG,"find_inv_param_index NULL");
		return -1;
	}
	for(int i = 0; i < INV_PARAM_COUNT; i++)
	{
		ESP_LOGI(TAG,"find_iot_param_index :%d",i);
		ESP_LOGI(TAG,"find_iot_param_index NAME :%s",iot_param_list[i].name);
		if(strcmp(name, inv_param_list[i].name) == 0)
		{
			*index = i;
			return 0;
		}
	}
	return -1;
}

/**
 * @brief 获取逆变参数
 *
 * @param name 参数名
 * @return 返回参数数据指针
 * - 应用层需要将返回的指针转换为对应的参数类型
 */
void* ll_inv_param_get(char *name)
{
	//if (!ll_param_evt) return NULL;
	ESP_LOGE(TAG, "ll_inv_param_get");
	int param_idx;
	if(find_inv_param_index(name, &param_idx) != 0)
	{
		ESP_LOGE(TAG, "parameter not found, name:%s", name);
		return NULL;
	}

	return inv_data_list[param_idx];
}

/**
 * @brief 逆变参数设置
 * - 设置成功后的参数将会保存到kvdb中
 *
 * @param name 参数名
 * @param data 参数数据指针
 * @param data_len 参数大小
 * @return 成功返回0，否则返回-1
 */
int ll_inv_param_set(char *name, void *data, int data_len, char *tag)
{

	int param_idx;
	int ret = 0;
	if(find_inv_param_index(name, &param_idx) != 0)
	{
		ESP_LOGE(TAG, "parameter not found, name:%s", name);
		ret = -1;
		goto __exit;
	}

	int param_len = inv_param_list[param_idx].size;
	if(inv_data_list[param_idx] == NULL)
	{
		ESP_LOGE(TAG, "set error, name:%s, expect len:%d, actual len:%d, data pointer:%lu",
			name, param_len, data_len, (uint32_t)inv_data_list[param_idx]);
		ret = -1;
		goto __exit;
	}

	/* 设置的参数与本地参数一致，直接返回成功 */
	if(memcmp(inv_data_list[param_idx], data, data_len) == 0) {
		goto __exit;
	}

	ESP_LOGW(TAG, "%s set %s parameters", tag, name);
	ret = SaveFileileWrite(LL_DISASTER_SAVE_NAME, data,param_idx,0, data_len);
	memcpy(inv_data_list[param_idx], data, data_len);
	config_param_type = (param_type_t)(INV_PARAM_START + param_idx + 1);
	xEventGroupSetBits(ll_param_evt, LL_PARAM_SYNC_EVT);

__exit:
	return ret;
}




#endif
#if 0
/**
 * @brief ep900基本参数默认值
 *
 * @param param_idx 参数在列表中的索引
 * @return 无
 */
static void inv_pack_param_set_default(void)
{
	inv_base_param_t *base_param = &SetData_Can.dev_info_t2.inv_set00;//ll_inv_param_get("inv_base_param");
	inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
	inv_auth_param_t *auth_param = &SetData_Can.dev_info_t2.inv_set03;//ll_inv_param_get("inv_auth_param");
	pack_config_param_t *config_param = &SetData_Can.dev_info_t2.pack_config;//ll_inv_param_get("inv_auth_param");

	base_param->ctrl_low_cap_pct = 20;
	base_param->ctrl_high_cap_pct = 80;
	base_param->ctrl_time_area = 0;
	base_param->ctrl_alarm_voice = 1;
/////////////////

	advance_param->ctrl_grid = 1;
	advance_param->ctrl_feedback = 1;
	advance_param->ctrl_output_inv_volt = 1;
	advance_param->ctrl_output_inv_freq = 1;
	advance_param->ctrl_grid_max_power = 6960;
	advance_param->ctrl_grid_max_curr = 58;
	advance_param->ctrl_feedback_max_power = 4500;
	advance_param->ctrl_feedback_max_curr = 37;
	advance_param->ctrl_user_area = 100;
	advance_param->ctrl_grid_plus = 0;
	advance_param->ctrl_save_power_state = 0;
	advance_param->ctrl_meter_enable = 0;
	advance_param->ctrl_meter_select = 0;

///////////////
	auth_param->SetGridEnable = 0;
	auth_param->SetGridCountry = 100; 	   // 100-美国，101-日本，102-韩国...
	auth_param->SetGridUV1Value = 1056;
	auth_param->SetGridUV1Time = 2100;
	auth_param->SetGridUV2Value = 600;
	auth_param->SetGridUV2Time = 200;
	auth_param->SetGridAVGOVValue = 0;
	auth_param->SetGridOV1Value = 1320;
	auth_param->SetGridOV1Time = 1300;
	auth_param->SetGridOV2Value = 1440;
	auth_param->SetGridOV2Time = 16;
	auth_param->SetGridUF1Value = 5490;
	auth_param->SetGridUF1Time = 30000;
	auth_param->SetGridUF2Value = 5450;
	auth_param->SetGridUF2Time = 16;
	auth_param->SetGridOF1Value = 6510;
	auth_param->SetGridOF1Time = 30000;
	auth_param->SetGridOF2Value = 6550;
	auth_param->SetGridOF2Time = 16;
	auth_param->SetGridVoltMinValue = 0;
	auth_param->SetGridVoltMaxValue = 0;
	auth_param->SetGridFreqMinValue = 0;
	auth_param->SetGridFreqMaxValue = 0;
	auth_param->SetGridRetryTime = 20000;
	auth_param->SetDefaultValue = 0;
////////////
	config_param->pack_heat_enable = 0;
	config_param->ctr_heat_enable = 0;
	config_param->unlock_failed_flags = 0;


}

/**
 * @brief 逆变参数初始化
 * - 该函数从kvdb中获取保存的逆变参数到本地缓存，如果参数不存在，则使用默认值
 *
 * @return 无
 */
static void inv_param_init(void)
{
	inv_pack_param_set_default();

}
#endif

/**
 * @brief 底层设备参数初始化
 * - 该函数会初始化kvdb库，同时从kvdb中获取保存的参数到本地缓存
 *
 * @return 无
 */
void ll_param_init(void)
{
	inv_param_init();
	iot_param_init();
//	ll_param_evt = xEventGroupCreate();
//	assert(ll_param_evt);
}

/**
 * @brief 等待参数同步事件
 *
 * @param timeout 超时时间
 * @return 成功返回0，否则返回-1
 */
int ll_param_wait_sync(uint32_t timeout)
{
	EventBits_t bit_mask = LL_PARAM_SYNC_EVT;
	int bits = xEventGroupWaitBits(ll_param_evt, bit_mask, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout));
	if(bits & LL_PARAM_SYNC_EVT) {
		return 0;
	}
	return -1;
}

/**
 * @brief dump disaster参数
 *
 * @return 无
 */
void disaster_param_dump(void)
{
	int param_idx;
	if(find_iot_param_index("disaster_save_param", &param_idx) != 0)
	{
		ESP_LOGE(TAG, "disaster_save_param not found");
		return;
	}
	if(param_idx >= IOT_PARAM_COUNT) return;

	ESP_LOGI(TAG, "---------------disaster_save_param dump---------------");
	disaster_param_t *disaster_param = (disaster_param_t *)iot_data_list[param_idx];
	ESP_LOGI(TAG, "disaster_param.power_on_flag:%d", power_on_flag);
	ESP_LOGI(TAG, "disaster_param.LastWorkModes:%d", disaster_param->LastWorkMode);
	ESP_LOGI(TAG, "disaster_param.StartTime:%lu", disaster_param->SaveWebStormWatchApiData.StartTime);
	ESP_LOGI(TAG, "disaster_param.EndTime:%lu", disaster_param->SaveWebStormWatchApiData.EndTime);
	ESP_LOGI(TAG, "disaster_param.Event:%d", disaster_param->SaveWebStormWatchApiData.Event);
	ESP_LOGI(TAG, "disaster_param.SoC:%d", disaster_param->SaveWebStormWatchApiData.SoC);
	ESP_LOGI(TAG, "disaster_param.disaster_send_param.soc_start:%d", disaster_param->disaster_send_param.soc_start);
	ESP_LOGI(TAG, "disaster_param.disaster_send_param.soc_end:%d", disaster_param->disaster_send_param.soc_end);
	ESP_LOGI(TAG, "disaster_param.disaster_send_param.BackupStartTime:%lu", disaster_param->disaster_send_param.BackupStartTime);
	ESP_LOGI(TAG, "disaster_param.disaster_send_param.BackupEndTime:%lu", disaster_param->disaster_send_param.BackupEndTime);
	ESP_LOGI(TAG, "disaster_param.AppCancelStormEvent:%d", disaster_param->AppCancelStormEvent.EventEndFlag);
	ESP_LOGI(TAG, "disaster_param.StormWatchState.now:%d", StormWatchSetData.StormWatchState);
	ESP_LOGI(TAG, "disaster_param.StormWatchState.now.single:%d", StormWatchSetData.StormWatchStateSingle);
	printf("\n");
}