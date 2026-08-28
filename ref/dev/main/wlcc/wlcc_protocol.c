/**
 * @file wlcc_protocol.c
 * @brief 无线设备间应用层通用协议处理, 包括设备发现和WIFI协议
 *        《无线设备间（如mesh）应用层通用协议20250528.xlsx》
 * @version 1.0
 */

#include "wlcc_protocol.h"
#include "wlcc_interface.h"
#include "wlcc_process.h"
#include "wlcc_common.h"
#include "wlcc_tlv.h"

#include "modbus_data.h"
#include "modbus_master_data.h"
#include "crc.h"
#include "can_protocol.h"
#include "modbus_protocol.h"
#include "modbus_slave.h"
#include "modbus_master.h"
#include "filesystem.h"
// #include "iot_define.h"
#include "esp_log.h"

#if CONFIG_DEV_MODBUS_SUPPORT
// #include "app_bt.h"
#include "app_ble.h"
#endif


#if CONFIG_DEV_MODBUS_SUPPORT
#include "dev_modbus_manage.h"
#include "dev_modbus_s1_data_handler.h"
#include "dev_modbus_meter_data_handler.h"
#include "dev_modbus_other_inv_data_handler.h"
#endif

#include "iot_mqtt.h"
#include "dev_discovery.h"
#include "modbus_data.h"


#define TAG "[wlcc_protocol]"


typedef struct
{
    uint8_t current_netif_id;//网卡名称序号
    struct ifreq udp_netif_req;
    uint8_t states[NETIF_TYPE_MAX];//0:未配置 1：可配置 2：配置成功
    char source_ip_str[32];//本机WIFI AP IP
}wlcc_config_t;

EXT_RAM_BSS_ATTR wlcc_config_t wlcc_config;

EXT_RAM_BSS_ATTR modbus_handle_t g_modbus_handle; // 发送临时辅助变量
EXT_RAM_BSS_ATTR static uint8_t send_buffer[512];

static uint16_t wlcc_msg_id = 0;

extern QueueHandle_t can_cmd_queue;

uint16_t wlcc_get_msg_id(void)
{
	return wlcc_msg_id;
}

/**
 * @brief 构建WLCC协议头（不包含Modbus帧内容和CRC）
 * @param outbuf 输出缓冲区
 * @param frametype 帧类型
 * @param dev_type 目标设备类型（小端序）
 * @param dev_sn 目标设备SN（8字节）
 * @return 协议头结束位置（即Modbus帧长度字段的位置）
 */
static uint16_t wlcc_build_frame_header(uint8_t *outbuf,
                                         uint8_t frametype,
                                         uint16_t dev_type,
                                         uint64_t dev_sn)
{
    uint16_t i = 0;
    uint16_t u16Tempdata = 0;

    // 协议头
    outbuf[i++] = WLCC_FRAME_HEADER_COMMON;
    outbuf[i++] = WLCC_FRAME_VERSION_WIFI;

    // 消息ID(MSG_ID) - 先递增再写入
    wlcc_msg_id++;
    outbuf[i++] = wlcc_msg_id & 0xFF;
    outbuf[i++] = wlcc_msg_id >> 8;

    // 源设备SN (8B)
    memcpy((uint8_t *)&outbuf[i], (uint8_t *)&iot_factory.iot_sn, 8);
    i += 8;

    // 机型序号_源设备 (2B, 小端序)
    u16Tempdata = SN_TYPE_SELF;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

    // 目标设备:机型序号 (2B, 小端序)
    memcpy((uint8_t *)&outbuf[i], &dev_type, 2);
    i += 2;

    // 目标设备:SN (8B)
    memcpy((uint8_t *)&outbuf[i], &dev_sn, 8);
    i += 8;

    // 报文类型
    outbuf[i++] = frametype;

    // TTL (2B, 小端序)
    u16Tempdata = 0;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

    // 源设备优先级 (2B, 小端序)
    u16Tempdata = DEV_PRIORITY_SELF;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

    // 并机序号 - 根据主从模式动态设置
    outbuf[i++] = (wlcc_is_master() == WLCC_EMS_MODE_MASTER) ? 0 : 1;

    // 路由层级，第三方路由器模式，填2
    u16Tempdata = 2;
    outbuf[i++] = u16Tempdata & 0xFF;

    // IP1 (路由器IP)
    for (uint8_t j = 0; j < 4; j++)
    {
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_gw[3 - j];
    }

    // IP2 (本机IP)
    for (uint8_t j = 0; j < 4; j++)
    {
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[3 - j];
    }

    return i; // 返回Modbus帧长度字段的位置
}

/**
 * @brief      检查帧格式
 * @param[in]  income  接收的帧数据
 * @param[in]  inlen   帧数据长度
 * @return     IOT_OK if the frame format is correct, otherwise IOT_FAIL
 */
int check_wlcc_frame_format(const uint8_t *income, uint16_t inlen)
{
    uint16_t value_crc16 = calcu_crc16(income, (inlen - 2));

    /* 检查CRC和协议版本 */
    if ((inlen > (WLCC_FRAME_ADDR_IP_LEVEL+2))
		&& (value_crc16 == (((uint16_t)income[inlen - 1] << 8) | income[inlen - 2]))
		&& (WLCC_FRAME_VERSION_WIFI == income[WLCC_FRAME_ADDR_VER]))
    {
        return IOT_OK;
    }

    ESP_LOGE(TAG, "check frame format error, crc:%d, version:%d", value_crc16, income[WLCC_FRAME_ADDR_VER]);

    return IOT_FAIL;
}

