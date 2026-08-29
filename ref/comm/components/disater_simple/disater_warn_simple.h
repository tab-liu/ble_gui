/**
  ******************************************************************************
  * @file      disater_warn_simple.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/06/25
  * @brief     灾害预警
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/1/14  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>

/* ======================== 本地模块文件引用（可选） ============================ */


/* ================================ 头文件宏定义 ================================ */

// 灾害预警非标协议头及功能码
#define DISATER_MODE_FUNC_CODE  0x04
#define DISATER_PROTOCOL_HEADER	4 		

/* ============================== 头文件结构体定义 ================================ */

#pragma pack(1)

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief      风暴预警响应
  * @param[in]  uint8_t *buff  
                int len        
  * @param[out] None
  * @return     int
  */
int disater_warn_mode_handle(uint8_t *buff, int len);

/**
  * @brief      风暴预警备电任务
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void disater_warn_process_task(void);

#ifdef __cplusplus
}
#endif
