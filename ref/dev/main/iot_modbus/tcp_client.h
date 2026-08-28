#pragma once

#include "modbus_tcp_master_common.h"

#include <stdint.h>
#include <unistd.h>

int tcp_client_connect(modbus_tcp_client_t *client);
int tcp_client_disconnect(modbus_tcp_client_t *client);
void tcp_client_reset(modbus_tcp_client_t *client);
void tcp_client_close(modbus_tcp_client_t *client);
int tcp_client_send(modbus_tcp_client_t *client, const void *buf, size_t len);
int tcp_client_recv(modbus_tcp_client_t *client, void *buf, size_t len);
int tcp_client_is_read_ready(modbus_tcp_client_t *client, struct timeval *tv);
int tcp_client_is_write_ready(modbus_tcp_client_t *client, struct timeval *tv);
bool tcp_client_is_connected(modbus_tcp_client_t *client);
