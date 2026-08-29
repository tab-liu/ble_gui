/**
  ******************************************************************************
  * @file      cloud_sync.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/7/6
  * @brief     云边参数同步
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
#include "freertos/queue.h"
  * <tr><td>2026/7/6   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  参考文档
  1.https://192.168.102.10/svn/TecCenter/ComGroup/protocol/AC_coupling/阳台光伏产品-云端配套缓存变量定义20260624.xlsx
  2.https://192.168.102.10/svn/TecCenter/ComGroup/protocol/AC_coupling/Balco阳台光伏系列并机参数同步方案_V1.0_20260625.docx
  3.https://192.168.102.10/svn/TecCenter/ComGroup/protocol/AC_coupling/阳台光伏HEMS分析整理20260627.xlsx
  */

/* ================================ 库文件引用 ================================ */
   
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
    
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
  
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"
  
/* ======================== 本地模块文件引用（可选） ============================= */

#include "cloud_sync.h"
#include "parameter.h"
#include "modbus_data.h"
#include "modbus_slave.h"
#include "modbus_protocol.h"
#include "mqtt_payload_tlv.h"
#include "mini_tlv_struct.h"
#include "comm_define.h"

/* ================================ 文件内宏定义 ================================ */

#ifdef CONFIG_IOT_DATA_CLOUD_SYNC_ENABLE

#define TAG "[CLOUD_SYNC]"

/**
当前支持参数列表
1	setCtrlWorkMode	逆变工作模式	2005
2	setCapacity	SOC放电低阈值	2022
		        SOC充电高阈值	2023
3	setCtrlFeedback	设置馈电开启关闭	2208
4	setGridMaxCurrent	最大电网电流	2214
5	setFdMaxCurrent	最大馈电电流	2216
6	emsCtrl	EMS运行模式	2241
7	stormModeCloudCtrl	风暴预警	12163
8	meshId	MESH ID	13500~13502
9	acEmsCoordinationMode	本地设备间AC EMS调度协作模式	13809
10	tou1	TOU 1~40点位	26000~26280
11	tou2	TOU 41~100点位	26281~26700
12	tou3	TOU 101~140点位	26701~26980
13	ifThen	If-then	27001~27072
14	bluettiDevList	我司设备列表	21001
15	thirdDevList	第三方设备列表	21008
 */
#define FIELD_setCtrlWorkMode       "setCtrlWorkMode"
#define FIELD_setCapacity           "setCapacity"
#define FIELD_setCtrlFeedback       "setCtrlFeedback"
#define FIELD_setGridMaxCurrent     "setGridMaxCurrent"
#define FIELD_setFdMaxCurrent       "setFdMaxCurrent"
#define FIELD_emsCtrl               "emsCtrl"
#define FIELD_stormModeCloudCtrl    "stormModeCloudCtrl"
#define FIELD_meshId                "meshId" // 单机时影响识别，不作为参数上报
#define FIELD_acEmsCoordinationMode "acEmsCoordinationMode"
#define FIELD_tou1                  "tou1"
#define FIELD_tou2                  "tou2"
#define FIELD_tou3                  "tou3"
#define FIELD_ifThen                "ifThen"
#define FIELD_bluettiDevList        "bluettiDevList"
#define FIELD_thirdDevList          "thirdDevList"

/* =============================== 文件内全局变量 ================================ */

/* 参数对象文件头 */
USE_EXT_RAM_BSS static modbus_param_desc_header_t modbus_param_desc_header = {0};

/* 参数对象属性区（本地存储） */
USE_EXT_RAM_BSS static modbus_param_desc_t modbus_param_desc[MODBUS_PARAM_DESC_OBJECT_MAX] = {0};

/* 参数额外对象属性区（非本地存储） */
USE_EXT_RAM_BSS static cloud_sync_ext_param_desc_t cloud_sync_ext_param_desc[MODBUS_PARAM_DESC_OBJECT_MAX] = {0};

