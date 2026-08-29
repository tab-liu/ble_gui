/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi_types.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem.h"
#include "esp_modem_recov_helper.h"
#include "esp_modem_dce.h"
#include "esp_modem_dce_common_commands.h"
#include "usbh_modem_board.h"

static const char *TAG = "modem_board";
ESP_EVENT_DEFINE_BASE(MODEM_BOARD_EVENT);

#define MODEM_CHECK_GOTO(a, str, goto_tag, ...)                                       \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);     \
            goto goto_tag;                                                            \
        }

#define MODEM_CHECK(a, str, return_tag, ...)                                            \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);     \
            return return_tag;                                                            \
        }                                                                             \

#define MODEM_POWER_GPIO                    CONFIG_MODEM_POWER_GPIO
#define MODEM_RESET_GPIO                    CONFIG_MODEM_RESET_GPIO
#define MODEM_POWER_GPIO_INACTIVE_LEVEL     CONFIG_MODEM_POWER_GPIO_INACTIVE_LEVEL
#define MODEM_RESET_GPIO_INACTIVE_LEVEL     CONFIG_MODEM_RESET_GPIO_INACTIVE_LEVEL
#define MODEM_POWER_GPIO_ACTIVE_MS          500
#define MODEM_POWER_GPIO_INACTIVE_MS        8000
#define MODEM_RESET_GPIO_ACTIVE_MS          1000
#define MODEM_RESET_GPIO_INACTIVE_MS        1000
#define MODEM_NET_RECONNECT_DELAY_S         5
#define MODEM_SIM_PIN_PWD                   CONFIG_MODEM_SIM_PIN_PWD

#define MODEM_POLL_SIM_SIGNAL_QUALITY_MS    5000
#define MODEM_POLL_GNSS_GPS_MS              10000

/*AGNSS辅助定位使能*/
#define MODEM_CONFIG_AGNSS_ENABLE

/***********************AGNSS服务器地址配置*************************/

// 华大AGPS服务器地址
#define AGNSS_ALLYSTAR_SERV_CMD "AT+GTAGPSSERV=\"http://uagnss.allystar.com/ephemeris/HD_GPS_BDS_QZS.hdb?compid=delanmh&token=t9yzV9yAZgWWnjQK\",80\r\n"

#if 0   // 下述已停用

// 联通内网 SUPL 服务器
#define AGNSS_UNICOM_SERV_CMD   "AT+GTAGPSSERV=\"10.1.101.63\",7275\r\n"

// 移动公网 SUPL 服务器
#define AGNSS_MOBILE_SERV_CMD   "AT+GTAGPSSERV=\"221.176.0.55\",7275\r\n"

// SiRF（高通旗下 GPS 方案商）国内服务器
#define AGNSS_SIRF_CN_SERV_CMD  "AT+GTAGPSSERV=\"suplcn.sirf.com\",7275\r\n"

// SiRF 国际服务器
#define AGNSS_SIRF_SERV_CMD     "AT+GTAGPSSERV=\"sls1.sirf.com\",7275\r\n"

// 谷歌 SUPL 服务器
#define AGNSS_GOOGLE_SERV_CMD   "AT+GTAGPSSERV=\"supl.google.com\",7275\r\n"

// 诺基亚 SUPL 服务器
#define AGNSS_NOKIA_SERV_CMD    "AT+GTAGPSSERV=\"supl.nokia.com\",7275\r\n"

// CalyxOS 全球通用国际SUPL服务器
#define AGNSS_CALYXOS_SERVID_CMD    "AT+GTAGPSSERV=\"supl.calyxos.org\",7275\r\n"

#endif

// 中国移动官方 SUPL 服务器
#define AGNSS_CMCC_OFFICIAL_SERVID_CMD    "AT+GTAGPSSERV=\"supl.3gpp.org.cn\",7276\r\n"

// 千寻位置 SUPL 服务器
#define AGNSS_QIANXUN_SERVID_CMD    "AT+GTAGPSSERV=\"supl.qxwz.com\",7276\r\n"

// GrapheneOS 国际公开SUPL服务器
#define AGNSS_GRAPHENEOS_SERVID_CMD    "AT+GTAGPSSERV=\"supl.grapheneos.org\",7275\r\n"

// T-Mobile 美国专属SUPL服务器
#define AGNSS_TMOBILE_US_SERVID_CMD    "AT+GTAGPSSERV=\"lbs.geo.t-mobile.com\",7275\r\n"

// Orange 欧洲法国专属SUPL服务器
#define AGNSS_ORANGE_EU_SERVID_CMD    "AT+GTAGPSSERV=\"agpss.orange.fr\",7275\r\n"

// Telstra 澳洲专属SUPL服务器
#define AGNSS_TELSTRA_AU_SERVID_CMD    "AT+GTAGPSSERV=\"agps.supl.telstra.com\",7275\r\n"

/***********************不同4G模块初始化配置命令表*************************/

/*4G模块名称（AT指令获取名称）*/
#define MODEM_NAME_NL668AM10                "NL668-AM-10"
#define MODEM_NAME_NL668CN                  "NL668-CN"
#define MODEM_NAME_NL668EAU                 "NL668-EAU"
#define MODEM_NAME_L610EU                   "L610-EU"
#define MODEM_NAME_EC25                     "EC25"

typedef struct {
    const char *cmd;      /*!< AT command string */
    const char *desc;     /*!< Description for logging */
} modem_cmd_item_t;

/*NL668-AM-10 配置指令表*/
static const modem_cmd_item_t nl668_am_10_cmd_list[] = {
    { "AT+MSMPD=1\r",                "Enable SIM card hot-plug" },
    { "AT+GTSET=\"SIMPHASE\",1\r",   "Enable SIM card high-level detection" },
};

/*NL668-CN（含GPS）配置指令表*/
static const modem_cmd_item_t nl668_cn_cmd_list[] = {
    { "AT+MSMPD=1\r",                "Enable SIM card hot-plug" },
    { "AT+GTSET=\"SIMPHASE\",1\r",   "Enable SIM card high-level detection" },
#ifdef MODEM_CONFIG_AGNSS_ENABLE
    { AGNSS_QIANXUN_SERVID_CMD,      "Configure AGNSS server" },
    { "AT+GTGPSEPO=1\r\n",           "Enable AGNSS" },
#endif
    { "AT+GTGPSPOWER=1\r\n",         "Enable GPS power" },
};

/*NL668-EAU（含GPS）配置指令表*/
static const modem_cmd_item_t nl668_eau_cmd_list[] = {
    { "AT+MSMPD=1\r",                "Enable SIM card hot-plug" },
    { "AT+GTSET=\"SIMPHASE\",1\r",   "Enable SIM card high-level detection" },
#ifdef MODEM_CONFIG_AGNSS_ENABLE
    { AGNSS_GRAPHENEOS_SERVID_CMD,   "Configure AGNSS server" },
    { "AT+GTGPSEPO=1\r\n",           "Enable AGNSS" },
#endif
    { "AT+GTGPSPOWER=1\r\n",         "Enable GPS power" },
};

/*L610-EU（含GPS） 配置指令表*/
static const modem_cmd_item_t L610_EU_cmd_list[] = {
    { "AT+MSMPD=1\r",                "Enable SIM card hot-plug" },
    { "AT+SIMPHASE=1\r",             "Enable SIM card high-level detection" },
#ifdef MODEM_CONFIG_AGNSS_ENABLE
    { AGNSS_ALLYSTAR_SERV_CMD,       "Configure AGNSS server" },
    { "AT+GTGPSEPO=1\r\n",           "Enable AGNSS" },
#endif
    { "AT+GTGPSPOWER=1\r\n",         "Enable GPS power" },
};

/*EC25（含GPS） 配置指令表*/
static const modem_cmd_item_t EC25_cmd_list[] = {
    { "AT+QSIMDET=1,1\r",            "Enable SIM card hot-plug ( high-level inserted)"}, 
    { "AT+QGPS=1\r",                 "Enable GNSS(Mode1)" },
    { "AT+QGPSCFG=\"nmeasrc\",1\r",  "Enable NEMA" },
};

/*广和通GPS数据指令表*/
static const modem_cmd_item_t Fibocom_GPS_list[] = {
    { "AT+GTGPS?\r\n",               "Get GPS-NEMA-Data"}, 
};

/*移远GPS数据指令表("GGA","RMC","GSV","GSA","VTG","GNS")*/
static const modem_cmd_item_t Quectel_GPS_list[] = {
    { "AT+QGPSGNMEA=\"GGA\"\r\n",    "Get GPS-NEMA-Data(GGA)"}, 
    { "AT+QGPSGNMEA=\"RMC\"\r\n",    "Get GPS-NEMA-Data(RMC)"}, 
//    { "AT+QGPSGNMEA=\"GSV\"\r\n",    "Get GPS-NEMA-Data(GSV)"}, // 暂不支持
//    { "AT+QGPSGNMEA=\"GSA\"\r\n",    "Get GPS-NEMA-Data(GSA)"}, // 暂不使用
//    { "AT+QGPSGNMEA=\"VTG\"\r\n",    "Get GPS-NEMA-Data(VTG)"}, // 暂不使用
//    { "AT+QGPSGNMEA=\"GNS\"\r\n",    "Get GPS-NEMA-Data(GNS)"}, // 暂不支持
};

