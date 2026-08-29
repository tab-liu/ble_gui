/**
 ******************************************************************************
 * @file      bt_adv.c
 * @version   1.0
 * @author    IOT Team
 * @date      2024
 * @brief     BLE自定义广播协议实现文件
 * @details   实现基于AES-CTR加密的BLE广播协议
 *            支持实时数据、设置数据的安全传输
 *            为储能系统提供无线数据广播服务
 * @par       主要功能
 *            - 实时数据广播：SOC、功率、状态等系统实时信息
 *            - 设置数据广播：工作模式、显示配置等系统设置
 *            - 加密传输：AES-CTR模式确保数据安全
 *            - 密钥管理：动态密钥生成和管理
 *            - 智能调度：基于优先级的广播间隔控制
 * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
 ******************************************************************************
 */

#include "bt_adv.h"

#include "filesystem.h"
#include "app_bt.h"
#include "drv_nimble.h"
#include "can_protocol.h"

#include "mbedtls/md5.h"
#include "esp_random.h"
#include <esp_log.h>
#include "host/ble_hs.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "comm_define.h"
#include "ble_dev.h"
#include "modbus_data.h"
#include "parameter.h"
#include <stdbool.h>

#define TAG "[ble_adv]"


/* ================================ 厂商标识 ================================ */
/** 
 * @brief 公司向SIG申请的厂商ID
 * @details SHENZHEN POWEROAK NEWENER CO., LTD
 * @see https://www.bluetooth.com/specifications/assigned-numbers/
 */
#define COMPANY_ID_UUID         0x0F06

/* ================================ 广播包长度定义 ================================ */
#define BLE_ADV_TOTAL_LEN       BLE_HCI_MAX_ADV_DATA_LEN    /**< 广播包总长度(31字节) */
#define BLE_ADV_HEADER_LEN      15                          /**< 广播包头部长度 */
#define MFG_PAYLOAD_LEN         (BLE_ADV_TOTAL_LEN-BLE_ADV_HEADER_LEN) /**< 有效负载长度(16字节) */
#define BLE_MFG_HEADER_LEN      11                          /**< 自定义厂商数据头部长度 */

/* ================================ 常量定义 ================================ */

/** BLE广播标准字段类型 */
#define BLE_AD_TYPE_FLAG                    0x01    /**< BLE广播Flag字段类型标识 */
#define BLE_AD_MFG_TYPE                     0xFF    /**< 厂商特定数据类型标识 */

/** 设备标识 */
#define DEVICE_TYPE                         0  /**< 产品型号ID，来自《产品型号(ASCII码)的代号分配表格》 */

/** 广播头部前缀定义(bit15) */
#define MFG_HEADER_PREFIX_CONNECTED         0x8000  /**< BLE已连接状态前缀，小端格式 */
#define MFG_HEADER_PREFIX_DISCONNECT        0x0000  /**< BLE未连接状态前缀，小端格式 */

/** 广播头部前缀定义(bit14/13) */
#define MFG_HEADER_PREFIX_ENCRYPT           0x4000  /**< BLE加密状态前缀，小端格式 */
#define MFG_HEADER_PREFIX_DISENCRYPT        0x2000  /**< BLE非加密状态前缀，小端格式 */

#define MFG_RECORD_TYPE                     0x0B    /**< 厂商记录类型标识 - 逆变器*/

#define BLE_ADV_KEY_DEFAULT                 "BluettiBluetooth"
#define BLE_ADV_KEY_LEN                     10      // 随机种子长度

/* ================================ 数据类型标识 ================================ */
/** 
 * @brief 记录类型定义 - 用于标识广播数据的具体格式
 */
#define BLE_RECORD_TYPE_REALTIME            0x80    /**< LCD实时数据记录类型 */
#define BLE_RECORD_TYPE_SETTINGS            0x81    /**< LCD设置显示记录类型 */
#define BLE_RECORD_TYPE_LCD_BIND            0x85    /**< LCD绑定设置命令类型 */

/** 
 * @brief 内部广播类型枚举 - 用于状态机控制
 */
#define BLE_ADV_TYPE_DEFAULT                0       /**< 默认广播数据(设备发现) */
#define BLE_ADV_TYPE_SELFDATA               1       /**< 自身数据广播 */
#define BLE_ADV_TYPE_LCD_DATA               2       /**< LCD数据广播 */

/* ================================ 工具宏定义 ================================ */
/** 
 * @brief 16位数据字节序转换宏
 * @details 将小端格式转换为大端格式或相反
 */
#define SWAP16(N)       ((((uint16_t)(N) & 0x00ff) << 8) | \
                         (((uint16_t)(N) & 0xff00) >> 8))

/** 
 * @brief BLE广播容器初始化宏
 * @details 初始化BLE广播数据包的固定字段
 */
#define BLE_ADV_CONTAINER_INIT() {                  \
    .flags = {                                      \
        .len = 0x02,                                \
        .type = BLE_AD_TYPE_FLAG,                   \
    },                                              \
    .mfg_type = BLE_AD_MFG_TYPE,                    \
    .company_id = (COMPANY_ID_UUID),                \
    .header = {                                     \
        .device_id = (DEVICE_TYPE),                 \
    }                                               \
}

/* =============================== 文件内变量定义 ================================ */

