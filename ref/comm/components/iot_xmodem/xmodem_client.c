#include <string.h>
#include "esp_log.h"
#include "filesystem.h"
#include "xmodem_client.h"
#include "iot_ota.h"
#include "can_protocol.h"
// #include "iot_period_task.h"
#include "http_client.h"
#include "utils.h"
#include "ota_define.h"
#include "comm_define.h"
#include "image_handle.h"
#include "image_back.h"
#include "ota_define.h"
#include <arpa/inet.h>
#include "file_ota.h"
#include "bms_can.h"
#include "bms_ota.h"

#define TAG "[XMODEM-CLIENT]"

#define     MAX_RESEND_CNT             5

#define     BLOCK_BUFFER_SIZE               4096

#define     DATA_LENGTH_128                 128
#define     DATA_LENGTH_1024                1024
#define     FILL_LENGTH                     5       // head + seq + ~seq + crc1 + crc2



xmodem_struct gXmodem_Status = {
    .exit = 0,
    .used_chl = CHANNEL_UNKOWN,
    .timer = NULL,
    .pfile = NULL,
    .crc32 = ~0,
    .system_restart = 0,
};

#define	OTA_OBJECT_CAN_OTA	2// 下级CAN 设备OTA
#define	OTA_OBJECT_LCD	3//LCD,IOT模块直接uart关联的MCU


static uint8_t xmodem_client(FILE *pfile, xmodem_struct * status, const uint8_t *payload, uint16_t len);
static uint8_t xmodem_unpack(xmodem_struct * status, const uint8_t *payload, uint16_t len);
//EXT_RAM_BSS_ATTR uint8_t can_ota_buffer[1024*1024];
//EXT_RAM_BSS_ATTR uint8_t Ota_temp_buffer[1024];//1k read write flash

// CanOtaStruct   CanotaStatus;
CanOtaStruct   *CanotaStatus;
// extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];
static uint16_t count_1k;// int
static uint32_t ota_data_len;// int
// static char ota_bin_name[50];
static uint8_t ota_md_addr;//modbus slave address
ota_mode_t ota_mode;//=begin,0：不开启OTA只用于查看，1：需要开启单播OTA升级，2：需要开启半广播OTA升级，3：需要开启全广播OTA升级

/*------------------------------------------------------------------------------
 Function: vXmodemCmdCheck
 -----------------------------------------------------------------------------*/
/**
  * @brief      升级启动检查
  * @param[in]  uint8_t md_addr  
                uint8_t channel  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t vXmodemCmdCheck(uint8_t md_addr, uint8_t channel) 
{

	if (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.begin == 0) {
        return 0;
	}

    /*存在进行中的升级，本次升级无效*/
    if ( sys_is_updating() ) {
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.begin = 0;
        ESP_LOGE(TAG, "xmodem_ota : system is already updating!");
        return 0;
    }
    
	ota_mode_t _ota_mode = (ota_mode_t)top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.begin;//MicroInv[0]
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.begin = 0;//MicroInv[0]

    for (uint8_t i = 0; i < 16; i++)
	{
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[i].type    = 0;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[i].level   = 0;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[i].where   = 0;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[i].errCode = 0;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[i].pct     = 0;
        top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[i].isOta   = 0;
    }
    
    uint8_t dev_type = (top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type) & 0xFF;	// 升级的设备类型
	uint8_t devId = 0xFF;//MicroInv[0].mod_reg00700_OTA.ota_cmd.group.dev_id & 0xFF;			// 升级的设备地址(FF表示广播升级)

    ESP_LOGW(TAG, "Group[%d : %d] type=%u, version=%lu, size=%u",
			 dev_type, devId,
			 top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.type,
			 top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.version,
			 top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.size);

	if ((dev_type <= 0) || (dev_type >= GROUP_MAX))
	{
		ESP_LOGE(TAG, "Group[%d] is unknown", dev_type);
		return 0;
	}

    if (devId != 0xFF)
    {
        switch (dev_type) {
            // case 0xFF:  devId = CAN_BROADCAST_ADDRESS; break;      /* 设备的目标ID - 广播 */
            case GROUP_INV  : devId += INV_CAN_ADDR;  break;             /* 设备的目标ID - 逆变 */
            case GROUP_PACK : devId += PACK_CAN_ADDR; break;             /* 设备的目标ID - pack */
            case GROUP_IOT  : devId = INV_IOT_CAN_ADDR;   break;             /* 设备的目标ID - iot */
            case GROUP_LCD  : devId = 0x02;           break;             /* 设备的目标ID - LCD */
            default         : devId = 0xFF;           break;             /* 设备的目标ID - 其他*/
        }
    }

	gXmodem_Status.is_esp_ota = 0;
	ota_md_addr = md_addr;
	ota_mode = _ota_mode;

	//...todo
	// ota_mode = OTA_SEMI_BROADCASST;
	// ota_mode = OTA_UNICAST;
	// devId = 0x10;
    ota_mode = OTA_FULL_BROADCAST;//windy debug force


    uint8_t ret = vXmodemClientInit(devId,
							 top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.type,
							 top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.size,
							 top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.version,
							 channel);
    
    if(!ret) ota_status_err_set();
    return ret;
}

static void vXmodemTimeout(TimerHandle_t xTimer) {
    gXmodem_Status.time_count += 1000;
    // ESP_LOGI(TAG, "timer 1s");
}

