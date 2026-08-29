/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName:
Author: weiyt\xiongf
Date: 2022/5/23
Description: ble驱动、服务、数据、鉴权收发接口
Version: V1.00
Function List: none
***********************************************************/
#define LOG_TAG "IOT_BLE"

#include "iot_ble.h"
#include "iot_uart.h"
#include "iot_service.h"
#include "iot_software_timer.h"
#include "iot_at_cmd.h"
#include "tinyfs.h"
#include "iot_gpio.h"
#include "iot_bsp.h"
#include "ls_hal_iwdg.h"
#include "modem_rf_le501x.h"
#include "iot_ota.h"
#include "iot_uart.h"
#include "iot_md5.h"
#include "ls_hal_crypt.h"
#include "iot_aes.h"
#include "iot_timer.h"
#include "iot_magnetic_screen.h"
#include "iot_adc_battery.h"
#include "iot_iic.h"
#include "sensor_cht8325.h"
#include "rtc_aip8563.h"
#include "app_config.h"

#define IOT_BOUD_ENABLE 0
#define MAX_MTU 247
#define DEFAULT_MTU 23
#define UART_RX_MAX_LEN (MAX_MTU - 3)
#define UART_TX_MAX_LEN (MAX_MTU - 3)
#define BLE_KEY 2
#define DIR1_TEST 7
#define DIR1_TEST_DATA "hello world!"
#define MAGNETIC_SCREEN_RECORD 0x01

#define CON_IDX_INVALID_VAL 0xff

#define BLE_GAP_IO_CAPS_DISPLAY_ONLY 0x0 /**< Display Only. */
#define BLE_GAP_IO_CAPS_DISPLAY_YESNO 0x1 /**< Display and Yes/No entry. */
#define BLE_GAP_IO_CAPS_KEYBOARD_ONLY 0x2 /**< Keyboard Only. */
#define BLE_GAP_IO_CAPS_NONE 0x3 /**< No I/O capabilities. */
#define BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY 0x4 /**< Keyboard and Display. */

#define OOB_DATA_FLAG 0x0 /**< Indicate whether OOB is supported. */
#define AUTHREQ (AUTH_BOND) /**< Set the auth. */
#define KEY_SIZE 0x10 /**< Indicate the supported maximum LTK size (range: 7-16). */
#define INIT_KEY_DIST (KDIST_ENCKEY | KDIST_IDKEY) /**< Set the initial key distribution. */
#define RESP_KEY_DIST (KDIST_ENCKEY | KDIST_IDKEY) /**< Set the response key distribution. */
#define passkey_number {'1', '2', '3', '4', '5', '6'} /**< Set the passkey,size(6). */

#define Log_enable 01

struct gap_slave_security_req test_auth;
struct pair_feature test_feat
    = {BLE_GAP_IO_CAPS_NONE, OOB_DATA_FLAG, AUTHREQ, KEY_SIZE, INIT_KEY_DIST, RESP_KEY_DIST};

struct gap_pin_str test_passkey = {passkey_number, 0};

ble_para_t ble_para = {
    .adv_update_flag = false,
    .conn_prar_update_flag = 0,
    .connect_id = CON_IDX_INVALID_VAL,
    .connect_status = false,
    .adv_enable = true,
    .can_send_notify = false,
    .self_can_send_notify = false,
    .mtu_size = 23,
    .max_wait_time = 60,
};

uint8_t pwd[6];
uint8_t pack_buf[10];
uint8_t resend_index[3] = {1, 2, 3};
uint8_t resend_flag = 0;
uint8_t pack_buf_first[10];
uint8_t ECDH_Pack_ptr[134];
uint8_t client_ECDH_Pack_ptr[134];
tinyfs_dir_t ble_para_dir;
tinyfs_dir_t magnetic_screen_para_dir;
bool ECDH_key_rsp_flag;
bool client_ECDH_key_rsp_flag;
ble_save_para_t ble_save_para;
magnetic_screen_para_t magnetic_screen_para;
ble_data_t ble_data;
identify_handr_t myident;

static iot_timer_id_t ble_receive_timer_id = 0xFF;
static iot_timer_id_t ble_self_timer_id = 0xFF;
static iot_timer_id_t ble_identify_timer_id = 0xFF;
static iot_timer_id_t ble_resend_timer_id = 0xFF;
static iot_timer_id_t ble_adv_timeout_timer_id = 0xFF;
static iot_timer_id_t fast_match_timeout_timer_id = 0xFF;
static bool fast_match_mode_active = false;
static iot_timer_id_t adv_recv_timeout_timer_id = 0xFF;

// 临时存储广播设备mac地址
static uint8_t adv_mac_temp[6] = {0};

bool make_ecc_key_startup = false;

struct gap_update_conn_param conn_param = {
    .intv_min = 0x0C,
    .intv_max = 0x0C,
    .latency = 0,
    .sup_timeout = 400,
};

// static uint8_t bleuart_SetCharValue = 0;
static const uint8_t ble_uart_svc_uuid[] = {0x00, 0xff};
static const uint8_t ble_uart_rx_uuid[] = {0x01, 0xff};
static const uint8_t ble_uart_tx_uuid[] = {0x02, 0xff};
static const uint8_t ble_set_uuid[] = {0x04, 0xff};
static const uint8_t ble_self_uuid[] = {0x03, 0xff};
static const uint8_t att_decl_char_array[] = {0x03, 0x28};
static const uint8_t att_desc_client_char_cfg_array[] = {0x02, 0x29};

static struct gatt_svc_env uart_server_svc_env;

static uint8_t adv_obj_hdl;
static uint16_t cccd_config = 0x0000;
static uint16_t cccd_self_config = 0x0000;

static void uart_server_data_length_update(uint8_t con_idx);

enum uart_svc_att_db_handles {
    BLE_UART_RX_CHAR,
    BLE_UART_RX_CHAR_VAL,
    BLE_UART_RX_CHAR_CFG,
    BLE_UART_TX_CHAR,
    BLE_UART_TX_VAL,
    BLE_SET_CHAR,
    BLE_SET_VAL,
    BLE_SELF_CHAR,
    BLE_SELF_VAL,
    BLE_SELF_CHAR_CFG,
    BLE_UART_IDX_NB
};

enum devinfo_handles {
    SYSTEM_ID_CHAR,
    SYSTEM_ID_CHAR_VAL,

    MODEL_NUMBER_CHAR,
    MODEL_NUMBER_CHAR_VAL,

    SERIAL_NUMBER_UUID_CHAR,
    SERIAL_NUMBER_UUID_CHAR_VAL,

    FIRMWARE_REV_CHAR,
    FIRMWARE_REV_CHAR_VAL,

    HARDWARE_REV_CHAR,
    HARDWARE_REV_CHAR_VAL,

    SOFTWARE_REV_CHAR,
    SOFTWARE_REV_CHAR_VAL,

    MANUFACTURER_NAME_CHAR,
    MANUFACTURER_NAME_CHAR_VAL,

    DEVINFO_IDX_NB,
};

enum silabs_ota_handles {
    DILABS_OTA_CTR_CHAR,
    DILABS_OTA_CTR_CHAR_VAL,
    DILABS_OTA_DATA_CHAR,
    DILABS_OTA_DATA_CHAR_VAL,
    DILABS_OTA_IDX_NB,
};

/*扫描回应包*/
uint8_t scanRspData[31] = {
    12, /*长度=名字长度+1个类型*/
    0x09, /*产品全名，这里最大字符串个数为29*/
    'B',
    'L',
    'U',
    'E',
    'T',
    'T',
    'I',
    '-',
    'I',
    'O',
    'F',
};

/*广播包*/
static uint8_t advertData[24] = {
    /*FF00服务（经过测试，app bluetti软件会判断是否有这个类型，符合的才会发起连接）  */
    0x03,
    0x03,
    0x00,
    0xff,

    /*6位MAC地址，由于苹果IOS系统不支持直接获取MAC，所以需要将MAC附着在广播包里面*/
    0x08,
    0x1b,
    0x00,
    0x11,
    0x22,
    0x33,
    0x44,
    0x55,
    0x66,

    /*bluetti自定义厂商名称，方便手机识别用*/
    0x0A,
    0xff,
    'B',
    'L',
    'B',
    'L',
    'U',
    'E',
    'T',
    'T',
    'F',
};

// static uint8_t adv_no_connect_data[] = {
//     0x07, // len
//     0xFF,
//     // 0x0F06, company id
//     0x06,
//     0x0F,
//     // can not connect
//     0x10,
//     0x80,
//     // product id
//     0x13,
//     0x30,
// };

/*Attribute declaration*/
static const struct att_decl gatts_attr_db_t[BLE_UART_IDX_NB] = {
    [BLE_UART_RX_CHAR] = {
        .uuid = att_decl_char_array,
        .s.max_len = 0,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.rd_en = 1, /*可读*/
    },
    [BLE_UART_RX_CHAR_VAL] = {
        .uuid = ble_uart_rx_uuid,
        .s.max_len = UART_RX_MAX_LEN,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.ntf_en = 1, /*可通知*/
    },
    [BLE_UART_RX_CHAR_CFG] = {
        .uuid = att_desc_client_char_cfg_array,
        .s.max_len = 0,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.rd_en = 1,
        .char_prop.wr_req = 1, /*写回复*/
    },
    [BLE_UART_TX_CHAR] = {
        .uuid = att_decl_char_array,
        .s.max_len = 0,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.rd_en = 1, /*可读*/
        // .char_prop.wr_cmd = 1, /*写无需回复*/
    },
    [BLE_UART_TX_VAL] = {
        .uuid = ble_uart_tx_uuid,
        .s.max_len = UART_TX_MAX_LEN,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.wr_cmd = 1, /*写无需回复*/
        .char_prop.wr_req = 1, /*写回复*/
    },
    [BLE_SET_CHAR] = {
        .uuid = att_decl_char_array,
        .s.max_len = 0,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.rd_en = 1, /*可读*/
    },
    [BLE_SET_VAL] = {
        .uuid = ble_set_uuid,
        .s.max_len = 1,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.wr_cmd = 1, /*写无需回复*/
        //.char_prop.rd_en = 1, /*可读*/
    },
    [BLE_SELF_CHAR] = {
        .uuid = att_decl_char_array,
        .s.max_len = 0,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.rd_en = 1, /*可读*/
    },
    [BLE_SELF_VAL] = {
        .uuid = ble_self_uuid,
        .s.max_len = UART_RX_MAX_LEN,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.ntf_en = 1, /*可通知*/
        .char_prop.wr_cmd = 1, /*写无需回复*/
        .char_prop.wr_req = 1, /*写回复*/
        //.char_prop.rd_en = 1, /*可读*/
    },
    [BLE_SELF_CHAR_CFG] = {
        .uuid = att_desc_client_char_cfg_array,
        .s.max_len = 0,
        .s.uuid_len = UUID_LEN_16BIT,
        .s.read_indication = 1,
        .char_prop.rd_en = 1,
        .char_prop.wr_req = 1, /*写回复*/
    },

};

/*Service declaration*/
static const struct svc_decl uart_server_svc = {
    .uuid = ble_uart_svc_uuid,
    .att = (struct att_decl *)gatts_attr_db_t,
    .nb_att = BLE_UART_IDX_NB,
    .uuid_len = UUID_LEN_16BIT,
#if IOT_BOUD_ENABLE
    .sec_lvl = 1,
#endif
};

/*********** data for client ***************/
#if MASTER_CLIENT_ROLE == 1
enum initiator_status {
    INIT_IDLE,
    INIT_BUSY,
};
enum scan_status {
    SCAN_IDLE,
    SCAN_BUSY,
};

// static struct dev_addr specified_slave_addr;

static uint8_t con_idx_client;
static uint16_t ble_client_mtu;

static uint16_t ble_client_svc_attribute_handle; // handle for primary service attribute handle
static uint16_t ble_client_svc_end_handle;
// static uint16_t ble_client_tx_attribute_handle;
static uint16_t ble_client_tx_pointer_handle;
// static uint16_t ble_client_rx_attribute_handle;
// static uint16_t ble_client_rx_pointer_handle;
static uint16_t ble_client_cccd_handle;
// static uint8_t *next_connect_addr = NULL;

static uint8_t scan_obj_hdl = 0xff;
// static uint8_t init_obj_hdl = 0xff;
// static uint8_t init_status = INIT_IDLE;
// static uint8_t dev_addr_type = 0; /*  0:Public, 1:Private */

static bool client_encryption_required = true; // 默认是加密方式，根据广播名判断
static identify_handr_t ble_client_ident;

static void ble_client_init(void);
static void start_scan(void);

#define CONNECTION_IS_SERVER(idx) ((idx) != CON_IDX_INVALID_VAL && (idx) == ble_para.connect_id)
#define CONNECTION_IS_CLIENT(idx) ((idx) != CON_IDX_INVALID_VAL && (idx) == con_idx_client)

#endif // MASTER_CLIENT_ROLE == 1

/*********** func for client *********************/
#if MASTER_CLIENT_ROLE == 1
static void ble_client_init(void) {
    con_idx_client = CON_IDX_INVALID_VAL;
    ble_client_mtu = DEFAULT_MTU;
    ble_client_svc_attribute_handle = 0x1;
    ble_client_svc_end_handle = 0xffff;
}

