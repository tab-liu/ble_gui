#include "app_time.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "driver/i2c.h"
#include "ext_rtc.h"
//#include "ll_time.h"

#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "drv_pcf8563.h"
#include "iot_mqtt.h"
#include "iot_period_task.h"
//#include "cc1312_ota.h"
#include "can_protocol.h"

#include "uart_device_process.h"

static const char *TAG = "[APP_TIME]";
static SemaphoreHandle_t rtc_sync_sem; 			/**< rtc time synchronization semaphore */



/*

struct tm {
   int tm_sec;      //    秒，范围从 0 到 59                
   int tm_min;      //   分，范围从 0 到 59          
   int tm_hour;     //    小时，范围从 0 到 23                
   int tm_mday;     //    一月中的第几天，范围从 1 到 31        
   int tm_mon;      //   月份，范围从 0 到 11        
   int tm_year;     //    自 1900 起的年数          
   int tm_wday;     //   一周中的第几天，范围从 0 到 6  
   int tm_yday;     //    一年中的第几天，范围从 0 到 365     
   int tm_isdst;    //    夏令时                      
};

*/




/**
 * @brief synchronize RTC time to system time
 *
 * @return esp error code, ESP_OK if successful, otherwise ESP_FAIL


 系统时间更新到reals.rtc_time
 */
