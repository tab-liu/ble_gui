#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "modbus_tcp_slave.h"
#include "modbus_data.h"
#include "esp_log.h"
#include "modbus_protocol.h"
#include "comm_define.h"
#include "modbus_slave.h"
#include "xmodem_client.h"
#include "parameter.h"
#include "iot_partition.h"
#include "utils.h"
#include "aiot_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

static const char *TAG = "[MODBUS_TCP]";

#define MODBUS_TCP_NODE             "0.0.0.0"   // any address
#define MODBUS_TCP_SERVICE_DEFAULT  "502"

#define INDICATION_TIMEOUT_SEC      0
#define INDICATION_TIMEOUT_USEC     0
#define RESPONSE_TIMEOUT_SEC        0
#define RESPONSE_TIMEOUT_USEC       500000  // 0.5 s
#define RECEIVE_TIMEOUT_SEC         0
#define RECEIVE_TIMEOUT_USEC        500000  // 0.5 s

#define REGISTER_NUM_MAX            350

#define CONFIG_CAN_SUPPORT          1
#define CONFIG_UART_SUPPORT         0

enum {
    _RUN_STATE_IDLE = 0,
    _RUN_STATE_INIT,
    _RUN_STATE_OPEN,
    _RUN_STATE_LISTENNING,
    _RUN_STATE_ACCEPT,
    _RUN_STATE_RUNNING,
    _RUN_STATE_RESTART,
    _RUN_STATE_EXIT,
} run_status_t;

enum {
    PERMISSION_READ_ONLY = 0,
    PERMISSION_READ_WRITE,
};

#pragma pack(push, 1)

/** 
 * @brief modbus地址映射表
 */
typedef struct _modbus_addr_map
{
    uint16_t private_addr;      /*< 内部地址 */
    uint16_t public_addr;       /*< 对外地址 */
    uint16_t num;               /*< 寄存器个数 */
    uint8_t  permission;        /*< 读写权限, 0:只读, 1:读写 */
}modbus_addr_map_t;

#pragma pack(pop)

extern QueueHandle_t can_cmd_queue;

static int g_tcp_run_status = _RUN_STATE_IDLE;

USE_EXT_RAM_BSS static modbus_addr_map_t segments[REGISTER_NUM_MAX] = {0};
USE_EXT_RAM_BSS static modbus_tcp_server_t md_tcp_svr = {0};
USE_EXT_RAM_BSS static modbus_addr_map_t g_modbus_addr[REGISTER_NUM_MAX] = {0};

