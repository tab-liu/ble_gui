/**
  ******************************************************************************
  * @file      app_time.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/24
  * @brief     时间控制相关函数
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/24  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "sync_time.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_sntp.h"

#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "drv_pcf8563.h"

#include "can_protocol.h"
#include "parameter.h"
#include "comm_define.h"

static const char *TAG = "[SYNC_TIME]";

static bool ext_rtc_connect_status = false;

/**
 * @brief synchronize RTC time to system time

 SYS->reals.rtc_time
 *
 * @return esp error code, ESP_OK if successful, otherwise ESP_FAIL
 */
esp_err_t time_sync_to_local(void)//read
{
   time(&reals.now);//从系统获取时间
    
    // 换算成日期
	localtime_r(&reals.now, &reals.tm);//输出 tm 叠加了时区的时间输出

	/* RTC时间转换为自定义BETA格式时间 */
	if (reals.tm.tm_year < 100) 
	{
		reals.rtc_time.year = 0;
	}
	else 
	{
		reals.rtc_time.year = ((uint16_t)reals.tm.tm_year + 1900 - 2000);
	}
	reals.rtc_time.mon = reals.tm.tm_mon + 1;
	reals.rtc_time.day = reals.tm.tm_mday;
	reals.rtc_time.hour = reals.tm.tm_hour;
	reals.rtc_time.min = reals.tm.tm_min;
	reals.rtc_time.sec = reals.tm.tm_sec;

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.year = reals.rtc_time.year;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.mon = reals.rtc_time.mon;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.day = reals.rtc_time.day;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.hour = reals.rtc_time.hour;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.min = reals.rtc_time.min;
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.sec = reals.rtc_time.sec; 
    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.SetTimeZone.all = IotSetData.dev_info_t.SetTimeZone.all;

    // ESP_LOGI(TAG, "RTC get: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,(uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
    //     (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,(uint16_t)reals.rtc_time.sec);

	return ESP_OK;
}


/*------------------------------------------------------------------------------
 Function: time_sync_save_to_system
 reals.rtc_time、RTC HW
 -----------------------------------------------------------------------------*/
/**
  * @brief      时间存储至本地（IOT）
  * @param[in]  void  
  * @param[out] None
  * @return     int
  */
int time_sync_save_to_system(void)//write hw rtc
{
	time_t now;
	struct tm time_stamp;
	time(&now);//从系统获取时间
    localtime_r(&now, &time_stamp); // 换算成日期

	time_stamp.tm_year = reals.rtc_time.year + 100;// + 2000 - 1900;
	if(reals.rtc_time.mon) 
	{
		time_stamp.tm_mon = reals.rtc_time.mon - 1;
	}
	else 
	{
		time_stamp.tm_mon = 0;
	}

	time_stamp.tm_mday = reals.rtc_time.day;
	time_stamp.tm_hour = reals.rtc_time.hour;
	time_stamp.tm_min = reals.rtc_time.min;
	time_stamp.tm_sec = reals.rtc_time.sec;
	time_stamp.tm_isdst = -1; //否是 DST(夏令时)，tm_isdst 含有：1: 是 DST;0: 不是 DST;-1: 由 mktime() 自己去判断当前系统设置是否是 DST
	
	now = mktime(&time_stamp);						// 转换为时间戳

    struct timeval tv_time = {
        .tv_sec = now,
        .tv_usec = 0
    };
    settimeofday(&tv_time, NULL);//设置时间到系统时间

	return 0;
}


/*------------------------------------------------------------------------------
 Function: time_new_set_check
 -----------------------------------------------------------------------------*/