esp_err_t time_sync_to_local(void)//read
{

	time_t now_date = time(NULL);
	ESP_LOGI(TAG,"time_sync_to_local now_date1:%llu",now_date);
	/* 日期转换为时间结构 */
	struct tm tm_now;
	struct tm tm_now1;
//	static uint16_t scnt=0;
	memcpy(&tm_now, localtime(&now_date), sizeof(struct tm));

	ESP_LOGI(TAG, "time_sync_to_local ChinadateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now.tm_mon+1,tm_now.tm_mday,tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
	memcpy(&tm_now1, gmtime(&now_date), sizeof(struct tm));
	ESP_LOGI(TAG, "time_sync_to_local NowdateTime:%d-%d-%d-%d-%d-%d",reals.rtc_time.year,tm_now1.tm_mon+1,tm_now1.tm_mday,tm_now1.tm_hour,tm_now1.tm_min,tm_now1.tm_sec);

    time(&reals.now);//从系统获取时间
	ESP_LOGI(TAG,"time_sync_to_local reals.now:%llu",reals.now);
    
    // 换算成日期
	localtime_r(&reals.now, &reals.tm);

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
	ESP_LOGI(TAG,"time_sync_to_local mon:%d day:%d hour:%d min:%d sec:%d",reals.rtc_time.mon,reals.rtc_time.day,reals.rtc_time.hour,reals.rtc_time.min,reals.rtc_time.sec);

	return ESP_OK;
}


/**
 * @brief synchronize system time to extended RTC
 *
 * @return0 if successful, otherwise -1
 */
int time_sync_local_to_rtc(void)//write hw rtc
{
	time_t now;
	struct tm time_stamp;
	time(&now);//从系统获取时间
    localtime_r(&now, &time_stamp); // 换算成日期

	{
		time_stamp.tm_year = reals.rtc_time.year +100;// + 2000 - 1900;
		if(reals.rtc_time.mon) 
		{
			time_stamp.tm_mon = reals.rtc_time.mon - 1;
		}
		else 
		{
			time_stamp.tm_mon = 0;
		}
//		time_stamp.tm_mon = reals.rtc_time.mon;		
		time_stamp.tm_mday = reals.rtc_time.day;
		time_stamp.tm_hour = reals.rtc_time.hour;
		time_stamp.tm_min = reals.rtc_time.min;
		time_stamp.tm_sec = reals.rtc_time.sec;
		time_stamp.tm_isdst = -1; //否是 DST(夏令时)，tm_isdst 含有：1: 是 DST;0: 不是 DST;-1: 由 mktime() 自己去判断当前系统设置是否是 DST



		ESP_LOGI(TAG, "windyAA:secs=%d, mins=%d, hours=%d, "
				"mday=%d, mon=%d, year=%d, wday=%d\n",
				time_stamp.tm_sec, time_stamp.tm_min, time_stamp.tm_hour,
				time_stamp.tm_mday, time_stamp.tm_mon, time_stamp.tm_year, time_stamp.tm_wday);

		ESP_LOGI(TAG, "windyBB:secs=%d, mins=%d, hours=%d, "
				"mday=%d, mon=%d, year=%d\n",
				reals.rtc_time.sec, reals.rtc_time.min, reals.rtc_time.hour,
				reals.rtc_time.day, reals.rtc_time.mon, reals.rtc_time.year);

		
		now = mktime(&time_stamp);						// 转换为时间戳，->s
	}
	ESP_LOGI(TAG, "windy1:1970 base secs=%lu",(uint32_t)now );


//	if(pcf8563_set_datetime((uint32_t)now) != 0)
//	{
//		ESP_LOGE(TAG, "local time synchronization to RTC time failed");
//		return -1;
//	}

	return 0;
}


/*------------------------------------------------------------------------------
 Function: rtc_have_new_set
 -----------------------------------------------------------------------------*/
/**
  * @brief      本地时间修改，存储至rtc
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void rtc_have_new_set(void)
{
//    reals.rtc_time.year = MicroInv_WR.mod_reg02000_Inv_base_set.year;
//    reals.rtc_time.mon = MicroInv_WR.mod_reg02000_Inv_base_set.mon;
//    reals.rtc_time.day = MicroInv_WR.mod_reg02000_Inv_base_set.day;
//    reals.rtc_time.hour = MicroInv_WR.mod_reg02000_Inv_base_set.hour;
//    reals.rtc_time.min = MicroInv_WR.mod_reg02000_Inv_base_set.min;
//    reals.rtc_time.sec = MicroInv_WR.mod_reg02000_Inv_base_set.sec;   
//    
    if(time_sync_local_to_rtc() != 0)
    {
        ESP_LOGE(TAG, "windy Rx RTC set error");
        return;
    }
    
    ESP_LOGW(TAG, "windy Rx RTC set cmd");
    ESP_LOGW(TAG, "windy RTC SET:YY-MM-DD:HH-MM-SS= %u-%u-%u:%u-%u-%u ",(uint16_t)reals.rtc_time.year+2000,
        (uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
        (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,
        (uint16_t)reals.rtc_time.sec);
    
    if(time_sync_rtc_to_local() != ESP_OK)
    {
        ESP_LOGE(TAG, "windy Rx RTC READ error");
        return;
    }
    
    ESP_LOGI(TAG, "windy Rx RTC set result");
//    ESP_LOGI(TAG, "set RTC read : YY-MM-DD:HH-MM-SS= %u-%u-%u:%u-%u-%u ",(uint16_t)MicroInv[0].mod_reg02000_Inv_base_set.year+2000,
//        (uint16_t)MicroInv[0].mod_reg02000_Inv_base_set.mon,(uint16_t)MicroInv[0].mod_reg02000_Inv_base_set.day,
//        (uint16_t)MicroInv[0].mod_reg02000_Inv_base_set.hour,(uint16_t)MicroInv[0].mod_reg02000_Inv_base_set.min,
//        (uint16_t)MicroInv[0].mod_reg02000_Inv_base_set.sec);
    
    reals.new_inv_rtc_set = 1;

}


void app_sntp_set_timezone(int8_t hour, int8_t min)
{
    char str[100] = {0};
    char signal;
    
    if (hour > 0) {
        signal = '-';
    } else {
        signal = '+';
        hour = -hour;
    }
    snprintf(str, sizeof(str), "UTC%c%d:%02d", signal, hour, min);
    ESP_LOGW(TAG, "set new timezone [%s]", str);
    setenv("TZ", str, 1);
    tzset();
}

/*------------------------------------------------------------------------------
 Function: time_have_new_set
 -----------------------------------------------------------------------------*/
/**
  * @brief      本地时间修改，存储至rtc
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void time_have_new_set(void)
{
	struct tm time_stamp;	 
    if ( 1 == reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT )
    {
        reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 0;
        
        char buffer[80] = {0};
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        

        if(time_sync_to_local() != ESP_OK)
        {
            ESP_LOGE(TAG, "RTC_set_SERVER_to_IOT : Rx RTC READ error");
            return;
        }
        
        memcpy(&Inv_WR.mod_reg02000_Inv_base_set.mon, &reals.rtc_time, sizeof(reals.rtc_time));
        memcpy(&Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone, &SetData.dev_info_t.SetTimeZone, sizeof(SetData.dev_info_t.SetTimeZone));
        
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %p %A", &timeinfo);
        ESP_LOGW(TAG, "SERVER Time is synchronized local [%s]", buffer);

        reals.last_rtc_sync_cloud = reals.now;
        reals.rtc_flag.sBit.RTC_valid_from_SERVER = 1;//初始化成功
    }
	else if(1 == reals.rtc_flag.sBit.RTC_set_APP_to_IOT)
	{
		reals.rtc_flag.sBit.RTC_set_APP_to_IOT =0;	
        /*服务器时间有效，不同步APP时间*/
        if(reals.rtc_flag.sBit.RTC_valid_from_SERVER==1)
        {
            ESP_LOGW(TAG,"Server Time has been obtained,App Time has been rejected");
            return;
        }

		time_stamp.tm_year = Inv_WR.mod_reg02000_Inv_base_set.year + 100;// + 2000 - 1900;
		if(Inv_WR.mod_reg02000_Inv_base_set.mon) 
		{
			time_stamp.tm_mon = Inv_WR.mod_reg02000_Inv_base_set.mon - 1;
		}
		else 
		{
			time_stamp.tm_mon = 0;
		}
	//		time_stamp.tm_mon = reals.rtc_time.mon; 	
		time_stamp.tm_mday = Inv_WR.mod_reg02000_Inv_base_set.day;
		time_stamp.tm_hour = Inv_WR.mod_reg02000_Inv_base_set.hour;
		time_stamp.tm_min = Inv_WR.mod_reg02000_Inv_base_set.min;
		time_stamp.tm_sec = Inv_WR.mod_reg02000_Inv_base_set.sec;
		time_stamp.tm_isdst = -1; //否是 DST(夏令时)，tm_isdst 含有：1: 是 DST;0: 不是 DST;-1: 由 mktime() 自己去判断当前系统设置是否是 DST		  
		ESP_LOGI(TAG,"time_have_new_set tm_mon:%d day:%d hour:%d min:%d sec:%d",time_stamp.tm_mon, time_stamp.tm_mday ,time_stamp.tm_hour ,time_stamp.tm_min ,time_stamp.tm_sec );
		reals.now = mktime(&time_stamp);			
		ESP_LOGI(TAG,"time_have_new_set reals.now1:%llu",reals.now);

		app_sntp_set_timezone(Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.hour, Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.min);
	
		//time_stamp.tm_hour-=Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.hour;
		reals.now = mktime(&time_stamp);						// 转换为时间戳
		ESP_LOGI(TAG,"time_have_new_set reals.now:%llu",reals.now);

		struct timeval tv_time = {
			.tv_sec = reals.now,
			.tv_usec = 0
		};
		reals.rtc_flag.sBit.RTC_valid_from_APP = 1;//初始化成功
		if(!esp_ota_is_doing())settimeofday(&tv_time, NULL);//设置时间到系统时间
        time_sync_to_local();
		//time_stamp.tm_hour+=Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.hour;
		reals.rtc_flag.sBit.RTC_set_IOT_to_INV =1;
        reals.last_rtc_sync_ble = reals.now;
	}
}

