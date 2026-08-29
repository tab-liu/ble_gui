#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
//#include "modbus_slave_data.h"
//#include "uart_device_process.h"
#include "crc.h"
#include "modbus_data.h"
#include "md2can.h"

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

/*modbus单帧转CAN指令最大个数*/
#define MODBUS_TO_CAN_MAX_NUM      5

typedef struct
{
    uint16_t regaddress;
    uint16_t regcnt;
    uint16_t slaveindex;
}ble_to_dev_struct;


int Modbus_Format_Check(const uint8_t *income, uint16_t inlen);
int Modbus_Slave(const uint8_t *income, uint16_t inlen, uint8_t *response, can_data_label *cmd_label, uint16_t *cmd_num, channel_modbus chl, reg_position_list_t **position_list) ;
uint16_t Modbus_Error(uint8_t *response, uint8_t error);
uint16_t Modbus_WriteMultiRegs_Report_Frame(uint8_t *response ,uint16_t writeRegsCnt, uint16_t ver); 
uint16_t md_data_CallBack_run(uint8_t SlaveAddress, uint16_t startAddress,uint16_t readRegCnt); 
void Single_Reg_Set_to_IoT(uint8_t SlaveAddress, uint16_t RegAddress, uint16_t value);
bool Single_Reg_Get_From_IoT(uint8_t SlaveAddress, uint16_t RegAddress, uint16_t *value);
void Modbus_To_CAN_Write_Info_After_Process(uint16_t iReadAddr, uint16_t writeRegsCnt, can_data_label *can_label, uint16_t *cmd_num);
void Modbus_To_Wireless_Write_Info_After_Process(uint16_t regAddress, uint8_t reg_Num, uint8_t slave_address, int chl);
void Get_Modbus_Read_Real_And_Mask_Length(uint16_t startAddress, uint16_t readRegCnt, uint16_t *reg_real_num, uint16_t *feign_reg_remain_num);

#endif
