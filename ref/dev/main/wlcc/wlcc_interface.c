#include "wlcc_interface.h"
#include "wlcc_common.h"
#include "iot_mqtt.h"
// #include "iot_define.h"

#include "esp_netif.h"
#include "esp_log.h"

#define TAG     "[wlcc_interface]"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int wlcc_singlecast_handle = -1;
int wlcc_multicast_handle = -1;
char *netif_key[] = {NETIF_KEY_WIFI_STA, NETIF_KEY_WIFI_AP};

int get_netif_req_by_type(netif_type_t netif_type, struct ifreq *netif_req)
{
    esp_err_t ret = ESP_OK;
#if !CONFIG_LWIP_NETIF_API
    ret = esp_netif_get_netif_impl_name(esp_netif_get_handle_from_ifkey(netif_key[netif_type]),
                                    netif_req->ifr_name);
#else
    if_indextoname(esp_netif_get_netif_impl_index(esp_netif_get_handle_from_ifkey(netif_key[netif_type])),
                                netif_req->ifr_name);
#endif

    ESP_LOGW(TAG, "set_udp_client_netif:%s", netif_req->ifr_name);

    return ret == ESP_OK ? IOT_OK : IOT_ERR_WLCC_IF;
}

bool get_netif_ip_by_type(netif_type_t netif_type, char *netif_ip, uint16_t netif_ip_size)
{
    esp_netif_ip_info_t ip_info = {0};

    esp_netif_t *netif_handle = esp_netif_get_handle_from_ifkey(netif_key[netif_type]);
    if (esp_netif_get_ip_info(netif_handle, &ip_info) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get IP address info for netif type %d", netif_type);
        return false;
    }

    snprintf(netif_ip, netif_ip_size, "%d.%d.%d.%d",
             ip4_addr1_16(&ip_info.ip),
             ip4_addr2_16(&ip_info.ip),
             ip4_addr3_16(&ip_info.ip),
             ip4_addr4_16(&ip_info.ip));

    return true;
}

int add_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int ret = 0;
    // Configure source interface
// #if LISTEN_ALL_IF
//     imreq.imr_interface.s_addr = IPADDR_ANY;
// #else
//     esp_netif_ip_info_t ip_info = { 0 };
//     err = esp_netif_get_ip_info(get_example_netif(), &ip_info);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to get IP address info. Error 0x%x", err);
//         goto err;
//     }
//     inet_addr_from_ip4addr(&iaddr, &ip_info.ip);
// #endif // LISTEN_ALL_IF
    // Configure multicast address to listen to
    ret = inet_aton(MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);
    if (ret != 1)
    {
        ESP_LOGE(TAG, "Configured IPV4 multicast address '%s' is invalid.", MULTICAST_IPV4_ADDR);
        // Errors in the return value have to be negative
        ret = IOT_ERR_WLCC_IP;
        goto err;
    }

    ESP_LOGI(TAG, "Configured Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr)))
    {
        ESP_LOGW(TAG, " '%s' is not a valid multicast address. This will probably not work.", MULTICAST_IPV4_ADDR);
    }

    if (assign_source_if)
    {
        // Assign the IPv4 multicast source interface, via its IP
        // (only necessary if this socket is IPV4 only)
        ret = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &imreq.imr_multiaddr,
                         sizeof(struct in_addr));
        if (ret < 0)
        {
            ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d, %s", errno, strerror(errno));
            ret = IOT_ERR_WLCC_SOCKET_SETOPT_FAILED;
            goto err;
        }
    }

    ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &imreq, sizeof(struct ip_mreq));
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d, %s", errno, strerror(errno));
        ret = IOT_ERR_WLCC_SOCKET_SETOPT_FAILED;
        goto err;
    }

 err:
    return ret;
}

int create_multicast_network(netif_type_t netif_type)
{
    struct sockaddr_in saddr = { 0 };
    int sock = -1;
    int ret = IOT_OK;
    struct ifreq netif_req;

    ret = get_netif_req_by_type(netif_type, &netif_req);
    if(IOT_OK != ret)
    {
        ESP_LOGE(TAG, "create multicast network error: Failed to get netif request for type %d", netif_type);
        return ret;
    }

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);//PF_INET=IPV4；SOCK_DGRAM=UDP
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Udp_multicast_init : Failed to create socket. Error %d, %s", errno, strerror(errno));
        return -2;
    }

    // 绑定套接字到特定的网卡
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (void *)&netif_req, sizeof(netif_req)) < 0)
    {
        ESP_LOGE(TAG, "Udp_multicast_init : Failed to bind socket to device. Error %d, %s", errno, strerror(errno));
        ret = IOT_ERR_WLCC_SOCKET_SETOPT_FAILED;
        goto err;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(UDP_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ret = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        ESP_LOGE(TAG, "Udp_multicast_init : Failed to bind socket. Error %d, %s", errno, strerror(errno));
        goto err;
    }

    // Assign multicast TTL
    uint8_t ttl = MULTICAST_TTL;
    ret = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (ret < 0)
    {
        ESP_LOGE(TAG, "Udp_multicast_init : Failed to set IP_MULTICAST_TTL. Error %d, %s", errno, strerror(errno));
        ret = IOT_ERR_WLCC_SOCKET_SETOPT_FAILED;
        goto err;
    }

    // this is also a listening socket, so add it to the multicast
    // group for listening...
    ret = add_multicast_group(sock, true);
    if (ret != IOT_OK)
    {
        goto err;
    }

    wlcc_multicast_handle = sock;
    // All set, socket is configured for sending and receiving
    ESP_LOGI(TAG,"Udp_multicast_init : sockfd is %d", sock);
    return ret;

err:
    close(sock);
    return ret;
}

