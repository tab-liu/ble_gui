/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

ESP_EVENT_DECLARE_BASE(MODEM_BOARD_EVENT);

typedef enum {
    MODEM_EVENT_DTE_CONN = 0,        /*!< Modem DTE Connected */
    MODEM_EVENT_DTE_DISCONN,         /*!< Modem DTE Disconnected */
    MODEM_EVENT_DEV_ID_GET,          /*!< Modem device ID obtained */          
    MODEM_EVENT_SIMCARD_CONN,        /*!< Modem SIM CARD Connected */
    MODEM_EVENT_SIMCARD_DISCONN,     /*!< Modem SIM CARD Disconnected */
    MODEM_EVENT_DTE_RESTART,         /*!< Modem DTE Reset to restart */
    MODEM_EVENT_DTE_RESTART_DONE,    /*!< Modem DTE Restart done */
    MODEM_EVENT_NET_CONN,            /*!< Modem Net Connected */
    MODEM_EVENT_NET_DISCONN,         /*!< Modem Net Disconnected */
    MODEM_EVENT_WIFI_STA_CONN,       /*!< Modem Wi-Fi new station Connected */
    MODEM_EVENT_WIFI_STA_DISCONN,    /*!< Modem Wi-Fi station disconnected */
    MODEM_EVENT_UNKNOWN              /*!< Modem Unknown Response */
} modem_event_t;

#define MODEM_DEFAULT_CONFIG()\
    {                                \
        .rx_buffer_size = 1024*15,   \
        .tx_buffer_size = 1024*15,   \
        .line_buffer_size = 1600,    \
        .event_task_priority = CONFIG_USBH_TASK_BASE_PRIORITY + 1,\
        .event_task_stack_size = 3072\
    }

#define MODEM_FLAGS_INIT_NOT_FORCE_RESET   (1UL<< 1)   /*!< If set, will not reset 4g modem using reset pin during init */
#define MODEM_FLAGS_INIT_NOT_ENTER_PPP     (1UL<< 2)   /*!< If set, will not enter ppp mode during init */
#define MODEM_FLAGS_INIT_NOT_BLOCK         (1UL<< 3)   /*!< If set, will not wait until ppp got ip before modem_board_init return */

typedef struct {
    int rx_buffer_size;             /*!< USB RX Buffer Size */
    int tx_buffer_size;             /*!< USB TX Buffer Size */
    int line_buffer_size;           /*!< Line buffer size for command mode */
    int event_task_priority;        /*!< USB Event/Data Handler Task Priority*/
    int event_task_stack_size;      /*!< USB Event/Data Handler Task Stack Size*/
    esp_event_handler_t handler;    /*!< Modem event handler */
    void *handler_arg;              /*!< Modem event handler arg */
    int flags;                      /*!< Modem config flag bits */
} modem_config_t;

#pragma pack(1)

/**
 * @brief GPS location information structure
 */
typedef struct {
    double latitude;       /*!< Latitude (decimal degrees) */
    char ns;              /*!< North/South indicator ('N'/'S') */
    double longitude;     /*!< Longitude (decimal degrees) */
    char ew;              /*!< East/West indicator ('E'/'W') */
    int fix;              /*!< Fix status (1 = valid, 0 = invalid) */
    int satellites;       /*!< Number of satellites used in fix */
    double altitude;      /*!< Altitude (meters) */
    double hdop;          /*!< Horizontal Dilution of Precision */
    double speed;         /*!< Speed over ground (knots) */
    double course;        /*!< Course over ground (degrees) */
    double mag_var;       /*!< Magnetic variation (degrees) */
    char mag_var_dir;     /*!< Magnetic variation direction ('E'/'W') */
    
    // GPS 卫星
    uint8_t gps_num;     /*!< Number of GPS satellites in view */
    uint8_t gps_snr;     /*!< GPS maximum SNR (dB-Hz) */
    
    // GLONASS 卫星
    uint8_t gl_num;      /*!< Number of GLONASS satellites in view */
    uint8_t gl_snr;      /*!< GLONASS maximum SNR (dB-Hz) */
    
    // Galileo 卫星
    uint8_t gal_num;     /*!< Number of Galileo satellites in view */
    uint8_t gal_snr;     /*!< Galileo maximum SNR (dB-Hz) */
    
    // BeiDou 卫星
    uint8_t bd_num;      /*!< Number of BeiDou satellites in view */
    uint8_t bd_snr;      /*!< BeiDou maximum SNR (dB-Hz) */

    // QZSS 卫星
    uint8_t qzss_num;    /*!< Number of QZSS satellites in view */
    uint8_t qzss_snr;    /*!< QZSS maximum SNR (dB-Hz) */
} gps_location_t;

/**
 * @brief Structure to store 4G modem information
 *
 * This structure holds the key runtime and identification information for a 4G modem,
 * including module model, operator, SIM card details, network parameters, interface info,
 * and signal quality.
 */