void modbus_addr_init(void)
{
    const modbus_addr_map_t modbus_addr[] = {
        /*private   public      number*/
        /* Inverter Summary Information */
        {120,        50001,       1,        0},
        {142,        50002,       1,        0},
        {143,        50003,       1,        0},
        {144,        50004,       1,        0},
        {145,        50005,       1,        0},
        {146,        50006,       1,        0},
        {147,        50007,       1,        0},
        {148,        50008,       1,        0},
        {149,        50009,       1,        0},
        {165,        50010,       1,        0},
        {166,        50011,       1,        0},
        {152,        50012,       1,        0},
        {153,        50013,       1,        0},
        {154,        50014,       1,        0},
        {155,        50015,       1,        0},
        {156,        50016,       1,        0},
        {157,        50017,       1,        0},
        {158,        50018,       1,        0},
        {159,        50019,       1,        0},
        {162,        50020,       1,        0},
        {163,        50021,       1,        0},
        {161,        50022,       1,        0},
        {126,        50023,       4,        0},
        {133,        50027,       5,        0},
        /* Inverter Basic Information */
        {1101,       50200,       1,        0},
        {1102,       50201,       1,        0},
        {1103,       50202,       1,        0},
        {1104,       50203,       1,        0},
        {1105,       50204,       1,        0},
        {1106,       50205,       1,        0},
        {1107,       50206,       1,        0},
        {1108,       50207,       1,        0},
        {1109,       50208,       1,        0},
        {1110,       50209,       1,        0},
        {1114,       50210,       1,        0},
        {1115,       50211,       1,        0},
        {1117,       50212,       1,        0},
        {1118,       50213,       1,        0},
        {1300,       50214,       1,        0},
        {1301,       50215,       1,        0},
        {1302,       50216,       1,        0},
        {1420,       50217,       1,        0},
        {1421,       50218,       1,        0},
        {1200,       50219,       1,        0},
        {1201,       50220,       1,        0},
        {1204,       50221,       1,        0},
        {1205,       50222,       1,        0},
        {1303,       50223,       1,        0},
        {1304,       50224,       1,        0},
        {1305,       50225,       1,        0},
        {1306,       50226,       1,        0},
        {1422,       50227,       1,        0},
        {1423,       50228,       1,        0},
        {1202,       50229,       1,        0},
        {1203,       50230,       1,        0},
        {1206,       50231,       1,        0},
        {1207,       50232,       1,        0},
        {1208,       50233,       1,        0},
        {1312,       50234,       1,        0},
        {1313,       50235,       1,        0},
        {1314,       50236,       1,        0},
        {1315,       50237,       1,        0},
        {1319,       50238,       1,        0},
        {1320,       50239,       1,        0},
        {1321,       50240,       1,        0},
        {1325,       50241,       1,        0},
        {1326,       50242,       1,        0},
        {1327,       50243,       1,        0},
        {1429,       50244,       1,        0},
        {1430,       50245,       1,        0},
        {1431,       50246,       1,        0},
        {1432,       50247,       1,        0},
        {1436,       50248,       1,        0},
        {1437,       50249,       1,        0},
        {1438,       50250,       1,        0},
        {1442,       50251,       1,        0},
        {1443,       50252,       1,        0},
        {1444,       50253,       1,        0},
        {1508,       50254,       1,        0},
        {1509,       50255,       1,        0},
        {1510,       50256,       1,        0},
        {1511,       50257,       1,        0},
        {1512,       50258,       1,        0},
        {1516,       50259,       1,        0},
        {1517,       50260,       1,        0},
        {1518,       50261,       1,        0},
        {1519,       50262,       1,        0},
        {1523,       50263,       1,        0},
        {1524,       50264,       1,        0},
        {1525,       50265,       1,        0},
        {1526,       50266,       1,        0},
        {1209,       50267,       1,        0},
        {1211,       50268,       1,        0},
        {1212,       50269,       1,        0},
        {1213,       50270,       1,        0},
        {1214,       50271,       1,        0},
        {1219,       50272,       1,        0},
        {1220,       50273,       1,        0},
        {1221,       50274,       1,        0},
        {1222,       50275,       1,        0},
        {1227,       50276,       1,        0},
        {1228,       50277,       1,        0},
        {1229,       50278,       1,        0},
        {1230,       50279,       1,        0},
        {1235,       50280,       1,        0},
        {1236,       50281,       1,        0},
        {1237,       50282,       1,        0},
        {1238,       50283,       1,        0},
        {1243,       50284,       1,        0},
        {1244,       50285,       1,        0},
        {1245,       50286,       1,        0},
        {1246,       50287,       1,        0},
        /* Battery Pack Summary Information */
        {6001,       51001,       1,        0},
        {6003,       51002,       1,        0},
        {6004,       51003,       1,        0},
        {6005,       51004,       1,        0},
        {6006,       51005,       1,        0},
        {6009,       51006,       1,        0},
        {6017,       51007,       1,        0},
        {6018,       51008,       1,        0},
        /* Each Battery Pack Base Information */
        {6101,       51200,       1,        0},
        {6102,       51201,       1,        0},
        {6103,       51202,       1,        0},
        {6104,       51203,       1,        0},
        {6105,       51204,       1,        0},
        {6106,       51205,       1,        0},
        {6107,       51206,       1,        0},
        {6108,       51207,       1,        0},
        {6109,       51208,       1,        0},
        {6110,       51209,       1,        0},
        {6173,       51210,       1,        0},
        {6175,       51211,       1,        0},
        {6176,       51212,       1,        0},
        {6178,       51213,       1,        0},
        {6179,       51214,       1,        0},
        {6181,       51215,       1,        0},
        {6182,       51216,       1,        0},
        {6184,       51217,       1,        0},
        {6185,       51218,       1,        0},
        {6111,       51219,       1,        0},
        {6112,       51220,       1,        0},
        {6113,       51221,       1,        0},
        {6114,       51222,       1,        0},
        {6143,       51223,       1,        0},
        {6115,       51224,       1,        0},
#if 0   // 电芯信息属于敏感数据，暂不对外显示
        {6116,       51225,       1,        0},
        {6117,       51226,       1,        0},
        {6118,       51227,       1,        0},
        {6119,       51228,       1,        0},
        {6120,       51229,       1,        0},
        {6121,       51230,       1,        0},
        // {6123,       51231,       1},
        {6122,       51232,       1,        0},
        {6123,       51233,       1,        0},
#endif
        {6152,       51234,       1,        0},
        {6153,       51235,       1,        0},
        {6133,       51236,       1,        0},
        {6134,       51237,       1,        0},
        {6135,       51238,       1,        0},
        {6136,       51239,       1,        0},
        {6144,       51240,       2,        0},
        {6146,       51242,       3,        0},
        {6149,       51245,       1,        0},
        {6164,       51246,       2,        0},
        {6150,       51248,       1,        0},
        {6151,       51249,       1,        0},
        /** DCDC Power module RO Register */
        {15500,      51501,       6,        0},
        {15506,      51507,       4,        0},
        {15581,      51511,       1,        0},
        {15582,      51512,       2,        0},
        {15527,      51514,       2,        0},
        {15529,      51516,       2,        0},
        {15531,      51518,       1,        0},
        {15532,      51519,       2,        0},
        {15534,      51521,       1,        0},
        {15535,      51522,       1,        0},
        {15536,      51523,       2,        0},
        {15538,      51525,       1,        0},
        {15539,      51526,       1,        0},
        {15540,      51527,       2,        0},
        {15542,      51529,       1,        0},
        {15543,      51530,       1,        0},
        {15544,      51531,       2,        0},
        {15546,      51533,       1,        0},
        {15547,      51534,       1,        0},
        {15548,      51535,       2,        0},
        {15550,      51537,       1,        0},
        {15551,      51538,       1,        0},
        {15552,      51539,       2,        0},
        {15554,      51541,       1,        0},
        {15555,      51542,       2,        0},
        {15557,      51544,       2,        0},
        {15559,      51546,       2,        0},
        {15561,      51548,       2,        0},
        {15563,      51550,       2,        0},
        {15565,      51552,       2,        0},
        {15567,      51554,       2,        0},
        {15569,      51556,       2,        0},
        {15571,      51558,       2,        0},
        {15573,      51560,       2,        0},
        {15575,      51562,       2,        0},
        {15577,      51564,       2,        0},
        {15579,      51566,       2,        0},
        /** IOT/HMI */
        {11000,      53001,       6,        0},
        {11006,      53007,       4,        0},
        {11014,      53011,       2,        0},
        /** Smart Plug */
        {14500,      55001,       6,        0},
        {14506,      55007,       4,        0},
        {14510,      55011,       2,        0},
        {14512,      55013,       1,        0},
        {14513,      55014,       1,        0},
        {14514,      55015,       1,        0},
        {14515,      55016,       1,        0},
        {14516,      55017,       1,        0},
        {14517,      55018,       1,        0},
        {14518,      55019,       1,        0},
        {14519,      55020,       1,        0},
        {14520,      55021,       1,        0},
        {14521,      55022,       1,        0},
        {14522,      55023,       1,        0},
        {14523,      55024,       1,        0},
        {14524,      55025,       1,        0},
        {14525,      55026,       1,        0},
        {14526,      55027,       1,        0},
        {14527,      55028,       1,        0},
        {14528,      55029,       1,        0},
        {14529,      55030,       1,        0},
        /** AC Meter */
        {1700,       55101,       6,        0},
        {1706,       55107,       4,        0},
        {1710,       55111,       1,        0},
        {1711,       55112,       2,        0},
        {1713,       55114,       2,        0},
        {1715,       55116,       2,        0},
        {1717,       55118,       2,        0},
        {1719,       55120,       2,        0},
        {1721,       55122,       2,        0},
        {1723,       55124,       2,        0},
        {1725,       55126,       2,        0},
        {1727,       55128,       2,        0},
        {1729,       55130,       2,        0},
        {1731,       55132,       2,        0},
        {1733,       55134,       2,        0},
        {1735,       55136,       2,        0},
        {1737,       55138,       2,        0},
        {1739,       55140,       2,        0},
        {1741,       55142,       2,        0},
        {1743,       55144,       2,        0},
        {1745,       55146,       2,        0},
        {1747,       55148,       2,        0},
        {1749,       55150,       2,        0},
        {1751,       55152,       2,        0},
        {1753,       55154,       2,        0},
        {1755,       55156,       2,        0},
        {1757,       55158,       2,        0},
        {1759,       55160,       2,        0},
        {1761,       55162,       2,        0},
        {1763,       55164,       2,        0},
        {1765,       55166,       2,        0},
        {1767,       55168,       2,        0},
        {1769,       55170,       2,        0},
        {1771,       55172,       2,        0},
        {1773,       55174,       2,        0},
        {1775,       55176,       2,        0},
        {1777,       55178,       2,        0},
        {1779,       55180,       2,        0},
        {1781,       55182,       2,        0},
        {1783,       55184,       2,        0},
        {1785,       55186,       2,        0},
        {1787,       55188,       2,        0},
        {1789,       55190,       2,        0},
        /** Inverter Summary Information */
        {2011,       57001,       1,        1},
        // {2012,       57002,       1,        1},
        // {2013,       57003,       1,        1},
        // {2014,       57004,       1,        1},
        {2012,       57005,       1,        1},
        // {2016,       57006,       1,        1},
        // {2017,       57007,       1,        1},
        // {2018,       57008,       1,        1},
        {2207,       57009,       1,        1},
        {2208,       57010,       1,        1},
        {2214,       57011,       1,        1},
        {2216,       57012,       1,        1},
        {2212,       57013,       1,        1},
        // {2024,       57014,       1,        1},
        // {2025,       57015,       1,        1},
        {2022,       57016,       1,        1},
        {2023,       57017,       1,        1},
        {2247,       57018,       1,        1},
        {2248,       57019,       1,        1},
        {2005,       57020,       1,        1},
        // {2031,       57021,       1,        1},
        {2021,       57022,       1,        1},
        {2020,       57023,       1,        1},
        {2017,       57024,       1,        1},
        {2014,       57025,       1,        1},
        {2013,       57026,       1,        1},
        /** DCDC Power module */
        {14700,       59501,       1,        1},
        {14701,       59502,       1,        1},
        {14702,       59503,       1,        1},
        {14703,       59504,       1,        1},
        {14704,       59505,       1,        1},
        {14705,       59506,       1,        1},
        {14706,       59507,       1,        1},
        {14707,       59508,       1,        1},
        {14708,       59509,       1,        1},
        {14709,       59510,       1,        1},
        /** AC Meter */
        {1900,       59301,       1,        1},
        {1901,       59302,       1,        1},
        {1902,       59303,       1,        1},
        {1903,       59304,       1,        1},
        {1904,       59305,       1,        1},
        {1905,       59306,       1,        1},
        {1906,       59307,       1,        1},
        {1922,       59323,       1,        1},
    };

    uint16_t total_num = sizeof(modbus_addr)/sizeof(modbus_addr[0]);
    if (total_num > REGISTER_NUM_MAX)
    {
        ESP_LOGE(TAG, "Modbus TCP register data exceeds the limit");
        return;
    }

    memset(g_modbus_addr, 0x0, sizeof(g_modbus_addr));
    memcpy(g_modbus_addr, modbus_addr, sizeof(modbus_addr));
}

uint16_t modbus_error(uint8_t *response, uint8_t error)
{
    response[1] |= 0x80;
    response[2]  = error;
    return 3;
}

void modbus_tcp_server_config_set(uint8_t is_crypted, uint16_t port, uint8_t block)
{
    snprintf(md_tcp_svr.config.port, sizeof(md_tcp_svr.config.port), "%d", port);
    md_tcp_svr.config.crypt_en = is_crypted;
    md_tcp_svr.config.block = block;
}