// static void start_init(uint8_t *peer_addr) {
//     struct dev_addr peer_dev_addr_str;
//     memcpy(peer_dev_addr_str.addr, peer_addr, BLE_ADDR_LEN);
//     struct start_init_param init_param = {
//         .scan_intv = 64,
//         .scan_window = 48,
//         .conn_to = 0,
//         .conn_intv_min = 20, // 配置连接间隔, 单位为1.25ms
//         .conn_intv_max = 20,
//         .conn_latency = 0,
//         .supervision_to = 400, // 配置连接超时, 单位为10ms

//         .peer_addr = &peer_dev_addr_str,
//         .peer_addr_type = dev_addr_type,
//         .type = DIRECT_CONNECTION,
//     };
//     LOG_D("start init, peer addr is: ------------------------>");
//     LOG_HEX(peer_addr, BLE_ADDR_LEN);
//     LOG_D("init object handle is %d", init_obj_hdl);
//     dev_manager_start_init(init_obj_hdl, &init_param);
//     LOG_D("Start initializing connection");
// }

static void client_update_conn_param(uint8_t conidx) {
    struct gap_update_conn_param param = {
        .intv_min = 200, // 配置连接间隔, 单位为1.25ms
        .intv_max = 200, // 配置连接间隔, 单位为1.25ms
        .latency = 0, // 连续不回应的空包或通讯数据个数
        .sup_timeout = 1000, // 连接超时时间，时间为10秒
    };
    gap_manager_update_conn_param(conidx, &param);
}

static void start_scan(void) {
    LS_ASSERT(scan_obj_hdl != 0xff);
    struct start_scan_param scan_param = {
        .scan_intv = 0x4000,
        .scan_window = 0x4000,
        .duration = 0,
        .period = 0,
        .type = OBSERVER,
        .active = 0,
        .filter_duplicates = 0,
    };
    dev_manager_start_scan(scan_obj_hdl, &scan_param);
    // LOG_I("start scanning");
}

// 发送明文数据
void ble_client_send_data(uint8_t *data, uint16_t len) {
    // LOG_I("client: going to send data to ble server");
    // LOG_HEX(data, len);
    if (con_idx_client != CON_IDX_INVALID_VAL && ble_client_tx_pointer_handle != NULL) {
        gatt_manager_client_write_no_rsp(con_idx_client, ble_client_tx_pointer_handle, data, len);
        // gatt_manager_client_write_with_rsp(con_idx_client, ble_client_tx_pointer_handle, data,
        // len);
    } else {
        // LOG_I("ble_client_send_data con_idx_client is invalid!");
    }
}

// 发送AES加密数据
void ble_client_send_data_aes(uint8_t *data, uint16_t len, client_aes_key_t key_type) {
    uint16_t aes_len = 0;
    uint8_t *aes_buf = malloc(len + 32); // 申请内存
    if (aes_buf == NULL) {
        LOG_I("malloc aes_buf failed!");
        return;
    }
    memset(aes_buf, 0, len + 32); // 清空内存

    aes_buf[0] = len >> 8;
    aes_buf[1] = len & 0xff;
    if (key_type == CLIENT_IDENTITY_KEY) { // 鉴权过程
        aes_len = client_Aes_Ecb_encry(data, ble_client_ident.md5_rst, len); // 加密数据
        if (aes_len <= 0) {
            LOG_I("ble_client_send_data_aes aes_len is invalid!");
            free(aes_buf); // 释放内存
            return;
        }
        memcpy(aes_buf + 2, data, aes_len); // 复制数据
        ble_client_send_data(aes_buf, aes_len + 2); // 发送数据
    } else if (key_type == CLIENT_DATA_KEY) { // 发送数据
        memcpy(aes_buf + 6, data, len);
        aes_len = client_Aes_Cbc_encry(aes_buf + 2, len); // 加密数据, 预留4字节放随机数
        if (aes_len <= 0) {
            LOG_I("ble_client_send_data_aes aes_len is invalid!");
            free(aes_buf); // 释放内存
            return;
        }
        ble_client_send_data(aes_buf, aes_len + 6); // 发送数据
    }
    // LOG_I("ble_client_send_data_aes success");
    // LOG_HEX(aes_buf, aes_len + 2);
    free(aes_buf); // 释放内存
}

// * 鉴权相关

// 供外部查看是否为加密连接
bool is_client_encryption_required() {
    return client_encryption_required;
}

// step 1: 接收并处理鉴权请求
static void ble_client_identify_request_check(const uint8_t *data, uint16_t len) {
    uint8_t rng_tmp[4];
    // 更新连接参数，给足计算时间
    struct gap_update_conn_param param = {
        .intv_min = 12, // 保持原有的连接间隔
        .intv_max = 12,
        .latency = 0,
        .sup_timeout = 1000 // 增加到10秒，给足计算时间
    };
    gap_manager_update_conn_param(con_idx_client, &param);
    for (uint8_t i = 0; i < 4; i++) {
        rng_tmp[i] = data[7 - i]; // 取出随机数，反转存储
    }

    LOG_I("step 1: rng_tmp = 0x%02x%02x%02x%02x", rng_tmp[0], rng_tmp[1], rng_tmp[2], rng_tmp[3]);
    MD5_Encrpyt(rng_tmp, ble_client_ident.md5_rst, 4); // 计算MD5
    memcpy(ble_client_ident.MD5_DATA, ble_client_ident.md5_rst + 8, 4); // 取出MD5数据 9- 12位

    client_Pdc_Aes_key_first(ble_client_ident.md5_rst); // 计算AES密钥

    return;
}

// step 2: 鉴权请求回应处理
static void ble_client_identify_rsp() {
    uint8_t pack_buf[10] = {0};
    identify_verify_pack(pack_buf, ble_client_ident.MD5_DATA); // 组包
    ble_client_send_data(pack_buf, 10); // 发送数据
}

// step 3: 鉴权结果检查
static bool ble_client_rst_check(const uint8_t *data, uint16_t len) {
    if (data[4] != IDENT_PASS) {
        LOG_I("identify_rst data rst error!");
        gap_manager_disconnect(con_idx_client, 0x13); // 断开连接
        return false;
    }
    return true;
}

// step 4 : ECDH密钥请求检查
static bool client_ECDH_key_request_check(const uint8_t *data, uint8_t len) {
    uint8_t text[144] = {0};
    uint16_t text_len = ((uint16_t)data[0] << 8) + data[1]; // 获取数据长度
    // LOG_D("text_len = %d", text_len);

    memcpy(text, data + 2, len - 2); // 取出数据
    client_Aes_Ecb_decry(text, ble_client_ident.md5_rst, text_len); // 解密数据
    // LOG_HEX(text, text_len);

    if (!client_ECDH_key_req_check(text, ble_client_ident.md5_rst)) { // 处理ECDH密钥请求
        LOG_I("client_ECDH_key_req_check error!");
        gap_manager_disconnect(con_idx_client, 0x13); // 断开连接
        return false;
    }
    client_ECDH_key_rsp_flag = true;
    LOG_I("client ecdh key req check success");
    return true;
}

// step 5: client(app) 鉴权回应（这里复用server端开机生成的key节省开销）
static bool ble_client_ecdh_key_rsp() {
    if (client_ECDH_key_rsp_flag) {
        LOG_I("before step 5 sign");
        uint8_t sign_buf[64] = {0}; // 装签名值
        client_ECDH_key_rsp_flag = false; // 重置标志位
        if (!client_OS_sign_test(sign_buf,
                                 client_ECDH_Pack_ptr + 4,
                                 ble_client_ident.md5_rst)) { // 前面空4个字节打包格式
            LOG_I("client_OS_sign_test error!");
            return false;
        }

        client_ECDH_Pack_ptr[0] = '*';
        client_ECDH_Pack_ptr[1] = '*';
        client_ECDH_Pack_ptr[2] = ECDH_key_check; // ECDH密钥请求回应
        client_ECDH_Pack_ptr[3] = 0x80; // 数据长度
        uint16_t sum = 0;
        for (uint8_t i = 0; i < 130; i++)
            sum += client_ECDH_Pack_ptr[2 + i];
        client_ECDH_Pack_ptr[132] = sum >> 8;
        client_ECDH_Pack_ptr[133] = sum & 0xff;
        LOG_I("after step 5 sign");
    }

    // 发送数据
    uint8_t temp_data[134] = {0};
    memcpy(temp_data, client_ECDH_Pack_ptr, 134);
    ble_client_send_data_aes(temp_data, 134, CLIENT_IDENTITY_KEY);
    LOG_I("send step 5 sign");
    return true;
}

// step 6: ECDH密钥验证结果处理
static bool ble_client_ecdh_key_check(const uint8_t *data, uint16_t len) {
    uint8_t text[32] = {0};
    uint16_t text_len = ((uint16_t)data[0] << 8) + data[1]; // 获取数据长度

    memcpy(text, data + 2, len - 2); // 取出数据
    client_Aes_Ecb_decry(text, ble_client_ident.md5_rst, text_len); // 解密数据

    LOG_HEX(text, text_len);

    if (text[4] != IDENT_PASS) {
        LOG_I("client ECDH key check rst error!");
        gap_manager_disconnect(con_idx_client, 0x13); // 断开连接
        return false;
    }
    ble_client_ident.rst_second = true; // 鉴权结果检查成功
    LOG_I("step 6: ECDH key check success");
    return true;
}

// step 7: 验证结果后，发送随意数数据以表示鉴权成功
static void ble_client_send_some_data() {
    uint8_t pack_buf[6] = {6, 6, 6, 6, 6, 6};
    LOG_I("step 7: send some data");
    ble_client_send_data(pack_buf, 6); // 发送数据
    client_update_conn_param(con_idx_client); // 更新连接参数
    uint8_t len = strlen((char *)ble_para.device_name);
}

static void client_identify_handler(const uint8_t *data, uint16_t len) {
    if (data[0] == '*' || data[1] == '*') {
        if (!check_sum((uint8_t *)&data[2], len - 2)) {
            LOG_I("identify_rst data sum error!");
            gap_manager_disconnect(con_idx_client, 0x13); // 断开连接
            return;
        }
        // 第一阶段鉴权
        if (len == 10 && data[2] == identify_request) {
            if (ble_client_ident.status == identify_request) {
                ble_client_identify_request_check(data, len); // 处理鉴权请求
                ble_client_ident.status = identify_verity;
                ble_client_identify_rsp(); // 发送鉴权请求回应
                ble_client_ident.status = identify_rst;
            } else if (ble_client_ident.status == identify_rst) {
                LOG_I("resend identify verity!");
                ble_client_identify_rsp(); // 发送鉴权请求回应
            }
        } else if (len == 7 && data[2] == identify_rst && ble_client_ident.status == identify_rst) {
            if (!ble_client_rst_check(data, len)) { // 处理鉴权结果
                ble_client_ident.status = identify_request;
            } else {
                ble_client_ident.rst_first = true; // 鉴权结果检查成功
                ble_client_ident.status = ECDH_key_request;
            }
        }
    } else if (data[0] == 0x00 && data[1] == 0x86) { // 收到鉴权请求
        if (ble_client_ident.status == ECDH_key_request) {
            if (!client_ECDH_key_request_check(data, len)) { // 处理ECDH密钥请求
                ble_client_ident.status = identify_request;
                ble_client_ident.rst_first = false;
            } else {
                ble_client_ident.status = ECDH_key_check; // 进入ECDH密钥验证阶段
                if (!ble_client_ecdh_key_rsp()) { // 发送ECDH密钥请求回应
                    LOG_I("ble_client_ecdh_key_rsp error!");
                    return;
                }
                ble_client_ident.status = ECDH_key_rsp; // 进入ECDH密钥验证阶段
            }
        } else if (ble_client_ident.status == ECDH_key_rsp) {
            LOG_I("resend ECDH key request!");
            if (!ble_client_ecdh_key_rsp()) { // 发送ECDH密钥请求回应
                LOG_I("ble_client_ecdh_key_rsp error!");
                return;
            }
        }
    } else if (data[0] == 0x00 && data[1] == 0x07) { // 收到鉴权结果
        if (ble_client_ident.status == ECDH_key_rsp) {
            if (!ble_client_ecdh_key_check(data, len)) { // 处理ECDH密钥验证结果
                ble_client_ident.status = identify_request;
                ble_client_ident.rst_second = false;
                ble_client_ident.rst_first = false;
            } else {
                ble_client_ident.rst_second = true; // 鉴权结果检查成功
                LOG_I("indent ok, send some data!");
                ble_client_send_some_data();
                ble_client_ident.status = IDENTIFY_OK; // 进入下一步
            }
        } else if (ble_client_ident.status == IDENTIFY_OK) {
            LOG_I("resend identify ok rng data!");
            ble_client_send_some_data();
            LOG_I("indent ok, send some data! 222");
        }
    } else {
        LOG_I("client_identify_handler data error!");
        // gap_manager_disconnect(con_idx_client, 0x13); // 断开连接
        return;
    }
}

