#include "drv_nimble.h"
#include "gatts_svr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "esp_bt.h"
#include "app_ble.h"
#include "iot_period_task.h"

#define TAG "[drv_nimble]"
// #define BLE_CONFIG_EXTENDED_ADV             1 // 拓展广播

#if BLE_CONFIG_EXTENDED_ADV
#define BLE_AD_TYPE_LE_DEV_ADDR             0x1b
#define BLE_ADDR_TYPE_PUBLIC                0x00

#define BLE_AD_TYPE_FLAG                    0x01
#define BLE_ADV_FLAG_GEN_DISC               (0x01 << 1)
#define BLE_ADV_FLAG_BREDR_NOT_SPT          (0x01 << 2)

#define BLE_AD_TYPE_16SRV_CMPL              0x03

#define BLE_AD_TYPE_NAME_CMPL               0x09

#define BLE_AD_MANUFACTURER_SPECIFIC_TYPE   0xFF

static uint8_t ext_adv_pattern[] = {
    /* AD数据结构：LE蓝牙设备地址 */
    0x08, BLE_AD_TYPE_LE_DEV_ADDR,
    BLE_ADDR_TYPE_PUBLIC,           // 地址是公共设备地址类型，蓝牙地址有2类，共4种类型
                                    // 公共设备地址
                                    // 随机设备地址：
                                    // - 静态设备地址
                                    // - 不可解析私密地址
                                    // - 可解析私密地址
    0x11,0x12,0x13,0x14,0x15,0x16,  // 6位MAC地址，由于苹果IOS系统不支持直接获取MAC，所以需要将MAC附着在广播包里面

    /* AD数据结构：可发现广播标记，通可发现,不支持BR/EDR(经典蓝牙) */
    0x02, BLE_AD_TYPE_FLAG, BLE_ADV_FLAG_GEN_DISC | BLE_ADV_FLAG_BREDR_NOT_SPT,

    /* AD数据结构：完整的16bit的服务UUID */
    0x03, BLE_AD_TYPE_16SRV_CMPL, GATTS_SVR_UUID&0xFF, GATTS_SVR_UUID>>8,

    /* AD数据结构：厂商指定数据 */
    0x0A, BLE_AD_MANUFACTURER_SPECIFIC_TYPE, 'B','L','B','L','U','E','T','T','I',

    // /* AD数据结构：设备名 */
    // 0x0C, BLE_AD_TYPE_NAME_CMPL, 's','m','a','r','t',' ','p','l','u','g','s',
};
#endif

#if BLE_CONFIG_ADDR
static uint8_t own_addr_type = BLE_OWN_ADDR_RANDOM;
#else
static uint8_t own_addr_type;                               // 蓝牙地址类型
#endif
static uint16_t ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;  // 蓝牙连接句柄
static uint8_t notify_flag1;                                // 特征1通知标志
static uint8_t notify_flag3;                                // 特征3通知标志
static app_rx_callback_t app_rx_cb;                         // 应用层接收数据回调函数
static nimble_event_cb_t nimble_event_cb;                   // ble事件回调函数
static ble_info_t ble_info = {
    .mtu_size = BLE_ATT_MTU_DFLT,                           // 蓝牙MTU默认大小
};
static bool drv_inited;
static ble_EventInfo_t ble_eventInfo ={
	.ble_event.bit.connectEvent=0,
	.ble_event.bit.res=0,
};

/* 用于跟踪 BLE 广播逻辑状态：物理 ADV_COMPLETE 时避免在逻辑停播后被再次拉起 */
static bool ble_adv_logically_on = false;

static int nimble_gap_event(struct ble_gap_event *event, void *arg);

/**
 * @brief 设置蓝牙扫描响应数据
 * - 该函数添加设备名到蓝牙扫描响应数据中
 * 
 * @return 成功返回0，失败返回BLE_ATT_ERR_XXX
 */
static int drv_ble_response(void)
{
    struct ble_hs_adv_fields fields = {0};
    int rc;

    /* 蓝牙扫描响应中加入设备名 */
    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting response data, rc=%d\n", rc);
    }
    return rc;
}

/**
 * @brief 组装蓝牙广播中的AD数据结构
 * 
 * @param ad_type AD类型
 * @param ad_len AD数据长度
 * @param ad_data AD数据
 * @param buf AD缓存
 * @param len AD累计长度
 */
