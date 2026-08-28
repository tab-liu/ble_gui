#include "ble_adv.h"

#include "app_ble.h"
#include "drv_nimble.h"
#include "dev_discovery.h"
#include "can_protocol.h"

#include <esp_log.h>
#include "host/ble_hs.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md5.h"

#define TAG "[ble_adv]"

// Flags
#define BLE_AD_TYPE_FLAG                    0x01
// Manufacturer Specific Data
#define BLE_AD_MFG_TYPE                     0xFF

// 产品型号ID 《产品型号(ASCII码)的代号分配表格》
#define DEVICE_TYPE                         SN_TYPE_SELF

#define MFG_HEADER_PREFIX_CONNECTED         0x82F0
#define MFG_HEADER_PREFIX_DISCONNECT        0x02F0

#define MFG_RECORD_TYPE                     0x01

/* 广播周期定义 */
#define BLE_ADV_REALTIME_INTERVAL           1000   /* 实时数据广播间隔(秒) - 快速更新 */
#define BLE_ADV_SETTINGS_INTERVAL           2000  /* 设置数据广播间隔(秒) - 慢速更新 */
#define BLE_ADV_DEFAULT_INTERVAL            300  /* 默认数据广播间隔(秒) 待调试，太慢无法连接 */

/* record_type 定义 */
#define BLE_RECORD_TYPE_REALTIME            0x80    /* LCD实时数据 */
#define BLE_RECORD_TYPE_SETTINGS            0x81    /* LCD设置显示 */

/* 广播数据类型 */
#define BLE_ADV_TYPE_DEFAULT                0   /* 默认广播数据 */
#define BLE_ADV_TYPE_REALTIME               1   /* 实时数据 0x80 */
#define BLE_ADV_TYPE_SETTINGS               2   /* 设置数据 0x81 */

/* 短整型数据高低交换 */
#define SWAP16(N)       ((((uint16_t)(N) & 0x00ff) << 8) | \
                         (((uint16_t)(N) & 0xff00) >> 8))

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

/**
 * @brief 生成加密的随机数
 * @return 加密的随机数
 */
uint16_t ble_adv_nonce_get(void)
{
    uint16_t rand_value = esp_random() & 0xFFFF;

    // ESP_LOGW(TAG, "rand value:%04x", rand_value);

    return rand_value;
}

/**
 * @brief 使用AES CTR模式进行加解密(加密和解密均为此函数)
 * @details AES CTR模式: 加密使用的元素: 秘钥+随机数. 
 *          使用秘钥对随机数做加密，然后使用加密后的随机数密文与被加密数据异或, 得到最终的密文.
 *          所以，这里需要保证每次的随机数不同, 才能保证加密数据的安全.
 */
uint8_t ble_aes_ctr_encrypt(uint8_t *nonce, uint8_t *plaintext, uint8_t len, uint8_t *chipertext)
{
    mbedtls_aes_context ctx;
    uint8_t stream_block[16] = {0};
    uint8_t nonce_iv[16] = {0};
    size_t nc_off = 0;

    mbedtls_aes_init(&ctx);
    //使用16字节，128bits秘钥
    mbedtls_aes_setkey_enc(&ctx, SetData.dev_info_t.bles_adv_key, 128);

    // Encrypt
    memcpy(nonce_iv, nonce, 16);
    mbedtls_aes_crypt_ctr(&ctx, len, &nc_off, nonce_iv, stream_block, plaintext, chipertext);

    mbedtls_aes_free(&ctx);

    return len;
}

/**
 * @brief 准备LCD实时数据
 * @param[out] realtime_data 实时数据结构体指针
 */
