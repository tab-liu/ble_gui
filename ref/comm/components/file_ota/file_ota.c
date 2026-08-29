/**
  ******************************************************************************
  * @file      file_ota.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/11
  * @brief     文件资源升级模块
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

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "file_ota.h"
#include "filesystem.h"
#include "utils.h"
#include "image_handle.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[FILE_OTA]"

/**
 * @brief 用于 FilePacketInfo_t 结构体的静态初始化器。
 * 
 * 可在声明变量时直接使用，将所有成员初始化为0或NULL。
 */
#define FILE_PACKET_INFO_INITIALIZER {0}

// 字节反转
#define reversal32( a,b,c,d )   ((uint32_t)(a<<24) | (b<<16) | (c<<8) | (d))

/* =============================== 文件内全局变量 ================================ */

// 文件升级数据总大小
static uint32_t File_OTA_Data_Size = 0;

// 当前升级状态标志
static bool File_Is_OTA_Status = false;

// 当前升级进度
static uint8_t File_OTA_Pct = 0;

// 当前升级故障码
static uint8_t File_OTA_Errcode = 0;

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

// flash操作函数
extern esp_err_t iot_image_read(uint32_t address, uint8_t *data, size_t len);

/**
 * @brief 检查指定路径的文件是否存在。
 *
 * @param path 要检查的文件路径。
 * @return bool 如果文件存在，返回 true；否则返回 false。
 */
extern bool fs_file_exists(const char *path);

/**
 * @brief 删除一个文件。
 *
 * @param path 要删除的文件的路径。
 * @return int 成功时返回 0，失败时返回 -1。
 */
extern int fs_file_delete(const char *path);

/**
 * @brief 重命名或移动一个文件。
 *
 * 在大多数现代文件系统（如 LittleFS）上，如果新旧路径在同一个分区，
 * 这个操作是原子的，即它要么完全成功，要么完全不发生，不会产生中间状态。
 *
 * @param old_path 文件的原始路径。
 * @param new_path 文件的新路径。
 * @return int 成功时返回 0，失败时返回 -1。
 */
extern int fs_file_rename(const char *old_path, const char *new_path);

/**
 * @brief 计算指定文件的CRC32校验和。
 *
 * @param path 要计算校验和的文件路径。
 * @param crc_buf 用于读取文件的缓冲区。
 * @param buf_size 缓冲区的大小。
 * @param out_crc 指向uint32_t的指针，用于存储计算出的CRC32结果。
 * @return esp_err_t 成功时返回 ESP_OK，失败时返回 ESP_FAIL。
 */
extern esp_err_t fs_file_crc32(const char *path, uint8_t *crc_buf, size_t buf_size, uint32_t *out_crc);

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

// flash读取函数
esp_err_t iot_image_read(uint32_t address, uint8_t *data, size_t len) 
{
    esp_err_t err;

    // Find the next OTA update partition
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    
    // read the data, starting from the beginning of the partition
    err = esp_partition_read(partition, address, data, len);
    return err;
}

/**
 * @brief 检查指定路径的文件是否存在。
 *
 * @param path 要检查的文件路径。
 * @return bool 如果文件存在，返回 true；否则返回 false。
 */
bool fs_file_exists(const char *path)
{
    // stat 是一个标准的 POSIX 函数，用于获取文件状态。
    // 如果函数成功返回0，说明文件或目录存在。
    struct stat st;
    if (stat(path, &st) == 0) {
        // 确认它是一个常规文件而不是目录
        return S_ISREG(st.st_mode);
    }
    // 如果 stat 返回非0，说明文件不存在或路径无效。
    return false;
}

/**
 * @brief 删除一个文件。
 *
 * @param path 要删除的文件的路径。
 * @return int 成功时返回 0，失败时返回 -1。
 */
int fs_file_delete(const char *path)
{
    // remove 是一个标准的C库函数，用于删除文件。
    // 它在 ESP-IDF VFS 中被正确地路由到相应的文件系统驱动。
    if (remove(path) == 0) {
        return 0; // 成功
    } else {
        return -1; // 失败
    }
}

