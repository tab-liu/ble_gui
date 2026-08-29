/**
 * 历史数据传输模块示例: 本示例为IOT节点功能
 *
 * 示例通信包括三个节点IOT、ARM、PACK,其通信架构如下所示:
 *		┌───────┐
 *	IOT	│  485  │
 *		└───┬───┘
 *			│
 *		┌───┴───┐
 *		│  485  │
 *	ARM	│       │
 *		│  CAN  │
 *		└───┬───┘
 *			│
 *		┌───┴───┐
 *	PACK│  CAN  │
 *		└───────┘
 *
 * IOT通过485与ARM通信,ARM通过CAN与PACK通信,IOT节点地址0,ARM节点地址1,PACK节点地址41
 * 对IOT来说,其上级设备是云服务器或PC端,下级设备是ARM
 * 对ARM来说,其上级设备是IOT或PC端,下级设备是PACK
 * 对PACK来说,其上级设备是ARM或PC端,无下级设备
 *
 * UDT(通用数据传输)模块包含三个功能:
 * 1、通用数据中继功能函数
 * - udt_relay_to_terminal: 将下级设备上报数据转发到终端(上级设备)
 * - udt_relay_to_device: 将终端(上级设备)下发的数据转发到下级设备
 * 2、通用数据收发功能函数
 * - udt_transmit: 上报本机数据到终端(上级设备)
 * - udt_receive: 接收终端(上级设备)下发给本机的数据
 * 3、PC端接口功能函数
 * - udt_pc_interface: 处理与PC端的通信
 *
 *
 * IOT节点功能:
 * - 实现历史记录通用数据中继:
 * - 接收到终端获取ARM/PACK的历史数据时,转发终端下发的指令
 * - 接收到ARM/PACK上报的历史数据时,转发ARM/PACK上报的数据
 *
 * ARM节点功能:
 * 1、实现历史记录通用数据收发,即接收终端下发的指令,根据指令将本地历史数据发送到终端
 * 2、实现历史记录通用数据中继:
 * - 接收到终端获取PACK的历史数据时,转发终端下发的指令
 * - 接收到PACK上报的历史数据时,转发PACK上报的数据
 *
 * PACK节点功能:
 * - 实现历史记录通用数据收发,即接收终端下发的指令,根据指令将本地历史数据发送到终端
 * - PACK节点无中继功能
 */

#include "udt_transfer.h"
#include "udt_port.h"
#include "udt_incremental.h"
#include <string.h>
#include <time.h>

#include "dev_data_record.h"
#include "esp_system.h"
#include "esp_log.h"
#include "parameter.h"
#include "modbus_data.h"
#include "reg_change_log.h"
#include "dev_access_log.h"
#include "energy_process.h"
#include "iot_box_task.h"
#include "mqtt_log.h"

#include "utils.h"
#include "comm_define.h"


#define IOT_NODE_ADDR		0	// IOT节点地址
#define ARM_NODE_ADDR		1 	// ARM节点地址
#define PACK_NODE_ADDR		41 	// PACK节点地址
//static const char fname[] = "\0alarm\0\0\0\0,";	// 目录结构/alarm

#define TAG "[UDT_EXP_IOT]"

static uint8_t udt_last_read_fname[11] = {0};
static uint16_t udt_last_send_total = 0;
static time_t udt_last_read_time = 0;

/**
  * @brief    从存储根据文件名和偏移读取数据，支持基于时间戳的增量过滤
  *
  * @details  行为说明：
  *            - 根据 fname 定位文件（或路径）并从 offset 开始读取；
  *            - 若 last_read_time 非 0，则仅返回时间戳晚于 last_read_time 的数据（由 type/文件格式决定如何判断）；
  *            - data 为调用者提供的缓冲区指针（若为 NULL，可由函数内部分配并返回）；
  *            - *size 作为输入时表示 data 缓冲区大小，返回时表示实际读取长度或剩余长度（按模块约定）。
  *
  * @param[in]  fname           文件名或相对路径字符串（以模块约定为准）
  * @param[in]  offset          从文件开头的字节偏移
  * @param[in]  last_read_time  时间戳过滤（0 表示不按时间过滤）
  * @param[out] data            用于接收数据的缓冲区指针（可为 NULL）
  * @param[out] size            输入：缓冲区大小（或期望读取长度）；输出：实际读取到的数据长度或剩余长度
  *
  * @return   成功返回指向数据的指针（通常为 data 或内部分配的指针），失败返回 NULL（*size 不保证被修改）
  */
