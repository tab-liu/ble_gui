/*
 * File : prot_param.c
 * 该文件是智能插座软件工程的一部分
 * 该文件提供modbus数据表注册，并自动执行初始化
 * 自动初始化功能利用__attribute__((constructor))属性实现
 * 需要在编译中添加WHOLE_ARCHIVE选项
 * 
 * 该文件定义了每个modbus表的处理函数，在函数中处理表格读写操作
 * 
 * Change Logs:
 * Date         Author          Notes
 * 2024-03-28   heyinping       初始版本


 constructor在main开始运行之前被调用，destructor在main函数结束后被调用。如果有多个constructor或destructor，可以给每个constructor或destructor赋予优先级，对于constructor，优先级数值越小，运行越早。destructor则相反
 */
#include "inv_define.h"
#include "modbus_data.h"
#include "modbus_protocol.h"
#include "parameter.h"
#include "esp_check.h"
#include "esp_log.h"
#include <string.h>
#include "can_protocol.h"


#include "freertos/FreeRTOS.h"
#include "stddef.h"
#include "comm_define.h"
#include "filesystem.h"
#include "utils.h"
#include "http_client.h"
#include "dev_data_record.h"

#include "smart_tou_ctrl_v1.h"
#include "ac_ems_cloud.h"
#include "bms_can.h"
#include "dev_modbus_s1_data_handler.h"
#include "dev_modbus_manage.h"

#define TAG "md_data"

#define BIT_COMPARE(new_val, old_val)   (((old_val == CFG_INVALID) && (new_val == CFG_ENABLE)) ||  \
                                        ((old_val != CFG_INVALID) && (new_val != CFG_INVALID) && (new_val != old_val)))

//系统获取的其他设备的信息结构体
USE_EXT_RAM_BSS STRUCT_TOP_MODBUS_READ top_modbus_rd = {0};//BLE/WIFI对上 modbus汇总
USE_EXT_RAM_BSS STRUCT_TOP_MODBUS_WRITE top_modbus_wr = {0};

USE_EXT_RAM_BSS STRUCT_OTHER_READ g_other_rd = {0};//通过BLE client/WIFI MESH等接入的其他设备汇总
USE_EXT_RAM_BSS STRUCT_OTHER_WRITE g_other_wr = {0};

static USE_EXT_RAM_BSS uint16_t Old_Inv_Status[INV_MAX_NUM + 1] = {0};
static USE_EXT_RAM_BSS uint16_t Old_Pack_Status[PACK_MAX_NUM] = {0};
static USE_EXT_RAM_BSS uint64_t Old_Pack_SN_Code[PACK_MAX_NUM] = {0};
static USE_EXT_RAM_BSS uint16_t Old_DCHUB_Status[DC_HUB_MAX_NUM] = {0};
static USE_EXT_RAM_BSS uint16_t Old_DCDC_Status[DCDC_MAX_NUM] = {0};

static SemaphoreHandle_t modbus_21000_semaphore; //modbus 21000空闲信号量
static QueueHandle_t modbus_21000_bind_msg_queue = NULL;

extern uint8_t iot_debug_index_valid(uint16_t index);
extern uint8_t iot_test_index_valid(uint16_t index);

/*------------------------------------------------------------------------
*@Function :vLookupDataTab
uart modbus beta查表
-------------------------------------------------------------------------*/
/**
*@brief  
master_source:1- BLE/WIFI;2-ESP32->CC1312 uart

uint8_t SlaveAddr:modbus从机地址,用于区分不同设备的结构体变量，如 S1的 单设备信息 必须>=MOD_REG_START_ADDR_14500

*@param[iReadAddr]	   寄存器起始地址
*@param[iReadNum]	 寄存器数量
*@param[is_write]	 1-WR;0-RD
*@param[* reg_position]   uart透传， 仅当WR有效，输出向下级传递的辅助信息
*@return		 
*/
const uint16_t* vLookupDataTab( uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t* reg_position, channel_modbus chl)
{
	uint16_t start = 0;
	const uint16_t *reg_ptr = NULL;
	
	if ((iReadAddr >= MOD_REG_START_ADDR_00000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000)))
	{
		start = MOD_REG_START_ADDR_00000;	  /* 可读 */
		if (is_write != true) //read
		{
			reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000;
		} 
		else //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg00000;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_00100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100)))
	{
		start = MOD_REG_START_ADDR_00100;	  /* 可读 */
		if (true == is_write) //write
		{
			
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg00100_AppPage1;
				
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg00100_AppPage1;
			}
            else if((SlaveAddr >= MD_DCHUB_ADDR_START) && (SlaveAddr < (MD_DCHUB_ADDR_START + DC_HUB_MAX_NUM)))
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[SlaveAddr-MD_DCHUB_ADDR_START].mod_reg00100_AppPage1;
            }
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_00700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00700 + MOD_REG_LEN_00700)))
	{
		start = MOD_REG_START_ADDR_00700;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg00700_OTA;//统计各子设备进度后再汇总
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg00700_OTA;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg00700_OTA;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100)))
	{
		start = MOD_REG_START_ADDR_01100;	  /* 可读 */
		if (true == is_write) //write
		{

		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg01100_Inv_base;
			}
            else if((SlaveAddr >= MD_DCHUB_ADDR_START) && (SlaveAddr < (MD_DCHUB_ADDR_START + DC_HUB_MAX_NUM)))
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[SlaveAddr-MD_DCHUB_ADDR_START].mod_reg01100_Inv_base;
            }
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01200) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01200 + MOD_REG_LEN_01200)))
	{
		start = MOD_REG_START_ADDR_01200;	  /* 可读 */
		if (true == is_write) //write
		{
			
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg01200_Inv_pv;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg01200_Inv_pv;
			}				
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300)))
	{
		start = MOD_REG_START_ADDR_01300;	  /* 可读 */
		if (true == is_write) //write
		{
			
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg01300_Inv_grid;
			}
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg01300_Inv_grid;
			}			
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01400) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400)))
	{
		start = MOD_REG_START_ADDR_01400;	  /* 可读 */
		if (true == is_write) //write
		{
			
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg01400_Inv_load;
			}
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg01400_Inv_load;
			}				
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01500 + MOD_REG_LEN_01500)))
	{
		start = MOD_REG_START_ADDR_01500;	  /* 可读 */
		if (true == is_write) //write
		{
			
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg01500_Inv_inv;
			}
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg01500_Inv_inv;
			}			
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01600 + MOD_REG_LEN_01600)))
	{
		start = MOD_REG_START_ADDR_01600;	  /* 可读 */
		if (true == is_write) //write
		{
			
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg01600_Inv_generator;
			}
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg01600_Inv_generator;
			}
		}
	}	
	else if ((iReadAddr >= MOD_REG_START_ADDR_02000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000)))
	{
		start = MOD_REG_START_ADDR_02000;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg02000_Inv_base_set;

		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg02000_Inv_base_set;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg02000_Inv_base_set;
			}	
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_02200) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200)))
	{
		start = MOD_REG_START_ADDR_02200;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg02200_Inv_advance_set;
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg02200_Inv_advance_set;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg02200_Inv_advance_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_02400) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400)))
	{
		start = MOD_REG_START_ADDR_02400;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg02400_Inv_certification;
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg02400_Inv_certification;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg02400_Inv_certification;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_03000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_03000 + MOD_REG_LEN_03000)))//read
	{
		start = MOD_REG_START_ADDR_03000;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg03000_Inv_history;
		} 
		else //read
		{            
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg03000_Inv_history;
			}
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg03000_Inv_history;
			}
	
		}
	}	
	else if ((iReadAddr >= MOD_REG_START_ADDR_03500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_03500 + MOD_REG_LEN_03500)))//read
	{
		start = MOD_REG_START_ADDR_03500;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg03500_Inv_yearX_statistic;
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03500_Inv_yearX_statistic;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg03500_Inv_yearX_statistic;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_03600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_03600 + MOD_REG_LEN_03600)))//read
	{
		start = MOD_REG_START_ADDR_03600;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg03600_Inv_year1_statistic;
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03600_Inv_year1_statistic;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg03600_Inv_year1_statistic;
			}

		}
	}
#ifdef CONFIG_MODBUS_REG_ADDR_3700_ENABLE    
	else if ((iReadAddr >= MOD_REG_START_ADDR_03700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_03700 + MOD_REG_LEN_03700)))//read
	{
		start = MOD_REG_START_ADDR_03700;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg03700_Inv_day_energy;
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[(INV_MAX_NUM)].mod_reg03700_Inv_day_energy;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg03700_Inv_day_energy;
			}
		}
	}
#endif    
	else if ((iReadAddr >= MOD_REG_START_ADDR_06000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_06000 + MOD_REG_LEN_06000)))//read
	{
		start = MOD_REG_START_ADDR_06000;	  /* 可读 */
		if (true == is_write) //write
		{
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[(PACK_MAX_NUM)].mod_reg06000_Pack_sum;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < (MD_INV_ADDR_START + PACK_GROUP_NUM)))//各逆变器内置电池包
			{
			    uint8_t index = (SlaveAddr - MD_INV_ADDR_START) * PACK_INGROUP_MAX_NUM;
				if(index < PACK_MAX_NUM) reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[index].mod_reg06000_Pack_sum;
			} 
			else if((SlaveAddr >= MD_PACK_ADDR_START) && (SlaveAddr < (MD_PACK_ADDR_START + PACK_MAX_NUM)))//各电池包
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[SlaveAddr-MD_PACK_ADDR_START].mod_reg06000_Pack_sum;
			} 
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_06100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_06100 + MOD_REG_LEN_06100)))//read
	{
		start = MOD_REG_START_ADDR_06100;	  /* 可读 */
		if (true == is_write) //write
		{
		} 
		else //read
		{
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[(PACK_MAX_NUM)].mod_reg06100_Pack_each;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < (MD_INV_ADDR_START + PACK_GROUP_NUM)))//各逆变器内置电池包
			{
			    uint8_t index = (SlaveAddr - MD_INV_ADDR_START) * PACK_INGROUP_MAX_NUM;
				if(index < PACK_MAX_NUM) reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[index].mod_reg06100_Pack_each;
			} 
			else if((SlaveAddr >= MD_PACK_SUM_ADDR_START) && (SlaveAddr < (MD_PACK_SUM_ADDR_START + MD_PACK_SUM_ADDR_CNT)))//各电池包
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Type_Pack[SlaveAddr-MD_PACK_SUM_ADDR_START].mod_reg06100_Pack_each;
			} 
			else if((SlaveAddr >= MD_PACK_ADDR_START) && (SlaveAddr < (MD_PACK_ADDR_START + PACK_MAX_NUM)))//各电池包
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[SlaveAddr-MD_PACK_ADDR_START].mod_reg06100_Pack_each;
			} 
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_06300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_06300 + MOD_REG_LEN_06300)))//动态读取
	{
		start = MOD_REG_START_ADDR_06300;	  /* 可读 */
		if (true == is_write) //write
		{
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg06300_Pack_cell read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[(PACK_MAX_NUM)].mod_reg06300_Pack_cell;//top_modbus_rd.Dcdc
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < (MD_INV_ADDR_START + PACK_GROUP_NUM)))//各逆变器内置电池包
			{
			    uint8_t index = (SlaveAddr - MD_INV_ADDR_START) * PACK_INGROUP_MAX_NUM;
				if(index < PACK_MAX_NUM) reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[index].mod_reg06300_Pack_cell;
			} 
			else if((SlaveAddr >= MD_PACK_ADDR_START) && (SlaveAddr < (MD_PACK_ADDR_START + PACK_MAX_NUM)))//各电池包
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[SlaveAddr-MD_PACK_ADDR_START].mod_reg06300_Pack_cell;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_07000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_07000 + MOD_REG_LEN_07000)))
	{
		start = MOD_REG_START_ADDR_07000;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Pack.mod_reg07000_Pack_set;
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg07000_Pack_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[(PACK_MAX_NUM)].mod_reg07000_Pack_set;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < (MD_INV_ADDR_START + PACK_GROUP_NUM)))//各逆变器内置电池包
			{
			    uint8_t index = (SlaveAddr - MD_INV_ADDR_START) * PACK_INGROUP_MAX_NUM;
				if(index < PACK_MAX_NUM) reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[index].mod_reg07000_Pack_set;
			} 
			else if((SlaveAddr >= MD_PACK_ADDR_START) && (SlaveAddr < (MD_PACK_ADDR_START + PACK_MAX_NUM)))//各电池包
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[SlaveAddr-MD_PACK_ADDR_START].mod_reg07000_Pack_set;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_11000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_11000 + MOD_REG_LEN_11000)))//IOT自身数据，不往下转
	{
		start = MOD_REG_START_ADDR_11000;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg11000_IOT_info;
		
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg11000_IOT_info read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START +INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg11000_IOT_info;					
			}	
            else if(MD_DCDC_SUM_ADDR== SlaveAddr)
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg11000_IOT_info;
            }
            else if((SlaveAddr >= MD_DCDC_ADDR_START) && (SlaveAddr < (MD_DCDC_ADDR_START + DCDC_MAX_NUM)))
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[SlaveAddr-MD_DCDC_ADDR_START].mod_reg11000_IOT_info;
            }
		}
	}	
	else if ((iReadAddr >= MOD_REG_START_ADDR_12000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000)))//IOT自身数据，不往下转
	{
		start = MOD_REG_START_ADDR_12000;	  /* 可读 */
		if (true == is_write) //write
		{
			ESP_LOGI(TAG,"get in mod_reg12000_IOT_set write");
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg12000_IOT_set; 
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg12000_IOT_set read");
			if(0 == SlaveAddr)//汇总
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set;
			}
			else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START +INV_MAX_NUM))//单INV
			{
				reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg12000_IOT_set;
			}			
		}		
	}
    else if ((iReadAddr >= MOD_REG_START_ADDR_13500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_13500 + MOD_REG_LEN_13500)))
    {
        start = MOD_REG_START_ADDR_13500;

        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg13500_mesh;
        }
        else //read
        {
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh;
            }
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg13500_mesh;
            }
		}
	}
    else if ((iReadAddr >= MOD_REG_START_ADDR_13600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_13600 + MOD_REG_LEN_13600)))
    {
        start = MOD_REG_START_ADDR_13600;

        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg13600_open;
        }
        else //read
        {
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open;
            }
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START + INV_MAX_NUM))//单INV
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg13600_open;
            }
            else if((SlaveAddr >= DEV_MODBUS_ADDR_START) && (SlaveAddr <= DEV_MODBUS_ADDR_END))
            {
                switch ( gDevModbusAddrList[SlaveAddr - DEV_MODBUS_ADDR_START].dev_type )
                {
                    case SN_TYPE_S1 :
                        uint16_t real_addr = dev_modbus_plug_slave_addr_get(gDevModbusAddrList[SlaveAddr - DEV_MODBUS_ADDR_START].dev_type, gDevModbusAddrList[SlaveAddr - DEV_MODBUS_ADDR_START].dev_sn);
                        if (real_addr > 0 || real_addr <= PLUG_MAX_NUM) {
                            reg_ptr = (const uint16_t*)&g_other_rd.Plug[real_addr - 1].mod_reg13600_open;
                        }
                        break;
                        
                    default:
                        break;
                }
            }
		}
	}
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE    
    else if ((iReadAddr >= MOD_REG_START_ADDR_14500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500)))
    {
        start = MOD_REG_START_ADDR_14500;     /* 可读 */
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&g_other_wr.Plug.mod_reg14500_SmartPlug_info;
        
        } 
        else //read
        {
            ESP_LOGI(TAG,"get in mod_reg14500_SmartPlug_info read");
            if(0 == SlaveAddr)//汇
            {
                reg_ptr = (const uint16_t*)&g_other_rd.Plug[PLUG_MAX_NUM].mod_reg14500_SmartPlug_info;
            }
            else if((SlaveAddr >= DEV_MODBUS_ADDR_START) && (SlaveAddr <= DEV_MODBUS_ADDR_END))
            {
                uint16_t real_addr = dev_modbus_plug_slave_addr_get(gDevModbusAddrList[SlaveAddr - DEV_MODBUS_ADDR_START].dev_type, gDevModbusAddrList[SlaveAddr - DEV_MODBUS_ADDR_START].dev_sn);
                if (real_addr > 0 || real_addr <= PLUG_MAX_NUM) {
                    reg_ptr = (const uint16_t*)&g_other_rd.Plug[real_addr - 1].mod_reg14500_SmartPlug_info;
                }
            }         
        }
    }
    else if ((iReadAddr >= MOD_REG_START_ADDR_14700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700)))
    {
        start = MOD_REG_START_ADDR_14700;     /* 可读 */

        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&g_other_wr.Plug.mod_reg14700_SmartPlug_set;
        
        } 
        else //read
        {
            ESP_LOGI(TAG,"get in mod_reg14700_SmartPlug_set read");
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&g_other_rd.Plug[PLUG_MAX_NUM].mod_reg14700_SmartPlug_set;
            }
            else if((SlaveAddr >= DEV_MODBUS_ADDR_START) && (SlaveAddr <= DEV_MODBUS_ADDR_END))
            {
                uint16_t real_addr = dev_modbus_plug_slave_addr_get(gDevModbusAddrList[SlaveAddr - DEV_MODBUS_ADDR_START].dev_type, gDevModbusAddrList[SlaveAddr - DEV_MODBUS_ADDR_START].dev_sn);
                if (real_addr > 0 || real_addr <= PLUG_MAX_NUM) {
                    reg_ptr = (const uint16_t*)&g_other_rd.Plug[real_addr - 1].mod_reg14700_SmartPlug_set;
                }
            }
        }       
    }
