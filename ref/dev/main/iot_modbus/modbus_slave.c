#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "modbus_slave.h"
#include "modbus_data.h"
#include "esp_log.h"
#include "can_data.h"
#include "modbus_protocol.h"
#include "iot_period_task.h"
#include "can_protocol.h"
#include "uart_device_process.h"
#include "comm_define.h"
#include "can_data.h"


#define DEFAULT_ADDRESS     0x01

//#define TAG "[MODBUS]"
static const char *TAG = "[MODBUS]";

static SemaphoreHandle_t xSemaphore = NULL; //申明互斥型信号量，在FreeRTOS中二值型信号量和互斥型信号量类型完全相同。

/*------------------------------------------------------------------------
*@Function： Modbus_Format_Check
判断接收报文是否为modbus格式

*@param[in]     *income
*@param[out]    inlen
*@return
-1： fail
other:功能码

*/
int Modbus_Format_Check(const uint8_t *income, uint16_t inlen) {
    if (!income || (inlen < 5)) {
        return -1; // modbus unknown pack
    }

    if ((income[1] != 0x03) && (income[1] != 0x06) && (income[1] != 0x10)) {
        return -1;
    }

    uint16_t crc16 = ModbusCrc16(income, (inlen - 2));
    if (crc16 != ((income[inlen - 1]<<8) | income[inlen - 2])) { // crc check
        // esp_log_buffer_hex(TAG, income, inlen);
        return -1; // modbus unknown pack
    }

    return income[1]; /* 返回接收的功能码 */
}

int Modbus_Rebuild_Frame_With_Addr(uint8_t slaveAddr, uint8_t *pIn, uint8_t inLen, uint8_t *pOut)
{
	uint16_t crc;

	pOut[0] = slaveAddr;
	memcpy(pOut+1, pIn+1, inLen-3);

    crc = ModbusCrc16(pOut, inLen-2);

	pOut[inLen-2] = (uint8_t) crc;
    pOut[inLen-1] = (uint8_t)(crc>>8);

	return 0;
}

/*
系数换算
*K
1/K
*/
static uint16_t unit_check(uint16_t value, int unit) {
    uint16_t ret = 0;
    if (unit < 0) {
        ret = value / (-unit);
    } else if ((unit) > 0) {
        ret = value * unit;
    } else {
        ret = 0;
    }
    //ESP_LOGE(TAG, "unit check: %x", ret);
    return ret;
}

static uint16_t value_check(uint16_t value, uint16_t min, uint16_t max)
{
    uint16_t ret = 0;
    if (value < min) {
        ret = min;
    } else if (value > max) {
        ret = max;
    } else {
        ret = value;
    }
    //ESP_LOGE(TAG, "value check: %x", ret);
    return ret ;
}

uint16_t Modbus_Error(uint8_t *response, uint8_t error)
{
    response[1] |= 0x80;
    response[2]  = error;
    uint16_t crc16 = ModbusCrc16(response,  3);
    response[3] = crc16;
    response[4] = crc16 >> 8;
    return 5;
}


/*------------------------------------------------------------------------
*@Function :Modbus_WriteMultiRegs_report

主动上报绑定帧

*@return
0- ok
no 0: fail
*/
uint16_t md_data_CallBack_run(uint8_t SlaveAddress, uint16_t startAddress,uint16_t readRegCnt)
{
	const md_priv_data_t priv_data =
	{
//		.ota_type = BLE_OTA,					// ota通道类型
//		.ota_response = ble_ota_data_reponse,	// 传递给xmodem升级的响应函数
	};
	md_data_t *p_data = md_tbl_find(startAddress);
	if (p_data == NULL)
	{
		ESP_LOGE(TAG, "find register table failure, line: %d", __LINE__);
		return -1;
	}


	/* 表回调函数 */
	if (p_data->tbl.tbl_cb)//检查
	{
		tbl_cb_data_t cb_data = {
				.SlaveAddress =SlaveAddress,
				.reg_addr = startAddress,
				.reg_nums = readRegCnt,
				.is_write = true,
		};
		if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
		{
			ESP_LOGE(TAG, "md_data_CallBack_run :tbl_cb 1,startAddress=%u ",startAddress );

			return -1;
		}
	}
	return 0;

}

//基于《绑定帧协议》约定A100S的modbus从机地址，上报，范围定义限制在101~200；目的为解决户用储能和微逆的地址兼容



/*------------------------------------------------------------------------
*@Function :Modbus_ReadRegs


*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] chl：输入来源

*@return
0- fail
no 0: tx len
*/
static uint16_t Modbus_ReadRegs(const uint8_t *income, uint16_t inLen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, int chl)
{
    uint16_t j = 0;
    reg_array *pvSrc = NULL;
	uint16_t *dst = NULL;
	reg2_position_t reg_position = {0};
	const uint16_t *p_tab2 = NULL;
    uint8_t SlaveAddress = 0;



    {
	    SlaveAddress = income[0];
    }
    uint16_t startAddress = (income[2] << 8) | income[3];
    uint16_t readRegCnt   = (income[4] << 8) | income[5];
    // response[j++] = DEFAULT_ADDRESS;
	response[j++] = income[0];
    response[j++] = 0x03;

    if (inLen != 8)
	{
        return Modbus_Error(response, BAD_COUNT);
    }


    response[j++] = (readRegCnt << 1); // 读取的字节长度

	// Modbus_Read_Info_Process(income);

    /* 该数据由ota数据表处理 */
    const md_priv_data_t priv_data =
    {
//      .ota_type = BLE_OTA,                    // ota通道类型
//      .ota_response = ble_ota_data_reponse,   // 传递给xmodem升级的响应函数
    };
    md_data_t *p_data = md_tbl_find(startAddress);
    if (p_data == NULL)
    {
        ESP_LOGE(TAG, "find register table failure, line: %d;startAddress=%u;readRegCnt= %u", __LINE__,startAddress,readRegCnt);
//          mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
        return 0;
    }

//      if (p_data->tbl.is_write == 0)
//      {
//          ESP_LOGE(TAG, "register write prohibited, addr: %d", startAddress);
//          //mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
//          return -1;
//      }

/* 表回调函数 */
    if (p_data->tbl.tbl_cb)//检查
    {
        tbl_cb_data_t cb_data = {
			.SlaveAddress = SlaveAddress,
//			.reg_addr = startAddress,
//			.reg_nums = readRegCnt,
            .is_write = false,
			.cb_chl = chl,
        };

//		if((cb_data.SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
//			&&(cb_data.SlaveAddress <= MODBUS_SLAVE_ADDR_MICROINV_END))
		{
//			cb_data.SlaveAddress -= MODBUS_SLAVE_ADDR_WIFI_START;

	        if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
	        {
	//              mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
	            return 0;
	        }
		}
    }

	if((startAddress >= MOD_REG_START_ADDR_21000)
		&& ((startAddress + readRegCnt) <= (MOD_REG_START_ADDR_21000 + MOD_REG_LEN_21000)))//绑定帧，特殊处理，改为周期上报的多字节写发送，因为长度受限制
	{
		ESP_LOGW(TAG,"test get in Modbus_ReadRegs MOD_REG_START_ADDR_21000");
	    // if(chl == MD_CHL_BLE)
        // {
    	//     reals.net_point_Comein=1;
    	// 	return Modbus_Error(response, ACK_MASTER_WAIT);
        // }
	    // else
        // {
    	// 	return Modbus_Error(response, CMD_NOT_COMPLETE);
        // }
		if(chl == MD_CHL_BLE)
        {
			reals.bind_state_ask = g_self_data.mod_reg21000_bind.ver;//根据此值判断是读取上报还是主动上报
            if ( reals.bind_state_ask == 0 )
            {
                /*未输入协议版本*/
                return Modbus_Error(response, CMD_NOT_COMPLETE);
            }
            else
            {
                /*存在协议版本，等待处理*/
				ESP_LOGW(TAG,"test return response len: -1");
                return 0;
            }
        }
		else if(chl == MD_CHL_WIFI_CLOUD)
        {
    	    reals.modbus_self_report_mqtt = 1;// wifi mqtt通道预留
    		return Modbus_Error(response, ACK_MASTER_WAIT);
        }
	    else
        {
    		return Modbus_Error(response, CMD_NOT_COMPLETE);
        }

	}
	else if((SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
		&&(SlaveAddress <= MODBUS_SLAVE_ADDR_WIFI_TOP_END))//uart
	{

		return Modbus_Error(response, CMD_NOT_COMPLETE);

	}
	else//can
	{
#ifdef CAN_PORT_ENABLE
		// ESP_LOGI(TAG,"go in vLookupDataTab_Can");
		{
			p_tab2 = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddress, startAddress, readRegCnt, false,cmd_label, cmd_num ,&reg_position);//下级的历史记录，需要触发临时读取，上级需要查询2次才能有准确值
		}

		if(NULL != p_tab2)//can
		{
			dst = (uint16_t *)&response[3];
			for (uint16_t i = 0; i < readRegCnt; i++, j += 2)
			{
				/* table2中的数据 */
				dst[i] = LSB2MSB(p_tab2[i]);
			}
		}
		else
		{
			return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
		}
#else

        return Modbus_Error(response, CMD_NOT_COMPLETE);

#endif
	}


    uint16_t crc16 = ModbusCrc16(response, j);
    response[j++] = crc16;
    response[j++] = crc16 >> 8;
	//ESP_LOGI(TAG,"go in vLookupDataTab_Can :%d",j);
    //esp_log_buffer_hex(TAG, response, j);
    return j;
}

