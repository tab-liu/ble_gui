/**
  ******************************************************************************
  * @file      energy_process.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/14
  * @brief     能量本地存储模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/26  <td>1.0     <td>lixingyu   <td>Create the initial version
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
#include <stdbool.h>
#include <time.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "energy_process.h"
#include "filesystem.h"
#include "utils.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[ENERGY_PROCESS]"

/* =============================== 文件内全局变量 ================================ */

// 当前文件头本地缓存
static USE_EXT_RAM_BSS energy_file_header_t Energy_File_Header = {0};

// 能量数据文件操作互斥信号量
static SemaphoreHandle_t xEnergyFileProcessSemaphore = NULL;

// 固定时间存储，存储时间点取决于每小时存储次数
static USE_EXT_RAM_BSS uint16_t Time_Hour_Bias_Sec[ENERGY_HOUR_PERIOD_CNT] = {0};

// 本地记录缓冲区，用于暂存从队列中拉取的数据，以实现批量写入。
static USE_EXT_RAM_BSS energy_file_record_unit_t Energy_Unit_Data[ENERGY_UNIT_PROCESS_MAX_NUM] = {0};

// 能量节点记录补0标志
static bool Energy_File_Unit_Empty_Flag = false;

// 能量记录补0条数
static uint16_t Energy_File_Unit_Empty_Cnt = 0;

/* ============================== 文件内引用外部接口 ============================= */

#if 1   // 引用其他文件时可选择于该文件内实现，这里不定义

/**
 * @brief 从文件的指定偏移处读取指定长度的数据。
 *
 * @param path 文件路径。
 * @param offset 读取操作的起始偏移地址（字节）。
 * @param data 用于存储读取数据的缓冲区指针。
 * @param size 希望读取的数据长度（字节）。
 * @return int 成功时返回实际读取到的字节数。如果发生错误（如文件无法打开）则返回-1。
 *         如果读取的起始偏移超出了文件末尾，将返回0。
 */
extern int fs_file_read_at(const char *path, uint32_t offset, uint8_t *data, int size);

/**
 * @brief 将数据写入文件的指定偏移处。
 *
 * 如果文件不存在，将会被创建。如果文件存在，则从指定偏移处开始覆盖写入，
 * 文件其他部分的数据保持不变。
 *
 * @param path 文件路径。
 * @param offset 写入操作的起始偏移地址（字节）。
 * @param data 指向要写入数据的缓冲区的指针。
 * @param size 希望写入的数据长度（字节）。
 * @return int 成功时返回实际写入的字节数。如果发生错误则返回-1。
 */
extern int fs_file_write_at(const char *path, uint32_t offset, const uint8_t *data, int size);

/**
 * @brief 将一条记录写入文件。
 * @param path 文件路径
 * @param record 数据指针
 * @param start_offset 开始写入的偏移地址
 * @param record_index 要写入的记录索引。如果为 FILE_APPEND_MODE，则在文件末尾追加。
 * @param record_len 单条记录的长度
 * @param record_cnt 要写入的记录数量
 * @return 成功返回0, 失败返回-1 
 */
extern int fs_record_write(const char *path, void *record, uint32_t start_offset, uint16_t record_index, uint16_t record_len, uint16_t record_cnt);

/**
 * @brief 分配并清零一段内存
 *
 * @details 这是一个内存分配的封装函数。
 *          通过预编译宏，它可以选择使用 ESP-IDF 特定的 `heap_caps_calloc`
 *          从 PSRAM (SPIRAM) 中分配内存，或者回退到标准的 `calloc` 函数。
 *          无论使用哪种方式，分配的内存都会被自动初始化为零。
 *
 * @param size 要分配的内存大小（以字节为单位）。
 *
 * @return void* 成功时返回指向已分配并清零的内存块的指针。
 *             如果内存分配失败，则返回 NULL。
 */
extern void *iot_calloc(size_t size);

#else

static SemaphoreHandle_t xFileSemaphore = NULL;

/**
 * @brief 递归创建文件路径中的所有目录
 * @param path 文件路径
 * @return int 成功返回0，失败返回-1
 */
static int fs_mkdirs(const char *path)
{
    char tmp[256];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;

    strcpy(tmp, path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            mkdir(tmp, 0777); // 忽略已存在目录的错误
            *p = '/';
        }
    }
    return 0;
}

/**
 * @brief 从文件的指定偏移处读取指定长度的数据。
 *
 * @param path 文件路径。
 * @param offset 读取操作的起始偏移地址（字节）。
 * @param data 用于存储读取数据的缓冲区指针。
 * @param size 希望读取的数据长度（字节）。
 * @return int 成功时返回实际读取到的字节数。如果发生错误（如文件无法打开）则返回-1。
 *         如果读取的起始偏移超出了文件末尾，将返回0。
 */
static int fs_file_read_at(const char *path, uint32_t offset, uint8_t *data, int size)
{
    // 1. 参数检查
    if (path == NULL || data == NULL || size <= 0) {
        return -1;
    }

    int read_bytes = -1; // 默认返回值为-1，表示错误

    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

    // 2. 以二进制只读模式打开文件
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        xSemaphoreGive(xFileSemaphore);
        return -1; // 文件打开失败
    }

    // 3. 移动文件指针到指定的偏移位置
    if (fseek(file, offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to offset %lu in file: %s", offset, path);
        read_bytes = -1; // fseek 失败
    } else {
        // 4. 从当前位置读取指定大小的数据
        // fread 会返回成功读取的元素个数。如果到达文件末尾，返回值会小于请求的size。
        read_bytes = fread(data, 1, size, file);
        if (read_bytes < size && ferror(file)) {
            // 如果读取的字节数少于预期，检查是否发生了错误
            ESP_LOGE(TAG, "Error occurred while reading file: %s", path);
            read_bytes = -1; // 确认是读取错误，而不是文件末尾
        }
    }

    // 5. 清理并返回
    fclose(file);
    xSemaphoreGive(xFileSemaphore);

    return read_bytes;
}

/**
 * @brief 将数据写入文件的指定偏移处。
 *
 * 如果文件不存在，将会被创建。如果文件存在，则从指定偏移处开始覆盖写入，
 * 文件其他部分的数据保持不变。
 *
 * @param path 文件路径。
 * @param offset 写入操作的起始偏移地址（字节）。
 * @param data 指向要写入数据的缓冲区的指针。
 * @param size 希望写入的数据长度（字节）。
 * @return int 成功时返回实际写入的字节数。如果发生错误则返回-1。
 */
int fs_file_write_at(const char *path, uint32_t offset, const uint8_t *data, int size)
{
    // 1. 参数检查
    if (path == NULL || data == NULL || size <= 0) {
        return -1;
    }

    int written_bytes = -1; // 默认返回值为-1，表示错误
    FILE *file = NULL;

    // 2. 获取文件操作信号量，确保线程安全
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

    // 3. 打开文件
    // 尝试以 "rb+" (读写更新) 模式打开已存在的文件。
    // 这种模式不会在打开时清空文件内容。
    file = fopen(path, "rb+");

    // 如果以 "rb+" 模式打开失败（通常意味着文件不存在），
    // 则尝试以 "wb" (写入) 模式创建新文件。
    if (file == NULL) {
        file = fopen(path, "wb");
        fs_mkdirs(path); // 新增：确保目录存在
    }

    // 如果两种方式都失败，则无法继续
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open or create file for writing: %s", path);
        xSemaphoreGive(xFileSemaphore);
        return -1;
    }

    // 4. 移动文件指针到指定的偏移位置
    if (fseek(file, offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to offset %lu in file: %s", (unsigned long)offset, path);
        written_bytes = -1; // fseek 失败
    } else {
        // 5. 从当前位置写入指定大小的数据
        // fwrite 返回成功写入的元素个数。我们写入 size 个 1 字节的元素。
        written_bytes = fwrite(data, 1, size, file);
        if (written_bytes < size) {
            // 如果写入的字节数少于预期，这通常表示磁盘空间不足或发生其他I/O错误
            ESP_LOGE(TAG, "Error or partial write to file: %s. Expected %d, wrote %d", path, size, written_bytes);
            // 即使是部分写入，也将其视为一种错误，返回-1，让调用者处理
            if (!ferror(file)) {
                // 如果不是标准库报告的错误（可能是磁盘满），也统一返回-1
                written_bytes = -1;
            }
        }
    }

    // 6. 清理并返回
    fclose(file);
    xSemaphoreGive(xFileSemaphore);

    return written_bytes;
}

/**
 * @brief 将一条记录写入文件。
 * @param path 文件路径
 * @param record 数据指针
 * @param start_offset 开始写入的偏移地址
 * @param record_index 要写入的记录索引。如果为 FILE_APPEND_MODE，则在文件末尾追加。
 * @param record_len 单条记录的长度
 * @param record_cnt 要写入的记录数量
 * @return 成功返回0, 失败返回-1 
 */