void modbus_tcp_server_init(void)
{
    md_tcp_svr.server_ctx.tcp.listen_fd = -1;
    md_tcp_svr.server_ctx.tcp.client_fd = -1;
    md_tcp_svr.server_ctx.tcps.tls_ctx = NULL; // 初始化为NULL
    md_tcp_svr.server_ctx.next_state = TCP_STATE_OPEN;

    modbus_addr_init();

    ESP_LOGI(TAG, "Modbus TCP server init, port:%s, crypt_en:%d, block:%d",
             md_tcp_svr.config.port, md_tcp_svr.config.crypt_en, md_tcp_svr.config.block);

    // 根据是否加密，设置不同的函数指针
    if (1 == md_tcp_svr.config.crypt_en)
    {
#if ENCRYPT_CERT_USE_FILE_SYSTEM
        // 加载Modbus TCP加密证书
        load_modbus_tcp_cert();
#endif
        /* 使用TLS加密, 加密证书为ca分区中的证书 */
        md_tcp_svr.open = tcps_server_open;
        md_tcp_svr.listen = tcps_server_listen;
        md_tcp_svr.accept = tcps_server_accept;
        md_tcp_svr.send = tcps_server_send;
        md_tcp_svr.recv = tcps_server_recv;
        md_tcp_svr.close = tcps_server_close;
        md_tcp_svr.reset = tcps_server_reset;
        md_tcp_svr.is_read_ready = tcps_server_read_ready;
        md_tcp_svr.is_connected = tcps_server_is_connected;
    }
    else
    {
        md_tcp_svr.open = tcp_server_open;
        md_tcp_svr.listen = tcp_server_listen;
        md_tcp_svr.accept = tcp_server_accept;
        md_tcp_svr.send = tcp_server_send;
        md_tcp_svr.recv = tcp_server_recv;
        md_tcp_svr.close = tcp_server_close;
        md_tcp_svr.reset = tcp_server_reset;
        md_tcp_svr.is_read_ready = tcp_server_read_ready;
        md_tcp_svr.is_connected = tcp_server_is_connected;
    }

    if (md_tcp_svr.query == NULL) {
        md_tcp_svr.query = (uint8_t *)iot_calloc(MODBUS_TCP_MAX_ADU_LENGTH);
        if (md_tcp_svr.query == NULL) {
            ESP_LOGE(TAG, "malloc query buffer failed");
            return;
        }
    }
}

/* Computes the length to read after the function received */
static uint8_t compute_meta_length_after_function(int function)
{
    int length;

    if (function <= MODBUS_FC_WRITE_SINGLE_REGISTER)
    {
        length = 4;
    }
    else if (function == MODBUS_FC_WRITE_MULTIPLE_COILS ||
                function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS)
    {
        length = 5;
    }
    else if (function == MODBUS_FC_MASK_WRITE_REGISTER)
    {
        length = 6;
    } else if (function == MODBUS_FC_WRITE_AND_READ_REGISTERS)
    {
        length = 9;
    } else
    {
        /* MODBUS_FC_READ_EXCEPTION_STATUS, MODBUS_FC_REPORT_SLAVE_ID */
        length = 0;
    }

    return length;
}

/* Computes the length to read after the meta information (address, count, etc) */
static int compute_data_length_after_meta(uint8_t *msg)
{
    int function = msg[_MODBUS_TCP_HEADER_LENGTH];
    int length;

    switch (function)
    {
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            length = msg[_MODBUS_TCP_HEADER_LENGTH + 5];
            break;
        case MODBUS_FC_WRITE_AND_READ_REGISTERS:
            length = msg[_MODBUS_TCP_HEADER_LENGTH + 9];
            break;
        default:
            length = 0;
    }

    length += _MODBUS_TCP_CHECKSUM_LENGTH;

    return length;
}

static int _modbus_tcp_send_msg_pre(uint8_t *req, int req_length)
{
    /* Subtract the header length to the message length */
    int mbap_length = req_length - 6;

    req[4] = mbap_length >> 8;
    req[5] = mbap_length & 0x00FF;

    return req_length;
}

/* Sends a request/response */
static int send_msg(int sockfd, uint8_t *msg, int msg_length)
{
    int rc = -1;

    /* Notice: 这里会根据实际长度，填充协议头中的"长度"字段 */
    msg_length = _modbus_tcp_send_msg_pre(msg, msg_length);

    rc = md_tcp_svr.send(&md_tcp_svr, msg, msg_length);
    if (rc == -1)
    {
        ESP_LOGE(TAG, "send error:%d", rc);
        return -1;
    }

#if CONFIG_LOG_UDP_REDIRECT == 2 
    supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
    supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
    if ((1 == resource.modbus_tcp) && (1 == direction.tx)) {
        aiot_log_push((const uint8_t *)msg, (uint16_t)msg_length, LOG_TYPE_MODBUS_TCP_TX);
    }
#endif

    return rc;
}

static int _modbus_tcp_check_integrity(uint8_t *msg, const int msg_length)
{
    return msg_length;
}

int _modbus_receive_msg(uint8_t *msg)
{
    int rc;
    struct timeval tv;
    struct timeval *p_tv = NULL;
    unsigned int length_to_read;
    int msg_length = 0;
    _step_t step;

    if (!md_tcp_svr.is_connected(&md_tcp_svr))
    {
        ESP_LOGE(TAG, "ERROR The connection is not established.\n");
        return -1;
    }

    /* 解决TCP粘包问题: 第一次先读取报文头MBAP */
    step = _STEP_FUNCTION;
    length_to_read = _MODBUS_TCP_HEADER_LENGTH + 1;
    /* 阻塞模式下, 如果超时时间设置0则永久等待. 非阻塞模式下，设置为0则立刻返回 */
    if (md_tcp_svr.config.block && INDICATION_TIMEOUT_SEC == 0 && INDICATION_TIMEOUT_USEC == 0)
    {
        /* By default, the indication timeout isn't set */
        p_tv = NULL;
    }
    else
    {
        /* Wait for an indication (name of a received request by a server, see schema)
            */
        tv.tv_sec = INDICATION_TIMEOUT_SEC;
        tv.tv_usec = INDICATION_TIMEOUT_USEC;
        p_tv = &tv;
    }

    while (length_to_read != 0)
    {
        // ESP_LOGI(TAG, "modbus tcp slave is_read_ready 111");
        // rc = md_tcp_svr.is_read_ready(&md_tcp_svr, p_tv);
        // if (rc == -1)
        // {
        //     ESP_LOGE(TAG, "select error:%d", rc);
        //     return -1;
        // }

        // if (rc == 0)
        // {
        //     ESP_LOGI(TAG, "modbus tcp slave timeout");
        //     return 0;
        // }

        rc = md_tcp_svr.recv(&md_tcp_svr, msg + msg_length, length_to_read);
        if (rc == 0)
        {
            return 0;
        }

        if (rc < 0)
        {
            ESP_LOGE(TAG, "read error:%d", rc);
            return -1;
        }

        /* Sums bytes received */
        msg_length += rc;
        /* Computes remaining bytes */
        length_to_read -= rc;

        /* 解决TCP粘包问题: 第一次先读取报文头MBAP,
            根据MBAP中的报文长度字段，计算数据的有效长度 */
        if (length_to_read == 0)
        {
            switch (step)
            {
                case _STEP_FUNCTION:
                    /* Function code position */
                    length_to_read = compute_meta_length_after_function(
                        msg[_MODBUS_TCP_HEADER_LENGTH]);
                    if (length_to_read != 0)
                    {
                        step = _STEP_META;
                        break;
                    }

                case _STEP_META:
                    /* 解决TCP粘包问题: 根据MBAP中的报文长度字段，计算数据的有效长度 */
                    length_to_read = compute_data_length_after_meta(msg);
                    if ((msg_length + length_to_read) > MODBUS_TCP_MAX_ADU_LENGTH)
                    {
                        ESP_LOGE(TAG, "too many data");
                        return -1;
                    }
                    step = _STEP_DATA;
                    break;
                default:
                    break;
            }
        }

        // if (length_to_read > 0 && (RECEIVE_TIMEOUT_SEC > 0 || RECEIVE_TIMEOUT_USEC > 0))
        // {
        //     tv.tv_sec = RECEIVE_TIMEOUT_SEC;
        //     tv.tv_usec = RECEIVE_TIMEOUT_USEC;
        //     p_tv = &tv;
        // }
    }

    // return _modbus_tcp_check_integrity(msg, msg_length);
    return msg_length;
}

static int _modbus_tcp_receive(uint8_t *req)
{
    return _modbus_receive_msg(req);
}

int modbus_tcp_format_check(uint8_t *data, uint16_t len)
{
    if (!data)
    {
        return -1; // modbus unknown pack
    }

    // 外部slave地址为100则认为是读取汇总数据
    if(data[0] == MODBUS_SLAVE_SUMMARY_ADDR)
    {
        data[0] = 0x00;
    }

    if ((data[1] != MODBUS_FC_READ_HOLDING_REGISTERS)
        && (data[1] != MODBUS_FC_WRITE_SINGLE_REGISTER)
        && (data[1] != MODBUS_FC_WRITE_MULTIPLE_REGISTERS))
    {
        ESP_LOGE(TAG, "modbus function code error:%d", data[1]);
        return -2;
    }

    uint16_t crc16 = ModbusCrc16(data, len - 2);
    data[len - 1] = (crc16 & 0xFF00) >> 8;
    data[len - 2] = (crc16 & 0x00FF);
    ESP_LOGW(TAG, "modbus tcp receive data crc16: 0x%x", crc16);
    esp_log_buffer_hex(TAG, data, len);

    return data[1]; /* 返回接收的功能码 */
}