/**
 * income:接收到的MODBUS指令,
 * inLen:MODBUS指令长度
 * response:MODBUS响应数据
 * cmd_label: MODBUS转换成CAN命令关系表,
 * cmd_num：命令数量
 *
 * return ：MODBUS响应的数据长度
*/
/*------------------------------------------------------------------------
*@Function :Modbus_WriteSingleReg


*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

*@return
0- fail
no 0: tx len
*/
static uint16_t Modbus_WriteSingleReg(const uint8_t *income, uint16_t inLen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, int chl, reg_position_list_t **position_list)
{
    uint16_t j = 0;
    reg_array *pvdst = NULL;
	reg2_position_t reg_position = {0};
    uint16_t max_cmd_cnt = 0;
	uint8_t SlaveAddress = 0;
	uint16_t *p_tab2 = NULL;
    if (cmd_num) {
        max_cmd_cnt = (*cmd_num);
        *cmd_num = 0;
    }
    if (position_list) {
        *position_list = NULL;
    }


    {
	    SlaveAddress = income[0];
    }

    uint16_t startAddress = income[2]<<8 | income[3]; // 开始地址
    uint16_t writeRegData = income[4]<<8 | income[5]; // 写入的数据,数据已经交换lsb
	uint16_t writeRegsCnt=0;

    response[j++] = income[0];
	// response[j++] = DEFAULT_ADDRESS;
    response[j++] = 0x06;

    if (inLen != 8) {
        return Modbus_Error(response, BAD_COUNT);
    }

	// Modbus_Write_Info_Process(income);//该函数的功能 modbus写 暂由Modbus_To_CAN_Write_Info_After_Process替代

	writeRegsCnt=1;

	 if((SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
		&&(SlaveAddress <= MODBUS_SLAVE_ADDR_WIFI_TOP_END))//uart
	{

	return Modbus_Error(response, CMD_NOT_COMPLETE);
	}
	else// can
	{

#ifdef CAN_PORT_ENABLE
		p_tab2 = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddress, startAddress, writeRegsCnt, true,cmd_label, cmd_num ,&reg_position);				// 查询table2中的数据
		if(p_tab2)//can
		{
			*p_tab2 = writeRegData;
			// ESP_LOGE(TAG, "windy: vLookupDataTab_Can:  ");
		}
		else//can invalid
		{
			ESP_LOGE(TAG, "modbus to CAN single convert failed, addr: %u", startAddress);
			return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
		}

		//testwx
		printf("Modbus_WriteSingleReg ---000\n");
		char *p = (char *)income;
		for(int i = 0;i < inLen;i++)
		{
			printf("%02x ",p[i]);
		}
		printf("\n");
		Modbus_onlyread_Check(SlaveAddress, startAddress, writeRegsCnt,true);
		// if(startAddress < MOD_REG_START_ADDR_12000)//IOT自身数据的设置指令不要往下发
		{
			// Modbus_Write_Info_Process(income);//testwx
			*cmd_num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
        	Modbus_To_CAN_Write_Info_After_Process(startAddress, writeRegsCnt, cmd_label);//testwx
		}
		// else if((startAddress >= MOD_REG_START_ADDR_12000) && (startAddress < MOD_REG_START_ADDR_13000))//IOT自身数据的设置单独处理
		// {
		// 	memcpy(&Inv[reals.Addr_can_self].mod_reg12000_IOT_set, &Inv_WR.mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));
		// }
#else

        return Modbus_Error(response, CMD_NOT_COMPLETE);

#endif

	}



	/* 该数据由ota数据表处理 */
	const md_priv_data_t priv_data =
	{
//		.ota_type = BLE_OTA,					// ota通道类型
//		.ota_response = ble_ota_data_reponse,	// 传递给xmodem升级的响应函数
	};
	md_data_t *p_data = md_tbl_find(startAddress);
	if (p_data == NULL)
	{
		ESP_LOGE(TAG, "find register table failure, line: %d", __LINE__);
//			mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
		return 0;
	}

//		if (p_data->tbl.is_write == 0)
//		{
//			ESP_LOGE(TAG, "register write prohibited, addr: %d", startAddress);
//			//mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
//			return -1;
//		}

/* 表回调函数 */
	if (p_data->tbl.tbl_cb)//检查
	{
//		tbl_cb_data_t cb_data;//tbd not use
		tbl_cb_data_t cb_data = {
			.SlaveAddress = SlaveAddress,
			.reg_addr_offset =reg_position.offset,
			.reg_addr = reg_position.reg_addr,
			.reg_nums = writeRegsCnt,
			.is_write = true,
			.cb_chl = chl,
		};

//		if((cb_data.SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
//			&&(cb_data.SlaveAddress <= MODBUS_SLAVE_ADDR_MICROINV_END))
		{
//			cb_data.SlaveAddress -= MODBUS_SLAVE_ADDR_WIFI_START;

	        if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
	        {
	//              mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
	            return 0;
	        }
		}
	}

    response[j++] = income[2];
    response[j++] = income[3];
    response[j++] = income[4];
    response[j++] = income[5];
    response[j++] = income[6];
    response[j++] = income[7];
    return j;
}


/**
 * income:接收到的MODBUS指令,
 * inLen:MODBUS指令长度
 * response:MODBUS响应数据
 * cmd_label: MODBUS转换成CAN命令关系表,
 * cmd_num：命令数量
 *
 * return ：MODBUS响应的数据长度
*/
/*------------------------------------------------------------------------
*@Function :Modbus_WriteMultiRegs


*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

*@return
0- fail
no 0: tx len
*/
static uint16_t Modbus_WriteMultiRegs(const uint8_t *income, uint16_t inLen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, int chl, reg_position_list_t **position_list)
{
    // ESP_LOGI(TAG, "111 income:%p, inLen:%d, response:%p, cmd_label:%p, cmd_num:%p, chl:%d, position_list:%p",
    //             income, inLen, response, cmd_label, cmd_num, chl, position_list);
    uint16_t i = 0;
    uint16_t j = 0;
    reg_array *pvdst = NULL;
	uint16_t *src =NULL;
	reg2_position_t reg_position = {0};
	uint16_t *p_tab2 = NULL;

    uint16_t writeRegData;
    uint16_t index = 0;
    uint16_t max_cmd_cnt = 0;
    uint8_t SlaveAddress = 0;

    if (cmd_num) {
        max_cmd_cnt = *cmd_num;
        *cmd_num = 0;
    }
    if (position_list) {
        *position_list = NULL;
    }


    {
	    SlaveAddress = income[0];
    }

    //testwx
    // printf("Modbus_WriteMultiRegs ---000\n");
    // char *p = (char *)income;
    // for(int i = 0;i < inLen;i++)
    // {
    //     printf("%02x ",p[i]);
    // }
    // printf("\n");
    uint16_t startAddress  = income[2]<<8 | income[3]; // 写入寄存器地址
    uint16_t writeRegsCnt  = income[4]<<8 | income[5]; // 写入寄存器数量

    // response[j++] = DEFAULT_ADDRESS;
	response[j++] = income[0];
    response[j++] = 0x10;

    if (inLen < 9 || (inLen - 9) != (writeRegsCnt*2))
	{
        return Modbus_Error(response, BAD_COUNT);
    }
	// Modbus_Write_Info_Process(income);//tbd


	 if((SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
		&&(SlaveAddress <= MODBUS_SLAVE_ADDR_WIFI_TOP_END))//uart
	 {

	 return Modbus_Error(response, CMD_NOT_COMPLETE);


	 }
	 else  //can
 	{
#ifdef CAN_PORT_ENABLE
        ESP_LOGI(TAG, "testwx: vLookupDataTab_Can:multi  ");
		p_tab2 = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddress, startAddress, writeRegsCnt, true,cmd_label, cmd_num ,&reg_position);				// 查询table2中的数据

		src = (uint16_t *)(income + 7);
		if(p_tab2)//can
		{
			//testwx
			printf("Modbus_WriteMultiRegs ---111\n");
			char *p = (char *)income;
			esp_log_buffer_hex(TAG, p, inLen);

			for ( i = 0; i < writeRegsCnt; i++)//把从上位机、app接收到的数据写入到vLookupDataTab_Can()查到的返回对应的数据表中
			{
				writeRegData = LSB2MSB(src[i]);
				*(p_tab2+i) = writeRegData;
			}
			ESP_LOGI(TAG, "testwx: i=%d",i);
		}
		else//can invalid
		{
			ESP_LOGE(TAG, "modbus to CAN multi convert failed, addr: %u", startAddress);
			return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
		}
		Modbus_onlyread_Check(SlaveAddress, startAddress, writeRegsCnt,true);

		// if(startAddress < MOD_REG_START_ADDR_12000)//IOT自身数据的设置指令不要往下发
		{
			// Modbus_Write_Info_Process(income);//testwx
			*cmd_num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
        	Modbus_To_CAN_Write_Info_After_Process(startAddress, writeRegsCnt, cmd_label);//testwx
		}
		// else if((startAddress >= MOD_REG_START_ADDR_12000) && (startAddress < MOD_REG_START_ADDR_13000))//IOT自身数据的设置单独处理
		// {
		// 	memcpy(&Inv[reals.Addr_can_self].mod_reg12000_IOT_set, &Inv_WR.mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));
		// }


#else

        return Modbus_Error(response, CMD_NOT_COMPLETE);

#endif


	}

	/* 该数据由ota数据表处理 */
	const md_priv_data_t priv_data =
	{
//		.ota_type = BLE_OTA,					// ota通道类型
//		.ota_response = ble_ota_data_reponse,	// 传递给xmodem升级的响应函数
	};
	md_data_t *p_data = md_tbl_find(startAddress);
	ESP_LOGI(TAG,"md_tbl_find:0x%x",startAddress);
	if (p_data == NULL)
	{
		ESP_LOGE(TAG, "find register table failure, line: %d", __LINE__);
//		mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
		return 0;
	}

//		if (p_data->tbl.is_write == 0)
//		{
//			ESP_LOGE(TAG, "register write prohibited, addr: %d", startAddress);
//			//mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
//			return -1;
//		}

    /* 表回调函数 */
 	if (p_data->tbl.tbl_cb)//检查
 	{
 //		tbl_cb_data_t cb_data;//tbd not use
 		tbl_cb_data_t cb_data = {
 				.SlaveAddress = SlaveAddress,
 				.reg_addr_offset =reg_position.offset,
 				.reg_addr = reg_position.reg_addr,
 				.reg_nums = writeRegsCnt,
 				.is_write = true,
				.cb_chl = chl,
 		};



// 		if((cb_data.SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
// 			&&(cb_data.SlaveAddress <= MODBUS_SLAVE_ADDR_MICROINV_END))
 		{
// 			cb_data.SlaveAddress -= MODBUS_SLAVE_ADDR_WIFI_START;

 	        if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
 	        {
 	//              mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
 	            return 0;
 	        }
 		}
	// ESP_LOGE(TAG, "windy2 : reg_position.offset=%d,cb_data.reg_addr_offset=%d", reg_position.offset, cb_data.reg_addr_offset);

 	}
	//ESP_LOGE(TAG, "windy writeX :tbl_cb 2,startAddress=%u ",startAddress );

    // ESP_LOGI(TAG, "0x10 type=0x%x offset=0x%x len=0x%x num=%d", cmd_label[index-1].type, cmd_label[index-1].offset, cmd_label[index-1].len, index);
    response[j++] = income[2];
    response[j++] = income[3];
    response[j++] = income[4];
    response[j++] = income[5];
    uint16_t crc16 = ModbusCrc16(response, j);
    response[j++] = crc16;
    response[j++] = crc16 >> 8;

    return j;
}