#endif
#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
    else if ((iReadAddr >= MOD_REG_START_ADDR_15500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15500 + MOD_REG_LEN_15500)))
    {
        start = MOD_REG_START_ADDR_15500;     /* 可读 */
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.Dcdc.mod_reg15500_DCDC_info;
        } 
        else //read
        {
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15500_DCDC_info;
            }
            else if(MD_DCDC_SUM_ADDR == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15500_DCDC_info;
            }
            else if((SlaveAddr >= MD_DCDC_ADDR_START) && (SlaveAddr < (MD_DCDC_ADDR_START + DCDC_MAX_NUM)))
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[SlaveAddr-MD_DCDC_ADDR_START].mod_reg15500_DCDC_info;
            }
        }
    }
    else if ((iReadAddr >= MOD_REG_START_ADDR_15600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600)))
    {
        start = MOD_REG_START_ADDR_15600;     /* 可读 */
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.Dcdc.mod_reg15600_DCDC_set;
        } 
        else //read
        {
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15600_DCDC_set;
            }
            else if(MD_DCDC_SUM_ADDR == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg15600_DCDC_set;
            }
            else if((SlaveAddr >= MD_DCDC_ADDR_START) && (SlaveAddr < (MD_DCDC_ADDR_START + DCDC_MAX_NUM)))
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Dcdc[SlaveAddr-MD_DCDC_ADDR_START].mod_reg15600_DCDC_set;
            }
        }
    }
#endif
#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    else if ((iReadAddr >= MOD_REG_START_ADDR_15700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15700 + MOD_REG_LEN_15700)))
    {
        start = MOD_REG_START_ADDR_15700;     /* 可读 */
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.DcHub.mod_reg15700_Dc_Hub_info;
        } 
        else //read
        {
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg15700_Dc_Hub_info;
            }
            else if(MD_DCHUB_SUM_ADDR == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg15700_Dc_Hub_info;
            }
            else if((SlaveAddr >= MD_DCHUB_ADDR_START) && (SlaveAddr < (MD_DCHUB_ADDR_START + DC_HUB_MAX_NUM)))
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[SlaveAddr-MD_DCHUB_ADDR_START].mod_reg15700_Dc_Hub_info;
            }
        }
    }
    else if ((iReadAddr >= MOD_REG_START_ADDR_15750) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15750 + MOD_REG_LEN_15750)))
    {
        start = MOD_REG_START_ADDR_15750;     /* 可读 */
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.DcHub.mod_reg15750_Dc_Ac_Hub_set;
        } 
        else //read
        {
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg15750_Dc_Ac_Hub_set;
            }
            else if(MD_DCHUB_SUM_ADDR == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[DC_HUB_MAX_NUM].mod_reg15750_Dc_Ac_Hub_set;
            }
            else if((SlaveAddr >= MD_DCHUB_ADDR_START) && (SlaveAddr < (MD_DCHUB_ADDR_START + DC_HUB_MAX_NUM)))
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.DcHub[SlaveAddr-MD_DCHUB_ADDR_START].mod_reg15750_Dc_Ac_Hub_set;
            }
        }
    }    
#endif
#ifdef CONFIG_RELAY_CTRL_ENABLE    
	else if ((iReadAddr >= MOD_REG_START_ADDR_19000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000)))
	{
		start = MOD_REG_START_ADDR_19000;
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg19000_Relay_ctrl;
		}
		else //read
		{
			reg_ptr = (const uint16_t*)&RelaySetData.Relay_info_t.PowerRelay_SmartSet01;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_19100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_19100 + MOD_REG_LEN_19100)))
	{
		start = MOD_REG_START_ADDR_19100;
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg19100_Relay_ctrl_2;
		}
		else //read
		{
			reg_ptr = (const uint16_t*)&RelaySetData.Relay_info_t.PowerRelay_SmartSet02;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_19200) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_19200 + MOD_REG_LEN_19200)))
	{
		start = MOD_REG_START_ADDR_19200;
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg19200_Backup_power;
		}
		else //read
		{
			reg_ptr = (const uint16_t*)&RelaySetData.Relay_info_t.Backup_power_set;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_19300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_19300 + MOD_REG_LEN_19300)))
	{
		start = MOD_REG_START_ADDR_19300;
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg19300_Relay_ctrl_3;
		}
		else //read
		{
			reg_ptr = (const uint16_t*)&RelaySetData.Relay_info_t.PowerRelay_SmartSet03;
		}
	}
#endif    
	else if ((iReadAddr >= MOD_REG_START_ADDR_21000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_21000 + MOD_REG_LEN_21000)))
	{
		start = MOD_REG_START_ADDR_21000;	  
		//读和写的含义不同
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg21000_bind_WR;
		} 
		else //read
		{
		    // 21000段协议不允许直接进行读取操作
			// reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;
		}		
	}	
#ifdef CONFIG_OPEN_MQTT_ENABLE    
	else if ((iReadAddr >= MOD_REG_START_ADDR_22000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_22000 + MOD_REG_LEN_22000)))//公版网页服务器：配置�?
	{
		start = MOD_REG_START_ADDR_22000;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg22000_net_server_2rd;
		
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg22000_net_server_2rd read");	
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg22000_net_server_2rd;
            }
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START +INV_MAX_NUM))//单INV
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg22000_net_server_2rd;
            }
		}		
	}
#endif    
#ifdef CONFIG_SMART_TOU_CTRL_V1_ENABLE
	else if ((iReadAddr >= SMART_TOU_REG_START_ADDR) && ((iReadAddr+ iReadNum) <= (SMART_TOU_REG_START_ADDR + SMART_TOU_REG_MAX_LEN)))
	{
		start = SMART_TOU_REG_START_ADDR;
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)Smart_Tou_Ctrl_Config_Get_Ptr(1);
		}
		else //read
		{
			reg_ptr = (const uint16_t*)Smart_Tou_Ctrl_Config_Get_Ptr(0);
		}
	}
#endif
#ifdef CONFIG_AC_EMS_CLOUD_ENABLE
	else if ((iReadAddr >= AC_EMS_CLOUD_REG_START_ADDR) && ((iReadAddr+ iReadNum) <= (AC_EMS_CLOUD_REG_START_ADDR + AC_EMS_CLOUD_REG_MAX_LEN)))
	{
		start = AC_EMS_CLOUD_REG_START_ADDR;
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)Ac_Ems_Cloud_Config_Get_Ptr(1);
		}
		else //read
		{
			reg_ptr = (const uint16_t*)Ac_Ems_Cloud_Config_Get_Ptr(0);
		}
	}
#endif    
    else if ((iReadAddr >= MOD_REG_START_ADDR_29800) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_29800 + MOD_REG_LEN_29800)))
    {
        start = MOD_REG_START_ADDR_29800;     
        //读和写的含义不同
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg29800_softver_info;
        } 
        else //read
        {
            reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg29800_softver_info;
        }       
    }   
#ifdef CONFIG_FUNC_PAYGO_ENABLE
    else if ((iReadAddr >= MOD_REG_START_ADDR_30000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_30000 + MOD_REG_LEN_30000)))
    {
        start = MOD_REG_START_ADDR_30000;     /* 可读 */
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.Pack.mod_reg30000_Pack_paygo;
        } 
        else //read
        {
            ESP_LOGI(TAG,"get in mod_reg30000_Pack_paygo read");
            if(0 == SlaveAddr)//汇总
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[(PACK_MAX_NUM)].mod_reg30000_Pack_paygo;
            }
            else if((SlaveAddr >= MD_PACK_ADDR_START) && (SlaveAddr < (MD_PACK_ADDR_START + PACK_MAX_NUM)))//各电池包
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[SlaveAddr-MD_PACK_ADDR_START].mod_reg30000_Pack_paygo;
            }
        }
    }
#endif
    else if ((iReadAddr >= MOD_REG_START_ADDR_30900) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_30900 + MOD_REG_LEN_30900)))
    {
        start = MOD_REG_START_ADDR_30900;     
        //读和写的含义不同
        if (true == is_write) //write
        {
            reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg30900_test;

            // 部分模式需要立即生效
            iot_test_index_valid(top_modbus_wr.Inv.mod_reg30900_test.test_work_mode);
        } 
        else //read
        {
            if (( 0 == SlaveAddr ) && ( iot_test_index_valid(top_modbus_wr.Inv.mod_reg30900_test.test_work_mode) )) {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Self_IoT.mod_reg30900_IoT_Test;
            } else {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg30900_test;
            }
        }       
    }       
	else if ((iReadAddr >= MOD_REG_START_ADDR_40000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000)))
	{
		start = MOD_REG_START_ADDR_40000;	  /* 可读 */
		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&top_modbus_wr.Inv.mod_reg40000_transparent;
		} 
		else //read
		{
            if(0 == SlaveAddr)//汇总
            {
                /*默认上报数据-index=300*/
                uint16_t param_index = (chl == MD_CHL_SELF) ? 300 : top_modbus_wr.Inv.mod_reg40000_transparent.param_index;
                
                if ( iot_debug_index_valid(param_index) ) {
                    reg_ptr = (const uint16_t*)&top_modbus_rd.Self_IoT.mod_reg40000_IOT_Debug;
                } else {
                    // IOT本地数据无效，默认显示ARM数据
                    reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg40000_transparent;
                }
            }
            else if((SlaveAddr >= MD_INV_ADDR_START) && (SlaveAddr < MD_INV_ADDR_START +INV_MAX_NUM))//单INV
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Inv[SlaveAddr-MD_INV_ADDR_START].mod_reg40000_transparent;
            }
            else if((SlaveAddr >= MD_PACK_ADDR_START) && (SlaveAddr < (MD_PACK_ADDR_START + PACK_MAX_NUM)))//各电池包
            {
                reg_ptr = (const uint16_t*)&top_modbus_rd.Pack[SlaveAddr-MD_PACK_ADDR_START].mod_reg40000_transparent;
            }
		}		
	}
	else 
	{
        ESP_LOGE(TAG,"vLookupDataTab unknown reg(addr: %u, num: %u)", iReadAddr, iReadNum);
	}	

	if(NULL != reg_position)
	{
			reg_position->reg_addr = start;
			reg_position->offset= (iReadAddr -start)*2;
			reg_position->len=iReadNum*2;
	}		
		
	if (reg_ptr) 
	{
		return (reg_ptr + (iReadAddr - start));
	}
	else
	{
		return NULL;
	}
}

/**
*@brief
uint8_t SlaveAddr:modbus从机地址,用于区分不同设备的结构体变量， 
*@param[iReadAddr]  寄存器起始地址
*@param[iReadNum]	 寄存器数量
*@param[is_write]	 1-WR;0-RD
*@param[* reg_position]    仅当WR有效，输出向下级传递的辅助信息
*@param[iReaddev] 不同设备信息
*@return
*/
const uint16_t* vLookupDataTab_from_other_dev( uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t* reg_position, channel_modbus chl)
{
	uint16_t start = 0;
	const uint16_t *reg_ptr = NULL;

//		ESP_LOGW(TAG, "vLookupDataTab_from_other_dev:	SlaveAddr=%d,startAddress=%u,writeRegsCnt=%u ,is_write=%d ",SlaveAddr,iReadAddr,iReadNum,is_write);
	if ((iReadAddr >= MOD_REG_START_ADDR_00000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000)))
	{
		start = MOD_REG_START_ADDR_00000;	  /* 可读 */
		
		if (is_write != true) //read
		{
			reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.mod_reg00000;
		} 
		else //write
		{
			reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.mod_reg00000;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_00100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00100 + MOD_REG_LEN_00100)))
	{
		start = MOD_REG_START_ADDR_00100;	  /* 可读 */
		
		if (is_write != true) //read
		{
			ESP_LOGI(TAG,"get in mod_reg00100_AppPage1 read from other dev, iReadAddr=%d, iReadNum=%d", iReadAddr, iReadNum);
			reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.mod_reg00100_AppPage1;
		} 
		else //write
		{
			reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.mod_reg00100_AppPage1;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01100) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01100 + MOD_REG_LEN_01100)))
	{
		start = MOD_REG_START_ADDR_01100;	  /* 可读 */
		
		if (is_write != true) //read
		{
			ESP_LOGI(TAG,"get in mod_reg01100_Inv_base read from other dev, iReadAddr=%d, iReadNum=%d", iReadAddr, iReadNum);
			reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.mod_reg01100_Inv_base;
		} 
		else //write
		{
			reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.mod_reg01100_Inv_base;
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01300 + MOD_REG_LEN_01300)))
	{
		start = MOD_REG_START_ADDR_01300;	  /* 可读 */
		if (true == is_write) //write
		{
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.mod_reg01300_Inv_grid;
			}
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg01300_Inv_grid read from other dev");
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.mod_reg01300_Inv_grid;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_01400) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_01400 + MOD_REG_LEN_01400)))
	{
		start = MOD_REG_START_ADDR_01400;	  /* 可读 */
		if (true == is_write) //write
		{
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.mod_reg01400_Inv_load;
			}
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg01400_Inv_load read from other dev");
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.mod_reg01400_Inv_load;
			}
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_02000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000)))
	{
		start = MOD_REG_START_ADDR_02000;	  /* 可读 */
		if (true == is_write) //write
		{
			if (chl == MD_CHL_BLE_CLIENT) {
				ESP_LOGI(TAG,"set in mod_reg02000_Inv_base_set read for other dev, iReadAddr=%d, iReadNum=%d", iReadAddr, iReadNum);
				reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.mod_reg02000_Inv_base_set;
			}
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg02000_Inv_base_set read from other dev");
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.mod_reg02000_Inv_base_set;
			}
		}
	}
    else if ((iReadAddr >= MOD_REG_START_ADDR_13600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_13600 + MOD_REG_LEN_13600)))
	{
		start = MOD_REG_START_ADDR_13600;	  /* 可读 */
		if (true == is_write) //write
		{
            ESP_LOGI(TAG,"set in mod_reg13600_Inv_base_set read for other dev, iReadAddr=%d, iReadNum=%d", iReadAddr, iReadNum);
            reg_ptr = (const uint16_t*)&g_other_wr.Plug.mod_reg13600_open;
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg13600_Inv_base_set read from other dev");
            reg_ptr = (const uint16_t*)&g_other_rd.Plug[SlaveAddr - 1].mod_reg13600_open;
            ESP_LOGI(TAG, "dev:%d, pcs_name_set:[%s]", SlaveAddr, g_other_rd.Plug[SlaveAddr - 1].mod_reg13600_open.pcs_name_set);
		}
	}
