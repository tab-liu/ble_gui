/**
  ******************************************************************************
  * @file      dev_discovery.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/6
  * @brief     网络设备及sub1g设备发现和状态处理
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/6   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */


/*

COMBOX网络的三层网络设备定义：
基于同一WIFI路由器的设备节点(如COMBOX/D100S/S1)
基于同一Sub 1Ghz子网络的设备节点(如COMBOX下级的A100S)
基于绑定的 绑定子网络，和并列子网络(如D100S和A80)

同一层级的设备主动发现需要基于《无线设备间（如mesh）应用层通用协议》中的“局域网设备发现”协议约定


*/
#include "mesh_api.h"
#include "iot_period_task.h"
#include "dev_discovery.h"
#include "comm_define.h"
#include "filesystem.h"
#include "udp_multicast.h"
#include "uart_device_process.h"
#include "can_protocol.h"
#include "iot_mqtt.h"
#include "iot_ota.h"
#include "ota_type.h"
#include "can_pack.h"
#include "http_client.h"

extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];

static const char *TAG = "[DEV_DISCOVERY]";

static QueueHandle_t xQueue_new_wifi_dev_info = NULL;//UDP 接收的 设备SN 队列,包括UDP RX组播和单播源设备SN
static SemaphoreHandle_t modbus_21000_semaphore; //modbus 21000空闲信号量

static uint8_t Wifi_Mesh_Protocol_Process(const uint8_t *cmdBuf, char *income_ip_str, uint16_t inport);
static uint8_t Udp_Multicast_Finish_After(const uint8_t *cmdBuf);
/*------------------------------------------------------------------------------
 Function: Modbus_21000_semaphore_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      信号量初始化
  * @param[in]  void
  * @param[out] None
  * @return     void
  */
void Modbus_21000_semaphore_init(void)
{
	modbus_21000_semaphore = xSemaphoreCreateBinary();			/**< create fuda wifi shared resource */
	if(modbus_21000_semaphore == NULL)
	{
        ESP_LOGE(TAG, "modbus_21000_semaphore create failed");
		return;
	}
   // ESP_LOGE(TAG, "modbus_21000_semaphore create OK");
	xSemaphoreGive(modbus_21000_semaphore);						/**< the resource is available after creation */
}

/*------------------------------------------------------------------------------
 Function: Modbus_21000_semaphore_Take
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取信号量
  * @param[in]  void
  * @param[out] None
  * @return     uint8_t
  */
uint8_t Modbus_21000_semaphore_Take(void)
{
    if (!modbus_21000_semaphore)
    {
        ESP_LOGE(TAG,"modbus_21000_semaphore null");
        return 0;
    }

    if(xSemaphoreTake(modbus_21000_semaphore, pdMS_TO_TICKS(200)) != pdTRUE)
    {
        ESP_LOGE(TAG,"modbus_21000_semaphore error");
        return 0;
    }
   // ESP_LOGE(TAG,"modbus_21000_semaphore ok");
    return 1;
}

/*------------------------------------------------------------------------------
 Function: Modbus_21000_semaphore_Give
 -----------------------------------------------------------------------------*/
/**
  * @brief      释放信号量
  * @param[in]  void
  * @param[out] None
  * @return     void
  */
void Modbus_21000_semaphore_Give(void)
{
    //xSemaphoreGive(modbus_21000_semaphore);  /*释放信号量*/

	xSemaphoreGive(modbus_21000_semaphore);
}



static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ",
                        buf[i], i == len - 1 ? "\n":"");
    }
}
/*------------------------------------------------------------------------------
 Function: top_dev_info_queue_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      初始化队列
  * @param[in]  void
  * @param[out] None
  * @return     void
  */
void top_dev_info_queue_init(void)
{
    /*队列发送*/
    if (xQueue_new_wifi_dev_info == NULL)
	{
		xQueue_new_wifi_dev_info = xQueueCreate(NET_WIFI_MAX_POINT, sizeof(DISCOVERY_POINT_InfoStruct));
		if (xQueue_new_wifi_dev_info == NULL)
		{
			ESP_LOGE(TAG, "top_dev_info_queue_init create failed");
		}
	}
}

/*------------------------------------------------------------------------------
 Function: top_dev_info_queue_Push
 -----------------------------------------------------------------------------*/
/**
  * @brief      发送到队列
  * @param[in]  uint64_t MAC_64
                uint64_t SN_64
                uint16_t Dev_Type
  * @param[out] None
  * @return     void
  */
void top_dev_info_queue_Push(DISCOVERY_POINT_InfoStruct *data)
{
//	DISCOVERY_POINT_InfoStruct *queue_msg = NULL;
//
//    queue_msg = (DISCOVERY_POINT_InfoStruct *)heap_caps_malloc(sizeof(DISCOVERY_POINT_InfoStruct), MALLOC_CAP_SPIRAM);
//
//	if (!queue_msg)
//	{
//		ESP_LOGE(TAG, "top_dev_info calloc failed");
//	}
//	else
//	{
//		memcpy(queue_msg, data, sizeof(DISCOVERY_POINT_InfoStruct));
//
//		/*消息保存到队列*/
//		if (xQueueSendToBack((QueueHandle_t)xQueue_new_wifi_dev_info, &queue_msg, 0) != pdPASS)
//		{
//			ESP_LOGE(TAG, "top_dev_info push queue failed");
//			free(queue_msg);
//			queue_msg = NULL;
//		}
//	}


  /*设备信息变化*/
  reals.net_point_Comein = Top_Net_Point_Rx_Get(data);

}



/*------------------------------------------------------------------------
*@Function： Device_Discovery_Frame_Format_Check
判断接收报文是否为《无线设备间（如mesh）应用层通用协议》中的“局域网设备发现”协议约定

*@return
-1： fail
other:功能码

*/
int Device_Discovery_Frame_Format_Check(const uint8_t *income, uint16_t inlen)
{
    int8_t ret = 0;
    int8_t head = income[WIFI_UDP_FRAME_ADDR_HEAD];
	uint16_t Value_crc16 = ModbusCrc16(income, (inlen - 2));
	if((Value_crc16 == (((uint16_t)income[inlen - 1]<<8) | income[inlen - 2])) // crc check
		&&(MESH_FRAME_VERSION_WIFI == income[WIFI_UDP_FRAME_ADDR_VER])//
		)
	{
		ret =0;
//		ESP_LOGI(TAG, "Device_Discovery_Frame_Format_Check: rx ok");
	}

	else
	{
		ret =-1;
		ESP_LOGE(TAG, "Device_Discovery_Frame_Format_Check: rx bad frame");

	}


	return ret; /* 返回接收的功能码 */
}

/*------------------------------------------------------------------------
*@Function： Device_Discovery_Get_Frame
从UDP广播报文解析 tbd

-------------------------------------------------------------------------*/
/**
*@brief
income：UDP RX 报文
income：UDP RX 报文长度
income_ip_str： 接收报文IP
inport： 接收报文IP port

*@param[out]    cmdLen接收长度
*@return
0- ok
1- fail,no data

*/
uint8_t Device_Discovery_Get_Frame( uint8_t *income, uint16_t cmdLen, char *income_ip_str, uint16_t inport)
{
    int8_t rtn=0;
    uint8_t errFlags;
    uint16_t crc;
    uint8_t ret = 0;
    uint8_t j = 0;
    uint8_t n = 0;
    uint8_t tempi = 0;

	uint8_t SN[10];//L8=SN,H2=type seq

//    uint8_t net_point_Comein = 0;//区别全局变量，避免本函数未执行完毕即在其他任务触发其他操作
//    uint8_t Wifi_Udp_point_rx_get = 0;

    /*检查是否符合协议规范*/
    rtn = Device_Discovery_Frame_Format_Check(income, cmdLen);

	 if((-1) != rtn)
	 {
//	     ESP_LOGW(TAG, "[Device_Discovery_Get_Frame]  head:%d, cmdlen:%d", income[WIFI_UDP_FRAME_ADDR_HEAD], cmdLen);
//         ESP_LOG_BUFFER_HEX_LEVEL(TAG, income, cmdLen, ESP_LOG_WARN);

		 if(MESH_FRAME_HEADER_COMMON == income[WIFI_UDP_FRAME_ADDR_HEAD])//modbus
		 {
             /*modbus响应*/
             Modbus_MasterRespones_Udp(income, cmdLen);
		 }
		 else if((income[WIFI_UDP_FRAME_ADDR_HEAD] >= MESH_FRAME_HEADER_TRIGER)
				&&(income[WIFI_UDP_FRAME_ADDR_HEAD] <= MESH_FRAME_HEADER_FINISH_AFTER))//discovery
		 {
             /*协议封装解析*/


			 if(MESH_FRAME_HEADER_TRIGER == income[WIFI_UDP_FRAME_ADDR_HEAD])
			 {
				 reals.Step_dev_discovery = MESH_FRAME_HEADER_SEND_SN;
			 }
             else if((MESH_FRAME_HEADER_FINISH == income[WIFI_UDP_FRAME_ADDR_HEAD])
				 	||(MESH_FRAME_HEADER_FINISH_AFTER == income[WIFI_UDP_FRAME_ADDR_HEAD]))/*设备存储状态刷新*/
			 {
				 Wifi_Mesh_Protocol_Process(income, income_ip_str, inport);
				 top_dev_info_queue_Push(&reals.discovery_Info);

			 }


             /*上报帧SN解析*/
             Udp_Multicast_Finish_After(income);
		 }
	 }
     else
     {
        ret = 1;
     }

    return ret;
}




/*------------------------------------------------------------------------
*@Function： Udp_multicast_Device_Discovery_TxFrame
无线设备间（如mesh）应用层通用协议初稿20240425.xlsx
局域网设备发现帧组帧
-------------------------------------------------------------------------*/
/**
*@brief   组帧
outbuf: output buffer

step :  状态机序号

0- fail
no 0: tx len

*/
uint16_t Udp_multicast_Device_Discovery_TxFrame(uint8_t *outbuf, uint8_t step, uint8_t current_netif_id)
{
    uint16_t Crcvalue=0;
    uint16_t i = 0;
    uint8_t j = 0;
    uint16_t len = 0;
    uint16_t u16Tempdata = 0;


    outbuf[i++] = step;
    outbuf[i++] = MESH_FRAME_VERSION_WIFI;//1-WIFI
	memcpy((uint8_t*)&outbuf[i], (uint8_t*)&iot_factory.iot_sn, 8);//源 SN
	i+=8;

    //机型序号_源设备
	u16Tempdata=SN_TYPE_SELF;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;

    for (j = 0; j < 8; j++)//目标 SN
    {
        outbuf[i++] = 0;//
    }

    //机型序号_目标设备
	u16Tempdata=0;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;

    outbuf[i++] = 0;//报文类型

    //modbus协议帧区长度
	u16Tempdata=0;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;

/*
源设备网络IPV4地址:路由器决定
(顺序填充，数字内容表示，如192.168.1.2依次填充2,1,168,192这4个数字)

*/
    if((NETIF_TYPE_WIFI_STA == current_netif_id)
		||(NETIF_TYPE_WIFI_AP == current_netif_id))
    {
        for (j = 0; j < 4; j++)//源 IP
        {
//            outbuf[i++] = g_self_data.mod_reg11000_IOT_info.sta_ipv4[3-j];//
            outbuf[i++] = reals.self_wifi_ap_ip[3-j];//

        }
    }
//    else if ( current_netif_id == NETIF_TYPE_ETH )
//    {
//        for (j = 0; j < 4; j++)//源 IP
//        {
//            outbuf[i++] = g_self_data.mod_reg11000_IOT_info.IP_ETH[3-j];//MAC
//        }
//    }
    else
    {
        for (j = 0; j < 4; j++)//源 IP
        {
            outbuf[i++] = 0;//
        }
//        return 0;
    }

/*
源设备网络IPV4 服务端口号：本地可随机指定，但是必须要和实际UDP单播发送一致

*/
	u16Tempdata = UDP_PORT_SINGLE;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;


//源设备优先级
/*
初步定义建议值，数值越大，优先级越高；数值间隙预留
0-无效
AT1(ATS):30300
EMS:30200
COMBOX:30100
D100S:30000
A80/A100:20900
超级强制主设备-65535（只允许特殊短暂发送）
*/

	u16Tempdata = DEV_PRIORITY_COMBOX;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;

/*
	并机序号

	相同类型设备在局域网中的序号
	0-主设备，最高优先级
	1-默认/单机序号
	*/
	u16Tempdata = 0;

	outbuf[i++]=u16Tempdata&0xFF;
    for (j = 0; j < 10; j++)//reserved
    {
        outbuf[i++] = 0;
    }

    Crcvalue = ModbusCrc16(outbuf, i);

    outbuf[i++] = (uint8_t) Crcvalue;
    outbuf[i++] = (uint8_t)(Crcvalue>>8);

    return i;
}

