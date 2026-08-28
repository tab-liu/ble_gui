#include "data_summary.h"
#include "can_data.h"
//#include "EMSControlBox.h"
//#include "ac_couple_meter.h"
//#include "grid_meter.h"
//#include "energy_data.h"
#include "ll_param.h"
#include "can_protocol.h"
#include "iot_period_task.h"

#include "can_protocol.h"


//static int online_main_node;
//static int online_sub_node;

typedef uint64_t (*data_operate)(uint32_t *buff, uint32_t len);
#define MEMSIZE(type, x)	sizeof(((type*)0)->x)


static const char *TAG = "[data_summary]";

/* 数据求平均值 */
static inline uint64_t data_average_inv(uint32_t *buff, uint32_t len)
{
	uint64_t sum = 0;
	if(reals.online_Inv_num == 0)
	{
		return sum;
	}
	for(int i = 0; i < len; i++) {
		sum += buff[i];
	}
	sum /= reals.online_Inv_num;
	return (uint64_t)sum;
}

/* 数据求平均值 */
static inline uint64_t data_average_pack(uint32_t *buff, uint32_t len)
{
	uint64_t sum = 0;
	if(reals.online_Pack_num == 0)
	{
		return sum;
	}
	for(int i = 0; i < len; i++) {
		sum += buff[i];
	}
	sum /= reals.online_Pack_num;
	return (uint64_t)sum;
}

/* 数据求和 */
static inline uint64_t data_sum(uint32_t *buff, uint32_t len)
{
	uint64_t sum = 0;
	for(int i = 0; i < len; i++) {
		sum += buff[i];
	}
	return sum;
}

/* 数据逻辑或运算 */
static inline uint64_t data_or_operate(uint32_t *buff, uint32_t len)
{
	uint64_t ret = 0;
	for(int i = 0; i < len; i++) {
		ret |= buff[i];
	}
	return ret;
}

/* 计算数据最大值 */
static inline uint64_t data_max(uint32_t *buff, uint32_t len)
{
	uint64_t max = 0;
	for(int i = 0; i < len; i++)
	{
		if(max < buff[i]) {
			max = buff[i];
		}
	}
	return max;
}

/* 计算数据最小值 */
  uint64_t data_min(uint32_t *buff, uint32_t len)//static inline
{
	uint64_t min = 0xffffffff;
	for(int i = 0; i < len; i++)
	{
		if(min > buff[i]) {
			min = buff[i];
		}
	}
	return min;
}

/*
windy 修改二维求和
Inv_can[i].inv_data[y]

*/
static uint32_t summary_inv_node_y(uint8_t index_x, uint8_t *pdata, int offset, int width, data_operate operate)
{
	uint32_t buff[INV_MAX_NUM];
	uint8_t i = 0;
	uint8_t *pdata2;
	uint32_t rtn = 0;

//	ESP_LOGI(TAG, "summary_inv_node_y	      1");

	for (i = 0; i < INV_MAX_NUM; i++)
	{
		pdata2 = pdata + (i * sizeof(inv_node_struct)); // 指针指向下一个inv节点数据
//		ESP_LOGI(TAG, "summary_inv_node_y		 2 ");

		if (Inv_can[index_x].inv_data[i].online)
		{
//			ESP_LOGI(TAG, "summary_inv_node_y		3  ");

			if(width == 1) {
//				ESP_LOGI(TAG, "summary_inv_node_y		 4 ");

				buff[i] = (uint32_t)(*((uint8_t*)(pdata2 + offset)));
			}
			else if(width == 2) {
//				ESP_LOGI(TAG, "summary_inv_node_y		 5 ");

				buff[i] = (uint32_t)(*((uint16_t*)(pdata2 + offset)));
			}
			else if(width == 4) {
//				ESP_LOGI(TAG, "summary_inv_node_y		6  ");

				buff[i] = (uint32_t)(*((uint32_t*)(pdata2 + offset)));
			}
		}
		else
		{
			buff[i] =0;
		}
//		ESP_LOGI(TAG, "summary_inv_node_y		7  ");

	}
	rtn =operate(buff, i);
//	ESP_LOGI(TAG, "summary_inv_node_y	    8  ");

	pdata2 = pdata + ((uint32_t)i * sizeof(inv_node_struct)) + offset;


//	ESP_LOGI(TAG, "summary_inv_node_y		9,Inv_can[0].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Inv_can[0].inv_data[0].inv_announce);
//	ESP_LOGI(TAG, "summary_inv_node_y		9,Inv_can[1].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Inv_can[1].inv_data[0].inv_announce);
//	ESP_LOGI(TAG, "summary_inv_node_y		9,Inv_can[2].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Inv_can[2].inv_data[0].inv_announce);
//
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata=0x%lx  ",(uint32_t)pdata);
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata2=0x%lx  ",(uint32_t)pdata2);
//
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,sizeof(inv_node_struct))=%ld  ",(uint32_t)sizeof(inv_node_struct));
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,i *sizeof(inv_node_struct))=%ld  ",(uint32_t)((uint32_t)i * sizeof(inv_node_struct)));
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)offset=%ld  ",(uint32_t)offset);
//
//	ESP_LOGI(TAG, "summary_inv_node_y	    9  ");
	memcpy(pdata2, (uint8_t *)&rtn, width);//尾部数组是汇总
	//windy debug
//	memcpy(pdata2, rtn, width);//尾部数组是汇总
//	ESP_LOGI(TAG, "summary_inv_node_y	    10  ");


	if(i != 0)
	{
		return rtn;
	}

	return 0;
}


/*
windy 修改二维求和
Inv_can[i].inv_data[y]

*/
static uint32_t summary_Pack_node_y(uint8_t index_x, uint8_t *pdata, int offset, int width, data_operate operate)
{
	uint32_t buff[PACK_MAX_NUM];
	uint8_t i = 0;
	uint8_t *pdata2;
	uint32_t rtn = 0;

//	ESP_LOGI(TAG, "summary_inv_node_y	      1");

	for (i = 0; i < PACK_MAX_NUM; i++)
	{
		pdata2 = pdata + (i * sizeof(pack_node_struct)); // 指针指向下一个inv节点数据
//		ESP_LOGI(TAG, "summary_inv_node_y		 2 ");

		if (Inv_can[index_x].pack_data[i].online)
		{
//			ESP_LOGI(TAG, "summary_inv_node_y		3  ");

			if(width == 1) {
//				ESP_LOGI(TAG, "summary_inv_node_y		 4 ");

				buff[i] = (uint32_t)(*((uint8_t*)(pdata2 + offset)));
			}
			else if(width == 2) {
//				ESP_LOGI(TAG, "summary_inv_node_y		 5 ");

				buff[i] = (uint32_t)(*((uint16_t*)(pdata2 + offset)));
			}
			else if(width == 4) {
//				ESP_LOGI(TAG, "summary_inv_node_y		6  ");

				buff[i] = (uint32_t)(*((uint32_t*)(pdata2 + offset)));
			}
		}
		else
		{
			buff[i] =0;
		}
//		ESP_LOGI(TAG, "summary_inv_node_y		7  ");

	}
	rtn =operate(buff, i);
//	ESP_LOGI(TAG, "summary_inv_node_y	    8  ");

	pdata2 = pdata + ((uint32_t)i * sizeof(pack_node_struct)) + offset;


//	ESP_LOGI(TAG, "summary_inv_node_y		9,Inv_can[0].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Inv_can[0].inv_data[0].inv_announce);
//	ESP_LOGI(TAG, "summary_inv_node_y		9,Inv_can[1].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Inv_can[1].inv_data[0].inv_announce);
//	ESP_LOGI(TAG, "summary_inv_node_y		9,Inv_can[2].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Inv_can[2].inv_data[0].inv_announce);
//
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata=0x%lx  ",(uint32_t)pdata);
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata2=0x%lx  ",(uint32_t)pdata2);
//
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,sizeof(inv_node_struct))=%ld  ",(uint32_t)sizeof(inv_node_struct));
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,i *sizeof(inv_node_struct))=%ld  ",(uint32_t)((uint32_t)i * sizeof(inv_node_struct)));
//	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)offset=%ld  ",(uint32_t)offset);
//
//	ESP_LOGI(TAG, "summary_inv_node_y	    9  ");
	memcpy(pdata2, (uint8_t *)&rtn, width);//尾部数组是汇总
	//windy debug
//	memcpy(pdata2, rtn, width);//尾部数组是汇总
//	ESP_LOGI(TAG, "summary_inv_node_y	    10  ");


	if(i != 0)
	{
		return rtn;
	}

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_announce( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;

//ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{

//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
		pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_announce ;	// 指针指向下一个inv节点数据
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");

		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");

	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");

	pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_announce  + ((uint32_t)INV_MAX_NUM * sizeof(inv_node_struct)) + offset;
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");

	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");

	if(i != 0)
	{
		return operate(buff, i);
	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_base( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_base + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
		pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_base;

		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
	}

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
	// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_base + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)INV_MAX_NUM * sizeof(inv_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].inv_data[INV_MAX_NUM].inv_base + offset;
	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总

	if(i != 0)
	{
		return operate(buff, i);
	}

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_pv( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_pv + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
		pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_pv;	// 指针指向下一个inv节点数据

		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
	}

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
	// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_pv + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)INV_MAX_NUM * sizeof(inv_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].inv_data[INV_MAX_NUM].inv_pv + offset;
	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总

	if(i != 0)
	{
		return operate(buff, i);
	}

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_grid( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_grid + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
		pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_grid;	// 指针指向下一个inv节点数据

		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
	}

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
	// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_grid + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)INV_MAX_NUM * sizeof(inv_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].inv_data[INV_MAX_NUM].inv_grid + offset;
	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总

	if(i != 0)
	{
		return operate(buff, i);
	}

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_load( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_load + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
		pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_load;	// 指针指向下一个inv节点数据

		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
	}
	rtn =operate(buff, i);

//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
	// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_load + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)INV_MAX_NUM * sizeof(inv_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].inv_data[INV_MAX_NUM].inv_load+ offset;	 /*+ ((uint16_t)INV_MAX_NUM * sizeof(inv_node_struct)) */
	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总

	if(i != 0)
	{
		return operate(buff, i);
	}

	return 0;
}
/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_data( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_data + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
		pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_data;	// 指针指向下一个inv节点数据

		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
	}

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
	// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_data + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)INV_MAX_NUM * sizeof(inv_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].inv_data[INV_MAX_NUM].inv_data + offset;
	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总

	if(i != 0)
	{
		return operate(buff, i);
	}

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_gen( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_gen + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
		pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_gen;	// 指针指向下一个inv节点数据

		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
	}

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
	// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_gen + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)INV_MAX_NUM * sizeof(inv_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].inv_data[INV_MAX_NUM].inv_gen + offset;
	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总

	if(i != 0)
	{
		return operate(buff, i);
	}

	return 0;
}

static uint32_t summary_inv_node(uint8_t *pdata, int offset, int width, data_operate operate)
{
	uint8_t idx = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX*INV_MAX_NUM];
	int cnt = 0;

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for (int i = 0; i < INV_MAX_NUM; i++)
		{
			uint8_t *p_data = pdata + ((cnt++) * sizeof(inv_node_struct));	// 指针指向下一个inv节点数据

			if (Inv_can[node_id].inv_data[i].online)
			{
				if(width == 1) {
					buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
				}
				else if(width == 2) {
					buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
				}
				else if(width == 4) {
					buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
				}
			}
		}
	}

	if(idx != 0) {
		return operate(buff, idx);
	}

	return 0;
}

static uint32_t summary_pack_node(uint8_t *pdata, int offset, int width, data_operate operate)
{
	uint8_t idx = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX*PACK_MAX_NUM];
	int cnt = 0;

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			uint8_t *p_data = pdata + ((cnt++) * sizeof(pack_node_struct));	// 指针指向下一个pack节点数据

			if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))
			{
				if(width == 1) {
					buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
				}
				else if(width == 2) {
					buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
				}
				else if(width == 4) {
					buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
				}
			}
		}
	}

	if(idx != 0) {
		return operate(buff, idx);
	}

	return 0;
}