#if DCDC_ENABLE
	else if ((iReadAddr >= MOD_REG_START_ADDR_15500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15500 + 100)))
	{
		start = MOD_REG_START_ADDR_15500;	  /* 可读 */						
		
		if (is_write != true) //read
		{
			ESP_LOGI(TAG,"get in mod_reg15500_DCDC_info read");		

				switch (chl)
				{
					case MD_CHL_SELF:
						
						break;
					case MD_CHL_UART_DOWN:

					memcpy( &top_modbus_rd.Dcdc[reals.Addr_can_self].mod_reg15500_DCDC_info,&uart_data_rd.mod_reg15500_DCDC_info, sizeof(MOD_STRUCT_reg15500));		 
					memcpy( &can_node_rd.Dcdc[reals.Addr_can_self].iot_can_15500,&uart_data_rd.mod_reg15500_DCDC_info, sizeof(MOD_STRUCT_reg15500));				 
				
				reg_ptr = (const uint16_t*)&uart_data_rd.mod_reg15500_DCDC_info; 	
					
					break;
					case MD_CHL_CAN:
						if(0 == SlaveAddr)//汇
						{
							reg_ptr = (const uint16_t*)&can_node_rd.Dcdc[(DCDC_MAX_NUM)].iot_can_15500; 		
						}
						// else if(SlaveAddr >= MODBUS_SLAVE_ADDR_WIFI_S1_START)
						// {
						// 	reg_ptr = (const uint16_t*)&can_node_rd.Dcdc[SlaveAddr].iot_can_15500;			
						// }
						else 
						{
							reg_ptr = (const uint16_t*)&can_node_rd.Dcdc[SlaveAddr-1].iot_can_15500;
						} 
					break;
					case MD_CHL_BLE_CLIENT:
							
					break;
					case MD_CHL_WIFI_MESH:
							
					break;
					default :
					break;
					
				}					 								 
		} 
		else //write
		{
			/*只读区域不可修改*/
			reg_ptr = (const uint16_t*)&top_modbus_wr.Dcdc.mod_reg15500_DCDC_info;

//D400S uart透传仅实现 DCDC set
			
		}
	}
	else if ((iReadAddr >= MOD_REG_START_ADDR_15600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600)))
	{
		start = MOD_REG_START_ADDR_15600;	  /* 可读 */		
		if (is_write != true) //read
		{
			switch (chl)
			{
				case MD_CHL_SELF:
					
					break;
				case MD_CHL_UART_DOWN:
					memcpy( &top_modbus_rd.Dcdc[reals.Addr_can_self].mod_reg15600_DCDC_set,&uart_data_rd.mod_reg15600_DCDC_set, sizeof(MOD_STRUCT_reg15600));		
					memcpy( &can_node_rd.Dcdc[reals.Addr_can_self].iot_can_15600,&uart_data_rd.mod_reg15600_DCDC_set, sizeof(MOD_STRUCT_reg15600));	

					reg_ptr = (const uint16_t*)&uart_data_rd.mod_reg15600_DCDC_set; 
				break;
				case MD_CHL_CAN:
					if(0 == SlaveAddr)//汇
					{
						reg_ptr = (const uint16_t*)&can_node_rd.Dcdc[(DCDC_MAX_NUM)].iot_can_15600; 		
					}
					// else if(SlaveAddr >= MODBUS_SLAVE_ADDR_WIFI_S1_START)
					// {
					// 	reg_ptr = (const uint16_t*)&can_node_rd.Dcdc[SlaveAddr].iot_can_15600;			
					// }
					else 
					{
						reg_ptr = (const uint16_t*)&can_node_rd.Dcdc[SlaveAddr-1].iot_can_15600;
					} 
				break;
				case MD_CHL_BLE_CLIENT:
						
				break;
				case MD_CHL_WIFI_MESH:
						
				break;
				default :
				break;
					
				}					 								 
		} 
		else //write
		{
			switch (chl)
			{
				case MD_CHL_SELF:
					
					break;
				case MD_CHL_UART_DOWN:
				
					reg_ptr = (const uint16_t*)&uart_data_wr.mod_reg15600_DCDC_set; 
				break;
				case MD_CHL_CAN:
					if(0 == SlaveAddr)//汇
					{
						reg_ptr = (const uint16_t*)&can_node_wr.Dcdc.iot_can_15600; 		
					}
					// else if(SlaveAddr >= MODBUS_SLAVE_ADDR_WIFI_S1_START)
					// {
					// 	reg_ptr = (const uint16_t*)&can_node_wr.Dcdc.iot_can_15600;			
					// }
					else 
					{
						reg_ptr = (const uint16_t*)&can_node_wr.Dcdc.iot_can_15600;
					} 
				break;
				case MD_CHL_BLE_CLIENT:
						
				break;
				case MD_CHL_WIFI_MESH:
						
				break;
				default :
				break;
			}
		}
	}
#endif
	if(NULL != reg_position)
	{
			reg_position->reg_addr = start;
		//		reg_position->can_type=0;
			reg_position->offset= (iReadAddr -start)*2;
			reg_position->len=iReadNum*2;
// ESP_LOGI(TAG, "windy tab_can :reg_position->reg_addr=%d,reg_position->offset=%d,reg_position->len=%d  ",reg_position->reg_addr,reg_position->offset,reg_position->len);
//	ESP_LOGI(TAG, "windy tab_can22 ：iReadAddr=%d,start=%d  ",iReadAddr,start);
			
	}		
		

	if (reg_ptr) 
	{
		return (reg_ptr + (iReadAddr - start));
	}
	else
	{
		return NULL;
	}


}

/*------------------------------------------------------------------------
*@Function :vLookupDataTab_from_other_dev_alpha
*Alpha版本协议的modbus查表函数

通过BLE/WIFI查询其他设备的信息，专门处理Alpha版本协议
-------------------------------------------------------------------------*/
/**
*@brief  ALPHA版本协议的查表函数

uint8_t SlaveAddr:modbus从机地址,用于区分不同设备的结构体变量

*@param[iReadAddr]	   寄存器起始地址
*@param[iReadNum]	 寄存器数量
*@param[is_write]	 1-WR;0-RD
*@param[* reg_position]    仅当WR有效，输出向下级传递的辅助信息
*@param[chl] 通信通道类型

*@return		 
*/
const uint16_t* vLookupDataTab_from_other_dev_alpha( uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t* reg_position, channel_modbus chl)
{
	uint16_t start = 0;
	const uint16_t *reg_ptr = NULL;

	ESP_LOGI(TAG, "vLookupDataTab_from_other_dev_alpha: SlaveAddr=%d, startAddress=%u, writeRegsCnt=%u, is_write=%d, chl=%d", SlaveAddr, iReadAddr, iReadNum, is_write, chl);
	
	if ((iReadAddr >= ALPHA_MOD_REG_START_ADDR_00000) && ((iReadAddr + iReadNum) <= (ALPHA_MOD_REG_START_ADDR_00000 + ALPHA_MOD_REG_LEN_00000)))
	{
		start = ALPHA_MOD_REG_START_ADDR_00000;	  /* 可读 */
		
		if (is_write != true) //read
		{
			ESP_LOGI(TAG,"get in Alpha_mod_reg_PV_sum read from other dev");
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.alpha_inv_info_0000;
			}
		} 
		else //write
		{
			ESP_LOGI(TAG,"get in Alpha_mod_reg_PV_sum write from other dev");
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.alpha_inv_info_0000;
			}
		}
	}
	else if ((iReadAddr >= ALPHA_MOD_REG_START_ADDR_03000) && ((iReadAddr + iReadNum) <= (ALPHA_MOD_REG_START_ADDR_03000 + ALPHA_MOD_REG_LEN_03000)))
	{
		start = ALPHA_MOD_REG_START_ADDR_03000;	  /* 可读 */
		
		if (is_write != true) //read
		{
			ESP_LOGI(TAG,"get in Alpha_mod_reg_PV_each read from other dev");
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_rd.bind_dev.alpha_set_03000;
			}
		} 
		else //write
		{
			ESP_LOGI(TAG,"get in Alpha_mod_reg_PV_each write from other dev");
			if (chl == MD_CHL_BLE_CLIENT) {
				reg_ptr = (const uint16_t*)&g_other_wr.bind_dev.alpha_set_03000;
			}
		}
	}
	else
	{
		ESP_LOGW(TAG, "vLookupDataTab_from_other_dev_alpha: Unsupported register address range: %d", iReadAddr);
	}
	
	if(NULL != reg_position)
	{
		reg_position->reg_addr = start;
		reg_position->offset = (iReadAddr - start) * 2;
		reg_position->len = iReadNum * 2;
		ESP_LOGD(TAG, "Alpha lookup: reg_addr=%d, offset=%d, len=%d", reg_position->reg_addr, reg_position->offset, reg_position->len);
	}		

	if (reg_ptr) 
	{
		return (reg_ptr + (iReadAddr - start));
	}
	else
	{
		ESP_LOGE(TAG, "vLookupDataTab_from_other_dev_alpha: Failed to get register pointer for address %d", iReadAddr);
		return NULL;
	}
}


void copy_data_from_set_to_modbus(void)
{   
    /*modbus 00*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver_iot = IOT_PROTOCOL_VERSION;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver = MODBUS_VERSION;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.support_mode.bit.support_ble_pwd = 1; //蓝牙密码设置区
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.support_mode.bit.visitor_mode = IotSetData.dev_info_t.support_mode.bit.visitor_mode;
#ifdef CONFIG_BLE_LCD_LINK_ENABLE    
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.iot_reg00_status.bit.lcd_data_adv_enable = 1; //磁吸屏配套支持使能
#endif 
#ifdef CONFIG_WLCC_METER_SUPPLY_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.iot_reg00_status.bit.meter_shelly_enable = 1; // 支持shelly电表
#endif
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.iot_reg00_status.bit.plug_s1_enable = 1; // 支持S1插座
#endif
#ifdef CONFIG_MODBUS_REG_TLV_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.iot_reg00_status.bit.modbus_tlv_enable = 1; //支持ModbusTLV（功能码40004/40005）
#endif
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.app_password, IotSetData.dev_info_t.app_password,  sizeof(IotSetData.dev_info_t.app_password));

    /*modbus 100*/
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.dev_type, dev_factory.dev_type, TYPE_SIZE); // 设备类型字符最大12个
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1.dev_sn = dev_factory.dev_sn;
    
    /*modbus 1100*/
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base.InvType, dev_factory.dev_type, TYPE_SIZE); // 设备类型字符最大12个
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base.InvSN = dev_factory.dev_sn;

    /*modbus 2000*/
#ifdef CONFIG_IOT_LED_SET_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.LED_SET.all = IotSetData.dev_info_t.LED_SET.all;
#endif 
#ifdef CONFIG_IOT_CAN_EMS_CTRL_FUNC5   
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.Remote_set.all = IotSetData.dev_info_t.Remote_set.all;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.Remote_set_soc_limit = IotSetData.dev_info_t.Remote_set_soc_limit;
#endif  

    /*modbus 2200*/
