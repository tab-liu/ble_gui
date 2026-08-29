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
#define LOG_TAG "Lcd_Hw_Cn91c4"

/* Private includes (#include "..\xxx\xx.h")----------------------------------*/
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h> // 添加malloc支持

#include "Lcd_Hw_cn91c4.h"
#include "ls_dbg.h"
#include "ls_hal_i2c.h"
#include "platform.h"
#include "log.h"
#include "iot_iic.h"

/* Private macros (#define)---------------------------------------------------*/

#define CN91_DEV_WR_ADDR (0x3E)

extern I2C_HandleTypeDef I2cHandle;

// 修复LCD的I2C通信方式 - 根据CN91C4协议正确实现
static int Cn91c4sIntWriteData(uint8_t cmd, uint8_t cmd_len, uint8_t *data_buf, uint8_t data_len) {
    if (cmd_len > 0) {
        if (iot_i2c_write_blocking(CN91_DEV_WR_ADDR, &cmd, 1) != 0) {
            // LOG_E("Failed to send LCD command: 0x%02X", cmd);
            return -1;
        }
        // LOG_D("LCD command sent: 0x%02X", cmd);
    }

    // 如果有数据需要发送
    if (data_len > 0 && data_buf != NULL) {
        // 对于显示数据，需要先发送地址命令0xE800，然后发送数据

        uint8_t *full_packet = malloc(2 + data_len);
        if (full_packet == NULL) {
            LOG_E("Memory allocation failed for LCD data packet");
            return -1;
        }

        full_packet[0] = 0xE8;
        full_packet[1] = 0x00;
        memcpy(&full_packet[2], data_buf, data_len);

        int result = iot_i2c_write_blocking(CN91_DEV_WR_ADDR, full_packet, 2 + data_len);

        free(full_packet);

        if (result != 0) {
            // LOG_E("Failed to send LCD data, length: %d", data_len);
            return -1;
        }
        // LOG_D("LCD data sent successfully, length: %d", data_len);
    }

    return 0;
}

#define CN_CMDQUE_MAX 40
#define CN_DATABUF_MAX 100
#define HT_DATABUF_MAX 100

/* Private types (typedef) ---------------------------------------------------*/
#ifdef CCS
#else
#pragma pack(push, 1) /*内存单字节对齐*/
#endif

enum {
    E_STEP_IDEL,
    E_STEP_CMD,
    E_STEP_DATA,
};

typedef struct {
    uint16_t Head;
    uint16_t Tail;
    uint8_t CmdBuf[CN_CMDQUE_MAX];
} LCD_HW_CMD_QUE_T;

typedef struct {
    volatile uint8_t ScanIs;
    volatile uint8_t DataLen;
    volatile uint8_t Data[HT_DATABUF_MAX];
} LCD_HW_SCAN_DATA_T;

typedef union {
    uint8_t All;

    struct {
        uint8_t P0_4 : 5;
        uint8_t ConstCode : 2;
        uint8_t CmdOrData : 1;
    } Bit;
} CN_REG_ADSET_U;

typedef union {
    uint8_t All;

    struct {
        uint8_t Sr : 2;
        uint8_t Lf : 1;
        uint8_t Fr : 2;
        uint8_t ConstCode : 2;
        uint8_t CmdOrData : 1;
    } Bit;
} CN_REG_DISCTL_U;

typedef union {
    uint8_t All;

    struct {
        uint8_t NC : 3;
        uint8_t En : 1;
        uint8_t Ulp : 1;
        uint8_t ConstCode : 2;
        uint8_t CmdOrData : 1;
    } Bit;

} CN_REG_MODSET_U;

typedef union {
    uint8_t All;

    struct {
        uint8_t Ev2_0 : 3;
        uint8_t ConstCode : 4;
        uint8_t CmdOrData : 1;
    } Bit;

} CN_REG_EVRSET_U;

typedef union {
    uint8_t All;

    struct {
        uint8_t P6 : 1;
        uint8_t Rst : 1;
        uint8_t P5 : 1;
        uint8_t ConstCode : 4;
        uint8_t CmdOrData : 1;
    } Bit;

} CN_REG_ICSET_U;