static int fs_record_write(const char *path, void *record, uint32_t start_offset, uint16_t record_index, uint16_t record_len, uint16_t record_cnt)
{
    if (path == NULL || record == NULL || record_len == 0 || record_cnt == 0) {
        return -1;
    }

    int err = -1;
    FILE *pfile = NULL;

    // 关键：确保文件操作的原子性
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

    // 根据模式选择打开方式
    if (record_index == FILE_APPEND_MODE) {
        // 追加模式
        pfile = fopen(path, "ab"); // "ab" = append binary
    } else {
        // 更新模式
        pfile = fopen(path, "rb+"); // "rb+" = read/write binary, file must exist
    }

    // 如果以更新模式打开失败（可能文件不存在），则以写入模式创建它
    if (pfile == NULL && record_index != FILE_APPEND_MODE) {
        pfile = fopen(path, "wb"); // "wb" = write binary, create or truncate
    }

    if (pfile)
    {
        // 如果不是追加模式，则移动文件指针
        if (record_index != FILE_APPEND_MODE) {
            uint32_t byte_offset = start_offset + record_index * record_len;
            if (0 != fseek(pfile, byte_offset, SEEK_SET)) {
                goto err_cleanup; // fseek 失败
            }
        }

        // 写入数据
        if (fwrite(record, record_len, record_cnt, pfile) == record_cnt) {
            err = 0;
        }
    }

err_cleanup:
    if (pfile) {
        fclose(pfile);
    }

    xSemaphoreGive(xFileSemaphore);

    return err;
}

/**
 * @brief 分配并清零一段内存
 *
 * @details 这是一个内存分配的封装函数。
 *          通过预编译宏，它可以选择使用 ESP-IDF 特定的 `heap_caps_calloc`
 *          从 PSRAM (SPIRAM) 中分配内存，或者回退到标准的 `calloc` 函数。
 *          无论使用哪种方式，分配的内存都会被自动初始化为零。
 *
 * @param size 要分配的内存大小（以字节为单位）。
 *
 * @return void* 成功时返回指向已分配并清零的内存块的指针。
 *             如果内存分配失败，则返回 NULL。
 */
