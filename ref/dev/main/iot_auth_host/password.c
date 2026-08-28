#include "string.h"
#include "password.h"
//#include "modbus_slave.h"
#include "md5.h"
#include "rtc.h"

#include "esp_log.h"
#include "esp_event.h"//EXT_RAM_BSS_ATTR 需要

#define START_TIME          1371517200  // 2013-06-18 09:00:00
#define TIME_STEP           30          // 30s --> 30 *1000 ms

#define SHARED_PRIVATE_KEY  "ga3sa4hj6kfl"


static uint8_t gBytesSecret[48] = {0};

static void HexToAscii( uint8_t *str, const uint8_t *hex , uint16_t hex_len)
{
    uint16_t i=0;
	uint16_t j=0;
    uint8_t  high = 0;
    uint8_t  low = 0;

    while( j < hex_len ) // to ascii
    {
        high = hex[ j ]/0x10;
        low  = hex[ j ]%0x10;
        str[i] = ( high < 10 ) ? (high + '0')  : (high-0x0A + 'A');
        str[i+1] = ( low < 10 ) ?  (low + '0') : (low-0x0A + 'A');

        i += 2;
        j++;
    }
}

//static void CreatePasswordText(uint8_t *text, uint8_t *src, uint8_t length)
//{
//    uint8_t buf[16] = {0};
//    MD5_CTX   md5;
////    static MD5_CTX   md5;
////	memset(&md5, 0,sizeof(MD5_CTX));
//    MD5Init(&md5);
//    MD5Update( &md5, src, length ); /* md5 calac */
//    MD5Final( &md5 , buf );  
//    
//    HexToAscii(text, (const uint8_t *)buf, 16); // shift capital letter ASCII
//}




static void CreatePasswordText(uint8_t *text, uint8_t *src, uint8_t length)
{
//	uint8_t j=0;
//	EXT_RAM_BSS_ATTR MD5_CTX   md5_debug;
//	EXT_RAM_BSS_ATTR uint8_t buf_debug[16] = {0};
	MD5_CTX   md5;
	uint8_t buf[16] = {0};

    MD5Init(&md5);
//	ESP_LOGW("[WD PASSWD]", "windy1 md5_debug:state=%lu,%lu,%lu,%lu,count=%lu,%lu ",md5_debug.state[0],md5_debug.state[1],md5_debug.state[2],md5_debug.state[3],md5_debug.count[0],md5_debug.count[1]);//windy debug
//	ESP_LOGW("[WD PASSWD]", "windy1 md5_debug.buffer=%s ",md5_debug.buffer);//windy debug
//    for(j = 0; j < 64; j++)
//    {
//		ESP_LOGW("[WD PASSWD]", "windy1 md5_debug.buffer[%d]=%u ",j,md5_debug.buffer[j]);
//    }    

	
//	ESP_LOGW("[WD PASSWD]", "windy src=%s ",(uint8_t *)&src);//windy debug
	
    MD5Update( &md5, src, length ); /* md5 calac */
//	if(8 == length)
//	{
//		for(j = 0; j < 8; j++)
//		{
//			ESP_LOGW("[WD PASSWD]", "windy src[%d]=%u ",j,src[j]);
//		} 
//	}
	
//	ESP_LOGW("[WD PASSWD]", "windy2 md5_debug:state=%lu,%lu,%lu,%lu,count=%lu,%lu ",md5_debug.state[0],md5_debug.state[1],md5_debug.state[2],md5_debug.state[3],md5_debug.count[0],md5_debug.count[1]);//windy debug
//	ESP_LOGW("[WD PASSWD]", "windy2 md5_debug.buffer=%s ",md5_debug.buffer);//windy debug
   	
//windy debug:实验发现，此行之前，	md5_debug.buffer有内容，此行之后，md5_debug.buffer 变全0
    //MD5Final_2( &md5_debug , buf_debug );  //windy issue
    MD5Final( buf, &md5 );  //windy issue
    
//    for(j = 0; j < 16; j++)
//    {
//		ESP_LOGW("[WD PASSWD]", "windy buf_debug[%d]=%u ",j,buf_debug[j]);
//    }       
//	ESP_LOGW("[WD PASSWD]", "windy3 md5_debug:state=%lu,%lu,%lu,%lu,count=%lu,%lu ",md5_debug.state[0],md5_debug.state[1],md5_debug.state[2],md5_debug.state[3],md5_debug.count[0],md5_debug.count[1]);//windy debug
//	ESP_LOGW("[WD PASSWD]", "windy3 md5_debug.buffer=%s ",md5_debug.buffer);//windy debug

    HexToAscii(text, (const uint8_t *)buf, 16); // shift capital letter ASCII
}