static uint32_t summary_pack_announce(uint8_t *pdata, int offset, int width, data_operate operate)
{
	uint8_t idx = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX*PACK_MAX_NUM];
	int cnt = 0;

	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			uint8_t *p_data = pdata + ((cnt++) * sizeof(pack_node_struct));	// 指针指向下一个pack节点数据
			if (i != 0) continue;											// 只对主电池包的数据进行汇总

			if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))
			{
				if(width == 1) {
					buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
				}
				else if(width == 2) {
					buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
				}
				else if(width == 4) {
					buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
				}
			}
		}
	}

	if(idx != 0) {
		return operate(buff, idx);
	}

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_Pack_node_X_pack_announce( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;

//ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{

//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
		pdata = (uint8_t *)&Inv_can[i].pack_data[0].pack_announce ;	// 指针指向下一个inv节点数据
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");

		buff[i]= summary_Pack_node_y(i,pdata,offset,width, operate);
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");

	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");

	// pdata = (uint8_t *)&Inv_can[i].pack_data[0].pack_announce  + ((uint32_t)PACK_MAX_NUM * sizeof(pack_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].pack_data[INV_MAX_NUM].pack_announce + offset;
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");

	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");

	if(i != 0)
	{
		return operate(buff, i);
	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_Pack_node_X_pack_base( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;

//ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{

//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
		pdata = (uint8_t *)&Inv_can[i].pack_data[0].pack_base ;	// 指针指向下一个inv节点数据
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");

		buff[i]= summary_Pack_node_y(i,pdata,offset,width, operate);
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");

	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");

	// pdata = (uint8_t *)&Inv_can[i].pack_data[0].pack_base  + ((uint32_t)PACK_MAX_NUM * sizeof(pack_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].pack_data[INV_MAX_NUM].pack_base + offset;
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");

	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");

	if(i != 0)
	{
		return operate(buff, i);
	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");

	return 0;
}

/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_Pack_node_X_pack_extend( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[DEV_MAIN_NODE_MAX];
	uint8_t *pdata;
	uint32_t rtn = 0;

//ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
	for (i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{

//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
		pdata = (uint8_t *)&Inv_can[i].pack_data[0].pack_extend ;	// 指针指向下一个inv节点数据
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");

		buff[i]= summary_Pack_node_y(i,pdata,offset,width, operate);
//		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");

	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");

	rtn =operate(buff, i);
//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");

	// pdata = (uint8_t *)&Inv_can[i].pack_data[0].pack_extend  + ((uint32_t)PACK_MAX_NUM * sizeof(pack_node_struct)) + offset;
	pdata = (uint8_t *)&Inv_can[i].pack_data[INV_MAX_NUM].pack_extend + offset;
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");

	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");

	if(i != 0)
	{
		return operate(buff, i);
	}
//	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");

	return 0;
}




static uint32_t summary_pack_group_node(uint8_t node_id, uint8_t *pdata, int offset, int width, data_operate operate)
{
	uint8_t idx = 0;
	uint32_t buff[PACK_MAX_NUM];

	for (int i = 0; i < PACK_MAX_NUM; i++)
	{
		uint8_t *p_data = pdata + (i * sizeof(pack_node_struct));			// 指针指向下一个pack节点数据

		if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))
		{
			if(width == 1) {
				buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
			}
			else if(width == 2) {
				buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
			}
			else if(width == 4) {
				buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
			}
		}
	}

	if(idx != 0) {
		return operate(buff, idx);
	}

	return 0;
}

void data_sum_inv_announce(void)
{
	#undef INV_DATA_TYPE
	#define INV_DATA_TYPE	inv_announce_struct

	INV_DATA_TYPE inv_announce = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_announce;	// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].inv_data[0].inv_announce;
//ESP_LOGI(TAG, "data_sum_inv_announce	     1  ");

	int offset = offsetof(INV_DATA_TYPE, alarm[0]);
	int memsize = MEMSIZE(INV_DATA_TYPE, alarm[0]);
	int item_size = sizeof(inv_announce.alarm[0]);
	for (int i = 0; i < sizeof(inv_announce.alarm)/sizeof(inv_announce.alarm[0]); i++)
	{
		int _offset = offset + (i * item_size);
	summary_inv_node_X_inv_announce( _offset, memsize, data_or_operate);		// 汇总告警标志
	}
//	ESP_LOGI(TAG, "data_sum_inv_announce		4   ");

	offset = offsetof(INV_DATA_TYPE, fault[0]);
	memsize = MEMSIZE(INV_DATA_TYPE, fault[0]);
	item_size = sizeof(inv_announce.fault[0]);
	for (int i = 0; i < sizeof(inv_announce.fault)/sizeof(inv_announce.fault[0]); i++)
	{
		int _offset = offset + (i * item_size);
		summary_inv_node_X_inv_announce( _offset, memsize, data_or_operate);		// 汇总故障标志
	}
//	ESP_LOGI(TAG, "data_sum_inv_announce		5   ");

	uint16_t inv_online = 0;
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)								// 汇总逆变在线标志
	{
		for (int sub_node_id = 0; sub_node_id < INV_MAX_NUM; sub_node_id++)								// 汇总逆变在线标志
		{
			if (Inv_can[node_id].inv_data[sub_node_id].online)
			{
				inv_online |= (1 << (node_id*INV_MAX_NUM+sub_node_id));
			}
		}

	}
	inv_announce.online = inv_online;

	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce = inv_announce;										// 保存汇总数据

	summary_inv_node_X_inv_announce(										// 汇总状态标志
		 offsetof(INV_DATA_TYPE, status_flags), MEMSIZE(INV_DATA_TYPE, status_flags), data_or_operate);
//	return;//windy debug
//		 ESP_LOGI(TAG, "data_sum_inv_announce		2	");

	summary_inv_node_X_inv_announce(												// 汇总状态标志2
		 offsetof(INV_DATA_TYPE, inv_flag2), MEMSIZE(INV_DATA_TYPE, inv_flag2), data_or_operate);
//	ESP_LOGI(TAG, "data_sum_inv_announce		 3  ");

	summary_inv_node_X_inv_announce(												// 汇总待机功耗
		 offsetof(INV_DATA_TYPE, stdby_pwr_coms), MEMSIZE(INV_DATA_TYPE, stdby_pwr_coms), data_sum);


	summary_inv_node_X_inv_announce(											// 汇总总充电功率
		 offsetof(INV_DATA_TYPE, total_chg_power), MEMSIZE(INV_DATA_TYPE, total_chg_power), data_sum);
//	ESP_LOGI(TAG, "data_sum_inv_announce		 6  ");

	summary_inv_node_X_inv_announce(											// 汇总总放电功率
		 offsetof(INV_DATA_TYPE, total_dsg_power), MEMSIZE(INV_DATA_TYPE, total_dsg_power), data_sum);
//	ESP_LOGI(TAG, "data_sum_inv_announce		 7  ");

	summary_inv_node_X_inv_announce(														// 汇总故障标志5
		 offsetof(INV_DATA_TYPE, fault5), MEMSIZE(INV_DATA_TYPE, fault5), data_or_operate);
//ESP_LOGI(TAG, "data_sum_inv_announce		 8  ");
//	#define AC_METER_MASK_POS			11 		// AC电表异常位置
//	#define GRID_METER_MASK_POS			4 		// 电网电表异常位置
//	#define PARALLEL_EXP_MASK_POS		14 		// 并机状态异常位置
//	uint16_t alarm3 = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.alarm[2];
//	uint16_t alarm4 = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.alarm[3];
//	uint16_t fault4 = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.fault[3];
//
//	/* 当使能AC电表时才判断AC电表通信状态 */
//	alarm3 &= ~((uint16_t)1 << AC_METER_MASK_POS);
//	inv_advance_param_t *advance_param = ll_inv_param_get("inv_advance_param");
//	if (advance_param->ctrl_meter_enable)
//	{
////		if (ac_couple_meter_status_get() == AC_METER_STA_ERR) {
////			alarm3 |= ((uint16_t)1 << AC_METER_MASK_POS);
////		}
//	}
////	ESP_LOGI(TAG, "data_sum_inv_announce		 9	");
//
//	/* 电网电表状态检测 */
//	alarm4 &= ~((uint16_t)1 << GRID_METER_MASK_POS);
////	if (grid_meter_status_get() == GRID_METER_STA_ERR) {
////		alarm4 |= ((uint16_t)1 << GRID_METER_MASK_POS);
////	}
//
//	/* 离网并机使能状态检测 */
//	fault4 &= ~((uint16_t)1 << PARALLEL_EXP_MASK_POS);
//	if ((NULL!=  advance_param)
//		&&(advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1))						// 并机开关打开时才检测底层设备的并机开关状态
//	{
//		for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
//		{
//			for (int i = 0; i < INV_MAX_NUM; i++)
//			{
//				if (!Inv_can[node].inv_data[i].online) continue;
////				ESP_LOGI(TAG, "data_sum_inv_announce,node=%d,i=%d  ",node, i);
//
//				inv_set01_struct *inv_set01 = &Inv_can[node].inv_data[i].inv_set01;
////				ESP_LOGI(TAG, "BB  data_sum_inv_announce,node=%d,i=%d  ",node, i);
//
//				if ((inv_set01->ctrl_chg_max_volt == 0) &&
//					(inv_set01->ctrl_pv_max_curr == 0) &&
//					(inv_set01->ctrl_grid_max_power == 0) &&
//					(inv_set01->ctrl_feedback_max_power == 0) &&
//					(inv_set01->ctrl_Inv_addr_Set == 0)) continue;	// 判断逆变的数据是否有效
////				ESP_LOGI(TAG, "CC  data_sum_inv_announce,node=%d,i=%d  ",node, i);
//
//				if (inv_set01->ctrl_mix.bit.multi_inv_off_grid_en != 1)
//				{
//					fault4 |= ((uint16_t)1 << PARALLEL_EXP_MASK_POS);
//					break;
//				}
////				ESP_LOGI(TAG, "DD  data_sum_inv_announce,node=%d,i=%d  ",node, i);
//
//			}
//
//			if (fault4 & ((uint16_t)1 << PARALLEL_EXP_MASK_POS)) break;
////			ESP_LOGI(TAG, "EE  data_sum_inv_announce,node=%d  ",node);
//
//		}
//	}
////	ESP_LOGI(TAG, "data_sum_inv_announce		 10	");
//
//	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.alarm[2] = alarm3;	// 保存汇总了AC电表状态后的告警
//	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.alarm[3] = alarm4;	// 保存汇总了电网电表状态后的告警
//	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.fault[3] = fault4;	// 保存汇总了并网并联状态后的故障

	/* 故障调试代码,当相应开关打开后有效 */
//	extern int alarm_dbg;			// 告警调试开关
//	extern int fault_dbg;			// 故障调试开关
//	extern uint16_t dbg_alarm[4];	// 告警信息
//	extern uint16_t dbg_fault[4];	// 故障信息
//	extern uint16_t dbg_fault5;		// 故障信息
//	if (alarm_dbg) {
//		memcpy(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.alarm, dbg_alarm, sizeof(dbg_alarm));
//	}
//	if (fault_dbg) {
//		memcpy(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.fault, dbg_fault, sizeof(dbg_fault));
//		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.fault5 = dbg_fault5;
//	}
}
static uint16_t GetAbsoluteVal(uint32_t val1,uint16_t val2)
{
	uint16_t bigVal,smallVal ,val;
	if(val1>val2)
	{
		bigVal=val1;
		smallVal=val2;
	}else{
		bigVal=val2;
		smallVal=val1;
	}
	val=((bigVal-smallVal)*10)/(bigVal+1);
	ESP_LOGI(TAG,"GetAbsoluteVal:%u",val);
	return val;
}
/**
 * @brief 整数乘以 0.866（使用 Q16 定点数）
 * @param x 输入整数
 * @return x * 0.866 的近似整数值（截断）
 */
int multiply_by_0_866_fixed(int x) {
    const int factor = 56756; // 0.866 * 65536 (Q16 格式)
    return (x * factor) >> 16; // 右移 16 位恢复比例
}

uint8_t power_phase_convert(uint16_t grid_powerL1,uint16_t grid_powerL2,uint16_t load_powerL1,uint16_t load_powerL2)
{
	uint16_t rate_volt= (Inv_can[reals.online_X_inv_index].inv_data[0].inv_about.rated_Voltage)/2;
	uint8_t phase_type=Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.Phase_set&0xff;
	uint8_t volt_type=(Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.Phase_set>>8)&0xff;
	int16_t grid_angle= abs(Inv_can[reals.online_X_inv_index].inv_data[0].inv_grid.grid_angle);
	int32_t tmpWarm=0;
	uint8_t tempFlag=0;
	int16_t grid_power_cut=abs((int16_t)grid_powerL1-(int16_t)grid_powerL2);
	int16_t load_power_cut=abs((int16_t)load_powerL1-(int16_t)load_powerL2);
	//ESP_LOGI(TAG,"rate_volt:%u,phase_type:%d,volt_type:%d,grid_angle:%d,grid_power_cut:%hd,load_power_cut:%hd,charging_pile_en:%d",rate_volt,phase_type,volt_type,grid_angle,grid_power_cut,load_power_cut,Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.ctrl_mix2.charging_pile_en);
	//ESP_LOGI(TAG,"Volt_grid_L1:%u,Volt_grid_L2:%u",Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L1,Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L2);
	if((phase_type==2)&&(Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.ctrl_mix2.charging_pile_en!=1))
	{
		if(((Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L1/100)>rate_volt)&&((Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L2/100)>rate_volt)) //电压大于50V
		{
			if ((abs(grid_angle)>9000) && (abs(grid_angle)<15000))
			{
							//0.866
				tmpWarm = (grid_power_cut*10)/(grid_powerL1+1);
				ESP_LOGI(TAG,"tmpWarm:%ld",tmpWarm);
				if(tmpWarm <= 2)
				{
					tempFlag |= 1;
				}

				tmpWarm = (load_power_cut*10)/(load_powerL1+1);
				if(tmpWarm <= 2)
				{
					tempFlag |= 1;
				}
			}
		}else if((volt_type==2)&&((Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L1/100)<=rate_volt)&&((Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L2/100)<=rate_volt))
		{
			tmpWarm = (grid_power_cut*10)/(grid_powerL1+1);
			ESP_LOGI(TAG,"tmpWarm:%ld",tmpWarm);
			if(tmpWarm <= 2)
			{
				tempFlag |= 1;
			}

			tmpWarm = (load_power_cut*10)/(load_powerL1+1);
			if(tmpWarm <= 2)
			{
				tempFlag |= 1;
			}
		}
	}

	return tempFlag;
}

void data_sum_inv_base(void)
{
	#undef INV_DATA_TYPE
	#define INV_DATA_TYPE	inv_base_struct

	uint16_t line_event = 0;
	device_ctrl data_ctrl_status; //
	inv_status inv_statu;
	int found = 0;
	uint8_t i;
	data_ctrl_status.all=0;
	inv_statu.status=0;
	uint8_t inv_work_state=0;

	INV_DATA_TYPE inv_base = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base;			// 取在线设备数据
	uint8_t *pdata = (uint8_t *)&Inv_can[0].inv_data[0].inv_base;

	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_power_rang = inv_base.inv_power_rang;

	/* 汇总逆变告警与故障标志 */
	memcpy(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.alarm,  inv_base.alarm, sizeof(inv_base.alarm));
	memcpy(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.fault,  inv_base.fault,  sizeof(inv_base.fault));

	inv_base.inv_online = Inv_can[0].inv_data[0].inv_base.inv_online |
							Inv_can[1].inv_data[0].inv_base.inv_online |
							Inv_can[2].inv_data[0].inv_base.inv_online;

	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.online = inv_base.inv_online;				// 汇总逆变在线标志 0x10
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_online = inv_base.inv_online;				// 汇总逆变在线标志 0x11
	uint8_t inv_nums = 0;
	for (i = 0; i < DEV_MAIN_NODE_MAX*INV_MAX_NUM; i++)
	{
		if (inv_base.inv_online & (1 << i)) {
			inv_nums++;
		}
	}
	inv_base.inv_num = inv_nums;
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_num = inv_base.inv_num;  // 汇总逆变数量

	/* 汇总逆变工作状态与开关状态,以首台在线且处于并网运行的设备状态为准 */ // 仅以首台在线为准，底层参数会自动同步
	uint16_t switch_memory_state = 0;
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (i = 0; i < INV_MAX_NUM; i++)
		{
			if (Inv_can[node].inv_data[i].online)
			{
				inv_base_struct *p_inv_base = &Inv_can[node].inv_data[i].inv_base;
				inv_announce_struct *p_inv_announce=&Inv_can[node].inv_data[i].inv_announce;
				ESP_LOGI(TAG,"Inv_can[%d].inv_data[%d].inv_base.ctrl_status.all:0x%x",node,i,Inv_can[node].inv_data[i].inv_base.ctrl_status.all);
				// if (p_inv_base->inv_work_state >= 2)	// 并网运行 //testwx 底层参数会自动同步
				{
					ESP_LOGI(TAG,"p_inv_base->ctrl_status.all:%u",p_inv_base->ctrl_status.all);
					if(inv_work_state<p_inv_base->inv_work_state)
						inv_work_state=p_inv_base->inv_work_state;

					data_ctrl_status.all|= p_inv_base->ctrl_status.all;
					inv_statu.status|=p_inv_announce->status_flags.status;
					switch_memory_state |= p_inv_base->switch_memory_state; // 191 bit0:AC bit1:DC bit2:DCHUB
					//Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ctrl_status.all |= p_inv_base->ctrl_status.all;
					found = 1;
					//break;
				}
			}
		}

		//if (found) break;
	}
	if(found==1)
	{
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_work_state = inv_work_state;
		ESP_LOGI(TAG,"Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_work_state:%d",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_work_state);
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ctrl_status.all=data_ctrl_status.all;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.status_flags.status=inv_statu.status;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.switch_memory_state = switch_memory_state;
		//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,ctrl_status), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base + offsetof(inv_base_struct,ctrl_status),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ctrl_status));// 124
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.ctrl_status.all=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ctrl_status.all;
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.Inv_Flag.status=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.status_flags.status;
		Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.SwitchMemoryState = switch_memory_state; // 191
		//ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ctrl_status.all:0x%x",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ctrl_status.all);
	}

	summary_inv_node_X_inv_base(											// 汇总DC负载总功率
		offsetof(INV_DATA_TYPE, DCLoadAllTotalPower), MEMSIZE(INV_DATA_TYPE, DCLoadAllTotalPower), data_sum);

	summary_inv_node_X_inv_base(											// 汇总AC负载总功率
		offsetof(INV_DATA_TYPE, ACLoadAllTotalPower), MEMSIZE(INV_DATA_TYPE, ACLoadAllTotalPower), data_sum);

	summary_inv_node_X_inv_base(												// 汇总PV总功率
		offsetof(INV_DATA_TYPE, PVAllTotalPower), MEMSIZE(INV_DATA_TYPE, PVAllTotalPower), data_sum);

	summary_inv_node_X_inv_base(												// 汇总电网总功率
		offsetof(INV_DATA_TYPE, GridAllTotalPower), MEMSIZE(INV_DATA_TYPE, GridAllTotalPower), data_sum);

	summary_inv_node_X_inv_base(												// 汇总逆变总功率
		offsetof(INV_DATA_TYPE, InvAllTotalPower), MEMSIZE(INV_DATA_TYPE, InvAllTotalPower), data_sum);

	summary_inv_node_X_inv_base(												// 汇总DC负载总能量
		offsetof(INV_DATA_TYPE, DCLoadTotalEnergy), MEMSIZE(INV_DATA_TYPE, DCLoadTotalEnergy), data_sum);

	summary_inv_node_X_inv_base(												// 汇总AC负载总能量
		offsetof(INV_DATA_TYPE, ACLoadTotalEnergy), MEMSIZE(INV_DATA_TYPE, ACLoadTotalEnergy), data_sum);

	summary_inv_node_X_inv_base(											// 汇总PV总充电能量
		offsetof(INV_DATA_TYPE, PvTotalChargingEnergy), MEMSIZE(INV_DATA_TYPE, PvTotalChargingEnergy), data_sum);
	if(reals.online_ACHUB_num == 0) {
		summary_inv_node_X_inv_base(										// 汇总电网总充电能量
			offsetof(INV_DATA_TYPE, GridTotalChargingEnergy), MEMSIZE(INV_DATA_TYPE, GridTotalChargingEnergy), data_sum);
	}
	summary_inv_node_X_inv_base(													// 汇总馈电总能量
		offsetof(INV_DATA_TYPE, FeedbackEnergy), MEMSIZE(INV_DATA_TYPE, FeedbackEnergy), data_sum);

	summary_inv_node_X_inv_base(												// 汇总PV到AC负载的总能量
		offsetof(INV_DATA_TYPE, PvToACLoadEnergy), MEMSIZE(INV_DATA_TYPE, PvToACLoadEnergy), data_sum);

//test 接入achub并机汇总电网、负载显示
	ESP_LOGI(TAG,"0 Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower:%lu",Inv_can[0].inv_data[0].inv_base.PVAllTotalPower);
	ESP_LOGI(TAG,"1 Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower:%lu",Inv_can[1].inv_data[0].inv_base.PVAllTotalPower);
	ESP_LOGI(TAG,"2 Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower:%lu",Inv_can[2].inv_data[0].inv_base.PVAllTotalPower);
	ESP_LOGI(TAG,"3 Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower:%lu",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower);
	ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PVAllTotalPower:%lu",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PVAllTotalPower);
	Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.DCLoadAllTotalPower=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.DCLoadAllTotalPower;
	if(reals.online_ACHUB_num == 1)
	{
		if(reals.achub_grid_transfer_flag)//achub新版本程序，IOT直接透传ACHUB的能量和功率
		{
			Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.GridTotalChargingEnergy=Inv_can_mix.ac_hub_data[0].ac_hub_info.GridEnergy/100 ;
		}
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PVAllTotalPower), \
		(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base + offsetof(inv_base_struct,PVAllTotalPower),(offsetof(inv_base_struct,DCLoadTotalEnergy) - offsetof(inv_base_struct,PVAllTotalPower)));// 142~149
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PvToACLoadEnergy=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PvToACLoadEnergy;
		// Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.DCLoadTotalEnergy=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.DCLoadTotalEnergy;
		// Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.PvTotalChargingEnergy=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PvTotalChargingEnergy;
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.FeedbackEnergy=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.FeedbackEnergy;
		ESP_LOGI(TAG,"GridTotalChargingEnergy:%lu",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.GridTotalChargingEnergy);
	}else{
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1 + offsetof(MOD_STRUCT_reg00100,PVAllTotalPower), \
	 	(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base + offsetof(inv_base_struct,PVAllTotalPower),(offsetof(inv_base_struct,fault5) - offsetof(inv_base_struct,PVAllTotalPower)));// 142~159
	}



	uint8_t ac_phase_num=0;
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (i = 0; i < INV_MAX_NUM; i++)
		{
			if (Inv_can[node].inv_data[i].online)
			{
				if(ac_phase_num<Inv_can[node].inv_data[i].inv_load.ac_phase_number)
				{
					ac_phase_num=Inv_can[node].inv_data[i].inv_load.ac_phase_number;
				}
				//ESP_LOGI(TAG,"Inv_can[%d].inv_data[0].inv_base.ACLoadAllTotalPower:%lu",i,Inv_can[i].inv_data[0].inv_base.ACLoadAllTotalPower);
			}
			ESP_LOGI(TAG,"ac_phase_num:%d",ac_phase_num);
		}
	}
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_phase_number=ac_phase_num;
	Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_phase_number=ac_phase_num;
	ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ACLoadAllTotalPower:%lu",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ACLoadAllTotalPower);
	//三台DC输出功率
	/*
	for(uint8_t i = 0;i < 3;i++)//相数
	{
		ESP_LOGW(TAG, "HuangJi dc_hub_data[%d-%d]:%u", i,reals.online_Y_inv_index,Inv_can[i].dc_hub_data[0].dc_hub_info.output_power);
		ESP_LOGW(TAG, "HuangJi USB_A_output_power[%d-%d]:%u", i,reals.online_Y_inv_index,Inv_can[i].dc_hub_data[0].dc_hub_info.USB_A_output_power);
		ESP_LOGW(TAG, "HuangJi Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.DCLoadAllTotalPower:%u",( unsigned int)Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.DCLoadAllTotalPower);

	}
	*/
	//ESP_LOGW(TAG,"HuangJi data_sum_inv_base Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].output_power=0x%x ",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.output_power);
	//ESP_LOGW(TAG, "HuangJi reals.online_ACHUB_num:%d", reals.online_ACHUB_num);

	if(reals.online_ACHUB_num == 1)
	{
		uint32_t ParallelGridAllTotalPower = 0;  			//并机系统电网总功率
		uint32_t ParallelACLoadAllTotalPower = 0;			//并机系统AC负载总功率
		int16_t ParallelInvAllTotalPower = 0;				//并机逆变总功率
		uint32_t ParallelACLoadInvTotalPower = 0;			//并机逆变负载总功率(仅含逆变的负载，三相)
		uint16_t SingleGridAllTotalPower[3] = {0};  		//单相电网总功率
		uint32_t SingleACLoadAllTotalPower[3] = {0};		//单相AC负载总功率(仅含逆变的负载)
		uint16_t SingleACAllTotalPower[3] = {0};		  //单相AC总功率
		int16_t SingleInvAllTotalPower[3] = {0};			//单相逆变总功率
		int16_t achub_Power_load = 0;						//achub负载总功率
		int16_t temppower = 0;
		uint8_t power_208V_Flag=0;


		for(i = 0;i < 3;i++)
		{
			achub_Power_load += Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_load[i]; // 三相achub负载功率总和
		}

		// 单相三台逆变功率和; 单相三台逆变负载功率和
		for( i = 0;i < 3;i++)//相数
		{
			for(uint8_t j = 0;j < DEV_MAIN_NODE_MAX;j++)//逆变器数量
			{
				SingleInvAllTotalPower[i] += Inv_can[j].inv_data[0].inv_data.inv_detail[i].power;
				SingleACLoadAllTotalPower[i] += Inv_can[j].inv_data[0].inv_load.ac_load[i].load_power;
				//ESP_LOGI(TAG,"SingleACLoadAllTotalPower[%d]:%lu",i,SingleACLoadAllTotalPower[i]);
				//ESP_LOGI(TAG,"",j,i, Inv_can[j].inv_data[0].inv_data.inv_detail[i].power);
			}
		}
		//三台电网功率
		for( i = 0;i < 3;i++)//相数
		{
			//temppower =  Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_load[i]-SingleInvAllTotalPower[i];
			//SingleGridAllTotalPower[i] = temppower > 0 ? temppower : -temppower;
			//SingleGridAllTotalPower[i] = temppower > 0 ? temppower : 0;
			if(reals.achub_grid_transfer_flag)//achub新版本，并机电网功率通过ACHUB获取
			{
			 	temppower = Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_grid[i] ;
			}else{
				temppower = Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_load[i]-SingleInvAllTotalPower[i];
			}
			 SingleGridAllTotalPower[i] =temppower > 0 ? temppower : 0;
			 ESP_LOGI(TAG, "ac_hub_info.Power_load[%d]:%u", i,Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_load[i]);
			 ESP_LOGI(TAG,"SingleInvAllTotalPower[%d]:%u",i,SingleInvAllTotalPower[i]);
		   	 ESP_LOGI(TAG, "SingleGridAllTotalPower[%d]:%u", i,SingleGridAllTotalPower[i]);
			ParallelGridAllTotalPower+=SingleGridAllTotalPower[i] ;	//此处计算L1、L2、L3叠加的电网总功率
		}
		//三台AC功率
		for( i = 0;i < 3;i++)//相数
		{
			if(SingleInvAllTotalPower[i] < 0)//正值为放电
			{
				// 单相AC总功率 = 单相achub负载功率 + 逆变总的负载功率 - 单相逆变总功率(仅为正的时候，为负值时调整为0)
				SingleACAllTotalPower[i] = Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_load[i] + SingleACLoadAllTotalPower[i];
			}
			else
			{
				// 系统负载总功率 = 单相achub负载功率 + 单相AC负载总功率 - 单相逆变总功率(仅为正的时候，为负值时调整为0)
				temppower = Inv_can_mix.ac_hub_data[0].ac_hub_info.Power_load[i] + SingleACLoadAllTotalPower[i] - SingleInvAllTotalPower[i];
				SingleACAllTotalPower[i] = temppower > 0 ? temppower : 0;
			}
			ParallelACLoadAllTotalPower+=SingleACAllTotalPower[i];
			//ESP_LOGI(TAG,"SingleACAllTotalPower[%d]:%u,SingleInvAllTotalPower[%d]:%u",i,SingleACAllTotalPower[i],i,SingleInvAllTotalPower[i]);
		}
		//ESP_LOGI(TAG,"Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.Phase_set:%d",Inv_can[reals.online_X_inv_index].inv_data[0].inv_set01.Phase_set);
		ESP_LOGI(TAG,"ParallelACLoadAllTotalPower:%lu",ParallelACLoadAllTotalPower);

		// 三相三台逆变功率和; 三相三台逆变负载功率和
		for( i = 0;i < DEV_MAIN_NODE_MAX;i++)
		{
			ParallelInvAllTotalPower += SingleInvAllTotalPower[i];
			ParallelACLoadInvTotalPower += SingleACLoadAllTotalPower[i];
		}


		// 滤波处理
		if(ParallelGridAllTotalPower < 50)
		{
			ParallelGridAllTotalPower = 0;
		}

		if(ParallelACLoadAllTotalPower < 50)
		{
			ParallelACLoadAllTotalPower = 0;
		}
		if(power_phase_convert(SingleGridAllTotalPower[0],SingleGridAllTotalPower[1],SingleACAllTotalPower[0],SingleACAllTotalPower[1]))
		{
			ParallelGridAllTotalPower=multiply_by_0_866_fixed(ParallelGridAllTotalPower);
			ParallelACLoadAllTotalPower=multiply_by_0_866_fixed(ParallelACLoadAllTotalPower);
			ESP_LOGI(TAG,"0.866 convert: grid:%lu,load%lu",ParallelGridAllTotalPower,ParallelACLoadAllTotalPower);
		}
		if(Inv_can_mix.ac_hub_data[0].ac_hub_info.alarm_State.bit.GridWorkSta)
		{
			Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.GridAllTotalPower = ParallelGridAllTotalPower;// 146
			Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_power=ParallelGridAllTotalPower;	//1301
		}
		else
		{
			Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.GridAllTotalPower =0;
			Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_power=0;	//1301
		}
		//Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq=Inv_can[reals.online_X_inv_index].inv_data[0].inv_grid.freq;

		//ESP_LOGI(TAG,"AAA mod_reg01300_Inv_grid.freq:%u,total_chg_power:%lu",Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq,Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_power);


		//ESP_LOGI(TAG,"ParallelACLoadAllTotalPower:%lu",ParallelACLoadAllTotalPower);
		//Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.DCLoadAllTotalPower= ParallelDCLoadAllTotalPower;//140
		//AC 负载总功率
		if(is_ACLoadPowerUpdate())
		{
			Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ACLoadAllTotalPower = ParallelACLoadAllTotalPower;// 142
			Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_power=ParallelACLoadAllTotalPower;//1420
			//AC负载 L1 L2 L3 功率
			for( i = 0;i < DEV_MAIN_NODE_MAX;i++)
			{
				Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[i].load_power = SingleACAllTotalPower[i];// 1430 1436 1442
				ESP_LOGW(TAG, "HuangJi ac_load[%d]:%d, SingleACAllTotalPower[%d]:%u", i,Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[i].load_power,i,( unsigned int)SingleACAllTotalPower[i]);
			}
			ESP_LOGI(TAG,"HuangJi ACLoadAllTotalPower:%lu,total_ac_load_power:%lu",Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_power,Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ACLoadAllTotalPower);
			can_data_poll_mask_set(ACLOAD_UPDATE_MASK,0);
			ESP_LOGI(TAG,"Inv_can_mix.ac_hub_data[0].ac_hub_info.a0:%d",Inv_can_mix.ac_hub_data[0].ac_hub_info.alarm_State.bit.GridWorkSta);
		}
		for( i = 0;i < DEV_MAIN_NODE_MAX;i++)
		{
			ESP_LOGI(TAG,"GridWorkSta:%d",Inv_can_mix.ac_hub_data[0].ac_hub_info.alarm_State.bit.GridWorkSta);
			//电网L1 L2 L3 功率
			if(Inv_can_mix.ac_hub_data[0].ac_hub_info.alarm_State.bit.GridWorkSta)
			{
				Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_power = SingleGridAllTotalPower[i];// 1313
			}
			else
			{
				Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_power =0;
			}
			ESP_LOGI(TAG,"freq:%u,GridFrequency:%u",Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq,Inv_can_mix.ac_hub_data[0].ac_hub_info.GridFrequency);
			//Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq=100;
			ESP_LOGW(TAG, "HuangJi grid_load[%d]:%d, SingleGridAllTotalPower[%d]:%u", i,Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_power,i,( unsigned int)SingleGridAllTotalPower[i]);

		}

		Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[0].input_voltage=(Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L1/10);		//1314
		Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[1].input_voltage=(Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L2/10);		//1320
		Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[2].input_voltage=(Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L3/10);		//1326
		Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq=Inv_can_mix.ac_hub_data[0].ac_hub_info.GridFrequency;


	}else
	{
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.ACLoadAllTotalPower=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ACLoadAllTotalPower;
		Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_power=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ACLoadAllTotalPower;
		for(uint8_t i = 0;i < 3;i++)
		{
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[i].load_power, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_power,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[i].load_power));
			ESP_LOGI(TAG,"no achub Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[%d].load_power:%u",i,Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[i].load_power);
		}
		for(uint8_t i = 0;i < 3;i++)// 1313
		{
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_power,
				(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_power,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_power));
			memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[i].input_voltage,
				(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_voltage,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[i].input_voltage));
		}
		Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_power=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.total_chg_power;
		Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq=Inv_can[reals.online_X_inv_index].inv_data[0].inv_grid.freq;
