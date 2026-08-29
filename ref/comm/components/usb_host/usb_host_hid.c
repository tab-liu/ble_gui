/**
  ******************************************************************************
  * @file      usb_host_hid.c
  * @version   1.0
  * @author    lixingyu
  * @date      2025/12/31
  * @brief     USB主机HID类设备处理
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2025/12/31 <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
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

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

/* ======================== 本地模块文件引用（可选） ============================= */

#include "usb_host_hid.h"
#include "usb_host_common.h"
#include "utils.h"

/* ================================ 文件内宏定义 ================================ */

#define TAG "[USB_HOST_HID]"

/* Main char symbol for ENTER key */
#define KEYBOARD_ENTER_MAIN_CHAR    '\r'
/* When set to 1 pressing ENTER will be extending with LineFeed during serial debug output */
#define KEYBOARD_ENTER_LF_EXTEND    1

/*小键盘地址范围*/
#define KEYPAD_KEY_MIN HID_KEY_KEYPAD_DIV
#define KEYPAD_KEY_MAX HID_KEY_KEYPAD_EQUAL

/*键盘输入行最大字符串长度*/
#define MAX_KEY_INPUT_LEN 128

/*键盘输入行超时时间*/
#define KEY_INPUT_TIMEOUT_MS 15000

/*键盘输出字符回显使能*/
#define CONFID_USB_HID_PRINT_OUT_LOG

/* ============================== 文件内本地结构体定义 ================================ */

#pragma pack(1)

/**
 * @brief Key event
 */
