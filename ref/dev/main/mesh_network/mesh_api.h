#ifndef _MESH_COMMON_H_
#define _MESH_COMMON_H_

#include "lwip/sockets.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "dev_discovery.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Wlan Mesh supported define
//#define CONFIG_BLUETTI_WLAN_MESH_SUPPORTED   1

#define CONFIG_MESH_LOG_LEVEL ESP_LOG_INFO

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
// #define MESH_SELF_TEST
// mesh支持设备发现功能开关
#define CONFIG_MESH_DEV_DISCOVERY_SUPPORTED 1
// mesh支持固件升级功能开关
#define CONFIG_MESH_UPGRADE_SUPPORTED       1
// mesh使用SPI RAM开关
#define CONFIG_MESH_USE_SPI_RAM             1
#endif



/*
局域网设备发现，设备优先级
初步定义建议值，数值越大，优先级越高；数值间隙预留

*/
#define DEV_PRIORITY_INVALID        0//0-无效


#define DEV_PRIORITY_MIN_VALID      1//1-最低有效优先级

#define DEV_PRIORITY_AC380          30350//AC380 IOT

#define DEV_PRIORITY_AT1            30300//AT1(ATS):30300
#define DEV_PRIORITY_EMS            30200//EMS:30200
#define DEV_PRIORITY_COMBOX         30100//COMBOX:30100
#define DEV_PRIORITY_D100S          30000//D100S:30000
#define DEV_PRIORITY_A100           20900//A80/A100:20900
#define DEV_PRIORITY                65535//超级强制主设备-65535（只允许特殊短暂发送）

#define	MESH_FRAME_HEADER_COMMON	0//默认的普通无线设备间报文
#define	MESH_FRAME_HEADER_TRIGER	1//	1-设备发现触发帧（组播,STEP1）
#define	MESH_FRAME_HEADER_SEND_SN	2//	2-设备发现SN信息上报帧（组播,STEP2）
#define	MESH_FRAME_HEADER_FINISH	3//	3-设备发现完成帧（组播,STEP3）
#define	MESH_FRAME_HEADER_FINISH_AFTER	4//设备发现完成后，进入定时周期上报模式，上报自身SN
#define MESH_FRAME_HEADER_WIFI_MESH_BROADCAST        5//5-设备发现SN信息周期上报帧（组播，WIFI AP/STA模式MESH，多AP网络）
#define MESH_FRAME_HEADER_WIFI_MESH_SINGLE        6//6-WIFI AP/STA模式单播

//#define	MESH_FRAME_VERSION_WIFI		1//
//#define	MESH_FRAME_VERSION_SUB1GHZ	2//


//无线设备间协议版本类型
#define	MESH_VERSION_TYPE_WIFI	    1//1-WIFI
#define	MESH_VERSION_TYPE_SUB1GHZ	2//2-SUB1GHZ；
#define MESH_VERSION_TYPE_WIFI_MESH        3//3-WIFI AP/STA多AP网络



//无线设备间协议报文类型
#define	MESH_FRAME_TYPE_READ	1//1-读取；
#define	MESH_FRAME_TYPE_WRITE	2//2-写入；
#define	MESH_FRAME_TYPE_READ_RTN	3//3-读取-RTN；
#define	MESH_FRAME_TYPE_WRITE_RTN	4//4-写入-RTN；
#define	MESH_FRAME_TYPE_PERIOD	5//5-周期上报（无RTN），固定使用0x10多字节写
#define	MESH_FRAME_TYPE_HEART	6//6-周期上报（心跳）
#define	MESH_FRAME_TYPE_XMODEM	7//7-上级发xmodem升级相关
#define	MESH_FRAME_TYPE_XMODEM_RTN	8//8-下级响应xmodem升级相关

//公司产品条码的ASCII内容的代号-序号;配合绑定贞21000

// #define SN_TYPE_AC380_PLP023   	6//

// #define SN_TYPE_EP600    	1000//
// #define SN_TYPE_A80    		2000//
// #define SN_TYPE_D100S    	2001//
// #define SN_TYPE_A100_Sub1G    2002//
// #define SN_TYPE_A100_WIFI    2003//
// #define SN_TYPE_D400S   	 2005//


