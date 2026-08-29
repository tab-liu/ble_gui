/**
 ******************************************************************************
 * @file      lcd.h
 * @version   0.1
 * @author    Lixr
 * @date      2023/11/17
 * @brief     CfileFormat.c's header file
 * @par       History
 * <table>
 * <tr><th>Date       <th>Version <th>Author     <th>Description
 * <tr><td>2023/11/17 <td>1.0     <td>DL         <td>Create the initial version
 * </table>
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LCD_H
#define __LCD_H
/* Includes (#include "..\xxx\xx.h")------------------------------------------*/
// #include "DRV\BSW\TypeDefine.h"
#include <stdint.h>
/* Exported macros (#define)--------------------------------------------------*/

/* Exported types (typedef) --------------------------------------------------*/
typedef enum {
    E_ICO_MODE_CLOSE = 0, /**<LCD图标关闭*/
    E_ICO_MODE_OPEN, /**<LCD图标常亮*/
    E_ICO_MODE_SLOW_BLINK, /**<LCD图标慢闪（2hz）*/
    E_ICO_MODE_FAST_BLINK, /**<LCD图标快闪（1hz）*/

    E_ICO_MODE_SELF_BLINK1, /**<LCD图标自定义闪烁1（1s亮、9s灭）*/

    E_ICO_MODE_MAX,
} LCD_ICO_MODE_E;

typedef enum {
    E_LCD_PER_MODE_CONST, /**<SOC进度条常亮*/
    E_LCD_PER_MODE_CHG, /**<SOC进度条充电跑马模式（从当前百分比跑马到100）*/
    E_LCD_PER_MODE_HYS, /**<SOC滞回跑马（只闪烁下一格的百分比进度条）*/
} LCD_PER_MODE_E;

typedef enum {
    E_BRIGHT_CLOSE, /**<LCD背光灯关闭*/
    E_BRIGHT_OPEN, /**<LCD背光灯开启*/
} LCD_BRIGHT_MODE_E;

/**<LCD图标编号枚举（和硬件断码屏图标位置一一对应）*/
typedef enum {
    ICON_BATTERY_LV3 = 0, // T35 -- 电池3格
    ICON_BATTERY_LV4 = 1, // T34 -- 电池4格
    ICON_BATTERY_LV5 = 2, // T33 -- 电池5格
    ICON_BATTERY = 3, // T27 -- 电池图标
    ICON_BATTERY_LV2 = 4, // T36 -- 电池2格
    ICON_BATTERY_LV1 = 5, // T37 -- 电池1格

    ICON_AC_ONLINE = 11, // T31 -- 市电在线

    ICON_PV_IN = 19, // T30 -- PV接入图标

    ICON_OUTPUT_W = 24, // T29 -- OUTPUT W图标

    ICON_INV_CON = 48, // T32 -- 逆变器连接图标
    ICON_BLE_CON = 56, // T26 -- BLE连接图标
    ICON_INPUT_W = 64, // T28 -- INPUT W图标

    ICON_SOC_PER_40 = 72, // R4 -- 储能百分比40
    ICON_SOC_PER_50 = 73, // R5 -- 储能百分比50
    ICON_SOC_PER_60 = 74, // R6 -- 储能百分比60
    ICON_SOC_PER_70 = 75, // R7 -- 储能百分比70

    ICON_SOC_PER_100 = 77, // R10 -- 储能百分比100
    ICON_SOC_PER_90 = 78, // R9 -- 储能百分比90
    ICON_SOC_PER_80 = 79, // R8 -- 储能百分比80

    ICON_SAVE_MONEY_K = 87, // T25 -- 省钱参数单位K
    ICON_SAVE_MONEY_POINT = 95, // P4 -- 省钱参数小数点
    ICON_SAVE_MONEY = 96, // T24 -- 省钱参数符号

    ICON_SOC_PER = 111, // T22 -- SOC百分比符号
    ICON_SOC_NUM100 = 112, // T23 -- SOC百分比100的最高百位

    ICON_CHG_TIME_POINT = 140, // P3 -- 充放电时间小数点

    ICON_SOC_PER_FRAME = 144, // R0 -- SOC 跑马灯外框
    ICON_SOC_PER_30 = 145, // R3 -- SOC百分比30
    ICON_SOC_PER_20 = 146, // R2 -- SOC百分比20
    ICON_SOC_PER_10 = 147, // R1 -- SOC百分比10
    ICON_ECO_MODE = 148, // T18 -- ECO模式图标
    ICON_OUTPUT_MEMORY = 149, // T19 -- 输出记忆图标
    ICON_UPS_MODE = 150, // T20 -- 储能备电图标
    ICON_CHG_TIME_H = 151, // T21 -- 充放电时间H单位

    ICON_TIME_COLON = 160, // P1 -- 时间冒号

    ICON_TIME_AM = 168, // T3 -- 时间AM
    ICON_TIME_PM = 176, // T4 -- 时间PM

    ICON_AC_OFFLINE_CNT = 188, // T2 -- 市电离线次数溢出

    ICON_TEMP_C_F_1 = 200, // T11 -- 温度单位组装
    ICON_TEMP_C_F_2 = 201, // T10 -- 温度单位组装
    ICON_TEMP_C_F_3 = 202, // T9 -- 温度单位组装
    ICON_TEMP_C_F_4 = 203, // T8 -- 温度单位组装

    ICON_AC_OFFLINE = 206, // T1 -- 市电离线
    ICON_STORM_WARNING = 207, // T0 -- 风暴预警

    ICON_TEMP_POINT = 223, // P2 -- 温度显示小数点

    ICON_TEMP_100 = 227, // T7 -- 温度100

    ICON_HUMI_PER = 235, // T13 -- 湿度百分号
    ICON_TEMP_NEG = 238, // T6 -- 温度负号
    ICON_TEMP = 239, // T5 -- 湿度符号

    ICON_WARNING = 248, // T14 -- 警告符号
    ICON_SUPER_POWER = 249, // T15 -- 大力士
    ICON_FAST_CHARGE = 250, // T16 -- 快充模式
    ICON_QUIET_MODE = 251, // T17 -- 静音模式
    ICON_HUMI = 252, // T12 -- 湿度符号

    ICON_ID_MAX = 256,
} LCD_ICO_ID_E;

