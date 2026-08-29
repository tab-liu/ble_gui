#include "modbus_master.h"
#include "esp_log.h"

#define TAG "[MODBUS_MASTER]"

#if 0   // 改到"modbus_master.h"查表

/*------------------------------------------------------------------------------
 Function: Get_Regnum_By_Protocol_Ver
 -----------------------------------------------------------------------------*/
/**
  * @brief      根据对应协议版本匹配寄存器长度，用于轮�-
                �和主动上报运维数据
  * @param[in]  uint16_t ReadRegAddress   
                uint16_t md_protocol_ver  
  * @param[out] None
  * @return     uint16_t
  */
uint16_t Get_Regnum_By_Protocol_Ver(uint16_t ReadRegAddress, uint16_t md_protocol_ver)
{
    uint16_t ReadRegCnt = 0;

    switch ( ReadRegAddress )
    {
        case MOD_REG_START_ADDR_00000 :
            ReadRegCnt = MOD_REG_LEN_00000;
            break;
        
        case MOD_REG_START_ADDR_00100 :
            if ( md_protocol_ver >= 2015 ) {
                ReadRegCnt = 89;//100~188
            } else if ( md_protocol_ver >= 2014 ) {
                ReadRegCnt = 88;//100~187
            } else if ( md_protocol_ver >= 2013 ) {
                ReadRegCnt = 78;//100~177
            } else if ( md_protocol_ver >= 2011 ) {
                ReadRegCnt = 75;//100~174
            } else if ( md_protocol_ver >= 2010 ) {
                ReadRegCnt = 72;//100~171
            } else {
                ReadRegCnt = 71;//100~170
            }
            break;
            
        case MOD_REG_START_ADDR_01100 :
            if ( md_protocol_ver >= 2015 ) {
                ReadRegCnt = 82;//1100~1181
            } else if ( md_protocol_ver >= 2014 ) {
                ReadRegCnt = 70;//1100~1169
            } else if ( md_protocol_ver >= 2013 ) {
                ReadRegCnt = 67;//1100~1166
            } else if ( md_protocol_ver >= 2012 ) {
                ReadRegCnt = 61;//1100~1160
            } else {
                ReadRegCnt = 55;//1100~1154
            }
            break;
            
        case MOD_REG_START_ADDR_01200 :
            if ( md_protocol_ver >= 2012 ) {
                ReadRegCnt = 92;//1200~1291
            } else {
                ReadRegCnt = 90;//1200~1289
            }
            break;
            
        case MOD_REG_START_ADDR_01300 :
            ReadRegCnt = 31;//1300~1330
            break;

        case MOD_REG_START_ADDR_01400 :
            if ( md_protocol_ver >= 2014 ) {
                ReadRegCnt = 60;//1400~1459
            } else {
                ReadRegCnt = 48;//1400~1447
            }
            break;
            
        case MOD_REG_START_ADDR_01500 :
            if ( md_protocol_ver >= 2013 ) {
                ReadRegCnt = 33;//1500~1532
            } else {
                ReadRegCnt = 30;//1500~1529
            }
            break;
                
        case MOD_REG_START_ADDR_02000 :
            if ( md_protocol_ver >= 2015 ) {
                ReadRegCnt = 85;//2000~2084
            } else if ( md_protocol_ver >= 2014 ) {
                ReadRegCnt = 81;//2000~2080
            } else if ( md_protocol_ver >= 2013 ) {
                ReadRegCnt = 78;//2000~2077
            } else if ( md_protocol_ver >= 2012 ) {
                ReadRegCnt = 75;//2000~2074
            } else {
                ReadRegCnt = 72;//2000~2071
            }
            break;
            
        case MOD_REG_START_ADDR_02200 :
            if ( md_protocol_ver >= 2013 ) {
                ReadRegCnt = 80;//2200~2279
            } else if ( md_protocol_ver >= 2012 ) {
                ReadRegCnt = 74;//2200~2273
            } else if ( md_protocol_ver >= 2010 ) {
                ReadRegCnt = 72;//2200~2271
            } else {
                ReadRegCnt = 70;//2200~2269
            }
            break;
            
        case MOD_REG_START_ADDR_03700 :
#ifdef CONFIG_MODBUS_REG_ADDR_3700_ENABLE            
            if ( md_protocol_ver >= 2017 ) {
                ReadRegCnt = 11;//3700~3710
            } else {
                ReadRegCnt = 0;//不支持
            }
#else
            ReadRegCnt = 0;//不支持
#endif
            break;
        
        case MOD_REG_START_ADDR_06000 :
            if ( md_protocol_ver >= 2015 ) {
                ReadRegCnt = 54;//6000~6053
            } else if ( md_protocol_ver >= 2014 ) {
                ReadRegCnt = 42;//6000~6041
            } else if ( md_protocol_ver >= 2012 ) {
                ReadRegCnt = 34;//6000~6033
            } else {
                ReadRegCnt = 32;//6000~6031
            }
            break;

        case MOD_REG_START_ADDR_06100 :
            ReadRegCnt = 104;//6100~6203
            break;
        
        case MOD_REG_START_ADDR_06300 :
            uint8_t cnt = (top_modbus_rd.mod_reg06300_Pack_debug.PackCellCnt + ((top_modbus_rd.mod_reg06300_Pack_debug.PackNTCCnt + 1) >> 1));
            if (0 == cnt) {
                ReadRegCnt = 2;
            } else if (96 < cnt) {
                ReadRegCnt = 2 + 96;
            } else {
                ReadRegCnt = 2 + cnt;
            }
            break;
        
        case MOD_REG_START_ADDR_07000 :
            if ( md_protocol_ver >= 2015 ) {
                ReadRegCnt = 14;//7000~7013
            } else if ( md_protocol_ver >= 2014 ) {
                ReadRegCnt = 8;//7000~7007
            } else if ( md_protocol_ver >= 2011 ) {
                ReadRegCnt = 6;//7000~7005
            } else {
                ReadRegCnt = 5;//7000~7004
            }
            break;
        case MOD_REG_START_ADDR_11000 :
            ReadRegCnt = MOD_REG_LEN_11000;
            break;
            
        default:
            ESP_LOGE(TAG, "Get_Regnum_By_Protocol_Ver ERROR! (ReadRegAddress : %d)", ReadRegAddress);
            return 0;
    }
    
    return ReadRegCnt;
}

