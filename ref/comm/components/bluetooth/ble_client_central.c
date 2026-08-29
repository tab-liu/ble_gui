/*
  ******************************************************************************
  * @file      app_ble.c
  * @version   1.0
  * @author    lixingyu
  * @date      2024/7/1
  * @brief     蓝牙任务及接口函数
  * @par       History
  * <table>
  * <tr><th>Date       <th>Version <th>Author     <th>Description
  * <tr><td>2024/7/1   <td>1.0     <td>lixingyu   <td>Create the initial version
  * </table>
  * @copyright Copyright (C) PowerOak Co.,LTD. All rights reserved.
  ******************************************************************************
  */

#include <ctype.h>

#include "parameter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "utils.h"


/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_crypt.h"
#include "ble_dev.h"
#include "bt_adv.h"


#include "comm_define.h"
#include "modbus_define.h"
#include "modbus_protocol.h"
#include "modbus_data.h"
#include "modbus_ble_client.h"

#include "ble_client_central.h"
#include "utils.h"


#define TAG "[BLE_CLIENT_CENTRAL]"


#define BLE_CLIENT_RX_QUEUE_SZ     5              // 接收队列大小


#define  BLE_CENTER_MTU_LEN 247//tbd windy
#define BLE_CLIENT_MAX_MTU                                 512

#define  BLE_SERVER_ISSI_ONLINE_LEVEL ((int8_t)(-70))//


/////////////
/*
 * SPDX-FileCopyrightText: 2017-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#define BLE_FACTORY_BLUETT_ENCRYPT 	  	 "BLBLUETTF"//加密
#define BLE_FACTORY_BLUETT_NO_ENCRYPT 	 "BLBLUETTI"//非加密
#define BLE_FACTORY_BLUETT_NO_ENCRYPT_S1 "bluetti-plugs"//"bluetti-p"////非加密,S1



//协议类型：1-加密；2-非加密,0-无效
enum {
    BLE_ENCRYPT_TYPE_INVALID = 0,
    BLE_ENCRYPT_TYPE_YES = 1,
    BLE_ENCRYPT_TYPE_NO = 2,
};


#ifdef BLE_CLIENT_ENABLE

static QueueHandle_t ble_client_rx_queue =NULL;          // ble client蓝牙数据接收队列
static QueueHandle_t ble_c2s_send_queue = NULL;  // 发送转发队列


USE_EXT_RAM_BSS BLE_SERVER ble_server_get_buf;//获取的 ble server临时缓存
USE_EXT_RAM_BSS Ble_Server_node_sum_STRUCT Ble_Server_node_sum;

// BLE配对超时定时器句柄
static TimerHandle_t ble_pairing_timer = NULL;

static SemaphoreHandle_t ble_node_mutex = NULL;   // BLE节点数组互斥锁

USE_EXT_RAM_BSS BLE_CLIENT_DATA_STRUCT ble_client_connect;

USE_EXT_RAM_BSS ble_c2s_msg_bck_t gC2sMsgBck;


// BLE设备配对控制全局变量
USE_EXT_RAM_BSS static ble_pairing_control_t pairing_control = {
	.pairing_step = BLE_PAIRING_STEP_INIT,
	.timeout = 0,
	.current_device_index = 0,
	.device_try_count = 0,
	.local_pairing_set_step = 0,
	.scan_started = 0,
	.connect_started = 0
};


static int ble_htp_cent_gap_event(struct ble_gap_event *event, void *arg);

/*
cllient主动断开 和server连接
*/
int ble_gap_terminate_top(uint16_t conn_handle, uint8_t hci_reason)
{
	Ble_Server_node_sum.conn_handle =0xFF;
	Ble_Server_node_sum.val_handle_FF01 =0;
	Ble_Server_node_sum.val_handle_FF02_C2S_Wr =0;
	Ble_Server_node_sum.val_handle_FF03 =0;
	ble_client_connect.ble_connect_step =0;

	return ble_gap_terminate( conn_handle, hci_reason);
}



//void ble_store_config_init(void);
/**
 * Application callback.  Called when the attempt to subscribe to notifications
 * for the HTP intermediate temperature characteristic has completed.

 成功连接并配置CCCD
 */
static int ble_htp_cent_on_subscribe_CCCD(uint16_t conn_handle,
                               const struct ble_gatt_error *error,
                               struct ble_gatt_attr *attr,
                               void *arg)
{
	ESP_LOGW(TAG, "Subscribe to server CCCD char completed; status=%d "
				"conn_handle=%d attr_handle=%d\n", error->status, conn_handle, attr->handle);

	if(BLE_ENCRYPT_TYPE_YES == Ble_Server_node_sum.node[Ble_Server_node_sum.node_index].encrypt)
	{
		//			ESP_LOGI(TAG, "windy debug	ble_client_Rx_data_prase CC ");
		ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_ENCRYPT_1;
	}
	else if(BLE_ENCRYPT_TYPE_NO == Ble_Server_node_sum.node[Ble_Server_node_sum.node_index].encrypt){
		ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_NORMAL;
	}

	return 0;
}

/**
 * Application callback.  Called when the attempt to subscribe to notifications
 * for the HTP temperature measurement characteristic has completed.
 */
static int ble_htp_cent_on_subscribe(uint16_t conn_handle)
{
//    ESP_LOGW(TAG, "Subscribe to ble_htp_cent_on_subscribe char completed; status=%d "
//                "conn_handle=%d attr_handle=%d\n",
//                error->status, conn_handle, attr->handle);

    /* Subscribe to notifications for the intermediate temperature characteristic.
     * A central enables notifications by writing two bytes (1, 0) to the
     * characteristic's client-characteristic-configuration-descriptor (CCCD).
     */
    const struct peer_dsc *dsc;
    uint8_t value[2];
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    dsc = peer_dsc_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLE_SVC_UUID16),
                             BLE_UUID16_DECLARE(BLE_SVC_CHR1_UUID16_S_TO_C),
                             BLE_UUID16_DECLARE(BLE_SVC_HTP_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        ESP_LOGE(TAG, "Error: Peer lacks a CCCD characteristic\n ");
        goto err;
    }
//写入server CCCD配置，必须
    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(conn_handle, dsc->dsc.handle,
                              &value, sizeof value, ble_htp_cent_on_subscribe_CCCD, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error: Failed to ble_gattc_write_flat; "
                    "rc=%d\n", rc);
        goto err;
    }

    return 0;
	
err:
    /* Terminate the connection. */
    return ble_gap_terminate_top(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/*
get server uuid handle
*/
  static void ble_center_get_uuid_handle(const struct peer *peer)
 {
	 const struct peer_chr *chr;
	 int rc;
	  /* Read the Temparature Type characteristic. */
	  chr = peer_chr_find_uuid(peer,
							   BLE_UUID16_DECLARE(BLE_SVC_UUID16),
							   BLE_UUID16_DECLARE(BLE_SVC_CHR1_UUID16_S_TO_C));
	  if (chr == NULL) 
	  {
		  ESP_LOGE(TAG, "Error: Peer doesn't support BLE_SVC_CHR1_UUID16_S_TO_C characteristic\n");
		  goto err;
	  }
	  else
	  {
		  ESP_LOGD(TAG, "in BLE_SVC_CHR1_UUID16_S_TO_C\n");
		  Ble_Server_node_sum.val_handle_FF01 = chr->chr.val_handle;
	  }
  ///////////
	  chr = peer_chr_find_uuid(peer,
							   BLE_UUID16_DECLARE(BLE_SVC_UUID16),
							   BLE_UUID16_DECLARE(BLE_SVC_CHR2_UUID16_C_TO_S));
	  if (chr == NULL) 
	  {
		  ESP_LOGE(TAG, "Error: Peer doesn't support BLE_SVC_CHR2_UUID16_C_TO_S characteristic\n");
		  goto err;
	  }
	  else
	  {
		  ESP_LOGD(TAG, "in BLE_SVC_CHR2_UUID16_C_TO_S\n");
		  Ble_Server_node_sum.val_handle_FF02_C2S_Wr = chr->chr.val_handle;
	  }
 ///////
	  chr = peer_chr_find_uuid(peer,
							   BLE_UUID16_DECLARE(BLE_SVC_UUID16),
							   BLE_UUID16_DECLARE(BLE_SVC_CHR3_UUID16_S_TO_C));
	  if (chr == NULL) //FF03 -非必须
	  {
		  ESP_LOGE(TAG, "Error: Peer doesn't support BLE_SVC_CHR3_UUID16_S_TO_C characteristic\n");
//		  goto err;
	  }
	  else
	  {
		  ESP_LOGD(TAG, "in BLE_SVC_CHR3_UUID16_S_TO_C\n");
		  Ble_Server_node_sum.val_handle_FF03 = chr->chr.val_handle;
	  }

	  return;
  err:
	  /* Terminate the connection. */
	  ble_gap_terminate_top(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs three GATT operations against the specified peer:
 * 1. Reads the HTP temparature type characteristic.
 * 2. After read is completed, writes the HTP temperature measurement interval characteristic.
 * 3. After write is completed, subscribes to notifications for the HTP intermediate temperature
 *    and temperature measurement characteristic.
 *
 * If the peer does not support a required service, characteristic, or
 * descriptor, then the peer lied when it claimed support for the health
 * thermometer service!  When this happens, or if a GATT procedure fails,
 * this function immediately terminates the connection.
 */
static void ble_htp_cent_read_write_subscribe(const struct peer *peer)
{
	ble_htp_cent_on_subscribe(peer->conn_handle);
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
ble_htp_cent_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        ESP_LOGE(TAG, "Error: Service discovery failed; status=%d "
                    "conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate_top(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    ESP_LOGW(TAG, "Service discovery complete; status=%d "
                "conn_handle=%d\n", status, peer->conn_handle);

    /* Now perform three GATT procedures against the peer: read,
     * write, and subscribe to notifications for the HTP service.
     */
	ble_center_get_uuid_handle(peer);
	
//	ble_center_read_begin();
	ble_htp_cent_read_write_subscribe(peer);
}


/**
 * Initiates the GAP general discovery procedure.
 */
 void ble_htp_cent_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;

	Ble_Server_node_sum.mtu_value =BLE_CENTER_MTU_LEN;
	Ble_Server_node_sum.conn_handle =0xFF;
	Ble_Server_node_sum.event_type =0xFF;

    int rc;
//    ESP_LOGW(TAG, "run in :ble_htp_cent_scan status \n" );

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }
	else
	{
		Ble_Server_node_sum.own_addr_type =own_addr_type;
//		ESP_LOGW(TAG, "Ble_Server_node_sum.own_addr_type =%d \n",Ble_Server_node_sum.own_addr_type );
	}

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;//1


    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 100;//50 1600=((_ms) * 8 / 5)
    disc_params.window = 40;//800 320;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;//0;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 0;//1

	
	rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
					  ble_htp_cent_gap_event, NULL);

//    rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params,//BLE_OWN_ADDR_RANDOM BLE_OWN_ADDR_PUBLIC
//                      ble_htp_cent_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
}



///////////////


