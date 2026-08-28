/**@file iot_4g_at.c
* @brief 4g模块AT指令封装
* @details 此文件实现AT指令集封装，包括模组初始化，获取信息，TCP连接，mqtt登录，获取gps定位等等，
*           注意不同厂商4g模组指令有所不同，此文件测试模组为广和通NL668
* @author zhongdongming
* @date 2023-12-12
* @version V1.0
* @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
********************************************************************************
* @attention
* 硬件平台: ESP32\NL668
* SDK 版本: ESP_IDF_V4.4.6
* @par 修改日志:
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2023/1/25 <td>1.0 <td>zhongdongming <td>创建初始版本
* </table>
********************************************************************************
*/

#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "password.h"
#include "Cellular_4g_at.h"
#include "iot_mqtt.h"
#include "app_uart.h"
#include "lwip/inet.h"
#include "iot_period_task.h"
#include "Cellular_4g_handle.h"
#include "utils.h"

static const char *TAG = "[4G_AT_CMD]";





//-------------------- 函数列表 --------------------------------------------
static int SendATCmd(const uint8_t *data, uint16_t len);
static int parse_gps_info(char *rsp_ptr);
static void reset_4g_state(void);
static uint16_t hex2str(uint8_t *hex, char *str, uint16_t len);
static uint16_t str2hex(char *str, uint8_t *hex, uint16_t len);
void parse_mqtt_msg(char *msg_ptr, uint16_t msg_len, char *send_topic);
//-------------------- 变量列表 --------------------------------------------
static iot_4g_state_t iot_4g_state = { .byte = 0xFF };       // 4g状态位
static iot_4g_data_t iot_4g_data = {0};         // 4g相关数据

 char ATSendBuff[AT_SEND_BUF_SIZE]={0};   // AT指令发送缓存 static

/*------------------------------------------------------------------------
*@Function： SendATCmd
uart 发送AT指令封装
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return 
(-1) Parameter error
OTHERS (>=0) The number of bytes pushed to the TX FIFO

*/
static int SendATCmd(const uint8_t *data, uint16_t len) 
{
	int ret=0;
//清除rx缓存
	memset(reals.AT_Cmd_RxPointer, 0, AT_RECV_BUF_SIZE);
	reals.struct_uart2.FlagRx_ok=0; 	
	reals.struct_uart2.rxBytesNum=0;
    if (len <= AT_SEND_BUF_SIZE) 
    {
		ret =app_write_uart2_data(data, len); //tbd 更改串口收发驱动后请修改此函数
	}
	else
	{
        ESP_LOGE(TAG, "Error: Uart tx Data is too large!!");
    }

     return ret;
}


/**
*@Function： RecvATAck
uart rx 接收超时等待/阻塞
收到任何返回都算收到

* @brief :
* @param[in] :	maxlen,接收的最大限定长度   
* @param[out] : 
* @return :	 uart rx len

*/
static uint16_t RecvATAck( uint16_t maxlen, int timeout_cnt_ms)
{
//    uint16_t RecvAckLen = 0;	
//	vTaskDelay(pdMS_TO_TICKS(2000)); //timeout_cnt_ms
//	if(1 == reals.struct_uart2.FlagRx_ok)//收到退出
//	{
//		RecvAckLen=reals.struct_uart2.rxBytesNum;	
//	}
//	else//超时退出
//	{
//		RecvAckLen=0;
//	}


	uint16_t RecvAckLen = 0;
//	char *ret = NULL;
	uint16_t timeout=0;
	timeout =timeout_cnt_ms/100;//15;//15000;//;
	while(timeout--)
	{
//		ret = strstr((char *)reals.AT_Cmd_RxPointer, Ack);//strstr，查找相等字符串，返回在 haystack 中第一次出现 needle 字符串的位置，如果未找到则返回 null。
		if(//(ret)&&
			(reals.struct_uart2.rxBytesNum <= maxlen)
			&&(1 == reals.struct_uart2.FlagRx_ok)
			)
		{
			RecvAckLen=reals.struct_uart2.rxBytesNum;	
//			ESP_LOGI(TAG, "ACK:RecvATAck1： %s", reals.AT_Cmd_RxPointer);
	//			ESP_LOGW(TAG, "ACK:RecvATAck2： %s", reals.struct_uart2.Rxbuffer);
			break;
		}
		else//超时退出
		{
			RecvAckLen=0;
			vTaskDelay(pdMS_TO_TICKS(100)); //实测太小1，不为1ms
		}
	}
	return RecvAckLen;// 

}



/**
*@Function： RecvATAck
uart rx 接收指定的字符串，超时等待/阻塞；强制等待
必须收到指定内容才算接收成功
* @brief :
* @param[in] :	   
* @param[out] : 
* @return :	 成功接收到指定内容后的uart rx len

*/
static uint16_t RecvATAck2(char *Ack, uint16_t timeout_cnt_ms)
{
    uint16_t RecvAckLen = 0;
	char *ret = NULL;
	uint16_t timeout=0;
	timeout =timeout_cnt_ms/100;//15;//15000;//;
	while(timeout--)
	{
		ret = strstr((char *)reals.AT_Cmd_RxPointer, Ack);//strstr，查找相等字符串，返回在 haystack 中第一次出现 needle 字符串的位置，如果未找到则返回 null。
		if((1 == reals.struct_uart2.FlagRx_ok)
			&&(ret))
		{
			RecvAckLen=reals.struct_uart2.rxBytesNum;	
//			ESP_LOGI(TAG, "ACK:RecvATAck2： %s", reals.AT_Cmd_RxPointer);
//			ESP_LOGW(TAG, "ACK:RecvATAck2： %s", reals.struct_uart2.Rxbuffer);
			break;
		}
		else//超时退出
		{
			RecvAckLen=0;
			vTaskDelay(pdMS_TO_TICKS(100)); //实测太小1，不为1ms
		}
	}
    return RecvAckLen;// 
}


