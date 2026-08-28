#ifndef __LL_PARAM_H__
#define __LL_PARAM_H__
#include <stdint.h>
#include "ll_param_def.h"


#define LL_PARAM_MAGIC_NUM		0x5A5AA5A5	// 底层设备参数标志魔数定义
#define LL_DISASTER_PARAM_NAME "disaster_save_param"
#define LL_DISASTER_SAVE_NAME "disa"

#define LL_INVBASE_PARAM_NAME "inv_base_param"
#define LL_INVBASE_SAVE_NAME "base"

#define LL_INVADVANCE_PARAM_NAME "inv_advance_param"
#define LL_INVADVANCE_SAVE_NAME "adva"

#define LL_INVAUTH_PARAM_NAME "inv_auth_param"
#define LL_INVAUTH_SAVE_NAME "auth"
/**
 * @brief 底层参数标志数据结构定义
 */
typedef struct ll_param
{
	/* 当该标志为指定的魔数时表示参数来自底层设备或上层APP设置
	 * 此时软件会同步参数到每一台设备 */
	uint32_t inv_init_flag;
	uint32_t pack_init_flag;
} ll_param_flag_t;

/**
 * @brief 逆变参数列表
 * - 参数数据结构需要与can_data.h中对应的数据结构保持一致
 */
#define INV_PARAM_LIST	{									\
	{LL_INVBASE_PARAM_NAME, sizeof(inv_base_param_t)},			\
	{LL_INVADVANCE_PARAM_NAME, sizeof(inv_advance_param_t)},		\
	{LL_INVAUTH_PARAM_NAME, sizeof(inv_auth_param_t)},			\
}

/**
 * @brief PACK参数列表
 * - 参数数据结构需要与can_data.h中对应的数据结构保持一致
 */
#define PACK_PARAM_LIST {									\
	{"pack_config_param", sizeof(pack_config_param_t)},		\
}

/**
 * @brief PACK参数列表
 * - 参数数据结构需要与can_data.h中对应的数据结构保持一致
 */
#define IOT_PARAM_LIST {									\
	{LL_DISASTER_PARAM_NAME, sizeof(disaster_param_t)},		\
}

/**
 * @brief 参数类型定义
 */
typedef enum {
	INV_PARAM_START,
	INV_BASE_PARAM,
	INV_ADVANCE_PARAM,
	INV_AUTH_PARAM,
	INV_PARAM_END,
	PACK_PARAM_START,
	PACK_CONFIG_PARAM,
	PACK_PARAM_END
} param_type_t;

void ll_param_init(void);
int ll_param_flag_set(ll_param_flag_t *param_flag);
ll_param_flag_t *ll_param_flag_get(void);
int ll_inv_param_set(char *name, void *data, int data_len, char *tag);
void* ll_inv_param_get(char *name);
int ll_pack_param_set(char *name, void *data, int data_len, char *tag);
void* ll_pack_param_get(char *name);
int ll_param_wait_sync(uint32_t timeout);

void ll_param_flag_dump(void);
void ep900_base_param_dump(void);
void ep900_advance_param_dump(void);
void ep900_auth_param_dump(void);
void b500_config_param_dump(void);

int ll_iot_param_set(char *name, void *data, int data_len, char *tag);
void* ll_iot_param_get(char *name);
param_type_t ll_config_param_type_get(void);
void disaster_param_dump(void);

//============================================

#endif

