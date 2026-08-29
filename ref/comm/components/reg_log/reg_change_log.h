/**
  ******************************************************************************
  * @file      reg_change_log.h
  * @version   1.0
  * @author    lixingyu
  * @date      2025/9/21
  * @brief     寄存器修改日志模块头文件
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/9/21  <td>1.0     <td>lixingyu   <td>Create the initial version
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

// 本地存储寄存器修改记录最多条数，根据芯片实际能力决定，默认500条
#define REG_CHANGE_LOG_MAX_NUM          500

// 本地存储寄存器修改记录单次最多处理条数，默认32条
#define REG_CHANGE_PR0CESS_MAX_NUM      32

// 寄存器修改日志文件头起始地址
#define REG_CHANGE_FILE_HEADER_ADDR		0

// 寄存器修改日志文件头长度
#define REG_CHANGE_FILE_HEADER_LEN		20

// 寄存器修改日志起始地址
#define REG_CHANGE_LOG_START_ADDR		20

// 寄存器修改日志单条长度
#define REG_CHANGE_LOG_LEN		        20

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
#define REG_CHANGE_FILE_FOLDER_LEVEL    LOG_RECORD_FOLDER_LEVEL1
#define REG_CHANGE_FILE_PATH_IOT(buf, node)	\
	sprintf(buf, "%s/%s%s%02d_c02", FS_BASE_PATH, REG_CHANGE_FILE_FOLDER_LEVEL, "iot", node)

/* ============================== 头文件结构体定义 ================================ */


#pragma pack(1)

/**
 * @brief 通信协议类型枚举
 */
typedef enum {
    PROTOCOL_MODBUS_ALPHA = 1,  /**< Modbus Alpha协议 */
    PROTOCOL_MODBUS_BETA  = 2,  /**< Modbus Beta协议 */
    PROTOCOL_MODBUS_OPEN  = 3,  /**< Modbus Open协议 */
    PROTOCOL_CAN_BETA     = 4,  /**< CAN Beta协议 */
} protocol_type_t;

/**
 * @brief 数据来源枚举
 */
typedef enum {
    DATA_SOURCE_UNKNOWN = 0,        /**< 未知 */
    DATA_SOURCE_APP_BLE_ADV = 1,    /**< 手机APP通过BLE广播 */
    DATA_SOURCE_APP_BLE_SERVER = 2, /**< 手机APP作为BLE客户端连接 */
    DATA_SOURCE_WIFI_UDP = 3,       /**< WiFi UDP */
    DATA_SOURCE_WIFI_MQTT = 4,      /**< WiFi MQTT */
    DATA_SOURCE_CAN = 5,            /**< CAN总线 */
    DATA_SOURCE_UART = 6,           /**< 串口 */
    DATA_SOURCE_MODBUS_TCP = 7,     /**< Modbus TCP */
    DATA_SOURCE_IOT_INTERNAL = 8,   /**< IOT设备自身 */
    DATA_SOURCE_CLOUD_SYNC = 9,     /**< 云边参数同步 */
} data_source_t;

/**
 * @brief 记录文件的头部信息结构体
 * @note 头部大小固定为 20 字节。
 *       使用联合体，方便以字节数组或结构化形式访问。
 */
typedef union {
    /**
     * @brief 以字节数组形式访问全部20字节数据。
     */
    uint8_t all[REG_CHANGE_FILE_HEADER_LEN];

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
} reg_change_file_header_t;

_Static_assert(sizeof(reg_change_file_header_t) == REG_CHANGE_FILE_HEADER_LEN, "reg_change_file_header_t size mismatch");


/**
 * @brief 参数修改的单条记录结构体
 * @note 总长度为 20 字节。使用联合体，方便以字节数组或结构化形式访问。
 * @源地址：在Modbus协议中无效，在CAN协议中表示本次写入的源CANID
 * @目标地址：在Modbus协议中表示本次写入的目标从机地址，在CAN协议中表示本次写入的目标CANID
 * @起始地址：在Modbus协议中表示本次写入指令的寄存器起始地址(如2000)，在CAN协议中表示本次写入指令的Tpye类型（如0x1A）
 * @偏移地址：在Modbus协议中表示本次写入寄存器地址相对于起始地址的地址偏移数，在CAN协议中表示本次写入位置在Tpye类型中的偏移字节数
 */
