#include "wl_mesh.h"
#include "mesh_api.h"

#include "filesystem.h"
#include "uart_device_process.h"
#include "modbus_slave.h"
#include "modbus_data.h"
#include "can_protocol.h"
#include "iot_period_task.h"
#include "comm_define.h"
#include "iot_wifi_init.h"
#include "dev_discovery.h"
#include "udp_multicast.h"
#include "xmodem_client.h"
#include "uart_ota.h"
#include "utils.h"
#include "dev_discovery.h"

#define TAG     "[Wl_mesh]"

WIFI_MESH_INTERFACE_STRUCT wireless_interface;

/*
wifi mesh主节点信息
*/
mesh_node_master_t g_master_info = {
    .dev_sn.sn = 0,
    .dev_sn.dev_type = 0,
    .priority = 0,
    .parallel_seq = 0
};

typedef struct {
    uint8_t *msg_ptr;
    uint16_t msg_len;
}msg_struct;
QueueHandle_t wifi_mesh_rx_queue;//wifi_mesh rx 队列

#define	WIFI_MESH_GET_RTN_DISCOVERY             1   //discovery
#define	WIFI_MESH_GET_RTN_APPLICATION           2   //modbus
#define	WIFI_MESH_GET_RTN_APPLICATION_XMODEM    3   //xmodem
#define WIFI_MESH_CYCLE_REPORT                  4   //S1周期上报

#define XMODEM_FRAME_INDEX_VER                  0
#define XMODEM_FRAME_INDEX_SEQ_L                1
#define XMODEM_FRAME_INDEX_SEQ_H                2
#define XMODEM_FRAME_INDEX_ACK                  3

// static int handle_mesh_data(uint8_t *in_data, uint16_t data_len);
static int parse_mesh_data_report(uint8_t *in_data, uint16_t data_len, uint8_t **outbuf);
static uint8_t add_discovery_dev(uint8_t *sn, uint16_t priority);
static void del_discovery_dev(uint16_t dev_type, uint64_t sn);
static int8_t get_slave_address(uint16_t dev_type, uint64_t sn);
static uint16_t modbus_slave_write_multi_regs(const uint8_t *in_data, uint16_t data_len,
                                        uint8_t *response, uint16_t slaveAddr);
void iot_mesh_node_change_cb(int16_t index, uint8_t type, uint64_t dev_sn,
                             uint16_t dev_type, uint16_t priority, uint8_t parallel_seq);

static void wifi_mesh_dev_cnt_timeout_check(void);
static void WIFI_MESH_Get_Rx_Data(void);
static void WIFI_MESH_Rx_process(void);
static int8_t wifi_mesh_rx_data_handle(uint8_t *data, int len);
static int wifi_mesh_rx_frame_parse(uint8_t *buff, int len);
static uint8_t wifi_mesh_rx_data_Xmodem_Process( uint8_t *buff, int len);

/**
 * @brief mesh数据处理任务
 */
void wl_mesh_task(void *param)
{
    ESP_LOGI(TAG, "Wireless mesh task start...");
#ifdef CONFIG_BLUETTI_WLAN_MESH_SUPPORTED
    init_mesh_config(iot_factory.iot_sn, SN_TYPE_SELF, MESH_DEV_PRIORITY,
                        MESH_DEV_PARALLEL_SEQ, reals.Addr_can_master);
    register_mesh_node_change_cb(iot_mesh_node_change_cb);
    
    wifi_mesh_rx_queue = xQueueCreate(5, sizeof(msg_struct));
    esp_log_level_set("[Mesh]", ESP_LOG_NONE);
    while(1)
    {
        iot_mesh_task();

        WIFI_MESH_Get_Rx_Data();
        WIFI_MESH_Rx_process();

        // wifi_mesh_dev_cnt_timeout_check();

        vTaskDelay(pdMS_TO_TICKS(50));
    }
#endif
}

/**
 * @brief 处理mesh数据
 * @param[in] from_parent 数据来源, true:来自父节点, false:来自子节点
 * @param[in] in_data 输入数据
 * @param[in] data_len 数据长度
 * @return 成功返回0，失败返回-1
 */
// static int handle_mesh_data(uint8_t *in_data, uint16_t data_len)
// {
//     int ret = 0;
//     if(NULL == in_data || 0 == data_len)
//     {
//         ESP_LOGE(TAG, "Invalid input parameters");
//         return -1;
//     }

//     int8_t frame_type = in_data[WL_DEV_PRO_ADDR_FUNC_CODE] & 0xFF;
//     switch (frame_type)
//     {
//         case WL_DEV_PRO_FUNC_REPORT:
//         {
//             uint8_t *data_buf = NULL;
//             int buf_len = parse_mesh_data_report(in_data, data_len, &data_buf);
//             if (0 >= buf_len)
//             {
//                 if (NULL != data_buf)
//                 {
//                     free(data_buf);
//                     data_buf = NULL;
//                 }
//                 break;
//             }

//             uint64_t sn = *((uint64_t *)&in_data[WL_DEV_PRO_ADDR_SN_SRC]);
//             uint16_t dev_type = *((uint16_t *)&in_data[WL_DEV_PRO_ADDR_TYPE_SRC]);
//             int8_t slave_addr = get_slave_address(dev_type, sn);
//             if (0 > slave_addr)
//             {
//                 ESP_LOGE(TAG, "mesh report data:get slave address error");
//             }
//             modbus_slave_write_multi_regs(data_buf, buf_len, NULL, slave_addr);

