/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef H_ESP_CENTRAL_
#define H_ESP_CENTRAL_

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"

#include "modlog/modlog.h"

#include "comm_define.h"


#ifdef BLE_CLIENT_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

// BLE调试输出控制
#define BLE_SCAN_DEBUG_ENABLE           1       // 1-启用扫描结果详细调试输出，0-禁用
// BLE厂商ID过滤控制
#define BLE_MANUFACTURER_FILTER_ENABLE  0       // 1-启用厂商ID过滤(只添加BLUETTI设备)，0-禁用过滤(添加所有设备) 备注：不过滤，否则容易丢失设备名称


#define PEER_ADDR_VAL_SIZE                                  6
#define BLE_BROADCAST_SN_LEN                               31


#pragma pack(1)


struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

#define BLE_SVC_UUID16                                   			 0xFF00
#define BLE_SVC_CHR1_UUID16_S_TO_C                                   0xFF01
#define BLE_SVC_CHR2_UUID16_C_TO_S                                   0xFF02//APP to DEV
#define BLE_SVC_CHR3_UUID16_S_TO_C                                   0xFF03

/* 16 BIT CCCD UUID */
#define BLE_SVC_DSC_CFG_UUID16                       0x2901


/* 16 Bit Health Thermometer Service UUID */
#define BLE_SVC_HTP_UUID16                                   0x1809

/* 16 Bit Health Thermometer Service Characteristic UUIDs */
#define BLE_SVC_HTP_CHR_UUID16_TEMP_MEASUREMENT              0x2A1C
#define BLE_SVC_HTP_CHR_UUID16_TEMP_TYPE                     0x2A1D
#define BLE_SVC_HTP_CHR_UUID16_INTERMEDIATE_TEMP             0x2A1E
#define BLE_SVC_HTP_CHR_UUID16_MEASUREMENT_ITVL              0x2A21

/* 16 Bit Device Information Service Characteristic UUIDs */
#define GATT_DIS_DEVICE_INFO_UUID                            0x180A
#define GATT_DIS_CHR_UUID16_SYS_ID                           0x2A23
#define GATT_DIS_CHR_UUID16_MODEL_NO                         0x2A24
#define GATT_DIS_CHR_UUID16_MFC_NAME                         0x2A29

/* 16 BIT CCCD UUID */
#define BLE_SVC_HTP_DSC_CLT_CFG_UUID16                       0x2902


//#define CONFIG_EXAMPLE_PEER_ADDR "ADDR_ANY"
#define MAX_CNT_BLE_SERVER  10//

// 自定义广播,tx 字节序号(第1帧广播)
#define BLE_ADV_FLAGS_ADDR_LEN                0//value=len+1=1+1=2
#define BLE_ADV_FLAGS_ADDR_TYPE               1//
#define BLE_ADV_FLAGS_ADDR_VALUE              2//len=1
 
#define BLE_ADV_COMP_UUIDS16_ADDR_LEN                 3//value=len+1=2+1=3
#define BLE_ADV_COMP_UUIDS16_ADDR_TYPE                4//
#define BLE_ADV_COMP_UUIDS16_ADDR_VALUE               5//len=2
 
#define BLE_ADV_DEV_ADDR_ADDR_LEN                7//value=len+1=7+1=8
#define BLE_ADV_DEV_ADDR_ADDR_TYPE               8//
#define BLE_ADV_DEV_ADDR_ADDR_VALUE              9//len=7

//"BLBLUETTF"
#define BLE_ADV_MFG_DATA_ADDR_LEN                16//value=len+1=9+1=10
#define BLE_ADV_MFG_DATA_ADDR_TYPE               17//
#define BLE_ADV_MFG_DATA_ADDR_VALUE              18//len=9


//type 值
#define BLE_ADV_TYPE_FLAGS                   0x01//;len=1
#define BLE_ADV_TYPE_COMP_UUIDS16            0x03//;len=2
#define BLE_ADV_TYPE_DEV_ADDR 				 0x1b//;len=7
#define BLE_ADV_TYPE_MFG_DATA                0xff//;len=9 "BLBLUETTF"
#define BLE_ADV_TYPE_COMP_NAME               0x09//;len=20~tbd//SN

