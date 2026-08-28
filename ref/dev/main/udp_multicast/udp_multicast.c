#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <stdio.h>
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "cmd_udp_back.h"
#include "udp_multicast.h"
#include "iot_period_task.h"
#include "dev_discovery.h"
#include "comm_define.h"
#include "modbus_slave.h"
#include "modbus_slave_data.h"
#include "filesystem.h"
#include "can_protocol.h"
#include "iot_mqtt.h"
#include "wl_mesh.h"
#include "uart_device_process.h"
#include "mesh_api.h"
#include "uart_ota.h"
#include "modbus_data.h"
#include "iot_period_task.h"

#define MULTICAST_TTL 1

//#define MULTICAST_IPV6_ADDR "FF02::FC"

//#define LISTEN_ALL_IF   EXAMPLE_MULTICAST_LISTEN_ALL_IF

static const char *TAG = "[udp_multicast]";
//#ifdef CONFIG_EXAMPLE_IPV4
static const char *V4TAG = "[mcast-ipv4]";
static const char *VmodbusTAG = "[mcast-ipv4:modbus]";
UDP_MODBUS_STRUCT g_udp_modbus;//udp_modbus 发送临时辅助变量

//#endif

#define PROMPT_STR CONFIG_IDF_TARGET

#define MAX(a, b) ((a) > (b) ? (a) : (b))
// EXT_RAM_BSS_ATTR RealS_STRUCT reals;

int Udp_singlecast_sock_fd = -1;
int Udp_multicast_sock_fd = -1;

uint16_t Udp_singlecast_step = 0;
uint16_t Udp_Multicast_step = 0;

static SemaphoreHandle_t udp_tx_semaphore; 
static QueueHandle_t xQueue_udp_fast_tx = NULL;
EXT_RAM_BSS_ATTR static uint8_t Udp_TxBuffer[512];



EXT_RAM_BSS_ATTR udp_config_struct udp_config;
//    udp_config.current_netif_id = NETIF_TYPE_MIN,


/*------------------------------------------------------------------------------
 Function: udp_fast_tx_queue_init
 -----------------------------------------------------------------------------*/
/**
  * @brief      队列初始化
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
uint8_t udp_fast_tx_queue_init(void)
{
    uint8_t ret = 0;
    
    /*队列不存在*/
    if (xQueue_udp_fast_tx == NULL)
	{
		xQueue_udp_fast_tx = xQueueCreate(2, sizeof(ble_to_dev_struct));
		if (xQueue_udp_fast_tx == NULL)
		{
		    /*队列初始化失败*/
			ESP_LOGE(TAG, "udp_fast_tx_queue_init create failed");
            return ret;
		}
        
        /*队列正常*/
        ret = 1;
	}
    else
    {
        /*队列正常*/
        ret = 1;
    }

    return ret;
}

/*------------------------------------------------------------------------------
 Function: udp_fast_tx_Push
 -----------------------------------------------------------------------------*/
/**
  * @brief      紧急发送存储到队列缓存
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void udp_fast_tx_Push(uint16_t regaddress, uint16_t regcnt, uint16_t slaveindex)
{
	ble_to_dev_struct *queue_msg = NULL;

    if(!udp_fast_tx_queue_init()) return;
    
    queue_msg = (ble_to_dev_struct *)heap_caps_malloc(sizeof(ble_to_dev_struct), MALLOC_CAP_SPIRAM);

	if (!queue_msg) 
	{
		ESP_LOGE(TAG, "uart_Sub1G_fast_tx_Push calloc failed");
	} 
	else 
	{
		queue_msg->regaddress = regaddress;
        queue_msg->regcnt = regcnt;
        queue_msg->slaveindex = slaveindex;
        
		/*消息保存到队列*/
		if (xQueueSendToBack((QueueHandle_t)xQueue_udp_fast_tx, &queue_msg, 0) != pdPASS) 
		{
			free(queue_msg);
			queue_msg = NULL;
		}
        else
        {
            reals.flag_array1.bits.udp_fast_tx = 1;
        }
	}
}

/**
 * 判断数组的所有元素是否均为零
 *
 * @param arr 数组指针
 * @param length 数组长度
 * @return 如果所有元素均为零，返回 true；否则返回 false
 */
bool is_array_all_zero(const uint8_t *arr, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (arr[i] != 0) {
            return false;
        }
    }
    return true;
}

/*------------------------------------------------------------------------------
 Function: check_socket_type
 -----------------------------------------------------------------------------*/
/**
  * @brief      检查句柄类型
  * @param[in]  int sockfd  
  * @param[out] None
  * @return     static void
  */
static void check_socket_type(int sockfd)
{
    int type;
    socklen_t len = sizeof(type);

    if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &type, &len) == -1) {
        ESP_LOGE(V4TAG, "getsockopt failed");
        return;
    }

    if (type == SOCK_STREAM) {
        ESP_LOGI(V4TAG, "Socket is TCP");
    } else if (type == SOCK_DGRAM) {
        ESP_LOGI(V4TAG, "Socket is UDP");
    } else {
        ESP_LOGI(V4TAG, "Unknown socket type");
    }
}

/*------------------------------------------------------------------------------
 Function: udp_socket_clean
 -----------------------------------------------------------------------------*/
/**
  * @brief      关闭UDP socket
  * @param[in]  int sockfd  
  * @param[out] None
  * @return     static void
  */
static void udp_socket_clean(int *sockfd)
{
    /* socket fd 0、1、2分别表示标准输入、标准输出、标准错误,不能被关闭 */
    if ((*sockfd != 0xff) && (*sockfd >= 3)) {
        check_socket_type(*sockfd);
        // 先调用 shutdown 再调用 close
        if (shutdown(*sockfd, SHUT_RDWR) == -1) {
            ESP_LOGE(TAG, "Failed to shutdown socket, ret: %d, errno:%d, mean:%s", *sockfd, errno, strerror(errno));
        }
        ESP_LOGW(V4TAG, "close Udp_multicast_sock_fd: %d", *sockfd);
        if (close(*sockfd) == -1) {
            ESP_LOGE(V4TAG, "Failed to close socket, ret: %d, errno:%d, mean:%s", *sockfd, errno, strerror(errno));
            if ( errno == EBADF )
            {
                /*Bad file number*/
                *sockfd = -1;
            }
        }
        else
        {
            *sockfd = -1;
        }
    }
}


/*
配置网卡名称
*/
void set_udp_client_netif(uint8_t netif_type)
{
    char *netif_key[3] = { NETIF_KEY_WIFI_STA,NETIF_KEY_WIFI_AP};//NETIF_KEY_ETH,
#if !CONFIG_LWIP_NETIF_API
    esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[netif_type]),
                                    udp_config.udp_netif_req.ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[netif_type])),
                                udp_config.udp_netif_req.ifr_name);
#endif

    ESP_LOGW(V4TAG, "set_udp_client_netif:%s", udp_config.udp_netif_req.ifr_name);
}

/*
网卡选择
*/
void iot_udp_start(uint8_t mode) 
{
    if (( mode != 0 )&&( mode != 1 ))
    {
        return;//非法输入
    }
    
    ESP_LOGI(V4TAG, "iot_udp_start : %u, current_netif_id:%u", mode, udp_config.current_netif_id);
    udp_config.states[mode] = 1;
    
    //TODO: modify by debug: mode > udp_config.current_netif_id
    // 优先级更低的网卡上线，不切换udp组网方式
    if(mode >= udp_config.current_netif_id) {
        return;
    }

    if ( udp_config.states[udp_config.current_netif_id] == 2 ) {
        udp_config.states[udp_config.current_netif_id] = 1;
    }
    
    udp_config.current_netif_id = mode;
    set_udp_client_netif(udp_config.current_netif_id);
    udp_config.states[udp_config.current_netif_id] = 2;
    
    /* UDP处理任务 */
    Top_Net_Point_Clean();
//    reals.Step_dev_discovery = MESH_FRAME_HEADER_COMMON;
//    Udp_singlecast_step = UDP_STEP_RESTART;
//    Udp_Multicast_step = UDP_STEP_RESTART;
//    memset(udp_config.source_ip_str, 0, sizeof(udp_config.source_ip_str));
}

void iot_udp_delete(uint8_t mode) 
{
return;//windy debug 
    if (( mode != 0 )&&( mode != 1 ))
    {
        return;//非法输入
    }

	ESP_LOGI(V4TAG, "iot_udp_delete : %u, current_netif_id:%u", mode, udp_config.current_netif_id);
    udp_config.states[mode] = 0;

    if (mode == udp_config.current_netif_id)
    {
        /* UDP处理任务 */
        Top_Net_Point_Clean();
//        reals.Step_dev_discovery = MESH_FRAME_HEADER_COMMON;
//        Udp_singlecast_step = UDP_STEP_RESTART;
//        Udp_Multicast_step = UDP_STEP_RESTART;
        memset(udp_config.source_ip_str, 0, sizeof(udp_config.source_ip_str));
    }
}

/* Add a socket, either IPV4-only or IPV6 dual mode, to the IPV4
   multicast group */
int socket_add_ipv4_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int err = 0;
    // Configure source interface
// #if LISTEN_ALL_IF
//     imreq.imr_interface.s_addr = IPADDR_ANY;
// #else
//     esp_netif_ip_info_t ip_info = { 0 };
//     err = esp_netif_get_ip_info(get_example_netif(), &ip_info);
//     if (err != ESP_OK) {
//         ESP_LOGE(V4TAG, "Failed to get IP address info. Error 0x%x", err);
//         goto err;
//     }
//     inet_addr_from_ip4addr(&iaddr, &ip_info.ip);
// #endif // LISTEN_ALL_IF
    // Configure multicast address to listen to
    err = inet_aton(MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(V4TAG, "Configured IPV4 multicast address '%s' is invalid.", MULTICAST_IPV4_ADDR);
        // Errors in the return value have to be negative
        err = -1;
        goto err;
    }
    ESP_LOGI(TAG, "Configured IPV4 Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(V4TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", MULTICAST_IPV4_ADDR);
    }

    if (assign_source_if) {
        // Assign the IPv4 multicast source interface, via its IP
        // (only necessary if this socket is IPV4 only)
        err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
                         sizeof(struct in_addr));
        if (err < 0) {
            ESP_LOGE(V4TAG, "Failed to set IP_MULTICAST_IF. Error %d, %s", errno, strerror(errno));
            goto err;
        }
    }

    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d, %s", errno, strerror(errno));
        goto err;
    }

 err:
    return err;
}

/**
 * 创建IPv4组播套接字。
 * 
 * @return 返回创建的套接字的文件描述符。
 */