static uint8_t CreateKarray(char *name, uint64_t code)
{
    uint8_t i = 0, j = 0;
    char codeStr[100] = {0};
    const char *ptr = SHARED_PRIVATE_KEY;
    uint8_t len ;
    
    snprintf(codeStr, sizeof(codeStr), "%llu", code);
    
    gBytesSecret[i++] = name[0]; // sn byte0
    gBytesSecret[i++] = name[1]; // sn byte1
    
    len = strlen(SHARED_PRIVATE_KEY)/2;
    for(j = 0; j < len; j++)
    {
        gBytesSecret[i++] = ptr[j];
    }
    
    len = strlen(codeStr);
    for(j = 0; j < len; j++)
    {
        gBytesSecret[i++] = codeStr[j];
    } 
    
    len = strlen(SHARED_PRIVATE_KEY)/2;
    for(j = 0; j < len; j++)
    {
        gBytesSecret[i++] = ptr[j + len];
    }
    
    len = strlen((const char *)name);
    gBytesSecret[i++] = name[len-2]; // sn byte n-1
    gBytesSecret[i++] = name[len-1]; // sn byte n

    return i;
}


//uint8_t  kText[32];
//uint8_t  cText[32]; 
//uint8_t  rawPw[64];
//uint8_t  bytesTime[8];

uint32_t CreateEncryptPassword(char *name, uint64_t code, uint32_t nowTime)
{
    uint8_t  i, j;
    uint8_t  kText[32];
    uint8_t  cText[32]; 
    uint8_t  rawPw[64];
    uint8_t  bytesTime[8];
    uint32_t password = 0; 
    uint64_t time=0;    

	//nowTime =START_TIME;//1675402903;//windy add force debug
    time = (nowTime - START_TIME) / TIME_STEP;
//	ESP_LOGW("[WD PASSWD]", "windy in CreateEncryptPassword time=%llu ",time);//windy debug
//	ESP_LOGW("[WD PASSWD]", "windy22 in CreateEncryptPassword nowTime=%lu ",nowTime);//windy debug

    for(i = 8; i > 0; i--)
    {
        bytesTime[i - 1] = (time & 0xFF);
        time >>= 8;
//		ESP_LOGW("[WD PASSWD]", "windy IN bytesTime[%d]=%u ",i - 1,bytesTime[i - 1]);//windy debug
//		ESP_LOGW("[WD PASSWD]", "windy time=%llu ",time);//windy debug
    }

    i = CreateKarray(name, code);
    
    CreatePasswordText(kText, gBytesSecret, i);
   // CreatePasswordText(cText, bytesTime,    sizeof(bytesTime));
    CreatePasswordText(cText, bytesTime,    8);
	
//	ESP_LOGW("[WD PASSWD]", "windy bytesTime=%s ",bytesTime);//windy debug
//	ESP_LOGW("[WD PASSWD]", "windy bytesTime[x8]=%u,%u,%u,%u,%u,%u,%u,%u ",bytesTime[0],bytesTime[1],bytesTime[2],bytesTime[3],bytesTime[4],bytesTime[5],bytesTime[6],bytesTime[7]);//windy debug
//	
//	ESP_LOGW("[WD PASSWD]", "windy  sizeof(bytesTime)=%u ",sizeof(bytesTime));//windy debug
//	ESP_LOGW("[WD PASSWD]", "windy kText=%s ",kText);//windy debug
//	ESP_LOGW("[WD PASSWD]", "windy cText=%s ",cText);//windy debug
    
    for (i = 0, j = 0; i < 32; i++)
    {
        rawPw[j]     = kText[i];
        rawPw[j + 1] = cText[i];
        j += 2;
    }
//	ESP_LOGW("[WD PASSWD]", "windy rawPw=%s ",rawPw);//windy debug
    
    i = rawPw[63] & 0x0F;
//	ESP_LOGW("[WD PASSWD]", "windy i=%u ",i);//windy debug
    
    password |= (uint32_t)(rawPw[i+0] & 0x7F) << 24;
    password |= (uint32_t)(rawPw[i+1] & 0xFF) << 16;
    password |= (uint32_t)(rawPw[i+2] & 0xFF) << 8;
    password |= (uint32_t)(rawPw[i+3] & 0xFF) ;
//	ESP_LOGW("[WD PASSWD]", "windy password 111=%lu ",password);//windy debug
    
    password %= 100000000;
//	ESP_LOGW("[WD PASSWD]", "windy  CreateEncryptPassword return=%lu ",password);//windy debug
    return password;
}