// 标准BLE AD类型定义
#define BLE_AD_TYPE_FLAGS                    0x01  // Flags
#define BLE_AD_TYPE_INCOMPLETE_16BIT_UUIDS   0x02  // Incomplete List of 16-bit Service UUIDs
#define BLE_AD_TYPE_COMPLETE_16BIT_UUIDS     0x03  // Complete List of 16-bit Service UUIDs
#define BLE_AD_TYPE_INCOMPLETE_32BIT_UUIDS   0x04  // Incomplete List of 32-bit Service UUIDs
#define BLE_AD_TYPE_COMPLETE_32BIT_UUIDS     0x05  // Complete List of 32-bit Service UUIDs
#define BLE_AD_TYPE_INCOMPLETE_128BIT_UUIDS  0x06  // Incomplete List of 128-bit Service UUIDs
#define BLE_AD_TYPE_COMPLETE_128BIT_UUIDS    0x07  // Complete List of 128-bit Service UUIDs
#define BLE_AD_TYPE_SHORTENED_LOCAL_NAME     0x08  // Shortened Local Name
#define BLE_AD_TYPE_COMPLETE_LOCAL_NAME      0x09  // Complete Local Name
#define BLE_AD_TYPE_TX_POWER_LEVEL           0x0A  // TX Power Level
#define BLE_AD_TYPE_SIMPLE_PAIRING_OOB       0x0D  // Simple Pairing OOB Tags
#define BLE_AD_TYPE_SECURITY_MANAGER_TK      0x10  // Security Manager TK Value
#define BLE_AD_TYPE_SECURITY_MANAGER_OOB     0x11  // Security Manager OOB Flags
#define BLE_AD_TYPE_SLAVE_CONN_INTERVAL      0x12  // Slave Connection Interval Range
#define BLE_AD_TYPE_SERVICE_SOLICITATION_16  0x14  // Service Solicitation 16-bit UUIDs
#define BLE_AD_TYPE_SERVICE_SOLICITATION_128 0x15  // Service Solicitation 128-bit UUIDs
#define BLE_AD_TYPE_SERVICE_DATA_16BIT       0x16  // Service Data - 16-bit UUID
#define BLE_AD_TYPE_BLUETOOTH_MAC_ADDRESS    0x1B  // 蓝牙MAC地址
#define BLE_AD_TYPE_MANUFACTURER_DATA        0xFF  // Manufacturer Specific Data


/////////RSP，REQ的响应帧(第二帧广播)
//SN
#define BLE_HS_ADV_COMP_NAME_ADDR_LEN               0//27//value=len+1= 
#define BLE_HS_ADV_COMP_NAME_ADDR_TYPE              1//28//
#define BLE_HS_ADV_COMP_NAME_ADDR_VALUE             2//29//


//BLE RSSI
#define BLE_HIGH_THRESHOLD             -70//29//
#define BLE_LOW_THRESHOLD              -100//29//


#define BLE_CLIENT_STEP_ENCRYPT_1			1//加密，握手过程,BLBLUETTF版本，且配置CCCD后置1
#define BLE_CLIENT_STEP_ENCRYPT_GET_1		2//
#define BLE_CLIENT_STEP_ENCRYPT_SEND_2		3//
#define BLE_CLIENT_STEP_ENCRYPT_GET_3		4//
#define BLE_CLIENT_STEP_ENCRYPT_GET_4		5//
#define BLE_CLIENT_STEP_ENCRYPT_SEND_5		6//
#define BLE_CLIENT_STEP_ENCRYPT_GET_6_OK	7//解密完成后，
#define BLE_CLIENT_STEP_NORMAL				10//明文传输过程


#define BLE_CLIENT_MAX_TX_LEN		1200
#define BLE_CLIENT_MAX_RX_LEN		1200

#define BLE_CLIENT_DETECTOR_PERIOD	100 	// 定时器周期 单位ms
#define	BLE_CLIENT_DISCOVERY_TIME	1000 	// 蓝牙连接超时时间

// BLE扫描间隔时间定义（单位：ms）
#define BLE_SCAN_IDLE_TIME_MS           2000    // 空闲等待时间 ms
#define BLE_SCAN_TIME_MS                15000   // 蓝牙扫描时间 ms
#define BLE_CONNECT_TIMEOUT_MS          8000    // 连接超时时间 ms
#define BLE_SET_TIMEOUT_MS    			5000    // 参数设置超时时间 ms
#define BLE_PARAM_CHECK_TIMEOUT_MS    	15000   // 电压检查超时时间 ms
#define BLE_PERIOD_READ_MS    			1000   	// 周期获取消息时间 ms

#define MAX_DEVICE_TRY_COUNT	6	// 最大尝试设备数量

