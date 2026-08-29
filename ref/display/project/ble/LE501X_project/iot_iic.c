#define LOG_TAG "IOT_IIC"

#include "compile_flag.h"
#include "ls_hal_i2c.h"
#include "ls_soc_gpio.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "ls_hal_timer.h"

#define I2C_SPEED I2C_SPEED_NORMAL_100K
// #define I2C_SPEED I2C_SPEED_FAST_400K
#define I2C_ADDRESS (0x7C)
#define BUFFER_LEN 255

static I2C_HandleTypeDef I2cHandle;

// 添加I2C状态检查函数
// void iot_i2c_check_status(void) {
//     HAL_I2C_StateTypeDef state = HAL_I2C_GetState(&I2cHandle);
//     HAL_I2C_ModeTypeDef mode = HAL_I2C_GetMode(&I2cHandle);
//     uint32_t error = HAL_I2C_GetError(&I2cHandle);

//     LOG_I("I2C Status - State: %d, Mode: %d, Error: 0x%08X",
//           state,
//           mode,
//           error);
// }

// 添加I2C设备扫描功能
// void iot_i2c_scan_devices(void) {
//     LOG_I("Scanning I2C devices...");
//     uint8_t found_count = 0;
//     uint8_t dummy_data = 0x00; // 添加一个虚拟数据字节

//     for (uint8_t addr = 1; addr < 128; addr++) {
//         // 尝试发送一个字节的虚拟数据来检测设备
//         HAL_StatusTypeDef result = HAL_I2C_Master_Transmit(&I2cHandle, addr, &dummy_data, 1,
//         100);

//         if (result == HAL_OK) {
//             LOG_I("Found device at address 0x%02X", addr);
//             found_count++;
//         }

//         // 在每次扫描之间添加小延时，避免总线过于繁忙
//         DELAY_US(100);
//     }

//     if (found_count == 0) {
//         LOG_W("No I2C devices found!");
//     } else {
//         LOG_I("Found %d I2C device(s)", found_count);
//     }
// }

void iot_i2c_init(void) {
    pinmux_iic2_init(PB15, PB13);
    I2cHandle.Instance = I2C2;
    I2cHandle.Init.ClockSpeed = I2C_SPEED;
    I2cHandle.Init.OwnAddress1 = I2C_ADDRESS;
    I2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    I2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    I2cHandle.Init.OwnAddress2 = 0x7E;

    if (HAL_I2C_Init(&I2cHandle) != HAL_OK) {
        LOG_E("IIC init failed");
    } else {
        // LOG_I("IIC init success");

        // 添加短暂延时，确保I2C硬件完全初始化
        // DELAY_US(1000);

        // 检查初始状态
        // iot_i2c_check_status();

        // 扫描设备（可选，用于调试）
        // iot_i2c_scan_devices();
    }
}

void iot_i2c_deinit(void) {
    HAL_I2C_DeInit(&I2cHandle);
    pinmux_iic2_deinit();
}

// 阻塞式版本的I2C读写函数
int iot_i2c_write_blocking(uint16_t i2c_addr, uint8_t *data, uint16_t len) {
    // LOG_D("I2C blocking write start: addr=0x%02X, len=%d", i2c_addr, len);

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&I2cHandle, i2c_addr, data, len, 1000);

    if (status == HAL_OK) {
        // LOG_D("I2C blocking write completed successfully");
        return 0;
    } else {
        uint32_t error = HAL_I2C_GetError(&I2cHandle);
        // LOG_E("I2C blocking write failed: status=%d, error=0x%08X", status, error);
        return -1;
    }
}

int iot_i2c_read_blocking(uint16_t i2c_addr, uint8_t *data, uint16_t len) {
    // LOG_D("I2C blocking read start: addr=0x%02X, len=%d", i2c_addr, len);

    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&I2cHandle, i2c_addr, data, len, 1000);

    if (status == HAL_OK) {
        // LOG_D("I2C blocking read completed successfully");
        return 0;
    } else {
        uint32_t error = HAL_I2C_GetError(&I2cHandle);
        LOG_E("I2C blocking read failed: status=%d, error=0x%08X", status, error);
        return -1;
    }
}

// 阻塞式寄存器读取函数
int iot_i2c_reg_read(
    uint16_t i2c_addr, uint16_t reg_addr, uint8_t reg_addr_size, uint8_t *data, uint16_t len) {
    uint8_t addr_buf[2];

    // 参数检查
    if (reg_addr_size > 2 || data == NULL) {
        LOG_E("Invalid parameter");
        return -1;
    }

    // 构建地址缓冲区
    if (reg_addr_size == 1) {
        addr_buf[0] = (uint8_t)reg_addr;
    } else {
        addr_buf[0] = (uint8_t)(reg_addr >> 8);
        addr_buf[1] = (uint8_t)(reg_addr & 0xFF);
    }

    // 步骤1：写入寄存器地址
    if (iot_i2c_write_blocking(i2c_addr, addr_buf, reg_addr_size) != 0) {
        LOG_E("Failed to write register address");
        return -1;
    }

    // 步骤2：读取数据
    if (iot_i2c_read_blocking(i2c_addr, data, len) != 0) {
        LOG_E("Failed to read register data");
        return -1;
    }

    return 0;
}

// 阻塞式寄存器写入函数
int iot_i2c_reg_write(
    uint16_t i2c_addr, uint16_t reg_addr, uint8_t reg_addr_size, uint8_t *data, uint16_t len) {
    uint8_t buffer[BUFFER_LEN]; // 使用足够大的缓冲区
    uint16_t total_len = 0;

    // 参数检查
    if (reg_addr_size > 2 || data == NULL || (reg_addr_size + len) > BUFFER_LEN) {
        LOG_E("Invalid parameter: reg_addr_size=%d, len=%d, buffer_size=%d",
              reg_addr_size,
              len,
              BUFFER_LEN);
        return -1;
    }

    // 构建完整的传输缓冲区：寄存器地址 + 数据
    if (reg_addr_size == 1) {
        buffer[0] = (uint8_t)reg_addr;
    } else {
        buffer[0] = (uint8_t)(reg_addr >> 8);
        buffer[1] = (uint8_t)(reg_addr & 0xFF);
    }

    // 复制数据到缓冲区
    memcpy(&buffer[reg_addr_size], data, len);
    total_len = reg_addr_size + len;

    // 一次性传输：寄存器地址 + 数据
    HAL_StatusTypeDef status
        = HAL_I2C_Master_Transmit(&I2cHandle, i2c_addr, buffer, total_len, 1000);

    if (status != HAL_OK) {
        uint32_t error = HAL_I2C_GetError(&I2cHandle);
        LOG_E("I2C register write failed: addr=0x%02X, reg=0x%02X, status=%d, error=0x%08X",
              i2c_addr,
              reg_addr,
              status,
              error);
        return -1;
    }
    return 0;
}
