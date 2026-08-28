#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gatts_svr.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"
#include "esp_check.h"
#include "esp_log.h"


#define TAG     "[gatts_svr]"

// #define GATTS_CONFIG_ENCRYPTION                     // 配置通信数据加密

/* 特征FF01定义，用于发送本地数据 */
#define CHR1_BUF_SIZE   256                         // 特征1数据缓存
static uint16_t chr1_send_len = 1;                  // 特征1发送数据长度
static uint8_t gatt_svr_chr1_val[CHR1_BUF_SIZE];    // 特征1属性值变量
static uint16_t gatt_svr_chr1_val_handle;           // 特征1属性句柄

/* 特征FF01描述符定义 */
static char gatt_svr_dsc1_val[] = "nimble: app read data channel";

/* 特征FF02定义，用于接收外部数据 */
#define CHR2_BUF_SIZE   256                         // 特征2数据缓存
static uint16_t chr2_recv_len = 1;                  // 特征2数据接收长度
static uint8_t gatt_svr_chr2_val[CHR2_BUF_SIZE];    // 特征2属性值变量
static uint16_t gatt_svr_chr2_val_handle;           // 特征2属性句柄

/* 特征FF02描述符定义 */
static char gatt_svr_dsc2_val[] = "nimble: app write data channel";

/* 特征FF03定义，用于发送本地数据 */
#define CHR3_BUF_SIZE   256                         // 特征3数据缓存
static uint16_t chr3_send_len = 1;                  // 特征3发送数据长度
static uint8_t gatt_svr_chr3_val[CHR1_BUF_SIZE];    // 特征3属性值变量
static uint16_t gatt_svr_chr3_val_handle;           // 特征3属性句柄

/* 特征FF03描述符定义 */
static char gatt_svr_dsc3_val[] = "nimble: notify data channel";
static gatts_data_rx_callback data_rx_callback;     //FF02 特征2数据接收回调函数
static int gatts_svr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

/**
 * @brief 蓝牙服务定义
 * - 该服务下包含两个特征FF01、FF02、FF03
 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* 定义服务 */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        // .uuid = &gatt_svr_svc_uuid.u,
        .uuid = BLE_UUID16_DECLARE(0xFF00),

        /* 定义该服务下的多个特征(特征数组) */
        .characteristics = (struct ble_gatt_chr_def[])
        {
            /* 特征FF01 */
            {
                // .uuid = &gatt_svr_chr1_uuid.u,
                .uuid = BLE_UUID16_DECLARE(0xFF01),
                .access_cb = gatts_svr_access,                           // 特征1访问回调函数
#if GATTS_CONFIG_ENCRYPTION
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC |
                BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
#else
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,   // 特征1属性（读、通知）
#endif
                .val_handle = &gatt_svr_chr1_val_handle,                // 特征1句柄

                /* 特征描述符数组 */
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                    //   .uuid = &gatt_svr_dsc1_uuid.u,
                      .uuid = BLE_UUID16_DECLARE(0x2901),
#if GATTS_CONFIG_ENCRYPTION
                      .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
#else
                      .att_flags = BLE_ATT_F_READ,
#endif
                      .access_cb = gatts_svr_access,                    // 特征1描述符访问回调函数
                      .arg = &gatt_svr_chr1_val_handle,                 // 特征1的描述符句柄
                    }, {
                      0, /* No more descriptors in this characteristic */
                    }
                },
            }, 

            /* 特征FF02 */
            {
                /* This characteristic can be subscribed to by writing 0x00 and 0x01 to the CCCD */
                // .uuid = &gatt_svr_chr2_uuid.u,
                .uuid = BLE_UUID16_DECLARE(0xFF02),
                .access_cb = gatts_svr_access,                           // 特征2访问回调函数
#if GATTS_CONFIG_ENCRYPTION
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC |
                BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
#else
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,  // 特征2属性（读、写、写不响应）
#endif
                .val_handle = &gatt_svr_chr2_val_handle,                // 特征2句柄

                /* 特征描述符数组 */
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                    //   .uuid = &gatt_svr_dsc2_uuid.u,
                      .uuid = BLE_UUID16_DECLARE(0x2901),
#if GATTS_CONFIG_ENCRYPTION
                      .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
#else
                      .att_flags = BLE_ATT_F_READ,
#endif
                      .access_cb = gatts_svr_access,                    // 特征2描述符访问回调函数
                      .arg = &gatt_svr_chr2_val_handle,                 // 特征2的描述符句柄
                    }, {
                      0, /* No more descriptors in this characteristic */
                    }
                },
            }, 

            /* 特征FF03 */
            {
                // .uuid = &gatt_svr_chr3_uuid.u,
                .uuid = BLE_UUID16_DECLARE(0xFF03),
                .access_cb = gatts_svr_access,                           // 特征3访问回调函数
#if GATTS_CONFIG_ENCRYPTION
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC |
                BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
#else
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,   // 特征3属性（读、通知）
#endif
                .val_handle = &gatt_svr_chr3_val_handle,                // 特征3句柄

                /* 特征描述符数组 */
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                    //   .uuid = &gatt_svr_dsc1_uuid.u,
                      .uuid = BLE_UUID16_DECLARE(0x2901),
#if GATTS_CONFIG_ENCRYPTION
                      .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC,
#else
                      .att_flags = BLE_ATT_F_READ,
#endif
                      .access_cb = gatts_svr_access,                    // 特征3描述符访问回调函数
                      .arg = &gatt_svr_chr3_val_handle,                 // 特征3的描述符句柄
                    }, {
                      0, /* No more descriptors in this characteristic */
                    }
                },
            }, 

            {
                0, /* No more characteristics in this service. */
            }
        },
    },

    {
        0, /* No more services. */
    },
};

