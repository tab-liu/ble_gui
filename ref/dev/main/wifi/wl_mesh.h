#ifndef _WL_MESH_H_
#define _WL_MESH_H_

#include <stdint.h>

/** 无线设备间协议字段偏移量 */
#define WL_DEV_PRO_ADDR_HEAD                    (0 )  // 协议头: 此处用作功能码
#define WL_DEV_PRO_ADDR_VER                     (1 )  // 协议版本号
#define WL_DEV_PRO_ADDR_SN_SRC                  (2 )  // 源设备SN
#define WL_DEV_PRO_ADDR_TYPE_SRC                (10)  // 源设备类型
#define WL_DEV_PRO_ADDR_FUNC_CODE               (22)  // 功能码
#define WL_DEV_PRO_ADDR_TTL                     (23)  // TTL

/** 周期上报相关 */
// #define WL_DEV_PRO_ADDR_REPORT_PRIORITY         (25)  // 优先级
// #define WL_DEV_PRO_ADDR_REPORT_PARALLEL_SEQ     (27)  // 并机序号
#define WL_DEV_PRO_ADDR_REPORT_LAYER            (25)  // 节点层级
#define WL_DEV_PRO_ADDR_REPORT_ROUTE            (27)  // 节点路由
// #define WL_DEV_PRO_ADDR_REPORT_DATA_LEN         (28)  // 数据长度
// #define WL_DEV_PRO_ADDR_REPORT_DATA_START       (30)  // 数据起始位置

/** 功能码 */
#define WL_DEV_PRO_FUNC_REPORT                  0x20   //周期上报, 无响应

#pragma pack(1)


/**
 * @brief 报文类型定义
 * 
 */
typedef struct {
	uint8_t code : 4;
	uint8_t protocol : 4;
} frame_type_t;

typedef union
{
    uint8_t dev_sn[10];
    struct
    {
		uint64_t sn;				/**< 设备SN */
		uint16_t dev_type;			/**< 设备型号 */
    };
} mesh_node_sn_t;

/**
 * @brief 无线设备间协议帧格式 局域网设备发现帧
 * 
 */
typedef struct {
	/* 报文类型定义 */
	uint8_t protocol_head;					// 报文类型,协议头
	uint8_t protocol_version;						// 无线传输通道,协议版本
	mesh_node_sn_t src_devSN;							/**源设备 节点SN */
	mesh_node_sn_t dst_devSN;							/**目标设备节点SN */
	frame_type_t frame_type;			// 报文类型
	uint16_t source_priority;			// 源设备优先级
	uint8_t parallel_num;				// 并机序号
	uint16_t ttl;//广播生命周期
	uint8_t level_route;//路由层级
//	uint8_t *Ip_array_route;	//占用4字节
//	uint32_t IP;//IP地址层1

} msg_discovery_t;




/**
 * @brief 无线设备间协议帧格式 业务逻辑协议
 * 
 */
typedef struct {
	/* 报文类型定义 */
	uint8_t protocol_head;					// 报文类型,协议头
	uint8_t protocol_version;						// 无线传输通道,协议版本
	mesh_node_sn_t src_devSN;							/**源设备 节点SN */
	mesh_node_sn_t dst_devSN;							/**目标设备节点SN */
	
	frame_type_t frame_type;					// 报文类型
	uint16_t ttl;//广播生命周期
	uint16_t md_len;			// modbus长度
	
//	uint8_t *modbus_data;				// modbus数据//占用4字节

} msg_general_t;//


#define	MAX_RX_LEN_WIFI_MESH	1100//1029+25=1054
#define	MAX_TX_LEN_WIFI_MESH	1100//

typedef struct {

		uint8_t data_rx_wifi_mesh[MAX_RX_LEN_WIFI_MESH];
		uint8_t data_tx_wifi_mesh[MAX_TX_LEN_WIFI_MESH];
		int16_t len_rx_wifi_mesh;//<0 :非法无效
		uint16_t len_tx_wifi_mesh;


}WIFI_MESH_INTERFACE_STRUCT;
extern   WIFI_MESH_INTERFACE_STRUCT wireless_interface;

typedef struct _mesh_node_master_t
{
	mesh_node_sn_t dev_sn;							/**< 节点SN */

    uint16_t priority;              /**< 优先级 */
    uint16_t parallel_seq;          /**< 并机序号 */
} mesh_node_master_t;
extern mesh_node_master_t g_master_info;

#pragma pack()

void wl_mesh_task(void *param);
void wl_mesh_disable(void);

#endif