//             if (NULL != data_buf)
//             {
//                 free(data_buf);
//                 data_buf = NULL;
//             }
//         }
//             break;
//         default:
//         {
//             ESP_LOGE(TAG, "Receive mesh data type error:%d, length:%d", frame_type, data_len);
//         }
//             break;
//     }

//     return ret;
// }

void iot_mesh_node_change_cb(int16_t index, uint8_t type, uint64_t dev_sn,
                             uint16_t dev_type, uint16_t priority, uint8_t parallel_seq)
{
    ESP_LOGI(TAG, "iot mesh node change cb, type:%d, sn:%llu, dev_type:%d, priority:%d, parallel_seq:%d",
                        type, dev_sn, dev_type, priority, parallel_seq);

    uint8_t sn[10] = {0};
    memcpy(sn, &dev_sn, 8);
    memcpy(&sn[8], &dev_type, 2);

    if (MESH_NODE_STATUS_ONLINE == type)
    {
        add_discovery_dev(sn, priority);
    }
    else if (MESH_NODE_STATUS_OFFLINE == type)
    {
        del_discovery_dev(dev_type, dev_sn);
    }
}

/**
 * @brief 解析mesh周期上报数据
 * @param[in] in_data 输入数据
 * @param[in] data_len 数据长度
 * @param[out] out_data 输出数据
 * @return 成功返回数据长度，失败返回<0
 */
static int parse_mesh_data_report(uint8_t *in_data, uint16_t data_len, uint8_t **out_data)
{
    if(NULL == in_data || 0 == data_len)
    {
        ESP_LOGE(TAG, "Invalid input parameters");
        return -1;
    }

#ifdef CONFIG_MESH_DEV_DISCOVERY_SUPPORTED
    uint8_t dev_sn[10] = {0};
    memcpy(dev_sn, &in_data[WL_DEV_PRO_ADDR_SN_SRC], sizeof(dev_sn));
#endif

    uint8_t ttl = in_data[WL_DEV_PRO_ADDR_TTL];
    if (0 == ttl)
    {
        ESP_LOGE(TAG, "Invalid ttl:%d", ttl);
        return -2;
    }
    in_data[WL_DEV_PRO_ADDR_TTL] = ttl - 1;
    //更新TTL字段，需要更新crc校验码
    uint16_t crc_value = calcu_crc16(in_data, data_len - 2);
    in_data[data_len - 2] = (uint8_t) crc_value;
    in_data[data_len - 1] = (uint8_t)(crc_value >> 8);

#ifdef CONFIG_MESH_DEV_DISCOVERY_SUPPORTED
    // uint16_t priority = in_data[WL_DEV_PRO_ADDR_REPORT_PRIORITY]
    //                   | in_data[WL_DEV_PRO_ADDR_REPORT_PRIORITY + 1] << 8;
    // uint8_t parallel_seq = in_data[WL_DEV_PRO_ADDR_REPORT_PARALLEL_SEQ];
    //收到消息时，添加到发现设备列表
    // add_discovery_dev((uint8_t *)dev_sn, priority);
#endif

    uint8_t *node_offset = &in_data[WL_DEV_PRO_ADDR_REPORT_LAYER];
    uint8_t layer = *node_offset;
    node_offset += 2;
    node_offset += (4 * layer);

    uint8_t out_data_len = (*node_offset) | (*(node_offset + 1)) >> 8;
    if (0 == out_data_len)
    {
        ESP_LOGE(TAG, "parse mesh report data len error");
        return 0;
    }
    node_offset += 2;

    *out_data = heap_caps_malloc(out_data_len, MALLOC_CAP_SPIRAM);
    if (NULL == *out_data)
    {
        ESP_LOGE(TAG, "malloc buffer for mesh report data failed");
        return -4;
    }

    memcpy(*out_data, node_offset, out_data_len);

    return out_data_len;
}

/**
 * @brief 通过设备类型和SN获取modbus从机地址
 * @param[in] dev_type 设备型号
 * @param[in] sn 设备序列号
 * @return 成功返回从机地址，失败返回-1
 */
