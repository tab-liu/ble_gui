#pragma once

#include "modbus_tcp_common.h"

#include <stdint.h>
#include <unistd.h>

int tcps_server_open(modbus_tcp_server_t *server);
int tcps_server_listen(modbus_tcp_server_t *server);
int tcps_server_accept(modbus_tcp_server_t *server);
void tcps_server_reset(modbus_tcp_server_t *server);
void tcps_server_close(modbus_tcp_server_t *server);
ssize_t tcps_server_send(modbus_tcp_server_t *server, const uint8_t *req, int req_length);
ssize_t tcps_server_recv(modbus_tcp_server_t *server, uint8_t *rsp, int rsp_length);
int tcps_server_read_ready(modbus_tcp_server_t *server, struct timeval *tv);
bool tcps_server_is_connected(modbus_tcp_server_t *server);