bool modbus_addr_check(uint16_t addr, uint16_t num)
{
    for (int i = 0; i < sizeof(g_modbus_addr)/sizeof(modbus_addr_map_t); i++)
    {
        // ESP_LOGI(TAG, "modbus addr check %d: %u, %u\n", i, addr, g_modbus_addr[i].public_addr);
        if (addr == g_modbus_addr[i].public_addr && 0 != g_modbus_addr[i].num)
        {
            // 校验地址个数范围内的地址是否均合法
            if (num > g_modbus_addr[i].num && 0 != g_modbus_addr[i].num)
            {
                //TODO: 不能使用递归???
                // 使用递归, 根据列表逐个检测每个地址
                return modbus_addr_check(addr + g_modbus_addr[i].num, num - g_modbus_addr[i].num);
            }

            return true;
        }
    }

    return false;
}

/*!
 * @brief modbus地址转换，考虑到信息安全，仅对外公开部分地址
 * @TODO: 目前仅支持单个变量地址的读写，不支持连续读多个
 */
int modbus_addr_pri_to_pub(char* addr)
{
    if (NULL == addr)
    {
        return 0;
    }

    uint16_t pri_addr = ((addr[0] << 8) & 0xFF) | addr[1];
    for (int i = 0; i < sizeof(g_modbus_addr)/sizeof(modbus_addr_map_t); i++)
    {
        if (pri_addr == g_modbus_addr[i].private_addr)
        {
            uint16_t pub_addr = g_modbus_addr[i].public_addr;
            addr[0] = (pub_addr >> 8) & 0xFF;
            addr[1] = pub_addr & 0xFF;
            return 1;
        }
    }

    return 0;
}

int modbus_addr_pub_to_pri(unsigned char* data)
{
    if (NULL == data)
    {
        return 0;
    }

    uint16_t pub_addr = ((data[2] << 8) & 0xFF00) | data[3];
    uint16_t number = 0;
    uint8_t func = data[1];

    if (func == MODBUS_FC_WRITE_SINGLE_REGISTER)
    {
        number = 1;
    }
    else
    {
        number = ((data[4] << 8) & 0xFF00) | data[5];
    }

    if (false == modbus_addr_check(pub_addr, number))
    {
        ESP_LOGE(TAG, "modbus addr rang error\n");
        return 0;
    }

    for (int i = 0; i < sizeof(g_modbus_addr)/sizeof(modbus_addr_map_t); i++)
    {
        if (pub_addr == g_modbus_addr[i].public_addr)
        {
            /** 权限管理：如果为写命令，但是权限为只读，则拒绝 */
            if ((func == MODBUS_FC_WRITE_SINGLE_REGISTER || func == MODBUS_FC_WRITE_MULTIPLE_REGISTERS)
                && g_modbus_addr[i].permission == PERMISSION_READ_ONLY)
            {
                ESP_LOGE(TAG, "modbus addr permission error");
                return 0;
            }

            uint16_t pri_addr = g_modbus_addr[i].private_addr;
            data[2] = (pri_addr >> 8) & 0xFF;
            data[3] = pri_addr & 0xFF;
            // printf(TAG" modbus addr pub to pri success \n");
            return 1;
        }
    }

    ESP_LOGE(TAG, "not found modbus addr\n");
    return 0;
}

/**
 * @brief 将公共地址范围转换为私有地址
 * @param pub_start 起始公共地址
 * @param num 要读取/写入的寄存器数量
 * @param segments 输出段数组(私有地址范围)
 * @param max_segments 数组可以容纳的最大段数
 * @return 创建的段数，错误时返回-1
 */
int modbus_addr_range_pub_to_pri(uint16_t addr, uint16_t num,
                                 modbus_addr_map_t *segments, int max_segments)
{
    if (num == 0 || !segments || max_segments <= 0)
    {
        ESP_LOGE(TAG, "modbus_addr_range_pub_to_pri param error, num = %d, segments = %p, max_segments = %d",
                 num, segments, max_segments);
        return -1;
    }

    int segment_count = 0;
    uint16_t pub_current = addr;
    uint16_t num_remaining = num;

    while (num_remaining > 0 && segment_count < max_segments)
    {
        // 1. 查找当前外部地址 pub_current 的映射条目
        modbus_addr_map_t *map_entry = NULL;
        for (int i = 0; i < sizeof(g_modbus_addr)/sizeof(modbus_addr_map_t); i++)
        {
            // 检查 g_modbus_addr[i] 是否定义了 pub_current 的映射
            // 注意：g_modbus_addr[i].num 是该条目定义的地址范围大小
            if (pub_current >= g_modbus_addr[i].public_addr &&
                pub_current < (g_modbus_addr[i].public_addr + g_modbus_addr[i].num))
            {
                map_entry = &g_modbus_addr[i];
                break;
            }
        }

        // 2. 错误处理：如果找不到当前外部地址的映射
        if (!map_entry)
        {
            ESP_LOGE(TAG, "Public address %u is not found in addrmap(g_modbus_addr)", pub_current);
            return -1; // 地址无效
        }

        // 3. 计算可以从当前 map_entry 映射多少个连续的寄存器
        //    确保不超过请求的剩余数量 num_remaining
        //    也确保不超过 map_entry 自身定义的数量范围
        uint16_t map_entry_start_offset = pub_current - map_entry->public_addr; // 在 map_entry 范围内的偏移
        uint16_t max_consecutive_from_entry = map_entry->num - map_entry_start_offset; // 从当前地址起，map_entry 还能提供多少个
        uint16_t consecutive_count = MIN(num_remaining, max_consecutive_from_entry);

        // 4. 确定是合并到上一个段，还是开启一个新段
        bool can_merge = false;
        if (segment_count > 0)
        {
            // 获取上一个段的信息
            modbus_addr_map_t *last_segment = &segments[segment_count - 1];
            uint16_t last_segment_end_pri_addr = last_segment->private_addr + last_segment->num - 1;
            uint16_t current_start_pri_addr = map_entry->private_addr + map_entry_start_offset;

            // 判断是否可以合并：上一个段的结束内部地址是否等于当前段起始内部地址 - 1
            if (last_segment_end_pri_addr + 1 == current_start_pri_addr)
            {
                can_merge = true;
            }
        }

        if (can_merge && segment_count > 0)
        {
            // 5a. 合并到上一个段：增加上一个段的 num
            segments[segment_count - 1].num += consecutive_count;
            ESP_LOGI(TAG, "Merged into segment: pri=%u, pub=%u, num=%u",
                     segments[segment_count - 1].private_addr,
                     segments[segment_count - 1].public_addr,
                     segments[segment_count - 1].num);
        } 
        else
        {
            // 5b. 开启一个新段
            if (segment_count >= max_segments)
            {
                 ESP_LOGE(TAG, "Segment array overflow");
                 return -1;
            }
            segments[segment_count].private_addr = map_entry->private_addr + map_entry_start_offset;
            segments[segment_count].public_addr = pub_current; // 或者 map_entry->public_addr + map_entry_start_offset;
            segments[segment_count].num = consecutive_count;
            segment_count++;
            ESP_LOGI(TAG, "New segment: pri=%u, pub=%u, num=%u",
                     segments[segment_count - 1].private_addr,
                     segments[segment_count - 1].public_addr,
                     segments[segment_count - 1].num);
        }

        // 6. 更新循环变量：移动到下一个需要处理的外部地址块
        pub_current += consecutive_count;
        num_remaining -= consecutive_count;
    }

    if (num_remaining > 0)
    {
        ESP_LOGE(TAG, "Has not enough segments or mapping to cover all requested addresses");
        return -1; // 理论上不会发生，因为上面循环里检查了max_segments 和 map_entry
    }

    ESP_LOGI(TAG, "Successfully mapped public range [%u, %u] to %d segment(s)", addr, addr + num - 1, segment_count);
    return segment_count;
}

