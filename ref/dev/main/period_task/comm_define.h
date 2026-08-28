#ifndef __COMM_DEFINE_H__
#define __COMM_DEFINE_H__
#include <stdint.h>
//#include "esp_log.h"
#include "esp_event.h"//EXT_RAM_BSS_ATTR 需要
//#include "esp_smartconfig.h"



#include "app_uart.h"

#include <time.h>
#include <sys/time.h>




// #define     TCP_ENCRYPT_ENABLE//使能网络加密，兼容网络加密和不加密两种服务器交互

#define ENCRYPT_CERT_USE_FILE_SYSTEM      1   //加密证书保存到文件系统，否则保存到分区
// #define ENCRYPT_CERT_ENCRYPT_SAVE         1   // 加密证书加密存储


//#define     TCP2_ENCRYPT_ENABLE//第二网络服务器的加密使能
#define     TCP_SERVER2_ENABLE//第二网络服务器(公开)通讯使能

#define     CAN_PORT_ENABLE//inv can下级设备
//#define     SUB_1GHZ_PORT_ENABLE//sub 1GHz微逆使能,AC380无此，ATS有此

#define     KWH_YEAR_REPORT_MODBUS_ENABLE//本地存储的基于年的kwh电量，通过新modbus beta上报
#define     KWH_YEAR_REPORT_DATA_CHANNEL_ENABLE//本地存储的基于年的kwh电量，通过新data channel上报

// #define     FORCE_BLE_WIFI_ON_ENABLE//AC380默认开启需要仅主机开启，为调试，要屏蔽逻辑强制开启BLE/WIFI


// #define     DEBUG_PRINTF_ENABLE//debug 调试打印



//#define CONFIG_HARDWARE_ATS	//二选一，配电柜
//#define CONFIG_HARDWARE_DTU100	//二选一,COM box 通讯盒

#define CONFIG_ENABLE_UNIVERSAL_DATA_TRANSMISSION
#define CONFIG_ENABLE_HTTP_OTA

#define CONFIG_ENABLE_MD_TCP_SLAVE

#define MODBUS_VERSION 2021//modbus beta

//二者并存
#define	DTU_MICRO_INV_UART//微逆DTU用uart透传
#define	ATS_INV_CAN//ATS 用CAN透传

#define TX_LEN 1000
#define NET_SUB1G_MAX_POINT 0//32//最大网络节点数量,Sub 1GHz网络,AC380无 sub 1ghz
#define	NET_WIFI_INVBAT_POINT		    (uint16_t)100//普通储能逆变器数量,协议预留
#define	NET_WIFI_INVBAT_POINT_MINI	    (uint16_t)10//普通储能逆变器数量,设备实际预留


#define NET_WIFI_S1_POINT 16//S1点位
#define NET_WIFI_METER_POINT 3
#define NET_WIFI_MIX_POINT 20//除S1外其他 WIFI点位
#define NET_WIFI_MAX_POINT (NET_WIFI_INVBAT_POINT_MINI + NET_WIFI_S1_POINT + NET_WIFI_MIX_POINT)//最大网络节点数量,WIFI网络


#define BIND_ARRAY_MAX 5//绑定组最大数量
#define BIND_POINT_IN1ARRAY_MAX 5//绑定单组内最大节点数量

#define CAN_INV_MAX_POINT 10//CAN INV并机最多数量


#define  TIME_BASE_1S_CNT_10S	10//10s
#define  TIME_BASE_1S_CNT_2S	2//
#define  TIME_BASE_1S_CNT_60S	60//60s


#define TYPE_SIZE 			12									// 类型号大小
#define  TIME_BASE_100MS_CNT_10S	100//10s



#define TEST_MODE_OLDING	//0xAA:使能老化模式
#define TEST_MODE_NORMAL	//0x55:进入正常模式
#define TEST_MODE_BOARD_TEST	//0x40:进入单板测试模式
#define TEST_MODE_REVD	//0xA1:预留
#define TEST_MODE_RESET	//0xA2:重启

/*
modbus slave address地址分配：
0~100：储能 INV + BAT
S1部分：101~116
除S1外其他 WIFI点位部分:117~117+19
*/
/*HTTP我司服务器获取时间使能*/
#define TIME_GET_FROM_SERVER_BY_HTTP_ENABLE