/**
  * @brief      APP时间修改，存储至本地
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void time_new_set_check(void)
{
	if( 1 == reals.rtc_flag.sBit.RTC_set_APP_to_IOT )
	{
		reals.rtc_flag.sBit.RTC_set_APP_to_IOT = 0;	

        /*服务器时间有效，不同步蓝牙时间*/
        if (( reals.rtc_flag.sBit.RTC_valid_from_SERVER == 1) 
            && ( 2 != IotSetData.dev_info_t.Protocol_3r_Enable_mix1.bluetti_time_update )) {
            ESP_LOGW(TAG, "The server time has been obtained, the Bluetooth time has been rejected. ");
            return;
        }
        
        reals.rtc_time.year = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.year;
        reals.rtc_time.mon = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.mon;
        reals.rtc_time.day = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.day;
        reals.rtc_time.hour = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.hour;
        reals.rtc_time.min = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.min;
        reals.rtc_time.sec = top_modbus_wr.Inv.mod_reg02000_Inv_base_set.sec;   
                            
        ESP_LOGW(TAG, "TIME SET:YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,
            (uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
            (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,
            (uint16_t)reals.rtc_time.sec);
        
#ifdef CONFIG_IIC_BUS_EXT_RTC_ENABLE
        // RTC通信正常时更新时间
        if(ext_rtc_connect_status) {
            if(pcf8563_set_datetime(time(NULL)) != 0) {
                ESP_LOGE(TAG, "local time synchronization to RTC time failed");
            }
        }
#else
        if(time_sync_save_to_system() != 0)
        {
            ESP_LOGE(TAG, "Time set error");
            return;
        }
#endif

        if(time_sync_system_to_local(1) != ESP_OK)
        {
            ESP_LOGE(TAG, "RTC_set_APP_to_IOT : Rx RTC READ error");
            return;
        }

        reals.last_rtc_sync_ble = reals.now;
        reals.rtc_flag.sBit.RTC_valid_from_APP = 1;//初始化成功
        reals.rtc_flag.sBit.RTC_set_IOT_to_INV = 1;//同步设置到下级
	}
    else if ( 1 == reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT )
    {
        reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 0;
        
        char buffer[80] = {0};
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
#ifdef CONFIG_IIC_BUS_EXT_RTC_ENABLE
        // RTC通信正常时更新时间
        if(ext_rtc_connect_status) {
            if(pcf8563_set_datetime(time(NULL)) != 0) {
                ESP_LOGE(TAG, "local time synchronization to RTC time failed");
            }
        }
#endif

        if(time_sync_system_to_local(1) != ESP_OK)
        {
            ESP_LOGE(TAG, "RTC_set_SERVER_to_IOT : Rx RTC READ error");
            return;
        }
        
        memcpy(&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.mon, &reals.rtc_time, sizeof(reals.rtc_time));
        memcpy(&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.SetTimeZone, &IotSetData.dev_info_t.SetTimeZone, sizeof(IotSetData.dev_info_t.SetTimeZone));
        
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %p %A", &timeinfo);
        ESP_LOGW(TAG, "SERVER Time is synchronized local [%s]", buffer);

        reals.last_rtc_sync_cloud = reals.now;
        reals.rtc_flag.sBit.RTC_valid_from_SERVER = 1;//初始化成功
        reals.rtc_flag.sBit.RTC_set_IOT_to_INV = 1;//同步设置到下级
    }

    if ( reals.rtc_flag.sBit.RTC_valid_from_SERVER
        || reals.rtc_flag.sBit.RTC_valid_from_APP
        || reals.rtc_flag.sBit.RTC_valid_from_CAN 
        || reals.rtc_flag.sBit.RTC_valid_from_NTP 
        || reals.rtc_flag.sBit.RTC_valid_from_RTC ) 
    {
        reals.rtc_flag.sBit.Time_valid = 1;
    }
}


/*------------------------------------------------------------------------------
 Function: time_sync_system_to_local
 RTC HW->reals.rtc_time
 -----------------------------------------------------------------------------*/
/**
  * @brief      从本地（RTC/IOT获取时间）
  * @param[in]  uint8_t set_flag  
  * @param[out] None
  * @return     esp_err_t
  */
