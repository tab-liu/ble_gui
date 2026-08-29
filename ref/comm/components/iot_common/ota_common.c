#include "ota_define.h"
#include "can_protocol.h"
#include "xmodem_client.h"
#include "http_client.h"
#include "comm_define.h"
#include "esp_log.h"
#include "bms_ota.h"
#include "utils.h"

#define TAG "[ota_common]"

const char *FileTypeString[] = {
 	"iot", "invarm", "invdsp", "bms", "ba", "packbcu", "packbmu", "packbms", "packm1",
    "packsafet", "packhv", "iotlcd", "iotlcd2", "RF", "DC_HUB", "AC_HUB", "DC-DC", "AT1 ARM", "Panel ARM","Synlink",
};

/*
这里sizeof(FileTypeString)得到整个数组的字节数，而sizeof(FileTypeString[0])得到数组中单个元素的字节数。两者相除即得到数组中元素的数量
*/
const uint8_t TypeCnt = sizeof(FileTypeString) / sizeof(FileTypeString[0]);

/**
 * @brief  系统是否正在升级
 * @return true 正在升级; false 正常运行
 */
bool sys_is_updating(void)
{
    if ((esp_ota_is_doing())
        || (CHANNEL_UNKOWN != xmodem_client_is_doing())
        || (http_ota_is_doing())
        || (bms_ota_is_doing())
        || (can_ota_is_doing()))
    {
#if 0
        ESP_LOGW(TAG, "esp_ota_is_doing(%d)", esp_ota_is_doing());
        ESP_LOGW(TAG, "xmodem_client_is_doing(%d)", xmodem_client_is_doing());
        ESP_LOGW(TAG, "http_ota_is_doing(%d)", (int)http_ota_is_doing());
        ESP_LOGW(TAG, "http_ota_is_doing(%d)", can_ota_status.sender_work);
#endif        
        return true;
    }

    return false;
}

/**
 * 均匀映射到 0..100（整数，向下截断），0 和 100 唯一
 * 逻辑：每任务拆为 2 个阶段（下载、非下载），每阶段等分，总单位 = total_count * 200
 *       部分升级仅含1个阶段
 * units = (curr_count-1)*200 + (is_download ? curr_pct : 100 + curr_pct)
 * real_pct = units / (2 * total_count)
 */
uint8_t System_ota_total_pct_update(uint8_t curr_pct, bool is_download, uint8_t curr_count, uint8_t total_count, bool is_only_stage)
{
    if (curr_pct > 100) curr_pct = 100;
    if (total_count == 0) total_count = 1;
    if (curr_count == 0) curr_count = 1;
    if (curr_count > total_count) curr_count = total_count;

    /* units 范围 0 .. total_count*200 */
    uint32_t units = (uint32_t)(curr_count - 1) * 100 * 2;
    if ( is_only_stage ) {
        units += (uint32_t)(curr_pct * 2);
    } else {
        units += (uint32_t)(is_download ? curr_pct : (100 + curr_pct));
    }

    uint32_t denom = 2 * (uint32_t)total_count; /* denom > 0 */
    uint32_t pct = units / denom; /* 向下截断，不四舍五入 */

    if (pct > 100) pct = 100;
    return (uint8_t)pct;
}