int check_modbus_frame_format(const uint8_t *income, uint16_t inlen)
{
	int ret = IOT_FAIL;
	uint8_t ip_level= income[WLCC_FRAME_ADDR_IP_LEVEL];
	int8_t frame_typeL4 = income[WLCC_FRAME_ADDR_TYPE] & 0xF;
	uint8_t frame_typeH4 =(income[WLCC_FRAME_ADDR_TYPE]>>4)&0x0F;

	uint8_t funcode = income[WLCC_FRAME_ADDR_IP_BEGIN + ip_level*4 + 3] & 0x7F;
	uint8_t err_flag = income[WLCC_FRAME_ADDR_IP_BEGIN + ip_level*4 + 3] & 0x80;
	uint16_t value_crc16 = CalcCrc16_modbus(income, (inlen - 2));

	if (income[WLCC_FRAME_ADDR_VER] != WLCC_FRAME_VERSION_WIFI)
	{
		ret = IOT_FAIL;
		ESP_LOGE(TAG, "WLCC_FRAME_ADDR_VER error");
	}
	else if ((value_crc16 != (((uint16_t)income[inlen - 1] << 8) | income[inlen - 2])) // check crc
			 || (frame_typeL4 > 8)) 													 // 报文类型,非法
	{
		ret = IOT_FAIL;
		ESP_LOGE(TAG, "frame_type or crc error");
	}
	else if ((WLCC_FRAME_TYPE_READ == frame_typeL4)
			|| (WLCC_FRAME_TYPE_WRITE == frame_typeL4)
			|| (WLCC_FRAME_TYPE_READ_RTN == frame_typeL4)
			|| (WLCC_FRAME_TYPE_WRITE_RTN == frame_typeL4)
			|| (WLCC_FRAME_TYPE_PERIOD == frame_typeL4)
		)
	{
		if (err_flag)
		{
			ret = IOT_FAIL;
			ESP_LOGE(TAG, " modbus funcode:0x%x, err_flag:0x%x, frame_typeH4=%d, frame_typeL4=%d",funcode,err_flag, frame_typeH4,frame_typeL4);
			// ESP_LOGE(TAG, " modbus errcode:%x", income[WLCC_FRAME_ADDR_IP_BEGIN + ip_level*4 + 2]);
		}
		else if ((0x03 != funcode) && (0x06 != funcode) && (0x10 != funcode)) // modbus功能码
		{
			ret = IOT_FAIL;
			ESP_LOGE(TAG, " modbus funcode:0x%x, err_flag:0x%x, frame_typeH4=%d, frame_typeL4=%d",funcode,err_flag, frame_typeH4,frame_typeL4);
		}
		else
		{
			ret = frame_typeL4;
		}
	}
	else
	{
		ret = frame_typeL4;
	}

	return ret; /* 返回接收的功能码 */
}


uint8_t modbus_read_reg_rsp_handle(uint8_t SlaveAddr, uint16_t regAdderss, uint16_t gRegCnt, const uint8_t *cmdBuf)
{
    uint8_t bytesCounter = 0;
    uint8_t i = 0;
    uint16_t *regPtr = NULL;
    reg_position_t reg_position;

    ESP_LOGW(TAG, "[Modbus_ReadReg_03H_RTN_Udp] RegAddress : %d,  gRegCnt : %d,  RTN_gRegCnt : %d", regAdderss, gRegCnt, cmdBuf[2] >> 1);

    regPtr = vLookupDataTab(SlaveAddr, regAdderss, gRegCnt, false, &reg_position);
    if ((NULL != regPtr) && ((gRegCnt << 1) == cmdBuf[2]))
    {
        ESP_LOGI(TAG, "[Modbus_ReadReg_03H_RTN_Udp] regAdderss2 ok");

        bytesCounter = gRegCnt << 1;

        for (i = 0; i < bytesCounter; i += 2)
        {
            regPtr[i / 2] = ((uint16_t)cmdBuf[3 + i] << 8) | cmdBuf[4 + i]; // H/L
        }
    }
    else
    {
        ESP_LOGE(TAG, "[Modbus_ReadReg_03H_RTN_Udp] regAdderss2 or gRegCnt error");
        return 1;
    }

    return 0;
}

void wlcc_modbus_report_handle(const uint8_t *income, uint16_t inLen, wlcc_dev_info_t src_addr)
{
	uint16_t startAddress = income[2] << 8 | income[3]; // 写入寄存器地址
    uint16_t writeRegsCnt = 0;
	uint16_t writeRegData;
	uint8_t bytesCounter = 0;
	uint16_t *reg_ptr = NULL;
	int i = 0;

	if(0x06 == income[1]){
		writeRegsCnt = 1;
	}
	else if(0x10 == income[1]){
		writeRegsCnt = income[4] << 8 | income[5]; // 写入寄存器数量
	}
	else
	{
		ESP_LOGE(TAG, "[Modbus_Report] funcode[%d] error", income[1]);
		return;
	}

	// switch(src_addr.dev_type)
	// {
	// 	case SN_TYPE_S1:
	// 		//  reg_ptr = vLookupS1DataTab(src_addr.dev_type, src_addr.dev_sn, startAddress, writeRegsCnt, false);
	// 		break;
	// 	case SN_TYPE_METER:
	// 		// reg_ptr = vLookupMeterDataTab(src_addr.dev_type, src_addr.dev_sn, startAddress, writeRegsCnt, false);
	// 		break;
	// 	case SN_TYPE_AC2AC:
	// 		// reg_ptr = vLookupDataTab_from_other_inv(src_addr.dev_type, src_addr.dev_sn, startAddress, writeRegsCnt, false);
	// 		break;
	// 	default:
	// 		break;
	// }

	if(NULL != reg_ptr)
	{
		if(0x06 == income[1])
		{
			writeRegData = income[4] << 8 | income[5]; // 写入的数据,数据已经交换lsb
			reg_ptr[0] = writeRegData;
		}
		else
		{
			bytesCounter = writeRegsCnt << 1;
	        for (i = 0; i < bytesCounter; i += 2){
	            reg_ptr[i / 2] = ((uint16_t)income[7 + i] << 8) | income[8 + i]; // H/L
	        }
		}
	}
	else{
		ESP_LOGE(TAG, "[Modbus_Report] (dev_type=%d, Sn=%llu, msg_id=%d)regAdderss2 or gRegCnt error", src_addr.dev_type, src_addr.dev_sn, src_addr.msg_id);
	}
}