int create_multicast_ipv4_socket(void)
{
    struct sockaddr_in saddr = { 0 };
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);//PF_INET=IPV4；SOCK_DGRAM=UDP
    if (sock < 0) {
        ESP_LOGE(V4TAG, "Udp_multicast_init : Failed to create socket. Error %d, %s", errno, strerror(errno));
        return -1;
    }

    // 绑定套接字到特定的网卡
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (void *)&udp_config.udp_netif_req, sizeof(udp_config.udp_netif_req)) < 0) {
        ESP_LOGE(V4TAG, "Udp_multicast_init : Failed to bind socket to device. Error %d, %s", errno, strerror(errno));
        goto err;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(UDP_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Udp_multicast_init : Failed to bind socket. Error %d, %s", errno, strerror(errno));
        goto err;
    }

    // Assign multicast TTL (set separately from normal interface TTL)
    uint8_t ttl = MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Udp_multicast_init : Failed to set IP_MULTICAST_TTL. Error %d, %s", errno, strerror(errno));
        goto err;
    }

    // this is also a listening socket, so add it to the multicast
    // group for listening...
    err = socket_add_ipv4_multicast_group(sock, true);
    if (err < 0) {
        goto err;
    }

    // All set, socket is configured for sending and receiving
    ESP_LOGI(V4TAG,"Udp_multicast_init : sockfd is %d",sock);
    return sock;

err:
    close(sock);
    return -1;
}

/**
 * 绑定 IPv4 地址到套接字
 *
 * @param source_ip_str 源 IP 地址字符串
 * @return 成功绑定返回 0，否则返回 -1
 */
int socket_bind_ipv4_address(const char* source_ip_str)
{
    struct sockaddr_in addr = { 0 };
    int err = 0;
    int sock = -1;
    
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);//PF_INET=IPV4；SOCK_DGRAM=UDP
    if (sock < 0) {
        ESP_LOGE(V4TAG, "Udp_singlecast_init : Failed to create socket. Error %d, %s", errno, strerror(errno));
        return -1;
    }

    // 绑定套接字到特定的网卡
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (void *)&udp_config.udp_netif_req, sizeof(udp_config.udp_netif_req)) < 0) {
        ESP_LOGE(V4TAG, "Udp_multicast_init : Failed to bind socket to device. Error %d, %s", errno, strerror(errno));
        goto err;
    }

    // Configure the address
    addr.sin_family = PF_INET;
    addr.sin_port = htons(UDP_PORT_SINGLE);
    err = inet_pton(PF_INET, source_ip_str, &(addr.sin_addr));
//    err = inet_pton(PF_INET, "192.168.0.128", &(addr.sin_addr));
//    err = inet_pton(PF_INET, "192.168.0.206", &(addr.sin_addr));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Udp_singlecast_init : Failed to convert IP address '%s'. Error %d, %s", source_ip_str, errno, strerror(errno));
        goto err;
    }
    ESP_LOGW(V4TAG, "Udp_singlecast_init : Configured to convert IP address '%s'. ", source_ip_str);
    
    // Bind the socket to the address
    err = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Udp_singlecast_init : Failed to bind the socket. Error %d, %s", errno, strerror(errno));
        goto err;
    }
    
    // All set, socket is configured for sending and receiving
    ESP_LOGI(V4TAG,"Udp_singlecast_init : sockfd is %d",sock);
    return sock;

    err:
    close(sock);
    return -1;
}


//int udp_back_send(void)//(int argc, char **argv)
//{

//    /*udp send begin*/
//    int sockfd = -1;
//	sockfd =Udp_multicast_sock_fd;
//    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
//    if (sockfd < 0) {
//        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
//        return -1;
//    }

//    struct sockaddr_in dest_addr;
//    memset(&dest_addr, 0, sizeof(dest_addr));
//    dest_addr.sin_family = AF_INET;
//    dest_addr.sin_port = htons(UDP_PORT);
//    inet_pton(AF_INET, MULTICAST_IPV4_ADDR, &dest_addr.sin_addr); // 设置接收方的IP地址 "192.168.0.217"

//    char *message = "Hello, this is a UDP message!";
//    int bytes_sent = sendto(sockfd, message, strlen(message), 0,
//                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));//UDP send
//    if (bytes_sent < 0) {
//        perror("Error in sending message");
//        exit(EXIT_FAILURE);
//    }

//    close(sockfd);

//    /*udp send end*/
//    return 0;
//}




/**
 * @brief Initializes the UDP singlecast functionality.
 *
 * This function initializes the UDP singlecast functionality by setting up the necessary configurations and parameters.
 *
 * @param source_ip_str The source IP address as a string.
 */
void Udp_singlecast_init(void)
{
//	int sock;
	int s;
	int len;

    /*暂停工作*/
    if ( UDP_STEP_NONE == Udp_singlecast_step )
    {
        udp_socket_clean(&Udp_singlecast_sock_fd);
    }
    /*准备初始化*/
    else if ( UDP_STEP_RESTART == Udp_singlecast_step )
    {        
        /*组播已停止，同步停止*/
        if ( UDP_STEP_NONE == Udp_Multicast_step )
        {
            Udp_singlecast_step = UDP_STEP_NONE;
        }
        /*组播正常，准备初始化*/
        else if ( UDP_STEP_INIT_READY == Udp_Multicast_step )
        {
            Udp_singlecast_step = UDP_STEP_INIT;
        }
    }

	if(UDP_STEP_INIT == Udp_singlecast_step)//init
	{
        udp_socket_clean(&Udp_singlecast_sock_fd);

		Udp_singlecast_sock_fd = socket_bind_ipv4_address(udp_config.source_ip_str);
		if (Udp_singlecast_sock_fd < 0) 
		{
			ESP_LOGE(V4TAG, "Failed to create IPv4 singlecast socket");

			return;//continue;
		}

         /*检查套接字格式*/
         int type;
         socklen_t len = sizeof(type);
        if (getsockopt(Udp_singlecast_sock_fd, SOL_SOCKET, SO_TYPE, &type, &len) == -1) {
            ESP_LOGE(TAG, "getsockopt failed");
            udp_socket_clean(&Udp_singlecast_sock_fd);
        
            return;
        }
        else if (type != SOCK_DGRAM) 
        {
            ESP_LOGE(TAG, "Socket type error(%d)", type);
            udp_socket_clean(&Udp_singlecast_sock_fd);
            
            return;
        } 

		Udp_singlecast_step = UDP_STEP_INIT_READY;

	}
}


/*
局域网UDP组播
*/
void Udp_multicast_init(void)
{
//	int sock;
	int s;
	int len;

    /*暂停工作*/
    if ( UDP_STEP_NONE == Udp_Multicast_step )
    {
        udp_socket_clean(&Udp_multicast_sock_fd);
    }
    /*准备初始化*/
    else if ( UDP_STEP_RESTART == Udp_Multicast_step )
    {        
//        for ( int i = 0 ; i < NETIF_TYPE_MAX ; i++ )
//        {
//            /*存在配置成功网络*/
//            if ( udp_config.states[i] == 2 )
//            {
//                goto multicast_sock_init;
//            }
//            /*存在可配置网络*/
//            else if ( udp_config.states[i] == 1 )
//            {
//                udp_config.current_netif_id = i;
//                set_udp_client_netif(udp_config.current_netif_id);
//                udp_config.states[udp_config.current_netif_id] = 2;
//                goto multicast_sock_init;
//            }
//        }
		goto multicast_sock_init;//windy debug

        /*不存在可用网络，回到上一状态*/
        Udp_Multicast_step = UDP_STEP_NONE;
        return;

       multicast_sock_init:
        
        if ( udp_config.current_netif_id == NETIF_TYPE_ETH )
        {
            if (!is_array_all_zero(g_self_data.mod_reg11000_IOT_info.IP_ETH, 4))
            {
                sprintf(udp_config.source_ip_str, "%d.%d.%d.%d", 
                    g_self_data.mod_reg11000_IOT_info.IP_ETH[0], 
                    g_self_data.mod_reg11000_IOT_info.IP_ETH[1], 
                    g_self_data.mod_reg11000_IOT_info.IP_ETH[2], 
                    g_self_data.mod_reg11000_IOT_info.IP_ETH[3]);
                Udp_Multicast_step = UDP_STEP_INIT;
            }
        }
        else if((NETIF_TYPE_WIFI_STA == udp_config.current_netif_id)
			||(NETIF_TYPE_WIFI_AP == udp_config.current_netif_id))
        {
//            if (!is_array_all_zero(g_self_data.mod_reg11000_IOT_info.sta_ipv4, 4))
//            {
//                sprintf(udp_config.source_ip_str, "%d.%d.%d.%d", 
//                    g_self_data.mod_reg11000_IOT_info.sta_ipv4[0], 
//                    g_self_data.mod_reg11000_IOT_info.sta_ipv4[1], 
//                    g_self_data.mod_reg11000_IOT_info.sta_ipv4[2], 
//                    g_self_data.mod_reg11000_IOT_info.sta_ipv4[3]);
//                Udp_Multicast_step = UDP_STEP_INIT;
//            }

			  if (0 != strlen(udp_config.source_ip_str))
			  {
				  ESP_LOGE(TAG, "udp_config.source_ip_str =%s",udp_config.source_ip_str);
				  
				  Udp_Multicast_step = UDP_STEP_INIT;
			  }

        }
    }
    
	if(UDP_STEP_INIT == Udp_Multicast_step)//init
	{
        udp_socket_clean(&Udp_multicast_sock_fd);

		Udp_multicast_sock_fd = create_multicast_ipv4_socket();
		if (Udp_multicast_sock_fd < 0) 
		{
			ESP_LOGE(V4TAG, "Failed to create IPv4 multicast socket");
		
			return;//continue;
		}

         /*检查套接字格式*/
         int type;
         socklen_t len = sizeof(type);
        if (getsockopt(Udp_multicast_sock_fd, SOL_SOCKET, SO_TYPE, &type, &len) == -1) {
            ESP_LOGE(TAG, "getsockopt failed");
            udp_socket_clean(&Udp_multicast_sock_fd);
            
            return;
        }
        else if (type != SOCK_DGRAM) 
        {
            ESP_LOGE(TAG, "Socket type error(%d)", type);
            udp_socket_clean(&Udp_multicast_sock_fd);

            return;
        } 

		Udp_Multicast_step =UDP_STEP_INIT_READY;

	}
}


/*------------------------------------------------------------------------------
 Function: Udp_singlecast_Tx
 -----------------------------------------------------------------------------*/
/**
  * @brief      单播发送
  * @param[in]  uint8_t *sta_ipv4  
                uint16_t port      
                uint8_t *buff      
                uint16_t Len       
  * @param[out] None
  * @return     int
  */