/*------------------------------------------------------------------------
*@Function :Modbus_Slave
作为modbus从机的报文解析

上级BLE/wifi等到下级uart/can的转发原则：
uart 只写转发，
can:写和部分读历史记录转发



*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

*cmd_num、**position_list用 NULL来区分切换，二者一个NULL，另一个非NULL(需要输出的对象)


*@return
0- fail
no 0: tx len
*/
int Modbus_Slave(const uint8_t *income, uint16_t inlen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, int chl, reg_position_list_t **position_list)
{
    // ESP_LOGI(TAG, "000 income:%p, inlen:%d, response:%p, cmd_label:%p, cmd_num:%p, chl:%d, position_list:%p",
    //             income, inlen, response, cmd_label, cmd_num, chl, position_list);
    if (xSemaphore == NULL) { // MODBUS互斥锁,防止多任务同时访问
        xSemaphore = xSemaphoreCreateMutex();
        if (xSemaphore == NULL) {
            ESP_LOGE(TAG, "Modbus slave Semaphore Create failed");
            return 0;
        }
    }

    /* 获取信号量 */
    if (xSemaphoreTake( xSemaphore, pdMS_TO_TICKS(300) ) != pdPASS)
	{
        return 0;
    }

    //testwx
    printf("Modbus_Slave\n");
    char *p = (char *)income;
    for(int i = 0;i < inlen;i++)
    {
        printf("%02x ",p[i]);
    }
    printf("\n");
    int len = 0;
    switch (income[1])
    {
        case 0x03: len = Modbus_ReadRegs(income, inlen, response, cmd_label, cmd_num, chl);   *cmd_num = 0;    break;
        case 0x06: len = Modbus_WriteSingleReg(income, inlen, response, cmd_label, cmd_num, chl, position_list); break;
        case 0x10: len = Modbus_WriteMultiRegs(income, inlen, response, cmd_label, cmd_num, chl, position_list); break;

        default:
            response[0] = income[0];
            response[1] = income[1];
            len = Modbus_Error(response, FCN_NOT_SUPPORTED);
            break;
    }

    xSemaphoreGive(xSemaphore);  /* 释放信号量 */
    return len; // modbus response data length
}