uint8_t wlcc_heartbeat_handle(const uint8_t *cmdBuf)
{
    // 机型序号(机型序号)
    uint16_t Type_Cnt = *((uint16_t *)&cmdBuf[WLCC_FRAME_ADDR_TYPE_SOURCE]);
    // SN
    uint64_t SN_64 = *((uint64_t *)&cmdBuf[WLCC_FRAME_ADDR_SN_SOURCE]);

    ESP_LOGW(TAG, "[Wifi_Udp_Network_Heartbeat]  SN:%llu, tpye:%u", SN_64, Type_Cnt);

    return 0;
}

uint16_t wlcc_build_data_send_frame(uint8_t frametype, uint16_t dev_type, uint64_t dev_sn, uint8_t *inbuf, uint16_t inlen, uint8_t *outbuf)
{
    uint16_t crc_value = 0;
    uint16_t i = 0, j = 0;
    uint16_t len = 0;
    uint16_t u16Tempdata = 0;

    outbuf[i++] = WLCC_FRAME_HEADER_COMMON;
    outbuf[i++] = WLCC_FRAME_VERSION_WIFI;

	//消息ID(MSG_ID)
	wlcc_msg_id++;
    outbuf[i++] = wlcc_msg_id & 0xFF;
    outbuf[i++] = wlcc_msg_id >> 8;

    memcpy((uint8_t *)&outbuf[i], (uint8_t *)&iot_factory.iot_sn, 8); // 源 SN
    i += 8;

    // 机型序号_源设备
    u16Tempdata = SN_TYPE_SELF;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 目标设备:机型序号
	memcpy((uint8_t *)&outbuf[i], &dev_type, 2);
	i += 2;

    // 目标设备:SN
    memcpy((uint8_t *)&outbuf[i], &dev_sn, 8);
    i += 8;

    // 报文类型
    outbuf[i++] = frametype;

	// TTL
	u16Tempdata = 0;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 源设备优先级
	u16Tempdata = DEV_PRIORITY_SELF;
	outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 并机排序
	outbuf[i++] = (wlcc_is_master() == WLCC_EMS_MODE_MASTER)?0:1;

	//路由层级,第三方路由器模式，填2
    u16Tempdata = 2;
    outbuf[i++] = u16Tempdata & 0xFF;

    // IP1 ,router decide
    for (j = 0; j < 4; j++){
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_gw[3 - j];
    }
    // IP2 ,self
    for (j = 0; j < 4; j++){
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[3 - j];
    }

    // modbus协议帧区长度
    u16Tempdata = inlen;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;
	 if((NULL != inbuf) && inlen){
		memcpy(outbuf+i, inbuf, inlen);
	}
    i += inlen;

    crc_value = calcu_crc16(outbuf, i);
    outbuf[i++] = (uint8_t)crc_value;
    outbuf[i++] = (uint8_t)(crc_value >> 8);

    return i;
}

uint16_t wlcc_build_response_frame(uint16_t msg_id, uint8_t frametype, uint16_t dev_type, uint64_t dev_sn, uint8_t *inbuf, uint16_t inlen, uint8_t *outbuf)
{
    uint16_t crc_value = 0;
    uint16_t i = 0, j = 0;
    uint16_t len = 0;
    uint16_t u16Tempdata = 0;

    outbuf[i++] = WLCC_FRAME_HEADER_COMMON;
    outbuf[i++] = WLCC_FRAME_VERSION_WIFI;

	//消息ID(MSG_ID)
    outbuf[i++] = msg_id & 0xFF;
    outbuf[i++] = msg_id >> 8;

    memcpy((uint8_t *)&outbuf[i], (uint8_t *)&iot_factory.iot_sn, 8); // 源 SN
    i += 8;

    // 机型序号_源设备
    u16Tempdata = SN_TYPE_SELF;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 目标设备:机型序号
	memcpy((uint8_t *)&outbuf[i], &dev_type, 2);
	i += 2;

    // 目标设备:SN
    memcpy((uint8_t *)&outbuf[i], &dev_sn, 8);
    i += 8;

    // 报文类型
    outbuf[i++] = frametype;

	// TTL
	u16Tempdata = 0;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 源设备优先级
	u16Tempdata = DEV_PRIORITY_SELF;
	outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 并机排序
	outbuf[i++] = (wlcc_is_master() == WLCC_EMS_MODE_MASTER)?0:1;

	//路由层级,第三方路由器模式，填2
    u16Tempdata = 2;
    outbuf[i++] = u16Tempdata & 0xFF;

    // IP1 ,router decide
    for (j = 0; j < 4; j++){
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_gw[3 - j];
    }
    // IP2 ,self
    for (j = 0; j < 4; j++){
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[3 - j];
    }

    // modbus协议帧区长度
    u16Tempdata = inlen;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;
	 if((NULL != inbuf) && inlen){
		memcpy(outbuf+i, inbuf, inlen);
	}
    i += inlen;

    crc_value = calcu_crc16(outbuf, i);
    outbuf[i++] = (uint8_t)crc_value;
    outbuf[i++] = (uint8_t)(crc_value >> 8);

    return i;
}