typedef union {
    uint8_t All;

    struct {
        uint8_t Bf : 3;
        uint8_t ConstCode : 4;
        uint8_t CmdOrData : 1;
    } Bit;

} CN_REG_BLKCTL_U;

typedef union {
    uint8_t All;

    struct {
        uint8_t Aof : 1;
        uint8_t Aon : 1;
        uint8_t Ev3 : 1;

        uint8_t ConstCode : 4;
        uint8_t CmdOrData : 1;
    } Bit;

} CN_REG_APCTL_U;

typedef struct {
    CN_REG_ADSET_U Adset;
    CN_REG_DISCTL_U Disctl;
    CN_REG_MODSET_U Modset;
    CN_REG_EVRSET_U Evrset;
    CN_REG_ICSET_U Icset;
    CN_REG_BLKCTL_U Blkctl;
    CN_REG_APCTL_U Apctl;
} CN_REG_T;

#ifdef CCS
#else
#pragma pack(pop)
#endif

/* Private function prototype declarations------------------------------------*/

/* Private variable definitions-----------------------------------------------*/
static LCD_HW_CMD_QUE_T LcdHwCmdQue = {0};
static LCD_HW_SCAN_DATA_T LcdHwScanData = {0};
static CN_REG_T CnConfigReg = {0};

/* Exported variable definitions----------------------------------------------*/

/* Private constant definitions (const)---------------------------------------*/

/* Exported constant definitions (const)--------------------------------------*/

/* Function definitions ------------------------------------------------------*/

/*------------------------------------------------------------------------------
 Function: PollCn91c4CmdQue
 -----------------------------------------------------------------------------*/
/**
 * @brief      Cn91c4驱动命令出队列
 * @details    N/A
 * @param[in]  N/A
 * @param[out] *Cmd：
 * @return     ！0：队列非空，出队列成功
 * @note       N/A
 */
static uint8_t PollLcdHwCmdQue(uint8_t *Cmd) {
    uint8_t PollSuccess = 0;

    LS_ASSERT(Cmd);

    if (LcdHwCmdQue.Tail != LcdHwCmdQue.Head) {
        *Cmd = LcdHwCmdQue.CmdBuf[LcdHwCmdQue.Tail];
        LcdHwCmdQue.Tail = (LcdHwCmdQue.Tail + 1) % CN_CMDQUE_MAX;
        PollSuccess = 1;
        // LOG_I("PollLcdHwCmdQue: Cmd = 0x%02X", *Cmd);
    }
    // LOG_I("PollLcdHwCmdQue: Cmd = 0x%02X", *Cmd);
    // LOG_I("PollLcdHwCmdQue: Head = %d, Tail = %d", LcdHwCmdQue.Head, LcdHwCmdQue.Tail);
    // LOG_I("PollLcdHwCmdQue: PollSuccess = %d", PollSuccess);

    return PollSuccess;
}

/*------------------------------------------------------------------------------
 Function: PushCn91c4CmdQue
 -----------------------------------------------------------------------------*/
/**
 * @brief      CN91C4驱动命令进队列
 * @details    N/A
 * @param[in]  Cmd：
 * @param[out] N/A
 * @return     ！0：队列非满，进队列成功
 * @note       N/A
 */
static uint8_t PushLcdHwCmdQue(uint8_t Cmd) {
    uint8_t PushSuccess = 0;

    if (((LcdHwCmdQue.Head + 1) % CN_CMDQUE_MAX) != LcdHwCmdQue.Tail) {
        LcdHwCmdQue.CmdBuf[LcdHwCmdQue.Head] = Cmd;
        LcdHwCmdQue.Head = (LcdHwCmdQue.Head + 1) % CN_CMDQUE_MAX;

        PushSuccess = 1;
    }

    return PushSuccess;
}

/*------------------------------------------------------------------------------
 Function: Lcd_Hw_SetFR
 -----------------------------------------------------------------------------*/