/*------------------------------------------------------------------------
*@Function :Modbus_WriteMultiRegs_report

主动上报绑定帧
借用modbus master send 0x10 报文格式
framecnt:设备数量偏移;
第一帧为0，第二帧为前面所有帧已传递节点数量的和，最大节点数量仅支持255个
writeRegsCnt:带查设备节点数量的寄存器数量

*@return
0- fail
no 0: tx len
*/
uint16_t Modbus_WriteMultiRegs_Report_Frame(uint8_t *response , uint8_t writeRegsCnt,uint16_t ver)
{
    uint16_t i = 0;
    uint16_t j = 0;
	uint16_t *p_tab2 = NULL;
	uint16_t crc16_temp =0;

	uint8_t SlaveAddress =0;
    uint16_t startAddress  = MOD_REG_START_ADDR_21000; // 写入寄存器地址
    //uint16_t writeRegsCnt  = (sizeof(POINT_BIND_INFO) * 14)>>1; // 写入寄存器数量

	response[j++] = 0;
    response[j++] = 0x10;
    response[j++] = startAddress>>8;
    response[j++] = startAddress&0xFF;
    response[j++] = (writeRegsCnt+2)>>8;
    response[j++] = (writeRegsCnt+2)&0xFF;
    response[j++] = (writeRegsCnt+2)*2;

	response[j++] = (ver>>8)&0xFF;
	response[j++] = (ver)&0xFF;

	response[j++] = (LSB2MSB(g_self_data.mod_reg21000_bind.bias))&0xFF;
	response[j++] = (LSB2MSB(g_self_data.mod_reg21000_bind.bias)>>8)&0xFF;

	Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias=g_self_data.mod_reg21000_bind.bias;
	ESP_LOGI(TAG,"Modbus_WriteMultiRegs_Report_Frame bias:%d",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias);

    if (ver == 6) // 电表扫描结果上报
    {
        p_tab2 =(uint16_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.meter_scan_result[Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias];
    }
    else if (ver == 7) // 绑定结果上报
    {
        p_tab2 =(uint16_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.meter_bind_result[Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias];
    }
    else if (ver == 8)
    {
        p_tab2 =(uint16_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.meter_dev_state[Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias];
    }
    else
    {
	    p_tab2 =(uint16_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias];
    }

	if(p_tab2)//微逆，uart
	{
		for ( i = 0; i < writeRegsCnt; i++)
		{
    		 response[j++] = (LSB2MSB(p_tab2[i]))&0xFF;
    		 response[j++] = (LSB2MSB(p_tab2[i])>>8)&0xFF;
//
//            response[j++] = ((p_tab2[i])>>8)&0xFF;
//            response[j++] = ((p_tab2[i]))&0xFF;
		}
	}
    crc16_temp = ModbusCrc16(response, j);
    response[j++] = crc16_temp;
    response[j++] = crc16_temp >> 8;

    return j;



	//=======
}



/*
寄存器只读属性检测
*/
void Modbus_onlyread_Check(uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum,bool is_write)
{
	/*密码区有效（支持密码功能）标志:0-无效；1-有效；2/3-预留（只读） 只能为1*/
	if ((iReadAddr >= MOD_REG_START_ADDR_00000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_00000 + MOD_REG_LEN_00000)))
	{
		if (is_write != true) //read
		{
			if(0 == SlaveAddr)//汇总
			{
				Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00000.support_mode.bit.support_ble_pwd=1;
			}
			else//单INV
			{
				Inv[SlaveAddr-1].mod_reg00000.support_mode.bit.support_ble_pwd=1;
			}
		}
		else //write
		{
			Inv_WR.mod_reg00000.support_mode.bit.support_ble_pwd=1;
		}
	}
}


/*


执行接收后，将modbus INV全局变量赋值给CAN INV全局变量地址



Can beta多字节写，共计 四块 数据
2000
2200
2300
2400


*/
void Modbus_To_CAN_Write_Info_After_Process(uint16_t iReadAddr, uint16_t writeRegsCnt, can_data_label *can_label)//app_param_handler
{
    uint16_t start = 0;
    uint8_t i = 0;
    uint8_t j = 0;
    uint16_t templen = 0;
    uint8_t *temp_input = 0;
    uint8_t *temp_store = 0;

    int remain_cmd_num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
    int remain_reg_cnt = writeRegsCnt;
    uint16_t iReadNum = 0;

	ESP_LOGI(TAG, "src modbus can_label->offset:%d, can_label->len:%d, writeRegsCnt:%d, iReadAddr:%d",
		can_label->offset, can_label->len, writeRegsCnt, iReadAddr);

	/* 将modbus表的值赋给CAN表 并返回CAN表的偏移量 并修正写入字节数*/
    while((remain_cmd_num > 0) && (remain_reg_cnt > 0))
    {
        iReadNum = 0;
        can_label->type = 0;
        can_label->offset = 0;
        can_label->active_can_cmd_type = 0;
        can_label->len = 0;

    	/* 2000-2200 -> 0x1a */ //系统基本参数
    	// 2001-2004
    	if(iReadAddr <= 2004 && iReadAddr >= 2001)
    	{
    	    /*同时将设置值存入下发CAN的结构体SetData_Can*/
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,time1), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,mon), 8);
    		memcpy((uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct,time1), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,mon), 8);

            can_label->offset = ((iReadAddr - 2001) * 2) + offsetof(inv_set00_struct,time1);

            iReadNum = ((2004 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2004 - iReadAddr + 1);

    		//AC380 IOT不保存设置时间
    		SetData_Can.dev_info_t2.inv_set00.time1 = 0;
    		SetData_Can.dev_info_t2.inv_set00.time2 = 0;
    		SetData_Can.dev_info_t2.inv_set00.time3 = 0;

			// 2004 寄存器 app要求能往下设，IOT要能回复设下去的值(仅回复对应下设的值 无后续动作)，但can协议无对应寄存器 arm无法保存，只能IOT保存
			if(iReadAddr == 2004&&reals.rtc_flag.sBit.RTC_valid_from_SERVER==0)
			{
				SetData_Can.dev_info_t2.inv_set00.res = Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.all;
			}else if(reals.rtc_flag.sBit.RTC_valid_from_SERVER==1){
				SetData_Can.dev_info_t2.inv_set00.res =SetData.dev_info_t.SetTimeZone.all;
			}
    	}
    	// 2005
    	if(iReadAddr == 2005)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,work_mode), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,work_mode), 1);
    		can_label->offset = offsetof(inv_set00_struct,work_mode);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, work_mode);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, work_mode);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2006
    	if(iReadAddr == 2006)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl), 2);
    		can_label->offset = offsetof(inv_set00_struct,ctrl);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl);
			ESP_LOGI(TAG,"2006 ctrl:0x%x",Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl.all);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2007
    	if(iReadAddr == 2007)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_led), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_led), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_led);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_led);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_led);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2008
    	if(iReadAddr == 2008)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_meter), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_meter), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_meter);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_meter);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_meter);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2009
    	if(iReadAddr == 2009)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_pv), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_pv), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_pv);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_pv);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_pv);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2010
    	if(iReadAddr == 2010)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_inv), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_inv);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_inv);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_inv);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2011
    	if(iReadAddr == 2011)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_ac), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_ac);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_ac);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_ac);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2012
    	if(iReadAddr == 2012)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_dc), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_dc);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_dc);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_dc);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2013
    	if(iReadAddr == 2013)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_poweron), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_poweron), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_poweron);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_poweron);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_poweron);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
			can_data_poll_index_set(INDEX_INV_TYPE_BASE_11H); // 插队查询，防止CAN掉电来不及更新数据
    	}
    	// 2014
    	if(iReadAddr == 2014)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_dc_eco);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2015
    	if(iReadAddr == 2015)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_dc_eco_time), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dc_eco_time), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_dc_eco_time);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco_time);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_dc_eco_time);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2016
    	if(iReadAddr == 2016)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,eco_dc_power_value), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_dc_power_value), 2);
    		can_label->offset = offsetof(inv_set00_struct,eco_dc_power_value);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, eco_dc_power_value);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, eco_dc_power_value);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2017
    	if(iReadAddr == 2017)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_ac_eco);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2018
    	if(iReadAddr == 2018)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_ac_eco_time), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_ac_eco_time), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_ac_eco_time);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco_time);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_ac_eco_time);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2019
    	if(iReadAddr == 2019)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,eco_ac_power_value), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,eco_ac_power_value), 2);
    		can_label->offset = offsetof(inv_set00_struct,eco_ac_power_value);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, eco_ac_power_value);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, eco_ac_power_value);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2020
    	if(iReadAddr == 2020)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_chg_mode), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_chg_mode), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_chg_mode);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_chg_mode);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_chg_mode);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2021
        if(iReadAddr == 2021)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_super_power), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_super_power), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_super_power);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_super_power);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_super_power);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2022
    	if(iReadAddr == 2022)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_low_cap_pct), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_low_cap_pct), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_low_cap_pct);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_low_cap_pct);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_low_cap_pct);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2023
    	if(iReadAddr == 2023)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_high_cap_pct), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_high_cap_pct), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_high_cap_pct);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_high_cap_pct);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_high_cap_pct);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2024
    	if(iReadAddr == 2024)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_inv_mode), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_inv_mode), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_inv_mode);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_inv_mode);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_inv_mode);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2025
    	if(iReadAddr == 2025)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_dev_id), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_dev_id), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_dev_id);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_dev_id);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_dev_id);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2026
    	if(iReadAddr == 2026)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_all_energy_type), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_all_energy_type), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_all_energy_type);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_all_energy_type);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_all_energy_type);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2027
    	if(iReadAddr == 2027)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_now_energy_type), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_now_energy_type), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_now_energy_type);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_now_energy_type);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_now_energy_type);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2028
    	if(iReadAddr == 2028)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_log_page), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_log_page), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_log_page);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_log_page);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_log_page);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2029
    	if(iReadAddr == 2029)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_time_area), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time_area), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_time_area);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_time_area);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_time_area);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2030-2059
    	if(iReadAddr <= 2059 && iReadAddr >= 2030)
    	{
    		uint8_t temp_count = (iReadAddr - 2030);
    		// memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_time), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_time), sizeof(Inv_WR.mod_reg02000_Inv_base_set.ctrl_time));
    		can_label->offset = ((temp_count * 2) - (temp_count/3)) + offsetof(inv_set00_struct,ctrl_time);

            iReadNum = ((2059 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2059 - iReadAddr + 1);

    		for(int i = 0;i < 10;i++)// 2030~2059
    		{
    			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_time[i].lable, (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set.ctrl_time[i].lable,sizeof(Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_time[i].lable));
    			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_time[i].start, (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set.ctrl_time[i].start,sizeof(Inv_WR.mod_reg02000_Inv_base_set.ctrl_time[i].start));
    			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00.ctrl_time[i].end, (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set.ctrl_time[i].end,sizeof(Inv_WR.mod_reg02000_Inv_base_set.ctrl_time[i].end));
    		}

    		templen = sizeof(Inv_WR.mod_reg02000_Inv_base_set.ctrl_time);
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_time);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_time);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2060-2065
    	// memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType), sizeof(Inv_WR.mod_reg02000_Inv_base_set.ctrl_PvType));
    	if(iReadAddr <= 2065 && iReadAddr >= 2060) //can_label->offset = ((iReadAddr - 2060) * 2) + offsetof(inv_set00_struct,ctrl_PvType);
    	{
    		templen = 1;

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		switch(iReadAddr)
    		{
    			case 2060 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[0]), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[0]), 1);
    				can_label->offset = ((iReadAddr - 2060)) + offsetof(inv_set00_struct,ctrl_PvType[0]);
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[0]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[0]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2061 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[1]), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[1]), 1);
    				can_label->offset = ((iReadAddr - 2061)) + offsetof(inv_set00_struct,ctrl_PvType[1]);
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[1]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[1]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2062 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[2]), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[2]), 1);
    				can_label->offset = ((iReadAddr - 2062)) + offsetof(inv_set00_struct,ctrl_PvType[2]);
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[2]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[2]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2063 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[3]), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[3]), 1);
    				can_label->offset = ((iReadAddr - 2063)) + offsetof(inv_set00_struct,ctrl_PvType[3]);
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[3]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[3]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2064 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[4]), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[4]), 1);
    				can_label->offset = ((iReadAddr - 2064)) + offsetof(inv_set00_struct,ctrl_PvType[4]);
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[4]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[4]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2065 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_PvType[5]), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_PvType[5]), 1);
    				can_label->offset = ((iReadAddr - 2065)) + offsetof(inv_set00_struct,ctrl_PvType[5]);
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[5]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_PvType[5]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    		}
    	}
    	// 2066
    	if(iReadAddr == 2066)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ctrl_alarm_voice), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_alarm_voice), 1);
    		can_label->offset = offsetof(inv_set00_struct,ctrl_alarm_voice);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ctrl_alarm_voice);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ctrl_alarm_voice);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2067
    	if(iReadAddr == 2067)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,setLcdActiveTime), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ctrl_lcd_active_time), 1);
    		can_label->offset = offsetof(inv_set00_struct,setLcdActiveTime);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, setLcdActiveTime);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, setLcdActiveTime);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2072
    	if(iReadAddr == 2072)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,self_config), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,self_config), 2);
    		can_label->offset = offsetof(inv_set00_struct,self_config);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, self_config);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, self_config);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2073
    	if(iReadAddr == 2073)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,remoteSet), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,remoteSet), 2);
    		can_label->offset = offsetof(inv_set00_struct,remoteSet);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, remoteSet);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, remoteSet);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2074
    	if(iReadAddr == 2074)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,remoteSoc), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,remoteSoc), 2);
    		can_label->offset = offsetof(inv_set00_struct,remoteSoc);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, remoteSoc);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, remoteSoc);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2075
    	if(iReadAddr == 2075)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ownerShip), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ownerShip), 2);
    		can_label->offset = offsetof(inv_set00_struct,ownerShip);
			ESP_LOGI(TAG,"2075 can_label->offset:%d",can_label->offset);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ownerShip);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ownerShip);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2076
    	if(iReadAddr == 2076)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,LevelSwitch), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,LevelSwitch), 2);
    		can_label->offset = offsetof(inv_set00_struct,LevelSwitch);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, LevelSwitch);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, LevelSwitch);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2078
		if(iReadAddr == 2078)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ledColorSet), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ledColorSet), 2);
			can_label->offset = offsetof(inv_set00_struct,ledColorSet);

			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ledColorSet);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct,ledColorSet);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2083
    	if(iReadAddr == 2083)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,soc_max_ownership_set), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,soc_max_ownership_set), 2);
    		can_label->offset = offsetof(inv_set00_struct,soc_max_ownership_set);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
			ESP_LOGI(TAG,"can_label->offset:%u,iReadNum:%u,soc_max_ownership_set:%u",can_label->offset,iReadNum,Inv_can_WR.bk_inv_dev_set.inv_set00.soc_max_ownership_set.all);
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, soc_max_ownership_set);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, soc_max_ownership_set);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2084
    	if(iReadAddr == 2084)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,pv_senior_set), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,pv_senior_set), 2);
    		can_label->offset = offsetof(inv_set00_struct,pv_senior_set);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
			ESP_LOGI(TAG,"can_label->offset:%u,iReadNum:%u,pv_senior_set:%u",can_label->offset,iReadNum,Inv_can_WR.bk_inv_dev_set.inv_set00.pv_senior_set.all);
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, pv_senior_set);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, pv_senior_set);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2085
		if(iReadAddr == 2085)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,dc_output), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,DC_output), 2);
			can_label->offset = offsetof(inv_set00_struct,dc_output);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, dc_output);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, dc_output);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2086
		if(iReadAddr == 2086)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,Regulatory_set), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Regulatory_set), 2);
			can_label->offset = offsetof(inv_set00_struct,Regulatory_set);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, Regulatory_set);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, Regulatory_set);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2087
		if(iReadAddr == 2087)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,Cycle_capacity), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_capacity), 2);
			can_label->offset = offsetof(inv_set00_struct,Cycle_capacity);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, Cycle_capacity);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, Cycle_capacity);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2088
		if(iReadAddr == 2088)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,Cycle_max_capacity), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Cycle_max_capacity), 2);
			can_label->offset = offsetof(inv_set00_struct,Cycle_max_capacity);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, Cycle_max_capacity);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, Cycle_max_capacity);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2089
		if(iReadAddr == 2089)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,Effective_time_mon), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ym), 2);
			can_label->offset = offsetof(inv_set00_struct,Effective_time_mon);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, Effective_time_mon);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, Effective_time_mon);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2090
		if(iReadAddr == 2090)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,Effective_time_hour), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_dh), 2);
			can_label->offset = offsetof(inv_set00_struct,Effective_time_hour);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, Effective_time_hour);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, Effective_time_hour);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2091
		if(iReadAddr == 2091)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,Effective_time_sec), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,Effective_time_ms), 2);
			can_label->offset = offsetof(inv_set00_struct,Effective_time_sec);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, Effective_time_sec);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, Effective_time_sec);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2092
		if(iReadAddr == 2092)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,ECO_status), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,ECO_status), 2);
			can_label->offset = offsetof(inv_set00_struct,ECO_status);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct, ECO_status);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct, ECO_status);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}
		// 2093
		if(iReadAddr == 2093)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,set_AC_branch), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_AC_branch), 2);
			can_label->offset = offsetof(inv_set00_struct,set_AC_branch);

			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,set_AC_branch);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct,set_AC_branch);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}

		// 2094
		if(iReadAddr == 2094)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,set_DC_branch), (uint8_t *)&Inv_WR.mod_reg02000_Inv_base_set + offsetof(MOD_STRUCT_reg02000,set_DC_branch), 2);
			can_label->offset = offsetof(inv_set00_struct,set_DC_branch);

			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set00 + offsetof(inv_set00_struct,set_DC_branch);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set00 + offsetof(inv_set00_struct,set_DC_branch);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}

    	/* 2200-2300 -> 0x1b */ //高级设置区
    	// 2200-2203

    	if(iReadAddr <= 2203 && iReadAddr >= 2200)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,password), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,password), sizeof(Inv_WR.mod_reg02200_Inv_advance_set.password));
    		can_label->offset = ((iReadAddr - 2200) * 2) + offsetof(inv_set01_struct,password);

            iReadNum = ((2203 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2203 - iReadAddr + 1);

    		templen = sizeof(Inv_WR.mod_reg02200_Inv_advance_set.password);
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, password);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, password);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2206
    	if(iReadAddr == 2206)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_reset_factory), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_reset_factory), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_reset_factory);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_reset_factory);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_reset_factory);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2207
    	if(iReadAddr == 2207)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_grid), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_grid);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_grid);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_grid);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2208
    	if(iReadAddr == 2208)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_feedback), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_feedback), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_feedback);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_feedback);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_feedback);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2209-2217
    	if(iReadAddr <= 2217 && iReadAddr >= 2209)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_output_inv_volt), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_output_inv_volt), (offsetof(MOD_STRUCT_reg02200,off_grid_micro_rated_power) - offsetof(MOD_STRUCT_reg02200,ctrl_feedback)));
    		can_label->offset = ((iReadAddr - 2209) * 2) + offsetof(inv_set01_struct,ctrl_output_inv_volt);

            iReadNum = ((2217 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2217 - iReadAddr + 1);

    		templen = (offsetof(MOD_STRUCT_reg02200,off_grid_micro_rated_power) - offsetof(MOD_STRUCT_reg02200,ctrl_feedback));
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_output_inv_volt);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_output_inv_volt);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2218
    	if(iReadAddr == 2218)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_user_area), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_user_area), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_user_area);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_user_area);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_user_area);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2219-2224
    	// memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle), 1);//sizeof(Inv_WR.mod_reg02200_Inv_advance_set.ctrl_pv_paralle)
    	if(iReadAddr <= 2224 && iReadAddr >= 2219) //can_label->offset = ((iReadAddr - 2219) * 2) + offsetof(inv_set01_struct,ctrl_pv_paralle);
    	{
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		switch(iReadAddr)
    		{
    			case 2219 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[0]), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[0]), 1);
    				can_label->offset = ((iReadAddr - 2219)) + offsetof(inv_set01_struct,ctrl_pv_paralle[0]);
    				templen = 1;
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[0]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[0]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2220 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[1]), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[1]), 1);
    				can_label->offset = ((iReadAddr - 2220)) + offsetof(inv_set01_struct,ctrl_pv_paralle[1]);
    				templen = 1;
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[1]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[1]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2221 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[2]), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[2]), 1);
    				can_label->offset = ((iReadAddr - 2221)) + offsetof(inv_set01_struct,ctrl_pv_paralle[2]);
    				templen = 1;
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[2]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[2]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2222 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[3]), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[3]), 1);
    				can_label->offset = ((iReadAddr - 2222)) + offsetof(inv_set01_struct,ctrl_pv_paralle[3]);
    				templen = 1;
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[3]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[3]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2223 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[4]), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[4]), 1);
    				can_label->offset = ((iReadAddr - 2223)) + offsetof(inv_set01_struct,ctrl_pv_paralle[4]);
    				templen = 1;
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[4]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[4]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    			case 2224 :
    				memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_pv_paralle[5]), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_pv_paralle[5]), 1);
    				can_label->offset = ((iReadAddr - 2224)) + offsetof(inv_set01_struct,ctrl_pv_paralle[5]);
    				templen = 1;
    				temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[5]);
    				temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_pv_paralle[5]);
    				if(memcmp(temp_input, temp_store, templen) != 0)
    				{
    					memcpy(temp_store, temp_input, templen);
    					reals.flasWrFlag.sBit.set_data_inv = 1;
    				}
    				break;
    		}
    	}
    	// 2225
    	if(iReadAddr == 2225)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_grid_plus), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_grid_plus), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_grid_plus);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_grid_plus);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_grid_plus);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2226
    	if(iReadAddr == 2226)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_save_power_state), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_save_power_state), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_save_power_state);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_save_power_state);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_save_power_state);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2227
    	if(iReadAddr == 2227)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_meter_enable), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_enable), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_meter_enable);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_enable);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_enable);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2228
    	if(iReadAddr == 2228)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_meter_select), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_meter_select), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_meter_select);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_select);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_meter_select);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2229
    	if(iReadAddr == 2229)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_Multi_enable), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_Multi_enable), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_Inv_Multi_enable);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_Multi_enable);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_Multi_enable);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2230
    	if(iReadAddr == 2230)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_Inv_addr_Set), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_Inv_addr_Set);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_addr_Set);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_Inv_addr_Set);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2231-2232
    	if(iReadAddr <= 2232 && iReadAddr >= 2231)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ct_test), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ct_test), (offsetof(MOD_STRUCT_reg02200,ctrl_mix) - offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set)));
    		can_label->offset = ((iReadAddr - 2231) * 2) + offsetof(inv_set01_struct,ct_test);

            iReadNum = ((2232 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2232 - iReadAddr + 1);

    		templen = (offsetof(MOD_STRUCT_reg02200,ctrl_mix) - offsetof(MOD_STRUCT_reg02200,ctrl_Inv_addr_Set));
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ct_test);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ct_test);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2242
    	if(iReadAddr == 2242)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ctrl_mix2), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ctrl_mix2), 1);
    		can_label->offset = offsetof(inv_set01_struct,ctrl_mix2);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ctrl_mix2);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ctrl_mix2);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2243-2244
    	if(iReadAddr <= 2244 && iReadAddr >= 2243)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,ChargingPile_SET), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,ChargingPile_SET), (offsetof(MOD_STRUCT_reg02200,ct_ratio) - offsetof(MOD_STRUCT_reg02200,ems_ctrl)));
    		can_label->offset = ((iReadAddr - 2243) * 2) + offsetof(inv_set01_struct,ChargingPile_SET);

            iReadNum = ((2244 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2244 - iReadAddr + 1);

    		templen = 1;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, ChargingPile_SET);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, ChargingPile_SET);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2245~2257 ......
    	// 2258
    	if(iReadAddr == 2258)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection), 2);
    		can_label->offset = offsetof(inv_set01_struct,Undervoltage_protection);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2259
    	if(iReadAddr == 2259)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Undervoltage_protection_time), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Undervoltage_protection_time), 2);
    		can_label->offset = offsetof(inv_set01_struct,Undervoltage_protection_time);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection_time);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Undervoltage_protection_time);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2260
    	if(iReadAddr == 2260)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection), 2);
    		can_label->offset = offsetof(inv_set01_struct,Highvoltage_protection);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2261
    	if(iReadAddr == 2261)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Highvoltage_protection_time), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Highvoltage_protection_time), 2);
    		can_label->offset = offsetof(inv_set01_struct,Highvoltage_protection_time);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection_time);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Highvoltage_protection_time);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2262
    	if(iReadAddr == 2262)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection), 2);
    		can_label->offset = offsetof(inv_set01_struct,Underfrequency_protection);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2263
    	if(iReadAddr == 2263)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Underfrequency_protection_time), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Underfrequency_protection_time), 2);
    		can_label->offset = offsetof(inv_set01_struct,Underfrequency_protection_time);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection_time);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Underfrequency_protection_time);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2264
    	if(iReadAddr == 2264)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection), 2);
    		can_label->offset = offsetof(inv_set01_struct,Overvoltage_protection);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2265
    	if(iReadAddr == 2265)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Overvoltage_protection_time), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Overvoltage_protection_time), 2);
    		can_label->offset = offsetof(inv_set01_struct,Overvoltage_protection_time);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection_time);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Overvoltage_protection_time);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2266~2268 ......
    	// 2269
    	if(iReadAddr == 2269)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,setting_pv), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetCtrlPv), 2);
    		can_label->offset = offsetof(inv_set01_struct,setting_pv);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, setting_pv);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, setting_pv);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2270
    	if(iReadAddr == 2270)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Phase_set), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Phase_set), 2);
    		can_label->offset = offsetof(inv_set01_struct,Phase_set);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Phase_set);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Phase_set);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
    	// 2271
    	if(iReadAddr == 2271)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,DCHUB_set), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,DCHUB_set), 2);
    		can_label->offset = offsetof(inv_set01_struct,DCHUB_set);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, DCHUB_set);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, DCHUB_set);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2272
    	if(iReadAddr == 2272)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,SetGridMaxCurrent_in), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,SetGridMaxCurrent_in), 2);
    		can_label->offset = offsetof(inv_set01_struct,SetGridMaxCurrent_in);

            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, SetGridMaxCurrent_in);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, SetGridMaxCurrent_in);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2273
    	if(iReadAddr == 2273)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,Func_Set), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,Func_Set), 2);
    		can_label->offset = offsetof(inv_set01_struct,Func_Set);
			ESP_LOGI(TAG,"huangji Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set.all:0x%x ,offset:%u",Inv_can_WR.bk_inv_dev_set.inv_set01.Func_Set.all,can_label->offset);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;

    		templen = 2;
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, Func_Set);
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, Func_Set);
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}
		// 2274
		if(iReadAddr == 2274)
		{
			memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct,HomeCarBat_Set), (uint8_t *)&Inv_WR.mod_reg02200_Inv_advance_set + offsetof(MOD_STRUCT_reg02200,RvSettings), 2);
			can_label->offset = offsetof(inv_set01_struct,HomeCarBat_Set);
			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
			templen = 2;
			temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set01 + offsetof(inv_set01_struct, HomeCarBat_Set);
			temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set01 + offsetof(inv_set01_struct, HomeCarBat_Set);
			if(memcmp(temp_input, temp_store, templen) != 0)
			{
				memcpy(temp_store, temp_input, templen);
				reals.flasWrFlag.sBit.set_data_inv = 1;
			}
		}


    	/* 2300-2400 -> 0x1c */ //电网认证区(保留)
    	// 2300-2325
    	if(iReadAddr >= 2300 && iReadAddr <= 2325)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set02, (uint8_t *)&Inv_WR.mod_reg02300_Inv_set02_struct, sizeof(Inv_WR.mod_reg02300_Inv_set02_struct));
            can_label->offset = ((iReadAddr - 2300) * 2);

            iReadNum = ((2325 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2325 - iReadAddr + 1);

    		templen = sizeof(Inv_WR.mod_reg02300_Inv_set02_struct);
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set02;
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set02;
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}


    	/* 2400-2500 -> 0x1d */ //电网认证区
    	// 2400-2440
    	if(iReadAddr >= 2400 && iReadAddr <= 2440)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set03, (uint8_t *)&Inv_WR.mod_reg02400_Inv_certification, sizeof(Inv_WR.mod_reg02400_Inv_certification));
            can_label->offset = ((iReadAddr - 2400) * 2);

            iReadNum = ((2440 - iReadAddr + 1) > remain_reg_cnt) ? remain_reg_cnt : (2440 - iReadAddr + 1);

    		templen = sizeof(Inv_WR.mod_reg02400_Inv_certification);
    		temp_input = (uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_set03;
    		temp_store = (uint8_t *)&SetData_Can.dev_info_t2.inv_set03;
    		if(memcmp(temp_input, temp_store, templen) != 0)
    		{
    			memcpy(temp_store, temp_input, templen);
    			reals.flasWrFlag.sBit.set_data_inv = 1;
    		}
    	}