typedef struct {
    enum key_state {
        KEY_STATE_PRESSED = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

/* HID Host - Device related info */
typedef struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
    void *arg;
} hid_host_related_info_t;

/* 键盘输入参数队列*/
typedef struct {
    char *data;
    size_t len;
} key_input_msg_t;

#pragma pack()

/* =============================== 文件内全局变量 ================================ */

/**
 * @brief HID Protocol string names
 */
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

// 键盘主要映射表
static const uint8_t keycode2ascii [57][2] = {
    {0, 0}, /* HID_KEY_NO_PRESS        */
    {0, 0}, /* HID_KEY_ROLLOVER        */
    {0, 0}, /* HID_KEY_POST_FAIL       */
    {0, 0}, /* HID_KEY_ERROR_UNDEFINED */
    {'a', 'A'}, /* HID_KEY_A               */
    {'b', 'B'}, /* HID_KEY_B               */
    {'c', 'C'}, /* HID_KEY_C               */
    {'d', 'D'}, /* HID_KEY_D               */
    {'e', 'E'}, /* HID_KEY_E               */
    {'f', 'F'}, /* HID_KEY_F               */
    {'g', 'G'}, /* HID_KEY_G               */
    {'h', 'H'}, /* HID_KEY_H               */
    {'i', 'I'}, /* HID_KEY_I               */
    {'j', 'J'}, /* HID_KEY_J               */
    {'k', 'K'}, /* HID_KEY_K               */
    {'l', 'L'}, /* HID_KEY_L               */
    {'m', 'M'}, /* HID_KEY_M               */
    {'n', 'N'}, /* HID_KEY_N               */
    {'o', 'O'}, /* HID_KEY_O               */
    {'p', 'P'}, /* HID_KEY_P               */
    {'q', 'Q'}, /* HID_KEY_Q               */
    {'r', 'R'}, /* HID_KEY_R               */
    {'s', 'S'}, /* HID_KEY_S               */
    {'t', 'T'}, /* HID_KEY_T               */
    {'u', 'U'}, /* HID_KEY_U               */
    {'v', 'V'}, /* HID_KEY_V               */
    {'w', 'W'}, /* HID_KEY_W               */
    {'x', 'X'}, /* HID_KEY_X               */
    {'y', 'Y'}, /* HID_KEY_Y               */
    {'z', 'Z'}, /* HID_KEY_Z               */
    {'1', '!'}, /* HID_KEY_1               */
    {'2', '@'}, /* HID_KEY_2               */
    {'3', '#'}, /* HID_KEY_3               */
    {'4', '$'}, /* HID_KEY_4               */
    {'5', '%'}, /* HID_KEY_5               */
    {'6', '^'}, /* HID_KEY_6               */
    {'7', '&'}, /* HID_KEY_7               */
    {'8', '*'}, /* HID_KEY_8               */
    {'9', '('}, /* HID_KEY_9               */
    {'0', ')'}, /* HID_KEY_0               */
    {KEYBOARD_ENTER_MAIN_CHAR, KEYBOARD_ENTER_MAIN_CHAR}, /* HID_KEY_ENTER           */
    {0, 0}, /* HID_KEY_ESC             */
    {'\b', 0}, /* HID_KEY_DEL             */
    {0, 0}, /* HID_KEY_TAB             */
    {' ', ' '}, /* HID_KEY_SPACE           */
    {'-', '_'}, /* HID_KEY_MINUS           */
    {'=', '+'}, /* HID_KEY_EQUAL           */
    {'[', '{'}, /* HID_KEY_OPEN_BRACKET    */
    {']', '}'}, /* HID_KEY_CLOSE_BRACKET   */
    {'\\', '|'}, /* HID_KEY_BACK_SLASH      */
    {'\\', '|'}, /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
    {';', ':'}, /* HID_KEY_COLON           */
    {'\'', '"'}, /* HID_KEY_QUOTE           */
    {'`', '~'}, /* HID_KEY_TILDE           */
    {',', '<'}, /* HID_KEY_LESS            */
    {'.', '>'}, /* HID_KEY_GREATER         */
    {'/', '?'} /* HID_KEY_SLASH           */
};

// 小键盘ASCII映射表，索引为 key_code - KEYPAD_KEY_MIN
static const char keypad_ascii_map[KEYPAD_KEY_MAX - KEYPAD_KEY_MIN + 1] = {
    '/',  // HID_KEY_KEYPAD_DIV      0x54
    '*',  // HID_KEY_KEYPAD_MUL      0x55
    '-',  // HID_KEY_KEYPAD_SUB      0x56
    '+',  // HID_KEY_KEYPAD_ADD      0x57
    KEYBOARD_ENTER_MAIN_CHAR, // HID_KEY_KEYPAD_ENTER 0x58
    '1',  // HID_KEY_KEYPAD_1        0x59
    '2',  // HID_KEY_KEYPAD_2        0x5A
    '3',  // HID_KEY_KEYPAD_3        0x5B
    '4',  // HID_KEY_KEYPAD_4        0x5C
    '5',  // HID_KEY_KEYPAD_5        0x5D
    '6',  // HID_KEY_KEYPAD_6        0x5E
    '7',  // HID_KEY_KEYPAD_7        0x5F
    '8',  // HID_KEY_KEYPAD_8        0x60
    '9',  // HID_KEY_KEYPAD_9        0x61
    '0',  // HID_KEY_KEYPAD_0        0x62
    '.',  // HID_KEY_KEYPAD_DELETE   0x63
    '/',  // HID_KEY_KEYPAD_SLASH    0x64
    // 可继续补充其它KEYPAD键码
    0,    // 0x65
    0,    // 0x66
    '=',  // HID_KEY_KEYPAD_EQUAL    0x67
};

// 键盘输入行数据队列
static QueueHandle_t key_input_queue = NULL;

// 键盘输入缓存及其长度
EXT_RAM_BSS_ATTR static char key_input_buffer[MAX_KEY_INPUT_LEN] = {0};
static size_t key_input_len = 0;

/* ================================ 模块函数定义 ================================ */

/**
 * @brief Makes new line depending on report output protocol type
 *
 * @param[in] proto Current protocol to output
 */
static void hid_print_new_device_report_header(hid_protocol_t proto)
{
    static hid_protocol_t prev_proto_output = -1;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        if (proto == HID_PROTOCOL_MOUSE) {
            ESP_LOGW(TAG, "New_device_report : Mouse");
        } else if (proto == HID_PROTOCOL_KEYBOARD) {
            ESP_LOGW(TAG, "New_device_report : Keyboard");
        } else {
            ESP_LOGW(TAG, "New_device_report : Generic");
        }
    }
}

