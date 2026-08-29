#include "dev_modbus_s1_data_handler.h"
#include "wlcc_process.h"
#include "wlcc_interface.h"
#include "wlcc_protocol.h"
#include "wlcc_crypt.h"
#include "wlcc_common.h"
#include "dev_modbus_manage.h"
#include "modbus_protocol.h"
#include "modbus_data.h"
#include "modbus_slave.h"
#include "modbus_master.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "modbus_define.h"
#include "modbus_protocol.h"
#include "utils.h"
#include "dev_access_log.h"

#define TAG "[MD_S1]"


#define PLUG_SCAN_MAX_NUM      (NET_WIFI_S1_POINT * 2) //插座扫描最大数量

static uint8_t smart_plug_online = 0;
static uint16_t bluetti_plug_online_count = 0;
USE_EXT_RAM_BSS static POINT_STATE bluetti_plug_status[PLUG_SCAN_MAX_NUM] = {0};
USE_EXT_RAM_BSS static wlcc_dev_t bluetti_plug[PLUG_SCAN_MAX_NUM] = {0}; 
USE_EXT_RAM_BSS static uint8_t PlugSendBuff[270] = {0};

uint8_t dev_smart_plug_online(void)
{
	return smart_plug_online;
}

void dev_modbus_s1_clean(void)
{
	memset(&g_other_rd.Plug, 0, sizeof(g_other_rd.Plug));
}

void dev_modbus_s1_status_notify(uint64_t dev_sn)
{
    for (int i = 0; i < NET_WIFI_S1_POINT; i++)
    {
        if (IotSetData.dev_info_t.plug_cfg[i].dev_type == SN_TYPE_S1
            && IotSetData.dev_info_t.plug_cfg[i].dev_sn == dev_sn)
        {
            reals.net_point_Comein = 1; // 连接成功，触发一次net_point_Comein事件，通知APP刷新界面
        }
    }
}

