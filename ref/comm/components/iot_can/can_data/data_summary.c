#include "data_summary.h"
#include "can_data.h"
#include "can_protocol.h"
#include "can_protocol.h"
#include "parameter.h"

//static int online_main_node;
//static int online_sub_node;

typedef uint64_t (*data_operate)(uint32_t *buff, uint32_t len);
#define MEMSIZE(type, x)	sizeof(((type*)0)->x)


//static const char *TAG = "[data_summary]";

#define TAG "[data_summary]"


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
//
///*
//windy 修改二维求和
//Dcdc_can[i].inv_data[y]
//
//*/
//static uint32_t summary_inv_node_y(uint8_t index_x, uint8_t *pdata, int offset, int width, data_operate operate)
//{
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t i = 0;
//	uint8_t *pdata2;
//	uint32_t rtn = 0;
//
////	ESP_LOGI(TAG, "summary_inv_node_y	      1");
//
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		pdata2 = pdata + (i * sizeof(inv_node_struct)); // 指针指向下一个inv节点数据
////		ESP_LOGI(TAG, "summary_inv_node_y		 2 ");
//	
//		if (Dcdc_can[index_x].inv_data[i].online)
//		{
////			ESP_LOGI(TAG, "summary_inv_node_y		3  ");
//		
//			if(width == 1) {
////				ESP_LOGI(TAG, "summary_inv_node_y		 4 ");
//				
//				buff[i] = (uint32_t)(*((uint8_t*)(pdata2 + offset)));
//			}
//			else if(width == 2) {
////				ESP_LOGI(TAG, "summary_inv_node_y		 5 ");
//				
//				buff[i] = (uint32_t)(*((uint16_t*)(pdata2 + offset)));
//			}
//			else if(width == 4) {
////				ESP_LOGI(TAG, "summary_inv_node_y		6  ");
//				
//				buff[i] = (uint32_t)(*((uint32_t*)(pdata2 + offset)));
//			}
//		}
//		else
//		{
//			buff[i] =0;
//		}
////		ESP_LOGI(TAG, "summary_inv_node_y		7  ");
//		
//	}
//	rtn =operate(buff, i);
////	ESP_LOGI(TAG, "summary_inv_node_y	    8  ");
//	
//	pdata2 = pdata + ((uint32_t)i * sizeof(inv_node_struct)) + offset;		
//
//	
////	ESP_LOGI(TAG, "summary_inv_node_y		9,Dcdc_can[0].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Dcdc_can[0].inv_data[0].inv_announce);
////	ESP_LOGI(TAG, "summary_inv_node_y		9,Dcdc_can[1].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Dcdc_can[1].inv_data[0].inv_announce);
////	ESP_LOGI(TAG, "summary_inv_node_y		9,Dcdc_can[2].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Dcdc_can[2].inv_data[0].inv_announce);
////		
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata=0x%lx  ",(uint32_t)pdata);
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata2=0x%lx  ",(uint32_t)pdata2);
////	
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,sizeof(inv_node_struct))=%ld  ",(uint32_t)sizeof(inv_node_struct));
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,i *sizeof(inv_node_struct))=%ld  ",(uint32_t)((uint32_t)i * sizeof(inv_node_struct)));
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)offset=%ld  ",(uint32_t)offset);
////
////	ESP_LOGI(TAG, "summary_inv_node_y	    9  ");
//	memcpy(pdata2, (uint8_t *)&rtn, width);//尾部数组是汇总
//	//windy debug
////	memcpy(pdata2, rtn, width);//尾部数组是汇总
////	ESP_LOGI(TAG, "summary_inv_node_y	    10  ");
//
//
//	if(i != 0) 
//	{
//		return rtn;
//	}
//
//	return 0;
//}


/*
windy 修改二维求和
Dcdc_can[i].inv_data[y]

*/
//static uint32_t summary_Pack_node_y(uint8_t index_x, uint8_t *pdata, int offset, int width, data_operate operate)
//{
//	uint32_t buff[PACK_MAX_NUM];
//	uint8_t i = 0;
//	uint8_t *pdata2;
//	uint32_t rtn = 0;
//
////	ESP_LOGI(TAG, "summary_inv_node_y	      1");
//
//	for (i = 0; i < PACK_MAX_NUM; i++)
//	{
//		pdata2 = pdata + (i * sizeof(pack_node_struct)); // 指针指向下一个inv节点数据
////		ESP_LOGI(TAG, "summary_inv_node_y		 2 ");
//	
//		if (Dcdc_can[index_x].pack_data[i].online)
//		{
////			ESP_LOGI(TAG, "summary_inv_node_y		3  ");
//		
//			if(width == 1) {
////				ESP_LOGI(TAG, "summary_inv_node_y		 4 ");
//				
//				buff[i] = (uint32_t)(*((uint8_t*)(pdata2 + offset)));
//			}
//			else if(width == 2) {
////				ESP_LOGI(TAG, "summary_inv_node_y		 5 ");
//				
//				buff[i] = (uint32_t)(*((uint16_t*)(pdata2 + offset)));
//			}
//			else if(width == 4) {
////				ESP_LOGI(TAG, "summary_inv_node_y		6  ");
//				
//				buff[i] = (uint32_t)(*((uint32_t*)(pdata2 + offset)));
//			}
//		}
//		else
//		{
//			buff[i] =0;
//		}		
////		ESP_LOGI(TAG, "summary_inv_node_y		7  ");
//		
//	}
//	rtn =operate(buff, i);
////	ESP_LOGI(TAG, "summary_inv_node_y	    8  ");
//	
//	pdata2 = pdata + ((uint32_t)i * sizeof(pack_node_struct)) + offset;		
//
//	
////	ESP_LOGI(TAG, "summary_inv_node_y		9,Dcdc_can[0].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Dcdc_can[0].inv_data[0].inv_announce);
////	ESP_LOGI(TAG, "summary_inv_node_y		9,Dcdc_can[1].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Dcdc_can[1].inv_data[0].inv_announce);
////	ESP_LOGI(TAG, "summary_inv_node_y		9,Dcdc_can[2].inv_data[0].inv_announce=0x%lx  ",(uint32_t)&Dcdc_can[2].inv_data[0].inv_announce);
////		
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata=0x%lx  ",(uint32_t)pdata);
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)pdata2=0x%lx  ",(uint32_t)pdata2);
////	
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,sizeof(inv_node_struct))=%ld  ",(uint32_t)sizeof(inv_node_struct));
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,i *sizeof(inv_node_struct))=%ld  ",(uint32_t)((uint32_t)i * sizeof(inv_node_struct)));
////	ESP_LOGI(TAG, "summary_inv_node_y	    9,(uint32_t)offset=%ld  ",(uint32_t)offset);
////
////	ESP_LOGI(TAG, "summary_inv_node_y	    9  ");
//	memcpy(pdata2, (uint8_t *)&rtn, width);//尾部数组是汇总
//	//windy debug
////	memcpy(pdata2, rtn, width);//尾部数组是汇总
////	ESP_LOGI(TAG, "summary_inv_node_y	    10  ");
//
//
//	if(i != 0) 
//	{
//		return rtn;
//	}
//
//	return 0;
//}
//
///*
//windy 修改二维求和
//Dcdc_can[x]
//
//
//*/
//static uint32_t summary_inv_node_X_inv_announce( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	
////ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//	
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
//		pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_announce ;	// 指针指向下一个inv节点数据
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");
//	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");
//		
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");
//
//	pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_announce  + ((uint32_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;		
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");
//
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");
//
//	return 0;
//}
//
///*
//windy 修改二维求和
//Dcdc_can[x]
//
//
//*/
//static uint32_t summary_inv_node_X_inv_base( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_base + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
//		pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_base;
//	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//	}
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_base + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;	
//	pdata = (uint8_t *)&Dcdc_can[i].inv_data[DCDC_MAX_NUM].inv_base + offset;
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
//
//	return 0;
//}

/*
windy 修改二维求和
Dcdc_can[x]


*/
//static uint32_t summary_inv_node_X_inv_pv( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_pv + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
//		pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_pv;	// 指针指向下一个inv节点数据
//	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//	}
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_pv + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;
//	pdata = (uint8_t *)&Dcdc_can[i].inv_data[DCDC_MAX_NUM].inv_pv + offset;		
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
//
//	return 0;
//}
//
///*
//windy 修改二维求和
//Dcdc_can[x]
//
//
//*/
//static uint32_t summary_inv_node_X_inv_grid( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_grid + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
//		pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_grid;	// 指针指向下一个inv节点数据
//	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//	}
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_grid + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;
//	pdata = (uint8_t *)&Dcdc_can[i].inv_data[DCDC_MAX_NUM].inv_grid + offset;		
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
//
//	return 0;
//}
//
///*
//windy 修改二维求和
//Dcdc_can[x]
//
//
//*/
//static uint32_t summary_inv_node_X_inv_load( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_load + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
//		pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_load;	// 指针指向下一个inv节点数据
//	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//	}
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_load + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;	
//	pdata = (uint8_t *)&Dcdc_can[i].inv_data[DCDC_MAX_NUM].inv_load + ((uint16_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;	
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
//
//	return 0;
//}
///*
//windy 修改二维求和
//Dcdc_can[x]
//
//
//*/
//static uint32_t summary_inv_node_X_inv_data( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_data + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
//		pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_data;	// 指针指向下一个inv节点数据
//	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//	}
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_data + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;	
//	pdata = (uint8_t *)&Dcdc_can[i].inv_data[DCDC_MAX_NUM].inv_data + offset;	
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
//
//	return 0;
//}
//
///*
//windy 修改二维求和
//Dcdc_can[x]
//
//
//*/
//static uint32_t summary_inv_node_X_inv_gen( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_gen + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
//		pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_gen;	// 指针指向下一个inv节点数据
//	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//	}
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
//	// pdata = (uint8_t *)&Dcdc_can[i].inv_data[0].inv_gen + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ)) + ((uint16_t)DCDC_MAX_NUM * sizeof(inv_node_struct)) + offset;	
//	pdata = (uint8_t *)&Dcdc_can[i].inv_data[DCDC_MAX_NUM].inv_gen + offset;	
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
//
//	return 0;
//}
//
//static uint32_t summary_inv_node(uint8_t *pdata, int offset, int width, data_operate operate)
//{
//	uint8_t idx = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	int cnt = 0;
//
//	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)
//	{
//		for (int i = 0; i < DCDC_MAX_NUM; i++)
//		{
//			uint8_t *p_data = pdata + ((cnt++) * sizeof(inv_node_struct));	// 指针指向下一个inv节点数据
//
//			if (Dcdc_can[node_id].inv_data[i].online)
//			{
//				if(width == 1) {
//					buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
//				}
//				else if(width == 2) {
//					buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
//				}
//				else if(width == 4) {
//					buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
//				}
//			}
//		}
//	}
//
//	if(idx != 0) {
//		return operate(buff, idx);
//	}
//
//	return 0;
//}
//
//static uint32_t summary_pack_node(uint8_t *pdata, int offset, int width, data_operate operate)
//{
//	uint8_t idx = 0;
//	uint32_t buff[DCDC_MAX_NUM*PACK_MAX_NUM];
//	int cnt = 0;
//
//	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)
//	{
//		for (int i = 0; i < PACK_MAX_NUM; i++)
//		{
//			uint8_t *p_data = pdata + ((cnt++) * sizeof(pack_node_struct));	// 指针指向下一个pack节点数据
//
//			if (Dcdc_can[node_id].pack_data[0].pack_announce.online & (1 << i))
//			{
//				if(width == 1) {
//					buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
//				}
//				else if(width == 2) {
//					buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
//				}
//				else if(width == 4) {
//					buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
//				}
//			}
//		}
//	}
//
//	if(idx != 0) {
//		return operate(buff, idx);
//	}
//
//	return 0;
//}
//
//static uint32_t summary_pack_announce(uint8_t *pdata, int offset, int width, data_operate operate)
//{
//	uint8_t idx = 0;
//	uint32_t buff[DCDC_MAX_NUM*PACK_MAX_NUM];
//	int cnt = 0;
//
//	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)
//	{
//		for (int i = 0; i < PACK_MAX_NUM; i++)
//		{
//			uint8_t *p_data = pdata + ((cnt++) * sizeof(pack_node_struct));	// 指针指向下一个pack节点数据
//			if (i != 0) continue;											// 只对主电池包的数据进行汇总
//
//			if (Dcdc_can[node_id].pack_data[0].pack_announce.online & (1 << i))
//			{
//				if(width == 1) {
//					buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
//				}
//				else if(width == 2) {
//					buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
//				}
//				else if(width == 4) {
//					buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
//				}
//			}
//		}
//	}
//
//	if(idx != 0) {
//		return operate(buff, idx);
//	}
//
//	return 0;
//}