typedef struct {
    const char *name_pattern;           // 模块名称匹配模式
    const modem_cmd_item_t *cmd_list;  // INIT 命令列表
    size_t cmd_list_size;              // INIT 命令列表大小
    const modem_cmd_item_t *gps_list;  // GPS 命令列表
    size_t gps_list_size;              // GPS 命令列表大小
} modem_cmd_map_t;

// 模块命令映射表
static const modem_cmd_map_t modem_cmd_map[] = {
    { MODEM_NAME_NL668CN,   nl668_cn_cmd_list,   sizeof(nl668_cn_cmd_list)   / sizeof(nl668_cn_cmd_list[0]),   Fibocom_GPS_list,    sizeof(Fibocom_GPS_list)    / sizeof(Fibocom_GPS_list[0])    },
    { MODEM_NAME_L610EU,    L610_EU_cmd_list,    sizeof(L610_EU_cmd_list)    / sizeof(L610_EU_cmd_list[0]),    Fibocom_GPS_list,    sizeof(Fibocom_GPS_list)    / sizeof(Fibocom_GPS_list[0])    },
    { MODEM_NAME_NL668AM10, nl668_am_10_cmd_list,sizeof(nl668_am_10_cmd_list)/ sizeof(nl668_am_10_cmd_list[0]),NULL,                0                                                            },
    { MODEM_NAME_EC25,      EC25_cmd_list,       sizeof(EC25_cmd_list)       / sizeof(EC25_cmd_list[0]),       Quectel_GPS_list,   sizeof(Quectel_GPS_list)   / sizeof(Quectel_GPS_list[0])      },
    { MODEM_NAME_NL668EAU,  nl668_eau_cmd_list,  sizeof(nl668_eau_cmd_list)  / sizeof(nl668_eau_cmd_list[0]),  Fibocom_GPS_list,    sizeof(Fibocom_GPS_list)    / sizeof(Fibocom_GPS_list[0])    },
};

/***************************4G模块APN列表***************************/

// 运营商枚举
typedef enum {
    OPERATOR_UNKNOWN = 0,  // 未知/兜底
    OPERATOR_CMCC,         // 中国移动
    OPERATOR_CUCC,         // 中国联通
    OPERATOR_CTCC,         // 中国电信
    OPERATOR_CBN           // 中国广电
} OperatorType;

// APN信息结构体（完整拨号参数）
typedef struct {
    const char* apn;       // APN名称
    const char* username;  // 用户名（绝大多数物联卡为空）
    const char* password;  // 密码（绝大多数物联卡为空）
    uint8_t auth_type;     // 认证类型：0-无 1-PAP 2-CHAP 3-自动
    OperatorType op;       // 所属运营商，用于筛选
} ApnInfo;

/**************************
 * 最终APN列表（只读放Flash，节省RAM）
 **************************/
static const ApnInfo ApnList[] = 
{
    /**************** 中国联通（物联优先） ****************/
    {"linksnet",   "", "", 0, OPERATOR_CUCC},  // 联通物联网通用公网APN，物联卡标配
    {"3gnet",      "", "", 0, OPERATOR_CUCC},  // 联通公网通用，2/3/4/5G全兼容，成功率最高
    {"uninet",     "", "", 0, OPERATOR_CUCC},  // 联通老款公网APN，部分老卡/地区适配

    /**************** 中国移动（物联优先） ****************/
    {"cm.iot1",    "", "", 0, OPERATOR_CMCC},  // 移动物联网通用公网APN，大众物联卡最常用
    {"m2m.iot1",   "", "", 0, OPERATOR_CMCC},  // 移动M2M专网APN，行业定向物联卡
    {"cmi.m2m",    "", "", 0, OPERATOR_CMCC},  // 中移物联网专网APN，行业定制卡
    {"cmnet",      "", "", 0, OPERATOR_CMCC},  // 移动公网通用，全网络兼容，普通手机卡/流量卡标配

    /**************** 中国电信（物联优先） ****************/
    {"ctiot.m2m",  "", "", 0, OPERATOR_CTCC},  // 电信物联网通用公网APN，物联卡标配
    {"ctnet",      "", "", 0, OPERATOR_CTCC},  // 电信公网通用，普通手机卡/流量卡标配

    /**************** 中国广电（可选） ****************/
    {"cbnet",      "", "", 0, OPERATOR_CBN},   // 广电5G公网通用，广电SIM卡适配

    /********** 终极兜底：空APN，依赖核心网自动分配 **********/
    {"",           "", "", 0, OPERATOR_UNKNOWN}, // 空APN，多数模组会触发核心网默认分配
};

static const uint8_t Apn_Total_Num = (sizeof(ApnList) / sizeof(ApnList[0]));

/*当前使用的APN序号*/
static uint8_t modem_apn_index = 0;

/*******************************************************************/

/* user event */
static const int MODEM_DESTROY_BIT                = BIT0;    /* destroy modem daemon task, trigger by user, clear by daemon task */
static const int MODEM_DESTROY_DONE_BIT           = BIT1;    /* modem daemon task destroy done, trigger by daemon task, clear by user */
static const int PPP_NET_MODE_ON_BIT              = BIT2;    /* dte usb reconnect event, trigger by user, clear by daemon task */
static const int PPP_NET_MODE_OFF_BIT             = BIT3;    /* dte usb disconnect event, trigger by user, clear by hardware */
static const int PPP_NET_AUTO_SUSPEND_USER_BIT    = BIT4;    /* suspend ppp net auto reconnect, trigger by user, clear by user */
/* net event */
static const int PPP_NET_CONNECT_BIT              = BIT5;    /* ppp net got ip, trigger by lwip, clear by lwip or daemon task */
static const int PPP_NET_DISCONNECT_BIT           = BIT6;    /* ppp net loss ip, trigger by lwip, clear by lwip */
/* usb event */
static const int DTE_USB_DISCONNECT_BIT           = BIT7;    /* dte usb disconnect event, trigger by hardware, clear by daemon task */
static const int DTE_USB_RECONNECT_BIT            = BIT8;    /* dte usb reconnect event, trigger by hardware or user, clear by daemon task */
/* daemon task internal event bit */
static const int PPP_NET_RECONNECTING_BIT         = BIT9;    /* ppp net reconnecting, trigger by daemon task, clear by daemon task */

static esp_modem_dce_t *s_dce = NULL;
static EventGroupHandle_t s_modem_evt_hdl = NULL;
static bool modem_init_config = true;
EXT_RAM_BSS_ATTR static modem_info_t modem_data = {0};
EXT_RAM_BSS_ATTR static char modem_custom_apn[60] = {0};

typedef struct {
    esp_modem_dce_t parent;
    esp_modem_recov_gpio_t *power_pin;
    esp_modem_recov_gpio_t *reset_pin;
    esp_err_t (*reset)(esp_modem_dce_t *dce);
    esp_err_t (*power_up)(esp_modem_dce_t *dce);
    esp_err_t (*power_down)(esp_modem_dce_t *dce);
    esp_modem_recov_resend_t *re_sync;
    esp_modem_recov_resend_t *re_store_profile;
} modem_board_t;

/**
 * @brief 4G modem working stage enumeration
 */
typedef enum {
    STAGE_DTE_LOSS = -1,        /*!< DTE lost, restoring to command state */
    STAGE_WAITING = 0,          /*!< In command state, waiting for PPP on event */
    STAGE_SYNC = 1,             /*!< Trying to synchronize with modem using AT commands */
    STAGE_STOP_PPP = 2,         /*!< Stopping PPP, restoring to command state */
    STAGE_DEV_INIT = 3,         /*!< Initializing modem device */
    STAGE_DEV_ID_GET = 4,       /*!< Getting modem device ID */
    STAGE_CHECK_SIM = 5,        /*!< Checking SIM card state */
    STAGE_CHECK_CARD = 6,       /*!< Checking SIM card presence */
    STAGE_CHECK_SIGNAL = 7,     /*!< Checking network signal strength */
    STAGE_CHECK_REGIST = 8,     /*!< Checking if registered to network operator */
    STAGE_START_PPP = 9,        /*!< Starting PPP dial-up */
    STAGE_WAIT_IP = 10,         /*!< Waiting for IP address */
    STAGE_RUNNING = 11          /*!< Network is up and running */
} _modem_stage_t;

static _modem_stage_t modem_stage = STAGE_WAITING;

#define MODEM_STAGE_CODE2STR(code) {code, #code}

typedef struct {
    int code;
    const char *msg;
} _modem_stage_msg_t;

static const _modem_stage_msg_t modem_stage_msg_table[] = {
    MODEM_STAGE_CODE2STR(STAGE_DTE_LOSS),
    MODEM_STAGE_CODE2STR(STAGE_WAITING),
    MODEM_STAGE_CODE2STR(STAGE_SYNC),
    {STAGE_STOP_PPP, "STAGE_SYNC/STOP_PPP"},
    MODEM_STAGE_CODE2STR(STAGE_DEV_INIT),
    MODEM_STAGE_CODE2STR(STAGE_DEV_ID_GET),
    MODEM_STAGE_CODE2STR(STAGE_CHECK_SIM),
    MODEM_STAGE_CODE2STR(STAGE_CHECK_CARD),    
    MODEM_STAGE_CODE2STR(STAGE_CHECK_SIGNAL),
    MODEM_STAGE_CODE2STR(STAGE_CHECK_REGIST),
    MODEM_STAGE_CODE2STR(STAGE_START_PPP),
    MODEM_STAGE_CODE2STR(STAGE_WAIT_IP),
    MODEM_STAGE_CODE2STR(STAGE_RUNNING)
};