// 蓝牙广播状态机
static adv_state_machine_t ble_adv_machine = {
    .current_state = ADV_STATE_IDLE,
    .rotation_counter = {0},
};

// 当前触发式发送广播内容
static ble_adv_trigger_t ble_adv_trigger = {0};

/* ================================ 私有函数实现 ================================ */

/**
 * @brief 生成AES加密用的随机数
 * @return 16位随机数值
 * @details 使用ESP32硬件随机数生成器产生加密随机数
 *          每次广播都会生成新的随机数，确保CTR模式的安全性
 *          随机数用于AES-CTR模式的初始向量(IV)
 */
uint16_t ble_adv_nonce_get(void)
{
    uint16_t rand_value = esp_random() & 0xFFFF;

    // ESP_LOGW(TAG, "Generated nonce: 0x%04x", rand_value);

    return rand_value;
}

/**
 * @brief AES-CTR模式加密函数
 * @param[in] nonce 16字节初始向量(IV)，前2字节为随机数，后14字节为0
 * @param[in] plaintext 明文数据指针
 * @param[in] len 数据长度
 * @param[out] chipertext 密文输出缓冲区
 * @return 加密后的数据长度
 * @details AES-CTR模式加密原理：
 *          1. 使用128位密钥对计数器(随机数)进行AES加密
 *          2. 将加密结果与明文进行异或运算得到密文
 *          3. CTR模式的特点：加密和解密使用相同的函数
 *          4. 安全性依赖于每次使用不同的随机数(nonce)
 * @note 密钥来源：SetData.dev_info_t.bles_adv_key (16字节)
 *       每次广播必须使用新的随机数以确保安全性
 */
uint8_t ble_aes_ctr_encrypt(uint8_t *nonce, uint8_t *plaintext, uint8_t len, uint8_t *chipertext)
{
    mbedtls_aes_context ctx;
    uint8_t stream_block[16] = {0};     // AES加密流缓冲区
    uint8_t nonce_iv[16] = {0};         // 初始向量副本
    size_t nc_off = 0;                  // 块内偏移量

    // 初始化AES上下文
    mbedtls_aes_init(&ctx);
    
    // 设置128位加密密钥
    mbedtls_aes_setkey_enc(&ctx, IotSetData.dev_info_t.bles_adv_key, 128);

    // 复制初始向量(避免修改原始nonce)
    memcpy(nonce_iv, nonce, 16);
    
    // 执行AES-CTR模式加密
    mbedtls_aes_crypt_ctr(&ctx, len, &nc_off, nonce_iv, stream_block, plaintext, chipertext);

    /* 解密测试代码(调试用)
    nc_off = 0;
    memcpy(nonce_iv, nonce, 16);
    uint8_t *decryptedtest = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    mbedtls_aes_crypt_ctr(&ctx, len, &nc_off, nonce_iv, stream_block, chipertext, decryptedtest);
    ESP_LOGD(TAG, "Decrypted test data length: %d", len);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, decryptedtest, len, ESP_LOG_DEBUG);
    free(decryptedtest);
    */

    // 释放AES上下文资源
    mbedtls_aes_free(&ctx);

    return len;
}

/*
输入参数包括一个未知大小的整形数据、该数据是否存在符号、该数据占用几个字节，该数据绝对值的最大值，该数据越界的默认值。
功能是检查该数据绝对值是否符合要求，符合的话返回原数据，否则返回默认值
eg:
int16_t v = -12345;
int64_t res = check_int_value(&v, true, 2, 20000, 0); // 返回-12345
*/
int64_t check_int_value(const void *data, bool is_signed, size_t bytes, int64_t abs_max, int64_t default_val) {
    int64_t value = 0;

    // 按字节数和符号类型读取数据
    if (is_signed) {
        switch (bytes) {
            case 1: value = *(int8_t *)data; break;
            case 2: value = *(int16_t *)data; break;
            case 4: value = *(int32_t *)data; break;
            case 8: value = *(int64_t *)data; break;
            default: return default_val;
        }
    } else {
        switch (bytes) {
            case 1: value = *(uint8_t *)data; break;
            case 2: value = *(uint16_t *)data; break;
            case 4: value = *(uint32_t *)data; break;
            case 8: value = *(uint64_t *)data; break;
            default: return default_val;
        }
    }

    // 检查绝对值是否越界
    if (llabs(value) > abs_max) {
        return default_val;
    }
    return value;
}


/**
 * @brief BLE广播数据帧打包函数
 * @param[out] adv_buf 输出的广播数据缓冲区，至少31字节
 * @param[in] record_type 数据类型标识(0x80:实时数据, 0x81:设置数据)
 * @param[in] mfg_data 待打包的有效负载数据指针
 * @return >0: 返回打包后的数据长度; <=0: 打包失败
 * @details 完成BLE广播数据的完整打包流程：
 *          1. 初始化广播容器结构
 *          2. 设置BLE标准字段(Flags)
 *          3. 填充厂商数据头部信息
 *          4. 生成加密随机数
 *          5. AES-CTR加密有效负载
 *          6. 输出完整的31字节广播数据包
 * @note 数据包格式：
 *       Flag(3) + MFG_Len(1) + MFG_Type(1) + Company_ID(2) + Header(7) + Encrypted_Data(16)
 */