int wlcc_modbus_to_self_handle(uint8_t *data, int len, wlcc_dev_info_t src_addr)
{
	uint8_t modbus_buffer[256];
	int16_t rsp_len = 0;
	int16_t tx_len = 0;

	uint8_t funcode = data[1];
	uint8_t frametype = 0;

	can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};
    uint8_t can_cmd_flag = 0;
    if (((funcode == 0x06) || (funcode == 0x10)) && can_cmd_queue)
    {
        // 当MODBUS为设置指令时,才需要开辟空间
        can_cmd.cmd = heap_caps_malloc(sizeof(can_data_label) * can_cmd.num, MALLOC_CAP_SPIRAM);
        if (!can_cmd.cmd)
        {
            ESP_LOGE (TAG, "ble to can malloc failed");
        }
    }

	rsp_len = Modbus_Slave(data, len,
                           modbus_buffer, can_cmd.cmd, &can_cmd.num,
                           MD_CHL_WIFI_CLOUD, NULL); /* modbus handle */
    if (rsp_len <= 0)
    {
        ESP_LOGE(TAG, "Modbus Slave read error:%d", rsp_len);
		if(!can_cmd.cmd){
			free(can_cmd.cmd);
		}
		return -1;
    }

    ESP_LOGI(TAG, "WLCC modbus rsp data:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, modbus_buffer, rsp_len, ESP_LOG_INFO);

	if((funcode == 0x06) || (funcode == 0x10)){
		frametype = WLCC_FRAME_TYPE_WRITE_RTN;
	}else if((funcode == 0x03)){
		frametype = WLCC_FRAME_TYPE_READ_RTN;
	}
	/* wlcc协议组帧 */
	tx_len = wlcc_build_response_frame(src_addr.msg_id, frametype,
										src_addr.dev_type, src_addr.dev_sn,
										modbus_buffer, rsp_len, send_buffer);
    ESP_LOGE(TAG, "Udp_singlecast_Tx FFF");
    wlcc_msg_send_to_queue(send_buffer, tx_len, NULL, 0);

     /* modbus指令转换为can指令发送到队列 */
    if (can_cmd.cmd != NULL && can_cmd.num != 0)
    {
        can_cmd.md_addr = data[0];
        if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            can_cmd_flag = 1;
            reals.BLE_WIFI_to_Can_Cnt++;
            ESP_LOGW(TAG, "BLE_WIFI_to_Can_Cnt:%d", reals.BLE_WIFI_to_Can_Cnt);
        }
    }

    if (!can_cmd_flag && can_cmd.cmd){
        free(can_cmd.cmd);
    }

	return 0;
}



int build_device_discovery_frame(uint8_t *outbuf, uint8_t step, uint8_t *src_ip)
{
    uint16_t crc_value = 0;
    uint16_t i = 0;
    uint8_t j = 0;
    uint16_t len = 0;
    uint16_t temp_data = 0;

    outbuf[i++] = step;
    outbuf[i++] = WLCC_FRAME_VERSION_WIFI;

	//消息ID(MSG_ID)
	wlcc_msg_id++;
    outbuf[i++] = wlcc_msg_id & 0xFF;
    outbuf[i++] = wlcc_msg_id >> 8;

    memcpy((uint8_t *)&outbuf[i], (uint8_t *)&iot_factory.iot_sn, 8); // 源 SN
    i += 8;

    // 机型序号_源设备
    temp_data = SN_TYPE_SELF;
    outbuf[i++] = temp_data & 0xFF;
    outbuf[i++] = temp_data >> 8;

    for (j = 0; j < 8; j++) // 目标 SN
    {
        outbuf[i++] = 0;
    }

    // 机型序号_目标设备
    temp_data = 0;
    outbuf[i++] = temp_data & 0xFF;
    outbuf[i++] = temp_data >> 8;

    outbuf[i++] = 0; // 报文类型

	// TTL
    temp_data = 0;
    outbuf[i++] = temp_data & 0xFF;
    outbuf[i++] = temp_data >> 8;

	/**
     * @brief 源设备优先级: 数值越大，优先级越高
     */
    temp_data = DEV_PRIORITY_CMM_INV;
    outbuf[i++] = temp_data & 0xFF;
    outbuf[i++] = temp_data >> 8;

    /**
     * @brief 并机序号: 相同类型设备在局域网中的序号
        0-主设备，最高优先级
        1-默认/单机序号
    */
    temp_data = (wlcc_is_master() == WLCC_EMS_MODE_MASTER)?0:1;
    outbuf[i++] = temp_data & 0xFF;

	//路由层级,第三方路由器模式，填2
    temp_data = 2;
    outbuf[i++] = temp_data & 0xFF;

    // IP1 ,router decide
    for (j = 0; j < 4; j++){
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_gw[3 - j];
    }
    // IP2 ,self
    for (j = 0; j < 4; j++){
        outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[3 - j];
    }

    crc_value = calcu_crc16(outbuf, i);
    outbuf[i++] = (uint8_t)crc_value;
    outbuf[i++] = (uint8_t)(crc_value >> 8);

    return i;
}

uint8_t build_common_heart_frame(uint8_t *outbuf)
{
    uint16_t crc_value = 0;
    uint16_t i = 0;
    uint8_t j = 0;
    uint16_t u16Tempdata = 0;

    outbuf[i++] = WLCC_FRAME_HEADER_COMMON;
    outbuf[i++] = WLCC_FRAME_VERSION_WIFI;

	//消息ID(MSG_ID)
	wlcc_msg_id++;
    outbuf[i++] = wlcc_msg_id & 0xFF;
    outbuf[i++] = wlcc_msg_id >> 8;

    memcpy((uint8_t *)&outbuf[i], (uint8_t *)&iot_factory.iot_sn, 8); // 源 SN
    i += 8;

    // 机型序号_源设备
    u16Tempdata = SN_TYPE_SELF;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

    // SN,机型序号_目标设备
    for (j = 0; j < 10; j++)
    {
        outbuf[i++] = 0;
    }

    // 报文类型
    outbuf[i++] = WLCC_FRAME_TYPE_HEART;

	// TTL
	u16Tempdata = 0;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 源设备优先级
	u16Tempdata = DEV_PRIORITY_SELF;
	outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

	// 并机排序
	outbuf[i++] = (wlcc_is_master() == WLCC_EMS_MODE_MASTER)?0:1;

	//路由层级,第三方路由器模式，填2
	outbuf[i++] = 1;

    /*
    源设备网络IPV4地址:路由器决定
    (顺序填充，数字内容表示，如192.168.1.2依次填充2,1,168,192这4个数字)
    */
    if ((NETIF_TYPE_WIFI_STA == wlcc_config.current_netif_id) || (NETIF_TYPE_WIFI_AP == wlcc_config.current_netif_id))
    {
        for (j = 0; j < 4; j++) // 源 IP
        {
            outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_ipv4[3 - j];
        }
    }
    else if (wlcc_config.current_netif_id == NETIF_TYPE_ETH)
    {
        for (j = 0; j < 4; j++) // 源IP
        {
            outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.IP_ETH[3 - j]; // MAC
        }
    }
    else
    {
        for (j = 0; j < 4; j++) // 源IP
        {
            outbuf[i++] = 0;
        }
    }

    /*
    源设备网络IPV4 服务端口号：本地可随机指定，但是必须要和实际UDP单播发送一致
    */
    u16Tempdata = UDP_PORT_SINGLE;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

    // 机型序号_源设备
    u16Tempdata = SN_TYPE_SELF;
    outbuf[i++] = u16Tempdata & 0xFF;
    outbuf[i++] = u16Tempdata >> 8;

    // 信号强度
    outbuf[i++] = Inv[reals.Addr_can_self].mod_reg11000_IOT_info.sta_rssi;

    for (j = 0; j < 10; j++) // reserved
    {
        outbuf[i++] = 0;
    }

    crc_value = calcu_crc16(outbuf, i);

    outbuf[i++] = (uint8_t)crc_value;
    outbuf[i++] = (uint8_t)(crc_value >> 8);

    return i;
}

