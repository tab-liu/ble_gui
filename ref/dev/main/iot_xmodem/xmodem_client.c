#include <string.h>
#include "esp_log.h"
#include "filesystem.h"
#include "xmodem_client.h"
#include "comm_define.h"
#include "iot_ota.h"
#include "iot_wifi_init.h"
#include "uart_ota.h"
#include "uart_device_process.h"
#include "iot_mqtt.h"
#include "can_protocol.h"
#include "xmodem_transmitter.h"
#include "iot_period_task.h"
#include "ota_type.h"

#include <arpa/inet.h>

#define TAG "[XMODEM]"

#define     MAX_RESEND_CNT             5

#define     BLOCK_BUFFER_SIZE               4096

#define     DATA_LENGTH_128                 128
#define     DATA_LENGTH_1024                1024
#define     FILL_LENGTH                     5       // head + seq + ~seq + crc1 + crc2



xmodem_struct gXmodem_Status = {
    .exit = 0,
    .used_chl = OTA_CH_UNKOWN,
    .timer = NULL,
    .pfile = NULL,
    .crc32 = ~0,
};


#define	OTA_OBJECT_EXIT	1//
#define	OTA_OBJECT_CAN_OTA	2// 下级CAN 设备OTA
#define	OTA_OBJECT_UART_OTA	3// ,IOT模块直接uart关联的MCU
#define	OTA_OBJECT_WIFI_MESH_OTA	4//WIFI MESH网络 设备OTA


static uint8_t xmodem_client(FILE *pfile, xmodem_struct * status, const uint8_t *payload, uint16_t len);
static uint8_t xmodem_unpack(xmodem_struct * status, const uint8_t *payload, uint16_t len);
//EXT_RAM_BSS_ATTR uint8_t can_ota_buffer[1024*1024];
//EXT_RAM_BSS_ATTR uint8_t Ota_temp_buffer[1024];//1k read write flash

extern CanOtaStruct   CanotaStatus;
extern CanOtaStruct can_ota_status[DEV_MAIN_NODE_MAX];
static uint16_t count_1k;// int
static uint32_t ota_data_len;// int
// static char ota_bin_name[50];
static uint8_t ota_md_addr;//modbus slave address
ota_mode_t ota_mode;//=begin,0：不开启OTA只用于查看，1：需要开启单播OTA升级，2：需要开启半广播OTA升级，3：需要开启全广播OTA升级

