#ifndef __IOT_4G_AT__
#define __IOT_4G_AT__

#include <stdint.h>
#include "iot_mqtt.h"

#define MQTT_FORMAT_HEAD        10

extern char ATSendBuff[];



#pragma pack (1)
typedef struct{
    uint8_t byte;
    union{
        uint8_t sim_missing: 1;      //  表示SIM卡是否未插入（bit0）
        uint8_t sim_inactive:1;      //  表示SIM卡是否未激活（bit1）
        uint8_t netdial_fail:1;      //  表示互联网拨号是否失败（bit2）
        uint8_t mqtt_login_fail: 1;      //  表示服务器连接是否失败（bit3）
        uint8_t GpsFail:    1;      //  表示GPS定位是否失败（bit4）
        uint8_t RsvBit5:    1;      //  保留位（bit5）
        uint8_t ComSwitch:  2;      //  表示4G通信开关状态（bit6/7）
    }bit;
    uint8_t rssi;           //  表示4G无线信号强度（bit15~8）
}iot_4g_state_t;

typedef struct {
    double time;                    // UTC 时间，格式为 hhmmss.sss，范围：000000.000 - 235959.999
    double latitude;                // 纬度，格式为 ddmm.mmmm，范围：0000.0000 - 9000.0000
    char ns;                        // 纬度半球，'N' 表示北半球，'S' 表示南半球
    double longitude;               // 经度，格式为 dddmm.mmmm，范围：00000.0000 - 18000.0000
    char ew;                        // 经度半球，'E' 表示东半球，'W' 表示西半球
    int fix_quality;                // 定位质量指示，0 = 无效，1 = GPS 定位，2 = 差分 GPS 定位，等等
    int satellites_tracked;         // 正在使用的卫星数量，范围：0 - 12
    double horizontal_dilution;     // 水平精度衰减因子，范围：0.5 - 99.9，较小的值表示更好的精度
    double altitude;                // 海拔高度，单位为米，范围：-9999.9 - 99999.9
    char altitude_units;            // 海拔高度单位，'M' 表示米
    double geoidal_separation;      // 大地水准面高度（地球椭球面与平均海平面之间的距离），单位为米
    char geoidal_separation_units;  // 大地水准面高度单位，'M' 表示米
} gnss_info_t; // 只取GPGGA字段信息

typedef struct{
    gnss_info_t gnss_info; // gnss信息
}iot_4g_data_t;
//
//typedef struct {
//    char raw_url[128]; // 原始后台地址
//    char host[128]; // 解析后的后台地址
//    char recv_topic[50]; // 数据接收主题
//    char send_topic[50]; // 数据发布主题
//    int socketfd; // tcp 连接socket
//    uint64_t nowTime; // 服务器的当前 时间
//    uint16_t host_port;
//    uint8_t host_ipv4[4];
//    //uint8_t client_start;
//
//    // 下有设备
//    char dev_type[12]; // 逆变设备型号
//    uint64_t dev_sn; // 逆变设备SN码
//
//    // iot设备
//    char iot_type[12];
//    uint64_t iot_sn;
//    uint64_t iot_safety;
//    
//}mqtt_about_t;

//typedef union {
//    uint8_t byte[4];
//    uint32_t uword;
//}host_time_t;

typedef struct {
    uint8_t *data_ptr;
    uint16_t len; 
}cmd_4g_t;

#pragma pack ()

int Init4GModule(void);
int get_local_ip(void);
int mqtt_client_close(void);
int mqtt_client_dns(login_info_t *mqtt_about);
int mqtt_client_create_tcp(login_info_t *mqtt_about);
int mqtt_client_sync_time(login_info_t *mqtt_about);
int mqtt_client_start(login_info_t *mqtt_about);
int mqtt_client_data_handle(login_info_t *mqtt_about);

int iot_4g_reset(void);
void stop_4g_module(void);
void start_4g_module(void);

int16_t AT_TCP_Send_Data(uint8_t *Txdata_ptr, uint16_t Txdata_len,uint8_t *Rxdata_ptr);
int16_t AT_MQTT_Send_Public_Data(char *send_topic, uint8_t *data_ptr, uint16_t data_len);
void at_query_rssi(void);
void at_query_gps(void);

iot_4g_state_t get_iot_4g_state(void);
iot_4g_data_t get_iot_4g_data(void);

int mqtt_data_head(uint8_t ver, uint8_t *head, uint8_t cause, uint16_t cycel, uint8_t total, uint8_t seq);
char *Send4GCmd(char *ATcmd,char *Ack,uint8_t Times,uint16_t WaitTime);//static


char *At_4G_Get_In_PPP_Mode(void);
char *At_4G_Get_Out_PPP_Mode(void);

#endif

