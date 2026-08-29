/**
 ******************************************************************************
 * @file      Lcd.c
 * @version   0.1
 * @author    Lixr
 * @date      2023/11/17
 * @brief     lcd屏显示，适配CN91C4驱动
 * @par       History
 * <table>
 * <tr><th>Date       <th>Version <th>Author     <th>Description
 * <tr><td>2023/11/17 <td>1.0     <td>DL         <td>Create the initial version
 * </table>
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 ******************************************************************************
 */

/* Private includes (#include "..\xxx\xx.h")----------------------------------*/
#include <string.h>
#include <stdint.h>
// #include "DRV\Lib\FunLib\AssertMsg.h"
// #include "DRV\BSW\GPIO\GPIO_interface.h"
// #include "DRV\CDD\Lcd\Lcd_Hw_ht1621.h"
// #include "DRV\CDD\Lcd\Lcd.h"

#include "ls_dbg.h"
#include "Lcd_Hw_cn91c4.h"
#include "Lcd.h"

/* Private macros (#define)---------------------------------------------------*/

/** 慢闪、快闪设定（尽量不动统一固定）*/
#define LCD_RUN_CYC 10 /**<Lcd最小时基单位，根据Lcd_Run调用周期而定（单位ms）*/
#define LCD_TIM_CYC 50 /**<Lcd底层驱动中断时基，根据Lcd_TimIsrCallback调用周期而定（单位us）*/

#define DEF_LCD_SLOW_BLINK_TIME (500 / LCD_RUN_CYC) /**<慢闪周期*/

#define DEF_LCD_FAST_BLINK_TIME (250 / LCD_RUN_CYC) /**<快闪周期*/

#define LCD_CHG_PER_BLINK_TIEM (300 / LCD_RUN_CYC) /**<充电跑马闪烁时间*/

#define LCD_DATA_LEN 32 /**< LCD屏显示扫描结构体长度 64个seg */

#define LCD_REG_LEN sizeof(TubeRegTable)

/** 开关机动画时间效果控制 */
#define MOVIE_ON_SCHE1_TIME (100 / LCD_RUN_CYC)
#define MOVIE_ON_SCHE2_TIME (40 / LCD_RUN_CYC)
#define MOVIE_ON_SCHE3_TIME (80 / LCD_RUN_CYC)
#define MOVIE_ON_SCHE4_TIME (40 / LCD_RUN_CYC)
#define MOVIE_ON_SCHE5_TIME (20 / LCD_RUN_CYC)

#define MOVIE_OFF_SCHE1_TIME (50 / LCD_RUN_CYC)
#define MOVIE_OFF_SCHE2_TIME (1000 / LCD_RUN_CYC)
#define MOVIE_OFF_SCHE3_TIME (50 / LCD_RUN_CYC)

/** 数字段码模式位域设置*/
#define TUBE_MODE_SEG(x, y) LcdIcoModeSet.DataFrame.Number_##x##y
#define TUBEA_MODE_SEG(x) TUBE_MODE_SEG(x, A)
#define TUBEB_MODE_SEG(x) TUBE_MODE_SEG(x, B)
#define TUBEC_MODE_SEG(x) TUBE_MODE_SEG(x, C)
#define TUBED_MODE_SEG(x) TUBE_MODE_SEG(x, D)
#define TUBEE_MODE_SEG(x) TUBE_MODE_SEG(x, E)
#define TUBEF_MODE_SEG(x) TUBE_MODE_SEG(x, F)
#define TUBEG_MODE_SEG(x) TUBE_MODE_SEG(x, G)

/** 数字段码状态位域设置*/
#define TUBE_STA_SEG(x, y) LcdScanData.DataFrame.Number_##x##y
#define TUBEA_STA_SEG(x) TUBE_STA_SEG(x, A)
#define TUBEB_STA_SEG(x) TUBE_STA_SEG(x, B)
#define TUBEC_STA_SEG(x) TUBE_STA_SEG(x, C)
#define TUBED_STA_SEG(x) TUBE_STA_SEG(x, D)
#define TUBEE_STA_SEG(x) TUBE_STA_SEG(x, E)
#define TUBEF_STA_SEG(x) TUBE_STA_SEG(x, F)
#define TUBEG_STA_SEG(x) TUBE_STA_SEG(x, G)

/* Private types (typedef) ---------------------------------------------------*/
#ifdef CCS
#else
#pragma pack(push, 1) /*内存单字节对齐*/
#endif

/**<LCD屏数字段码枚举*/
typedef enum {
    E_LCD_TUBE_A,
    E_LCD_TUBE_B,
    E_LCD_TUBE_C,
    E_LCD_TUBE_D,
    E_LCD_TUBE_E,
    E_LCD_TUBE_F,
    E_LCD_TUBE_G,
} LCD_TUBE_REG_E;

typedef enum {
    E_LCO_STA_CLOSE,
    E_LCO_STA_OPEN,
} LCD_ICO_STA_E;

/**<LCD断码屏显示内容结构体*/
typedef union {
    volatile uint8_t Data[LCD_DATA_LEN];

    struct {
        /* 0 */
        uint8_t Battery_lv3 : 1; // T35 -- 电池3格
        uint8_t Battery_lv4 : 1; // T34 -- 电池4格
        uint8_t Battery_lv5 : 1; // T33 -- 电池5格
        uint8_t Battery : 1; // T27 -- 电池图标

        uint8_t Battery_lv2 : 1; // T36 -- 电池2格
        uint8_t Battery_lv1 : 1; // T37 -- 电池1格
        uint8_t Null_1 : 1;
        uint8_t Null_2 : 1;

        /* 1 */
        uint8_t Number_27F : 1;
        uint8_t Number_27G : 1;
        uint8_t Number_27E : 1;
        uint8_t AC_Online : 1; // T31 -- 市电在线

        uint8_t Number_27A : 1;
        uint8_t Number_27B : 1;
        uint8_t Number_27C : 1;
        uint8_t Number_27D : 1;

        /* 2 */
        uint8_t Number_26F : 1;
        uint8_t Number_26G : 1;
        uint8_t Number_26E : 1;
        uint8_t PV_Input : 1; // T30 -- PV接入图标

        uint8_t Number_26A : 1;
        uint8_t Number_26B : 1;
        uint8_t Number_26C : 1;
        uint8_t Number_26D : 1;

        /* 3 */
        uint8_t Output_W : 1; // T29 -- OUTPUT W图标
        uint8_t Number_25F : 1;
        uint8_t Number_25G : 1;
        uint8_t Number_25E : 1;

        uint8_t Number_25A : 1;
        uint8_t Number_25B : 1;
        uint8_t Number_25C : 1;
        uint8_t Number_25D : 1;

        /* 4 */
        uint8_t Null_3 : 1;
        uint8_t Number_24F : 1;
        uint8_t Number_24G : 1;
        uint8_t Number_24E : 1;

        uint8_t Number_24A : 1;
        uint8_t Number_24B : 1;
        uint8_t Number_24C : 1;
        uint8_t Number_24D : 1;

        /* 5 */
        uint8_t Null_4 : 1;
        uint8_t Number_23F : 1;
        uint8_t Number_23G : 1;
        uint8_t Number_23E : 1;

        uint8_t Number_23A : 1;
        uint8_t Number_23B : 1;
        uint8_t Number_23C : 1;
        uint8_t Number_23D : 1;

        /* 6 */
        uint8_t INV_CONN : 1; // T32 -- 逆变器连接图标
        uint8_t Number_22F : 1;
        uint8_t Number_22G : 1;
        uint8_t Number_22E : 1;

        uint8_t Number_22A : 1;
        uint8_t Number_22B : 1;
        uint8_t Number_22C : 1;
        uint8_t Number_22D : 1;

        /* 7 */
        uint8_t BLE_CONN : 1; // T26 -- BLE连接图标
        uint8_t Number_21F : 1;
        uint8_t Number_21G : 1;
        uint8_t Number_21E : 1;

        uint8_t Number_21A : 1;
        uint8_t Number_21B : 1;
        uint8_t Number_21C : 1;
        uint8_t Number_21D : 1;

        /* 8 */
        uint8_t Input_W : 1; // T28 -- INPUT W图标
        uint8_t Number_20F : 1;
        uint8_t Number_20G : 1;
        uint8_t Number_20E : 1;

        uint8_t Number_20A : 1;
        uint8_t Number_20B : 1;
        uint8_t Number_20C : 1;
        uint8_t Number_20D : 1;

        /* 9 */
        uint8_t SocPer40 : 1; // R4 -- 储能百分比40
        uint8_t SocPer50 : 1; // R5 -- 储能百分比50
        uint8_t SocPer60 : 1; // R6 -- 储能百分比60
        uint8_t SocPer70 : 1; // R7 -- 储能百分比70

        uint8_t Null_5 : 1;
        uint8_t SocPer100 : 1; // R10 -- 储能百分比100
        uint8_t SocPer90 : 1; // R9 -- 储能百分比90
        uint8_t SocPer80 : 1; // R8 -- 储能百分比80

        /* 10 */
        uint8_t Number_14A : 1;
        uint8_t Number_14F : 1;
        uint8_t Number_14E : 1;
        uint8_t Number_14D : 1;

        uint8_t Number_14B : 1;
        uint8_t Number_14G : 1;
        uint8_t Number_14C : 1;
        uint8_t Save_Money_K : 1; // T25 -- 省钱参数单位K

        /* 11 */
        uint8_t Number_13A : 1;
        uint8_t Number_13F : 1;
        uint8_t Number_13E : 1;
        uint8_t Number_13D : 1;

        uint8_t Number_13B : 1;
        uint8_t Number_13G : 1;
        uint8_t Number_13C : 1;
        uint8_t Save_Money_Point : 1; // P4 -- 省钱参数小数点

        /* 12 */
        uint8_t Save_Money : 1; // T24 -- 省钱参数符号
        uint8_t Number_12F : 1;
        uint8_t Number_12G : 1;
        uint8_t Number_12E : 1;

        uint8_t Number_12A : 1;
        uint8_t Number_12B : 1;
        uint8_t Number_12C : 1;
        uint8_t Number_12D : 1;

        /* 13 */
        uint8_t Number_16A : 1;
        uint8_t Number_16F : 1;
        uint8_t Number_16E : 1;
        uint8_t Number_16D : 1;

        uint8_t Number_16B : 1;
        uint8_t Number_16G : 1;
        uint8_t Number_16C : 1;
        uint8_t Soc_Per : 1; // T22 -- SOC百分比符号

        /* 14 */
        uint8_t Soc_Num100 : 1; // T23 -- SOC百分比100的最高百位
        uint8_t Number_15F : 1;
        uint8_t Number_15G : 1;
        uint8_t Number_15E : 1;

        uint8_t Number_15A : 1;
        uint8_t Number_15B : 1;
        uint8_t Number_15C : 1;
        uint8_t Number_15D : 1;

        /* 15 */
        uint8_t Number_17D : 1;
        uint8_t Number_17C : 1;
        uint8_t Number_17B : 1;
        uint8_t Number_17A : 1;

        uint8_t Null_6 : 1;
        uint8_t Number_17E : 1;
        uint8_t Number_17G : 1;
        uint8_t Number_17F : 1;

        /* 16 */
        uint8_t Number_18D : 1;
        uint8_t Number_18C : 1;
        uint8_t Number_18B : 1;
        uint8_t Number_18A : 1;

        uint8_t Null_7 : 1;
        uint8_t Number_18E : 1;
        uint8_t Number_18G : 1;
        uint8_t Number_18F : 1;

        /* 17 */
        uint8_t Number_19D : 1;
        uint8_t Number_19C : 1;
        uint8_t Number_19B : 1;
        uint8_t Number_19A : 1;

        uint8_t ChgTime_Point : 1; // P3 -- 充放电时间小数点
        uint8_t Number_19E : 1;
        uint8_t Number_19G : 1;
        uint8_t Number_19F : 1;

        /* 18 */
        uint8_t Soc_Per_Frame : 1; // R0 -- SOC 跑马灯外框
        uint8_t SocPer30 : 1; // R3 -- SOC百分比30
        uint8_t SocPer20 : 1; // R2 -- SOC百分比20
        uint8_t SocPer10 : 1; // R1 -- SOC百分比10

        uint8_t Eco_Mode : 1; // T18 -- ECO模式图标
        uint8_t Output_Memory : 1; // T19 -- 输出记忆图标
        uint8_t UPS_Mode : 1; // T20 -- 储能备电图标
        uint8_t Chg_Time_H : 1; // T21 -- 充放电时间小时H单位

        /* 19 */
        uint8_t Null_8 : 1;
        uint8_t Number_6F : 1;
        uint8_t Number_6G : 1;
        uint8_t Number_6E : 1;

        uint8_t Number_6A : 1;
        uint8_t Number_6B : 1;
        uint8_t Number_6C : 1;
        uint8_t Number_6D : 1;

        /* 20 */
        uint8_t Time_Colon : 1; // P1 -- 时间冒号
        uint8_t Number_5F : 1;
        uint8_t Number_5G : 1;
        uint8_t Number_5E : 1;

        uint8_t Number_5A : 1;
        uint8_t Number_5B : 1;
        uint8_t Number_5C : 1;
        uint8_t Number_5D : 1;

        /* 21 */
        uint8_t Time_AM : 1; // T3 -- 时间AM
        uint8_t Number_4F : 1;
        uint8_t Number_4G : 1;
        uint8_t Number_4E : 1;

        uint8_t Number_4A : 1;
        uint8_t Number_4B : 1;
        uint8_t Number_4C : 1;
        uint8_t Number_4D : 1;

        /* 22 */
        uint8_t Time_PM : 1; // T4 -- 时间PM
        uint8_t Number_3F : 1;
        uint8_t Number_3G : 1;
        uint8_t Number_3E : 1;

        uint8_t Number_3A : 1;
        uint8_t Number_3B : 1;
        uint8_t Number_3C : 1;
        uint8_t Number_3D : 1;

        /* 23 */
        uint8_t Number_2A : 1;
        uint8_t Number_2F : 1;
        uint8_t Number_2E : 1;
        uint8_t Number_2D : 1;

        uint8_t AC_Offline_Cnt : 1; // T2 -- 市电离线次数溢出
        uint8_t Number_2B : 1;
        uint8_t Number_2G : 1;
        uint8_t Number_2C : 1;

        /* 24 */
        uint8_t Number_1A : 1;
        uint8_t Number_1F : 1;
        uint8_t Number_1E : 1;
        uint8_t Number_1D : 1;

        uint8_t Null_9 : 1;
        uint8_t Number_1B : 1;
        uint8_t Number_1G : 1;
        uint8_t Number_1C : 1;

        /* 25 */
        uint8_t Temp_C_F_1 : 1; // T11 -- 温度单位组装
        uint8_t Temp_C_F_2 : 1; // T10 -- 温度单位组装
        uint8_t Temp_C_F_3 : 1; // T9 -- 温度单位组装
        uint8_t Temp_C_F_4 : 1; // T8 -- 温度单位组装

        uint8_t Null_10 : 1;
        uint8_t Null_11 : 1;
        uint8_t AC_Offline : 1; // T1 -- 市电离线
        uint8_t Storm_Warning : 1; // T0 -- 风暴预警

        /* 26 */
        uint8_t Number_9A : 1;
        uint8_t Number_9F : 1;
        uint8_t Number_9E : 1;
        uint8_t Number_9D : 1;

        uint8_t Null_12 : 1;
        uint8_t Number_9B : 1;
        uint8_t Number_9G : 1;
        uint8_t Number_9C : 1;

        /* 27 */
        uint8_t Number_8A : 1;
        uint8_t Number_8F : 1;
        uint8_t Number_8E : 1;
        uint8_t Number_8D : 1;

        uint8_t Number_8B : 1;
        uint8_t Number_8G : 1;
        uint8_t Number_8C : 1;
        uint8_t Temp_Point : 1; // P2 -- 温度显示小数点

        /* 28 */
        uint8_t Number_7F : 1;
        uint8_t Number_7G : 1;
        uint8_t Number_7E : 1;
        uint8_t Temp_100 : 1; // T7 -- 温度100

        uint8_t Number_7A : 1;
        uint8_t Number_7B : 1;
        uint8_t Number_7C : 1;
        uint8_t Number_7D : 1;

        /* 29 */
        uint8_t Number_11B : 1;
        uint8_t Number_11G : 1;
        uint8_t Number_11C : 1;
        uint8_t Humi_Per : 1; // T13 -- 湿度百分号

        uint8_t Null_13 : 1;
        uint8_t Null_14 : 1;
        uint8_t Temp_Neg : 1; // T6 -- 温度负号
        uint8_t Temp : 1; // T5 -- 湿度符号

        /* 30 */
        uint8_t Number_10A : 1;
        uint8_t Number_10B : 1;
        uint8_t Number_10C : 1;
        uint8_t Number_10D : 1;

        uint8_t Number_11A : 1;
        uint8_t Number_11F : 1;
        uint8_t Number_11E : 1;
        uint8_t Number_11D : 1;

        /* 31 */
        uint8_t Warning : 1; // T14 -- 警告符号
        uint8_t Super_Power : 1; // T15 -- 大力士
        uint8_t Fast_Charge : 1; // T16 -- 快充模式
        uint8_t Quiet_Mode : 1; // T17 -- 静音模式

        uint8_t Humi : 1; // T12 -- 湿度符号
        uint8_t Number_10F : 1;
        uint8_t Number_10G : 1;
        uint8_t Number_10E : 1;
    } DataFrame;

} LCD_SCAN_DATA_U;