//		Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_power=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.total_ac_load_power;// 1420~1423

	//	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq,
	//	(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.freq,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.freq));
	}
	// Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.GridAllTotalPower = 1917;// 146
	// Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.total_chg_power=1917;	//1301
	// Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[0].input_voltage=1201;		//1314
	// Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[1].input_voltage=1198;		//1320
	// //Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[2].input_voltage=(Inv_can_mix.ac_hub_data[0].ac_hub_info.Volt_grid_L3/10);		//1326
	// Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.freq=600;
	// Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[0].input_power=960;
	// Inv[DEV_MAIN_NODE_MAX].mod_reg01300_Inv_grid.grid_detail[1].input_power=957;

//	memcpy(inv_base.alarm, Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.alarm, sizeof(inv_base.alarm));		// 汇总告警标志
//	memcpy(inv_base.fault, Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.fault, sizeof(inv_base.fault));		// 汇总故障标志
//	inv_base.fault5 = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_announce.fault5;							// 汇总故障标志5
//	inv_base.SelfConsumptionPercent = energy_self_consumption_ratio_get();						// 获取自给率
	int offset = offsetof(INV_DATA_TYPE, alarm[0]);
	int memsize = MEMSIZE(INV_DATA_TYPE, alarm[0]);
	int item_size = sizeof(inv_base.alarm[0]);
	for (int i = 0; i < sizeof(inv_base.alarm)/sizeof(inv_base.alarm[0]); i++)
	{
		int _offset = offset + (i * item_size);
	summary_inv_node_X_inv_base( _offset, memsize, data_or_operate);		// 汇总告警标志
	}
