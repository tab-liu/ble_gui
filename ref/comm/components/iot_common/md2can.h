#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t type;    // modbus转can的指令数据类型
    uint16_t offset; // 数据偏移 是相对某一块CAN数据的偏移(eg:0x1a)
    uint16_t len;    // 数据长度

    /* 发送该指令后需要激活的指令类型
     * 例如要读取逆变的历史日志，先设置历史日志页指令（type=INV_TYPE_CONFIG00，offset=offsetof(inv_set00_struct, ctrl_log_page)，len=sizeof(ctrl_log_page)）
     * 然后激活历史日志上传指令（active_can_cmd_type=0x20,因为历史日志是触发读取）*/
    uint16_t active_can_cmd_type;
} can_data_label;

typedef struct
{
    can_data_label *cmd;
    uint16_t num;    // 需要转换成多少条CAN命令
    uint8_t md_addr; // modbus地址
} can_cmd_queue_struct;