uint8_t* get_data_from_storage(const char *fname, uint32_t offset, time_t last_read_time, uint8_t *data, uint32_t *size)
{
	// 从存储器中获取数据
    if ( 0 == Is_Inv_Log_File(fname) )
    {
        // 调用 Inv_Log_Read_Incremental 进行全量/增量读取。
        // 该函数会内部自分配内存，并通过二级指针返回缓冲区地址。
        if (Inv_Log_Read_Incremental(last_read_time, &data, size) != 0)
        {
            // 如果读取失败，确保返回 NULL
            data = NULL;
        }

        return data;
    }

	// 从存储器中获取数据
    if ( 0 == Is_Event_Log_File(fname) )
    {
        // 调用 IoT_Event_Log_Read_Incremental 进行全量/增量读取。
        // 该函数会内部自分配内存，并通过二级指针返回缓冲区地址。
        if (IoT_Event_Log_Read_Incremental(last_read_time, &data, size) != 0)
        {
            // 如果读取失败，确保返回 NULL
            data = NULL;
        }

        return data;
    }

#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
	// 从存储器中获取数据
    if ( 0 == Is_Reg_Change_Log_File(fname) )
    {
        // 调用 Reg_Change_Log_Read_Incremental 进行全量/增量读取。
        // 该函数会内部自分配内存，并通过二级指针返回缓冲区地址。
        if (Reg_Change_Log_Read_Incremental(last_read_time, &data, size) != 0)
        {
            // 如果读取失败，确保返回 NULL
            data = NULL;
        }

        return data;
    }
#endif

#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE
    // 从存储器中获取数据
    if ( 0 == Is_Dev_Access_Log_File(fname) )
    {
        // 调用 Dev_Access_Log_Read_Incremental 进行全量/增量读取。
        // 该函数会内部自分配内存，并通过二级指针返回缓冲区地址。
        if (Dev_Access_Log_Read_Incremental(last_read_time, &data, size) != 0)
        {
            // 如果读取失败，确保返回 NULL
            data = NULL;
        }

        return data;
    }
#endif

#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
    // 从存储器中获取数据
    if ( 0 == Is_Energy_File_Unit_Data_File(fname) )
    {
        // 调用 Energy_File_Unit_Data_Read_Incremental 进行全量/增量读取。
        // 该函数会内部自分配内存，并通过二级指针返回缓冲区地址。
        if (Energy_File_Unit_Data_Read_Incremental(last_read_time, &data, size) != 0)
        {
            // 如果读取失败，确保返回 NULL
            data = NULL;
        }

        return data;
    }
#endif

#ifdef INV_LOG_DETAILED_INFO_RECORD
    // 从存储器中获取数据
    if ( 0 == Is_Box_Log_File(fname) )
    {
        // 调用 Box_Log_Read_Incremental 进行全量/增量读取。
        // 该函数会内部自分配内存，并通过二级指针返回缓冲区地址。
        if (Box_Log_Read_Incremental(last_read_time, &data, size) != 0)
        {
            // 如果读取失败，确保返回 NULL
            data = NULL;
        }

        return data;
    }
#endif

#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
    // 从存储器中获取数据
    if ( 0 == Is_Mqtt_Log_File(fname) )
    {
        // 调用 Mqtt_Log_Read_Incremental 进行全量/增量读取。
        // 该函数会内部自分配内存，并通过二级指针返回缓冲区地址。
        if (Mqtt_Log_Read_Incremental(last_read_time, &data, size) != 0)
        {
            // 如果读取失败，确保返回 NULL
            data = NULL;
        }

        return data;
    }
#endif

    /* 读取通用文件数据指定长度size到data */
    if(historic_data_get_size(fname, offset, size) >= 0) {
        data = iot_calloc(*size);
        if(data == NULL) {
            return NULL;
        }
        if(historic_data_read(fname, data, size, offset) < 0) {
            free(data);
            return NULL;
        }
    }
    
    return data;
}