/**
 * @brief 发送4G命令并等待确认。
 *
 * 此函数向4G模块发送指定的AT命令并等待确认。
 * 如果失败，它允许多次尝试接收确认。
 *
 * @param ATcmd 要发送的AT命令。
 * @param Ack 预期的确认。
 * @param Counts 尝试接收确认的次数。
 * @param WaitTime 等待确认的最大时间。
 * @return 如果找到，则返回指向确认字符串的指针，否则返回NULL。
 */
 char *Send4GCmd(char *ATcmd, char *Ack, uint8_t Counts, uint16_t WaitTime)//static
{
	uint8_t attempt = 0;
	uint16_t RecvAckLen = 0;
	char *ret = NULL;
   if((NULL != ATcmd)&&(NULL != Ack)) 
   {
	   for (uint8_t i = 0; i < Counts; i++) 
	   {
		   attempt =i+1;
		   SendATCmd((uint8_t *)ATcmd, strlen(ATcmd));
		   SendATCmd((uint8_t *)"\r\n", 2);
		   ESP_LOGI(TAG, "AT: %s", ATcmd);
	   
		   RecvAckLen = RecvATAck2( Ack, WaitTime);
		   if (RecvAckLen > 0) 
		   {
//			   ESP_LOGI(TAG, "ACK: %s", reals.AT_Cmd_RxPointer);
			   ret = strstr((char *)reals.AT_Cmd_RxPointer, Ack);//strstr，查找相等字符串，返回在 haystack 中第一次出现 needle 字符串的位置，如果未找到则返回 null。
			   if (ret)
			   {
				   break; // 找到目标字符串，直接返回
			   } 
			   else if(strstr((char *)reals.AT_Cmd_RxPointer, "ERROR"))
			   {
				   ESP_LOGW(TAG, "ERROR CMD");
				   break;
			   } 
			   else 
			   {
				   // 非预期应答
			   }
		   } 
	   }

   }
   else
   {
        ESP_LOGE(TAG, "Invalid input: ATcmd or Ack is NULL");
   }

   ESP_LOGI(TAG, "Send4GCmd Attempt cnt =%d", attempt);

    return ret; // 
}

void uart_tx_debug(void) 
{
	int ret=0;
//清除rx缓存
	memset(reals.AT_Cmd_RxPointer, 0, AT_RECV_BUF_SIZE);
	reals.struct_uart1.FlagRx_ok=0; 	
	reals.struct_uart1.rxBytesNum=0;
	memcpy(reals.struct_uart1.Txbuffer, "hello windy tx uart1", 20);
	ret =app_write_uart1_data(reals.struct_uart1.Txbuffer, strlen((char *)reals.struct_uart1.Txbuffer)); //tbd 更改串口收发驱动后请修改此函数

	reals.struct_uart2.FlagRx_ok=0; 	
	reals.struct_uart2.rxBytesNum=0;
	memcpy(reals.struct_uart2.Txbuffer, "hello windy tx uart2", 20);
	ret =app_write_uart1_data(reals.struct_uart2.Txbuffer, strlen((char *)reals.struct_uart2.Txbuffer)); //tbd 更改串口收发驱动后请修改此函数

    ESP_LOGW(TAG, "windy uart tx buff=%s",reals.struct_uart2.Txbuffer);


}


iot_4g_state_t get_iot_4g_state(void) {
    return iot_4g_state;
}

iot_4g_data_t get_iot_4g_data(void) {
    return iot_4g_data;
}

/*--------------------------------- private ------------------------------------------------------------*/
void stop_4g_module(void) {
    iot_4g_state.bit.ComSwitch = 2; // 关闭
    //TODO 关闭4G模块电源
}
void start_4g_module(void) {
    iot_4g_state.bit.ComSwitch = 1; // 开启
    //TODO 开启4G模块电源
}

// 将字符串转换成十六进制数组
static uint16_t str2hex(char *str, uint8_t *hex, uint16_t strlen) {
    unsigned int value;
    for (uint16_t i = 0; i < strlen; i += 2) {
        sscanf(&str[i], "%2x", &value);
        hex[i / 2] = (uint8_t)value;
    }
    return strlen / 2;
}

// 将十六进制数组转换成ASCII字符串,即一个数字字节实际用2个字节的ASCII表示
static uint16_t hex2str(uint8_t *hex, char *str, uint16_t hexlen) {
    for(uint16_t i = 0; i < hexlen; i++){
        sprintf(str + i*2, "%02X", hex[i]);
    }
    return hexlen * 2;
}

static void reset_4g_state(void){
    iot_4g_state.bit.sim_missing = 1;
    iot_4g_state.bit.sim_inactive = 1;
    iot_4g_state.bit.netdial_fail = 1;
    iot_4g_state.bit.mqtt_login_fail = 1;
    iot_4g_state.bit.GpsFail = 1;
    iot_4g_state.bit.ComSwitch = 1; // 开启
}