static int8_t get_slave_address(uint16_t dev_type, uint64_t sn)
{
    uint8_t bias = 0;
    uint8_t slave_addr = 0;
    int j = 0;

    ESP_LOGI(TAG, "Get slave address, dev_type:%u, sn:%llu", dev_type, sn);

    if(SN_TYPE_SELF == dev_type)
    {
        for (j = 0; j < (reals.Topnet_point_Num_invbat); j++)
        {
            DISCOVERY_POINT_InfoStruct *dev_info = &reals.discovery_net_Info[reals.Topseq_Invbat_index[j]];
            //ESP_LOGW(TAG, "self discovery_net_Info[%d] sn:%llu", j, *((uint64_t *)dev_info->SN));
            if((sn == *((uint64_t *)dev_info->SN))//sn
                && (sn != 0))//已存储SN
            {
                reals.discovery_net_Info[reals.Topseq_Invbat_index[j]].net_point_TimeOut_cnt = 0;
                if(dev_info->ptr_modbus_data >= (uint32_t)&Inv[0])
                {
                    bias = (dev_info->ptr_modbus_data -(uint8_t *)&Inv[0])/sizeof(Inv[0]);
                }
                else
                {
                    ESP_LOGE(TAG, "ptr_modbus_data error "  );
                }
                slave_addr = bias  + MODBUS_SLAVE_ADDR_WIFI_INVBAT_START;
                ESP_LOGI(TAG, "Self SN:%llu, tpye:%u, slave_addr:%d",
                            sn, dev_type, slave_addr);

                return slave_addr;
            }
        }
    }
    else if(SN_TYPE_S1 == dev_type)
    {
        for (j = 0; j < (reals.Topnet_point_Num_S1); j++)
        { 
            ESP_LOGW(TAG, "s1 discovery_net_Info[%d] sn:%llu",
                j, *((uint64_t *)reals.discovery_net_Info[reals.Topseq_S1_index[j]].SN));
            if((sn == *((uint64_t *)reals.discovery_net_Info[reals.Topseq_S1_index[j]].SN))//sn
                &&(sn != 0))//已存储SN
            {
                reals.discovery_net_Info[reals.Topseq_S1_index[j]].net_point_TimeOut_cnt = 0;
                if(reals.discovery_net_Info[reals.Topseq_S1_index[j]].ptr_modbus_data >= (uint32_t)&Plug[0])
                {
                    bias = (reals.discovery_net_Info[reals.Topseq_S1_index[j]].ptr_modbus_data -(uint8_t *)&Plug[0])/sizeof(Plug[0]);					
                }
                else
                {
                    ESP_LOGE(TAG, "ptr_modbus_data error "  );
                }
                slave_addr = bias  + MODBUS_SLAVE_ADDR_WIFI_S1_START;

                ESP_LOGI(TAG, "S1 SN:%llu, tpye:%u, slave_addr=%d",
                    sn, dev_type, slave_addr);

                return slave_addr;
            }
        }
    }
    else
    {
        for (j = 0; j < (reals.Topnet_point_Num_mix); j++)
        { 
            if((sn == *((uint64_t *)reals.discovery_net_Info[reals.Topseq_mix_index[j]].SN))
                &&(sn != 0))//已存储SN
            {
                reals.discovery_net_Info[reals.Topseq_mix_index[j]].net_point_TimeOut_cnt = 0;
                bias = 0;
                if(reals.discovery_net_Info[reals.Topseq_mix_index[j]].ptr_modbus_data >= (uint8_t *)&Plug[0])
                {
                    ESP_LOGE(TAG, "ptr_modbus_data error,tbd ");
                }
                else
                {
                    ESP_LOGE(TAG, "ptr_modbus_data error ");
                }
                slave_addr = bias + MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START;
                ESP_LOGI(TAG, "Mix SN:%llu, tpye:%u, slave_addr=%d",
                    sn, dev_type, slave_addr);

                return slave_addr;
            }
        }
    }

    return -1;
}

/**
 * @brief 添加设备到设备发现列表
 * @param[in] sn 设备SN+type
 * @param[in] priority 设备优先级
 * @return 新添加返回1, 已存在返回0
 */
static uint8_t add_discovery_dev(uint8_t *sn, uint16_t priority)
{
    uint8_t j = 0;
    uint8_t n = 0;

    for (j = 0; j < NET_WIFI_MAX_POINT; j++)//新设备填充
    {
        if (memcmp(sn, reals.discovery_net_Info[j].SN, 10) == 0)//same
        {
            if(reals.discovery_net_Info[j].net_point_online == NET_POINT_OFFLINE_HALF) 
            {
                //ESP_LOGW(TAG, "[add_discovery_dev] device update. sn(%llu)", *((uint64_t *)&reals.discovery_net_Info[j].SN[0]));
            }

            reals.discovery_net_Info[j].net_point_online = NET_POINT_ONLINE;
            reals.discovery_net_Info[j].net_point_TimeOut_cnt = 0;

            return 0;//已存设备，不再填充
        }
        else if(NET_POINT_OFFLINE == reals.discovery_net_Info[j].net_point_online)//新设备填充到前面，不在线的设备位置
        { 
            memcpy(&reals.discovery_net_Info[j].SN[0], sn, 10);

            if(0 != priority)
            {
                reals.discovery_net_Info[j].priority = priority;
            }

            reals.discovery_net_Info[j].net_point_online = NET_POINT_ONLINE;
            reals.discovery_net_Info[j].net_point_TimeOut_cnt =0;

            //ESP_LOGW(TAG, "[add_discovery_dev] Get new device(sn:%llu), dev(%d)", *((uint64_t *)&reals.discovery_net_Info[j].SN[0]), j);
            return 1;//完成存储
        }
    }

    return 0;//在线设备已满
}