/*------------------------------------------------------------------------------
 Function: Wifi_Mesh_Protocol_Process
 -----------------------------------------------------------------------------*/
/**
  * @brief      无线设备间协议wifi（udp）:局域网设备发现帧，协议封装解析
  cmdBuf RX 报文
  income_ip_str： 接收报文IP
  inport: 接收报文IP port

  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t Wifi_Mesh_Protocol_Process(const uint8_t *cmdBuf, char *income_ip_str, uint16_t inport)
{
    uint8_t ret = 0;


    memset(&reals.discovery_Info, 0, sizeof(reals.discovery_Info));
    memcpy(&reals.discovery_Info.SN[0], &cmdBuf[WIFI_UDP_FRAME_ADDR_SN_SOURCE], 10);



	if(0)//(0 != (*(uint64_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_MAC]))
	{
		memcpy(&reals.discovery_Info.MAC[0], &cmdBuf[WIFI_UDP_FRAME_ADDR_MAC], 6);//MAC地址可能变化，随时更新
	}
	else
	{
		//暂不使用局域网发现帧内ip地址及端口号，通过udp服务直接获取
		char *token;
		uint8_t i = 0;
		token = strtok(income_ip_str, ".");
		while(token != NULL)
		{
		reals.discovery_Info.MAC[3-i] = atoi(token);
		i++;
		token = strtok(NULL, ".");
		}
		reals.discovery_Info.MAC[4] = (uint8_t)inport;
		reals.discovery_Info.MAC[5] = (uint8_t)(inport >> 8);

	}

	dump_buf_global("Wifi_Mesh_Protocol_Process  cmdBuf[WIFI_UDP_FRAME_ADDR_MAC]=", &reals.discovery_Info.MAC[0], 6);

	reals.discovery_Info.net_point_online =NET_POINT_ONLINE;
	reals.discovery_Info.net_point_TimeOut_cnt =0;

#if 1

//    memcpy(&reals.discovery_Info.MAC[0], &cmdBuf[WIFI_UDP_FRAME_ADDR_MAC], 6);//源IP +port
    reals.discovery_Info.priority =((uint16_t)cmdBuf[WIFI_UDP_FRAME_ADDR_PRIORITY+1]<<8)| cmdBuf[WIFI_UDP_FRAME_ADDR_PRIORITY];

#else

    //机型序号(机型序号)
	uint16_t Type_Cnt = *((uint16_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]);

    //暂不使用局域网发现帧内ip地址及端口号，通过udp服务直接获取
    char *token;
    uint8_t i = 0;
    token = strtok(income_ip_str, ".");
    while(token != NULL)
    {
    reals.discovery_Info.MAC[3-i] = atoi(token);
    i++;
    token = strtok(NULL, ".");
    }
    reals.discovery_Info.MAC[4] = (uint8_t)inport;
    reals.discovery_Info.MAC[5] = (uint8_t)(inport >> 8);

    //暂不使用局域网发现帧内优先级，直接根据设备类型自行判断
	if(SN_TYPE_A100_WIFI == Type_Cnt)
	{
		reals.discovery_Info.priority = DEV_PRIORITY_A100;
	}
	else if(SN_TYPE_D100S == Type_Cnt)
	{
		reals.discovery_Info.priority = DEV_PRIORITY_D100S;
	}
	else if(SN_TYPE_A80 == Type_Cnt)
	{
		reals.discovery_Info.priority = DEV_PRIORITY_A100;
	}
	else if(SN_TYPE_S1 == Type_Cnt)
	{
		reals.discovery_Info.priority = DEV_PRIORITY_INVALID;
	}
    else if(SN_TYPE_COMBOX == Type_Cnt)
	{
		reals.discovery_Info.priority = DEV_PRIORITY_COMBOX;
	}
	else if(SN_TYPE_AT1 == Type_Cnt)
	{
		reals.discovery_Info.priority = DEV_PRIORITY_AT1;
	}
    else if(SN_TYPE_EBOX == Type_Cnt)
	{
		reals.discovery_Info.priority = DEV_PRIORITY_EMS;
	}
	else
	{
		ESP_LOGE(TAG, "[Udp_Multicast_Finish_After]  Native type : ERROR(%d)", Type_Cnt);
		reals.discovery_Info.priority = DEV_PRIORITY_INVALID;
	}

#endif

    if ( reals.discovery_Info.priority != DEV_PRIORITY_COMBOX )
    {
        ret = 1;
    }

    return ret;
}


/*------------------------------------------------------------------------------
 Function: Udp_Multicast_Finish_After
 -----------------------------------------------------------------------------*/
/**
  * @brief      周期上报SN信息解析
  * @param[in]  const uint8_t *cmdBuf
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t Udp_Multicast_Finish_After(const uint8_t *cmdBuf)//tbd
{
    char     iot_type[12];//

    //机型序号(机型序号)
	uint16_t Type_Cnt = *((uint16_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]);
    //SN
    uint64_t SN_64 = *((uint64_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_SN_SOURCE]);

    uint8_t slaveaddress = 0;
    uint8_t j = 0;

    /*若已存储，寻找对应存储位置*/
    for (j = 0; j < (NET_WIFI_MAX_POINT); j++)//
    {
       if(SN_64 == *((uint64_t *)reals.discovery_net_Info[j].SN))//已存储SN
       {
           slaveaddress = j + 1 + NET_SUB1G_MAX_POINT;
           break;
       }
    }

    if(!slaveaddress)//未存储
    {
        return 2;
    }



	if(SN_TYPE_A100_Sub1G == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_A100_Sub1G_ASCII,strlen(SN_TYPE_A100_Sub1G_ASCII));//SN
	}
	else if(SN_TYPE_A100_WIFI == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_A100_WIFI_ASCII,strlen(SN_TYPE_A100_WIFI_ASCII));//SN
	}
	else if(SN_TYPE_D100S == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_D100S_ASCII,strlen(SN_TYPE_D100S_ASCII));//SN
	}
	else if(SN_TYPE_A80 == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_A80_ASCII,strlen(SN_TYPE_A80_ASCII));//SN
	}
	else if(SN_TYPE_S1 == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_S1_ASCII,strlen(SN_TYPE_S1_ASCII));//SN
	}
    else if(SN_TYPE_COMBOX == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_COMBOX_ASCII,strlen(SN_TYPE_COMBOX_ASCII));//SN
	}
	else if(SN_TYPE_AT1 == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_AT1_ASCII,strlen(SN_TYPE_AT1_ASCII));//SN
	}
    else if(SN_TYPE_EBOX == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_EBOX_ASCII,strlen(SN_TYPE_EBOX_ASCII));//SN
	}
    else if(SN_TYPE_AC380_PLP023==Type_Cnt)
    {
        memcpy(&iot_type, SN_TYPE_AC380_ASCII,strlen(SN_TYPE_AC380_ASCII));//SN
    }
	else
	{
		ESP_LOGE(TAG, "[Udp_Multicast_Finish_After]  Native type : ERROR(%d)", Type_Cnt);
		return 1;
	}
//	memcpy(&Inv[slaveaddress].mod_reg11000_IOT_info.iot_type, iot_type,strlen(iot_type));//SN
//    Inv[slaveaddress].mod_reg11000_IOT_info.iot_sn = SN_64;//SN


    ESP_LOGW(TAG, "[Udp_Multicast_Finish_After]  Native type : %s",  iot_type);

    return 0;
}



/*------------------------------------------------------------------------
*@Function :Device_Discovery_Step

设备发现广播帧动作时序:基于超时进入下一阶段

*@return
0- fail
no 0: tx len
*/
void Device_Discovery_Step(uint8_t *buff, uint16_t *Len, uint8_t current_netif_id) //1s cycle
{
    uint16_t i = 0;
    static uint16_t scnt = 0;
    static uint16_t scnt2 = 0xFFFF;

    if (MESH_FRAME_HEADER_COMMON == reals.Step_dev_discovery)
    {
        reals.Step_dev_discovery = MESH_FRAME_HEADER_TRIGER;
        scnt = 0;
    }
    /*触发帧，仅首次上电设备发送，周期1s，共5s*/
    else if(MESH_FRAME_HEADER_TRIGER == reals.Step_dev_discovery)
    {
    	if(++scnt >= 5)
    	{
    		scnt = 0;
            reals.Step_dev_discovery = MESH_FRAME_HEADER_SEND_SN;
    	}
    }
    /*信息上报帧，周期1s，共15s*/
    else if(MESH_FRAME_HEADER_SEND_SN == reals.Step_dev_discovery)
    {
    	if(++scnt >= 15)
    	{
    		scnt = 0;
            reals.Step_dev_discovery = MESH_FRAME_HEADER_FINISH;
    	}
    }
    /*完成帧，仅发送一次*/
    else if(MESH_FRAME_HEADER_FINISH == reals.Step_dev_discovery)
    {
        reals.Step_dev_discovery = MESH_FRAME_HEADER_FINISH_AFTER;
        scnt2 = 0;
    }

	if((reals.Step_dev_discovery >= MESH_FRAME_HEADER_TRIGER)
		&&(reals.Step_dev_discovery <= MESH_FRAME_HEADER_FINISH))
	{
	    /*发现帧组帧*/
		*Len = Udp_multicast_Device_Discovery_TxFrame(buff, reals.Step_dev_discovery, current_netif_id);
	}
	else if(MESH_FRAME_HEADER_FINISH_AFTER == reals.Step_dev_discovery)//
	{
	    /*周期3min,tbd*/
		if(++scnt2 >= 10)//300
		{
			scnt2 =0;

            /*发现帧组帧*/
			*Len = Udp_multicast_Device_Discovery_TxFrame(buff, reals.Step_dev_discovery, current_netif_id);
		}
	    else if (0 == (scnt2 % 60))
	    {
            /*mesh网络心跳帧组帧*/
			*Len = Udp_Heartbeat_TxFrame(buff);
	    }
	}
}
//
//
//static void get_type_cnt_ascii_sub1g(uint16_t Type_Cnt, uint8_t indev)
//{
//	if(SN_TYPE_A100_Sub1G == Type_Cnt)
//	{
//		memcpy(&MicroInv[indev].mod_reg01100_Inv_base.InvType, SN_TYPE_A100_Sub1G_ASCII,strlen(SN_TYPE_A100_Sub1G_ASCII));//SN
//	}
//	else if(SN_TYPE_AT1 == Type_Cnt)
//	{
//		memcpy(&MicroInv[indev].mod_reg01100_Inv_base.InvType, SN_TYPE_AT1_ASCII,strlen(SN_TYPE_AT1_ASCII));//SN
//	}
//	else if(SN_TYPE_COMBOX == Type_Cnt)
//	{
//		memcpy(&MicroInv[indev].mod_reg01100_Inv_base.InvType, SN_TYPE_COMBOX_ASCII,strlen(SN_TYPE_COMBOX_ASCII));//SN
//	}
//	else
//	{
//		ESP_LOGE(TAG, "[get_type_cnt_ascii_sub1g]  Native type : ERROR(%d)", Type_Cnt);
//	}
//}

