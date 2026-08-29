#include "filesystem.h"
#include "comm_define.h"

#include "esp_vfs.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <dirent.h>

#include "esp_system.h"
#include "esp_err.h"
#include "crc.h"
#include "image_handle.h"
#include "utils.h"

#define TAG "[FileSystem]"

static SemaphoreHandle_t xFileSemaphore = NULL;

static void fs_file_list(const char *path);

void fs_mount(void)
{
    xFileSemaphore = xSemaphoreCreateMutex();
    if (xFileSemaphore == NULL)
    {
        return;
    }

    ESP_LOGI(TAG, "Initializing LittelFS");

    esp_vfs_littlefs_conf_t conf =
        {
            .base_path = FS_BASE_PATH,
            .partition_label = FS_PARTITION_NAME,
            .format_if_mount_failed = true,
            .dont_mount = false,
        };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    fs_file_list(FS_BASE_PATH);
}

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
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777); // 忽略已存在目录的错误
            *p = '/';
        }
    }
    return 0;
}

/**
 * @brief 读取数据
 * @param path 文件路径
 * @param data 数据指针
 * @param len 数据长度
 * @return 成功返回0,失败返回-1 
 */
int fs_file_read(const char *path, uint8_t *data, int size)
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
    if (fseek(file, 0, SEEK_SET) != 0) {
        read_bytes = -1; // fseek 失败
    } else {
        // 4. 从当前位置读取指定大小的数据
        // fread 会返回成功读取的元素个数。如果到达文件末尾，返回值会小于请求的size。
        read_bytes = fread(data, 1, size, file);
        if (read_bytes < size) {
            ESP_LOGE(TAG, "Error occurred while reading file: %s", path);
            read_bytes = -1; // 读取错误
        }
    }

    // 5. 清理并返回
    fclose(file);
    xSemaphoreGive(xFileSemaphore);

    return (read_bytes == -1) ? read_bytes : 0;
}

/**
 * @brief 写入数据到文件
 * @param path 文件路径
 * @param data 数据指针
 * @param size 数据长度
 * @return 成功返回0,失败返回-1 
 */
int fs_file_write(const char *path, uint8_t *data, uint32_t offset, int size)
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
        fs_mkdirs(path); // 新增：确保目录存在
        file = fopen(path, "wb");
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

    return (written_bytes == -1) ? written_bytes : 0;
}

int fs_dir_create(const char *path)
{
    char name[strlen(path)+1];
    strcpy(name, path);

    char dir_path[100] = {0};
    dir_path[0] = '/'; // 从空字符串开始

    char *p;
    char *token = strtok_r(name, "/", &p);

    /* 检查路劲中的目录是否存在 */
    while(token != NULL)
    {
        char buffer[strlen(token)+1];
        strcpy(buffer, token);

        token = strtok_r(NULL, "/", &p);
        if(token != NULL)
        {
            strcat(dir_path, buffer);

            DIR *dir = opendir(dir_path);
            if(dir == NULL) 
            {
                //目录不存在则创建目录
                ESP_LOGW(TAG, "create dir (%s)",dir_path);
                mkdir(dir_path, 0777);
            }
            else
            {
                closedir(dir);
            }

            strcat(dir_path, "/");
        }
    }

    return 0;
}