static void del_discovery_dev(uint16_t dev_type, uint64_t sn)
{
    int j = 0;

    ESP_LOGI(TAG, "Get slave address, dev_type:%u, sn:%llu", dev_type, sn);

    if(SN_TYPE_SELF == dev_type)
    {
        for (j = 0; j < (reals.Topnet_point_Num_invbat); j++)
        {
            DISCOVERY_POINT_InfoStruct *dev_info = &reals.discovery_net_Info[reals.Topseq_Invbat_index[j]];
            if((sn == *((uint64_t *)dev_info->SN))//sn
                && (sn != 0))//已存储SN
            {
                reals.discovery_net_Info[reals.Topseq_Invbat_index[j]].net_point_online = NET_POINT_ONLINE;
                ESP_LOGI(TAG, "Delete discovery dev SN:%llu, tpye:%u", sn, dev_type);
                return;
            }
        }
    }
    else if(SN_TYPE_S1 == dev_type)
    {
        for (j = 0; j < (reals.Topnet_point_Num_S1); j++)
        { 
            if((sn == *((uint64_t *)reals.discovery_net_Info[reals.Topseq_S1_index[j]].SN))//sn
                &&(sn != 0))//已存储SN
            {
                reals.discovery_net_Info[reals.Topseq_S1_index[j]].net_point_online = NET_POINT_ONLINE;
                ESP_LOGI(TAG, "Delete discovery s1 dev SN:%llu, tpye:%u", sn, dev_type);
                return;
            }
        }
    }
    else
    {
        for (j = 0; j < (reals.Topnet_point_Num_mix); j++)
        { 
            if((sn == *((uint64_t *)reals.discovery_net_Info[reals.Topseq_mix_index[j]].SN))
                &&(sn != 0))//已存储SN
            {
                reals.discovery_net_Info[reals.Topseq_mix_index[j]].net_point_online = NET_POINT_ONLINE;
                ESP_LOGI(TAG, "Delete discovery s1 dev SN:%llu, tpye:%u", sn, dev_type);
                return;
            }
        }
    }
}

/**
 * @brief 从机写多个寄存器
 * @param[in] in_data 输入数据
 * @param[in] data_len 输入数据长度
 * @param[out] response 输出数据
 * @param[in] slaveAddr 从机地址
 * @return 返回输出数据长度
 */