//
///*------------------------------------------------------------------------------
// Function: Sub_Net_Point_Delete
// -----------------------------------------------------------------------------*/
///**
//  * @brief      节点超时清除信息（将表中末尾节点设备移臭
//                Ԧͤ处）
//  * @param[in]  uint8_t target_indev
//                int8_t source_indev
//  * @param[out] None
//  * @return     static uint8_t
//  */
//static uint8_t  Sub_Net_Point_Delete(uint8_t target_indev, int8_t source_indev)
//{
//    ESP_LOGE(TAG, "[Sub_Net_Point_Delete] Delete device(mac:%llx, sn:%llu), dev(%d)",
//            reals.net_point_base_Info[target_indev].mac_union.MAC_64,
//            MicroInv[target_indev+1].mod_reg01100_Inv_base.InvSN,
//            target_indev);
//
//    if ( source_indev > target_indev )
//    {
//        if ( reals.net_point_base_Info[source_indev].net_point_online != NET_POINT_OFFLINE )
//        {
//            memcpy(&reals.net_point_base_Info[target_indev], &reals.net_point_base_Info[source_indev], sizeof(reals.net_point_base_Info[source_indev]));//clean
//            memcpy(&reals.net_point_ota_Info[target_indev], &reals.net_point_ota_Info[source_indev], sizeof(reals.net_point_ota_Info[source_indev]));
//            memcpy(&reals.net_point_power[target_indev], &reals.net_point_power[source_indev], sizeof(reals.net_point_power[source_indev]));
//            memcpy((uint8_t*)&MicroInv[target_indev+1], (uint8_t*)&MicroInv[source_indev+1], sizeof(MicroInv[source_indev+1]));
//            memcpy((uint8_t*)&MicroInv_WR[target_indev+1], (uint8_t*)&MicroInv_WR[source_indev+1], sizeof(MicroInv_WR[source_indev+1]));
//
//            memset(&reals.net_point_base_Info[source_indev], 0, sizeof(reals.net_point_base_Info[source_indev]));//clean
//            memset(&reals.net_point_ota_Info[source_indev], 0, sizeof(reals.net_point_ota_Info[source_indev]));
//            memset(&reals.net_point_power[source_indev], 0, sizeof(reals.net_point_power[source_indev]));
//            memset((uint8_t*)&MicroInv[source_indev+1], 0, sizeof(MicroInv[source_indev+1]));
//            memset((uint8_t*)&MicroInv_WR[source_indev+1], 0, sizeof(MicroInv_WR[source_indev+1]));
//
//            ESP_LOGW(TAG, "[Sub_Net_Point_Delete] target_indev (%d), source_indev(%d)", target_indev, source_indev);
//
//            return 1;
//        }
//    }
//    else
//    {
//        memset(&reals.net_point_base_Info[target_indev], 0, sizeof(reals.net_point_base_Info[target_indev]));//clean
//        memset(&reals.net_point_ota_Info[target_indev], 0, sizeof(reals.net_point_ota_Info[target_indev]));
//        memset(&reals.net_point_power[target_indev], 0, sizeof(reals.net_point_power[target_indev]));
//        memset((uint8_t*)&MicroInv[target_indev+1], 0, sizeof(MicroInv[target_indev+1]));
//        memset((uint8_t*)&MicroInv_WR[target_indev+1], 0, sizeof(MicroInv_WR[target_indev+1]));
//        ESP_LOGW(TAG, "[Sub_Net_Point_Delete] target_indev (%d)", target_indev);
//    }
//
//    return 0;
//}
//
//
///*------------------------------------------------------------------------------
// Function: Sub_Net_Point_TimeOut_Check
// -----------------------------------------------------------------------------*/
///**
//  * @brief      sub1g网络设备超时检测（1s）
//  * @param[in]  void
//  * @param[out] None
//  * @return     void
//  */
//void  Sub_Net_Point_TimeOut_Check(void)//1s cycle
//{
//	 uint8_t j = 0;
//     uint8_t order_flag = 0;
//     int8_t end_indev = reals.Subnet_point_Num - 1;
//
// 	 for (j = 0; j < NET_SUB1G_MAX_POINT; j++)//
//	 {
//	     if (reals.net_point_base_Info[j].net_point_online != NET_POINT_OFFLINE)
//	     {
//             reals.net_point_base_Info[j].net_point_TimeOut_cnt++;
//
//             /*4min未收到任何消息，视为该设备离线，清除缓存*/
//             if(reals.net_point_base_Info[j].net_point_TimeOut_cnt >= 240)//s,4min
//             {
//                 order_flag |= Sub_Net_Point_Delete(j, end_indev);
//                 end_indev--;
//             }
//             /*15s未收到任何消息，视为该设备短期离线/通讯不良，仅标记，不做处理*/
//             else if(reals.net_point_base_Info[j].net_point_TimeOut_cnt >= 15)//s
//             {
//                 if(NET_POINT_ONLINE == reals.net_point_base_Info[j].net_point_online)//s
//                 {
//                      reals.net_point_base_Info[j].net_point_online = NET_POINT_OFFLINE_HALF;
//                      ESP_LOGW(TAG, "[Sub_Net_Point_TimeOut_Check] Device(mac:%llx, sn:%llu) timeout.",
//                            reals.net_point_base_Info[j].mac_union.MAC_64,
//                            MicroInv[j+1].mod_reg01100_Inv_base.InvSN);
//                 }
//             }
//	     }
//         else
//         {
//             /*保险*/
//             if (( reals.net_point_base_Info[j].mac_union.MAC_64 != 0 )
//                || (reals.net_point_base_Info[j].net_point_type != 0))
//             {
//                 memset(&reals.net_point_base_Info[j], 0, sizeof(reals.net_point_base_Info[j]));//clean
//                 memset(&reals.net_point_ota_Info[j], 0, sizeof(reals.net_point_ota_Info[j]));
//                 memset(&reals.net_point_power[j], 0, sizeof(reals.net_point_power[j]));
//                 memset((uint8_t*)&MicroInv[j+1], 0, sizeof(MicroInv[j+1]));//clean
//                 memset((uint8_t*)&MicroInv_WR[j+1], 0, sizeof(MicroInv_WR[j+1]));//clean
//
//                 ESP_LOGE(TAG, "[Sub_Net_Point_TimeOut_Check] clean offline device");
//             }
//         }
//	 }
//
//     if ( order_flag != 0 )
//     {
//         /*产生移位，立即排序*/
//         Sub_Net_Point_Serial_Order(1);
//     }
//}

/*------------------------------------------------------------------------------
 Function: Top_Net_Point_Delete
 -----------------------------------------------------------------------------*/
/**
  * @brief      target_indev 节点超时清除信息：
  将表中末尾节点在线设备source_indev移到target_indev位置
  * @param[in]  uint8_t target_indev
                int8_t source_indev
  * @param[out] None
  * @return     1-发生序号调换，需要重排
  */


static uint8_t  Top_Net_Point_Delete(uint8_t target_indev, int8_t source_indev)
{
    ESP_LOGE(TAG, "[Top_Net_Point_Delete] Delete device(sn:%llu), dev(%d)", *((uint64_t *)&reals.discovery_net_Info[target_indev].SN[0]), target_indev);
    if ( source_indev > target_indev )
    {
        if ( reals.discovery_net_Info[source_indev].net_point_online != NET_POINT_OFFLINE )//尾部的变量移动到前面，尾部清零
        {
            memcpy(&reals.discovery_net_Info[target_indev], &reals.discovery_net_Info[source_indev], sizeof(reals.discovery_net_Info[source_indev]));

//先清除discovery_net_Info[],再清除discovery_net_Info[].ptr_modbus_data指向的mosbus
			if(NULL != reals.discovery_net_Info[source_indev].ptr_modbus_data)
			{
                //reals.discovery_net_Info[source_indev].ptr_modbus_data=NULL;
				memset((uint8_t*)reals.discovery_net_Info[source_indev].ptr_modbus_data, 0, reals.discovery_net_Info[source_indev].modbus_data_len);//clean
			}
            memset((uint8_t*)&(reals.discovery_net_Info[source_indev]), 0, sizeof(DISCOVERY_POINT_InfoStruct));//clean


            ESP_LOGW(TAG, "[Top_Net_Point_Delete] target_indev (%d), source_indev(%d)", target_indev, source_indev);

            return 1;
        }
    }
    else
    {
		if(NULL != reals.discovery_net_Info[target_indev].ptr_modbus_data)
		{
           // ESP_LOGI(TAG,"modbus_data_len:%u",reals.discovery_net_Info[target_indev].modbus_data_len);
			//reals.discovery_net_Info[target_indev].ptr_modbus_data=NULL;
            memset((uint8_t*)reals.discovery_net_Info[target_indev].ptr_modbus_data, 0, reals.discovery_net_Info[target_indev].modbus_data_len);//clean
		}

        memset((uint8_t*)&(reals.discovery_net_Info[target_indev]), 0, sizeof(DISCOVERY_POINT_InfoStruct));//clean
        ESP_LOGW(TAG, "[Top_Net_Point_Delete] target_indev (%d)", target_indev);
    }

    return 0;
}
//}

/*------------------------------------------------------------------------------
 Function: Top_Net_Point_TimeOut_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      wifi网络设备超时检测（1s）
  * @param[in]  void
  * @param[out] None
  * @return     void  T
  */
 void  Top_Net_Point_TimeOut_Check(void)//1s cycle
 {
	 uint8_t j =0;
     uint8_t order_flag = 0;
     int8_t end_indev = 0;//有符号
     uint8_t TempDevSn[20] = {0};//

	 end_indev = reals.Topnet_point_Num - 1;


     for (j = 0; j < NET_WIFI_MAX_POINT; j++)//
	 {
	     if (reals.discovery_net_Info[j].net_point_online != NET_POINT_OFFLINE)
	     {
             reals.discovery_net_Info[j].net_point_TimeOut_cnt++;

             /*5min未收到任何消息，视为该设备离线，清除缓存*/
             if(reals.discovery_net_Info[j].net_point_TimeOut_cnt >= 30)//s,60*6
             {
                 order_flag |= Top_Net_Point_Delete(j, end_indev);//从尾部倒序清除
                 end_indev--;
             }
             /*60s未收到任何消息，视为该设备短期离线/通讯不良，仅标记，不做处理*/
             else if(reals.discovery_net_Info[j].net_point_TimeOut_cnt >= 15)//60s
             {
                 if(NET_POINT_ONLINE == reals.discovery_net_Info[j].net_point_online)//s
                 {
                      reals.discovery_net_Info[j].net_point_online = NET_POINT_OFFLINE_HALF;
                      ESP_LOGW(TAG, "[Top_Net_Point_TimeOut_Check] Device(sn:%llu) timeout.", *((uint64_t *)&reals.discovery_net_Info[j].SN[0]));
                 }
             }
	     }
         else//offline
         {
             /*保险*/
             if((0 == CompareSetData((uint8_t *)&reals.discovery_net_Info[j].MAC[0], (uint8_t *)&TempDevSn[0], 6))//非0，有节点内容
                || (0 == CompareSetData((uint8_t *)&reals.discovery_net_Info[j].SN[0], (uint8_t *)&TempDevSn[0], 10)))
             {
				if(NULL != reals.discovery_net_Info[j].ptr_modbus_data)
				{
                    //reals.discovery_net_Info[j].ptr_modbus_data=NULL;
					memset((uint8_t*)reals.discovery_net_Info[j].ptr_modbus_data, 0, reals.discovery_net_Info[j].modbus_data_len);//clean
				}
                 memset((uint8_t*)&(reals.discovery_net_Info[j]), 0, sizeof(DISCOVERY_POINT_InfoStruct));//clean
//                 memset((uint8_t*)&MicroInv[j+1+NET_SUB1G_MAX_POINT], 0, sizeof(MicroInv[j+1+NET_SUB1G_MAX_POINT]));//clean
//                 memset((uint8_t*)&MicroInv_WR[j+1+NET_SUB1G_MAX_POINT], 0, sizeof(MicroInv_WR[j+1+NET_SUB1G_MAX_POINT]));//clean
//				 memset((uint8_t*)&Plug[j+1], 0, sizeof(Plug[j+1]));//clean

                 ESP_LOGE(TAG, "[Top_Net_Point_TimeOut_Check] clean offline device");
             }
         }
	 }

     if ( order_flag != 0 )
     {
         /*产生移位，立即排序*/
//         Top_Net_Point_Serial_Order(1);
     }
 }