void dev_modbus_s1_del(uint64_t dev_sn)
{
	int i = 0;
	
	for (i = 0; i < PLUG_MAX_NUM; i++)
	{
        ESP_LOGI(TAG,"dev_modbus_s1_del i:%d dev_sn:%llu smart_sn:%llu",i,dev_sn,g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
		if(dev_sn == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN) {
            dev_modbus_s1_status_notify(dev_sn);
            memset(&g_other_rd.Plug[i], 0, sizeof(g_other_rd.Plug[i]));
			break;
		}
	}
}

void dev_modbus_s1_update(uint64_t dev_sn, uint32_t online_time)
{
	int i = 0;
	int empty_index = PLUG_MAX_NUM;
	int offline_index = PLUG_MAX_NUM;
	uint32_t last_online_time;
	uint32_t last_online_time_tmp;
	
	for (i = 0; i < PLUG_MAX_NUM; i++)
	{
		if(0 == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN)
		{
		    // 记录最近的空位置
			if(PLUG_MAX_NUM == empty_index){
				empty_index = i;
			}
		}
		else
		{
			last_online_time_tmp = wlcc_dev_online_time_get(SN_TYPE_S1, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
			if(0 == wlcc_dev_online_check(SN_TYPE_S1, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN))
			{
			    // 记录最近的已离线位置
				if(PLUG_MAX_NUM == offline_index)
				{
					offline_index = i;
					last_online_time = last_online_time_tmp;
				}
				else
				{
					if(last_online_time_tmp < last_online_time)
					{
						offline_index = i;
						last_online_time = last_online_time_tmp;
					}
				}
			}
		}

        // 找到对应设备，直接返回
		if(dev_sn == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN){
			return;
		}
	}

	if(empty_index < PLUG_MAX_NUM){
		g_other_rd.Plug[empty_index].mod_reg14500_SmartPlug_info.SmartPlug_SN = dev_sn;
        dev_modbus_s1_status_notify(dev_sn);
	}else  if(offline_index < PLUG_MAX_NUM){
		g_other_rd.Plug[offline_index].mod_reg14500_SmartPlug_info.SmartPlug_SN = dev_sn;
        dev_modbus_s1_status_notify(dev_sn);
	}
}


int dev_modbus_s1_slave_addr_get(uint16_t dev_type, uint64_t dev_sn)
{
	int i = 0;
	int modbus_slave_addr = IOT_FAIL;
	
	ESP_LOGI(TAG, "income SN:%llu", dev_sn);

	for (i = 0; i < PLUG_MAX_NUM; i++)
    {
        ESP_LOGI(TAG, "modbus[%d] SN:%llu", i, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
		if((0 != dev_sn) && (dev_sn == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN))
        {
			modbus_slave_addr = i+1;
			break;
		}
    }
	
	ESP_LOGI(TAG, "modbus_slave_addr:%d", modbus_slave_addr);

	return modbus_slave_addr;
}


int dev_modbus_plug_data_read_handle(uint16_t dev_type, uint64_t dev_sn)
{
	static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	modbus_addr_info_t src_addr;

	if((now_time - pre_time) >= 5000) 
	{
		pre_time = now_time;
        uint8_t *modbusBuff = PlugSendBuff;
        uint16_t real_regNum = ((uint16_t)offsetof(MOD_STRUCT_reg14500, revd_end) >> 1);
		uint16_t tx_len = Modbus_MasterReadCmd_03H(MOD_REG_START_ADDR_14500, real_regNum, modbusBuff, 1, MD_CHL_WIFI_WLCC);
		if(tx_len > 0)
		{
			ESP_LOGD(TAG, "plug_data_read len=%d!", tx_len);
			src_addr.channel = MD_CHL_SELF;
			src_addr.dev_type = SN_TYPE_SELF;
			src_addr.dev_sn = dev_factory.dev_sn;
			src_addr.regAddr = MOD_REG_START_ADDR_14500;
			src_addr.regNum = real_regNum;
			wlcc_modbus_msg_send(modbusBuff, tx_len, dev_type, dev_sn, src_addr);
		}
	}

	return 0;
}

int dev_modbus_plug_set_read_handle(uint16_t dev_type, uint64_t dev_sn, bool fast_flag)
{
	static uint32_t pre_time = 0;
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	modbus_addr_info_t src_addr;

	if(fast_flag || ((now_time - pre_time) >= 5000)) 
	{
		pre_time = now_time;
        uint8_t *modbusBuff = PlugSendBuff;
        uint16_t real_regNum = ((uint16_t)offsetof(MOD_STRUCT_reg14700, revd_end) >> 1);
		uint16_t tx_len = Modbus_MasterReadCmd_03H(MOD_REG_START_ADDR_14700, real_regNum, modbusBuff, 1, MD_CHL_WIFI_WLCC);
		if(tx_len > 0)
		{
			ESP_LOGD(TAG, "plug_set_read len=%d!", tx_len);
			src_addr.channel = MD_CHL_SELF;
			src_addr.dev_type = SN_TYPE_SELF;
			src_addr.dev_sn = dev_factory.dev_sn;
			src_addr.regAddr = MOD_REG_START_ADDR_14700;
			src_addr.regNum = real_regNum;
			wlcc_modbus_msg_send(modbusBuff, tx_len, dev_type, dev_sn, src_addr);
		}
	}

	return 0;
}

md_read_t dev_modbus_s1_polling_read_rtn_handle(uint16_t dev_type, uint64_t dev_sn, uint8_t step)
{
	md_read_t modbus_read_ret;
	int modbus_slave_addr = IOT_FAIL;

	modbus_read_ret.reg_num = 0;

	if(0 == step)
	{
		modbus_read_ret.reg_addr = MOD_REG_START_ADDR_14500;
		modbus_slave_addr = dev_modbus_s1_slave_addr_get(dev_type, dev_sn);
		if(IOT_FAIL == modbus_slave_addr){
			modbus_read_ret.reg_num = 0;
		}else{
			modbus_read_ret.reg_num = MOD_REG_LEN_14500;
		}
	}else{
		modbus_read_ret.reg_num = 0;
	}
	
	return modbus_read_ret;
}

uint16_t vLookupS1DataMaxLength(uint16_t regStartAddr)
{
	uint16_t regMaxLen = 0;

	if ((regStartAddr >= MOD_REG_START_ADDR_14500) && (regStartAddr <= (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500))){
		regMaxLen = MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500 - regStartAddr;
	}
    else if ((regStartAddr >= MOD_REG_START_ADDR_14700) && (regStartAddr <= (MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700))){
        regMaxLen = MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700 - regStartAddr;
    }

	return regMaxLen;
}

const uint16_t* vLookupS1DataTab(uint16_t dev_type, uint64_t dev_sn, uint16_t iReadAddr, uint16_t iReadNum, bool is_write)
{
	uint16_t start = 0;
	uint16_t *reg_ptr = NULL;

	int slave_addr = dev_modbus_s1_slave_addr_get(dev_type, dev_sn);

	if(slave_addr < 0){
		return NULL;
	}

	if ((iReadAddr >= MOD_REG_START_ADDR_14500) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500)))
	{
		start = MOD_REG_START_ADDR_14500;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&g_other_wr.Plug.mod_reg14500_SmartPlug_info;
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg14500_SmartPlug_info read");
			if(slave_addr < PLUG_MAX_NUM){
				reg_ptr = (const uint16_t*)&g_other_rd.Plug[slave_addr-1].mod_reg14500_SmartPlug_info;
			}
		}
	}
    else if ((iReadAddr >= MOD_REG_START_ADDR_14700) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_14700 + MOD_REG_LEN_14700)))
	{
		start = MOD_REG_START_ADDR_14700;	  /* 可读 */

		if (true == is_write) //write
		{
			reg_ptr = (const uint16_t*)&g_other_wr.Plug.mod_reg14700_SmartPlug_set;
		} 
		else //read
		{
			ESP_LOGI(TAG,"get in mod_reg14700_SmartPlug_set read");
			if(slave_addr < PLUG_MAX_NUM){
				reg_ptr = (const uint16_t*)&g_other_rd.Plug[slave_addr-1].mod_reg14700_SmartPlug_set;
			}
		}
	}

	if (reg_ptr) {
		return (reg_ptr + (iReadAddr - start));
	}
	else{
		return NULL;
	}
}