/* 参数字段表格 */
static const modbus_beta_field_object_t modbus_beta_field_table[] = {
    {
        .data_field      = FIELD_setCtrlWorkMode,
        .slaveaddr       = 0,
        .start_reg_addr  = 2005,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_setCapacity,
        .slaveaddr       = 0,
        .start_reg_addr  = 2022,
        .data_reg_cnt    = 2,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_setCtrlFeedback,
        .slaveaddr       = 0,
        .start_reg_addr  = 2208,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_setGridMaxCurrent,
        .slaveaddr       = 0,
        .start_reg_addr  = 2214,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_setFdMaxCurrent,
        .slaveaddr       = 0,
        .start_reg_addr  = 2216,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_emsCtrl,
        .slaveaddr       = 0,
        .start_reg_addr  = 2241,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_stormModeCloudCtrl,
        .slaveaddr       = 0,
        .start_reg_addr  = 12163,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_acEmsCoordinationMode,
        .slaveaddr       = 0,
        .start_reg_addr  = 13809,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_tou1,
        .slaveaddr       = 0,
        .start_reg_addr  = 26000,
        .data_reg_cnt    = 281, /* 26000~26280 */
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_tou2,
        .slaveaddr       = 0,
        .start_reg_addr  = 26281,
        .data_reg_cnt    = 420, /* 26281~26700 */
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_tou3,
        .slaveaddr       = 0,
        .start_reg_addr  = 26701,
        .data_reg_cnt    = 280, /* 26701~26980 */
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_ifThen,
        .slaveaddr       = 0,
        .start_reg_addr  = 27001,
        .data_reg_cnt    = 72,
        .is_user_defined = false,
    },
#if 0 // 待定   
    {
        .data_field      = FIELD_bluettiDevList,
        .slaveaddr       = 0,
        .start_reg_addr  = 21001,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
    {
        .data_field      = FIELD_thirdDevList,
        .slaveaddr       = 0,
        .start_reg_addr  = 21008,
        .data_reg_cnt    = 1,
        .is_user_defined = false,
    },
#endif    
};

/* 参数字段个数 */
static const size_t modbus_beta_field_table_count =
    sizeof(modbus_beta_field_table) / sizeof(modbus_beta_field_table[0]);

/* 云边参数同步互斥锁 */
static SemaphoreHandle_t xCloudSyncMutex = NULL;  

/* 云同步更新队列句柄 */
static QueueHandle_t xCloudSyncQueue = NULL;

/* 云同步接收标志 */
static bool cloud_sync_revd_param_flag = false;

/* 云同步等待标志 */
static bool cloud_sync_wait_param_flag = false;

/* 云同步发送标志 */
static bool cloud_sync_push_param_flag = false;

/* 云同步参数文件头标志 */
static bool cloud_sync_save_header_flag = false;

/* 云同步参数存储标志 */
static bool cloud_sync_save_param_flag = false;

/* 云同步方式（0：单机，1：空间） */
static uint8_t cloud_sync_save_chl = 0;

/* ============================== 文件内引用外部接口 ============================= */

/**
 * @brief 从文件的指定偏移处读取指定长度的数据。
 */
extern int fs_file_read_at(const char *path, uint32_t offset, uint8_t *data, int size);

/**
 * @brief 将数据写入文件的指定偏移处。
 */
extern int fs_file_write_at(const char *path, uint32_t offset, const uint8_t *data, int size);

/**
 * @brief 数据地址查询函数。
 */
extern const uint16_t* vLookupDataTab(uint8_t SlaveAddr,uint16_t iReadAddr, uint16_t iReadNum, bool is_write, reg_position_t* reg_position, channel_modbus chl);

/**
  * @brief 根据设备类型转换对应类型号码
  */
extern uint16_t SN_TYPE_ASCII_TO_NUM(const char *type);

/**
  * @brief CAN转发接口
  */
extern void sys_new_can_data_resend(can_cmd_queue_struct *can_cmd, uint8_t modbus_slave_address);

/* ============================== MQTT TLV辅助处理接口 ============================= */

/**
 * @brief 根据 data_field 字符串查找 modbus_beta_field_table 中的索引
 * @param data_field 要查找的字段名称字符串（如 "SetCtrlWorkMode"）
 * @return 找到的索引（0 ~ modbus_beta_field_table_count-1），未找到返回 UINT16_MAX
 */
static uint16_t cloud_sync_find_field_index(const char *data_field)
{
    if (data_field == NULL) {
        ESP_LOGE(TAG, "field_param_common_handle : Invalid Field");
        return UINT16_MAX;
    }

    for (uint32_t i = 0; i < modbus_beta_field_table_count; i++) {
        if (strcmp(modbus_beta_field_table[i].data_field, data_field) == 0) {
            ESP_LOGD(TAG, "field_param_common_handle : Field: %s", data_field);
            return i;  // 找到匹配，返回索引
        }
    }
    
    ESP_LOGE(TAG, "field_param_common_handle : Unknown Field: %s", data_field);
    return UINT16_MAX;  // 未找到
}

/**
 * @brief 解析Field参数对象
 */
static bool field_param_common_handle(const char *field, const tlv_type_t *tlv_tag, const void *value_data, uint16_t value_len)
{
    if (( 1 == tlv_tag->data_type ) 
        && ( TLV_LOGIC_EQ == tlv_tag->logic_type )
        && ( TLV_ENCODE_HEX_STRING == tlv_tag->encode_type ))
    {
        uint16_t field_index = cloud_sync_find_field_index(field);
        if ( field_index != UINT16_MAX ) {
            uint8_t *data = (uint8_t *)value_data;
            uint16_t data_index = 0;
            mini_tlv_head_t *mini_tlv_head = NULL;
            uint8_t head_len = sizeof(mini_tlv_head_t);
            
            /* Table 01：小TLV数量     */
            uint8_t tlv_count = 0;
            mini_tlv_head = (mini_tlv_head_t *)&data[data_index];
            data_index += head_len;
            if ( 4 < value_len && mini_tlv_head->table == MINI_TLV_TABLE_SMALL_COUNT ) {
                tlv_count = data[data_index];
                data_index += mini_tlv_head->length;
            } else {
                ESP_LOGE(TAG, "field_param_common_handle : TLV Table 01(%s) is error!", field);
                goto param_handle_exit;
            }

            while ( tlv_count && (data_index < value_len)) {
                /* Table 02：设备SN    */
                mini_tlv_head = (mini_tlv_head_t *)&data[data_index];
                data_index += head_len;
                if ( mini_tlv_head->table == MINI_TLV_TABLE_DEV_SN ) {
                    // TODO：当前不实际处理该项，预留
                    mini_tlv_dev_sn_t *mini_tlv_dev_sn = (mini_tlv_dev_sn_t *)&data[data_index];
                    data_index += mini_tlv_head->length;
                } else {
                    ESP_LOGE(TAG, "field_param_common_handle : TLV Table 02(%s) is error!", field);
                    goto param_handle_exit;
                }
                
                /* Table 03：时间戳     */
                uint32_t timestamp = 0;
                mini_tlv_head = (mini_tlv_head_t *)&data[data_index];
                data_index += head_len;
                if ( mini_tlv_head->table == MINI_TLV_TABLE_TIMESTAMP ) {
                    mini_tlv_timestamp_t *mini_tlv_timestamp = (mini_tlv_timestamp_t *)&data[data_index];
                    timestamp = mini_tlv_timestamp->timestamp;
                    data_index += mini_tlv_head->length;
                } else {
                    ESP_LOGE(TAG, "field_param_common_handle : TLV Table 03(%s) is error!", field);
                    goto param_handle_exit;
                }

                /* Table 250：modbus beta */
                mini_tlv_head = (mini_tlv_head_t *)&data[data_index];
                data_index += head_len;
                if ( mini_tlv_head->table == MINI_TLV_TABLE_MODBUS_BETA ) {
                    if ( timestamp > modbus_param_desc[field_index].timestamp) {
                        // 新参数，需要同步到本地
                        mini_tlv_modbus_beta_t *mini_tlv_modbus_beta = (mini_tlv_modbus_beta_t *)&data[data_index];

                        // 从机地址
                        uint8_t SlaveAddress = MD_SUMMARY_ADDR;
#ifdef CONFIG_CLOUD_SYNC_SLAVE_ADDR_ENABLE        
                        SlaveAddress = modbus_param_desc[field_index].slave_addr;
#endif
                        // ========== 调用 vLookupDataTab 获取数据地址 ==========
                        reg_position_t reg_position = {0};
                        uint16_t *p_tab = vLookupDataTab( SlaveAddress, mini_tlv_modbus_beta->reg_addr, mini_tlv_modbus_beta->reg_number, true, &reg_position, MD_CHL_CLOUD_SYNC);
                        if ( p_tab ) {
                            /* 将 TLV 的 Value 区按寄存器写入本地表 */
                            for ( uint16_t i = 0; i < mini_tlv_modbus_beta->reg_number; i++) {
                                *(p_tab + i) = ((uint16_t *)mini_tlv_modbus_beta->reg_value)[i];
                            }   
#ifdef CONFIG_CAN_PORT_ENABLE
                            /* 为modbus转can指令申请内存 */
                            can_cmd_queue_struct can_cmd = {NULL, 0, 0};
                            can_cmd.num = (mini_tlv_modbus_beta->reg_number > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : mini_tlv_modbus_beta->reg_number;
                            can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
                            if (can_cmd.cmd) {
                                Modbus_To_CAN_Write_Info_After_Process(mini_tlv_modbus_beta->reg_addr, mini_tlv_modbus_beta->reg_number, can_cmd.cmd, &can_cmd.num);
                                sys_new_can_data_resend(&can_cmd, SlaveAddress);  
                            } else {
                                ESP_LOGE(TAG, "field_param_common_handle: can sync alloc failed");
                            }
#endif
                            /*无线透传设置*/
                            Modbus_To_Wireless_Write_Info_After_Process(mini_tlv_modbus_beta->reg_addr, mini_tlv_modbus_beta->reg_number, SlaveAddress, MD_CHL_CLOUD_SYNC);
                        
                            /* 查找寄存器对应数据对象，供写后回调使用 */
                            md_data_t *p_data = md_tbl_find(mini_tlv_modbus_beta->reg_addr);
                            if (p_data) {
                                /* 若注册了表回调，则在写入完成后触发回调，完成状态同步 */
                                if (p_data->tbl.tbl_cb) {
                                    tbl_cb_data_t cb_data = {0};
                                    md_priv_data_t priv_data = {0};
                                
                                    cb_data.SlaveAddress    = SlaveAddress;
                                    cb_data.reg_addr        = mini_tlv_modbus_beta->reg_addr;
                                    cb_data.reg_nums        = mini_tlv_modbus_beta->reg_number;
                                    cb_data.reg_addr_offset = reg_position.offset;
                                    cb_data.is_write        = true;
                                    cb_data.cb_chl          = MD_CHL_CLOUD_SYNC;
                                
                                    int cb_rc = p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data);
                                    if (cb_rc != 0) {
                                        ESP_LOGE(TAG, "field_param_common_handle: tbl_cb returned %d for slave=%u reg=%u len=%u", cb_rc,
                                                 (unsigned)SlaveAddress, (unsigned)mini_tlv_modbus_beta->reg_addr, (unsigned)mini_tlv_modbus_beta->reg_number);
                                    }

                                    ESP_LOGI(TAG, "field_param_common_handle: Updata psram(%s) from Cloud(%lu) to local(%lu) (slave=%u reg=%u len=%u)", 
                                                field, timestamp, modbus_param_desc[field_index].timestamp, (unsigned)SlaveAddress, 
                                                (unsigned)mini_tlv_modbus_beta->reg_addr, (unsigned)mini_tlv_modbus_beta->reg_number);
                                    modbus_param_desc[field_index].timestamp = timestamp;
                                    cloud_sync_save_param_flag |= true;
                                }
                            } else {
                                ESP_LOGE(TAG, "field_param_common_handle: md_tbl_find failed slave=%u reg=%u len=%u",
                                         (unsigned)SlaveAddress, (unsigned)mini_tlv_modbus_beta->reg_addr, (unsigned)mini_tlv_modbus_beta->reg_number);
                            }
                        } else {
                            ESP_LOGE(TAG, "field_param_common_handle: no write table for slave=%u reg=%u len=%u",
                                     (unsigned)SlaveAddress, (unsigned)mini_tlv_modbus_beta->reg_addr, (unsigned)mini_tlv_modbus_beta->reg_number);
                        }
                    } else if ( timestamp < modbus_param_desc[field_index].timestamp ) {
                        // 旧参数，需要更新到云端
                        cloud_sync_ext_param_desc[field_index].is_need_update = true;
                        ESP_LOGI(TAG, "field_param_common_handle: Updata psram(%s) from local(%lu) to Cloud(%lu)", 
                                    field, modbus_param_desc[field_index].timestamp, timestamp);
                    } else {
                        // 相同参数，不做处理
                        ESP_LOGI(TAG, "field_param_common_handle : Parameter(%s) are already newest(%lu)!", field, timestamp);
                    }
                    data_index += mini_tlv_head->length;
                } else {
                    ESP_LOGE(TAG, "field_param_common_handle : TLV Table 250(%s) is error!", field);
                    goto param_handle_exit;
                }
                
                tlv_count--;
            }

            if ( 1 == cloud_sync_save_chl ) {
                modbus_param_desc_header.mesh_utc_timestamp = time(NULL);
            } else {
                modbus_param_desc_header.sn_utc_timestamp = time(NULL);
            }

            cloud_sync_save_header_flag |= true;
            cloud_sync_wait_param_flag = false;
            return true;
        }
    }

param_handle_exit:

    return false;
}

// 云边参数同步处理请求
const field_handler_t device_cfg_set_field_handler[] = {
    {FIELD_setCtrlWorkMode,         field_param_common_handle},
    {FIELD_setCapacity,             field_param_common_handle},
    {FIELD_setCtrlFeedback,         field_param_common_handle},
    {FIELD_setGridMaxCurrent,       field_param_common_handle},
    {FIELD_setFdMaxCurrent,         field_param_common_handle},
    {FIELD_emsCtrl,                 field_param_common_handle},
    {FIELD_stormModeCloudCtrl,      field_param_common_handle},
    {FIELD_acEmsCoordinationMode,   field_param_common_handle},
    {FIELD_tou1,                    field_param_common_handle},
    {FIELD_tou2,                    field_param_common_handle},
    {FIELD_tou3,                    field_param_common_handle},
    {FIELD_ifThen,                  field_param_common_handle},
#if 0   // 待定
    {FIELD_bluettiDevList,          NULL},
    {FIELD_thirdDevList,            NULL},
#endif    
    {NULL,                          NULL}  // 结束标记
};

/* ================================ 模块函数定义 ================================ */

/* 简单封装：获取/释放互斥 */
static inline BaseType_t CloudSyncTakeMutex(TickType_t timeout_ms)
{
    if (!xCloudSyncMutex) return pdFALSE;
    return xSemaphoreTake(xCloudSyncMutex, pdMS_TO_TICKS(timeout_ms));
}

static inline void CloudSyncGiveMutex(void)
{
    if (xCloudSyncMutex) xSemaphoreGive(xCloudSyncMutex);
}

/**
 * @brief 云边参数同步模块初始化
 * @return true 成功；false 创建失败
 */
bool CloudSyncInit(void)
{
    char path[30] = {0};

    // 文件操作互斥信号量
    if (xCloudSyncMutex == NULL) {
        xCloudSyncMutex = xSemaphoreCreateMutex();
        if (xCloudSyncMutex == NULL) {
            ESP_LOGE(TAG, "CloudSyncInit: create mutex failed");
            return false;
        }
    }
    
    // 文件记录更新队列
    if (xCloudSyncQueue == NULL) {
        xCloudSyncQueue = xQueueCreate(modbus_beta_field_table_count, sizeof(cloud_sync_queue_item_t));
        if (xCloudSyncQueue == NULL) {
            ESP_LOGE(TAG, "cloud_sync_queue_init: create queue failed");
            return false;
        }
    }

    // 数据文件路径
    CLOUD_SYNC_FILE_PATH_IOT_MAX(path);

    // 尝试读取文件
    int bytes_read_header = fs_file_read_at(path, 0, (uint8_t*)&modbus_param_desc_header, MODBUS_PARAM_DESC_HEADER_LEN);
    int bytes_read_data = fs_file_read_at(path, CLOUD_SYNC_FILE_DATA_ADDR, (uint8_t*)&modbus_param_desc, sizeof(modbus_param_desc));

    // 检查读取是否成功，以及文件头内容是否有效
    if (bytes_read_header != MODBUS_PARAM_DESC_HEADER_LEN 
        || bytes_read_data != sizeof(modbus_param_desc)
        || modbus_param_desc_header.ver != MODBUS_PARAM_DESC_FILE_VER_V1
        || modbus_param_desc_header.unit_max_cnt == 0 
        || modbus_param_desc_header.unit_bytes != sizeof(modbus_param_desc_t)) {
        ESP_LOGW(TAG, "Cloud sync file read failed or is invalid. Initializing a new one.");

        // 初始化文件头
        memset(&modbus_param_desc_header, 0, sizeof(modbus_param_desc_header_t));
        modbus_param_desc_header.ver = MODBUS_PARAM_DESC_FILE_VER_V1;
        modbus_param_desc_header.unit_bytes = sizeof(modbus_param_desc_t);
        modbus_param_desc_header.unit_max_cnt = MODBUS_PARAM_DESC_OBJECT_MAX;

        // 初始化文件
        memset(&modbus_param_desc, 0, sizeof(modbus_param_desc));
        for ( uint16_t i = 0 ; i < modbus_param_desc_header.unit_max_cnt && i < modbus_beta_field_table_count; i++ )
        {
#ifdef CONFIG_CLOUD_SYNC_SLAVE_ADDR_ENABLE        
            modbus_param_desc[i].slave_addr = modbus_beta_field_table[i].slaveaddr;
#endif
            modbus_param_desc[i].len.bits.length_type = 0;  // 取字节数量
            modbus_param_desc[i].len.bits.protocol_ver = 0; // modbus-beta
            modbus_param_desc[i].start_addr = modbus_beta_field_table[i].start_reg_addr;
            modbus_param_desc[i].len.bits.length_value = modbus_beta_field_table[i].data_reg_cnt * 2;
            modbus_param_desc_header.unit_real_cnt++;
        }

        // 写入新的文件头
        if (fs_file_write_at(path, CLOUD_SYNC_FILE_HEADER_ADDR, (uint8_t*)&modbus_param_desc_header, MODBUS_PARAM_DESC_HEADER_LEN) == MODBUS_PARAM_DESC_HEADER_LEN) {
            // 更新文件
            if (fs_file_write_at(path, CLOUD_SYNC_FILE_DATA_ADDR, (uint8_t*)modbus_param_desc, sizeof(modbus_param_desc)) == sizeof(modbus_param_desc)) {
                ESP_LOGI(TAG, "New cloud sync file initialized and written successfully(%d/%d).", 
                    modbus_param_desc_header.unit_real_cnt, modbus_param_desc_header.unit_max_cnt);
                return true;
            } else {
                ESP_LOGE(TAG, "FATAL: Failed to write cloud sync file!");
                return false;
            }
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write cloud sync file header!");
            return false;
        }
    } 

    // 检查是否有新增成员
    if (( modbus_param_desc_header.unit_real_cnt < modbus_beta_field_table_count ) 
        && ( modbus_param_desc_header.unit_real_cnt < modbus_param_desc_header.unit_max_cnt )) 
    {
        for ( uint16_t i = modbus_param_desc_header.unit_real_cnt ; i < modbus_param_desc_header.unit_max_cnt && i < modbus_beta_field_table_count; i++ )
        {
#ifdef CONFIG_CLOUD_SYNC_SLAVE_ADDR_ENABLE        
            modbus_param_desc[i].slave_addr = modbus_beta_field_table[i].slaveaddr;
#endif
            modbus_param_desc[i].len.bits.length_type = 0;  // 取字节数量
            modbus_param_desc[i].len.bits.protocol_ver = 0; // modbus-beta
            modbus_param_desc[i].start_addr = modbus_beta_field_table[i].start_reg_addr;
            modbus_param_desc[i].len.bits.length_value = modbus_beta_field_table[i].data_reg_cnt * 2;
            modbus_param_desc_header.unit_real_cnt++;
        }

        // 写入新的文件头
        if (fs_file_write_at(path, CLOUD_SYNC_FILE_HEADER_ADDR, (uint8_t*)&modbus_param_desc_header, MODBUS_PARAM_DESC_HEADER_LEN) == MODBUS_PARAM_DESC_HEADER_LEN) {
            // 更新文件
            if (fs_file_write_at(path, CLOUD_SYNC_FILE_DATA_ADDR, (uint8_t*)modbus_param_desc, sizeof(modbus_param_desc)) == sizeof(modbus_param_desc)) {
                ESP_LOGI(TAG, "New cloud sync file added and written successfully(%d/%d).", 
                    modbus_param_desc_header.unit_real_cnt, modbus_param_desc_header.unit_max_cnt);
                return true;
            } else {
                ESP_LOGE(TAG, "FATAL: Failed to write cloud sync file!");
                return false;
            }
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write cloud sync file header!");
            return false;
        }
    }

    ESP_LOGI(TAG, "Cloud sync file load successfully(%d/%d).", 
        modbus_param_desc_header.unit_real_cnt, modbus_param_desc_header.unit_max_cnt);

#if 0   // 调试打印
    ESP_LOGI(TAG, "/***************************Cloud sync list****************************/");
    for (uint32_t i = 0; i < modbus_param_desc_header.unit_real_cnt; i++) {
        ESP_LOGI(TAG, "Param[%s] : start_addr(%d), length_value(%d), timestamp(%lu)",
                 modbus_beta_field_table[i].data_field, modbus_param_desc[i].start_addr, 
                 modbus_param_desc[i].len.bits.length_value, modbus_param_desc[i].timestamp);
    }
    ESP_LOGI(TAG, "/*********************************************************************/");
#endif

    return true;
}

/**
 * @brief 检查并更新云同步参数的时间戳
 *
 * 当 Modbus 数据更新时，检查该更新是否影响云同步参数列表中的某个参数项。
 * 如果从机地址匹配且寄存器地址范围有交集，则更新该参数的时间戳。
 *
 * @param slave_addr  Modbus 从机地址
 * @param reg_addr    寄存器起始地址
 * @param reg_cnt     寄存器个数
 * @return true       存在交集并更新了时间戳
 * @return false      不存在交集，或参数无效
 */
static bool cloud_sync_update_param_timestamp(uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_cnt)
{
    // 参数有效性检查
    if (reg_cnt == 0) {
        return false;
    }

    // 检查输入范围是否溢出
    if (reg_addr > UINT16_MAX - reg_cnt + 1) {
        return false;  // 范围溢出
    }

    ESP_LOGD(TAG, "cloud_sync_update_param_timestamp : slave_addr(%d), reg_addr(%d), reg_cnt(%d)", slave_addr, reg_addr, reg_cnt);

    bool updated = false;
    uint16_t input_end = reg_addr + reg_cnt;

    // 遍历所有有效参数项
    for (uint32_t i = 0; i < modbus_param_desc_header.unit_real_cnt; i++) {
#ifdef CONFIG_CLOUD_SYNC_SLAVE_ADDR_ENABLE
        // 检查从机地址是否匹配
        if (modbus_param_desc[i].slave_addr != slave_addr) {
            continue;
        }
#endif

        // 计算参数项的寄存器范围 [start, end)
        uint16_t param_start = modbus_param_desc[i].start_addr;
        uint16_t param_reg_cnt = modbus_param_desc[i].len.bits.length_value / 2;

        // 检查参数项的寄存器个数是否有效
        if (param_reg_cnt == 0 || param_start > UINT16_MAX - param_reg_cnt + 1) {
            continue;  // 无效的参数项，跳过
        }

        uint16_t param_end = param_start + param_reg_cnt;

        // 检查两个范围是否有交集
        // 范围 [a1, a2) 和 [b1, b2) 有交集的条件：a1 < b2 && b1 < a2
        bool has_overlap = (param_start < input_end) && (reg_addr < param_end);

        if (has_overlap) {
            // 更新时间戳（当前 UTC 时间）
            modbus_param_desc[i].timestamp = (uint32_t)time(NULL);
            updated = true;

            ESP_LOGI(TAG, "Updated param[%u] to local : %s, timestamp: slave=%u, reg=[%u-%u)",
                     i, modbus_beta_field_table[i].data_field, slave_addr, param_start, param_end);
            break;
        }
    }

    return updated;
}

/**
 * @brief 向云同步更新队列压入一个参数更新请求（非阻塞）
 * @param slave_addr  Modbus 从机地址
 * @param reg_addr    寄存器起始地址
 * @param reg_cnt     寄存器个数
 * @return true 压入成功；false 队列满或队列未初始化
 */
bool cloud_sync_queue_push(uint8_t slave_addr, uint16_t reg_addr, uint16_t reg_cnt)
{
    if (xCloudSyncQueue == NULL) {
        return false;
    }

    cloud_sync_queue_item_t item = {
        .slave_addr = slave_addr,
        .reg_addr   = reg_addr,
        .reg_cnt    = reg_cnt,
    };

    // 非阻塞压入：队列满时丢弃，避免阻塞调用者
    if (xQueueSend(xCloudSyncQueue, &item, 0) != pdPASS) {
        ESP_LOGW(TAG, "cloud_sync_queue_push: queue full, item dropped");
        return false;
    }
    
    ESP_LOGD(TAG, "cloud_sync_queue_push : slave_addr(%d), reg_addr(%d), reg_cnt(%d)", slave_addr, reg_addr, reg_cnt);
    return true;
}

/**
 * @brief 基于时间戳对比的参数更新推送
 * @param mesh_id Mesh ID (6字节)，全0表示无效
 * @param dev_type 设备类型字符串指针
 * @param dev_sn 设备序列号（用于SN校验）
 * @return true 推送成功；false 失败
 */
static bool cloud_sync_push_param_by_timestamp(uint8_t mesh_id[6], char *dev_type_ascii, uint64_t dev_sn)
{
    bool updated = false;
    bool use_mesh = false;
    uint16_t update_count = 0;
    uint16_t dev_type = 0;
    
    // ========== 1. 确定主时间戳 ==========
    uint32_t *main_timestamp = NULL;
    if (mesh_id != NULL && memcmp(mesh_id, "\0\0\0\0\0\0", 6) != 0) {
        // mesh_id 有效，使用 mesh_utc_timestamp
        main_timestamp = &modbus_param_desc_header.mesh_utc_timestamp;
        use_mesh = true;
    } else if (dev_type_ascii != NULL && dev_sn != 0 && dev_sn != MASS_PRODUCTION_DEFAULT_DEV_SN){
        // 使用 sn_utc_timestamp
        main_timestamp = &modbus_param_desc_header.sn_utc_timestamp;
        dev_type = SN_TYPE_ASCII_TO_NUM(dev_type_ascii);
        use_mesh = false;
    } else {
        ESP_LOGE(TAG, "cloud_sync_push_param_by_timestamp : meshid and sn error!");
        goto push_exit;
    }

    // ========== 2. 遍历所有参数项，筛选需要更新的 ==========
    for (uint32_t i = 0; i < modbus_param_desc_header.unit_real_cnt; i++) {
        
        // 跳过时间戳 <= 主时间戳的参数
        if (modbus_param_desc[i].timestamp <= *main_timestamp) {
            // 已置位更新标志，也更新计数
            if( cloud_sync_ext_param_desc[i].is_need_update ) update_count++;
            continue;
        }

        // 更新标志
        cloud_sync_ext_param_desc[i].is_need_update = true;
        update_count++;
    }     

    if( !update_count ) goto push_exit;

    // ========== 3. 申请内存待发送对象表数据 ==========
    mqtt_tlfv_config_field_t *object_table = (mqtt_tlfv_config_field_t *)iot_calloc(sizeof(mqtt_tlfv_config_field_t) * update_count);
    if ( !object_table ) {
        ESP_LOGE(TAG, "cloud_sync_push_param_by_timestamp : object_table calloc failed");
        goto push_exit;
    }
    
    uint16_t table_index = 0;
    for (uint32_t i = 0; i < modbus_param_desc_header.unit_real_cnt; i++) {
        // 检查上报标志
        if ( cloud_sync_ext_param_desc[i].is_need_update ) {
            cloud_sync_ext_param_desc[i].is_need_update = false;
            object_table[table_index].data_field = modbus_beta_field_table[i].data_field;

            // 判断字段处理类型
            if ( true == modbus_beta_field_table[i].is_user_defined ) {
                // TODO：特殊类型需要根据SN自识别地址，允许存在多个TLV块，暂未实现
                table_index++;
                continue;
            } else {
                // 仅允许单个TLV块
                uint8_t head_len = sizeof(mini_tlv_head_t);
                object_table[table_index].data_len = (head_len * 4) + 1 + 11 + 4 + (4 + modbus_param_desc[i].len.bits.length_value);

                // ========== 3.1 申请内存小块TLV数据 ==========
                object_table[table_index].data_addr = (uint8_t *)iot_calloc(object_table[table_index].data_len);
                if ( !object_table[table_index].data_addr ) {
                    ESP_LOGE(TAG, "cloud_sync_push_param_by_timestamp : param [%s] calloc failed", modbus_beta_field_table[i].data_field);
                    object_table[table_index].data_len = 0;
                    table_index++;
                    continue;
                }

                uint16_t len = 0;
                mini_tlv_head_t *mini_tlv_head = NULL;
                
                /* Table 01：小TLV数量     */
                mini_tlv_head = (mini_tlv_head_t *)&object_table[table_index].data_addr[len];
                mini_tlv_head->table = MINI_TLV_TABLE_SMALL_COUNT;
                mini_tlv_head->length = 1;
                len += head_len;
                
                object_table[table_index].data_addr[len++] = 1; // Value

                /* Table 02：设备SN    */
                mini_tlv_dev_sn_t *mini_tlv_dev_sn = NULL;
                mini_tlv_head = (mini_tlv_head_t *)&object_table[table_index].data_addr[len];
                mini_tlv_head->table = MINI_TLV_TABLE_DEV_SN;
                mini_tlv_head->length = 11; // 当前分类默认我司设备，第三方设备参考“0xFF == modbus_param_desc[i].slave_addr”分支后期实现
                len += head_len;
                
                mini_tlv_dev_sn = (mini_tlv_dev_sn_t *)&object_table[table_index].data_addr[len]; //Value
                if ( use_mesh ) {
                    mini_tlv_dev_sn->vendor_type = DEV_VENDOR_MESH_ID;
                    memcpy(mini_tlv_dev_sn->dev.bluetti.sn.raw, mesh_id, 6);
                } else {
                    mini_tlv_dev_sn->vendor_type = DEV_VENDOR_BLUETTI;
                    mini_tlv_dev_sn->dev.bluetti.sn.sn_src = dev_sn;
                    mini_tlv_dev_sn->dev.bluetti.sn.model = dev_type;
                }
                len += mini_tlv_head->length;

                /* Table 03：时间戳     */
                mini_tlv_timestamp_t *mini_tlv_timestamp = NULL;
                mini_tlv_head = (mini_tlv_head_t *)&object_table[table_index].data_addr[len];
                mini_tlv_head->table = MINI_TLV_TABLE_TIMESTAMP;
                mini_tlv_head->length = 4;
                len += head_len;

                mini_tlv_timestamp = (mini_tlv_timestamp_t *)&object_table[table_index].data_addr[len]; //Value
                mini_tlv_timestamp->timestamp = modbus_param_desc[i].timestamp;
                len += mini_tlv_head->length;

                /* Table 250：modbus beta */
                mini_tlv_modbus_beta_t *mini_tlv_modbus_beta = NULL;
                mini_tlv_head = (mini_tlv_head_t *)&object_table[table_index].data_addr[len];
                mini_tlv_head->table = MINI_TLV_TABLE_MODBUS_BETA;
                mini_tlv_head->length = 4 + modbus_param_desc[i].len.bits.length_value;
                len += head_len;

                mini_tlv_modbus_beta = (mini_tlv_modbus_beta_t *)&object_table[table_index].data_addr[len]; //Value
                mini_tlv_modbus_beta->reg_addr = modbus_param_desc[i].start_addr;

                uint8_t SlaveAddress = MD_SUMMARY_ADDR;
#ifdef CONFIG_CLOUD_SYNC_SLAVE_ADDR_ENABLE        
                SlaveAddress = modbus_param_desc[i].slave_addr;
#endif
                // ========== 4. 调用 vLookupDataTab 读取数据 ==========
                uint16_t *p_tab = vLookupDataTab( SlaveAddress, modbus_param_desc[i].start_addr, modbus_param_desc[i].len.bits.length_value / 2, false, NULL, MD_CHL_SELF);
                if ( p_tab ) {
                    mini_tlv_modbus_beta->reg_number = modbus_param_desc[i].len.bits.length_value / 2;
                    memcpy(mini_tlv_modbus_beta->reg_value, p_tab, modbus_param_desc[i].len.bits.length_value);
                } else {
                    mini_tlv_modbus_beta->reg_number = 0;
                    mini_tlv_head->length = 4;
                }
                
                len += mini_tlv_head->length;
                
                if ( object_table[table_index].data_len < len) {
                    // 非法检查，理论上不可触发
                    ESP_LOGE(TAG, "cloud_sync_push_param_by_timestamp : ERROR!!!!!!!!! (calloc_len=%d, real_len=%d)", object_table[table_index].data_len, len);
                } else {
                    object_table[table_index].data_len = len;
                }
                
                ESP_LOGI(TAG, "Updating param[%u] to Cloud: %s, SlaveAddress: %u, reg_addr: %u, cnt: %u",
                         table_index, modbus_beta_field_table[i].data_field, SlaveAddress, mini_tlv_modbus_beta->reg_addr, mini_tlv_modbus_beta->reg_number);
                
                table_index++;
            }
        }
    }

    // ========== 5. 组帧并发送 ==========
    int send_ret = -1;
    uint16_t buff_size = 4096;
    uint8_t *send_buffer = (uint8_t *) iot_calloc(buff_size);
    if ( send_buffer ) {
        if (use_mesh) {
            // 使用 mesh_id 发送
            send_ret = mqtt_tlv_request_device_config_set(mesh_id, NULL, object_table, update_count, send_buffer, buff_size);
        } else {
            // 使用 dev_type 和 dev_sn 发送
            char dev_name[32] = {0};
            snprintf((char *)dev_name, sizeof(dev_name), "%s%llu", dev_type_ascii, dev_sn);
            send_ret = mqtt_tlv_request_device_config_set(NULL, dev_name, object_table, update_count, send_buffer, buff_size);
        }

        if(0 != send_ret) {
            iot_free(send_buffer);
        } else {
            // 更新最新同步时间
            *main_timestamp = time(NULL);
            updated = true;
        }
    }
   
    // ========== 6. 释放内存 ==========
    if ( object_table ) {
        for (uint32_t i = 0; i < update_count; i++) {
            iot_free(object_table[i].data_addr);
        }

        iot_free(object_table);
    }
    
push_exit:

    return updated;
}

/**
 * @brief 请求云端最新参数更新推送
 * @param mesh_id Mesh ID (6字节)，全0表示无效
 * @param dev_type 设备类型字符串指针
 * @param dev_sn 设备序列号（用于SN校验）
 * @return true 请求成功；false 失败
 */
static bool cloud_sync_get_param(uint8_t mesh_id[6], char *dev_type_ascii, uint64_t dev_sn)
{
    bool updated = false;
    bool use_mesh = false;
    uint16_t dev_type = 0;
    
    // ========== 1. 确定请求目标 ==========
    if (mesh_id != NULL && memcmp(mesh_id, "\0\0\0\0\0\0", 6) != 0) {
        use_mesh = true;
        cloud_sync_save_chl = 1;
    } else if (dev_type_ascii != NULL && dev_sn != 0 && dev_sn != MASS_PRODUCTION_DEFAULT_DEV_SN){
        dev_type = SN_TYPE_ASCII_TO_NUM(dev_type_ascii);
        use_mesh = false;
        cloud_sync_save_chl = 0;
    } else {
        ESP_LOGE(TAG, "cloud_sync_get_param : meshid and sn error!");
        goto get_exit;
    }

    // ========== 2. 确认更新对象 ==========

    // TODO：当前全量请求，后续可按需求部分请求
    mqtt_tlfv_config_field_t *object_table = NULL;
    uint16_t object_count = 0;

    // ========== 3. 组帧并发送 ==========
    int send_ret = -1;
    uint16_t buff_size = 256;
    uint8_t *send_buffer = (uint8_t *) iot_calloc(buff_size);
    if ( send_buffer ) {
        if (use_mesh) {
            // 使用 mesh_id 发送
            send_ret = mqtt_tlv_request_device_config_get(mesh_id, NULL, object_table, object_count, send_buffer, buff_size);
        } else {
            // 使用 dev_type 和 dev_sn 发送
            char dev_name[32] = {0};
            snprintf((char *)dev_name, sizeof(dev_name), "%s%llu", dev_type_ascii, dev_sn);
            send_ret = mqtt_tlv_request_device_config_get(NULL, dev_name, object_table, object_count, send_buffer, buff_size);
        }

        if(0 != send_ret) {
            iot_free(send_buffer);
        } else {
            updated = true;
        }
    }
   
get_exit:

    return updated;
}

/**
  * @brief      本地参数更新记录存储
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void cloud_sync_record_process(void)
{
    static uint32_t pre_time = 0;
    static uint32_t revd_wait_time = 0;
    static uint8_t retry_count = 0;
    bool header_updated = false;
    bool data_updated = false;
    bool send_flag = false;
    char path[30] = {0};
    char *dev_type_ascii = dev_factory.dev_type;
    uint64_t dev_sn = dev_factory.dev_sn;
    
    // 获取互斥锁保护共享数据
    if (CloudSyncTakeMutex(100) == pdFALSE) {
        return;  // 获取锁失败
    }

    // 循环取出队列中的所有项，直到队列为空
    cloud_sync_queue_item_t item;
    while (xCloudSyncQueue && xQueueReceive(xCloudSyncQueue, &item, 0) == pdPASS) {
        data_updated |= cloud_sync_update_param_timestamp(item.slave_addr, item.reg_addr, item.reg_cnt);

        // 记录更新后延迟一段时间执行上报，确保数据回读完成
        if( data_updated ) revd_wait_time = 30000; // 3s
    }
    
    // 延时等待接收机制
    if ( cloud_sync_wait_param_flag || revd_wait_time ) {
        uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if( 0 == pre_time ) pre_time = now_time;
        if( 0 == revd_wait_time ) revd_wait_time = 10000;   // 10s
        if ((now_time - pre_time) >= revd_wait_time) {
            pre_time = 0;
            revd_wait_time = 0;

            // 空间场景增加有限次重试机制
            if ( cloud_sync_wait_param_flag 
                && 1 == cloud_sync_save_chl 
                && ++retry_count < 3) {
                cloud_sync_wait_param_flag = false;
                cloud_sync_revd_param_flag = true;
            } else {
                cloud_sync_wait_param_flag = false;
                retry_count = 0;
            }
        } 
    } else {
        if ( cloud_sync_revd_param_flag ) {
            cloud_sync_revd_param_flag = false;
        
            // 向云端请求最新参数
            send_flag |= cloud_sync_get_param((uint8_t *)IotSetData.dev_info_t.WIFI_Mesh_ID, 
                                        dev_type_ascii, dev_sn);
        
            // 请求成功，延时等待接收后存储本地
            if ( send_flag ) {
                cloud_sync_wait_param_flag = true;
                revd_wait_time = 10000;   // 10s
            }
        } else if ( cloud_sync_push_param_flag ) {
            cloud_sync_push_param_flag = false;
        
            // 更新本地参数至云端
            send_flag |= cloud_sync_push_param_by_timestamp((uint8_t *)IotSetData.dev_info_t.WIFI_Mesh_ID, 
                                                                    dev_type_ascii, dev_sn);
        
            // 发送成功，记录更新时间
            if ( send_flag ) cloud_sync_save_header_flag = true;                                                          
        }

        // 更新存储标志
        if ( cloud_sync_save_header_flag ) {
            cloud_sync_save_header_flag = false;
            header_updated = true;
        }

        // 更新存储标志
        if ( cloud_sync_save_param_flag ) {
            cloud_sync_save_param_flag = false;
            data_updated = true;
        }
    }
    
    // 写入新的文件头
    if ( header_updated ) {
        CLOUD_SYNC_FILE_PATH_IOT_MAX(path);
        if (fs_file_write_at(path, CLOUD_SYNC_FILE_HEADER_ADDR, (uint8_t*)&modbus_param_desc_header, MODBUS_PARAM_DESC_HEADER_LEN) == MODBUS_PARAM_DESC_HEADER_LEN) {
            ESP_LOGI(TAG, "modbus_param_desc_header written successfully.");
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write cloud sync file header!");
        }
    }
    
    // 参数存储逻辑
    if ( data_updated ) {
        CLOUD_SYNC_FILE_PATH_IOT_MAX(path);
        if (fs_file_write_at(path, CLOUD_SYNC_FILE_DATA_ADDR, (uint8_t*)modbus_param_desc, sizeof(modbus_param_desc)) == sizeof(modbus_param_desc)) {
            ESP_LOGI(TAG, "modbus_param_desc written successfully.");
        } else {
            ESP_LOGE(TAG, "FATAL: Failed to write cloud sync file!");
        }
    }
    
    // 释放互斥锁
    CloudSyncGiveMutex();
}

/**
 * @brief 请求云端最新参数更新推送命令
 */
void cloud_sync_revd_param_cmd(bool cmd) {
    cloud_sync_revd_param_flag = cmd;
}

/**
 * @brief 基于时间戳对比的参数更新推送命令
 */
void cloud_sync_push_param_cmd(bool cmd) {
    cloud_sync_push_param_flag = cmd;
}

#endif