/*
windy 修改二维求和
Dcdc_can[x]


*/
//static uint32_t summary_Pack_node_X_pack_announce( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	
////ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//	
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
//		pdata = (uint8_t *)&Dcdc_can[i].pack_data[0].pack_announce ;	// 指针指向下一个inv节点数据
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");
//	
//		buff[i]= summary_Pack_node_y(i,pdata,offset,width, operate);
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");
//		
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");
//
//	// pdata = (uint8_t *)&Dcdc_can[i].pack_data[0].pack_announce  + ((uint32_t)PACK_MAX_NUM * sizeof(pack_node_struct)) + offset;	
//	pdata = (uint8_t *)&Dcdc_can[i].pack_data[DCDC_MAX_NUM].pack_announce + offset;	
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");
//
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");
//
//	return 0;
//}

/*
windy 修改二维求和
Dcdc_can[x]


*/
//static uint32_t summary_Pack_node_X_pack_base( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	
////ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//	
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
//		pdata = (uint8_t *)&Dcdc_can[i].pack_data[0].pack_base ;	// 指针指向下一个inv节点数据
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");
//	
//		buff[i]= summary_Pack_node_y(i,pdata,offset,width, operate);
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");
//		
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");
//
//	// pdata = (uint8_t *)&Dcdc_can[i].pack_data[0].pack_base  + ((uint32_t)PACK_MAX_NUM * sizeof(pack_node_struct)) + offset;	
//	pdata = (uint8_t *)&Dcdc_can[i].pack_data[DCDC_MAX_NUM].pack_base + offset;	
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");
//
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");
//
//	return 0;
//}

/*
windy 修改二维求和
Dcdc_can[x]


*/
//static uint32_t summary_Pack_node_X_pack_extend( int offset, int width, data_operate operate)
//{
//	uint8_t i = 0;
//	uint32_t buff[DCDC_MAX_NUM];
//	uint8_t *pdata;
//	uint32_t rtn = 0;
//	
////ESP_LOGI(TAG, "summary_inv_node_X_inv_announce  1");
//	for (i = 0; i < DCDC_MAX_NUM; i++)
//	{
//	
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	2");//((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ))
//		pdata = (uint8_t *)&Dcdc_can[i].pack_data[0].pack_extend ;	// 指针指向下一个inv节点数据
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	3");
//	
//		buff[i]= summary_Pack_node_y(i,pdata,offset,width, operate);
////		ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	4");
//		
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	5");
//
//	rtn =operate(buff, i);
////汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	6");
//
//	// pdata = (uint8_t *)&Dcdc_can[i].pack_data[0].pack_extend  + ((uint32_t)PACK_MAX_NUM * sizeof(pack_node_struct)) + offset;
//	pdata = (uint8_t *)&Dcdc_can[i].pack_data[DCDC_MAX_NUM].pack_extend + offset;		
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	7");
//
//	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	8");
//
//	if(i != 0) 
//	{
//		return operate(buff, i);
//	}
////	ESP_LOGI(TAG, "summary_inv_node_X_inv_announce	9");
//
//	return 0;
//}

//static uint32_t summary_pack_group_node(uint8_t node_id, uint8_t *pdata, int offset, int width, data_operate operate)
//{
//	uint8_t idx = 0;
//	uint32_t buff[PACK_MAX_NUM];
//
//	for (int i = 0; i < PACK_MAX_NUM; i++)
//	{
//		uint8_t *p_data = pdata + (i * sizeof(pack_node_struct));			// 指针指向下一个pack节点数据
//
//		if (Dcdc_can[node_id].pack_data[0].pack_announce.online & (1 << i))
//		{
//			if(width == 1) {
//				buff[idx++] = (uint32_t)(*((uint8_t*)(p_data + offset)));
//			}
//			else if(width == 2) {
//				buff[idx++] = (uint32_t)(*((uint16_t*)(p_data + offset)));
//			}
//			else if(width == 4) {
//				buff[idx++] = (uint32_t)(*((uint32_t*)(p_data + offset)));
//			}
//		}
//	}
//
//	if(idx != 0) {
//		return operate(buff, idx);
//	}
//
//	return 0;
//}

//void data_sum_inv_announce(void)
//{
//	#undef INV_DATA_TYPE
//	#define INV_DATA_TYPE	inv_announce_struct
//
//	INV_DATA_TYPE inv_announce = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_announce;	// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].inv_data[0].inv_announce;
////ESP_LOGI(TAG, "data_sum_inv_announce	     1  ");
//
//	int offset = offsetof(INV_DATA_TYPE, alarm[0]);
//	int memsize = MEMSIZE(INV_DATA_TYPE, alarm[0]);
//	int item_size = sizeof(inv_announce.alarm[0]);
//	for (int i = 0; i < sizeof(inv_announce.alarm)/sizeof(inv_announce.alarm[0]); i++)
//	{
//		int _offset = offset + (i * item_size);
//	summary_inv_node_X_inv_announce( _offset, memsize, data_or_operate);		// 汇总告警标志
//	}
////	ESP_LOGI(TAG, "data_sum_inv_announce		4   ");
//
//	offset = offsetof(INV_DATA_TYPE, fault[0]);
//	memsize = MEMSIZE(INV_DATA_TYPE, fault[0]);
//	item_size = sizeof(inv_announce.fault[0]);
//	for (int i = 0; i < sizeof(inv_announce.fault)/sizeof(inv_announce.fault[0]); i++)
//	{
//		int _offset = offset + (i * item_size);
//		summary_inv_node_X_inv_announce( _offset, memsize, data_or_operate);		// 汇总故障标志
//	}
////	ESP_LOGI(TAG, "data_sum_inv_announce		5   ");
//
//	uint16_t inv_online = 0;
//	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)								// 汇总逆变在线标志
//	{
//		for (int sub_node_id = 0; sub_node_id < DCDC_MAX_NUM; sub_node_id++)								// 汇总逆变在线标志
//		{
//			if (Dcdc_can[node_id].inv_data[sub_node_id].online) 
//			{
//				inv_online |= (1 << (node_id*DCDC_MAX_NUM+sub_node_id));
//			}
//		}	
//
//	}
//	inv_announce.online = inv_online;
//
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce = inv_announce;										// 保存汇总数据
//
//	summary_inv_node_X_inv_announce(										// 汇总状态标志
//		 offsetof(INV_DATA_TYPE, status_flags), MEMSIZE(INV_DATA_TYPE, status_flags), data_or_operate);
////	return;//windy debug
////		 ESP_LOGI(TAG, "data_sum_inv_announce		2	");
//
//	summary_inv_node_X_inv_announce(												// 汇总状态标志2
//		 offsetof(INV_DATA_TYPE, inv_flag2), MEMSIZE(INV_DATA_TYPE, inv_flag2), data_or_operate);
////	ESP_LOGI(TAG, "data_sum_inv_announce		 3  ");
//
//	summary_inv_node_X_inv_announce(												// 汇总待机功耗
//		 offsetof(INV_DATA_TYPE, stdby_pwr_coms), MEMSIZE(INV_DATA_TYPE, stdby_pwr_coms), data_sum);
//
//
//	summary_inv_node_X_inv_announce(											// 汇总总充电功率
//		 offsetof(INV_DATA_TYPE, total_chg_power), MEMSIZE(INV_DATA_TYPE, total_chg_power), data_sum);
////	ESP_LOGI(TAG, "data_sum_inv_announce		 6  ");
//
//	summary_inv_node_X_inv_announce(											// 汇总总放电功率
//		 offsetof(INV_DATA_TYPE, total_dsg_power), MEMSIZE(INV_DATA_TYPE, total_dsg_power), data_sum);
////	ESP_LOGI(TAG, "data_sum_inv_announce		 7  ");
//
//	summary_inv_node_X_inv_announce(														// 汇总故障标志5
//		 offsetof(INV_DATA_TYPE, fault5), MEMSIZE(INV_DATA_TYPE, fault5), data_or_operate);
////ESP_LOGI(TAG, "data_sum_inv_announce		 8  ");
////	#define AC_METER_MASK_POS			11 		// AC电表异常位置
////	#define GRID_METER_MASK_POS			4 		// 电网电表异常位置
////	#define PARALLEL_EXP_MASK_POS		14 		// 并机状态异常位置
////	uint16_t alarm3 = Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.alarm[2];
////	uint16_t alarm4 = Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.alarm[3];
////	uint16_t fault4 = Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.fault[3];
////
////	/* 当使能AC电表时才判断AC电表通信状态 */
////	alarm3 &= ~((uint16_t)1 << AC_METER_MASK_POS);
////	inv_advance_param_t *advance_param = ll_inv_param_get("inv_advance_param");
////	if (advance_param->ctrl_meter_enable)
////	{
//////		if (ac_couple_meter_status_get() == AC_METER_STA_ERR) {
//////			alarm3 |= ((uint16_t)1 << AC_METER_MASK_POS);
//////		}
////	}
//////	ESP_LOGI(TAG, "data_sum_inv_announce		 9	");
////
////	/* 电网电表状态检测 */
////	alarm4 &= ~((uint16_t)1 << GRID_METER_MASK_POS);
//////	if (grid_meter_status_get() == GRID_METER_STA_ERR) {
//////		alarm4 |= ((uint16_t)1 << GRID_METER_MASK_POS);
//////	}
////
////	/* 离网并机使能状态检测 */
////	fault4 &= ~((uint16_t)1 << PARALLEL_EXP_MASK_POS);
////	if ((NULL!=  advance_param)
////		&&(advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1))						// 并机开关打开时才检测底层设备的并机开关状态
////	{
////		for (int node = 0; node < DCDC_MAX_NUM; node++)
////		{
////			for (int i = 0; i < DCDC_MAX_NUM; i++)
////			{
////				if (!Dcdc_can[node].inv_data[i].online) continue;
//////				ESP_LOGI(TAG, "data_sum_inv_announce,node=%d,i=%d  ",node, i);
////
////				inv_set01_struct *inv_set01 = &Dcdc_can[node].inv_data[i].inv_set01;
//////				ESP_LOGI(TAG, "BB  data_sum_inv_announce,node=%d,i=%d  ",node, i);
////				
////				if ((inv_set01->ctrl_chg_max_volt == 0) &&
////					(inv_set01->ctrl_pv_max_curr == 0) &&
////					(inv_set01->ctrl_grid_max_power == 0) &&
////					(inv_set01->ctrl_feedback_max_power == 0) &&
////					(inv_set01->ctrl_Inv_addr_Set == 0)) continue;	// 判断逆变的数据是否有效
//////				ESP_LOGI(TAG, "CC  data_sum_inv_announce,node=%d,i=%d  ",node, i);
////
////				if (inv_set01->ctrl_mix.bit.multi_inv_off_grid_en != 1)
////				{
////					fault4 |= ((uint16_t)1 << PARALLEL_EXP_MASK_POS);
////					break;
////				}
//////				ESP_LOGI(TAG, "DD  data_sum_inv_announce,node=%d,i=%d  ",node, i);
////				
////			}
////
////			if (fault4 & ((uint16_t)1 << PARALLEL_EXP_MASK_POS)) break;
//////			ESP_LOGI(TAG, "EE  data_sum_inv_announce,node=%d  ",node);
////			
////		}
////	}
//////	ESP_LOGI(TAG, "data_sum_inv_announce		 10	");
////
////	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.alarm[2] = alarm3;	// 保存汇总了AC电表状态后的告警
////	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.alarm[3] = alarm4;	// 保存汇总了电网电表状态后的告警
////	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.fault[3] = fault4;	// 保存汇总了并网并联状态后的故障
//
//	/* 故障调试代码,当相应开关打开后有效 */
////	extern int alarm_dbg;			// 告警调试开关
////	extern int fault_dbg;			// 故障调试开关
////	extern uint16_t dbg_alarm[4];	// 告警信息
////	extern uint16_t dbg_fault[4];	// 故障信息
////	extern uint16_t dbg_fault5;		// 故障信息
////	if (alarm_dbg) {
////		memcpy(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.alarm, dbg_alarm, sizeof(dbg_alarm));
////	}
////	if (fault_dbg) {
////		memcpy(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.fault, dbg_fault, sizeof(dbg_fault));
////		Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.fault5 = dbg_fault5;
////	}
//}