#else

#if 0
// 06300地址的自定义处理函数
static bool handle_addr_06300(uint16_t *count, uint16_t md_protocol_ver) {
    uint8_t cnt = (top_modbus_rd.Pack[reals.Addr_can_self].mod_reg06300_Pack_cell.PackCellCnt + 
                   ((top_modbus_rd.Pack[reals.Addr_can_self].mod_reg06300_Pack_cell.PackNTCCnt + 1) >> 1));
    if (0 == cnt) {
        *count = 2;
    } else if (96 < cnt) {
        *count = 2 + 96;
    } else {
        *count = 2 + cnt;
    }
    return true;
}
#endif

// 00100地址版本表
static const protocol_version_entry_t addr_00100_versions[] = {
    {2022, 115}, {2017, 92}, {2016, 91}, {2015, 89}, {2014, 88}, {2013, 78}, {2011, 75}, {2010, 72}
};

// 01100地址版本表
static const protocol_version_entry_t addr_01100_versions[] = {
    {2021, 100}, {2014, 70}, {2013, 67}, {2012, 61}
};

// 01200地址版本表
static const protocol_version_entry_t addr_01200_versions[] = {
    {2012, 92}
};

// 01300地址版本表
static const protocol_version_entry_t addr_01300_versions[] = {
    {2022, 43}
};

// 01400地址版本表
static const protocol_version_entry_t addr_01400_versions[] = {
    {2022, 68}, {2014, 56}
};

// 01500地址版本表
static const protocol_version_entry_t addr_01500_versions[] = {
    {2020, 35}, {2016, 34}, {2013, 33}
};

