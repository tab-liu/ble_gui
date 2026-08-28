#include <string.h>

#include "driver/uart.h"
#include <driver/gpio.h>
#include <sys/unistd.h>
#include <sys/select.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "app_uart.h"
#include "iot_period_task.h"
#include "uart_device_process.h"
#include "app_time.h"
#include "modbus_slave_data.h"
#include "modbus_data.h"
#include "modbus_slave.h"
//#include "cc1312_ota.h"
#include "can_data.h"
#include "dev_discovery.h"
#include "udp_multicast.h"
#include "can_protocol.h"

#include "filesystem.h"
#include "modbus_protocol.h"
#include "util_swap.h"

static const char *TAG = "[UART_DEV_PROCESS]";

//0-汇总；1~x-单个节点
//AC380,不用如下变量
//EXT_RAM_BSS_ATTR MOD_STRUCT_MicroInv	MicroInv[MAX_NUM_MICRO_INV];//微逆设备modbus beta总变量结构体
//EXT_RAM_BSS_ATTR MOD_STRUCT_MicroInv	MicroInv_WR;//[MAX_NUM_MICRO_INV];//modbus写缓存,待改为谢单结构体



EXT_RAM_BSS_ATTR SELF_DATA_STRUCT  g_self_data;//modbus beta
//EXT_RAM_BSS_ATTR SELF_DATA_STRUCT  g_self_data_WR;

//GRID_METER_UNION    gModbusGridRegs;//电表 tbd

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

/*------------------------------------------------------------------------------
 Function: convertToUint64
 -----------------------------------------------------------------------------*/
/**
  * @brief      uint16_t i[4]转化为uint64_t （用于SN码标定）
  * @param[in]  uint16_t *i  
  * @param[out] None
  * @return     uint64_t
  */
uint64_t convertToUint64(uint16_t *i) 
{
    uint64_t result = 0;
    for (int j = 0; j < 4; j++) {
        result |= ((uint64_t)i[j]) << (16 * (3-j));
    }
    return result;

}
/*------------------------------------------------------------------------
*@Function： Modbus_MasterReadCmd_03H
-------------------------------------------------------------------------*/
/**
*@brief  100ms cycle  组帧
*@param[regAddress]     reg address
*@param[regNum]    reg num
*@param[*cmdbuf]     data
*@param[type]    slave address


*@return      frame len   
*/
uint16_t Modbus_MasterReadCmd_03H(uint16_t regAddress, uint8_t regNum, uint8_t *outbuf, uint8_t slave_address)
{
    uint16_t crc;
    uint8_t i = 0;

    
//    gFunCode    = 0x03;
//    gRegCnt     = regNum;
//    gRegAddress = regAddress;
    
    outbuf[i++] = slave_address;
    outbuf[i++] = 0x03;
    outbuf[i++] = (uint8_t)(regAddress >> 8);
    outbuf[i++] = (uint8_t) regAddress;
    outbuf[i++] = (uint8_t)(regNum >> 8);
    outbuf[i++] = (uint8_t) regNum;
    
    crc = ModbusCrc16(outbuf, i);
    
    outbuf[i++] = (uint8_t) crc;
    outbuf[i++] = (uint8_t)(crc>>8);
    
    return i;
}
#if 1
/**
*@brief  100ms cycle 组帧
*@param[regAddress]     reg address
*@param[regNum]    reg num
*@param[*cmdbuf]     data
*@param[type]    slave address


*@return         
*/
uint16_t Modbus_MasterWriteCmd_06H_10H(uint16_t regAddress, uint8_t regNum, bool is_write, uint8_t *outbuf, uint8_t slave_address)
{
    uint16_t crc;
    uint8_t i = 0, j = 0;
	ESP_LOGI(TAG,"Modbus_MasterWriteCmd_06H_10H");
    reg2_position_t reg_position;
    // 查询table2中的数据
    can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};
    if (is_write)
    { // 当MODBUS为设置指令时,才需要开辟空间
        can_cmd.cmd = heap_caps_malloc(sizeof(can_data_label) * can_cmd.num, MALLOC_CAP_SPIRAM);
        if (!can_cmd.cmd)
        {
            ESP_LOGE(TAG, "ble to can malloc failed");
        }
    }
    const uint16_t *data = vLookupDataTab_Can(MASTER_BLE_WIFI, slave_address, regAddress,
                            regNum, is_write, can_cmd.cmd, &can_cmd.num, &reg_position);
    if(data == NULL)
    {
        ESP_LOGE(TAG, "Modbus_MasterWriteCmd_06H_10H vLookupDataTab_Can UNKNOWN_REG_ADDRESS");
        goto end;
    }

    outbuf[i++] = slave_address;

    if (regNum == 1)
    {
        outbuf[i++] = 0x06; // write single 
    }
    else 
    {
        outbuf[i++] = 0x10; // write muitl 
    }
    outbuf[i++] = (unsigned char)(regAddress >> 8);
    outbuf[i++] = (unsigned char) regAddress;

    if (outbuf[1] == 0x10)
    {
        outbuf[i++] = (unsigned char)(regNum >> 8);
        outbuf[i++] = (unsigned char) regNum;
        outbuf[i++] = regNum << 1; //  
    }
    
    while(regNum--)
    {
        //outbuf[i++] = (unsigned char)(data[j] >> 8);
        
        outbuf[i++] =  (data[j]>>8)&0xFF;//H
        outbuf[i++] =  data[j]&0xFF;//L
        j++;
//        j++;
    }
    
    crc = ModbusCrc16(outbuf,i);
    
    outbuf[i++] = (unsigned char) crc;
    outbuf[i++] = (unsigned char)(crc>>8);