static void put_ad(uint8_t ad_type, uint8_t ad_len, const void *ad_data, uint8_t *buf, uint8_t *len)
{
    if (*len + ad_len > BLE_HCI_MAX_ADV_DATA_LEN)
    {
        ESP_LOGE(TAG, "BLE adv data full");
        return;
    }

    buf[(*len)++] = ad_len + 1;
    buf[(*len)++] = ad_type;
    memcpy(&buf[*len], ad_data, ad_len);
    *len += ad_len;
}

uint8_t ble_adv_mfg_data_default(uint8_t *mfg_data)
{
    uint8_t ad_data[BLE_HCI_MAX_ADV_DATA_LEN];
    uint8_t ad_len = 0;
    int rc = 0;

    /* 蓝牙广播flags：Discoverability and BLE-only (BR/EDR unsupported) */
    uint8_t ad_flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    put_ad(BLE_HS_ADV_TYPE_FLAGS, 1, &ad_flags, ad_data, &ad_len);

    /* 蓝牙广播中加入服务UUID */
    uint16_t svc_uuid = GATTS_SVR_UUID;
    put_ad(BLE_HS_ADV_TYPE_COMP_UUIDS16, 2, &svc_uuid, ad_data, &ad_len);

    /* 蓝牙广播中加入mac地址 */
    #define BLE_AD_TYPE_LE_DEV_ADDR 0x1b
    #define BLE_ADDR_TYPE_PUBLIC    0x00
    uint8_t addr_val[7] = {0};
    addr_val[0] = BLE_ADDR_TYPE_PUBLIC;
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    rc |= ble_hs_id_copy_addr(own_addr_type, &addr_val[1], NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting mac addr data to advertisement data, rc=%d\n", rc);
        return -1;
    }
    put_ad(BLE_AD_TYPE_LE_DEV_ADDR, 7, addr_val, ad_data, &ad_len);

    /* 蓝牙广播中加入产商自定义数据 */
    #define MFG_DATA    "BLBLUETTF"
    put_ad(BLE_HS_ADV_TYPE_MFG_DATA, strlen(MFG_DATA), (uint8_t*)MFG_DATA, ad_data, &ad_len);

    memcpy(mfg_data, ad_data, ad_len);

    return ad_len;
}


int16_t set_ble_adv_data(uint8_t *ad_data, uint8_t ad_len)
{
    /* 设置蓝牙广播数据 */
    int rc = ble_gap_adv_set_data(ad_data, ad_len);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data, rc=%d\n", rc);
        return -1;
    }

    return 0;
}


#if BLE_CONFIG_EXTENDED_ADV
/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void ext_drv_ble_advertise(void)
{
    struct ble_gap_ext_adv_params params;
    struct os_mbuf *data;
    uint8_t instance = 0;
    int rc;

    /* First check if any instance is already active */
    if(ble_gap_ext_adv_active(instance)) {
        return;
    }

    /* use defaults for non-set params */
    memset (&params, 0, sizeof(params));

    /* enable connectable advertising */
    params.connectable = 1;

    /* advertise using random addr */
    params.own_addr_type = BLE_OWN_ADDR_PUBLIC;

    params.primary_phy = BLE_HCI_LE_PHY_1M;
    // params.secondary_phy = BLE_HCI_LE_PHY_2M;
    params.secondary_phy = BLE_HCI_LE_PHY_1M;

    params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;

    /* configure instance 0 */
    rc = ble_gap_ext_adv_configure(instance, &params, NULL,
                                   nimble_gap_event, NULL);
    assert (rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    rc |= ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting mac addr data to advertisement data, rc=%d\n", rc);
        return;
    }
    memcpy(&ext_adv_pattern[3], addr_val, sizeof(addr_val));

    /* get mbuf for advertise data */
    data = os_msys_get_pkthdr(sizeof(ext_adv_pattern), 0);
    assert(data);

    /* fill mbuf with advertise data */
    rc = os_mbuf_append(data, ext_adv_pattern, sizeof(ext_adv_pattern));
    assert(rc == 0);

    /* 设置扫描响应数据（当使用nrf connect蓝牙调试工具查看raw数据时，其中包含了广播与响应的数据） */
    rc = drv_ble_response();
    assert (rc == 0);

    rc = ble_gap_ext_adv_set_data(instance, data);
    assert (rc == 0);

    /* start advertising */
    rc = ble_gap_ext_adv_start(instance, 0, 0);
    assert (rc == 0);
}
#else

/**
 * @brief 蓝牙广播设置
 * @param[in] conn_mode BLE_GAP_CONN_MODE_UND - 非定向可连接 ; BLE_GAP_CONN_MODE_NON - 不可连接
 * @return 成功返回0，失败返回-1
 */