/*
windy:
初始化xmodem升级变量

md_addr:modbus slave address

*/
uint8_t vXmodemCmdCheck(uint8_t md_addr, uint8_t channel)
{
    uint8_t i,j;

    uint8_t chkVer=0;
	if (g_self_data.mod_reg00700_OTA.ota_cmd.begin == 0)
    {
        //ESP_LOGE(TAG, "lxy debug : otaSTART = %u",g_device_data.ota_cmd.begin);
        return 0;
	}
	ota_mode_t _ota_mode = (ota_mode_t)g_self_data.mod_reg00700_OTA.ota_cmd.begin;//MicroInv[0]
	g_self_data.mod_reg00700_OTA.ota_cmd.begin = 0;//MicroInv[0]

//	if (cc1312_ota_is_doing()) {					// lcd正在升级
//		ESP_LOGW(TAG, "cc1312 ota is doing\n");
//		return 0;
//	}

#if 0
	if (CanotaStatus.ota_doing) {				// CAN正在升级
		ESP_LOGW(TAG, "can ota is doing\n");
		return 0;
	}

	// CAN正在升级
	for (int node = 0; node < DEV_MAIN_NODE_MAX; node++)
	{
		for (int i = 0; i < 32; i++)
		{
			if (can_ota_status[node].ota_doing) {
				ESP_LOGW(TAG, "can ota is doing, main_node: %d, sub_node:%d\n", node, i);
				return 0;
			}
		}
	}
	memset(can_ota_status, 0x00, sizeof(can_ota_status));
	memset(&CanotaStatus, 0x00, sizeof(CanotaStatus));
#endif

//	cc1312_ota_state_clear();

    for ( i = 0; i < 16; i++)
	{
        g_self_data.mod_reg00700_OTA.ota_group[i].type    = 0;
        g_self_data.mod_reg00700_OTA.ota_group[i].level   = 0;
        g_self_data.mod_reg00700_OTA.ota_group[i].where   = 0;
        g_self_data.mod_reg00700_OTA.ota_group[i].errCode = 0;
        g_self_data.mod_reg00700_OTA.ota_group[i].pct     = 0;
        g_self_data.mod_reg00700_OTA.ota_group[i].isOta   = 0;
    }


    uint8_t dev_type = (g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type) & 0xFF;	// 升级的设备类型
	uint8_t devId = 0xFF;			// 升级的设备地址(FF表示广播升级)
    uint16_t otaFileType=g_self_data.mod_reg00700_OTA.ota_cmd.type;

	for(j=0;j<6;j++)
	{
        ESP_LOGI(TAG, "vXmodemCmdCheck mod_reg01100_Inv_base.soft[%d].type=%d",j,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].type);
        ESP_LOGI(TAG, "vXmodemCmdCheck mod_reg01100_Inv_base.soft[%d].version=%lu",j,Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version);
    }
    ESP_LOGI(TAG, "vXmodemCmdCheck IOT.software_ver=%lu",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver);
    ESP_LOGI(TAG, "vXmodemCmdCheck D400S_software_ver=%lu",Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver);

    ESP_LOGW(TAG, "Group[%d : %d] type=%u, version=%lu, size=%u",
			 dev_type, devId,
			 g_self_data.mod_reg00700_OTA.ota_cmd.type,
			 g_self_data.mod_reg00700_OTA.ota_cmd.version,
			 g_self_data.mod_reg00700_OTA.ota_cmd.size);

	if ((dev_type < GROUP_INV) || (dev_type > GROUP_CHARGE))
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
            case GROUP_IOT  : devId = IOT_CAN_ADDR;   break;             /* 设备的目标ID - iot */
            case GROUP_LCD  : devId = 0x02;           break;             /* 设备的目标ID - LCD */
            case GROUP_CHARGE  : devId = D400S_CAN_ADDR;           break;             /* 设备的目标ID - D400S */
            default: return 0;
        }
    }
    //can_ota_status[0].ota_summary.type=otaFileType;//获取升级文件类型
    switch(otaFileType)
    {
        case DEVICE_IOT:
        {
            if((g_self_data.mod_reg00700_OTA.ota_cmd.version/100)==(Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver/100))
            {
                reals.preSoftVersion =Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg11000_IOT_info.software_ver;
                goto _final;
            }
            else if(reals.online_D400S_num)
            {
                for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
                {
                    if ((g_self_data.mod_reg00700_OTA.ota_cmd.version/100)==(Inv_D400S[node_id].mod_reg11000_IOT_info.software_ver/100))
                    {
                        ESP_LOGI(TAG,"Inv_D400S[%d].mod_reg11000_IOT_info.software_ver:%lu",node_id,Inv_D400S[node_id].mod_reg11000_IOT_info.software_ver);
                        reals.preSoftVersion =Inv_D400S[node_id].mod_reg11000_IOT_info.software_ver;
                        goto _final;
                    }
                }
            }

            goto _err;
        }
        break;
        case DEVICE_ARM:
        case DEVICE_DSP:
        case DEVICE_DC_HUB:
        {

            for(int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
            {
                for( i = 0; i < 6; i++)
                {
                    if (otaFileType == Inv_can[node_id].inv_data[0].inv_about.soft[i].type &&
                        ((g_self_data.mod_reg00700_OTA.ota_cmd.version/100)==(Inv_can[node_id].inv_data[0].inv_about.soft[i].version/100)))
                    {
                        reals.preSoftVersion = Inv_can[node_id].inv_data[0].inv_about.soft[i].version;
                        goto _final;
                    }
                }
            }
            ESP_LOGI(TAG,"dcdc_SoftwareType:%u",Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dcdc_SoftwareType);
            if((dev_type == GROUP_CHARGE)&&
            ((g_self_data.mod_reg00700_OTA.ota_cmd.version/100)==(Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dcdc_SoftwareVersion/100)))
            {
                reals.preSoftVersion = Inv_can[DEV_MAIN_NODE_MAX].d400s_data[D400S_MAX_NUM].d400s_common_info.dcdc_SoftwareVersion;
                goto _final;
            }
            goto _err;
        }
        break;
        case DEVICE_AC_HUB:
        {
            for (j = 0 ; j < 6 ; j++ )
            {
                ESP_LOGI(TAG, "mod_reg01100_Inv_base.soft[%d].type: %d,otaFileType:%d", j,(unsigned int)Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].type,otaFileType);
                if (Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].type==otaFileType)
                {
                    reals.preSoftVersion = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version;
                    goto _final;
                }
            }
            goto _err;
        }
        break;
        case DEVICE_BMS:
        {
            if(dev_type==GROUP_PACK)
            {
                // uint8_t ret=0;
                // uint8_t ret2=0;
                for (j = 0 ; j < 6 ; j++ )
                {
                    if ((Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version/100)==(g_self_data.mod_reg00700_OTA.ota_cmd.version/100))
                    {
                        ESP_LOGI(TAG,"DEVICE_BMS2 version:%lu -%lu",Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version,g_self_data.mod_reg00700_OTA.ota_cmd.version);
                        reals.preSoftVersion = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version;
                        goto _final;
                    }
                }

                for (int node = 0; node < DEV_MAIN_NODE_MAX; node++) {
                  for (j = 0; j < DEFAULT_PACK_TYPE_NUM; j++) {
                        uint8_t idx = node * DEFAULT_PACK_TYPE_NUM + j;
                        if ((Inv_Pack_Slave[idx].mod_reg06100_Pack_each.soft[0].version/100)==(g_self_data.mod_reg00700_OTA.ota_cmd.version/100))
                        {
                            ESP_LOGI(TAG,"DEVICE_PACK version:%lu -%lu",Inv_Pack_Slave[idx].mod_reg06100_Pack_each.soft[0].version,g_self_data.mod_reg00700_OTA.ota_cmd.version);
                            reals.preSoftVersion =Inv_Pack_Slave[idx].mod_reg06100_Pack_each.soft[0].version;
                            goto _final;
                        }
                    }
                    goto _err;
                }
            }
        }
        break;
        case DEVICE_PACK_BMS:
        {
            if(dev_type==GROUP_PACK)
            {
                for(i=0;i<3;i++)
                {
                    if(Inv[i].mod_reg06100_Pack_each.sn_code)
                    {
                        if((strcmp(Inv[i].mod_reg06100_Pack_each.type_ascii,"B300")==0)||(strcmp(Inv[i].mod_reg06100_Pack_each.type_ascii,"B300S")==0))
                        {
                            ESP_LOGW(TAG,"GROUP_PACK OTA TYPE B300 OR B300S :%s",Inv[i].mod_reg06100_Pack_each.type_ascii);
                            for (j = 0 ; j < 6 ; j++ )
                            {
                                if (Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].type==DEVICE_PACK_BMS)
                                {
                                    reals.preSoftVersion = Inv[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg01100_Inv_base.soft[j].version;
                                    goto _final;
                                }
                            }
                            goto _err;
                        }else if(strcmp(Inv[i].mod_reg06100_Pack_each.type_ascii,"B300K")==0)
                        {
                            reals.preSoftVersion = Inv[0].mod_reg06100_Pack_each.soft[0].version;
                            ESP_LOGW(TAG,"GROUP_PACK OTA TYPE B300K:%s",Inv[i].mod_reg06100_Pack_each.type_ascii);
                            goto _final;
                        }else if(strcmp(Inv[i].mod_reg06100_Pack_each.type_ascii,"B500K")==0)
                        {
                            reals.preSoftVersion = Inv[0].mod_reg06100_Pack_each.soft[0].version;
                            ESP_LOGW(TAG,"GROUP_PACK OTA TYPE B500K:%s",Inv[i].mod_reg06100_Pack_each.type_ascii);
                            goto _final;
                        }else
                        {
                            ESP_LOGW(TAG,"GROUP_PACK OTA TYPE ERR");
                            goto _err;
                        }
                    }

                }
            }
        }
        break;
        case DEVICE_DC_DC:
        {
                if ((Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15500_D400s_info.dcdc_SoftwareVersion/100)==(g_self_data.mod_reg00700_OTA.ota_cmd.version/100))
                {
                    ESP_LOGI(TAG,"DEVICE_DCDC version:%lu -%lu",Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15500_D400s_info.dcdc_SoftwareVersion,g_self_data.mod_reg00700_OTA.ota_cmd.version);
                    reals.preSoftVersion = Inv_D400S[(INV_MAX_NUM*DEV_MAIN_NODE_MAX)].mod_reg15500_D400s_info.dcdc_SoftwareVersion;
                    goto _final;
                }
        }
        break;
    }

    _final:
    ESP_LOGI(TAG, "g_self_data.mod_reg00700_OTA.ota_cmd.type %d", (unsigned int)g_self_data.mod_reg00700_OTA.ota_cmd.type);
    ESP_LOGI(TAG, "reals.preSoftVersion %ld", reals.preSoftVersion);
    ESP_LOGI(TAG, "software_ver == %ld, version == %ld", g_self_data.mod_reg11000_IOT_info.software_ver,g_self_data.mod_reg00700_OTA.ota_cmd.version);

	gXmodem_Status.is_esp_ota = 0;
	ota_md_addr = md_addr;
	ota_mode = _ota_mode;

	//...todo
	// ota_mode = OTA_SEMI_BROADCASST;
	// ota_mode = OTA_UNICAST;
	// devId = 0x10;
    ota_mode = OTA_FULL_BROADCAST;//windy debug force

    return vXmodemClientInit(devId,
							 g_self_data.mod_reg00700_OTA.ota_cmd.type,
							 g_self_data.mod_reg00700_OTA.ota_cmd.size,
							 g_self_data.mod_reg00700_OTA.ota_cmd.version,
                             g_self_data.mod_reg00700_OTA.ota_cmd.group.all,
							 channel);

    _err:
    return 0;
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

        iot_wifi_open(WIFI_MODE_STA); 			// 重新开启WiFi

        // ctrl_sim7600_access_service(true);
        gXmodem_Status.used_chl = OTA_CH_UNKOWN;
    }

    return 0;
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

    uint8_t *buffer = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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

     gXmodem_Status.crc32 = ntohl(image_crc32);
    if (crc32A !=  gXmodem_Status.crc32) {
        ESP_LOGE(TAG, "image crc32 verify failed (file: 0x%04lX, new: 0x%04lX)", crc32A,  gXmodem_Status.crc32 );
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "image crc32 verify successfully: (file: 0x%04lx, new: 0x%04lx)", crc32A,  gXmodem_Status.crc32 );
    return ESP_OK;
}
/*
return:
1-ok
0-fail
*/
uint8_t vXmodemClientInit(int id, uint8_t file_type, uint16_t size, uint32_t version,  uint16_t group, channel_type channel)
{
    // char path[255] = {0};

    if (file_type > TypeCnt) {
        ESP_LOGE(TAG, "file type unknown, file id = %d", file_type);
        return 0;
    }
    uint8_t dev_group = (group>>8) & 0xFF;	// 升级的设备类型
    ESP_LOGI(TAG,"dev_group:0x%x,group:0x%x",dev_group,group);
    // ESP_LOGI(TAG, "upgrade file type: %s, file id = %d", FileTypeString[file_type], file_type);
    // if (gXmodem_Status.pfile) 	// 关闭上次打开的文件,防止文件打开太多出问题
	// {
    //     fclose(gXmodem_Status.pfile);
    //     gXmodem_Status.pfile = NULL;
    // }

    gXmodem_Status.block.paylen = 0;
    if (gXmodem_Status.block.payload == NULL) {
        gXmodem_Status.block.payload = heap_caps_malloc(BLOCK_BUFFER_SIZE * sizeof(uint8_t), MALLOC_CAP_SPIRAM); // 4096缓存
    }

    if (gXmodem_Status.block.payload == NULL)	 //  内存分配失败
	{
        gXmodem_Status.exit = 1;
		ESP_LOGE(TAG, "upgrade block allocate failed, exit OTA");
        return 0;
    }

    gXmodem_Status.exit = 0;
    if ((file_type == IOT)&&(dev_group==GROUP_IOT)) { // IOT本身进行OTA升级 或者升级其他IOT
        gXmodem_Status.is_esp_ota = 1;
        ESP_LOGI(TAG, "xmodem client init is_esp_ota = 1");
        if (iot_ota_begin() != ESP_OK) {
            gXmodem_Status.exit = 0;
            return 0;
        }
    }
	else
	{ // 创建新的文件
        if (iot_image_erase(IMAGE_FLASH_AREA_ADDRESS, IMAGE_FLASH_AREA_MAX_LEN) != ESP_OK)
		{
	        gXmodem_Status.exit = 1;
	        return 0;
        }
        else{
			//实验表明，需要分区块读取，否则会触发看门狗重启：Task watchdog got triggered
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (iot_image_erase(IMAGE_FLASH_AREA_ADDRESS+IMAGE_FLASH_AREA_MAX_LEN, IMAGE_FLASH_AREA_MAX_LEN_EXTERN) != ESP_OK)
            {
                ESP_LOGE(TAG, "erase iot image 222 failed");
                gXmodem_Status.exit = 1;
	             return 0;
            }
        }
        ESP_LOGW(TAG,"gXmodem_Status.exit=%d",gXmodem_Status.exit);
	}

    if (gXmodem_Status.timer == NULL) {
        gXmodem_Status.timer = xTimerCreate("xmodem", pdMS_TO_TICKS(1000), pdTRUE, NULL, vXmodemTimeout);  // 创建超时定时器
        if (gXmodem_Status.timer == NULL) {
            ESP_LOGE(TAG, "xmodem timer create failed");
            gXmodem_Status.exit = 1;
            return 0;
        }
        xTimerStart(gXmodem_Status.timer, pdMS_TO_TICKS(100));
    }

    gXmodem_Status.crc32 = ~0;		// 初始化循环冗余校验初始值为0xffffffff
    gXmodem_Status.file_type = file_type;
    gXmodem_Status.file_size = size;  // 固件文件大小
    gXmodem_Status.version = version; // 固件版本号
    gXmodem_Status.target_id = id;
    gXmodem_Status.block_seq = 1;
    gXmodem_Status.used_chl = channel;
    gXmodem_Status.step = 0;
    gXmodem_Status.resend_count = 0;
    gXmodem_Status.time_count = 0;
    gXmodem_Status.group=group;
    ESP_LOGI(TAG,"SIZEOF USED_CHL:%u",sizeof(gXmodem_Status.used_chl));
    ESP_LOGI(TAG, "device information to be upgraded is: DevId=0x%x, Type=%d, Size:%ukb, Ver:%lu", id, file_type, size, version);
    if (OTA_CH_BLE_TO_SELF == gXmodem_Status.used_chl){
        ESP_LOGW(TAG, "channel is BLE, wifi close!!!");
        iot_wifi_close(WIFI_MODE_ALL); // 蓝牙升级时，关掉WiFi保证升级的稳定性和速度
    }
    vTaskDelay(pdMS_TO_TICKS(100));

#ifdef FUNC_ONECLICK_UPGRADE_EN
    if (!esp_ota_is_doing()) {
        Can_OneClickOta_Start_Bt(&can_ota_status[0]);
    }
#endif

    return 1;
}