int Udp_singlecast_Tx(uint8_t *target_sta_ipv4, uint8_t *target_portaddr, uint8_t *buff, uint16_t Len)//
{
    struct sockaddr_in dest_addr;
	char target_ip_str[32];
    uint16_t target_port = (uint16_t)(target_portaddr[0] | (target_portaddr[1] << 8));
    sprintf(target_ip_str, "%d.%d.%d.%d", target_sta_ipv4[3], target_sta_ipv4[2], target_sta_ipv4[1], target_sta_ipv4[0]);
    
    /*udp send begin*/
    int sockfd = -1;
    int err = 0;
	
	if(UDP_STEP_INIT_READY == Udp_singlecast_step)
	{
		sockfd = Udp_singlecast_sock_fd;
		if (sockfd < 0) 
		{
			ESP_LOGE(TAG, "[Udp_singlecast_Tx] Failed to create socket. Error %d, %s", errno, strerror(errno));
			return -1;
		}
	
		memset(&dest_addr, 0, sizeof(dest_addr));
		dest_addr.sin_family = PF_INET;
		dest_addr.sin_port = htons(target_port);
		err = inet_pton(PF_INET, target_ip_str, &dest_addr.sin_addr); // 设置接收方的IP地址 "192.168.0.217"
		int bytes_sent = sendto(sockfd, buff, Len, 0,
								(struct sockaddr *)&dest_addr, sizeof(dest_addr));//UDP send
								
		if (bytes_sent < 0) 
		{
		    ESP_LOGE(TAG, "[Udp_singlecast_Tx]  send to %s : %d",target_ip_str, target_port);
			ESP_LOGE(TAG, "[Udp_singlecast_Tx] Error in sending message(socket = %d), Error %d, %s", sockfd, errno, strerror(errno));
            if ( errno == EBADF )
            {
                /*Bad file number*/
                Udp_singlecast_step = UDP_STEP_INIT;
            }
            return -1;
//			exit(EXIT_FAILURE);
		}
        ESP_LOGI(TAG, "[Udp_singlecast_Tx] %s send to %s : %d", udp_config.source_ip_str, target_ip_str, target_port);
//        Udp_singlecast_step = UDP_STEP_INIT;
//        close(sockfd);
	}

    /*udp send end*/
    return 0;
}


/*------------------------------------------------------------------------
*@Function :Udp_Multicast_Tx 

UDP组播发送

*@return		 
0- ok
no 0: fail
*/
int Udp_Multicast_Tx(void)//
{
    struct sockaddr_in dest_addr;
	uint8_t *buff = NULL;
	uint16_t Len=0;
	
    /*udp send begin*/
    int sockfd = -1;
	
	if(UDP_STEP_INIT_READY == Udp_Multicast_step)
	{
		sockfd = Udp_multicast_sock_fd;
		if (sockfd < 0) 
		{
			ESP_LOGE(TAG, "Failed to create socket. Error %d, %s", errno, strerror(errno));
			return -1;
		}
	
		memset(&dest_addr, 0, sizeof(dest_addr));
		dest_addr.sin_family = PF_INET;
		dest_addr.sin_port = htons(UDP_PORT);
		inet_pton(PF_INET, MULTICAST_IPV4_ADDR, &dest_addr.sin_addr); // 设置组播IP地址 "239.0.0.238"
	
		buff = (uint8_t *)heap_caps_malloc(sizeof(uint8_t) * 512, MALLOC_CAP_SPIRAM);
        if ( buff == NULL ) {
            ESP_LOGE(TAG, "Failed to heap_caps_malloc");
            return -1;
        }
		Device_Discovery_Step(buff, &Len, udp_config.current_netif_id);
        if(Len == 0) {
            free(buff);
            return -1;
        }
		int bytes_sent = sendto(sockfd, buff, Len, 0,
								(struct sockaddr *)&dest_addr, sizeof(dest_addr));//UDP send
		dump_buf_global("Udp TX data", buff, Len);
								
		free(buff);
        
		if (bytes_sent < 0) 
		{
			ESP_LOGE(TAG, "[Udp_Multicast_Tx] Error in sending message(socket = %d), Error %d, %s", sockfd, errno, strerror(errno));
            if ( errno == EBADF )
            {
                /*Bad file number*/
                Udp_Multicast_step = UDP_STEP_INIT;
            }
            return -1;

//			exit(EXIT_FAILURE);
		}
        ESP_LOGI(TAG, "[Udp_Multicast_Tx] %s send to %s : %d", udp_config.source_ip_str, MULTICAST_IPV4_ADDR, UDP_PORT);
	}

    /*udp send end*/
    return 0;
}


uint8_t *UdpRxdata=NULL;//BLE RX接收缓存指针
uint16_t UdpRxLen =0;////BLE RX接收缓存长度

#define UDP_RX_SIZE 300// 


#if 0
int16_t Udp_singlecast_Rx(void)
{
	int s;
	int len =0;
	fd_set rfds;
	struct sockaddr_storage raddr; // Large enough for both IPv4 or IPv6
	socklen_t socklen = sizeof(raddr);
 
	struct timeval tv = {
		.tv_sec = 0,//windy changed ,not block,2,
		.tv_usec = 0,
	};
	
	if(UDP_STEP_INIT_READY == Udp_singlecast_step)
	{
        FD_ZERO(&rfds);
        FD_SET(Udp_singlecast_sock_fd, &rfds);

		s = select(Udp_singlecast_sock_fd + 1, &rfds, NULL, NULL, &tv);//阻塞等待
		// ESP_LOGW(TAG,"select_s: %d",s);
		if (s > 0) //ok
		{
			if (FD_ISSET(Udp_singlecast_sock_fd, &rfds)) 
			{
			
//				char recvbuf[48];
				// Incoming datagram received
				char raddr_name[32] = { 0 };
                uint16_t raddr_port = 0;

				if(NULL == UdpRxdata)
				{
					UdpRxdata = heap_caps_malloc(UDP_RX_SIZE, MALLOC_CAP_SPIRAM);
				}

//				len = recvfrom(Udp_multicast_sock_fd, recvbuf, sizeof(recvbuf)-1, 0,
//								   (struct sockaddr *)&raddr, &socklen);//(char*)

			   len = recvfrom(Udp_singlecast_sock_fd, UdpRxdata, UDP_RX_SIZE-1, 0,
								  (struct sockaddr *)&raddr, &socklen);//(char*)

				// Get the sender's address as a string
				if((len > 0) 
					&&(raddr.ss_family == PF_INET) )
				{
					inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr, raddr_name, sizeof(raddr_name)-1);//网络地址转换
                    raddr_port = ntohs(((struct sockaddr_in *)&raddr)->sin_port);
                    
					UdpRxLen = len;

                    ESP_LOGW(TAG, "[Udp_singlecast_Rx] received %d bytes from %s, %d", len, raddr_name, raddr_port);
                    
					Device_Discovery_Get_Frame(UdpRxdata, UdpRxLen, raddr_name, raddr_port);
					
				}
				
//				ESP_LOGI(TAG, "%s", (char*)UdpRxdata);
//				ESP_LOGI(TAG, "%s", (char*)recvbuf);
				
			}
		}

	}

	if(NULL != UdpRxdata)
	{
	
        free(UdpRxdata);
		UdpRxdata = NULL;
	}		
    
    return (int16_t)len;
}

int16_t Udp_multicast_Rx(void)
{
	int s;
	int len =0;
	fd_set rfds;
	struct sockaddr_storage raddr; // Large enough for both IPv4 or IPv6
	socklen_t socklen = sizeof(raddr);

	struct timeval tv = {
		.tv_sec = 0,//windy changed ,not block,2,
		.tv_usec = 0,
	};
	
	if(UDP_STEP_INIT_READY == Udp_Multicast_step)
	{
        FD_ZERO(&rfds);
        FD_SET(Udp_multicast_sock_fd, &rfds);

		s = select(Udp_multicast_sock_fd + 1, &rfds, NULL, NULL, &tv);//阻塞等待
		// ESP_LOGW(TAG,"select_s: %d",s);
		if (s > 0) //ok
		{
			if (FD_ISSET(Udp_multicast_sock_fd, &rfds)) 
			{
			
//				char recvbuf[48];
				// Incoming datagram received
				char raddr_name[32] = { 0 };
                uint16_t raddr_port = 0;

				if(NULL == UdpRxdata)
				{
					UdpRxdata = heap_caps_malloc(UDP_RX_SIZE, MALLOC_CAP_SPIRAM);
				}

//				len = recvfrom(Udp_multicast_sock_fd, recvbuf, sizeof(recvbuf)-1, 0,
//								   (struct sockaddr *)&raddr, &socklen);//(char*)

			   len = recvfrom(Udp_multicast_sock_fd, UdpRxdata, UDP_RX_SIZE-1, 0,
								  (struct sockaddr *)&raddr, &socklen);//(char*)

				// Get the sender's address as a string
				if((len > 0) 
					&&(raddr.ss_family == PF_INET) )
				{
                    inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr, raddr_name, sizeof(raddr_name)-1);
                    raddr_port = ntohs(((struct sockaddr_in *)&raddr)->sin_port);
                    
                    // 获取本机的IP地址
                    char local_ip[32] = { 0 };
                    struct sockaddr_in local_addr;
                    socklen_t local_addr_len = sizeof(local_addr);
                    getsockname(Udp_multicast_sock_fd, (struct sockaddr *)&local_addr, &local_addr_len);
                    inet_ntoa_r(local_addr.sin_addr, local_ip, sizeof(local_ip)-1);
                    
                    // 检查发送者的地址是否与本机的地址相同
                    if(strcmp(raddr_name, local_ip) == 0)
                    {
                        // 如果相同，那么忽略这个报文
                        ESP_LOGW(TAG, "Ignore message from self");
                    }
                    else
                    {
                        // 获取目标地址
                        struct sockaddr_in dest_addr;
                        socklen_t dest_addr_len = sizeof(dest_addr);
                        getsockname(Udp_multicast_sock_fd, (struct sockaddr *)&dest_addr, &dest_addr_len);
                        char dest_ip[32] = {0};
                        inet_ntoa_r(dest_addr.sin_addr, dest_ip, sizeof(dest_ip) - 1);

                        // 判断是否为组播地址
                        uint32_t dest_ip_addr = ntohl(dest_addr.sin_addr.s_addr);
                        if (dest_ip_addr >= 0xE0000000 && dest_ip_addr <= 0xEFFFFFFF)
                        {
                            ESP_LOGW(TAG, "[Udp_multicast_Rx] received multicast %d bytes from %s, %d to %s", len, raddr_name, raddr_port, dest_ip);
                        }
                        else
                        {
                            ESP_LOGW(TAG, "[Udp_multicast_Rx] received unicast %d bytes from %s, %d to %s", len, raddr_name, raddr_port, dest_ip);
                        }

                        UdpRxLen = len;
                        Device_Discovery_Get_Frame(UdpRxdata, UdpRxLen, raddr_name, raddr_port);
                    }
				}
			}
		}
	}

	if(NULL != UdpRxdata)
	{
	
        free(UdpRxdata);
		UdpRxdata = NULL;
	}		
    return (int16_t)len;
}