#if 0
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
        
        reals.rtc_time.year = Modbus_WR.mod_reg02000_Inv_base_set.year;
        reals.rtc_time.mon = Modbus_WR.mod_reg02000_Inv_base_set.mon;
        reals.rtc_time.day = Modbus_WR.mod_reg02000_Inv_base_set.day;
        reals.rtc_time.hour = Modbus_WR.mod_reg02000_Inv_base_set.hour;
        reals.rtc_time.min = Modbus_WR.mod_reg02000_Inv_base_set.min;
        reals.rtc_time.sec = Modbus_WR.mod_reg02000_Inv_base_set.sec;   
        
        app_sntp_set_timezone(Modbus_WR.mod_reg02000_Inv_base_set.SetTimeZone.hour, Modbus_WR.mod_reg02000_Inv_base_set.SetTimeZone.min);
            
        if(time_sync_local_to_rtc() != 0)
        {
            ESP_LOGE(TIMETAG, "Time set error");
            return;
        }
        
        ESP_LOGW(TIMETAG, "TIME SET:YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,
            (uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
            (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,
            (uint16_t)reals.rtc_time.sec);
			
#ifdef CONFIG_HARDWARE_PANEL
        if(time_sync_rtc_to_local(1) != ESP_OK)
        {
            ESP_LOGE(TIMETAG, "Rx RTC READ error");
            return;
        }
        
        ESP_LOGI(TIMETAG, "RTC read : YY-MM-DD:HH-MM-SS= [%u-%u-%u  %u:%u:%u] ",(uint16_t)reals.rtc_time.year+2000,
            (uint16_t)reals.rtc_time.mon,(uint16_t)reals.rtc_time.day,
            (uint16_t)reals.rtc_time.hour,(uint16_t)reals.rtc_time.min,
            (uint16_t)reals.rtc_time.sec);
#endif
        
        reals.rtc_flag.sBit.RTC_valid_from_APP = 1;//初始化成功
	}
    else if ( 1 == reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT )
    {
        reals.rtc_flag.sBit.RTC_set_SERVER_to_IOT = 0;
        
        char buffer[80] = {0};
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
#ifdef CONFIG_HARDWARE_PANEL  
        if(pcf8563_set_datetime(now) != 0)// 将当前UTC时间保存到外部RTC中
        {
            ESP_LOGE(TIMETAG, "local time synchronization to RTC time failed");
            return;
        }
#endif

        if(time_sync_rtc_to_local(1) != ESP_OK)
        {
            ESP_LOGE(TIMETAG, "RTC_set_SERVER_to_IOT : Rx RTC READ error");
            return;
        }
        
        memcpy(&Modbus_WR.mod_reg02000_Inv_base_set.mon, &reals.rtc_time, sizeof(reals.rtc_time));
        memcpy(&Modbus_WR.mod_reg02000_Inv_base_set.SetTimeZone, &SetData.dev_info_t.SetTimeZone, sizeof(SetData.dev_info_t.SetTimeZone));
        
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %p %A", &timeinfo);
        ESP_LOGW(TIMETAG, "SERVER Time is synchronized local [%s]", buffer);

        reals.rtc_flag.sBit.RTC_valid_from_SERVER = 1;//初始化成功
    }
}

