#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "modbus_slave_data.h"
#include "screen_slave_data.h"
#include "can_data.h"
#include "modbus_data.h"
#include "uart_device_process.h"

#define ADDR            0
#define FCN             1
#define REGHI           2
#define REGLO           3
#define NUMREGSHI       4
#define NUMREGSLO       5
#define OUTBYTES        2


#define FCN_NOT_SUPPORTED       1    // a Modbus function code we can't handle
#define UNKNOWN_REG_ADDRESS     2    // a Modbus "register" (command or address to us) we don't know
#define BAD_COUNT               3    // num regs != num bytes
#define CMD_NOT_COMPLETE        4    // command didn't complete successfully
#define ACK_MASTER_WAIT         5    // slave received command but need handle long time


#define IOT_FACTORY_ADDRESS         9000

#define MODBUS_TO_CAN_MAX_NUM      10
typedef struct
{
    uint16_t regaddress;
    uint16_t regcnt;
    uint16_t slaveindex;
}ble_to_dev_struct;

int Modbus_Format_Check(const uint8_t *income, uint16_t inlen);
int Modbus_Rebuild_Frame_With_Addr(uint8_t slaveAddr, uint8_t *pIn, uint8_t inLen, uint8_t *pOut);
//int Modbus_Slave(const uint8_t *income, uint16_t inlen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, int chl);
int Modbus_Slave(const uint8_t *income, uint16_t inlen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, int chl, reg_position_list_t **position_list) ;
uint16_t Modbus_Error(uint8_t *response, uint8_t error);
uint16_t Modbus_WriteMultiRegs_Report_Frame(uint8_t *response , uint8_t writeRegsCnt,uint16_t ver);
uint16_t md_data_CallBack_run(uint8_t SlaveAddress, uint16_t startAddress,uint16_t readRegCnt);
void Modbus_To_CAN_Write_Info_After_Process(uint16_t startAddress, uint16_t iReadNum, can_data_label *cmd_label);
uint16_t Modbus_ReadRegs_Bind_Ack_Frame(uint8_t *response , uint16_t readRegsCnt, uint16_t ver);
uint16_t Modbus_WriteSingleReg2(const uint8_t *income, uint16_t inLen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl, reg_position_list_t **position_list,uint8_t SlaveAddress,bool is_write);
uint16_t Modbus_WriteMultiRegs2(const uint8_t *income, uint16_t inLen, uint8_t *response,can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl, reg_position_list_t **position_list,uint8_t SlaveAddress,bool is_write) ;
void Modbus_onlyread_Check(uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum,bool is_write);
#endif