static int drv_ble_advertise(uint8_t conn_mode)
{
    struct ble_gap_adv_params adv_params;
#if 0    
    struct ble_hs_adv_fields fields = {0};
    int rc;

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* 蓝牙广播中加入服务UUID */
    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATTS_SVR_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    /* 蓝牙广播中加入mac地址 */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    rc |= ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting mac addr data to advertisement data, rc=%d\n", rc);
        return -1;
    }
    fields.public_tgt_addr = addr_val;
    fields.num_public_tgt_addrs = 1;

    /* 蓝牙广播中加入产商自定义数据 */
    #define MFG_DATA    "bluetti-plugs"
    fields.mfg_data = (const uint8_t*)MFG_DATA;
    fields.mfg_data_len = strlen(MFG_DATA);

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data, rc=%d\n", rc);
        return -1;
    }
#else
    #define BLE_ADV_MAX_SZ   BLE_HCI_MAX_ADV_DATA_LEN
    uint8_t ad_data[BLE_ADV_MAX_SZ];
    uint8_t ad_len = 0;

    /* 蓝牙广播flags：Discoverability and BLE-only (BR/EDR unsupported) */
    uint8_t ad_flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    put_ad(BLE_HS_ADV_TYPE_FLAGS, 1, &ad_flags, ad_data, &ad_len);

    /* 蓝牙广播中加入服务UUID */
    uint16_t svc_uuid = GATTS_SVR_UUID;
    put_ad(BLE_HS_ADV_TYPE_COMP_UUIDS16, 2, &svc_uuid, ad_data, &ad_len);

    /* 蓝牙广播中加入mac地址 */
    #define BLE_AD_TYPE_LE_DEV_ADDR 0x1b
    #define BLE_ADDR_TYPE_PUBLIC    0x00
    uint8_t addr_val[7] = {0};
    addr_val[0] = BLE_ADDR_TYPE_PUBLIC;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    rc |= ble_hs_id_copy_addr(own_addr_type, &addr_val[1], NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting mac addr data to advertisement data, rc=%d\n", rc);
        return -1;
    }
    put_ad(BLE_AD_TYPE_LE_DEV_ADDR, 7, addr_val, ad_data, &ad_len);

    /* 蓝牙广播中加入产商自定义数据 */
    #define MFG_DATA    "BLBLUETTF"//"bluetti-plugs"
//    BLE_HS_ADV_TYPE_MFG_DATA=0xFF
    put_ad(BLE_HS_ADV_TYPE_MFG_DATA, strlen(MFG_DATA), (uint8_t*)MFG_DATA, ad_data, &ad_len);

    /* 设置蓝牙广播数据 */
    rc = ble_gap_adv_set_data(ad_data, ad_len);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data, rc=%d\n", rc);
        return -1;
    }
#endif

    /* 设置蓝牙扫描响应数据（当使用nrf connect蓝牙调试工具查看raw数据时，其中包含了广播与响应的数据） */
    rc = drv_ble_response();
    if (rc != 0) return -1;

    /* 开启蓝牙广播 */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = conn_mode; 
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    adv_params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MAX;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, nimble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement, rc=%d\n", rc);
        return -1;
    }

    /* 只有在 API 调用成功后，才将逻辑状态设置为 true */
    ble_adv_logically_on = true;

    ESP_LOGW(TAG, "ble advertise successfully, name: %s", ble_info.dev_name);
    if (nimble_event_cb) {
        nimble_event_cb(NIMBLE_EVT_ADV);
    }
    return 0;
}
#endif

#if MYNEWT_VAL(BLE_POWER_CONTROL)
static void nimble_power_control(uint16_t conn_handle)
{
    int rc;

    rc = ble_gap_read_remote_transmit_power_level(conn_handle, 0x01 );  // Attempting on LE 1M phy
    assert (rc == 0);

    rc = ble_gap_set_transmit_power_reporting_enable(conn_handle, 0x1, 0x1);
    assert (rc == 0);
}
#endif

/**
 * @brief The nimble host executes this callback when a GAP event occurs.  
 * - The application associates a GAP event callback with each connection that forms.
 * - bleprph uses the same callback for all connections.
 * 
 * @param event The type of event being signalled.
 * @param arg pplication-specified argument, unused by bleprph
 * @return 0 if the application successfully handled the event; nonzero on failure.  
 * - The semantics of the return code is specific to the particular GAP event being signalled.
 */
