#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <stdint.h>
#include "modbus_data.h"
#include "modbus_define.h"

#define REP_REG_START_ADDR_11000    (MOD_REG_START_ADDR_11000 + ((uint16_t)offsetof(MOD_STRUCT_reg11000, software_ver) >> 1))
#define REP_REG_LEN_11000           ((uint16_t)(offsetof(MOD_STRUCT_reg11000, revd_end) - offsetof(MOD_STRUCT_reg11000, software_ver)) >> 1)
#define REP_REG_LEN_12000           ((uint16_t)offsetof(MOD_STRUCT_reg12000, revd_end) >> 1)
#define REP_REG_LEN_13600           ((uint16_t)offsetof(MOD_STRUCT_reg13600, revd_end) >> 1)
#define REP_REG_LEN_40000           ((uint16_t)sizeof(MOD_STRUCT_reg40000_IotDebugStatus_t) >> 1)

// 协议版本阈值结构体
typedef struct {
    uint16_t version;
    uint16_t count;
} protocol_version_entry_t;

// 寄存器配置结构体
typedef struct {
    uint16_t address;
    bool has_version_dependency;
    uint16_t default_count;
    const protocol_version_entry_t *version_table;
    uint8_t version_table_size;
    bool (*custom_handler)(uint16_t *count, uint16_t md_protocol_ver, uint8_t slaveaddr);  // 自定义处理函数指针
} register_config_t;

uint16_t Get_Regnum_By_Protocol_Ver(uint16_t ReadRegAddress, uint16_t md_protocol_ver, uint8_t slaveaddr);

uint16_t Modbus_WriteCmd_06H_10H_Build(uint8_t slave_address, uint16_t regAddress, uint8_t regNum, uint8_t *inbuf, uint8_t *outbuf);
uint16_t Modbus_MasterWriteCmd_06H_10H(uint16_t regAddress, uint8_t regNum, bool is_write, uint8_t *outbuf, uint8_t slave_address, channel_modbus chl);
uint16_t Modbus_MasterReadCmd_03H(uint16_t regAddress, uint8_t regNum, uint8_t *outbuf, uint8_t slave_address, channel_modbus chl);

#endif