int dev_modbus_plug_data_get(uint16_t dev_type, uint64_t dev_sn, 
											uint8_t slaveAddr, uint8_t funcode, 
											uint16_t regAdderss, uint16_t gRegCnt, 
											uint8_t *response)
{
	const uint16_t *p_tab = NULL;
	uint16_t *dst = NULL;
	uint16_t j = 0;

	response[j++] = slaveAddr;
	response[j++] = funcode;
	response[j++] = (gRegCnt << 1); // 读取的字节长度
	
	p_tab = vLookupS1DataTab(dev_type, dev_sn, regAdderss, gRegCnt, false);
	/*填充数据*/
    if(NULL != p_tab) 
    {
        dst = (uint16_t *)&response[3];
        for (uint16_t i = 0; i < gRegCnt; i++, j += 2)
        {
            /* table2中的数据 */
            dst[i] = LSB2MSB(p_tab[i]);
        }  
    }   
    else
    {
        return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
    }   

	uint16_t crc16 = ModbusCrc16(response, j);
	response[j++] = crc16;
	response[j++] = crc16 >> 8;
    //esp_log_buffer_hex(TAG, response, j);
    return j;
}


int dev_modbus_s1_data_rsp_handle(uint16_t dev_type, uint64_t dev_sn, uint16_t regAdderss, uint16_t gRegCnt, uint8_t *income, int len)
{
	uint8_t bytesCounter = 0;
	uint8_t i = 0;
    uint16_t *regPtr = NULL;

	if(0x03 == income[1])
	{
		regPtr = vLookupS1DataTab(0, dev_sn, regAdderss, gRegCnt, false);
	    if ((NULL != regPtr) && ((gRegCnt << 1) == income[2]))
	    {
	        ESP_LOGI(TAG, "[s1 modbus rsp] regAdderss2 ok");

	        bytesCounter = gRegCnt << 1;
	        for (i = 0; i < bytesCounter; i += 2){
	            regPtr[i / 2] = ((uint16_t)income[3 + i] << 8) | income[4 + i]; // H/L
	        }
	    }
	    else
	    {
	        ESP_LOGE(TAG, "[s1 modbus rsp] regAdderss2 or gRegCnt error");
	        return 1;
	    }
	}

	return 0;
}

