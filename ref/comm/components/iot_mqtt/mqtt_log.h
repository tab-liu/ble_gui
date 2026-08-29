/**
  ******************************************************************************
  * @file      mqtt_log.h
  * @version   1.0
  * @author    lixingyu
  * @date      2026/8/5
  * @brief     MQTT登录日志模块
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/8/5   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>

#include <time.h>
#include "mqtt_client.h"

/* ======================== 本地模块文件引用（可选） ============================ */

#include "filesystem.h"

/* ================================ 头文件宏定义 ================================ */

#if 0   // 引用其他文件时于该文件内实现，这里不定义

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE                ((uint16_t)-1)

/**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_BASE_PATH                    "/littlefs" 

#endif

/*文件二级目录*/
#define MQTT_LOG_FOLDER_LEVEL1   "L/"

/*文件系统内文件名称 */
#define MQTT_LOG_FILE_NAME       FS_BASE_PATH"/"MQTT_LOG_FOLDER_LEVEL1"iot01_c06"

/*文件内容版本*/
#define MQTT_LOG_FILE_VER        0x01

/*文件最大长度*/
#define MQTT_LOG_FILE_MAX_LEN    0x8000 // 32K（视芯片能力而定）

/*单条长度长度*/
#define MQTT_LOG_LEN		     64

/*记录文件头长度*/
#define MQTT_LOG_FILE_HEADER_LEN 64

/*记录起始地址*/
#define MQTT_LOG_START_ADDR		 MQTT_LOG_FILE_HEADER_LEN

/*记录最多条数*/
#define MQTT_LOG_MAX_NUM         ((MQTT_LOG_FILE_MAX_LEN - MQTT_LOG_START_ADDR) / MQTT_LOG_LEN)

/*临时缓存长度*/
#define MQTT_LOG_BUF_LEN         0x1000 // 4K

/*临时缓存最多条数*/
#define MQTT_LOG_PR0CESS_MAX_NUM (MQTT_LOG_BUF_LEN / MQTT_LOG_LEN)

/*文件存储写入间隔（未溢出时）*/
#define MQTT_LOG_SAVE_TIMEOUT   (10 * 60 * 1000) // 10min

/* ============================== 头文件结构体定义 ================================ */


#pragma pack(1)

/**
 * @brief 记录文件的头部信息结构体
 * @note 使用联合体，方便以字节数组或结构化形式访问。
 */
typedef union {
    /**
     * @brief 以字节数组形式访问全部数据。
     */
    uint8_t all[MQTT_LOG_FILE_HEADER_LEN];

    /**
     * @brief 以结构化形式访问数据成员。
     * 内部结构体也需要 packed 属性以保证与字节数组的内存布局完全一致。
     */
    struct __attribute__((packed)) {
        /**
         * @brief 记录版本。
         * 这个值在文件创建时确定。
         */
        uint16_t record_ver;
        
        /**
         * @brief 文件内可存储的最大记录条数。
         * 这个值在文件创建时确定，通常是 (文件总大小 - 头部大小) / 单条记录大小。
         */
        uint16_t max_records;
        
        /**
         * @brief 文件中当前已存储的有效记录条数。
         * 对于循环写入的日志，这个值可能等于 max_records。
         */
        uint16_t current_records;
        
        /**
         * @brief 下一条记录应写入的位置索引（偏移量）。
         * 当文件写满后，此索引会从0开始循环，覆盖最旧的记录。
         * 它的值范围是 0 到 (max_records - 1)。
         */
        uint16_t write_index;
    };
}mqtt_log_file_header_t;

_Static_assert(sizeof(mqtt_log_file_header_t) == MQTT_LOG_FILE_HEADER_LEN, "mqtt_log_file_header_t size mismatch");