typedef enum
{
	MATCH_PARAM_ERR = -2,	// 参数错误
	MATCH_FAIL  	= -1,	// 比配失败
    MATCH_OK  		= 0,	// 比配正确
	MATCH_CPL  		= 1		// 比配完成
} eMatchStatus;


// BLE设备配对步骤枚举
typedef enum {
	BLE_PAIRING_STEP_INIT = 0,           // 初始化步骤（包含空闲等待功能）
	BLE_PAIRING_STEP_IDLE,               // 空闲状态（不做任何操作）
	BLE_PAIRING_STEP_SCAN_DEVICE,        // 扫描附近BLE设备，按RSSI排序
	BLE_PAIRING_STEP_CONNECT_DEVICE,     // 连接扫描到的设备
	BLE_PAIRING_STEP_READ_MD_VER,     	 // 读设备modbus版本号
	BLE_PAIRING_STEP_SET,        		 // 匹配参数设置
	BLE_PAIRING_STEP_CHECK,      		 // 匹配参数验证
	BLE_PAIRING_STEP_MATCH_SUCCESS,      // 物理接线和蓝牙通讯匹配一致
	BLE_PAIRING_STEP_MAX                 // 枚举边界检查
} ble_pairing_step_t;

// BLE设备配对控制结构体
typedef struct {
	ble_pairing_step_t pairing_step;     // 当前配对步骤
	uint16_t timeout;                    // 超时计数器（直接存储毫秒值，由定时器每100ms递减100）
	uint8_t current_device_index;        // 当前尝试的设备索引
	uint8_t device_try_count;            // 已尝试的设备数量
	uint8_t local_pairing_set_step;      // 本地匹配验证步骤
	uint8_t scan_started;                // 扫描状态标志：0=未开始，1=已开始
	uint8_t connect_started;             // 连接状态标志：0=未开始，1=已开始
} ble_pairing_control_t;



//static int8_t high_threshold = ;
//static int8_t low_threshold = ;