/*
单播升级
type:OtaFileType ,modbus 701
*/
static int can_ota_unicast_begin(uint8_t id, uint8_t type, uint32_t version,uint8_t group)
{
	if (ota_md_addr == 0) return -1;

	uint8_t dev_idx = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_id;
	uint8_t node_id = ota_md_addr -1;

	if (node_id >= DEV_MAIN_NODE_MAX)
	{
		ESP_LOGE(TAG, "ota device node error, now node: %d, max node: %d", node_id, DEV_MAIN_NODE_MAX);
		return -1;
	}

	if (group == GROUP_INV)
	{
		inv_announce_struct *inv_ann = &Inv_can[node_id].inv_data[dev_idx].inv_announce;
		if (!inv_ann->online)
		{
			ESP_LOGE(TAG, "inv is offline, node: %d, devid: %d, exit ota", node_id, dev_idx);
			return -1;
		}
	}
	else
	{
		pack_announce_struct *pack_ann = &Inv_can[node_id].pack_data[0].pack_announce;
		if (!(pack_ann->online & (1 << dev_idx)))
		{
			ESP_LOGE(TAG, "pack is offline, node: %d, devid: %d, exit ota", node_id, dev_idx);
			return -1;
		}
	}

//	ESP_LOGW(TAG, "begin to unicast upgrade, node: %d, addr: %02x, file type:%s, version: %u, dev_idx: %d",
//			 node_id, id, FileTypeString[type], version, dev_idx);
	memcpy(&can_ota_status[node_id], &CanotaStatus, sizeof(CanotaStatus));
	CanOtaSender_Begin(id, type, version, &can_ota_status[node_id],group);
	return 0;
}