#ifdef CONFIG_SMART_GENERATOR_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.GenSet.ctrl_mode = IotSetData.dev_info_t.Gen_ctrl.ctrl_mode;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.GenSet.Unuse_time_ctrl = IotSetData.dev_info_t.Gen_ctrl.Unuse_time_ctrl;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.Gen_SOC_Start = IotSetData.dev_info_t.Gen_ctrl.soc_min;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02200_Inv_advance_set.Gen_SOC_Stop = IotSetData.dev_info_t.Gen_ctrl.soc_max;
#endif

    /*modbus 11000*/
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_type, iot_factory.iot_type, TYPE_SIZE); // 设备类型字符最大12个
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.safe_code = iot_factory.safe_code; // IOT 出厂随机安全码
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_sn = iot_factory.iot_sn; // IOT出厂 SN码
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.Bind_SN =((uint16_t)IotSetData.dev_info_t.Sn_bind[1]<<8)|IotSetData.dev_info_t.Sn_bind[0];
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.Save_Money_Nums = IotSetData.dev_info_t.saved_money; // 省钱金额
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver = IotSetData.dev_info_t.software_ver;
#ifdef CONFIG_MORE_WIFI_STA_LINK_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.wifi_mult_sta_flag.bit.ext_wifi_enable_num = 2; // 额外支持STA2和STA3
#endif
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_sta_ssid, IotSetData.dev_info_t.wifi_sta_ssid,  sizeof(IotSetData.dev_info_t.wifi_sta_ssid));
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_sta_password, IotSetData.dev_info_t.wifi_sta_password,  sizeof(IotSetData.dev_info_t.wifi_sta_password));
    memcpy(&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.thunder_ctrl.all, &IotSetData.dev_info_t.thunder_ctrl.all,  sizeof(IotSetData.dev_info_t.thunder_ctrl.all));
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.period_report = IotSetData.dev_info_t.data_report_cycle;
    memcpy(&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.on_off, &IotSetData.dev_info_t.on_off,  sizeof(IotSetData.dev_info_t.on_off));
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.could_dns, IotSetData.dev_info_t.could_dns,  sizeof(IotSetData.dev_info_t.could_dns));
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.mobile_apn, IotSetData.dev_info_t.mobile_apn,  sizeof(IotSetData.dev_info_t.mobile_apn));
    memcpy(&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.sta_enable, &IotSetData.dev_info_t.sta_enable,  sizeof(IotSetData.dev_info_t.sta_enable));
    memcpy(&top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_sta_auth, &IotSetData.dev_info_t.wifi_sta_auth,  sizeof(IotSetData.dev_info_t.wifi_sta_auth));
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Protocol_3r_Enable_mix1.all = IotSetData.dev_info_t.Protocol_3r_Enable_mix1.all; 
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Protocol_3r_Enable_mix2.all = IotSetData.dev_info_t.Protocol_3r_Enable_mix2.all; 
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.LCD_Mode.all = IotSetData.dev_info_t.lcd_mode.all; // 断码屏模式设置
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.Time_Span.all = IotSetData.dev_info_t.Time_Span.all;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_ap_auth = IotSetData.dev_info_t.wifi_ap_auth;
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_AP_ssid, IotSetData.dev_info_t.wifi_AP_ssid, sizeof(IotSetData.dev_info_t.wifi_AP_ssid));
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.wifi_AP_password, IotSetData.dev_info_t.wifi_AP_password, sizeof(IotSetData.dev_info_t.wifi_AP_password));
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.sta_enable.sta_authmode_open = 1;
#ifdef CONFIG_WIFI_STA_UNDHCP_ENABLE
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set.sta_enable.sta_undhcp_enable = 1;
#endif

    /*modbus 13500*/
    memcpy((uint8_t *)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13500_mesh.mesh_id, (uint8_t *)IotSetData.dev_info_t.WIFI_Mesh_ID, 6);

    /*modbus 13600*/
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.ble_protocol = IotSetData.dev_info_t.ble_protocol;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.blec_rssi_threshold = IotSetData.dev_info_t.blec_rssi_th;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.blec_switch_interval = IotSetData.dev_info_t.blec_switch_int;
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.bles_adv_key, IotSetData.dev_info_t.bles_adv_key, sizeof(IotSetData.dev_info_t.bles_adv_key));
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta_rssi_threshold = IotSetData.dev_info_t.wifi_sta_rssi_th;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta_switch_interval = IotSetData.dev_info_t.wifi_sta_switch_int;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_mul_sta_en = IotSetData.dev_info_t.wifi_mul_sta_en;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_ip = IotSetData.dev_info_t.wifi_sta1_ip;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_mask = IotSetData.dev_info_t.wifi_sta1_mask;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_gw = IotSetData.dev_info_t.wifi_sta1_gw;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_dns1 = IotSetData.dev_info_t.wifi_sta1_dns1;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta1_dns2 = IotSetData.dev_info_t.wifi_sta1_dns2;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_auth = IotSetData.dev_info_t.wifi_sta2_auth;
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_ssid, IotSetData.dev_info_t.wifi_sta2_ssid, sizeof(IotSetData.dev_info_t.wifi_sta2_ssid));
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_password, IotSetData.dev_info_t.wifi_sta2_password, sizeof(IotSetData.dev_info_t.wifi_sta2_password));
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_ip = IotSetData.dev_info_t.wifi_sta2_ip;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_mask = IotSetData.dev_info_t.wifi_sta2_mask;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_gw = IotSetData.dev_info_t.wifi_sta2_gw;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_dns1 = IotSetData.dev_info_t.wifi_sta2_dns1;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta2_dns2 = IotSetData.dev_info_t.wifi_sta2_dns2;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_auth = IotSetData.dev_info_t.wifi_sta3_auth;
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_ssid, IotSetData.dev_info_t.wifi_sta3_ssid, sizeof(IotSetData.dev_info_t.wifi_sta3_ssid));
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_password, IotSetData.dev_info_t.wifi_sta3_password, sizeof(IotSetData.dev_info_t.wifi_sta3_password));
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_ip = IotSetData.dev_info_t.wifi_sta3_ip;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_mask = IotSetData.dev_info_t.wifi_sta3_mask;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_gw = IotSetData.dev_info_t.wifi_sta3_gw;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_dns1 = IotSetData.dev_info_t.wifi_sta3_dns1;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.wifi_sta3_dns2 = IotSetData.dev_info_t.wifi_sta3_dns2;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.open_mqtt_enable = IotSetData.dev_info_t.open_mqtt_enable;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.open_mqtt_report_cycle = IotSetData.dev_info_t.open_mqtt_report_cycle;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.modbus_tcp_enable.all = IotSetData.dev_info_t.modbus_tcp_enable.all;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.modbus_tcp_port = IotSetData.dev_info_t.modbus_tcp_port;

    /*modbus 22000*/
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg22000_net_server_2rd.Net_Server_address, IotSetData.dev_info_t.Net_Server_address,	sizeof(IotSetData.dev_info_t.Net_Server_address));
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg22000_net_server_2rd.Net_Server_secret, IotSetData.dev_info_t.Net_Server_secret,  sizeof(IotSetData.dev_info_t.Net_Server_secret));

    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00000, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000, sizeof(MOD_STRUCT_reg00000));
    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00100_AppPage1, sizeof(MOD_STRUCT_reg00100));
    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg01100_Inv_base, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg01100_Inv_base, sizeof(MOD_STRUCT_reg01100));
    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg02000_Inv_base_set, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set, sizeof(MOD_STRUCT_reg02000));
    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg11000_IOT_info, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info, sizeof(MOD_STRUCT_reg11000));
    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg12000_IOT_set, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));
    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg13600_open, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open, sizeof(MOD_STRUCT_reg13600));
    memcpy(&top_modbus_rd.Inv[INV_MAX_NUM].mod_reg22000_net_server_2rd, &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg22000_net_server_2rd, sizeof(MOD_STRUCT_reg22000));

    memcpy(&top_modbus_wr.Inv, &top_modbus_rd.Inv[reals.Addr_can_self], sizeof(MOD_STRUCT_Inv));
}

/*------------------------------------------------------------------------------
 Function: delete_set_position
 -----------------------------------------------------------------------------*/
/**
  * @brief      释放设置
  * @param[in]  reg_position_list_t *position_list  
  * @param[out] None
  * @return     static void
  */
void delete_set_position(reg_position_list_t *position_list) {
    reg_position_list_t *curr_position = position_list;
    while (curr_position != NULL) {
        reg_position_list_t *next_position = curr_position->next;
        free(curr_position);
        curr_position = next_position;
    }
}

/**
  * @brief      信号量初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Modbus_21000_semaphore_init(void)
{
	modbus_21000_semaphore = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(modbus_21000_semaphore == NULL)
	{
        ESP_LOGE(TAG, "modbus_21000_semaphore create failed");
		return;
	}
	xSemaphoreGive(modbus_21000_semaphore);						/**< the resource is available after creation */
}

uint8_t Modbus_21000_semaphore_Take(void)
{
    if (!modbus_21000_semaphore || xSemaphoreTake(modbus_21000_semaphore, pdMS_TO_TICKS(200)) != pdTRUE)
    {
        ESP_LOGE(TAG,"modbus_21000_semaphore error");
        return 0; 
    }

    return 1;
}

/**
  * @brief      释放信号量
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Modbus_21000_semaphore_Give(void)
{
    xSemaphoreGive(modbus_21000_semaphore);  /*释放信号量*/
}

#define Modbus_21000_TAG "[Modbus_21000]"

/*产品型号ID，来自《产品型号(ASCII码)的代号分配表格》*/
typedef struct {
    const char *ascii;
    uint16_t id;
} dev_model_map_t;