#if 0   //暂未使用
    	/* xxx -> 0x23 */ //第三方wifi
    	memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.inv_wifi, (uint8_t *)&Inv_WR.mod_reg13000_3rd_WIFI, sizeof(Inv_WR.mod_reg13000_3rd_WIFI));

    	/* xxx -> 0x27 */ //认证参数

    	/* 7000-7004 -> 0x55 */ //pack设置区
    	// 7001
    	if(iReadAddr == 7001)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_pack_dev_set.pack_config + offsetof(pack_config_struct,pack_heat_enable), (uint8_t *)&Inv_WR.mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,pack_heat_enable), 1);
    		can_label->offset = offsetof(pack_config_struct,pack_heat_enable);
    	}
    	// 7002
    	if(iReadAddr == 7002)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_pack_dev_set.pack_config + offsetof(pack_config_struct,ctr_heat_enable), (uint8_t *)&Inv_WR.mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,ctr_heat_enable), 1);
    		can_label->offset = offsetof(pack_config_struct,ctr_heat_enable);
    	}
    	// 7003
    	if(iReadAddr == 7003)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_pack_dev_set.pack_config + offsetof(pack_config_struct,unlock_failed_flags), (uint8_t *)&Inv_WR.mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,unlock_failed_flags), 1);
    		can_label->offset = offsetof(pack_config_struct,unlock_failed_flags);
    	}
    	// 7004
    	if(iReadAddr == 7004)
    	{
    		memcpy((uint8_t *)&Inv_can_WR.bk_pack_dev_set.pack_config + offsetof(pack_config_struct,max_parallel_nums), (uint8_t *)&Inv_WR.mod_reg07000_Pack_set + offsetof(MOD_STRUCT_reg07000,max_parallel_nums), 1);
    		can_label->offset = offsetof(pack_config_struct,max_parallel_nums);
    	}
