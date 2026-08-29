#define LOG_TAG "CHT8325"

#include "compile_flag.h"
#include "ls_hal_i2c.h"
#include "ls_soc_gpio.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "ls_hal_timer.h"
#include "iot_iic.h"
#include "iot_timer.h"
#include "iot_magnetic_screen.h"
#include "sensor_cht8325.h"

/* I2C address */
#define CHT8325_I2C_ADDR 0x44

/* 寄存器地址长度和寄存器大小 */
#define CHT8325_REG_ADDR_LEN 0x02
#define CHT8325_REG_LEN 0x02

/* 寄存器列表 */
#define CHT8325_NIST_ID 0x3683 // 出厂ID 6字节 高字节在前
#define CHT8325_MANUFACT_ID 0x3781 // 0x5959
#define CHT8325_ONESHOT_HT 0x2c06

static iot_timer_id_t cht8325_check_timer_id = 0xFF;

/* 计算 CRC-8 的函数，多项式 POLY 0x31，初始值 0xFF */
static uint8_t calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0xFF; // 初始值

    for (size_t i = 0; i < length; i++) {
        crc ^= data[i]; // 将当前字节与 CRC 进行异或

        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31; // 多项式 0x31
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

int i2c_read_env_ht(float *temperature, float *humidity) {
    if (humidity == NULL || temperature == NULL) {
        LOG_E("Invalid parameter.");
        return -1;
    }
    uint8_t read_buff[10] = {0};
    float humi, temp;

    // 步骤1：发送测量命令 (写操作)
    uint8_t cmd_buf[2];
    cmd_buf[0] = (CHT8325_ONESHOT_HT >> 8) & 0xFF; // 高字节
    cmd_buf[1] = CHT8325_ONESHOT_HT & 0xFF; // 低字节

    if (iot_i2c_write_blocking(CHT8325_I2C_ADDR, cmd_buf, 2) != 0) {
        LOG_E("Failed to send CHT8325 measurement command");
        return -1;
    }

    // 步骤2：等待测量完成 - CHT8325需要更长时间进行ADC转换
    DELAY_US(2000); // 增加到2ms延时，确保测量完成

    // 步骤3：读取测量结果 - 6字节：温度(2) + CRC(1) + 湿度(2) + CRC(1)
    // 添加重试机制，因为传感器可能需要更多时间
    int retry_count = 0;
    int max_retries = 3;

    while (retry_count < max_retries) {
        if (iot_i2c_read_blocking(CHT8325_I2C_ADDR, read_buff, 6) == 0) {
            break; // 读取成功，跳出循环
        }

        retry_count++;
        LOG_W("CHT8325 read attempt %d failed, retrying...", retry_count);

        if (retry_count < max_retries) {
            DELAY_US(2000); // 额外等待2ms后重试
        }
    }

    if (retry_count >= max_retries) {
        LOG_E("Failed to read CHT8325 measurement data after %d attempts", max_retries);
        return -1;
    }

    // 打印原始数据用于调试
    // LOG_D("CHT8325 raw data: %02x %02x %02x %02x %02x %02x",
    //       read_buff[0],
    //       read_buff[1],
    //       read_buff[2],
    //       read_buff[3],
    //       read_buff[4],
    //       read_buff[5]);

    // CRC8 校验温度数据
    uint8_t temp_crc = calculate_crc8(read_buff, 2);
    if (temp_crc != read_buff[2]) {
        LOG_E("temp CRC8 check failed. Expected: 0x%02x, Got: 0x%02x", temp_crc, read_buff[2]);
        return -1;
    }

    // CRC8 校验湿度数据
    uint8_t humi_crc = calculate_crc8(&read_buff[3], 2);
    if (humi_crc != read_buff[5]) {
        LOG_E("humi CRC8 check failed. Expected: 0x%02x, Got: 0x%02x", humi_crc, read_buff[5]);
        return -1;
    }

    // 转换温湿度数据
    temp = ((float)((read_buff[0] << 8) | read_buff[1]) / 65535) * 175 - 45;
    humi = ((float)((read_buff[3] << 8) | read_buff[4]) / 65535) * 100;

    // LOG_I("CHT8325 Temperature: %.2f°C, Humidity: %.2f%%RH", temp, humi);

    *temperature = temp;
    *humidity = humi;
    return 0;
}

static void cht8325_check_timer_cb(void *param) {
    float temperature = 0.0;
    float humidity = 0.0;
    i2c_read_env_ht(&temperature, &humidity);
    // LOG_I("CHT8325 Temperature: %.2f°C, Humidity: %.2f%%RH", temperature, humidity);
    magnetic_screen_update_temp_humi(temperature, humidity);
}

void cht8325_check_init(void) {
    uint8_t read_buff[10] = {0};

    // LOG_I("Initializing CHT8325 sensor...");

    // 等待传感器上电稳定
    // DELAY_US(50000); // 50ms上电稳定时间

    // 读取制造商ID进行设备识别
    if (iot_i2c_reg_read(
            CHT8325_I2C_ADDR, CHT8325_MANUFACT_ID, CHT8325_REG_ADDR_LEN, read_buff, CHT8325_REG_LEN)
        != 0) {
        LOG_E("Failed to read CHT8325 manufacturer ID");
        return;
    }

    // LOG_I("CHT8325 manufacturer ID: 0x%02x%02x", read_buff[0], read_buff[1]);
    if (read_buff[0] != 0x59 || read_buff[1] != 0x59) {
        LOG_E("CHT8325 not found. Expected: 0x5959, Got: 0x%02x%02x", read_buff[0], read_buff[1]);
        return;
    }

    // 读取出厂ID
    // memset(read_buff, 0, sizeof(read_buff));
    // if (iot_i2c_reg_read(
    //         CHT8325_I2C_ADDR, CHT8325_NIST_ID, CHT8325_REG_ADDR_LEN, read_buff, CHT8325_REG_LEN *
    //         3)
    //     != 0) {
    //     LOG_E("Failed to read CHT8325 factory ID");
    //     return;
    // }

    // LOG_I("CHT8325 init success - device detected and ready");

    cht8325_check_timer_id = iot_timer_add_task_ms(1000, true, cht8325_check_timer_cb, NULL);

    return;
}

void cht8325_check_deinit() {
    if (cht8325_check_timer_id != 0xFF) {
        iot_timer_stop_task(cht8325_check_timer_id);
        iot_timer_remove_task(cht8325_check_timer_id);
        cht8325_check_timer_id = 0xFF;
    }
}

void cht8325_check_start() {
    cht8325_check_timer_cb(NULL);
    iot_timer_start_task(cht8325_check_timer_id);
}

void cht8325_check_stop() {
    iot_timer_stop_task(cht8325_check_timer_id);
}