static int modbus_data_handle_read_multi_regs(int sockfd, uint8_t *data, modbus_addr_map_t *segments, int segment_count)
{
	ESP_LOGI(TAG, "++++++Enter modbus_data_handle_read_multi_regs");

    int ret = 0;
    uint8_t md_addr = data[_MODBUS_TCP_DATA_OFFSET]; // Slave ID
    uint8_t read_rsp[MODBUS_TCP_MAX_ADU_LENGTH] = {0};

    memcpy(read_rsp, data, _MODBUS_TCP_DATA_OFFSET); // MBAP(6Bytes)
    read_rsp[_MODBUS_TCP_DATA_OFFSET] = md_addr;    // slave ID
    read_rsp[_MODBUS_TCP_DATA_OFFSET + 1] = MODBUS_FC_READ_HOLDING_REGISTERS; //function code
    read_rsp[_MODBUS_TCP_DATA_OFFSET + 2] = 0; // byte count

    uint16_t total_bytes_read = 0;

    for (int i = 0; i < segment_count; i++)
    {
        uint16_t seg_data_bytes = segments[i].num * 2;

        if (total_bytes_read + seg_data_bytes > (sizeof(read_rsp) - (_MODBUS_TCP_DATA_OFFSET + 3)))
        {
            ESP_LOGE(TAG, "Read response buffer overflow");
            ret = 0;
            goto send_error;
        }

        uint8_t temp_req[264] = {0};
        int temp_req_len = 6; // Slave ID + Func Code + Addr(2B) + Count(2B)
        memcpy(temp_req, data, _MODBUS_TCP_DATA_OFFSET); // MBAP
        temp_req[_MODBUS_TCP_DATA_OFFSET] = md_addr;
        temp_req[_MODBUS_TCP_DATA_OFFSET + 1] = MODBUS_FC_READ_HOLDING_REGISTERS;
        temp_req[_MODBUS_TCP_DATA_OFFSET + 2] = (segments[i].private_addr >> 8) & 0xFF;
        temp_req[_MODBUS_TCP_DATA_OFFSET + 3] = segments[i].private_addr & 0xFF;
        temp_req[_MODBUS_TCP_DATA_OFFSET + 4] = (segments[i].num >> 8) & 0xFF;
        temp_req[_MODBUS_TCP_DATA_OFFSET + 5] = segments[i].num & 0xFF;

        ret = modbus_tcp_format_check(&temp_req[_MODBUS_TCP_DATA_OFFSET], temp_req_len + 2);
        if (ret < 0)
        {
            ESP_LOGE(TAG, "modbus_tcp_format_check failed for read segment %d, ret = %d", i, ret);
            ret = 0;
            goto send_error;
        }

        uint8_t temp_rsp[264] = {0};
        reg_position_list_t *position_list = NULL;
#if CONFIG_CAN_SUPPORT
        can_cmd_queue_struct dummy_can_cmd = {NULL, 0, 0};
        int16_t rsp_len = Modbus_Slave(&temp_req[_MODBUS_TCP_DATA_OFFSET], temp_req_len + 2,
                                    &temp_rsp[_MODBUS_TCP_DATA_OFFSET],
                                    dummy_can_cmd.cmd, &dummy_can_cmd.num, MD_CHL_TCP, &position_list);
#else
        int16_t rsp_len = Modbus_Slave(&temp_req[_MODBUS_TCP_DATA_OFFSET], temp_req_len + 2,
                                    &temp_rsp[_MODBUS_TCP_DATA_OFFSET], MD_CHL_TCP, &position_list);
#endif

        if (rsp_len <= 0 || (temp_rsp[_MODBUS_TCP_DATA_OFFSET + 1] & 0x7F) != MODBUS_FC_READ_HOLDING_REGISTERS)
        {
            ESP_LOGE(TAG, "Modbus_Slave read failed for segment %d, rsp_len=%d, fc=%d",
                    i, rsp_len, temp_rsp[_MODBUS_TCP_DATA_OFFSET + 1]);
            if (rsp_len > 0) //Modbus_Slave返回了错误则透传，
            {
                memcpy(&read_rsp[_MODBUS_TCP_DATA_OFFSET], &temp_rsp[_MODBUS_TCP_DATA_OFFSET], rsp_len);
                read_rsp[_MODBUS_TCP_DATA_OFFSET] = md_addr;
                send_msg(sockfd, read_rsp, rsp_len + _MODBUS_TCP_DATA_OFFSET - 2);
            }
            return 0;
        }

        uint8_t seg_rsp_byte_count = temp_rsp[_MODBUS_TCP_DATA_OFFSET + 2];
        if (seg_rsp_byte_count != seg_data_bytes)
        {
            ESP_LOGE(TAG, "Segment %d read byte count mismatch: expected %u, got %u", i, seg_data_bytes, seg_rsp_byte_count);
            ret = 0;
            goto send_error;
        }

        memcpy(&read_rsp[_MODBUS_TCP_DATA_OFFSET + 3 + total_bytes_read],
               &temp_rsp[_MODBUS_TCP_DATA_OFFSET + 3],
               seg_rsp_byte_count);
        total_bytes_read += seg_rsp_byte_count;
    }

    // Send successful response
    read_rsp[_MODBUS_TCP_DATA_OFFSET + 2] = (uint8_t)(total_bytes_read & 0xFF);
    ret = send_msg(sockfd, read_rsp, _MODBUS_TCP_DATA_OFFSET + 3 + total_bytes_read);
    ESP_LOGI(TAG, "Multi-read response sent, len: %d", _MODBUS_TCP_DATA_OFFSET + 3 + total_bytes_read);
	ESP_LOG_BUFFER_HEX(TAG, read_rsp, _MODBUS_TCP_DATA_OFFSET + 3 + total_bytes_read);

    if (vXmodemCmdCheck(data[0], CHANNEL_BLE))
    {
        ESP_LOGW(TAG, "ble received xmodem start cmd");
    }
    return ret;

send_error:
    // Send error response
    uint8_t error_rsp[264] = {0};
    if (_MODBUS_TCP_DATA_OFFSET + 3 > sizeof(error_rsp))
    {
        ESP_LOGE(TAG, "Error response buffer too small");
        return 0;
    }
    memcpy(error_rsp, data, _MODBUS_TCP_DATA_OFFSET);
    error_rsp[_MODBUS_TCP_DATA_OFFSET] = md_addr;
    error_rsp[_MODBUS_TCP_DATA_OFFSET + 1] = MODBUS_FC_READ_HOLDING_REGISTERS | 0x80;
    error_rsp[_MODBUS_TCP_DATA_OFFSET + 2] = CMD_NOT_COMPLETE;
    send_msg(sockfd, error_rsp, _MODBUS_TCP_DATA_OFFSET + 3 - 2);
    ESP_LOGI(TAG, "Multi-read error response sent, len: %d", _MODBUS_TCP_DATA_OFFSET + 3 - 2);
    ESP_LOG_BUFFER_HEX(TAG, error_rsp, _MODBUS_TCP_DATA_OFFSET + 3 - 2);
    return 0;
}

