#pragma once
#include <stdbool.h>
#include "nimble/ble.h"
#include "modlog/modlog.h"
#include "ble_client_central.h"
#include "host/ble_uuid.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ble_gatt_register_ctxt;

#define GATTS_SVR_UUID          0xff00  // 定义gatts服务UUID
#define GATTS_SVR_CHR1_UUID     0xff01  // 定义gatts服务下的特征1 UUID
#define GATTS_SVR_CHR2_UUID     0xff02  // 定义gatts服务下的特征2 UUID



//#define PEER_ADDR_VAL_SIZE                                  6

typedef void (*gatts_data_rx_callback)(uint8_t *data, uint8_t len);


//struct peer {
//    SLIST_ENTRY(peer) next;
//    uint16_t conn_handle;
//
//    uint8_t peer_addr[PEER_ADDR_VAL_SIZE];
//
//    /** List of discovered GATT services. */
//    struct peer_svc_list svcs;
//
//    /** Keeps track of where we are in the service discovery process. */
//    uint16_t disc_prev_chr_val;
//    struct peer_svc *cur_svc;
//
//    /** Callback that gets executed when service discovery completes. */
//    peer_disc_fn *disc_cb;
//    void *disc_cb_arg;
//};




void gatts_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatts_svr_init(gatts_data_rx_callback cb);
int gatts_data_tx(uint8_t *data, uint8_t len, uint8_t notify, uint16_t conn_handle);
int gatts_chr3_data_tx(uint8_t *data, uint8_t len, uint8_t notify, uint16_t conn_handle);
uint16_t gatts_get_chr1_handle(void);
uint16_t gatts_get_chr3_handle(void);
ble_uuid_t *get_svc_uuid(void);
bool gatts_flowctrl(void);

#ifdef __cplusplus
}
#endif

// #endif