/**
 * @brief gatts特征写处理
 * - 该函数将central设备发送的数据复制到用户设置的缓存区
 * 
 * @param om 协议栈中的数据缓存
 * @param min_len 该特征的最小长度
 * @param max_len 该特征的最大长度
 * @param dst 用户指定的缓存区
 * @param len 返回的实际数据
 * @return 成功返回0，失败返回BLE_ATT_ERR_XXX
 */
static int gatt_svr_write_handler(struct os_mbuf *om, uint16_t min_len, uint16_t max_len, void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    /* 复制协议栈中的数据到用户缓存区 */
    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

/**
 * @brief Access callback whenever a characteristic/descriptor is read or written to.
 * Here reads and writes need to be handled.
 * ctxt->op tells weather the operation is read or write and
 * weather it is on a characteristic or descriptor,
 * ctxt->dsc->uuid tells which characteristic/descriptor is accessed.
 * attr_handle give the value handle of the attribute being accessed.
 * Accordingly do:
 *     Append the value to ctxt->om if the operation is READ
 *     Write ctxt->om to the value if the operation is WRITE
 * 
 * @param conn_handle 连接句柄
 * @param attr_handle 属性句柄
 * @param ctxt gatts属性上下文（数据）
 * @param arg 特征初始化时的自定义参数
 * @return 成功返回0，失败返回 BLE_ATT_ERR_XXX
 */
static int gatts_svr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid;
    int rc;

    switch (ctxt->op) 
    {
        /* gatts特征属性读（发送到central的数据）:FF01：ESP32 TX */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        uuid = ctxt->chr->uuid;
        if (attr_handle == gatt_svr_chr1_val_handle) {          // 读取特征1的数据
            ESP_LOGD(TAG, "read characteristic1, attr_handle=%d, len: %d", attr_handle, chr1_send_len);
            rc = os_mbuf_append(ctxt->om,
                                gatt_svr_chr1_val,
                                chr1_send_len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        else if (attr_handle == gatt_svr_chr2_val_handle) {     // 读取特征2的数据
            ESP_LOGD(TAG, "read characteristic2, attr_handle=%d, len: %d", attr_handle, chr2_recv_len);
            rc = os_mbuf_append(ctxt->om,
                                gatt_svr_chr2_val,
                                chr2_recv_len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        else if (attr_handle == gatt_svr_chr3_val_handle) {     // 读取特征3的数据
            ESP_LOGD(TAG, "read characteristic3, attr_handle=%d, len: %d", attr_handle, chr3_send_len);
            rc = os_mbuf_append(ctxt->om,
                                gatt_svr_chr3_val,
                                chr3_send_len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto unknown;

    /* gatts特征属性写（来自central的数据）;FF02：ESP32 RX */
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        uuid = ctxt->chr->uuid;
        if (attr_handle == gatt_svr_chr2_val_handle) 
		{          // 处理发送给特征2的数据
            rc = gatt_svr_write_handler(ctxt->om,
                                        0,
                                        CHR2_BUF_SIZE,
                                        &gatt_svr_chr2_val, &chr2_recv_len);
            ble_gatts_chr_updated(attr_handle);
            ESP_LOGD(TAG, "write characteristic2, attr_handle=%d, len: %d", attr_handle, chr2_recv_len);
            if (chr2_recv_len) {
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, gatt_svr_chr2_val, chr2_recv_len, ESP_LOG_ERROR);
            }

//////////

/////////
			
            if (data_rx_callback) {
                data_rx_callback(gatt_svr_chr2_val, chr2_recv_len);
            }
            return rc;
        }
        goto unknown;

    /* gatts读取特征描述符 */
    case BLE_GATT_ACCESS_OP_READ_DSC:
        /* 读取描述符，初始化时arg指向了各自特征handle的地址 */
        uuid = ctxt->dsc->uuid;
        const ble_uuid16_t gatt_svr_dsc_uuid = BLE_UUID16_INIT(0x2901); // 标准描述符UUID
        if (ble_uuid_cmp(uuid, &gatt_svr_dsc_uuid.u) == 0) {
            if ((*(uint16_t*)arg) == gatt_svr_chr1_val_handle) {        // 读取特征1下的描述符
                ESP_LOGD(TAG, "read descriptor from characteristic1, conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
                rc = os_mbuf_append(ctxt->om,
                                    gatt_svr_dsc1_val,
                                    sizeof(gatt_svr_dsc1_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            else if ((*(uint16_t*)arg) == gatt_svr_chr2_val_handle) {   // 读取特征2下的描述符
                ESP_LOGD(TAG, "read descriptor from characteristic2, conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
                rc = os_mbuf_append(ctxt->om,
                                    gatt_svr_dsc2_val,
                                    sizeof(gatt_svr_dsc2_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            else if ((*(uint16_t*)arg) == gatt_svr_chr3_val_handle) {   // 读取特征3下的描述符
                ESP_LOGD(TAG, "read descriptor from characteristic3, conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
                rc = os_mbuf_append(ctxt->om,
                                    gatt_svr_dsc3_val,
                                    sizeof(gatt_svr_dsc3_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_WRITE_DSC:
        goto unknown;

    default:
        goto unknown;
    }

unknown:
    /* Unknown characteristic/descriptor;
     * The NimBLE host should not have called this function;
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief gatts服务事件回调函数
 * 
 * @param ctxt gatts属性上下文（数据）
 * @param arg 特征初始化时的自定义参数
 */
void gatts_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) 
    {
        /* 服务注册事件 */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "registered service %s with handle=%d",
                ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                ctxt->svc.handle);
        break;

        /* 特征注册事件 */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG, "registering characteristic %s with def_handle=%d val_handle=%d",
                ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                ctxt->chr.def_handle,
                ctxt->chr.val_handle);
        break;

        /* 特征描述符注册事件 */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(TAG, "registering descriptor %s with handle=%d",
                ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

/**
 * @brief gatts服务初始化
 * - 该函数会初始化一个自定义服务FF00
 * - 该函数会在FF00服务下初始化两个特征FF01、FF02
 * - 特征FF01发送数据到central设备，该特征具备读、通知属性
 * - 特征FF02接收central设备发送的数据，该特征具备读、写、写无响应属性
 * 
 * @param cb 数据接收回调函数（来自特征FF02的数据）
 * @return 成功返回0，失败返回BLE_ATT_ERR_XXX
 */
int gatts_svr_init(gatts_data_rx_callback cb)
{
    int rc;

    ESP_LOGI(TAG, "nimble gap gatt init...");
    ble_svc_gap_init();     // 初始化蓝牙GAP
    ble_svc_gatt_init();    // 初始化蓝牙GATT

    /* 设置蓝牙服务数量 */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    /* 添加蓝牙服务数量 */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    data_rx_callback = cb;//回调函数初始化给指针
    return 0;
}

/**

FF01

 * @brief gatts数据发送
 * - 该函数通过特征FF01的通知功能发送数据
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @param notify 通知标志
 * @param conn_handle 连接句柄
 * @return 成功返回0，失败返回-1 
 */
int gatts_data_tx(uint8_t *data, uint8_t len, uint8_t notify, uint16_t conn_handle)
{
    int ret = -1;
    ESP_RETURN_ON_FALSE(notify!=0, -1, TAG, "notification not opened");

    #define RETRY_MAX_CNT   10
    memcpy(gatt_svr_chr1_val, data, len);
    chr1_send_len = len;
    struct os_mbuf *om;
    int retry_cnt = RETRY_MAX_CNT;

	if (chr1_send_len) {
		ESP_LOGI(TAG, "gatts_data_tx: chr1_send_len= %d", chr1_send_len);
		
		ESP_LOG_BUFFER_HEX_LEVEL(TAG, gatt_svr_chr1_val, chr1_send_len, ESP_LOG_ERROR);
	}


    do {
        om = ble_hs_mbuf_from_flat(data, len);  // 数据复制到os_mbuf
        if (om == NULL) 
        {
            /* Memory not available for mbuf */
            ESP_LOGE(TAG, "no MBUFs available from pool, retry...");
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    } while ((om == NULL) && (--retry_cnt));

    if (retry_cnt <= 0)
    {
        ESP_LOGE(TAG, "retrying exceeds limit(%d), tx failed", RETRY_MAX_CNT);
        goto __exit;
    }

    /* 通知发送数据 */
    ret = ble_gatts_notify_custom(conn_handle, gatt_svr_chr1_val_handle, om);
    if (ret != 0) 
    {
        ESP_LOGE(TAG, "error while tx notification, ret = %d", ret);

        /* Most probably error is because we ran out of mbufs (rc = 6),
            * increase the mbuf count/size from menuconfig. Though
            * inserting delay is not good solution let us keep it
            * simple for time being so that the mbufs get freed up
            * (?), of course assumption is we ran out of mbufs */
        vTaskDelay(10 / portTICK_PERIOD_MS);
        goto __exit;
    }

 __exit:
    return ret;
}

/**
 * @brief gatts数据发送
 * - 该函数通过特征FF03的通知功能发送数据
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @param notify 通知标志
 * @param conn_handle 连接句柄
 * @return 成功返回0，失败返回-1 
 */
int gatts_chr3_data_tx(uint8_t *data, uint8_t len, uint8_t notify, uint16_t conn_handle)
{
    int ret = -1;
    ESP_RETURN_ON_FALSE(notify!=0, -1, TAG, "notification not opened");

    #define RETRY_MAX_CNT   10
    memcpy(gatt_svr_chr3_val, data, len);
    chr3_send_len = len;
    struct os_mbuf *om;
    int retry_cnt = RETRY_MAX_CNT;

    do {
        om = ble_hs_mbuf_from_flat(data, len);  // 数据复制到os_mbuf
        if (om == NULL) 
        {
            /* Memory not available for mbuf */
            ESP_LOGE(TAG, "no MBUFs available from pool, retry...");
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    } while ((om == NULL) && (--retry_cnt));

    if (retry_cnt <= 0)
    {
        ESP_LOGE(TAG, "retrying exceeds limit(%d), tx failed", RETRY_MAX_CNT);
        goto __exit;
    }

    /* 通知发送数据 */
    ret = ble_gatts_notify_custom(conn_handle, gatt_svr_chr3_val_handle, om);
    if (ret != 0) 
    {
        ESP_LOGE(TAG, "error while tx notification, ret = %d", ret);

        /* Most probably error is because we ran out of mbufs (rc = 6),
            * increase the mbuf count/size from menuconfig. Though
            * inserting delay is not good solution let us keep it
            * simple for time being so that the mbufs get freed up
            * (?), of course assumption is we ran out of mbufs */
        vTaskDelay(10 / portTICK_PERIOD_MS);
        goto __exit;
    }

 __exit:
    return ret;
}

/**
 * @brief 获取特征FF01的句柄
 * 
 * @return 特征FF01的句柄
 */
uint16_t gatts_get_chr1_handle(void)
{
    return gatt_svr_chr1_val_handle;
}

/**
 * @brief 获取特征FF03的句柄
 * 
 * @return 特征FF03的句柄
 */
uint16_t gatts_get_chr3_handle(void)
{
    return gatt_svr_chr3_val_handle;
}