static int modbus_data_handle_write_multi_regs(int sockfd, uint8_t *data, modbus_addr_map_t *segments, int segment_count, int reg_count)
{
    ESP_LOGI(TAG, "++++++Enter modbus_data_handle_write_multi_regs");

    int ret = 0;
    uint8_t md_addr = data[_MODBUS_TCP_DATA_OFFSET]; // Slave ID
    uint8_t start_addr[2] = {0};
    start_addr[0] = data[_MODBUS_TCP_DATA_OFFSET + 2];
    start_addr[1] = data[_MODBUS_TCP_DATA_OFFSET + 3];
    bool all_writes_successful = true;
    uint16_t total_regs_written = 0; // 写的总寄存器数量

    // 7表示：SlaveID(1) + Func Code(1) + Addr(2) + 寄存器数量(2) + 数据长度(1) = 7 字节
    uint8_t *write_data_ptr = &data[_MODBUS_TCP_DATA_OFFSET + 7];   //指向具体数据
    uint8_t byte_count = data[_MODBUS_TCP_DATA_OFFSET + 6];         //数据长度
    if (byte_count != reg_count * 2)
    {
        ESP_LOGE(TAG, "Write byte count unmatch: byte_count = %u, Count*2 = %u", byte_count, reg_count*2);
        uint8_t error_rsp[64] = {0};
        memcpy(error_rsp, data, _MODBUS_TCP_DATA_OFFSET);
        error_rsp[_MODBUS_TCP_DATA_OFFSET] = md_addr;
        error_rsp[_MODBUS_TCP_DATA_OFFSET + 1] = MODBUS_FC_WRITE_MULTIPLE_REGISTERS | 0x80;
        error_rsp[_MODBUS_TCP_DATA_OFFSET + 2] = BAD_COUNT;
        send_msg(sockfd, error_rsp, _MODBUS_TCP_DATA_OFFSET + 3 - 2);
        return 0;
    }

    // 分别处理各个段
    for (int i = 0; i < segment_count; i++)
    {
        uint16_t seg_data_bytes = segments[i].num * 2;
        uint8_t temp_req[264] = {0};// MBAP + SlaveID + Func Code + Addr + 寄存器数量 + 数据长度 + 数据 + CRC
        int temp_req_len = 7 + seg_data_bytes; // 7 bytes header + data
        if (temp_req_len > sizeof(temp_req))
        {
            ESP_LOGE(TAG, "Temp request buffer too small for write segment %d (len %d)", i, temp_req_len);
            all_writes_successful = false;
            break;
        }
        memcpy(temp_req, data, _MODBUS_TCP_DATA_OFFSET); // MBAP
        temp_req[_MODBUS_TCP_DATA_OFFSET] = md_addr; // Slave ID
        temp_req[_MODBUS_TCP_DATA_OFFSET + 1] = MODBUS_FC_WRITE_MULTIPLE_REGISTERS; // Func Code
        temp_req[_MODBUS_TCP_DATA_OFFSET + 2] = (segments[i].private_addr >> 8) & 0xFF; // 地址高位
        temp_req[_MODBUS_TCP_DATA_OFFSET + 3] = segments[i].private_addr & 0xFF;        // 地址低位
        temp_req[_MODBUS_TCP_DATA_OFFSET + 4] = (segments[i].num >> 8) & 0xFF;    // 寄存器数量高位
        temp_req[_MODBUS_TCP_DATA_OFFSET + 5] = segments[i].num & 0xFF;           // 寄存器数量低位
        temp_req[_MODBUS_TCP_DATA_OFFSET + 6] = (uint8_t)(seg_data_bytes & 0xFF); // 写入的数据长度
        memcpy(&temp_req[_MODBUS_TCP_DATA_OFFSET + 7], write_data_ptr, seg_data_bytes);// 写入的数据

        write_data_ptr += seg_data_bytes;

        // modbus tcp格式校验: 不校验头部, 为了借用modbus slave接口(Modbus RTU)，在后面添加crc
        ret = modbus_tcp_format_check(&temp_req[_MODBUS_TCP_DATA_OFFSET], temp_req_len + 2);
        if (ret < 0)
        {
            ESP_LOGE(TAG, "modbus_tcp_format_check failed for write segment %d", i);
            all_writes_successful = false;
            break;
        }

        uint8_t temp_rsp[264] = {0};
#if CONFIG_CAN_SUPPORT
        can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};
        uint8_t can_cmd_flag = 0;
        if (can_cmd_queue)
        {
            can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
            if (!can_cmd.cmd) 
            {
                ESP_LOGE (TAG, "ble to can malloc failed");
            }
        }
        reg_position_list_t *position_list = NULL;
        int16_t rsp_len = Modbus_Slave(&temp_req[_MODBUS_TCP_DATA_OFFSET], temp_req_len + 2,
                                &temp_rsp[_MODBUS_TCP_DATA_OFFSET],
                                can_cmd.cmd, &can_cmd.num, MD_CHL_TCP, &position_list);
#else
        reg_position_list_t *position_list = NULL;
        int16_t rsp_len = Modbus_Slave(&temp_req[_MODBUS_TCP_DATA_OFFSET], temp_req_len,
                                &temp_rsp[_MODBUS_TCP_DATA_OFFSET], MD_CHL_TCP, &position_list);
#endif
        if (rsp_len > 0 && (temp_rsp[_MODBUS_TCP_DATA_OFFSET + 1] & 0x7F) == MODBUS_FC_WRITE_MULTIPLE_REGISTERS)
        {
            ESP_LOGI(TAG,"Multi Write temprsp:");
            ESP_LOG_BUFFER_HEX(TAG, temp_rsp, _MODBUS_TCP_DATA_OFFSET + rsp_len);

            // // 检查当前段的写入地址和寄存器数量是否正确，出错了直接break 
            // // 测试过程中发现Modbus_Slave回复的响应并不是标准响应，忽略这个检查，只要rsp_len>0就认为目前写入成功，手动组织响应报文。
            // uint16_t rsp_addr = (temp_rsp[_MODBUS_TCP_DATA_OFFSET + 2] << 8) | temp_rsp[_MODBUS_TCP_DATA_OFFSET + 3];
            // uint16_t rsp_count = (temp_rsp[_MODBUS_TCP_DATA_OFFSET + 4] << 8) | temp_rsp[_MODBUS_TCP_DATA_OFFSET + 5];

            // if (rsp_addr == segments[i].private_addr && rsp_count == segments[i].num)
            // {
                total_regs_written += segments[i].num;
                ESP_LOGI(TAG, "Segment %d write successful: addr=%u, count=%u", i, segments[i].private_addr, segments[i].num);

#if CONFIG_CAN_SUPPORT
                /* modbus指令转换为can指令发送到队列 */
                if (can_cmd.cmd != NULL && can_cmd.num != 0)
                {
                    can_cmd.md_addr = md_addr;
                    if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(100)) == pdTRUE)
                    {
                        can_cmd_flag = 1;
                        reals.BLE_WIFI_to_Can_Cnt++;
                        ESP_LOGI(TAG, "Write segment %d to CAN count:%d", i, reals.BLE_WIFI_to_Can_Cnt);
                    }
                }
                if (!can_cmd_flag && can_cmd.cmd)
                {
                    free(can_cmd.cmd);
                }
#elif CONFIG_UART_SUPPORT
                if (NULL != position_list)
                {
                    sys_new_position_and_transmit(position_list);//透传转发给下级uart
                }
#endif
            // }
            // else
            // {
            //     ESP_LOGE(TAG, "Segment %d write response mismatch: exp_addr=%u, rsp_addr=%u, exp_count=%u, rsp_count=%u",
            //             i, segments[i].private_addr, rsp_addr, segments[i].num, rsp_count);
            //     all_writes_successful = false;
            //     break;
            // }
        }
        else
        {
            //如果Modbus Slave 返回了错误则直接透传
            ESP_LOGE(TAG, "Modbus_Slave write failed for segment %d, rsp_len=%d, fc=%d",
                    i, rsp_len, temp_rsp[_MODBUS_TCP_DATA_OFFSET + 1]);
            all_writes_successful = false;
            if (rsp_len > 0)
            {
                temp_rsp[_MODBUS_TCP_DATA_OFFSET] = md_addr;
                send_msg(sockfd, temp_rsp, rsp_len + _MODBUS_TCP_DATA_OFFSET - 2);
                return 0;
            }
            break;
        }
    }

    if (all_writes_successful && total_regs_written == reg_count)
    {
        //每个段都写入成功后，组织响应报文
        uint8_t confirm_rsp[264] = {0};
        memcpy(confirm_rsp, data, _MODBUS_TCP_DATA_OFFSET); // MBAP
        confirm_rsp[_MODBUS_TCP_DATA_OFFSET] = md_addr; // Slave ID
        confirm_rsp[_MODBUS_TCP_DATA_OFFSET + 1] = MODBUS_FC_WRITE_MULTIPLE_REGISTERS; // Func Code
        confirm_rsp[_MODBUS_TCP_DATA_OFFSET + 2] = start_addr[0];
        confirm_rsp[_MODBUS_TCP_DATA_OFFSET + 3] = start_addr[1];
        confirm_rsp[_MODBUS_TCP_DATA_OFFSET + 4] = data[_MODBUS_TCP_DATA_OFFSET + 4]; // 寄存器数量高位
        confirm_rsp[_MODBUS_TCP_DATA_OFFSET + 5] = data[_MODBUS_TCP_DATA_OFFSET + 5]; // 寄存器数量低位

        ret = send_msg(sockfd, confirm_rsp, _MODBUS_TCP_DATA_OFFSET + 6); // 6 bytes response - 2 CRC calc
        ESP_LOGI(TAG, "Multi-write confirmation sent, len: %d", _MODBUS_TCP_DATA_OFFSET + 6);
        ESP_LOG_BUFFER_HEX(TAG, confirm_rsp, _MODBUS_TCP_DATA_OFFSET + 6);

        if (vXmodemCmdCheck(data[0], CHANNEL_BLE)) //初始化xmodem升级变量
        {
            ESP_LOGW(TAG, "ble received xmodem start cmd");
        }
    }
    else if(all_writes_successful == false && ret >= 0)
    {
        //如果前面处理每个段出现了错误，在这里组织通用错误报文
        uint8_t error_rsp[64] = {0};
        memcpy(error_rsp, data, _MODBUS_TCP_DATA_OFFSET);
        error_rsp[_MODBUS_TCP_DATA_OFFSET] = md_addr;
        error_rsp[_MODBUS_TCP_DATA_OFFSET + 1] = MODBUS_FC_WRITE_MULTIPLE_REGISTERS | 0x80;
        error_rsp[_MODBUS_TCP_DATA_OFFSET + 2] = CMD_NOT_COMPLETE;
        send_msg(sockfd, error_rsp, _MODBUS_TCP_DATA_OFFSET + 3 - 2);
        ESP_LOGI(TAG, "Multi-write error response sent, len: %d", _MODBUS_TCP_DATA_OFFSET + 3 - 2);
        ESP_LOG_BUFFER_HEX(TAG, error_rsp, _MODBUS_TCP_DATA_OFFSET + 3 - 2);
    }
    return ret;
}