/**
 * @brief 查询文件响应
 *
 * @param evt 终端事件
 * @return 无
 */
static void udt_resp_query_file(udt_event_t *evt)
{
	udt_tx_data_t tx_data;
	uint16_t dir_file_len=0;
	uint8_t *dir_file_data = NULL;

//	char fname[30] = {0};
//	COMMON_FILE_PATH_YYMM(fname,KWH_RECORD_FOLDER_LEVEL1, reals.rtc_time.year, reals.rtc_time.mon);
	filename_list_get((char *)evt->param.qry_cont.fname);
	Filename_Compare_Sequence();


	dir_file_data = iot_calloc(reals.file_nums * (FILE_DIR_LEN + 1));

	/* 序列化文件名及目录名 */
	dir_file_len = (uint16_t)historic_record_serialize_query( dir_file_data);

	int offset = 0;
	int total = dir_file_len / UDT_ONCE_TX_MAX_SIZE;		// 计算传输的总包数
	if (dir_file_len % UDT_ONCE_TX_MAX_SIZE) 
	{
		total += 1;
	}

	/* 分包传输 */
	for (int i = 0; i < total; i++)
	{
		udt_tx_data_t tx_data;
		tx_data.funcode = UDT_FUNCODE_QUERY_FILE_RTN;	    // 功能码
        tx_data.mode = evt->mode;                           // 发送方式
        tx_data.req_id = evt->req_id;                       // 查询标志
		tx_data.pkg_idx = i + 1;							// 包索引
		tx_data.pkg_total = total;							// 总包数
		tx_data.start_pos = offset;							// 数据偏移
		tx_data.data_size = (dir_file_len > UDT_ONCE_TX_MAX_SIZE) ? (UDT_ONCE_TX_MAX_SIZE) : (dir_file_len);	// 数据长度
		tx_data.data = &dir_file_data[offset];						// 数据

        // 发送数据到终端端
		if(udt_transmit(&tx_data) < 0) {
            break;  // 发送失败，立即停止
        }

		offset += tx_data.data_size;						// 计算下次数据偏移
		dir_file_len -= tx_data.data_size;							// 计算剩余发送数据大小
	}
    
	free(dir_file_data);
    free(fsys_file_list);
    fsys_file_list = NULL;
}

/**
 * @brief 查询文件内容响应
 *
 * @param evt 终端事件
 * @return 无
 */