// * 解析接收的通知数据 */
static void client_recv_notification_parse(const uint8_t *data, uint16_t len) {
    // 加密连接需要鉴权
    if (client_encryption_required && !ble_client_ident.rst_second) {
        client_identify_handler(data, len);
    } else { // 非加密连接 或 加密连接通过鉴权
        // 如果加密，则解密后解析数据（ModBus格式）
        if (client_encryption_required) {
            uint8_t *temp_data = (uint8_t *)malloc(len);
            if (temp_data == NULL) {
                LOG_E("temp_data malloc failed!");
                return;
            }
            memset(temp_data, 0, len);
            memcpy(temp_data, data, len);
            uint16_t temp_len = data[0] << 8 | data[1];
            client_Aes_Cbc_decry(temp_data + 2, temp_len);
            free(temp_data);
        } else {
        }
    }
}

#endif // MASTER_CLIENT_ROLE == 1

/*鉴权处理函数*/
static void ble_identify_handler(void) {
    ble_data_t *ptr = &ble_data;
    switch (myident.status) {
    case identify_request: { /*请求*/
        BleSendBytes(pack_buf_first, 10);
#if Log_enable
        LOG_D("Request 01 data is:\r\n");
        for (uint8_t i = 0; i < sizeof(pack_buf_first); i++) {
            printf("%02x,", pack_buf_first[i]);
        }
        LOG_D("\r\n");
#endif
        /*等待300ms内若无响应重传5次*/
        iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(300)); /*设置定时器周期*/
        iot_timer_set_param(ble_resend_timer_id, &resend_index[0]); /*设置定时器回调参数*/
        iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/
        break;
    }
    case identify_verity: { /*验证*/
        memcpy(pack_buf, ptr->rx_buf, 10);
        if (identify_rst_check(pack_buf, myident.MD5_DATA)) {
            myident.rst_first = true;
            Pdc_Aes_key_first(myident.md5_rst);
        } else {
            myident.rst_first = false;
        }
        memset(ptr->rx_buf, 0, BLE_RX_SIZE);
        iot_timer_set_period(ble_identify_timer_id, MS_TO_TICKS(50)); /*设置定时器周期*/
        iot_timer_start_task(ble_identify_timer_id); /*开始定时器*/
        LOG_I("verity is done!");
        myident.status = identify_rst;
        ptr->rx_state = BLE_RX_IDLE;
        ptr->rx_size = 0;
        ptr->rx_offset = 0;
        break;
    }

    case identify_rst: { /*结果*/
        if (myident.rst_first == true) {
#if Log_enable
            LOG_I("identify_fisrt pass!\n");
#endif
            identify_rst_pack(pack_buf, IDENT_PASS);
            BleSendBytes(pack_buf, 7);
            myident.status = ECDH_key_request;
            iot_timer_set_period(ble_identify_timer_id, MS_TO_TICKS(20)); /*设置定时器周期 50ms ? */
            iot_timer_start_task(ble_identify_timer_id); /*开始定时器*/
        } else {
#if Log_enable
            LOG_I("identify_fisrt failed!\n");
#endif
            gap_manager_disconnect(ble_para.connect_id, 0x13);
        }
        break;
    }
    case ECDH_key_request: { /*密钥协商请求*/
        LOG_I("ECDH_key_request!");
        LOG_HEX(ECDH_Pack_ptr, 134);
        AES_BleSendBytes(ECDH_Pack_ptr, 134, false);
        /*等待300ms内若无响应重传5次*/
        iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(500)); /*设置定时器周期*/
        iot_timer_set_param(ble_resend_timer_id, &resend_index[1]); /*设置定时器回调参数*/
        iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/

        // printf("ECDH_Pack_ptr is:\r\n");
        // for (uint8_t i = 0; i < 134; i++) {
        //     printf("%02x,", pack_ecdh_sign_buf[i]);
        // }
        // printf("\r\n");

        // myident.status = test_sign;
        // builtin_timer_start(ble_identify_timer, 0, NULL); /*50ms*/
        break;
    }
    // case test_sign: { /*密钥协商请求*/
    //     uint8_t pack_ecdh_sign_buf[128];
    //     uint8_t sign_buf[64];
    //     OS_sign_test(sign_buf, pack_ecdh_sign_buf, myident.md5_rst);
    //     break;
    // }
    case ECDH_key_check: { /*密钥协商验证*/
        uint16_t text_len = ((uint16_t)ptr->rx_buf[0] << 8) + ptr->rx_buf[1];
        Aes_Ecb_decry(&ptr->rx_buf[2], myident.md5_rst, text_len);
        if (ECDH_key_rst_check(ptr->rx_buf + 2, myident.md5_rst)) {
            myident.rst_second = true;
            LOG_I("verity is done!");
        } else {
            LOG_I("RUN HERE_3!");
        }
        iot_timer_set_period(ble_identify_timer_id, MS_TO_TICKS(50)); /*设置定时器周期*/
        iot_timer_start_task(ble_identify_timer_id); /*开始定时器*/

        myident.status = ECDH_key_rsp;
        ptr->rx_state = BLE_RX_IDLE;
        ptr->rx_size = 0;
        ptr->rx_offset = 0;
        break;
    }
    case ECDH_key_rsp: { /*密钥协商结果*/
        if (myident.rst_second == true) {
#if Log_enable
            LOG_I("identify_second pass!");
#endif
            ECDH_key_rsp_pack(pack_buf, IDENT_PASS);
            ECDH_key_rsp_flag = true;
            AES_BleSendBytes(pack_buf, 7, false);
            iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(5000)); /*设置定时器周期*/
            iot_timer_set_param(ble_resend_timer_id, &resend_index[2]); /*设置定时器回调参数*/
            iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/

        } else {
#if Log_enable
            LOG_I("identify_second failed!");
#endif
            gap_manager_disconnect(ble_para.connect_id, 0x13);
        }
        break;
    }
    default:
        break;
    }
}

/* 定时器回调函数 */
static void ble_identify_timeout_cb(void *param) { /* 鉴权处理 */
    // identify_handr_t *ptr = (identify_handr_t *)param;
    iot_timer_stop_task(ble_identify_timer_id); /* 停止定时器 */
    LOG_I("status is %x", myident.status);
    ble_identify_handler();
}

/* 蓝牙数据接收 */
static void ble_receive_timeout_cb(void *param) {
    ble_data_t *pctx = &ble_data;
    iot_timer_stop_task(ble_receive_timer_id); /* 停止定时器 */
    pctx->rx_state = BLE_RX_CPMPLATE; /* 接收完成标志 */

    // ServiceSendBytes(pctx->rx_buf, pctx->rx_size); /* 串口发送 */
    pctx->rx_state = BLE_RX_IDLE;
    pctx->rx_size = 0;
    pctx->rx_offset = 0;
    // LOG_I("ble_receive_timeout");
    // LOG_I("len:%d",pctx->rx_size);
}

static void ble_self_timeout_cb(void *param) {
    ble_data_t *pctx = &ble_data;
    iot_timer_stop_task(ble_self_timer_id);
    pctx->self_rx_state = BLE_RX_CPMPLATE; /* 接收完成标志 */
    // LOG_I("self_ble_receive_timeout");
    // LOG_I("len:%d",pctx->self_rx_size);
}

/* 重发 */
static void ble_resend_timeout_cb(void *param) {
    iot_timer_stop_task(ble_resend_timer_id);
    uint8_t *index = param;
    switch (*index) {
    case 1: {
        BleSendBytes(pack_buf_first, 10);
        resend_flag++;
        if (resend_flag < 5) {
            iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(200)); /*设置定时器周期*/
            iot_timer_set_param(ble_resend_timer_id, &resend_index[0]); /*设置定时器回调参数*/
            iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/
        } else if (resend_flag == 5) {
            iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(500)); /*设置定时器周期*/
            iot_timer_set_param(ble_resend_timer_id, &resend_index[0]); /*设置定时器回调参数*/
            iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/
        } else {
            myident.status = identify_request;
            if (ble_para.connect_status == true) {
                gap_manager_disconnect(ble_para.connect_id, 0x13);
            }
        }
        break;
    }
    case 2: {
        AES_BleSendBytes(ECDH_Pack_ptr, 134, false);
        resend_flag++;
        if (resend_flag < 5) {
            iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(500)); /*设置定时器周期*/
            iot_timer_set_param(ble_resend_timer_id, &resend_index[1]); /*设置定时器回调参数*/
            iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/

        } else if (resend_flag == 5) {
            iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(500)); /*设置定时器周期*/
            iot_timer_set_param(ble_resend_timer_id, &resend_index[1]); /*设置定时器回调参数*/
            iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/
        } else {
            myident.status = identify_request;
            if (ble_para.connect_status == true) {
                gap_manager_disconnect(ble_para.connect_id, 0x13);
            }
        }
        break;
    }
    case 3: {
        AES_BleSendBytes(pack_buf, 7, false);
        resend_flag++;
        if (resend_flag < 5) {
            iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(200)); /*设置定时器周期*/
            iot_timer_set_param(ble_resend_timer_id, &resend_index[2]); /*设置定时器回调参数*/
            iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/
        } else if (resend_flag == 5) {
            iot_timer_set_period(ble_resend_timer_id, MS_TO_TICKS(500)); /*设置定时器周期*/
            iot_timer_set_param(ble_resend_timer_id, &resend_index[2]); /*设置定时器回调参数*/
            iot_timer_start_task(ble_resend_timer_id); /*开始定时器*/
        } else {
            myident.status = identify_request;
            if (ble_para.connect_status == true) {
                gap_manager_disconnect(ble_para.connect_id, 0x13);
            }
        }
        break;
    }
    default:
        break;
    }
}

static void adv_recv_timeout_cb(void *param) {
    magnetic_screen_inv_offline();
}

/*创建定时器*/
static void create_ble_receive_timer(void) {
    ble_receive_timer_id = iot_timer_add_task_ms(50, false, ble_receive_timeout_cb, NULL);
    ble_self_timer_id = iot_timer_add_task_ms(50, false, ble_self_timeout_cb, NULL);
    ble_identify_timer_id = iot_timer_add_task_ms(50, false, ble_identify_timeout_cb, NULL);
    ble_resend_timer_id = iot_timer_add_task_ms(50, false, ble_resend_timeout_cb, NULL);
    adv_recv_timeout_timer_id = iot_timer_add_task_ms(40 * 1000, false, adv_recv_timeout_cb, NULL);
}

/*主动断开连接*/
void disconnect_ble(void) {
    gap_manager_disconnect(ble_para.connect_id, 0x13);
}

/*删除设备所有绑定*/
void init_bond(void) {
    disconnect_ble();
    uint8_t len = gap_manager_get_bonded_dev_num();
    LOG_I("current bond numbers: %d", len);
    if (len) {
        for (uint8_t i = 0; i < len; i++) {
            gap_manager_delete_bonding(i);
        }
    }
}

/*修改扫描回应包*/
void modify_scanRspData(uint8_t offset, uint8_t *value, uint8_t len) {
    memcpy(scanRspData + offset, value, len);
}

/*添加扫描回应包名称*/
void add_scanRespon_name(uint8_t *name, uint8_t len) {
    uint8_t adv_type = 0x09;
    uint8_t scanRespon_len;
    scanRespon_len = len + 1;
    memset(scanRspData, 0, sizeof(scanRspData));
    modify_scanRspData(0, &scanRespon_len, 1);
    modify_scanRspData(1, &adv_type, 1);
    modify_scanRspData(2, (uint8_t *)name, len);
}
/*修改广播包*/
void modify_advertData(uint8_t offset, uint8_t *value, uint8_t len) {
    memcpy(advertData + offset, value, len);
}
/*添加广播包本机蓝牙Mac地址（针对IOS系统没法显示Mac的问题）*/
void add_adverData_mac(void) {
    modify_advertData(6, ble_para.ble_mac, 6);
}
/*根据type和devicesn生成蓝牙扫描回应名称*/
// void Ble_SetName(uint8_t *type, uint64_t deviceSn) {
//     memset((char *)ble_para.device_name, 0, sizeof(ble_para.device_name));
//     snprintf((char *)ble_para.device_name, sizeof(ble_para.device_name), "%s%llu", type,
//     deviceSn); add_scanRespon_name(ble_para.device_name, strlen((char *)ble_para.device_name));
// }

void Ble_SetName(char *name) {
    memset((char *)ble_para.device_name, 0, sizeof(ble_para.device_name));
    memcpy((char *)ble_para.device_name, name, sizeof(ble_para.device_name));
    add_scanRespon_name(ble_para.device_name, strlen((char *)ble_para.device_name));
}

static void start_adv(void) {
    LS_ASSERT(adv_obj_hdl != 0xff);

    dev_manager_start_adv(
        adv_obj_hdl, advertData, sizeof(advertData), scanRspData, sizeof(scanRspData));
}

static void ble_adv_timeout_cb(void *param) {
    if (adv_obj_hdl != 0xff) {
        dev_manager_stop_adv(adv_obj_hdl);
    }

    iot_timer_stop_task(ble_adv_timeout_timer_id);
    iot_timer_remove_task(ble_adv_timeout_timer_id);
    ble_adv_timeout_timer_id = 0xFF;
    magnetic_screen_set_ble_connect_status(BLE_DIS_CONN);
}

// 手动中止adv()广播
void ble_adv_abort(void) {
    ble_adv_timeout_cb(NULL);
}

