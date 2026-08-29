/**
  ******************************************************************************
  * @file      modbus_tlv.c
  * @version   1.0
  * @author    lixingyu
  * @date      2026/4/15
  * @brief     Modbus TLV寄存器组合（借用21000段框架）
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2026/4/15  <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/*
https://192.168.102.10/svn/TecCenter/文件中心/技术资料/德兰明海总线协议/无线设备间协议/基于uart ble 网络等通讯介质的设备信息绑定帧通讯协议-20260420.xlsx
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
#include "esp_err.h"
#include "esp_check.h"
#include "sdkconfig.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "modbus_tlv.h"
#include "modbus_data.h"
#include "modbus_slave.h"
#include "modbus_protocol.h"
#include "modbus_define.h"
#include "comm_define.h"

/* ================================ 文件内宏定义 ================================ */

#ifdef CONFIG_MODBUS_REG_TLV_ENABLE

#define TAG "[MODBUS_TLV]"

// 高低字节转换
#define U16_TO_BE(val)   (uint16_t)((((val) & 0xFF00) >> 8) | (((val) & 0x00FF) << 8))

// TLV结构中TL的长度
#define MODBUS_TL_LEN   (offsetof(modbus_tlv_unit_t, value))

// 单次指令中TLV的最大个数
#define MODBUS_TLV_SINGLE_NUM_MAX  ((MODBUS_REG_SINGLE_PROCESS_LEN_MAX * 2) / MODBUS_TL_LEN)

// TLV数据发送Buffer长度
#define MODBUS_TLV_BUFFER_LEN_MAX  ((MODBUS_REG_SINGLE_PROCESS_LEN_MAX * 2) + 9)

// 单次TLV数据响应最大长度
#define MODBUS_TLV_DATA_LEN_MAX    ((MODBUS_REG_SINGLE_PROCESS_LEN_MAX * 2) - 4)

/* =============================== 文件内全局变量 ================================ */

/* 接收队列句柄（入队者需分配 data 并将其所有权转移给队列） */
static QueueHandle_t xQueue_modbus_tlv_rx = NULL;

/* 信号量（互斥） */
static SemaphoreHandle_t xModbusTlvMutex = NULL;

/* TLV数据发送Buffer*/
USE_EXT_RAM_BSS static uint8_t modbus_tlv_tx_buffer[MODBUS_TLV_BUFFER_LEN_MAX] = {0};

/* TLV读取处理完成列表 */
USE_EXT_RAM_BSS static modbus_tlv_unit_t modbus_tlv_read_unit[MODBUS_TLV_SINGLE_NUM_MAX] = {0};

/* ============================== 文件内引用外部接口 ============================= */

#if 0   // 引用其他文件时可选择于该文件内实现，这里不定义
extern uint8_t modbus_data_semaphore_Take(void);
extern void modbus_data_semaphore_Give(void);
#endif

// CAN转发接口
extern void sys_new_can_data_resend(can_cmd_queue_struct *can_cmd, uint8_t modbus_slave_address);

// 蓝牙上报接口
extern void ble_ff03_tx_queue_push(uint8_t *pdata, uint16_t len);

// MQTT上报接口
extern void Mqtt_Active_Report_Queue_Push(uint8_t *pdata, uint16_t len, uint8_t scene, uint16_t cycle, uint8_t total, uint8_t seq);

/* ================================ 模块函数定义 ================================ */

/* 简单封装：获取/释放互斥 */
static inline BaseType_t Modbus_Tlv_TakeMutex(TickType_t timeout_ms)
{
    if (!xModbusTlvMutex) return pdFALSE;
    return xSemaphoreTake(xModbusTlvMutex, pdMS_TO_TICKS(timeout_ms));
}
static inline void Modbus_Tlv_GiveMutex(void)
{
    if (xModbusTlvMutex) xSemaphoreGive(xModbusTlvMutex);
}

/**
 * @brief 底层发送 TLV 数据
 *
 * 根据通道标识将 TLV 数据分发到对应的发送链路。
 *
 * @param[in] data 待发送数据
 * @param[in] len  数据长度
 * @param[in] chl  通道标识
 */
static void modbus_tlv_send(const uint8_t *data, uint16_t len, uint8_t chl)
{
    if ((data == NULL) || (len == 0)) {
        ESP_LOGE(TAG, "modbus_tlv_send: invalid args");
        return;
    }

    ESP_LOGD(TAG, "modbus_tlv_send: len=%u, chl=%u", (unsigned)len, (unsigned)chl);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, len, ESP_LOG_DEBUG);

    switch (chl) {
        case MD_CHL_BLE:
            ble_ff03_tx_queue_push((uint8_t *)data, len);
            break;

        case MD_CHL_WIFI_CLOUD:
            Mqtt_Active_Report_Queue_Push((uint8_t *)data, len, 0x0F, 0, 0, 0);
            break;

        default:
            ESP_LOGE(TAG, "modbus_tlv_send: unsupported chl=%u", (unsigned)chl);
            break;
    }
}

/**
 * @brief 初始化 modbus TLV 模块
 * @param capacity 最大队列长度（0 使用默认32）
 * @return void
 */
void modbus_tlv_init(size_t capacity)
{
    if (xQueue_modbus_tlv_rx != NULL) return;

    if (capacity == 0) capacity = 32;

    // 1. 创建模块队列（队列长度和元素大小可根据实际需求调整）
    xQueue_modbus_tlv_rx = xQueueCreate(capacity, sizeof(queue_msg_tlv_t));
    if (xQueue_modbus_tlv_rx == NULL) {
        ESP_LOGE(TAG, "modbus_tlv_init: xQueueCreate failed (cap=%u)", (unsigned)capacity);
    } else {
        ESP_LOGI(TAG, "modbus_tlv_init: created (cap=%u)", (unsigned)capacity);
    }

    // 2. 创建互斥信号量用于保护配置/运行态数据；
    if (xModbusTlvMutex == NULL) {
        xModbusTlvMutex = xSemaphoreCreateMutex();
        if (xModbusTlvMutex == NULL) {
            ESP_LOGE(TAG, "modbus_tlv_init: create mutex failed");
        }
    }
}