/** Peer. */
struct peer_dsc {
    SLIST_ENTRY(peer_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(peer_dsc_list, peer_dsc);

struct peer_chr {
    SLIST_ENTRY(peer_chr) next;
    struct ble_gatt_chr chr;

    struct peer_dsc_list dscs;
};
SLIST_HEAD(peer_chr_list, peer_chr);

struct peer_svc {
    SLIST_ENTRY(peer_svc) next;
    struct ble_gatt_svc svc;

    struct peer_chr_list chrs;
};
SLIST_HEAD(peer_svc_list, peer_svc);

struct peer;
typedef void peer_disc_fn(const struct peer *peer, int status, void *arg);

/**
 * @brief The callback function for the devices traversal.
 *
 * @param peer
 * @param arg
 * @return int  0, continue; Others, stop the traversal.
 *
 */
typedef int peer_traverse_fn(const struct peer *peer, void *arg);

struct peer {
    SLIST_ENTRY(peer) next;
    uint16_t conn_handle;

    uint8_t peer_addr[PEER_ADDR_VAL_SIZE];
    uint8_t ASCII_SN[31];//windy add

    /** List of discovered GATT services. */
    struct peer_svc_list svcs;

    /** Keeps track of where we are in the service discovery process. */
    uint16_t disc_prev_chr_val;
    struct peer_svc *cur_svc;

    /** Callback that gets executed when service discovery completes. */
    peer_disc_fn *disc_cb;
    void *disc_cb_arg;
};


typedef struct  {
	ble_addr_t peer_addr;//type +6 u8 address

//    uint8_t peer_addr[PEER_ADDR_VAL_SIZE];
    char ASCII_SN[BLE_BROADCAST_SN_LEN];//windy add
	int8_t rssi;//在连接后获取

    uint8_t encrypt;//协议类型：1-加密；2-非加密,0-无效

//timeout_cnt放在最后
    uint8_t timeout_cnt;//超时计数器,1s+1

}BLE_SERVER;
extern BLE_SERVER ble_server_get_buf;


typedef struct {
	 BLE_SERVER node[MAX_CNT_BLE_SERVER];//符合我司设备广播定义的设备信息

    uint16_t conn_handle;//当前正在连接的BLE server句柄,暂定0xFF-无效
    uint16_t val_handle_FF01;//0xFF02
    uint16_t val_handle_FF02_C2S_Wr;//0xFF02，APP to dev
    uint16_t val_handle_FF03;//0xFF02
	uint8_t node_index;//0~MAX_CNT_BLE_SERVER, 当前连接的server 数组序号
	uint8_t online_node_cnt;//广播发现合格在线数量：issi> x

	//self
    uint8_t own_addr_type;
	uint16_t mtu_value;
    uint8_t event_type;//ble client event当前状态,0xFF-无效；0-库函数定义为连接

}Ble_Server_node_sum_STRUCT;
extern  Ble_Server_node_sum_STRUCT Ble_Server_node_sum;

typedef struct {
	uint8_t FlagRx_ok;//1-收到uart报文，发送前清零，解析完清零
	uint16_t rxBytesNum;//
	uint16_t TxBytesNum;
	
	uint32_t Rx_timeout_cnt;//rx超时计数器；100ms+1
	uint16_t RxOk_Delay_cnt;//rx收到后，要超时清零复位，1s；

	uint8_t ble_connect_step;//加密握手状态机，0-no connect;

	uint8_t ble_client_Rx_bufA[BLE_CLIENT_MAX_RX_LEN];//rx, 一级buf，接收拼接缓存,
	uint16_t ble_client_Rx_lenA;
	
	uint8_t *ble_client_Rx_bufB;//rx, 2级buf，拼接后解析缓存，指针，指向临时malloc RAM,下级解析取此变量
	uint16_t ble_client_Rx_lenB;
	
	uint8_t ble_client_Tx_buf[BLE_CLIENT_MAX_TX_LEN];
	uint16_t ble_client_Tx_len;

	src_addr_info_t src_addr;
}BLE_CLIENT_DATA_STRUCT;

/**
 * @brief 解析广播数据中的AD结构
 */
typedef struct {
	const uint8_t *mfg_data;
	uint8_t mfg_len;
	const uint8_t *name_data;
	uint8_t name_len;
} ad_data_t;


#pragma pack()

typedef struct {
    uint8_t *data;          // 数据指针
    int len;                // 数据长度
    src_addr_info_t src_addr;
} ble_c2s_msg_t;

typedef struct {
    uint8_t msg[256];       // 数据内容
    int len;                // 数据长度
    src_addr_info_t src_addr;
} ble_c2s_msg_bck_t;


/** Misc. */
void print_bytes(const uint8_t *bytes, int len);
void print_mbuf(const struct os_mbuf *om);
void print_mbuf_data(const struct os_mbuf *om);
char *addr_str(const void *addr);
void print_uuid(const ble_uuid_t *uuid);
void print_conn_desc(const struct ble_gap_conn_desc *desc);
void print_adv_fields(const struct ble_hs_adv_fields *fields);
void ext_print_adv_report(const void *param);

void peer_traverse_all(peer_traverse_fn *trav_cb, void *arg);
int peer_disc_svc_by_uuid(uint16_t conn_handle, const ble_uuid_t *uuid, peer_disc_fn *disc_cb,
                          void *disc_cb_arg);

int peer_disc_all(uint16_t conn_handle, peer_disc_fn *disc_cb,
                  void *disc_cb_arg);
const struct peer_dsc *
peer_dsc_find_uuid(const struct peer *peer, const ble_uuid_t *svc_uuid,
                   const ble_uuid_t *chr_uuid, const ble_uuid_t *dsc_uuid);
const struct peer_chr *
peer_chr_find_uuid(const struct peer *peer, const ble_uuid_t *svc_uuid,
                   const ble_uuid_t *chr_uuid);
const struct peer_svc *
peer_svc_find_uuid(const struct peer *peer, const ble_uuid_t *uuid);
int peer_delete(uint16_t conn_handle);
int peer_add(uint16_t conn_handle);
int peer_init(int max_peers, int max_svcs, int max_chrs, int max_dscs);
struct peer *peer_find(uint16_t conn_handle);
//#if MYNEWT_VAL(ENC_ADV_DATA)
int peer_set_addr(uint16_t conn_handle, uint8_t *peer_addr);
//#endif
int peer_set_addr_SN(uint16_t conn_handle, uint8_t *peer_addr, uint8_t *sn);



void ble_htp_cent_scan(void);
void ble_client_Rx_buf_copy(uint8_t direct_run);//
void ble_client_Rx_data_prase(void);
void debug_print_adv_node(void);

int Ble_C2S_Msg_Send(uint8_t *pMsg, uint16_t len, src_addr_info_t src_addr);
int Ble_C_Msg_Type_Send(uint8_t msgType, uint8_t slaveAddr, uint8_t *pIn, uint16_t inLen);

int ble_c2s_send_to_queue(const uint8_t *data_buf, uint16_t len, src_addr_info_t src_addr);


void ble_client_task(void);

void ble_client_init(void);

#ifdef __cplusplus
}
#endif

#endif

#endif
