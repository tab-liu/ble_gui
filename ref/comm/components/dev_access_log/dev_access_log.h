/**
  ******************************************************************************
  * @file      dev_access_log.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/9/26
  * @brief     设备接入记录日志模块头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/26  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#pragma once

/* ================================ 库文件引用 ================================ */

#include <stdint.h>
#include <stddef.h>

/* ======================== 本地模块文件引用（可选） ============================ */

#include "filesystem.h"
#include "dev_data_record.h"

/* ================================ 头文件宏定义 ================================ */

// 设备接入记录最多条数，根据芯片实际能力决定，默认100条
#define DEV_ACCESS_LOG_MAX_NUM          100

// 设备接入记录单次最多处理条数，默认16条
#define DEV_ACCESS_PR0CESS_MAX_NUM      16

// 设备接入记录文件头起始地址
#define DEV_ACCESS_FILE_HEADER_ADDR		0

// 设备接入记录文件头长度
#define DEV_ACCESS_FILE_HEADER_LEN		20

// 设备接入记录起始地址
#define DEV_ACCESS_LOG_START_ADDR		20

// 设备接入记录单条长度
#define DEV_ACCESS_LOG_LEN		        40

// 设备接入模块线程间隔
#define DEV_ACCESS_TASK_PERIOD_MS       3000

#if 0   // 引用其他文件时于该文件内实现，这里不定义

// 定义一个特殊值来表示追加模式，避免使用魔术数字
#define FILE_APPEND_MODE                ((uint16_t)-1)

/**< 挂载点路径，文件系统将挂载到此路径下 */
#define FS_BASE_PATH                    "/littlefs" 

// 历史记录存储目录
#define LOG_RECORD_FOLDER_LEVEL1        "L/"

#endif

/**
  ******************************************************************************
    IOT智能记录
    buf: 输出访问路径
    node: IOT代号种类，使用2位数字表示
    protocol_code: 事件记录协议代号, 使用2位数字表示

    " 协议代号":
    1-模块更换记录
    2-寄存器修改记录
    3-能量记录
    4-IOT事件记录
    5-INV事件记录
    6-PACK事件记录
******************************************************************************
*/
#define DEV_ACCESS_FILE_FOLDER_LEVEL    LOG_RECORD_FOLDER_LEVEL1
#define DEV_ACCESS_FILE_PATH_IOT(buf, node)	\
	sprintf(buf, "%s/%s%s%02d_c01", FS_BASE_PATH, DEV_ACCESS_FILE_FOLDER_LEVEL, "iot", node)

/* ============================== 头文件结构体定义 ================================ */


#pragma pack(1)

/**
 * @brief 记录文件的头部信息结构体
 * @note 头部大小固定为 20 字节。
 *       使用联合体，方便以字节数组或结构化形式访问。
 */