const char *MODEM_STAGE_STR(int code)
{
    size_t i;
    for (i = 0; i < sizeof(modem_stage_msg_table) / sizeof(modem_stage_msg_table[0]); ++i) {
        if (modem_stage_msg_table[i].code == code) {
            return modem_stage_msg_table[i].msg;
        }
    }
    return "unknown";
}

static esp_err_t modem_board_handle_powerup(esp_modem_dce_t *dce, const char *line)
{
    if (strstr(line, "PB DONE")) {
        ESP_LOGI(TAG, "Board ready after hard reset/power-cycle");
    } else {
        ESP_LOGI(TAG, "xxxxxxxxxxxxxx Board ready after hard reset/power-cycle");
    }
    return ESP_OK;
}

static esp_err_t modem_board_reset(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_reset!");
    dce->handle_line = modem_board_handle_powerup;
    if (board->reset_pin) {
        board->reset_pin->pulse(board->reset_pin);
    }
    return ESP_OK;
}

static esp_err_t modem_board_reset_special(esp_modem_dce_t *dce, int active_width_ms, int inactive_width_ms)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_reset_special!");
    dce->handle_line = modem_board_handle_powerup;
    if (board->reset_pin) {
        board->reset_pin->pulse_special(board->reset_pin, active_width_ms, inactive_width_ms);
    }
    return ESP_OK;
}

static esp_err_t modem_board_at_reset(esp_modem_dce_t *dce)
{
#if 1
    return esp_modem_dce_reset(dce, NULL, NULL);
#else
    char buf[64] = {0};
    return modem_board_send_at_cmd("AT+RESET\r", buf, sizeof(buf));
#endif
}

esp_err_t modem_board_force_reset(void)
{
    ESP_LOGI(TAG, "Force reset modem board....");
    gpio_config_t io_config = {
        .pin_bit_mask = BIT64(MODEM_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io_config);
    // gpio default to inactive state
    gpio_set_level(MODEM_RESET_GPIO, MODEM_RESET_GPIO_INACTIVE_LEVEL);
    // gpio active to reset modem
    gpio_set_level(MODEM_RESET_GPIO, !MODEM_RESET_GPIO_INACTIVE_LEVEL);
    ESP_LOGI(TAG, "Resetting modem using io=%d, level=%d", MODEM_RESET_GPIO, !MODEM_RESET_GPIO_INACTIVE_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(MODEM_RESET_GPIO_ACTIVE_MS));
    gpio_set_level(MODEM_RESET_GPIO, MODEM_RESET_GPIO_INACTIVE_LEVEL);
    // waiting for modem re-init ready
    ESP_LOGI(TAG, "Waiting for modem initialize ready");
    vTaskDelay(pdMS_TO_TICKS(MODEM_RESET_GPIO_INACTIVE_MS));
    return ESP_OK;
}

static esp_err_t modem_board_power_up(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_power_up!");
    dce->handle_line = modem_board_handle_powerup;
    if (board->power_pin) {
        board->power_pin->pulse(board->power_pin);
    }
    return ESP_OK;
}

static esp_err_t modem_board_power_down(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_power_down!");
    dce->handle_line = modem_board_handle_powerup;
    if (board->power_pin) {
        board->power_pin->pulse(board->power_pin);
    }
    return ESP_OK;
}

/* Functions in recover loop, will run many times to ensure work */
static esp_err_t my_recov(esp_modem_recov_resend_t *retry_cmd, esp_err_t err, int timeouts, int errors)
{
    esp_modem_dce_t *dce = retry_cmd->dce;
    ESP_LOGI(TAG, "Current timeouts: %d and errors: %d", timeouts, errors);
    if (err != ESP_OK) {
        if (timeouts + errors < 2) {
            ESP_LOGW(TAG, "Try to exit ppp mode........");
            dce->mode = ESP_MODEM_TRANSITION_MODE;
            dce->set_command_mode(dce, NULL, NULL);
        } else if (timeouts + errors <= 3) {
            if ( MODEM_RESET_GPIO ) {
                // try to reset with GPIO if resend didn't help
                ESP_LOGW(TAG, "Reset modem through reset pin........");
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART, NULL, 0, 0);
                modem_board_t *board = __containerof(dce, modem_board_t, parent);
                board->reset(dce);
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART_DONE, NULL, 0, 0);
            }
        } else {
            if ( MODEM_POWER_GPIO ) {
                // otherwise power-cycle the board
                ESP_LOGW(TAG, "Reset modem through power pin........");
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART, NULL, 0, 0);
                modem_board_t *board = __containerof(dce, modem_board_t, parent);
                board->power_down(dce);
                board->power_up(dce);
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART_DONE, NULL, 0, 0);
            }
        }
    }
    return ESP_OK;
}

static DEFINE_RETRY_CMD(re_sync_fn, re_sync, modem_board_t)

static esp_err_t modem_board_start_up(esp_modem_dce_t *dce)
{
    MODEM_CHECK_GOTO(re_sync_fn(dce, NULL, NULL) == ESP_OK, "sending sync failed", err);
    MODEM_CHECK_GOTO(dce->set_echo(dce, (void *)false, NULL) == ESP_OK, "set_echo failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;

}

static esp_err_t modem_board_dce_deinit(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    if (board->power_pin) {
        board->power_pin->destroy(board->power_pin);
    }
    if (board->reset_pin) {
        board->reset_pin->destroy(board->reset_pin);
    }
    esp_err_t err = esp_modem_command_list_deinit(&board->parent);
    if (err == ESP_OK) {
        free(dce);
    }
    s_dce = NULL;
    return err;
}

static esp_modem_dce_t *modem_board_create(esp_modem_dce_config_t *config)
{
    modem_board_t *board = calloc(1, sizeof(modem_board_t));
    MODEM_CHECK_GOTO(board, "failed to allocate modem_board object", err);
    MODEM_CHECK_GOTO(esp_modem_dce_init(&board->parent, config) == ESP_OK, "Failed to init modem_dce", err);
    // /* power on sequence (typical values for modem Ton=500ms, Ton-status=8s) */
    if (MODEM_POWER_GPIO) board->power_pin = esp_modem_recov_gpio_new(MODEM_POWER_GPIO, MODEM_POWER_GPIO_INACTIVE_LEVEL,
                                                                          MODEM_POWER_GPIO_ACTIVE_MS, MODEM_POWER_GPIO_INACTIVE_MS);
    // /* reset sequence (typical values for modem reser, Treset=1000ms, wait 1s after reset */
    if (MODEM_RESET_GPIO) board->reset_pin = esp_modem_recov_gpio_new(MODEM_RESET_GPIO, MODEM_RESET_GPIO_INACTIVE_LEVEL,
                                                                          MODEM_RESET_GPIO_ACTIVE_MS, MODEM_RESET_GPIO_INACTIVE_MS);
    board->reset = modem_board_reset;
    board->power_up = modem_board_power_up;
    board->power_down = modem_board_power_down;
    board->re_sync = esp_modem_recov_resend_new(&board->parent, board->parent.sync, my_recov, 5, 5);
    //overwrite default function
    board->parent.deinit = modem_board_dce_deinit;
    board->parent.start_up = modem_board_start_up;

    return &board->parent;
err:
    free(board);
    return NULL;
}

static void on_modem_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT) {
        ESP_LOGI(TAG, "IP event! %"PRIi32"", event_id);
        if (event_id == IP_EVENT_PPP_GOT_IP) {
            esp_netif_dns_info_t dns_info;
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            modem_data.ppp_ip_info = event->ip_info;
            esp_netif_t *netif = event->esp_netif;
            ESP_LOGI(TAG, "Modem Connected to PPP Server");
            ESP_LOGI(TAG, "%s ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR, esp_netif_get_desc(netif),
                     IP2STR(&modem_data.ppp_ip_info.ip),
                     IP2STR(&modem_data.ppp_ip_info.netmask),
                     IP2STR(&modem_data.ppp_ip_info.gw));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
            modem_data.ppp_dns_ip_main = dns_info.ip;
            ESP_LOGI(TAG, "Main DNS: " IPSTR, IP2STR(&modem_data.ppp_dns_ip_main.u_addr.ip4));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            modem_data.ppp_dns_ip_backup = dns_info.ip;
            ESP_LOGI(TAG, "Backup DNS: " IPSTR, IP2STR(&modem_data.ppp_dns_ip_backup.u_addr.ip4));
            if (s_modem_evt_hdl) {
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_DISCONNECT_BIT);
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_CONNECT_BIT);
            }
        } else if (event_id == IP_EVENT_PPP_LOST_IP) {
            ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
            if (s_modem_evt_hdl) {
                memset(&modem_data.ppp_ip_info, 0, sizeof(modem_data.ppp_ip_info));
                memset(&modem_data.ppp_dns_ip_main, 0, sizeof(modem_data.ppp_dns_ip_main));
                memset(&modem_data.ppp_dns_ip_backup, 0, sizeof(modem_data.ppp_dns_ip_backup));
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_CONNECT_BIT);
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_DISCONNECT_BIT);
            }
        } else if (event_id == IP_EVENT_GOT_IP6) {
            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        }
    } else if (event_base == ESP_MODEM_EVENT) {
        switch (event_id) {
        default:
            ESP_LOGW(TAG, "Modem event! %"PRIi32"", event_id);
            break;
        }
    }
}

static void _usb_dte_conn_callback(usbh_cdc_handle_t cdc_handle, void *arg)
{
    /* first conn callback, dce*/
    xEventGroupSetBits(s_modem_evt_hdl, DTE_USB_RECONNECT_BIT);
    xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
    esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_CONN, NULL, 0, 0);
}