int device_discovery_is_ready(uint8_t *buff, const uint8_t *src_ip)
{
    uint16_t len = 0;
    static uint16_t scnt = 0;
    static uint16_t scnt2 = 0xFFFF;

    if (WLCC_FRAME_HEADER_COMMON == reals.Step_dev_discovery)
    {
        reals.Step_dev_discovery = WLCC_FRAME_HEADER_TRIGER;
        scnt = 0;
    }
    /*触发帧，仅首次上电设备发送，周期1s，共3s*/
    else if (WLCC_FRAME_HEADER_TRIGER == reals.Step_dev_discovery)
    {
        if (++scnt >= 3)
        {
            scnt = 0;
            reals.Step_dev_discovery = WLCC_FRAME_HEADER_SEND_SN;
        }
    }
    /*信息上报帧，周期1s，共3s*/
    else if (WLCC_FRAME_HEADER_SEND_SN == reals.Step_dev_discovery)
    {
        if (++scnt >= 3)
        {
            scnt = 0;
            reals.Step_dev_discovery = WLCC_FRAME_HEADER_FINISH;
        }
    }
    /*完成帧，发送3次*/
    else if (WLCC_FRAME_HEADER_FINISH == reals.Step_dev_discovery)
    {
    	if (++scnt >= 3)
        {
            scnt = 0;
            reals.Step_dev_discovery = WLCC_FRAME_HEADER_FINISH_AFTER;
        }
    }

    /* 根据当前步骤, 设备发现协议组帧 */
    if ((reals.Step_dev_discovery >= WLCC_FRAME_HEADER_TRIGER) && (reals.Step_dev_discovery <= WLCC_FRAME_HEADER_FINISH))
    {
        if (NULL == src_ip)
        {
            ESP_LOGE(TAG, "Step_dev_discovery:%d: src_ip is NULL", reals.Step_dev_discovery);
            return IOT_FAIL;
        }
        /*发现帧组帧*/
        return build_device_discovery_frame(buff, reals.Step_dev_discovery, src_ip);
    }
    else if (WLCC_FRAME_HEADER_FINISH_AFTER == reals.Step_dev_discovery)
    {
        /*周期3min,tbd*/
        if (++scnt2 >= 180) // 300
        {
            scnt2 = 0;

            if (NULL == src_ip)
            {
                ESP_LOGE(TAG, "device_discovery_is_ready: src_ip is NULL");
                return IOT_FAIL;
            }
            /*发现帧组帧*/
            return build_device_discovery_frame(buff, reals.Step_dev_discovery, src_ip);
        }
        else if (0 == (scnt2 % 50))
        {
            /*mesh网络心跳帧组帧*/
//            return build_common_heart_frame(buff);  // 使用实际数据帧上报代替心跳包
        }
    }

    // ESP_LOGE(TAG, "device_discovery_is_ready: reals.Step_dev_discovery error:%d", reals.Step_dev_discovery);
    return IOT_FAIL;
}


int wlcc_modbus_rsp_to_self_hanlde(uint8_t *income, int len, wlcc_dev_info_t rsp_addr, modbus_addr_info_t src_pending)
{
	uint8_t funcode = income[1];
	uint16_t regAdderss = income[2] << 8 | income[3];
    uint16_t gRegCnt = 0;

    if (0x06 == funcode)
    {
        gRegCnt = 1;

        if (regAdderss == (((uint16_t)income[2] << 8) | income[3])){
            ESP_LOGW(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] regAdderss : %d, gRegCnt : %d", regAdderss, gRegCnt);
        }
        else{
            ESP_LOGE(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] ERROR");
        }

		return 0;
    }
    else if (0x10 == funcode)
    {
        gRegCnt = income[4] << 8 | income[5];
        if ((regAdderss == 700) && (gRegCnt == 6))
        {
            // uart_ota_recv(cmdBuf, 8);//modbus wr 0x10 700 6 rx   //TODO: OTA
            ESP_LOGW(TAG, "Modbus_WriteReg_06H_10H_RTN_Udp uart_ota_recv  OTA triger!");
        }
        else if ((regAdderss == (((uint16_t)income[2] << 8) | income[3])) && (gRegCnt == (((uint16_t)income[4] << 8) | income[5])))
        {
            ESP_LOGW(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] regAdderss : %d, gRegCnt : %d", regAdderss, gRegCnt);
        }
        else
        {
            ESP_LOGE(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] ERROR:%d, regcnt:%d", regAdderss, gRegCnt);
        }

		return 0;
    }
#if CONFIG_DEV_MODBUS_SUPPORT
	switch(rsp_addr.dev_type)
	{
		case SN_TYPE_S1:
			dev_modbus_s1_data_rsp_handle(rsp_addr.dev_type, rsp_addr.dev_sn, src_pending.regAddr, src_pending.regNum, income, len);
			break;
		case SN_TYPE_METER:
			dev_modbus_meter_data_rsp_handle(rsp_addr.dev_type, rsp_addr.dev_sn, src_pending.regAddr, src_pending.regNum, income, len);
			break;
		case SN_TYPE_AC2AC:
			dev_modbus_other_inv_data_rsp_handle(rsp_addr.dev_type, rsp_addr.dev_sn, src_pending.regAddr, src_pending.regNum, income, len);
			break;
		default:
			break;
	}
#endif
    return 0;
}