end:
    if (is_write && can_cmd.cmd)
    {
        free(can_cmd.cmd);
    }

    return i;
}
#else
uint16_t Modbus_MasterWriteCmd_06H_10H(uint16_t regAddress, uint8_t regNum, const uint16_t *data, uint8_t *outbuf, uint8_t slave_address)
{
    uint16_t crc;
    uint8_t i = 0, j = 0;
    
    if(data == NULL)
	{
        ESP_LOGE(TAG, "Modbus_MasterWriteCmd_06H_10H vLookupDataTab UNKNOWN_REG_ADDRESS");
		return 0;
	}
    
	outbuf[i++] = slave_address;

    if (regNum == 1)
    {
        outbuf[i++] = 0x06; // write single 
    }
    else 
    {
        outbuf[i++] = 0x10; // write muitl 
    }
    outbuf[i++] = (unsigned char)(regAddress >> 8);
    outbuf[i++] = (unsigned char) regAddress;

    if (outbuf[1] == 0x10)
    {
        outbuf[i++] = (unsigned char)(regNum >> 8);
        outbuf[i++] = (unsigned char) regNum;
        outbuf[i++] = regNum << 1; //  
    }
    
    while(regNum--)
    {
        //outbuf[i++] = (unsigned char)(data[j] >> 8);
        
        outbuf[i++] =  (data[j]>>8)&0xFF;//H
        outbuf[i++] =  data[j]&0xFF;//L
        j++;
    }
    
    crc = ModbusCrc16(outbuf,i);
    
    outbuf[i++] = (unsigned char) crc;
    outbuf[i++] = (unsigned char)(crc>>8);
    
    return i;
}
#endif

/*-----------------------------------------------------------------------------------------------------------------------*/

#define FCTY_TYPE_ADDR	    29701   // 标定协议-设备类型地址
#define FCTY_SN_ADDR	    29713   // 标定协议-设备SN地址
#define FCTY_CODE_ADDR	    29717   // 标定协议-设备安全码地址

#define FCTY_HIGH_IN_FRONT_ENABLE   // 标定协议-高位在前使能

static uint8_t cal_running = 0;		// 标定运行标志