// 连接失败大类，快速区分故障域
typedef enum {
    MQTT_FAIL_UNKNOWN = 0,
    MQTT_FAIL_DNS,               // DNS 解析失败
    MQTT_FAIL_TCP,               // TCP 连接失败
    MQTT_FAIL_TCP_TIME,          // TCP 获取时间失败
    MQTT_FAIL_CERT,              // 证书更新下载失败
    MQTT_FAIL_LOGIN,             // MQTT登录过程失败
    MQTT_FAIL_MQTT_UNACCEPTABLE, // MQTT CONNACK 返回非 0 原因码
    MQTT_FAIL_SUBSCRIBE,         // MQTT订阅失败
    MQTT_FAIL_NETWORK_CHANGE,    // 网络切换导致中断
    MQTT_DISCONNECT,             // 协议层关闭或客户端主动断开
    MQTT_DEVICE_STOP,            // 设备主动终止连接
    MQTT_POLL_TIMEOUT,           // 应用层等待超时（poll 超时）
    MQTT_CONNECT_EOF_OR_TIMEOUT, // 等待 CONNACK 时对端关闭或超时
} MqttFailReason;

/**
 * @brief 单条记录结构体
 * @note 使用联合体，方便以字节数组或结构化形式访问。
 */
typedef union {
    /**
    * @brief 以字节数组形式访问全部数据，便于原始数据拷贝和存储。
    */
    uint8_t all[MQTT_LOG_LEN];
    
    /**
    * @brief 以结构化形式访问数据成员。
    * @note 使用 __attribute__((packed)) 确保结构体成员紧凑排列，
    *       与 'all' 字节数组的内存布局完全一致。
    */
    struct __attribute__((packed)) {
        /* ---- 基础标识 ---- */
        uint32_t        log_id;                 // 日志流水号（上电后从0开始）

        /* ---- 时间信息 ---- */
        uint64_t        fail_timestamp;         // 失败发生的 UTC 时间戳（精准定位）
        uint64_t        svc_timestamp;          // 连接时获取到的服务器时间戳（密码计算使用）
        uint32_t        uptime_seconds;         // 本次启动后到失败时的运行时长（分析是否在启动初期频发）

        /* ---- 连接目标 ---- */
        uint8_t         use_mqtts;              // 是否启用 mqtt加密（0/1）
        uint64_t        password;               // 本次连接计算密码

        /* ---- 失败原因 ---- */
        uint8_t         reason;                 // 分类后的失败原因（上层归类后填写）
        int32_t         sys_errno;              // 系统级错误码（如 errno，用于底层分析）
        uint8_t         error_type;             /*!< error type referring to the source of the error */
        uint8_t         connect_return_code;    /*!< connection refused error code reported from *MQTT* broker on connection */

        /* ---- 网络环境 ---- */
        uint8_t         network_type;           // 0=Ethernet，1=Wi-Fi, 2=PPP
        int8_t          rssi;                   // 信号强度 dBm
        
        /* ---- 重试上下文 ---- */
        uint16_t        retry_delay_ms;         // 本次重连等待时长

        /* ---- TLS报错原因 ---- */
        int32_t esp_tls_last_esp_err;           /*!< last esp_err code reported from esp-tls component */
        int32_t esp_tls_stack_err;              /*!< tls specific error code reported from underlying tls stack */
        int32_t esp_tls_cert_verify_flags;      /*!< tls flags reported from underlying tls stack during certificate verification */

        /* ---- 扩展预留 ---- */
        uint8_t         reserved[8];            // 保留字段，便于后续追加信息不破坏结构
    };
} mqtt_conn_fail_log_t;

_Static_assert(sizeof(mqtt_conn_fail_log_t) == MQTT_LOG_LEN, "mqtt_conn_fail_log_t size mismatch");

/**
 * @brief 用于封装指向记录的指针的结构体。
 *
 * 这个结构体主要用于在不同模块或任务之间传递记录的引用，
 * 例如在 FreeRTOS 队列中传递指针，以避免复制整个记录数据，
 * 从而提高效率并减少栈空间的使用。
 */