/**<LCD断码屏显示图标控制结构体：4bit长度模式*/
typedef union {
    volatile uint32_t Data[LCD_DATA_LEN];
    struct {
        /* 0 */
        uint8_t Battery_lv3 : 4; // T35 -- 电池3格
        uint8_t Battery_lv4 : 4; // T34 -- 电池4格
        uint8_t Battery_lv5 : 4; // T33 -- 电池5格
        uint8_t Battery : 4; // T27 -- 电池图标

        uint8_t Battery_lv2 : 4; // T36 -- 电池2格
        uint8_t Battery_lv1 : 4; // T37 -- 电池1格
        uint8_t Null_1 : 4;
        uint8_t Null_2 : 4;

        /* 1 */
        uint8_t Number_27F : 4;
        uint8_t Number_27G : 4;
        uint8_t Number_27E : 4;
        uint8_t AC_Online : 4; // T31 -- 市电在线

        uint8_t Number_27A : 4;
        uint8_t Number_27B : 4;
        uint8_t Number_27C : 4;
        uint8_t Number_27D : 4;

        /* 2 */
        uint8_t Number_26F : 4;
        uint8_t Number_26G : 4;
        uint8_t Number_26E : 4;
        uint8_t PV_Input : 4; // T30 -- PV接入图标

        uint8_t Number_26A : 4;
        uint8_t Number_26B : 4;
        uint8_t Number_26C : 4;
        uint8_t Number_26D : 4;

        /* 3 */
        uint8_t Output_W : 4; // T29 -- OUTPUT W图标
        uint8_t Number_25F : 4;
        uint8_t Number_25G : 4;
        uint8_t Number_25E : 4;

        uint8_t Number_25A : 4;
        uint8_t Number_25B : 4;
        uint8_t Number_25C : 4;
        uint8_t Number_25D : 4;

        /* 4 */
        uint8_t Null_3 : 4;
        uint8_t Number_24F : 4;
        uint8_t Number_24G : 4;
        uint8_t Number_24E : 4;

        uint8_t Number_24A : 4;
        uint8_t Number_24B : 4;
        uint8_t Number_24C : 4;
        uint8_t Number_24D : 4;

        /* 5 */
        uint8_t Null_4 : 4;
        uint8_t Number_23F : 4;
        uint8_t Number_23G : 4;
        uint8_t Number_23E : 4;

        uint8_t Number_23A : 4;
        uint8_t Number_23B : 4;
        uint8_t Number_23C : 4;
        uint8_t Number_23D : 4;

        /* 6 */
        uint8_t INV_CONN : 4; // T32 -- 逆变器连接图标
        uint8_t Number_22F : 4;
        uint8_t Number_22G : 4;
        uint8_t Number_22E : 4;

        uint8_t Number_22A : 4;
        uint8_t Number_22B : 4;
        uint8_t Number_22C : 4;
        uint8_t Number_22D : 4;

        /* 7 */
        uint8_t BLE_CONN : 4; // T26 -- BLE连接图标
        uint8_t Number_21F : 4;
        uint8_t Number_21G : 4;
        uint8_t Number_21E : 4;

        uint8_t Number_21A : 4;
        uint8_t Number_21B : 4;
        uint8_t Number_21C : 4;
        uint8_t Number_21D : 4;

        /* 8 */
        uint8_t Input_W : 4; // T28 -- INPUT W图标
        uint8_t Number_20F : 4;
        uint8_t Number_20G : 4;
        uint8_t Number_20E : 4;

        uint8_t Number_20A : 4;
        uint8_t Number_20B : 4;
        uint8_t Number_20C : 4;
        uint8_t Number_20D : 4;

        /* 9 */
        uint8_t SocPer40 : 4; // R4 -- 储能百分比40
        uint8_t SocPer50 : 4; // R5 -- 储能百分比50
        uint8_t SocPer60 : 4; // R6 -- 储能百分比60
        uint8_t SocPer70 : 4; // R7 -- 储能百分比70

        uint8_t Null_5 : 4;
        uint8_t SocPer100 : 4; // R10 -- 储能百分比100
        uint8_t SocPer90 : 4; // R9 -- 储能百分比90
        uint8_t SocPer80 : 4; // R8 -- 储能百分比80

        /* 10 */
        uint8_t Number_14A : 4;
        uint8_t Number_14F : 4;
        uint8_t Number_14E : 4;
        uint8_t Number_14D : 4;

        uint8_t Number_14B : 4;
        uint8_t Number_14G : 4;
        uint8_t Number_14C : 4;
        uint8_t Save_Money_K : 4; // T25 -- 省钱参数单位K

        /* 11 */
        uint8_t Number_13A : 4;
        uint8_t Number_13F : 4;
        uint8_t Number_13E : 4;
        uint8_t Number_13D : 4;

        uint8_t Number_13B : 4;
        uint8_t Number_13G : 4;
        uint8_t Number_13C : 4;
        uint8_t Save_Money_Point : 4; // P4 -- 省钱参数小数点

        /* 12 */
        uint8_t Save_Money : 4; // T24 -- 省钱参数符号
        uint8_t Number_12F : 4;
        uint8_t Number_12G : 4;
        uint8_t Number_12E : 4;

        uint8_t Number_12A : 4;
        uint8_t Number_12B : 4;
        uint8_t Number_12C : 4;
        uint8_t Number_12D : 4;

        /* 13 */
        uint8_t Number_16A : 4;
        uint8_t Number_16F : 4;
        uint8_t Number_16E : 4;
        uint8_t Number_16D : 4;

        uint8_t Number_16B : 4;
        uint8_t Number_16G : 4;
        uint8_t Number_16C : 4;
        uint8_t Soc_Per : 4; // T22 -- SOC百分比符号

        /* 14 */
        uint8_t Soc_Num100 : 4; // T23 -- SOC百分比100的最高百位
        uint8_t Number_15F : 4;
        uint8_t Number_15G : 4;
        uint8_t Number_15E : 4;

        uint8_t Number_15A : 4;
        uint8_t Number_15B : 4;
        uint8_t Number_15C : 4;
        uint8_t Number_15D : 4;

        /* 15 */
        uint8_t Number_17D : 4;
        uint8_t Number_17C : 4;
        uint8_t Number_17B : 4;
        uint8_t Number_17A : 4;

        uint8_t Null_6 : 4;
        uint8_t Number_17E : 4;
        uint8_t Number_17G : 4;
        uint8_t Number_17F : 4;

        /* 16 */
        uint8_t Number_18D : 4;
        uint8_t Number_18C : 4;
        uint8_t Number_18B : 4;
        uint8_t Number_18A : 4;

        uint8_t Null_7 : 4;
        uint8_t Number_18E : 4;
        uint8_t Number_18G : 4;
        uint8_t Number_18F : 4;

        /* 17 */
        uint8_t Number_19D : 4;
        uint8_t Number_19C : 4;
        uint8_t Number_19B : 4;
        uint8_t Number_19A : 4;

        uint8_t ChgTime_Point : 4; // P3 -- 充放电时间小数点
        uint8_t Number_19E : 4;
        uint8_t Number_19G : 4;
        uint8_t Number_19F : 4;

        /* 18 */
        uint8_t Soc_Per_Frame : 4; // R0 -- SOC 跑马灯外框
        uint8_t SocPer30 : 4; // R3 -- SOC百分比30
        uint8_t SocPer20 : 4; // R2 -- SOC百分比20
        uint8_t SocPer10 : 4; // R1 -- SOC百分比10

        uint8_t Eco_Mode : 4; // T18 -- ECO模式图标
        uint8_t Output_Memory : 4; // T19 -- 输出记忆图标
        uint8_t UPS_Mode : 4; // T20 -- 储能备电图标
        uint8_t Chg_Time_H : 4; // T21 -- 充放电时间小时H单位

        /* 19 */
        uint8_t Null_8 : 4;
        uint8_t Number_6F : 4;
        uint8_t Number_6G : 4;
        uint8_t Number_6E : 4;

        uint8_t Number_6A : 4;
        uint8_t Number_6B : 4;
        uint8_t Number_6C : 4;
        uint8_t Number_6D : 4;

        /* 20 */
        uint8_t Time_Colon : 4; // P1 -- 时间冒号
        uint8_t Number_5F : 4;
        uint8_t Number_5G : 4;
        uint8_t Number_5E : 4;

        uint8_t Number_5A : 4;
        uint8_t Number_5B : 4;
        uint8_t Number_5C : 4;
        uint8_t Number_5D : 4;

        /* 21 */
        uint8_t Time_AM : 4; // T3 -- 时间AM
        uint8_t Number_4F : 4;
        uint8_t Number_4G : 4;
        uint8_t Number_4E : 4;

        uint8_t Number_4A : 4;
        uint8_t Number_4B : 4;
        uint8_t Number_4C : 4;
        uint8_t Number_4D : 4;

        /* 22 */
        uint8_t Time_PM : 4; // T4 -- 时间PM
        uint8_t Number_3F : 4;
        uint8_t Number_3G : 4;
        uint8_t Number_3E : 4;

        uint8_t Number_3A : 4;
        uint8_t Number_3B : 4;
        uint8_t Number_3C : 4;
        uint8_t Number_3D : 4;

        /* 23 */
        uint8_t Number_2A : 4;
        uint8_t Number_2F : 4;
        uint8_t Number_2E : 4;
        uint8_t Number_2D : 4;

        uint8_t AC_Offline_Cnt : 4; // T2 -- 市电离线次数溢出
        uint8_t Number_2B : 4;
        uint8_t Number_2G : 4;
        uint8_t Number_2C : 4;

        /* 24 */
        uint8_t Number_1A : 4;
        uint8_t Number_1F : 4;
        uint8_t Number_1E : 4;
        uint8_t Number_1D : 4;

        uint8_t Null_9 : 4;
        uint8_t Number_1B : 4;
        uint8_t Number_1G : 4;
        uint8_t Number_1C : 4;

        /* 25 */
        uint8_t Temp_C_F_1 : 4; // T11 -- 温度单位组装
        uint8_t Temp_C_F_2 : 4; // T10 -- 温度单位组装
        uint8_t Temp_C_F_3 : 4; // T9 -- 温度单位组装
        uint8_t Temp_C_F_4 : 4; // T8 -- 温度单位组装

        uint8_t Null_10 : 4;
        uint8_t Null_11 : 4;
        uint8_t AC_Offline : 4; // T1 -- 市电离线
        uint8_t Storm_Warning : 4; // T0 -- 风暴预警

        /* 26 */
        uint8_t Number_9A : 4;
        uint8_t Number_9F : 4;
        uint8_t Number_9E : 4;
        uint8_t Number_9D : 4;

        uint8_t Null_12 : 4;
        uint8_t Number_9B : 4;
        uint8_t Number_9G : 4;
        uint8_t Number_9C : 4;

        /* 27 */
        uint8_t Number_8A : 4;
        uint8_t Number_8F : 4;
        uint8_t Number_8E : 4;
        uint8_t Number_8D : 4;

        uint8_t Number_8B : 4;
        uint8_t Number_8G : 4;
        uint8_t Number_8C : 4;
        uint8_t Temp_Point : 4; // P2 -- 温度显示小数点

        /* 28 */
        uint8_t Number_7F : 4;
        uint8_t Number_7G : 4;
        uint8_t Number_7E : 4;
        uint8_t Temp_100 : 4; // T7 -- 温度100

        uint8_t Number_7A : 4;
        uint8_t Number_7B : 4;
        uint8_t Number_7C : 4;
        uint8_t Number_7D : 4;

        /* 29 */
        uint8_t Number_11B : 4;
        uint8_t Number_11G : 4;
        uint8_t Number_11C : 4;
        uint8_t Humi_Per : 4; // T13 -- 湿度百分号

        uint8_t Null_13 : 4;
        uint8_t Null_14 : 4;
        uint8_t Temp_Neg : 4; // T6 -- 温度负号
        uint8_t Temp : 4; // T5 -- 湿度符号

        /* 30 */
        uint8_t Number_10A : 4;
        uint8_t Number_10B : 4;
        uint8_t Number_10C : 4;
        uint8_t Number_10D : 4;

        uint8_t Number_11A : 4;
        uint8_t Number_11F : 4;
        uint8_t Number_11E : 4;
        uint8_t Number_11D : 4;

        /* 31 */
        uint8_t Warning : 4; // T14 -- 警告符号
        uint8_t Super_Power : 4; // T15 -- 大力士
        uint8_t Fast_Charge : 4; // T16 -- 快充模式
        uint8_t Quiet_Mode : 4; // T17 -- 静音模式

        uint8_t Humi : 4; // T12 -- 湿度符号
        uint8_t Number_10F : 4;
        uint8_t Number_10G : 4;
        uint8_t Number_10E : 4;
    } DataFrame;

} LCD_ICO_MODE_U;