/**
 * @brief 出厂标定应用程序
 * - 该程序会判断该设备是否已被标定，如果已标定则直接退出
 * - 否则进入标定程序运行状态。如果在一定时间未接收到标定上位机的数据，则退出
 * - 当标定完成后，需要手动复位设备（标定时关闭关机校验）
 * 
 */
void fcty_cal_app(void)
{
    const uint64_t Default_Safe_code = 0;
    const uint32_t Delay_time_ms = 20000;
    factory_struct old_factory;

    app_uart0_init_task();
    vTaskDelay(pdMS_TO_TICKS(500));                            // 500ms延迟，等待串口任务运行正常

	/* 如果设备SN不是默认SN，则设备已被标定，直接返回 */
	if((strncmp(iot_factory.iot_type, IOT_TYPE_IOT, sizeof(iot_factory.iot_type)))
        ||(iot_factory.iot_sn == MASS_PRODUCTION_DEFAULT_DEV_SN)
        ||(iot_factory.safe_code == Default_Safe_code)
        ||(cal_running == 1))
    {   
        uint8_t update_cfg0 = 0;
        uint8_t update_cfg1 = 0;
        
        ESP_LOGW (TAG, "old iot type: %s, iot sn: %llu, iot safet: %llu",
          iot_factory.iot_type,
          iot_factory.iot_sn,
          iot_factory.safe_code);
    	ESP_LOGW(TAG, "enter factory calibration procedure...");
        
    	/* 关闭所有日志输出，避免对上位机接收造成影响 */
    	esp_log_level_set("*", ESP_LOG_NONE);

    	/* 记录标定程序开始时间 */
    	uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        memcpy(&old_factory, &iot_factory, sizeof(iot_factory));
        memcpy(iot_factory.iot_type, IOT_TYPE_IOT, sizeof(IOT_TYPE_IOT));
        
    	for ( ; ; )
    	{
            if (iot_factory.iot_sn != old_factory.iot_sn) {
                ESP_LOGW (TAG, "new iot sn: %llu", iot_factory.iot_sn);
                update_cfg0 = 1;
            }

            if (iot_factory.safe_code != old_factory.safe_code) {
                ESP_LOGW (TAG, "new iot safecode: %llu", iot_factory.safe_code);
                update_cfg1 = 1;
            }
            
    		/* 当启动后，如果在一定时间内未进行标定操作，则退出标定流程 */
    		uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (cal_running == 1)
            {
                cal_running = 0;
                start_time = now_time;
            }
    		if (((now_time - start_time) >= Delay_time_ms)||(update_cfg0 & update_cfg1))//20s
    		{                
                if (update_cfg0 & update_cfg1) 
            	{
                    size_t data_len = sizeof(iot_factory);
                    iot_wtite_dev_info(IOT_FACTORY, (const uint8_t *)&iot_factory, data_len);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    iot_read_dev_info(IOT_FACTORY, (uint8_t *)&iot_factory, &data_len); /* 读取IOT出厂信息 */
                    
                    #ifndef MASS_PRODUCTION_CONFIG_ENABLE  //调试模式：软件复位关机校验

                        app_uart0_close_tasks();//停止串口交互，预备关机校验
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        esp_restart();
                    
                    #else                           //出厂模式：硬件下电关机校验（预留软件复位协议接口）

                        while(1)
                        {
                            //if(Modbus_WR.mod_reg30900_Test.TestMode_Set == 0xA2) {
                               // esp_restart();
                            //}
                            vTaskDelay(pdMS_TO_TICKS(200));
                        }
                    
                    #endif

                }
                app_uart0_close_tasks();
                #ifndef MASS_PRODUCTION_CONFIG_ENABLE
     		        esp_log_level_set("*", ESP_LOG_DEBUG);
                #endif
    			ESP_LOGW(TAG, "exit factory calibration procedure");    
                
    			break;
    		}
    		vTaskDelay(pdMS_TO_TICKS(10));
    	}
    }
    else
    {
        app_uart0_close_tasks();
    }
}