static int nimble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;
    ESP_LOGI(TAG,"nimble_gap_event:%d",event->type);
    switch (event->type) 
    {
        /* 蓝牙连接事件 */
    case BLE_GAP_EVENT_CONNECT:
        reals.last_ble_server_connect_time = reals.now;
        ESP_LOGW(TAG, "ble connection %s, status=%d, conn_handle: %d", 
                event->connect.status == 0 ? "established" : "failed", event->connect.status, event->connect.conn_handle);

        /* 触发连接事件后广播停止（逻辑状态） */
        ble_adv_logically_on = false;
        
        /* 连接成功 */
        if (event->connect.status == 0) 
        {
            ble_conn_handle = event->connect.conn_handle;
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);  // 查找连接描述符
            if (rc == 0)
            {
                uint8_t addr_val[6];
                memcpy(addr_val, desc.our_id_addr.val, 6);
                ESP_LOGI(TAG, "loacl addr: %02x:%02x:%02x:%02x:%02x:%02x", 
                        addr_val[5],addr_val[4],addr_val[3],addr_val[2],addr_val[1],addr_val[0]);
                memcpy(addr_val, desc.peer_id_addr.val, 6);
                ESP_LOGI(TAG, "peer addr: %02x:%02x:%02x:%02x:%02x:%02x", 
                        addr_val[5],addr_val[4],addr_val[3],addr_val[2],addr_val[1],addr_val[0]);
            }

            /* 设置期望的连接参数，最终由central设备决定 */
            struct ble_gap_upd_params conn_params = {0};
            conn_params.itvl_min = BLE_GAP_CONN_ITVL_MS(15);
            conn_params.itvl_max = BLE_GAP_CONN_ITVL_MS(15);
            conn_params.latency = BLE_GAP_INITIAL_CONN_LATENCY;
            conn_params.supervision_timeout = (uint16_t)(500); // 5s（单位：10ms）
            conn_params.min_ce_len = BLE_GAP_INITIAL_CONN_MIN_CE_LEN;
            conn_params.max_ce_len = BLE_GAP_INITIAL_CONN_MAX_CE_LEN;
            rc = ble_gap_update_params(event->connect.conn_handle, &conn_params);
            if (rc != 0) {
                ESP_LOGE(TAG, "failed to update connection params, rc = %d", rc);
            }
			
			ble_encript_connected();
            ble_eventInfo.ble_event.bit.connectEvent=1;

            ESP_LOGW(TAG, "restart advertising");
#if BLE_CONFIG_EXTENDED_ADV
            ext_drv_ble_advertise();
#else
            drv_ble_advertise(BLE_GAP_CONN_MODE_NON);
#endif
        }

        /* 连接失败后重启广播 */
        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ble_eventInfo.ble_event.bit.connectEvent=0;
#if BLE_CONFIG_EXTENDED_ADV
            ext_drv_ble_advertise();
#else
            drv_ble_advertise(BLE_GAP_CONN_MODE_UND);
#endif
        }

#if MYNEWT_VAL(BLE_POWER_CONTROL)
	    nimble_power_control(event->connect.conn_handle);  // 设置链路功率
#endif
        return 0;

        /* 蓝牙连断开连接事件，断开后开启广播 */
    case BLE_GAP_EVENT_DISCONNECT:
        reals.last_ble_server_disconn_time = reals.now;
        ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_eventInfo.ble_event.bit.connectEvent=0;
		ble_encript_disconnect_reset();			
        drv_ble_advertise_pause();
        /* 仅在蓝牙仍使能时恢复广播，避免 iot_ble_stop 异步 DISCONNECT 又把广播拉起 */
        if (ble_encrypt_info.flag.bit.is_enable) {
            ESP_LOGE(TAG, "ble disconnect, reason=%d, and ble resume advertising", event->disconnect.reason);
#if BLE_CONFIG_EXTENDED_ADV
            ext_drv_ble_advertise();
#else
            drv_ble_advertise(BLE_GAP_CONN_MODE_UND);
#endif
        } else {
            ESP_LOGE(TAG, "ble disconnect, reason=%d, ble disabled, skip resume advertising",
                     event->disconnect.reason);
        }
        return 0;

        /* central更新蓝牙连接参数 */
    case BLE_GAP_EVENT_CONN_UPDATE:
        if (event->conn_update.status) {
            ESP_LOGE(TAG, "central update connection parameters, status=%d", event->conn_update.status);
        } else {
            ESP_LOGI(TAG, "conn update success");
        }
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc == 0)
        {
            ESP_LOGI(TAG, "conn_interval=%d(%gms) conn_latency=%d(%dms) supervision_timeout=%d(%dms)",
                    desc.conn_itvl, (float)desc.conn_itvl * 1.25f,
                    desc.conn_latency, (int)(desc.conn_latency * desc.conn_itvl * 1.25f),
                    desc.supervision_timeout, desc.supervision_timeout * 10);

            /* 外围设备最多可跳过 latency 个连接事件，因此两次实际通信之间最长间隔 */
            ble_info.rx_timeout = MAX(((float)(desc.conn_latency + 1) * desc.conn_itvl * 1.25f + 495), 500);

            /* rx_timeout 不应超过监督超时，否则还未触发应用层超时连接就已经断开 */
            uint32_t sup_timeout_ms = (uint32_t)desc.supervision_timeout * 10;
            if ((uint32_t)ble_info.rx_timeout > sup_timeout_ms) {
                ble_info.rx_timeout = (uint16_t)(sup_timeout_ms - 10);
            }
        }
        else
        {
            ESP_LOGE(TAG, "ble_gap_conn_find failed rc=%d, desc not valid", rc);
            ble_info.rx_timeout = 500;
        }

        ESP_LOGI(TAG, "receive ble message timeout = %ums", ble_info.rx_timeout);
        return 0;

        /* 蓝牙广播完成，蓝牙可设置每次广播的时间，当广播时间达到后则广播完成
         * 广播完成后继续开启广播 */
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGW(TAG, "advertise complete, reason=%d, ble_adv_logically_on=%d",
                 event->adv_complete.reason, (uint8_t)ble_adv_logically_on);
        if (ble_adv_logically_on == false) {
            return 0;
        }