static const dev_model_map_t dev_model_table[] = {
    // 添加便携电源设备类型判断代码
    {SN_TYPE_INV_AC200MAX_ASCII, SN_TYPE_INV_AC200MAX},     // 1
    {SN_TYPE_INV_AC200L_ASCII, SN_TYPE_INV_AC200L},         // 2
    {SN_TYPE_INV_AC300_ASCII, SN_TYPE_INV_AC300},           // 3
    {SN_TYPE_INV_AC500_ASCII, SN_TYPE_INV_AC500},           // 4
    {SN_TYPE_INV_AC70_ASCII, SN_TYPE_INV_AC70},             // 5
    {SN_TYPE_INV_AP300_ASCII, SN_TYPE_INV_AP300},           // 6
    {SN_TYPE_INV_PLP022_ASCII, SN_TYPE_INV_PLP022},         // 7
    {SN_TYPE_INV_ELITE200_V2_ASCII, SN_TYPE_INV_ELITE200_V2},// 8
    {SN_TYPE_INV_RV5_ASCII, SN_TYPE_INV_RV5},               // 9
    {SN_TYPE_FP_ASCII, SN_TYPE_INV_FP},                     // 10
    {SN_TYPE_INV_EL300_ASCII, SN_TYPE_INV_EL300},           // 11
    {SN_TYPE_INV_EB3A_ASCII, SN_TYPE_INV_EB3A},             // 12
    {SN_TYPE_INV_AC60_ASCII, SN_TYPE_INV_AC60},             // 13
    {SN_TYPE_INV_EB55_ASCII, SN_TYPE_INV_EB55},             // 14
    {SN_TYPE_INV_EB70_ASCII, SN_TYPE_INV_EB70},             // 15
    {SN_TYPE_INV_AC180T_SINGLE_ASCII, SN_TYPE_INV_AC180T_SINGLE},   // 16
    {SN_TYPE_INV_AC180T_DUAL_ASCII, SN_TYPE_INV_AC180T_DUAL},       // 17
    {SN_TYPE_INV_EP500_ASCII, SN_TYPE_INV_EP500},           // 18
    {SN_TYPE_INV_EP500PRO_ASCII, SN_TYPE_INV_EP500PRO},     // 19
    {SN_TYPE_INV_AC2A_ASCII, SN_TYPE_INV_AC2A},             // 20
    {SN_TYPE_INV_AC50B_ASCII, SN_TYPE_INV_AC50B},           // 21
    {SN_TYPE_INV_AC60P_ASCII, SN_TYPE_INV_AC60P},           // 22
    {SN_TYPE_INV_AC180_ASCII, SN_TYPE_INV_AC180},           // 23
    {SN_TYPE_INV_AC200P_ASCII, SN_TYPE_INV_AC200P},         // 24
    {SN_TYPE_INV_AC240_ASCII, SN_TYPE_INV_AC240},           // 25
    {SN_TYPE_INV_HANDSFREE1_ASCII, SN_TYPE_INV_HANDSFREE1}, // 26
    {SN_TYPE_INV_HANDSFREE2_ASCII, SN_TYPE_INV_HANDSFREE2}, // 27
    {SN_TYPE_INV_EL320_ASCII, SN_TYPE_INV_EL320},           // 28
    {SN_TYPE_INV_EL400_ASCII, SN_TYPE_INV_EL400},           // 29
    {SN_TYPE_INV_EL80V2_ASCII, SN_TYPE_INV_EL80V2},         // 30
    {SN_TYPE_INV_EL100V2_ASCII, SN_TYPE_INV_EL100},         // 31
    {SN_TYPE_INV_EL30V2_ASCII, SN_TYPE_INV_EL30V2},         // 32
    {SN_TYPE_INV_AC200PL_ASCII, SN_TYPE_INV_AC200PL},       // 33
    {SN_TYPE_INV_PR002_ASCII, SN_TYPE_INV_PR002},           // 34
    {SN_TYPE_INV_AC45_ASCII, SN_TYPE_INV_AC45},             // 35
    {SN_TYPE_INV_AC50P_ASCII, SN_TYPE_INV_AC50P},           // 36
    {SN_TYPE_INV_AC55_ASCII, SN_TYPE_INV_AC55},             // 37
    {SN_TYPE_INV_AC70P_ASCII, SN_TYPE_INV_AC70P},           // 38
    {SN_TYPE_INV_AC240P_ASCII, SN_TYPE_INV_AC240P},         // 39
    {SN_TYPE_INV_AC180P_ASCII, SN_TYPE_INV_AC180P},         // 40
    {SN_TYPE_INV_AC2P_ASCII, SN_TYPE_INV_AC2P},             // 41
    {SN_TYPE_INV_PREMIUM_20C_ASCII, SN_TYPE_INV_PREMIUM_20C}, // 42
    {SN_TYPE_INV_KW1000_ASCII, SN_TYPE_INV_KW1000},         // 43
    {SN_TYPE_INV_LFP700_ASCII, SN_TYPE_INV_LFP700},         // 44
    {SN_TYPE_INV_PR30V2_ASCII, SN_TYPE_INV_PR30V2},         // 45
    {SN_TYPE_INV_PR100V2_ASCII, SN_TYPE_INV_PR100V2},       // 46
    {SN_TYPE_INV_PR200V2_ASCII, SN_TYPE_INV_PR200V2},       // 47
    {SN_TYPE_INV_AORA10_ASCII, SN_TYPE_INV_AORA10},         // 48
    {SN_TYPE_INV_AORA30P_ASCII, SN_TYPE_INV_AORA30P},       // 49
    {SN_TYPE_INV_AORA30V2_ASCII, SN_TYPE_INV_AORA30V2},     // 50
    {SN_TYPE_INV_AORA_80_ASCII, SN_TYPE_INV_AORA_80},       // 51
    {SN_TYPE_INV_AORA_100_ASCII, SN_TYPE_INV_AORA_100},     // 52
    {SN_TYPE_INV_AORA100V2_ASCII, SN_TYPE_INV_AORA100V2},   // 53
    {SN_TYPE_INV_AORA200_ASCII, SN_TYPE_INV_AORA200},       // 54
    {SN_TYPE_INV_AORA320_ASCII, SN_TYPE_INV_AORA320},       // 55
    {SN_TYPE_INV_AP500_ASCII, SN_TYPE_INV_AP500},           // 56
    {SN_TYPE_INV_POWER5_ASCII, SN_TYPE_INV_POWER5},         // 57
    {SN_TYPE_INV_HS5_ASCII, SN_TYPE_INV_HS5},               // 58
    {SN_TYPE_INV_HS3_ASCII, SN_TYPE_INV_HS3},               // 59
    {SN_TYPE_INV_HS2_ASCII, SN_TYPE_INV_HS2},               // 60
    {SN_TYPE_INV_AORA400_ASCII, SN_TYPE_INV_AORA400},       // 61
    {SN_TYPE_INV_EL10_ASCII, SN_TYPE_INV_EL10},             // 62
    {SN_TYPE_INV_EL100MINI_ASCII, SN_TYPE_INV_EL100MINI},   // 63
    {SN_TYPE_INV_EL30MINI_ASCII, SN_TYPE_INV_EL30MINI},     // 64
    {SN_TYPE_INV_EL200MINI_ASCII, SN_TYPE_INV_EL200MINI},   // 65
    {SN_TYPE_INV_AORA100MINI_ASCII, SN_TYPE_INV_AORA100MINI}, // 66
    {SN_TYPE_INV_AORA30MINI_ASCII, SN_TYPE_INV_AORA30MINI},   // 67
    {SN_TYPE_INV_AORA200MINI_ASCII, SN_TYPE_INV_AORA200MINI}, // 68
    {SN_TYPE_INV_AP200_ASCII, SN_TYPE_INV_AP200},           // 69
    {SN_TYPE_INV_AP300V2_ASCII, SN_TYPE_INV_AP300V2},       // 70
    {SN_TYPE_INV_EL200V3_ASCII, SN_TYPE_INV_EL200V3},       // 71
    {SN_TYPE_INV_EL250V3_ASCII, SN_TYPE_INV_EL250V3},       // 72
    {SN_TYPE_INV_HYBRID3_ASCII, SN_TYPE_INV_HYBRID3},       // 73
    {SN_TYPE_INV_AORA200V3_ASCII, SN_TYPE_INV_AORA200V3},   // 74
    {SN_TYPE_INV_AORA250V3_ASCII, SN_TYPE_INV_AORA250V3},   // 75
    {SN_TYPE_INV_AORA300_ASCII, SN_TYPE_INV_AORA300},       // 76
    {SN_TYPE_INV_EL100V3_ASCII, SN_TYPE_INV_EL100V3},       // 77
    {SN_TYPE_INV_AORA100V3_ASCII, SN_TYPE_INV_AORA100V3},   // 78
    {SN_TYPE_INV_PR150V3_ASCII, SN_TYPE_INV_PR150V3},       // 79
    {SN_TYPE_INV_AORA150V3_ASCII, SN_TYPE_INV_AORA150V3},   // 80
    {SN_TYPE_INV_AORA200V2_ASCII, SN_TYPE_INV_AORA200V2},   // 81

    // 户储
    {SN_TYPE_EP600_ASCII, SN_TYPE_EP600},                   // 1000
    {SN_TYPE_EP760_ASCII, SN_TYPE_EP760},                   // 1001
    {SN_TYPE_EP800_ASCII, SN_TYPE_EP800},                   // 1002
    {SN_TYPE_EP900_ASCII, SN_TYPE_EP900},                   // 1003
    {SN_TYPE_EP2000_ASCII, SN_TYPE_EP2000},                 // 1004
    {SN_TYPE_EP13K_ASCII, SN_TYPE_EP13K},                   // 1005
    {SN_TYPE_EP6K_ASCII, SN_TYPE_EP6K},                     // 1006
    {SN_TYPE_EP18K_ASCII, SN_TYPE_EP18K},                   // 1007
    {SN_TYPE_EP5K_ASCII, SN_TYPE_EP5K},                     // 1008
    {SN_TYPE_EP5K_ASCII, SN_TYPE_EP5K5},                    // 1009
    {SN_TYPE_AP12K_ASCII, SN_TYPE_AP12K},                    // 1010
    {SN_TYPE_NPP004_ASCII, SN_TYPE_NPP004},                 // 1051
    {SN_TYPE_NPP005_ASCII, SN_TYPE_NPP005},                 // 1052

    // 微逆
    {SN_TYPE_A80_ASCII, SN_TYPE_A80},                       // 2000
    {SN_TYPE_D100S_ASCII, SN_TYPE_D100S},                   // 2001
    {SN_TYPE_A100_Sub1G_ASCII, SN_TYPE_A100_Sub1G},         // 2002
    {SN_TYPE_A100_WIFI_ASCII, SN_TYPE_A100_WIFI},           // 2003
    {SN_TYPE_D100P_ASCII, SN_TYPE_D100P},                   // 2004
    {SN_TYPE_Will_ASCII, SN_TYPE_Will},                     // 2005

    // 配件
    {SN_TYPE_S1_ASCII, SN_TYPE_S1},                         // 3000
    {SN_TYPE_AT1_ASCII, SN_TYPE_AT1},                       // 3001
    {SN_TYPE_COMBOX_ASCII, SN_TYPE_COMBOX},                 // 3002
    {SN_TYPE_PBOX_ASCII, SN_TYPE_PBOX},                     // 3003
    {SN_TYPE_EBOX_ASCII, SN_TYPE_EBOX},                     // 3004
    {SN_TYPE_HMI_ASCII, SN_TYPE_HMI},                       // 3005
    {SN_TYPE_PANEL_ASCII, SN_TYPE_PANEL},                   // 3006
    {SN_TYPE_DCHUB_ASCII, SN_TYPE_DCHUB},                   // 3007
    {SN_TYPE_ACHUB_ASCII, SN_TYPE_ACHUB},                   // 3008
    {SN_TYPE_SOLARX4K_ASCII, SN_TYPE_SOLARX4K},             // 3009
    {SN_TYPE_CHARGER1_ASCII, SN_TYPE_CHARGER1},             // 3010
    {SN_TYPE_CHARGER2_ASCII, SN_TYPE_CHARGER2},             // 3011
    {SN_TYPE_PACK_BOX_ASCII, SN_TYPE_PACK_BOX},             // 3012
    {SN_TYPE_BLE_HMI_ASCII, SN_TYPE_BLE_HMI},               // 3013
    {SN_TYPE_IOT_INSIDE_ASCII, SN_TYPE_IOT_INSIDE},         // 3014
    {SN_TYPE_SHELLY_METER_ASCII, SN_TYPE_SHELLY_METER},     // 3015
    {SN_TYPE_METER_ASCII, SN_TYPE_METER},                   // 3018

    // 电池包
    {SN_TYPE_B500_ASCII, SN_TYPE_B500},                     // 4000
    {SN_TYPE_B500H_ASCII, SN_TYPE_B500H},                   // 4001
    {SN_TYPE_IB500_ASCII, SN_TYPE_IB500},                   // 4002
    {SN_TYPE_B1210_ASCII, SN_TYPE_B1210},                   // 4003
    {SN_TYPE_B4810_ASCII, SN_TYPE_B4810},                   // 4004
    {SN_TYPE_B300_ASCII, SN_TYPE_B300},                     // 4005
    {SN_TYPE_B300K_ASCII, SN_TYPE_B300K},                   // 4006
    {SN_TYPE_B300S_ASCII, SN_TYPE_B300S},                   // 4007
    {SN_TYPE_B1232_ASCII, SN_TYPE_B1232},                   // 4008
    {SN_TYPE_LEADACID_ASCII, SN_TYPE_LEADACID},             // 4009
    {SN_TYPE_LFP_ASCII, SN_TYPE_LFP},                       // 4010
    {SN_TYPE_B500A_ASCII, SN_TYPE_B500A},                   // 4012
    {SN_TYPE_BC200_ASCII, SN_TYPE_BC200},                   // 4013
    {SN_TYPE_B230_ASCII, SN_TYPE_B230},                     // 4014
    {SN_TYPE_B210_ASCII, SN_TYPE_B210},                     // 4015
    {SN_TYPE_B500K_ASCII, SN_TYPE_B500K},                   // 4016
    {SN_TYPE_B900_ASCII, SN_TYPE_B900},                     // 4017
    {SN_TYPE_HB500_ASCII, SN_TYPE_HB500},                   // 4018
    {SN_TYPE_BC260_ASCII, SN_TYPE_BC260},                   // 4019
    {SN_TYPE_EK900_ASCII, SN_TYPE_EK900},                   // 4020
    {SN_TYPE_IB800JP_ASCII, SN_TYPE_IB800JP},               // 4021
    {SN_TYPE_B500PRO_ASCII, SN_TYPE_B500PRO},               // 4022
    {SN_TYPE_B300PRO_ASCII, SN_TYPE_B300PRO},               // 4023
    {SN_TYPE_IB800_ASCII, SN_TYPE_IB800},                   // 4024    
    {SN_TYPE_HB500S_ASCII, SN_TYPE_HB500S},                 // 4025
    {SN_TYPE_BH500E_ASCII, SN_TYPE_BH500E},                 // 4026
    {SN_TYPE_B4805_ASCII, SN_TYPE_B4805},                   // 4027  

    // 通用设备伪装ASCII
    {SN_TYPE_COMMON_DEVICE_ASCII,        SN_TYPE_COMMON_DEVICE},               // 30000
    {SN_TYPE_COMMON_BATTERY_ASCII,       SN_TYPE_COMMON_BATTERY},              // 30001
    {SN_TYPE_COMMON_INVERTER_ASCII,      SN_TYPE_COMMON_INVERTER},             // 30002
    {SN_TYPE_COMMON_DC_CHARGER_ASCII,    SN_TYPE_COMMON_DC_CHARGER},           // 30003
    {SN_TYPE_COMMON_DC_DISCHARGER_ASCII, SN_TYPE_COMMON_DC_DISCHARGER},        // 30004
    {SN_TYPE_COMMON_DEVICE_CAN_ASCII,    SN_TYPE_COMMON_DEVICE_CAN},           // 30005
    {SN_TYPE_COMMON_BATTERY_CAN_ASCII,   SN_TYPE_COMMON_BATTERY_CAN},          // 30006
    {SN_TYPE_COMMON_INVERTER_CAN_ASCII,  SN_TYPE_COMMON_INVERTER_CAN},         // 30007
    {SN_TYPE_COMMON_DCCHG_CAN_ASCII,     SN_TYPE_COMMON_DC_CHARGER_CAN},       // 30008
    {SN_TYPE_COMMON_DCDCHG_CAN_ASCII,    SN_TYPE_COMMON_DC_DISCHARGER_CAN},    // 30009
    {SN_TYPE_COMMON_DEV_WL_ASCII,        SN_TYPE_COMMON_DEVICE_WIRELESS},      // 30010
    {SN_TYPE_COMMON_BAT_WL_ASCII,        SN_TYPE_COMMON_BATTERY_WIRELESS},     // 30011
    {SN_TYPE_COMMON_INV_WL_ASCII,        SN_TYPE_COMMON_INVERTER_WIRELESS},    // 30012
    {SN_TYPE_COMMON_DCCHG_WL_ASCII,      SN_TYPE_COMMON_DC_CHARGER_WIRELESS},  // 30013
    {SN_TYPE_COMMON_DCDCHG_WL_ASCII,     SN_TYPE_COMMON_DC_DISCHARGER_WIRELESS}// 30014    
};

static const size_t dev_model_table_len = sizeof(dev_model_table) / sizeof(dev_model_table[0]);

/**
  * @brief 根据设备类型转换对应类型号码
  * @param[in]  char *type 设备类型字符串
  * @param[out] None
  * @return 设备型号编码
  */
uint16_t SN_TYPE_ASCII_TO_NUM(const char *type)
{
    if (!type || !strnlen(type, TYPE_SIZE)) {
        return 0;
    }
    size_t type_len = strlen(type);
    for (size_t i = 0; i < dev_model_table_len; ++i) {
        // 1. 先比较长度是否完全相等
        if (type_len == strlen(dev_model_table[i].ascii)) {
            // 2. 长度相等再比较内容是否完全相等
            if (0 == strcmp(type, dev_model_table[i].ascii)) {
                return dev_model_table[i].id; // 找到唯一精确匹配
            }
        }
    }
    return 0; // 未找到精确匹配
}

/*------------------------------------------------------------------------------
 Function: SN_TYPE_NUM_TO_ASCII
 -----------------------------------------------------------------------------*/
/**
  * @brief      根据设备类型序号填充对应的字符串到缓冲区 (线程安全，可重入)
  * @param[in]  uint16_t type_num
  * @param[out] out_buffer   用于存储结果的缓冲区
  * @param[in]  buffer_size  缓冲区大小
  * @return     bool         如果成功找到并填充则返回true，否则返回false
  */
bool SN_TYPE_NUM_TO_ASCII(uint16_t type_num, char* out_buffer, size_t buffer_size) 
{
    if (!out_buffer || buffer_size == 0) {
        return false;
    }

    const char* src = NULL;

    // 遍历查找表以找到匹配的ID
    for (size_t i = 0; i < dev_model_table_len; ++i) {
        if (dev_model_table[i].id == type_num) {
            src = dev_model_table[i].ascii;
            break;
        }
    }

    // 如果未找到匹配项
    if (!src) {
        // 清空输出缓冲区
        memset(out_buffer, 0x00, buffer_size);
        return false;
    }

    // 将源字符串复制到输出缓冲区，并用0x00填充剩余部分
    memset(out_buffer, 0x00, buffer_size);
    size_t len = strlen(src);
    if (len >= buffer_size) {
        // 防止溢出，确保最后一个字节是'\0'
        len = buffer_size - 1;
    }
    memcpy(out_buffer, src, len);

    return true;
}

// 电池包类型统计结构
typedef struct {
    char type_ascii[TYPE_SIZE];      // 类型名称
    uint8_t count;         // 电池包数量
    uint8_t version_count; // 版本数量
    modbus_soft_version soft[10]; // 10组软件版本信息
    uint8_t slaveaddr;     // 对应从机地址
} modbus_pack_ver_stat_t;

#define PACK_SUPPORT_TYPE_MAX            20

// 电池包类型统计数组
USE_EXT_RAM_BSS static modbus_pack_ver_stat_t pack_type_stats[PACK_SUPPORT_TYPE_MAX] = {0};
static bool pack_ver_busy = false;
static bool pack_ver_mix = false;
static uint8_t pack_type_count = 0;

/**
 * @brief 获取电池包是否处于繁忙状态
 * @return true 繁忙，false 空闲
 */
bool get_pack_ver_busy(void)
{
    return pack_ver_busy;
}

/**
 * @brief 获取当前是否为混包状态
 * @return true 混包，false 非混包
 */
bool get_pack_ver_mix(void)
{
    return pack_ver_mix;
}

/**
 * @brief 获取当前统计到的电池包类型数量
 * @return 类型数量
 */
uint8_t get_pack_type_count(void)
{
    return pack_type_count;
}

/**
 * @brief 根据电池包类型查找对应的slaveaddr
 *
 * @param[in]  type_ascii  电池包类型
 * @return uint8_t      对应的slaveaddr（加上MD_PACK_SUM_ADDR_START偏移），找不到返回MD_PACK_SUM_ADDR_END
 */
static uint8_t get_slaveaddr_by_type_ascii(char* type_ascii)
{
    if ( type_ascii ) {
        for (int i = 0; i < PACK_SUPPORT_TYPE_MAX; i++) {
            if (0 == strncmp(pack_type_stats[i].type_ascii, type_ascii, TYPE_SIZE)) {
                return (pack_type_stats[i].slaveaddr + MD_PACK_SUM_ADDR_START);
            }
        }
    }
    
    return MD_PACK_SUM_ADDR_END;
}

/**
 * @brief 添加电池包版本信息到类型统计数组
 *
 * 功能说明：
 * 1. 根据type_ascii查找pack_type_stats中的对应类型项
 * 2. 统计该类型电池包数量。
 * 3. 遍历输入的版本信息数组，按type和版本前缀（version/100）分组，统计每组的最小版本号。
 * 4. 每种类型最多记录10组不同type/前缀的版本信息。
 *
 * @param[in] type_ascii 电池包类型
 * @param[in] soft_num   版本信息数量
 * @param[in] ver_info   版本信息数组指针
 */
static void add_pack_version_to_stats(char* type_ascii, uint16_t soft_num, modbus_soft_version *ver_info)
{
    // 搜索电池包类型
    uint8_t addr_finded = 0;
    uint8_t index = 0;
    for (int i = 0; i < PACK_SUPPORT_TYPE_MAX; i++) {
        if (0 == strncmp(pack_type_stats[i].type_ascii, type_ascii, TYPE_SIZE)) {
            pack_type_stats[i].count++;
            addr_finded = 1;
            index = i;
            break;
        }
    }

    // 未存储类型，寻找空位新增
    if ( !addr_finded ) {
        for (int i = 0; i < PACK_SUPPORT_TYPE_MAX; i++) {
            if (0 == pack_type_stats[i].count) {
                memcpy(pack_type_stats[i].type_ascii, type_ascii, TYPE_SIZE);
                pack_type_stats[i].count++;
                addr_finded = 1;
                index = i;
                break;
            }
        }
    }

    // 检查参数位置
    if (addr_finded == 0 || soft_num == 0 || !ver_info) return;
    
    // 添加版本号
    for (int i = 0; i < soft_num; i++) {
        uint16_t type = ver_info[i].type;
        uint32_t version = ver_info[i].version;
        if (type == 0 || version == 0) continue;

        uint32_t version_prefix = version / 100;

        // 查找是否已记录
        int found = 0;
        for (int j = 0; j < pack_type_stats[index].version_count; j++) {
            if (pack_type_stats[index].soft[j].type == type &&
                pack_type_stats[index].soft[j].version / 100 == version_prefix) {
                if (version < pack_type_stats[index].soft[j].version) {
                    pack_type_stats[index].soft[j].version = version;
                }
                found = 1;
                break;
            }
        }

        // 新记录
        if (!found) {
            if (pack_type_stats[index].version_count < 10) {
                uint8_t add_index = pack_type_stats[index].version_count;
                pack_type_stats[index].soft[add_index].type = type;
                pack_type_stats[index].soft[add_index].version = version;
                pack_type_stats[index].version_count++;
            }
        }
    }
}