//	ESP_LOGI(TAG, "data_sum_inv_announce		4	");

	offset = offsetof(INV_DATA_TYPE, fault[0]);
	memsize = MEMSIZE(INV_DATA_TYPE, fault[0]);
	item_size = sizeof(inv_base.fault[0]);
	for (int i = 0; i < sizeof(inv_base.fault)/sizeof(inv_base.fault[0]); i++)
	{
		int _offset = offset + (i * item_size);
		summary_inv_node_X_inv_base( _offset, memsize, data_or_operate);		// 汇总故障标志
	}

	summary_inv_node_X_inv_base(														// 汇总故障标志5
		 offsetof(INV_DATA_TYPE, fault5), MEMSIZE(INV_DATA_TYPE, fault5), data_or_operate);

	summary_inv_node_X_inv_base(												// 汇总PV到AC负载的总功率
	 offsetof(INV_DATA_TYPE, PVToACloadPower), MEMSIZE(INV_DATA_TYPE, PVToACloadPower), data_sum);

	// /* 能量线数据来自外部能量线计算模块 */
	// Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.line_event = line_event;									// 恢复能量线数据

	// 计算汇总的能量线 数据来源于0x11的轮询 123	Inv_can[node].inv_data[i].online
	for(int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (uint8_t i = 0; i < (INV_MAX_NUM); i++)
		{
			if (Inv_can[node].inv_data[i].online == 1)
			{
				// line_event |= Inv_can[node].inv_data[i].inv_line.line_event;
				line_event |= Inv_can[node].inv_data[i].inv_base.line_event;
			}
		}
	}
	static uint32_t count1=0;
	count1++;
	// Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_line.line_event = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_line.line_event;		// 汇总能量线
	// 待放入EMS能量线模块汇总给APP
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.line_event = line_event;		// 汇总能量线
	Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.line_event.all = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.line_event;// 123
	ESP_LOGI(TAG,"SUM IN BASE line_event:0x%x",Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.line_event.all);
}

void data_sum_inv_line(void)
{
	// uint16_t line_event = 0;

	// // 计算汇总的能量线 数据来源于0x11的轮询 123	Inv_can[node].inv_data[i].online
	// for(int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	// {
	// 	for (uint8_t i = 0; i < (INV_MAX_NUM); i++)
	// 	{
	// 		if (Inv_can[node].inv_data[i].online == 1)
	// 		{
	// 			line_event |= Inv_can[node].inv_data[i].inv_line.line_event;
	// 		}
	// 	}
	// }
	// // Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_line.line_event = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_line.line_event;		// 汇总能量线
	// Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_line.line_event = line_event;		// 汇总能量线
}

void data_sum_inv_about(void)
{
	judge_inv_min_version();// 判断每组系统里最小的arm、dsp、bms版本号，并将汇总后的最小版本号存入Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base，同时传递版本数量
	// 假设此处待汇总的数据已经过排序比较，
	//ESP_LOGI(TAG, "reals.online_X_inv_index == %d, reals.online_Y_inv_index == %d",reals.online_X_inv_index,reals.online_Y_inv_index);
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about;	// 取在线设备数据

	// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvType), (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.dev_type));// 1101~1106  ASCII 机型(单个)
	// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvSN), (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.dev_sn));// 1107~1110  设备唯一识别SN码(单个)

	// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,inv_power_rang), (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.inv_power_rang));// 1111(单机)

	// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,DevVoltageLable), (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,voltage_lable),sizeof(Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.voltage_lable));// 1149
	// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,WorkTimeNumber), (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,time_area_num),sizeof(Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.time_area_num));// 1148
	// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,software_total), (uint8_t *)&Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,software_total),sizeof(Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.software_total));// 1112



	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvType), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.dev_type));// 1101~1106  ASCII 机型(单个)
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvSN), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.dev_sn));// 1107~1110  设备唯一识别SN码(单个)

	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,inv_power_rang), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_power_rang));// 1111(单机)

	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,DevVoltageLable), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,voltage_lable),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.voltage_lable));// 1149
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,WorkTimeNumber), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about + offsetof(inv_about_struct,time_area_num),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_about.time_area_num));// 1148

}

void data_sum_inv_pv(void)
{
	memcpy(&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv,&Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_pv,sizeof(inv_pv_struct) );
	ESP_LOGI(TAG,"data_sum_inv_pv total_chg_power:%lu",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.total_chg_power);
}

void data_sum_inv_grid(void)
{
	#undef INV_DATA_TYPE
	#define INV_DATA_TYPE	inv_grid_struct

	INV_DATA_TYPE inv_grid = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_grid;			// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].inv_data[0].inv_grid;

	int offset = offsetof(INV_DATA_TYPE, grid_detail[0].input_power);
	int memsize = MEMSIZE(INV_DATA_TYPE, grid_detail[0].input_power);
	int item_size = sizeof(inv_grid.grid_detail[0].input_power) +
					sizeof(inv_grid.grid_detail[0].input_voltage) +
					sizeof(inv_grid.grid_detail[0].input_current);
	for (int i = 0; i < sizeof(inv_grid.grid_detail)/sizeof(inv_grid.grid_detail[0]); i++)
	{
		int _offset = offset + (i * item_size);
		summary_inv_node_X_inv_grid( _offset, memsize, data_sum);				// 汇总电网三相功率
		summary_inv_node_X_inv_grid( _offset+memsize, memsize, data_max);	// 汇总电网三相电压
		summary_inv_node_X_inv_grid( _offset+memsize*2, memsize, data_sum);	// 汇总电网三相电流
	}

	// for (int i = 0; i < sizeof(inv_grid.grid_detail)/sizeof(inv_grid.grid_detail[0]); i++)
	// {
	// 	if (inv_grid.grid_detail[i].input_power < 150) {		// 电网单相功率低于150W不显示
	// 		inv_grid.grid_detail[i].input_power = 0;
	// 	}

	// 	if (inv_grid.grid_detail[i].input_voltage < 300) {		// 电网单相电压低于30V不显示
	// 		inv_grid.grid_detail[i].input_voltage = 0;
	// 	}

	// 	if (inv_grid.grid_detail[i].input_current < 5) {		// 电网单相电流低于0.5A不显示
	// 		inv_grid.grid_detail[i].input_current = 0;
	// 	}
	// }

	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid = inv_grid;

	summary_inv_node_X_inv_grid(												// 汇总电网总功率
		 offsetof(INV_DATA_TYPE, total_chg_power), MEMSIZE(INV_DATA_TYPE, total_chg_power), data_sum);

	summary_inv_node_X_inv_grid(												// 汇总电网总充电能量
		 offsetof(INV_DATA_TYPE, total_chg_energy), MEMSIZE(INV_DATA_TYPE, total_chg_energy), data_sum);

	summary_inv_node_X_inv_grid(												// 汇总电网总馈电能量
		 offsetof(INV_DATA_TYPE, total_fb_energy), MEMSIZE(INV_DATA_TYPE, total_fb_energy), data_sum);
											// 保存汇总数据
}

