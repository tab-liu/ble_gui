

#include "comm_define.h"

static const char *TAG = "[SHARE_FUN]";


/************************************************/
/*函数名称:CompareSetData  						          */
/*函数功能:比较两组RAM中的数据					        */
/*入口参数:unsigned char *pRAM1 要比较的数据1	  */
/*         unsigned char *pRAM2 要比较的数据2	  */
/*         uint16_t	Length	要比较的数据长度      */
/*出口参数:1:相同,0:不相同 						          */
/*资源:											                    */
/*子函数:	             						              */
/************************************************/
uint16_t	CompareSetData(uint8_t *pRAM1, uint8_t *pRAM2, uint16_t Length)
{
	uint16_t ii=0;
    for(ii = 0;ii < Length;ii++)
	{
		if((*pRAM1++) != (*pRAM2++))
		{
			return	0;							/*两组数据不相同*/
		}
	}	
	return	1;									/*两组数据相同*/
}