/*------------------------------------------------------------------------------
Function: Sub_Net_Point_Clean
-----------------------------------------------------------------------------*/
/**
* @brief      清零wifi设备缓存
* @param[in]  void
* @param[out] None
* @return     void    Su
*/
void  Top_Net_Point_Clean(void)
{
    uint16_t j =0;

    for (j = 0; j < NET_WIFI_MAX_POINT; j++)//
    {
        memset((uint8_t*)&(reals.discovery_net_Info[j]), 0, 8);//右侧位置清零
        reals.discovery_net_Info[j].net_point_online =NET_POINT_OFFLINE;
        reals.discovery_net_Info[j].net_point_TimeOut_cnt =0xFFFF;
//        memset((uint8_t*)&MicroInv[j+1+NET_SUB1G_MAX_POINT], 0, sizeof(MicroInv[j+1+NET_SUB1G_MAX_POINT]));//clean
//        memset((uint8_t*)&MicroInv_WR[j+1+NET_SUB1G_MAX_POINT], 0, sizeof(MicroInv_WR[j+1+NET_SUB1G_MAX_POINT]));//clean
//		memset((uint8_t*)&Plug[j], 0, sizeof(Plug[j]));//clean

    }




	for (j = 0; j < NET_WIFI_MAX_POINT; j++)//
	{
		if(NET_POINT_OFFLINE == reals.discovery_net_Info[j].net_point_online)
		{
            //reals.discovery_net_Info[j].ptr_modbus_data=NULL;
		    memset((uint8_t*)reals.discovery_net_Info[j].ptr_modbus_data, 0, reals.discovery_net_Info[j].modbus_data_len);//clean
		}
	}



    top_dev_info_queue_init();

    reals.Topnet_point_Num = 0;
    reals.Topnet_point_Num_S1 = 0;
    reals.Topnet_point_Num_mix = 0;
    reals.Topnet_point_Num_invbat = 0;



    reals.net_point_Comein = 0;
}


  /*------------------------------------------------------------------------------
   Function: Top_Net_Point_Rx_Get
   -----------------------------------------------------------------------------*/
  /**
    * @brief      设备存储状态刷新:更新reals.discovery_net_Info[j]
    * @param[in]  void
    * @param[out] None
    * @return
    1-有新设备SN
    */
  uint8_t Top_Net_Point_Rx_Get(DISCOVERY_POINT_InfoStruct *queue_msg)
  {
     uint8_t j;
     uint8_t n;

    for (j = 0; j < NET_WIFI_MAX_POINT; j++)//新设备填充
    {
        if (memcmp( queue_msg->SN, reals.discovery_net_Info[j].SN, 10) == 0)//same
        {
            if(reals.discovery_net_Info[j].net_point_online == NET_POINT_OFFLINE_HALF)
            {
                 ESP_LOGW(TAG, "[Top_Net_Point_Rx_Get] device update. sn(%llu)", *((uint64_t *)&reals.discovery_net_Info[j].SN[0]));
            }

            reals.discovery_net_Info[j].net_point_online = NET_POINT_ONLINE;
			if(0 != (*(uint64_t *)&queue_msg->MAC[0]))
			{
				memcpy(&reals.discovery_net_Info[j].MAC[0], &queue_msg->MAC[0], 6);//MAC地址可能变化，随时更新
			}
            reals.discovery_net_Info[j].net_point_TimeOut_cnt = 0;

            return 0;//已存设备，不再填充
        }
        else if(NET_POINT_OFFLINE == reals.discovery_net_Info[j].net_point_online)//新设备填充到前面，不在线的设备位置
        {


            memcpy(&reals.discovery_net_Info[j].SN[0], &queue_msg->SN[0], 10);
			if(0 != (*(uint64_t *)&queue_msg->MAC[0]))
			{
				memcpy(&reals.discovery_net_Info[j].MAC[0], &queue_msg->MAC[0], 6);
			}

			if(0 != queue_msg->priority)
			{
				reals.discovery_net_Info[j].priority = queue_msg->priority;
			}

            reals.discovery_net_Info[j].net_point_online = NET_POINT_ONLINE;
            reals.discovery_net_Info[j].net_point_TimeOut_cnt =0;

//			for (n = j+1; n < NET_WIFI_MAX_POINT; n++)//设备填充
//			{
//				 /*该if逻辑理论上不会再进入*/
//				 if(memcmp(queue_msg->SN, reals.discovery_net_Info[n].SN, 10) == 0)//原来已存储，转移并清除尾部旧位置数据
//				 {
//					 memset(&reals.discovery_net_Info[n], 0, sizeof(DISCOVERY_POINT_InfoStruct));
//					 reals.discovery_net_Info[n].net_point_online = NET_POINT_OFFLINE;
//					 reals.discovery_net_Info[n].net_point_TimeOut_cnt = UINT_MAX;
//
//
//
//					 memcpy(&reals.discovery_net_Info[j], &reals.discovery_net_Info[n], sizeof(reals.discovery_net_Info[n]));
//					 reals.discovery_net_Info[j].ptr_modbus_data  =reals.discovery_net_Info[n].ptr_modbus_data;
//
//					 memcpy((uint8_t*)reals.discovery_net_Info[j].ptr_modbus_data,(uint8_t*)reals.discovery_net_Info[n].ptr_modbus_data, reals.discovery_net_Info[j].modbus_data_len);//clean
//					 if(NULL != reals.discovery_net_Info[source_indev].ptr_modbus_data)
//					 {
//						 memset((uint8_t*)reals.discovery_net_Info[source_indev].ptr_modbus_data, 0, reals.discovery_net_Info[source_indev].modbus_data_len);//clean
//					 }
//					 memset((uint8_t*)&(reals.discovery_net_Info[source_indev]), 0, sizeof(DISCOVERY_POINT_InfoStruct));//clean
//
//					 ESP_LOGE(TAG, "[Top_Net_Point_Rx_Get] repeat device error");
//
//					 break;
//				 }
//			}

            ESP_LOGW(TAG, "[Top_Net_Point_Rx_Get] Get new device(sn:%llu), dev(%d)", *((uint64_t *)&reals.discovery_net_Info[j].SN[0]), j);
            return 1;//完成存储
        }
    }

    return 0;//在线设备已满
  }



  /*------------------------------------------------------------------------------
   Function: Top_Net_Point_Bubble_Sort
   -----------------------------------------------------------------------------*/
  /**
    * @brief      设备SN冒泡排序，更新reals.Topseq_index[j]
    * @param[in]  void
    * @param[out] None
    * @return
    1-排序后，要更新上报
    */
uint8_t  Top_Net_Point_Bubble_Sort(void)
{
    uint8_t i;
    uint8_t j;
    uint8_t temp;
    uint8_t ret = 0;

    for (i = 0; i < (NET_WIFI_MAX_POINT -1); i++)//
    {
        if(NET_POINT_OFFLINE == reals.discovery_net_Info[i].net_point_online)//设备不在线，认为前面只要有设备不在线，之后的设备就都不在线
        {
            break;
        }
        for (j = 1; j < (NET_WIFI_MAX_POINT - i); j++)//
        {
            if((0 != *(uint64_t*)&reals.discovery_net_Info[reals.Topseq_index[j]].SN[0])//排除空节点
               &&(*(uint64_t*)&reals.discovery_net_Info[reals.Topseq_index[j-1]].SN[0] > *(uint64_t*)&reals.discovery_net_Info[reals.Topseq_index[j]].SN[0])//相邻两个数如果逆序，则交换位置(从小到大排序)
               )
            {
                temp = reals.Topseq_index[j-1];
                reals.Topseq_index[j-1] = reals.Topseq_index[j];
                reals.Topseq_index[j] = temp;

                ret=1;//排序后，更新上报
            }
            else if((0 == *(uint64_t*)&reals.discovery_net_Info[reals.Topseq_index[j]].SN[0])//空节点按最大做排序，即排到末尾
              &&(*(uint64_t*)&reals.discovery_net_Info[reals.Topseq_index[j-1]].SN[0] < *(uint64_t*)&reals.discovery_net_Info[reals.Topseq_index[j]].SN[0])//相邻两个数不全为零，且前者为零，此时互换
              )
            {
                temp = reals.Topseq_index[j-1];
                reals.Topseq_index[j-1] = reals.Topseq_index[j];
                reals.Topseq_index[j] = temp;


                ret=1;//排序后，更新上报
            }
        }
    }

    return ret;
}

  /*------------------------------------------------------------------------------
   Function: Top_Net_Point_Type_Sort
   -----------------------------------------------------------------------------*/
  /**
    * @brief      设备SN的分类筛选，区分 INV_bat(储能型)，micro_inv(不含电池型)，S1，其他个别种类
    不同类型设备存放不同表格
    * @param[in]  void
    * @param[out] None
    * @return     uint8_t  Top
    */
void  Top_Net_Point_Type_Sort(void)
{
    uint8_t i;
    uint8_t num_S1=0;
    uint8_t num_invbat=0;
    uint8_t num_mix=0;
    //机型序号(机型序号)
	uint16_t Type_Cnt ;
//    uint8_t temparray[50];
	uint8_t Topseq_Invbat_index[NET_WIFI_INVBAT_POINT];//
	uint8_t Topseq_S1_index[NET_WIFI_S1_POINT];//
	uint8_t Topseq_mix_index[NET_WIFI_MIX_POINT];//

	memset(Topseq_Invbat_index, 0, sizeof(Topseq_Invbat_index));
	memset(Topseq_S1_index, 0, sizeof(Topseq_S1_index));
	memset(Topseq_mix_index, 0, sizeof(Topseq_mix_index));


    for (i = 0; i < (NET_WIFI_MAX_POINT -1); i++)//
    {
		if(NET_POINT_OFFLINE != reals.discovery_net_Info[reals.Topseq_index[i]].net_point_online)
		{
			Type_Cnt= *((uint16_t *)&reals.discovery_net_Info[reals.Topseq_index[i]].SN[8]);

			if(SN_TYPE_S1 == Type_Cnt)
			{
				reals.discovery_net_Info[reals.Topseq_index[i]].ptr_modbus_data = (uint8_t *)&Plug[num_S1];
				reals.discovery_net_Info[reals.Topseq_index[i]].modbus_data_len	=sizeof(Plug[0]);
				Topseq_S1_index[num_S1++] = reals.Topseq_index[i];

				if(num_S1 > NET_WIFI_S1_POINT)
				{
					num_S1 = NET_WIFI_S1_POINT;
				}
			}
            #if 0
			else if(SN_TYPE_SELF == Type_Cnt)
			{
				reals.discovery_net_Info[reals.Topseq_index[i]].ptr_modbus_data = (uint8_t *)&Inv[num_invbat];
				reals.discovery_net_Info[reals.Topseq_index[i]].modbus_data_len	=sizeof(Inv[0]);
				Topseq_Invbat_index[num_invbat++] = reals.Topseq_index[i];
				if(num_invbat > NET_WIFI_INVBAT_POINT)
				{
					num_invbat = NET_WIFI_INVBAT_POINT;
				}
			}
            #endif
			else//mix
			{
				reals.discovery_net_Info[reals.Topseq_index[i]].ptr_modbus_data = NULL;//tbd (uint8_t *)&Plug[num_mix];
				reals.discovery_net_Info[reals.Topseq_index[i]].modbus_data_len	=0;	//tbd
				Topseq_mix_index[num_mix++] = reals.Topseq_index[i];
				if(num_mix > NET_WIFI_MIX_POINT)
				{
					num_mix = NET_WIFI_MIX_POINT;
				}
				//ESP_LOGE(TAG, "[Top_Net_Point_Type_Sort]  Native type : ERROR(%d)", Type_Cnt);
//				return 1;
			}
		}
	}

	memcpy(reals.Topseq_Invbat_index, Topseq_Invbat_index, sizeof(Topseq_Invbat_index));
	memcpy(reals.Topseq_S1_index, Topseq_S1_index, sizeof(Topseq_S1_index));
	memcpy(reals.Topseq_mix_index, Topseq_mix_index, sizeof(Topseq_mix_index));

    reals.Topnet_point_Num_S1 = num_S1;
    reals.Topnet_point_Num_mix = num_mix;
    reals.Topnet_point_Num_invbat = num_invbat;
	Plug[PLUG_MAX_NUM].mod_reg14500_SmartPlug_info.SmartPlug_Nums =reals.Topnet_point_Num_S1;




}
/*------------------------------------------------------------------------
*@Function： Top_Net_Point_Serial_Order
WIFI网络节点SN地址排序
在线设备在前，不在线设备在后

20ms cycle
-------------------------------------------------------------------------*/
/**
*@brief
*@param[in]	 net_point_update:1-强制新设备更新


*@return

*/
void	Top_Net_Point_Serial_Order(uint8_t net_point_update)
{
  uint8_t i;
  uint8_t j;
  uint8_t n;
  uint8_t temp;//NET_POINT_InfoStruct
  uint8_t net_point_Num_temp = 0;//在线的设备节点
  static uint16_t scnt_10ms = 0;
  static uint16_t scnt_sort = 0;//排序延时计数器

  uint8_t net_point_Comein = 0;//区别全局变量，避免本函数未执行完毕即在其他任务触发其他操作
  net_point_Comein |= net_point_update;

  DISCOVERY_POINT_InfoStruct *queue_msg = NULL;

  /*设备存储状态刷新*/
//  if(xQueue_new_wifi_dev_info && (xQueueReceive(xQueue_new_wifi_dev_info, &queue_msg, 0) == pdTRUE))
//  {
//       net_point_Comein = Top_Net_Point_Rx_Get(queue_msg);
//       if ( queue_msg != NULL )
//       {
//           free(queue_msg);
//           queue_msg = NULL;
//       }
//  }
//  /*设备信息变化*/
//  if (net_point_Comein == 1)
//  {
//      reals.net_point_Comein = 1;
//  }

  /*存量设备排序刷新，慢速*/
  if((++scnt_sort >= 250)||(net_point_Comein == 1))//50ms*100
  {
	  scnt_sort=0;
//	  net_point_Comein |= Top_Net_Point_Bubble_Sort();//windy tbd排序目的是什么？？？
  }

  /*设备数量刷新*/
  for (j = 0; j < NET_WIFI_MAX_POINT; j++)//
  {
	  if(NET_POINT_OFFLINE != reals.discovery_net_Info[j].net_point_online)//统计在线数量设备在线，在尾部追加新设备
	  {
		  net_point_Num_temp++;
	  }
  }
  reals.Topnet_point_Num = net_point_Num_temp;
  Top_Net_Point_Type_Sort();

  /*设备信息变化*/
  if (net_point_Comein == 1)
  {
      reals.net_point_Comein = 1;
  }


}


  /*------------------------------------------------------------------------
  *@Function： Modbus_21000_1_Net_Point_Frame
绑定帧组帧

 按照排序后的变量内容顺序上报
包含 WIFI和Sub 1GHz两个网络的设备节点信息，顺序排列

 50ms cycle
  -------------------------------------------------------------------------*/
  /**
  *@brief
  *@param[in]	  *income

  *@return

  */