/**
 * @brief 重命名或移动一个文件。
 *
 * 在大多数现代文件系统（如 LittleFS）上，如果新旧路径在同一个分区，
 * 这个操作是原子的，即它要么完全成功，要么完全不发生，不会产生中间状态。
 *
 * @param old_path 文件的原始路径。
 * @param new_path 文件的新路径。
 * @return int 成功时返回 0，失败时返回 -1。
 */
int fs_file_rename(const char *old_path, const char *new_path)
{
    // rename 是一个标准的C库函数，用于重命名文件。
    // 这是实现原子性文件替换的关键。
    if (rename(old_path, new_path) == 0) {
        return 0; // 成功
    } else {
        return -1; // 失败
    }
}

/**
 * @brief 计算指定文件的CRC32校验和。
 *
 * @param path 要计算校验和的文件路径。
 * @param crc_buf 用于读取文件的缓冲区。
 * @param buf_size 缓冲区的大小。
 * @param out_crc 指向uint32_t的指针，用于存储计算出的CRC32结果。
 * @return esp_err_t 成功时返回 ESP_OK，失败时返回 ESP_FAIL。
 */
esp_err_t fs_file_crc32(const char *path, uint8_t *crc_buf, size_t buf_size, uint32_t *out_crc)
{
    if (path == NULL || crc_buf == NULL || buf_size == 0 || out_crc == NULL) {
        return ESP_FAIL;
    }

    // 获取文件操作信号量，确保线程安全
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for CRC calculation: %s", path);
        xSemaphoreGive(xFileSemaphore);
        return ESP_FAIL;
    }
    
    // 设置CRC32初始值
    uint32_t current_crc = 0xFFFFFFFF; 
    size_t bytes_read;
    
    // 逐块读取文件并更新CRC值
    do {
        bytes_read = fread(crc_buf, 1, buf_size, f);
        if (bytes_read > 0) {
            // 上一次的CRC结果作为本次计算的初始值。
            if ( bytes_read % 4 == 0 ) {
                current_crc = calcu_crc32(current_crc, crc_buf, bytes_read);
            } else {
                // 用0补齐到4字节
                size_t pad = 4 - (bytes_read % 4);
                memset(crc_buf + bytes_read, 0, pad);
                current_crc = calcu_crc32(current_crc, crc_buf, bytes_read + pad);
            }
        }
    } while (bytes_read > 0);

    // 检查是否正常到达文件末尾，否则视为读取错误
    if (!feof(f)) {
        ESP_LOGE(TAG, "Error reading file during CRC calculation: %s", path);
        fclose(f);
        xSemaphoreGive(xFileSemaphore);
        return ESP_FAIL;
    }

    fclose(f);
    xSemaphoreGive(xFileSemaphore);
    
    *out_crc = Swap32(current_crc); // 将最终结果存入输出参数
    return ESP_OK;
}

#endif

/* ================================ 模块函数定义 ================================ */

/**
 * @brief 跳过镜像文件末尾的填充字节（0x1A），并更新剩余数据长度。
 *
 * @param len 指向剩余数据长度的指针，函数执行后会更新为实际剩余长度。
 * @return esp_err_t 成功返回 ESP_OK，读取失败返回 ESP_FAIL。
 */
static esp_err_t skip_image_padding(uint32_t *len)
{
    uint32_t remaining = *len;

    for (int i = 0; i < 2048 && remaining > 0; i++) {
        uint8_t data = 0;
        if (iot_image_read(IMAGE_FLASH_AREA_ADDRESS, &data, 1) == ESP_OK) {
            if (data != 0x1A) { // 遇到非填充字节
                remaining -= 20; // 跳过20字节填充信息
                break;
            }
            remaining -= 1;
        } else {
            ESP_LOGE(TAG, "Image read failed at offset %d", i);
            return ESP_FAIL;
        }
    }

    *len = remaining;
    return ESP_OK;
}