void data_sum_inv_load(void)
{
	#undef INV_DATA_TYPE
	#define INV_DATA_TYPE	inv_load_struct

	INV_DATA_TYPE inv_load = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_load;			// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].inv_data[0].inv_load;

	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load = inv_load;

	int offset = offsetof(INV_DATA_TYPE, ac_load[0].load_power);
	int memsize = MEMSIZE(INV_DATA_TYPE, ac_load[0].load_power);
	int item_size = sizeof(inv_load.ac_load[0].load_power) +
					sizeof(inv_load.ac_load[0].load_voltage) +
					sizeof(inv_load.ac_load[0].load_current);
	for (int i = 0; i <sizeof(inv_load.ac_load)/sizeof(inv_load.ac_load[0]); i++)
	{
		int _offset = offset + (i * item_size);
		summary_inv_node_X_inv_load( _offset, memsize, data_sum);				// 汇总负载三相功率
		summary_inv_node_X_inv_load( _offset+memsize, memsize, data_max);		// 汇总负载三相电压
		summary_inv_node_X_inv_load( _offset+memsize*2, memsize, data_sum);	// 汇总负载三相电流
	}

	for (int i = 0; i < sizeof(inv_load.ac_load)/sizeof(inv_load.ac_load[0]); i++)
	{
		//ESP_LOGI(TAG,"inv_load.ac_load[%d].load_power:%u",i,inv_load.ac_load[i].load_power);
		//if (inv_load.ac_load[i].load_power < 20) {			// 电网单相功率低于20W不显示
			//inv_load.ac_load[i].load_power = 0;
		//}

		if (inv_load.ac_load[i].load_voltage < 300) {		// 电网单相电压低于30V不显示
			inv_load.ac_load[i].load_voltage = 0;
		}

		if (inv_load.ac_load[i].load_current < 5) {			// 电网单相电流低于0.5A不显示
			inv_load.ac_load[i].load_current = 0;
		}
	}

	summary_inv_node_X_inv_load(											// 汇总DC负载总功率
		 offsetof(INV_DATA_TYPE, total_dc_load_power), MEMSIZE(INV_DATA_TYPE, total_dc_load_power), data_sum);

	summary_inv_node_X_inv_load(											// 汇总DC负载总能量
		 offsetof(INV_DATA_TYPE, total_dc_load_energy), MEMSIZE(INV_DATA_TYPE, total_dc_load_energy), data_sum);

	summary_inv_node_X_inv_load(												// 汇总5V直流负载功率
		 offsetof(INV_DATA_TYPE, dc_05v_load_power), MEMSIZE(INV_DATA_TYPE, dc_05v_load_power), data_sum);

	summary_inv_node_X_inv_load(											// 汇总5V直流负载电流
		 offsetof(INV_DATA_TYPE, dc_05v_load_current), MEMSIZE(INV_DATA_TYPE, dc_05v_load_current), data_sum);

	summary_inv_node_X_inv_load(												// 汇总12V直流负载功率
		 offsetof(INV_DATA_TYPE, dc_12v_load_power), MEMSIZE(INV_DATA_TYPE, dc_12v_load_power), data_sum);

	summary_inv_node_X_inv_load(											// 汇总12V直流负载电流
		 offsetof(INV_DATA_TYPE, dc_12v_load_current), MEMSIZE(INV_DATA_TYPE, dc_12v_load_current), data_sum);

	summary_inv_node_X_inv_load(												// 汇总24V直流负载功率
		 offsetof(INV_DATA_TYPE, dc_24v_load_power), MEMSIZE(INV_DATA_TYPE, dc_24v_load_power), data_sum);

	summary_inv_node_X_inv_load(											// 汇总24V直流负载电流
		 offsetof(INV_DATA_TYPE, dc_24v_load_current), MEMSIZE(INV_DATA_TYPE, dc_24v_load_current), data_sum);

	summary_inv_node_X_inv_load(											// 汇总AC负载总功率
		 offsetof(INV_DATA_TYPE, total_ac_load_power), MEMSIZE(INV_DATA_TYPE, total_ac_load_power), data_sum);

	summary_inv_node_X_inv_load(											// 汇总AC负载总能量
		 offsetof(INV_DATA_TYPE, total_ac_load_energy), MEMSIZE(INV_DATA_TYPE, total_ac_load_energy), data_sum);
												// 保存汇总数据
	for (uint8_t i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		ESP_LOGI(TAG,"Inv_can[%d].inv_data[0].inv_load.total_ac_load_power:%lu",i,Inv_can[i].inv_data[0].inv_load.total_ac_load_power);
	}
	ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_power:%lu",Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_power);
	ESP_LOGI(TAG,"Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.total_ac_load_volt1:%u  lvolt2:%u",Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[0].load_voltage,Inv[DEV_MAIN_NODE_MAX].mod_reg01400_Inv_load.ac_load[1].load_voltage);
}

void data_sum_inv_data(void)
{
	#undef INV_DATA_TYPE
	#define INV_DATA_TYPE	inv_data_struct

	INV_DATA_TYPE inv_data = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_data;		// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].inv_data[0].inv_data;

	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_data = inv_data;
	int offset = offsetof(INV_DATA_TYPE, freq);
	summary_inv_node_X_inv_data( offset, sizeof(inv_data.freq), data_max);

	offset = offsetof(INV_DATA_TYPE, inv_detail[0].work_status);
	for (int i = 0; i < sizeof(inv_data.inv_detail)/sizeof(inv_data.inv_detail[0]); i++)		// 汇总逆变详细信息
	{
		// inv_data.inv_detail[i].work_status = summary_inv_node(pdata, offset, sizeof(inv_data.inv_detail[0].work_status), data_sum);
		summary_inv_node_X_inv_data( offset+=sizeof(inv_data.inv_detail[0].work_status), sizeof(inv_data.inv_detail[0].power), data_sum);
		summary_inv_node_X_inv_data( offset+=sizeof(inv_data.inv_detail[0].power), sizeof(inv_data.inv_detail[0].voltage), data_max);
		summary_inv_node_X_inv_data( offset+=sizeof(inv_data.inv_detail[0].voltage), sizeof(inv_data.inv_detail[0].current), data_sum);
		offset += sizeof(inv_data.inv_detail[0].current);
	}


	summary_inv_node_X_inv_data(													// 汇总逆变总能量
		 offsetof(INV_DATA_TYPE, total_energy), MEMSIZE(INV_DATA_TYPE, total_energy), data_sum);
	// ESP_LOGI(TAG,"Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_data. is %d");
}

void data_sum_inv_meter(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_meter = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_meter;	// 取在线设备数据
}

void data_sum_inv_gen(void)
{
	#undef INV_DATA_TYPE
	#define INV_DATA_TYPE	inv_gen_struct

	INV_DATA_TYPE inv_gen = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_gen;					// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].inv_data[0].inv_gen;

	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_gen = inv_gen;

	summary_inv_node_X_inv_gen(													// 汇总发电机总能量
		 offsetof(INV_DATA_TYPE, total_energy), MEMSIZE(INV_DATA_TYPE, total_energy), data_sum);

	summary_inv_node_X_inv_gen(															// 汇总发电机总功率
		 offsetof(INV_DATA_TYPE, power), MEMSIZE(INV_DATA_TYPE, power), data_sum);

	summary_inv_node_X_inv_gen(															// 汇总发电机电压
		 offsetof(INV_DATA_TYPE, voltage), MEMSIZE(INV_DATA_TYPE, voltage), data_max);

	summary_inv_node_X_inv_gen(															// 汇总发电机电流
		 offsetof(INV_DATA_TYPE, current), MEMSIZE(INV_DATA_TYPE, current), data_sum);
											// 保存汇总数据
}

uint16_t process_ac1_to_ac4_status(uint16_t set_AC_branch_tmp) {
	uint16_t ac_status = 0;
	for (int i = 0; i < 4; i++) {
		uint16_t ac = (set_AC_branch_tmp >> (i * 2)) & 0x0003;
		if (ac != 2) { // 开关不是全关，则可能有开的状态
			ac &= 1; // 如果是0这里不影响
		}
		ac_status |= (ac << (i * 2));
	}
	return ac_status;
}

void data_sum_inv_base_param(void)
{

	int i;
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set00;	// 取在线设备数据

	// modbus 2000, 1A段汇总 Inv_can -> Inv
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,mon), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,time1),(offsetof(inv_set00_struct,work_mode) - offsetof(inv_set00_struct,time1)));// 2000~2004
	Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.SetTimeZone.all =SetData.dev_info_t.SetTimeZone.all;// SetData_Can.dev_info_t2.inv_set00.res;	// 2004
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,work_mode), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,work_mode),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode));// 2005
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl));// 2006
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_led), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_led),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_led));// 2007
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_meter), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_meter),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_meter));// 2008
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_pv), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_pv),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_pv));// 2009
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_inv),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_inv));// 2010
	//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_ac),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_ac));// 2011
	//memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dc),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_dc));// 2012
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_poweron), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_poweron),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_poweron));// 2013
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_dc_eco));// 2014
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco_time), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco_time),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_dc_eco_time));// 2015
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_dc_power_value), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,eco_dc_power_value),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.eco_dc_power_value));// 2016
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_ac_eco));// 2017
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco_time), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco_time),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_ac_eco_time));// 2018
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_ac_power_value), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,eco_ac_power_value),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.eco_ac_power_value));// 2019
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_chg_mode), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_chg_mode),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_chg_mode));// 2020
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_super_power), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_super_power),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_super_power));// 2021
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_low_cap_pct), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_low_cap_pct),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_low_cap_pct));// 2022
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_high_cap_pct), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_high_cap_pct),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_high_cap_pct));// 2023
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv_mode), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_inv_mode),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_inv_mode));// 2024
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dev_id), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dev_id),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_dev_id));// 2025
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_all_energy_type), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_all_energy_type),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_all_energy_type));// 2026
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_now_energy_type), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_now_energy_type),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_now_energy_type));// 2027
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_log_page), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_log_page),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_log_page));// 2028
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time_area), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_time_area),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_time_area));// 2029
	for(int i = 0;i < 10;i++)// 2030~2059
	{
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_time[i].lable, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_time[i].lable,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_time[i].lable));
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_time[i].start, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_time[i].start,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_time[i].start));
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_time[i].end, (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_time[i].end,sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_time[i].end));
	}
	for(int i = 0;i < 6;i++)// 2060~2065
	{
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_PvType[i], (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_PvType[i],sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_PvType[i]));
	}
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_alarm_voice), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_alarm_voice),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_alarm_voice));// 2066
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_lcd_active_time), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,setLcdActiveTime),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.setLcdActiveTime));// 2067
	// 2068~2071 对380来讲没有
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,self_config),
		(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,self_config),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.self_config));// 2072
	// memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,on_off_set),
	// 	(uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,on_off_set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.on_off_set));// 2073

	/* 2073~2076*/
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,remoteSet), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,remoteSet),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[0].inv_set00.remoteSet));// 2073
	Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.remoteSoc=Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.remoteSoc;	//2074
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ownerShip), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ownerShip),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[0].inv_set00.ownerShip));// 2075
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,LevelSwitch), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,LevelSwitch),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[0].inv_set00.LevelSwitch));// 2076
	//ESP_LOGI(TAG,"data_sum_inv_base_param self_config.all:0x%x ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.self_config.all);
	//ESP_LOGI(TAG,"data_sum_inv_base_param LevelSwitch.bit.level:%d ",Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.LevelSwitch.bit.level);
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,sleepRemainTime), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,sleepRemainTime),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.sleepRemainTime));// 2077
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ledColorSet), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ledColorSet),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ledColorSet));// 2078
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,remote_set_power), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,remote_set_power),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.remote_set_power));// 2079
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,pack_set_show), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,pack_set_show),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.pack_set_show));// 2080
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,inv_set_show), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,inv_set_show),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.inv_set_show));// 2081
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,dcdc_set_show), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,dcdc_set_show),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.dcdc_set_show));// 2082
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,soc_max_ownership_set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,soc_max_ownership_set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.soc_max_ownership_set));// 2083
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,pv_senior_set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,pv_senior_set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.pv_senior_set));// 2084
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DC_output), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,dc_output),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.dc_output));// 2085
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Regulatory_set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,Regulatory_set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Regulatory_set));// 2086
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_capacity), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,Cycle_capacity),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Cycle_capacity));// 2087
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_max_capacity), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,Cycle_max_capacity),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Cycle_max_capacity));// 2088
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ym), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,Effective_time_mon),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Effective_time_mon) + sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Effective_time_year));// 2089
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_dh), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,Effective_time_hour),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Effective_time_hour) + sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Effective_time_day));// 2090
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ms), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,Effective_time_sec),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Effective_time_sec) + sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.Effective_time_min));// 2091
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ECO_status), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ECO_status),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ECO_status));// 2092

	inv_advance_param_t *advance_param = &SetData_Can.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
	if ((NULL!=  advance_param))
		// &&(advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1))						// 并机开关打开时才检测底层设备的AC状态
	{
		uint8_t ctrl_ac_tmp = 0;
		uint8_t ctrl_dc_tmp = 0;
		uint16_t set_AC_branch_tmp = 0;
		for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
		{
			for (i = 0; i < INV_MAX_NUM; i++)
			{
				inv_set00_struct *inv_set00 = &Inv_can[node].inv_data[i].inv_set00;
				//ESP_LOGI(TAG,"000---Inv_can[%d].inv_data[%d].online:%d",node,i,Inv_can[node].inv_data[i].online);
				/* 并机模式下,只要系统中有一台逆变的系统总开关是关闭的,则设置整个系统的总开关状态是关闭的 */
				if (Inv_can[node].inv_data[i].online)
				{
					ctrl_ac_tmp |= inv_set00->ctrl_ac;
					set_AC_branch_tmp |= inv_set00->set_AC_branch;
					ESP_LOGI(TAG,"000---Inv_can[%d].inv_data[%d].inv_set00.ctrl_ac:%d",node,i,Inv_can[node].inv_data[i].inv_set00.ctrl_ac);
					ESP_LOGI(TAG,"000---Inv_can[%d].inv_data[%d].inv_set00.set_AC_branch:%d",node,i,Inv_can[node].inv_data[i].inv_set00.set_AC_branch);
				}
			}
			for (i = 0; i < INV_MAX_NUM; i++)
			{
				inv_set00_struct *inv_set00 = &Inv_can[node].inv_data[i].inv_set00;
				// 这里只有设备类型为AP300时才进行dchub的在线判断处理
				if (strcmp(SetData.dev_info_t.INV_dev_type, IOT_TYPE_AP300) == 0)
				{
					if (Inv_can[node].dc_hub_data[i].online)//ac380本身没有dc开关，该dc开关控制dchub
					{
						ctrl_dc_tmp |= inv_set00->ctrl_dc;
					}
				} else {
					ctrl_dc_tmp |= inv_set00->ctrl_dc;
				}
				ESP_LOGI(TAG,"node:%d,online0:%d,online1:%d",node,Inv_can[node].dc_hub_data[0].online,Inv_can[node].dc_hub_data[1].online);

				ESP_LOGI(TAG,"node:%d,inv_set00->ctrl_dc:%d,ctrl_dc_tmp:%d",node,inv_set00->ctrl_dc,ctrl_dc_tmp);
			}
		}

	//__exit:
		ESP_LOGI(TAG,"ctrl_ac_tmp:%d ctrl_ac:%d",ctrl_ac_tmp,Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.ctrl_ac);
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_ac = ctrl_ac_tmp;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_dc = ctrl_dc_tmp;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode=Inv_can[reals.online_X_inv_index].inv_data[0].inv_set00.work_mode;
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_ac),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_ac));// 2011
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dc),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.ctrl_dc));// 2012
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,work_mode), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,work_mode),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.work_mode));// 2005
		ESP_LOGI(TAG,"set_AC_branch_tmp:%x set_AC_branch:%x",set_AC_branch_tmp,Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set.set_AC_branch);
		set_AC_branch_tmp = process_ac1_to_ac4_status(set_AC_branch_tmp);
		ESP_LOGI(TAG,"after process: set_AC_branch_tmp:%x",set_AC_branch_tmp);
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set00.set_AC_branch = set_AC_branch_tmp;
		memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_AC_branch), (uint8_t *)&set_AC_branch_tmp,sizeof(set_AC_branch_tmp));// 2093
	}
}