#if BLE_CONFIG_EXTENDED_ADV
        ext_drv_ble_advertise();
#else
        drv_ble_advertise_restart();
#endif
        return 0;

        /* 蓝牙加密事件，对此连接启用或禁用加密时发生 */
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGW(TAG, "encryption change event; status=%d", event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        if (rc == 0)
        {
            ESP_LOGI(TAG, "encrypted=%d authenticated=%d bonded=%d",
                    desc.sec_state.encrypted, desc.sec_state.authenticated, desc.sec_state.bonded);
        }
        return 0;

        /* 蓝牙通知发送事件，当调用API发送通知时产生该事件 */
    case BLE_GAP_EVENT_NOTIFY_TX:
        ESP_LOGD(TAG, "notify_tx event, conn_handle=%d attr_handle=%d status=%d is_indication=%d",
                event->notify_tx.conn_handle,
                event->notify_tx.attr_handle,
                event->notify_tx.status,
                event->notify_tx.indication);
        return 0;

        /* 蓝牙订阅事件，当central设备订阅或取消订阅某个特性的通知或指示时产生该事件 */
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (gatts_get_chr1_handle() == event->subscribe.attr_handle) {
            notify_flag1 = event->subscribe.cur_notify;
            //if(reals.ota_happen!=1)
			    //ble_encript_part(notify_flag1);
            //else if(notify_flag1!=0x0000)
            {
                ble_encript_part(notify_flag1);
            }
        }
        else if (gatts_get_chr3_handle() == event->subscribe.attr_handle) {
            notify_flag3 = event->subscribe.cur_notify;
        }
        
        ESP_LOGI(TAG, "subscribe event, conn_handle=%d attr_handle=%d reason=%d "
                "prev_notify=%d cur_notify=%d prev_indicate=%d cur_indicate=%d\n",
                event->subscribe.conn_handle,
                event->subscribe.attr_handle,
                event->subscribe.reason,
                event->subscribe.prev_notify,
                event->subscribe.cur_notify,
                event->subscribe.prev_indicate,
                event->subscribe.cur_indicate);
        return 0;

        /* 蓝牙MTU交换事件， 当发起更新连接的MTU时会发生此事件*/
    case BLE_GAP_EVENT_MTU:
        ble_info.mtu_size = event->mtu.value;
        if (ble_info.mtu_size > 247)
        {   
            /*当前限制MTU最大长度为247*/
            ble_info.mtu_size = 247;
        }
        ESP_LOGW(TAG, "mtu update event, conn_handle=%d cid=%d mtu=%d\n",
                event->mtu.conn_handle,
                event->mtu.channel_id,
                event->mtu.value);
        return 0;

        /* 蓝牙重复配对 */
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc != 0) {
            ESP_LOGE(TAG, "failed to delete the old bond, rc=%d\n", rc);
        }
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

        /* 蓝牙配对码 */
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGE(TAG, "passkey action event, unsupported");
        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
        } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
        } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
        }
        return 0;