static void udt_resp_query_content(udt_event_t *evt)
{
	uint8_t *pdata = NULL;
	uint32_t len = 0;
    time_t last_read_time = 0;
    uint16_t last_send_cnt = 0;
    uint16_t last_resp_cnt = 0;
    
    int index = Udt_Incremental_Find_Index(evt->param.qry_cont.fname);
    ESP_LOGI(TAG, "[udt_resp_query_content] UDT record index(%d)", index);
    
#ifdef CONFIG_UDT_INCREMENTAL_READ_ENABLE
    // 1-增量读取（下级只上报差异部分）
    if ( 1 == evt->param.qry_cont.type_read ) {
        if ( 0 <= index ) {
            last_read_time = Udt_Incremental_Get_Record_By_Index(index)->last_time;
            ESP_LOGW(TAG, "[udt_resp_query_content] UDT last read time: %llu.", last_read_time);
        }
    }
#endif

	pdata = get_data_from_storage((const char *)evt->param.qry_cont.fname, evt->param.qry_cont.start_pos, last_read_time, pdata, &len);	// 从存储器中获取历史数据
	if(pdata == NULL) {
	    ESP_LOGE(TAG, "[udt_resp_query_content] error");
		return;
	}

	uint8_t *send_pdata = pdata;
	int offset = 0;
	int total = len / UDT_ONCE_TX_MAX_SIZE;		// 计算传输的总包数
	if (len % UDT_ONCE_TX_MAX_SIZE) {
		total += 1;
	}

#ifdef CONFIG_UDT_BREAKPOINT_RESUME_ENABLE
    // 检查是否需要断点续传（仅增量读取时触发）
    if ( 1 == evt->param.qry_cont.type_read ) {
        if ( 0 <= index ) {
            last_send_cnt = Udt_Incremental_Get_Record_By_Index(index)->last_send_cnt;
            last_resp_cnt = Udt_Incremental_Get_Record_By_Index(index)->last_resp_cnt;

            if (( last_send_cnt > last_resp_cnt )
                && ( total > last_resp_cnt ))
            {
                ESP_LOGW(TAG, "[udt_resp_query_content] Total(%d), last_resp_cnt(%d)", total, last_resp_cnt);
                total = total - last_resp_cnt;
                send_pdata = pdata + (UDT_ONCE_TX_MAX_SIZE * last_resp_cnt);
                len = len - (UDT_ONCE_TX_MAX_SIZE * last_resp_cnt);
            }
        }
    }
#endif

    // 更新发送记录
    memcpy(udt_last_read_fname, evt->param.qry_cont.fname, sizeof(udt_last_read_fname));
    udt_last_send_total = total;
    udt_last_read_time = time(NULL);
    
	/* 分包传输 */
	for (int i = 0; i < total; i++)
	{
		udt_tx_data_t tx_data;
		tx_data.funcode = UDT_FUNCODE_QUERY_CONTENT_RTN;	// 功能码
        tx_data.mode = evt->mode;                           // 发送方式
        tx_data.req_id = evt->req_id;                       // 查询标志
		tx_data.pkg_idx = i + 1;							// 包索引
		tx_data.pkg_total = total;							// 总包数
		tx_data.start_pos = offset;							// 数据偏移
		tx_data.data_size = (len > UDT_ONCE_TX_MAX_SIZE) ? (UDT_ONCE_TX_MAX_SIZE) : (len);	// 数据长度
		tx_data.data = &send_pdata[offset];						// 数据
		
        // 发送数据到终端端
		if(udt_transmit(&tx_data) < 0) {
            break;  // 发送失败，立即停止
        }

		offset += tx_data.data_size;						// 计算下次数据偏移
		len -= tx_data.data_size;							// 计算剩余发送数据大小		
	}
	free(pdata);
}

// UDT上电后首次检查时间间隔
#define UDT_POWERUP_CHECK_TIMEOUT   10800000    // 3h

// UDT周期检查上报周期
#define UDT_PERIOD_CHECK_TIMEOUT    86400000    // 24h

// UDT周期发送周期
#define UDT_PERIOD_SEND_TIMEOUT     300000       // 5min

/**
 * @brief 通用数据通道主动上报检查（保留 report_step 扩展点）
 *
 * 功能说明：
 * - 周期性检查寄存器修改日志是否有新增记录，若有则通过 UDT 上报。
 * - 使用 report_step 的 switch 结构预留多步流程扩展（例如分阶段扫描、延迟重试等）。
 * - 调用 Is_Reg_Change_Log_Count_New() 获取新记录数量；该函数内部已做信号量保护，
 *   因此本函数无需再次上锁。
 * - new_log.file_name 严格按 fname 原始字节拷贝，不强求 null 终止，长度由 file_name_len 指定。
 *
 * 主要变量：
 * - last_start_time: 上次开始检查的系统时间（ms）
 * - last_send_time : 上次成功发送 UDT 报文的系统时间（ms）
 * - report_step    : 状态机步骤索引（便于扩展）
 *
 * 流程概要：
 * 1. 节流：根据时间间隔判断是否需要执行本次检查（避免频繁扫描和发送）。
 * 2. 通过 Get_Reg_Change_Log_File_Id 获取文件标识（fname），若失败则退出本次检查。
 * 3. 根据 fname 查增量索引（Udt_Incremental_Find_Index）并读取基准时间（若存在）；
 *    若无索引或记录不可得，则视为全量（since_ts = 0）。
 * 4. 调用 Is_Reg_Change_Log_Count_New(since_ts) 统计晚于基准时间的新记录数。
 * 5. 如果有新记录 (>0)，构造 TLV_NewLog_t（按字节拷贝 file_name，不写终止符），通过 udt_transmit 发送。
 * 6. 更新周期控制变量并推进 report_step（循环使用，防止单步阻塞）。
 *
 * 注意事项：
 * - 函数内不重复使用信号量，依赖 Is_Reg_Change_Log_Count_New 的内部保护。
 * - fname 缓冲使用固定长度（10 字节），拷贝时严格按长度限制，避免越界。
 * - 上报报文中的 file_name_len 必须与 file_name 中拷贝的字节数一致，以便接收端正确解析。
 */