int8_t ble_adv_frame_pack(uint8_t *adv_buf, uint8_t record_type, const ble_mfg_data_t *mfg_data)
{
    ble_adv_container_t ble_adv_container = BLE_ADV_CONTAINER_INIT();

    // 参数有效性检查
    if (NULL == adv_buf || NULL == mfg_data) {
        ESP_LOGE(TAG, "BLE adv frame pack: invalid parameters");
        return -1;
    }

    // 设置BLE标准Flag字段
    ble_adv_container.flags.value = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // 计算厂商数据长度：总长度 - Flag字段(3字节) - 长度字段本身(1字节)
    ble_adv_container.mfg_len = sizeof(ble_adv_container) - 4;
    
    // 根据BLE连接状态设置前缀
    ble_adv_container.header.prefix.all = 0; // 先清零
    if (ble_encrypt_info.flag.bit.ble_connect) {
        ble_adv_container.header.prefix.connect = 1; // MFG_HEADER_PREFIX_CONNECTED;     // 已连接状态
    } else {
        ble_adv_container.header.prefix.connect = 0; // MFG_HEADER_PREFIX_DISCONNECT;    // 未连接状态
    }

	ble_adv_container.header.prefix.encrypt = 2; //MFG_HEADER_PREFIX_ENCRYPT

	// 更新设备类型
	ble_adv_container.header.device_id = SN_TYPE_ASCII_TO_NUM(dev_factory.dev_type);
	 
    // 设置数据类型和加密参数
    ble_adv_container.header.record_type = record_type;
    
    // 生成新的加密随机数并转换为小端格式
    uint16_t nonce = ble_adv_nonce_get();
    ble_adv_container.header.nonce = SWAP16(nonce);
    
    // 设置密钥校验字节(密钥的第一个字节)
    ble_adv_container.header.key_0 = IotSetData.dev_info_t.bles_adv_key[0];

    // 复制有效负载数据到容器中
    memcpy(&ble_adv_container.data, mfg_data, sizeof(ble_mfg_data_t));

    // 先复制整个数据包到输出缓冲区
    memcpy(adv_buf, &ble_adv_container, sizeof(ble_adv_container));

    // 调试信息(可选)
    // ESP_LOGD(TAG, "Container data length: %d", sizeof(ble_adv_container.data));
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, (uint8_t *)&ble_adv_container.data, sizeof(ble_adv_container.data), ESP_LOG_DEBUG);

    // 准备AES-CTR加密的初始向量：前2字节为随机数，后14字节为0
    uint8_t nonce_iv[16] = {0};
    nonce_iv[0] = (nonce & 0xFF00) >> 8;    // 随机数高字节
    nonce_iv[1] = nonce & 0xFF;             // 随机数低字节
    
    // 对有效负载进行AES-CTR加密，直接加密到输出缓冲区的数据部分
    ble_aes_ctr_encrypt(nonce_iv, (uint8_t *)&ble_adv_container.data,
                        (uint8_t)sizeof(ble_mfg_data_t), &adv_buf[BLE_ADV_HEADER_LEN]);

    // 调试信息(可选)
    // ESP_LOGD(TAG, "Final adv packet length: %d", sizeof(ble_adv_container));
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, adv_buf, sizeof(ble_adv_container), ESP_LOG_DEBUG);

    return (sizeof(ble_adv_container));
}

/* ================================ 公共函数实现 ================================ */

/**
 * @brief 准备LCD实时数据
 * @param[out] realtime_data 实时数据结构体指针
 * @details 从Modbus寄存器中读取系统实时状态数据并组装成广播格式
 *          主要包括：SOC电量、功率信息、充电状态、告警状态等
 *          数据来源：top_modbus_rd.Inv[INV_MAX_NUM].mod_reg00100_AppPage1
 * @note 功率计算逻辑：
 *       - 输入功率 = PV功率 + 电网输入功率(GridAllTotalPower < 0时)
 *       - 输出功率 = AC负载 + DC负载 + 电网输出功率(GridAllTotalPower > 0时)
 */