#endif


int16_t UDP_Rx_Task(void)//100ms
{
	static uint8_t  wifi_connect_ap_old =0;// 
	static uint8_t  wifi_connect_STA_old =0;// 
	uint8_t  s_Net_connect_flag =0;// 


    int s;
    int len = 0;
    int socket = -1;
    fd_set rfds;
    struct sockaddr_storage raddr; // Large enough for both IPv4 or IPv6
    socklen_t socklen = sizeof(raddr);

    struct timeval tv = {
        .tv_sec = 0, // windy changed, not block, 2,
        .tv_usec = 0,
    };

	reals.debug1  = Udp_singlecast_step;
	reals.debug2  = Udp_Multicast_step;

	if(wifi_connect_ap_old != reals.wifi_connect_ap)
	{
		if(0 == wifi_connect_ap_old)//首次接入
		{
			s_Net_connect_flag++;

		}
		wifi_connect_ap_old = reals.wifi_connect_ap;
		
	}

	if(wifi_connect_STA_old != reals.wifi_connect_STA)
	{
		if(0 == wifi_connect_STA_old)//首次接入
		{
			s_Net_connect_flag++;
		}
		ESP_LOGI(V4TAG, " current_netif_id:%u,s_Net_connect_flag =%d", udp_config.current_netif_id,s_Net_connect_flag);
		
			{
				reals.Step_dev_discovery = MESH_FRAME_HEADER_COMMON;
				Udp_singlecast_step = UDP_STEP_RESTART;
				Udp_Multicast_step = UDP_STEP_RESTART;

				udp_config.current_netif_id =NETIF_TYPE_WIFI_AP;//只有WIFI，不切换		
				iot_udp_start(NETIF_TYPE_WIFI_AP);
			}	
		wifi_connect_STA_old = reals.wifi_connect_STA;
		
	}
	
//	if(1 == s_Net_connect_flag)//首次接入
//	{
//		reals.Step_dev_discovery = MESH_FRAME_HEADER_COMMON;
//		Udp_singlecast_step = UDP_STEP_RESTART;
//		Udp_Multicast_step = UDP_STEP_RESTART;
//
//		login_info.current_netif_id =NETIF_TYPE_WIFI_STA;//只有WIFI，不切换		
//		iot_udp_start(NETIF_TYPE_WIFI_STA);
//	}

//	if((1 == reals.wifi_connect_ap)
//		||(reals.wifi_connect_STA > 0))
//	{
//	}

    Udp_multicast_init();
    Udp_singlecast_init();
    
    if ((UDP_STEP_INIT_READY == Udp_Multicast_step)
        &&(UDP_STEP_INIT_READY == Udp_singlecast_step))
    {
        FD_ZERO(&rfds);
        FD_SET(Udp_multicast_sock_fd, &rfds);
        FD_SET(Udp_singlecast_sock_fd, &rfds);
        
        s = select(MAX(Udp_multicast_sock_fd, Udp_singlecast_sock_fd) + 1, &rfds, NULL, NULL, &tv); // 阻塞等待
        
        if (s > 0) // ok
        {
            /*判断接受来源*/
            if (FD_ISSET(Udp_singlecast_sock_fd, &rfds)) {
                socket = Udp_singlecast_sock_fd;
            }
            else if (FD_ISSET(Udp_multicast_sock_fd, &rfds)) {
                socket = Udp_multicast_sock_fd;
            }
            else {
                return 0;
            }
            
            char raddr_name[32] = {0};
            uint16_t raddr_port = 0;
            
            if (NULL == UdpRxdata)
            {
                UdpRxdata = heap_caps_malloc(UDP_RX_SIZE, MALLOC_CAP_SPIRAM);
                if (UdpRxdata == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for UdpRxdata");
                    return -1;
                }
            }
            
            struct msghdr msg;
            struct iovec iov;
            char ctrl_buf[CMSG_SPACE(sizeof(struct in_addr))];
            struct cmsghdr *cmsg;
            
            memset(&msg, 0, sizeof(msg));
            memset(ctrl_buf, 0, sizeof(ctrl_buf));
            
            iov.iov_base = UdpRxdata;
            iov.iov_len = UDP_RX_SIZE - 1;
            msg.msg_name = &raddr;
            msg.msg_namelen = socklen;
            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;
            msg.msg_control = ctrl_buf;
            msg.msg_controllen = sizeof(ctrl_buf);
            
            len = recvmsg(socket, &msg, 0);
            
            if ((len > 0) && (raddr.ss_family == PF_INET))
            {
                inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr, raddr_name, sizeof(raddr_name) - 1);
                raddr_port = ntohs(((struct sockaddr_in *)&raddr)->sin_port);
            
                // 检查发送者的地址是否与本机的地址相同
                if (strcmp(raddr_name, udp_config.source_ip_str) == 0)
                {
                    // 如果相同，那么忽略这个报文
                    ESP_LOGW(TAG, "Ignore message from self");
                }
                else
                {
                    ESP_LOGI(TAG, "[UDP_Rx_Task] socket(%d) received %d bytes from %s, %d", socket, len, raddr_name, raddr_port);
                    UdpRxLen = len;
					dump_buf_global("UdpRxdata !!!!!!!!!!!!!!!!!!!!", UdpRxdata, UdpRxLen);

                    ESP_LOGI(TAG, " Udp_multicast_sock_fd= %d,Udp_singlecast_sock_fd= %d", Udp_multicast_sock_fd, Udp_singlecast_sock_fd);


                    Device_Discovery_Get_Frame(UdpRxdata, UdpRxLen, raddr_name, raddr_port);
					app_sys_debug_info();
                }
            }
        }
    }

    if (NULL != UdpRxdata)
    {
        free(UdpRxdata);
        UdpRxdata = NULL;
    }
    return (int16_t)len;
}



/*------------------------------------------------------------------------------------------------------------------------------------------------*/




/*------------------------------------------------------------------------------
 Function: Udp_Heartbeat_TxFrame
 -----------------------------------------------------------------------------*/
/**
  * @brief      单播心跳帧发送
  * @param[in]  void  
  * @param[out] None
  * @return     uint8_t
  */
uint8_t Udp_Heartbeat_TxFrame(uint8_t *outbuf)
{
    uint16_t Crcvalue=0;
    uint16_t i = 0;
    uint8_t j = 0;
    uint16_t u16Tempdata = 0;
    
    outbuf[i++] = MESH_FRAME_HEADER_COMMON;//reserved
    outbuf[i++] = MESH_FRAME_VERSION_WIFI;//1-WIFI
    
	memcpy((uint8_t*)&outbuf[i], (uint8_t*)&iot_factory.iot_sn, 8);//源 SN
	i+=8;

    //机型序号_源设备	
	u16Tempdata=SN_TYPE_SELF;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;

    //SN,机型序号_目标设备 
    for (j = 0; j < 10; j++)
    {
        outbuf[i++] = 0;
    } 
    
    //报文类型  
    outbuf[i++] = MESH_FRAME_TYPE_HEART;

/*
源设备网络IPV4地址:路由器决定
(顺序填充，数字内容表示，如192.168.1.2依次填充2,1,168,192这4个数字)

*/
	if((NETIF_TYPE_WIFI_STA == udp_config.current_netif_id)
		||(NETIF_TYPE_WIFI_AP == udp_config.current_netif_id))		
    {
        for (j = 0; j < 4; j++)//源 IP
        {
//            outbuf[i++] = g_self_data.mod_reg11000_IOT_info.sta_ipv4[3-j];//MAC
            outbuf[i++] = reals.self_wifi_ap_ip[3-j];//
            
        } 
    }
    else if ( udp_config.current_netif_id == NETIF_TYPE_ETH )
    {
        for (j = 0; j < 4; j++)//源 IP
        {
            outbuf[i++] = g_self_data.mod_reg11000_IOT_info.IP_ETH[3-j];//MAC
        } 
    }
    else
    {
        for (j = 0; j < 4; j++)//源 IP
        {
			outbuf[i++] = 0;	
        }     
//        return 0;
    }

/*
源设备网络IPV4 服务端口号：本地可随机指定，但是必须要和实际UDP单播发送一致

*/	
	u16Tempdata = UDP_PORT_SINGLE;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;
	    
    //机型序号_源设备    
    u16Tempdata=SN_TYPE_SELF;
    outbuf[i++]=u16Tempdata&0xFF;
    outbuf[i++]=u16Tempdata>>8;

    //信号强度
    outbuf[i++]=g_self_data.mod_reg11000_IOT_info.sta_rssi;
        
    for (j = 0; j < 10; j++)//reserved
    {
        outbuf[i++] = 0;
    } 
    
    Crcvalue = ModbusCrc16(outbuf, i);
    
    outbuf[i++] = (uint8_t) Crcvalue;
    outbuf[i++] = (uint8_t)(Crcvalue>>8);
    
    return i;
}



