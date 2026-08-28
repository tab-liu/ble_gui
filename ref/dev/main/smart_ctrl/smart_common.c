/**
  ******************************************************************************
  * @file      smart_common.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/25
  * @brief     智能控制模块通用
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/21  <td>1.0     <td>lixingyu   <td>Create the initial version
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
#include <time.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "smart_common.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[SMART_COMMON]"

// TOU检查时间间隔最大天数
#define CONFIG_TOU_CHECK_BACK_DAYS   7

/* =============================== 文件内全局变量 ================================ */



/* ============================== 文件内引用外部接口 ============================= */

#if 1

extern SemaphoreHandle_t xFileSemaphore;

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
int fs_file_read_at(const char *path, uint32_t offset, uint8_t *data, int size)
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
// void *iot_calloc(size_t size)
// {
// #if CONFIG_SPI_RAM_ENABLE
//     // 使用 heap_caps_calloc 分配内存并自动初始化为零
//     return heap_caps_calloc(size, sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
// #else
//     // 使用 calloc 分配内存并自动初始化为零
//     return calloc(size, sizeof(uint8_t));
// #endif
// }

#endif

/* ================================ 模块函数定义 ================================ */

/**
 * @brief 检查 TOU 时间配置是否无效
 *
 * 如果 cfg 为 NULL，或 month_sel/day_sel/week_sel 任一为 0，则认为配置无效。
 *
 * @param[in] cfg 指向 tou_time_config_t 的指针
 * @return int 1: 无效，0: 有效
 */
int tou_time_cfg_invalid(const tou_time_config_t *cfg)
{
    if (cfg == NULL) {
        return 1;
    }
    return (cfg->bits.month_sel == 0) || (cfg->bits.day_sel == 0) || (cfg->bits.week_sel == 0);
}

/**
 * @brief 查找单个TOU配置点在指定区间内最近一次有效触发时间
 * 
 * @param tou_cfg    TOU时间配置指针
 * @param end        当前时间（秒，time_t）
 * @return time_t    最近一次有效触发时间（若无则返回0）
 *
 * 说明：
 * - 回溯区间为 [end - CONFIG_TOU_CHECK_BACK_DAYS * 86400, end]，即向前查找指定天数。
 * - 只要找到第一个满足月、日、周条件的有效触发点即返回，减少无效计算。
 * - 若配置无效或区间内无有效触发点，返回0。
 */
time_t Tou_Point_Last_Trigger(const tou_time_config_t *tou_cfg, time_t end)
{
    // 参数有效性检查
    if (!tou_cfg) return 0;

    // 配置无效直接返回
    if ((TOU_TIME_TYPE_POINT != tou_cfg->bits.time_type) || tou_time_cfg_invalid(tou_cfg)) return 0;

    time_t start = end - CONFIG_TOU_CHECK_BACK_DAYS * 86400;
    time_t last_trigger = 0;
    struct tm tm_day;

    // 只遍历有效的天数，遇到第一个有效即返回
    for (time_t t = end; t >= start; t -= 86400) {
        localtime_r(&t, &tm_day);

        // 月、日、周判断
        if (!TOU_MONTH_IS_SET(tou_cfg->bits.month_sel, tm_day.tm_mon + 1)) continue;
        if (!TOU_DAY_IS_SET(tou_cfg->bits.day_sel, tm_day.tm_mday)) continue;
        if (!TOU_WEEK_IS_SET(tou_cfg->bits.week_sel, tm_day.tm_wday)) continue;

        // 构造当天的触发时间
        time_t trigger_time = mktime(&(struct tm){
            .tm_year = tm_day.tm_year,
            .tm_mon  = tm_day.tm_mon,
            .tm_mday = tm_day.tm_mday,
            .tm_hour = 0,
            .tm_min  = 0,
            .tm_sec  = 0,
        });
        trigger_time += tou_cfg->bits.time_sec;

        // 判断触发时间是否在区间内
        if (trigger_time <= end && trigger_time >= start) {
            last_trigger = trigger_time;
            break; // 找到最近一次，无需再往前查
        }
    }
    
    return last_trigger;
}


/**
 * @brief 计算(last_time, now_time]区间内每一天的本地00:00:00的time_t基准数组（严谨跨天/夏令时安全版）
 * 
 * 该函数用于批量TOU时间点判断等场景，返回区间内所有日历天的本地零点time_t数组。
 * 采用逐日历天递增方式，确保跨天但不足24小时、夏令时切换等场景都能正确处理。
 * 
 * @param last_time 上一次检查的时间戳（秒，time_t，UTC或系统本地时间）
 * @param now_time  当前时间戳（秒，time_t，UTC或系统本地时间）
 * @return tou_time_base_t 结构体
 *         - t_base_arr: 指向分配的time_t数组（需调用者释放）
 *         - days: 数组元素个数（天数，含首尾）
 *         若无有效天数，t_base_arr为NULL，days为0
 */