static void ble_prepare_realtime_data(ble_lcd_realtime_t *realtime_data)
{
    if (realtime_data == NULL) return;

    // 清空数据
    memset(realtime_data, 0, sizeof(ble_lcd_realtime_t));
    // int32_t grid_input_power = 0;
    // int32_t grid_output_power = 0;

    realtime_data->soc = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.soc&0xFF;                    /* 电量 */

    realtime_data->charge_time = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.chg_full_time;  // 充电中显示预计充满时间其他显示放空时间
    realtime_data->energy_line = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.line_event.all;        /* 能量线显示 */
    // if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.GridAllTotalPower > 0){
    //     grid_output_power = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.GridAllTotalPower;
    // } else {
    //     grid_input_power = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.GridAllTotalPower;
    // }

    realtime_data->input_power = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.PVAllTotalPower
                               + Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.GridAllTotalPower;
    realtime_data->output_power = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.ACLoadAllTotalPower
                                + Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.DCLoadAllTotalPower;
    
    // 状态位设置
    if(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.fault[0] | 
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.fault[1] | 
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.fault[2] | 
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.fault[3] | 
        Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.fault[4]) {
        realtime_data->status.alarm_status = 1;     /* 有告警 */
    } else {
        realtime_data->status.alarm_status = 0;     /* 无告警 */
    }
    realtime_data->status.charge_status = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg00100_AppPage1.chg_status;    /* 充电中 */
    realtime_data->status.disaster_status = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg12000_IOT_set.thunder_ctrl.thunder_state;     /* 风暴预警 */
    
    ESP_LOGD(TAG, "Prepared realtime data: SOC=%d%%, Input=%dW, Output=%dW", 
             realtime_data->soc, realtime_data->input_power, realtime_data->output_power);
}

/**
 * @brief 准备LCD设置显示数据
 * @param[out] settings_data 设置数据结构体指针
 */
static void ble_prepare_settings_data(ble_lcd_set_show_t *settings_data)
{
    if (settings_data == NULL) return;
    uint8_t ble_lcd_active_time = 0;
    uint8_t ctrl_lcd_active_time = 0;  
    
    // 清空数据
    memset(settings_data, 0, sizeof(ble_lcd_set_show_t));

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
    
    // uint64_t timestamp = time(NULL);
    settings_data->timestamp = fake_utc_timestamp & 0xFFFFFFFF;  /* 当前时间戳 */
    settings_data->inv_work_mode = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.work_mode;           /* 工作模式 */
    settings_data->money_save = g_self_data.mod_reg11000_IOT_info.saveMoneyNums; /* 省钱参数 */
    settings_data->power_off_count = g_self_data.mod_reg11000_IOT_info.powerOff_Nums & 0xFF;        /* 断电次数0~99 */

    // 屏幕显示设置
    ble_lcd_active_time = g_self_data.mod_reg12000_IOT_set.LCD_Mode.ble_lcd_active_time; //设备显示的时间背光亮的时间
    ctrl_lcd_active_time = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.ctrl_lcd_active_time;
    if ((ble_lcd_active_time == 0) || (ble_lcd_active_time == 6)) {
        settings_data->display_config.screen_sleep_time = ctrl_lcd_active_time;
    } else {
        settings_data->display_config.screen_sleep_time = ble_lcd_active_time;
    }
    settings_data->display_config.temp_unit = g_self_data.mod_reg12000_IOT_set.LCD_Mode.temperature_unit;          /* 摄氏度 */
    
    // 工作模式设置
    settings_data->work_mode.ac_eco_enable = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.ctrl_ac_eco;           /* AC ECO开启普通 */
    settings_data->work_mode.dc_eco_enable = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.ctrl_dc_eco;           /* DC ECO关闭 */
    settings_data->work_mode.charge_mode = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.ctrl_chg_mode;             /* 快充模式 */
    if (reals.online_ACHUB_num > 0) { // 接入ACHUB时，大力士模式关闭
        settings_data->work_mode.high_power_mode = 0;         /* 大力士模式关闭 */
    } else {
        settings_data->work_mode.high_power_mode = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02000_Inv_base_set.ctrl_super_power;         /* 大力士模式关闭 */
    }
    settings_data->work_mode.output_memory = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg02200_Inv_advance_set.ctrl_save_power_state;           /* 输出记忆开启 */
    
    ESP_LOGD(TAG, "Prepared settings data: work_mode=%d, timestamp=%lu", 
             settings_data->inv_work_mode, settings_data->timestamp);
}