/*------------------------------------------------------------------------
*@Function： Udp_Singlecast_Modbus_MasterTxCmd
-------------------------------------------------------------------------*/
/**
*@brief   组帧
regAddress :   reg address
regNum  :  reg num
*outbuf :   组帧输出
slave_address :   slave address
frametype :   报文类型
*input_regdata:  输入寄存器内容


*@return      frame len   
*/
uint16_t Udp_Singlecast_Modbus_MasterTxCmd(uint16_t regAddress, uint8_t regNum,
                                            uint8_t *outbuf, uint8_t *dst_sn_type,
                                            uint8_t frametype, uint16_t data_len)
{
    uint16_t Crcvalue=0;
    uint16_t i = 0;
    uint16_t len = 0;
    uint16_t u16Tempdata = 0;
    uint8_t slave_address = 0;

    if(true == is_array_all_zero(dst_sn_type, 10))
    {
        outbuf[i++] = MESH_FRAME_HEADER_WIFI_MESH_BROADCAST;// 
    }
    else
    {
        outbuf[i++] = MESH_FRAME_HEADER_WIFI_MESH_SINGLE;// 
    }

    outbuf[i++] = MESH_VERSION_TYPE_WIFI_MESH;//1-WIFI
    
	memcpy((uint8_t*)&outbuf[i], (uint8_t*)&iot_factory.iot_sn, 8);//源 SN
	i+=8;

    //机型序号_源设备	
	u16Tempdata=SN_TYPE_SELF;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;

    //SN,机型序号_目标设备 
    memcpy((uint8_t*)&outbuf[i], dst_sn_type, 10);
    i+=10;
    
    //报文类型  
    outbuf[i++] = frametype;//报文类型

    if(MESH_FRAME_TYPE_READ == frametype)
    {        
        len = Modbus_MasterReadCmd_03H(regAddress,regNum,&outbuf[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD],0);
    }
    else if((MESH_FRAME_TYPE_WRITE == frametype)
		||(MESH_FRAME_TYPE_PERIOD == frametype))
    {
        if((700 == regAddress)&&(6 == regNum) )//ota 700, 6len;OTA 特殊处理
        {
            len = 21;//modbus tx len
        }
        else
        {
            //S1的 slave addr 必须从100开始排序交互
            if ((regAddress >= MOD_REG_START_ADDR_14500) && ((regAddress+ regNum) <= (MOD_REG_START_ADDR_14500 + MOD_REG_LEN_14500)))
            {
                slave_address = MODBUS_SLAVE_ADDR_WIFI_S1_START;
            }
            else
            {
                slave_address = 0;
            }
            len = Modbus_MasterWriteCmd_06H_10H(regAddress, regNum, false, &outbuf[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], slave_address);
        }
        // len = Modbus_MasterWriteCmd_06H_10H(regAddress, regNum, input_regdata, &outbuf[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], 0);
    } 
    else if(MESH_FRAME_TYPE_READ_RTN == frametype)//tbd
    {
        len = Modbus_Error(&outbuf[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], CMD_NOT_COMPLETE);//BAD_COUNT
    }
	else if(MESH_FRAME_TYPE_WRITE_RTN == frametype)//0x10 rtn
    {
        len = Modbus_Error(&outbuf[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], CMD_NOT_COMPLETE);//BAD_COUNT
    }
    else if(MESH_FRAME_TYPE_XMODEM == frametype)
    {
        len = data_len;// 
    }
    else if(MESH_FRAME_TYPE_XMODEM_RTN == frametype)
    {
        len = 4;//有效内容已在此函数外 前赋值
    }

    else
    {
        ESP_LOGE(TAG, "Udp_Singlecast_Modbus_MasterTxCmd : frametype:%u", frametype);
    }

    u16Tempdata = MESH_NODE_MAX_COUNT;
    outbuf[i++]=u16Tempdata&0xFF;//TTL
    outbuf[i++]=u16Tempdata>>8;//TTL

    //modbus协议帧区长度    
    u16Tempdata = len;
    outbuf[i++]=u16Tempdata&0xFF;
    outbuf[i++]=u16Tempdata>>8;
    i += len;
    
    Crcvalue = ModbusCrc16(outbuf, i);
    
    outbuf[i++] = (uint8_t) Crcvalue;
    outbuf[i++] = (uint8_t)(Crcvalue>>8);
    
    return i;
}





/*------------------------------------------------------------------------
*@Function： Udp_Singlecast_Modbus_MasterTxCmd
基于modbus核心报文添加外部 外部设备间MESH协议外壳，modbus报文已在外部填充
-------------------------------------------------------------------------*/
/**
*@brief   组帧
regAddress :   reg address
regNum  :  reg num
*outbuf :   组帧输出
slave_address :   slave address
frametype :   报文类型
*input_regdata:  输入寄存器内容
dst_sn_type: 10字节  目标DEV SN +type
modebus_frame_len：待填充的modbus报文长度



*@return      frame len   
*/
uint16_t Udp_Singlecast_Modbus_MasterTxCmd_add_head_shell( uint8_t *outbuf, uint8_t *dst_sn_type,uint8_t frametype,uint16_t modebus_frame_len )
{
    uint16_t Crcvalue=0;
    uint16_t i = 0;
    uint16_t u16Tempdata = 0;
	uint8_t slave_address =0;

	if(true == is_array_all_zero(dst_sn_type, 10))
	{
	    outbuf[i++] = MESH_FRAME_HEADER_WIFI_MESH_BROADCAST;// 
	}
	else
	{
	    outbuf[i++] = MESH_FRAME_HEADER_WIFI_MESH_SINGLE;// 
	}

	
    outbuf[i++] = MESH_VERSION_TYPE_WIFI_MESH;//1-WIFI
    
	memcpy((uint8_t*)&outbuf[i], (uint8_t*)&iot_factory.iot_sn, 8);//源 SN
	i+=8;

    //机型序号_源设备	
	u16Tempdata=SELF_DEV_TYPE;
	outbuf[i++]=u16Tempdata&0xFF;
	outbuf[i++]=u16Tempdata>>8;

    //SN,机型序号_目标设备 
    memcpy((uint8_t*)&outbuf[i], dst_sn_type, 10);
    i+=10;
    
    //报文类型  
    outbuf[i++] = frametype;//报文类型

    {
        ESP_LOGE(TAG, "Udp_Singlecast_Modbus_MasterTxCmd_add_head_shell : frametype:%u", frametype);
    }
	

    u16Tempdata = MESH_NODE_MAX_COUNT;
    outbuf[i++]=u16Tempdata&0xFF;//TTL
    outbuf[i++]=u16Tempdata>>8;//TTL
    
    //modbus协议帧区长度    
    u16Tempdata = modebus_frame_len;
    outbuf[i++]=u16Tempdata&0xFF;
    outbuf[i++]=u16Tempdata>>8;
    i += modebus_frame_len;
    
    Crcvalue = ModbusCrc16(outbuf, i);
    
    outbuf[i++] = (uint8_t) Crcvalue;
    outbuf[i++] = (uint8_t)(Crcvalue>>8);
    
    return i;
}

/*------------------------------------------------------------------------
*@Function： Modbus_Format_Check
判断接收报文是否为modbus格式

*@param[in] 	*income
*@param[out]	inlen
*@return		 
-1： fail
other:功能码

名称	长度(Byte)	说明	备注	起始地址
协议头	1	0,预留		0
协议版本	1	1		1
MAC_源设备	8			2
MAC_目标设备	8			12
"报文类型
（功能码）"	1	"bit7~:4:
0-modbus beta协议
其他-预留
bit3~0:
1-读取；
2-写入；
3-读取-RTN；
4-写入-RTN；
5-周期上报（无RTN）
6-周期上报（心跳）" 	22
modbus协议帧 N	"标准modbus;
报文类型和modbus协议功能码要匹配对应；
增加功能主要为区分modbus 相同功能码的 发送和RTN等"		25
*/
int Udp_Net_Frame_Modbus_Format_Check(const uint8_t *income, uint16_t inlen) 
{
    int8_t ret = 0;
    int8_t frame_type = income[WIFI_UDP_FRAME_ADDR_TYPE]&0xF;
    uint8_t funcode = income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD+1]&0x7F;
    uint8_t errflag = income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD+1]&0x80;
	uint16_t Value_crc16 = ModbusCrc16(income, (inlen - 2));

    if (income[WIFI_UDP_FRAME_ADDR_VER] != MESH_VERSION_TYPE_WIFI)
    {
		ret =-1;
		ESP_LOGE(TAG, "WIFI_UDP_FRAME_ADDR_VER error");
    }
	else if((Value_crc16 != (((uint16_t)income[inlen - 1]<<8) | income[inlen - 2])) // crc check
		||(frame_type > 8))//报文类型,非法
	{ 
		ret =-1;
		ESP_LOGE(TAG, "frame_type or crc error");
	}
    else if((1 == frame_type)||(2 == frame_type)||(3 == frame_type)||(4 == frame_type)||(5 == frame_type))
    {
        if(errflag)
        {
            ret =-1;
            ESP_LOGE(TAG, " modbus funcode:%x", income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD+1]);
            ESP_LOGE(TAG, " modbus errcode:%x", income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD+2]);
        }
        else if((0x03 != funcode) &&(0x06 != funcode)&&(0x10 != funcode))//modbus功能码
        {
            ret =-1;
            ESP_LOGE(TAG, " modbus funcode:%x", income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD+1]);
        }
        else
    	{
    	    ret= frame_type;
    	}
    }
	else
	{
	    ret= frame_type;
	}


	return ret; /* 返回接收的功能码 */
}