void data_sum_inv_advance_param(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set01;	// 取在线设备数据

	// 汇总DCHUB设置: 如果HUB接在从机, 则遍历所有节点, 取第一个在线HUB的设置项到汇总数据
	for (int i = 0; i < DEV_MAIN_NODE_MAX; i++)
	{
		if (reals.online_DCHUB_bit & (1 << i))
		{
			Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.DCHUB_set = Inv_can[i].inv_data[reals.online_Y_inv_index].inv_set01.DCHUB_set;
			break;
		}
	}
	// modbus 2200, 1B段汇总
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,password), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,password),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.password));// 2200~2203
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_reset_factory), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_reset_factory),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_reset_factory));// 2206
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_grid),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_grid));// 2207
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_feedback), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_feedback),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_feedback));// 2208
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_output_inv_volt), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_output_inv_volt),(offsetof(inv_set01_struct,ctrl_user_area) - offsetof(inv_set01_struct,ctrl_output_inv_volt)));// 2209~2217汇总
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_user_area), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_user_area),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_user_area));// 2218
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_pv_paralle));// 2219~2224
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid_plus), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_grid_plus),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_grid_plus));// 2225汇总
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_save_power_state), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_save_power_state),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_save_power_state));// 2226
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_enable), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_meter_enable),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_meter_enable));// 2227
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_select), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_meter_select),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_meter_select));// 2228
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_Multi_enable), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_Multi_enable),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_Inv_Multi_enable));// 2229
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_addr_Set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_Inv_addr_Set));// 2230
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_test), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ct_test),(offsetof(inv_set01_struct,ctrl_mix2) - offsetof(inv_set01_struct,ct_test)));// 2231~2242
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_mix2), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_mix2),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ctrl_mix2));// 2242
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ChargingPile_SET), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ChargingPile_SET),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ChargingPile_SET));// 2243
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_ratio), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ct_ratio),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ct_ratio));// 2244
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,GenSet), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,GenSet),(offsetof(inv_set01_struct,Undervoltage_protection) - offsetof(inv_set01_struct,GenSet)));// 2246~2257	res1
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Undervoltage_protection));// 2258
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection_time), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection_time),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Undervoltage_protection_time));// 2259
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Highvoltage_protection));// 2260
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection_time), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection_time),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Highvoltage_protection_time));// 2261
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Underfrequency_protection));// 2262
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection_time), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection_time),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Underfrequency_protection_time));// 2263
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Overvoltage_protection));// 2264
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection_time), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection_time),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Overvoltage_protection_time));// 2265
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetCtrlPv), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,setting_pv),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.setting_pv));// 2269
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Phase_set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Phase_set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Phase_set));// 2270
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,DCHUB_set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,DCHUB_set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.DCHUB_set));// 2271
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetGridMaxCurrent_in), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,SetGridMaxCurrent_in),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.SetGridMaxCurrent_in));// 2272
	memcpy((uint8_t *)&Inv[DEV_MAIN_NODE_MAX].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Func_Set), (uint8_t *)&Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Func_Set),sizeof(Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Func_Set));// 2273
	ESP_LOGI(TAG,"Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Phase_set:0x%x",Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.Phase_set);

}

void data_sum_inv_auth_param(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set03 = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set03;	// 取在线设备数据
}

void data_sum_inv_log(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_log = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_log;		// 取在线设备数据
}

void data_sum_inv_year_energy(void)
{
	// 由IOT计算,不再根据底层设备的数据汇总
}

void data_sum_inv_now_energy(void)
{
	// 由IOT计算,不再根据底层设备的数据汇总
}

void data_sum_inv_wifi(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_wifi = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_wifi;	// 取在线设备数据
}

void data_sum_inv_wifi_report(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].wifi_report = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].wifi_report;
}

void data_sum_inv_wifi_auth_param(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].wifi_param = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].wifi_param;
}

void data_sum_inv_auth_param_27H(void)// auth_param
{
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].auth_param = Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].auth_param;
}

void data_sum_inv_dc_hub_info(void)
{
	dc_hub_info_struct uDc_Hub_Infos;
	uint8_t i;
	memset(&uDc_Hub_Infos,0,sizeof(uDc_Hub_Infos));
	if(!reals.online_DCHUB_num)
		return ;
	for(i=0;i<DEV_MAIN_NODE_MAX;i++)
	{
		if(reals.online_DCHUB_bit&(1<<i))
		{
			memcpy(&uDc_Hub_Infos.dc_hub_type,&Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_type,sizeof(uDc_Hub_Infos.dc_hub_type));
			memcpy(&uDc_Hub_Infos.dc_hub_sn,&Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_type,sizeof(uDc_Hub_Infos.dc_hub_sn));

			uDc_Hub_Infos.input_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.input_voltage;  // 输入电压 0.1V 15711
			uDc_Hub_Infos.output_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.output_voltage;  // 输出电压 0.1V  15714
			uDc_Hub_Infos.cig1_output_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.cig1_output_voltage;  // 点烟器1输出电压 0.1V	15717
			uDc_Hub_Infos.cig2_output_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.cig2_output_voltage;  // 点烟器2输出电压 0.1V 15720
			uDc_Hub_Infos.USB_A_output_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.USB_A_output_voltage;  // USB-A输出电压 0.1V 15723
			uDc_Hub_Infos.Type_C_output_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C_output_voltage;  // Type-C输出电压 0.1V 15726
			uDc_Hub_Infos.Type_C2_output_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C2_output_voltage;  // Type-C2输出电压 0.1V 15729
			uDc_Hub_Infos.Anderson_output_voltage = Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Anderson_output_voltage;  // 安德森输出电压 0.1V 15732
			break;
		}
	}

	for(i=0;i<DEV_MAIN_NODE_MAX;i++)
	{
		if(reals.online_DCHUB_bit&(1<<i))
		{
			Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].devId=Inv_can[i].dc_hub_data[reals.online_Y_inv_index].devId;
			Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].online=Inv_can[i].dc_hub_data[reals.online_Y_inv_index].online;
			uDc_Hub_Infos.input_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.input_power;  // 输入功率 1w 15710
			uDc_Hub_Infos.input_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.input_current;  // 输入电流 0.01A 15712
			uDc_Hub_Infos.output_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.output_power;  // 输出总功率 1w 15713
			uDc_Hub_Infos.output_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.output_current;  // 输出电流 0.01A 15715
			uDc_Hub_Infos.cig1_output_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.cig1_output_power;  // 点烟器1输出总功率 1w 15716
			uDc_Hub_Infos.cig1_output_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.cig1_output_current;  // 点烟器1输出电流 0.01A 15718
			uDc_Hub_Infos.cig2_output_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.cig2_output_power;  // 点烟器2输出总功率 1w 15719
			uDc_Hub_Infos.cig2_output_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.cig2_output_current;  // 点烟器2输出电流 0.01A	15721
			uDc_Hub_Infos.USB_A_output_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.USB_A_output_power;  // USB-A输出功率 1w 15722
			uDc_Hub_Infos.USB_A_output_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.USB_A_output_current;  // USB-A输出电流 0.01A	15724
			uDc_Hub_Infos.Type_C_output_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C_output_power;  // Type-C输出功率 1w 15725
			uDc_Hub_Infos.Type_C_output_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C_output_current;  // Type-C输出电流 0.01A 15727
			uDc_Hub_Infos.Type_C2_output_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C2_output_power;  // Type-C2输出功率 1w 15728
			uDc_Hub_Infos.Type_C2_output_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C2_output_current;  // Type-C2输出电流 0.01A 15730
			uDc_Hub_Infos.Anderson_output_power += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Anderson_output_power;  // 安德森输出功率 1w 15731
			uDc_Hub_Infos.Anderson_output_current += Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Anderson_output_current;  // 安德森输出电流 0.01A	15733
			ESP_LOGI(TAG,"HuangJi Inv_can[%d].dc_hub_data[%d].dc_hub_info.cig2_output_power=0x%x ",i,reals.online_Y_inv_index,Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.cig2_output_power);
			//ESP_LOGI(TAG,"HuangJi Inv_can[%d].dc_hub_data[%d].dc_hub_info.USB_A_output_power=0x%x ",i,reals.online_Y_inv_index,Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.USB_A_output_power);
			//ESP_LOGI(TAG,"HuangJi Inv_can[%d].dc_hub_data[%d].dc_hub_info.Type_C2_output_power=0x%x ",i,reals.online_Y_inv_index,Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C2_output_power);
			//ESP_LOGI(TAG,"HuangJi Inv_can[%d].dc_hub_data[%d].dc_hub_info.Type_C_output_power=0x%x ",i,reals.online_Y_inv_index,Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.Type_C_output_power);
			//ESP_LOGI(TAG,"HuangJi data_sum_inv_dc_hub_info");
			ESP_LOG_BUFFER_HEX(TAG, (char *)&Inv_can[i].dc_hub_data[0].dc_hub_info,68);
		}
	}
	memcpy(&Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info,&uDc_Hub_Infos,sizeof(dc_hub_info_struct));
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].cig1_output_output_power=0x%x ",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.output_power);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].cig2_output_power=0x%x ,cig2_output_current:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.cig2_output_current);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].USB_A_output_power=0x%x ,USB_A_output_current:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.USB_A_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.USB_A_output_current);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].Anderson_output_power=0x%x ,Anderson_output_current:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Anderson_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Anderson_output_current);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].Type_C1_output_power=0x%x ,Type_C1_output_current:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C_output_current);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].Type_C2_output_power=0x%x ,Type_C2_output_current:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C2_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C2_output_current);
	//Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info = Inv_can[reals.online_X_inv_index].dc_hub_data[reals.online_Y_inv_index].dc_hub_info;
		//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].cig1_output_output_power=0x%x ",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.output_power);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].cig2_output_power=0x%x ,cig2_output_voltage:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.cig2_output_voltage);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].USB_A_output_power=0x%x ,USB_A_output_voltaget:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.USB_A_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.USB_A_output_voltage);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].Anderson_output_power=0x%x ,Anderson_output_voltage:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Anderson_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Anderson_output_voltage);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].Type_C1_output_power=0x%x ,Type_C1_output_voltage:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C_output_voltage);
	//ESP_LOGW(TAG,"HuangJi Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].Type_C2_output_power=0x%x ,Type_C2_output_voltage:0x%x",Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C2_output_power,Inv_can[DEV_MAIN_NODE_MAX].dc_hub_data[INV_MAX_NUM].dc_hub_info.Type_C2_output_voltage);
}

void data_sum_inv_ac_hub_info(void)
{
	Inv_can_mix.ac_hub_data[0].ac_hub_info = Inv_can_mix.ac_hub_data[reals.online_Y_inv_index].ac_hub_info;
}

