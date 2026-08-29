/**
  ******************************************************************************
  * @file      udt_incremental.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/2/27
  * @brief     数据通道增量读取逻辑模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/2/27  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */
 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"

#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "utils.h"
#include "udt_incremental.h"
#include "comm_define.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[UDT_INCREMENTAL]"

/* =============================== 文件内全局变量 ================================ */

// 增量读取记录文件最大个数
static USE_EXT_RAM_BSS udt_file_record_t udt_incremental_record[UDT_READ_RECORD_FILE_NUM_MAX] = {0};

// 增量读取记录生成互斥信号量
static SemaphoreHandle_t xUdtIncrementalSemaphore = NULL;

/* ============================== 文件内引用外部接口 ============================= */


/* ================================ 模块函数定义 ================================ */

/* 静态/运行时初始化 udt_file_record_t 的辅助函数 */
udt_file_record_t udt_record_init_from(const char *fname, time_t last_time, uint16_t last_send_cnt, uint16_t last_resp_cnt)
{
    udt_file_record_t rec;
    memset(&rec, 0, sizeof(rec)); /* 清零包含 reserved */

    if (fname != NULL) {
        size_t n = strnlen(fname, 10); /* 最多拷贝 10 字节，不要求终止符 */
        memcpy(rec.fname, fname, n);
    }

    rec.last_time = (uint32_t)last_time;
    rec.last_send_cnt = last_send_cnt;
    rec.last_resp_cnt = last_resp_cnt;

    return rec;
}

/**
  * @brief      UDT 增量读取模块初始化
  * @note       建议在系统上电后、相关任务启动前调用。本函数会：
  *                - 初始化本地增量记录数组为0；
  *                - 创建用于保护增量记录/文件操作的互斥信号量（若尚未创建）；
  *                - 从文件读取增量记录到内存，若读取长度不符则写入默认（全0）记录文件。
  *                - 在访问文件时会获取互斥锁（超时 1000 ms）。
  * @param[in]  void
  * @param[out] None
  * @return
  *  -  0 : 成功
  *  - -1 : 失败，可能原因包括信号量创建失败、互斥获取超时或文件读写错误
  */