/**
 * @brief 将一条 TLV 消息入队（队列保存 data 指针所有权）
 * @param pdata 消息数据指针（入队前由调用者提供）
 * @param len   数据长度（字节）
 * @param chl   通道标识
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数错误；ESP_ERR_NO_MEM 队列满或分配失败；ESP_ERR_INVALID_STATE 队列未初始化
 */
esp_err_t modbus_tlv_queue_push(const uint8_t *pdata, uint16_t len, uint8_t chl)
{
    if (!pdata || len == 0) return ESP_ERR_INVALID_ARG;
    if (xQueue_modbus_tlv_rx == NULL) return ESP_ERR_INVALID_STATE;

    queue_msg_tlv_t msg;
    msg.data = (uint8_t *)iot_calloc(len);
    if (msg.data == NULL) {
        ESP_LOGE(TAG, "modbus_tlv_queue_push: calloc failed len=%u", (unsigned)len);
        return ESP_ERR_NO_MEM;
    }
    memcpy(msg.data, pdata, len);
    msg.len = len;
    msg.chl = chl;

    if (xQueueSendToBack(xQueue_modbus_tlv_rx, &msg, 0) != pdPASS) {
        iot_free(msg.data);
        ESP_LOGW(TAG, "modbus_tlv_queue_push: queue full");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/**
 * @brief 发送一帧 TLV 读取响应数据
 *
 * 根据当前已经写入到发送缓冲区 payload 区的数据长度，补齐 Modbus 报文头、
 * 当前分包序号，并对 TLV 数据区按 16 位进行高低字节转换（根据需求可选），最后计算 CRC
 * 并通过底层发送接口发出。
 *
 * 报文格式沿用 21000 段 TLV 上报协议：
 * - [0]  目标从机地址
 * - [1]  功能码 0x10
 * - [2:3] 起始寄存器地址
 * - [4:5] 寄存器数量
 * - [6]  后续字节数
 * - [7:8] TLV 功能标识
 * - [9]  总包数
 * - [10] 当前包序号
 * - [11...] TLV 数据区
 * - [末尾2字节] CRC16
 *
 * 注意：
 * - TLV 数据区中的 TL 和 V 都按 16 位字进行高低字节转换；
 * - 当前设计下 TL 长度固定为偶数，V 长度也要求为偶数，因此 payload_len
 *   理论上应始终为偶数
 *
 * @param tx_buffer   发送缓冲区
 * @param payload_len 当前帧 TLV 数据区长度（从 modbus_tlv_tx_buffer[11] 开始）
 * @param total_index 总包数
 * @param curr_index 当前包序号（从 1 开始）
 * @param chl 发送通道
 */
static void modbus_tlv_read_rtn_send_frame(uint8_t *tx_buffer, uint16_t payload_len, uint8_t total_index, uint8_t curr_index, uint8_t chl)
{
    uint16_t crc16 = 0;

    tx_buffer[0]  = MD_SUMMARY_ADDR;
    tx_buffer[1]  = 0x10;
    tx_buffer[2]  = MOD_REG_START_ADDR_21000 >> 8;
    tx_buffer[3]  = MOD_REG_START_ADDR_21000 & 0xFF;
    tx_buffer[4]  = ((payload_len + 4) / 2) >> 8;
    tx_buffer[5]  = ((payload_len + 4) / 2) & 0xFF;
    tx_buffer[6]  = payload_len + 4;
    tx_buffer[7]  = (LSB2MSB(MODBUS_21000_FUNC_TLV_DATA)) & 0xFF;
    tx_buffer[8]  = (LSB2MSB(MODBUS_21000_FUNC_TLV_DATA) >> 8) & 0xFF;
    tx_buffer[9]  = total_index;
    tx_buffer[10] = curr_index;

#if 1   // 按MModbus协议，转换为大端格式
    /* 对 TLV 数据区按 16 位进行高低字节转换 */
    for (uint16_t offset = 11; (offset + 1) < (uint16_t)(11 + payload_len); offset += 2) {
        uint16_t word = 0;
        memcpy(&word, &tx_buffer[offset], sizeof(word));
        word = U16_TO_BE(word);
        memcpy(&tx_buffer[offset], &word, sizeof(word));
    }
#endif

    crc16 = ModbusCrc16(tx_buffer, payload_len + 11);
    tx_buffer[payload_len + 11] = crc16 & 0xFF;
    tx_buffer[payload_len + 12] = (crc16 >> 8) & 0xFF;

    modbus_tlv_send(tx_buffer, payload_len + 13, chl);
}

/**
 * @brief 处理单个 Modbus TL 读取单元并完成读前状态同步
 *
 * 该函数根据 TL 单元中的从机地址、寄存器地址和字节长度，
 * 查找对应的数据表地址，并在需要时调用表回调函数完成读前同步。
 *
 * 处理流程：
 * 1. 检查输入参数合法性；
 * 2. 根据字节长度换算寄存器数量；
 * 3. 调用 vLookupDataTab() 获取目标寄存器数据表地址；
 * 4. 调用 md_tbl_find() 获取从机表对象；
 * 5. 若从机注册了 tbl_cb，则调用回调同步数据状态。
 *
 * @param[out] p_tab   返回查找到的寄存器表地址
 * @param[in]  tl_data 指向 TL 读取单元
 * @param[in]  chl     当前访问通道
 *
 * @return >0  伪装字节长度（部分正常处理）
 * @return 0   正常处理成功
 * @return -1  输入参数非法
 * @return -2  查找数据表失败
 * @return -3  查找从机对象失败
 * @return -4  表回调执行失败
 */
static int modbus_tlv_process_read_unit(uint16_t **p_tab, const modbus_tlv_unit_t *tl_data, uint8_t chl)
{
    if ((p_tab == NULL) || (tl_data == NULL)) {
        return -1;
    }

    *p_tab = NULL;

    if ((tl_data->len == 0) || ((tl_data->len % 2) != 0)) {
        ESP_LOGE(TAG, "modbus_tlv: invalid len=%u", (unsigned)tl_data->len);
        return -1;
    }

    uint16_t reg_nums = tl_data->len / 2;
    uint16_t reg_real_num = 0;//实际读取寄存器长度
    uint16_t feign_reg_remain_num = 0;//剩余需伪装读取寄存器长度

#ifdef MODBUS_TLV_READ_RTN_REG_LEN_FEIGN_ENABLE
    /*获取本次Modbus读取的实际长度和伪装长度*/
    Get_Modbus_Read_Real_And_Mask_Length(tl_data->reg_addr, reg_nums, &reg_real_num, &feign_reg_remain_num);
#else
    /*不进行伪装*/
    reg_real_num = reg_nums;
    feign_reg_remain_num = 0;
#endif

    if ( 0 != reg_real_num ) {
        /* 查询目标寄存器表地址 */
        *p_tab = vLookupDataTab(tl_data->slave_addr, tl_data->reg_addr, reg_real_num, false, NULL, chl);
        if (*p_tab == NULL) {
            ESP_LOGE(TAG, "modbus_tlv: no table for slave=%u reg=%u len=%u",
                     (unsigned)tl_data->slave_addr,
                     (unsigned)tl_data->reg_addr,
                     (unsigned)tl_data->len);
            return -2;
        }

        /* 查找寄存器对应数据对象 */
        md_priv_data_t priv_data = {0};
        md_data_t *p_data = md_tbl_find(tl_data->reg_addr);
        if (p_data == NULL) {
            ESP_LOGE(TAG, "modbus_tlv: md_tbl_find failed slave=%u reg=%u len=%u",
                     (unsigned)tl_data->slave_addr,
                     (unsigned)tl_data->reg_addr,
                     (unsigned)tl_data->len);
            return -3;
        }

        /* 若注册了表回调，则在读取前先同步数据 */
        if (p_data->tbl.tbl_cb) {
            tbl_cb_data_t cb_data = {0};
            cb_data.SlaveAddress    = tl_data->slave_addr;
            cb_data.reg_addr        = tl_data->reg_addr;
            cb_data.reg_addr_offset = 0;
            cb_data.reg_nums        = reg_real_num;
            cb_data.is_write        = false;
            cb_data.cb_chl          = chl;

            int cb_rc = p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data);
            if (cb_rc != 0) {
                ESP_LOGE(TAG, "modbus_tlv: tbl_cb returned %d for slave=%u reg=%u len=%u",
                         cb_rc,
                         (unsigned)tl_data->slave_addr,
                         (unsigned)tl_data->reg_addr,
                         (unsigned)tl_data->len);
                return -4;
            }
        }
    } else {
        // 无有效寄存器，无需伪装，直接报错
        return -2;
    }
    
    return (feign_reg_remain_num * 2);
}

/**
 * @brief 预计算 TLV 读取响应需要拆分的总包数
 *
 * 按照与实际发包一致的装包规则进行预扫描，
 * 避免简单使用总字节数上取整导致总包数字段偏小。
 * 
 * @param[in] tlv_tab   TLV 读取列表
 * @param[in] tab_total 有效 TLV 单元数量 
 * @param[in] data_max_len 单次TLV数据最大长度
 * @return uint8_t 需要发送的总包数
 */
static uint8_t modbus_tlv_read_rtn_calc_total_index(const modbus_tlv_unit_t *tlv_tab, uint8_t tab_total, uint16_t data_max_len)
{
    uint16_t payload_len = 0;
    uint8_t total_index = 0;
    uint16_t curr_tlv_len = 0;

    for (uint8_t curr_tab = 0; curr_tab < tab_total; ++curr_tab) {
        /* 基于“有效发送长度”，即 value 为 NULL 时按 0 参与计算 */
        if ( tlv_tab[curr_tab].value ) {
            curr_tlv_len = tlv_tab[curr_tab].len;
        } else {
            curr_tlv_len = 0;
        }

        /* 放 TL 头之前，先确认本帧还能否容纳一个完整 TL 头 */
        if (payload_len > (data_max_len - MODBUS_TL_LEN)) {
            total_index++;
            payload_len = 0;
        }

        /* 放入当前 TL */
        payload_len += MODBUS_TL_LEN;

        /* 放入当前 TL 对应的 V 数据，必要时跨帧 */
        while (curr_tlv_len > 0) {
            uint16_t free_len = data_max_len - payload_len;

            if (free_len == 0) {
                total_index++;
                payload_len = 0;
                free_len = data_max_len;
            }

            if (curr_tlv_len > free_len) {
                curr_tlv_len -= free_len;
                total_index++;
                payload_len = 0;
            } else {
                payload_len += curr_tlv_len;
                curr_tlv_len = 0;
            }
        }
    }

    /* 总包数至少为1 */
    if (payload_len > 0 || total_index == 0) {
        total_index++;
    }

    return total_index;
}

/**
 * @brief 将读取结果按 TLV 协议进行分帧并发送
 *
 * 本函数从 modbus_tlv_read_unit[] 中读取已经准备好的 TLV 单元，
 * 将其按“TL + V”的格式写入发送缓冲区；当当前帧空间不足时立即封包发送，
 * 最终把所有读取结果拆分成若干 TLV 响应包发出。
 *
 * @param[in] tab_total    有效 TLV 单元总数
 * @param[in] chl          发送通道
 */
static void modbus_tlv_read_rtn_framing_and_send(uint8_t tab_total, uint8_t chl)
{
    uint16_t tlv_send_len = 0;
    uint8_t total_index = 0;
    uint8_t curr_index = 1;
    uint8_t curr_tab = 0;
    uint8_t *buffer = modbus_tlv_tx_buffer;
    uint8_t *data = &modbus_tlv_tx_buffer[11];

    if (tab_total == 0) {
        ESP_LOGW(TAG, "modbus_tlv: no read result to send");
        return;
    }

    // 预计算 TLV 读取响应需要拆分的总包数
    total_index = modbus_tlv_read_rtn_calc_total_index(modbus_tlv_read_unit, tab_total, MODBUS_TLV_DATA_LEN_MAX);

    // 获取Modbus信号量
    if (!modbus_data_semaphore_Take()) {
        ESP_LOGE(TAG, "modbus_tlv_read_rtn_framing_and_send: modbus_data_semaphore_Take Failed");
        return;
    }

    while (curr_tab < tab_total) {
        modbus_tlv_unit_t send_unit = modbus_tlv_read_unit[curr_tab];
        uint16_t copied_len = 0;

        /* 当前帧剩余空间不足以容纳完整 TL 头时，先发当前帧 */
        if (tlv_send_len > (MODBUS_TLV_DATA_LEN_MAX - MODBUS_TL_LEN)) {
            modbus_tlv_read_rtn_send_frame(buffer, tlv_send_len, total_index, curr_index++, chl);
            tlv_send_len = 0;
        }

        if ((send_unit.value == NULL) && (send_unit.len > 0)) {
            ESP_LOGE(TAG, "modbus_tlv: null value ptr at index=%u len=%u",
                     (unsigned)curr_tab,
                     (unsigned)send_unit.len);
            
            /* 保留 TL，但本次响应不带 V */
            send_unit.len = 0;
        }

        /* 写入 TL 头 */
        memcpy(data + tlv_send_len, &send_unit, MODBUS_TL_LEN);
        tlv_send_len += MODBUS_TL_LEN;

        /* 写入 V 数据，必要时跨帧发送（仅当存在有效 V 数据时才继续发送） */
        while (copied_len < send_unit.len) {
            uint16_t free_len = MODBUS_TLV_DATA_LEN_MAX - tlv_send_len;
            uint16_t copy_len = send_unit.len - copied_len;

            if (free_len == 0) {
                modbus_tlv_read_rtn_send_frame(buffer, tlv_send_len, total_index, curr_index++, chl);
                tlv_send_len = 0;
                free_len = MODBUS_TLV_DATA_LEN_MAX;
            }

            if (copy_len > free_len) {
                copy_len = free_len;
            }

#ifdef MODBUS_TLV_READ_RTN_REG_LEN_FEIGN_ENABLE
            if ( send_unit.real_len ) {
                if (copy_len > send_unit.real_len) {
                    copy_len = send_unit.real_len;
                }

                /* table中的数据 */
                memcpy(data + tlv_send_len, send_unit.value + copied_len, copy_len);
                send_unit.real_len -= copy_len;
            } else if (send_unit.feign_len) {
                if (copy_len > send_unit.feign_len) {
                    copy_len = send_unit.feign_len;
                }

                /* 伪装数据 */
                memset(data + tlv_send_len, 0, copy_len);
                send_unit.feign_len -= copy_len;
            }
#else
            /*不进行伪装*/
            memcpy(data + tlv_send_len, send_unit.value + copied_len, copy_len);
#endif

            tlv_send_len += copy_len;
            copied_len += copy_len;

            if ((tlv_send_len == MODBUS_TLV_DATA_LEN_MAX) && (copied_len < send_unit.len)) {
                modbus_tlv_read_rtn_send_frame(buffer, tlv_send_len, total_index, curr_index++, chl);
                tlv_send_len = 0;
            }
        }

        curr_tab++;
    }

    /* 发送最后一帧剩余数据 */
    if (tlv_send_len > 0) {
        modbus_tlv_read_rtn_send_frame(buffer, tlv_send_len, total_index, curr_index, chl);
    }

    /* 释放Modbus信号量 */
    modbus_data_semaphore_Give();
}

/**
 * @brief 处理 TLV 读取命令
 *
 * 该函数逐个解析读取命令中的 TL 单元，查找对应寄存器数据表地址，
 * 将有效结果缓存到 modbus_tlv_read_unit[] 中，最后统一按 TLV 响应格式分帧发送。
 *
 * @param[in] data     读取命令数据区，内容仅包含若干 TL 单元，不含 V
 * @param[in] data_len 数据区长度
 * @param[in] chl      当前访问通道
 */
static void modbus_tlv_read_cmd_process(uint8_t *data, uint16_t data_len, uint8_t chl)
{
    uint16_t processed_len = 0;
    uint16_t remain_len = data_len;
    uint8_t index = 0;

    if ((data == NULL) || (data_len < MODBUS_TL_LEN)) {
        ESP_LOGE(TAG, "modbus_tlv_read_cmd_process: invalid input len=%u", (unsigned)data_len);
        return;
    }

    /* 清空读取结果列表 */
    memset(modbus_tlv_read_unit, 0, sizeof(modbus_tlv_read_unit));

    while (remain_len >= MODBUS_TL_LEN) {
        /* 读取命令单元只包含 TL，不包含 Value 内容 */
        modbus_tlv_unit_t *tl_data = (modbus_tlv_unit_t *)(data + processed_len);
        ESP_LOGD(TAG, "modbus_tlv_read_cmd_process: slaveaddr(%d), regaddr(%d), len(%d)", 
                tl_data->slave_addr, tl_data->reg_addr, tl_data->len);
    
        /* 长度非法则跳过当前 TL */
        if ((tl_data->len == 0) || ((tl_data->len % 2) != 0)) {
            ESP_LOGE(TAG, "modbus_tlv: invalid TL len=%u at offset=%u",
                     (unsigned)tl_data->len,
                     (unsigned)processed_len);
            processed_len += MODBUS_TL_LEN;
            remain_len = (processed_len <= data_len) ? (data_len - processed_len) : 0;
            continue;
        }

        /* 判断最大可接受TLV数量溢出 */
        if (index >= MODBUS_TLV_SINGLE_NUM_MAX) {
            ESP_LOGE(TAG, "modbus_tlv: read unit list full");
            break;
        }

        /* 先复制 TL 头部固定字段 */
        memcpy(&modbus_tlv_read_unit[index], tl_data, MODBUS_TL_LEN);

        /* 处理TL单元，寻找对应Value位置 */
        uint16_t *p_tab = NULL;
        int ret = modbus_tlv_process_read_unit(&p_tab, tl_data, chl);
        if ( ret >= 0) {
            modbus_tlv_read_unit[index].value = (uint8_t *)p_tab;
            modbus_tlv_read_unit[index].real_len = modbus_tlv_read_unit[index].len - (uint16_t)ret;
            modbus_tlv_read_unit[index].feign_len = (uint16_t)ret;
        } else {
            modbus_tlv_read_unit[index].value = NULL;
        }

        index++;
        processed_len += MODBUS_TL_LEN;
        remain_len = (processed_len <= data_len) ? (data_len - processed_len) : 0;
    }

    if (index > 0) {
        modbus_tlv_read_rtn_framing_and_send(index, chl);
    } else {
        ESP_LOGW(TAG, "modbus_tlv: no valid TL read unit");
    }
}

/**
 * @brief 处理单个 Modbus TLV 写入单元
 *
 * 该函数用于处理一个完整的 TLV 写入单元，其核心流程如下：
 * 1. 校验 TLV 单元及其 Value 数据指针是否合法；
 * 2. 根据从机地址、寄存器地址和数据长度查找目标寄存器表地址；
 * 3. 将 TLV 中的 Value 数据按 16 位寄存器写入本地寄存器表；
 * 4. 根据配置触发 UART/CAN 同步通知；
 * 5. 若目标对象注册了表回调，则调用回调完成写后同步处理。
 *
 * 说明：
 * - tl_data->len 的单位为字节，且必须为 2 的整数倍；
 * - tl_data->value 指向当前 TL 单元后面的 V 区首地址；
 * - 协议中的寄存器值按高字节在前，写入本地表前需要做高低字节转换。
 *
 * @param[in] tl_data 指向待处理的 TLV 写入单元
 * @param[in] chl     当前访问通道标识
 *
 * @return 0   处理成功
 * @return -1  输入参数非法
 * @return -2  查找目标寄存器表失败
 * @return -3  查找寄存器对应数据对象失败
 * @return -4  写后表回调执行失败
 */
static int modbus_tlv_process_write_unit(const modbus_tlv_unit_t *tl_data, uint8_t chl)
{
    reg_position_t reg_position = {0};
    md_priv_data_t priv_data = {0};
    md_data_t *p_data = NULL;
    uint16_t *p_tab = NULL;
    uint16_t reg_nums = 0;

    if ((tl_data == NULL) || (tl_data->value == NULL)) {
        return -1;
    }

    if ((tl_data->len == 0) || ((tl_data->len % 2) != 0)) {
        ESP_LOGE(TAG, "modbus_tlv: invalid write len=%u slave=%u reg=%u",
                 (unsigned)tl_data->len,
                 (unsigned)tl_data->slave_addr,
                 (unsigned)tl_data->reg_addr);
        return -1;
    }

    reg_nums = tl_data->len / 2;

    /* 查询目标寄存器表地址，并获取对应寄存器位置信息 */
    p_tab = vLookupDataTab(tl_data->slave_addr, tl_data->reg_addr, reg_nums, true, &reg_position, chl);
    if (p_tab == NULL) {
        ESP_LOGE(TAG, "modbus_tlv: no write table for slave=%u reg=%u len=%u",
                 (unsigned)tl_data->slave_addr,
                 (unsigned)tl_data->reg_addr,
                 (unsigned)tl_data->len);
        return -2;
    }

    /* 将 TLV 的 Value 区按寄存器写入本地表 */
    uint16_t writeRegData = 0;
    for ( uint16_t i = 0; i < reg_nums; i++) {
        writeRegData = LSB2MSB(((uint16_t *)tl_data->value)[i]);
        *(p_tab + i) = writeRegData;
    }   
    
#ifdef CONFIG_UART_PORT_ENABLE
    reg_position_list_t *new_position = (reg_position_list_t *)iot_calloc(sizeof(reg_position_list_t));
    if (new_position != NULL) {
        new_position->next = NULL;
        new_position->position.dev_index = tl_data->slave_addr;
        new_position->position.reg_addr = reg_position.reg_addr;
        new_position->position.offset = reg_position.offset;
        new_position->position.len  = reg_position.len;

        sys_new_position_and_transmit(new_position);
        ESP_LOGI(TAG, "uart sync dev=%02x reg=%02x offset=%02x bytes=%02x",
                 new_position->position.dev_index,
                 new_position->position.reg_addr,
                 new_position->position.offset,
                 new_position->position.len);
    } else {
        ESP_LOGE(TAG, "modbus_tlv: uart sync alloc failed");
    }
#endif	
    
#ifdef CONFIG_CAN_PORT_ENABLE
    /* 为modbus转can指令申请内存 */
    can_cmd_queue_struct can_cmd = {NULL, 0, 0};
	can_cmd.num = (reg_nums > MODBUS_TO_CAN_MAX_NUM) ? MODBUS_TO_CAN_MAX_NUM : reg_nums;
    can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
    if (can_cmd.cmd) {
        Modbus_To_CAN_Write_Info_After_Process(tl_data->reg_addr, reg_nums, can_cmd.cmd, &can_cmd.num);
        sys_new_can_data_resend(&can_cmd, tl_data->slave_addr);  
    } else {
        ESP_LOGE(TAG, "modbus_tlv: can sync alloc failed");
    }
#endif

    /*无线透传设置*/
    Modbus_To_Wireless_Write_Info_After_Process(tl_data->reg_addr, reg_nums, tl_data->slave_addr, chl);

    /* 查找寄存器对应数据对象，供写后回调使用 */
    p_data = md_tbl_find(tl_data->reg_addr);
    if (p_data == NULL) {
        ESP_LOGE(TAG, "modbus_tlv: md_tbl_find failed slave=%u reg=%u len=%u",
                 (unsigned)tl_data->slave_addr,
                 (unsigned)tl_data->reg_addr,
                 (unsigned)tl_data->len);
        return -3;
    }

    /* 若注册了表回调，则在写入完成后触发回调，完成状态同步 */
    if (p_data->tbl.tbl_cb) {
        tbl_cb_data_t cb_data = {0};
        cb_data.SlaveAddress    = tl_data->slave_addr;
        cb_data.reg_addr        = tl_data->reg_addr;
        cb_data.reg_addr_offset = reg_position.offset;
        cb_data.reg_nums        = reg_nums;
        cb_data.is_write        = true;
        cb_data.cb_chl          = chl;

        int cb_rc = p_data->tbl.tbl_cb(&p_data->tbl, &cb_data, &priv_data);
        if (cb_rc != 0) {
            ESP_LOGE(TAG, "modbus_tlv: tbl_cb returned %d for slave=%u reg=%u len=%u",
                     cb_rc,
                     (unsigned)tl_data->slave_addr,
                     (unsigned)tl_data->reg_addr,
                     (unsigned)tl_data->len);
            return -4;
        }
    }

    return 0;
}

/**
 * @brief 处理 TLV 写入命令
 *
 * 该函数逐个解析写入命令中的 TLV 单元。每个单元在线上格式中为：
 * TL + V，其中 TL 为固定头部，V 为长度为 len 的写入数据区。
 *
 * 处理流程：
 * 1. 解析当前 TL 头；
 * 2. 校验 len 与剩余报文长度是否合法；
 * 3. 构造本地 TLV 单元对象，并令 value 指向当前单元的 V 区；
 * 4. 调用单元写入处理函数；
 * 5. 按 TL + V 的实际长度推进到下一个单元。
 *
 * @param[in] data     写入命令数据区，内容为若干个 TLV 单元
 * @param[in] data_len 数据区总长度
 * @param[in] chl      当前访问通道
 */
static void modbus_tlv_write_cmd_process(uint8_t *data, uint16_t data_len, uint8_t chl)
{
    uint16_t processed_len = 0;
    uint16_t remain_len = data_len;

    if ((data == NULL) || (data_len < MODBUS_TL_LEN)) {
        ESP_LOGE(TAG, "modbus_tlv_write_cmd_process: invalid input len=%u", (unsigned)data_len);
        return;
    }

    while (remain_len >= MODBUS_TL_LEN) {
        modbus_tlv_unit_t tl_unit = {0};
        uint16_t unit_total_len = 0;

        /* 解析 TL 头 */
        memcpy(&tl_unit, data + processed_len, MODBUS_TL_LEN);

        /* TL 长度非法则跳过该单元，避免死循环 */
        if ((tl_unit.len == 0) || ((tl_unit.len % 2) != 0)) {
            ESP_LOGE(TAG, "modbus_tlv: invalid TL len=%u at offset=%u",
                     (unsigned)tl_unit.len,
                     (unsigned)processed_len);
            processed_len += MODBUS_TL_LEN + tl_unit.len;
            remain_len = (processed_len <= data_len) ? (data_len - processed_len) : 0;
            continue;
        }

        unit_total_len = MODBUS_TL_LEN + tl_unit.len;

        /* 当前单元总长度越界，说明报文不完整 */
        if (unit_total_len > remain_len) {
            ESP_LOGE(TAG, "modbus_tlv: incomplete write unit at offset=%u tl_len=%u remain=%u",
                     (unsigned)processed_len,
                     (unsigned)tl_unit.len,
                     (unsigned)remain_len);
            break;
        }

        /* value 指向当前 TL 后面的真实 V 区 */
        tl_unit.value = data + processed_len + MODBUS_TL_LEN;

        /* 处理当前 TLV 写入单元 */
        if (modbus_tlv_process_write_unit(&tl_unit, chl) != 0) {
            ESP_LOGE(TAG, "modbus_tlv: write unit failed slave=%u reg=%u len=%u",
                     (unsigned)tl_unit.slave_addr,
                     (unsigned)tl_unit.reg_addr,
                     (unsigned)tl_unit.len);
        }

        /* 无论成功失败，都按 TL + V 的实际长度推进 */
        processed_len += unit_total_len;
        remain_len = (processed_len <= data_len) ? (data_len - processed_len) : 0;
    }
}

/**
 * @brief 处理 modbus TLV 接收队列中的消息（非阻塞，处理队列内所有待处理项）
 *
 * 行为与约定：
 * - 从全局队列 xQueue_modbus_tlv_rx 非阻塞取出所有消息并处理；
 * - 入队时队列取得 data 指针所有权，出队后此函数负责释放；
 * - 当前消息数据起始位置为 21000 段 ver 字段，因此功能码按 2 字节解析；
 * - 对未知或不合法消息会记录警告并释放资源。
 */
void modbus_tlv_process_task(void)
{
    uint8_t process_num = 0;
    queue_msg_tlv_t msg;

    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (Modbus_Tlv_TakeMutex(10) != pdTRUE) {
        ESP_LOGE(TAG, "modbus_tlv_process_task: mutex busy, skip");
        return;
    }

    /* 非阻塞循环，直到队列为空 */
    while (xQueue_modbus_tlv_rx && xQueueReceive(xQueue_modbus_tlv_rx, &msg, 0) == pdTRUE) {
        if ((msg.data == NULL) || (msg.len < 2)) {
            ESP_LOGE(TAG, "modbus_tlv_process_task: invalid msg (len=%u, chl=%u)",
                     (unsigned)msg.len, (unsigned)msg.chl);
            iot_free(msg.data);
            continue;
        }

        uint16_t func_code = 0;
        memcpy(&func_code, msg.data, sizeof(func_code));

        ESP_LOGD(TAG, "modbus_tlv_process_task: recv msg len=%u chl=%u func=%u",
                 (unsigned)msg.len, (unsigned)msg.chl, (unsigned)func_code);

        switch (func_code) {
        case MODBUS_21000_FUNC_TLV_DATA:
            ESP_LOGD(TAG, "modbus_tlv_process_task: MODBUS_21000_FUNC_TLV_DATA");
            modbus_tlv_write_cmd_process(msg.data + 4, msg.len - 4, msg.chl);
            break;

        case MODBUS_21000_FUNC_TLV_READ_CMD:
            ESP_LOGD(TAG, "modbus_tlv_process_task: MODBUS_21000_FUNC_TLV_READ_CMD");
            modbus_tlv_read_cmd_process(msg.data + 4, msg.len - 4, msg.chl);
            break;

        default:
            ESP_LOGE(TAG, "modbus_tlv_process_task: unknown func=%u", (unsigned)func_code);
            break;
        }

        /* 处理完成，释放消息数据 */
        iot_free(msg.data);

        /* 单次循环最多处理5条数据 */
        if ( ++process_num >= 5 ) {
            break;
        }
    }

    /* 释放互斥 */
    Modbus_Tlv_GiveMutex();
}

#ifdef CONFIG_MQTT_4G_ANNOUNCE_MODBUS_TLV_ENABLE

/**
 * @brief 底层发送 TLV 数据
 *
 * 根据通道标识将 TLV 数据分发到对应的发送链路。
 *
 * @param[in] data 待发送数据
 * @param[in] len  数据长度
 * @param[in] chl  通道标识
 */
static void modbus_tlv_mqtt_announce_send(uint8_t *tx_buffer, uint16_t payload_len, uint8_t total_index, uint8_t curr_index)
{
    static uint16_t cycle = 0;
    uint16_t crc16 = 0;

    tx_buffer[0]  = MD_SUMMARY_ADDR;
    tx_buffer[1]  = 0x10;
    tx_buffer[2]  = MOD_REG_START_ADDR_21000 >> 8;
    tx_buffer[3]  = MOD_REG_START_ADDR_21000 & 0xFF;
    tx_buffer[4]  = ((payload_len + 4) / 2) >> 8;
    tx_buffer[5]  = ((payload_len + 4) / 2) & 0xFF;
    tx_buffer[6]  = ((payload_len + 4) >= 0xFF) ? 0xFF : (payload_len + 4);
    tx_buffer[7]  = (LSB2MSB(MODBUS_21000_FUNC_TLV_DATA)) & 0xFF;
    tx_buffer[8]  = (LSB2MSB(MODBUS_21000_FUNC_TLV_DATA) >> 8) & 0xFF;
    tx_buffer[9]  = total_index;
    tx_buffer[10] = curr_index;

#if 1   // 按MModbus协议，转换为大端格式
    /* 对 TLV 数据区按 16 位进行高低字节转换 */
    for (uint16_t offset = 11; (offset + 1) < (uint16_t)(11 + payload_len); offset += 2) {
        uint16_t word = 0;
        memcpy(&word, &tx_buffer[offset], sizeof(word));
        word = U16_TO_BE(word);
        memcpy(&tx_buffer[offset], &word, sizeof(word));
    }
#endif

    crc16 = ModbusCrc16(tx_buffer, payload_len + 11);
    tx_buffer[payload_len + 11] = crc16 & 0xFF;
    tx_buffer[payload_len + 12] = (crc16 >> 8) & 0xFF;

    if(cycle == 65535) cycle = 0;
    Mqtt_Active_Report_Queue_Push(tx_buffer, payload_len + 13, 0x01, ++cycle, total_index, curr_index);
}

/**
 * @brief 按 TLV 协议进行分帧并发送
 *
 * 本函数从 modbus_tlv_announce_unit[] 中读取已经准备好的 TLV 单元，
 * 将其按“TL + V”的格式写入发送缓冲区；当当前帧空间不足时立即封包发送，
 * 最终把所有读取结果拆分成若干 TLV 响应包发出。
 * 
 * @param[in] modbus_tlv_announce_unit 有效 TLV 单元表
 * @param[in] tab_total         有效 TLV 单元总数
 * @param[in] buffer_max_len    当前最大帧空间
 */
static void modbus_tlv_mqtt_announce_framing(modbus_tlv_unit_t *modbus_tlv_announce_unit, uint8_t tab_total, uint16_t buffer_max_len)
{
    if ((modbus_tlv_announce_unit == NULL) || (tab_total == 0) || (buffer_max_len <= 13)) {
        ESP_LOGE(TAG, "modbus_tlv_mqtt_announce_framing: invalid.");
        return;
    }

    uint16_t tlv_send_len = 0;
    uint8_t total_index = 0;
    uint8_t curr_index = 1;
    uint8_t curr_tab = 0;
    
    /* 申请对应缓存 */
    uint8_t *buffer = (uint8_t *)iot_calloc(buffer_max_len);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "modbus_tlv_mqtt_announce_framing: calloc failed.");
        return;
    }
    uint8_t *data = &buffer[11];
    uint16_t data_max_len = buffer_max_len - 23;

    // 预计算 TLV 读取响应需要拆分的总包数
    total_index = modbus_tlv_read_rtn_calc_total_index(modbus_tlv_announce_unit, tab_total, data_max_len);

    while (curr_tab < tab_total) {
        modbus_tlv_unit_t send_unit = modbus_tlv_announce_unit[curr_tab];
        uint16_t copied_len = 0;

        /* 当前帧剩余空间不足以容纳完整 TL 头时，先发当前帧 */
        if (tlv_send_len > (data_max_len - MODBUS_TL_LEN)) {
            modbus_tlv_mqtt_announce_send(buffer, tlv_send_len, total_index, curr_index++);
            tlv_send_len = 0;
        }

        if ((send_unit.value == NULL) && (send_unit.len > 0)) {
            ESP_LOGE(TAG, "modbus_tlv: null value ptr at index=%u len=%u",
                     (unsigned)curr_tab,
                     (unsigned)send_unit.len);
            
            /* 保留 TL，但本次响应不带 V */
            send_unit.len = 0;
        }

        /* 写入 TL 头 */
        memcpy(data + tlv_send_len, &send_unit, MODBUS_TL_LEN);
        tlv_send_len += MODBUS_TL_LEN;

        /* 写入 V 数据，必要时跨帧发送（仅当存在有效 V 数据时才继续发送） */
        while (copied_len < send_unit.len) {
            uint16_t free_len = data_max_len - tlv_send_len;
            uint16_t copy_len = send_unit.len - copied_len;

            if (free_len == 0) {
                modbus_tlv_mqtt_announce_send(buffer, tlv_send_len, total_index, curr_index++);
                tlv_send_len = 0;
                free_len = data_max_len;
            }

            if (copy_len > free_len) {
                copy_len = free_len;
            }

            memcpy(data + tlv_send_len,
                   send_unit.value + copied_len,
                   copy_len);

            tlv_send_len += copy_len;
            copied_len += copy_len;

            if ((tlv_send_len == data_max_len) && (copied_len < send_unit.len)) {
                modbus_tlv_mqtt_announce_send(buffer, tlv_send_len, total_index, curr_index++);
                tlv_send_len = 0;
            }
        }

        curr_tab++;
    }

    /* 发送最后一帧剩余数据 */
    if (tlv_send_len > 0) {
        modbus_tlv_mqtt_announce_send(buffer, tlv_send_len, total_index, curr_index);
    }

    // 释放占用空间
    iot_free(buffer);
}