void data_sum_inv_d400s_info(void)
{
	d400s_common_info_struct uD400s_Common_Info;
	d400s_charger_set_struct   uD400s_Charger_Set;
	uint8_t i;
	memset(&uD400s_Common_Info,0,sizeof(uD400s_Common_Info));
	memset(&uD400s_Charger_Set,0,sizeof(uD400s_Charger_Set));

	for(i=0;i<DEV_MAIN_NODE_MAX;i++)
	{
		if(reals.online_D400S_bit&(1<<i))
		{
			memcpy(&uD400s_Common_Info.d400s_type,&Inv_can[i].d400s_data[reals.online_Y_inv_index].d400s_common_info.d400s_type,sizeof(uD400s_Common_Info.d400s_type));
			memcpy(&uD400s_Common_Info.d400s_sn,&Inv_can[i].d400s_data[reals.online_Y_inv_index].d400s_common_info.d400s_sn,sizeof(uD400s_Common_Info.d400s_sn));
			uD400s_Common_Info.battery_type=Inv_can[i].d400s_data[reals.online_Y_inv_index].d400s_common_info.battery_type;
			for(int j=0;j<6;j++)
			{
				uD400s_Common_Info.dc_info[j].dc_voltage = Inv_can[i].d400s_data[reals.online_Y_inv_index].d400s_common_info.dc_info[j].dc_voltage;
			}
			break;
		}
	}

	//memcpy(&Inv_can[DEV_MAIN_NODE_MAX].d400s_data[INV_MAX_NUM].d400s_common_info,&Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info,sizeof(Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_common_info));
	//memcpy(&Inv_can[DEV_MAIN_NODE_MAX].d400s_data[INV_MAX_NUM].d400s_charger_set,&Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_charger_set,sizeof(Inv_can[reals.online_X_inv_index].d400s_data[reals.online_Y_inv_index].d400s_charger_set));
}

void data_sum_inv_ota_state(void)
{
	ESP_LOGI(TAG,"Inv_can[%d].inv_data[%d].inv_base.ctrl_status.bit.remote_sleep_on:%d",reals.online_X_inv_index,reals.online_Y_inv_index,Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.ctrl_status.bit.remote_sleep_on);

	if (reals.IOT_Status_Flag.sBit.system_sleep_flag) {
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state = 1;
		iot_sleep_ctx_restore_mod_reg00100(&Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1);
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Parts_online.bit.remote_off_en = 1;	// 休眠期间保持支持远程关机标志，避免CAN离线清零后App不显示休眠
	} else {
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Iot_State.bit.remote_off_state =
			Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.ctrl_status.bit.remote_sleep_on;
		Inv[DEV_MAIN_NODE_MAX].mod_reg00100_AppPage1.Parts_online.bit.remote_off_en=Inv_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set00.remoteSet.bit.remoteOffCtrlEn;
	}

}
void data_sum_pack_announce(void)
{
	#undef PACK_DATA_TYPE
	#define PACK_DATA_TYPE	pack_announce_struct

	PACK_DATA_TYPE pack_announce = Inv_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_announce;	// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].pack_data[0].pack_announce;

	pack_announce.total_voltage = summary_Pack_node_X_pack_announce(										// 汇总电压
		 offsetof(PACK_DATA_TYPE, total_voltage), MEMSIZE(PACK_DATA_TYPE, total_voltage), data_max);

	pack_announce.total_current = summary_Pack_node_X_pack_announce(										// 汇总电流
		 offsetof(PACK_DATA_TYPE, total_current), MEMSIZE(PACK_DATA_TYPE, total_current), data_sum);

	uint8_t chg_status=0;
	uint16_t  chg_full_times=0;
	uint16_t  dsg_empty_tims=0;
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)								// 汇总电池状态
	{
		if (Inv_can[node_id].pack_data[0].pack_announce.online & 0x01)
		{
			chg_status = Inv_can[node_id].pack_data[0].pack_announce.chg_status;
			if (chg_status != 0)
				break;
			#if 0 //该处理方案先屏蔽，等产品确认是否这么处理
			ESP_LOGI(TAG,"node_id:%d,chg_status:%d chg_full_time:%u,dsg_empty_time:%u",node_id,Inv_can[node_id].pack_data[0].pack_announce.chg_status,Inv_can[node_id].pack_data[0].pack_announce.chg_full_time,Inv_can[node_id].pack_data[0].pack_announce.dsg_empty_time);
			switch(Inv_can[node_id].pack_data[0].pack_announce.chg_status)
			{
				case 0:
				case 2:
				{
					uint16_t cur_times;
					if((chg_status!=1)&&(chg_status!=2))
					{
						chg_status=Inv_can[node_id].pack_data[0].pack_announce.chg_status;
					}
					if(Inv_can[node_id].pack_data[0].pack_announce.dsg_empty_time)
						cur_times=Inv_can[node_id].pack_data[0].pack_announce.dsg_empty_time;
					else
						cur_times=Inv_can[node_id].pack_data[0].pack_announce.chg_full_time;
					if(dsg_empty_tims<cur_times)
					{
						dsg_empty_tims=cur_times;
					}
				}
					break;
				case 1:
				{
					if(chg_status!=1)
					chg_status=1;
					if(chg_full_times<Inv_can[node_id].pack_data[0].pack_announce.chg_full_time)
					{
						chg_full_times=Inv_can[node_id].pack_data[0].pack_announce.chg_full_time;
					}
				}
					break;
				default:
					break;

			}
			#endif
		}
	}
	pack_announce.chg_status = chg_status;
	//pack_announce.chg_full_time=chg_full_times;
	//pack_announce.dsg_empty_time=dsg_empty_tims;
	//ESP_LOGI(TAG,"END chg_status:%d,chg_full_time:%u,dsg_empty_time:%u",pack_announce.chg_status,pack_announce.chg_full_time,pack_announce.dsg_empty_time);

	uint32_t pack_sum_soc = 0;
	uint32_t pack_sum_soc2 = 0;
	uint8_t pack_nums = 0;

	/* 不能直接用电池主机的SOC相加后除以主机数量得到平均SOC,因为电池主机带的电池包数量可能是不相同的 */
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)								// 汇总SOC与SOC2
	{
		//ESP_LOGI(TAG,"Inv_can[%lu].pack_data[0].pack_announce.online:0x%x",(long unsigned int)node_id,Inv_can[node_id].pack_data[0].pack_announce.online);
		if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << 0))
		{
			uint8_t pack_soc = Inv_can[node_id].pack_data[0].pack_announce.soc;
			uint8_t pack_soc2 = Inv_can[node_id].pack_data[0].pack_announce.soc2;
			uint8_t pack_cnt = Inv_can[node_id].pack_data[0].pack_announce.pack_cnt;
			pack_nums += pack_cnt;
			pack_sum_soc += (pack_soc * pack_cnt);
			pack_sum_soc2 += (pack_soc2 * pack_cnt);
		}
	}
	//ESP_LOGI(TAG,"pack_sum_soc:%lu,pack_sum_soc2:%lu,pack_nums:%d",(long unsigned int)pack_sum_soc,(long unsigned int)pack_sum_soc2,pack_nums);
	if (pack_nums)
	{
		//只要还有任意一个电池包有电量，都不算做%0
		pack_announce.soc = (pack_sum_soc / pack_nums);
		if(!pack_announce.soc&&pack_sum_soc)
			pack_announce.soc=1;

		pack_announce.soc2 = pack_sum_soc2 / pack_nums;
		if(!pack_announce.soc2&&pack_sum_soc2)
			pack_announce.soc2=1;
	}


	pack_announce.avg_temp = summary_Pack_node_X_pack_announce(												// 汇总平均温度
		 offsetof(PACK_DATA_TYPE, avg_temp), MEMSIZE(PACK_DATA_TYPE, avg_temp), data_average_pack);

	pack_announce.max_chg_voltage = summary_Pack_node_X_pack_announce(										// 汇总最大充电电压
		 offsetof(PACK_DATA_TYPE, max_chg_voltage), MEMSIZE(PACK_DATA_TYPE, max_chg_voltage), data_min);

	pack_announce.max_chg_current = summary_Pack_node_X_pack_announce(										// 汇总最大充电电流
		 offsetof(PACK_DATA_TYPE, max_chg_current), MEMSIZE(PACK_DATA_TYPE, max_chg_current), data_min);

	pack_announce.max_dsg_current = summary_Pack_node_X_pack_announce(										// 汇总最大放电电流
		 offsetof(PACK_DATA_TYPE, max_dsg_current), MEMSIZE(PACK_DATA_TYPE, max_dsg_current), data_min);

	pack_announce.status1.all = summary_Pack_node_X_pack_announce(											// 汇总状态1
		 offsetof(PACK_DATA_TYPE, status1.all), MEMSIZE(PACK_DATA_TYPE, status1.all), data_or_operate);

	pack_announce.status2.all = summary_Pack_node_X_pack_announce(											// 汇总状态2
		 offsetof(PACK_DATA_TYPE, status2.all), MEMSIZE(PACK_DATA_TYPE, status2.all), data_or_operate);

	// pack_announce.soh = summary_Pack_node_X_pack_announce(													// 汇总SOH
	// 	 offsetof(PACK_DATA_TYPE, soh), MEMSIZE(PACK_DATA_TYPE, soh), data_average_pack);

	/* 6006 SOH：各在线node直接取主包0x50(pack_announce.soh)，再对node做简单平均（不按pack_cnt加权）
	 * 单机时仅1个node → 与从机1的6006一致；AC并机时为各簇soh算术平均 */
	{
		uint32_t soh_sum = 0;
		uint8_t soh_node_cnt = 0;
		for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
		{
			if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << 0))
			{
				soh_sum += Inv_can[node_id].pack_data[0].pack_announce.soh;
				soh_node_cnt++;
			}
		}
		if (soh_node_cnt)
		{
			pack_announce.soh = (uint8_t)(soh_sum / soh_node_cnt);
		}
	}

//	uint16_t online = 0;
	uint16_t cnt = 0;
//	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)								// 汇总在线标志
//	{
//		for (int i = 0; i < PACK_MAX_NUM; i++)
//		{
//			if (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i)) {
//				online |= (1 << (cnt++));
//			}
//		}
//	}
	pack_announce.online = reals.online_Pack_num;//3*24溢出，不能用bit表示，改总数量；online;

	uint16_t cap_online = 0;
	cnt = 0;
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)								// 汇总mos输出标志
	{
		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			if (Inv_can[node_id].pack_data[0].pack_announce.cap_online & (1 << i)) {
				cap_online |= (1 << (cnt++));
			}
		}
	}
	pack_announce.cap_online = cap_online;

	pack_announce.pack_cnt = summary_Pack_node_X_pack_announce(												// 汇总电池包数量
		 offsetof(PACK_DATA_TYPE, pack_cnt), MEMSIZE(PACK_DATA_TYPE, pack_cnt), data_sum);

	pack_announce.chg_full_time = summary_Pack_node_X_pack_announce(										// 汇总预计充满时间
		 offsetof(PACK_DATA_TYPE, chg_full_time), MEMSIZE(PACK_DATA_TYPE, chg_full_time), data_max);

	pack_announce.dsg_empty_time = summary_Pack_node_X_pack_announce(										// 汇总预计放空时间
		 offsetof(PACK_DATA_TYPE, dsg_empty_time), MEMSIZE(PACK_DATA_TYPE, dsg_empty_time), data_max);

	pack_announce.TotalCurrent_bias = summary_Pack_node_X_pack_announce(									// 汇总总电流(基准偏移30000)
		 offsetof(PACK_DATA_TYPE, TotalCurrent_bias), MEMSIZE(PACK_DATA_TYPE, TotalCurrent_bias), data_average_pack);

	pack_announce.cell_min_volt = summary_Pack_node_X_pack_announce(										// 汇总最小电芯电压
		 offsetof(PACK_DATA_TYPE, cell_min_volt), MEMSIZE(PACK_DATA_TYPE, cell_min_volt), data_min);

	pack_announce.cell_max_volt = summary_Pack_node_X_pack_announce(										// 汇总最大电芯电压
		 offsetof(PACK_DATA_TYPE, cell_max_volt), MEMSIZE(PACK_DATA_TYPE, cell_max_volt), data_min);

	pack_announce.protect_status = summary_Pack_node_X_pack_announce(										// 汇总保护状态
		 offsetof(PACK_DATA_TYPE, protect_status), MEMSIZE(PACK_DATA_TYPE, protect_status), data_min);

	pack_announce.aging_current = summary_Pack_node_X_pack_announce(										// 汇总老化电流
		 offsetof(PACK_DATA_TYPE, aging_current), MEMSIZE(PACK_DATA_TYPE, aging_current), data_sum);

	Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce = pack_announce;
	//ESP_LOGI(TAG,"chg_full_time:%u",Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.chg_full_time);								// 保存汇总数据
	//ESP_LOGI(TAG,"dsg_empty_time:%u",Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.dsg_empty_time);
}