//void data_sum_inv_base(void)
//{
//	#undef INV_DATA_TYPE
//	#define INV_DATA_TYPE	inv_base_struct
//
//	INV_DATA_TYPE inv_base = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base;			// 取在线设备数据
//	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].inv_data[0].inv_base;
//
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.online = inv_base.inv_online;				// 汇总逆变在线标志
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.inv_power_rang = inv_base.inv_power_rang;
//
//	/* 汇总逆变告警与故障标志 */
//	memcpy(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.alarm,  inv_base.alarm, sizeof(inv_base.alarm));
//	memcpy(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.fault,  inv_base.fault,  sizeof(inv_base.fault));
//
//	inv_base.inv_online = Dcdc_can[0].inv_data[0].inv_base.inv_online |
//							Dcdc_can[1].inv_data[0].inv_base.inv_online |
//							Dcdc_can[2].inv_data[0].inv_base.inv_online;
//	uint8_t inv_nums = 0;
//	for (int i = 0; i < DCDC_MAX_NUM; i++)
//	{
//		if (inv_base.inv_online & (1 << i)) {
//			inv_nums++;
//		}
//	}
//	inv_base.inv_num = inv_nums;																
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.inv_num = inv_base.inv_num;  // 汇总逆变数量
//
//	/* 汇总逆变工作状态与开关状态,以首台在线且处于并网运行的设备状态为准 */
//	for (int node = 0; node < DCDC_MAX_NUM; node++)
//	{
//		int found = 0;
//		for (int i = 0; i < DCDC_MAX_NUM; i++)
//		{
//			if (Dcdc_can[node].inv_data[i].online)
//			{
//				inv_base_struct *p_inv_base = &Dcdc_can[node].inv_data[i].inv_base;
//				if (p_inv_base->inv_work_state >= 2)	// 并网运行
//				{
//					Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.inv_work_state = p_inv_base->inv_work_state;
//					Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.ctrl_status.all = p_inv_base->ctrl_status.all;
//					found = 1;
//					break;
//				}
//			}
//		}
//
//		if (found) break;
//	}
//
//		summary_inv_node_X_inv_base(											// 汇总DC负载总功率
//		 offsetof(INV_DATA_TYPE, DCLoadAllTotalPower), MEMSIZE(INV_DATA_TYPE, DCLoadAllTotalPower), data_sum);
//
//		summary_inv_node_X_inv_base(											// 汇总AC负载总功率
//		 offsetof(INV_DATA_TYPE, ACLoadAllTotalPower), MEMSIZE(INV_DATA_TYPE, ACLoadAllTotalPower), data_sum);
//
//		summary_inv_node_X_inv_base(												// 汇总PV总功率
//		 offsetof(INV_DATA_TYPE, PVAllTotalPower), MEMSIZE(INV_DATA_TYPE, PVAllTotalPower), data_sum);
//
//		summary_inv_node_X_inv_base(												// 汇总电网总功率
//		 offsetof(INV_DATA_TYPE, GridAllTotalPower), MEMSIZE(INV_DATA_TYPE, GridAllTotalPower), data_sum);
//
//		summary_inv_node_X_inv_base(												// 汇总逆变总功率
//		 offsetof(INV_DATA_TYPE, InvAllTotalPower), MEMSIZE(INV_DATA_TYPE, InvAllTotalPower), data_sum);
//
//		summary_inv_node_X_inv_base(												// 汇总DC负载总能量
//		 offsetof(INV_DATA_TYPE, DCLoadTotalEnergy), MEMSIZE(INV_DATA_TYPE, DCLoadTotalEnergy), data_sum);
//
//		summary_inv_node_X_inv_base(												// 汇总AC负载总能量
//		 offsetof(INV_DATA_TYPE, ACLoadTotalEnergy), MEMSIZE(INV_DATA_TYPE, ACLoadTotalEnergy), data_sum);
//
//		summary_inv_node_X_inv_base(											// 汇总PV总充电能量
//		 offsetof(INV_DATA_TYPE, PvTotalChargingEnergy), MEMSIZE(INV_DATA_TYPE, PvTotalChargingEnergy), data_sum);
//
//		summary_inv_node_X_inv_base(										// 汇总电网总充电能量
//		 offsetof(INV_DATA_TYPE, GridTotalChargingEnergy), MEMSIZE(INV_DATA_TYPE, GridTotalChargingEnergy), data_sum);
//
//		summary_inv_node_X_inv_base(													// 汇总馈电总能量
//		 offsetof(INV_DATA_TYPE, FeedbackEnergy), MEMSIZE(INV_DATA_TYPE, FeedbackEnergy), data_sum);
//
//		summary_inv_node_X_inv_base(												// 汇总PV到AC负载的总能量
//		 offsetof(INV_DATA_TYPE, PvToACLoadEnergy), MEMSIZE(INV_DATA_TYPE, PvToACLoadEnergy), data_sum);
//
////	memcpy(inv_base.alarm, Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.alarm, sizeof(inv_base.alarm));		// 汇总告警标志
////	memcpy(inv_base.fault, Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.fault, sizeof(inv_base.fault));		// 汇总故障标志
////	inv_base.fault5 = Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_announce.fault5;							// 汇总故障标志5
////	inv_base.SelfConsumptionPercent = energy_self_consumption_ratio_get();						// 获取自给率
//	int offset = offsetof(INV_DATA_TYPE, alarm[0]);
//	int memsize = MEMSIZE(INV_DATA_TYPE, alarm[0]);
//	int item_size = sizeof(inv_base.alarm[0]);
//	for (int i = 0; i < sizeof(inv_base.alarm)/sizeof(inv_base.alarm[0]); i++)
//	{
//		int _offset = offset + (i * item_size);
//	summary_inv_node_X_inv_base( _offset, memsize, data_or_operate);		// 汇总告警标志
//	}
////	ESP_LOGI(TAG, "data_sum_inv_announce		4	");
//
//	offset = offsetof(INV_DATA_TYPE, fault[0]);
//	memsize = MEMSIZE(INV_DATA_TYPE, fault[0]);
//	item_size = sizeof(inv_base.fault[0]);
//	for (int i = 0; i < sizeof(inv_base.fault)/sizeof(inv_base.fault[0]); i++)
//	{
//		int _offset = offset + (i * item_size);
//		summary_inv_node_X_inv_base( _offset, memsize, data_or_operate);		// 汇总故障标志
//	}
//
//	summary_inv_node_X_inv_base(														// 汇总故障标志5
//		 offsetof(INV_DATA_TYPE, fault5), MEMSIZE(INV_DATA_TYPE, fault5), data_or_operate);
//
//	summary_inv_node_X_inv_base(												// 汇总PV到AC负载的总功率
//	 offsetof(INV_DATA_TYPE, PVToACloadPower), MEMSIZE(INV_DATA_TYPE, PVToACloadPower), data_sum);
//
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.line_event = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.line_event;		// 汇总能量线
//	// uint16_t line_event = Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.line_event;						// 保存能量线数据
//	// Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base = inv_base;												// 保存汇总数据
//
//	// /* 能量线数据来自外部能量线计算模块 */
//	// Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.line_event = line_event;									// 恢复能量线数据
//}
//
//void data_sum_inv_line(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_line.line_event = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_line.line_event;		// 汇总能量线
//}
//
//void data_sum_inv_about(void)
//{
//	judge_inv_min_version();// 判断每组系统里最小的arm、dsp、bms版本号，并将汇总后的最小版本号存入Inv[(DCDC_MAX_NUM)].mod_reg01100_Inv_base
//	// 假设此处待汇总的数据已经过排序比较，
////	ESP_LOGI(TAG, "reals.online_X_inv_index == %d, reals.online_Y_inv_index == %d",reals.online_X_inv_index,reals.online_Y_inv_index);
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about;	// 取在线设备数据
//
//	// memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvType), (uint8_t *)&Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.dev_type));// 1101~1106  ASCII 机型(单个)
//	// memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvSN), (uint8_t *)&Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.dev_sn));// 1107~1110  设备唯一识别SN码(单个)
//
//	// memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,inv_power_rang), (uint8_t *)&Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_base.inv_power_rang));// 1111(单机)
//
//	// memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,DevVoltageLable), (uint8_t *)&Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,voltage_lable),sizeof(Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.voltage_lable));// 1149
//	// memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,WorkTimeNumber), (uint8_t *)&Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,time_area_num),sizeof(Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.time_area_num));// 1148
//	// memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,software_total), (uint8_t *)&Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about + offsetof(inv_about_struct,software_total),sizeof(Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_about.software_total));// 1112
//
//
//
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvType), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about + offsetof(inv_about_struct,dev_type),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about.dev_type));// 1101~1106  ASCII 机型(单个)
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,InvSN), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about + offsetof(inv_about_struct,dev_sn),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about.dev_sn));// 1107~1110  设备唯一识别SN码(单个)
//
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,inv_power_rang), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base + offsetof(inv_base_struct,inv_power_rang),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_base.inv_power_rang));// 1111(单机)
//
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,DevVoltageLable), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about + offsetof(inv_about_struct,voltage_lable),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about.voltage_lable));// 1149
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,WorkTimeNumber), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about + offsetof(inv_about_struct,time_area_num),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about.time_area_num));// 1148
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg01100_Inv_base + offsetof(MOD_STRUCT_reg01100,software_total), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about + offsetof(inv_about_struct,software_total),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_about.software_total));// 1112
//
//}
//
//void data_sum_inv_pv(void)
//{
//	#undef INV_DATA_TYPE
//	#define INV_DATA_TYPE	inv_pv_struct
//
//	INV_DATA_TYPE inv_pv = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_pv;					// 取在线设备数据
//	// uint8_t *pdata = (uint8_t *)&Dcdc_can[0].inv_data[0].inv_pv;
//
//	#define DC_PV_CODE			100 	// 直流PV类型定义
//	#define AC_PV_CODE			101		// 交流PV类型定义
//
//	uint8_t pv_cnt = 0;
//	uint8_t pv_dc_nums = 0;
//	int max_pv_nums = sizeof(inv_pv.pv_detail) /  sizeof(inv_pv.pv_detail[0]);					// 最大PV数量
//	for (int node = 0; node < DCDC_MAX_NUM; node++)
//	{
//		for (int i = 0; i < DCDC_MAX_NUM; i++)
//		{
//			if (Dcdc_can[node].inv_data[i].online)
//			{
//				INV_DATA_TYPE *pv_data = &Dcdc_can[node].inv_data[i].inv_pv;
//				int pv_sum = pv_data->pv_number.ac_pv_numbers + pv_data->pv_number.dc_pv_numbers;
//				for (int k = 0; k < pv_sum && k < max_pv_nums; k++)
//				{
//					if (pv_data->pv_detail[k].input_type == DC_PV_CODE)
//					{
//						pv_dc_nums++;
//						Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_pv.pv_detail[pv_cnt++] = pv_data->pv_detail[k];					// 汇总DC PV详细数据
//						if (pv_cnt >= max_pv_nums) goto __end;
//					}
//				}
//			}
//		}
//	}
//
//	// pv_detail_struct ac_pv;
//	// ac_couple_meter_data_t *ac_meter = {0};//windy debug; ac_couple_meter_data_get();
//	// ac_pv.input_type = AC_PV_CODE;
//	// ac_pv.input_voltage = ac_meter->PhaseVoltageAN * 10;
//
//	// /* 汇总AC PV第一相数据 */
//	// if (ac_meter->WphA <= 0)
//	// {
//	// 	ac_pv.input_power = 0;
//	// 	ac_pv.input_current = 0;
//	// }
//	// else
//	// {
//	// 	ac_pv.input_power = ac_meter->WphA;
//	// 	if (ac_meter->PhaseVoltageAN == 0) {
//	// 		ac_pv.input_current = 0;
//	// 	}
//	// 	else {
//	// 		ac_pv.input_current = (uint16_t)((double)ac_meter->WphA * 10 / ac_meter->PhaseVoltageAN);
//	// 	}
//	// }
//
//	// if (ac_pv.input_power < 100) {
//	// 	ac_pv.status = 0;
//	// }
//	// else {
//	// 	ac_pv.status = 1;
//	// }
//	// inv_pv.pv_detail[pv_cnt++] = ac_pv;
//
//	// /* 汇总AC PV第二相数据 */
//	// if (ac_meter->WphB <= 0)
//	// {
//	// 	ac_pv.input_power = 0;
//	// 	ac_pv.input_current = 0;
//	// }
//	// else
//	// {
//	// 	ac_pv.input_power = ac_meter->WphB;
//	// 	if (ac_meter->PhaseVoltageAN == 0) {
//	// 		ac_pv.input_current = 0;
//	// 	}
//	// 	else {
//	// 		ac_pv.input_current = (uint16_t)((double)ac_meter->WphB * 10 / ac_meter->PhaseVoltageAN);
//	// 	}
//	// }
//
//	// if (ac_pv.input_power < 100) {
//	// 	ac_pv.status = 0;
//	// }
//	// else {
//	// 	ac_pv.status = 1;
//	// }
//	// inv_pv.pv_detail[pv_cnt++] = ac_pv;
//
//__end:
//	inv_pv.pv_number.ac_pv_numbers = 2;													// AC PV固定为两路
//	inv_pv.pv_number.dc_pv_numbers = pv_dc_nums;										// 汇总DC PV总数量
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_pv = inv_pv;	
//	
//	//单独调用汇总函数的，应该放在最后，目的为了防止被覆盖；
//	summary_inv_node_X_inv_pv(													// 汇总PV总充电功率
//		offsetof(INV_DATA_TYPE, total_chg_power), MEMSIZE(INV_DATA_TYPE, total_chg_power), data_sum);
//
//	summary_inv_node_X_inv_pv(													// 汇总PV总充电能量
//		offsetof(INV_DATA_TYPE, total_chg_energy), MEMSIZE(INV_DATA_TYPE, total_chg_energy), data_sum);
//									// 保存汇总数据
//}
//
//void data_sum_inv_grid(void)
//{
//	#undef INV_DATA_TYPE
//	#define INV_DATA_TYPE	inv_grid_struct
//
//	INV_DATA_TYPE inv_grid = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_grid;			// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].inv_data[0].inv_grid;
//
//	int offset = offsetof(INV_DATA_TYPE, grid_detail[0].input_power);
//	int memsize = MEMSIZE(INV_DATA_TYPE, grid_detail[0].input_power);
//	int item_size = sizeof(inv_grid.grid_detail[0].input_power) +
//					sizeof(inv_grid.grid_detail[0].input_voltage) +
//					sizeof(inv_grid.grid_detail[0].input_current);
//	for (int i = 0; i < sizeof(inv_grid.grid_detail)/sizeof(inv_grid.grid_detail[0]); i++)
//	{
//		int _offset = offset + (i * item_size);
//		summary_inv_node_X_inv_grid( _offset, memsize, data_sum);				// 汇总电网三相功率
//		summary_inv_node_X_inv_grid( _offset+memsize, memsize, data_max);	// 汇总电网三相电压
//		summary_inv_node_X_inv_grid( _offset+memsize*2, memsize, data_sum);	// 汇总电网三相电流
//	}
//
//	for (int i = 0; i < sizeof(inv_grid.grid_detail)/sizeof(inv_grid.grid_detail[0]); i++)
//	{
//		if (inv_grid.grid_detail[i].input_power < 150) {		// 电网单相功率低于150W不显示
//			inv_grid.grid_detail[i].input_power = 0;
//		}
//
//		if (inv_grid.grid_detail[i].input_voltage < 300) {		// 电网单相电压低于30V不显示
//			inv_grid.grid_detail[i].input_voltage = 0;
//		}
//
//		if (inv_grid.grid_detail[i].input_current < 5) {		// 电网单相电流低于0.5A不显示
//			inv_grid.grid_detail[i].input_current = 0;
//		}
//	}
//
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_grid = inv_grid;	
//	
//	summary_inv_node_X_inv_grid(												// 汇总电网总功率
//		 offsetof(INV_DATA_TYPE, total_chg_power), MEMSIZE(INV_DATA_TYPE, total_chg_power), data_sum);
//
//	summary_inv_node_X_inv_grid(												// 汇总电网总充电能量
//		 offsetof(INV_DATA_TYPE, total_chg_energy), MEMSIZE(INV_DATA_TYPE, total_chg_energy), data_sum);
//
//	summary_inv_node_X_inv_grid(												// 汇总电网总馈电能量
//		 offsetof(INV_DATA_TYPE, total_fb_energy), MEMSIZE(INV_DATA_TYPE, total_fb_energy), data_sum);
//											// 保存汇总数据
//}
//
//void data_sum_inv_load(void)
//{
//	#undef INV_DATA_TYPE
//	#define INV_DATA_TYPE	inv_load_struct
//
//	INV_DATA_TYPE inv_load = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_load;			// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].inv_data[0].inv_load;
//
//	int offset = offsetof(INV_DATA_TYPE, ac_load[0].load_power);
//	int memsize = MEMSIZE(INV_DATA_TYPE, ac_load[0].load_power);
//	int item_size = sizeof(inv_load.ac_load[0].load_power) +
//					sizeof(inv_load.ac_load[0].load_voltage) +
//					sizeof(inv_load.ac_load[0].load_current);
//	for (int i = 0; i < sizeof(inv_load.ac_load)/sizeof(inv_load.ac_load[0]); i++)
//	{
//		int _offset = offset + (i * item_size);
//		summary_inv_node_X_inv_load( _offset, memsize, data_sum);				// 汇总负载三相功率
//		summary_inv_node_X_inv_load( _offset+memsize, memsize, data_max);		// 汇总负载三相电压
//		summary_inv_node_X_inv_load( _offset+memsize*2, memsize, data_sum);	// 汇总负载三相电流
//	}
//
//	for (int i = 0; i < sizeof(inv_load.ac_load)/sizeof(inv_load.ac_load[0]); i++)
//	{
//		if (inv_load.ac_load[i].load_power < 150) {			// 电网单相功率低于150W不显示
//			inv_load.ac_load[i].load_power = 0;
//		}
//
//		if (inv_load.ac_load[i].load_voltage < 300) {		// 电网单相电压低于30V不显示
//			inv_load.ac_load[i].load_voltage = 0;
//		}
//
//		if (inv_load.ac_load[i].load_current < 5) {			// 电网单相电流低于0.5A不显示
//			inv_load.ac_load[i].load_current = 0;
//		}
//	}
//
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_load = inv_load;
//	
//	summary_inv_node_X_inv_load(											// 汇总DC负载总功率
//		 offsetof(INV_DATA_TYPE, total_dc_load_power), MEMSIZE(INV_DATA_TYPE, total_dc_load_power), data_sum);
//
//	summary_inv_node_X_inv_load(											// 汇总DC负载总能量
//		 offsetof(INV_DATA_TYPE, total_dc_load_energy), MEMSIZE(INV_DATA_TYPE, total_dc_load_energy), data_sum);
//
//	summary_inv_node_X_inv_load(												// 汇总5V直流负载功率
//		 offsetof(INV_DATA_TYPE, dc_05v_load_power), MEMSIZE(INV_DATA_TYPE, dc_05v_load_power), data_sum);
//
//	summary_inv_node_X_inv_load(											// 汇总5V直流负载电流
//		 offsetof(INV_DATA_TYPE, dc_05v_load_current), MEMSIZE(INV_DATA_TYPE, dc_05v_load_current), data_sum);
//
//	summary_inv_node_X_inv_load(												// 汇总12V直流负载功率
//		 offsetof(INV_DATA_TYPE, dc_12v_load_power), MEMSIZE(INV_DATA_TYPE, dc_12v_load_power), data_sum);
//
//	summary_inv_node_X_inv_load(											// 汇总12V直流负载电流
//		 offsetof(INV_DATA_TYPE, dc_12v_load_current), MEMSIZE(INV_DATA_TYPE, dc_12v_load_current), data_sum);
//
//	summary_inv_node_X_inv_load(												// 汇总24V直流负载功率
//		 offsetof(INV_DATA_TYPE, dc_24v_load_power), MEMSIZE(INV_DATA_TYPE, dc_24v_load_power), data_sum);
//
//	summary_inv_node_X_inv_load(											// 汇总24V直流负载电流
//		 offsetof(INV_DATA_TYPE, dc_24v_load_current), MEMSIZE(INV_DATA_TYPE, dc_24v_load_current), data_sum);
//
//	summary_inv_node_X_inv_load(											// 汇总AC负载总功率
//		 offsetof(INV_DATA_TYPE, total_ac_load_power), MEMSIZE(INV_DATA_TYPE, total_ac_load_power), data_sum);
//
//	summary_inv_node_X_inv_load(											// 汇总AC负载总能量
//		 offsetof(INV_DATA_TYPE, total_ac_load_energy), MEMSIZE(INV_DATA_TYPE, total_ac_load_energy), data_sum);
//												// 保存汇总数据
//}
//
//void data_sum_inv_data(void)
//{
//	#undef INV_DATA_TYPE
//	#define INV_DATA_TYPE	inv_data_struct
//
//	INV_DATA_TYPE inv_data = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_data;		// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].inv_data[0].inv_data;
//
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_data = inv_data;
//
//	int offset = offsetof(INV_DATA_TYPE, inv_detail[0].work_status);
//	for (int i = 0; i < sizeof(inv_data.inv_detail)/sizeof(inv_data.inv_detail[0]); i++)		// 汇总逆变详细信息
//	{
//		// inv_data.inv_detail[i].work_status = summary_inv_node(pdata, offset, sizeof(inv_data.inv_detail[0].work_status), data_sum);
//		summary_inv_node_X_inv_data( offset+=sizeof(inv_data.inv_detail[0].work_status), sizeof(inv_data.inv_detail[0].power), data_sum);
//		summary_inv_node_X_inv_data( offset+=sizeof(inv_data.inv_detail[0].power), sizeof(inv_data.inv_detail[0].voltage), data_max);
//		summary_inv_node_X_inv_data( offset+=sizeof(inv_data.inv_detail[0].voltage), sizeof(inv_data.inv_detail[0].current), data_sum);
//		offset += sizeof(inv_data.inv_detail[0].current);
//	}
//
//	
//	summary_inv_node_X_inv_data(													// 汇总逆变总能量
//		 offsetof(INV_DATA_TYPE, total_energy), MEMSIZE(INV_DATA_TYPE, total_energy), data_sum);
//	// ESP_LOGI(TAG,"Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_data. is %d");							
//}
//
//void data_sum_inv_meter(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_meter = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_meter;	// 取在线设备数据
//}
//
//void data_sum_inv_gen(void)
//{
//	#undef INV_DATA_TYPE
//	#define INV_DATA_TYPE	inv_gen_struct
//
//	INV_DATA_TYPE inv_gen = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_gen;					// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].inv_data[0].inv_gen;
//
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_gen = inv_gen;	
//	
//	summary_inv_node_X_inv_gen(													// 汇总发电机总能量
//		 offsetof(INV_DATA_TYPE, total_energy), MEMSIZE(INV_DATA_TYPE, total_energy), data_sum);
//
//	summary_inv_node_X_inv_gen(															// 汇总发电机总功率
//		 offsetof(INV_DATA_TYPE, power), MEMSIZE(INV_DATA_TYPE, power), data_sum);
//
//	summary_inv_node_X_inv_gen(															// 汇总发电机电压
//		 offsetof(INV_DATA_TYPE, voltage), MEMSIZE(INV_DATA_TYPE, voltage), data_max);
//
//	summary_inv_node_X_inv_gen(															// 汇总发电机电流
//		 offsetof(INV_DATA_TYPE, current), MEMSIZE(INV_DATA_TYPE, current), data_sum);
//											// 保存汇总数据
//}
//
//void data_sum_inv_base_param(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set00;	// 取在线设备数据
//	
//	// modbus 2000, 1A段汇总 Dcdc_can -> top_modbus_rd.Dcdc
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,mon), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,time1),(offsetof(inv_set00_struct,work_mode) - offsetof(inv_set00_struct,time1)));// 2000~2004
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,work_mode), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,work_mode),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.work_mode));// 2005
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl));// 2006
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_led), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_led),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_led));// 2007
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_meter), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_meter),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_meter));// 2008
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_pv), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_pv),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_pv));// 2009
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_inv),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_inv));// 2010
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_ac),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_ac));// 2011
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dc),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_dc));// 2012
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_poweron), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_poweron),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_poweron));// 2013
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_dc_eco));// 2014
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco_time), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco_time),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_dc_eco_time));// 2015
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_dc_power_value), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,eco_dc_power_value),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.eco_dc_power_value));// 2016
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_ac_eco));// 2017
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco_time), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco_time),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_ac_eco_time));// 2018
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_ac_power_value), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,eco_ac_power_value),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.eco_ac_power_value));// 2019
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_chg_mode), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_chg_mode),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_chg_mode));// 2020
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_super_power), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_super_power),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_super_power));// 2021
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_low_cap_pct), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_low_cap_pct),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_low_cap_pct));// 2022
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_high_cap_pct), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_high_cap_pct),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_high_cap_pct));// 2023
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv_mode), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_inv_mode),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_inv_mode));// 2024
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dev_id), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_dev_id),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_dev_id));// 2025
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_all_energy_type), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_all_energy_type),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_all_energy_type));// 2026
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_now_energy_type), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_now_energy_type),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_now_energy_type));// 2027
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_log_page), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_log_page),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_log_page));// 2028
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time_area), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_time_area),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_time_area));// 2029
//	for(int i = 0;i < 10;i++)// 2030~2059
//	{
//		memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].lable, (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_time[i].lable,sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_time[i].lable));
//		memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].start, (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_time[i].start,sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_time[i].start));
//		memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_time[i].end, (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_time[i].end,sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_time[i].end));
//	}
//	for(int i = 0;i < 6;i++)// 2060~2065
//	{
//		memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set.ctrl_PvType[i], (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_PvType[i],sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_PvType[i]));
//	}
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_alarm_voice), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,ctrl_alarm_voice),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_alarm_voice));// 2066
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_lcd_active_time), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00 + offsetof(inv_set00_struct,setLcdActiveTime),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.setLcdActiveTime));// 2067
//
//
//	inv_advance_param_t *advance_param = &InvSetData.dev_info_t2.inv_set01;//ll_inv_param_get("inv_advance_param");
//	if ((NULL!=  advance_param))
//		// &&(advance_param->ctrl_mix.bit.multi_inv_off_grid_en == 1))						// 并机开关打开时才检测底层设备的AC状态
//	{
//		uint8_t ctrl_ac_tmp = 0;
//		for (int node = 0; node < DCDC_MAX_NUM; node++)
//		{
//			for (int i = 0; i < DCDC_MAX_NUM; i++)
//			{
//				if (!Dcdc_can[node].inv_data[i].online) continue;
//				ctrl_ac_tmp = 1;
//
//				/* 并机模式下,只要系统中有一台逆变的系统总开关是关闭的,则设置整个系统的总开关状态是关闭的 */
//				inv_set00_struct *inv_set00 = &Dcdc_can[node].inv_data[i].inv_set00;
//				if (inv_set00->ctrl_ac == 0)
//				{
//					ctrl_ac_tmp = 0;
//					goto __exit;
//				}
//			}
//		}
//
//	__exit:
//		Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set00.ctrl_ac = ctrl_ac_tmp;
//	}
//}
//
//void data_sum_inv_advance_param(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set01;	// 取在线设备数据
//
//	// modbus 2200, 1B段汇总
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,password), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,password),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.password));// 2200~2203
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_reset_factory), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_reset_factory),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_reset_factory));// 2206
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_grid),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_grid));// 2207
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_feedback), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_feedback),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_feedback));// 2208
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_output_inv_volt), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_output_inv_volt),(offsetof(inv_set01_struct,ctrl_user_area) - offsetof(inv_set01_struct,ctrl_output_inv_volt)));// 2209~2217汇总
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_user_area), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_user_area),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_user_area));// 2218
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_pv_paralle));// 2219~2224
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid_plus), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_grid_plus),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_grid_plus));// 2225汇总
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_save_power_state), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_save_power_state),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_save_power_state));// 2226
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_save_power_state), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_save_power_state),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_save_power_state));// 2226
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_enable), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_meter_enable),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_meter_enable));// 2227
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_select), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_meter_select),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_meter_select));// 2228
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_Multi_enable), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_Multi_enable),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_Inv_Multi_enable));// 2229
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_addr_Set),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_Inv_addr_Set));// 2230
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_test), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ct_test),(offsetof(inv_set01_struct,ctrl_mix2) - offsetof(inv_set01_struct,ct_test)));// 2231~2242
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_mix2), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ctrl_mix2),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ctrl_mix2));// 2242
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ChargingPile_SET), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ChargingPile_SET),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ChargingPile_SET));// 2243
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_ratio), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,ct_ratio),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.ct_ratio));// 2244
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,GenSet), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,GenSet),(offsetof(inv_set01_struct,Undervoltage_protection) - offsetof(inv_set01_struct,GenSet)));// 2246~2257	res1
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Undervoltage_protection));// 2258
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection_time), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection_time),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Undervoltage_protection_time));// 2259
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Highvoltage_protection));// 2260
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection_time), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection_time),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Highvoltage_protection_time));// 2261
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Underfrequency_protection));// 2262
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection_time), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection_time),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Underfrequency_protection_time));// 2263
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Overvoltage_protection));// 2264
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection_time), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection_time),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Overvoltage_protection_time));// 2265
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetCtrlPv), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,setting_pv),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.setting_pv));// 2269
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Phase_set), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,Phase_set),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.Phase_set));// 2270
//	memcpy((uint8_t *)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,DCHUB_set), (uint8_t *)&Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01 + offsetof(inv_set01_struct,DCHUB_set),sizeof(Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set01.DCHUB_set));// 2271
//}
//
//void data_sum_inv_auth_param(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_set03 = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_set03;	// 取在线设备数据
//}
//
//void data_sum_inv_log(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_log = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_log;		// 取在线设备数据
//}
//
//void data_sum_inv_year_energy(void)
//{
//	// 由IOT计算,不再根据底层设备的数据汇总
//}
//
//void data_sum_inv_now_energy(void)
//{
//	// 由IOT计算,不再根据底层设备的数据汇总
//}
//
//void data_sum_inv_wifi(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].inv_wifi = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].inv_wifi;	// 取在线设备数据
//}
//
//void data_sum_inv_wifi_report(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].wifi_report = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].wifi_report;
//}
//
//void data_sum_inv_wifi_auth_param(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].inv_data[DCDC_MAX_NUM].wifi_param = Dcdc_can[reals.online_X_inv_index].inv_data[reals.online_Y_inv_index].wifi_param;
//}
//
//void data_sum_inv_dc_hub_info(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].dc_hub_data[DCDC_MAX_NUM].dc_hub_info = Dcdc_can[reals.online_X_inv_index].dc_hub_data[reals.online_Y_inv_index].dc_hub_info;
//}
//
//void data_sum_inv_ac_hub_info(void)
//{
//	Inv_can_mix.ac_hub_data[DCDC_MAX_NUM].ac_hub_info = Inv_can_mix.ac_hub_data[reals.online_Y_inv_index].ac_hub_info;
//}
//
//void data_sum_pack_announce(void)
//{
//	#undef PACK_DATA_TYPE
//	#define PACK_DATA_TYPE	pack_announce_struct
//
//	PACK_DATA_TYPE pack_announce = Dcdc_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_announce;	// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].pack_data[0].pack_announce;
//
//	pack_announce.total_voltage = summary_Pack_node_X_pack_announce(										// 汇总电压
//		 offsetof(PACK_DATA_TYPE, total_voltage), MEMSIZE(PACK_DATA_TYPE, total_voltage), data_max);
//
//	pack_announce.total_current = summary_Pack_node_X_pack_announce(										// 汇总电流
//		 offsetof(PACK_DATA_TYPE, total_current), MEMSIZE(PACK_DATA_TYPE, total_current), data_sum);
//
//	uint8_t chg_status;
//	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)								// 汇总电池状态
//	{
//		if (Dcdc_can[node_id].pack_data[0].pack_announce.online & 0x01)
//		{
//			chg_status = Dcdc_can[node_id].pack_data[0].pack_announce.chg_status;
//			if (chg_status != 0) 
//				break;
//		}
//	}
//	pack_announce.chg_status = chg_status;
//
//	uint32_t pack_sum_soc = 0;
//	uint32_t pack_sum_soc2 = 0;
//	uint8_t pack_nums = 0;
//
//	/* 不能直接用电池主机的SOC相加后除以主机数量得到平均SOC,因为电池主机带的电池包数量可能是不相同的 */
//	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)								// 汇总SOC与SOC2
//	{
//		if (Dcdc_can[node_id].pack_data[0].pack_announce.online & (1 << 0))
//		{
//			uint8_t pack_soc = Dcdc_can[node_id].pack_data[0].pack_announce.soc;
//			uint8_t pack_soc2 = Dcdc_can[node_id].pack_data[0].pack_announce.soc2;
//			uint8_t pack_cnt = Dcdc_can[node_id].pack_data[0].pack_announce.pack_cnt;
//			pack_nums += pack_cnt;
//			pack_sum_soc += (pack_soc * pack_cnt);
//			pack_sum_soc2 += (pack_soc2 * pack_cnt);
//		}
//	}
//	if (pack_nums)
//	{
//		pack_announce.soc = pack_sum_soc / pack_nums;
//		pack_announce.soc2 = pack_sum_soc2 / pack_nums;
//	}
//
//	pack_announce.avg_temp = summary_Pack_node_X_pack_announce(												// 汇总平均温度
//		 offsetof(PACK_DATA_TYPE, avg_temp), MEMSIZE(PACK_DATA_TYPE, avg_temp), data_average_pack);
//
//	pack_announce.max_chg_voltage = summary_Pack_node_X_pack_announce(										// 汇总最大充电电压
//		 offsetof(PACK_DATA_TYPE, max_chg_voltage), MEMSIZE(PACK_DATA_TYPE, max_chg_voltage), data_min);
//
//	pack_announce.max_chg_current = summary_Pack_node_X_pack_announce(										// 汇总最大充电电流
//		 offsetof(PACK_DATA_TYPE, max_chg_current), MEMSIZE(PACK_DATA_TYPE, max_chg_current), data_min);
//
//	pack_announce.max_dsg_current = summary_Pack_node_X_pack_announce(										// 汇总最大放电电流
//		 offsetof(PACK_DATA_TYPE, max_dsg_current), MEMSIZE(PACK_DATA_TYPE, max_dsg_current), data_min);
//
//	pack_announce.status1.all = summary_Pack_node_X_pack_announce(											// 汇总状态1
//		 offsetof(PACK_DATA_TYPE, status1.all), MEMSIZE(PACK_DATA_TYPE, status1.all), data_or_operate);
//
//	pack_announce.status2.all = summary_Pack_node_X_pack_announce(											// 汇总状态2
//		 offsetof(PACK_DATA_TYPE, status2.all), MEMSIZE(PACK_DATA_TYPE, status2.all), data_or_operate);
//
//	pack_announce.soh = summary_Pack_node_X_pack_announce(													// 汇总SOH
//		 offsetof(PACK_DATA_TYPE, soh), MEMSIZE(PACK_DATA_TYPE, soh), data_average_pack);
//
////	uint16_t online = 0;
//	uint16_t cnt = 0;
////	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)								// 汇总在线标志
////	{
////		for (int i = 0; i < PACK_MAX_NUM; i++)
////		{
////			if (Dcdc_can[node_id].pack_data[0].pack_announce.online & (1 << i)) {
////				online |= (1 << (cnt++));
////			}
////		}
////	}
//	pack_announce.online = reals.online_Pack_num;//3*24溢出，不能用bit表示，改总数量；online;
//
//	uint16_t cap_online = 0;
//	cnt = 0;
//	for (int node_id = 0; node_id < DCDC_MAX_NUM; node_id++)								// 汇总mos输出标志
//	{
//		for (int i = 0; i < PACK_MAX_NUM; i++)
//		{
//			if (Dcdc_can[node_id].pack_data[0].pack_announce.cap_online & (1 << i)) {
//				cap_online |= (1 << (cnt++));
//			}
//		}
//	}
//	pack_announce.cap_online = cap_online;
//
//	pack_announce.pack_cnt = summary_Pack_node_X_pack_announce(												// 汇总电池包数量
//		 offsetof(PACK_DATA_TYPE, pack_cnt), MEMSIZE(PACK_DATA_TYPE, pack_cnt), data_sum);
//
//	pack_announce.chg_full_time = summary_Pack_node_X_pack_announce(										// 汇总预计充满时间
//		 offsetof(PACK_DATA_TYPE, chg_full_time), MEMSIZE(PACK_DATA_TYPE, chg_full_time), data_max);
//
//	pack_announce.dsg_empty_time = summary_Pack_node_X_pack_announce(										// 汇总预计放空时间
//		 offsetof(PACK_DATA_TYPE, dsg_empty_time), MEMSIZE(PACK_DATA_TYPE, dsg_empty_time), data_max);
//
//	pack_announce.TotalCurrent_bias = summary_Pack_node_X_pack_announce(									// 汇总总电流(基准偏移30000)
//		 offsetof(PACK_DATA_TYPE, TotalCurrent_bias), MEMSIZE(PACK_DATA_TYPE, TotalCurrent_bias), data_average_pack);
//
//	pack_announce.cell_min_volt = summary_Pack_node_X_pack_announce(										// 汇总最小电芯电压
//		 offsetof(PACK_DATA_TYPE, cell_min_volt), MEMSIZE(PACK_DATA_TYPE, cell_min_volt), data_min);
//
//	pack_announce.cell_max_volt = summary_Pack_node_X_pack_announce(										// 汇总最大电芯电压
//		 offsetof(PACK_DATA_TYPE, cell_max_volt), MEMSIZE(PACK_DATA_TYPE, cell_max_volt), data_min);
//
//	pack_announce.protect_status = summary_Pack_node_X_pack_announce(										// 汇总保护状态
//		 offsetof(PACK_DATA_TYPE, protect_status), MEMSIZE(PACK_DATA_TYPE, protect_status), data_min);
//
//	pack_announce.aging_current = summary_Pack_node_X_pack_announce(										// 汇总老化电流
//		 offsetof(PACK_DATA_TYPE, aging_current), MEMSIZE(PACK_DATA_TYPE, aging_current), data_sum);
//
//	Dcdc_can[DCDC_MAX_NUM].pack_data[PACK_MAX_NUM].pack_announce = pack_announce;									// 保存汇总数据
//}
//
//void data_sum_pack_base(void)
//{
//	#undef PACK_DATA_TYPE
//	#define PACK_DATA_TYPE	pack_base_struct
//
//	PACK_DATA_TYPE pack_base = Dcdc_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_base;			// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].pack_data[0].pack_base;
//
//	pack_base.total_voltage = summary_Pack_node_X_pack_base(												// 汇总电压
//		 offsetof(PACK_DATA_TYPE, total_voltage), MEMSIZE(PACK_DATA_TYPE, total_voltage), data_max);
//
//	pack_base.total_current = summary_Pack_node_X_pack_base(												// 汇总电流
//		 offsetof(PACK_DATA_TYPE, total_current), MEMSIZE(PACK_DATA_TYPE, total_current), data_sum);
//
//	pack_base.soc = summary_Pack_node_X_pack_base(												// 汇总soc
//		 offsetof(PACK_DATA_TYPE, soc), MEMSIZE(PACK_DATA_TYPE, soc), data_sum);
//
//	// pack_base.soc = Dcdc_can[DCDC_MAX_NUM].pack_data[PACK_MAX_NUM].pack_announce.soc;								// 汇总soc
//
//	pack_base.soh = summary_Pack_node_X_pack_base(															// 汇总soh
//		 offsetof(PACK_DATA_TYPE, soh), MEMSIZE(PACK_DATA_TYPE, soh), data_average_pack);
////	ESP_LOGI(TAG, "summary pack soh == %d\n", pack_base.soh);
//
//	pack_base.avg_temp = summary_Pack_node_X_pack_base(														// 汇总平均温度
//		 offsetof(PACK_DATA_TYPE, avg_temp), MEMSIZE(PACK_DATA_TYPE, avg_temp), data_average_pack);
//
//	pack_base.min_cell_voltage = summary_Pack_node_X_pack_base(												// 汇总最小电芯电压
//		 offsetof(PACK_DATA_TYPE, min_cell_voltage), MEMSIZE(PACK_DATA_TYPE, min_cell_voltage), data_min);
//
//	pack_base.max_cell_voltage = summary_Pack_node_X_pack_base(												// 汇总最大电芯电压
//		 offsetof(PACK_DATA_TYPE, max_cell_voltage), MEMSIZE(PACK_DATA_TYPE, max_cell_voltage), data_max);
//
//	pack_base.min_temp_value = summary_Pack_node_X_pack_base(												// 汇总最低温度
//		 offsetof(PACK_DATA_TYPE, min_temp_value), MEMSIZE(PACK_DATA_TYPE, min_temp_value), data_min);
//
//	pack_base.max_temp_value = summary_Pack_node_X_pack_base(												// 汇总最高温度
//		 offsetof(PACK_DATA_TYPE, max_temp_value), MEMSIZE(PACK_DATA_TYPE, max_temp_value), data_max);
//
//	int offset = offsetof(PACK_DATA_TYPE, protect[0]);
//	for (int i = 0; i < sizeof(pack_base.protect)/sizeof(pack_base.protect[0]); i++)			// 汇总保护信息
//	{
//		pack_base.protect[i] = summary_Pack_node_X_pack_base( offset, sizeof(pack_base.protect[0]), data_or_operate);
//		offset += sizeof(pack_base.protect[0]);
//	}
//
//	offset = offsetof(PACK_DATA_TYPE, fault[0]);
//	for (int i = 0; i < sizeof(pack_base.fault)/sizeof(pack_base.fault[0]); i++)				// 汇总故障信息
//	{
//		pack_base.fault[i] = summary_Pack_node_X_pack_base( offset, sizeof(pack_base.fault[0]), data_or_operate);
//		offset += sizeof(pack_base.fault[0]);
//	}
//
//	offset = offsetof(PACK_DATA_TYPE, alarm[0]);
//	for (int i = 0; i < sizeof(pack_base.alarm)/sizeof(pack_base.alarm[0]); i++)				// 汇总告警信息
//	{
//		pack_base.alarm[i] = summary_Pack_node_X_pack_base( offset, sizeof(pack_base.alarm[0]), data_or_operate);
//		offset += sizeof(pack_base.alarm[0]);
//	}
//
//	pack_base.relay = summary_Pack_node_X_pack_base(														// 汇总继电器状态
//		 offsetof(PACK_DATA_TYPE, relay), MEMSIZE(PACK_DATA_TYPE, relay), data_or_operate);
//
//	pack_base.pack_total_cell = summary_Pack_node_X_pack_base(												// 汇总电芯总数
//		 offsetof(PACK_DATA_TYPE, pack_total_cell), MEMSIZE(PACK_DATA_TYPE, pack_total_cell), data_sum);
//
//	pack_base.pack_total_ntc = summary_Pack_node_X_pack_base(												// 汇总NTC总数
//		 offsetof(PACK_DATA_TYPE, pack_total_ntc), MEMSIZE(PACK_DATA_TYPE, pack_total_ntc), data_sum);
//
//	pack_base.chg_full_time = summary_Pack_node_X_pack_base(												// 汇总预计充满时间
//		 offsetof(PACK_DATA_TYPE, chg_full_time), MEMSIZE(PACK_DATA_TYPE, chg_full_time), data_max);
//
//	pack_base.dsg_empty_time = summary_Pack_node_X_pack_base(												// 汇总预计放空时间
//		 offsetof(PACK_DATA_TYPE, dsg_empty_time), MEMSIZE(PACK_DATA_TYPE, dsg_empty_time), data_max);
//
//	Dcdc_can[DCDC_MAX_NUM].pack_data[PACK_MAX_NUM].pack_base = pack_base;											// 保存汇总数据
//}
//
//void data_sum_pack_extend(void)
//{
//	#undef PACK_DATA_TYPE
//	#define PACK_DATA_TYPE	pack_extend_struct
//
//	PACK_DATA_TYPE pack_extend = Dcdc_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_extend;	// 取在线设备数据
////	uint8_t *pdata = (uint8_t *)&Dcdc_can[0].pack_data[0].pack_extend;
//
//	pack_extend.capacity = summary_Pack_node_X_pack_extend(													// 汇总容量
//		 offsetof(PACK_DATA_TYPE, capacity), MEMSIZE(PACK_DATA_TYPE, capacity), data_sum);
//
//	pack_extend.total_chg_energy = summary_Pack_node_X_pack_extend(											// 汇总累积充电能量
//		 offsetof(PACK_DATA_TYPE, total_chg_energy), MEMSIZE(PACK_DATA_TYPE, total_chg_energy), data_sum);
//
//	pack_extend.total_dsg_energy = summary_Pack_node_X_pack_extend(											// 汇总累积放电能量
//		 offsetof(PACK_DATA_TYPE, total_dsg_energy), MEMSIZE(PACK_DATA_TYPE, total_dsg_energy), data_sum);
//
//	pack_extend.once_chg_energy = summary_Pack_node_X_pack_extend(											// 汇总当前充电能量
//		 offsetof(PACK_DATA_TYPE, once_chg_energy), MEMSIZE(PACK_DATA_TYPE, once_chg_energy), data_sum);
//
//	pack_extend.once_dsg_energy = summary_Pack_node_X_pack_extend(											// 汇总当前放电能量
//		 offsetof(PACK_DATA_TYPE, once_dsg_energy), MEMSIZE(PACK_DATA_TYPE, once_dsg_energy), data_sum);
//
//	pack_extend.sir_value = summary_Pack_node_X_pack_extend(													// 汇总绝缘内阻
//		 offsetof(PACK_DATA_TYPE, sir_value), MEMSIZE(PACK_DATA_TYPE, sir_value), data_average_pack);
//
//	pack_extend.inner_value = summary_Pack_node_X_pack_extend(												// 汇总电池包内阻
//		 offsetof(PACK_DATA_TYPE, inner_value), MEMSIZE(PACK_DATA_TYPE, inner_value), data_average_pack);
//
//	pack_extend.cycle_count = summary_Pack_node_X_pack_extend(												// 汇总循环次数
//		 offsetof(PACK_DATA_TYPE, cycle_count), MEMSIZE(PACK_DATA_TYPE, cycle_count), data_average_pack);
//
//	Dcdc_can[DCDC_MAX_NUM].pack_data[PACK_MAX_NUM].pack_extend = pack_extend;										// 保存汇总数据
//}
//
//void data_sum_pack_about(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].pack_data[PACK_MAX_NUM].pack_about = Dcdc_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_about;	// 取在线设备数据
//}
//
//void data_sum_pack_config(void)
//{
//	Dcdc_can[DCDC_MAX_NUM].pack_data[PACK_MAX_NUM].pack_config = Dcdc_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_config; // 取在线设备数据
//}
//
//void data_sum_pack_debug(void)
//{
//	for (int node = 0; node < DCDC_MAX_NUM; node++)
//	{
//		for (int i = 0; i < PACK_MAX_NUM; i++)
//		{
//			if (Dcdc_can[node].pack_data[0].pack_announce.online & (1 << i))
//			{
//				Dcdc_can[node].pack_data[i].pack_debug1.cells = Dcdc_can[node].pack_data[i].pack_base.pack_total_cell;
//				Dcdc_can[node].pack_data[i].pack_debug1.ntc_cnt = Dcdc_can[node].pack_data[i].pack_base.pack_total_ntc;
//			}
//		}
//	}
//	Dcdc_can[DCDC_MAX_NUM].pack_data[PACK_MAX_NUM].pack_debug1 = Dcdc_can[reals.online_X_bat_index].pack_data[reals.online_Y_bat_index].pack_debug1; // 取在线设备数据
//}
//








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
//	data_sum_inv_announce();
//	data_sum_inv_base();
//	data_sum_inv_line();
//	data_sum_inv_about();
//	data_sum_inv_pv();
//	data_sum_inv_grid();
//	data_sum_inv_load();
//	data_sum_inv_data();
//	data_sum_inv_meter();
//	data_sum_inv_gen();
//	data_sum_inv_base_param();
//	data_sum_inv_advance_param();
//	data_sum_inv_auth_param();
//	data_sum_inv_log();
//	data_sum_inv_year_energy();
//	data_sum_inv_now_energy();
//	data_sum_inv_wifi();
//	data_sum_inv_wifi_report();
//	data_sum_inv_wifi_auth_param();
//	data_sum_inv_dc_hub_info();
//	data_sum_inv_ac_hub_info();
}

