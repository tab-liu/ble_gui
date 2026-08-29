/**
  ******************************************************************************
  * @file      usbh_cdc_user_common.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/28
  * @brief     USB用户自定义参数
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/28 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

/* ================================ 库文件引用 ================================ */
 
#include "sdkconfig.h"
#include "usbh_cdc_user_common.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[USB_CDC_USER_COMMON]"

/* =============================== 文件内全局变量 ================================ */

/*
信息来源于文档
1.“Fibocom_MTC_拨号指南_Linux_V1.3.pdf” [2.1 USB端口信息]
2."Quectel_EC2x&EG2x-G&EG9x系列_USB描述符介绍_V1.1.pdf"
*/
static const usbh_cdc_user_info_t usbh_cdc_user_list[] = {
    // L61x/LC61X/LG61X/MC61X/MG61X
    { .vid = 0x1782, .pid = 0x4D12, .modem_port = 0, .second_at_port = 0, .allow_connect = false },     // NPI模式，模块校准用(禁止连接)
    { .vid = 0x1782, .pid = 0x4D10, .modem_port = 0, .second_at_port = 5, .allow_connect = true },      // GTUSBMODE: 31(默认)
    { .vid = 0x1782, .pid = 0x4D11, .modem_port = 2, .second_at_port = 7, .allow_connect = true },      // GTUSBMODE: 32/33

    // NL668/MG110/MC116/LC116
    { .vid = 0x1508, .pid = 0x1001, .modem_port = 1, .second_at_port = 2, .allow_connect = true },      // GTUSBMODE: 17/18/25/26/27
    { .vid = 0x1508, .pid = 0x1000, .modem_port = 0, .second_at_port = 1, .allow_connect = true },      // GTUSBMODE: 21/22/23

    // EC21系列/EG21-G
    { .vid = 0x2C7C, .pid = 0x0121, .modem_port = 3, .second_at_port = 2, .allow_connect = true },

    // EC25系列/EG25-G
    { .vid = 0x2C7C, .pid = 0x0125, .modem_port = 3, .second_at_port = 2, .allow_connect = true },

    // EC20-CN/EC20-CE
    { .vid = 0x2C7C, .pid = 0x0120, .modem_port = 3, .second_at_port = 2, .allow_connect = true },

    // EG91系列
    { .vid = 0x2C7C, .pid = 0x0191, .modem_port = 3, .second_at_port = 2, .allow_connect = true },

    // EG95系列
    { .vid = 0x2C7C, .pid = 0x0195, .modem_port = 3, .second_at_port = 2, .allow_connect = true },
};

/* ================================ 模块函数定义 ================================ */

/**
 * @brief  根据VID和PID查找usbh_cdc_user_list中的设备信息
 *
 * @param  vid       设备的厂商ID
 * @param  pid       设备的产品ID
 * @param  out_info  输出参数，指向找到的usbh_cdc_user_info_t结构体指针，未找到时为NULL
 * @return ESP_OK    查找到对应设备信息
 * @return ESP_FAIL  未查找到对应设备信息
 */
esp_err_t find_usbh_cdc_user_info(uint16_t vid, uint16_t pid, const usbh_cdc_user_info_t **out_info)
{
    size_t count = sizeof(usbh_cdc_user_list) / sizeof(usbh_cdc_user_list[0]);
    for (size_t i = 0; i < count; ++i) {
        if (usbh_cdc_user_list[i].vid == vid && usbh_cdc_user_list[i].pid == pid) {
            if (out_info) {
                *out_info = &usbh_cdc_user_list[i];
            }
            return ESP_OK;
        }
    }
    if (out_info) {
        *out_info = NULL;
    }
    return ESP_FAIL;
}