void dev_modbus_s1_data_summary(void)
{
	static uint16_t time_100ms_cnt = 0;
	int i = 0;
	uint8_t plug_online_count = 0;

	time_100ms_cnt++;

	memset(&g_other_rd.Plug[PLUG_MAX_NUM], 0, sizeof(MOD_STRUCT_PLUG));
	
	for (i = 0; i < PLUG_MAX_NUM; i++)
	{
		if(g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN)
		{
			if(wlcc_dev_detect_stable_conn_check(SN_TYPE_S1, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN))
			{
				g_other_rd.Plug[PLUG_MAX_NUM].mod_reg14500_SmartPlug_info.SmartPlug_Power += g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power;

				if(0 == (time_100ms_cnt%30))
				{
					ESP_LOGI(TAG, "Voltage = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Voltage);
					ESP_LOGI(TAG, "Current = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Current);
					ESP_LOGI(TAG, "Freq = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Frequency);
					ESP_LOGI(TAG, "States = %d", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_States);
					ESP_LOGI(TAG, "Power = %1f", g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_Power/10.0);
				}
				plug_online_count++;
			}
		}
	}
	if(plug_online_count)
		smart_plug_online=1;
	else
		smart_plug_online=0;
}


/*********************** 插座绑定相关 **************************/
int dev_modbus_plug_slave_addr_get(uint16_t dev_type, uint64_t dev_sn)
{
	int i = 0;
	int modbus_slave_addr = IOT_FAIL;
	
	ESP_LOGI(TAG, "income SN:%llu", dev_sn);

	for (i = 0; i < PLUG_MAX_NUM; i++)
    {
        // ESP_LOGI(TAG, "modbus[%d] SN:%llu", i, g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN);
		if((0 != dev_sn) && (dev_sn == g_other_rd.Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN))
        {
			modbus_slave_addr = i;
			break;
		}
    }

	if(IOT_FAIL != modbus_slave_addr)
	{
        modbus_slave_addr += 1;
		ESP_LOGI(TAG, "slave_addr_index:%d", modbus_slave_addr);
	}
	
	ESP_LOGI(TAG, "modbus_slave_addr:%d", modbus_slave_addr);

	return modbus_slave_addr;
}

int16_t plug_is_exist(uint64_t dev_sn)
{
    if (dev_sn == 0)
    {
        ESP_LOGE(TAG, "Plug exist parameter error");
        return -2;
    }

    for (int i = 0; i < NET_WIFI_S1_POINT; i++)
    {
        // ESP_LOGI(TAG, "IotSetData plug[%d] sn:%llu", i, IotSetData.dev_info_t.plug_cfg[i].dev_sn);
        if(IotSetData.dev_info_t.plug_cfg[i].dev_sn == dev_sn)
        {
            return i;
        }
    }

    return -1;
}

/**
 * @brief 判断插座是否在线
 */
uint16_t plug_is_online(uint64_t dev_sn)
{
    if (dev_sn == 0)
    {
        ESP_LOGE(TAG, "Plug online check parameter error");
        return 0;
    }

    return wlcc_dev_online_check(SN_TYPE_S1, dev_sn);
}

uint8_t plug_info_status_check(void)
{
    uint8_t flag = 0;
    for (int i = 0; i < bluetti_plug_online_count; i++)
    {   
        // 检查是否绑定
        int16_t exist_index = plug_is_exist(bluetti_plug[i].dev_sn);
        if (exist_index < 0) {
            continue;
        }
        
        POINT_STATE state = bluetti_plug_status[i];

        // 设备在线检查
        state.bit.point_online = wlcc_dev_online_check(bluetti_plug[i].dev_type, bluetti_plug[i].dev_sn);

        // 设备故障状态检查
        uint16_t data_slave_addr = dev_modbus_plug_slave_addr_get(bluetti_plug[i].dev_type, bluetti_plug[i].dev_sn);
        if (data_slave_addr <= 0 
            || data_slave_addr > PLUG_MAX_NUM ) {
            state.bit.alarm = 0;
            state.bit.protect = 0;
        } else {
            state.bit.alarm = g_other_rd.Plug[data_slave_addr - 1].mod_reg14500_SmartPlug_info.WarnInformation ? 1 : 0;
            state.bit.protect = g_other_rd.Plug[data_slave_addr - 1].mod_reg14500_SmartPlug_info.FaultInformation ? 1 : 0;
        }

        if ( state.all != bluetti_plug_status[i].all ) {
            bluetti_plug_status[i].all = state.all;
            flag |= 1;
        }
    }

    return flag;
}

int16_t plug_info_report_process(POINT_BIND_INFO *point_bind_info, bool is_all)
{
    if (!point_bind_info) {
        ESP_LOGE(TAG, "Plug info report parameter error");
        return -1;
    }

    uint8_t point_cnt = 0;

    memset(bluetti_plug, 0, sizeof(bluetti_plug));

    // 获取在线插座数量
    bluetti_plug_online_count = wlcc_online_dev_info_get(SN_TYPE_S1, bluetti_plug, PLUG_SCAN_MAX_NUM);
    ESP_LOGI(TAG,"bluetti_plug_scan_result_get online count:%d", bluetti_plug_online_count);

    // 上报当前所有在线电表
    for (int i = 0; i < bluetti_plug_online_count; i++)
    {   
        // 检查是否绑定
        int16_t exist_index = plug_is_exist(bluetti_plug[i].dev_sn);
        ESP_LOGI(TAG,"plug exist_index:%d is_all:%d",exist_index,is_all);
        // If not bound and not reporting all devices, skip
        if (exist_index < 0 && !is_all) {
            continue;
        }

        if (exist_index >= 0) {
            point_bind_info[point_cnt].group_addr = 1;
        } else {
            point_bind_info[point_cnt].group_addr = 0;
        }

        // get modbus slave addr
        int slave_addr = dev_modbus_get_addr_from_type_sn(bluetti_plug[i].dev_type, bluetti_plug[i].dev_sn);
        if (slave_addr > 0) {
            point_bind_info[point_cnt].slave_addr = slave_addr;
        } else {
            point_bind_info[point_cnt].slave_addr = 0;
        }

        point_bind_info[point_cnt].state.bit.point_online = bluetti_plug[i].online_status == WLCC_STATUS_OFFLINE ? 0 : 1;
        point_bind_info[point_cnt].group_same_type_addr = 0;
        
        uint16_t data_slave_addr = dev_modbus_plug_slave_addr_get(bluetti_plug[i].dev_type, bluetti_plug[i].dev_sn);
        if (data_slave_addr <= 0 
            || data_slave_addr > PLUG_MAX_NUM ) {
            point_bind_info[point_cnt].state.bit.alarm = 0;
            point_bind_info[point_cnt].state.bit.protect = 0;
        } else {
            point_bind_info[point_cnt].state.bit.alarm = g_other_rd.Plug[data_slave_addr - 1].mod_reg14500_SmartPlug_info.WarnInformation ? 1 : 0;
            point_bind_info[point_cnt].state.bit.protect = g_other_rd.Plug[data_slave_addr - 1].mod_reg14500_SmartPlug_info.FaultInformation ? 1 : 0;
        }
        
        point_bind_info[point_cnt].SN_64 = bluetti_plug[i].dev_sn;
        point_bind_info[point_cnt].Dev_Type = bluetti_plug[i].dev_type;

        ESP_LOGI(TAG, "bluetti plug %d: %llu, slave_addr: %d, online: %d, group_addr: %d",
                bluetti_plug[i].dev_type, point_bind_info[point_cnt].SN_64,
                point_bind_info[point_cnt].slave_addr,
                point_bind_info[point_cnt].state.bit.point_online,
                point_bind_info[point_cnt].group_addr);

        bluetti_plug_status[i].all = point_bind_info[point_cnt].state.all;
        point_cnt++;
    }

    // 处理已绑定但未在线设备
    for (int i = 0; i < NET_WIFI_S1_POINT; i++)
    {
        if (0 == IotSetData.dev_info_t.plug_cfg[i].dev_sn) {
            continue;
        }

        /** If device information can be obtained from Modbus management, the device is considered online.
         * Here, only bound but offline devices are retrieved; online devices will be populated later. */
        int16_t slave_addr = dev_modbus_get_addr_from_type_sn(IotSetData.dev_info_t.plug_cfg[i].dev_type,
                                                            IotSetData.dev_info_t.plug_cfg[i].dev_sn);
        if (slave_addr > 0) {
            continue;
        } else {
            point_bind_info[point_cnt].slave_addr = 0;
        }
        
        point_bind_info[point_cnt].state.bit.point_online = 0;

        point_bind_info[point_cnt].group_same_type_addr = 0;
        point_bind_info[point_cnt].group_addr = 1;

        point_bind_info[point_cnt].SN_64 = IotSetData.dev_info_t.plug_cfg[i].dev_sn;
        point_bind_info[point_cnt].Dev_Type = IotSetData.dev_info_t.plug_cfg[i].dev_type;

        ESP_LOGI(TAG, "21000 report plug info, dev sn:%llu, online:%d, slave addr:%d",
        IotSetData.dev_info_t.plug_cfg[i].dev_sn, point_bind_info[point_cnt].state.bit.point_online, point_bind_info[point_cnt].slave_addr);
        point_cnt++;
    }

    return point_cnt;
}

/**
 * @brief 绑定插座, 把插座信息写入到本地存储, 并开始连接
 * @param dev_info 待绑定设备信息
 */
static int16_t plug_bind(const POINT_BIND_INFO_WR *bind_cmd)
{
    if (!bind_cmd) {
        ESP_LOGE(TAG, "Plug bind parameter error");
        return -1;
    }

    if(bind_cmd->group_addr != 0 && 0 <= plug_is_exist(bind_cmd->SN_64)) {
        ESP_LOGE(TAG, "Plug already binded");
        return -2;
    }

    char dev_type[TYPE_SIZE] = {0};
    SN_TYPE_NUM_TO_ASCII(bind_cmd->Dev_Type, dev_type, TYPE_SIZE);

    /* 查找空闲位置绑定，或查找dev_id想同的设备进行解绑 */
    for (int i = 0; i < NET_WIFI_S1_POINT; i++)
    {
        if (bind_cmd->group_addr == 0)   // 解绑
        {
            if (IotSetData.dev_info_t.plug_cfg[i].dev_sn != bind_cmd->SN_64)
            {
                continue;
            }

#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
            // 更新记录参数
            dev_access_params_t record = {0};
            record.record_type = DEVICE_EVENT_REC_TYPE_DEV_FROM_WIFI_UDP;
            record.parent_address = 0xFF; // 无效
            record.local_address = 0xFF;
            record.operation_attribute = DEVICE_EVENT_OP_FORCE_UNBIND;
            record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
            record.identifier_info.sn_info.dev_type = dev_type;
            record.identifier_info.sn_info.dev_sn = &IotSetData.dev_info_t.plug_cfg[i].dev_sn;

            // 生成记录到队列
            Dev_Access_Log_Generate(&record);
#endif   

            IotSetData.dev_info_t.plug_cfg[i].dev_sn = 0;
            IotSetData.dev_info_t.plug_cfg[i].dev_type = 0;
            reals.SetDataWrFlag.sBit.plug_cfg = 1;
            
            return 1;
        }
        else
        {
            // 查找空闲位置
            if (IotSetData.dev_info_t.plug_cfg[i].dev_sn == 0)
            {
                IotSetData.dev_info_t.plug_cfg[i].dev_sn = bind_cmd->SN_64;
                IotSetData.dev_info_t.plug_cfg[i].dev_type = bind_cmd->Dev_Type;
                ESP_LOGI(TAG,"bind sn:%llu, to index:%d", bind_cmd->SN_64, i);
                reals.SetDataWrFlag.sBit.plug_cfg = 1;

#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE           
                // 更新记录参数
                dev_access_params_t record = {0};
                record.record_type = DEVICE_EVENT_REC_TYPE_DEV_FROM_WIFI_UDP;
                record.parent_address = 0xFF; // 无效
                record.local_address = 0xFF;
                record.operation_attribute = DEVICE_EVENT_OP_FORCE_BIND;
                record.info_type = DEVICE_EVENT_INFO_TYPE_SN;
                record.identifier_info.sn_info.dev_type = dev_type;
                record.identifier_info.sn_info.dev_sn = &IotSetData.dev_info_t.plug_cfg[i].dev_sn;
    
                // 生成记录到队列
                Dev_Access_Log_Generate(&record);
#endif  

                return 2;
            }
        }
    }

    // 未找到空闲位置进行绑定，返回错误码为-4；如果是解绑操作但未找到对应设备进行解绑，返回错误码为-3
    return (bind_cmd->group_addr == 0) ? (-3) : (-4);
}

#if 0
uint16_t Modbus_MasterWriteCmd_06H_10H(uint16_t regAddress, uint8_t regNum, bool is_write, uint8_t *outbuf, uint8_t slave_address,channel_modbus chl)
{
    uint16_t crc;
    uint16_t i = 0, j = 0;

    reg_position_t reg_position;	
    const uint16_t *data = NULL;

	if((MD_CHL_UART_DOWN == chl) || (MD_CHL_BLE_CLIENT == chl))
	{
        data = vLookupDataTab_from_other_dev( slave_address, regAddress, regNum, is_write, &reg_position, chl);					// 查询table2中的数据
	}
	else
	{
		// const uint16_t *data = vLookupDataTab( slave_address, regAddress, regNum, is_write, &reg_position);					// 查询table2中的数据
        data = vLookupDataTab( slave_address, regAddress, regNum, is_write, &reg_position);					// 查询table2中的数据
    }

	
	if(data == NULL)
	{                           
        ESP_LOGE(TAG, "UNKNOWN_REG_ADDRESS: SlaveAddr=%d, RegAddress=%d, RegNum=%d, IsWrite=%d", slave_address, regAddress, regNum, is_write);
		return 0;
	}
    
	outbuf[i++] = slave_address;

    if (regNum == 1)
    {
        outbuf[i++] = 0x06; // write single 
    }
    else 
    {
        outbuf[i++] = 0x10; // write muitl 
    }
	
    outbuf[i++] = (unsigned char)(regAddress >> 8);
    outbuf[i++] = (unsigned char) regAddress;

    if (outbuf[1] == 0x10)
    {
        outbuf[i++] = (unsigned char)(regNum >> 8);
        outbuf[i++] = (unsigned char) regNum;
        outbuf[i++] = regNum << 1; //  
    }
    
    while(regNum--)
    {
        //outbuf[i++] = (unsigned char)(data[j] >> 8);
        
        outbuf[i++] =  (data[j]>>8)&0xFF;//H
        outbuf[i++] =  data[j]&0xFF;//L
        j++;
//        j++;
    }
    
    crc = ModbusCrc16(outbuf,i);
    
    outbuf[i++] = (unsigned char) crc;
    outbuf[i++] = (unsigned char)(crc>>8);
    
    return i;
}
#endif

void plug_bind_process(const POINT_BIND_INFO_WR *point_bind_cmd)
{
    if (!point_bind_cmd)
    {
        ESP_LOGE(TAG, "Plug bind process parameter error");
        return;
    }

    if (point_bind_cmd->Dev_Type != SN_TYPE_S1)
    {
        ESP_LOGE(TAG, "Unsupported device type in bind command: %d", point_bind_cmd->Dev_Type);
        return;
    }

    int16_t ret = plug_bind(point_bind_cmd);
    if (ret >= 0)
    {
        // 可能存在不同步问题，所以以WLCC命令为准
        ESP_LOGW(TAG, "Plug SN:%llu bound successfully", point_bind_cmd->SN_64);
        int j = 0;
        for (j = 0; j < NET_WIFI_MAX_POINT; j++)
        {
            if((point_bind_cmd->SN_64 == gWlccDevList[j].dev_sn))
            {
                uint16_t slave_addr = dev_modbus_plug_slave_addr_get(gWlccDevList[j].dev_type, gWlccDevList[j].dev_sn);
                int modbus_addr = dev_modbus_get_addr_from_type_sn(gWlccDevList[j].dev_type, gWlccDevList[j].dev_sn);
                ESP_LOGW(TAG, "Get plug:%d-%llu slave addr(%d), modbus_addr(%d)", gWlccDevList[j].dev_type, gWlccDevList[j].dev_sn, slave_addr, modbus_addr);
                
                if (slave_addr <= 0 
                    || slave_addr > PLUG_MAX_NUM 
                    || modbus_addr <= 0) {
                    continue;
                }

                if (point_bind_cmd->group_addr == 0) {
                    // 解绑
                    memset((char *)g_other_rd.Plug[slave_addr - 1].mod_reg13600_open.pcs_name_set, 0x00,
                    sizeof(g_other_rd.Plug[slave_addr - 1].mod_reg13600_open.pcs_name_set));
                } else {
                    // 绑定
                    snprintf((char *)g_other_rd.Plug[slave_addr - 1].mod_reg13600_open.pcs_name_set,
                        sizeof(g_other_rd.Plug[slave_addr - 1].mod_reg13600_open.pcs_name_set),
                        "%s%llu", dev_factory.dev_type, dev_factory.dev_sn);
                }
                
                int len = wlcc_send_modbus_msg(13776, 16, gWlccDevList[j].dev_type, gWlccDevList[j].dev_sn,
                    slave_addr, (uint8_t *)inet_ntoa(gWlccDevList[j].ip), gWlccDevList[j].port, MD_CHL_WIFI_WLCC);

                if ( len < 0 ) {
                    ESP_LOGE(TAG, "wlcc send plug bound change msg error, index: %d, dev_sn: %llu, ip: %s, port: %u",
                        j, gWlccDevList[j].dev_sn, inet_ntoa(gWlccDevList[j].ip), gWlccDevList[j].port);
                } else {
                    ESP_LOGW(TAG, "wlcc send plug bound change msg, index: %d, dev_sn: %llu, ip: %s, port: %u, len: %d",
                        j, gWlccDevList[j].dev_sn, inet_ntoa(gWlccDevList[j].ip), gWlccDevList[j].port, len);
                }

                break;
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to bind plug SN:%llu, error code: %d", point_bind_cmd->SN_64, ret);
    }
}

uint8_t plug_bind_check(uint64_t plug_sn)
{
    uint8_t bind_ok=0;
    for(int i=0;i<NET_WIFI_S1_POINT;i++)
    {

        if((IotSetData.dev_info_t.plug_cfg[i].dev_sn)&&(plug_sn==IotSetData.dev_info_t.plug_cfg[i].dev_sn))
        {
            ESP_LOGI(TAG,"S1 plug bind ok");
            bind_ok=1;
            break;
        }
    }

    return bind_ok;
}