static uint16_t modbus_slave_write_multi_regs(const uint8_t *in_data, uint16_t data_len,
                                        uint8_t *response, uint16_t slaveAddr)
{
    uint16_t i = 0;
    uint16_t j = 0;
    reg2_position_t reg_position = {0};
    uint16_t *p_tab2 = NULL;
    uint16_t write_reg_data = 0;

    if (in_data == NULL) {
        ESP_LOGE(TAG, "Invalid input parameters, in_data is NULL");
        return 0;
    }

    uint16_t start_address  = in_data[2]<<8 | in_data[3]; // 写入寄存器地址
    uint16_t write_regs_cnt  = in_data[4]<<8 | in_data[5]; // 写入寄存器数量

    if (data_len < 9 || (data_len - 9) != (write_regs_cnt * 2))
    {
        ESP_LOGE(TAG, "Invalid input parameters, data_len:%d, write_regs_cnt:%d", data_len, write_regs_cnt);
        if(response != NULL)
        {
            return Modbus_Error(response, BAD_COUNT);
        }
        return 0;
    }

    /** 查找读寄存器数据缓存地址<数据不保存，直接写入到读寄存器, 与UDP广播功能保持一致> */
    p_tab2 = vLookupDataTab_Can(1, slaveAddr, start_address,
                                write_regs_cnt, 0, NULL, NULL, &reg_position);
    if(p_tab2 != NULL)
    {
        for ( i = 0; i < write_regs_cnt; i++) 
        {
            // 直接修改寄存器数据
            write_reg_data = ((uint16_t)in_data[7 + 2 * i]<<8) | in_data[8 + 2 * i];
            *(p_tab2+i) = write_reg_data;
            if (13 == i)
            {
                ESP_LOGW(TAG, " -----Report data %d:%d ", i, write_reg_data);
            }
            else if (18 == i)
            {
                ESP_LOGW(TAG, " -----Smart Plug State %d, %d", write_reg_data, write_reg_data & 0xC000);
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "get modbus data buffer error");
        if(response != NULL)
        {
            return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
        }
        return 0;
    }

    if(response != NULL)
    {
        response[j++] = in_data[0];
        response[j++] = 0x10;
        response[j++] = in_data[2];
        response[j++] = in_data[3];
        response[j++] = in_data[4];
        response[j++] = in_data[5];
        uint16_t crc_value = calcu_crc16(response, j);
        response[j++] = crc_value;
        response[j++] = crc_value >> 8;
    }

    return j;
}

/**
windy
 * @brief app modbus数据处理
 * 
 * @param data 数据
 * @param len 长度

 rx ->tx
 */
static void WIFI_MESH_Get_Rx_Data(void)//tbd 待基于调试情况看是否放入独立线程和1ms定时器，目的为获取udp数据包，不丢帧
{
    msg_struct income = {NULL, 0};//BLE rx 队列解析

    wireless_interface.len_rx_wifi_mesh =recv_wifi_mesh_data(wireless_interface.data_rx_wifi_mesh,MAX_RX_LEN_WIFI_MESH );

    if(wireless_interface.len_rx_wifi_mesh > 0)
    {
        ESP_LOGI(TAG, "wireless_interface.len_rx_wifi_mesh =%d",wireless_interface.len_rx_wifi_mesh);
        if(wireless_interface.len_rx_wifi_mesh > MAX_RX_LEN_WIFI_MESH) //提示非法长度，限制长度，防止溢出
        {
            ESP_LOGE(TAG, "over len:%d", wireless_interface.len_rx_wifi_mesh);
            wireless_interface.len_rx_wifi_mesh = MAX_RX_LEN_WIFI_MESH;
        }	
        income.msg_ptr = iot_calloc(wireless_interface.len_rx_wifi_mesh * sizeof(char));
        if (income.msg_ptr == NULL) 
        {
            ESP_LOGE(TAG, "malloc fail");
            return ;
        }
        memcpy((uint8_t *)income.msg_ptr, (uint8_t *)wireless_interface.data_rx_wifi_mesh, wireless_interface.len_rx_wifi_mesh);
        income.msg_len =wireless_interface.len_rx_wifi_mesh;

        ESP_LOGW(TAG, "after decrypt len:%d", income.msg_len);
    //        ESP_LOG_BUFFER_HEX_LEVEL(TAG, income.msg_ptr, income.msg_len, ESP_LOG_WARN);
        if (wifi_mesh_rx_queue && xQueueSend(wifi_mesh_rx_queue, &income, 0) != pdPASS) //pdMS_TO_TICKS(100)
        {
            free(income.msg_ptr);
        }
    }
}

/**
windy
 * @brief app modbus数据处理
 * 
 * @param data 数据
 * @param len 长度

 rx ->tx
 */
static void WIFI_MESH_Rx_process(void)
{
    msg_struct income = {NULL, 0};//  rx 队列解析

    if(wifi_mesh_rx_queue && xQueueReceive(wifi_mesh_rx_queue, &income, 0) == pdTRUE)
    {
        if(wifi_mesh_rx_data_handle(income.msg_ptr, income.msg_len) >= 0) 
        {
            goto end;
        }
    }

end:
    if (income.msg_ptr != NULL) 
    {
        free(income.msg_ptr);
        income.msg_ptr = NULL;
    }
}

/**
windy
 * @brief app modbus数据处理
 * 
 * @param data Rx数据
 * @param len 长度
 rx ->tx
 */
static int8_t wifi_mesh_rx_data_handle(uint8_t *data, int len)//app_modbus_data_handle
{
    int8_t rst=0;
    uint8_t md_addr=0;
    /* 解析收到的数据 */
    msg_general_t msg_gen;

    /* modbus协议检查 */
    //  if (md_protocol_check(data, len) != 0)
    //      return -1;

    dump_buf_global("wifi_mesh_rx_frame_parse EE", data, len);
    rst = wifi_mesh_rx_frame_parse(data, len);
    if (WIFI_MESH_GET_RTN_DISCOVERY == rst)
    {
        ESP_LOGE (TAG, "windy wifi mesh rx:  BB");
    }
    else if (WIFI_MESH_GET_RTN_APPLICATION == rst)
    {
        /*modbus响应*/
        Modbus_MasterRespones_Udp(data, len);
        ESP_LOGE(TAG, "wifi mesh rx modbus respones data");

        uint8_t *modbus_data =&data[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD];
        msg_general_t *msg_gen = (msg_general_t *)data;
        uint16_t temp_tx_len = 0;
        /** modbus启动xmodem升级指令后，除了要回复modbus响应，也要回复'C' */
        wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_ACK] = Xmodem_Client_top(OTA_CH_WIFI_MESH_TO_SELF, modbus_data, 0); /* xmodem运行 */   //msg_gen->md_len
        if (wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_ACK]) 
        {
            if (wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_ACK] != 0x06) //XMODEM_ACK
            {
                ESP_LOGW(TAG, "resp:0x%x", wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_ACK]);
            }
            wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_VER] =0xFE;
            wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_SEQ_L] =modbus_data[1];
            wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_SEQ_H] =0;//revd

            temp_tx_len = Udp_Singlecast_Modbus_MasterTxCmd(0, 0, wireless_interface.data_tx_wifi_mesh, msg_gen->src_devSN.dev_sn, MESH_FRAME_TYPE_XMODEM_RTN, wireless_interface.len_tx_wifi_mesh);
            /* 通过modbus发送xmodem客户端的响应数据 */
            send_wifi_mesh_data(msg_gen->src_devSN.sn,msg_gen->src_devSN.dev_type, wireless_interface.data_tx_wifi_mesh, temp_tx_len);  // 单播回复 rtn 非标4字节xmodem 
        }
    }
    else if (WIFI_MESH_GET_RTN_APPLICATION_XMODEM == rst)
    {
        ESP_LOGW(TAG, "windy wifi mesh rx:  XXXX");
        /*xmodem响应*/
        wifi_mesh_rx_data_Xmodem_Process(data, len);
        ESP_LOGE(TAG, "windy wifi mesh rx:  XY");
    }
    else if (WIFI_MESH_CYCLE_REPORT == rst)
    {
        uint8_t *data_buf = NULL;
        int buf_len = parse_mesh_data_report(data, len, &data_buf);
        if (0 >= buf_len)
        {
            if (NULL != data_buf)
            {
                free(data_buf);
                data_buf = NULL;
            }
            return rst;
        }

        uint64_t sn = *((uint64_t *)&data[WL_DEV_PRO_ADDR_SN_SRC]);
        uint16_t dev_type = *((uint16_t *)&data[WL_DEV_PRO_ADDR_TYPE_SRC]);
        int8_t slave_addr = get_slave_address(dev_type, sn);
        if (0 > slave_addr)
        {
            ESP_LOGE(TAG, "mesh report data:get slave address error");
        }
        if (NULL != data_buf)
        {
            modbus_slave_write_multi_regs(data_buf, buf_len, NULL, slave_addr);
            free(data_buf);
            data_buf = NULL;
        }
    }
    else//
    {
        ESP_LOGE(TAG, "windy wifi mesh rx:  DD,rst=%d",rst);
    }

    return rst;
}

