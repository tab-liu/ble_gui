#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "cmd_udp_back.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


#define JOIN_TIMEOUT_MS (10000)

static const char *TAG = "[cmd_udp_back]";

//
///** Arguments used by 'join' function */
//static struct {
//    struct arg_str *ip_addr;
//    struct arg_int *port;
//    struct arg_end *end;
//} udp_args;
//
//static int udp_back_send(int argc, char **argv)
//{
//    int nerrors = arg_parse(argc, argv, (void **) &udp_args);
//    if (nerrors != 0) {
//        arg_print_errors(stderr, udp_args.end, argv[0]);
//        return 1;
//    }
//
//    printf("\nsend ip_addr:%s, send port:%d\n",udp_args.ip_addr->sval[0],udp_args.port->ival[0]);
//
//    /*udp send begin*/
//    int sockfd = -1;
//	sockfd =Udp_multicast_sock_fd;
////    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
//    if (sockfd < 0) {
//        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
//        return -1;
//    }
//
//    struct sockaddr_in dest_addr;
//    memset(&dest_addr, 0, sizeof(dest_addr));
//    dest_addr.sin_family = AF_INET;
//    dest_addr.sin_port = htons(udp_args.port->ival[0]); // PORT是接收方的端口号 3333
//    inet_pton(AF_INET, udp_args.ip_addr->sval[0], &dest_addr.sin_addr); // 设置接收方的IP地址 "192.168.0.217"
//
//    char *message = "Hello, this is a UDP message!";
//    int bytes_sent = sendto(sockfd, message, strlen(message), 0,
//                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));
//    if (bytes_sent < 0) {
//        perror("Error in sending message");
//        exit(EXIT_FAILURE);
//    }
//
//    close(sockfd);
//
//    /*udp send end*/
//    
//    return 0;
//}
//
//void register_udp_back(void)
//{
//    udp_args.ip_addr = arg_str0(NULL, "ip", "", "send back ip address");//多字符 --ip=xx.xx.xx.xx
//    udp_args.port = arg_int0("p", NULL, "", "send back port");//单字符 -p xx
//    udp_args.end = arg_end(2);
//
//    const esp_console_cmd_t udp_cmd = {
//        .command = "udp",
//        .help = "send a udp packet",
//        .hint = NULL,
//        .func = &udp_back_send,
//        .argtable = &udp_args
//    };
//
//    ESP_ERROR_CHECK( esp_console_cmd_register(&udp_cmd) );
//}
