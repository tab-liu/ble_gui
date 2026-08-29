#pragma once

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

void iot_period_fast_task(void *pvParameters);
void iot_period_slow_task (void * pvParameters);
void iot_cloud_process_task(void * pvParameters);
void System_File_Data_Process_Task (void * pvParameters);

#ifdef __cplusplus
}
#endif