void *iot_calloc(size_t size)
{
#if CONFIG_SPI_RAM_ENABLE
    // 使用 heap_caps_calloc 分配内存并自动初始化为零
    return heap_caps_calloc(size, sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    // 使用 calloc 分配内存并自动初始化为零
    return calloc(size, sizeof(uint8_t));
#endif
}

#endif

/* ================================ 模块函数定义 ================================ */

/**
 * @brief      根据能量项宏定义返回对应的名称宏定义字符串
 * @details    输入能量项宏定义（如 ENERGY_RAW_PV_TOTAL），返回对应的名称宏定义（如 ENERGY_RAW_PV_TOTAL_NAME）。
 *             找不到时返回NULL。
 *
 * @param[in]  raw_index  能量项宏定义值
 * @return     const char* 对应的名称宏定义字符串，找不到时返回NULL
 */
static const char* Energy_Raw_Index_To_Name(uint8_t raw_index)
{
    switch (raw_index) {
        case ENERGY_RAW_PV_TOTAL:              return ENERGY_RAW_PV_TOTAL_NAME;
        case ENERGY_RAW_GRID_INPUT:            return ENERGY_RAW_GRID_INPUT_NAME;
        case ENERGY_RAW_GRID_OUTPUT:           return ENERGY_RAW_GRID_OUTPUT_NAME;
        case ENERGY_RAW_AC_LOAD:               return ENERGY_RAW_AC_LOAD_NAME;
        case ENERGY_RAW_DC_LOAD:               return ENERGY_RAW_DC_LOAD_NAME;
        case ENERGY_RAW_AC_PV:                 return ENERGY_RAW_AC_PV_NAME;
        case ENERGY_RAW_PV_TOTAL_AGAIN:        return ENERGY_RAW_PV_TOTAL_AGAIN_NAME;
        case ENERGY_RAW_BATTERY_CHARGE:        return ENERGY_RAW_BATTERY_CHARGE_NAME;
        case ENERGY_RAW_BATTERY_DISCHARGE:     return ENERGY_RAW_BATTERY_DISCHARGE_NAME;
        case ENERGY_RAW_PV_TO_AC_LOAD:         return ENERGY_RAW_PV_TO_AC_LOAD_NAME;
        case ENERGY_RAW_CAR_CHARGE:            return ENERGY_RAW_CAR_CHARGE_NAME;
        default:                               return NULL;
    }
}

/**
 * @brief 初始化能量项索引映射（结构体成员顺序 -> 实际能量数组索引）
 * @note 只对实际使用的能量项赋值，未使用项可设为0或-1（根据实际需求）。
 */
static void Energy_Index_Map_Init(void)
{
    // 先全部清零或设为无效值
    memset(Energy_File_Header.index, 0, sizeof(Energy_File_Header.index));
    Energy_File_Header.item_total = ENERGY_ITEM_REAL_NUM;

    // 按实际能量项宏定义顺序赋值为原始顺序宏定义
#ifdef ENABLE_ENERGY_RAW_PV_TOTAL
    Energy_File_Header.index[ENERGY_IDX_PV_TOTAL] = ENERGY_RAW_PV_TOTAL;         // 1
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_INPUT
    Energy_File_Header.index[ENERGY_IDX_GRID_INPUT] = ENERGY_RAW_GRID_INPUT;     // 2
#endif

#ifdef ENABLE_ENERGY_RAW_GRID_OUTPUT
    Energy_File_Header.index[ENERGY_IDX_GRID_OUTPUT] = ENERGY_RAW_GRID_OUTPUT;   // 3
#endif

#ifdef ENABLE_ENERGY_RAW_AC_LOAD
    Energy_File_Header.index[ENERGY_IDX_AC_LOAD] = ENERGY_RAW_AC_LOAD;           // 4
#endif

#ifdef ENABLE_ENERGY_RAW_DC_LOAD
    Energy_File_Header.index[ENERGY_IDX_DC_LOAD] = ENERGY_RAW_DC_LOAD;           // 5
#endif

#ifdef ENABLE_ENERGY_RAW_AC_PV
    Energy_File_Header.index[ENERGY_IDX_AC_PV] = ENERGY_RAW_AC_PV;               // 6
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TOTAL_AGAIN
    Energy_File_Header.index[ENERGY_IDX_PV_TOTAL_AGAIN] = ENERGY_RAW_PV_TOTAL_AGAIN; // 7
#endif

#ifdef ENABLE_ENERGY_RAW_BATTERY_CHARGE
    Energy_File_Header.index[ENERGY_IDX_BATTERY_CHARGE] = ENERGY_RAW_BATTERY_CHARGE; // 8
#endif

#ifdef ENABLE_ENERGY_RAW_BATTERY_DISCHARGE
    Energy_File_Header.index[ENERGY_IDX_BATTERY_DISCHARGE] = ENERGY_RAW_BATTERY_DISCHARGE; // 9
#endif

#ifdef ENABLE_ENERGY_RAW_PV_TO_AC_LOAD
    Energy_File_Header.index[ENERGY_IDX_PV_TO_AC_LOAD] = ENERGY_RAW_PV_TO_AC_LOAD; // 10
#endif

#ifdef ENABLE_ENERGY_RAW_CAR_CHARGE
    Energy_File_Header.index[ENERGY_IDX_CAR_CHARGE] = ENERGY_RAW_CAR_CHARGE; // 11
#endif

    // 可继续补充后续保留项
    // 未使用项可不赋值或设为0/无效
}

/*------------------------------------------------------------------------------
 Function: Energy_Module_Init
 -----------------------------------------------------------------------------*/
/**
  * @brief      能量存储模块初始化
  * @note       建议在系统上电后初始化期间执行。
  * @param[in]  void  
  * @param[out] None
  * @return
  *  - 0: 成功，所有资源已成功创建并尝试读取文件头。
  *  - -1: 失败，可能由于参数错误或内存分配失败。
  */
int Energy_Module_Init(void)
{
    char path[30] = {0};

    // 能量数据文件操作互斥信号量
    if (xEnergyFileProcessSemaphore == NULL) {
        xEnergyFileProcessSemaphore = xSemaphoreCreateMutex();
        if (xEnergyFileProcessSemaphore == NULL) {
            ESP_LOGE(TAG, "xEnergyFileProcessSemaphore Create failed");
            return -1;
        }
    }

    // 能量数据文件路径
    ENERGY_FILE_PATH_IOT_MAX(path);

    // 尝试读取文件头
    int bytes_read = fs_file_read_at(path, 0, (uint8_t*)&Energy_File_Header, ENERGY_FILE_HEADER_LEN);

    // 检查读取是否成功，以及文件头内容是否有效
    if (bytes_read != ENERGY_FILE_HEADER_LEN || Energy_File_Header.unit_max_cnt == 0) {
        ESP_LOGW(TAG, "Energy file header read failed or is invalid. Initializing a new one.");

        // 初始化文件头
        memset(&Energy_File_Header, 0, sizeof(energy_file_header_t));
        Energy_File_Header.ver = ENERGY_FILE_VER_V1;
        Energy_File_Header.unit_hour_period = ENERGY_HOUR_PERIOD_CNT;
        Energy_File_Header.unit_bytes = ENERGY_FILE_RECORD_UNIT_LEN;
        Energy_File_Header.unit_max_cnt = ENERGY_UNIT_MAX_NUM;

        // 初始化能量项索引映射
        Energy_Index_Map_Init();

        // 写入新的文件头
        if (fs_file_write_at(path, 0, (uint8_t*)&Energy_File_Header, ENERGY_FILE_HEADER_LEN) == ENERGY_FILE_HEADER_LEN) {
            ESP_LOGI(TAG, "New energy file header initialized and written successfully.");
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write new energy file header!");
            return -1;
        }
    }

    // 每小时内检查节点时间偏移
    for ( uint8_t i = 0 ; i < ENERGY_HOUR_PERIOD_CNT ; i++ ) {
        Time_Hour_Bias_Sec[i] = (i * 3600) / ENERGY_HOUR_PERIOD_CNT;
    }

    return 0;
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Unit_Data_Count_New_Records_By_Timestamp
 -----------------------------------------------------------------------------*/
/**
 * @brief      根据最新记录时间戳和每小时记录数估算新记录数量。
 * @details    通过 Energy_File_Header.utc_timestamp 与 since_timestamp 计算时间差，
 *             并结合 ENERGY_HOUR_PERIOD_CNT 得出新记录数量，最大不超过实际已存记录数。
 *
 * @param[in]  since_timestamp  需要检查的新起点时间戳。
 *
 * @return     uint16_t  新记录数量。
 */
static uint16_t Energy_File_Unit_Data_Count_New_Records(time_t since_timestamp)
{
    // 1. 基本参数和路径检查
    char path[30] = {0};
    ENERGY_FILE_PATH_IOT_MAX(path);

    if (Energy_File_Header.unit_max_cnt == 0 || Energy_File_Header.unit_real_cnt == 0) {
        return 0; // 模块未初始化或文件为空，没有新记录
    }

    // 2. 快速路径检查：处理全量同步的情况
    if (since_timestamp == 0) {
        ESP_LOGI(TAG, "since_timestamp is 0, counting all records.");
        return Energy_File_Header.unit_real_cnt;
    }

    // 3. 根据最新记录时间戳和每小时记录数快速估算新记录数量
    //    假设记录均匀分布，每小时 ENERGY_HOUR_PERIOD_CNT 条
    if (Energy_File_Header.utc_timestamp > since_timestamp) {
        time_t delta_sec = Energy_File_Header.utc_timestamp - since_timestamp;
        uint16_t new_count = (uint16_t)((delta_sec * ENERGY_HOUR_PERIOD_CNT) / 3600);
        // 最大不能超过实际记录数
        if (new_count > Energy_File_Header.unit_real_cnt) {
            new_count = Energy_File_Header.unit_real_cnt;
        }
        return new_count;
    }

    return 0; // 没有找到任何新记录
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Unit_Data_Read_Incremental
 -----------------------------------------------------------------------------*/
/**
 * @brief      读取增量能量数据到动态分配的缓冲区。
 * @details    此函数是实现能量数据增量同步的核心接口。它会：
 *             1. 调用辅助函数 `Energy_File_Unit_Data_Count_New_Records` 计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *             5. 注意，返回的缓冲区大小取决于ENERGY_UNIT_MAX_NUM，
 *                1000条时可达几十K，自行评估使用
 *
 * @param[in]  since_timestamp  一个UNIX时间戳，用于界定“新”记录。
 * @param[out] out_buffer       一个指向 `uint8_t*` 的指针。如果函数成功，
 *                              它将被设置为新分配的缓冲区的地址。
 * @param[out] out_size         一个指向 `uint32_t` 的指针。如果函数成功，
 *                              它将被设置为缓冲区的总大小（字节）。
 *
 * @return     int
 *             - 0: 成功 (即使没有新记录也返回成功)。
 *             - -1: 失败（如内存分配失败、文件读取失败等）。
 *
 * @note       **重要**: 调用者在处理完 `out_buffer` 中的数据后，
 *             **必须**负责调用 `free(out_buffer)` 来释放内存。
 */
int Energy_File_Unit_Data_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size)
{
    // 1. 参数校验
    if (out_buffer == NULL || out_size == NULL) {
        return -1;
    }

    // 获取文件操作互斥锁，保护整个读操作过程
    if (xEnergyFileProcessSemaphore && xSemaphoreTake(xEnergyFileProcessSemaphore, pdMS_TO_TICKS(300)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to take file process semaphore for reading.");
        return -1;
    }

    int ret = 0;
    uint8_t *buffer = NULL;

    *out_buffer = NULL;
    *out_size = 0;

    // 2. 调用辅助函数，获取新记录的数量
    uint16_t new_records_count = Energy_File_Unit_Data_Count_New_Records(since_timestamp);

    // 如果没有新记录，则无需分配内存和读取，直接返回成功
    if (new_records_count == 0) {
        ESP_LOGW(TAG, "No new records found since timestamp %lld.", since_timestamp);
        goto exit_point;
    }

    ESP_LOGI(TAG, "Found %u new records since timestamp %lld. Preparing to read.", new_records_count, since_timestamp);

    // 3. 计算总大小并分配内存
    uint32_t total_size = ENERGY_FILE_HEADER_LEN + new_records_count * ENERGY_FILE_RECORD_UNIT_LEN;
    buffer = (uint8_t *)iot_calloc(total_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes for incremental energy data.", total_size);
        ret = -1;
        goto exit_point;
    }

    // 4. 读取文件头到缓冲区的起始位置
    char path[30] = {0};
    ENERGY_FILE_PATH_IOT_MAX(path);
    if (fs_file_read_at(path, 0, buffer, ENERGY_FILE_HEADER_LEN) != ENERGY_FILE_HEADER_LEN) {
        ESP_LOGE(TAG, "Failed to read file header for incremental sync.");
        ret = -1;
        goto exit_point;
    }

    // 5. 读取所有新记录到缓冲区
    //    首先，找到文件中第一个新记录的物理位置
    uint16_t oldest_record_p_idx; // 最旧记录的物理索引
    if (Energy_File_Header.unit_real_cnt < Energy_File_Header.unit_max_cnt) {
        // 文件未满，最旧的记录在索引0
        oldest_record_p_idx = 0;
    } else {
        // 文件已满，最旧的记录在当前的写指针位置
        oldest_record_p_idx = Energy_File_Header.unit_bias;
    }

    // 第一个新记录的逻辑索引 = 总记录数 - 新记录数
    uint16_t first_new_l_idx = Energy_File_Header.unit_real_cnt - new_records_count;
    // 第一个新记录的物理索引 = (最旧记录的物理索引 + 当前逻辑索引) % 数组总容量
    uint16_t first_new_p_idx = (oldest_record_p_idx + first_new_l_idx) % Energy_File_Header.unit_max_cnt;

    // 分一或两块，将所有新记录顺序读入缓冲区
    uint8_t *record_write_ptr = buffer + ENERGY_FILE_HEADER_LEN;
    uint16_t space_to_end = Energy_File_Header.unit_max_cnt - first_new_p_idx;

    if (new_records_count <= space_to_end) {
        // 情况A: 所有新记录都在一个连续的块中
        uint32_t read_offset = ENERGY_FILE_RECORD_UNIT_ADDR + first_new_p_idx * ENERGY_FILE_RECORD_UNIT_LEN;
        if (fs_file_read_at(path, read_offset, record_write_ptr, new_records_count * ENERGY_FILE_RECORD_UNIT_LEN) != new_records_count * ENERGY_FILE_RECORD_UNIT_LEN) {
            ESP_LOGE(TAG, "Failed to read continuous block of new records.");
            ret = -1;
            goto exit_point;
        }
    } else {
        // 情况B: 新记录跨越了物理文件的末尾，需要分两次读取
        uint16_t cnt1 = space_to_end;
        uint16_t cnt2 = new_records_count - cnt1;

        // 读取第一部分 (从 first_new_p_idx 到文件末尾)
        uint32_t read_offset1 = ENERGY_FILE_RECORD_UNIT_ADDR + first_new_p_idx * ENERGY_FILE_RECORD_UNIT_LEN;
        if (fs_file_read_at(path, read_offset1, record_write_ptr, cnt1 * ENERGY_FILE_RECORD_UNIT_LEN) != cnt1 * ENERGY_FILE_RECORD_UNIT_LEN) {
            ESP_LOGE(TAG, "Failed to read first part of wrapped records.");
            ret = -1;
            goto exit_point;
        }

        // 读取第二部分 (从文件开头到剩余记录结束)
        uint32_t read_offset2 = ENERGY_FILE_RECORD_UNIT_ADDR; // 从记录区开头读
        if (fs_file_read_at(path, read_offset2, record_write_ptr + cnt1 * ENERGY_FILE_RECORD_UNIT_LEN, cnt2 * ENERGY_FILE_RECORD_UNIT_LEN) != cnt2 * ENERGY_FILE_RECORD_UNIT_LEN) {
            ESP_LOGE(TAG, "Failed to read second part of wrapped records.");
            ret = -1;
            goto exit_point;
        }
    }

    // 6. 成功，返回缓冲区地址和大小
    *out_buffer = buffer;
    *out_size = total_size;

exit_point:
    // 统一的出口：处理资源释放
    if (ret != 0 && buffer != NULL) {
        // 如果函数执行失败，且内存已分配，则释放内存
        free(buffer);
        *out_buffer = NULL; // 确保外部不会使用悬空指针
        *out_size = 0;
    }

    // 释放文件操作互斥锁
    xSemaphoreGive(xEnergyFileProcessSemaphore);

    return ret;
}

/*------------------------------------------------------------------------------
 Function: Is_Energy_File_Unit_Data_File
 -----------------------------------------------------------------------------*/
/**
 * @brief      判断传入的文件标识符是否为当前的能量数据文件。
 * @details    此函数用于验证外部请求的文件是否为本模块管理的能量数据文件。
 *             它会根据传入的参数构建一个完整的路径，并与模块内部生成的
 *             标准路径进行比较。
 *
 * @param[in]  fname  一个指向数据通道文件（自定义格式）参数的指针。
 *                    - fname[0]: 文件夹层级。
 *                    - &fname[1]: 文件名字符串。
 *
 * @return     int
 *             - 0: 是能量数据文件。
 *             - -1: 不是能量数据文件或参数错误。
 */
int Is_Energy_File_Unit_Data_File(const char *fname)
{
    // 1. 参数校验
    if (fname == NULL) {
        return -1;
    }

    // 2. 根据传入的参数构建请求的文件路径
    //    fname[0] 是目录, &fname[1] 是文件名
    char request_path[40] = {0};
    char filedir = fname[0];
    char* filename = &fname[1];

    if ((char)(ENERGY_FILE_FOLDER_LEVEL[0]) != filedir) {
        // 目录不匹配，不是目标文件
        return -1;
    }

    // 使用 snprintf 安全地构建路径，防止溢出
    snprintf(request_path, sizeof(request_path), "%s/%s%s", FS_BASE_PATH, ENERGY_FILE_FOLDER_LEVEL, filename);

    // 3. 使用与能量数据模块内部完全相同的宏来生成标准文件名
    char local_energy_path[40] = {0};
    ENERGY_FILE_PATH_IOT_MAX(local_energy_path);

    // 4. 比较两个路径字符串
    if (strcmp(request_path, local_energy_path) == 0) {
        // 两个字符串完全相同，确认是目标能量数据文件
        return 0;
    }

    // 字符串不匹配，不是目标文件
    return -1;
}

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符（ENERGY_FILE_FOLDER_LEVEL[0]）
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Energy_File_Unit_Data_File_Id(char *out_buf, size_t buf_len)
{
    if (out_buf == NULL || buf_len < 2) {
        return -1;
    }

    char local_log_path[30] = {0};
    ENERGY_FILE_PATH_IOT_MAX(local_log_path);

    const char *folder_char = (ENERGY_FILE_FOLDER_LEVEL && ENERGY_FILE_FOLDER_LEVEL[0]) ?
                                &ENERGY_FILE_FOLDER_LEVEL[0] : NULL;
    if (folder_char == NULL) {
        return -1;
    }

    // 提取文件名部分（最后一个 '/' 之后）
    const char *p = strrchr(local_log_path, '/');
    const char *filename = p ? p + 1 : local_log_path;
    size_t fnlen = strlen(filename);

    if (fnlen + 1 > buf_len) { // 1 字节 folder + fnlen
        return -1;
    }

    out_buf[0] = folder_char[0];
    memcpy(out_buf + 1, filename, fnlen); // 不包含终止符

    return 0;
}

/**
 * @brief 统计晚于指定时间戳的日志新记录数（含信号量保护）。
 *
 * @param[in] since_timestamp  自 Unix 纪元起的时间戳；传 0 表示统计全部记录。
 *
 * @return int
 *         - >=0 : 新记录数量
 *         - -1  : 参数错误或其它不可恢复错误
 *         - -2  : 获取文件处理信号量失败（记录将被丢弃）
 */
int Is_Energy_File_Unit_Data_Count_New(time_t since_timestamp)
{
    uint16_t new_count = 0;

    /* 获取信号量保护 */
    if (xEnergyFileProcessSemaphore && xSemaphoreTake(xEnergyFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "failed to take file process semaphore. Records will be discarded.");
        return -2;
    }

    /* 统计晚于 since_timestamp 的新记录数量（内部处理文件读错时返回0） */
    new_count = Energy_File_Unit_Data_Count_New_Records(since_timestamp);

    /* 释放信号量 */
    if (xEnergyFileProcessSemaphore) {
        xSemaphoreGive(xEnergyFileProcessSemaphore);
    }

    return (int)new_count;
}

/*------------------------------------------------------------------------------
 Function: Get_Nearest_Hour_Bias_Time
 -----------------------------------------------------------------------------*/
/**
 * @brief      获取当前时间最近的前一个小时偏移标准时间点。
 * @details    根据每小时存储次数（ENERGY_HOUR_PERIOD_CNT）和偏移数组 Time_Hour_Bias_Sec，
 *             计算当前时间最近的“前一个”存储节点时间点（如整点、整点半等），用于能量记录时间对齐。
 *             当前小时第一个节点是整点0。
 *
 * @param[in]  now 当前时间戳（time_t）。
 * @return     time_t 最近的前一个标准时间点（UTC时间戳）。
 */
static time_t Get_Nearest_Hour_Bias_Time(time_t now)
{
    uint32_t sec_in_hour = now % 3600;
    int16_t nearest_bias = 0;

    // 找到当前小时内不大于当前秒数的最大偏移
    for (uint8_t i = 0; i < ENERGY_HOUR_PERIOD_CNT; i++) {
        if (Time_Hour_Bias_Sec[i] <= sec_in_hour) {
            nearest_bias = Time_Hour_Bias_Sec[i];
        } else {
            break; // 偏移数组递增，遇到第一个大于当前秒数的即停止
        }
    }

    // 返回最近的前一个节点时间点（当前小时起点 + 最近偏移）
    return now - sec_in_hour + nearest_bias;
}

/*------------------------------------------------------------------------------
 Function: Energy_Unit_Write
 -----------------------------------------------------------------------------*/
/**
 * @brief      【辅助函数】将能量记录块写入能量数据文件。
 * @details    封装了对 fs_record_write 的调用和错误处理，以减少主函数中的代码重复。
 *
 * @param[in]  path          文件路径。
 * @param[in]  records       指向待写入能量记录数组的指针。
 * @param[in]  start_offset  文件内记录区的起始物理地址。
 * @param[in]  record_index  写入的目标记录索引 (或 FILE_APPEND_MODE)。
 * @param[in]  num_to_write  本次要写入的记录数量。
 *
 * @return     int 0表示成功, -1表示失败。
 */
static int Energy_Unit_Write(const char *path, void *records, uint32_t start_offset, uint16_t record_index, uint16_t num_to_write)
{
    if (num_to_write == 0) {
        return 0; // 没有记录要写入，直接返回成功。
    }
    if (fs_record_write(path, records, start_offset, record_index, ENERGY_FILE_RECORD_UNIT_LEN, num_to_write) != 0) {
        ESP_LOGE(TAG, "fs_record_write failed! index: %u, count: %u", record_index, num_to_write);
        return -1;
    }
    return 0;
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Unit_Data_Generate
 -----------------------------------------------------------------------------*/
/**
 * @brief      生成本周期能量记录数据（支持补零和异常处理）
 * @details    根据当前时间与文件头时间戳，判断是否到达新的存储节点，
 *             并生成对应的能量记录数据。支持自动补零（空记录）和时间回退异常处理。
 *             - 若到达新节点，则生成新记录，并检测是否需要补零。
 *             - 若时间回退，则生成异常记录并打印警告。
 *             - 若未到节点，则不生成记录。
 *
 * @param[out] Unit_Data  指向能量记录单元数组的指针（输出）
 *
 * @return     uint16_t   实际生成的记录条数
 */
static uint16_t Energy_File_Unit_Data_Generate(energy_file_record_unit_t *Unit_Data)
{
    uint16_t cnt = 0;
    time_t now = time(NULL);
    time_t nearest_time = Get_Nearest_Hour_Bias_Time(now);

    if (nearest_time > Energy_File_Header.utc_timestamp)
    {
        // 到达新节点，生成新记录
        memcpy(&Unit_Data[cnt].delta_energy, &Energy_File_Header.delta_energy_unit_1Wh, sizeof(kwh_energy_u16_t));
        cnt += 1;

        // 检查是否需要补零（空记录），仅在长时间未写入时触发
        if (((nearest_time - Energy_File_Header.utc_timestamp) > (3600 / ENERGY_HOUR_PERIOD_CNT))
            && (Energy_File_Header.utc_timestamp != 0))
        {
            Energy_File_Unit_Empty_Flag = true;
            Energy_File_Unit_Empty_Cnt = (nearest_time - Energy_File_Header.utc_timestamp - 1) / (3600 / ENERGY_HOUR_PERIOD_CNT);
            ESP_LOGW(TAG, "Detected %u missing records, will fill empty records.", Energy_File_Unit_Empty_Cnt);
        }
    }
    else if (nearest_time < Energy_File_Header.utc_timestamp)
    {
        // 时间回退异常，生成异常记录，修改时间标签至当前时间
        ESP_LOGE(TAG, "Time rollback detected! nearest_time=%lld, last_record=%lld", (long long)nearest_time, (long long)Energy_File_Header.utc_timestamp);
        memcpy(&Unit_Data[cnt].delta_energy, &Energy_File_Header.delta_energy_unit_1Wh, sizeof(kwh_energy_u16_t));
        cnt += 1;
    }
    else
    {
        // 未到存储节点，不生成记录
        // ESP_LOGI(TAG, "Not reached next storage node, no record generated.");
    }

    return cnt;
}


/*------------------------------------------------------------------------------
 Function: Energy_File_Unit_Data_Save_Empty
 -----------------------------------------------------------------------------*/
/**
 * @brief      以循环方式批量写入空能量记录到能量数据文件（写入次数最少）。
 * @details    用于补齐或占位，写入指定数量的空记录，自动分块，尽量减少写入次数。
 *             若empty_count大于unit_max_cnt，则直接清空文件并重写文件头。
 *
 * @param[in]  empty_count  需要写入的空记录条数。
 *
 * @return     int
 *             - 0: 成功。
 *             - -1: 失败（如文件写入失败等）。
 */
static int Energy_File_Unit_Data_Save_Empty(uint16_t empty_count)
{
    char path[30] = {0};
    int ret = 0;

    ENERGY_FILE_PATH_IOT_MAX(path);

    // 如果empty_count大于unit_max_cnt，直接清空文件并重写文件头
    if (empty_count >= Energy_File_Header.unit_max_cnt) {
        // 删除原文件
        if (fs_file_delete(path) != 0) {
            ESP_LOGE(TAG, "Failed to delete energy file: %s", path);
            ret = -1; 
            goto save_exit;
        }

        // 重新生成文件，直接写最后一个记录的位置（文件系统会自动补零）
        energy_file_record_unit_t empty_record = {0};
        if (Energy_Unit_Write(path, &empty_record, ENERGY_FILE_RECORD_UNIT_ADDR, (Energy_File_Header.unit_max_cnt - 1), 1) != 0) {
            ESP_LOGE(TAG, "Failed to create new empty energy file: %s", path);
            ret = -1; 
            goto save_exit;
        }

        // 重置文件头
        Energy_File_Header.unit_real_cnt = Energy_File_Header.unit_max_cnt;
        Energy_File_Header.unit_bias = 0;

        ret = 0; 
        goto save_exit;
    }

    // 只分两块写入，避免多次循环
    uint16_t space_left, space_to_end;
    uint16_t cnt1 = 0, cnt2 = 0;

    if (Energy_File_Header.unit_real_cnt < Energy_File_Header.unit_max_cnt) {
        space_left = Energy_File_Header.unit_max_cnt - Energy_File_Header.unit_real_cnt;
        cnt1 = (empty_count > space_left) ? space_left : empty_count;
        cnt2 = empty_count - cnt1;

        // 第1部分追加写入到文件末尾
        if (cnt1 > 0) {
            energy_file_record_unit_t *empty_buf = (energy_file_record_unit_t *)iot_calloc(cnt1 * sizeof(energy_file_record_unit_t));
            if (!empty_buf) { ret = -1; goto save_exit; }
            if (Energy_Unit_Write(path, empty_buf, ENERGY_FILE_RECORD_UNIT_ADDR, FILE_APPEND_MODE, cnt1) != 0) {
                free(empty_buf); ret = -1; goto save_exit;
            }
            free(empty_buf);
        }

        // 第2部分（溢出时）从文件记录区的开头开始覆盖写入
        if (cnt2 > 0) {
            energy_file_record_unit_t *empty_buf = (energy_file_record_unit_t *)iot_calloc(cnt2 * sizeof(energy_file_record_unit_t));
            if (!empty_buf) { ret = -1; goto save_exit; }
            if (Energy_Unit_Write(path, empty_buf, ENERGY_FILE_RECORD_UNIT_ADDR, 0, cnt2) != 0) {
                free(empty_buf); ret = -1; goto save_exit;
            }
            free(empty_buf);
        }

        // 更新文件头
        if (cnt2 > 0) {
            Energy_File_Header.unit_real_cnt = Energy_File_Header.unit_max_cnt;
            Energy_File_Header.unit_bias = cnt2;
        } else {
            Energy_File_Header.unit_real_cnt += cnt1;
            Energy_File_Header.unit_bias = 0;
        }
    } else {
        space_to_end = Energy_File_Header.unit_max_cnt - Energy_File_Header.unit_bias;
        cnt1 = (empty_count > space_to_end) ? space_to_end : empty_count;
        cnt2 = empty_count - cnt1;

        // 第1部分从当前索引开始覆盖写入
        if (cnt1 > 0) {
            energy_file_record_unit_t *empty_buf = (energy_file_record_unit_t *)iot_calloc(cnt1 * sizeof(energy_file_record_unit_t));
            if (!empty_buf) { ret = -1; goto save_exit; }
            if (Energy_Unit_Write(path, empty_buf, ENERGY_FILE_RECORD_UNIT_ADDR, Energy_File_Header.unit_bias, cnt1) != 0) {
                free(empty_buf); ret = -1; goto save_exit;
            }
            free(empty_buf);
        }

        // 第2部分（跨越末尾）从文件记录区的开头继续覆盖写入
        if (cnt2 > 0) {
            energy_file_record_unit_t *empty_buf = (energy_file_record_unit_t *)iot_calloc(cnt2 * sizeof(energy_file_record_unit_t));
            if (!empty_buf) { ret = -1; goto save_exit; }
            if (Energy_Unit_Write(path, empty_buf, ENERGY_FILE_RECORD_UNIT_ADDR, 0, cnt2) != 0) {
                free(empty_buf); ret = -1; goto save_exit;
            }
            free(empty_buf);
        }

        // 更新文件头
        if (cnt2 > 0) {
            Energy_File_Header.unit_bias = cnt2;
        } else {
            Energy_File_Header.unit_bias += cnt1;
            if (Energy_File_Header.unit_bias == Energy_File_Header.unit_max_cnt) {
                Energy_File_Header.unit_bias = 0;
            }
        }
    }

save_exit:
    return ret;
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Unit_Data_Save_Raw
 -----------------------------------------------------------------------------*/
/**
 * @brief      批量写入能量记录到能量数据文件（循环队列方式）
 * @details    根据文件当前状态（未满/已满），分块写入能量记录，实现循环覆盖。
 *             自动更新文件头的记录数和写入索引，保证数据结构正确。
 *             - 首次填充阶段：追加写入，溢出时覆盖文件头。
 *             - 循环覆盖阶段：从当前索引覆盖写入，跨越末尾时分两块写入。
 *
 * @param[in]  record_cnt  本次要写入的能量记录条数
 *
 * @return     int
 *             - 写入成功返回实际写入的记录数
 *             - 写入失败返回0
 */
static int Energy_File_Unit_Data_Save_Raw(uint16_t record_cnt)
{
    char path[30] = {0};

    // 获取能量数据文件的完整路径。
    ENERGY_FILE_PATH_IOT_MAX(path);

    // 检查文件是否已满，分为“首次填充”和“循环覆盖”两种主要情况。
    if (Energy_File_Header.unit_real_cnt < Energy_File_Header.unit_max_cnt) {
        // --- 情况 A: 文件尚未写满 (首次填充阶段) ---
        uint16_t space_left = Energy_File_Header.unit_max_cnt - Energy_File_Header.unit_real_cnt;
        uint16_t cnt1 = (record_cnt > space_left) ? space_left : record_cnt;
        uint16_t cnt2 = record_cnt - cnt1;
    
        // 第1部分追加写入到文件末尾
        if (Energy_Unit_Write(path, &Energy_Unit_Data[0], ENERGY_FILE_RECORD_UNIT_ADDR, FILE_APPEND_MODE, cnt1) != 0) return 0;
    
        // 第2部分（如果本次写入后，文件将会被填满并溢出时存在）从文件记录区的开头开始覆盖写入
        if (Energy_Unit_Write(path, &Energy_Unit_Data[cnt1], ENERGY_FILE_RECORD_UNIT_ADDR, 0, cnt2) != 0) return 0;
    
        // 更新文件头
        if (cnt2 > 0) {
            // 当前记录数达到最大值，写入点索引更新为本次溢出覆盖的记录数。
            Energy_File_Header.unit_real_cnt = Energy_File_Header.unit_max_cnt;
            Energy_File_Header.unit_bias = cnt2;
        } else {
            // 增加当前记录数，写入点索引在填充阶段无意义，保持为0。
            Energy_File_Header.unit_real_cnt += cnt1;
            Energy_File_Header.unit_bias = 0;
        }
    } else {
        // --- 情况 B: 文件已满 (循环覆盖阶段) ---
        uint16_t space_to_end = Energy_File_Header.unit_max_cnt - Energy_File_Header.unit_bias;
        uint16_t cnt1 = (record_cnt > space_to_end) ? space_to_end : record_cnt;
        uint16_t cnt2 = record_cnt - cnt1;
    
        // 第1部分从当前索引开始覆盖写入
        if (Energy_Unit_Write(path, &Energy_Unit_Data[0], ENERGY_FILE_RECORD_UNIT_ADDR, Energy_File_Header.unit_bias, cnt1) != 0) return 0;
    
        // 第2部分（如果本次写入会跨越末尾时存在）从文件记录区的开头继续覆盖写入
        if (Energy_Unit_Write(path, &Energy_Unit_Data[cnt1], ENERGY_FILE_RECORD_UNIT_ADDR, 0, cnt2) != 0) return 0;
    
        // 更新文件头
        if (cnt2 > 0) {
            // 本次写入跨越末尾，写入点索引更新为第二部分写入的记录数。
            Energy_File_Header.unit_bias = cnt2;
        } else {
            // 本次写入未跨越末尾，写入点索引向后移动。如果恰好到达末尾，则归零
            Energy_File_Header.unit_bias += cnt1;
            if (Energy_File_Header.unit_bias == Energy_File_Header.unit_max_cnt) {
                Energy_File_Header.unit_bias = 0;
            }
        }
    }

    return record_cnt;
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Long_Term_Data_Save
 -----------------------------------------------------------------------------*/
/**
 * @brief      持久化能量长期统计数据（年度/月度/日度）
 * @details    检查当前节点时间与上次记录时间的年、月、日是否发生变动，
 *             若发生变动则将累计的长期能量数据（年度、月度、日度）写入长期统计文件。
 *             主要流程如下：
 *             1. 获取当前节点时间、上次记录时间、首次记录时间，并转换为本地时间结构体。
 *             2. 判断年、月、日是否发生变动，若有变动则进入持久化流程。
 *             3. 计算当前年度在长期文件中的偏移（year_bia），支持多年度数据管理。
 *             4. 遍历所有能量项，依次处理每项的长期统计数据：
 *                - 获取长期统计文件路径（按能量项类型、年度等生成）。
 *                - 读取历史数据，若读取失败则初始化为零。
 *                - 更新结构体成员（类型、年份、年度总能量、月度能量、日度能量）。
 *                - 写回长期统计文件指定偏移位置。
 *                - 写入成功后，按变动类型清零对应缓存，防止重复累计。
 *             5. 若写入失败则打印错误日志并跳过该项。
 *
 * @note       仅在年、月、日变动时触发持久化，避免频繁写入，提升性能和可靠性。
 *
 * @return     void
 */
static void Energy_File_Long_Term_Data_Save(void)
{
    // 时间戳无效时暂不处理
    if((0 == Energy_File_Header.first_utc_timestamp) 
        || (0 == Energy_File_Header.utc_timestamp)) {
        ESP_LOGE(TAG, "Energy_File_Long_Term_Data_Save: first_utc_timestamp or utc_timestamp is zero, skip long term data save.");
        return;
    }

    // 获取当前时间和最近节点时间
    time_t now = time(NULL);
    time_t nearest_time = Get_Nearest_Hour_Bias_Time(now);
    time_t t_last = (time_t)Energy_File_Header.utc_timestamp;
    time_t t_first = (time_t)Energy_File_Header.first_utc_timestamp;

    // 转换为本地时间结构
    struct tm tm_nearest, tm_last, tm_first = {0};
    localtime_r(&nearest_time, &tm_nearest);
    localtime_r(&t_last, &tm_last);
    localtime_r(&t_first, &tm_first);

    // 判断年、月、日是否发生变动
    bool year_changed  = (tm_nearest.tm_year != tm_last.tm_year);
    bool month_changed = (tm_nearest.tm_mon  != tm_last.tm_mon);
    bool day_changed   = (tm_nearest.tm_mday != tm_last.tm_mday);

    // 仅在年/月/日变动时进行长期数据持久化
    if (year_changed || month_changed || day_changed)
    {
        // 仅支持首次时间早于或等于当前时间
        if (tm_first.tm_year <= tm_last.tm_year)
        {
            ESP_LOGW(TAG, "Energy_File_Long_Term_Data_Save : first_utc_timestamp(%lu), last_utc_timestamp(%lu), nearest_time(%llu)", 
                Energy_File_Header.first_utc_timestamp, Energy_File_Header.utc_timestamp, nearest_time);

            // 多年度数据管理，计算年度偏移
            uint16_t year_bia = tm_last.tm_year - tm_first.tm_year;

            // 超过最大保存年限后循环覆盖，保证文件空间可控
            year_bia = year_bia % ENERGY_LONG_TERM_SAVE_YEAR_MAX;
                
            for (uint8_t index = 0; index < ENERGY_ITEM_REAL_NUM; index++)
            {
                // 获取能量项英文简称（如 "pv"），跳过无效项
                if( 0 == Energy_File_Header.index[index] ) continue;
                const char* item_name = Energy_Raw_Index_To_Name(Energy_File_Header.index[index]);
                if (item_name == NULL) {
                    ESP_LOGW(TAG, "Skip invalid energy item index: %d", Energy_File_Header.index[index]);
                    continue;
                }

                char path[30] = {0};
                energy_file_year_record_t cur_data = {0};

                // 获取能量长期数据文件的完整路径（当前仅支持汇总文件）
                ENERGY_FILE_PATH_YEAR(path, item_name, 0, 0);

                // 读取历史数据，若读取失败则初始化为零
                int bytes_read = fs_file_read_at(path, (year_bia * ENERGY_LONG_TERM_DATA_MAX_LEN), (uint8_t*)&cur_data, ENERGY_LONG_TERM_DATA_MAX_LEN);
                if (bytes_read != ENERGY_LONG_TERM_DATA_MAX_LEN) {
                    ESP_LOGE(TAG, "Failed to read long term energy data: %s, bia: %d. Init zero.", path, year_bia);
                    memset(&cur_data, 0, sizeof(cur_data));
                }

                // 更新结构体成员
                cur_data.type         = Energy_File_Header.index[index]; // 能量类型
                cur_data.year         = tm_last.tm_year + 1900;          // 年份
                cur_data.total_energy = Energy_File_Header.delta_energy_year_100Wh.energy_u32[index]; // 年累计
                cur_data.mouth_energy[tm_last.tm_mon] = Energy_File_Header.delta_energy_month_100Wh.energy_u32[index]; // 月累计
                cur_data.day_enengy[tm_last.tm_mday - 1] = Energy_File_Header.delta_energy_day_100Wh.energy_u32[index]; // 日累计

                // 可选：计算CRC16校验
                // cur_data.crc16 = Calc_CRC16((uint8_t*)&cur_data, sizeof(cur_data)-2);

                // 写入新的长期统计数据
                if (fs_file_write_at(path, (year_bia * ENERGY_LONG_TERM_DATA_MAX_LEN), (uint8_t*)&cur_data, ENERGY_LONG_TERM_DATA_MAX_LEN) != ENERGY_LONG_TERM_DATA_MAX_LEN) {
                    ESP_LOGE(TAG, "Failed to write long term energy data: %s, bia: %d", path, year_bia);
                    continue;
                }

                // 写入成功，清除指定缓存，防止重复累计
                if ( year_changed ) Energy_File_Header.delta_energy_year_100Wh.energy_u32[index] = 0;
                if ( month_changed ) Energy_File_Header.delta_energy_month_100Wh.energy_u32[index] = 0;
                if ( day_changed ) Energy_File_Header.delta_energy_day_100Wh.energy_u32[index] = 0;
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to calc tm_year! tm_first.tm_year(%d), tm_last.tm_year(%d)", tm_first.tm_year, tm_last.tm_year);
        }
    }
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Data_Process
 -----------------------------------------------------------------------------*/
/**
 * @brief      能量数据文件主处理任务（批量写入+补零+时间对齐）
 * @details    该函数负责将本周期生成的能量记录批量写入本地文件，并自动处理补零、长期数据保存及时间戳对齐。
 *             主要流程如下：
 *             1. 批量生成节点单位数据，填充本地缓冲区（Energy_Unit_Data）。
 *             2. 若有新记录，分块写入能量数据文件（循环队列方式），并清空本周期增量。
 *             3. 检查是否需要补零（空记录），如有则批量补零并打印日志。
 *             4. 持久化长期能量统计数据（如年度、月度等）。
 *             5. 按小时节点对齐更新时间戳（utc_timestamp/first_utc_timestamp），保证记录时间与存储节点一致。
 *             6. 持久化文件头到磁盘，确保状态同步。
 *             7. 全过程受互斥锁保护，保证线程安全。
 *
 * @note       本函数适用于循环队列结构的能量数据文件，支持高效批量写入和自动覆盖。
 */
void Energy_File_Data_Process(void)
{
    uint16_t record_cnt = 0;
    char path[30] = {0};

    // 获取文件操作互斥锁，保护整个操作过程
    if (xEnergyFileProcessSemaphore && xSemaphoreTake(xEnergyFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Save_Task failed to take file process semaphore. Records will be discarded.");
        return;
    }

    // =================================================================================
    // 阶段 1: 批量生成节点单位数据
    // =================================================================================
    // 生成多节点数据，直到缓冲区满或数据为空。
    memset(Energy_Unit_Data, 0, sizeof(Energy_Unit_Data));
    record_cnt = Energy_File_Unit_Data_Generate(Energy_Unit_Data);

    // =================================================================================
    // 阶段 2: 将缓冲区中的数据持久化到文件
    // =================================================================================
    // 仅当本次拉取到了至少一条记录时，才执行文件操作。
    if (record_cnt > 0) {
        // 获取能量数据文件的完整路径。
        ENERGY_FILE_PATH_IOT_MAX(path);
    
        // =================================================================================
        // 阶段 2.1: 持久化能量单元数据
        // =================================================================================
        if ( 0 == Energy_File_Unit_Data_Save_Raw(record_cnt) ) {
            goto save_exit;
        } else {
            memset(&Energy_File_Header.delta_energy_unit_1Wh, 0, sizeof(kwh_energy_u16_t));
        }

        // =================================================================================
        // 阶段 2.2: 检查是否需要补零写入
        // =================================================================================
        if (Energy_File_Unit_Empty_Flag) {
            ESP_LOGW(TAG, "Energy_File_Unit_Empty_Flag is set, writing %u empty records...", Energy_File_Unit_Empty_Cnt);
            int empty_ret = Energy_File_Unit_Data_Save_Empty(Energy_File_Unit_Empty_Cnt);
            if (empty_ret == 0) {
                ESP_LOGI(TAG, "Successfully wrote %u empty records to energy file.", Energy_File_Unit_Empty_Cnt);
            } else {
                ESP_LOGE(TAG, "Failed to write empty records to energy file! Count: %u", Energy_File_Unit_Empty_Cnt);
            }
            Energy_File_Unit_Empty_Flag = false;
            Energy_File_Unit_Empty_Cnt = 0;
        }
        
        // =================================================================================
        // 阶段 2.3: 持久化能量长期数据（可选）
        // =================================================================================
        Energy_File_Long_Term_Data_Save();
        
        // =================================================================================
        // 阶段 2.4: 更新能量节点文件内时间戳（按小时节点对齐）
        // =================================================================================
        time_t now = time(NULL);
        Energy_File_Header.utc_timestamp = Get_Nearest_Hour_Bias_Time(now);
        if(( 0 == Energy_File_Header.first_utc_timestamp ) 
            || (Energy_File_Header.utc_timestamp < Energy_File_Header.first_utc_timestamp)) {
            // 如果首次写入，或当前记录时间早于首次时间，则更新首次时间戳为当前节点时间
            Energy_File_Header.first_utc_timestamp = Get_Nearest_Hour_Bias_Time(now);
        }
            
        // =================================================================================
        // 阶段 3: 持久化文件头
        // =================================================================================
        // 无论走了哪个逻辑分支，最后都需要将内存中更新过的文件头持久化。
        if (fs_file_write_at(path, ENERGY_FILE_HEADER_ADDR, (uint8_t*)&Energy_File_Header, ENERGY_FILE_HEADER_LEN) != ENERGY_FILE_HEADER_LEN) {
            ESP_LOGE(TAG, "Energy_File_Header update failed!");
            // 注意：即使文件头更新失败，记录本身已写入，这里只记录错误日志而不影响下次运行。
        } else {
            ESP_LOGI(TAG, "Energy file updated. Records added: %u, Current total: %lu, Next index: %lu",
                     record_cnt, Energy_File_Header.unit_real_cnt, Energy_File_Header.unit_bias);
        }
    }

save_exit:
    // 释放文件操作互斥锁
    xSemaphoreGive(xEnergyFileProcessSemaphore);
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Raw_kWh_Update
 -----------------------------------------------------------------------------*/
/**
 * @brief      能量累计数据更新（支持所有能量项，增量统计，自动归档）
 * @details    遍历所有能量项，比较新旧数据，计算增量，按1Wh为单位累计到各级统计变量。
 *             - 支持能量回绕（新数据小于旧数据时，按新数据为增量）。
 *             - 每满100Wh自动归档到100Wh统计项，并清零1Wh累加器。
 *             - 更新所有相关统计结构体成员，包括总量、年度、月度、日度等。
 *             - 旧数据自动更新为本次新数据，便于下次增量计算。
 *             - 若参数无效（如指针为NULL或换算比例为0），直接返回错误。
 *
 * @param[in]  new_data  新的能量数据指针（kwh_energy_u32_t结构体，包含所有能量项的最新累计值）
 * @param[in]  to_1wh    单位换算比例（如1表示直接累加，10表示每单位乘10）
 *
 * @return     int
 *             - 0: 成功，所有能量项已正确累计
 *             - -1: 参数错误，未做任何累计
 */
static int Energy_File_Raw_kWh_Update(kwh_energy_u32_t *new_data, uint16_t to_1wh)
{
    static kwh_energy_u32_t old_data = {0};
    if (new_data == NULL || to_1wh == 0) {
        return -1;
    }

    for (uint8_t index = 0; index < ENERGY_ITEM_REAL_NUM; index++)
    {
        if( 0 == Energy_File_Header.index[index] ) continue;

        // 仅处理新数据大于0的项
        uint16_t delta_data = 0;
        if (new_data->energy_u32[index] > 0)
        {
            if (old_data.energy_u32[index] < new_data->energy_u32[index]) {
                // 正常累计，增量为新旧差值
                delta_data = new_data->energy_u32[index] - old_data.energy_u32[index];
            } else if (old_data.energy_u32[index] > new_data->energy_u32[index]) {
                // 数据回绕或重置，增量为新数据本身
                delta_data = new_data->energy_u32[index];
            } else {
                // 新旧数据相同，无需累计
                delta_data = 0;
            }
        }

        // 换算为1Wh单位
        uint16_t delta_data_1wh = delta_data * to_1wh;

        if ((delta_data_1wh > 0) && (delta_data_1wh < ENERGY_DATA_FILTER_LIMIT_WH))
        {
            // 1Wh增量累加到本周期和本单元
            Energy_File_Header.delta_energy_calc_1Wh.energy_u16[index] += delta_data_1wh;
            Energy_File_Header.delta_energy_unit_1Wh.energy_u16[index] += delta_data_1wh;

            // 若累计满100Wh，自动归档到各级统计项
            if (Energy_File_Header.delta_energy_calc_1Wh.energy_u16[index] >= 100) {
                uint32_t energy_100wh = Energy_File_Header.delta_energy_calc_1Wh.energy_u16[index] / 100;
                Energy_File_Header.delta_energy_all_100Wh.energy_u32[index] += energy_100wh;      // 总累计
                Energy_File_Header.delta_energy_year_100Wh.energy_u32[index] += energy_100wh;     // 年累计
                Energy_File_Header.delta_energy_month_100Wh.energy_u32[index] += energy_100wh;    // 月累计
                Energy_File_Header.delta_energy_day_100Wh.energy_u32[index] += energy_100wh;      // 日累计
                Energy_File_Header.delta_energy_calc_1Wh.energy_u16[index] %= 100;                // 归零1Wh累加器
            }
        }

        // 更新旧数据，便于下次增量计算
        old_data.energy_u32[index] = new_data->energy_u32[index];
    }
    
    return 0;
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Charge_Plan_Update
 -----------------------------------------------------------------------------*/
/**
 * @brief      更新能量文件头中的基础充电计划数据
 * @details    将传入的充电计划结构体内容覆盖写入到能量文件头的对应成员，
 *             用于同步最新的充电计划参数到本地存储。
 *
 * @param[in]  charge_plan  指向基础充电计划结构体的指针
 *
 * @return     int
 *             - 0: 成功，充电计划已更新
 *             - -1: 参数错误，未做任何更新
 */
static int Energy_File_Charge_Plan_Update(base_charge_plan_t *charge_plan)
{
    if (charge_plan == NULL) {
        return -1;
    }

    // 将传入的充电计划结构体内容覆盖写入到能量文件头的对应成员，
    memcpy(&Energy_File_Header.base_charge_plan, charge_plan, sizeof(base_charge_plan_t));
    return 0;
}

/*------------------------------------------------------------------------------
 Function: Energy_File_Raw_Data_Update
 -----------------------------------------------------------------------------*/
/**
 * @brief      能量原始数据及充电计划批量更新接口
 * @details    依次调用能量累计数据更新和充电计划更新函数，
 *             用于一次性同步能量统计和充电计划到文件头缓存。
 *             - 能量数据和充电计划均可独立为空，若为空则对应部分不更新。
 *             - 返回值为各子操作的结果，若有任一失败则整体返回失败。
 *
 * @param[in]  energy      指向能量数据结构体（kwh_energy_u32_t），可为NULL
 * @param[in]  to_1wh      能量单位换算比例（如1表示直接累加，10表示每单位乘10）
 * @param[in]  charge_plan 指向充电计划结构体（base_charge_plan_t），可为NULL
 *
 * @return     int
 *             - 0: 成功，所有数据已正确更新
 *             - -1: 参数错误或任一子操作失败
 */
int Energy_File_Raw_Data_Update(kwh_energy_u32_t *energy, uint16_t to_1wh, base_charge_plan_t *charge_plan)
{
    int ret = 0;

    // 获取文件操作互斥锁，保护整个操作过程
    if (xEnergyFileProcessSemaphore && xSemaphoreTake(xEnergyFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        return -1;
    }

    // 能量累计数据更新（允许energy为NULL，若为NULL则跳过）
    if (energy != NULL && to_1wh != 0) {
        if (Energy_File_Raw_kWh_Update(energy, to_1wh) != 0) {
            ESP_LOGE(TAG, "Energy_File_Raw_kWh_Update failed");
            ret = -1;
        }
    }

    // 充电计划数据更新（允许charge_plan为NULL，若为NULL则跳过）
    if (charge_plan != NULL) {
        if (Energy_File_Charge_Plan_Update(charge_plan) != 0) {
            ESP_LOGE(TAG, "Energy_File_Charge_Plan_Update failed");
            ret = -1;
        }
    }

    // 释放文件操作互斥锁
    xSemaphoreGive(xEnergyFileProcessSemaphore);
    
    return ret;
}

/**
 * @brief 获取指定索引的日能量（单位：Wh）
 *
 * @param index 索引号（由调用方保证有效范围）
 * @return uint32_t 返回日能量值，单位为瓦时(Wh)
 */
uint32_t Energy_Day_Data_Wh_Get(uint8_t index) 
{
    return (Energy_File_Header.delta_energy_day_100Wh.energy_u32[index] * 100
            + Energy_File_Header.delta_energy_calc_1Wh.energy_u16[index]);
}

/**
 * @brief 获取指定索引的月能量（单位：100Wh）
 *
 * @param index 索引号（由调用方保证有效范围）
 * @return uint32_t 返回月能量值，单位为100瓦时(Wh)
 */
uint32_t Energy_Month_Data_100Wh_Get(uint8_t index) 
{
    return (Energy_File_Header.delta_energy_month_100Wh.energy_u32[index]);
}

/**
 * @brief 获取指定索引的总能量（单位：100Wh）
 *
 * @param index 索引号（由调用方保证有效范围）
 * @return uint32_t 返回总能量值，单位为100瓦时(Wh)
 */
uint32_t Energy_All_Data_100Wh_Get(uint8_t index) 
{
    return (Energy_File_Header.delta_energy_all_100Wh.energy_u32[index]);
}


/**
  * @brief      立即保存文件头，可用于下电前存储临时数据                
  */
int Energy_File_Header_Save(void) 
{
    // 获取文件操作互斥锁，保护整个操作过程
    if (xEnergyFileProcessSemaphore && xSemaphoreTake(xEnergyFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Save_Task failed to take file process semaphore. Records will be discarded.");
        return -1;
    }

    int ret = 0;
    char path[30] = {0};
    ENERGY_FILE_PATH_IOT_MAX(path);
    if (fs_file_write_at(path, ENERGY_FILE_HEADER_ADDR, (uint8_t*)&Energy_File_Header, ENERGY_FILE_HEADER_LEN) != ENERGY_FILE_HEADER_LEN) {
        ESP_LOGE(TAG, "Energy_File_Header update failed!");
        ret = -1;
    }

    // 释放文件操作互斥锁
    xSemaphoreGive(xEnergyFileProcessSemaphore);

    return ret;
}

/**
 * @brief 恢复能量文件到出厂默认并初始化文件头并写入存储
 *
 * 该函数执行以下步骤：
 *  - 尝试获取文件操作互斥锁（xEnergyFileProcessSemaphore），超时则返回错误。
 *  - 删除能量文件所在目录（递归删除）。
 *  - 清零并初始化 Energy_File_Header（版本、单元周期、单元字节数、最大单元数等）。
 *  - 初始化能量索引映射（Energy_Index_Map_Init）。
 *  - 将新的文件头写入到能量文件起始位置（fs_file_write_at）。
 *  - 释放互斥锁。
 *
 * @note
 *  - 函数在写入失败时会记录日志但仍会释放互斥；调用者可根据返回值决定后续动作。
 *  - 该函数会修改全局 Energy_File_Header，并依赖全局信号量 xEnergyFileProcessSemaphore。
 *  - 删除目录与写文件为可能耗时操作，互斥等待超时时间为 100 ms（可按需调整）。
 *
 * @return int
 *      0   成功（已完成默认恢复并写入文件头）
 *     -1   失败（未能获取互斥或其他致命错误）
 */
int Energy_File_Restore_Factory_Defaults(void)
{
    int ret = 0;
    char path[30] = {0};

    /* 获取文件操作互斥锁，保护整个操作过程
       若信号量不存在或获取超时（100ms），直接返回错误，避免并发文件操作 */
    if (xEnergyFileProcessSemaphore && xSemaphoreTake(xEnergyFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        return -1;
    }
    
    /* 1) 删除能量文件所在目录（递归删除所有旧文件/目录）
          先构造基础目录路径用于删除（此调用不写回 path 用于后续写入） */
    sprintf(path, "%s/%c", FS_BASE_PATH, (char)(ENERGY_FILE_FOLDER_LEVEL[0]));
    fs_delete_dir_recursive(path);
    
    /* 2) 初始化全局文件头结构为默认值（清零后逐项设置） */
    memset(&Energy_File_Header, 0, sizeof(energy_file_header_t));
    Energy_File_Header.ver = ENERGY_FILE_VER_V1;                    /* 文件格式版本 */
    Energy_File_Header.unit_hour_period = ENERGY_HOUR_PERIOD_CNT;   /* 每单元的小时周期数 */
    Energy_File_Header.unit_bytes = ENERGY_FILE_RECORD_UNIT_LEN;    /* 每记录单元的字节长度 */
    Energy_File_Header.unit_max_cnt = ENERGY_UNIT_MAX_NUM;          /* 单元最大数量上限 */
    
    /* 3) 初始化能量索引映射表（将各能量索引映射到文件布局），以确保运行时索引有效 */
    Energy_Index_Map_Init();
    
    /* 4) 写入新的文件头到持久存储：先构造写入路径（可能含最大路径变体）*/
    ENERGY_FILE_PATH_IOT_MAX(path);
    /* 将 Energy_File_Header 写入文件起始位置，长度为 ENERGY_FILE_HEADER_LEN */
    if (fs_file_write_at(path, 0, (uint8_t*)&Energy_File_Header, ENERGY_FILE_HEADER_LEN) == ENERGY_FILE_HEADER_LEN) {
        ESP_LOGW(TAG, "New energy file header initialized and written successfully.");
    } else {
        /* 写入失败：记录错误并设置返回码，但仍需释放互斥后返回 */
        ESP_LOGE(TAG, "FATAL: Failed to write new energy file header!");
        ret = -1;
    }
    
    /* 5) 释放文件操作互斥锁（无论成功或失败都必须释放） */
    xSemaphoreGive(xEnergyFileProcessSemaphore);
    
    return ret;    
}

/**
 * @brief 清除指定索引的能量数据并更新能量文件头
 * @param index 能量项索引
 * @return 0 成功；-1 失败
 */
int Energy_File_Clean_Specific_Data_By_Index(uint8_t index)
{
    int ret = 0;
    char path[30] = {0};

    /* 1) 校验索引是否已映射到有效能量项：若未映射则直接返回错误 */
    if (Energy_File_Header.index[index] == 0) return -1;

    /* 获取能量项英文简称（如 "pv"），若转换失败则视为无效项并返回 */
    const char* item_name = Energy_Raw_Index_To_Name(Energy_File_Header.index[index]);
    if (item_name == NULL) {
        ESP_LOGW(TAG, "Skip invalid energy item index: %d", Energy_File_Header.index[index]);
        return -1;
    }

    /* 2) 获取文件操作互斥锁，保护后续删除/写入操作（超时 100ms 表示忙时放弃） */
    if (xEnergyFileProcessSemaphore && xSemaphoreTake(xEnergyFileProcessSemaphore, pdMS_TO_TICKS(100)) != pdPASS) {
        return -1;
    }

    /* 3) 删除该能量项对应的年度/长期文件（当前实现仅删除汇总文件路径）：
          ENERGY_FILE_PATH_YEAR 会把 item_name 填入 path */
    ENERGY_FILE_PATH_YEAR(path, item_name, 0, 0);
    fs_file_delete(path);

    /* 4) 在内存中清零该索引对应的累加/年/月/日/单位/计算值，恢复为 0 */
    Energy_File_Header.delta_energy_all_100Wh.energy_u32[index]   = 0;
    Energy_File_Header.delta_energy_year_100Wh.energy_u32[index]  = 0;
    Energy_File_Header.delta_energy_month_100Wh.energy_u32[index] = 0;
    Energy_File_Header.delta_energy_day_100Wh.energy_u32[index]   = 0;
    Energy_File_Header.delta_energy_unit_1Wh.energy_u16[index]    = 0;
    Energy_File_Header.delta_energy_calc_1Wh.energy_u16[index]    = 0;

    /* 5) 将更新后的文件头写回持久存储（确保使用 IOT_MAX 路径宏构造路径） */
    ENERGY_FILE_PATH_IOT_MAX(path);
    if (fs_file_write_at(path, 0, (uint8_t*)&Energy_File_Header, ENERGY_FILE_HEADER_LEN) == ENERGY_FILE_HEADER_LEN) {
        ESP_LOGW(TAG, "Energy file header updated after cleaning item %s (index %u).", item_name, (unsigned)index);
    } else {
        ESP_LOGE(TAG, "FATAL: Failed to write new energy file header!");
        ret = -1;
    }

    /* 6) 释放互斥锁（无论成功或失败均必须释放） */
    xSemaphoreGive(xEnergyFileProcessSemaphore);

    return ret;
}