static int wifi_mesh_rx_frame_parse(uint8_t *buff, int len)
{
    uint16_t tempdata=0;
    if(len < 23)
    {
        ESP_LOGE(TAG, "wifi_mesh_rx_frame_parse rx frame lenth short");
        return -1;
    }

    // dump_buf_global("wifi_mesh_rx_frame_parse DD", buff, len);
    /* 检测数据crc是否正确 */
    uint16_t crc16 = ModbusCrc16(buff, (len - 2));
    if (crc16 != ((buff[len - 1]<<8) | buff[len - 2])) 
    { // crc check
        ESP_LOGD(TAG, "wifi_mesh_rx_frame_parse full crc check failed");
        return -2; // modbus unknown pack
    }
    if(((MESH_FRAME_HEADER_WIFI_MESH_BROADCAST == buff[WIFI_UDP_FRAME_ADDR_HEAD])||(MESH_FRAME_HEADER_WIFI_MESH_SINGLE == buff[WIFI_UDP_FRAME_ADDR_HEAD]))//wif mesh无线设备发现协议
        &&(MESH_VERSION_TYPE_WIFI_MESH == buff[WIFI_UDP_FRAME_ADDR_VER]))
    {
        tempdata = (buff[WIFI_UDP_FRAME_ADDR_TYPE] & 0xF0);
        if(0x10 == tempdata)//报文类型 局域网设备发现帧
        {
            msg_discovery_t *msg_gen = (msg_discovery_t *)buff;
            
    //			memcpy(msg, msg_gen, sizeof(msg_discovery_t));//复制帧头内容
    //			msg->Ip_array_route = (uint8_t*)buff + offsetof(msg_discovery_t, Ip_array_route);	// 指向modbus数据区
            return WIFI_MESH_GET_RTN_DISCOVERY;
        }
        else if(0 == tempdata)//业务逻辑协议
        {
            msg_general_t *msg_gen = (msg_general_t *)buff;
            uint8_t *modbus_data =&buff[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD];
    //			dump_buf_global("wifi_mesh_rx_frame_parse B", buff, len);
            ESP_LOGI(TAG, "wifi_mesh_rx_frame_parse modbus  XX=msg_gen->src_devSN.sn= %lld,msg_gen->md_len=%d",msg_gen->src_devSN.sn,msg_gen->md_len );
    //			dump_buf_global("wifi_mesh_rx_frame_parse A", (uint8_t *)modbus_data, msg_gen->md_len);//67

            // if(len != (msg_gen->md_len + WIFI_UDP_FRAME_ADDR_MODBUS_HEAD +2))//长度错误
            // {
            //     ESP_LOGI(TAG, "wifi_mesh_rx_frame_parse modbus len err,len=%d, msg_gen->md_len=%d",len,msg_gen->md_len);
            //     return -3; //
            // }

            if((MESH_FRAME_TYPE_XMODEM == (buff[WIFI_UDP_FRAME_ADDR_TYPE]&0xF))//非标xmodem 
             ||(MESH_FRAME_TYPE_XMODEM_RTN == (buff[WIFI_UDP_FRAME_ADDR_TYPE]&0xF)))
            {
                ESP_LOGI(TAG, "wifi_mesh_rx_frame_parse xmodem data:%d", buff[WIFI_UDP_FRAME_ADDR_TYPE]);
                return WIFI_MESH_GET_RTN_APPLICATION_XMODEM; //
            }
            else if (MESH_FRAME_TYPE_PERIOD == (buff[WIFI_UDP_FRAME_ADDR_TYPE]&0xF))
            {
                ESP_LOGI(TAG, "wifi_mesh_rx_frame_parse cycle report data");
                return WIFI_MESH_CYCLE_REPORT;
            }
            else//modbus
            {
                crc16 = ModbusCrc16((uint8_t *)modbus_data, (msg_gen->md_len - 2));
                if (crc16 != ((modbus_data[msg_gen->md_len - 1]<<8) | (modbus_data[msg_gen->md_len - 2])))
                { //内部modbus crc check
                    ESP_LOGD(TAG, "wifi_mesh_rx_frame_parse modbus crc check failed,crc16=%d, %d,%d",crc16,modbus_data[msg_gen->md_len - 1],modbus_data[msg_gen->md_len - 2]);
                    return -4; // modbus unknown pack
                }
                else
                {
                    return WIFI_MESH_GET_RTN_APPLICATION; //
                }
            }


    //			memcpy(msg, msg_gen, sizeof(msg_general_t));//复制帧头内容
    //			msg->modbus_data = (uint8_t*)buff + offsetof(msg_general_t, modbus_data);	// 指向modbus数据区

        }
        else//非法报文
        {
            ESP_LOGD(TAG, "wifi_mesh_rx_frame_parse invalid frame");
        
            return -5; //
        }
    }

    return 0;
}

