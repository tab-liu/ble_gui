/**
  ******************************************************************************
  * @file      general_build_get.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/7/25
  * @brief     脚本编译更新文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/7/25  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_log.h"

#include "build_date.h"

#if BUILD_TIMESTAMP > 4294967295
    #define IOT_CODE_BUILD_TIMESTAMP_U32 (BUILD_TIMESTAMP % 100000000)
#else
    #define IOT_CODE_BUILD_TIMESTAMP_U32 BUILD_TIMESTAMP
#endif


/*------------------------------------------------------------------------------
 Function: Get_IoT_Code_Build_U32
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取内部管控号
  * @param[in]  void  
  * @param[out] None
  * @return     uint32_t
  */
uint32_t Get_IoT_Code_Build_U32(void) {
    return (uint32_t)IOT_CODE_BUILD_TIMESTAMP_U32;
}

/*------------------------------------------------------------------------------
 Function: Get_IoT_Build_Time_U32
 -----------------------------------------------------------------------------*/
/**
  * @brief      获取IOT编译时间戳
  * @param[in]  void  
  * @param[out] None
  * @return     uint32_t
  */
uint32_t Get_IoT_Build_Time_U32(void) {
    return (uint32_t)BUILD_LOCAL_TS_SEC;
}