/**
 * @brief      设置LCD驱动IC刷屏率
 * @details    N/A
 * @param[in]  FrCmd：
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void CDD_Lcd_Hw_SetFR(CN91_SET_CMD_FR_E FrCmd) {
    CnConfigReg.Disctl.Bit.CmdOrData = 1;
    CnConfigReg.Disctl.Bit.ConstCode = 1;
    CnConfigReg.Disctl.Bit.Fr = (uint8_t)FrCmd & 0x03;

    (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Disctl.All);
}

/*------------------------------------------------------------------------------
 Function: Lcd_Hw_SetSR
 -----------------------------------------------------------------------------*/
/**
 * @brief      设置LCD驱动IC内置偏置电流
 * @details    N/A
 * @param[in]  SrCmd：
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void CDD_Lcd_Hw_SetSR(CN91_SET_CMD_SR_E SrCmd) {
    CnConfigReg.Disctl.Bit.CmdOrData = 1;
    CnConfigReg.Disctl.Bit.ConstCode = 1;
    CnConfigReg.Disctl.Bit.Sr = (uint8_t)SrCmd & 0x03;

    (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Disctl.All);
}

/*------------------------------------------------------------------------------
 Function: Lcd_Hw_SetEV
 -----------------------------------------------------------------------------*/
/**
 * @brief      设置LCD驱动IC显示对比度
 * @details    N/A
 * @param[in]  EvCmd：
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
static void CDD_Lcd_Hw_SetEV(CN91_SET_CMD_EV_E EvCmd) {
    CnConfigReg.Evrset.Bit.CmdOrData = 1;
    CnConfigReg.Evrset.Bit.ConstCode = 0xC;
    CnConfigReg.Evrset.Bit.Ev2_0 = (uint8_t)(EvCmd & 0x07);

    CnConfigReg.Apctl.Bit.CmdOrData = 1;
    CnConfigReg.Apctl.Bit.ConstCode = 0xf;
    CnConfigReg.Apctl.Bit.Ev3 = ((EvCmd & 0x08) > 0 ? 1 : 0);

    (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Evrset.All);
    (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Apctl.All);
}

/*------------------------------------------------------------------------------
 Function: CDD_Lcd_Hw_SetEN
 -----------------------------------------------------------------------------*/
/**
 * @brief      设置LCD使能位
 * @details    设置MODSET寄存器的EN位来使能LCD
 * @param[in]  enable: 1=使能LCD, 0=禁用LCD
 * @param[out] N/A
 * @return     N/A
 * @note       这是LCD正常工作的必要条件
 */
void CDD_Lcd_Hw_SetEN(uint8_t enable) {
    CnConfigReg.Modset.Bit.CmdOrData = 1;
    CnConfigReg.Modset.Bit.ConstCode = 0x2; // MODSET寄存器的固定码
    CnConfigReg.Modset.Bit.NC = 0; // 保留位设为0
    CnConfigReg.Modset.Bit.Ulp = 0; // 超低功耗模式关闭
    CnConfigReg.Modset.Bit.En = enable ? 1 : 0; // 设置使能位

    // LOG_I("Setting LCD EN bit to: %d, MODSET=0x%02X", enable, CnConfigReg.Modset.All);

    /** 发送两次（增加安全冗余） */
    (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Modset.All);
    (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Modset.All);
}

/*------------------------------------------------------------------------------
 Function: Lcd_Hw_SetBriSta
 -----------------------------------------------------------------------------*/
/**
 * @brief      Lcd背光灯设置（或LCD像素点状态使能、关闭）
 * @details    N/A
 * @param[in]  Sta：
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_Hw_SetBriSta(LCD_HW_BRIGHT_STA_E Sta) {
    CnConfigReg.Apctl.Bit.CmdOrData = 1;
    CnConfigReg.Apctl.Bit.ConstCode = 0xf;
    CnConfigReg.Apctl.Bit.Aon = 0;

    static LCD_HW_BRIGHT_STA_E LastSta = E_LCD_BRIGHT_OPEN;

    if (LastSta != Sta) {
        LastSta = Sta;

        switch (Sta) {
        case E_LCD_BRIGHT_CLOSE:

            CnConfigReg.Apctl.Bit.Aof = 1;
            break;

        case E_LCD_BRIGHT_OPEN:
            CnConfigReg.Apctl.Bit.Aof = 0;
            break;

        default:
            CnConfigReg.Apctl.Bit.Aof = 1;
            break;
        }

        /** 发送两次（增加安全冗余） */
        (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Apctl.All);
        (void)PushLcdHwCmdQue((uint8_t)CnConfigReg.Apctl.All);
    }
}