tou_time_base_t tou_time_get_base_arr(time_t last_time, time_t now_time)
{
    tou_time_base_t result = {0};

    // 参数检查，最大跨度限制为7天，防止极端情况下CPU占用过高
    if ((last_time == 0) || (now_time <= last_time) || ((now_time - last_time) > CONFIG_TOU_CHECK_BACK_DAYS * 86400)) {
        result.t_base_arr = NULL;
        result.days = 0;
        return result;
    }

    // 获取last_time和now_time对应的本地日历天
    struct tm tm_last, tm_now;
    localtime_r(&last_time, &tm_last);
    localtime_r(&now_time, &tm_now);

    // 归零到当天00:00:00，确保跨天但不足24小时、夏令时切换等也能正确处理
    tm_last.tm_hour = tm_last.tm_min = tm_last.tm_sec = 0;
    tm_now.tm_hour = tm_now.tm_min = tm_now.tm_sec = 0;

    // 计算天数（含首尾），用循环递增方式，确保每个日历天都被覆盖
    int days = 0;
    struct tm tm_iter = tm_last;
    while (1) {
        time_t t_iter = mktime(&tm_iter); // 得到本地00:00:00的time_t
        if (t_iter > mktime(&tm_now)) break; // 超过now_time当天则结束
        ++days;
        tm_iter.tm_mday += 1; // 递增到下一天
    }

    // 分配数组，包含所有天的00:00:00
    result.t_base_arr = (time_t*)iot_calloc(days * sizeof(time_t));
    if (!result.t_base_arr) {
        result.days = 0;
        return result;
    }
    result.days = days;
    result.last_time = last_time;
    result.now_time = now_time;

    // 填充每一天的本地00:00:00的time_t
    tm_iter = tm_last;
    for (int i = 0; i < days; ++i) {
        tm_iter.tm_hour = tm_iter.tm_min = tm_iter.tm_sec = 0;
        result.t_base_arr[i] = mktime(&tm_iter);
        tm_iter.tm_mday += 1;
    }

    // 返回结果结构体，调用者需释放t_base_arr
    return result;
}

/**
 * @brief 判断tou_time_base_t区间内是否存在TOU配置的有效触发点（高效且时区安全）
 * 
 * 本函数遍历tou_time_base_t中的每一天的本地time_sec时刻，避免时区误差和效率低下。
 * 适用于任意跨天、跨月、跨年、跨周等场景，且不会遗漏边界触发点。
 * 
 * @param tou_cfg   TOU时间配置结构体指针
 * @param base      tou_time_base_t结构体，包含区间内所有天的本地00:00:00 time_t数组及天数
 * @return int      1-达到触发条件，0-未达到
 */
int tou_time_is_trigger(const tou_time_config_t *tou_cfg, const tou_time_base_t *base)
{
    // 参数有效性检查
    if (!tou_cfg || !base || !base->t_base_arr || base->days <= 0) return 0;

    // 配置无效直接返回
    if ((TOU_TIME_TYPE_POINT != tou_cfg->bits.time_type) || tou_time_cfg_invalid(tou_cfg)) return 0;

    // 遍历区间内每一天
    for (int i = 0; i < base->days; ++i) {
        // 计算本地当天的触发点time_t（当天00:00:00 + 配置秒数）
        time_t t = base->t_base_arr[i] + tou_cfg->bits.time_sec;

        // 只判断(last_time, now_time]区间内的触发点，防止重复或遗漏
        if (t <= base->last_time || t > base->now_time) continue;

        // 转为本地时间，便于判断年月日周配置
        struct tm tm_check;
        localtime_r(&t, &tm_check);

        // 判断月、日、周是否满足配置
        if (!TOU_MONTH_IS_SET(tou_cfg->bits.month_sel, tm_check.tm_mon + 1)) continue;
        if (!TOU_DAY_IS_SET(tou_cfg->bits.day_sel, tm_check.tm_mday)) continue;
        if (!TOU_WEEK_IS_SET(tou_cfg->bits.week_sel, tm_check.tm_wday)) continue;

        // 区间内存在有效触发点，立即返回1
        return 1;
    }

    // 区间内无有效触发点
    return 0;
}

/**
 * @brief 检查当前本地日期是否满足 TOU 配置中的“日/周/月”选择
 *
 * 本函数基于本地时间判断当天是否为配置中指定的有效日子。
 * - 使用本地 time() + localtime_r()，避免时区/夏令时偏差；
 * - 先校验配置有效性，再按月、日、周逐项验证；
 * - 仅判断日期匹配，不涉及具体时间点（time_sec）或区间比较。
 *
 * @param tou_cfg   指向 TOU 时间配置结构体的指针（不得为 NULL）
 * @return int      返回 1 表示“今天”满足配置；返回 0 表示不满足或参数/配置无效
 *
 * 备注：
 * - 函数内部依赖宏/函数：
 *     tou_time_cfg_invalid()  — 检查配置整体有效性
 *     TOU_MONTH_IS_SET(...)  — 判断月份是否在配置集合中
 *     TOU_DAY_IS_SET(...)    — 判断日（1-31）是否在配置集合中
 *     TOU_WEEK_IS_SET(...)   — 判断星期（tm_wday，0=周日）是否在配置集合中
 * - 若需要判断某一具体时间点是否触发，应使用更通用的 tou_time_is_trigger()。
 */
int tou_day_is_valid(const tou_time_config_t *tou_cfg)
{
    // 参数有效性检查
    if (!tou_cfg) return 0;

    // 配置无效直接返回
    if (tou_time_cfg_invalid(tou_cfg)) return 0;

    // 获取当前本地时间并转换为 struct tm
    time_t t = time(NULL);
    struct tm tm_check;
    localtime_r(&t, &tm_check);

    // 按月/日/周依次校验，任意一项不满足即返回 0
    if (!TOU_MONTH_IS_SET(tou_cfg->bits.month_sel, tm_check.tm_mon + 1)) return 0;
    if (!TOU_DAY_IS_SET(tou_cfg->bits.day_sel, tm_check.tm_mday)) return 0;
    if (!TOU_WEEK_IS_SET(tou_cfg->bits.week_sel, tm_check.tm_wday)) return 0;

    return 1;
}