#endif
		//15600
		if(iReadAddr == 15600)
		{
			Inv_can_mix_WR.d400s_hub_sets.charger_set.all=Inv_WR.mod_reg15600_D400s_set.charger_set.all;
			can_label->offset = offsetof(d400s_hub_set,charger_set);
			//ESP_LOGI(TAG,"15600 OFFSET:%u val:%u",can_label->offset,Inv_WR.mod_reg15600_D400s_set.charger_set.all);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
		}

		//15606
		if(iReadAddr == 15606)
		{
			Inv_can_mix_WR.d400s_hub_sets.dc_val_set[2].dc_current_set=Inv_WR.mod_reg15600_D400s_set.dc_val_set[2].dc_current_set;
			can_label->offset = offsetof(d400s_hub_set,dc_val_set)+5*sizeof(uint16_t);
			//ESP_LOGI(TAG,"15606 OFFSET:%u val:%u",can_label->offset,Inv_WR.mod_reg15600_D400s_set.dc_val_set[2].dc_current_set);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
		}
		//15613
		if(iReadAddr == 15613)
		{
			Inv_can_mix_WR.d400s_hub_sets.memory_val_set=Inv_WR.mod_reg15600_D400s_set.memory_val_set;
			//can_label->offset = offsetof(d400s_hub_set,dc_val_set)+5*sizeof(uint16_t);
			can_label->offset = offsetof(d400s_hub_set,memory_val_set);
			//ESP_LOGI(TAG,"15606 OFFSET:%u val:%u",can_label->offset,Inv_WR.mod_reg15600_D400s_set.dc_val_set[2].dc_current_set);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
		}

		//15614
		if(iReadAddr == 15614)
		{
			memcpy((uint8_t*)&Inv_can_mix_WR.d400s_hub_sets.mode2_set,(uint8_t*)&Inv_WR.mod_reg15600_D400s_set.mode2_set,sizeof(Inv_WR.mod_reg15600_D400s_set.mode2_set));
			can_label->offset = offsetof(d400s_hub_set,mode2_set);
			//ESP_LOGI(TAG,"15606 OFFSET:%u val:%u",can_label->offset,Inv_WR.mod_reg15600_D400s_set.dc_val_set[2].dc_current_set);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
		}

		//15615
		if(iReadAddr == 15615)
		{
			memcpy((uint8_t*)&Inv_can_mix_WR.d400s_hub_sets.mode3_set,(uint8_t*)&Inv_WR.mod_reg15600_D400s_set.mode3_set,sizeof(Inv_WR.mod_reg15600_D400s_set.mode3_set));
			can_label->offset = offsetof(d400s_hub_set,mode3_set);
			//ESP_LOGI(TAG,"15606 OFFSET:%u val:%u",can_label->offset,Inv_WR.mod_reg15600_D400s_set.dc_val_set[2].dc_current_set);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
		}

		//15616
		if(iReadAddr == 15621)
		{
			Inv_can_mix_WR.d400s_hub_sets.dc_Power_Set[2]=Inv_WR.mod_reg15600_D400s_set.dc_Power_Set[2];
			can_label->offset = offsetof(d400s_hub_set,dc_Power_Set)+3*sizeof(uint16_t);
			//ESP_LOGI(TAG,"15606 OFFSET:%u val:%u",can_label->offset,Inv_WR.mod_reg15600_D400s_set.dc_val_set[2].dc_current_set);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
		}

		//15615
		if(iReadAddr == 15625)
		{
			memcpy((uint8_t*)&Inv_can_mix_WR.d400s_hub_sets.mode4_set,(uint8_t*)&Inv_WR.mod_reg15600_D400s_set.mode4_set,sizeof(Inv_WR.mod_reg15600_D400s_set.mode4_set));
			can_label->offset = offsetof(d400s_hub_set,mode4_set);
			//ESP_LOGI(TAG,"15606 OFFSET:%u val:%u",can_label->offset,Inv_WR.mod_reg15600_D400s_set.dc_val_set[2].dc_current_set);
            iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
		}

    	// 15750
    	// 15751
    	if(iReadAddr == 15751)
    	{
    		memcpy((uint8_t *)&Inv_can_mix_WR.dc_ac_hub_setting + offsetof(dc_ac_hub_set,ac_hug_setting), (uint8_t *)&Inv_WR.mod_reg15750_Dc_Ac_Hub_set + offsetof(MOD_STRUCT_reg15750,ac_hug_setting), 2);
    		can_label->offset = offsetof(dc_ac_hub_set,ac_hug_setting);

			iReadNum = (1 > remain_reg_cnt) ? remain_reg_cnt : 1;
    	}

		/* 19000 继电器 SOC 智能控制：仅 IOT 本地，不下发 CAN */
		if ((iReadAddr >= MOD_REG_START_ADDR_19000) && (iReadAddr < (MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000)))
		{
			uint16_t seg_remain = (MOD_REG_START_ADDR_19000 + MOD_REG_LEN_19000) - iReadAddr;
			iReadNum = (seg_remain > remain_reg_cnt) ? remain_reg_cnt : seg_remain;
		}
		/* 26000 智能 TOU 控制：仅 IOT 本地，不下发 CAN */
		else if ((iReadAddr >= MOD_REG_START_ADDR_26000) && (iReadAddr < (MOD_REG_START_ADDR_26000 + MOD_REG_LEN_26000)))
		{
			uint16_t seg_remain = (MOD_REG_START_ADDR_26000 + MOD_REG_LEN_26000) - iReadAddr;
			iReadNum = (seg_remain > remain_reg_cnt) ? remain_reg_cnt : seg_remain;
		}
		/* 40000-40511 -> 0x27 认证参数透传 */
		else if ((iReadAddr >= MOD_REG_START_ADDR_40000) && (iReadAddr < (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000)))
		{
			uint16_t seg_remain = (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000) - iReadAddr;
			iReadNum = (seg_remain > remain_reg_cnt) ? remain_reg_cnt : seg_remain;
		}


        /*判断指令类型及长度*/
        if((iReadAddr >= MOD_REG_START_ADDR_02000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02000 + MOD_REG_LEN_02000)))
        {
            start = MOD_REG_START_ADDR_02000;
            can_label->type = INV_TYPE_CONFIG00_1AH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02000[%d]=%d",i,j,i,MOD_STRUCT_len_reg02000[i]);
                can_label->len += MOD_STRUCT_len_reg02000[i];
            }
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_02200) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02200 + MOD_REG_LEN_02200)))
        {
            start = MOD_REG_START_ADDR_02200;
            can_label->type = INV_TYPE_CONFIG01_1BH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02200[%d]=%d",i,j,i,MOD_STRUCT_len_reg02200[i]);
                can_label->len += MOD_STRUCT_len_reg02200[i];
            }
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_02300) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02300 + MOD_REG_LEN_02300)))
        {
            start = MOD_REG_START_ADDR_02300;
            can_label->type = INV_TYPE_CONFIG02_1CH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02300[%d]=%d",i,j,i,MOD_STRUCT_len_reg02300[i]);
                can_label->len += MOD_STRUCT_len_reg02300[i];
            }
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_02400) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_02400 + MOD_REG_LEN_02400)))
        {
            start = MOD_REG_START_ADDR_02400;
            can_label->type = INV_TYPE_CONFIG03_1DH;
            can_label->active_can_cmd_type = 0; //tbd
            for ( i = (iReadAddr -start), j = 0; j < iReadNum; i++,j++) //testwx
            {
                ESP_LOGI(TAG,"i=%d,j=%d,MOD_STRUCT_len_reg02400[%d]=%d",i,j,i,MOD_STRUCT_len_reg02400[i]);
                can_label->len += MOD_STRUCT_len_reg02400[i];
            }
        }
        else if((iReadAddr >= MOD_REG_START_ADDR_12000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_12000 + MOD_REG_LEN_12000)))
        {
            start = MOD_REG_START_ADDR_12000;
            can_label->type = IOT_TYPE_SET_02H;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            //
            memcpy((uint8_t *)&Inv_can_WR.mod_IOT_set.mod_reg12000_IOT_set + (iReadAddr -start)*2, (uint8_t *)&Inv_WR.mod_reg12000_IOT_set + (iReadAddr -start)*2,can_label->len);
        }
		else if((iReadAddr >= MOD_REG_START_ADDR_15600) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15600 + MOD_REG_LEN_15600)))
        {
            start = MOD_REG_START_ADDR_15600;
            can_label->type = MODULE_TYPE_D400S_SET_49H;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            //
            memcpy((uint8_t *)&Inv_can_mix_WR.d400s_hub_sets + (iReadAddr -start)*2, (uint8_t *)&Inv_WR.mod_reg15600_D400s_set + (iReadAddr -start)*2,can_label->len);
        }
		else if((iReadAddr >= MOD_REG_START_ADDR_15750) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_15750 + MOD_REG_LEN_15750)))
        {
            start = MOD_REG_START_ADDR_15750;
            can_label->type = MODULE_TYPE_DC_AC_HUB_SET_40H;
            can_label->active_can_cmd_type = 0; //tbd
            can_label->len = iReadNum*2;
            can_label->offset =(iReadAddr -start)*2;//此段 的modbus和can 相同结构

            //
            memcpy((uint8_t *)&Inv_can_mix_WR.dc_ac_hub_setting + (iReadAddr -start)*2, (uint8_t *)&Inv_WR.mod_reg15750_Dc_Ac_Hub_set + (iReadAddr -start)*2,can_label->len);
        }
		else if((iReadAddr >= MOD_REG_START_ADDR_40000) && ((iReadAddr+ iReadNum) <= (MOD_REG_START_ADDR_40000 + MOD_REG_LEN_40000)))
        {
            start = MOD_REG_START_ADDR_40000;
            can_label->type = INV_TYPE_CERT_27H;
            can_label->active_can_cmd_type = 0;
            can_label->len = iReadNum * 2;
            can_label->offset = (iReadAddr - start) * 2;

            memcpy((uint8_t *)&Inv_can_WR.bk_inv_dev_set.auth_param + can_label->offset,
                   (uint8_t *)&Inv_WR.mod_reg40000_transparent + can_label->offset,
                   can_label->len);
        }

        ESP_LOGI(TAG, "fix modbus can_label->offset:%d, can_label->len:%d, can_label->type:%d, iReadAddr:%d",can_label->offset, can_label->len, can_label->type, iReadAddr);

        /*判断下次循环相关*/
        remain_reg_cnt -= iReadNum;
        iReadAddr += iReadNum;
        if ( can_label->len > 0)
        {
            // 将指针向后移动一个 can_data_label 大小
            if ( remain_cmd_num > 0 )
            {
                can_label = can_label + 1;
                remain_cmd_num--;
            }
        }
        else
        {
            // CAN指令长度无效，继续访问
            can_label->type = 0;
            can_label->offset = 0;
            can_label->active_can_cmd_type = 0;
            if (!iReadNum)//无效原因为iReadNum，直接退出，访问地址有误
            {
                break;
            }
        }
    }
}


