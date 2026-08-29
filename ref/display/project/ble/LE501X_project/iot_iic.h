#ifndef __IOT__IIC_H__
#define __IOT__IIC_H__

#include <stdint.h>

void iot_i2c_init(void);
void iot_i2c_deinit(void);

int iot_i2c_write_blocking(uint16_t i2c_addr, uint8_t *data, uint16_t len);
int iot_i2c_read_blocking(uint16_t i2c_addr, uint8_t *data, uint16_t len);
/**
 * @brief 从I2C设备的指定寄存器读取数据
 *
 * @param i2c_addr I2C设备地址
 * @param reg_addr 寄存器地址
 * @param reg_addr_size 寄存器地址大小（字节数）
 * @param data 接收数据的缓冲区
 * @param len 要读取的数据长度
 * @return int 0表示成功，非0表示错误
 */
int iot_i2c_reg_read(
    uint16_t i2c_addr, uint16_t reg_addr, uint8_t reg_addr_size, uint8_t *data, uint16_t len);

/**
 * @brief 向I2C设备的指定寄存器写入数据
 *
 * @param i2c_addr I2C设备地址
 * @param reg_addr 寄存器地址
 * @param reg_addr_size 寄存器地址大小（字节数）
 * @param data 要写入的数据
 * @param len 要写入的数据长度
 * @return int 0表示成功，非0表示错误
 */
int iot_i2c_reg_write(
    uint16_t i2c_addr, uint16_t reg_addr, uint8_t reg_addr_size, uint8_t *data, uint16_t len);

#endif // __IOT__IIC_H__