#if 1
/*------------------------------------------------------------------------
*@Function： Modbus_MasterRespones_Udp
无线设备间协议：常规单播报文RX解析
ESP32S3做Modbus主，本函数是主解析modbus从的报文
-------------------------------------------------------------------------*/
/**
*@brief  
*@param[in]     struct_uart1
*@param[in]     *cmdBuf,接收buf

*@param[out]    cmdLen接收长度
*@return         
0- ok
1- fail,no data

*/
uint8_t Modbus_MasterRespones_Udp(uint8_t *income, uint16_t cmdLen)
{
    int8_t frame_type=0;
    uint8_t errFlags;
    uint16_t crc;
    uint8_t ret = 0;
    uint8_t j = 0;
    uint16_t len;
    int16_t rsp_len;
    uint8_t dev_list_slaveaddr;
    uint16_t bias =0;
    uint8_t slave_address =0;
    reg_position_list_t *position_list = NULL;
    UDP_MODBUS_STRUCT *udp_modbus = &g_udp_modbus;
    msg_general_t *msg_gen = (msg_general_t *)income;
    uint8_t *modbus_data =&income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD];

    uint8_t i=0;
    uint8_t *dev_sn = NULL; 
    for (i = 0; i < MESH_NODE_MAX_COUNT; i++)// 
    {
        dev_sn = wifi_mesh_get_node_SN(i);
        if(msg_gen->src_devSN.sn == *(uint64_t *)dev_sn)
        {
            slave_address =wifi_mesh_get_node_modbus_slave_address(i);
            msg_gen->src_devSN.dev_type = *(uint16_t *)&dev_sn[9];
            break;
        }
    }

    frame_type = msg_gen->frame_type.code;

    if(frame_type > 0) 
    {
        ESP_LOGW(TAG, "[Modbus_MasterRespones_Udp] frame_type: %d",frame_type);	

        switch (frame_type)
        {
            case MESH_FRAME_TYPE_READ: 

                rsp_len = Modbus_Slave(modbus_data, msg_gen->md_len, 
                                        &wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], NULL, NULL, MD_CHL_WIFI_CLOUD, &position_list); /* modbus handle */
                if (rsp_len > 0) 
                {
                    if(//((0 == msg_gen->dst_dev_sn)
                        //&&(0 == msg_gen->dst_dev_type_code))||
                        ((reals.wifimesh_self_dev_sn == msg_gen->dst_devSN.sn)
                        &&(reals.wifimesh_self_dev_type_code == msg_gen->dst_devSN.dev_type)))//只响应自身
                    {
                        udp_modbus->TxBytesNum = Udp_Singlecast_Modbus_MasterTxCmd_add_head_shell(wireless_interface.data_tx_wifi_mesh, &income[WIFI_UDP_FRAME_ADDR_SN_SOURCE], MESH_FRAME_TYPE_READ_RTN,rsp_len );
                
                        ESP_LOGE(TAG, "Udp_singlecast_Tx MESH_FRAME_TYPE_READ FFF");
                        send_wifi_mesh_data(msg_gen->src_devSN.sn,msg_gen->src_devSN.dev_type, 
                                wireless_interface.data_tx_wifi_mesh, udp_modbus->TxBytesNum);
                    }
                }
                break;

            case MESH_FRAME_TYPE_WRITE: 
                rsp_len = Modbus_Slave(modbus_data, msg_gen->md_len, 
                                        &wireless_interface.data_tx_wifi_mesh[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], NULL, NULL, MD_CHL_WIFI_CLOUD, &position_list); /* modbus handle */
                if (rsp_len > 0) 
                {
                    if (NULL != position_list) 
                    {
                        position_list->position.chl =MD_CHL_UART_DOWN;
                        // sys_new_position_and_transmit(position_list);//透传转发给下级uart
                    }

                    if (vXmodemCmdCheck(modbus_data[0], OTA_CH_WIFI_MESH_TO_SELF)) //初始化xmodem升级变量
                    {
                        ESP_LOGW(TAG, "ble received xmodem start cmd");
                    }
#ifdef CAN_PORT_ENABLE
                    /* 为modbus转can指令申请内存 */
//						can_cmd_queue_struct can_cmd = {NULL, 2, 0};//10，windy实际只有一次转发1帧
//						uint8_t can_cmd_flag = 0;
//	//					if ((rst == 0x06 || rst == 0x10) && can_cmd_queue) 
//						{ // 当MODBUS为设置指令时,才需要开辟空间
//							can_cmd.cmd = calloc(sizeof(can_data_label) * can_cmd.num, 1);
//							if (!can_cmd.cmd) {
//								ESP_LOGE (TAG, "ble to can malloc failed");
//							}
//						}
//					
//						/* modbus指令转换为can指令发送到队列 */
//						sys_new_can_data_resend(&can_cmd, md_addr);
#endif
                    if(//((0 == msg_gen->dst_dev_sn)
                        //&&(0 == msg_gen->dst_dev_type_code))||
                        ((reals.wifimesh_self_dev_sn == msg_gen->dst_devSN.sn)
                        &&(reals.wifimesh_self_dev_type_code == msg_gen->dst_devSN.dev_type)))//只响应自身
                    {
                        udp_modbus->TxBytesNum = Udp_Singlecast_Modbus_MasterTxCmd_add_head_shell(wireless_interface.data_tx_wifi_mesh, &income[WIFI_UDP_FRAME_ADDR_SN_SOURCE], MESH_FRAME_TYPE_WRITE_RTN,rsp_len );
                        ESP_LOGE(TAG, "Udp_singlecast_Tx MESH_FRAME_TYPE_WRITE  FFF");
                        send_wifi_mesh_data(msg_gen->src_devSN.sn, msg_gen->src_devSN.dev_type, 
                                    wireless_interface.data_tx_wifi_mesh, udp_modbus->TxBytesNum);
                    }
                }
                break;

            case MESH_FRAME_TYPE_READ_RTN://windy tbd
                ret = Modbus_ReadReg_03H_RTN_Udp(udp_modbus, modbus_data);
                break;

            case MESH_FRAME_TYPE_WRITE_RTN: //windy tbd
                ret = Modbus_WriteReg_06H_10H_RTN_Udp(udp_modbus, modbus_data);
                break;

            case MESH_FRAME_TYPE_PERIOD:
                if(income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + 1] == 0x06)
                {
                    //周期上报 不做回复
                    Modbus_WriteSingleReg2(modbus_data, msg_gen->md_len, NULL,NULL,NULL,MD_CHL_WIFI_CLOUD,NULL,slave_address,false);
                }
                else if(income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + 1] == 0x10)
                {
                    ESP_LOGE(TAG, "wifi mesh rx MESH_FRAME_TYPE_PERIOD ,0x10 fun!");
                    //周期上报 不做回复
                    slave_address +=MODBUS_SLAVE_ADDR_WIFI_S1_START;
                    Modbus_WriteMultiRegs2(modbus_data, msg_gen->md_len, NULL,NULL,NULL,MD_CHL_WIFI_CLOUD,NULL,slave_address,false);
                }
                else
                {
                    ESP_LOGE(TAG, "frame_type error (%d)(%u)",frame_type, income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + 1]);
                }
                break;

            case MESH_FRAME_TYPE_HEART: 
                ret = Wifi_Udp_Network_Heartbeat(income);
                break;

            default:
                ESP_LOGE(TAG, "frame_type invalid");
                break;
        }
    }
    else
    {
        ESP_LOGW(TAG, "frame_type error (%d)",frame_type);
        ret = 1;
    }

    return ret;
}
#else
uint8_t Modbus_MasterRespones_Udp(uint8_t *income, uint16_t cmdLen)
{
    int8_t frame_type=0;
    uint8_t errFlags;
    uint16_t crc;
    uint8_t ret = 0;
    uint8_t j = 0;
    uint16_t len;
    int16_t rsp_len;
    uint8_t dev_list_slaveaddr;
    uint16_t bias =0;
	uint8_t slave_address =0;
    reg_position_list_t *position_list = NULL;
    
    UDP_MODBUS_STRUCT *udp_modbus = &reals.udp_modbus;
        
//    ESP_LOGW(VmodbusTAG, "[Modbus_MasterRespones_Udp]  cmdLen = %d", cmdLen);
//    ESP_LOG_BUFFER_HEX_LEVEL(VmodbusTAG, income, cmdLen, ESP_LOG_WARN);
    frame_type = Udp_Net_Frame_Modbus_Format_Check(income, cmdLen);
    
	if((-1) != frame_type) 
	{         	 	     
        ESP_LOGW(VmodbusTAG, "[Modbus_MasterRespones_Udp] frame_type: %d",frame_type);	
		memset(&reals.discovery_Info, 0, sizeof(reals.discovery_Info));
		memcpy(&reals.discovery_Info.SN[0], &income[WIFI_UDP_FRAME_ADDR_SN_SOURCE], 10);		
		reals.discovery_Info.net_point_online =NET_POINT_ONLINE;
		reals.discovery_Info.net_point_TimeOut_cnt =0;
		
		top_dev_info_queue_Push(&reals.discovery_Info);

		if(SN_TYPE_SELF == *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]))// 
		{
			for (j = 0; j < (reals.Topnet_point_Num_invbat); j++)// 
			{ 
				if((*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]) == *((uint64_t *)reals.discovery_net_Info[reals.Topseq_Invbat_index[j]].SN))//sn
				 &&(*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]) != 0))//已存储SN
				{
					if(reals.discovery_net_Info[reals.Topseq_Invbat_index[j]].ptr_modbus_data >= (uint32_t)&Inv[0])
					{
						bias = (reals.discovery_net_Info[reals.Topseq_Invbat_index[j]].ptr_modbus_data -(uint8_t *)&Inv[0])/sizeof(Inv[0]);					
					}
					else
					{
						ESP_LOGE(TAG, "ptr_modbus_data error "  );
					}
					udp_modbus->slaveaddress = bias  + MODBUS_SLAVE_ADDR_WIFI_INVBAT_START;
				   ESP_LOGI(TAG, "[Modbus_MasterRespones_Udp SN_TYPE_SELF]  SN:%llu, tpye:%u, frame:%d,udp_modbus->slaveaddress=%d",  
	                   *((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]), 
	                   *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]),
	                   frame_type,udp_modbus->slaveaddress);					
					break;
				 }
			}
		}
		else if(SN_TYPE_S1 == *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]))// 
		{
			for (j = 0; j < (reals.Topnet_point_Num_S1); j++)// 
			{ 
				if((*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]) == *((uint64_t *)reals.discovery_net_Info[reals.Topseq_S1_index[j]].SN))//sn
				 &&(*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]) != 0))//已存储SN
				{
					if(reals.discovery_net_Info[reals.Topseq_S1_index[j]].ptr_modbus_data >= (uint32_t)&Plug[0])
					{
						bias = (reals.discovery_net_Info[reals.Topseq_S1_index[j]].ptr_modbus_data -(uint8_t *)&Plug[0])/sizeof(Plug[0]);					
					}
					else
					{
						ESP_LOGE(TAG, "ptr_modbus_data error "  );
					}
					udp_modbus->slaveaddress = bias  + MODBUS_SLAVE_ADDR_WIFI_S1_START;
		
				   ESP_LOGI(TAG, "[Modbus_MasterRespones_Udp  SN_TYPE_S1]  SN:%llu, tpye:%u, frame:%d,udp_modbus->slaveaddress=%d",  
	                   *((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]), 
	                   *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]),
	                   frame_type,udp_modbus->slaveaddress);						
					break;
				 }
			}
		}
		else//if(SN_TYPE_SELF == *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]))//   普通,mix tbd
		{
			for (j = 0; j < (reals.Topnet_point_Num_mix); j++)// 
			{ 
				if((*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]) == *((uint64_t *)reals.discovery_net_Info[reals.Topseq_mix_index[j]].SN))//sn
				 &&(*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]) != 0))//已存储SN
				{
					bias =0;
					if(reals.discovery_net_Info[reals.Topseq_Invbat_index[j]].ptr_modbus_data >= (uint8_t *)&Plug[0])
					{
//						bias = (reals.discovery_net_Info[reals.Topseq_Invbat_index[j]].ptr_modbus_data -(uint32_t)&Inv[0])/sizeof(Inv[0]);				
						ESP_LOGE(TAG, "ptr_modbus_data error,tbd "	);

					}
					else
					{
						ESP_LOGE(TAG, "ptr_modbus_data error "  );
					}				
					udp_modbus->slaveaddress = bias + MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START;
				   ESP_LOGI(TAG, "[Modbus_MasterRespones_Udp  MIX]  SN:%llu, tpye:%u, frame:%d,udp_modbus->slaveaddress=%d",  
	                   *((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]), 
	                   *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]),
	                   frame_type,udp_modbus->slaveaddress);						
					break;
				 }
			}
		}


		if(SN_TYPE_SELF == *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]))// 
		{
			if(j >= (reals.Topnet_point_Num_invbat))
			{			 
				ESP_LOGE(VmodbusTAG, "[Modbus_MasterRespones_Udp] sn error or unsave(%llu)，Topnet_point_Num_invbat",*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]));
				  return 0;//windy debug tbd
			}

		}
		else if(SN_TYPE_S1 == *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]))// 
		{
			if(j >= (reals.Topnet_point_Num_S1))
			{			 
				ESP_LOGE(VmodbusTAG, "[Modbus_MasterRespones_Udp] sn error or unsave(%llu),Topnet_point_Num_S1",*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]));
				  return 0;//windy debug tbd
			}

		}
		else//if(SN_TYPE_SELF == *((uint16_t *)&income[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]))//   普通,mix tbd
		{
			if(j >= (reals.Topnet_point_Num_mix))
			{			 
				ESP_LOGE(VmodbusTAG, "[Modbus_MasterRespones_Udp] sn error or unsave(%llu),Topnet_point_Num_mix",*((uint64_t *)&income[WIFI_UDP_FRAME_ADDR_SN_SOURCE]));
				  return 0;//windy debug tbd
			}

		}

		dump_buf_global("Udp RX  modbus data", &income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], (cmdLen - WIFI_UDP_FRAME_ADDR_MODBUS_HEAD - 2));

        switch (frame_type)
        {
            case MESH_FRAME_TYPE_READ: 

                rsp_len = Modbus_Slave(&income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], (cmdLen - WIFI_UDP_FRAME_ADDR_MODBUS_HEAD - 2), 
                                        &Udp_TxBuffer[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], NULL, NULL, MD_CHL_WIFI, &position_list); /* modbus handle */
                if (rsp_len > 0) 
                {
                    len = (uint16_t)rsp_len;
					if(udp_modbus->slaveaddress >= MODBUS_SLAVE_ADDR_WIFI_INVBAT_START)
					{
						dev_list_slaveaddr = udp_modbus->slaveaddress - MODBUS_SLAVE_ADDR_WIFI_INVBAT_START;
					}
					if(udp_modbus->slaveaddress >= MODBUS_SLAVE_ADDR_WIFI_S1_START)
					{
						dev_list_slaveaddr = udp_modbus->slaveaddress - MODBUS_SLAVE_ADDR_WIFI_S1_START;
					}
					if(udp_modbus->slaveaddress >= MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START)
					{
						dev_list_slaveaddr = udp_modbus->slaveaddress - MODBUS_SLAVE_ADDR_WIFI_OTHER_MIX_START;
					}
					else
					{
						dev_list_slaveaddr =0;//tbd

					}
					ESP_LOGI(TAG, "tx udp_modbus->slaveaddress =%d,dev_list_slaveaddr =%d",udp_modbus->slaveaddress, dev_list_slaveaddr);
					
                    udp_modbus->TxBytesNum = Udp_Singlecast_Modbus_MasterTxCmd(NULL, NULL, Udp_TxBuffer, dev_list_slaveaddr, MESH_FRAME_TYPE_READ_RTN, &len);
					ESP_LOGE(TAG, "Udp_singlecast_Tx   FFF");

					Udp_singlecast_Tx(&reals.discovery_net_Info[dev_list_slaveaddr].MAC[0], 
                            &reals.discovery_net_Info[dev_list_slaveaddr].MAC[4], Udp_TxBuffer, udp_modbus->TxBytesNum);
                }

                break;	
                
            case MESH_FRAME_TYPE_WRITE: 

				rsp_len = Modbus_Slave(&income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], (cmdLen - WIFI_UDP_FRAME_ADDR_MODBUS_HEAD - 2), 
                                        &Udp_TxBuffer[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], NULL, NULL, MD_CHL_WIFI, &position_list); /* modbus handle */
				if (rsp_len > 0) 
				{
				    len = (uint16_t)rsp_len;
                    dev_list_slaveaddr = udp_modbus->slaveaddress - NET_SUB1G_MAX_POINT - 1;
                    udp_modbus->TxBytesNum = Udp_Singlecast_Modbus_MasterTxCmd(NULL, NULL, Udp_TxBuffer, dev_list_slaveaddr, MESH_FRAME_TYPE_WRITE_RTN, &len);

					ESP_LOGE(TAG, "Udp_singlecast_Tx   EEE");

					Udp_singlecast_Tx(&reals.discovery_net_Info[dev_list_slaveaddr].MAC[0], 
                            &reals.discovery_net_Info[dev_list_slaveaddr].MAC[4], Udp_TxBuffer, udp_modbus->TxBytesNum);
                }
                
				if (NULL != position_list) 
				{
//					sys_new_position_and_transmit(position_list);//copy from A80,透传转发给下级uart
				}						
                
                break;	

            case MESH_FRAME_TYPE_READ_RTN:
                
            	ret = Modbus_ReadReg_03H_RTN_Udp(udp_modbus, &income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD]);
                break;
                
            case MESH_FRAME_TYPE_WRITE_RTN: 
                
            	ret = Modbus_WriteReg_06H_10H_RTN_Udp(udp_modbus, &income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD]);
                break;   
                
            case MESH_FRAME_TYPE_PERIOD:  
                
                if(income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + 1] == 0x06)
                {
                    //周期上报 不做回复
                    Modbus_Slave_WriteSingleReg_Udp(&income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], (cmdLen - WIFI_UDP_FRAME_ADDR_MODBUS_HEAD - 2), NULL);
                }
                else if(income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + 1] == 0x10)
                {
                    //周期上报 不做回复
                    Modbus_Slave_WriteMultiRegs_Udp(&income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD], (cmdLen - WIFI_UDP_FRAME_ADDR_MODBUS_HEAD - 2), NULL);
                }
                else
                {
                    ESP_LOGE(VmodbusTAG, "frame_type error (%d)(%u)",frame_type, income[WIFI_UDP_FRAME_ADDR_MODBUS_HEAD + 1]);
                }

                break;
                
            case MESH_FRAME_TYPE_HEART: 
                
                ret = Wifi_Udp_Network_Heartbeat(income);
                break;	
                
            default: 

                ESP_LOGE(VmodbusTAG, "frame_type invalid");
                break;
        }
	}	
    else
    {
        ESP_LOGW(VmodbusTAG, "frame_type error (%d)",frame_type);
        ret = 1;
    }
    
    return ret;
}
#endif