uint8_t vXmodemClientExit(channel_type channel) 
{
    if (gXmodem_Status.used_chl == channel) {
        if (gXmodem_Status.block.payload) {		// 释放xmodem交互过程中申请的缓存
            free(gXmodem_Status.block.payload);
            gXmodem_Status.block.payload = NULL;
            ESP_LOGI(TAG, "block buffer free");
        }

        if (gXmodem_Status.timer != NULL) {		// 删除xmodem交互过程中使用的定时器
            xTimerDelete(gXmodem_Status.timer, pdMS_TO_TICKS(1000));
            gXmodem_Status.timer = NULL;
            ESP_LOGI(TAG, "xTimerDelete");
        }

        ESP_LOGW(TAG, "XmodemClient Exit!");
        gXmodem_Status.used_chl = CHANNEL_UNKOWN;
        gXmodem_Status.is_esp_ota = 0;
    }

    return 0;
}

static esp_err_t dev_ota_begin(uint16_t size) {
    size_t file_len = size * 1024;
    size_t result = ((file_len + FLASH_SEC_SIZE - 1) / FLASH_SEC_SIZE) * FLASH_SEC_SIZE; // 向上对齐到4096倍数
    /*检查文件大小*/
    if ( result > IMAGE_FLASH_AREA_MAX_LEN ) {
        ESP_LOGE(TAG, "file size is too big(max: %d KB, real: %d KB)", (uint16_t)(IMAGE_FLASH_AREA_MAX_LEN/1024), result/1024);
        gXmodem_Status.errcode = OTA_ERR_SIZE;
        return ESP_FAIL;
    } else {
        /*创建新的文件*/
        if (iot_image_erase(IMAGE_FLASH_AREA_ADDRESS, result) != ESP_OK) {
            gXmodem_Status.errcode = OTA_ERR_FLASH_ERASE;
            ESP_LOGE(TAG, "iot_image_erase failed.");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

/*
return:
1-ok
0-fail
*/
uint8_t vXmodemClientInit(int id, uint8_t file_type, uint16_t size, uint32_t version,  channel_type channel) 
{
    gXmodem_Status.file_type = file_type;
    gXmodem_Status.file_size = size;  // 固件文件大小
    gXmodem_Status.version = version; // 固件版本号
#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE    
    reals.system_ota.ota_mcu_curr_count++;
#endif

    if (file_type >= TypeCnt) {
        ESP_LOGE(TAG, "file type unknown, file id = %d", file_type);
        gXmodem_Status.exit = 1;
        gXmodem_Status.errcode = OTA_ERR_FILE_TYPE;
        return 0;
    }

    gXmodem_Status.block.paylen = 0;
    current_ota_info_update(1, file_type, 0, 0, version);
    
    if (gXmodem_Status.block.payload == NULL) {
        gXmodem_Status.block.payload = iot_calloc(BLOCK_BUFFER_SIZE * sizeof(uint8_t)); // 4096缓存
    }

    if (gXmodem_Status.block.payload == NULL)	 //  内存分配失败
	{
        gXmodem_Status.exit = 1;
        gXmodem_Status.errcode = OTA_ERR_MEMONY;
		ESP_LOGE(TAG, "upgrade block allocate failed, exit OTA");
        return 0;
    }

    gXmodem_Status.exit = 0;
    gXmodem_Status.errcode = OTA_ERR_NORMAL;
    
    if (file_type == IOT) 
    {   
        /* IOT本身进行OTA升级 */
        if ((top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver/100) == (version / 100)) {
            ESP_LOGI(TAG, "new:%lu, old:%lu", version, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver);
            gXmodem_Status.is_esp_ota = 1;
            if (iot_ota_begin() != ESP_OK) {
                gXmodem_Status.exit = 1;
                return 0;
            }
        }
        /*非本机，检查CAN总线IOT设备*/
        else if ((top_modbus_rd.Dcdc[DCDC_MAX_NUM].mod_reg11000_IOT_info.software_ver/100) == (version / 100))
        {
            if (dev_ota_begin(size) != ESP_OK) {
                gXmodem_Status.exit = 1;
                ESP_LOGE(TAG, "dev_ota_begin failed, exit OTA");
                return 0;
            }
        }
        else
        {
            ESP_LOGE(TAG, "software_ver error(new:%lu, old:%lu)", version, top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.software_ver);
            gXmodem_Status.exit = 1;
            gXmodem_Status.errcode = OTA_ERR_FILE;
            return 0;
        }
    } 
	else 
	{
	    /*其他固件类型创建新的文件*/
        if (dev_ota_begin(size) != ESP_OK) {
            gXmodem_Status.exit = 1;
            ESP_LOGE(TAG, "dev_ota_begin failed, exit OTA");
            return 0;
        }
    }

    if (gXmodem_Status.timer == NULL) {
        gXmodem_Status.timer = xTimerCreate("xmodem", pdMS_TO_TICKS(1000), pdTRUE, NULL, vXmodemTimeout);  // 创建超时定时器
        if (gXmodem_Status.timer == NULL) {
            ESP_LOGE(TAG, "xmodem timer create failed");
            gXmodem_Status.exit = 1;
            gXmodem_Status.errcode = OTA_ERR_MEMONY;
            return 0;
        }
        xTimerStart(gXmodem_Status.timer, pdMS_TO_TICKS(100));
    }

    gXmodem_Status.target_id = id;
    gXmodem_Status.block_seq = 1;
    gXmodem_Status.used_chl = channel;
    gXmodem_Status.step = 0;
    gXmodem_Status.resend_count = 0;
    gXmodem_Status.time_count = 0;

    ESP_LOGI(TAG, "device information to be upgraded is: DevId=0x%x, Type=%d, Size:%ukb, Ver:%lu", id, file_type, size, version);
//    iot_wifi_close(WIFI_MODE_ALL); // 蓝牙升级时，关掉WiFi保证升级的稳定性和速度
//    vTaskDelay(pdMS_TO_TICKS(100));

    return 1;
}

/*
单播升级
type:OtaFileType ,modbus 701
*/
static int can_ota_unicast_begin(uint8_t id, uint8_t type, uint32_t version)
{
	if (ota_md_addr == 0) return -1;

	uint8_t group = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type;
	uint8_t dev_idx = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_id;
	uint8_t node_id = ota_md_addr -1;

	if (node_id >= DEV_MAIN_NODE_MAX)
	{
		ESP_LOGE(TAG, "ota device node error, now node: %d, max node: %d", node_id, DEV_MAIN_NODE_MAX);
		return -1;
	}

	if (group == GROUP_INV)
	{
		if (!can_node_rd.Inv[id].online)
		{
			ESP_LOGE(TAG, "inv is offline, node: %d, devid: %d, exit ota", node_id, dev_idx);
			return -1;
		}
	}
	else
	{
	    #if 0
		pack_announce_struct *pack_ann = &can_node_rd.Pack[dev_idx].pack_announce;
		if (!(pack_ann->online & (1 << dev_idx)))
		{
			ESP_LOGE(TAG, "pack is offline, node: %d, devid: %d, exit ota", node_id, dev_idx);
			return -1;
		}
        #else
        ESP_LOGE(TAG, "pack is offline, node: %d, devid: %d, exit ota", node_id, dev_idx);
        return -1;
        #endif
	}

//	ESP_LOGW(TAG, "begin to unicast upgrade, node: %d, addr: %02x, file type:%s, version: %u, dev_idx: %d",
//			 node_id, id, FileTypeString[type], version, dev_idx);
	// memcpy(&can_ota_status, &CanotaStatus, sizeof(CanotaStatus));
	CanOtaSender_Begin(id, type, version, &can_ota_status);
	return 0;
}

/*
半广播升级
type:OtaFileType ,modbus 701

*/
static int can_ota_semi_broadcast_begin(uint8_t id, uint8_t type, uint32_t version)
{
	int ret = -1;
	uint8_t group = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type;
	uint8_t dev_idx = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_id;

	/* 轮询检测哪些节点需要CAN-OTA */
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		int online = 0;
		if (group == GROUP_INV) {
			online = can_node_rd.Inv[dev_idx].online;
		}
		else {
            #if 0
			online = can_node_rd.Pack[dev_idx].pack_announce.online & (1 << dev_idx);
            #endif
		}

		/* 设备在线该节点启动CAN-OTA */
		if(online)
		{
//			ESP_LOGW(TAG, "begin to semi-broadcast upgrade, node: %d, addr: %02x, file type:%s, version: %u, dev_idx: %d",
//					 node_id, id, FileTypeString[type], version, dev_idx);
			// memcpy(&can_ota_status, &CanotaStatus, sizeof(CanotaStatus));
			CanOtaSender_Begin(id, type, version, &can_ota_status);
			ret = 0;
		}
	}

	return ret;
}

/*
全广播升级

*/
static int can_ota_full_broadcast_begin(uint8_t id, uint8_t type, uint32_t version)
{
	int ret = -1;
    uint8_t group = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type;	// 升级的组类型
    ESP_LOGI(TAG, "get in can_ota_full_broadcast_begin");
    
	/* 轮询检测哪些节点需要CAN-OTA */
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		int online = 0;
		if (group == GROUP_INV)
		{
		    if ( type != DEVICE_DC_HUB )
		    {
                for(int i = 0; i < INV_MAX_NUM; i++)    // 查询节点上是否有逆变设备在线
                {
                    online = can_node_rd.Inv[i].online;
                    if(online) 
                    {
                        ESP_LOGI(TAG, "inv online:%d, node_id:%d",online,node_id);//test OTA
                        break;
                    }
                }
		    }
            else
            {
                for(int i = 0; i < DC_HUB_MAX_NUM; i++)    // 查询节点上是否有DCHUB在线
                {
                    online = can_node_rd.DCHUB[i].online;
                    if(online) 
                    {
                        ESP_LOGI(TAG, "DCHUB online:%d, node_id:%d",online,node_id);//test OTA
                        break;
                    }
                }
            }
		}
		else if(group == GROUP_PACK)
		{
            uint8_t level1_addr = 0;
            uint8_t level2_addr = 0;

            /*Pack*/
            for (int i = 0; i < PACK_MAX_NUM; i++)      // 查询节点上是否有电池包在线
            {
                level1_addr = i / PACK_INGROUP_MAX_NUM; // PACK
                level2_addr = i % PACK_INGROUP_MAX_NUM; // PACK

#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE
                bat_data_t *bms_data = get_bat_data_ptr();
                online = bms_data->bms_data[i].valid;
#endif

#ifdef BAT_CAN_PROTOCOL_BETA_ENABLE
                online = (can_node_rd.Pack[level2_addr].pack_announce.online & (1 << level2_addr));
#endif
				if(online)
                {
                    ESP_LOGI(TAG, "pack online:%d, node_id:%d", online, i);//test OTA
					break;
				}
			}

            // 没有PACK在线时，有逆变也进行下发
            if(!online) {
                for(int i = 0; i < INV_MAX_NUM; i++)    // 查询节点上是否有逆变设备在线
                {
                    online = can_node_rd.Inv[i].online;
                    if(online) 
                    {
                        ESP_LOGI(TAG, "inv online:%d, node_id:%d",online,node_id);//test OTA
                        break;
                    }
                }
            }
		}
		else if (group == GROUP_CHARGE)
		{
            for(int i = 0; i < DCDC_MAX_NUM; i++)    // 查询节点上是否有逆变设备在线
            {
                online = can_node_rd.Dcdc[i].online;
                if(online) 
                {
                    ESP_LOGI(TAG, "dcdc online:%d, node_id:%d",online,node_id);//test OTA
                    break;
                }
            }
		}
        else
        {
            ESP_LOGE(TAG, "GROUP_UNKNOWN: %d", group);
        }
        
		/* 设备在线该节点启动CAN-OTA */
		if(online)
		{
			ESP_LOGI(TAG, "begin to full-broadcast upgrade, node: %d, addr: %02x, file type:%d, version: %lu",
					 node_id, id, type, version);
			// memcpy(&can_ota_status, &CanotaStatus, sizeof(CanotaStatus));
			CanOtaSender_Begin(id, type, version, &can_ota_status);
			ret = 0;
		}
	}

	return ret;
}

/*
type:OtaFileType ,modbus 701


*/
 int can_ota_begin(uint8_t id, uint8_t type, uint32_t version)
{
	int ret = -1;
    uint8_t group = top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_cmd.group.dev_type;	// 升级的组类型
    ESP_LOGI(TAG, "group(%d), ota_mode:%d, id:%d, type:%d, version:%ld", group,ota_mode,id,type,version);
	
	//uint8_t group = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_type;//wen
	//uint8_t dev_idx = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_id;

	/* 升级类型错误退出 */
	if((group <= 0) || (group >= GROUP_MAX))
	{
		ESP_LOGE(TAG, "group(%d) is error, exit can-ota", group);
		return -1;
	}

    /*实际当前仅使用全广播升级*/
	switch (ota_mode)
	{
	case OTA_UNICAST:				// 单播升级
		ret = can_ota_unicast_begin(id, type, version);
		break;

	case OTA_SEMI_BROADCASST:		// 半广播升级
		ret = can_ota_semi_broadcast_begin(id, type, version);
		break;

	case OTA_FULL_BROADCAST:		// 全广播升级
		ret = can_ota_full_broadcast_begin(id, type, version);
		break;

	default:
		break;
	}

	if (ret != 0) {
		ESP_LOGW(TAG, "no device upgrade, exit can-ota");
	}

	return ret;
}

/*
从flash中读取全部待OTA文件，判断CRC
 data= NULL

 */
static uint32_t can_ota_data_crc(uint8_t *data,  uint32_t len)
{
	if (len == 0) return 0;

	#define BUF_SIZE 1024
    uint32_t address = IMAGE_FLASH_AREA_ADDRESS;
    uint32_t remaining = len;
    
    uint8_t *buffer = iot_calloc(BUF_SIZE);
    if (buffer != NULL) {
        memset(buffer, 0, BUF_SIZE);
    }
    if (buffer == NULL) {
        ESP_LOGI(TAG, "file: %s, function: %s, %d, malloc(1024) falied", __FILE__, __func__, __LINE__);
        return 0;
    }

    uint32_t image_crc32 = 0xFFFFFFFF; // 设置CRC32初始值
    while (remaining) {
        int size = (remaining > BUF_SIZE) ? BUF_SIZE : remaining;
        esp_err_t err = iot_image_read(address, buffer, size);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "iot read image data failed %d (address: %lu, size: %d)", err, address, size);
            break;
        }
        image_crc32 = calcu_crc32(image_crc32, buffer, size);
        remaining -= size;
        address += size;
    }
    free(buffer);

    return image_crc32;
}


 int can_ota_data_init(uint8_t type, uint32_t version, uint16_t filesize, uint16_t dev_id, uint32_t len)
 {
    int ret = -1;
    CanotaStatus = &can_ota_status;
    memset(&CanotaStatus->start, 0, sizeof(OtaStart));
    memset(&CanotaStatus->end,	 0, sizeof(OtaEnd));
    CanotaStatus->start.fileType = type;						 // 文件类型
    CanotaStatus->start.fileVersion = version;					 /* 文件完全版本 */
    CanotaStatus->start.fileSize = filesize;	 /* 获取文件大小,转换为Kbyte */
    CanotaStatus->start.dev_id = dev_id;
    CanotaStatus->is_only_stage = false;

    /*此处CRC并非升级文件的CRC校验，而是文件报文（即文件+CRC32+1A填充）整体的CRC校验*/
    uint32_t ota_data_crc = can_ota_data_crc(NULL, len);
    if(!ota_data_crc) goto __exit;
    
    CanotaStatus->end.fileCrc32 = ota_data_crc;
    ESP_LOGI(TAG, "ready ota data success(crc: 0x%08lx), ready for can-ota upgrade", CanotaStatus->end.fileCrc32);
    ret = 0;
 
 __exit:
	 return ret;
 }

 