typedef struct {
    LCD_PER_MODE_E Mode;
    uint8_t SocValue;

    int16_t TimeCnt;
    uint8_t LastBar;
    uint8_t CurrBar;

} LCD_SOCPER_CTRL_T;

typedef struct {
    uint8_t LcdSta;
    int16_t KeepTime;
} LCD_SIG_ACTION_T;

typedef struct {
    const LCD_SIG_ACTION_T *ActPtr; /**<动作结构体数组指针*/
    LCD_SIG_ACTION_T CurrAct; /**<当前动作*/
    uint8_t ActLen; /**<动作个数*/
    uint8_t Index; /**<动作下标*/
} LCD_SELF_DEF_BLINK_T;

typedef struct {
    LCD_MOVIE_SEL_E MovieSel;
    LCD_MOVIE_SEL_E LastMovieSel;

    uint8_t Step;

} LCD_MOVIE_CTRL_T;

#ifdef CCS
#else
#pragma pack(pop)
#endif

/* Private function prototype declarations------------------------------------*/

/* Private variable definitions-----------------------------------------------*/

volatile static LCD_SCAN_DATA_U LcdScanData = {0}; /**<LCD数据显示结构体*/

volatile static LCD_ICO_MODE_U LcdIcoModeSet = {0}; /**<LCD单图标工作模式控制*/

static LCD_SOCPER_CTRL_T LcdSocPerCtrl; /**<SOC进度条控制*/

static LCD_MOVIE_CTRL_T LcdMovieCtrl; /**<动画控制*/

static LCD_SELF_DEF_BLINK_T LcdBlinkMode[E_ICO_MODE_MAX]; /**<闪烁模式动作数组*/

/* Exported variable definitions----------------------------------------------*/

/* Private constant definitions (const)---------------------------------------*/

static const uint8_t TubeRegTable[] = /**<段码顺序为ABCDEFG的段码表*/
    {
        0xFC,
        0x60,
        0xDA,
        0xF2,
        0x66,
        0xB6,
        0xBE,
        0xE0,
        0xFE,
        0xF6, /**<"0" - "9"段码*/

        0x9E,
        0xCE,
        0xB6,
        0xEC,
        0x6E,
        0x60,
        0x8E, /**<"E"、"P"、"S"、"N"、"H"、"I"、"F"*/
};

/* Exported constant definitions (const)--------------------------------------*/

static const LCD_SIG_ACTION_T RegCloseAction[] = /**<长灭动作数组*/
    {
        {0, DEF_LCD_SLOW_BLINK_TIME},
};

static const LCD_SIG_ACTION_T RegOpenAction[] = /**<长亮动作数组*/
    {
        {1, DEF_LCD_SLOW_BLINK_TIME},
};

static const LCD_SIG_ACTION_T RegSlowBlinkAction[] = /**<慢闪动作数组（2hz）*/
    {
        {1, DEF_LCD_SLOW_BLINK_TIME},
        {0, DEF_LCD_SLOW_BLINK_TIME},
};

static const LCD_SIG_ACTION_T RegFastBlinkAction[] = /**<快闪动作数组（1hz）*/
    {
        {1, DEF_LCD_FAST_BLINK_TIME},
        {0, DEF_LCD_FAST_BLINK_TIME},
};

static const LCD_SIG_ACTION_T RegSelfBlink1Action[] = /**<自定义动作数组*/
    {
        {1, 1000 / LCD_RUN_CYC},
        {0, 9000 / LCD_RUN_CYC},
};

/* Function definitions ------------------------------------------------------*/

