/**
  ******************************************************************************
  * @file      filesystem.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/11/14
  * @brief     文件系统相关定义头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/11/14 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

#include "stdint.h"
#include <stdbool.h>
#include "esp_err.h"

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE ((uint16_t)-1)

#define FS_BASE_PATH        "/littlefs"         /**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_PARTITION_NAME   "disk"              /**< 分区表中文件系统所挂载的分区名称 */

#define PARAMETER_ROOT_PATH FS_BASE_PATH    // 文件系统根目录

#define PARAMETER_FOLDER_LEVEL1 "S/"
#define PARAMETER_FILE_MARK_IOT         PARAMETER_FOLDER_LEVEL1"iot" // IOT本身参数存储
#define PARAMETER_FILE_MARK_INV         PARAMETER_FOLDER_LEVEL1"inv" // INV下级参数存储
#define PARAMETER_FILE_MARK_RELAY       PARAMETER_FOLDER_LEVEL1"smart"  // 智能控制参数
#define PARAMETER_FILE_MARK_DISASTER    PARAMETER_FOLDER_LEVEL1"storm"  // 风暴备电数据
#define PARAMETER_FILE_MARK_CA          PARAMETER_FOLDER_LEVEL1"ca"//网络安全存储:CA证书
#define PARAMETER_FILE_MARK_CERT        PARAMETER_FOLDER_LEVEL1"cert"//网络安全存储:IOT证书
#define PARAMETER_FILE_MARK_KEY         PARAMETER_FOLDER_LEVEL1"key"//网络安全存储:IOT私钥


#define PARAMETER_FILE_PATH_IOT(node, sub_node) \
    PARAMETER_ROOT_PATH"/"PARAMETER_FILE_MARK_IOT""#node"_"#sub_node

#define PARAMETER_FILE_PATH_INV(node, sub_node) \
    PARAMETER_ROOT_PATH"/"PARAMETER_FILE_MARK_INV""#node"_"#sub_node

#define PARAMETER_FILE_PATH_RELAY(node, sub_node) \
    PARAMETER_ROOT_PATH"/"PARAMETER_FILE_MARK_RELAY""#node"_"#sub_node

#define PARAMETER_FILE_PATH_DISASTER(node, sub_node) \
    PARAMETER_ROOT_PATH"/"PARAMETER_FILE_MARK_DISASTER""#node"_"#sub_node

#define PARAMETER_FILE_PATH_CA_CERT(node, sub_node) \
    PARAMETER_ROOT_PATH"/"PARAMETER_FILE_MARK_CA""#node"_"#sub_node

#define PARAMETER_FILE_PATH_IOT_CERT(node, sub_node) \
    PARAMETER_ROOT_PATH"/"PARAMETER_FILE_MARK_CERT""#node"_"#sub_node

#define PARAMETER_FILE_PATH_IOT_KEY(node, sub_node) \
    PARAMETER_ROOT_PATH"/"PARAMETER_FILE_MARK_KEY""#node"_"#sub_node

enum
{
    FS_FLAG_CREATE = 0x01,
    FS_FLAG_APPEND = 0x02,
    FS_FLAG_READ = 0x04,
    FS_FLAG_WRITE = 0x08,
    FS_FLAG_TRUNC = 0x10,
    FS_FLAG_EXCL = 0x20,
    FS_FLAG_DIR = 0x40,
    FS_FLAG_RDONLY = 0x80,
};

#ifdef __cplusplus
extern "C"
{
#endif

void fs_mount(void);
int fs_file_read(const char *path, uint8_t *data, int size);
int fs_file_write(const char *path, uint8_t *data, uint32_t offset, int size);
int fs_dir_create(const char *path);
int fs_file_size(const char *path);
int fs_file_check(void);
int fs_record_write(const char *path, void *record, uint32_t start_offset, uint16_t record_index, uint16_t record_len, uint16_t record_cnt);
int fs_file_read_at(const char *path, uint32_t offset, uint8_t *data, int size);

int fs_file_write_at(const char *path, uint32_t offset, const uint8_t *data, int size);
bool fs_file_exists(const char *path);
int fs_file_delete(const char *path);
int fs_file_rename(const char *old_path, const char *new_path);
esp_err_t fs_file_crc32(const char *path, uint8_t *crc_buf, size_t buf_size, uint32_t *out_crc);

void *fs_file_open(const char *path, const char *mode);
int fs_file_close(void *file);
int fs_file_custom_erase_at(FILE *file, uint32_t offset, int length);
int fs_file_custom_write_at(FILE *file, uint32_t offset, const uint8_t *data, int size);
int fs_file_custom_append(FILE *file, const uint8_t *data, int size);

// 递归删除目录及其所有内容
int fs_delete_dir_recursive(const char *dir_path);

#ifdef __cplusplus
}
#endif
