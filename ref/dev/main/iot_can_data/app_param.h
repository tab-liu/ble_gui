#ifndef __APP_PARAM_H__
#define __APP_PARAM_H__
#include "esp_err.h"


//esp_err_t app_ll_param_task(void);
void app_ll_param_thread(void);
void app_reset_factory_after_can_tx(void);
void app_ac_button_sign_handle(void);

#endif
