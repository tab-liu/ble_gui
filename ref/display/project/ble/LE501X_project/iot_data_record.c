/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName: iot_data_record.c
Author: weiyt
Date: 2021/11/1
Description: IOT板本身的型号、SN码的保存和读取，与IOT板连接的设备的型号和SN码的保存和读取
Version: V1.00
Function List: none
***********************************************************/

#include "iot_data_record.h"
#include "iot_flash.h"
#include "iot_crc16.h"
// #include "nvs_flash.h"

static uint8_t gFlashLock = 0;
SystemFactoryInfo gFactoryInfo;

#define TAG "data_record"

int Iot_Factroy_Info_test(uint8_t num) {
    gFactoryInfo.iot.IotType[0] = 'I';
    gFactoryInfo.iot.IotType[1] = 'O';
    gFactoryInfo.iot.IotType[2] = 'T';
    gFactoryInfo.iot.SafetyCode = 123456; /*安全码*/
    gFactoryInfo.iot.IotSn = 2022021600999 + 1000 * num;
    WriteIotFactoryInfo();

    return 1;
}

uint64_t get_IotSn(void) {
    return gFactoryInfo.iot.IotSn;
}

uint16_t Reversal(uint16_t data) {
    uint16_t temp = 0;
    temp = (data >> 8) | (data << 8);
    return temp;
}

uint16_t ntohs(uint16_t src) {
    return ((src << 8) | (src >> 8));
}

