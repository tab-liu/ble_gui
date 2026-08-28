#ifndef UDP_MULTICAST_MAIN_H
#define UDP_MULTICAST_MAIN_H

#include "comm_define.h"
#include "lwip/sockets.h"
#include "iot_mqtt.h"

#define UDP_STEP_NONE	0//
#define UDP_STEP_RESTART	1//
#define UDP_STEP_INIT	2//
#define UDP_STEP_INIT_READY	3//




#define MULTICAST_IPV4_ADDR "239.0.0.238"//
#define UDP_PORT 5000//UDP广播端口
#define UDP_PORT_SINGLE 1234//1234//UDP单播端口地址

//void iot_console_init(void);
//void Udp_multicast_task(void);

typedef struct {
	uint8_t rxBytesNum;//
	uint16_t TxBytesNum;

	uint16_t gRegAddress;//作为modbus主的辅助解析变量，寄存器地址
	uint16_t gRegCnt;//作为modbus主的辅助解析变量，度寄存器数量

	uint8_t slaveaddress;//只在 UDP RX 报文接收时候，和reals.discovery_net_Info[]比对查表时候赋值，用于21000上报和 Inv[]数组序号关联
}UDP_MODBUS_STRUCT;

typedef struct
{
    uint8_t current_netif_id;//网卡名称序号
    struct ifreq udp_netif_req;
    uint8_t states[NETIF_TYPE_MAX];//0:未配置 1：可配置 2：配置成功
    char source_ip_str[32];//本机WIFI AP IP
}udp_config_struct;

extern EXT_RAM_BSS_ATTR udp_config_struct udp_config;

int Udp_singlecast_Tx(uint8_t *target_sta_ipv4, uint8_t *target_portaddr, uint8_t *buff, uint16_t Len);//
int Udp_Multicast_Tx(void);//
void wifi_mesh_Top_Tx_task(void);

int16_t UDP_Rx_Task(void);
void Udp_singlecast_init(void);
void Udp_multicast_init(void);
void udp_fast_tx_Push(uint16_t regaddress, uint16_t regcnt, uint16_t slaveindex);
int Udp_Net_Frame_Modbus_Format_Check(const uint8_t *income, uint16_t inlen);
uint8_t Udp_Heartbeat_TxFrame(uint8_t *outbuf);
uint8_t Modbus_MasterRespones_Udp(uint8_t *income, uint16_t cmdLen);
static uint16_t Modbus_Slave_WriteSingleReg_Udp(const uint8_t *income, uint16_t inLen, uint8_t *response);
static uint16_t Modbus_Slave_WriteMultiRegs_Udp(const uint8_t *income, uint16_t inLen, uint8_t *response) ;
static uint8_t Wifi_Udp_Network_Heartbeat(const uint8_t *cmdBuf);
uint16_t Udp_Singlecast_Modbus_MasterTxCmd(uint16_t regAddress, uint8_t regNum, uint8_t *outbuf, uint8_t *dst_sn_type,uint8_t frametype, uint16_t data_len);
static uint8_t Modbus_ReadReg_03H_RTN_Udp(UDP_MODBUS_STRUCT *udp_modbus, const uint8_t *cmdBuf);
static uint8_t Modbus_WriteReg_06H_10H_RTN_Udp(UDP_MODBUS_STRUCT *udp_modbus, const uint8_t *cmdBuf);
void UDP_Tx_Period_module(void);
void iot_wifi_udp_task(void);
void iot_udp_start(uint8_t mode);
void iot_udp_delete(uint8_t mode) ;


#endif
