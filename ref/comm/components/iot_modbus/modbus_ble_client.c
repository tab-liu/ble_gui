#include <ctype.h>

#include "parameter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "utils.h"


#include "comm_define.h"
#include "modbus_define.h"
#include "modbus_protocol.h"
#include "modbus_data.h"
#include "modbus_master.h"
#include "modbus_ble_client.h"

#include "app_bt.h"


static const char *TAG = "[BLE_C_MD]";

sBleMdRet_t Ble_C_Md_Alpha_Msg_Build(uint8_t msgType, uint8_t slaveAddr, uint8_t *pIn, uint16_t inLen, uint8_t *pOut)
{
	sBleMdRet_t modbusRet;

	memset(&modbusRet, 0, sizeof(sBleMdRet_t));

	if(NULL == pOut)
	{
		ESP_LOGE(TAG, "pOut is noll!");
		return modbusRet;
	}

	modbusRet.slaveAddr = slaveAddr;

	switch(msgType)
	{
		case BLE_C_MSG_R_INV_VER:
		case BLE_C_MSG_R_INV_BASE:
			modbusRet.regAddr = 1;
			modbusRet.regNum = 35;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_SOC:
			modbusRet.regAddr = 43;
			modbusRet.regNum = 1;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_LOAD_V:
			modbusRet.regAddr = 71;
			modbusRet.regNum = 1;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_GRID_V:
			modbusRet.regAddr = 77;
			modbusRet.regNum = 1;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_AC_SW:
			modbusRet.regAddr = 3007;
			modbusRet.regNum = 1;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_W_INV_AC_SW:
			modbusRet.regAddr = 3007;
			modbusRet.regNum = 1;
			modbusRet.txLen = Modbus_WriteCmd_06H_10H_Build(slaveAddr, modbusRet.regAddr, modbusRet.regNum, pIn, pOut);
			break;
		default:
			break;
	}

	return modbusRet;
}


sBleMdRet_t Ble_C_Md_Beta_Msg_Build(uint8_t msgType, uint8_t slaveAddr, uint8_t *pIn, uint16_t inLen, uint8_t *pOut)
{
	sBleMdRet_t modbusRet;

	memset(&modbusRet, 0, sizeof(sBleMdRet_t));

	if(NULL == pOut)
	{
		ESP_LOGE(TAG, "pOut is noll!");
		return modbusRet;
	}

	modbusRet.slaveAddr = slaveAddr;

	switch(msgType)
	{
		case BLE_C_MSG_R_INV_VER:
			modbusRet.regAddr = 1;
			modbusRet.regNum = 16;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_BASE:
			modbusRet.regAddr = MOD_REG_START_ADDR_01100;
			modbusRet.regNum = MOD_REG_LEN_01100;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_SOC:
			modbusRet.regAddr = MOD_REG_START_ADDR_00100;
			modbusRet.regNum = MOD_REG_LEN_00100;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_GRID_V:
			modbusRet.regAddr = MOD_REG_START_ADDR_01300;
			modbusRet.regNum = MOD_REG_LEN_01300;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_LOAD_V:
			modbusRet.regAddr = MOD_REG_START_ADDR_01400;
			modbusRet.regNum = MOD_REG_LEN_01400;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_R_INV_AC_SW:
			modbusRet.regAddr = MOD_REG_START_ADDR_02000 + (offsetof(MOD_STRUCT_reg02000,ctrl_ac)>>1);
			modbusRet.regNum = 1;
			modbusRet.txLen = Modbus_MasterReadCmd_03H(modbusRet.regAddr, modbusRet.regNum, pOut, slaveAddr, MD_CHL_BLE_CLIENT);
			break;
		case BLE_C_MSG_W_INV_AC_SW:
			modbusRet.regAddr = MOD_REG_START_ADDR_02000 + (offsetof(MOD_STRUCT_reg02000,ctrl_ac)>>1);
			modbusRet.regNum = 1;
			modbusRet.txLen = Modbus_WriteCmd_06H_10H_Build(slaveAddr, modbusRet.regAddr, modbusRet.regNum, pIn, pOut);
			break;
		default:
			break;
	}


	return modbusRet;
}