void LsbtoMsb(uint8_t *dst, uint8_t *src, uint16_t len) {
    uint16_t i;
    if (len < 1 || !dst || !src) {
        return;
    }

    for (i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

uint64_t ResShort2long(uint16_t data[4]) {
    uint64_t temp = 0;
    temp |= (uint64_t)(data[0]) << 48; // MSB
    temp |= (uint64_t)(data[1]) << 32;
    temp |= (uint64_t)(data[2]) << 16;
    temp |= (uint64_t)(data[3]);
    return temp;
}

void ResLong2short(uint16_t data[4], uint64_t src) {
    data[0] = (src >> 48) & 0xFFFF; // MSB
    data[1] = (src >> 32) & 0xFFFF;
    data[2] = (src >> 16) & 0xFFFF;
    data[3] = (src) & 0xFFFF;
}

uint64_t short2long(uint16_t data[4]) {
    uint64_t temp = 0;
    temp |= (uint64_t)data[0]; // LSB
    temp |= (uint64_t)data[1] << 16;
    temp |= (uint64_t)data[2] << 32;
    temp |= (uint64_t)data[3] << 48;
    return temp;
}

void long2short(uint16_t data[4], uint64_t src) {
    data[0] = src & 0xFFFF; // LSB
    data[1] = (src >> 16) & 0xFFFF;
    data[2] = (src >> 32) & 0xFFFF;
    data[3] = (src >> 48) & 0xFFFF;
}

/*写入出厂信息*/
int WriteIotFactoryInfo(void) {
    if (gFlashLock > 30) {
        return 0;
    }

    if (!FlashErase(IOT_FACTORY_INFO_ADDRESS, IOT_FACTORY_INFO_MAX_LEN)) {
        gFlashLock++;
        return 0;
    }
    gFactoryInfo.iot.flags = 0xAABB;
    if (!FlashWriteBuffer(
            IOT_FACTORY_INFO_ADDRESS, (uint8_t *)&gFactoryInfo.iot, sizeof(gFactoryInfo.iot))) {
        gFlashLock++;
        return 0;
    }

    FlashReadBuffer(
        IOT_FACTORY_INFO_ADDRESS, (uint8_t *)&gFactoryInfo.iot, sizeof(gFactoryInfo.iot));
    long2short(gModbusIotRegs.IotDataStruct.DeviceSecurityCode, gFactoryInfo.iot.SafetyCode);
    long2short(gModbusIotRegs.IotDataStruct.IotSN, gFactoryInfo.iot.IotSn);
    ResLong2short(gModbusFactortSetRegs.FactoryInfo.IotSn, gFactoryInfo.iot.IotSn);
    ResLong2short(gModbusFactortSetRegs.FactoryInfo.SafetyCode, gFactoryInfo.iot.SafetyCode);
    gFlashLock = 0;
    return 1;
}

/*写入出厂信息到nvs*/
int WriteIotFactoryInfo_to_nvs(void) {
    // esp_err_t err;
    // nvs_handle_t iot_factory_info_handle;
    // gFactoryInfo.iot.flags = 0xAABB;

    // err = nvs_open("iot_factory_info", NVS_READWRITE, &iot_factory_info_handle);
    // if (err != ESP_OK) {
    //     ESP_LOGI(TAG, "nvs_open fail!");
    // } else {
    //     // ESP_LOGI(TAG, "nvs_open Success!");
    // }
    // err = nvs_set_blob(
    //     iot_factory_info_handle,
    //     "iot_factory_info",
    //     (uint8_t *)&gFactoryInfo.iot,
    //     sizeof(gFactoryInfo.iot)
    // );

    // err = nvs_commit(iot_factory_info_handle); /*提交*/
    // if (err != ESP_OK) {
    //     // ESP_LOGI(TAG, "write NVS Success!");
    // }
    // nvs_close(iot_factory_info_handle);
    return 1;
}

/*从nvs读取设备出厂信息*/
int ReadIotFactoryInfo_from_nvs(void) {
    // esp_err_t err;
    // nvs_handle_t iot_factory_info_handle;

    // size_t required_size = 0;

    // size_t len = 32;

    // err = nvs_open("iot_factory_info", NVS_READONLY, &iot_factory_info_handle);
    // if (err != ESP_OK) {
    //     ESP_LOGI(TAG, "nvs_open fail!");
    // } else {
    //     // ESP_LOGI(TAG, "nvs_open Success!");
    // }
    // ESP_ERROR_CHECK(err);

    // err = nvs_get_blob(iot_factory_info_handle, "iot_factory_info", NULL, &required_size);
    // /*读取获取长度*/

    // err = nvs_get_blob(
    //     iot_factory_info_handle, "iot_factory_info", (uint8_t *)&gFactoryInfo.iot, &required_size
    // );

    // if (err != ESP_OK) {
    //     ESP_LOGI(TAG, "read NVS fail!");
    // } else {
    //     // ESP_LOGI(TAG, "read NVS Success!");
    // }
    // nvs_close(iot_factory_info_handle);
    // if (gFactoryInfo.iot.flags != 0xAABB) /*判断标志位，如果没有数据，则写入默认数据*/
    // {
    //     memset(gFactoryInfo.iot.IotType, 0, sizeof(gFactoryInfo.iot.IotType));
    //     gFactoryInfo.iot.IotType[0] = 'I';
    //     gFactoryInfo.iot.IotType[1] = 'O';
    //     gFactoryInfo.iot.IotType[2] = 'T';
    //     gFactoryInfo.iot.IotSn = 1;
    //     gFactoryInfo.iot.SafetyCode = 0;
    //     ESP_LOGE(TAG, "no factory_info"); /*打印DeviceSn*/
    // }

    return 1;
}

int ReadIotFactoryInfo(void) {
    /*读取iot工厂信息，包括IOT类型、Iotsn码和IOT安全码*/
    FlashReadBuffer(
        IOT_FACTORY_INFO_ADDRESS, (uint8_t *)&gFactoryInfo.iot, sizeof(gFactoryInfo.iot));
    if (gFactoryInfo.iot.flags != 0xAABB) { /*判断标志位，如果没有数据，则写入默认数据*/
        memset(gFactoryInfo.iot.IotType, 0, sizeof(gFactoryInfo.iot.IotType));
        gFactoryInfo.iot.IotType[0] = 'I';
        gFactoryInfo.iot.IotType[1] = 'O';
        gFactoryInfo.iot.IotType[2] = 'T';
        gFactoryInfo.iot.IotSn = 1;
        gFactoryInfo.iot.SafetyCode = 0;
        // ESP_LOGE(TAG, "no factory_info"); /*打印DeviceSn*/
    }

    return 1;
}

/*读取设备信息*/
int ReadPowerDeviceInfo(void) {
    uint16_t dummyAddress = 0xFFFF;
    uint8_t infoStructSize = sizeof(gFactoryInfo.device);
    uint32_t startAddress = POWER_FACTORY_INFO_ADDRESS;
    uint32_t endAddress
        = startAddress + (POWER_FACTORY_INFO_MAX_LEN / infoStructSize) * infoStructSize;

    while (endAddress > startAddress) {
        if (FlashReadBuffer((endAddress - 2), (uint8_t *)&dummyAddress, 2)) {
            endAddress -= infoStructSize;
            if (dummyAddress != 0xAABB) {
                continue;
            }

            if (FlashReadBuffer(endAddress, (uint8_t *)&gFactoryInfo.device, infoStructSize)) {
                return 1;
            }
        }
    }

    memset((char *)&gFactoryInfo.device, 0, infoStructSize);
    return 0;
}

/*写入设备信息*/
int WritePowerDeviceInfo(void) {
    uint8_t infoStructSize = sizeof(DeviceFactory);
    uint8_t buf[infoStructSize];
    uint8_t i;
    uint32_t startAddress = POWER_FACTORY_INFO_ADDRESS;
    uint32_t endAddress
        = startAddress + (POWER_FACTORY_INFO_MAX_LEN / infoStructSize) * infoStructSize;

    if (gFlashLock > 30) {
        return 0;
    }

    while (startAddress < endAddress) {
        if (!FlashReadBuffer(startAddress, buf, infoStructSize)) { // read flash
            gFlashLock++;
            return 0;
        }

        for (i = 0; i < infoStructSize; i++) { // Check if flash can write
            if (buf[i] != 0xFF) {
                break;
            }
        }

        if (i == infoStructSize) { // can write
            break;
        }
        startAddress += infoStructSize; // connot wirte and check next segment
    }

    if (startAddress == endAddress) { // flash is write full
        startAddress = POWER_FACTORY_INFO_ADDRESS;
        if (!FlashErase(startAddress, POWER_FACTORY_INFO_MAX_LEN)) { // erase old data
            gFlashLock++;
            return 0;
        }
    }

    // write new data
    gFactoryInfo.device.flags = 0xAABB;
    if (FlashWriteBuffer(startAddress, (uint8_t *)&gFactoryInfo.device, infoStructSize)) {
        // memset((char *)&gFactoryInfo.device, 0, sizeof(infoStructSize));
        FlashReadBuffer(startAddress, (uint8_t *)&gFactoryInfo.device, infoStructSize);
        if (gFactoryInfo.device.flags != 0xAABB) {
            gFlashLock++;
            return 0;
        }
    }
    return 1;
}

/*获取下游设备的DeviceType和DeviceSn，检测是否需要更新信息
在出厂时会写入iot的type和sn码，此函数用于检测是否需要将信息写入到内部flash保存
*/
int IotFactoryConfigCheck(void) {
    uint64_t temp = 0;
    uint8_t write = 0;

    temp = ResShort2long(gModbusFactortSetRegs.FactoryInfo.IotSn);
    // ESP_LOGE(TAG, "FactoryInfo.IotSn:%lld",temp); /*打印DeviceSn*/
    if (temp && temp != gFactoryInfo.iot.IotSn) {
        gFactoryInfo.iot.IotSn = temp;
        write = 1;
    }

    // IOT Security code
    temp = ResShort2long(gModbusFactortSetRegs.FactoryInfo.SafetyCode);
    // ESP_LOGE(TAG, "FactoryInfo.SafetyCode:%lld",temp); /*打印DeviceSn*/
    if (temp && (temp != gFactoryInfo.iot.SafetyCode)) {
        gFactoryInfo.iot.SafetyCode = temp;
        write = 1;
    }

    return write;
}

/*获取下游设备的DeviceType和DeviceSn，检测是否需要更新信息
比如：某款机器的DeviceType为EP500，DeviceSn为2101000003790*/
int PowerDeviceConfigCheck(void) {
    uint64_t temp = 0;
    uint8_t write = 0;

    // ESP_LOGE(TAG, "DataStruct.StaticData.DeviceType:%s",(char
    // *)gModbusDataRegs.DataStruct.StaticData.DeviceType);
    //  product model (example EP500)
    if (strlen((const char *)gModbusDataRegs.DataStruct.StaticData.DeviceType)) {
        for (uint8_t i = 0, j = 0; i < DEVICE_TYPE_SRTING_LENGTH; i += 2, j++) {
            // ESP_LOGE(TAG, "temp[%d]:%lld",i,temp);
            temp = ntohs(*(uint16_t *)&gFactoryInfo.device.DeviceType[i]);
            if (gModbusDataRegs.DataStruct.StaticData.DeviceType[j] != temp) {
                *(uint16_t *)&gFactoryInfo.device.DeviceType[i]
                    = ntohs(gModbusDataRegs.DataStruct.StaticData.DeviceType[j]);
                write = 1;
            }
        }
        // ESP_LOGE(TAG, "DeviceType:%s",gFactoryInfo.device.DeviceType);
    }

    // product SN code
    temp = short2long(gModbusDataRegs.DataStruct.StaticData.DeviceSNCode);
    if (temp && temp != gFactoryInfo.device.DeviceSn) { /*判断是否需要修改DeviceSn*/
        gFactoryInfo.device.DeviceSn = temp;
        write = 1; /*需要写入信息更新*/
        // ESP_LOGE(TAG, "DeviceSn:%lld",gFactoryInfo.device.DeviceSn); /*打印DeviceSn*/
    }
    return write;
}