/**
 * @brief HID Keyboard modifier verification for capitalization application (right or left shift)
 *
 * @param[in] modifier
 * @return true  Modifier was pressed (left or right shift)
 * @return false Modifier was not pressed (left or right shift)
 *
 */
static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier)
{
    if (((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) ||
            ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT)) {
        return true;
    }
    return false;
}

/**
 * @brief HID Keyboard get char symbol from key code
 *
 * @param[in] modifier  Keyboard modifier data
 * @param[in] key_code  Keyboard key code
 * @param[in] key_char  Pointer to key char data
 *
 * @return true  Key scancode converted successfully
 * @return false Key scancode unknown
 */
static inline bool hid_keyboard_get_char(uint8_t modifier,
                                         uint8_t key_code,
                                         unsigned char *key_char)
{
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;
 
    // 主键盘区
    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
        *key_char = keycode2ascii[key_code][mod];
        return true;
    }

    // 小键盘区直接索引查找
    if ((key_code >= KEYPAD_KEY_MIN) && (key_code <= KEYPAD_KEY_MAX)) {
        *key_char = keypad_ascii_map[key_code - KEYPAD_KEY_MIN];
        return true;
    }

    // 其它未支持
    ESP_LOGE(TAG, "hid_keyboard_get_char failed (0x%x)", key_code);

    return false;
}


/**
 * @brief HID Keyboard print char symbol
 *
 * @param[in] key_char  Keyboard char to stdout
 */
static inline void hid_keyboard_print_char(unsigned int key_char)
{
    if (!!key_char) {
        putchar(key_char);
#if (KEYBOARD_ENTER_LF_EXTEND)
        if (KEYBOARD_ENTER_MAIN_CHAR == key_char) {
            putchar('\n');
        }
#endif // KEYBOARD_ENTER_LF_EXTEND
        fflush(stdout);
    }
}

/**
 * @brief 申请内存并将输入字符串发送到队列
 *
 * @param[in] buffer 输入字符串缓冲区
 * @param[in] len    输入字符串长度
 */
static void hid_send_key_input_to_queue(const char *buffer, size_t len)
{
    if (!buffer || len == 0) {
        return;
    }
    char *send_buf = calloc(len + 1, 1);
    if (send_buf) {
        memcpy(send_buf, buffer, len);
        send_buf[len] = '\0';
        key_input_msg_t msg = {
            .data = send_buf,
            .len = len
        };
        if (key_input_queue) {
            if (xQueueSend(key_input_queue, &msg, 0) != pdPASS) {
                ESP_LOGW(TAG, "Key input queue full, drop line.");
                free(send_buf);
            }
        } else {
            free(send_buf);
        }
    }
}

/**
 * @brief 处理检测到的按键字符，存入输入缓冲区，支持回车提交和退格删除
 *
 * 功能说明：
 * 1. 若本次输入与上次输入超过指定时长，则重置输入缓冲区。
 * 2. 支持回车（Enter）提交整行输入，并重置缓冲区。
 * 3. 支持退格（Del/Backspace）删除上一个字符。
 * 4. 输入缓冲区溢出时自动重置。
 *
 * @param[in] key_code 按键代码
 * @param[in] key_char 按键对应字符
 */