int wlcc_modbus_msg_rsp_hanlde(uint8_t *income, int len, wlcc_dev_info_t rsp_addr)
{
	uint8_t modbus_buffer[256];
	wlcc_msg_pending_t msg_pending = wlcc_check_message_response(rsp_addr);

	if(MD_CHL_INVALID == msg_pending.src_addr.channel){
		return -1;
	}

	ESP_LOGI(TAG, "type[%d], Sn[%llu], msgId[%d] rsp OK!", rsp_addr.dev_type, rsp_addr.dev_sn, rsp_addr.msg_id);
	ESP_LOGI(TAG, "msg pending channel[%d]", msg_pending.src_addr.channel);

    switch (msg_pending.src_addr.channel)
    {
#if CONFIG_DEV_MODBUS_SUPPORT
		case MD_CHL_BLE:
			Modbus_Rebuild_Frame_With_Addr(msg_pending.src_addr.slaveAddr, income, len, modbus_buffer);
			ESP_LOGI(TAG, "rebuild data:");
   			ESP_LOG_BUFFER_HEX_LEVEL(TAG, modbus_buffer, len, ESP_LOG_WARN);
			iot_ble_response(modbus_buffer, len, (uint8_t)BLE_FF01_CHAR_VAL); // modbus 响应给手机
            break;
#endif
		case MD_CHL_SELF:
			wlcc_modbus_rsp_to_self_hanlde(income, len, rsp_addr, msg_pending.src_addr);
			break;
		default:
			break;
	}

	wlcc_remove_pending_msg(msg_pending);

	return 0;
}

/**
 * @brief 解析TLV格式的21000帧
 * @param modbus_data Modbus帧数据（不包括长度字段和CRC）
 * @param modbus_data_len Modbus帧数据长度
 * @param src_addr 源设备信息（包含IP地址等）
 *
 * TLV格式的21000帧结构：
 * 协议版本(2B, 大端) + 帧序号(2B, 大端) + 设备数量(2B, 大端) + 设备数据
 * 设备数据 = 设备SN(8B) + 设备类型(2B, 大端) + 后续数据长度(2B, 大端) + TLV数据
 */
