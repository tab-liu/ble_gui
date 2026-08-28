#ifndef FILESYSTEM_H
#define FILESYSTEM_H


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"

#define root_path  "/spiflash"
#define ota_bin_path    root_path

//#define DEV_INFO    "dev_info"

#define IOT_FACTORY     "iot_factory"//标定
#define INV_FACTORY     "inv_factory"//标定,reserved

#define NVS_IOT_SET    	"IOT_SET"//IOT自身信息存储,改到文件系统
#define NVS_INV_SET    	"INV_SET"//下级INV信息存储,改到文件系统


#ifdef	MASS_PRODUCTION_CONFIG_ENABLE
#define IOT1EPROM_READY_FLAG 	MASS_PRODUCTION_SET_DATA_VALID_FLAG
#define IOT2EPROM_READY_FLAG  		MASS_PRODUCTION_SET_DATA_VALID_FLAG

#define INV_EPROM_READY_FLAG 	MASS_PRODUCTION_SET_DATA_VALID_FLAG
#define PACK_EPROM_READY_FLAG 	MASS_PRODUCTION_SET_DATA_VALID_FLAG
#define TOURELAY_EPROM_READY_FLAG 	MASS_PRODUCTION_SET_DATA_VALID_FLAG

#else//debug
#define IOT1EPROM_READY_FLAG  		((uint16_t)0xAA0A)// 0xAABB
#define IOT2EPROM_READY_FLAG  		((uint16_t)0xAA0A)// 0xAABB

#define INV_EPROM_READY_FLAG  		((uint16_t)0xAA0A)// 0xAABB
#define PACK_EPROM_READY_FLAG  		((uint16_t)0xAA0A)// 0xAABB
#define TOURELAY_EPROM_READY_FLAG 	((uint16_t)0xAA0A)
#endif



//#define DEV_INFO    "dev_info"
//#define PASSWORD    "password"

//
#define NET_SERVER_2RD_ADDRESS    "net server 2rd address"//Net_Server_address 22005 服务器网址
#define NET_SERVER_2RD_PASSWD    "net server 2rd password"//Net_Server_secret 22069 登录服务器密码



/* 系统恢复出厂和第一次运行的默认参数值 */
#define DEV_DEFAULT_VALUE   { \
                                .dev_type = "BLUETTI",  \
                                .dev_sn = 123456, \
                                .on_off.bit.wifi_enable = 1,     \
                                .on_off.bit.wifi_sta_enable = 1, \
                                .on_off.bit.wifi_ap_enable = 1,  \
                                .on_off.bit.ble_enable = 1,  \
                                .on_off.bit.ble_mash_enable = 0, \
                                .on_off.bit._4g_enable = 1,  \
                                .on_off.bit.gps_enable = 1, \
                                .wifi_sta_auth = WIFI_AUTH_WPA2_PSK,  \
                                .wifi_sta_ssid = "BLUETTI",  \
                                .wifi_sta_password = "poweroak",  \
								.sta_enable.all = 0x1A5,	\
                                .could_dns = "iot.bluettipower.com:18760",\
								.thunder_ctrl.thunder_enable = 1,\
                            }

//  "dev.iot.poweroak.ltd:18760"      开发
//  "iot.bluettipower.com:18760"      生产
//  "10.16.16.234:18760"              调试
#define DEV_SERVER_URL     DEV_ENV_HOST
//#define DEV_SERVER_URL		"mq.tongxinmao.com:18830"// 免费开源地址 通讯猫

//


// 剩余flash空间大小为960K

#ifndef FLASH_SEC_SIZE
#define FLASH_SEC_SIZE  ((uint32_t)4096)    /**< SPI Flash sector size */
#endif
#define IOT_FIRMWARE_BEGIN_ADDRESS        ((uint32_t)0x0000u)//IOT自身固件起始地址

#define IOT_FACTORY_INFO_ADDRESS        ((uint32_t)0x0000u)
#define IOT_FACTORY_INFO_MAX_LEN        (FLASH_SEC_SIZE)  // 4Kbyte

#define IMAGE_HEAD_AREA_ADDRESS         (IOT_FACTORY_INFO_ADDRESS + IOT_FACTORY_INFO_MAX_LEN) // 4096
#define IMAGE_HEAD_AREA_MAX_LEN         (FLASH_SEC_SIZE)  // 4Kbyte

#define IMAGE_FLASH_AREA_ADDRESS         (IMAGE_HEAD_AREA_ADDRESS + IMAGE_HEAD_AREA_MAX_LEN) // 8192
#define IMAGE_FLASH_AREA_MAX_LEN         ((uint32_t)0x1C2000u) // 512Kbyte
#define IMAGE_FLASH_AREA_MAX_LEN_EXTERN         ((uint32_t)0x77000u) //    500Kbyte


#if 0
#define DEFAULT_IOT         { \
                                .type = "IOT", \
                                .version = 905307, \
                                .sn = 2023000001206, \
                                .safetyCode = 1234567, \
                                .could_dns = DEV_ENV_HOST, \
                            }
#define DEFAULT_DEVICE      { \
                                .type = "A80", \
                                .version = 905307, \
                                .sn = 2023000001206, \
                            }

#endif

typedef struct {
    char iot_type[12]; // iot出厂标定类型
    uint64_t iot_sn; // iot出厂标定sn
    uint64_t safe_code; // iot出厂标定安全随机码
    uint8_t cert_md5[48]; //三份证书的MD5值
    uint8_t nc[20]; //  保留
}factory_struct;//存储在 flash NVS
extern factory_struct iot_factory;

typedef struct
{
    char dev_type[12];    // dev出厂标定类型
    uint64_t dev_sn;      // dev出厂标定sn
    uint8_t nc[24];       //  保留
} dev_factory_struct;         // 存储在 flash NVS

extern dev_factory_struct dev_factory;

//typedef struct {
//    char inv_type[12]; // inv出厂标定类型
//    uint64_t inv_sn; // inv出厂标定sn
//    uint8_t nc[20]; //  保留
//}inv_factory_struct;//存储在 flash NVS
//extern inv_factory_struct inv_factory;


//extern const char *FileTypeString[];

extern  uint8_t TypeCnt;//const
#define JUMP_BYTES          20

void initialize_nvs(void);

esp_err_t iot_wtite_dev_info (const char *label, const uint8_t *data, size_t len);
esp_err_t iot_read_dev_info (const char *label, uint8_t *data, size_t *len);
void Read_Data_From_NVS(void);


esp_err_t iot_image_erase(uint32_t address, size_t len);
esp_err_t iot_image_read(uint32_t address, uint8_t *data, size_t len);
esp_err_t iot_image_write(uint32_t address, const uint8_t *data, size_t len);
esp_err_t iot_image_read_From_Appx_backup_Part(uint32_t address, uint8_t *data, size_t len);

esp_err_t iot_read_factory(uint8_t *data, size_t len);
esp_err_t iot_write_factory(const uint8_t *data, size_t len);

esp_err_t iot_read_flash(const char *label, uint8_t *data, size_t len);
esp_err_t iot_write_flash(const char *label, const uint8_t *data, size_t len);


/*
//大端模式获取2字节长度的数据
uint16_t  GetWordFromPtrBig( const uint8_t* uBufPoint);

//大端模式获取4字节长度的数据
uint32_t  GetDWordFromPtrBig( const uint8_t* uBufPoint);

//小端模式获取2字节长度的数据
uint16_t  GetWordFromPtrSmall( const uint8_t* uBufPoint);

//小端模式获取4字节长度的数据
uint32_t  GetDWordFromPtrSmall( const uint8_t* uBufPoint);
*/
#endif