static void hid_store_key_char(uint8_t key_code, unsigned char key_char)
{
    static uint32_t last_input_time = 0;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 超时重置
    if (key_input_len != 0 && last_input_time != 0 && (now - last_input_time > KEY_INPUT_TIMEOUT_MS)) {
        key_input_len = 0;
        key_input_buffer[0] = '\0';
        ESP_LOGE(TAG, "Input timeout, buffer reset.");
    }
    last_input_time = now;

    if (key_input_len < MAX_KEY_INPUT_LEN - 1) {
        if ((HID_KEY_KEYPAD_ENTER == key_code) || (HID_KEY_ENTER == key_code)) {
            // 回车
            ESP_LOGI(TAG, "Input line: %s", key_input_buffer);
            hid_send_key_input_to_queue(key_input_buffer, key_input_len);
            key_input_len = 0;
            key_input_buffer[0] = '\0';
        } else if ((HID_KEY_DEL == key_code) || (HID_KEY_DELETE == key_code)) {
            // 退格
            if (key_input_len > 0) {
                key_input_len--;
                ESP_LOGW(TAG, "Delete char: %c", key_input_buffer[key_input_len]);
                key_input_buffer[key_input_len] = '\0';
            }
        } else {
            if ( key_char != '\0' ) {
                key_input_buffer[key_input_len++] = key_char;
                key_input_buffer[key_input_len] = '\0';
            }
        }
    } else {
        key_input_len = 0;
        key_input_buffer[0] = '\0';
        ESP_LOGE(TAG, "Input buffer overflow, reset.");
    }
}

/**
 * @brief Key Event. Key event with the key code, state and modifier.
 *
 * @param[in] key_event Pointer to Key Event structure
 *
 */
static void key_event_callback(key_event_t *key_event)
{
    unsigned char key_char = '\0';

    hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

    if (KEY_STATE_PRESSED == key_event->state) {
        if (hid_keyboard_get_char(key_event->modifier,
                                  key_event->key_code, &key_char)) {
#ifdef CONFID_USB_HID_PRINT_OUT_LOG
            hid_keyboard_print_char(key_char);
#endif
        }

        hid_store_key_char(key_event->key_code, key_char);
    }
}

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t *const src,
                             uint8_t key,
                             unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length)
{
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t *)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = { 0 };
    key_event_t key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {

        // key has been released verification
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        // key has been pressed verification
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host Mouse Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_mouse_report_callback(const uint8_t *const data, const int length)
{
    hid_mouse_input_report_boot_t *mouse_report = (hid_mouse_input_report_boot_t *)data;

    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        return;
    }

    static int x_pos = 0;
    static int y_pos = 0;

    // Calculate absolute position from displacement
    x_pos += mouse_report->x_displacement;
    y_pos += mouse_report->y_displacement;

    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

#ifdef CONFID_USB_HID_PRINT_OUT_LOG
    printf("X: %06d\tY: %06d\t|%c|%c|\r",
           x_pos, y_pos,
           (mouse_report->buttons.button1 ? 'o' : ' '),
           (mouse_report->buttons.button2 ? 'o' : ' '));
    fflush(stdout);
#endif    
}

/**
 * @brief USB HID Host Generic Interface report callback handler
 *
 * 'generic' means anything else than mouse or keyboard
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_generic_report_callback(const uint8_t *const data, const int length)
{
    hid_print_new_device_report_header(HID_PROTOCOL_NONE);
    
#ifdef CONFID_USB_HID_PRINT_OUT_LOG    
    for (int i = 0; i < length; i++) {
        printf("%02X", data[i]);
    }
    putchar('\r');
#endif    
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  data,
                                                                  64,
                                                                  &data_length));

        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
#ifdef CONFIG_USB_HID_KEYBOARD_ENABLE                
                hid_host_keyboard_report_callback(data, data_length);
#endif
            } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
#ifdef CONFIG_USB_HID_MOUSE_ENABLE            
                hid_host_mouse_report_callback(data, data_length);
#endif
            }
        } else {
#ifdef CONFIG_USB_HID_GENERAL_ENABLE        
            hid_host_generic_report_callback(data, data_length);
#endif
        }

        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
                 hid_proto_name_str[dev_params.proto]);
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
                 hid_proto_name_str[dev_params.proto]);
        break;
    default:
        ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
                 hid_proto_name_str[dev_params.proto]);
        break;
    }
}

/**
 * @brief HID设备连接后的一次性任务处理函数
 *
 * @param[in] param 指向 hid_host_related_info_t 结构体的指针
 */