static void wlcc_modbus_21000_tlv_frame_handle(const uint8_t *modbus_data, uint16_t modbus_data_len, wlcc_dev_info_t src_addr)
{
	int i;
	uint8_t data_buff[512]; // 暂时定512，后面有更大的长度再改
	reg_position_t reg_position;

    if ((modbus_data == NULL) || (modbus_data_len < 6) || modbus_data_len > sizeof(data_buff))
    {
        ESP_LOGE(TAG, "Invalid TLV frame data: len=%d", modbus_data_len);
        return;
    }

    uint16_t offset = 7; //tlv的内容必然是多个寄存器，因此功能码必须是0x10

    // 协议版本 (2B)
    //      40002-TLV寄存器组合 上报信息（信息单元对象包括TLV）
    //      40003-查询(读取)命令（信息单元对象包括TL）
    uint16_t protocol_ver = ((uint16_t)modbus_data[offset] << 8) | modbus_data[offset + 1];
    offset += 2;
    if (protocol_ver == TLV_PROTOCOL_VERSION_REPORT)
    {
        ESP_LOGI(TAG, "TLV Frame 21000: Protocol Version = %d (REPORT)", protocol_ver);
    }
    else if (protocol_ver == TLV_PROTOCOL_VERSION_QUERY)
    {
        ESP_LOGI(TAG, "TLV Frame 21000: Protocol Version = %d (QUERY)", protocol_ver);
        // TODO: 查询命令当前仅解析，是否需要实现响应逻辑？
    }
    else
    {
        ESP_LOGW(TAG, "TLV Frame 21000: Unknown Protocol Version = %d (0x%04X)", protocol_ver, protocol_ver);
    }

    // 帧序号(2B)
    uint16_t frame_index = ((uint16_t)modbus_data[offset] << 8) | modbus_data[offset + 1];
    offset += 2;
    ESP_LOGI(TAG, "TLV Frame 21000: Frame Index = 0x%04X (%d)", frame_index, frame_index);

    // 设备数量(2B)
    uint16_t device_count = ((uint16_t)modbus_data[offset] << 8) | modbus_data[offset + 1];
    offset += 2;
    ESP_LOGI(TAG, "TLV Frame 21000: Device Count = %d", device_count);

	for(i=0; i<(modbus_data_len-offset-2); i += 2)
	{
		data_buff[i] = modbus_data[offset+i+1];
		data_buff[i+1] = modbus_data[offset+i];
	}

	uint16_t tlv_data_index = 0;
	uint64_t device_sn;
	uint16_t device_type;
	uint16_t *regPtr = NULL;

    // 解析每个设备的数据
    for (uint16_t device_idx = 0; device_idx < device_count; device_idx++)
    {
        // 设备SN (8B)
        if (tlv_data_index + 8 > (modbus_data_len-offset-2))
        {
            ESP_LOGE(TAG, "Device %d: Insufficient data for SN", device_idx + 1);
            break;
        }

        memcpy(&device_sn, &data_buff[tlv_data_index], 8);
        tlv_data_index += 8;
        ESP_LOGI(TAG, "Device %d SN: %llu", device_idx + 1, device_sn);

        // 设备机型简化序号 (2B)
        if (tlv_data_index + 2 > (modbus_data_len-offset-2))
        {
            ESP_LOGE(TAG, "Device %d: Insufficient data for type", device_idx + 1);
            break;
        }
        device_type = ((uint16_t)data_buff[tlv_data_index + 1] << 8) | data_buff[tlv_data_index];
        tlv_data_index += 2;
        ESP_LOGI(TAG, "Device %d Type: (%d)", device_idx + 1, device_type);

        // 后续数据长度 (2B), TLV数据总长度
        if (tlv_data_index + 2 > (modbus_data_len-offset-2))
        {
            ESP_LOGE(TAG, "Device %d: Insufficient data for subsequent length", device_idx + 1);
            break;
        }
        uint16_t subsequent_len = ((uint16_t)data_buff[tlv_data_index + 1] << 8) | data_buff[tlv_data_index];
        tlv_data_index += 2;
        ESP_LOGI(TAG, "Device %d: Subsequent Data Length = %d", device_idx + 1, subsequent_len);

        // 解析TLV数据(包含多个信息单元对象,每个设备最多TLV_MAX_BLOCK_COUNT)
        if (tlv_data_index + subsequent_len > (modbus_data_len-offset-2))
        {
            ESP_LOGE(TAG, "Device %d: Insufficient data for TLV: need %d, have %d",
                    device_idx + 1, subsequent_len, (modbus_data_len-offset-2));
            break;
        }

        if (subsequent_len > 0)
        {
            wlcc_tlv_block_t tlv_blocks[TLV_MAX_BLOCK_COUNT];
            uint16_t parsed_blocks = 0;

            int ret = wlcc_tlv_decode(&data_buff[tlv_data_index],
                                    subsequent_len,
                                    tlv_blocks,
                                    TLV_MAX_BLOCK_COUNT,
                                    &parsed_blocks);

            if (ret == IOT_OK)
            {
                ESP_LOGI(TAG, "Device %d: Parsed %d TLV blocks", device_idx + 1, parsed_blocks);

                // 打印每个TLV块的信息
                for (uint16_t tlv_idx = 0; tlv_idx < parsed_blocks; tlv_idx++)
                {
                    ESP_LOGI(TAG, "  TLV Block %d: Addr=0x%04X (%d), Length=%d",
                            tlv_idx + 1,
                            tlv_blocks[tlv_idx].addr,
                            tlv_blocks[tlv_idx].addr,
                            tlv_blocks[tlv_idx].length);

                    if (tlv_blocks[tlv_idx].length > 0 && tlv_blocks[tlv_idx].value != NULL)
                    {
                        ESP_LOG_BUFFER_HEX_LEVEL(TAG,
                                                tlv_blocks[tlv_idx].value,
                                                tlv_blocks[tlv_idx].length,
                                                ESP_LOG_DEBUG);
                    }
#if CONFIG_DEV_MODBUS_SUPPORT
					switch(device_type)
					{
						case SN_TYPE_S1:
							regPtr = vLookupS1DataTab(device_type, device_sn, tlv_blocks[tlv_idx].addr, tlv_blocks[tlv_idx].length>>1, false);
							break;
						case SN_TYPE_METER:
							regPtr = vLookupMeterDataTab(device_type, device_sn, tlv_blocks[tlv_idx].addr, tlv_blocks[tlv_idx].length>>1, false);
							break;
						case SN_TYPE_AC2AC:
							if(device_sn == iot_factory.iot_sn){
								regPtr = vLookupDataTab(0, tlv_blocks[tlv_idx].addr, tlv_blocks[tlv_idx].length>>1, true, &reg_position);
							}else{
								regPtr = vLookupDataTab_from_other_inv(device_type, device_sn, tlv_blocks[tlv_idx].addr, tlv_blocks[tlv_idx].length>>1, false);
							}
							break;
						default:
							regPtr = NULL;
							break;
					}
#endif
					if (NULL != regPtr)
				    {
				        for (i = 0; i < tlv_blocks[tlv_idx].length; i += 2){
				            regPtr[i / 2] = ((uint16_t)tlv_blocks[tlv_idx].value[i+1] << 8) | tlv_blocks[tlv_idx].value[i]; // H/L
				        }

						// 这个地方要修改成特定项目的类型
						if((SN_TYPE_SELF == device_type) && (device_sn == iot_factory.iot_sn))
						{
							md_data_t *p_data = md_tbl_find(tlv_blocks[tlv_idx].addr);
							if (p_data != NULL)
							{
								/* 表回调函数 */
								if (p_data->tbl.tbl_cb)//检查
								{
									tbl_cb_data_t cb_data = {
											.SlaveAddress = 0,
											.reg_addr_offset = reg_position.offset,
											.reg_addr = tlv_blocks[tlv_idx].addr,
											.reg_nums = tlv_blocks[tlv_idx].length>>1,
											.is_write = true,
											.cb_chl = MD_CHL_WIFI_CLOUD,
									};

							       p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, NULL);
								}
							}
						}
				    }
				    else{
				        ESP_LOGE(TAG, "tlv regAdderss or gRegCnt error");
				    }
                }
            }
            else
            {
                ESP_LOGE(TAG, "Device %d: TLV decode failed: %d", device_idx + 1, ret);
            }

            offset += subsequent_len;
        }
    }

    ESP_LOGI(TAG, "TLV Frame 21000 processed (dev_type:%d, dev_sn:%llu)",
                src_addr.dev_type, src_addr.dev_sn);
}