// #define SN_TYPE_S1   		3000//
// #define SN_TYPE_AT1   		3001//
// #define SN_TYPE_COMBOX      3002//
// #define SN_TYPE_PBOX   		3003//
// #define SN_TYPE_EBOX   		3004//

// #define SN_TYPE_SELF      SN_TYPE_AC380_PLP023//自己的 设备 SN type



//////////
//WIFI UDP: 无线设备间协议报文寄存器地址序号
#define	WIFI_UDP_FRAME_ADDR_HEAD		0//
#define	WIFI_UDP_FRAME_ADDR_VER			1//
#define	WIFI_UDP_FRAME_ADDR_SN_SOURCE	2//MAC_源设备
#define	WIFI_UDP_FRAME_ADDR_TYPE_SOURCE	10//

#define	WIFI_UDP_FRAME_ADDR_SN_TARGET	12//MAC_目标设备
#define	WIFI_UDP_FRAME_ADDR_TYPE_TARGET	20//

#define	WIFI_UDP_FRAME_ADDR_TYPE		22//
#define	WIFI_UDP_FRAME_ADDR_TTL			23//

#define	WIFI_UDP_FRAME_ADDR_MODBUS_LEN	25//
#define	WIFI_UDP_FRAME_ADDR_MODBUS_HEAD	27//

#define DEBUG_WIFI_MESH_MASTER_ENABLE	//使能：AC380;禁止：S1


#ifdef DEBUG_WIFI_MESH_MASTER_ENABLE
// 机型序号(源设备) 详细可参考《无线设备间（如mesh）应用层通用协议》
#define SELF_DEV_TYPE                       SN_TYPE_SELF     //微逆DTU_Sub1G
#define MESH_DEV_PRIORITY                   30100
#define MESH_DEV_PARALLEL_SEQ               0
#else
#define SELF_DEV_TYPE                       SN_TYPE_S1     // 
#define MESH_DEV_PRIORITY                   DEV_PRIORITY_MIN_VALID
#define MESH_DEV_PARALLEL_SEQ               0
#endif

/** 网卡Key，通过此名称，可获取到网卡*/
#define NETIF_KEY_WIFI_STA                  "WIFI_STA_DEF"
#define NETIF_KEY_WIFI_AP                   "WIFI_AP_DEF"

//TODO: modify by debug
#define MESH_AP_SSID                        "Bluetti_Mesh_YJH"
#define MESH_AP_PASSWORD                    "esp32s31"
#define MESH_DEPTH                          (10)    // mesh网络层级
#define MESH_AP_MAX_CONNECT                 (4)     // AP最大连接数
#define MESH_NODE_MAX_COUNT                 (30)    // mesh网络最大节点数

/** Mesh任务相关时间参数, 单位是iot_mesh_task()调用的时间间隔 */
#define MESH_TASK_PERIOD                    (50)    //50ms
#define MESH_HEARTBEAT_INTERVAL             (30000/MESH_TASK_PERIOD)   // 30s
#define MESH_HEARTBEAT_CHECK_INTERVAL       (200/MESH_TASK_PERIOD)     // 200ms
#define MESH_HEARTBEAT_TIMEOUT              (90)    // 90s
#define MESH_NODE_INFO_SYNC_TIMEOUT         (300)   // 3s

/** 黑名单 */
#define NODE_BLACKLIST_COUNT                (30)    // 黑名单最大数量
#define MESH_BLACKLIST_TIMEOUT              (120)   // 黑名单超时时间: 120s

// AP联网失败重试次数，超次数后重新扫描新的节点
#define MAX_OVER_LIMIT_ERR_CNT              (5)

//#define MESH_VERSION_TYPE_WIFI_MESH     (3 )    // 无线设备间协议版本

#define MESH_WIFI_AP_CHANNEL                (0)
#define MESH_WIFI_STA_CHANNEL               MESH_WIFI_AP_CHANNEL

typedef void (*mesh_node_change_cb_t)(int16_t, uint8_t, uint64_t, uint16_t, uint16_t, uint8_t);

typedef enum _mesh_node_type_t
{
    MESH_NODE_TYPE_MIN = 0,
    MESH_NODE_TYPE_COMMON = MESH_NODE_TYPE_MIN,         /**< 普通节点 */
    MESH_NODE_TYPE_ROOT,                                /**< 根节点 */
    MESH_NODE_TYPE_MAX = MESH_NODE_TYPE_ROOT
} mesh_node_type_t;