#define HTTP_UTC_TIME_VERSION 2

/*三WIFI自切换逻辑使能*/
#define CONFIG_MORE_WIFI_STA_LINK_ENABLE


#define	MODBUS_SLAVE_ADDR_WIFI_INVBAT_START		(1)//

#define	MODBUS_SLAVE_ADDR_WIFI_START		    (uint16_t)101//Start

//S1部分：101~116
#define	MODBUS_SLAVE_ADDR_WIFI_S1_START		(MODBUS_SLAVE_ADDR_WIFI_START)//Start
#define	MODBUS_SLAVE_ADDR_WIFI_S1_END		    (MODBUS_SLAVE_ADDR_WIFI_S1_START + NET_WIFI_S1_POINT -1)//

//除S1外其他 WIFI点位部分:117~117+19
#define	MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START		(MODBUS_SLAVE_ADDR_WIFI_S1_END+1)//Start
#define	MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_END		    (MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START + NET_WIFI_MAX_POINT-NET_WIFI_S1_POINT -1)//

#define	MODBUS_SLAVE_ADDR_WIFI_TOP_START		(MODBUS_SLAVE_ADDR_WIFI_INVBAT_START)//Start
#define	MODBUS_SLAVE_ADDR_WIFI_TOP_END		    (MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_END )//


//sub 1ghz子网络;not use
//#define MODBUS_SLAVE_ADDR_MICROINV_SUB_START        (150)//Start
//#define	MODBUS_SLAVE_ADDR_MICROINV_SUB_END		    (MODBUS_SLAVE_ADDR_MICROINV_SUB_START + NET_SUB1G_MAX_POINT -1)//(uint16_t)200//End

//#define	MODBUS_SLAVE_ADDR_MICROINV_END              MODBUS_SLAVE_ADDR_MICROINV_TOP_END



#define	PARALLEL_IOT_CAN_ADDR_FROM_ARM_ENALBE	//使能表示使用量产要求的 从ARM获取排地址SN

#define BLE_ADV_KEY_DEFAULT                  "BluettiBluetooth"

/*MODBUS_TLV寄存器组合协议使能*/
#define CONFIG_MODBUS_REG_TLV_ENABLE

/* 统一量产宏定义默认值 */
// #define  MASS_PRODUCTION_CONFIG_ENABLE  //使能-量产；不使能-debug

#define MASS_PRODUCTION_ENV_HOST_URL        "iot.bluettipower.com:18760"//量产bluetti服务器地址
#define MASS_PRODUCTION_WIFI_STA_SSID		"BLUETTI"//默认wifi SSID
#define MASS_PRODUCTION_WIFI_STA_PASSWORD	"poweroak"//默认wifi 密码

#define MASS_PRODUCTION_DEFAULT_DEV_TYPE	"BLUETTI"//未标定使用的默认type ;因标定上位机读取判断原因，可忽略本行，基于具体产品更改
#define MASS_PRODUCTION_DEFAULT_DEV_SN		123456//未标定使用的默认SN
#define MASS_PRODUCTION_SET_DATA_VALID_FLAG	0xAA55//2B 本地存储配置参数区域的有效标志


#define DEV_ENV_HOST        "dev-iot.poweroak.ltd:18760"//研发服务器
#define TEST_ENV_HOST       "test-iot.poweroak.ltd:18760"
#define DEV_ENV_HOST_LAST   "dev-iot.poweroak.ltd:18760"
//#define PRO_ENV_HOST        "iot.bluettipower.com:18760"//PRO_ENV_HOST

//windy增加第二服务器+
#define	MQTT_SERVER_ADDRESS_DEFAULT "dev-iot.poweroak.ltd:18083"//MQTT公网服务器默认地址
#define	MQTT_SERVER_PASSWD_DEFAULT "admin123456"//MQTT公网服务器登录默认密码
#define CONFIG_EXAMPLE_MQTT_TEST_TOPIC2 "PLP023_US_TEST_ONLY"