static void ble_prepare_realtime_data(ble_lcd_realtime_t *realtime_data)
{
    if (realtime_data == NULL) {
        ESP_LOGE(TAG, "realtime_data pointer is NULL");
        return;
    }
    
    // 清空数据结构，确保未使用字段为0
    memset(realtime_data, 0, sizeof(ble_lcd_realtime_t));
    MOD_STRUCT_Inv *DeviceData = &top_modbus_rd.Inv[INV_MAX_NUM];
    
    // 电网功率分解：正值为输出到电网，负值为从电网输入
    int32_t grid_input_power = 0;   // 从电网输入的功率
    int32_t grid_output_power = 0;  // 输出到电网的功率

    // 基础数据读取
    realtime_data->soc = DeviceData->mod_reg00100_AppPage1.soc & 0xFF;
    realtime_data->charge_time = DeviceData->mod_reg00100_AppPage1.chg_full_time;
    realtime_data->energy_line = DeviceData->mod_reg00100_AppPage1.line_event.all;
    
#ifdef CONFIG_GRID_FEEDBACK_DISABLE    
    // 便携储只存在系统从电网输入功率（买电），且为正值
    grid_input_power = abs(DeviceData->mod_reg00100_AppPage1.GridAllTotalPower);
    grid_output_power = 0;
#else
    // 电网功率方向判断和分解
    if(DeviceData->mod_reg00100_AppPage1.GridAllTotalPower > 0) {
        // 正值：系统向电网输出功率(卖电)
        grid_output_power = DeviceData->mod_reg00100_AppPage1.GridAllTotalPower;
    } else {
        // 负值：系统从电网输入功率(买电)
        grid_input_power = -(DeviceData->mod_reg00100_AppPage1.GridAllTotalPower);
    }
#endif

    // 系统总输入功率计算：光伏发电 + 电网输入
    realtime_data->input_power = DeviceData->mod_reg00100_AppPage1.PVAllTotalPower + grid_input_power;
    
    // 系统总输出功率计算：AC负载 + DC负载 + 电网输出
    realtime_data->output_power = DeviceData->mod_reg00100_AppPage1.ACLoadAllTotalPower +
                                  DeviceData->mod_reg00100_AppPage1.DCLoadAllTotalPower + 
                                  grid_output_power;
    
    // 系统告警状态检测：检查所有故障寄存器
    if(DeviceData->mod_reg00100_AppPage1.fault[0] | 
       DeviceData->mod_reg00100_AppPage1.fault[1] | 
       DeviceData->mod_reg00100_AppPage1.fault[2] | 
       DeviceData->mod_reg00100_AppPage1.fault[3] | 
       DeviceData->mod_reg00100_AppPage1.fault[4]) {
        realtime_data->status.alarm_status = 1;     // 有故障
    } else if(DeviceData->mod_reg00100_AppPage1.alarm[0] | 
       DeviceData->mod_reg00100_AppPage1.alarm[1] | 
       DeviceData->mod_reg00100_AppPage1.alarm[2] | 
       DeviceData->mod_reg00100_AppPage1.alarm[3]) {
        realtime_data->status.alarm_status = 1;     // 有告警
    } else {
        realtime_data->status.alarm_status = 0;     // 无异常
    }
    
    // 充放电状态
    realtime_data->status.charge_status = DeviceData->mod_reg00100_AppPage1.chg_status;
    
    // 风暴预警状态
    realtime_data->status.disaster_status = DeviceData->mod_reg12000_IOT_set.thunder_ctrl.thunder_state;
    
    ESP_LOGI(TAG, "Realtime data prepared: SOC=%d%%, Input=%dW, Output=%dW, Status=0x%02x", 
             realtime_data->soc, realtime_data->input_power, realtime_data->output_power, realtime_data->status.status_byte);
}

/**
 * @brief 准备LCD设置显示数据  
 * @param[out] settings_data 设置数据结构体指针
 * @details 从系统配置和Modbus寄存器中读取设备设置信息并组装成广播格式
 *          主要包括：时间戳、工作模式、显示配置、功能开关等
 *          数据来源：多个Modbus寄存器和系统配置结构
 * @note 时间戳处理：
 *       - 读取UTC时间戳
 *       - 伪 UTC 时间戳 = 真实 UTC 时间戳 + 本地时区偏移秒数
 *       - 用于接收端的时间同步显示
 */
static void ble_prepare_settings_data(ble_lcd_set_show_t *settings_data)
{
    if (settings_data == NULL) {
        ESP_LOGE(TAG, "settings_data pointer is NULL");
        return;
    }
    
    // 清空数据结构，确保未使用字段为0
    memset(settings_data, 0, sizeof(ble_lcd_set_show_t));
    MOD_STRUCT_Inv *DeviceData = &top_modbus_rd.Inv[INV_MAX_NUM];
    
    // 1. 获取当前的真实 UTC 时间戳
    time_t true_utc_timestamp = time(NULL);
    
    // 2. 计算本地时区与 UTC 的偏移秒数
    struct tm local_tm;
    struct tm utc_tm;
    localtime_r(&true_utc_timestamp, &local_tm);
    gmtime_r(&true_utc_timestamp, &utc_tm);
    
    // mktime 会将输入的 struct tm 视为本地时间，并将其转换为 UTC 时间戳
    time_t time_from_local_tm = mktime(&local_tm); // 这实际等于 true_utc_timestamp
    time_t time_from_utc_tm = mktime(&utc_tm);     // 这里会产生一个比实际 UTC 时间戳小的值（差值为时区偏移）
    
    // 时区偏移秒数 = (本地时间表示的时间戳) - (UTC时间表示的时间戳)
    // 对于东八区，这个值是 +28800
    int32_t timezone_offset_seconds = difftime(time_from_local_tm, time_from_utc_tm);
    
    // 3. 计算出最终需要的“伪 UTC 时间戳”
    //    这个时间戳如果用 gmtime() 来解析，会得到当前的本地时间
    time_t fake_utc_timestamp;
    if(true_utc_timestamp > abs(timezone_offset_seconds)) {
        fake_utc_timestamp = true_utc_timestamp + timezone_offset_seconds;
    } else {
        // 避免时间戳下溢，设置为0
        fake_utc_timestamp = 0;
    }

    // 基础设置数据
    settings_data->timestamp = fake_utc_timestamp & 0xFFFFFFFF;  // 32位UTC时间戳
    settings_data->inv_work_mode = DeviceData->mod_reg02000_Inv_base_set.work_mode;
    settings_data->money_save = DeviceData->mod_reg11000_IOT_info.Save_Money_Nums;
    settings_data->power_off_count = DeviceData->mod_reg11000_IOT_info.Power_Off_Nums & 0xFF;

    // 屏幕显示配置
    if (( 0 != IotSetData.dev_info_t.lcd_mode.lcd_active_time ) 
        && ( 6 != IotSetData.dev_info_t.lcd_mode.lcd_active_time )) {
        settings_data->display_config.screen_sleep_time = IotSetData.dev_info_t.lcd_mode.lcd_active_time;
    } else {
        settings_data->display_config.screen_sleep_time = DeviceData->mod_reg02000_Inv_base_set.ctrl_lcd_active_time;
    }
    settings_data->display_config.temp_unit = DeviceData->mod_reg12000_IOT_set.LCD_Mode.temperature_unit;
    
    // 系统工作模式配置
    settings_data->work_mode.ac_eco_enable = DeviceData->mod_reg02000_Inv_base_set.ctrl_ac_eco;
    settings_data->work_mode.dc_eco_enable = DeviceData->mod_reg02000_Inv_base_set.ctrl_dc_eco;
    settings_data->work_mode.charge_mode = DeviceData->mod_reg02000_Inv_base_set.ctrl_chg_mode;
    settings_data->work_mode.high_power_mode = DeviceData->mod_reg02000_Inv_base_set.ctrl_super_power;
    settings_data->work_mode.output_memory = DeviceData->mod_reg02200_Inv_advance_set.ctrl_save_power_state;
    
    ESP_LOGI(TAG, "Settings data prepared: mode=%d, UTC_timestamp=%lu, charge_mode=%d, display=0x%02x, work=0x%04x", 
             settings_data->inv_work_mode, settings_data->timestamp, settings_data->work_mode.charge_mode,
             settings_data->display_config.screen_display_setting, settings_data->work_mode.work_mode_config);
}