/*
windy 修改二维求和
Inv_can[i].inv_data[y]

*/
static uint32_t summary_inv_node_y(uint8_t index_x, uint8_t *pdata, int offset, int width, data_operate operate)
{
// 	uint32_t buff[DCDC_MAX_NUM];
// 	uint8_t i = 0;
// 	uint8_t *pdata2;
// 	uint32_t rtn = 0;

// //	ESP_LOGI(TAG, "summary_inv_node_y	      1");

// 	/*求和二维数据*/
// //	for (index_x ; index_x < DCDC_MAX_NUM; index_x++)
// //	{
// 		pdata2 = pdata + (index_x * sizeof(can_node_struct_Inv)); // 指针指向下一个inv节点数据
// 		if (can_node_rd.Inv[index_x].online)
// 		{	
// 			if(width == 1) {
// 				buff[index_x] = (uint32_t)(*((uint8_t*)(pdata2 + offset)));
// 			}
// 			else if(width == 2) {
// 				buff[index_x] = (uint32_t)(*((uint16_t*)(pdata2 + offset)));
// 			}
// 			else if(width == 4) {		
// 				buff[index_x] = (uint32_t)(*((uint32_t*)(pdata2 + offset)));
// 			}
// 		}
// 		else
// 		{
// 			buff[index_x] =0;
// 		}
		
// //	}
// 	rtn =operate(buff, index_x);
	
// 	pdata2 = pdata + ((uint32_t)index_x* sizeof(can_node_struct_Inv)) + offset;		
// 	memcpy(pdata2, (uint8_t *)&rtn, width);//尾部数组是汇总

// 	if(index_x != 0) 
// 	{
// 		return rtn;
// 	}

	return 0;
}


