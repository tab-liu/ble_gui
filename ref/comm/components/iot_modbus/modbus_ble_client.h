#ifndef __MODBUS_BLE_CLIENT_H__
#define __MODBUS_BLE_CLIENT_H__

#define	MODBUS_VERSION_ALPHA 	1//
#define	MODBUS_VERSION_BETA 	2//

typedef enum
{
    BLE_C_MSG_R_INV_VER  = 0,	// 逆变modbus版本信息
    BLE_C_MSG_R_INV_BASE,		// 逆变基本信息，设备类型和SN
    BLE_C_MSG_R_INV_SOC,		// 逆变汇总信息
    BLE_C_MSG_R_INV_GRID_V,		// 逆变电网信息
    BLE_C_MSG_R_INV_LOAD_V,		// 逆变负载信息
    BLE_C_MSG_R_INV_AC_SW,		// 读AC开关状态
    
	BLE_C_MSG_W_INV_WORK_MODE,	// 设置逆变工作模式
	BLE_C_MSG_W_INV_AC_SW,		// 设置AC开关
	
	BLE_C_MSG_TYPE_MAX
} eBleClientMsgType;

typedef struct {
	uint8_t slaveAddr;
	uint16_t regAddr;
	uint16_t regNum;
	uint16_t txLen;
}sBleMdRet_t;


uint8_t Ble_Client_Modbus_MasterRespones(modbus_addr_info_t src_addr, uint8_t *pInData, uint16_t inlen);

sBleMdRet_t Ble_C_Md_Beta_Msg_Build(uint8_t msgType, uint8_t slaveAddr, uint8_t *pIn, uint16_t inLen, uint8_t *pOut);
sBleMdRet_t Ble_C_Md_Alpha_Msg_Build(uint8_t msgType, uint8_t slaveAddr, uint8_t *pIn, uint16_t inLen, uint8_t *pOut);


#endif