/*------------------------------------------------------------------------------
 Function: Modbus_Slave_WriteSingleReg_Udp
 -----------------------------------------------------------------------------*/
/**
  * @brief      从模式 06H
  * @param[in]  const uint8_t *income  
                uint16_t inLen         
                uint8_t *response      
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t Modbus_Slave_WriteSingleReg_Udp(const uint8_t *income, uint16_t inLen, uint8_t *response)
{
    uint16_t j = 0;
	reg2_position_t reg_position;
	uint16_t *p_tab2 = NULL;
	
    uint16_t SlaveAddr = reals.udp_modbus.slaveaddress;//income[0];//
    
    uint16_t startAddress = income[2]<<8 | income[3]; // 开始地址
    uint16_t writeRegData = income[4]<<8 | income[5]; // 写入的数据,数据已经交换lsb
    if (inLen != 8) {
        ESP_LOGE(VmodbusTAG, "WriteSingleReg BAD_COUNT");
        
        if(response != NULL)
        {
            return Modbus_Error(response, BAD_COUNT);
        }
        return 0;
    }
	p_tab2 = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddr, startAddress, 1, false,NULL, NULL ,&reg_position); 			//  
//    p_tab2 = vLookupDataTab2(MASTER_ESP2UART, SlaveAddr, startAddress, 1, false, &reg_position);					// 查询table2中的数据
    if(NULL != p_tab2)
    {
        p_tab2[0] = writeRegData;//H/L 
        ESP_LOGW(VmodbusTAG, "WriteSingleReg : %x", p_tab2[0]);
    }
    else
    {
        ESP_LOGE(VmodbusTAG, "WriteSingleReg vLookupDataTab2 UNKNOWN_REG_ADDRESS");

        if(response != NULL)
        {
            return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
        }
        return 0;
    }

    if(response != NULL)
    {
        response[j++] = income[0];
        response[j++] = 0x06;
        response[j++] = income[2];
        response[j++] = income[3];
        response[j++] = income[4];
        response[j++] = income[5];
        response[j++] = income[6];
        response[j++] = income[7];
    }
    return j;
}


/*------------------------------------------------------------------------------
 Function: Modbus_Slave_WriteMultiRegs_Udp
 -----------------------------------------------------------------------------*/
/**
  * @brief      从模式 10H
  * @param[in]  const uint8_t *income  
                uint16_t inLen         
                uint8_t *response      
  * @param[out] None
  * @return     static uint16_t
  */
static uint16_t Modbus_Slave_WriteMultiRegs_Udp(const uint8_t *income, uint16_t inLen, uint8_t *response) 
{
    uint16_t i = 0;
    uint16_t j = 0;
	reg2_position_t reg_position;
	uint16_t *p_tab2 = NULL;
    uint16_t writeRegData;
    
    uint16_t SlaveAddr = reals.udp_modbus.slaveaddress;//income[0];//

    uint16_t startAddress  = income[2]<<8 | income[3]; // 写入寄存器地址
    uint16_t writeRegsCnt  = income[4]<<8 | income[5]; // 写入寄存器数量

    if (inLen < 9 || (inLen - 9) != (writeRegsCnt*2)) 
	{
        if(response != NULL)
        {
            return Modbus_Error(response, BAD_COUNT);
        }
        return 0;
    }
	
//	SlaveAddr =SlaveAddr +MODBUS_SLAVE_ADDR_WIFI_S1_START;//tbd
	p_tab2 = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddr, startAddress, writeRegsCnt, false,NULL, NULL ,&reg_position); 			//  
//	p_tab2 = vLookupDataTab2(MASTER_ESP2UART, SlaveAddr, startAddress, writeRegsCnt, false, &reg_position);					// 查询table2中的数据
	if(p_tab2 != NULL)//微逆，uart
	{
		for ( i = 0; i < writeRegsCnt; i++) 
		{
			 writeRegData = ((uint16_t)income[7+2*i]<<8) | income[8 + 2*i];
			*(p_tab2+i) = writeRegData;
		}	
	}
	else
	{
        ESP_LOGE(VmodbusTAG, "WriteMultiRegs vLookupDataTab2 UNKNOWN_REG_ADDRESS");
        if(response != NULL)
        {
            return Modbus_Error(response, UNKNOWN_REG_ADDRESS);
        }
        return 0;
	}

    if(response != NULL)
    {
    	response[j++] = income[0];
        response[j++] = 0x10;
        response[j++] = income[2];
        response[j++] = income[3];
        response[j++] = income[4];
        response[j++] = income[5];
        uint16_t crc16 = ModbusCrc16(response, j);
        response[j++] = crc16;
        response[j++] = crc16 >> 8;
    }

    return j;
}