/**
*@brief 无线设备间协议：常规单播报文RX解析, ESP32S3做Modbus主，本函数是主解析modbus从的报文
*@param[in]     buff 接收buffer
*@param[out]    len 接收长度
*@return  0xFF: 不返回报文; 其他,返回的报文内容
*/
static uint8_t wifi_mesh_rx_data_Xmodem_Process( uint8_t *buff, int len)
{
    msg_general_t *msg_gen = (msg_general_t *)buff;
    uint8_t *modbus_data =&buff[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD];
    uint8_t temp_rx_len=0;
    uint8_t temp_tx_len=0;

    /*非标xmodem tx, 自己做 xmodem client*/
    if(MESH_FRAME_TYPE_XMODEM == (buff[WIFI_UDP_FRAME_ADDR_TYPE]&0xF))
    {
        ESP_LOGW(TAG, "-----------MESH_FRAME_TYPE_XMODEM-----------");
        // xmodem数据报文(长度为1029)和控制命令报文(EOT/ACK/NAK...)
        if(1029 ==msg_gen->md_len || 1 == msg_gen->md_len)
        {
            if(uart_ota_is_doing())
            {
                temp_rx_len =msg_gen->md_len;
                /*升级状态下接收，单独处理*/
                uart_ota_recv(modbus_data + 3, temp_rx_len);//非标xmodem tx 1029 rx
            }
            // uint16_t crc16 = xm_calcu_crc16(modbus_data +3, (msg_gen->md_len - 5)); // xmodem协议:校验数据
            // if (crc16 != ((modbus_data[msg_gen->md_len - 2] << 8) | modbus_data[msg_gen->md_len - 1])) {
            //     ESP_LOGE(TAG, "xmodem pack seq %d crc16 error", modbus_data[1]);
            //     return XMODEM_NAK;
            // }
            wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_ACK] = Xmodem_Client_top(OTA_CH_WIFI_MESH_TO_SELF, modbus_data, msg_gen->md_len); /* xmodem运行 */
            if (wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_ACK]) 
            {
                if (wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_ACK] != 0x06) //XMODEM_ACK
                {
                    ESP_LOGW(TAG, "resp:0x%x", wireless_interface.data_tx_wifi_mesh[0]);
                }
                wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_VER] =0xFE;
                wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_SEQ_L] =modbus_data[1];
                wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + XMODEM_FRAME_INDEX_SEQ_H] =0;//revd

                temp_tx_len = Udp_Singlecast_Modbus_MasterTxCmd(0, 0, wireless_interface.data_tx_wifi_mesh, msg_gen->src_devSN.dev_sn, MESH_FRAME_TYPE_XMODEM_RTN, wireless_interface.len_tx_wifi_mesh);
                /* 发送xmodem客户端的响应数据 */
                send_wifi_mesh_data(msg_gen->src_devSN.sn,msg_gen->src_devSN.dev_type, wireless_interface.data_tx_wifi_mesh, temp_tx_len);  // 单播回复 rtn 非标4字节xmodem 
            }
        }
        else
        {
            ESP_LOGE(TAG, "xmodem pack len error;=%d", msg_gen->md_len);
            return XMODEM_NAK;
        }
    }
    /*非标xmodem rx , 自己做 xmodem server*/
    if(MESH_FRAME_TYPE_XMODEM_RTN == (buff[WIFI_UDP_FRAME_ADDR_TYPE]&0xF))
    {
        /*
        非标准xomdem RTN:4字节
        0：版本号：0xFE
        1：响应包序号L_8bit
        2：响应包序号H_8bit
        3：C/ACK/NAK
        */
        ESP_LOGW(TAG, "-----------MESH_FRAME_TYPE_XMODEM_RTN-----------");
        if((4 == msg_gen->md_len)&&(0xFE == modbus_data[0]))
        {
            temp_rx_len = msg_gen->md_len -3;
            /*升级状态下接收，单独处理*/
            ESP_LOGW(TAG, "-----------xmodem rtn:0x%x, len:%d", modbus_data[3], temp_rx_len);
            uart_ota_recv(&modbus_data[3], temp_rx_len);//非标xmodem rx len+3
            return 0xFF;
        }
        else
        {
            ESP_LOGE(TAG, "xmodem pack len error;=%d,modbus_data[0]=0x%X", msg_gen->md_len,modbus_data[0]);
            return XMODEM_NAK;
        }
    }

    return 0xFF;
}

/*
add:
wifi mesh设备增删回调函数
更新本地modbus 设备结构体变量
add:添加到前面非NULL的数组，之后的清除
index:g_mesh_net_tree.node[index]的数组元素序号
*/
// static void WIFI_MESH_Rx_Add_Node(int16_t index, uint8_t type, uint64_t dev_sn, uint16_t dev_type, uint16_t priority, uint8_t parallel_seq)
// {
//     uint8_t i=0;
//     uint8_t j=0;