int iot_4g_reset(void) 
{
    reset_4g_state();

    if(!Send4GCmd("AT+GTGPSPOWER=0", "OK", AT_RETRY_TIMES, AT_WAIT_TIME))
	{ // 关闭GPS
        ESP_LOGE(TAG,"GPS power off failed");
    }
    if(!Send4GCmd("AT+RESET","AT command ready", AT_RETRY_TIMES, 15000))
	{ // 重启模块
        ESP_LOGE(TAG, "reset 4g module failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}



// /**
//  * @brief 获取4G信号强度
//  * 
//  * 该函数用于获取4G模块的信号强度。
//  * 函数通过发送AT指令"AT+CSQ"来获取信号强度，并解析返回的响应数据。
//  * 如果成功获取到信号强度，则返回信号强度值；否则返回0。
//  * 函数最多尝试40次获取信号强度，如果40次都未成功，则返回0。
//  * 
//  * @return int 4G信号强度值，范围为0到31，0表示无信号，31表示最强信号。
//  */
// int get_4g_rssi(void){
//     char *ack = NULL;
//     for(uint8_t i = 0; i < 40; i++){
//         ack = Send4GCmd("AT+CSQ","+CSQ:",1,AT_WAIT_TIME);
//         if(ack) {
//             int signal_strength;
//             sscanf(ack, "+CSQ:%d:%*d", &signal_strength);
//             return signal_strength;
//         }
//     }
//     return 0;
// }


/**
 * @brief 通过发送AT命令并检查响应来初始化4G模块。
 * 
 * @return 如果初始化成功，则返回ESP_OK，否则返回ESP_FAIL。
 */
int Init4GModule(void)
{
    char *ack;
    uint8_t i;
    reset_4g_state();
    // 检测模块AT指令状态，回复OK可以正常发送AT指令    
    if(!Send4GCmd("AT", "OK", 10, AT_WAIT_TIME)) { 
        ESP_LOGE(TAG, "handshake failed");
        return ESP_FAIL;
    }

    // 取消回显             
    if(!Send4GCmd("ATE0", "OK", AT_RETRY_TIMES, AT_WAIT_TIME)){
        ESP_LOGE(TAG, "echo cancelled failed");
    }
    
    // 读取模块SN码 每个模块唯一
    if(!Send4GCmd("AT+CGSN","OK", AT_RETRY_TIMES, AT_WAIT_TIME)){
        ESP_LOGE(TAG, "IMEI SN readed failed");
    }

    // 读取模块固件版本号
    if(!Send4GCmd("AT+CGMR", "OK", AT_RETRY_TIMES, AT_WAIT_TIME)){
        ESP_LOGE(TAG, "IMEI version readed failed");
    }

    // 查询移动设备的国际移动用户识别码
    if(!Send4GCmd("AT+CIMI", "OK", AT_RETRY_TIMES , AT_WAIT_TIME)){                
        ESP_LOGE(TAG, "serching IMSI failed");
    }

    // 查询模块固件版本号，便于问题分析
    if(!Send4GCmd("ATI", "OK", AT_RETRY_TIMES , AT_WAIT_TIME)){                
        ESP_LOGE(TAG, "check module version failed");
    }
    // 设置正常工作模式
    if(!Send4GCmd("AT+CFUN?", "+CFUN: 1", AT_RETRY_TIMES , AT_WAIT_TIME)){                
        if(!Send4GCmd("AT+CFUN=1", "OK", AT_RETRY_TIMES , AT_WAIT_TIME)){
            ESP_LOGE(TAG, "set work mode failed");
        }
    }
    // 查询核心板能否读到SIM卡(必选)
    if(!Send4GCmd("AT+CPIN?", "+CPIN: READY", 30, AT_WAIT_TIME)){
       ESP_LOGE(TAG, "no sim card detected!");
       iot_4g_state.bit.sim_missing = 1;
       return ESP_FAIL;
    }
    iot_4g_state.bit.sim_missing = 0;
    // 查询信号强度RSSI：    0-9：低信号质量 10-19：中等信号质量
    //                      20-31：良好信号质量 99: 不可知或不可测量
    for(i = 0; i < 40; i++){
        ack = Send4GCmd("AT+CSQ","+CSQ:",1,AT_WAIT_TIME);
        if(ack) {
            int rssi;
            sscanf(ack, "+CSQ:%d:%*d", &rssi);
            if(rssi >= 10 && rssi <= 31){ 
                iot_4g_state.rssi = rssi;
                break; // 信号强度在此区间才进行下一步
            }
        } else{ // 查询失败发CPMEE
            Send4GCmd("AT+CPMEE=2","OK",1,AT_WAIT_TIME);
        }
        if(i == 39){
            ESP_LOGE(TAG, "signal weak");
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 间隔1s查询一次信号强度
    }

    // 查询电话卡注册状态
    // 注意:在国内除了港澳台，可能因为卡配置问题出现假漫游，
    // 注册网络时间与 SIM 卡和环境有关，例如在海外使用 NL668 模块时，
    // 最长注册网络时间可能会达到 7 分钟。 
    for(i = 0; i < 40; i++){
        ack = Send4GCmd("AT+CREG?","+CREG",1,AT_WAIT_TIME);
        if(ack) {
            int state;
            sscanf(ack, "+CREG: %*d,%d", &state);
            ESP_LOGI(TAG,"state %d", state);
            if(state == 1 || state == 5){
                iot_4g_state.bit.sim_inactive = 0;
                break;
            }
        }
        if(i == 39)
        {
            ESP_LOGE(TAG, "network reg checked failed");
            iot_4g_state.bit.sim_inactive = 1;
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 设置附着
    if(!Send4GCmd("AT+CGATT=1","OK",AT_RETRY_TIMES,AT_WAIT_TIME))
    {
        ESP_LOGE(TAG, "packet domain detach");
    }

    for(i = 0; i < 40; i++){
        if(Send4GCmd("AT+CGATT?", "+CGATT: 1", AT_RETRY_TIMES, AT_WAIT_TIME)){                
            break;
        }
        if(!Send4GCmd("AT+CGATT=1","OK",AT_RETRY_TIMES,AT_WAIT_TIME)){
           ESP_LOGE(TAG, "packet domain detach");
        }
    }
    if(!Send4GCmd("AT+COPS?","+COPS: 0",AT_RETRY_TIMES,AT_WAIT_TIME)){ // 查询网络注册状态
        ESP_LOGE(TAG, "check cops failed");
        if(!Send4GCmd("AT+COPS=0","OK",AT_RETRY_TIMES,AT_WAIT_TIME)){
            ESP_LOGE(TAG, "cops set 0 failed");
            return ESP_FAIL;
        }
    }
    if(!Send4GCmd("AT+GTGPSPOWER=1", "OK", AT_RETRY_TIMES, AT_WAIT_TIME)){
        ESP_LOGE(TAG, "gps power on failed");
    }
    ack = Send4GCmd("AT+GTGPS?", "+GTGPS:", AT_RETRY_TIMES, AT_WAIT_TIME);
    if(ack || parse_gps_info(ack) != ESP_OK){
        ESP_LOGW(TAG, "did not get gps info, try again after a few minutes");
    }

    return ESP_OK;
}
/**
 * @brief 订阅MQTT主题
 * 
 * 该函数用于订阅MQTT主题。根据传入的mqtt_about结构体中的设备类型和设备序列号，
 * 构建订阅和发布的主题，并发送AT指令进行订阅操作。如果订阅成功，则返回ESP_OK，
 * 否则返回ESP_FAIL。
 共计订阅2个主题：公共，和sub
 * @param mqtt_about MQTT相关信息结构体
 * @return bool 订阅结果，成功返回ESP_OK，失败返回ESP_FAIL
 */
int mqtt_sub_topic(login_info_t *mqtt_about) 
{

    // TODO 绑定设备
    uint8_t temp_array[30];
    bool ret = ESP_FAIL;
    snprintf(mqtt_about->sub_topic, sizeof(mqtt_about->sub_topic), SUBSCRIBE_TOPIC, 
                                            mqtt_about->dev_type, mqtt_about->dev_sn);  
    snprintf(mqtt_about->pub_topic, sizeof(mqtt_about->pub_topic), PUBLISH_TOPIC,   
                                            mqtt_about->dev_type, mqtt_about->dev_sn);
    snprintf((char *)temp_array, 30, ALL_PUBLIC_TOPIC,	mqtt_about->dev_type);//PUBLIC_TOPIC
    snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MQTTSUB=1,\"%s\",0", temp_array);//PUBLIC_TOPIC
//	snprintf(all_public_topic, sizeof(all_public_topic), ALL_PUBLIC_TOPIC,	login_info->dev_type);
	
    if (!Send4GCmd(ATSendBuff, "+MQTTSUB: 1,1", AT_RETRY_TIMES, 10000)) //fail
	{ // 订阅公共主题
        ESP_LOGE(TAG, "sub public topic failed"); 
    } 
	else //success
	{
        ESP_LOGI(__func__, "Subscribe success, msg_id=1 topic: %s", temp_array);//PUBLIC_TOPIC ALL_PUBLIC_TOPIC
        snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MQTTSUB=1,\"%s\",0", mqtt_about->sub_topic);
        if (!Send4GCmd(ATSendBuff, "+MQTTSUB: 1,1", AT_RETRY_TIMES, 10000)) { // 订阅接收主题
            ESP_LOGE(TAG, "sub recv topic failed"); 
        } 
		else //ok
		{
            ESP_LOGI(__func__, "Subscribe success, msg_id=1 topic: %s", mqtt_about->sub_topic); 
            ret = ESP_OK;
        }
    }
    return ret;
}

/**
 * @brief 获取GPS信息
 * 
 * 从响应字符串中提取GPS信息，并打印提取的数据。
 * 
 * @param rsp_ptr 响应字符串指针
 * @return int 返回执行结果，成功返回ESP_OK，失败返回ESP_FAIL
 */
static int parse_gps_info(char *rsp_ptr){
    char *response = NULL;
    int result = ESP_FAIL;

    if(rsp_ptr) {
        ESP_LOGI(TAG, "get gps info");
        gnss_info_t gnss_info = {0};
        sscanf(response, "%*[^$GPGGA]$GPGGA,%lf,%lf,%c,%lf,%c,%d,%d,%lf,%lf,%c,%lf,%c",
        &gnss_info.time, &gnss_info.latitude, &gnss_info.ns, &gnss_info.longitude, 
        &gnss_info.ew, &gnss_info.fix_quality, &gnss_info.satellites_tracked, 
        &gnss_info.horizontal_dilution, &gnss_info.altitude, &gnss_info.altitude_units, 
        &gnss_info.geoidal_separation, &gnss_info.geoidal_separation_units);
        // 打印提取的数据
        ESP_LOGI(TAG, "Time: %lf", gnss_info.time);
        ESP_LOGI(TAG, "Latitude: %lf %c", gnss_info.latitude, gnss_info.ns);
        ESP_LOGI(TAG, "Longitude: %lf %c", gnss_info.longitude, gnss_info.ew);
        ESP_LOGI(TAG, "Fix Quality: %d", gnss_info.fix_quality);
        ESP_LOGI(TAG, "Satellites Tracked: %d", gnss_info.satellites_tracked);
        ESP_LOGI(TAG, "Horizontal Dilution: %lf", gnss_info.horizontal_dilution);
        ESP_LOGI(TAG, "Altitude: %lf %c", gnss_info.altitude, gnss_info.altitude_units);
        ESP_LOGI(TAG, "Geoidal Separation: %lf %c", gnss_info.geoidal_separation, gnss_info.geoidal_separation_units);
        if(gnss_info.fix_quality == 0){
            ESP_LOGE(TAG, "GNSS unreachable, try again after a few minutes");
        } else {
            ESP_LOGI(TAG, "GNSS get successfully");
            iot_4g_data.gnss_info = gnss_info;
            iot_4g_state.bit.GpsFail = 0;
            result = ESP_OK;
        }
    } else {
        ESP_LOGE(TAG, "GPGGA sentence not found.");
    }

    return result;
}

int mqtt_client_close(void){
    if(!Send4GCmd("AT+MQTTCLOSE=1","+MQTTCLOSE: 1,1", 1, AT_WAIT_TIME)){ // 先关闭之前的连接
        ESP_LOGI(TAG, "mqtt close yet"); 
    } else {
        ESP_LOGI(TAG, "mqtt close success"); 
    }
    return ESP_OK;
}

int get_local_ip(void) {
    char *response;
    char apn[51] = {0};
    int result = ESP_FAIL; // 默认设置为失败状态
    int ip[4];
    // 获取APN, eg:+CGDCONT: 1,"IP","UNINET","10.114.92.39",0,0,0,0
    response = Send4GCmd("AT+CGDCONT?", "+CGDCONT", AT_RETRY_TIMES, AT_WAIT_TIME); 
    if (response && (sscanf(response, "+CGDCONT: %*d,\"%*[^\"]\",\"%50[^\"]\"", apn) == 1)) {
        // 成功获取APN，继续执行
        if(Send4GCmd("AT+MIPCALL?", "+MIPCALL: 1", 1, AT_WAIT_TIME)){
            // 已经获取到IP，跳过
            ESP_LOGI(TAG,"got ipv4 addr yet, skip step");
            iot_4g_state.bit.netdial_fail = 0;
            result = ESP_OK; // 成功获取IP
        } else {
            snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MIPCALL=1,\"%s\"", apn); 
            response = Send4GCmd(ATSendBuff, "+MIPCALL:", AT_RETRY_TIMES, 30000);
            if (sscanf(response,"+MIPCALL: %d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) == 4) {
                ESP_LOGI(TAG, "git ipv4 address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                iot_4g_state.bit.netdial_fail = 0;
                result = ESP_OK; // 成功获取IP
            } else {
                iot_4g_state.bit.netdial_fail = 1;
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP_LOGE(TAG, "get IP failed");
            }
        }
    } else {
        iot_4g_state.bit.netdial_fail = 1;
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGE(TAG, "get APN failed");
    }

    return result; // 返回执行结果
}

int mqtt_client_dns(login_info_t *mqtt_about)  {
    int ip[4];
    int result = ESP_FAIL;
    if (strlen(mqtt_about->raw_url) == 0) {
        ESP_LOGE(__func__,"url is null can not analyze");
        return result;
    } 

    sscanf(mqtt_about->raw_url, "%120[^:] %*[:]%hu",  mqtt_about->host, &mqtt_about->port); // 将输入的HOST地址解析出地址和端口
    ESP_LOGI(TAG,"host: %s, port: %hu", mqtt_about->host,  mqtt_about->port);    

    snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MIPDNS=\"%s\"",mqtt_about->host); 
    char *response = Send4GCmd(ATSendBuff,"+MIPDNS", AT_RETRY_TIMES, AT_WAIT_TIME); // DNS查询IP
    if(response){
        if (sscanf(response, "+MIPDNS: \"%*[^\"]\",%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) == 4) {
            // 输出提取的IP地址
            mqtt_about->ipaddr[0] = ip[0];
            mqtt_about->ipaddr[1] = ip[1];
            mqtt_about->ipaddr[2] = ip[2];
            mqtt_about->ipaddr[3] = ip[3];
            ESP_LOGI(TAG, "HOST IP Address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            result = ESP_OK;
        } else {
            // 解析失败
            ESP_LOGE(TAG, "Failed to parse IP address");
        }
    } 
    return result;
}

int mqtt_client_create_tcp(login_info_t *mqtt_about)
{
    int result = ESP_FAIL;
    if(Send4GCmd("AT+MIPSEND?","+MIPSEND: 0", 1, AT_WAIT_TIME)){ // 没有正在使用的socket 28392
        snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MIPOPEN=1,28392,\"%d.%d.%d.%d\",%d,0",mqtt_about->ipaddr[0], 
            mqtt_about->ipaddr[1], mqtt_about->ipaddr[2], mqtt_about->ipaddr[3], mqtt_about->port); //  建立TCP连接
        if(!Send4GCmd(ATSendBuff,"+MIPOPEN: 1,1",AT_RETRY_TIMES, 60000)){ // 需要等待60s，失败重新解析DNS
            ESP_LOGE(TAG, "TCP connect failed");
        } 
		else 
		{
            result = ESP_OK;
        }
    } 
	else 
	{ // 已经打开socket
        ESP_LOGI(TAG,"mipopen has done yet, skip");
        result = ESP_OK;
    }

    return result;
}

int mqtt_client_sync_time(login_info_t *mqtt_about) 
{
    char cmd_sync_time[128] = {0};
    char iot_dev_ascii[60] = {0};
    host_time_t  nowtime;
    uint32_t time = 0;
    char *response = NULL;
    uint16_t cmd_len = 0;
    snprintf(iot_dev_ascii, sizeof(iot_dev_ascii), "%s%llu", mqtt_about->iot_type, mqtt_about->iot_sn);
    cmd_sync_time[cmd_len++] = 0x00;
    cmd_sync_time[cmd_len++] = 0x01;
    cmd_sync_time[cmd_len++] = (strlen(iot_dev_ascii) >> 8);  // 负载长度高8bit
    cmd_sync_time[cmd_len++] = (strlen(iot_dev_ascii));       // 负载长度低8bit
    memcpy(&cmd_sync_time[4], iot_dev_ascii, strlen(iot_dev_ascii));
    cmd_len += strlen(iot_dev_ascii);

    if(!Send4GCmd("AT+GTSET=\"IPRFMT\",0","OK",AT_RETRY_TIMES,AT_WAIT_TIME)){ // 设置数据接收格式为十六进制,0接收数据为+MIPRTCP:
        ESP_LOGE(TAG, "gtset failed");
    }

    snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MIPSEND=1"); // 不定长度发送
    if(!Send4GCmd(ATSendBuff,">",AT_RETRY_TIMES,AT_WAIT_TIME)){ // 发送到缓存
        ESP_LOGE(TAG, "TCP send failed");
        return 0;
    }
    SendATCmd((uint8_t *)cmd_sync_time, cmd_len);
    ESP_LOG_BUFFER_HEX(TAG, (uint8_t *)cmd_sync_time, cmd_len);

    response = Send4GCmd("\x1A","+MIPRTCP:",AT_RETRY_TIMES,AT_WAIT_TIME); // 接收最大长度1000
    if(response){ 
        ESP_LOGI(TAG, "get response");
        char rx_buffer[101] = {0};
        uint8_t sync_time_ack[50] = {0};
        sscanf(response,"+MIPRTCP: 1,%*d,%100s", rx_buffer); // 最大读取100个字符
        uint16_t rx_len = strlen(rx_buffer);
        if(rx_len == 0 ){
            ESP_LOGE(TAG,"+MIPRTCP no data");
            return ESP_FAIL;
        }
        uint16_t sync_len = str2hex(rx_buffer, sync_time_ack, rx_len);

        ESP_LOG_BUFFER_HEX(TAG, sync_time_ack, sync_len);
        nowtime.uword = 0;
        if ((0x00 == sync_time_ack[0]) 
			&&(0x01 == sync_time_ack[1]) ) 
		{
            int len = (((uint16_t)sync_time_ack[2]<<8) | sync_time_ack[3]);
            if (len == 4) {
                memcpy(nowtime.byte, sync_time_ack + 4, len);
                time = htonl(nowtime.uword); /* 返回获取到的主机时间 */
                ESP_LOGI(TAG, "host now time: %"PRId32"", time);
            }
        }
    }
    return time;
}

uint32_t windydebug11=0;

//uint8_t windy_debug_bind(void);

/**
* @brief :mqtt_client_start
* @param[in] :	   
* @param[out] : 
* @return :	 
ESP_OK-完成 订阅主题流程

*/
int mqtt_client_start(login_info_t *mqtt_about) 
{
    int ret = ESP_OK;
//#if 0	
    char mqtt_host[150] = {0};
    char password[128]  = {0};
    char username[128]  = {0};

    snprintf(mqtt_host, sizeof(mqtt_host), "%s", mqtt_about->host);
    snprintf(username, sizeof(username), "%s%llu", mqtt_about->iot_type, mqtt_about->iot_sn); 
    uint64_t pwd = CreateEncryptPassword(username, mqtt_about->safetyCode, mqtt_about->now_time); /* 计算MQTT登录密码 */
    snprintf(password, sizeof(password), "%08llu", pwd);

if(2 == windydebug11)
{
	ret = ESP_OK;
}


//    esp_mqtt_client_config_t 
        mqtt_cfg.broker.address.uri = mqtt_host;//mqtt_uri,                  /*!< MQTT host */
        mqtt_cfg.broker.address.port = mqtt_about->port;
#if !MQTT_DEBUG
        mqtt_cfg.credentials.username = username;                /*!< MQTT username */        
        mqtt_cfg.credentials.authentication.password = password; /*!< MQTT password */
#endif
        mqtt_cfg.credentials.client_id = username;//clientId,               /*!< MQTT clientId */
        mqtt_cfg.network.disable_auto_reconnect = false;         /*!< MQTT auto reconnect  */
        mqtt_cfg.session.keepalive = 50;//60,                         /*!< MQTT keep alive */
//#endif
    //ESP_LOGI(TAG, "********mqtt login info*********");
    ESP_LOGI(__func__, "hosturi: mqtt://%s",  mqtt_cfg.broker.address.uri);
    ESP_LOGI(__func__, "hostport: %"PRId32"",  mqtt_cfg.broker.address.port);
    ESP_LOGI(__func__, "ClientId: %s",  mqtt_cfg.credentials.client_id);
    ESP_LOGI(__func__, "username: %s",  mqtt_cfg.credentials.username);   
    ESP_LOGI(__func__, "password: %s",  mqtt_cfg.credentials.authentication.password); 



    vTaskDelay(pdMS_TO_TICKS(500));
    snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MQTTUSER=1,\"%s\",\"%s\",\"%s\"", mqtt_cfg.credentials.username, mqtt_cfg.credentials.authentication.password, mqtt_cfg.credentials.client_id); 
    if(!Send4GCmd("AT+MQTTOPEN?","OK",AT_RETRY_TIMES,AT_WAIT_TIME)){ // 查询连接状态
        ESP_LOGE(TAG, "query avaliable failed"); 
        ret = ESP_FAIL;
        iot_4g_state.bit.mqtt_login_fail = 1;
    } 
	else if(!Send4GCmd(ATSendBuff,"OK",AT_RETRY_TIMES,AT_WAIT_TIME))
	{ // 设置账号和密码
        ESP_LOGE(TAG, "mqtt login failed"); 
        ret = ESP_FAIL;
		
    } 

	else 
	{
	
	
	  memset(ATSendBuff, 0, sizeof(ATSendBuff));
	  
	  vTaskDelay(pdMS_TO_TICKS(8000));//windy debug
        snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MQTTOPEN=1,\"%s\",%"PRId32",1,%"PRId16"", mqtt_cfg.broker.address.uri, mqtt_cfg.broker.address.port, mqtt_cfg.session.keepalive); //  information of client will be cleaned when the connection is closed
        if(!Send4GCmd(ATSendBuff,"+MQTTOPEN: 1,1",AT_RETRY_TIMES,AT_WAIT_TIME)){ // 连接MQTT
            ESP_LOGE(TAG, "mqtt open failed");
            ret = ESP_FAIL;
        } 
		else 
		{
		
			vTaskDelay(pdMS_TO_TICKS(8000));//windy debug
			if(0 == windydebug11)//绑定，windy add
			{//0!= windy_debug_bind()
//				windy_debug_bind();
				windydebug11 =2;
				 ret = ESP_OK;
				 vTaskDelay(pdMS_TO_TICKS(8000));//windy debug
				 
			}

		
            vTaskDelay(pdMS_TO_TICKS(500));
            ret = mqtt_sub_topic(mqtt_about); // 订阅主题
        }
    }

    if(ret == ESP_FAIL)
	{
        ESP_LOGE(TAG, "mqtt client start failed"); 
    } 
	else
	{
        iot_4g_state.bit.mqtt_login_fail = 0;
        ESP_LOGI(TAG, "mqtt client start success"); 
    }
    return ret;
}

/**
 * 通过MQTT发布消息
 *
 * 此函数将接收到的数据转换为十六进制字符串格式，
 * 并使用AT命令"AT+MQTTPUB"通过MQTT协议发布。
 AT+MQTTPUB=1,"test1主题",0,0,"123消息" //发布消息到对应主题，

 * @param send_topic 发布的主题。
 * @param data_ptr 指向要发送的数据的指针。
 * @param data_len 要发送的数据的长度。
 
 *@return 
 (-1) tx fail
 OTHERS (>=0) The number of bytes pushed to the TX FIFO
 */
int16_t AT_MQTT_Send_Public_Data(char *send_topic, uint8_t *data_ptr, uint16_t data_len)
{
	int16_t ret=-1;

    // 定义AT命令的前缀和后缀
    char at_cmd_prefix[100];
    sprintf(at_cmd_prefix, "AT+MQTTPUB=1,\"%s\",0,0,\"", send_topic);
    uint16_t prefix_len = strlen(at_cmd_prefix);

    // 检查数据长度是否超过缓冲区大小
    if ((data_len * 2 + prefix_len + 2) <= AT_SEND_BUF_SIZE) 
	{
		// 将AT命令的前缀复制到发送缓冲区
		memcpy(ATSendBuff, at_cmd_prefix, prefix_len);
		
		// 在发送缓冲区中生成十六进制字符串
		char *hex_str_ptr = ATSendBuff + prefix_len;
		hex2str(data_ptr, hex_str_ptr, data_len);
		
		// 添加AT命令的后缀
		strcat(ATSendBuff, "\"");
		
		// 发送AT命令
		ret =SendATCmd((uint8_t *)ATSendBuff, strlen(ATSendBuff));
    }
	else
	{
        ESP_LOGE(TAG, "Error: Data is too large to mqtt publish");
	}

	return ret;
}


#define  MAX_TX_LEN_VALID_DATA_4G_MODULE (AT_SEND_BUF_SIZE-100)//单次最长发送字节数

/*------------------------------------------------------------------------
*@Function： AT_TCP_Send_Data
* @brief 通过4G模块发送TCP数据。
* 此函数将接收到的数据转换为十六进制字符串格式，
* 并使用AT命令"AT+MIPSEND"通过TCP连接发送。
AT+MIPSEND=  //TCP发送，有多种格式，分阶段，或连续一次发出,如下使用拼接后单次发出


-------------------------------------------------------------------------*/
/**
*@brief  
* @param Txdata_ptr 指向要发送的数据的指针。
* @param Txdata_len 要发送的数据的长度。
* @param Rxdata_ptr 指向要接收的数据的指针，NULL标识不判断接收内容
* @param Rxdata_len 要接收的数据的长度。

*@return  Rx Valid data len,     
<0: send fail
>=0:实际接收的有效内容长度（不含指令部分）
*/ 
//int16_t AT_TCP_Send_Data(uint8_t *Txdata_ptr, uint16_t Txdata_len,uint8_t *Rxdata_ptr)
//{
//	int16_t Rxdata_len = -1;
//    char *response = NULL;
//	unsigned int Len_rxbuffer=0;//int16_t  int
//    char at_cmd[20] = "AT+MIPSEND=1,\"";
//	uint16_t cmd_len = strlen(at_cmd);
//
//	int16_t Txdata_len_1Frame=0;//TCP发送拆分的单次有效内容长度
//	int16_t Txdata_len_total=0;//TCP发送拆分的总剩余有效内容长度
//
//
///* set IPRFMT：设置数据接收格式为十六进制(字符串),接收数据为+MIPRTCP:
//0-rx内容为字符串，带+MIPRTCP:标识头
//1-rx内容为原始数据，不带+MIPRTCP:标识头
//2-rx内容为原始数据，带+MIPRTCP:标识头
//*/
//    if(!Send4GCmd("AT+GTSET=\"IPRFMT\",0","OK",AT_RETRY_TIMES,AT_WAIT_TIME))
//	{ 
//        ESP_LOGE(TAG, "gtset failed");
//    }	
//
//	Txdata_len_total = Txdata_len;
//	while (Txdata_len_total > 0)
//	{
//		if(Txdata_len_total > MAX_TX_LEN_VALID_DATA_4G_MODULE)
//		{
//			Txdata_len_1Frame =MAX_TX_LEN_VALID_DATA_4G_MODULE;
//		}
//		else
//		{
//			Txdata_len_1Frame =Txdata_len_total;
//		}	
//		Txdata_len_total -= Txdata_len_1Frame;
//
//		if ((Txdata_len_1Frame * 2 + cmd_len + 2) > AT_SEND_BUF_SIZE) //长度超限制判断
//		{
//			ESP_LOGE(TAG, "Error: Data is too large to tcp send");
//			Rxdata_len =-1;
//			break;
//		}
//
//		memcpy(ATSendBuff, at_cmd, cmd_len);
//		char *char_ptr = ATSendBuff + cmd_len;
//
//	   ESP_LOGI(TAG, "TCP Txdata_ptr: %s", (char *)Txdata_ptr);
//		
//		hex2str(Txdata_ptr, char_ptr, Txdata_len_1Frame);
//		strcat(ATSendBuff, "\"");
//	
//		response = Send4GCmd(ATSendBuff, "+MIPRTCP:", AT_RETRY_TIMES, AT_WAIT_TIME);
//		if(response)//rx valid
//		{ 
//			ESP_LOGI(TAG, "get response");
//			uint8_t sync_time_ack[50] = {0};
//			//sscanf(response,"+MIPRTCP: 1,%*d,%100s", rx_buffer); //格式化转换， 最大读取100个字符
//			sscanf(response,"+MIPRTCP: 1,%u,%s",&Len_rxbuffer, Rxdata_ptr); //格式化转换， 最大读取100个字符
//			if(NULL == Rxdata_ptr)
//			{
////				sscanf(response,"+MIPRTCP: 1,%u,",&Len_rxbuffer); //格式化转换， 最大读取100个字符
//			}
//			else
//			{
//				
//				ESP_LOGI(TAG, "rx_buffer =%d,%d,%d,%d,%d",Rxdata_ptr[0],Rxdata_ptr[1],Rxdata_ptr[2],Rxdata_ptr[3],Rxdata_ptr[4]);
//			}
//			
//			//Len_rxbuffer = strlen(rx_buffer);
//			if(Len_rxbuffer == 0 )
//			{
//				ESP_LOGE(TAG,"+MIPRTCP no data");
//			}
//	//		  uint16_t sync_len = str2hex(rx_buffer, sync_time_ack, Len_rxbuffer);
//	//		  ESP_LOG_BUFFER_HEX(TAG, sync_time_ack, sync_len);
//			Rxdata_len =Len_rxbuffer;
//	
//		}		
//		else
//		{
//			Rxdata_len =-1;
//			ESP_LOGE(TAG, "Send4GCmd failed");
//		}
//	}
//
//
//	return Rxdata_len;
//}

//windy debug
int16_t AT_TCP_Send_Data(uint8_t *Txdata_ptr, uint16_t Txdata_len,uint8_t *Rxdata_ptr)
{
	int16_t Rxdata_len = -1;

    char cmd_sync_time[128] = {0};
    char iot_dev_ascii[60] = {0};
    host_time_t  nowtime;
    char *response = NULL;
    uint16_t cmd_len = 0;

    int result = ESP_FAIL;
    if(Send4GCmd("AT+MIPSEND?","+MIPSEND: 0", AT_RETRY_TIMES, AT_WAIT_TIME))//之前未打开
	{ // 没有正在使用的socket


    } 
	else //+MIPSEND: 1,2048
	{ // 已经打开socket
        ESP_LOGI(TAG,"mipopen has done yet, skip");
        if(!Send4GCmd("AT+MIPCLOSE=1","+MIPCLOSE: 1,0",AT_RETRY_TIMES, 60000))
		{ // 需要等待60s，失败重新解析DNS
            ESP_LOGE(TAG, "TCP CLOSE failed");
			return 0;
			
        } 

    }

	snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MIPOPEN=1,28392,\"%d.%d.%d.%d\",%d,0",login_info.ipaddr[0], 
		login_info.ipaddr[1], login_info.ipaddr[2], login_info.ipaddr[3], login_info.port); //	建立TCP连接 28392
	if(!Send4GCmd(ATSendBuff,"+MIPOPEN: 1,1",AT_RETRY_TIMES, 60000))
	{ // 需要等待60s，失败重新解析DNS
		ESP_LOGE(TAG, "TCP connect failed");
		return 0;
		
	} 


	{
		snprintf(ATSendBuff, AT_SEND_BUF_SIZE, "AT+MIPSEND=1"); // 不定长度发送
		if(!Send4GCmd(ATSendBuff,">",AT_RETRY_TIMES,AT_WAIT_TIME)){ // 发送到缓存
			ESP_LOGE(TAG, "TCP send failed");
			return 0;
		}
		ESP_LOGW(TAG, "1 AT_TCP_Send_Data: %s ",Txdata_ptr);
		ESP_LOGW(TAG, "1 AT_TCP_Send_Data[4]: %s ",&Txdata_ptr[4]);
		
		SendATCmd((uint8_t *)Txdata_ptr, Txdata_len);
		ESP_LOG_BUFFER_HEX(TAG, (uint8_t *)Txdata_ptr, cmd_len);
		
		response = Send4GCmd("\x1A","+MIPRTCP:",AT_RETRY_TIMES,AT_WAIT_TIME); // 接收最大长度1000
		if(response){ 
			ESP_LOGI(TAG, "get response");
			char rx_buffer[101] = {0};
			uint8_t sync_time_ack[50] = {0};
			sscanf(response,"+MIPRTCP: 1,%*d,%100s", rx_buffer); // 最大读取100个字符
			uint16_t rx_len = strlen(rx_buffer);
			if(rx_len == 0 ){
				ESP_LOGE(TAG,"+MIPRTCP no data");
				return ESP_FAIL;
			}
			uint16_t sync_len = str2hex(rx_buffer, sync_time_ack, rx_len);
		
			ESP_LOG_BUFFER_HEX(TAG, sync_time_ack, sync_len);
		
		}
	}


//    if(!Send4GCmd("AT+GTSET=\"IPRFMT\",0","OK",AT_RETRY_TIMES,AT_WAIT_TIME)){ // 设置数据接收格式为十六进制,0接收数据为+MIPRTCP:
//        ESP_LOGE(TAG, "gtset failed");
//    }


	return Rxdata_len;
}

/**
 * @brief 。
 *
 * 此函数向4G模块发送指定的AT命令并等待确认。
 * 如果失败，它允许多次尝试接收确认。
 *
 * @param ATcmd 要发送的AT命令。
 * @param Ack 预期的确认。
 * @param Counts 尝试接收确认的次数。
 * @param WaitTime 等待确认的最大时间。
 * @return 如果找到，则返回指向确认字符串的指针，否则返回NULL。
 */
 char *At_4G_Get_In_PPP_Mode(void)
{
	char *ret = NULL;

	//ret =Send4GCmd("ATDT*99#","CONNECT", AT_RETRY_TIMES, AT_WAIT_TIME);//进入PPP-》NO CARRIER;CONNECT 150000000
	ret =Send4GCmd("ATD*99#","CONNECT", AT_RETRY_TIMES, AT_WAIT_TIME);//进入PPP-》NO CARRIER;CONNECT 150000000 ESP demo
	
    if(ret)
	{ // 没有正在使用的socket


    } 
	
    return ret; // 
}
 /**
  * @brief 发送4G命令并等待确认。
  *
  * 此函数向4G模块发送指定的AT命令并等待确认。
  * 如果失败，它允许多次尝试接收确认。
  *
  * @param ATcmd 要发送的AT命令。
  * @param Ack 预期的确认。
  * @param Counts 尝试接收确认的次数。
  * @param WaitTime 等待确认的最大时间。
  * @return 如果找到，则返回指向确认字符串的指针，否则返回NULL。
  */
  char *At_4G_Get_Out_PPP_Mode(void)
 {
	 char *ret = NULL;
 
	 ret =Send4GCmd("ATZ","ATZ", AT_RETRY_TIMES, AT_WAIT_TIME);//退出PPP,->OK
	 if(ret)
	 { // 没有正在使用的socket
 
 
	 } 
	 
	 return ret; // 
 }




// 通过AT指令查询rssi
void at_query_rssi(void)
{
    SendATCmd((uint8_t *)&"AT+CSQ?", strlen("AT+CSQ?"));
}

// 通过AT指令获取GPS信息
void at_query_gps(void){
    if(iot_4g_state.bit.GpsFail == 1){
        SendATCmd((uint8_t *)&"AT+GTGPS?", strlen("AT+GTGPS?"));
    }
}

/*------------------------------------------------------------------------
*@Function： mqtt_client_data_handle
查询uart rx来自4G模块的解析报文
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     None
*@param[out]    None
*@return 
(-1) Parameter error
OTHERS (>=0) The number of bytes pushed to the TX FIFO

*/
int mqtt_client_data_handle(login_info_t *mqtt_about)
{
    int ret = ESP_OK; // 断开连接后返回ESP_FAIL

//    memset(reals.AT_Cmd_RxPointer, 0, sizeof(reals.struct_uart2.Rxbuffer));
    uint16_t RecvAckLen = RecvATAck( AT_RECV_BUF_SIZE, 2000); // 不可阻塞
    if(RecvAckLen)
	{
        if(strstr((char *)reals.AT_Cmd_RxPointer, "$GPGGA"))
		{ // GPS 数据
            parse_gps_info((char *)reals.AT_Cmd_RxPointer);
        } 
		else if(strstr((char *)reals.AT_Cmd_RxPointer, "+MQTTBREAK: 1,1") || strstr((char *)&reals.AT_Cmd_RxPointer, "+MQTTCLOSE: 1,1"))
		{ // MQTT断开退出循环、重新连接MQTT
            iot_4g_state.bit.mqtt_login_fail = 1;
            ret = ESP_FAIL;
        } 
		else if(strstr((char *)reals.AT_Cmd_RxPointer, "+MIPRTCP:"))
		{ // TCP 接收数据
            ESP_LOGI(TAG, "TCP recv data:");
            ESP_LOG_BUFFER_HEX(TAG, (uint8_t *)reals.AT_Cmd_RxPointer, RecvAckLen);
        } 
		else if(strstr((char *)reals.AT_Cmd_RxPointer, ">"))
		{ // > 发送数据
            // 
        } 
		else if(strstr((char *)reals.AT_Cmd_RxPointer, "+CSQ:"))
       {
            int rssi = 0;
            sscanf((char*)reals.AT_Cmd_RxPointer, "+CSQ:%d", &rssi);
            iot_4g_state.rssi = rssi;
        } 
		else if(strstr((char *)reals.AT_Cmd_RxPointer, "+MQTTMSG:"))
		{ // MQTT数据 eg：+MQTTMSG: 1, 0, “test1”, “123”
            // 解析MQTT数据
            parse_mqtt_msg((char *)reals.AT_Cmd_RxPointer, RecvAckLen, mqtt_about->pub_topic);
        }
    }
    return ret;
}


/**
 * @brief 解析MQTT消息。
 *
 * 此函数从接收到的MQTT消息中提取主题和有效载荷，并将有效载荷转换为十六进制数据。
 *
 * @param msg_ptr 指向MQTT消息的指针。
 * @param msg_len MQTT消息的长度。
 */
void parse_mqtt_msg(char *msg_ptr, uint16_t msg_len, char *send_topic) {

    if(msg_ptr==NULL)
    {
        return;
    }
    uint8_t topic[50] = {0};
    char *payload = (char *)iot_malloc((msg_len + 1) * sizeof(char));
    if (payload != NULL) {
        memset(payload, 0, (msg_len + 1) * sizeof(char));
    }else{
        ESP_LOGW(TAG,"payload ptr NULL");
        return;
    }
    uint8_t *hex_data = (uint8_t *)iot_malloc(msg_len/2 * sizeof(uint8_t));
    if (hex_data != NULL) {
        memset(hex_data, 0, msg_len/2 * sizeof(uint8_t));
    }

    int rsp_len = 0;
    uint16_t plen = 0;
    uint8_t *pdata = NULL;
    if (sscanf(msg_ptr, "+MQTTMSG: %*d, %*d, \"%49[^\"]\", \"%[^\"]\"", topic, payload) == 2) {
        uint16_t hex_len = str2hex(payload, hex_data, strlen(payload));
        if ((hex_len > 2) && (hex_data[0] == 0x01) && (hex_data[1] == 0x01)) { // 载荷V1.0 
            pdata = &hex_data[1]; 
            plen = hex_len - 1;
        } else if ((hex_len > 10) && (hex_data[0] == 0x01) && (hex_data[1] == 0xF8)) {  // 载荷V1.2
            pdata = &hex_data[10];
            plen = hex_len - 10;
        } else if ((hex_len > 5) && (hex_data[0] == 0x00) && (hex_data[1] == 0x09)) {  // https 升级命令
            plen = (hex_data[2] << 8) | hex_data[3];
            // https_ota_cmd_parse((char *)&mqtt_msg.pdata[4], plen); // https 升级命令解析
            pdata = NULL; 
            plen = 0;
        } else {
            pdata = NULL;
            plen = 0;
        }
        
        if (pdata) { // modbus 命令
            uint8_t *response = heap_caps_malloc(600, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (response) {
                rsp_len += mqtt_data_head(0x01, response, hex_data[2], 0, 0, 0);
//windy tbd                rsp_len += Modbus_Slave(pdata, plen, (response + rsp_len));
                if (rsp_len > 10) {
                    AT_MQTT_Send_Public_Data(send_topic, response, rsp_len);
                }
                free(response);
            }
        }
    } else {
        ESP_LOGE(TAG, "mqtt recv failed");
    }
    if(payload != NULL){
        free(payload);
    }
    if(hex_data != NULL){
        free(hex_data);
    }
}


int mqtt_data_head(uint8_t ver, uint8_t *head, uint8_t cause, uint16_t cycel, uint8_t total, uint8_t seq) {
    /*
    * MQTT数据添加数据头 10字节 V1.2版本头
    * 
    * */
   iot_4g_state_t state = get_iot_4g_state();

    head[0] = ver;  // 0x01 modbus格式，0x02字符串格式
    head[1] = 0xF8;
    head[2] = cause;
    head[3] = cycel ;
    head[4] = (cycel >> 8) & 0xFF;
    head[5] = seq;
    head[6] = total;
    head[7] = state.rssi; //  信号强度rssi
    head[8] = 0; // 保留
    head[9] = 0; // 保留
    return MQTT_FORMAT_HEAD;
}