// 02000地址版本表
static const protocol_version_entry_t addr_02000_versions[] = {
    {2022, 97}, {2021, 96}, {2020, 93}, {2016, 86}, {2015, 85}, {2014, 81}, {2013, 78}, {2012, 75}
};

// 02200地址版本表
static const protocol_version_entry_t addr_02200_versions[] = {
    {2022, 117}, {2021, 106}, {2020, 105}, {2019, 104}, {2017, 103}, {2016, 91}, {2013, 80}, {2012, 74}, {2010, 72}
};


#ifdef CONFIG_MODBUS_REG_ADDR_3700_ENABLE
// 03700地址版本表
static const protocol_version_entry_t addr_03700_versions[] = {
    {2017, 11}
};
#endif

// 06000地址版本表
static const protocol_version_entry_t addr_06000_versions[] = {
    {2015, 54}, {2014, 42}, {2012, 34}
};

// 07000地址版本表
static const protocol_version_entry_t addr_07000_versions[] = {
    {2015, 14}, {2014, 8}, {2011, 6}
};

// 15500地址版本表
static const protocol_version_entry_t addr_15500_versions[] = {
    {2019, 97}, {2017, 86}, {2012, 18}, {2007, 17}
};

// 06300地址的自定义处理函数
static bool handle_addr_06300(uint16_t *count, uint16_t md_protocol_ver, uint8_t slaveaddr) {
    uint8_t cnt = (top_modbus_rd.Pack[slaveaddr].mod_reg06300_Pack_cell.PackCellCnt + 
                   ((top_modbus_rd.Pack[slaveaddr].mod_reg06300_Pack_cell.PackNTCCnt + 1) >> 1));
    if (0 == cnt) {
        *count = 2;
    } else if (96 < cnt) {
        *count = 2 + 96;
    } else {
        *count = 2 + cnt;
    }
    return true;
}

// 40000地址的自定义处理函数
static bool handle_addr_40000(uint16_t *count, uint16_t md_protocol_ver, uint8_t slaveaddr) {
    *count = (sizeof(MOD_STRUCT_reg40000_IotDebugStatus_t) + 1) / 2 + 1;
    return true;
}

// 寄存器配置表
static const register_config_t register_configs[] = {
    {MOD_REG_START_ADDR_00000, false,   MOD_REG_LEN_00000,  NULL,                   0,                                                              NULL},
    {MOD_REG_START_ADDR_00100, true,    71,                 addr_00100_versions,    sizeof(addr_00100_versions)/sizeof(addr_00100_versions[0]),     NULL},
    {MOD_REG_START_ADDR_01100, true,    55,                 addr_01100_versions,    sizeof(addr_01100_versions)/sizeof(addr_01100_versions[0]),     NULL},
    {MOD_REG_START_ADDR_01200, true,    90,                 addr_01200_versions,    sizeof(addr_01200_versions)/sizeof(addr_01200_versions[0]),     NULL},
    {MOD_REG_START_ADDR_01300, true,    31,                 addr_01300_versions,    sizeof(addr_01300_versions)/sizeof(addr_01300_versions[0]),     NULL},
    {MOD_REG_START_ADDR_01400, true,    48,                 addr_01400_versions,    sizeof(addr_01400_versions)/sizeof(addr_01400_versions[0]),     NULL},
    {MOD_REG_START_ADDR_01500, true,    30,                 addr_01500_versions,    sizeof(addr_01500_versions)/sizeof(addr_01500_versions[0]),     NULL},
    {MOD_REG_START_ADDR_02000, true,    72,                 addr_02000_versions,    sizeof(addr_02000_versions)/sizeof(addr_02000_versions[0]),     NULL},
    {MOD_REG_START_ADDR_02200, true,    70,                 addr_02200_versions,    sizeof(addr_02200_versions)/sizeof(addr_02200_versions[0]),     NULL},
#ifdef CONFIG_MODBUS_REG_ADDR_3700_ENABLE
    {MOD_REG_START_ADDR_03700, true,    0,                  addr_03700_versions,    sizeof(addr_03700_versions)/sizeof(addr_03700_versions[0]),     NULL},
#else
    {MOD_REG_START_ADDR_03700, false,   0,                  NULL,                   0,                                                              NULL},
#endif
    {MOD_REG_START_ADDR_06000, true,    32,                 addr_06000_versions,    sizeof(addr_06000_versions)/sizeof(addr_06000_versions[0]),     NULL},
    {MOD_REG_START_ADDR_06100, false,   104,                NULL,                   0,                                                              NULL},
    {MOD_REG_START_ADDR_06300, false,   20,                 NULL,                   0,                                                              handle_addr_06300},
    {MOD_REG_START_ADDR_07000, true,    5,                  addr_07000_versions,    sizeof(addr_07000_versions)/sizeof(addr_07000_versions[0]),     NULL},
    {REP_REG_START_ADDR_11000, false,   REP_REG_LEN_11000,  NULL,                   0,                                                              NULL},
    {MOD_REG_START_ADDR_12000, false,   REP_REG_LEN_12000,  NULL,                   0,                                                              NULL},
    {MOD_REG_START_ADDR_13600, false,   REP_REG_LEN_13600,  NULL,                   0,                                                              NULL},
    {MOD_REG_START_ADDR_15500, true,    17,                 addr_15500_versions,    sizeof(addr_15500_versions)/sizeof(addr_15500_versions[0]),     NULL},
    {MOD_REG_START_ADDR_15700, false,   MOD_REG_LEN_15700,  NULL,                   0,                                                              NULL},
    {MOD_REG_START_ADDR_30000, false,   MOD_REG_LEN_30000,  NULL,                   0,                                                              NULL},
    {MOD_REG_START_ADDR_40000, false,   REP_REG_LEN_40000,  NULL,                   0,                                                              handle_addr_40000},
};