static void _usb_dte_disconn_callback(usbh_cdc_handle_t cdc_handle, void *arg)
{
    /* withdraw reconnect if disconn happened first */
    xEventGroupSetBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
    xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_RECONNECT_BIT);
    esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_DISCONN, NULL, 0, 0);
}

static inline const char* extract_colon_string(const char *src, char *dst, size_t dst_size)
{
    const char *p = strchr(src, ':');
    if (!p) p = src;
    if (*p == ':') p++; // 跳过引号
    while (*p == ' ' || *p == '\t') p++; // 跳过空白
    if (*p == '\"') p++; // 跳过引号
    size_t i = 0;
    while (*p && *p != '\"' && *p != '\r' && *p != '\n' && i < dst_size - 1) {
        dst[i++] = *p++;
    }
    dst[i] = '\0';
    return dst;
}

// 根据模块名称查找对应的命令列表
static void modem_board_get_cmd_list(const char *module_name, const modem_cmd_item_t **cmd_list, size_t *cmd_list_size)
{
    *cmd_list = NULL;
    *cmd_list_size = 0;

    if (module_name == NULL) {
        return;
    }

    for (size_t i = 0; i < sizeof(modem_cmd_map) / sizeof(modem_cmd_map[0]); ++i) {
        if (strstr(module_name, modem_cmd_map[i].name_pattern) != NULL) {
            *cmd_list = modem_cmd_map[i].cmd_list;
            *cmd_list_size = modem_cmd_map[i].cmd_list_size;
            ESP_LOGI(TAG, "Matched modem cmd list: %s", modem_cmd_map[i].name_pattern);
            return;
        }
    }

    ESP_LOGW(TAG, "Unknown modem module(%s)", module_name);
}

/**
 * @brief  从IMSI字符串解析运营商
 * @param  imsi_str  原始IMSI字符串（允许带\r\n、空格等前后缀）
 * @return OperatorType 运营商枚举
 */
static uint8_t get_operator_by_imsi(const char *imsi_str)
{
    if (imsi_str == NULL) {
        return OPERATOR_UNKNOWN;
    }

    // 跳过前导非数字字符，兼容模组回显/换行
    const char *p = imsi_str;
    while (*p && (*p < '0' || *p > '9')) p++;

    // 至少需要5位（3位MCC + 2位MNC）
    if (strlen(p) < 5) {
        return OPERATOR_UNKNOWN;
    }

    // 国家码校验：仅处理国内460号段
    if (p[0] != '4' || p[1] != '6' || p[2] != '0') {
        return OPERATOR_UNKNOWN;
    }

    // 提取MNC（第4~5位，数组下标3、4）
    char mnc[3] = {p[3], p[4], '\0'};

    // 匹配中国移动
    if (!strcmp(mnc,"00") || !strcmp(mnc,"02") || !strcmp(mnc,"04") ||
        !strcmp(mnc,"07") || !strcmp(mnc,"08")) {
        return OPERATOR_CMCC;
    }
    // 匹配中国联通
    else if (!strcmp(mnc,"01") || !strcmp(mnc,"06") || !strcmp(mnc,"09")) {
        return OPERATOR_CUCC;
    }
    // 匹配中国电信
    else if (!strcmp(mnc,"03") || !strcmp(mnc,"05") || !strcmp(mnc,"11")) {
        return OPERATOR_CTCC;
    }

    return OPERATOR_UNKNOWN;
}

/**
 * @brief  查找下一个匹配运营商的APN下标
 * @param  start_idx  起始查找下标（从当前下标的下一个开始找）
 * @param  op         目标运营商，UNKNOWN表示匹配全部
 * @return 找到返回APN下标，找不到返回0xFF
 */
static uint8_t find_next_apn(uint8_t start_idx, OperatorType op)
{
    for (uint8_t i = start_idx; i < Apn_Total_Num; i++) {
        if (op == OPERATOR_UNKNOWN || ApnList[i].op == op) {
            return i;
        }
    }
    return 0xFF; // 无匹配项
}

static bool _check_dev_config()
{   
    // 避免重复初始化
    if ( !modem_init_config ) return true;

    char buf[64] = {0};
    const modem_cmd_item_t *cmd_list = NULL;
    size_t cmd_list_size = 0;

    // Set Echo mode    
    if (s_dce && esp_modem_dce_set_echo(s_dce, false, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set Echo mode OFF.");
        return false;
    }

    // Get Module Name
    memset(buf, 0, sizeof(buf));
    if (modem_board_get_module_name(buf, sizeof(buf)) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get module name.");
        return false;
    }
    if (strlen(buf) == 0) {
        ESP_LOGW(TAG, "Module name is empty.");
        return false;
    }
    if ((!extract_colon_string(buf, modem_data.module_name, sizeof(modem_data.module_name)))  
        || (2 >= strlen(modem_data.module_name))){
        ESP_LOGW(TAG, "Module name response format unexpected: %s", buf);
        return false;
    }
    ESP_LOGI(TAG, "Modem module name: %s", modem_data.module_name);

    // 根据 module_name 选择不同的命令列表
    modem_board_get_cmd_list(modem_data.module_name, &cmd_list, &cmd_list_size);     
    
    for (size_t i = 0; i < cmd_list_size; ++i) {
        memset(buf, 0, sizeof(buf));
        if (modem_board_send_at_cmd(cmd_list[i].cmd, buf, sizeof(buf)) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to %s. Command: %s Response: %s", cmd_list[i].desc, cmd_list[i].cmd, buf);
            return false;
        }
        ESP_LOGI(TAG, "%s succeeded.", cmd_list[i].desc);
    }

    // 清空GPS数据
    memset(&modem_data.gps_info, 0, sizeof(modem_data.gps_info));

    // 更新初始化标志
    modem_init_config = false;
    
    return true;
}

static bool _check_imei_number()
{
    char buf[64] = {0};     
    if (modem_board_get_imei_number(buf, sizeof(buf)) == ESP_OK) {
        if (strlen(buf) > 0) {
            if ((extract_colon_string(buf, modem_data.IMEI, sizeof(modem_data.IMEI)))  
                && (2 < strlen(modem_data.IMEI))){
                ESP_LOGI(TAG, "Modem IMEI: %s", modem_data.IMEI);
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DEV_ID_GET, NULL, 0, 0);
                return true;
            } else {
                ESP_LOGW(TAG, "IMEI response format unexpected: %s", buf);
                return false;
            }
        } else {
            ESP_LOGW(TAG, "IMEI is empty, modem may not be registered on the network.");
            return false;
        }
    }
    ESP_LOGW(TAG, "Failed to get IMEI, modem not registered on the network.");
    return false;
}

static bool _check_sim_card()
{
    int if_ready = false;
    if (modem_board_get_sim_cart_state(&if_ready) == ESP_OK) {
        if (if_ready == true) {
            ESP_LOGI(TAG, "SIM Card Ready");
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_SIMCARD_CONN, NULL, 0, 0);
            return true;
        } else {
            ESP_LOGW(TAG, "No SIM Card or PIN Wrong!");
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_SIMCARD_DISCONN, NULL, 0, 0);
            return false;
        }
    }
    ESP_LOGW(TAG, "Get SIM card state failed");
    return false;
}

static bool _check_imsi_iccid_number(void)
{
    char buf[64] = {0};

    // Check IMSI
    memset(buf, 0, sizeof(buf));
    if (modem_board_get_imsi_number(buf, sizeof(buf)) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get IMSI, modem may not be registered on the network.");
        return false;
    }
    if (strlen(buf) == 0) {
        ESP_LOGW(TAG, "IMSI is empty, modem may not be registered on the network.");
        return false;
    }
    if ((!extract_colon_string(buf, modem_data.IMSI, sizeof(modem_data.IMSI)))  
        || (2 >= strlen(modem_data.IMSI))){
        ESP_LOGW(TAG, "IMSI response format unexpected: %s", buf);
        return false;
    }

    uint8_t operator_index = get_operator_by_imsi(modem_data.IMSI);    
    ESP_LOGI(TAG, "Modem IMSI: %s (operator: %d)", modem_data.IMSI, operator_index);

    // Check ICCID
    memset(buf, 0, sizeof(buf));
    if (strstr(modem_data.module_name, MODEM_NAME_EC25)) {
        if (modem_board_send_at_cmd("AT+QCCID\r", buf, sizeof(buf)) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get ICCID, modem may not be registered on the network.");
            return false;
        }
    } else {
        if (modem_board_get_iccid_number(buf, sizeof(buf)) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get ICCID, modem may not be registered on the network.");
            return false;
        }
    }

    if (strlen(buf) == 0) {
        ESP_LOGW(TAG, "ICCID is empty, modem may not be registered on the network.");
        return false;
    }
    if ((!extract_colon_string(buf, modem_data.ICCID, sizeof(modem_data.ICCID)))  
        || (2 >= strlen(modem_data.ICCID))){
        ESP_LOGW(TAG, "ICCID response format unexpected: %s", buf);
        return false;
    }
    ESP_LOGI(TAG, "Modem ICCID: %s", modem_data.ICCID);

    // 未设置自定义apn，轮询列表
    static uint8_t curr_apn_index = 0xFF;
    if (0 == strlen(modem_custom_apn)) {
        uint8_t index = find_next_apn(modem_apn_index, operator_index);
        if ( 0xFF != index ) {
            modem_apn_index = index;
            if ( curr_apn_index != index ) {
                curr_apn_index = index;
                if (esp_modem_dce_set_apn(s_dce, ApnList[index].apn) == ESP_OK) {
                    ESP_LOGI(TAG, "APN auto change success: %s", ApnList[index].apn);
                } else {
                    ESP_LOGI(TAG, "APN no change: %s", ApnList[index].apn);
                }
            }
        } else {
            ESP_LOGE(TAG, "APN auto find failed");
            curr_apn_index = 0xFF;
            modem_apn_index = 0;
            return false;
        }
    }

    return true;
}