#if MYNEWT_VAL(BLE_POWER_CONTROL)
        /* 蓝牙传输功率事件 */
    case BLE_GAP_EVENT_TRANSMIT_POWER:
        ESP_LOGI(TAG, "Transmit power event : status=%d conn_handle=%d reason=%d "
                "phy=%d power_level=%x power_level_flag=%d delta=%d",
                event->transmit_power.status,
                event->transmit_power.conn_handle,
                event->transmit_power.reason,
                event->transmit_power.phy,
                event->transmit_power.transmit_power_level,
                event->transmit_power.transmit_power_level_flag,
                event->transmit_power.delta);
        return 0;

        /* 路径损耗阈值事件 */
     case BLE_GAP_EVENT_PATHLOSS_THRESHOLD:
        ESP_LOGI(TAG, "Pathloss threshold event : conn_handle=%d current path loss=%d zone_entered =%d",
                event->pathloss_threshold.conn_handle,
                event->pathloss_threshold.current_path_loss,
                event->pathloss_threshold.zone_entered);
        return 0;
#endif
    }

    return 0;
}

/**
 * @brief 协议栈复位回调函数
 * - 当蓝牙主机或控制器发生致命错误时，该函数执行
 * - 协议栈复位后重启CPU
 * 
 */
static void nimble_on_reset(int reason)
{
    ESP_LOGE(TAG, "nimble reset due to fatal error, reason=%d, restart cpu", reason);
    esp_restart();
}

#if BLE_CONFIG_ADDR
static void ble_app_set_addr(void)
{
    ble_addr_t addr;
    int rc;

    /* generate new non-resolvable private address */
    rc = ble_hs_id_gen_rnd(0, &addr);
    assert(rc == 0);

    /* set generated address */
    rc = ble_hs_id_set_rnd(addr.val);

    assert(rc == 0);
}
#endif

/**
 * @brief 蓝牙主机与控制器同步函数
 * - 此回调在启动和复位之后，主机和控制器同步时执行
 * 
 */
static void nimble_on_sync(void)
{
    int rc;

#if BLE_CONFIG_ADDR
    /* Generate a non-resolvable private address. */
    ble_app_set_addr();
#endif

    /* Make sure we have proper identity address set (public preferred) */
#if BLE_CONFIG_ADDR
    rc = ble_hs_util_ensure_addr(1);
#else
    rc = ble_hs_util_ensure_addr(0);    // 设置公共地址
#endif
    assert(rc == 0);

    /* figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) 
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    ESP_LOGW(TAG, "device addr: %02x:%02x:%02x:%02x:%02x:%02x", 
            addr_val[5],addr_val[4],addr_val[3],addr_val[2],addr_val[1],addr_val[0]);

    /* begin advertising. */
#if BLE_CONFIG_EXTENDED_ADV
    ext_drv_ble_advertise();
#else
    drv_ble_advertise(BLE_GAP_CONN_MODE_UND);    // 蓝牙广播从此处开启
#endif
}

/**
 * @brief 蓝牙主机任务
 * 
 * @param param 任务参数
 */
static void drv_nimble_host_task(void *param)
{
    ESP_LOGW(TAG, "nimble host task started");

    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * @brief 用户数据接收
 等同于data_rx_callback()
 * 
 * @param data 数据指针
 * @param len 数据长度
 */
static void app_data_rx_callback(uint8_t *data, uint8_t len)
{
    if (app_rx_cb) {
        app_rx_cb(data, len);
    }
}

/**
 * @brief 蓝牙主机配置
 * 
 */
static void nimble_host_config(void)
{
    /* 初始化NimBLE主机配置，ble_hs_cfg是nimble内置的一个变量 */
    ble_hs_cfg.reset_cb = nimble_on_reset;
    ble_hs_cfg.sync_cb = nimble_on_sync;
    ble_hs_cfg.gatts_register_cb = gatts_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
#ifdef BLE_CONFIG_BONDING
    ble_hs_cfg.sm_bonding = 1;
    /* Enable the appropriate bit masks to make sure the keys
     * that are needed are exchanged
     */
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
#endif

#ifdef BLE_CONFIG_MITM
    ble_hs_cfg.sm_mitm = 1;
#endif

#ifdef BLE_CONFIG_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0;
#endif

#ifdef BLE_CONFIG_RESOLVE_PEER_ADDR
    /* Stores the IRK */
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
#endif
}

/**
 * @brief 蓝牙初始化
 * 
 * @param name 蓝牙名称
 * @param cb 应用层数据接收回调函数
 * @param event_cb 蓝牙事件回调函数
 * @return 成功返回0，失败返回-1
 */
int drv_ble_init(const char *name, app_rx_callback_t cb, nimble_event_cb_t event_cb)
{
    if (drv_inited) return 0;
    
    /* 初始化蓝牙控制器与NimBLE主机协议栈 */
    int ret = nimble_port_init();
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "failed to init nimble, ret: %d ", ret);
        return -1;
    }
    
    /* 蓝牙主机配置，蓝牙广播在sync_cb回调函数中开启 */
    nimble_host_config();
    
    /* 蓝牙服务初始化 */
    ret = gatts_svr_init(app_data_rx_callback);
    assert(ret == 0);
    
    /* 设置蓝牙广播名称 */
    if (name) {
        ret = ble_svc_gap_device_name_set(name);
    }
    else {
        ret = ble_svc_gap_device_name_set(BLE_DEFAULT_NAME);
    }
    assert(ret == 0);
    
    /* 指向蓝牙协议内部蓝牙名称变量的地址，当使用蓝牙API修改设备名时，该指针仍然指向最新的蓝牙名称 */
    ble_info.dev_name = ble_svc_gap_device_name();
    ble_info.mtu_size = BLE_ATT_MTU_DFLT;
    ble_info.rx_timeout = 500;

    /* 初始化蓝牙存储接口，该函数是nimble内置的一个函数 */
    extern void ble_store_config_init(void);
    ble_store_config_init();
    
    /* 初始化nimble主机任务且保存应用层数据回调函数 */
    nimble_port_freertos_init(drv_nimble_host_task);
    
    app_rx_cb = cb;//回调函数初始化给指针
    if (event_cb) {
        nimble_event_cb = event_cb;//回调函数初始化给指针
    }
    drv_inited = true;
    return 0;
}