#define ERR_DEV_ABORT				12 		// 设备终止传输

void ota_status_err_set(void)
{
    if ( gXmodem_Status.errcode == 0 ) {
        gXmodem_Status.errcode = ERR_DEV_ABORT;
    }

    top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].type    = gXmodem_Status.file_type;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].level   = 0;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].where   = 0;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].errCode = gXmodem_Status.errcode;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].pct     = 0;
	top_modbus_rd.Inv[reals.Addr_can_self].mod_reg00700_OTA.ota_group[0].isOta   = 1;

    current_ota_info_update(3, gXmodem_Status.file_type, 0, gXmodem_Status.errcode, gXmodem_Status.version);
}

/*
return:
输出串口返回发送报文
*/
uint8_t vXmodemClient(channel_type channel, const uint8_t *pdata, uint16_t plen) 
{
    uint8_t xresp_value = 0;
	static uint32_t err_set_pre_time = 0;

    if (gXmodem_Status.system_restart == 1) {
        gXmodem_Status.system_restart = 0;
        ESP_LOGI(__func__, "Delay 3s Prepare to restart system!");
        current_ota_info_update(2, gXmodem_Status.file_type, 100, 0, gXmodem_Status.version);
        vXmodemClientExit(channel);					// 退出xmodem
        
        uint8_t reset_cnt = 3;
        while ( reset_cnt-- )
        {
            reals.modbus_self_report_ble = 3;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        esp_restart();
        while(1);
    }

    if (channel != gXmodem_Status.used_chl) {
        if ( err_set_pre_time > 0 )
        {
            uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if((now_time - err_set_pre_time) >= 10000)
            {
                err_set_pre_time = 0;
                set_ota_pct_info(NULL, 0, 0, 0, 0, 0, 0);
                ESP_LOGW(TAG, "vXmodemClient : OTA status clean! ");
            }
        }
        
        return xresp_value;
    }

    if (gXmodem_Status.exit)
	{
        if (gXmodem_Status.exit == OTA_OBJECT_LCD)
        {
#ifdef CONFIG_FILE_DATA_OTA_ENABLE         
            if (Start_File_OTA_Task(gXmodem_Status.file_size * 1024) != ESP_OK) {
                ESP_LOGE(TAG, "File OTA Init fail");
				xresp_value = XMODEM_CAN;
				ota_status_err_set();
                err_set_pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            }
#else
            ESP_LOGE(TAG, "File OTA Unable");
            xresp_value = XMODEM_CAN;
            ota_status_err_set();
            err_set_pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif
        }
        else if (gXmodem_Status.exit == OTA_OBJECT_CAN_OTA)				// 启动CAN-OTA发送器
		{
#ifdef CONFIG_MCU_AUTO_UPDATE_IN_BOOT
            uint8_t index = back_image_type_to_index(gXmodem_Status.file_type);
            if ( index ) {
                if (((gXmodem_Status.file_size * 1024) <= IMAGE_CUSTOM_DATA_MAX) 
                    && ((gXmodem_Status.version / 100) % 100 != 0)) {
                    Start_Back_Image_Task(index, gXmodem_Status.file_type, gXmodem_Status.version, gXmodem_Status.file_size, gXmodem_Status.target_id, ota_data_len);
                }
            }
#endif
			if (can_ota_data_init(gXmodem_Status.file_type, gXmodem_Status.version, gXmodem_Status.file_size, gXmodem_Status.target_id, ota_data_len) != 0) 
			{			
				xresp_value = XMODEM_CAN;
				ota_status_err_set();
                err_set_pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
			}
			else
            {
#ifdef BAT_CAN_PROTOCOL_ALPHA_ENABLE                        
                if(!is_bms_alpha_update_protocol(ota_data.cmd.version/100)) // 第二代升级协议
#else
                if (1)
#endif
                {
                    ESP_LOGI(TAG, "can beta ota start! ");  
                    if (can_ota_begin(gXmodem_Status.target_id, gXmodem_Status.file_type, gXmodem_Status.version) != 0) 
                    {
                        ESP_LOGE(TAG, "can_ota_begin failed");
                        xresp_value = XMODEM_CAN;
                        ota_status_err_set();
                        err_set_pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        can_ota_status.ready_timeout = 15000;   // 15s
                    }
                    else
                    {
                        ESP_LOGI(TAG, "can_ota_begin success! ");
                    }

                }
                else // can alpha ota
                {
                    ESP_LOGI(TAG, "can alpha ota start! ");                        
                    if (0 != bms_ota_init(gXmodem_Status.file_type, gXmodem_Status.file_size, 0, get_addr_trace(gXmodem_Status.target_id), gXmodem_Status.version)) 
                    {
                        ESP_LOGE(TAG, "bms_ota_init failed");
                        xresp_value = XMODEM_CAN;
                        ota_status_err_set();
                        err_set_pre_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        can_ota_status.ready_timeout = 15000;   // 15s
                    }
                    else
                    {
                        ESP_LOGI(TAG, "bms_ota_init success! ");
                        IotSetData.dev_info_t.bms_ota_info.ota_state = OTA_STATE_START;
                        IotSetData.dev_info_t.bms_ota_info.ota_type = gXmodem_Status.file_type;
                        IotSetData.dev_info_t.bms_ota_info.version = gXmodem_Status.version;
                        IotSetData.dev_info_t.bms_ota_info.target_id = gXmodem_Status.target_id;
                        IotSetData.dev_info_t.bms_ota_info.file_size = gXmodem_Status.file_size;
                        
                        reals.SetDataWrFlag.sBit.update_status = 1;
                    }
                }
			}
        }
        
        if ( gXmodem_Status.errcode != 0 ) reals.modbus_self_report_ble = 3;
        
        vXmodemClientExit(channel);					// 退出xmodem
        return xresp_value;
    }

    if (gXmodem_Status.resend_count >= 20) {
        gXmodem_Status.resend_count = 0;
        gXmodem_Status.exit = 1;
        gXmodem_Status.errcode = OTA_ERR_NO_RESP;
        xresp_value = XMODEM_CAN;
        ESP_LOGI(TAG, "retry max %d xmodem client exit", gXmodem_Status.resend_count);
        return xresp_value;
    }

    switch (gXmodem_Status.step)
    {
        case 0:   // is nothings received
            if (!pdata || !plen) {
                if ((gXmodem_Status.time_count == 0) || (gXmodem_Status.time_count > 3000)) {
                    ESP_LOGW(TAG, "xresp_value = 'C'");
                    xresp_value = XMODEM_CRC;
                    gXmodem_Status.time_count = 1;
                    gXmodem_Status.resend_count++; // 发送次数增加 (每3s发送一次)
                }
                break;
            } else {
                gXmodem_Status.step = 1;
                gXmodem_Status.time_count = 0;
                gXmodem_Status.resend_count = 0;
				count_1k = 0;
				ota_data_len = 0;
            } // 这里没有break; 接收到信息后直接运行 step = 1

        case 1:
            if (!pdata || !plen) {
                if (gXmodem_Status.time_count >= 2000) {
                    xresp_value = XMODEM_NAK;
                    gXmodem_Status.resend_count++; // 发送次数增加 (每2s发送一次)
                    gXmodem_Status.time_count = 0;
                    ESP_LOGE(TAG, "pack timeout (NAK)");
                }
                break;
            }

            xresp_value = xmodem_client(gXmodem_Status.pfile, &gXmodem_Status, pdata, plen);
            if (xresp_value) { // 正常响应
                gXmodem_Status.time_count = 0;
                if (xresp_value == XMODEM_ACK) {
                    gXmodem_Status.resend_count = 0; // 重发次数清0
                } else if (xresp_value == XMODEM_CAN) {
                    gXmodem_Status.resend_count = 0;
                    gXmodem_Status.exit = 1;
                    if ( gXmodem_Status.errcode == 0 ) {
                        gXmodem_Status.errcode = OTA_ERR_ABORT;
                    }
                } else {
                    gXmodem_Status.resend_count++; // 重发次数增加
                }
            }

            break;

        default: gXmodem_Status.step = 0; break;
    }

    return xresp_value;
}

/* xmodem数据解析 */
static uint8_t xmodem_unpack(xmodem_struct * status, const uint8_t *payload, uint16_t len)
{
    if ((payload[1] + payload[2]) != 0xFF) {	// xmodem协议:payload[2]=~payload[1],校验包序值
        ESP_LOGI(TAG, "pack seq error != 0xFF");
        return XMODEM_NAK;
    }

    if (status->block_seq != payload[1]) {		// xmodem协议:校验包序列号
        ESP_LOGI(TAG, "pack seq error [recv %d, wait %d]", payload[1], status->block_seq);
        return XMODEM_NAK;
    }

    uint16_t vcrc16 = xm_calcu_crc16(payload + 3, len);	// xmodem协议:校验数据
    if (vcrc16 != ((payload[len + 3] << 8) | payload[len + 4])) {
        ESP_LOGE(TAG, "pack %d crc16 error", payload[1]);
        return XMODEM_NAK;
    }

    if (!status->block.payload) {	// 查看数据缓存是否为空
        ESP_LOGE(TAG, "memony error empty");
        return XMODEM_CAN;
    }

    if ((status->block.paylen + len) > BLOCK_BUFFER_SIZE) {	// 查询数据缓存是否越界
        ESP_LOGE(TAG, "memony error");
        return XMODEM_NAK;
    }

    // ESP_LOGI(TAG, "recv seq %d", status->block_seq);
	/* 本次数据存储到数据缓存中 */
    memcpy((status->block.payload + status->block.paylen), (payload + 3), len); // 4K写入一次
    status->block.paylen += len;
    status->block_seq ++;

    return XMODEM_ACK;	// xmodem协议:返回ACK
}

static esp_err_t xmodem_ota_crc_verify(uint32_t address, uint32_t len) {

    // 找出文件末尾追加的CRC32
    uint32_t crc32A = 0;
    uint32_t endAddr = address + len - 1;
    uint32_t remaining = len;
    for (int i = 0; i < 2048; i++) {
        uint8_t data = 0; 
        if (iot_image_read(endAddr, &data, 1) == ESP_OK) {
            if (data != 0x1A) {   // 排除文件内部填充的0x1A
                endAddr -= JUMP_BYTES;  // 跳过20字节填充信息
                remaining -= JUMP_BYTES;
                iot_image_read(endAddr + 1, (uint8_t *)&crc32A, 4);
                break;
            }
            endAddr -= 1;
            remaining -= 1;
        } else {
            ESP_LOGI(TAG, "image interval crc32 read failed");
            break;
        }
    }

    uint8_t *buffer = iot_calloc(1024);
    if (buffer != NULL) {
        memset(buffer, 0, 1024);
    }
    if (buffer == NULL) {
        ESP_LOGI(TAG, "file: %s, function: %s, %d, malloc(1024) falied", __FILE__, __func__, __LINE__);
        return ESP_ERR_NO_MEM;
    }

    uint32_t image_crc32 = 0xFFFFFFFF; // 设置CRC32初始值
    while (remaining) {
        int size = (remaining > 1024) ? 1024 : remaining;
        esp_err_t err = iot_image_read(address, buffer, size);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "iot read image data failed %d (address: %lu, size: %d)", err, address, size);
            break;
        }
        image_crc32 = calcu_crc32(image_crc32, buffer, size);
        remaining -= size;
        address += size;
    }
    free(buffer);

    image_crc32 = ntohl(image_crc32);
    if (crc32A != image_crc32) {
        ESP_LOGE(TAG, "image crc32 verify failed (file: 0x%04lX, new: 0x%04lX)", crc32A, image_crc32);
        return ESP_FAIL;
    }  
    ESP_LOGI(TAG, "image crc32 verify successfully: (file: 0x%04lx, new: 0x%04lx)", crc32A, image_crc32);
    return ESP_OK;
}