/*------------------------------------------------------------------------
*@Function： Ble_Client_Modbus_MasterReadCmd_03H_RTN
接收解析函数
-------------------------------------------------------------------------*/
/**
 *@brief	As master ,read 1 or more reg, feedback
查询返回报文透传，本地有效则进行存储

*@param[in]	 None
*@param[out]	 None
*@return		  
0- ok
1- fail,no data
*/
static uint8_t Ble_Client_Modbus_MasterReadCmd_03H_RTN(uint16_t readRegAddr, uint16_t readRegNum, const uint8_t *cmdBuf, uint16_t cmdLen)
{
    uint8_t bytesCounter = 0; 
    uint8_t i = 0;
    uint16_t regAdderss = 0xFFFF;
    uint16_t ReadRegCnt = 0xFFFF;
    uint16_t *regPtr = NULL;
    uint16_t SlaveAddr = cmdBuf[0];
//	 int chl = -1;
    reg_position_t reg_position;
    

    if((readRegNum<<1) == cmdBuf[2])
    {	 
        regAdderss = readRegAddr;
        ReadRegCnt = readRegNum;
        ESP_LOGI(TAG, "readRegAddr[%d], readRegNum[%d], retNum[%d]", readRegAddr, readRegNum, cmdBuf[2]);
    }
    else
    {
        ESP_LOGE(TAG, "readRegAddr[%d], readRegNum[%d], retNum[%d]", readRegAddr, readRegNum, cmdBuf[2]);
        return 0;
    }

    // 根据协议类型选择不同的查表函数
    if (g_other_rd.bind_dev.modbus_version == MODBUS_VERSION_BETA)
    {
        // BETA版本协议，使用现有的查表函数
        regPtr = vLookupDataTab_from_other_dev(SlaveAddr, regAdderss, ReadRegCnt, false, &reg_position, MD_CHL_BLE_CLIENT);
        ESP_LOGI(TAG, "Using BETA protocol lookup function");
    }
    else if (g_other_rd.bind_dev.modbus_version == MODBUS_VERSION_ALPHA)
    {
        // ALPHA版本协议，使用专用的查表函数
        regPtr = vLookupDataTab_from_other_dev_alpha(SlaveAddr, regAdderss, ReadRegCnt, false, &reg_position, MD_CHL_BLE_CLIENT);
        ESP_LOGI(TAG, "Using ALPHA protocol lookup function");
    }
    else
    {
        // 未知协议版本，使用默认的BETA版本处理
        ESP_LOGI(TAG, "Unknown modbus version: %d, using BETA as default", g_other_rd.bind_dev.modbus_version);
        regPtr = vLookupDataTab_from_other_dev(SlaveAddr, regAdderss, ReadRegCnt, false, &reg_position, MD_CHL_BLE_CLIENT);
    }	
    
    if (NULL != regPtr)
    {
// 		   ESP_LOGI(TAG, "Modbus_MasterReadCmd_03H_RTN : regAdderss2 ok");

        reals.uart_to_arm_read_state = 1;
        
        bytesCounter = ReadRegCnt<<1;
        
        for(i = 0; i < bytesCounter; i += 2)
        {
            regPtr[i/2] = ((uint16_t)cmdBuf[3 + i]<<8) | cmdBuf[4 + i];//H/L	 
            //ESP_LOGW(TAG, "lxy 03H_RTN_Sub1GHz : regPtr[%d] = %x",i/2, regPtr[i/2]);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Ble_Client_Modbus_MasterReadCmd_03H_RTN vLookupDataTab UNKNOWN_REG_ADDRESS");
    }

    return 0;
    
}
 
 
/*------------------------------------------------------------------------------
Function: Ble_Client_Modbus_MasterWriteCmd_06H_10H_RTN
-----------------------------------------------------------------------------*/
/**
 * @brief 	 06H 10H rtn识别（不做处理）
 * @param[in]  UART_STRUCT *struct_uart  
             const uint8_t *cmdBuf	   
* @param[out] None
* @return	 static uint8_t
*/
static uint8_t Ble_Client_Modbus_MasterWriteCmd_06H_10H_RTN(uint16_t writeRegAddr, uint16_t writeRegNum, const uint8_t *cmdBuf, uint16_t cmdLen)
{
    int chl = -1;
    
    if(0x06 == cmdBuf[1])
    {
        /*写入返回报文透传*/
        if(writeRegAddr == (((uint16_t)cmdBuf[2]<<8) | cmdBuf[3])){
            ESP_LOGW(TAG, "Ble_Client_Modbus_MasterWriteCmd_06H_10H_RTN : regAdderss : %d, ReadRegCnt : %d",writeRegAddr,writeRegNum);
        }
    }
    else if(0x10 == cmdBuf[1])
    {
        if(cmdLen == 8)
        {
            /*写入返回报文透传*/
            if((writeRegAddr == (((uint16_t)cmdBuf[2]<<8) | cmdBuf[3]))
                &&(writeRegNum == (((uint16_t)cmdBuf[4]<<8) | cmdBuf[5])))
            {
                ESP_LOGW(TAG, "Ble_Client_Modbus_MasterWriteCmd_06H_10H_RTN : regAdderss : %d, ReadRegCnt : %d",writeRegAddr,writeRegNum);
            } 
        }
        else	 
        {
// 		   /*主动上报响应*/
// 		   Modbus_Slave_Arm_Self_10H(cmdBuf, cmdLen);
//
// 		   /*主动上报透传*/
// 		   uart_to_server_queue_send(cmdBuf, cmdLen, MD_CHL_WIFI_CLOUD);
        }
    }
    return 0;
}

/*------------------------------------------------------------------------
*@Function： Ble_Client_Modbus_MasterRespones
ESP32S3做Modbus主，本函数是主解析modbus从的报文
-------------------------------------------------------------------------*/
/**
 *@brief  
*@param[in]	 struct_uart1
*@param[in]	 *cmdBuf,接收buf

*@param[out]	 cmdLen接收长度
*@return		  
0- ok
1- fail,no data
*/
uint8_t Ble_Client_Modbus_MasterRespones(modbus_addr_info_t src_addr, uint8_t *pInData, uint16_t inlen)
{
    uint8_t ret = 0;
	int funcode = 0;
	uint16_t regAdderss = 0xFFFF;
    uint16_t ReadRegCnt = 0xFFFF;
	uint8_t modbus_buffer[256];

	ESP_LOGW(TAG, "Modbus_MasterRespones   cmdLen = %d", inlen);
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, pInData, inlen, ESP_LOG_WARN);

	funcode = Modbus_Format_Check(pInData, inlen);

	if((0x03 == funcode) || (0x06 == funcode) || (0x10 == funcode))
	{ 
		if(MD_CHL_BLE_CLIENT == src_addr.channel)
		{
			switch (funcode)
		    {
		        case 0x03: 
		            //ESP_LOGI(TAG, "lxy Modbus_MasterRespones_RS485 03H");
		            ret = Ble_Client_Modbus_MasterReadCmd_03H_RTN(src_addr.regAddr, src_addr.regNum, pInData, inlen);
		            break;
		        
		        case 0x06: 	   
		        case 0x10:  
		            ret = Ble_Client_Modbus_MasterWriteCmd_06H_10H_RTN(src_addr.regAddr, src_addr.regNum, pInData, inlen);
		            break;
		        
		        default: 
		            break;
		    }
		}
		else if(MD_CHL_BLE == src_addr.channel)
		{
			Modbus_Rebuild_Frame_With_Addr(src_addr.slaveAddr, pInData, inlen, modbus_buffer);
			iot_ble_response(modbus_buffer, inlen, (uint8_t)BLE_FF01_CHAR_VAL); // modbus 响应给手机
		}	
	}
	else{
		ESP_LOGE(TAG, "funcode error");
	}

    return ret;
}