typedef union {
    /**
     * @brief 以字节数组形式访问全部20字节数据。
     */
    uint8_t all[REG_CHANGE_LOG_LEN];

    /**
     * @brief 以结构化形式访问数据成员。
     * 内部结构体也需要 packed 属性以保证与字节数组的内存布局完全一致。
     */
    struct __attribute__((packed)) {
        uint32_t timestamp;             /**< 时间戳 (4字节) */
        uint8_t source_addr;            /**< 源地址 (1字节) */
        uint8_t target_addr;            /**< 目标地址 (1字节) */
        uint16_t start_address;         /**< 起始地址 (2字节) */
        uint16_t offset_address;        /**< 偏移地址 (2字节) */
        uint16_t original_value;        /**< 原值 (2字节) */
        uint16_t modified_value;        /**< 修改后值 (2字节) */
        uint8_t  protocol_version;      /**< 协议版本 (1字节) */
        uint8_t  modification_source;   /**< 修改源头 (1字节) */
    };
} reg_param_change_record_t;

_Static_assert(sizeof(reg_param_change_record_t) == REG_CHANGE_LOG_LEN, "reg_param_change_record_t size mismatch");

/**
 * @brief 用于封装指向参数修改记录的指针的结构体。
 *
 * 这个结构体主要用于在不同模块或任务之间传递记录的引用，
 * 例如在 FreeRTOS 队列中传递指针，以避免复制整个20字节的记录数据，
 * 从而提高效率并减少栈空间的使用。
 */
typedef struct {
    /**
     * @brief 指向一个 reg_param_change_record_t 实例的指针。
     */
    reg_param_change_record_t *p_record;
} reg_param_record_pointer_t;

/**
 * @brief Modbus参数修改日志生成所需参数的集合。
 * @details 用于替代 Reg_Change_Log_Generate_By_Modbus 函数的长参数列表。
 */
typedef struct {
    uint8_t  slave_address;       ///< Modbus从机地址
    uint16_t start_address;       ///< 操作的起始寄存器地址
    uint16_t reg_count;           ///< 操作的寄存器数量
    uint16_t *old_data;           ///< 指向修改前数据的指针
    uint16_t *new_data;           ///< 指向修改后数据的指针
    uint8_t  protocol_version;    ///< 协议版本
    uint8_t  modification_source; ///< 修改源
} modbus_change_params_t;

/**
 * @brief CAN总线参数修改日志生成所需参数的集合。
 * @details 用于替代 Reg_Change_Log_Generate_By_Can 函数的长参数列表。
 */
typedef struct {
    uint8_t  source_id;           ///< CAN报文的源设备地址
    uint8_t  target_id;           ///< CAN报文的目标设备地址
    uint16_t can_type;            ///< CAN数据类型或功能码，用作分类
    uint16_t offset;              ///< 本次修改在 can_type 分类中的起始偏移
    uint16_t length;              ///< 本次修改的参数数量
    uint8_t *old_data;            ///< 指向修改前数据的指针
    uint8_t *new_data;            ///< 指向修改后数据的指针
    uint8_t  protocol_version;    ///< 协议版本
    uint8_t  modification_source; ///< 修改源
} can_change_params_t;

#pragma pack()

/* ============================ 头文件对外函数接口定义 ================================ */