uint8_t handle_common_modbus_msg(uint8_t *income, uint16_t cmd_len, wlcc_dev_info_t src_addr)
{
    int8_t frame_type = 0;
    int ret = 0;
	uint8_t ip_level = income[WLCC_FRAME_ADDR_IP_LEVEL];
	uint8_t modbus_index_start = 0;
	uint8_t modbus_len = 0;

    ESP_LOGW(TAG, "Modbus Udp cmd_len = %d", cmd_len);
//    ESP_LOG_BUFFER_HEX_LEVEL(TAG, income, cmd_len, ESP_LOG_WARN);

    frame_type = check_modbus_frame_format(income, cmd_len);
    if (IOT_FAIL == frame_type)
    {
        ESP_LOGE(TAG, "frame_type error (%d)", frame_type);
        return IOT_FAIL;
    }

	modbus_index_start = WLCC_FRAME_ADDR_IP_BEGIN + ip_level*4 +2;
	modbus_len = (cmd_len - (WLCC_FRAME_ADDR_IP_BEGIN + ip_level*4 +2) - 2);

    ESP_LOGI(TAG, "frame_type: %d, msg_id:%d", frame_type, src_addr.msg_id);
    ESP_LOGI(TAG, "WLCC recv modbus data:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &income[modbus_index_start], modbus_len, ESP_LOG_WARN);

    switch (frame_type)
	{
	    case WLCC_FRAME_TYPE_READ: // Modbus 读
	    case WLCC_FRAME_TYPE_WRITE: // Modbus 写
	    	if(income[modbus_index_start] < DEV_MODBUS_ADDR_START){
				wlcc_modbus_to_self_handle(&income[modbus_index_start], modbus_len, src_addr);
			}
			else
			{
				modbus_addr_info_t src_dev;
				src_dev.channel = MD_CHL_WIFI_CLOUD;
				src_dev.msg_id = src_addr.msg_id;
				src_dev.dev_type = src_addr.dev_type;
				src_dev.dev_sn = src_addr.dev_sn;
				src_dev.slaveAddr = income[modbus_index_start];
				src_dev.regAddr = (income[modbus_index_start+2]<<8) + income[modbus_index_start+3];
				if(0x06 == income[modbus_index_start+1]){
					src_dev.regNum = 1;
				}else{
					src_dev.regNum = (income[modbus_index_start+4]<<8) + income[modbus_index_start+5];
				}
//				modbus_slave_to_other_hanlde(&income[modbus_index_start], modbus_len, src_dev);
			}
	        break;

	    case WLCC_FRAME_TYPE_READ_RTN: // Modbus 返回
	    case WLCC_FRAME_TYPE_WRITE_RTN:
	        wlcc_modbus_msg_rsp_hanlde(&income[modbus_index_start], modbus_len, src_addr);
	        break;

	    case WLCC_FRAME_TYPE_PERIOD:
	        if ((income[modbus_index_start + 1] == 0x06) || (income[modbus_index_start + 1] == 0x10))
	        {
	            // 周期上报 不做回复
	            if (wlcc_tlv_is_tlv_frame(&income[modbus_index_start], modbus_len) == IOT_OK)
		        {
		            // 确认为TLV格式的21000帧，解析TLV数据（不包括长度字段和CRC）
		            wlcc_modbus_21000_tlv_frame_handle(&income[modbus_index_start], modbus_len, src_addr);
		        }else{
					wlcc_modbus_report_handle(&income[modbus_index_start], modbus_len, src_addr);
				}
	        }
	        else
	        {
	            ESP_LOGE(TAG, "frame_type error (%d)(%u)", frame_type, income[modbus_index_start + 1]);
	        }
	        break;

	    case WLCC_FRAME_TYPE_HEART:
	        ret = wlcc_heartbeat_handle(income);
	        break;

	    default:
	        ESP_LOGE(TAG, "frame_type invalid");
	        break;
	}

    return ret;
}


/**
 * @brief 处理无线设备间通信协议帧
 * @param[in] income  接收的帧数据
 * @param[in] cmd_len  帧数据长度
 * @param[in] src_ip  源设备IP地址
 * @param[in] src_port 源设备端口
 */
int handle_wlcc_frame(uint8_t *income, uint16_t cmd_len, char *src_ip, uint16_t src_port)
{
    int ret = IOT_FAIL;
	wlcc_dev_info_t src_info;

//	time_t now;
//
//	time(&now);

    /*检查是否符合协议规范*/
    ret = check_wlcc_frame_format(income, cmd_len);
    if (IOT_OK != ret){
        return IOT_ERR_WLCC_FORMAT;
    }

	src_info.msg_id = (income[WLCC_FRAME_ADDR_MSG_ID+1]<<8) + income[WLCC_FRAME_ADDR_MSG_ID];
	src_info.dev_type = (income[WLCC_FRAME_ADDR_TYPE_SOURCE+1]<<8) + income[WLCC_FRAME_ADDR_TYPE_SOURCE];
	memcpy(&src_info.dev_sn, &income[WLCC_FRAME_ADDR_SN_SOURCE], 8);
	src_info.dev_priority = (income[WLCC_FRAME_ADDR_PRIORITY+1]<<8) + income[WLCC_FRAME_ADDR_PRIORITY];

	src_info.ip = inet_addr(src_ip);
	src_info.port = src_port;

    uint8_t frame_type = income[WLCC_FRAME_ADDR_HEAD];

    /* 无线设备间通信: 通用协议(Modbus命令) */
    if ((WLCC_FRAME_HEADER_COMMON == frame_type)
		|| (WLCC_FRAME_TYPE_PERIOD == frame_type)
		|| (WLCC_FRAME_TYPE_HEART == frame_type)) // modbus
    {
	    /* 接收到协议数据，也认为是设备发现 */
	    update_device_discovery_info(src_info);

		 /* modbus处理 */
        handle_common_modbus_msg(income, cmd_len, src_info);
    }
    /* 无线设备间通信: 设备发现协议 */
    else if ((frame_type >= WLCC_FRAME_HEADER_TRIGER) && (frame_type <= WLCC_FRAME_HEADER_FINISH_AFTER)) // discovery
    {
        /* 触发帧 */
        if (WLCC_FRAME_HEADER_TRIGER == frame_type)
        {
            reals.Step_dev_discovery = WLCC_FRAME_HEADER_SEND_SN;
			wlcc_master_triger();
        }
        else if ((WLCC_FRAME_HEADER_FINISH == frame_type) || (WLCC_FRAME_HEADER_FINISH_AFTER == frame_type)) /*设备存储状态刷新*/
        {
            /* 设备发现完成, 存储设备信息 */
            update_device_discovery_info(src_info);
        }

		if(WLCC_FRAME_HEADER_FINISH == frame_type){
			wlcc_master_pk(src_info);
		}

        /*上报帧SN解析*/
        // Udp_Multicast_Finish_After(income);
    }

    return ret;
}