//     if((g_master_info.priority < priority)
//     || (g_master_info.priority == priority
//         && g_master_info.parallel_seq < parallel_seq))
//     {
//         g_master_info.dev_sn.sn = dev_sn;
//         g_master_info.dev_sn.dev_type = dev_type;
//         g_master_info.priority = priority;
//         g_master_info.parallel_seq = parallel_seq;
//     }

//     if(SN_TYPE_D400S == dev_type)
//     {
//         for (i = 0; i < (DCDC_MAX_NUM ); i++)
//         {
//             if(Dcdc_modbus[i].mod_reg01100_Inv_base.InvSN == 0)
//             {
//                 Dcdc_modbus[i].mod_reg01100_Inv_base.InvSN =dev_sn;

//                 wifi_mesh_set_node_modbus_slave_address(index ,i);

//                 for (j = i+1; j < (DCDC_MAX_NUM ); j++)
//                 {
//                     if(Dcdc_modbus[j].mod_reg01100_Inv_base.InvSN == dev_sn)//如果后面有重复，需要删除
//                     {
//                         Dcdc_modbus[j].mod_reg01100_Inv_base.InvSN =0;//clean
//                     }
//                 }
//             }

//         }
//     }
//     else if(SN_TYPE_S1 == dev_type)
//     {
//         for (i = 0; i < (PLUG_MAX_NUM ); i++)
//         {
//             if(Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN == 0)
//             {
//                 Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN =dev_sn;
//                 wifi_mesh_set_node_modbus_slave_address(index ,i);
//                 for (j = i+1; j < (PLUG_MAX_NUM); j++)
//                 {
//                     if(Plug[j].mod_reg14500_SmartPlug_info.SmartPlug_SN == dev_sn)//如果后面有重复，需要删除
//                     {
//                         Plug[j].mod_reg14500_SmartPlug_info.SmartPlug_SN =0;//clean
//                     }
//                 }
//             }
//         }
//     }
// }

/*
delete:
wifi mesh设备增删回调函数
更新本地modbus 设备结构体变量
delete:clean
*/
// static void WIFI_MESH_Rx_Delete_Node(int16_t index, uint8_t type, uint64_t dev_sn, uint16_t dev_type, uint16_t priority, uint8_t parallel_seq)
// {
//     uint8_t i=0;
//     if(SN_TYPE_D400S == dev_type)
//     {
//         for (i = 0; i < (DCDC_MAX_NUM ); i++)// 
//         {
//             if(Dcdc_modbus[i].mod_reg01100_Inv_base.InvSN == dev_sn)//same
//             {
//                 Dcdc_modbus[i].mod_reg01100_Inv_base.InvSN =0;//clean
//             }
//         }
//     }
//     else if(SN_TYPE_S1 == dev_type)
//     {
//         for (i = 0; i < (PLUG_MAX_NUM ); i++)// 
//         {
//             if(Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN == dev_sn)//same
//             {
//                 Plug[i].mod_reg14500_SmartPlug_info.SmartPlug_SN =0;//clean
//             }
//         }
//     }
// }

void wifi_mesh_dev_cnt_timeout_check(void)
{
    uint8_t i=0;
    uint8_t tempdata1=0;
    uint8_t tempdata2=0;
    static uint8_t scnt=0;
    mesh_node_sn_t temp_devSN;

    if(++scnt >= 60)
    {
        scnt =0;
        for (i = 0; i < MESH_NODE_MAX_COUNT; i++)
        {
            memcpy((uint8_t *)temp_devSN.dev_sn , wifi_mesh_get_node_SN(i), 10);
            
            if(SN_TYPE_S1 == temp_devSN.dev_type)
            {
                ESP_LOGI(TAG, "wifi_mesh_dev_cnt_timeout_check AAA   online = temp_devSN.dev_type:%d", temp_devSN.dev_type);
            
                tempdata1++;
            }
            else if(SN_TYPE_SELF == temp_devSN.dev_type)
            {
                tempdata2++;
            }
        }

        reals.Topnet_point_Num_S1 = tempdata1;
        reals.Topnet_point_Num_invbat = tempdata2;
    }
}

/*

uint8_t type= MESH_NODE_STATUS_ONLINE,MESH_NODE_STATUS_OFFLINE 
uint64_t dev_sn, 
uint16_t dev_type

index:g_mesh_net_tree.node[index]的数组元素序号

*/
// void iot_mesh_node_change_cb(int16_t index, uint8_t type, uint64_t dev_sn, uint16_t dev_type, uint16_t priority, uint8_t parallel_seq)
// {
//     if(MESH_NODE_STATUS_ONLINE == type)
//     {
//         WIFI_MESH_Rx_Add_Node( index,  type,  dev_sn,  dev_type,  priority,  parallel_seq);
//     }
//     else if(MESH_NODE_STATUS_OFFLINE == type)
//     {
//         WIFI_MESH_Rx_Delete_Node( index,  type,  dev_sn,  dev_type,  priority,  parallel_seq);
//     }

//     ESP_LOGI(TAG, "iot mesh node change cb, type:%d, sn:%llu, dev_type:%d", type, dev_sn, dev_type);
// }
