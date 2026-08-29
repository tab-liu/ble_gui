/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName: iot_modbus_master.c
Author: lhy
Date:
Description: 作为mobus主机，生成读、写modbus命令，处理modbus从机的回应
Version: V1.00
Function List: none
***********************************************************/
#include "iot_modbus_master.h"
#include "iot_crc16.h"
// #include "esp_log.h"

#define DEFAULT_ADDRESS 1

// uint8_t sendBuf[10];
#define TAG "modbus_master"

static uint16_t gRegAddress = 0;
static uint8_t gFunCode = 0;
static uint16_t gRegCnt = 0;
static uint16_t gData = 0;
static uint8_t gRWFlags = 0;
static uint8_t gErrorCode = 0;

inline uint8_t Modbus_ErrorCode(void) {
    return gErrorCode;
}

static uint8_t Modbus_RecvRegsDatas(const uint8_t *cmdBuf) {
    uint8_t bytesCounter = 0, i;
    uint16_t regAdderss = gRegAddress;
    uint8_t *regPtr;

    if (regAdderss >= SET_REG_START_ADDRESS && regAdderss <= IOT_REG_ADDRESS) {
        regAdderss -= SET_REG_START_ADDRESS;
        regPtr = gModbusSetRegs.SetModbusData;
    } else if (regAdderss >= HISTORY_REG_START_ADDRESS
               && regAdderss <= HISTORY_REG_START_ADDRESS + sizeof(HistoryLogStruct) / 2) {
        regAdderss -= HISTORY_REG_START_ADDRESS;
        regPtr = gModbusHistoryRegs.ModbusData;
    } else {
        regPtr = gModbusDataRegs.ModbusData;
    }
    regAdderss <<= 1;
    if ((gRegCnt << 1) == cmdBuf[2]) {
        bytesCounter = cmdBuf[2];
        for (i = 0; i < bytesCounter; i += 2) {
            regPtr[regAdderss + i] = cmdBuf[4 + i];
            regPtr[regAdderss + i + 1] = cmdBuf[3 + i];
        }
        return 1;
    }

    return 0;
}

static uint8_t Modbus_RecvReadSingleResponse(const uint8_t *cmdBuf) {
    uint16_t address;
    uint16_t data;
    if (cmdBuf[0] != DEFAULT_ADDRESS) {
        return 0;
    }

    if (cmdBuf[1] != gFunCode) {
        return 0;
    }

    address = (uint16_t)(cmdBuf[2] << 8) | cmdBuf[3];
    data = (uint16_t)(cmdBuf[4] << 8) | cmdBuf[5];

    if ((address != gRegAddress) || (data != gData)) {
        return 0;
    }

    return 1;
}

static uint8_t Modbus_RecvReadMultiResponse(const uint8_t *cmdBuf) {
    uint16_t address;
    uint16_t regCnt;
    if (cmdBuf[0] != DEFAULT_ADDRESS) {
        return 0;
    }

    if (cmdBuf[1] != gFunCode) {
        return 0;
    }

    address = (uint16_t)(cmdBuf[2] << 8) | cmdBuf[3];
    regCnt = (uint16_t)(cmdBuf[2] << 4) | cmdBuf[5];

    if ((address != gRegAddress) || (regCnt != gRegCnt)) {
        return 0;
    }

    return 1;
}

uint16_t Modbus_MasterReadCmd(uint16_t regAddress, uint8_t regNum, uint8_t *cmdbuf) {
    uint16_t crc;
    uint8_t i = 0;

    if (gModbusFactortSetRegs.FactoryInfo.FactoryMode) {
        // ESP_LOGI(TAG, "FactoryMode");
        return 0;
    }

    gFunCode = 0x03;
    gRegCnt = regNum;
    gRegAddress = regAddress;

    cmdbuf[i++] = DEFAULT_ADDRESS;
    cmdbuf[i++] = gFunCode;
    cmdbuf[i++] = (uint8_t)(regAddress >> 8);
    cmdbuf[i++] = (uint8_t)regAddress;
    cmdbuf[i++] = (uint8_t)(regNum >> 8);
    cmdbuf[i++] = (uint8_t)regNum;

    crc = Modbus_Crc16Cal(cmdbuf, i);

    cmdbuf[i++] = (uint8_t)crc;
    cmdbuf[i++] = (uint8_t)(crc >> 8);

    gRWFlags = 1; // read flags
    return i;
}