/**
 * @brief 处理 TLV 上报列表
 *
 * 该函数逐个解析上报列表，查找对应寄存器数据表地址，
 * 将有效结果缓存，最后统一按 TLV 响应格式分帧发送。
 *
 * @param[in] data_ctx 上报列表
 * @param[in] data_ctx_total 上报列表成员个数
 * @param[in] buffer_max_len 单帧最大允许长度
 */
void modbus_tlv_mqtt_announce(const modbus_tlv_announce_t *data_ctx, uint8_t data_ctx_total, uint16_t buffer_max_len)
{
    if ((data_ctx == NULL) || (data_ctx_total == 0) || (buffer_max_len <= 13)) {
        ESP_LOGE(TAG, "modbus_tlv_mqtt_announce: invalid.");
        return;
    }

    /* 尝试获取互斥，超时则跳过本轮处理 */
    if (Modbus_Tlv_TakeMutex(100) != pdTRUE) {
        ESP_LOGE(TAG, "modbus_tlv_mqtt_announce: mutex busy, skip");
        return;
    }

    uint16_t curr_ctx = 0;
    uint8_t index = 0;

    /* 申请对应缓存 */
    modbus_tlv_unit_t *modbus_tlv_announce_unit = (modbus_tlv_unit_t *)iot_calloc(data_ctx_total * sizeof(modbus_tlv_unit_t));
    if (modbus_tlv_announce_unit == NULL) {
        ESP_LOGE(TAG, "modbus_tlv_mqtt_announce: calloc failed.");
        goto EXIT_GIVEMUTEX;
    }

    ESP_LOGD(TAG, "modbus_tlv_mqtt_announce: data_ctx_total = %d", data_ctx_total);

    while (curr_ctx < data_ctx_total) {
        /* 寄存器数量非法则跳过当前对象 */
        if (data_ctx[curr_ctx].reg_num == 0) {
            curr_ctx++;
            continue;
        }

        /* 生成 TL 头部固定字段 */
        modbus_tlv_announce_unit[index].slave_addr = data_ctx[curr_ctx].slave_addr;
        modbus_tlv_announce_unit[index].reg_addr = data_ctx[curr_ctx].reg_addr;
        modbus_tlv_announce_unit[index].len = data_ctx[curr_ctx].reg_num * 2;

        /* 处理TL单元，寻找对应Value位置 */
        uint16_t *p_tab = NULL;
        if (modbus_tlv_process_read_unit(&p_tab, &modbus_tlv_announce_unit[index], MD_CHL_SELF) == 0) {
            modbus_tlv_announce_unit[index].value = (uint8_t *)p_tab;
        } else {
            modbus_tlv_announce_unit[index].value = NULL;
        }

        index++;
        curr_ctx++;
    }

    if (index > 0) {
        ESP_LOGD(TAG, "modbus_tlv_mqtt_announce: unit_total = %d", index);
        modbus_tlv_mqtt_announce_framing(modbus_tlv_announce_unit, index, buffer_max_len);
    }

    // 释放占用空间
    iot_free(modbus_tlv_announce_unit);

EXIT_GIVEMUTEX:
    
    /* 释放互斥 */
    Modbus_Tlv_GiveMutex();
}

#endif

#endif