static int modbus_data_handle_multi_regs(int sockfd, uint8_t *data, int len, int reg_count)
{
    int ret = 0;
    uint8_t function_code = data[_MODBUS_TCP_DATA_OFFSET + 1];
    uint8_t start_addr[2] = {0};
    start_addr[0] = data[_MODBUS_TCP_DATA_OFFSET + 2];
    start_addr[1] = data[_MODBUS_TCP_DATA_OFFSET + 3];
    uint16_t pub_start_addr = (start_addr[0] << 8) | start_addr[1];

    ESP_LOGI(TAG, "++++++Enter modbus_data_handle_multi_regs: function_code = %u, pub_start_addr = %u, reg_count = %d", function_code, pub_start_addr, reg_count);

    //外部地址转内部地址，当连续访问50001~50005 预期得到两段内容分别是：50001-120-1 50002-142-4
    int segment_count = modbus_addr_range_pub_to_pri(pub_start_addr, reg_count, segments, REGISTER_NUM_MAX);
    if (segment_count <= 0)
    {
        ESP_LOGE(TAG, "modbus_addr_range_pub_to_pri failed: addr %u, count %d", pub_start_addr, reg_count);
        return 0;
    }
	
	//分别对每一段进行处理，同时 区分不同的function code 03-读
    if (function_code == MODBUS_FC_READ_HOLDING_REGISTERS)
    {
        ret = modbus_data_handle_read_multi_regs(sockfd, data, segments, segment_count);
        if (ret <= 0)
        {
            ESP_LOGE(TAG, "modbus_data_handle_read_multi_regs failed: ret = %d", ret);
        }
    } 
    else if (function_code == MODBUS_FC_WRITE_MULTIPLE_REGISTERS)
    {
        ret = modbus_data_handle_write_multi_regs(sockfd, data, segments, segment_count, reg_count);
        if (ret <= 0)
        {
            ESP_LOGE(TAG, "modbus_data_handle_write_multi_regs failed: ret = %d", ret);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Unsupported function code : %u", function_code);

        uint8_t error_rsp[64] = {0};

        memcpy(error_rsp, data, _MODBUS_TCP_DATA_OFFSET);
        error_rsp[_MODBUS_TCP_DATA_OFFSET] = data[_MODBUS_TCP_DATA_OFFSET];// Slave address
        error_rsp[_MODBUS_TCP_DATA_OFFSET + 1] = function_code | 0x80;
        error_rsp[_MODBUS_TCP_DATA_OFFSET + 2] = FCN_NOT_SUPPORTED;
        send_msg(sockfd, error_rsp, _MODBUS_TCP_DATA_OFFSET + 3 - 2);
        return 0;
    }

    ESP_LOGI(TAG, "++++++Exit modbus_data_handle_multi_regs: ret = %d", ret);
    return ret;
}

/*
 * @brief modbus tcp slave 处理函数
 * @param sockfd: socket fd
 * @param data: 接收数据
 * @param len: 数据长度
 * @return 0: 成功, -1: modbus tcp连接异常
 */
static int modbus_data_handle(int sockfd, uint8_t *data, int len)
{
    int16_t rsp_len = 0;
    uint8_t md_addr = 0;
    int ret = 0;
    uint8_t ble_resp_data[264] = {0};
    uint8_t start_addr[2] = {0};

    md_addr = data[_MODBUS_TCP_DATA_OFFSET];
    start_addr[0] = data[_MODBUS_TCP_DATA_OFFSET + 2];
    start_addr[1] = data[_MODBUS_TCP_DATA_OFFSET + 3];

    // 获取功能码和寄存器数量
    uint8_t function_code = data[_MODBUS_TCP_DATA_OFFSET + 1];
    uint16_t reg_count = 0;

    if (function_code == MODBUS_FC_READ_HOLDING_REGISTERS ||
        function_code == MODBUS_FC_WRITE_MULTIPLE_REGISTERS)
    {
        reg_count = (data[_MODBUS_TCP_DATA_OFFSET + 4] << 8) |
                    data[_MODBUS_TCP_DATA_OFFSET + 5];
    }
    else if (function_code == MODBUS_FC_WRITE_SINGLE_REGISTER)
    {
        reg_count = 1;
    }

#if CONFIG_LOG_UDP_REDIRECT == 2 
    supper_control_resource_t resource = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_RESOURCE].value };
    supper_control_direction_t direction = { .value = reals.supper_control.cmd_list[SUPPER_CMD_LOG_DIRECTION].value };
    if ((1 == resource.modbus_tcp) && (1 == direction.rx)) {
        aiot_log_push((const uint8_t *)data, (uint16_t)len, LOG_TYPE_MODBUS_TCP_RX);
    }
#endif

    //多寄存器特殊处理，单寄存器走原有逻辑
    if(reg_count > 1)
    {
        return modbus_data_handle_multi_regs(sockfd, data, len, reg_count);
    }

    //在添加crc之前，对外地址转换为内部私有地址
    ret = modbus_addr_pub_to_pri(&data[_MODBUS_TCP_DATA_OFFSET]);
    if (ret == 0)
    {
        ble_resp_data[_MODBUS_TCP_DATA_OFFSET] = md_addr;
        ble_resp_data[_MODBUS_TCP_DATA_OFFSET + 1] = data[_MODBUS_TCP_DATA_OFFSET + 1];
        len = modbus_error(&ble_resp_data[_MODBUS_TCP_DATA_OFFSET], UNKNOWN_REG_ADDRESS);

        /* modbus tcp请求帧的头部复制到响应帧*/
        memcpy(ble_resp_data, data, _MODBUS_TCP_DATA_OFFSET);

        /* modbus tcp响应帧发送到client端.
        注意: 内部会根据实际数据长度更新modbus tcp协议头中的长度字段 */
        ret = send_msg(sockfd, ble_resp_data, len + _MODBUS_TCP_DATA_OFFSET);

        return 0;
    }

    // modbus tcp格式校验: 不校验头部, 为了借用modbus slave接口(Modbus RTU)，在后面添加crc
    ret = modbus_tcp_format_check(&data[_MODBUS_TCP_DATA_OFFSET], len - _MODBUS_TCP_DATA_OFFSET + 2);
    if (ret < 0)//功能码
    {
        ESP_LOGE(TAG, "modbus function code error:%d", ret);

        ble_resp_data[_MODBUS_TCP_DATA_OFFSET] = md_addr;
        ble_resp_data[_MODBUS_TCP_DATA_OFFSET + 1] = data[_MODBUS_TCP_DATA_OFFSET + 1];
        len = modbus_error(&ble_resp_data[_MODBUS_TCP_DATA_OFFSET], FCN_NOT_SUPPORTED);

        /* modbus tcp请求帧的头部复制到响应帧*/
        memcpy(ble_resp_data, data, _MODBUS_TCP_DATA_OFFSET);

        /* modbus tcp响应帧发送到client端.
        注意: 内部会根据实际数据长度更新modbus tcp协议头中的长度字段 */
        ret = send_msg(sockfd, ble_resp_data, len + _MODBUS_TCP_DATA_OFFSET);

        return 0;
    }
#if CONFIG_CAN_SUPPORT
    can_cmd_queue_struct can_cmd = {NULL, MODBUS_TO_CAN_MAX_NUM, 0};
    uint8_t can_cmd_flag = 0;
    if ((ret == 0x06 || ret == 0x10) && can_cmd_queue) 
    {
        // 当MODBUS为设置指令时,才需要开辟空间
        // can_cmd.cmd = heap_caps_malloc(sizeof(can_data_label) * can_cmd.num, MALLOC_CAP_SPIRAM);
        can_cmd.cmd = iot_calloc(sizeof(can_data_label) * can_cmd.num);
        if (!can_cmd.cmd)
        {
            ESP_LOGE (TAG, "ble to can malloc failed");
        }
    }

    reg_position_list_t *position_list = NULL;
    rsp_len = Modbus_Slave(&data[_MODBUS_TCP_DATA_OFFSET], (uint16_t)(len - _MODBUS_TCP_DATA_OFFSET + 2),
                    &ble_resp_data[_MODBUS_TCP_DATA_OFFSET],  can_cmd.cmd, &can_cmd.num, MD_CHL_TCP, &position_list); /* modbus handle */
#else
    reg_position_list_t *position_list = NULL;
    rsp_len = Modbus_Slave(&data[_MODBUS_TCP_DATA_OFFSET], (uint16_t)(len - _MODBUS_TCP_DATA_OFFSET + 2),
                    &ble_resp_data[_MODBUS_TCP_DATA_OFFSET], MD_CHL_TCP, &position_list); /* modbus handle */