int udt_period_check_and_report(void)
{
    static uint32_t last_start_time = 0;
    static uint32_t last_send_time = 0;
    static uint8_t report_step = 0;

    uint8_t fname[10] = {0};
    uint8_t find_step = 0;
    uint16_t new_records_count = 0;
    time_t since_ts = 0;
    int ret = -1;
    
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* 节流检查：如果距离上次启动或上次发送时间过短，则跳过 */
    if (((0 == last_start_time) && ((now_time - last_start_time) < UDT_POWERUP_CHECK_TIMEOUT))
        || ((0 != last_start_time) && ((now_time - last_start_time) < UDT_PERIOD_CHECK_TIMEOUT))
        || ((now_time - last_send_time) < UDT_PERIOD_SEND_TIMEOUT)) {
        return -1;
    }

    /* 保留 switch 结构，case 可用于不同阶段的初始化或条件判断 */
    switch (report_step) {
        case 0:
            /* 获取当前日志的文件标识（fname） */
            if (Get_Inv_Log_File_Id((char *)fname, sizeof(fname)) == 0) {
                find_step = 1; /* 成功获取，进入下一步处理 */
            }
            break;
            
        case 1:
            /* 获取当前日志的文件标识（fname） */
            if (Get_IoT_Event_Log_File_Id((char *)fname, sizeof(fname)) == 0) {
                find_step = 1; /* 成功获取，进入下一步处理 */
            }
            break;

        case 2:
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE            
            /* 获取当前寄存器修改日志的文件标识（fname） */
            if (Get_Reg_Change_Log_File_Id((char *)fname, sizeof(fname)) == 0) {
                find_step = 1; /* 成功获取，进入下一步处理 */
            }
#endif            
            break;

        case 3:
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE            
            /* 获取当前日志的文件标识（fname） */
            if (Get_Dev_Access_Log_File_Id((char *)fname, sizeof(fname)) == 0) {
                find_step = 1; /* 成功获取，进入下一步处理 */
            }
#endif            
            break;

        case 4:
#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE            
            /* 获取当前日志的文件标识（fname） */
            if (Get_Energy_File_Unit_Data_File_Id((char *)fname, sizeof(fname)) == 0) {
                find_step = 1; /* 成功获取，进入下一步处理 */
            }
#endif            
            break;

        case 5:
#ifdef INV_LOG_DETAILED_INFO_RECORD            
            /* 获取当前日志的文件标识（fname） */
            if (Get_Box_Log_File_Id((char *)fname, sizeof(fname)) == 0) {
                find_step = 1; /* 成功获取，进入下一步处理 */
            }
#endif            
            break;

        case 6:
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE            
            /* 获取当前日志的文件标识（fname） */
            if (Get_Mqtt_Log_File_Id((char *)fname, sizeof(fname)) == 0) {
                find_step = 1; /* 成功获取，进入下一步处理 */
            }
#endif            
            break;

        default:
            /* 其它扩展步骤的默认处理（重置状态、更新时间等） */
            report_step = 0;
            last_start_time = now_time;
            return -1;
    }

    /* 未找到文件标识则退出（保留扩展点） */
    if (!find_step) goto exit;

    /* 查找增量索引，决定从哪个时间点开始统计新记录 */
    int index = Udt_Incremental_Find_Index(fname);    
    if (index >= 0) {
        /* 存在增量索引，尝试读取对应记录以获取上次时间点 */
        const udt_file_record_t *rec = Udt_Incremental_Get_Record_By_Index(index);
        if (rec != NULL) {
            since_ts = rec->last_time; /* 从该时间戳之后为新记录 */
        } else {
            since_ts = 0; /* 记录不可得，降级为全量统计 */
        }
    } else {
        /* 未找到索引，首次或全量上报 */
        since_ts = 0;
    }

    switch (report_step) {
        case 0:
            /* 调用已带信号量保护的计数函数统计新记录数 */
            new_records_count = Is_Inv_Log_Count_New(since_ts);
            if (new_records_count > 0) {
                find_step = 2; /* 有新记录，准备上报 */
            }
            break;
            
        case 1:
            /* 调用已带信号量保护的计数函数统计新记录数 */
            new_records_count = Is_IoT_Event_Log_Count_New(since_ts);
            if (new_records_count > 0) {
                find_step = 2; /* 有新记录，准备上报 */
            }
            break;

        case 2:
#ifdef CONFIG_REG_CHANGE_LOG_SAVE_ENABLE
            /* 调用已带信号量保护的计数函数统计新记录数 */
            new_records_count = Is_Reg_Change_Log_Count_New(since_ts);
            if (new_records_count > 0) {
                find_step = 2; /* 有新记录，准备上报 */
            }
#endif            
            break;

        case 3:
#ifdef CONFIG_DEV_ACCESS_LOG_SAVE_ENABLE
            /* 调用已带信号量保护的计数函数统计新记录数 */
            new_records_count = Is_Dev_Access_Log_Count_New(since_ts);
            if (new_records_count > 0) {
                find_step = 2; /* 有新记录，准备上报 */
            }
#endif            
            break;

        case 4:
#ifdef CONFIG_ENERGY_FILE_PROCESS_ENABLE
            /* 调用已带信号量保护的计数函数统计新记录数 */
            new_records_count = Is_Energy_File_Unit_Data_Count_New(since_ts);
            if (new_records_count > 0) {
                find_step = 2; /* 有新记录，准备上报 */
            }
#endif            
            break;

        case 5:
#ifdef INV_LOG_DETAILED_INFO_RECORD
            /* 调用已带信号量保护的计数函数统计新记录数 */
            new_records_count = Is_Box_Log_Count_New(since_ts);
            if (new_records_count > 0) {
                find_step = 2; /* 有新记录，准备上报 */
            }
#endif            
            break;

        case 6:
#ifdef CONFIG_MQTT_LOG_SAVE_ENABLE
            /* 调用已带信号量保护的计数函数统计新记录数 */
            new_records_count = Is_Mqtt_Log_Count_New(since_ts);
            if (new_records_count > 0) {
                find_step = 2; /* 有新记录，准备上报 */
            }
#endif            
            break;

        default:
            report_step = 0;
            last_start_time = now_time;
            break;
    }

    /* 若确定需要上报（find_step == 2）则构造 TLV 并发送 */
    if (find_step == 2) {
        TLV_NewLog_t new_log = {0};
        ESP_LOGI(TAG, "UDT Start Report record index %d, name: %.*s", index, 10, (const char *)fname);

        new_log.tag = 1; // 01-新增LOG条数类型
        new_log.value_len = sizeof(new_log.new_log_value);
        new_log.new_log_value = UDT_SWAP16(new_records_count); /* 网络/协议字节序转换（若需） */
        new_log.file_index = 2; // 02-文件名用ASCII字符串表示，共计10字节
        new_log.file_name_len = sizeof(new_log.file_name);
        memcpy(new_log.file_name, fname, sizeof(new_log.file_name));    // 直接使用文件标识符

        /* 构造并发送 UDT 报文 */
        udt_tx_data_t tx_data;
        memset(&tx_data, 0, sizeof(tx_data));
        tx_data.funcode = UDT_FUNCODE_NEW_LOG_REPORT;
        tx_data.mode = UDT_MODE_SVC;
        tx_data.pkg_idx = 1;
        tx_data.pkg_total = 1;
        tx_data.start_pos = 0;
        tx_data.data_size = sizeof(TLV_NewLog_t);
        tx_data.data = (uint8_t *)&new_log;

        ret = udt_transmit(&tx_data);
        last_send_time = now_time;
    }

exit:
    /* 推进状态机并做周期变量维护，保持 report_step 可扩展 */
    report_step++;
    return ret;
}