/*------------------------------------------------------------------------------
 Function: Modbus_WriteMultiRegs_Report_Frame
 -----------------------------------------------------------------------------*/
/**
  * @brief      绑定帧响应（modbus03回复）
  * @param[in]  uint8_t *response
                uint8_t writeRegsCnt
  * @param[out] None
  * @return     uint16_t
  */
uint16_t Modbus_ReadRegs_Bind_Ack_Frame(uint8_t *response , uint16_t readRegsCnt, uint16_t ver)
{
    uint16_t i = 0;
    uint16_t j = 0;
	uint16_t *p_tab2 = NULL;
	uint16_t crc16_temp = 0;
	uint8_t uBuf[256];
//	uint8_t readRegSize = (readRegsCnt+2)*2;
    uint16_t startAddress  = MOD_REG_START_ADDR_21000; // 写入寄存器地址
	j++;//response[j++] = 0;(modbus_slave中赋值)
    response[j++] = 0x10;
    //response[j++] = 0xFF;//readRegSize;

	response[j++] = startAddress>>8;
    response[j++] = startAddress&0xFF;

	response[j++] = (readRegsCnt+2)>>8;
    response[j++] = (readRegsCnt+2)&0xFF;
    response[j++] = (readRegsCnt+2)*2;

	response[j++] = (LSB2MSB(ver))&0xFF;
	response[j++] = (LSB2MSB(ver)>>8)&0xFF;

	response[j++] = (LSB2MSB(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias))&0xFF;
	response[j++] = (LSB2MSB(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias)>>8)&0xFF;
	ESP_LOGW(TAG,"mod_reg21000_bind.bias:%d",(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias);
    p_tab2 =(uint16_t *)&Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias];

	if(p_tab2)//微逆，uart
	{
		for ( i = 0; i < readRegsCnt; i++)
		{
    		 response[j++] = (LSB2MSB(p_tab2[i]))&0xFF;
    		 response[j++] = (LSB2MSB(p_tab2[i])>>8)&0xFF;
			ESP_LOGW(TAG,"p_tab2[%d]:0x%x",i,(unsigned int)p_tab2[i]);
		}
	}
	response[2]=j-3;
	ESP_LOGI(TAG, "j: %d", j);
    crc16_temp = ModbusCrc16(response, j);
    response[j++] = crc16_temp;
    response[j++] = crc16_temp >> 8;

    return j;
}

//windy add通用型 modbus交互函数