/**
 * @brief 检查当前Flash偏移是否为填充区域，如果是，则计算下一个对齐的偏移地址。
 *
 * @param current_offset 当前正在检查的Flash偏移地址。
 * @param p_next_offset  如果检测到填充，此指针指向的地址将被更新为下一个对齐的偏移地址。
 * @return bool           如果当前位置是填充区域，则返回 true；否则返回 false。
 */
static bool CheckAndSkipPadding(uint32_t current_offset, uint32_t *p_next_offset)
{
    // --- 1. 计算到下一个对齐边界的距离 ---
    uint32_t relative_offset = current_offset - IMAGE_FLASH_AREA_ADDRESS;
    uint32_t next_aligned_relative_offset = ((relative_offset / XMODEM_PAYLOAD_SIZE) + 1) * XMODEM_PAYLOAD_SIZE;
    uint32_t bytes_to_next_alignment = next_aligned_relative_offset - relative_offset;

    // --- 2. 确定要检查的字节数 ---
    // 取 "到下一个对齐点的字节数" 和 "文件固定头长度" 中的较小者。
    size_t bytes_to_check = (bytes_to_next_alignment < FILE_HEADER_FIXED_LEN) ? bytes_to_next_alignment : FILE_HEADER_FIXED_LEN;

    // 如果计算出的检查字节数为0（意味着当前已对齐），则不认为是填充。
    if (bytes_to_check == 0) {
        return false;
    }

    // --- 3. 读取并检查数据 ---
    uint8_t check_buf[bytes_to_check];
    if (iot_image_read(current_offset, check_buf, bytes_to_check) != ESP_OK) {
        // 如果连检查所需的数据都读不出来，则认为不是填充。
        return false;
    }

    for (int i = 0; i < bytes_to_check; i++) {
        if (check_buf[i] != 0xFF) {
            // 只要有一个字节不是0xFF，就不是填充区域。
            return false;
        }
    }

    // --- 4. 确认是填充，并计算下一个地址 ---
    // 如果所有检查的字节都是0xFF，则确认是填充区域。
    *p_next_offset = IMAGE_FLASH_AREA_ADDRESS + next_aligned_relative_offset;
    ESP_LOGW(TAG, "Detected padding at offset 0x%08lX. Jumping to next aligned offset: 0x%08lX", 
             (unsigned long)current_offset, (unsigned long)*p_next_offset);
             
    return true;
}


/**
 * @brief 从 Flash 的指定偏移读取并解析文件头信息。
 *
 * 该函数从 Flash 的 block_flash_offset 地址开始，读取文件头所需的最大数据量，
 * 然后解析出协议中定义的所有字段，并填充到 out_info 结构体中。
 * 文件路径不再判断是否有 ".bin"，而是统一规范化路径分隔符，并追加 FS_BASE_PATH。
 * 备份文件名在路径最后追加 "_bp"。
 *
 * @param block_flash_offset      要读取的文件头的 Flash 起始地址。
 * @param out_info                指向 FilePacketInfo_t 结构体的指针，用于存储解析结果。
 * @return static int             成功时返回 0，失败时（如Flash读取失败、数据头无效等）返回 -1。
 */