static const size_t register_configs_len = sizeof(register_configs) / sizeof(register_configs[0]);

/*------------------------------------------------------------------------------
 Function: Get_Regnum_By_Protocol_Ver
 -----------------------------------------------------------------------------*/
/**
  * @brief      根据对应协议版本匹配寄存器长度，用于轮�-
                �和主动上报运维数据
  * @param[in]  uint16_t ReadRegAddress   
                uint16_t md_protocol_ver  
  * @param[out] None
  * @return     uint16_t
  */
uint16_t Get_Regnum_By_Protocol_Ver(uint16_t ReadRegAddress, uint16_t md_protocol_ver, uint8_t slaveaddr)
{
    // 查找配置
    const register_config_t *config = NULL;
    for (size_t i = 0; i < register_configs_len; i++) {
        if (register_configs[i].address == ReadRegAddress) {
            config = &register_configs[i];
            break;
        }
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Get_Regnum_By_Protocol_Ver ERROR! (ReadRegAddress : %d)", ReadRegAddress);
        return 0;
    }
    
    // 自定义处理函数
    if (config->custom_handler) {
        uint16_t count;
        if (config->custom_handler(&count, md_protocol_ver, slaveaddr)) {
            return count;
        }
        return 0;
    }
    
    // 无版本依赖的情况
    if (!config->has_version_dependency) {
        return config->default_count;
    }
    
    // 有版本依赖的情况，查找版本表
    for (uint8_t i = 0; i < config->version_table_size; i++) {
        if (md_protocol_ver >= config->version_table[i].version) {
            return config->version_table[i].count;
        }
    }
    
    // 返回默认值
    return config->default_count;
}

#endif