uint8_t Modbus_21000_1_Net_Point_Frame(void)
{
    uint16_t bias=0;
    uint16_t j=0;
    uint16_t m =0;
    uint16_t n =0;
    uint8_t TempDevSn[20] = {0};//


    memset( Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info,0,sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info));

    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.self_bind_info.group_same_type_addr= 0;//预留
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.self_bind_info.group_addr = 1;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.self_bind_info.slave_addr = 0;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.self_bind_info.state.bit.point_online = 1;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.self_bind_info.SN_64 =g_self_data.mod_reg11000_IOT_info.iot_sn;//g_self_data.mod_reg11000_IOT_info.iot_sn;
    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.self_bind_info.Dev_Type = SN_TYPE_SELF;

    ///////////////////////////////////0 tbd
    if (reals.online_ACHUB_num == 0)		// 无ACHUB设备在线时 逆变器识别为主机
    {
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_same_type_addr= DEFAULT_SALVE_ADDR;//预留
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_addr = DEFAULT_SALVE_ADDR;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].slave_addr = DEFAULT_SALVE_ADDR;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].state.all=INV_Point_State_Update(INV_MAX_NUM*DEV_MAIN_NODE_MAX,INV_MAX_NUM);
        //Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].state.bit.point_online=1;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64=SetData.dev_info_t.INV_dev_sn;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_SELF;
        ESP_LOGI(TAG,"21000_1_Net_Point_Frame AP300 Sn:%llu", Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64);
        bias++;
    }
    else if (reals.online_ACHUB_num )		// ACHUB设备在线时 ACHUB识别为主机
    {
        reals.achubPointInfos.slaveAddr=DEFAULT_SALVE_ADDR;
        reals.achubPointInfos.groupIndex=DEFAULT_SALVE_ADDR;
        reals.achubPointInfos.sumAddr=DEFAULT_SALVE_ADDR;

        reals.achubPointInfos.pointState.all=INV_AcHub_State_Update();
        //reals.achubPointInfos.pointState.state.point_online=1;

        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_same_type_addr= reals.achubPointInfos.sumAddr;//预留
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_addr = reals.achubPointInfos.groupIndex;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].slave_addr = reals.achubPointInfos.slaveAddr;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].state.all =  INV_AcHub_State_Update();
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 =  SetData.dev_info_t.Parallel_dev_sn;
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_ACHUB;
        ESP_LOGI(TAG,"21000_1_Net_Point_Frame ACHUB Sn:%llu",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64);
        ESP_LOGI(TAG,"achub state.all:0x%x",reals.achubPointInfos.pointState.all);
        //*pointcnt++;
        bias++;

        for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
        {
            for (int i = 0; i < INV_MAX_NUM; i++)
            {
                if(Inv_can[node].inv_data[i].online)
                {
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_same_type_addr= INV_SALVE_ADDR1+node;//预留
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_addr = INV_SALVE_ADDR1+node;
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].slave_addr = INV_SALVE_ADDR1+node;
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].state.all=INV_Point_State_Update(node,i);
                    //Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].state.bit.point_online=1;
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64=Inv_can[node].inv_data[i].inv_about.dev_sn ;
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_SELF;
                    ESP_LOGI(TAG,"21000_1_Net_Point_Frame index:%d AP300 Sn:%llu", node ,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64);
                    bias++;
                }
            }

        }
    }

        /*DC_HUB节点信息*/
    ///////////////////////////////////0 tbd
    if (reals.online_DCHUB_num )		// 该总线有ACHUB设备在线才回读数据
    {
        for(j=0;j<DEV_MAIN_NODE_MAX;j++)
        {
            if(Inv_can[j].dc_hub_data[reals.online_Y_inv_index].online)
            {
                reals.dchubPointInfos.slaveAddr=DCHUB_SALVE_ADDR+j;
                reals.dchubPointInfos.groupIndex=DCHUB_SALVE_ADDR+j;
                reals.dchubPointInfos.sumAddr=DCHUB_SALVE_ADDR+j;
                reals.dchubPointInfos.pointState.all=INV_DcHub_State_Update();
                ESP_LOGI(TAG,"DCHUB State:0x%x",reals.dchubPointInfos.pointState.all);
                //reals.dchubPointInfos.pointState.state.point_online=1;
                uint64_t dcSn=GetUin64FromPtrSmall(Inv_can[j].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn);
                //ESP_LOGI(TAG,"21000_1_Net_Point_Frame DCHUB Sn:%u-%u-%u-%u",Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[0],
                        //Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[1],
                        //Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[2],
                        //Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[3]);
                ESP_LOGI(TAG,"21000_1_Net_Point_Frame DCHUB Sn[%d]:%llu",j,dcSn);

                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_same_type_addr= reals.dchubPointInfos.sumAddr;//预留
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_addr = reals.dchubPointInfos.groupIndex;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].slave_addr = reals.dchubPointInfos.slaveAddr;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].state.all =  reals.dchubPointInfos.pointState.all;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 =  dcSn;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_DCHUB;
                bias++;
            }
        }
    }


    /*D400s节点信息*/
    ///////////////////////////////////0 tbd
    if (reals.online_D400S_num )		// 该总线有ACHUB设备在线才回读数据
    {
        for(j=0;j<DEV_MAIN_NODE_MAX;j++)
        {
            if(Inv_can[j].d400s_data[reals.online_Y_inv_index].online)
            {
                reals.d400sPointInfos[j].slaveAddr=D400S_SALVE_ADDR+j;
                reals.d400sPointInfos[j].groupIndex=D400S_SALVE_ADDR+j;
                reals.d400sPointInfos[j].sumAddr=D400S_GROUP_ADDR;
                //reals.d400sPointInfos[j].pointState.state.point_online=1;
                // reals.d400sPointInfos[j].pointState.all=INV_D400s_State_Update();
                ESP_LOGI(TAG,"D400s :%d State:0x%x",j,reals.d400sPointInfos[j].pointState.all);
                uint64_t d400sSn=GetUin64FromPtrSmall(Inv_can[j].d400s_data[reals.online_Y_inv_index].d400s_common_info.d400s_sn);
                //ESP_LOGI(TAG,"21000_1_Net_Point_Frame DCHUB Sn:%u-%u-%u-%u",Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[0],
                        //Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[1],
                        //Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[2],
                        //Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn[3]);
                ESP_LOGI(TAG,"21000_1_Net_Point_Frame D400S Sn[%d]:%llu",j,d400sSn);

                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_same_type_addr= reals.d400sPointInfos[j].sumAddr;//预留
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].group_addr = reals.d400sPointInfos[j].groupIndex;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].slave_addr = reals.d400sPointInfos[j].slaveAddr;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].state.all =  reals.d400sPointInfos[j].pointState.all;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 =  d400sSn;
                if (strcmp(Inv_can[j].d400s_data[reals.online_Y_inv_index].d400s_common_info.d400s_type, SN_TYPE_CHARGER2_ASCII) == 0) {
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_CHARGER2;
                } else {
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_D400S;
                }
                bias++;
            }
        }
    }

    /*电池包节点信息*/
    /////////////////////////////////////invbat

    if(reals.packPoinitNum)
    {
        char type[TYPE_SIZE];
        ESP_LOGI(TAG,"21000_1_Net_Point_Frame reals.packPoinitNum:%d",(unsigned int)reals.packPoinitNum);
        //reals.packPointInfos[j].pointState.bit.update_need |= Get_http_new_version_flag(HTTPS_CHECK_PACK_IMAGE, 0) ;
        for (j = 0; j < reals.packPoinitNum; j++)//
        {
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].group_same_type_addr= reals.packPointInfos[j].sumAddr;//预留
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].group_addr = reals.packPointInfos[j].groupIndex;
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].slave_addr = reals.packPointInfos[j].slaveAddr;
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].state.all=reals.packPointInfos[j].pointState.all;
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].SN_64=Inv_Pack_Slave[reals.packPointInfos[j].slaveIndex].mod_reg06100_Pack_each.sn_code;
            ESP_LOGI(TAG,"j:%d,sumAddr:0x%x,slaveAddr:0x%x,pointState:0x%x,SN:%llu",j,reals.packPointInfos[j].sumAddr,reals.packPointInfos[j].slaveAddr,reals.packPointInfos[j].pointState.all,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].SN_64);
            strncpy(type, Inv_Pack_Slave[reals.packPointInfos[j].slaveIndex].mod_reg06100_Pack_each.type_ascii, TYPE_SIZE);

            if(strcmp(type,"B300") == 0)
            {
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].Dev_Type = SN_TYPE_B300;
            }
		    else if(strcmp(type,"B300K") == 0)
            {
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].Dev_Type = SN_TYPE_B300K;
            }
            else if(strcmp(type,"B500K") == 0)
            {
			     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].Dev_Type = SN_TYPE_B500K;
            }
		    else if(strcmp(type,"B300S") == 0)
            {
			     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].Dev_Type = SN_TYPE_B300S;
            }
            else {
                uint16_t dev_type = SN_TYPE_ASCII_TO_NUM(type);
                dev_type = dev_type != 0 ? dev_type : SN_TYPE_COMMON_PACK;
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias+j].Dev_Type = dev_type;
            }

        }
    }
	bias+=j;

    return bias;
}


/*------------------------------------------------------------------------------
 Function: Modbus_21000_3_Net_Point_Frame
 -----------------------------------------------------------------------------*/
/**
 * @brief      借用绑定帧上报升级进度
 * @param[in]  void
 * @param[out] None
 * @return     void Mo
 */