/**
 * @brief 统计所有在线电池包的类型和软件版本信息，并分配从机地址
 *
 * 功能说明：
 * 1. 清空类型统计数组，每次重新统计。
 * 2. 遍历所有在线电池包，统计类型和版本信息。
 * 3. 为每种类型分配唯一的slaveaddr（从机地址），并同步到主寄存器。
 * 4. 检查slaveaddr是否有变化，若有变化则change_flag置位（用于后续通知主机）。
 * 5. 检查B300/B300S类型是否有多包，若有则设置繁忙标志（pack_ver_busy）。
 * 6. 判断是否为混包（多种类型），设置混包标志（pack_ver_mix）。
 *
 * @return uint8_t 是否有新增类型汇总
 */
static uint8_t Modbus_Pack_Type_Ver_Check(void)
{
    uint8_t index = 0;      // 当前分配的从机地址
    uint8_t change_flag = 0;// 地址变化标志
    uint8_t is_busy = 0;    // 电池包繁忙标志
    memset(pack_type_stats, 0, sizeof(pack_type_stats));

    // 遍历所有在线电池包，统计类型和版本信息
#ifdef INTERNAL_PACK_ENABLE
    for (uint8_t num = 1; num < reals.online_Pack_num; num++) 
#else    
    for (uint8_t num = 0; num < reals.online_Pack_num; num++) 
#endif   
    {
        add_pack_version_to_stats(
            top_modbus_rd.Pack[num].mod_reg06100_Pack_each.type_ascii,
            top_modbus_rd.Pack[num].mod_reg06100_Pack_each.software_total,
            top_modbus_rd.Pack[num].mod_reg06100_Pack_each.soft
        );
    }

    // 为每种类型分配唯一的slaveaddr，并同步到主寄存器，检测是否有变化
    for (uint8_t i = 0; i < PACK_SUPPORT_TYPE_MAX; i++) {
        if (pack_type_stats[i].count > 0) { // 该类型有在线电池包
            pack_type_stats[i].slaveaddr = index; // 分配从机地址

            // 同步类型信息到主寄存器
            if (strncmp(top_modbus_rd.Type_Pack[index].mod_reg06100_Pack_each.type_ascii, pack_type_stats[i].type_ascii, TYPE_SIZE)) {
                memcpy(top_modbus_rd.Type_Pack[index].mod_reg06100_Pack_each.type_ascii, pack_type_stats[i].type_ascii, TYPE_SIZE);
                change_flag |= 1;
            }
            top_modbus_rd.Type_Pack[index].mod_reg06100_Pack_each.sn_code = dev_factory.dev_sn;
            top_modbus_rd.Type_Pack[index].mod_reg06100_Pack_each.software_total = pack_type_stats[i].version_count;
            memcpy(top_modbus_rd.Type_Pack[index].mod_reg06100_Pack_each.soft, pack_type_stats[i].soft, sizeof(top_modbus_rd.Type_Pack[index].mod_reg06100_Pack_each.soft));

            // B300、B300S类型不支持并包升级，若有多包则设置繁忙标志
            if (pack_type_stats[i].count > 1 
                && (0 == strncmp(pack_type_stats[i].type_ascii, SN_TYPE_B300_ASCII, TYPE_SIZE) 
                    || 0 == strncmp(pack_type_stats[i].type_ascii, SN_TYPE_B300S_ASCII, TYPE_SIZE))) {
                is_busy |= 1;
            }

            // 当前协议地址预留最多支持MODBUS_PACK_TYPE_MAX种电池包
            if ( ++index >= MODBUS_PACK_TYPE_MAX ) {
                break;
            }
        }
    }

    // 电池包类型数量
    if ( pack_type_count != index ) {
        pack_type_count = index;
        change_flag |= 1;
    }

    // 设置电池包繁忙标志
    pack_ver_busy = (is_busy > 0) ? true : false;

    // 判断是否为混包（多种类型）
    pack_ver_mix = (index > 1) ? true : false;

#if 1   
    if ( change_flag ) {
        ESP_LOGI(Modbus_21000_TAG, "/***************************pack_type_stats****************************/");
        for (uint8_t i = 0; i < PACK_SUPPORT_TYPE_MAX; i++) {
            if (pack_type_stats[i].count > 0) {
                ESP_LOGI(Modbus_21000_TAG, "type_ascii: %s, count: %u, version_count: %u, slaveaddr: %u",
                    pack_type_stats[i].type_ascii,
                    pack_type_stats[i].count,
                    pack_type_stats[i].version_count,
                    pack_type_stats[i].slaveaddr);
                for (uint8_t j = 0; j < pack_type_stats[i].version_count; j++) {
                    ESP_LOGI(Modbus_21000_TAG, "  soft[%u]: type=%u, version=%lu",
                        j,
                        pack_type_stats[i].soft[j].type,
                        pack_type_stats[i].soft[j].version);
                }
            }
        }
        ESP_LOGI(Modbus_21000_TAG, "/*********************************************************************/");
    }
#endif

    // change_flag用于通知主机地址变化
    return change_flag;
}


/*------------------------------------------------------------------------------
 Function: INV_Point_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      逆变状态更新
  * @param[in]  uint8_t index  
                uint8_t x      
                uint8_t y      
  * @param[out] None
  * @return     void
  */
static uint16_t INV_Point_State_Update(uint8_t x)
{
    uint8_t alarm_status = 0;
    uint8_t fault_status = 0;
    POINT_STATE state = {0};

    /*在线状态*/
    state.bit.point_online = 1;
    
    /*告警状态*/
    for ( uint8_t i = 0 ; i < 4; i++ )
    {
        if ( top_modbus_rd.Inv[x].mod_reg00100_AppPage1.alarm[i] != 0 )
        {
            alarm_status |= 1;
            break;
        }
    }
   state.bit.alarm = alarm_status;
    
    /*逆变故障状态*/
    for ( uint8_t i = 0 ; i < 6; i++ )
    {
        if ( top_modbus_rd.Inv[x].mod_reg00100_AppPage1.fault[i] != 0 )
        {
            fault_status |= 1;
            break;
        }
    }
    
    /*IOT故障状态（与逆变共用）*/
    if ( x == INV_MAX_NUM ) {
        if(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_fault.all) fault_status |= 1;
    } else {
        if(top_modbus_rd.Inv[x].mod_reg11000_IOT_info.iot_fault.all) fault_status |= 1;
    }
    state.bit.protect = fault_status;

    /*内置电池包故障告警状态*/
    if ( x == INV_MAX_NUM ) {
        uint8_t bat_status = 0; 
        /*保护状态*/
        if ( (top_modbus_rd.Pack[0].mod_reg06100_Pack_each.protect)
            || (top_modbus_rd.Pack[0].mod_reg06100_Pack_each.SysErr[0])
            || (top_modbus_rd.Pack[0].mod_reg06100_Pack_each.SysErr[1])
            || (top_modbus_rd.Pack[0].mod_reg06100_Pack_each.SysErr[2]))
        {
            bat_status |= 1;
        } 
            
        /*告警状态*/
        if ( (top_modbus_rd.Pack[0].mod_reg06100_Pack_each.alarm1)
            || (top_modbus_rd.Pack[0].mod_reg06100_Pack_each.alarm2)
            || (top_modbus_rd.Pack[0].mod_reg06100_Pack_each.alarm3))
        {
            bat_status |= 1;
        } 

        state.bit.bat_alarm = bat_status;
    } 
    
    /*升级状态*/
    uint8_t update_need = Get_http_new_version_flag(HTTPS_CHECK_INV_IMAGE, 0)
                            | Get_http_new_version_flag(HTTPS_CHECK_IOT_IMAGE, 0);
    
    state.bit.update_need = update_need;

    return state.all;
}

/*------------------------------------------------------------------------------
 Function: Pack_Point_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      PACK状态更新
  * @param[in]  uint8_t addr  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t Pack_Point_State_Update(uint8_t addr, uint8_t group_slaveaddr)
{
    /*在线状态*/
    POINT_STATE state = {0};
    state.bit.point_online = 1;
    
    /*保护状态*/
    if ( (top_modbus_rd.Pack[addr].mod_reg06100_Pack_each.protect)
		|| (top_modbus_rd.Pack[addr].mod_reg06100_Pack_each.SysErr[0])
		|| (top_modbus_rd.Pack[addr].mod_reg06100_Pack_each.SysErr[1])
		|| (top_modbus_rd.Pack[addr].mod_reg06100_Pack_each.SysErr[2]))
	{
        state.bit.protect = 1;
    } 
        
    /*告警状态*/
    if ( (top_modbus_rd.Pack[addr].mod_reg06100_Pack_each.alarm1)
        || (top_modbus_rd.Pack[addr].mod_reg06100_Pack_each.alarm2)
        || (top_modbus_rd.Pack[addr].mod_reg06100_Pack_each.alarm3))
    {
        state.bit.alarm = 1;
    } 
        
    /*升级状态*/
    if ( group_slaveaddr >= MD_PACK_SUM_ADDR_START  ) {
        uint8_t update_need = Get_http_new_version_flag(HTTPS_CHECK_PACK_IMAGE, (group_slaveaddr - MD_PACK_SUM_ADDR_START));
        state.bit.update_need = update_need;
    }
    
    return state.all;
}

/*------------------------------------------------------------------------------
 Function: DCHUB_Point_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      DCHUB状态更新
  * @param[in]  uint8_t index  
                uint8_t x      
                uint8_t y      
  * @param[out] None
  * @return     void
  */
static uint16_t DCHUB_Point_State_Update(uint8_t x)
{
    uint8_t fault_status = 0;
    POINT_STATE state = {0};

    /*在线状态*/
    state.bit.point_online = 1;
    
    /*告警状态*/
    state.bit.alarm = 0;
    
    /*故障状态*/
    if ((top_modbus_rd.DcHub[x].mod_reg00100_AppPage1.fault[1] 
        | top_modbus_rd.DcHub[x].mod_reg00100_AppPage1.fault[4]) != 0 ) fault_status |= 1;
    state.bit.protect = fault_status;

    /*升级状态*/
    uint8_t update_need = Get_http_new_version_flag(HTTPS_CHECK_DCHUB_IMAGE, 0);
    state.bit.update_need = update_need;

    return state.all;
}

/*------------------------------------------------------------------------------
 Function: DCDC_Point_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      DCDC状态更新
  * @param[in]  uint8_t x  
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t DCDC_Point_State_Update(uint8_t x)
{
    uint8_t alarm_status = 0;
    uint8_t fault_status = 0;
    POINT_STATE state = {0};

    /*在线状态*/
    state.bit.point_online = 1;
    
    /*告警状态*/
    if (top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.alarm_dcdc) alarm_status |= 1;
    state.bit.alarm = alarm_status;
    
    /*故障状态*/
    if (top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_charge2 
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dcdc
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dc1
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dc2
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dc3
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dc4
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dc5
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dc6
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.fault_dcdc 
        || top_modbus_rd.Dcdc[x].mod_reg15500_DCDC_info.proctect_dcdc )
    {
        fault_status |= 1;
    }
    state.bit.protect = fault_status;

    /*升级状态*/
    uint8_t update_need = Get_http_new_version_flag(HTTPS_CHECK_DCDC_IMAGE, 0);
    state.bit.update_need = update_need;

    return state.all;
}

/*------------------------------------------------------------------------------
 Function: Dev_All_Point_State_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      系统内所有设备状态检查
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void Dev_All_Point_State_Check(void)
{ 
    uint16_t status = 0;
    uint8_t State_change_flag = 0;
    static uint8_t online_vaild_num = 0;
    static uint32_t pre_time = 0;
    
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    /*升级状态下不检查*/
    if (sys_is_updating()) {
        pre_time = now_time;
        return;
    }

    /*指定周期检查*/
    if((now_time - pre_time) < 2000) return;

    pre_time = now_time;
    
    if (!Modbus_21000_semaphore_Take())
        return;

    uint8_t online_num = 0;
    
    /*本机汇总*/
    if((0 != strnlen(dev_factory.dev_type, TYPE_SIZE)) 
        && (0 != dev_factory.dev_sn ))//非0，有节点内容
    {
        online_num++;
        status = INV_Point_State_Update(INV_MAX_NUM);
        if ( status != Old_Inv_Status[INV_MAX_NUM] )
        {
            State_change_flag |= 1;
            Old_Inv_Status[INV_MAX_NUM] = status;
        }
    }

    /*逆变（仅并机有效）*/
    if ( INV_MAX_NUM > 1 && reals.online_Inv_num > 1 )
    {
        for ( int i = 0 ; i < INV_MAX_NUM ; i++ )
        {
            if((can_node_rd.Inv[i].online)
                && (0 != strnlen(top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvType, TYPE_SIZE))
                && (0 != top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvSN)//非0，有节点内容
                && (dev_factory.dev_sn != top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvSN)//非本机
                )
            {
                online_num++;
                status = INV_Point_State_Update(i);
                if ( status != Old_Inv_Status[i] )
                {
                    State_change_flag |= 1;
                    Old_Inv_Status[i] = status;
                }
            }
        }
    }

    /*PACK*/
    if ( reals.online_Pack_num > 0 )
    {
        for ( int i = 0 ; i < PACK_MAX_NUM ; i++ ) 
        {
#ifdef	BAT_CAN_PROTOCOL_ALPHA_ENABLE     
            bat_data_t *bms_data = get_bat_data_ptr();
            /*电池包在线则继续执行*/
            if (bms_data->bms_data[i].valid && strnlen(bms_data->bms_data[i].type, TYPE_SIZE))
            {
                online_num++;
                status = Pack_Point_State_Update(i, get_slaveaddr_by_type_ascii(bms_data->bms_data[i].type));
                if (( status != Old_Pack_Status[i] ) 
                    || (Old_Pack_SN_Code[i] != bms_data->bms_data[i].sn_code))
                {
                    State_change_flag |= 1;
                    Old_Pack_Status[i] = status;
                    Old_Pack_SN_Code[i] = bms_data->bms_data[i].sn_code;
                }
            }
#endif  

#ifdef	BAT_CAN_PROTOCOL_BETA_ENABLE        
            /*电池包在线则继续执行*/
            if (can_node_rd.Pack[i].online && strnlen(can_node_rd.Pack[i].pack_about.type_ascii, TYPE_SIZE))
            {
                online_num++;
                status = Pack_Point_State_Update(i, get_slaveaddr_by_type_ascii(can_node_rd.Pack[i].pack_about.type_ascii));
                if (( status != Old_Pack_Status[i] ) 
                    || (Old_Pack_SN_Code[i] != can_node_rd.Pack[i].pack_about.sn_code))
                {
                    State_change_flag |= 1;
                    Old_Pack_Status[i] = status;
                    Old_Pack_SN_Code[i] = can_node_rd.Pack[i].pack_about.sn_code;
                }
            }
#endif            
        }
    }

    /*电池包分类型版本信息统计*/
    State_change_flag |= Modbus_Pack_Type_Ver_Check();
    if(State_change_flag) reals.need_new_version_flag.sBit.pack = 1;

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    /*DCHUB*/
    if ( reals.online_DCHUB_num > 0 )
    {
        for ( int i = 0 ; i < DC_HUB_MAX_NUM ; i++ )
        {
            if((can_node_rd.DCHUB[i].online)
                && (0 != top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_sn)//非0，有节点内容
                && (0 != strnlen(top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_type, TYPE_SIZE))
                )
            {
                online_num++;
                status = DCHUB_Point_State_Update(i);
                if ( status != Old_DCHUB_Status[i] )
                {
                    State_change_flag |= 1;
                    Old_DCHUB_Status[i] = status;
                }
            }
        }
    }