/**
 * @brief 准备LCD绑定设置命令  
 * @param[out] bind_cmd 设置数据结构体指针
 */
static void ble_prepare_lcd_bind_cmd(ble_lcd_bind_cmd_t *bind_cmd)
{
    if (bind_cmd == NULL) {
        ESP_LOGE(TAG, "bind_cmd pointer is NULL");
        return;
    }
    
    // 清空数据结构，确保未使用字段为0
    memset(bind_cmd, 0, sizeof(ble_lcd_bind_cmd_t));

    // 屏幕绑定命令
    bind_cmd->cmd = (uint8_t)ble_adv_trigger.cmd;
    
    // 获取蓝牙MAC地址（小端）
    drv_ble_get_mac_addr(bind_cmd->ble_mac);
}

/**
 * @brief 准备逆变周期广播数据  
 * @param[out] inv_data 设置数据结构体指针
 * @details 从系统配置和Modbus寄存器中读取设备信息并组装成广播格式
 *          数据来源：多个Modbus寄存器和系统配置结构
 */
static void ble_prepare_inv_0x0B_data(ble_mfg_data_inv_0x0B_t *inv_data)
{
    if (inv_data == NULL) {
        ESP_LOGE(TAG, "inv_data pointer is NULL");
        return;
    }
    
    // 清空数据结构，确保未使用字段为0
    memset(inv_data, 0, sizeof(ble_mfg_data_inv_0x0B_t));
    MOD_STRUCT_Inv *DeviceData = &top_modbus_rd.Inv[INV_MAX_NUM];
    
    inv_data->alarm_code       = 0xFFFF;   // 暂不支持
    inv_data->battery_current  = (int16_t)check_int_value(&DeviceData->mod_reg00100_AppPage1.total_current, true, 2, INT16_MAX, 0x7FFF);
    inv_data->battery_voltage  = (uint16_t)check_int_value(&DeviceData->mod_reg00100_AppPage1.total_voltage, false, 2, 0x3FFF, 0x3FFF);
    inv_data->ac_port_index    = 0x3;      // 暂不支持
    inv_data->ac_port_power    = (int16_t)check_int_value(&DeviceData->mod_reg00100_AppPage1.GridAllTotalPower, true, 4, INT16_MAX, 0x7FFF);
    inv_data->ac_out_power     = (int16_t)check_int_value(&DeviceData->mod_reg00100_AppPage1.ACLoadAllTotalPower, true, 4, INT16_MAX, 0x7FFF);
    inv_data->pv_power         = (uint16_t)check_int_value(&DeviceData->mod_reg00100_AppPage1.PVAllTotalPower, false, 4, UINT16_MAX, 0xFFFF);
    inv_data->yield_today      = (1 == DeviceData->mod_reg03700_Inv_day_energy.PvTotalChargingEnergy.bit.valid) ? DeviceData->mod_reg03700_Inv_day_energy.PvTotalChargingEnergy.bit.day_energy : 0xFFFF;
    inv_data->soc              = (uint8_t)check_int_value(&DeviceData->mod_reg00100_AppPage1.soc, false, 2, UINT8_MAX, 0xFF);
}

/**
 * @brief 更新广播触发参数，同时记录开始时间
 * @param new_type 触发类型
 * @param new_cmd 触发命令
 */
void ble_adv_trigger_update(uint8_t new_type, uint8_t new_cmd)
{
    ble_adv_trigger.type = new_type;
    ble_adv_trigger.cmd  = new_cmd;
    ble_adv_trigger.start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGW(TAG, "BLE adv: ble_adv_trigger_update(0x%x), cmd(%d).", new_type, new_cmd);
}