#endif







static void app_sntp_config(void);

/*
now->real.rtc 在1秒周期内统一更新
NTP
*/
void time_sync_notification_cb (struct timeval *tv)
{
	#if 1
    char buffer[80] = {0};
    sntp_sync_status_t state = sntp_get_sync_status();
    if (state == SNTP_SYNC_STATUS_COMPLETED)
    {   
        struct tm tm;
        time_t now = time(NULL);
		ESP_LOGI(TAG,"time_sync_notification_cb now:::::%llu",now);
        
		reals.rtc_flag.sBit.RTC_valid_from_NTP = 1;//初始化成功
        
		//time_sync_to_local();     
        memcpy(&Inv_WR.mod_reg02000_Inv_base_set.mon, &reals.rtc_time, sizeof(reals.rtc_time));
        memcpy(&Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone, &SetData.dev_info_t.SetTimeZone, sizeof(SetData.dev_info_t.SetTimeZone));
		app_sntp_set_timezone(Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.hour, Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.min);
        //app_sntp_set_timezone(SetData.dev_info_t.SetTimeZone.hour, Inv_WR.mod_reg02000_Inv_base_set.SetTimeZone.min);
        //SetData.dev_info_t.SetTimeZone.al
        time_sync_to_local();     

  		//time_set_iot_to_arm();
        localtime_r((const time_t *)&now, &tm);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %p %A", &tm);
        ESP_LOGW(TAG, "Time is synchronized local [%s]", buffer);
    }
    else if (state == SNTP_SYNC_STATUS_IN_PROGRESS)
    {
    }
    else 
    {
    }
#endif
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
    esp_sntp_set_sync_interval(ONE_HOUR_INTERVAL); // 1hour (unit 1ms)
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED); // 同步时间后立即更新模式
    esp_sntp_init(); // init and start 
}