int create_singlecast_network(netif_type_t netif_type)
{
    struct sockaddr_in addr = { 0 };
    char source_ip_str[32] = {0};
    int err = 0;
    int sock = -1;
    struct ifreq netif_req;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);//PF_INET=IPV4；SOCK_DGRAM=UDP
    if (sock < 0) {
        ESP_LOGE(TAG, "create singlecast: Failed to create socket. Error %d, %s", errno, strerror(errno));
        return -1;
    }

    if(IOT_OK != get_netif_req_by_type(netif_type, &netif_req))
    {
        ESP_LOGE(TAG, "create siglecase network error: Failed to get netif request for type %d", netif_type);
        return -2;
    }

    // 绑定套接字到特定的网卡
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (void *)&netif_req, sizeof(netif_req)) < 0) {
        ESP_LOGE(TAG, "siglecase init : Failed to bind socket to device. Error %d, %s", errno, strerror(errno));
        goto err;
    }

    if (!get_netif_ip_by_type(netif_type, source_ip_str, sizeof(source_ip_str)))
    {
        ESP_LOGE(TAG, "Udp_singlecast_init : Failed to get IP address for netif type %d", netif_type);
        goto err;
    }

    // Configure the address
    addr.sin_family = PF_INET;
    addr.sin_port = htons(UDP_PORT_SINGLE);
    err = inet_pton(PF_INET, source_ip_str, &(addr.sin_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Udp_singlecast_init : Failed to convert IP address '%s'. Error %d, %s", source_ip_str, errno, strerror(errno));
        goto err;
    }
    ESP_LOGW(TAG, "Udp_singlecast_init : Configured to convert IP address '%s'. ", source_ip_str);

    // Bind the socket to the address
    err = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Udp_singlecast_init : Failed to bind the socket. Error %d, %s", errno, strerror(errno));
        goto err;
    }

    wlcc_singlecast_handle = sock;
    // All set, socket is configured for sending and receiving
    ESP_LOGI(TAG,"Udp_singlecast_init : sockfd is %d",sock);
    return sock;

err:
    close(sock);
    return -1;
}

/**
 * @brief 创建设备间通信网络
 * @param netif_type 网络绑定的网卡
 * @return int IOT_OK 成功, 其它失败
 */
int create_wlcc_network(netif_type_t netif_type)
{
    int ret = IOT_OK;

    ret = create_multicast_network(netif_type);
    if (ret != IOT_OK)
    {
        ESP_LOGE(TAG, "Failed to create multicast network for type %d", netif_type);
        return ret;
    }

#if !USR_SINGLE_PORT
    ret = create_singlecast_network(netif_type);
    if (ret != IOT_OK)
    {
        ESP_LOGE(TAG, "Failed to create singlecast network for type %d", netif_type);
        return ret;
    }
#endif
    return IOT_OK;
}

void destroy_wlcc_network(void)
{
    if (wlcc_multicast_handle >= 0)
    {
        shutdown(wlcc_multicast_handle, 0);
        close(wlcc_multicast_handle);
        wlcc_multicast_handle = -1;
        ESP_LOGI(TAG, "Multicast socket closed");
    }
#if !USR_SINGLE_PORT
    if (wlcc_singlecast_handle >= 0)
    {
        shutdown(wlcc_singlecast_handle, 0);
        close(wlcc_singlecast_handle);
        wlcc_singlecast_handle = -1;
        ESP_LOGI(TAG, "Singlecast socket closed");
    }
#endif
}

int is_ready_wlcc_network(void)
{
#if USR_SINGLE_PORT
    return (wlcc_multicast_handle >= 0) ? IOT_OK : IOT_ERR_WLCC_NOT_STARTED;
#else
    return (wlcc_multicast_handle >= 0 || wlcc_singlecast_handle >= 0) ? IOT_OK : IOT_ERR_WLCC_NOT_STARTED;
#endif
}