/**
 * @brief 标定错误响应
 *
 * @param buff_in 输入缓存
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
static void fcty_rsp_error(uint8_t *buff_in, uint8_t *buff_out, int *out_len)
{
	uint8_t idx = 0;
	buff_out[idx++] = buff_in[0];
	buff_out[idx++] = buff_in[1] | MB_FUNCODE_ERROR;
	buff_out[idx++] = MB_ERROR_NOT_SUPPORTED;
	uint16_t crc = ModbusCrc16(buff_out, idx);
	buff_out[idx++] = crc;
	buff_out[idx++] = crc >> 8;
	*out_len = idx;
}

/*------------------------------------------------------------------------------
 Function: reverseArray
 -----------------------------------------------------------------------------*/
/**
  * @brief      数组反转
  * @param[in]  uint8_t* array  
                int length      
  * @param[out] None
  * @return     static void
  */
static void reverseArray(uint8_t* array, int reg_write_nums) {
    int start = 0;
    int end = reg_write_nums*2 - 1;
    while (start < end) {
        // 交换元素
        uint8_t temp = array[start];
        array[start] = array[end];
        array[end] = temp;
        // 移动指针
        start++;
        end--;
    }
}

/**
 * @brief 标定读数据处理
 * 
 * @param buff_in 输入缓存
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
void fcty_read_handler(uint8_t *buff_in, uint8_t *buff_out, int *out_len)
{
	uint16_t reg_addr = ((uint16_t)buff_in[2] << 8) | buff_in[3];
	uint16_t reg_nums = ((uint16_t)buff_in[4] << 8) | buff_in[5];
    uint16_t reg_write_nums = 0;
	/* 组装modbus读响应数据 */
	int idx = 0;
    int k;
	buff_out[idx++] = buff_in[0];		// 地址
	buff_out[idx++] = buff_in[1];		// 功能码
	buff_out[idx++] = reg_nums * 2;		// 字节数

	for (int i = 0; i < reg_nums; )
	{
		reg_addr += reg_write_nums;
		uint16_t *pdata = (uint16_t*)&buff_out[idx];

		if (reg_addr == FCTY_TYPE_ADDR)			// 读设备类型
		{
			memcpy(&buff_out[idx], iot_factory.iot_type, sizeof(iot_factory.iot_type));
            reg_write_nums = (sizeof(iot_factory.iot_type) / 2);
			i += reg_write_nums;

			/* 每个寄存器高低字节交换 */
			for (int cnt = 0, k = reg_addr; k < (reg_addr + reg_write_nums); k++, cnt++) {
				pdata[cnt] = swap_htons(pdata[cnt]);
			}

            idx += sizeof(iot_factory.iot_type);
		}
		/* 整形数据的高低字节序由标定文件定义 */
		else if (reg_addr == FCTY_SN_ADDR)		// 读设备SN
		{
			memcpy(&buff_out[idx], &iot_factory.iot_sn, sizeof(iot_factory.iot_sn));
            reg_write_nums = (sizeof(iot_factory.iot_sn) / 2);
			i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
            
			reverseArray((uint8_t*)pdata, reg_write_nums);

            #endif
            
			idx += sizeof(iot_factory.iot_sn);
		}
		/* 整形数据的高低字节序由标定文件定义 */
		else if (reg_addr == FCTY_CODE_ADDR)	// 读设备安全码
		{
			memcpy(&buff_out[idx], &iot_factory.safe_code, sizeof(iot_factory.safe_code));
            reg_write_nums = (sizeof(iot_factory.safe_code) / 2);
			i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
            
			reverseArray((uint8_t*)pdata, reg_write_nums);

            #endif
            
			idx += sizeof(iot_factory.safe_code);
		}
		else
		{
			fcty_rsp_error(buff_in, buff_out, out_len);
			return;
		}
	}

	/* 计算数据CRC */
	uint16_t crc = ModbusCrc16(buff_out, idx);
	buff_out[idx++] = crc;
	buff_out[idx++] = crc >> 8;
	*out_len = idx;
}