typedef struct {
    /**
     * @brief 指向一个 mqtt_conn_fail_log_t 实例的指针。
     */
    mqtt_conn_fail_log_t *p_record;
} mqtt_conn_fail_log_pointer_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief      MQTT登录日志模块初始化
  * @note       建议在系统上电后、相关任务开始前执行。
  * @param[in]  void
  * @param[out] None
  * @return
  *  - 0: 成功
  *  - -1: 失败，通常是由于资源（信号量、队列）创建失败或文件系统错误。
  */
int Mqtt_Log_Module_Init(void);

/**
 * @brief      根据传入的参数生成一条MQTT事件记录。
 * @details    此函数负责动态分配内存、填充记录内容，并将其指针发送到处理队列。
 *             函数内部处理了线程安全和内存管理。
 *
 * @param[in]  params  一个指向 `mqtt_conn_fail_log_t` 结构体的常量指针，
 *                     该结构体封装了生成日志所需的所有参数。
 *
 * @return
 *         - 0: 成功，记录已成功创建并尝试发送到队列。
 *         - -1: 失败，可能由于参数错误、内存分配失败或队列发送失败。
 */
int Mqtt_Log_Generate(const mqtt_conn_fail_log_t *params);

/**
 * @brief      从队列中批量消费日志，并以循环方式写入文件。
 * @details    此函数是设备接入日志模块的"消费者"核心。它被设计为一个周期性执行的任务。
 *             其主要工作流程如下：
 *             1. **批量拉取**: 非阻塞地从 `xQueue_mqtt_log` 队列中拉取最多
 *                `MQTT_LOG_PR0CESS_MAX_NUM` 条记录，暂存到本地数组中。
 *                这样做是为了将多次零散的文件写入合并为一次或两次批量写入，以提升I/O性能。
 *             2. **内存管理**: 将队列中指针指向的数据复制到本地后，立即释放生产者分配的内存，
 *                防止内存泄漏。
 *             3. **分块计算**: 根据文件当前状态（是否已满），计算出本次批量写入需要被拆分的
 *                两个部分（`cnt1` 和 `cnt2`）。`cnt1` 是填满当前空间的部分，`cnt2` 是
 *                回环覆盖到文件开头的部分。
 *             4. **循环写入**: 调用辅助函数 `Mqtt_Log_Write`，最多执行两次写入操作，
 *                以完成 `cnt1` 和 `cnt2` 数据的持久化。
 *             5. **文件头更新**: 根据 `cnt1` 和 `cnt2` 的计算结果，精确地更新文件头信息
 *                （`current_records` 和 `write_index`），以保证状态的正确性。
 *             6. **状态持久化**: 将更新后的文件头写回磁盘。
 */
void Mqtt_Log_Save_Task(void);

/**
 * @brief      读取增量MQTT连接日志到动态分配的缓冲区。
 * @details    此函数是实现日志增量同步的核心接口。它会：
 *             1. 调用辅助函数 `Mqtt_Log_Count_New_Records` 计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *
 * @param[in]  since_timestamp  一个UNIX时间戳，用于界定"新"记录。
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
int Mqtt_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size);

/**
 * @brief      判断传入的文件标识符是否为当前的MQTT连接日志文件。
 * @details    此函数用于验证外部请求的文件是否为本模块管理的日志文件。
 *             它会根据传入的参数构建一个完整的路径，并与模块内部生成的
 *             标准路径进行比较。
 *
 * @param[in]  fname  一个指向数据通道文件（自定义格式）参数的指针。
 *                    - fname[0]: 文件夹层级。
 *                    - &fname[1]: 文件名字符串。
 *
 * @return     int
 *             - 0: 是日志文件。
 *             - -1: 不是日志文件或参数错误。
 */
int Is_Mqtt_Log_File(const char *fname);

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符（MQTT_LOG_FOLDER_LEVEL1）
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Mqtt_Log_File_Id(char *out_buf, size_t buf_len);

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
int Is_Mqtt_Log_Count_New(time_t since_timestamp);

#ifdef __cplusplus
}
#endif