void key_2_start_adv() {
    set_fast_match_mode(false);
    start_adv();
    ble_adv_timeout_timer_id = iot_timer_add_task_ms(60 * 1000, false, ble_adv_timeout_cb, NULL);
    iot_timer_start_task(ble_adv_timeout_timer_id);
    magnetic_screen_set_ble_connect_status(BLE_CONNECTING);
}

static void fast_match_timeout_cb(void *param) {
    (void)param;
    set_fast_match_mode(false);
}

bool is_fast_match_mode(void) {
    return fast_match_mode_active;
}

bool is_ble_pairing_mode(void) {
    return get_app_ble_connect_status() == BLE_CONNECTING;
}

void set_fast_match_mode(bool enable) {
    if (enable == fast_match_mode_active) {
        if (enable && fast_match_timeout_timer_id != 0xFF) {
            iot_timer_restart_task(fast_match_timeout_timer_id);
        }
        return;
    }

    fast_match_mode_active = enable;
    if (enable) {
        if (fast_match_timeout_timer_id == 0xFF) {
            fast_match_timeout_timer_id
                = iot_timer_add_task_ms(60 * 1000, false, fast_match_timeout_cb, NULL);
        }
        iot_timer_start_task(fast_match_timeout_timer_id);
        // 逆变器图标在长按3s时已开始闪烁，进入模式不再刷新，避免覆盖当前显示
    } else {
        if (fast_match_timeout_timer_id != 0xFF) {
            iot_timer_stop_task(fast_match_timeout_timer_id);
            iot_timer_remove_task(fast_match_timeout_timer_id);
            fast_match_timeout_timer_id = 0xFF;
        }
        update_icon_inv_con(); // 退出后按真实逆变器连接状态恢复图标
    }
}

void key_2_start_fast_match(void) {
    if (is_ble_pairing_mode()) {
        return; // 蓝牙匹配中不允许进入快配，两种模式互斥
    }
    set_fast_match_mode(true);
}

// static void start_adv_no_connect(void) {
//     LS_ASSERT(adv_obj_hdl != 0xff);
//     dev_manager_start_adv(adv_obj_hdl,
//                           adv_no_connect_data,
//                           sizeof(adv_no_connect_data),
//                           scanRspData,
//                           sizeof(scanRspData));
// }

/*修改扫描回应包名称*/
// void modify_scanRespon_name(uint8_t *type, uint64_t deviceSn) {
//     if (ble_para.adv_enable) { /*广播使能*/
//         dev_manager_stop_adv(adv_obj_hdl);
//         ; /*关闭广播*/
//     }
//     if (ble_para.connect_status) { /*如果蓝牙处于连接状态*/
//         disconnect_ble(); /*主动断开连接*/
//         ble_para.connect_status = false;
//     }
//     Ble_SetName(type, deviceSn);
//     start_adv(); /*打开广播*/
//     ble_para.adv_enable = true;
// }

/*修改扫描回应包名称*/
void modify_scanRespon_name(char *name) {
    if (ble_para.connect_status) { /*如果蓝牙处于连接状态*/
        disconnect_ble(); /*主动断开连接*/
        ble_para.connect_status = false;
    }
    Ble_SetName(name);
    if (ble_para.adv_enable) { /*广播使能*/
        dev_manager_stop_adv(adv_obj_hdl); /*关闭广播*/
        ble_para.adv_update_flag = true; /*标记更新广播*/
    }
}

void adv_switch(uint8_t enable) {
    if (!enable && ble_para.adv_enable) {
        dev_manager_stop_adv(adv_obj_hdl); /*关闭广播*/
        ble_para.adv_enable = false;
    } else if (enable && !ble_para.adv_enable) {
        start_adv(); /*开始广播*/
        ble_para.adv_enable = true;
    }
}

static void uart_server_data_length_update(uint8_t con_idx) {
    struct gap_set_pkt_size dlu_param = {
        .pkt_size = 251, /*payload长度*/
    };
    gap_manager_set_pkt_size(con_idx, &dlu_param);
}

static void create_adv_obj() {
    struct legacy_adv_obj_param adv_param = {
        .adv_intv_min = 0x32,
        .adv_intv_max = 0x32,
        .own_addr_type = PUBLIC_OR_RANDOM_STATIC_ADDR,
        .filter_policy = 0,
        .ch_map = 0x7, /*37、38、39通道广播*/
        .disc_mode = ADV_MODE_LIM_DISC,
        .prop = {
                 .connectable = 1,
                 .scannable = 1,
                 .directed = 0,
                 .high_duty_cycle = 0,
                 },
    };
    dev_manager_create_legacy_adv_object(&adv_param);
}

/*读服务处理*/
static void uart_server_read_req_ind(uint8_t att_idx, uint8_t con_idx) {
    uint16_t handle = 0;
    if (att_idx == BLE_UART_RX_CHAR_CFG) { /*notify开关*/
        // LOG_I("BLE_UART_RX_CHAR_CFG");
        handle = gatt_manager_get_svc_att_handle(&uart_server_svc_env, att_idx); /*获取句柄*/
        gatt_manager_server_read_req_reply(con_idx, handle, 0, (void *)&cccd_config, 2);
    } else if (att_idx == BLE_SELF_CHAR_CFG) {
        // LOG_I("BLE_UART_RX_CHAR_CFG");
        handle = gatt_manager_get_svc_att_handle(&uart_server_svc_env, att_idx); /*获取句柄*/
        gatt_manager_server_read_req_reply(con_idx, handle, 0, (void *)&cccd_self_config, 2);
    }
}

/*写服务处理*/
static void uart_server_write_req_ind(uint8_t att_idx,
                                      uint8_t con_idx,
                                      uint16_t length,
                                      uint8_t const *value) {
    ble_data_t *pctx = &ble_data;
    // LOG_I("att_idx:%d", att_idx);
    if (att_idx == BLE_UART_TX_VAL) {
        iot_timer_stop_task(ble_receive_timer_id); /*停止定时器*/
        // LOG_I("BLE_UART_TX_VAL");
        // LS_ASSERT(length <= BLE_RX_SIZE);
        /*接收数据*/
        if (pctx->rx_offset + length <= BLE_RX_SIZE) { /*一次性接收数据不超过最大值*/
            memcpy(pctx->rx_buf + pctx->rx_offset, value, length); /*复制数据到缓存中*/
            pctx->rx_offset += length; /*更新偏移*/
        } else {
            memcpy(pctx->rx_buf + pctx->rx_offset,
                   value,
                   BLE_RX_SIZE - pctx->rx_offset); /*复制数据填满缓存中，剩余丢弃*/
            pctx->rx_offset = BLE_RX_SIZE; /*更新偏移为最大值*/
        }
        if (length >= (ble_para.mtu_size - 3)) { /*根据MTU判断是否完成*/
            if (ble_para.mtu_size > DEFAULT_MTU) { /*判断是否已经支持修改了MTU(BLE4.2以上)*/
                ble_para.max_wait_time
                    = (1 + ble_para.latency) * ble_para.conn_int
                      + 180; /*为了兼容不标准的BLE4.2手机，需要加大超时时间等待中间的大包接收完成*/
            }
            /*重置定时器*/
            iot_timer_set_period(ble_receive_timer_id, MS_TO_TICKS(ble_para.max_wait_time));
            iot_timer_start_task(ble_receive_timer_id); /*启动定时器*/
        } else { /*小于MTU值，可认为最后的数据*/
            /*接收完成*/
            // LOG_I("ble recv complate!");
            pctx->rx_size = pctx->rx_offset;
            pctx->rx_state = BLE_RX_CPMPLATE; /*接收完成标志*/
            uint16_t text_len = 0;
            text_len = ((uint16_t)pctx->rx_buf[0] << 8) + pctx->rx_buf[1]; /*获取整包长度*/

            /*是否为鉴权事件，true-通过身份验证，false-未通过身份验证*/
            if (myident.rst_second != true) {
                if (pctx->rx_buf[2] == identify_verity) { // 第一步身份验证请求
                    // LOG_I("identify_key_check_evt!");
                    myident.status = identify_verity;

                    iot_timer_stop_task(ble_resend_timer_id);

                    iot_timer_set_period(ble_identify_timer_id, 50); // 0 ? 50ms
                    iot_timer_start_task(ble_identify_timer_id); /*开始定时器*/

                } else if (pctx->rx_buf[1] == 0x86) { // 第二步密钥协商验证请求
                    // LOG_I("ECDH_key_check_evt!");
                    myident.status = ECDH_key_check;

                    iot_timer_stop_task(ble_resend_timer_id);

                    iot_timer_set_period(ble_identify_timer_id, 50); // 0 ? 50ms
                    iot_timer_start_task(ble_identify_timer_id); /*开始定时器*/

                } else { /*end if(pctx->rx_buf[2]==identify_verity))*/ // 非法访问，断开连接
                    gap_manager_disconnect(ble_para.connect_id, 0x13);
                    pctx->rx_state = BLE_RX_IDLE;
                    pctx->rx_size = 0;
                    pctx->rx_offset = 0;
                }
            } else { /*end if(myident.rst_second != true))*/ // 加密通信
                // LOG_I("tongxin_evt!");
                if (myident.Fct_test != true) { /*加密通信模式*/
                    if (ECDH_key_rsp_flag == true) {
                        iot_timer_stop_task(ble_resend_timer_id);
                        ECDH_key_rsp_flag = false;
                    }
                    if (cmp_data_valid(text_len, pctx->rx_size)) { /*检查数据合法性*/
                        // LOG_I("Receive data is valid!");
                        // server端收到蓝牙数据，进行处理 包括配置与OTA，按原框架，去uart模块处理
                        ServiceSendBytes_Uart_buf(pctx->rx_buf, pctx->rx_size);
                    } else { /*end if(cmp_data_valid(text_len,pctx->rx_size))*/
                        LOG_I("Receive data is unvalid! [0..6]");
                        LOG_HEX(pctx->rx_buf, 6);
                    }
                    pctx->rx_state = BLE_RX_IDLE;
                    pctx->rx_size = 0;
                    pctx->rx_offset = 0;
                } else { /*end if(myident.Fct_test != true),工厂测试模式*/
                    char *p;
                    p = (char *)pctx->rx_buf;
                    if (strncmp((char *)"\r\r\n+BLE_MODLE_TEST\n", p, 19) == 0) { // 过滤其他信息
                        // ServiceSendBytes(pctx->rx_buf, pctx->rx_size);
                    } else {
                        myident.Fct_test = false;
                        LOG_I("Fct_test !!");
                        gap_manager_disconnect(ble_para.connect_id, 0x13);
                    }
                    pctx->rx_state = BLE_RX_IDLE;
                    pctx->rx_size = 0;
                    pctx->rx_offset = 0;
                }
            }
        }

    } else if (att_idx == BLE_UART_RX_CHAR_CFG) { /*通知配置*/
        // LOG_I("write BLE_UART_RX_CHAR_CFG");
        // LOG_I("BLE_UART_RX_CHAR_CFG :%x", BLE_UART_RX_CHAR_CFG);
        LS_ASSERT(length == 2);
        memcpy(&cccd_config, value, length);
        if (cccd_config == 0x0000) {
            ble_para.can_send_notify = false;
            LOG_I("disable notify");
        } else if (cccd_config == 0x0001) {
            if (myident.Fct_test == true && myident.rst_second != true) {
                myident.rst_second = true;
            }
            if (myident.Fct_test != true && myident.rst_second != true) {
                myident.status = identify_request;
                iot_timer_set_period(ble_identify_timer_id, 50);
                iot_timer_start_task(ble_identify_timer_id); /*开始定时器*/
            }
            ble_para.can_send_notify = true;
            LOG_I("enable notify");
        } else if (cccd_config == 0x0002) { /*使能indicate*/
            ble_para.can_send_notify = true;
            LOG_I("enable indicate");
        }
        // LOG_I("cccd_config:%d",cccd_config);
    } else if (att_idx == BLE_SET_VAL) {
        // LOG_I("BLE_SET_VAL");
        // LOG_I("get len:%d", length);
    } else if (att_idx == BLE_SELF_VAL) {
        iot_timer_stop_task(ble_self_timer_id); /*停止定时器*/
        // LOG_I("BLE_SELF_VAL");
        // LOG_I("get len:%d", length);
        LS_ASSERT(length <= BLE_RX_SIZE);
        /*接收数据*/
        memcpy(pctx->self_rx_buf + pctx->self_rx_offset, value, length); /*复制数据到缓存中*/
        pctx->self_rx_offset += length; /*更新偏移*/
        if (length >= (ble_para.mtu_size - 3)) { /*根据MTU判断是否完成*/
            /*重置定时器*/
            iot_timer_set_period(ble_self_timer_id, MS_TO_TICKS(ble_para.max_wait_time));
            iot_timer_start_task(ble_self_timer_id); /*启动定时器*/
        } else {
            /*接收完成*/
            // LOG_I("ble recv complate!");
            pctx->self_rx_size = pctx->self_rx_offset;
            pctx->self_rx_state = BLE_RX_CPMPLATE; /*接收完成标志*/
        }

    } else if (att_idx == BLE_SELF_CHAR_CFG) {
        // LOG_I("BLE_SELF_CHAR_CFG");
        LS_ASSERT(length == 2);
        memcpy(&cccd_self_config, value, length);
        if (cccd_self_config == 0x0000) {
            ble_para.self_can_send_notify = false;
            // LOG_I("disable notify");
        } else if (cccd_self_config == 0x0001) {
            ble_para.self_can_send_notify = true;
            // LOG_I("enable notify");
        } else if (cccd_self_config == 0x0002) { /*使能indicate*/
            ble_para.self_can_send_notify = true;
            // LOG_I("enable notify");
        }
        // LOG_I("cccd_self_config:%d",cccd_self_config);
    }
}