/**
 * @brief 标定写数据处理
 * 
 * @param buff_in 输入缓存
 * @param in_len 输入长度
 * @param buff_out 输出缓存
 * @param out_len 输出长度
 */
void fcty_write_handler(uint8_t *buff_in, int in_len, uint8_t *buff_out, int *out_len)
{
	uint16_t reg_nums;
	uint16_t *pdata;
	uint16_t reg_addr = ((uint16_t)buff_in[2] << 8) | buff_in[3];
    uint16_t reg_write_nums = 0;
    int k;

	/* 判断是否是写单个寄存器还是多个寄存器 */
	if (buff_in[1] == MB_WRITE_ONE_HOLD_REG)
	{
		reg_nums = 1;
		pdata = (uint16_t*)&buff_in[4];
	}
	else
	{
		reg_nums = ((uint16_t)buff_in[4] << 8) | buff_in[5];
		pdata = (uint16_t*)&buff_in[7];
	}

	for (int i = 0; i < reg_nums; i++)
	{
		reg_addr += reg_write_nums;

		/* 整形数据的高低字节序由标定文件定义（此处为HSB格式） */
		if (reg_addr == FCTY_SN_ADDR)			// 标定设备SN
		{
		    reg_write_nums = (sizeof(iot_factory.iot_sn) / 2);
            i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
            
        	reverseArray((uint8_t*)pdata, reg_write_nums);

            #endif
            
			memcpy(&iot_factory.iot_sn, pdata, sizeof(iot_factory.iot_sn));
			pdata += sizeof(iot_factory.iot_sn);
		}
		/* 整形数据的高低字节序由标定文件定义（此处为HSB格式） */
		else if (reg_addr == FCTY_CODE_ADDR)	// 标定设备安全码
		{
		    reg_write_nums = (sizeof(iot_factory.safe_code) / 2);
		    i += reg_write_nums;

            #ifdef FCTY_HIGH_IN_FRONT_ENABLE
            
        	reverseArray((uint8_t*)pdata, reg_write_nums);

            #endif
            
			memcpy(&iot_factory.safe_code, pdata, sizeof(iot_factory.safe_code));
			pdata += sizeof(iot_factory.safe_code);
		}
		else
		{
			fcty_rsp_error(buff_in, buff_out, out_len);
			return;
		}
	}

	/* 响应写单个寄存器 */
	if (buff_in[1] == MB_WRITE_ONE_HOLD_REG)
	{
		memcpy(buff_out, buff_in, in_len);
		*out_len = in_len;
	}
	/* 响应写多个寄存器 */
	else
	{
		memcpy(buff_out, buff_in, 6);
		*(uint16_t*)&buff_out[6] = ModbusCrc16(buff_out, 6);
		*out_len = 8;
	}
}

/**
 * @brief 串口接收回调函数
 * 
 * @param handle 串口对象句柄
 * @param buff 接收缓存
 * @param len 数据大小
 */
void serial_recv_callback(UART_STRUCT *uart_struct)
{
    uint8_t *buff = uart_struct->Rxbuffer;
    int len = uart_struct->rxBytesNum;
    
	/* 标定协议检测 */
	if (md_protocol_check(buff, len) != 0) return;

	/* 置位标定运行状态 */
	cal_running = 1;

	uint8_t rsp_buff[256];
	int rsp_len;

	/* 标定读数据处理 */
	if (buff[1] == MB_READ_HOLD_REG) {
		fcty_read_handler(buff, rsp_buff, &rsp_len);
	}
	/* 标定写数据处理 */
	else {
		fcty_write_handler(buff, len, rsp_buff, &rsp_len);
	}

	/* 发送标定响应数据 */
	app_write_uart0_data(rsp_buff, rsp_len);
}