/*
windy 修改二维求和
Inv_can[x]


*/
static uint32_t summary_inv_node_X_inv_base( int offset, int width, data_operate operate)
{
	uint8_t i = 0;
	uint32_t buff[INV_MAX_NUM];  //求和结果
	uint8_t *pdata;				//求和对象
	uint32_t rtn = 0;
	// uint8_t *pdata2;

	/*遍历所有dcdc结构体取值*/
	for (i = 0; i < INV_MAX_NUM; i++)
	{
		// pdata = (uint8_t *)&Inv_can[i].inv_data[0].inv_base + ((uint16_t)i * sizeof(MOD_STRUCT_Inv_READ));	// 指针指向下一个inv节点数据
		pdata = (uint8_t *)&can_node_rd.Inv[i].inv_base;
	
//		buff[i]= summary_inv_node_y(i,pdata,offset,width, operate);
//		pdata2 = pdata + (i * sizeof(inv_node_struct)); // 指针指向下一个inv节点数据
		if (can_node_rd.Inv[i].online)
		{	
			if(width == 1) {
				buff[i] = (uint32_t)(*((uint8_t*)(pdata + offset)));
			}
			else if(width == 2) {
				buff[i] = (uint32_t)(*((uint16_t*)(pdata + offset)));
			}
			else if(width == 4) {		
				buff[i] = (uint32_t)(*((uint32_t*)(pdata + offset)));
			}
		}
		else
		{
			buff[i] =0;
		}
//		ESP_LOGI(TAG, "summary_inv_node_x11111111111111,(uint32_t)buff[%d]=%lu  ",i,(uint32_t)buff[i]);

	}

	/*数据求和*/
	rtn =operate(buff, i);

//	ESP_LOGI(TAG, "summary_inv_node_x2222222222222222, rtn is =%lu  ",rtn);
	
	//汇总的汇总存储在二维数组的全尾部数组 [x+1][y+1]
	pdata = (uint8_t *)&can_node_rd.Inv[i].inv_base + offset;
	memcpy(pdata, (uint8_t *)&rtn, width);//尾部数组是汇总
//	ESP_LOGI(TAG, "summary_inv_node_y		9,(uint32_t)pdata=0x%lx  ",(uint32_t)pdata);

	if(i != 0) 
	{
		return operate(buff, i);
	}

	return 0;
}