static bool _check_signal_quality()
{
    if (modem_board_get_signal_quality(&modem_data.rssi, &modem_data.ber) == ESP_OK) {
        if (modem_data.rssi != 99 && modem_data.rssi > 5) {
            ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", modem_data.rssi, modem_data.ber);
            return true;
        } else {
            ESP_LOGW(TAG, "Low signal quality: rssi=%d, ber=%d", modem_data.rssi, modem_data.ber);
            return false;
        }
    }
    ESP_LOGW(TAG, "Get signal quality failed");
    return false;
}

static bool _check_network_registration()
{
    char *operater_name = modem_data.operater_name;
    uint16_t buf_len = sizeof(modem_data.operater_name);
    memset(operater_name, 0, buf_len);
    
    if (modem_board_get_operator_state(operater_name, buf_len) == ESP_OK) {
        if (strlen(operater_name) > 0) {
            ESP_LOGI(TAG, "Network registered, Operator: %s", operater_name);
            return true;
        } else {
            // no operator name, but registered?
            ESP_LOGW(TAG, "No operator information, Network not registered ?");
            return false;
        }
    }
    ESP_LOGW(TAG, "Get operator failed, Network not registered");
    return false;
}

static bool _ppp_network_start(esp_modem_dte_t *dte)
{
    dte->dce->mode = ESP_MODEM_TRANSITION_MODE;
    if (esp_modem_start_ppp(dte) == ESP_OK) {
        return true;
    }
    return false;
}

static bool _ppp_network_stop(esp_modem_dte_t *dte)
{
    if (esp_modem_stop_ppp(dte) == ESP_OK) {
        return true;
    }
    return false;
}

static void _modem_daemon_task(void *param)
{
    modem_config_t *config = (modem_config_t *)param;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if ((config->flags & MODEM_FLAGS_INIT_NOT_FORCE_RESET) == 0) {
        if (MODEM_RESET_GPIO) modem_board_force_reset();
    }
    // init the USB DTE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.rx_buffer_size = config->rx_buffer_size; //rx ringbuffer for usb transfer
    dte_config.tx_buffer_size = config->tx_buffer_size; //tx ringbuffer for usb transfer
    dte_config.line_buffer_size = config->line_buffer_size;
    dte_config.event_task_stack_size = config->event_task_stack_size; //task to handle usb rx data
    dte_config.event_task_priority = config->event_task_priority; //task to handle usb rx data
    dte_config.conn_callback = _usb_dte_conn_callback;
    dte_config.disconn_callback = _usb_dte_disconn_callback;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(ApnList[0].apn);
    if (strlen(modem_custom_apn)) dce_config.pdp_context.apn =  modem_custom_apn;
    esp_netif_config_t ppp_netif_config = ESP_NETIF_DEFAULT_PPP();

    // Initialize esp-modem units, DTE, DCE, ppp-netif
    esp_modem_dte_t *dte = esp_modem_dte_new(&dte_config);
    assert(dte != NULL);
    esp_modem_dce_t *dce = modem_board_create(&dce_config);
    assert(dce != NULL);
    esp_netif_t *ppp_netif = esp_netif_new(&ppp_netif_config);
    assert(ppp_netif != NULL);
    
    /* attach driver to ppp interface, start DTE handling */
    s_dce = dce;
    ESP_ERROR_CHECK(esp_modem_default_attach(dte, dce, ppp_netif));
    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, on_modem_event, ESP_EVENT_ANY_ID, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_modem_event, NULL));
    if (config->handler) {
        ESP_ERROR_CHECK(esp_event_handler_register(MODEM_BOARD_EVENT, ESP_EVENT_ANY_ID, config->handler, config->handler_arg));
    }

    // Get ppp-netif mac addr
    uint8_t mac[6];
    esp_netif_get_mac(ppp_netif, mac);
    ESP_LOGI(TAG, "MAC(ppp_netif): %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    memcpy(modem_data.ppp_mac, mac, sizeof(mac));
    
    int stage_retry_times = 0;
    int retry_after_ms = 0;
    const int RETRY_TIMEOUT = CONFIG_MODEM_DIAL_RETRY_TIMES;
    modem_stage = STAGE_WAITING;
    while (true) {
        /********************************** handle external event *********************************************************/
        EventBits_t bits = xEventGroupWaitBits(s_modem_evt_hdl, (PPP_NET_MODE_ON_BIT | PPP_NET_MODE_OFF_BIT | DTE_USB_RECONNECT_BIT | DTE_USB_DISCONNECT_BIT | PPP_NET_RECONNECTING_BIT |
                                                                 PPP_NET_DISCONNECT_BIT | MODEM_DESTROY_BIT), pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
        if(!bits && (modem_stage != STAGE_RUNNING)) continue;
        ESP_LOGD(TAG, "Handling bits = %04X, stage = %d, retry = %d ", (unsigned int)bits, modem_stage, stage_retry_times);
        /* daemon task destroy */
        if (bits & MODEM_DESTROY_BIT) {
            break;
        }

        /* user trigger ppp start */
        if (bits & PPP_NET_MODE_ON_BIT) {
            if (modem_stage < STAGE_CHECK_SIM) {
                if( STAGE_WAITING == modem_stage ) {
                    modem_stage = STAGE_SYNC;
                }
            } else {
                /* ppp is started */
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
            }
            stage_retry_times = 0;
        }

        /* user trigger ppp stop */
        if (bits & PPP_NET_MODE_OFF_BIT) {
            if (modem_stage > STAGE_START_PPP) {
                modem_stage = STAGE_DTE_LOSS;
            } else {
                modem_stage = STAGE_WAITING;
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_OFF_BIT);
            }
            stage_retry_times = 0;
        }

        /* usb dis-connect will trigger network pending */
        if (bits & DTE_USB_DISCONNECT_BIT) {
            if (modem_stage > STAGE_START_PPP) {
                modem_stage = STAGE_DTE_LOSS;
            } else {
                modem_stage = STAGE_WAITING;
                xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
            }
            stage_retry_times = 0;
        }

        /* usb re-connect will trigger network re-dial after few seconds */
        if (bits & DTE_USB_RECONNECT_BIT) {
            ESP_LOGI(TAG, "DTE reconnect, reconnecting ...\n");
            /* add delay here to wait modem ready */
            for (size_t i = 0; i < MODEM_NET_RECONNECT_DELAY_S; i++) {
                /* break the reconnect if reconnect withdraw or modem destroy */
                if (((xEventGroupGetBits(s_modem_evt_hdl) & DTE_USB_RECONNECT_BIT) == 0)
                        || (xEventGroupGetBits(s_modem_evt_hdl) & MODEM_DESTROY_BIT)) {
                    break;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ESP_LOGI(TAG, "reconnect after %ds...", MODEM_NET_RECONNECT_DELAY_S - i);
                }
            }

            xEventGroupClearBits(s_modem_evt_hdl, (DTE_USB_RECONNECT_BIT | PPP_NET_CONNECT_BIT));
            if ((xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_AUTO_SUSPEND_USER_BIT) == 0) {
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
            } 
            
            modem_stage = STAGE_SYNC;
            modem_init_config = true;
            modem_apn_index = 0;
            stage_retry_times = 0;
        }
        /* net disconnect will trigger ppp stop */
        if (bits & PPP_NET_DISCONNECT_BIT) {
            if ( modem_stage == STAGE_RUNNING ) {
                if ((xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_AUTO_SUSPEND_USER_BIT) == 0) {
                    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
                } 
                modem_stage = STAGE_STOP_PPP;
                stage_retry_times = 0;
            }

            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_NET_DISCONN, NULL, 0, 0);
            xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_DISCONNECT_BIT);
        }

        /************************************ Stage Retry logic **********************************/
        const char *stare_str = MODEM_STAGE_STR(modem_stage);
        if ( modem_stage != STAGE_RUNNING ) {
            if (stage_retry_times >= RETRY_TIMEOUT) {
                //IF stage retry timeout, retry from start
                ESP_LOGE(TAG, "Modem state %s, retry %d, timeout !", stare_str, stage_retry_times);
                ESP_LOGW(TAG, "Retry From Start !");
                if (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_RECONNECTING_BIT) {
                    xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_RECONNECTING_BIT);
                    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
                }
                modem_stage = STAGE_SYNC;
                stare_str = MODEM_STAGE_STR(modem_stage);
                stage_retry_times = 0;
            } else if (stage_retry_times > 0) {
                vTaskDelay(pdMS_TO_TICKS(retry_after_ms));
                if (((xEventGroupGetBits(s_modem_evt_hdl) & DTE_USB_DISCONNECT_BIT)
                        || (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_MODE_OFF_BIT))
                      && (modem_stage != STAGE_DTE_LOSS)) {
                    ESP_LOGW(TAG, "Modem state from %s to STAGE_WAITING, waiting for event.", stare_str);
                    continue;
                } else {
                    ESP_LOGW(TAG, "Modem state %s, Failed, retry%d, after %dms...", stare_str, stage_retry_times, retry_after_ms);
                }
            }
        }

        /************************************ Processing stage **********************************/
        
        if ((modem_stage != STAGE_WAITING) 
            && ( modem_stage != STAGE_RUNNING )){
            ESP_LOGI(TAG, "Modem state %s, Start", stare_str);
        }

        switch (modem_stage) {
        case STAGE_DTE_LOSS:
            if (_ppp_network_stop(dte) != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_WAITING;
                xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_OFF_BIT);
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_NET_DISCONN, NULL, 0, 0);
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_SIMCARD_DISCONN, NULL, 0, 0);
                goto _stage_succeed;
            }
            break;
        case STAGE_WAITING:
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case STAGE_SYNC:
            if (esp_modem_default_start(dte) != ESP_OK) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_DEV_INIT;
                goto _stage_succeed;
            }
            break;
        case STAGE_STOP_PPP:
            if (_ppp_network_stop(dte) != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_SYNC;
                goto _stage_succeed;
            }
            break;
        case STAGE_DEV_INIT:
            if (_check_dev_config() != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_DEV_ID_GET;
                goto _stage_succeed;
            }
            break;
        case STAGE_DEV_ID_GET:
            if (_check_imei_number() != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_WAITING;
                if (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_MODE_ON_BIT) {
                    xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
                    ESP_LOGI(TAG, "Network Auto reconnecting ...");
                    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_RECONNECTING_BIT);
                    modem_stage = STAGE_CHECK_SIM;
                }
                goto _stage_succeed;
            }
            break;            
        case STAGE_CHECK_SIM:
            if (_check_sim_card() != true) {
                modem_apn_index = 0;
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_CHECK_CARD;
                goto _stage_succeed;
            }
            break;
        case STAGE_CHECK_CARD:
            if (_check_imsi_iccid_number() != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_CHECK_SIGNAL;
                goto _stage_succeed;
            }
            break;            
        case STAGE_CHECK_SIGNAL:
            if (_check_signal_quality() != true)  {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_CHECK_REGIST;
                goto _stage_succeed;
            }
            break;
        case STAGE_CHECK_REGIST:
            if (_check_network_registration() != true)  {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_START_PPP;
                goto _stage_succeed;
            }
            break;
        case STAGE_START_PPP:
            if (_ppp_network_start(dte) != true)  {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_WAIT_IP;
                goto _stage_succeed;
            }
            break;
        case STAGE_WAIT_IP: {
            //waiting 60s at most
            EventBits_t con_bits = xEventGroupWaitBits(s_modem_evt_hdl, (PPP_NET_CONNECT_BIT | PPP_NET_MODE_OFF_BIT | MODEM_DESTROY_BIT), pdFALSE, pdFALSE, pdMS_TO_TICKS(60000));
            if (con_bits & PPP_NET_MODE_OFF_BIT) {
                continue;
            }
            if (con_bits & MODEM_DESTROY_BIT) {
                break;
            }
            if (con_bits & PPP_NET_CONNECT_BIT) {
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_RECONNECTING_BIT);
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_NET_CONN, NULL, 0, 0);
#ifdef CONFIG_MODEM_SUPPORT_SECONDARY_AT_PORT
                // 辅助AT口初始化（同步时序，关闭命令回显）
                if(dce->start_up(dce) == ESP_OK) ESP_LOGI(TAG, "Modem Second AT Port start_up successfully.");
