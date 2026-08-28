#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//#define FLASH_BASE_PATH "/spiflash"

#define FLASH_BASE_PATH         		"/littlefs"
#define FLASH_PARTITION_NAME    		"disk"//"custom_data"//"littlefs"

enum{
    CREATE_FILE = 0, // 创建空文件
    OPEN_IOT_FILE, // 打开文件
    READ_WRITE_FILE, // 读或写入文件
    CLOSE_FILE, // 关闭文件
    DELETE_FILE, // 删除文件
};


// Register NVS functions
void mount_littlefs(void);

void file_list(const char *path);
int read_file(const char *path, uint8_t *data, int len);
int write_file(const char *path, uint8_t *data, int len);
int get_file_size(const char *path);

#ifdef __cplusplus
}
#endif