/////////////////

 
 
 /*
 填充原则：优先覆盖现有已存在位置（基于MAC地址匹配）；
 否则，填充到最前的第一个空（离线）位置
 
 BLE广播识别：单次广播包含完整信息
 - MAC地址、设备名称、RSSI、加密类型等都在一次广播中获取
 - 只有在数据有效时才进行填充，避免覆盖已有有效数据
 - 简化了函数参数，不再需要broadcast_step区分步骤
 */
 
 /**
  * @brief 检查MAC地址是否为空（全零）
  * @param addr MAC地址
  * @return true表示为空，false表示非空
  */
 static bool is_mac_addr_empty(const ble_addr_t *addr)
 {
	 static const uint8_t zero_addr[6] = {0};
	 return memcmp(addr->val, zero_addr, 6) == 0;
 }
 
 /**
  * @brief 验证设备名称是否有效
  */
 static bool is_valid_device_name(const char *name)
 {
	 if (!name || name[0] == '\0') return false;
	 
	 for (int i = 0; name[i] && i < BLE_BROADCAST_SN_LEN - 1; i++) {
		 if (!isprint((unsigned char)name[i])) return false;
	 }
	 return true;
 }
 
 /**
  * @brief 安全更新设备名称
  */
 static void update_device_name(BLE_SERVER *target, const char *new_name)
 {
	 if (!is_valid_device_name(new_name)) return;
	 
	 strncpy(target->ASCII_SN, new_name, BLE_BROADCAST_SN_LEN - 1);
	 target->ASCII_SN[BLE_BROADCAST_SN_LEN - 1] = '\0';
 }
 
 /**
  * @brief 填充BLE节点数据
  */
 static void fill_ble_node_data(BLE_SERVER *target, const BLE_SERVER *input, bool is_new_node)
 {
	 if (!input) return;
	 
	 // MAC地址更新
	 if (!is_mac_addr_empty(&input->peer_addr)) {
		 memcpy(&target->peer_addr, &input->peer_addr, sizeof(input->peer_addr));
	 } else if (is_new_node) {
		 ESP_LOGE(TAG, "New node with invalid MAC address");
		 return;
	 }
	 
	 // RSSI更新：新节点总是更新，现有节点只在合理范围内更新
	 bool rssi_valid = (input->rssi >= BLE_SERVER_ISSI_ONLINE_LEVEL && input->rssi < 0);
	 if (is_new_node || rssi_valid) {
		 target->rssi = input->rssi;
	 }
	 
	 // 加密类型更新：新节点或有有效加密类型时更新
	 if (is_new_node || input->encrypt != BLE_ENCRYPT_TYPE_INVALID) {
		 target->encrypt = input->encrypt;
	 }
	 
	 // 设备名称更新
	 update_device_name(target, input->ASCII_SN);
	 
	 // 重置超时计数器
	 target->timeout_cnt = 0;
 }
 
 /**
  * @brief 安全的MAC地址比较函数
  * @param addr1 地址1
  * @param addr2 地址2
  * @return true表示相同，false表示不同
  */
 static bool mac_addr_equal(const ble_addr_t *addr1, const ble_addr_t *addr2)
 {
	 return memcmp(addr1->val, addr2->val, 6) == 0;
 }
 
 /**
  * @brief 查找现有设备索引
  */
 static int8_t find_existing_device(const ble_addr_t *addr)
 {
	 for (uint8_t i = 0; i < MAX_CNT_BLE_SERVER; i++) {
		 if (mac_addr_equal(addr, &Ble_Server_node_sum.node[i].peer_addr)) {
			 return i;
		 }
	 }
	 return -1;
 }
 
 /**
  * @brief 查找空闲槽位
  */
 static int8_t find_empty_slot(void)
 {
	 for (uint8_t i = 0; i < MAX_CNT_BLE_SERVER; i++) {
		 if (is_mac_addr_empty(&Ble_Server_node_sum.node[i].peer_addr)) {
			 return i;
		 }
	 }
	 return -1;
 }
 
 /**
  * @brief 添加或更新BLE设备节点
  */
 void ble_node_add(BLE_SERVER input_node) 
 {
	bool mutex_taken = false;
	int8_t existing_index = -1;
	int8_t empty_index = -1;
	
	// 检查互斥锁初始化状态
	if (ble_node_mutex == NULL) {
		ESP_LOGE(TAG, "BLE node mutex not initialized in node add");
		goto exit;
	}
	
	// 获取互斥锁，超时时间100ms
	if (xSemaphoreTake(ble_node_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
		ESP_LOGW(TAG, "Failed to take BLE node mutex for node add");
		goto exit;
	}
	mutex_taken = true;
	
	// 查找现有设备
	existing_index = find_existing_device(&input_node.peer_addr);
	
	if (existing_index >= 0) {
		// 更新现有设备
		fill_ble_node_data(&Ble_Server_node_sum.node[existing_index], &input_node, false);
		ESP_LOGD(TAG, "Updated device[%d]: %s", existing_index, 
				input_node.ASCII_SN[0] ? input_node.ASCII_SN : "<no name>");
		goto exit;
	}
	
		// 添加新设备：若加密类型有效或设备名称有效
	if (input_node.encrypt == BLE_ENCRYPT_TYPE_INVALID && strlen(input_node.ASCII_SN) == 0) {
		ESP_LOGD(TAG, "Ignoring new device without encrypt and name");
		goto exit;
	}
	
	// 查找空闲槽位
	empty_index = find_empty_slot();
	if (empty_index < 0) {
		ESP_LOGD(TAG, "No available slot for new device");
		goto exit;
	}
	
	// 添加新设备
	fill_ble_node_data(&Ble_Server_node_sum.node[empty_index], &input_node, true);
	ESP_LOGD(TAG, "Added new device[%d]: %s", empty_index, 
			input_node.ASCII_SN[0] ? input_node.ASCII_SN : "<no name>");

exit:
	// 统一的出口：释放互斥锁
	if (mutex_taken) {
		xSemaphoreGive(ble_node_mutex);
	}
}

 /**
  * @brief 创建BLE设备节点
  */
static BLE_SERVER create_ble_node(const struct ble_gap_disc_desc *disc, 
 											const ad_data_t *ad_data, int encrypt_type)
{
	 BLE_SERVER node = {0};
	 
	 // 基础信息
	 memcpy(&node.peer_addr, &disc->addr, sizeof(ble_addr_t));
	 node.rssi = disc->rssi;
	 node.encrypt = encrypt_type;
	 
	 // 设备名称
	 if (ad_data->name_data && ad_data->name_len > 0) {
		 size_t copy_len = (ad_data->name_len < BLE_BROADCAST_SN_LEN - 1) ? 
						 ad_data->name_len : BLE_BROADCAST_SN_LEN - 1;
		 strncpy(node.ASCII_SN, (char*)ad_data->name_data, copy_len);
		 node.ASCII_SN[copy_len] = '\0';
	 }
	 
	 return node;
}

static void parse_advertisement_data(const struct ble_gap_disc_desc *disc, ad_data_t *ad_data)
{
	memset(ad_data, 0, sizeof(ad_data_t));
	 
	int pos = 0;
	while (pos < disc->length_data) 
	{
		uint8_t ad_len = disc->data[pos];
		if (ad_len == 0 || pos + ad_len >= disc->length_data) break;

		uint8_t ad_type = disc->data[pos + 1];
		const uint8_t *ad_payload = &disc->data[pos + 2];
		uint8_t payload_len = ad_len - 1;

		switch (ad_type) 
		{
			case BLE_AD_TYPE_MANUFACTURER_DATA:
				ad_data->mfg_data = ad_payload;
				ad_data->mfg_len = payload_len;
				break;
			case BLE_AD_TYPE_COMPLETE_LOCAL_NAME:
				ad_data->name_data = ad_payload;
				ad_data->name_len = payload_len;
				break;
			case BLE_AD_TYPE_SHORTENED_LOCAL_NAME:
				if (!ad_data->name_data) {
					ad_data->name_data = ad_payload;
					ad_data->name_len = payload_len;
				}
				break;
		}

		pos += ad_len + 1;
	}
}

 // 检查厂商信息是否为预设的BLUETTI厂商标识
static int check_bluetti_manufacturer_data(const uint8_t *mfg_data, uint8_t mfg_len) {
	 if (!mfg_data || mfg_len == 0) {
		 return BLE_ENCRYPT_TYPE_INVALID;
	 }
 
	 // 检查是否为BLUETTI加密设备
	 if (strncmp(BLE_FACTORY_BLUETT_ENCRYPT, (char *)mfg_data, strlen(BLE_FACTORY_BLUETT_ENCRYPT)) == 0) {
		 // ESP_LOGI(TAG, "Found BLUETTI encrypted device: %s", BLE_FACTORY_BLUETT_ENCRYPT);
		 return BLE_ENCRYPT_TYPE_YES;
	 }
	 // 检查是否为BLUETTI非加密设备
	 else if (strncmp(BLE_FACTORY_BLUETT_NO_ENCRYPT, (char *)mfg_data, strlen(BLE_FACTORY_BLUETT_NO_ENCRYPT)) == 0) {
		 // ESP_LOGI(TAG, "Found BLUETTI non-encrypted device: %s", BLE_FACTORY_BLUETT_NO_ENCRYPT);
		 return BLE_ENCRYPT_TYPE_NO;
	 }
	 // 检查是否为BLUETTI S1非加密设备
	 else if (strncmp(BLE_FACTORY_BLUETT_NO_ENCRYPT_S1, (char *)mfg_data, strlen(BLE_FACTORY_BLUETT_NO_ENCRYPT_S1)) == 0) {
		 // ESP_LOGI(TAG, "Found BLUETTI S1 non-encrypted device: %s", BLE_FACTORY_BLUETT_NO_ENCRYPT_S1);
		 return BLE_ENCRYPT_TYPE_NO;
	 }
	 else if(mfg_data[0] == 0x06 &&mfg_data[1] == 0x0F){ // 0x0F06 poweroak厂商ID
		 ble_mfg_header_t *header = (ble_mfg_header_t *)&mfg_data[2];
		 if(header->prefix.encrypt == 0x02){
			 return BLE_ENCRYPT_TYPE_YES;
		 } else if(header->prefix.encrypt == 0x01){
			 return BLE_ENCRYPT_TYPE_NO;
		 } else {
			 // 忽略
		 }
	 }
	 
	 // 不是预设的三种厂商信息
	 // ESP_LOGW(TAG, "Unknown manufacturer data, length: %d", mfg_len);
	 // ESP_LOG_BUFFER_HEX_LEVEL(TAG, mfg_data, mfg_len, ESP_LOG_WARN);
	 return BLE_ENCRYPT_TYPE_INVALID;
}



 /**
  * @brief 处理BLE广播数据
  * 根据宏定义BLE_MANUFACTURER_FILTER_ENABLE控制是否过滤厂商ID
  * - 启用过滤：仅添加BLUETTI厂商设备
  * - 禁用过滤：添加所有扫描到的BLE设备，按MAC地址区分
  */
static int ble_center_find_advertise_node (const struct ble_gap_disc_desc *disc)//ble_htp_cent_should_connect
{
 
	 // ESP_LOGI(TAG, "BLE Advertisement Debug:");
	 // ESP_LOGI(TAG, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X, RSSI: %d, Event: %d, Length: %d",
	 // 	 disc->addr.val[0], disc->addr.val[1], disc->addr.val[2], 
	 // 	 disc->addr.val[3], disc->addr.val[4], disc->addr.val[5],
	 // 	 disc->rssi, disc->event_type, disc->length_data);
	 
	 // if (disc->length_data > 0 && disc->data) {
	 //  ESP_LOG_BUFFER_HEX_LEVEL(TAG, disc->data, disc->length_data, ESP_LOG_INFO);
	 // }
 
	 // 基本过滤：排除不可连接的设备和信号过弱的设备
	 if (disc->rssi < BLE_SERVER_ISSI_ONLINE_LEVEL) { // disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND || 
		 return -1;
	 }
 
	 
	 // 解析广播数据
	 ad_data_t ad_data = {0};
	 parse_advertisement_data(disc, &ad_data);
 
	 int encrypt_type = BLE_ENCRYPT_TYPE_INVALID;
	 
#if BLE_MANUFACTURER_FILTER_ENABLE
	 // 启用厂商ID过滤：仅处理BLUETTI设备
	 if (ad_data.mfg_data && ad_data.mfg_len > 0) {
		 encrypt_type = check_bluetti_manufacturer_data(ad_data.mfg_data, ad_data.mfg_len);
		 if (encrypt_type == BLE_ENCRYPT_TYPE_INVALID) {
			 ESP_LOGD(TAG, "Filtered out non-BLUETTI device (MAC: %02X:%02X:%02X:%02X:%02X:%02X)", 
					 disc->addr.val[0], disc->addr.val[1], disc->addr.val[2], 
					 disc->addr.val[3], disc->addr.val[4], disc->addr.val[5]);
			 return -1; // 非BLUETTI设备，过滤掉
		 }
	 } else {
		 // 没有厂商数据的设备也过滤掉
		 ESP_LOGD(TAG, "Filtered out device without manufacturer data (MAC: %02X:%02X:%02X:%02X:%02X:%02X)", 
				 disc->addr.val[0], disc->addr.val[1], disc->addr.val[2], 
				 disc->addr.val[3], disc->addr.val[4], disc->addr.val[5]);
		 return -1;
	 }
#else
	 // 禁用厂商ID过滤：处理所有设备
	 if (ad_data.mfg_data && ad_data.mfg_len > 0) {
		 // 尝试识别BLUETTI设备的加密类型
		 encrypt_type = check_bluetti_manufacturer_data(ad_data.mfg_data, ad_data.mfg_len);
		 if (encrypt_type != BLE_ENCRYPT_TYPE_INVALID) {
			 ESP_LOGD(TAG, "Found BLUETTI device: encrypt=%d", encrypt_type);
		 } else {
			 ESP_LOGD(TAG, "Found non-BLUETTI device with manufacturer data");
		 }
	 } else {
		 ESP_LOGD(TAG, "Found device without manufacturer data");
	 }
#endif
	 
	 // 创建设备节点并添加到列表
	 BLE_SERVER node = create_ble_node(disc, &ad_data, encrypt_type);
	 ble_node_add(node);
	 
	 // 输出调试信息
	 char device_name[BLE_BROADCAST_SN_LEN + 1] = {0};
	 if (ad_data.name_data && ad_data.name_len > 0) {
		 size_t copy_len = (ad_data.name_len < BLE_BROADCAST_SN_LEN) ? ad_data.name_len : BLE_BROADCAST_SN_LEN - 1;
		 memcpy(device_name, ad_data.name_data, copy_len);
		 device_name[copy_len] = '\0';
	 } else {
		 strcpy(device_name, "<No Name>");
	 }
	 
	 ESP_LOGD(TAG, "Added device: %s (MAC: %02X:%02X:%02X:%02X:%02X:%02X, RSSI: %d, Encrypt: %d)", 
			 device_name,
			 disc->addr.val[0], disc->addr.val[1], disc->addr.val[2], 
			 disc->addr.val[3], disc->addr.val[4], disc->addr.val[5],
			 disc->rssi, encrypt_type);
	 
	return 0;
}



/**
 * @brief 蓝牙数据备份到接收队列
 将多段RX报文拼接后，才一起存入队列
 * 
 * @param data 数据
 * @param len 长度
 */
static void ble_client_data_Rx_to_queue(uint8_t *data, int len)
{
    /* 创建接收消息队列 */
    if (!ble_client_rx_queue) {
        ble_client_rx_queue = xQueueCreate(BLE_CLIENT_RX_QUEUE_SZ, sizeof(ble_data_t));
		
	    if (ble_client_rx_queue == NULL)
	    {
	        ESP_LOGE(TAG, "xQueueCreate ble_client_rx_queue failed ");
	    }		
        assert(ble_client_rx_queue != NULL);
    }

    if (len == 0) return;

    /* 为数据申请缓存 */
    ble_data_t rx_buff;
    rx_buff.data = (uint8_t *)iot_calloc(len);
    if (rx_buff.data == NULL)
    {
        ESP_LOGE(TAG, "memory malloc failed, size: %d", len);
        return;
    }

    /* 拷贝数据到缓存 */
    memcpy(rx_buff.data, data, len);
    rx_buff.len = len;
    
    /* 数据暂存到队列中 */
    if (xQueueSend(ble_client_rx_queue, &rx_buff, pdMS_TO_TICKS(100)) != pdPASS)
    {
        ESP_LOGE(TAG, "send data to ble_client_rx_queue timeout");
        free(rx_buff.data); // 发送失败释放申请的缓存
    }
	else
	{
		ESP_LOGW(TAG, "ble_client_data_Rx_to_queue, data_len:%d, data:",  rx_buff.len);
//		ESP_LOG_BUFFER_HEX(TAG, rx_buff.data, rx_buff.len);

	}
}


/*
RX A buf copy to RX B buf
接收完成或超时执行
direct_run:
1- 立刻执行；
0- 超时执行
*/
void ble_client_Rx_buf_copy(uint8_t direct_run)//
{
	static uint8_t scnt=0;

	if(1 == direct_run)
	{
//		dump_buf_global(" ble_client_Rx_buf_copy 111:", ble_client_connect.ble_client_Rx_bufA, ble_client_connect.ble_client_Rx_lenA);
//	ESP_LOGE(TAG,"ble client rx time t(ms)= %ld,direct_run =%d",GET_CURRENT_TIME_MS_B(),direct_run);
		
		ble_client_data_Rx_to_queue(ble_client_connect.ble_client_Rx_bufA ,ble_client_connect.ble_client_Rx_lenA);
		ble_client_connect.ble_client_Rx_lenA=0;
	}
	else
	{
		if(0 != ble_client_connect.ble_client_Rx_lenA)
		{
			if(++scnt >= 10)//10ms cycle, 10*10=100ms
			{
				scnt=0;
//				ESP_LOGE(TAG,"ble client rx time t(ms)= %ld,direct_run =%d",GET_CURRENT_TIME_MS_B(),direct_run);
//				dump_buf_global(" ble_client_Rx_buf_copy 000:", ble_client_connect.ble_client_Rx_bufA, ble_client_connect.ble_client_Rx_lenA);
				
				ble_client_data_Rx_to_queue(ble_client_connect.ble_client_Rx_bufA ,ble_client_connect.ble_client_Rx_lenA);
				ble_client_connect.ble_client_Rx_lenA=0;
			}
		}
	}
}


/**
 * @brief 蓝牙应用数据接收回调函数
 * - APP发送数据时触发该函数执行
 * - 判断蓝牙数据接收完成有三种情况：
 * - 1、蓝牙单次发送数据小于MTU值
 * - 2、蓝牙数据总接收长度大于一包指令数据的最大值
 * - 3、蓝牙接收到数据后等待下一帧数据超时
 * 
 * @param data 数据指针
 * @param len 数据长度

  等同于data_rx_callback(),FF02：ESP32 RX
 * 
 */
static void ble_client_Rx_callback(uint8_t *data, int len)
{
    memcpy(&ble_client_connect.ble_client_Rx_bufA[ble_client_connect.ble_client_Rx_lenA], data, len);//收到任何数据先拼接到缓存
    ble_client_connect.ble_client_Rx_lenA += len;

    /* 单次长度小于MTU长度认为数据接收完成，总长度大于接收最大值认为数据接收完成 （MTU<=247）*/
    if ((len < Ble_Server_node_sum.mtu_value) || (ble_client_connect.ble_client_Rx_lenA >= (BLE_CLIENT_MAX_RX_LEN)))//本次接收数据较短或超出最大长度，则立刻存入队列
    {
		ble_client_Rx_buf_copy(1);
    }
    /* 设置等待下一帧数据超时时间 */
    else//本次接收数据为单帧切割长度，则等待
    {

    }
}


/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  ble_htp_cent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  ble_htp_cent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
ble_htp_cent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;
	
	Ble_Server_node_sum.event_type =event->type;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC://client未连接 server,在循环扫描
//		ESP_LOGI(TAG," BLE_GAP_EVENT_DISC :\n");		
//        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
//                                     event->disc.length_data);
//        if (rc != 0) {
//            return 0;
//        }
//
//        /* An advertisment report was received during GAP discovery. */
//        print_adv_fields(&fields);

        /* Try to connect to the advertiser if it looks interesting. */
		ble_center_find_advertise_node((struct ble_gap_disc_desc *)&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event BLE_GAP_EVENT_CONNECT  :\n");		
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) 
		{
            /* Connection successfully established. */
            ESP_LOGW(TAG, "Connection established ");
            reals.last_ble_server_connect_time = reals.now;

            rc = ble_att_set_preferred_mtu(BLE_CLIENT_MAX_MTU);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to set preferred MTU; rc = %d", rc);
            }

            rc = ble_gattc_exchange_mtu(event->connect.conn_handle, NULL, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to negotiate MTU; rc = %d", rc);
            }

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
			 if (rc != 0)
			{
				 ESP_LOGE(TAG, "failed to ble_gap_conn_find, rc: %d ", rc);
				 Ble_Server_node_sum.conn_handle= 0xFF;
				 
				return -1;
			} 
			 else
			 {
				 Ble_Server_node_sum.conn_handle= event->connect.conn_handle;
			 }

			 ESP_LOGE(TAG, "Ble_Server_node_sum.conn_handle =%d ", Ble_Server_node_sum.conn_handle);
			 
//            print_conn_desc(&desc);
            ESP_LOGW(TAG, "\n");
			
			int8_t rssi;
			rc = ble_gap_conn_rssi(event->connect.conn_handle, &rssi);
			if (rc == 0) 
			{
				ESP_LOGD(TAG, "Current  BLE RSSI = %d", rssi);
				Ble_Server_node_sum.node[Ble_Server_node_sum.node_index].rssi = rssi;
				reals.ble_server_signal_strength = rssi;
			}
			else
			{
				 ESP_LOGE(TAG, "failed to ble_gap_conn_find, rc: %d ", rc);
				return -1;
			} 

			/* Remember peer. */
			rc = peer_add(event->connect.conn_handle);
			if (rc != 0) {
				ESP_LOGE(TAG, "Failed to add peer; rc=%d\n", rc);
				return 0;
			}

				/* Perform service discovery */
			  rc = peer_disc_all(event->connect.conn_handle,
								 ble_htp_cent_on_disc_complete, NULL);
			  if (rc != 0) {
				  ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
				  return 0;
			  }

//	  		peer_delete(event->connect.conn_handle);
				  
#if CONFIG_EXAMPLE_ENCRYPTION
            /** Initiate security - It will perform
             * Pairing (Exchange keys)
             * Bonding (Store keys)
             * Encryption (Enable encryption)
             * Will invoke event BLE_GAP_EVENT_ENC_CHANGE
             **/
            rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGW(TAG, "Security could not be initiated, rc = %d\n", rc);
                return ble_gap_terminate_top(event->connect.conn_handle,
                                         BLE_ERR_REM_USER_CONN_TERM);
            } else {
                ESP_LOGW(TAG, "Connection secured\n");
            }