static int ParseFileHeaderFromFlash(uint32_t block_flash_offset, FilePacketInfo_t *out_info)
{
    // --- 1. 参数校验 ---
    if (out_info == NULL) {
        ESP_LOGE(TAG, "Invalid arguments (out_info is NULL).");
        File_OTA_Errcode = OTA_STATUS_FILE_NULL;
        return -1;
    }

    // --- 2. 从 Flash 读取文件头数据 ---
    const uint32_t File_Header_Max_Len = FILE_HEADER_FIXED_LEN + FILE_NAME_MAX_LEN;
    uint8_t block_buffer[File_Header_Max_Len];
    
    // iot_image_read返回ESP_OK或ESP_FAIL
    if (iot_image_read(block_flash_offset, block_buffer, File_Header_Max_Len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read header from flash at offset %lu.", (unsigned long)block_flash_offset);
        File_OTA_Errcode = OTA_STATUS_FLASH_READ_ERROR;
        return -1;
    }

    // --- 3. 解析文件头 ---
    const uint8_t *header_ptr = &block_buffer[0];

    // 解析路径长度，并进行合法性检查
    uint16_t path_len = (header_ptr[7] << 8) | header_ptr[6];
    if (path_len == 0 || path_len >= sizeof(out_info->original_name)) {
        ESP_LOGE(TAG, "Invalid path length in header: %u", path_len);
        File_OTA_Errcode = OTA_STATUS_FILE_ERROR;
        return -1;
    }
    if (FILE_HEADER_FIXED_LEN + path_len > File_Header_Max_Len) {
        ESP_LOGE(TAG, "Incomplete file header in the read data. Required: %u, Available: %ld", FILE_HEADER_FIXED_LEN + path_len, File_Header_Max_Len);
        File_OTA_Errcode = OTA_STATUS_FILE_ERROR;
        return -1;
    }

    // --- 4. 填充 out_info 结构体 ---
    // 假设协议字段为小端序，直接使用移位操作进行组合。
    // 如果是大端序，需要调整移位顺序。

    // 解析并填充协议中的所有固定字段（起始信息的所有格式均为低位在前，高位在后）
    out_info->file_id           = (header_ptr[1] << 8) | header_ptr[0];
    out_info->crc32             = reversal32(header_ptr[5], header_ptr[4], header_ptr[3], header_ptr[2]);
    out_info->path_length       = path_len; // path_len 已经解析过了
    out_info->file_length       = reversal32(header_ptr[11], header_ptr[10], header_ptr[9], header_ptr[8]);
    out_info->modification_time = reversal32(header_ptr[15], header_ptr[14], header_ptr[13], header_ptr[12]);
    out_info->extra_info        = reversal32(header_ptr[19], header_ptr[18], header_ptr[17], header_ptr[16]);

    // 解析并填充运行时字段（文件名）
    char raw_path[FILE_NAME_MAX_LEN + 1] = {0};
    memcpy(raw_path, &header_ptr[FILE_HEADER_FIXED_LEN], path_len);
    raw_path[path_len] = '\0'; // 确保字符串以 null 结尾

#if 1
    // 打印解析结果
    ESP_LOGW(TAG, "FileHeader: id=0x%04X, crc32=0x%08lX, path_len=%u, file_len=%lu, mtime=%lu, extra=0x%08lX, path='%s'",
        out_info->file_id,
        (unsigned long)out_info->crc32,
        out_info->path_length,
        (unsigned long)out_info->file_length,
        (unsigned long)out_info->modification_time,
        (unsigned long)out_info->extra_info,
        raw_path
    );
#endif

    // --- 新增：规范化路径分隔符，统一为 '/' ---
    // 替换路径中的 ":", ":/", ":\\", "//", "\" 为 "/"
    char norm_path[FILE_NAME_MAX_LEN + 1] = {0};
    int j = 0;
    for (int i = 0; i < path_len && j < FILE_NAME_MAX_LEN; i++) {
        char c = raw_path[i];
        // 处理 ":/" 或 ":\\"
        if ((c == ':' && (raw_path[i + 1] == '/' || raw_path[i + 1] == '\\'))) {
            norm_path[j++] = '/';
            i++; // 跳过组合的第二个字符
        }
        // 处理 "//"
        else if (c == '/' && raw_path[i + 1] == '/') {
            norm_path[j++] = '/';
            i++; // 跳过第二个 '/'
        }
        // 处理单独的 ':' 或 '\'
        else if (c == ':' || c == '\\') {
            norm_path[j++] = '/';
        }
        else {
            norm_path[j++] = c;
        }
    }
    norm_path[j] = '\0';

    // --- 新增：拼接 FS_BASE_PATH 作为前缀 ---
    // 拼接 FS_BASE_PATH + "/" + 规范化路径
    snprintf(out_info->original_name, sizeof(out_info->original_name), "%s/%s", FS_BASE_PATH, norm_path);

    // --- 新增：备份文件名在路径最后追加 "_bp" ---
    size_t orig_len = strlen(out_info->original_name);
    size_t max_backup_len = sizeof(out_info->backup_name) - 1;
    if (orig_len + 3 > max_backup_len) { // "_bp" 长度为3
        ESP_LOGW(TAG, "Backup name too long, truncating.");
        orig_len = max_backup_len - 3;
    }
    snprintf(out_info->backup_name, sizeof(out_info->backup_name), "%.*s_bp", (int)orig_len, out_info->original_name);

    return 0; // 所有步骤成功完成，返回成功标志。
}

/**
 * @brief 将指定长度的数据从Flash的一个区域拷贝到文件系统中的一个文件。
 *
 * @param dest_path           目标文件的路径（例如，备份文件名）。
 * @param source_start_offset 源数据在Flash中的起始地址。
 * @param total_size          要拷贝的数据总长度（字节）。
 * @param buffer              用于拷贝操作的临时缓冲区。
 * @param buffer_size         缓冲区的大小。
 * @return bool               如果拷贝完全成功，返回 true；否则返回 false。
 */
static bool CopyFileFromFlashToFileSystem(const char* dest_path, uint32_t source_start_offset, uint32_t total_size, uint8_t* buffer, size_t buffer_size)
{
    uint32_t remaining_bytes = total_size;
    uint32_t current_source_offset = source_start_offset;
    uint32_t current_dest_offset = 0;

    ESP_LOGI(TAG, "Starting copy of %lu bytes from 0x%08lX to '%s'.", (unsigned long)total_size, (unsigned long)source_start_offset, dest_path);

    while (remaining_bytes > 0) {
        // 计算本次要拷贝的字节数：取 缓冲区大小 和 剩余字节数 中的较小者。
        uint32_t bytes_to_copy = (remaining_bytes > buffer_size) ? buffer_size : remaining_bytes;

        // 从Flash源地址读取数据到缓冲区。
        if (iot_image_read(current_source_offset, buffer, bytes_to_copy) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read data from flash offset 0x%08lX", (unsigned long)current_source_offset);
            File_OTA_Errcode = OTA_STATUS_FLASH_READ_ERROR;
            return false; // 读取失败
        }

        // 将缓冲区的数据写入目标备份文件。
        if (fs_file_write_at(dest_path, current_dest_offset, buffer, bytes_to_copy) != bytes_to_copy) {
            ESP_LOGE(TAG, "Failed to write data to file '%s' at offset 0x%08lX", dest_path, (unsigned long)current_dest_offset);
            File_OTA_Errcode = OTA_STATUS_FLASH_WRITE_ERROR;
            return false; // 写入失败
        }

        // 更新各个偏移量，为下一次循环做准备。
        remaining_bytes -= bytes_to_copy;
        current_source_offset += bytes_to_copy;
        current_dest_offset += bytes_to_copy;

        // 计算升级进度
        File_OTA_Pct = ((current_source_offset - IMAGE_FLASH_AREA_ADDRESS) * 100) / File_OTA_Data_Size;
        vTaskDelay(pdMS_TO_TICKS(1)); // 短暂让出线程，避免看门狗超时
    }

    return true; // 所有数据拷贝完成，返回成功
}

/**
 * @brief 验证备份文件的CRC，如果正确，则通过原子重命名来激活它。
 *
 * @param file_info   包含文件所有信息（CRC、文件名等）的结构体指针。
 * @param buffer      用于CRC计算的临时缓冲区。
 * @param buffer_size 缓冲区的大小。
 * @return bool       如果验证和激活都成功，返回 true；否则返回 false。
 */
static bool VerifyAndActivateNewFile(const FilePacketInfo_t *file_info, uint8_t *buffer, size_t buffer_size)
{
    ESP_LOGI(TAG, "Verifying CRC32 of temporary file '%s'...", file_info->backup_name);
    
    uint32_t calculated_crc = 0;
    // 1. 调用CRC计算函数
    if (fs_file_crc32(file_info->backup_name, buffer, buffer_size, &calculated_crc) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate CRC for temp file '%s'.", file_info->backup_name);
        File_OTA_Errcode = OTA_STATUS_FLASH_READ_ERROR;
        return false;
    } 
    
    // 2. 比较计算出的CRC和文件头中记录的CRC
    if (calculated_crc != file_info->crc32) {
        ESP_LOGE(TAG, "CRC32 MISMATCH! Expected: 0x%08lX, Calculated: 0x%08lX.", file_info->crc32, calculated_crc);
        File_OTA_Errcode = OTA_STATUS_CRC32_ERROR;
        return false;
    } 
    
    // 3. CRC校验成功，执行原子替换
    ESP_LOGI(TAG, "CRC32 verification successful. Atomically replacing original file.");
    
    if (fs_file_rename(file_info->backup_name, file_info->original_name) != 0) {
        // 如果 rename 失败，这是一个严重的系统错误。
        // 但此时系统仍然是安全的：旧的 original_name 文件仍然存在且有效。
        ESP_LOGE(TAG, "CRITICAL: Failed to rename '%s' to '%s'. The original file is safe, but the update failed.", 
                 file_info->backup_name, file_info->original_name);
        File_OTA_Errcode = OTA_STATUS_ABORT;
        return false;
    }
    
    // 4. rename 成功，新文件已安全生效。
    ESP_LOGI(TAG, "Successfully updated file '%s'.", file_info->original_name);
    return true;
}


/**
 * @brief OTA文件处理任务
 * 
 * 该任务从指定的Flash区域 (IMAGE_FLASH_AREA_ADDRESS) 开始，
 * 循环解析文件头，并以原子更新的方式将每个文件的内容安全地拷贝到文件系统中。
 * 原子更新确保了即使在更新过程中发生中断，旧的有效文件也不会被破坏。
 */
void File_OTA_Task(void *pvParameters)
{
    // 1. 初始化变量和资源
    // 从任务参数中获取OTA数据总长度
    File_OTA_Data_Size = (uint32_t)pvParameters;

    // 处理OTA数据总长度，忽略填充字节0x1A及尾部CRC，避免解析失败
    if ( skip_image_padding(&File_OTA_Data_Size) != ESP_OK ) {
        ESP_LOGE(TAG, "Failed to Skip Image Padding!");
        vTaskDelete(NULL); // 内存分配失败，任务无法继续。
        return;
    }
    
    // current_flash_offset: 追踪当前在源Flash数据区中处理到的位置。
    uint32_t current_flash_offset = IMAGE_FLASH_AREA_ADDRESS;
    // end_flash_address: 标记源Flash数据区的结束位置，用于循环终止判断。
    const uint32_t end_flash_address = IMAGE_FLASH_AREA_ADDRESS + File_OTA_Data_Size;
    
    // 为文件拷贝操作分配一个缓冲区，可以显著提高I/O效率。
    uint8_t *file_copy_buf = iot_calloc(FLASH_SEC_SIZE);
    if (file_copy_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for file copy buffer!");
        vTaskDelete(NULL); // 内存分配失败，任务无法继续。
        return;
    }

    File_Is_OTA_Status = true;
    File_OTA_Pct = 0;
    File_OTA_Errcode = OTA_STATUS_OK;
    ESP_LOGI(TAG, "Starting File OTA process. Total data length: %lu bytes.", (unsigned long)File_OTA_Data_Size);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. 主循环：遍历整个OTA数据区，处理每一个文件。
    while (current_flash_offset < end_flash_address)
    {
        // 文件间升级间隔
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // 在每次循环开始时，创建一个新的文件信息结构体实例。
        FilePacketInfo_t file_info = FILE_PACKET_INFO_INITIALIZER;

        // 先检查是否为填充区域
        uint32_t next_offset = 0;
        if (CheckAndSkipPadding(current_flash_offset, &next_offset)) {
            // 是填充区域，更新偏移并继续下一次循环
            current_flash_offset = next_offset;
            // 计算升级进度
            File_OTA_Pct = ((current_flash_offset - IMAGE_FLASH_AREA_ADDRESS) * 100) / File_OTA_Data_Size;
            continue;
        } 
        
        // 2.1 解析下一个文件头。
        if (ParseFileHeaderFromFlash(current_flash_offset, &file_info) != 0) {
            // 不是填充区域，判定为真正的数据损坏，中止OTA
            ESP_LOGE(TAG, "Failed to parse file header at offset 0x%08lX and it's not a padding area. Aborting.", (unsigned long)current_flash_offset);
            break; // 退出主循环。
        }

        ESP_LOGI(TAG, "Found file: '%s', Size: %lu bytes. Using temp name: '%s'", 
                 file_info.original_name, (unsigned long)file_info.file_length, file_info.backup_name);

        // 2.2 写入前，确保备份文件不存在，保证从零开始写入。
        if (fs_file_exists(file_info.backup_name)) {
            ESP_LOGW(TAG, "Temporary file '%s' already exists. Deleting it.", file_info.backup_name);
            if (fs_file_delete(file_info.backup_name) != 0) {
                ESP_LOGE(TAG, "Failed to delete existing temporary file. Aborting file copy.");
                File_OTA_Errcode = OTA_STATUS_ABORT;
                break; // 这是一个严重错误，可能文件系统有问题，直接终止OTA。
            }
        }

        // 2.3 计算文件数据在源Flash中的起始地址，并检查文件大小
        uint32_t file_data_start_offset = current_flash_offset + FILE_HEADER_FIXED_LEN + file_info.path_length;
        if ( file_data_start_offset + file_info.file_length > end_flash_address ) {
            // 文件头内注明的文件大小超过了本次接收的升级数据
            ESP_LOGE(TAG, "File length is error! Header_File_length: %lu bytes, OTA_Data_Size remain: %lu bytes", file_info.file_length, (end_flash_address - file_data_start_offset));
            File_OTA_Errcode = OTA_STATUS_SIZE_ERROR;
            break; // 退出主循环
        }
        
        // 2.4 将文件内容从Flash完整地拷贝到临时的备份文件中。
        bool copy_success = CopyFileFromFlashToFileSystem(
            file_info.backup_name,
            file_data_start_offset,
            file_info.file_length,
            file_copy_buf,
            FLASH_SEC_SIZE
        );

        // 2.5 如果拷贝完全成功，则执行原子替换操作。
        if (copy_success) {
            copy_success = VerifyAndActivateNewFile(&file_info, file_copy_buf, FLASH_SEC_SIZE);
        }

        // 如果整个过程（包括拷贝和重命名）有任何失败，则立即终止OTA。
        if (!copy_success) {
            ESP_LOGE(TAG, "Aborting OTA process due to file update error for '%s'.", file_info.original_name);
            // 作为清理，尝试删除可能已创建的不完整的备份文件。
            if (fs_file_exists(file_info.backup_name)) {
                fs_file_delete(file_info.backup_name);
            }
            if(!File_OTA_Errcode) File_OTA_Errcode = OTA_STATUS_ABORT;
            break;
        }

        // 2.6 更新当前Flash偏移，使其指向下一个文件头，准备处理下一个文件。
        current_flash_offset = file_data_start_offset + file_info.file_length;

        // 计算升级进度
        File_OTA_Pct = ((current_flash_offset - IMAGE_FLASH_AREA_ADDRESS) * 100) / File_OTA_Data_Size;
        ESP_LOGI(TAG, "File Updating: %3lu, total: %3lu, pct: %hhu%%", 
                        (current_flash_offset - IMAGE_FLASH_AREA_ADDRESS), File_OTA_Data_Size, File_OTA_Pct);
    }

    // 等待升级进度上报
    if ( File_OTA_Errcode ) {
        ESP_LOGE(TAG, "File OTA End, errcode: %d", File_OTA_Errcode);
    } else {
        ESP_LOGI(TAG, "File OTA End, pct: %hhu%%", File_OTA_Pct);
    }
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 3. 任务结束，清理资源。
    ESP_LOGI(TAG, "File OTA process finished.");
    File_Is_OTA_Status = false;
    
    free(file_copy_buf); // 释放之前分配的缓冲区内存。
    vTaskDelete(NULL);   // 删除当前任务，释放其堆栈和TCB。
}

/**
 * @brief 创建并启动文件OTA处理任务。
 *
 * @param ota_data_len 要处理的OTA数据的总长度（字节）。
 * @return esp_err_t 成功时返回 ESP_OK，失败时返回 ESP_FAIL。
 */
esp_err_t Start_File_OTA_Task(uint32_t ota_data_len)
{
    // 检查输入参数的有效性
    if (ota_data_len == 0) {
        ESP_LOGE(TAG, "Cannot start File OTA task with zero data length.");
        return ESP_FAIL;
    }

    // 使用 xTaskCreate 来创建任务
    // 在32位系统上，可以将一个 uint32_t 值直接强制转换为 void* 来传递，这是一种高效的技巧。
    BaseType_t result = xTaskCreate(
        File_OTA_Task,                  // 任务函数指针
        "File_OTA_Task",                // 任务名 (用于调试)
        5*1024,                           // 任务堆栈大小 (字节)
        (void *)ota_data_len,           // 传递给任务的参数
        5,                              // 任务优先级 (中等优先级)
        NULL                            // 任务句柄 (如果不需要，则为NULL)
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create File_OTA_Task.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File_OTA_Task created successfully.");
    return ESP_OK;
}

/**
 * @brief 获取当前文件升级状态标志
 * @return bool 当前升级状态（true: 正在升级，false: 未升级）
 */
bool Get_File_OTA_Status(void)
{
    return File_Is_OTA_Status;
}

/**
 * @brief 获取当前文件升级进度
 * @return uint8_t 当前升级进度（0~100）
 */
uint8_t Get_File_OTA_Pct(void)
{
    return File_OTA_Pct;
}

/**
 * @brief 获取当前文件升级故障码
 * @return uint8_t 当前升级故障码
 */
uint8_t Get_File_OTA_Errcode(void)
{
    return File_OTA_Errcode;
}

/**
 * @brief 获取当前文件数据版本号
 * @return uint32_t 当前文件数据版本号
 */
uint32_t Get_File_Data_Version(void)
{
    // 从指定文件中读取版本号字符串
    uint8_t verstr[ver_str_len] = {0};
    if (ver_str_len != fs_file_read_at(FILE_DATA_MARK_VERSION_PATH, 0, verstr, ver_str_len)) {
        ESP_LOGE(TAG, "Failed to Get File Data Version.");
        return 0;
    }

    // 计算版本号字符串长度，过滤“.”和0xFF
    char numstr[ver_str_len] = {0};
    uint8_t j = 0;
    for (uint8_t i = 0; i < ver_str_len; i++) {
        if (verstr[i] == 0xFF) break;
        if (verstr[i] >= '0' && verstr[i] <= '9') {
            numstr[j++] = verstr[i];
        }
    }
    numstr[j] = '\0';

    // 防止空字符串导致转换错误
    if (j == 0) {
        ESP_LOGE(TAG, "Version string is empty or invalid.");
        return 0;
    }

    // 转为 u32
    uint32_t value = (uint32_t)strtoul(numstr, NULL, 10);    
    ESP_LOGI(TAG, "Get_File_Data_Version Successfully, ver = %lu", value);
    
    return value;
}