/*notify数据到手机*/
static void notify_data_to_app(void) {
    uint16_t tx_len = 0;
    ble_data_t *pctx = &ble_data;
    uint16_t max_data_num = ble_para.mtu_size - 3;
    if (ble_para.can_send_notify) { /*判断notify开关是否打开*/
        if (pctx->tx_state == BLE_TX_CPMPLATE && pctx->tx_size) { /*判断是否还有数据*/
            uint16_t handle = gatt_manager_get_svc_att_handle(&uart_server_svc_env,
                                                              BLE_UART_RX_CHAR_VAL); /*获取句柄*/
            if (pctx->tx_offset == pctx->tx_size) { /*判断数据是否全部发送完成*/
                // LOG_I("notify complate");
                pctx->tx_state = BLE_TX_IDLE; /*蓝牙发送空闲*/
                pctx->tx_offset = 0;
                pctx->tx_size = 0;
            } else {
                // LOG_I("!!tx_len：%d,offset:%d", pctx->tx_size, pctx->tx_offset);
                tx_len = ((pctx->tx_size - pctx->tx_offset) > max_data_num)
                             ? max_data_num
                             : pctx->tx_size - pctx->tx_offset;
                ble_para.current_notify = 1;
                gatt_manager_server_send_notification(
                    ble_para.connect_id, handle, pctx->tx_buf + pctx->tx_offset, tx_len, NULL);
                pctx->tx_offset += tx_len;
                // LOG_I("notify uncomplate!");
                // LOG_I("tx_len：%d,offset:%d", tx_len, pctx->tx_offset);
                // LOG_I("tx_offset%d", pctx->tx_offset);
            }
        }
    } else {
        // LOG_I("notify is closed!");
        pctx->tx_state = BLE_TX_IDLE;
        pctx->tx_offset = 0;
        pctx->tx_size = 0;
    }
}

static void self_notify_data_to_app(void) {
    ble_data_t *pctx = &ble_data;
    uint16_t tx_len = 0;
    uint16_t max_data_num = ble_para.mtu_size - 3;
    if (ble_para.self_can_send_notify) { /*判断notify开关是否打开*/
        if (pctx->self_tx_state == BLE_TX_CPMPLATE && pctx->self_tx_size) { /*判断是否还有数据*/
            uint16_t handle
                = gatt_manager_get_svc_att_handle(&uart_server_svc_env, BLE_SELF_VAL); /*获取句柄*/
            if (pctx->self_tx_offset == pctx->self_tx_size) { /*判断数据是否全部发送完成*/
                // LOG_I("notify complate!");
                pctx->self_tx_state = BLE_TX_IDLE; /*蓝牙发送空闲*/
                pctx->self_tx_offset = 0;
                pctx->self_tx_size = 0;
            } else {
                tx_len = ((pctx->self_tx_size - pctx->self_tx_offset) > max_data_num)
                             ? max_data_num
                             : pctx->self_tx_size - pctx->self_tx_offset;
                ble_para.current_notify = 2;
                gatt_manager_server_send_notification(ble_para.connect_id,
                                                      handle,
                                                      pctx->self_tx_buf + pctx->self_tx_offset,
                                                      tx_len,
                                                      NULL);
                pctx->self_tx_offset += tx_len;
            }
        }
    } else {
        // LOG_I("notify is closed!");
        pctx->self_tx_state = BLE_TX_IDLE;
        pctx->self_tx_offset = 0;
        pctx->self_tx_size = 0;
    }
}

/*校验加密数据合法性*/
int cmp_data_valid(uint16_t plaintxet_len, uint16_t rx_buf_size) {
    uint16_t cmp1 = 0;
    if (plaintxet_len % 16 != 0) {
        cmp1 = (plaintxet_len / 16 + 1) * 16 + 6; //+6表示明文长度的两字节+四个随机数
    } else {
        cmp1 = plaintxet_len + 6;
    }
    // LOG_I("plaintxet_len is：%d ", cmp1);
    // LOG_I("rx_buf_size is: %d ", rx_buf_size);
    if (cmp1 == rx_buf_size) {
        return 1;
    } else {
        return 0;
    }
}

/*发送加密数据到蓝牙*/
uint8_t AES_BleSendBytes(uint8_t *data, uint16_t len, bool type_aes) {
    ble_data_t *pctx = &ble_data;
    uint16_t aes_len;

    memset(pctx->tx_buf, 0, BLE_TX_SIZE); // 第一时间清零
    pctx->tx_buf[0] = len >> 8;
    pctx->tx_buf[1] = (uint8_t)len;
    if (type_aes == true) { // 判断加密方式为CBC第一阶段加密还是CBC第二阶段
        memcpy(&pctx->tx_buf[6],
               data,
               len); // 拷贝到蓝牙发送缓存(前两位空出装长度，3-6位空出装随机数,后接密文)
        aes_len = Aes_Cbc_encry(&pctx->tx_buf[2], len); // AES_cbc第二阶段加密
        pctx->tx_size = aes_len + 6;
    } else { // CBC第一阶段加密
        memcpy(&pctx->tx_buf[2], data, len); // 拷贝到蓝牙发送缓存(前两位空出装长度，后接密文)
        aes_len = Aes_Ecb_encry(&pctx->tx_buf[2], myident.md5_rst, len); // AES_cbc第一阶段加密
        pctx->tx_size = aes_len + 2;
    }
    pctx->tx_state = BLE_TX_CPMPLATE; /*标记数据需要notify发送给手机*/
    // printf("pctx->tx_size = %d\n", pctx->tx_size);
    notify_data_to_app(); /*启动notify到手机*/
    return 0;
}

/*发送数据到蓝牙*/
uint8_t BleSendBytes(uint8_t *data, uint16_t len) {
    ble_data_t *pctx = &ble_data;
    // LOG_I("ble send state:%d\n",pctx->tx_state);
    if (pctx->tx_state == BLE_TX_IDLE && len < BLE_TX_SIZE) { /*检查发送是否空闲*/
        memcpy(pctx->tx_buf, data, len);
        pctx->tx_size = len;
        pctx->tx_state = BLE_TX_CPMPLATE; /*标记数据需要notify发送给手机*/
        // LOG_I("notify_data_to_app");
        notify_data_to_app(); /*启动notify到手机*/
        return 0;
    }
    return 1;
}

/*获取ble数据*/
uint8_t *BleRecvBytes(uint16_t *msgLen) {
    ble_data_t *pctx = &ble_data;
    if (pctx->rx_state == BLE_RX_CPMPLATE) { // send finish and read data
        pctx->rx_state = BLE_RX_IDLE;
        *msgLen = pctx->rx_size;
        pctx->rx_size = 0;
        pctx->rx_offset = 0;
        return pctx->rx_buf;
    }
    return NULL;
}

/*发送数据到蓝牙,借用通道FF03*/
uint8_t self_BleSendBytes(uint8_t *data, uint16_t len) {
    ble_data_t *pctx = &ble_data;
    uint16_t aes_len;

    // LOG_I("ble send state:%d\n",pctx->tx_state);
    memset(pctx->self_tx_buf, 0, 300); // 第一时间清零
    pctx->self_tx_buf[0] = len >> 8;
    pctx->self_tx_buf[1] = (uint8_t)len;
    if (pctx->self_tx_state == BLE_TX_IDLE && len < BLE_TX_SIZE) { /*检查发送是否空闲*/
        memcpy(&pctx->self_tx_buf[6],
               data,
               len); // 拷贝到蓝牙发送缓存(前两位空出装长度，3-6位空出装随机数,后接密文)
        aes_len = Aes_Cbc_encry(&pctx->self_tx_buf[2], len); // AES_cbc第二阶段加密
        pctx->self_tx_size = aes_len + 6;

        pctx->self_tx_state = BLE_TX_CPMPLATE; /*标记数据需要notify发送给手机*/
        // LOG_I("notify_data_to_app");
        self_notify_data_to_app(); /*启动notify到手机*/
        return 0;
    }
    return 1;
}

/*获取ble数据*/
uint8_t *self_BleRecvBytes(uint16_t *msgLen) {
    ble_data_t *pctx = &ble_data;
    if (pctx->self_rx_state == BLE_RX_CPMPLATE) { // send finish and read data
        pctx->self_rx_state = BLE_RX_IDLE;
        *msgLen = pctx->self_rx_size;
        pctx->self_rx_size = 0;
        pctx->self_rx_offset = 0;
        return pctx->self_rx_buf;
    }
    return NULL;
}

/*获取设备名称*/
static void server_get_dev_name(struct gap_dev_info_dev_name *dev_name_ptr, uint8_t con_idx) {
    LS_ASSERT(dev_name_ptr);
    dev_name_ptr->value = ble_para.device_name;
    dev_name_ptr->length = strlen((char *)ble_para.device_name);
}

static void server_get_appearance(struct gap_dev_info_appearance *dev_appearance_ptr,
                                  uint8_t con_idx) {
    LS_ASSERT(dev_appearance_ptr);
    dev_appearance_ptr->appearance = 0;
}

static void server_get_slv_pref_param(struct gap_dev_info_slave_pref_param *dev_slv_pref_param_ptr,
                                      uint8_t con_idx) {
    LS_ASSERT(dev_slv_pref_param_ptr);
    dev_slv_pref_param_ptr->con_intv_min = 0x0c;
    dev_slv_pref_param_ptr->con_intv_max = 0x0c;
    dev_slv_pref_param_ptr->slave_latency = 0;
    dev_slv_pref_param_ptr->conn_timeout = 400;
}

bool get_ble_status(void) {
    return ble_para.connect_status;
}

/*GAP层回调函数*/
static void gap_manager_callback(enum gap_evt_type type, union gap_evt_u *evt, uint8_t con_idx) {
    // LOG_D("gap_manager_callback con_idx:%d", con_idx);
    // LOG_D("gap_manager_callback type:%d", type);
    switch (type) {
    case CONNECTED: { /*连接*/
        if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_SLAVE) { // 做为server服务端
#if IOT_BOUD_ENABLE
            uint8_t peer_id_cont = gap_manager_get_bonding_peer_id(con_idx);
            LOG_I("CONNECT Peer_id = %d", peer_id_cont);
            // if (peer_id_cont == 0xff) {
            //     gap_manager_slave_security_req(con_idx, 0x04);
            // }
#endif
            ble_para.connect_id = con_idx;
            ble_para.connect_status = true;
            LOG_I("connected!");
            ble_para.adv_enable = false; // 已经连接成功，停止广播
            ble_para.conn_prar_update_flag = false;

            // 连接成功后，停止并删除adv超时timer
            iot_timer_stop_task(ble_adv_timeout_timer_id);
            iot_timer_remove_task(ble_adv_timeout_timer_id);
            ble_adv_timeout_timer_id = 0xFF;

            magnetic_screen_set_ble_connect_status(BLE_CONNECTED);
            // LOG_I("try to update
            // conn_para,min:%d,max:%d,lat:%d,timeout:%d",conn_param.intv_min,conn_param.intv_max,conn_param.latency,conn_param.sup_timeout);
            // gap_manager_update_conn_param(con_idx, &conn_param);
            // send_at_indicate(IM_CONN); /*发送连接指示*/
            // auxiliary_power_pin_ctrl(1); /*输出高电平，控制辅源*/
            myident.rst_second = false;
        }
#if MASTER_CLIENT_ROLE == 1
        if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_MASTER) { // 做为client
            LOG_I("client connected!!!");
            LS_ASSERT(con_idx_client == 0xff);
            magnetic_screen_set_inv_connect_status(BLE_CONNECTED);
            con_idx_client = con_idx;
            ble_client_ident.status = identify_request; // 连接成功后，开始身份验证
            ble_client_ident.rst_first = false;
            ble_client_ident.rst_second = false;
            gatt_manager_client_mtu_exch_send(con_idx);
        }
#endif // MASTER_CLIENT_ROLE == 1
        break;
    }
    case DISCONNECTED: /*断开连接*/
        if (CONNECTION_IS_SERVER(con_idx)) {
            ble_para.connect_id = CON_IDX_INVALID_VAL;
            ble_para.mtu_size = DEFAULT_MTU; /*恢复默认MTU为23*/
            ble_para.max_wait_time = 60;
            ble_para.connect_status = false;
            ble_para.can_send_notify = false;
            ble_para.self_can_send_notify = false;
            ble_para.conn_prar_update_flag = false;
            cccd_config = 0;
            cccd_self_config = 0;
            LOG_I("disconnected,reason:%x", evt->disconnected.reason);
            magnetic_screen_set_ble_connect_status(BLE_DIS_CONN);
            // Ecc_Make_key(ECDH_Pack_ptr, pack_buf_first, myident.md5_rst, myident.MD5_DATA);
            resend_flag = 0;

            // start_adv(); /*开始广播*/ // adv 不自动开启
            // send_at_indicate(IM_DISC); /*发送断开连接指示*/
            // auxiliary_power_pin_ctrl(0); /*输出低电平，控制辅源*/
        } else if (CONNECTION_IS_CLIENT(con_idx)) {
#if MASTER_CLIENT_ROLE == 1
            LOG_D("client --------------------> disconnected,reason:%x", evt->disconnected.reason);
            con_idx_client = 0xff; // 重置连接索引
            // init_status = INIT_IDLE;
            ble_client_ident.status = identify_request; // 重置认证状态
            ble_client_ident.rst_first = false;
            ble_client_ident.rst_second = false;
            magnetic_screen_set_inv_connect_status(BLE_DIS_CONN);

            Ecc_Make_key(ECDH_Pack_ptr, pack_buf_first, myident.md5_rst, myident.MD5_DATA);

            ble_client_init();
            start_scan(); // 重新扫描
#endif // MASTER_CLIENT_ROLE == 1
        }
        break;
    case CONN_PARAM_REQ: /*参数请求*/
        // LOG_I("CONN_PARAM_REQ");
        break;
    case CONN_PARAM_UPDATED: /*手机APP申请更新新的连接参数*/
        if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_SLAVE) { // 做为server服务端
            ble_para.conn_int = evt->conn_param_updated.con_interval;
            ble_para.latency = evt->conn_param_updated.con_latency;
            // LOG_I("CONN_PARAM_UPDATED");
            // LOG_I(
            //     "conn_param_updated,intv:%d latency:%d sup_to:%d",
            //     evt->conn_param_updated.con_interval,
            //     evt->conn_param_updated.con_latency,
            //     evt->conn_param_updated.sup_to
            // );
            ble_para.max_wait_time = (1 + ble_para.latency) * ble_para.conn_int * 5 / 4 + 50;
            if (!ble_para.conn_prar_update_flag) { /*判断更新连接参数标志*/
                // LOG_I("try to update
                // conn_para,min:%d,max:%d,lat:%d,timeout:%d",conn_param.intv_min,conn_param.intv_max,conn_param.latency,conn_param.sup_timeout);
                /*本来更新连接参数可以在CONNECTED下执行，实际测试从机没法正常更新连接参数，反而会出现断开连接情况，所以在此调用一次即可。在此函数LL层发起更新参数请求。*/
                gap_manager_update_conn_param(con_idx, &conn_param); /*申请更新连接参数*/
                ble_para.conn_prar_update_flag = true; /*标志置1*/
                // LOG_I("try to update
                // conn_para,min:%d,max:%d,lat:%d,timeout:%d",conn_param.intv_min,conn_param.intv_max,conn_param.latency,conn_param.sup_timeout);
            }
        } else if (CONNECTION_IS_CLIENT(con_idx)) { // 做为client
#if MASTER_CLIENT_ROLE == 1
            LOG_D("conn_param_updated,intv:%d latency:%d sup_to:%d",
                  evt->conn_param_updated.con_interval,
                  evt->conn_param_updated.con_latency,
                  evt->conn_param_updated.sup_to);
#endif // MASTER_CLIENT_ROLE == 1
        }
        break;
    case MASTER_PAIR_REQ: // 4
                          // LOG_I("MASTER_PAIR_REQ");