enum
{
    MESH_NODE_STATUS_OFFLINE = 0,
    MESH_NODE_STATUS_ONLINE,
};

#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
void init_mesh_config(uint64_t sn, uint16_t dev_type,uint16_t priority,uint8_t parallel_seq,uint16_t node_type);
void iot_mesh_task(void);
int init_mesh_ap_config(wifi_ap_config_t *ap_config);
void init_mesh_scan_config(void);
int set_ip_info_ap_netif(void);
bool is_root_node(void);
void mesh_event_sta_connected(esp_netif_ip_info_t ip_info);
void mesh_event_sta_disconnected(int over_limit_cnt);
void mesh_event_ap_start(void);
void mesh_event_ap_stop(void);
void print_mesh_tree(void);
void print_mesh_debug(void);
wifi_scan_config_t *get_wifi_scan_config(void);
int wifi_scan_handler(const wifi_event_sta_scan_done_t *scan_result, uint8_t *auth_mod,
                        uint8_t *ssid, uint8_t *bssid, uint8_t *pwd);
int send_wifi_mesh_data(uint64_t dst_sn, uint16_t dst_type, uint8_t *data, uint16_t len);
int recv_wifi_mesh_data(uint8_t *data, uint16_t size);
uint16_t get_mesh_node_type(void);
uint16_t get_current_mesh_node_info(uint8_t *out_buf);
void set_mesh_node_type(uint16_t node_type);
void register_mesh_node_change_cb(mesh_node_change_cb_t *cb);
int16_t get_mesh_node_cnt(uint16_t model);
uint8_t *wifi_mesh_get_node_SN(uint8_t index);
uint8_t wifi_mesh_get_node_modbus_slave_address(uint8_t index);
uint8_t wifi_mesh_set_node_modbus_slave_address(uint8_t index,uint8_t address);
#else
static inline void init_mesh_config(uint64_t sn, uint16_t dev_type,uint16_t priority,uint8_t parallel_seq,uint16_t node_type) {}
static inline void iot_mesh_task(void) {}
static inline int init_mesh_ap_config(wifi_ap_config_t *ap_config) { return 0; }
static inline void init_mesh_scan_config(void) {}
static inline int set_ip_info_ap_netif(void) { return 0; }
static inline bool is_root_node(void) { return false; }
static inline void mesh_event_sta_connected(esp_netif_ip_info_t ip_info) {}
static inline void mesh_event_sta_disconnected(int over_limit_cnt) {}
static inline void mesh_event_ap_start(void) {}
static inline void mesh_event_ap_stop(void) {}
static inline void print_mesh_tree(void) {}
static inline void print_mesh_debug(void) {}
static inline wifi_scan_config_t *get_wifi_scan_config(void) { return NULL; }
static inline int wifi_scan_handler(const wifi_event_sta_scan_done_t *scan_result, uint8_t *auth_mod,
                        uint8_t *ssid, uint8_t *bssid, uint8_t *pwd) { return 0; }
static inline int send_wifi_mesh_data(uint64_t dst_sn, uint16_t dst_type, uint8_t *data, uint16_t len) { return 0; }
static inline int recv_wifi_mesh_data(uint8_t *data, uint16_t size) { return 0; }
static inline uint16_t get_mesh_node_type(void) { return 0; }
static inline uint16_t get_current_mesh_node_info(uint8_t *out_buf) { return 0; }
static inline void set_mesh_node_type(uint16_t node_type) {}
static inline void register_mesh_node_change_cb(mesh_node_change_cb_t *cb) {}
static inline int16_t get_mesh_node_cnt(uint16_t model) { return 0; }
static inline uint8_t *wifi_mesh_get_node_SN(uint8_t index) { return NULL; }
static inline uint8_t wifi_mesh_get_node_modbus_slave_address(uint8_t index) { return 0; }
static inline uint8_t wifi_mesh_set_node_modbus_slave_address(uint8_t index,uint8_t address) { return 0; }
#endif

#ifdef MESH_SELF_TEST
void iot_mesh_test_task(void *param);
void mesh_self_test_add_child_node(uint32_t ip);
#endif

#ifdef __cplusplus
}
#endif

#endif