/**
 * @brief 事件处理
 *
 * @param evt 终端事件
 * @return 无
 */
static void udt_evt_process(udt_event_t *evt)
{
	switch (evt->funcode)
	{
	case UDT_FUNCODE_QUERY_FILE:					// 终端查询文件
	
	    ESP_LOGI(TAG, "UDT_FUNCODE_QUERY_FILE : %d",evt->funcode);
		udt_resp_query_file(evt);
		break;

	case UDT_FUNCODE_QUERY_CONTENT:					// 终端查询文件内容
	
	    ESP_LOGI(TAG, "UDT_FUNCODE_QUERY_CONTENT : %d",evt->funcode);
		udt_resp_query_content(evt);
		break;

	case UDT_FUNCODE_QUERY_CONTENT_SVC_ACK:			// 终端响应本次查询事件
	
		// 本次历史数据传输完成处理
		ESP_LOGI(TAG, "UDT_FUNCODE_QUERY_CONTENT_SVC_ACK (%d) : End_Pkg: %d (%d / %d)", evt->funcode, 
		        evt->param.svc_ack.last_pkg_seq, evt->param.svc_ack.pkg_total, udt_last_send_total);

        // 当前仅云端获取支持增量读取（APP读取时不可发送54帧响应）
        if ( UDT_MODE_SVC == evt->mode ) {
            // 更新读取记录
            udt_file_record_t new_record = udt_record_init_from((const char *)udt_last_read_fname, udt_last_read_time, udt_last_send_total, evt->param.svc_ack.last_pkg_seq);
            Udt_Incremental_Update_Record(&new_record);
        }
        
		break;

	default:
		return;
	}
    
    reals.file_nums = 0;
}