#ifdef	MASS_PRODUCTION_CONFIG_ENABLE
#define BLUETTI_DNS			MASS_PRODUCTION_ENV_HOST_URL//
#define DEF_SSID			MASS_PRODUCTION_WIFI_STA_SSID//"BLUETTI"//
#define DEF_WIFI_STA_PASSWD			MASS_PRODUCTION_WIFI_STA_PASSWORD//"poweroak"//

#define CERT_HTTP_URL   "https://gw.bluettipower.com/api/midppkic/pki/84670462/update/device"
#define CERT_HTTP_URL_FIRST   "https://gw.bluettipower.com/api/midppkic/pki/84670462/apply/device"
#define CERT_HTTP_DOWN_URL "https://gw.bluettipower.com/api/midppkic/pki/84670462/download"
#else//debug
#define BLUETTI_DNS			DEV_ENV_HOST//"iot.bluettipower.com:18760"//
#define DEF_SSID			"TP-Link_DD40"//"POWEROAKHZ"//"BLUETTI"//
#define DEF_WIFI_STA_PASSWD			"esp32s31"//"88888888"//"poweroak"//

#define CERT_HTTP_URL   "http://dev-gwpry.poweroak.ltd:18888/api/midppkic/pki/84670462/update/device"
#define CERT_HTTP_URL_FIRST   "http://dev-gwpry.poweroak.ltd:18888/api/midppkic/pki/84670462/apply/device"
#define CERT_HTTP_DOWN_URL "http://dev-gwpry.poweroak.ltd:18888/api/midppkic/pki/84670462/download"
//#define DEF_SSID			"de-dev-2.4"//"TP-Link_DD40"//"POWEROAKHZ"//"BLUETTI"//
//#define DEF_WIFI_STA_PASSWD			"88888888"//"esp32s31"//"88888888"//"poweroak"//

#endif


#define DEFAULT_HOST_ADDR   { \
                                .could_dns = MASS_PRODUCTION_ENV_HOST_URL, \
                            }

#define DEFAULT_WIFI_CFG    { \
                                .wifi_sta_auth = WIFI_AUTH_WPA_WPA2_PSK, \
                                .wifi_sta_ssid = "POWEROAKHZ", \
                                .wifi_sta_password = "88888888", \
                                .wifi_sta_bssid = {0,0,0,0,0,0}, \
                            }


// 开发环境域名  http://dev-gw.poweroak.ltd:18888
// 测试环境域名  http://test-gw.poweroak.ltd:18888
// 生产环境域名  https://gw.bluettipower.com // TODO发布前更改为生产环境
#define DEV_ENV_URL         "http://dev-gw.poweroak.ltd:18888"
#define TEST_ENV_URL        "http://test-gw.poweroak.ltd:18888"
#define PRO_ENV_URL         "https://gw.bluettipower.com"

#define HTTPS_INTERFACE     "/api/blusmartprod/device/upgrade/record/v1/save"

// HTTP 请求固件版本信息列表
#define HTTPS_REQUEST_IMAGE_IF   "/api/blusmartprod/device/firmware/v1/latest/firmwareVerList/batch?gwcredentials=osUZ8ygqt1s%2FawsURLTwupGKN%2FCH8sRRODw%2FlZLlrv49jdBtu7UuRqxUkYHH6jWswlLPybPJ2WShH%2Fr928K10amSgY0pWE2%2BeeijxYovV9DIcRgwZhBbSQ%3D%3D"
#define HTTPS_OTA_STATE_IF     "/api/blusmartprod/device/upgrade/record/v1/save"

#define HTTPS_SAVE_MONEY_INTERFACE      "/api/bluiotdata/dashboard/v1/getEnergyStorageElectricCostSave"

#define HOUR_OF_SEC     2.778e-4 //1/3600*1=0.0002778
//#define HOUR_OF_SEC     2.778e-6 //1/(3600*100)=0.000002778,->0.1KWH



#define MAX_NUM_CAN_TYPE	(29)//30 2