uint8_t Modbus_21000_3_Net_Point_Frame(void)
{
    uint16_t i=0;
    uint16_t j=0;
    uint8_t bias=0;
    uint32_t softVersion[PACK_SOFT_NUM]={0};
    //ESP_LOGW(TAG,"test get in Modbus_21000_3_Net_Point_Frame");
    ESP_LOGW(TAG,"ota_cmd.type:%u,group:%u, reals.preSoftVersion;:%d",(unsigned int)g_self_data.mod_reg00700_OTA.ota_cmd.type,g_self_data.mod_reg00700_OTA.ota_cmd.group.all,(unsigned int)reals.preSoftVersion);

    switch(g_self_data.mod_reg00700_OTA.ota_cmd.type)
    {
        case DEVICE_IOT:
            {
                switch (g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type)
                {
                case GROUP_IOT:
                {
                    for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                    {
                        if(Inv_can[i].iot_data[0].online)
                        {
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can[i].iot_data[0].mod_reg11000_IOT_info.iot_sn;
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_SELF;//设备简化型号，AP300暂用6?
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[IOT_CAN_ADDR +i].errCode;
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                            if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                            {
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                            }else
                            {
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                            }
                            bias++;
                        }
                    }
                    //Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].SN_64 = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.iot_sn;
                    //Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].Dev_Type = SN_TYPE_SELF;//设备简化型号，AP300暂用6?
                }
                    break;
                case GROUP_CHARGE:
                {
                    for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                    {
                        if(Inv_can[i].d400s_data[0].online)
                        {
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can[i].d400s_data[0].iot_can_11000.iot_sn;
                            if (strcmp(Inv_can[j].d400s_data[reals.online_Y_inv_index].d400s_common_info.d400s_type, SN_TYPE_CHARGER2_ASCII) == 0) {
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_CHARGER2;
                            } else {
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_D400S;
                            }
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[INV_CAN_ADDR +i].errCode;
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                            if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                            {
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                            }else
                            {
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                            }
                            bias++;
                        }
                    }
                }
                default:
                    break;
                }

            }
            break;
        case DEVICE_DSP:
            {
                switch (g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type)
                {
                    case GROUP_INV:
                        for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                        {
                            if(Inv_can[i].inv_data[0].online)
                            {
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can[i].inv_data[0].inv_about.dev_sn;
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_SELF;
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[INV_CAN_ADDR +i].errCode;
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                                if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                                }else
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                                }
                                bias++;
                            }
                        }
                        // for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                        // {
                        //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].SN_64 = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.InvSN;
                        //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].Dev_Type = SN_TYPE_SELF;//设备简化型号，AP300暂用6?d
                        // }
                    break;
                    case GROUP_CHARGE:
                        for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                        {
                            if(Inv_can[i].d400s_data[0].online)
                            {
                                //Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can[i].d400s_data[0].d400s_common_info.d400s_sn;
                                //Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_SELF;
                                uint64_t d400s_Sn=GetUin64FromPtrSmall(&Inv_can[i].d400s_data[0].d400s_common_info.d400s_sn[0]);
                                ESP_LOGI(TAG,"i:%d,d400s_Sn:%llu",i,d400s_Sn);
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = d400s_Sn;
                                if (strcmp(Inv_can[j].d400s_data[reals.online_Y_inv_index].d400s_common_info.d400s_type, SN_TYPE_CHARGER2_ASCII) == 0) {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_CHARGER2;
                                } else {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_D400S;
                                }
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[INV_CAN_ADDR +i].errCode;
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                                if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                                }else
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                                }
                                bias++;
                            }
                        }
                        // uint64_t d400s_Sn=GetUin64FromPtrSmall(&Inv_D400S[DEV_MAIN_NODE_MAX].mod_reg15500_D400s_info.d400s_sn[0]);
                        // ESP_LOGI(TAG,"d400s_Sn:%llu",d400s_Sn);
                        // Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].SN_64 = d400s_Sn;
                        // Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].Dev_Type = SN_TYPE_D400S;//设备简化型号，AP300暂用6?
                    break;
                }
                // Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].errCode = can_ota_status[0].ota_summary.errCode;
                // Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].soft_version = reals.preSoftVersion;
            }
            break;
        case DEVICE_ARM:
            for(i=0;i<DEV_MAIN_NODE_MAX;i++)
            {
                if(Inv_can[i].inv_data[0].online)
                {
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can[i].inv_data[0].inv_about.dev_sn;
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_SELF;
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[INV_CAN_ADDR +i].errCode;
                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                    if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                    {
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                    }else
                    {
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                    }
                    bias++;
                }
            }
            break;
        case DEVICE_AC_HUB:
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].ota_summary.errCode;

            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can_mix.ac_hub_data[0].ac_hub_info.ac_hub_sn;//Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15800_Ac_Hub_info.ac_hub_sn;
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_ACHUB;

            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
            if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
            {
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
            }else
            {
                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
            }
            bias++;
            break;
        case DEVICE_DC_HUB:
            {
                uint64_t dcSn;
                for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                {
                    if(Inv_can[i].dc_hub_data[reals.online_Y_inv_index].online)
                    {
                        dcSn=GetUin64FromPtrSmall(Inv_can[i].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn);
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = dcSn;
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_DCHUB;
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[INV_CAN_ADDR +i].errCode;
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                        if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                        {
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                        }else
                        {
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                        }
                        bias++;
                    }
                }

            //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].errCode = can_ota_status[0].ota_summary.errCode;

            //     for(j=0;j<DEV_MAIN_NODE_MAX;j++)
            //     {
            //         if(Inv_can[j].dc_hub_data[reals.online_Y_inv_index].online)
            //         {
            //             dcSn=GetUin64FromPtrSmall(Inv_can[j].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn);
            //             ESP_LOGI(TAG,"Modbus_21000_3_Net_Point_Frame DCHUB Sn[%d]:%llu",j,dcSn);
            //         }
            //     }

            // // uint64_t dcSn=GetUin64FromPtrBig(Inv_can[0].dc_hub_data[reals.online_Y_inv_index].dc_hub_info.dc_hub_sn);
            //     ESP_LOGI(TAG,"dcSn:%d",(unsigned int)dcSn);
            //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].SN_64 =dcSn;
            //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].Dev_Type = SN_TYPE_DCHUB;//设备简化型号，AP300暂用6?d
            //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].soft_version = reals.preSoftVersion;
            }
            break;
        case DEVICE_BMS:
        {
            uint8_t ret=0;
            int ret2=-1;

            //Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].isOta = can_ota_status[0].ota_summary.isOta;
            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].errCode = can_ota_status[0].ota_summary.errCode;

            for (i = 0 ; i < 6 ; i++ )
            {
                if (Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].type==DEVICE_BMS)
                {
                    softVersion[0]=Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].version;
                    ESP_LOGI(TAG,"softVersion[0]:%ld,reals.preSoftVersion:%ld",softVersion[0],reals.preSoftVersion);
                    ret2=findFirstMatchVersion(softVersion,1,reals.preSoftVersion);
                    if((reals.preSoftVersion/100)==(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[i].version/100))//内置电池包类型
                    {
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].SN_64 = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.InvSN;
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].Dev_Type = SN_TYPE_SELF;
                        ret=1;
                    }
                    break;
                }
            }

            if(ret)//内置电池包类型
            {
                for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                {
                    if(Inv_can[i].pack_data[reals.online_Y_inv_index].online)
                    {
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can[i].pack_data[reals.online_Y_inv_index].pack_about.sn_code;
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_SELF;
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[INV_CAN_ADDR +i].errCode;
                        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                        if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                        {
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                        }else
                        {
                            Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                        }
                        bias++;
                    }
                }
            }else{
                /* 外置电池包：按每个在线包单独上报，SN 取自身 sn_code，避免复用类型汇总的第一个 SN */
                for(i=0;i<DEV_MAIN_NODE_MAX;i++)
                {
                    for(j=1;j<PACK_MAX_NUM;j++)
                    {
                        if(Inv_can[i].pack_data[j].online == 1)
                        {
                            if((reals.preSoftVersion/100)==(Inv_can[i].pack_data[j].pack_about.soft[0].version/100))
                            {
                                const char *pack_type = Inv_can[i].pack_data[j].pack_about.type_ascii;
                                if(strcmp(pack_type,"B300")==0)
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_B300;
                                }
                                else if(strcmp(pack_type,"B300S")==0)
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_B300S;
                                }
                                else if(strcmp(pack_type,"B300K")==0)
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_B300K;
                                }
                                else if(strcmp(pack_type,"B500K")==0)
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = SN_TYPE_B500K;
                                }
                                else
                                {
                                    uint16_t dev_type = SN_TYPE_ASCII_TO_NUM(pack_type);
                                    dev_type = dev_type != 0 ? dev_type : SN_TYPE_COMMON_PACK;
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].Dev_Type = dev_type;
                                }
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].SN_64 = Inv_can[i].pack_data[j].pack_about.sn_code;
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode = can_ota_status[0].devStatus[INV_CAN_ADDR +i].errCode;
                                Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].soft_version = reals.preSoftVersion;
                                if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].errCode)
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =3;
                                }else
                                {
                                    Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[bias].isOta =2;
                                }
                                bias++;
                                ESP_LOGI(TAG,"bias:%d",bias);
                                ESP_LOGI(TAG,"i:%d,j:%d,ret2:%d,reals.preSoftVersion:%lu,version:%lu,sn:%llu",i,j,ret2,reals.preSoftVersion,Inv_can[i].pack_data[j].pack_about.soft[0].version,Inv_can[i].pack_data[j].pack_about.sn_code);
                            }
                        }
                    }
                }
            }
        }
        break;

    }
    // Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.bias=0;
    // if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].errCode)
    // {
    //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].isOta =3;
    // }else
    // {
    //     Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[0].isOta =2;
    // }
    for(i=0;i<bias;i++)
    {
        ESP_LOGW(TAG,"Modbus_21000_3_Net_Point_Frame i:%d",i);
        ESP_LOGW(TAG,"isOta:%d",(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[i].isOta);
        ESP_LOGW(TAG,"errCode:%d",(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[i].errCode);
        ESP_LOGW(TAG,"SN_64:0x%llu",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[i].SN_64);
        ESP_LOGW(TAG,"Dev_Type:%d",(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[i].Dev_Type);
        ESP_LOGW(TAG,"soft_version:%d",(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg21000_bind.point_bind_info[i].soft_version);
    }

    return bias;
    // g_self_data.mod_reg21000_bind.point_bind_info[i].isOta = reals.net_point_ota_Info[i].is_ota;
    // g_self_data.mod_reg21000_bind.point_bind_info[i].errCode = reals.net_point_ota_Info[i].errcode;

    // g_self_data.mod_reg21000_bind.point_bind_info[i].SN_64 = MicroInv[i+1].mod_reg01100_Inv_base.InvSN;
    // g_self_data.mod_reg21000_bind.point_bind_info[i].Dev_Type = reals.net_point_base_Info[i].net_point_type;

    // for ( j = 0 ; j < 6 ; j++ )
    // {
    //     if (g_self_data_WR.mod_reg21000_bind_WR.bias == MicroInv[i+1].mod_reg01100_Inv_base.soft[j].type)
    //     {
    //         g_self_data.mod_reg21000_bind.point_bind_info[i].soft_version = MicroInv[i+1].mod_reg01100_Inv_base.soft[j].version;
    //         break;
    //     }
    // }
}

/*产品型号ID，来自《产品型号(ASCII码)的代号分配表格》*/
typedef struct {
    const char *ascii;
    uint16_t id;
} dev_model_map_t;

static const dev_model_map_t dev_model_table[] = {
    // 添加便携电源设备类型判断代码
    {SN_TYPE_INV_AC200MAX_ASCII, SN_TYPE_INV_AC200MAX},     // 1
    {SN_TYPE_INV_AC200L_ASCII, SN_TYPE_INV_AC200L},         // 2
    {SN_TYPE_INV_AC300_ASCII, SN_TYPE_INV_AC300},           // 3
    {SN_TYPE_INV_AC500_ASCII, SN_TYPE_INV_AC500},           // 4
    {SN_TYPE_INV_AC70_ASCII, SN_TYPE_INV_AC70},             // 5
    {SN_TYPE_INV_AP300_ASCII, SN_TYPE_INV_AP300},           // 6
    {SN_TYPE_INV_PLP022_ASCII, SN_TYPE_INV_PLP022},         // 7
    {SN_TYPE_INV_ELITE200_V2_ASCII, SN_TYPE_INV_ELITE200_V2},// 8
    {SN_TYPE_INV_RV5_ASCII, SN_TYPE_INV_RV5},               // 9
    {SN_TYPE_FP_ASCII, SN_TYPE_INV_FP},                     // 10
    {SN_TYPE_INV_EL300_ASCII, SN_TYPE_INV_EL300},           // 11
    {SN_TYPE_INV_EB3A_ASCII, SN_TYPE_INV_EB3A},             // 12
    {SN_TYPE_INV_AC60_ASCII, SN_TYPE_INV_AC60},             // 13
    {SN_TYPE_INV_EB55_ASCII, SN_TYPE_INV_EB55},             // 14
    {SN_TYPE_INV_EB70_ASCII, SN_TYPE_INV_EB70},             // 15
    {SN_TYPE_INV_AC180T_SINGLE_ASCII, SN_TYPE_INV_AC180T_SINGLE},   // 16
    {SN_TYPE_INV_AC180T_DUAL_ASCII, SN_TYPE_INV_AC180T_DUAL},       // 17
    {SN_TYPE_INV_EP500_ASCII, SN_TYPE_INV_EP500},           // 18
    {SN_TYPE_INV_EP500PRO_ASCII, SN_TYPE_INV_EP500PRO},     // 19
    {SN_TYPE_INV_AC2A_ASCII, SN_TYPE_INV_AC2A},             // 20
    {SN_TYPE_INV_AC50B_ASCII, SN_TYPE_INV_AC50B},           // 21
    {SN_TYPE_INV_AC60P_ASCII, SN_TYPE_INV_AC60P},           // 22
    {SN_TYPE_INV_AC180_ASCII, SN_TYPE_INV_AC180},           // 23
    {SN_TYPE_INV_AC200P_ASCII, SN_TYPE_INV_AC200P},         // 24
    {SN_TYPE_INV_AC240_ASCII, SN_TYPE_INV_AC240},           // 25
    {SN_TYPE_INV_HANDSFREE1_ASCII, SN_TYPE_INV_HANDSFREE1}, // 26
    {SN_TYPE_INV_HANDSFREE2_ASCII, SN_TYPE_INV_HANDSFREE2}, // 27
    {SN_TYPE_INV_EL320_ASCII, SN_TYPE_INV_EL320},           // 28
    {SN_TYPE_INV_EL400_ASCII, SN_TYPE_INV_EL400},           // 29
    {SN_TYPE_INV_EL80V2_ASCII, SN_TYPE_INV_EL80V2},         // 30
    {SN_TYPE_INV_EL100V2_ASCII, SN_TYPE_INV_EL100},         // 31
    {SN_TYPE_INV_EL30V2_ASCII, SN_TYPE_INV_EL30V2},         // 32
    {SN_TYPE_INV_AC200PL_ASCII, SN_TYPE_INV_AC200PL},       // 33
    {SN_TYPE_INV_PR002_ASCII, SN_TYPE_INV_PR002},           // 34
    {SN_TYPE_INV_AC45_ASCII, SN_TYPE_INV_AC45},             // 35
    {SN_TYPE_INV_AC50P_ASCII, SN_TYPE_INV_AC50P},           // 36
    {SN_TYPE_INV_AC55_ASCII, SN_TYPE_INV_AC55},             // 37
    {SN_TYPE_INV_AC70P_ASCII, SN_TYPE_INV_AC70P},           // 38
    {SN_TYPE_INV_AC240P_ASCII, SN_TYPE_INV_AC240P},         // 39
    {SN_TYPE_INV_AC180P_ASCII, SN_TYPE_INV_AC180P},         // 40
    {SN_TYPE_INV_AC2P_ASCII, SN_TYPE_INV_AC2P},             // 41
    {SN_TYPE_INV_PREMIUM_20C_ASCII, SN_TYPE_INV_PREMIUM_20C}, // 42
    {SN_TYPE_INV_KW1000_ASCII, SN_TYPE_INV_KW1000},         // 43
    {SN_TYPE_INV_LFP700_ASCII, SN_TYPE_INV_LFP700},         // 44
    {SN_TYPE_INV_PR30V2_ASCII, SN_TYPE_INV_PR30V2},         // 45
    {SN_TYPE_INV_PR100V2_ASCII, SN_TYPE_INV_PR100V2},       // 46
    {SN_TYPE_INV_PR200V2_ASCII, SN_TYPE_INV_PR200V2},       // 47
    {SN_TYPE_INV_AORA10_ASCII, SN_TYPE_INV_AORA10},         // 48
    {SN_TYPE_INV_AORA30P_ASCII, SN_TYPE_INV_AORA30P},       // 49
    {SN_TYPE_INV_AORA30V2_ASCII, SN_TYPE_INV_AORA30V2},     // 50
    {SN_TYPE_INV_AORA_80_ASCII, SN_TYPE_INV_AORA_80},       // 51
    {SN_TYPE_INV_AORA_100_ASCII, SN_TYPE_INV_AORA_100},     // 52
    {SN_TYPE_INV_AORA100V2_ASCII, SN_TYPE_INV_AORA100V2},   // 53
    {SN_TYPE_INV_AORA200_ASCII, SN_TYPE_INV_AORA200},       // 54
    {SN_TYPE_INV_AORA320_ASCII, SN_TYPE_INV_AORA320},       // 55
    {SN_TYPE_INV_AP500_ASCII, SN_TYPE_INV_AP500},           // 56
    {SN_TYPE_INV_POWER5_ASCII, SN_TYPE_INV_POWER5},         // 57
    {SN_TYPE_INV_HS5_ASCII, SN_TYPE_INV_HS5},               // 58
    {SN_TYPE_INV_HS3_ASCII, SN_TYPE_INV_HS3},               // 59
    {SN_TYPE_INV_HS2_ASCII, SN_TYPE_INV_HS2},               // 60
    {SN_TYPE_INV_AORA400_ASCII, SN_TYPE_INV_AORA400},       // 61
    {SN_TYPE_INV_EL10_ASCII, SN_TYPE_INV_EL10},             // 62
    {SN_TYPE_INV_EL100MINI_ASCII, SN_TYPE_INV_EL100MINI},   // 63
    {SN_TYPE_INV_EL30MINI_ASCII, SN_TYPE_INV_EL30MINI},     // 64
    {SN_TYPE_INV_EL200MINI_ASCII, SN_TYPE_INV_EL200MINI},   // 65
    {SN_TYPE_INV_AP200_ASCII, SN_TYPE_INV_AP200},           // 69
    {SN_TYPE_INV_AP300V2_ASCII, SN_TYPE_INV_AP300V2},       // 70

    // 户储
    {SN_TYPE_EP600_ASCII, SN_TYPE_EP600},                   // 1000
    {SN_TYPE_EP760_ASCII, SN_TYPE_EP760},                   // 1001
    {SN_TYPE_EP800_ASCII, SN_TYPE_EP800},                   // 1002
    {SN_TYPE_EP900_ASCII, SN_TYPE_EP900},                   // 1003
    {SN_TYPE_EP2000_ASCII, SN_TYPE_EP2000},                 // 1004
    {SN_TYPE_EP13K_ASCII, SN_TYPE_EP13K},                   // 1005
    {SN_TYPE_EP6K_ASCII, SN_TYPE_EP6K},                     // 1006
    {SN_TYPE_EP18K_ASCII, SN_TYPE_EP18K},                   // 1007
    {SN_TYPE_EP5K_ASCII, SN_TYPE_EP5K},                     // 1008
    {SN_TYPE_EP5K_ASCII, SN_TYPE_EP5K5},                    // 1009

    // 微逆
    {SN_TYPE_A80_ASCII, SN_TYPE_A80},                       // 2000
    {SN_TYPE_D100S_ASCII, SN_TYPE_D100S},                   // 2001
    {SN_TYPE_A100_Sub1G_ASCII, SN_TYPE_A100_Sub1G},         // 2002
    {SN_TYPE_A100_WIFI_ASCII, SN_TYPE_A100_WIFI},           // 2003
    {SN_TYPE_D100P_ASCII, SN_TYPE_D100P},                   // 2004
    {SN_TYPE_Will_ASCII, SN_TYPE_Will},                     // 2005

    // 配件
    {SN_TYPE_S1_ASCII, SN_TYPE_S1},                         // 3000
    {SN_TYPE_AT1_ASCII, SN_TYPE_AT1},                       // 3001
    {SN_TYPE_COMBOX_ASCII, SN_TYPE_COMBOX},                 // 3002
    {SN_TYPE_PBOX_ASCII, SN_TYPE_PBOX},                     // 3003
    {SN_TYPE_EBOX_ASCII, SN_TYPE_EBOX},                     // 3004
    {SN_TYPE_HMI_ASCII, SN_TYPE_HMI},                       // 3005
    {SN_TYPE_PANEL_ASCII, SN_TYPE_PANEL},                   // 3006
    {SN_TYPE_DCHUB_ASCII, SN_TYPE_DCHUB},                   // 3007
    {SN_TYPE_ACHUB_ASCII, SN_TYPE_ACHUB},                   // 3008
    {SN_TYPE_SOLARX4K_ASCII, SN_TYPE_SOLARX4K},             // 3009
    {SN_TYPE_CHARGER1_ASCII, SN_TYPE_CHARGER1},             // 3010
    {SN_TYPE_CHARGER2_ASCII, SN_TYPE_CHARGER2},             // 3011
    {SN_TYPE_PACK_BOX_ASCII, SN_TYPE_PACK_BOX},             // 3012
    {SN_TYPE_BLE_HMI_ASCII, SN_TYPE_BLE_HMI},               // 3013
    {SN_TYPE_IOT_INSIDE_ASCII, SN_TYPE_IOT_INSIDE},         // 3014
    {SN_TYPE_SHELLY_METER_ASCII, SN_TYPE_SHELLY_METER},     // 3015

    // 电池包
    {SN_TYPE_B500_ASCII, SN_TYPE_B500},                     // 4000
    {SN_TYPE_B500H_ASCII, SN_TYPE_B500H},                   // 4001
    {SN_TYPE_IB500_ASCII, SN_TYPE_IB500},                   // 4002
    {SN_TYPE_B1210_ASCII, SN_TYPE_B1210},                   // 4003
    {SN_TYPE_B4810_ASCII, SN_TYPE_B4810},                   // 4004
    {SN_TYPE_B300_ASCII, SN_TYPE_B300},                     // 4005
    {SN_TYPE_B300K_ASCII, SN_TYPE_B300K},                   // 4006
    {SN_TYPE_B300S_ASCII, SN_TYPE_B300S},                   // 4007
    {SN_TYPE_B1232_ASCII, SN_TYPE_B1232},                   // 4008
    {SN_TYPE_LEADACID_ASCII, SN_TYPE_LEADACID},             // 4009
    {SN_TYPE_LFP_ASCII, SN_TYPE_LFP},                       // 4010
    {SN_TYPE_B500A_ASCII, SN_TYPE_B500A},                   // 4012
    {SN_TYPE_BC200_ASCII, SN_TYPE_BC200},                   // 4013
    {SN_TYPE_B230_ASCII, SN_TYPE_B230},                     // 4014
    {SN_TYPE_B210_ASCII, SN_TYPE_B210},                     // 4015
    {SN_TYPE_B500K_ASCII, SN_TYPE_B500K},                   // 4016
    {SN_TYPE_B900_ASCII, SN_TYPE_B900},                     // 4017
    {SN_TYPE_HB500_ASCII, SN_TYPE_HB500},                   // 4018
    {SN_TYPE_BC260_ASCII, SN_TYPE_BC260},                   // 4019
    {SN_TYPE_EK900_ASCII, SN_TYPE_EK900},                   // 4020
    {SN_TYPE_IB800JP_ASCII, SN_TYPE_IB800JP},               // 4021
    {SN_TYPE_B500PRO_ASCII, SN_TYPE_B500PRO},               // 4022
    {SN_TYPE_B300PRO_ASCII, SN_TYPE_B300PRO},               // 4023
    {SN_TYPE_IB800_ASCII, SN_TYPE_IB800},                   // 4024    
    {SN_TYPE_HB500S_ASCII, SN_TYPE_HB500S},                 // 4025
    {SN_TYPE_BH500E_ASCII, SN_TYPE_BH500E},                 // 4026
    {SN_TYPE_B4805_ASCII, SN_TYPE_B4805},                   // 4027 

    // 通用设备伪装ASCII
    {SN_TYPE_COMMON_DEVICE_ASCII,        SN_TYPE_COMMON_DEVICE},               // 30000
    {SN_TYPE_COMMON_BATTERY_ASCII,       SN_TYPE_COMMON_BATTERY},              // 30001
    {SN_TYPE_COMMON_INVERTER_ASCII,      SN_TYPE_COMMON_INVERTER},             // 30002
    {SN_TYPE_COMMON_DC_CHARGER_ASCII,    SN_TYPE_COMMON_DC_CHARGER},           // 30003
    {SN_TYPE_COMMON_DC_DISCHARGER_ASCII, SN_TYPE_COMMON_DC_DISCHARGER},        // 30004
    {SN_TYPE_COMMON_DEVICE_CAN_ASCII,    SN_TYPE_COMMON_DEVICE_CAN},           // 30005
    {SN_TYPE_COMMON_BATTERY_CAN_ASCII,   SN_TYPE_COMMON_BATTERY_CAN},          // 30006
    {SN_TYPE_COMMON_INVERTER_CAN_ASCII,  SN_TYPE_COMMON_INVERTER_CAN},         // 30007
    {SN_TYPE_COMMON_DCCHG_CAN_ASCII,     SN_TYPE_COMMON_DC_CHARGER_CAN},       // 30008
    {SN_TYPE_COMMON_DCDCHG_CAN_ASCII,    SN_TYPE_COMMON_DC_DISCHARGER_CAN},    // 30009
    {SN_TYPE_COMMON_DEV_WL_ASCII,        SN_TYPE_COMMON_DEVICE_WIRELESS},      // 30010
    {SN_TYPE_COMMON_BAT_WL_ASCII,        SN_TYPE_COMMON_BATTERY_WIRELESS},     // 30011
    {SN_TYPE_COMMON_INV_WL_ASCII,        SN_TYPE_COMMON_INVERTER_WIRELESS},    // 30012
    {SN_TYPE_COMMON_DCCHG_WL_ASCII,      SN_TYPE_COMMON_DC_CHARGER_WIRELESS},  // 30013
    {SN_TYPE_COMMON_DCDCHG_WL_ASCII,     SN_TYPE_COMMON_DC_DISCHARGER_WIRELESS}// 30014
};

static const size_t dev_model_table_len = sizeof(dev_model_table) / sizeof(dev_model_table[0]);


/*------------------------------------------------------------------------------
 Function: SN_TYPE_ASCII_TO_NUM
 -----------------------------------------------------------------------------*/
/**
  * @brief      根据设备类型转换对应类型号码
  * @param[in]  char *type
  * @param[out] None
  * @return     uint16_t
  */
uint16_t SN_TYPE_ASCII_TO_NUM(char *type)
{
    if (!type || !strnlen(type, TYPE_SIZE)) {
        return 0;
    }
    size_t type_len = strlen(type);
    for (size_t i = 0; i < dev_model_table_len; ++i) {
        // 1. 先比较长度是否完全相等
        if (type_len == strlen(dev_model_table[i].ascii)) {
            // 2. 长度相等再比较内容是否完全相等
            if (0 == strcmp(type, dev_model_table[i].ascii)) {
                return dev_model_table[i].id; // 找到唯一精确匹配
            }
        }
    }
    return 0; // 未找到精确匹配
}

/*------------------------------------------------------------------------------
 Function: SN_TYPE_NUM_TO_ASCII
 -----------------------------------------------------------------------------*/
/**
  * @brief      根据设备类型序号填充对应的字符串到缓冲区 (线程安全，可重入)
  * @param[in]  uint16_t type_num
  * @param[out] out_buffer   用于存储结果的缓冲区
  * @param[in]  buffer_size  缓冲区大小
  * @return     bool         如果成功找到并填充则返回true，否则返回false
  */
bool SN_TYPE_NUM_TO_ASCII(uint16_t type_num, char* out_buffer, size_t buffer_size)
{
    if (!out_buffer || buffer_size == 0) {
        return false;
    }

    const char* src = NULL;

    // 遍历查找表以找到匹配的ID
    for (size_t i = 0; i < dev_model_table_len; ++i) {
        if (dev_model_table[i].id == type_num) {
            src = dev_model_table[i].ascii;
            break;
        }
    }

    // 如果未找到匹配项
    if (!src) {
        // 清空输出缓冲区
        memset(out_buffer, 0x00, buffer_size);
        return false;
    }

    // 将源字符串复制到输出缓冲区，并用0x00填充剩余部分
    memset(out_buffer, 0x00, buffer_size);
    size_t len = strlen(src);
    if (len >= buffer_size) {
        // 防止溢出，确保最后一个字节是'\0'
        len = buffer_size - 1;
    }
    memcpy(out_buffer, src, len);

    return true;
}


// /*------------------------------------------------------------------------------
//  Function: PACK_SN_TYPE_TO_Group_Addr
//  -----------------------------------------------------------------------------*/
// /**
//   * @brief      根据PACK类型给出对应群组地址
//   * @param[in]  uint16_t type_cnt
//   * @param[out] None
//   * @return     uint8_t
//   */
// uint8_t PACK_SN_TYPE_TO_Group_Addr(uint16_t type_cnt)
// {
//     uint16_t group_same_addr = 0;

//     /*
// 地址暂时固定*/
//     if (type_cnt == SN_TYPE_PACK_BOX) {
//             group_same_addr = MD_PACK_SUM_ADDR_START;
//     } else if (type_cnt == SN_TYPE_B300) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_B300 - SN_TYPE_PACK_MIN_START);
//     } else if (type_cnt == SN_TYPE_B300K) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_B300K - SN_TYPE_PACK_MIN_START);
//     } else if (type_cnt == SN_TYPE_B300S) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_B300S - SN_TYPE_PACK_MIN_START);
//     } else if (type_cnt == SN_TYPE_B1210) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_B1210 - SN_TYPE_PACK_MIN_START);
//     } else if (type_cnt == SN_TYPE_B4810) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_B4810 - SN_TYPE_PACK_MIN_START);
//     } else if (type_cnt == SN_TYPE_B1232) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_B1232 - SN_TYPE_PACK_MIN_START);
//     } else if (type_cnt == SN_TYPE_LEADACID) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_LEADACID - SN_TYPE_PACK_MIN_START);
//     } else if (type_cnt == SN_TYPE_LFP) {
//         group_same_addr = MD_PACK_SUM_ADDR_START + (SN_TYPE_LFP - SN_TYPE_PACK_MIN_START);
//     }

//     return group_same_addr;
// }

// /*------------------------------------------------------------------------------
//  Function: INV_Point_State_Update
//  -----------------------------------------------------------------------------*/
// /**
//   * @brief      逆变状态更新
//   * @param[in]  uint8_t index
//                 uint8_t x
//                 uint8_t y
//   * @param[out] None
//   * @return     void
//   */
// uint16_t INV_Point_State_Update(uint8_t x, uint8_t y)
// {
//     uint8_t alarm_status = 0;
//     uint8_t fault_status = 0;
//     POINT_STATE state = {0};

//     /*在线状态*/
//     state.bit.point_online = 1;

//     /*告警状态*/
//     for ( uint8_t i = 0 ; i < 4; i++ )
//     {
//         if ( Inv_can[x].inv_data[y].inv_base.alarm[i] != 0 )
//         {
//             alarm_status |= 1;
//             break;
//         }
//     }
//     state.bit.alarm = alarm_status;

//     /*故障状态*/
//     for ( uint8_t i = 0 ; i < 4; i++ )
//     {
//         if ( Inv_can[x].inv_data[y].inv_base.fault[i] != 0 )
//         {
//             fault_status |= 1;
//             break;
//         }
//     }
//     if ( Inv_can[x].inv_data[y].inv_base.fault5 != 0 ) fault_status |= 1;
// #ifndef CONFIG_HARDWARE_PANEL
//     /*主机IOT故障*/
//     if ( Inv[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_fault.all != 0 ) fault_status |= 1;

//     /*主机逆变汇总PACK系统故障*/
//     if ( Pack_Collect[0].mod_reg06000_Pack_sum.all_pack_alarm1
//         || Pack_Collect[0].mod_reg06000_Pack_sum.all_pack_alarm2) {
//         fault_status |= 1;
//     }
// #endif
//     state.bit.protect = fault_status;

//     /*升级状态*/
// #ifdef CONFIG_HARDWARE_PANEL
//     /*Panel*/
//     state.bit.update_need = Get_http_new_version_flag(HTTPS_CHECK_INV_IMAGE, 0);
// #else
//     /*主机*/
//     state.bit.update_need = Get_http_new_version_flag(HTTPS_CHECK_INV_IMAGE, 0)
//                             | Get_http_new_version_flag(HTTPS_CHECK_IOT_IMAGE, 0);
// #endif

//     return state.all;
// }

// /*------------------------------------------------------------------------------
//  Function: Pack_Point_State_Update
//  -----------------------------------------------------------------------------*/
// /**
//   * @brief      PACK状态更新
//   * @param[in]  uint8_t index
//                 uint8_t x
//                 uint8_t y
//   * @param[out] None
//   * @return     void
//   */
// uint16_t Pack_Point_State_Update(uint8_t group_same_type_addr, uint8_t x, uint8_t y)
// {
//     POINT_STATE state = {0};

//     /*在线状态*/
//     state.bit.point_online = 1;

//     /*保护状态*/
//     if ( Pack_can.pack_single_data[x][y].pack_base.protect_status
//         || Pack_can.pack_single_data[x][y].pack_base.protect_status2) {
//         state.bit.protect = 1;
//     } else {
//         state.bit.protect = 0;
//     }

//     /*升级状态*/
//     state.bit.update_need = Get_http_new_version_flag(HTTPS_CHECK_PACK_IMAGE, (group_same_type_addr - MD_PACK_SUM_ADDR_START));

//     return state.all;
// }

/*------------------------------------------------------------------------------
 Function: INV_Point_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      逆变状态更新
  * @param[in]  uint8_t index
                uint8_t x
                uint8_t y
  * @param[out] None
  * @return     void
  */
uint16_t INV_Point_State_Update(uint8_t x, uint8_t y)
{
    uint8_t alarm_status = 0;
    uint8_t fault_status = 0;
    POINT_STATE state = {0};

    /*在线状态*/
    state.bit.point_online = 1;

    /*告警状态*/
    for ( uint8_t i = 0 ; i < 4; i++ )
    {
        if ( Inv_can[x].inv_data[y].inv_base.alarm[i] != 0 )
        {
            alarm_status |= 1;
            break;
        }
    }
    state.bit.alarm = alarm_status;

    /*故障状态*/
    for ( uint8_t i = 0 ; i < 4; i++ )
    {
        if ( Inv_can[x].inv_data[y].inv_base.fault[i] != 0 )
        {
            fault_status |= 1;
            break;
        }
    }
    if ( Inv_can[x].inv_data[y].inv_base.fault5 != 0 ) fault_status |= 1;
#ifndef CONFIG_HARDWARE_PANEL
    /*主机IOT故障*/
    if ( Inv[DEV_MAIN_NODE_MAX].mod_reg11000_IOT_info.iot_fault.all != 0 ) fault_status |= 1;

    // /*主机逆变汇总PACK系统故障*/ AP300 暂时没有该需求
    // if ( Pack_Collect[0].mod_reg06000_Pack_sum.all_pack_alarm1
    //     || Pack_Collect[0].mod_reg06000_Pack_sum.all_pack_alarm2) {
    //     fault_status |= 1;
    // }
#endif
    state.bit.protect = fault_status;

    /*升级状态*/

    /*主机*/
    state.bit.update_need = Get_http_new_version_flag(HTTPS_CHECK_INV_IMAGE, 0)
                            | Get_http_new_version_flag(HTTPS_CHECK_IOT_IMAGE, 0);
    ESP_LOGI(TAG,"INV Need update:%d",state.bit.update_need);
    return state.all;
}

/*------------------------------------------------------------------------------
 Function: INV_Point_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      DCHUB状态更新
  * @param[in]  uvoid
  * @param[out] None
  * @return     void
  */
uint16_t INV_DcHub_State_Update(void)
{
    uint8_t alarm_status = 0;
    uint8_t fault_status = 0;
    POINT_STATE state = {0};

    /*在线状态*/
    state.bit.point_online = 1;

    /*升级状态*/

    /*DCHUB*/
    state.bit.update_need = Get_http_new_version_flag(HTTPS_CHECK_DCHUB_IMAGE, 0);
    ESP_LOGI(TAG,"Dchub Need update:%d",state.bit.update_need);
    return state.all;
}

/*------------------------------------------------------------------------------
 Function: INV_AcHub_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      ACHUB状态更新
  * @param[in]  void
  * @param[out] None
  * @return     void
  */
uint16_t INV_AcHub_State_Update(void)
{
    uint8_t alarm_status = 0;
    uint8_t fault_status = 0;
    POINT_STATE state = {0};

    /*在线状态*/
    state.bit.point_online = 1;

    /*警报状态*/
    if(Inv_can_mix.ac_hub_data[0].ac_hub_info.alarm_State.all!=0)
        state.bit.alarm = 1;

    /*升级状态*/

    /*ACHUB*/
    state.bit.update_need = Get_http_new_version_flag(HTTPS_CHECK_ACHUB_IMAGE, 0);
    ESP_LOGI(TAG,"Achub Need update:%d",state.bit.update_need);
    return state.all;
}

/*------------------------------------------------------------------------------
 Function: INV_D400s_State_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      D400S状态更新
  * @param[in]  void

  * @param[out] None
  * @return     void
  */
uint16_t INV_D400s_State_Update(void)
{
    uint8_t alarm_status = 0;
    uint8_t fault_status = 0;
    POINT_STATE state = {0};

    /*在线状态*/
    state.bit.point_online = 1;

    /*升级状态*/

    /*DCHUB*/
    state.bit.update_need = Get_http_new_version_flag(HTTPS_CHECK_ACHUB_IMAGE, 0);
    ESP_LOGI(TAG,"d400S Need update:%d",state.bit.update_need);
    return state.all;
}