static void fs_file_list(const char *path)
{
    DIR *dir = NULL;
    struct dirent *ptr;
    struct stat file_stat;
    char base[255];
    char full_path[300];
    static size_t total_size = 0;
    static int file_count = 0;
    static int dir_count = 0;
    static bool is_root_call = true;

    // 如果是根调用，重置统计信息
    if (is_root_call)
    {
        total_size = 0;
        file_count = 0;
        dir_count = 0;
        is_root_call = false;

        // 显示文件系统信息
        size_t fs_total = 0, fs_used = 0;
        esp_err_t ret = esp_littlefs_info(FS_PARTITION_NAME, &fs_total, &fs_used);
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "=== LittleFS File System ===");
            ESP_LOGI(TAG, "Total: %zu bytes (%.2f KB)", fs_total, fs_total / 1024.0);
            ESP_LOGI(TAG, "Used:  %zu bytes (%.2f KB)", fs_used, fs_used / 1024.0);
            ESP_LOGI(TAG, "Free:  %zu bytes (%.2f KB)", fs_total - fs_used, (fs_total - fs_used) / 1024.0);
            ESP_LOGI(TAG, "Usage: %.1f%%", (float)fs_used / fs_total * 100);
        }
        ESP_LOGI(TAG, "\n=== File List ===");
    }

    if ((dir = opendir(path)) == NULL)
    {
        ESP_LOGE(TAG, "Open dir %s error...", path);
        return;
    }

    while ((ptr = readdir(dir)) != NULL)
    {
        // 忽略当前目录和上级目录
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
        {
            continue;
        }

        // 构建完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", path, ptr->d_name);
        // 获取文件/目录状态信息
        if (stat(full_path, &file_stat) == 0)
        {
            if (ptr->d_type == DT_DIR || S_ISDIR(file_stat.st_mode))
            {
                // 目录
                ESP_LOGI(TAG, "[DIR]  %s/", full_path);
                dir_count++;

                // 递归处理子目录
                memset(base, '\0', sizeof(base));
                strcpy(base, path);
                strcat(base, "/");
                strcat(base, ptr->d_name);
                fs_file_list(base);
            }
            else
            {
                // 文件
                ESP_LOGI(TAG, "[FILE] %s - %ld bytes (%.2f KB)",
                         full_path, file_stat.st_size, file_stat.st_size / 1024.0);
                total_size += file_stat.st_size;
                file_count++;
            }
        }
        else
        {
            // 无法获取文件状态，仍然显示
            if (ptr->d_type == DT_DIR)
            {
                ESP_LOGI(TAG, "[DIR]  %s/ (stat failed)", full_path);
                dir_count++;

                memset(base, '\0', sizeof(base));
                strcpy(base, path);
                strcat(base, "/");
                strcat(base, ptr->d_name);
                fs_file_list(base);
            }
            else
            {
                ESP_LOGI(TAG, "[FILE] %s (stat failed)", full_path);
                file_count++;
            }
        }
    }

    closedir(dir);

    // 如果回到根目录级别，显示统计信息
    if (strcmp(path, FS_BASE_PATH) == 0)
    {
        ESP_LOGI(TAG, "\n=== Summary ===");
        ESP_LOGI(TAG, "Total directories: %d", dir_count);
        ESP_LOGI(TAG, "Total files: %d", file_count);
        ESP_LOGI(TAG, "Total file size: %zu bytes (%.2f KB)", total_size, total_size / 1024.0);
        is_root_call = true; // 重置标志以备下次调用
    }

    return;
}

// 递归删除目录及其所有内容
int fs_delete_dir_recursive(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open dir: %s", dir_path);
        return -1;
    }
    struct dirent *entry;
    char path[300];
    int ret = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            // 递归删除子目录
            fs_delete_dir_recursive(path);
        } else {
            // 删除文件
            if (remove(path) == 0) {
                ESP_LOGW(TAG, "[fs_delete_dir_recursive] delete file: %s", path);
            } else {
                ESP_LOGE(TAG, "[fs_delete_dir_recursive] failed to delete file: %s", path);
                ret = -1;
            }
        }
    }
    closedir(dir);
    // 删除空目录
    if (rmdir(dir_path) == 0) {
        ESP_LOGW(TAG, "[fs_delete_dir_recursive] deleted directory: %s", dir_path);
    } else {
        ESP_LOGE(TAG, "[fs_delete_dir_recursive] failed to delete directory: %s", dir_path);
        ret = -1;
    }
    return ret;
}
int fs_file_check(void)
{
    char path[30] = {0};
    char subpath[300] = {0};

    DIR *dir;
    struct dirent *p_dir;

    snprintf(path, sizeof(path), "%s", FS_BASE_PATH);

    dir = opendir(path);
    if(dir == NULL)
    {
        ESP_LOGE(TAG, "[file_list_check] opendir error");
        return -1;
    }

    while((p_dir = readdir(dir)) != NULL)
    {
        if(strcmp(p_dir->d_name,".")==0 || strcmp(p_dir->d_name,"..")==0)
        {
            continue;
        }

        snprintf(subpath, sizeof(subpath), "%s/%s", FS_BASE_PATH, p_dir->d_name);

        struct stat st;
        if (stat(subpath, &st) == 0 && S_ISDIR(st.st_mode))
        {
#ifdef FILE_SYSTEM_DIRECTORY_ENABLE
            // 如果目录名长度不为1，删除该目录
            if (strlen(p_dir->d_name) != 1) {
                fs_delete_dir_recursive(subpath);
                continue;
            }

            // 保留一层目录及目录下的文件，删除子目录及子目录下的文件
            DIR *subdir = opendir(subpath);
            if(subdir == NULL) {
                ESP_LOGE(TAG, "[file_list_check] opendir error: %s", subpath);
                continue;
            }
            struct dirent *p_subdir;
            char subfile[300];
            while((p_subdir = readdir(subdir)) != NULL)
            {
                if(strcmp(p_subdir->d_name,".")==0 || strcmp(p_subdir->d_name,"..")==0)
                    continue;
                snprintf(subfile, sizeof(subfile), "%s/%s", subpath, p_subdir->d_name);
                struct stat st2;
                if (stat(subfile, &st2) == 0 && S_ISDIR(st2.st_mode)) {
                    fs_delete_dir_recursive(subfile); // 递归删除二级及以下目录
                }
                // 如果是文件则保留，不做删除
            }
            closedir(subdir);
#else
            // 未定义时，直接递归删除整个目录
            fs_delete_dir_recursive(subpath);
#endif
        }
        else
        {
#ifdef FILE_SYSTEM_DIRECTORY_ENABLE
            remove(subpath);
            ESP_LOGW(TAG, "[file_list_check] delete : %s", subpath);
#endif
        }
    }

    closedir(dir);
    return 0;
}