#endif

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
        /*DCDC*/
        if ( reals.online_Dcdc_num > 0 )
        {
            for ( int i = 0 ; i < DCDC_MAX_NUM ; i++ )
            {
                if((can_node_rd.Dcdc[i].online)
                    && (top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn > 0xFF)
                    && (0 != strnlen(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, TYPE_SIZE))
                    )
                {
                    online_num++;
                    status = DCDC_Point_State_Update(i);
                    if ( status != Old_DCDC_Status[i] )
                    {
                        State_change_flag |= 1;
                        Old_DCDC_Status[i] = status;
                    }
                }
            }
        }
#endif

#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE
    /*智能插座状态检查*/
    State_change_flag |= plug_info_status_check();
#endif

    /*设备状态发生变化*/
    if (( State_change_flag ) 
        || (online_num != online_vaild_num)) {
        online_vaild_num = online_num;
        reals.net_point_Comein = 1;
    }

    Modbus_21000_semaphore_Give();
}

#define MODBUS_21000_DEBUG_LOG

/**
  * @brief      INV类型设备统计
  * @param[in]  POINT_BIND_INFO *point_bind_info
                bool is_all
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t INV_Modbus_Device_Report_Process(POINT_BIND_INFO *point_bind_info, bool is_all)
{
    uint8_t point_cnt = 0;
    
    if ( INV_MAX_NUM > 1 && reals.online_Inv_num > 1 )
    {
        for ( int i = 0 ; i < INV_MAX_NUM ; i++ )
        {
            if((can_node_rd.Inv[i].online)
                && (0 != strnlen(top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvType, TYPE_SIZE))
                && (0 != top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvSN)//非0，有节点内容
                && (dev_factory.dev_sn != top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvSN)//非本机
                )
            {
                uint16_t dev_type = SN_TYPE_ASCII_TO_NUM(top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvType);
                point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_INVERTER;
                point_bind_info[point_cnt].SN_64 = top_modbus_rd.Inv[i].mod_reg01100_Inv_base.InvSN;
                point_bind_info[point_cnt].group_same_type_addr = 0;//预留
                point_bind_info[point_cnt].slave_addr = i + MD_INV_ADDR_START;
                point_bind_info[point_cnt].master_addr = MD_SUMMARY_ADDR;
                point_bind_info[point_cnt].group_addr = 0;
                point_bind_info[point_cnt].state.all = INV_Point_State_Update(i);
                point_cnt++;
            }
        }
    }

    return point_cnt;
}

/**
  * @brief      Pack类型设备统计
  * @param[in]  POINT_BIND_INFO *point_bind_info
                bool is_all
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t Pack_Modbus_Device_Report_Process(POINT_BIND_INFO *point_bind_info, bool is_all)
{
    uint8_t point_cnt = 0;

    if ( reals.online_Pack_num > 0 )
    {
#ifdef INTERNAL_PACK_ENABLE
        for ( int i = 1 ; i < PACK_MAX_NUM ; i++ ) // 忽略0号主包
#else
        for ( int i = 0 ; i < PACK_MAX_NUM ; i++ ) 
#endif
        {
            char *type_ascii = NULL;
            uint16_t dev_type = 0;
            uint64_t sn_code = 0;
            uint8_t online = 0;
            
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
            bat_data_t *bms_data = get_bat_data_ptr();
            if (bms_data->bms_data[i].valid && strnlen(bms_data->bms_data[i].type, TYPE_SIZE)) {
                online = bms_data->bms_data[i].valid;
                type_ascii = bms_data->bms_data[i].type;
                sn_code = bms_data->bms_data[i].sn_code;
            }
#endif

#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE
            if (can_node_rd.Pack[i].online && strnlen(can_node_rd.Pack[i].pack_about.type_ascii, TYPE_SIZE)) {
                online = can_node_rd.Pack[i].online;
                type_ascii = can_node_rd.Pack[i].pack_about.type_ascii;
                sn_code = can_node_rd.Pack[i].pack_about.sn_code;
            }
#endif

            if (online)
            {
                dev_type = SN_TYPE_ASCII_TO_NUM(type_ascii);
                point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_BATTERY;
                point_bind_info[point_cnt].SN_64 = (sn_code > 0) ? sn_code : dev_factory.dev_sn;
#ifdef BATTERY_TYPE_VER_STAT_ENABLE                
                point_bind_info[point_cnt].group_same_type_addr = get_slaveaddr_by_type_ascii(type_ascii);
#else
                point_bind_info[point_cnt].group_same_type_addr = 0;
#endif
                point_bind_info[point_cnt].slave_addr = i + MD_PACK_ADDR_START;
                point_bind_info[point_cnt].state.all = Pack_Point_State_Update(i, point_bind_info[point_cnt].group_same_type_addr);
                point_bind_info[point_cnt].master_addr = MD_SUMMARY_ADDR;
                point_bind_info[point_cnt].group_addr = 0;
                point_cnt++;
            }
        }
    }

    return point_cnt;
}

/**
  * @brief      Dchub类型设备统计
  * @param[in]  POINT_BIND_INFO *point_bind_info
                bool is_all
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t Dchub_Modbus_Device_Report_Process(POINT_BIND_INFO *point_bind_info, bool is_all)
{
    uint8_t point_cnt = 0;

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE    
    if ( reals.online_DCHUB_num > 0 )
    {
        for ( int i = 0 ; i < DC_HUB_MAX_NUM ; i++ )
        {
            if((can_node_rd.DCHUB[i].online)
                && (0 != top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_sn)//非0，有节点内容
                && (0 != strnlen(top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_type, TYPE_SIZE))
                )
            {
                uint16_t dev_type = SN_TYPE_ASCII_TO_NUM(top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_type);
                point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_DC_DISCHARGER;
                point_bind_info[point_cnt].SN_64 = top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_sn;
                point_bind_info[point_cnt].group_same_type_addr = MD_DCHUB_SUM_ADDR;
                point_bind_info[point_cnt].slave_addr = i + MD_DCHUB_ADDR_START;
                point_bind_info[point_cnt].master_addr = MD_SUMMARY_ADDR;
                point_bind_info[point_cnt].group_addr = 0;
                point_bind_info[point_cnt].state.all = DCHUB_Point_State_Update(i);
                point_cnt++;
            }
        }
    }
#endif

    return point_cnt;
}

/**
  * @brief      Dcdc类型设备统计
  * @param[in]  POINT_BIND_INFO *point_bind_info
                bool is_all
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t Dcdc_Modbus_Device_Report_Process(POINT_BIND_INFO *point_bind_info, bool is_all)
{
    uint8_t point_cnt = 0;

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE    
    if ( reals.online_Dcdc_num > 0 )
    {
        for ( int i = 0 ; i < DCDC_MAX_NUM ; i++ )
        {
            if((can_node_rd.Dcdc[i].online)
                && (top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn > 0xFF)
                && (0 != strnlen(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, TYPE_SIZE))
                )
            {
                uint16_t dev_type = SN_TYPE_ASCII_TO_NUM(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type);
                point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_DC_CHARGER;
                point_bind_info[point_cnt].SN_64 = top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn;
                point_bind_info[point_cnt].group_same_type_addr = MD_DCDC_SUM_ADDR;
                point_bind_info[point_cnt].slave_addr = i + MD_DCDC_ADDR_START;
                point_bind_info[point_cnt].master_addr = MD_SUMMARY_ADDR;
                point_bind_info[point_cnt].group_addr = 0;
                point_bind_info[point_cnt].state.all = DCDC_Point_State_Update(i);
                point_cnt++;
            }
        }
    }
#endif

    return point_cnt;
}

/**
  * @brief      已组网设备帧组帧
                按照排序后的变量内容顺序上报
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
uint8_t Modbus_21000_1_Net_Point_Frame(void)
{ 
    uint8_t self_flag = 0;
    uint8_t point_cnt = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;

    /*清空*/
    memset(&Device_info->self_bind_info, 0, sizeof(Device_info->self_bind_info));
    memset(Device_info->point_bind_info, 0, sizeof(Device_info->point_bind_info));

    /*本机*/
    Device_info->self_bind_info.group_same_type_addr = 0; // 预留
    Device_info->self_bind_info.slave_addr = MD_SUMMARY_ADDR;
    Device_info->self_bind_info.group_addr = 0;
    Device_info->self_bind_info.master_addr = 0;
    Device_info->self_bind_info.SN_64 = dev_factory.dev_sn ;
    Device_info->self_bind_info.Dev_Type = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
    Device_info->self_bind_info.state.all = INV_Point_State_Update(INV_MAX_NUM);
    self_flag = 1;

    /*逆变（仅并机有效）*/
    point_cnt += INV_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], false);

    /*PACK*/
    point_cnt += Pack_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], false);

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    /*DCHUB*/
    point_cnt += Dchub_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], false);
#endif

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
    /*DCDC*/
    point_cnt += Dcdc_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], false);
#endif

#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE
    /*插座*/
    point_cnt += plug_info_report_process(&Device_info->point_bind_info[point_cnt], false);
#endif

    /*其他设备暂未实现*/

#ifdef MODBUS_21000_DEBUG_LOG

    ESP_LOGI(Modbus_21000_TAG, "/***************************device list****************************/");
    if ( self_flag == 1 )
    {
        ESP_LOGI(Modbus_21000_TAG, "Type_Group(%d), Group(%d), MasterSlave(%d), SelfSlave(%d), Type(%d), SN(%llu), State(%d)", 
            Device_info->self_bind_info.group_same_type_addr,
            Device_info->self_bind_info.group_addr,
            Device_info->self_bind_info.master_addr,
            Device_info->self_bind_info.slave_addr,
            Device_info->self_bind_info.Dev_Type,
            Device_info->self_bind_info.SN_64,
            Device_info->self_bind_info.state.all); 
    }

    for ( int i = 0 ; i < point_cnt ; i++ )
    {
        ESP_LOGI(Modbus_21000_TAG, "Type_Group(%d), Group(%d), MasterSlave(%d), SelfSlave(%d), Type(%d), SN(%llu), State(%d)", 
            Device_info->point_bind_info[i].group_same_type_addr,
            Device_info->point_bind_info[i].group_addr,
            Device_info->point_bind_info[i].master_addr,
            Device_info->point_bind_info[i].slave_addr,
            Device_info->point_bind_info[i].Dev_Type,
            Device_info->point_bind_info[i].SN_64,
            Device_info->point_bind_info[i].state.all); 
    }
    ESP_LOGI(Modbus_21000_TAG, "/******************************************************************/");
    
#endif

    return (point_cnt + self_flag);
}

/**
  * @brief      IOT类型升级进度
  * @param[in]  OtaDevice* dev_ota_status  
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t IOT_Modbus_21000_3_update(OtaDevice* dev_ota_status)
{
    uint8_t point_cnt = 0;
    uint16_t dev_type = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;

    if ((reals.current_ota_info.soft_version/100) == (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver/100))
    {
        /*本机*/
        if (0 != strnlen(dev_factory.dev_type, TYPE_SIZE))
        {
            dev_type = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
            Device_info->point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_INVERTER;
            Device_info->point_bind_info[point_cnt].SN_64 = dev_factory.dev_sn;
            Device_info->point_bind_info[point_cnt].isOta = reals.current_ota_info.isOta;
            Device_info->point_bind_info[point_cnt].errCode = reals.current_ota_info.errCode;
            Device_info->point_bind_info[point_cnt].soft_version = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver;

            point_cnt++;
        }
    }
    else if ((reals.current_ota_info.soft_version/100) == (top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg11000_IOT_info.software_ver/100))
    {
        for(uint8_t i = 0; i < DCDC_MAX_NUM; i++)
        {
            /*DCDC*/
            if (0 != strnlen(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, TYPE_SIZE))
            {
                dev_type = SN_TYPE_ASCII_TO_NUM(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type);
                Device_info->point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_DC_CHARGER;
                Device_info->point_bind_info[point_cnt].SN_64 = top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn;
                Device_info->point_bind_info[point_cnt].isOta = reals.current_ota_info.isOta;
                Device_info->point_bind_info[point_cnt].errCode = reals.current_ota_info.errCode;
                Device_info->point_bind_info[point_cnt].soft_version = top_modbus_rd.Dcdc[i].mod_reg11000_IOT_info.software_ver;

                point_cnt++;
            }
        }
    }

    return point_cnt;
}

/**
  * @brief      INV类型升级进度
  * @param[in]  OtaDevice* dev_ota_status  
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t INV_Modbus_21000_3_update(OtaDevice* dev_ota_status)
{
    uint8_t point_cnt = 0;
    uint16_t dev_type = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;
    
    /*所有INV*/
    if (reals.online_Inv_num > 0)
    {
        for (int i = 0; i < INV_MAX_NUM; i++)
        {
            if (can_node_rd.Inv[i].online)
            {
                for (int n = 0; n < top_modbus_rd.Inv[i].mod_reg01100_Inv_base.software_total; n++)
                {
                    if ((reals.current_ota_info.soft_version / 100) == (top_modbus_rd.Inv[i].mod_reg01100_Inv_base.soft[n].version / 100))
                    {
                        /*本机*/
                        if (0 != strnlen(dev_factory.dev_type, TYPE_SIZE))
                        {
                            dev_type = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
                            Device_info->point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_INVERTER;
                            Device_info->point_bind_info[point_cnt].SN_64 = dev_factory.dev_sn;
                            Device_info->point_bind_info[point_cnt].isOta = reals.current_ota_info.isOta;
                            Device_info->point_bind_info[point_cnt].errCode = reals.current_ota_info.errCode;
                            Device_info->point_bind_info[point_cnt].soft_version = top_modbus_rd.Inv[i].mod_reg01100_Inv_base.soft[n].version;
                            point_cnt++;
                            break;
                        }
                    }
                }
            }
        }
    }

    return point_cnt;
}