void data_sum_pack_base(void)
{
	#undef PACK_DATA_TYPE
	#define PACK_DATA_TYPE	pack_base_struct

	PACK_DATA_TYPE pack_base = Inv_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_base;			// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].pack_data[0].pack_base;

	pack_base.total_voltage = summary_Pack_node_X_pack_base(												// 汇总电压
		 offsetof(PACK_DATA_TYPE, total_voltage), MEMSIZE(PACK_DATA_TYPE, total_voltage), data_max);

	pack_base.total_current = summary_Pack_node_X_pack_base(												// 汇总电流
		 offsetof(PACK_DATA_TYPE, total_current), MEMSIZE(PACK_DATA_TYPE, total_current), data_sum);

	// pack_base.soc = summary_Pack_node_X_pack_base(												// 汇总soc
	// 	 offsetof(PACK_DATA_TYPE, soc), MEMSIZE(PACK_DATA_TYPE, soc), data_sum);

	pack_base.soc = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soc;								// 汇总soc
	// ESP_LOGI(TAG, "summary pack soc == %d\n", pack_base.soc);


	// pack_base.soh = summary_Pack_node_X_pack_base(															// 汇总soh
	// 	 offsetof(PACK_DATA_TYPE, soh), MEMSIZE(PACK_DATA_TYPE, soh), data_average_pack);
	/* 
	* 汇总soh：簇级SOH以0x50为准（data_sum_pack_announce已按node算术平均），
	* 不再对0x51各包soh做data_average_pack；与6006同源 
	*/
	pack_base.soh = Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_announce.soh;
	// ESP_LOGI(TAG, "summary pack soh == %d\n", pack_base.soh);

	pack_base.avg_temp = summary_Pack_node_X_pack_base(														// 汇总平均温度
		 offsetof(PACK_DATA_TYPE, avg_temp), MEMSIZE(PACK_DATA_TYPE, avg_temp), data_average_pack);

	pack_base.min_cell_voltage = summary_Pack_node_X_pack_base(												// 汇总最小电芯电压
		 offsetof(PACK_DATA_TYPE, min_cell_voltage), MEMSIZE(PACK_DATA_TYPE, min_cell_voltage), data_min);

	pack_base.max_cell_voltage = summary_Pack_node_X_pack_base(												// 汇总最大电芯电压
		 offsetof(PACK_DATA_TYPE, max_cell_voltage), MEMSIZE(PACK_DATA_TYPE, max_cell_voltage), data_max);

	pack_base.min_temp_value = summary_Pack_node_X_pack_base(												// 汇总最低温度
		 offsetof(PACK_DATA_TYPE, min_temp_value), MEMSIZE(PACK_DATA_TYPE, min_temp_value), data_min);

	pack_base.max_temp_value = summary_Pack_node_X_pack_base(												// 汇总最高温度
		 offsetof(PACK_DATA_TYPE, max_temp_value), MEMSIZE(PACK_DATA_TYPE, max_temp_value), data_max);

	int offset = offsetof(PACK_DATA_TYPE, protect[0]);
	for (int i = 0; i < sizeof(pack_base.protect)/sizeof(pack_base.protect[0]); i++)			// 汇总保护信息
	{
		pack_base.protect[i] = summary_Pack_node_X_pack_base( offset, sizeof(pack_base.protect[0]), data_or_operate);
		offset += sizeof(pack_base.protect[0]);
	}

	offset = offsetof(PACK_DATA_TYPE, fault[0]);
	for (int i = 0; i < sizeof(pack_base.fault)/sizeof(pack_base.fault[0]); i++)				// 汇总故障信息
	{
		pack_base.fault[i] = summary_Pack_node_X_pack_base( offset, sizeof(pack_base.fault[0]), data_or_operate);
		offset += sizeof(pack_base.fault[0]);
	}

	offset = offsetof(PACK_DATA_TYPE, alarm[0]);
	for (int i = 0; i < sizeof(pack_base.alarm)/sizeof(pack_base.alarm[0]); i++)				// 汇总告警信息
	{
		pack_base.alarm[i] = summary_Pack_node_X_pack_base( offset, sizeof(pack_base.alarm[0]), data_or_operate);
		offset += sizeof(pack_base.alarm[0]);
	}

	pack_base.relay = summary_Pack_node_X_pack_base(														// 汇总继电器状态
		 offsetof(PACK_DATA_TYPE, relay), MEMSIZE(PACK_DATA_TYPE, relay), data_or_operate);

	pack_base.pack_total_cell = summary_Pack_node_X_pack_base(												// 汇总电芯总数
		 offsetof(PACK_DATA_TYPE, pack_total_cell), MEMSIZE(PACK_DATA_TYPE, pack_total_cell), data_sum);

	pack_base.pack_total_ntc = summary_Pack_node_X_pack_base(												// 汇总NTC总数
		 offsetof(PACK_DATA_TYPE, pack_total_ntc), MEMSIZE(PACK_DATA_TYPE, pack_total_ntc), data_sum);

	pack_base.chg_full_time = summary_Pack_node_X_pack_base(												// 汇总预计充满时间
		 offsetof(PACK_DATA_TYPE, chg_full_time), MEMSIZE(PACK_DATA_TYPE, chg_full_time), data_max);

	pack_base.dsg_empty_time = summary_Pack_node_X_pack_base(												// 汇总预计放空时间
		 offsetof(PACK_DATA_TYPE, dsg_empty_time), MEMSIZE(PACK_DATA_TYPE, dsg_empty_time), data_max);

	Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_base = pack_base;											// 保存汇总数据
}

void data_sum_pack_extend(void)
{
	#undef PACK_DATA_TYPE
	#define PACK_DATA_TYPE	pack_extend_struct

	PACK_DATA_TYPE pack_extend = Inv_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_extend;	// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Inv_can[0].pack_data[0].pack_extend;
	//ESP_LOGI(TAG,"data_sum_pack_extend chg_energy 1:%lu, 2:%lu. 3:%lu",Inv_can[0].pack_data[0].pack_extend.total_chg_energy,Inv_can[1].pack_data[0].pack_extend.total_chg_energy,Inv_can[2].pack_data[0].pack_extend.total_chg_energy);
	pack_extend.capacity = summary_Pack_node_X_pack_extend(													// 汇总容量
		 offsetof(PACK_DATA_TYPE, capacity), MEMSIZE(PACK_DATA_TYPE, capacity), data_sum);

	pack_extend.total_chg_energy = summary_Pack_node_X_pack_extend(											// 汇总累积充电能量
		 offsetof(PACK_DATA_TYPE, total_chg_energy), MEMSIZE(PACK_DATA_TYPE, total_chg_energy), data_sum);

	pack_extend.total_dsg_energy = summary_Pack_node_X_pack_extend(											// 汇总累积放电能量
		 offsetof(PACK_DATA_TYPE, total_dsg_energy), MEMSIZE(PACK_DATA_TYPE, total_dsg_energy), data_sum);

	pack_extend.once_chg_energy = summary_Pack_node_X_pack_extend(											// 汇总当前充电能量
		 offsetof(PACK_DATA_TYPE, once_chg_energy), MEMSIZE(PACK_DATA_TYPE, once_chg_energy), data_sum);

	pack_extend.once_dsg_energy = summary_Pack_node_X_pack_extend(											// 汇总当前放电能量
		 offsetof(PACK_DATA_TYPE, once_dsg_energy), MEMSIZE(PACK_DATA_TYPE, once_dsg_energy), data_sum);

	pack_extend.sir_value = summary_Pack_node_X_pack_extend(													// 汇总绝缘内阻
		 offsetof(PACK_DATA_TYPE, sir_value), MEMSIZE(PACK_DATA_TYPE, sir_value), data_average_pack);

	pack_extend.inner_value = summary_Pack_node_X_pack_extend(												// 汇总电池包内阻
		 offsetof(PACK_DATA_TYPE, inner_value), MEMSIZE(PACK_DATA_TYPE, inner_value), data_average_pack);

	pack_extend.cycle_count = summary_Pack_node_X_pack_extend(												// 汇总循环次数
		 offsetof(PACK_DATA_TYPE, cycle_count), MEMSIZE(PACK_DATA_TYPE, cycle_count), data_average_pack);

	Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend = pack_extend;										// 保存汇总数据
	ESP_LOGI(TAG,"data_sum_pack_extend total_chg_energy:%lu",Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_extend.total_chg_energy);
}

void data_sum_pack_about(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_about = Inv_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_about;	// 取在线设备数据
}

void data_sum_pack_config(void)
{
	Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_config = Inv_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_config; // 取在线设备数据
}

void data_sum_pack_debug(void)
{
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < PACK_MAX_NUM; i++)
		{
			if (Inv_can[node].pack_data[0].pack_announce.online & (1 << i))
			{
				Inv_can[node].pack_data[i].pack_debug1.cells = Inv_can[node].pack_data[i].pack_base.pack_total_cell;
				Inv_can[node].pack_data[i].pack_debug1.ntc_cnt = Inv_can[node].pack_data[i].pack_base.pack_total_ntc;
			}
		}
	}
	Inv_can[DEV_MAIN_NODE_MAX].pack_data[PACK_MAX_NUM].pack_debug1 = Inv_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_debug1; // 取在线设备数据
}




#if 0
void data_sum_ems(void)
{
	/* 汇总CT测试结果 */
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_set01.ct_test.all = UploadEMSData.ct_test.all;

	/* 汇总电网、PV、AC负载功率 */
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.GridAllTotalPower = UploadEMSData.SysEnergyData.GridAllTotalPower;
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PVAllTotalPower = UploadEMSData.SysEnergyData.PVAllTotalPower;
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ACLoadAllTotalPower = UploadEMSData.SysEnergyData.ACLoadAllTotalPower;

	enum {
		INV_WORK_STA_IDLE, 				// 逆变状态空闲
		INV_WORK_STA_OFF_GRID, 			// 逆变状态离网
		INV_WORK_STA_GRID_CHARGE,		// 逆变状态并网充电
		INV_WORK_STA_GRID_DISCHARGE,	// 逆变状态并网放电
		INV_WORK_STA_GRID_LOAD			// 逆变状态并网带载
	};

	/* 当有电网时负载数据与电网数据取EMS模块汇总的数据,否则取逆变汇总数据 */
	uint8_t inv_work_status = Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.inv_work_state;
	if ((inv_work_status != INV_WORK_STA_IDLE) && (inv_work_status != INV_WORK_STA_OFF_GRID))
	{
		/* 汇总逆变负载数据 */
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_phase_number = UploadEMSData.SysEnergyData.ac_phase_number;
		for(uint16_t num = 0; num < UploadEMSData.SysEnergyData.ac_phase_number; num++)
		{
			Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[num].load_power = (uint16_t)UploadEMSData.SysEnergyData.ac_load[num].load_power;
			Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[num].load_voltage = (uint16_t)UploadEMSData.SysEnergyData.ac_load[num].load_voltage;
			Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_load.ac_load[num].load_current = (uint16_t)UploadEMSData.SysEnergyData.ac_load[num].load_current;
		}
	}

	inv_advance_param_t *advance_param = ll_inv_param_get("inv_advance_param");
	if (advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1)						// 并机开关打开时才汇总电网数据
	{
		/* 汇总电网数据 */
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_phase_number = UploadEMSData.SysEnergyData.grid_phase_number;
		for(uint16_t num = 0; num < UploadEMSData.SysEnergyData.grid_phase_number; num++)
		{
			Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[num].input_power = (uint16_t)UploadEMSData.SysEnergyData.grid_detail[num].input_power;
			Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[num].input_voltage = (uint16_t)UploadEMSData.SysEnergyData.grid_detail[num].input_voltage;
			Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_grid.grid_detail[num].input_current = (uint16_t)UploadEMSData.SysEnergyData.grid_detail[num].input_current;
		}
	}

	/* 汇总PV数据 */
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_number.pv_total = UploadEMSData.SysEnergyData.pv_number.pv_total;
	int pv_sum = UploadEMSData.SysEnergyData.pv_number.dc_pv_numbers + UploadEMSData.SysEnergyData.pv_number.ac_pv_numbers;
	for(uint16_t num = 0; num < pv_sum; num++)
	{
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[num].status = UploadEMSData.SysEnergyData.pv_detail[num].status;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[num].input_type = UploadEMSData.SysEnergyData.pv_detail[num].input_type;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[num].input_power = (uint16_t)UploadEMSData.SysEnergyData.pv_detail[num].input_power;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[num].input_voltage = (uint16_t)UploadEMSData.SysEnergyData.pv_detail[num].input_voltage;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_pv.pv_detail[num].input_current = (uint16_t)UploadEMSData.SysEnergyData.pv_detail[num].input_current;
	}

	/* 汇总能量线 */
	Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.line_event = UploadEMSData.EnergyLineStatus.ALL;

	if (advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1)						// 并机开关打开时才从EMS获取能量
	{
		energy_data_t* energy_data = energy_data_get();
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.ACLoadTotalEnergy = energy_data->load_all;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.PvTotalChargingEnergy = energy_data->pv_charge;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.GridTotalChargingEnergy = energy_data->grid_charge;
		Inv_can[DEV_MAIN_NODE_MAX].inv_data[INV_MAX_NUM].inv_base.FeedbackEnergy = energy_data->grid_feedback;
	}
}
#endif





/**
 * @brief 逆变数据汇总
 *
 * @return 无
 */
void data_sum_inv(void)
{
	if(0 == reals.online_Inv_num)
	{
//		return;		// 没有设备在线直接返回
	}


//__inv_sum:
	data_sum_inv_announce();
	data_sum_inv_base();
	data_sum_inv_line();
	data_sum_inv_about();
	data_sum_inv_pv();
	data_sum_inv_grid();
	data_sum_inv_load();
	data_sum_inv_data();
	data_sum_inv_meter();
	data_sum_inv_gen();
	data_sum_inv_base_param();
	data_sum_inv_advance_param();
	data_sum_inv_auth_param();
	data_sum_inv_log();
	data_sum_inv_year_energy();
	data_sum_inv_now_energy();
	data_sum_inv_wifi();
	data_sum_inv_wifi_report();
	data_sum_inv_wifi_auth_param();
	data_sum_inv_auth_param_27H();
	data_sum_inv_dc_hub_info();
	data_sum_inv_ac_hub_info();
	data_sum_inv_ota_state();
	data_sum_inv_d400s_info();
}

/**
 * @brief PACK数据汇总
 *
 * @return 无
 */
void data_sum_pack(void)
{

	if(0 == reals.online_Pack_num)
	{
//		return;		// 没有设备在线直接返回
	}

//	return;		// 没有设备在线直接返回

//__pack_sum:
	data_sum_pack_announce();
	data_sum_pack_base();
	data_sum_pack_extend();
	data_sum_pack_about();
	data_sum_pack_config();
	data_sum_pack_debug();
}