/**
 * @brief 蓝牙反初始化
 * - 该函数停止蓝牙主机任务，释放蓝牙内存资源
 * 
 * @return 成功返回0，失败返回-1
 */
int drv_ble_deinit(void)
{
    esp_err_t err = ESP_OK;
#if CONFIG_BT_NIMBLE_ENABLED
    if (!ble_hs_is_enabled()) {
        ESP_LOGI(TAG, "BLE already deinited");
        return 0;
    }

    if (nimble_port_stop() != 0) {
        ESP_LOGE(TAG, "nimble_port_stop() failed");
        return -1;
    }
    vTaskDelay(100);
    nimble_port_deinit();
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    err = esp_nimble_hci_and_controller_deinit();
#endif
#endif /* CONFIG_BT_NIMBLE_ENABLED */

#if CONFIG_IDF_TARGET_ESP32
    err |= esp_bt_mem_release(ESP_BT_MODE_BTDM);
#elif CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32H2
    err |= esp_bt_mem_release(ESP_BT_MODE_BLE);
#endif
    
    drv_inited = false;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ble deinit failed, system restart");
        esp_restart();
        return -1;
    }
    ESP_LOGW(TAG, "ble deinit successful and memory reclaimed");
    return 0;
}

/**
 * @brief 蓝牙断开连接
 * 
 * @return 成功返回0，失败返回-1
 */