static void hid_host_connected_task(void *param)
{
    hid_host_related_info_t *dev = (hid_host_related_info_t *)param;
    esp_err_t err;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(dev->handle, &dev_params));

    const hid_host_device_config_t dev_config = {
        .callback = hid_host_interface_callback,
        .callback_arg = NULL
    };

    err = hid_host_device_open(dev->handle, &dev_config);
    if (err == ESP_OK) {
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            err = hid_class_request_set_protocol(dev->handle, HID_REPORT_PROTOCOL_BOOT);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "hid_class_request_set_protocol failed: 0x%x", err);
            }
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                err = hid_class_request_set_idle(dev->handle, 0, 0);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "hid_class_request_set_idle failed: 0x%x", err);
                }
            }
        }
        err = hid_host_device_start(dev->handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "hid_host_device_start failed: 0x%x", err);
        } else {
            ESP_LOGI(TAG, "HID Device Start Successfully.");
        }
    } else {
        ESP_LOGE(TAG, "hid_host_device_open failed: 0x%x", err);
    }

    free(dev); // 释放参数内存
    vTaskDelete(NULL); // 任务自删除
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
static void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
                 hid_proto_name_str[dev_params.proto]);

        hid_host_related_info_t *dev = calloc(sizeof(hid_host_related_info_t), 1);
        if (dev) {
            dev->handle = hid_device_handle;
            dev->event = event;
            dev->arg = arg;
            xTaskCreate(hid_host_connected_task, "hid_host_conn_task", 3072, dev, 5, NULL);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief 初始化 HID Host 驱动
 */
void hid_host_init(void)
{
#if 0   // 外部初始化USB任务
    /*
    * Create usb_lib_task to:
    * - initialize USB Host library
    * - Handle USB Host events while APP pin in in HIGH state
    */
    task_created = xTaskCreatePinnedToCore(usb_lib_task,
                                           "usb_events",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           2, NULL, 0);
    assert(task_created == pdTRUE);

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, 1000);
#endif

    /*初始化键盘输入队列*/
    if (!key_input_queue) {
        key_input_queue = xQueueCreate(4, sizeof(key_input_msg_t));
    }

    /*
    * HID host driver configuration
    * - create background task for handling low level event inside the HID driver
    * - provide the device callback to get new HID Device connection event
    */
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_event,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
    ESP_LOGI(TAG, "HID Driver install successfully.");
}

/**
 * @brief 清除 HID Host 驱动
 */
void hid_host_deinit(void)
{
    ESP_LOGI(TAG, "HID Driver uninstall.");
    ESP_ERROR_CHECK(hid_host_uninstall());
}

/**
 * @brief 从 key_input_queue 获取一条输入行，拷贝到目标缓冲区
 *
 * @param[out] dst      目标存储地址
 * @param[in]  dst_size 目标存储区最大长度
 * @param[out] out_len  实际拷贝的长度
 * @param[in]  timeout_ticks 等待超时时间（单位：tick）
 * @return true 获取成功，false 队列无数据或出错
 */
bool hid_receive_key_input_from_queue(char *dst, size_t dst_size, size_t *out_len, TickType_t timeout_ticks)
{
    if (!dst || dst_size == 0 || !out_len) {
        return false;
    }
    key_input_msg_t msg;
    if (key_input_queue && xQueueReceive(key_input_queue, &msg, timeout_ticks) == pdPASS) {
        size_t copy_len = (msg.len < dst_size - 1) ? msg.len : (dst_size - 1);
        memcpy(dst, msg.data, copy_len);
        dst[copy_len] = '\0';
        *out_len = copy_len;
        free(msg.data);
        return true;
    }
    *out_len = 0;
    return false;
}

/**
 * @brief 获取当前键盘输入缓存及其长度
 *
 * 功能说明：
 * 1. 返回当前键盘输入缓存的指针和有效长度，便于外部模块直接读取当前输入内容。
 * 2. 仅返回指针和长度，不做任何修改操作。
 *
 * @param[out] buf  返回输入缓存的指针地址（const char **，指向只读字符串）
 * @param[out] len  返回输入缓存的有效长度（size_t *）
 */
void hid_receive_key_input_real(const char **buf, size_t *len)
{
    if (buf) *buf = key_input_buffer;
    if (len) *len = key_input_len;
}