/*------------------------------------------------------------------------------
 Function: Lcd_Hw_SetScanData
 -----------------------------------------------------------------------------*/
/**
 * @brief      lcd显示内容扫描数据更新
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */

void CDD_Lcd_Hw_SetScanData(const uint8_t *Src, uint8_t Len) {
    LS_ASSERT((Len < HT_DATABUF_MAX) && (Src != NULL));

    /** 有数据无变化，更新 */
    if (memcmp((void *)&LcdHwScanData.Data, (void *)Src, Len) != 0) {
        // LOG_I("set scan data changed");
        (void)memcpy((void *)&LcdHwScanData.Data, (void *)Src, Len);

        LcdHwScanData.DataLen = Len;
        LcdHwScanData.ScanIs = 1;
    }
}

/*------------------------------------------------------------------------------
 Function: Lcd_Hw_Init
 -----------------------------------------------------------------------------*/
/**
 * @brief      lcd驱动IC初始化配置
 * @details    N/A
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       N/A
 */
void CDD_Lcd_Hw_Init(void) {
    (void)memset((void *)&CnConfigReg, 0, sizeof(CnConfigReg));

    // LOG_I("LCD INIT START");

    /** 刷屏率 */
    CDD_Lcd_Hw_SetFR(E_CN_FR_72HZ);

    /** 偏置电流 */
    CDD_Lcd_Hw_SetSR(E_CN_SR_BIAS_100);

    /** 对比度 */
    CDD_Lcd_Hw_SetEV(E_CN_EV_LEVEL_0);

    /** 使能LCD - 这是关键的缺失设置！ */
    CDD_Lcd_Hw_SetEN(1);

    /** 开启显示 */
    CDD_Lcd_Hw_SetBriSta(E_LCD_BRIGHT_OPEN);

    // LOG_I("LCD INIT END");
}

/*------------------------------------------------------------------------------
 Function: Lcd_Hw_ScanData
 -----------------------------------------------------------------------------*/
/**
 * @brief      LCD屏显示内容及命令控制发送（驱动IC：Cn91c4）
 * @details    调用BSW层的IIC模块（中断模拟），实现数据的发送
 * @param[in]  N/A
 * @param[out] N/A
 * @return     N/A
 * @note       10ms周期
 */
void CDD_Lcd_Hw_ScanData(void) {
    static uint8_t ScanStep = 0;
    static uint8_t ScanCmd = 0;

    // LOG_I("CDD_Lcd_Hw_ScanData: ScanStep = %d", ScanStep);
    switch (ScanStep) {
    /** 获取指令 */
    case E_STEP_IDEL:
        /** 写控制命令（优先） */
        if (PollLcdHwCmdQue(&ScanCmd)) {
            ScanStep = E_STEP_CMD;
        }
        /** 写入更新数据 */
        else if (LcdHwScanData.ScanIs == 1) {
            ScanStep = E_STEP_DATA;
        }
        break;

    /** 发送控制设置命令 */
    case E_STEP_CMD:
        // 发送单个命令字节
        if (Cn91c4sIntWriteData(ScanCmd, 1, NULL, 0) == 0) {
            // LOG_D("LCD command sent successfully: 0x%02X", ScanCmd);
        } else {
            // LOG_E("LCD command failed: 0x%02X", ScanCmd);
        }
        ScanStep = E_STEP_IDEL;
        break;

    /** 发送扫描数据 */
    case E_STEP_DATA:
        // 根据CN91C4协议，发送显示数据（包含地址0xE800）
        if (Cn91c4sIntWriteData(0, 0, (uint8_t *)LcdHwScanData.Data, LcdHwScanData.DataLen) == 0) {
            // LOG_D("LCD data sent successfully, length: %d", LcdHwScanData.DataLen);
            LcdHwScanData.ScanIs = 0; // 清除数据发送标志
        } else {
            // LOG_E("LCD data transmission failed");
        }
        ScanStep = E_STEP_IDEL;
        break;

    default:
        ScanStep = 0;
        break;
    }
}
