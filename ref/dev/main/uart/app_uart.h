#ifndef __APP_UART_H__
#define __APP_UART_H__



#include <string.h>
#include "esp_log.h"
#include "esp_err.h"


/*
UART 分配
ATS:
UART0:Sub 1GHz
UART1:AT403
UART2:4G


DTU:
UART0:RS485
UART1:Sub 1GHz
UART2:4G



*/

#define UART0_TAG       "[APP_UART0]"//
#define UART1_TAG       "[APP_UART1]"//
#define UART2_TAG       "[APP_UART2]"//

#define UART2_SW        (27)

#define UART0_BAUD	115200//19200		/**< 串口波特率 */
#define UART1_BAUD	115200//9600		/**< 串口波特率 */
#define UART2_BAUD	115200//115200		/**< 串口波特率 */



#define UART0           UART_NUM_0//RS485
#define UART0_TXD       (UART_PIN_NO_CHANGE)  
#define UART0_RXD       (UART_PIN_NO_CHANGE) 
#define UART0_RTS       (UART_PIN_NO_CHANGE)
#define UART0_CTS       (UART_PIN_NO_CHANGE)

#define UART1           UART_NUM_1//Sub1GHz
#define UART1_TXD       (42)  
#define UART1_RXD       (41) 
#define UART1_RTS       (UART_PIN_NO_CHANGE)
#define UART1_CTS       (UART_PIN_NO_CHANGE)

#define UART2           UART_NUM_2//4G
#define UART2_TXD       (39)  
#define UART2_RXD       (40) 
#define UART2_RTS       (UART_PIN_NO_CHANGE)
#define UART2_CTS       (UART_PIN_NO_CHANGE)




#define RX_HW_FIFO_LEN      120

/*
芯片要求：
Rx_buffer_size should be greater than UART_HW_FIFO_LEN(uart_num). 
Tx_buffer_size should be either zero or greater than UART_HW_FIFO_LEN(uart_num).

*/
#define UART_RX_BUF_SIZE         1100
#define UART_TX_BUF_SIZE         1100//1024

#define UART0_RX_BUF_SIZE         1100
#define UART0_TX_BUF_SIZE         1100//1024
#define UART1_RX_BUF_SIZE         1100
#define UART1_TX_BUF_SIZE         1100//1024
#define UART2_RX_BUF_SIZE         1100
#define UART2_TX_BUF_SIZE         1100//1024

/* uart在接收到uart fifo大小字节的数据时将触发数据中断，
 * 设置等待接收任何后续数据的时间 */
#define UART_SUBSEQUENT_DATA_TIMEOUT	200
#define UART_HW_FIFO_SIZE				120 			// UART硬件FIFO大小
#define UART_RX_TIMEOUT	10//ms,uart rx超时延时，容许报文分断间隔


#define UART0_BYTE_TRANSMISSION_TIME_MS (1000.0 / UART0_BAUD * 10) // 每字节传输时间（毫秒）
#define UART0_SUBSEQUENT_DATA_TIMEOUT (UART_HW_FIFO_SIZE * UART0_BYTE_TRANSMISSION_TIME_MS + 20) // 增加一些余量


//typedef struct {
//	uint8_t FlagRx_ok;//1-收到uart报文，发送前清零，解析完清零
//	uint8_t rxBytesNum;//
//	uint32_t Rx_timeout_cnt;//rx超时计数器,收到清零
//	uint16_t RxOk_Delay_cnt;//rx收到后，要超时清零复位，1s；
//	
//	uint16_t gRegAddress;//作为modbus主的辅助解析变量，寄存器地址
//	uint8_t	gFunCode;
//	uint16_t gRegCnt;//作为modbus主的辅助解析变量，度寄存器数量
//	
//	uint8_t Rxbuffer[UART0_RX_BUF_SIZE];
//	uint8_t Txbuffer[UART0_TX_BUF_SIZE];
//
//}UART0_STRUCT;

typedef struct {
	uint8_t FlagRx_ok;//1-收到uart报文，发送前清零，解析完清零
	uint8_t rxBytesNum;//
	uint16_t TxBytesNum;
	
	uint32_t Rx_timeout_cnt;//rx超时计数器；100ms+1
	uint16_t RxOk_Delay_cnt;//rx收到后，要超时清零复位，1s；

	uint16_t gRegAddress;//作为modbus主的辅助解析变量，寄存器地址
	uint8_t	gFunCode;
	uint16_t gRegCnt;//作为modbus主的辅助解析变量，度寄存器数量

	uint8_t slaveaddress;
    
	uint8_t Rxbuffer[UART1_RX_BUF_SIZE];
	uint8_t Txbuffer[UART1_TX_BUF_SIZE];

}UART_STRUCT;//UART1_STRUCT
//typedef	UART1_STRUCT UART0_STRUCT	;


typedef struct {
	uint8_t FlagRx_ok;//1-收到uart报文，发送前清零，解析完清零
	uint16_t rxBytesNum;//
	uint16_t Rx_timeout_cnt;//rx超时计数器；
	uint16_t RxOk_Delay_cnt;//rx收到后，要超时清零复位，1s；

	uint16_t gRegAddress;//作为modbus主的辅助解析变量，寄存器地址
	uint8_t	gFunCode;
	uint16_t gRegCnt;//作为modbus主的辅助解析变量，度寄存器数量
	
	uint8_t Rxbuffer[UART2_RX_BUF_SIZE];
	uint8_t Txbuffer[UART2_TX_BUF_SIZE];

}UART2_STRUCT;


void app_uart_init(void);
int app_read_uart1_data(uint8_t *pdata, int max_len, uint32_t timeout);
int app_read_uart2_data(uint8_t *pdata, int max_len, uint32_t timeout);


void app_uart0_init_task(void);
void app_uart0_close_tasks(void);

int app_write_uart0_data(uint8_t *pdata, int len);
int app_write_uart1_data(uint8_t *pdata, int len);
int app_write_uart2_data(uint8_t *pdata, int len);
void uart_rx_timeout_clean (void);
void app_uart2_Deinit(void);
#endif