static uint8_t xmodem_client(FILE *pfile, xmodem_struct * status, const uint8_t *payload, uint16_t len)
{
	static uint8_t pre_packet_id = 0xff;
	uint8_t now_packet_id;
    uint8_t resp = 0;
    uint8_t pct = 0;

    if (!len || !payload) {
        ESP_LOGI(TAG, "!len || !payload");
        return 0;
    }

    gXmodem_Status.system_restart = 0;

    switch (payload[0])
    {
        case XMODEM_SOH:	// 上层应用发送128数据帧
                if ((DATA_LENGTH_128 + FILL_LENGTH) != len) {
                    ESP_LOGE(TAG, "XMODEM_SOH len error %d", len);
                    resp = XMODEM_NAK; // 数据长度错误
                    break;
                }

                if (status->block.paylen == BLOCK_BUFFER_SIZE) { // 4k
                    if (gXmodem_Status.is_esp_ota == 1) {
                        if (iot_ota_write(status->block.payload, status->block.paylen) != ESP_OK) {
                            ESP_LOGE(TAG, "soh,IOT Write binary file faied");
                            resp = XMODEM_CAN; // 写文件失败
                            status->errcode = OTA_ERR_FLASH_WRITE;
                            break;
                        }
                    } else {	// 每4k数据写入到文件中
						if (ota_data_len >= ((IMAGE_FLASH_AREA_MAX_LEN-BLOCK_BUFFER_SIZE))) {
							ESP_LOGW(TAG, "soh, binary file beyond ota buffer size, ota failed");
							resp = XMODEM_CAN;
                            status->errcode = OTA_ERR_SIZE;
							break;
						}
                        else {
                            if (iot_image_write((IMAGE_FLASH_AREA_ADDRESS + ota_data_len), status->block.payload, status->block.paylen) != ESP_OK)
                            {
                                ESP_LOGE(TAG, "Error: iot_image_write failed! err");
                                resp = XMODEM_CAN; // 写文件失败
                                status->errcode = OTA_ERR_FLASH_WRITE;
                                break;
                                
                            } else {
                                ota_data_len += status->block.paylen;
                            }
                        }
                        // fseek(pfile, 0, SEEK_END);
                        // if (fwrite(status->block.payload, 4, status->block.paylen/4, pfile) != (status->block.paylen/4)) {
                        //     ESP_LOGI(TAG, "xx.bin Write binary file faied");
                        //     resp = XMODEM_CAN; // 写文件失败
                        //     status->exit = 1;
                        //     break;
                        // }
                    }
                    status->block.paylen = 0;	// 重置缓存长度
                }

				now_packet_id = payload[1];
				if (now_packet_id != pre_packet_id)
				{
					count_1k++;
					pre_packet_id = now_packet_id;
				}
                pct = (count_1k * 100) / gXmodem_Status.file_size;
                ESP_LOGI(TAG, "received xmodem SOH data, progress: %d%%", pct);
#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE
                reals.system_ota.ota_total_pct = System_ota_total_pct_update(pct, true, reals.system_ota.ota_mcu_curr_count, reals.system_ota.ota_mcu_total_count, gXmodem_Status.is_esp_ota);
#endif
				pre_packet_id = now_packet_id;
                resp = xmodem_unpack(status, payload, DATA_LENGTH_128);	// 解析xmodem数据
                if (resp != XMODEM_ACK) {
                   break;
                }
            break;

        case XMODEM_STX:	// 上层应用发送1024数据帧
                if ((DATA_LENGTH_1024 + FILL_LENGTH) != len) {
                    ESP_LOGE(TAG, "XMODEM_STX len error %d", len);
                    resp = XMODEM_NAK; // 数据长度错误

                    break;
                }

                if (status->block.paylen == BLOCK_BUFFER_SIZE) {
                    if (gXmodem_Status.is_esp_ota == 1) {
						static uint32_t last_time = 0;
						if (last_time == 0) {
							last_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
						}
						uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
						last_time = now_time;
                        if (!status->block.payload || iot_ota_write(status->block.payload, status->block.paylen) != ESP_OK) {
                            ESP_LOGE(TAG, "stx, IOT Write binary file failed");
                            resp = XMODEM_CAN; // 写文件失败
                            status->errcode = OTA_ERR_FLASH_WRITE;
                            break;
                        }
                    } 
					else 
					{
						if (ota_data_len >= ((IMAGE_FLASH_AREA_MAX_LEN-BLOCK_BUFFER_SIZE))) 
						{
							ESP_LOGW(TAG, "stx, binary file beyond ota buffer size, ota failed");
							resp = XMODEM_CAN;
                            status->errcode = OTA_ERR_SIZE;
							break;
						}
						else 
						{
							if (iot_image_write((IMAGE_FLASH_AREA_ADDRESS + ota_data_len), status->block.payload, status->block.paylen) != ESP_OK)
							  {
								  ESP_LOGE(TAG, "Error: iot_image_write failed! err");
								  resp = XMODEM_CAN; // 写文件失败
                                  status->errcode = OTA_ERR_FLASH_WRITE;
								  break;
								  
							  } else {
								  ota_data_len += status->block.paylen;
							  }
						}
                    }
                    status->block.paylen = 0;
                }

				now_packet_id = payload[1];
				if (now_packet_id != pre_packet_id)
				{
					count_1k++;
					pre_packet_id = now_packet_id;
				}
                pct = (count_1k * 100) / gXmodem_Status.file_size;
				ESP_LOGI(TAG, "received xmodem STX data, progress: %d%%", pct);
#ifdef CONFIG_SYSTEM_OTA_PCT_CTRL_ENABLE
                reals.system_ota.ota_total_pct = System_ota_total_pct_update(pct, true, reals.system_ota.ota_mcu_curr_count, reals.system_ota.ota_mcu_total_count, gXmodem_Status.is_esp_ota);
#endif
                resp = xmodem_unpack(status, payload, DATA_LENGTH_1024);
                // printf("xmodem_client ---  xmodem_unpack count_1k==%d:\n",count_1k);
                //     esp_log_buffer_hex(TAG, can_ota_buffer + (uint16_t)62*1024, 1024);
                if (resp != XMODEM_ACK) {
                   break;
                }
            break;

        case XMODEM_ETX:	// 上层应用发送终止传输
            if (len != 1) {
                break;
            }
            ESP_LOGI(TAG, "received xmodem abort upgrade signal ETX(0x03)");
            status->block.paylen = 0;
            status->exit = 1;	// 退出xmoden升级

            // if (pfile) {
            //     fclose(pfile);
            //     pfile = NULL;
            // }

            resp = XMODEM_ACK;
            if ((status->is_esp_ota == 1) && (iot_ota_abort() != ESP_OK)) {
                resp = XMODEM_CAN;
            }
            break;

        case XMODEM_EOT:	// 上层应用发送传输完成
            if (len != 1) {
                break;
            }
            ESP_LOGI(TAG, "received xmodem upgrade completion signal EOT(0x04)");
            resp = XMODEM_ACK;

            if (status->block.paylen) {
                if (status->is_esp_ota == 1) { // 升级iot自身
                    if (iot_ota_write(status->block.payload, status->block.paylen) != ESP_OK) {
                        ESP_LOGE(TAG, "IOT Write binary file faied");
                        resp = XMODEM_CAN; // 写文件失败
                        status->errcode = OTA_ERR_FLASH_WRITE;
                    }
                } else {
					if (ota_data_len >= (IMAGE_FLASH_AREA_MAX_LEN-BLOCK_BUFFER_SIZE))
					{
						ESP_LOGW(TAG, "stx, binary file beyond ota buffer size, ota failed");
						resp = XMODEM_CAN; 
                        status->errcode = OTA_ERR_SIZE;
						break;
					}
                    else 
					{
                        if (iot_image_write((IMAGE_FLASH_AREA_ADDRESS + ota_data_len), status->block.payload, status->block.paylen) != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Error: iot_image_write failed! err");
                            resp = XMODEM_CAN; // 写文件失败
                            status->errcode = OTA_ERR_FLASH_WRITE;
                            break;
                            
                        } else {
                            ota_data_len += status->block.paylen;
                        }
                    }
                }
                status->block.paylen = 0;
            }

            if ((status->is_esp_ota == 1)) {
                if (iot_ota_end() != ESP_OK) {
                    resp = XMODEM_CAN;
				    status->errcode = OTA_ERR_FILE;
                    break;
                }
                // 升级iot自身完成,重启系统
                status->system_restart = 1;	
            } else {
	            if (xmodem_ota_crc_verify(IMAGE_FLASH_AREA_ADDRESS, ota_data_len) == ESP_OK) {
                    if ((status->file_type == IOT_LCD) || (status->file_type == IOT_LCD2)) {
                        // 升级文件命令完成后开始启动lcd升级
                        status->exit = OTA_OBJECT_LCD;  
                    } else {
                        // 升级文件命令完成后开始启动can-ota升级
                        status->exit = OTA_OBJECT_CAN_OTA;
                    }
				} else {
				    resp = XMODEM_CAN; // crc32错误
				    status->errcode = OTA_ERR_CRC32;
                }
            }

            break;

        default:            
            ESP_LOGE(TAG, "XMODEM Unkenown Data(payload[0] = %d) len error (len = %d)", payload[0], len);
            break;
    }

    return resp;
}

/**
 * @brief 获取esp模块升级状态
 *
 * @return 0-未升级, 1-正在升级
 */
int esp_ota_is_doing(void)
{
	return gXmodem_Status.is_esp_ota;
}

/**
 * @brief 获取xmodem服务状态
 *
 * @return FF-未运行, (0,1,2,3)-正在运行
 */
int xmodem_client_is_doing(void)
{
	return gXmodem_Status.used_chl;
}

///**
// * @brief 获取buffer地址
// *
// * @return 0-未升级, 1-正在升级
// */
//uint8_t* can_ota_buffer_addr_get(void)
//{
//	return can_ota_buffer;
//}