/**
  * @brief      PACK类型升级进度
  * @param[in]  OtaDevice* dev_ota_status
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t PACK_Modbus_21000_3_update(OtaDevice* dev_ota_status)
{
    uint8_t point_cnt = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;

    /*PACK(堆、簇、BMU)*/
    if (reals.online_Pack_num > 0) // online_Pack_num为在线簇的数量
    {
        for ( int i = 0 ; i < PACK_MAX_NUM ; i++ ) 
        {
            uint16_t dev_type = 0;
            uint64_t sn_code = 0;
            uint8_t online = 0;

#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
            bat_data_t *bms_data = get_bat_data_ptr();
            if (bms_data->bms_data[i].valid && strnlen(bms_data->bms_data[i].type, TYPE_SIZE)) {
                online = bms_data->bms_data[i].valid;
                dev_type = SN_TYPE_ASCII_TO_NUM(bms_data->bms_data[i].type);
                sn_code = bms_data->bms_data[i].sn_code;
            }
#endif

#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE
            if (can_node_rd.Pack[i].online && strnlen(can_node_rd.Pack[i].pack_about.type_ascii, TYPE_SIZE)) {
                online = can_node_rd.Pack[i].online;
                dev_type = SN_TYPE_ASCII_TO_NUM(can_node_rd.Pack[i].pack_about.type_ascii);
                sn_code = can_node_rd.Pack[i].pack_about.sn_code;
            }
#endif

#ifdef INTERNAL_PACK_ENABLE
            if ( 0 == i ) {
                online = 1;
                dev_type = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
                sn_code = dev_factory.dev_sn ;
            }
#endif
            /*在线则继续执行*/
            if (online)
            {    
                for ( int j = 0 ; j < top_modbus_rd.Pack[i].mod_reg06100_Pack_each.software_total ; j++ )
                {
                    if ((reals.current_ota_info.soft_version / 100) == (top_modbus_rd.Pack[i].mod_reg06100_Pack_each.soft[j].version / 100))
                    {
                        Device_info->point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_BATTERY;
                        Device_info->point_bind_info[point_cnt].SN_64 = (sn_code > 0) ? sn_code : dev_factory.dev_sn;
                        Device_info->point_bind_info[point_cnt].isOta = reals.current_ota_info.isOta;
                        Device_info->point_bind_info[point_cnt].errCode = reals.current_ota_info.errCode;
                        Device_info->point_bind_info[point_cnt].soft_version = top_modbus_rd.Pack[i].mod_reg06100_Pack_each.soft[j].version;

                        point_cnt++;
                        break;
                    }
                }
            }
        }
    }

    return point_cnt;
}

/**
  * @brief      DCHUB类型升级进度
  * @param[in]  OtaDevice* dev_ota_status  
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t DCHUB_Modbus_21000_3_update(OtaDevice* dev_ota_status)
{
    uint8_t point_cnt = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;
    
    /*所有INV*/
    if (reals.online_DCHUB_num > 0)
    {
        for (int i = 0; i < DC_HUB_MAX_NUM; i++)
        {
            if (can_node_rd.DCHUB[i].online)
            {
                if ((reals.current_ota_info.soft_version / 100) == (top_modbus_rd.DcHub[i].mod_reg01100_Inv_base.soft[0].version / 100))
                {
                    /*本机*/
                    if (0 != strnlen(top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_type, TYPE_SIZE))
                    {
                        uint16_t dev_type = SN_TYPE_ASCII_TO_NUM(top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_type);
                        Device_info->point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_DC_DISCHARGER;
                        Device_info->point_bind_info[point_cnt].SN_64 = top_modbus_rd.DcHub[i].mod_reg15700_Dc_Hub_info.dc_hub_sn;
                        Device_info->point_bind_info[point_cnt].isOta = reals.current_ota_info.isOta;
                        Device_info->point_bind_info[point_cnt].errCode = reals.current_ota_info.errCode;
                        Device_info->point_bind_info[point_cnt].soft_version = top_modbus_rd.DcHub[i].mod_reg01100_Inv_base.soft[0].version;
                        point_cnt++;
                        break;
                    }
                }
            }
        }
    }

    return point_cnt;
}

/**
  * @brief      DCDC类型升级进度
  * @param[in]  OtaDevice* dev_ota_status  
  * @param[out] None
  * @return     uint8_t
  */
static uint8_t DCDC_Modbus_21000_3_update(OtaDevice* dev_ota_status)
{
    uint8_t point_cnt = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;
    
    /*所有INV*/
    if (reals.online_Dcdc_num > 0)
    {
        for (int i = 0; i < DCDC_MAX_NUM; i++)
        {
            if (can_node_rd.Dcdc[i].online)
            {
                if ((reals.current_ota_info.soft_version / 100) == (top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareVersion / 100))
                {
                    /*本机*/
                    if (0 != strnlen(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type, TYPE_SIZE))
                    {
                        uint16_t dev_type = SN_TYPE_ASCII_TO_NUM(top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_type);
                        Device_info->point_bind_info[point_cnt].Dev_Type = (dev_type != 0) ? dev_type : SN_TYPE_COMMON_DC_CHARGER;
                        Device_info->point_bind_info[point_cnt].SN_64 = top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dev_sn;
                        Device_info->point_bind_info[point_cnt].isOta = reals.current_ota_info.isOta;
                        Device_info->point_bind_info[point_cnt].errCode = reals.current_ota_info.errCode;
                        Device_info->point_bind_info[point_cnt].soft_version = top_modbus_rd.Dcdc[i].mod_reg15500_DCDC_info.dcdc_SoftwareVersion;
                        point_cnt++;
                        break;
                    }
                }
            }
        }
    }

    return point_cnt;
}

/**
  * @brief      借用绑定帧上报升级进度
  * @param[in]  void  
  * @param[out] None
  * @return     void Mo
  */
uint8_t Modbus_21000_3_Net_Point_Frame(void)
{ 
    uint8_t point_cnt = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;

    /*清空*/
    memset(&Device_info->self_bind_info, 0, sizeof(Device_info->self_bind_info));
    memset(Device_info->point_bind_info, 0, sizeof(Device_info->point_bind_info));

    /*
    该结构体为CAN总线0x0878帧获取到的各ID升级进度
    目前暂不存在部分升级的情况，故仅汇总上报该类型本次升级状态，暂不使用dev_ota_status
    */
    // OtaDevice* dev_ota_status = &get_dev_ota_announce_status_address();
    OtaDevice* dev_ota_status = NULL;

    if ( reals.current_ota_info.isOta != 0 )    
    {
        switch ( reals.current_ota_info.type )
        {
            case DEVICE_IOT :
                point_cnt = IOT_Modbus_21000_3_update(dev_ota_status);
                break;
                
            case DEVICE_ARM :
                point_cnt = INV_Modbus_21000_3_update(dev_ota_status);
                break;
            
            case DEVICE_DSP :
                point_cnt += INV_Modbus_21000_3_update(dev_ota_status);
                point_cnt += DCDC_Modbus_21000_3_update(dev_ota_status);
                break;
            
            case DEVICE_DC_HUB :
                point_cnt = DCHUB_Modbus_21000_3_update(dev_ota_status);
                break;
            
            case DEVICE_PACK_M1:
                point_cnt += INV_Modbus_21000_3_update(dev_ota_status);
                point_cnt += PACK_Modbus_21000_3_update(dev_ota_status);
                break;
                
            case DEVICE_BMS :
            case DEVICE_PACK_BCU :
            case DEVICE_PACK_BMU :
            case DEVICE_PACK_BMS :
            case DEVICE_PACK_SAFTY :
            case DEVICE_SYNLINK :
                point_cnt = PACK_Modbus_21000_3_update(dev_ota_status);
                break;
           
            default:
                break;
        }
    }
    
#ifdef MODBUS_21000_DEBUG_LOG
    
        ESP_LOGW(Modbus_21000_TAG, "/***************************OTA INFO****************************/");

        for ( int i = 0 ; i < point_cnt ; i++ )
        {
            ESP_LOGW(Modbus_21000_TAG, "Dev_Type(%d), SN(%llu), isOta(%d), errCode(%d), soft_version(%lu)", 
                Device_info->point_bind_info[i].Dev_Type,
                Device_info->point_bind_info[i].SN_64,
                Device_info->point_bind_info[i].isOta,
                Device_info->point_bind_info[i].errCode,
                Device_info->point_bind_info[i].soft_version); 
        }
        
        ESP_LOGW(Modbus_21000_TAG, "/******************************************************************/");
        
#endif

    return point_cnt;
}

/**
 * @brief 上报所有设备，包括已绑定和未绑定的
 * @note 已绑定且被其它系统绑定的设备未不可再次绑定设备, 需设置群组序号为0xff
 */
uint8_t Modbus_21000_5_Net_Point_Frame(void)
{
    uint8_t self_flag = 0;
    uint8_t point_cnt = 0;
	MOD_STRUCT_reg21000	*Device_info = &top_modbus_rd.Inv[reals.Addr_can_self].mod_reg21000_data;

    /*清空*/
    memset(&Device_info->self_bind_info, 0, sizeof(Device_info->self_bind_info));
    memset(Device_info->point_bind_info, 0, sizeof(Device_info->point_bind_info));

    /*本机*/
    Device_info->self_bind_info.group_same_type_addr = 0; // 预留
    Device_info->self_bind_info.slave_addr = MD_SUMMARY_ADDR;
    Device_info->self_bind_info.group_addr = 0;
    Device_info->self_bind_info.master_addr = 0;
    Device_info->self_bind_info.SN_64 = dev_factory.dev_sn ;
    Device_info->self_bind_info.Dev_Type = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
    Device_info->self_bind_info.state.all = INV_Point_State_Update(INV_MAX_NUM);
    self_flag = 1;

    /*逆变（仅并机有效）*/
    point_cnt += INV_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], true);

    /*PACK*/
    point_cnt += Pack_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], true);

#ifdef CONFIG_CAN_DCHUB_SUPPLY_ENABLE
    /*DCHUB*/
    point_cnt += Dchub_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], true);
#endif

#ifdef CONFIG_CAN_DCDC_SUPPLY_ENABLE
    /*DCDC*/
    point_cnt += Dcdc_Modbus_Device_Report_Process(&Device_info->point_bind_info[point_cnt], true);
#endif

#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE
    /*插座*/
    point_cnt += plug_info_report_process(&Device_info->point_bind_info[point_cnt], true);
#endif

    /*其他设备暂未实现*/

#ifdef MODBUS_21000_DEBUG_LOG

    ESP_LOGI(Modbus_21000_TAG, "/***************************device list****************************/");
    if ( self_flag == 1 )
    {
        ESP_LOGI(Modbus_21000_TAG, "Group(%d), MasterSlave(%d), SelfSlave(%d), Type(%d), SN(%llu), State(%d)", 
            Device_info->self_bind_info.group_same_type_addr,
            Device_info->self_bind_info.master_addr,
            Device_info->self_bind_info.slave_addr,
            Device_info->self_bind_info.Dev_Type,
            Device_info->self_bind_info.SN_64,
            Device_info->self_bind_info.state.all); 
    }

    for ( int i = 0 ; i < point_cnt ; i++ )
    {
        ESP_LOGI(Modbus_21000_TAG, "Group(%d), MasterSlave(%d), SelfSlave(%d), Type(%d), SN(%llu), State(%d)", 
            Device_info->point_bind_info[i].group_same_type_addr,
            Device_info->point_bind_info[i].master_addr,
            Device_info->point_bind_info[i].slave_addr,
            Device_info->point_bind_info[i].Dev_Type,
            Device_info->point_bind_info[i].SN_64,
            Device_info->point_bind_info[i].state.all); 
    }
    ESP_LOGI(Modbus_21000_TAG, "/******************************************************************/");
    
#endif

    return (point_cnt + self_flag);
}

/**
 * @brief 创建Modbus 21000绑定的消息队列
 */
int16_t Modbus_21000_bind_msg_create(void)
{
    if (modbus_21000_bind_msg_queue != NULL) {
        ESP_LOGW(Modbus_21000_TAG, "modbus_21000_bind_msg_queue already created");
        return 1;
    }

    modbus_21000_bind_msg_queue = xQueueCreate(5, sizeof(bind_msg_t));
    
    if (modbus_21000_bind_msg_queue == NULL) {
        ESP_LOGE(Modbus_21000_TAG, "modbus_21000_bind_msg_queue create failed");
        return -1;
    }

    ESP_LOGD(Modbus_21000_TAG, "modbus_21000_bind_msg_queue created successfully");
    return 1;
}

/**
 * @brief 向Modbus 21000绑定的消息队列中推送消息
 * @param[in] bind_num 绑定的消息数量
 * @param[in] bind_info 绑定信息的指针
 * @note 非阻塞运行，如果消息队列已满则返回错误
 */
int16_t Modbus_21000_bind_msg_push(uint8_t bind_num, const POINT_BIND_INFO_WR *bind_info)
{
    if (modbus_21000_bind_msg_queue == NULL) {
        ESP_LOGE(Modbus_21000_TAG, "modbus_21000_bind_msg_queue not created");
        return -1;
    }

    if (bind_info == NULL || bind_num == 0) {
        ESP_LOGE(Modbus_21000_TAG, "Invalid parameters: bind_info=%p, bind_num=%d", bind_info, bind_num);
        return -1;
    }

    bind_msg_t queue_msg;

    queue_msg.data = (POINT_BIND_INFO_WR *)iot_calloc((sizeof(POINT_BIND_INFO_WR) * bind_num));

    if (!queue_msg.data)
    {
        ESP_LOGE(Modbus_21000_TAG, "Modbus_21000_bind_msg_push calloc failed");
        return -1;
    }
    else
    {
        memcpy(queue_msg.data, bind_info, sizeof(POINT_BIND_INFO_WR) * bind_num);
        queue_msg.bind_num = bind_num;

        /*消息保存到队列*/
        if (xQueueSendToBack(modbus_21000_bind_msg_queue, &queue_msg, 0) != pdPASS)
        {
            free(queue_msg.data);
            queue_msg.data = NULL;
            ESP_LOGE(Modbus_21000_TAG, "Modbus_21000_bind_msg_push xQueueSendToBack failed");
            return -2;
        }
    }

    ESP_LOGI(Modbus_21000_TAG, "Successfully pushed %d messages to queue", bind_num);
    return bind_num;
}

/**
 * @brief 从Modbus 21000绑定的消息队列中弹出消息
 * @return 绑定设备数量
 * @note 非阻塞运行
 */
int16_t Modbus_21000_bind_msg_process(void)
{
    bind_msg_t queue_msg = {0, NULL};
    uint16_t size = NET_WIFI_S1_POINT + NET_WIFI_METER_POINT;
    
    if (modbus_21000_bind_msg_queue
        && xQueueReceive(modbus_21000_bind_msg_queue, &queue_msg, 0) == pdTRUE) // 非阻塞
    {
        ESP_LOGD(Modbus_21000_TAG, "Received message from queue: bind_num=%d", queue_msg.bind_num);
        if (queue_msg.data)
        {
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, queue_msg.data, sizeof(POINT_BIND_INFO_WR) * queue_msg.bind_num, ESP_LOG_DEBUG);
            
            uint16_t size_needed = queue_msg.bind_num > size ? size : queue_msg.bind_num;
            
            for (int i = 0; i < size_needed; i++)
            {
                POINT_BIND_INFO_WR *bind_info = queue_msg.data + i;
                ESP_LOGI(Modbus_21000_TAG, "Processing bind message %d: Dev_Type=%u, SN_64=%llu",
                        i, bind_info->Dev_Type, bind_info->SN_64);
                
                switch (bind_info->Dev_Type)
                {
                    case SN_TYPE_S1:
#ifdef CONFIG_WLCC_PLUG_SUPPLY_ENABLE                        
                        plug_bind_process(bind_info);
#endif
                        break;
            
                    case SN_TYPE_METER:
#ifdef CONFIG_WLCC_METER_SUPPLY_ENABLE                        
                        meter_bluetti_bind_process(bind_info);
#endif
                        break;
            
                    default:
                        ESP_LOGW(TAG, "Unknown device type: %u", bind_info->Dev_Type);
                        break;
                }
            }
            
            free(queue_msg.data);
            queue_msg.data = NULL;

            return size_needed;
        }
    }

    return 0;
}

