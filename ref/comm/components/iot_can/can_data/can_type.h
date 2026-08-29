#ifndef CAN_TYPE_H
#define CAN_TYPE_H

#include <stdint.h>
#include "inv_define.h"

#pragma pack(1)

//enum {
//    IOT = 0,
//    INV_ARM = 1,
//    INV_DSP = 2,
//    BMS = 3,
//    BA = 4,
//    PACK_BCU = 5,
//    PACK_BMU = 6,
//    PACK_BMS = 7,
//    PACK_M1 = 8,
//    PACK_SAFT = 9,
//    PACK_HV = 10,
//	IOT_LCD = 11,
//	IOT_LCD2 = 12,
//	RF = 13,
//};
// 0-IOT; 1-INV_ARM; 2-INV_DSP;3-BMS�?-BA; 5-PACK_BCU; 6-PACK_BMU;7-PACK_BMS; 8-PACK_M1;9-PACK安全模块�?0-PACK高压模块

typedef union
{
	uint32_t id;
	struct{
        uint16_t pn : 8;      // 0:7
        uint16_t sa : 8;      // 8:15  源地址             
        uint16_t fix : 16;     // 
    }bit;
}can_id_t_alpha;//B300系列旧CAN ID定义规则  



#pragma pack()




#endif