esp_err_t time_sync_system_to_local(uint8_t set_flag)//read；1s cycle
{
#ifdef CONFIG_IIC_BUS_EXT_RTC_ENABLE
    if ((1 == set_flag) && ext_rtc_connect_status ) {
        if(pcf8563_get_datetime(&reals.now) > 0) {
            // 换算成日期
            localtime_r(&reals.now, &reals.tm);//输出 tm 叠加了时区的时间输出
            
            /* RTC时间转换为自定义BETA格式时间 */
            if (reals.tm.tm_year < 100) {
                reals.rtc_time.year = 0;
            } else {
                reals.rtc_time.year = ((uint16_t)reals.tm.tm_year + 1900 - 2000);
            }
            reals.rtc_time.mon = reals.tm.tm_mon + 1;
            reals.rtc_time.day = reals.tm.tm_mday;
            reals.rtc_time.hour = reals.tm.tm_hour;
            reals.rtc_time.min = reals.tm.tm_min;
            reals.rtc_time.sec = reals.tm.tm_sec;

            if(time_sync_save_to_system() != 0) {
                ESP_LOGE(TAG, "Time set error");
            }

            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.year = reals.rtc_time.year;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.mon = reals.rtc_time.mon;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.day = reals.rtc_time.day;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.hour = reals.rtc_time.hour;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.min = reals.rtc_time.min;
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.sec = reals.rtc_time.sec; 
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg02000_Inv_base_set.SetTimeZone.all = IotSetData.dev_info_t.SetTimeZone.all;

            ESP_LOGI(TAG, "RTC get: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,(uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
                 (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,(uint16_t)reals.rtc_time.sec);
        } else {
            time_sync_to_local();
        }
    } else {
        time_sync_to_local();
    }
#else     
	time_sync_to_local();
#endif

	return ESP_OK;
}


/*
SNTP同步，不含时区
*/
void time_sync_notification_cb (struct timeval *tv)
{
    char buffer[80] = {0};
    sntp_sync_status_t state = sntp_get_sync_status();
    if (state == SNTP_SYNC_STATUS_COMPLETED)
    {   
        struct tm tm;
        time_t now = time(NULL);

#ifdef CONFIG_IIC_BUS_EXT_RTC_ENABLE
        // RTC通信正常时更新时间
        if(ext_rtc_connect_status) {
            if(pcf8563_set_datetime(now) != 0) {
                ESP_LOGE(TAG, "local time synchronization to RTC time failed");
            }
        }
#endif
		time_sync_system_to_local(1);
        memcpy(&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.mon, &reals.rtc_time, sizeof(reals.rtc_time));
        memcpy(&top_modbus_wr.Inv.mod_reg02000_Inv_base_set.SetTimeZone, &IotSetData.dev_info_t.SetTimeZone, sizeof(IotSetData.dev_info_t.SetTimeZone));
		reals.rtc_flag.sBit.RTC_valid_from_NTP = 1;//初始化成功
    
        localtime_r((const time_t *)&now, &tm);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %p %A", &tm);
        ESP_LOGW(TAG, "Time is synchronized local [%s]", buffer);

    }
    else if (state == SNTP_SYNC_STATUS_IN_PROGRESS)
    {
        // do nothing
    }
    else 
    {
        // do nothing
    }
}


static void app_sntp_config(void)
{

#if LWIP_DHCP_GET_NTP_SRV
    esp_sntp_servermode_dhcp(true);      // accept NTP offers from DHCP server, if any
#endif
    
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");      // 全球统一NTP服务器
    esp_sntp_setservername(1, "time.windows.com");  // 微软系统NTP服务器
    esp_sntp_setservername(2, "ntp.ntsc.ac.cn");    // 中国国家授时中心NTP服务器
    esp_sntp_setservername(3, "europe.pool.ntp.org"); // 欧洲NTP服务器
    esp_sntp_setservername(4, "time.nist.gov");     // 美国国家标准与技术研究所NTP服务器
    esp_sntp_setservername(5, "time.nus.edu.sg ");  // 新加坡国立教育大学NTP服务器
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_set_sync_interval(ONE_HOUR_INTERVAL); //刷新间隔
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED); // 同步时间后立即更新模式
    esp_sntp_init(); // init and start 
}

/*------------------------------------------------------------------------------
 Function: app_sntp_set_timezone
 -----------------------------------------------------------------------------*/
/**
  * @brief      时区设置
  * @param[in]  int8_t hour  
                int8_t min   
  * @param[out] None
  * @return     void
  */
void app_sntp_set_timezone(int8_t hour, int8_t min)
{
    char tz_posix_str[20] = {0};
    char signal;
    int8_t posix_hour = hour;
    int8_t posix_min = min;
    
    /*
    原因：POSIX TZ 环境变量的定义与我们通常的认知是相反的。它定义的偏移量是**“本地时间需要加上多少才能得到UTC时间”**。
    举例：
    我们通常说美国东部时间是 UTC-5。这意味着本地时间比UTC时间晚5个小时。
    根据POSIX定义，本地时间 +5 小时才能得到UTC时间。
    因此，对应的 TZ 字符串必须是 UTC+5。    
    */
    
    if (posix_hour > 0) {
        signal = '-';
    } else {
        signal = '+';
        posix_hour = -posix_hour;
    }
    snprintf(tz_posix_str, sizeof(tz_posix_str), "UTC%c%d:%02d", signal, posix_hour, posix_min);
    
    // 打印出实际时区和用于设置的POSIX字符串，避免混淆
    ESP_LOGW(TAG, "Set timezone to UTC[%02d:%02d] (POSIX TZ string: %s)", 
             hour, min, tz_posix_str);
    
    setenv("TZ", tz_posix_str, 1);
    tzset();
}  