#endif
    if (rsp_len > 0)
    {
        len = 0;

        /* modbus tcp请求帧的头部复制到响应帧*/
        memcpy(ble_resp_data, data, _MODBUS_TCP_DATA_OFFSET);

        // 地址再更改回私有地址.
        data[_MODBUS_TCP_DATA_OFFSET + 2] = start_addr[0];
        data[_MODBUS_TCP_DATA_OFFSET + 3] = start_addr[1];

        //寄存器地址修改回Modbus TCP.
        ble_resp_data[_MODBUS_TCP_DATA_OFFSET] = md_addr;

        /* modbus tcp响应帧发送到client端.
        注意: 内部会根据实际数据长度更新modbus tcp协议头中的长度字段 */
        ret = send_msg(sockfd, ble_resp_data, rsp_len + _MODBUS_TCP_DATA_OFFSET - 2);

        ESP_LOGI(TAG, " response data len: %d, value:\n", rsp_len + _MODBUS_TCP_DATA_OFFSET - 2);
        ESP_LOG_BUFFER_HEX(TAG, ble_resp_data, rsp_len + _MODBUS_TCP_DATA_OFFSET - 2);

        if (vXmodemCmdCheck(data[0], CHANNEL_BLE)) //初始化xmodem升级变量
        {
            ESP_LOGW(TAG, "ble received xmodem start cmd");
        }
#if CONFIG_CAN_SUPPORT
        /* modbus指令转换为can指令发送到队列 */
        if (can_cmd.cmd != NULL && can_cmd.num != 0) 
        {
            can_cmd.md_addr = md_addr;
            if (xQueueSendToBack(can_cmd_queue, &can_cmd, pdMS_TO_TICKS(100)) == pdTRUE) 
            {
                can_cmd_flag = 1;
                reals.BLE_WIFI_to_Can_Cnt++;
                ESP_LOGE(TAG, "modbus tcp to can count:%d", reals.BLE_WIFI_to_Can_Cnt);
            }
        }

        if (!can_cmd_flag && can_cmd.cmd)
        {
            free(can_cmd.cmd);
        }
#elif CONFIG_UART_SUPPORT
        if (NULL != position_list)
        {
            sys_new_position_and_transmit(position_list);//透传转发给下级uart
        }
#endif
    }

    return ret;
}

/**
 * @brief 更新modbus tcp服务端口
 * @note 端口可配置(第二服务器端口)，如果没有配置第二服务器，则使用默认端口502
 */
int modbus_tcp_service_update(char *service, uint16_t size)
{
    if (IotSetData.dev_info_t.modbus_tcp_port == 0)
    {
        memcpy(service, MODBUS_TCP_SERVICE_DEFAULT, strlen(MODBUS_TCP_SERVICE_DEFAULT));
        return strlen(MODBUS_TCP_SERVICE_DEFAULT); //没有配置端口使用默认502
    }

    snprintf(service, size, "%d", IotSetData.dev_info_t.modbus_tcp_port);

    return strlen(service);
}

/*
 * @brief 创建一个TCP server, 监听Modbus TCP请求
 * @note  监听IP为任意IP(0.0.0.0),默认端口502(modbus地址22005)
 */
void modbus_tcp_task(void)
{
    int ret = -1;
    static int retry_count = 0;

    if (_RUN_STATE_IDLE == g_tcp_run_status)
    {
        return;
    }

    switch(g_tcp_run_status)
    {
        case _RUN_STATE_INIT:
        {
            modbus_tcp_server_init();
            retry_count = 0;
            g_tcp_run_status = _RUN_STATE_OPEN;
        }
        break;

        case _RUN_STATE_OPEN:
        {
            ret = md_tcp_svr.open(&md_tcp_svr);
            if (ret < 0)
            {
                ESP_LOGE(TAG, "modbus tcp open error");
                break;
            }

            g_tcp_run_status = _RUN_STATE_LISTENNING;
        }
        break;

        case _RUN_STATE_LISTENNING:
        {
            /* 监听客户端的连接 */
            ret = md_tcp_svr.listen(&md_tcp_svr);
            if (ret < 0)
            {
                ESP_LOGE(TAG, "modbus tcp listen error");
                // vTaskDelay(pdMS_TO_TICKS(500));
                break;
            }

            ESP_LOGI(TAG, "modbus tcp listen service[%s:%s], fd:%d", MODBUS_TCP_NODE, md_tcp_svr.config.port, ret);
            g_tcp_run_status = _RUN_STATE_ACCEPT;
        }
        break;

        case _RUN_STATE_ACCEPT:
        {
            /* 接收客户端的连接 */
            ret = md_tcp_svr.accept(&md_tcp_svr);
            if (ret < 0)
            {
                ESP_LOGW(TAG, "modbus tcp accept error:%d", ret);
                break;
            }
            else if (ret == 0)
            {
                // ESP_LOGW(TAG, "modbus tcp accept timeout");
                break; // 继续等待连接
            }

            ESP_LOGI(TAG, "The modbus tcp is established:%d", ret);
            g_tcp_run_status = _RUN_STATE_RUNNING;
        }
        break;

        case _RUN_STATE_RUNNING:
        {
            ret = _modbus_tcp_receive(md_tcp_svr.query);
            if (ret == 0)
            {
                return; // 继续等待数据
            }

            if (ret == -1)
            {
                retry_count++;
                if (retry_count < 5)
                {
                    /* 断开旧连接 */
                    md_tcp_svr.reset(&md_tcp_svr);
                    g_tcp_run_status = _RUN_STATE_ACCEPT;
                    ESP_LOGE(TAG, "The modbus tcp is reset");
                }
                else
                {
                    retry_count = 0;
                    /* 断开旧连接 */
                    g_tcp_run_status = _RUN_STATE_RESTART;
                    ESP_LOGE(TAG, "The modbus tcp is restart");
                }
                break;
            }

            /* Process the query */
            if (ret > 0)
            {
                retry_count = 0;
                ESP_LOGI(TAG, "modbus tcp recv len:%d", ret);
                esp_log_buffer_hex(TAG, md_tcp_svr.query, ret);
                ret = modbus_data_handle(ret, md_tcp_svr.query, ret);
                if (ret == -1)
                {
                    /* 断开旧连接 */
                    md_tcp_svr.reset(&md_tcp_svr);
                    g_tcp_run_status = _RUN_STATE_ACCEPT;
                    ESP_LOGE(TAG, "modbus tcp send error");
                    break;
                }
            }
        }
        break;

        case _RUN_STATE_RESTART:
        {
            /* 断开旧连接 */
            md_tcp_svr.close(&md_tcp_svr);

            g_tcp_run_status = _RUN_STATE_INIT;
            ESP_LOGI(TAG, "modbus tcp restart");
        }
        break;

        case _RUN_STATE_EXIT:
        {
            /* 回收socket连接 */
            md_tcp_svr.close(&md_tcp_svr);
            iot_free(md_tcp_svr.query);
            g_tcp_run_status = _RUN_STATE_IDLE;
            ESP_LOGW(TAG, "Modbus TCP exit");

            break;
        }

        default:
        {
            ESP_LOGE(TAG, "modbus tcp run status error");
        }
        break;
    }
}

/**
 * @brief 第一次启动Modbus TCP Slave
 * @param is_crypted: 是否加密
 * @param port: 监听端口号
 * @param block: 是否阻塞模式运行
 * @return void
 * @note 若已经启动，则不再重复启动
 */
void modbus_tcp_start(uint8_t is_crypted, uint16_t port, uint8_t block)
{
    if (1 != IotSetData.dev_info_t.modbus_tcp_enable.enable)
    {
        ESP_LOGW(TAG, "Modbus TCP disabled.");
        return;
    }

    if (g_tcp_run_status != _RUN_STATE_IDLE)
    {
        ESP_LOGW(TAG, "Modbus TCP already.");
        return;
    }

    modbus_tcp_server_config_set(is_crypted, port, block);

    g_tcp_run_status = _RUN_STATE_INIT;
}

/**
 * @brief 停止Modbus TCP Slave
 */
void modbus_tcp_stop(void)
{
    if (g_tcp_run_status != _RUN_STATE_IDLE)
    {
        g_tcp_run_status = _RUN_STATE_EXIT;
    }
}

/**
 * @brief 以新的参数重启Modbus TCP Slave
 * @param is_crypted: 是否加密
 * @param port: 监听端口号
 * @param block: 是否阻塞模式运行
 * @return void
 * @note 重启时会重新初始化Modbus TCP服务器，释放之前的资源
 */
void modbus_tcp_restart(uint8_t is_crypted, uint16_t port, uint8_t block)
{
    modbus_tcp_server_config_set(is_crypted, port, block);
    g_tcp_run_status = _RUN_STATE_RESTART;
}

/**
 * @brief 检查Modbus TCP是否正在运行
 * @return true: 正在运行, false: 已停止或未启动
 */
bool modbus_tcp_is_running(void)
{
    return (g_tcp_run_status == _RUN_STATE_IDLE) ? false : true;
}