/*------------------------------------------------------------------------
*@Function： Modbus_MasterReadCmd_03H
-------------------------------------------------------------------------*/
/**
*@brief  100ms cycle  组帧
*@param[regAddress]     reg address
*@param[regNum]    reg num
*@param[*cmdbuf]     data
*@param[type]    slave address


*@return      frame len   
*/
 uint16_t Modbus_MasterReadCmd_03H(uint16_t regAddress, uint8_t regNum, uint8_t *outbuf, uint8_t slave_address,channel_modbus chl)
{
    uint16_t crc;
    uint8_t i = 0;
    
    outbuf[i++] = slave_address;
    outbuf[i++] = 0x03;
    outbuf[i++] = (uint8_t)(regAddress >> 8);
    outbuf[i++] = (uint8_t) regAddress;
    outbuf[i++] = (uint8_t)(regNum >> 8);
    outbuf[i++] = (uint8_t) regNum;
    
    crc = ModbusCrc16(outbuf, i);
    
    outbuf[i++] = (uint8_t) crc;
    outbuf[i++] = (uint8_t)(crc>>8);

//    ESP_LOGW(TAG, "Modbus_MasterReadCmd_03H : RegAddress : %d,  ReadRegCnt : %d",regAddress, regNum);
//     ESP_LOG_BUFFER_HEX_LEVEL(TAG, outbuf, i, ESP_LOG_WARN);
    
    return i;
}

 uint16_t Modbus_WriteCmd_06H_10H_Build(uint8_t slave_address, uint16_t regAddress, uint8_t regNum, uint8_t *inbuf, uint8_t *outbuf)
 {
	 uint16_t crc;
	 uint16_t i = 0, j = 0;
	 
	 outbuf[i++] = slave_address;
 
	 if (regNum == 1){
		 outbuf[i++] = 0x06; // write single
	 }
	 else {
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
		 outbuf[i++] =	inbuf[j+1];//H
		 outbuf[i++] =	inbuf[j];//L
		 j += 2;
	 }
	 
	 crc = ModbusCrc16(outbuf,i);
	 
	 outbuf[i++] = (unsigned char) crc;
	 outbuf[i++] = (unsigned char)(crc>>8);
	 
	 return i;
 }


/*------------------------------------------------------------------------
*@Function： Modbus_MasterWriteCmd_06H_10H
-------------------------------------------------------------------------*/
/**
*@brief  100ms cycle 组帧
*@param[regAddress]     reg address
*@param[regNum]    reg num
*@param[*cmdbuf]     data
*@param[type]    slave address


*@return         
*/
uint16_t Modbus_MasterWriteCmd_06H_10H(uint16_t regAddress, uint8_t regNum, bool is_write, uint8_t *outbuf, uint8_t slave_address,channel_modbus chl)
{
    uint16_t crc;
    uint16_t i = 0, j = 0;

    reg_position_t reg_position;	
    const uint16_t *data = NULL;

	if((MD_CHL_UART_DOWN == chl) 
        || (MD_CHL_BLE_CLIENT == chl)
        || (MD_CHL_WIFI_WLCC == chl))
	{
        data = vLookupDataTab_from_other_dev( slave_address, regAddress, regNum, is_write, &reg_position, chl);					// 查询table2中的数据
	}
	else
	{
        data = vLookupDataTab( slave_address, regAddress, regNum, is_write, &reg_position, chl);					            // 查询table2中的数据
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


/*------------------------------------------------------------------------------
 Function: iot_modbus_version_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查下级modbus协议版本
  * @param[in]  void  
  * @param[out] None
  * @return     static void
  */
// static void iot_modbus_version_check(void)
// {
//     static uint8_t send_cnt = 0;//向下级询问次数

//     if(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver != 0)
//     {
//         if(IotSetData.dev_info_t.protocol_ver != top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver)
//         {
//             IotSetData.dev_info_t.protocol_ver = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver;
//             reals.SetDataWrFlag.sBit.modbus_version = 1;
//         }
//         reals.modbus_version_flag = 1;
//         ESP_LOGI(TAG, "iot_modbus_version_check : get arm modbus version : %d",top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver);
//         return;
//     }

//     if (++send_cnt > 5)
//     {
//         //超时未获取，使用本地存储
//         top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver = IotSetData.dev_info_t.protocol_ver;
//         reals.modbus_version_flag = 1;
//         ESP_LOGE(TAG, "iot_modbus_version_check : locol modbus version : %d",top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00000.modbus_ver);
//     }

//     return;
// }