/**<LCD数码管编号（灵活根据实际增删）*/
typedef enum {
    /** E_NUM_1 - E_NUM_2 为 断电次数 区域数码管（从左往右）*/
    E_NUM_1,
    E_NUM_2,

    /** E_NUM_3 - E_NUM_6 为 时钟 区域数码管（从左往右）*/
    E_NUM_3, // 小时十位
    E_NUM_4, // 小时个位
    E_NUM_5, // 分钟十位
    E_NUM_6, // 分钟个位

    /** E_NUM_7 - E_NUM_9 为 温度 区域数码管（从左往右）*/
    E_NUM_7, // 温度十位
    E_NUM_8, // 温度个位
    E_NUM_9, // 温度小数

    /** E_NUM_9 - E_NUM_10 为 湿度 区域数码管（从左往右）*/
    E_NUM_10, // 湿度十位
    E_NUM_11, // 湿度个位

    /** E_NUM_12 - E_NUM_14 为 省钱数 区域数码管（从左往右）*/
    E_NUM_12, // 省钱数十位
    E_NUM_13, // 省钱数个位
    E_NUM_14, // 省钱数小数

    /** E_NUM_15 - E_NUM_16 为 SOC 区域数码管（从左往右）*/
    E_NUM_15, // SOC十位
    E_NUM_16, // SOC个位

    /** E_NUM_17 - E_NUM_19 为 充电时间 区域数码管（从左往右）*/
    E_NUM_17, // 充电时间十位
    E_NUM_18, // 充电时间个位
    E_NUM_19, // 充电时间小数

    /** E_NUM_20 - E_NUM_23 为 输入功率 区域数码管（从左往右）*/
    E_NUM_20, // 输入功率千位
    E_NUM_21, // 输入功率百位
    E_NUM_22, // 输入功率十位
    E_NUM_23, // 输入功率个位

    /** E_NUM_24 - E_NUM_27 为 输出功率 区域数码管（从左往右）*/
    E_NUM_24, // 输出功率千位
    E_NUM_25, // 输出功率百位
    E_NUM_26, // 输出功率十位
    E_NUM_27, // 输出功率个位

    /*...*/
    E_NUM_MAX,
} LCD_NUMBER_ID_E;

/**<LCD数码管显示段码表编号*/
typedef enum {
    E_LCD_SHOW_0,
    E_LCD_SHOW_1,
    E_LCD_SHOW_2,
    E_LCD_SHOW_3,
    E_LCD_SHOW_4,
    E_LCD_SHOW_5,
    E_LCD_SHOW_6,
    E_LCD_SHOW_7,
    E_LCD_SHOW_8,
    E_LCD_SHOW_9,

    E_LCD_SHOW_E,
    E_LCD_SHOW_P,
    E_LCD_SHOW_S,
    E_LCD_SHOW_N,
    E_LCD_SHOW_H,
    E_LCD_SHOW_I,
    E_LCD_SHOW_F,
} LCD_NUM_SHOW_E;

/**<开机动画选项*/
typedef enum {
    E_LCD_MOVIE_IDEL,
    E_LCD_MOVIE_INIT,
    E_LCD_MOVIE_OFF,
} LCD_MOVIE_SEL_E;

/* Exported function prototype declarations (extern)--------------------------*/

extern void CDD_Lcd_Init(void);

/**
 *（考虑有些工况，在上电初始化时间可能会长，如大量读取EEPROM，
 * 可将Lcd_Run放中断函数中处理）
 */

/** 运行控制总进程，固定10ms周期（考虑到需和LED闪烁同频，需要和Led_Run放一起） */
extern void CDD_Lcd_Run(void);

/** LCD清屏 */
extern void CDD_Lcd_Clear(void);

/** 数据内容显示：（亮、灭、慢闪、快闪）*/
extern void CDD_Lcd_SetNumberMode(LCD_NUMBER_ID_E NumIndex,
                                  LCD_NUM_SHOW_E Data,
                                  LCD_ICO_MODE_E Mode);

/** 独立图标工作模式设定：（亮、灭、慢闪、快闪） */
extern void CDD_Lcd_SetSigIcoMode(LCD_ICO_ID_E IcoIndex, LCD_ICO_MODE_E Mode);

/** LCD跑马进度条设置：（跑马、常显） */
extern void CDD_Lcd_SetSocPerMode(uint8_t SocPer, LCD_PER_MODE_E Mode);

/** LCD开关机动画设置：（开机、关机） */
extern void CDD_Lcd_SetMovie(LCD_MOVIE_SEL_E MovieSel);

/** LCD开关机动画完成标志 */
extern uint8_t CDD_Lcd_MovieFinishIs(void);

/** 背光灯状态设置 */
extern void CDD_Lcd_SetBrightSta(LCD_BRIGHT_MODE_E BriSta);

/** 中断回调用函数，建议50us周期（底层驱动） */
void CDD_Lcd_TimIsrCallback(void);

/* Exported variable declarations (extern)------------------------------------*/

/* Exported constant declarations (extern const)------------------------------*/

#endif /* End F_CFILEFORMAT_H */
/* End of file ---------------------------------------------------------------*/
