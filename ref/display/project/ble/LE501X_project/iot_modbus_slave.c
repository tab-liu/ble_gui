/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName: iot_modbus_slave.c
Author: lhy
Date:
Description: 作为mobus从机，处理主机读、写modbus命令，生成modbus的回应主机
Version: V1.00
Function List: none
***********************************************************/
#include "iot_modbus_slave.h"
#include "iot_crc16.h"
#include "iot_data_record.h"

#define DEFAULT_ADDRESS 1

MODBUS_IOT_UNION gModbusIotRegs;
MODBUS_DATA_UNION gModbusDataRegs;
MODBUS_SET_UNION gModbusSetRegs;
MODBUS_HISTORY_UNION gModbusHistoryRegs;
MODBUS_FACTORY_UNION gModbusFactortSetRegs;

MODBUS_IAP_UNION gModbusUpdateRegs;

// MODBUS_REG06000_UNION gModbusReg06000;
// MODBUS_REG15500_UNION gModbusReg15500;
// MODBUS_REG15600_UNION gModbusReg15600;

// data lock
static volatile uint8_t Mutex;

inline void ApplyMutex(void) {
    Mutex = 1;
}

inline uint8_t GetMutex(void) {
    return Mutex;
}

inline void FreeMutex(void) {
    Mutex = 0;
}

// static void modbus_reg_init_06000(void) {
//     memset(&gModbusReg06000, 0, sizeof(gModbusReg06000));
// }

// static void modbus_reg_init_15500(void) {
//     memset(&gModbusReg15500, 0, sizeof(gModbusReg15500));
// }

// static void modbus_reg_init_15600(void) {
//     memset(&gModbusReg15600, 0, sizeof(gModbusReg15600));
// }

void ModbusRegsInit(uint32_t systemVer) {
    memset(gModbusIotRegs.IotDataStruct.InternetName,
           0,
           sizeof(gModbusIotRegs.IotDataStruct.InternetName));
    memset(gModbusIotRegs.IotDataStruct.InternetPassword,
           0,
           sizeof(gModbusIotRegs.IotDataStruct.InternetPassword));
    memset(&gModbusDataRegs, 0, sizeof(gModbusDataRegs));
    memset(&gModbusSetRegs, 0, sizeof(gModbusSetRegs));
    gModbusFactortSetRegs.FactoryInfo.FactoryMode = 0;
    long2short(gModbusDataRegs.DataStruct.StaticData.DeviceSNCode, gFactoryInfo.device.DeviceSn);

    long2short(gModbusIotRegs.IotDataStruct.IotSN, gFactoryInfo.iot.IotSn);
    long2short(gModbusIotRegs.IotDataStruct.DeviceSecurityCode, gFactoryInfo.iot.SafetyCode);

    ResLong2short(gModbusFactortSetRegs.FactoryInfo.IotSn, gFactoryInfo.iot.IotSn);
    ResLong2short(gModbusFactortSetRegs.FactoryInfo.SafetyCode, gFactoryInfo.iot.SafetyCode);

    gModbusDataRegs.DataStruct.StaticData.MCU1SoftWareH = 0;
    gModbusDataRegs.DataStruct.StaticData.MCU1SoftWareL = 0;

    gModbusDataRegs.DataStruct.StaticData.MCU2SoftWareH = 0;
    gModbusDataRegs.DataStruct.StaticData.MCU2SoftWareL = 0;

    gModbusDataRegs.DataStruct.StaticData.MCU3SoftWareH = 0;
    gModbusDataRegs.DataStruct.StaticData.MCU3SoftWareL = 0;

    gModbusDataRegs.DataStruct.StaticData.MCU4SoftWareH = (systemVer >> 16) & 0xFFFF;
    gModbusDataRegs.DataStruct.StaticData.MCU4SoftWareL = (systemVer) & 0xFFFF;

    if (strlen((const char *)gFactoryInfo.device.DeviceType)) { // Did not read Device type
        for (uint8_t i = 0, j = 0; i < DEVICE_TYPE_SRTING_LENGTH; i += 2) {
            gModbusDataRegs.DataStruct.StaticData.DeviceType[j++]
                = (gFactoryInfo.device.DeviceType[i] << 8) | gFactoryInfo.device.DeviceType[i + 1];
        }
    }

    for (uint8_t i = 0, j = 0; i < DEVICE_TYPE_SRTING_LENGTH; i += 2) { // Default Device type (IOT)
        gModbusIotRegs.IotDataStruct.IotType[j++]
            = (gFactoryInfo.iot.IotType[i] << 8) | gFactoryInfo.iot.IotType[i + 1];
    }

    // gModbusDataRegs.DataStruct.AdvancedData.BleEnable = 1; // ble  default on
    // gModbusDataRegs.DataStruct.AdvancedData.WifiEnable = 1; // wifi default on

    gModbusUpdateRegs.UpdateDataStruct.DeviceType = 0xFFFF; /*升级设备类型*/

    // modbus_reg_init_06000();
    // modbus_reg_init_15500();
    // modbus_reg_init_15600();
}