/**
 * @brief BLE广播数据更新主函数
 * @details 智能广播调度器，根据时间间隔和优先级策略更新不同类型的广播数据
 *          实现三种广播模式的时分复用：
 *          1. 实时数据广播(0x80)：包含SOC、功率等实时信息
 *          2. 设置数据广播(0x81)：包含工作模式、配置等
 *          3. 逆变数据广播(0x0b)：包含逆变基本数据
 *          4. 默认数据广播：1s间隔，低优先级，用于设备发现和识别
 * @note 调用要求：
 *       - 需要在主循环或定时任务中周期性调用(500ms间隔)
 *       - 系统休眠时自动停止广播以节省功耗
 *       - BLE禁用时停止广播
 * @warning 该函数包含静态变量，非线程安全，请在单一任务中调用
 */
void ble_adv_update(void)
{
    // 静态变量保存广播状态和时间戳
    static uint8_t adv_type = BLE_ADV_TYPE_DEFAULT;     // 当前广播类型
    
    uint8_t ret = 0;
    uint8_t adv_buffer[BLE_ADV_TOTAL_LEN + 1] = {0};    // 广播数据缓冲区
    ble_mfg_data_t mfg_data = {0};                      // 有效负载数据容器
    static uint8_t restart_adv_cnt = 0;                 // 广播异常恢复计数

    // 等待任务延时间隔
	static uint32_t pre_time = 0;
	uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	if((now_time - pre_time) >= 500) {
        pre_time = now_time;
    } else {
        return;
    }

    // 蓝牙驱动异常，停止更新广播
    if(!drv_ble_get_init_flag()) {
        ble_encrypt_info.flag.bit.is_adv_on = 0;
        return; 
    }
    
    /* ================================ 广播调度逻辑 ================================ */

    // 状态机边界检查
    if ( ble_adv_machine.current_state == ADV_STATE_IDLE ) ble_adv_machine.current_state = ADV_STATE_SEND_1;

    // 状态机控制逻辑
    switch (ble_adv_machine.current_state) {
        case ADV_STATE_SEND_1:
            // 根据手机是否连接，发送不同的广播包
            if (ble_encrypt_info.flag.bit.ble_connect) {
                // 发送 BLE HMI fast (0x80) 广播
                ble_prepare_realtime_data(&mfg_data.lcd_realtime);
                ret = ble_adv_frame_pack(adv_buffer, BLE_RECORD_TYPE_REALTIME, &mfg_data);
                if (ret > 0) {
                    adv_type = BLE_ADV_TYPE_LCD_DATA;
                    ESP_LOGI(TAG, "BLE adv: realtime data (0x80) - SOC=%d%%, Input=%dW, Output=%dW", 
                             mfg_data.lcd_realtime.soc, mfg_data.lcd_realtime.input_power, mfg_data.lcd_realtime.output_power);
                }
            } else {
                // 广播默认识别数据，供APP扫描发现设备
                ret = ble_adv_mfg_data_default(adv_buffer);
                if (ret > 0) {
                    adv_type = BLE_ADV_TYPE_DEFAULT;
                    ESP_LOGI(TAG, "BLE adv: default identification data");
                }
            }
            break;

        case ADV_STATE_SEND_2:
            // 发送 EMS DC slave  广播
            break;

        case ADV_STATE_SEND_3:
            // 发送 EMS DC master  广播
            break;

        case ADV_STATE_SEND_4:
            // 轮替计数器边界检查
            if ( ble_adv_machine.rotation_counter[ADV_STATE_SEND_4] >= 2 ) ble_adv_machine.rotation_counter[ADV_STATE_SEND_4] = 0;

            // 根据轮替计数器发送不同的广播
            switch ( ble_adv_machine.rotation_counter[ADV_STATE_SEND_4] )
            {
                case 0 :
                    // 发送 EMS DC slave  广播
                    break;
                case 1 :
                    // Base DEV : 准备INV数据
                    ble_prepare_inv_0x0B_data(&mfg_data.inv_data);
                    ret = ble_adv_frame_pack(adv_buffer, MFG_RECORD_TYPE, &mfg_data);
                    if (ret > 0) {
                        adv_type = BLE_ADV_TYPE_SELFDATA;
                        ESP_LOGI(TAG, "BLE adv: inv data (0x0B)");
                    }
                    break;
                default:
                    break;
            }

            // 轮替计数器状态更新
            ble_adv_machine.rotation_counter[ADV_STATE_SEND_4]++;
            break;

        case ADV_STATE_SEND_5:
            // 根据轮替计数器发送 0xC9,SN / HMI fast / HMI slow
            // 轮替计数器边界检查
            if ( ble_adv_machine.rotation_counter[ADV_STATE_SEND_5] >= 3 ) ble_adv_machine.rotation_counter[ADV_STATE_SEND_5] = 0;
            
            // 根据轮替计数器发送不同的广播
            switch ( ble_adv_machine.rotation_counter[ADV_STATE_SEND_5] )
            {
                case 0 :
                    // 发送 0xC9  广播
                    break;
                case 1 :
                    // 发送 BLE HMI fast (0x80) 广播
                    ble_prepare_realtime_data(&mfg_data.lcd_realtime);
                    ret = ble_adv_frame_pack(adv_buffer, BLE_RECORD_TYPE_REALTIME, &mfg_data);
                    if (ret > 0) {
                        adv_type = BLE_ADV_TYPE_LCD_DATA;
                        ESP_LOGI(TAG, "BLE adv: realtime data (0x80) - SOC=%d%%, Input=%dW, Output=%dW", 
                                 mfg_data.lcd_realtime.soc, mfg_data.lcd_realtime.input_power, mfg_data.lcd_realtime.output_power);
                    }
                    break;
                case 2 :
                    // 发送 BLE HMI slow (0x81) 广播
                    ble_prepare_settings_data(&mfg_data.lcd_settings);
                    ret = ble_adv_frame_pack(adv_buffer, BLE_RECORD_TYPE_SETTINGS, &mfg_data);
                    if (ret > 0) {
                        adv_type = BLE_ADV_TYPE_LCD_DATA;
                        ESP_LOGI(TAG, "BLE adv: settings data (0x81) - mode=%d, UTC=%lu", 
                                 mfg_data.lcd_settings.inv_work_mode, mfg_data.lcd_settings.timestamp);
                    }
                    break;
                default:
                    break;
            }
            
            // 轮替计数器状态更新
            ble_adv_machine.rotation_counter[ADV_STATE_SEND_5]++;
            break;
            
        case ADV_STATE_SEND_6:
            // 检查触发式发送状态
            switch ( ble_adv_trigger.type )
            {
                case BLE_RECORD_TYPE_LCD_BIND :
                    // 超时检查（暂定1min）
                    if ( (now_time - ble_adv_trigger.start_time) <= 60000 ) {
                        // 发送 BLE_LCD绑定设置命令 (0x85)
                        ble_prepare_lcd_bind_cmd(&mfg_data.lcd_bind_cmd);
                        ret = ble_adv_frame_pack(adv_buffer, BLE_RECORD_TYPE_LCD_BIND, &mfg_data);
                        if (ret > 0) {
                            adv_type = BLE_ADV_TYPE_LCD_DATA;
                            ESP_LOGI(TAG, "BLE adv: lcd bind cmd (0x85)");
                        }
                    } else {
                        // 超时结束
                        ble_adv_trigger.type = 0;
                        ble_adv_trigger.start_time = 0;
                        ESP_LOGW(TAG, "BLE adv: lcd bind cmd (0x85) stop.");
                    }

                    break;
                default:
                    // 默认：发送 EMS DC slave  广播
                    break;
            }
            break;
            
        case ADV_STATE_IDLE:
            // 默认或周期结束后的空闲状态
            break;
    }    

    // 状态机状态更新
    ESP_LOGI(TAG, "BLE adv: current_state(%d), rotation_counter(%d)", ble_adv_machine.current_state, ble_adv_machine.rotation_counter[ble_adv_machine.current_state]);
    ble_adv_machine.current_state++;
    if (ret <= 0) return;

    // 获取当前广播进行状态（0：未广播，1：正在广播）
    bool adv_running = drv_ble_advertise_status();  
    ble_encrypt_info.flag.bit.is_adv_on = adv_running ? 1 : 0;

    // 系统状态检查：BLE禁用或广播禁用时的处理
    if ((!ble_encrypt_info.flag.bit.is_enable)  // 蓝牙关闭时，停止任何广播
        || ((ble_encrypt_info.flag.bit.ble_connect) && (adv_type == BLE_ADV_TYPE_DEFAULT))                     // 如果蓝牙已被连接则不广播默认数据
        || ((1 != IotSetData.dev_info_t.ble_protocol.adv_en) && (adv_type != BLE_ADV_TYPE_DEFAULT))            // 关闭广播数据使能时，仅维持设备发现帧的发送，其余全部禁止
#ifdef CONFIG_IOT_AUTO_LIGHT_SLEEP_ENABLE
        || ((1 == reals.IOT_Status_Flag.sBit.system_sleep_flag) && (adv_type != BLE_ADV_TYPE_DEFAULT))         // 低功耗休眠时，仅维持设备发现帧的发送，其余全部禁止
#endif  
        || ((1 != IotSetData.dev_info_t.ble_protocol.general_adv_en) && (adv_type == BLE_ADV_TYPE_SELFDATA))  // 关闭通用广播使能时，禁止通用广播帧（record type为0x00~0x7F）的发送
        || ((1 != IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en) && (adv_type == BLE_ADV_TYPE_LCD_DATA)) // 关闭LCD数据广播使能时，禁止LCD数据广播帧(record type为0x80/0x81)的发送
        )
    {
        bool adv_payload_disabled_when_connected =
            ble_encrypt_info.flag.bit.ble_connect &&
            ((1 != IotSetData.dev_info_t.ble_protocol.adv_en) ||
             ((1 != IotSetData.dev_info_t.ble_protocol.general_adv_en) && (1 != IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en)));
        
        if ((!ble_encrypt_info.flag.bit.is_enable) || adv_payload_disabled_when_connected) {
            if (adv_running) {
                // 不存在可以广播的数据，暂停广播
                drv_ble_advertise_pause();
                ESP_LOGW(TAG, "BLE adv: There is no data to broadcast. Broadcasting is paused.");
            }
        } else {
            ESP_LOGD(TAG, "BLE advertising disabled: enable=%d, adv_en=%d, general_adv_en=%d, lcd_data_adv_en=%d, type=%d", 
                    ble_encrypt_info.flag.bit.is_enable, IotSetData.dev_info_t.ble_protocol.adv_en, 
                    IotSetData.dev_info_t.ble_protocol.general_adv_en, IotSetData.dev_info_t.ble_protocol.lcd_data_adv_en, adv_type);
        }

        return;
    }

    /* ================================ 广播数据更新 ================================ */
    
    // 只有成功打包数据才更新BLE广播内容
    if (ret > 0) {
        // 广播已暂停
        if(!adv_running) {
            restart_adv_cnt++;
            // 恢复广播
            if ( restart_adv_cnt > 2 ) {
                if ( 0 == drv_ble_advertise_restart() ) {
                    ESP_LOGW(TAG, "BLE adv: Broadcasting is restart.");
                } else {
                    ESP_LOGE(TAG, "BLE adv: Broadcasting restart failed.");
                }
            }
        } else {
            restart_adv_cnt = 0;
        }
    
        // 调用底层BLE接口更新广播数据
        set_ble_adv_data(adv_buffer, ret);
        
        // 调试：输出完整的广播数据包(仅在调试模式下)
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, adv_buffer, ret, ESP_LOG_DEBUG);
    } else {
        ESP_LOGE(TAG, "Failed to pack BLE advertising data, type=%d", adv_type);
    }
}