#define DEFAULT_SALVE_ADDR		(0)//默认从机地址
#define INV_SALVE_ADDR1		(1)//逆变器从机地址1
#define INV_SALVE_ADDR2		(2)//逆变器从机地址2
#define INV_SALVE_ADDR3		(3)//逆变器从机地址3
#define PACK_SALVE_MAX_POINT 		(30)//iot电池包从机设备最大节点数量
#define ACHUB_SALVE_MAX_POINT 		(1)//iot ACHUB从机设备最大节点数量
#define DCHUB_SALVE_MAX_POINT 		(3)//iot DCHUB从机设备最大节点数量
#define INV_SALVE_MAX_POINT 		(3)//iot 逆变器从机设备最大节点数量
#define D400S_SALVE_MAX_POINT 		(3)//iot D400s从机设备最大节点数量

#define DCHUB_START_ADDR		(10)//dchub起始从机地址1
#define DCHUB_GROUP_ADDR		(DCHUB_START_ADDR)//dchub同类型设备地址
#define DCHUB_SALVE_ADDR		(DCHUB_START_ADDR+1)//dchub起始从机地址+dchub同类型设备地址

#define D400S_START_ADDR		(20)//dchub起始从机地址
#define D400S_GROUP_ADDR		(D400S_START_ADDR)//D400S同类型设备
#define D400S_SALVE_ADDR        (D400S_START_ADDR+1)//dchub起始从机地址+D400S同类型设备

#define DEFAULT_PACK_TYPE_NUM   (10)


#define POINT_SLAVE_MAX_NUM         (ACHUB_SALVE_MAX_POINT+DCHUB_SALVE_MAX_POINT+PACK_SALVE_MAX_POINT+D400S_SALVE_MAX_POINT)
#define METER_SCAN_MAX_NUM      (10) //电表扫描最大数量
#define METER_MAX_NUM           (1) //电表最大数量

/*绑定帧内单帧最大发送设备数量*/
#define BIND_READ_MAX_DEV_NUM 15


#define CAN_DATA_POLL_INDEX_MAXNUM   2

#define FUNC_PACK_NEWTYPE_CTRL

#define FUNC_ONECLICK_UPGRADE_EN

//#define INV_LOG_DETAILED_INFO_RECORD //逆变历史数据记录功能

//#define FUNC_DEV_TRIGGER_BIND   //设备自动绑定功能

//CAN总线上设备类型
enum {
    CAN_DEV_IOT=1,//
    CAN_DEV_INV ,//
	CAN_DEV_PACK ,//
	CAN_DEV_AC_HUB ,//
	CAN_DEV_DC_HUB ,//
	CAN_DEV_AT1 ,//
	CAN_DEV_EMS ,//
	CAN_DEV_D400S ,//
};


enum{
    DEVICE_IOT = 0,
    DEVICE_ARM = 1,
    DEVICE_DSP = 2,
    DEVICE_BMS = 3,
    DEVICE_PACK_BA = 4,
    DEVICE_PACK_BCU = 5,
    DEVICE_PACK_BMU = 6,
    DEVICE_PACK_BMS = 7,
    DEVICE_PACK_M1 = 8,
    DEVICE_PACK_SAFTY = 9,
    DEVICE_PACK_HV = 10,
    DEVICE_HMI1 = 11,
    DEVICE_HMI2 = 12,
    DEVICE_RF = 13,
    DEVICE_DC_HUB = 14,
    DEVICE_AC_HUB = 15,
    DEVICE_DC_DC = 16,
    MAX_DEVICE_NUN,
    DEVICE_FREE = 0xFF,
};//modbus beta 701



typedef union {
    uint16_t all;

    struct {
        int8_t hour;//低8bit：范围在-12~14（小时）
        int8_t min;//高8bit: 范围在0~59（分钟）
    };
} time_res_struct;

uint16_t	CompareSetData(uint8_t *pRAM1, uint8_t *pRAM2, uint16_t Length);

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* 返回计算后的 0..UINT16_MAX 值，作为外部返回值使用 */
#define COUNTER_DELTA_U16(curr, last) ({ \
    uint32_t _c = (uint32_t)(curr); \
    uint32_t _l = (uint32_t)(last); \
    uint32_t _d = _c - _l; /* 无符号回绕安全 */ \
    (uint16_t)((_d > UINT16_MAX) ? UINT16_MAX : _d); \
})

#endif