#endif
                modem_stage = STAGE_RUNNING;
                goto _stage_succeed;
            } else {
                stage_retry_times = RETRY_TIMEOUT;
                if (0 == strlen(modem_custom_apn)) modem_apn_index++;
                ESP_LOGW(TAG, "Modem Got IP timeout, retry from start!");
            }
        }
        break;
        case STAGE_RUNNING:
#ifdef CONFIG_MODEM_SUPPORT_SECONDARY_AT_PORT
            modem_board_periodic_poll();
#endif
            break;
        default:
            assert(0); //no stage get in here
_stage_succeed:
            ESP_LOGI(TAG, "Modem state %s, Success!", stare_str);
            stage_retry_times = 0;
            //add delay between each stage
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }
    }

    if ( modem_stage > STAGE_START_PPP ) {
        _ppp_network_stop(dte);
    }
        
    if (config->handler) {
        esp_event_handler_unregister(MODEM_BOARD_EVENT, ESP_EVENT_ANY_ID, config->handler);
    }
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, on_modem_event));
    // destroy dte & dce
    ESP_ERROR_CHECK(esp_modem_default_destroy(dte));
    modem_board_t *board = __containerof(s_dce, modem_board_t, parent);
    free(board);
    xEventGroupClearBits(s_modem_evt_hdl, (PPP_NET_MODE_ON_BIT | PPP_NET_MODE_OFF_BIT | MODEM_DESTROY_BIT | DTE_USB_RECONNECT_BIT | PPP_NET_RECONNECTING_BIT));
    ESP_LOGI(TAG, "Modem Daemon Task Deleted!");
    memset(&modem_data, 0, sizeof(modem_data));
    modem_stage = STAGE_WAITING;
    xEventGroupSetBits(s_modem_evt_hdl, MODEM_DESTROY_DONE_BIT);
    vTaskDelete(NULL);
}