/**
 * @brief 获取文件大小
 * @param path 文件路径
 * @return 读取成功返回文件大小，失败返回-1
 */
int fs_file_size(const char *path)
{
    int file_size = -1;

    if (NULL == path)
    {
        return -1;
    }

    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

    FILE *file = fopen(path, "rb"); 
    if (file)
    {
        // 移动文件指针到文件末尾，通过ftell获取文件大小
        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        fclose(file);
    }

    xSemaphoreGive(xFileSemaphore); 
    return file_size;
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
int fs_record_write(const char *path, void *record, uint32_t start_offset, uint16_t record_index, uint16_t record_len, uint16_t record_cnt)
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
        fs_mkdirs(path); // 新增：确保目录存在
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
        fs_mkdirs(path); // 新增：确保目录存在
        file = fopen(path, "wb");
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
 * @brief 打开文件
 * @param path 文件路径
 * @param mode 打开模式，与标准fopen一致，常用：
 *             "rb"  只读打开（文件必须存在）
 *             "rb+" 读写打开（文件必须存在，适配擦除、覆盖写）
 *             "wb"  只写创建（文件不存在则创建，存在则清空）
 *             "ab"  追加写入
 * @return 成功返回文件指针，失败返回NULL
 */
void *fs_file_open(const char *path, const char *mode)
{
    if (path == NULL || mode == NULL) {
        return NULL;
    }

    FILE *file = fopen(path, mode);
    if (file == NULL) {
        ESP_LOGE(TAG, "failed to open file: %s, mode: %s", path, mode);
        return NULL;
    }

    return (void *)file;
}

/**
 * @brief 关闭文件并释放文件操作互斥锁
 * @param file 已打开的文件指针
 * @return 成功返回0，失败返回-1
 */
int fs_file_close(void *file)
{
    int ret = 0;

    if (file == NULL) {
        return -1;
    }

    // 强制刷盘，确保所有数据写入物理Flash，对齐LittleFS掉电安全特性
    if (fflush((FILE *)file) != 0) {
        ESP_LOGW(TAG, "fflush file failed");
        ret = -1;
    }

    if (fclose((FILE *)file) != 0) {
        ESP_LOGE(TAG, "fclose file failed");
        ret = -1;
    }

    return ret;
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

// 擦除配置：缓冲区大小与填充值，可根据系统内存/业务需求调整
#define ERASE_BUF_SIZE    4096
#define ERASE_FILL_VALUE  0x00

/**
 * @brief 擦除文件指定偏移处的区域。
 *
 * 将文件中从指定偏移开始的指定长度区域填充为擦除值（默认 0xFF，模拟 Flash 擦除效果），
 * 文件其他区域的数据保持不变。文件必须已存在，若不存在则返回错误。
 * 若擦除区域超出文件当前末尾，文件将被自动扩展，超出部分同样填充擦除值。
 *
 * @param file 文件指针
 * @param offset 擦除操作的起始偏移地址（字节）。
 * @param length 希望擦除的区域长度（字节）。
 * @return int 成功时返回实际擦除的字节数。如果发生错误则返回-1。
 */
int fs_file_custom_erase_at(FILE *file, uint32_t offset, int length)
{
    // 1. 参数检查
    if (file == NULL || length <= 0) {
        return -1;
    }

    int total_erased = -1;
    uint8_t *erase_buf = NULL;

    // 2. 获取文件操作信号量，确保线程安全，与 write_at 完全一致
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

#if 0   // 外部整体操作
    // 3. 以读写模式打开已有文件
    // 擦除是覆盖操作，文件必须存在；不自动创建空文件，避免无意义的文件生成
    file = fopen(path, "rb+");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for erasing: %s", path);
        xSemaphoreGive(xFileSemaphore);
        return -1;
    }
#endif

    // 4. 移动文件指针到指定的偏移位置
    if (fseek(file, offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to offset %lu", (unsigned long)offset);
        total_erased = -1;
    } else {
        erase_buf = (uint8_t *)iot_calloc(ERASE_BUF_SIZE);
        if ( erase_buf ) {
            // 预填充擦除缓冲区
            memset(erase_buf, ERASE_FILL_VALUE, ERASE_BUF_SIZE);
            
            int remaining = length;
            total_erased = 0;
            
            // 5. 循环分段写入擦除值
            while (remaining > 0) {
                int chunk = (remaining > ERASE_BUF_SIZE) ? ERASE_BUF_SIZE : remaining;
                size_t written = fwrite(erase_buf, 1, chunk, file);
            
                if (written < chunk) {
                    // 写入不完整统一判定为错误，与 write_at 逻辑保持一致
                    ESP_LOGE(TAG, "Partial erase. Expected %d, wrote %u", chunk, (unsigned)written);
                    total_erased = -1;
                    break;
                }
            
                total_erased += written;
                remaining -= written;
                vTaskDelay(1); // 让出CPU，避免长时间阻塞
            }
        } else{
            ESP_LOGE(TAG, "Failed to calloc ERASE_BUF_SIZE");
            total_erased = -1;
        }
    }

    // 6. 清理资源并返回
#if 0
    fclose(file);
#endif
    xSemaphoreGive(xFileSemaphore);
    iot_free(erase_buf);

    return total_erased;
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
int fs_file_custom_write_at(FILE *file, uint32_t offset, const uint8_t *data, int size)
{
    // 1. 参数检查
    if (file == NULL || data == NULL || size <= 0) {
        return -1;
    }

    int written_bytes = -1; // 默认返回值为-1，表示错误

    // 2. 获取文件操作信号量，确保线程安全
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

#if 0   // 外部整体操作
    // 3. 打开文件
    // 尝试以 "rb+" (读写更新) 模式打开已存在的文件。
    // 这种模式不会在打开时清空文件内容。
    file = fopen(path, "rb+");

    // 如果以 "rb+" 模式打开失败（通常意味着文件不存在），
    // 则尝试以 "wb" (写入) 模式创建新文件。
    if (file == NULL) {
        fs_mkdirs(path); // 新增：确保目录存在
        file = fopen(path, "wb");
    }

    // 如果两种方式都失败，则无法继续
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open or create file for writing: %s", path);
        xSemaphoreGive(xFileSemaphore);
        return -1;
    }
#endif

    // 4. 移动文件指针到指定的偏移位置
    if (fseek(file, offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to offset %lu", (unsigned long)offset);
        written_bytes = -1; // fseek 失败
    } else {
        // 5. 从当前位置写入指定大小的数据
        // fwrite 返回成功写入的元素个数。我们写入 size 个 1 字节的元素。
        written_bytes = fwrite(data, 1, size, file);
        if (written_bytes < size) {
            // 如果写入的字节数少于预期，这通常表示磁盘空间不足或发生其他I/O错误
            ESP_LOGE(TAG, "Error or partial write. Expected %d, wrote %d", size, written_bytes);
            // 即使是部分写入，也将其视为一种错误，返回-1，让调用者处理
            if (!ferror(file)) {
                // 如果不是标准库报告的错误（可能是磁盘满），也统一返回-1
                written_bytes = -1;
            }
        }
    }

    // 6. 清理并返回
#if 0    
    fclose(file);
#endif
    xSemaphoreGive(xFileSemaphore);

    return written_bytes;
}

/**
 * @brief 将数据追加写入到文件末尾。
 *
 * 文件需已以可写模式打开，调用后数据将被追加到文件当前末尾之后，
 * 不会覆盖文件原有内容，文件其他部分数据保持不变。
 *
 * @param file 已打开的文件指针。
 * @param data 指向要写入数据的缓冲区的指针。
 * @param size 希望写入的数据长度（字节）。
 * @return int 成功时返回实际写入的字节数。如果发生错误则返回-1。
 */
int fs_file_custom_append(FILE *file, const uint8_t *data, int size)
{
    // 1. 参数检查
    if (file == NULL || data == NULL || size <= 0) {
        return -1;
    }

    int written_bytes = -1; // 默认返回值为-1，表示错误

    // 2. 获取文件操作信号量，确保线程安全
    xSemaphoreTake(xFileSemaphore, pdMS_TO_TICKS(portMAX_DELAY));

    // 3. 移动文件指针到文件末尾
    if (fseek(file, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "Failed to seek to file end");
        written_bytes = -1;
    } else {
        // 4. 从文件末尾位置写入指定大小的数据
        written_bytes = fwrite(data, 1, size, file);
        if (written_bytes < size) {
            // 写入字节数少于预期，判定为写入错误
            ESP_LOGE(TAG, "Error or partial append. Expected %d, wrote %d", size, written_bytes);
            if (!ferror(file)) {
                // 无标准库显式错误也统一返回-1，与原写入函数逻辑保持一致
                written_bytes = -1;
            }
        }
    }

    // 5. 释放信号量并返回
    xSemaphoreGive(xFileSemaphore);

    return written_bytes;
}