#if IOT_BOUD_ENABLE
        gap_manager_slave_pair_response_send(ble_para.connect_id, true, &test_feat);
        // gap_manager_passkey_input(ble_para.connect_id,&test_passkey);
#endif
        break;
    case SLAVE_SECURITY_REQ: // 5
        // LOG_I("SLAVE_SECURITY_REQ");
        break;

    case PAIR_DONE: // 6
        // LOG_I(
        //     "PAIR_DONE reason :%d , success :%d",
        //     evt->pair_done.u.fail_reason,
        //     evt->pair_done.succeed
        // );
        break;

    case ENCRYPT_DONE: // 7
                       // LOG_I("ENCRYPT_DONE");
#if IOT_BOUD_ENABLE
        if (!ble_para.conn_prar_update_flag) { /*判断更新连接参数标志*/
            LOG_I("try to update conn_para,min:%d,max:%d,lat:%d,timeout:%d",
                  conn_param.intv_min,
                  conn_param.intv_max,
                  conn_param.latency,
                  conn_param.sup_timeout);
            /*本来更新连接参数可以在CONNECTED下执行，实际测试从机没法正常更新连接参数，反而会出现断开连接情况，所以在此调用一次即可。在此函数LL层发起更新参数请求。*/
            gap_manager_update_conn_param(con_idx, &conn_param); /*申请更新连接参数*/
            // ble_para.conn_prar_update_flag = true; /*标志置1*/
        }
#endif
        break;

    case DISPLAY_PASSKEY: // 8
                          // LOG_I("DISPLAY_PASSKEY");
#if IOT_BOUD_ENABLE
                          // LOG_I("passkey=%d", evt->display_passkey.passkey.pin);
                          // LOG_I("DISPLAY_PASSKEY");
                          // gap_manager_passkey_input(ble_para.connect_id, &test_passkey);
#endif
        break;

    case REQUEST_PASSKEY: // 9
                          // LOG_I("REQUEST_PASSKEY");
#if IOT_BOUD_ENABLE
        // gap_manager_passkey_input(ble_para.connect_id, &test_passkey);
#endif
        break;

    case NUMERIC_COMPARE: // 10
        // LOG_I("NUMERIC_COMPARE");
        // gap_manager_numeric_compare_set(hid_connect_id,true);
        break;
    case GET_DEV_INFO_DEV_NAME: /*获取设备名字*/
        // LOG_I("GET_DEV_INFO_DEV_NAME");
        server_get_dev_name((struct gap_dev_info_dev_name *)evt, con_idx);
        break;
    case GET_DEV_INFO_APPEARANCE:
        // LOG_I("GET_DEV_INFO_APPEARANCE");
        server_get_appearance((struct gap_dev_info_appearance *)evt, con_idx);
        break;
    case GET_DEV_INFO_SLV_PRE_PARAM: /*获取设备信息的从属首选参数*/
        // LOG_I("GET_DEV_INFO_SLV_PRE_PARAM");
        server_get_slv_pref_param((struct gap_dev_info_slave_pref_param *)evt, con_idx);
        break;
    default:

        break;
    }
}

/*GATT层回调函数*/
static void gatt_manager_callback(enum gatt_evt_type type, union gatt_evt_u *evt, uint8_t con_idx) {
    bool disconnected = false;
    // LOG_D("gatt_manager_callback con_idx:%d", con_idx);
    // LOG_D("gatt_manager_callback type:%d", type);

    if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_SLAVE) { // 做为server服务端
        disconnected = ble_para.connect_id == CON_IDX_INVALID_VAL;
    }
#if MASTER_CLIENT_ROLE == 1
    if (gap_manager_get_role(con_idx) == LS_BLE_ROLE_MASTER) { // 做为client
        disconnected = con_idx_client == CON_IDX_INVALID_VAL;
    }
#endif // MASTER_CLIENT_ROLE == 1
    if (disconnected) {
        LOG_W("receive gatt msg when disconnected!");
        return; // 连接断开，直接返回
    }

    switch (type) {
    case SERVER_READ_REQ: /*读服务*/
        // LOG_I("read req");
        uart_server_read_req_ind(evt->server_read_req.att_idx, con_idx);
        break;
    case SERVER_WRITE_REQ: /*写服务*/
        // LOG_I("write req");
        uart_server_write_req_ind(evt->server_write_req.att_idx,
                                  con_idx,
                                  evt->server_write_req.length,
                                  evt->server_write_req.value);
        break;
    case SERVER_NOTIFICATION_DONE: /*单次notify完成*/
        // LOG_I("ntf done");
        if (ble_para.current_notify == 1) {
            ble_para.current_notify = 0;
            notify_data_to_app();
        } else if (ble_para.current_notify == 2) {
            ble_para.current_notify = 0;
            self_notify_data_to_app();
        }
        break;
    case MTU_CHANGED_INDICATION: /*MTU交换r*/
        if (CONNECTION_IS_SERVER(con_idx)) {
            ble_para.mtu_size = evt->mtu_changed_ind.mtu; /*当前MTU*/
            if (ble_para.mtu_size > MAX_MTU) {
                ble_para.mtu_size = MAX_MTU; /*限制最大为247*/
                // LOG_I("mtu: %d", ble_para.mtu_size);
            }
            if (!ble_para.conn_prar_update_flag) { /*判断更新连接参数标志*/
                gap_manager_update_conn_param(con_idx, &conn_param); /*申请更新连接参数*/
                ble_para.conn_prar_update_flag = true; /*标志置1*/
            }
            uart_server_data_length_update(con_idx);
        } else if (CONNECTION_IS_CLIENT(con_idx)) {
#if MASTER_CLIENT_ROLE == 1
            ble_client_mtu = evt->mtu_changed_ind.mtu; /*当前MTU*/
            if (ble_client_mtu > MAX_MTU) {
                ble_client_mtu = MAX_MTU; /*限制最大为247*/
                // LOG_I("mtu: %d", ble_client_mtu);
            }
            // 发起服务发现
            gatt_manager_client_svc_discover_by_uuid(
                con_idx, (uint8_t *)&ble_uart_svc_uuid[0], UUID_LEN_16BIT, 1, 0xffff);
#endif // MASTER_CLIENT_ROLE == 1
        }
        break;

#if MASTER_CLIENT_ROLE == 1 // * 这里都是client相关的分支
        // * 这里区分是否加密，以及处理数据或透传的逻辑，并处理认证逻辑
    case CLIENT_RECV_NOTIFICATION: // 接收通知
        // LOG_I("client recv notification, length = %d", evt->client_recv_notify_indicate.length);
        // LOG_HEX(evt->client_recv_notify_indicate.value, evt->client_recv_notify_indicate.length);

        // AES_BleSendBytes(
        //     (uint8_t *)evt->client_recv_notify_indicate.value,
        //     evt->client_recv_notify_indicate.length,
        //     true
        // );
        client_recv_notification_parse(evt->client_recv_notify_indicate.value,
                                       evt->client_recv_notify_indicate.length);
        break;
    case CLIENT_RECV_INDICATION: // 接收指示
        // LOG_I("client recv indication");
        break;
    case CLIENT_PRIMARY_SVC_DIS_IND: // 主服务发现
        if (!memcmp(
                evt->client_svc_disc_indicate.uuid, ble_uart_svc_uuid, sizeof(ble_uart_svc_uuid))) {
            ble_client_svc_attribute_handle
                = evt->client_svc_disc_indicate.handle_range.begin_handle;
            ble_client_svc_end_handle = evt->client_svc_disc_indicate.handle_range.end_handle;

            // 发起特征发现
            gatt_manager_client_char_discover_by_uuid(con_idx,
                                                      (uint8_t *)&ble_uart_tx_uuid[0],
                                                      UUID_LEN_16BIT,
                                                      ble_client_svc_attribute_handle,
                                                      ble_client_svc_end_handle);
            // LOG_I("svc dis success, attribute_handle = %d, end_handle = %d",
            //       ble_client_svc_attribute_handle,
            //       ble_client_svc_end_handle);
        } else {
            LOG_I("unexpected svc uuid");
        }
        break;
    case CLIENT_CHAR_DIS_BY_UUID_IND:
        if (!memcmp(
                evt->client_disc_char_indicate.uuid, ble_uart_tx_uuid, sizeof(ble_uart_tx_uuid))) {
            // ble_client_tx_attribute_handle = evt->client_disc_char_indicate.attr_handle;
            ble_client_tx_pointer_handle = evt->client_disc_char_indicate.pointer_handle;
            // 发起特征发现
            gatt_manager_client_char_discover_by_uuid(con_idx,
                                                      (uint8_t *)&ble_uart_rx_uuid[0],
                                                      UUID_LEN_16BIT,
                                                      ble_client_svc_attribute_handle,
                                                      ble_client_svc_end_handle);
            // LOG_I("tx dis success, attribute handle = %d, pointer handler = %d",
            //       ble_client_tx_attribute_handle,
            //       ble_client_tx_pointer_handle);
        } else if (!memcmp(evt->client_disc_char_indicate.uuid,
                           ble_uart_rx_uuid,
                           sizeof(ble_uart_rx_uuid))) {
            // ble_client_rx_attribute_handle = evt->client_disc_char_indicate.attr_handle;
            // ble_client_rx_pointer_handle = evt->client_disc_char_indicate.pointer_handle;
            gatt_manager_client_desc_char_discover(
                con_idx, ble_client_svc_attribute_handle, ble_client_svc_end_handle);
            // LOG_I("rx dis success, attribute handle = %d, pointer handler = %d",
            //       ble_client_rx_attribute_handle,
            //       ble_client_rx_pointer_handle);
        } else {
            LOG_I("unexpected char uuid");
        }
        break;
    case CLIENT_CHAR_DESC_DIS_BY_UUID_IND: // 特征描述发现
        if (!memcmp(evt->client_disc_char_desc_indicate.uuid,
                    att_desc_client_char_cfg_array,
                    sizeof(att_desc_client_char_cfg_array))) {
            ble_client_cccd_handle = evt->client_disc_char_desc_indicate.attr_handle;
            // LOG_I("cccd dis success, cccd handle = %d", ble_client_cccd_handle);
            gatt_manager_client_cccd_enable(con_idx, ble_client_cccd_handle, 1, 0);
        } else {
            // LOG_I("unexpected desc uuid");
        }
        break;
    case CLIENT_WRITE_WITH_RSP_DONE: // 写响应完成
        if (evt->client_write_rsp.status == 0) {
            // LOG_I("write success with rsp");
        } else {
            LOG_I("write fail, status = %d", evt->client_write_rsp.status);
        }
        break;
    case CLIENT_WRITE_NO_RSP_DONE: // 写无响应完成
        if (evt->client_write_no_rsp.status == 0) {
            LS_ASSERT(gap_manager_get_role(con_idx) == LS_BLE_ROLE_MASTER); // client 角色
            // LOG_I("write no rsp success");
        } else {
            LOG_I("write fail, status = %d", evt->client_write_rsp.status);
        }
        break;
#endif // MASTER_CLIENT_ROLE == 1
    default:
        // LOG_I("Event not handled!");
        break;
    }
}