/*------------------------------------------------------------------------------
 Function: SegA_Sta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码A状态设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegA_Sta(LCD_NUMBER_ID_E NumIndex, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEA_STA_SEG(1) = Sta;
        break;
    case E_NUM_2:
        TUBEA_STA_SEG(2) = Sta;
        break;
    case E_NUM_3:
        TUBEA_STA_SEG(3) = Sta;
        break;
    case E_NUM_4:
        TUBEA_STA_SEG(4) = Sta;
        break;
    case E_NUM_5:
        TUBEA_STA_SEG(5) = Sta;
        break;
    case E_NUM_6:
        TUBEA_STA_SEG(6) = Sta;
        break;
    case E_NUM_7:
        TUBEA_STA_SEG(7) = Sta;
        break;
    case E_NUM_8:
        TUBEA_STA_SEG(8) = Sta;
        break;
    case E_NUM_9:
        TUBEA_STA_SEG(9) = Sta;
        break;
    case E_NUM_10:
        TUBEA_STA_SEG(10) = Sta;
        break;
    case E_NUM_11:
        TUBEA_STA_SEG(11) = Sta;
        break;
    case E_NUM_12:
        TUBEA_STA_SEG(12) = Sta;
        break;
    case E_NUM_13:
        TUBEA_STA_SEG(13) = Sta;
        break;
    case E_NUM_14:
        TUBEA_STA_SEG(14) = Sta;
        break;
    case E_NUM_15:
        TUBEA_STA_SEG(15) = Sta;
        break;
    case E_NUM_16:
        TUBEA_STA_SEG(16) = Sta;
        break;
    case E_NUM_17:
        TUBEA_STA_SEG(17) = Sta;
        break;
    case E_NUM_18:
        TUBEA_STA_SEG(18) = Sta;
        break;
    case E_NUM_19:
        TUBEA_STA_SEG(19) = Sta;
        break;
    case E_NUM_20:
        TUBEA_STA_SEG(20) = Sta;
        break;
    case E_NUM_21:
        TUBEA_STA_SEG(21) = Sta;
        break;
    case E_NUM_22:
        TUBEA_STA_SEG(22) = Sta;
        break;
    case E_NUM_23:
        TUBEA_STA_SEG(23) = Sta;
        break;
    case E_NUM_24:
        TUBEA_STA_SEG(24) = Sta;
        break;
    case E_NUM_25:
        TUBEA_STA_SEG(25) = Sta;
        break;
    case E_NUM_26:
        TUBEA_STA_SEG(26) = Sta;
        break;
    case E_NUM_27:
        TUBEA_STA_SEG(27) = Sta;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegB_Sta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码B状态设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegB_Sta(LCD_NUMBER_ID_E NumIndex, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEB_STA_SEG(1) = Sta;
        break;
    case E_NUM_2:
        TUBEB_STA_SEG(2) = Sta;
        break;
    case E_NUM_3:
        TUBEB_STA_SEG(3) = Sta;
        break;
    case E_NUM_4:
        TUBEB_STA_SEG(4) = Sta;
        break;
    case E_NUM_5:
        TUBEB_STA_SEG(5) = Sta;
        break;
    case E_NUM_6:
        TUBEB_STA_SEG(6) = Sta;
        break;
    case E_NUM_7:
        TUBEB_STA_SEG(7) = Sta;
        break;
    case E_NUM_8:
        TUBEB_STA_SEG(8) = Sta;
        break;
    case E_NUM_9:
        TUBEB_STA_SEG(9) = Sta;
        break;
    case E_NUM_10:
        TUBEB_STA_SEG(10) = Sta;
        break;
    case E_NUM_11:
        TUBEB_STA_SEG(11) = Sta;
        break;
    case E_NUM_12:
        TUBEB_STA_SEG(12) = Sta;
        break;
    case E_NUM_13:
        TUBEB_STA_SEG(13) = Sta;
        break;
    case E_NUM_14:
        TUBEB_STA_SEG(14) = Sta;
        break;
    case E_NUM_15:
        TUBEB_STA_SEG(15) = Sta;
        break;
    case E_NUM_16:
        TUBEB_STA_SEG(16) = Sta;
        break;
    case E_NUM_17:
        TUBEB_STA_SEG(17) = Sta;
        break;
    case E_NUM_18:
        TUBEB_STA_SEG(18) = Sta;
        break;
    case E_NUM_19:
        TUBEB_STA_SEG(19) = Sta;
        break;
    case E_NUM_20:
        TUBEB_STA_SEG(20) = Sta;
        break;
    case E_NUM_21:
        TUBEB_STA_SEG(21) = Sta;
        break;
    case E_NUM_22:
        TUBEB_STA_SEG(22) = Sta;
        break;
    case E_NUM_23:
        TUBEB_STA_SEG(23) = Sta;
        break;
    case E_NUM_24:
        TUBEB_STA_SEG(24) = Sta;
        break;
    case E_NUM_25:
        TUBEB_STA_SEG(25) = Sta;
        break;
    case E_NUM_26:
        TUBEB_STA_SEG(26) = Sta;
        break;
    case E_NUM_27:
        TUBEB_STA_SEG(27) = Sta;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegC_Sta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码C状态设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegC_Sta(LCD_NUMBER_ID_E NumIndex, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEC_STA_SEG(1) = Sta;
        break;
    case E_NUM_2:
        TUBEC_STA_SEG(2) = Sta;
        break;
    case E_NUM_3:
        TUBEC_STA_SEG(3) = Sta;
        break;
    case E_NUM_4:
        TUBEC_STA_SEG(4) = Sta;
        break;
    case E_NUM_5:
        TUBEC_STA_SEG(5) = Sta;
        break;
    case E_NUM_6:
        TUBEC_STA_SEG(6) = Sta;
        break;
    case E_NUM_7:
        TUBEC_STA_SEG(7) = Sta;
        break;
    case E_NUM_8:
        TUBEC_STA_SEG(8) = Sta;
        break;
    case E_NUM_9:
        TUBEC_STA_SEG(9) = Sta;
        break;
    case E_NUM_10:
        TUBEC_STA_SEG(10) = Sta;
        break;
    case E_NUM_11:
        TUBEC_STA_SEG(11) = Sta;
        break;
    case E_NUM_12:
        TUBEC_STA_SEG(12) = Sta;
        break;
    case E_NUM_13:
        TUBEC_STA_SEG(13) = Sta;
        break;
    case E_NUM_14:
        TUBEC_STA_SEG(14) = Sta;
        break;
    case E_NUM_15:
        TUBEC_STA_SEG(15) = Sta;
        break;
    case E_NUM_16:
        TUBEC_STA_SEG(16) = Sta;
        break;
    case E_NUM_17:
        TUBEC_STA_SEG(17) = Sta;
        break;
    case E_NUM_18:
        TUBEC_STA_SEG(18) = Sta;
        break;
    case E_NUM_19:
        TUBEC_STA_SEG(19) = Sta;
        break;
    case E_NUM_20:
        TUBEC_STA_SEG(20) = Sta;
        break;
    case E_NUM_21:
        TUBEC_STA_SEG(21) = Sta;
        break;
    case E_NUM_22:
        TUBEC_STA_SEG(22) = Sta;
        break;
    case E_NUM_23:
        TUBEC_STA_SEG(23) = Sta;
        break;
    case E_NUM_24:
        TUBEC_STA_SEG(24) = Sta;
        break;
    case E_NUM_25:
        TUBEC_STA_SEG(25) = Sta;
        break;
    case E_NUM_26:
        TUBEC_STA_SEG(26) = Sta;
        break;
    case E_NUM_27:
        TUBEC_STA_SEG(27) = Sta;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegD_Sta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码D状态设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegD_Sta(LCD_NUMBER_ID_E NumIndex, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBED_STA_SEG(1) = Sta;
        break;
    case E_NUM_2:
        TUBED_STA_SEG(2) = Sta;
        break;
    case E_NUM_3:
        TUBED_STA_SEG(3) = Sta;
        break;
    case E_NUM_4:
        TUBED_STA_SEG(4) = Sta;
        break;
    case E_NUM_5:
        TUBED_STA_SEG(5) = Sta;
        break;
    case E_NUM_6:
        TUBED_STA_SEG(6) = Sta;
        break;
    case E_NUM_7:
        TUBED_STA_SEG(7) = Sta;
        break;
    case E_NUM_8:
        TUBED_STA_SEG(8) = Sta;
        break;
    case E_NUM_9:
        TUBED_STA_SEG(9) = Sta;
        break;
    case E_NUM_10:
        TUBED_STA_SEG(10) = Sta;
        break;
    case E_NUM_11:
        TUBED_STA_SEG(11) = Sta;
        break;
    case E_NUM_12:
        TUBED_STA_SEG(12) = Sta;
        break;
    case E_NUM_13:
        TUBED_STA_SEG(13) = Sta;
        break;
    case E_NUM_14:
        TUBED_STA_SEG(14) = Sta;
        break;
    case E_NUM_15:
        TUBED_STA_SEG(15) = Sta;
        break;
    case E_NUM_16:
        TUBED_STA_SEG(16) = Sta;
        break;
    case E_NUM_17:
        TUBED_STA_SEG(17) = Sta;
        break;
    case E_NUM_18:
        TUBED_STA_SEG(18) = Sta;
        break;
    case E_NUM_19:
        TUBED_STA_SEG(19) = Sta;
        break;
    case E_NUM_20:
        TUBED_STA_SEG(20) = Sta;
        break;
    case E_NUM_21:
        TUBED_STA_SEG(21) = Sta;
        break;
    case E_NUM_22:
        TUBED_STA_SEG(22) = Sta;
        break;
    case E_NUM_23:
        TUBED_STA_SEG(23) = Sta;
        break;
    case E_NUM_24:
        TUBED_STA_SEG(24) = Sta;
        break;
    case E_NUM_25:
        TUBED_STA_SEG(25) = Sta;
        break;
    case E_NUM_26:
        TUBED_STA_SEG(26) = Sta;
        break;
    case E_NUM_27:
        TUBED_STA_SEG(27) = Sta;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegE_Sta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码E状态设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegE_Sta(LCD_NUMBER_ID_E NumIndex, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEE_STA_SEG(1) = Sta;
        break;
    case E_NUM_2:
        TUBEE_STA_SEG(2) = Sta;
        break;
    case E_NUM_3:
        TUBEE_STA_SEG(3) = Sta;
        break;
    case E_NUM_4:
        TUBEE_STA_SEG(4) = Sta;
        break;
    case E_NUM_5:
        TUBEE_STA_SEG(5) = Sta;
        break;
    case E_NUM_6:
        TUBEE_STA_SEG(6) = Sta;
        break;
    case E_NUM_7:
        TUBEE_STA_SEG(7) = Sta;
        break;
    case E_NUM_8:
        TUBEE_STA_SEG(8) = Sta;
        break;
    case E_NUM_9:
        TUBEE_STA_SEG(9) = Sta;
        break;
    case E_NUM_10:
        TUBEE_STA_SEG(10) = Sta;
        break;
    case E_NUM_11:
        TUBEE_STA_SEG(11) = Sta;
        break;
    case E_NUM_12:
        TUBEE_STA_SEG(12) = Sta;
        break;
    case E_NUM_13:
        TUBEE_STA_SEG(13) = Sta;
        break;
    case E_NUM_14:
        TUBEE_STA_SEG(14) = Sta;
        break;
    case E_NUM_15:
        TUBEE_STA_SEG(15) = Sta;
        break;
    case E_NUM_16:
        TUBEE_STA_SEG(16) = Sta;
        break;
    case E_NUM_17:
        TUBEE_STA_SEG(17) = Sta;
        break;
    case E_NUM_18:
        TUBEE_STA_SEG(18) = Sta;
        break;
    case E_NUM_19:
        TUBEE_STA_SEG(19) = Sta;
        break;
    case E_NUM_20:
        TUBEE_STA_SEG(20) = Sta;
        break;
    case E_NUM_21:
        TUBEE_STA_SEG(21) = Sta;
        break;
    case E_NUM_22:
        TUBEE_STA_SEG(22) = Sta;
        break;
    case E_NUM_23:
        TUBEE_STA_SEG(23) = Sta;
        break;
    case E_NUM_24:
        TUBEE_STA_SEG(24) = Sta;
        break;
    case E_NUM_25:
        TUBEE_STA_SEG(25) = Sta;
        break;
    case E_NUM_26:
        TUBEE_STA_SEG(26) = Sta;
        break;
    case E_NUM_27:
        TUBEE_STA_SEG(27) = Sta;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegF_Sta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码F状态设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegF_Sta(LCD_NUMBER_ID_E NumIndex, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEF_STA_SEG(1) = Sta;
        break;
    case E_NUM_2:
        TUBEF_STA_SEG(2) = Sta;
        break;
    case E_NUM_3:
        TUBEF_STA_SEG(3) = Sta;
        break;
    case E_NUM_4:
        TUBEF_STA_SEG(4) = Sta;
        break;
    case E_NUM_5:
        TUBEF_STA_SEG(5) = Sta;
        break;
    case E_NUM_6:
        TUBEF_STA_SEG(6) = Sta;
        break;
    case E_NUM_7:
        TUBEF_STA_SEG(7) = Sta;
        break;
    case E_NUM_8:
        TUBEF_STA_SEG(8) = Sta;
        break;
    case E_NUM_9:
        TUBEF_STA_SEG(9) = Sta;
        break;
    case E_NUM_10:
        TUBEF_STA_SEG(10) = Sta;
        break;
    case E_NUM_11:
        TUBEF_STA_SEG(11) = Sta;
        break;
    case E_NUM_12:
        TUBEF_STA_SEG(12) = Sta;
        break;
    case E_NUM_13:
        TUBEF_STA_SEG(13) = Sta;
        break;
    case E_NUM_14:
        TUBEF_STA_SEG(14) = Sta;
        break;
    case E_NUM_15:
        TUBEF_STA_SEG(15) = Sta;
        break;
    case E_NUM_16:
        TUBEF_STA_SEG(16) = Sta;
        break;
    case E_NUM_17:
        TUBEF_STA_SEG(17) = Sta;
        break;
    case E_NUM_18:
        TUBEF_STA_SEG(18) = Sta;
        break;
    case E_NUM_19:
        TUBEF_STA_SEG(19) = Sta;
        break;
    case E_NUM_20:
        TUBEF_STA_SEG(20) = Sta;
        break;
    case E_NUM_21:
        TUBEF_STA_SEG(21) = Sta;
        break;
    case E_NUM_22:
        TUBEF_STA_SEG(22) = Sta;
        break;
    case E_NUM_23:
        TUBEF_STA_SEG(23) = Sta;
        break;
    case E_NUM_24:
        TUBEF_STA_SEG(24) = Sta;
        break;
    case E_NUM_25:
        TUBEF_STA_SEG(25) = Sta;
        break;
    case E_NUM_26:
        TUBEF_STA_SEG(26) = Sta;
        break;
    case E_NUM_27:
        TUBEF_STA_SEG(27) = Sta;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegG_Sta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码G状态设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegG_Sta(LCD_NUMBER_ID_E NumIndex, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEG_STA_SEG(1) = Sta;
        break;
    case E_NUM_2:
        TUBEG_STA_SEG(2) = Sta;
        break;
    case E_NUM_3:
        TUBEG_STA_SEG(3) = Sta;
        break;
    case E_NUM_4:
        TUBEG_STA_SEG(4) = Sta;
        break;
    case E_NUM_5:
        TUBEG_STA_SEG(5) = Sta;
        break;
    case E_NUM_6:
        TUBEG_STA_SEG(6) = Sta;
        break;
    case E_NUM_7:
        TUBEG_STA_SEG(7) = Sta;
        break;
    case E_NUM_8:
        TUBEG_STA_SEG(8) = Sta;
        break;
    case E_NUM_9:
        TUBEG_STA_SEG(9) = Sta;
        break;
    case E_NUM_10:
        TUBEG_STA_SEG(10) = Sta;
        break;
    case E_NUM_11:
        TUBEG_STA_SEG(11) = Sta;
        break;
    case E_NUM_12:
        TUBEG_STA_SEG(12) = Sta;
        break;
    case E_NUM_13:
        TUBEG_STA_SEG(13) = Sta;
        break;
    case E_NUM_14:
        TUBEG_STA_SEG(14) = Sta;
        break;
    case E_NUM_15:
        TUBEG_STA_SEG(15) = Sta;
        break;
    case E_NUM_16:
        TUBEG_STA_SEG(16) = Sta;
        break;
    case E_NUM_17:
        TUBEG_STA_SEG(17) = Sta;
        break;
    case E_NUM_18:
        TUBEG_STA_SEG(18) = Sta;
        break;
    case E_NUM_19:
        TUBEG_STA_SEG(19) = Sta;
        break;
    case E_NUM_20:
        TUBEG_STA_SEG(20) = Sta;
        break;
    case E_NUM_21:
        TUBEG_STA_SEG(21) = Sta;
        break;
    case E_NUM_22:
        TUBEG_STA_SEG(22) = Sta;
        break;
    case E_NUM_23:
        TUBEG_STA_SEG(23) = Sta;
        break;
    case E_NUM_24:
        TUBEG_STA_SEG(24) = Sta;
        break;
    case E_NUM_25:
        TUBEG_STA_SEG(25) = Sta;
        break;
    case E_NUM_26:
        TUBEG_STA_SEG(26) = Sta;
        break;
    case E_NUM_27:
        TUBEG_STA_SEG(27) = Sta;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegA_Mode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码A显示模式设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegA_Mode(LCD_NUMBER_ID_E NumIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEA_MODE_SEG(1) = Mode;
        break;
    case E_NUM_2:
        TUBEA_MODE_SEG(2) = Mode;
        break;
    case E_NUM_3:
        TUBEA_MODE_SEG(3) = Mode;
        break;
    case E_NUM_4:
        TUBEA_MODE_SEG(4) = Mode;
        break;
    case E_NUM_5:
        TUBEA_MODE_SEG(5) = Mode;
        break;
    case E_NUM_6:
        TUBEA_MODE_SEG(6) = Mode;
        break;
    case E_NUM_7:
        TUBEA_MODE_SEG(7) = Mode;
        break;
    case E_NUM_8:
        TUBEA_MODE_SEG(8) = Mode;
        break;
    case E_NUM_9:
        TUBEA_MODE_SEG(9) = Mode;
        break;
    case E_NUM_10:
        TUBEA_MODE_SEG(10) = Mode;
        break;
    case E_NUM_11:
        TUBEA_MODE_SEG(11) = Mode;
        break;
    case E_NUM_12:
        TUBEA_MODE_SEG(12) = Mode;
        break;
    case E_NUM_13:
        TUBEA_MODE_SEG(13) = Mode;
        break;
    case E_NUM_14:
        TUBEA_MODE_SEG(14) = Mode;
        break;
    case E_NUM_15:
        TUBEA_MODE_SEG(15) = Mode;
        break;
    case E_NUM_16:
        TUBEA_MODE_SEG(16) = Mode;
        break;
    case E_NUM_17:
        TUBEA_MODE_SEG(17) = Mode;
        break;
    case E_NUM_18:
        TUBEA_MODE_SEG(18) = Mode;
        break;
    case E_NUM_19:
        TUBEA_MODE_SEG(19) = Mode;
        break;
    case E_NUM_20:
        TUBEA_MODE_SEG(20) = Mode;
        break;
    case E_NUM_21:
        TUBEA_MODE_SEG(21) = Mode;
        break;
    case E_NUM_22:
        TUBEA_MODE_SEG(22) = Mode;
        break;
    case E_NUM_23:
        TUBEA_MODE_SEG(23) = Mode;
        break;
    case E_NUM_24:
        TUBEA_MODE_SEG(24) = Mode;
        break;
    case E_NUM_25:
        TUBEA_MODE_SEG(25) = Mode;
        break;
    case E_NUM_26:
        TUBEA_MODE_SEG(26) = Mode;
        break;
    case E_NUM_27:
        TUBEA_MODE_SEG(27) = Mode;
        break;
    /*..根据段码屏实际数字个数补充...*/
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegB_Mode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码B显示模式设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegB_Mode(LCD_NUMBER_ID_E NumIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEB_MODE_SEG(1) = Mode;
        break;
    case E_NUM_2:
        TUBEB_MODE_SEG(2) = Mode;
        break;
    case E_NUM_3:
        TUBEB_MODE_SEG(3) = Mode;
        break;
    case E_NUM_4:
        TUBEB_MODE_SEG(4) = Mode;
        break;
    case E_NUM_5:
        TUBEB_MODE_SEG(5) = Mode;
        break;
    case E_NUM_6:
        TUBEB_MODE_SEG(6) = Mode;
        break;
    case E_NUM_7:
        TUBEB_MODE_SEG(7) = Mode;
        break;
    case E_NUM_8:
        TUBEB_MODE_SEG(8) = Mode;
        break;
    case E_NUM_9:
        TUBEB_MODE_SEG(9) = Mode;
        break;
    case E_NUM_10:
        TUBEB_MODE_SEG(10) = Mode;
        break;
    case E_NUM_11:
        TUBEB_MODE_SEG(11) = Mode;
        break;
    case E_NUM_12:
        TUBEB_MODE_SEG(12) = Mode;
        break;
    case E_NUM_13:
        TUBEB_MODE_SEG(13) = Mode;
        break;
    case E_NUM_14:
        TUBEB_MODE_SEG(14) = Mode;
        break;
    case E_NUM_15:
        TUBEB_MODE_SEG(15) = Mode;
        break;
    case E_NUM_16:
        TUBEB_MODE_SEG(16) = Mode;
        break;
    case E_NUM_17:
        TUBEB_MODE_SEG(17) = Mode;
        break;
    case E_NUM_18:
        TUBEB_MODE_SEG(18) = Mode;
        break;
    case E_NUM_19:
        TUBEB_MODE_SEG(19) = Mode;
        break;
    case E_NUM_20:
        TUBEB_MODE_SEG(20) = Mode;
        break;
    case E_NUM_21:
        TUBEB_MODE_SEG(21) = Mode;
        break;
    case E_NUM_22:
        TUBEB_MODE_SEG(22) = Mode;
        break;
    case E_NUM_23:
        TUBEB_MODE_SEG(23) = Mode;
        break;
    case E_NUM_24:
        TUBEB_MODE_SEG(24) = Mode;
        break;
    case E_NUM_25:
        TUBEB_MODE_SEG(25) = Mode;
        break;
    case E_NUM_26:
        TUBEB_MODE_SEG(26) = Mode;
        break;
    case E_NUM_27:
        TUBEB_MODE_SEG(27) = Mode;
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegC_Mode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码C显示模式设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegC_Mode(LCD_NUMBER_ID_E NumIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEC_MODE_SEG(1) = Mode;
        break;
    case E_NUM_2:
        TUBEC_MODE_SEG(2) = Mode;
        break;
    case E_NUM_3:
        TUBEC_MODE_SEG(3) = Mode;
        break;
    case E_NUM_4:
        TUBEC_MODE_SEG(4) = Mode;
        break;
    case E_NUM_5:
        TUBEC_MODE_SEG(5) = Mode;
        break;
    case E_NUM_6:
        TUBEC_MODE_SEG(6) = Mode;
        break;
    case E_NUM_7:
        TUBEC_MODE_SEG(7) = Mode;
        break;
    case E_NUM_8:
        TUBEC_MODE_SEG(8) = Mode;
        break;
    case E_NUM_9:
        TUBEC_MODE_SEG(9) = Mode;
        break;
    case E_NUM_10:
        TUBEC_MODE_SEG(10) = Mode;
        break;
    case E_NUM_11:
        TUBEC_MODE_SEG(11) = Mode;
        break;
    case E_NUM_12:
        TUBEC_MODE_SEG(12) = Mode;
        break;
    case E_NUM_13:
        TUBEC_MODE_SEG(13) = Mode;
        break;
    case E_NUM_14:
        TUBEC_MODE_SEG(14) = Mode;
        break;
    case E_NUM_15:
        TUBEC_MODE_SEG(15) = Mode;
        break;
    case E_NUM_16:
        TUBEC_MODE_SEG(16) = Mode;
        break;
    case E_NUM_17:
        TUBEC_MODE_SEG(17) = Mode;
        break;
    case E_NUM_18:
        TUBEC_MODE_SEG(18) = Mode;
        break;
    case E_NUM_19:
        TUBEC_MODE_SEG(19) = Mode;
        break;
    case E_NUM_20:
        TUBEC_MODE_SEG(20) = Mode;
        break;
    case E_NUM_21:
        TUBEC_MODE_SEG(21) = Mode;
        break;
    case E_NUM_22:
        TUBEC_MODE_SEG(22) = Mode;
        break;
    case E_NUM_23:
        TUBEC_MODE_SEG(23) = Mode;
        break;
    case E_NUM_24:
        TUBEC_MODE_SEG(24) = Mode;
        break;
    case E_NUM_25:
        TUBEC_MODE_SEG(25) = Mode;
        break;
    case E_NUM_26:
        TUBEC_MODE_SEG(26) = Mode;
        break;
    case E_NUM_27:
        TUBEC_MODE_SEG(27) = Mode;
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegD_Mode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码D显示模式设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegD_Mode(LCD_NUMBER_ID_E NumIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBED_MODE_SEG(1) = Mode;
        break;
    case E_NUM_2:
        TUBED_MODE_SEG(2) = Mode;
        break;
    case E_NUM_3:
        TUBED_MODE_SEG(3) = Mode;
        break;
    case E_NUM_4:
        TUBED_MODE_SEG(4) = Mode;
        break;
    case E_NUM_5:
        TUBED_MODE_SEG(5) = Mode;
        break;
    case E_NUM_6:
        TUBED_MODE_SEG(6) = Mode;
        break;
    case E_NUM_7:
        TUBED_MODE_SEG(7) = Mode;
        break;
    case E_NUM_8:
        TUBED_MODE_SEG(8) = Mode;
        break;
    case E_NUM_9:
        TUBED_MODE_SEG(9) = Mode;
        break;
    case E_NUM_10:
        TUBED_MODE_SEG(10) = Mode;
        break;
    case E_NUM_11:
        TUBED_MODE_SEG(11) = Mode;
        break;
    case E_NUM_12:
        TUBED_MODE_SEG(12) = Mode;
        break;
    case E_NUM_13:
        TUBED_MODE_SEG(13) = Mode;
        break;
    case E_NUM_14:
        TUBED_MODE_SEG(14) = Mode;
        break;
    case E_NUM_15:
        TUBED_MODE_SEG(15) = Mode;
        break;
    case E_NUM_16:
        TUBED_MODE_SEG(16) = Mode;
        break;
    case E_NUM_17:
        TUBED_MODE_SEG(17) = Mode;
        break;
    case E_NUM_18:
        TUBED_MODE_SEG(18) = Mode;
        break;
    case E_NUM_19:
        TUBED_MODE_SEG(19) = Mode;
        break;
    case E_NUM_20:
        TUBED_MODE_SEG(20) = Mode;
        break;
    case E_NUM_21:
        TUBED_MODE_SEG(21) = Mode;
        break;
    case E_NUM_22:
        TUBED_MODE_SEG(22) = Mode;
        break;
    case E_NUM_23:
        TUBED_MODE_SEG(23) = Mode;
        break;
    case E_NUM_24:
        TUBED_MODE_SEG(24) = Mode;
        break;
    case E_NUM_25:
        TUBED_MODE_SEG(25) = Mode;
        break;
    case E_NUM_26:
        TUBED_MODE_SEG(26) = Mode;
        break;
    case E_NUM_27:
        TUBED_MODE_SEG(27) = Mode;
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegE_Mode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码E显示模式设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegE_Mode(LCD_NUMBER_ID_E NumIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEE_MODE_SEG(1) = Mode;
        break;
    case E_NUM_2:
        TUBEE_MODE_SEG(2) = Mode;
        break;
    case E_NUM_3:
        TUBEE_MODE_SEG(3) = Mode;
        break;
    case E_NUM_4:
        TUBEE_MODE_SEG(4) = Mode;
        break;
    case E_NUM_5:
        TUBEE_MODE_SEG(5) = Mode;
        break;
    case E_NUM_6:
        TUBEE_MODE_SEG(6) = Mode;
        break;
    case E_NUM_7:
        TUBEE_MODE_SEG(7) = Mode;
        break;
    case E_NUM_8:
        TUBEE_MODE_SEG(8) = Mode;
        break;
    case E_NUM_9:
        TUBEE_MODE_SEG(9) = Mode;
        break;
    case E_NUM_10:
        TUBEE_MODE_SEG(10) = Mode;
        break;
    case E_NUM_11:
        TUBEE_MODE_SEG(11) = Mode;
        break;
    case E_NUM_12:
        TUBEE_MODE_SEG(12) = Mode;
        break;
    case E_NUM_13:
        TUBEE_MODE_SEG(13) = Mode;
        break;
    case E_NUM_14:
        TUBEE_MODE_SEG(14) = Mode;
        break;
    case E_NUM_15:
        TUBEE_MODE_SEG(15) = Mode;
        break;
    case E_NUM_16:
        TUBEE_MODE_SEG(16) = Mode;
        break;
    case E_NUM_17:
        TUBEE_MODE_SEG(17) = Mode;
        break;
    case E_NUM_18:
        TUBEE_MODE_SEG(18) = Mode;
        break;
    case E_NUM_19:
        TUBEE_MODE_SEG(19) = Mode;
        break;
    case E_NUM_20:
        TUBEE_MODE_SEG(20) = Mode;
        break;
    case E_NUM_21:
        TUBEE_MODE_SEG(21) = Mode;
        break;
    case E_NUM_22:
        TUBEE_MODE_SEG(22) = Mode;
        break;
    case E_NUM_23:
        TUBEE_MODE_SEG(23) = Mode;
        break;
    case E_NUM_24:
        TUBEE_MODE_SEG(24) = Mode;
        break;
    case E_NUM_25:
        TUBEE_MODE_SEG(25) = Mode;
        break;
    case E_NUM_26:
        TUBEE_MODE_SEG(26) = Mode;
        break;
    case E_NUM_27:
        TUBEE_MODE_SEG(27) = Mode;
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegF_Mode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码F显示模式设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegF_Mode(LCD_NUMBER_ID_E NumIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEF_MODE_SEG(1) = Mode;
        break;
    case E_NUM_2:
        TUBEF_MODE_SEG(2) = Mode;
        break;
    case E_NUM_3:
        TUBEF_MODE_SEG(3) = Mode;
        break;
    case E_NUM_4:
        TUBEF_MODE_SEG(4) = Mode;
        break;
    case E_NUM_5:
        TUBEF_MODE_SEG(5) = Mode;
        break;
    case E_NUM_6:
        TUBEF_MODE_SEG(6) = Mode;
        break;
    case E_NUM_7:
        TUBEF_MODE_SEG(7) = Mode;
        break;
    case E_NUM_8:
        TUBEF_MODE_SEG(8) = Mode;
        break;
    case E_NUM_9:
        TUBEF_MODE_SEG(9) = Mode;
        break;
    case E_NUM_10:
        TUBEF_MODE_SEG(10) = Mode;
        break;
    case E_NUM_11:
        TUBEF_MODE_SEG(11) = Mode;
        break;
    case E_NUM_12:
        TUBEF_MODE_SEG(12) = Mode;
        break;
    case E_NUM_13:
        TUBEF_MODE_SEG(13) = Mode;
        break;
    case E_NUM_14:
        TUBEF_MODE_SEG(14) = Mode;
        break;
    case E_NUM_15:
        TUBEF_MODE_SEG(15) = Mode;
        break;
    case E_NUM_16:
        TUBEF_MODE_SEG(16) = Mode;
        break;
    case E_NUM_17:
        TUBEF_MODE_SEG(17) = Mode;
        break;
    case E_NUM_18:
        TUBEF_MODE_SEG(18) = Mode;
        break;
    case E_NUM_19:
        TUBEF_MODE_SEG(19) = Mode;
        break;
    case E_NUM_20:
        TUBEF_MODE_SEG(20) = Mode;
        break;
    case E_NUM_21:
        TUBEF_MODE_SEG(21) = Mode;
        break;
    case E_NUM_22:
        TUBEF_MODE_SEG(22) = Mode;
        break;
    case E_NUM_23:
        TUBEF_MODE_SEG(23) = Mode;
        break;
    case E_NUM_24:
        TUBEF_MODE_SEG(24) = Mode;
        break;
    case E_NUM_25:
        TUBEF_MODE_SEG(25) = Mode;
        break;
    case E_NUM_26:
        TUBEF_MODE_SEG(26) = Mode;
        break;
    case E_NUM_27:
        TUBEF_MODE_SEG(27) = Mode;
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: SegG_Mode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码G显示模式设置
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void SegG_Mode(LCD_NUMBER_ID_E NumIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (NumIndex) {
    case E_NUM_1:
        TUBEG_MODE_SEG(1) = Mode;
        break;
    case E_NUM_2:
        TUBEG_MODE_SEG(2) = Mode;
        break;
    case E_NUM_3:
        TUBEG_MODE_SEG(3) = Mode;
        break;
    case E_NUM_4:
        TUBEG_MODE_SEG(4) = Mode;
        break;
    case E_NUM_5:
        TUBEG_MODE_SEG(5) = Mode;
        break;
    case E_NUM_6:
        TUBEG_MODE_SEG(6) = Mode;
        break;
    case E_NUM_7:
        TUBEG_MODE_SEG(7) = Mode;
        break;
    case E_NUM_8:
        TUBEG_MODE_SEG(8) = Mode;
        break;
    case E_NUM_9:
        TUBEG_MODE_SEG(9) = Mode;
        break;
    case E_NUM_10:
        TUBEG_MODE_SEG(10) = Mode;
        break;
    case E_NUM_11:
        TUBEG_MODE_SEG(11) = Mode;
        break;
    case E_NUM_12:
        TUBEG_MODE_SEG(12) = Mode;
        break;
    case E_NUM_13:
        TUBEG_MODE_SEG(13) = Mode;
        break;
    case E_NUM_14:
        TUBEG_MODE_SEG(14) = Mode;
        break;
    case E_NUM_15:
        TUBEG_MODE_SEG(15) = Mode;
        break;
    case E_NUM_16:
        TUBEG_MODE_SEG(16) = Mode;
        break;
    case E_NUM_17:
        TUBEG_MODE_SEG(17) = Mode;
        break;
    case E_NUM_18:
        TUBEG_MODE_SEG(18) = Mode;
        break;
    case E_NUM_19:
        TUBEG_MODE_SEG(19) = Mode;
        break;
    case E_NUM_20:
        TUBEG_MODE_SEG(20) = Mode;
        break;
    case E_NUM_21:
        TUBEG_MODE_SEG(21) = Mode;
        break;
    case E_NUM_22:
        TUBEG_MODE_SEG(22) = Mode;
        break;
    case E_NUM_23:
        TUBEG_MODE_SEG(23) = Mode;
        break;
    case E_NUM_24:
        TUBEG_MODE_SEG(24) = Mode;
        break;
    case E_NUM_25:
        TUBEG_MODE_SEG(25) = Mode;
        break;
    case E_NUM_26:
        TUBEG_MODE_SEG(26) = Mode;
        break;
    case E_NUM_27:
        TUBEG_MODE_SEG(27) = Mode;
        break;

    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_TubeSegBitSta_Set
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码显示状态设定
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             TubeReg：断码编号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void CDD_TubeSegBitSta_Set(LCD_NUMBER_ID_E NumIndex,
                                  LCD_TUBE_REG_E TubeReg,
                                  LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (TubeReg) {
    case E_LCD_TUBE_A:
        SegA_Sta(NumIndex, Sta);
        break;
    case E_LCD_TUBE_B:
        SegB_Sta(NumIndex, Sta);
        break;
    case E_LCD_TUBE_C:
        SegC_Sta(NumIndex, Sta);
        break;
    case E_LCD_TUBE_D:
        SegD_Sta(NumIndex, Sta);
        break;
    case E_LCD_TUBE_E:
        SegE_Sta(NumIndex, Sta);
        break;
    case E_LCD_TUBE_F:
        SegF_Sta(NumIndex, Sta);
        break;
    case E_LCD_TUBE_G:
        SegG_Sta(NumIndex, Sta);
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_TubeSegBitMode_Set
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码显示模式设定
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             TubeReg：断码编号
 *             Mode：显示模式
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void CDD_TubeSegBitMode_Set(LCD_NUMBER_ID_E NumIndex,
                                   LCD_TUBE_REG_E TubeReg,
                                   LCD_ICO_MODE_E Mode) {
    LS_ASSERT(NumIndex < E_NUM_MAX);

    switch (TubeReg) {
    case E_LCD_TUBE_A:
        SegA_Mode(NumIndex, Mode);
        break;
    case E_LCD_TUBE_B:
        SegB_Mode(NumIndex, Mode);
        break;
    case E_LCD_TUBE_C:
        SegC_Mode(NumIndex, Mode);
        break;
    case E_LCD_TUBE_D:
        SegD_Mode(NumIndex, Mode);
        break;
    case E_LCD_TUBE_E:
        SegE_Mode(NumIndex, Mode);
        break;
    case E_LCD_TUBE_F:
        SegF_Mode(NumIndex, Mode);
        break;
    case E_LCD_TUBE_G:
        SegG_Mode(NumIndex, Mode);
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_IconCtrlTask
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD屏段码模式控制进程
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       10ms周期（在Lcd_Run中调用）
 */
static void CDD_Lcd_IconCtrlTask(void) {
    uint8_t j;
    uint8_t i;
    uint32_t IcoMode;
    uint8_t Shit;
    uint8_t Temp;

    uint8_t LcdTimeEn[E_ICO_MODE_MAX] = {0};
    static uint8_t LcdSta[E_ICO_MODE_MAX] = {0};

    /** 遍历每个LCD屏每个段码状态（开机动画完成后） */
    if (CDD_Lcd_MovieFinishIs() == 1) {
        for (i = 0; i < LCD_DATA_LEN; i++) {
            for (j = 0; j < 8; j++) {
                Shit = j * 4;

                /** 对应断码位置4bit长度表示工作模式 */
                IcoMode = LcdIcoModeSet.Data[i];
                IcoMode &= ((uint32_t)0x0000000F << Shit);
                IcoMode >>= Shit;

                /** 模式超过设定 */
                LS_ASSERT(IcoMode < E_ICO_MODE_MAX);

                /** 使能对应闪烁的时基定时 */
                LcdTimeEn[IcoMode] = 1;

                /** 查找当前模式对应的动作数组亮灭状态 */
                if (LcdSta[IcoMode] == 1) {
                    LcdScanData.Data[i] |= (0x01 << j);
                } else {
                    LcdScanData.Data[i] &= ~(0x01 << j);
                }
            }
        }
    }

    /** 不同闪烁模式按对应动作数组遍历动作*/
    for (i = 0; i < E_ICO_MODE_MAX; i++) {
        /* 时基使能后才开始定时计算（前4种模式作为通用模式和需和LED闪烁同频的除外）*/
        if ((LcdTimeEn[i] == 1) || (i <= E_ICO_MODE_FAST_BLINK)) {
            LcdBlinkMode[i].CurrAct.KeepTime--;
            if (LcdBlinkMode[i].CurrAct.KeepTime <= 0) {
                Temp = LcdBlinkMode[i].Index;

                LcdBlinkMode[i].CurrAct = LcdBlinkMode[i].ActPtr[Temp];

                LcdBlinkMode[i].Index = (Temp + 1) % LcdBlinkMode[i].ActLen;
            }
        } else {
            LcdBlinkMode[i].Index = 0;
            LcdBlinkMode[i].CurrAct.LcdSta = 0;
            LcdBlinkMode[i].CurrAct.KeepTime = 0;
        }

        /** 更新当前等级的LCD段码状态 */
        LcdSta[i] = LcdBlinkMode[i].CurrAct.LcdSta;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SocPerSet
 -----------------------------------------------------------------------------*/
/**
 * @brief      SOC进度条设置给定
 * @details    N/A
 * @param[in]  Per：0 - 10，10格进度条
 * @param[out] N/A
 * @return     N/A
 * @note       在Lcd_SocPerCtrlTask中调用
 */
static void CDD_Lcd_SocPerSet(uint8_t Per) {
    uint8_t Temp;

    LcdIcoModeSet.DataFrame.SocPer10 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer20 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer30 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer40 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer50 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer60 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer70 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer80 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer90 = E_ICO_MODE_OPEN;
    LcdIcoModeSet.DataFrame.SocPer100 = E_ICO_MODE_OPEN;

    Temp = (Per > 10) ? 10 : Per;

    switch (Temp) {
    case 0: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer10 = E_ICO_MODE_CLOSE;
    case 1: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer20 = E_ICO_MODE_CLOSE;
    case 2: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer30 = E_ICO_MODE_CLOSE;
    case 3: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer40 = E_ICO_MODE_CLOSE;
    case 4: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer50 = E_ICO_MODE_CLOSE;
    case 5: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer60 = E_ICO_MODE_CLOSE;
    case 6: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer70 = E_ICO_MODE_CLOSE;
    case 7: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer80 = E_ICO_MODE_CLOSE;
    case 8: /** 不需break */
        LcdIcoModeSet.DataFrame.SocPer90 = E_ICO_MODE_CLOSE;
    case 9:
        LcdIcoModeSet.DataFrame.SocPer100 = E_ICO_MODE_CLOSE;
        break;

    default:
        // LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SocDivToBar
 -----------------------------------------------------------------------------*/
/**
 * @brief      SOC值转换成进度条
 * @details    N/A
 * @param[in]  *ChgCtrl：控制结构体
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */

static void CDD_Lcd_SocDivToBar(LCD_SOCPER_CTRL_T *ChgCtrl) {
    if (ChgCtrl->SocValue >= 100) {
        ChgCtrl->CurrBar = 10;
    } else {
        ChgCtrl->CurrBar = ChgCtrl->SocValue / 10;

        /** SOC：不满一格按照一格处理*/
        if ((ChgCtrl->SocValue % 10) > 0) {
            ChgCtrl->CurrBar += 1;
        }
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SocChgConst
 -----------------------------------------------------------------------------*/
/**
 * @brief      SOC进度条常亮
 * @details    N/A
 * @param[in]  *ChgCtrl：控制结构体
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */

static uint8_t CDD_Lcd_SocChgConst(LCD_SOCPER_CTRL_T *ChgCtrl) {
    ChgCtrl->TimeCnt = 0;
    ChgCtrl->LastBar = ChgCtrl->CurrBar;

    return (uint8_t)ChgCtrl->CurrBar;
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SocChgBlink
 -----------------------------------------------------------------------------*/
/**
 * @brief      SOC充电跑马
 * @details    N/A
 * @param[in]  *ChgCtrl：控制结构体
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static uint8_t CDD_Lcd_SocChgBlink(LCD_SOCPER_CTRL_T *ChgCtrl) {
    ChgCtrl->TimeCnt--;
    if (ChgCtrl->TimeCnt <= 0) {
        ChgCtrl->TimeCnt = LCD_CHG_PER_BLINK_TIEM;

        ChgCtrl->LastBar++;
        if (ChgCtrl->LastBar > 10) {
            if (ChgCtrl->CurrBar > 0) {
                ChgCtrl->LastBar = ChgCtrl->CurrBar - 1;
            } else {
                ChgCtrl->LastBar = 0;
            }
        }
    }

    return (uint8_t)ChgCtrl->LastBar;
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SocHysBlink
 -----------------------------------------------------------------------------*/
/**
 * @brief      SOC进度条滞回闪烁
 * @details    N/A
 * @param[in]  *ChgCtrl：控制结构体
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static uint8_t CDD_Lcd_SocHysBlink(LCD_SOCPER_CTRL_T *ChgCtrl) {
    ChgCtrl->TimeCnt--;
    if (ChgCtrl->TimeCnt <= 0) {
        ChgCtrl->TimeCnt = LCD_CHG_PER_BLINK_TIEM;

        if (ChgCtrl->CurrBar > 0) {
            ChgCtrl->LastBar++;
            if (ChgCtrl->LastBar > ChgCtrl->CurrBar) {
                ChgCtrl->LastBar = ChgCtrl->CurrBar - 1;
            }
        } else {
            ChgCtrl->LastBar = (ChgCtrl->LastBar > 0) ? 0 : 1;
        }
    }

    return (uint8_t)ChgCtrl->LastBar;
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SocPerCtrlTask
 -----------------------------------------------------------------------------*/
/**
 * @brief      SOC进度条设置给定
 * @details    N/A
 * @param[in]  Per：0 - 10，10格进度条
 * @param[out] N/A
 * @return     N/A
 * @note       在Lcd_Run中调用
 */
static void CDD_Lcd_SocPerCtrlTask(void) {
    uint8_t SocPer = 0;

    CDD_Lcd_SocDivToBar(&LcdSocPerCtrl);

    switch (LcdSocPerCtrl.Mode) {
    /** 常亮 */
    case E_LCD_PER_MODE_CONST:
        SocPer = CDD_Lcd_SocChgConst(&LcdSocPerCtrl);
        break;

    /** 充电跑马闪烁（从当前SOC跑到满量程）*/
    case E_LCD_PER_MODE_CHG:
        SocPer = CDD_Lcd_SocChgBlink(&LcdSocPerCtrl);
        break;

    /** 充电滞回跑马（只闪当前的SOC） */
    case E_LCD_PER_MODE_HYS:
        SocPer = CDD_Lcd_SocHysBlink(&LcdSocPerCtrl);
        break;

    default:
        LS_ASSERT(0);
        break;
    }

    CDD_Lcd_SocPerSet(SocPer);
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SetNumberSta
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码显示状态设定
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Data：显示数字：0 - 9，及一些特殊符号
 *             Sta：状态设定：0、1；
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void CDD_Lcd_SetNumberSta(LCD_NUMBER_ID_E NumIndex, LCD_NUM_SHOW_E Data, LCD_ICO_STA_E Sta) {
    LS_ASSERT(NumIndex < E_NUM_MAX);
    LS_ASSERT(Data < LCD_REG_LEN);

    uint8_t RegTemp;

    /** 取对应的段码表 */
    RegTemp = TubeRegTable[Data];

    /** 按照数字段码排序为：ABCDEFG顺序扫描 */
    for (uint8_t i = 0; i < 7; i++) {
        if ((RegTemp & 0x80) > 0) {
            CDD_TubeSegBitSta_Set(NumIndex, (LCD_TUBE_REG_E)i, Sta);
        } else {
            CDD_TubeSegBitSta_Set(NumIndex, (LCD_TUBE_REG_E)i, E_LCO_STA_CLOSE);
        }

        RegTemp <<= 1;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_InitMovie
 -----------------------------------------------------------------------------*/
/**
 * @brief      开机动画
 * @details    开机动画效果：由中间向两边展开
 * @param[in]  N/A
 * @param[out] N/A
 * @return     !0：动画完成
 * @note       N/A
 */
static uint8_t CDD_Lcd_InitMovie(void) {
    static int32_t MovieScheTime = 0;

    uint8_t Finish = 0;

    if ((MovieScheTime--) <= 0) {
        switch (LcdMovieCtrl.Step) {
        /** 清屏操作 */
        case 0:
            CDD_Lcd_Clear();
            MovieScheTime = 1;
            LcdMovieCtrl.Step++;
            break;

        /** 开启画面，从中间向两边展开*/
        case 1:
            CDD_Lcd_SetBrightSta(E_BRIGHT_OPEN);

            CDD_Lcd_SetNumberMode(E_NUM_15, 0, E_ICO_MODE_OPEN);
            CDD_Lcd_SetNumberMode(E_NUM_16, 0, E_ICO_MODE_OPEN);
            CDD_Lcd_SetNumberMode(E_NUM_20, 0, E_ICO_MODE_OPEN);
            CDD_Lcd_SetNumberMode(E_NUM_21, 0, E_ICO_MODE_OPEN);
            CDD_Lcd_SetNumberMode(E_NUM_24, 0, E_ICO_MODE_OPEN);
            CDD_Lcd_SetNumberMode(E_NUM_25, 0, E_ICO_MODE_OPEN);

            CDD_Lcd_SetSigIcoMode(ICON_SOC_PER, E_ICO_MODE_OPEN);
            MovieScheTime = MOVIE_ON_SCHE1_TIME;
            LcdMovieCtrl.Step++;
            break;

        /** 动画完成，清除当前状态*/
        default:
            LcdMovieCtrl.Step = 0;

            Finish = 1;
            break;
        }
    }

    return Finish;
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_OffMovie
 -----------------------------------------------------------------------------*/
/**
 * @brief      关机动画
 * @details    关机动画，显示"OFF"
 * @param[in]  N/A
 * @param[out] N/A
 * @return     ！0：动画完成
 * @note       10ms周期
 */

static uint8_t CDD_Lcd_OffMovie(void) {
    uint8_t finish = 0;
    static uint32_t MovieTime = 0;

    if ((MovieTime--) <= 0) {
        switch (LcdMovieCtrl.Step) {
        /** 清屏 */
        case 0:
            CDD_Lcd_Clear();
            LcdMovieCtrl.Step++;
            MovieTime = MOVIE_OFF_SCHE1_TIME;
            break;

        /** 开背光灯，显示"OFF"*/
        case 1:
            CDD_Lcd_SetBrightSta(E_BRIGHT_OPEN);

            CDD_Lcd_SetNumberSta(E_NUM_22, (LCD_NUM_SHOW_E)0, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_1, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_2, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_3, (LCD_NUM_SHOW_E)0, E_LCO_STA_CLOSE);

            CDD_Lcd_SetNumberSta(E_NUM_6, (LCD_NUM_SHOW_E)0, E_LCO_STA_CLOSE);
            CDD_Lcd_SetNumberSta(E_NUM_7, (LCD_NUM_SHOW_E)0, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_8, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_9, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);

            CDD_Lcd_SetNumberSta(E_NUM_10, (LCD_NUM_SHOW_E)0, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_11, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_12, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_13, (LCD_NUM_SHOW_E)0, E_LCO_STA_CLOSE);

            CDD_Lcd_SetNumberSta(E_NUM_14, (LCD_NUM_SHOW_E)0, E_LCO_STA_CLOSE);
            CDD_Lcd_SetNumberSta(E_NUM_15, (LCD_NUM_SHOW_E)0, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_16, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);
            CDD_Lcd_SetNumberSta(E_NUM_17, (LCD_NUM_SHOW_E)E_LCD_SHOW_F, E_LCO_STA_OPEN);

            MovieTime = MOVIE_OFF_SCHE2_TIME;
            LcdMovieCtrl.Step++;
            break;

        /** 清屏，关背光灯*/
        case 2:
            CDD_Lcd_Clear();

            LcdMovieCtrl.Step++;
            CDD_Lcd_SetBrightSta(E_BRIGHT_CLOSE);
            MovieTime = MOVIE_OFF_SCHE3_TIME;
            break;

        /** 动作完成 */
        default:
            LcdMovieCtrl.Step = 0;
            finish = 1;
            break;
        }
    }

    return finish;
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_MovieCtrlTask
 -----------------------------------------------------------------------------*/
/**
 * @brief      开关机动画操作
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       10ms调用周期
 */
static void CDD_Lcd_MovieCtrlTask(void) {
    if (LcdMovieCtrl.LastMovieSel != LcdMovieCtrl.MovieSel) {
        LcdMovieCtrl.LastMovieSel = LcdMovieCtrl.MovieSel;
        LcdMovieCtrl.Step = 0;
    }

    switch (LcdMovieCtrl.MovieSel) {
    /** 开机动画 */
    case E_LCD_MOVIE_INIT:
        if (CDD_Lcd_InitMovie() == 1) {
            LcdMovieCtrl.MovieSel = E_LCD_MOVIE_IDEL;
        }
        break;

    /** 关机动画 */
    case E_LCD_MOVIE_OFF:
        if (CDD_Lcd_OffMovie() == 1) {
            LcdMovieCtrl.MovieSel = E_LCD_MOVIE_IDEL;
        }
        break;

    /** 正常状态（该界面才会响应界面图标的其他控制） */
    case E_LCD_MOVIE_IDEL:;
        break;
    default:
        LS_ASSERT(0);
        break;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SigModeDef
 -----------------------------------------------------------------------------*/
/**
 * @brief      单个LCD段码闪烁模式设定
 * @details    N/A
 * @param[in]  Mode：模式枚举
 *             ActArry：动作数组指针
 *             ActLen：动作数据长度
 * @param[out] N/A
 * @return     N/A
 * @note
 */
static void CDD_Lcd_SigModeDef(LCD_ICO_MODE_E Mode,
                               const LCD_SIG_ACTION_T *ActArry,
                               uint8_t ActLen) {
    LS_ASSERT(Mode < E_ICO_MODE_MAX);
    LS_ASSERT(ActArry != NULL);

    LcdBlinkMode[Mode].ActPtr = ActArry;
    LcdBlinkMode[Mode].ActLen = ActLen;
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_MovieFinishIs
 -----------------------------------------------------------------------------*/
/**
 * @brief      开关机动画动作是否完成判断
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     ！0：动作完成
 * @note       N/A
 */

uint8_t CDD_Lcd_MovieFinishIs(void) {
    return (uint8_t)((LcdMovieCtrl.MovieSel == E_LCD_MOVIE_IDEL) ? 1 : 0);
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_Init
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD段码屏初始化配置
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_Init(void) {
    (void)memset((void *)&LcdScanData, 0, sizeof(LcdScanData));
    (void)memset((void *)&LcdIcoModeSet, 0, sizeof(LcdIcoModeSet));
    (void)memset((void *)&LcdSocPerCtrl, 0, sizeof(LcdSocPerCtrl));
    (void)memset((void *)&LcdMovieCtrl, 0, sizeof(LcdMovieCtrl));
    (void)memset((void *)&LcdBlinkMode, 0, sizeof(LcdBlinkMode));

    /** 底层驱动指令插入待写队列 */
    CDD_Lcd_Hw_Init();

    /** 设定闪烁模式（通过独立的数组） */
    CDD_Lcd_SigModeDef(
        E_ICO_MODE_CLOSE, RegCloseAction, sizeof(RegCloseAction) / sizeof(LCD_SIG_ACTION_T));
    CDD_Lcd_SigModeDef(
        E_ICO_MODE_OPEN, RegOpenAction, sizeof(RegOpenAction) / sizeof(LCD_SIG_ACTION_T));
    CDD_Lcd_SigModeDef(E_ICO_MODE_SLOW_BLINK,
                       RegSlowBlinkAction,
                       sizeof(RegSlowBlinkAction) / sizeof(LCD_SIG_ACTION_T));
    CDD_Lcd_SigModeDef(E_ICO_MODE_FAST_BLINK,
                       RegFastBlinkAction,
                       sizeof(RegFastBlinkAction) / sizeof(LCD_SIG_ACTION_T));
    CDD_Lcd_SigModeDef(E_ICO_MODE_SELF_BLINK1,
                       RegSelfBlink1Action,
                       sizeof(RegSelfBlink1Action) / sizeof(LCD_SIG_ACTION_T));
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_Run
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD屏运行控制汇总进程
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       10ms周期（为使得LED和LCD屏相关的闪烁同频，调用周期统一10ms）
 */
void CDD_Lcd_Run(void) {
    /** 开关机动画控制 */
    CDD_Lcd_MovieCtrlTask();

    /** 充电跑马进度条控制 */
    CDD_Lcd_SocPerCtrlTask();

    /** 遍历段码屏所有段码的工作模式 */
    CDD_Lcd_IconCtrlTask();

    /** 更新数据到扫描数组 */
    CDD_Lcd_Hw_SetScanData((uint8_t *)&LcdScanData, sizeof(LcdScanData));
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_Clear
 -----------------------------------------------------------------------------*/
/**
 * @brief      清除屏幕显示
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_Clear(void) {
    (void)memset((void *)&LcdScanData, 0, sizeof(LcdScanData));
    (void)memset((void *)&LcdIcoModeSet, 0, sizeof(LcdIcoModeSet));
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SetBrightSta
 -----------------------------------------------------------------------------*/
/**
 * @brief      背光灯控制
 * @details    N/A
 * @param[in]  BriSta：亮、灭
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_SetBrightSta(LCD_BRIGHT_MODE_E BriSta) {
    if (BriSta == E_BRIGHT_CLOSE) {
        CDD_Lcd_Hw_SetBriSta(E_LCD_BRIGHT_CLOSE);
    } else {
        CDD_Lcd_Hw_SetBriSta(E_LCD_BRIGHT_OPEN);
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_TimIsrCallback
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD屏中断回调函数
 * @details    底层驱动发送时序、开关机动画、显示控制等；
 *             （考虑到有些工况，在上电初始化时间可能会长，如大量读取EEPROM，
 *             开关机动画等一些需要立马执行的进程控制放入到定时器中断中）
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       50us级定时器中断
 */
void CDD_Lcd_TimIsrCallback(void) {
    static uint32_t TimeCnt = 0;
    static uint32_t RunTime = (uint32_t)(LCD_RUN_CYC * 1000) / LCD_TIM_CYC;

    /** 10ms周期 */
    if ((TimeCnt++) >= RunTime) {
        TimeCnt = 0;
        CDD_Lcd_Hw_ScanData();
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SetNumberMode
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD数字断码管段码显示模式设定
 * @details    N/A
 * @param[in]  NumIndex：数码管编号
 *             Data：显示数字：0 - 9，及一些特殊符号
 *             Mode：显示模式：亮、灭、闪
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_SetNumberMode(LCD_NUMBER_ID_E NumIndex, LCD_NUM_SHOW_E Data, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(Data < LCD_REG_LEN);
    LS_ASSERT(Mode < E_ICO_MODE_MAX);
    LS_ASSERT(NumIndex < E_NUM_MAX);

    uint8_t RegTemp;
    uint8_t i;

    /** 取对应的段码表 */
    RegTemp = TubeRegTable[Data];

    /** 按照数字段码排序为：ABCDEFG顺序扫描 */
    for (i = 0; i < 7; i++) {
        if ((RegTemp & 0x80) > 0) {
            CDD_TubeSegBitMode_Set(NumIndex, (LCD_TUBE_REG_E)i, Mode);
        } else {
            CDD_TubeSegBitMode_Set(NumIndex, (LCD_TUBE_REG_E)i, E_ICO_MODE_CLOSE);
        }

        RegTemp <<= 1;
    }
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SetSocPerMode
 -----------------------------------------------------------------------------*/
/**
 * @brief      设定充电进度百分条工作模式，及显示百分比
 * @details    N/A
 * @param[in]  Mode：工作模式：常显，充电跑马
 *             SocPer：SOC百分比值（0 - 100）
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_SetSocPerMode(uint8_t SocPer, LCD_PER_MODE_E Mode) {
    LcdSocPerCtrl.Mode = Mode;
    LcdSocPerCtrl.SocValue = SocPer;
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SetSigIcoMode
 -----------------------------------------------------------------------------*/
/**
 * @brief      lCD段码屏中独立图标工作模式设定
 * @details    N/A
 * @param[in]  IcoIndex：独立图标枚举（与段码位置一一对应）
 *             Mode：工作模式：亮、灭、闪
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_SetSigIcoMode(LCD_ICO_ID_E IcoIndex, LCD_ICO_MODE_E Mode) {
    LS_ASSERT(Mode < E_ICO_MODE_MAX);
    LS_ASSERT(IcoIndex < ICON_ID_MAX);

    uint8_t Pos = IcoIndex / 8;
    uint8_t BitPos = (IcoIndex % 8) * 4;

    LcdIcoModeSet.Data[Pos] &= ~((uint32_t)0x0000000F << BitPos);
    LcdIcoModeSet.Data[Pos] |= (((uint32_t)0x0000000F & Mode) << BitPos);
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_SetMovie
 -----------------------------------------------------------------------------*/
/**
 * @brief      开启开、关机界面
 * @details    N/A
 * @param[in]  MovieSel：动画选择（开机、关机）
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_SetMovie(LCD_MOVIE_SEL_E MovieSel) {
    LcdMovieCtrl.MovieSel = MovieSel;
}