/*------------------------------------------------------------------------------
 Function: Wifi_Udp_Network_Heartbeat
 -----------------------------------------------------------------------------*/
/**
  * @brief      心跳帧识别
  * @param[in]  UART_STRUCT *struct_uart  
                const uint8_t *cmdBuf     
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t Wifi_Udp_Network_Heartbeat(const uint8_t *cmdBuf)//tbd
{
    char     iot_type[12];//

    reals.Wifi_Udp_point_rx_get = 1;
    
    //机型序号(机型序号)
	uint16_t Type_Cnt = *((uint16_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]);
    //SN
    uint64_t SN_64 = *((uint64_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_SN_SOURCE]);

    uint8_t slaveaddress = 0;//income[0];//reals.udp_modbus.slaveaddress;
    uint8_t j = 0;
    
    ESP_LOGW(TAG, "[Wifi_Udp_Network_Heartbeat]  SN:%llu, tpye:%u", 
        *((uint64_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_SN_SOURCE]), 
        *((uint16_t *)&cmdBuf[WIFI_UDP_FRAME_ADDR_TYPE_SOURCE]));

	

	
	if(SN_TYPE_A100_Sub1G == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_A100_Sub1G_ASCII,strlen(SN_TYPE_A100_Sub1G_ASCII));//SN
	}
	else if(SN_TYPE_A100_WIFI == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_A100_WIFI_ASCII,strlen(SN_TYPE_A100_WIFI_ASCII));//SN
	}
	else if(SN_TYPE_D100S == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_D100S_ASCII,strlen(SN_TYPE_D100S_ASCII));//SN
	}
	else if(SN_TYPE_A80 == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_A80_ASCII,strlen(SN_TYPE_A80_ASCII));//SN
	}
	else if(SN_TYPE_S1 == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_S1_ASCII,strlen(SN_TYPE_S1_ASCII));//SN
	}
    else if(SN_TYPE_COMBOX == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_COMBOX_ASCII,strlen(SN_TYPE_COMBOX_ASCII));//SN
	}
	else if(SN_TYPE_AT1 == Type_Cnt)
	{
		memcpy(&iot_type, SN_TYPE_AT1_ASCII,strlen(SN_TYPE_AT1_ASCII));//SN
	}
	else
	{
		ESP_LOGE(TAG, "[Wifi_Udp_Network_Heartbeat]  Native type : ERROR(%d)", Type_Cnt);
		return 1;
	}

	//	memcpy(&Inv[slaveaddress].mod_reg11000_IOT_info.iot_type, iot_type,strlen(iot_type));//SN
	//	  Inv[slaveaddress].mod_reg11000_IOT_info.iot_sn = SN_64;//SN


    ESP_LOGW(TAG, "[Wifi_Udp_Network_Heartbeat]  Native type : %s", iot_type);

    return 0;

}

/*------------------------------------------------------------------------
*@Function： Modbus_ReadReg_03H_RTN_Udp
接收解析函数
-------------------------------------------------------------------------*/
/**
*@brief    As master ,read 1 or more reg, feedback
接收的数据先到reals结构体缓存，，再到modbus变量解析
*@param[in]     None
*@param[out]    None
*@return         
0- ok
1- fail,no data
*/
static uint8_t Modbus_ReadReg_03H_RTN_Udp(UDP_MODBUS_STRUCT *udp_modbus, const uint8_t *cmdBuf)
{
    uint8_t bytesCounter = 0; 
	uint8_t i = 0;
    uint16_t regAdderss = udp_modbus->gRegAddress;
    uint16_t gRegCnt = udp_modbus->gRegCnt;
    uint16_t *regPtr = NULL;
	uint16_t SlaveAddr = udp_modbus->slaveaddress;
	
    reg2_position_t reg_position;

    ESP_LOGW(VmodbusTAG, "[Modbus_ReadReg_03H_RTN_Udp] RegAddress : %d,  gRegCnt : %d,  RTN_gRegCnt : %d",regAdderss,gRegCnt,cmdBuf[2]>>1);

	regPtr = vLookupDataTab_Can(MASTER_BLE_WIFI, SlaveAddr, regAdderss, gRegCnt, false,NULL, NULL ,&reg_position); 			//  
//    regPtr = vLookupDataTab2(MASTER_ESP2UART, SlaveAddr,regAdderss, gRegCnt, false, &reg_position);	
    
    if((NULL != regPtr)&&((gRegCnt<<1) == cmdBuf[2]))
    {
        ESP_LOGI(VmodbusTAG, "[Modbus_ReadReg_03H_RTN_Udp] regAdderss2 ok");
        reals.udp_read_state = 1;
        
		bytesCounter = gRegCnt<<1;
        
		for(i = 0; i < bytesCounter; i += 2)
		{
			regPtr[i/2] = ((uint16_t)cmdBuf[3 + i]<<8) | cmdBuf[4 + i];//H/L	
		}
    }
    else
    {
        ESP_LOGE(VmodbusTAG, "[Modbus_ReadReg_03H_RTN_Udp] regAdderss2 or gRegCnt error");
        return 1;
    }

    return 0;
	
}


/*------------------------------------------------------------------------------
 Function: Modbus_WriteReg_06H_10H_RTN_Udp
 -----------------------------------------------------------------------------*/
/**
  * @brief      06H 10H rtn识别（不做处理）
  * @param[in]  UART_STRUCT *struct_uart  
                const uint8_t *cmdBuf     
  * @param[out] None
  * @return     static uint8_t
  */
static uint8_t Modbus_WriteReg_06H_10H_RTN_Udp(UDP_MODBUS_STRUCT *udp_modbus, const uint8_t *cmdBuf)
{
    // uint16_t    regAdderss = udp_modbus->gRegAddress;
    // uint16_t    gRegCnt = 0;
    uint16_t regAdderss = cmdBuf[2] << 8 | cmdBuf[3];
    uint16_t gRegCnt = 0;

    if(0x06 == cmdBuf[1])
    {
        gRegCnt = 1;
        
        if(regAdderss == (((uint16_t)cmdBuf[2]<<8) | cmdBuf[3]))
        {
            ESP_LOGW(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] regAdderss : %d, gRegCnt : %d",regAdderss,gRegCnt);
        }
        else
        {
            ESP_LOGE(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] ERROR");
            return 1;
        }
    }
    else if(0x10 == cmdBuf[1])
    {
        gRegCnt = cmdBuf[4] << 8 | cmdBuf[5];
        if((regAdderss == 700)
            &&(gRegCnt == 6))
        {
            uart_ota_recv(cmdBuf, 8);//modbus wr 0x10 700 6 rx
            ESP_LOGW(TAG, "Modbus_WriteReg_06H_10H_RTN_Udp uart_ota_recv  OTA triger!");
        }       
        else if((regAdderss == (((uint16_t)cmdBuf[2]<<8) | cmdBuf[3]))
            &&(gRegCnt == (((uint16_t)cmdBuf[4]<<8) | cmdBuf[5])))
        {
            ESP_LOGW(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] regAdderss : %d, gRegCnt : %d",regAdderss,gRegCnt);
        } 
        else
        {
            ESP_LOGE(TAG, "[Modbus_WriteReg_06H_10H_RTN_Udp] ERROR:%d, regcnt:%d",regAdderss, gRegCnt);
            return 1;
        }
    }
    return 0;
	
}

/*------------------------------------------------------------------------------
 Function: UDP_Tx_Period_module
 -----------------------------------------------------------------------------*/
/**
  * @brief      UDP周期发送
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void UDP_Tx_Period_module(void)
{
    // static uint16_t sCnt_time_out=0;    //接受读取rtn超时判断
    static uint16_t sCnt_big=0;
    // static uint64_t sCnt_action_gap=0;
    // static uint16_t udp_read_state = 0;    //当前读取状态 0：待发送 1：待接收
    // uint16_t i=0;
    int ret=0; 

    // static uint8_t slaveaddr = 0;

    UDP_MODBUS_STRUCT *udp_modbus = &g_udp_modbus;

    /*当前设备查询结束，查询下一设备*/
    if(++sCnt_big > 50) //5s 
    {
        sCnt_big = 0;
        {
            udp_modbus->gRegAddress = MOD_REG_START_ADDR_14500;
            udp_modbus->gRegCnt  = MOD_REG_LEN_14500; 
            uint16_t len = 0;
            udp_modbus->TxBytesNum = Udp_Singlecast_Modbus_MasterTxCmd(udp_modbus->gRegAddress, udp_modbus->gRegCnt, wireless_interface.data_tx_wifi_mesh, g_master_info.dev_sn.dev_sn, MESH_FRAME_TYPE_PERIOD, len);
            
            dump_buf_global(" tx1 :g_master_info.dev_sn.dev_sn[0]", (uint8_t *)&g_master_info.dev_sn.dev_sn[0], 10);
            ESP_LOGE(TAG, "udp_modbus->TxBytesNum	 =%d",udp_modbus->TxBytesNum);

            // ESP_LOGE(TAG, "Udp_singlecast_Tx   BBB,slaveaddr=%d",g_master_info.dev_sn.sn);
            ret = send_wifi_mesh_data(g_master_info.dev_sn.sn,g_master_info.dev_sn.dev_type, 
                        wireless_interface.data_tx_wifi_mesh, udp_modbus->TxBytesNum);

            ESP_LOGW(TAG, "uart_xmd_send,ret=%d",ret);
            // dump_buf_global(" tx2 :wireless_interface.data_tx_wifi_mesh", (uint8_t *)&wireless_interface.data_tx_wifi_mesh, udp_modbus->TxBytesNum);
        }
    }
}

/**
  * @brief      UDP发送任务（100ms）
  * @param[in]  void  
  * @param[out] None
  * @return     void
  */
void wifi_mesh_Top_Tx_task(void)//100ms
{
    static uint64_t scnt = 0;
    static uint8_t sflag = 0;

	if(0 == sflag)
	{
		sflag =1;
//		Top_Net_Point_Clean();
	}
	/*周期发送查询*/
    if (0 == reals.Addr_can_master)
    {
	    UDP_Tx_Period_module();
    }
    // modify by debug: 10 --> 1
    if ( ++scnt > 10 )
    {
        scnt = 0;
    }
	else
	{

	}
}
