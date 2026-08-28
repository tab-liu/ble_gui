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

#define MD_TBL_MAX 					10 		// modbus数据表最大数量

/**
 * @brief 表处理函数回调参数定义
 *
 */
typedef struct {
	uint8_t SlaveAddress;
	uint16_t reg_addr_offset;	//写入reg相对寄存器块起始的相对偏移，指示具体写入寄存器地址起始位置，=寄存器个数*2，即字节偏移

	uint16_t reg_addr;			// 寄存器地址
	uint16_t reg_nums;			// 寄存器数量
	bool is_write;				// 是否是寄存器写
	int cb_chl;                 // 访问途径
} tbl_cb_data_t;

/**
 * @brief 表读写事件回调函数
 *
 * @param tbl 表指针
 * @param cb_data 参数
 * @param priv_data 私有数据
 * - 该数据来自应用层传递的私有数据
 */
typedef int (*tbl_callback_t)(void *tbl, tbl_cb_data_t *cb_data, void *priv_data);//, uint8_t SlaveAddress

/**
 * @brief modbus数据表结构定义
 *
 */
typedef struct {
	uint16_t start;				// 表开始地址
	uint16_t end;				// 表结束地址
	bool is_write;				// 表是否可写
	tbl_callback_t tbl_cb;		// 表回调函数
} md_tbl_t;

/**
 * @brief 定义输入输出数据缓存结构
 *
 */
typedef struct {
	uint8_t *in_buff; 			// 输入缓存
	int in_len; 				// 输入大小
	uint8_t *out_buff; 			// 输出缓存
	uint16_t out_len;				// 输出大小
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
int md_protocol_check(uint8_t *buff, int len);
int md_reg_data_get(uint16_t reg_addr, uint16_t *data, int len);
int md_reg_data_set(uint16_t reg_addr, uint16_t *data, int len);
md_data_t *md_tbl_find(uint16_t reg_addr);
void Modbus_beta_reg_table_register_init(void);
//__weak *md_data_t Modbus_beta_reg_callback_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data);//windy:通用统一回调函数
md_data_t *Modbus_beta_reg_callback_handler(void *tbl, tbl_cb_data_t *cb_data, void *priv_data);
void md_data_init(void);
void iot_modbus_data_init(void);

#ifdef __cplusplus
}
#endif