/**
 * @brief BLE广播密钥管理函数
 * @param flag 密钥操作类型
 *        - 0: 不执行任何操作(直接返回)
 *        - 1: 生成新的随机密钥(使用MD5哈希)
 *        - 2: 恢复出厂默认密钥
 * @details 密钥管理策略：
 *          生成模式(flag=1)：
 *          1. 生成10字节随机数据作为种子
 *          2. 使用MD5哈希算法生成16字节密钥
 *          3. 确保新密钥第一字节与默认密钥不同(避免校验冲突)
 *          4. 同步更新所有相关数据结构中的密钥
 *          
 *          恢复模式(flag=2)：
 *          1. 直接使用预定义的默认密钥
 *          2. 同步更新所有相关数据结构
 * @note 密钥用途：
 *       - AES-CTR加密算法的128位密钥
 *       - 密钥第一字节用于接收端校验密钥正确性
 *       - 密钥存储在多个位置以保证数据一致性
 */
void ble_adv_key_update(uint8_t flag)
{    
    uint8_t out_md5[16] = {0};              // MD5输出缓冲区(16字节)

    // 参数有效性检查
    if (flag != 1 && flag != 2) {
        ESP_LOGE(TAG, "Invalid key update flag: %d, should be 1 or 2", flag);
        return;
    }

    if (flag == 2) {
        // 模式2：恢复出厂默认密钥
        memcpy(out_md5, BLE_ADV_KEY_DEFAULT, sizeof(out_md5));
        ESP_LOGI(TAG, "BLE key restored to factory default");
    } else if (flag == 1) {
        // 模式1：生成新的随机密钥
        uint32_t random = 0;
        mbedtls_md5_context md5_ctx;
        uint8_t *encrypt = NULL;

        // 分配随机种子缓冲区
        encrypt = iot_calloc(BLE_ADV_KEY_LEN);
        if (encrypt == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for key generation");
            return;
        }
        
        // 生成随机种子数据
        for (int i = 0; i < BLE_ADV_KEY_LEN; i++) {
            encrypt[i] = (uint8_t)esp_random();
        }

        random = esp_random(); // 额外的随机数(未使用，保留原逻辑)

        // 使用MD5哈希算法生成128位密钥
        mbedtls_md5_init(&md5_ctx);
        mbedtls_md5_starts(&md5_ctx);
        mbedtls_md5_update(&md5_ctx, encrypt, BLE_ADV_KEY_LEN);
        mbedtls_md5_finish(&md5_ctx, out_md5);
        mbedtls_md5_free(&md5_ctx);

        // 释放种子数据缓冲区
        free(encrypt);

        // 密钥冲突避免：确保第一个字节与默认密钥不同
        // 降低密钥校验时的误判概率
        if (out_md5[0] == 'B') {  // 'B'是BLE_ADV_KEY_DEFAULT的第一个字节
            out_md5[0] = 'A';     // 强制修改为不同值
        }
        
        ESP_LOGI(TAG, "BLE key generated: first_byte=0x%02x", out_md5[0]);
    }

    /* ================================ 密钥同步更新 ================================ */
    
    // 同步更新所有相关数据结构中的密钥，确保数据一致性
    
    // 更新写寄存器中的密钥
    memcpy(top_modbus_wr.Inv.mod_reg13600_open.bles_adv_key, out_md5, sizeof(top_modbus_wr.Inv.mod_reg13600_open.bles_adv_key));
	
	// 更新读寄存器中的密钥
    memcpy(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.bles_adv_key, out_md5, sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg13600_open.bles_adv_key));
	// 更新系统配置中的密钥
    memcpy(IotSetData.dev_info_t.bles_adv_key, out_md5, sizeof(IotSetData.dev_info_t.bles_adv_key));
	
	reals.SetDataWrFlag.sBit.ble_server_adv_key = 1;
	
    ESP_LOGW(TAG, "BLE advertising key updated successfully, operation=%s", 
             (flag == 1) ? "generate" : "restore");
}