esp_err_t app_sntp_init(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
	app_sntp_config();
    return ESP_OK;
} 

bool is_time_diff_reasonable(uint8_t hour1, uint8_t min1, uint8_t hour2, uint8_t min2, uint8_t minute_threshold) {
    uint32_t minutes1 = hour1 * 60 + min1;
    uint32_t minutes2 = hour2 * 60 + min2;
    uint32_t minute_diff = (minutes1 > minutes2) ? (minutes1 - minutes2) : (minutes2 - minutes1);
    return (minute_diff <= minute_threshold);
}

int Time_PACK_to_IOT(uint8_t id_index)
{
    if(!(can_node_rd.Pack[id_index].pack_announce.pack_date.month  | 
        can_node_rd.Pack[id_index].pack_announce.pack_date.hour | can_node_rd.Pack[id_index].pack_announce.pack_date.day | 
        can_node_rd.Pack[id_index].pack_announce.pack_date.second | can_node_rd.Pack[id_index].pack_announce.pack_date.minute)
        || (can_node_rd.Pack[id_index].pack_announce.pack_date.year < 20)) // 时间从BMS获取
    {
        return -1; // 或者其他适当的错误处理
    }
    
	reals.tm.tm_year = can_node_rd.Pack[id_index].pack_announce.pack_date.year + 100;// + 2000 - 1900;
	if(can_node_rd.Pack[id_index].pack_announce.pack_date.month) 
	{
		reals.tm.tm_mon = can_node_rd.Pack[id_index].pack_announce.pack_date.month - 1;
	}
	else 
	{
		reals.tm.tm_mon = 0;
	}
	reals.tm.tm_mday = can_node_rd.Pack[id_index].pack_announce.pack_date.day;
	reals.tm.tm_hour = can_node_rd.Pack[id_index].pack_announce.pack_date.hour;
	reals.tm.tm_min = can_node_rd.Pack[id_index].pack_announce.pack_date.minute;
	reals.tm.tm_sec = can_node_rd.Pack[id_index].pack_announce.pack_date.second;
	reals.tm.tm_isdst = -1; //否是 DST(夏令时)，tm_isdst 含有：1: 是 DST;0: 不是 DST;-1: 由 mktime() 自己去判断当前系统设置是否是 DST		

	reals.now = mktime(&reals.tm);						// 转换为时间戳
	
	struct timeval tv_time = {
		.tv_sec = reals.now,
		.tv_usec = 0
	};
	settimeofday(&tv_time, NULL);//设置时间到系统时间
    ESP_LOGI(TAG, "Get bms[%d] time: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",id_index,(uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.year + 2000,
            (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.month, (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.day,
            (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.hour, (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.minute,
            (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.second);
    time_sync_system_to_local(0);
    return 0;
}

/*------------------------------------------------------------------------------
 Function: Time_ARM_to_IOT
 -----------------------------------------------------------------------------*/
/**
  * @brief      下级ARM 时间赋值给IOT
  * @param[in]  uint8_t id_index  
  * @param[out] None
  * @return     int
  */
int Time_ARM_to_IOT(uint8_t id_index)
{
    if(!(can_node_rd.Inv[id_index].inv_announce.date.month  | 
        can_node_rd.Inv[id_index].inv_announce.date.hour | can_node_rd.Inv[id_index].inv_announce.date.day | 
        can_node_rd.Inv[id_index].inv_announce.date.second | can_node_rd.Inv[id_index].inv_announce.date.minute)
        || (can_node_rd.Inv[id_index].inv_announce.date.year < 20)) // 时间从BMS获取
    {
        return -1; // 或者其他适当的错误处理
    }
    

	reals.tm.tm_year = can_node_rd.Inv[id_index].inv_announce.date.year + 100;// + 2000 - 1900;
	if(can_node_rd.Inv[id_index].inv_announce.date.month) 
	{
		reals.tm.tm_mon = can_node_rd.Inv[id_index].inv_announce.date.month - 1;
	}
	else 
	{
		reals.tm.tm_mon = 0;
	}
	reals.tm.tm_mday = can_node_rd.Inv[id_index].inv_announce.date.day;
	reals.tm.tm_hour = can_node_rd.Inv[id_index].inv_announce.date.hour;
	reals.tm.tm_min = can_node_rd.Inv[id_index].inv_announce.date.minute;
	reals.tm.tm_sec = can_node_rd.Inv[id_index].inv_announce.date.second;
	reals.tm.tm_isdst = -1; //否是 DST(夏令时)，tm_isdst 含有：1: 是 DST;0: 不是 DST;-1: 由 mktime() 自己去判断当前系统设置是否是 DST		

	reals.now = mktime(&reals.tm);						// 转换为时间戳
	
	struct timeval tv_time = {
		.tv_sec = reals.now,
		.tv_usec = 0
	};
	settimeofday(&tv_time, NULL);//设置时间到系统时间
    ESP_LOGI(TAG, "Get inv[%d] time: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",id_index,(uint16_t)can_node_rd.Inv[id_index].inv_announce.date.year + 2000,
            (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.month, (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.day,
            (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.hour, (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.minute,
            (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.second);
    time_sync_system_to_local(0);
    return 0;
}

/*------------------------------------------------------------------------------
 Function: RTC_Valid_Check
 -----------------------------------------------------------------------------*/
/**
  * @brief      本地时间检查
  * @param[in]  uint8_t id_index  
  * @param[out] None
  * @return     void
  */
void RTC_Valid_Check(uint8_t id_index)
{
#ifdef CONFIG_RTC_TIME_VAILD_FROM_CAN

	//CAN之外的其他来源RTC无效时候，才从CAN获取RTC
    if((0 == reals.rtc_flag.sBit.RTC_valid_from_NTP)
		&&(0 == reals.rtc_flag.sBit.RTC_valid_from_APP)
		&&(0 == reals.rtc_flag.sBit.RTC_valid_from_RTC)
		&&(0 == reals.rtc_flag.sBit.RTC_valid_from_SERVER)
        &&(0 == reals.rtc_flag.sBit.RTC_valid_from_CAN)//仅首次获取
	)
    { 
#if CONFIG_RTC_TIME_SOURCE_CHANGE == 1 || CONFIG_RTC_TIME_SOURCE_CHANGE == 3
        // 时间从INV获取
        if(Time_ARM_to_IOT(id_index) == 0) {
            reals.rtc_flag.sBit.RTC_valid_from_CAN = 1;//初始化成功
        }
#elif CONFIG_RTC_TIME_SOURCE_CHANGE == 2
        // 时间从BMS获取
        if(Time_PACK_to_IOT(id_index) == 0) {
            reals.rtc_flag.sBit.RTC_valid_from_CAN = 1;//初始化成功
        }
#endif
        if ( 1 == reals.rtc_flag.sBit.RTC_valid_from_CAN ) {
            reals.last_rtc_sync_local = reals.now;
        }
	}
    
#endif        

#ifdef CONFIG_RTC_TIME_CHECK_SET_TO_CAN

    //在CAN和非CAN来源 RTC都有效情况下，需要给CAN校正
    static time_t last_check_time = 0;
	if(((1 == reals.rtc_flag.sBit.RTC_valid_from_NTP)
            || (1 == reals.rtc_flag.sBit.RTC_valid_from_APP)
            || (1 == reals.rtc_flag.sBit.RTC_valid_from_RTC)
            || (1 == reals.rtc_flag.sBit.RTC_valid_from_SERVER))
        && (0 == reals.rtc_flag.sBit.RTC_set_IOT_to_INV)
        && ((reals.now - last_check_time) > 60)
        && (0 == reals.IOT_Status_Flag.sBit.system_sleep_flag)
#if CONFIG_RTC_TIME_SOURCE_CHANGE == 1
        && (0 != reals.online_Inv_num)
#else
        && (0 != reals.online_Pack_num)
#endif
        && (0 == reals.system_ota.ota_total_pct)
        && (false == sys_is_updating()))
	{
#if CONFIG_RTC_TIME_SOURCE_CHANGE == 1
        if((can_node_rd.Inv[id_index].inv_announce.date.month != reals.rtc_time.mon)
            || (can_node_rd.Inv[id_index].inv_announce.date.year != reals.rtc_time.year)
            || (can_node_rd.Inv[id_index].inv_announce.date.day != reals.rtc_time.day)
            || (!is_time_diff_reasonable(can_node_rd.Inv[id_index].inv_announce.date.hour, can_node_rd.Inv[id_index].inv_announce.date.minute, 
                                            reals.rtc_time.hour, reals.rtc_time.min, 2))
        )
        {   
            ESP_LOGW(TAG, "arm: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)can_node_rd.Inv[id_index].inv_announce.date.year + 2000,
                    (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.month, (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.day,
                    (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.hour, (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.minute,
                    (uint16_t)can_node_rd.Inv[id_index].inv_announce.date.second);
            
            ESP_LOGW(TAG, "local: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,(uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
                    (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,(uint16_t)reals.rtc_time.sec);   
            reals.rtc_flag.sBit.RTC_set_IOT_to_INV = 1;

        }
#elif CONFIG_RTC_TIME_SOURCE_CHANGE == 2
    	if((can_node_rd.Pack[id_index].pack_announce.pack_date.month != reals.rtc_time.mon)
			|| (can_node_rd.Pack[id_index].pack_announce.pack_date.year != reals.rtc_time.year)
			|| (can_node_rd.Pack[id_index].pack_announce.pack_date.day != reals.rtc_time.day)
			|| (!is_time_diff_reasonable(can_node_rd.Pack[id_index].pack_announce.pack_date.hour, can_node_rd.Pack[id_index].pack_announce.pack_date.minute, 
			                                reals.rtc_time.hour, reals.rtc_time.min, 3))
        )
		{	
            ESP_LOGW(TAG, "BMS: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.year + 2000,
                    (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.month, (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.day,
                    (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.hour, (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.minute,
                    (uint16_t)can_node_rd.Pack[id_index].pack_announce.pack_date.second);
            
            ESP_LOGW(TAG, "local: YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,(uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
                    (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,(uint16_t)reals.rtc_time.sec);   
			reals.rtc_flag.sBit.RTC_set_IOT_to_INV = 1;
		}
#elif CONFIG_RTC_TIME_SOURCE_CHANGE == 3    
        // 广播时间
        reals.rtc_flag.sBit.RTC_set_IOT_to_INV = 1;
#endif
        
        last_check_time = reals.now;
	}

#endif        
}


void system_time_init(void)
{
	/* when using extended rtc for the first time, it is necessary to first write a time,
	* otherwise an incorrect time will be read. Here, a time is set when rtc reads an error or
	* when the reading time is less than 2023 */
	time_t rtc_time = 0;
    char buffer[80] = {0};

	//设置时区
	app_sntp_set_timezone(IotSetData.dev_info_t.SetTimeZone.hour, IotSetData.dev_info_t.SetTimeZone.min);

#ifdef CONFIG_IIC_BUS_EXT_RTC_ENABLE
    
    if((pcf8563_get_datetime(&rtc_time) < 0)//获取时间失败 
        || ((uint32_t)rtc_time < TIME_STAMP_2020))//时间不合规范
    {
        if (pcf8563_set_datetime((time_t)TIME_STAMP_2020) != 0) 
        {           
            // RTC通信异常
            ESP_LOGE(TAG, "Failed to Connect to Ext RTC!");
            top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.iot_fault.bit.rtc_fault = 1;
            ext_rtc_connect_status = false;
        }
        else
        {
            // RTC通信正常
            ESP_LOGW(TAG, "Ext RTC Initial Successfully!");
            ext_rtc_connect_status = true;
        }
    }
    else
    {
        // RTC通信正常
        ext_rtc_connect_status = true;
        
        // 时间检查（ if the system time is less than 2023 years, set the system time to 2023 years）
        if ( (uint32_t)rtc_time > TIME_STAMP_2023 )
        {
            /* Synchronize RTC time to system time during system initialization */
                struct timeval tv_time = {
                .tv_sec = rtc_time,
                .tv_usec = 0
            };
            settimeofday(&tv_time, NULL);                   //设置时间到系统时间
            time_sync_to_local();
            reals.rtc_flag.sBit.RTC_valid_from_RTC = 1;

            /* print system current time */
            struct tm time_stamp;
            localtime_r(&rtc_time, &time_stamp); 
            strftime(buffer, 80, "%Y-%m-%d %H:%M:%S %p %A", &time_stamp);
            ESP_LOGW(TAG, "Get RTC Time: [%s]", buffer);
        }
        else
        {
            /* print system current time */
            struct tm time_stamp;
            localtime_r(&rtc_time, &time_stamp); 
            strftime(buffer, 80, "%Y-%m-%d %H:%M:%S %p %A", &time_stamp);
            ESP_LOGE(TAG, "RTC Time is invaild : [%s]", buffer);
        }
    }
    
#endif 
}

bool parse_iso8601_to_tm_timestamp(const char *datetime_str, struct tm *tm_timestamp, time_res_struct *CurrTimeZone) {
    struct tm tm_time = {0};
    int tz_hour = 0, tz_min = 0;
    char sign = '+';
    char z_char = 0;

    // 检查输入是否为空
    if (datetime_str == NULL || tm_timestamp == NULL || CurrTimeZone == NULL) {
        ESP_LOGE(TAG, "Error: Input string or timestamp pointer is NULL.");
        return false;
    }

    // 检查输入长度是否合理
    if (strlen(datetime_str) < 20) { // ISO 8601 格式的最短长度 (e.g., 2024-07-26T10:30:00Z)
        ESP_LOGE(TAG, "Error: Input string is too short.");
        return false;
    }

    // 1. 先解析固定的日期和时间部分 (YYYY-MM-DDTHH:MM:SS)
    int parsed_items = sscanf(datetime_str, "%d-%d-%dT%d:%d:%d",
                               &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
                               &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);

    if (parsed_items != 6) {
        ESP_LOGE(TAG, "Error: Failed to parse the base datetime part.");
        return false;
    }

    // 2. 定位到时间部分的末尾，准备解析时区
    const char *tz_part = datetime_str + 19; // 指向 "YYYY-MM-DDTHH:MM:SS" 之后的位置

    // 3. 跳过可选的毫秒部分
    if (*tz_part == '.' || *tz_part == ',') { // 同时支持点和逗号
        tz_part++; // 跳过分隔符
        while (isdigit((unsigned char)*tz_part)) {
            tz_part++; // 跳过所有毫秒数字
        }
    }

    // 4. 解析时区部分
    if (*tz_part == 'Z' || *tz_part == 'z') {
        // UTC 时间
        sign = '+';
        tz_hour = 0;
        tz_min = 0;
    } else if (*tz_part == '+' || *tz_part == '-') {
        // 带偏移的时区
        sign = *tz_part;
        // 尝试解析 HH:MM 格式
        if (sscanf(tz_part + 1, "%d:%d", &tz_hour, &tz_min) != 2) {
            // 如果失败，尝试解析 HHMM 格式
            if (sscanf(tz_part + 1, "%2d%2d", &tz_hour, &tz_min) != 2) {
                 ESP_LOGE(TAG, "Error: Failed to parse timezone offset.");
                 return false;
            }
        }
    } else {
        ESP_LOGE(TAG, "Error: Invalid or missing timezone specifier.");
        return false;
    }

    // 检查日期和时间的范围
    if (tm_time.tm_year < 1900 || tm_time.tm_year > 9999) {
        ESP_LOGE(TAG, "Error: Year out of range (1900-9999).");
        return false;
    }
    if (tm_time.tm_mon < 1 || tm_time.tm_mon > 12) {
        ESP_LOGE(TAG, "Error: Month out of range (1-12).");
        return false;
    }
    if (tm_time.tm_mday < 1 || tm_time.tm_mday > 31) {
        ESP_LOGE(TAG, "Error: Day out of range (1-31).");
        return false;
    }
    if (tm_time.tm_hour < 0 || tm_time.tm_hour > 23) {
        ESP_LOGE(TAG, "Error: Hour out of range (0-23).");
        return false;
    }
    if (tm_time.tm_min < 0 || tm_time.tm_min > 59) {
        ESP_LOGE(TAG, "Error: Minute out of range (0-59).");
        return false;
    }
    if (tm_time.tm_sec < 0 || tm_time.tm_sec > 59) {
        ESP_LOGE(TAG, "Error: Second out of range (0-59).");
        return false;
    }

    // 更新DST时间内时区
    if (sign == '-') {
        CurrTimeZone->hour = -tz_hour;
    } else {
        CurrTimeZone->hour = tz_hour;
    }
    CurrTimeZone->min = tz_min; 

    // 调整年份和月份
    tm_time.tm_year -= 1900; // tm_year 是从 1900 年开始计数
    tm_time.tm_mon -= 1;     // tm_mon 是从 0 开始计数
    tm_time.tm_isdst = -1;   // 是否 DST(夏令时)，tm_isdst 含有：1: 是 DST;0: 不是 DST;-1: 由 mktime() 自己去判断当前系统设置是否是 DST   

    // 将解析后的值复制到 tm_timestamp 指针所指向的内存中
    *tm_timestamp = tm_time;

    return true;
}

bool tm_time_to_timestamp(time_t *timestamp, struct tm *tm_time) 
{
    // 检查输入指针是否为空
    if (timestamp == NULL || tm_time == NULL) {
        ESP_LOGE(TAG, "Error: Input pointer is NULL.");
        return false;
    }

    // 检查 tm_time 的字段是否在合理范围内
    if (tm_time->tm_year < 70 || tm_time->tm_mon < 0 || tm_time->tm_mon > 11 ||
        tm_time->tm_mday < 1 || tm_time->tm_mday > 31 ||
        tm_time->tm_hour < 0 || tm_time->tm_hour > 23 ||
        tm_time->tm_min < 0 || tm_time->tm_min > 59 ||
        tm_time->tm_sec < 0 || tm_time->tm_sec > 59) {
        ESP_LOGE(TAG, "Error: tm_time contains invalid values.");
        return false;
    }

    // 转换为时间戳（假设本地时区与字符串中的时区一致）
    *timestamp = mktime(tm_time);

    // 检查 mktime 的返回值
    if (*timestamp == -1) {
        ESP_LOGE(TAG, "Error: Failed to convert to timestamp.");
        return false;
    }

    return true;
}

/*------------------------------------------------------------------------------
 Function: iCloud_Dst_Time_Check_and_Update
 -----------------------------------------------------------------------------*/
/**
  * @brief      解析DST时间并直接更新到本地
  * @param[in]  const char *datetime_str  
                uint16_t str_len          
  * @param[out] None
  * @return     void
  */
bool iCloud_Dst_Time_Check_and_Update(const char *datetime_str, uint16_t str_len)
{
    struct tm server_tm_time = {0};
    time_t server_time_now = {0}; 
    time_res_struct CurrTimeZone = {0};

    if ( datetime_str == NULL || str_len == 0 ) {
        ESP_LOGE(TAG, "iCloud_Dst_Time_Check_and_Update : Input is NULL.");
        return false;
    }

    // 分配缓冲区
    char *buff = (char *)iot_calloc(str_len + 1);
    if (buff == NULL) {
        ESP_LOGE(TAG, "iCloud_Dst_Time_Check_and_Update malloc failed!");
        return false;
    }

    memcpy(buff, datetime_str, str_len);
    if(false == parse_iso8601_to_tm_timestamp(buff, &server_tm_time, &CurrTimeZone)) {
        iot_free(buff);
        return false;
    };
        
    //判断是否为无效写入
    if(IotSetData.dev_info_t.SetTimeZone.all != CurrTimeZone.all)
    {
        app_sntp_set_timezone(CurrTimeZone.hour, CurrTimeZone.min);
        IotSetData.dev_info_t.SetTimeZone.all = CurrTimeZone.all;
        reals.SetDataWrFlag.sBit.SetTimeZone = 1;
    }
    
    if ( tm_time_to_timestamp(&server_time_now, &server_tm_time) )
    {
        ESP_LOGI(TAG, "get dst time success(ISO8601) : %llu", server_time_now);
        struct timeval tv_time = {
            .tv_sec = server_time_now,
            .tv_usec = 0
        };
        settimeofday(&tv_time, NULL);//设置时间到系统时间
        reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 1;
    }

    iot_free(buff);
    return true;
}

/**
 * @brief 获取本地时区当天00:00:00的时间戳
 * @return 成功返回零点时间戳，失败返回 (time_t)-1
 */
time_t get_local_midnight(void)
{
    time_t now = time(NULL);
    struct tm tm_val;

    // 线程安全：将时间戳转为本地时间结构体
    if (localtime_r(&now, &tm_val) == NULL) {
        return (time_t)-1;
    }

    // 仅清零时分秒，保留年月日，得到当天 00:00:00
    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;

    // 转回时间戳：自动处理时区、夏令时、日期合法性
    return mktime(&tm_val);
}