#ifdef __cplusplus
extern "C" {
#endif


 /**
 * @brief 寄存器修改模块初始化
 * @note 建议在系统上电后初始化期间执行。
 */
int Reg_Change_Module_Init(void);


/**
 * @brief      根据一次Modbus写入操作生成多条参数修改记录 (结构体参数版本)。
 * @details    此函数将一次连续的Modbus寄存器写入操作分解为多条独立的修改记录。
 *             它接收一个包含所有必要参数的结构体，为每条记录动态分配内存，
 *             并将其指针发送到日志队列以供后台异步处理。
 *
 * @param[in]  params  一个指向 `modbus_change_params_t` 结构体的常量指针，
 *                     该结构体封装了生成日志所需的所有参数。
 *
 * @return
 *         - 0: 成功，所有记录已成功创建并尝试发送到队列。
 *         - -1: 失败，可能由于参数错误或内存分配失败。
 *
 * @note
 *         - 调用此函数时，应先在调用方栈上创建并填充一个 `modbus_change_params_t` 结构体，
 *           然后将其地址传递给此函数。
 *         - 这是一个异步函数，它不直接执行写日志操作。
 *         - 消费队列消息的任务在处理完数据后，**必须**负责调用 `free()` 来释放内存。
 *         - 不建议在单次写入寄存器数量 (`params->reg_count`) 过大时调用此函数，
 *           因为这可能会在短时间内造成较大的内存分配压力。
 *
 * @par Example:
 * @code
 * // 这是一个在Modbus回调函数中如何使用本函数的示例。
 * // 假设有一个回调函数，当特定地址范围被写入时触发（该回调通常定义在“modbus_protocol.c”文件内）
 * // 实际使用时，可以有选择性的仅关注部分寄存器，降低芯片写入压力
 * int Modbus_Callback_Example(md_tbl_t *tbl, tbl_cb_data_t *cb_data, md_priv_data_t *priv_data)
 * {
 *     // 仅在写入操作时记录日志
 *     if (cb_data->is_write == true)
 *     { 
 * #ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
 *         // 1. 确定修改来源 (例如: BLE, WiFi, UART等)
 *         uint8_t modification_source = DATA_SOURCE_UNKNOWN;
 *         switch (cb_data->cb_chl)
 *         {
 *             case MD_CHL_BLE:
 *                 modification_source = DATA_SOURCE_APP_BLE_SERVER;
 *                 break;
 *             case MD_CHL_WIFI_CLOUD:
 *                 modification_source = DATA_SOURCE_WIFI_MQTT;
 *                 break;
 *             // ... 其他通道
 *             default:
 *                 break;
 *         }
 *
 *         // 2. 声明并填充参数结构体
 *         //    - old_data 指向实际的只读数据区，用于获取修改前的值。
 *         //    - new_data 指向临时的写入缓冲区，用于获取即将写入的新值。
 *         modbus_change_params_t log_params = {
 *             .slave_address       = cb_data->SlaveAddress,
 *             .start_address       = cb_data->reg_addr,
 *             .reg_count           = cb_data->reg_nums,
 *             .old_data            = (uint16_t *)((uint8_t *)&g_modbus_read_area + cb_data->reg_addr_offset),
 *             .new_data            = (uint16_t *)((uint8_t *)&g_modbus_write_buffer + cb_data->reg_addr_offset),
 *             .protocol_version    = PROTOCOL_MODBUS_BETA,
 *             .modification_source = modification_source,
 *         };
 *
 *         // 3. 调用日志生成函数
 *         if (Reg_Change_Log_Generate_By_Modbus(&log_params) != 0) {
 *             // 可选：处理日志生成失败的情况
 *             ESP_LOGE(TAG, "Failed to generate modbus change log.");
 *         }
 *
 * #endif
 *
 *         // 4. 执行实际的数据拷贝操作，将新值更新到只读数据区
 *         memcpy((uint8_t *)&g_modbus_read_area + cb_data->reg_addr_offset,
 *                (uint8_t *)&g_modbus_write_buffer + cb_data->reg_addr_offset,
 *                cb_data->reg_nums * 2);
 *     }
 *
 *     return 0; // 返回成功
 * }
 * @endcode
 */
int Reg_Change_Log_Generate_By_Modbus(const modbus_change_params_t *params);


/**
 * @brief      根据一次CAN总线写入操作生成多条参数修改记录 (结构体参数版本)。
 * @details    此函数将一次连续的CAN数据修改操作分解为多条独立的修改记录。
 *             它接收一个包含所有必要参数的结构体，为每条记录动态分配内存，
 *             并将其指针发送到日志队列以供后台异步处理。
 *
 * @param[in]  params  一个指向 `can_change_params_t` 结构体的常量指针，
 *                     该结构体封装了生成日志所需的所有参数。
 *
 * @return
 *         - 0: 成功，所有记录已成功创建并尝试发送到队列。
 *         - -1: 失败，可能由于参数错误或内存分配失败。
 *
 * @note
 *         - 调用此函数时，应先在调用方栈上创建并填充一个 `can_change_params_t` 结构体，
 *           然后将其地址传递给此函数。
 *         - 这是一个异步函数，它不直接执行写日志操作。
 *         - 消费队列消息的任务在处理完数据后，**必须**负责调用 `free()` 来释放内存。
 *         - 不建议在单次写入参数数量 (`params->length`) 过大时调用此函数，
 *           因为这可能会在短时间内造成较大的内存分配压力。
 *
 * @par Example (具体用法):
 * @details
 *      此函数通常在处理底层通信协议的模块中被调用。一个典型的应用场景是
 *      在CAN总线的多帧数据接收处理函数中，例如项目中的 `CanVerifyData` 函数。
 *
 *      **调用时机与关键逻辑**：
 *      在 `CanVerifyData` 的设计中，日志记录发生在 **CRC校验成功** 且 **主数据区已被新数据覆盖 (`memcpy`) 之后**。
 *      这是一个关键点，因为它意味着在调用本函数时，原始的旧数据已不存在于其原始位置。
 *
 *      为了解决这个问题，`CanVerifyData` 的实现采用了一种精巧的方法：
 *      1.  首先，将接收到的新数据 `cmd->temp_buffer` 拷贝到主数据区 `ptr + cmd->write_offset`。
 *      2.  然后，**再次调用 `CanLookupTypePosition`**，但这次是为了查找一个独立的、用于读取的、未被修改的【只读数据区】（`rd_ptr`），并从该区域获取【旧值】。
 *      3.  最后，将【只读数据区】的旧值和【主数据区】的新值一起传递给本函数以生成日志。
 *
 * @code
 * // 以下代码精确地复现了在 CanVerifyData 函数中的高级调用逻辑。
 *
 * // ... 在 CanVerifyData 函数中，当确认所有数据帧已接收完毕并通过CRC校验后 ...
 * if (cmd->write_crc16 == crc16)
 * {
 *     // 1. 【关键步骤】首先，执行数据覆盖操作。
 *     //    此时，(ptr + cmd->write_offset) 中的旧数据被覆盖。
 *     memcpy((ptr + cmd->write_offset), cmd->temp_buffer, data_len);
 *
 * #ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
 *     // 2. 仅当操作为“写入本机”时，才记录日志。
 *     if (1 == Writeflag)
 *     {
 *         // 3. 【关键步骤】为了获取已被覆盖的旧值，需查找对应的只读数据区。
 *         uint8_t *rd_ptr = NULL; // 用于指向只读区的指针
 *         rw_cmd_struct *rd_cmd = NULL;
 *         uint32_t rd_maxlen = 0;
 *         CanLookupTypePosition(0, node, devId, type, &rd_ptr, &rd_maxlen, &rd_cmd);
 *
 *         if (rd_cmd && rd_ptr && (cmd->write_offset + data_len <= rd_maxlen)) {
 *             // 4. 声明并填充日志参数结构体
 *             can_change_params_t log_params = {
 *                 .source_id           = RxcanId.bit.src,
 *                 .target_id           = RxcanId.bit.dst,
 *                 .can_type            = cmd->can_type,
 *                 .offset              = cmd->write_offset,
 *                 .length              = data_len,
 *                 .old_data            = (uint16_t *)(rd_ptr + cmd->write_offset), // 从【只读区】获取旧值
 *                 .new_data            = (uint16_t *)(ptr + cmd->write_offset),    // 从【主数据区】获取新值
 *                 .protocol_version    = PROTOCOL_CAN_BETA,
 *                 .modification_source = DATA_SOURCE_CAN,
 *             };
 *
 *             // 5. 调用日志生成函数
 *             if (Reg_Change_Log_Generate_By_Can(&log_params) != 0) {
 *                 ESP_LOGE(TAG, "Failed to generate CAN change log.");
 *             }
 *         }
 *     }
 * #endif
 *
 *     // ... 后续其他处理 ...
 * }
 * @endcode
 */
int Reg_Change_Log_Generate_By_Can(const can_change_params_t *params);

/**
 * @brief      从队列中批量消费日志记录，并以循环方式写入文件。
 * @details    此函数是寄存器修改日志模块的“消费者”核心。它被设计为一个周期性执行的任务。
 *             其主要工作流程如下：
 *             1. **批量拉取**: 非阻塞地从 `xQueue_reg_change_log` 队列中拉取最多
 *                `REG_CHANGE_PR0CESS_MAX_NUM` 条记录，暂存到本地数组中。
 *                这样做是为了将多次零散的文件写入合并为一次或两次批量写入，以提升I/O性能。
 *             2. **内存管理**: 将队列中指针指向的数据复制到本地后，立即释放生产者分配的内存，
 *                防止内存泄漏。
 *             3. **分块计算**: 根据文件当前状态（是否已满），计算出本次批量写入需要被拆分的
 *                两个部分（`cnt1` 和 `cnt2`）。`cnt1` 是填满当前空间的部分，`cnt2` 是
 *                回环覆盖到文件开头的部分。
 *             4. **循环写入**: 调用辅助函数 `Reg_Change_Log_Write`，最多执行两次写入操作，
 *                以完成 `cnt1` 和 `cnt2` 数据的持久化。
 *             5. **文件头更新**: 根据 `cnt1` 和 `cnt2` 的计算结果，精确地更新文件头信息
 *                （`current_records` 和 `write_index`），以保证状态的正确性。
 *             6. **状态持久化**: 将更新后的文件头写回磁盘。
 *
 * @note       - 此函数应作为一个周期性任务（Task）来调用，例如每隔1-5秒执行一次。
 *             - 函数内部包含复杂的文件I/O操作，可能会有一定耗时，不应在对实时性
 *               要求极高的上下文（如中断服务程序）中调用。
 *             - 函数的写入逻辑依赖于一个全局缓存的文件头 `Reg_Change_File_Header`，
 *               该文件头在 `Reg_Change_Module_Init` 中被初始化。
 */
void Reg_Change_Log_Save_Task(void);

/**
 * @brief      读取增量日志数据到动态分配的缓冲区。
 * @details    此函数是实现日志增量同步的核心接口。它会：
 *             1. 调用辅助函数 `Reg_Change_Log_Count_New_Records` 计算出新记录的数量。
 *             2. 根据新记录数量计算总大小（包含文件头），并分配相应大小的内存。
 *             3. 从文件开头读取文件头，然后从文件中读取所有新记录，
 *                并将它们按时间顺序（旧->新）存入缓冲区。
 *             4. 通过输出参数返回缓冲区地址和总大小。
 *             5. 注意，返回的缓冲区大小取决于REG_CHANGE_LOG_MAX_NUM，
 *                1000条时可达20k，自行评估使用
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
 *     // 使用 Is_Reg_Change_Log_File 来判断文件类型
 *     if (0 == Is_Reg_Change_Log_File(fname))
 *     {
 *         // 文件是特殊的“寄存器修改日志文件”，调用其专用的全量读取函数。
 *         // 该函数会内部自分配内存。
 *         if (Reg_Change_Log_Read_Incremental(0, &data, size) != 0)
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
int Reg_Change_Log_Read_Incremental(time_t since_timestamp, uint8_t **out_buffer, uint32_t *out_size);

/**
 * @brief      判断传入的文件标识符是否为当前的寄存器修改日志文件。
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
int Is_Reg_Change_Log_File(const char *fname);

/**
 * @brief 生成当前寄存器修改日志对应的文件标识符。
 *
 * 标识符格式与 Is_Reg_Change_Log_File 接受的 fname 参数一致：
 * - out_buf[0] = 文件夹层级字符（REG_CHANGE_FILE_FOLDER_LEVEL[0]）
 * - &out_buf[1] = 文件名字符串（不含路径分隔符）
 *
 * @param[out] out_buf  输出缓冲区，至少应能容纳 1 + 文件名长度
 * @param[in]  buf_len  out_buf 长度（字节）
 *
 * @return int
 *         - 0 : 成功（out_buf 被填充）
 *         - -1: 参数错误或缓冲区不足
 */
int Get_Reg_Change_Log_File_Id(char *out_buf, size_t buf_len);

/**
 * @brief 统计晚于指定时间戳的寄存器修改日志新记录数（含信号量保护）。
 *
 * @param[in] since_timestamp  自 Unix 纪元起的时间戳；传 0 表示统计全部记录。
 *
 * @return int
 *         - >=0 : 新记录数量
 *         - -1  : 参数错误或其它不可恢复错误
 *         - -2  : 获取文件处理信号量失败（记录将被丢弃）
 */
int Is_Reg_Change_Log_Count_New(time_t since_timestamp);

#ifdef __cplusplus
}
#endif