esp_err_t app_sntp_init(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
	app_sntp_config();
    return ESP_OK;
} 

/*
下级ARM 时间赋值给IOT

陆文杰：
INV CAN RTC实时数据，有INV主动上报帧，周期2s，0x1811xxxx；
set rtc的反向读取仅仅是set变量，不是实时值

*/
void Time_ARM_to_IOT(uint8_t id_index)
{
//	Inv[id_index].mod_reg02000_Inv_base_set.year
	reals.tm.tm_year = Inv_can[id_index].inv_data[0].inv_announce.date.year + 100;// + 2000 - 1900;
	if(Inv_can[id_index].inv_data[0].inv_announce.date.month) 
	{
		reals.tm.tm_mon = Inv_can[id_index].inv_data[0].inv_announce.date.month - 1;
	}
	else 
	{
		reals.tm.tm_mon = 0;
	}
//		reals.tm.tm_mon = reals.rtc_time.mon;		
	reals.tm.tm_mday = Inv_can[id_index].inv_data[0].inv_announce.date.day;
	reals.tm.tm_hour = Inv_can[id_index].inv_data[0].inv_announce.date.hour;
	reals.tm.tm_min = Inv_can[id_index].inv_data[0].inv_announce.date.minute;
	reals.tm.tm_sec = Inv_can[id_index].inv_data[0].inv_announce.date.second;
	reals.tm.tm_isdst = -1; //否是 DST(夏令时)，tm_isdst 含有：1: 是 DST;0: 不是 DST;-1: 由 mktime() 自己去判断当前系统设置是否是 DST		


	reals.now = mktime(&reals.tm);						// 转换为时间戳
	ESP_LOGI(TAG,"Time_ARM_to_IOT reals.now1:%llu",reals.now);
	app_sntp_set_timezone(SetData.dev_info_t.SetTimeZone.hour, SetData.dev_info_t.SetTimeZone.min);
	reals.now = mktime(&reals.tm);						// 转换为时间戳
	ESP_LOGI(TAG,"Time_ARM_to_IOT mon:%d day:%d hour:%d min:%d sec:%d",reals.tm.tm_mon,reals.tm.tm_mday ,reals.tm.tm_hour ,reals.tm.tm_min ,reals.tm.tm_sec );
	ESP_LOGI(TAG,"Time_ARM_to_IOT reals.now:%llu",reals.now);
	struct timeval tv_time = {
		.tv_sec = reals.now,
		.tv_usec = 0
	};
	settimeofday(&tv_time, NULL);//设置时间到系统时间
}

// 计算两个 uint8_t 数的绝对差值
uint8_t abs_diff_u8(uint8_t a, uint8_t b) {
    return a > b ? a - b : b - a;
}

bool is_time_diff_reasonable(uint8_t hour1, uint8_t min1, uint8_t hour2, uint8_t min2, uint8_t minute_threshold) {
    uint32_t minutes1 = hour1 * 60 + min1;
    uint32_t minutes2 = hour2 * 60 + min2;
    uint32_t minute_diff = abs(minutes1 - minutes2);
    return (minute_diff <= minute_threshold);
}