uint16_t Modbus_Error(uint8_t *ackBuf, uint8_t error) {
    uint16_t crc;
    ackBuf[1] |= 0x80;
    ackBuf[2] = error;
    crc = Modbus_Crc16Cal(ackBuf, 3);
    ackBuf[3] = crc;
    ackBuf[4] = crc >> 8;
    return 5;
}

uint16_t Modbus_ReadRegDatas(uint8_t *cmdBuf, uint8_t *ackBuf) {
    uint16_t crc; // max 256bytes
    uint16_t ackLen;
    uint16_t address;
    uint16_t regCnt;
    uint16_t dataLen = 0;
    uint8_t *dataPtr = NULL;

    address = (cmdBuf[2] << 8) | cmdBuf[3];
    regCnt = (cmdBuf[4] << 8) | cmdBuf[5];

    ackBuf[0] = DEFAULT_ADDRESS;
    ackBuf[1] = 0x03;
    ackBuf[2] = regCnt << 1;

    // LOG_D("Modbus Read address: %d, regCnt: %d", address, regCnt);
    // LOG_HEX(cmdBuf, 8);

    // if (address >= MODBUS_REG_ADDRESS_06000
    //     && address < MODBUS_REG_ADDRESS_06000 + sizeof(gModbusReg06000.ModbusData) / 2) {
    //     // LOG_D("MODBUS_REG_ADDRESS_06000: %d", address);
    //     address -= MODBUS_REG_ADDRESS_06000;
    //     dataPtr = gModbusReg06000.ModbusData;
    //     dataLen = sizeof(gModbusReg06000.ModbusData);
    // } else if (address >= MODBUS_REG_ADDRESS_15500
    //            && address < MODBUS_REG_ADDRESS_15500 + sizeof(gModbusReg15500.ModbusData) / 2) {
    //     // LOG_D("MODBUS_REG_ADDRESS_15500: %d", address);
    //     address -= MODBUS_REG_ADDRESS_15500;
    //     dataPtr = gModbusReg15500.ModbusData;
    //     dataLen = sizeof(gModbusReg15500.ModbusData);
    // } else if (address >= MODBUS_REG_ADDRESS_15600
    //            && address < MODBUS_REG_ADDRESS_15600 + sizeof(gModbusReg15600.ModbusData) / 2) {
    //     // LOG_D("MODBUS_REG_ADDRESS_15600: %d", address);
    //     address -= MODBUS_REG_ADDRESS_15600;
    //     dataPtr = gModbusReg15600.ModbusData;
    //     dataLen = sizeof(gModbusReg15600.ModbusData);
    // }

    if (address < ADV_DATA_REG_START_ADDRESS) {
        dataPtr = gModbusDataRegs.ModbusData;
        dataLen = sizeof(gModbusDataRegs.DataStruct.BasicData)
                  + sizeof(gModbusDataRegs.DataStruct.StaticData);
    } else if (address >= ADV_DATA_REG_START_ADDRESS && address < IAP_REG_ADDRESS) { // 70 - 1080
        if (Mutex != 0) { // data busy dont respones
            return Modbus_Error(ackBuf, ACK_MASTER_WAIT); // respones error
        }
        address -= ADV_DATA_REG_START_ADDRESS;
        dataPtr = (uint8_t *)&gModbusDataRegs.DataStruct.AdvancedData;
        dataLen = sizeof(gModbusDataRegs.DataStruct.AdvancedData);
    } else if (address >= HISTORY_REG_START_ADDRESS && address < SET_REG_START_ADDRESS) { // history

        address -= HISTORY_REG_START_ADDRESS;
        dataPtr = gModbusHistoryRegs.ModbusData;
        dataLen = sizeof(gModbusHistoryRegs.ModbusData);
    } else if (address >= SET_REG_START_ADDRESS && address < IOT_REG_ADDRESS) { // Set data
        address -= SET_REG_START_ADDRESS;
        dataPtr = gModbusSetRegs.SetModbusData;
        dataLen = sizeof(gModbusSetRegs.SetModbusData);
    } else if (address >= IOT_REG_ADDRESS && address < IOT_FACTORY_ADDRESS) {
        address -= IOT_REG_ADDRESS;
        dataPtr = gModbusIotRegs.ModbusData;
        dataLen = sizeof(gModbusIotRegs.ModbusData); // IoT info data
    } else if (address >= IOT_FACTORY_ADDRESS) {
        address -= IOT_FACTORY_ADDRESS;
        dataPtr = gModbusFactortSetRegs.ModbusData;
        dataLen = sizeof(gModbusFactortSetRegs.ModbusData); // IoT Factory data
    }

    address <<= 1;
    regCnt <<= 1;
    regCnt &= 0xFF;
    ackLen = 0;
    if ((address + regCnt) > dataLen) {
        // LOG_D("xx Modbus Read error address: %d, regCnt: %d", address, regCnt);
        ackBuf[1] |= 0x80;
        ackBuf[2] = BAD_ADDR_OR_CMD;
    } else {
        // LOG_D("Modbus Read address: %d, regCnt: %d", address, regCnt);
        for (ackLen = 0; ackLen < regCnt; ackLen += 2) {
            ackBuf[3 + ackLen] = dataPtr[address + ackLen + 1];
            ackBuf[4 + ackLen] = dataPtr[address + ackLen];
        }
    }

    crc = Modbus_Crc16Cal(ackBuf, (ackLen + 3));
    ackBuf[ackLen + 3] = crc; // lsb
    ackBuf[ackLen + 4] = (crc >> 8); // msb

    return (ackLen + 5);
}