/**
 * @brief BLE自定义广播数据帧打包
 * @param[out] adv_buf 组帧后的广播数据容器
 * @param[in] record_type 自定义数据有效负载类型
 * @param[in] mfg_data 自定义数据有效负载
 * @return >0 - 返回组帧数据的长度; <= 0 - 组帧错误
 */
int8_t ble_adv_frame_pack(uint8_t *adv_buf, uint8_t record_type, const ble_mfg_data_t *mfg_data)
{
    ble_adv_container_t ble_adv_container = BLE_ADV_CONTAINER_INIT();

    if (NULL == adv_buf || NULL == mfg_data)
    {
        ESP_LOGE(TAG, "pack ble adv frame param error");
        return -1;
    }

    ble_adv_container.flags.value = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    //除flags部分的长度, 包括自定义有效负载的数据长度
    ble_adv_container.mfg_len = sizeof(ble_adv_container) - 4;  //去掉flags + mfg len, 总共4个字节
    if (ble_encrypt_info.flag.bit.ble_connect)
    {
        ble_adv_container.header.prefix = MFG_HEADER_PREFIX_CONNECTED;
    }
    else
    {
        ble_adv_container.header.prefix = MFG_HEADER_PREFIX_DISCONNECT;
    }

    ble_adv_container.header.record_type = record_type;
    // 加密随机数
    uint16_t nonce = ble_adv_nonce_get();
    ble_adv_container.header.nonce = SWAP16(nonce);
    // 加密秘钥第一个字节，用于对方检测秘钥是否正确
    ble_adv_container.header.key_0 = SetData.dev_info_t.bles_adv_key[0];

    // 复制传入的自定义数据
    memcpy(&ble_adv_container.data, mfg_data, sizeof(ble_mfg_data_t));

    memcpy(adv_buf, &ble_adv_container, sizeof(ble_adv_container));

    // ESP_LOGI(TAG, "ble_adv_container data len:%d", sizeof(ble_adv_container.data));
    // ESP_LOG_BUFFER_HEX(TAG, (uint8_t *)&ble_adv_container.data, sizeof(ble_adv_container.data));

    // 随机数使用 uint16_t，后面字节填充为0
    uint8_t nonce_iv[16] = {0};
    nonce_iv[0] = (nonce & 0xFF00) >> 8;
    nonce_iv[1] = nonce & 0xFF;
    ble_aes_ctr_encrypt(nonce_iv, (uint8_t *)&ble_adv_container.data,
                        (uint8_t)sizeof(ble_mfg_data_t), &adv_buf[BLE_ADV_HEADER_LEN]);

    // ESP_LOGW(TAG, "ble_adv_container data len:%d", sizeof(ble_adv_container));
    // ESP_LOG_BUFFER_HEX(TAG, adv_buf, sizeof(ble_adv_container));

    return (sizeof(ble_adv_container));
}

/**
 * @brief BLE自定义广播数据帧更新
 * @note 根据当前的广播类型，更新相应的广播数据
 */