#if MASTER_CLIENT_ROLE == 1
// 广播模式，磁吸屏广播数据帧过滤
static bool client_adv_report_parse(const struct adv_report_evt *adv_report,
                                    uint8_t *record_type,
                                    uint8_t *nonce,
                                    uint8_t *data) {
    bool result = false;
    uint8_t *p_data = adv_report->data;
    uint16_t index = 0;

    while (index < adv_report->length) {
        uint8_t field_length = p_data[index];
        uint8_t field_type = p_data[index + 1];

        if (field_type == GAP_ADV_TYPE_MANU_SPECIFIC_DATA) {
            if (field_length != 27) {
                // LOG_D("adv type error: field_length: %d", field_length);
                return false;
            }
            // 避免强制类型转换，使用字节组合
            uint16_t company_id = p_data[index + 2] | (p_data[index + 3] << 8);
            // uint16_t prefix = p_data[index + 4] | (p_data[index + 5] << 8);
            // uint16_t product_id = p_data[index + 6] | (p_data[index + 7] << 8);
            if (company_id != COMPANY_ID) {
                return false;
            }

            *record_type = p_data[index + 8];
            nonce[0] = p_data[index + 9];
            nonce[1] = p_data[index + 10];
            uint8_t key_0 = p_data[index + 11];

            // 避免使用memcpy，直接循环复制
            for (int i = 0; i < 16; i++) {
                data[i] = p_data[index + 12 + i];
            }

            // 快配：只收0x85，默认密钥；不处理80/81
            if (*record_type == 0x85) {
                if (!is_fast_match_mode()) {
                    return false;
                }
                if (key_0 != IOT_ADV_KEY_DEFAULT[0]) {
                    return false;
                }
                return true;
            }
            if (is_fast_match_mode()) {
                return false;
            }

            // 校验key_0
            if (magnetic_screen_para.pairing_mode != PAIRING_MODE_SPECIFIC) { // 自动连接模式
                if (key_0 != IOT_ADV_KEY_DEFAULT[0]) { // 自动模式
                    // LOG_D("AUTO CONNECT: key_0 error: %02x", key_0);
                    return false;
                }
            } else { // 指定连接模式
                if (key_0 != magnetic_screen_para.target_key[0]) { // 指定模式，key_0不为0x00
                    // LOG_D("SPECIFIC CONNECT: key_0 error: %02x", key_0);
                    // 打印广播数据
                    // LOG_D("adv data:");
                    // LOG_HEX(p_data, adv_report->length);

                    // LOG_I("adv received, addr: %2x:%2x:%2x:%2x:%2x:%2x",
                    //       adv_report->adv_addr->addr[5],
                    //       adv_report->adv_addr->addr[4],
                    //       adv_report->adv_addr->addr[3],
                    //       adv_report->adv_addr->addr[2],
                    //       adv_report->adv_addr->addr[1],
                    //       adv_report->adv_addr->addr[0]);
                    // LOG_HEX(adv_report->data, adv_report->length);
                    // LOG_I("adv addr type: %2x", adv_report->adv_addr_type);
                    // LOG_I("adv rssi: %d", adv_report->rssi);
                    return false;
                }
            }
            // 磁吸屏只支持80和81的广播数据
            if (*record_type != 0x80 && *record_type != 0x81) {
                return false;
            }
            return true;
        }
        index += field_length + 1;
    }
    return result;
}
#endif // MASTER_CLIENT_ROLE == 1

void make_ecc_key_startup_init(void) {
    // Ecc_fill_rng_function(); /* 填充随机数发生函数 */
    Ecc_Make_key(ECDH_Pack_ptr, pack_buf_first, myident.md5_rst, myident.MD5_DATA);
    make_ecc_key_startup = true;
}

static void dev_manager_callback(enum dev_evt_type type, union dev_evt_u *evt) {
    // LOG_D("dev_manager_callback type:%d", type);
    switch (type) {
    case STACK_INIT: { /*蓝牙堆栈初始化*/
        struct ble_stack_cfg cfg = {
            .private_addr = false,
            .controller_privacy = false,
        };
        dev_manager_stack_init(&cfg);
        break;
    }
    case STACK_READY: {
        bool type;
        uint8_t wkup_source = get_wakeup_source();
        // LOG_I("WKUP_SOURCE = %d",wkup_source);

        dev_manager_get_identity_bdaddr(ble_para.ble_mac, &type); /*获取mac地址*/

        // LOG_I("type:%d,addr:",type);
        LOG_HEX(ble_para.ble_mac, sizeof(ble_para.ble_mac));
        dev_manager_add_service((struct svc_decl *)&uart_server_svc); /*添加服务*/

        add_adverData_mac(); /*添加蓝牙mac地址到广播包中*/

        iot_timer_queue_init();
        key_detect_timer_init();
        ble_sleep_input_init();
        charging_detect_io_init();
        iot_serivice_init();
        iot_software_timer_init(); /*软件定时器初始化*/

        TimerStart(SYSTEM_CONFIG_TIMER, 100, gSoftTimer);
        iot_uart_init(); /*串口初始化*/

        // 在长按按键时，生成ECC密钥，平时不需要这个。
        Ecc_fill_rng_function(); /* 填充随机数发生函数 */
        // Ecc_Make_key(ECDH_Pack_ptr, pack_buf_first, myident.md5_rst, myident.MD5_DATA);
        // make_ecc_key_startup = true; // 标记开机时ECC密钥生成完成，之后才可以处理at指令。
        if (!is_fcty_calibrated()) { // 未标定，单板测试用，自动进入配对模式
            make_ecc_key_startup_init();
            // key_2_start_adv(); // 开启广播
        }

        iot_i2c_init();
        magnetic_screen_init(); /*磁吸屏初始化*/

        cht8325_check_init();
        cht8325_check_start(); // 温湿度传感器定时读取

        adc_battery_check_init(); /*电池检测初始化*/
        adc_battery_check_start();

        rtc_time_init();
        rtc_time_check_start();

#if IOT_DOG_ENABLE
        HAL_IWDG_Init(64000); /*看门狗初始化*/
#endif

#if MASTER_CLIENT_ROLE == 1
        ble_client_init();
        if (scan_obj_hdl == 0xff) {
            dev_manager_create_scan_object(PUBLIC_OR_RANDOM_STATIC_ADDR);
        }
#endif // MASTER_CLIENT_ROLE == 1
        break;
    }
    case SERVICE_ADDED:
        // LOG_I("SERVICE_ADDED");
        gatt_manager_svc_register(
            evt->service_added.start_hdl, BLE_UART_IDX_NB, &uart_server_svc_env); /*注册服务*/
        create_adv_obj(); /*创建广播对象*/
        break;
    case PROFILE_ADDED:
        // prf_added_handler(&evt->profile_added);
        break;
    case ADV_OBJ_CREATED:
        LS_ASSERT(evt->obj_created.status == 0);
        adv_obj_hdl = evt->obj_created.handle; /*广播句柄*/

        if (!is_fcty_calibrated()) { // 未标定，单板测试用，自动进入配对模式
            key_2_start_adv();
        }
        // 不开启广播，需要通过按键来开启广播
        // start_adv(); /*开始广播*/
        // LOG_I("adv start");
        break;
    case ADV_STARTED:
        LOG_I("adv started");
        break;
    case ADV_STOPPED:
        LOG_I("adv stopped");
        // if (ble_para.adv_enable) { /*判断广播开关*/
        //     start_adv(); /*开始广播*/
        // }
        if (ble_para.adv_update_flag) {
            ble_para.adv_update_flag = false;
        }
        // update_adv(); /*更新广播*/
        // ble_para.adv_enable = false; /*广播广播*/
        break;
#if MASTER_CLIENT_ROLE == 1
    case SCAN_OBJ_CREATED:
        LOG_I("scan obj created");
        LS_ASSERT(evt->obj_created.status == 0);
        scan_obj_hdl = evt->obj_created.handle;
        dev_manager_create_init_object(PUBLIC_OR_RANDOM_STATIC_ADDR);
        break;
    case SCAN_STARTED:
        LOG_I("scan started");
        break;
    case SCAN_STOPPED:
        LOG_I("scan stopped");
        // LOG_I("scan stopped, next_connect_addr=0x%x", next_connect_addr);
        // if (next_connect_addr) {
        //     start_init(next_connect_addr);
        //     next_connect_addr = 0;
        //     init_status = INIT_BUSY;
        // }
        break;
    case ADV_REPORT: {
#if 0
        LOG_I("adv received, addr: %2x:%2x:%2x:%2x:%2x:%2x",
              evt->adv_report.adv_addr->addr[5],
              evt->adv_report.adv_addr->addr[4],
              evt->adv_report.adv_addr->addr[3],
              evt->adv_report.adv_addr->addr[2],
              evt->adv_report.adv_addr->addr[1],
              evt->adv_report.adv_addr->addr[0]);
        LOG_HEX(evt->adv_report.data, evt->adv_report.length);
        LOG_I("adv addr type: %2x", evt->adv_report.adv_addr_type);
        LOG_I("adv rssi: %d", evt->adv_report.rssi);
#endif
        uint8_t record_type = 0;
        uint8_t nonce[2] = {0};
        uint8_t data[IOT_ADV_MSG_LEN] = {0};
        // 目标mac地址数据收到后，解析前进行超时清零
        if (magnetic_screen_para.pairing_mode != PAIRING_MODE_SPECIFIC) { // 不指定目标
            if (memcmp(adv_mac_temp, (uint8_t[]){0, 0, 0, 0, 0, 0}, 6) != 0) { // 有记录
                if (memcmp(adv_mac_temp, evt->adv_report.adv_addr->addr, 6) == 0) { // 匹配
                    iot_timer_start_task(adv_recv_timeout_timer_id);
                }
            }
        } else { // 指定目标
            if (memcmp(magnetic_screen_para.target_mac, evt->adv_report.adv_addr->addr, 6)
                == 0) { // 匹配
                iot_timer_start_task(adv_recv_timeout_timer_id);
            }
        }

        // 解析广播数据
        if (!client_adv_report_parse(&evt->adv_report, &record_type, nonce, data)) {
            // LOG_D("adv parse error...");
            break;
        }

        if (record_type == 0x85) {
            magnetic_screen_adv_85_parse(data, nonce);
            break;
        }

        // 广播数据格式匹配，且Key_0正确

        // 判断是否自动连接模式
        if (magnetic_screen_para.pairing_mode != PAIRING_MODE_SPECIFIC) { // 不指定目标
            // 存储临时的设备MAC地址,
            // LOG_D("AUTO MAC");
            if (memcmp(adv_mac_temp, (uint8_t[]){0, 0, 0, 0, 0, 0}, 6) == 0) {
                memcpy(adv_mac_temp, evt->adv_report.adv_addr->addr, 6);
                // LOG_D("AUTO MAC SAFE");
            } else {
                if (memcmp(adv_mac_temp, evt->adv_report.adv_addr->addr, 6) != 0) {
                    // LOG_D("AUTO MAC ERROR");
                    break;
                } else {
                    // LOG_D("AUTO MAC MATCHED.......");
                }
            }
        } else { // 指定目标
            if (memcmp(evt->adv_report.adv_addr->addr, magnetic_screen_para.target_mac, 6) != 0) {
                // LOG_D("SPECIFIC MAC ERROR");
                // LOG_D("evt addr: %2x:%2x:%2x:%2x:%2x:%2x",
                //       evt->adv_report.adv_addr->addr[5],
                //       evt->adv_report.adv_addr->addr[4],
                //       evt->adv_report.adv_addr->addr[3],
                //       evt->adv_report.adv_addr->addr[2],
                //       evt->adv_report.adv_addr->addr[1],
                //       evt->adv_report.adv_addr->addr[0]);
                // LOG_D("target addr: %2x:%2x:%2x:%2x:%2x:%2x",
                //       magnetic_screen_para.target_mac[5],
                //       magnetic_screen_para.target_mac[4],
                //       magnetic_screen_para.target_mac[3],
                //       magnetic_screen_para.target_mac[2],
                //       magnetic_screen_para.target_mac[1],
                //       magnetic_screen_para.target_mac[0]);
                break;
            }
        }
        // LOG_I("adv received, addr: %2x:%2x:%2x:%2x:%2x:%2x",
        //       evt->adv_report.adv_addr->addr[5],
        //       evt->adv_report.adv_addr->addr[4],
        //       evt->adv_report.adv_addr->addr[3],
        //       evt->adv_report.adv_addr->addr[2],
        //       evt->adv_report.adv_addr->addr[1],
        //       evt->adv_report.adv_addr->addr[0]);
        if (record_type == 0x80) {
            magnetic_screen_adv_80_parse(data, nonce);
            magnetic_screen_set_inv_connect_status(BLE_CONNECTED);
            iot_timer_start_task(adv_recv_timeout_timer_id);
        } else if (record_type == 0x81) {
            magnetic_screen_adv_81_parse(data, nonce);
            magnetic_screen_set_inv_connect_status(BLE_CONNECTED);
            iot_timer_start_task(adv_recv_timeout_timer_id);
        } else {
            // LOG_D("adv type error: %02x", record_type);
        }
    } break;
    case INIT_OBJ_CREATED:
        LOG_I("init obj created");
        LS_ASSERT(evt->obj_created.status == 0);
        // init_obj_hdl = evt->obj_created.handle;
        start_scan();
        break;
    case INIT_STARTED:
        LOG_I("init started");
        break;
    case INIT_STOPPED:
        // init_status = INIT_IDLE;
        LOG_I("init stopped");
        break;
#endif // MASTER_CLIENT_ROLE == 1
    default:
        break;
    }
}

