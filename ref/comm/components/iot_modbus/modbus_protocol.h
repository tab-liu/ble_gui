#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_ADDR                     0x01 	// modbus本机地址
#define MB_READ_HOLD_REG         	0x03 	// modbus读多个寄存器
#define MB_WRITE_ONE_HOLD_REG 		0x06 	// modbus单个寄存器写
#define MB_WRITE_MULTI_HOLD_REG  	0x10 	// modbus写多个寄存器
#define MB_FUNCODE_ERROR 			0x80 	// modbus错误功能码
#define MB_ERROR_NOT_SUPPORTED 		0X01 	// modbus错误类型

#define MD_TBL_MAX 					40 		// modbus数据表最大数量

/* 前向声明：保留struct标签声明（可选） */
typedef struct md_tbl_t md_tbl_t;
typedef struct md_priv_data_t md_priv_data_t;
typedef struct tbl_cb_data_t tbl_cb_data_t;

/**
 * @brief 表处理函数回调参数定义
 *
 * 修复说明：
 * - 之前使用 anonymous typedef（typedef struct { ... } tbl_cb_data_t;）
 *   与前向声明 typedef struct tbl_cb_data_t tbl_cb_data_t; 冲突。
 * - 解决方法：使用带 tag 的 struct 定义并与前向声明一致。
 */
typedef struct tbl_cb_data_t {
    uint8_t SlaveAddress;
    uint16_t reg_addr_offset;	// 写入reg相对寄存器块起始的相对偏移（字节）
    uint16_t reg_addr;			// 寄存器地址
    uint16_t reg_nums;			// 寄存器数量
    bool is_write;				// 是否是寄存器写
    int cb_chl;                 // 访问途径
	uint8_t is_param_sync;		// 是否是参数同步
	uint32_t param_set_time;	// 同步参数配置的时间
} tbl_cb_data_t;

/**
 * @brief 表读写事件回调函数
 *
 * 将回调类型声明放在 md_tbl_t 之前，使用带 tag 的 struct 名称保证一致性
 */
typedef int (*tbl_callback_t)(struct md_tbl_t *tbl, struct tbl_cb_data_t *cb_data, struct md_priv_data_t *priv_data);

/**
 * @brief modbus数据表结构定义（带 struct tag，与前向声明一致）
 */
typedef struct md_tbl_t {
    uint16_t start;				// 表开始地址
    uint16_t end;				// 表结束地址
    bool is_write;				// 表是否可写
    tbl_callback_t tbl_cb;		// 表回调函数
} md_tbl_t;

/* modbus私有数据定义（带 struct tag） */
typedef struct md_priv_data_t {
    int channel;	// 传输通道
    int (*ota_response)(void *data, int len);
} md_priv_data_t;

/**
 * @brief 表读写事件回调函数
 * 
 * @param tbl 表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * - 该数据来自应用层传递的私有数据
 */
typedef int (*tbl_callback_t)(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data);//, uint8_t SlaveAddress

/**
 * @brief 定义输入输出数据缓存结构
 * 
 */
typedef struct {
	uint8_t *in_buff; 			// 输入缓存
	int in_len; 				// 输入大小
	uint8_t *out_buff; 			// 输出缓存
	int *out_len;				// 输出大小
} md_buff_t;
/**
 * @brief 定义modbus数据表结构
 *
 */
typedef struct {
	md_tbl_t tbl;	// 数据表
	uint16_t *data;	// 数据表数据
} md_data_t;

typedef struct {
	uint8_t slave_addr;
    uint16_t reg_addr;
    uint16_t reg_num;
}md_read_t;


int md_add_tbl(md_tbl_t *p_tbl);
int Modbus_Format_Check(const uint8_t *income, uint16_t inlen);
int Modbus_Rebuild_Frame_With_Addr(uint8_t slaveAddr, uint8_t *pIn, uint8_t inLen, uint8_t *pOut);
int md_protocol_check(uint8_t *buff, int len);
int md_reg_data_get(uint16_t reg_addr, uint16_t *data, int len);
int md_reg_data_set(uint16_t reg_addr, uint16_t *data, int len);
md_data_t *md_tbl_find(uint16_t reg_addr);
void Modbus_beta_reg_table_register_init(void);
void md_data_init(void);
void modbus_data_semaphore_init(void);  //TODO: modbus init
uint8_t modbus_data_semaphore_Take(void);
void modbus_data_semaphore_Give(void);
#ifdef __cplusplus
}
#endif