void ble_adv_update(void)
{
    static uint8_t adv_type = BLE_ADV_TYPE_DEFAULT;
    static int64_t last_realtime_time = 0;
    static int64_t last_settings_time = 0;
    static int64_t last_default_time = 0;

    uint8_t ret = 0;
    uint8_t adv_buffer[BLE_ADV_TOTAL_LEN + 1] = {0};
    int64_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS; // ms
    ble_mfg_data_t mfg_data = {0};
    const uint8_t phone_connected = drv_ble_get_ConnectEvent();

    // 蓝牙禁用或禁用广播时, 如果已经是默认内容（adv_type == 0），则不再进行广播
    if ((1 != SetData.dev_info_t.on_off.bit.ble_enable || 1 != SetData.dev_info_t.ble_protocol.adv_en)
        && (1 != SetData.dev_info_t.ble_protocol.lcd_adv_en)
        && adv_type == BLE_ADV_TYPE_DEFAULT)
    {
        ESP_LOGE(TAG, "SetData ble_enable:%d, SetData adv_en:%d, adv_type:%d",
            SetData.dev_info_t.on_off.bit.ble_enable, SetData.dev_info_t.ble_protocol.adv_en, adv_type);
        return;
    }
    
    // 根据时间间隔决定广播内容
    if (current_time - last_realtime_time >= BLE_ADV_REALTIME_INTERVAL)
    {
        // 广播实时数据 - 快速更新（每3秒）
            ble_prepare_realtime_data(&mfg_data.lcd_realtime);
            ret = ble_adv_frame_pack(adv_buffer, BLE_RECORD_TYPE_REALTIME, &mfg_data); 
        if (ret > 0)
        {
            adv_type = BLE_ADV_TYPE_REALTIME;
            last_realtime_time = current_time + esp_random()%500; // 追加随机数延时避免冲突
            ESP_LOGI(TAG, "BLE adv: realtime data (0x80)");
        }
    }
    else if (current_time - last_settings_time >= BLE_ADV_SETTINGS_INTERVAL)
    {
        // 广播设置数据 - 慢速更新（每30秒）
            ble_prepare_settings_data(&mfg_data.lcd_settings);
            ret = ble_adv_frame_pack(adv_buffer, BLE_RECORD_TYPE_SETTINGS, &mfg_data);
        if (ret > 0)
        {
            adv_type = BLE_ADV_TYPE_SETTINGS;
            last_settings_time = current_time + esp_random()%500; // 追加随机数延时避免冲突
            ESP_LOGI(TAG, "BLE adv: settings data (0x81)");
            }
    }
    else if (!phone_connected
             && (current_time - last_default_time >= BLE_ADV_DEFAULT_INTERVAL))
    {
        // 广播默认数据 - 供APP扫描识别（手机已连接时不广播）
            ret = ble_adv_mfg_data_default(adv_buffer);
        if (ret > 0)
        {
            adv_type = BLE_ADV_TYPE_DEFAULT;
            last_default_time = current_time + esp_random()%500; // 追加随机数延时避免冲突
            ESP_LOGI(TAG, "BLE adv: default data");
            }
    }
    else
    {
        // 如果还没有到任何广播间隔，保持上一次的广播数据
            return;
    }

    // 只有成功打包数据才更新广播
    if (ret > 0)
    {
        set_ble_adv_data(adv_buffer, ret);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, adv_buffer, ret, ESP_LOG_DEBUG);
    }
}

void ble_adv_key_update(uint8_t flag)
{
    #define BLE_ADV_KEY_LEN 10
    
    uint8_t out_md5[16] = {0};

    // 如果flag不为1或2，直接退出
    if (flag != 1 && flag != 2) {
        return;
    }

    if (flag == 2) {
        // 恢复为默认key
        memcpy(out_md5, BLE_ADV_KEY_DEFAULT, sizeof(out_md5));
    } else if (flag == 1) {
        // 随机生成key
        uint32_t random = 0;
        mbedtls_md5_context md5_ctx;
        uint8_t encrypt[BLE_ADV_KEY_LEN] = {0};

        for (int i = 0; i < BLE_ADV_KEY_LEN; i++) {
            encrypt[i] = (uint8_t)esp_random();
        }

        random = esp_random(); // 产生一个32位随机数

        mbedtls_md5_init(&md5_ctx);
        mbedtls_md5_starts(&md5_ctx);

        mbedtls_md5_update(&md5_ctx, encrypt, BLE_ADV_KEY_LEN);
        mbedtls_md5_finish(&md5_ctx, out_md5);
        mbedtls_md5_free(&md5_ctx);

        // 确保第一个字节与默认Key的第一个字节不同, 降低第一个字节相同错误的概率
        if (out_md5[0] == 'B') {
            out_md5[0] = 'A';
        }
    }

    memcpy(Inv_WR.mod_reg13600_open.bles_adv_key, out_md5, sizeof(Inv_WR.mod_reg13600_open.bles_adv_key));
    memcpy(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.bles_adv_key, out_md5,
                    sizeof(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg13600_open.bles_adv_key));
    memcpy(g_self_data.mod_reg13600_open.bles_adv_key, out_md5, sizeof(g_self_data.mod_reg13600_open.bles_adv_key));
    memcpy(SetData.dev_info_t.bles_adv_key, out_md5, sizeof(SetData.dev_info_t.bles_adv_key));
}