uint16_t Modbus_MasterWriteCmd(uint16_t regAddress,
                               uint8_t regNum,
                               uint16_t *data,
                               uint8_t *outbuf) {
    uint16_t crc;
    uint8_t i = 0, j = 0;

    if (gModbusFactortSetRegs.FactoryInfo.FactoryMode) {
        return 0;
    }

    outbuf[i++] = DEFAULT_ADDRESS;
    gRegCnt = regNum;
    if (regNum == 1) {
        outbuf[i++] = 0x06; // write single
        gFunCode = 0x06;
        gData = data[0];
    } else {
        outbuf[i++] = 0x10; // write muitl
        gFunCode = 0x10;
    }
    gRegAddress = regAddress;
    outbuf[i++] = (unsigned char)(regAddress >> 8);
    outbuf[i++] = (unsigned char)regAddress;

    if (outbuf[1] == 0x10) {
        outbuf[i++] = (unsigned char)(regNum >> 8);
        outbuf[i++] = (unsigned char)regNum;
        outbuf[i++] = regNum << 1; //
    }

    while (regNum--) {
        outbuf[i++] = (unsigned char)(data[j] >> 8);
        outbuf[i++] = (unsigned char)data[j];
        j++;
    }

    crc = Modbus_Crc16Cal(outbuf, i);

    outbuf[i++] = (unsigned char)crc;
    outbuf[i++] = (unsigned char)(crc >> 8);

    gRWFlags = 2; // write flags
    return i;
}

uint8_t Modbus_MasterRespones(const uint8_t *cmdBuf, uint16_t cmdLen) {
    uint8_t funcode;
    uint8_t errFlags;
    uint16_t crc;
    uint8_t ret = 0;

    if (!gRWFlags) {
        return 0;
    }

    gErrorCode = 0;
    funcode = cmdBuf[1] & 0x7F;
    errFlags = cmdBuf[1] & 0x80;

    crc = Modbus_Crc16Cal(cmdBuf, (cmdLen - 2));
    if (crc != ((cmdBuf[cmdLen - 1] << 8) | cmdBuf[cmdLen - 2])) {
        return 0;
    }

    if (gFunCode != funcode) {
        return 0;
    }

    if (gRWFlags == 1) { // read
        gRWFlags = 0;
        if (errFlags) {
            gErrorCode = cmdBuf[2];
            return 2; // modbus fault
        }

        if (gRegCnt != (cmdBuf[2] >> 1)) {
            return 0;
        }

        switch (funcode) {
        case 0x03:
            ret = Modbus_RecvRegsDatas(cmdBuf);
            break;

        case 0x06:
            ret = Modbus_RecvReadSingleResponse(cmdBuf);
            break;

        case 0x10:
            ret = Modbus_RecvReadMultiResponse(cmdBuf);
            break;

        default:
            break;
        }
        gRWFlags = 0;
    } else if (gRWFlags == 2) { // write
        gRWFlags = 0;
        if (errFlags) {
            gErrorCode = cmdBuf[2];
            return 2; // modbus fault
        }

        switch (funcode) {
        case 0x06:
            ret = 1;
            break;

        case 0x10:
            ret = 1;
            break;
        }
    }

    return ret;
}

uint8_t Modbus_Master(const uint8_t *cmdBuf, uint16_t cmdLen) {
    if (gModbusFactortSetRegs.FactoryInfo.FactoryMode) {
        gRWFlags = 0;
        return 0;
    }

    if (!cmdBuf || (cmdLen < 5) || (cmdBuf[0] != DEFAULT_ADDRESS)) {
        return 0;
    }

    return Modbus_MasterRespones(cmdBuf, cmdLen);
}