/*
半广播升级
type:OtaFileType ,modbus 701

*/
static int can_ota_semi_broadcast_begin(uint8_t id, uint8_t type, uint32_t version,uint8_t group)
{
	int ret = -1;
	uint8_t dev_idx = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_id;

	/* 轮询检测哪些节点需要CAN-OTA */
	for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
	{
		int online = 0;
		if (group == GROUP_INV) {
			online = Inv_can[node_id].inv_data[0].inv_announce.online & (1 << dev_idx);
		}
		else {
			online = Inv_can[node_id].pack_data[0].pack_announce.online & (1 << dev_idx);
		}

		/* 设备在线该节点启动CAN-OTA */
		if(online)
		{
//			ESP_LOGW(TAG, "begin to semi-broadcast upgrade, node: %d, addr: %02x, file type:%s, version: %u, dev_idx: %d",
//					 node_id, id, FileTypeString[type], version, dev_idx);
			memcpy(&can_ota_status[node_id], &CanotaStatus, sizeof(CanotaStatus));
			CanOtaSender_Begin(id, type, version, &can_ota_status[node_id],group);
			ret = 0;
		}
	}

	return ret;
}

/*
全广播升级

*/
static int can_ota_full_broadcast_begin(uint8_t id, uint8_t type, uint32_t version,uint8_t group)
{
	int ret = -1;
	// uint8_t group = (g_device_data.ota_cmd.group >> 8) & 0xFF;
    // uint8_t group = g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type;	// 升级的组类型
    // uint8_t group = Inv[DEV_MAIN_NODE_MAX].mod_reg00700_OTA.ota_cmd.group.dev_type;	// 升级的组类型

    ESP_LOGI(TAG, "get in can_ota_full_broadcast_begin");
    for(uint8_t i = 0; i < DEV_MAIN_NODE_MAX; i++)
    {
        ESP_LOGI(TAG, "Inv_can[%d].inv_data[0].inv_announce.online:%d, Inv_can[%d].inv_data[0].inv_base.inv_online:%d",
            i,Inv_can[i].inv_data[0].inv_announce.online,i,Inv_can[i].inv_data[0].inv_base.inv_online);
    }


	//uint8_t group = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_type;//wen
	//uint8_t dev_idx = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_id;

	/* 轮询检测哪些节点需要CAN-OTA */
	// for (int node_id = 0; node_id < DEV_MAIN_NODE_MAX; node_id++)
    int node_id = 0;//对于ac380 由于是全广播 不再区分节点
	{
		int online = 0;
		// if (group == GROUP_INV)
		// {
		// 	for(int i = 0; i < INV_MAX_NUM; i++)	// 查询节点上是否有逆变设备在线
		// 	{
		// 		if((online = (Inv_can[node_id].inv_data[0].inv_announce.online & (1 << i))))
        //         {
		// 			break;
		// 		}
		// 	}
		// }
		// else
		// {
		// 	for(int i = 0; i < PACK_MAX_NUM; i++)	// 查询节点上是否有PACK设备在线
		// 	{
		// 		if((online = (Inv_can[node_id].pack_data[0].pack_announce.online & (1 << i))))
        //         {
		// 			break;
		// 		}
		// 	}
		// }

        // ESP_LOGI(TAG, "online:%d, node_id:%d",online,node_id);//test OTA
        ESP_LOGI(TAG, "reals.online_Inv_num:%d, node_id:%d",reals.online_Inv_num,node_id);//test OTA
		/* 设备在线该节点启动CAN-OTA */
		// if(online)
        if(reals.online_Inv_num)
		{
			ESP_LOGI(TAG, "begin to full-broadcast upgrade, node: %d, addr: %02x, file type:%d, version: %lu",
					 node_id, id, type, version);//FileTypeString[type]
			memcpy(&can_ota_status[node_id], &CanotaStatus, sizeof(CanotaStatus));
			CanOtaSender_Begin(id, type, version, &can_ota_status[node_id],group);
			ret = 0;
		}
        else
        {
            ESP_LOGE(TAG, "no Inv device online, exit can-ota");
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
    // uint8_t group = g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type;	// 升级的组类型
    // uint8_t group = Inv[DEV_MAIN_NODE_MAX].mod_reg00700_OTA.ota_cmd.group.dev_type;	// 升级的组类型

    //modify by yjh: APP下发升级IoT的ota_cmd_group.dev_type为0
    // uint8_t group = Inv[DEV_MAIN_NODE_MAX].mod_reg00700_OTA.ota_cmd_group.dev_type;	// 升级的组类型 720由Inv_WR.mod_reg00700_OTA.ota_cmd.group赋值而来
    uint8_t group = (g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type) & 0xFF;

    for(int i = 0; i <= DEV_MAIN_NODE_MAX; i++)
    {
        ESP_LOGI(TAG, "111--- Inv[%d].mod_reg00700_OTA.ota_cmd.group.dev_type:%d",i,Inv[i].mod_reg00700_OTA.ota_cmd.group.dev_type);
    }
    for(int i = 0; i <= DEV_MAIN_NODE_MAX; i++)
    {
        ESP_LOGI(TAG, "111--- Inv[%d].mod_reg00700_OTA.ota_cmd_group.dev_type:%d",i,Inv[i].mod_reg00700_OTA.ota_cmd_group.dev_type);
    }
    ESP_LOGI(TAG, "111--- Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type :%d",Inv_WR.mod_reg00700_OTA.ota_cmd.group.dev_type);
    ESP_LOGI(TAG, "111--- g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type :%d", g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type);
    ESP_LOGI(TAG, "group(%d), ota_mode:%d, id:%d, type:%d, version:%ld", group, ota_mode, id, type, version);

	//uint8_t group = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_type;//wen
	//uint8_t dev_idx = Inv[0].mod_reg00700_OTA.ota_cmd.group.dev_id;

	/* 升级类型错误退出 */
	if((group != GROUP_INV) && (group != GROUP_PACK) && (group != GROUP_IOT)&&(group !=GROUP_CHARGE))
	{
		ESP_LOGE(TAG, "group(%d) is not inv or pack, exit can-ota", group);
		goto _exit;
	}

	switch (ota_mode)
	{
	case OTA_UNICAST:				// 单播升级
		ret = can_ota_unicast_begin(id, type, version,group);
		break;

	case OTA_SEMI_BROADCASST:		// 半广播升级
		ret = can_ota_semi_broadcast_begin(id, type, version,group);
		break;

	case OTA_FULL_BROADCAST:		// 全广播升级
		ret = can_ota_full_broadcast_begin(id, type, version,group);
		break;

	default:
		break;
	}

_exit:
	if (ret != 0) {
        gXmodem_Status.firmware_resend_step=0;
		ESP_LOGW(TAG, "no device upgrade, exit can-ota");
	}

	return ret;
}

/*
从flash中读取全部待OTA文件，判断CRC
从IOTAPPx备份区获取
 data= NULL

 */
static uint32_t can_ota_data_crc_From_Appx_backup_Part(uint8_t *data,  uint32_t len)
{
	if (len == 0) return 0;//data == NULL ||

	#define BUF_SIZE 	1024
	uint8_t buf[BUF_SIZE];

	uint32_t crc32 = (uint32_t)(~0);
    uint32_t offset = 0;
    uint32_t remaining = len - len % BUF_SIZE;

    while (remaining)
	{
	    if ( ESP_OK != iot_image_read_From_Appx_backup_Part((IOT_FIRMWARE_BEGIN_ADDRESS + offset), buf, BUF_SIZE) )
		{
			vTaskDelay(pdMS_TO_TICKS(5));	//
		    if ( ESP_OK != iot_image_read_From_Appx_backup_Part((IOT_FIRMWARE_BEGIN_ADDRESS + offset), buf, BUF_SIZE) )
			{
				ESP_LOGE(TAG, " %s  can_ota_data_crc:read image head flash failed", __func__);
		    }
	    }
		vTaskDelay(pdMS_TO_TICKS(5));	//

//		memcpy(buf, &data[offset], BUF_SIZE);
        crc32 = calcu_crc32(crc32, buf, BUF_SIZE);
        offset += BUF_SIZE;
        remaining -= BUF_SIZE;
    }

    remaining = len % BUF_SIZE;
    if (remaining)
	{
//		memcpy(buf, &data[offset], remaining);
		if ( ESP_OK != iot_image_read_From_Appx_backup_Part((IOT_FIRMWARE_BEGIN_ADDRESS + offset), buf, BUF_SIZE) )
		{
			vTaskDelay(pdMS_TO_TICKS(5));	//
			if ( ESP_OK != iot_image_read_From_Appx_backup_Part((IOT_FIRMWARE_BEGIN_ADDRESS + offset), buf, BUF_SIZE) )
			{
				ESP_LOGE(TAG, " %s	can_ota_data_crc:read image head flash failed", __func__);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(5));	//

        crc32 = calcu_crc32(crc32, buf, remaining);
    }

    return crc32;
}


/*
从flash中读取全部待OTA文件，判断CRC
 从IOTcustom_data区获取

 data= NULL

 */
static uint32_t can_ota_data_crc(uint8_t *data,  uint32_t len)
{
	if (len == 0) return 0;//data == NULL ||

	#define BUF_SIZE 	1024
	uint8_t buf[BUF_SIZE];

	uint32_t crc32 = (uint32_t)(~0);
    uint32_t offset = 0;
    uint32_t remaining = len - len % BUF_SIZE;

    while (remaining)
	{
	    if ( ESP_OK != iot_image_read((IMAGE_FLASH_AREA_ADDRESS + offset), buf, BUF_SIZE) )
		{
			vTaskDelay(pdMS_TO_TICKS(5));	//
		    if ( ESP_OK != iot_image_read((IMAGE_FLASH_AREA_ADDRESS + offset), buf, BUF_SIZE) )
			{
				ESP_LOGE(TAG, " %s  can_ota_data_crc:read image head flash failed", __func__);
		    }
	    }
		vTaskDelay(pdMS_TO_TICKS(5));	//

//		memcpy(buf, &data[offset], BUF_SIZE);
        crc32 = calcu_crc32(crc32, buf, BUF_SIZE);
        offset += BUF_SIZE;
        remaining -= BUF_SIZE;
    }

    remaining = len % BUF_SIZE;
    if (remaining)
	{
//		memcpy(buf, &data[offset], remaining);
		if ( ESP_OK != iot_image_read((IMAGE_FLASH_AREA_ADDRESS + offset), buf, BUF_SIZE) )
		{
			vTaskDelay(pdMS_TO_TICKS(5));	//
			if ( ESP_OK != iot_image_read((IMAGE_FLASH_AREA_ADDRESS + offset), buf, BUF_SIZE) )
			{
				ESP_LOGE(TAG, " %s	can_ota_data_crc:read image head flash failed", __func__);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(5));	//

        crc32 = calcu_crc32(crc32, buf, remaining);
    }

    return crc32;
}
 int can_ota_data_init(uint8_t type, uint32_t version, uint16_t filesize, uint16_t dev_id, uint32_t len)
 {
	 int ret = -1;
	 memset(&CanotaStatus.start, 0, sizeof(OtaStart));
	 memset(&CanotaStatus.end,	 0, sizeof(OtaEnd));
	 CanotaStatus.start.fileType = type;						 // 文件类型
	 CanotaStatus.start.fileVersion = version;					 /* 文件完全版本 */
	 CanotaStatus.start.fileSize = filesize;	 /* 获取文件大小,转换为Kbyte */
	 CanotaStatus.start.dev_id = dev_id;
     uint8_t group=dev_id&0xff;
     ESP_LOGI(TAG,"can_ota_data_init group:%d len:%lu type:%d filesize:%u",group,len,type,filesize);

	 /* 直接使用静态内存做ota文件数据缓存 */
//	 CanotaStatus.ota_data = can_ota_buffer;
	 // http校验长度
//	 if(http_ota_doing){
//
////		 if ((iot_image_read((IMAGE_FLASH_AREA_ADDRESS + 0), CanotaStatus.ota_data, len)) != 0) {
////			 ESP_LOGE(TAG, "Failed to read data into CanotaStatus.ota_data");
////			 goto __exit;
////		 }
//		 ota_data_len = len;
//	 }

	 uint32_t ota_data_crc=0;
	if((IOT == type)&&(GROUP_IOT==group))//IOT
	{
		ota_data_crc = can_ota_data_crc_From_Appx_backup_Part(NULL, len);//CanotaStatus.ota_data,ota_data_len
	}
	else
	{
		ota_data_crc = can_ota_data_crc(NULL, len);//CanotaStatus.ota_data,ota_data_len
	}

	 if(ota_data_crc == gXmodem_Status.crc32)
	 {
		ESP_LOGE(TAG, "ota data stored in memory crc error, now: 0x%08lx, old: 0x%08lx", ota_data_crc, gXmodem_Status.crc32);
		 goto __exit;
	 }

	 CanotaStatus.end.fileCrc32 = ota_data_crc;
	 ESP_LOGI(TAG, "ready ota data success, ready for can-ota upgrade ota_data_crc:%lu",ota_data_crc);
	 ret = 0;

 __exit:
#ifdef FUNC_ONECLICK_UPGRADE_EN
    Can_OneClickOta_End();
#endif
	 return ret;
 }
#define ERR_DEV_ABORT				12 		// 设备终止传输

void ota_status_err_set(void)
{
    g_self_data.mod_reg00700_OTA.ota_group[0].type    = gXmodem_Status.file_type;
	g_self_data.mod_reg00700_OTA.ota_group[0].level   = 0;
	g_self_data.mod_reg00700_OTA.ota_group[0].where   = 0;
	g_self_data.mod_reg00700_OTA.ota_group[0].errCode = ERR_DEV_ABORT;
	g_self_data.mod_reg00700_OTA.ota_group[0].pct     = 0;
	g_self_data.mod_reg00700_OTA.ota_group[0].isOta   = 1;
}

/*
return:
输出串口返回发送报文
*/
uint8_t Xmodem_Client_top(channel_type channel, const uint8_t *pdata, uint16_t plen)
{
    uint8_t xresp_value = 0;

    if (gXmodem_Status.system_restart == 1) {
        gXmodem_Status.system_restart = 0;
        ESP_LOGI(TAG, "Delay 500ms Prepare to restart system!");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    ESP_LOGI(TAG,"exit:%d,gXmodem_Status.used_chl:%d,channel:%d,step:%d",gXmodem_Status.exit,gXmodem_Status.used_chl,channel,gXmodem_Status.step);
    if (channel != gXmodem_Status.used_chl) {
        return xresp_value;
    }

    if (gXmodem_Status.exit)
	{
        vXmodemClientExit(channel);					// 退出xmodem

        if (gXmodem_Status.exit == OTA_OBJECT_CAN_OTA)				// 启动CAN-OTA发送器
		{
            ESP_LOGI(TAG, "XMODEM CLIENT TOP exit OTA_OBJECT_CAN_OTA");
			if (can_ota_data_init(gXmodem_Status.file_type, gXmodem_Status.version, gXmodem_Status.file_size, ((gXmodem_Status.group>>8)&0xff), ota_data_len) != 0)
			{
				xresp_value = XMODEM_CAN;
				ota_status_err_set();
			}
			else if (can_ota_begin(gXmodem_Status.target_id, gXmodem_Status.file_type, gXmodem_Status.version) != 0) {
				xresp_value = XMODEM_CAN;
				ota_status_err_set();
			}
        }
        else if (gXmodem_Status.exit == OTA_OBJECT_WIFI_MESH_OTA)
        {
            ESP_LOGW(TAG, "Xmodem_Client_top channel error");
            ota_set_t ota_set;
            ota_set.file_type = gXmodem_Status.file_type;
            ota_set.ver_low = gXmodem_Status.version & 0xffff;
            ota_set.ver_high = (uint32_t)gXmodem_Status.version >> 16;
            ota_set.file_size = gXmodem_Status.file_size;
            ota_set.group.dev_type = (g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_type) & 0xFF;

            {
                ota_set.start_flag = 1; //默认
            }

            ota_set.group.dev_id = g_self_data.mod_reg00700_OTA.ota_cmd.group.dev_id & 0xFF;

            ESP_LOGW(TAG, "uart : Group[%d : %d] type=%u, version=%lu, size=%u",
            ota_set.group.dev_type, ota_set.group.dev_id,
            ota_set.file_type,
            gXmodem_Status.version,
            ota_set.file_size);
            ESP_LOGW(TAG, "Xmodem_Client_top channel error");
            if (uart_ota_start(&ota_set, NULL, NULL, 1) != 0) {
                xresp_value = XMODEM_CAN;
                ota_status_err_set();
                iot_mqtt_start(NETIF_TYPE_WIFI_STA);
            }
            gXmodem_server_Status.used_chl =OTA_CH_SELF_TO_WIFI_MESH;

        }
        return xresp_value;
    }

    if (gXmodem_Status.resend_count >= 20) {
        gXmodem_Status.resend_count = 0;
        gXmodem_Status.exit = 1;
        xresp_value = XMODEM_CAN;
        ESP_LOGI(TAG, "retry max %d xmodem client exit", gXmodem_Status.resend_count);
        return xresp_value;
    }

    switch (gXmodem_Status.step)
    {
        case 0:   // is nothings received
            // if (!pdata || !plen)
            uint16_t startAddress  = pdata[2]<<8 | pdata[3]; // 写入寄存器地址
            if(startAddress == 700)
            {
                if ((gXmodem_Status.time_count == 0) || (gXmodem_Status.time_count > 3000))
                {
                    ESP_LOGW(TAG, "xresp_value = 'C'");
                    xresp_value = XMODEM_CRC;
                    gXmodem_Status.time_count = 1;
                    gXmodem_Status.resend_count++; // 发送次数增加 (每3s发送一次)
                }
                break;
            }
            else
            {
                gXmodem_Status.step = 1;
                gXmodem_Status.time_count = 0;
                gXmodem_Status.resend_count = 0;
                count_1k = 0;
                ota_data_len = 0;
                __attribute__((fallthrough)); // 这里没有break; 接收到信息后直接运行 step = 1
            }

        case 1:
            if (!pdata || !plen)
            {
                if (gXmodem_Status.time_count >= 2000)
                {
                    xresp_value = XMODEM_NAK;
                    gXmodem_Status.resend_count++; // 发送次数增加 (每2s发送一次)
                    gXmodem_Status.time_count = 0;
                    ESP_LOGI(TAG, "pack timeout (NAK)");
                }
                break;
            }

            xresp_value = xmodem_client(gXmodem_Status.pfile, &gXmodem_Status, pdata, plen);
            ESP_LOGI(TAG,"gXmodem_Status.system_restart:%d,xresp_value:%d",gXmodem_Status.system_restart,xresp_value);
            if (xresp_value)
            { // 正常响应
                gXmodem_Status.time_count = 0;
                if (xresp_value == XMODEM_ACK)
                {
                    gXmodem_Status.resend_count = 0; // 重发次数清0
                }
                else if (xresp_value == XMODEM_CAN)
                {
                    gXmodem_Status.resend_count = 0;
                    gXmodem_Status.exit = 1;
                }
                else
                {
                    gXmodem_Status.resend_count++; // 重发次数增加
                }
            }

            if (gXmodem_Status.exit != 0)
            {
                // if (gXmodem_Status.pfile)
                // {
                //     fclose(gXmodem_Status.pfile);
                //     gXmodem_Status.pfile = NULL;
                // }
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

static uint8_t xmodem_client(FILE *pfile, xmodem_struct * status, const uint8_t *payload, uint16_t len)
{
	static uint8_t pre_packet_id = 0xff;
	uint8_t now_packet_id;
    uint8_t resp = 0;

    if (!len || !payload) {
        ESP_LOGI(TAG, "!len || !payload");
        return 0;
    }

    // if (gXmodem_Status.is_esp_ota != 1) {
    //     if (!pfile) { /* 未创建文件 */
    //         return 0;
    //     }
    // }

    gXmodem_Status.system_restart = 0;
    ESP_LOGW(TAG, "xmodem client: func:%d", payload[0]);
    //printf("xmodem_client recv data len: %d, value:\n", len);
	//esp_log_buffer_hex(TAG, payload, len);
    switch (payload[0])
    {
        case XMODEM_SOH:	// 上层应用发送128数据帧
                if ((DATA_LENGTH_128 + FILL_LENGTH) != len) {
                    ESP_LOGE(TAG, "XMODEM_SOH len error %d", len);
                    resp = XMODEM_NAK; // 数据长度错误
                    break;
                }

                // if (status->block.paylen >= 128) {
				// 	/* 保存循环校验升级文件数据的crc值,即本机校验的文件crc值 */
                //     gXmodem_Status.crc32 = calcu_crc32(gXmodem_Status.crc32,
                //                                        status->block.payload + (status->block.paylen-128), 128);
                // }

                if (status->block.paylen == BLOCK_BUFFER_SIZE) { // 4k
                    if (gXmodem_Status.is_esp_ota == 1) {
                        ESP_LOGI(TAG, "xmodem esp ota write 4k data");
                        if (iot_ota_write(status->block.payload, status->block.paylen) != ESP_OK) {
                            ESP_LOGE(TAG, "IOT Write binary file faied1");
                            resp = XMODEM_CAN; // 写文件失败
                            status->exit = 1;
                            break;
                        }else
                        {
                            ota_data_len += status->block.paylen;
                        }
                    } else {	// 每4k数据写入到文件中
						if (ota_data_len >= ((IMAGE_FLASH_AREA_MAX_LEN+IMAGE_FLASH_AREA_MAX_LEN_EXTERN-BLOCK_BUFFER_SIZE))) {
							ESP_LOGW(TAG, "soh, binary file beyond ota buffer size, ota failed");
							resp = XMODEM_CAN; // 写文件失败
							status->exit = 1;
							break;
						}
                        else {
                            if (iot_image_write((IMAGE_FLASH_AREA_ADDRESS + ota_data_len), status->block.payload, status->block.paylen) != ESP_OK)
                            {
                                ESP_LOGE(TAG, "Error: iot_image_write failed! err");
                                resp = XMODEM_CAN; // 写文件失败
                                status->exit = 1;
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
				ESP_LOGI(TAG, "received xmodem SOH data, seq: %d now_packet_id:%d", count_1k,now_packet_id);
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

                // if (status->block.paylen >= 1024) {
                //     gXmodem_Status.crc32 = calcu_crc32(gXmodem_Status.crc32,
                //                                        status->block.payload+(status->block.paylen-1024), 1024);
                // }

                if (status->block.paylen == BLOCK_BUFFER_SIZE) {
                    if (gXmodem_Status.is_esp_ota == 1) {
						static uint32_t last_time = 0;
						if (last_time == 0) {
							last_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
						}
						uint32_t now_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
//						ESP_LOGW(TAG, "write 4k data to ota partition, last-->now time elapse: %u", now_time-last_time);
						last_time = now_time;
                        if (!status->block.payload || iot_ota_write(status->block.payload, status->block.paylen) != ESP_OK) {
                            ESP_LOGE(TAG, "IOT Write binary file faied2");
                            resp = XMODEM_CAN; // 写文件失败
                            status->exit = 1;
                            break;
                        }else
                        {
                        	ota_data_len += status->block.paylen;
                        }
                    }
					else
					{
						if (ota_data_len >= ((IMAGE_FLASH_AREA_MAX_LEN+IMAGE_FLASH_AREA_MAX_LEN_EXTERN-BLOCK_BUFFER_SIZE)))
						{
							ESP_LOGW(TAG, "stx, binary file beyond ota buffer size, ota failed");
							resp = XMODEM_CAN; // 写文件失败
							status->exit = 1;
							break;
						}
						else
						{
							if (iot_image_write((IMAGE_FLASH_AREA_ADDRESS + ota_data_len), status->block.payload, status->block.paylen) != ESP_OK)
							  {
								  ESP_LOGE(TAG, "Error: iot_image_write failed! err");
								  resp = XMODEM_CAN; // 写文件失败
								  status->exit = 1;
								  break;

							  } else {
								  ota_data_len += status->block.paylen;
							  }

						}

                        // fseek(pfile, 0, SEEK_END);
                        // if (!status->block.payload || fwrite(status->block.payload, 4, status->block.paylen/4, pfile) != status->block.paylen/4) {
                        //     ESP_LOGI(TAG, "xxx.bin Write binary file faied");
                        //     resp = XMODEM_CAN; // 写文件失败
                        //     status->exit = 1;
                        //     break;
                        // }
                    }
                    status->block.paylen = 0;
                }

				now_packet_id = payload[1];
				if (now_packet_id != pre_packet_id)
				{
					count_1k++;
					pre_packet_id = now_packet_id;
				}
				ESP_LOGI(TAG, "received xmodem STX data, seq: %d", count_1k);
                resp = xmodem_unpack(status, payload, DATA_LENGTH_1024);
                printf("xmodem_client ---  xmodem_unpack count_1k==%d resp==%d :\n",count_1k,resp);
                //esp_log_buffer_hex(TAG, can_ota_buffer + (uint16_t)62*1024, 1024);
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
            ESP_LOGI(TAG, "received xmodem upgrade completion signal EOT(0x04) status->block.paylen:%d",status->block.paylen);
            resp = XMODEM_ACK;
            status->exit = 1;

            if (status->block.paylen) {
                uint32_t i;
//                 for (i = 0; i < status->block.paylen; i++) {
// 					/* xmodem协议:如果发送的数据不满128字节或者1024字节,使用0x1A填充
// 					 * 找到填充数据的位置 */
//                     if (status->block.payload[status->block.paylen-i-1] != 0x1A) {
//                         break;
//                     }
//                 }

//                 vTaskDelay(pdMS_TO_TICKS(100));

// 				/* 升级文件末尾人为加上JUMP_BYTES个字节数据 */
//                 uint32_t remaining = status->block.paylen-i-JUMP_BYTES;

// 				/* 得到升级文件的crc校验值 */
//                 uint32_t crc32B = *(uint32_t *)(&status->block.payload[remaining]);
//                 crc32B = ntohl(crc32B);

// 				/* 每接收完成1024就会计算一次crc32,所以这里只计算最后一次,即得到文件的crc值 */
//                 ESP_LOGI(TAG,"eot calcu_crc32");
//                 gXmodem_Status.crc32 = calcu_crc32(gXmodem_Status.crc32, status->block.payload + remaining - remaining%1024, remaining%1024);
//                 ESP_LOGI(TAG,"status->block.paylen:%u,remaining:%lu,i:%lu",status->block.paylen,remaining,i);
// //				ESP_LOGW(TAG, "last packet length: %d, data crc: %08x", remaining, gXmodem_Status.crc32);
//                 if (gXmodem_Status.crc32 != crc32B) {	// 比较本地计算的文件crc值与上层应用下发的文件crc值
//                     resp = XMODEM_CAN; // crc32错误
//                     ESP_LOGE(TAG, "file crc32 error(0x%lu:0x%lu)", gXmodem_Status.crc32, crc32B);
//                     break;
//                 }

                if (status->is_esp_ota == 1) { // 升级iot自身
                    if (iot_ota_write(status->block.payload, status->block.paylen) != ESP_OK) {
                        ESP_LOGE(TAG, "IOT Write binary file faied3");
                        resp = XMODEM_CAN; // 写文件失败
                    }else{
                        ota_data_len += status->block.paylen;
                    }
                } else {
					if (ota_data_len >= (IMAGE_FLASH_AREA_MAX_LEN+IMAGE_FLASH_AREA_MAX_LEN_EXTERN-BLOCK_BUFFER_SIZE))
					{
						ESP_LOGW(TAG, "stx, binary file beyond ota buffer size, ota failed");
						resp = XMODEM_CAN; // 写文件失败
						status->exit = 1;
						break;
					}
                    else
					{
                        if (iot_image_write((IMAGE_FLASH_AREA_ADDRESS + ota_data_len), status->block.payload, status->block.paylen) != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Error: iot_image_write failed! err");
                            resp = XMODEM_CAN; // 写文件失败
                            status->exit = 1;
                            break;

                        } else {
                            ota_data_len += status->block.paylen;
                        }
                    }
                }
                status->block.paylen = 0;
            }
            ESP_LOGE(TAG, "xmodem client: online_Iot_num is %d! status->file_type:%d", reals.online_Iot_num,status->file_type);
            //IoT升级，且无并机IoT时，升级自身
            if ((status->is_esp_ota == 1) && (reals.online_Iot_num <= 1))
            {
                ESP_LOGI(TAG, "Xmodem ota end, is_esp_ota");
                if (iot_ota_end() != ESP_OK) {
                    resp = XMODEM_CAN;
                    break;
                }
                if(reals.online_ACHUB_num)
                {
                    Inv[(DEV_MAIN_NODE_MAX)].mod_reg00700_OTA.ota_group[0].pct=100;
                    g_self_data.mod_reg00700_OTA.ota_group[0].pct =100;
                    gXmodem_Status.firmware_resend_step =2;
                    reals.iot_ota_flag=1;
                    reals.iot_ota_end_count=10;
                }else{
                    status->system_restart = 1;	// 升级iot自身完成,重启系统
                }
                ESP_LOGI(TAG,"status->system_restart:%d",status->system_restart);
            } else {
                uint32_t flash_addr = 0;
                if (status->is_esp_ota != 1)
                {
                    flash_addr = IMAGE_FLASH_AREA_ADDRESS;
                }
                if (xmodem_ota_crc_verify(flash_addr, ota_data_len) != ESP_OK) {
                        resp = XMODEM_CAN; // crc32错误
                        //status->errcode = OTA_ERR_CRC32;
                        break;
                    }

				if (status->file_type == IOT_LCD)
				{
					status->exit = OTA_OBJECT_UART_OTA;	// 升级文件命令完成后开始启动lcd升级
				}
                else if (status->file_type == IOT)  //智能插座也是IOT类型
                {
                    ESP_LOGI(TAG, "Xmodem ota end, file_type IoT, online iot num:%d ,targetid:0x%x", reals.online_Iot_num,status->target_id);
                    if (OTA_CH_SELF_TO_WIFI_MESH == status->used_chl
                       || OTA_CH_WIFI_MESH_TO_SELF == status->used_chl)
                    {
                        //IOT通过mesh升级S1
                        status->exit = OTA_OBJECT_WIFI_MESH_OTA;
                        ESP_LOGI(TAG, "Xmodem ota end, start wifi-mesh-ota");
                    }
                    //存在并机IoT情况下才使用Can并机升级
                    else //if (reals.online_Iot_num > 1)
                    {
                        //can并机升级从机IoT
                        status->exit = OTA_OBJECT_CAN_OTA;
                        gXmodem_Status.firmware_resend_step =1;
                        ESP_LOGI(TAG, "Xmodem ota end, start IOT can-ota");
                    }
                }
				else
				{
					status->exit = OTA_OBJECT_CAN_OTA;	// 升级文件命令完成后开始启动can-ota升级
					gXmodem_Status.firmware_resend_step =1;
                    ESP_LOGI(TAG, "Xmodem ota end, start can-ota file_type:%d",status->file_type);
				}

            }

            break;
    }

#ifdef FUNC_ONECLICK_UPGRADE_EN
    if (!esp_ota_is_doing()) {
        if (resp == XMODEM_ACK) {
            Can_OneClickOta_Keep();
        } else {
            Can_OneClickOta_End();
        }
    }
#endif

    if (resp == XMODEM_CAN) {
        // if (pfile)
        // {
        //     fclose(pfile);
        //     pfile = NULL;
        // }
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