/*文件系统读取密码数据*/
uint8_t readPwd_from_tinyfs(void) {
    uint8_t rslt;
    uint16_t length = sizeof(ble_save_para);
    memset(&ble_save_para, 0, sizeof(ble_save_para));
    rslt = tinyfs_read(ble_para_dir, BLE_KEY, (uint8_t *)&ble_save_para, &length);
    memcpy(pwd, ble_save_para.app_pwd, sizeof(pwd));
    LOG_I("result is %d\n", rslt);
    for (uint8_t i = 0; i < 6; i++) {
        LOG_I("%d", ble_save_para.app_pwd[i]);
    }
    return rslt;
}

/*保存密码数据到文件系统*/
uint8_t savePwd_to_tinyfs(void) {
    uint8_t rslt;
    memcpy(ble_save_para.app_pwd, pwd, sizeof(pwd));
    ble_save_para.save_flag = 0xaabb; /*保存标志*/
    rslt = tinyfs_write(ble_para_dir, BLE_KEY, (uint8_t *)&ble_save_para, sizeof(ble_save_para));
    tinyfs_write_through(); /*刷新缓存*/
    return rslt;
}

/*文件系统读取蓝牙名称数据*/
uint8_t read_from_tinyfs(void) {
    uint8_t rslt;
    uint16_t length = sizeof(ble_save_para);
    memset(&ble_save_para, 0, sizeof(ble_save_para));
    rslt = tinyfs_read(ble_para_dir, BLE_KEY, (uint8_t *)&ble_save_para, &length);
    memcpy(ble_para.device_name, ble_save_para.ble_name, sizeof(ble_para.device_name));
    return rslt;
}

static uint8_t read_from_tinyfs_for_magnetic_screen(void) {
    uint16_t length = sizeof(magnetic_screen_para);
    memset(&magnetic_screen_para, 0, sizeof(magnetic_screen_para));
    tinyfs_read(magnetic_screen_para_dir,
                MAGNETIC_SCREEN_RECORD,
                (uint8_t *)&magnetic_screen_para,
                &length);
    if (magnetic_screen_para.save_flag != 0xaabb) { /*第一次保存数据*/
        LOG_I("first save data");
        memset(&magnetic_screen_para, 0, sizeof(magnetic_screen_para));
        magnetic_screen_para.save_flag = 0xaabb;
        magnetic_screen_para.pairing_mode = PAIRING_MODE_AUTOMATIC; // 默认自动配对模式
        magnetic_screen_para.display_timeout = DISPLAY_TIMEOUT_DFT; // 默认显示30s
        magnetic_screen_para.temp_unit = TEMPERATURE_UNIT_DFT; // 默认华氏度
        tinyfs_write(magnetic_screen_para_dir,
                     MAGNETIC_SCREEN_RECORD,
                     (uint8_t *)&magnetic_screen_para,
                     sizeof(magnetic_screen_para));
        tinyfs_write_through(); /*刷新缓存*/
    }
    return 0;
}

void save_magnetic_screen_para_to_tinyfs(void) {
    tinyfs_write(magnetic_screen_para_dir,
                 MAGNETIC_SCREEN_RECORD,
                 (uint8_t *)&magnetic_screen_para,
                 sizeof(magnetic_screen_para));
    tinyfs_write_through(); /*刷新缓存*/
}

void clear_magnetic_screen_para(void) {
    memset(&magnetic_screen_para, 0, sizeof(magnetic_screen_para));
    save_magnetic_screen_para_to_tinyfs();
    // 初始密码默认无
    memset(ble_save_para.app_pwd, 0x00, sizeof(ble_save_para.app_pwd));
    save_to_tinyfs();
}

void Set_Magnetic_Screen_Para_pairing_mode(pairing_mode_t pairing_mode) {
    uint8_t mode = (pairing_mode >> 2) & 0x3;
    magnetic_screen_para.pairing_mode = mode == 1 ? PAIRING_MODE_SPECIFIC : PAIRING_MODE_AUTOMATIC;

    magnetic_screen_para.save_flag = 0xaabb;
    save_magnetic_screen_para_to_tinyfs();
}

// char target_name[12];
// uint64_t target_sn;
// uint8_t target_key[16];
// uint8_t target_mac[6];
void Set_Magnetic_Screen_Para_target(uint8_t *target) {
    uint16_t out_len = 0;
    magnetic_screen_para.save_flag = 0xaabb;
    change_to_modbus_data((uint8_t *)magnetic_screen_para.target_name, target, &out_len, 12);
    change_to_modbus_data((uint8_t *)&magnetic_screen_para.target_sn, target + 12, &out_len, 8);
    change_to_modbus_data(magnetic_screen_para.target_key, target + 20, &out_len, 16);
    change_to_modbus_data(magnetic_screen_para.target_mac, target + 36, &out_len, 6);
    // memcpy(magnetic_screen_para.target_name, target, 12);
    // memcpy(&magnetic_screen_para.target_sn, target + 12, 8);
    // memcpy(magnetic_screen_para.target_key, target + 20, 16);
    // memcpy(magnetic_screen_para.target_mac, target + 36, 6);
    // LOG_D("Set_Magnetic_Screen_Para_target");
    // LOG_D("set target_name: %s", magnetic_screen_para.target_name);
    // LOG_D("set target_sn: %llu", magnetic_screen_para.target_sn);
    // LOG_D("set target_key:");
    // LOG_HEX(magnetic_screen_para.target_key, 16);
    // LOG_D("set target_mac:");
    // LOG_HEX(magnetic_screen_para.target_mac, 6);
    save_magnetic_screen_para_to_tinyfs();
}

/*保存AT指令修改蓝牙名称数据到文件系统*/
uint8_t save_to_tinyfs(void) {
    uint8_t rslt;
    memcpy(ble_save_para.ble_name, ble_para.device_name, sizeof(ble_para.device_name));
    ble_save_para.save_flag = 0xaabb; /*保存标志*/
    rslt = tinyfs_write(ble_para_dir, BLE_KEY, (uint8_t *)&ble_save_para, sizeof(ble_save_para));
    tinyfs_write_through(); /*刷新缓存*/
    return rslt;
}

uint8_t update_to_tinyfs(void) {
    uint8_t rslt;
    ble_save_para.save_flag = 0xaabb; /*保存标志*/
    rslt = tinyfs_write(ble_para_dir, BLE_KEY, (uint8_t *)&ble_save_para, sizeof(ble_save_para));
    tinyfs_write_through(); /*刷新缓存*/
    return rslt;
}

/*保存默认数据到文件系统*/
uint8_t defalut_data_to_tinyfs(void) {
    uint8_t rslt;

    ble_save_para.safe_code = IOT_SAFECODE_DEFAULT;

    ble_save_para.save_flag = 0xaabb; /* 保存标志 */

    memset(ble_save_para.app_pwd, 0x00, sizeof(ble_save_para.app_pwd)); // 初始密码默认无
    ble_save_para.iot_sn = IOT_SN_DEFAULT; // 默认 TYPE
    memset(ble_save_para.iot_type, 0x00, sizeof(ble_save_para.iot_type));
    memcpy(ble_save_para.iot_type, IOT_TYPE_DEFAULT, sizeof(IOT_TYPE_DEFAULT)); // 默认SN

    // ble name: type + sn
    memcpy(ble_save_para.ble_name, IOT_TYPE_DEFAULT, strlen(IOT_TYPE_DEFAULT));
    // 拼接sn
    char sn_str[24] = {0};
    sprintf(sn_str, "%lu", IOT_SN_DEFAULT);
    memcpy(ble_save_para.ble_name + strlen(IOT_TYPE_DEFAULT), sn_str, strlen(sn_str));

    rslt = tinyfs_write(ble_para_dir, BLE_KEY, (uint8_t *)&ble_save_para, sizeof(ble_save_para));
    tinyfs_write_through(); /*刷新缓存*/

    memcpy(ble_para.device_name, ble_save_para.ble_name, sizeof(ble_para.device_name));

    LOG_D("set default data to tinyfs");

    return rslt;
}

void show_magnetic_screen_info() {
    LOG_D("magnetic_screen_para.save_flag = %x", magnetic_screen_para.save_flag);
    LOG_D("magnetic_screen_para.pairing_mode = %d", magnetic_screen_para.pairing_mode);
    LOG_D("magnetic_screen_para.target_name = %s", magnetic_screen_para.target_name);
    LOG_D("magnetic_screen_para.target_sn = %llu", magnetic_screen_para.target_sn);
    LOG_D("magnetic_screen_para.target_key =");
    LOG_HEX(magnetic_screen_para.target_key, 16);
    LOG_D("magnetic_screen_para.target_mac =");
    LOG_HEX(magnetic_screen_para.target_mac, 6);
}

/*蓝牙数据缓存初始化*/
void ble_data_init(void) {
    ble_data_t *pctx = &ble_data;
    pctx->tx_size = 0;
    pctx->tx_state = BLE_TX_IDLE;
    pctx->tx_offset = 0;

    pctx->rx_size = 0;
    pctx->rx_state = BLE_RX_IDLE;
    pctx->rx_offset = 0;

    pctx->self_tx_size = 0;
    pctx->self_tx_state = BLE_TX_IDLE;
    pctx->self_tx_offset = 0;

    pctx->self_rx_size = 0;
    pctx->self_rx_state = BLE_RX_IDLE;
    pctx->self_rx_offset = 0;

    // memcpy((char*)ble_para.device_name, "BLUETTI-IOT", sizeof("BLUETTI-IOT"));
    tinyfs_mkdir(&ble_para_dir, ROOT_DIR, 5);
    tinyfs_mkdir(&magnetic_screen_para_dir, ROOT_DIR, 6);
    read_from_tinyfs(); /*读取信息*/
    if (ble_save_para.save_flag != 0xaabb) { /*第一次保存数据*/
        // LOG_I("first save data");
        defalut_data_to_tinyfs();
        memset(ble_para.device_name, 0, sizeof(ble_para.device_name));
        /*复制名称*/
        memcpy(ble_para.device_name, ble_save_para.ble_name, sizeof(ble_save_para.ble_name));
    }
    LOG_D("ble_para.device_name is %s", ble_para.device_name);
    read_from_tinyfs_for_magnetic_screen(); /*读取磁吸屏信息*/

    // show_magnetic_screen_info(); /*打印磁吸屏信息*/

    // memcpy(ble_para.device_name, "SolarX 4K2513000002240", strlen("SolarX 4K2513000002240"));
    // 用这个型号测试升级程序
    // memcpy(ble_para.device_name, "EB3A2429004806666", strlen("EB3A2429004806666"));
    add_scanRespon_name(ble_para.device_name, strlen((char *)ble_para.device_name));
    create_ble_receive_timer(); /*创建定时器*/
    rf_set_power(TX_7DBM);
}

void myaes_test(void) {
    // char test[] = "hello world!";
    // ble_data_t *pctx = &ble_data;
    // uint8_t test_Data[4];
    // encry_save_to_tinyfs(
    //     ble_para_dir, DIR1_TEST, (uint8_t *)DIR1_TEST_DATA, strlen(DIR1_TEST_DATA)
    // );
    // decry_read_from_tinyfs(ble_para_dir, DIR1_TEST, MYTEST, strlen(DIR1_TEST_DATA));
    // uint8_t sign[64];
    // Aes_Cbc_encry(test_2, 0x60); // AES_cbc解密
    // LOG_I("encry_text is :");
    // for (uint8_t i = 0; i < 80; i++) {
    //     printf("%x,", test_2[i]);
    // }
}

void iot_ble_init(void) {
    sys_init_app(); /*平台外设初始化*/
    LOG_I("IOT_SYSTEM_VER is %d ", IOT_SYSTEM_VER);
    LOG_I("compile time is %s", __DATE__ " " __TIME__);

    ble_init();
    ble_data_init();
    dev_manager_init(dev_manager_callback);
    gap_manager_init(gap_manager_callback);
    gatt_manager_init(gatt_manager_callback);
    ble_loop();
}