esp_err_t modem_board_init(modem_config_t *config)
{
    MODEM_CHECK(s_modem_evt_hdl == NULL, "Modem already initialized", ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "iot_usbh_modem, version: %d.%d.%d", IOT_USBH_MODEM_VER_MAJOR, IOT_USBH_MODEM_VER_MINOR, IOT_USBH_MODEM_VER_PATCH);
    MODEM_CHECK(config != NULL && config->line_buffer_size && config->rx_buffer_size && config->tx_buffer_size, "Buffer size can not be 0", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(config != NULL && config->event_task_stack_size, "Task stack size can not be 0", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(config != NULL && config->event_task_priority > CONFIG_USBH_TASK_BASE_PRIORITY, "Task priority must > USB", ESP_ERR_INVALID_ARG);
    s_modem_evt_hdl = xEventGroupCreate();
    assert(s_modem_evt_hdl != NULL);
    // if set not enter ppp mode, daemon task will suspend
    if (config->flags & MODEM_FLAGS_INIT_NOT_ENTER_PPP) {
        modem_board_ppp_auto_connect(false);
    } else {
        modem_board_ppp_auto_connect(true);
    }
    /* Create Modem Daemon task */
    TaskHandle_t daemon_task_handle = NULL;
    xTaskCreate(_modem_daemon_task, "modem_daemon", config->event_task_stack_size, config, config->event_task_priority, &daemon_task_handle);
    assert(daemon_task_handle != NULL);
    xTaskNotifyGive(daemon_task_handle);
    // If auto enter ppp and block until ppp got ip
    if (((config->flags & MODEM_FLAGS_INIT_NOT_ENTER_PPP) == 0) && ((config->flags & MODEM_FLAGS_INIT_NOT_BLOCK) == 0)) {
        xEventGroupWaitBits(s_modem_evt_hdl, PPP_NET_CONNECT_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t modem_board_deinit(void)
{
    MODEM_CHECK(s_modem_evt_hdl, "Modem not init", ESP_ERR_INVALID_STATE);
    xEventGroupSetBits(s_modem_evt_hdl, MODEM_DESTROY_BIT);
    xEventGroupWaitBits(s_modem_evt_hdl, MODEM_DESTROY_DONE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    vEventGroupDelete(s_modem_evt_hdl);
    s_modem_evt_hdl = NULL;
    ESP_LOGI(TAG, "iot_usbh_modem Deinit Success!");
    return ESP_OK;
}

esp_err_t modem_board_get_dns_info(esp_netif_dns_type_t type, esp_netif_dns_info_t *dns)
{
    MODEM_CHECK(dns != NULL && type <= ESP_NETIF_DNS_BACKUP, "Invalid DNS info", ESP_ERR_INVALID_ARG);

    switch (type) {
    case ESP_NETIF_DNS_MAIN:
        dns->ip = modem_data.ppp_dns_ip_main;
        break;
    case ESP_NETIF_DNS_BACKUP:
        dns->ip = modem_data.ppp_dns_ip_backup;
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t modem_board_get_signal_quality(int *rssi, int *ber)
{
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    esp_modem_dce_csq_ctx_t result;
    esp_err_t err = esp_modem_dce_get_signal_quality(s_dce, NULL, &result);
    if (err == ESP_OK) {
        if (rssi) {
            *rssi = result.rssi;
        }
        if (ber) {
            *ber = result.ber;
        }
    }
    return err;
}

esp_err_t modem_board_get_sim_cart_state(int *if_ready)
{
    MODEM_CHECK(if_ready != NULL, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    
#if 0   // 禁用esp_modem_dce_set_pin逻辑 
    *if_ready = false;
    esp_modem_dce_read_pin(s_dce, NULL, if_ready);
    if ((*if_ready) == false) {
        esp_modem_dce_set_pin(s_dce, MODEM_SIM_PIN_PWD, NULL);
    } else {
        return ESP_OK;
    }
#endif

    return esp_modem_dce_read_pin(s_dce, NULL, if_ready);
}

esp_err_t modem_board_get_imei_number(char *buf, size_t buf_size)
{
    MODEM_CHECK(buf != NULL && buf_size != 0, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    return esp_modem_dce_get_imei_number(s_dce, (void *)buf_size, (void *)buf);
}

esp_err_t modem_board_get_imsi_number(char *buf, size_t buf_size)
{
    MODEM_CHECK(buf != NULL && buf_size != 0, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    return esp_modem_dce_get_imsi_number(s_dce, (void *)buf_size, (void *)buf);
}

esp_err_t modem_board_get_iccid_number(char *buf, size_t buf_size)
{
    MODEM_CHECK(buf != NULL && buf_size != 0, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    return esp_modem_dce_get_iccid_number(s_dce, (void *)buf_size, (void *)buf);
}

esp_err_t modem_board_get_module_name(char *buf, size_t buf_size)
{
    MODEM_CHECK(buf != NULL && buf_size != 0, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    return esp_modem_dce_get_module_name(s_dce, (void *)buf_size, (void *)buf);
}

esp_err_t modem_board_get_operator_state(char *buf, size_t buf_size)
{
    MODEM_CHECK(buf != NULL && buf_size != 0, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    return esp_modem_dce_get_operator_name(s_dce, (void *)buf_size, (void *)buf);
}

esp_err_t modem_board_apn_init(const char *new_apn)
{
    strncpy(modem_custom_apn, new_apn, sizeof(modem_custom_apn) - 1);
    modem_custom_apn[sizeof(modem_custom_apn) - 1] = '\0';
    return ESP_OK;
}

esp_err_t modem_board_set_apn(const char *new_apn, bool force_enable)
{
    strncpy(modem_custom_apn, new_apn, sizeof(modem_custom_apn) - 1);
    modem_custom_apn[sizeof(modem_custom_apn) - 1] = '\0';
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    if (esp_modem_dce_set_apn(s_dce, new_apn) != ESP_OK) {
        ESP_LOGI(TAG, "APN not change (:%s)", new_apn);
        return ESP_ERR_INVALID_STATE;
    }
    if (force_enable) {
        if (s_modem_evt_hdl) {
            if ((xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_AUTO_SUSPEND_USER_BIT) == 0) {
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
                ESP_LOGI(TAG, "re-dial after change APN");
            } 

            if ( modem_stage >= STAGE_START_PPP) {
                modem_stage = STAGE_STOP_PPP;
            }
        }
    }
    return ESP_OK;
}

esp_err_t modem_board_ppp_auto_connect(bool enable)
{
    MODEM_CHECK(s_modem_evt_hdl != NULL, "modem not initialized", ESP_ERR_INVALID_STATE);
    if (enable) {
        xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_AUTO_SUSPEND_USER_BIT);
    } else {
        xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_AUTO_SUSPEND_USER_BIT);
    }
    ESP_LOGI(TAG, "PPP Auto Connect %s", enable ? "Enable" : "Disable");
    return ESP_OK;
}

esp_err_t modem_board_ppp_start(uint32_t timeout_ms)
{
    MODEM_CHECK(s_modem_evt_hdl != NULL, "modem not initialized", ESP_ERR_INVALID_STATE);
    xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_OFF_BIT);
    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
    if ( 0 != timeout_ms ) {
        uint32_t timeout_ms_step = 10;
        uint32_t waiting_ms = 0;
        while (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_MODE_ON_BIT) {
            vTaskDelay(pdMS_TO_TICKS(timeout_ms_step));
            waiting_ms += timeout_ms_step;
            if (waiting_ms >= timeout_ms) {
                break;
            }
        }
        if (waiting_ms >= timeout_ms) {
            ESP_LOGW(TAG, "PPP start timeout");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "PPP start succeed");
    return ESP_OK;
}

esp_err_t modem_board_ppp_stop(uint32_t timeout_ms)
{
    MODEM_CHECK(s_modem_evt_hdl != NULL, "modem not initialized", ESP_ERR_INVALID_STATE);
    xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_OFF_BIT);
    if ( 0 != timeout_ms ) {
        uint32_t timeout_ms_step = 10;
        uint32_t waiting_ms = 0;
        while (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_MODE_OFF_BIT) {
            vTaskDelay(pdMS_TO_TICKS(timeout_ms_step));
            waiting_ms += timeout_ms_step;
            if (waiting_ms >= timeout_ms) {
                break;
            }
        }
        if (waiting_ms >= timeout_ms) {
            ESP_LOGW(TAG, "PPP stop timeout");
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "PPP stop succeed");
    return ESP_OK;
}

const modem_info_t * modem_board_get_info(void)
{
    return &modem_data;
}

esp_err_t modem_board_send_at_cmd(const char *command, char *buf, size_t buf_size)
{
    MODEM_CHECK(command != NULL && buf != NULL && buf_size != 0, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    return esp_modem_dce_common_send_at_cmd(s_dce, command, (void *)buf_size, (void *)buf);
}

static esp_err_t modem_board_poll_signal_quality(uint32_t now_time)
{
    esp_err_t err = modem_board_get_signal_quality(&modem_data.rssi, &modem_data.ber);
    if (err == ESP_OK) {
        if (modem_data.rssi != 99 && modem_data.rssi > 5) {
            ESP_LOGD(TAG, "[Poll] Signal quality: rssi=%d, ber=%d, use_time(%lu ms)",
                     modem_data.rssi, modem_data.ber, (xTaskGetTickCount() * portTICK_PERIOD_MS - now_time));
        } else {
            ESP_LOGW(TAG, "[Poll] Low signal quality: rssi=%d, ber=%d", modem_data.rssi, modem_data.ber);
        }
    } else {
        ESP_LOGE(TAG, "[Poll] Get signal quality failed");
    }

    return err;
}

static void modem_board_get_gps_list(const char *module_name, const modem_cmd_item_t **gps_list, size_t *gps_list_size)
{
    *gps_list = NULL;
    *gps_list_size = 0;

    if (module_name == NULL) {
        return;
    }

    for (size_t i = 0; i < sizeof(modem_cmd_map) / sizeof(modem_cmd_map[0]); ++i) {
        if (strstr(module_name, modem_cmd_map[i].name_pattern) != NULL) {
            *gps_list = modem_cmd_map[i].gps_list;
            *gps_list_size = modem_cmd_map[i].gps_list_size;
            return;
        }
    }
}

// GGA 语句类型 (位置 + 高度)
static const char *gga_types[] = {
    "$GPGGA",  // GPS
    "$GNGGA",  // GLONASS + GPS 混合
    "$GAGGA",  // Galileo
    "$BDGGA",  // BeiDou
    "$QZSGGA", // QZSS
};

// RMC 语句类型 (位置 + 速度 + 航向)
static const char *rmc_types[] = {
    "$GPRMC",  // GPS
    "$GNRMC",  // GLONASS + GPS 混合
    "$GARMC",  // Galileo
    "$BDRMC",  // BeiDou
    "$QZSRMC", // QZSS
};

// GSV 语句类型 (卫星数)
static const char *gsv_types[] = {
    "$GPGSV",  // GPS
    "$GLGSV",  // GLONASS
    "$GAGSV",  // Galileo
    "$BDGSV",  // BeiDou
    "$QZSSGSV", // QZSS
};

// 周期任务解析buffer
EXT_RAM_BSS_ATTR static char periodic_handle_buffer[2048] = {0};

// 检查 NMEA 语句是否匹配指定类型数组
static bool nmea_check_type(const char *line, const char **types, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (strncmp(line, types[i], strlen(types[i])) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 解析 NMEA GGA 语句
 * @param line NMEA 语句行
 * @param tmp gps_location_t 结构体指针
 * @return true 解析成功
 */
static bool parse_gga(const char *line, gps_location_t *tmp) {
    char utc[32] = {0}, lat[32] = {0}, ns = 0, lon[32] = {0}, ew = 0;
    int fix = 0, sats = 0;
    double hdop = 99.0, alt = 0.0;
    
    // 解析 GGA 语句 (支持所有类型)
    int parsed = sscanf(line, "$%*c%*cGGA,%[^,],%[^,],%c,%[^,],%c,%d,%d,%lf,%lf",
                        utc, lat, &ns, lon, &ew, &fix, &sats, &hdop, &alt);
    
    if (parsed < 9 || fix == 0) {
        return false;
    }
    
    // 转换坐标格式 (DDMM.MMMM → DD.DDDDDD)
    double lat_deg = atof(lat);
    double lon_deg = atof(lon);
    double latitude = (int)(lat_deg / 100) + (lat_deg - (int)(lat_deg / 100) * 100) / 60.0;
    double longitude = (int)(lon_deg / 100) + (lon_deg - (int)(lon_deg / 100) * 100) / 60.0;

    // 处理南北纬、东西经
    if (ns == 'S') {
        latitude = -latitude;
    }
    if (ew == 'W') {
        longitude = -longitude;
    }
    
    // 选择更可靠的数据
    // 优先级: 卫星数多 > HDOP 小 > 海拔高
    bool update = false;
    
    // 首次解析或卫星数更多
    if (tmp->satellites == 0 || sats > tmp->satellites) {
        update = true;
    } else if (sats == tmp->satellites && hdop < tmp->hdop) {
        // 卫星数相同时，选择 HDOP 更小的
        update = true;
    }
    
    if (update) {
        tmp->latitude = latitude;
        tmp->ns = ns;
        tmp->longitude = longitude;
        tmp->ew = ew;
        tmp->fix = fix;
        tmp->satellites = sats;
        tmp->altitude = alt;
        tmp->hdop = hdop;
    }
    
    return true;
}

/**
 * @brief 解析 NMEA RMC 语句
 * @param line NMEA 语句行
 * @param tmp gps_location_t 结构体指针
 * @return true 解析成功
 */
static bool parse_rmc(const char *line, gps_location_t *tmp) {
    char utc[32] = {0}, status = 0;
    char lat[32] = {0}, ns = 0, lon[32] = {0}, ew = 0;
    double speed = 0.0, course = 0.0;
    char date[32] = {0}, mode = 0, nav_status = 0;
    char mag_var_dir = 0;
    double mag_var = 0.0;
    
    // 解析 RMC 语句 (支持所有类型)
    int parsed = sscanf(line, "$%*c%*cRMC,%[^,],%c,%[^,],%c,%[^,],%c,%lf,%lf,%[^,],%lf,%c,%c,%c",
                        utc, &status, lat, &ns, lon, &ew, &speed, &course, date, &mag_var, &mag_var_dir, &mode, &nav_status);
    
    if (parsed < 13 || status != 'A') {
        return false;
    }
    
    // 转换坐标格式 (DDMM.MMMM → DD.DDDDDD)
    double lat_deg = atof(lat);
    double lon_deg = atof(lon);
    double latitude = (int)(lat_deg / 100) + (lat_deg - (int)(lat_deg / 100) * 100) / 60.0;
    double longitude = (int)(lon_deg / 100) + (lon_deg - (int)(lon_deg / 100) * 100) / 60.0;

    // 处理南北纬、东西经
    if (ns == 'S') {
        latitude = -latitude;
    }
    if (ew == 'W') {
        longitude = -longitude;
    }
    
    // RMC 主要更新速度和航向，位置可能与 GGA 重复
    // 只在未获取有效位置时更新
    if (tmp->fix == 0) {
        tmp->latitude = latitude;
        tmp->ns = ns;
        tmp->longitude = longitude;
        tmp->ew = ew;
        tmp->fix = 1;
    }
    
    // 更新速度和航向
    tmp->speed = speed;      // 节
    tmp->course = course;    // 度
    
    // 更新磁偏数据
    tmp->mag_var = mag_var;         // 磁偏角
    tmp->mag_var_dir = mag_var_dir; // 磁偏方向 (E/W)
    
    return true;
}

// 解析 GSV 语句 (卫星数和信噪比)
static bool parse_gsv(const char *line, gps_location_t *tmp)
{
    char *token;
    char *subtoken[20] = {0};
    char tmp_line[128];
    uint8_t *num_ptr = NULL;
    uint8_t *snr_ptr = NULL;
    
    // 识别卫星系统类型
    if (strncmp(line, "$GPGSV", 6) == 0) {
        num_ptr = &tmp->gps_num;
        snr_ptr = &tmp->gps_snr;
    } else if (strncmp(line, "$GLGSV", 6) == 0) {
        num_ptr = &tmp->gl_num;
        snr_ptr = &tmp->gl_snr;
    } else if (strncmp(line, "$GAGSV", 6) == 0) {
        num_ptr = &tmp->gal_num;
        snr_ptr = &tmp->gal_snr;
    } else if (strncmp(line, "$BDGSV", 6) == 0) {
        num_ptr = &tmp->bd_num;
        snr_ptr = &tmp->bd_snr;
    } else if (strncmp(line, "$QZSSGSV", 8) == 0) {
        num_ptr = &tmp->qzss_num;
        snr_ptr = &tmp->qzss_snr;
    } else {
        return false;
    }
    
    strncpy(tmp_line, line, sizeof(tmp_line) - 1);
    tmp_line[sizeof(tmp_line) - 1] = '\0';
    int idx = 0;
    token = strtok(tmp_line, ",");
    while (token && idx < 20) {
        subtoken[idx++] = token;
        token = strtok(NULL, ",");
    }
    
    // 解析可见卫星数
    if (idx > 3 && num_ptr) {
        uint8_t num = (uint8_t)atoi(subtoken[3]);
        if (num > *num_ptr) *num_ptr = num;
    }
    
    // 解析 SNR (取最大值)
    // 每组 4 个字段: PRN, 仰角, 方位角, SNR
    if (snr_ptr) {
        for (int i = 4; i + 3 < idx; i += 4) {
            char *snr_str = subtoken[i + 3];
            if (snr_str && strlen(snr_str) > 0) {
                uint8_t snr = (uint8_t)atoi(snr_str);
                // 过滤异常值 (SNR 正常范围 0-70)
                if (snr > 0 && snr < 100) {
                    if (snr > *snr_ptr) *snr_ptr = snr;
                }
            }
        }
    }
    
    return true;
}

/*
┌─────────────────────────────────────────────────────────────┐
│                    数据选择优先级                                  │
├─────────────────────────────────────────────────────────────┤
│  位置 (GGA)         │ 卫星数多 → HDOP小 → 数据新                      │
│  速度/航向(RMC)       │ 始终使用最新数据                                │
│  卫星数 (GSV)        │ 取各系统最大值                                 │
└─────────────────────────────────────────────────────────────┘
*/
static int nmea_parse_gps_location(const char *nmea_str, gps_location_t *info)
{
    char line[128];
    const char *p = nmea_str;
    gps_location_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.ns = 'N';
    tmp.ew = 'E';

    while (*p) {
        int len = 0;
        while (*p && *p != '\n' && len < sizeof(line) - 1) {
            line[len++] = *p++;
        }
        line[len] = '\0';
        if (*p == '\n') p++;

        // 去除空格
        char *line_start = line;
        while (*line_start == ' ') line_start++;

        // 使用数组判断语句类型
        // 解析 RMC (支持所有类型)
        if (nmea_check_type(line_start, rmc_types, sizeof(rmc_types) / sizeof(rmc_types[0]))) {
            parse_rmc(line_start, &tmp);
        }
        // 解析 GGA (支持所有类型)
        else if (nmea_check_type(line_start, gga_types, sizeof(gga_types) / sizeof(gga_types[0]))) {
            parse_gga(line_start, &tmp);
        }
        // 解析 GSV (支持所有类型)
        else if (nmea_check_type(line_start, gsv_types, sizeof(gsv_types) / sizeof(gsv_types[0]))) {
            parse_gsv(line_start, &tmp);
        }
    }
    
    *info = tmp;
    return info->fix;
}

static esp_err_t modem_board_poll_gps(uint32_t now_time)
{
    const modem_cmd_item_t *cmd_list = NULL;
    size_t cmd_list_size = 0;
    bool flag_value = false;

    // 根据 module_name 选择不同的命令
    modem_board_get_gps_list(modem_data.module_name, &cmd_list, &cmd_list_size);

    if ( cmd_list ) {
        for ( uint8_t i = 0 ; i < cmd_list_size ; i++ ) {
            memset(periodic_handle_buffer, 0, sizeof(periodic_handle_buffer));
            esp_err_t err = modem_board_send_at_cmd(cmd_list[i].cmd, periodic_handle_buffer, sizeof(periodic_handle_buffer));
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed. Command: %s Response: %s", cmd_list[i].cmd, periodic_handle_buffer);
                return err;
            } else {
                ESP_LOGD(TAG, "Succeeded. Command: %s Response: %s.", cmd_list[i].cmd, periodic_handle_buffer);
                if (nmea_parse_gps_location(periodic_handle_buffer, &modem_data.gps_info)) flag_value = true;
            }
        }

        if ( flag_value ) {
            ESP_LOGD(TAG, "Location fixed: lat=%.8f%c, lon=%.8f%c, alt=%.2f m, "
                     "fix=%d, satellites=%d, "
                     "GPS(num=%u, snr=%u), "
                     "GLONASS(num=%u, snr=%u), "
                     "Galileo(num=%u, snr=%u), "
                     "QZSS(num=%u, snr=%u), "
                     "BeiDou(num=%u, snr=%u), "
                     "time=%lu ms",
                modem_data.gps_info.latitude, modem_data.gps_info.ns,
                modem_data.gps_info.longitude, modem_data.gps_info.ew,
                modem_data.gps_info.altitude,
                modem_data.gps_info.fix, modem_data.gps_info.satellites,
                modem_data.gps_info.gps_num, modem_data.gps_info.gps_snr,
                modem_data.gps_info.gl_num, modem_data.gps_info.gl_snr,
                modem_data.gps_info.gal_num, modem_data.gps_info.gal_snr,
                modem_data.gps_info.qzss_num, modem_data.gps_info.qzss_snr,
                modem_data.gps_info.bd_num, modem_data.gps_info.bd_snr,
                (xTaskGetTickCount() * portTICK_PERIOD_MS - now_time));
        }
    } 

    return ESP_OK;
}

void modem_board_periodic_poll(void)
{
    esp_err_t err = ESP_OK;
    static uint32_t pre_signal_quality_time = 0;
    static uint32_t pre_gps_data_time = 0;
    
    uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Only poll signal quality when modem is running
    if (modem_stage == STAGE_RUNNING) {
        // Check if it's time to poll signal quality
        if (0 != MODEM_POLL_SIM_SIGNAL_QUALITY_MS && (now_time - pre_signal_quality_time) >= MODEM_POLL_SIM_SIGNAL_QUALITY_MS) {    
            err = modem_board_poll_signal_quality(now_time);
            pre_signal_quality_time = now_time;
        } else if ( 0 != MODEM_POLL_GNSS_GPS_MS && (now_time - pre_gps_data_time) >= MODEM_POLL_GNSS_GPS_MS ) {
            err = modem_board_poll_gps(now_time);
            pre_gps_data_time = now_time;
        }
    } else {
        // Reset polling timer if modem is not running
        pre_signal_quality_time = now_time;
        pre_gps_data_time = now_time;
    }
}

