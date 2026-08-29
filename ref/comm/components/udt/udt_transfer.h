#ifndef __UDT_TRANSFER_H__
#define __UDT_TRANSFER_H__
#include <stdint.h>


#define MAX_LIST_NUMS			10 							// 中继列表最大数量
#define UDT_PROT_MARK			0x09						// 协议标志
#define UDT_PROT_VER			0xF8						// 协议版本
#define UDT_PROT_SCENE			0x01						// 场景码
#define UDT_ONCE_TX_MAX_SIZE	1024						// 单次发送最大长度
#define UDT_DATA_BUFF_SIZE		(UDT_ONCE_TX_MAX_SIZE+30)	// 数据缓存

#define UDT_MODE_SVC			0							// 服务端模式
#define UDT_MODE_PC				1							// PC端模式
#define UDT_MODE_BLE			2							// 蓝牙模式

/* 短整型数据高低交换 */
#define UDT_SWAP16(N)     		((((uint16_t)(N) & 0xff00) >> 8) | \
								(((uint16_t)(N) & 0x00ff) << 8))

/* 整型数据高低交换 */
#define UDT_SWAP32(N)     		((((uint32_t)(N) & 0xff000000) >> 24) | \
								(((uint32_t)(N) & 0x00ff0000) >> 8) | \
								(((uint32_t)(N) & 0x0000ff00) << 8) | \
								(((uint32_t)(N) & 0x000000ff) << 24))

/**
 * @brief 数据中转函数指针定义
 */
typedef int (*data_transfer_t)(uint8_t addr, uint8_t *data, int size);

#pragma pack(1)
/**
 * @brief 通用传输协议功能码
 */
typedef enum {
    UDT_FUNCODE_QUERY_FILE = 50,   				// 查询文件功能码
    UDT_FUNCODE_QUERY_FILE_RTN = 51,    		// 查询文件设备端响应功能码
    UDT_FUNCODE_QUERY_CONTENT = 52,   			// 查询数据功能码
    UDT_FUNCODE_QUERY_CONTENT_RTN = 53,			// 查询数据设备端响应功能码
    UDT_FUNCODE_QUERY_CONTENT_SVC_ACK = 54,		// 查询数据服务端响应功能码
    UDT_FUNCODE_NEW_LOG_REPORT = 55,            // 设备主动上报服务器功能码
	UDT_FUNCODE_MAX
} udt_funcode_t;

/**
 * @brief 通用传输协议定义
 串行载荷协议透传,帧格式，帧头
 */
typedef struct {
	uint8_t mark;
	uint8_t version;
	uint8_t scene;//场景代码
	uint16_t req_id;
	uint16_t pkg_idx;//第几包数据
	uint16_t pkg_total;//数据包总数
	uint8_t type_read;//0-默认全量读取；1-增量读取（下级只上报差异部分）
	uint8_t address;
	uint8_t funcode;
	uint32_t start_pos;//文件数据开始位
	uint32_t data_size;//本次指令有效数据字节数
} udt_protocol_t;

/**
 * @brief 中继列表结构定义
 * - 服务端通过地址来确认查询哪一台设备数据
 * - 当接收到服务端数据时,通过地址查找对应的发送函数,之后调用该发送函数将服务端的数据发送到对应设备
 * - exp: 当地址在1-20范围时,调用udt_transfer_to_inv转发服务端数据到逆变器
 * - exp: 当地址在41-104范围时,调用udt_transfer_to_pack转发服务端数据到电池包
 */
typedef struct {
	/* 地址范围 */
	uint8_t addr1;				// 地址起始值
	uint8_t addr2;				// 地址结束值

	data_transfer_t transfer;	// 该地址范围内对应的发送函数
} udt_relay_list_t;

/**
 * @brief 中继结构定义
 */
typedef struct {
	uint8_t nums;								// 列表中的数量
	udt_relay_list_t list[MAX_LIST_NUMS];		// list中保存目标地址与其对应的发送函数
} udt_relay_t;

/**
 * @brief 传输结构定义
 */
typedef struct {
	data_transfer_t to_server;					// 传输到服务端函数指针
	data_transfer_t to_pc;						// 传输到PC端函数指针
	data_transfer_t to_ble;						// 传输到蓝牙端函数指针
} udt_transfer_t;

/**
 * @brief 发送数据结构定义
 * - 作为udt_transmit函数的参数,用于将本地数据发送到服务端
 */
typedef struct {
    uint8_t mode;
	uint16_t req_id;
	uint16_t pkg_idx;
	uint16_t pkg_total;
	uint32_t start_pos;
	uint32_t data_size;
	uint8_t funcode;
	uint8_t *data;
} udt_tx_data_t;

/**
 * @brief 事件结构定义
 * - 当接收到服务端的事件时,作为回调函数的参数,通过判断事件类型来确认服务端发送的指令
 */
typedef struct {
	uint8_t funcode;				// 功能码
    uint8_t mode;                   // 查询模式
	uint16_t req_id;                // 查询标签
	union {
		struct {					// 当功能码为UDT_FUNCODE_QUERY_CONTENT时的参数
			uint32_t start_pos;
			uint8_t fname[11];//待查询 文件名(额外预留1个空字节，避免sprintf时引入不可见字符)
			uint8_t type_read;
		} qry_cont;

		struct {					// 当功能码为UDT_FUNCODE_QUERY_CONTENT_SVC_ACK时的参数
			uint16_t pkg_total;//成功总包数
			uint16_t last_pkg_seq;//连续成功最后一包序号
		} svc_ack;
	} param;
} udt_event_t;

typedef struct {
	uint8_t *data;
	int data_len;
    uint8_t chl;
} udt_recv_data_t;

#pragma pack()

/**
 * @brief 事件回调函数定义
 */
typedef int (*udt_evt_callback_t)(udt_event_t *evt);

// UDT模块初始化
void udt_init(uint8_t local_addr, udt_transfer_t *transfer, udt_relay_t *relay, udt_evt_callback_t evt_cb);
void udt_set_address(uint8_t local_addr);

// UDT模块数据中继接口
int udt_relay_to_terminal(uint8_t *data, int size);
int udt_relay_to_device(uint8_t *data, int size, uint8_t mode);

// UDT模块数据收发接口
int udt_transmit(udt_tx_data_t *tx_data);
int udt_receive(uint8_t *data, int size, uint8_t mode);

// UDT模块接收用户层请求逻辑
int udt_recv_push(uint8_t *data, int size, uint8_t chl);

// UDT模块用户层请求处理逻辑
void udt_recv_process_task(void);

// 获取当前UDT中继工作状态
int udt_relay_mode_get(void);

// UDT组帧
uint16_t udt_header_data(uint8_t *buffer, udt_tx_data_t *tx_data);

#endif