/**
 * income:接收到的MODBUS指令,
 * inLen:MODBUS指令长度
 * response:MODBUS响应数据
 * cmd_label: MODBUS转换成CAN命令关系表,
 * cmd_num：命令数量
 *
 * return ：MODBUS响应的数据长度
*/
/*------------------------------------------------------------------------
*@Function :Modbus_WriteSingleReg


*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

uint8_t SlaveAddress:输入从机地址
bool is_write：是否为写入变量数组,true,false


*@return
0- fail
no 0: tx len
*/
uint16_t Modbus_WriteSingleReg2(const uint8_t *income, uint16_t inLen, uint8_t *response,
                                can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl,
                                reg_position_list_t **position_list, uint8_t SlaveAddress, bool is_write)
{
    uint16_t j = 0;
    reg_array *pvdst = NULL;
    reg2_position_t reg_position = {0};
    uint16_t max_cmd_cnt = 0;
    // uint8_t SlaveAddress = 0;
    uint16_t *p_tab2 = NULL;
    if (cmd_num) {
        max_cmd_cnt = (*cmd_num);
        *cmd_num = 0;
    }
    if (position_list) {
        *position_list = NULL;
    }

    // SlaveAddress = income[0];

    uint16_t startAddress = income[2]<<8 | income[3]; // 开始地址
    uint16_t writeRegData = income[4]<<8 | income[5]; // 写入的数据,数据已经交换lsb
    uint16_t writeRegsCnt=0;

    response[j++] = income[0];
    // response[j++] = DEFAULT_ADDRESS;
    response[j++] = 0x06;

    if (inLen != 8) {
        return Modbus_Error(response, BAD_COUNT);
    }

    // Modbus_Write_Info_Process(income);//该函数的功能 modbus写 暂由Modbus_To_CAN_Write_Info_After_Process替代

    writeRegsCnt=1;

    if((SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
        &&(SlaveAddress <= MODBUS_SLAVE_ADDR_WIFI_TOP_END))//uart
    {
        return Modbus_Error(response, CMD_NOT_COMPLETE);
    }
    else// can
    {
#ifdef CAN_PORT_ENABLE
        p_tab2 = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddress, startAddress, writeRegsCnt, true,cmd_label, cmd_num ,&reg_position);				// 查询table2中的数据
        if(p_tab2)//can
        {
            *p_tab2 = writeRegData;
            // ESP_LOGE(TAG, "windy: vLookupDataTab_Can:  ");
        }
        else//can invalid
        {
            ESP_LOGE(TAG, "modbus to CAN single convert failed, addr: %u", startAddress);
            return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
        }

        // if(startAddress < MOD_REG_START_ADDR_12000)//IOT自身数据的设置指令不要往下发
        {
            // Modbus_Write_Info_Process(income);//testwx
            *cmd_num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
            Modbus_To_CAN_Write_Info_After_Process(startAddress, writeRegsCnt, cmd_label);//testwx
        }
        // else if((startAddress >= MOD_REG_START_ADDR_12000) && (startAddress < MOD_REG_START_ADDR_13000))//IOT自身数据的设置单独处理
        // {
        // 	memcpy(&Inv[reals.Addr_can_self].mod_reg12000_IOT_set, &Inv_WR.mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));
        // }
#else
        return Modbus_Error(response, CMD_NOT_COMPLETE);
#endif
    }
    /* 该数据由ota数据表处理 */
    const md_priv_data_t priv_data =
    {
        // .ota_type = BLE_OTA,					// ota通道类型
        // .ota_response = ble_ota_data_reponse,	// 传递给xmodem升级的响应函数
    };
    md_data_t *p_data = md_tbl_find(startAddress);
    if (p_data == NULL)
    {
        ESP_LOGE(TAG, "find register table failure, line: %d", __LINE__);
        // mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
        return -1;
    }

    // if (p_data->tbl.is_write == 0)
    // {
    //     ESP_LOGE(TAG, "register write prohibited, addr: %d", startAddress);
    //     //mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
    //     return -1;
    // }

    /* 表回调函数 */
    if (p_data->tbl.tbl_cb)//检查
    {
        tbl_cb_data_t cb_data = {
            .SlaveAddress = SlaveAddress,
            .reg_addr_offset =reg_position.offset,
            .reg_addr = reg_position.reg_addr,
            .reg_nums = writeRegsCnt,
            .is_write = true,
			.cb_chl = chl,
        };

    // if((cb_data.SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
    //     &&(cb_data.SlaveAddress <= MODBUS_SLAVE_ADDR_MICROINV_END))
        {
            // cb_data.SlaveAddress -= MODBUS_SLAVE_ADDR_WIFI_START;

            if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
            {
                // mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
                return -1;
            }
        }
    }

    response[j++] = income[2];
    response[j++] = income[3];
    response[j++] = income[4];
    response[j++] = income[5];
    response[j++] = income[6];
    response[j++] = income[7];
    return j;
}

/**
 * income:接收到的MODBUS指令,
 * inLen:MODBUS指令长度
 * response:MODBUS响应数据
 * cmd_label: MODBUS转换成CAN命令关系表,
 * cmd_num：命令数量
 *
 * return ：MODBUS响应的数据长度
*/
/*------------------------------------------------------------------------
*@Function :Modbus_WriteMultiRegs
*@param[in] *income  :uart rx buf
*@param[in] inlen : rx len
*@param[in] *response:uart tx buf
*@param[in] *cmd_label:CAN透传
*@param[in] *cmd_num:CAN透传
*@param[in] chl：输入来源
*@param[in] **position_list：DTU UART透传

uint8_t SlaveAddress:输入从机地址
bool is_write：是否为写入变量数组,true,false

*@return
0- fail
no 0: tx len
*/
uint16_t Modbus_WriteMultiRegs2(const uint8_t *income, uint16_t inLen, uint8_t *response,
                                can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl,
                                reg_position_list_t **position_list, uint8_t SlaveAddress, bool is_write)
{
    uint16_t i = 0;
    uint16_t j = 0;
    reg_array *pvdst = NULL;
    uint16_t *src =NULL;
    reg2_position_t reg_position = {0};
    uint16_t *p_tab2 = NULL;
    uint16_t writeRegData;
    uint16_t index = 0;
    uint16_t max_cmd_cnt = 0;
    // uint8_t SlaveAddress = 0;

    if (cmd_num)
    {
        max_cmd_cnt = *cmd_num;
        *cmd_num = 0;
    }
    if (position_list)
    {
        *position_list = NULL;
    }

    // SlaveAddress = income[0];

    uint16_t startAddress  = income[2]<<8 | income[3]; // 写入寄存器地址
    uint16_t writeRegsCnt  = income[4]<<8 | income[5]; // 写入寄存器数量

    // response[j++] = DEFAULT_ADDRESS;
	response[j++] = income[0];
    response[j++] = 0x10;

    if (inLen < 9 || (inLen - 9) != (writeRegsCnt*2))
    {
        return Modbus_Error(response, BAD_COUNT);
    }
    // Modbus_Write_Info_Process(income);//tbd

    if((SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
    &&(SlaveAddress <= MODBUS_SLAVE_ADDR_WIFI_TOP_END))//uart
    {
        return Modbus_Error(response, CMD_NOT_COMPLETE);
    }
    else  //can
    {
#ifdef CAN_PORT_ENABLE
        ESP_LOGI(TAG, "testwx: vLookupDataTab_Can:multi  ");
        p_tab2 = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddress, startAddress,
                                    writeRegsCnt, is_write, cmd_label,
                                    cmd_num ,&reg_position);                        // 查询table2中的数据

        src = (uint16_t *)(income + 7);
        if(p_tab2)//can
        {
            //testwx
            printf("Modbus_WriteMultiRegs ---111\n");
            char *p = (char *)income;
            esp_log_buffer_hex(TAG, p, inLen);

            for ( i = 0; i < writeRegsCnt; i++)//把从上位机、app接收到的数据写入到vLookupDataTab_Can()查到的返回对应的数据表中
            {
                writeRegData = LSB2MSB(src[i]);
                *(p_tab2+i) = writeRegData;
            }
            ESP_LOGI(TAG, "testwx: i=%d",i);
        }
        else//can invalid
        {
            ESP_LOGE(TAG, "modbus to CAN multi convert failed, addr: %u", startAddress);
            return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
        }

        // if(startAddress < MOD_REG_START_ADDR_12000)//IOT自身数据的设置指令不要往下发
        {
            // Modbus_Write_Info_Process(income);//testwx
            *cmd_num = (writeRegsCnt > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : writeRegsCnt;
            Modbus_To_CAN_Write_Info_After_Process(startAddress, writeRegsCnt, cmd_label);//testwx
        }
        // else if((startAddress >= MOD_REG_START_ADDR_12000) && (startAddress < MOD_REG_START_ADDR_13000))//IOT自身数据的设置单独处理
        // {
        // 	memcpy(&Inv[reals.Addr_can_self].mod_reg12000_IOT_set, &Inv_WR.mod_reg12000_IOT_set, sizeof(MOD_STRUCT_reg12000));
        // }
#else
        return Modbus_Error(response, CMD_NOT_COMPLETE);
#endif
    }

    /* 该数据由ota数据表处理 */
    const md_priv_data_t priv_data =
    {
    //		.ota_type = BLE_OTA,					// ota通道类型
    //		.ota_response = ble_ota_data_reponse,	// 传递给xmodem升级的响应函数
    };
    md_data_t *p_data = md_tbl_find(startAddress);
    if (p_data == NULL)
    {
        ESP_LOGE(TAG, "find register table failure, line: %d", __LINE__);
        return -1;
    }

    /* 表回调函数 */
    if (p_data->tbl.tbl_cb)//检查
    {
    //		tbl_cb_data_t cb_data;//tbd not use
        tbl_cb_data_t cb_data = {
            .SlaveAddress = SlaveAddress,
            .reg_addr_offset =reg_position.offset,
            .reg_addr = reg_position.reg_addr,
            .reg_nums = writeRegsCnt,
            .is_write = true,
			.cb_chl = chl,
        };
    // 		if((cb_data.SlaveAddress >= MODBUS_SLAVE_ADDR_WIFI_START)
    // 			&&(cb_data.SlaveAddress <= MODBUS_SLAVE_ADDR_MICROINV_END))
        {
    // 			cb_data.SlaveAddress -= MODBUS_SLAVE_ADDR_WIFI_START;

            if (p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data) != 0)//(void *)，执行函数
            {
    //              mb_rsp_error(buff->in_buff, buff->out_buff, buff->out_len);
                return -1;
            }
        }
    }
    response[j++] = income[2];
    response[j++] = income[3];
    response[j++] = income[4];
    response[j++] = income[5];
    uint16_t crc16 = ModbusCrc16(response, j);
    response[j++] = crc16;
    response[j++] = crc16 >> 8;

    return j;
}

