/**
 ******************************************************************************
 * @file      Lcd_Hw_Cn91c4.c
 * @version   0.1
 * @author    Lixr
 * @date      2024/04/26
 * @brief     Cn91c4s96底层驱动（和写EEPROM类似的IIC通讯）
 * @par       History
 * <table>
 * <tr><th>Date       <th>Version <th>Author     <th>Description
 * <tr><td>2023/11/17 <td>1.0     <td>DL         <td>Create the initial version
 * </table>
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LCD_HW_CN91C4_H
#define __LCD_HW_CN91C4_H
/* Includes (#include "..\xxx\xx.h")------------------------------------------*/
// #include "DRV\BSW\TypeDefine.h"
#include <stdint.h>
/* Exported macros (#define)--------------------------------------------------*/

/* Exported types (typedef) --------------------------------------------------*/

typedef enum {
    E_CN_FR_72HZ, /**<帧频率：72hz（默认值）*/
    E_CN_FR_96HZ, /**<帧频率：96hz（省电模式1）*/
    E_CN_FR_49HZ, /**<帧频率：49hz（省电模式2）*/
    E_CN_FR_144HZ, /**<帧频率：144hz（省电模式3）*/
} CN91_SET_CMD_FR_E;

typedef enum {
    E_CN_SR_BIAS_050, /**<内部偏置电流：*0.5（省电模式1）*/
    E_CN_SR_BIAS_067, /**<内部偏置电流：*0.67（省电模式2）*/
    E_CN_SR_BIAS_100, /**<内部偏置电流：*1.0（正常模式，默认值）*/
    E_CN_SR_BIAS_180, /**<内部偏置电流：*1.8（高功率模式）*/
} CN91_SET_CMD_SR_E;

typedef enum {
    E_CN_EV_LEVEL_0, /**<LCD对比度0：（1.000*VLCD，默认值）*/
    E_CN_EV_LEVEL_1, /**<LCD对比度1：（0.975*VLCD）*/
    E_CN_EV_LEVEL_2, /**<LCD对比度2：（0.950*VLCD）*/
    E_CN_EV_LEVEL_3, /**<LCD对比度3：（0.925*VLCD）*/
    E_CN_EV_LEVEL_4, /**<LCD对比度4：（0.900*VLCD）*/
    E_CN_EV_LEVEL_5, /**<LCD对比度5：（0.875*VLCD）*/
    E_CN_EV_LEVEL_6, /**<LCD对比度6：（0.850*VLCD）*/
    E_CN_EV_LEVEL_7, /**<LCD对比度7：（0.825*VLCD）*/

    E_CN_EV_LEVEL_8, /**<LCD对比度8：（0.800*VLCD）*/
    E_CN_EV_LEVEL_9, /**<LCD对比度9：（0.775*VLCD）*/
    E_CN_EV_LEVEL_A, /**<LCD对比度A：（0.750*VLCD）*/
    E_CN_EV_LEVEL_B, /**<LCD对比度B：（0.725*VLCD）*/
    E_CN_EV_LEVEL_C, /**<LCD对比度C：（0.700*VLCD）*/
    E_CN_EV_LEVEL_D, /**<LCD对比度D：（0.675*VLCD）*/
    E_CN_EV_LEVEL_E, /**<LCD对比度E：（0.650*VLCD）*/
    E_CN_EV_LEVEL_F, /**<LCD对比度F：（0.625*VLCD）*/
} CN91_SET_CMD_EV_E;

typedef enum {
    E_CN_BF_SHORP_NOT, /**<显示闪烁频率：不闪烁（默认值）*/
    E_CN_BF_SHORP_030HZ, /**<显示闪烁频率：0.3hz*/
    E_CN_BF_SHORP_025HZ, /**<显示闪烁频率：0.25hz*/
    E_CN_BF_SHORP_200HZ, /**<显示闪烁频率：2hz*/
    E_CN_BF_SHORP_100HZ, /**<显示闪烁频率：1hz*/
} CN91_SET_CMD_BF_E;

typedef enum {
    E_LCD_BRIGHT_CLOSE,
    E_LCD_BRIGHT_OPEN,
} LCD_HW_BRIGHT_STA_E;

/* Exported function prototype declarations (extern)--------------------------*/

/** LCD初始化配置 */
extern void CDD_Lcd_Hw_Init(void);

/** LCD刷屏数据或控制命令更新（10ms调用周期） */
extern void CDD_Lcd_Hw_ScanData(void);

/** LCD显示内容数据设置 */
extern void CDD_Lcd_Hw_SetScanData(const uint8_t *Src, uint8_t Len);

/** 背光灯控制（或像素点状态设置）*/
extern void CDD_Lcd_Hw_SetBriSta(LCD_HW_BRIGHT_STA_E Sta);

/** LCD使能控制 */
extern void CDD_Lcd_Hw_SetEN(uint8_t enable);

/* Exported variable declarations (extern)------------------------------------*/

/* Exported constant declarations (extern const)------------------------------*/

#endif /* End __LED_H */
/* End of file ---------------------------------------------------------------*/