typedef union {
    /**
     * @brief 以字节数组形式访问全部20字节数据。
     */
    uint8_t all[DEV_ACCESS_FILE_HEADER_LEN];

    /**
     * @brief 以结构化形式访问数据成员。
     * 内部结构体也需要 packed 属性以保证与字节数组的内存布局完全一致。
     */
    struct __attribute__((packed)) {
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
} device_event_file_header_t;

_Static_assert(sizeof(device_event_file_header_t) == DEV_ACCESS_FILE_HEADER_LEN, "device_event_file_header_t size mismatch");

/**
 * @brief 设备事件记录类型枚举
 */
typedef enum {
    DEVICE_EVENT_REC_TYPE_MAIN_MACHINE = 0,         /**< 0: 通用整机设备（未知来源） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_UART = 1,        /**< 1: 整机设备（串口总线） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_CAN = 2,         /**< 2: 整机设备（CAN总线） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_BLE_ADV = 3,     /**< 3: 整机设备（蓝牙数据广播） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_BLE_MESH = 4,    /**< 4: 整机设备（蓝牙Mesh） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_WIFI_MESH = 5,   /**< 5: 整机设备（WIFI Mesh） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_WIFI_UDP = 6,    /**< 6: 整机设备（WIFI UDP） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_SUB1GHZ = 7,     /**< 7: 整机设备（Sub1G） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_CAN2 = 8,        /**< 8: 整机设备（CAN总线-2） */
    DEVICE_EVENT_REC_TYPE_DEV_FROM_RS485 = 9,       /**< 9: 整机设备（RS485总线） */
    DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN   = 0xFC,  /**< 0xFC: MCU模块（CAN总线架构系统） */
    DEVICE_EVENT_REC_TYPE_MCU_MODULE_UART  = 0xFD,  /**< 0xFD: MCU模块（串口总线架构系统） */
    DEVICE_EVENT_REC_TYPE_MCU_MODULE_RS485 = 0xFE,  /**< 0xFE: MCU模块（RS485总线架构系统） */
    DEVICE_EVENT_REC_TYPE_MCU_MODULE = 0xFF,        /**< 0xFF: 通用MCU模块（未知来源） */
} device_event_record_type_t;

/**
 * @brief 设备事件操作属性枚举
 */
typedef enum {
    DEVICE_EVENT_OP_AUTO_ADD      = 1, /**< 1: 自动识别增加 */
    DEVICE_EVENT_OP_TIMEOUT_OFFLINE = 2, /**< 2: 设备超时离线 */
    DEVICE_EVENT_OP_FORCE_BIND    = 3, /**< 3: 外部强制绑定 */
    DEVICE_EVENT_OP_FORCE_UNBIND  = 4, /**< 4: 外部强制解绑 */
} device_event_operation_t;

/**
 * @brief 设备事件信息类型枚举
 */
typedef enum {
    DEVICE_EVENT_INFO_TYPE_SN      = 0, /**< 0: 标准类型加SN (12+8) */
    DEVICE_EVENT_INFO_TYPE_BLE_NAME = 1, /**< 1: 蓝牙广播名 (最长12+13字节) */
} device_event_info_type_t;


/**
 * @brief 设备事件的单条记录结构体
 * @note 总长度为 40 字节。使用联合体，方便以字节数组或结构化形式访问。
 *       根据表格定义，有效数据为34字节，剩余6字节为保留字段。
 */
typedef union {
    /**
     * @brief 以字节数组形式访问全部40字节数据，便于原始数据拷贝和存储。
     */
    uint8_t all[DEV_ACCESS_LOG_LEN];

    /**
     * @brief 以结构化形式访问数据成员。
     * @note 使用 __attribute__((packed)) 确保结构体成员紧凑排列，
     *       与 'all' 字节数组的内存布局完全一致。
     */
    struct __attribute__((packed)) {
        uint32_t timestamp;             /**< [ 4字节] 事件发生时的UNIX时间戳 */
        uint8_t  record_type;           /**< [ 1字节] 记录类型, 参考 device_event_record_type_t */
        uint8_t  parent_address;        /**< [ 1字节] 上级地址, 0xFF表示无效 */
        uint8_t  local_address;         /**< [ 1字节] 本机地址, 0xFF表示无效 */
        uint8_t  operation_attribute;   /**< [ 1字节] 操作属性, 参考 device_event_operation_t */
        uint8_t  info_type;             /**< [ 1字节] 信息类型, 参考 device_event_info_type_t */

        /**
         * @brief [25字节] 设备标识符的联合体。
         * @details
         *   - 当 info_type 为 DEVICE_EVENT_INFO_TYPE_BLE_NAME 时，应使用 'device_identifier' 访问。
         *   - 当 info_type 为 DEVICE_EVENT_INFO_TYPE_SN 时，应使用 'dev_type' 和 'dev_sn' 访问。
         */
        union 
        {
            /**
             * @brief 原始的25字节标识符数组，用于存储蓝牙广播名等非结构化数据。
             */
            uint8_t  device_identifier[25];

            /**
             * @brief 用于访问标准SN类型记录的结构化视图。
             */
            struct {
                char     dev_type[12];    /**< 设备类型字符串 (应为null-terminated) */
                uint64_t dev_sn;        /**< 64位设备SN号 */
            };
        };  

        uint8_t  reserved[6];           /**< [ 6字节] 保留字段, 用于凑齐40字节总长 */
    };
} device_event_record_t;

_Static_assert(sizeof(device_event_record_t) == DEV_ACCESS_LOG_LEN, "device_event_record_t size mismatch");

/**
 * @brief 用于封装指向参数记录的指针的结构体。
 *
 * 这个结构体主要用于在不同模块或任务之间传递记录的引用，
 * 例如在 FreeRTOS 队列中传递指针，以避免复制整个记录数据，
 * 从而提高效率并减少栈空间的使用。
 */
typedef struct {
    /**
     * @brief 指向一个 device_event_record_t 实例的指针。
     */
    device_event_record_t *p_record;
} device_event_record_pointer_t;

/**
 * @brief 模块接入日志生成所需参数的集合。
 * @details 用于替代 Dev_Access_Log_Generate 函数的长参数列表。
 */
typedef struct {
    uint8_t  record_type;           /**< [ 1字节] 记录类型, 参考 device_event_record_type_t */
    uint8_t  parent_address;        /**< [ 1字节] 上级地址, 0xFF表示无效 */
    uint8_t  local_address;         /**< [ 1字节] 本机地址, 0xFF表示无效 */
    uint8_t  operation_attribute;   /**< [ 1字节] 操作属性, 参考 device_event_operation_t */
    uint8_t  info_type;             /**< [ 1字节] 信息类型, 参考 device_event_info_type_t */
    /**
     * @brief 根据 info_type 选择性填充的设备标识符联合体。
     */
    union {
        /**
         * @brief 用于蓝牙广播名等非结构化数据。
         * @note 当 info_type 为 DEVICE_EVENT_INFO_TYPE_BLE_NAME 时使用。
         */
        const uint8_t *device_identifier;

        /**
         * @brief 用于标准SN类型记录。
         * @note 当 info_type 为 DEVICE_EVENT_INFO_TYPE_SN 时使用。
         */
        struct {
            const char     *dev_type;
            const uint64_t *dev_sn;
        } sn_info;
    } identifier_info;
} dev_access_params_t;


#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif

 /**
 * @brief 设备接入日志模块初始化
 * @note 建议在系统上电后初始化期间执行。
 */
int Dev_Access_Module_Init(void);

/**
 * @brief      生成一条设备接入日志并将其放入处理队列。
 * @details    此函数是设备接入日志模块的“生产者”核心接口。它负责将一个描述设备
 *             状态变化的事件，转换成一条标准的日志记录，并发送到后台处理队列中。
 *             其主要工作流程如下：
 *             1. **动态分配内存**: 为新的日志记录 `device_event_record_t` 分配内存。
 *                这样做是为了将数据从调用者的栈（或易变的数据区）安全地传递给消费者任务。
 *             2. **数据拷贝**: 将传入的 `params` 结构体中的信息（包括通过指针引用的SN等）
 *                完整地拷贝到新分配的内存中。
 *             3. **封装指针**: 将新分配记录的内存地址封装到一个指针结构体中。
 *             4. **发送到队列**: 以非阻塞方式将该指针发送到 `xQueue_dev_access_log` 队列，
 *                等待消费者任务 `Dev_Access_Log_Save_Task` 进行处理。
 *
 * @param[in]  params 一个指向 `dev_access_params_t` 结构体的指针，包含了生成
 *                    一条日志所需的所有信息。调用者需要填充此结构体。
 *
 * @return     int
 *             - 0: 成功生成记录并发送到队列。
 *             - -1: 失败（如内存分配失败、队列已满等）。
 *
 * @note       - 此函数是线程安全的，内部不含互斥锁，但向FreeRTOS队列发送是安全的。
 *             - 调用者提供的 `params` 结构体及其内部指针所指向的数据，在此函数返回后
 *               即可释放或复用，因为函数内部已完成数据快照的拷贝。
 *
 * @code
 * // 使用示例：在设备状态检测函数(如InvOfflineCheck)中记录上线和下线事件
 *
 * // 1. 当检测到新设备上线时
 * if (is_new_device_online)
 * {
 *     // 准备日志参数
 *     dev_access_params_t record_params = {0};
 *     record_params.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
 *     record_params.parent_address = 0xFF; // 假设无上级
 *     record_params.local_address = calculated_device_address;
 *     record_params.operation_attribute = DEVICE_EVENT_OP_AUTO_ADD; // 记录为 "自动识别增加"
 *     record_params.info_type = DEVICE_EVENT_INFO_TYPE_SN;
 *     
 *     // 注意：这里传递的是指向实际数据的指针
 *     record_params.identifier_info.sn_info.dev_type = &device_info->inv_about.dev_type;
 *     record_params.identifier_info.sn_info.dev_sn = &device_info->inv_about.dev_sn;
 *
 *     // 调用API生成日志
 *     Dev_Access_Log_Generate(&record_params);
 * }
 *
 * // 2. 当检测到设备超时下线时
 * if (is_device_timeout)
 * {
 *     // 准备日志参数
 *     dev_access_params_t record_params = {0};
 *     record_params.record_type = DEVICE_EVENT_REC_TYPE_MCU_MODULE_CAN;
 *     record_params.parent_address = 0xFF;
 *     record_params.local_address = calculated_device_address;
 *     record_params.operation_attribute = DEVICE_EVENT_OP_TIMEOUT_OFFLINE; // 记录为 "设备超时离线"
 *     record_params.info_type = DEVICE_EVENT_INFO_TYPE_SN;
 *     
 *     record_params.identifier_info.sn_info.dev_type = &device_info->inv_about.dev_type;
 *     record_params.identifier_info.sn_info.dev_sn = &device_info->inv_about.dev_sn;
 *
 *     // 调用API生成日志
 *     Dev_Access_Log_Generate(&record_params);
 * }
 * @endcode
 */
int Dev_Access_Log_Generate(const dev_access_params_t *params);

/**
 * @brief      从队列中批量消费设备接入日志，并以循环方式写入文件。
 * @details    此函数是设备接入日志模块的“消费者”核心。它被设计为一个周期性执行的任务。
 *             其主要工作流程如下：
 *             1. **批量拉取**: 非阻塞地从 `xQueue_dev_access_log` 队列中拉取最多
 *                `DEV_ACCESS_PR0CESS_MAX_NUM` 条记录，暂存到本地数组中。
 *                这样做是为了将多次零散的文件写入合并为一次或两次批量写入，以提升I/O性能。
 *             2. **内存管理**: 将队列中指针指向的数据复制到本地后，立即释放生产者分配的内存，
 *                防止内存泄漏。
 *             3. **分块计算**: 根据文件当前状态（是否已满），计算出本次批量写入需要被拆分的
 *                两个部分（`cnt1` 和 `cnt2`）。`cnt1` 是填满当前空间的部分，`cnt2` 是
 *                回环覆盖到文件开头的部分。
 *             4. **循环写入**: 调用辅助函数 `Dev_Access_Log_Write`，最多执行两次写入操作，
 *                以完成 `cnt1` 和 `cnt2` 数据的持久化。
 *             5. **文件头更新**: 根据 `cnt1` 和 `cnt2` 的计算结果，精确地更新文件头信息
 *                （`current_records` 和 `write_index`），以保证状态的正确性。
 *             6. **状态持久化**: 将更新后的文件头写回磁盘。
 *
 * @note       - 此函数应作为一个周期性任务（Task）来调用，例如每隔3-5秒执行一次。
 *             - 函数内部包含复杂的文件I/O操作，可能会有一定耗时，不应在对实时性
 *               要求极高的上下文（如中断服务程序）中调用。
 *             - 函数的写入逻辑依赖于一个全局缓存的文件头 `Dev_Access_File_Header`，
 *               该文件头在 `Dev_Access_Module_Init` 中被初始化。
 */
void Dev_Access_Log_Save_Task(void);


/**
 * @brief      读取增量设备接入日志到动态分配的缓冲区。
 * @details    此函数是实现日志增量同步的核心接口。它会：
 *             1. 调用辅助函数 `Dev_Access_Log_Count_New_Records` 计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *             5. 注意，返回的缓冲区大小取决于DEV_ACCESS_LOG_MAX_NUM，
 *                1000条时可达40k，自行评估使用。
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
 *
 * @code
 * // 使用示例:
 * // 一个通用的数据获取函数，根据文件名分发到不同的处理逻辑。
 * uint8_t* get_data_from_storage(char *fname, uint32_t offset, uint8_t *data, uint32_t *size)
 * {
 *     // 使用 Is_Dev_Access_Log_File 来判断文件类型
 *     if (0 == Is_Dev_Access_Log_File(fname))
 *     {
 *         // 文件是特殊的“设备接入日志文件”，调用其专用的增量读取函数。
 *         // 该函数会内部自分配内存。
 *         if (Dev_Access_Log_Read_Incremental(0, &data, size) != 0)
 *         {
 *             // 读取失败
 *             return NULL;
 *         }
 *     }
 *     else
 *     {
 *         // 文件是其他类型的历史数据文件，走通用读取逻辑。
 *     }
 *
 *     return data;
 * }
 * @endcode
 */
int Dev_Access_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size);

/**
 * @brief      判断传入的文件标识符是否为当前的设备接入日志文件。
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
int Is_Dev_Access_Log_File(const char *fname);

/**
 * @brief 生成当前日志文件对应的文件标识符。
 *
 * - out_buf[0] = 文件夹层级字符（DEV_ACCESS_FILE_FOLDER_LEVEL[0]）
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Dev_Access_Log_File_Id(char *out_buf, size_t buf_len);

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
int Is_Dev_Access_Log_Count_New(time_t since_timestamp);

#ifdef __cplusplus
}
#endif