typedef struct {
    char module_name[32];                /*!< Modem module name/model (e.g., "EC200U", "SIM7600G") */
    char operater_name[32];              /*!< Operator name string (e.g., "CHINA MOBILE") */
    char IMEI[32];                       /*!< Modem IMEI (International Mobile Equipment Identity), unique device ID */
    char IMSI[32];                       /*!< SIM card IMSI (International Mobile Subscriber Identity), unique SIM ID */
    char ICCID[32];                      /*!< SIM card ICCID (Integrated Circuit Card Identifier), SIM card serial number */
    esp_ip_addr_t ppp_dns_ip_main;       /*!< Main DNS IP address assigned for PPP connection */
    esp_ip_addr_t ppp_dns_ip_backup;     /*!< Backup DNS IP address assigned for PPP connection */
    esp_netif_ip_info_t ppp_ip_info;     /*!< PPP connection IP information (local IP, netmask, gateway) */
    uint8_t ppp_mac[6];                  /*!< MAC address used by PPP interface (if applicable, usually zero for PPP) */
    int rssi;                            /*!< Signal strength (RSSI), 2~31: -109~-51 dBm, 99 means unknown */
    int ber;                             /*!< Bit error rate (BER), in percent, 99 means unknown or not detectable */
    gps_location_t gps_info;             /*!< GPS location information */
} modem_info_t;

#pragma pack()

/**
 * @brief Init all about the modem object
 *
 * @param config modem board config value
 * @return ** esp_err_t
 */
esp_err_t modem_board_init(modem_config_t *config);

/**
 * @brief Deinit all about the modem object
 *
 * @return
 *      - ESP_ERR_INVALID_STATE
 *      - ESP_OK
 */
esp_err_t modem_board_deinit(void);

/**
 * @brief Get the DNS information of modem ppp interface
 *
 * @param  type Type of DNS Server to get: ESP_NETIF_DNS_MAIN, ESP_NETIF_DNS_BACKUP
 * @param dns  DNS Server result is written here on success
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_dns_info(esp_netif_dns_type_t type, esp_netif_dns_info_t *dns);

/**
 * @brief Get 4g signal quality value
 *
 * @param rssi received signal strength indication <rssi>, 2..30: -109..-53 dBm, 99 means not known or not detectable
 * @param ber channel bit error rate (in percent)
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_signal_quality(int *rssi, int *ber);

/**
 * @brief Check if SIM Card ready
 *
 * @param if_ready output true if ready
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_sim_cart_state(int *if_ready);

/**
 * @brief Force reset modem through reset pin
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_force_reset(void);

/**
 * @brief Get the IMEI (International Mobile Equipment Identity) number of the modem.
 *
 * This function retrieves the IMEI number from the modem and stores it in the provided buffer.
 *
 * @param buf      Buffer to store the IMEI string.
 * @param buf_size Size of the buffer.
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t modem_board_get_imei_number(char *buf, size_t buf_size);

/**
 * @brief Get the IMSI (International Mobile Subscriber Identity) number of the modem.
 *
 * This function retrieves the IMSI number from the modem and stores it in the provided buffer.
 *
 * @param buf      Buffer to store the IMSI string.
 * @param buf_size Size of the buffer.
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t modem_board_get_imsi_number(char *buf, size_t buf_size);

/**
 * @brief Get the ICCID (Integrated Circuit Card Identifier) number of the modem.
 *
 * This function retrieves the ICCID number from the modem and stores it in the provided buffer.
 *
 * @param buf      Buffer to store the ICCID string.
 * @param buf_size Size of the buffer.
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t modem_board_get_iccid_number(char *buf, size_t buf_size);

/**
 * @brief This function is used to get the operator's name, otherwise return ESP_FAIL if the network is not registered.
 *
 * @param buf buffer to store name
 * @param buf_size buffer size
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_operator_state(char *buf, size_t buf_size);

/**
 * @brief Initialize or update the custom APN for the modem
 *
 * This function copies the provided APN string into the global modem_custom_apn buffer.
 * The APN will be used for subsequent modem connections.
 *
 * @param new_apn Pointer to the new APN string to set
 * @return ESP_OK on success
 */
esp_err_t modem_board_apn_init(const char *new_apn);

/**
 * @brief Set APN, force_enable true will try to re-dialup if not will be enabled in next dialup
 *
 * @param new_apn APN string
 * @param force_enable if force enable
 * @return ** esp_err_t
 */
esp_err_t modem_board_set_apn(const char *new_apn, bool force_enable);

/**
 * @brief If enable modem network auto-connect, true will re-dialup if PPP/usb lost
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_ppp_auto_connect(bool enable);

/**
 * @brief If enter network mode, if enabled network AT command will not response
 * if need AT response during network mode, the modem must support secondary AT port
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_ppp_start(uint32_t timeout_ms);

/**
 * @brief If exit network mode, AT command can be response after ppp stop
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_ppp_stop(uint32_t timeout_ms);

/**
 * @brief Get the pointer to the static modem information structure.
 *
 * @return Pointer to the static modem_info_t structure.
 */
const modem_info_t * modem_board_get_info(void);

/**
 * @brief Send a custom AT command to the modem and get the response.
 *
 * This function sends the specified AT command to the modem and stores the response in the provided buffer.
 *
 * @param command   AT command string to send (null-terminated)
 * @param buf       Buffer to store the modem's response string
 * @param buf_size  Size of the response buffer
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t modem_board_send_at_cmd(const char *command, char *buf, size_t buf_size);

/**
 * @brief Get the module name of the modem.
 *
 * This function retrieves the modem's module name (model identifier) and stores it in the provided buffer.
 *
 * @param buf      Buffer to store the module name string.
 * @param buf_size Size of the buffer.
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t modem_board_get_module_name(char *buf, size_t buf_size);

/**
 * @brief Periodically poll the modem board status or perform periodic tasks.
 *
 * This function should be called regularly to handle modem board maintenance or status checks.
 */
void modem_board_periodic_poll(void);

#ifdef __cplusplus
}
#endif