uint16_t Modbus_WriteSingleData(uint8_t *cmdBuf, uint8_t *ackBuf) {
    uint16_t maxLen;
    uint8_t *regAddress;
    uint16_t address = 0;

    address = (cmdBuf[2] << 8) | cmdBuf[3];

    if (address >= IAP_REG_ADDRESS && address <= (IAP_REG_ADDRESS + sizeof(UpdateStruct) / 2)) {
        address -= IAP_REG_ADDRESS;
        maxLen = sizeof(gModbusUpdateRegs.ModbusData);
        regAddress = (uint8_t *)&gModbusUpdateRegs.ModbusData;
    } else if (address >= IOT_REG_WRITE_ADDRESS) {
        address -= IOT_REG_WRITE_ADDRESS;
        maxLen = sizeof(gModbusIotRegs.ModbusData);
        regAddress = (uint8_t *)&gModbusIotRegs.IotDataStruct.DeviceSecurityCode;
    } else {
        return 0; // unknown address
    }

    ackBuf[0] = DEFAULT_ADDRESS;
    address <<= 1;
    if ((address + 2) <= maxLen) {
        regAddress[address] = cmdBuf[5];
        regAddress[address + 1] = cmdBuf[4];
        memcpy(ackBuf, cmdBuf, 8);
        return 8;
    } else {
        return Modbus_Error(ackBuf, BAD_ADDR_OR_CMD);
    }
}