#else


		
#endif
        } 
		else 
		{
            /* Connection attempt failed; resume scanning. */
            ESP_LOGE(TAG, "Error: Connection failed; status=%d\n",
                        event->connect.status);
            ble_htp_cent_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT://被外部连接设备断开
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_DISCONNECT :\n");

		Ble_Server_node_sum.conn_handle =0xFF;
		Ble_Server_node_sum.val_handle_FF01 =0;
		Ble_Server_node_sum.val_handle_FF02_C2S_Wr =0;
		Ble_Server_node_sum.val_handle_FF03 =0;
		ble_client_connect.ble_connect_step =0;
        reals.last_ble_server_disconn_time = reals.now;
        /* Connection terminated. */
        ESP_LOGW(TAG, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        ESP_LOGW(TAG, "\n");

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        ble_htp_cent_scan();
        return 0;

	case BLE_GAP_EVENT_CONN_UPDATE:
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_CONN_UPDATE :\n");
		
		/* The central has updated the connection parameters. */
//			MODLOG_DFLT(INFO, "connection updated; status=%d ",
//						event->conn_update.status);
//			rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
//			assert(rc == 0);
//			bleprph_print_conn_desc(&desc);
//			MODLOG_DFLT(INFO, "\n");
		return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_DISC_COMPLETE :\n");
        ESP_LOGW(TAG, "discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_ENC_CHANGE :\n");
        /* Encryption has been enabled or disabled for this connection. */
        ESP_LOGW(TAG, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
#if CONFIG_EXAMPLE_ENCRYPTION
        /*** Go for service discovery after encryption has been successfully enabled ***/
        rc = peer_disc_all(event->connect.conn_handle,
                           ble_htp_cent_on_disc_complete, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
            return 0;
        }
#endif
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:// as client ,rx data
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_NOTIFY_RX :\n");
        /* Peer sent us a notification or indication. */
        ESP_LOGW(TAG, "received %s; conn_handle=%d attr_handle=%d "
                    "attr_len=%d\n",
                    event->notify_rx.indication ?
                    "indication" :
                    "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));//rx 有效内容数据长度

//		dump_buf_global("rx data:", event->notify_rx.om->om_data, OS_MBUF_PKTLEN(event->notify_rx.om));
		
		ble_client_Rx_callback(event->notify_rx.om->om_data ,OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.om. Use
         * `os_mbuf_copydata` to copy the data received in notification mbuf */
        return 0;

    case BLE_GAP_EVENT_MTU:
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_MTU :\n");
        ESP_LOGW(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
		Ble_Server_node_sum.mtu_value =event->mtu.value;
	
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_REPEAT_PAIRING :\n");
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;


	case BLE_GAP_EVENT_L2CAP_UPDATE_REQ://
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_L2CAP_UPDATE_REQ :\n");

	return 0;		
	case BLE_GAP_EVENT_REATTEMPT_COUNT://BLE_GAP_EVENT_CONNECT 重连的底层自动尝试
//		ESP_LOGI(TAG,"  DEBUG ble_htp_cent_gap_event  BLE_GAP_EVENT_REATTEMPT_COUNT :\n");

	return 0;


    default:
		ESP_LOGE(TAG,"Ble_Server_node_sum.event_type xx=%d	:\n",Ble_Server_node_sum.event_type);	
		
        return 0;
    }
}

void ble_client_data_init(void) // 
{
	// for (uint8_t i = 0; i < MAX_CNT_BLE_SERVER; i++)
	// {
	// 	Ble_Server_node_sum.node[i].timeout_cnt = 0xFF;
	// 	memset(&Ble_Server_node_sum.node[i].peer_addr, 0, sizeof(Ble_Server_node_sum.node[i].peer_addr));
	// 	memset(Ble_Server_node_sum.node[i].ASCII_SN, 0, sizeof(Ble_Server_node_sum.node[i].ASCII_SN));
	// 	Ble_Server_node_sum.node[i].encrypt = BLE_ENCRYPT_TYPE_INVALID;
	// 	Ble_Server_node_sum.node[i].rssi = -120;
	// 	Ble_Server_node_sum.online_node_cnt = 0;
	// }
	// 此处不能清空，由节点超时清理
	Ble_Server_node_sum.conn_handle = 0xFF;
}

/**
 * @brief 对BLE设备节点按RSSI信号强度从大到小排序（线程安全）
 * 排序规则：
 * 1. 有效节点按RSSI从大到小排序
 * 2. 无效节点排在最后
 */
static void sort_ble_nodes_by_rssi(void) {
	if (ble_node_mutex == NULL) {
		ESP_LOGE(TAG, "BLE node mutex not initialized");
		return;
	}
	
	// 获取互斥锁，超时时间100ms
	if (xSemaphoreTake(ble_node_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
		ESP_LOGW(TAG, "Failed to take BLE node mutex for sorting");
		return;
	}
	
	int i, j;
	BLE_SERVER temp_node;   
	
	// 冒泡排序，按RSSI值从大到小排序，无效节点排在最后
	for (i = 0; i < MAX_CNT_BLE_SERVER - 1; i++) {
		for (j = 0; j < MAX_CNT_BLE_SERVER - i - 1; j++) {
			bool current_valid = !is_mac_addr_empty(&Ble_Server_node_sum.node[j].peer_addr);
			bool next_valid = !is_mac_addr_empty(&Ble_Server_node_sum.node[j + 1].peer_addr);
			
			// 如果当前节点无效，但下一个节点有效，则交换（将无效节点往后移）
			if (!current_valid && next_valid) {
				memcpy(&temp_node, &Ble_Server_node_sum.node[j], sizeof(BLE_SERVER));
				memcpy(&Ble_Server_node_sum.node[j], &Ble_Server_node_sum.node[j + 1], sizeof(BLE_SERVER));
				memcpy(&Ble_Server_node_sum.node[j + 1], &temp_node, sizeof(BLE_SERVER));
			}
			// 如果两节点都有效，则按RSSI值从大到小排序
			else if (current_valid && next_valid &&
					Ble_Server_node_sum.node[j].rssi < Ble_Server_node_sum.node[j + 1].rssi) {
				memcpy(&temp_node, &Ble_Server_node_sum.node[j], sizeof(BLE_SERVER));
				memcpy(&Ble_Server_node_sum.node[j], &Ble_Server_node_sum.node[j + 1], sizeof(BLE_SERVER));
				memcpy(&Ble_Server_node_sum.node[j + 1], &temp_node, sizeof(BLE_SERVER));
			}
		}
	}
	
	// 释放互斥锁
	xSemaphoreGive(ble_node_mutex);
}

// 1s周期
void ble_client_node_online_check(void) 
{
	if (ble_node_mutex == NULL) {
		ESP_LOGE(TAG, "BLE node mutex not initialized in online check");
		return;
	}
	
	// 获取互斥锁，超时时间200ms（比排序函数长一些，避免死锁）
	if (xSemaphoreTake(ble_node_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
		ESP_LOGW(TAG, "Failed to take BLE node mutex for online check");
		return;
	}
	
	uint8_t tempcnt = 0;

	for (uint8_t i = 0; i < MAX_CNT_BLE_SERVER; i++)
	{
		// 只对有效节点进行超时计数（基于MAC地址判断是否为有效节点）
		if ((*(uint64_t *)(Ble_Server_node_sum.node[i].peer_addr.val) & 0xFFFFFFFFFFFF) != 0) {
			Ble_Server_node_sum.node[i].timeout_cnt++;
			if(Ble_Server_node_sum.node[i].timeout_cnt >= 0xFF)
			{
				// 超时的节点清空为无效状态
				memset(&Ble_Server_node_sum.node[i], 0, sizeof(BLE_SERVER));
				Ble_Server_node_sum.node[i].timeout_cnt = 0xFF;
				Ble_Server_node_sum.node[i].encrypt = BLE_ENCRYPT_TYPE_INVALID;
				Ble_Server_node_sum.node[i].rssi = -120;
			}
		}

		// 统计在线节点数量（只对有效节点进行统计）
		if ((*(uint64_t *)(Ble_Server_node_sum.node[i].peer_addr.val) & 0xFFFFFFFFFFFF) != 0 &&
			Ble_Server_node_sum.node[i].rssi >= BLE_SERVER_ISSI_ONLINE_LEVEL)
		{
			tempcnt++;	
		}
	}
	Ble_Server_node_sum.online_node_cnt = tempcnt;
	
	// 释放互斥锁
	xSemaphoreGive(ble_node_mutex);
}

/**
 * @brief 检查是否存在活跃的BLE连接
 * @return true - 存在连接, false - 无连接
 */
static uint8_t is_ble_connecting(void)
{
	return ble_gap_conn_active();
}


/**
 * @brief 检查是否存在活跃的BLE连接
 * @return true - 存在连接, false - 无连接
 */
static bool is_ble_connected(void)
{
	// return ((BLE_GAP_EVENT_CONNECT == Ble_Server_node_sum.event_type) ||
	//         (BLE_GAP_EVENT_NOTIFY_RX == Ble_Server_node_sum.event_type) ||
	//         (BLE_GAP_EVENT_CONN_UPDATE == Ble_Server_node_sum.event_type) ||
	//         (0xFF != Ble_Server_node_sum.conn_handle));

	if (Ble_Server_node_sum.conn_handle == 0xFF) {
	    return false;
	}
	
	// 使用NimBLE API验证
	struct ble_gap_conn_desc desc;
	return (ble_gap_conn_find(Ble_Server_node_sum.conn_handle, &desc) == 0);
}


/**
 * @brief 强制断开当前BLE连接
 * @return 0 - 成功, 其他 - 失败
 */
static int force_disconnect_ble(void)
{
	int rc = 0;
	uint16_t saved_conn_handle = Ble_Server_node_sum.conn_handle;
	
	// 停止可能正在进行的扫描
	if (BLE_GAP_EVENT_DISC == Ble_Server_node_sum.event_type) {
		ble_gap_disc_cancel();
		ESP_LOGI(TAG, "Cancelled ongoing scan");
	}
	
	// 断开连接
	if (Ble_Server_node_sum.conn_handle != 0xFF) {
		rc = ble_gap_terminate_top(Ble_Server_node_sum.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
		if (rc != 0) {
			ESP_LOGE(TAG, "Error: Failed to ble_gap_terminate; rc=%d", rc);
			// 即使断开失败，也要尝试清理peer资源
		} else {
			ESP_LOGI(TAG, "ble_gap_terminate ok");
		}
		
		// 等待断开完成
		vTaskDelay(pdMS_TO_TICKS(200));

		// 在断开后删除peer资源，确保资源正确释放
		peer_delete(saved_conn_handle);
		ESP_LOGI(TAG, "Peer resources deleted for conn_handle=%d", saved_conn_handle);
	}
	
	// 清理所有相关状态
	Ble_Server_node_sum.conn_handle = 0xFF;
	Ble_Server_node_sum.val_handle_FF01 = 0;
	Ble_Server_node_sum.val_handle_FF02_C2S_Wr = 0;
	Ble_Server_node_sum.val_handle_FF03 = 0;
	Ble_Server_node_sum.event_type = 0xFF;
	ble_client_connect.ble_connect_step = 0;
	
	ESP_LOGI(TAG, "BLE connection fully cleaned up");
	return rc;
}

/**
 * @brief 开始BLE扫描
 * @return 0 - 成功, 其他 - 失败
 */
static int start_ble_scan(void)
{
	// 检查是否已经在扫描中
	if (ble_gap_disc_active()) {
		ESP_LOGW(TAG, "BLE scan already active, skipping start request");
		return 0;
	}
	
	if (BLE_GAP_EVENT_DISC != Ble_Server_node_sum.event_type) {
		ble_htp_cent_scan();
		ESP_LOGI(TAG, "Started BLE scanning");
	}
	return 0;
}

/**
 * @brief 停止BLE扫描
 * @return 0 - 成功, 其他 - 失败
 */
static int stop_ble_scan(void)
{
	if (BLE_GAP_EVENT_DISC == Ble_Server_node_sum.event_type && 
		Ble_Server_node_sum.online_node_cnt >= 1) {
		int rc = ble_gap_disc_cancel();
			if (rc != 0) {
			ESP_LOGW(TAG, "Failed to cancel scan; rc=%d", rc);
		} else {
			ESP_LOGW(TAG, "Successfully cancelled scan; rc=%d", rc);
			Ble_Server_node_sum.event_type = 0xFF;
		}
		return rc;
	}
	return 0;
}


/*
data: ble tx 底层发送函数


*/
esp_err_t app_ble_send_data_to_remote( const uint8_t *data, int data_len)
{
    uint8_t i = 0;

    int index = 0;
//	const struct peer_chr *chr;
	int rc;	
    while (data_len)
    {
    
	ESP_LOGE(TAG, "in app_ble_send_data_to_remote ");
        int send_count = (data_len > Ble_Server_node_sum.mtu_value) ? Ble_Server_node_sum.mtu_value : data_len;
		
//		chr = peer_chr_find_uuid(peer,
//								 BLE_UUID16_DECLARE(BLE_SVC_UUID16),
//								 BLE_UUID16_DECLARE(BLE_SVC_CHR2_UUID16_C_TO_S));
//		if (chr == NULL) {
//			ESP_LOGE(TAG, "Error: Peer doesn't support BLE_SVC_CHR2_UUID16_C_TO_S characteristic\n");
//			goto err;
//		}
//		else
//		{
//			ESP_LOGD(TAG, "in BLE_SVC_CHR2_UUID16_C_TO_S\n");
//		
//		}
	
		rc = ble_gattc_write_flat(Ble_Server_node_sum.conn_handle, Ble_Server_node_sum.val_handle_FF02_C2S_Wr,//peer->conn_handle
								  (uint8_t *)&data[index], send_count, NULL, NULL);
		if (rc != 0) 
		{
			ESP_LOGE(TAG, "Error: Failed to ble_gattc_write_flat; "
						"rc=%d\n", rc);
            return ESP_FAIL;
		}
		else
		{
			ESP_LOGE(TAG, "ble_gattc_write_flat, send_count=%d, data_len=%d ",send_count,data_len);
		}

										
        data_len -= send_count;
        index += send_count;
    }
    return ESP_OK;
}

 
 
 
 /*------------------------------------------------------------------------
 蓝牙发送报文
 
 *@brief  
 *@param[in]	 *rsp_data：BLE tx buf
 *@param[out]	 rsp_len:BLE tx len
 
 *@param[in] :TxChannel :BLE_FF01_CHAR_VAL/BLE_FF03_CHAR_VAL
 
 *@return		  


 0- ok
 not 0-fail
 */
int Ble_Client_Tx_Data(const uint8_t *rsp_data, uint16_t rsp_len ) 
 {
	 uint8_t *encrypt_data = NULL;
	 uint8_t *rsp_pdata = NULL;
	 uint16_t rsp_plen = 0;
	 uint16_t encrypt_len = 0;
	 int ret = -1;

	 if(0 == rsp_len){
		return -1;
	 }
	 
	ESP_LOGW(TAG, "Ble_Client_Tx_Data[%d]:", rsp_len);
	ESP_LOG_BUFFER_HEX_LEVEL(TAG, rsp_data, rsp_len, ESP_LOG_WARN);
	 if(1 == Ble_Server_node_sum.node[Ble_Server_node_sum.node_index].encrypt) //判断是否启用加密
//	 if(0)//debug only
	 {
		 encrypt_len = get_encrypt_sending_pack_len(rsp_len); //获取长度
//		 if(ble_encrypt_info.authenticate_state == BLE_ENCRYPT_COMPLATE)
		 {
			 encrypt_len+=4; //加上随机数长度
		 }
		 printf( "Ble_Client_Tx_Data encrypt_len =%d",encrypt_len);
		 
		 encrypt_data = iot_calloc(encrypt_len * sizeof(char)); //申请内存
		 if(encrypt_data == NULL)
		 {
			 ESP_LOGI(TAG, "malloc fail");
			 return -1;
		 }
		 
		  ESP_LOGI (TAG, "Ble_Client_Tx_Data : AES-CBC");
		  
		  aes_cbc_encrypt_sending_pack_run_in_client(rsp_data, rsp_len, encrypt_data, &encrypt_len); //生成AES-CBC加密包
//		  ESP_LOG_BUFFER_HEX_LEVEL(TAG, encrypt_data, encrypt_len, ESP_LOG_WARN);			 
		 rsp_pdata = encrypt_data;
		 rsp_plen = encrypt_len;
	 }
	 else  //未加密
	 {
		 ESP_LOGE (TAG, "Ble_Client_Tx_Data: unencrypted");
		 rsp_pdata = (uint8_t*)rsp_data;
		 rsp_plen = rsp_len;
	 }
 
	 ret =app_ble_send_data_to_remote(rsp_pdata, rsp_plen);
		 
	 if(encrypt_data != NULL)
	 {
		 free(encrypt_data);
		 encrypt_data = NULL;
	 }

	 return ret;
 }

uint8_t *publicKeyA1 =NULL;//[64];

/*
Rx data握手解密过程

uint8_t *data;	// 数据指针，原始 rx data
int len;		// 数据长度

*/
void ble_client_Encrypt_step(uint8_t *data, int len)
{
	/// @brief 交换拿到的公钥
	static  uint8_t  SendPublicKey_calc_flag =0;

	//认证鉴权信息
	if ((data[0] == '*') && (data[1] == '*')) //鉴权数据
	{
		if((10 == len) && (BLE_CLIENT_STEP_ENCRYPT_1 == ble_client_connect.ble_connect_step))
		{
			ESP_LOGI(TAG, "01 Pack Recive............");
			SendPublicKey_calc_flag =1;

			if (ble_center_encrypt_process_data(data, len, ble_client_connect.ble_client_Tx_buf) == ESP_OK)
			{
				ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_ENCRYPT_GET_1;
				if (app_ble_send_data_to_remote( (const uint8_t *)ble_client_connect.ble_client_Tx_buf, 10) != ESP_OK){
					ESP_LOGE(TAG, "Failed to re Ble data (len: %d) ", len);
				}else{
					ESP_LOGI(TAG, "02 Pack ReSend............");
					ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_ENCRYPT_SEND_2;
				}
			}
		}
		if((7 == len) && (BLE_CLIENT_STEP_ENCRYPT_SEND_2 == ble_client_connect.ble_connect_step))
		{
			ESP_LOGI(TAG, "03 Pack Recive............");     
			if(data[0]!='*' || data[1]!='*' || data[4]!=0)
			{
				ESP_LOGI(TAG, "03 Pack Fail.....");
				//断开连接
				/* Terminate the connection. */
				ble_gap_terminate_top(Ble_Server_node_sum.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
			}  
			else{
				ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_ENCRYPT_GET_3;
			}
		}
	}

	//认证鉴权信息2
	if (((data[0] == 0) && (data[1] == 0x86)) //鉴权数据
		&&(BLE_CLIENT_STEP_ENCRYPT_GET_3 == ble_client_connect.ble_connect_step))
	{
		ESP_LOGI(TAG, "04 Pack Recive............");
		if(NULL == publicKeyA1)
		{
			publicKeyA1 = (uint8_t *)iot_calloc(64);
			if (NULL == publicKeyA1)
			{
				ESP_LOGE(TAG, "memory malloc failed");
				return;
			}
		}		

		if(1 == SendPublicKey_calc_flag)
		{
			SendPublicKey_calc_flag =2;
			RandomSendPKey();//ble client need
		}

		if (ble_center_encrypt_process_data4(data, len, ble_client_connect.ble_client_Tx_buf,publicKeyA1) == ESP_OK)
		{
			ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_ENCRYPT_GET_4;

			ESP_LOGI(TAG, "05 Pack ReSending............");
			if (app_ble_send_data_to_remote( (const uint8_t *)ble_client_connect.ble_client_Tx_buf, 146) != ESP_OK){
				ESP_LOGE(TAG, "Failed to re Ble data (len: %d) ", len);
			}
			else{
				ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_ENCRYPT_SEND_5;
			}				
		}
	}


	if( ((data[0] == 0) && (18 == len)) //鉴权最终结果
		&&(BLE_CLIENT_STEP_ENCRYPT_SEND_5 == ble_client_connect.ble_connect_step))
	{
		ESP_LOGI(TAG, "06 Pack Recive............");
		if (ble_center_encrypt_process_data6(data, len, ble_client_connect.ble_client_Tx_buf,publicKeyA1) == ESP_OK)
		{
			//				ESP_LOGI(TAG, "windy debug	ble_client_Rx_data_prase  AA ");
			if (app_ble_send_data_to_remote( (const uint8_t *)ble_client_connect.ble_client_Tx_buf, 2) != ESP_OK){
				ESP_LOGE(TAG, "Failed to re Ble data (len: %d) ", len);
			}
			else{
				ble_client_connect.ble_connect_step =BLE_CLIENT_STEP_ENCRYPT_GET_6_OK;
			}				
		}     

		if(NULL != publicKeyA1)
		{
			free(publicKeyA1);
			publicKeyA1 =NULL;
		}
		//			ESP_LOGI(TAG, "windy debug	ble_client_Rx_data_prase BB ");
	}
}


/**
 * @brief 打印扫描完成后的BLE设备节点信息
 * @param scan_completed 是否为扫描完成后调用（true时显示更详细信息）
 */
void debug_print_ble_scan_result(bool scan_completed)
{
	ESP_LOGI(TAG, "=== BLE Scan Result Debug ===");
	ESP_LOGI(TAG, "Online devices count: %d (RSSI >= %d)", 
			Ble_Server_node_sum.online_node_cnt, BLE_SERVER_ISSI_ONLINE_LEVEL);
	ESP_LOGI(TAG, "Connection handle: %d", Ble_Server_node_sum.conn_handle);
	
	if (scan_completed) {
		ESP_LOGI(TAG, "Scan duration: %dms", BLE_SCAN_TIME_MS);
	}
	
	ESP_LOGI(TAG, "Device list:");
	
	uint8_t valid_device_count = 0;
	for (uint8_t i = 0; i < MAX_CNT_BLE_SERVER; i++) {
		if(is_mac_addr_empty(&Ble_Server_node_sum.node[i].peer_addr))
		{
			continue;
		}
		if (Ble_Server_node_sum.node[i].rssi > -120) { // 有效设备阈值
			valid_device_count++;
			
			// 格式化设备名称 (ASCII_SN)
			const char* device_name;
			if (Ble_Server_node_sum.node[i].ASCII_SN[0] != '\0') {
				device_name = Ble_Server_node_sum.node[i].ASCII_SN;
			} else {
				device_name = "<No Name>";
			}
			
			// 格式化MAC地址
			char mac_str[18] = {0};
			snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
					Ble_Server_node_sum.node[i].peer_addr.val[0],
					Ble_Server_node_sum.node[i].peer_addr.val[1],
					Ble_Server_node_sum.node[i].peer_addr.val[2],
					Ble_Server_node_sum.node[i].peer_addr.val[3],
					Ble_Server_node_sum.node[i].peer_addr.val[4],
					Ble_Server_node_sum.node[i].peer_addr.val[5]);
			
			// 设备状态
			const char* encrypt_str;
			
			switch (Ble_Server_node_sum.node[i].encrypt) {
				case BLE_ENCRYPT_TYPE_YES:
					encrypt_str = "ENCRYPTED";
					break;
				case BLE_ENCRYPT_TYPE_NO:
					encrypt_str = "UNENCRYPTED";
					break;
				default:
					encrypt_str = "UNKNOWN";
					break;
			}
			
			// 打印设备信息，重点显示ASCII_SN
			ESP_LOGI(TAG, "  [%02d] ASCII_SN: %s", i, device_name);
			ESP_LOGI(TAG, "       RSSI: %d dBm | MAC: %s | Encrypt: %s",
					Ble_Server_node_sum.node[i].rssi, mac_str, encrypt_str);
		}
	}
	
	if (valid_device_count == 0) {
		ESP_LOGW(TAG, "  No valid devices found");
	} else {
		ESP_LOGI(TAG, "Total valid devices: %d", valid_device_count);
	}
	
	ESP_LOGI(TAG, "=== End of BLE Scan Result ===");
}


/*

*/
void debug_print_adv_node(void)
{
	uint8_t i=0;

	
	ESP_LOGI(TAG,"Ble_Server_node_sum.online_node_cnt=  %d:\n",Ble_Server_node_sum.online_node_cnt); 
	ESP_LOGI(TAG,"ble_client_connect.ble_connect_step=  %d:\n",ble_client_connect.ble_connect_step); 
	
//	for ( i = 0; i < MAX_CNT_BLE_SERVER; i++)
//	{
//		if( Ble_Server_node_sum.node[i].rssi > BLE_SERVER_ISSI_ONLINE_LEVEL)//0 !=
//		{
//					ESP_LOGE(TAG, "node=%d,timeout_cnt=%d,rssi=%d,encrypt =%d",i,
//						Ble_Server_node_sum.node[i].timeout_cnt,Ble_Server_node_sum.node[i].rssi,
//						Ble_Server_node_sum.node[i].encrypt);
//			//		dump_buf_global(" ASCII_SN:", Ble_Server_node_sum.node[i].ASCII_SN, BLE_BROADCAST_SN_LEN);
//					
//					ESP_LOGI(TAG,"ASCII_SN xx  %s:\n",(char *)Ble_Server_node_sum.node[i].ASCII_SN); 
//		}
//	}

	ESP_LOGE(TAG, "Ble_Server_node_sum.conn_handle=%d\n", Ble_Server_node_sum.conn_handle);
	ESP_LOGE(TAG, "Ble_Server_node_sum.val_handle_FF01=%d\n", Ble_Server_node_sum.val_handle_FF01);
	ESP_LOGE(TAG, "Ble_Server_node_sum.val_handle_FF02_C2S_Wr=%d\n", Ble_Server_node_sum.val_handle_FF02_C2S_Wr);
	ESP_LOGE(TAG, "Ble_Server_node_sum.val_handle_FF03=%d\n", Ble_Server_node_sum.val_handle_FF03);

}



/*------------------------------------------------------------------------
*@Function :Ble_C2S_Msg_Send


BLE查询时序：
1.查询 16：获取协议版本；
if alpha:
read 160~188:获取 V/I/P DC IN

if beta:
read 1209-1289:获取 V/I/P DC IN

-------------------------------------------------------------------------*/
/**
*@brief  1000ms cycle
发送优先级：
modbus beta write
meter1/2
other

*@param[in]     None
*@param[out]    None
*@return         
*/
int Ble_C2S_Msg_Send(uint8_t *pMsg, uint16_t len, src_addr_info_t src_addr)
{
	int ret = -1;
	static uint16_t sCnt_time_out=0;	//接受读取rtn超时判断
	static uint8_t period_send_buf_flag = 0;//uart_read_state,0- empty,can send;1- not ready, stop to wait

	if((0xFF == Ble_Server_node_sum.conn_handle)
		||((BLE_CLIENT_STEP_ENCRYPT_GET_6_OK != ble_client_connect.ble_connect_step)
			&&(BLE_CLIENT_STEP_NORMAL != ble_client_connect.ble_connect_step)))
	{	
//		ESP_LOGI(TAG, "Ble_C Failed to Tx, conn=%d, step=%d", Ble_Server_node_sum.conn_handle, ble_client_connect.ble_connect_step);
		return -3; // 发送失败
	}

	if(0 == period_send_buf_flag)
	{
		ret = 1;
		period_send_buf_flag = 1;
		ble_client_connect.src_addr = src_addr;
		if (Ble_Client_Tx_Data(pMsg, len) != ESP_OK){
			ESP_LOGE(TAG, "Failed to app_ble_send_data_to_remote Ble data (len: %d) ", ble_client_connect.ble_client_Tx_len);
		}
		else{
			ESP_LOGE(TAG, "Ble_client RegAddress[%d] regNum[%d] Tx OK, version=%d", 
						ble_client_connect.src_addr.regAddr,
						ble_client_connect.src_addr.regNum,
						g_other_rd.bind_dev.modbus_version);
		}
	}
	
	if(1 == ble_client_connect.FlagRx_ok)
	{
		ble_client_connect.FlagRx_ok = 0;
		period_send_buf_flag = 0;
		sCnt_time_out = 0;
		// sCnt_big++; // 不再需要递增，因为只执行一种数据发送
		ret = 0; // 发送完成并应答
	}
	else
	{
		sCnt_time_out++;
		if(sCnt_time_out >= 5)//500ms
		{
			ble_client_connect.FlagRx_ok = 0;
			period_send_buf_flag = 0;
			sCnt_time_out = 0;
			// sCnt_big++; // 不再需要递增，因为只执行一种数据发送
			ret = -2; //发送超时
		}  
	}
	
	// 不再需要 sCnt_big 的循环逻辑，因为只发送 case 3 的数据

	return ret;
}


int Ble_C_Msg_Type_Send(uint8_t msgType, uint8_t slaveAddr, uint8_t *pIn, uint16_t inLen)
{
	int ret = -1;
	src_addr_info_t src_addr;
	sBleMdRet_t mdRet;
	uint8_t slaveAddrTemp;

	// 目标地址是0或者1是发给目标设备本身，否则是目标设备的子设备
	if(slaveAddr < 2)
	{
		// 根据设备名设置slave地址：AP300开头的设备使用地址0，其他设备使用地址1，AP300从机地址会动态改变，不能直接用1地址
		if (Ble_Server_node_sum.node_index < MAX_CNT_BLE_SERVER && 
			strncmp(Ble_Server_node_sum.node[Ble_Server_node_sum.node_index].ASCII_SN, "AP300", 5) == 0) {
			slaveAddrTemp = 0;
	//		ESP_LOGI(TAG, "AP300 device detected, using slave address 0");
		} else {
			slaveAddrTemp = 1;
	//		ESP_LOGI(TAG, "Non-AP300 device, using slave address 1");
		}
	}else{
		slaveAddrTemp = slaveAddr;
	}

	if (MODBUS_VERSION_ALPHA == g_other_rd.bind_dev.modbus_version) {
		mdRet = Ble_C_Md_Alpha_Msg_Build(msgType, slaveAddrTemp, pIn, inLen,  ble_client_connect.ble_client_Tx_buf);
	}else{
		mdRet = Ble_C_Md_Beta_Msg_Build(msgType, slaveAddrTemp, pIn, inLen,  ble_client_connect.ble_client_Tx_buf);
	}

	if(mdRet.txLen > 0)
	{
		src_addr.channel = MD_CHL_BLE_CLIENT;
		src_addr.regAddr = mdRet.regAddr;
		src_addr.regNum = mdRet.regNum;
		src_addr.slaveAddr = slaveAddrTemp;

		ret = Ble_C2S_Msg_Send(ble_client_connect.ble_client_Tx_buf, mdRet.txLen, src_addr);
	}

	return ret;
}


/*
Rx data解析
*/
void ble_client_Rx_data_prase(void)
{
	/* 尝试接收数据 */
	ble_data_t pdata = {0}; 

	if((ble_client_rx_queue)&&(pdTRUE == xQueueReceive(ble_client_rx_queue, &pdata, 0)))
	{		
		if (pdata.len > 0)
		{
			// dump_buf_global(" ble_client_Rx_data_prase, pri data:", pdata.data, pdata.len);

			if((ble_client_connect.ble_connect_step >= BLE_CLIENT_STEP_ENCRYPT_1)
				&&(ble_client_connect.ble_connect_step < BLE_CLIENT_STEP_ENCRYPT_GET_6_OK))
			{
				ble_client_Encrypt_step(pdata.data, pdata.len);
			}
			else if((BLE_CLIENT_STEP_ENCRYPT_GET_6_OK == ble_client_connect.ble_connect_step)
					||(BLE_CLIENT_STEP_NORMAL == ble_client_connect.ble_connect_step))
			{
				ble_client_connect.ble_client_Rx_lenB =0;
				if(BLE_ENCRYPT_TYPE_YES == Ble_Server_node_sum.node[Ble_Server_node_sum.node_index].encrypt)
				{
//					ESP_LOGI(TAG, "windy debug	ble_client_Rx_data_prase CC ");
//					ble_center_process_Decrypt(pdata.data, pdata.len, ble_client_connect.ble_client_Tx_buf,&ble_client_connect.ble_client_Tx_len);////临时借用ble_client_Tx_buf RAM
					aes_cbc_decrypt_receive_pack_run_in_client(pdata.data, pdata.len, ble_client_connect.ble_client_Tx_buf,&ble_client_connect.ble_client_Tx_len);
					if(pdata.len >= ble_client_connect.ble_client_Tx_len)//临时借用pdata.data RAM
					{
						memcpy(pdata.data,ble_client_connect.ble_client_Tx_buf,ble_client_connect.ble_client_Tx_len);//解密后数据
						pdata.len=ble_client_connect.ble_client_Tx_len;
						ble_client_connect.ble_client_Rx_bufB =pdata.data;
						ble_client_connect.ble_client_Rx_lenB =pdata.len;
						
//						dump_buf_global(" jiemi after data:", ble_client_connect.ble_client_Rx_bufB, ble_client_connect.ble_client_Rx_lenB);
					}
					else
					{
//						ESP_LOGI(TAG, "windy debug	ble_client_Rx_data_prase FF ");
					}
				
				}
				else if(BLE_ENCRYPT_TYPE_NO == Ble_Server_node_sum.node[Ble_Server_node_sum.node_index].encrypt)
				{
				
					ble_client_connect.ble_client_Rx_bufB =pdata.data;
					ble_client_connect.ble_client_Rx_lenB =pdata.len;
					// dump_buf_global(" pri ble client rx data:", ble_client_connect.ble_client_Rx_bufB, ble_client_connect.ble_client_Rx_lenB);
				}	

				if(0 != ble_client_connect.ble_client_Rx_lenB)
				{
					ble_client_connect.FlagRx_ok =1;
//					ESP_LOGI(TAG, "ble_client_connect.FlagRx_ok ");
					Ble_Client_Modbus_MasterRespones(ble_client_connect.src_addr,
														ble_client_connect.ble_client_Rx_bufB, ble_client_connect.ble_client_Rx_lenB);
				}
			}		
		}

		/* 释放接收蓝牙数据时分配的内存 */
		if (NULL != pdata.data) 
		{
			free(pdata.data);
			pdata.data= NULL;
		}
	}
}

/*
BLE客户端任务状态机，设备自动配对和连接管理统一状态机
功能：集成连接管理和电压匹配验证的完整流程
流程：
1. SCANNING: 扫描BLE设备，按RSSI排序
2. CONNECTING: 尝试连接设备（支持设备遍历）
3. MATCHING: 进行参数测试和验证
4. MATCH_SUCCESS: 物理连接验证成功
5. DISCONNECTING: 断开连接，尝试下一设备

注意：timeout超时计数由独立的FreeRTOS定时器处理（100ms周期）
*/
void ble_client_pairing_task(void)
{
	static uint8_t init_flag = 0;
	uint8_t i = 0;
	uint8_t msgData[10];
	uint16_t Vdc_set_temp = 0;
	int ret = -1;
	
	if(init_flag == 0){
		init_flag = 1;
		start_ble_scan(); // 提前开始启动蓝牙扫描
		pairing_control.scan_started = 1; // 标记已开始扫描
	}

	// 检查功能开关状态
	bool ble_client_enabled = 1;
	bool dcdc_enabled = 1;
	bool has_pairing_record = (strlen(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.BLE_Server_Type) != 0);
	
	if(sys_is_updating()){ // 如果正在升级则停止扫描
		stop_ble_scan();
		pairing_control.scan_started = 0;
		if(pairing_control.pairing_step != BLE_PAIRING_STEP_IDLE && pairing_control.pairing_step != BLE_PAIRING_STEP_INIT){ // 如果不是空闲状态或初始化状态则重置为初始化状态
			pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
			pairing_control.timeout = BLE_SCAN_IDLE_TIME_MS;
		}
		return;
	}
	// 如果BLE客户端功能关闭，清除配对记录并重置状态
	if (!ble_client_enabled) {
		if (has_pairing_record) {
			memset(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.BLE_Server_Type, 0, sizeof(top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.BLE_Server_Type));
			ESP_LOGW(TAG, "BLE client disabled, cleared pairing record");
		}
		// ESP_LOGW(TAG, "BLE client disabled, cleared pairing record, %d", g_self_data.mod_reg12000_IOT_set.on_off.bit.BLE_Client_connet_state);
		pairing_control.pairing_step = BLE_PAIRING_STEP_INIT; // 功能关闭时重置状态机
		pairing_control.timeout = BLE_SCAN_IDLE_TIME_MS;
		return;
	}
	
	// 如果DCDC总开关关闭，重置到初始化状态但保留配对记录
	if (!dcdc_enabled && pairing_control.pairing_step != BLE_PAIRING_STEP_IDLE) {
		// ESP_LOGW(TAG, "DCDC disabled, reset to INIT state");
		pairing_control.pairing_step = BLE_PAIRING_STEP_INIT; // 重置状态机，保留配对记录
		pairing_control.timeout = BLE_SCAN_IDLE_TIME_MS;
		return;
	}
	
	// 如果有配对记录且当前不在终态，直接进入成功状态（只执行一次）
	if (has_pairing_record && 
		pairing_control.pairing_step != BLE_PAIRING_STEP_MATCH_SUCCESS && 
		pairing_control.pairing_step != BLE_PAIRING_STEP_IDLE) {
		ESP_LOGI(TAG, "Found existing pairing record: %s, skipping auto-pairing", top_modbus_rd.Inv[reals.Addr_can_self].mod_reg11000_IOT_info.BLE_Server_Type);
		pairing_control.pairing_step = BLE_PAIRING_STEP_MATCH_SUCCESS;
	} 

	// 统一的BLE设备自动配对状态机 - 使用结构体管理所有状态
	switch (pairing_control.pairing_step) {
		case BLE_PAIRING_STEP_IDLE:
		{
			// 空闲状态：如果没有配对记录，重新启动配对流程
			if (!has_pairing_record) {
				ESP_LOGI(TAG, "IDLE state: No pairing record found, restarting pairing process");
				pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
			}
			break;
		}

		case BLE_PAIRING_STEP_INIT:
		{
			// 初始化状态，重置所有变量并处理空闲等待逻辑
			if (pairing_control.timeout == 0) {
				// 首次进入初始化状态或从异常状态恢复，完全重置所有变量
				pairing_control.local_pairing_set_step = 0;
				pairing_control.current_device_index = 0;
				pairing_control.device_try_count = 0;
				pairing_control.scan_started = 0; // 重置扫描状态标志
				pairing_control.connect_started = 0; // 重置连接状态标志
				
				// 强制断开现有连接
				if (is_ble_connected() || is_ble_connecting()) {
					force_disconnect_ble();
				}
				
				// 清空之前的数据，确保干净的初始状态
				memset(&g_other_rd.bind_dev, 0, sizeof(g_other_rd.bind_dev));
				memset(&g_other_wr.bind_dev, 0, sizeof(g_other_wr.bind_dev));
				
				// 初始化BLE客户端数据结构
				ble_client_data_init();

				pairing_control.pairing_step = BLE_PAIRING_STEP_SCAN_DEVICE;
				ESP_LOGI(TAG, "phy detect step: Initialized");
			}
			break;
		}

		case BLE_PAIRING_STEP_SCAN_DEVICE:
		{
			// 扫描BLE设备（timeout用于扫描超时计数）
			if (pairing_control.scan_started == 0) {
				// 刚进入扫描状态，开始扫描并设置超时
				start_ble_scan();
				pairing_control.timeout = BLE_SCAN_TIME_MS;
				pairing_control.scan_started = 1; // 标记已开始扫描
				ESP_LOGI(TAG, "phy detect step: Started scanning for devices, timeout: %dms", BLE_SCAN_TIME_MS);
			}
			
			if (pairing_control.timeout == 0) { //定时器已递减到0，扫描超时
				stop_ble_scan();
				
				// 计算在线设备数量
			  	ble_client_node_online_check();
				
				// 扫描完成后按RSSI排序，确保连接时优先尝试信号最强的设备
				sort_ble_nodes_by_rssi();
				ESP_LOGI(TAG, "phy detect step: BLE devices sorted by RSSI after scan completion");
				
#if BLE_SCAN_DEBUG_ENABLE
				// 调用debug函数打印详细的扫描结果
				debug_print_ble_scan_result(true);
#else
				// 简化输出
				ESP_LOGI(TAG, "phy detect step: Scan completed, found %d online devices (RSSI >= %d)", 
						Ble_Server_node_sum.online_node_cnt, BLE_SERVER_ISSI_ONLINE_LEVEL);
#endif
				
				if (Ble_Server_node_sum.online_node_cnt >= 1) {
					pairing_control.current_device_index = 0;
					pairing_control.device_try_count = 0;
					pairing_control.pairing_step = BLE_PAIRING_STEP_CONNECT_DEVICE;
					pairing_control.timeout = 0; // 重置超时计数器
					pairing_control.scan_started = 0; // 重置扫描状态标志
					pairing_control.connect_started = 0; // 重置连接状态标志
					// ESP_LOGI(TAG, "phy detect step: Starting connection attempts to %d devices", 
					// 		Ble_Server_node_sum.online_node_cnt);
				} else {
					// 没有找到设备，重新初始化等待后重新扫描
					ESP_LOGW(TAG, "phy detect step: No qualified devices found (need RSSI >= %d), reinitializing (%dms)...", 
							BLE_SERVER_ISSI_ONLINE_LEVEL, BLE_SCAN_IDLE_TIME_MS);
					pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
					pairing_control.timeout = BLE_SCAN_IDLE_TIME_MS; // 重置超时计数器
					pairing_control.scan_started = 0; // 重置扫描状态标志
				}
			}
			break;
		}

		case BLE_PAIRING_STEP_CONNECT_DEVICE:
		{
			// 检查是否超过最大尝试次数
			if (pairing_control.device_try_count >= MAX_DEVICE_TRY_COUNT || 
				pairing_control.current_device_index >= Ble_Server_node_sum.online_node_cnt) {
				ESP_LOGW(TAG, "phy detect step: Reached max attempts (%d) or no more devices, reinitializing (%dms)...", 
						MAX_DEVICE_TRY_COUNT, BLE_SCAN_IDLE_TIME_MS);
				pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
				pairing_control.timeout = BLE_SCAN_IDLE_TIME_MS; // 重置超时计数器
				pairing_control.connect_started = 0; // 重置连接状态标志
				break;
			}
			
			// 使用connect_started作为连接状态标志：0=准备连接，1=连接中，2=等待连接结果
			if (pairing_control.connect_started == 0) {
				// 第零步：检查设备加密类型，跳过无效设备
				if (Ble_Server_node_sum.node[pairing_control.current_device_index].encrypt == BLE_ENCRYPT_TYPE_INVALID) {
					// 确保ASCII_SN字符串安全打印
					char device_name[BLE_BROADCAST_SN_LEN + 1] = {0};
					memcpy(device_name, Ble_Server_node_sum.node[pairing_control.current_device_index].ASCII_SN, BLE_BROADCAST_SN_LEN);
					device_name[BLE_BROADCAST_SN_LEN] = '\0'; // 确保字符串终止
					
					ESP_LOGW(TAG, "phy detect step: Skipping device[%d]: '%s' (invalid encrypt type), trying next device", 
							pairing_control.current_device_index, device_name);
					
					// 跳过当前设备，尝试下一个
					pairing_control.current_device_index++;
					pairing_control.device_try_count++;
					pairing_control.connect_started = 0; // 保持状态，下次循环尝试下一个设备
					break; // 退出当前case，下次进入时处理下一个设备
				}
				
				// 仅测试 AC200PL2438000310180(len:20), AP3002514000116453(len:18)
				if((strncmp(Ble_Server_node_sum.node[pairing_control.current_device_index].ASCII_SN, "AC200PL2438000310180", 20))
					&& (strncmp(Ble_Server_node_sum.node[pairing_control.current_device_index].ASCII_SN, "AP3002514000116453", 18)))
				{
					// 确保ASCII_SN字符串安全打印
					char device_name[BLE_BROADCAST_SN_LEN + 1] = {0};
					memcpy(device_name, Ble_Server_node_sum.node[pairing_control.current_device_index].ASCII_SN, BLE_BROADCAST_SN_LEN);
					device_name[BLE_BROADCAST_SN_LEN] = '\0'; // 确保字符串终止
					ESP_LOGW(TAG, "phy detect step: Skipping device[%d]: '%s' (not test dev), trying next device", 
							pairing_control.current_device_index, device_name);
					// 跳过当前设备，尝试下一个
					pairing_control.current_device_index++;
					pairing_control.device_try_count++;
					pairing_control.connect_started = 0; // 保持状态，下次循环尝试下一个设备
					break; // 退出当前case，下次进入时处理下一个设备
				}
				
				// 第一步：如果已有连接，先断开
				if (is_ble_connected() || is_ble_connecting()) {
					ESP_LOGI(TAG, "phy detect step: Disconnecting existing BLE connection before connecting to device %d", 
							pairing_control.current_device_index);
					force_disconnect_ble();
					vTaskDelay(pdMS_TO_TICKS(100)); // 等待断开完成
				}
				
				// 第二步：检查是否正在扫描，如果是则先停止扫描
				if (ble_gap_disc_active()) {
					ESP_LOGI(TAG, "phy detect step: Stopping active scan before connection attempt");
					ble_gap_disc_cancel();
					vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延迟确保扫描完全停止
				}
				
				// 第三步：清空数据，确保每次连接尝试都有干净的数据环境
				memset(&g_other_rd.bind_dev, 0, sizeof(g_other_rd.bind_dev));
				memset(&g_other_wr.bind_dev, 0, sizeof(g_other_wr.bind_dev));

				ESP_LOGI(TAG, "phy detect step: Cleared g_other_rd and g_other_wr before connection attempt");
				
				// 第四步：开始连接尝试
				char device_name[BLE_BROADCAST_SN_LEN + 1] = {0};
				memcpy(device_name, Ble_Server_node_sum.node[pairing_control.current_device_index].ASCII_SN, BLE_BROADCAST_SN_LEN);
				device_name[BLE_BROADCAST_SN_LEN] = '\0'; // 确保字符串终止
				
				ESP_LOGI(TAG, "phy detect step: Attempting to connect to device[%d]: '%s', RSSI: %d, encrypt: %d, attempt %d/%d", 
						pairing_control.current_device_index, 
						device_name,
						Ble_Server_node_sum.node[pairing_control.current_device_index].rssi,
						Ble_Server_node_sum.node[pairing_control.current_device_index].encrypt,
						pairing_control.device_try_count + 1, MAX_DEVICE_TRY_COUNT);
				
				// 使用设备信息进行连接
				int rc = ble_gap_connect(Ble_Server_node_sum.own_addr_type, 
							&Ble_Server_node_sum.node[pairing_control.current_device_index].peer_addr, 
							BLE_CLIENT_DISCOVERY_TIME, NULL,
							ble_htp_cent_gap_event, NULL);
				
				if (rc == 0) {
					Ble_Server_node_sum.node_index = pairing_control.current_device_index;
					pairing_control.timeout = BLE_CONNECT_TIMEOUT_MS; // 设置连接超时
					pairing_control.connect_started = 1; // 标记连接中
					// ESP_LOGI(TAG, "phy detect step: Connection initiated successfully, waiting for result (timeout: %dms)", BLE_CONNECT_TIMEOUT_MS);
				} else {
					ESP_LOGW(TAG, "phy detect step: Failed to initiate connection to device %d, rc=%d, trying next device", 
							pairing_control.current_device_index, rc);
					// 连接发起失败，立即尝试下一个设备
					pairing_control.current_device_index++;
					pairing_control.device_try_count++;
					pairing_control.connect_started = 0; // 重置状态，下次循环尝试下一个设备
				}
			} else if (pairing_control.connect_started == 1) {
				// 连接中，检查连接结果（等待连接+服务发现完成）
				if (is_ble_connected() && (Ble_Server_node_sum.val_handle_FF02_C2S_Wr != 0))  {
					// 连接成功且服务发现完成（通过handle判断），进入电压设置阶段
					ESP_LOGI(TAG, "phy detect step: Connected successfully to device %d, starting modbus_ver check...", 
							pairing_control.current_device_index);
					pairing_control.pairing_step = BLE_PAIRING_STEP_READ_MD_VER;
					pairing_control.local_pairing_set_step = 1;  // 从第一步开始
					pairing_control.timeout = BLE_SET_TIMEOUT_MS;   // 重置超时计数器
					pairing_control.connect_started = 0; // 重置连接状态标志
				} else if (pairing_control.timeout == 0) {
					// 连接或服务发现超时，尝试下一个设备
					ESP_LOGW(TAG, "phy detect step: Connection/Discovery timeout (%dms) for device %d, trying next device...", 
							BLE_CONNECT_TIMEOUT_MS, pairing_control.current_device_index);
					pairing_control.current_device_index++;
					pairing_control.device_try_count++;
					pairing_control.connect_started = 0; // 重置连接状态标志
				}
			}
			break;
		}

		case BLE_PAIRING_STEP_READ_MD_VER:
			Ble_C_Msg_Type_Send(BLE_C_MSG_R_INV_VER, 0, msgData, 0);
			if(0 != g_other_rd.bind_dev.mod_reg00000.modbus_ver)
			{
				pairing_control.pairing_step = BLE_PAIRING_STEP_SET;
				pairing_control.local_pairing_set_step = 1;  // 从第一步开始
				pairing_control.timeout = BLE_PARAM_CHECK_TIMEOUT_MS;   // 重置超时计数器
				pairing_control.connect_started = 0; // 重置连接状态标志

				if(g_other_rd.bind_dev.mod_reg00000.modbus_ver >= MODBUS_VERSION_GAP) { // beta
					g_other_rd.bind_dev.modbus_version = MODBUS_VERSION_BETA;
				}else{
					g_other_rd.bind_dev.modbus_version = MODBUS_VERSION_ALPHA;
				}

				ESP_LOGW(TAG, "phy detect step: modbus_ver=%d, is OK, starting voltage matching...", g_other_rd.bind_dev.mod_reg00000.modbus_ver);
				break;
			}

			if(pairing_control.timeout == 0)
			{
				ESP_LOGW(TAG, "phy detect step: Trying next device...");
				pairing_control.local_pairing_set_step = 1;  // 重置验证步骤到第1步
				pairing_control.timeout = 0; // 重置超时计数器，立即开始下一个状态
				
				// 强制断开当前连接
				force_disconnect_ble();
				vTaskDelay(100);
				// 尝试下一个设备
				pairing_control.current_device_index++;
				pairing_control.device_try_count++;
				
				// 检查是否还有设备可以尝试
				if (pairing_control.device_try_count < MAX_DEVICE_TRY_COUNT && 
					pairing_control.current_device_index < Ble_Server_node_sum.online_node_cnt) {
					pairing_control.pairing_step = BLE_PAIRING_STEP_CONNECT_DEVICE;
					pairing_control.connect_started = 0;
					ESP_LOGI(TAG, "phy detect step: Moving to next device index %d (attempt %d/%d)", 
							pairing_control.current_device_index, pairing_control.device_try_count + 1, MAX_DEVICE_TRY_COUNT);
				} else {
					// 所有设备都尝试过了，重新初始化后重新扫描
					ESP_LOGW(TAG, "phy detect step: All devices tried, reinitializing (%dms)...", BLE_SCAN_IDLE_TIME_MS);
					pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
				}
			}
			break;

		case BLE_PAIRING_STEP_SET:
		{
			//ret = Ble_C2S_Match_Param_Set(pairing_control.local_pairing_set_step);
			if(MATCH_OK == ret)
			{
				pairing_control.pairing_step = BLE_PAIRING_STEP_CHECK;
				pairing_control.timeout = BLE_PARAM_CHECK_TIMEOUT_MS;
			}
			else if(MATCH_PARAM_ERR == ret)
			{ // 步骤无效，初始化到step1
				pairing_control.local_pairing_set_step = 1;
				pairing_control.pairing_step = BLE_PAIRING_STEP_CHECK;
				pairing_control.timeout = BLE_PARAM_CHECK_TIMEOUT_MS;
			}
			else
			{
				// 参数设置不匹配，继续等待直到超时
				if (pairing_control.timeout == 0)
				{
					ESP_LOGW(TAG, "phy detect step: Param Set Timeout...");
					ESP_LOGW(TAG, "phy detect step: Trying next device...");
					pairing_control.local_pairing_set_step = 1;  // 重置验证步骤到第1步
					pairing_control.timeout = BLE_PARAM_CHECK_TIMEOUT_MS; // 重置超时计数器，立即开始下一个状态
					
					// 强制断开当前连接
					force_disconnect_ble();
					vTaskDelay(100);
					// 尝试下一个设备
					pairing_control.current_device_index++;
					pairing_control.device_try_count++;
					
					// 检查是否还有设备可以尝试
					if (pairing_control.device_try_count < MAX_DEVICE_TRY_COUNT && 
						pairing_control.current_device_index < Ble_Server_node_sum.online_node_cnt) {
						pairing_control.pairing_step = BLE_PAIRING_STEP_CONNECT_DEVICE;
						pairing_control.connect_started = 0;
						ESP_LOGI(TAG, "phy detect step: Moving to next device index %d (attempt %d/%d)", 
								pairing_control.current_device_index, pairing_control.device_try_count + 1, MAX_DEVICE_TRY_COUNT);
					} else {
						// 所有设备都尝试过了，重新初始化后重新扫描
						ESP_LOGW(TAG, "phy detect step: All devices tried, reinitializing (%dms)...", BLE_SCAN_IDLE_TIME_MS);
						pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
					}
				}
			}
			break;
		}

		case BLE_PAIRING_STEP_CHECK:
		{
			//ret = Ble_C2S_Match_Check(pairing_control.local_pairing_set_step);
			if(MATCH_CPL == ret)
			{
				// 所有步骤验证都完成，认为物理连接在一起
				pairing_control.pairing_step = BLE_PAIRING_STEP_MATCH_SUCCESS;
				pairing_control.timeout = BLE_PERIOD_READ_MS; // 重置超时计数器
				ESP_LOGI(TAG, "BLE device pairing: MATCH SUCCESS! All-step verification completed.");
			}
			else if(MATCH_OK == ret)
			{
				// 匹配成功，进入下一步
				ESP_LOGI(TAG, "BLE device pairing: Step[%d] Param matched!", pairing_control.local_pairing_set_step);
				// 继续下一个步骤验证
				pairing_control.pairing_step = BLE_PAIRING_STEP_SET;
				pairing_control.local_pairing_set_step++;
				ESP_LOGI(TAG, "BLE device pairing: Moving to step[%d]", pairing_control.local_pairing_set_step);
			}
			else 
			{
				// 验证不匹配，继续等待直到超时
				if (pairing_control.timeout == 0)
				{
					// 定时器已递减到0，超时内都没有匹配，认为匹配失败，尝试下一个设备
				#if BLE_SCAN_DEBUG_ENABLE
					if (MODBUS_VERSION_ALPHA == g_other_rd.bind_dev.modbus_version) {
							// Alpha版本
							ESP_LOGW(TAG, "phy detect step: check timeout (%dms) on device %d step %d - Alpha protocol", 
									BLE_PARAM_CHECK_TIMEOUT_MS, pairing_control.current_device_index, pairing_control.local_pairing_set_step);
						
					} else if (MODBUS_VERSION_BETA == g_other_rd.bind_dev.modbus_version) {
							// Beta版本
							ESP_LOGW(TAG, "phy detect step: check timeout (%dms) on device %d step %d - Beta protocol", 
									BLE_PARAM_CHECK_TIMEOUT_MS, pairing_control.current_device_index, pairing_control.local_pairing_set_step);
						
					}
				#endif
					
					ESP_LOGW(TAG, "phy detect step: Trying next device...");
					pairing_control.local_pairing_set_step = 1;  // 重置验证步骤到第1步
					pairing_control.timeout = 0; // 重置超时计数器，立即开始下一个状态
					
					// 强制断开当前连接
					force_disconnect_ble();
					vTaskDelay(100);
					// 尝试下一个设备
					pairing_control.current_device_index++;
					pairing_control.device_try_count++;
					
					// 检查是否还有设备可以尝试
					if (pairing_control.device_try_count < MAX_DEVICE_TRY_COUNT && 
						pairing_control.current_device_index < Ble_Server_node_sum.online_node_cnt) {
						pairing_control.pairing_step = BLE_PAIRING_STEP_CONNECT_DEVICE;
						pairing_control.connect_started = 0;
						ESP_LOGI(TAG, "phy detect step: Moving to next device index %d (attempt %d/%d)", 
								pairing_control.current_device_index, pairing_control.device_try_count + 1, MAX_DEVICE_TRY_COUNT);
					} else {
						// 所有设备都尝试过了，重新初始化后重新扫描
						ESP_LOGW(TAG, "phy detect step: All devices tried, reinitializing (%dms)...", BLE_SCAN_IDLE_TIME_MS);
						pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
					}
				}
			}
			break;
		}

		case BLE_PAIRING_STEP_MATCH_SUCCESS:
		{
//			ESP_LOGI(TAG, "BLE voltage matching: Physical connection verified, data handle...");
			// 匹配完成后,进行数据交互
			if(is_ble_connected())
			{
				pairing_control.device_try_count = 0;
				//Ble_C2S_Match_Succ_Handle();
			}
			else
			{
				if(0 == pairing_control.timeout)
				{
					pairing_control.device_try_count++;
					// 断开重连机制
					if(pairing_control.device_try_count > 30)
					{
						pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
						pairing_control.device_try_count = 0;
						pairing_control.timeout = 0;
						ESP_LOGE(TAG, "phy detect : re init!!!");
					}
					else
					{
						ESP_LOGW(TAG, "reconnect try(%d)!!!", pairing_control.device_try_count);
						// 第一步：先断开连接
						force_disconnect_ble();
						vTaskDelay(pdMS_TO_TICKS(100)); // 等待断开完成
						
						// 第二步：检查是否正在扫描，如果是则先停止扫描
						if (ble_gap_disc_active()) {
							ESP_LOGI(TAG, "phy detect step: Stopping active scan before connection attempt");
							ble_gap_disc_cancel();
							vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延迟确保扫描完全停止
						}
						
						// 使用设备信息进行连接
						ble_gap_connect(Ble_Server_node_sum.own_addr_type, 
									&Ble_Server_node_sum.node[pairing_control.current_device_index].peer_addr, 
									BLE_CLIENT_DISCOVERY_TIME, NULL,
									ble_htp_cent_gap_event, NULL);
						
						pairing_control.timeout = BLE_CONNECT_TIMEOUT_MS;
					}	
				}
			}
			break;
		}

		default:
		{
			// 未知状态，重置为初始化状态
			ESP_LOGW(TAG, "phy detect step: Unknown step %d, resetting to INIT", pairing_control.pairing_step);
			pairing_control.pairing_step = BLE_PAIRING_STEP_INIT;
			pairing_control.timeout = BLE_SCAN_IDLE_TIME_MS; // 重置超时计数器，让INIT状态进行完整初始化
			break;
		}
	}
}

void ble_c2s_msg_queue_handle(void)
{
	ble_c2s_msg_t data;
	static uint8_t msg_handle_flag = 0;
	int ret = 0;

	if((BLE_PAIRING_STEP_MATCH_SUCCESS != pairing_control.pairing_step) || (is_ble_connected() == false))
	{
		msg_handle_flag = 0;
		if (ble_c2s_send_queue && pdTRUE == xQueueReceive(ble_c2s_send_queue, &data, 0))
		{
			// 释放数据内存
	        if (data.data)
	        {
	            free(data.data);
	            data.data = NULL;
	        }
		}
	}
	else
	{
		if(msg_handle_flag)
		{
			ret = Ble_C2S_Msg_Send(gC2sMsgBck.msg, gC2sMsgBck.len, gC2sMsgBck.src_addr);
			if((1 == ret) || (-3 == ret)){
				msg_handle_flag = 0;
			}
		}
		else
		{
			if (ble_c2s_send_queue && pdTRUE == xQueueReceive(ble_c2s_send_queue, &data, 0))
		    {
				ret = Ble_C2S_Msg_Send(data.data, data.len, data.src_addr);
				if(1 != ret)
				{
					if(data.len < 256)
					{
						msg_handle_flag = 1;
						gC2sMsgBck.len = data.len;
						memcpy(gC2sMsgBck.msg, data.data, data.len);
						gC2sMsgBck.src_addr = data.src_addr;
					}
				}

		        // 释放数据内存
		        if (data.data)
		        {
		            free(data.data);
		            data.data = NULL;
		        }
		    }
		}
	}
	
}

int ble_c2s_send_to_queue(const uint8_t *data_buf, uint16_t len, src_addr_info_t src_addr)
{
	ble_c2s_msg_t msg_send;

	if((BLE_PAIRING_STEP_MATCH_SUCCESS != pairing_control.pairing_step) || (is_ble_connected() == false)){
		ESP_LOGE (TAG, "ble_c2s_send_to_queue pairing not cpl");
		return -1;
	}

	msg_send.data = iot_calloc(len);
	if(NULL == msg_send.data){
		ESP_LOGE (TAG, "ble_c2s_send_to_queue malloc failed");
		return -1;
	}

	memcpy(msg_send.data, data_buf, len);
	msg_send.len = len;
	msg_send.src_addr = src_addr;

	if (xQueueSendToBack(ble_c2s_send_queue, &msg_send, pdMS_TO_TICKS(0)) != pdTRUE)
	{
		ESP_LOGE (TAG, " ble_c2s_send_to_queue failed");
		free(msg_send.data);
		return -1;
	}

    return 0;
}

/**
 * @brief BLE配对超时定时器回调函数
 * @param xTimer 定时器句柄
 * 
 * 该函数每100ms执行一次，用于递减pairing_control.timeout
 * timeout直接设置为毫秒值，每次递减100ms
 * 例如：2000ms超时 -> timeout = 2000，每次递减100
 * 当timeout小于等于100ms时停止递减，避免溢出
 */
static void ble_pairing_timer_callback(TimerHandle_t xTimer)
{
	// 只有当timeout大于BLE_CLIENT_DETECTOR_PERIOD时才递减，避免溢出
	if (pairing_control.timeout >= BLE_CLIENT_DETECTOR_PERIOD) {
		pairing_control.timeout -= BLE_CLIENT_DETECTOR_PERIOD;
	} else {
		// 如果剩余时间小于100ms，直接设为0表示超时
		pairing_control.timeout = 0;
	}
}


/**
 * @brief 创建并启动BLE配对超时定时器
 * @return ESP_OK - 成功, ESP_FAIL - 失败
 */
esp_err_t ble_pairing_timer_init(void)
{
	// 如果定时器已存在，先删除
	if (ble_pairing_timer != NULL) {
		xTimerDelete(ble_pairing_timer, portMAX_DELAY);
		ble_pairing_timer = NULL;
	}
	
	// 创建周期性定时器，周期为100ms
	ble_pairing_timer = xTimerCreate(
		"BLE_Pairing_Timer",           // 定时器名称
		pdMS_TO_TICKS(BLE_CLIENT_DETECTOR_PERIOD), // 定时器周期：100ms
		pdTRUE,                        // 自动重载：true
		(void*)0,                      // 定时器ID
		ble_pairing_timer_callback     // 回调函数
	);
	
	if (ble_pairing_timer == NULL) {
		ESP_LOGE(TAG, "Failed to create BLE pairing timer");
		return ESP_FAIL;
	}
	
	// 启动定时器
	if (xTimerStart(ble_pairing_timer, portMAX_DELAY) != pdPASS) {
		ESP_LOGE(TAG, "Failed to start BLE pairing timer");
		xTimerDelete(ble_pairing_timer, portMAX_DELAY);
		ble_pairing_timer = NULL;
		return ESP_FAIL;
	}
	
	// 创建BLE节点数组互斥锁
	ble_node_mutex = xSemaphoreCreateMutex();
	if (ble_node_mutex == NULL) {
		ESP_LOGE(TAG, "Failed to create BLE node mutex");
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "BLE node mutex created successfully");
	ESP_LOGI(TAG, "BLE pairing timer started successfully (period: %dms)", BLE_CLIENT_DETECTOR_PERIOD);
	return ESP_OK;
}

void ble_client_init(void)
{
	if (NULL == ble_c2s_send_queue)
    {
        ble_c2s_send_queue = xQueueCreate(5, sizeof(ble_c2s_msg_t));
        assert(ble_c2s_send_queue != NULL);
    }

	ble_pairing_timer_init();
}

void ble_client_task(void)
{
	ble_c2s_msg_queue_handle();
	ble_client_pairing_task();
}


#endif