/**
 * @brief DCDC数据汇总
 *
 * @return 无
 */
void data_sum_dcdc(void)
{
	#undef DCDC_DATA_TYPE
	#define DCDC_DATA_TYPE	iot_dcdc_15500_can_struct
	
	/*DCDC设备不在线时退出*/
	if(1 == reals.online_Dcdc_num)
	{
		// memcpy((uint8_t *)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg01100_Inv_base, (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base,sizeof(MOD_STRUCT_reg01100));
		// memcpy((uint8_t *)&can_node_rd.Inv[(INV_MAX_NUM)].inv_base, (uint8_t *)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base,sizeof(MOD_STRUCT_reg01100));
//		return;		// 没有设备在线直接返回
	}
	else if(reals.online_Dcdc_num > 0)
	{
			/*处理 15500 DCDC信息区汇总数据*/		
			summary_inv_node_X_inv_base(											// 汇总DC负载总功率
			 offsetof(DCDC_DATA_TYPE, TotalInputPower), MEMSIZE(DCDC_DATA_TYPE, TotalInputPower), data_sum);
		
			summary_inv_node_X_inv_base(											// 汇总AC负载总功率
			 offsetof(DCDC_DATA_TYPE, TotalOutputPower), MEMSIZE(DCDC_DATA_TYPE, TotalOutputPower), data_sum);
		
			summary_inv_node_X_inv_base(												// 汇总PV总功率
			 offsetof(DCDC_DATA_TYPE, DC1Power), MEMSIZE(DCDC_DATA_TYPE, DC1Power), data_sum);
		
			summary_inv_node_X_inv_base(												// 汇总电网总功率
			 offsetof(DCDC_DATA_TYPE, DC2Power), MEMSIZE(DCDC_DATA_TYPE, DC2Power), data_sum);
		
			summary_inv_node_X_inv_base(												// 汇总逆变总功率
			 offsetof(DCDC_DATA_TYPE, DC3Power), MEMSIZE(DCDC_DATA_TYPE, DC3Power), data_sum);
		
			summary_inv_node_X_inv_base(												// 汇总DC负载总能量
			 offsetof(DCDC_DATA_TYPE, DC4Power), MEMSIZE(DCDC_DATA_TYPE, DC4Power), data_sum);
		
			summary_inv_node_X_inv_base(												// 汇总AC负载总能量
			 offsetof(DCDC_DATA_TYPE, DC5Power), MEMSIZE(DCDC_DATA_TYPE, DC5Power), data_sum);
		
			summary_inv_node_X_inv_base(											// 汇总PV总充电能量
			 offsetof(DCDC_DATA_TYPE, DC6Power), MEMSIZE(DCDC_DATA_TYPE, DC6Power), data_sum);
		
			summary_inv_node_X_inv_base(										// 汇总电网总充电能量
			 offsetof(DCDC_DATA_TYPE, TotalInputEnergy), MEMSIZE(DCDC_DATA_TYPE, TotalInputEnergy), data_sum);
		
			 summary_inv_node_X_inv_base(										// 汇总电网总充电能量
			 offsetof(DCDC_DATA_TYPE, DC1Voltage), MEMSIZE(DCDC_DATA_TYPE, DC1Voltage), data_sum);
		
		
		/*
		告警汇总
			 uint16_t DC1_fault;//DC1故障
			 uint16_t DC2_fault;//DC2故障
			 uint16_t DC3_alarm;//DC3故障
			 uint16_t DC4_fault;//DC4故障
			 uint16_t DC5_fault;//DC5故障
			 uint16_t DC6_fault;//DC6故障
		
		
			 */
			summary_inv_node_X_inv_base(									   // 汇总电网总充电能量
			offsetof(DCDC_DATA_TYPE, DC1_fault), MEMSIZE(DCDC_DATA_TYPE, DC1_fault), data_or_operate);
			summary_inv_node_X_inv_base(									   // 汇总电网总充电能量
			offsetof(DCDC_DATA_TYPE, DC2_fault), MEMSIZE(DCDC_DATA_TYPE, DC2_fault), data_or_operate);
			summary_inv_node_X_inv_base(									   // 汇总电网总充电能量
			offsetof(DCDC_DATA_TYPE, DC3_alarm), MEMSIZE(DCDC_DATA_TYPE, DC3_alarm), data_or_operate);
			summary_inv_node_X_inv_base(									   // 汇总电网总充电能量
			offsetof(DCDC_DATA_TYPE, DC4_fault), MEMSIZE(DCDC_DATA_TYPE, DC4_fault), data_or_operate);
			summary_inv_node_X_inv_base(									   // 汇总电网总充电能量
			offsetof(DCDC_DATA_TYPE, DC5_fault), MEMSIZE(DCDC_DATA_TYPE, DC5_fault), data_or_operate);
		
			summary_inv_node_X_inv_base(									   // 汇总电网总充电能量
			offsetof(DCDC_DATA_TYPE, DC6_fault), MEMSIZE(DCDC_DATA_TYPE, DC6_fault), data_or_operate);

	}
//		ESP_LOGE(TAG, "can_node_rd.Dcdc[0].iot_can_15500.DC1Power=%d\n", can_node_rd.Dcdc[0].iot_can_15500.DC1Power);
//		ESP_LOGE(TAG, "can_node_rd.Dcdc[1].iot_can_15500.DC1Power=%d\n", can_node_rd.Dcdc[1].iot_can_15500.DC1Power);
//		ESP_LOGE(TAG, "can_node_rd.Dcdc[2].iot_can_15500.DC1Power=%d\n", can_node_rd.Dcdc[2].iot_can_15500.DC1Power);
//		ESP_LOGE(TAG, "can_node_rd.Dcdc[3].iot_can_15500.DC1Power=%d\n", can_node_rd.Dcdc[3].iot_can_15500.DC1Power);

//	ESP_LOGW(TAG, "can_node_rd.Dcdc[0].iot_can_15500.DC2Power=%d\n", can_node_rd.Dcdc[0].iot_can_15500.DC2Power);
//	ESP_LOGW(TAG, "can_node_rd.Dcdc[1].iot_can_15500.DC2Power=%d\n", can_node_rd.Dcdc[1].iot_can_15500.DC2Power);
//	ESP_LOGW(TAG, "can_node_rd.Dcdc[2].iot_can_15500.DC2Power=%d\n", can_node_rd.Dcdc[2].iot_can_15500.DC2Power);
//	ESP_LOGW(TAG, "can_node_rd.Dcdc[3].iot_can_15500.DC2Power=%d\n", can_node_rd.Dcdc[3].iot_can_15500.DC2Power);

	
	// memcpy((uint8_t *)&top_modbus_rd.Inv[(DCDC_MAX_NUM)].mod_reg01100_Inv_base, (uint8_t *)&can_node_rd.Inv[DCDC_MAX_NUM].inv_base,sizeof(MOD_STRUCT_reg01100));




}