int Udt_Incremental_Module_Init(void)
{
    char path[40] = {0}; // 为文件路径分配足够空间

    /* 初始化增量读取记录数组（保证为0） */
    memset(udt_incremental_record, 0, sizeof(udt_incremental_record));

    /* 增量读取记录生成互斥信号量 */
    if (xUdtIncrementalSemaphore == NULL) {
        xUdtIncrementalSemaphore = xSemaphoreCreateMutex();
        if (xUdtIncrementalSemaphore == NULL) {
            ESP_LOGE(TAG, "xUdtIncrementalSemaphore Create failed");
            return -1;
        }
    }

    // 获取增量读取记录日志文件名
    UDT_READ_RECORD_FILE_PATH_IOT(path);
    
    /* 在访问文件前获取互斥，防止并发文件操作 */
    if (xSemaphoreTake(xUdtIncrementalSemaphore, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Take xUdtIncrementalSemaphore failed");
        return -1;
    }

    // 尝试读取整个记录数组到本地缓存
    int bytes_read = fs_file_read_at(path, 0, (uint8_t*)udt_incremental_record, sizeof(udt_incremental_record));
    
    // 检查读取是否成功
    if (bytes_read != sizeof(udt_incremental_record)) 
    {
        // 将新的（全0）记录写入文件
        if (fs_record_write(path, udt_incremental_record, 0, 0, UDT_READ_RECORD_MAX_LEN, UDT_READ_RECORD_FILE_NUM_MAX) == 0) {
            ESP_LOGI(TAG, "UDT incremental record file initialized and written successfully.");
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write UDT incremental record file!");
            xSemaphoreGive(xUdtIncrementalSemaphore);
            return -1; // 写入失败是致命错误
        }
    }

    xSemaphoreGive(xUdtIncrementalSemaphore);
    return 0;
}

/**
  * @brief    在 udt_incremental_record 数组中查找与 fname 相同的记录并返回序号
  * @param[in] fname  长度为10的文件名（不保证以'\0'终止）
  * @return   >=0: 找到的索引； -1: 未找到或发生错误（包括互斥获取失败）
  */
int Udt_Incremental_Find_Index(const uint8_t fname[10])
{
    if (fname == NULL) {
        return -1;
    }

    /* 若有互斥量则保护访问 */
    if (xUdtIncrementalSemaphore) {
        if (xSemaphoreTake(xUdtIncrementalSemaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG, "Take xUdtIncrementalSemaphore failed");
            return -1;
        }
    }

    for (int i = 0; i < UDT_READ_RECORD_FILE_NUM_MAX; ++i) {
        if (memcmp(udt_incremental_record[i].fname, fname, 10) == 0) {
            if (xUdtIncrementalSemaphore) xSemaphoreGive(xUdtIncrementalSemaphore);
            return i;
        }
    }

    if (xUdtIncrementalSemaphore) xSemaphoreGive(xUdtIncrementalSemaphore);
    return -1;
}

/**
  * @brief    根据索引返回指向 udt_file_record_t 的指针
  * @note     返回的指针指向静态数组，调用者在并发访问时应自行保证同步（或通过模块接口保护）。
  * @param[in] index 要查找的索引（0 ~ UDT_READ_RECORD_FILE_NUM_MAX-1）
  * @return   指向对应记录的指针；索引越界或互斥获取失败时返回 NULL
  */
udt_file_record_t* Udt_Incremental_Get_Record_By_Index(int index)
{
    if (index < 0 || index >= UDT_READ_RECORD_FILE_NUM_MAX) {
        return NULL;
    }

    return &udt_incremental_record[index];
}

/**
  * @brief    使用输入记录更新 udt_incremental_record 数组
  * @note     行为：
  *             - 若数组中存在 fname 相同的记录，则覆盖该记录；
  *             - 否则查找 fname 为空（全0）的槽位写入；
  *             - 若无空位，则覆盖 last_time 最小的记录。
  *           本函数对数组访问做互斥保护。
  * @param[in] new_rec 指向要写入的记录（不得为 NULL）
  * @return   >=0: 更新/插入的索引； -1: 参数错误或互斥获取失败
  */
int Udt_Incremental_Update_Record(const udt_file_record_t *new_rec)
{
    if (new_rec == NULL) {
        return -1;
    }

    if (xUdtIncrementalSemaphore) {
        if (xSemaphoreTake(xUdtIncrementalSemaphore, pdMS_TO_TICKS(200)) != pdTRUE) {
            ESP_LOGE(TAG, "Take xUdtIncrementalSemaphore failed");
            return -1;
        }
    }

    int found_idx = -1;
    /* 1) 查找 fname 相同的记录 */
    for (int i = 0; i < UDT_READ_RECORD_FILE_NUM_MAX; ++i) {
        if (memcmp(udt_incremental_record[i].fname, new_rec->fname, 10) == 0) {
            found_idx = i;
            break;
        }
    }

    /* 2) 若未找到相同 fname，则查找空槽（fname 全 0） */
    if (found_idx == -1) {
        for (int i = 0; i < UDT_READ_RECORD_FILE_NUM_MAX; ++i) {
            bool empty = true;
            for (int j = 0; j < 10; ++j) {
                if (udt_incremental_record[i].fname[j] != 0) {
                    empty = false;
                    break;
                }
            }
            if (empty) {
                found_idx = i;
                break;
            }
        }
    }

    /* 3) 若仍未找到，则选择 last_time 最小的索引覆盖 */
    if (found_idx == -1) {
        uint32_t min_time = UINT32_MAX;
        int min_idx = 0;
        for (int i = 0; i < UDT_READ_RECORD_FILE_NUM_MAX; ++i) {
            if (udt_incremental_record[i].last_time < min_time) {
                min_time = udt_incremental_record[i].last_time;
                min_idx = i;
            }
        }
        found_idx = min_idx;
    }

    /* 覆盖/写入记录 */
    memcpy(udt_incremental_record[found_idx].fname, new_rec->fname, sizeof(udt_incremental_record[found_idx].fname));
    udt_incremental_record[found_idx].last_send_cnt = new_rec->last_send_cnt;
    udt_incremental_record[found_idx].last_resp_cnt = new_rec->last_resp_cnt;
    if ( new_rec->last_send_cnt == new_rec->last_resp_cnt ) {
        // 仅在上次服务器全部接收成功时更新时间戳，否则只更新发送记录，等待下次断点重传
        udt_incremental_record[found_idx].last_time = new_rec->last_time;
    }

    /* 将修改写入文件 */
    char path[40] = {0};
    UDT_READ_RECORD_FILE_PATH_IOT(path);

    /* 写入单条记录（从 found_idx 开始，写入 1 条） */
    if (fs_record_write(path, &udt_incremental_record[found_idx], 0, found_idx, UDT_READ_RECORD_MAX_LEN, 1) == 0) {
        ESP_LOGI(TAG, "UDT incremental record index %d written to file, name: %.*s", 
            found_idx, 10, (const char *)udt_incremental_record[found_idx].fname);
    } else {
        ESP_LOGE(TAG, "FATAL: Failed to write UDT incremental record index %d!", found_idx);
        if (xUdtIncrementalSemaphore) xSemaphoreGive(xUdtIncrementalSemaphore);
        return -1;
    }

    if (xUdtIncrementalSemaphore) {
        xSemaphoreGive(xUdtIncrementalSemaphore);
    }

    return found_idx;
}