int drv_ble_disconnect(void)
{
    if (ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) return 0;

    /* terminate the connection. */
    int ret = ble_gap_terminate(ble_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return (ret == 0)? (0) : (-1);
}

/**
 * @brief 蓝牙特征1数据发送
 * 
 * @param data 数据指针
 * @param size 数据大小
 * @return 成功返回0，失败返回-1
 */
int drv_ble_chr1_send(uint8_t *data, int size)
{
    if (!drv_inited) return -1;
    if (ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) return -1;
    int max_len = ble_info.mtu_size - 3;

    while (size) 
    {
        int len = (size >= max_len) ? (max_len) : (size);
        if (gatts_data_tx(data, len, notify_flag1, ble_conn_handle) != 0)
        {
            ESP_LOGE(TAG, "ble characteristic1 send data failed");
            return -1;
        }
        else
        {
            size -= len;
            data += len;
        }
    }
    return 0;
}

/**
 * @brief 蓝牙特征3数据发送
 * 
 * @param data 数据指针
 * @param size 数据大小
 * @return 成功返回0，失败返回-1
 */
int drv_ble_chr3_send(uint8_t *data, int size)
{
    if (!drv_inited) return -1;
    if (ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) return -1;
    int max_len = ble_info.mtu_size - 3;

    while (size) 
    {
        int len = (size >= max_len) ? (max_len) : (size);
        if (gatts_chr3_data_tx(data, len, notify_flag3, ble_conn_handle) != 0)
        {
            ESP_LOGE(TAG, "ble characteristic3 send data failed");
            return -1;
        }
        else
        {
            size -= len;
            data += len;
        }
    }
    return 0;
}

/**
 * @brief 设置蓝牙名称
 * - 如果蓝牙已连接则仅仅是设置广播名，后续断开连接后会以新的名称广播
 * - 如果未处于连接状态则停止当前广播后以新的名称广播
 * 
 * @param name 名称
 * @return 成功返回0，失败返回-1
 */
int drv_ble_set_dev_name(const char *name)
{
    if (!drv_inited) return -1;
    ESP_RETURN_ON_FALSE(name, -1, TAG, "name is null");

    /* 设置设备名 */
    int ret = ble_svc_gap_device_name_set(name);
    ESP_RETURN_ON_FALSE(ret==0, -1, TAG, "ble gap set device name failed");

    if (ble_gap_adv_active())
    {
        ESP_LOGI(TAG, "stop advertise for setting device name, then restart advertise");
        ret |= drv_ble_advertise_pause();
#if BLE_CONFIG_EXTENDED_ADV
        ext_drv_ble_advertise();
#else
        ret |= drv_ble_advertise(BLE_GAP_CONN_MODE_UND);
#endif
    }

    return ret;
}

/**
 * @brief 暂停蓝牙广播
 * - 停止当前广播，并清除逻辑开播标志
 * @return 成功返回0，失败返回非0
 */
int drv_ble_advertise_pause(void)
{
    int ret = ble_gap_adv_stop();

    /* 只有在 API 调用成功后，才更新逻辑状态 */
    if (ret == 0) {
        ble_adv_logically_on = false;
    }

    return ret;
}

/**
 * @brief 查询蓝牙广播是否正在进行
 * @return true: 正在广播; false: 未广播
 */
bool drv_ble_advertise_status(void)
{
    return ble_gap_adv_active();
}

/**
 * @brief 按当前连接状态恢复蓝牙广播
 * - 已连接：不可连接广播；未连接：可连接广播
 * @return 成功返回0，失败返回-1
 */
int drv_ble_advertise_restart(void)
{
    int ret = 0;

    ESP_LOGI(TAG, "restart advertise");
    if (ble_eventInfo.ble_event.bit.connectEvent) {
        ret = drv_ble_advertise(BLE_GAP_CONN_MODE_NON);
    } else {
        ret = drv_ble_advertise(BLE_GAP_CONN_MODE_UND);
    }

    return ret;
}

/**
 * @brief 关闭蓝牙广播
 * - 如果蓝牙已连接则立即断开连接并停止当前广播
 * - 如果未处于连接状态则停止当前广播
 * 
 * @return 成功返回0，失败返回-1
 */
int drv_ble_advertise_stop(void)
{
    int ret = drv_ble_disconnect();
    if (0 != ret) {
        ESP_LOGI(TAG, "drv_ble_disconnect  fail");
    }

    ret = drv_ble_advertise_pause();
    return ret;
}
/**
 * @brief 关闭蓝牙广播（仅停广播，不断开连接）
 * 
 * @return 成功返回0，失败返回非0
 */
int drv_ble_advertise_stop2(void)
{
    return drv_ble_advertise_pause();
}

/**
 * @brief 开启蓝牙广播
 * 
 * @return 成功返回0，失败返回-1
 */
int drv_ble_advertise_start(void)
{
    ESP_LOGI(TAG, "restart advertise");
    return drv_ble_advertise_restart();
}

/**
 * @brief 获取蓝牙数据信息
 * 
 * @return 数据信息结构指针
 */
ble_info_t* drv_ble_info_get(void)
{
    if (ble_gap_adv_active()) 
	{
        ble_info.conn_sta = 0;
    }
	else
	{
		ble_info.conn_sta = 1;
	}
    return &ble_info;
}

uint8_t drv_ble_get_ConnectEvent(void)
{
    //return ble_State;
    return ble_eventInfo.ble_event.bit.connectEvent;
}

/**
 * @brief 获取蓝牙数据信息
 * 
 * @return 数据信息结构指针
 */
char* drv_ble_name_get(void)
{

	return ble_info.dev_name;
}

/**
 * @brief 获取ble mac地址
 * 
 * @param mac 返回的mac地址
 */
void drv_ble_get_mac_addr(uint8_t mac[6])
{
    if (!drv_inited)
    {
        memset(mac, 0x00, 6);
        return;
    }

    /* figure out address to use while advertising (no privacy for now) */
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) 
    { 
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        memset(mac, 0x00, 6);
        return;
    }

    ble_hs_id_copy_addr(own_addr_type, mac, NULL);
}

void Print_debug1(void) 
{
	ESP_LOGI(TAG, "ble_info.dev_name	  =%s",ble_info.dev_name);
}