/**
 * @brief 事件回调函数
 *
 * @param evt 终端事件
 * @return 无
 */
static int udt_evt_callback(udt_event_t *evt)
{
	udt_evt_process(evt);
	return 0;
}


/**
 * @brief 通用数据协议配置
 *
 * @return 无
 */
void udt_configuration(void)//universal_data_transmission
{
	/* 设置传输函数 */
	udt_relay_t relay;
	uint8_t list_index = 0;

#if 1
    // 直接按CAN ID透传
    relay.list[list_index].addr1 = 0x01;
    relay.list[list_index].addr2 = 0xFF;
    relay.list[list_index].transfer = udt_transfer_to_can;
    list_index++;
#else
    // INV
	relay.list[list_index].addr1 = MD_INV_ADDR_START;
	relay.list[list_index].addr2 = MD_INV_ADDR_END;
	relay.list[list_index].transfer = udt_transfer_to_can;
	list_index++;

    // PACK (BA/BCU)
    relay.list[list_index].addr1 = MD_PACK_ADDR_START;
    relay.list[list_index].addr2 = MD_PACK_ADDR_END;
    relay.list[list_index].transfer = udt_transfer_to_can;
    list_index++;

    // DCDC
    relay.list[list_index].addr1 = MD_DCDC_ADDR_START;
    relay.list[list_index].addr2 = MD_DCDC_ADDR_END;
    relay.list[list_index].transfer = udt_transfer_to_can;
    list_index++;

    // DCHUB
    relay.list[list_index].addr1 = MD_DCHUB_ADDR_START;
    relay.list[list_index].addr2 = MD_DCHUB_ADDR_END;
    relay.list[list_index].transfer = udt_transfer_to_can;
    list_index++;
#endif

	relay.nums = list_index;
	udt_transfer_t transfer;
	transfer.to_server = udt_transfer_to_cloud;		// 传输到服务端
	transfer.to_pc = udt_transfer_to_pc;			// 传输到PC端->CAN
	transfer.to_ble = udt_transfer_to_ble;		    // 传输到蓝牙端
    
	udt_init(IOT_NODE_ADDR, &transfer, &relay, udt_evt_callback);
	udt_queue_init();

    /*UDT 增量读取模块初始化*/
    Udt_Incremental_Module_Init();
}