/*
因AC380 RTC硬件在 下级ARM，所以要先从ARM获取非零时间才算有效
仅在CAN RX 执行
*/
void RTC_Valid_Check(uint8_t id_index)
{
    ESP_LOGI(TAG,"Enter RTC_Valid_Check :%d",id_index);
	//CAN之外的其他来源RTC无效时候，才从CAN获取RTC
    if((0 == reals.rtc_flag.sBit.RTC_valid_from_NTP)
		&&(0 == reals.rtc_flag.sBit.RTC_valid_from_APP)
		&&(0 == reals.rtc_flag.sBit.RTC_valid_from_SERVER)
		&&(0 == reals.rtc_flag.sBit.RTC_valid_from_CAN)
		&&(Inv_can[id_index].inv_data[0].inv_announce.date.month  | 
        Inv_can[id_index].inv_data[0].inv_announce.date.hour | Inv_can[id_index].inv_data[0].inv_announce.date.day | 
        Inv_can[id_index].inv_data[0].inv_announce.date.second | Inv_can[id_index].inv_data[0].inv_announce.date.minute)
        &&(Inv_can[id_index].inv_data[0].inv_announce.date.year > 20))
    { 
        reals.rtc_flag.sBit.RTC_valid_from_CAN = 1;//初始化成功
		Time_ARM_to_IOT(id_index);
        reals.last_rtc_sync_local = reals.now;
	}


//在CAN和非CAN来源 RTC都有效情况下，需要给CAN校正
	if(((1 == reals.rtc_flag.sBit.RTC_valid_from_NTP)
			||(1 == reals.rtc_flag.sBit.RTC_valid_from_APP)
			||(1 == reals.rtc_flag.sBit.RTC_valid_from_SERVER))
			&&(0 == reals.rtc_flag.sBit.RTC_set_IOT_to_INV)
		)
		{
			// if((Inv[id_index].mod_reg02000_Inv_base_set.mon != reals.rtc_time.mon)
			// 	|| (Inv[id_index].mod_reg02000_Inv_base_set.year != reals.rtc_time.year)
			// 	|| (Inv[id_index].mod_reg02000_Inv_base_set.hour != reals.rtc_time.hour)
			// 	|| (Inv[id_index].mod_reg02000_Inv_base_set.day != reals.rtc_time.day)
			// 	|| (!is_time_diff_reasonable(Inv_can[0].inv_data[id_index].inv_announce.date.hour, Inv_can[0].inv_data[id_index].inv_announce.date.minute, 
			//                                 reals.rtc_time.hour, reals.rtc_time.min, 3))
			if((Inv_can[id_index].inv_data[0].inv_announce.date.month != reals.rtc_time.mon)
			|| (Inv_can[id_index].inv_data[0].inv_announce.date.year != reals.rtc_time.year)
			|| (Inv_can[id_index].inv_data[0].inv_announce.date.day != reals.rtc_time.day)
			|| (!is_time_diff_reasonable(Inv_can[id_index].inv_data[0].inv_announce.date.hour, Inv_can[id_index].inv_data[0].inv_announce.date.minute, 
			                                reals.rtc_time.hour, reals.rtc_time.min, 3))
				)
			{	
                ESP_LOGI(TAG,"RTC_Valid_Check INVCAN id_index:%d,year:%d mouth:%d day:%d hour:%d min:%d",id_index,Inv_can[id_index].inv_data[0].inv_announce.date.year,Inv_can[id_index].inv_data[0].inv_announce.date.month,
                               Inv_can[id_index].inv_data[0].inv_announce.date.day,Inv_can[id_index].inv_data[0].inv_announce.date.hour,
                                Inv_can[id_index].inv_data[0].inv_announce.date.minute);
                ESP_LOGI(TAG,"RTC_Valid_Check reals year:%d mouth:%d day:%d hour:%d min:%d",reals.rtc_time.year,reals.rtc_time.mon,
                                reals.rtc_time.day,reals.rtc_time.hour,
                                reals.rtc_time.min);
				reals.rtc_flag.sBit.RTC_set_IOT_to_INV = 1;//can tx	
			}

		}
}