int recv_wlcc(uint8_t *rx_buf, uint16_t rx_buf_size, char *src_ip, uint16_t *src_port)
{
    int ret = IOT_OK;
    int len = 0;
    int socket = -1;
    fd_set rfds;
    struct sockaddr_storage raddr;
    socklen_t socklen = sizeof(raddr);
    struct timeval tv =
    {
        .tv_sec = 0,
        .tv_usec = 0,
    };
#if USR_SINGLE_PORT
    if (wlcc_multicast_handle < 0)
#else
    if (wlcc_multicast_handle < 0 && wlcc_singlecast_handle < 0)
#endif
    {
        ESP_LOGE(TAG, "wlcc socket not initialized");
        return -IOT_ERR_WLCC_NOT_INIT;
    }

    FD_ZERO(&rfds);
    if (wlcc_multicast_handle >= 0)
    {
        FD_SET(wlcc_multicast_handle, &rfds);
    }
#if USR_SINGLE_PORT
    ret = select(wlcc_multicast_handle + 1, &rfds, NULL, NULL, &tv);
#else
    if (wlcc_singlecast_handle >= 0)
    {
        FD_SET(wlcc_singlecast_handle, &rfds);
    }

    ret = select(MAX(wlcc_multicast_handle, wlcc_singlecast_handle) + 1, &rfds, NULL, NULL, &tv);
#endif
    if (ret < 0)
    {
        ESP_LOGE(TAG, "wlcc select error: %d, %s", errno, strerror(errno));
        return IOT_FAIL;
    }
    else if (ret == 0)
    {
        return IOT_OK;
    }
#if USR_SINGLE_PORT
    if (0)
#else
    /*判断接受来源*/
    if (FD_ISSET(wlcc_singlecast_handle, &rfds))
#endif
    {
        socket = wlcc_singlecast_handle;
    }
    else if (FD_ISSET(wlcc_multicast_handle, &rfds))
    {
        socket = wlcc_multicast_handle;
    }
    else
    {
        return IOT_OK;
    }

    // char raddr_name[32] = {0};
    // uint16_t raddr_port = 0;

    struct msghdr msg;
    struct iovec iov;
    char ctrl_buf[CMSG_SPACE(sizeof(struct in_addr))];

    memset(&msg, 0, sizeof(msg));
    memset(ctrl_buf, 0, sizeof(ctrl_buf));

    iov.iov_base = rx_buf;
    iov.iov_len = rx_buf_size - 1;
    msg.msg_name = &raddr;
    msg.msg_namelen = socklen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl_buf;
    msg.msg_controllen = sizeof(ctrl_buf);

    ret = recvmsg(socket, &msg, 0);
    if ((ret > 0))  // && (raddr.ss_family == PF_INET)
    {
        if (src_ip != NULL)
        {
            inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr, src_ip, 32 - 1);
        }

        if (src_port != NULL)
        {
            *src_port = ntohs(((struct sockaddr_in *)&raddr)->sin_port);
        }
        // inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr, raddr_name, sizeof(raddr_name) - 1);
        // raddr_port = ntohs(((struct sockaddr_in *)&raddr)->sin_port);

        len = ret;
        ESP_LOGI(TAG, "socket(%d) received %d bytes from %s, %d", socket, len, src_ip, *src_port);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, rx_buf, len, ESP_LOG_INFO);
    }

    return (int16_t)len;
}

int send_wlcc(const uint8_t *data, uint16_t data_len, const char *dst_ip, uint16_t dst_port)
{
    struct sockaddr_in dest_addr;
    int sockfd = -1;

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = PF_INET;

    /* 未指定目的IP和端口，则使用组播 */
    if (NULL == dst_ip || 0 == dst_port)
    {
        sockfd = wlcc_multicast_handle;
        dest_addr.sin_port = htons(UDP_PORT);
        inet_pton(AF_INET, MULTICAST_IPV4_ADDR, &dest_addr.sin_addr);
        ESP_LOGI(TAG, "send multicast to %s:%d", MULTICAST_IPV4_ADDR, UDP_PORT);
    }
    else
    {
        // char target_ip_str[32] = {0};
        // uint16_t target_port = (uint16_t)(dst_port[0] | (dst_port[1] << 8));
        // sprintf(target_ip_str, "%d.%d.%d.%d", dst_ip[3], dst_ip[2], dst_ip[1], dst_ip[0]);
        ESP_LOGI(TAG, "send siglecast to %s:%d", dst_ip, dst_port);
#if USR_SINGLE_PORT
        sockfd = wlcc_multicast_handle;
#else
        sockfd = wlcc_singlecast_handle;
#endif
        dest_addr.sin_port = htons(dst_port);
        inet_pton(PF_INET, dst_ip, &dest_addr.sin_addr);
    }

    if (sockfd < 0)
    {
        ESP_LOGE(TAG, "send error: socket not initialized");
        return -IOT_ERR_WLCC_NOT_INIT;
    }

    int bytes_sent = sendto(sockfd, data, data_len, 0,
                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (bytes_sent < 0)
    {
        // ESP_LOGE(TAG, "send to %s : %d, error:%d", dst_ip, dest_addr.sin_port, bytes_sent);
        // ESP_LOGE(TAG, "send socket:%d error, Errorno:%d, mean:%s", sockfd, errno, strerror(errno));
        return -IOT_ERR_WLCC_SEND;
    }

    // char source_ip_str[32] = {0};
    // inet_pton(PF_INET, source_ip_str, &(dest_addr.sin_addr));
    // ESP_LOGI(TAG, "[Udp_singlecast_Tx]  send to %s : %d", source_ip_str, dest_addr.sin_port);

    return bytes_sent;
}