uint16_t Modbus_WriteMultiRegData(uint8_t *cmdBuf, uint8_t *ackBuf) {
    uint16_t crc;
    uint16_t address;
    uint16_t writeLen;
    uint16_t maxLen;
    uint8_t *regAddress;
    uint8_t i;

    address = (cmdBuf[2] << 8) | cmdBuf[3];
    writeLen = (uint16_t)(cmdBuf[4] << 8) | cmdBuf[5];
    writeLen <<= 1;

    memcpy(ackBuf, cmdBuf, 6);

    if (cmdBuf[6] != writeLen) {
        return Modbus_Error(ackBuf, BAD_COUNT);
    }

    if (address >= IAP_REG_ADDRESS && address <= (IAP_REG_ADDRESS + sizeof(UpdateStruct) / 2)) {
        regAddress = gModbusUpdateRegs.ModbusData;
        address -= IAP_REG_ADDRESS;
        maxLen = sizeof(gModbusUpdateRegs.ModbusData);
    } else if (address >= IOT_REG_WRITE_ADDRESS && address < IOT_FACTORY_ADDRESS) {
        regAddress = (uint8_t *)&gModbusIotRegs.IotDataStruct.InternetName; // iot info
        address -= IOT_REG_WRITE_ADDRESS;
        maxLen = sizeof(gModbusIotRegs.ModbusData) - address;
    } else if (address >= IOT_FACTORY_ADDRESS) {
        regAddress = (uint8_t *)&gModbusFactortSetRegs.ModbusData;
        address -= IOT_FACTORY_ADDRESS;
        maxLen = sizeof(gModbusFactortSetRegs.ModbusData); // iot factory
    } else {
        return 0;
    }

    address <<= 1;
    if ((address + writeLen) <= maxLen) {
        for (i = 0; i < writeLen; i += 2) {
            regAddress[address + i] = cmdBuf[8 + i];
            regAddress[address + i + 1] = cmdBuf[7 + i];
        }

        crc = Modbus_Crc16Cal(ackBuf, 6);
        ackBuf[6] = (unsigned char)crc; // lsb
        ackBuf[7] = (crc >> 8); // msb
    } else {
        return Modbus_Error(ackBuf, BAD_ADDR_OR_CMD);
    }

    return 8;
}

int Modbus_Slave(uint8_t *cmdBuf, uint16_t cmdLen, uint8_t *ackBuf) {
    uint16_t crc;
    int len = 0;
    // LOG_D("modebus slave");
    // LOG_HEX(cmdBuf, cmdLen);

    // if (!cmdBuf || (cmdLen < 5) || (cmdBuf[0] != DEFAULT_ADDRESS) || !ackBuf) {
    //     LOG_D("11 Modbus_Slave error cmdBuf: %p, cmdLen: %d, ackBuf: %p", cmdBuf, cmdLen,
    //     ackBuf); return -1; // modbus unkown pack
    // }

    crc = Modbus_Crc16Cal(cmdBuf, (cmdLen - 2));
    if (crc != ((cmdBuf[cmdLen - 1] << 8) | cmdBuf[cmdLen - 2])) { // crc check
        // LOG_D(
        //     "22 Modbus_Slave crc error cmdBuf: %p, cmdLen: %d, ackBuf: %p", cmdBuf, cmdLen,
        //     ackBuf
        // );
        return -1; // modbus unkown pack
    }

    switch (cmdBuf[1]) {
    case 0x03:
        len = Modbus_ReadRegDatas(cmdBuf, ackBuf);
        break;

    case 0x06:
        len = Modbus_WriteSingleData(cmdBuf, ackBuf);
        break;

    case 0x10:
        len = Modbus_WriteMultiRegData(cmdBuf, ackBuf);
        break;

    default:
        ackBuf[0] = DEFAULT_ADDRESS;
        ackBuf[1] = cmdBuf[1];
        len = Modbus_Error(ackBuf, FCN_NOT_SUPPORTED);
        break;
    }

    return len; // modbus response data length
}

uint8_t ModbusRespon_Check(const uint8_t *cmdBuf, uint16_t cmdLen) {
    uint8_t ret = 0;
    if ((cmdLen < 5) || (cmdBuf[0] != DEFAULT_ADDRESS)) {
        ret = 0;
    }

    if (cmdBuf[2] == 0x02) {
        ret = 1;
    } else if (cmdBuf[2] == 0x6a) {
        ret = 2;
    } else if ((cmdBuf[1] == 0x10) && (cmdBuf[3] == 0x07)) { // 按键重置密码
        ret = 3;
    } else if ((cmdBuf[1] == 0x10) && (cmdLen > 8)) { // 21000段主动上报，区分APP主动设置回复
        ret = 4;
    }
    return ret;
}
